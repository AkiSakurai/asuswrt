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
 * $Id: threadx.c 774076 2019-04-09 12:13:18Z $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <osl_ext.h>
#include <bcmutils.h>
#include <sbchipc.h>
#include <hndsoc.h>
#include <rte_isr.h>
#include "rte_isr_priv.h"
#include "rte_pmu_priv.h"
#include <rte.h>
#include "rte_priv.h"
#include <rte_mem.h>
#include <rte_cons.h>
#include <rte_timer.h>
#include <bcmstdlib_ext.h>
#include <hndcpu.h>
#include <hndarm_gt.h>

#include <tx_api.h>
#include <tx_initialize.h>

#include <tx_thread.h>
#include "threadx_priv.h"
#include <threadx_low_power.h>
#include "threadx_low_power_priv.h"

#include <bcm_buzzz.h>

/* Thread statistics */
/* #define BCM_THREAD_STATS */

static void threadx_hw_timer_isr(void);

static bool in_isr = FALSE;
static bool is_initialized = FALSE;
static TX_THREAD *current_thread = NULL;
static hnd_stats_t *stats_threadx_hw_timer_isr = NULL;

#ifdef THREADX_WATCHDOG

/* Watchdog logging */
/* #define WDDBG_INFORM */

#define WATCHDOG_MIN_TIMEOUT_MS		100
#define WATCHDOG_MIN_INTERVAL_MS	100
#define WATCHDOG_RESET_DELAY_MS		2000	/* Time between ARM WD and system WD firing */
#define WATCHDOG_REBOOT_DELAY_MS	1000	/* Time to allow for console flushing */
#define WATCHDOG_REBOOT_NOW		10

#define WD_ERROR(args)			do { printf args; } while (0)
#if defined(BCMDBG) && defined(WDDBG_INFORM)
#define WD_INFORM(args)			do { printf args; } while (0)
#else
#define WD_INFORM(args)
#endif /* BCMDBG || WDDBG_INFORM */

static osl_ext_task_t *watchdog_thread = NULL;	/* Main watchdog thread handle */
static osl_ext_task_t *watchdog_reason = NULL;	/* Watchdog reason thread handle */
static bool watchdog_enabled = TRUE;

#endif /* THREADX_WATCHDOG */

#if defined(BCM_THREAD_STATS)

static void
threads_cmd(void *arg, int argc, char *argv[])
{
	TX_THREAD *th = _tx_thread_created_ptr;

	ASSERT(th != NULL);

	printf_suppress_timestamp(TRUE);
	printf("NAME                   TX_PTR ST PRIO  STACK   "
		"USED   HIGH Q_CUR Q_CAP Q_ERR\n");

	do {
		uint stack_used, stack_max;

		uint32 *stack_ptr = th->tx_thread_stack_start;
		while (stack_ptr < (uint32*)th->tx_thread_stack_ptr && *stack_ptr == TX_STACK_FILL)
			stack_ptr++;

		stack_used = (uint32)th->tx_thread_stack_end - (uint32)th->tx_thread_stack_ptr;
		stack_max  = (uint32)th->tx_thread_stack_end - (uint32)stack_ptr;

		printf("%20s %p %2u %4u %6lu %6u %6u ",
			th->tx_thread_name, th, th->tx_thread_state,
			th->tx_thread_priority, th->tx_thread_stack_size,
			stack_used, stack_max);
		if (th->queue != NULL) {
			printf("%5u %5u %5u", th->queue->tx_queue_enqueued,
				th->queue->tx_queue_capacity, th->queue_errors);
		}
		printf("\n");

		th = th->tx_thread_created_next;
	} while (th != _tx_thread_created_ptr);

	printf_suppress_timestamp(FALSE);
}

#endif /* BCM_THREAD_STATS */

#ifdef HND_INTSTATS

/**
 * Helper function for parsing console commands.
 */

static bool
has_option(const char *option, int argc, char *argv[], uint *value)
{
	while (argc) {
		char *offset = argv[argc-1];
		uint len = strlen(option);
		if (!strncmp(offset, option, len)) {
			if (value) {
				offset += len;
				*value = (*offset == ':' ? atoi(++offset) : 0);
			}
			return TRUE;
		}
		argc--;
	}
	return FALSE;
}

/**
 * Handle interrupt statistics console command.
 *
 * intstats enable [interval[:msec]] [usr]:
 *	Reset and enable statistics gathering.
 *	Option [interval[:msec]] resets all statistics and enables statistics gathering.
 *	Option [usr] causes only user-defined statistics to be gathered.
 *
 * intstats disable
 *	Disable statistics gathering.
 *
 * intstats show [all] [cycles] [usr] [reset]
 *	Show current statistics for objects with non-zero runcount.
 *	Option [all] shows statistics for object with zero runcount also.
 *	Option [cycles] shows times in cycles as opposed to microseconds.
 *	Option [usr] shows user-defined statistics only.
 *	Option [reset] causes statistics to be reset after showing.
 *
 * intstats reset
 *	Reset statistics.
 */

static void
intstats_cmd(void *arg, int argc, char *argv[])
{
	if (argc > 1) {
		if (!strcmp(argv[1], "enable")) {
			uint interval = 0;
			hnd_stats_enableflags_t flags = 0;
			if (has_option("interval", argc-2, argv+2, &interval)) {
				flags |= INTSTATS_ENAB_INTERVAL;
			}
			if (has_option("usr", argc-2, argv+2, NULL)) {
				flags |= INTSTATS_ENAB_USR_ONLY;
			}
			hnd_isr_enable_stats(TRUE, flags, interval);
		} else if (!strcmp(argv[1], "disable")) {
			hnd_isr_enable_stats(FALSE, 0, 0);
		} else if (!strcmp(argv[1], "show")) {
			hnd_stats_dumpflags_t flags = 0;
			if (has_option("all", argc-2, argv+2, NULL)) {
				flags |= INTSTATS_DUMP_ALL;
			}
			if (has_option("cycles", argc-2, argv+2, NULL)) {
				flags |= INTSTATS_DUMP_CYCLES;
			}
			if (has_option("usr", argc-2, argv+2, NULL)) {
				flags |= INTSTATS_DUMP_USR_ONLY;
			}
			if (has_option("reset", argc-2, argv+2, NULL)) {
				flags |= INTSTATS_DUMP_RESET;
			}
			hnd_isr_dump_stats(flags);
		} else if (!strcmp(argv[1], "reset")) {
			hnd_isr_reset_stats();
		}
	}
}
#endif /* HND_INTSTATS */

#ifdef THREADX_WATCHDOG
#if defined(BCMDBG) || defined(BCMDBG_ASSERT) || defined(WLTEST)

static void
watchdog_status_cmd(void)
{
	TX_INTERRUPT_SAVE_AREA

	TX_THREAD *thread = _tx_thread_created_ptr;
	uint now = OSL_SYSUPTIME();

	if (watchdog_thread == NULL) {
		printf("watchdog inactive\n");
		return;
	}

	if (watchdog_enabled == FALSE) {
		printf("watchdog disabled\n");
	}

#ifdef THREADX_WATCHDOG_HW_DISABLE
	printf("HW watchdog disabled\n");
#endif /* THREADX_WATCHDOG_HW_DISABLE */

	TX_DISABLE

	do {
		if (thread == watchdog_thread) {
			printf("[%s] interval=%ums timeout=%ums last=%ums max=%ums WD\n",
				THREAD_NAMEPTR(thread),
				thread->watchdog_interval,
				thread->watchdog_timeout,
				thread->watchdog_last_ts ? now - thread->watchdog_last_ts : 0,
				thread->watchdog_max);
		} else if (thread->watchdog_timeout != 0) {
			printf("[%s] interval=%ums remain=%ums last=%ums max=%ums\n",
				THREAD_NAMEPTR(thread),
				thread->watchdog_interval,
				thread->watchdog_timeout * watchdog_thread->watchdog_interval,
				thread->watchdog_last_ts ? now - thread->watchdog_last_ts : 0,
				thread->watchdog_max);
		} else {
			printf("[%s] not protected\n", THREAD_NAMEPTR(thread));
		}
		thread = thread->tx_thread_created_next;
	} while (thread != _tx_thread_created_ptr);

	TX_RESTORE
}

static void
watchdog_suspend_cmd(char *thread_name)
{
	TX_THREAD *thread = _tx_thread_created_ptr;

	do {
		if (strcmp(THREAD_NAMEPTR(thread), thread_name) == 0) {
			printf("suspending [%s]\n", THREAD_NAMEPTR(thread));
			tx_thread_suspend(thread);
			return;
		}
		thread = thread->tx_thread_created_next;
	} while (thread != _tx_thread_created_ptr);

	printf("unknown thread [%s]\n", thread_name);
}

/**
 * Handle ThreadX watchdog console command.
 *
 * wd enable:
 *	Enable watchdog
 *
 * wd disable
 *	Disable watchdog
 *
 * wd status
 *	Show current watchdog status for all threads.
 *
 * wd suspend [name]
 *	Test the watchdog by suspending the thread with the specified [name].
 *
 * wd loop
 *	Test the watchdog by entering an infinite loop on the main thread.
 *
 * wd trap
 *	Test the watchdog by executing a null pointer dereference on the main thread.
 *
 * wd pmu
 *	Test the PMU hardware watchdog.
 *
 * wd cc
 *	Test the chipcommon hardware watchdog.
 */

static void
watchdog_cmd(void *arg, int argc, char *argv[])
{
	if (argc > 1) {
		if (!strcmp(argv[1], "status")) {
			watchdog_status_cmd();
		} else if (!strcmp(argv[1], "suspend") && argc > 2) {
			watchdog_suspend_cmd(argv[2]);
		} else if (!strcmp(argv[1], "loop")) {
			while (1) {};
		} else if (!strcmp(argv[1], "trap")) {
			char* null = 0;
			*null = 1;
		} else if (!strcmp(argv[1], "pmu")) {
			pmu_corereg(hnd_sih, SI_CC_IDX, pmuwatchdog, ~0, 100);
		} else if (!strcmp(argv[1], "cc")) {
			si_corereg(hnd_sih, SI_CC_IDX, OFFSETOF(chipcregs_t, watchdog), ~0, 100);
		} else if (!strcmp(argv[1], "enable")) {
			hnd_thread_watchdog_enable(argc > 2 ? *argv[2] == '1' : TRUE);
		} else if (!strcmp(argv[1], "disable")) {
			hnd_thread_watchdog_enable(FALSE);
		}
	}
}

#endif /* BCMDBG || BCMDBG_ASSERT || WLTEST */

/**
 * Check watchdog timeouts on non-main watchdog threads.
 *
 * @return	Pointer to thread for which timeout was detected, or NULL.
 */

static osl_ext_task_t *
watchdog_check_threads(void)
{
	TX_INTERRUPT_SAVE_AREA

	TX_THREAD *thread = _tx_thread_created_ptr;
	TX_THREAD *timeout_thread = NULL;

	TX_DISABLE

	/* Check all threads in the system for timeouts */
	do {
		if (thread != watchdog_thread) {
			/* Update thread timeout counter and signal a timeout if it drops to zero */
			if (thread->watchdog_timeout != 0) {
				thread->watchdog_timeout--;

				WD_INFORM(("[%s] checking WD, timeout=%ums\n",
					THREAD_NAMEPTR(thread),
					thread->watchdog_timeout *
						watchdog_thread->watchdog_interval));

				if (thread->watchdog_timeout == 0) {
					timeout_thread = thread;
					break;
				}
			}
		}
		thread = thread->tx_thread_created_next;
	} while (thread != _tx_thread_created_ptr);

	TX_RESTORE

	return (osl_ext_task_t*)timeout_thread;
}

/**
 * Decode and print a ThreadX thread suspension frame.
 *
 * ThreadX pushes thread state onto the stack during a context switch, so we can decode and
 * print it when the main watchdog thread detects a timeout on any of the other threads.
 *
 * @param thread	Thread pointer.
 */

static void
watchdog_dump_thread_state(osl_ext_task_t *thread)
{
	uint32 *stack;
	int i;

	ASSERT(thread);
	ASSERT(thread != watchdog_thread);
	ASSERT(thread != osl_ext_task_current());

	stack = thread->tx_thread_stack_ptr;
	if (stack != NULL && thread != osl_ext_task_current()) {
		/* Check if a synchronous task suspension frame is present on the stack */
		if (*stack == 0) {
			printf("synchronous task suspension of [%s], state=%u\n",
				THREAD_NAMEPTR(thread), thread->tx_thread_state);
			printf("cpsr 0x%x, lr 0x%x, sp 0x%x\n",
				stack[1], stack[10], (uint32)stack);
			for (i = 0; i < 8; i++) {
				printf("%sr%u 0x%x", i ? ", " : "", i+4, stack[2 + i]);
			}
			printf("\n");
			stack += 10;
		} else {
			printf("interrupt of [%s], state=%u\n",
				THREAD_NAMEPTR(thread), thread->tx_thread_state);
			printf("spsr 0x%x, lr 0x%x, pc 0x%x, sp 0x%x\n",
				stack[1], stack[13], stack[14], (uint32)stack);
			for (i = 0; i < 13; i++) {
				printf("%sr%u 0x%x", i ? ", " : "", i, stack[2+  i]);
			}
			printf("\n");
			stack += 14;
		}
		hnd_print_stack((uint32)stack);
	} else {
		printf("no suspension info for [%s]\n", THREAD_NAMEPTR(thread));
	}
}

/**
 * Helper function for programming hardware watchdog.
 *
 * @param timeout	Timeout value (ms), or WATCHDOG_DISABLE (0).
 */

static void
hnd_thread_set_watchdog(osl_ext_time_ms_t timeout)
{
#if defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__)
	/* To allow TRAP generation for watchdog resets, configure the EL1 physical timer of
	 * the ARM Generic Timer to generate an interrupt just before the system watchdog fires.
	 * The GIC is configured to route this to the FIQ handler (@see threadx_fiq_isr).
	 */
	if (timeout != WATCHDOG_DISABLE) {
		/* Add a small delay between ARM GT and system watchdog firing */
		timeout += WATCHDOG_RESET_DELAY_MS;

		hnd_arm_gt_set_timer(ARM_GT_PHYSICAL_TIMER, (osl_ext_time_us_t)(timeout * 1000));
	} else {
		hnd_arm_gt_ack_timer(ARM_GT_PHYSICAL_TIMER);
	}
#endif /* __ARM_ARCH_7A__ */

#ifndef THREADX_WATCHDOG_HW_DISABLE
	/* Set the system watchdog timeout */
	si_watchdog_ms(hnd_sih, timeout);
#endif /* THREADX_WATCHDOG_HW_DISABLE */
}

/**
 * Internal function for feeding the watchdog.
 *
 * @param thread	Pointer to target thread.
 * @param timeout	Timeout value (ms), or WATCHDOG_DISABLE (0).
 * @return		OSL_EXT_SUCCESS
 */

static osl_ext_status_t
hnd_thread_watchdog_feed_internal(osl_ext_task_t *thread, osl_ext_time_ms_t timeout)
{
	TX_INTERRUPT_SAVE_AREA

	uint interval, now = OSL_SYSUPTIME();
	osl_ext_task_t *dead_thread;

	ASSERT(thread != NULL);
	ASSERT(thread->watchdog_timer == NULL || timeout != WATCHDOG_DISABLE);

	if (thread == watchdog_thread) {

		/* The main watchdog thread is the only thread that may access
		 * the watchdog hardware registers, all other threads are checked
		 * by the main watchdog thread.
		 */
		hnd_thread_set_watchdog(watchdog_enabled ? timeout : WATCHDOG_DISABLE);

		WD_INFORM(("[%s] feeding WD, timeout=%ums\n",
			THREAD_NAMEPTR(thread), timeout));

		/* Fire the WD if a timeout is detected on any of the other threads */
		dead_thread = watchdog_check_threads();
		if (dead_thread != NULL) {
			watchdog_reason = dead_thread;

			printf("WD timeout in [%s], last=%ums\n", THREAD_NAMEPTR(dead_thread),
				now - dead_thread->watchdog_last_ts);
			watchdog_dump_thread_state(dead_thread);

			if (watchdog_enabled) {
#ifdef RTE_CONS
				hnd_thread_set_watchdog(WATCHDOG_REBOOT_DELAY_MS);
				hnd_cons_flush();
#else
				hnd_thread_set_watchdog(WATCHDOG_REBOOT_NOW);
#endif /* RTE_CONS */
				while (1);
			}
		}

		if (thread->watchdog_last_ts != 0) {
			interval = now - thread->watchdog_last_ts;
			thread->watchdog_max = MAX(thread->watchdog_max, interval);
		}
		thread->watchdog_last_ts = now;
	} else if (watchdog_thread != NULL) {
		if (timeout != WATCHDOG_DISABLE) {

			/* Calculate thread timeout in periods for easier checking
			 * by the main watchdog thread.
			 */

			TX_DISABLE

			ASSERT(watchdog_thread->watchdog_interval != 0);
			thread->watchdog_timeout = 1 +
				(timeout + watchdog_thread->watchdog_interval - 1) /
				watchdog_thread->watchdog_interval;

			TX_RESTORE

			WD_INFORM(("[%s] feeding WD, timeout=%ums\n",
				THREAD_NAMEPTR(thread),
				thread->watchdog_timeout * watchdog_thread->watchdog_interval));

			if (thread->watchdog_last_ts != 0) {
				interval = now - thread->watchdog_last_ts;
				thread->watchdog_max = MAX(thread->watchdog_max, interval);
			}
			thread->watchdog_last_ts = now;
		} else {
			thread->watchdog_timeout = 0;
			thread->watchdog_last_ts = 0;
		}
	}

	return OSL_EXT_SUCCESS;
}

/**
 * Watchdog timer callback.
 *
 * @param t	Pointer to timer
 */

static void
watchdog_timer_cb(hnd_timer_t *t)
{
	osl_ext_task_t *thread = osl_ext_task_current();

	hnd_thread_watchdog_feed_internal(thread, thread->watchdog_timeout);
}

/**
 * Set or disable the watchdog timeout for the specified thread. Can't be used
 * for threads for with a timer-based watchdog is enabled.
 *
 * The specified thread must call this function before the timeout specified in the previous call
 * expires, or the watchdog will reset the system.
 *
 * @param thread	Pointer to target thread, or NULL for current thread.
 * @param timeout	Timeout value (ms), or WATCHDOG_DISABLE (0).
 * @return		OSL_EXT_SUCCESS on success, or OSL_EXT_ERROR if a
 *			timer-based watchdog is enabled for the specified thread.
 */

osl_ext_status_t
hnd_thread_watchdog_feed(osl_ext_task_t *thread, osl_ext_time_ms_t timeout)
{
	ASSERT(!hnd_in_isr());

	if (thread == NULL) {
		thread = osl_ext_task_current();
	}

	/* Don't allow manual feeding of a timer-based watchdog */
	if (thread->watchdog_timer != NULL) {
		ASSERT(FALSE);
		return OSL_EXT_ERROR;
	}

	if (timeout != WATCHDOG_DISABLE) {
		timeout = MAX(timeout, WATCHDOG_MIN_TIMEOUT_MS);
	}

	return hnd_thread_watchdog_feed_internal(thread, timeout);
}

/**
 * Initialize and start the system watchdog.
 *
 * This creates a timer-based watchdog that periodically feeds the hardware watchdog
 * checks the status of all other watchdog-enabled threads in the system.
 *
 * The specified @see interval determines the minimum timeout unit that will be used for
 * other threads.
 *
 * @param thread	Pointer to target thread.
 * @param interval	Watchdog feeding interval (ms).
 * @param timeout	Watchdog timeout (ms).
 * @return		OSL_EXT_SUCCESS on success.
 */

osl_ext_status_t
hnd_thread_watchdog_init(osl_ext_task_t *thread, osl_ext_time_ms_t interval,
	osl_ext_time_ms_t timeout)
{
	osl_ext_status_t status;

	ASSERT(thread != NULL);
	ASSERT(watchdog_thread == NULL);

	/* Mark the specified thread as the main watchdog thread */
	watchdog_thread = thread;

	/* Start timer-based watchdog */
	status = hnd_thread_watchdog_start(thread, interval, timeout);
	if (status == OSL_EXT_SUCCESS) {
		WD_INFORM(("[%s] set to WD thread\n", THREAD_NAMEPTR(thread)));
		watchdog_reason = thread;
	} else {
		watchdog_thread = NULL;
	}

	return status;
}

/**
 * Enable timer-driven watchdog feeding for the specified thread.
 *
 * A timer is created to call @see hnd_thread_watchdog_feed with the specified timeout value at
 * the specified interval. The timer is created using @see hnd_timer_create and thus
 * @see hnd_thread_watchdog_feed is only called when the hardware watchdog timer, timer interrupt,
 * ThreadX timers, DPC queue and target thread event handling are functioning correctly.
 *
 * @param thread	Pointer to target thread, or NULL for current thread.
 * @param interval	Watchdog feeding interval (ms).
 * @param timeout	Watchdog timeout (ms).
 * @return		OSL_EXT_SUCCESS on success.
 *
 * @note The specified thread must have been initialized for handling
 *	 events (@see hnd_thread_init_event_handling)
 */

osl_ext_status_t
hnd_thread_watchdog_start(osl_ext_task_t *thread, osl_ext_time_ms_t interval,
	osl_ext_time_ms_t timeout)
{
	ASSERT(interval != 0);
	ASSERT(timeout != 0);
	ASSERT(watchdog_thread != NULL);

	ASSERT(!hnd_in_isr());

	if (thread == NULL) {
		thread = osl_ext_task_current();
	}

	/* Don't support calling this function multiple times for the same thread */
	if (thread->watchdog_timer != NULL) {
		ASSERT(FALSE);
		return OSL_EXT_ERROR;
	}

	/* Create a watchdog timer targetting the specified thread */
	thread->watchdog_timer = hnd_timer_create(NULL, NULL, watchdog_timer_cb, NULL, thread);
	if (thread->watchdog_timer != NULL) {
		thread->watchdog_interval = MAX(interval, WATCHDOG_MIN_INTERVAL_MS);
		thread->watchdog_timeout  = MAX(timeout, WATCHDOG_MIN_TIMEOUT_MS) +
			/* */		    thread->watchdog_interval;

		WD_INFORM(("[%s] enable WD, interval=%u timeout=%u\n", THREAD_NAMEPTR(thread),
			thread->watchdog_interval, thread->watchdog_timeout));

		if (hnd_timer_start(thread->watchdog_timer, thread->watchdog_interval, TRUE)) {
			hnd_thread_watchdog_feed_internal(thread, thread->watchdog_timeout);
			printf("[%s] watchdog timeout set to %d ms\n", THREAD_NAMEPTR(thread),
				thread->watchdog_timeout);
			return OSL_EXT_SUCCESS;
		}

		hnd_timer_free(thread->watchdog_timer);
		thread->watchdog_timer = NULL;
	}

	return OSL_EXT_ERROR;
}

#if defined(BCMDBG) || defined(BCMDBG_ASSERT) || defined(WLTEST)

/**
 * System watchdog control.
 *
 * New watchdog state will become effective when the main watchdog timer fires, this
 * call should therefore not be used to disable the watchdog just before a blocking operation.
 *
 * @param enable	New state
 */

void
hnd_thread_watchdog_enable(bool enable)
{
	if (enable != watchdog_enabled) {
		WD_INFORM(("[%s] WD %sabled\n", CURRENT_THREAD_NAMEPTR, enable ? "en" : "dis"));
		watchdog_enabled = enable;
	}
}

#endif /* BCMDBG || BCMDBG_ASSERT WLTEST */

#else /* THREADX_WATCHDOG */

inline osl_ext_status_t
hnd_thread_watchdog_init(osl_ext_task_t *thread, osl_ext_time_ms_t interval,
	osl_ext_time_ms_t timeout)
{
	return OSL_EXT_SUCCESS;
}

inline osl_ext_status_t
hnd_thread_watchdog_start(osl_ext_task_t *thread, osl_ext_time_ms_t interval,
	osl_ext_time_ms_t timeout)
{
	return OSL_EXT_SUCCESS;
}

inline osl_ext_status_t
hnd_thread_watchdog_feed(osl_ext_task_t *thread, osl_ext_time_ms_t timeout)
{
	return OSL_EXT_SUCCESS;
}

inline void
hnd_thread_watchdog_enable(bool enable) {}

#endif /* THREADX_WATCHDOG */

/**
 * Get hardware timer value.
 *
 * @return		Timer value in microseconds.
 */

inline uint64
hnd_thread_get_hw_timer_us(void)
{
#ifdef THREADX_ARM_GT_TIMER

	return hnd_arm_gt_get_count();

#else /* THREADX_ARM_GT_TIMER */

	return hnd_time_us();

#endif /* THREADX_ARM_GT_TIMER */
}

/**
 * Set hardware timer timeout value.
 *
 * @param us		Timeout value in microseconds.
 */

inline void
hnd_thread_set_hw_timer(uint us)
{
#ifdef THREADX_ARM_GT_TIMER

	/* Configure the EL1 virtual timer of the ARM Generic Timer to generate an interrupt
	 * at the specified time. This assumes the GT timer runs at 1MHz. The interrupt is
	 * handled in @see threadx_isr.
	 */
	hnd_arm_gt_set_timer(ARM_GT_VIRTUAL_TIMER, us);

#else /* THREADX_ARM_GT_TIMER */

	/* Configure the PMU to generate an interrupt at the specified time. */
	hnd_set_irq_timer(us);

#endif /* THREADX_ARM_GT_TIMER */
}

/**
 * Acknowledge hardware timer.
 */

inline void
hnd_thread_ack_hw_timer(void)
{
#ifdef THREADX_ARM_GT_TIMER

	/* Acknowledge the EL1 virtual timer of the ARM Generic Timer */
	hnd_arm_gt_ack_timer(ARM_GT_VIRTUAL_TIMER);

#else /* THREADX_ARM_GT_TIMER */

	/* Acknowledge the PMU timer */
	hnd_ack_irq_timer();

#endif /* THREADX_ARM_GT_TIMER */
}

/**
 * Get hardware timer interrupt status.
 *
 * @return		Nonzero if an interrupt is pending.
 */

static inline uint32
hnd_thread_get_hw_timer_status(void)
{
#ifdef THREADX_ARM_GT_TIMER

	return hnd_arm_gt_get_status(ARM_GT_VIRTUAL_TIMER);

#else /* THREADX_ARM_GT_TIMER */

	return FALSE;	/* Not used. */

#endif /* THREADX_ARM_GT_TIMER */
}

#ifndef BCM_BUZZZ

/* entering idle */
static void
threadx_idle_enter(si_t *sih)
{
	BUZZZ_LVL3(THREADX_IDLE, 0);

#ifdef BCMDBG_CPU
	hnd_update_cpu_debug();
#endif // endif

	threadx_low_power_enter();
}

#endif /* BCM_BUZZZ */

void
hnd_idle(si_t *sih)
{
#if defined(BCM_BUZZZ)

	/* idle_thread executes a busy burn loop without WFI.
	 * WFI may not be used with BUZZZ, as ARM-PMU counters stop counting
	 */
	while (TRUE); /* busy burn loop */

#elif !defined(RTE_POLL)
	TX_INTERRUPT_SAVE_AREA

	ASSERT(is_initialized == TRUE);

	while (TRUE) {
		/* To avoid a race condition, interrupts are disabled while
		 * prepararing for low power mode and during WFI. The WFI
		 * instruction does not require interrupts to be enabled to
		 * wake the processor.
		 */

		if (!hnd_isr_stats_enabled()) {

			TX_DISABLE
			threadx_idle_enter(sih);
			hnd_wait_irq(sih);
			TX_RESTORE

			/* Any pending interrupts will be handled here, and any DPCs
			 * will be processed by other threads.
			 */
		}
	}
#else /* !BCM_BUZZZ && RTE_POLL */
	while (TRUE) {
		threadx_idle_enter(sih);
	}
#endif /* !BCM_BUZZZ && RTE_POLL */
}

/**
 * Get context.
 *
 * @return		TRUE if we are in interrupt context.
 *
 */

inline bool
hnd_in_isr(void)
{
	return in_isr;
}

/**
 * Get the current thread in non-ISR context, and preempted thread in ISR context.
 *
 * @return		Current thread.
 */

inline osl_ext_task_t*
hnd_get_last_thread(void)
{
	return current_thread;
}

/**
 * Do final ThreadX initialisation just before enabling interrupts.
 */

void
hnd_thread_initialize(void)
{
	ASSERT(is_initialized == FALSE);

	/* Create some statistics objects for ThreadX */
	stats_threadx_hw_timer_isr = hnd_isr_stats_create(OBJ_TYPE_ISR,
		"threadx_hw_timer_isr", threadx_hw_timer_isr, NULL);

#if defined(THREADX_ARM_GT_TIMER) && (defined(__ARM_ARCH_7A__) || \
	defined(__ARM_ARCH_7R__))

	/* Configure the clock for driving the ARM Generic Timer. The frequency must be
	 * set to 1MHz on chips where the GT is used for watchdog or ThreadX hardware timer.
	 */
	hnd_pmu_set_gtimer_period();

	/* Initialize the ARM Generic Timer */
	hnd_arm_gt_init(hnd_sih);

#endif /* THREADX_ARM_GT_TIMER && (__ARM_ARCH_7A__ || __ARM_ARCH_7R__) */

	is_initialized = TRUE;
}

/**
 * Cancel execution of a DPC.
 *
 * If execution of the associated callback function has already begun, it will run
 * to completion.
 *
 * @param dpc		Pointer to DPC.
 * @return		TRUE if the callback function was not being executed at
 *			the time of this call, or the DPC was not scheduled.
 */

bool
hnd_dpc_cancel(hnd_dpc_t *dpc)
{
	TX_INTERRUPT_SAVE_AREA

	bool cancelled = TRUE;

	ASSERT(dpc != NULL);
	ASSERT(dpc->deleted == FALSE);

	TX_DISABLE

	if (dpc->state == DPC_SCHEDULED) {
		dpc->state = DPC_CANCELLED;
	} else if (dpc->executing) {
		cancelled = FALSE;
	}

	TX_RESTORE

	return cancelled;
}

/**
 * Schedule execution of a DPC by the DPC target thread.
 *
 * The specified DPC is scheduled only if it is not pending (@see hnd_dpc_is_pending).
 *
 * @param dpc		Pointer to DPC.
 * @return		TRUE if the DPC was scheduled.
 *
 * @note The DPC target thread must have been initialized for handling
 *	 events (@see initialize_event_handling)
 * @note When cancelled, a DPC remains in the queue but will be ignored by the target
 *	 thread. When a cancelled DPC is rescheduled while still on the queue, it will
 *	 be marked as scheduled and keep it's original position in the queue.
 */

bool
hnd_dpc_schedule(hnd_dpc_t *dpc)
{
	ASSERT(dpc != NULL);
	ASSERT(dpc->deleted == FALSE);

	if (dpc == NULL || dpc->deleted == TRUE) {
		return FALSE;
	}

	return hnd_dpc_schedule_internal(dpc);
}

/**
 * Reschedule execution of the currently executing DPC.
 *
 * @return		TRUE if the DPC was scheduled.
 */

bool
hnd_dpc_reschedule(void)
{
	osl_ext_task_t *thread = osl_ext_task_current();

	ASSERT(!hnd_in_isr());
	ASSERT(thread != NULL);

	return hnd_dpc_schedule(thread->current_dpc);
}

/**
 * Schedule execution of a DPC by the DPC target thread, internal function
 * that does not check deleted state.
 *
 * @param dpc		Pointer to DPC.
 * @return		TRUE if the DPC was scheduled.
 */

bool
hnd_dpc_schedule_internal(hnd_dpc_t *dpc)
{
	TX_INTERRUPT_SAVE_AREA

	UINT result = TX_SUCCESS;

	ASSERT(dpc != NULL);
	ASSERT(dpc->dpc_fn != NULL);
	ASSERT(dpc->thread != NULL);

	/* Check if the thread can handle events */
	if (dpc->thread->queue == NULL) {
		return FALSE;
	}

	TX_DISABLE

	if (dpc->state == DPC_NOT_SCHEDULED) {
		/* Mark the DPC as pending and post it to the target thread */
		dpc->state = DPC_SCHEDULED;
		result = tx_queue_send(dpc->thread->queue, &dpc, TX_NO_WAIT);
		if (result == TX_SUCCESS) {
			HND_INTSTATS_NOTIFY(NOTIFICATION_TYPE_QUEUE_ENTER, (void*)dpc->stats);
		} else {
			/* Event queue needs to be dimensioned to handle worst-case event count */
			dpc->state = DPC_NOT_SCHEDULED;
			dpc->thread->queue_errors++;
			ASSERT(FALSE);
		}
	} else if (dpc->state == DPC_CANCELLED) {
		/* Mark the DPC as pending, keeping the original position in the queue */
		dpc->state = DPC_SCHEDULED;
		HND_INTSTATS_NOTIFY(NOTIFICATION_TYPE_QUEUE_ENTER, (void*)dpc->stats);
	} else {
		/* Already scheduled */
		ASSERT(dpc->state == DPC_SCHEDULED);
	}

	TX_RESTORE

	return result == TX_SUCCESS;
}

/**
 * Run the specified DPC.
 *
 * This function will be called in the context of the DPC target thread.
 *
 * @param dpc		Pointer to DPC.
 */

static void
run_dpc(hnd_dpc_t *dpc)
{
	ASSERT(dpc != NULL);
	ASSERT(dpc->deleted == FALSE);
	ASSERT(dpc->dpc_fn != NULL);

	HND_CPUUTIL_NOTIFY(HND_CPUUTIL_TRANS_DPC_ENT, dpc->cpuutil);
	HND_INTSTATS_NOTIFY(NOTIFICATION_TYPE_DPC_ENTER,
		dpc->stats->type != OBJ_TYPE_TDPC ? (void*)dpc->stats : NULL);
	BUZZZ_LVL3(HND_DPC_ENT, 1, (uint32)dpc->dpc_fn);

	(dpc->dpc_fn)(dpc->cbdata);

	BUZZZ_LVL3(HND_DPC_RTN, 0);
	HND_INTSTATS_NOTIFY(NOTIFICATION_TYPE_DPC_EXIT,
		dpc->stats->type != OBJ_TYPE_TDPC ? (void*)dpc->stats : NULL);
	HND_CPUUTIL_NOTIFY(HND_CPUUTIL_TRANS_DPC_EXT, dpc->cpuutil);
}

/**
 * Initialize event handling for the specified thread.
 *
 * A thread can optionally become a target for events, which can be either regular DPCs
 * or DPCs associated with interrupts or timers. This function needs to be called before any
 * DPCs are scheduled (@see hnd_dpc_schedule) for this thread, and before the thread calls
 * @see process_events.
 *
 * This function can only be called once per thread.
 *
 * @param thread	Pointer to event target thread, or NULL for current thread.
 * @param event_depth	Maximum number of simultaneous events.
 * @return		OSL_EXT_SUCCESS on success.
 */

osl_ext_status_t
hnd_thread_init_event_handling(osl_ext_task_t *thread, uint event_depth)
{
	TX_INTERRUPT_SAVE_AREA

	TX_QUEUE *queue;
	osl_ext_status_t status = OSL_EXT_ERROR;

	ASSERT(thread != NULL);
	ASSERT(event_depth != 0);

	TX_DISABLE

	/* This function can only be called once per thread */
	if (thread->queue == NULL) {

		/* Allocate event message queue and memory area in one go */
		queue = (struct TX_QUEUE_STRUCT*)MALLOC(hnd_osh,
			sizeof(struct TX_QUEUE_STRUCT) + event_depth * sizeof(void*));

		if (queue != NULL) {
			/* Create the queue and associate it with the target thread */
			tx_queue_create(queue, "dpc queue", sizeof(void*) / sizeof(ULONG),
				(void*)(queue+1), event_depth * sizeof(void*));
			thread->queue = queue;
			status = OSL_EXT_SUCCESS;
		}
	}

	TX_RESTORE

	return status;
}

/**
 * Start handling events for the current thread. This function never returns.
 *
 * @note The current thread must have been initialized for handling events
 *       (@see hnd_thread_init_event_handling).
 */

static void
process_events(void)
{
	TX_INTERRUPT_SAVE_AREA

	hnd_dpc_t *dpc;
	osl_ext_task_t *thread;

	thread = osl_ext_task_current();
	ASSERT(thread != NULL);

	/* Check if the thread can handle events */
	ASSERT(thread->queue != NULL);

	while (TRUE) {
#ifdef ATE_BUILD
		hnd_poll(sih);
#endif // endif
		/* Process all events and run DPCs */
		if (tx_queue_receive(thread->queue, &dpc, TX_WAIT_FOREVER) == TX_SUCCESS) {

			HND_INTSTATS_NOTIFY(NOTIFICATION_TYPE_QUEUE_EXIT, (void*)dpc->stats);
			deadman_state_machine(DEADMAN_EVENT_DPC_START);

			TX_DISABLE

			if (dpc->deleted == TRUE) {
				hnd_dpc_free_internal(dpc);

				TX_RESTORE

			} else if (dpc->state == DPC_SCHEDULED) {
				dpc->state = DPC_NOT_SCHEDULED;
				dpc->executing = TRUE;
				thread->current_dpc = dpc;

				TX_RESTORE

				run_dpc(dpc);

				dpc->executing = FALSE;
				thread->current_dpc = NULL;
			} else {
				/* Assume DPC_CANCELLED */
				ASSERT(dpc->state == DPC_CANCELLED);
				dpc->state = DPC_NOT_SCHEDULED;

				TX_RESTORE
			}

			deadman_refresh(hnd_time());

		} else {
			thread->queue_errors++;
		}

		deadman_state_machine(DEADMAN_EVENT_DPC_END);
	}
}

#ifndef BCMDBG_LOADAVG

/**
 * Fast interrupt handler.
 *
 * FIQ is generated from ARM Generic Timer when configured timeout elapses,
 * this is used to implement watchdog in ARM context.
 *
 * The return value is used to work around a hardware bug.
 *
 * @return	Nonzero to generate trap.
 */

uint32
BCMRAMFN(threadx_fiq_isr)(void)
{
#ifdef THREADX_WATCHDOG
	if (watchdog_reason != NULL) {
		printf("WD timeout in [%s], last=%ums\n", THREAD_NAMEPTR(watchdog_reason),
			OSL_SYSUPTIME() - watchdog_reason->watchdog_last_ts);
	}
#endif /* THREADX_WATCHDOG */

	return (uint32)hnd_cpu_gtimer_trap_validation();
}
#endif /* !BCMDBG_LOADAVG */

/**
 * Hardware timer interrupt handler.
 */

static void
BCMRAMFN(threadx_hw_timer_isr)(void)
{
	HND_INTSTATS_NOTIFY(NOTIFICATION_TYPE_ISR_ENTER, stats_threadx_hw_timer_isr);

	/* Update the software timers, run any timeouts, and schedule the next
	 * hardware interrupt.
	 */
	threadx_advance_time();
	threadx_schedule_timer();

	HND_INTSTATS_NOTIFY(NOTIFICATION_TYPE_ISR_EXIT, stats_threadx_hw_timer_isr);
}

/**
 * Interrupt handler.
 *
 * @param tr		Pointer to trap.
 */

void
BCMRAMFN(threadx_isr)(threadx_trap_t *tr)
{
	uint32 sbflagst;

	in_isr = TRUE;

	deadman_state_machine(DEADMAN_EVENT_ISR_START);

	/* Leave low power mode. This function does nothing if we were not in low power mode */
	threadx_low_power_exit();

	/* If we are using the ARM GT as our hardware timer, check if an interrupt has occured. */
	if (hnd_thread_get_hw_timer_status() != 0) {
		threadx_hw_timer_isr();
	}

	/* Read interrupt flags */
	sbflagst = si_intflag(hnd_sih);
	if (sbflagst != 0) {
		BUZZZ_LVL4(THREADX_CPU_ISR_ENT, 1, (uint32)sbflagst);

		/* For each active interrupt, run the registered ISR callback in interrupt context.
		 * If the ISR has a DPC registered for it, schedule it for execution by the target
		 * thread.
		 *
		 * If we are using the PMU timer as our hardware timer then updating software
		 * timers, running timeouts and scheduling the next hardware interrupt is done
		 * as part of PMU ISR/DPC processing.
		 */
		hnd_isr_proc_sbflagst(sbflagst);
	}

	deadman_state_machine(DEADMAN_EVENT_ISR_END);

	BUZZZ_LVL4(THREADX_CPU_ISR_RTN, 0);

	in_isr = FALSE;
}

/**
 * Low-level initialization to handle all processor specific initialization issues.
 *
 * This function is called as part of ThreadX initialization. It runs with all interrupts
 * disabled, before the system is initialized and before the OS scheduler is started.
 */

void _tx_initialize_low_level(void)
{
	// XXX: enable caches
}

/**
 * Create a thread.
 *
 * This function differs from @see osl_ext_task_create in that it accepts an additional argument
 * (@see event_depth) specifying the size of the queue for sending events to this thread.
 * If this argument is 0, no queue will be allocated and no events messages can be send to the
 * thread.
 *
 * @param name		Pointer to task string descriptor.
 * @param stack		Pointer to stack.
 * @param stack_size 	Stack size in bytes.
 * @param priority	Abstract task priority.
 * @param func		A pointer to the thread entry point function.
 * @param arg		Value passed into thread entry point function.
 * @param event_depth	Maximum number of pending events for this thread, can be 0.
 * @param task		Task to create.
 * @return		OSL_EXT_SUCCESS if the task was created successfully.
 *
 * @note The specified thread function should call @see process_events to receive and handle
 *	 DPC messages.
 */

osl_ext_status_t
hnd_thread_create(char* name, void *stack, unsigned int stack_size,
	osl_ext_task_priority_t priority, osl_ext_task_entry func, osl_ext_task_arg_t arg,
	uint event_depth, osl_ext_task_t *task)
{
	osl_ext_status_t status;

	/* Highest priority reserved for main task during system initialization */
	ASSERT(priority < OSL_EXT_TASK_TIME_CRITICAL_PRIORITY);

	/* Create a task but don't start it yet if DPC handling capability was specified */
	status = osl_ext_task_create_ex(name, stack, stack_size, priority, 0, func, arg,
		(event_depth != 0), task);
	if (status == OSL_EXT_SUCCESS) {

		ASSERT(task != NULL);
		task->stats = hnd_isr_stats_create(OBJ_TYPE_THREAD, name, (void*)func, NULL);

		if (event_depth != 0) {
			/* Enable event handling (DPCs scheduled from ISRs or timers) */
			status = hnd_thread_init_event_handling(task, event_depth);
			if (status == OSL_EXT_SUCCESS) {

				/* Start the thread */
				status = osl_ext_task_resume(task);
			} else {
				osl_ext_task_delete(task);
			}
		}
	}

	return status;
}

void * /* used by threadx_osl_ext.c, as callers may not use hnd_thread_create */
hnd_thread_cpuutil_bind(unsigned int thread_func)
{
	return HND_CPUUTIL_CONTEXT_GET(HND_CPUUTIL_CONTEXT_THR, thread_func);
}

/**
 * Start handling events for the current thread, to be used by external callers.
 */

void
hnd_thread_process_events(void)
{
	/* Start processing events. */
	process_events();
}

/**
 * Default thread entry function.
 *
 * You can specify this function as the thread entry function in the call to hnd_thread_create()
 * if the thread only needs to process DPCs.
 *
 * @param arg           Thread argument as specified in the call to hnd_thread_create().
 */

void
hnd_thread_default_implementation(osl_ext_task_arg_t arg)
{
	/* Start processing events. */
	process_events();
}

void
BCMATTACHFN(hnd_ths_cli_init)(void)
{
#if defined(RTE_CONS) && !defined(BCM_BOOTLOADER)

#ifdef BCM_THREAD_STATS
	hnd_cons_add_cmd("threads", threads_cmd, 0);
#endif // endif

#ifdef THREADX_WATCHDOG
#if defined(BCMDBG) || defined(BCMDBG_ASSERT) || defined(WLTEST)
	hnd_cons_add_cmd("wd", watchdog_cmd, 0);
#endif /* BCMDBG || BCMDBG_ASSERT WLTEST */
#endif /* THREADX_WATCHDOG */

#ifdef HND_INTSTATS
	hnd_cons_add_cmd("intstats", intstats_cmd, 0);
#endif /* HND_INTSTATS */

#endif /* RTE_CONS  && ! BCM_BOOTLOADER */
}

void
hnd_trap_threadx_info(void)
{
	osl_ext_task_t *cur_task = osl_ext_task_current();
	if (cur_task == NULL) {
		return;
	}

	printf("Thread: %s(ID:%#lx) run cnt:%lu\n", cur_task->tx_thread_name,
		cur_task->tx_thread_id, cur_task->tx_thread_run_count);
	printf("Thread: Stack:%p Start Addr:%p End Addr:%p Size:%lu\n",
		cur_task->tx_thread_stack_ptr, cur_task->tx_thread_stack_start,
		cur_task->tx_thread_stack_end, cur_task->tx_thread_stack_size);
	printf("Thread: Entry func:%p\n", cur_task->tx_thread_entry);
	printf("Thread: Timer:%p\n", &cur_task->tx_thread_timer);
}

/**
 * Update ThreadX time and process any expired timers. Accessor for when using the PMU
 * timer to generate hardware interrupts.
 */

inline void
hnd_advance_time(void)
{
#if !defined(THREADX_ARM_GT_TIMER) || defined(RTE_POLL)
	threadx_advance_time();
#endif /* !THREADX_ARM_GT_TIMER || RTE_POLL */
}

/**
 * Schedule a hardware interrupt based on next timer expiration time. Accessor for when
 * using the PMU timer to generate hardware interrupts.
 */

inline void
hnd_schedule_timer(void)
{
#if !defined(THREADX_ARM_GT_TIMER) || defined(RTE_POLL)
	threadx_schedule_timer();
#endif /* !THREADX_ARM_GT_TIMER || RTE_POLL */
}

/**
 * Suspend current thread for the specified time.
 *
 * A thread may only sleep once the system is fully initialized and global interrupts
 * are enabled, as the system may enter low power mode and needs a PMU timer interrupt
 * to wake up. If this function is called before initialization is complete, we spinwait
 * instead.
 *
 * @param us		Suspend time in MICROSECONDS.
 *
 * @note Suspension may be aborted by another thread, timer, or ISR.
 */

void
hnd_thread_sleep(osl_ext_time_us_t us)
{
	ASSERT(!hnd_in_isr());

	if (is_initialized == TRUE) {
		/* Temporary disable preemption to make sure the hardware timer is
		 * rescheduled before yielding.
		 */
		_tx_thread_preempt_disable++;
		tx_thread_sleep((ULONG)OSL_USEC_TO_TICKS(us));
		threadx_schedule_timer();
		_tx_thread_preempt_disable--;
		tx_thread_relinquish();
	} else {
		OSL_DELAY(us);
	}
}

#if defined(BCM_BUZZZ) || defined(HND_CPUUTIL) || defined(HND_INTSTATS)
/* Generic hook into tx_thread_schedule.S, invoked on a context switch.
 * Avoid changes to a threadx source, and use _tx_execution_thread_enter with
 * TX_ENABLE_EXECUTION_CHANGE_NOTIFY.
 */
extern void _bcm_execution_thread_enter(TX_THREAD *thread_to_exec);

void BCM_BUZZZ_NOINSTR_FUNC
_bcm_execution_thread_enter(TX_THREAD *thread_to_exec)
{
#if defined(BCM_BUZZZ_FUNC) && defined(BCM_BUZZZ_STREAMING_BUILD)
#else /* ! (BCM_BUZZZ_FUNC && BCM_BUZZZ_STREAMING_BUILD) */

	/* Notify CPU Utilization tool of a thread thread context switch */
	HND_CPUUTIL_NOTIFY(HND_CPUUTIL_TRANS_THR_ENT, thread_to_exec->cpuutil);
#if defined(BCM_BUZZZ_THREADX)
#if (BCM_BUZZZ_TRACING_LEVEL >= 3)
	{
		char name = *(THREAD_NAMEPTR(thread_to_exec)); /* first char in name */
		BUZZZ_LVL3(THREADX_SCHED_THREAD, 2,
			(uint32)name, THREAD_PRIO(thread_to_exec));
	}
#endif // endif
#endif /* BCM_BUZZZ_THREADX */

#endif /* ! (BCM_BUZZZ_FUNC && BCM_BUZZZ_STREAMING_BUILD) */

	HND_INTSTATS_NOTIFY(NOTIFICATION_TYPE_THREAD_ENTER, (void*)thread_to_exec);
	current_thread = thread_to_exec;
}
#endif /* BCM_BUZZZ || HND_CPUUTIL || HND_INTSTATS */
