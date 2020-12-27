/*
 * Proxd FTM method ranging support. See twiki FineTimingMeasurement.
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
 * $Id: pdftmrange.c 777082 2019-07-18 14:48:21Z $
 */

#include "pdftmpvt.h"

#ifdef WL_FTM_RANGE

static int
ftm_ranging_alloc_sid(wlc_ftm_ranging_ctx_t *rctx, wl_proxd_session_id_t *out_sid)
{
	int err = BCME_NORESOURCE;
	wl_proxd_session_id_t sid;

	if (!rctx->sid_min || !rctx->sid_max)
		goto done;

	/* simple search */
	for (sid = rctx->sid_min; sid <= rctx->sid_max; ++sid) {
		if (!FTM_SESSION_FOR_SID(rctx->ftm, sid)) {
			err = BCME_OK;
			break;
		}
	}

	if (err == BCME_OK && out_sid != NULL)
		*out_sid = sid;

done:
	return err;
}

static pdftm_session_t*
ftm_ranging_get_session(wlc_ftm_ranging_ctx_t *rctx, wl_proxd_session_id_t sid)
{
	int i;
	for (i = 0; i < rctx->num_sids; ++i) {
		if (rctx->sids[i] == sid)
			break;
	}

	if (i >= rctx->num_sids)
		return NULL;

	return FTM_SESSION_FOR_SID(rctx->ftm, sid);
}

static int
ftm_ranging_done(wlc_ftm_ranging_ctx_t *rctx, wl_proxd_status_t status)
{
	int err = BCME_OK;
	wlc_ftm_t *ftm;
	pdftm_notify_info_t ni;

	ftm = rctx->ftm;
	FTM_LOGRANGE(ftm, (("wl%d: %s: status %d\n", FTM_UNIT(ftm), __FUNCTION__, status)));

	if (rctx->state == WL_PROXD_RANGING_STATE_DONE) {
		err = WL_PROXD_E_BAD_STATE;
		goto done;
	}

	rctx->state = WL_PROXD_RANGING_STATE_DONE;
	rctx->status = status;
	FTM_CNT_NOSN(ftm->ftm_cmn, rctx_done);

	if (rctx->cb) {
		(*(rctx->cb))(ftm->wlc, ftm, rctx->status, WL_PROXD_EVENT_RANGING,
			WL_PROXD_SESSION_ID_GLOBAL, rctx->cb_ctx);
	}

	FTM_INIT_RANGING_NOTIFY_INFO(&ni, WL_PROXD_EVENT_RANGING, rctx);
	pdftm_notify(ftm, FTM_BSSCFG(ftm), PDFTM_NOTIF_EVENT_TYPE, &ni);

done:
	return err;
}

static void
ftm_ranging_evcb(void *cb_ctx, wlc_ftm_event_data_t *evdata)
{
	wlc_ftm_t *ftm;
	wlc_ftm_ranging_ctx_t *rctx;
	pdftm_session_t *sn;
	int err = BCME_OK;

	rctx  = (wlc_ftm_ranging_ctx_t *)cb_ctx;
	ASSERT(FTM_RCTX_VALID(rctx));

	ftm = rctx->ftm;
	BCM_REFERENCE(ftm);

	if (rctx->state <= WL_PROXD_RANGING_STATE_NOTSTARTED) {
		err = WL_PROXD_E_BAD_STATE;
		goto done;
	}

	switch (evdata->event_type) {
	case WL_PROXD_EVENT_SESSION_END:
		/* ignore all sessions except ours */
		sn = ftm_ranging_get_session(rctx, evdata->sid);
		if (!sn)
			goto done;

		FTM_CNT_SN(sn, rctx_done);
		rctx->num_done++;
		break;
	default: /* ignore the rest */
		goto done;
	}

	if (rctx->num_done < rctx->num_sids)
		goto done;

	/* all sessions are done - process completion */
	err = ftm_ranging_done(rctx, BCME_OK);
	if (err != BCME_OK)
		goto done;

done:
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d event %d sid %d ctx %p\n",
			FTM_UNIT(ftm), __FUNCTION__, err,
			evdata->event_type, evdata->sid, OSL_OBFUSCATE_BUF(rctx))));
}

/* public interface */
int
wlc_ftm_ranging_create(wlc_ftm_t *ftm,
	wlc_ftm_ranging_cb_t cb, void *cb_ctx,
	wlc_ftm_ranging_ctx_t **out_rctx)
{
	int err = BCME_OK;
	wlc_ftm_ranging_ctx_t *rctx = NULL;
	pdftm_cmn_t *ftm_cmn = ftm->ftm_cmn;
	ASSERT(FTM_VALID(ftm));
	rctx = MALLOCZ(FTM_OSH(ftm), sizeof(*rctx));
	if (!rctx) {
		err = BCME_NOMEM;
		goto done;
	}

	rctx->ftm = ftm;
	rctx->max_sids = ftm_cmn->config->max_rctx_sids;
	/* done_sids, num_sids, status already inited */
	rctx->state = WL_PROXD_RANGING_STATE_NOTSTARTED;
	rctx->flags = WL_PROXD_RANGING_FLAG_NONE;

	rctx->sids = MALLOCZ(FTM_OSH(ftm), sizeof(rctx->sids[0]) * rctx->max_sids);
	if (!rctx->sids) {
		err = BCME_NOMEM;
		goto done;
	}

	err = wlc_ftm_event_register(ftm, WL_PROXD_EVENT_MASK_ALL,
		ftm_ranging_evcb, rctx);
	if (err != BCME_OK)
		goto done;

	FTM_CNT_NOSN(ftm->ftm_cmn, rctx);
	if (cb) {
		rctx->cb = cb;
		rctx->cb_ctx = cb_ctx;
	}

done:
	if (err == BCME_OK) {
		*out_rctx = rctx;
	} else if (rctx != (wlc_ftm_ranging_ctx_t *) NULL) {
		(void)wlc_ftm_ranging_destroy(&rctx);
	}

	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d ctx %p\n",
			FTM_UNIT(ftm), __FUNCTION__, err, OSL_OBFUSCATE_BUF(rctx))));
	return err;
}

int
wlc_ftm_ranging_destroy(wlc_ftm_ranging_ctx_t **in_rctx)
{
	int err = BCME_OK;
	wlc_ftm_ranging_ctx_t *rctx = NULL;
	wlc_ftm_t *ftm = NULL;
	int i;

	if (!in_rctx || !*in_rctx)
		goto done;

	rctx = *in_rctx;
	*in_rctx = NULL;

	ASSERT(FTM_RCTX_VALID(rctx));
	ftm = rctx->ftm;

	err = wlc_ftm_event_unregister(ftm, ftm_ranging_evcb, rctx);
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d unregistering for events\n",
		FTM_UNIT(ftm), __FUNCTION__, err)));

	if (rctx->flags & WL_PROXD_RANGING_FLAG_DEL_SESSIONS_ON_STOP) {
		for (i = 0; i < rctx->num_sids; ++i) {
			pdftm_session_t *sn;
			wl_proxd_session_id_t sid;

			sid = rctx->sids[i];
			sn = FTM_SESSION_FOR_SID(ftm, sid);
			if (!sn)
				continue;

			err = pdftm_set_iov(ftm, sn->bsscfg,
				WL_PROXD_CMD_DELETE_SESSION, sid, NULL, 0);
			FTM_LOG_STATUS(ftm, err,
				(("wl%d: %s: status %d deleting session w/sid %d\n",
				FTM_UNIT(ftm), __FUNCTION__, err, sid)));
		}
	}

	if (rctx->sids) {
		MFREE(FTM_OSH(ftm), rctx->sids, sizeof(rctx->sids[0]) * rctx->max_sids);
	}

	rctx->ftm = NULL;
	MFREE(FTM_OSH(ftm), rctx, sizeof(*rctx));

done:
	if (ftm)
		FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d\n",
			FTM_UNIT(ftm), __FUNCTION__, err)));
	return err;
}

int
wlc_ftm_ranging_start(wlc_ftm_ranging_ctx_t *rctx)
{
	int err = BCME_OK;
	wlc_ftm_t *ftm;
	int i;
	int ret_err = BCME_OK;

	ASSERT(FTM_RCTX_VALID(rctx));
	ftm = rctx->ftm;

	if (rctx->state != WL_PROXD_RANGING_STATE_NOTSTARTED) {
		ret_err = WL_PROXD_E_BAD_STATE;
		goto done;
	}

	if (!rctx->num_sids) {
		ret_err = BCME_BADARG;
		goto done;
	}

	for (i = 0; i < rctx->num_sids; ++i) {
		pdftm_session_t *sn;

		sn = FTM_SESSION_FOR_SID(ftm, rctx->sids[i]);
		if (!sn) {
			FTM_LOG_STATUS(ftm, err, (("wl%d: %s: session for sid %d not found\n",
				FTM_UNIT(ftm), __FUNCTION__, rctx->sids[i])));
			ret_err = WL_PROXD_E_INVALID_SESSION;
			continue;
		}

		err = pdftm_start_session(sn);
		if (err != BCME_OK) {
			FTM_LOG_STATUS(ftm, err, (("wl%d: %s: error %d starting session id %d\n",
				FTM_UNIT(ftm), __FUNCTION__, err, rctx->sids[i])));
			ret_err = err;
			(void) pdftm_stop_session(sn, err);
		}
		FTM_CNT_SN(sn, rctx);
	}

	rctx->state = WL_PROXD_RANGING_STATE_INPROGRESS;

done:
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d state %d\n",
			FTM_UNIT(ftm), __FUNCTION__, ret_err, rctx->state)));
	return ret_err;
}

int
wlc_ftm_ranging_cancel(wlc_ftm_ranging_ctx_t *rctx)
{
	int err = BCME_OK;
	wlc_ftm_t *ftm;
	int i;

	ASSERT(FTM_RCTX_VALID(rctx));
	ftm = rctx->ftm;

	if (rctx->state == WL_PROXD_RANGING_STATE_DONE)
		goto done;

	for (i = 0; i < rctx->num_sids; ++i) {
		pdftm_session_t *sn;

		sn = FTM_SESSION_FOR_SID(ftm, rctx->sids[i]);
		if (!sn) {
			FTM_LOG_STATUS(ftm, err, (("wl%d: %s: session for sid %d not found\n",
				FTM_UNIT(ftm), __FUNCTION__, rctx->sids[i])));
			continue;
		}

		err = pdftm_stop_session(sn, WL_PROXD_E_CANCELED);
		if (err != BCME_OK) {
			FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d stopping session sid %d\n",
				FTM_UNIT(ftm), __FUNCTION__, err, rctx->sids[i])));
			continue;
		}
	}

	err = ftm_ranging_done(rctx, WL_PROXD_E_CANCELED);

done:
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status rctx status %d %d state %d\n",
			FTM_UNIT(ftm), __FUNCTION__, err, rctx->status, rctx->state)));
	return err;
}

int
wlc_ftm_ranging_add_sid(wlc_ftm_ranging_ctx_t *rctx,
	wlc_bsscfg_t *bsscfg, wl_proxd_session_id_t *in_sid)
{
	wl_proxd_session_id_t sid;
	pdftm_session_t *sn;
	bool newsn = FALSE;
	pdftm_t *ftm;
	int err = BCME_OK;

	ASSERT(FTM_RCTX_VALID(rctx));
	ftm = rctx->ftm;

	if (!in_sid) {
		err = BCME_BADARG;
		goto done;
	}

	/* disallow adding sessions after start */
	if (rctx->state > WL_PROXD_RANGING_STATE_NOTSTARTED) {
		err = WL_PROXD_E_BAD_STATE;
		goto done;
	}

	if (rctx->num_sids >= rctx->max_sids) {
		err = BCME_NORESOURCE;
		goto done;
	}

	sid = *in_sid;
	if (sid == WL_PROXD_SESSION_ID_GLOBAL) {
		err = ftm_ranging_alloc_sid(rctx, &sid);
		if (err != BCME_OK)
			goto done;

		err = pdftm_alloc_session(ftm, bsscfg, sid, &sn, &newsn);
		if (err != BCME_OK)
			goto done;

		ASSERT(sn != NULL && newsn != FALSE);
		err = pdftm_change_session_state(sn, BCME_OK, WL_PROXD_SESSION_STATE_CONFIGURED);
		if (err != BCME_OK) {
			pdftm_free_session(&sn);
			goto done;
		}

		*in_sid = sid;
	} else {
		sn = ftm_ranging_get_session(rctx, sid);
		if (sn != NULL) /* already a member */
			goto done;

		sn = FTM_SESSION_FOR_SID(ftm, sid);
		if (!sn) {
			err = BCME_NOTFOUND;
			goto done;
		}
	}

	rctx->sids[rctx->num_sids++] = sid;

done:
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d state %d\n",
			FTM_UNIT(rctx->ftm), __FUNCTION__, err, rctx->state)));
	return err;
}

int
wlc_ftm_ranging_set_flags(wlc_ftm_ranging_ctx_t *rctx,
	wl_proxd_ranging_flags_t flags, wl_proxd_ranging_flags_t mask)
{
	ASSERT(FTM_RCTX_VALID(rctx));

	FTM_UPDATE_FLAGS(rctx->flags, flags, mask);
	return BCME_OK;
}

int
wlc_ftm_ranging_lookup_sid(wlc_ftm_ranging_ctx_t *rctx,
	const struct ether_addr *addr, wl_proxd_session_id_t *sid)
{
	int err = BCME_OK;
	pdftm_session_t *sn;

	ASSERT(FTM_RCTX_VALID(rctx));
	ASSERT(addr != NULL);
	sn = FTM_SESSION_FOR_PEER(rctx->ftm, addr, WL_PROXD_SESSION_FLAG_NONE);
	if (!sn) {
		err = BCME_NOTFOUND;
	} else {
		sn = ftm_ranging_get_session(rctx, sn->sid);
		if (!sn)
			err = BCME_NOTFOUND;
		else if (sid)
			*sid = sn->sid;
	}

	return err;
}

int
wlc_ftm_ranging_get_sids(wlc_ftm_ranging_ctx_t *rctx,
	int max_sids, wl_proxd_session_id_list_t *sids)
{
	int i;
	int err = BCME_OK;
	ASSERT(FTM_RCTX_VALID(rctx));
	ASSERT(sids != NULL);

	if (!sids) {
		err = BCME_BADARG;
		goto done;
	}

	sids->num_ids = MIN(max_sids, rctx->num_sids);
	for (i = 0; i < sids->num_ids; ++i)
		sids->ids[i] = rctx->sids[i];

	if (max_sids < rctx->num_sids)
		err = BCME_BUFTOOSHORT;

done:
	return err;
}

int
wlc_ftm_ranging_get_result(wlc_ftm_ranging_ctx_t *rctx, wl_proxd_session_id_t sid,
	int max_rtt, wl_proxd_rtt_result_t *res)
{
	int err;
	pdftm_session_t *sn;

	ASSERT(FTM_RCTX_VALID(rctx));

	sn = ftm_ranging_get_session(rctx, sid);
	if (sn != NULL)
		err = pdftm_get_session_result(rctx->ftm, sn, res, max_rtt);
	else
		err = BCME_NOTFOUND;

	return err;
}

int
wlc_ftm_ranging_get_info(wlc_ftm_ranging_ctx_t *rctx, wl_proxd_ranging_info_t *info)
{
	ASSERT(FTM_RCTX_VALID(rctx));
	ASSERT(info != NULL);

	info->status = rctx->status;
	info->state = rctx->state;
	info->flags = rctx->flags;
	info->num_sids = rctx->num_sids;
	info->num_done = rctx->num_done;
	return BCME_OK;
}

int
pdftm_ranging_get_event_len(wlc_ftm_ranging_ctx_t *rctx, wl_proxd_event_type_t event_type)
{
	int data_len;
	uint16 num_tlvs;

	ASSERT(FTM_RCTX_VALID(rctx));
	BCM_REFERENCE(event_type);

	/* tlvs returned are - ranging info + rtt result for each session */
	num_tlvs = rctx->num_sids + 1;

	data_len = sizeof(wl_proxd_ranging_info_t);
	data_len += (rctx->num_sids * sizeof(wl_proxd_rtt_result_t));
	return FTM_EVENT_ALLOC_SIZE(data_len, num_tlvs);
}

int
pdftm_ranging_pack_tlvs(wlc_ftm_ranging_ctx_t *rctx,
	wl_proxd_event_type_t event_type, int max_tlvs_len, uint8* tlvs, int *out_tlvs_len)
{
	int err = BCME_OK;
	uint16 tlv_id;
	int tlvs_len = 0;
	pdftm_iov_tlv_digest_t *dig;
	wlc_ftm_t *ftm;
	wl_proxd_ranging_info_t ranging_info;
	int i;
	int len;
	wl_proxd_rtt_result_t res;
	pdftm_session_t *sn;
	wl_proxd_session_id_t sid;

	ASSERT(FTM_RCTX_VALID(rctx));
	BCM_REFERENCE(event_type);

	ftm = rctx->ftm;
	dig = pdftm_iov_dig_init(ftm, NULL);

	err = wlc_ftm_ranging_get_info(rctx, &ranging_info);
	if (err != BCME_OK) {
		FTM_LOG_STATUS(ftm, err,
			(("wl%d: %s: status %d getting ranging info\n",
				FTM_UNIT(ftm), __FUNCTION__, err)));
	} else {
		tlv_id = WL_PROXD_TLV_ID_RANGING_INFO;
		dig->tlv_data.out.ranging_info = &ranging_info;
		err = pdftm_iov_pack(ftm, max_tlvs_len - tlvs_len,
			(wl_proxd_tlv_t *)&tlvs[tlvs_len], &tlv_id, 1, dig, &len);
		if (err != BCME_OK) {
			FTM_LOG_STATUS(ftm, err,
				(("wl%d: %s: status %d packing ranging info\n",
					FTM_UNIT(ftm), __FUNCTION__, err)));
		} else {
			tlvs_len += len;
		}
	}

	tlv_id = WL_PROXD_TLV_ID_RTT_RESULT_V2;
	for (i = 0; i < rctx->num_sids && tlvs_len < max_tlvs_len; ++i) {
		sid = rctx->sids[i];
		sn = FTM_SESSION_FOR_SID(ftm, sid);
		if (!sn) {
			FTM_LOG_STATUS(ftm, err,
				(("wl%d: %s: status %d no session sid %d\n",
					FTM_UNIT(ftm), __FUNCTION__, err, sid)));
			continue;
		}

		err = pdftm_get_session_result(ftm, sn, &res, 0); /* no detail */
		if (err != BCME_OK) {
			FTM_LOG_STATUS(ftm, err,
				(("wl%d: %s: status %d getting result for session sid %d\n",
					FTM_UNIT(ftm), __FUNCTION__, err, sid)));
			continue;
		}

		dig->session = sn;
		dig->tlv_data.out.rtt_result = &res;
		err = pdftm_iov_pack(ftm, max_tlvs_len - tlvs_len,
			(wl_proxd_tlv_t *)&tlvs[tlvs_len], &tlv_id, 1, dig, &len);
		if (err != BCME_OK) {
			FTM_LOG_STATUS(ftm, err,
				(("wl%d: %s: status %d packing result for session sid %d\n",
					FTM_UNIT(ftm), __FUNCTION__, err, sid)));
			continue;
		}

		tlvs_len += len;
	}

	if (out_tlvs_len)
		*out_tlvs_len = tlvs_len;

	return err;
}

void
pdftm_ranging_pack_sessions(wlc_ftm_ranging_ctx_t *rctx,
	uint8 **buf, int buf_len)
{
	uint i;

	ASSERT(FTM_RCTX_VALID(rctx));
	ASSERT(buf_len >= (int)(OFFSETOF(wl_proxd_session_id_list_t, ids) +
			rctx->num_sids * sizeof(uint16)));
	ASSERT(buf && *buf);

	/* pack sessions-id list (data only, no tlv-header) */
	/* update the num sessions */
	htol16_ua_store((uint16) rctx->num_sids, *buf);
	*buf += sizeof(uint16);

	for (i = 0; i < rctx->num_sids; i++) {
		htol16_ua_store((uint16) rctx->sids[i], *buf);
		*buf += sizeof(uint16);
	}

	return;
}

int
wlc_ftm_ranging_set_sid_range(wlc_ftm_ranging_ctx_t *rctx,
    wl_proxd_session_id_t sid_min, wl_proxd_session_id_t sid_max)
{
	int err = BCME_OK;

	ASSERT(FTM_RCTX_VALID(rctx));
	if (!sid_min || !sid_max || (sid_min > sid_max)) {
		err = BCME_RANGE;
		goto done;
	}

	rctx->sid_min = sid_min;
	rctx->sid_max = sid_max;

done:
	return err;
}

#endif /* WL_FTM_RANGE */
