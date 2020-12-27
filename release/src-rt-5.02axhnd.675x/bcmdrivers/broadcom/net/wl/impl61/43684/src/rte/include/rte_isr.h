/*
 * OS independent ISR functions for ISRs or DPCs.
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
 * $Id: rte_isr.h 774076 2019-04-09 12:13:18Z $
 */

#ifndef	_RTE_ISR_H
#define	_RTE_ISR_H

#include <typedefs.h>
#include <osl_ext.h>

typedef enum { DPC_NOT_SCHEDULED, DPC_SCHEDULED, DPC_CANCELLED } dpc_state_t;

typedef struct hnd_isr_action hnd_isr_action_t;
typedef void (*hnd_isr_fn_t)(void *cbdata);
typedef void (*hnd_dpc_fn_t)(void *cbdata);

typedef struct hnd_sec_isr_action hnd_sec_isr_action_t;
typedef void (*hnd_sec_isr_fn_t)(void *cbdata, uint32 intstatus);
typedef void (*hnd_sec_dpc_fn_t)(void *cbdata, uint32 intstatus);

typedef struct hnd_dpc hnd_dpc_t;

typedef bool (*hnd_schedule_dpc_fn_t)(hnd_dpc_t *dpc);

typedef struct hnd_stats hnd_stats_t;

typedef enum object_type
{
	OBJ_TYPE_THREAD = 0,			/* Thread */
	OBJ_TYPE_TDPC,				/* DPC associated to timer */
	OBJ_TYPE_IDPC,				/* DPC associated with interrupt */
	OBJ_TYPE_DPC,				/* Regular DPC */
	OBJ_TYPE_ISR,				/* ISR */
	OBJ_TYPE_SEC_ISR,			/* Secondary ISR */
	OBJ_TYPE_SEC_DPC,			/* Secondary DPC */
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
	NOTIFICATION_TYPE_DPC_ENTER,		/* Entering DPC other than timer DPC */
	NOTIFICATION_TYPE_DPC_EXIT,		/* Leavin DPC other than timer DPC */
	NOTIFICATION_TYPE_QUEUE_ENTER,		/* DPC enqueue */
	NOTIFICATION_TYPE_QUEUE_EXIT,		/* DPC dequeue */
	NOTIFICATION_TYPE_USR_ENTER,		/* User-defined measurement start */
	NOTIFICATION_TYPE_USR_EXIT,		/* User-defined measurement end */
	NOTIFICATION_TYPE_LAST
} hnd_stats_notification_t;

typedef struct hnd_stats_val
{
	uint8		avg_count;
	uint8		unused1;
	uint16		unused2;
	uint32		total;
	uint32		min;
	uint32		max;
	uint32		avg;
} hnd_stats_val_t;

struct hnd_stats
{
	hnd_stats_t	*next;			/* List of all statistics */
	hnd_stats_t	*parent;		/* Parent statistics, or NULL */
	const char	*id;			/* Statistics identifier */
	void		*target;		/* Target function */
	object_type_t	type;			/* Statistics type */
	uint8		reserved;		/* Internal use */
	uint8		is_active;		/* Measure active time flag */
	uint32		count;			/* Invoke counter */
	uint32		accumulated_time;	/* Active time accumulated time */
	uint32		active_enter;		/* Time of active period start */
	uint32		interval_enter;		/* Time of interval start */
	uint32		queue_enter;		/* Time of enqueuing */

	hnd_stats_val_t	active;
	hnd_stats_val_t interval;
	hnd_stats_val_t queued;
};

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
 * Measurement results will be added to the output of hnd_isr_dump_stats() as 'USR' entries
 * with identifiers as specified to the HND_INTSTATS_* macros.
 */

#define HND_INTSTATS_NOTIFY(type, context)	hnd_isr_stats_notify(type, context);
#define HND_INTSTATS_SAVE_AREA(name)		static hnd_stats_t *___##name = NULL;
#define HND_INTSTATS_START(name)						\
	if (___##name == NULL && hnd_isr_stats_enabled())			\
	{ ___##name = hnd_isr_stats_create_user(#name); }			\
	hnd_isr_stats_notify(NOTIFICATION_TYPE_USR_ENTER, ___##name);
#define HND_INTSTATS_END(name)							\
	hnd_isr_stats_notify(NOTIFICATION_TYPE_USR_EXIT, ___##name);

#else /* HND_INTSTATS */

#define HND_INTSTATS_NOTIFY(type, context)
#define HND_INTSTATS_SAVE_AREA(name)
#define HND_INTSTATS_START(name)
#define HND_INTSTATS_END(name)

#endif /* HND_INTSTATS */

struct hnd_dpc
{
	dpc_state_t	state;			/* DPC pending state */
	bool		executing;		/* DPC execution state */
	bool		deleted;		/* DPC deleted flag */
	hnd_dpc_fn_t	dpc_fn;			/* DPC callback function */
	void		*cbdata;		/* DPC data */
	osl_ext_task_t	*thread;		/* DPC target thread context */
	void		*cpuutil;		/* CPU Util context */
	hnd_stats_t	*stats;			/* DPC statistics */
};

#define hnd_dpc_create(dpc_fn, cbdata, thread)	\
	hnd_dpc_create_internal(dpc_fn, cbdata, thread, OBJECT_ID(dpc_fn))
hnd_dpc_t* hnd_dpc_create_internal(hnd_dpc_fn_t dpc_fn, void *cbdata,
	osl_ext_task_t *thread, const char *id);
void hnd_dpc_free(hnd_dpc_t *dpc);
void hnd_dpc_free_internal(hnd_dpc_t *dpc);
bool hnd_dpc_cancel(hnd_dpc_t *dpc);
bool hnd_dpc_schedule(hnd_dpc_t *dpc);
bool hnd_dpc_reschedule(void);
bool hnd_dpc_schedule_internal(hnd_dpc_t *dpc);
bool hnd_dpc_is_pending_or_executing(hnd_dpc_t *dpc);
bool hnd_dpc_is_pending(hnd_dpc_t *dpc);
bool hnd_dpc_is_executing(hnd_dpc_t *dpc);
bool hnd_in_isr(void);
osl_ext_task_t* hnd_get_last_thread(void);
hnd_dpc_t* hnd_dpc_for_isr(hnd_isr_action_t *isr);

#define hnd_isr_register(irq, coreid, unit, isr_fn, cbdata_isr, dpc_fn, cbdata_dpc, thread, bus) \
	hnd_isr_register_internal((irq), (coreid), (unit), (isr_fn), (cbdata_isr),		\
	(dpc_fn), (cbdata_dpc), (thread), (bus), OBJECT_ID(isr_fn), OBJECT_ID(dpc_fn))
hnd_isr_action_t* hnd_isr_register_internal(uint irq, uint coreid, uint unit, hnd_isr_fn_t isr_fn,
	void *cbdata_isr, hnd_dpc_fn_t dpc_fn, void *cbdata_dpc, osl_ext_task_t *thread, uint bus,
	const char *isr_id, const char *dpc_id);

#define hnd_isr_register_with_dpc(irq, coreid, unit, isr_fn, cbdata, dpc, bus)			\
	hnd_isr_register_with_dpc_internal((irq), (coreid), (unit), (isr_fn), (cbdata), (dpc),	\
	(bus), OBJECT_ID(isr_fn))
hnd_isr_action_t* hnd_isr_register_with_dpc_internal(uint irq, uint coreid, uint unit,
	hnd_isr_fn_t isr_fn, void *cbdata, hnd_dpc_t* dpc, uint bus, const char *id);

#define hnd_isr_register_n(irq, isr_num, isr_fn, cbdata_isr, dpc_fn, cbdata_dpc, thread)	\
	hnd_isr_register_n_internal((irq), (isr_num), (isr_fn), (cbdata_isr), (dpc_fn),		\
	(cbdata_dpc), (thread), OBJECT_ID(isr_fn), OBJECT_ID(dpc_fn))
hnd_isr_action_t* hnd_isr_register_n_internal(uint irq, uint isr_num, hnd_isr_fn_t isr_fn,
	void *cbdata_isr, hnd_dpc_fn_t dpc_fn, void *cbdata_dpc, osl_ext_task_t* thread,
	const char *isr_id, const char *dpc_id);

#define hnd_sec_isr_register(isr, intmask, isr_fn, dpc_fn, cbdata)				\
	hnd_sec_isr_register_internal((isr), (intmask), (isr_fn), (dpc_fn), (cbdata),		\
	OBJECT_ID(isr_fn), OBJECT_ID(dpc_fn))
bool hnd_sec_isr_register_internal(hnd_isr_action_t *isr, uint32 intmask, hnd_sec_isr_fn_t isr_fn,
	hnd_sec_dpc_fn_t dpc_fn, void *cbdata, const char *isr_id, const char *dpc_id);

void hnd_sec_isr_run(hnd_isr_action_t *isr, uint32 intstatus);
void hnd_sec_dpc_run(hnd_isr_action_t *isr, uint32 intstatus);

bool hnd_isr_stats_enabled(void);
hnd_stats_t* hnd_isr_stats_create(object_type_t type, const char *id,
	void *target, hnd_stats_t *parent);
void hnd_isr_stats_free(hnd_stats_t *stats);

#ifdef HND_INTSTATS
void hnd_isr_stats_notify(uint type, void *context);
void hnd_isr_stats_fixup(hnd_stats_t *parent);
hnd_stats_t* hnd_isr_stats_create_user(const char *id);
void hnd_isr_dump_stats(hnd_stats_dumpflags_t flags);
void hnd_isr_reset_stats(void);
void hnd_isr_enable_stats(bool enable, hnd_stats_enableflags_t flags, osl_ext_time_ms_t interval);
#endif /* HND_INTSTATS */

#endif	/* _RTE_ISR_H */
