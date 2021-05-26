/** \file rte_scheduler_priv.h
 *
 * OS independent worklet scheduler functions, private to RTE.
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
 * $Id: rte_scheduler_priv.h 771683 2019-02-05 13:42:44Z $
 */

#ifndef	_RTE_SCHEDULER_PRIV_H
#define	_RTE_SCHEDULER_PRIV_H

#include <typedefs.h>
#include <rte_scheduler.h>
#include "rte_isr_stats_priv.h"

typedef void (*hnd_schedule_worklet_fn_t)(hnd_worklet_t *worklet);

/* Worklet state and worklet stop reasons */

#define WORKLET_STATE_MASK		0x0F
#define WORKLET_STATE_NONE		0x00
#define WORKLET_STATE_SCHEDULED		0x01	/* Enqueued on scheduled or expired queue */
#define WORKLET_STATE_DELETED		0x02	/* Marked for deletion */
#define WORKLET_STATE_CANCELLED		0x04	/* Marked for cancellation */
#define WORKLET_STATE_EXECUTING		0x08	/* Currently executing */

#define STOP_REASON_MASK		0xF0
#define STOP_REASON_SHIFT		4
#define STOP_REASON_COUNT		4	/* Should match @see stop_reason_t */

struct worklet
{
	uint8			state;			/* State and last stop reason */
	uint8			priority;		/* Assigned priority */
	uint8			quota;			/* Assigned time quota */
	uint8			quota_used;		/* Used time quota */
	hnd_scheduler_t		*scheduler;		/* Target scheduler */
	hnd_worklet_fn_t	fn;			/* Pointer to target function */
	void			*cbdata;		/* Target function opaque data */
	osl_ext_time_us_t	start;			/* Last execution start timestamp */
	hnd_worklet_t		*scheduled_next;	/* Scheduled or expired queue link */
	hnd_worklet_t		*all_next;		/* All worklets link (DEBUG) */
	hnd_stats_t		*stats;			/* Statistics context */

#ifdef BCMDBG
	osl_ext_time_us_t	stop_query_last;
	hnd_stats_val_t		stop_query_interval;
	uint32			stop_reason_count[STOP_REASON_COUNT];
#endif /* BCMDBG */
};

struct queue
{
	hnd_worklet_t		*head;
	hnd_worklet_t		*tail;
	hnd_worklet_t		*indexer[SCHEDULER_MAX_PRIORITY + 1];
};
typedef struct queue queue_t;

struct scheduler
{
	osl_ext_task_t		*thread;		/* Target thread context */
	hnd_worklet_t		*worklets;		/* List of all worklets */
	queue_t			queues[2];		/* Scheduled and expired queues */
	queue_t			*scheduled_queue;	/* Pointer to current scheduled queue */
	queue_t			*expired_queue;		/* Pointer to current expired queue */
	hnd_worklet_t		*executing;		/* Currently executing worklet, or NULL */
	stop_reason_t		stop_reason;		/* Executing worklet stop request flag */
	osl_ext_time_us_t	period_start;		/* Scheduler period start time */
	osl_ext_event_t		events;			/* Scheduler trigger event */
	osl_ext_time_us_t	virtual_time;		/* Virtual time value */
	osl_ext_time_us_t	virtual_time_start;	/* Start of last active period */

#ifdef BCMDBG
	uint			period_count;
#endif /* BCMDBG */
};

/* Private interface */

bool hnd_worklet_is_valid(hnd_worklet_t *worklet);
osl_ext_time_us_t hnd_scheduler_get_virtual_time(hnd_scheduler_t *scheduler);
void hnd_worklet_set_priority(hnd_worklet_t *worklet, priority_t priority);
void hnd_worklet_set_quota(hnd_worklet_t *worklet, osl_ext_time_us_t quota);
hnd_scheduler_t* hnd_scheduler_get_inst(osl_ext_task_t *thread);
void hnd_scheduler_notify_thread(osl_ext_task_t *current, osl_ext_task_t *next);
void hnd_scheduler_notify_isr_enter(osl_ext_task_t *current);
void hnd_scheduler_notify_isr_exit(osl_ext_task_t *next);

#endif /* _RTE_SCHEDULER_PRIV_H */
