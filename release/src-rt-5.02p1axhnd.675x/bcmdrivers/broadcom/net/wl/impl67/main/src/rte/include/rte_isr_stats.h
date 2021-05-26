/*
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: rte_isr_stats.h 779823 2019-10-08 11:09:11Z $
 */

#ifndef	_RTE_ISR_STATS_H
#define	_RTE_ISR_STATS_H

#include <typedefs.h>
#include <osl_ext.h>

typedef struct hnd_stats hnd_stats_t;

typedef enum object_type
{
	OBJ_TYPE_THREAD = 0,			/* Thread */
	OBJ_TYPE_TMR_WORKLET,			/* Worklet associated to timer */
	OBJ_TYPE_INT_WORKLET,			/* Worklet associated with interrupt */
	OBJ_TYPE_WORKLET,			/* Regular worklet */
	OBJ_TYPE_ISR,				/* ISR */
	OBJ_TYPE_SEC_ISR,			/* Secondary ISR */
	OBJ_TYPE_SEC_WORKLET,			/* Secondary worklet */
	OBJ_TYPE_USR,				/* User-defined measurement */
	OBJ_TYPE_LAST
} object_type_t;

#ifdef HND_INTSTATS

typedef enum hnd_stats_notification
{
	NOTIFICATION_TYPE_THREAD_ENTER = 0,	/* Thread switch */
	NOTIFICATION_TYPE_ISR_ENTER,		/* Entering ISR */
	NOTIFICATION_TYPE_ISR_EXIT,		/* Leaving ISR */
	NOTIFICATION_TYPE_TMR_ENTER,		/* Entering timer handler */
	NOTIFICATION_TYPE_TMR_EXIT,		/* Leaving timer handler */
	NOTIFICATION_TYPE_WORKLET_ENTER,	/* Entering worklet other than timer worklet */
	NOTIFICATION_TYPE_WORKLET_EXIT,		/* Leaving worklet other than timer worklet */
	NOTIFICATION_TYPE_QUEUE_ENTER,		/* Worklet enqueue */
	NOTIFICATION_TYPE_QUEUE_EXIT,		/* Worklet dequeue */
	NOTIFICATION_TYPE_USR_ENTER,		/* User-defined measurement start */
	NOTIFICATION_TYPE_USR_EXIT,		/* User-defined measurement end */
	NOTIFICATION_TYPE_LAST
} hnd_stats_notification_t;

typedef enum
{
	INTSTATS_DUMP_ALL	= 0x01,		/* Dump also if runcount is zero */
	INTSTATS_DUMP_USR_ONLY	= 0x02,		/* Only dump user-defined statistics */
	INTSTATS_DUMP_RESET	= 0x20,		/* Reset statistics */
	INTSTATS_DUMP_CYCLES	= 0x40,		/* Show timings in cycles i.s.o. microseconds */
} hnd_stats_dumpflags_t;

typedef enum
{
	INTSTATS_ENAB_INTERVAL	= 0x01,		/* Measure a single interval */
	INTSTATS_ENAB_USR_ONLY	= 0x02,		/* Only measure user-defined statistics */
} hnd_stats_enableflags_t;

/**
 * User-defined measurement of invoke count, duration and interval of a code section:
 *
 * void foo()
 * {
 *	HND_INTSTATS_SAVE_AREA(some_func)
 *
 *	HND_INTSTATS_START(some_func)
 *
 *	some_func();
 *
 *	HND_INTSTATS_END(some_func)
 * }
 *
 * User-defined measurement of invoke count and duration of a code section:
 *
 * void foo()
 * {
 *	HND_INTSTATS_SAVE_AREA(some_func)
 *
 *	HND_INTSTATS_START(some_func)
 *
 *	some_func();
 * }
 *
 * Multiple user-defined measurements:
 *
 * void foo()
 * {
 *	HND_INTSTATS_SAVE_AREA(some_func)
 *	HND_INTSTATS_SAVE_AREA(some_func_actual)
 *
 *	HND_INTSTATS_START(some_func)
 *
 *	if (condition) {
 *		HND_INTSTATS_START(some_func_actual)
 *		some_func();
 *		HND_INTSTATS_END(some_func_actual)
 *	}
 *
 *	HND_INTSTATS_END(some_func)
 * }
 *
 * Measurement results will be added to the output of hnd_isr_stats_dump() as 'USR' entries
 * with identifiers as specified to the HND_INTSTATS_* macros.
 */

#define HND_INTSTATS_NOTIFY(type, context)	hnd_isr_stats_notify(type, context);
#define HND_INTSTATS_SAVE_AREA(name)		static hnd_stats_t *___##name = NULL;
#define HND_INTSTATS_START(name)							\
	if (___##name == NULL && hnd_isr_stats_enabled())				\
	{ ___##name = hnd_isr_stats_create_user(#name, hnd_worklet_get_stats()); }	\
	hnd_isr_stats_notify(NOTIFICATION_TYPE_USR_ENTER, ___##name);
#define HND_INTSTATS_END(name)								\
	hnd_isr_stats_notify(NOTIFICATION_TYPE_USR_EXIT, ___##name);

#else /* HND_INTSTATS */

#define HND_INTSTATS_NOTIFY(type, context)
#define HND_INTSTATS_SAVE_AREA(name)
#define HND_INTSTATS_START(name)
#define HND_INTSTATS_END(name)

#endif /* HND_INTSTATS */

bool hnd_isr_stats_enabled(void);
const char* hnd_isr_stats_get_id(hnd_stats_t *stats);
hnd_stats_t* hnd_isr_stats_create(object_type_t type, const char *id,
	void *target, hnd_stats_t *parent);
hnd_stats_t* hnd_isr_stats_create_user(const char *id, hnd_stats_t *parent);
void hnd_isr_stats_free(hnd_stats_t *stats);

#ifdef HND_INTSTATS
void hnd_isr_stats_notify(uint type, void *context);
void hnd_isr_stats_dump(hnd_stats_dumpflags_t flags);
void hnd_isr_stats_reset(void);
void hnd_isr_stats_enable(bool enable, hnd_stats_enableflags_t flags, osl_ext_time_ms_t interval);
#endif /* HND_INTSTATS */

#endif	/* _RTE_ISR_STATS_H */
