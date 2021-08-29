/*
 * Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _IPQ6018_H
#define _IPQ6018_H

#ifndef DO_DEPS_ONLY
#include <generated/asm-offsets.h>
#endif

#define CONFIG_IPQ6018
#define CONFIG_QCA_DISABLE_SCM

#define CONFIG_BOARD_EARLY_INIT_F
#define CONFIG_BOARD_LATE_INIT
#define CONFIG_SYS_NO_FLASH
#define CONFIG_SYS_VSNPRINTF
#define CONFIG_IPQ_NO_RELOC

#define CONFIG_SYS_NONCACHED_MEMORY     (1 << 20)

#define CONFIG_IPQ6018_UART
#define CONFIG_NR_DRAM_BANKS		1
#define CONFIG_SKIP_LOWLEVEL_INIT

#define CONFIG_SYS_BOOTM_LEN		0x1000000

#define CONFIG_ENV_SIZE			0x2000
#define CONFIG_ENV_SIZE_MAX		(256 << 10) /* 256 KB */
#define CONFIG_SYS_MALLOC_LEN		(CONFIG_ENV_SIZE_MAX + (256 << 10))

/*
 * Size of malloc() pool
 */

/*
 * select serial console configuration
 */
#define CONFIG_CONS_INDEX		1

/* allow to overwrite serial and ethaddr */
#define CONFIG_BAUDRATE			115200
#define CONFIG_SYS_BAUDRATE_TABLE	{4800, 9600, 19200, 38400, 57600,\
								115200}

#define CONFIG_SYS_CBSIZE		(512 * 2) /* Console I/O Buffer Size */

/*

          svc_sp     --> --------------
          irq_sp     --> |            |
	  fiq_sp     --> |            |
	  bd         --> |            |
          gd         --> |            |
          pgt        --> |            |
          malloc     --> |            |
          text_base  --> |------------|
*/

#define CONFIG_SYS_INIT_SP_ADDR		(CONFIG_SYS_TEXT_BASE -\
			CONFIG_SYS_MALLOC_LEN - CONFIG_ENV_SIZE -\
			GENERATED_BD_INFO_SIZE)
#define CONFIG_IPQ6018_I2C 1
#ifdef CONFIG_IPQ6018_I2C
#define CONFIG_SYS_I2C_QUP
#define CONFIG_CMD_I2C
#define CONFIG_DM_I2C
#endif

#define CONFIG_SYS_MAXARGS		16
#define CONFIG_SYS_PBSIZE		(CONFIG_SYS_CBSIZE + \
						sizeof(CONFIG_SYS_PROMPT) + 16)

#define TLMM_BASE			0x01000000
#define GPIO_CONFIG_ADDR(x)		(TLMM_BASE + (x)*0x1000)
#define GPIO_IN_OUT_ADDR(x)		(TLMM_BASE + 0x4 + (x)*0x1000)

#define CONFIG_SYS_SDRAM_BASE		0x40000000
#define CONFIG_SYS_TEXT_BASE		0x48700000
#define CONFIG_SYS_SDRAM_SIZE		0x10000000
#define CONFIG_MAX_RAM_BANK_SIZE	CONFIG_SYS_SDRAM_SIZE
#define CONFIG_SYS_LOAD_ADDR		(CONFIG_SYS_SDRAM_BASE + (64 << 20))
#define CONFIG_ROOTFS_LOAD_ADDR		(CONFIG_SYS_SDRAM_BASE + (32 << 20))

#define QCA_KERNEL_START_ADDR		CONFIG_SYS_SDRAM_BASE
#define QCA_DRAM_KERNEL_SIZE		CONFIG_SYS_SDRAM_SIZE
#define QCA_BOOT_PARAMS_ADDR		(QCA_KERNEL_START_ADDR + 0x100)

#define CONFIG_OF_COMBINE		1

#define CONFIG_SMEM_VERSION_C
#define CONFIG_QCA_SMEM_BASE		0x48C00000
#define CONFIG_QCA_SMEM_SIZE		0x100000

#define CONFIG_IPQ_FDT_HIGH		0x48700000
#define CONFIG_IPQ_NO_MACS		6
#define CONFIG_ENV_IS_IN_SPI_FLASH	1
#define CONFIG_ENV_SECT_SIZE		(64 * 1024)
/*
 * IPQ_TFTP_MIN_ADDR: Starting address of Linux HLOS region.
 * CONFIG_TZ_END_ADDR: Ending address of Trust Zone and starting
 * address of WLAN Area.
 * TFTP file can only be written in Linux HLOS region and WLAN AREA.
 */
#define IPQ_TFTP_MIN_ADDR		(CONFIG_SYS_SDRAM_BASE + (16 << 20))
#define CONFIG_TZ_END_ADDR		(CONFIG_SYS_SDRAM_BASE + (88 << 21))
#define CONFIG_SYS_SDRAM_END	((long long)CONFIG_SYS_SDRAM_BASE + gd->ram_size)

#ifndef __ASSEMBLY__
#include <compiler.h>
extern loff_t board_env_offset;
extern loff_t board_env_range;
extern loff_t board_env_size;
#endif

#define CONFIG_IPQ6018_ENV		1
#define CONFIG_ENV_OFFSET		board_env_offset
#define CONFIG_ENV_SIZE			CONFIG_ENV_SIZE_MAX
#define CONFIG_ENV_RANGE		board_env_range
#define CONFIG_ENV_SIZE_MAX		(256 << 10) /* 256 KB */
#define CONFIG_SYS_MALLOC_LEN		(CONFIG_ENV_SIZE_MAX + (1024 << 10))

#define CONFIG_ENV_IS_IN_NAND		1

/* Allow to overwrite serial and ethaddr */
#define CONFIG_ENV_OVERWRITE

/*
 * Block Device & Disk  Partition Config
 */
#define HAVE_BLOCK_DEVICE
#define CONFIG_DOS_PARTITION

/*
 * USB Support
 */
#define CONFIG_USB_XHCI_IPQ
#ifdef CONFIG_USB_XHCI_IPQ
#define CONFIG_USB_XHCI
#define CONFIG_USB_XHCI_DWC3
#define CONFIG_CMD_USB
#define CONFIG_USB_STORAGE
#define CONFIG_SYS_USB_XHCI_MAX_ROOT_PORTS      2
#define CONFIG_USB_MAX_CONTROLLER_COUNT         2
#endif

#define CONFIG_PCI_IPQ
#define PCI_MAX_DEVICES	1
#ifdef CONFIG_PCI_IPQ
#define CONFIG_PCI
#define CONFIG_CMD_PCI
#define CONFIG_PCI_SCAN_SHOW
#endif

/*
* SPI Flash Configs
*/
#define CONFIG_QCA_SPI
#define CONFIG_SPI_FLASH
#define CONFIG_CMD_SF
#define CONFIG_SPI_FLASH_STMICRO
#define CONFIG_SPI_FLASH_WINBOND
#define CONFIG_SPI_FLASH_MACRONIX
#define CONFIG_SPI_FLASH_GIGADEVICE
#define CONFIG_SPI_FLASH_SPANSION
#define CONFIG_SF_DEFAULT_BUS	0
#define CONFIG_SF_DEFAULT_CS	0
#define CONFIG_SF_DEFAULT_MODE	SPI_MODE_0
#define CONFIG_SF_DEFAULT_SPEED	(48 * 1000 * 1000)
#define CONFIG_SPI_FLASH_BAR	1
#define CONFIG_SPI_FLASH_USE_4K_SECTORS
#define CONFIG_IPQ_4B_ADDR_SWITCH_REQD

#define CONFIG_EFI_PARTITION
#define CONFIG_QCA_BAM			1
/*
 * MMC configs
 */
#define CONFIG_QCA_MMC

#ifdef CONFIG_QCA_MMC
#define CONFIG_MMC
#define CONFIG_CMD_MMC
#define CONFIG_GENERIC_MMC
#define CONFIG_SDHCI
#define CONFIG_SDHCI_QCA
#define CONFIG_ENV_IS_IN_MMC
#define CONFIG_SYS_MMC_ENV_DEV	0
#define CONFIG_SDHCI_SUPPORT
#define CONFIG_MMC_ADMA
#endif

#define CONFIG_CMD_UBI

/*
 * NAND Flash Configs
 */

/* CONFIG_QPIC_NAND: QPIC NAND in BAM mode
 * CONFIG_IPQ_NAND: QPIC NAND in FIFO/block mode.
 * BAM is enabled by default.
 */
#define CONFIG_QPIC_NAND
#define CONFIG_CMD_NAND
#define CONFIG_CMD_NAND_YAFFS
#define CONFIG_SYS_NAND_SELF_INIT
#define CONFIG_SYS_NAND_ONFI_DETECTION

 /*
 * SPI NAND SUPPORT
 */
#define CONFIG_IPQ6018_SPI_NAND
#ifdef CONFIG_IPQ6018_SPI_NAND
#define CONFIG_SPI_NAND
#define CONFIG_SF_SPI_NAND_CS		1
#define CONFIG_IPQ_SPI_NAND_INFO_IDX	1
#define CONFIG_SF_NAND_BUS		1
#define CONFIG_SPI_NAND_MULTI_BYTE_READ_ID
#endif

/*
 * Expose SPI driver as a pseudo NAND driver to make use
 * of U-Boot's MTD framework.
 */
#define CONFIG_SYS_MAX_NAND_DEVICE	CONFIG_IPQ_MAX_NAND_DEVICE + \
					CONFIG_IPQ_MAX_SPI_DEVICE

#define CONFIG_IPQ_MAX_NAND_DEVICE	1
#define CONFIG_IPQ_MAX_SPI_DEVICE	2

#define CONFIG_QPIC_NAND_NAND_INFO_IDX	0
#define CONFIG_IPQ_SPI_NOR_INFO_IDX	2

#define CONFIG_NAND_FLASH_INFO_IDX	CONFIG_QPIC_NAND_NAND_INFO_IDX
#define CONFIG_SPI_FLASH_INFO_IDX	CONFIG_IPQ_SPI_NOR_INFO_IDX

#define QCA_SPI_NOR_DEVICE		"spi0.0"
#define CONFIG_QUP_SPI_USE_DMA		1

/*
 * U-Boot Env Configs
 */
#define CONFIG_OF_LIBFDT		1
#define CONFIG_SYS_HUSH_PARSER
#define CONFIG_CMD_XIMG

/* NSS firmware loaded using bootm */
#define CONFIG_BOOTCOMMAND		"bootipq"
#define CONFIG_BOOTARGS			"console=ttyMSM0,115200n8"
#define QCA_ROOT_FS_PART_NAME		"rootfs"

#define CONFIG_BOOTDELAY		2

#define CONFIG_MTD_DEVICE
#define CONFIG_CMD_MTDPARTS
#define CONFIG_MTD_PARTITIONS
#define NUM_ALT_PARTITION		16

#define CONFIG_CMD_UBI
#define CONFIG_RBTREE

#define CONFIG_CMD_BOOTZ

#define CONFIG_FDT_FIXUP_PARTITIONS
#define CONFIG_OF_BOARD_SETUP

#ifdef CONFIG_OF_BOARD_SETUP
#define DLOAD_DISABLE		0x1
/*
 * Below Configs need to be updated after enabling reset_crashdump
 * Included now to avoid build failure
 */
#define SET_MAGIC				0x1
#define CLEAR_MAGIC				0x0
#define SCM_CMD_TZ_CONFIG_HW_FOR_RAM_DUMP_ID	0x9
#define SCM_CMD_TZ_FORCE_DLOAD_ID		0x10
#define SCM_CMD_TZ_PSHOLD			0x15
#define BOOT_VERSION				0
#define TZ_VERSION				1
#define RPM_VERSION				3
#endif

#define CONFIG_IPQ6018_EDMA		1
#define CONFIG_IPQ6018_BRIDGED_MODE	1
#define CONFIG_NET_RETRY_COUNT		5
#define CONFIG_SYS_RX_ETH_BUFFER	16
#define CONFIG_CMD_PING
#define CONFIG_CMD_DHCP
#define CONFIG_MII
#define CONFIG_CMD_MII
#define CONFIG_IPADDR		192.168.10.10
#define CONFIG_NETMASK		255.255.255.0
#define CONFIG_SERVERIP		192.168.10.1
#define CONFIG_CMD_TFTPPUT
#define CONFIG_IPQ_MDIO			1
#define CONFIG_QCA8075_PHY		1
#define CONFIG_QCA8033_PHY		1
#define CONFIG_QCA8081_PHY		1
#define CONFIG_IPQ_ETH_INIT_DEFER

/* L1 cache line size is 64 bytes, L2 cache line size is 128 bytes
 * Cache flush and invalidation based on L1 cache, so the cache line
 * size is configured to 64 */
#define CONFIG_SYS_CACHELINE_SIZE	64
/*#define CONFIG_SYS_DCACHE_OFF*/

/* Enabling this flag will report any L2 errors.
 * By default we are disabling it */
/*#define CONFIG_IPQ_REPORT_L2ERR*/

/*
 * Other commands
 */

#define CONFIG_ARMV7_PSCI

#endif /* _IPQ6018_H */
