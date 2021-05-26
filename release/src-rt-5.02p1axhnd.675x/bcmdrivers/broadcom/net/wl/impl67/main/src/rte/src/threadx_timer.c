/** \file threadx.c
 *
 * Initialization and support routines for threadX.
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
 * $Id: threadx_timer.c 787482 2020-06-01 09:20:40Z $
 */

/* ThreadX timer wrapper and RTE timer APIs implementation */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <osl_ext.h>
#include <bcmutils.h>
#include <rte.h>
#include "rte_priv.h"
#include <rte_isr.h>
#include "rte_isr_priv.h"
#include <rte_isr_stats.h>
#include "rte_isr_stats_priv.h"
#include <rte_cons.h>
#include <rte_timer.h>

#include <tx_api.h>
#include <tx_timer.h>

#include <threadx_low_power.h>
#include "threadx_low_power_priv.h"
#include <rte_scheduler.h>
#include "rte_scheduler_priv.h"

#include <bcm_buzzz.h>

/* Software timer context */
struct hnd_timer
{
	osl_ext_timer_t		timer;		/* ThreadX timer */
	uint32			*context;	/* User supplied params */
	void			*data;
	hnd_timer_mainfn_t	mainfn;
	hnd_timer_auxfn_t	auxfn;
	osl_ext_time_us_t	usec;		/* Timer interval */
	bool			periodic;	/* Automatic reschedule flag */
	bool			deleted;	/* Timer deleted flag */
	hnd_worklet_t		*worklet;	/* Callback function worklet */
	void			*cpuutil;	/* CPU Util context */
};

/**
 * Delete the specified timer, internal function that does not check
 * deleted state of the timer.
 *
 * @param timer		Pointer to timer.
 */

static void
hnd_timer_free_internal(hnd_timer_t *timer)
{
	ASSERT(timer != NULL);
	ASSERT(hnd_in_isr() == FALSE);

	timer->cpuutil = /* Unbind Timer from CPU Util context */
		HND_CPUUTIL_CONTEXT_PUT(HND_CPUUTIL_CONTEXT_TMR, timer->cpuutil);

#ifdef BCMDBG
	/* Detect use after free */
	memset(timer, 0, sizeof(hnd_timer_t));
#endif /* BCMDBG */

	MFREE(hnd_osh, timer, sizeof(hnd_timer_t));
}

/**
 * Handle the worklet for the specified timer.
 *
 * This function will be called in the context of the worklet target thread.
 *
 * @param arg		Pointer to timer.
 * @return		TRUE to reschedule.
 */

static bool
handle_worklet_for_timer(void *arg)
{
	hnd_timer_t *timer = (hnd_timer_t*)arg;

	ASSERT(timer != NULL);
	ASSERT(timer->mainfn != NULL);
	ASSERT(hnd_worklet_is_valid(timer->worklet));
	ASSERT(hnd_in_isr() == FALSE);

	if (timer->deleted == FALSE) {
		HND_CPUUTIL_NOTIFY(HND_CPUUTIL_TRANS_TMR_ENT, timer->cpuutil);
		HND_INTSTATS_NOTIFY(NOTIFICATION_TYPE_TMR_ENTER, (void*)timer->worklet->stats);
		BUZZZ_LVL3(HND_TMR_ENT, 1, (uint32)timer->mainfn);

		/* Invoke timer callback */
		(timer->mainfn)(timer);

		BUZZZ_LVL3(HND_TMR_RTN, 0);
		HND_INTSTATS_NOTIFY(NOTIFICATION_TYPE_TMR_EXIT, (void*)timer->worklet->stats);
		HND_CPUUTIL_NOTIFY(HND_CPUUTIL_TRANS_TMR_EXT, timer->cpuutil);
	}

	if (timer->deleted == TRUE) {
		/* Timer worklet is not in use, free the timer and worklet now. */
		hnd_worklet_delete(timer->worklet);
		hnd_timer_free_internal(timer);
	}

	return FALSE;
}

/**
 * Timer callback function.
 *
 * This function will be called in interrupt context
 *
 * @param arg		Pointer to timer.
 *
 * @note ThreadX port assumes TX_TIMER_PROCESS_IN_ISR is defined.
 */

static void
threadx_timer_callback(osl_ext_timer_arg_t arg)
{
	hnd_timer_t *timer = (hnd_timer_t*)arg;

	ASSERT(hnd_in_isr());

	if (timer->worklet == NULL || timer->deleted == TRUE) {
		return;
	}

	hnd_worklet_schedule(timer->worklet);

	if (timer->periodic) {

		/* Periodic timers are started as single-shot so we are able to
		 * re-program the hardware timer on expiration
		 */

		ASSERT(timer->usec != 0);

		if (osl_ext_timer_start_us(&timer->timer, timer->usec, OSL_EXT_TIMER_MODE_ONCE)
			!= OSL_EXT_SUCCESS) {
				ASSERT(FALSE);
				return;
		}

		/* Determine next timer expiry and update the hardware timer */
		threadx_schedule_timer();
	}
}

/**
 * Create a timer.
 *
 * This will also create a worklet for execution of mainfn in the context
 * of the specified thread when the timer elapses.
 *
 * A scheduler must have been created and associated to the specified target thread,
 * @see hnd_scheduler_create, @see hnd_thread_set_scheduler.
 *
 * @param context
 * @param data		Pointer to target function data.
 * @param mainfn	Pointer to main target function.
 * @param auxfn		Pointer to aux target function.
 * @param thread	Pointer to target thread. Can be NULL to use the
 *			current thread as the target.
 * @param id		Timer id.
 * @return		Pointer to timer, or NULL on error.
 */

hnd_timer_t *
hnd_timer_create_internal(void *context, void *data,
	hnd_timer_mainfn_t mainfn, hnd_timer_auxfn_t auxfn, osl_ext_task_t* thread, const char *id)
{
	hnd_timer_t *timer;

	ASSERT(mainfn != NULL);

	BUZZZ_LVL4(HND_TMR_CRT, 1, (uint32)CALL_SITE);

	timer = (hnd_timer_t*)MALLOCZ_NOPERSIST(hnd_osh, sizeof(hnd_timer_t));
	if (timer != NULL) {
		timer->context = context;
		timer->data    = data;
		timer->mainfn  = mainfn;
		timer->auxfn   = auxfn;

		timer->cpuutil = /* Bind Timer execution context to CPU Util context */
			HND_CPUUTIL_CONTEXT_GET(HND_CPUUTIL_CONTEXT_TMR, (uint32)mainfn);

		/* Create a worklet for handling the timer callback */
		timer->worklet = hnd_worklet_create_internal(
			handle_worklet_for_timer, timer,
			SCHEDULER_DEFAULT_TIMER_PRIORITY,
			SCHEDULER_DEFAULT_TIMER_QUOTA, thread, id);
		if (timer->worklet != NULL) {
#ifdef HND_INTSTATS
			/* Update worklet stats identifiers */
			timer->worklet->stats->target = auxfn ? (void*)auxfn : (void*)mainfn;
			timer->worklet->stats->type   = OBJ_TYPE_TMR_WORKLET;
#endif /* HND_INTSTATS */

			if (osl_ext_timer_create((char*)(uintptr)id,	/* Not const in OSL */
				OSL_EXT_TIME_FOREVER, OSL_EXT_TIMER_MODE_ONCE,
				threadx_timer_callback, timer, &timer->timer) == OSL_EXT_SUCCESS) {
					return timer;
			}

			hnd_worklet_delete(timer->worklet);
		}

		hnd_timer_free_internal(timer);
	}

	return NULL;
}

/**
 * Free a timer.
 *
 * @note If the callback function associated to the timer is being executed at the time of the
 *	 call, it will run to completion. The timer callback function and any functions that
 *	 manipulate the timer should be able to handle this situation.
 * @note For short-lived timers, consider reusing the timer instead.
 *
 * @param timer		Pointer to timer
 */

void
hnd_timer_free(hnd_timer_t *timer)
{
	OSL_INTERRUPT_SAVE_AREA

	ASSERT(timer != NULL);

	BUZZZ_LVL4(HND_TMR_DEL, 1, (uint32)CALL_SITE);

	TX_DISABLE

	if (timer != NULL && timer->deleted == FALSE) {
		timer->deleted = TRUE;

		osl_ext_timer_delete(&timer->timer);

		if (hnd_worklet_is_pending_or_executing(timer->worklet) == FALSE) {
			/* Timer worklet is not in use, free the timer and worklet now. */
			hnd_worklet_delete(timer->worklet);
			hnd_timer_free_internal(timer);
		} else {
			/* Timer worklet is in use. Mark the timer as deleted and schedule the
			 * worklet to have the target thread do the cleanup of the
			 * timer and worklet.
			 */
			hnd_worklet_schedule(timer->worklet);
		}
	}

	TX_RESTORE
}

/**
 * Start a timer.
 *
 * @note When using timers with 0 millisecond timeouts just for deferring work, consider
 *	 creating and scheduling a dedicated worklet (@see hnd_worklet_create,
 *	 @see hnd_worklet_schedule) instead, as this doesn't incur the overhead of a timer.
 * @note If the callback function associated to the timer is being executed at the time of the
 *	 call, it will run to completion. The timer callback function and any functions that
 *	 manipulate the timer should be able to handle this situation.
 *
 * @param timer		Pointer to timer
 * @param ms		Timer interval in milliseconds, can be 0.
 * @param periodic	TRUE if this is a periodic timer.
 * @return		TRUE on success.
 */

bool
hnd_timer_start(hnd_timer_t *timer, osl_ext_time_ms_t ms, bool periodic)
{
	return hnd_timer_start_us(timer, ms * 1000, periodic);
}

/**
 * Start a timer, timeout specified in microseconds.
 *
 * @note When using timers with 0 microsecond timeouts just for deferring work, consider
 *	 creating and scheduling a dedicated worklet (@see hnd_worklet_create,
 *	 @see hnd_worklet_schedule) instead, as this doesn't incur the overhead of a timer.
 * @note If the callback function associated to the timer is being executed at the time of the
 *	 call, it will run to completion. The timer callback function and any functions that
 *	 manipulate the timer should be able to handle this situation.
 *
 * @param timer		Pointer to timer
 * @param ms		Timer interval in microseconds, can be 0.
 * @param periodic	TRUE if this is a periodic timer.
 * @return		TRUE on success.
 */

bool
hnd_timer_start_us(hnd_timer_t *timer, osl_ext_time_us_t us, bool periodic)
{
	ASSERT(timer != NULL);
	ASSERT(timer->deleted == FALSE);

	if (timer == NULL || timer->deleted == TRUE) {
		return FALSE;
	}

	BUZZZ_LVL5(HND_TMR_BGN, 1, (uint32)CALL_SITE);

	timer->usec = us;
	timer->periodic = periodic;

	/* Timer may be active */
	hnd_timer_stop(timer);

	/* Shortcut for 0ms timers */
	if (us == 0) {
		ASSERT(periodic == FALSE);

		hnd_worklet_schedule(timer->worklet);
		return TRUE;
	}

	/* Periodic timers are started as single-shot so we are able to
	 * re-program the hardware timer on expiration
	 */
	if (osl_ext_timer_start_us(&timer->timer, us, OSL_EXT_TIMER_MODE_ONCE) == OSL_EXT_SUCCESS) {
		/* Determine next timer expiry and update the hardware timer */
		threadx_schedule_timer();
		return TRUE;
	}

	return FALSE;
}

/**
 * Stop a timer.
 *
 * @note If the callback function associated to the timer is being executed at the time of the
 *	 call, it will run to completion. The timer callback function and any functions that
 *	 manipulate the timer should be able to handle this situation.
 *
 * @param timer		Pointer to timer
 * @return		TRUE if the timer callback function was not being executed at
 *			the time of this call.
 */

bool
hnd_timer_stop(hnd_timer_t *timer)
{
	BUZZZ_LVL5(HND_TMR_END, 1, (uint32)CALL_SITE);

	ASSERT(timer != NULL);
	ASSERT(timer->deleted == FALSE);

	if (timer == NULL || timer->deleted == TRUE) {
		return TRUE;
	}

	osl_ext_timer_stop(&timer->timer);

	return hnd_worklet_cancel(timer->worklet);
}

int
BCMATTACHFN(hnd_timer_init)(si_t *sih)
{
	/* Initialize ticks */
	hnd_update_now_us();

	/* Initialize low power mode */
	threadx_low_power_init();

	return BCME_OK;
}

INLINE void *
hnd_timer_get_ctx(hnd_timer_t *timer)
{
	return timer->context;
}

INLINE void *
hnd_timer_get_data(hnd_timer_t *timer)
{
	return timer->data;
}

INLINE hnd_timer_auxfn_t
hnd_timer_get_auxfn(hnd_timer_t *timer)
{
	return timer->auxfn;
}

/** Cancel the h/w timer if it is already armed and ignore any further h/w timer requests */
INLINE void
hnd_suspend_timer(void)
{
	hnd_ack_irq_timer();
}

/** Resume the timer activities */
INLINE void
hnd_resume_timer(void)
{
	hnd_set_irq_timer(0);
}

void
BCMATTACHFN(hnd_timer_cli_init)(void)
{
}
