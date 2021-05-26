/**
 * @file
 * @brief
 * Generic high resolution timer abstraction layer for multiplexing h/w timer
 * Broadcom 802.11abgn Networking Device Driver
 *
 * It also includes an implementation using d11 tsf/gptimer h/w.
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
 *
 * $Id: wlc_hrt.c 690768 2017-03-17 19:01:31Z $
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmendian.h>
#include <bcmutils.h>
#include <siutils.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#ifdef DONGLEBUILD
#include <rte_timer.h>
#endif /* DONGLEBUILD */
#include <wlc_hrt.h>
#include <wlc_dump.h>
#include <wlc_hw_priv.h>

/* timeout structure, storage provided by the user */
struct wlc_hrt_to {
	wlc_hrt_to_t *next;		/* pointer to next timeout object */
	wlc_hrt_info_t *hrti;
	uint timeout;			/* amount of time remaining till timeout */
	wlc_hrt_to_cb_fn fun;		/* function to call when timed out */
	void *arg;			/* argument passed to function above */
	bool expired;			/* indicates timeout object has expired */
};

/* high res. timer module private states */
struct wlc_hrt_info {
	wlc_info_t *wlc;		/* back pointer to wlc */
	wlc_hrt_to_t timer_list;	/* delta sorted timeout list */
	wlc_hrt_get_time_fn get_time;	/* fn to get current time */
	wlc_hrt_set_timer_fn set_timer;	/* fn to set the hw timer */
	wlc_hrt_ack_timer_fn ack_timer;	/* fn to ack the hw timer, can be NULL */
	void *ctx;
#ifdef DONGLEBUILD
	hnd_timer_t *pmu_timer;
#endif /* DONGLEBUILD */
	uint max_timer_val;		/* max timer value for this hw timer */
	uint last_time;			/* stores the last time gettime was called */
	bool timer_allowed;		/* disables timeout */
	bool timer_armed;		/* indicates hw timer is set */
};

/* local functions for multiplexed hw gptimer use */

/* module */

/* debug */
#if defined(BCMDBG)
static int wlc_hrt_dump(void *ctx, struct bcmstrbuf *b);
#endif // endif

/* rte timer callback */
#ifdef DONGLEBUILD
static void _wlc_hrt_timer_cb(hnd_timer_t *hrti);
#endif /* DONGLEBUILD */

/* timeout */
static void wlc_hrt_run_timeouts(wlc_hrt_info_t *hrti);

/* tsf/gptimer as high res. timer h/w */
static uint wlc_hrt_gptimer_gettime(void *ctx);
static void wlc_hrt_gptimer_settimer(void *ctx, uint timeout);
static void wlc_hrt_gptimer_ack(void *ctx);

/* module */
wlc_hrt_info_t *
BCMATTACHFN(wlc_hrt_attach)(wlc_info_t *wlc)
{
	wlc_hrt_info_t *hrti;

	if ((hrti = (wlc_hrt_info_t *)
	     MALLOCZ(wlc->osh, sizeof(wlc_hrt_info_t))) == NULL) {
		WL_ERROR(("%s: failed to allocate memory for hw timer\n", __FUNCTION__));
		goto fail;
	}

#ifdef DONGLEBUILD
	if (!(hrti->pmu_timer = hnd_timer_create(hrti, NULL, _wlc_hrt_timer_cb, NULL, NULL))) {
		goto fail;
	}
#endif /* DONGLEBUILD */

	hrti->wlc = wlc;

	/* TODO: mode this registration out when a separate module is created
	 * for gptimer h/w timer.
	 */
	(void)wlc_hrt_hai_register(hrti,
	                           wlc_hrt_gptimer_gettime,
	                           wlc_hrt_gptimer_settimer,
	                           wlc_hrt_gptimer_ack,
	                           hrti,
	                           (uint)(uint32)(~0));

#if defined(BCMDBG)
	wlc_dump_register(wlc->pub, "hrt", wlc_hrt_dump, hrti);
#endif // endif

	return hrti;

fail:
	MODULE_DETACH(hrti, wlc_hrt_detach);
	return NULL;
}

void
BCMATTACHFN(wlc_hrt_detach)(wlc_hrt_info_t *hrti)
{
	wlc_info_t *wlc;

	if (hrti == NULL)
		return;

#ifdef DONGLEBUILD
	if (hrti->pmu_timer) {
		hnd_timer_free(hrti->pmu_timer);
	}
#endif /* DONGLEBUILD */

	wlc = hrti->wlc;

	MFREE(wlc->osh, hrti, sizeof(wlc_hrt_info_t));
}

int
wlc_hrt_hai_register(wlc_hrt_info_t *hrti,
	wlc_hrt_get_time_fn get_time,
	wlc_hrt_set_timer_fn set_timer,
	wlc_hrt_ack_timer_fn ack_timer,
	void *ctx,
	uint max_timer_val)
{
	if (hrti->timer_allowed)
		return BCME_BUSY;

	hrti->get_time = get_time;
	hrti->set_timer = set_timer;
	hrti->ack_timer = ack_timer;
	hrti->ctx = ctx;
	hrti->max_timer_val = max_timer_val;

	hrti->timer_allowed = TRUE;

	return BCME_OK;
}

/* timeout handler */
static void
wlc_hrt_timeout_handler(wlc_hrt_info_t *hrti)
{
	wlc_hrt_to_t *head;

	hrti->timer_armed = FALSE;
	wlc_hrt_run_timeouts(hrti);
	if (hrti->timer_armed)
		return;
	head = &hrti->timer_list;
	if (head->next != NULL) {
		hrti->set_timer(hrti->ctx, head->next->timeout);
		hrti->timer_armed = TRUE;
	}
	else if (hrti->ack_timer != NULL) {
		hrti->ack_timer(hrti->ctx);
	}
}

/* timer isr */
void
wlc_hrt_isr(wlc_hrt_info_t *hrti)
{
#ifndef DONGLEBUILD
	wlc_hrt_timeout_handler(hrti);
#endif /* DONGLEBUILD */
}

/* timer multiplexing layer */

/* Update expiration times in timeout list */
static void
wlc_hrt_update_timeout_list(wlc_hrt_info_t *hrti)
{
	uint delta;
	wlc_hrt_to_t *head = &hrti->timer_list;
	wlc_hrt_to_t *this = head->next;

	delta = wlc_hrt_getdelta(hrti, &hrti->last_time);
	if (this == NULL)
		return;
	if (delta == 0 && this->timeout != 0)
		return;
	while (this != NULL) {
		if (this->timeout <= delta) {
			/* timer has expired */
			delta -= this->timeout;
			this->timeout = 0;
			this->expired = TRUE;
			this = this->next;
		} else {
			this->timeout -= delta;
			break;
		}
	}
}

/* Remove expired from timeout list and invoke their callbacks */
static void
wlc_hrt_run_timeouts(wlc_hrt_info_t *hrti)
{
	wlc_hrt_to_t *this;
	wlc_hrt_to_t *head = &hrti->timer_list;
	wlc_hrt_to_cb_fn fun;
	bool expired;

	if ((this = head->next) == NULL)
		return;

	do {
		expired = FALSE;

		wlc_hrt_update_timeout_list(hrti);

		/* Always start from the head */
		while (((this = head->next) != NULL) && (this->expired == TRUE)) {
			head->next = this->next;
			fun = this->fun;
			this->fun = NULL;
			this->expired = FALSE;
			fun(this->arg);
			/* This flag tells us we've run timeout functions */
			expired = TRUE;
		}
		/* if we're out of timeout elements, then no more updates necessary
		 * even if we've run timeout functions
		 */
		if (head->next == NULL) {
			expired = FALSE;
		}
		/* if we've run timeout functions and timer not restarted, need to
		 * update the timeouts again in case one of the timeout functions
		 * we just ran took a long time.
		 * We should do this anyway just to take into account the times used up
		 * by the timeout functions.
		 */
	} while ((expired == TRUE) && (hrti->timer_armed == FALSE));
}

#if defined(BCMDBG)
static int
wlc_hrt_dump(void *ctx, struct bcmstrbuf *b)
{
	wlc_hrt_info_t *hrti = (wlc_hrt_info_t *)ctx;
	wlc_hrt_to_t *this;

	bcm_bprintf(b, "get_time %p set_timer %p act_timer %p context %p max_time 0x%x\n",
	            OSL_OBFUSCATE_BUF(hrti->get_time), OSL_OBFUSCATE_BUF(hrti->set_timer),
			OSL_OBFUSCATE_BUF(hrti->ack_timer),
	            OSL_OBFUSCATE_BUF(hrti->ctx), hrti->max_timer_val);
	bcm_bprintf(b, "timer_allowed %d timer_armed %d last_time 0x%x\n",
	            hrti->timer_allowed, hrti->timer_armed, hrti->last_time);

	for (this = hrti->timer_list.next; this != NULL; this = this->next) {
		bcm_bprintf(b, "timer %p, fun %p, arg %p, %d TO units\n",
		            OSL_OBFUSCATE_BUF(this), OSL_OBFUSCATE_BUF(this->fun),
				OSL_OBFUSCATE_BUF(this->arg), this->timeout);
	}

	return BCME_OK;
}
#endif // endif

/* Add timeout to the list */
bool
wlc_hrt_add_timeout(wlc_hrt_to_t *to, uint timeout, wlc_hrt_to_cb_fn fun, void *arg)
{
	wlc_hrt_info_t *hrti = to->hrti;
	wlc_hrt_to_t *prev, *this;
	uint delta_to = timeout;

	ASSERT(fun);

	if (fun == NULL) {
		WL_ERROR(("%s(): callback fun is NULL!\n", __FUNCTION__));
		return FALSE;
	}

#ifdef BCM_BACKPLANE_TIMEOUT
	if (si_deviceremoved(hrti->wlc->pub->sih)) {
		/* New timers are not allowed while power cycle is in progress */
		return FALSE;
	}
#endif /* BCM_BACKPLANE_TIMEOUT */

	if (to->fun != NULL) {
		WL_INFORM(("fun not null in 0x%p, timer already in list?\n",
			OSL_OBFUSCATE_BUF(to)));
		return FALSE;
	}

	if (timeout == 0) {
		WL_INFORM(("timeout is 0\n"));
		return FALSE;
	}

	if (!hrti->timer_allowed) {
		WL_INFORM(("no timer is allowed\n"));
		return FALSE;
	}

	wlc_hrt_update_timeout_list(hrti);

	/* find a proper location for the new timeout and update the timer value */
	prev = &hrti->timer_list;
	while (((this = prev->next) != NULL) && (this->timeout <= delta_to)) {
		delta_to -= this->timeout;
		prev = this;
	}
	if (this != NULL)
		this->timeout -= delta_to;

	to->fun = fun;
	to->arg = arg;
	to->timeout = delta_to;

	/* insert the new timeout */
	to->next = this;
	prev->next = to;

	if (to == hrti->timer_list.next) {
		hrti->set_timer(hrti->ctx, to->timeout);
		hrti->timer_armed = TRUE;
	}

	return TRUE;
}

/* Remove specified timeout from the list */
void
wlc_hrt_del_timeout(wlc_hrt_to_t *to)
{
	wlc_hrt_info_t *hrti = to->hrti;
	wlc_hrt_to_t *this;
	wlc_hrt_to_t *prev = &hrti->timer_list;

	while (((this = prev->next) != NULL)) {
		if (this == to) {
			if (this->next != NULL)
				this->next->timeout += this->timeout;
			prev->next = this->next;
			this->fun = NULL;
			break;
		}
		prev = this;
	}
}

/* s/w timer object alloc/free */
wlc_hrt_to_t *
wlc_hrt_alloc_timeout(wlc_hrt_info_t *hrti)
{
	wlc_info_t *wlc = hrti->wlc;
	wlc_hrt_to_t *to;

	if ((to = (wlc_hrt_to_t *)
	     MALLOCZ(wlc->osh, sizeof(wlc_hrt_to_t))) == NULL) {
		WL_ERROR(("%s: failed to allocate memory for hrtime timeout obj\n",
		          __FUNCTION__));
		return NULL;
	}
	to->hrti = hrti;

	return to;
}

void
wlc_hrt_free_timeout(wlc_hrt_to_t *to)
{
	wlc_hrt_info_t *hrti = to->hrti;
	wlc_info_t *wlc = hrti->wlc;

	/* ensure deleted prior to freeing */
	wlc_hrt_del_timeout(to);

	MFREE(wlc->osh, to, sizeof(wlc_hrt_to_t));
}

/* accessors */
uint
wlc_hrt_gettime(wlc_hrt_info_t *hrti)
{
	ASSERT(hrti->get_time != NULL);
#ifndef DONGLEBUILD
	ASSERT(hrti->wlc->clk);
#endif /* DONGLEBUILD */

	return hrti->get_time(hrti->ctx);
}

uint
wlc_hrt_getdelta(wlc_hrt_info_t *hrti, uint *last)
{
	uint curr_time, last_time;
	uint delta;

	ASSERT(last != NULL);

	curr_time = wlc_hrt_gettime(hrti);
	last_time = *last;
	/* put in condition to check for wrap around */
	if (curr_time < last_time) {
		delta = hrti->max_timer_val - last_time + curr_time + 1;
	}
	else {
		delta = curr_time - last_time;
	}
	*last = curr_time;

	return delta;
}

/* gptimer specific helper functions for setting the
 * get_time, set_timer, ack_timer functions in the
 * wlc_hrt_info_t object for wlc->hrti.
 */
/* GP timer is a freerunning 32 bit counter, decrements at 1 us rate */
void
wlc_hrt_gptimer_set(wlc_info_t *wlc, uint us)
{
	ASSERT(wlc->pub->corerev >= 3); /* no gptimer in earlier revs */

	W_REG(wlc->osh, D11_gptimer(wlc), us);
}

uint32
wlc_hrt_gptimer_get(wlc_info_t *wlc)
{
	ASSERT(wlc->pub->corerev >= 3); /* no gptimer in earlier revs */

	return (R_REG(wlc->osh, D11_gptimer(wlc)));
}

bool
wlc_hrt_timer_empty(wlc_hrt_info_t *hrti)
{
	wlc_hrt_to_t *prev = &hrti->timer_list;
	return (prev->next == NULL);
}

#ifndef DONGLEBUILD
static uint
wlc_hrt_gptimer_gettime(void *ctx)
{
	wlc_hrt_info_t *hrti = (wlc_hrt_info_t *)ctx;
	wlc_info_t *wlc = hrti->wlc;
	return (uint)R_REG(wlc->osh, D11_TSFTimerLow(wlc));
}

static void
wlc_hrt_gptimer_settimer(void *ctx, uint timeout)
{
	wlc_hrt_info_t *hrti = (wlc_hrt_info_t *)ctx;
	wlc_info_t *wlc = hrti->wlc;

	wlc_hrt_gptimer_set(wlc, timeout);
}

static void
wlc_hrt_gptimer_ack(void *ctx)
{
	wlc_hrt_info_t *hrti = (wlc_hrt_info_t *)ctx;
	wlc_info_t *wlc = hrti->wlc;

	wlc_hrt_gptimer_set(wlc, 0);
}

#else

static void
_wlc_hrt_timer_cb(hnd_timer_t *hrt)
{
	wlc_hrt_info_t *hrti = (wlc_hrt_info_t *)hnd_timer_get_ctx(hrt);

	if (SRPWR_ENAB()) {
		wlc_srpwr_request_on_all(hrti->wlc, FALSE);
	}

	wlc_hrt_timeout_handler(hrti);
}

static uint
wlc_hrt_gptimer_gettime(void *ctx)
{
	return (uint)hnd_time_us();
}

static void
wlc_hrt_gptimer_settimer(void *ctx, uint timeout)
{
	wlc_hrt_info_t *hrti = (wlc_hrt_info_t *)ctx;
	hnd_timer_start_us(hrti->pmu_timer, timeout, FALSE);
}

static void
wlc_hrt_gptimer_ack(void *ctx)
{
	wlc_hrt_info_t *hrti = (wlc_hrt_info_t *)ctx;
	hnd_timer_stop(hrti->pmu_timer);
}
#endif /* DONGLEBUILD */
/* XXX: the two functions below:
 * wlc_hrt_gptimer_abort() and wlc_hrt_gptimer_cb() are no longer used.
 * However 43236b0 roml-usb-ag-cdc builds were failing due to
 * wlc_hrt_gptimer_cb() not being there.
 * For now, as a quick fix, put these functions back in even though not used.
 */
void wlc_hrt_gptimer_abort(wlc_info_t *wlc) { BCM_REFERENCE(wlc); }
void wlc_hrt_gptimer_cb(wlc_info_t *wlc) { BCM_REFERENCE(wlc); }
