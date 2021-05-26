/*
 * HostFetch module interface
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
 * $Id: rte_fetch.c 774140 2019-04-11 13:14:01Z $
 */

/** HostFetch: an infrastructure to get a block of data from host to device memory */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <rte_fetch.h>

#ifdef BCMDBG
#define FETCH_MSG(x) printf x
#else
#define FETCH_MSG(x)
#endif // endif

struct fetch_rqst_q_t {
	fetch_rqst_t *head;
	fetch_rqst_t *tail;
};

/* HostFetch module related info */
struct fetch_rqst_q_t *fetch_rqst_q = NULL;
struct fetch_module_info *fetch_info = NULL;

static void hnd_enque_fetch_rqst(struct fetch_rqst *fr, bool enque_to_head);
static int hnd_dispatch_fetch_rqst(struct fetch_rqst *fr);
static struct fetch_rqst *hnd_deque_fetch_rqst(void);

void
BCMATTACHFN(hnd_fetch_module_init)(osl_t *osh)
{
	fetch_info = MALLOCZ(osh, sizeof(struct fetch_module_info));
	if (fetch_info == NULL) {
		FETCH_MSG(("%s: Unable to init fetch module info!\n", __FUNCTION__));
		return;
	}

	fetch_rqst_q = MALLOCZ(osh, sizeof(struct fetch_rqst_q_t));
	if (fetch_rqst_q == NULL) {
		FETCH_MSG(("%s : Unable to initialize fetch_rqst_q!\n", __FUNCTION__));
		MFREE(osh, fetch_info, sizeof(struct fetch_module_info));
		return;
	}

	fetch_info->pool = SHARED_POOL;
}

void
hnd_fetch_bus_dispatch_cb_register(bus_dispatch_cb_t cb, void *arg)
{
	if (fetch_info == NULL) {
		FETCH_MSG(("fetch info not inited! Cannot register callback!\n"));
		ASSERT(0);
		return;
	}
	fetch_info->cb = cb;
	fetch_info->arg = arg;
}

/* Register the callback function to flush pending fetch requests in bus layer */
void
hnd_fetch_bus_flush_cb_register(bus_flush_cb_t cb, void *arg)
{
	if (fetch_info == NULL) {
		FETCH_MSG(("fetch info not inited! Cannot register callback!\n"));
		ASSERT(0);
		return;
	}
	fetch_info->flush_cb = cb;
}

void
hnd_fetch_rqst(struct fetch_rqst *fr)
{
	if (hnd_fetch_rqstq_empty()) {
		if (hnd_dispatch_fetch_rqst(fr) != BCME_OK)
			hnd_enque_fetch_rqst(fr, FALSE);
	} else {
		hnd_enque_fetch_rqst(fr, FALSE);
		hnd_wake_fetch_rqstq();
	}
}

static void
hnd_enque_fetch_rqst(struct fetch_rqst *fr, bool enque_to_head)
{
	struct fetch_rqst_q_t *frq = fetch_rqst_q;

	fr->next = NULL;

	if (frq->head == NULL)
		frq->head = frq->tail = fr;
	else {
		if (!enque_to_head) {
			frq->tail->next = fr;
			frq->tail = fr;
		} else {
			fr->next = frq->head;
			frq->head = fr;
		}
	}
}

/** called when the next item (in parameter 'fr') can be fetched from the host */
static int
hnd_dispatch_fetch_rqst(struct fetch_rqst *fr)
{
	int ret;

	/* Fetch module assumes a dest buffer is always allocated */
	ASSERT(fr->dest != NULL);

	/* Bus layer DMA dispatch */
	FETCH_RQST_FLAG_SET(fr, FETCH_RQST_IN_BUS_LAYER);
	if (fetch_info && fetch_info->cb)
		ret = fetch_info->cb(fr, fetch_info->arg);
	else {
		FETCH_MSG(("fetch_info or fetch_info callback is NULL!\n"));
		ASSERT(0);
		return BCME_ERROR;
	}

	return ret;
}

static struct fetch_rqst *
hnd_deque_fetch_rqst(void)
{
	struct fetch_rqst_q_t *frq = fetch_rqst_q;
	struct fetch_rqst *fr;

	if (frq->head == NULL)
		fr = NULL;
	else if (frq->head == frq->tail) {
		fr = frq->head;
		frq->head = NULL;
		frq->tail = NULL;
	} else {
		fr = frq->head;
		frq->head = frq->head->next;
		fr->next = NULL;
	}
	return fr;
}

bool
hnd_fetch_rqstq_empty(void)
{
	struct fetch_rqst_q_t *frq = fetch_rqst_q;

	return (frq->head == NULL);
}

void
hnd_wake_fetch_rqstq(void)
{
	struct fetch_rqst *fr;
	int ret;

	/* While frq not empty */
	while (!hnd_fetch_rqstq_empty())
	{
		/* Deque an fr */
		fr = hnd_deque_fetch_rqst();
		ret = hnd_dispatch_fetch_rqst(fr);

		if (ret != BCME_OK) {
			/* Reque fr to head of Q */
			hnd_enque_fetch_rqst(fr, TRUE);
			break;
		}
	}
}

void
hnd_flush_fetch_rqstq(void)
{
	struct fetch_rqst_q_t *frq = fetch_rqst_q;
	struct fetch_rqst *fr;

	while (frq->head != NULL) {
		fr = frq->head;
		frq->head = frq->head->next;
		if (fr->cb)
			fr->cb(fr, TRUE);
		else {
			FETCH_MSG(("%s: No callback registered for fetch_rqst!\n", __FUNCTION__));
			ASSERT(0);
		}
	}
	frq->tail = NULL;
}

/* Invoke the callback fn registered to flush the requests pending at bus side */
void
hnd_flush_pciedev_fetch_cmpltq(void)
{
	if (fetch_info && fetch_info->flush_cb && fetch_info->arg)
		fetch_info->flush_cb(fetch_info->arg);
	else {
		FETCH_MSG(("fetch_info or fetch_info flush callback is NULL!\n"));
		ASSERT(0);
	}
}

/* Generic callback invoked from the bus layer signifying
 * that a DMA descr was freed up / space available in the DMA queue
 */
void
hnd_dmadesc_avail_cb(void)
{
	if ((fetch_rqst_q)->head != NULL)
		hnd_wake_fetch_rqstq();
}
