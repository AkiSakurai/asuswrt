/*
 * hndrte ARM port specific routines. Contains functions that are specific to both the ARM processor
 * *and* the RTE RTOS.
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
 * $Id: hndrte_arm.c 676784 2016-12-24 16:10:17Z $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <hndsoc.h>
#include <bcmdevs.h>
#include <siutils.h>
#include <hndrte.h>
#include <hndcpu.h>
#include <sbhndarm.h>
#include <sbchipc.h>
#include <hndchipc.h>
#include <bcmsdpcm.h>
#include <hndpmu.h>
#include <hndrte_debug.h>
#include <bcmpcie.h>
#include <bcm_buzzz.h> /* BCM_BUZZZ */

#define HNDRTE_PMUREG(si, member) \
	(AOB_ENAB(si) ? \
	(&(hndrte_pmur)->member) : \
	(&(hndrte_ccr)->member))

/** prevents backplane stall caused by subsequent writes to 'ilp domain' PMU registers */
#define HNDRTE_PMU_ILP_WR(r, v) do { \
		if (hndrte_sih && hndrte_sih->pmurev >= 22) \
			while (R_REG(hndrte_osh, \
			       HNDRTE_PMUREG(hndrte_sih, pmustatus)) & PST_SLOW_WR_PENDING) {}; \
		W_REG(hndrte_osh, (r), (v)); \
	} while (0)

#ifndef HNDRTE_POLLING
static void hndrte_pmu_isr(void *cbdata, uint32 intst);
#endif // endif

#if defined(FIQMODE) && defined(__ARM_ARCH_7R__)
static void hndrte_fiqtrap_handler(uint32 epc, uint32 lr, uint32 sp, uint32 cpsr);
extern uint32 hndrte_set_fiqtrap(uint32 hook);
#endif /* FIQMODE && __ARM_ARCH_7R__ */

/*
 * Timing support:
 *	hndrte_delay(us): Delay us microseconds.
 */

/*
 * c0 ticks per usec - used by hndrte_delay() and is based on 80Mhz clock.
 * Values are refined after function hndrte_cpu_init() has been called, and are recalculated
 * when the CPU frequency changes.
 */
static uint32 c0counts_per_us = HT_CLOCK / 1000000;
static uint32 c0counts_per_ms = HT_CLOCK / 1000;

/*
 * ticks per msec - used by hndrte_update_now() and is based on either 80Mhz
 * clock or 32Khz clock depending on the compile-time decision.
 */
/* ILP clock speed default to 32KHz */

#define PMUTICK_CALC_COUNT_SHIFT 4	/* 1<<4 times around loop) */

/* ms_per_pmutick is scaled (shifted) to improve accuracy */
#define MS_PER_PMUTICK_DEFAULT_SCALE 32
static uint32 ms_per_pmutick =
	((uint64) 1000 << MS_PER_PMUTICK_DEFAULT_SCALE) / ILP_CLOCK;
static uint32 ms_per_pmutick_scale = MS_PER_PMUTICK_DEFAULT_SCALE;

/* pmuticks_per_ms is now scaled (shifted) to improve accuracy */
#define PMUTICKS_PER_MS_SCALE_DEFAULT PMUTICK_CALC_COUNT_SHIFT
static uint32 pmuticks_per_ms = (ILP_CLOCK << PMUTICKS_PER_MS_SCALE_DEFAULT) / 1000;
static uint32 pmuticks_per_ms_scale = PMUTICKS_PER_MS_SCALE_DEFAULT;

/* PmuRev0 has a 10-bit PMU RsrcReq timer which can last 31.x msec
 * at 32KHz clock. To work around this limitation we chop larger timer to
 * multiple maximum 31 msec timers. When these 31 msec timers expire the ISR
 * will be running at 32KHz to save power.
 */
static uint max_timer_dur = (1 << 10) - 1;	/* Now in ticks!! */

/* PmuRev1 has a 24-bit PMU RsrcReq timer. However it pushes all other bits
 * upward. To make the code to run for all revs we use a variable to tell how
 * many bits we need to shift.
 */

#if BCMCHIPID == BCM4328_CHIP_ID
static uint8 flags_shift = 0;
#else
#define flags_shift	14
#endif // endif

static uint32 lastcount = 0;
static uint64 cur_ticks = 0;
static uint64 ms_remainder = 0;

/* Registered halt callback function. */
static hndrte_halt_handler g_hndrte_haltfn = NULL;

/** updates several time related global variables and returns current time in [ms] */
uint32
hndrte_update_now(void)
{
	uint32 diff, ticks, ms, rem_high, rem_low;

	/* PR88659: pmutimer is updated on ILP clock asynchronous to the CPU read.  Its
	 * value may change DURING the read, so the read must be verified and retried (but
	 * not in a loop, in case CPU is running at ILP).
	 */
	if ((hndrte_sih == NULL) || (hndrte_ccr == NULL))
		return 0;

	ticks = R_REG(hndrte_osh, HNDRTE_PMUREG(hndrte_sih, pmutimer));
	if (ticks != R_REG(hndrte_osh, HNDRTE_PMUREG(hndrte_sih, pmutimer)))
		ticks = R_REG(hndrte_osh, HNDRTE_PMUREG(hndrte_sih, pmutimer));

	/* The next line assumes that we update at least every 2**32 ticks */
	diff = ticks - lastcount;
	if (diff == 0)
		return 0;
	lastcount = ticks;
	cur_ticks += diff;

	/* Calculate the ms and the remainder */
	ms_remainder += (uint64) diff * ms_per_pmutick;

	/*
	 * We want to calculate ms_remainder >> ms_per_pmutick_scale
	 * but that would be a 64-bit op and the compiler would
	 * generate a call to a 64-bit shift library routine which we
	 * do not support.  So we do the shift in 32-bit pices.  Note
	 * that we take advantage of knowing that the scale is > 32.
	 */
	rem_low = (uint32) ms_remainder;
	rem_high = ms_remainder >> 32;
	ms = rem_high >> (ms_per_pmutick_scale - 32);
	rem_high &= (1 << (ms_per_pmutick_scale - 32)) - 1;

	ms_remainder = ((uint64) rem_high << 32) | rem_low;

	return ms;
}

#ifdef BCMDBG_SD_LATENCY
static uint32 lastcount_us = 0;

uint32
hndrte_update_now_us(void)
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
#endif /* BCMDBG_SD_LATENCY */

void
hndrte_delay(uint32 us)
{
	uint32 curr, c0_wait;

#ifdef BCMQT
	/* To compensate for the slowness of quickturn */
	us /= 3000;
#endif  /* BCMQT */

	curr = get_arm_cyclecount();
	c0_wait = us * c0counts_per_us;

	while ((get_arm_cyclecount() - curr) < c0_wait)
		;
}

#ifdef BCMDBG_LOADAVG
/* Note: load average mechanism also requires BCMDBG_FORCEHT to be defined */

#define LOADAVG_HZ		100		/* Sample Hz */
#define LOADAVG_ALPHA		0.99		/* Decay per sample */
#define LOADAVG_PRT_IVAL	100		/* Print interval, in units of Hz */

#define LOADAVG_A		((int)(LOADAVG_ALPHA * 256))

#define LOADAVG_WFI_INSTR	0xbf30
#define LOADAVG_CODESPACE_MASK	0x1fffff

static unsigned int loadavg;
static unsigned int loadavg_prt;
#if defined(__ARM_ARCH_7R__)
static unsigned int loadavg_time;
#endif // endif
#endif /* BCMDBG_LOADAVG */

#ifndef DEADMAN_TIMEOUT
#define DEADMAN_TIMEOUT		0
#endif // endif

#ifndef BCM_BOOTLOADER
static uint32 deadman_to = DEADMAN_TIMEOUT;
#if !defined(BCMDBG_LOADAVG)
static hndrte_timer_t deadman_timer;

static void
_deadman_timer(hndrte_timer_t *timer)
{
	si_t *sih = (si_t *) timer->data;

	/* refresh the deadman timer w/the specified value */
	hnd_cpu_deadman_timer(sih, deadman_to);
}
#endif /* !BCMDBG_LOADAVG */
#endif /* BCM_BOOTLOADER */

static const char BCMATTACHDATA(rstr_deadman_to)[] = "deadman_to";

/*
 * hndrte.
 *
 * Initialize and background:
 *	hndrte_cpu_init: Initialize the world.
 */

void
BCMATTACHFN(hndrte_cpu_init)(si_t *sih)
{
	uint32 pmutimer, startcycles, cycles, rem;
	osl_t *osh = si_osh(sih);
	int i;
	uint32 ticks, ticks_high, ticks_low;
	uint64 ticks64;

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

	/* Set trap handler */
	hndrte_set_trap((uint32)hndrte_trap_handler);
#if defined(FIQMODE) && defined(__ARM_ARCH_7R__)
	hndrte_set_fiqtrap((uint32)hndrte_fiqtrap_handler);
#endif // endif
	/* Initialize timers */
	c0counts_per_ms = (si_cpu_clock(sih) + 999) / 1000;
	c0counts_per_us = (si_cpu_clock(sih) + 999999) / 1000000;

	/*
	 * Compute the pmu ticks per ms.  This is done by counting a
	 * few PMU timer transitions and using the ARM cyclecounter as
	 * a more accurate clock to measure the PMU tick interval.
	 */

	/* Loop until we see a change */
	pmutimer = R_REG(osh, HNDRTE_PMUREG(sih, pmutimer));
	while (pmutimer == R_REG(osh, HNDRTE_PMUREG(sih, pmutimer))) {}
	/* There is a clock boundary crosssing so do one more read */
	pmutimer = R_REG(osh, HNDRTE_PMUREG(sih, pmutimer));

	/* The PMU timer just changed - start the cyclecount timer */
	startcycles = get_arm_cyclecount();

	for (i = 0; i < (1 << PMUTICK_CALC_COUNT_SHIFT); i++) {
		while (pmutimer == R_REG(osh, HNDRTE_PMUREG(sih, pmutimer))) {}
		pmutimer = R_REG(osh, HNDRTE_PMUREG(sih, pmutimer));
	}

	cycles = get_arm_cyclecount() - startcycles;
	/*
	 * Calculate the pmuticks_per_ms with scaling for greater
	 * accuracy.  We scale by the amount needed to shift the
	 * c0counts_per_ms so the leading bit is set.  Since the
	 * divisor (cycles counted) is implicity shifted by
	 * PMUTICK_CALC_COUNT_SHIFT so that reduces the scale.
	 *
	 * We round up because we want the first tick AFTER the
	 * requested ms - otherwise we will get an extraneuous
	 * interrupt one tick early.
	 */

	pmuticks_per_ms_scale = CLZ(c0counts_per_ms) - PMUTICK_CALC_COUNT_SHIFT;
	pmuticks_per_ms =  ((c0counts_per_ms << CLZ(c0counts_per_ms)) / cycles);
	pmuticks_per_ms++;		/* Round up */

	/* Calculate the PMU clock frequency and set it */
	ticks64 = ((uint64) 1000) * pmuticks_per_ms;	/* ticks per sec */
	ticks_high = ticks64 >> 32;
	ticks_low = (uint32) ticks64;
	ticks = ticks_low >> pmuticks_per_ms_scale;
	ticks += ticks_high << (32 - pmuticks_per_ms_scale);

	si_pmu_ilp_clock_set(ticks);	/* Set */

	/*
	 * Do long-division to get a value that is the
	 * ms_per_pmutick scaled to have 31 bits of accuracy.
	 * Stopping one bit short (i.e., not using 32 bits of
	 * accuracy) leaves a spare bit to handle overflows during
	 * certain 32-bit math operations below.  Since we know that
	 * the pmuticks happen more often than once per ms we know
	 * that the scale will be >32.  This fact is used in other
	 * calculations.
	 */

	rem = cycles;			/* Initial numerator */
	ms_per_pmutick_scale = PMUTICK_CALC_COUNT_SHIFT;
	ms_per_pmutick = 0;

	while ((ms_per_pmutick & 0xc0000000) == 0) {
		uint32 partial, lz;
		/* Scale the remaining dividend */
		lz = MIN(CLZ(rem), CLZ(ms_per_pmutick) - 1);
		ms_per_pmutick <<= lz;
		rem <<= lz;
		ms_per_pmutick_scale += lz;

		partial = rem / c0counts_per_ms;
		ms_per_pmutick += partial;
		rem -= partial * c0counts_per_ms;
	}

	if (sih->pmurev >= 1) {
		max_timer_dur = ((1 << 24) - 1);
#if BCMCHIPID == BCM4328_CHIP_ID
		flags_shift = 14;
#endif // endif
	} else {
		max_timer_dur = ((1 << 10) - 1);
	}

#ifndef HNDRTE_POLLING
	/* Register the timer interrupt handler */
	if (AOB_ENAB(sih))
		hndrte_add_isr(0, PMU_CORE_ID, 0, (isr_fun_t)hndrte_pmu_isr, NULL, SI_BUS);
	else
		si_cc_register_isr(sih, hndrte_pmu_isr, CI_PMU, NULL);
#endif // endif

#ifndef BCM_BOOTLOADER
#if defined(BCMDBG_LOADAVG) && defined(__ARM_ARCH_7R__)
	loadavg_time = si_cpu_clock(sih)/LOADAVG_HZ;
	/* set unused deadman_to to pass the complier ROM map check */
	deadman_to = 0;
	/* Set the Inttimer first time value to 1 second to wait for initialization done */
	hnd_cpu_loadavg_timer(sih, loadavg_time*LOADAVG_HZ);
#else

	/* if a deadman_to override is provided in the nvram, use that */
	if (getintvar(NULL, rstr_deadman_to))
		deadman_to = getintvar(NULL, rstr_deadman_to);

	/* set up deadman timer if a timeout was specified */
	if (deadman_to) {
		int32 refresh_time;

		/* refresh every 1 second */
		refresh_time = 1;
		if (refresh_time > 0) {
			bzero(&deadman_timer, sizeof(hndrte_timer_t));
			deadman_timer.mainfn = _deadman_timer;
			deadman_timer.data = (void *)sih;
			if (hndrte_add_timer(&deadman_timer, refresh_time * 1000, TRUE))
				/* set the deadman timer to the initial value */
				hnd_cpu_deadman_timer(sih, deadman_to);
		}
	}
#endif /* BCMDBG_LOADAVG  && __ARM_ARCH_7R__ */
#endif /* BCM_BOOTLOADER */
} /* hndrte_cpu_init */

void *
#if defined(BCMDBG_MEM) || defined(BCMDBG_MEMFAIL)
hndrte_dma_alloc_consistent(uint size, uint16 align_bits, uint *alloced, void *pap,
	char *file, int line)
#else
hndrte_dma_alloc_consistent(uint size, uint16 align_bits, uint *alloced, void *pap)
#endif // endif
{
	void *buf;

	/* align on a OSL defined boundary */
#if defined(BCMDBG_MEM) || defined(BCMDBG_MEMFAIL)
	if (!(buf = hndrte_malloc_align(size, align_bits, file, line)))
#else
	if (!(buf = hndrte_malloc_align(size, align_bits)))
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
hndrte_dma_free_consistent(void *va)
{
	hndrte_free(va);
}

/* Two consecutive res_req_timer writes must be 2 ILP + 2 OCP clocks apart */
#if BCMCHIPID == BCM4328_CHIP_ID
static void
pr41608_war(void)
{
	static volatile uint32 hndrte_pmutmr_time = 0;
	uint32 expect;

	if (hndrte_sih->pmurev > 0)
		return;

	expect = hndrte_pmutmr_time + 3;
	if (expect < hndrte_pmutmr_time)
		while (R_REG(hndrte_osh, HNDRTE_PMUREG(hndrte_sih, pmutimer))
			>= hndrte_pmutmr_time);
	while (R_REG(hndrte_osh, HNDRTE_PMUREG(hndrte_sih, pmutimer)) <= expect)
		;
	hndrte_pmutmr_time = R_REG(hndrte_osh, HNDRTE_PMUREG(hndrte_sih, pmutimer));
}
#else
#define pr41608_war()
#endif /* BCMCHIPID == BCM4328_CHIP_ID */

/*
 * The number of ticks is limited by max_timer_dur so we break up
 * requests that are too large and keep track of the extra.
 */
static uint32 timer_extra_ticks = 0;
static uint64 timer_extra_ticks_end;

static void
hndrte_set_irq_tick_timer(uint32 ticks)
{
	uint32 req;

	if (ticks > max_timer_dur) {
		/* don't req HT if we are breaking a large timer to multiple max h/w duration */
		timer_extra_ticks = ticks - max_timer_dur;
		timer_extra_ticks_end = cur_ticks + ticks;
		ticks = max_timer_dur;
		req = (PRRT_ALP_REQ | PRRT_INTEN) << flags_shift;
	} else {
		req = (PRRT_HT_REQ | PRRT_INTEN) << flags_shift;
		timer_extra_ticks = 0;

		if (hndrte_sih->pmurev >= 15) {
			req |= PRRT_HQ_REQ << flags_shift;
		}
	}

	pr41608_war();

	if (ticks == 0) {
		ticks = 2;		/* Need a 1->0 transition */
	}

	HNDRTE_PMU_ILP_WR(HNDRTE_PMUREG(hndrte_sih, res_req_timer), req | ticks);
	(void)R_REG(hndrte_osh, HNDRTE_PMUREG(hndrte_sih, res_req_timer));
}

/**
 * The RTOS maintains a linked list of software timers, and requires a hardware timer to generate an
 * interrupt on the software timer that expires first. This function sets that hardware timer.
 */
void
hndrte_set_irq_timer(uint ms)
{
	uint32 ticks, ticks_high, ticks_low;
	uint64 ticks64;

	/*
	 *  Convert the ms to ticks rounding up so that we take the
	 *  first rupt AFTER the ms has expired.
	 *
	 *  Use 64-bit math with scaling to preserve accurracy.  This
	 *  is just "tick = ticks64  >> pmuticks_per_ms_scale" but
	 *  using only 32 bit shift operations.
	 */
	ticks64 = ((uint64) ms) * pmuticks_per_ms;
	ticks_high = ticks64 >> 32;
	ticks_low = (uint32) ticks64;
	ticks = ticks_low >> pmuticks_per_ms_scale;
	ticks += ticks_high << (32 - pmuticks_per_ms_scale);

	hndrte_set_irq_tick_timer(ticks);
}

#if  defined(WLSRVSDB)
uint32 hndrte_clk_count(void);
uint32
hndrte_clk_count(void)
{
	uint32 count = 0;

	count = R_REG(hndrte_osh, HNDRTE_PMUREG(hndrte_sih, pmutimer));
	return count;
}
#endif /* WLSRVSDB */

void
hndrte_ack_irq_timer(void)
{
	pr41608_war();

	HNDRTE_PMU_ILP_WR(HNDRTE_PMUREG(hndrte_sih, res_req_timer), 0);
	(void)R_REG(hndrte_osh, HNDRTE_PMUREG(hndrte_sih, res_req_timer));
}

void
hndrte_wait_irq(si_t *sih)
{
	hnd_cpu_wait(sih);
}

void
hndrte_enable_interrupts(void)
{
	enable_arm_irq();
#if defined(FIQMODE) && defined(__ARM_ARCH_7R__) && !defined(BCM_BOOTLOADER)
	enable_arm_fiq();
#endif /* FIQMODE && __ARM_ARCH_7R__ && !BCM_BOOTLOADER */
}

void
hndrte_disable_interrupts(void)
{
	disable_arm_irq();
#if defined(FIQMODE) && defined(__ARM_ARCH_7R__) && !defined(BCM_BOOTLOADER)
	disable_arm_fiq();
#endif /* FIQMODE && __ARM_ARCH_7R__ && !BCM_BOOTLOADER */
}

/* Enable/Disable h/w HT request */
#if defined(__ARM_ARCH_4T__)
#if BCMCHIPID == BCM4328_CHIP_ID || BCMCHIPID == BCM4325_CHIP_ID
#define HW_HTREQ_ON() \
	do { \
		if (hndarm_rev < 2) \
			AND_REG(hndrte_osh, ARMREG(hndarm_armr, clk_ctl_st), ~CCS_FORCEHWREQOFF); \
	} while (0)
#define HW_HTREQ_OFF() \
	do { \
		if (hndarm_rev < 2) \
			OR_REG(hndrte_osh, ARMREG(hndarm_armr, clk_ctl_st), CCS_FORCEHWREQOFF); \
	} while (0)
#else	/* BCMCHIPID != BCM4328_CHIP_ID && BCMCHIPID != BCM4325_CHIP_ID */
#define HW_HTREQ_ON()
#define HW_HTREQ_OFF()
#endif	/* BCMCHIPID != BCM4328_CHIP_ID && BCMCHIPID != BCM4325_CHIP_ID */
#elif defined(__ARM_ARCH_7M__)
#define HW_HTREQ_ON()
#define HW_HTREQ_OFF()
#elif defined(__ARM_ARCH_7R__)
#define HW_HTREQ_ON()
#define HW_HTREQ_OFF()
#define	HT_CLOCK_120		120000000
#endif	/* __ARM_ARCH_7M__ */

void
hndrte_idle_init(si_t *sih)
{
#ifndef HNDRTE_POLLING
	HW_HTREQ_OFF();
#endif // endif
}

#ifndef HNDRTE_POLLING
/* XXX This secondary chipcommon ISR should be moved to hndpmu.c. However
 * since we use it as the timer hence we leave it here.
 */
static void
hndrte_pmu_isr(void *cbdata, uint32 ccintst)
{
	/* Handle pmu timer interrupt here */

	/* Clear the pmustatus.intpend bit */
	W_REG(hndrte_osh, HNDRTE_PMUREG(hndrte_sih, pmustatus), PST_INTPEND);
	(void)R_REG(hndrte_osh, HNDRTE_PMUREG(hndrte_sih, pmustatus));

	if (AOB_ENAB(hndrte_sih)) {
		/* Clear resource req timer bit */
		W_REG(hndrte_osh, HNDRTE_PMUREG(hndrte_sih, pmuintstatus), PMUREQTIMER);
		(void)R_REG(hndrte_osh, HNDRTE_PMUREG(hndrte_sih, pmuintstatus));
	}

	if (
#if BCMCHIPID == BCM4328_CHIP_ID
	    ccintst == 0 ||
#endif // endif
	    (R_REG(hndrte_osh, HNDRTE_PMUREG(hndrte_sih, res_req_timer)) &
	      (PRRT_REQ_ACTIVE << flags_shift))) {
#ifndef BCM_OL_DEV
		hndrte_ack_irq_timer();
#endif /* BCM_OL_DEV */
		if (timer_extra_ticks) {
			/* Part way through a long timer */
			hndrte_update_now();
			if (timer_extra_ticks_end > cur_ticks) {
				hndrte_set_irq_tick_timer(timer_extra_ticks_end - cur_ticks);
			} else {
				/* Need a 0-tick timer to kick start HT clock */
				hndrte_set_irq_tick_timer(0);
			}
		}
	}
}
#endif	/* !HNDRTE_POLLING */

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

/* Trap initialization */
void
BCMATTACHFN(hndrte_trap_init)(void)
{
#if defined(BCMDBG_TRAP) && !defined(_HNDRTE_SIM_)
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

#endif /* BCMDBG_TRAP && !_HNDRTE_SIM_ */

#ifdef	__ARM_ARCH_7R__
	uint32	region_size_index;
	uint32	region_start_align;
	uint32	region_start;
	uint32	region_end;
	/* the first four MPU regions are being set in startarm-cr4.S */
	uint8	next_region = 4;

/* the NULL region is from 0-0x20 (2^5)) and it is always protected */
#define MPU_NULL_REGION_SIZE_INDEX	5

#ifdef ROMBASE
	region_start = ROMBASE;
	region_end = ROMEND;

	/* if the ROM is starting immediately after 0-0x20 */
	/* merge the two regions */
	if (region_start == (1 << MPU_NULL_REGION_SIZE_INDEX)) {
		region_start = 0;
	}
	if (cr4_calculate_mpu_region(region_start, region_end, &region_start_align,
		&region_size_index) == BCME_OK) {
			/* in CR4 set mpu write protection to the ROM area starting in address 0 */
			cr4_mpu_set_region(next_region, region_start_align, region_size_index-1,
				AP_VAL_110| TEX_VAL_110| C_BIT_ON);
			next_region++;
	}
	/* add special protection for 0-0x20 if the ROM is not starting */
	/* in 0x20 */
	if (region_start != 0) {
		/* in CR4 set mpu write protection to address 0-0x20 */
		cr4_mpu_set_region(next_region, 0x00000000, MPU_NULL_REGION_SIZE_INDEX-1,
			AP_VAL_110| TEX_VAL_110| C_BIT_ON);
		next_region++;
	}
#else
	/* in CR4 set mpu write protection to address 0-0x20 */
	cr4_mpu_set_region(next_region, 0x00000000, MPU_NULL_REGION_SIZE_INDEX-1,
		AP_VAL_110| TEX_VAL_110| C_BIT_ON);
	next_region++;
#endif /* ROMBASE */

#ifdef MPU_RAM_PROTECT_ENABLED
	extern char _ram_mpu_region_start[];
	extern char _ram_mpu_region_end[];
	region_start = (uint32)_ram_mpu_region_start;
	region_end = (uint32)_ram_mpu_region_end;

	if (cr4_calculate_mpu_region(region_start, region_end, &region_start_align,
		&region_size_index) == BCME_OK) {
			/* set mpu write protection to address maximum size of code area */
			cr4_mpu_set_region(next_region, region_start_align, region_size_index-1,
				AP_VAL_110| TEX_VAL_110| C_BIT_ON);
			next_region++;
	}
#endif  /* MPU_RAM_PROTECT_ENABLED */
#endif	/* __ARM_ARCH_7R__ */

#if defined(BCMDBG_LOADAVG) && !defined(__ARM_ARCH_7R__)
	/* Set up SysTick interrupt for profiling */
	wreg32(CM3_NVIC_TICK_RLDVAL,
	       HT_CLOCK / LOADAVG_HZ);
	wreg32(CM3_NVIC_TICK_CSR,
	       CM3_NVIC_TICK_CSR_CLKSOURCE | CM3_NVIC_TICK_CSR_TICKINT | CM3_NVIC_TICK_CSR_ENABLE);
#endif /* BCMDBG_LOADAVG  && !__ARM_ARCH_7R__ */

#ifdef  __ARM_ARCH_7R__
/* for CR4, clock at start is configured as 120Mhz [instead of default 80MHz].
 * So, make sure that, c0counts_per_us is inited with approx count before for all delays
 * used before hndrte_cpu_init().
 */
	c0counts_per_us = HT_CLOCK_120 / 1000000;

	/* enable imprecise aborts/exceptions */
	enable_arm_dab();
#endif // endif
} /* hndrte_trap_init */

/*
 * Use a debug comparator reg to cause a trap on stack underflow.
 */

/* Stack protection initialization */
#ifndef BCMDBG_STACKP_LG2SIZE
#define BCMDBG_STACKP_LG2SIZE 5
#endif // endif

void
BCMATTACHFN(hndrte_stack_prot)(void *stack_top)
{
#if defined(__ARM_ARCH_7M__)
#if defined(STACK_PROT_TRAP) && defined(BCMDBG_TRAP) && !defined(_HNDRTE_SIM_)
	/* Point at an STACKP_SIZE-aligned and sized area at the end */
	uint32 st = (((uint32) stack_top) + (1 << BCMDBG_STACKP_LG2SIZE) - 1) &
		~((1 << BCMDBG_STACKP_LG2SIZE) - 1);
	wreg32(CM3_DWT_MASK1, BCMDBG_STACKP_LG2SIZE);
	wreg32(CM3_DWT_COMP1, st);
	(void)rreg32(CM3_DWT_FUNCTION1);
	wreg32(CM3_DWT_FUNCTION1, CM3_DWT_FUNCTION_WP_RDWR);
#endif /* STACK_PROT ... */
#endif /* ARCH_7M */
}

void
hndrte_memtrace_enab(bool on)
{
#if defined(BCMDBG_TRAP) && !defined(_HNDRTE_SIM_)
#if defined(__ARM_ARCH_7M__)
	uint32 val = rreg32(CM3_DBG_EMCR) | CM3_DBG_EMCR_MON_EN;

	if (on)
		val |= CM3_DBG_EMCR_TRCENA;
	else
		val &= ~CM3_DBG_EMCR_TRCENA;

	/* Enable/Disable DWT (data watchpoint and tracing) functionality and Monitor Mode. */
	wreg32(CM3_DBG_EMCR, val);
#endif /* __ARM_ARCH_7M__ */
#endif /* BCMDBG_TRAP && !_HNDRTE_SIM_ */
}
#if defined(__ARM_ARCH_7R__)
extern uint32 _rambottom;
#endif // endif
#if defined(FIQMODE) && defined(__ARM_ARCH_7R__)
void
hndrte_fiqtrap_handler(uint32 epc, uint32 lr, uint32 sp, uint32 cpsr)
{
#if defined(BCMDBG_LOADAVG)
#ifdef EVENT_LOG_ROM_PRINTF_MAP
	/* Save current hook. */
	post_printf_hook hook = get_current_post_printf_hook();
	unregister_post_printf_hook();
#endif // endif

	hnd_cpu_loadavg_timer(hndrte_sih, loadavg_time);

	{
		unsigned int idle = ((epc & LOADAVG_CODESPACE_MASK) >= 2 &&
			(*(uint16 *)(epc - 2) == LOADAVG_WFI_INSTR));
		unsigned int load = (idle ? 0 : 100) * 256;

		/* Update average using fixed point arithmetic (8.8) */
		loadavg = (loadavg * LOADAVG_A + load * (256 - LOADAVG_A)) >> 8;
		if (++loadavg_prt >= LOADAVG_PRT_IVAL) {
			printf("Load: %2d.%02d (pc=0x%08x lr=0x%08x sp=0x%08x)\n",
				loadavg >> 8, (100 * (loadavg & 255)) >> 8, epc, lr, sp);
			loadavg_prt = 0;
		}
	}
#ifdef EVENT_LOG_ROM_PRINTF_MAP
	/* restore the hook. */
	register_post_printf_hook(hook);
#endif // endif

#else /* BCMDGB_LOADAVG */

	/* Deadman Fired (the dongle is likely spinning at epc) */
#if defined(BCMUSBDEV_ENABLED) || defined(FWID)
	printf("\nFWID 01-%x\n", gFWID);
#endif // endif

	{
		uint i = 0;
		uint j = 0;
		uint32 *stack = (uint32*)sp;
#if !defined(__ARM_ARCH_7R__)
		uint stack_headroom = (_memsize - sp) / 4;
#else
		uint stack_headroom = (_rambottom - sp) / 4;
#endif // endif

		/* Note that UTF parses the first line, so the format should not be changed. */
		printf("\nDeadman timer expired\n");
		printf("\nTRAP 0(dead): pc %x, lr %x, sp %x, cpsr %x\n",
			epc, lr, sp, cpsr);

		for (i = 0, j = 0; j < 16 && i < stack_headroom; i++) {
			/* make sure it's a odd (thumb) address and at least 0x100 */
			if (!(stack[i] & 1) || stack[i] < 0x100)
				continue;

			/* Check if it's within the RAM or ROM text regions */
			if ((stack[i] <= (uint32)text_end) ||
#if defined(BCMROMOFFLOAD)
				((stack[i] >= BCMROMBASE && (stack[i] <= BCMROMEND))) ||
#endif /* BCMROMOFFLOAD */
				FALSE) {
					printf("sp+%x %08x\n", (i*4), stack[i]);
					j++;
			}
		}
	}

	hndrte_die_no_trap();

#endif /* BCMDGB_LOADAVG */
#ifdef EVENT_LOG_ROM_PRINTF_MAP
	unregister_post_printf_hook();
#endif // endif

} /* hndrte_fiqtrap_handler */
#endif /* FIQMODE && __ARM_ARCH_7R__ */

/**
 * this handler is called by low level assembly code. It should withstand reentrancy. CM3 nor CR4
 * make use of the FVIC controller in the ARM subsystem. An implication is that a 'normal' interrupt
 * can be preempted by a 'fast interrupt', but not by another 'normal' interrupt.
 */
void
hndrte_trap_handler(trap_t *tr)
{
	BUZZZ_LVL1(HNDRTE_TRAP_HANDLER, 1, tr->type);

	/* Save the trap pointer */
	hndrte_debug_info.trap_ptr = tr;

#ifdef AXI_TIMEOUTS
	/* clear backplane timeout, if that caused the fault */
#if defined(__ARM_ARCH_7R__)
	if (tr->type == TR_DAB)
#else
	if (tr->type == TR_FAULT)
#endif // endif
		si_clear_backplane_to(hndrte_sih);
#endif /* AXI_TIMEOUTS */

#ifndef HNDRTE_POLLING
#if defined(__ARM_ARCH_4T__) || defined(__ARM_ARCH_7R__)
	if (tr->type == TR_IRQ) {
#if BCMCHIPID == BCM4328_CHIP_ID
		/* Check PMU res_req_timer interrupt apart from others because
		 * this may be an "intermediate" timer due to the h/w max timer
		 * length limitation. We request ALP with these intermediate
		 * timers to save power therefore don't request HT.
		 */
		if (hndrte_sih->ccrev < 21 &&
		    (R_REG(hndrte_osh, HNDRTE_PMUREG(hndrte_sih, res_req_timer)) &
			PRRT_REQ_ACTIVE)) {
			hndrte_pmu_isr(NULL, 0);
			return;
		}
#endif /* BCMCHIPID == BCM4328_CHIP_ID */
		HW_HTREQ_ON();
		if (BCM43602_CHIP(hndrte_sih->chip)) {
			/* Double CR4 clk for performance */
			hndrte_cpu_clockratio(hndrte_sih, 2);
		}
		hndrte_isr();
		if (BCM43602_CHIP(hndrte_sih->chip)) {
			/* For power saving need to turn off clock doubler */
			hndrte_cpu_clockratio(hndrte_sih, 1);
		}
		HW_HTREQ_OFF();
		BUZZZ_LVL2(HNDRTE_TRAP_HANDLER_RTN, 0);
		return;
	}
#elif defined(__ARM_ARCH_7M__)
	if (tr->type >= TR_ISR && tr->type < TR_ISR + ARMCM3_NUMINTS) {
		HW_HTREQ_ON();
		hndrte_isr();
		HW_HTREQ_OFF();
		return;
	}

	if (BCMOVLHW_ENAB(hndrte_sih) && (tr->type == TR_BUS)) {
		if (si_arm_ovl_int(hndrte_sih, tr->pc))
			return;
	}

#ifdef BCMDBG_LOADAVG
	if (tr->type == TR_SYSTICK) {
		/* CPU was idle if it broke out of a wfi instruction. */
		unsigned int idle = ((tr->epc & LOADAVG_CODESPACE_MASK) >= 2 &&
		                     (*(uint16 *)(tr->epc - 2) == LOADAVG_WFI_INSTR));
		unsigned int load = (idle ? 0 : 100) * 256;
		/* Update average using fixed point arithmetic (8.8) */
		loadavg = (loadavg * LOADAVG_A + load * (256 - LOADAVG_A)) >> 8;
		if (++loadavg_prt >= LOADAVG_PRT_IVAL) {
			printf("Load: %2d.%02d (pc=0x%08x lr=0x%08x sp=0x%08x)\n",
			       loadavg >> 8, (100 * (loadavg & 255)) >> 8,
			       tr->epc, tr->r14, tr->r13);
			loadavg_prt = 0;
		}
		return;
	}
#endif	/* BCMDBG_LOADAVG */
#endif	/* __ARM_ARCH_7M__ */
#endif	/* !HNDRTE_POLLING */

#ifdef TR_ARMRST
	if (tr->type == TR_ARMRST) {
		printf("ARM reset: SErr=%d, Rstlg=0x%x\n",
		       hndrte_ccsbr->sbtmstatehigh & SBTMH_SERR,
		       R_REG(hndrte_osh, ARMREG(hndarm_armr, resetlog)));
	}
#endif // endif

#if defined(__ARM_ARCH_7M__)
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
#endif /* defined(__ARM_ARCH_7M__) */

#if defined(BCMDBG_ASSERT) && defined(BCMDBG_ASSERT_TRAP)
#if defined(__ARM_ARCH_7M__)
	/* DBG_ASSERT_TRAP causes a trap/exception when an ASSERT fails, instead of calling
	 * an assert handler to log the file and line number. This is a memory optimization
	 * that eliminates the strings associated with the file/line and the function call
	 * overhead associated with invoking the assert handler. The assert location can be
	 * determined based upon the program counter displayed by the trap handler.
	 *
	 * The system service call (SVC) instruction is used to generate a software
	 * interrupt for a failed assert.
	 */
	if (tr->type == TR_SVC) {
		/* Adjust the program counter to the assert location. */
		tr->pc -= 2;
		printf("\n\nASSERT pc %x\n", tr->pc);
	}
#elif defined(__ARM_ARCH_7R__)
	if (tr->type == TR_SWI) {
		/* Adjust the program counter to the assert location. */
		tr->pc -= 2;
		printf("\n\nASSERT pc %x\n", tr->pc);
	}
#endif /* __ARM_ARCH_7M__ */
#endif /* BCMDBG_ASSERT && BCMDBG_ASSERT_TRAP */
	/*
	 * ARM7TDMI trap types:
	 *	0=RST, 1=UND, 2=SWI, 3=IAB, 4=DAB, 5=BAD, 6=IRQ, 7=FIQ
	 *
	 * ARM CM3 trap types:
	 *	1=RST, 2=NMI, 3=FAULT, 4=MM, 5=BUS, 6=USAGE, 11=SVC,
	 *	12=DMON, 14=PENDSV, 15=SYSTICK, 16+=ISR
	 */

	 uint i = 0;
	 uint j = 0;
	 uint32 *stack = (uint32*)tr->r13;
#if !defined(__ARM_ARCH_7R__)
	 uint stack_headroom = (_memsize - tr->r13) / 4;
#else
	 uint stack_headroom = (_rambottom - tr->r13) / 4;
#endif // endif
#if defined(BCMSDIODEV_ENABLED)
	 printf("\nFWID 01-%x\nflags %x\n", sdpcm_shared.fwid, sdpcm_shared.flags);
#endif // endif
#if defined(BCMPCIEDEV_ENABLED)
	 printf("\nFWID 01-%x\nflags %x\n", pciedev_shared.fwid, pciedev_shared.flags);
#endif // endif

#if (defined(BCMUSBDEV_ENABLED) && !defined(BCM_BOOTLOADER)) || defined(FWID)
	printf("\nFWID 01-%x\n", gFWID);
#endif // endif

	/* Note that UTF parses the first line, so the format should not be changed. */
	printf("\nTRAP %x(%x): pc %x, lr %x, sp %x, cpsr %x, spsr %x\n",
	       tr->type, (uint32)tr, tr->pc, tr->r14, tr->r13, tr->cpsr, tr->spsr);
#if defined(__ARM_ARCH_7R__)
	if (tr->type == TR_DAB)
		printf("  dfsr %x, dfar %x\n",
		       get_arm_data_fault_status(), get_arm_data_fault_address());
	else if (tr->type == TR_IAB)
		printf("  ifsr %x, ifar %x\n",
		       get_arm_instruction_fault_status(), get_arm_instruction_fault_address());
#endif // endif

#if defined(BCMSPACE) || defined(HNDRTE_CONSOLE) || defined(BCM_OL_DEV)
	printf("  r0 %x, r1 %x, r2 %x, r3 %x, r4 %x, r5 %x, r6 %x\n",
	       tr->r0, tr->r1, tr->r2, tr->r3, tr->r4, tr->r5, tr->r6);
	printf("  r7 %x, r8 %x, r9 %x, r10 %x, r11 %x, r12 %x\n",
	       tr->r7, tr->r8, tr->r9, tr->r10, tr->r11, tr->r12);

	/*
	 * stack content before trap occured
	 */
	printf("\n   sp+0 %08x %08x %08x %08x\n",
		stack[0], stack[1], stack[2], stack[3]);
	printf("  sp+10 %08x %08x %08x %08x\n\n",
		stack[4], stack[5], stack[6], stack[7]);

	/*
	 * further search into the stack to locate possible return address
	 */

	for (i = 0, j = 0; j < 16 && i < stack_headroom; i++) {
		/* make sure it's a odd (thumb) address and at least 0x100 */
		if (!(stack[i] & 1) || stack[i] < 0x100)
			continue;

		/* Check if it's within the RAM or ROM text regions */
		if ((stack[i] <= (uint32)text_end) ||
#if defined(BCMROMOFFLOAD)
		((stack[i] >= BCMROMBASE && (stack[i] <= BCMROMEND))) ||
#endif /* BCMROMOFFLOAD */
		FALSE) {
			printf("sp+%x %08x\n", (i*4), stack[i]);
			j++;
		}
	}
#endif /* BCMSPACE */

#if defined(BCMSDIODEV_ENABLED)
	/* Fill in structure that be downloaded by the host */
	sdpcm_shared.flags     |= SDPCM_SHARED_TRAP;
	sdpcm_shared.trap_addr  = (uint32)tr;
#endif // endif

#if	defined(BCMPCIEDEV_ENABLED)
		/* Fill in structure that be downloaded by the host */
		pciedev_shared.flags	   |= PCIE_SHARED_TRAP;
		pciedev_shared.trap_addr	= (uint32)tr;
#endif // endif

	/* Halt processing without forcing a trap since we are already in the trap handler. */
	hndrte_die_no_trap();
} /* hndrte_trap_handler */

/*
 * Enter and stay in busy loop forever.
 */
void _hndrte_die(bool trap)
{
	/* Flush out any pending console print messages
	 * ensures there is room for critical log messages that
	 * may follow
	 */
	hndrte_cons_flush();

	/* Force a trap. This will provide a register dump and callstack for post-mortem debug. */
	if (trap) {
		int *null = NULL;
		*null = 0;
	}

#if (defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7R__)) && \
	(defined(BCMSDIODEV_ENABLED) || defined(BCMUSBDEV_ENABLED) || \
	defined(BCMPCIEDEV_ENABLED))
#ifndef BCM_BOOTLOADER
	/* Make sure deadman timer is cleared. */
#if !defined(BCMDBG_LOADAVG)
	hnd_cpu_deadman_timer(hndrte_sih, 0);
#endif // endif
#endif /* !BCM_BOOTLOADER */

	/* Call halt function, if it was registered. */
	if (g_hndrte_haltfn != NULL) {
		(*g_hndrte_haltfn)();
	}
#endif /* __ARM_ARCH_7M__ && (BCMSDIODEV_ENABLED || BCMUSBDEV_ENABLED) */

	/* Flush out any console print messages that have just been
	 * added as FW will no longer service THR interrupt
	 */
	hndrte_cons_flush();

	/* Spin forever... */
	while (1) {
		;
	}
}

void hndrte_set_fwhalt(hndrte_halt_handler handler)
{
	g_hndrte_haltfn = handler;
}

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
hndrte_cpu_clockratio(si_t *sih, uint8 div)
{
	int32 ret;

	ret = si_arm_clockratio(sih, div);

	if (div != 0) {
		/* global vars 'c0counts..' are changed here for hndrte_delay() / OSL_DELAY() */
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
}
