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
 * $Id: threadx_low_power.c 787983 2020-06-17 17:59:06Z $
 */

/* These functions are implementations for the templates provided by threadX in tx_low_power.c.
 * The functions have been renamed to avoid symbol conflicts and are implemented in this source
 * file to avoid modifying the threadX source.
 *
 * Modifications have been made to this module to suit the Broadcom device which uses a hardware
 * timer to schedule an interrupt when the next software timer expires. This interrupt will also
 * wake the processor from WFI during low power mode.
 *
 * To avoid a race condition, hardware timer programming will happen directly in the PMU ISR and
 * when a software timer is started. The ISR will also advance the ThreadX time and invoke
 * _tx_timer_interrupt() to run the callbacks of any expired software timers.
 *
 * threadx_low_power_enter() will not do any hardware timer programming as we assume this has
 * already been done at that point. It takes any actions needed to bring the device in low power
 * mode. These actions will be undone in threadx_low_power_exit().
 */

#include <osl_ext.h>

#include <tx_api.h>
#include <tx_low_power.h>

#include <rte.h>
#include "rte_priv.h"
#include <rte_isr.h>

#include <threadx_low_power.h>
#include "threadx_low_power_priv.h"

#include <sbchipc.h>
#include <saverestore.h>

#ifndef THREADX_TIME_ADVANCE_INTERVAL_US
#define THREADX_TIME_ADVANCE_INTERVAL_US	1	/* Minimum time advance step */
#endif /* THREADX_TIME_ADVANCE_INTERVAL_US */

VOID _tx_timer_interrupt(VOID);

/* in tx_low_power.c */
extern UINT	tx_low_power_entered;
extern ULONG	tx_low_power_last_timer;
extern ULONG	tx_low_power_next_expiration;
extern ULONG	tx_low_power_current_timer;
extern ULONG	tx_low_power_adjust_ticks;

/**
 * Schedule a hardware interrupt based on next timer expiration time.
 *
 * This interrupt will also wake us up from low power (WFI). The return value could be
 * used to decide whether it makes sense to configure the device for low power operation,
 * based on the time we expect to be in low power mode.
 *
 * @return			Time until next timer expiration, in us.
 */

uint32
threadx_schedule_timer(void)
{
	TX_INTERRUPT_SAVE_AREA

	ULONG active;
	uint32 time_next_interrupt = 0xffffffff;

	TX_DISABLE

	/* At this point, we want to enter low power mode, since nothing
	 * meaningful is going on in the system. However, in order to keep
	 * the ThreadX timer services accurate, we must first determine the
	 * next ThreadX timer expiration in terms of ticks. This is
	 * accomplished via the tx_timer_get_next API.
	 */
	active = tx_timer_get_next(&tx_low_power_next_expiration);

	/* Determine if any timers are active. */
	if (active) {
		/* Reprogram the internal timer source such that the next timer
		 * interrupt is equal to:  next_timer_expiration*tick_frequency.
		 * In most applications, the tick_frequency is 10ms, but this is
		 * completely application specific in ThreadX.
		 */

		/* tx_timer_get_next() returns less 1 tick */
		tx_low_power_next_expiration += 1;

		uint32 time_since_update = (uint32)threadx_low_power_time_since_update();
		uint32 time_next_expiry  = (uint32)tx_low_power_next_expiration;

		time_next_interrupt = 0;
		if (time_next_expiry > time_since_update) {
			time_next_interrupt = time_next_expiry - time_since_update;
		}

		/* Schedule a hardware timer interrupt at the next expiry */
		hnd_thread_set_hw_timer(time_next_interrupt);
	} else {
		/* No active timers, disable the hardware timer */
		hnd_thread_ack_hw_timer();
	}

	TX_RESTORE

	return time_next_interrupt;
}

/**
 * Update ThreadX time and process any expired timers.
 *
 * @note Assumed to be called in ISR context.
 */

void
threadx_advance_time(void)
{
#ifndef RTE_POLL
	ASSERT(hnd_in_isr());
#endif /* RTE_POLL */

	tx_low_power_current_timer = (uint32)hnd_thread_get_hw_timer_us();

	/* Determine how many timer ticks the ThreadX time should be incremented to properly adjust
	 * for the time in low power mode. The result must be placed in tx_low_power_adjust_ticks.
	 */
	tx_low_power_adjust_ticks = tx_low_power_current_timer - tx_low_power_last_timer;

	/* Determine if the ThreadX timer needs incrementing.  */
	if (tx_low_power_adjust_ticks >= THREADX_TIME_ADVANCE_INTERVAL_US) {
		tx_low_power_last_timer = tx_low_power_current_timer;

		/* Advance time by ticks less 1 */
		tx_time_increment(tx_low_power_adjust_ticks - 1);

		/* Advance time by 1 tick and invoke expired timer callbacks */
		_tx_timer_interrupt();
	}
}

/**
 * Enter low power mode.
 *
 * @note: assumes interrupts are DISABLED.
 */

void
threadx_low_power_enter(void)
{
	if (hnd_sih != NULL) {
		if (SRPWR_ENAB()) {
			sr_process_save(hnd_sih);
		}

		/* For power saving need to turn off clock doubler, if applicable */
		hnd_cpu_clockratio(hnd_sih, 1);
	}

	tx_low_power_entered = TRUE;
}

/**
 * Exit low power mode.
 *
 * @note Assumed to be called in ISR context.
 */

void
threadx_low_power_exit(void)
{
	ASSERT(hnd_in_isr());

	if (!tx_low_power_entered)
		return;

	tx_low_power_entered = FALSE;

	if (hnd_sih != NULL) {
		hnd_cpu_clockratio(hnd_sih, 2); /* select high(er) ARM performance */

		if (SRPWR_ENAB()) {
			sr_process_restore(hnd_sih);
		}
	}
}

/**
 * Initialize low power mode
 */

void
threadx_low_power_init(void)
{
	tx_low_power_last_timer = (uint32)hnd_thread_get_hw_timer_us();
}

/**
 * Get usec since last call to @see threadx_advance_time.
 */

uint32
threadx_low_power_time_since_update(void)
{
	TX_INTERRUPT_SAVE_AREA

	uint32 usec;

	TX_DISABLE
	usec = (uint32)hnd_thread_get_hw_timer_us() - tx_low_power_last_timer;
	TX_RESTORE

	return usec;
}
