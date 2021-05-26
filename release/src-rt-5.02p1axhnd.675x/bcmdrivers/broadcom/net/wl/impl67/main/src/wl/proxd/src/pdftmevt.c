/*
 * Proxd FTM method implementation - event support. See twiki FineTimingMeasurement.
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
 * $Id: pdftmevt.c 790688 2020-08-31 20:02:59Z $
 */

#include "pdftmpvt.h"

static void
pdftm_event_signal(pdftm_t *ftm, wl_proxd_event_type_t event_type, pdftm_session_t *sn)
{
	pdftm_event_data_t evd;

	bzero(&evd, sizeof(evd));
	evd.ftm = ftm;
	evd.session = sn;
	evd.event_type = event_type;
	evd.sid = sn != NULL ? sn->sid : WL_PROXD_SESSION_ID_GLOBAL;
	evd.status = sn != NULL ? sn->status : BCME_OK;
	(void)bcm_notif_signal(ftm->h_notif, &evd);
}

static int
ftm_event_get_session_data_len(pdftm_session_t *sn)
{
	int session_status_len, rtt_results_len, num_rtt = 0, tlv_data_len;

	if (FTM_SESSION_IS_RTT_DETAIL(sn)) {
		num_rtt = (sn->ftm_state ? sn->ftm_state->num_rtt : 0);
	}

	session_status_len = sizeof(wl_proxd_ftm_session_status_t);
	/* rtt[0] is the avg_rtt */
	rtt_results_len = sizeof(wl_proxd_rtt_result_t) +
		(num_rtt * sizeof(wl_proxd_rtt_sample_t));

	tlv_data_len = MAX(session_status_len, rtt_results_len);

	return FTM_EVENT_ALLOC_SIZE(tlv_data_len, 1);
}

static int
ftm_send_event(pdftm_t *ftm,  wl_proxd_event_type_t event_type,
	pdftm_session_t *sn, uint16 *tlv_ids, uint16 num_tlv_ids,
	pdftm_iov_tlv_digest_t *dig, wlc_bsscfg_t *bsscfg, const struct ether_addr *addr,
	wl_proxd_status_t status, uint event_len, wl_proxd_event_t *event)
{
	int err = BCME_OK;
	wl_proxd_session_id_t sid;
	wlc_bsscfg_t *evt_bsscfg = NULL;

	sid = sn != NULL ? sn->sid : WL_PROXD_SESSION_ID_GLOBAL;
	proxd_init_event(ftm->pd, event_type, WL_PROXD_METHOD_FTM, sid, event);
	event_len -= OFFSETOF(wl_proxd_event_t, tlvs); /* skip over event header */
	/* TODO: EXCAST typecat to (int *) has to be removed
	 * once uint changes are made in pdftm_iov_pack
	 */
	err = pdftm_iov_pack(ftm, event_len, event->tlvs, tlv_ids, num_tlv_ids, dig,
		(int *)&event_len);
	if (err != BCME_OK) {
		FTM_ERR(("wl%d: %s: status %d packing event %d data for session id %d\n",
			FTM_UNIT(ftm), __FUNCTION__, err, event_type, sid));
		goto done;
	}

	/* send mac event */
	event_len += OFFSETOF(wl_proxd_event_t, tlvs); /* put the event header back */
	evt_bsscfg = (sn && sn->event_bsscfg) ? sn->event_bsscfg : bsscfg;
	proxd_send_event(ftm->pd, evt_bsscfg, status, addr, event, event_len);

done:
	return err;
}

/*
	allocate and send an event with info->data which has already been packed
		in XTLV format
	Note, variables XTLVs may be provided in 'info->data'. Also, 'info->data_len'
	should include num_xtlvs headers + all XTLVs data.
*/
static int
ftm_send_event_xtlv(pdftm_t *ftm, pdftm_session_t *sn, wlc_bsscfg_t *bsscfg,
	pdftm_notify_info_t *info, wl_proxd_event_t **out_event)
{
	int len;
	wl_proxd_event_t *event;

	/* data is already in XTLV format (with tlv hdr) */
	len = FTM_EVENT_ALLOC_SIZE(info->data_len, 0) - BCM_XTLV_HDR_SIZE;
	*out_event = event = MALLOCZ(FTM_OSH(ftm), len);
	if (event == (wl_proxd_event_t *) NULL) {
		return BCME_NOMEM;
	}

	proxd_init_event(ftm->pd, info->event_type, WL_PROXD_METHOD_FTM,
		sn != NULL ? sn->sid : WL_PROXD_SESSION_ID_GLOBAL, event);
	memcpy(event->tlvs, info->data, info->data_len);
	event->len += info->data_len;

	proxd_send_event(ftm->pd, bsscfg, BCME_OK, info->addr, event, len);
	return BCME_OK;
}

static int
ftm_event(pdftm_t *ftm, wlc_bsscfg_t *bsscfg, pdftm_notify_info_t *info)
{
	int err = BCME_OK;
	wl_proxd_event_t *event = NULL;
	int len = 0;
	uint16 tlv_ids[3];
	uint16 num_tlvs = 0;
	pdftm_iov_tlv_digest_t *dig;
#ifdef WL_FTM_RANGE
	int tlvs_len;
#endif /* WL_FTM_RANGE */

	ASSERT(info != NULL);

	dig = pdftm_iov_dig_init(ftm, NULL);
	if (!WL_PROXD_EVENT_ENABLED(ftm->config->event_mask, info->event_type)) {
		FTM_LOG(ftm, (("wl%d: %s: masked event %d, mask 0x%08x\n", FTM_UNIT(ftm),
			__FUNCTION__, info->event_type, ftm->config->event_mask)));
		goto done;
	}

	switch (info->event_type) {
	case WL_PROXD_EVENT_FTM_REQ:
		tlv_ids[num_tlvs++] = WL_PROXD_TLV_ID_FTM_REQ;
		tlv_ids[num_tlvs++] = WL_PROXD_TLV_ID_CHANSPEC;
		tlv_ids[num_tlvs++] = WL_PROXD_TLV_ID_RATESPEC;

		ASSERT(info->data != NULL && info->data_len != 0);
		dig->tlv_data.out.ftm_req = info->data;
		dig->tlv_data.out.ftm_req_len = info->data_len;
		dig->tlv_data.config.chanspec = &info->chanspec;
		dig->tlv_data.config.ratespec = &info->ratespec;

		len = ALIGN_SIZE(info->data_len, 4) + ALIGN_SIZE(sizeof(chanspec_t), 4) +
			ALIGN_SIZE(sizeof(ratespec_t), 4);
		len = FTM_EVENT_ALLOC_SIZE(len, num_tlvs);
		event = MALLOCZ(FTM_OSH(ftm), len);
		if (!event) {
			err = BCME_NORESOURCE;
			break;
		}

		err = ftm_send_event(ftm, info->event_type, NULL,
			tlv_ids, num_tlvs, dig, bsscfg, info->addr, BCME_OK, len, event);
		break;

	case WL_PROXD_EVENT_VS_INITIATOR_RPT: /* data already a set of xtlvs */
		err = ftm_send_event_xtlv(ftm, (pdftm_session_t *) NULL, bsscfg, info, &event);
		if (event != NULL)
			len = ltoh16(event->len);
		goto done;
#ifdef WL_FTM_RANGE
	case WL_PROXD_EVENT_RANGING:
		len = pdftm_ranging_get_event_len(info->rctx, info->event_type);
		event = MALLOCZ(FTM_OSH(ftm), len);
		if (!event) {
			err = BCME_NORESOURCE;
			break;
		}

		proxd_init_event(ftm->pd, info->event_type, WL_PROXD_METHOD_FTM,
			WL_PROXD_SESSION_ID_GLOBAL, event);
		err = pdftm_ranging_pack_tlvs(info->rctx, info->event_type,
			len - OFFSETOF(wl_proxd_event_t, tlvs), (uint8 *)event->tlvs, &tlvs_len);
		if (err != BCME_OK)
			break;
		proxd_send_event(ftm->pd, bsscfg, BCME_OK, info->addr, event,
			tlvs_len + OFFSETOF(wl_proxd_event_t, tlvs));
		break;
#endif /* WL_FTM_RANGE */
	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	if (err != BCME_OK)
		goto done;

done:
	if (event != NULL)
		MFREE(FTM_OSH(ftm), event, len);
	return err;
}

#define FTM_SESSION_EVENT_DATA_LEN FTM_EVENT_ALLOC_SIZE(\
	MAX(sizeof(wl_proxd_rtt_result_t), sizeof(wl_proxd_ftm_session_status_t)), 1)

union ftm_session_event_data {
		wl_proxd_ftm_session_status_t st;
		wl_proxd_rtt_result_t rtt;
};
typedef union ftm_session_event_data ftm_session_event_data_t;

static int
ftm_session_event(pdftm_t *ftm, wlc_bsscfg_t *bsscfg, pdftm_session_t *sn,
	pdftm_notify_info_t *info)
{
	int len;
	uint16 tlv_id;
	pdftm_iov_tlv_digest_t *dig;
	ftm_session_event_data_t u;
	int err = BCME_OK;
	wl_proxd_event_t *event = NULL;
	wl_proxd_event_type_t event_type;

	ASSERT(info != NULL);
	event_type = info->event_type;
	if (!WL_PROXD_EVENT_ENABLED(sn->config->event_mask, event_type)) {
		FTM_LOG(ftm, (("wl%d: %s: session %d, masked event %d, mask 0x%08x\n",
			FTM_UNIT(ftm), __FUNCTION__, sn->sid, event_type, sn->config->event_mask)));
		goto done;
	}

	/* lci/civic report */
	if (event_type == WL_PROXD_EVENT_LCI_MEAS_REP ||
		event_type == WL_PROXD_EVENT_CIVIC_MEAS_REP) {
		ASSERT(info->data != NULL && info->data_len != 0);
		len = ALIGN_SIZE(info->data_len, 4);
		len = FTM_EVENT_ALLOC_SIZE(len, 1);
	}
	else {
		/* plan to send at most two tlvs - status and result */
		len = ftm_event_get_session_data_len(sn);
	}

	event = MALLOCZ(FTM_OSH(ftm), len);
	if (!event) {
		err = BCME_NORESOURCE;
		goto done;
	}

	dig = pdftm_iov_dig_init(ftm, sn);

#ifdef WL_FTM_11K
	if (event_type == WL_PROXD_EVENT_LCI_MEAS_REP) {
		tlv_id = WL_PROXD_TLV_ID_LCI;
		dig->tlv_data.out.lci_rep = info->data;
		dig->tlv_data.out.lci_rep_len = info->data_len;
	}
	else if (event_type == WL_PROXD_EVENT_CIVIC_MEAS_REP)  {
		tlv_id = WL_PROXD_TLV_ID_CIVIC;
		dig->tlv_data.out.civic_rep = info->data;
		dig->tlv_data.out.civic_rep_len = info->data_len;
	}
	/* send status up, except for burst complete send rtt result that includes status */
	else
#endif /* WL_FTM_11K */
	if ((event_type != WL_PROXD_EVENT_BURST_END) || FTM_SESSION_IS_TARGET(sn)) {
		tlv_id = WL_PROXD_TLV_ID_SESSION_STATUS;
		u.st.sid = sn->sid;
		u.st.state = sn->state;
		u.st.status = sn->status;
		u.st.burst_num = sn->ftm_state ? sn->ftm_state->burst_num : -1;
		dig->tlv_data.out.session_status = &u.st;
	} else {
		tlv_id = WL_PROXD_TLV_ID_RTT_RESULT_V2;
		err = pdftm_get_session_result(ftm, sn, &u.rtt, 0); /* no detail here */
		if (err != BCME_OK)
			goto done;
		/*
		 * Update 'u.rtt.num_rtt' here, so that ftm_iov_pack_rtt_result()
		 * will pack with RTT details in ftm_send_event() call path
		 */
		if (FTM_SESSION_IS_RTT_DETAIL(sn))
			u.rtt.num_rtt = sn->ftm_state ? sn->ftm_state->num_rtt : 0;

		dig->tlv_data.out.rtt_result = &u.rtt;
	}

	ftm_send_event(ftm, event_type, sn, &tlv_id, 1,
		dig, bsscfg, info->addr, sn->status, len, event);
done:
	if (event)
		MFREE(FTM_OSH(ftm), event, len);
	return err;
}

void
pdftm_notify(pdftm_t *ftm, wlc_bsscfg_t *bsscfg,
	pdftm_notify_t notif, pdftm_notify_info_t *info)
{
	int err = BCME_OK;

	ASSERT(FTM_VALID(ftm));
	ASSERT(bsscfg != NULL);

	BCM_REFERENCE(err);
	BCM_REFERENCE(info);

	switch (notif) {
	case PDFTM_NOTIF_BSS_UP: /* fall through */
	case PDFTM_NOTIF_BSS_DOWN:
		if (!FTM_BSSCFG_FTM_ENABLED(ftm, bsscfg))
			break;
		if (notif == PDFTM_NOTIF_BSS_DOWN) {
			pdftm_free_sessions(ftm, bsscfg, FALSE);
		} else {
			/* initialize default device address used by FTM - advertised by NAN */
			if (ETHER_ISNULLADDR(&ftm->config->dev_addr) && ftm->wlc->primary_bsscfg) {
				memcpy(&ftm->config->dev_addr,
					&bsscfg->cur_etheraddr, ETHER_ADDR_LEN);
			}
		}
		break;
	case PDFTM_NOTIF_EVENT_TYPE:
		ASSERT(info != NULL);
		if (info->sn != NULL) {
			pdftm_session_t *sn = info->sn;
			ASSERT(sn->bsscfg == bsscfg);
			if (sn->bsscfg != bsscfg || !info->addr ||
				(memcmp(&sn->config->burst_config->peer_mac, info->addr,
				sizeof(struct ether_addr)))) {
				err = WL_PROXD_E_MISMATCH;
				break;
			}

			pdftm_proto_notify(ftm, sn, info->event_type);
			err = ftm_session_event(ftm, bsscfg, sn, info);

			pdftm_event_signal(ftm, info->event_type, sn);
		} else {
			err = ftm_event(ftm, bsscfg, info);

			pdftm_event_signal(ftm, info->event_type, NULL);
		}
		break;
	case PDFTM_NOTIF_NONE: /* fall through */
	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	FTM_LOG_STATUS(ftm, err,
		(("wl%d: %s: bss index %d notif %d status %d\n", FTM_UNIT(ftm),
		__FUNCTION__, WLC_BSSCFG_IDX(bsscfg), notif, err)));
}

#ifdef WL_FTM_RANGE
int
wlc_ftm_event_register(pdftm_t *ftm, wl_proxd_event_mask_t events,
	pdftm_event_callback_t cb, void *cb_ctx)
{
	int err;

	ASSERT(FTM_VALID(ftm));
	BCM_REFERENCE(events);
	err = bcm_notif_add_interest(ftm->h_notif, (bcm_notif_client_callback)cb,
		cb_ctx);

	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d for events 0x%08x\n",
		FTM_UNIT(ftm), __FUNCTION__, err, events)));
	return err;
}

int
wlc_ftm_event_unregister(pdftm_t *ftm,
	pdftm_event_callback_t cb, void *cb_ctx)
{
	int err;
	ASSERT(FTM_VALID(ftm));
	err = bcm_notif_remove_interest(ftm->h_notif, (bcm_notif_client_callback)cb,
		cb_ctx);

	FTM_LOG_STATUS(ftm, err,
		(("wl%d: %s: status %d\n", FTM_UNIT(ftm), __FUNCTION__, err)));
	return err;
}
#endif /* WL_FTM_RANGE */
