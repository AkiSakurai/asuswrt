/*
 * Broadcom Common Firmware Environment (CFE)
 * Board device initialization, File: ui_bcm947xx.c
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: ui_bcm947xx.c 258996 2011-05-11 14:33:26Z simonk $
 */

#include "lib_types.h"
#include "lib_string.h"
#include "lib_queue.h"
#include "lib_malloc.h"
#include "lib_printf.h"
#include "cfe.h"
#include "cfe_iocb.h"
#include "cfe_devfuncs.h"
#include "cfe_ioctl.h"
#include "cfe_error.h"
#include "cfe_fileops.h"
#include "cfe_loader.h"
#include "ui_command.h"
#include "bsp_config.h"

#include <typedefs.h>
#include <osl.h>
#include <bcmdevs.h>
#include <bcmutils.h>
#include <hndsoc.h>
#include <siutils.h>
#include <sbchipc.h>
#include <sbmemc.h>
#include <dmemc_core.h>
#include <bcmendian.h>
#include <bcmnvram.h>
#include <hndcpu.h>
#include <trxhdr.h>
#include "addrspace.h"
#include "initdata.h"

#include "bsp_priv.h"

#ifdef RESCUE_MODE
extern void LEDON(void);
extern void LEDOFF(void);
extern void GPIO_INIT(void);

/* define GPIOs*/
#define PWR_LED_GPIO   	(1 << 18) 	// GPIO 18 for RT-N12C1
#define RST_BTN_GPIO   	(1 << 22) 	// GPIO 22 for RT-N12C1
#define EZ_BTN_GPIO   	(1 << 23) 	// GPIO 23 for RT-N12C1

#endif

#ifdef RESCUE_MODE
extern int enable_rescue;
#endif	/* RESCUE_MODE */

static int
ui_cmd_reboot(ui_cmdline_t *cmd, int argc, char *argv[])
{
	hnd_cpu_reset(sih);
	return 0;
}


static int
ui_cmd_nvram(ui_cmdline_t *cmd, int argc, char *argv[])
{
	char *command, *name, *value;
	char *buf;
	size_t size;
	int ret;

	if (!(command = cmd_getarg(cmd, 0)))
		return CFE_ERR_INV_PARAM;

	if (!strcmp(command, "get")) {
		if ((name = cmd_getarg(cmd, 1)))
			if ((value = nvram_get(name)))
				printf("%s\n", value);
	} else if (!strcmp(command, "set")) {
		if ((name = cmd_getarg(cmd, 1))) {
			if ((value = strchr(name, '=')))
				*value++ = '\0';
			else if ((value = cmd_getarg(cmd, 2))) {
				if (*value == '=')
					value = cmd_getarg(cmd, 3);
			}
			if (value)
				nvram_set(name, value);
		}
	} else if (!strcmp(command, "unset")) {
		if ((name = cmd_getarg(cmd, 1)))
			nvram_unset(name);
	} else if (!strcmp(command, "commit")) {
		nvram_commit();
	} else if (!strcmp(command, "erase")) {
		extern char *flashdrv_nvram;
		if ((ret = cfe_open(flashdrv_nvram)) < 0)
			return ret;
		if (!(buf = KMALLOC(NVRAM_SPACE, 0)))
			return CFE_ERR_NOMEM;
		memset(buf, 0xff, NVRAM_SPACE);
		cfe_writeblk(ret, 0, (unsigned char *)buf, NVRAM_SPACE);
		cfe_close(ret);
		KFREE(buf);
	} else if (!strcmp(command, "show") || !strcmp(command, "getall")) {
		if (!(buf = KMALLOC(NVRAM_SPACE, 0)))
			return CFE_ERR_NOMEM;
		nvram_getall(buf, NVRAM_SPACE);
		for (name = buf; *name; name += strlen(name) + 1)
			printf("%s\n", name);
		size = sizeof(struct nvram_header) + ((uintptr)name - (uintptr)buf);
		printf("size: %d bytes (%d left)\n", size, NVRAM_SPACE - size);
		KFREE(buf);
	}

	return 0;
}

static int
check_trx(char *trx_name)
{
	int ret;
	fileio_ctx_t *fsctx;
	void *ref;
	struct trx_header trx;
	uint32 crc, buf[512];
	int first_read = 1;
	unsigned int len, count;

	/* Open header */
	if(trx_name == NULL){
		ret = fs_init("raw", &fsctx, "flash0.trx");
	}else{
		ret = fs_init("raw", &fsctx, trx_name);
	}

	if (ret)
		return ret;

	ret = fs_open(fsctx, &ref, "", FILE_MODE_READ);
	if (ret) {
		fs_uninit(fsctx);
		return ret;
	}

	/* Read header */
	ret = fs_read(fsctx, ref, (unsigned char *) &trx, sizeof(struct trx_header));
	if (ret != sizeof(struct trx_header)) {
		ret = CFE_ERR_IOERR;
		goto done;
	}

	/* Verify magic number */
	if (ltoh32(trx.magic) != TRX_MAGIC) {
		ret = CFE_ERR_INVBOOTBLOCK;
		goto done;
	}

	/* Checksum over header */
	crc = hndcrc32((uint8 *) &trx.flag_version,
	               sizeof(struct trx_header) - OFFSETOF(struct trx_header, flag_version),
	               CRC32_INIT_VALUE);

	for (len = ltoh32(trx.len) - sizeof(struct trx_header); len; len -= count) {
		if (first_read) {
			count = MIN(len, sizeof(buf) - sizeof(struct trx_header));
			first_read = 0;
		} else
			count = MIN(len, sizeof(buf));

		/* Read data */
		ret = fs_read(fsctx, ref, (unsigned char *) &buf, count);
		if (ret != count) {
			ret = CFE_ERR_IOERR;
			goto done;
		}

		/* Checksum over data */
		crc = hndcrc32((uint8 *) &buf, count, crc);
	}

	/* Verify checksum */
	if (ltoh32(trx.crc32) != crc) {
		ret = CFE_ERR_BOOTPROGCHKSUM;
		goto done;
	}

	ret = 0;

done:
	fs_close(fsctx, ref);
	fs_uninit(fsctx);
	if (ret)
		xprintf("%s\n", cfe_errortext(ret));
	return ret;
}

#ifdef RESCUE_MODE


extern void LEDON(void)
{
                                                                                                                  
        sih = si_kattach(SI_OSH);
        ASSERT(sih);
        si_gpioouten(sih, PWR_LED_GPIO, PWR_LED_GPIO, GPIO_DRV_PRIORITY);
                                                                                                                  
        /* led on */
        /* negative logic and hence val==0 */
        si_gpioout(sih, PWR_LED_GPIO, 0, GPIO_DRV_PRIORITY);
}
extern void GPIO_INIT(void)
{

        sih = si_kattach(SI_OSH);
        ASSERT(sih);
        si_gpiocontrol(sih, PWR_LED_GPIO, 0, GPIO_DRV_PRIORITY);
        si_gpioouten(sih, PWR_LED_GPIO, 0, GPIO_DRV_PRIORITY);
}


extern void LEDOFF(void)
{
        sih = si_kattach(SI_OSH);
        ASSERT(sih);
        si_gpioouten(sih, PWR_LED_GPIO, PWR_LED_GPIO, GPIO_DRV_PRIORITY);
        si_gpioout(sih, PWR_LED_GPIO, PWR_LED_GPIO, GPIO_DRV_PRIORITY);
}

#endif // RESCUE_MODE

/*
 *  ui_get_loadbuf(bufptr, bufsize)
 *
 *  Figure out the location and size of the staging buffer.
 *
 *  Input parameters:
 *       bufptr - address to return buffer location
 *       bufsize - address to return buffer size
 */
static void ui_get_loadbuf(uint8_t **bufptr, int *bufsize)
{
	int size = CFG_FLASH_STAGING_BUFFER_SIZE;

	/*	
	 * Get the address of the staging buffer.  We can't
	 * allocate the space from the heap to store the 
	 * new flash image, because the heap may not be big
	 * enough.  So, if FLASH_STAGING_BUFFER_SIZE is non-zero
	 * then just use it and FLASH_STAGING_BUFFER; else
	 * use the larger of (mem_bottomofmem - FLASH_STAGING_BUFFER)
	 * and (mem_totalsize - mem_topofmem).
	 */

	if (size > 0) {
		*bufptr = (uint8_t *) KERNADDR(CFG_FLASH_STAGING_BUFFER_ADDR);
		*bufsize = size;
	} else {
		int below, above;
		int reserved = CFG_FLASH_STAGING_BUFFER_ADDR;

		/* For small memory size (8MB), we tend to use the below region.
		 * The buffer address may conflict with the os running address,
		 * so we reserve 3MB for the os.
		 */
		if ((mem_totalsize == (8*1024)) && (PHYSADDR(mem_bottomofmem) > 0x300000))
			reserved = 0x300000;

		below = PHYSADDR(mem_bottomofmem) - reserved;
		above = (mem_totalsize << 10) - PHYSADDR(mem_topofmem);

		if (below > above) {
			*bufptr = (uint8_t *) KERNADDR(reserved);
			*bufsize = below;
		} else {
			*bufptr = (uint8_t *) KERNADDR(mem_topofmem);
			*bufsize = above;
		}
	}
}

#ifdef DUAL_IMAGE
static
int check_image_prepare_cmd(int the_image, char *buf, uint32 osaddr, int bufsize)
{
	int ret = -1;

	if (the_image == 0) {
		if ((ret = check_trx(IMAGE_1ST_FLASH_TRX)) == 0) {
			sprintf(buf, "boot -raw -z -addr=0x%x -max=0x%x %s:", osaddr, bufsize,
			        IMAGE_1ST_FLASH_OS);
		} else {
			printf("flash3.trx CRC check failed!\n");
		}
	} else if (the_image == 1) {
		if ((ret = check_trx(IMAGE_2ND_FLASH_TRX)) == 0) {
			sprintf(buf, "boot -raw -z -addr=0x%x -max=0x%x %s:", osaddr, bufsize,
			        IMAGE_2ND_FLASH_OS);
		} else {
			printf("flash3.trx2 CRC check failed!\n");
		}
	} else {
		printf("No such linux image partitions\n");
	}
	return ret;
}
#endif /* DUAL_IMAGE */

#ifdef CFG_NFLASH
static int ui_get_bootflags(void)
{
	int bootflags = 0;
	char *val;

	/* Only support chipcommon revision == 38 and BCM4706 for now */
	if (((CHIPID(sih->chip) == BCM4706_CHIP_ID) || sih->ccrev == 38) &&
		(sih->cccaps & CC_CAP_NFLASH)) {
		if ((val = nvram_get("bootflags")))
			bootflags = atoi(val);
		else if ((sih->chipst & (1 << 4)) != 0) {
			/* This is NANDBOOT */
			bootflags = FLASH_KERNEL_NFLASH | FLASH_BOOT_NFLASH;
		}
	}
	return bootflags;
}

void ui_get_boot_flashdev(char *flashdev)
{
	if (!flashdev)
		return;
	if ((ui_get_bootflags() & FLASH_BOOT_NFLASH) == FLASH_BOOT_NFLASH)
		strcpy(flashdev, "nflash1.boot");
	else
		strcpy(flashdev, "flash1.boot");

	return;
}

void ui_get_os_flashdev(char *flashdev)
{
	if (!flashdev)
		return;
	if ((ui_get_bootflags() & FLASH_KERNEL_NFLASH) == FLASH_KERNEL_NFLASH)
		strcpy(flashdev, "nflash0.os");
	else
		strcpy(flashdev, "flash0.os");

	return;
}

void ui_get_trx_flashdev(char *flashdev)
{
	if (!flashdev)
		return;
	if ((ui_get_bootflags() & FLASH_KERNEL_NFLASH) == FLASH_KERNEL_NFLASH)
		strcpy(flashdev, "nflash1.trx");
	else
		strcpy(flashdev, "flash1.trx");
	return;
}
#endif /* CFG_NFLASH */

static int
ui_cmd_go(ui_cmdline_t *cmd, int argc, char *argv[])
{
	int ret = 0;
	char buf[512];
	struct trx_header *file_buf;
	uint8_t *ptr;
	char *val;
	uint32 osaddr;
	int bufsize = 0;
	int retry = 0;

#ifdef RESCUE_MODE
	int  i = 0;
	GPIO_INIT();
	LEDON();
#endif

	int trx_failed;
#ifdef DUAL_IMAGE
	char *img_boot = nvram_get(IMAGE_BOOT);
#endif
#ifdef CFG_NFLASH
	char trx_name[16], os_name[16];
#else
	char *trx_name = "flash1.trx";
	char *os_name = "flash0.os";
#endif	/* CFG_NFLASH */

	val = nvram_get("os_ram_addr");
	if (val)
		osaddr = bcm_strtoul(val, NULL, 16);
	else
		osaddr = 0x80001000;

#ifdef DUAL_IMAGE
	if (img_boot != NULL)
		trx_failed = check_trx(IMAGE_1ST_FLASH_TRX) && check_trx(IMAGE_2ND_FLASH_TRX);
	else
#endif	/* DUAL_IMAGE */
	{
#ifdef CFG_NFLASH
		ui_get_trx_flashdev(trx_name);
		ui_get_os_flashdev(os_name);
#endif
		trx_failed = check_trx(trx_name);
	}

#ifdef RESCUE_MODE
	if (trx_failed || enable_rescue > 0) {
#else
	if (trx_failed) {
#endif
		/* Wait for CFE_ERR_TIMEOUT_LIMIT for an image */
		while (1) {
#ifdef RESCUE_MODE
			printf("Entering Rescue:[%s]\n", trx_name);
#endif
#ifdef RESCUE_MODE
			sprintf(buf, "flash -noheader : flash1.trx");
#else
			sprintf(buf, "flash -noheader :%s", trx_name);
#endif
			if ((ret = ui_docommand(buf)) != CFE_ERR_TIMEOUT)
				break;
			if (++retry == CFE_ERR_TIMEOUT_LIMIT) {
				ret = CFE_ERR_INTR;
				break;
			}
#ifdef RESCUE_MODE
			if (i%2 == 0) LEDON();
			else LEDOFF();
			i++;
			if (i==0xffffff) i = 0;
#endif
		}
	} else if (!nvram_invmatch("boot_wait", "on")) {
		ui_get_loadbuf(&ptr, &bufsize);
		/* Load the image */
		sprintf(buf, "load -raw -addr=0x%x -max=0x%x :", ptr, bufsize);
		ret = ui_docommand(buf);

		/* Load was successful. Check for the TRX magic.
		 * If it's a TRX image, then proceed to flash it, else try to boot
		 * Note: To boot a TRX image directly from the memory, address will need to be
		 * load address + trx header length.
		 */
		if (ret == 0) {
			file_buf = (struct trx_header *)ptr;
			/* If it's a TRX, then proceed to writing to flash else,
			 * try to boot from memory
			 */
			if (file_buf->magic != TRX_MAGIC) {
				sprintf(buf, "boot -raw -z -addr=0x%x -max=0x%x -fs=memory :0x%x",
				        osaddr, bufsize, ptr);
				return ui_docommand(buf);
			}
			/* Flash the image from memory directly */
			sprintf(buf, "flash -noheader -mem -size=0x%x 0x%x %s",
			        file_buf->len, ptr, trx_name);
			ret = ui_docommand(buf);
		}
	}

	if (ret == CFE_ERR_INTR)
		return ret;
	/* Boot the image */
	bufsize = PHYSADDR(mem_bottomofmem) - PHYSADDR(osaddr);
#ifdef DUAL_IMAGE
	/* Get linux_boot variable to see what is current image */
	if (img_boot != NULL) {
		int i = atoi(img_boot);

		if (i > 1)
			i = 0;

		/* We try the specified one, if it is failed, we try the other one */
		if (check_image_prepare_cmd(i, buf, osaddr, bufsize) == 0) {
			printf("Got the Linux image %d\n", i);
		} else if (check_image_prepare_cmd(1-i, buf, osaddr, bufsize) == 0) {
			char temp[20];
			printf("Changed to the other Linux image %d\n", 1 - i);
			sprintf(temp, "%d", 1-i);

			nvram_set(IMAGE_BOOT, temp);
			nvram_commit();
		}
	}
	else
#else
	sprintf(buf, "boot -raw -z -addr=0x%x -max=0x%x %s:", osaddr, bufsize, os_name);
#endif /* DUAL_IMAGE */
	return ui_docommand(buf);
}

static int
ui_cmd_clocks(ui_cmdline_t *cmd, int argc, char *argv[])
{
	chipcregs_t *cc = (chipcregs_t *)si_setcoreidx(sih, SI_CC_IDX);
	uint32 ccc = R_REG(NULL, &cc->capabilities);
	uint32 pll = ccc & CC_CAP_PLL_MASK;

	if (pll != PLL_NONE) {
		uint32 n = R_REG(NULL, &cc->clockcontrol_n);
		printf("Current clocks for pll=0x%x:\n", pll);
		printf("    mips: %d\n", si_clock_rate(pll, n, R_REG(NULL, &cc->clockcontrol_m3)));
		printf("    sb: %d\n", si_clock_rate(pll, n, R_REG(NULL, &cc->clockcontrol_sb)));
		printf("    pci: %d\n", si_clock_rate(pll, n, R_REG(NULL, &cc->clockcontrol_pci)));
		printf("    mipsref: %d\n", si_clock_rate(pll, n,
		       R_REG(NULL, &cc->clockcontrol_m2)));
	} else {
		printf("Current clocks: %d/%d/%d/%d Mhz.\n",
		       si_cpu_clock(sih) / 1000000,
		       si_mem_clock(sih) / 1000000,
		       si_clock(sih) / 1000000,
		       si_alp_clock(sih) / 1000000);
	}
	return 0;
}


int
ui_init_bcm947xxcmds(void)
{
	cmd_addcmd("reboot",
	           ui_cmd_reboot,
	           NULL,
	           "Reboot.",
	           "reboot\n\n"
	           "Reboots.",
	           "");
	cmd_addcmd("nvram",
	           ui_cmd_nvram,
	           NULL,
	           "NVRAM utility.",
	           "nvram [command] [args..]\n\n"
	           "Access NVRAM.",
	           "get [name];Gets the value of the specified variable|"
	           "set [name=value];Sets the value of the specified variable|"
	           "unset [name];Deletes the specified variable|"
	           "commit;Commit variables to flash|"
	           "erase;Erase all nvram|"
	           "show;Shows all variables|");
	cmd_addcmd("go",
	           ui_cmd_go,
	           NULL,
	           "Verify and boot OS image.",
	           "go\n\n"
	           "Boots OS image if valid. Waits for a new OS image if image is invalid\n"
	           "or boot_wait is unset or not on.",
	           "");
	cmd_addcmd("show clocks",
	           ui_cmd_clocks,
	           NULL,
	           "Show current values of the clocks.",
	           "show clocks\n\n"
	           "Shows the current values of the clocks.",
	           "");
#if CFG_WLU
	wl_addcmd();
#endif

	return 0;
}
