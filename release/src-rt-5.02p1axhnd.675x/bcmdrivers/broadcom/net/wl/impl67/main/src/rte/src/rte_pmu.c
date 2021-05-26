/** @file rte_pmu.c
 *
 * RTE support for PMU
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
 * $Id: rte_pmu.c 787983 2020-06-17 17:59:06Z $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <osl_ext.h>
#include <sbchipc.h>
#include <hndpmu.h>
#include <hndlhl.h>
#include <sbgci.h>
#include <hndsoc.h>
#include <rte_chipc.h>
#include "rte_chipc_priv.h"
#include "rte_pmu_priv.h"
#include <rte_isr.h>
#include <rte_pmu.h>
#include "rte_priv.h"
#include <bcmdevs.h>

#ifdef BCMDBG
#define PMU_MSG(x)	printf x
#else
#define PMU_MSG(x)
#endif /* BCMDBG */

struct pmu_extwake_ctx {
	pmu_extwake_event_t event;
	void *ctx;
	pmu_extwake_event_isr_cb_t event_isr_cb;
	pmu_extwake_event_worklet_cb_t event_worklet_cb;
	struct pmu_extwake_ctx *next;
};

struct pmu_info {
	si_t *sih;
	uint32 pmu_int_status; /* preserves pmu_isr_status for worklet */
	uint32 pmu_extwake_status; /* preserves pmu_extwake_status for worklet */
	pmu_extwake_ctx_t *pmu_extwake_ctx_list;
};
typedef struct pmu_info pmu_info_t;

static pmu_info_t pmu_info;

#ifndef RTE_POLL
static bool hnd_pmu_cc_isr(void* cbdata, uint index, uint32 intstatus);
static bool hnd_pmu_cc_worklet(void* cbdata);
#endif /* !RTE_POLL */

/* PmuRev1 has a 24-bit PMU RsrcReq timer. However it pushes all other bits
 * upward. To make the code to run for all revs we use a variable to tell how
 * many bits we need to shift.
 */
#define FLAGS_SHIFT	14

static pmuregs_t *hnd_pmur = NULL;	/* PMU core regs */

/*
 * ticks per msec - used by hnd_update_now() and is based on either 80Mhz
 * clock or 32Khz clock depending on the compile-time decision.
 */
/* ILP clock speed default to 32KHz */

#define PMUTICK_CALC_COUNT_SHIFT 4	/* 1<<4 times around loop) */

/* ms_per_pmutick is scaled (shifted) to improve accuracy */
#define MS_PER_PMUTICK_DEFAULT_SCALE 32
static uint32 ms_per_pmutick =	((uint64) 1000 << MS_PER_PMUTICK_DEFAULT_SCALE) / ILP_CLOCK;
static uint32 ms_per_pmutick_scale = MS_PER_PMUTICK_DEFAULT_SCALE;

/* pmuticks_per_ms is now scaled (shifted) to improve accuracy */
#define PMUTICKS_PER_MS_SCALE_DEFAULT PMUTICK_CALC_COUNT_SHIFT
static uint32 pmuticks_per_ms = (ILP_CLOCK << PMUTICKS_PER_MS_SCALE_DEFAULT) / 1000;
static uint32 pmuticks_per_ms_scale = PMUTICKS_PER_MS_SCALE_DEFAULT;

/* params for converting ticks <-> usec */
static uint32 pmutick_per_us;
static uint32 pmutick_per_us_scale;
static uint32 us_per_pmutick;
static uint32 us_per_pmutick_scale;

/* PmuRev0 has a 10-bit PMU RsrcReq timer which can last 31.x msec
 * at 32KHz clock. To work around this limitation we chop larger timer to
 * multiple maximum 31 msec timers. When these 31 msec timers expire the ISR
 * will be running at 32KHz to save power.
 */
static uint max_timer_dur = (1 << 10) - 1;	/* Now in ticks!! */

static pmu_info_t*
BCMRAMFN(hnd_pmu_get_pmu_info)(si_t *sih)
{
	return &pmu_info;
}

static void
pmu_extwake_hib_isr_cb(void *ctx)
{
	si_t *sih = (si_t *)ctx;

	LHL_REG(sih, gpio_int_st_port_adr[0],
		(1 << PCIE_GPIO1_GPIO_PIN) |
			(1 << PCIE_PERST_GPIO_PIN) |
			(1 << PCIE_CLKREQ_GPIO_PIN),
		(1 << PCIE_GPIO1_GPIO_PIN) |
			(1 << PCIE_PERST_GPIO_PIN) |
			(1 << PCIE_CLKREQ_GPIO_PIN));
}

void
BCMATTACHFN(hnd_pmu_init)(si_t *sih)
{
	uint32 pmutimer, startcycles, cycles, rem, alp;
	osl_t *osh = si_osh(sih);
	int i;
	uint32 ticks, ticks_high, ticks_low;
	uint64 ticks64;
	pmu_info_t *pmu_i;

	/* si_pmu_init() is called in si_attach() */

	/* get pmu core reg space addr */
	if (AOB_ENAB(sih)) {
		uint coreidx = si_coreidx(sih);

		hnd_pmur = si_setcore(sih, PMU_CORE_ID, 0);
		ASSERT(hnd_pmur != NULL);

		/* Restore to CC */
		si_setcoreidx(sih, coreidx);
	}
	/* pmu is a subcore in chipc hence use chipc reg space addr */
	else {
		ASSERT(hnd_ccr != NULL);
	}

	pmu_i = hnd_pmu_get_pmu_info(sih);

	pmu_i->sih = sih;

#ifdef USE_LHL_TIMER
	si_lhl_timer_config(sih, osh, LHL_MAC_TIMER);
	si_lhl_timer_config(sih, osh, LHL_ARM_TIMER);
	si_lhl_timer_enable(sih);
#endif /* USE_LHL_TIMER */

	/* Configure ALP period */
	if (hnd_sih->pmurev >= 34) {
		/* Setup 1 MHz and 8 MHz toggle signal for driving the PMU Precision usecTimer
		 * clock input and for use by other cores, including the ARM CA7 Generic Timer
		 * and the protocol timers in the 802.11 MAC core.
		 */
		alp = 0x8012f685;

		/* Enable Precision Usec Timer */
		OR_REG(hnd_osh, &hnd_pmur->PrecisionTmrCtrlStatus, PMU_PREC_USEC_TIMER_ENABLE);
	} else if (hnd_sih->pmurev >= 31) {
		alp = 0x80333330;
	} else {
		/* 0x199 = 16384/40 for using 40MHz crystal */
		alp = 0x10199;
	}
	W_REG(hnd_osh, PMUREGADDR(hnd_sih, hnd_pmur, hnd_ccr, slowclkperiod), alp);

	/* Configure ILP period, 0xcccc = 65536/1.25 for using 1.25MHz crystal */
	W_REG(hnd_osh, PMUREGADDR(hnd_sih, hnd_pmur, hnd_ccr, ILPPeriod), 0x10000);

	/*
	 * Compute the pmu ticks per ms.  This is done by counting a
	 * few PMU timer transitions and using the ARM cyclecounter as
	 * a more accurate clock to measure the PMU tick interval.
	 */

	/* Loop until we see a change */
	pmutimer = R_REG(osh, PMUREGADDR(sih, hnd_pmur, hnd_ccr, pmutimer));
	while (pmutimer == R_REG(osh, PMUREGADDR(sih, hnd_pmur, hnd_ccr, pmutimer))) {
		; /* empty */
	}
	/* There is a clock boundary crosssing so do one more read */
	pmutimer = R_REG(osh, PMUREGADDR(sih, hnd_pmur, hnd_ccr, pmutimer));

	/* The PMU timer just changed - start the cyclecount timer */
	OSL_GETCYCLES(startcycles);

	for (i = 0; i < (1 << PMUTICK_CALC_COUNT_SHIFT); i++) {
		while (pmutimer == R_REG(osh, PMUREGADDR(sih, hnd_pmur, hnd_ccr, pmutimer))) {
			/* empty */
		}
		pmutimer = R_REG(osh, PMUREGADDR(sih, hnd_pmur, hnd_ccr, pmutimer));
	}

	OSL_GETCYCLES(cycles);
	cycles -= startcycles;
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

	/* calculate params to convert tick to usec */
	us_per_pmutick_scale = CLZ(cycles) + PMUTICK_CALC_COUNT_SHIFT;
	us_per_pmutick =  ((cycles << CLZ(cycles)) / c0counts_per_us);
	us_per_pmutick++;		/* Round up */

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

	/* calculate params to convert (1/ILPClk)usec to tick */
	rem = c0counts_per_us << PMUTICK_CALC_COUNT_SHIFT;
	pmutick_per_us = 0;
	pmutick_per_us_scale = 0;
	ASSERT(cycles);
	while ((pmutick_per_us & 0xC0000000) == 0) {
		uint32 partial, lz;
		lz = MIN(CLZ(rem), CLZ(pmutick_per_us) - 1);
		pmutick_per_us <<= lz;
		rem <<= lz;
		pmutick_per_us_scale += lz;

		partial = rem / cycles;
		pmutick_per_us += partial;
		rem -= partial * cycles;
	}
	if (PMUREV(sih->pmurev) >= 1) {
		max_timer_dur = ((1 << 24) - 1);
	} else {
		max_timer_dur = ((1 << 10) - 1);
	}

#ifndef RTE_POLL
	/* Register the timer interrupt handler
	 * AOB chips have PMU (and GCI) interrupts coming from ChipC.
	 */
	si_cc_register_isr(sih, hnd_pmu_cc_isr, hnd_pmu_cc_worklet, CI_PMU, pmu_i);

	/* Enable the PMU interrupt for ResourceReqTimer0 */
	if ((PMUREV(hnd_sih->pmurev) >= 26) && (PMUREV(hnd_sih->pmurev) != 27)) {
		W_REG(hnd_osh, PMUREGADDR(hnd_sih, hnd_pmur, hnd_ccr, pmuintmask0),
		      RSRC_INTR_MASK_TIMER_INT_0);
		(void)R_REG(hnd_osh, PMUREGADDR(hnd_sih, hnd_pmur, hnd_ccr, pmuintmask0));
	}
#endif /* RTE_POLL */

	if (HIB_EXT_WAKEUP_CAP(sih)) {
		hnd_pmu_extwake_register_cb(sih, PMU_EXTWAKE_EVT_HIB,
			(void *)sih, pmu_extwake_hib_isr_cb, NULL);
	}
}

static uint64 cur_ticks = 0;

/* used for microsecond resolution APIs */
static uint64 cur_ticks_us = 0;

static uint64 ms_remainder = 0;

/* accumulate time delta between two PMU timer reads.
 * used in hnd_update_now()
 */
uint32
hnd_pmu_accu_tick(uint32 diff)
{
	uint32 ms, rem_high, rem_low;

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

uint32 hnd_pmu_accu_tick_us(uint32 diff)
{
	uint64 temp;
	uint32 us, rem_high, rem_low;

	cur_ticks_us += diff;

	/* Calculate the us and the remainder */
	temp = (uint64) diff * us_per_pmutick;
	rem_low = (uint32) temp;
	rem_high = temp >> 32;
	us = rem_low >> us_per_pmutick_scale;
	us += rem_high << (32 - us_per_pmutick_scale);

	return us;
}
#ifdef AXI_ACCESS_LIMIT
#define read_pmutimer(osh, cc, timer) \
do { \
    timer =  R_REG(osh, &cc->pmutimer); \
    if (timer != R_REG(osh, &cc->pmutimer)) \
		timer = R_REG(osh, &cc->pmutimer); \
} while (0);

static volatile uint32 hnd_pmutmr_time = 0;

/*
 * The function is to insure that back-to-back ILP write should be at least 3 PMU clock ticks apart
 * to avoid the backplane blocking
 */
static void
slowwrite_pending_check(void)
{
	uint32 timer =  0;
	read_pmutimer(hnd_osh, hnd_ccr, timer);
	/* guarantee process following it should be at least 3 PMU clock ticks apart */
	while ((timer - hnd_pmutmr_time) <= 3u) {
		read_pmutimer(hnd_osh, hnd_ccr, timer);
	}
	hnd_pmutmr_time = timer;
}
#endif /* AXI_ACCESS_LIMIT */

static void
hnd_pmu_set_timer_ex(uint32 ticks)
{
	uint32 req = (PRRT_ALP_REQ | PRRT_HT_REQ | PRRT_HQ_REQ | PRRT_INTEN) << FLAGS_SHIFT;

	/* Don't req HT if we are breaking a large timer to multiple max h/w duration */
	if (ticks > max_timer_dur) {

		ticks = max_timer_dur;
		req &= ~(PRRT_HT_REQ << FLAGS_SHIFT);
	}

	/* Don't request HQ for older revs */
	if (PMUREV(hnd_sih->pmurev) < 15) {
		req &= ~(PRRT_HQ_REQ << FLAGS_SHIFT);
	}
#ifdef AXI_ACCESS_LIMIT
	slowwrite_pending_check();
#endif // endif
	if (ticks == 0) {
#ifdef USE_LHL_TIMER
		ticks = 2;
#else
		/* In case of 0 ticks, request immediate resource.
		 * There is a delay more than 85 usec noticed if the immediate resource req
		 * is not set. It is possible that a timeout request with 0 ticks which
		 * skips the threadx timer could keep updating the PMU res req timer with
		 * 2 ticks. In this case if the immediate resource is not set the PMU timer
		 * interrupt could be delayed
		 */
		req |= (PRRT_IMMEDIATE_RES_REQ << FLAGS_SHIFT);
#endif /* USE_LHL_TIMER */
	}

#ifdef USE_LHL_TIMER
	/* Poll while previous write is pending */
	while (LHL_REG(hnd_sih, lhl_wkup_status_adr, 0, 0) & LHL_WKUP_STATUS_WR_PENDING_ARMTIM0);
	/* Load ARM Timer */
	LHL_REG(hnd_sih, lhl_wl_armtim0_adr, ~0, ticks);
#else
	HND_PMU_SYNC_WR(hnd_sih, hnd_pmur, hnd_ccr, hnd_osh,
		PMUREGADDR(hnd_sih, hnd_pmur, hnd_ccr, res_req_timer),
		req | ticks);
#endif /* USE_LHL_TIMER */
}

void
hnd_pmu_set_timer(uint32 ticks)
{
	OSL_INTERRUPT_SAVE_AREA

	OSL_DISABLE
	hnd_pmu_set_timer_ex(ticks);
	OSL_RESTORE
}

/* convert usec to closest h/w ticks */
uint32
hnd_pmu_us2tick(uint32 us)
{
	uint32 ticks, ticks_high;
	uint64 ticks64;

	ticks64 = (uint64) us * pmutick_per_us;
	ticks_high = (uint32)(ticks64 >> 32);
	ticks = ticks_high >> (pmutick_per_us_scale - 32);

	return ticks;
}

uint32
hnd_pmu_get_tick(void)
{
	uint32 ticks;

	/* PR88659: pmutimer is updated on ILP clock asynchronous to the CPU read.  Its
	 * value may change DURING the read, so the read must be verified and retried (but
	 * not in a loop, in case CPU is running at ILP).
	 */
	if ((hnd_sih == NULL) || (hnd_ccr == NULL))
		return 0;

#ifdef USE_LHL_TIMER
	ticks = LHL_REG(hnd_sih, lhl_hibtim_adr, 0, 0);
	if (ticks != LHL_REG(hnd_sih, lhl_hibtim_adr, 0, 0))
		ticks = LHL_REG(hnd_sih, lhl_hibtim_adr, 0, 0);
#else
	ticks = R_REG(hnd_osh, PMUREGADDR(hnd_sih, hnd_pmur, hnd_ccr, pmutimer));
	if (ticks != R_REG(hnd_osh, PMUREGADDR(hnd_sih, hnd_pmur, hnd_ccr, pmutimer)))
		ticks = R_REG(hnd_osh, PMUREGADDR(hnd_sih, hnd_pmur, hnd_ccr, pmutimer));
#endif /* USE_LHL_TIMER */

	return ticks;
}

void
hnd_pmu_ack_timer(void)
{
#ifdef AXI_ACCESS_LIMIT
	slowwrite_pending_check();
#endif // endif
#ifdef USE_LHL_TIMER
	/* Clear LHL ARM timer interrupt status */
	LHL_REG(hnd_sih, lhl_wl_armtim0_st_adr, LHL_WL_ARMTIM0_ST_WL_ARMTIM_INT_ST,
			LHL_WL_ARMTIM0_ST_WL_ARMTIM_INT_ST);
#else
	/* Needed for 43602, 43430, 4347, 43684 */
	HND_PMU_SYNC_WR(hnd_sih, hnd_pmur, hnd_ccr, hnd_osh,
			PMUREGADDR(hnd_sih, hnd_pmur, hnd_ccr, res_req_timer),
			((PRRT_ALP_REQ | PRRT_HT_REQ | PRRT_HQ_REQ) << FLAGS_SHIFT));

#endif /* USE_LHL_TIMER */
}

#ifndef RTE_POLL
static void
hnd_pmu_clear_interrupt(void)
{
	/* No explicit clearing of the PMU interrupt is needed for newer chips */
	if (PMUREV(hnd_sih->pmurev) < 30) {

		/* Changes from PMU revision 26 are not included in revision 27 */
		if ((PMUREV(hnd_sih->pmurev) >= 26) && (PMUREV(hnd_sih->pmurev) != 27)) {
#ifdef USE_LHL_TIMER
			LHL_REG(hnd_sih, lhl_wl_armtim0_st_adr, LHL_WL_ARMTIM0_ST_WL_ARMTIM_INT_ST,
				LHL_WL_ARMTIM0_ST_WL_ARMTIM_INT_ST);
#else
			W_REG(hnd_osh, PMUREGADDR(hnd_sih, hnd_pmur, hnd_ccr, pmuintstatus),
				RSRC_INTR_MASK_TIMER_INT_0);
			(void)R_REG(hnd_osh, PMUREGADDR(hnd_sih, hnd_pmur, hnd_ccr, pmuintstatus));
#endif /* USE_LHL_TIMER */
		} else {
			W_REG(hnd_osh, PMUREGADDR(hnd_sih, hnd_pmur, hnd_ccr, pmustatus),
				PST_INTPEND);
			(void)R_REG(hnd_osh, PMUREGADDR(hnd_sih, hnd_pmur, hnd_ccr, pmustatus));
		}
	}
}

static void
hnd_pmu_timer_int0_isr(void* cbdata)
{
	hnd_advance_time();
}

static void
hnd_pmu_extwake_req_active_0_isr(void* cbdata)
{
	pmu_info_t *pmu_i = cbdata;
	pmu_extwake_ctx_t *extwk_ctx = pmu_i->pmu_extwake_ctx_list;
	uint32 mask_reg = 0;

	ASSERT(pmu_i->pmu_int_status & PMU_INTR_MASK_EXTWAKE_REQ_ACTIVE_0);

	/* record extwake status */
	mask_reg = R_REG(hnd_osh, PMUREGADDR(hnd_sih, hnd_pmur, hnd_ccr, extwakemask0));
	pmu_i->pmu_extwake_status = R_REG(hnd_osh,
		PMUREGADDR(hnd_sih, hnd_pmur, hnd_ccr, extwakeupstatus));

	pmu_i->pmu_extwake_status = pmu_i->pmu_extwake_status & mask_reg;

	/* event specific isr handling */
	while (extwk_ctx) {

		if (pmu_i->pmu_extwake_status
			& PMU_EXTWAKE_EVT_MASK(extwk_ctx->event)) {

			/* clear status */
			W_REG(hnd_osh, PMUREGADDR(hnd_sih, hnd_pmur, hnd_ccr, extwakeupstatus),
				(PMU_EXTWAKE_EVT_MASK(extwk_ctx->event)));

			/* disable extwake event */
			AND_REG(hnd_osh, PMUREGADDR(hnd_sih, hnd_pmur, hnd_ccr, extwakemask0),
				~(PMU_EXTWAKE_EVT_MASK(extwk_ctx->event)));

			if (extwk_ctx->event_isr_cb) {
				extwk_ctx->event_isr_cb(extwk_ctx->ctx);
			}
		}

		extwk_ctx = extwk_ctx->next;
	}
}

#ifdef BCMPMU_STATS
static void
hnd_pmu_statstimer_int_isr(void* cbdata)
{
	if (PMUREV(hnd_sih->pmurev) >= 33) {
		si_pmustatstimer_int_disable(hnd_sih);
	}
}
#endif /* BCMPMU_STATS */

static bool
hnd_pmu_cc_isr(void* cbdata, uint index, uint32 intstatus)
{
	if (PMUREV(hnd_sih->pmurev) >= 26) {
		pmu_info_t *pmu_i = cbdata;
		uint32 pmuintstatus;

		pmuintstatus = R_REG(hnd_osh, PMUREGADDR(hnd_sih, hnd_pmur, hnd_ccr, pmuintstatus));

		pmu_i->pmu_int_status |= pmuintstatus;

		if (pmuintstatus & RSRC_INTR_MASK_TIMER_INT_0) {
			hnd_pmu_timer_int0_isr(cbdata);
		}

		if (PMUREV(hnd_sih->pmurev) > 30 &&
			pmuintstatus & PMU_INTR_MASK_EXTWAKE_REQ_ACTIVE_0) {
			hnd_pmu_extwake_req_active_0_isr(cbdata);
		}

#ifdef BCMPMU_STATS
		if (PMU_STATS_ENAB() &&
			pmuintstatus & PMU_INT_STAT_TIMER_INT_MASK) {
			hnd_pmu_statstimer_int_isr(cbdata);
		}
#endif /* BCMPMU_STATS */
	} else {
		/* Handle interrupts in DPC context after the ISR completes */
	}

	/* Request worklet */
	return TRUE;
}

static void
hnd_pmu_timer_int0_worklet(void* cbdata)
{
	hnd_pmu_clear_interrupt();
	hnd_pmu_ack_timer();
	hnd_schedule_timer();
}

static void
hnd_pmu_extwake_req_active_0_worklet(void* cbdata)
{
	pmu_info_t *pmu_i = cbdata;
	uint32 processed_events = 0;
	pmu_extwake_ctx_t *extwk_ctx = pmu_i->pmu_extwake_ctx_list;

	/* event specific worklet handling */
	while (extwk_ctx) {

		if (pmu_i->pmu_extwake_status
			& PMU_EXTWAKE_EVT_MASK(extwk_ctx->event)) {
			if (extwk_ctx->event_worklet_cb) {
				extwk_ctx->event_worklet_cb(extwk_ctx->ctx);
				processed_events |= PMU_EXTWAKE_EVT_MASK(extwk_ctx->event);
			}
		}
		pmu_i->pmu_extwake_status = pmu_i->pmu_extwake_status &
			~(PMU_EXTWAKE_EVT_MASK(extwk_ctx->event));

		extwk_ctx = extwk_ctx->next;
	}

	if (processed_events) {
		/* enable extwake event */
		OR_REG(hnd_osh, PMUREGADDR(hnd_sih, hnd_pmur, hnd_ccr, extwakemask0),
			processed_events);
	}
}

#ifdef BCMPMU_STATS
static void
hnd_pmu_statstimer_int_worklet(void* cbdata)
{
	if (PMUREV(hnd_sih->pmurev) >= 33) {
		si_pmustatstimer_clear_overflow(hnd_sih);
		si_pmustatstimer_int_enable(hnd_sih);
	}
}
#endif /* BCMPMU_STATS */

static bool
hnd_pmu_cc_worklet(void* cbdata)
{
	pmu_info_t *pmu_i = cbdata;

	if (PMUREV(hnd_sih->pmurev) >= 26) {

		if (pmu_i->pmu_int_status & RSRC_INTR_MASK_TIMER_INT_0) {
			hnd_pmu_timer_int0_worklet(cbdata);
		}

		if (pmu_i->pmu_int_status & PMU_INTR_MASK_EXTWAKE_REQ_ACTIVE_0) {
			hnd_pmu_extwake_req_active_0_worklet(cbdata);
		}

#ifdef BCMPMU_STATS
		if (PMU_STATS_ENAB() &&
			pmu_i->pmu_int_status & PMU_INT_STAT_TIMER_INT_MASK) {
			hnd_pmu_statstimer_int_worklet(cbdata);
		}
#endif /* BCMPMU_STATS */
	} else {
		hnd_pmu_clear_interrupt();
		hnd_pmu_ack_timer();
		hnd_schedule_timer();
		hnd_advance_time();
	}

	pmu_i->pmu_int_status = 0;

	/* Don't reschedule */
	return FALSE;
}
#endif	/* !RTE_POLL */

pmu_extwake_ctx_t *
hnd_pmu_extwake_register_cb(si_t *sih,
	pmu_extwake_event_t event, void *event_ctx,
	pmu_extwake_event_isr_cb_t isr_cb, pmu_extwake_event_worklet_cb_t worklet_cb)
{
	osl_t *osh = si_osh(sih);
	pmu_info_t *pmu_i = hnd_pmu_get_pmu_info(sih);
	pmu_extwake_ctx_t *extwk_item = NULL;
	pmu_extwake_ctx_t *last_item = NULL;

	if (!pmu_i) {
		ASSERT(pmu_i);
		return NULL;
	}

	extwk_item = pmu_i->pmu_extwake_ctx_list;

	while (extwk_item) {

		/* avoid duplicate registration */
		if (extwk_item->event == event) {
			goto done;
		}

		last_item = extwk_item;
		extwk_item = extwk_item->next;
	}
	ASSERT(!extwk_item);

	extwk_item = (pmu_extwake_ctx_t *)MALLOCZ(osh, sizeof(pmu_extwake_ctx_t));
	if (!extwk_item) {
		PMU_MSG(("PMU: %s memory allocation failed \n", __FUNCTION__));
		goto done;
	}

	extwk_item->event = event;
	extwk_item->ctx = event_ctx;
	extwk_item->event_isr_cb = isr_cb;
	extwk_item->event_worklet_cb = worklet_cb;
	extwk_item->next = NULL;

	if (!pmu_i->pmu_extwake_ctx_list) {
		pmu_i->pmu_extwake_ctx_list = extwk_item;
	} else {
		last_item->next = extwk_item;
	}

	OR_REG(hnd_osh, PMUREGADDR(hnd_sih, hnd_pmur, hnd_ccr, extwakemask0),
	      PMU_EXTWAKE_EVT_MASK(event));

	/* program required clks while waking up */
	W_REG(hnd_osh, PMUREGADDR(hnd_sih, hnd_pmur, hnd_ccr, pmuintctrl0),
		(PMU_INTC_HT_REQ | PMU_INTC_HQ_REQ));
	W_REG(hnd_osh, PMUREGADDR(hnd_sih, hnd_pmur, hnd_ccr, extwakectrl[0]),
		(PMU_INTC_HT_REQ | PMU_INTC_HQ_REQ));

	OR_REG(hnd_osh, PMUREGADDR(hnd_sih, hnd_pmur, hnd_ccr, pmuintmask0),
	      PMU_INTR_MASK_EXTWAKE_REQ_ACTIVE_0);

done:
	return extwk_item;
}

void
hnd_pmu_extwake_unregister_cb(si_t *sih, pmu_extwake_ctx_t *extwk_ctx)
{
	osl_t *osh = si_osh(sih);
	pmu_info_t *pmu_i = hnd_pmu_get_pmu_info(sih);
	pmu_extwake_ctx_t **extwk_item = NULL;

	if (!pmu_i->pmu_extwake_ctx_list) {
		return;
	}

	extwk_item = &pmu_i->pmu_extwake_ctx_list;

	while (*extwk_item != extwk_ctx) {
		extwk_item = &(*extwk_item)->next;
		if (!(*extwk_item))
			return;
	}
	*extwk_item = extwk_ctx->next;

	MFREE(osh, extwk_ctx, sizeof(pmu_extwake_ctx_t));
}

/* ========================== misc =========================== */
#if  defined(WLSRVSDB)
uint32 hnd_clk_count(void);
uint32
hnd_clk_count(void)
{
	return hnd_pmu_get_tick();
}
#endif /* WLSRVSDB */
