/*
 * Routines related to the uses of Pktfetch in WLC
 *
 * Broadcom Proprietary and Confidential. Copyright (C) 2017,
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom.
 *
 * $Id: wlc_pktfetch.c 469595 2014-04-10 21:19:06Z $
 */

#include <wlc_types.h>
#include <bcmwpa.h>
#include <phy_utils_api.h>
#include <wlc.h>
#include <wlc_pub.h>
#include <wlc_pktfetch.h>
#ifdef WLAMPDU
#include <wlc_scb.h>
#endif
#include <wlc_ampdu_rx.h>
#include <wlc_keymgmt.h>
#include <wlc_bsscfg.h>

#ifdef BCMSPLITRX
#include <rte_pktfetch.h>
#include <wlc_rx.h>

static void wlc_recvdata_pktfetch_cb(void *lbuf, void *orig_lfrag,
	void *ctxt, bool cancelled);
#if defined(PKTC) || defined(PKTC_DONGLE)
static void wlc_sendup_pktfetch_cb(void *lbuf, void *orig_lfrag,
	void *ctxt, bool cancelled);
#endif
static void wlc_recreate_frameinfo(wlc_info_t *wlc, void *lbuf, void *lfrag,
	wlc_frminfo_t *fold, wlc_frminfo_t *fnew);

bool
wlc_pktfetch_required(wlc_info_t *wlc,	void *p, uchar *pbody, uint body_len,
	wlc_key_info_t *key_info, bool skip_iv)
{
	struct dot11_llc_snap_header *lsh;

	/* If IV needs to be accounted for, move past IV len */
	lsh = (struct dot11_llc_snap_header *)(pbody + (skip_iv ? key_info->iv_len : 0));

	if ((PKTFRAGUSEDLEN(wlc->osh, p) > 0) &&
		(EAPOL_PKTFETCH_REQUIRED(lsh) ||
#ifdef WLTDLS
		WLTDLS_PKTFETCH_REQUIRED(wlc, lsh) ||
#endif
#ifdef WLNDOE
		NDOE_PKTFETCH_REQUIRED(wlc, lsh, pbody, body_len) ||
#endif
#ifdef WOWLPF
		WOWLPF_ACTIVE(wlc->pub) ||
#endif
		FALSE))
		return TRUE;

	return FALSE;
}

void
wlc_recvdata_schedule_pktfetch(wlc_info_t *wlc, struct scb *scb,
	wlc_frminfo_t *f, bool promisc_frame, bool ordered)
{
	wlc_eapol_pktfetch_ctx_t *ctx = NULL;
	struct pktfetch_info *pinfo = NULL;

	pinfo = MALLOCZ(wlc->osh, sizeof(struct pktfetch_info));
	if (!pinfo) {
		WL_ERROR(("%s: Out of mem: Unable to alloc pktfetch_info!\n", __FUNCTION__));
		goto error;
	}

	ctx = MALLOCZ(wlc->osh, sizeof(wlc_eapol_pktfetch_ctx_t));
	if (!ctx) {
		WL_ERROR(("%s: Out of mem: Unable to alloc pktfetch ctx!\n", __FUNCTION__));
		goto error;
	}

	ctx->wlc = wlc;

	bcopy(f, &ctx->f, sizeof(wlc_frminfo_t));
#ifdef WLAMPDU
	if (SCB_AMPDU(scb) && !f->ismulti && !promisc_frame)
		ctx->ampdu_path = TRUE;
#endif
	ctx->ordered = ordered;
	ctx->promisc = promisc_frame;
	ctx->pinfo = pinfo;

	/* Headroom does not need to be > PKTRXFRAGSZ */
	pinfo->osh = wlc->osh;
	pinfo->headroom = PKTRXFRAGSZ;
	pinfo->host_offset = 0;
	pinfo->lfrag = f->p;
	pinfo->ctx = ctx;
	pinfo->cb = wlc_recvdata_pktfetch_cb;
#ifdef BCMPCIEDEV
	if (BCMPCIEDEV_ENAB() && hnd_pktfetch(pinfo) != BCME_OK) {
		WL_ERROR(("%s: pktfetch request rejected\n", __FUNCTION__));
		goto error;
	}
#endif /* BCMPCIEDEV */

	return;

error:
	if (pinfo)
		MFREE(wlc->osh, pinfo, sizeof(struct pktfetch_info));

	if (ctx)
		MFREE(wlc->osh, ctx, sizeof(wlc_eapol_pktfetch_ctx_t));

	if (f->p)
		PKTFREE(wlc->osh, f->p, FALSE);
}

static void
wlc_recvdata_pktfetch_cb(void *lbuf, void *orig_lfrag, void *ctxt, bool cancelled)
{
	wlc_eapol_pktfetch_ctx_t *ctx = ctxt;
	wlc_info_t *wlc = ctx->wlc;
	struct scb *scb = NULL;
	osl_t *osh = wlc->osh;
	wlc_frminfo_t f;
	bool ampdu_path = ctx->ampdu_path;
	bool ordered = ctx->ordered;
	bool promisc = ctx->promisc;
	wlc_bsscfg_t *bsscfg = NULL;

	wlc_pktfetch_get_scb(wlc, &ctx->f, &bsscfg, &scb, promisc);

	/* Unable to acquire SCB: Clean up */
	if (scb == NULL || (ampdu_path && !SCB_AMPDU(scb))) {
		WL_ERROR(("%s: Unable to acquire scb!\n", __FUNCTION__));
		PKTFREE(osh, orig_lfrag, FALSE);
		MFREE(osh, ctx->pinfo, sizeof(struct pktfetch_info));
		MFREE(osh, ctx, sizeof(wlc_eapol_pktfetch_ctx_t));
		PKTFREE(osh, lbuf, FALSE);
		return;
	}

	/* Replicate frameinfo buffer */
	bcopy(&ctx->f, &f, sizeof(wlc_frminfo_t));
	wlc_recreate_frameinfo(wlc, lbuf, orig_lfrag, &ctx->f, &f);

	/* Copy PKTTAG */
	bcopy(WLPKTTAG(orig_lfrag), WLPKTTAG(lbuf), sizeof(wlc_pkttag_t));

	/* Cleanup first before dispatch */
	PKTFREE(osh, orig_lfrag, FALSE);
	MFREE(osh, ctx->pinfo, sizeof(struct pktfetch_info));
	MFREE(osh, ctx, sizeof(wlc_eapol_pktfetch_ctx_t));
	if (ampdu_path) {
		if (ordered)
			wlc_recvdata_ordered(wlc, scb, &f);
		else
			wlc_ampdu_recvdata(wlc->ampdu_rx, scb, &f);
	} else {
		wlc_recvdata_ordered(wlc, scb, &f);
	}
}

/* Recreates the frameinfo buffer based on offsets of the pulled packet */
static void
wlc_recreate_frameinfo(wlc_info_t *wlc, void *lbuf, void *lfrag,
	wlc_frminfo_t *fold, wlc_frminfo_t *fnew)
{
	osl_t *osh = wlc->osh;
	int16 offset_from_start;
	int16 offset = 0;

	/* During recvd frame processing, Pkt Data pointer was offset by some bytes
	 * Need to restore it to start
	 */
	offset_from_start = PKTDATA(osh, lfrag) - (uchar*)fold->wrxh;

	/* Push up data pointer to start of pkt: RxStatus start (wlc_d11rxhdr_t) */
	PKTPUSH(osh, lfrag, offset_from_start);

	/* lfrag data pointer is now at start of pkt.
	 * Push up lbuf data pointer by PKTLEN of lfrag
	 */
	PKTPUSH(osh, lbuf, PKTLEN(osh, lfrag));

	/* Copy the lfrag data starting from RxStatus (wlc_d11rxhdr_t) */
	bcopy(PKTDATA(osh, lfrag), PKTDATA(osh, lbuf), PKTLEN(osh, lfrag));

	/* wrxh and rxh pointers: both have same address */
	fnew->wrxh = (wlc_d11rxhdr_t *)PKTDATA(osh, lbuf);
	fnew->rxh = (d11rxhdr_t *)PKTDATA(osh, lbuf);

	/* Restore data pointer of lfrag and lbuf */
	PKTPULL(osh, lfrag, offset_from_start);
	PKTPULL(osh, lbuf, offset_from_start);

	/* Calculate offset of dot11_header from original lfrag
	 * and apply the same to lbuf frameinfo
	 */
	offset = ((uchar*)fold->h) - PKTDATA(osh, lfrag);
	fnew->h = (struct dot11_header *)(PKTDATA(osh, lbuf) + offset);

	/* Calculate the offset of Packet body pointer
	 * from original lfrag and apply to lbuf frameinfo
	 */
	offset = ((uchar*)fold->pbody) - PKTDATA(osh, lfrag);
	fnew->pbody = (uchar*)(PKTDATA(osh, lbuf) + offset);

	fnew->p = lbuf;
}

#if defined(PKTC) || defined(PKTC_DONGLE)
void
wlc_sendup_schedule_pktfetch(wlc_info_t *wlc, void *pkt)
{
	struct pktfetch_info *pinfo = NULL;
	struct pktfetch_generic_ctx *pctx = NULL;
	int ctx_count = 2;	/* No. of ctx variables needed to be saved */

	pinfo = MALLOCZ(wlc->osh, sizeof(struct pktfetch_info));
	if (!pinfo) {
		WL_ERROR(("%s: Out of mem: Unable to alloc pktfetch ctx!\n", __FUNCTION__));
		goto error;
	}

	pctx = MALLOCZ(wlc->osh, sizeof(struct pktfetch_generic_ctx) +
		ctx_count*sizeof(void*));
	if (!pctx) {
		WL_ERROR(("%s: Out of mem: Unable to alloc pktfetch ctx!\n", __FUNCTION__));
		goto error;
	}

	/* Fill up context */
	pctx->ctx_count = ctx_count;
	pctx->ctx[0] = (void *)wlc;
	pctx->ctx[1] = (void *)pinfo;

	/* Fill up pktfetch info */
	pinfo->osh = wlc->osh;
	pinfo->host_offset = 0;
	pinfo->headroom = PKTRXFRAGSZ;
	pinfo->lfrag = (void*)pkt;
	pinfo->cb = wlc_sendup_pktfetch_cb;
	pinfo->ctx = (void *)pctx;
	pinfo->next = NULL;
	if (hnd_pktfetch(pinfo) != BCME_OK) {
		WL_ERROR(("%s: pktfetch request rejected\n", __FUNCTION__));
		goto error;
	}

	return;

error:

	if (pinfo)
		MFREE(wlc->osh, pinfo, sizeof(struct pktfetch_info));

	if (pctx)
		MFREE(wlc->osh, pctx, sizeof(struct pktfetch_generic_ctx));

	if (pkt)
		PKTFREE(wlc->osh, pkt, FALSE);
}

static void
wlc_sendup_pktfetch_cb(void *lbuf, void *orig_lfrag, void *ctxt, bool cancelled)
{
	wlc_info_t *wlc;
	uint lcl_len;
	struct pktfetch_info *pinfo;
	struct pktfetch_generic_ctx *pctx = (struct pktfetch_generic_ctx *)ctxt;
	int ctx_count = pctx->ctx_count;

	/* Retrieve contexts */
	wlc = (wlc_info_t *)pctx->ctx[0];
	pinfo = (struct pktfetch_info *)pctx->ctx[1];

	lcl_len = PKTLEN(wlc->osh, orig_lfrag);
	PKTPUSH(wlc->osh, lbuf, lcl_len);
	bcopy(PKTDATA(wlc->osh, orig_lfrag), PKTDATA(wlc->osh, lbuf), lcl_len);

	/* Copy wl pkttag area */
	wlc_pkttag_info_move(wlc, orig_lfrag, lbuf);
	PKTSETIFINDEX(wlc->osh, lbuf, PKTIFINDEX(wlc->osh, orig_lfrag));
	PKTSETPRIO(lbuf, PKTPRIO(orig_lfrag));

	/* Free the original pktfetch_info and generic ctx  */
	MFREE(wlc->osh, pinfo, sizeof(struct pktfetch_info));
	MFREE(wlc->osh, pctx, sizeof(struct pktfetch_generic_ctx)
		+ ctx_count*sizeof(void *));

	PKTFREE(wlc->osh, orig_lfrag, TRUE);
	wlc_sendup_chain(wlc, lbuf);
}
#endif /* PKTC || PKTC_DONGLE */

#endif /* BCMSPLITRX */
