/** \file hnd_isr_stats.c
 *
 * OS independent ISR statistics functions for ISRs and worklets.
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
 * $Id: rte_isr_stats.c 779823 2019-10-08 11:09:11Z $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <rte.h>
#include "rte_priv.h"
#include <rte_isr.h>
#include "rte_isr_priv.h"
#include "rte_isr_stats_priv.h"
#include "rte_scheduler_priv.h"

/*
 *
 * Private data structures
 *
 */

#define INTSTATS_INTERVAL_MIN_MS	1000	/* Minimum statistics measurement time */
#define INTSTATS_INTERVAL_MAX_MS	20000	/* Maximum statistics measurement time */
#define INTSTATS_AVG_COUNT		32	/* Averaging count, must be power of two */
#define INTSTATS_CYCLES_DIV		8	/* Cycle count divider, must be power of two */
#define INTSTATS_CYCLES_SHIFT		3	/* Cycle count divider shift */

#define INTSTATS_OVERFLOW		0xFFFFFFFF

/*
 *
 * Internal Functions
 *
 */

#ifdef HND_INTSTATS

#ifndef BCM_ENABLE_THREAD_CHANGE_NOTIFY
#error "BCM_ENABLE_THREAD_CHANGE_NOTIFY not defined"
#endif /* !BCM_ENABLE_THREAD_CHANGE_NOTIFY */

/**
 * Check if the current measurement interval has elapsed.
 *
 * @param time			Current time.
 */

static void
check_measurement_interval(uint32 time)
{
	uint32 interval_elapsed;

	if (hnd_isr->stats_interval_last == 0) {
		hnd_isr->stats_interval_last = time;
	}

	interval_elapsed = time - hnd_isr->stats_interval_last;
	if (interval_elapsed != 0) {
		uint32 new = hnd_isr->stats_interval_current + interval_elapsed;
		hnd_isr->stats_interval_current = (new < hnd_isr->stats_interval_current) ?
			INTSTATS_OVERFLOW : new;
		if (hnd_isr->stats_interval != 0) {
			if (hnd_isr->stats_interval_current >= hnd_isr->stats_interval) {
				hnd_isr->stats_enable = FALSE;
				printf("statistics ready\n");
			}
		}
		hnd_isr->stats_interval_last = time;
	}
}

/**
 * End previous interval, and start active period and new interval.
 *
 * @param stats			Statistics object.
 */

static inline void
start_period_interval(hnd_stats_t *stats)
{
	if (stats != NULL) {
		/* End previous interval */
		if (stats->interval_enter != 0) {
			uint32 elapsed = hnd_isr->stats_scaled_time - stats->interval_enter;
			hnd_isr_stats_value_update(&stats->interval, elapsed);
		}

		/* Start new period and interval */
		stats->active_enter = stats->interval_enter = hnd_isr->stats_scaled_time;
		stats->accumulated_time = 0;
		stats->is_active = TRUE;
		stats->count++;
	}
}

/**
 * Pause active period.
 *
 * @param stats			Statistics object.
 */

static inline void
pause_period(hnd_stats_t *stats)
{
	if (stats != NULL && stats->is_active) {
		stats->accumulated_time += hnd_isr->stats_scaled_time - stats->active_enter;
	}
}

/**
 * Resume active period.
 *
 * @param stats			Statistics object.
 */

static inline void
resume_period(hnd_stats_t *stats)
{
	if (stats != NULL && stats->is_active) {
		stats->active_enter = hnd_isr->stats_scaled_time;
	}
}

/**
 * End active period.
 *
 * @param stats			Statistics object.
 */

static inline void
end_period(hnd_stats_t *stats)
{
	if (stats != NULL && stats->is_active) {
		uint32 elapsed = hnd_isr->stats_scaled_time - stats->active_enter;
		elapsed += stats->accumulated_time;
		hnd_isr_stats_value_update(&stats->active, elapsed);
		stats->is_active = FALSE;
	}
}

static uint32
convert_value(uint32 cycles, uint divider, hnd_stats_dumpflags_t flags)
{
	uint32 value = INTSTATS_OVERFLOW;

	cycles = (divider != 0 ? cycles / divider : 0);

	if (flags & INTSTATS_DUMP_CYCLES) {
		if (cycles <= 0xFFFFFFFF / INTSTATS_CYCLES_DIV) {
			value = cycles * INTSTATS_CYCLES_DIV;
		}
	} else if (cycles != INTSTATS_OVERFLOW) {
		if (cycles <= 0xFFFFFFFF / INTSTATS_CYCLES_DIV) {
			value = (cycles * INTSTATS_CYCLES_DIV) / c0counts_per_us;
		} else {
			value = cycles / (c0counts_per_us / INTSTATS_CYCLES_DIV);
		}
	}

	return value;
}

static void
dump_stats_single(hnd_stats_t *stats, uint level, hnd_stats_dumpflags_t flags, uint interval)
{
	static const char *types[] = { "    THREAD", "    TMR_WORKLET", "    INT_WORKLET",
				       "    USR_WORKLET", "    ISR", "    SEC_ISR",
				       "    SEC_WORKLET", "    USR" };

	uint percent = interval >= 10000 ? (stats->active.total / (interval / 10000)) : 0;

	level = (level > 4 ? 0 : 4 - level);

	printf("%-14s %10u %10u %3u.%02u%% %10u %10u %10u %10u %10u %10u %10u %10u %10u [%p] %s\n",
		stats->type < OBJ_TYPE_LAST ? types[stats->type] + level : "",
		stats->count,
		convert_value(stats->active.total, 1, flags),
		percent / 100, percent % 100,
		convert_value(stats->active.min, 1, flags),
		convert_value(stats->active.avg, stats->active.avg_count, flags),
		convert_value(stats->active.max, 1, flags),
		convert_value(stats->interval.min, 1, flags),
		convert_value(stats->interval.avg, stats->interval.avg_count, flags),
		convert_value(stats->interval.max, 1, flags),
		convert_value(stats->queued.min, 1, flags),
		convert_value(stats->queued.avg, stats->queued.avg_count, flags),
		convert_value(stats->queued.max, 1, flags),
		stats->target,
		stats->id ? stats->id : "");
}

static void
dump_stats_internal(hnd_stats_t *stats, uint level, hnd_stats_dumpflags_t flags, uint interval)
{
	hnd_stats_t *cur;

	if ((flags & INTSTATS_DUMP_ALL) || stats->count != 0) {
		dump_stats_single(stats, level, flags, interval);
		for (cur = hnd_isr->stats_list; cur; cur = cur->next) {
			if (cur->reserved == 1 && cur->parent == stats) {
				cur->reserved = 0;
				dump_stats_internal(cur, level+1, flags, interval);
			}
		}
	}
	stats->reserved = 0;
}

#endif /* HND_INTSTATS */

/*
 *
 * Export functions
 *
 */

/**
 * Get statistics gathering status.
 *
 * @return			TRUE if active.
 */

inline bool
hnd_isr_stats_enabled(void)
{
#ifdef HND_INTSTATS
	return hnd_isr != NULL && hnd_isr->stats_enable;
#else
	return FALSE;
#endif /* HND_INTSTATS */
}

/**
 * Return the id associated to the specified statistics object.
 *
 * @param stats		Statistics object.
 * @return		Id, or NULL if not available.
 */

const char*
hnd_isr_stats_get_id(hnd_stats_t *stats)
{
#ifdef HND_INTSTATS
	return stats != NULL && stats->id != NULL ? stats->id : "";
#else
	return "";
#endif /* HND_INTSTATS */
}

/**
 * Create a statistics object.
 *
 * @param type			Type of associated object.
 * @param id			Pointer to object identifier string, or NULL.
 * @param target		Associated object.
 * @param parent		Parent statistics object.
 * @returns			New statistics object.
 */

hnd_stats_t*
hnd_isr_stats_create(object_type_t type, const char *id, void *target, hnd_stats_t *parent)
{
#ifdef HND_INTSTATS
	hnd_stats_t *stats;

	ASSERT(hnd_in_isr() == FALSE);

	stats = (hnd_stats_t*)MALLOCZ(hnd_osh, sizeof(hnd_stats_t));
	if (stats != NULL) {
		stats->type   = type;
		stats->id     = id;
		stats->target = target;
		stats->parent = parent;

		ASSERT(hnd_isr != NULL);
		stats->next = hnd_isr->stats_list;
		hnd_isr->stats_list = stats;
	}

	return stats;
#else
	return NULL;
#endif /* HND_INTSTATS */
}

/**
 * Create a User-defined statistics object.
 *
 * @param id			Pointer to object identifier string, or NULL.
 * @param parent		Parent object, or NULL.
 * @return			Created statistics object, or NULL on error.
 */

hnd_stats_t*
hnd_isr_stats_create_user(const char *id, hnd_stats_t *parent)
{
	return hnd_isr_stats_create(OBJ_TYPE_USR, id, NULL, parent);
}

/**
 * Free a statistics object.
 *
 * @param stats			Statistics object to deregister.
 */

void
hnd_isr_stats_free(hnd_stats_t *stats)
{
#ifdef HND_INTSTATS
	hnd_stats_t *cur;

	ASSERT(stats != NULL);
	ASSERT(hnd_in_isr() == FALSE);

	cur = hnd_isr->stats_list;
	if (stats != cur) {
		while (cur != NULL) {
			if (cur->next == stats) {
				cur->next = stats->next;
				MFREE(hnd_osh, stats, sizeof(hnd_stats_t));
				break;
			}
			cur = cur->next;
		}
	} else {
		hnd_isr->stats_list = stats->next;
		MFREE(hnd_osh, stats, sizeof(hnd_stats_t));
	}
#endif /* HND_INTSTATS */
}

/**
 * Update a statistics value.
 *
 * @param value			Statistics value to update.
 * @param elapsed		Elapsed time.
 */

void
hnd_isr_stats_value_update(hnd_stats_val_t *value, uint32 elapsed)
{
#ifdef HND_INTSTATS
	uint32 prev_value = value->total;

	value->total += elapsed;
	if (value->total < prev_value) {
		value->total = INTSTATS_OVERFLOW;
	}
	if (elapsed > value->max) {
		value->max = elapsed;
	}
	if (value->min == 0 || elapsed < value->min) {
		value->min = elapsed;
	}
	if (value->avg_count == INTSTATS_AVG_COUNT) {
		value->avg -= (value->avg / INTSTATS_AVG_COUNT);
	} else {
		value->avg_count++;
	}

	prev_value = value->avg;
	value->avg += elapsed;
	if (value->avg < prev_value) {
		value->avg = value->avg_count = 0;
	}
#endif /* HND_INTSTATS */
}

INLINE uint32
hnd_isr_stats_value_avg(hnd_stats_val_t *value)
{
#ifdef HND_INTSTATS
	return value->avg_count ? value->avg / value->avg_count : 0;
#endif /* HND_INTSTATS */
	return 0;
}

/**
 * Reset a statistics value.
 *
 * @param value			Statistics value to reset.
 */

INLINE void
hnd_isr_stats_value_reset(hnd_stats_val_t *value)
{
#ifdef HND_INTSTATS
	memset(value, 0, sizeof(hnd_stats_val_t));
#endif /* HND_INTSTATS */
}

#ifdef HND_INTSTATS

/**
 * Handle a context switch notification.
 *
 * @param type			Notification type.
 * @param context		Additional context data.
 */

void
hnd_isr_stats_notify(hnd_stats_notification_t type, void *context)
{
	hnd_worklet_t *current_worklet;
	osl_ext_task_t *thread;
	uint32 time, elapsed;
	hnd_stats_t *stats = (hnd_stats_t*)context;

	TX_INTERRUPT_SAVE_AREA

	if (hnd_isr_stats_enabled() == FALSE) {
		return;
	}

	TX_DISABLE

	OSL_GETCYCLES(time);

	/* Create virtual clock to increase range */
	hnd_isr->stats_accumulated_cycles += time - hnd_isr->stats_cycles_last;
	hnd_isr->stats_scaled_time += hnd_isr->stats_accumulated_cycles >> INTSTATS_CYCLES_SHIFT;
	hnd_isr->stats_accumulated_cycles &= INTSTATS_CYCLES_DIV - 1;
	hnd_isr->stats_cycles_last = time;

	switch (type) {
	case NOTIFICATION_TYPE_THREAD_ENTER:
		if (hnd_isr->stats_usr_only) {
			break;
		}
		thread = hnd_get_last_thread();
		if (thread != NULL) {
			end_period((hnd_stats_t*)thread->stats);	/* Previous thread */

			current_worklet = THREAD_WORKLET(thread);	/* Preempted worklet */
			if (current_worklet != NULL) {
				pause_period(current_worklet->stats);
			}
		}

		thread = (osl_ext_task_t*)context;
		ASSERT(thread != NULL);

		current_worklet = THREAD_WORKLET(thread);		/* Scheduled worklet */
		if (current_worklet != NULL) {
			resume_period(current_worklet->stats);
		}
		start_period_interval((hnd_stats_t*)thread->stats);	/* Current thread */
		break;
	case NOTIFICATION_TYPE_ISR_ENTER:
		if (hnd_isr->stats_usr_only) {
			break;
		}
		ASSERT(stats != NULL);
		thread = osl_ext_task_current();
		if (thread != NULL) {
			pause_period(thread->stats);			/* Preempted thread */

			current_worklet = THREAD_WORKLET(thread);	/* Preempted worklet */
			if (current_worklet != NULL) {
				pause_period(current_worklet->stats);
			}
		}
		start_period_interval(stats);				/* ISR */
		break;
	case NOTIFICATION_TYPE_ISR_EXIT:
		if (hnd_isr->stats_usr_only) {
			break;
		}
		ASSERT(stats != NULL);
		end_period(stats);					/* ISR */

		thread = osl_ext_task_current();
		if (thread != NULL) {
			current_worklet = THREAD_WORKLET(thread);	/* Preempted worklet */
			if (current_worklet != NULL) {
				resume_period(current_worklet->stats);
			}
			resume_period(thread->stats);			/* Preempted thread */
		}
		break;
	case NOTIFICATION_TYPE_TMR_ENTER:
	case NOTIFICATION_TYPE_WORKLET_ENTER:
		if (hnd_isr->stats_usr_only == FALSE) {
			start_period_interval(stats);			/* Timer/Worklet */
		}
		break;
	case NOTIFICATION_TYPE_USR_ENTER:
		start_period_interval(stats);				/* USR */
		break;
	case NOTIFICATION_TYPE_TMR_EXIT:
	case NOTIFICATION_TYPE_WORKLET_EXIT:
		if (hnd_isr->stats_usr_only == FALSE) {
			end_period(stats);				/* Timer/Worklet */
		}
		check_measurement_interval(hnd_isr->stats_scaled_time);
		break;
	case NOTIFICATION_TYPE_USR_EXIT:
		end_period(stats);					/* USR */
		check_measurement_interval(hnd_isr->stats_scaled_time);
		break;
	case NOTIFICATION_TYPE_QUEUE_ENTER:
		ASSERT(stats != NULL);
		stats->queue_enter = hnd_isr->stats_scaled_time;	/* Enqueued worklet */
		break;
	case NOTIFICATION_TYPE_QUEUE_EXIT:
		ASSERT(stats != NULL);
		if (hnd_isr->stats_usr_only == FALSE) {
			if (stats->queue_enter != 0) {			/* Dequeued worklet */
				elapsed = hnd_isr->stats_scaled_time - stats->queue_enter;
				hnd_isr_stats_value_update(&stats->queued, elapsed);
			}
		}
		break;
	default:
		break;
	}

	TX_RESTORE
}

/**
 * Dump all statistics.
 *
 * @param flags			Dump flags.
 */

void
hnd_isr_stats_dump(hnd_stats_dumpflags_t flags)
{
	hnd_stats_t *stats;
	uint32 interval;
	uint64 total_active = 0;

	printf_suppress_timestamp(TRUE);

	if (hnd_isr->stats_interval != 0) {
		if (hnd_isr->stats_interval_current < hnd_isr->stats_interval) {
			printf("[busy]\n");
			return;
		}
	}

	if (hnd_isr->stats_enable == FALSE) {
		printf("[disabled] ");
	}

	if (hnd_isr->stats_usr_only == TRUE) {
		printf("[usr only] ");
	}

	interval = convert_value(hnd_isr->stats_interval_current, 1, flags);
	if ((flags & INTSTATS_DUMP_CYCLES) && interval == INTSTATS_OVERFLOW) {
		interval = hnd_isr->stats_interval_current;
		printf("[interval:%u *%u] ", hnd_isr->stats_interval_current, INTSTATS_CYCLES_DIV);
	} else {
		printf("[interval:%u] ", interval);
	}

	printf("[units:%s]\n", (flags & INTSTATS_DUMP_CYCLES) ? "cycles" : "us");

	printf("TYPE             RUNCOUNT  ACT_TOTAL PERCENT    "
		"ACT_MIN    ACT_AVG    ACT_MAX    "
		"INT_MIN    INT_AVG    INT_MAX  "
		"QUEUE_MIN  QUEUE_AVG  QUEUE_MAX TARGET\n");

	for (stats = hnd_isr->stats_list; stats; stats = stats->next) {
		stats->reserved = 1;
		if (stats->type == OBJ_TYPE_THREAD || stats->type == OBJ_TYPE_ISR) {
			total_active += stats->active.total;
		}
	}

	if (flags & INTSTATS_DUMP_USR_ONLY) {
		for (stats = hnd_isr->stats_list; stats; stats = stats->next) {
			if ((flags & INTSTATS_DUMP_ALL) || stats->count != 0) {
				if (stats->type == OBJ_TYPE_USR) {
					dump_stats_single(stats, 0, flags,
						total_active <= 0xffffffff ? total_active : 0);
				}
			}
		}
	} else {
		for (stats = hnd_isr->stats_list; stats; stats = stats->next) {
			if (stats->reserved == 1 && stats->parent == NULL) {
				dump_stats_internal(stats, 0, flags,
					total_active <= 0xffffffff ? total_active : 0);
			}
		}
	}

	if (flags & INTSTATS_DUMP_RESET) {
		hnd_isr_stats_reset();
	}

	printf_suppress_timestamp(FALSE);
}

/**
 * Reset all statistics.
 */

void
hnd_isr_stats_reset(void)
{
	OSL_INTERRUPT_SAVE_AREA

	hnd_stats_t *stats;

	OSL_DISABLE

	for (stats = hnd_isr->stats_list; stats; stats = stats->next) {
		stats->is_active = stats->count = stats->accumulated_time = 0;
		stats->active_enter = stats->interval_enter = stats->queue_enter = 0;
		hnd_isr_stats_value_reset(&stats->active);
		hnd_isr_stats_value_reset(&stats->interval);
		hnd_isr_stats_value_reset(&stats->queued);
	}

	hnd_isr->stats_interval_current = 0;
	hnd_isr->stats_interval_last = 0;
	hnd_isr->stats_interval = 0;
	hnd_isr->stats_accumulated_cycles = 0;
	hnd_isr->stats_scaled_time = 0;

	OSL_RESTORE
}

/**
 * Enable or disable statistics gathering.
 *
 * @param enable		TRUE to enable. Enabling will also reset statistics.
 * @param flags			Enable flags.
 * @param interval		Interval measurement duration, or 0.
 */

void
hnd_isr_stats_enable(bool enable, hnd_stats_enableflags_t flags, osl_ext_time_ms_t interval)
{
	OSL_INTERRUPT_SAVE_AREA

	interval = (interval < INTSTATS_INTERVAL_MIN_MS ? INTSTATS_INTERVAL_MIN_MS : interval);
	interval = (interval > INTSTATS_INTERVAL_MAX_MS ? INTSTATS_INTERVAL_MAX_MS : interval);

	OSL_DISABLE

	hnd_isr->stats_enable = enable;
	if (enable) {
		hnd_isr_stats_reset();
		hnd_isr->stats_usr_only = !!(flags & INTSTATS_ENAB_USR_ONLY);
		if (flags & INTSTATS_ENAB_INTERVAL) {
			hnd_isr->stats_interval = (c0counts_per_ms/INTSTATS_CYCLES_DIV) * interval;
		}
	}

	OSL_RESTORE
}

#endif /* HND_INTSTATS */
