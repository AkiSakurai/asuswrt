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
 * $Id: dngl_mthe.c 787482 2020-06-01 09:20:40Z $
 */

/* DoNGLe Main THreads Entries */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <osl_ext.h>
#include <bcmutils.h>
#include <rte.h>
#include <rte_isr.h>
#include <rte_scheduler.h>

#include <tx_api.h>
#include <tx_thread.h>

/* ======== Common threads ======== */

/* Thread handles */
static osl_ext_task_t main_thread;
static osl_ext_task_t idle_thread;

/* Watchdog */
#ifndef WATCHDOG_INTERVAL_MS
#define WATCHDOG_INTERVAL_MS		0	/* Disable */
#endif /* WATCHDOG_INTERVAL_MS */

#ifndef WATCHDOG_TIMEOUT_MS
#define WATCHDOG_TIMEOUT_MS		500
#endif /* WATCHDOG_TIMEOUT_MS */

static void user_application_define(void);

#ifdef THREADX_BTCM_STACK
extern uint32 _stackbottom;
static CHAR *main_thread_stack = NULL;
#else
static CHAR main_thread_stack[HND_STACK_SIZE] DECLSPEC_ALIGN(16);
#endif /* THREADX_BTCM_STACK */
static CHAR idle_thread_stack[IDLE_STACK_SIZE] DECLSPEC_ALIGN(16);

static osl_ext_task_t *
BCMRAMFN(get_main_thread)(void)
{
	return &main_thread;
}

static osl_ext_task_t *
BCMRAMFN(get_idle_thread)(void)
{
	return &idle_thread;
}

static CHAR *
BCMRAMFN(get_main_thread_stack)(void)
{
#ifdef THREADX_BTCM_STACK
	main_thread_stack = (CHAR *)_stackbottom;
#endif /* THREADX_BTCM_STACK */
	return main_thread_stack;
}

static CHAR *
BCMRAMFN(get_idle_thread_stack)(void)
{
	return idle_thread_stack;
}

/**
 * Idle thread entry function.
 *
 * This function is called in the context of the idle thread and runs at idle priority.
 *
 * @param arg			Not used.
 */

static void
idle_thread_entry(osl_ext_task_arg_t arg)
{
	si_t *sih = (si_t *)arg;

	hnd_idle(sih);
}

/**
 * Main thread entry function.
 *
 * This function is called in the context of the main thread and initially runs at the highest
 * priority to avoid additional threads created during system initialization being scheduled
 * before initialization is completed.
 *
 * @param arg			Not used.
 */

static void
main_thread_entry(osl_ext_task_arg_t arg)
{
	si_t *sih;
	osl_ext_task_t *thread = osl_ext_task_current();

	/* Initialize system. */
	sih = _c_main();

	/* A scheduler for the main thread should have been created during initialization. */
	ASSERT(thread->scheduler != NULL);

	/* Create idle thread at lowest priority */
	if (osl_ext_task_create("idle_thread",
		get_idle_thread_stack(), sizeof(idle_thread_stack),
		OSL_EXT_TASK_IDLE_PRIORITY, idle_thread_entry,
		(osl_ext_task_arg_t)sih, get_idle_thread()) != OSL_EXT_SUCCESS) {
			ASSERT(0);
			return;
	}

	/* Create idle thread statistics object */
	get_idle_thread()->stats = hnd_isr_stats_create(OBJ_TYPE_THREAD, "idle_thread",
		(void*)idle_thread_entry, NULL);

	/* Call user's initialization function */
	user_application_define();

#ifdef THREADX_WATCHDOG
	/* Start the main watchdog. */
	if (WATCHDOG_INTERVAL_MS != 0) {
		hnd_thread_watchdog_init(thread, WATCHDOG_INTERVAL_MS, WATCHDOG_TIMEOUT_MS);
	} else {
		printf("Watchdog DISABLED\n");
	}
#endif /* THREADX_WATCHDOG */

	/* Initialize ThreadX port */
	hnd_thread_initialize();

	/* Enable system (interrupt etc.) now */
	hnd_sys_enab(sih);

	printf("ThreadX v%d.%d initialized\n",
		__THREADX_MAJOR_VERSION, __THREADX_MINOR_VERSION);

	/* Lower the thread priority */
	osl_ext_task_priority_change(thread, OSL_EXT_TASK_NORMAL_PRIORITY, NULL);

	/* Start processing events. */
	hnd_thread_default_implementation(NULL);
}

/**
 * ThreadX application initialization.
 *
 * This function is called as part of ThreadX initialization. It runs with all interrupts
 * disabled, before the system is initialized and before the OS scheduler is started. Initial
 * application resources (threads, queues, locks, event flags, timers) can be defined here.
 *
 * @param first_unused_memory	Pointer to first unused memory.
 *
 * @note: no MALLOC, no printf.
 */

void
BCMATTACHFN(tx_application_define)(void *first_unused_memory)
{
	osl_ext_task_enable_stack_check();

	/* Create the main thread, which will do all system initialization and calls
	 * @see user_application_define just before enabling global interrupts.
	 */
	if (osl_ext_task_create("main_thread", get_main_thread_stack(), HND_STACK_SIZE,
		OSL_EXT_TASK_TIME_CRITICAL_PRIORITY, main_thread_entry, NULL,
		get_main_thread()) != OSL_EXT_SUCCESS) {
			ASSERT(0);
			return;
	}
}
