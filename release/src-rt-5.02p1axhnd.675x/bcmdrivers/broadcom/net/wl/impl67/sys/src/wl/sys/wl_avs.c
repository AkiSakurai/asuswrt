/*
 * Adaptive Voltage Scaling
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
 * $Id$
 */

#include <wl_avs.h>
#include <avs.h>

#include <typedefs.h>
#include <bcmdefs.h>
#include <wlioctl.h>
#include <bcmutils.h>
#include <bcmdevs.h>
#include <wlc.h>
#include <wlc_pub.h>
#include <wlc_iocv.h>
#include <wlc_dump.h>
#include <wl_dbg.h>
#include <osl_ext.h>

#ifdef DONGLEBUILD
#include <rte.h>
#include <wl_rte_priv.h>
#else /* DONGLEBUILD */
#include <wl_linux.h>
#include <wl_export.h>
#endif /* DONGLEBUILD */

#define AVS_DEFAULT_TIMER_INTERVAL_MS	1000		/* Default tracking interval */
#define AVS_MIN_TIMER_INTERVAL_MS	100		/* Minimum tracking interval */
#define AVS_STACK_SIZE			1024		/* (FD only) */
#define AVS_WATCHDOG_TIMEOUT_MS		4000		/* Watchdog timeout (FD only) */

static const char BCMATTACHDATA(rstr_avsint)[]   = "avsint";
static const char BCMATTACHDATA(rstr_avsdelay)[] = "avsdelay";

#ifdef DONGLEBUILD
static void avs_timer_cb(hnd_timer_t *t);
#else
static void avs_timer_cb(void *data);
static int avs_thread_fn(void *data);

#define WATCHDOG_DISABLE		0

#define EVENT_TIMER			(1 << 0)
#define EVENT_RESET			(1 << 1)
#endif /* DONGLEBUILD */

static int avs_dump(void *ctx, struct bcmstrbuf *buf);
static int avs_doiovar(void *hdl, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif);

/*
 * Globals
 */

#ifdef DONGLEBUILD
static CHAR avs_thread_stack[AVS_STACK_SIZE] DECLSPEC_ALIGN(16);
#endif /* DONGLEBUILD */

/*
 * Local prototypes
 */

enum {
	/* Unused entry    = 0 */
	IOV_AVS_INTERVAL   = 1,
	IOV_AVS_TRIGGER    = 2,
	/* Unused entry    = 3 */
	IOV_AVS_RESET      = 4,
	IOV_AVS_VOLTAGE    = 5,
	IOV_AVS_TEMP       = 6,
	IOV_AVS_ARMTEMP    = 7,
	IOV_AVS_TRACKING   = 8,
	IOV_AVS_VLIMITL    = 100,		/* Part of component interface */
	IOV_AVS_VLIMITH    = 101,
	IOV_AVS_VMARGINL   = 102,
	IOV_AVS_VMARGINH   = 103,
	IOV_AVS_THRMARGIN  = 104,
	IOV_AVS_DAC        = 105
};

static const bcm_iovar_t avs_iovars[] = {
	{"avs_interval",   IOV_AVS_INTERVAL,	0, 0, IOVT_UINT32, sizeof(uint32) },
	{"avs_trigger",    IOV_AVS_TRIGGER,	0, 0, IOVT_UINT32, sizeof(uint32) },
	{"avs_disable",	   IOV_AVS_RESET,	0, 0, IOVT_UINT32, sizeof(uint32) },
	{"avs_reset",	   IOV_AVS_RESET,	0, 0, IOVT_UINT32, sizeof(uint32) },
	{"avs_voltage",    IOV_AVS_VOLTAGE,	0, 0, IOVT_UINT32, sizeof(uint32) },
	{"avs_temp",       IOV_AVS_TEMP,	0, 0, IOVT_INT32, sizeof(int32) },
	{"avs_armtemp",    IOV_AVS_ARMTEMP,	0, 0, IOVT_INT32, sizeof(int32) },
	{"avs_tracking",   IOV_AVS_TRACKING,	0, 0, IOVT_UINT32, sizeof(uint32) },
	{"avs_vlimitl",    IOV_AVS_VLIMITL,	0, 0, IOVT_UINT32, sizeof(uint32) },
	{"avs_vlimith",    IOV_AVS_VLIMITH,	0, 0, IOVT_UINT32, sizeof(uint32) },
	{"avs_vmarginl",   IOV_AVS_VMARGINL,	0, 0, IOVT_UINT32, sizeof(uint32) },
	{"avs_vmarginh",   IOV_AVS_VMARGINH,	0, 0, IOVT_UINT32, sizeof(uint32) },
	{"avs_thrmargin",  IOV_AVS_THRMARGIN,	0, 0, IOVT_UINT32, sizeof(uint32) },
	{"avs_dac",        IOV_AVS_DAC,		0, 0, IOVT_UINT32, sizeof(uint32) },
	{NULL, 0, 0, 0, 0, 0}
};

/*
 * Private data structures
 */

struct wl_avs {
	wlc_info_t		*wlc;
	avs_context_t		*avs;			/* AVS component handle */
	uint			interval;		/* AVS tracking interval, in ms. */

#ifdef DONGLEBUILD
	osl_ext_task_t		thread;			/* AVS thread */
	hnd_timer_t		*timer;			/* Tracking timer */
#else
	struct task_struct	*thread;		/* AVS thread */
	wl_timer_t		*timer;			/* Tracking timer */
	struct completion	completion;		/* Completion handler */
	uint			events;			/* Current events for AVS thread */
#endif /* DONGLEBUILD */
};

/*
 * Helper functions to hide NIC/FD details
 */

static void
avs_timer_stop(wl_avs_t *avs)
{
#ifdef DONGLEBUILD
	hnd_thread_watchdog_feed(&avs->thread, WATCHDOG_DISABLE);
	if (avs->timer != NULL) {
		hnd_timer_stop(avs->timer);
	}
#else /* DONGLEBUILD */
	if (avs->timer != NULL) {
		wl_del_timer(avs->wlc->wl, avs->timer);
	}
#endif /* DONGLEBUILD */
}

static void
avs_timer_start(wl_avs_t *avs, osl_ext_time_ms_t timeout)
{
	ASSERT(avs->timer != NULL);

#ifdef DONGLEBUILD
	/* Don't set the watchdog if single stepping */
	if (timeout != 0) {
		hnd_thread_watchdog_feed(&avs->thread, timeout + AVS_WATCHDOG_TIMEOUT_MS);
	}
	hnd_timer_start(avs->timer, timeout, FALSE);
#else /* DONGLEBUILD */
	wl_add_timer(avs->wlc->wl, avs->timer, timeout, 0);
#endif /* DONGLEBUILD */
}

/*
 * Export functions
 */

#ifndef DONGLEBUILD
/**
 * ------------------------------------------------------------------------------
 * wl_avs_suspend:
 * - Called during Host platform(NIC based) entering Suspend State : S2 or S3
 * - Stops the AVS Timer
 * - Stops the AVS thread
 * ------------------------------------------------------------------------------
 */
void
wl_avs_suspend(wl_avs_t *avs)
{
	WL_INFORM(("%s: Entry\n", __FUNCTION__));

	avs_timer_stop(avs);

	/* Stop the thread otherwise due to race condition, Previous timer call back and
	 * thread fucntion can continue avs_track() or avs_reset() in thread context
	 * By this time Device may go suspend state and backplane access will fail.
	 */
	complete(&avs->completion);
	if (avs->thread) {
		kthread_stop(avs->thread);
	}
}

/**
 * ------------------------------------------------------------------------------
 * wl_avs_resume:
 * - Called during Host platform(NIC based) Resuming from Suspend State : S2 or S3
 * - Starts the AVS thread
 * - Reset AVS from thread context after resume
 * - Starts the AVS Timer
 * ------------------------------------------------------------------------------
 */
void
wl_avs_resume(wl_avs_t *avs)
{
	WL_INFORM(("%s: Entry\n", __FUNCTION__));

	avs->thread = kthread_run((void*)avs_thread_fn, avs, "avs");
	if (IS_ERR(avs->thread)) {
		WL_ERROR(("wl%d: %s: unable to start AVS thread\n",
			avs->wlc->pub->unit, __FUNCTION__));
		avs->thread = NULL;
	}

	/* Need to reset AVS from thread context after resume */
	avs->events |= EVENT_RESET;
	complete(&avs->completion);

	avs_timer_start(avs, avs->interval);
}
#endif /* !DONGLEBUILD */

wl_avs_t*
BCMATTACHFN(wl_avs_attach)(wlc_info_t *wlc)
{
	wl_avs_t *avs;
	int delay = -1;		/* Use AVS interval */

	ASSERT(wlc && wlc->pub);

	if (avs_supported(wlc->pub->osh, wlc->pub->sih) == FALSE) {
		WL_INFORM(("wl%d: %s: AVS not available\n", wlc->pub->unit, __FUNCTION__));
		return NULL;
	}

	avs = (wl_avs_t*)MALLOCZ(wlc->pub->osh, sizeof(wl_avs_t));
	if (avs == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->pub->osh)));
		goto exit;
	}

	avs->wlc = wlc;

	/* Determine AVS interval and startup delay */
	avs->interval = AVS_DEFAULT_TIMER_INTERVAL_MS;
	if (BCM6715_CHIP(wlc->pub->sih->chip)) {
		/*
		 * Temporary
		 * if 6715 then don't run unless "avsint" is set
		 */
		avs->interval = 0;
	}
	if (getvar(wlc->pub->vars, rstr_avsint) != NULL) {
		avs->interval = getintvar(wlc->pub->vars, rstr_avsint);
		avs->interval = avs->interval ? MAX(avs->interval, AVS_MIN_TIMER_INTERVAL_MS) : 0;
	}
	if (getvar(wlc->pub->vars, rstr_avsdelay) != NULL) {
		delay = getintvar(wlc->pub->vars, rstr_avsdelay);
		delay = MAX(delay, -1);
	}

	/* Attach to AVS component */
	avs->avs = avs_attach(wlc->pub->osh, wlc->pub->sih, (void*)wlc->wl, wlc->pub->vars);
	if (avs->avs == NULL) {
		goto exit;
	}

#ifdef DONGLEBUILD
	/*
	 * Create a thread for handling the AVS timer callback, using the default thread loop
	 * implementation.
	 */
	if (hnd_thread_create("avs_thread", &avs_thread_stack, sizeof(avs_thread_stack),
		OSL_EXT_TASK_HIGH_NORMAL_PRIORITY, hnd_thread_default_implementation, avs,
		&avs->thread) != OSL_EXT_SUCCESS) {
			WL_ERROR(("wl%d: %s: thread creation failed\n",
				wlc->pub->unit, __FUNCTION__));
			goto exit;
	}

	/*
	 * Create and start a timer and run the callback in the context
	 * of the newly created thread.
	 */
	avs->timer = hnd_timer_create(NULL, avs, avs_timer_cb, NULL, &avs->thread);
	if (avs->timer == NULL) {
		WL_ERROR(("wl%d: %s: timer failed\n", wlc->pub->unit, __FUNCTION__));
		goto exit;
	}

#else /* DONGLEBUILD */

	/* Start a thread on which to run AVS */
	init_completion(&avs->completion);
	avs->thread = kthread_run((void*)avs_thread_fn, avs, "avs");
	if (IS_ERR(avs->thread)) {
		WL_ERROR(("wl%d: %s: unable to start AVS thread\n", wlc->pub->unit, __FUNCTION__));
		avs->thread = NULL;
		goto exit;
	}

	/* Create a timer to signal the AVS thread */
	avs->timer = wl_init_timer(wlc->wl, avs_timer_cb, (void*)avs, "avs_tmr");
	if (avs->timer == NULL) {
		WL_ERROR(("wl%d: %s: timer failed\n", wlc->pub->unit, __FUNCTION__));
		goto exit;
	}

#endif /* DONGLEBUILD */

	/* Register module */
	if (wlc_module_register(wlc->pub, avs_iovars, "avs", avs, avs_doiovar,
	    NULL, NULL, NULL)) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed", wlc->pub->unit, __FUNCTION__));
		goto exit;
	}

#if defined(BCMDBG) || defined(AVS_ENABLE_STATUS)
	if (wlc_dump_register(wlc->pub, "avs", avs_dump, avs)) {
		WL_ERROR(("wl%d: %s: wlc_dump_register() failed", wlc->pub->unit, __FUNCTION__));
		/* Not fatal */
	}
#endif // endif

	if (avs->interval != 0) {
		avs_timer_start(avs, delay != -1 ? delay : avs->interval);
	}

	return avs;
exit:
	MODULE_DETACH(avs, wl_avs_detach);
	return NULL;
}

void
BCMATTACHFN(wl_avs_detach)(wl_avs_t *avs)
{
	if (avs == NULL)
		return;

	ASSERT(avs->wlc != NULL);

	avs_timer_stop(avs);

#ifdef DONGLEBUILD
	if (avs->timer != NULL) {
		hnd_timer_free(avs->timer);
	}
#else /* DONGLEBUILD */
	if (avs->thread) {
		kthread_stop(avs->thread);
	}
	if (avs->timer != NULL) {
		wl_free_timer(avs->wlc->wl, avs->timer);
	}
#endif /* DONGLEBUILD */

	wlc_module_unregister(avs->wlc->pub, "avs", avs);

	if (avs->avs != NULL) {
		avs_reset(avs->avs);
		avs_detach(avs->avs);
	}

	MFREE(avs->wlc->pub->osh, avs, sizeof(wl_avs_t));
}

/*
 * Local Functions
 */

#ifdef DONGLEBUILD

static void
avs_timer_cb(hnd_timer_t *t)
{
	wl_avs_t *avs = (wl_avs_t*)hnd_timer_get_data(t);
	int result;

	ASSERT_THREAD("avs_thread");
	ASSERT(avs != NULL && avs->avs != NULL);

	/* Run AVS algorithm */
	result = avs_track(avs->avs);
	if (result != BCME_OK) {
		avs->interval = 0;
		avs_timer_stop(avs);
		WL_ERROR(("AVS disabled (%d)\n", result));
	}

	/* Reschedule next AVS run */
	if (avs->interval != 0) {
		ASSERT(avs->interval >= AVS_MIN_TIMER_INTERVAL_MS);
		avs_timer_start(avs, avs->interval);
	}
}

#else /* DONGLEBUILD */

static void
avs_timer_cb(void *data)
{
	wl_avs_t *avs = (wl_avs_t*)data;

	ASSERT(avs != NULL);

	avs->events |= EVENT_TIMER;
	complete(&avs->completion);
}

static int
avs_thread_fn(void *data)
{
	wl_avs_t *avs = (wl_avs_t*)data;
	int result;
	const unsigned long timeout = msecs_to_jiffies(100);

	while (1) {
		if (avs->events == 0) {
			/* Wait for an event */
			wait_for_completion_timeout(&avs->completion, timeout);

#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 12, 0)
			reinit_completion(&avs->completion);
#else
			INIT_COMPLETION(avs->completion);
#endif /* 3.12 */
		}

		if (kthread_should_stop()) {
			break;
		}

		if (avs->events & EVENT_RESET) {
			avs->events = 0;	/* Clear other events */

			/* Reset AVS in thread context */
			avs_reset(avs->avs);
		}

		if (avs->events & EVENT_TIMER) {
			avs->events &= ~EVENT_TIMER;

			/* Run AVS algorithm */
			result = avs_track(avs->avs);
			if (result != BCME_OK) {
				avs->interval = 0;
				WL_ERROR(("AVS disabled (%d)\n", result));
			}

			/* Reschedule next AVS run */
			if (avs->interval != 0) {
				ASSERT(avs->interval >= AVS_MIN_TIMER_INTERVAL_MS);
				avs_timer_start(avs, avs->interval);
			}
		}
	}

	return 0;
}

#endif /* DONGLEBUILD */

#if defined(BCMDBG) || defined(AVS_ENABLE_STATUS)

static int
avs_dump(void *ctx, struct bcmstrbuf *buf)
{
	wl_avs_t *avs = (wl_avs_t*)ctx;

	/* avs_status is thread-safe, so no need to run this command on AVS thread */
	return avs_status(avs->avs, buf);
}

#endif // endif

static int
avs_doiovar(void *hdl, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif)
{
	wl_avs_t *avs = (wl_avs_t*)hdl;
	int err = BCME_OK;
	uint32 *pval32 = (uint32*)arg;

	BCM_REFERENCE(wlcif);
	BCM_REFERENCE(val_size);
	BCM_REFERENCE(p_len);
	BCM_REFERENCE(avs);

	ASSERT(avs != NULL);
	ASSERT(avs->avs != NULL);

	switch (actionid) {
	case IOV_SVAL(IOV_AVS_INTERVAL):
		if (*pval32 == 0) {
			avs->interval = 0;
			avs_timer_stop(avs);
		} else {
			avs->interval = MAX(*pval32, AVS_MIN_TIMER_INTERVAL_MS);
			avs_timer_start(avs, avs->interval);
		}
		break;
	case IOV_GVAL(IOV_AVS_INTERVAL):
		*pval32 = avs->interval;
		break;
	case IOV_GVAL(IOV_AVS_TRIGGER):
		*pval32 = 1;
		avs->interval = 0;
		avs_timer_stop(avs);
		avs_timer_start(avs, 0);
		break;
	case IOV_GVAL(IOV_AVS_VOLTAGE):
		err = avs_get_value(avs->avs, AVS_INDEX_VOLTAGE, pval32);
		break;
	case IOV_GVAL(IOV_AVS_TEMP):
		err = avs_get_value(avs->avs, AVS_INDEX_TEMP, pval32);
		break;
	case IOV_GVAL(IOV_AVS_ARMTEMP):
		err = avs_get_value(avs->avs, AVS_INDEX_ARM_TEMP, pval32);
		break;
	case IOV_GVAL(IOV_AVS_TRACKING):
		err = avs_get_value(avs->avs, AVS_INDEX_TRACKING, pval32);
		break;
	case IOV_SVAL(IOV_AVS_TRACKING):
		err = avs_tracking_switch(avs->avs, *pval32);
		break;
	case IOV_GVAL(IOV_AVS_RESET):
		avs->interval = 0;
		avs_timer_stop(avs);
#ifdef DONGLEBUILD
		avs_reset(avs->avs);
#else /* DONGLEBUILD */
		/* Need to reset AVS from thread context */
		avs->events |= EVENT_RESET;
		complete(&avs->completion);
#endif /* DONGLEBUILD */
		*pval32 = 1;
		break;
#ifdef BCMDBG
	case IOV_SVAL(IOV_AVS_VLIMITL):
		err = avs_set_vlimit_low(avs->avs, *pval32);
		break;
	case IOV_SVAL(IOV_AVS_VLIMITH):
		err = avs_set_vlimit_high(avs->avs, *pval32);
		break;
	case IOV_SVAL(IOV_AVS_VMARGINL):
		err = avs_set_vmargin_low(avs->avs, *pval32);
		break;
	case IOV_SVAL(IOV_AVS_VMARGINH):
		err = avs_set_vmargin_high(avs->avs, *pval32);
		break;
	case IOV_SVAL(IOV_AVS_THRMARGIN):
		err = avs_set_threshold_margin(avs->avs, *pval32);
		break;
	case IOV_SVAL(IOV_AVS_DAC):
		err = avs_set_dac(avs->avs, *pval32);
		break;
#endif /* BCMDBG */
	default:
		err = BCME_UNSUPPORTED;
		break;
	}
	return err;
}
