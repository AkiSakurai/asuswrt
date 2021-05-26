/*
 * Routines related to the uses of Pktfetch in WLC
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
 * $Id: wlc_pktfetch.c 787082 2020-05-18 09:21:51Z $
 */

/**
 * There are use cases which require the complete packet to be consumed in-dongle, and thus
 * neccessitates a mechanism (PktFetch) that can DMA down data blocks from the Host memory to dongle
 * memory, and thus recreate the full ethernet packet. Example use cases include:
 *     EAPOL frames
 *     SW TKIP
 *     WAPI
 *
 * This is a generic module which takes a Host address, block size and a pre-specified local buffer
 * (lbuf) and guarantees that the Host block will be fetched into device memory.
 */

#include <wlc_cfg.h>
#include <wlc_types.h>
#include <bcmwpa.h>
#include <hnd_cplt.h>
#include <phy_utils_api.h>
#include <wlc.h>
#include <wlc_pub.h>
#include <wlc_pktfetch.h>
#include <wlc_scb.h>
#include <wlc_ampdu_rx.h>
#include <wlc_keymgmt.h>
#include <wlc_bsscfg.h>
#include <d11_cfg.h>
#include <wlc_bmac.h>
#if defined(STS_XFER)
#include <wlc_sts_xfer.h>
#endif /* STS_XFER */
#ifdef BCMSPLITRX
#include <rte_pktfetch.h>
#include <wlc_rx.h>

static void wlc_recvdata_pktfetch_cb(void *lbuf, void *orig_lfrag,
	void *ctxt, bool cancelled);
#if defined(PKTC) || defined(PKTC_DONGLE)
static void wlc_sendup_pktfetch_cb(void *lbuf, void *orig_lfrag,
	void *ctxt, bool cancelled);
#endif // endif

static void wlc_recreate_frameinfo(wlc_info_t *wlc, void *lbuf, void *lfrag,
	wlc_frminfo_t *fold, wlc_frminfo_t *fnew);

static bool
wlc_pktfetch_checkpkt(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	struct wlc_frminfo *f, struct dot11_llc_snap_header *lsh, uint body_len);

static bool
wlc_pktfetch_required_mode4(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint16 *ether_type_offset,
		struct wlc_frminfo *f);
#ifdef BCMPCIEDEV
static void wl_tx_pktfetch_cb(void *lbuf, void *orig_lfrag, void *ctx, bool cancelled);
#endif /* BCMPCIEDEV */
bool
wlc_pktfetch_required(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	struct wlc_frminfo *f,	wlc_key_info_t *key_info, bool skip_iv)
{
	struct dot11_llc_snap_header *lsh;
	uint16* ether_type = NULL;
	uchar* f_body = f->pbody;

	/* All remaining checks are valid only if its a RX frag with host content */
	if (PKTFRAGUSEDLEN(wlc->osh, f->p) == 0)
		return FALSE;

#if defined(BCMHWA) && defined(HWA_RXDATA_BUILD)
	return hwa_rxdata_fhr_is_pktfetch(ltoh16(D11RXHDR_GE129_ACCESS_VAL(f->rxh, filtermap16)));
#endif /* BCMHWA && HWA_RXDATA_BUILD */

	/* In  MODE-4  need to check for  HdrType of conversion( Type1/Type2) to fetch correct
	 *    EtherType
	 */
	if (SPLITRX_DYN_MODE4(wlc->osh, f->p)) {
		bool conv_type;

		if (f->isamsdu) {
			f_body -= HDRCONV_PAD;
		}

		conv_type = RXHDR_GET_CONV_TYPE(&f->wrxh->rxhdr, wlc);
		if (conv_type) {
			ether_type = (unsigned short*)(f_body + ETHER_TYPE_2_OFFSET);
		} else {
			ether_type = (unsigned short*)(f_body + ETHER_TYPE_1_OFFSET);
		}
		return wlc_pktfetch_required_mode4(wlc, bsscfg, ether_type, f);
	}

	/* If IV needs to be accounted for, move past IV len */
	lsh = (struct dot11_llc_snap_header *)(f_body + (skip_iv ? key_info->iv_len : 0));

	/* For AMSDU packets packet starts after subframe header */
	if (f->isamsdu)
		lsh = (struct dot11_llc_snap_header*)((char *)lsh + ETHER_HDR_LEN);

	return (wlc_pktfetch_checkpkt(wlc, bsscfg, f, lsh, f->body_len));
}

static bool
wlc_pktfetch_required_mode4(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint16 *ether_type_offset,
		struct wlc_frminfo *f)
{
	uint16 ether_type = ntoh16(*ether_type_offset);

	BCM_REFERENCE(f);

	if (EAPOL_PKTFETCH_REQUIRED(ether_type) ||
#ifdef WLTDLS
			WLTDLS_PKTFETCH_REQUIRED(wlc, ether_type) ||
#endif // endif
#ifdef WLNDOE
			(NDOE_ENAB(wlc->pub) && (ether_type == ETHER_TYPE_IPV6) &&
			NDOE_PKTFETCH_REQUIRED_MODE4(wlc,
			((uint8 *)ether_type_offset + ETHER_TYPE_LEN),
			f->pbody, f->body_len)) ||
#endif // endif
#ifdef BCMWAPI_WAI
			WAPI_PKTFETCH_REQUIRED(ether_type) ||
#endif // endif
#ifdef WOWLPF
			WOWLPF_ACTIVE(wlc->pub) ||
#endif // endif
#ifdef WL_TBOW
			WLTBOW_PKTFETCH_REQUIRED(wlc, bsscfg, ether_type) ||
#endif // endif
#ifdef ICMP
			ICMP_PKTFETCH_REQUIRED(wlc, ether_type) ||
#endif // endif
			FALSE) {
				return TRUE;
			}
	return FALSE;
}

int
wlc_recvdata_schedule_pktfetch(wlc_info_t *wlc, struct scb *scb,
	wlc_frminfo_t *f, bool promisc_frame, bool ordered, bool amsdu_msdus)
{
	wlc_eapol_pktfetch_ctx_t *ctx = NULL;
	struct pktfetch_info *pinfo = NULL;
	d11rxhdr_t *rxh;
	uint32 copycount = (wlc->pub->tunables->copycount << 2);
	uint headroom;
	int ret = BCME_OK;
	uint8 pad;

	pinfo = MALLOCZ(wlc->osh, sizeof(struct pktfetch_info) + sizeof(wlc_eapol_pktfetch_ctx_t));
	if (!pinfo) {
		WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__,
			(int)sizeof(struct pktfetch_info), MALLOCED(wlc->osh)));
		ret = BCME_NOMEM;
		goto error;
	}

	pinfo->ctx_count = (sizeof(wlc_eapol_pktfetch_ctx_t) + sizeof(void*) - 1) / sizeof(void*);
	ctx = (wlc_eapol_pktfetch_ctx_t *) (pinfo + 1);
	ctx->wlc = wlc;

	/* are we using the existing packets wrxh or borrowed rxh */
	/* if f->wrxh is outside the headeroom, copy it in to current packet */
	if ((uchar *)f->wrxh <= (uchar *)PKTDATA(wlc->osh, f->p))
	{
		uint32 diff = 0;

		diff = (uchar *)PKTDATA(wlc->osh, f->p) - (uchar *)f->wrxh;
		WL_INFORM(("header adjust: diff: %d, data: %p, wrxh: %p, pktheadroom %d\n",
			diff, PKTDATA(wlc->osh, f->p), f->wrxh, PKTHEADROOM(wlc->osh, f->p)));
		if (diff <= PKTHEADROOM(wlc->osh, f->p)) {
			WL_INFORM(("rxheader in the packet\n"));
			goto wrxh_inline;
		}
	}
	/* wlc->datafiforxoff is the receive offset set in DMA engine (dma_attach_ext) */
	/* Check if mode4 is not enabled */
	if (PKTHEADROOM(wlc->osh, f->p) < wlc->datafiforxoff) {
		WL_ERROR(("no headroom to push the header\n"));
		OSL_SYS_HALT();
		ret = BCME_BUFTOOSHORT;
		goto error;
	}

	rxh = (d11rxhdr_t *)((uchar *)f->p + (LBUFFRAGSZ + WLC_RXHDR_LEN));
	pad = RXHDR_GET_PAD_LEN(rxh, wlc);
	headroom = PKTHEADROOM(wlc->osh, f->p);

	/* Copy rx header */
	PKTPUSH(wlc->osh, f->p, headroom);
	memcpy(PKTDATA(wlc->osh, f->p), (uchar *)f->wrxh, WL_RXHDR_LEN(wlc->pub->corerev));
	WL_INFORM(("f->wrsxh switching from %p to new %p\n", f->wrxh, PKTDATA(wlc->osh, f->p)));
	f->wrxh = (wlc_d11rxhdr_t *) PKTDATA(wlc->osh, f->p);
	f->rxh = &f->wrxh->rxhdr;
	PKTPULL(wlc->osh, f->p, headroom);

	if (pad) {
		wlc_rxhdr_set_pad_present(f->rxh, wlc);
	} else {
		wlc_rxhdr_clear_pad_present(f->rxh, wlc);
	}

wrxh_inline:
	memcpy(&ctx->f, f, sizeof(wlc_frminfo_t));
#ifdef WLAMPDU
	if ((f->subtype == FC_SUBTYPE_QOS_DATA) &&
		SCB_AMPDU(scb) && wlc_scb_ampdurx_on_tid(scb, f->prio) &&
		!f->ismulti && !promisc_frame) {
		ctx->ampdu_path = TRUE;
	}
#endif // endif
	/* Block processing of Classify FIFO (RX-FIFO2) when packet fetch is scheduled.
	 * This is done to ensure we don`t do any out of order procesing of management
	 * frames when packet fetch is not yet completed.
	 * One such example is processing of RX-DEAUTH frame from P2P-GO when EAP-FAILURE
	 * is being fetched from host
	 */
	wlc_bmac_classify_fifo_suspend(wlc->hw);

	ctx->ordered = ordered;
	ctx->promisc = promisc_frame;
	ctx->pinfo = pinfo;
	ctx->scb_assoctime = scb->assoctime;
	memcpy(&ctx->ea, &scb->ea,  sizeof(struct ether_addr));

	/* Headroom does not need to be > PKTRXFRAGSZ */
	pinfo->osh = wlc->osh;
	pinfo->headroom = PKTRXFRAGSZ;

	/* In case of RXMODE 3, host has full pkt including RxStatus (WL_HWRXOFF_AC bytes)
	 * Host buffer addresses in the lfrag points to start of this full pkt
	 * So, pktfetch host_offset needs to be set to (copycount + HW_RXOFF)
	 * so that only necessary (remaining) data if pktfetched from Host.
	*/
	if (SPLITRX_DYN_MODE3(wlc->osh, f->p)) {
		pinfo->host_offset = copycount + wlc->d11rxoff;
		PKTSETFRAGTOTLEN(wlc->osh, f->p,
			PKTFRAGTOTLEN(wlc->osh, f->p) + copycount + wlc->d11rxoff);
	}
	/* In RXMODE4 host has full 802.3 pkt starting from WL_HWRXOFF_AC.
	*  So to recreate orginal pkt dongle has to get the full payload excluding
	* WL_HWRXOFF_AC + Erhernet Header, as that part will be present in lfrag
	*/
	else if (SPLITRX_DYN_MODE4(wlc->osh, f->p)) {
		bool conv_type;

		conv_type = RXHDR_GET_CONV_TYPE(f->rxh, wlc);
		if (conv_type) {
			/* This is a type-2 conversion so hostoffset should be
			*  rxoffset + Ethernet Hdr + 2 bytes pad.
			*  This 2 bytes pad is absorbed into DOT3HDR_OFFSET[14 + 2].
			*/
			/* Its already converted header to 4 byte status */
			pinfo->host_offset = (wlc->datafiforxoff + DOT3HDR_OFFSET);
			PKTSETFRAGTOTLEN(wlc->osh, f->p, PKTFRAGTOTLEN(wlc->osh, f->p) +
					wlc->datafiforxoff + DOT3HDR_OFFSET);
		}
		else {
			/* This is Type-1 conversion so hostoffset should be
			* rxoffset + Ethernet Hdr + LLC hdr
			*/
			pinfo->host_offset = (wlc->datafiforxoff + DOT3HDR_OFFSET + LLC_HDR_LEN);
			PKTSETFRAGTOTLEN(wlc->osh, f->p, PKTFRAGTOTLEN(wlc->osh, f->p) +
					wlc->datafiforxoff + DOT3HDR_OFFSET + LLC_HDR_LEN);
		}
	} else {
		pinfo->host_offset = 0;
	}

	pinfo->lfrag = f->p;
	pinfo->cb = wlc_recvdata_pktfetch_cb;
	ctx->flags = 0;
	if (amsdu_msdus)
		ctx->flags = PKTFETCH_FLAG_AMSDU_SUBMSDUS;

#ifdef BCMPCIEDEV
	if (BCMPCIEDEV_ENAB()) {
		ret = hnd_pktfetch(pinfo);
		if (ret != BCME_OK) {
			wlc_bmac_classify_fifo_resume(wlc->hw, TRUE);
			WL_ERROR(("%s: pktfetch request rejected\n", __FUNCTION__));
			goto error;
		}
	}
#endif /* BCMPCIEDEV */
	return ret;

error:
	if (pinfo)
		MFREE(wlc->osh, pinfo, sizeof(struct pktfetch_info) +
			sizeof(wlc_eapol_pktfetch_ctx_t));

	if (f->p)
		PKTFREE(wlc->osh, f->p, FALSE);

	return ret;
}

static void
wlc_recvdata_pktfetch_cb(void *lbuf, void *orig_lfrag, void *ctxt, bool cancelled)
{
	struct pktfetch_info *pinfo = ctxt;
	wlc_eapol_pktfetch_ctx_t *ctx = (wlc_eapol_pktfetch_ctx_t *) (pinfo + 1);
	wlc_info_t *wlc = ctx->wlc;
	struct scb *scb = NULL;
	osl_t *osh = wlc->osh;
	wlc_frminfo_t f;
#ifdef WLAMPDU
	bool ampdu_path = ctx->ampdu_path;
	bool ordered = ctx->ordered;
#endif // endif
	bool promisc = ctx->promisc;
	wlc_bsscfg_t *bsscfg = NULL;

	ASSERT(pinfo->ctx_count ==
		(sizeof(wlc_eapol_pktfetch_ctx_t) + sizeof(void*) - 1) / sizeof(void*));

	if (cancelled) {
		/* lbuf could be NULL */
		if (lbuf) {
			PKTFREE(osh, lbuf, FALSE);
		}

		PKTFREE(osh, orig_lfrag, FALSE);
		WL_ERROR(("%s: Cancel Rx pktfetch for lfrag@%p type [%s]\n",
			__FUNCTION__, orig_lfrag, (lbuf) ? "IN_FETCH" : "IN_PKTFETCH"));
		goto done;
	}

	/* Replicate frameinfo buffer */
	memcpy(&f, &ctx->f, sizeof(wlc_frminfo_t));
	if (!(ctx->flags & PKTFETCH_FLAG_AMSDU_SUBMSDUS))
		wlc_recreate_frameinfo(wlc, lbuf, orig_lfrag, &ctx->f, &f);
	else {
		int err = 0;
		bsscfg = wlc_bsscfg_find(wlc, WLPKTTAGBSSCFGGET(orig_lfrag), &err);
		if (bsscfg)
			scb = wlc_scbfind(wlc, bsscfg, &ctx->ea);
		/* Subsequent subframe fetches */
		PKTPUSH(osh, lbuf, PKTLEN(osh, orig_lfrag));
		/* Copy the original lfrag data  */
		memcpy(PKTDATA(osh, lbuf), PKTDATA(osh, orig_lfrag), PKTLEN(osh, orig_lfrag));
		f.p = lbuf;
		/* Set length of the packet */
		PKTSETLEN(wlc->osh, f.p, PKTLEN(osh, orig_lfrag) +
			PKTFRAGTOTLEN(wlc->osh, orig_lfrag));
	}

	/* Copy PKTTAG */
	memcpy(WLPKTTAG(lbuf), WLPKTTAG(orig_lfrag), sizeof(wlc_pkttag_t));
	/* reset pkt next info old one needs to un chain, new one needs to be chained */
	PKTSETNEXT(osh, lbuf, PKTNEXT(osh, orig_lfrag));
	PKTSETNEXT(osh, orig_lfrag, NULL);

#if defined(STS_XFER)
	ASSERT(WLPKTTAG(orig_lfrag)->phyrxs_seqid == STS_XFER_PHYRXS_SEQID_INVALID);
#else /* ! STS_XFER */
	WLPKTTAG(orig_lfrag)->phystsbuf_idx = 0;
#endif /* ! STS_XFER */

	/* Cleanup first before dispatch */
	PKTFREE(osh, orig_lfrag, FALSE);

	/* Extract scb info */
	if (!scb && !(ctx->flags & PKTFETCH_FLAG_AMSDU_SUBMSDUS))
		wlc_pktfetch_get_scb(wlc, &f, &bsscfg, &scb, promisc, ctx->scb_assoctime);
	 if (scb == NULL) {
		/* Unable to acquire SCB: Clean up */
		WL_INFORM(("%s: Unable to acquire scb!\n", __FUNCTION__));
		PKTFREE(osh, lbuf, FALSE);
	} else {
		/*
		 * It is possible that the chip might have gone down into low power state
		* after scheduling pktfetch, so now make sure specific power islands are powered up
		*/
		if (SRPWR_ENAB()) {
			wlc_srpwr_request_on(wlc);
		}
		if ((ctx->flags & PKTFETCH_FLAG_AMSDU_SUBMSDUS)) {
			/* non first MSDU/suframe and if its 802.1x frame then fetch come here */
			if (wlc_process_eapol_frame(wlc, scb->bsscfg, scb, &f, f.p)) {
				WL_INFORM(("Processed fetched msdu %p\n", f.p));
				/* We have consumed the pkt drop and continue; */
				PKTFREE(osh, f.p, FALSE);
			} else {
				f.da = (struct ether_addr *)PKTDATA(wlc->osh, f.p);
				f.wds = FALSE;

				/* Call sendup as frames are in Ethernet format */
				wlc_recvdata_sendup_msdus(wlc, scb, &f);
			}
#ifdef WLAMPDU
		} else if (ampdu_path) {
			if (ordered)
				wlc_recvdata_ordered(wlc, scb, &f);
			else
				wlc_ampdu_recvdata(wlc->ampdu_rx, scb, &f);
#endif // endif
		} else {
			wlc_recvdata_ordered(wlc, scb, &f);
		}
	}

done:

	/* Resume processing of Classify FIFO (FIFO2, Management frames) once packet fetching is
	 * completed
	 */
	wlc_bmac_classify_fifo_resume(wlc->hw, FALSE);

	MFREE(osh, pinfo, sizeof(struct pktfetch_info) + sizeof(wlc_eapol_pktfetch_ctx_t));
}

/* Recreates the frameinfo buffer based on offsets of the pulled packet */
static void
wlc_recreate_frameinfo(wlc_info_t *wlc, void *lbuf, void *lfrag,
	wlc_frminfo_t *fold, wlc_frminfo_t *fnew)
{
	osl_t *osh = wlc->osh;
	int16 offset_from_start;
	int16 offset = 0;
	uint8 pad = 0;
	uint8 dot11_offset = 0;
	unsigned char llc_hdr[8] = {0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00, 0x88, 0x8e};
	uint16* ether_type = NULL;

	/* During recvd frame processing, Pkt Data pointer was offset by some bytes
	 * Need to restore it to start
	 */
	offset_from_start = PKTDATA(osh, lfrag) - (uchar*)fold->wrxh;
	PKTPUSH(osh, lfrag, offset_from_start);

	/* lfrag data pointer is now at start of pkt.
	 * Push up lbuf data pointer by PKTLEN of lfrag
	 */
	if (SPLITRX_DYN_MODE4(osh, lfrag)) {
		uchar* fold_pbody = fold->pbody;
		bool conv_type;
		uint16 sflen = 0;
		uint16 insert_len = 0;
		uint32 copycount = (wlc->pub->tunables->copycount << 2);

		if (fold->isamsdu) {
			fold_pbody -= HDRCONV_PAD;
		}

		conv_type = RXHDR_GET_CONV_TYPE(&fold->wrxh->rxhdr, wlc);
		if (conv_type) {
			ether_type = (unsigned short*)(fold_pbody + ETHER_TYPE_2_OFFSET);
		} else {
			ether_type = (unsigned short*)(fold_pbody + ETHER_TYPE_1_OFFSET);
			/* Copy the LLC header for type 1 */
			memcpy(llc_hdr, ((uchar *)ether_type - LLC_START_OFFSET_LEN),
					LLC_START_OFFSET_LEN);
		}
		llc_hdr[6] = (uint8)*ether_type;
		llc_hdr[7] = (uint8)((*ether_type) >> 8);

		/* 802.11 hdr offset */
		dot11_offset = ((uchar*)fold->pbody) - (PKTDATA(osh, lfrag) + offset_from_start);

		insert_len = dot11_offset + LLC_HDR_LEN + offset_from_start;

		if (WLPKTTAG(lfrag)->flags & WLF_HWAMSDU) {
			/* Consider sub frame header and pad */
			insert_len += ETHER_HDR_LEN + HDRCONV_PAD;

			/* Calculate sub frame length */
			sflen = hton16(PKTLEN(osh, lbuf) + LLC_HDR_LEN);

			/* Adjust total length */
			fnew->totlen -= (copycount - (HDRCONV_PAD + ETHER_HDR_LEN + LLC_HDR_LEN));
		}

		PKTPUSH(osh, lbuf, insert_len);

		/* Copy the lfrag data starting from RxStatus (wlc_d11rxhdr_t) */
		offset = 0;
		memcpy(PKTDATA(osh, lbuf), PKTDATA(osh, lfrag), offset_from_start);
		offset += offset_from_start;

		/* Copy pad */
		if (WLPKTTAG(lfrag)->flags & WLF_HWAMSDU) {
			pad = HDRCONV_PAD;
			offset += HDRCONV_PAD;
			wlc_rxhdr_set_pad_present(fnew->rxh, wlc);
		}

		/* Copy 802.11 hdr */
		memcpy((PKTDATA(osh, lbuf) + offset), (PKTDATA(osh, lfrag) + offset),
			dot11_offset);
		offset += dot11_offset;

		/* Copy sub frame header */
		if (WLPKTTAG(lfrag)->flags & WLF_HWAMSDU) {
			memcpy((PKTDATA(osh, lbuf) + offset), fold->pbody, ETHER_TYPE_OFFSET);
			offset += ETHER_TYPE_OFFSET;
			memcpy((PKTDATA(osh, lbuf) + offset), &sflen, ETHER_TYPE_LEN);
			offset += ETHER_TYPE_LEN;
		}

		/* Copy llc header */
		memcpy((PKTDATA(osh, lbuf) + offset), llc_hdr, LLC_HDR_LEN);
	} else {
		PKTPUSH(osh, lbuf, PKTLEN(osh, lfrag));
		/* Copy the lfrag data starting from RxStatus (wlc_d11rxhdr_t) */
		memcpy(PKTDATA(osh, lbuf), PKTDATA(osh, lfrag), PKTLEN(osh, lfrag));
	}

	/* wrxh and rxh pointers: both have same address */
	fnew->wrxh = (wlc_d11rxhdr_t *)PKTDATA(osh, lbuf);
	fnew->rxh = &fnew->wrxh->rxhdr;

	/* Restore data pointer of lfrag and lbuf */
	PKTPULL(osh, lfrag, offset_from_start);
	PKTPULL(osh, lbuf, offset_from_start);

	/* Calculate offset of dot11_header from original lfrag
	 * and apply the same to lbuf frameinfo
	 */
	offset = (((uchar*)fold->h) - PKTDATA(osh, lfrag)) + pad;
	fnew->h = (struct dot11_header *)(PKTDATA(osh, lbuf) + offset);

	/* Calculate the offset of Packet body pointer
	 * from original lfrag and apply to lbuf frameinfo
	 */
	offset = (((uchar*)fold->pbody) - PKTDATA(osh, lfrag)) + pad;
	fnew->pbody = (uchar*)(PKTDATA(osh, lbuf) + offset);

	fnew->p = lbuf;
}

#if defined(PKTC) || defined(PKTC_DONGLE)
bool
wlc_sendup_chain_pktfetch_required(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, void *p,
		uint16 body_offset)
{
	uint body_len;
	struct dot11_llc_snap_header *lsh;

	/* For AMSDU pkts, packet starts after subframe header */
	if (WLPKTTAG(p)->flags & WLF_HWAMSDU)
		body_offset += ETHER_HDR_LEN;

	lsh = (struct dot11_llc_snap_header *) (PKTDATA(wlc->osh, p) + body_offset);
	body_len = PKTLEN(wlc->osh, p) - body_offset;
	BCM_REFERENCE(body_len);

	if (PKTFRAGUSEDLEN(wlc->osh, p) > 0) {
		return (wlc_pktfetch_checkpkt(wlc, bsscfg, p, lsh, body_len));
	}
	return FALSE;
}

void
wlc_sendup_schedule_pktfetch(wlc_info_t *wlc, void *pkt, uint32 body_offset)
{
	struct pktfetch_info *pinfo = NULL;
	int ctx_count = 2;	/* No. of ctx variables needed to be saved */
	uint32 copycount = (wlc->pub->tunables->copycount << 2);

	pinfo = MALLOCZ(wlc->osh, (sizeof(struct pktfetch_info) + ctx_count*sizeof(void*)));
	if (!pinfo) {
		WL_ERROR(("%s: Out of mem: Unable to alloc pktfetch ctx!\n", __FUNCTION__));
		goto error;
	}

	/* Fill up context */
	pinfo->ctx_count = ctx_count;
	pinfo->ctx[0] = (void *)wlc;
	pinfo->ctx[1] = (void *)body_offset;
	/* Fill up pktfetch info */
	/* In case of RXMODE 3, host has full pkt including RxStatus (WL_HWRXOFF_AC bytes)
	 * Host buffer addresses in the lfrag points to start of this full pkt
	 * So, pktfetch host_offset needs to be set to (copycount + HW_RXOFF)
	 * so that only necessary (remaining) data if pktfetched from Host.
	*/
	if (SPLITRX_DYN_MODE3(wlc->osh, pkt)) {
		pinfo->host_offset = copycount + wlc->d11rxoff;
		PKTSETFRAGTOTLEN(wlc->osh, pkt,
			PKTFRAGTOTLEN(wlc->osh, pkt) + copycount + wlc->d11rxoff);
	}
	else
		pinfo->host_offset = 0;

	pinfo->osh = wlc->osh;
	pinfo->headroom = PKTRXFRAGSZ;

	/* if key processing done, make headroom to save body_offset */
	if (WLPKTTAG(pkt)->flags & WLF_RX_KM)
		pinfo->headroom += PKTBODYOFFSZ;
	pinfo->lfrag = (void*)pkt;
	pinfo->cb = wlc_sendup_pktfetch_cb;
	pinfo->next = NULL;
	if (hnd_pktfetch(pinfo) != BCME_OK) {
		WL_ERROR(("%s: pktfetch request rejected\n", __FUNCTION__));
		goto error;
	}

	return;

error:

	if (pinfo)
		MFREE(wlc->osh, pinfo, sizeof(struct pktfetch_info) + ctx_count*sizeof(void*));

	if (pkt)
		PKTFREE(wlc->osh, pkt, FALSE);
}

static void
wlc_sendup_pktfetch_cb(void *lbuf, void *orig_lfrag, void *ctxt, bool cancelled)
{
	wlc_info_t *wlc;
	uint lcl_len;
	struct pktfetch_info *pinfo = (struct pktfetch_info *) ctxt;
	uint32 body_offset;

	ASSERT(pinfo->ctx_count == 2);
	/* Retrieve contexts */
	wlc = (wlc_info_t *)pinfo->ctx[0];
	body_offset = (uint32)pinfo->ctx[1];

	lcl_len = PKTLEN(wlc->osh, orig_lfrag);
	PKTPUSH(wlc->osh, lbuf, lcl_len);
	memcpy(PKTDATA(wlc->osh, lbuf), PKTDATA(wlc->osh, orig_lfrag), lcl_len);

	/* append body_offset if key management has been processed */
	if (WLPKTTAG(orig_lfrag)->flags & WLF_RX_KM) {
		PKTPUSH(wlc->osh, lbuf, PKTBODYOFFSZ);
		*((uint32 *)PKTDATA(wlc->osh, lbuf)) = body_offset;
	}
	/* Copy wl pkttag area */
	wlc_pkttag_info_move(wlc, orig_lfrag, lbuf);
	PKTSETIFINDEX(wlc->osh, lbuf, PKTIFINDEX(wlc->osh, orig_lfrag));
	PKTSETPRIO(lbuf, PKTPRIO(orig_lfrag));

	PKTSETNEXT(wlc->osh, lbuf, PKTNEXT(wlc->osh, orig_lfrag));
	PKTSETNEXT(wlc->osh, orig_lfrag, NULL);

	/* Free the original pktfetch_info and generic ctx  */
	MFREE(wlc->osh, pinfo, sizeof(struct pktfetch_info)+ pinfo->ctx_count*sizeof(void *));

#if defined(STS_XFER)
	ASSERT(WLPKTTAG(orig_lfrag)->phyrxs_seqid == STS_XFER_PHYRXS_SEQID_INVALID);
#else /* ! STS_XFER */
	WLPKTTAG(orig_lfrag)->phystsbuf_idx = 0;
#endif /* ! STS_XFER */

	PKTFREE(wlc->osh, orig_lfrag, TRUE);

	/* Mark this lbuf has pktfetched lbuf pkt */
	PKTSETPKTFETCHED(wlc->osh, lbuf);

	/*
	 * It is possible that the chip might have gone down into some low power state
	 * while this is happening, so now make sure specific power islands are powered up
	*/
	if (SRPWR_ENAB()) {
		wlc_srpwr_request_on(wlc);
	}

	wlc_sendup_chain(wlc, lbuf);
}
#endif /* PKTC || PKTC_DONGLE */

static bool
wlc_pktfetch_checkpkt(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	struct wlc_frminfo *f, struct dot11_llc_snap_header *lsh, uint body_len)
{
	bool retval;
	uint16 ether_type = ntoh16(lsh->type);

	retval = LLC_SNAP_HEADER_CHECK(lsh) &&
		(EAPOL_PKTFETCH_REQUIRED(ether_type) ||
#ifdef WLTDLS
			WLTDLS_PKTFETCH_REQUIRED(wlc, ether_type) ||
#endif // endif
#ifdef WLNDOE
			NDOE_PKTFETCH_REQUIRED(wlc, lsh, f->pbody, body_len) ||
#endif // endif
#ifdef BCMWAPI_WAI
			WAPI_PKTFETCH_REQUIRED(ether_type) ||
#endif // endif
#ifdef WOWLPF
			WOWLPF_ACTIVE(wlc->pub) ||
#endif // endif
#ifdef WL_TBOW
			WLTBOW_PKTFETCH_REQUIRED(wlc, bsscfg, ether_type) ||
#endif // endif
#ifdef ICMP
			ICMP_PKTFETCH_REQUIRED(wlc, ether_type) ||
#endif // endif
			FALSE);

	return retval;
}

#endif /* BCMSPLITRX */
#ifdef BCMPCIEDEV
/**
 * For e.g. 802.1x packets, it is necessary to transfer the full packet from host memory into CPU
 * RAM, so firmware can parse packet contents before transmission.
 */
void
wl_tx_pktfetch(wlc_info_t *wlc, struct lbuf *lb, void *src, void *dev,
	wlc_bsscfg_t *bsscfg)
{
	struct pktfetch_info *pinfo = NULL;
	int ctx_count = 5;	/* No. of ctx variables needed to be saved */
	/* intention is to store ID - bsscfg Instance ID - to track delete before fetch cb */
	uint32 bsscfg_id = bsscfg->ID;

	pinfo = MALLOCZ(wlc->osh, sizeof(struct pktfetch_info) + ctx_count*sizeof(void*));
	if (!pinfo) {
		WL_ERROR(("%s: Out of mem: Unable to alloc pktfetch ctx!\n", __FUNCTION__));
		goto error;
	}

	/* Fill up context */
	pinfo->ctx_count = ctx_count;
	pinfo->ctx[0] = (void *)wlc;
	pinfo->ctx[1] = (void *)src;
	pinfo->ctx[2] = (void *)dev;
	pinfo->ctx[3] = (void *)bsscfg;
	pinfo->ctx[4] = (void *)bsscfg_id;

	/* Fill up pktfetch info */
#ifdef BCM_DHDHDR
	pinfo->host_offset = (-DOT11_LLC_SNAP_HDR_LEN);
#else
	pinfo->host_offset = 0;
#endif // endif

	/* Need headroom of atleast 224 for TXOFF/amsdu headroom
	 * Rounded to 256
	 */
	pinfo->headroom = PKTFETCH_DEFAULT_HEADROOM;
	pinfo->lfrag = (void*)lb;
	pinfo->cb = wl_tx_pktfetch_cb;
	pinfo->next = NULL;
	pinfo->osh = wlc->osh;
	if (hnd_pktfetch(pinfo) != BCME_OK) {
		//WL_ERROR(("%s: pktfetch request rejected\n", __FUNCTION__));
		goto error;
	}

	return;

error:
	if (pinfo)
		MFREE(wlc->osh, pinfo, sizeof(struct pktfetch_info) + ctx_count*sizeof(void*));

	if (lb)
		PKTFREE(wlc->osh, lb, TRUE);

}

/** Packet fetch callback. BCMPCIEDEV specific */
static void
wl_tx_pktfetch_cb(void *lbuf, void *orig_lfrag, void *ctx, bool cancelled)
{
	wlc_info_t *wlc;
	struct pktfetch_info *pinfo = (struct pktfetch_info *)ctx;
	void *src, *dev;
	uint32 bsscfg_ID;
	wlc_bsscfg_t *bsscfg = NULL;

	ASSERT(pinfo->ctx_count == 5);
	/* Retrieve contexts */
	wlc = (wlc_info_t*)pinfo->ctx[0];
	src = (void *)pinfo->ctx[1];
	dev = (void *)pinfo->ctx[2];
	bsscfg = (wlc_bsscfg_t *)pinfo->ctx[3];
	bsscfg_ID = (uint32)pinfo->ctx[4];

#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
	if (PKTISTXFRAG(wlc->osh, orig_lfrag)) {
		if (lbuf && lbuf != orig_lfrag)
			ASSERT(0);
	} else
#endif // endif
	{
		PKTSETNEXT(wlc->osh, orig_lfrag, lbuf);
	}

	/* Handle the pktfetch cancelled case */
	if (cancelled) {
		/* lbuf could be NULL */
		PKTFREE(wlc->osh, orig_lfrag, TRUE);
		WL_ERROR(("%s: Cancel Tx pktfetch for lfrag@%p type [%s]\n",
			__FUNCTION__, orig_lfrag, (lbuf) ? "IN_FETCH" : "IN_PKTFETCH"));
		goto done;
	}

	PKTSETFRAGTOTLEN(wlc->osh, orig_lfrag, 0);
	PKTSETFRAGLEN(wlc->osh, orig_lfrag, LB_FRAG1, 0);

	/* When BCM_DHDHDR is enabled, all tx packets that need to be fetched will
	 * include llc snap 8B header at start of lbuf.
	 * So we can do PKTSETFRAGTOTNUM here as well.
	 */
	PKTSETFRAGTOTNUM(wlc->osh, orig_lfrag, 0);

	/* The hnd_pktfetch_dispatch may get lbuf from PKTALLOC and the pktalloced counter
	 * will be increased by 1, later in the wl_send the PKTFROMNATIVE will increase 1 again
	 * for !lb_pool lbuf. (dobule increment)
	 * Here do PKTTONATIVE to decrease it before wl_send.
	 */
	if (!PKTPOOL(wlc->osh, lbuf)) {
		PKTTONATIVE(wlc->osh, lbuf);
	}

	if (bsscfg != wlc_bsscfg_find_by_ID(wlc, bsscfg_ID)) {
		/* Drop the pkt, bsscfg not valid */
		WL_ERROR(("wl%d: Bsscfg %p Was freed during pktfetch, Drop the rqst \n",
			wlc->pub->unit, bsscfg));
		PKTFREE(wlc->osh, orig_lfrag, TRUE);
	} else {
		wl_send_cb(wlc->wl, src, dev, orig_lfrag);
	}

done:
	/* Free the original pktfetch_info and generic ctx  */
	MFREE(wlc->osh, pinfo, sizeof(struct pktfetch_info) + pinfo->ctx_count*sizeof(void *));
}
#endif /* BCMPCIEDEV */
/*
 * Cancel all pending fetch requests
 * Requests could be waiting @
 *
 * 1. packet fetch queue @ rte_pktfetch layer
 * 2. Fetch request queue @ rte_fetch layer
 * 3. Fetch completion queue at pciedev bus layer
 *
 * This routine deletes all pending requests in rte layer.
 * Then cancels all pending fetch requests dispatched to bus layer.
 */

void
wlc_pktfetch_queue_flush(wlc_info_t * wlc)
{
	hnd_cancel_pktfetch();
}
