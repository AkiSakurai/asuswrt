/*
 * HND RTE misc. service routines.
 * compressed image.
 *
 * Copyright 2020 Broadcom
 *
 * This program is the proprietary software of Broadcom and/or
 * its licensors, and may only be used, duplicated, modified or distributed
 * pursuant to the terms and conditions of a separate, written license
 * agreement executed between you and Broadcom (an "Authorized License").
 * Except as set forth in an Authorized License, Broadcom grants no license
 * (express or implied), right to use, or waiver of any kind with respect to
 * the Software, and Broadcom expressly reserves all rights in and to the
 * Software and all intellectual property rights therein.  IF YOU HAVE NO
 * AUTHORIZED LICENSE, THEN YOU HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY
 * WAY, AND SHOULD IMMEDIATELY NOTIFY BROADCOM AND DISCONTINUE ALL USE OF
 * THE SOFTWARE.
 *
 * Except as expressly set forth in the Authorized License,
 *
 * 1. This program, including its structure, sequence and organization,
 * constitutes the valuable trade secrets of Broadcom, and you shall use
 * all reasonable efforts to protect the confidentiality thereof, and to
 * use this information only in connection with your use of Broadcom
 * integrated circuit products.
 *
 * 2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED
 * "AS IS" AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES,
 * REPRESENTATIONS OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR
 * OTHERWISE, WITH RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY
 * DISCLAIMS ANY AND ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY,
 * NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES,
 * ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
 * CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING
 * OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
 *
 * 3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL
 * BROADCOM OR ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL,
 * SPECIAL, INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR
 * IN ANY WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN
 * IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii)
 * ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF
 * OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS SHALL APPLY
 * NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
 *
 * $Id: rte.c 787482 2020-06-01 09:20:40Z $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <osl_ext.h>
#include <bcmtlv.h>
#include <bcmutils.h>
#include <hndsoc.h>
#include <siutils.h>
#include <sbchipc.h>
#include <hndcpu.h>
#include <hnd_debug.h>
#include <hnd_pktid.h>
#include <hnd_pt.h>
#include "rte_cpu_priv.h"
#include "rte_chipc_priv.h"
#include "rte_gpio_priv.h"
#include <rte_dev.h>
#include <rte_fetch.h>
#include <rte_pktfetch.h>
#include <rte_mem.h>
#include "rte_mem_priv.h"
#include <rte_heap.h>
#include "rte_heap_priv.h"
#include <rte_uart.h>
#include <rte_cons.h>
#include "rte_cons_priv.h"
#include <rte.h>
#include "rte_priv.h"
#include <rte_assert.h>
#include <rte_trap.h>
#include "rte_isr_priv.h"
#include <rte_timer.h>
#include <bcmstdlib_ext.h>
#include "rte_pmu_priv.h"
#include "rte_pktpool_priv.h"
#if defined(BCMUSBDEV) && defined(BCMUSBDEV_ENABLED) && defined(BCMTRXV2) && \
	!defined(BCM_BOOTLOADER)
#include <trxhdr.h>
#include <usbrdl.h>
#endif /* BCMUSBDEV && BCMUSBDEV_ENABLED && BCMTRXV2 && !BCM_BOOTLOADER */
#include <epivers.h>
#include <bcm_buzzz.h>
#include <hndpmu.h>
#include <saverestore.h>
#include <hnd_event.h>
#ifdef RTE_DEBUG_UART
#include <rte_debug.h>
#endif // endif
#include <hnd_gci.h>
#include <rte_cfg.h>
#include "rte_cfg_priv.h"
#include "threadx_priv.h"
#include <hnd_ds.h>
#ifdef SRMEM
#include <hndsrmem.h>
#endif /* SRMEM */
#ifdef HNDBME
#include <hndbme.h>
#endif // endif
#ifdef HNDM2M
#include <hndm2m.h>
#endif // endif
#ifdef BCMHME
#include <bcmhme.h>
#endif // endif
#ifdef HNDPQP
#include <hnd_pqp.h>
#endif // endif
#ifdef SW_PAGING
#include <swpaging.h>
#endif // endif

#if defined(BCMUSBDEV) && defined(BCMUSBDEV_ENABLED) && defined(BCMTRXV2) && \
	!defined(BCM_BOOTLOADER)
extern char *_vars;
extern uint _varsz;
#endif /* BCMUSBDEV && BCMUSBDEV_ENABLED && BCMTRXV2 && !BCM_BOOTLOADER */

si_t *hnd_sih = NULL;		/* Backplane handle */
osl_t *hnd_osh = NULL;		/* Backplane osl handle */

#ifdef INCLUDE_BUILD_SIGNATURE_IN_SOCRAM
static const char BCMATTACHDATA(build_signature_fwid_format_str)[] = "fwid=01-%x";
static const char BCMATTACHDATA(build_signature_ver_format_str)[] = "ver=%d.%d.%d.%d";
#endif // endif
static const char BCMATTACHDATA(epi_version_str)[] = EPI_VERSION_STR;

#if defined(DONGLEBUILD) || defined(RTE_TEST)
/*
 * Define reclaimable NVRAM area; startarm.S will copy downloaded NVRAM data to
 * this array and move the _vars pointer to point to this array. The originally
 * downloaded memory area will be released (reused).
 */
#if defined(WLTEST) || defined(ATE_BUILD)
uint8 nvram_array[NVRAM_ARRAY_MAXSIZE] DECLSPEC_ALIGN(4) = {0};
bool _nvram_reclaim_enb	= FALSE;
#else
uint8 BCMATTACHDATA(nvram_array)[NVRAM_ARRAY_MAXSIZE] DECLSPEC_ALIGN(4) = {0};
bool _nvram_reclaim_enb	= TRUE;
#endif // endif
#endif /* DONGLEBUILD || RTE_TEST */

#ifndef MAX_TRAP_NOTIFY_CBS
#define MAX_TRAP_NOTIFY_CBS	6
#endif /* MAX_TRAP_NOTIFY_CBS */

typedef int (*hnd_trap_notify_cb_t)(void* arg, uint8 trap_type);
typedef struct {
	hnd_trap_notify_cb_t cb;
	void *arg;
} hnd_trap_notify_cb_info_t;

#define HND_LOGBUF_FORMAT_REV	1
#define REGION_7		7

typedef struct hnd_logbuf_fatal {
	uint32 flags;
	uint32 size;
	struct hnd_logbuf_fatal *next;
} hnd_logbuf_fatal_t;

uchar *hnd_fatal_logbuf_addr = NULL;
uint32 hnd_fatal_logbuf_size = 0;

static hnd_trap_notify_cb_info_t  trap_notify_cbs[MAX_TRAP_NOTIFY_CBS];
static bool trap_notified = FALSE;

int
BCMATTACHFN(hnd_register_trapnotify_callback)(void *cb, void *arg)
{
	uint8 i = 0;

	for (i = 0; i < ARRAYSIZE(trap_notify_cbs); i++) {
		if (trap_notify_cbs[i].cb == NULL)
			break;
	}
	if (i == ARRAYSIZE(trap_notify_cbs))
		return BCME_ERROR;
	trap_notify_cbs[i].cb = (hnd_trap_notify_cb_t)cb;
	trap_notify_cbs[i].arg = arg;
	return BCME_OK;
}

/* This function calls all the registered callbacks during a TRAP. Host gets the trap
 * indication from the device about the different core up status, so that it can use that
 * information before taking the coredump from the cores which are in down state
 */
uint32
BCMRAMFN(hnd_notify_trapcallback)(uint8 trap_type)
{
	uint8 i = 0;
	uint32 trap_data = 1;

	/* In case of multiple traps, callbacks should be called only once */
	if (trap_notified == TRUE) {
		return 0;
	}
	trap_notified = TRUE;
	for (i = 0; i < ARRAYSIZE(trap_notify_cbs); i++) {
		if (trap_notify_cbs[i].cb != NULL) {
			trap_data |= trap_notify_cbs[i].cb(trap_notify_cbs[i].arg, trap_type);
		}
	}
	return trap_data;
}

#if defined(RTE_CONS) && !defined(BCM_BOOTLOADER)
static void
hnd_dump_test(void *arg, int argc, char *argv[])
{
	char a = 'a';
	uint32 numc = 8;
	uint32 i;

	if (argc == 2)
		numc = atoi(argv[1]);

	for (i = 0; i < numc; i++) {
		putc(a++);
		if ((a - 'a') == 26)
			a = 'a';
	}
	putc('\n');
}

static void
hnd_memdmp(void *arg, int argc, char **argv)
{
	uint32 addr, size, count;

	if (argc < 3) {
		printf("%s: start len [count]\n", argv[0]);
		return;
	}

	addr = bcm_strtoul(argv[1], NULL, 0);
	size = bcm_strtoul(argv[2], NULL, 0);
	count = argv[3] ? bcm_strtoul(argv[3], NULL, 0) : 1;

	for (; count-- > 0; addr += size) {
		if (size == 4)
			printf("%08x: %08x\n", addr, *(uint32 *)addr);
		else if (size == 2)
			printf("%08x: %04x\n", addr, *(uint16 *)addr);
		else
			printf("%08x: %02x\n", addr, *(uint8 *)addr);
	}
}

/**
 * Malloc memory to simulate low memory environment
 * Memory waste [kk] in KB usage: mw [kk]
 */
static void
hnd_memwaste(void *arg, int argc, char *argv[])
{
	/* Only process if input argv specifies the mw size(in KB) */
	if (argc > 1) {
		printf("%p\n", hnd_malloc(atoi(argv[1]) * 1024));
	}
}
#endif /* RTE_CONS  && !BCM_BOOTLOADER */

/* ================== debug =================== */

hnd_debug_t hnd_debug_info;

hnd_debug_t *
BCMRAMFN(hnd_debug_info_get)(void)
{
	return &hnd_debug_info;
}

static void
BCMATTACHFN(hnd_debug_info_init)(osl_t *osh)
{
	hnd_debug_t *hnd_debug_info_ptr;

	hnd_debug_info_ptr = hnd_debug_info_get();
	memset(hnd_debug_info_ptr, 0, sizeof(*(hnd_debug_info_ptr)));

	/* Initialize the debug area */
	hnd_debug_info_ptr->magic = HND_DEBUG_MAGIC;
	hnd_debug_info_ptr->version = HND_DEBUG_VERSION;
	strncpy(hnd_debug_info_ptr->epivers, epi_version_str, HND_DEBUG_EPIVERS_MAX_STR_LEN - 1);
	/* Force a null terminator at the end */
	hnd_debug_info_ptr->epivers[HND_DEBUG_EPIVERS_MAX_STR_LEN - 1] = '\0';

#ifdef INCLUDE_BUILD_SIGNATURE_IN_SOCRAM
	snprintf(hnd_debug_info_ptr->fwid_signature, HND_DEBUG_BUILD_SIGNATURE_FWID_LEN,
		build_signature_fwid_format_str, gFWID);
	/* Force a null terminator */
	hnd_debug_info_ptr->fwid_signature[HND_DEBUG_BUILD_SIGNATURE_FWID_LEN - 1] = '\0';

	snprintf(hnd_debug_info_ptr->ver_signature, HND_DEBUG_BUILD_SIGNATURE_VER_LEN,
		build_signature_ver_format_str, EPI_MAJOR_VERSION,
		EPI_MINOR_VERSION, EPI_RC_NUMBER, EPI_INCREMENTAL_NUMBER);
	/* Force a null terminator */
	hnd_debug_info_ptr->ver_signature[HND_DEBUG_BUILD_SIGNATURE_VER_LEN - 1] = '\0';
#endif /* INCLUDE_BUILD_SIGNATURE_IN_SOCRAM */

#if defined(RAMBASE)
	hnd_debug_info_ptr->ram_base = RAMBASE;
	hnd_debug_info_ptr->ram_size = RAMSIZE;
#endif // endif

#ifdef ROMBASE
	hnd_debug_info_ptr->rom_base = ROMBASE;
	hnd_debug_info_ptr->rom_size = ROMEND-ROMBASE;
#else
	hnd_debug_info_ptr->rom_base = 0;
	hnd_debug_info_ptr->rom_size = 0;
#endif /* ROMBASE */

	hnd_shared_fwid_get(&hnd_debug_info_ptr->fwid);

#if defined(RTE_CONS)
	hnd_debug_info_ptr->console = hnd_cons_active_cons_state();
	hnd_shared_cons_init(hnd_debug_info_ptr->console);
#endif /* RTE_CONS */
}

/* ====================== time/timer ==================== */

static uint32 now = 0;

/** Return the up time in miliseconds */
uint32
hnd_time(void)
{
	OSL_INTERRUPT_SAVE_AREA

	OSL_DISABLE
	now += hnd_update_now();
	OSL_RESTORE

	return now;
}

static uint64 now_us = 0;  /* Used in latency test for micro-second precision */

uint64
hnd_time_us(void)
{
	OSL_INTERRUPT_SAVE_AREA

	OSL_DISABLE
	now_us += hnd_update_now_us();
	OSL_RESTORE

	return now_us;
}

/** Schedule work callback wrapper to delete the timer */
static void
schedule_work_timer_cb(hnd_timer_t *t)
{
	hnd_timer_auxfn_t auxfn = hnd_timer_get_auxfn(t);

	/* invoke client callback with timer pointer */
	ASSERT(auxfn != NULL);

	BUZZZ_LVL3(HND_WORK_ENT, 1, (uint32)auxfn);
	(auxfn)(t);
	BUZZZ_LVL3(HND_WORK_RTN, 0);

	hnd_timer_free(t);
}

/** Schedule a completion handler to run at safe time */
int
hnd_schedule_work(void *context, void *data, hnd_timer_mainfn_t taskfn, int delay)
{
	hnd_timer_t *task;

	BUZZZ_LVL3(HND_SCHED_WORK, 2, (uint32)taskfn, delay);

	/* XXX: note that this allocates a brand new timer each time, which is freed again in the
	 * callback. If the caller repeatedly calls this function with the same arguments, it may
	 * be more efficient to create a DPC once (hnd_dpc_create) and call it when needed.
	 */
	if (!(task = hnd_timer_create(context, data, schedule_work_timer_cb,
	                              (hnd_timer_auxfn_t)taskfn, NULL))) {
		return BCME_NORESOURCE;
	}

	if (hnd_timer_start(task, delay, FALSE)) {
		return BCME_OK;
	}

	hnd_timer_free(task);
	return BCME_ERROR;
}

/* Sync time with host */
static uint32 host_reftime_delta_ms = 0;

void
BCMRAMFN(hnd_set_reftime_ms)(uint32 reftime_ms)
{
	host_reftime_delta_ms = reftime_ms - hnd_time();
}

uint32
BCMRAMFN(hnd_get_reftime_ms)(void)
{
	return (hnd_time() + host_reftime_delta_ms);
}

/* ======================= assert ======================= */
/* Global ASSERT type */
uint32 g_assert_type = 0;

si_t*
BCMRAMFN(get_hnd_sih)(void)
{
	return hnd_sih;
}

/* ============================= system ============================= */

#ifndef ATE_BUILD
void
hnd_poll(si_t *sih)
{
#ifdef RTE_POLL
	hnd_dev_poll();
	hnd_advance_time();
#endif /* RTE_POLL */
}
#endif /* !ATE_BUILD */

static void
hnd_write_shared_addr_to_host(void)
{
	uint32 asm_stk_bottom = hnd_get_rambottom();
	hnd_shared_stk_bottom_get((uint32 *)(asm_stk_bottom - 4));
}

void
hnd_sys_enab(si_t *sih)
{
	/*
	 * We come here after initialzing everything that is required
	 * to function well (i.e) after c_init(). So we can now write the shared
	 * structure's address to the Host.
	 *
	 * In case of PCIe Full Dongle case the DHD would be waiting in a loop
	 * until this is done.
	 */
	hnd_write_shared_addr_to_host();
#if defined(SAVERESTORE) && defined(__ARM_ARCH_7M__)
	if (SR_ENAB()) {
		sr_process_save(sih);
	}
#endif /* SAVERESTORE && __ARM_ARCH_7M__ */
#ifndef RTE_POLL
	hnd_set_irq_timer(0);	/* kick start timer interrupts driven by hardware */
	hnd_enable_interrupts();
#endif // endif
#ifdef BCMDBG_CPU
	hnd_enable_cpu_debug();
#endif // endif
}

/* ============================ misc =========================== */
void
hnd_unimpl(void)
{
	printf("UNIMPL: ra=%p\n", CALL_SITE);
	HND_DIE();
}

/*
 * ======HND======  Initialize and background:
 *
 *	hnd_init: Initialize the world.
 */

#ifdef _RTE_SIM_
extern uchar *sdrambuf;
#endif // endif

extern uint32 _stackbottom;

uint32 gFWID;

/* SVC mode stack is only used during intialization,
 * and maybe shared by IRQ mode stack.
 */
#ifndef	 SVC_STACK_SIZE
#define	 SVC_STACK_SIZE	512
#endif // endif

static void
BCMATTACHFN(hnd_mem_setup)(uintptr stacktop)
{
	uchar *ramStart, *ramLimit;
	uintptr stackbottom;

	BCM_REFERENCE(stackbottom);

	stackbottom = stacktop - SVC_STACK_SIZE;

	/* Initialize malloc arena */
#if defined(EXT_CBALL)
	ramStart = (uchar *)RAMADDRESS;
	ramLimit = ramStart + RAMSZ;
#elif defined(_RTE_SIM_)
	ramStart = sdrambuf;
	ramLimit = ramStart + RAMSZ;
#elif defined(DEVRAM_REMAP)
	{
	extern char _heap_start[], _heap_end[];
	ramStart = (uchar *)_heap_start;
	ramLimit = (uchar *)_heap_end;
	}
#else /* !EXT_CBALL && !_RTE_SIM_ && !DEVRAM_REMAP */
	ramStart = (uchar *)_end;
#if defined(BCMUSBDEV) && defined(BCMUSBDEV_ENABLED) && defined(BCMTRXV2) && \
	!defined(BCM_BOOTLOADER)
	{
	uint32 asm_ram_bottom = hnd_get_rambottom();
	struct trx_header *trx;
	/* Check for NVRAM parameters.
	 * If found, initialize _vars and _varsz. Also, update ramStart
	 * last 4 bytes at the end of RAM is left untouched.
	 */
	trx = (struct trx_header *) (asm_ram_bottom -(SIZEOF_TRXHDR_V2 + 4));
	/* sanity checks */
	if (trx->magic == TRX_MAGIC && ISTRX_V2(trx) && trx->offsets[TRX_OFFSETS_NVM_LEN_IDX]) {
		_varsz = trx->offsets[TRX_OFFSETS_NVM_LEN_IDX];
		_vars = (char *)(text_start + trx->offsets[TRX_OFFSETS_DLFWLEN_IDX]);
		/* overriding ramStart initialization */
		ramStart = (uchar *)_vars + trx->offsets[TRX_OFFSETS_NVM_LEN_IDX] +
			trx->offsets[TRX_OFFSETS_DSG_LEN_IDX] +
			trx->offsets[TRX_OFFSETS_CFG_LEN_IDX];
	}
	}
#endif /* BCMUSBDEV && BCMUSBDEV_ENABLED && BCMTRXV2 && !BCM_BOOTLOADER */
	ramLimit = (uchar *)stackbottom;
#ifdef STACK_PROT_TRAP
	hnd_stack_prot(ramLimit);
#endif // endif
#endif /* !EXT_CBALL && !_RTE_SIM_ && !DEVRAM_REMAP */

	hnd_arena_init((uintptr)ramStart, (uintptr)ramLimit);
}

static uint32 pmu_dump_buf_size = 0;
static uint32 wrapper_dump_buf_size = 0;

/* common handler to handle the trap notify for chipc and backplane */
static int
hnd_handle_trap(void *ptr, uint8 trap_type)
{
	bcm_xtlv_t *p;
	uint32 allocated_size = 0;
	si_t *sih = (si_t *)ptr;
	const bcm_xtlv_opts_t xtlv_opts = BCM_XTLV_OPTION_ALIGN32;

	if (pmu_dump_buf_size > 0) {
		pmu_dump_buf_size = bcm_xtlv_size_for_data(pmu_dump_buf_size,
			xtlv_opts);
		p = (bcm_xtlv_t *)OSL_GET_FATAL_LOGBUF(hnd_osh, pmu_dump_buf_size,
			&allocated_size);
		if ((p != NULL) && (allocated_size >= pmu_dump_buf_size)) {
			/* add tag and len , each 2 bytes wide Chipc core index */
			p->id = 0;
			p->len = allocated_size - BCM_XTLV_HDR_SIZE_EX(xtlv_opts);
			/* store the information in Raw Data format */
			si_pmu_dump_pmucap_binary(sih, p->data);
		}
	}
	/*
	 * XXX: dump the backplane wrapper registers, use the backplane_scan code
	 * to figure out the wrapper addresses
	 * define the tag like 0xFF to say backplane
	*/
	if (wrapper_dump_buf_size > 0) {
		wrapper_dump_buf_size = bcm_xtlv_size_for_data(wrapper_dump_buf_size,
			xtlv_opts);
		p = (bcm_xtlv_t *)OSL_GET_FATAL_LOGBUF(hnd_osh, wrapper_dump_buf_size,
			&allocated_size);
		if ((p != NULL) && (allocated_size >= wrapper_dump_buf_size)) {
			/* add tag and len , each 2 bytes wide Chipc core index */
			p->id = 0xE0;
			p->len = allocated_size - BCM_XTLV_HDR_SIZE_EX(xtlv_opts);
			/* store the information in Raw Data format */
			si_wrapper_dump_binary(sih, p->data);
		}
	}

	return 1;
}

si_t *
BCMATTACHFN(hnd_init)(void)
{
	/* ******** system init ******** */

	hnd_disable_interrupts();

	/* Initialize trap handling */
	hnd_trap_init();

	hnd_mem_setup((uintptr)_stackbottom);

	/* Initialize shared data area between host and dongle */
	hnd_shared_info_setup();

	/* ******** hardware init ******** */

	/* Now that we have initialized memory management let's allocate the osh */

	hnd_osh = osl_attach(NULL);
	ASSERT(hnd_osh);

#if defined(RTE_CONS)
	/* Create a logbuf separately from a console. This logbuf will be
	 * dumped and reused when the console is created later on.
	 */
	hnd_cons_log_init(hnd_osh);
#endif // endif

	/* Initialise the debug struct that sits in a known location */
	/* N.B: must be called after hnd_cons_log_init */
	hnd_debug_info_init(hnd_osh);

	/* Initialize interrupt handling */
	hnd_isr_module_init(hnd_osh, hnd_worklet_schedule);

	/* Create a scheduler for the main thread */
	ASSERT_THREAD("main_thread");
	hnd_scheduler_create(NULL);

	/* Create main thread statistics object */
	osl_ext_task_current()->stats = hnd_isr_stats_create(OBJ_TYPE_THREAD, "main_thread",
		(void*)NULL, NULL);

	/* Scan backplane */
	hnd_sih = si_kattach(hnd_osh);
	ASSERT(hnd_sih);

#ifdef RTE_CACHED
	/* Initialize coherent and caches */
	hnd_caches_init(get_hnd_sih());
#endif // endif

	/* Initialize chipcommon related stuff */
	hnd_chipc_init(hnd_sih);

	/* Initialize CPU related stuff */
	hnd_cpu_init(hnd_sih);

	/* Initialize timer/time */
	hnd_timer_init(hnd_sih);

#if defined(SRMEM)
	if (SRMEM_ENAB()) {
		srmem_init(hnd_sih);
		hnd_update_sr_size();
	}
#endif // endif

#ifdef RTE_UART
	if (UART_ENAB()) {
		serial_init_devs(hnd_sih, hnd_osh);
	}
#endif // endif
#ifdef RTE_CONS
	/* No printf's go to the UART earlier than this */
	(void)hnd_cons_init(hnd_sih, hnd_osh);
#endif /* RTE_CONS */

#if defined(HND_CPUUTIL)
	hnd_cpuutil_init(hnd_sih); /* after hnd_timer_init() */
#endif /* HND_CPUUTIL */

#ifdef	SBPCI
	/* Init pci core if there is one */
	hnd_dev_init_pci(hnd_sih, hnd_osh);
#endif // endif

#ifdef BCMECICOEX
	/* Initialize ECI registers */
	hnd_eci_init(hnd_sih);
#endif // endif

#ifdef WLGPIOHLR
	/* Initialize GPIO */
	rte_gpio_init(hnd_sih);
#endif // endif

	rte_gci_init(hnd_sih);

	hnd_gci_init(hnd_sih);

#ifdef HND_PT_GIANT
	mem_pt_init(hnd_osh);
#endif // endif

#if defined(BCMPKTIDMAP)
	/*
	 * Initialize the pktid to pktptr map prior to constructing pktpools,
	 * As part of constructing the pktpools a few packets will ne allocated
	 * an placed into the pools. Each of these packets must have a packet ID.
	 */
	hnd_pktid_init(hnd_osh, PKT_MAXIMUM_ID - 1);
#endif // endif
#if defined(BCMPKTPOOL) && defined(BCMPKTPOOL_ENABLED)
	/* initializes several packet pools and allocates packets within these pools */
	if (rte_pktpool_init(hnd_osh) != BCME_OK) {
		return NULL;
	}
#endif // endif
	hnd_fetch_module_init(hnd_osh);
#ifdef HEALTH_CHECK
	hnd_health_check_init(hnd_osh);
#endif /* HEALTH_CHECK */

#ifdef HNDBME
	/* Initialize the BME service */
	if (bme_init(hnd_sih, hnd_osh) == BCME_OK) {
		if (!hnd_cons_add_cmd("bme", hnd_cons_bme_dump, hnd_osh)) {
			return NULL;
		}
	}
#endif /* HNDBME */
#ifdef HNDM2M
	/* Initialize the DD based M2M service */
	if (m2m_init(hnd_sih, hnd_osh) == BCME_OK) {
		if (!hnd_cons_add_cmd("m2m", hnd_cons_m2m_dump, hnd_osh)) {
			return NULL;
		}
	}
#endif /* HNDM2M */

#ifdef BCMPCIEDEV
	if (BCMPCIEDEV_ENAB()) {
		hnd_pktfetch_module_init(hnd_sih, hnd_osh);
#ifdef BCMHME
		(void) hme_init(hnd_osh); /* Initialize Host Memory Extension service */
#ifdef HNDBME
		hme_bind_bme(); /* Bind HME to BME users BME_USR_H2D, BME_USR_D2H */
#endif // endif
#ifdef HNDPQP
		pqp_init(hnd_osh, PQP_REQ_MAX); /* Packet Queue Pager service */
#endif // endif
#endif /* BCMHME */
	}
#endif /* BCMPCIEDEV */
#if defined(SW_PAGING) && defined(SW_PAGING_DEBUG_BUILD)
	if (!hnd_cons_add_cmd("swp", hnd_cons_swp_dump, hnd_osh)) {
		return NULL;
	}
#endif /* SW_PAGING && SW_PAGING_DEBUG_BUILD */

	/* Add a few commands */
#if defined(RTE_CONS) && !defined(BCM_BOOTLOADER)
	if (!hnd_cons_add_cmd("dump", hnd_dump_test, (void *)2) ||
	    !hnd_cons_add_cmd("mw", hnd_memwaste, 0) ||
	    !hnd_cons_add_cmd("md", hnd_memdmp, 0)) {
		return NULL;
	}
#ifdef BCMTRAPTEST
	if (!hnd_cons_add_cmd("tr", traptest, 0)) {
		return NULL;
	}
#endif // endif
#ifdef BCMDBG_ASSERT_TEST
	if (!hnd_cons_add_cmd("as", asserttest, 0)) {
		return NULL;
	}
#endif // endif
#endif /* RTE_CONS  && !BCM_BOOTLOADER */

#ifdef RTE_DEBUG_UART
	if (rte_debug_init(hnd_sih, hnd_osh)) {
		return NULL;
	}
#endif // endif

	hnd_mem_cli_init();
	hnd_heap_cli_init();
	hnd_thread_cli_init();
	hnd_timer_cli_init();

	/* register for a trap notify handler for chipc and backplane */
	pmu_dump_buf_size = si_pmu_dump_buf_size_pmucap(hnd_sih);
	wrapper_dump_buf_size = si_wrapper_dump_buf_size(hnd_sih);
	hnd_register_trapnotify_callback(hnd_handle_trap, (void *)hnd_sih);

	return hnd_sih;
}

/* routine to manage the fatal buffer allocation */
void *
hnd_get_fatal_logbuf(uint32 size_requested, uint32 *allocated_size)
{
	hnd_image_info_t info;
	hnd_logbuf_fatal_t *fatal_log;
	void *ptr;

	if ((hnd_fatal_logbuf_size == 0) && (hnd_fatal_logbuf_addr == NULL)) {
		uchar *hnd_fatal_logbuf_start = NULL;

		hnd_image_info(&info);

#ifdef	__ARM_ARCH_7R__
#if defined(MPU_RAM_PROTECT_ENABLED)
		disable_rodata_mpu_protection();
#endif /* MPU_RAM_PROTECT_ENABLED */
#endif /* __ARM_ARCH_7R__ */

		printf_set_rodata_invalid();

		/* for now use the rodata portion to store the data */
		hnd_fatal_logbuf_start = (uchar *)info._rodata_start;
		hnd_fatal_logbuf_size = info._rodata_end - info._rodata_start;

		/* header information */
		fatal_log = (hnd_logbuf_fatal_t *)hnd_fatal_logbuf_start;
		fatal_log->flags = HND_LOGBUF_FORMAT_REV;
		fatal_log->size = hnd_fatal_logbuf_size;
		/* for the future expansion, for the next log buffer */
		fatal_log->next = NULL;

		hnd_fatal_logbuf_addr =
			hnd_fatal_logbuf_start + sizeof(hnd_logbuf_fatal_t);
		hnd_shared_fatal_init(hnd_fatal_logbuf_start);
		printf("initing the fatal buf block: %p(%d)\n",
			hnd_fatal_logbuf_start, hnd_fatal_logbuf_size);
	}
	ptr = hnd_fatal_logbuf_addr;
	if (size_requested < hnd_fatal_logbuf_size) {
		*allocated_size = size_requested;
		hnd_fatal_logbuf_size -= size_requested;
		hnd_fatal_logbuf_addr += size_requested;
	}
	else {
		*allocated_size = hnd_fatal_logbuf_size;
		hnd_fatal_logbuf_addr += hnd_fatal_logbuf_size;
		hnd_fatal_logbuf_size = 0;
	}
	bzero(ptr, *allocated_size);
	return (ptr);
}

/* Register a GCI GPIO interrupt handler.  Requires our private hnd_sih handle. */
void*
hnd_enable_gci_gpioint(uint8 gpio, uint8 sts, gci_gpio_handler_t hdlr, void *arg)
{
	return (si_gci_gpioint_handler_register(hnd_sih, gpio, sts, hdlr, arg));
}

void
(hnd_disable_gci_gpioint)(void *gci_i)
{
	return (si_gci_gpioint_handler_unregister(hnd_sih, gci_i));
}
