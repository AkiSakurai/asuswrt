/*
 * Packet fetch (from host) source.
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
 * $Id: rte_pktfetch.c 785625 2020-04-01 07:39:38Z $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <hnd_pktq.h>
#include <rte_fetch.h>
#include <rte_pktfetch.h>
#include <rte_cons.h>
#include <rte_timer.h>

#ifdef BCMDBG
#define PFETCH_MSG(x) printf x
#else
#define PFETCH_MSG(x)
#endif // endif

/* No. of fetch_rqsts prealloced for the PKTFETCH module. This needs to be a tunable */
#ifndef PKTFETCH_MAX_FETCH_RQST
#define PKTFETCH_MAX_FETCH_RQST	8
#endif // endif
/* PKTFETCH LBUF Q size can never exceed the no. of fetch_rqsts allocated for use by the module */
#define PKTFETCH_LBUF_QSIZE PKTFETCH_MAX_FETCH_RQST

struct pktfetch_rqstpool {
	struct fetch_rqst *head;
	struct fetch_rqst *tail;
	uint16 len;
	uint16 max;
};

enum pktfetchq_state {
	INITED = 0x00,
	STOPPED_UNABLE_TO_GET_LBUF,
	STOPPED_UNABLE_TO_GET_FETCH_RQST
};

struct pktfetch_q {
	struct pktfetch_info *head;
	struct pktfetch_info *tail;
	uint8 state;
	uint16 count;
};

#ifdef BCMDBG
/* For average value counters, no. of calls to skip before polling
 * This MUST be power of 2
 */
#define PKTFETCH_STATS_SKIP_CNT 32
typedef struct pktfetch_debug_stats {
	uint16 pktfetch_rqsts_rcvd;
	uint16 pktfetch_rqsts_rejected;
	uint16 pktfetch_succesfully_dispatched;
	uint16 pktfetch_dispatch_failed_nomem;
	uint16 pktfetch_dispatch_failed_no_fetch_rqst;
	uint16 pktfetch_rqsts_enqued;
	uint16 pktfetch_wake_pktfetchq_dispatches;
	uint32 pktfetch_qsize_accumulator;
	uint16 pktfetch_qsize_poll_count;
} pktfetch_debug_stats_t;
#endif /* BCMDBG */

struct pktfetch_rqstpool *pktfetchpool = NULL;
struct pktfetch_q *pktfetchq = NULL;
struct pktq *pktfetch_lbuf_q = NULL; /**< multi priority queue */

#ifdef BCMDBG
pktfetch_debug_stats_t pktfetch_stats;
#if defined(RTE_CONS)
static hnd_timer_t *pktfetch_stats_timer;
#endif // endif
#define PKTFETCH_STATS_DELAY 4000
#endif /* BCMDBG */

#if defined(BCMDBG) && defined(RTE_CONS)
static void hnd_update_pktfetch_stats(hnd_timer_t *t);
static void hnd_dump_pktfetch_stats(void *arg, int argc, char *argv[]);
#endif /* BDMDBG && RTE_CONS */

/* PktFetch */
static void hnd_pktfetch_completion_handler(struct fetch_rqst *fr, bool cancelled);
static struct pktfetch_info* hnd_deque_pktfetch(void);
static void hnd_enque_pktfetch(struct pktfetch_info *pinfo, bool enque_to_head);
static void hnd_pktfetchpool_reclaim(struct fetch_rqst *fr);
static struct fetch_rqst* hnd_pktfetchpool_get(void);
static int hnd_pktfetch_dispatch(struct pktfetch_info *pinfo);

static void hnd_lbuf_free_cb(struct lbuf *lb);
static void hnd_wake_pktfetchq(void);
static void hnd_flush_pktfetchq(void);

#if defined(BCMDBG) && defined(RTE_CONS)
static void
hnd_dump_pktfetch_stats(void *arg, int argc, char *argv[])
{
	hnd_timer_start(pktfetch_stats_timer, PKTFETCH_STATS_DELAY, 0);
	bzero(&pktfetch_stats, sizeof(pktfetch_debug_stats_t));
}

static void
hnd_update_pktfetch_stats(hnd_timer_t *t)
{
	printf("pktfetch_rqsts_rcvd = %d\n", pktfetch_stats.pktfetch_rqsts_rcvd);
	printf("pktfetch_rqsts_rejected = %d\n", pktfetch_stats.pktfetch_rqsts_rejected);
	printf("pktfetch_rqsts_enqued = %d\n", pktfetch_stats.pktfetch_rqsts_enqued);
	printf("pktfetch_dispatch_failed_nomem = %d\n",
		pktfetch_stats.pktfetch_dispatch_failed_nomem);
	printf("pktfetch_dispatch_failed_no_fetch_rqst = %d\n",
		pktfetch_stats.pktfetch_dispatch_failed_no_fetch_rqst);
	printf("pktfetch_succesfully_dispatched = %d\n",
		pktfetch_stats.pktfetch_succesfully_dispatched);
	printf("pktfetch_wake_pktfetchq_dispatches = %d\n",
		pktfetch_stats.pktfetch_wake_pktfetchq_dispatches);
	printf("average pktfetch Q size in %d secs = %d\n", PKTFETCH_STATS_DELAY,
	(pktfetch_stats.pktfetch_qsize_accumulator/pktfetch_stats.pktfetch_qsize_poll_count));
	bzero(&pktfetch_stats, sizeof(pktfetch_debug_stats_t));
}
#endif /* BCMDBG && RTE_CONS */

/* PktFetch module */
void
BCMATTACHFN(hnd_pktfetch_module_init)(si_t *sih, osl_t *osh)
{
	/* Initialize the pktfetch queue */
	pktfetchq = MALLOCZ(osh, sizeof(struct pktfetch_q));
	if (pktfetchq == NULL) {
		PFETCH_MSG(("%s: Unable to alloc pktfetch queue: Out of mem?\n", __FUNCTION__));
		return;
	}

	/* Initialize the pktfetch rqstpool */
	pktfetchpool = MALLOCZ(osh, sizeof(struct pktfetch_rqstpool));
	if (pktfetchpool == NULL) {
		PFETCH_MSG(("%s: Unable to alloc rqstpool: Out of mem?\n", __FUNCTION__));
		MFREE(osh, pktfetchq, sizeof(struct pktfetch_q));
		return;
	}

	pktfetch_lbuf_q = MALLOCZ(osh, sizeof(struct pktq));
	if (pktfetch_lbuf_q == NULL) {
		PFETCH_MSG(("%s: Unable to alloc pktfetch_lbuf_q: Out of mem?\n", __FUNCTION__));
		MFREE(osh, pktfetchq, sizeof(struct pktfetch_q));
		MFREE(osh, pktfetchpool, sizeof(struct pktfetch_rqstpool));
		return;
	}

	hnd_pktfetchpool_init(osh);

	/* Initialize the pktfetch lbuf Q */
	pktq_init(pktfetch_lbuf_q, 1, PKTFETCH_LBUF_QSIZE);

#if defined(BCMDBG) && defined(RTE_CONS)
	pktfetch_stats_timer = hnd_timer_create((void *)sih, NULL,
	                                         hnd_update_pktfetch_stats, NULL, NULL);

	if ((pktfetch_stats_timer == NULL) ||
		(!hnd_cons_add_cmd("pfstats", hnd_dump_pktfetch_stats, NULL))) {

		if (pktfetch_stats_timer != NULL) {
			PFETCH_MSG(("%s: hnd_cons_add_cmd() failed: Out of mem?\n",
			__FUNCTION__));

			hnd_timer_free(pktfetch_stats_timer);
			pktfetch_stats_timer = NULL;
		}
		else
			PFETCH_MSG(("%s: pktfetch_stats_timer create failed: Out of mem?\n",
			__FUNCTION__));
		pktq_deinit(pktfetch_lbuf_q);
		MFREE(osh, pktfetchq, sizeof(struct pktfetch_q));
		MFREE(osh, pktfetchpool, sizeof(struct pktfetch_rqstpool));
		return;
	}
#endif /* defined(BCMDBG) && defined(RTE_CONS) */

	lbuf_free_cb_set(hnd_lbuf_free_cb);
}

void
BCMATTACHFN(hnd_pktfetchpool_init)(osl_t *osh)
{
	struct pktfetch_rqstpool *pool = pktfetchpool;
	struct fetch_rqst *fr;
	int i;

	ASSERT(pktfetchpool);
	for (i = 0; i < PKTFETCH_MAX_FETCH_RQST; i++) {
		fr = MALLOCZ(osh, sizeof(struct fetch_rqst));
		if (fr == NULL) {
			PFETCH_MSG(("%s: Out of memory. Alloced only %d fetch_rqsts!\n",
				__FUNCTION__, pool->len));
			break;
		}
		fr->osh = osh;
		if (pool->head == NULL)
			pool->head = pool->tail = fr;
		else {
			pool->tail->next = fr;
			pool->tail = fr;
		}
		pool->len++;
	}
	pool->max = pool->len;
}

int
hnd_pktfetch(struct pktfetch_info *pinfo)
{
	int ret;

#ifdef BCMDBG
	if ((pktfetch_stats.pktfetch_rqsts_rcvd & (PKTFETCH_STATS_SKIP_CNT-1)) == 0) {
		pktfetch_stats.pktfetch_qsize_poll_count++;
		pktfetch_stats.pktfetch_qsize_accumulator += pktfetchq->count;
	}
	pktfetch_stats.pktfetch_rqsts_rcvd++;
#endif // endif

	if (pktfetchq->count >= PKTFETCH_MAX_OUTSTANDING) {
		 /* check if we can dispatch any pending item from the queue first */
		 hnd_wake_pktfetchq();

		if (pktfetchq->count >= PKTFETCH_MAX_OUTSTANDING) {
#ifdef BCMDBG
			pktfetch_stats.pktfetch_rqsts_rejected++;
#endif // endif
			return BCME_ERROR;
		}
	}

	ASSERT(pinfo->lfrag);

	if (pktfetchq->head == NULL) {
		ret = hnd_pktfetch_dispatch(pinfo);
		switch (ret) {
			case BCME_OK:
#ifdef BCMDBG
				pktfetch_stats.pktfetch_succesfully_dispatched++;
#endif // endif
				return BCME_OK;
			case BCME_NOMEM:
				pktfetchq->state = STOPPED_UNABLE_TO_GET_LBUF;
#ifdef BCMDBG
				pktfetch_stats.pktfetch_dispatch_failed_nomem++;
#endif // endif
				break;
			case BCME_NORESOURCE:
				pktfetchq->state = STOPPED_UNABLE_TO_GET_FETCH_RQST;
#ifdef BCMDBG
				pktfetch_stats.pktfetch_dispatch_failed_no_fetch_rqst++;
#endif // endif
				break;
			default:
				printf("Unrecognised return value from hnd_pktfetch_dispatch\n");
		}
	}
	hnd_enque_pktfetch(pinfo, FALSE);
#ifdef BCMDBG
	pktfetch_stats.pktfetch_rqsts_enqued++;
#endif // endif

	return BCME_OK;
}

/* XXX, PP_LBUF :
 * RX:
 *   1. Get new lfrag from SHARED_POOL or HEAP
 *   2. Free orig_lfrag --> free RPH + RX.
 *   3. Use hwa_rph_allocate as before.
 *
 * TX:
 *   1. Get heap_buf (not new lfrag) from HEAP
 *   2. Copy data to heap_buf.
 *   3. PKTBUFEARLYFREE
 *   4. PKTSETBUF with heap_buf
 *   5. PKTSETBUFALLOC, and PKTSETHEAPBUF
 *   NOTE: Be careful at PageOut, silimar to Mgmt local packet.
 */
static int
hnd_pktfetch_dispatch(struct pktfetch_info *pinfo)
{
	struct fetch_rqst *fr;
	void *lfrag = pinfo->lfrag;
#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
	void *heap_buf = NULL;
	uint32 header_len;
#endif /* BCM_DHDHDR && DONGLEBUILD */
	void *lbuf = NULL;
	uint32 totlen;
	osl_t *osh = pinfo->osh;

	ASSERT(!PKTHASHEAPBUF(osh, lfrag));

	fr = hnd_pktfetchpool_get();

	if (fr == NULL)
		return BCME_NORESOURCE;

	ASSERT(fetch_info->pool);
	if (!PKTISTXFRAG(osh, lfrag)) {
		lbuf = pktpool_get(fetch_info->pool);
	}

	/* If pool_get fails, but we have sufficient memory in the heap, let's alloc from heap */
	if ((lbuf == NULL) && (OSL_MEM_AVAIL() > PKTFETCH_MIN_MEM_FOR_HEAPALLOC)) {
#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
		if (PKTISTXFRAG(osh, lfrag)) {
			heap_buf = hnd_malloc(MAXPKTDATABUFSZ);
		} else
#endif /* BCM_DHDHDR && DONGLEBUILD */
		{
			lbuf = PKTALLOC(osh, MAXPKTDATABUFSZ, lbuf_basic);
		}
	}

	if ((lbuf == NULL) &&
#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
		(heap_buf == NULL) &&
#endif /* BCM_DHDHDR && DONGLEBUILD */
		TRUE) {
		hnd_pktfetchpool_reclaim(fr);
		return BCME_NOMEM;
	}

	/* Unset Hi bit if High bit set */
	PHYSADDR64HISET(fr->haddr, (uint32)(PKTFRAGDATA_HI(osh, lfrag, LB_FRAG1) & 0x7fffffff));

	/* User provides its own context and callback
	 * But for pktfetch module, callback needs to be hnd_pktfetch_completion_handler(),
	 * so wrap original context and callback over fr->ctx,
	 * so that the fetch_rqst is returned to the pktfetch module
	 * after HostFetch is done with it (either fetch completed or cancelled)
	 * and pktfetch module takes over original cb/ctx handling.
	*/
	fr->cb = hnd_pktfetch_completion_handler;
	fr->ctx = pinfo;

#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
	if (heap_buf) {
		header_len = PKTLEN(osh, lfrag);
		bcopy((char*)PKTDATA(osh, lfrag), (char*)heap_buf + pinfo->headroom - header_len,
			header_len);
		PKTBUFEARLYFREE(osh, lfrag);
		PKTSETBUF(osh, lfrag, heap_buf, MAXPKTDATABUFSZ);
		PKTSETBUFALLOC(osh, lfrag);
		PKTSETHEAPBUF(osh, lfrag);
		lbuf = lfrag;
	}
#endif /* BCM_DHDHDR && DONGLEBUILD */

	totlen = PKTFRAGTOTLEN(osh, lfrag);
	totlen -= pinfo->host_offset;
	fr->size = totlen;

	totlen += pinfo->headroom;
	PKTSETLEN(osh, lbuf, totlen);
	PKTPULL(osh, lbuf, pinfo->headroom);
	PKTSETIFINDEX(osh, lbuf, PKTIFINDEX(osh, lfrag));
	fr->dest = PKTDATA(osh, lbuf);
#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
	if (heap_buf) {
		PKTPUSH(osh, lbuf, header_len);
	}
#endif /* BCM_DHDHDR && DONGLEBUILD */
	PHYSADDR64LOSET(fr->haddr, (uint32)(PKTFRAGDATA_LO(osh, lfrag, LB_FRAG1)
		+ pinfo->host_offset));
	/* NOTE: This will not fail */
	pktq_penq(pktfetch_lbuf_q, 0, lbuf);

	/* Store the lfrag pointer in the lbuf */
	PKTTAG_SET_VALUE(lbuf, lfrag);

	/* Store fr req */
	pinfo->fr = fr;

	hnd_fetch_rqst(fr);
	return BCME_OK;
}

static struct fetch_rqst *
hnd_pktfetchpool_get(void)
{
	struct pktfetch_rqstpool *pool = pktfetchpool;
	struct fetch_rqst *fr;

	ASSERT(pktfetchpool);
	if (pool->head == NULL) {
		ASSERT(pool->len == 0);
		return NULL;
	}
	else {
		fr = pool->head;
		pool->head = pool->head->next;
		pool->len--;
		fr->next = NULL;
	}
	return fr;
}

static void
hnd_pktfetchpool_reclaim(struct fetch_rqst *fr)
{
	struct pktfetch_rqstpool *pool = pktfetchpool;
	ASSERT(pktfetchpool);
	bzero(fr, sizeof(struct fetch_rqst));

	if (pool->head == NULL) {
		ASSERT(pool->len == 0);
		pool->head = pool->tail = fr;
		pool->len++;
	}
	else {
		pool->tail->next = fr;
		pool->tail = fr;
		pool->len++;
	}
}

static void
hnd_enque_pktfetch(struct pktfetch_info *pinfo, bool enque_to_head)
{
	struct pktfetch_q *pfq = pktfetchq;

	if (pfq->head == NULL)
		pfq->head = pfq->tail = pinfo;
	else {
		if (!enque_to_head) {
			pfq->tail->next = pinfo;
			pfq->tail = pinfo;
		}
		else {
			pinfo->next = pfq->head;
			pfq->head = pinfo;
		}
	}

	pfq->count++;
	pfq->tail->next = NULL;
}

static struct pktfetch_info *
hnd_deque_pktfetch(void)
{
	struct pktfetch_q *pfq = pktfetchq;
	struct pktfetch_info *pinfo;

	if (pfq->head == NULL)
		return NULL;
	else if (pfq->head == pfq->tail) {
		pinfo = pfq->head;
		pfq->head = NULL;
		pfq->tail = NULL;
		pfq->count--;
	} else {
		pinfo = pfq->head;
		pfq->head = pinfo->next;
		pfq->count--;
	}
	pinfo->next = NULL;
	return pinfo;
}

static void
hnd_wake_pktfetchq(void)
{
	struct pktfetch_q *pfq = pktfetchq;
	struct pktfetch_info *pinfo;
	int ret;

	while (pfq->head != NULL) {
		pinfo = hnd_deque_pktfetch();
		ret = hnd_pktfetch_dispatch(pinfo);

		if (ret < 0) {
			/* Reque fr to head of Q and breakout */
			hnd_enque_pktfetch(pinfo, TRUE);
			pfq->state = (ret == BCME_NORESOURCE ?
				STOPPED_UNABLE_TO_GET_FETCH_RQST : STOPPED_UNABLE_TO_GET_LBUF);
			return;
		}
#ifdef BCMDBG
		else
			pktfetch_stats.pktfetch_wake_pktfetchq_dispatches++;
#endif // endif
	}
	/* All requests dispatched. Reset Q state */
	pfq->state = INITED;
}

/* Callback for every PKTFREE from pktpool registered with the fetch_module */
static void
hnd_lbuf_free_cb(struct lbuf *lb)
{
	/* If freed lbuf belongs to HostFetch pool or there are entries in
	 * pktfetchq, invoke callback
	 */
	if (fetch_info &&
		((lb_getpool(lb) == fetch_info->pool) ||
		(pktfetchq->count > 0) ||
		(PKTISTXFRAG(OSH_NULL, lb) && PKTHASHEAPBUF(OSH_NULL, lb))))
		if (pktfetchq->state != INITED)
			hnd_wake_pktfetchq();
}

/* After HostFetch module is done, the fetch_rqst is returned to pktfetch module
 * pktfetch module does necessary processing and the actual callback

 * This handler frees up the original lfrag
 * and returns the fr to user via the original user callback
 * Also returns the fetch_rqst to pktfetch_pool
 * NOTE: PKTFETCH User must never free fetch_rqst!!!
 */
static void
hnd_pktfetch_completion_handler(struct fetch_rqst *fr, bool cancelled)
{
	struct pktfetch_info *pinfo = fr->ctx;
	pktfetch_cmplt_cb_t pktfetch_cb = pinfo->cb;
	void *lfrag, *lbuf;

	lbuf = pktq_pdeq(pktfetch_lbuf_q, 0);

	if (lbuf == NULL) {
		PFETCH_MSG(("%s: Expected lbuf not found in queue!\n", __FUNCTION__));
		ASSERT(0);
		return;
	}

	lfrag = (void *)PKTTAG_GET_VALUE(lbuf);
	PKTTAG_SET_VALUE(lbuf, 0);	/* Reset PKTTAG */

	/* Mark the lfrag as PKTFETCHED */
	PKTSETPKTFETCHED(fr->osh, lfrag);

	hnd_pktfetchpool_reclaim(fr);

	/* Do not free the original lfrag; Leave it to the user callback */
	if (pktfetch_cb)
		pktfetch_cb(lbuf, lfrag, pinfo, cancelled);
	else {
		PFETCH_MSG(("%s: No callback registered for pktfetch!\n", __FUNCTION__));
		ASSERT(0);
	}

	/* schedule new reqst from pktfetchq */
	if (pktfetchq->state != INITED)
		hnd_wake_pktfetchq();
}
/* Release the first level queue with just pinfo and ctx block alloced */
static void
hnd_flush_pktfetchq(void)
{
	struct pktfetch_q *pfq = pktfetchq;
	struct pktfetch_info *pinfo;

	if (pktfetchq->head == NULL)
		return;

	while (pfq->head != NULL) {
		/* Deque each entry */
		pinfo = hnd_deque_pktfetch();

		ASSERT(pinfo);
		ASSERT(pinfo->cb);

		/* Invoke the callback function to release the resources/buffers */
		pinfo->cb(NULL, pinfo->lfrag, pinfo, TRUE);
	}
}

/* Cancel all pending fetch requests */
void
hnd_cancel_pktfetch(void)
{
	/* Flush the packet fetchq */
	hnd_flush_pktfetchq();

	/* Flush the fetch request queue */
	hnd_flush_fetch_rqstq();

	/* Flush fetch complete queue pending in bus layer */
	hnd_flush_pciedev_fetch_cmpltq();
}
