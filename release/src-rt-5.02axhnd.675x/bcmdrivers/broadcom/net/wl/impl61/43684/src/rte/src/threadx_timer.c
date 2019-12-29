/** \file threadx.c
 *
 * Initialization and support routines for threadX.
 *
 * Copyright 2019 Broadcom
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
 * $Id: threadx_timer.c 774076 2019-04-09 12:13:18Z $
 */

/* ThreadX timer wrapper and RTE timer APIs implementation */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <osl_ext.h>
#include <bcmutils.h>
#include <rte_isr.h>
#include <rte.h>
#include "rte_priv.h"
#include <rte_cons.h>
#include <rte_timer.h>

#include <tx_api.h>
#include <tx_timer.h>

#include <threadx_low_power.h>
#include "threadx_low_power_priv.h"

#include <bcm_buzzz.h>

#ifdef THREADX_STAT
/* Statistics */
typedef enum {
	THREADX_STAT_ERROR,			/* threadx error */
	THREADX_STAT_TIMER_STARTED,		/* timer started */
	THREADX_STAT_TIMER_STOPPED,		/* timer stopped */
	THREADX_STAT_TIMER_EXPIRED,		/* timer expired */
	THREADX_STAT_LAST
} threadx_stat_t;

static uint32 threadx_stat[THREADX_STAT_LAST];

/* Increment specified stat */
#define THREADX_STAT_UPDATE(stat)	threadx_stat[stat]++
#else /* THREADX_STAT */
#define THREADX_STAT_UPDATE(stat)
#endif	/* THREADX_STAT */

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
	hnd_dpc_t		*dpc;		/* Callback function DPC */
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
	ASSERT(timer->dpc == NULL);
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
 * Handle the DPC for the specified timer.
 *
 * This function will be called in the context of the DPC target thread.
 *
 * @param arg		Pointer to timer.
 */

static void
handle_dpc_for_timer(void *arg)
{
	hnd_timer_t *timer = (hnd_timer_t*)arg;

	ASSERT(timer != NULL);
	ASSERT(timer->mainfn != NULL);
	ASSERT(timer->dpc != NULL);
	ASSERT(timer->dpc->deleted == FALSE);
	ASSERT(timer->dpc->cbdata == timer);
	ASSERT(hnd_in_isr() == FALSE);

	THREADX_STAT_UPDATE(THREADX_STAT_TIMER_EXPIRED);

	if (timer->deleted == FALSE) {
		HND_CPUUTIL_NOTIFY(HND_CPUUTIL_TRANS_TMR_ENT, timer->cpuutil);
		HND_INTSTATS_NOTIFY(NOTIFICATION_TYPE_TMR_ENTER, (void*)timer->dpc->stats);
		BUZZZ_LVL3(HND_TMR_ENT, 1, (uint32)timer->mainfn);

		/* Invoke timer callback */
		(timer->mainfn)(timer);

		BUZZZ_LVL3(HND_TMR_RTN, 0);
		HND_INTSTATS_NOTIFY(NOTIFICATION_TYPE_TMR_EXIT, (void*)timer->dpc->stats);
		HND_CPUUTIL_NOTIFY(HND_CPUUTIL_TRANS_TMR_EXT, timer->cpuutil);
	}

	if (timer->deleted == TRUE && timer->dpc->state == DPC_NOT_SCHEDULED) {
		/* The timer was marked for deletion instead of being freed right away because
		 * the associated timer DPC was still in use. We just completed executing the
		 * timer DPC and it is currently not enqueued, so free the timer and DPC now.
		 */
		hnd_dpc_free_internal(timer->dpc);
		timer->dpc = NULL;

		hnd_timer_free_internal(timer);
	}
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

	if (timer->dpc == NULL || timer->deleted == TRUE) {
		return;
	}

	hnd_dpc_schedule_internal(timer->dpc);

	if (timer->periodic) {

		/* Periodic timers are started as single-shot so we are able to
		 * re-program the hardware timer on expiration
		 */

		ASSERT(timer->usec != 0);

		if (osl_ext_timer_start_us(&timer->timer, timer->usec, OSL_EXT_TIMER_MODE_ONCE)
		    != OSL_EXT_SUCCESS) {
			THREADX_STAT_UPDATE(THREADX_STAT_ERROR);
			ASSERT(FALSE);
			return;
		}

		/* Determine next timer expiry and update the hardware timer */
		threadx_schedule_timer();

		THREADX_STAT_UPDATE(THREADX_STAT_TIMER_STARTED);
	}
}

/**
 * Create a timer.
 *
 * This will also register a DPC for execution of mainfn in the context
 * of the specified thread when the timer elapses.
 *
 * @param context
 * @param data		Pointer to DPC target function data.
 * @param mainfn	Pointer to DPC target function.
 * @param auxfn		Pointer to aux target function.
 * @param thread	Pointer to DPC target thread. Can be NULL to use the
 *			current thread as the target.
 * @param id		Timer id.
 * @return		Pointer to timer, or NULL on error.
 *
 * @note The specified DPC target thread must have been initialized for handling
 *	 events (@see hnd_thread_init_event_handling)
 */

hnd_timer_t *
hnd_timer_create_internal(void *context, void *data,
	hnd_timer_mainfn_t mainfn, hnd_timer_auxfn_t auxfn, osl_ext_task_t* thread, const char *id)
{
	hnd_timer_t *timer;

	ASSERT(mainfn != NULL);

	BUZZZ_LVL4(HND_TMR_CRT, 1, (uint32)CALL_SITE);

	timer = (hnd_timer_t *)MALLOCZ_NOPERSIST(hnd_osh, sizeof(hnd_timer_t));
	if (timer == NULL) {
		return NULL;
	}

	timer->context = context;
	timer->data    = data;
	timer->mainfn  = mainfn;
	timer->auxfn   = auxfn;

	timer->cpuutil = /* Bind Timer execution context to CPU Util context */
		HND_CPUUTIL_CONTEXT_GET(HND_CPUUTIL_CONTEXT_TMR, (uint32)mainfn);

	/* Register a DPC for handling the timer callback */
	timer->dpc = hnd_dpc_create_internal(handle_dpc_for_timer, timer, thread, id);
	if (timer->dpc != NULL) {
#ifdef HND_INTSTATS
		/* Update DPC stats identifiers */
		timer->dpc->stats->target = (void*)(auxfn ? (uint32)auxfn : (uint32)mainfn);
		timer->dpc->stats->type   = OBJ_TYPE_TDPC;
#endif /* HND_INTSTATS */

		if (osl_ext_timer_create((char*)(uintptr)id,	/* Not const in OSL */
		    OSL_EXT_TIME_FOREVER, OSL_EXT_TIMER_MODE_ONCE,
		    threadx_timer_callback, timer, &timer->timer) == OSL_EXT_SUCCESS) {
			return timer;
		}
		hnd_dpc_free_internal(timer->dpc);
		timer->dpc = NULL;
	}

	THREADX_STAT_UPDATE(THREADX_STAT_ERROR);
	hnd_timer_free_internal(timer);
	return NULL;
}

/**
 * Free a timer and assocated timer DPC.
 *
 * @param timer		Pointer to timer
 *
 * @note If the timer was running at the time of this call, execution of the associated
 *	 callback function could already have begun and will run to completion.
 *	 @see hnd_timer_stop.
 * @note For short-lived timers, consider reuse instead.
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

		if (osl_ext_timer_delete(&timer->timer) != OSL_EXT_SUCCESS) {
			THREADX_STAT_UPDATE(THREADX_STAT_ERROR);
		}

		if (timer->dpc->state == DPC_NOT_SCHEDULED && timer->dpc->executing == FALSE) {
			/* Timer DPC is not in use, free the timer and DPC now. */
			hnd_dpc_free_internal(timer->dpc);
			timer->dpc = NULL;

			hnd_timer_free_internal(timer);
		} else {
			/* Timer DPC is in use. Mark the timer as deleted and schedule the DPC to
			 * have the target thread do the cleanup of both the timer and DPC.
			 */
			if (hnd_dpc_schedule_internal(timer->dpc) == FALSE) {
				/* Scheduling failed, which should not happen when a properly
				 * dimensioned event queue is used.
				 */
				ASSERT(FALSE);
			}
		}
	}

	TX_RESTORE
}

/**
 * Start a timer.
 *
 * @param timer		Pointer to timer
 * @param ms		Timer interval in MILLISECONDS, can be 0.
 * @param periodic	TRUE if this is a period timer.
 * @return		TRUE on success.
 *
 * @note When using a timer only with 0 ms timeouts to defer work, consider registering
 *	 and scheduling a dedicated DPC (@see hnd_dpc_create and @see hnd_dpc_schedule)
 *	 instead, as this doesn't incur the overhead of a ThreadX timer.
 * @note If the timer was already running at the time of this call, execution of the
 *	 associated callback function could already have begun and will run to completion.
 *	 @see hnd_timer_stop.
 */

bool
hnd_timer_start(hnd_timer_t *timer, osl_ext_time_ms_t ms, bool periodic)
{
	return hnd_timer_start_us(timer, ms * 1000, periodic);
}

/**
 * Start a timer.
 *
 * @param timer		Pointer to timer
 * @param us		Timer interval in MICROSECONDS, can be 0.
 * @param periodic	TRUE if this is a period timer.
 * @return		TRUE on success.
 *
 * @note When using a timer only with 0 ms timeouts to defer work, consider registering
 *	 and scheduling a dedicated DPC (@see hnd_dpc_create and @see hnd_dpc_schedule)
 *	 instead, as this doesn't incur the overhead of a ThreadX timer.
 * @note If the timer was already running at the time of this call, execution of the
 *	 associated callback function could already have begun and will run to completion.
 *	 @see hnd_timer_stop.
 */

bool
hnd_timer_start_us(hnd_timer_t *timer, osl_ext_time_us_t us, bool periodic)
{
	ASSERT(timer != NULL);
	ASSERT(timer->deleted == FALSE);
	ASSERT(timer->dpc != NULL && timer->dpc->deleted == FALSE);

	if (timer == NULL || timer->deleted == TRUE) {
		return FALSE;
	}

	BUZZZ_LVL5(HND_TMR_BGN, 1, (uint32)CALL_SITE);

	timer->usec = us;
	timer->periodic = periodic;

	/* Timer may be active */
	hnd_timer_stop(timer);

	if (us == 0) {
		ASSERT(periodic == FALSE);

		if (hnd_dpc_schedule(timer->dpc) == FALSE) {
			/* The DPC could not be scheduled immediately either because the target
			 * thread event queue is full or system initialization is still in
			 * progress. Retry as soon as interrupts are enabled at the end of
			 * system initialization, or on the next tick.
			 */
			us++;
		}
	}

	if (us != 0) {
		/* Periodic timers are started as single-shot so we are able to
		 * re-program the hardware timer on expiration
		 */
		if (osl_ext_timer_start_us(&timer->timer, us, OSL_EXT_TIMER_MODE_ONCE)
		    != OSL_EXT_SUCCESS) {
			THREADX_STAT_UPDATE(THREADX_STAT_ERROR);
			return FALSE;
		}

		/* Determine next timer expiry and update the hardware timer */
		threadx_schedule_timer();
	}

	THREADX_STAT_UPDATE(THREADX_STAT_TIMER_STARTED);
	return TRUE;
}

/**
 * Stop a timer.
 *
 * @param timer		Pointer to timer
 * @return		TRUE if the timer callback function was not being executed at
 *			the time of this call, or the timer hasn't elapsed yet.
 *
 * @note If the timer has elapsed just prior to stopping it, execution of the associated
 *	 callback function could already have begun and will run to completion. Callback
 *	 functions and functions manipulating the associated timer should be able to handle
 *	 this situation.
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

	if (osl_ext_timer_stop(&timer->timer) != OSL_EXT_SUCCESS) {
		THREADX_STAT_UPDATE(THREADX_STAT_ERROR);
	}

	THREADX_STAT_UPDATE(THREADX_STAT_TIMER_STOPPED);

	return hnd_dpc_cancel(timer->dpc);
}

int
BCMATTACHFN(hnd_timer_init)(si_t *sih)
{
	/* initialize ticks */
	hnd_update_now_us();

	/* initialize low power mode */
	threadx_low_power_init();

	return BCME_OK;
}

void *
hnd_timer_get_ctx(hnd_timer_t *timer)
{
	return timer->context;
}

void *
hnd_timer_get_data(hnd_timer_t *timer)
{
	return timer->data;
}

hnd_timer_auxfn_t
hnd_timer_get_auxfn(hnd_timer_t *timer)
{
	return timer->auxfn;
}

/** Cancel the h/w timer if it is already armed and ignore any further h/w timer requests */
void
hnd_suspend_timer(void)
{
	hnd_ack_irq_timer();
}

/** Resume the timer activities */
void
hnd_resume_timer(void)
{
	hnd_set_irq_timer(0);
}

#ifdef RTE_DBG_TIMER
static void
threadx_print_timers(void *arg, int argc, char *argv[])
{
	TX_TIMER *this = _tx_timer_created_ptr;
	ULONG i = 0;

	if (this == NULL) {
		printf("No timers\n");
		return;
	}

	while (i < _tx_timer_created_count) {
		/* assuming TX_TIMER is the first member of the struct */
		hnd_timer_t *t = (hnd_timer_t *)this;
		ULONG remaining_ticks;
		ULONG reschedule_ticks;

		tx_timer_info_get(this, NULL, NULL, &remaining_ticks,
		                  &reschedule_ticks, &this);

		printf("timer %p left %lu resched %lu "
		       "(main %p, aux %p, ctx %p, data %p, %d us, prd %d, exp %d)\n",
		       t, remaining_ticks, reschedule_ticks,
		       t->mainfn, t->auxfn, t->context, t->data, t->usec,
		       t->periodic, t->is_queued);
		i ++;
	}
}
#endif /* RTE_DBG_TIMER */

/* Must be called after hnd_cons_init() */
void
BCMATTACHFN(hnd_timer_cli_init)(void)
{
#if defined(RTE_CONS) && !defined(BCM_BOOTLOADER)
#ifdef RTE_DBG_TIMER
	if (!hnd_cons_add_cmd("tim", threadx_print_timers, 0))
		return;
#endif // endif
#endif /* RTE_CONS  && ! BCM_BOOTLOADER */
}
