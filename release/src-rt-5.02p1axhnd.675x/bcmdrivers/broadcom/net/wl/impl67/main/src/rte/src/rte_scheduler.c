/** \file rte_scheduler.c
 *
 * OS independent functions for worklet scheduler.
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
 * $Id: rte_scheduler.c 781914 2019-12-04 11:33:00Z $
 */

#include <rte.h>
#include "rte_priv.h"
#include <rte_isr.h>
#include "rte_isr_priv.h"
#include <rte_isr_stats.h>
#include "rte_isr_stats_priv.h"
#include <rte_scheduler.h>
#include "rte_scheduler_priv.h"
#include <rte_cons.h>
#include <threadx_osl_ext.h>
#include <bcmutils.h>

#ifndef BCM_ENABLE_THREAD_CHANGE_NOTIFY
#error "BCM_ENABLE_THREAD_CHANGE_NOTIFY not defined"
#endif /* !BCM_ENABLE_THREAD_CHANGE_NOTIFY */

#define SCHEDULER_QUOTA_UNIT_US		256	/* Duration of one quota unit, in us. */
#define SCHEDULER_MAX_QUOTA_VALUE	255	/* Max value for uint8 */
#define SCHEDULER_SIGNAL_RUN		0x01	/* Event group bit for checking for worklets */

#define SCHED_ERROR(args)		do { printf args; } while (0)
#ifdef BCMDBG
#define SCHED_INFORM(args)		do { printf args; } while (0)
#else
#define SCHED_INFORM(args)
#endif /* BCMDBG */

/*
 *
 * Internal Functions
 *
 */

static uint
limit(uint value, uint min, uint max)
{
	if (value >= max) return max;
	if (value <= min) return min;
	return value;
}

/**
 * Check if a worklet is on either the scheduled or expired queues.
 *
 * @param worklet	Pointer to worklet.
 * @return		TRUE if on one of both queues.
 */

static INLINE bool
worklet_scheduled_flag(hnd_worklet_t *worklet)
{
	ASSERT(hnd_worklet_is_valid(worklet));

	return (worklet->state & WORKLET_STATE_SCHEDULED) != 0;
}

/**
 * Check if a worklet is marked for deletion.
 *
 * @param worklet	Pointer to worklet.
 * @return		TRUE if marked for deletion.
 */

static INLINE bool
worklet_deleted_flag(hnd_worklet_t *worklet)
{
	ASSERT(hnd_worklet_is_valid(worklet));

	return (worklet->state & WORKLET_STATE_DELETED) != 0;
}

/**
 * Check if a worklet is cancelled.
 *
 * @param worklet	Pointer to worklet.
 * @return		TRUE if cancelled.
 */

static INLINE bool
worklet_cancelled_flag(hnd_worklet_t *worklet)
{
	ASSERT(hnd_worklet_is_valid(worklet));

	return (worklet->state & WORKLET_STATE_CANCELLED) != 0;
}

/**
 * Free a worklet.
 *
 * @param worklet	Pointer to worklet.
 */

static void
worklet_free_internal(hnd_worklet_t *worklet)
{
	ASSERT(!hnd_worklet_is_pending_or_executing(worklet));
	ASSERT(!hnd_in_isr());

	hnd_isr_stats_free(worklet->stats);

#ifdef BCMDBG
	/* Detect use after free, @see hnd_worklet_is_valid */
	memset(worklet, 0, sizeof(hnd_worklet_t));
#endif /* BCMDBG */

	MFREE(hnd_osh, worklet, sizeof(hnd_worklet_t));
}

/**
 * Get the current time.
 *
 * @return		Current time as indicated by scheduler timebase.
 */

static osl_ext_time_us_t
scheduler_get_time(void)
{
	return (osl_ext_time_us_t)hnd_thread_get_hw_timer_us();
}

/**
 * Handle the scheduler thread being preempted by a higher priority thread or interrupt.
 *
 * @param scheduler	Pointer to scheduler instance.
 */

static INLINE void
scheduler_preempt_start(hnd_scheduler_t *scheduler)
{
	if (scheduler != NULL && scheduler->virtual_time_start != 0) {
		scheduler->virtual_time += (scheduler_get_time() - scheduler->virtual_time_start);
		scheduler->virtual_time_start = 0;
	}
}

/**
 * Handle the scheduler thread being scheduled again after being preempted.
 *
 * @param scheduler	Pointer to scheduler instance.
 */

static INLINE void
scheduler_preempt_end(hnd_scheduler_t *scheduler)
{
	if (scheduler != NULL) {
		scheduler->virtual_time_start = scheduler_get_time();
	}
}

/**
 * Convert a duration in microseconds to a time quota value.
 *
 * The scheduler internally uses quota values to allow simultaneous scaling of time quotas
 * of all worklets in the system.
 *
 * @param duration	Duration in microseconds.
 * @return		Time quota value, rounded up.
 */

static INLINE quota_t
scheduler_duration_to_quota(osl_ext_time_us_t duration)
{
	return (duration + SCHEDULER_QUOTA_UNIT_US - 1) / SCHEDULER_QUOTA_UNIT_US;
}

#ifdef BCMDBG

/**
 * Convert a time quota value to a duration in microseconds.
 *
 * @param quota		Time quota value.
 * @return		Duration in microseconds.
 */

static INLINE osl_ext_time_us_t
scheduler_quota_to_duration(quota_t quota)
{
	return quota * SCHEDULER_QUOTA_UNIT_US;
}

static hnd_scheduler_t*
scheduler_find_by_name(const char *name)
{
	osl_ext_task_t *thread = hnd_thread_find_by_name(name ? name : "main_thread");
	if (thread != NULL) {
		return hnd_scheduler_get_inst(thread);
	}

	return NULL;
}

static void
scheduler_show(hnd_scheduler_t *scheduler, bool dump_all)
{
	hnd_worklet_t *worklet;

	ASSERT(scheduler != NULL);

	printf_suppress_timestamp(TRUE);

	printf("thread: [%p] %s\n", scheduler->thread, THREAD_NAMEPTR(scheduler->thread));
	printf("virtual time: %u\n", hnd_scheduler_get_virtual_time(scheduler));
	printf("period count: %u\n", scheduler->period_count);

	printf("STATE  PRIO   QUOTA_US  STOP_DONE STOP_QUOTA  STOP_PRIO STOP_CANCL   "
		"POLL_MIN   POLL_AVG   POLL_MAX TARGET\n");

	worklet = scheduler->worklets;
	while (worklet != NULL) {
		if (dump_all == TRUE || worklet->stats == NULL || worklet->stats->count != 0) {
			printf(" %c%c%c%c %5u %10u %10u %10u %10u %10u %10u %10u %10u [%p] %s\n",
				worklet->state & WORKLET_STATE_SCHEDULED ? 'S' : '.',
				worklet->state & WORKLET_STATE_DELETED ? 'D' : '.',
				worklet->state & WORKLET_STATE_CANCELLED ? 'C' : '.',
				worklet->state & WORKLET_STATE_EXECUTING ? 'E' : '.',
				worklet->priority,
				scheduler_quota_to_duration(worklet->quota),
				worklet->stop_reason_count[STOP_REASON_NONE],
				worklet->stop_reason_count[STOP_REASON_QUOTA],
				worklet->stop_reason_count[STOP_REASON_PRIORITY],
				worklet->stop_reason_count[STOP_REASON_CANCELLED],
				worklet->stop_query_interval.min,
				hnd_isr_stats_value_avg(&worklet->stop_query_interval),
				worklet->stop_query_interval.max,
				worklet->stats != NULL ? worklet->stats->target : worklet->fn,
				worklet->stats != NULL ? worklet->stats->id : "");
		}
		worklet = worklet->all_next;
	}

	printf_suppress_timestamp(FALSE);
}

static void
scheduler_dump_queue(queue_t *queue)
{
	hnd_worklet_t *worklet = queue->head;
	int i;

	printf("INDEX  PRIO QUOTA_US  WORKLET TARGET\n");

	while (worklet != NULL) {
		printf("%c     %5u %8u %p [%p] %s\n",
			queue->indexer[worklet->priority] == worklet ? '0'+worklet->priority : ' ',
			worklet->priority,
			scheduler_quota_to_duration(worklet->quota),
			worklet,
			worklet->stats != NULL ? worklet->stats->target : worklet->fn,
			worklet->stats != NULL ? worklet->stats->id : "");
		worklet = worklet->scheduled_next;
	}

	for (i = 0; i <= SCHEDULER_MAX_PRIORITY; i++) {
		printf("[%p]", queue->indexer[i]);
	}
	printf("\n");
}

static void
scheduler_dump(hnd_scheduler_t *scheduler)
{
	ASSERT(scheduler != NULL);

	printf_suppress_timestamp(TRUE);

	if (scheduler->scheduled_queue->head != NULL) {
		printf("scheduled:\n");
		scheduler_dump_queue(scheduler->scheduled_queue);
	}
	if (scheduler->expired_queue->head != NULL) {
		printf("expired:\n");
		scheduler_dump_queue(scheduler->expired_queue);
	}

	printf_suppress_timestamp(FALSE);
}

static void
scheduler_cmd(void *arg, int argc, char *argv[])
{
	hnd_scheduler_t *scheduler;
	hnd_worklet_t *worklet;

	if (argc <= 1)
		return;

	if (!strcmp(argv[1], "show")) {
		scheduler = scheduler_find_by_name(argc > 2 ? argv[2] : NULL);
		if (scheduler != NULL) {
			scheduler_show(scheduler, bcm_has_option("all", argc-2, argv+2, NULL));
		} else {
			printf("no scheduler found\n");
		}
	} else if (!strcmp(argv[1], "dump")) {
		scheduler = scheduler_find_by_name(argc > 2 ? argv[2] : NULL);
		if (scheduler != NULL) {
			scheduler_dump(scheduler);
		} else {
			printf("no scheduler found\n");
		}
	} else if (!strcmp(argv[1], "set")) {
		if (argc > 5) {
			/* This feature depends on HND_INTSTATS to associate an id to worklets */
			scheduler = scheduler_find_by_name(argv[2]);
			if (scheduler != NULL) {
				worklet = hnd_worklet_find_by_name(scheduler, argv[3]);
				if (worklet != NULL) {
					hnd_worklet_set_priority(worklet, atoi(argv[4]));
					hnd_worklet_set_quota(worklet, atoi(argv[5]));
					printf("worklet [%s] set to priority %u quota %u\n",
						argv[3], worklet->priority, worklet->quota);
					return;
				} else {
					printf("worklet [%s] not found\n", argv[3]);
				}
			} else {
				printf("no scheduler found\n");
			}
		} else {
			printf("set [thread] [worklet] [priority] [quota]\n");
		}
	}
}

#endif /* BCMDBG */

/**
 * Trigger the scheduler to check for pending worklets.
 *
 * @param scheduler	Pointer to scheduler instance.
 */

static INLINE void
scheduler_signal(hnd_scheduler_t *scheduler)
{
	ASSERT(scheduler != NULL);

	osl_ext_event_set(&scheduler->events, SCHEDULER_SIGNAL_RUN);
}

/**
 * Query whether the scheduler is currently idle.
 *
 * Idle means no worklets are scheduled and no worklet is being executed.
 *
 * @param scheduler	Pointer to scheduler instance.
 * @return		TRUE if scheduler is idle.
 */

static bool
scheduler_is_idle(hnd_scheduler_t *scheduler)
{
	return (scheduler->scheduled_queue->head == NULL && scheduler->executing == NULL);
}

/**
 * Start a new scheduler period.
 *
 * During a scheduler period all scheduler worklets get a chance to execute. When all worklets
 * have consumed their respective time quota, they all receive a new time quota and the next
 * scheduler period is started. This mechanism ensures no starvation can occur.
 *
 * @param scheduler	Pointer to scheduler instance.
 */

static void
scheduler_start_period(hnd_scheduler_t *scheduler)
{
#if defined(BCMDBG)
	scheduler->period_count++;
#endif /* BCMDBG */

	scheduler->period_start = hnd_scheduler_get_virtual_time(scheduler);
}

/**
 * Put a worklet on the scheduled queue.
 *
 * This inserts the specified worklet into the scheduled queue in order of descending
 * priority. If other worklets with the same priority are already on the queue, the new worklet
 * is inserted after the last existing worklet of the same priority.
 *
 * @note Interrupts should be disabled.
 *
 * @param worklet	Pointer to worklet.
 * @return		TRUE if the worklet is the new scheduled queue HEAD element.
 */

static bool
scheduler_push_scheduled(hnd_worklet_t *worklet)
{
	priority_t priority;
	hnd_worklet_t *insert_after;
	hnd_scheduler_t *scheduler = worklet->scheduler;
	queue_t *queue = scheduler->scheduled_queue;

	ASSERT(worklet->priority <= SCHEDULER_MAX_PRIORITY);
	ASSERT(!worklet_scheduled_flag(worklet));

	/* Search the queue indexer array for the insertion point of the worklet
	 * in the scheduled queue.
	 */
	priority = worklet->priority;
	do {
		insert_after = queue->indexer[priority];
	} while (insert_after == NULL && priority-- > 0);

	/* Update the queue indexer array */
	queue->indexer[worklet->priority] = worklet;

	/* Update worklet state */
	worklet->state |= WORKLET_STATE_SCHEDULED;

	/* Insert worklet into the scheduled queue */
	if (insert_after != NULL) {
		/* Insert into the scheduled queue after the last worklet of the same priority */
		worklet->scheduled_next = insert_after->scheduled_next;
		insert_after->scheduled_next = worklet;
		return FALSE;
	} else {
		/* Insert into the scheduled queue as the new head element */
		worklet->scheduled_next = queue->head;
		queue->head = worklet;
		return TRUE;
	}
}

/**
 * Get the next worklet from the scheduled queue.
 *
 * This removes the head element of the scheduled queue.
 *
 * @note Interrupts should be disabled.
 *
 * @param scheduler	Pointer to scheduler instance.
 * @return		Pointer to worklet, or NULL if none available.
 */

static hnd_worklet_t*
scheduler_pop_scheduled(hnd_scheduler_t *scheduler)
{
	queue_t *queue = scheduler->scheduled_queue;
	hnd_worklet_t *next = queue->head;
	if (next != NULL) {
		/* Update the queue indexer array if this was the last worklet with this priority */
		if (queue->indexer[next->priority] == next) {
			queue->indexer[next->priority] = NULL;
		}

		/* Make sure queue is sorted on priority */
		ASSERT(next->scheduled_next == NULL ||
			next->scheduled_next->priority >= next->priority);

		/* Remove worklet from scheduled queue */
		queue->head = next->scheduled_next;

		/* Update worklet state */
		next->scheduled_next = NULL;
		next->state &= ~WORKLET_STATE_SCHEDULED;
	}

	return next;
}

/**
 * Put a worklet on the expired queue and reset quota.
 *
 * This inserts the specified worklet at the tail of the queue. Since worklets are guaranteed
 * to arrive in order of priority, this will cause the queue elements to be sorted in order
 * of descending priority.
 *
 * @note Interrupts should be disabled.
 *
 * @param worklet	Pointer to worklet.
 */

static void
scheduler_push_expired(hnd_worklet_t *worklet)
{
	hnd_scheduler_t *scheduler = worklet->scheduler;
	queue_t *queue = scheduler->expired_queue;

	ASSERT(worklet->priority <= SCHEDULER_MAX_PRIORITY);
	ASSERT(!worklet_scheduled_flag(worklet));

	if (queue->head != NULL) {
		/* Append to the rear of the queue */
		ASSERT(queue->tail != NULL);
		ASSERT(worklet->priority >= queue->tail->priority);
		queue->tail->scheduled_next = worklet;
		queue->tail = worklet;
	} else {
		/* Queue is empty, insert as head element */
		ASSERT(queue->tail == NULL);
		queue->head = queue->tail = worklet;
	}

	queue->indexer[worklet->priority] = worklet;

	worklet->scheduled_next = NULL;
	worklet->quota_used = 0;
	worklet->state |= WORKLET_STATE_SCHEDULED;
}

/**
 * Select the next worklet and execute it if not cancelled, deleted, or out of quota.
 *
 * @param scheduler	Pointer to scheduler instance.
 * @return		TRUE if a worklet was available for execution.
 */

static INLINE bool
scheduler_execute_next_worklet(hnd_scheduler_t *scheduler)
{
	OSL_INTERRUPT_SAVE_AREA

	bool reschedule;
	quota_t quota_used;
	hnd_worklet_t *worklet;
	osl_ext_time_us_t now;
	queue_t *queue;

	ASSERT(!hnd_in_isr());

	OSL_DISABLE

	/* When all active worklets have been moved to the expired queue, swap the scheduled and
	 * expired queues and associated queue indexers, and start a new scheduler period.
	 */
	if (scheduler->scheduled_queue->head == NULL && scheduler->expired_queue->head != NULL) {
		queue = scheduler->scheduled_queue;
		scheduler->scheduled_queue = scheduler->expired_queue;
		scheduler->expired_queue   = queue;

		scheduler->expired_queue->tail = NULL;
		scheduler_start_period(scheduler);
	}

	/* Get the next worklet from the head of the scheduled queue */
	worklet = scheduler_pop_scheduled(scheduler);
	if (worklet == NULL) {
		/* No more worklets */

		OSL_RESTORE
		return FALSE;
	}

	/* Double check worklet state */
	ASSERT(hnd_worklet_is_valid(worklet));
	ASSERT(worklet->scheduler == scheduler);

	/* Check if the worklet is marked for deletion */
	if (worklet_deleted_flag(worklet) && !worklet_scheduled_flag(worklet)) {
		worklet_free_internal(worklet);

		OSL_RESTORE
		return TRUE;
	}

	/* Check if the worklet was cancelled */
	if (worklet_cancelled_flag(worklet)) {

		OSL_RESTORE
		return TRUE;
	}

	/* Check if worklet still has some quota left. There is a corner case where it can
	 * be already used up. If so, push it to the expired queue immediately.
	 */
	if (worklet->quota_used >= worklet->quota) {
		scheduler_push_expired(worklet);

		OSL_RESTORE
		return TRUE;
	}

	HND_INTSTATS_NOTIFY(NOTIFICATION_TYPE_QUEUE_EXIT, (void*)worklet->stats);

	/* Triple check worklet state */
	ASSERT((worklet->state &
		(WORKLET_STATE_SCHEDULED | WORKLET_STATE_DELETED |
		WORKLET_STATE_CANCELLED | WORKLET_STATE_EXECUTING)) == 0);

	now = hnd_scheduler_get_virtual_time(scheduler);

#ifdef BCMDBG
	worklet->stop_query_last = now;
#endif /* BCMDBG */

	/* Update currently executing worklet in two locations for quick access */
	scheduler->executing = scheduler->thread->current_worklet = worklet;

	scheduler->stop_reason = STOP_REASON_NONE;
	worklet->state |= WORKLET_STATE_EXECUTING;

	OSL_RESTORE

	HND_INTSTATS_NOTIFY(NOTIFICATION_TYPE_WORKLET_ENTER, (void*)worklet->stats);

	/* Execute the worklet */
	worklet->start = now;
	reschedule = (*worklet->fn)(worklet->cbdata);

	HND_INTSTATS_NOTIFY(NOTIFICATION_TYPE_WORKLET_EXIT, (void*)worklet->stats);

	OSL_DISABLE

	/* Calculate and update used quota  */
	quota_used = scheduler_duration_to_quota(
		hnd_scheduler_get_virtual_time(scheduler) - worklet->start);
	if (quota_used == 0)
		++quota_used;
	worklet->quota_used = limit(quota_used + worklet->quota_used, 0, SCHEDULER_MAX_QUOTA_VALUE);

	/* Update currently executing worklet in two locations for quick access */
	scheduler->executing = scheduler->thread->current_worklet = NULL;

	/* Update worklet state and store the last stop request reason in the worklet */
	worklet->state &= ~(WORKLET_STATE_EXECUTING | STOP_REASON_MASK);
	worklet->state |= (scheduler->stop_reason << STOP_REASON_SHIFT);

	switch (scheduler->stop_reason) {
		case STOP_REASON_NONE:
			/* Worklet completed normally. If it requested to be rescheduled, move it
			 * to the expired queue to allow it to run again during the next scheduler
			 * period.
			 */
			if (reschedule == TRUE) {
				if (!worklet_scheduled_flag(worklet)) {
					scheduler_push_expired(worklet);
				}
			}
			break;
		case STOP_REASON_QUOTA:
			/* Worklet consumed its time quota, move it to expired queue to allow it
			 * to run again during the next scheduler period.
			 */
			if (!worklet_scheduled_flag(worklet)) {
				scheduler_push_expired(worklet);
			}
			break;
		case STOP_REASON_PRIORITY:
			/* Worklet was requested to stop due to availability of a worklet of higher
			 * priority. Move it back to the scheduled queue if not already pending.
			 */
			if (!worklet_scheduled_flag(worklet)) {
				scheduler_push_scheduled(worklet);
			}
			break;
		case STOP_REASON_CANCELLED:
			/* Worklet was cancelled or deleted */
			break;
		default:
			/* Unknown reason */
			ASSERT(0);
			break;
	}

#ifdef BCMDBG
	worklet->stop_reason_count[scheduler->stop_reason]++;
#endif /* BCMDBG */

	OSL_RESTORE

	return TRUE;
}

/*
 *
 * Export functions
 *
 */

/**
 * Check if a worklet pointer and internal state are valid.
 *
 * @param worklet	Pointer to worklet.
 * @return		TRUE if valid.
 */

INLINE bool
hnd_worklet_is_valid(hnd_worklet_t *worklet)
{
#ifdef BCMDBG
	/* Detect use after free, @see worklet_free_internal */
	return worklet != NULL && worklet->fn != NULL && worklet->scheduler != NULL;
#else
	return worklet != NULL;
#endif /* BCMDBG */
}

/**
 * Check if a worklet is being executed.
 *
 * @param worklet	Pointer to worklet.
 * @return		TRUE if being executed.
 */

INLINE bool
hnd_worklet_is_executing(hnd_worklet_t *worklet)
{
	ASSERT(hnd_worklet_is_valid(worklet));

	return (worklet->state & WORKLET_STATE_EXECUTING) != 0;
}

/**
 * Check if a worklet is pending. Pending worklets are quaranteed to be executed unless they
 * are cancelled or deleted before that.
 *
 * @param worklet	Pointer to worklet.
 * @return		TRUE if pending.
 */

INLINE bool
hnd_worklet_is_pending(hnd_worklet_t *worklet)
{
	ASSERT(hnd_worklet_is_valid(worklet));

	return (worklet->state &
		(WORKLET_STATE_SCHEDULED | WORKLET_STATE_CANCELLED |
		WORKLET_STATE_DELETED)) == WORKLET_STATE_SCHEDULED;
}

/**
 * Check if a worklet is pending or being executed. Pending worklets are quaranteed to be executed
 * unless they are cancelled or deleted before that.
 *
 * @param worklet	Pointer to worklet.
 * @return		TRUE if pending or being executed.
 */

INLINE bool
hnd_worklet_is_pending_or_executing(hnd_worklet_t *worklet)
{
	return hnd_worklet_is_pending(worklet) || hnd_worklet_is_executing(worklet);
}

#ifdef BCMDBG

/**
 * Find a worklet by name.
 *
 * This function depends on the HND_INTSTATS feature being enabled.
 *
 * @param scheduler	Pointer to scheduler.
 * @param name		Pointer to name.
 * @return		Worklet, or NULL if no worklet found.
 */

hnd_worklet_t*
hnd_worklet_find_by_name(hnd_scheduler_t *scheduler, const char *name)
{
#ifdef HND_INTSTATS
	hnd_worklet_t *worklet;

	ASSERT(scheduler != NULL);

	worklet = scheduler->worklets;
	while (worklet != NULL) {
		if (worklet->stats != NULL && strcmp(worklet->stats->id, name) == 0) {
			return worklet;
		}
		worklet = worklet->all_next;
	}
#endif /* HND_INTSTATS */

	return NULL;
}

#endif /* BCMDBG */

/**
 * Get the scheduler associated to the worklet target thread.
 *
 * A worklet can only be scheduled on its associated target thread through the scheduler associated
 * to the same.
 *
 * @param worklet	Pointer to worklet.
 * @return		Pointer to associated scheduler.
 */

INLINE hnd_scheduler_t*
hnd_worklet_get_scheduler(hnd_worklet_t *worklet)
{
	ASSERT(hnd_worklet_is_valid(worklet));

	return worklet->scheduler;
}

/**
 * Get the worklet target thread.
 *
 * @param worklet	Pointer to worklet.
 * @return		Pointer to target thread.
 */

inline osl_ext_task_t*
hnd_worklet_get_thread(hnd_worklet_t *worklet)
{
	return hnd_worklet_get_scheduler(worklet)->thread;
}

/**
 * Return the currently executing worklet.
 *
 * @return		Pointer to current worklet, or NULL.if no worklet is executing or no
 *			scheduler is associated with the current thread (@see hnd_scheduler_init).
 */

INLINE hnd_worklet_t*
hnd_worklet_get_current(void)
{
	return hnd_scheduler_get_worklet(NULL);
}

/**
 * Return the statistics context of the currently executing worklet.
 *
 * This function depends on the HND_INTSTATS feature being enabled.
 *
 * @return		Statistics context, or NULL if not in worklet context.
 */

hnd_stats_t*
hnd_worklet_get_stats(void)
{
#ifdef HND_INTSTATS
	hnd_worklet_t *worklet = hnd_worklet_get_current();
	return worklet != NULL ? worklet->stats : NULL;
#else
	return NULL;
#endif /* HND_INTSTATS */
}

/**
 * Return the id of the currently executing worklet.
 *
 * This function depends on the HND_INTSTATS feature being enabled.
 *
 * @return		Id, or NULL if not in worklet context.
 */

const char*
hnd_worklet_get_id(void)
{
#ifdef HND_INTSTATS
	return hnd_isr_stats_get_id(hnd_worklet_get_stats());
#else
	return "";
#endif /* HND_INTSTATS */
}

/**
 * Get the current virtual time. Virtual time is expressed in microseconds since start of
 * execution of the worklet, and increases monotonically only when the worklet thread is active.
 *
 * @return		Current virtual time.
 */

osl_ext_time_us_t
hnd_worklet_get_virtual_time(void)
{
	hnd_worklet_t *worklet = hnd_worklet_get_current();
	ASSERT(worklet != NULL);

	if (worklet->start != 0) {
		return hnd_scheduler_get_virtual_time(worklet->scheduler) - worklet->start;
	}

	return 0;
}

/**
 * Set the priority value of a worklet.
 *
 * Effective values are clamed to 0 and SCHEDULER_MAX_PRIORITY. Changes take effect the next
 * time the worklet is scheduled or stopped.
 *
 * @param worklet	Pointer to worklet.
 * @param priority	Worklet priority value.
 */

void
hnd_worklet_set_priority(hnd_worklet_t *worklet, priority_t priority)
{
	ASSERT(hnd_worklet_is_valid(worklet));

	worklet->priority = limit(priority, 0, SCHEDULER_MAX_PRIORITY);
}

/**
 * Set the time quota of a worklet.
 *
 * Effective values are clamed to 1 and SCHEDULER_MAX_QUOTA_VALUE.  Changes take effect the next
 * time the worklet is scheduled or stopped.
 *
 * @param worklet	Pointer to worklet.
 * @param quota		Worklet time quota in microseconds.
 */

void
hnd_worklet_set_quota(hnd_worklet_t *worklet, osl_ext_time_us_t quota)
{
	ASSERT(hnd_worklet_is_valid(worklet));

	worklet->quota = limit(scheduler_duration_to_quota(quota), 1, SCHEDULER_MAX_QUOTA_VALUE);
}

/**
 * Create a worklet.
 *
 * A scheduler must have been created and associated to the specified target thread,
 * @see hnd_scheduler_create, @see hnd_thread_set_scheduler.
 *
 * @param worklet_fn	Pointer to worklet target function.
 * @param cbdata	Pointer to worklet target function data.
 * @param priority	Worklet priority value.
 * @param quota		Worklet time quota in microseconds.
 * @param thread	Pointer to worklet target thread. Can be NULL to use the
 *			current thread as the target.
 * @param id		Worklet identifier, or NULL.
 * @return		Pointer to the new worklet, or NULL on error.
 */

hnd_worklet_t*
hnd_worklet_create_internal(hnd_worklet_fn_t worklet_fn, void *cbdata, priority_t priority,
	osl_ext_time_us_t quota, osl_ext_task_t *thread, const char *id)
{
	hnd_scheduler_t *scheduler;
	hnd_worklet_t *worklet;

	ASSERT(!hnd_in_isr());
	ASSERT(worklet_fn != NULL);

	/* Use the current thread as the target thread if none is specified */
	if (thread == NULL) {
		thread = osl_ext_task_current();
		ASSERT(thread != NULL);
	}

	/* Verify that a scheduler is associated to the target thread */
	scheduler = hnd_scheduler_get_inst(thread);
	if (scheduler == NULL) {
		SCHED_ERROR(("no scheduler associated to target thread, worklet [%s]\n", id));
		ASSERT(FALSE);
		return NULL;
	}

	/* Create a new worklet */
	worklet = (hnd_worklet_t*)MALLOCZ(hnd_osh, sizeof(hnd_worklet_t));
	if (worklet != NULL) {
		worklet->state		= WORKLET_STATE_NONE | STOP_REASON_NONE;
		worklet->scheduler	= scheduler;
		worklet->fn		= worklet_fn;
		worklet->cbdata		= cbdata;

		worklet->all_next = scheduler->worklets;
		scheduler->worklets = worklet;

		worklet->stats = hnd_isr_stats_create(OBJ_TYPE_WORKLET, id,
			(void*)worklet_fn, thread->stats);

		hnd_worklet_set_priority(worklet, priority);
		hnd_worklet_set_quota(worklet, quota);

		SCHED_INFORM(("created worklet [%s] priority=%u quota=%u(%u)\n",
			id, worklet->priority, worklet->quota, quota));
	}

	ASSERT(worklet != NULL);
	return worklet;
}

/**
 * Schedule a worklet for execution.
 *
 * This function guarantees the scheduler will start execution of the specified worklet at some
 * time in the future, unless the worklet is cancelled or deleted before that. This means that
 * the worklet will be scheduled even if it is currently being executed on order to guarantee
 * it is executed in full.
 *
 * A scheduler must have been created and associated to the specified target thread,
 * @see hnd_scheduler_create, @see hnd_thread_set_scheduler.
 *
 * @note Safe to call from interrupt context.
 *
 * @param worklet	Pointer to worklet.
 */

void
hnd_worklet_schedule(hnd_worklet_t *worklet)
{
	OSL_INTERRUPT_SAVE_AREA

	bool was_idle;
	hnd_scheduler_t *scheduler;

	ASSERT(hnd_worklet_is_valid(worklet));

	scheduler = worklet->scheduler;
	ASSERT(scheduler != NULL);

	OSL_DISABLE

	was_idle = scheduler_is_idle(scheduler);
	if (was_idle) {
		/* Coming out of idle, start a new scheduler period */
		scheduler_start_period(scheduler);
	}

	/* Clear worklet cancelled flag to allow it to be executed if already scheduled */
	worklet->state &= ~WORKLET_STATE_CANCELLED;

	/* Only schedule the worklet if not already pending. */
	if (!hnd_worklet_is_pending(worklet)) {
		/* If a new scheduler period has started since this worklet was last scheduled,
		 * reset its quota.
		 */
		if (worklet->start < scheduler->period_start) {
			worklet->quota_used = 0;
		}

		HND_INTSTATS_NOTIFY(NOTIFICATION_TYPE_QUEUE_ENTER, (void*)worklet->stats);

		/* Insert the worklet into the scheduled queue. */
		if (scheduler_push_scheduled(worklet)) {
			/* Worklet was inserted at the head of the scheduled queue, check if
			 * it has a higher priority than the currently executing worklet.
			 */
			if (scheduler->executing != NULL) {
				if (scheduler->executing->priority > worklet->priority) {
					/* Need to request the executing worklet to yield if not
					 * requested already for a different reason.
					 */
					if (scheduler->stop_reason == STOP_REASON_NONE) {
						scheduler->stop_reason = STOP_REASON_PRIORITY;
					}
				}
			}
		}
	}

	OSL_RESTORE

	if (was_idle) {
		scheduler_signal(scheduler);
	}
}

/**
 * Reschedule the currently executing worklet.
 */

INLINE void
hnd_worklet_reschedule(void)
{
	hnd_worklet_schedule(hnd_worklet_get_current());
}

/**
 * Request deletion of a worklet.
 *
 * Worklet memory will be freed immediately if the worklet is not scheduled or being executed at
 * the time of the request. Otherwise, memory will be freed the next time the scheduler selects the
 * worklet for execution, and execution is skipped.
 *
 * If the worklet is executing at the time of the request, it will be requested to stop.
 *
 * @param worklet	Pointer to worklet.
 * @return		FALSE if the worklet was being executed at the time of the call.
 */

bool
hnd_worklet_delete(hnd_worklet_t *worklet)
{
	OSL_INTERRUPT_SAVE_AREA

	bool is_not_executing;

	ASSERT(!hnd_in_isr());
	ASSERT(hnd_worklet_is_valid(worklet));

	OSL_DISABLE

	is_not_executing = !hnd_worklet_is_executing(worklet);

	if (!worklet_deleted_flag(worklet)) {
		if (is_not_executing && !worklet_scheduled_flag(worklet)) {
			/* Worklet is not in use, delete immediately */
			worklet_free_internal(worklet);
		} else {
			/* Worklet is in use, mark for deletion */
			worklet->state |= WORKLET_STATE_DELETED;

			/* Reschedule the worklet to force deletion the next time it is
			 * selected for execution.
			 */
			if (!worklet_scheduled_flag(worklet)) {
				scheduler_push_scheduled(worklet);
				if (scheduler_is_idle(worklet->scheduler)) {
					scheduler_signal(worklet->scheduler);
				}
			}
		}
	}

	OSL_RESTORE

	return is_not_executing;
}

/**
 * Request cancellation of a worklet.
 *
 * The worklet will be unscheduled if it is in scheduled state at the time of the request. If the
 * worklet is executing at the time of the request, it will be requested to stop.
 *
 * @note Safe to call from interrupt context.
 *
 * @param worklet	Pointer to worklet.
 * @return		FALSE if the worklet was being executed at the time of the call.
 */

bool
hnd_worklet_cancel(hnd_worklet_t *worklet)
{
	OSL_INTERRUPT_SAVE_AREA

	bool is_executing;

	ASSERT(hnd_worklet_is_valid(worklet));

	OSL_DISABLE

	is_executing = hnd_worklet_is_executing(worklet);

	/* Mark the worklet for cancellation */
	if (is_executing || worklet_scheduled_flag(worklet)) {
		worklet->state |= WORKLET_STATE_CANCELLED;
	}

	OSL_RESTORE

	return !is_executing;
}

/**
 * Check whether the currently executing worklet should stop. To be called from within the
 * worklet itself.
 *
 * @param reason	Pointer to location to store the reason for the stop, can be NULL.
 * @return		TRUE if the currently executing worklet should yield.
 */

bool
hnd_worklet_should_stop(stop_reason_t *reason)
{
	OSL_INTERRUPT_SAVE_AREA

	bool stop;
	quota_t quota_used;
	hnd_worklet_t *worklet;
	hnd_scheduler_t *scheduler;
#if defined(BCMDBG) && defined(HND_INTSTATS)
	osl_ext_time_us_t now;
#endif /* BCMDBG && HND_INTSTATS */

	/* Allow higher priority threads to run */
	osl_ext_task_yield();

	scheduler = hnd_scheduler_get_inst(NULL);
	ASSERT(scheduler != NULL);

	OSL_DISABLE

	worklet = scheduler->executing;
	ASSERT(hnd_worklet_is_valid(worklet));
	ASSERT(worklet->scheduler = hnd_scheduler_get_inst(NULL));
	ASSERT(hnd_worklet_is_executing(worklet));

#if defined(BCMDBG) && defined(HND_INTSTATS)
	now = hnd_scheduler_get_virtual_time(scheduler);
	hnd_isr_stats_value_update(&worklet->stop_query_interval,
		now - worklet->stop_query_last);
	worklet->stop_query_last = now;
#endif /* BCMDBG && HND_INTSTATS */

	if (scheduler->stop_reason == STOP_REASON_NONE) {
		/* Check if cancelled or deleted */
		if (worklet->state & (WORKLET_STATE_CANCELLED | WORKLET_STATE_DELETED)) {
			scheduler->stop_reason = STOP_REASON_CANCELLED;
		} else {
			/* Check used quota */
			quota_used = scheduler_duration_to_quota(
				hnd_scheduler_get_virtual_time(scheduler) - worklet->start);
			if (quota_used == 0)
				quota_used++;

			if (quota_used + worklet->quota_used >= worklet->quota) {
				/* Quota exceeded */
				scheduler->stop_reason = STOP_REASON_QUOTA;
			}
		}
	}

	if (reason != NULL) {
		*reason = scheduler->stop_reason;
	}

	stop = (scheduler->stop_reason != STOP_REASON_NONE);

	OSL_RESTORE

	return stop;
}

/**
 * Get the reason why the currently executing worklet stopped the last time it was executed. To
 * be called from within the worklet itself.
 *
 * @return		Stop reason.
 */

stop_reason_t
hnd_worklet_prev_stop_reason(void)
{
	hnd_worklet_t *worklet;

	ASSERT(!hnd_in_isr());

	worklet = hnd_scheduler_get_worklet(NULL);
	ASSERT(worklet != NULL);

	return (worklet->state & STOP_REASON_MASK) >> STOP_REASON_SHIFT;
}

/**
 * Create a scheduler instance and associate it to a thread.
 *
 * @param thread	Pointer to scheduler target thread. Can be NULL to use the
 *			current thread as the target.
 * @param id		Scheduler identifier, or NULL.
 * @return		Scheduler handle, or NULL on error.
 */

hnd_scheduler_t*
hnd_scheduler_create_internal(osl_ext_task_t *thread, const char *id)
{
	hnd_scheduler_t *scheduler;

	ASSERT(!hnd_in_isr());

	/* Use the current thread as the target thread if none is specified */
	if (thread == NULL) {
		thread = osl_ext_task_current();
		ASSERT(thread != NULL);
	}

	/* Create a new scheduler */
	scheduler = (hnd_scheduler_t*)MALLOCZ(hnd_osh, sizeof(hnd_scheduler_t));
	if (scheduler != NULL) {
		scheduler->thread		= thread;
		scheduler->scheduled_queue	= &scheduler->queues[0];
		scheduler->expired_queue	= &scheduler->queues[1];

		osl_ext_event_create("event", &scheduler->events);

		/* Associate scheduler to thread */
		hnd_thread_set_scheduler(thread, (void*)scheduler);
	}

#ifdef BCMDBG
	hnd_cons_add_cmd("scheduler", scheduler_cmd, 0);
#endif /* BCMDBG */

	ASSERT(scheduler != NULL);
	return scheduler;
}

/**
 * Return the scheduler instance associated to a thread.
 *
 * @param thread	Pointer to scheduler thread, or NULL to get scheduler associated to
 *			the current thread.
 * @return		Pointer to scheduler, or NULL.if no scheduler is associated with the
 *			current thread (@see hnd_scheduler_init).
 */

INLINE hnd_scheduler_t*
hnd_scheduler_get_inst(osl_ext_task_t *thread)
{
	ASSERT(!hnd_in_isr());

	return (hnd_scheduler_t*)hnd_thread_get_scheduler(thread);
}

/**
 * Return the currently executing worklet.
 *
 * @param scheduler	Pointer to scheduler instance.
 * @return		Pointer to current worklet, or NULL.if no worklet is executing or no
 *			scheduler is associated with the current thread (@see hnd_scheduler_init).
 */

hnd_worklet_t*
hnd_scheduler_get_worklet(hnd_scheduler_t *scheduler)
{
	ASSERT(!hnd_in_isr());

	if (scheduler == NULL) {
		scheduler = (hnd_scheduler_t*)hnd_thread_get_scheduler(NULL);
		ASSERT(scheduler != NULL);
	}

	return scheduler ? scheduler->executing : NULL;
}

/**
 * Start scheduling worklets for the current thread using the associated scheduler. This function
 * never returns.
 *
 * @note A scheduler must have been created for the current thread (@see hnd_scheduler_create).
 */

void
hnd_scheduler_run(void)
{
	hnd_scheduler_t *scheduler;
	osl_ext_event_bits_t events;

	/* Verify that a scheduler is associated to the current thread */
	scheduler = hnd_scheduler_get_inst(NULL);
	if (scheduler == NULL) {
		SCHED_ERROR(("no scheduler associated to thread [%s]\n", CURRENT_THREAD_NAMEPTR));
		ASSERT(FALSE);
		return;
	}

	scheduler_start_period(scheduler);
	scheduler_signal(scheduler);

	/* Execute all available worklets */
	while (1) {

#ifdef ATE_BUILD
		hnd_poll(hnd_sih);
#endif /* ATE_BUILD */

		osl_ext_event_get(&scheduler->events, SCHEDULER_SIGNAL_RUN,
			OSL_EXT_TIME_FOREVER, &events);

		while (scheduler_execute_next_worklet(scheduler)) {
			/* Allow higher priority threads to run */
			osl_ext_task_yield();
		}
	}
}

/**
 * Get the current virtual time of the specified scheduler. Virtual time is expressed in
 * microseconds and increases monotonically only when the scheduler thread is active.
 *
 * @param scheduler	Pointer to scheduler instance.
 * @return		Current virtual time.
 */

osl_ext_time_us_t
hnd_scheduler_get_virtual_time(hnd_scheduler_t *scheduler)
{
	osl_ext_time_us_t time;

	ASSERT(scheduler != NULL);

	time = scheduler->virtual_time;
	if (scheduler->virtual_time_start != 0) {
		time += (scheduler_get_time() - scheduler->virtual_time_start);
	}

	return time;
}

/**
 * Handle a thread switch notification.
 *
 * @param current	Pointer to thread being preemtped.
 * @param next		Pointer to thread being activated.
 */

void
hnd_scheduler_notify_thread(osl_ext_task_t *current, osl_ext_task_t *next)
{
	if (current != NULL) {
		scheduler_preempt_start(current->scheduler);
	}

	ASSERT(next != NULL);
	scheduler_preempt_end(next->scheduler);
}

/**
 * Handle the start of a thread interruption notification.
 *
 * @param current	Pointer to thread being preemtped.
 */

void
hnd_scheduler_notify_isr_enter(osl_ext_task_t *current)
{
	if (current != NULL) {
		scheduler_preempt_start(current->scheduler);
	}
}

/**
 * Handle the end of a thread interruption notification.
 *
 * @param next		Pointer to thread being activated.
 */

void
hnd_scheduler_notify_isr_exit(osl_ext_task_t *next)
{
	if (next != NULL) {
		scheduler_preempt_end(next->scheduler);
	}
}
