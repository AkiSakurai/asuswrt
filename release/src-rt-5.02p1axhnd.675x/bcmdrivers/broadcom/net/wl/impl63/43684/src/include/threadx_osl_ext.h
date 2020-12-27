/*
 * Threadx OS Support Extension Layer
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
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 * $Id: threadx_osl_ext.h 771484 2019-01-29 09:54:08Z $
 */

#ifndef _threadx_osl_ext_h_
#define _threadx_osl_ext_h_

#ifdef __cplusplus
extern "C" {
#endif // endif

/* ---- Include Files ---------------------------------------------------- */

#include <tx_api.h>
#include <typedefs.h>

/* ---- Constants and Types ---------------------------------------------- */

/* Interrupt control */
#define OSL_INTERRUPT_SAVE_AREA		unsigned int interrupt_save;
#define OSL_DISABLE			interrupt_save = tx_interrupt_control(TX_INT_DISABLE);
#define OSL_RESTORE			tx_interrupt_control(interrupt_save);

/* Debugging helpers */
#define THREAD_NAMEPTR(t)		((t) ? ((TX_THREAD*)(t))->tx_thread_name : NULL)
#define CURRENT_THREAD_NAMEPTR		THREAD_NAMEPTR(osl_ext_task_current())
#define THREAD_ID(t)			((t) ? ((TX_THREAD*)(t))->tx_thread_id : NULL)
#define THREAD_PRIO(t)			((t) ? ((TX_THREAD*)(t))->tx_thread_priority : NULL)
#define CURRENT_THREAD_ID		THREAD_ID(osl_ext_task_current())
#define THREAD_CPUUTIL(t)		((t) ? ((TX_THREAD*)(t))->cpuutil : NULL)
#define CURRENT_THREAD_CPUUTIL		THREAD_CPUUTIL(osl_ext_task_current())
#define THREAD_DPC(t)			((t) ? ((TX_THREAD*)(t))->current_dpc : NULL)
#define CURRENT_THREAD_DPC()		THREAD_DPC(osl_ext_task_current())

#define ASSERT_THREAD(name)		ASSERT((name) == CURRENT_THREAD_NAMEPTR)

/* The default depth of the DPC event queue */
#ifndef OSL_DEFAULT_EVENT_DEPTH
#define OSL_DEFAULT_EVENT_DEPTH		32
#endif // endif

/* This is really platform specific and not OS specific. */
#ifndef BWL_THREADX_TICKS_PER_SECOND
#define BWL_THREADX_TICKS_PER_SECOND	1000
#endif // endif

#define OSL_MSEC_TO_TICKS(msec)		((msec) * 1000)
#define OSL_TICKS_TO_MSEC(ticks)	((ticks) / 1000)
#define OSL_USEC_TO_TICKS(usec)		(usec)
#define OSL_TICKS_TO_USEC(ticks)	(ticks)

/* Semaphore. */
typedef TX_SEMAPHORE osl_ext_sem_t;
#define OSL_EXT_SEM_DECL(sem)		osl_ext_sem_t  sem;

/* Mutex. */
typedef TX_MUTEX osl_ext_mutex_t;
#define OSL_EXT_MUTEX_DECL(mutex)	osl_ext_mutex_t  mutex;

/* Timer. */
typedef TX_TIMER osl_ext_timer_t;
#define OSL_EXT_TIMER_DECL(timer)	osl_ext_timer_t  timer;

/* Task. */
typedef TX_THREAD osl_ext_task_t;
#define OSL_EXT_TASK_DECL(task)		osl_ext_task_t  task;

/* Queue. */
typedef TX_QUEUE osl_ext_queue_t;
#define OSL_EXT_QUEUE_DECL(queue)	osl_ext_queue_t  queue;

/* Event. */
typedef TX_EVENT_FLAGS_GROUP osl_ext_event_t;
#define OSL_EXT_EVENT_DECL(event)	osl_ext_event_t  event;

/* ---- Variable Externs ------------------------------------------------- */
/* ---- Function Prototypes ---------------------------------------------- */

/* XXX: Hack invocation hnd_thread_cpuutil_alloc() of binding a cpuutil context
 * to a task created via osl_ext_task_create_ex().
 * rte_priv.h is not exposed to threadx_osl_ext.h and dngl_mthe.c directly
 * invokes osl_ext_task_create() instead of hnd_task_create().
 *
 * Better soln? Prefer not exposing all of rte_priv.h hnd_cpuutil here.
 */
void *hnd_thread_cpuutil_bind(unsigned int thread_func);

#ifdef __cplusplus
	}
#endif // endif

#endif  /* _threadx_osl_ext_h_  */
