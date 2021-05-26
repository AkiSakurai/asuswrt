/*
 * Proxd FTM method implementation - Vendor-Specific support. See twiki FineTimingMeasurement.
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
 * $Id: pdftmvs.c 790688 2020-08-31 20:02:59Z $
 */

#include "pdftmpvt.h"

/*
	Allocate a FTM vendor-specific action frame for XTLVs data
	Input:
		num_tlvs: specify num of XTLVs
		tlv_data_len: specify total data len of XTLVs, not including the XTLV hdr size
	Output:
		If succeeds, return a pkt-pointer(and body_len, body pointer),
		(Note, the buffer will not be initialized; caller can use
		ftm_vs_fill_action_frmhdr() to setup the AF header later.)
		If error, return NULL
*/
enum vs_type {
	VS_TYPE_REQ_START		= 1,
	VS_TYPE_MEASURE_END		= 2,
	VS_TYPE_MEASURE			= 3
};

/* Overhead for single fragment:
 * Vendor IE header + TLV header + MFBUF header
 */

static void*
ftm_vs_alloc_action_frame(pdsvc_t *pdsvc, wlc_bsscfg_t *bsscfg,
	const struct ether_addr *da, const struct ether_addr *sa,
	const struct ether_addr *bssid, int num_tlvs, uint total_tlvs_data_len,
	uint *out_body_len, uint8 **out_body)
{
	void *pkt;
	uint body_len;
	uint8 *body;

	body_len = OFFSETOF(dot11_action_vs_frmhdr_t, data) +
		BCM_XTLV_HDR_SIZE + total_tlvs_data_len;

	ASSERT(body_len >= DOT11_ACTION_VS_HDR_LEN);

	pkt = proxd_alloc_action_frame(pdsvc, bsscfg, da, sa, bssid,
		body_len, &body, DOT11_ACTION_CAT_VS, BRCM_FTM_VS_AF_TYPE);
	if (pkt == NULL) {
		*out_body_len = 0;
		return (void *) NULL; /* err = BCME_NOMEM; */
	}

	*out_body_len = body_len;	/* ret to caller */
	*out_body = body;

	return pkt;
}

/*
 * Fill in the FTM vendor-specific action frame header
 * Input:
 *	vs_type: specify a BRCM Vendor-Specific Action Frame type (e.g. RBCM_FTM_VS_AF_TYPE)
 *	vs_subtype: specify a subtype (e.g. BRCM_FTM_VS_INITIATOR_RPT_SUBTYPE)
*/
static int
ftm_vs_fill_action_frmhdr(uint8 *body, uint body_len, uint8 vs_type, uint8 vs_subtype)
{
	dot11_action_vs_frmhdr_t *vs_afhdr;

	if (body_len < DOT11_ACTION_VS_HDR_LEN)
		return BCME_BUFTOOSHORT;

	/* fill in vendor-specific action frame header */
	vs_afhdr = (dot11_action_vs_frmhdr_t *) body;
	bzero(vs_afhdr, DOT11_ACTION_VS_HDR_LEN);
	vs_afhdr->category = DOT11_ACTION_CAT_VS;
	memcpy(vs_afhdr->OUI, BRCM_PROP_OUI, DOT11_OUI_LEN);
	vs_afhdr->type = vs_type;
	vs_afhdr->subtype = vs_subtype;

	return BCME_OK;
}

/*
 * Fill in FTM Initiator-Report vendor-specific action frame body (XTLV hdr+data)
 * with the caller-provided rtt_result.
 */
static int
ftm_vs_fill_initiator_rpt(pdftm_t *ftm, pdftm_session_t *sn, wl_proxd_rtt_result_t *rtt_result,
	uint8 *body, uint body_len)
{
	int err;
	uint16 tlv_ids[] = { WL_PROXD_TLV_ID_RTT_RESULT_V2 };
	uint body_tlv_len = 0;
	pdftm_iov_tlv_digest_t *dig;
	dot11_action_vs_frmhdr_t *vs_afhdr = (dot11_action_vs_frmhdr_t *) body;

	dig = pdftm_iov_dig_init(ftm, sn);

	/* fill in vendor-specific data with rtt_result */
	/* use pdftm iov packer to pack rtt_result in XTLV format */
	dig->tlv_data.out.rtt_result = rtt_result;
	/* TODO: EXCAST to (int *) has to be removed once
	 * len changesa re made in pdftm_iov_pack
	 */
	err = pdftm_iov_pack(ftm,
		body_len - OFFSETOF(dot11_action_vs_frmhdr_t, data),
		(wl_proxd_tlv_t *) &vs_afhdr->data[0], tlv_ids, ARRAYSIZE(tlv_ids),
		dig, (int *)&body_tlv_len);

	return err;
}

/* external interface */
/*
	Transmit a FTM Initiator-Report to peer-target
*/
int
pdftm_vs_tx_initiator_rpt(pdftm_t *ftm, pdftm_session_t *sn)
{
	uint	tlv_data_len;
	void *pkt;
	uint body_len = 0;
	uint8 *body;
	int err = BCME_OK;
	wl_proxd_rtt_result_t	rtt_result;

	ASSERT(ftm != NULL);
	ASSERT(sn != NULL);

	FTM_LOGPROTO(ftm, (("wl%d.%d: %s: handling tx initiator-rpt to " MACF "\n",
		FTM_UNIT(ftm), WLC_BSSCFG_IDX(sn->bsscfg), __FUNCTION__,
		ETHERP_TO_MACF(&sn->config->burst_config->peer_mac))));

	/* collect the rtt-result from session manager, no detail */
	bzero(&rtt_result, sizeof(rtt_result));
	err = pdftm_get_session_result(ftm, sn, &rtt_result, 0);
	if (err != BCME_OK) {
		goto done;
	}
	/* keep 'initiator' addr in the rtt-result */
	memcpy(&rtt_result.peer, &sn->bsscfg->cur_etheraddr, sizeof(rtt_result.peer));

	/* figure out the tlv_data_size (not including the XTLV header) */
	/* need to export ftm_iov_xxx later */
	tlv_data_len = OFFSETOF(wl_proxd_rtt_result_t, rtt) +
		(rtt_result.num_rtt + 1) * sizeof(wl_proxd_rtt_sample_t);

	/*
	* allocate an initiator-report packet (vendor-specific action frame) for tx.
	* The body contains a vendor-specific-action-frame-header followed by
	* variable XTLVs, for an initiator-report, it contains only 1 xtlv with
	* WL_PROXD_TLV_ID_RTT_RESULT.
	*/
	pkt = ftm_vs_alloc_action_frame(ftm->pd, sn->bsscfg,
		&sn->config->burst_config->peer_mac,
		&sn->config->burst_config->cur_ether_addr,
		&sn->config->burst_config->bssid,
		1, tlv_data_len, &body_len, &body);
	if (!pkt) {
		err = BCME_NOMEM;
		goto done;
	}

	/* fill in vendor-specific action frame header */
	err = ftm_vs_fill_action_frmhdr(body, body_len,
		BRCM_FTM_VS_AF_TYPE, BRCM_FTM_VS_INITIATOR_RPT_SUBTYPE);
	if (err != BCME_OK) {
		PKTFREE(FTM_OSH(ftm), pkt, FALSE);
		goto done;
	}

	/* fill in vendor-specific data with rtt_result */
	err = ftm_vs_fill_initiator_rpt(ftm, sn, &rtt_result, body, body_len);
	if (err != BCME_OK) {
		PKTFREE(FTM_OSH(ftm), pkt, FALSE);
		goto done;
	}

	/* transmit initiator-report to peer-target, pkt will be free */
	if (!proxd_tx(ftm->pd, pkt, sn->bsscfg,
		0 /* TBD, sn->config->burst_config->ratespec */, err)) {
		err = BCME_TXFAIL;
		goto done;
	}

done:
	BCM_REFERENCE(err);
	/* ftm_proto_update_tx_cnt(ftm, sn, err); */
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d for session idx %d "
		"body_len %d initiator-report\n",
		FTM_UNIT(ftm), __FUNCTION__, err, sn->idx,
		body_len)));
	return err;
}

/*
	check for FTM Vendor-Specific Action Frame (e.g. Initiator Report Frame)
*/
bool
pdftm_vs_is_ftm_action(pdftm_t *ftm, const dot11_management_header_t *hdr,
	uint8 *body, uint body_len)
{
	ASSERT(FTM_VALID(ftm));
	ASSERT(body != NULL);
	ASSERT((ltoh16(hdr->fc) & FC_KIND_MASK) == FC_ACTION);

	/* check if this is a vendor-specific action frame for initiator-report */
	if (body_len >= DOT11_ACTION_VS_HDR_LEN) {
		dot11_action_vs_frmhdr_t	*vs_afhdr = (dot11_action_vs_frmhdr_t *)body;
		if ((body[DOT11_ACTION_CAT_OFF] == DOT11_ACTION_CAT_VS) &&
			(memcmp(&vs_afhdr->OUI[0], BRCM_PROP_OUI, DOT11_OUI_LEN) == 0) &&
			(vs_afhdr->type == BRCM_FTM_VS_AF_TYPE)) {
			/* found a VS initiator-report action frame */
			return TRUE;
		}
	}

	return FALSE;	/* not a FTM vendor-specific action frame */
}

/*
	(target-side) receive an initiator-report
	body: consist of a vendor-specific action-frame header + variable XTLVs
*/
static int
ftm_vs_handle_rx_initiator_rpt(pdftm_t *ftm, wlc_bsscfg_t *bsscfg,
	dot11_management_header_t *hdr, uint8 *body, uint body_len,
	wlc_d11rxhdr_t *wrxh, ratespec_t rspec)
{
	dot11_action_vs_frmhdr_t *vs_afhdr;
	int err = BCME_OK;
	bcm_xtlv_t *p_tlvs;
	uint tlvs_len;
	pdftm_session_t *sn = NULL;
	pdftm_notify_info_t notify_info;

	/* as a target, check if the session is still active */
	sn = FTM_SESSION_FOR_PEER(ftm, &hdr->sa, WL_PROXD_SESSION_FLAG_TARGET);
	FTM_LOGPROTO(ftm, (("wl%d.%d: %s: handling rx initiator-rpt from " MACF
		", body_len=%d, sn=%p\n",
		FTM_UNIT(ftm), WLC_BSSCFG_IDX(bsscfg), __FUNCTION__,
		ETHERP_TO_MACF(&hdr->sa), body_len, OSL_OBFUSCATE_BUF(sn))));

	/* no need to parse xtlvs of an initiator-report, simply propagate to the
	 * host/app if applicable (see event_mask)
	 */
	vs_afhdr = (dot11_action_vs_frmhdr_t *) body;
	p_tlvs = (bcm_xtlv_t *) vs_afhdr->data;
	tlvs_len = body_len - DOT11_ACTION_VS_HDR_LEN; /* including XTLV headers */

	FTM_INIT_SESSION_NOTIFY_INFO(&notify_info, NULL, &hdr->sa,
		WL_PROXD_EVENT_VS_INITIATOR_RPT,
		D11RXHDR_ACCESS_VAL(&wrxh->rxhdr, ftm->wlc->pub->corerev, RxChan),
		rspec, (uint8 *)p_tlvs, tlvs_len);
	(void) pdftm_notify(ftm, sn != NULL ? sn->bsscfg : bsscfg,
		PDFTM_NOTIF_EVENT_TYPE, &notify_info);

	/* ftm_proto_update_rx_cnt(ftm, sn, err); */

	FTM_LOG_STATUS(ftm, err,
		(("wl%d: %s: status %d for initiator-rpt vs-action with body_len %d\n",
		FTM_UNIT(ftm), __FUNCTION__, err, body_len)));

	return err;
}
#ifdef TOF_DBG
/*
	receive collect debug packet
	body: consist of a vendor-specific action-frame header + debug info
*/
static int
ftm_vs_handle_rx_collect(pdftm_t *ftm, wlc_bsscfg_t *bsscfg,
	const dot11_management_header_t *hdr, const uint8 *body, uint body_len,
	const wlc_d11rxhdr_t *wrxh, ratespec_t rspec)
{
	int err = BCME_OK;

	err = pdburst_rx(NULL, bsscfg, hdr, body, body_len, wrxh, rspec, NULL);

	return err;
}
#endif /* TOF_DBG */
/*
	handle receiving a FTM Vendor Specific Action Frame
*/
int
pdftm_vs_rx_frame(pdftm_t *ftm, wlc_bsscfg_t *bsscfg, dot11_management_header_t *hdr,
	uint8 *body, uint body_len, wlc_d11rxhdr_t *wrxh, ratespec_t rspec)
{
	int err;
	dot11_action_vs_frmhdr_t *vs_afhdr;

	ASSERT(bsscfg != NULL);

	FTM_LOGPROTO(ftm, (("wl%d: %s: handling rx Vendor Specific Action Frame, body_len=%d\n",
		FTM_UNIT(ftm), __FUNCTION__, body_len)));

	if (body_len < DOT11_ACTION_VS_HDR_LEN) {
		err = BCME_BUFTOOSHORT;
		goto done;
	}

	err = BCME_UNSUPPORTED;
	vs_afhdr = (dot11_action_vs_frmhdr_t *)body;
	if (vs_afhdr->type == BRCM_FTM_VS_AF_TYPE) {
		switch (vs_afhdr->subtype) {
			case BRCM_FTM_VS_INITIATOR_RPT_SUBTYPE:
				err = ftm_vs_handle_rx_initiator_rpt(ftm, bsscfg, hdr,
					body, body_len, wrxh, rspec);
			break;
#ifdef TOF_DBG
			case BRCM_FTM_VS_COLLECT_SUBTYPE:
				err = ftm_vs_handle_rx_collect(ftm, bsscfg, hdr,
					body, body_len, wrxh, rspec);
			break;
#endif /* TOF_DBG */
			default:
				ASSERT(0);
			break;
		}
	}

done:
	BCM_REFERENCE(err);
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: exit status %d for vs-action with body length %d\n",
		FTM_UNIT(ftm), __FUNCTION__, err, body_len)));
	/* we handled the errors if any, say okay */
	return BCME_OK;
}

int pdftm_vs_get_frame_len(void *ctx, pdburst_frame_type_t type,
	const pdburst_session_info_t *bsi)
{
	pdftm_session_t *sn = (pdftm_session_t *)ctx;
	uint len = 0;
	uint mf_len = 0;

	ASSERT(FTM_VALID_SESSION(sn));

	switch (type) {
	case PDBURST_FRAME_TYPE_REQ:
		len += sizeof(ftm_vs_req_params_t) + BCM_TLV_HDR_SIZE;
		if (FTM_SESSION_SEQ_EN(sn))
			len +=  sizeof(ftm_vs_seq_params_t) + BCM_TLV_HDR_SIZE;
		break;
	case PDBURST_FRAME_TYPE_MEAS:
		if (FTM_SESSION_SECURE(sn))
			len += sizeof(ftm_vs_sec_params_t) +
				sizeof(ftm_vs_timing_params_t) +
				BCM_TLV_HDR_SIZE;

		if (FTM_VS_TX_MEAS_INFO(bsi)) {
			len += MEAS_INFO_IE_TLV_LEN;
		}

		/* each mf buf in its own vs ie */
		if (FTM_VS_TX_MF_BUF(sn, type, bsi)) {
			int nfrags;
			uint max_frag_len;
			max_frag_len =  BCM_TLV_HDR_SIZE + BCM_TLV_MAX_DATA_SIZE
				- (OFFSETOF(dot11_ftm_vs_ie_t, tlvs) + BCM_TLV_HDR_SIZE);
			nfrags = HOWMANY(bsi->vs_mf_buf_data_len, max_frag_len);
			mf_len += nfrags *(BCM_TLV_HDR_SIZE + BCM_TLV_MAX_DATA_SIZE);
		}
		break;
	default:
		break;
	}

	/* if we have any tlvs, add vendor ie hdr len */
	if (len > 0)
		len += OFFSETOF(dot11_ftm_vs_ie_t, tlvs);
	len += mf_len;
	return len;
}

/* packing support - packing involves taking parameters from session and
 * burst session info and packing to output/frame. burst session info
 * may be updated in the process, but session remains unchanged
 */
static int
ftm_vs_pack_req_params(pdftm_session_t *sn, pdburst_session_info_t *bsi, uint8 *buf)
{
	uint16 flags = 0;

	/* store flags */
	flags |= FTM_VS_REQ_F_VALID;
	if (FTM_SESSION_SEQ_EN(sn)) {
		flags |= FTM_VS_REQ_F_SEQ_EN;
		bsi->flags |= PDBURST_SESSION_FLAGS_SEQ_EN;
	}

	if (FTM_SESSION_IS_VHTACK(sn)) {
		flags |= FTM_VS_REQ_F_VHTACK;
		bsi->flags |= PDBURST_SESSION_FLAGS_VHTACK;
	}

	if (FTM_SESSION_SECURE(sn)) {
		flags |= FTM_VS_REQ_F_SECURE;
		bsi->flags |= PDBURST_SESSION_FLAGS_SECURE;
	}

	if (FTM_SESSION_TSPT1NS(sn)) {
		flags |= FTM_VS_REQ_F_TSPT1NS;
		bsi->flags |= PDBURST_SESSION_FLAGS_TSPT1NS;
	}

	if (FTM_SESSION_TSPT1NS(sn)) {
		flags |= FTM_VS_REQ_F_TSPT1NS;
		bsi->flags |= PDBURST_SESSION_FLAGS_TSPT1NS;
	}

	store16_ua(buf, flags);
	buf += sizeof(flags);

	/* store total frame count */
	*buf++ = bsi->vs_req_params->totfrmcnt;
	*buf++ = sn->config->burst_config->ftm_retries;

	store32_ua(buf, sn->config->burst_config->chanspec);
	buf += sizeof(uint32);

	return BCME_OK;
}

static int
ftm_vs_pack_seq_params(pdftm_session_t *sn, pdburst_session_info_t *bsi, uint8 *buf)
{
	uint16 flags = 0;

	/* store flags */
	flags |= FTM_VS_SEQ_F_VALID;
	if (bsi->flags & PDBURST_SESSION_FLAGS_QPSK)
		flags |= FTM_VS_SEQ_F_QPSK;

	store16_ua(buf, flags);
	buf += sizeof(flags);

	memcpy(buf, bsi->vs_seq_params->d, sizeof(bsi->vs_seq_params->d));
	buf += sizeof(bsi->vs_seq_params->d);

	*buf++ = 0; /* cores - not supported */

	return BCME_OK;
}

static int
ftm_vs_pack_sec_params(pdftm_session_t *sn, pdburst_session_info_t *bsi, uint8 *buf)
{
	uint16 flags = 0;
	pdftm_session_config_t *sncfg;
	scb_t *scb = NULL;
	uint8 *rlenp = NULL;
	int err = BCME_OK;
	wlc_info_t *wlc;
	wlc_bsscfg_t *bsscfg;
	uint8 rbuf[FTM_VS_TPK_RAND_LEN];
	unsigned int rlen = FTM_TPK_LEN;
	ftm_scb_t *ftm_scb;

	wlc = sn->ftm->wlc;
	bsscfg = sn->bsscfg;

	/* store flags */
	flags |= (FTM_VS_SEC_F_VALID | FTM_VS_SEC_F_RANGING_2_0);
	store16_ua(buf, flags);
	buf += sizeof(flags);

	*buf++ = 0; /* rsvd */
	rlenp = buf++; /* length updated later */

	/* need ri rr for seq en, when ltf is not starting. also need clk for rng */
	if (!FTM_SESSION_SEQ_EN(sn) || !wlc->clk ||
		(bsi->flags & PDBURST_SESSION_FLAGS_SEQ_START))
		goto done_ri_rr;

	sncfg = sn->config;

	/* generate and pack ri_rr */
	scb = wlc_scbfind_dualband(wlc, bsscfg, &sncfg->burst_config->peer_mac);
	if (scb == NULL)
		goto done_ri_rr;

	/* ensure we have tpk */
	ftm_scb = FTM_SCB(sn->ftm, scb);
	if (!FTM_SCB_TPK_VALID(ftm_scb)) {
		err = WL_PROXD_E_SEC_NOKEY;
		goto done_ri_rr;
	}

	wlc_getrand(wlc, rbuf, sizeof(rbuf));
	/* note: can not place tpk into buffer directly because digest function needs 32B */
	rlen = FTM_VS_TPK_RAND_LEN;
	pdftm_calc_ri_rr(ftm_scb->tpk, ftm_scb->tpk_len, rbuf, sizeof(rbuf), rbuf, rlen);
	if (rlen > (sizeof(bsi->vs_sec_params->ri) + sizeof(bsi->vs_sec_params->rr)))
		rlen = sizeof(bsi->vs_sec_params->ri) + sizeof(bsi->vs_sec_params->rr);

	ASSERT((rlen / 2) == sizeof(bsi->vs_sec_params->ri));
	ASSERT((rlen / 2) == sizeof(bsi->vs_sec_params->rr));

#ifdef FTM_DONGLE_DEBUG
	if (FTM_DEBUG_OPT(sn->ftm, WL_PROXD_DEBUG_SEC)) {
		prhex("ri_rr:", rbuf, rlen);
	}
#endif // endif

	if ((flags & (FTM_VS_SEC_F_VALID | FTM_VS_SEC_F_RANGING_2_0))
		==  flags) {
		rlen = FTM_TPK_LEN_SECURE_2_0;
	}

	/* place ri and rr in buf and also burst params for use in burst */
	rlen >>= 1; /* rlen has length of ri or rr */
	memcpy(buf, rbuf, rlen);
	buf += sizeof(bsi->vs_sec_params->ri); /* note: covers rlen < sizeof(ri) */
	memcpy(buf, &rbuf[rlen], rlen);
	buf += sizeof(bsi->vs_sec_params->rr);

	memcpy(bsi->vs_sec_params->ri, rbuf, rlen);
	memcpy(bsi->vs_sec_params->rr, &rbuf[rlen], rlen);

done_ri_rr:
	bsi->vs_sec_params->rlen = (uint8)rlen;
	bsi->vs_sec_params->flags = flags;
	if (rlenp) {
		*rlenp = rlen;
	}

	FTM_LOG_STATUS(sn->ftm, err, (("wl%d: %s: status %d for sn idx %d rlen %d\n",
		FTM_UNIT(sn->ftm), __FUNCTION__, err, sn->idx, rlen)));
	return err;
}

static int
ftm_vs_pack_meas_info(pdftm_session_t *sn, pdburst_session_info_t *bsi, uint8 *buf)
{
	uint16 flags = 0;

	bzero(buf, sizeof(ftm_vs_meas_info_t));

	/* store flags */
	flags |= FTM_VS_MEAS_F_VALID;

	if (FTM_SESSION_TSPT1NS(sn)) {
		flags |= FTM_VS_MEAS_F_TSPT1NS;
		bsi->flags |= PDBURST_SESSION_FLAGS_TSPT1NS;
	}

	store16_ua(buf, flags);
	buf += sizeof(flags);

	store32_ua(buf, bsi->vs_meas_info->phy_err);
	buf += sizeof(wl_proxd_phy_error_t);

	return BCME_OK;
}

static int
ftm_vs_pack_mf_buf(pdftm_session_t *sn, pdburst_session_info_t *bsi,
	uint16 frag_off, uint16 frag_len, uint8 *buf)
{
	uint16 flags = 0;

	bzero(buf, sizeof(ftm_vs_mf_buf_t));

	flags |= FTM_VS_MF_BUF_F_VALID;
	store16_ua(buf, flags);
	buf += sizeof(flags);

	store16_ua(buf, frag_off);
	buf += sizeof(frag_off);

	store16_ua(buf, bsi->vs_mf_buf_data_len); /* total */
	buf += sizeof(bsi->vs_mf_buf_data_len);

	memcpy(buf, &bsi->vs_mf_buf_data[frag_off], frag_len);
	return BCME_OK;
}

static int
ftm_vs_pack_timing_params(pdftm_session_t *sn, pdburst_session_info_t *bsi, uint8 *buf)
{
	BCM_REFERENCE(sn);
	bzero(buf, sizeof(ftm_vs_timing_params_t));

	bsi->vs_timing_params->start_seq_time =
		TIMING_TLV_START_SEQ_TIME;
	bsi->vs_timing_params->delta_time_tx2rx =
		TIMING_TLV_DELTA_TIME_TX2RX;

	/* store flags */
	bsi->vs_timing_params->start_seq_time = 0x1;
	bsi->vs_timing_params->delta_time_tx2rx = 0x2;

	store16_ua(buf, bsi->vs_timing_params->start_seq_time);
	buf += sizeof(bsi->vs_timing_params->start_seq_time);

	store16_ua(buf, bsi->vs_timing_params->delta_time_tx2rx);
	buf += sizeof(bsi->vs_timing_params->delta_time_tx2rx);

	return BCME_OK;
}

/* unpacking support - unpacking involves taking parameters from buffer/frame
 * and updating the session and corresponding burst info
 */
static int
ftm_vs_unpack_req_params(pdftm_session_t *sn, const uint8* buf, uint len,
	pdburst_session_info_t *bsi)
{
	int err = BCME_OK;
	uint16 flags;
	pdftm_session_config_t *sncfg;

	sncfg = sn->config;
	bsi->vs_req_params->flags = FTM_VS_F_NONE;

	/* note: length check for each field - so fields may be added at end */
	if (len  < sizeof(flags)) {
		err = BCME_BADLEN;
		goto done;
	}

	flags = ltoh16_ua(buf);
	buf += sizeof(flags);

	if (!(flags & FTM_VS_REQ_F_VALID)) /* ignore invalid */
		goto done;

	bsi->vs_req_params->flags = flags;
	if (flags & FTM_VS_REQ_F_SEQ_EN) {
		bsi->flags |= PDBURST_SESSION_FLAGS_SEQ_EN;
		sncfg->flags |= WL_PROXD_SESSION_FLAG_SEQ_EN;
	}

	if (flags & FTM_VS_REQ_F_VHTACK) {
		bsi->flags |= PDBURST_SESSION_FLAGS_VHTACK;
		sncfg->flags |= WL_PROXD_SESSION_FLAG_VHTACK;
	}

	if ((flags & FTM_VS_REQ_F_SECURE) && FTM_BSSCFG_SECURE(sn->ftm, sn->bsscfg)) {
		bsi->flags |= PDBURST_SESSION_FLAGS_SECURE;
		sncfg->flags |= WL_PROXD_SESSION_FLAG_SECURE;
	}

	if (flags & FTM_VS_REQ_F_TSPT1NS) {
		bsi->flags |= PDBURST_SESSION_FLAGS_TSPT1NS;
		sn->flags |= FTM_SESSION_TSPT1NS;
	}

	if (len > OFFSETOF(ftm_vs_req_params_t, totfrmcnt))
		bsi->vs_req_params->totfrmcnt = *buf++;

	if (len > OFFSETOF(ftm_vs_req_params_t, ftm_retries)) {
		bsi->vs_req_params->ftm_retries = *buf++;
		sncfg->burst_config->ftm_retries = bsi->vs_req_params->ftm_retries;
	}

	if (len >= (OFFSETOF(ftm_vs_req_params_t, chanspec) + sizeof(uint32))) {
		uint32 chanspec = ltoh32_ua(buf);
		bsi->vs_req_params->chanspec = chanspec;
		sncfg->burst_config->chanspec = (chanspec_t)chanspec;
	}

done:
	return err;
}

static int
ftm_vs_unpack_seq_params(pdftm_session_t *sn, const uint8* buf, uint len,
	pdburst_session_info_t *bsi)
{
	int err = BCME_OK;
	uint16 flags;
	BCM_REFERENCE(sn);
	bsi->vs_seq_params->flags = FTM_VS_F_NONE;

	/* note: length check for each field - so fields may be added at end */
	if (len  < sizeof(flags)) {
		err = BCME_BADLEN;
		goto done;
	}

	flags = ltoh16_ua(buf);
	buf += sizeof(flags);

	if (!(flags & FTM_VS_SEQ_F_VALID)) /* ignore invalid */
		goto done;

	bsi->vs_seq_params->flags = flags;
	if (flags & FTM_VS_SEQ_F_QPSK)
		bsi->flags |= PDBURST_SESSION_FLAGS_QPSK;

	if (len >= (OFFSETOF(ftm_vs_seq_params_t, d) + sizeof(bsi->vs_seq_params->d))) {
		memcpy(bsi->vs_seq_params->d, buf, sizeof(bsi->vs_seq_params->d));
		buf += sizeof(bsi->vs_seq_params->d);
	}

	if (len > OFFSETOF(ftm_vs_seq_params_t, cores)) {
		bsi->vs_seq_params->cores = *buf++;
	}

done:
	return err;
}

static int
ftm_vs_unpack_sec_params(pdftm_session_t *sn, const uint8* buf, uint len,
	pdburst_session_info_t *bsi)
{
	int err = BCME_OK;
	uint16 flags;
	uint rlen;
	uint16 ri_len, rr_len, offset;
	bsi->vs_sec_params->flags = FTM_VS_F_NONE;

	/* note: length check for each field - so fields may be added at end */
	if (len  < sizeof(flags)) {
		err = BCME_BADLEN;
		goto done;
	}

	flags = ltoh16_ua(buf);
	buf += sizeof(flags);

	if (!(flags & FTM_VS_SEC_F_VALID)) /* ignore invalid */
		goto done;

	bsi->vs_sec_params->flags = flags;

	ri_len = (flags & FTM_VS_SEC_F_RANGING_2_0) ? FTM_VS_TPK_RI_LEN_SECURE_2_0 :
		FTM_VS_TPK_RI_LEN;
	rr_len = (flags & FTM_VS_SEC_F_RANGING_2_0) ? FTM_VS_TPK_RR_LEN_SECURE_2_0 :
		FTM_VS_TPK_RR_LEN;

	/* ignore rsvd */
	if (len >  OFFSETOF(ftm_vs_sec_params_t, rlen)) {
		rlen = bsi->vs_sec_params->rlen = *buf++;
		if (rlen > sizeof(bsi->vs_sec_params->ri) ||
			rlen > sizeof(bsi->vs_sec_params->rr)) {
			err = BCME_UNSUPPORTED;
			goto done;
		}
	}
	buf++; /* add the length to go to ri */
	offset = sizeof(bsi->vs_sec_params->ri) - ri_len;

	if (len >= (OFFSETOF(ftm_vs_sec_params_t, ri) + ri_len - offset)) {
		memcpy(bsi->vs_sec_params->ri, buf, ri_len);
		buf += ri_len;
	}

	offset = sizeof(bsi->vs_sec_params->ri) + sizeof(bsi->vs_sec_params->rr) - ri_len - rr_len;

	if (len >= (OFFSETOF(ftm_vs_sec_params_t, rr) + rr_len - offset)) {
		memcpy(bsi->vs_sec_params->rr, buf, rr_len);
		buf += rr_len;
	}

done:
	return err;
}

static int
ftm_vs_unpack_meas_info(pdftm_session_t *sn, const uint8* buf, uint len,
	pdburst_session_info_t *bsi)
{
	int err = BCME_OK;
	uint16 flags;

	bsi->vs_meas_info->flags = FTM_VS_F_NONE;

	/* note: length check for each field - so fields may be added at end */
	if (len  < sizeof(flags)) {
		err = BCME_BADLEN;
		goto done;
	}

	flags = ltoh16_ua(buf);
	buf += sizeof(flags);

	if (!(flags & FTM_VS_MEAS_F_VALID)) /* ignore invalid */
		goto done;

	bsi->vs_meas_info->flags = flags;

	if (flags & FTM_VS_MEAS_F_TSPT1NS)
	{
		/* If the vendor device does not set the .01ns timestamp
		 * bit in the measurement frame, still force set the flag to
		 * then use the nanosecond timestamps.
		 */
		bsi->flags |= PDBURST_SESSION_FLAGS_TSPT1NS;
		sn->flags |= FTM_SESSION_TSPT1NS;
	}
	bsi->vs_meas_info->phy_err = ltoh32_ua(buf);

done:
	return err;
}

static int
ftm_vs_unpack_timing_params(pdftm_session_t *sn, const uint8* buf, uint len,
	pdburst_session_info_t *bsi)
{
	int err = BCME_OK;
	uint16 start_seq_time = 0;
	uint32 delta_time_tx2rx = 0;

	BCM_REFERENCE(sn);
	BCM_REFERENCE(len);

	start_seq_time = ltoh16_ua(buf);
	buf += sizeof(bsi->vs_timing_params->start_seq_time);
	delta_time_tx2rx = ltoh32_ua(buf);

	if ((delta_time_tx2rx + start_seq_time) >
		TIMING_TLV_START_DELTA_MAX) {
		printf("error: ftm_vs_unpack_timing_params() seq_time %x   delta_time %x\n",
		start_seq_time, delta_time_tx2rx);
		bsi->vs_timing_params->start_seq_time = TIMING_TLV_START_SEQ_TIME;
		bsi->vs_timing_params->delta_time_tx2rx =  TIMING_TLV_DELTA_TIME_TX2RX;
		err = BCME_ERROR;
	} else if (start_seq_time >= TIMING_TLV_START_SEQ_TIME_MIN &&
		start_seq_time	<=	TIMING_TLV_START_SEQ_TIME_MAX &&
		delta_time_tx2rx >= TIMING_TLV_DELTA_TIME_TX2RX_MIN &&
		delta_time_tx2rx <= TIMING_TLV_DELTA_TIME_TX2RX_MAX &&
		!(delta_time_tx2rx % 4)) {
		bsi->vs_timing_params->start_seq_time = start_seq_time;
		bsi->vs_timing_params->delta_time_tx2rx =  delta_time_tx2rx;
	}
	return err;
}

static int
ftm_vs_unpack_mf_buf(pdftm_session_t *sn, const uint8* buf, uint len,
	pdburst_session_info_t *bsi)
{
	int err = BCME_OK;
	uint16 flags;
	uint16 offset;
	uint16 total;

	bsi->vs_mf_buf->flags = FTM_VS_F_NONE;
	if (len < sizeof(flags)) {
		err = BCME_BADLEN;
		goto done;
	}

	flags = ltoh16_ua(buf);
	buf += sizeof(flags);
	if (!(flags & FTM_VS_MEAS_F_VALID)) {
		goto done;
	}

	if (len < OFFSETOF(ftm_vs_mf_buf_t, data)) {
		goto done;
	}

	offset = bsi->vs_mf_buf->offset = ltoh16_ua(buf);
	buf += sizeof(offset);

	total = bsi->vs_mf_buf->total = ltoh16_ua(buf);
	buf += sizeof(total);

	len -= OFFSETOF(ftm_vs_mf_buf_t, data); /* length of data */
	if ((offset + len) >  bsi->vs_mf_buf_data_max) {
		err = BCME_BUFTOOSHORT;
		goto done;
	}

	/* note: vs_mf_buf_data_len should match total when rx is done */
	memcpy(&bsi->vs_mf_buf_data[offset], buf, len);
	bsi->vs_mf_buf_data_len += len;

done:
	return err;
}

int
ftm_vs_unpack_mf_stats_buf(pdftm_session_t *sn, const uint8* buf, uint len,
	pdburst_session_info_t *bsi)
{
	int err = BCME_OK;
	uint16 flags;
	uint16 offset;
	uint16 total;

	bsi->vs_mf_stats_buf_hdr->flags = FTM_VS_F_NONE;
	if (len < sizeof(flags)) {
		err = BCME_BADLEN;
		goto done;
	}

	flags = ltoh16_ua(buf);
	buf += sizeof(flags);
	if (!(flags & FTM_VS_MEAS_F_VALID))
		goto done;

	if (len < OFFSETOF(ftm_vs_mf_buf_t, data))
		goto done;

	offset = bsi->vs_mf_stats_buf_hdr->offset = ltoh16_ua(buf);
	buf += sizeof(offset);

	total = bsi->vs_mf_stats_buf_hdr->total = ltoh16_ua(buf);
	buf += sizeof(total);

	len -= OFFSETOF(ftm_vs_mf_buf_t, data); /* length of data */
	if ((offset + len) >  bsi->vs_mf_stats_buf_data_max) {
		err = BCME_BUFTOOSHORT;
		goto done;
	}

	/* note: vs_mf_stats_buf_data_len should match total when rx is done */
	memcpy(&bsi->vs_mf_stats_buf_data[offset], buf, len);
	bsi->vs_mf_stats_buf_data_len += len;
done:
	return err;
}

static void
ftm_vs_init_vs_ie(pdftm_session_t *sn, uint8 *buf, uint len,
	dot11_ftm_vs_ie_t **out_vsp, bcm_tlv_t **tlvp)
{
	dot11_ftm_vs_ie_t *vsp;

	ASSERT(len <= BCM_TLV_MAX_DATA_SIZE);

	vsp = (dot11_ftm_vs_ie_t *)buf;
	*out_vsp = vsp;

	vsp->id = DOT11_MNG_PROPR_ID;
	vsp->len = (uint8)len;
	memcpy(vsp->oui, FTM_VS_OUI, sizeof(vsp->oui));
	vsp->sub_type = FTM_VS_IE_TYPE;
	vsp->version = FTM_VS_PARAMS_VERSION;

	*tlvp = (bcm_tlv_t *)vsp->tlvs;
}

/* populate tlv id, len and return length to end of tlv */
static int
ftm_vs_params_tlv_init(pdftm_session_t *sn, uint8 tlv_id, uint8 tlv_len,
	const uint8 *body, uint body_max, bcm_tlv_t *tlvp, uint *out_len)
{
	uint len;
	int err = BCME_OK;

	len = (uint8 *)tlvp - body;
	len += BCM_TLV_HDR_SIZE + tlv_len;
	if (len > body_max) {
		err = BCME_BUFTOOSHORT;
		goto done;
	}

	tlvp->id = tlv_id;
	tlvp->len = tlv_len;

done:
	*out_len = len;
	return err;
}

/* prepare frame - add vendor specific information */
int pdftm_vs_prep_tx(void *ctx, pdburst_frame_type_t type,
	uint8 *body, uint body_max, uint *vs_ie_len,
	pdburst_session_info_t *bsi)
{
	pdftm_session_t *sn = (pdftm_session_t *)ctx;
	dot11_ftm_vs_ie_t *vsp = NULL;
	uint len = 0; /* total body len */
	int err = BCME_OK;
	bcm_tlv_t *tlvp;
	uint prev_ie_len = 0;
	uint max_tlv_len = BCM_TLV_HDR_SIZE + BCM_TLV_MAX_DATA_SIZE;
	uint8 *ri_rr = NULL;
	wlc_phy_tof_secure_2_0_t  tof_sec_params;
	wlc_info_t *wlc;

	bzero(&tof_sec_params, sizeof(tof_sec_params));

	ASSERT(FTM_VALID_SESSION(sn));

	wlc = sn->ftm->wlc;

	len += OFFSETOF(dot11_ftm_vs_ie_t, tlvs);
	if (len > max_tlv_len || len > body_max) {
		err = BCME_BUFTOOSHORT;
		goto done;
	}

	ftm_vs_init_vs_ie(sn, body, len, &vsp, &tlvp);

	if (!bsi) /* no burst session info, no tlvs */
		goto done;

	switch (type) {
	case PDBURST_FRAME_TYPE_REQ:
		if (FTM_VS_PARAMS_VALID2(bsi->vs_req_params)) {
			err = ftm_vs_params_tlv_init(sn, FTM_VS_TLV_REQ_PARAMS,
				sizeof(ftm_vs_req_params_t), body, body_max, tlvp, &len);
			if (err != BCME_OK)
				break;
			err = ftm_vs_pack_req_params(sn, bsi, tlvp->data);
			if (err != BCME_OK)
				break;
			tlvp = BCM_TLV_NEXT(tlvp);
		}

		if (FTM_VS_PARAMS_VALID2(bsi->vs_seq_params) && FTM_SESSION_SEQ_EN(sn)) {
			err = ftm_vs_params_tlv_init(sn, FTM_VS_TLV_SEQ_PARAMS,
				sizeof(ftm_vs_seq_params_t), body, body_max, tlvp, &len);
			if (err != BCME_OK)
				break;
			err = ftm_vs_pack_seq_params(sn, bsi, tlvp->data);
			if (err != BCME_OK)
				break;
			tlvp = BCM_TLV_NEXT(tlvp);
		}
		break;
	case PDBURST_FRAME_TYPE_MEAS:
		if (FTM_VS_PARAMS_VALID2(bsi->vs_sec_params) && FTM_SESSION_SECURE(sn)) {
			err = ftm_vs_params_tlv_init(sn, FTM_VS_TLV_SEC_PARAMS,
				sizeof(ftm_vs_sec_params_t), body, body_max, tlvp, &len);
			if (err != BCME_OK)
				break;
			err = ftm_vs_pack_sec_params(sn, bsi, tlvp->data);
			if (err != BCME_OK)
				break;
			tlvp = BCM_TLV_NEXT(tlvp);
		}

		if (FTM_VS_PARAMS_VALID2(bsi->vs_meas_info)) {
			err = ftm_vs_params_tlv_init(sn, FTM_VS_TLV_MEAS_INFO,
				sizeof(ftm_vs_meas_info_t), body, body_max, tlvp, &len);
			if (err != BCME_OK)
				break;
			err = ftm_vs_pack_meas_info(sn, bsi, tlvp->data);
			if (err != BCME_OK)
				break;
			tlvp = BCM_TLV_NEXT(tlvp);
#ifdef WL_RANGE_SEQ
			if ((bsi->vs_sec_params->flags & FTM_VS_SEC_F_RANGING_2_0) &&
				FTM_SESSION_SECURE(sn)) {
#else
			if (bsi->vs_sec_params->flags & FTM_VS_SEC_F_RANGING_2_0) {
#endif // endif
				if (ftm_vs_get_tof_txcnt(sn->ftm_state->burst) %
					TOF_DEFAULT_FTMCNT_SEQ ==  0) {
					printf("adding timestamp tlv  tof_txcnt %d\n",
					ftm_vs_get_tof_txcnt(sn->ftm_state->burst));
					tlvp->id = FTM_VS_TLV_TIMING_PARAMS;
					tlvp->len = sizeof(ftm_vs_timing_params_t);
					err = ftm_vs_pack_timing_params(sn, bsi,
						tlvp->data);
					if (err != BCME_OK) {
						break;
					}
					memcpy(&tof_sec_params, bsi->vs_timing_params,
						sizeof(*bsi->vs_timing_params));
					tlvp = (bcm_tlv_t *)BCM_TLV_NEXT(tlvp);
				}
			}
		}
		break;
	default:
		break;
	}

	if (type == PDBURST_FRAME_TYPE_MEAS) {
		if (ftm_vs_get_tof_txcnt(sn->ftm_state->burst) %
					TOF_DEFAULT_FTMCNT_SEQ ==  0) {
			/* call phy function to set ri_rr */
			ri_rr = MALLOCZ(wlc->osh, FTM_RI_RR_BUF_LEN);
			if (ri_rr != NULL) {
				if ((bsi->vs_sec_params->flags & ~FTM_VS_SEC_F_RANGING_2_0)
					==  bsi->vs_sec_params->flags) {
					memcpy(ri_rr, bsi->vs_sec_params->ri,
						FTM_VS_TPK_RI_LEN);
					memcpy((ri_rr + FTM_VS_TPK_RI_LEN),
						bsi->vs_sec_params->rr,
						(FTM_TPK_RI_RR_LEN -
						FTM_VS_TPK_RI_LEN));
				} else if ((bsi->vs_sec_params->flags &
						(FTM_VS_SEC_F_VALID |
						FTM_VS_SEC_F_RANGING_2_0))
						==   bsi->vs_sec_params->flags) {
						memcpy(ri_rr, bsi->vs_sec_params->ri,
							FTM_TPK_RI_PHY_LEN_SECURE_2_0);
						memcpy((ri_rr +
							FTM_TPK_RI_PHY_LEN_SECURE_2_0),
							bsi->vs_sec_params->rr,
							FTM_TPK_RI_PHY_LEN_SECURE_2_0);
				}
			}
		}
	}

	if (err != BCME_OK)
		goto done;

	if (vsp)
		vsp->len = (uint8)len - OFFSETOF(dot11_ftm_vs_ie_t, oui);

	err = vs_prep_mf_buf(body, body_max, len, bsi, sn, type, &prev_ie_len, vs_ie_len);

done:
	if (err != BCME_OK) {
		len = 0;
	}
	if (vs_ie_len) {
		*vs_ie_len = len + prev_ie_len;
	}
	if (ri_rr != NULL) {
		MFREE(wlc->osh, ri_rr, FTM_RI_RR_BUF_LEN);
	}
	FTM_LOG_STATUS(sn->ftm, err, (("wl%d: %s: status %d for sn idx %d\n",
		FTM_UNIT(sn->ftm), __FUNCTION__, err, sn->idx)));
	return err;
}

int vs_prep_mf_buf(uint8 *body, uint body_max, uint len, pdburst_session_info_t *bsi,
	pdftm_session_t *sn, pdburst_frame_type_t type, uint *prev_ie_len,
	uint *vs_ie_len)
{
	dot11_ftm_vs_ie_t *vsp = NULL;
	int err = BCME_OK;
	uint8 *mfbody = NULL;
	uint mfbody_max_len = 0u;
	uint mf_len = 0u;
	bcm_tlv_t *tlvp;
	uint frag_len = 0u;
	uint max_tlv_len;

	if (!prev_ie_len || !bsi || !body || (*vs_ie_len < len)) {
		err = BCME_ERROR;
		goto done;
	}

	if (FTM_VS_TX_MF_BUF(sn, pdburst_get_ftype(type), bsi)) {
		max_tlv_len = BCM_TLV_MAX_DATA_SIZE;
		mfbody = body + len; /* write after ftm structures/timestamps) */
		mfbody_max_len = *vs_ie_len - len;
		frag_len = 0;

		while (bsi->vs_mf_buf_data_len != mf_len) {
			uint mf_frag_len;
			if (mfbody_max_len < frag_len) {
				break;
			}
			mfbody += frag_len;
			mfbody_max_len -= frag_len;
			*prev_ie_len += frag_len;
			frag_len = OFFSETOF(dot11_ftm_vs_ie_t, tlvs) + BCM_TLV_HDR_SIZE;

			if (frag_len > max_tlv_len || frag_len > mfbody_max_len) {
				err = BCME_BUFTOOSHORT;
				break;
			}

			ftm_vs_init_vs_ie(sn, mfbody, frag_len, &vsp, &tlvp);
			mf_frag_len  = max_tlv_len -
				(OFFSETOF(dot11_ftm_vs_ie_t, tlvs) + BCM_TLV_HDR_SIZE) -
				OFFSETOF(ftm_vs_mf_buf_t, data);
			if ((bsi->vs_mf_buf_data_len - mf_len) <= mf_frag_len) /* last frag */
				mf_frag_len = bsi->vs_mf_buf_data_len - mf_len;

			err = ftm_vs_params_tlv_init(sn, FTM_VS_TLV_MF_BUF, mf_frag_len +
				OFFSETOF(ftm_vs_mf_buf_t, data), mfbody,
				mfbody_max_len, tlvp, &frag_len);
			if (err != BCME_OK) {
				FTM_ERR(("mf_buf tlv init failed - err=%d\n", err));
				break;
			}

			err = ftm_vs_pack_mf_buf(sn, bsi, mf_len, mf_frag_len, tlvp->data);
			if (err != BCME_OK) {
				FTM_ERR(("pack mf_buf failed, err=%d\n", err));
				break;
			}
			mf_len += mf_frag_len;

			/* adjust previous vsp length, buffer and track prev ies - note: order */
			if (vsp)
				vsp->len = (uint8)frag_len;
			len += frag_len;
		}

		if (err != BCME_OK) {
			vsp = NULL;
			goto done;
		}
	}

	len += mf_len;

#ifdef BCMDBG
	prhex("mf-buf Frm body", body, len);
#endif // endif

	if (prev_ie_len)
		*prev_ie_len = len;
done:
	return err;
}

#ifdef WL_RANGE_SEQ
int
pdftm_vs_rx(pdburst_method_ctx_t ctx, pdburst_frame_type_t type,
	const uint8 *body, uint body_len, pdburst_session_info_t *bsi)
{
	int err = BCME_OK;
	bcm_tlv_t *tlv;
	uint8 vtype = FTM_VS_IE_TYPE;
	dot11_ftm_vs_ie_t *vsp;
	uint vsp_size;
	wlc_info_t *wlc = NULL;
	wl_proxd_bitflips_t bit_flips = FTM_DEFAULT_TARGET_BITFLIPS;
	wl_proxd_snr_t snr = FTM_DEFAULT_TARGET_SNR;

	pdftm_session_t *sn = (pdftm_session_t *)ctx;
	ASSERT(FTM_VALID_SESSION(sn));
	wlc = sn->bsscfg->wlc;

	while ((tlv = bcm_find_vendor_ie((const void *)body,
		body_len + BCM_TLV_HDR_SIZE, FTM_VS_OUI,
		&vtype, sizeof(vtype))) != NULL) {

	vsp = (dot11_ftm_vs_ie_t *)tlv;
	vsp_size = BCM_TLV_HDR_SIZE + vsp->len;

	if (OFFSETOF(dot11_ftm_vs_ie_t, tlvs) > body_len || vsp_size > body_len) {
		err = BCME_BADLEN;
		goto done;
	}

	if (vsp->version != FTM_VS_PARAMS_VERSION) {
		err = WL_PROXD_E_VERNOSUPPORT;
		goto done;
	}

		body += vsp_size;
		body_len -= vsp_size;
	vsp_size -= OFFSETOF(dot11_ftm_vs_ie_t, tlvs);

	/* traverse process our tlvs */
	for (tlv = (bcm_tlv_t *)vsp->tlvs; tlv && bcm_valid_tlv(tlv, vsp_size);
	tlv = bcm_next_tlv(tlv, &vsp_size)) {
		switch (tlv->id) {
		case FTM_VS_TLV_REQ_PARAMS:
			if ((type != PDBURST_FRAME_TYPE_REQ) || !bsi->vs_req_params)
				break;
			err = ftm_vs_unpack_req_params(sn, tlv->data, tlv->len, bsi);
			break;
		case FTM_VS_TLV_SEQ_PARAMS:
			if ((type != PDBURST_FRAME_TYPE_REQ) || !bsi->vs_seq_params)
				break;
			err = ftm_vs_unpack_seq_params(sn, tlv->data, tlv->len, bsi);
			break;
		case FTM_VS_TLV_SEC_PARAMS:
			if ((type != PDBURST_FRAME_TYPE_MEAS) || !bsi->vs_sec_params)
				break;
			err = ftm_vs_unpack_sec_params(sn, tlv->data, tlv->len, bsi);
			vsp_size = vsp->len + BCM_TLV_HDR_SIZE;
			break;
		case FTM_VS_TLV_MEAS_INFO:
			{
				if ((type != PDBURST_FRAME_TYPE_MEAS) &&
					(type != PDBURST_FRAME_TYPE_MEAS_END)) {
					break;
				}
				if (bsi->vs_meas_info == NULL) {
					break;
				}
				err = ftm_vs_unpack_meas_info(sn,
					tlv->data, tlv->len, bsi);
				if (err) {
					FTM_ERR(("Error unpacking target phy err tlv %d\n",
						err));
					goto done;
				}
				sn->target_stats.status |= TARGET_STATS_MEAS_INFO_RCVD;
				sn->target_stats.target_phy_error =
					bsi->vs_meas_info->phy_err;
				FTM_ERR(("%s: TARGET PHY ERROR = %x\n",
					__FUNCTION__, bsi->vs_meas_info->phy_err));
			}
			break;
		case FTM_VS_TLV_TIMING_PARAMS:
			if ((type != PDBURST_FRAME_TYPE_MEAS) || !bsi->vs_timing_params)
				break;
			err = ftm_vs_unpack_timing_params(sn, tlv->data,
				tlv->len, bsi);
			break;
			case FTM_VS_TLV_MF_BUF:
				if (type != PDBURST_FRAME_TYPE_MEAS_END) {
					break;
				}

				err = ftm_vs_unpack_mf_buf(sn, tlv->data, tlv->len, bsi);
				break;

			default: /* ignore unsupported tlvs */
				break;

			}
		}
	}

	if (err == BCME_OK &&
#ifndef PROXD_ROM_COMPAT
		bsi->vs_mf_buf &&
#endif /* B1_ROM_COMPAT */
		(bsi->vs_mf_buf_data_len == bsi->vs_mf_buf->total)) {
		/* mf_buf rx is complete */
		err = phy_tof_calc_snr_bitflips(WLC_PI(wlc),
			bsi->vs_mf_buf_data,
			&bit_flips, &snr);
		FTM_LOGPROTO(sn->ftm,
			(("wl%d.%d: %s: calculated snr/bitflips from"
			"phy: snr=%d bit_flips=%d\n\n",
			FTM_UNIT(sn->ftm), WLC_BSSCFG_IDX(sn->bsscfg),
			__FUNCTION__,
			snr, bit_flips)));
	}
		sn->target_stats.snr = snr;
		sn->target_stats.bit_flips = bit_flips;
done:
	if (err != BCME_OK) {
		/* If there is not our vendor ie, need to clear  proprietary options
		 * for ftmr and ftm1. Not all measurement frames  have the vendor ie.
		 * Remap error so that burst handling can clear the options
		 */
		if ((err == BCME_NOTFOUND) && ((type == PDBURST_FRAME_TYPE_REQ) ||
				!(sn->flags & FTM_SESSION_FTM1_RCVD)))
			err = WL_PROXD_E_NOT_BCM;
	}
	FTM_LOG_STATUS(sn->ftm, err, (("wl%d: %s: status %d for sn idx %d\n",
		FTM_UNIT(sn->ftm), __FUNCTION__, err, sn->idx)));
	return err;
}
#else
int
pdftm_vs_rx(pdburst_method_ctx_t ctx, pdburst_frame_type_t type,
	const uint8 *body, uint body_len, pdburst_session_info_t *bsi)
{
	int err = BCME_OK;
	bcm_tlv_t *tlv;
	uint8 vtype = FTM_VS_IE_TYPE;
	dot11_ftm_vs_ie_t *vsp;
	int vsp_size;
	wlc_info_t *wlc = NULL;
	wl_proxd_bitflips_t bit_flips = FTM_DEFAULT_TARGET_BITFLIPS;
	wl_proxd_snr_t snr = FTM_DEFAULT_TARGET_SNR;

	pdftm_session_t *sn = (pdftm_session_t *)ctx;
	ASSERT(FTM_VALID_SESSION(sn));
	wlc = sn->bsscfg->wlc;

	tlv = bcm_find_vendor_ie(body, body_len, FTM_VS_OUI, &vtype, sizeof(vtype));
	if (!tlv) {
		err = BCME_NOTFOUND;
		goto done;
	}

	body = (uint8 *)tlv;
	body_len -= ((uint8 *)tlv - body);

	vsp = (dot11_ftm_vs_ie_t *)tlv;
	vsp_size = BCM_TLV_HDR_SIZE + vsp->len;

	if (OFFSETOF(dot11_ftm_vs_ie_t, tlvs) > body_len || vsp_size > body_len) {
		err = BCME_BADLEN;
		goto done;
	}

	if (body_len < vsp_size) {
		return BCME_BUFTOOSHORT;
	}
	body += vsp_size;
	body_len -= vsp_size;

	if (vsp->version != FTM_VS_PARAMS_VERSION) {
		err = WL_PROXD_E_VERNOSUPPORT;
		goto done;
	}

	vsp_size -= OFFSETOF(dot11_ftm_vs_ie_t, tlvs);

	/* traverse process our tlvs */
	for (tlv = (bcm_tlv_t *)vsp->tlvs; tlv && bcm_valid_tlv(tlv, vsp_size);
			tlv = bcm_next_tlv(tlv, &vsp_size)) {
		switch (tlv->id) {
		case FTM_VS_TLV_REQ_PARAMS:
			if ((type != PDBURST_FRAME_TYPE_REQ) || !bsi->vs_req_params)
				break;
			err = ftm_vs_unpack_req_params(sn, tlv->data, tlv->len, bsi);
			break;
		case FTM_VS_TLV_SEQ_PARAMS:
			if ((type != PDBURST_FRAME_TYPE_REQ) || !bsi->vs_seq_params)
				break;
			err = ftm_vs_unpack_seq_params(sn, tlv->data, tlv->len, bsi);
			break;
		case FTM_VS_TLV_SEC_PARAMS:
			if ((type != PDBURST_FRAME_TYPE_MEAS) || !bsi->vs_sec_params)
				break;
			err = ftm_vs_unpack_sec_params(sn, tlv->data, tlv->len, bsi);
			break;
		case FTM_VS_TLV_MEAS_INFO:
			{
				if (type != PDBURST_FRAME_TYPE_MEAS) {
					break;
				}

				if (!bsi->vs_meas_info) {
					bsi->vs_meas_info = (ftm_vs_meas_info_t *)
							MALLOCZ(wlc->osh,
								sizeof(ftm_vs_meas_info_t));
					if (!bsi->vs_meas_info) {
						FTM_ERR(("%s: MALLOC of "
							"ftm_vs_meas_info failed \n",
							__FUNCTION__));
						err = BCME_NOMEM;
						goto done;
					}
				}
				err = ftm_vs_unpack_meas_info(sn,
					tlv->data, tlv->len, bsi);
				if (err) {
					FTM_ERR(("Error unpacking target phy err tlv %d\n",
						err));
					goto done;
				}
				sn->target_stats.status |= TARGET_STATS_MEAS_INFO_RCVD;
				sn->target_stats.target_phy_error =
					bsi->vs_meas_info->phy_err;
				FTM_ERR(("%s: TARGET PHY ERROR = %x\n",
					__FUNCTION__, bsi->vs_meas_info->phy_err));
			}
			break;
		case FTM_VS_TLV_TIMING_PARAMS:
			if ((type != PDBURST_FRAME_TYPE_MEAS) || !bsi->vs_timing_params)
				break;
			err = ftm_vs_unpack_timing_params(sn, tlv->data,
				tlv->len, bsi);
			break;
		default: /* ignore unsupported tlvs */
			break;
		}
	}

	/* process additional vs ies - e.g. mf buf */
	bsi->vs_mf_buf_data_len = 0;
	bsi->vs_mf_buf_data_len = 0;
	bsi->vs_mf_buf_data_max = PHY_CH_EST_SMPL_SZ;
	bsi->vs_mf_buf_data = NULL;
	bsi->vs_mf_buf = NULL;
	/* Initialize the mf stats buf. Allocate 512 bytes */
	bsi->vs_mf_stats_buf_data_len = 0;
	bsi->vs_mf_stats_buf_data_max = MF_BUF_MAX_LEN;
	bsi->vs_mf_stats_buf_data = NULL;
	bsi->vs_mf_stats_buf_hdr = NULL;

	while ((tlv = bcm_find_vendor_ie(body, body_len, FTM_VS_OUI,
			&vtype, sizeof(vtype))) != NULL) {
		if (type != PDBURST_FRAME_TYPE_MEAS ||
			!bsi->vs_mf_buf || !bsi->vs_mf_buf_data_max) {
			break;
		}

		if (OFFSETOF(dot11_ftm_vs_ie_t, tlvs) > body_len ||
		    (BCM_TLV_HDR_SIZE + vsp->len) > body_len) {
			err = BCME_BADLEN;
			goto done;
		}

		/* Allocate memory for mf_buf data. */
		bsi->vs_mf_buf_data = (uint8 *)MALLOCZ(wlc->osh,
			bsi->vs_mf_buf_data_max);
		if (!bsi->vs_mf_buf_data) {
			err = BCME_NOMEM;
			goto done;
		}
		/* Allocate memory for vs_mf_buf header */
		bsi->vs_mf_buf = (ftm_vs_mf_buf_t *)MALLOCZ(wlc->osh,
			sizeof(ftm_vs_mf_buf_t));
		if (!bsi->vs_mf_buf) {
			err = BCME_NOMEM;
			goto done;
		}
		/* Allocate memory for vs_mf_buf header */
		bsi->vs_mf_stats_buf_hdr = (ftm_vs_mf_buf_t *)MALLOCZ(wlc->osh,
			sizeof(ftm_vs_mf_buf_t));
		if (!bsi->vs_mf_stats_buf_hdr) {
			err = BCME_NOMEM;
			goto done;
		}
		/* point mf_stats_buf to burstp */
		bsi->vs_mf_stats_buf_data =
			ftm_vs_init_mf_stats_buf(sn->ftm_state->burst);

		vsp = (dot11_ftm_vs_ie_t *)tlv;
		vsp_size = BCM_TLV_HDR_SIZE + vsp->len;
		body += vsp_size;
		body_len -= vsp_size;

		if (vsp->version != FTM_VS_PARAMS_VERSION) {
			err = WL_PROXD_E_VERNOSUPPORT;		/* ignore ? */
			goto done;
		}

		vsp_size -= OFFSETOF(dot11_ftm_vs_ie_t, tlvs);
		for (tlv = (bcm_tlv_t *)vsp->tlvs; tlv && bcm_valid_tlv(tlv, vsp_size);
			tlv = bcm_next_tlv(tlv, &vsp_size)) {
			switch (tlv->id) {
			case FTM_VS_TLV_MF_BUF:
				err = ftm_vs_unpack_mf_buf(sn, tlv->data, tlv->len, bsi);
				if (err) {
					WL_ERROR(("Error unpacking mfbuf err %d\n", err));
						goto done;
				}
				sn->target_stats.status |= TARGET_STATS_MF_BUF_RCVD;
				break;
			case FTM_VS_TLV_MF_STATS_BUF:
				err = ftm_vs_unpack_mf_stats_buf(sn, tlv->data, tlv->len, bsi);
				if (err) {
					printf("Error unpacking mf stats buf err %d\n", err);
					goto done;
				}
			default:
				break;
			}
		}
	}

	if (err == BCME_OK &&
#ifndef PROXD_ROM_COMPAT
		bsi->vs_mf_buf &&
#endif /* B1_ROM_COMPAT */
		(bsi->vs_mf_buf_data_len == bsi->vs_mf_buf->total)) {
		/* mf_buf rx is complete */
		err = wlc_phy_tof_calc_snr_bitflips(WLC_PI(wlc), bsi->vs_mf_buf_data,
			&bit_flips, &snr);
		FTM_LOGPROTO(sn->ftm,
			(("wl%d.%d: %s: calculated snr/bitflips from"
			"phy: snr=%d bit_flips=%d\n\n",
			FTM_UNIT(sn->ftm), WLC_BSSCFG_IDX(sn->bsscfg),
			__FUNCTION__,
			snr, bit_flips)));
		sn->target_stats.snr = snr;
		sn->target_stats.bit_flips = bit_flips;
		if (sn->target_stats.status & TARGET_STATS_MF_BUF_RCVD) {
			sn->target_stats.status |= TARGET_STATS_MF_BUF_BITFLIPS_VALID;
			sn->target_stats.status |= TARGET_STATS_MF_BUF_SNR_VALID;
		}
		ftm_vs_update_mf_stats_len(sn->ftm_state->burst,
			bsi->vs_mf_stats_buf_data_len);
	}
done:
	if (err != BCME_OK) {
		/* If there is not our vendor ie, need to clear  proprietary options
		 * for ftmr and ftm1. Not all measurement frames  have the vendor ie.
		 * Remap error so that burst handling can clear the options
		 */
		if ((err == BCME_NOTFOUND) && ((type == PDBURST_FRAME_TYPE_REQ) ||
				!(sn->flags & FTM_SESSION_FTM1_RCVD)))
			err = WL_PROXD_E_NOT_BCM;
	}
	/* Free up the mf buf data and the mf buf header */
	if (bsi->vs_mf_buf_data) {
		MFREE(wlc->osh, bsi->vs_mf_buf_data, bsi->vs_mf_buf_data_max);
	}
	if (bsi->vs_mf_buf) {
		MFREE(wlc->osh, bsi->vs_mf_buf, sizeof(ftm_vs_mf_buf_t));
	}
	if (bsi->vs_meas_info) {
		MFREE(wlc->osh, bsi->vs_meas_info, sizeof(ftm_vs_meas_info_t));
	}
	/* Free up the mf buf header */
	if (bsi->vs_mf_stats_buf_hdr) {
		MFREE(wlc->osh, bsi->vs_mf_stats_buf_hdr, sizeof(ftm_vs_mf_buf_t));
	}
	FTM_LOGPROTO(sn->ftm, (("wl%d: %s: status %d for sn idx %d\n",
		FTM_UNIT(sn->ftm), __FUNCTION__, err, sn->idx)));
	return err;
}
#endif /* WL_RANGE_SEQ */

wl_proxd_phy_error_t
ftm_vs_tgt_snr_bitfips(pdburst_method_ctx_t ctx,
	uint16 snr_thresh,
	uint16 bitflip_thresh,
	wl_proxd_snr_t *tof_target_snr,
	wl_proxd_bitflips_t *tof_target_bitflips)
{
	pdftm_session_t *sn = (pdftm_session_t *)ctx;
	wl_proxd_phy_error_t target_phy_error = 0;
	if (sn != NULL) {
		*tof_target_snr = sn->target_stats.snr;
		*tof_target_bitflips = sn->target_stats.bit_flips;
		if (sn->target_stats.status & TARGET_STATS_MF_BUF_RCVD) {
			if (sn->target_stats.status & TARGET_STATS_MF_BUF_BITFLIPS_VALID) {
				if (sn->target_stats.bit_flips > bitflip_thresh) {
					target_phy_error |= WL_PROXD_PHY_ERR_BITFLIP;
				}
			}
			if (sn->target_stats.status & TARGET_STATS_MF_BUF_SNR_VALID) {
				if (sn->target_stats.snr < snr_thresh) {
					target_phy_error |= WL_PROXD_PHY_ERR_SNR;
				}
			}
		}
		/* OR the target side phy err */
		target_phy_error |= sn->target_stats.target_phy_error;
	}
	return target_phy_error;
}

void
ftm_vs_reset_target_side_data(pdburst_method_ctx_t ctx)
{
	pdftm_session_t *sn = (pdftm_session_t *)ctx;
	if (sn != NULL) {
		bzero(&sn->target_stats, sizeof(target_side_data_t));
	}
	return;
}

/* API to get "ftm_vs_req_params_t" from FTM VS IE
*
* @param vs_ie: FTM vendor specfic in dot11_ftm_vs_ie_pyld_t format
* @param vs_ie_len: Lenght of FTM vs IE
* @param req_params_tlv_len: pointer to store lenght of FTM_VS_TLV_REQ_PARAMS if found
* @return : Pointer to ftm_vs_req_params_t buf if found
*/
static const uint8 *
ftm_vs_get_req_params_tlv(const dot11_ftm_vs_ie_pyld_t *vs_ie, uint8 vs_ie_len,
	uint16 *req_params_tlv_len)
{
	const uint8 *vs_req_params = NULL;
	const ftm_vs_tlv_t *tlvs = (const ftm_vs_tlv_t *)vs_ie->tlvs;
	uint8 tlvs_len = vs_ie_len - OFFSETOF(dot11_ftm_vs_ie_pyld_t, tlvs);

	/* Check version */
	if (vs_ie->version != FTM_VS_PARAMS_VERSION) {
		vs_req_params = NULL;
		goto done;
	}
	vs_req_params = bcm_get_data_from_xtlv_buf((const uint8*)tlvs, tlvs_len,
		FTM_VS_TLV_REQ_PARAMS, req_params_tlv_len,
		BCM_XTLV_OPTION_LENU8 | BCM_XTLV_OPTION_IDU8);

	/* check for valid req_params */
	if (!vs_req_params || (*req_params_tlv_len != sizeof(ftm_vs_req_params_t))) {
		vs_req_params = NULL;
	}
done:
	return vs_req_params;
}

/* API to pack FTM vendor specific TLVs.
* This will be carried over Nan Vendor specific attribute
* in Ranging Req/Resp frames for exchanging the VS capabilities
*
* @param vs_ie_out: output buffer for packing the FTM vendor elements
* @param vs_ie_in: FTM tlvs received from peer in "ftm_vs_tlv_t" format.
* NULL in case of initiator
* @param vs_ie_in_len: Length of FTM tlvs received from peer.
*/
uint16
wlc_ftm_build_vs_req_params(wlc_ftm_t *ftm, wl_proxd_session_id_t sid,
	dot11_ftm_vs_ie_pyld_t *vs_ie_out, const dot11_ftm_vs_ie_pyld_t *vs_ie_in,
	uint8 vs_ie_in_len)
{
	const ftm_vs_req_params_t *vs_req_params_rx = NULL;
	uint16 vs_req_params_rx_len;
	ftm_vs_req_params_t *vs_req_params = NULL;
	ftm_vs_tlv_t *tlvs = NULL;
	uint16 total_len = 0;

	if (vs_ie_in) {
		/* get req_params_tlv */
		vs_req_params_rx = (const ftm_vs_req_params_t*)
			ftm_vs_get_req_params_tlv(vs_ie_in, vs_ie_in_len, &vs_req_params_rx_len);
		/* Invalid req_params */
		if (!vs_req_params_rx) {
			total_len = 0;
			goto done;
		}
	}
	/* FTM VS main Header */
	if (vs_ie_out) {
		vs_ie_out->sub_type = BRCM_FTM_IE_TYPE;
		vs_ie_out->version = FTM_VS_PARAMS_VERSION;
	}
	total_len += OFFSETOF(dot11_ftm_vs_ie_pyld_t, tlvs);

	/* FTM VS sub TLV header */
	if (vs_ie_out) {
		tlvs = (ftm_vs_tlv_t *)(vs_ie_out->tlvs);
		tlvs->id = FTM_VS_TLV_REQ_PARAMS;
		tlvs->len = sizeof(*vs_req_params);
	}
	total_len += BCM_TLV_HDR_SIZE;

	/* FTM VS request params */
	if (vs_ie_out) {
		vs_req_params = (ftm_vs_req_params_t *)tlvs->data;
		/* In case of responder, validate the received req_params
		* with it own cap and respond.
		*/
		if (vs_req_params_rx) { /* responder */
			if (vs_req_params_rx->flags & FTM_VS_REQ_F_VHTACK) {
				vs_req_params->flags |= FTM_VS_REQ_F_VHTACK;
			}
		}
		/* In case of initiator send the supported Caps */
		if (!vs_ie_in) { /* initiator */
			vs_req_params->flags |= FTM_VS_REQ_F_VHTACK;
		}
	}
	total_len += sizeof(*vs_req_params);
done:
	return total_len;
}

/* Parse the FTM VS TLVs and config them over the given session.
*
* @param ftm: wlc FTM handle
* @param sid: FTM session ID;
* @param vs_ie: FTM prop IE.
* These params are parsed and set in session config
* @param vs_ie_len: Length of FTM Prop IE
*/
int
wlc_ftm_set_session_vs_req_params(wlc_ftm_t * ftm, wl_proxd_session_id_t sid,
	const dot11_ftm_vs_ie_pyld_t *vs_ie, uint16 vs_ie_len)
{
	int err = BCME_OK;
	pdftm_session_t *sn;
	uint16 vs_req_params_len;
	const ftm_vs_req_params_t *vs_req_params = NULL;

	sn = FTM_SESSION_FOR_SID(ftm, sid);
	if (!sn) {
		err = WL_PROXD_E_INVALID_SESSION;
		goto done;
	}

	vs_req_params = (const ftm_vs_req_params_t *)ftm_vs_get_req_params_tlv(vs_ie,
		vs_ie_len, &vs_req_params_len);
	if (!vs_req_params) {
		err = BCME_BADARG;
		goto done;
	}

	if (vs_req_params->flags & FTM_VS_REQ_F_VHTACK) {
		sn->config->flags |= WL_PROXD_SESSION_FLAG_VHTACK;
	}
	/* handle other params if required */
done:
	FTM_LOG_STATUS(ftm, err, (("wl%d: wlc_ftm_set_session_vs_req_params: "
		"set vs params for sid %d status %d\n", FTM_UNIT(ftm), sid, err)));
	return err;
}

#ifdef WL_RANGE_SEQ
void
pdftm_update_session_cfg_flags(pdburst_method_ctx_t ctx, bool seq_en)
{
	pdftm_session_t *sn = (pdftm_session_t *)ctx;
	if (sn) {
		if (seq_en) {
			sn->config->flags |= WL_PROXD_SESSION_FLAG_SEQ_EN;
		}
	}
}

bool
pdftm_get_session_cfg_flag(pdburst_method_ctx_t ctx, bool seq_en)
{
	pdftm_session_t *sn = (pdftm_session_t *)ctx;
	if (sn) {
		if (seq_en) {
			return (sn->config->flags & WL_PROXD_SESSION_FLAG_SECURE) ?
					TRUE : FALSE;
		}

	}
	return 0;
}
#endif /* WL_RANGE_SEQ */
