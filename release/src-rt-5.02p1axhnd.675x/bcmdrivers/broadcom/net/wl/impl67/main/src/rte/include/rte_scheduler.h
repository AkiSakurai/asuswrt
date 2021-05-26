/** \file rte_scheduler.h
 *
 * OS independent worklet scheduler functions.
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
 * $Id: rte_scheduler.h 781914 2019-12-04 11:33:00Z $
 */

#ifndef	_RTE_SCHEDULER_H
#define	_RTE_SCHEDULER_H

#include <typedefs.h>
#include <rte_isr_stats.h>

typedef uint8 priority_t;
typedef uint32 quota_t;

typedef struct scheduler hnd_scheduler_t;
typedef struct worklet hnd_worklet_t;

typedef bool (*hnd_worklet_fn_t)(void *cbdata);

#define SCHEDULER_MAX_PRIORITY			3	/* Max. (lowest) allowed priority value */

#define SCHEDULER_DEFAULT_PRIORITY		1
#define SCHEDULER_DEFAULT_QUOTA			1024

#define SCHEDULER_DEFAULT_TIMER_PRIORITY	SCHEDULER_DEFAULT_PRIORITY
#define SCHEDULER_DEFAULT_TIMER_QUOTA		SCHEDULER_DEFAULT_QUOTA

#define SCHEDULER_DEFAULT_INT_PRIORITY		SCHEDULER_DEFAULT_PRIORITY
#define SCHEDULER_DEFAULT_INT_QUOTA		SCHEDULER_DEFAULT_QUOTA

typedef enum {
	STOP_REASON_NONE			= 0,	/* Normal completion of worklet */
	STOP_REASON_QUOTA			= 1,	/* Quota exceeded */
	STOP_REASON_PRIORITY			= 2,	/* Higher priority worklet available */
	STOP_REASON_CANCELLED			= 3	/* Cancelled through API */
} stop_reason_t;

/* Public interface */

#define hnd_worklet_create(worklet_fn, cbdata, priority, quota, thread) \
	hnd_worklet_create_internal(worklet_fn, cbdata, priority, quota, \
	thread, OBJECT_ID(worklet_fn))
hnd_worklet_t* hnd_worklet_create_internal(hnd_worklet_fn_t worklet_fn, void *cbdata,
	priority_t priority, osl_ext_time_us_t quota, osl_ext_task_t *thread, const char *id);

bool hnd_worklet_is_executing(hnd_worklet_t *worklet);
bool hnd_worklet_is_pending(hnd_worklet_t *worklet);
bool hnd_worklet_is_pending_or_executing(hnd_worklet_t *worklet);
hnd_worklet_t* hnd_worklet_find_by_name(hnd_scheduler_t *scheduler, const char *name);
hnd_scheduler_t* hnd_worklet_get_scheduler(hnd_worklet_t *worklet);
osl_ext_task_t* hnd_worklet_get_thread(hnd_worklet_t *worklet);
hnd_worklet_t* hnd_worklet_get_current(void);
hnd_stats_t* hnd_worklet_get_stats(void);
const char* hnd_worklet_get_id(void);
osl_ext_time_us_t hnd_worklet_get_virtual_time(void);
void hnd_worklet_schedule(hnd_worklet_t *worklet);
void hnd_worklet_reschedule(void);
bool hnd_worklet_delete(hnd_worklet_t *worklet);
bool hnd_worklet_cancel(hnd_worklet_t *worklet);
bool hnd_worklet_should_stop(stop_reason_t *reason);
stop_reason_t hnd_worklet_prev_stop_reason(void);

#define hnd_scheduler_create(thread) \
	hnd_scheduler_create_internal(thread, OBJECT_ID(thread))
hnd_scheduler_t* hnd_scheduler_create_internal(osl_ext_task_t *thread, const char *id);
hnd_worklet_t* hnd_scheduler_get_worklet(hnd_scheduler_t *scheduler);
void hnd_scheduler_run(void);

#endif	/* _RTE_SCHEDULER_H */
