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
 * $Id: threadx.c 787983 2020-06-17 17:59:06Z $
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
#include <rte_isr_stats.h>
#include "rte_isr_stats_priv.h"
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

#if defined(THREADX_ARM_GT_TIMER) && !defined(__ARM_ARCH_7A__)
#error "ARM Generic Timer not supported on this platform"
#endif /* THREADX_ARM_GT_TIMER && !__ARM_ARCH_7A__ */

/* Thread statistics */
// #define BCM_THREAD_STATS

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

#ifdef BCM_THREAD_STATS

static void
threads_cmd(void *arg, int argc, char *argv[])
{
	TX_THREAD *thread = _tx_thread_created_ptr;

	ASSERT(thread != NULL);

	printf_suppress_timestamp(TRUE);
	printf("STATE PRIO  STACK   USED   HIGH    START      END   TX_PTR NAME\n");

	do {
		uint stack_used, stack_max;

		uint32 *stack_ptr = thread->tx_thread_stack_start;
		uint32 *end_ptr   = thread->tx_thread_stack_ptr;
		while (stack_ptr < end_ptr && *stack_ptr == TX_STACK_FILL)
			stack_ptr++;

		stack_used = (uint32)thread->tx_thread_stack_end - (uint32)end_ptr;
		stack_max  = (uint32)thread->tx_thread_stack_end - (uint32)stack_ptr;

		printf("   %2u %4u %6lu %6u %6u %08x %08x %p %s\n",
			thread->tx_thread_state,
			thread->tx_thread_priority, thread->tx_thread_stack_size,
			stack_used, stack_max,
			(uint)thread->tx_thread_stack_start, (uint)thread->tx_thread_stack_end,
			thread, thread->tx_thread_name);

		thread = thread->tx_thread_created_next;
	} while (thread != _tx_thread_created_ptr);

	printf_suppress_timestamp(FALSE);
}

#endif /* BCM_THREAD_STATS */

#ifdef HND_INTSTATS

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
			if (bcm_has_option("interval", argc-2, argv+2, &interval)) {
				flags |= INTSTATS_ENAB_INTERVAL;
			}
			if (bcm_has_option("usr", argc-2, argv+2, NULL)) {
				flags |= INTSTATS_ENAB_USR_ONLY;
			}
			hnd_isr_stats_enable(TRUE, flags, interval);
		} else if (!strcmp(argv[1], "disable")) {
			hnd_isr_stats_enable(FALSE, 0, 0);
		} else if (!strcmp(argv[1], "show")) {
			hnd_stats_dumpflags_t flags = 0;
			if (bcm_has_option("all", argc-2, argv+2, NULL)) {
				flags |= INTSTATS_DUMP_ALL;
			}
			if (bcm_has_option("cycles", argc-2, argv+2, NULL)) {
				flags |= INTSTATS_DUMP_CYCLES;
			}
			if (bcm_has_option("usr", argc-2, argv+2, NULL)) {
				flags |= INTSTATS_DUMP_USR_ONLY;
			}
			if (bcm_has_option("reset", argc-2, argv+2, NULL)) {
				flags |= INTSTATS_DUMP_RESET;
			}
			hnd_isr_stats_dump(flags);
		} else if (!strcmp(argv[1], "reset")) {
			hnd_isr_stats_reset();
		}
	}
}
#endif /* HND_INTSTATS */

#ifdef THREADX_WATCHDOG
#if defined(BCMDBG) || defined(WLTEST)

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
		} else if (!strcmp(argv[1], "assert")) {
			ASSERT(FALSE);
		}
	}
}

#endif // endif

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
			printf("found non-interrupt stack frame of [%s], state=%u:\n",
				THREAD_NAMEPTR(thread), thread->tx_thread_state);
			printf("cpsr 0x%x, lr 0x%x, sp 0x%x\n",
				stack[1], stack[10], (uint32)stack);
			for (i = 0; i < 8; i++) {
				printf("%sr%u 0x%x", i ? ", " : "", i+4, stack[2 + i]);
			}
			printf("\n");
			stack += 10;
		} else {
			printf("found interrupted stack frame of [%s], state=%u:\n",
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
		printf("found no stack frame for [%s]\n", THREAD_NAMEPTR(thread));
	}
}

#if defined(__ARM_ARCH_7A__)

/**
 * Dump thread information.
 *
 * @param thread	Target thread.
 */

static void
watchdog_dump_thread_info(osl_ext_task_t *thread)
{
	hnd_worklet_t *worklet = THREAD_WORKLET(thread);

	printf("thread [%s] p=0x%p, state=%u, stack_start=0x%p, stack_end=0x%p, sp=0x%p\n",
		THREAD_NAMEPTR(thread), thread,
		(uint)((TX_THREAD*)thread)->tx_thread_state,
		((TX_THREAD*)thread)->tx_thread_stack_start,
		((TX_THREAD*)thread)->tx_thread_stack_end,
		((TX_THREAD*)thread)->tx_thread_stack_ptr);

	if (worklet != NULL) {
#ifdef HND_INTSTATS
		hnd_stats_t *stats = worklet->stats;
		printf("worklet [%s] p=0x%p, fn=0x%p stats=%p\n",
			stats->id ? stats->id : "unknown",
			worklet, worklet->fn, stats);
#else
		printf("worklet p=0x%p, fn=0x%p\n", worklet, worklet->fn);
#endif /* HND_INTSTATS */
	}
}

#endif /* __ARM_ARCH_7A__ */

/**
 * Helper function for programming hardware watchdog.
 *
 * @param timeout	Timeout value (ms), or WATCHDOG_DISABLE (0).
 */

static void
watchdog_set_timeout(osl_ext_time_ms_t timeout)
{
	/* We are using two hardware watchdogs, one to generate a FIQ/NMI to trigger system state
	 * collection and reporting to the host, and one to reset the system. The second watchdog
	 * is scheduled slightly after the first to give the host enough time to retrieve system
	 * state.
	 */

#if defined(__ARM_ARCH_7A__)
	/* Configure the ARM CA7 Generic Timer to generate a hardware interrupt to the GIC. The GIC
	 * is configured to generate a FIQ (@see threadx_fiq_isr).
	 */
	if (timeout != WATCHDOG_DISABLE) {
		hnd_arm_gt_set_timer(ARM_GT_PHYSICAL_TIMER, (osl_ext_time_us_t)(timeout * 1000));
	} else {
		hnd_arm_gt_ack_timer(ARM_GT_PHYSICAL_TIMER);
	}

#elif defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__)
	/* For backwards compatibility, configure the CR4/CM3 IntTimer to generate a hardware
	 * interrupt. The PMU is configured to propagate IntTimer interrupts to the ARM FIQ (CR4)
	 * or INTNMI (M3) input, causing a software trap (@see hnd_trap_handler).
	 */

	/* XXX Should revisit use of the ARM IntTimer for watchdog purposes on CR4/CM3 because
	 * this does not take clock stalls during WFI (idle) nor dynamic clock switching into
	 * account.
	 */
	if (timeout != WATCHDOG_DISABLE) {
		hnd_arm_inttimer_set_timer(hnd_sih, c0counts_per_ms * timeout);
	} else {
		hnd_arm_inttimer_ack_timer(hnd_sih);
	}
#endif /* __ARM_ARCH_7R__ || __ARM_ARCH_7M__ */

#ifndef THREADX_WATCHDOG_HW_DISABLE
	/* Schedule the hardware watchdog to reset the system slightly later */
	if (timeout != WATCHDOG_DISABLE) {
		timeout += WATCHDOG_RESET_DELAY_MS;
	}

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
watchdog_feed_internal(osl_ext_task_t *thread, osl_ext_time_ms_t timeout)
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
		watchdog_set_timeout(watchdog_enabled ? timeout : WATCHDOG_DISABLE);

		WD_INFORM(("[%s] feeding WD, timeout=%ums\n",
			THREAD_NAMEPTR(thread), timeout));

		/* Fire the WD if a timeout is detected on any of the other threads */
		dead_thread = watchdog_check_threads();
		if (dead_thread != NULL) {
			watchdog_reason = dead_thread;

			printf("WD timeout in [%s], last alive %ums ago\n",
				THREAD_NAMEPTR(dead_thread), now - dead_thread->watchdog_last_ts);
			watchdog_dump_thread_state(dead_thread);

			if (watchdog_enabled) {
#ifdef RTE_CONS
				watchdog_set_timeout(WATCHDOG_REBOOT_DELAY_MS);
				hnd_cons_flush();
#else
				watchdog_set_timeout(WATCHDOG_REBOOT_NOW);
#endif /* RTE_CONS */
				hnd_infinite_loop();
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

	watchdog_feed_internal(thread, thread->watchdog_timeout);
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

	return watchdog_feed_internal(thread, timeout);
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

#if defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__)
	/* Initialize the IntTimer for generating watchdog interupts */
	hnd_arm_inttimer_init(hnd_sih);
#else
	/* Initialization of the ARM CA7 Generic Timer for generating watchdog interupts is
	 * done in @see hnd_thread_initialize.
	 */
#endif	/* __ARM_ARCH_7R__ || __ARM_ARCH_7M__ */

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
 * ThreadX timers, and the scheduler associated to the target thread are functioning correctly.
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
			watchdog_feed_internal(thread, thread->watchdog_timeout);
			printf("[%s] watchdog timeout set to %d ms\n", THREAD_NAMEPTR(thread),
				thread->watchdog_timeout);
			return OSL_EXT_SUCCESS;
		}

		hnd_timer_free(thread->watchdog_timer);
		thread->watchdog_timer = NULL;
	}

	return OSL_EXT_ERROR;
}

#if defined(BCMDBG) || defined(WLTEST)

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

#endif // endif

#else /* THREADX_WATCHDOG */

INLINE osl_ext_status_t
hnd_thread_watchdog_init(osl_ext_task_t *thread, osl_ext_time_ms_t interval,
	osl_ext_time_ms_t timeout)
{
	return OSL_EXT_SUCCESS;
}

INLINE osl_ext_status_t
hnd_thread_watchdog_start(osl_ext_task_t *thread, osl_ext_time_ms_t interval,
	osl_ext_time_ms_t timeout)
{
	return OSL_EXT_SUCCESS;
}

INLINE osl_ext_status_t
hnd_thread_watchdog_feed(osl_ext_task_t *thread, osl_ext_time_ms_t timeout)
{
	return OSL_EXT_SUCCESS;
}

INLINE void
hnd_thread_watchdog_enable(bool enable) {}

#endif /* THREADX_WATCHDOG */

/**
 * Get hardware timer value.
 *
 * @return		Timer value in microseconds.
 */

INLINE uint64
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

INLINE void
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

INLINE void
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

static INLINE uint32
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

			/* Any pending interrupts will be handled here, and any worklets
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

INLINE bool
hnd_in_isr(void)
{
	return in_isr;
}

/**
 * Get the current thread in non-ISR context, and preempted thread in ISR context.
 *
 * @return		Current thread.
 */

INLINE osl_ext_task_t*
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

#if defined(__ARM_ARCH_7A__)
	/* Initialize the ARM CA7 Generic Timer for driving the ThreadX software timers as well
	 * as generating the ThreadX watchdog interrupt. This assumes the microsecond timer in
	 * the PMU, which drives the GT, is configured to 1MHz.
	 */
	hnd_arm_gt_init(hnd_sih);
#endif /* __ARM_ARCH_7A__ */

	is_initialized = TRUE;
}

/**
 * Associate a scheduler to a thread.
 *
 * @param thread	Thread pointer, or NULL to use current thread.
 * @param scheduler	Scheduler pointer.
 * @return		OSL_EXT_SUCCESS on success.
 */

osl_ext_status_t
hnd_thread_set_scheduler(osl_ext_task_t *thread, void *scheduler)
{
	/* Associate the scheduler to the current thread if none is specified */
	if (thread == NULL) {
		thread = osl_ext_task_current();
		ASSERT(thread != NULL);
	}

	ASSERT(scheduler != NULL);
	ASSERT(thread->scheduler == NULL);

	/* Associated scheduler can only be set once */
	if (scheduler != NULL && thread->scheduler == NULL) {
		thread->scheduler = scheduler;
		return OSL_EXT_SUCCESS;
	}

	return OSL_EXT_ERROR;
}

/**
 * Get the scheduler associated to the specified thread.
 *
 * @param thread	Thread pointer, or NULL to use current thread.
 * @return		Scheduler pointer, or NULLn if none associated.
 */

void*
hnd_thread_get_scheduler(osl_ext_task_t *thread)
{
	ASSERT(!(thread == NULL && hnd_in_isr()));

	/* Get the scheduler associated to the current thread if none is specified */
	if (thread == NULL && !hnd_in_isr()) {
		thread = osl_ext_task_current();
		ASSERT(thread != NULL);
	}

	return thread ? thread->scheduler : NULL;
}

#if defined(__ARM_ARCH_7A__)

/**
 * Fast interrupt handler.
 *
 * FIQ is generated from ARM CA7 Generic Timer. This handler runs in FIQ context with limited stack.
 *
 * @return	Nonzero to generate a trap.
 */

bool
BCMRAMFN(threadx_fiq_isr)(void)
{
	/* Suppress timestamps in the output to avoid further traps when accessing current time */
	printf_suppress_timestamp(TRUE);

#ifdef THREADX_WATCHDOG
	if (watchdog_reason != NULL) {
		printf("WD timeout in [%s], last alive %ums ago\n",
			THREAD_NAMEPTR(watchdog_reason),
			OSL_SYSUPTIME() - watchdog_reason->watchdog_last_ts);

		watchdog_dump_thread_info(watchdog_reason);
	}
#endif /* THREADX_WATCHDOG */

	return TRUE;
}

#endif /* __ARM_ARCH_7A__ */

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
 * This handler runs in IRQ context with limited stack.
 */

void
BCMRAMFN(threadx_isr)(void)
{
	uint32 sbflagst;
	osl_ext_task_t *thread = osl_ext_task_current();

	in_isr = TRUE;

	/* Leave low power mode. This function does nothing if we were not in low power mode */
	threadx_low_power_exit();

	hnd_scheduler_notify_isr_enter(thread);

	/* If we are using the ARM GT as our hardware timer, check if an interrupt has occured. */
	if (hnd_thread_get_hw_timer_status() != 0) {
		threadx_hw_timer_isr();
	}

	/* Read interrupt flags */
	sbflagst = si_intflag(hnd_sih);
	if (sbflagst != 0) {
		BUZZZ_LVL4(THREADX_CPU_ISR_ENT, 1, (uint32)sbflagst);

		/* For each active interrupt, run the registered ISR callback in interrupt context.
		 * If the ISR has a worklet registered for it, schedule it for execution by
		 * the target thread.
		 *
		 * If we are using the PMU timer as our hardware timer then updating software
		 * timers, running timeouts and scheduling the next hardware interrupt is done
		 * as part of PMU ISR/worklet processing.
		 */
		hnd_isr_process_interrupts(sbflagst);
	}

	hnd_scheduler_notify_isr_exit(thread);

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
 * @param name		Pointer to thread string descriptor.
 * @param stack		Pointer to stack.
 * @param stack_size	Stack size in bytes.
 * @param priority	Abstract thread priority.
 * @param func		A pointer to the thread entry point function.
 * @param arg		Value passed into thread entry point function.
 * @param task		Pointer to location for storing thread handle.
 * @return		OSL_EXT_SUCCESS if the thread was created successfully.
 *
 * @note The specified thread function should call @see hnd_scheduler_run to start scheduling
 * worklets. A default thread function @see hnd_thread_default_implementation is available.
 */

osl_ext_status_t
hnd_thread_create(char* name, void *stack, unsigned int stack_size,
	osl_ext_task_priority_t priority, osl_ext_task_entry func, osl_ext_task_arg_t arg,
	osl_ext_task_t *task)
{
	osl_ext_status_t status;

	/* Highest priority reserved for main task during system initialization */
	ASSERT(priority < OSL_EXT_TASK_TIME_CRITICAL_PRIORITY);

	/* Create a task but don't start it yet */
	status = osl_ext_task_create_ex(name, stack, stack_size, priority, 0, func, arg,
		FALSE, task);
	if (status == OSL_EXT_SUCCESS) {

		ASSERT(task != NULL);
		task->stats = hnd_isr_stats_create(OBJ_TYPE_THREAD, name, (void*)func, NULL);

		/* Create a scheduler for executing worklets in the context of the new thread */
		if (hnd_scheduler_create_internal(task, name) != NULL) {
			/* Start the thread */
			status = osl_ext_task_resume(task);
		} else {
			osl_ext_task_delete(task);
			status = OSL_EXT_ERROR;
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
 * Start the scheduler associated to the the current thread, to be used by external callers.
 */

void
hnd_thread_run_scheduler(void)
{
	hnd_scheduler_run();
}

/**
 * Default thread entry function.
 *
 * @param arg		Thread argument as specified in the call to hnd_thread_create().
 */

void
hnd_thread_default_implementation(osl_ext_task_arg_t arg)
{
	hnd_scheduler_run();
}

void
BCMATTACHFN(hnd_thread_cli_init)(void)
{
#if defined(RTE_CONS) && !defined(BCM_BOOTLOADER)

#ifdef BCM_THREAD_STATS
	hnd_cons_add_cmd("threads", threads_cmd, 0);
#endif // endif

#ifdef THREADX_WATCHDOG
#if defined(BCMDBG) || defined(WLTEST)
	hnd_cons_add_cmd("wd", watchdog_cmd, 0);
#endif // endif
#endif /* THREADX_WATCHDOG */

#ifdef HND_INTSTATS
	hnd_cons_add_cmd("intstats", intstats_cmd, 0);
#endif /* HND_INTSTATS */

#endif /* RTE_CONS  && ! BCM_BOOTLOADER */
}

/**
 * Update ThreadX time and process any expired timers. Accessor for when using the PMU
 * timer to generate hardware interrupts.
 */

INLINE void
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

INLINE void
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

/**
 * Find a thread by name.
 *
 * @param name		Pointer to name.
 * @return		Pointer to thread, or NULL if not found.
 */

osl_ext_task_t*
hnd_thread_find_by_name(const char *name)
{
	TX_THREAD *thread = _tx_thread_created_ptr;
	do {
		if (strcmp(name, THREAD_NAMEPTR(thread)) == 0) {
			return (osl_ext_task_t*)thread;
		}
		thread = thread->tx_thread_created_next;
	} while (thread != _tx_thread_created_ptr);

	return NULL;
}

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

	hnd_scheduler_notify_thread(current_thread, (osl_ext_task_t*)thread_to_exec);
	HND_INTSTATS_NOTIFY(NOTIFICATION_TYPE_THREAD_ENTER, (void*)thread_to_exec);
	current_thread = thread_to_exec;
}
