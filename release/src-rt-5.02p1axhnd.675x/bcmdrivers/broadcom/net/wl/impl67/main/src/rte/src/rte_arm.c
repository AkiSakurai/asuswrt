/** @file hnd_rte_arm.c
 *
 * HND RTE support routines for ARM CPU.
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
 * $Id: rte_arm.c 786720 2020-05-06 04:39:24Z $
 */

#include <typedefs.h>
#include <osl.h>
#include <osl_ext.h>
#include <siutils.h>
#include <hndsoc.h>
#include <hndcpu.h>
#include <hndarm.h>
#include <sbchipc.h>
#include "rte_pmu_priv.h"
#include <rte_cons.h>
#include <rte_trap.h>
#include <rte.h>
#include "rte_priv.h"
#include <rte_mem.h>
#include "rte_mem_priv.h"
#include <rte_dev.h>
#include <rte_timer.h>
#include <rte_cfg.h>
#include "rte_cfg_priv.h"
#include <pcie_core.h>
#include <bcm_buzzz.h>
#include <hnd_debug.h>
#include <bcmdevs.h>
#include <bcmutils.h>
#include <rte_mmu.h>
#ifdef SW_PAGING
#include <swpaging.h>
#endif // endif

/* ======================= trap ======================= */

/* Registered halt callback function. */
static hnd_halt_hdlr_t g_hnd_haltfn = NULL;
static void *g_hnd_haltfn_ctx = NULL;

/* Used to propagate the up status of various cores to the host. g_trap_data
 * value can be 0x3 when wl is down, 0x1B(27) when both wl cores are up, and so forth
*/
uint32 g_trap_data = 0;
static uint8 host_trap_notify_count = 0;

/* Print stack */
void
hnd_print_stack(uint32 sp)
{
	uint i = 0;
	uint j = 0;
	uint32 *stack = (uint32 *)sp;
	uint stack_headroom = (hnd_get_rambottom() - sp) / 4;
	hnd_image_info_t info;

	hnd_image_info(&info);

	printf("  sp+0  %08x %08x %08x %08x\n",
		stack[0], stack[1], stack[2], stack[3]);
	printf("  sp+10 %08x %08x %08x %08x\n\n",
		stack[4], stack[5], stack[6], stack[7]);

	for (i = 0, j = 0; j < 16 && i < stack_headroom; i++) {
		/* make sure it's a odd (thumb) address and at least 0x100 */
		if (!(stack[i] & 1) || stack[i] < 0x100)
			continue;

		/* Check if it's within the RAM or ROM text regions */
		if ((stack[i] <= (uint32)info._text_end) ||
#if defined(ROMBASE)
		    ((stack[i] >= ROMBASE && (stack[i] <= ROMEND))) ||
#endif /* BCMROMOFFLOAD */
		    FALSE) {
			printf("sp+%x %08x\n", (i*4), stack[i]);
			j++;
		}
	}
}

/* Common trap handling */
void
hnd_trap_common(trap_t *tr)
{
	hnd_debug_t *hnd_debug_info_ptr;

	hnd_debug_info_ptr = hnd_debug_info_get();
	/* Save the trap pointer */
	hnd_debug_info_ptr->trap_ptr = tr;

	BUZZZ_LVL1(HND_TRAP, 2, tr->pc, tr->type);

#if defined(__ARM_ARCH_7M__)
#ifndef RTE_POLL
	if (BCMOVLHW_ENAB(hnd_sih) && (tr->type == TR_BUS)) {
		if (si_arm_ovl_int(hnd_sih, tr->pc))
			return;
	}
#endif	/* !RTE_POLL */

#ifndef	BCMSIM
	if ((tr->type == TR_FAULT) && (tr->pc  == ((uint32)&osl_busprobe & ~1))) {
		/*	LDR and ADR instruction always sets the least significant bit
		 *	of program address to 1 to indicate thumb mode
		 *	LDR R0, =osl_busprobe ; R0 = osl_busprobe + 1
		 */
		/* printf("busprobe failed for addr 0x%08x\n", tr->pc); */
		*((volatile uint32 *)(tr->r13 + CM3_TROFF_PC)) = tr->pc + 6;
		*((volatile uint32 *)(tr->r13 + CM3_TROFF_R0)) = ~0;
		return;
	}
#endif	/* BCMSIM */

	BCM_BUZZZ_STOP();
#endif /* __ARM_ARCH_7M__ */

#if defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7A__)
#endif /* __ARM_ARCH_7M__ || __ARM_ARCH_7A__ */

	hnd_shared_trap_print();

#if (defined(BCMUSBDEV_ENABLED) && !defined(BCM_BOOTLOADER)) || defined(FWID)
	printf("\nFWID 01-%x\n", gFWID);
#endif // endif

	/*
	 * ARM7TDMI trap types (__ARM_ARCH_7R__ / __ARM_ARCH_7A__):
	 *	0=RST, 1=UND, 2=SWI/SVC, 3=IAB, 4=DAB, 5=BAD, 6=IRQ, 7=FIQ
	 *
	 * ARM CM3 trap types (__ARM_ARCH_7M__):
	 *	1=RST, 2=NMI, 3=FAULT, 4=MM, 5=BUS, 6=USAGE, 11=SVC,
	 *	12=DMON, 14=PENDSV, 15=SYSTICK, 16+=ISR
	 */
	/* Note that UTF parses the first line, so the format should not be changed. */
	printf("\nTRAP %x(%x): pc %x, lr %x, sp %x, cpsr %x, spsr %x\n",
	       tr->type, (uint32)tr, tr->pc, tr->r14, tr->r13, tr->cpsr, tr->spsr);

#if defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7A__)
	if (tr->type == TR_DAB)
		printf("  dfsr %x, dfar %x\n",
		       get_arm_data_fault_status(), get_arm_data_fault_address());
	else if (tr->type == TR_IAB)
		printf("  ifsr %x, ifar %x\n",
		       get_arm_instruction_fault_status(), get_arm_instruction_fault_address());
#endif // endif

	if (SRPWR_ENAB()) {
		printf("  srpwr: 0x%08x clk:0x%x pmu:0x%x 0x%x 0x%x\n",
			si_srpwr_request(hnd_sih, 0, 0),
			si_corereg(hnd_sih, SI_CC_IDX, OFFSETOF(chipcregs_t, clk_ctl_st), 0, 0),
			PMU_REG(hnd_sih, pmustatus, 0, 0),
			PMU_REG(hnd_sih, res_state, 0, 0),
			PMU_REG(hnd_sih, res_pending, 0, 0));
	}

#if defined(BCMSPACE) || defined(RTE_CONS)
	printf("  r0 %x, r1 %x, r2 %x, r3 %x, r4 %x, r5 %x, r6 %x\n",
	       tr->r0, tr->r1, tr->r2, tr->r3, tr->r4, tr->r5, tr->r6);
	printf("  r7 %x, r8 %x, r9 %x, r10 %x, r11 %x, r12 %x\n",
	       tr->r7, tr->r8, tr->r9, tr->r10, tr->r11, tr->r12);

	hnd_print_stack(tr->r13);
#endif /* BCMSPACE || RTE_CONS */

	hnd_shared_trap_init(tr);

	/*
	 * flush entire cache so host has latest information
	 * upon handling the trap.
	 */
	cpu_flush_cache_all();

	g_trap_data = hnd_notify_trapcallback(tr->type);
} /* hnd_trap_common */

/*
 * By default we trap on reads/writes to addresses 0-127 in low memory.  This detects
 * illegal NULL pointer dereferences, including some like ((struct xxx)NULL)->field.
 * The trap region may be overridden in a Makefile for debugging special cases.  E.g.,
 * if a write corruption is occurring somewhere from 0x21488 to 0x2149f, one could change
 * the region start to 0x21480, region size to 5 (32 bytes), and trap type to WRITE (6).
 */
#ifndef BCMDBG_TRAP_LG2SIZE
#define BCMDBG_TRAP_LG2SIZE	7
#endif // endif

#ifndef BCMDBG_TRAP_BASE
#define BCMDBG_TRAP_BASE	0x00000000	/* Must be multiple of 2^LG2SIZE */
#endif // endif

#ifndef BCMDBG_TRAP_TYPE
#define BCMDBG_TRAP_TYPE	CM3_DWT_FUNCTION_WP_RDWR	/* 5=rd, 6=wr, 7=rd/wr */
#endif // endif

#if defined(__ARM_ARCH_7R__)
#ifdef  MPU_RAM_PROTECT_ENABLED
/* the first two MPU regions are being set in startarm-cr4.S */
/* mpu third region is reserved for host memory protection */
#define RAM_BASE_180000	0x180000
#define RAM_BASE_170000	0x170000

#endif  /* MPU_RAM_PROTECT_ENABLED */
#endif	/* __ARM_ARCH_7R__ || */

#if defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7A__)
#define	HT_CLOCK_120		120000000
static void hnd_cpu_clock_init(uint hz);
#endif	/* __ARM_ARCH_7R__ || __ARM_ARCH_7A__ */

/* Trap initialization */
void
BCMATTACHFN(hnd_trap_init)(void)
{
#if defined(BCMDBG_TRAP) && !defined(_RTE_SIM_)
	/*
	 * When BCMDBG_TRAP is true, set up CPU to trap on as many errors as possible.
	 * Capabilities vary by CPU.
	 */

#if defined(__ARM_ARCH_7M__)
	/* Enable traps for detecting divide by zero */
	wreg32(CM3_CFGCTRL, rreg32(CM3_CFGCTRL) | CM3_CFGCTRL_DIV_0_TRP);
#endif // endif

#if defined(__ARM_ARCH_7M__)
	/* Disable alignment of trap stack to a multiple of 8 bytes; confuses trap handler */
	wreg32(CM3_CFGCTRL, rreg32(CM3_CFGCTRL) & ~CM3_CFGCTRL_STKALIGN);
#endif // endif

#if defined(__ARM_ARCH_7M__)
	/* Enable DWT (data watchpoint and tracing) functionality and Monitor Mode. */
	wreg32(CM3_DBG_EMCR, rreg32(CM3_DBG_EMCR) | CM3_DBG_EMCR_TRCENA | CM3_DBG_EMCR_MON_EN);

	/* Set address mask to ignore the bottom N bits to match block of given size */
	wreg32(CM3_DWT_MASK0, BCMDBG_TRAP_LG2SIZE);

	/* Set comparator value to match the block at given address. */
	wreg32(CM3_DWT_COMP0, BCMDBG_TRAP_BASE);

	/* Set function; first clear MATCH bit in case already set */
	(void)rreg32(CM3_DWT_FUNCTION0);
	wreg32(CM3_DWT_FUNCTION0, BCMDBG_TRAP_TYPE);

	/* The priority of all exceptions and interrupts defaults to 0 (highest).  The data
	 * watchpoint exception will not occur while handling an interrupt if they're the same
	 * priority, and most of our code executes at interrupt level, so set the priority of
	 * interrupts to the next highest priority, which is 0x20 since priority is configured
	 * for 3 bits and is held in the upper 3 bits of an 8-bit value.
	 */
	wreg32(CM3_NVIC_IRQ_PRIO0, 1U << 5);
#endif /* __ARM_ARCH_7M__ */

#endif /* BCMDBG_TRAP && !_RTE_SIM_ */

#if defined(__ARM_ARCH_7R__)
#ifdef  MPU_RAM_PROTECT_ENABLED
	/* The assembly MPU setup in initiating the MPU for all the 4G memeory space */
	/* we are calling this funciton to set the write protection to the code area */
	/* we are not doing the write protection to the code area in the assembly file */
	/* because it can change from build to build. */
	mpu_protect_code_area();
#endif  /* MPU_RAM_PROTECT_ENABLED */
#endif	/* defined(__ARM_ARCH_7R__) */

#if defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7A__)
/* for CR4, clock at start is configured as 120Mhz [instead of default 80MHz].
 * So, make sure that, c0counts_per_us is inited with approx count before for all delays
 * used before hnd_cpu_init().
 */
	hnd_cpu_clock_init(HT_CLOCK_120);
	/* enable imprecise aborts/exceptions */
	enable_arm_dab();
#endif /* __ARM_ARCH_7R__ || defined(__ARM_ARCH_7A__) */

	/* Set trap handler */
	hnd_set_trap(hnd_trap_handler);
} /* hnd_trap_init */

void
hnd_inform_host_die(void)
{
	/* Simply return if already trap is informed to host. */
	if (++host_trap_notify_count > 1) {
		return;
	}
	/* Call halt function, if it was registered. */
	if (g_hnd_haltfn != NULL) {
		printf("logging: \t");
		if (si_check_enable_backplane_log(hnd_sih)) {
			printf("enabled\n");
			(*g_hnd_haltfn)(g_hnd_haltfn_ctx, g_trap_data);
		}
		else {
			printf("not enabled\n");
			(*g_hnd_haltfn)(g_hnd_haltfn_ctx, 0);
		}
	}
}

/*
 * Enter and stay in busy loop forever.
 */
void
hnd_infinite_loop(void)
{
	/* Flush out any console print messages that have just been
	 * added as FW will no longer service THR interrupt
	 */
	hnd_cons_flush();

	while (1) {
#ifdef RTE_DEBUG_UART
		hnd_cons_check();
#endif // endif
	}
}

void
hnd_the_end(void)
{
	BUZZZ_LVL1(HND_DIE, 1, (uint32)CALL_SITE);
	BCM_BUZZZ_STOP();

	/* Flush out any pending console print messages
	 * ensures there is room for critical log messages that
	 * may follow
	 */
	hnd_cons_flush();

#ifdef BCMDBG
	/* Disable watchdog to allow post-mortem debugging */
	hnd_thread_watchdog_enable(FALSE);
#endif /* BCMDBG */

#if !defined(BCM_BOOTLOADER)
	hnd_inform_host_die();
#endif /* !BCM_BOOTLOADER */

	hnd_infinite_loop();
}

void
hnd_set_fwhalt(hnd_halt_hdlr_t hdlr, void *ctx)
{
	g_hnd_haltfn = hdlr;
	g_hnd_haltfn_ctx = ctx;
}

/* ================ CPU stats ================= */

#ifdef BCMDBG_CPU
static hnd_timer_t *cu_timer;	/* timer for cpu usage calculation */
#define DEFUPDATEDELAY 5000

typedef struct {
	uint32 totalcpu_cycles;
	uint32 usedcpu_cycles;
	uint32 cpusleep_cycles;
	uint32 min_cpusleep_cycles;
	uint32 max_cpusleep_cycles;
	uint32 num_wfi_hit;
	uint32 last;
} hnd_cpu_stats_t;

static hnd_cpu_stats_t cpu_stats;
static uint32 enterwaittime = 0xFFFFFFFF;

static void
hnd_update_stats(hnd_cpu_stats_t *cpustats)
{
	cpustats->totalcpu_cycles = cpu_stats.totalcpu_cycles;
	cpustats->usedcpu_cycles = cpu_stats.usedcpu_cycles;
	cpustats->cpusleep_cycles = cpu_stats.cpusleep_cycles;
	cpustats->min_cpusleep_cycles = cpu_stats.min_cpusleep_cycles;
	cpustats->max_cpusleep_cycles = cpu_stats.max_cpusleep_cycles;
	cpustats->num_wfi_hit = cpu_stats.num_wfi_hit;

	/* clean it off */
	bzero(&cpu_stats, sizeof(cpu_stats));
}

static void
hnd_clear_stats(void)	/* Require separate routine for only stats clearance? */
{
	bzero(&cpu_stats, sizeof(cpu_stats));
}

static void
hnd_print_cpuuse(void *arg, int argc, char *argv[])
{
	si_t *sih = (si_t *)arg;
	printf("CPU stats to be displayed in %d msecs\n", DEFUPDATEDELAY);

	/* Force HT on */
	si_clkctl_cc(sih, CLK_FAST);

	/* Schedule timer for DEFUPDATEDELAY ms */
	hnd_timer_start(cu_timer, DEFUPDATEDELAY, 0);

	/* Clear stats and restart counters */
	hnd_clear_stats();
}

static void
hnd_update_timer(hnd_timer_t *t)
{
	si_t *sih = (si_t *)hnd_timer_get_ctx(t);
	hnd_cpu_stats_t cpustats;

	hnd_update_stats(&cpustats);

	/* Disable FORCE HT, which was enabled at hnd_print_cpuuse */
	si_clkctl_cc(sih, CLK_DYNAMIC);

	printf("Total cpu cycles : %u\n"
			"Used cpu cycles : %u\n"
			"Total sleep cycles (+.05%%): %u\n"
			"Average sleep cycles: %u\n"
			"Min sleep cycles: %u, Max sleep cycles: %u\n"
			"Total number of wfi hit %u\n",
			cpustats.totalcpu_cycles,
			cpustats.usedcpu_cycles,
			cpustats.cpusleep_cycles,
			cpustats.cpusleep_cycles/cpustats.num_wfi_hit,
			cpustats.min_cpusleep_cycles, cpustats.max_cpusleep_cycles,
			cpustats.num_wfi_hit);
}

void
hnd_cpu_stats_upd(uint32 start_time)
{
	uint32 current_time = 0;
	uint32 totalcpu_cycles = 0;
	uint32 usedcpu_cycles = 0;
	uint32 cpuwait_cycles = 0;

	/* get total cpu cycles */
	totalcpu_cycles = (start_time == 0) ? 0 : (enterwaittime - start_time);
	cpu_stats.totalcpu_cycles += totalcpu_cycles;

	/* get used cpu cycles */
	current_time = get_arm_inttimer();
	usedcpu_cycles = (current_time == 0) ? 0 : (enterwaittime - current_time);
	cpu_stats.usedcpu_cycles += usedcpu_cycles;

	/* get sleep cpu cycle */
	cpuwait_cycles = (cpu_stats.last == 0) ? 0 : (cpu_stats.last - start_time);
	cpu_stats.cpusleep_cycles += cpuwait_cycles;

	/* update last cpu usage time */
	cpu_stats.last = current_time;

	if (cpu_stats.num_wfi_hit == 0) {
		cpu_stats.min_cpusleep_cycles = cpuwait_cycles;
		cpu_stats.max_cpusleep_cycles = cpuwait_cycles;
	}

	/* update min max cycles in sleep state */
	cpu_stats.min_cpusleep_cycles =
		((cpuwait_cycles < cpu_stats.min_cpusleep_cycles)) ?
	        cpuwait_cycles : cpu_stats.min_cpusleep_cycles;
	cpu_stats.max_cpusleep_cycles =
		((cpuwait_cycles > cpu_stats.max_cpusleep_cycles)) ?
	        cpuwait_cycles : cpu_stats.max_cpusleep_cycles;
	cpu_stats.num_wfi_hit++;
}

void
hnd_enable_cpu_debug(void)
{
	set_arm_inttimer(enterwaittime);
}

void
hnd_update_cpu_debug(void)
{
	set_arm_inttimer(enterwaittime);
}
#endif	/* BCMDBG_CPU */

/* ================ CPU profiling ================= */

#if defined(HND_CPUUTIL)

/*
 * -------------- Broadcom HND CPU Utilization Tool for ThreadX ---------------
 *
 * CPU elapsed cycles are maintained for ISR, DPC, Timers, threads and user
 * defined code segments.
 *
 * ARM Low Power Mode:
 * Default idle thread uses ARM wfi to enter low power mode. WFI stops the
 * free running counter and a lower granularity PMU counter is relied upon to
 * wakeup the ARM on a possible active timer, if no other interrupt had to occur
 * before the timer expiry. To keep the high resolution cycle counter to be free
 * running, a busy loop cpuutil thread at a priority level higher (+1) than the
 * default wfi based idle thread is created. The cpuutil idle thread is
 * suspended and needs to be resumed explicitly when the CPU utilization tool is
 * on demand activated. This allow CPU Util Tool to be compiled in production
 * without impacting low power mode in field deployment. The CPU util tool needs
 * to be explicitly deactivated, resulting in a re-suspension of the cpuutil
 * idle thread, with the default wfi based idle thread enabling ARM low power.
 * When CPU Util tool is "activated", the CPU Util idle thread effectively
 * masks (scheduled prior to) the default system idle thread.
 *
 * CPU Utilization Accumulation Bins:
 * CPU Utilization is collected by binning into accumulator contexts. A max of
 * 27 dedicated contexts and 5 default contexts for Thread, ISR Action, Timer
 * Deferred Procedure Calls and User defined segments. When all 27 dedicated
 * named contexts are in use, all further requests for accumulator contexts are
 * overflowed into the default overflow contexts. The default contexts are hence
 * shared by multiple execution instances of the same context type.
 * Dedicated contexts may be shared too, if the type and function is the same.
 * E.g. Several timer instances may be created with the same timer function, say
 * wlc_main_timer(). Also hnd_timer_t may fire in multiple hnd_dpc_t instances,
 * wherein all such hnd_dpc_t objects have the same dpc_fn, namely
 * handle_dpc_for_timer(). Shared named cpuutil contexts are reference counted.
 *
 * CPU Util Finite State Machine (FSM)
 * The cost for ISR and TMR are subtracted from the preempted thread, DPC or
 * User context. For instance, the cost for ISR occurrences and TMR expiries
 * in the idle or main thread are not assigned to idle thread or main thread.
 * Likewise ISR and TMR cost is subtraced from the DPC that they nest in.
 *
 * CPU Utilization is computed over an epoch interval of 1 second.
 * CPU Utilization is computed by accumulating cycles elapsed (ARM Cycle Count)
 * in an execution context.
 *
 * CPU Utilization tool may be managed using the "cons" mechanism.
 * - "cons cu_bgn" : Start CPU Utilization tool
 *                   resume cpuutil_thread, start periodic epoch timer
 * - "cons cu_end" : Stop  CPU Utilization tool
 *                   suspend cpuutil_thread, stop periodic epoch timer
 * - "cons cu_dbg" : Verbose report the utilization of the last completed epoch.
 * - "cons cu"     : Report the utilization of the last completed epoch.
 *
 *
 * CPU Utilization Tool Implementation Constraints:
 *
 * 1. ISR nesting is not supported. (FIQ inside an ISR?)
 * 2. TMR nesting is not supported.
 * 3. Only RTE contexts bound to a CPU Util context are managed.
 * 4. Maximum 27 unique contexts are tracked. All other contexts share a common
 *    default context (by type).
 * 5. Multiple RTE contexts having the same function handler are treated share
 *    the same CPU Util context (reference counted to a max of 32 K).
 * 6. Single CPU Core.
 * 7. FSM assumes that cur context pointers are never NULL.
 *
 * -----------------------------------------------------------------------------
 */

/**
 * Section: CPU Util Tool definitions and macros
 */

/** CPU Util Debug of HND_CPUUTIL_TRANS_USR_ENT/EXT transactions */
// #define HND_CPUUTIL_USR_DEBUG

/** CPU Util: Epoch interval over which the running CPU usage is summarized */
#define HND_CPUUTIL_INTERVAL        1000 /* 1000 millisecs = 1 second */

/** CPU Util: Total number CPU contexts supported. */
#define HND_CPUUTIL_CONTEXTS_MAX    32   /* uint32 bitmap */
#define HND_CPUUTIL_CONTEXT_INVALID HND_CPUUTIL_CONTEXT_TYPE_MAX

/** CPU Util: Use ARM's CycleCount register */
#define HND_CPUUTIL_CYCLES_PER_USEC (CYCLES_PER_USEC) /* xyz-ramk.mk */

/** CPU Util: Compute elapsed time with wrapover */
#define HND_CPUUTIL_ELAPSED(t1, t2) \
	((t2) > (t1)) ? ((t2) - (t1)) : (~0U - (t1) + (t2))

/** CPU Util: Runtime state of Tool's sub-components.
 * Do not use TRUE/FALSE for runtime state and use below ON/OFF for runtime
 * state manipulation of CPU Util sub-components thread, timer, contexts.
 */
#define HND_CPUUTIL_RUNTIME_ON      (~0)
#define HND_CPUUTIL_RUNTIME_OFF     (0)

/** CPU Util: Avoid NULL current context pointers in FSM. */
#define HND_CPUUTIL_GARBAGE  (&hnd_cpuutil_g.context[HND_CPUUTIL_CONTEXTS_MAX])

/** CPU Util: shortcut accessors for fields in hnd_cpuutil_g global object. */
#define _CUR_THR                    hnd_cpuutil_g.cur_thr
#define _CUR_ISR                    hnd_cpuutil_g.cur_isr
#define _CUR_TMR                    hnd_cpuutil_g.cur_tmr
#define _CUR_DPC                    hnd_cpuutil_g.cur_dpc
#define _CUR_USR                    hnd_cpuutil_g.cur_usr

#if !defined(__ARM_ARCH_7A__)
#define HND_CPUUTIL_EVENTID         (0)
#define HND_CPUUTIL_IS_CCNT()       (TRUE)
#define HND_CPUUTIL_NOW()           get_arm_cyclecount()
#define HND_CPUUTIL_CNTDWN(u32)     (FALSE)
#else  /* __ARM_ARCH_7A__ */
/* CPU Performance Monitoring Unit: -march=armv7-a | -mcpu=cortex-a7 -DCA7 */
typedef uint32 (*hnd_cpuutil_now_t)(void); /* Get cycle or armv7 pmu event */
static uint32 __armv7_pmu_ctr0_rd(void);   /* ARMv7 PMU counter 0 */
static void   __armv7_pmu_clear(void);     /* Reset PMU counter */
#define HND_CPUUTIL_EVENTID         (hnd_cpuutil_g.pmu_eventid)
#define HND_CPUUTIL_IS_CCNT()       (hnd_cpuutil_g.pmu_eventid == 0U)
#define HND_CPUUTIL_NOW()           hnd_cpuutil_g.now()
#define HND_CPUUTIL_CNTDWN(u32)     ((u32 & 0xFF000000) == 0xFF000000)
#endif /* __ARM_ARCH_7A__ */

/**
 * Section: CPU Util Tool Object types
 */

/** CPU Util accumulator bin per RTE execution context */
typedef struct hnd_cpuutil_context
{
	uint32 start_time; /* cycle counter value on ENT event, cur epoch */
	uint32 total_time; /* Total cycles elapsed in this context, cur epoch */

	uint32 func;       /* func name used as execution context */
	uint8  type;       /* type of context: THR, ISR, TMR, DPC */
	uint8  state;      /* bound to a RTE execution context */
	int16  refcnt;     /* reference count, e.g. several wl_timer_main and dpc */
} hnd_cpuutil_context_t;

/** State for CPU Util Tool */
typedef struct hnd_cpuutil
{
	/* CPU Util FSM cur context pointers. Must never be NULL */
	hnd_cpuutil_context_t *cur_thr; /* current executing thread context */
	hnd_cpuutil_context_t *cur_isr; /* current executing isr context */
	hnd_cpuutil_context_t *cur_tmr; /* current executing timer context */
	hnd_cpuutil_context_t *cur_dpc; /* current executing dpc context */
	hnd_cpuutil_context_t *cur_usr; /* current executing usr context */

#if defined(__ARM_ARCH_7A__)
	uint32         pmu_eventid;     /* 0 = Cycle count or ARMv7 PMU Event */
	hnd_cpuutil_now_t now;          /* Get ARMv7 cycle count or PMU Counter 0 */
#endif // endif

	/* CPU Util Accumulators : 5 default + 27 named + 1 garbage */
	hnd_cpuutil_context_t context[HND_CPUUTIL_CONTEXTS_MAX + 1];
	uint32         cur_events[HND_CPUUTIL_CONTEXT_TYPE_MAX];
	uint32         cur_delays;      /* total delay usecs in epoch */

	/* Fields used on epoch timer expiry */
	uint32         cur_epoch;       /* counts epoch - timer expiry */
	uint32         cur_start;       /* start time of current epoch */

	/* Completed epoch state collected for reporting */
	uint32         idle_period;     /* idle time in last completed epoch */
	uint32         util_time[HND_CPUUTIL_CONTEXTS_MAX]; /* contexts util */
	uint32         util_events[HND_CPUUTIL_CONTEXT_TYPE_MAX];
	uint32         util_period;     /* total time in last completed epoch */
	uint32         util_delays;     /* total delays in last completed epoch */

	/* CPU Util Tool Runtime State of sub-components */
	union {
		uint16     state;           /* Collective state of all sub-components */
		struct {
			uint8  thread_state;    /* idle thread suspended or active */
			uint8  timer_state;     /* epoch timer stopped or periodic */
		};
	} runtime;

	/* CPU Util Tool RTE sub-systems */
	hnd_cpuutil_context_t *idle_context; /* cpuutil context of cpuutil_thread */

	osl_ext_task_t cpuutil_thread;  /* TX_THREAD/osl_ext_task_t object */
	hnd_timer_t    *timer;          /* timer to advance an epoch */

	/* CPU Util Tool Context Management: get/put. 1b1 = FREE, 1b0 = INUSE */
	uint32         context_bmap;    /* context alloc/free 32 bit map */

	/* CPU Util Tool Errors/Debug Support */
	uint32         errors;

#if defined(HND_CPUUTIL_USR_DEBUG)
	hnd_cpuutil_context_t *usr_debug;
#endif // endif

} hnd_cpuutil_t;

/**
 * Section: CPU Util Tool Global State
 *
 * see also stack for cpuutil thread: cpuutil_thread_stack_g
 *
 * Global structure using BSS segment:
 * - Until tool is activated, all cur_xyz point to a garbage context
 * - context_bmap may be used much before hnd_cpuutil_init().
 */

static hnd_cpuutil_t hnd_cpuutil_g =
{
	// All current contexts point to the garbage accumulator context
	.cur_thr           = HND_CPUUTIL_GARBAGE,
	.cur_isr           = HND_CPUUTIL_GARBAGE,
	.cur_tmr           = HND_CPUUTIL_GARBAGE,
	.cur_dpc           = HND_CPUUTIL_GARBAGE,
	.cur_usr           = HND_CPUUTIL_GARBAGE,

#if defined(__ARM_ARCH_7A__)
	.pmu_eventid       = 0, // 0 = CycleCount
	.now               = get_arm_cyclecount,
#endif // endif

	// Mark "default contexts" as allocated in context_bmap
	.context_bmap      = 0xFFFFFFE0, // [0 .. 4] = { THR, ISR, TMR, DPC, USR }
};

/** Used in UI: context type to string conversion */
static const char *_hnd_cpuutil_context_s[HND_CPUUTIL_CONTEXT_TYPE_MAX] =
{
	"\e[0;32mTHR\e[0m", // _G_
	"\e[0;31mISR\e[0m", // _R_
	"\e[0;35mTMR\e[0m", // _M_
	"\e[0;34mDPC\e[0m", // _B_
	"\e[0;33mUSR\e[0m"  // _Y_
};

/** CPU Util Tool: Dummy functions for overflow default contexts */
static void BCMRAMFN(cpuutil_thr)(void) { /* dummy function */ }
static void BCMRAMFN(cpuutil_isr)(void) { /* dummy function */ }
static void BCMRAMFN(cpuutil_tmr)(void) { /* dummy function */ }
static void BCMRAMFN(cpuutil_dpc)(void) { /* dummy function */ }
static void BCMRAMFN(cpuutil_usr)(void) { /* dummy function */ }

/**
 * Section: CPU Util Tool Debug/UI Support
 */

static void /* for use only in debug */
_hnd_cpuutil_dbg(void)
{
	printf("CPU Util status: thread=%u timer=%u bmap=0x%08x MHz=%u Errors=%u\n",
		(hnd_cpuutil_g.runtime.thread_state == (uint8)HND_CPUUTIL_RUNTIME_ON),
		(hnd_cpuutil_g.runtime.timer_state == (uint8)HND_CPUUTIL_RUNTIME_ON),
		hnd_cpuutil_g.context_bmap, HND_CPUUTIL_CYCLES_PER_USEC,
		hnd_cpuutil_g.errors);

} /* _hnd_cpuutil_dbg */

/**
 * Section: CPU Util Tool FSM
 *
 * hnd_cpuutil_notify is invoked on transitions from one RTE context to another.
 * - ISR and TMR can preempt current THR and DPC.
 *   Elapsed time in ISR and DPC must be subtracted from ongoing THR and DPC.
 * - ISR or TMR nesting is not supported.
 *
 * Caller ensure that interrupts are disabled in hnd_cpuutil_notify().
 */
void
hnd_cpuutil_notify(uint32 trans_type, void *ctx)
{
	uint32 now, elapsed;
	hnd_cpuutil_context_t *context = (hnd_cpuutil_context_t*)ctx;

	/* Quickly exit FSM: do not impact performance when tool is disabled */
	if (hnd_cpuutil_g.runtime.state != (uint16)HND_CPUUTIL_RUNTIME_ON)
		return;

	if (context == (hnd_cpuutil_context_t*)NULL) {
		hnd_cpuutil_g.errors++;
		printf("%s:\e[0;31m ERROR NULL context [@%p] errors=%u\e[0m\n",
			__FUNCTION__, CALL_SITE, hnd_cpuutil_g.errors);
		return;
	}

	BUZZZ_LVL5(HND_CPUUTIL_TRANS, 2, context->func, trans_type);

	now = HND_CPUUTIL_NOW();

	/* cur_xyz pointers must never be NULL. */
	switch (trans_type) {

		case HND_CPUUTIL_TRANS_THR_ENT:      // Threadx context switch
			elapsed = HND_CPUUTIL_ELAPSED(_CUR_THR->start_time, now);
			_CUR_THR->total_time += elapsed; // cur_thr switches out
			_CUR_THR = context;              // transition to new cur thread
			_CUR_THR->start_time = now;      // NEW Thread starts
			hnd_cpuutil_g.cur_events[HND_CPUUTIL_CONTEXT_THR]++;
		break;

		case HND_CPUUTIL_TRANS_ISR_ENT:
			elapsed = HND_CPUUTIL_ELAPSED(_CUR_THR->start_time, now);
			_CUR_THR->total_time += elapsed; // cur_thr interrupted by ISR
			elapsed = HND_CPUUTIL_ELAPSED(_CUR_DPC->start_time, now);
			_CUR_DPC->total_time += elapsed; // cur_dpc interrupted by ISR
			elapsed = HND_CPUUTIL_ELAPSED(_CUR_USR->start_time, now);
			_CUR_USR->total_time += elapsed; // cur_usr interrupted by ISR
			_CUR_ISR = context;              // setup ISR context
			_CUR_ISR->start_time = now;      // ISR begins
			hnd_cpuutil_g.cur_events[HND_CPUUTIL_CONTEXT_ISR]++;
		break;

		case HND_CPUUTIL_TRANS_ISR_EXT:
			elapsed = HND_CPUUTIL_ELAPSED(_CUR_ISR->start_time, now);
			_CUR_ISR->total_time += elapsed; // cur_isr completed
			_CUR_ISR = HND_CPUUTIL_GARBAGE;  // no ISR nesting
			_CUR_THR->start_time = now;      // THR resumes
			_CUR_DPC->start_time = now;      // DPC resumes
			_CUR_USR->start_time = now;      // USR resumes
		break;

		case HND_CPUUTIL_TRANS_TMR_ENT:
			elapsed = HND_CPUUTIL_ELAPSED(_CUR_THR->start_time, now);
			_CUR_THR->total_time += elapsed; // cur_thr interrupted by TMR
			elapsed = HND_CPUUTIL_ELAPSED(_CUR_DPC->start_time, now);
			_CUR_DPC->total_time += elapsed; // cur_dpc interrupted by TMR***
			elapsed = HND_CPUUTIL_ELAPSED(_CUR_USR->start_time, now);
			_CUR_USR->total_time += elapsed; // cur_dpc interrupted by TMR***
			_CUR_TMR = context;              // setup cur_tmr
			_CUR_TMR->start_time = now;      // TMR begins
			hnd_cpuutil_g.cur_events[HND_CPUUTIL_CONTEXT_TMR]++;
		break;

		case HND_CPUUTIL_TRANS_TMR_EXT:
			elapsed = HND_CPUUTIL_ELAPSED(_CUR_TMR->start_time, now);
			_CUR_TMR->total_time += elapsed; // cur_tmr completed
			_CUR_TMR = HND_CPUUTIL_GARBAGE;  // no TMR nesting
			_CUR_THR->start_time = now;      // THR resumes
			_CUR_DPC->start_time = now;      // DPC resumes
			_CUR_USR->start_time = now;      // USR resumes
		break;

		case HND_CPUUTIL_TRANS_DPC_ENT:
			_CUR_DPC = context;              // setup cur_dpc
			_CUR_DPC->start_time = now;      // DPC begins
			hnd_cpuutil_g.cur_events[HND_CPUUTIL_CONTEXT_DPC]++;
		break;

		case HND_CPUUTIL_TRANS_DPC_EXT:
			elapsed = HND_CPUUTIL_ELAPSED(_CUR_DPC->start_time, now);
			_CUR_DPC->total_time += elapsed; // DPC completes
			_CUR_DPC = HND_CPUUTIL_GARBAGE;  // no DPC nesting
		break;

		case HND_CPUUTIL_TRANS_USR_ENT:
			_CUR_USR = context;              // setup cur_usr
			_CUR_USR->start_time = now;      // USR begins
			hnd_cpuutil_g.cur_events[HND_CPUUTIL_CONTEXT_USR]++;
		break;

		case HND_CPUUTIL_TRANS_USR_EXT:
			elapsed = HND_CPUUTIL_ELAPSED(_CUR_USR->start_time, now);
			_CUR_USR->total_time += elapsed; // USR completes
			_CUR_USR = HND_CPUUTIL_GARBAGE;  // no USR nesting
		break;

		default:
			hnd_cpuutil_g.errors++;
			printf("%s:\e[0;31m ERROR transition=%u [@%p] errors=%u\e[0m\n",
				__FUNCTION__, trans_type, CALL_SITE, hnd_cpuutil_g.errors);
			return;
	}

} /* hnd_cpuutil_notify */

/**
 * Section: CPU Util Tool Context Management
 *
 * 5 types of RTE Execution contexts are tracked. A set of 27 dedicated
 * "named" CPU util contexts can be supported. Beyond the total 27 "named"
 * contexts, five default shared contexts are assigned.
 *
 * Dedicated name contexts may be shared. Sharing is defined by the func. A
 * request for a named context for a given "func" will use a previously
 * allocated dedicated named context with the same "func". Sharing is manged
 * using the refcnt field. This is especially used in generic timers and their
 * dpc contexts in which they execute.
 *
 * CPU Util contexts may be allocated/deallocated independent of the runtime
 * status of the CPU Util Tool.
 *
 * Context allocation/deallocation routines may not access FSM fields like
 * cur_xyz.
 * Only newly allocated/deallocated hnd_cpuutil_context fields may be accessed.
 * hnd_cpuutil_context::refcnt may only be accessed for a shared context.
 */

void * /* Allocate CPU Util context for a threadx/hnd execution context */
hnd_cpuutil_context_get(hnd_cpuutil_context_type_t ctx_type, uint32 func)
{
	int ctx_ix;
	uint32 bmap;
	hnd_cpuutil_context_t *context;

	TX_INTERRUPT_SAVE_AREA

	ASSERT(ctx_type < HND_CPUUTIL_CONTEXT_TYPE_MAX);
	ASSERT(func != 0U);

	TX_DISABLE // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

	for (ctx_ix = HND_CPUUTIL_CONTEXT_TYPE_MAX; ctx_ix < 32; ctx_ix++) {
		if ((hnd_cpuutil_g.context[ctx_ix].func == func) &&
		    (hnd_cpuutil_g.context[ctx_ix].type == ctx_type))
		{
			context = &hnd_cpuutil_g.context[ctx_ix];
			/* shared context, so only update refcnt */
			context->refcnt++;
			goto tx_restore_done;
		}
	}

	/* If no more dedicated named contexts are available, use default shared */
	if (hnd_cpuutil_g.context_bmap == 0U) { /* depleted named contexts */
		/* Use the default shared context type on depletion of named contexts */
		context = &hnd_cpuutil_g.context[ctx_type];
		ASSERT(context->state == (uint8)HND_CPUUTIL_RUNTIME_ON);
		ASSERT(context->refcnt > 0);
		context->refcnt++;
		goto tx_restore_done;
	}

	/* Allocate an available named context */
	bmap = hnd_cpuutil_g.context_bmap;
	bmap = hnd_cpuutil_g.context_bmap;
	bmap = (uint32)((int)bmap & (-(int)bmap));
	ctx_ix = 31 - bcm_count_leading_zeros(bmap); /* use clz */
	ASSERT(ctx_ix < 32);

	/* Mark new named context as in-use */
	clrbit(&hnd_cpuutil_g.context_bmap, ctx_ix);

	/* Prepare new named context */
	context = &hnd_cpuutil_g.context[ctx_ix];
	context->start_time = 0U;
	context->total_time = 0U;
	context->func       = func;
	context->type       = ctx_type;
	context->state      = (uint8)HND_CPUUTIL_RUNTIME_ON;
	context->refcnt     = 1;

tx_restore_done:

	/* Post condition for a fully formed runtime on context */
	ASSERT(context->func   != 0U);
	ASSERT(context->type   == ctx_type);
	ASSERT(context->state  == (uint8)HND_CPUUTIL_RUNTIME_ON);
	ASSERT(context->refcnt >  0);

	TX_RESTORE // -------------------------------------------------------------

	return context;

} /* hnd_cpuutil_context_get */

void * /* Free a previously allocated CPU Util context */
hnd_cpuutil_context_put(hnd_cpuutil_context_type_t ctx_type, void *ctx)
{
	int ctx_ix;
	hnd_cpuutil_context_t *context = (hnd_cpuutil_context_t*)ctx;

	TX_INTERRUPT_SAVE_AREA

	if (context == NULL) {
		printf("%s: warning NULL\n", __FUNCTION__);
		return NULL;
	}

	ASSERT(ctx_type < HND_CPUUTIL_CONTEXT_TYPE_MAX);
	ASSERT((context >= (&hnd_cpuutil_g.context[0])) &&
	       (context < (HND_CPUUTIL_GARBAGE))); /* last garbage never alloced */

	ASSERT(context->func != 0U);
	ASSERT(context->type != HND_CPUUTIL_CONTEXT_INVALID);
	ASSERT(context->state == (uint8)HND_CPUUTIL_RUNTIME_ON);
	ASSERT(context->refcnt > 0);

	ctx_ix = (int)(((hnd_cpuutil_context_t *)context) - hnd_cpuutil_g.context);

	TX_DISABLE // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

	context->refcnt--;

	/* Only named contexts are truly freed */
	if ((ctx_ix >= HND_CPUUTIL_CONTEXT_TYPE_MAX) && (context->refcnt <= 0)) {
		context->start_time = 0U;
		context->total_time = 0U;
		context->func       = 0U;
		context->type       = HND_CPUUTIL_CONTEXT_INVALID;
		context->state      = (uint8)HND_CPUUTIL_RUNTIME_OFF;
		context->refcnt		= 0;

		setbit(&hnd_cpuutil_g.context_bmap, ctx_ix);
	}

	TX_RESTORE // -------------------------------------------------------------

	return NULL; /* caller unbinds ... */

} /* hnd_cpuutil_context_put */

static void /* CPU Util Tool Timer that advances an epoch */
hnd_cpuutil_advance(hnd_timer_t *t)
{
	uint32 i, now;

	TX_INTERRUPT_SAVE_AREA

	TX_DISABLE // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

	now = HND_CPUUTIL_NOW();

	/* Collect all completed epoch's results */

	for (i = 0; i < HND_CPUUTIL_CONTEXT_TYPE_MAX; i++) {
		hnd_cpuutil_g.util_events[i] = hnd_cpuutil_g.cur_events[i];
		hnd_cpuutil_g.cur_events[i] = 0U; /* setup for new epoch */
	}

#if defined(HND_CPUUTIL_USR_DEBUG)
	/* Estimate cost for hnd_cpuutil_advance */
	hnd_cpuutil_notify(HND_CPUUTIL_TRANS_USR_ENT, hnd_cpuutil_g.usr_debug);
#endif /* HND_CPUUTIL_USR_DEBUG */

	/* Total idle time in last epoch */
	hnd_cpuutil_g.idle_period =
		hnd_cpuutil_g.idle_context->total_time;

	for (i = 0; i < HND_CPUUTIL_CONTEXTS_MAX; i++) {
		hnd_cpuutil_g.util_time[i] = hnd_cpuutil_g.context[i].total_time;
		hnd_cpuutil_g.context[i].total_time = 0U; /* setup for new epoch */
	}
	hnd_cpuutil_g.idle_context->total_time = 0U;

	/* Total time in last epoch */
	hnd_cpuutil_g.util_period =
		HND_CPUUTIL_ELAPSED(hnd_cpuutil_g.cur_start, now);

	/* Total delay usecs in last epoch */
	hnd_cpuutil_g.util_delays = hnd_cpuutil_g.cur_delays;

#if defined(__ARM_ARCH_7A__)
	if (!HND_CPUUTIL_IS_CCNT()) {
		__armv7_pmu_clear(); /* reset PMU counter for this epoch */
		now = HND_CPUUTIL_NOW();
	}
#endif /* __ARM_ARCH_7A__ */

	/* Setup start of new epoch. (see also above for loops) */
	hnd_cpuutil_g.cur_start   =
		_CUR_THR->start_time  =
		_CUR_ISR->start_time  = // redundant
		_CUR_TMR->start_time  =
		_CUR_DPC->start_time  =
		_CUR_USR->start_time  = now;

	hnd_cpuutil_g.cur_delays  = 0U;

	hnd_cpuutil_g.cur_epoch++; /* advance current epoch */

	BUZZZ_LVL1(HND_CPUUTIL_EPOCH, 2, now, hnd_cpuutil_g.cur_epoch);

#if defined(HND_CPUUTIL_USR_DEBUG)
	printf("CPU Util: %u %u\n", now, hnd_cpuutil_g.cur_epoch); // debug cyccnt

	/* Estimate cost for hnd_cpuutil_advance with printf */
	hnd_cpuutil_notify(HND_CPUUTIL_TRANS_USR_EXT, hnd_cpuutil_g.usr_debug);
#endif /* HND_CPUUTIL_USR_DEBUG */

	TX_RESTORE // -------------------------------------------------------------

} /* hnd_cpuutil_advance */

/**
 * Section: CPU Util Tool Idle Thread Support
 *
 * Creation of a cpuutil thread that runs a busy while (1) idle loop, its
 * suspension and resumption. Other than the thread priority, it uses the same
 * thread configuration as the default idle thread.
 */

char cpuutil_thread_stack_g[IDLE_STACK_SIZE] DECLSPEC_ALIGN(16);

static osl_ext_task_t * /* Pointer to TX_THREAD represented as osl_ext_task_t */
BCMRAMFN(get_cpuutil_thread)(void)
{
	return &hnd_cpuutil_g.cpuutil_thread;
}

static char * /* Pointer to cpuutil thread's stack memory */
BCMRAMFN(get_cpuutil_thread_stack)(void)
{
	return cpuutil_thread_stack_g;
}

static void /* cpuutil thread's execution body */
cpuutil_thread_entry(osl_ext_task_arg_t arg)
{
	while (TRUE); /* busy loop */
}

/**
 * Section: CPU Util Tool User Interface Support:
 * Console cu_bgn, cu_end, cu_dbg and cu handlers
 */

static void /* CPU Util Tool Activation using "cons cu_bgn" */
hnd_cpuutil_activate(void *arg, int argc, char *argv[])
{
	uint32 i, now;

	TX_INTERRUPT_SAVE_AREA

	if (hnd_cpuutil_g.timer == NULL) {
		printf("%s: Error - Tool not initialized\n", __FUNCTION__);
		_hnd_cpuutil_dbg();
		return;
	}

	if (hnd_cpuutil_g.runtime.state == (uint16)HND_CPUUTIL_RUNTIME_ON) {
		printf("%s: Tool already activated\n", __FUNCTION__);
		_hnd_cpuutil_dbg();
		return;
	}

#if defined(__ARM_ARCH_7A__)
	if (!HND_CPUUTIL_IS_CCNT()) {
		__armv7_pmu_clear(); /* reset PMU counter */
	}
#endif /* __ARM_ARCH_7A__ */

	TX_DISABLE // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

	now = HND_CPUUTIL_NOW();

	for (i = 0; i < HND_CPUUTIL_CONTEXTS_MAX; i++) {
		hnd_cpuutil_g.context[i].start_time = now;
		hnd_cpuutil_g.context[i].total_time = 0U;
		hnd_cpuutil_g.util_time[i] = 0U;
	}

	for (i = 0; i < HND_CPUUTIL_CONTEXT_TYPE_MAX; i++) {
		hnd_cpuutil_g.cur_events[i] = 0U;
		hnd_cpuutil_g.util_events[i] = 0U;
	}

	/* Prepare the start state */

	/* Fetch current thread's cpuutil context */
	_CUR_THR = CURRENT_THREAD_CPUUTIL;

	/* Approximation: FSM is not running.
	 * current DPC should need to point to:
	 * DPC=pciedev_dpc_thread's cpuutil context as cons commands come via bus.
	 * Instead cur_dpc is pointing to the dummy cpuutil context.
	 */
	_CUR_DPC = HND_CPUUTIL_GARBAGE;

	/* Approximation: FSM is not running. So garbage first epoch.
	 * We do not know whether we are inside a usr context.
	 */
	_CUR_USR = HND_CPUUTIL_GARBAGE;

	/* Activate is not hooked into isr or timer RTE execution context */
	_CUR_ISR = HND_CPUUTIL_GARBAGE;
	_CUR_TMR = HND_CPUUTIL_GARBAGE;

	/* Start epochs */
	hnd_cpuutil_g.cur_epoch   = 0U;

	hnd_cpuutil_g.cur_start   =
		_CUR_THR->start_time  =
		_CUR_ISR->start_time  = // redundant
		_CUR_TMR->start_time  = // redundant
		_CUR_DPC->start_time  =
		_CUR_USR->start_time  = now;

	hnd_cpuutil_g.idle_period = 0U;
	hnd_cpuutil_g.util_period = 0U;

	hnd_cpuutil_g.cur_delays  = 0U;
	hnd_cpuutil_g.util_delays = 0U;

	TX_RESTORE // -------------------------------------------------------------

	/* Resume the CPU Util idle thread - preempt the default wfi idle thread */
	if (hnd_cpuutil_g.runtime.thread_state == (uint8)HND_CPUUTIL_RUNTIME_OFF) {
		if (osl_ext_task_resume(get_cpuutil_thread()) != OSL_EXT_SUCCESS) {
			printf("%s: Failed to resume Idle thread\n", __FUNCTION__);
			return;
		}
		hnd_cpuutil_g.runtime.thread_state = (uint8)HND_CPUUTIL_RUNTIME_ON;
	}

	/* ARM the timer for a periodic expiry of 1 sec */
	if (hnd_cpuutil_g.runtime.timer_state == (uint8)HND_CPUUTIL_RUNTIME_OFF) {
		if (hnd_timer_start(hnd_cpuutil_g.timer, HND_CPUUTIL_INTERVAL, TRUE)
		        == FALSE)
		{
			printf("%s: Failed to start periodic timer\n", __FUNCTION__);
			osl_ext_task_suspend(get_cpuutil_thread());
			return;
		}
		hnd_cpuutil_g.runtime.timer_state = (uint8)HND_CPUUTIL_RUNTIME_ON;
	}

	ASSERT(hnd_cpuutil_g.runtime.state == (uint16)HND_CPUUTIL_RUNTIME_ON);

	printf("CPU Util Tool Activated\n");

} /* hnd_cpuutil_activate */

static void /* CPU Util Tool Deactivation using "cons cu_end" */
hnd_cpuutil_deactivate(void *arg, int argc, char *argv[])
{
	TX_INTERRUPT_SAVE_AREA

	if (hnd_cpuutil_g.runtime.state == (uint16)HND_CPUUTIL_RUNTIME_OFF) {
		printf("%s: Tool already deactivated\n", __FUNCTION__);
		_hnd_cpuutil_dbg();
		return;
	}

	TX_DISABLE // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

	/* Map all current cpu contexts to garbage accumulator context */
	_CUR_THR = _CUR_DPC = _CUR_ISR = _CUR_TMR = _CUR_USR = HND_CPUUTIL_GARBAGE;

	TX_RESTORE // -------------------------------------------------------------

	/* Suspend CPUUTIL Idle thread, so default idle thread with wfi can run */
	if (hnd_cpuutil_g.runtime.thread_state == (uint8)HND_CPUUTIL_RUNTIME_ON) {
		if (osl_ext_task_suspend(get_cpuutil_thread()) != OSL_EXT_SUCCESS) {
			printf("%s: Failed to suspend Idle thread\n", __FUNCTION__);
			return;
		}
		hnd_cpuutil_g.runtime.thread_state = (uint8)HND_CPUUTIL_RUNTIME_OFF;
	}

	/* Disarm the periodic timer */
	if (hnd_cpuutil_g.runtime.timer_state == (uint8)HND_CPUUTIL_RUNTIME_ON) {
		if (hnd_timer_stop(hnd_cpuutil_g.timer) == FALSE) {
			printf("%s: Failed timer cancel\n", __FUNCTION__);
		}
		hnd_cpuutil_g.runtime.timer_state = (uint8)HND_CPUUTIL_RUNTIME_OFF;
	}

	ASSERT(hnd_cpuutil_g.runtime.state == (uint16)HND_CPUUTIL_RUNTIME_OFF);

	printf("CPU Util Tool Deactivated\n");

} /* hnd_cpuutil_deactivate */

static uint32
hnd_cpuutil_util(uint32 epoch, uint32 idle_period, uint32 util_period)
{
	uint32 pct_period;
	if (HND_CPUUTIL_IS_CCNT()) {
		pct_period = util_period / 100; // for % calc, use bcm_math64.h ?
		if (pct_period == 0U) pct_period = 1; // div-by-zero paranoia

		printf("CPU Util:\e[0;32m IDLE %u \e[0m epoch #%u %u usecs\n",
			idle_period / pct_period, epoch,
			util_period / HND_CPUUTIL_CYCLES_PER_USEC);
	} else {
		pct_period = ~0U;
		printf("CPU Util:\e[0;32m IDLE %u \e[0m epoch #%u %u eventid %u\n",
			idle_period, epoch, util_period, HND_CPUUTIL_EVENTID);
	}

	return pct_period;

} /* hnd_cpuutil_util */

static void /* CPU Util Tool "cons cu_dbg" verbose reporting function */
hnd_cpuutil_verbose(void *arg, int argc, char *argv[])
{
	uint32 i;
	uint32 epoch;
	uint32 events[HND_CPUUTIL_CONTEXT_TYPE_MAX];
	uint32 util_time[HND_CPUUTIL_CONTEXTS_MAX];
	uint32 idle_period, util_delays, util_period;
	uint32 pct_period;

	TX_INTERRUPT_SAVE_AREA

	if (hnd_cpuutil_g.runtime.state != (uint16)HND_CPUUTIL_RUNTIME_ON) {
		printf("%s: CPU Util not enabled. use cu_bgn \n", __FUNCTION__);
		_hnd_cpuutil_dbg();
		return;
	}

	/* Copy out data */
	TX_DISABLE // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

	epoch = hnd_cpuutil_g.cur_epoch;
	for (i = 0; i < HND_CPUUTIL_CONTEXTS_MAX; i++) {
		util_time[i] = hnd_cpuutil_g.util_time[i];
	}
	for (i = 0; i < HND_CPUUTIL_CONTEXT_TYPE_MAX; i++) {
		events[i] = hnd_cpuutil_g.cur_events[i];
	}
	idle_period = hnd_cpuutil_g.idle_period;
	util_period = hnd_cpuutil_g.util_period;
	util_delays = hnd_cpuutil_g.util_delays;

	TX_RESTORE // -------------------------------------------------------------

	if (!HND_CPUUTIL_IS_CCNT()) {
		if (HND_CPUUTIL_CNTDWN(idle_period))
			idle_period = 0xFFFFFFFF - idle_period;
		if (HND_CPUUTIL_CNTDWN(util_period))
			util_period = 0xFFFFFFFF - util_period;
		for (i = 0; i < HND_CPUUTIL_CONTEXTS_MAX; i++) {
			if (HND_CPUUTIL_CNTDWN(util_time[i])) {
				util_time[i] = 0xFFFFFFFF - util_time[i];
			}
		}
	}

	pct_period = hnd_cpuutil_util(epoch, idle_period, util_period);

	printf("\tPCNT TYPE REF  %s    FUNCTION\n",
		HND_CPUUTIL_IS_CCNT() ? "CYCLECNT" : "ARMv7CNT");

	for (i = 0; i < HND_CPUUTIL_CONTEXTS_MAX; i++)
	{
		if (hnd_cpuutil_g.context[i].state == (uint8)HND_CPUUTIL_RUNTIME_ON)
		{
			if (hnd_cpuutil_g.idle_context == &hnd_cpuutil_g.context[i])
				printf("\e[0;32m"); // visually locate idle context in table

			if (hnd_cpuutil_g.context[i].refcnt > 1) {
				printf("\t%4u %s  %2u  %-10u [@%08x]\n",
					util_time[i] / pct_period,
					_hnd_cpuutil_context_s[hnd_cpuutil_g.context[i].type],
					hnd_cpuutil_g.context[i].refcnt, util_time[i],
					hnd_cpuutil_g.context[i].func);
			} else {
				printf("\t%4u %s      %-10u [@%08x]\n",
					util_time[i] / pct_period,
					_hnd_cpuutil_context_s[hnd_cpuutil_g.context[i].type],
					util_time[i], hnd_cpuutil_g.context[i].func);
			}
		}
	}

	printf("\n"); // XXX don't delete this
	printf("\tTYPE FSM_EVENTS\n");
	for (i = 0; i < HND_CPUUTIL_CONTEXT_TYPE_MAX; i++) {
		printf("\t%s  %-10u\n", _hnd_cpuutil_context_s[i], events[i]);
	}

	printf("\n"); // XXX don't delete this
	printf("\t\e[0;31mTotal delays %u usecs\e[0m\n", util_delays);

	if (hnd_cpuutil_g.errors)
		printf("\t\e[0;31m;40mErrors = %u\e[0m\n", hnd_cpuutil_g.errors);

} /* hnd_cpuutil_verbose */

static void /* CPU Util Tool "cons cu" reporting function */
hnd_cpuutil_print(void *arg, int argc, char *argv[])
{
	uint32 epoch, idle_period, util_period;

	TX_INTERRUPT_SAVE_AREA

	if (hnd_cpuutil_g.runtime.state != (uint16)HND_CPUUTIL_RUNTIME_ON) {
		printf("%s: CPU Util not enabled: use cu_bgn\n", __FUNCTION__);
		_hnd_cpuutil_dbg();
		return;
	}

	/* Copy out data */
	TX_DISABLE // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

	epoch       = hnd_cpuutil_g.cur_epoch;
	idle_period = hnd_cpuutil_g.idle_period;
	util_period = hnd_cpuutil_g.util_period;

	TX_RESTORE // -------------------------------------------------------------

	hnd_cpuutil_util(epoch, idle_period, util_period);

} /* hnd_cpuutil_print */

#if defined(__ARM_ARCH_7A__)
static void __attribute__ ((no_instrument_function))
__armv7_pmu_clear(void)
{
	uint32 u32;
	/* cp15_c9_c12_PMCR: b0 enable, b1 evt ctr reset, b6..10 reserved */
	asm volatile("mrc p15, 0, %0, c9, c12, 0" : "=r"(u32)); /* read pmcr */
	u32 |= (1 << 1); /* event counter reset */
	u32 &= 0xFFFFFC3F; /* zero preserve reserved bits: 6..10 */
	asm volatile("mcr p15, 0, %0, c9, c12, 0" : : "r"(u32));
}

static uint32 __attribute__ ((no_instrument_function))
__armv7_pmu_ctr0_rd(void)
{
	uint32 ctr0 = 0U;
	asm volatile("mcr p15, 0, %0, c9, c12, 5" : : "r"(ctr0)); // pmselr
	asm volatile("mrc p15, 0, %0, c9, c13, 2" : "=r"(ctr0));
	return ctr0;
}

static void /* CPU Util Tool "cons cu_cfg" verbose reporting function */
hnd_cpuutil_config(void *arg, int argc, char *argv[])
{
	uint32 u32;

	if (argc == 2) {
		hnd_cpuutil_g.pmu_eventid = (uint32)bcm_atoi(argv[1]);
	} else {
		printf("\n\t0:CYCCNT 1:I$FILL 3:D$FILL 4:D$ACC 6:LD 7:ST\n"
			"\t8:INST 9:EXC 13:BRIMM 14:BRRTN 15:UNAL 16:BRMIS\n"
			"\t17:CYCCNT 18:BR 19:MEMACC 20:I$ACC 21:D$WB\n");
		printf("\n\t25:BUSACC 27:ISPEC 29:BUSCYC 96:BUSRD 97:BUSWR\n"
			"\t134:IRQ 135:FIQ 192:EXTMEM 193:NON$EXTMEM\n"
			"\t194:PREFFILL 195:PREFDROP 201:WRSTALL\n\n");
	}

	if (HND_CPUUTIL_IS_CCNT()) {
		printf("HND CPU Util Tool counting cycles\n");
		hnd_cpuutil_g.now = get_arm_cyclecount;
		return;
	}

#if defined(BCM_BUZZZ) && !defined(BCM_BUZZZ_FUNC)
	if (BCM_BUZZZ_STATUS() == BCM_BUZZZ_ENABLED) {
		printf("ERROR: BUZZZ is enabled and using ARMv7 PMU counter.\n");
		return;
	}
#endif /* BCM_BUZZZ */

	hnd_cpuutil_g.now = __armv7_pmu_ctr0_rd;

	/* cp15_c9_c12_PMSELR: b0..7 ctr_select, 8..31 reserved */
	u32 = 0U; /* Select Counter#0 using PMSELR.ctr_select */
	asm volatile("mcr p15, 0, %0, c9, c12, 5" : : "r"(u32));

	/* cp15_c9_c13_PMXEVTYPER: b0..7 evt_type */
	u32 = hnd_cpuutil_g.pmu_eventid; /* Configure PMXEVTYPER.evt_type */
	asm volatile("mcr p15, 0, %0, c9, c13, 1" : : "r"(u32));

	/* cp15_c9_c12_PMCR: b0 enable, b1 evt ctr reset, b6..10 reserved */
	asm volatile("mrc p15, 0, %0, c9, c12, 0" : "=r"(u32)); /* read pmcr */
	u32 |= (1 << 0); /* set enable */
	u32 |= (1 << 1); /* event counter reset */
	u32 &= 0xFFFFFC3F; /* zero preserve reserved bits: 6..10 */
	asm volatile("mcr p15, 0, %0, c9, c12, 0" : : "r"(u32));

	/* cp15_c9_REG PMINTENCLR: b0 ctr0 */
	u32 = 1 << 0; /* Disable overflow interrupts for ctr0 */
	asm volatile("mcr p15, 0, %0, c9, c14, 2" : : "r"(u32));

	/* cp15_c9_REG PMCNTENSET: b0 ctr0 */
	u32 = 1 << 0; /* Enable counter 0 */
	asm volatile("mcr p15, 0, %0, c9, c12, 1" : : "r"(u32));

	printf("HND CPU Util Tool counting event %u 0x%08x\n",
		hnd_cpuutil_g.pmu_eventid, hnd_cpuutil_g.pmu_eventid);

} /* hnd_cpuutil_config */
#endif /* __ARM_ARCH_7A__ */

/**
 * Section: CPU Util Tool Initialization
 *
 * Constraints:
 * - Must be invoked only after hnd cons and timer library are initialized.
 * - Do not zero out "context_bmap" which may be used even before the tool is
 *   fully initialized.
 * - New Idle thread must run at a priority higher than the default idle thread
 *   that invokes wfi. see threadx_osl_ext.h,c.
 * - Initialization failure does not gracefully return system to original state.
 *
 * Initialization sub-components:
 * - Ascertain tool dimensioning
 * - Setup all accumulators: default, named, garbage
 * - Setup cur context types (could be done in activation stage)
 * - Create the idle cpuutil thread and suspend it immediately
 * - Allocate a timer, but do not arm it
 * - Register User Interface handler to "cons":
 *       "cu_bgn", "cu_end", "cu_dbg", "cu"
 */
int
BCMATTACHFN(hnd_cpuutil_init)(si_t *sih)
{
	uint32 i;
	hnd_cpuutil_context_t *garbage;

	/* CPU Util Tool implementation constrained to 5 default context types */
	ASSERT(HND_CPUUTIL_CONTEXT_TYPE_MAX == 5);

	/* --- Prepare the 5 default contexts --- */
	hnd_cpuutil_g.context[HND_CPUUTIL_CONTEXT_THR].func = (uint32)cpuutil_thr;
	hnd_cpuutil_g.context[HND_CPUUTIL_CONTEXT_ISR].func = (uint32)cpuutil_isr;
	hnd_cpuutil_g.context[HND_CPUUTIL_CONTEXT_TMR].func = (uint32)cpuutil_tmr;
	hnd_cpuutil_g.context[HND_CPUUTIL_CONTEXT_DPC].func = (uint32)cpuutil_dpc;
	hnd_cpuutil_g.context[HND_CPUUTIL_CONTEXT_USR].func = (uint32)cpuutil_usr;

	for (i = 0; i < HND_CPUUTIL_CONTEXT_TYPE_MAX; i++) {
		hnd_cpuutil_g.context[i].type   = i;
		hnd_cpuutil_g.context[i].state  = (uint8)HND_CPUUTIL_RUNTIME_ON;
		hnd_cpuutil_g.context[i].refcnt = 1;
	}

	/* --- Prepare garbage accumulator context --- */
	garbage = HND_CPUUTIL_GARBAGE;
	garbage->func   = 0U;
	garbage->type   = HND_CPUUTIL_CONTEXT_INVALID;
	garbage->state  = (uint8)HND_CPUUTIL_RUNTIME_ON;
	garbage->refcnt = 1;

	/* --- Map current contexts pointers to garbage accumulator context --- */
	_CUR_THR = _CUR_ISR = _CUR_TMR = _CUR_DPC = _CUR_USR = HND_CPUUTIL_GARBAGE;

	/* --- Initialize named allocatable (just the un-assigned) contexts --- */
	for (i = HND_CPUUTIL_CONTEXT_TYPE_MAX; i < HND_CPUUTIL_CONTEXTS_MAX; i++) {
		if (isset(&hnd_cpuutil_g.context_bmap, i)) { // is un-assigned yet?
			hnd_cpuutil_g.context[i].func   = 0U;
			hnd_cpuutil_g.context[i].type   = HND_CPUUTIL_CONTEXT_INVALID;
			hnd_cpuutil_g.context[i].state  = (uint8)HND_CPUUTIL_RUNTIME_OFF;
			hnd_cpuutil_g.context[i].refcnt = 0;
		}
	}

	/* --- Create and Suspend the Idle CPU Util thread --- */
	if (osl_ext_task_create("IDLE_cpuutil_thread",
		get_cpuutil_thread_stack(), sizeof(cpuutil_thread_stack_g),
		OSL_EXT_TASK_CPUUTIL_PRIORITY, cpuutil_thread_entry,
		(osl_ext_task_arg_t)sih, get_cpuutil_thread()) != OSL_EXT_SUCCESS) {
		printf("%s: Failed creating idle thread\n", __FUNCTION__);
		return BCME_NORESOURCE;
	}

	/* Suspend CPU Util Idle thread, so default idle thread with wfi can run */
	if (osl_ext_task_suspend(get_cpuutil_thread()) != OSL_EXT_SUCCESS) {
		printf("%s: Failed suspending idle thread\n", __FUNCTION__);
		return BCME_ERROR;
	}

	hnd_cpuutil_g.idle_context = /* TX_THREAD::cpuutil context user extension */
		(hnd_cpuutil_context_t *)(hnd_cpuutil_g.cpuutil_thread.cpuutil);

	/* --- Create the CPU Util periodic timer ... do not arm as yet --- */
	hnd_cpuutil_g.timer =
		hnd_timer_create((void *)sih, NULL, hnd_cpuutil_advance, NULL, NULL);

	if (hnd_cpuutil_g.timer == NULL) {
		printf("%s: Failed creating timer\n", __FUNCTION__);
		return BCME_NORESOURCE;
	}

	/* --- Register CPU Util User Interface --- */

	/* Setup "cons cu_end" handler (Deactivate CPU Util Tool) */
	if (!hnd_cons_add_cmd("cu_end", hnd_cpuutil_deactivate, (void*)sih)) {
		printf("%s: Failed adding cons cmd: cu_end\n", __FUNCTION__);
		return BCME_NORESOURCE;
	}

	/* Setup "cons cu_bgn" handler (Activate CPU Util Tool) */
	if (!hnd_cons_add_cmd("cu_bgn", hnd_cpuutil_activate, (void*)sih)) {
		printf("%s: Failed adding cons cmd: cu_bgn\n", __FUNCTION__);
		return BCME_NORESOURCE;
	}

	/* Setup "cons cu_dbg" handler (Verbose CPU Util Reporting) */
	if (!hnd_cons_add_cmd("cu_dbg", hnd_cpuutil_verbose, (void*)sih)) {
		printf("%s: Failed adding cons cmd: cu\n", __FUNCTION__);
		return BCME_NORESOURCE;
	}

	/* Setup "cons cu" handler (Summarized CPU Util Reporting) */
	if (!hnd_cons_add_cmd("cu", hnd_cpuutil_print, (void*)sih)) {
		printf("%s: Failed adding cons cmd: cu\n", __FUNCTION__);
		return BCME_NORESOURCE;
	}

#if defined(__ARM_ARCH_7A__)
	/* Setup "cons cu_cfg" handler (ARMv7 PMU Event selection) */
	if (!hnd_cons_add_cmd("cu_cfg", hnd_cpuutil_config, (void*)sih)) {
		printf("%s: Failed adding cons cmd: cu_cfg\n", __FUNCTION__);
		return BCME_NORESOURCE;
	}
#endif /* __ARM_ARCH_7A__ */

#if defined(HND_CPUUTIL_USR_DEBUG)
	/* Debug Testing of USR context */
	hnd_cpuutil_g.usr_debug = // allocate a cpuutil bin for usr profiling
		HND_CPUUTIL_CONTEXT_GET(HND_CPUUTIL_CONTEXT_USR,
		                        (uint32)hnd_cpuutil_advance);
#endif /* HND_CPUUTIL_USR_DEBUG */

	printf("CPU UTil Tool Initialized\n");

	return BCME_OK;

} /* BCMATTACHFN hnd_cpuutil_init */

#endif /* HND_CPUUTIL */

int
BCMATTACHFN(hnd_cpu_stats_init)(si_t *sih)
{
#ifdef BCMDBG_CPU
	cu_timer = hnd_timer_create((void *)sih, NULL, hnd_update_timer, NULL, NULL);
	if (cu_timer == NULL)
		return BCME_NORESOURCE;

#if defined(RTE_CONS) && !defined(BCM_BOOTLOADER)
	if (!hnd_cons_add_cmd("cu", hnd_print_cpuuse, (void *)sih))
		return BCME_NORESOURCE;
#endif /* RTE_CONS  && ! BCM_BOOTLOADER */
#endif /* BCMDBG_CPU */

	return BCME_OK;
}

/* ========================== CPU init ========================= */
/*
 * Initialize and background:
 *	hnd_cpu_init: Initialize the world.
 */

/*
 * c0 ticks per usec - used by hnd_delay() and is based on 80Mhz clock.
 * Values are refined after function hnd_cpu_init() has been called, and are recalculated
 * when the CPU frequency changes.
 */
uint32 c0counts_per_us = HT_CLOCK / 1000000;
uint32 c0counts_per_ms = HT_CLOCK / 1000;

#if defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7A__)
static void
hnd_cpu_clock_init(uint hz)
{
	c0counts_per_us = hz / 1000000;
}
#endif // endif

#if !defined(RTE_POLL) && defined(__ARM_ARCH_7A__)
#define MPCORE_GIC_DIST_OFF	0x1000
#define MPCORE_GIC_CPUIF_OFF	0x2000

#define GIC_DIST_CTRL	0x000
#define GIC_DIST_CTR	0x004
#define GIC_DIST_IGROUP	0x080
#define GIC_DIST_ENABLE_SET	0x100
#define GIC_DIST_ENABLE_CLEAR	0x180
#define GIC_DIST_PRI	0x400
#define GIC_DIST_TARGET 0x800
#define GIC_DIST_CONFIG 0xc00

#define GIC_CPU_CTRL	0x0
#define GIC_CPU_PRIMASK 0x4

static void gic_dist_init(si_t *sih, uint32 base)
{
	unsigned int max_irq, i, igroup;
	uint32 disabled_irq = 0, cpumask = 1 << 0;	/* single-cpu for now */

	cpumask |= cpumask << 8;
	cpumask |= cpumask << 16;

	*(volatile uint32 *)(base + GIC_DIST_CTRL) = 0;

	/*
	 * Find out how many interrupts are supported.
	 */
	max_irq = *(volatile uint32 *)(base + GIC_DIST_CTR) & 0x1f;
	max_irq = (max_irq + 1) * 32;
	printf("gic_dist_init max_irq %u\n", max_irq);

	/* Secure Physical Timer event (PPI1)
	 * This is the event generated from the Secure Physical Timer and uses
	 * ID29. The interrupt is level-sensitive
	 */
	/* Set all interrupts to Group1 except PPI1 (ID29), (Group1 always signal IRQ) */
	for (i = 0; i < max_irq; i += 32) {
		if (!i)
			igroup = ~(1 << 29); /* except PPI1 (ID29) */
		else
			igroup = 0xffffffff;

		*(volatile uint32 *)(base + GIC_DIST_IGROUP + i * 4 / 32) = igroup;
	}

	/*
	 * Set all global interrupts to be level triggered, active low.
	 */
	for (i = 32; i < max_irq; i += 16)
		*(volatile uint32 *)(base + GIC_DIST_CONFIG + i * 4 / 16) = 0;

	/*
	 * Set all global interrupts to this CPU only.
	 */
	for (i = 32; i < max_irq; i += 4)
		*(volatile uint32 *)(base + GIC_DIST_TARGET + i * 4 / 4) = cpumask;

	/*
	 * Set priority on all interrupts.
	 */
	for (i = 0; i < max_irq; i += 4)
		*(volatile uint32 *)(base + GIC_DIST_PRI + i * 4 / 4) = 0xa0a0a0a0;

	/*
	 * Enable all interrupts but the error irqs.
	 */
	for (i = 0; i < max_irq; i += 32) {
		*(volatile uint32 *)(base + GIC_DIST_ENABLE_SET + i * 4 / 32) = 0xffffffff;
	}

	switch (CHIPID(sih->chip)) {
	CASE_BCM4365_CHIP:
		/* Disable vasip2mac interrupt in bit-10, as we don't want to process with ARM. */
		disabled_irq = BCM_BIT(10);
		break;
	CASE_BCM43684_CHIP:
	CASE_BCM6715_CHIP:
		/* Disable vasip2mac interrupt in bit-9, as we don't want to process with ARM. */
		disabled_irq = BCM_BIT(9);
		break;
	default:
		break;
	}

	if (disabled_irq) {
		*(volatile uint32 *)(base + GIC_DIST_ENABLE_CLEAR + 4) = disabled_irq;
	}

	/* Enable both Group0 and Group1 */
	*(volatile uint32 *)(base + GIC_DIST_CTRL) = 0x3;
}

static void gic_cpu_init(uint32 base)
{
	*(volatile uint32 *)(base + GIC_CPU_PRIMASK) = 0xf0;

	/* Enable both Group0 and Group1 and FIQ on Group0 */
	*(volatile uint32 *)(base + GIC_CPU_CTRL) = 0xb;
}

static void armca7_gic_init(si_t *sih)
{
	uint32 periphbase;

	asm volatile("mrc p15,4,%0,c15,c0,0 @ Read Configuration Base Address Register"
		: "=&r" (periphbase) : : "cc");
	/* Init GIC interrupt distributor */
	gic_dist_init(sih, periphbase + MPCORE_GIC_DIST_OFF);
	/* Initialize the GIC CPU interface for the boot processor */
	gic_cpu_init(periphbase + MPCORE_GIC_CPUIF_OFF);
}
#endif /* !RTE_POLL && __ARM_ARCH_7A__ */

#if defined(RTE_CACHED) && defined(__ARM_ARCH_7A__)
/* Translation Table Base Address must be aligned to 16KB, and can't be reclaimed
 * after inited. TLB would go back to look up these tables when cache missed
 */
static uint8 loader_pagetable_array[PAGETABLE_SIZE] __attribute__ ((aligned(PAGETABLE_SIZE)));
static uint8 l2_pagetable_array[L2_PAGETABLE_ARRAY_SIZE] __attribute__ ((aligned(KB)));

uint8 *hnd_mmu_l1_page_table;
uint8 *hnd_mmu_l2_page_table;

/* hold pointers to l2 page tables for easy access */
static uint page_tbl_ptr_idx = 0;
hnd_mmu_page_tbl_t hnd_mmu_page_tbl_ptrs[L2_NUM_PAGE_TABLES];

/* Allocate a new MMU level-2 page table.
 *
 * Inputs: mb  - Allocate page table for this MB.
 *
 * Return: Page table state.
 */
hnd_mmu_page_tbl_t*
BCMATTACHFN(hnd_mmu_alloc_l2_page_tbl_entry)(uint32 mb)
{
	hnd_mmu_page_tbl_t *page_tbl;

	page_tbl = hnd_mmu_get_l2_page_tbl_entry(mb);
	if (page_tbl == NULL) {
		/* Allocate a new L2 page table. */
		if (page_tbl_ptr_idx >= ARRAYSIZE(hnd_mmu_page_tbl_ptrs)) {
			OSL_SYS_HALT();
		}
		page_tbl       = &hnd_mmu_page_tbl_ptrs[page_tbl_ptr_idx];
		page_tbl->mb   = mb;
		page_tbl->addr = (uint32)&hnd_mmu_l2_page_table[page_tbl_ptr_idx * KB];
		page_tbl_ptr_idx++;
	}

	return (page_tbl);
}

static void
BCMATTACHFN(ca7_caches_on)(si_t *sih)
{
	uint32 *ptb;
	uint32 l2_ptbaddr, *l2_ptb;
	uint32 val;
	uint32 mb;		/**< megabyte counter */
	uint32 page;		/**< one 4KB page */
	uint32 page_4ks = 32;
	uint32 addr, size;

	cpu_inv_cache_all();

	/* Enable I$ */
	asm volatile("mrc p15, 0, %0, c1, c0, 0	@ get CR" : "=r" (val) : : "cc"); // SCTLR
	printf("Enabling caches.... SCTLR=%08x\n", val);
	val |= CR_I;
	dsb();
	asm volatile("mcr p15, 0, %0, c1, c0, 0	@ set CR" : : "r" (val) : "cc"); // SCTLR
	isb();

	hnd_mmu_l1_page_table = loader_pagetable_array;
	hnd_mmu_l2_page_table = l2_pagetable_array;
	MMU_MSG(("%s: hnd_mmu_l1_page_table %p hnd_mmu_l2_page_table %p\n",
		__FUNCTION__, hnd_mmu_l1_page_table, hnd_mmu_l2_page_table));

	ptb = (uint32 *)hnd_mmu_l1_page_table; // the single level 1 page table 'master page table'

	if (BCM4365_CHIP(sih->chip)) {
		/* l2 page table, used to describe mem regions with a finer granularity than 1MB */
		l2_ptbaddr = (uint32)hnd_mmu_l2_page_table;

		/* cur_addr is now 0x0000_0000. ROM/boot-flops start here. */
		ptb[0] = L1_ENTRY_PAGE_TABLE(l2_ptbaddr, 0);
		l2_ptb = (uint32 *)l2_ptbaddr;
		for (page = 0; page < 192; page++) {
			// Each 32-bit entry provides translation information for 4KB of memory.
			l2_ptb[page] = L2_ENTRY_NORMAL_MEM(page);
		}

		/* cur_addr is now 0x00CD_0000. Gap between ROM and RAM starts here. */
		for (; page < L2_PAGETABLE_ENTRIES_NUM; page++) {
			l2_ptb[page] = L2_ENTRY_NOTHING(page);
		}

		/* cur_addr is now 0x0010_0000 */
		for (mb = 1; mb < 2; mb++) {
			ptb[mb] = L1_ENTRY_NOTHING(mb);
		}

		/* cur_addr is now 0x0020_0000 */
		/* RAM (sysmem) starts here (at the '2 Mbytes' address marker). It is 2688KB large.
		 * The first 2304KB of sysmem is used by the ARM, the rest of sysmem by the MAC. The
		 * ARM should not prefetch/modify the memory used by the MAC.
		 */
		for (mb = 2; mb < 4; mb++) {
			ptb[mb] = L1_ENTRY_NORMAL_MEM(mb);
		}

		/* cur_addr is now 0x0040_0000 */
		/* The next 256KB is still for the ARM. The rest is for the MAC core. */
		/* The boundary MB is pointed to the 2nd level table */
		l2_ptbaddr += KB;
		ptb[4] = L1_ENTRY_PAGE_TABLE(l2_ptbaddr, 0);
		l2_ptb = (uint32 *)l2_ptbaddr;
		/* Special sysmem configuration between 2M ~ 3M region */
#ifdef RAMSIZE
		ASSERT(RAMSIZE <= 3 * MB);
		if (RAMSIZE > (2 * MB)) {
			page_4ks = (RAMSIZE - (2 * MB)) >> 12;
		}
#endif // endif
		for (page = 0; page < page_4ks; page++) {
			l2_ptb[page] = (0x400000 | L2_ENTRY_NORMAL_MEM(page));
		}

		/* We should now be at address 0x0044_0000 ('Adjust dongle ram size to 0x240000').
		 * Gap between RAM and io space starts here.
		 */
		for (; page < L2_PAGETABLE_ENTRIES_NUM; page++) {
			l2_ptb[page] = L2_ENTRY_NOTHING(page);
		}

		/* cur_addr is now 0x0030_0000 */
		for (mb = 5; mb < 0x18000000 / MB; mb++) {
			ptb[mb] = L1_ENTRY_NOTHING(mb);
		}

		/* cur_addr is now 0x1800_0000 */
		for (; mb < 0x19400000 / MB; mb++) {
			ptb[mb] = L1_ENTRY_DEVICE_MEM(mb);
		}

		/* cur_addr is now 0x1940_0000 */
		for (; mb < 0x20000000 / MB; mb++) {
			ptb[mb] = L1_ENTRY_NOTHING(mb);
		}

		/* cur_addr is now 0x2000_0000. Start of the 128MB PCIe small address window, used
		 * by the dongle to access host memory.
		 */
		for (; mb < 0x28000000 / MB; mb++) {
			ptb[mb] = L1_ENTRY_DEVICE_MEM(mb);
		}

		/* cur_addr is now 0x2800_0000 */
		for (; mb < L1_PAGETABLE_ENTRIES_NUM; mb++) {
			ptb[mb] = L1_ENTRY_NOTHING(mb);
		}

		/* cur_addr is now 1_0000_0000 */
	}
	else if (BCM43684_CHIP(sih->chip)) {
		/* Total RAM size : 5767168B = 5632KB = 5.5MB */
		/* Map the first 5MB as totally available for A7 */
		for (mb = 0; mb < 5; mb++) {
			addr = (mb * MB);
			size = MB;
			HND_MMU_ALLOC_L2_PAGE_ATTRIBUTES(addr, addr, size,
				L2S_PAGE_ATTR_NORMAL_MEM, FALSE);
		}

		/* Remaining 512K should be mapped using L2 page table
		 * cur addr now : 0x50_0000
		 */
		page_4ks = 128;		/* Default value when RAMSIZE !defined */
#ifdef RAMSIZE
		ASSERT(RAMSIZE <= 6 * MB);
		if (RAMSIZE > (5 * MB)) {
			page_4ks = (RAMSIZE - (5 * MB)) >> 12;
		}
#endif // endif
		addr = (mb * MB);
		size = page_4ks * PAGE_SIZE;
		HND_MMU_ALLOC_L2_PAGE_ATTRIBUTES(addr, addr, size,
			L2S_PAGE_ATTR_NORMAL_MEM, FALSE);

		addr += size;
		size = MB - size;
		HND_MMU_ALLOC_L2_PAGE_ATTRIBUTES(addr, addr, size,
			L2S_PAGE_ATTR_NO_ACC, FALSE);

		for (mb = 6; mb < 0x20000000 / MB; mb++) {
			ptb[mb] = L1_ENTRY_NOTHING(mb);
		}
#ifdef SW_PAGING
		/* Virtual memory starts after physical memory.
		 * Mark each page as no access to start with.
		 * Virtual pages will be remapped to physical page on page fault
		 */
		HND_MMU_ALLOC_L2_PAGE_ATTRIBUTES(SW_PAGING_CODE_VIRTUAL_ADDR_BASE,
			SW_PAGING_CODE_VIRTUAL_ADDR_BASE, SW_PAGING_CODE_VIRTUAL_ADDR_SIZE,
			L2S_PAGE_ATTR_NO_ACC, FALSE);
#endif // endif

		/* Device memory area:
		 * 0x2000_0000 -- 0x27ff_ffff: PCIe small address window.
		 * 0x2800_0000 -- 0x281f_ffff: core regs, wrappers, radio dig, coresight.
		 * 0x2820_0000 -- 0x285f_ffff: NIC400, CCI400, GIC, PHYDSP, HWA.
		 * 0x2860_0000 -- 0x287f_ffff: Undefined.
		 * 0x2880_0000 -- 0x28ff_ffff: DOT11MAC region (8MB).
		 * 0x2900_0000 -- 0xffff_ffff: Undefined.
		 */
		for (; mb < 0x28600000 / MB; mb++) {
			ptb[mb] = L1_ENTRY_DEVICE_MEM(mb);
		}

		for (; mb < 0x28800000 / MB; mb++) {
			ptb[mb] = L1_ENTRY_NOTHING(mb);
		}

		for (; mb < 0x29000000 / MB; mb++) {
			ptb[mb] = L1_ENTRY_DEVICE_MEM(mb); /* DOT11MAC region (8MB) */
		}

		for (; mb < L1_PAGETABLE_ENTRIES_NUM; mb++) {
			ptb[mb] = L1_ENTRY_NOTHING(mb);
		}
	} else if (BCM6715_CHIP(sih->chip)) {
		/* Total RAM size : 3670016B = 3584KB = 3.5MB */
		/* Map the first 3MB as totally available for A7 */
		for (mb = 0; mb < 3; mb++) {
			addr = (mb * MB);
			size = MB;
			HND_MMU_ALLOC_L2_PAGE_ATTRIBUTES(addr, addr, size,
				L2S_PAGE_ATTR_NORMAL_MEM, FALSE);
		}

		/* Remaining 512K should be mapped using L2 page table
		 * cur addr now : 0x30_0000
		 */
		/* Configure the 1st 512K in the 3M - 4M region as accessible to CA7
		 * Each L2 table entry maps 4K bytes. So estimate number of L2 entries
		 */
		page_4ks = 128;		/* Default value when RAMSIZE !defined */
#ifdef RAMSIZE
		ASSERT(RAMSIZE <= 4 * MB);
		if (RAMSIZE > (3 * MB)) {
			page_4ks = (RAMSIZE - (3 * MB)) >> 12;
		}
#endif // endif
		addr = (mb * MB);
		size = page_4ks * PAGE_SIZE;
		HND_MMU_ALLOC_L2_PAGE_ATTRIBUTES(addr, addr, size,
			L2S_PAGE_ATTR_NORMAL_MEM, FALSE);
		addr += size;
		size = MB - size;
		HND_MMU_ALLOC_L2_PAGE_ATTRIBUTES(addr, addr, size,
			L2S_PAGE_ATTR_NO_ACC, FALSE);

		for (mb = 4; mb < 0x20000000 / MB; mb++) {
			ptb[mb] = L1_ENTRY_NOTHING(mb);
		}
#ifdef SW_PAGING
		/* Virtual memory starts after physical memory.
		 * Mark each page as no access to start with.
		 * Virtual pages will be remapped to physical page on page fault
		 */
		HND_MMU_ALLOC_L2_PAGE_ATTRIBUTES(SW_PAGING_CODE_VIRTUAL_ADDR_BASE,
			SW_PAGING_CODE_VIRTUAL_ADDR_BASE, SW_PAGING_CODE_VIRTUAL_ADDR_SIZE,
			L2S_PAGE_ATTR_NO_ACC, FALSE);
#endif /* SW_PAGING */

		/* Device memory area:
		 * 0x2000_0000 -- 0x27ff_ffff: PCIe small address window.
		 * 0x2800_0000 -- 0x281f_ffff: core regs, wrappers, radio dig, coresight.
		 * 0x2820_0000 -- 0x285f_ffff: NIC400, CCI400, GIC, PHYDSP, HWA.
		 * 0x2860_0000 -- 0x286f_ffff: CCI500.
		 * 0x2870_0000 -- 0x287f_ffff: Undefined.
		 * 0x2880_0000 -- 0x28ff_ffff: DOT11MAC region (8MB).
		 * 0x2900_0000 -- 0x2fff_ffff: Undefined.
		 * 0x3000_0000 -- 0x3fff_ffff: PCIE small address window 3 for RAC.
		 * 0x4000_0000 -- 0xffff_ffff: Undefined.
		 */
		for (; mb < 0x28700000 / MB; mb++) {
			ptb[mb] = L1_ENTRY_DEVICE_MEM(mb);
		}

		for (; mb < 0x28800000 / MB; mb++) {
			ptb[mb] = L1_ENTRY_NOTHING(mb);
		}

		for (; mb < 0x29000000 / MB; mb++) {
			ptb[mb] = L1_ENTRY_DEVICE_MEM(mb); /* DOT11MAC region (8MB) */
		}

		for (; mb < 0x30000000 / MB; mb++) {
			ptb[mb] = L1_ENTRY_NOTHING(mb);
		}

		for (; mb < 0x40000000 / MB; mb++) {
			ptb[mb] = L1_ENTRY_DEVICE_MEM(mb); /* PCIE small address window 3 for RAC */
		}

		for (; mb < L1_PAGETABLE_ENTRIES_NUM; mb++) {
			ptb[mb] = L1_ENTRY_NOTHING(mb);
		}
	}

	dsb(); // Synchronize mmu tables updates with main memory

	asm volatile("mcr p15, 0, %0, c2, c0, 2" : : "r" (0)); // TTBCR

	/* Apply page table address to CP15 */
	asm volatile("mcr p15, 0, %0, c2, c0, 0" : : "r" (ptb) : "memory"); //TTBR0

	/* Set access control to be determined by TLB entry for only domain zero. */
	/* All page entries are to be assumed domain zero. */
	asm volatile("mcr p15, 0, %0, c3, c0, 0" : : "r" (0b01)); // DACR

	/* Enabling MMU and D$ */
	asm volatile("mrc p15, 0, %0, c1, c0, 0 @ get CR" : "=r" (val) : : "cc"); // SCTLR
	val |= CR_C|CR_M;
	dsb();
	asm volatile("mcr p15, 0, %0, c1, c0, 0	@ set CR" : : "r" (val) : "cc");
	isb();

	/* Invalidate ITLB after update of TTBR0 (enabled by MMU) */
	asm volatile("mcr p15, 0, %0, c8, c7, 0" : : "r" (0)); // TLBIALL
	dsb(); // TLB operations require dsb and isb to complete

	/* Invalidate DTLB after update of TTBR0 (enabled by MMU) */
	asm volatile("mcr p15, 0, %0, c8, c6, 0" : : "r" (0)); // DTLBIALL
	dsb(); // TLB operations require dsb and isb to complete

	/* Branch Predictor Invalidate All. This is required AFTER enabling or diabling MMU,
	 * see TRM, chapter B2.2.6 Branch predictors. The invalidate requires an isb to complete
	 */
	asm volatile("mcr p15, 0, %0, c7, c5, 6" : : "r" (0)); // BPIALL
	dsb(); // Ensure completion of the invalidations
	isb(); // Synchronize fetched instruction stream
} /* ca7_caches_on */

#if defined(__ARM_ARCH_7A__) && defined(RTE_CACHED) && !defined(BCM_BOOTLOADER)
/* Update the existing MMU table. Protect data area with "Do Not eXecute" to protect against
 * buffer overflows which are being misused for hacking purpose. This function should be called
 * after all initialisation has finished and all memory has been reclaimed. Once reclaim is done
 * all heap and stack can be marked as non-executable.
 * Testing:
 * Data abort: add this line to end of this function: "*((uint32 *)(0x20)) = 0xBACF5174;"
 * Instruction abort: change "(uint32)text_end" into ((uint32)text_end - 0x10000)" on 2 ! places.
 */
void
ca7_execute_protect_on(si_t *sih)
{
	uint32 split;
	uint32 addr, size;

	if (!(BCM43684_CHIP(sih->chip) || BCM6715_CHIP(sih->chip))) {
		return;
	}

	MMU_PRT(("Turn memory protection ON\n"));

	split = (uint32)text_end & ~(PAGE_SIZE - 1);
	addr = 0;
	size = split;
	/* Now make the code section in RAM RO (without XN) */
	MMU_MSG(("%s: setting 0x%08x (size 0x%08x) as EXEC_NOWR\n", __FUNCTION__,
		addr, size));
	HND_MMU_SET_L2_PAGE_ATTRIBUTES(addr, addr, size, L2S_PAGE_ATTR_EXEC_NOWR, FALSE);
	/* Make the split page as normal memory */
	addr = split;
	size = PAGE_SIZE;
	MMU_MSG(("%s: setting 0x%08x (size 0x%08x) as NORMAL_MEM\n", __FUNCTION__,
		addr, size));
	HND_MMU_SET_L2_PAGE_ATTRIBUTES(addr, addr, size, L2S_PAGE_ATTR_NORMAL_MEM, FALSE);
	/* Mark RW data section as XN */
	addr = split + PAGE_SIZE;
	size = RAMSIZE - addr;
	MMU_MSG(("%s: setting 0x%08x (size 0x%08x) as NOEXEC_WR_MEM\n", __FUNCTION__,
		addr, size));
	HND_MMU_SET_L2_PAGE_ATTRIBUTES(addr, addr, size, L2S_PAGE_ATTR_NOEXEC_WR_MEM, FALSE);
#ifdef SW_PAGING
	/* Mark reserved pages as not accessible */
	addr = (uint32)&paging_reserved_base;
	size = (uint32)&paging_reserved_end - addr;
	if (size) {
		MMU_MSG(("%s: setting 0x%08x (size 0x%08x) as NO_ACC\n", __FUNCTION__,
			addr, size));
		HND_MMU_SET_L2_PAGE_ATTRIBUTES(addr, addr, size, L2S_PAGE_ATTR_NO_ACC, FALSE);
	}
#endif /* SW_PAGING */

	HND_MMU_FLUSH_TLB();
}
#endif /* __ARM_ARCH_7R__ && RTE_CACHED && !BCM_BOOTLOADER */

#ifdef BCM_DMA_TRANSCOHERENT
static void
BCMATTACHFN(ca7_coherent_on)(si_t *sih)
{
	uint32 val;

	hnd_hw_coherent_enable(sih);

#ifndef ATE_BUILD
	pcie_coherent_accenable(si_osh(sih), sih);

#endif // endif
	/* Enabling SMP bit */
	asm volatile("mrc p15, 0, %0, c1, c0, 1  @ get CR" : "=r" (val) : : "cc"); // ACTLR
	val |= CR_SMP;
	asm volatile("mcr p15, 0, %0, c1, c0, 1 @ set CR" : : "r" (val) : "cc"); // ACTLR
	isb();
}
#endif /* BCM_DMA_TRANSCOHERENT */
#endif /* RTE_CACHED && __ARM_ARCH_7A__ */

#ifdef RTE_CACHED
void
BCMATTACHFN(hnd_caches_init)(si_t *sih)
{
#ifdef __ARM_ARCH_7A__
#ifdef BCM_DMA_TRANSCOHERENT
	ca7_coherent_on(sih);
#endif /* BCM_DMA_TRANSCOHERENT */

#ifdef SW_PAGING
	sw_paging_init();
#endif // endif
	/* Enable caches and MMU */
	ca7_caches_on(sih);
#endif /* __ARM_ARCH_7A__ */
}
#endif /* RTE_CACHED */

/** ca-7a specific function */
void
BCMATTACHFN(hnd_cpu_init)(si_t *sih)
{
	si_arm_init(sih);

#ifdef EXT_CBALL
	{
	uint32 *v = (uint32 *)0;
	extern char __traps[], _mainCRTStartup[];

	/*
	 * Write new exception vectors.
	 * EXT_CBALL currently does not link with 'startup' at address 0.
	 */

	v[ 0] = 0xea000000 | ((uint32)_mainCRTStartup / 4 - 2);	/* 0000: b <reset> */
	v[ 1] = 0xe59ff014;				/* 0004: ldr pc, [pc, #20] */
	v[ 2] = 0xe59ff014;				/* 0008: ldr pc, [pc, #20] */
	v[ 3] = 0xe59ff014;				/* 000c: ldr pc, [pc, #20] */
	v[ 4] = 0xe59ff014;				/* 0010: ldr pc, [pc, #20] */
	v[ 5] = 0xe59ff014;				/* 0014: ldr pc, [pc, #20] */
	v[ 6] = 0xe59ff014;				/* 0018: ldr pc, [pc, #20] */
	v[ 7] = 0xe59ff014;				/* 001c: ldr pc, [pc, #20] */
	v[ 8] = (uint32)__traps + 0x00;			/* 0020: =tr_und */
	v[ 9] = (uint32)__traps + 0x10;			/* 0024: =tr_swi */
	v[10] = (uint32)__traps + 0x20;			/* 0028: =tr_iab */
	v[11] = (uint32)__traps + 0x30;			/* 002c: =tr_dab */
	v[12] = (uint32)__traps + 0x40;			/* 0030: =tr_bad */
	v[13] = (uint32)__traps + 0x50;			/* 0034: =tr_irq */
	v[14] = (uint32)__traps + 0x60;			/* 0038: =tr_fiq */
	}
#endif /* EXT_CBALL */

	/* Initialize timers */
	c0counts_per_ms = (si_cpu_clock(sih) + 999) / 1000;
	c0counts_per_us = (si_cpu_clock(sih) + 999999) / 1000000;

#if !defined(RTE_POLL) && defined(__ARM_ARCH_7A__)
	armca7_gic_init(sih);
#endif /* !RTE_POLL && __ARM_ARCH_7A__ */

	/* Use PMU as a timer source */
	hnd_pmu_init(sih);

#ifndef BCM_BOOTLOADER
	hnd_cpu_stats_init(sih);
#endif /* BCM_BOOTLOADER */
} /* hnd_cpu_init */

/**
 * Set or query CPU clock frequency
 * parameters:
 *	div: 0 to query, 1 or 2 to set.
 * return value:
 *	if div == 0:
 *		1 for standard frequency, 2 for double frequency, -1 for not capable of switching
 *	if div == 1 or div == 2:
 *		0 for no switch occurred, 1 for double->std switch, 2 for std->double switch
 */
int32
hnd_cpu_clockratio(si_t *sih, uint8 div)
{
#ifdef CPU_CLK_SWITCHING
	int32 ret;

	ret = si_arm_clockratio(sih, div);

	if (div != 0) {
		/* global vars 'c0counts..' are changed here for hnd_delay() / OSL_DELAY() */
		switch (ret) {
		case 1:
			c0counts_per_ms /= 2; /* because CPU clock frequency dropped */
			c0counts_per_us /= 2;
			break;
		case 2:
			c0counts_per_ms *= 2;
			c0counts_per_us *= 2;
			break;
		}
	}
	return ret;
#else /* CPU_CLK_SWITCHING */
	return 0;
#endif /* CPU_CLK_SWITCHING */
}

/* ========================== time ========================== */

/*
 * Timing support:
 *	hnd_delay(us): Delay us microseconds.
 */

static uint32 lastcount = 0;

/** updates several time related global variables and returns current time in [ms] */
uint32
hnd_update_now(void)
{
	uint32 diff, ticks, ms;

	ticks = hnd_pmu_get_tick();

	/* The next line assumes that we update at least every 2**32 ticks */
	diff = ticks - lastcount;
	if (diff == 0)
		return 0;
	lastcount = ticks;

	ms = hnd_pmu_accu_tick(diff);

	return ms;
}

static uint32 lastcount_us = 0;

#ifndef USE_ARM_CYCLE_COUNT_FOR_US
uint32
hnd_update_now_us(void)
{
	uint32 diff, ticks;
	uint32 us;

	ticks = hnd_pmu_get_tick();

	/* The next line assumes that we update at least every 2**32 ticks */
	diff = ticks - lastcount_us;
	if (diff == 0)
		return 0;
	lastcount_us = ticks;

	us = hnd_pmu_accu_tick_us(diff);
	return us;
}
#else
uint32
hnd_update_now_us(void)
{
	uint32 count, diff;

	count = get_arm_cyclecount();

	diff = count - lastcount_us;
	if (diff >= c0counts_per_us) {
		lastcount_us += (diff / c0counts_per_us) * c0counts_per_us;
		return diff / c0counts_per_us;
	}

	return 0;
}
#endif /* USE_ARM_CYCLE_COUNT_FOR_US */

static void
hnd_delay_ex(uint32 us)
{
	uint32 curr, c0_wait;

	BUZZZ_LVL4(HND_DELAY, 2, (uint32)CALL_SITE, us);

	curr = get_arm_cyclecount();
	c0_wait = us * c0counts_per_us;

	while ((get_arm_cyclecount() - curr) < c0_wait) {
		;	/* empty */
	}
}

void
hnd_delay(uint32 us)
{
	OSL_INTERRUPT_SAVE_AREA

	OSL_DISABLE
#if defined(HND_CPUUTIL)
	hnd_cpuutil_g.cur_delays += us;
#endif /* HND_CPUUTIL */
	hnd_delay_ex(us);
	OSL_RESTORE
}

/* ========================== timer ========================== */

/**
 * The RTOS maintains a linked list of software timers, and requires a hardware timer to generate an
 * interrupt on the software timer that expires first. This function sets that hardware timer.
 */
void
hnd_set_irq_timer(uint us)
{
	uint32 ticks = hnd_pmu_us2tick(us);

	hnd_pmu_set_timer(ticks);
}

void
hnd_ack_irq_timer(void)
{
	hnd_pmu_ack_timer();
}

/* ======================= system ====================== */

void
hnd_wait_irq(si_t *sih)
{
	hnd_cpu_wait(sih);
}

void
hnd_enable_interrupts(void)
{
	enable_arm_irq();
#if defined(FIQMODE) && (defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7A__)) && \
	!defined(BCM_BOOTLOADER)
	enable_arm_fiq();
#endif /* FIQMODE && (__ARM_ARCH_7R__ || __ARM_ARCH_7A__) && !BCM_BOOTLOADER */
}

void
hnd_disable_interrupts(void)
{
	disable_arm_irq();
#if defined(FIQMODE) && (defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7A__)) && \
	!defined(BCM_BOOTLOADER)
	disable_arm_fiq();
#endif /* FIQMODE && (__ARM_ARCH_7R__ || __ARM_ARCH_7A__) && !BCM_BOOTLOADER */
}

/* ======================= dma ====================== */

void *
hnd_dma_alloc_consistent(uint size, uint16 align_bits, uint *alloced, void *pap)
{
	void *buf;

	/* align on a OSL defined boundary */
#if defined(BCMDBG_MEMFAIL)
	if (!(buf = hnd_malloc_align(size, align_bits, CALL_SITE)))
#else
	if (!(buf = hnd_malloc_align(size, align_bits)))
#endif // endif
		return NULL;
	ASSERT(ISALIGNED(buf, (1 << align_bits)));
	*alloced = size;

#ifdef CONFIG_XIP
	/*
	 * arm bootloader memory is remapped but backplane addressing is
	 * 0-based
	 *
	 * Background: since the mask rom bootloader code executes in a
	 * read-only memory space apart from SoC RAM, data addresses
	 * specified by bootloader code must be decoded differently from
	 * text addresses. Consequently, the processor must run in a
	 * "memory remap" mode whereby data addresses containing the
	 * MEMORY_REMAP bit are decoded as residing in SoC RAM. However,
	 * backplane agents, e.g., the dma engines, always use 0-based
	 * addresses for SoC RAM, regardless of processor mode.
	 * Consequently it is necessary to strip the MEMORY_REMAP bit
	 * from addresses programmed into backplane agents.
	 */
	*(ulong *)pap = (ulong)buf & ~MEMORY_REMAP;
#else
	*(ulong *)pap = (ulong)buf;
#endif /* CONFIG_XIP */

	return (buf);
}

void
hnd_dma_free_consistent(void *va)
{
	hnd_free(va);
}

/* ======================= debug ===================== */

/*
 * Use a debug comparator reg to cause a trap on stack underflow.
 */

/* Stack protection initialization */
#ifndef BCMDBG_STACKP_LG2SIZE
#define BCMDBG_STACKP_LG2SIZE 5
#endif // endif

void
BCMATTACHFN(hnd_stack_prot)(void *stack_top)
{
#if defined(__ARM_ARCH_7M__)
#if defined(STACK_PROT_TRAP) && defined(BCMDBG_TRAP) && !defined(_RTE_SIM_)
	/* Point at an STACKP_SIZE-aligned and sized area at the end */
	uint32 st = (((uint32) stack_top) + (1 << BCMDBG_STACKP_LG2SIZE) - 1) &
		~((1 << BCMDBG_STACKP_LG2SIZE) - 1);
	wreg32(CM3_DWT_MASK1, BCMDBG_STACKP_LG2SIZE);
	wreg32(CM3_DWT_COMP1, st);
	(void)rreg32(CM3_DWT_FUNCTION1);
	wreg32(CM3_DWT_FUNCTION1, CM3_DWT_FUNCTION_WP_WRITE);
#endif /* STACK_PROT ... */
#endif /* ARCH_7M */
}

void
hnd_memtrace_enab(bool on)
{
#if defined(BCMDBG_TRAP) && !defined(_RTE_SIM_)
#if defined(__ARM_ARCH_7M__)
	uint32 val = rreg32(CM3_DBG_EMCR) | CM3_DBG_EMCR_MON_EN;

	if (on)
		val |= CM3_DBG_EMCR_TRCENA;
	else
		val &= ~CM3_DBG_EMCR_TRCENA;

	/* Enable/Disable DWT (data watchpoint and tracing) functionality and Monitor Mode. */
	wreg32(CM3_DBG_EMCR, val);
#endif /* __ARM_ARCH_7M__ */
#endif /* BCMDBG_TRAP && !_RTE_SIM_ */
}

#ifdef  __ARM_ARCH_7R__   /* cortex-R4 */
#ifdef  MPU_RAM_PROTECT_ENABLED
/*
* Enable additional protection on top of the assembly level protection
*/
extern char _ram_mpu_region_start[];
extern char _ram_mpu_region_end[];
void
mpu_protect_code_area(void)
{

	uint32	region_start;
	uint32	region_end;
#if defined(RAMBASE)
	uint32	rom_mpu_end = 0, ram_mpu_end = 0;
#endif /* RAMBASE */

	region_start = (uint32)_ram_mpu_region_start;
	region_end = (uint32)_ram_mpu_region_end;

	/* Make the lower ROM area write-protected */
	if (cr4_mpu_set_region(LOWER_RO_ROM_CODE_MPU_REGION, 0,
			RS_VAL_2MB, AP_VAL_110 | TEX_VAL_001 | C_BIT_ON | B_BIT_ON,
			SUBR_VAL_8 | SUBR_VAL_7 | SUBR_VAL_6) != BCME_OK) {
		return;
	}
#if defined(RAMBASE)
	/* Get end of ROM MPU and end of RAM MPU from the MPU settings in the assembly. */
	cr4_mpu_get_assembly_region_addresses(&rom_mpu_end, &ram_mpu_end);

	/* Re-adjust the MPU settings done in the assembly */
	if (rom_mpu_end == ROM_CODE_MPU_END_ASSEMBLY &&
		ram_mpu_end == RAM_CODE_MPU_END_ASSEMBLY) {
		if (MEMBASE == RAM_BASE_180000) {
			/* For those with RAMBASE=0x180000, disable subregion 6 from
			 * ROM_CODE_MPU_REGION.
			* So, this means read_only region is extended 0x180000. So, from
			* 0 - 0x180000 would be read_only
			*/
			if (cr4_mpu_set_region(LOWER_RO_ROM_CODE_MPU_REGION, 0,
					RS_VAL_2MB, AP_VAL_110 | TEX_VAL_001 | C_BIT_ON | B_BIT_ON,
					SUBR_VAL_8 | SUBR_VAL_7) != BCME_OK)
				return;
		} else if ((MEMBASE == RAM_BASE_170000) && (MAX_MPU_REGION > 8)) {
			/* For those with RAMBASE=0x170000, add a new region from 0x140000 to
			* 0x170000 with 2 subregions disabled.
			* 0 - 0x170000 would be read_only
			*/
			if (cr4_mpu_set_region(UPPEREND_RO_ROM_CODE_MPU_REGION, rom_mpu_end,
				RS_VAL_256KB, AP_VAL_110 | TEX_VAL_001 | C_BIT_ON | B_BIT_ON,
				SUBR_VAL_8 | SUBR_VAL_7) != BCME_OK)
				return;
		}
		if ((MEMBASE + RAMSIZE) == 0x240000) {
			/* For those with MEMBASE+RAMSIZE=0x240000, enable subregion 6 from
			* UPPER_RAM_CODE_MPU_REGION. So, this means read_write region is shrunk
			* to 0x280000. Above 0x280000 would be no access.
			*/
			if (cr4_mpu_set_region(UPPER_RW_RAM_CODE_MPU_REGION, 0,
					RS_VAL_4MB, AP_VAL_011 | TEX_VAL_001 | C_BIT_ON | B_BIT_ON,
					SUBR_VAL_8 | SUBR_VAL_7 | SUBR_VAL_6) != BCME_OK)
				return;
		} else if (((MEMBASE + RAMSIZE) == 0x2c0000) && (MAX_MPU_REGION > 8)) {
			/* For those with MEMBASE+RAMSIZE=0x2c0000, add a new region from
			* 0x2c0000 to 0x300000 with no access.
			*/
			if (cr4_mpu_set_region(UPPEREND_RW_RAM_CODE_MPU_REGION, 0x2c0000,
				RS_VAL_256KB, AP_VAL_000 | TEX_VAL_000 | S_BIT_ON | B_BIT_ON,
				0) != BCME_OK)
				return;
		}
	}
#endif /* RAMBASE */

	/* Now protect the CODE Area in RAM */
	mpu_protect_best_fit(LOWER_RO_RAM_CODE_MPU_REGION,
		UPPEREND_RO_ROM_CODE_MPU_REGION - 1, region_start, region_end);
}
#endif  /* MPU_RAM_PROTECT_ENABLED */
#endif /* __ARM_ARCH_7R__ */
