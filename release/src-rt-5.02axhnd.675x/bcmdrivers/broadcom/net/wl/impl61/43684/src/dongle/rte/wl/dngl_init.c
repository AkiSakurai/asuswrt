/*
 * Initialization and support routines for self-booting compressed image.
 *
 * Copyright (C) 2019, Broadcom. All Rights Reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: dngl_init.c 773199 2019-03-14 14:36:42Z $
 */

#if !defined(BCMUSBDEV) && !defined(BCMSDIODEV) && !defined(BCMPCIEDEV)
#error "Bus type undefined"
#endif // endif

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <siutils.h>
#include <hndcpu.h>
#include <bcmdevs.h>
#include <epivers.h>
#include <bcmutils.h>
#include <bcmnvram.h>
#include <hndsoc.h>
#include <sbchipc.h>
#include <hndpmu.h>
#include <dngl_dbg.h>
#include <dngl_bus.h>
#include <hndtcam.h>
#ifdef MSGTRACE
#include <dngl_msgtrace.h>
#endif // endif
#ifdef LOGTRACE
#include <dngl_logtrace.h>
#endif // endif
#ifdef ATE_BUILD
#include "wl_ate_init.h"
#endif // endif
#include <rte_dev.h>
#include <rte_mem.h>
#include <rte_tcam.h>
#include <rte.h>

#include <hnd_boot.h>
#include <hnd_event.h>

#include <dngl_init.h>
#ifdef BCMPCIEDEV
#include <pciedev_rte.h>
#include <dngl_msgbuf.h>
#endif // endif
#ifdef BCMSDIODEV
#include <sdpcmdev_rte.h>
#include <dngl_cdc.h>
#endif // endif
#ifdef BCMUSBDEV
#include <usbdev_rte.h>
#include <dngl_cdc.h>
#endif // endif
#include <wl_rte.h>

#ifdef RTE_ONE_SECOND_PERIODIC_TIMER
#include <rte_timer.h>
#include <rte_one_second_timer.h>
#endif // endif
#ifdef ECOUNTERS
#include <ecounters.h>
#endif // endif

hnd_dev_t *bus_dev = NULL;		/**< contains e.g. pciedev_dev */
hnd_dev_t *net_dev = NULL;		/**< contains e.g. wl_dev */

struct dngl_bus_ops *bus_ops = NULL;	/**< contains e.g. pciedev_bus_ops */
struct dngl_proto_ops *proto_ops = NULL;	/**< contains e.g. msgbuf_proto_ops */

#if defined(BCMUSBDEV) && defined(BCMSDIODEV) && defined(BCMPCIEDEV)
static const char busname[] = "USB-SDIO-PCIE";
#elif defined(BCMUSBDEV) && defined(BCMUSBDEV_ENABLED)
static const char BCMATTACHDATA(busname)[] = "USB";
#elif defined(BCMSDIODEV) && defined(BCMSDIODEV_ENABLED)
static const char BCMATTACHDATA(busname)[] = "SDIO";
#elif defined(BCMPCIEDEV) && defined(BCMPCIEDEV_ENABLED)
static const char busname[] = "PCIE";
#else
static const char BCMATTACHDATA(busname)[] = "UNKNOWN";
#endif // endif

#if defined(BCMMSGBUF)
static const char busproto[] = "MSGBUF";
#elif defined(BCMCDC)
static const char BCMATTACHDATA(busproto)[] = "CDC";
#else
static const char BCMATTACHDATA(busproto)[] = "UNKNOWN";
#endif // endif

static const char BCMATTACHDATA(rstr_nocrc)[] = "nocrc";
static const char BCMATTACHDATA(rstr_crcchk)[] = "crcchk";
static const char BCMATTACHDATA(rstr_crcadr)[] = "crcadr";
static const char BCMATTACHDATA(rstr_crclen)[] = "crclen";
static const char BCMATTACHDATA(rstr_POLL)[] = "-POLL";
static const char BCMATTACHDATA(rstr_empty)[] = "";
static const char BCMATTACHDATA(rstr_RECLAIM)[] = "-RECLAIM";

#ifdef __ARM_ARCH_7A__
extern void *sysmem_regs;
extern uint32 sysmem_rev;
#else
extern void *socram_regs;
extern uint32 socram_rev;
#endif // endif

#ifdef BCMROMOFFLOAD
void hnd_patch_init(void *srp);
void __attribute__ ((weak))
BCMATTACHFN(hnd_patch_init)(void *srp)
{
	/* Avoid patch bits execution in roml builds */
	/* hnd_tcam_disablepatch(srp); */
}
#endif /* BCMROMOFFLOAD */

/** Writes to end of data section, so not to last RAM word */
static void
BCMATTACHFN(get_FWID)(void)
{
	uint8 *tagsrc = (uint8 *)_fw_meta_data;
	uint8 *tagdst = (uint8 *)&gFWID;

	tagdst[0] = tagsrc[27];
	tagdst[1] = tagsrc[28];
	tagdst[2] = tagsrc[29];
	tagdst[3] = tagsrc[30];
}

#if defined(RTE_CRC32_BIN) && defined(BCMSDIODEV_ENABLED)

/* For locations that may differ from download */
struct modlocs {
	uint32 vars;
	uint32 varsz;
	uint32 memsize;
	uint32 rstvec;
	void   *armregs;
	uint32 watermark;
	uint32 chiptype;
	uint32 armwrap;
	void  *ram_regs;
	uint32 ram_rev;
#if defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7A__)
	uint32 rambottom;
	uint32 atcmrambase;
#endif // endif
	uint32  stackbottom;
	uint32 fwid;
};

uint32   crc_computed = 0xdeadbeef;

static void
BCMATTACHFN(chk_image)(void)
{
	/* Default to entire image (excluding the padded crc and fwtag) */
	uint8 *crcadr = (uint8 *)hnd_get_rambase();
	uint32 crclen = (_end - (char *)crcadr);
	uint32 crcchk = ~(*(uint32 *)_fw_meta_data);
	uint32 crcval;

	/* Will need to save/restore modified locations */
	extern char *_vars;
	extern uint32 _varsz, _memsize, orig_rst;
	extern uint32 arm_wrap, chiptype, _stackbottom;
	extern void *arm_regs;
	struct modlocs newvals;

	/*
	 * if (_varsz > NVRAM_ARRAY_SIZE) startarm.S has already overwritten
	 * part of memory, so there is not much we can do but fail miserably
	 */
	while (_varsz > NVRAM_ARRAY_MAXSIZE);

	/* Bail if nvram explicity suppresses CRC check */
	if (getintvar(_vars, rstr_nocrc) == 1)
		goto done;

	/* Save possibly-modified locations and reset to original values */
	newvals.vars = (uint32)_vars;
	newvals.varsz = _varsz;
	newvals.memsize = _memsize;
	newvals.armregs = arm_regs;
	newvals.watermark = __watermark;
	newvals.chiptype = chiptype;
	newvals.armwrap = arm_wrap;
#if defined(__ARM_ARCH_7A__)
	newvals.ram_regs = (void *)sysmem_regs;
	newvals.ram_rev = sysmem_rev;
#else
	newvals.ram_regs = (void *)socram_regs;
	newvals.ram_rev = socram_rev;
#endif // endif

#if defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7A__)
	newvals.rambottom = _rambottom;
	newvals.atcmrambase = _atcmrambase;
#else
	/* __ARM_ARCH_7M__ */
	/* BACKGROUND:- In CR4(7R) ROM starts with 0x000, */
	/* but in CM3(7M) RAM starts with 0x000 */
	newvals.rstvec = *(uint32*)0;
#endif // endif
	newvals.stackbottom = _stackbottom;
	newvals.fwid = gFWID;

#if defined(BCMSDIODEV) || defined(BCMHOSTVARS)
	/* Temporarily copy NVRAM to Arena and clear the original NVRAM area for CRC purposes */
	(void) memcpy(_end + 4, _vars, _varsz);
	(void) memset(_vars, 0, _varsz);
#endif	/* BCMSDIODEV || BCMHOSTVARS */

	__watermark = 0xbbadbadd;
	_vars = NULL;
	_varsz = _memsize = 0;
	arm_wrap = 0;
	arm_regs = NULL;
#if defined(__ARM_ARCH_7R__)	/* See startarm-cr4.S */
	chiptype = 1;
	socram_rev = -1;
	socram_regs = (void *)-1;
	_rambottom = _atcmrambase = 0xbbadbadd;
#elif defined(__ARM_ARCH_7A__)	/* See startarm-ca7.S */
	chiptype = 1;
	sysmem_rev = -1;
	sysmem_regs = (void *)-1;
	_rambottom = _atcmrambase = 0xbbadbadd;
#elif defined(__ARM_ARCH_7M__) /* See startarm-cm3.S */
	socram_rev = chiptype = 0;
	socram_regs = NULL;
#endif // endif
	_stackbottom = 0;
	 gFWID = 0;

#ifdef BCMDBG_ARMRST
	*(uint32*)0 = orig_rst;
	orig_rst = 0;
#endif // endif

	/* Generate the checksum */
	crcval = hndcrc32(crcadr, crclen, CRC32_INIT_VALUE);

	/* Restore modified locations */
	_vars = (char*)newvals.vars;
	_varsz = newvals.varsz;
	_memsize = newvals.memsize;
	orig_rst = *(uint32*)0;
	arm_regs = newvals.armregs;
	__watermark = newvals.watermark;
	chiptype = newvals.chiptype;
	arm_wrap = newvals.armwrap;
#if defined(__ARM_ARCH_7A__)
	sysmem_regs = newvals.ram_regs;
	sysmem_rev = newvals.ram_rev;
#else
	socram_regs = newvals.ram_regs;
	socram_rev = newvals.ram_rev;
#endif // endif
#if defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7A__)
	_rambottom = newvals.rambottom;
	_atcmrambase = newvals.atcmrambase;
#else
	/* __ARM_ARCH_7M__ */
	/* BACKGROUND:- In CR4(7R) ROM starts with 0x000, */
	/* but in CM3(7M) RAM starts with 0x000 */
	*(uint32*)0 = newvals.rstvec;
#endif // endif
	_stackbottom = newvals.stackbottom;
	gFWID = newvals.fwid;

#if defined(BCMSDIODEV) || defined(BCMHOSTVARS)
	/* Copy back NVRAM data to _vars, and clear Arena */
	(void) memcpy(_vars, _end + 4, _varsz);
	(void) memset(_end + 4, 0, _varsz);
#endif	/* BCMSDIODEV || BCMHOSTVARS */

#if defined(SR_ATTACH_MOVE)
	{
		/* A subset of ATTACH text/data has been relocated to the save-restore memory
		 * region. There are now multiple ATTACH sections, which are no longer contiguous,
		 * and the memory in between them is the initial heap/stack. But by the time this
		 * function has been called, the stack has obviously been modified and is in use.
		 * The CRC from the start of RAM to the end of the normal text/data sections has
		 * been calculated above ('crc1' below). Calculate the remaining CRC in sections:
		 *
		 *    [ txt/dat/bss/rclm ] [future heap] [ ...stack... (active) ] [srm]
		 *    |<----- crc1 ------>|<---------- crc2 --------->|<--crc3-->|<crc4>
		 *                      _end                          |-128-| local var stackbottom
		 */

		/* Calculate CRC from the end of the normal text/data sections to the start of the
		 * stack pointer (with a bit extra for good measure).
		 */
		uint32 stack_bottom = 0xdeaddead;
		crcadr = (uint8 *)_end;
		crclen = ((uint8 *)&stack_bottom - crcadr - 128);
		crcval = hndcrc32(crcadr, crclen, crcval);

		/* Calculate CRC for stack - don't use the actual stack which has been modified.
		 * Just run the CRC on a block of zeros.
		 */
		crclen = (_srm_start - _end - crclen);
		crcval = hndcrc32(crcadr, crclen, crcval);

		/* Calculate CRC for ATTACH text/data moved to save-restore region. */
		crcadr = (uint8 *)_srm_start;
		crclen = (_srm_end - (char *)crcadr);
		crcval = hndcrc32(crcadr, crclen, crcval);
	}
#endif /* SR_ATTACH_MOVE */

	crc_computed = crcval;
	while (crcval != crcchk);

done:;

}

#endif /* RTE_CRC32_BIN && BCMSDIODEV_ENABLED */

#ifdef USB_XDCI
static bool  usbd_is30(si_t *sih)
{
	bool usb30d = FALSE;
	uint32 cs;

	cs = sih->chipst;

	dbg("cc status %x", cs);

	/* need to check IFC since CST4350_USB30D_MODE is not reliable */
	if (CST4350_CHIPMODE_USB30D(cs) || CST4350_CHIPMODE_USB30D_WL(cs) ||
		CST4350_CHIPMODE_HSIC30D(cs)) {
		usb30d = TRUE;
	}

	return usb30d;
}
#endif /* USB_XDCI */

/* Set dev/bus/proto global pointers */
static void
BCMATTACHFN(sel_dev)(void)
{
#if defined(BCMUSBDEV) && defined(BCMUSBDEV_ENABLED)
	bus_dev = get_usbdev_dev();
	bus_ops = get_usbdev_bus_ops();
	proto_ops = get_cdc_proto_ops();
#endif /* BCMUSBDEV && BCMUSBDEV_ENABLED */

#if defined(BCMSDIODEV) && defined(BCMSDIODEV_ENABLED)
	bus_dev = get_sdpcmdev_dev();
	bus_ops = get_sdpcmdev_bus_ops();
	proto_ops = get_cdc_proto_ops();
#endif /* BCMSDIODEV && BCMSDIODEV_ENABLED */

#if defined(BCMPCIEDEV)
	if (BCMPCIEDEV_ENAB()) {
		bus_dev = get_pciedev_dev();
		bus_ops = get_pciedev_bus_ops();
		proto_ops = get_msgbuf_proto_ops();
	}
#endif /* BCMPCIEDEV */

	ASSERT(bus_dev != NULL);
	ASSERT(bus_ops != NULL);
	ASSERT(proto_ops != NULL);

	net_dev = get_wl_dev();

	ASSERT(net_dev != NULL);
}

static void add_dev(si_t *sih);

#define CLK_MPT(clk)		(((clk) + 50000) / 1000000)
#define CLK_KPT(clk)		((((clk) + 50000) - CLK_MPT(clk) * 1000000) / 100000)

static char BCMATTACHDATA(rstr_banner)[] =
	"\nRTE (%s-%s%s%s) %s on BCM%s r%d @ %d.%d/%d.%d/%d.%dMHz\n";

static si_t *
BCMATTACHFN(_c_init)(void)
{
	chipcregs_t *cc;
	si_t *sih;
	osl_t *osh;
	char chn[8];

	/* Basic initialization */
	sih = hnd_init();

	/* clear the watchdog counter which may have been set by the bootrom download code */
	si_watchdog(sih, 0);

	osh = si_osh(sih);

#if defined(__arm__) || defined(__thumb__) || defined(__thumb2__)
	/* TODO: Check flags in the arm's resetlog */
#endif // endif

	if (((cc = si_setcoreidx(sih, SI_CC_IDX)) != NULL) &&
	    (R_REG(osh, &cc->intstatus) & CI_WDRESET)) {
		err("Watchdog reset bit set, clearing");
		W_REG(osh, &cc->intstatus, CI_WDRESET);
	}

	/* Initialize and turn caches on */
	caches_on();

#if defined(EVENT_LOG_COMPILE) && !defined(ATE_BUILD)
#ifdef LOGTRACE
	/* Initialize event log host sender */
	logtrace_init(osh);
#endif /* LOGTRACE */
#endif /* EVENT_LOG_COMPILE  && !ATE_BUILD */

#ifdef MSGTRACE
	/* Initialize message log host sender */
	msgtrace_init(osh);
#endif /* MSGTRACE */

#if defined(ECOUNTERS)
	/*
	 * This is a temporary WAR. Details are in the RB: http://wlan-rb.sj.broadcom.com/r/67803/
	 * This is to ensure the runtime checks in the ROM does not crash due to missing function
	 * definition.
	 */
	printf("ecounters enabled/disabled %p\n", ecounters_enabled());
#endif // endif

#if defined(EVENT_LOG_COMPILE) && defined(ECOUNTERS) && !defined(ECOUNTERS_DISABLED)
#if defined(RTE_ONE_SECOND_PERIODIC_TIMER)
	rte_init_one_second_timer(sih);
#endif // endif
	ecounters_init(osh);
#endif /* EVENT_LOG_COMPILE && ECOUNTERS && !ECOUNTERS_DISABLED */

	/* Print the banner */
	printf(rstr_banner,
	       busname, busproto,
#ifdef RTE_POLL
	       rstr_POLL,
#else
	       rstr_empty,
#endif // endif
#ifdef BCM_RECLAIM_INIT_FN_DATA
	       rstr_RECLAIM,
#else
	       rstr_empty,
#endif // endif
	       EPI_VERSION_STR,
	       bcm_chipname(si_chipid(sih), chn, sizeof(chn)), sih->chiprev,
	       CLK_MPT(si_alp_clock(sih)), CLK_KPT(si_alp_clock(sih)),
	       CLK_MPT(si_clock(sih)), CLK_KPT(si_clock(sih)),
	       CLK_MPT(si_cpu_clock(sih)), CLK_KPT(si_cpu_clock(sih)));

	/* Init dev/bus/proto objects global pointers. */
	sel_dev();

	/* Add the USB or SDIO/PCMCIA/PCIE device.  Only one may be defined
	 * at a time, except during a GENROMTBL build where both are
	 * defined to make sure all the symbols are pulled into ROM.
	 */
	add_dev(sih);

#ifdef ATE_BUILD
	/* Perform ATE init */
	wl_ate_init(sih, net_dev);
#endif // endif

#ifdef WIFI_REFLECTOR
	/* Get the interface up */
	wifi_reflector_init(net_dev);
#endif // endif

	return sih;
}

/* add bus/net devices to system */
static void
BCMATTACHFN(add_dev)(si_t *sih)
{
	uint16 id;

#if defined(BCMUSBDEV) && defined(BCMUSBDEV_ENABLED)
	trace("add USB device");
	if (hnd_add_device(sih, bus_dev, USB_CORE_ID, BCM47XX_USBD_ID) != BCME_OK &&
#ifdef USB_XDCI
	    (!usbd_is30(sih) ||
	     (bus_dev->flags = (1 << RTEDEVFLAG_USB30),
	      hnd_add_device(sih, bus_dev, USB30D_CORE_ID, BCM47XX_USB30D_ID) != BCME_OK)) &&
#endif // endif
	    hnd_add_device(sih, bus_dev, USB20D_CORE_ID, BCM47XX_USB20D_ID) != BCME_OK &&
	    hnd_add_device(sih, bus_dev, USB11D_CORE_ID, BCM47XX_USBD_ID) != BCME_OK) {
		err("fail to add USB device!");
		bus_dev = NULL;
	}
#endif /* BCMUSBDEV && BCMUSBDEV_ENABLED */

#if defined(BCMSDIODEV) && defined(BCMSDIODEV_ENABLED)
	trace("add SDIO device");
	if (hnd_add_device(sih, bus_dev, PCMCIA_CORE_ID, SDIOD_FPGA_ID) != BCME_OK &&
	    hnd_add_device(sih, bus_dev, SDIOD_CORE_ID, SDIOD_FPGA_ID) != BCME_OK) {
		err("fail to add SDIO device!");
		bus_dev = NULL;
	}
#endif /* BCMSDIODEV && BCMSDIODEV_ENABLED */

#if defined(BCMPCIEDEV)
	if (BCMPCIEDEV_ENAB()) {
		trace("  c_init: add PCIE device\n");
		if (hnd_add_device(sih, bus_dev, PCIE2_CORE_ID, 0x43ff) != BCME_OK) {
			err("fail to add PCIe device!");
			bus_dev = NULL;
		}
	}
#endif /* BCMPCIEDEV */

#ifndef ATE_BUILD
	/* This assert is unneeded on ATE since there is no sdio core to add bus_dev. */
	ASSERT(bus_dev);
#endif /* !ATE_BUILD */

	/* Add the WL device, they are exclusive */
	if ((id = si_d11_devid(sih)) == 0xffff)
		id = BCM4318_D11G_ID;
	trace("add WL device 0x%x", id);
	if (hnd_add_d11device(sih, net_dev, id) != BCME_OK) {
		err("fail to add WL device!");
		net_dev = NULL;
	}

	ASSERT(net_dev);

	/* If USB/SDIO/PCMCIA/PCI device is there then open it */
	if (bus_dev != NULL && net_dev != NULL) {
		if (bus_ops->binddev(bus_dev, net_dev,
			si_numd11coreunits(sih)) < 0) {
			err("%s%s device binddev failed", busname, busproto);
		}

#ifndef BCMPCIEDEV
		if (bus_dev->ops->open(bus_dev)) {
			err("%s%s device open failed", busname, busproto);
		}
#endif // endif
	}

	/* Initialize hnd_event */
	if (bus_dev != NULL) {
		hnd_event_init(si_osh(sih), bus_dev);
	}
}

/* Declare a runtime flag with value 0 to trick the compiler
 * to not complain "defined but not used" warning...and also
 * to allow the optimizer to remove any code referenced under
 * 'if (0) {}' only.
 */
BCM_ATTACH_REF_DECL()

/* c_init is wrapper function and is forced to be non discard so that
 * it can be referenced in a gloabl non discarded data structure which
 * otherwise will trigger the linker error "X referenced in section Y of Z.o:
 * defined in discarded section xxx of Z.o".
 * Use the 'if (0) {}' trick mentioned above to remove the wrapped
 * function in builds where it is not needed such as in ROML builds
 * but in builds where the wrapped function is actually needed such as
 * in RAM or ROM offload builds add the wrapped function in reclaim_protect.cfg
 * to skip the discarded code referenced in non discarded section error.
 * Keep this function out of ROML builds to force it in RAM in ROM offload builds.
 */
static si_t *
BCMRAMFN(c_init)(void)
{
	if (BCM_ATTACH_REF()) {
		return _c_init();
	}
	return NULL;
}

/*
 * Common code for triggering any iovar/ioctl's after firmware initialization.
 * Currently used by reflcetor and ULP
 */
#if defined(WIFI_REFLECTOR) || defined(BCMULP)
typedef struct _init_cmds_t {
	uint32	cmd;
	uint32	len;
	char	*data;
	int	value;
} init_cmds_t;

#define MAX_INITCMD_BUFLEN	(32)

static void
BCMATTACHFN(init_cmds_process)(hnd_dev_t *bcmwl, const init_cmds_t *wifi_init_cmds, int count)
{
	int i;
	char buf[MAX_INITCMD_BUFLEN];
	const char *src;
	int idx = 0;

	for (i = 0; i < count; ++i) {
		if ((strlen(wifi_init_cmds[i].data) + sizeof(int) + 1) > MAX_INITCMD_BUFLEN) {
			printf("%s: ignoring cmd idx[%d]\n", __FUNCTION__, i);
			continue;
		}
		src = (char*)&wifi_init_cmds[i].value;
		if (wifi_init_cmds[i].data != NULL) {
			idx = strlen(wifi_init_cmds[i].data);
			memcpy(buf, wifi_init_cmds[i].data, idx);
			buf[idx] = '\0';
		} else
			idx = 0;
		buf[++idx] = src[0];
		buf[++idx] = src[1];
		buf[++idx] = src[2];
		buf[++idx] = src[3];

		bcmwl->ops->ioctl(bcmwl, wifi_init_cmds[i].cmd,
			buf, wifi_init_cmds[i].len, NULL, NULL, FALSE);
	}
}
#endif /* defined(WIFI_REFLECTOR) || defined (BCMULP) */

#ifdef WIFI_REFLECTOR
static void
BCMATTACHFN(wifi_reflector_init)(hnd_dev_t *bcmwl)
{
	static const init_cmds_t wifi_init_cmds[] = {
		{WLC_UP, 0x0, NULL, 0x0},
		{WLC_SET_VAR, 0x8, "mpc", 0},
		{WLC_SET_WSEC, 0x4, NULL, 0x0},
		{WLC_SET_VAR, 0xf, "slow_timer", 999999},
		{WLC_SET_VAR, 0xf, "fast_timer", 999999},
		{WLC_SET_VAR, 0x12, "glacial_timer", 999999},
		{WLC_LEGACY_LINK_BEHAVIOR, 0x04, NULL, 0x1},
		{WLC_SET_MONITOR, 0x4, NULL, 0x1}
	};
	init_cmds_process(bcmwl, wifi_init_cmds, ARRAYSIZE(wifi_init_cmds));
}
#endif /* WIFI_REFLECTOR */

#ifdef BCMULP
static void ulp_post_reclaim(hnd_dev_t *bcmwl)
{
	static const init_cmds_t ulp_init_cmds[] = {
		{WLC_SET_VAR, 0xf, "ulp_wlc_up", 0x0}
	};
	init_cmds_process(bcmwl, ulp_init_cmds, ARRAYSIZE(ulp_init_cmds));
}
#endif // endif

static uint32
BCMATTACHFN(rte_check_iginore_defslave_decode_error)(void)
{
	uint32 def_slave_addr;
	aidmp_t *def_slave;
	uint8 axi_id;
	uint32 val;
	uint32 slave_err_addr = 0;

	/* check for the chip specific values addr, axiid for ARM */
	/* handle the axi error the default way */

	def_slave_addr = 0;
	axi_id = 0;
	def_slave = (aidmp_t *)def_slave_addr;
	if (def_slave == NULL)
		return slave_err_addr;

	/* read errlog register */
	val = R_REG(NULL, &def_slave->errlogstatus);
	if ((val & AIELS_TIMEOUT_MASK) == AIELS_DECODE) {
		val = R_REG(NULL, &def_slave->errlogid);
		/* check that transaction is from the CR4 Prefetch unit */
		if (val & axi_id) {
			slave_err_addr = R_REG(NULL, &def_slave->errlogaddrlo);
			W_REG(NULL, &def_slave->errlogdone, AIELD_ERRDONE_MASK);
			while ((val = R_REG(NULL, &def_slave->errlogstatus)) &
					AIELS_TIMEOUT_MASK);
		}
	}
	return slave_err_addr;
}

static uint32 boot_slave_err_addr = 0;

static void
BCMATTACHFN(c_image_init)(void)
{
#if defined(RTE_CRC32_BIN) && defined(BCMSDIODEV_ENABLED)
	chk_image();
#endif /* (RTE_CRC32_BIN) && defined(BCMSDIODEV_ENABLED) */

#if defined(WAR_ENABLE_CONFIG_WRCAP_AXI2APB)
	{
		/* XXX: WAR for HW JIRA HW4349-475:
		* Enable configure WR_CAP=1 for AXI2APB bridge
		* Set bit 1 of location 0x18204008. With this WAR, any write
		* to a register where corresponding core is in reset, causes
		* an AXI timeout and TRAP the ARM.
		*/
		uint32 *wr_cap_addr =
			(uint32 *)OSL_UNCACHED(SI_NIC400_GPV_BASE + SI_GPV_WR_CAP_ADDR);
		W_REG(NULL, wr_cap_addr, SI_GPV_WR_CAP_EN);
	}
#endif /* defined(WAR_ENABLE_CONFIG_WRCAP_AXI2APB) */

	/* CPU Instruction prefetch unit can generate accesses to undef memory before MPU init */
	boot_slave_err_addr = rte_check_iginore_defslave_decode_error();
}

#ifdef BCMROMOFFLOAD
static void
BCMATTACHFN(init_patch)(void)
{
#ifdef BCMTCAM
	/* Load patch table */
	hnd_tcam_load_default(hnd_get_rambase());
#endif // endif
#ifndef __ARM_ARCH_7A__
	/* "Patch hardware" initialization */
	if ((socram_rev == 4) || (socram_rev == 5)) {
		hnd_patch_init(socram_regs);
	}
#endif // endif
}
#endif /* BCMROMOFFLOAD */

static void
BCMATTACHFN(pre_reclaim_mem)(si_t *sih)
{
#ifdef BCM_RECLAIM_INIT_FN_DATA
	bcm_reclaimed = TRUE;
#endif /* BCM_RECLAIM_INIT_FN_DATA */

#ifdef BCMTCAM
	hnd_tcam_reclaim();
#endif // endif

#if !defined(WLTEST) && !defined(BCMDBG_DUMP) && !defined(ATE_BUILD)
	/* After nvram_exit(), NVRAM variables are no longer accessible */
	nvram_exit((void*)sih);
#endif // endif
}

static void
post_reclaim_mem(si_t *sih)
{
#ifdef BCMULP
	/* Get the interface up */
	if (si_is_warmboot()) {
		ulp_post_reclaim(net_dev);
	}
#endif // endif
}

/* reclaim_mem is wrapper function and is forced to be non discard so that
 * it can be referenced in a gloabl non discarded data structure which
 * otherwise will trigger the linker error "X referenced in section Y of Z.o:
 * defined in discarded section xxx of Z.o".
 * Use the 'if (0) {}' trick mentioned above to remove the wrapped
 * function in builds where it is not needed such as in ROML builds
 * but in builds where the wrapped function is actually needed such as
 * in RAM or ROM offload builds add the wrapped function in reclaim_protect.cfg
 * to skip the discarded code referenced in non discarded section error.
 * Keep this function out of ROML builds to force it in RAM in ROM offload builds.
 */
static void
BCMRAMFN(reclaim_mem)(si_t *sih)
{
	if (BCM_ATTACH_REF()) {
		pre_reclaim_mem(sih);
		hnd_reclaim();
		post_reclaim_mem(sih);
	}
}

/* entry C function from assembly */
/* c_main is non-reclaimable, as it is the one calling hnd_reclaim. */
static si_t *
_c_main(void)
{
	si_t *sih;

	/* Call reclaimable init function */
	sih = c_init();

	/* Upon return from this function all BCMATTACHFN/DATA attributed
	 * sections are reclaimed i.e. memories are reused...
	 */
	reclaim_mem(sih);

#ifdef BCMPCIEDEV
	if (BCMPCIEDEV_ENAB()) {
		if (bus_dev->ops->open(bus_dev)) {
			err("%s%s device open failed", busname, busproto);
		}
	}
#endif // endif

	ca7_execute_protect_on(sih);

	return sih;
}
