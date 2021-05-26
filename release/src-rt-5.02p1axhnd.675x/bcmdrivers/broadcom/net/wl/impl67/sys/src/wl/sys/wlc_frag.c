/*
 * 802.11 frame fragmentation handling module
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
 * $Id: wlc_frag.c 784805 2020-03-05 20:08:26Z $
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wlc_frmutil.h>
#include <wlc_frag.h>

/* module info */
struct wlc_frag_info {
	wlc_info_t *wlc;
	int scbh;
};

/* scb cubby */
typedef struct scb_frag {
	void	*fragbuf[NUMPRIO];	/**< defragmentation buffer per prio */
	uint16	fragresid[NUMPRIO];	/**< #bytes unused in frag buffer per prio */
} scb_frag_t;

/* handy macros to access the scb cubby and the frag states */
#define SCB_FRAG_LOC(frag, scb) (scb_frag_t **)SCB_CUBBY(scb, (frag)->scbh)
#define SCB_FRAG(frag, scb) *SCB_FRAG_LOC(frag, scb)

/* local decalarations */
static int wlc_frag_scb_init(void *ctx, struct scb *scb);
static void wlc_frag_scb_deinit(void *ctx, struct scb *scb);
static uint wlc_frag_scb_secsz(void *ctx, struct scb *scb);
#if defined(BCMDBG)
static void wlc_frag_scb_dump(void *ctx, struct scb *scb, struct bcmstrbuf *b);
#else
#define wlc_frag_scb_dump NULL
#endif // endif

static void wlc_defrag_prog_cleanup(wlc_frag_info_t *frag, struct scb *scb,
	uint8 prio);

static void wlc_lastfrag(wlc_info_t *wlc, struct wlc_frminfo *f);
static void wlc_appendfrag(wlc_info_t *wlc, void *fragbuf, uint16 *fragresid,
	uint8 *body, uint body_len);

/* module attach/detach interfaces */
wlc_frag_info_t *
BCMATTACHFN(wlc_frag_attach)(wlc_info_t *wlc)
{
	scb_cubby_params_t cubby_params;
	wlc_frag_info_t *frag;

	if ((frag = MALLOCZ(wlc->osh, sizeof(*frag))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	frag->wlc = wlc;

	/* reserve some space in scb container for frag states */
	bzero(&cubby_params, sizeof(cubby_params));

	cubby_params.context = frag;
	cubby_params.fn_init = wlc_frag_scb_init;
	cubby_params.fn_deinit = wlc_frag_scb_deinit;
	cubby_params.fn_dump = wlc_frag_scb_dump;
	cubby_params.fn_secsz = wlc_frag_scb_secsz;

	frag->scbh = wlc_scb_cubby_reserve_ext(wlc, sizeof(scb_frag_t *), &cubby_params);

	if (frag->scbh < 0) {
		WL_ERROR(("wl%d: %s: wlc_scb_cubby_reserve_ext failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	return frag;

fail:
	MODULE_DETACH(frag, wlc_frag_detach);
	return NULL;
}

void
BCMATTACHFN(wlc_frag_detach)(wlc_frag_info_t *frag)
{
	wlc_info_t *wlc;

	if (frag == NULL)
		return;

	wlc = frag->wlc;
	BCM_REFERENCE(wlc);
	MFREE(wlc->osh, frag, sizeof(*frag));
}

/* scb cubby interfaces */
static int
wlc_frag_scb_init(void *ctx, struct scb *scb)
{
	wlc_frag_info_t *frag = (wlc_frag_info_t *)ctx;
	wlc_info_t *wlc = frag->wlc;
	scb_frag_t **pscb_frag = SCB_FRAG_LOC(frag, scb);
	scb_frag_t *scb_frag;

	BCM_REFERENCE(wlc);

	if (SCB_INTERNAL(scb))
		return BCME_OK;

	if ((scb_frag = wlc_scb_sec_cubby_alloc(wlc, scb, sizeof(*scb_frag))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return BCME_NOMEM;
	}
	*pscb_frag = scb_frag;

	return BCME_OK;
}

static void
wlc_frag_scb_deinit(void *ctx, struct scb *scb)
{
	wlc_frag_info_t *frag = (wlc_frag_info_t *)ctx;
	wlc_info_t *wlc = frag->wlc;
	scb_frag_t **pscb_frag = SCB_FRAG_LOC(frag, scb);
	scb_frag_t *scb_frag = SCB_FRAG(frag, scb);
	uint8 prio;

	BCM_REFERENCE(wlc);

	/* free any frame reassembly buffer */
	for (prio = 0; prio < NUMPRIO; prio++) {
		wlc_defrag_prog_reset(frag, scb, prio, 0);
	}

	if (scb_frag != NULL) {
		wlc_scb_sec_cubby_free(wlc, scb, scb_frag);
	}
	*pscb_frag = NULL;
}

static uint
wlc_frag_scb_secsz(void *ctx, struct scb *scb)
{
	if (SCB_INTERNAL(scb))
		return 0;
	return sizeof(scb_frag_t);
}

#if defined(BCMDBG)
static void
wlc_frag_scb_dump(void *ctx, struct scb *scb, struct bcmstrbuf *b)
{
	wlc_frag_info_t *frag = (wlc_frag_info_t *)ctx;
	scb_frag_t *scb_frag = SCB_FRAG(frag, scb);
	uint8 prio;

	if (scb_frag == NULL)
		return;

	bcm_bprintf(b, "     defrag:%d ", (scb->flags & SCB_DEFRAG_INPROG) != 0);
	for (prio = 0; prio < NUMPRIO; prio++) {
		bcm_bprintf(b, "(%u:%d)", prio, scb_frag->fragbuf[prio] != NULL);
	}
	bcm_bprintf(b, "\n");
}
#endif // endif

/* rx defragmentation interfaces */

/* reset the state machine */
void
wlc_defrag_prog_reset(wlc_frag_info_t *frag, struct scb *scb, uint8 prio,
	uint16 prev_seqctl)
{
	wlc_info_t *wlc = frag->wlc;
	scb_frag_t *scb_frag = SCB_FRAG(frag, scb);
#ifdef BCMDBG_ERR
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG_ERR */

	BCM_REFERENCE(wlc);

	if (scb_frag != NULL && scb_frag->fragbuf[prio] != NULL) {
		WL_ERROR(("wl%d: %s: discarding partial "
		          "MSDU %03x with prio %d received from %s\n",
		          wlc->pub->unit, __FUNCTION__,
		          prev_seqctl >> SEQNUM_SHIFT, prio,
		          bcm_ether_ntoa(&scb->ea, eabuf)));
		PKTFREE(wlc->osh, scb_frag->fragbuf[prio], FALSE);
		wlc_defrag_prog_cleanup(frag, scb, prio);
	}
}

static void
wlc_defrag_scb_upd(struct scb *scb, struct wlc_frminfo *f)
{

	if (f->body_len >= DOT11_LLC_SNAP_HDR_LEN &&
		!bcmp(wlc_802_1x_hdr, f->pbody, DOT11_LLC_SNAP_HDR_LEN)) {
		scb->flags |= SCB_8021XHDR;
	}
	else {
		scb->flags &= ~SCB_8021XHDR;
	}

#ifdef BCMWAPI_WAI
	if (f->body_len >= DOT11_LLC_SNAP_HDR_LEN &&
		WAPI_WAI_SNAP(f->pbody)) {
		scb->flags2 |= SCB2_WAIHDR;
	} else {
		scb->flags2 &= ~SCB2_WAIHDR;
	}
#endif /* BCMWAPI_WAI */
}

/* process the first frag.
 * return TRUE when success FALSE otherwise
 */
bool
wlc_defrag_first_frag_proc(wlc_frag_info_t *frag, struct scb *scb, uint8 prio,
	struct wlc_frminfo *f)
{
	wlc_info_t *wlc = frag->wlc;
	osl_t *osh = wlc->osh;
	uint rxbufsz = wlc->pub->tunables->rxbufsz;
	scb_frag_t *scb_frag = SCB_FRAG(frag, scb);
	void *p = f->p;

	/* map the contents of 1st frag */
	PKTCTFMAP(osh, p);

	/* save packet for reassembly */
	scb_frag->fragbuf[prio] = p;

	if ((uint)((PKTTAILROOM(osh, p) + PKTHEADROOM(osh, p) + PKTLEN(osh, p))) < rxbufsz) {
		scb_frag->fragbuf[prio] = PKTGET(osh, rxbufsz, FALSE);
		if (scb_frag->fragbuf[prio] == NULL) {
			WL_ERROR(("wl%d: %s(): Allocate %d rxbuf for"
			          " frag pkt failed!\n",
			          wlc->pub->unit, __FUNCTION__,
			          rxbufsz));
			RX_PKTDROP_COUNT(wlc, scb, RX_PKTDROP_RSN_NO_BUF);
			WLCIFCNTINCR(scb, rxnobuf);
			goto toss;
		}
		memcpy(PKTDATA(osh, scb_frag->fragbuf[prio]),
		       (PKTDATA(osh, p) - PKTHEADROOM(osh, p)),
		       (PKTHEADROOM(osh, p) + PKTLEN(osh, p)));
		PKTPULL(osh, scb_frag->fragbuf[prio],
		        (int)PKTHEADROOM(osh, p));
		PKTSETLEN(osh, scb_frag->fragbuf[prio], PKTLEN(osh, p));
	}

	scb_frag->fragresid[prio] = (uint16)PKTTAILROOM(osh, p);

	scb->flags |= SCB_DEFRAG_INPROG;

	/* old pkt accessed by frminfo ptr */
	wlc_defrag_scb_upd(scb, f);

	if (scb_frag->fragbuf[prio] != p) {
		/* realloc'ed pkt - free old pkt */
		PKTFREE(osh, p, FALSE);

		/* NULL references to old pkt */
		f->p = NULL;
		f->pbody = NULL;
		f->h = NULL;
	}

	return TRUE;

toss:
	return FALSE;
}

/* process the remaining frags
 * return TRUE when success FALSE otherwise
 */
bool
wlc_defrag_other_frag_proc(wlc_frag_info_t *frag, struct scb *scb, uint8 prio,
	struct wlc_frminfo *f, uint16 prev_seqctl, bool more_frag, bool seq_chk_only)
{
	wlc_info_t *wlc = frag->wlc;
	osl_t *osh = wlc->osh;
	scb_frag_t *scb_frag = SCB_FRAG(frag, scb);
	void *p = f->p;

	BCM_REFERENCE(wlc);

	/* Make sure this MPDU:
	 * - matches the partially-received MSDU
	 * - is the one we expect (next in sequence)
	 */
	if (((f->seq & ~FRAGNUM_MASK) != (prev_seqctl & ~FRAGNUM_MASK)) ||
	    ((f->seq & FRAGNUM_MASK) != ((prev_seqctl & FRAGNUM_MASK) + 1))) {
		/* discard the partially-received MSDU */
		wlc_defrag_prog_reset(frag, scb, prio, prev_seqctl);

		/* discard the MPDU */
		WL_ERROR(("wl%d: %s: discarding MPDU %04x with "
		           "prio %d; previous MPDUs missed\n",
		           wlc->pub->unit, __FUNCTION__,
		           f->seq, prio));
		goto toss;
	}

	if (!seq_chk_only) {

		/*
		 * This isn't the first frag, but we don't have a partially-
		 * received MSDU.  We must have somehow missed the previous
		 * frags or timed-out the partially-received MSDU (not implemented yet).
		 */

		if (scb_frag->fragbuf[prio] == NULL) {
			WL_ERROR(("wl%d: %s: discarding MPDU %04x with "
			          "prio %d; previous MPDUs missed or partially-received "
			          "MSDU timed-out\n", wlc->pub->unit, __FUNCTION__,
			          f->seq, prio));
			goto toss;
		}

		/* detect fragbuf overflow */
		if (f->body_len > scb_frag->fragresid[prio]) {
			/* discard the partially-received MSDU */
			wlc_defrag_prog_reset(frag, scb, prio, prev_seqctl);

			/* discard the MPDU */
			WL_ERROR(("wl%d: %s: discarding MPDU %04x "
			          "with prio %d; resulting MSDU too big\n",
			          wlc->pub->unit, __FUNCTION__,
			          f->seq, prio));
			goto toss;
		}

		/* map the contents of each subsequent frag before copying */
		PKTCTFMAP(osh, p);

		/* copy frame into fragbuf */
		wlc_appendfrag(wlc, scb_frag->fragbuf[prio], &scb_frag->fragresid[prio],
		               f->pbody, f->body_len);

		PKTFREE(osh, p, FALSE);

		/* last frag. */
		if (!more_frag) {
			f->p = scb_frag->fragbuf[prio];

			wlc_defrag_prog_cleanup(frag, scb, prio);

			wlc_lastfrag(wlc, f);
		}
	}

	return TRUE;

toss:
	WLCNTINCR(wlc->pub->_cnt->rxfragerr);
	WLCIFCNTINCR(scb, rxfragerr);
	return FALSE;
}

static void
wlc_defrag_prog_cleanup(wlc_frag_info_t *frag, struct scb *scb, uint8 prio)
{
	scb_frag_t *scb_frag = SCB_FRAG(frag, scb);

	/* clear the packet pointer */
	scb_frag->fragbuf[prio] = NULL;
	scb_frag->fragresid[prio] = 0;

	/* clear the scb flag - note it's a summary flag */
	for (prio = 0; prio < NUMPRIO; prio++) {
		if (scb_frag->fragbuf[prio] != NULL)
			return;
	}
	scb->flags &= ~SCB_DEFRAG_INPROG;
}

static void
wlc_lastfrag(wlc_info_t *wlc, struct wlc_frminfo *f)
{
	osl_t *osh = wlc->osh;
	uint extra_hdrlen;

	/* reset packet pointers to beginning */
	f->h = (struct dot11_header *)PKTDATA(osh, f->p);
	f->len = PKTLEN(osh, f->p);
	f->pbody = (uchar*)(f->h) + DOT11_A3_HDR_LEN;
	f->body_len = f->len - DOT11_A3_HDR_LEN;
	extra_hdrlen = 0;
	if (f->wds) {
		extra_hdrlen += ETHER_ADDR_LEN;
	}
	if (f->qos) {
		extra_hdrlen += DOT11_QOS_LEN;
	}
	if (f->htc) {
		extra_hdrlen += DOT11_HTC_LEN;
	}
	if (f->rx_wep && f->key) {
		extra_hdrlen += f->key_info.iv_len;
	}
	f->pbody += extra_hdrlen;
	f->body_len -= extra_hdrlen;
	f->fc = ltoh16(f->h->fc);
}

static void
wlc_appendfrag(wlc_info_t *wlc, void *fragbuf, uint16 *fragresid,
               uint8 *body, uint body_len)
{
	osl_t *osh = wlc->osh;
	uint8 *dst;
	uint fraglen;

	/* append frag payload to end of partial packet */
	fraglen = PKTLEN(osh, fragbuf);
	dst = PKTDATA(osh, fragbuf) + fraglen;
	bcopy(body, dst, body_len);
	PKTSETLEN(osh, fragbuf, (fraglen + body_len));
	*fragresid -= (uint16)body_len;
}

bool
wlc_defrag_in_progress(wlc_frag_info_t *frag, struct scb *scb, uint8 prio)
{
	scb_frag_t *scb_frag = SCB_FRAG(frag, scb);

	/* The fragbuf[] pointer will be non-null if defragmentation is in progress */
	return (scb_frag && scb_frag->fragbuf[prio]);
}
