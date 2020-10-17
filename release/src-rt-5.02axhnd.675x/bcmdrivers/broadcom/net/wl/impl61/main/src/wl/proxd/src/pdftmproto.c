/*
 * Proxd FTM method implementation - protocol support. See twiki FineTimingMeasurement.
 *
 * Copyright 2019 Broadcom
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
 * $Id: pdftmproto.c 777979 2019-08-19 23:14:13Z $
 */

#include "pdftmpvt.h"
#include <wlc_iocv.h>

static uint8
ftm_proto_get_chaninfo(pdftm_t *ftm, pdftm_session_t *sn)
{
	uint8 ci;
	chanspec_t chspec;
	ratespec_t rspec;

	ci = FTM_PARAMS_CHAN_INFO_NO_PREF;
	chspec = sn->config->burst_config->chanspec;
	rspec = sn->config->burst_config->ratespec;

	/* TBD 5 MHz, HT_MF, DMG */

	if (CHSPEC_IS20(chspec)) {
		if (RSPEC_ISVHT(rspec))
			ci = FTM_PARAMS_CHAN_INFO_VHT_20;
		else if (!RSPEC_ISHT(rspec))
			ci = FTM_PARAMS_CHAN_INFO_NON_HT_20;
		else
			ci = FTM_PARAMS_CHAN_INFO_HT_MF_20;
	} else if (CHSPEC_IS40(chspec)) {
		if (RSPEC_ISVHT(rspec))
			ci = FTM_PARAMS_CHAN_INFO_VHT_40;
		else
			ci = FTM_PARAMS_CHAN_INFO_HT_MF_40;
	} else if (CHSPEC_IS80(chspec)) {
		ci = FTM_PARAMS_CHAN_INFO_VHT_80;
	} else if (CHSPEC_IS8080(chspec)) {
		ci = FTM_PARAMS_CHAN_INFO_VHT_80_80;
	} else if (CHSPEC_IS160(chspec)) {
		ci = FTM_PARAMS_CHAN_INFO_VHT_160;
	}

	FTM_LOGPROTO(ftm, (("wl%d: %s: chaninfo %d  for chanspec 0x%04x, ratespec 0x%08x\n",
		FTM_UNIT(ftm), __FUNCTION__, (int)ci, chspec, rspec)));

	return ci;
}

/* For NAN Ranging, we need to get FTM1 params  to be exchanged in
* NAN Ranging frames (before FTM burst starts). So calling this API with
* req_err = BCME_NOTREADY and exit after filling the FTM1 params
*/
int
ftm_proto_get_ftm1_params(pdftm_t *ftm, pdftm_session_t *sn, int req_err,
	dot11_ftm_params_t *params)
{
	uint8 st;
	int val = 0;
	uint8 asap;
	uint16 partial_tsf = 0;
	uint8 ftm1; /* asap capable */
	uint8 bexp;
	uint8 btmo;
	uint16 bperiod;
	uint8 chaninfo;
	const pdftm_session_config_t *sncfg;
	uint64 sn_tsf = 0;
	uint64 cur_tsf;
	int err;
	pdftm_cmn_t *ftm_cmn = ftm->ftm_cmn;

	sncfg = sn->config;
	asap = (sncfg->flags & WL_PROXD_SESSION_FLAG_ASAP) ? 1 : 0;
	if (FTM_SESSION_IS_TARGET(sn) && (ftm_cmn->config->flags & WL_PROXD_FLAG_ASAP_CAPABLE)) {
		ftm1 = (ftm_cmn->caps & WL_PROXD_FTM_CAP_FTM1) ? 0x1: 0x0;
	} else {
		ftm1 = 0;
	}

	if (FTM_SESSION_INITIATOR_NOPREF(sn, WL_PROXD_SESSION_FLAG_NUM_BURST_NOPREF)) {
		bexp = FTM_PARAMS_NBURSTEXP_NOPREF;
	} else {
		FTM_PROTO_BEXP(sncfg->num_burst, bexp);
	}

	if (!bexp) {
		/* single burst */
		bperiod = 0; /* reserved */
	} else {
		if (FTM_SESSION_INITIATOR_NOPREF(sn, WL_PROXD_SESSION_FLAG_BURST_PERIOD_NOPREF)) {
			bperiod = FTM_PARAMS_BURST_PERIOD_NOPREF;
		} else {
			bperiod = FTM_USECIN100MILLI(FTM_INTVL2USEC(&sncfg->burst_period));
		}
	}

	if (FTM_SESSION_INITIATOR_NOPREF(sn, WL_PROXD_SESSION_FLAG_BDUR_NOPREF)) {
		btmo = FTM_PARAMS_BURSTTMO_NOPREF - 2; /* adjust for FTM_PARAMS_SET_BURSTTMO */
	} else {
		FTM_PROTO_BURSTTMO(FTM_INTVL2USEC(&sncfg->burst_config->duration), btmo);
	}

	chaninfo = ftm_proto_get_chaninfo(ftm, sn);

	FTM_PARAMS_SET_NBURSTEXP(params, bexp);
	FTM_PARAMS_SET_ASAP(params, asap);
	FTM_PARAMS_SET_FTM1(params, ftm1);
	FTM_PARAMS_SET_BURSTTMO(params, btmo);

	if (FTM_SESSION_INITIATOR_NOPREF(sn, WL_PROXD_SESSION_FLAG_FTM_SEP_NOPREF)) {
		FTM_PARAMS_SET_MINDELTA_USEC(params, FTM_PARAMS_MINDELTA_NOPREF);
	} else {
		FTM_PARAMS_SET_MINDELTA_USEC(params, FTM_INTVL2USEC(&sncfg->burst_config->ftm_sep));
	}

	if (FTM_SESSION_INITIATOR_NOPREF(sn, WL_PROXD_SESSION_FLAG_NUM_FTM_NOPREF)) {
		FTM_PARAMS_SET_FTMS_PER_BURST(params, FTM_PARAMS_FTMS_PER_BURST_NOPREF);
	} else {
		FTM_PARAMS_SET_FTMS_PER_BURST(params, sncfg->burst_config->num_ftm);
	}

	FTM_PARAMS_SET_CHAN_INFO(params, chaninfo);
	FTM_PARAMS_SET_BURST_PERIOD(params, bperiod);

	/* called this api to get ftm1 params */
	if (req_err == BCME_NOTREADY) {
		err = BCME_OK;
		goto done;
	}

	FTM_GET_TSF(ftm, cur_tsf);

	if (FTM_SESSION_IS_TARGET(sn)) {
		pdftm_expiration_t delay;
		switch (req_err) {
		case BCME_OK:
			st = FTM_PARAMS_STATUS_SUCCESSFUL;
			break;
		case WL_PROXD_E_INCAPABLE:
			st = FTM_PARAMS_STATUS_INCAPABLE;
			break;
		case WL_PROXD_E_SCHED_FAIL:
			/* tell not to retry for a while - we have conflicts until
			 * the scheduled burst start.
			 */
			st = FTM_PARAMS_STATUS_FAILED;
			delay = sn->ftm_state ? sn->ftm_state->burst_start - cur_tsf : 0;
			val = FTM_MICRO2SEC(delay);
			break;
		default:
			st = FTM_PARAMS_STATUS_FAILED;
			break;
		}
	} else {
		ASSERT(sncfg->flags & WL_PROXD_SESSION_FLAG_INITIATOR);
		ASSERT(req_err == BCME_OK);
		ASSERT(sn->state == WL_PROXD_SESSION_STATE_BURST);
		st = FTM_PARAMS_STATUS_RESERVED;
	}
	FTM_PARAMS_SET_STATUS(params, st);
	FTM_PARAMS_SET_VALUE(params, val);

	/* partial_tsf is for local tsf. compute it for session and set params */
	sn_tsf = 0;
	err = pdftm_get_session_tsf(sn, &sn_tsf);
	if (sn->ftm_state != NULL && err == BCME_OK) {
		int delta;
		delta = (int)(sn->ftm_state->burst_start - cur_tsf);
		if (delta > FTM_PARAMS_TSF_FW_MAX || delta < -FTM_PARAMS_TSF_BW_MAX) {
			err = WL_PROXD_E_SCHED_FAIL;
			st = FTM_PARAMS_STATUS_FAILED;
		}

		sn_tsf += delta;
		if (delta < 0)
			sn_tsf += FTM_TSF_64KTU;

		partial_tsf = (uint16)FTM_MICRO2TU(sn_tsf);
	} else if (asap && (err != BCME_OK)) {
		err = BCME_OK; /* allow no tsf for asap */
	}
	FTM_PARAMS_SET_PARTIAL_TSF(params, partial_tsf);

done:
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d\n",
		FTM_UNIT(ftm), __FUNCTION__, err)));
	return err;
}

#ifdef WL_FTM_11K
/* iovar get of self LCI payload
 * 'body' indicates where to copy the LCI data
 *  if 'body' is null, return length without copying
 * 'body_len' is how much space left to copy
 */
static int
ftm_proto_get_lci(pdftm_t *ftm, pdftm_session_t *sn,
	uint8 *body, uint body_len)
{
	wlc_info_t *wlc = ftm->wlc;
	uint16 malloc_len = 0;
	dot11_lci_subelement_t unknown_lci;
	uint len = 0;
	int err = BCME_OK;
	wl_rrm_config_ioc_t *rrm_config = NULL; /* output */
	wl_rrm_config_ioc_t param; /* input */

	bzero(&param, sizeof(param));

	bzero(&unknown_lci, sizeof(unknown_lci));
	malloc_len = sizeof(*rrm_config) + TLV_BODY_LEN_MAX - DOT11_MNG_IE_MREP_FIXED_LEN;
	param.id = WL_RRM_CONFIG_GET_LCI;

	rrm_config = MALLOCZ(wlc->osh, malloc_len);
	if (rrm_config == NULL) {
		if (body && (body_len >= DOT11_FTM_LCI_UNKNOWN_LEN)) {
			len = DOT11_FTM_LCI_UNKNOWN_LEN;
			memcpy(body, (uint8 *)&unknown_lci, DOT11_FTM_LCI_UNKNOWN_LEN);
		}
		else if (body == NULL && body_len == 0) {
			/* length check only */
			len = DOT11_FTM_LCI_UNKNOWN_LEN;
		}
		FTM_LOG_STATUS(ftm, BCME_NOMEM, (("wl%d: ERROR: MALLOC failed: %s, size %d\n",
			wlc->pub->unit, __FUNCTION__, malloc_len)));
		return len;
	}

	err = wlc_iovar_op(wlc, WL_RRM_CONFIG_NAME, (void *)&param, sizeof(param),
			(void *)rrm_config,
		malloc_len, IOV_GET, sn->bsscfg->wlcif);
	if (err != BCME_OK) {
		if (body && (body_len >= DOT11_FTM_LCI_UNKNOWN_LEN)) {
			len = DOT11_FTM_LCI_UNKNOWN_LEN;
			memcpy(body, (uint8 *)&unknown_lci, DOT11_FTM_LCI_UNKNOWN_LEN);
		}
		else if (body == NULL && body_len == 0) {
			/* length check only */
			len = DOT11_FTM_LCI_UNKNOWN_LEN;
		}
		FTM_LOG_STATUS(ftm, err, (("wl%d: ERR %d wlc_iovar_op failed: \"rrm_config lci\"\n",
			wlc->pub->unit, err)));
	}
	else {
		if (body && (body_len >= rrm_config->len)) {
			len = rrm_config->len;
			memcpy(body, &rrm_config->data[0], len);
		}
		else if (body == NULL && body_len == 0) {
			/* length check only */
			len = rrm_config->len;
		}
		else {
			ASSERT(body != NULL && body_len != 0);
		}
	}
	MFREE(wlc->osh, rrm_config, malloc_len);
	return len;
}

/* iovar get of self civic payload
 * 'body' indicates where to copy the civic data
 *  if 'body' is null, returns length without copying
 * 'body_len' is how much space left to copy
 */
static int
ftm_proto_get_civic(pdftm_t *ftm, pdftm_session_t *sn,
	uint8 *body, uint body_len)
{
	wlc_info_t *wlc = ftm->wlc;
	uint16 malloc_len = 0;
	dot11_civic_subelement_t unknown_civic;
	uint len = 0;
	int err = BCME_OK;
	wl_rrm_config_ioc_t *rrm_config = NULL; /* output */
	wl_rrm_config_ioc_t param; /* input */

	bzero(&param, sizeof(param));

	bzero(&unknown_civic, sizeof(unknown_civic));
	malloc_len = sizeof(*rrm_config) + TLV_BODY_LEN_MAX - DOT11_MNG_IE_MREP_FIXED_LEN;
	param.id = WL_RRM_CONFIG_GET_CIVIC;

	rrm_config = MALLOCZ(wlc->osh, malloc_len);
	if (rrm_config == NULL) {
		if (body && (body_len >= DOT11_FTM_CIVIC_UNKNOWN_LEN)) {
			len = DOT11_FTM_CIVIC_UNKNOWN_LEN;
			memcpy(body, (uint8 *)&unknown_civic, DOT11_FTM_CIVIC_UNKNOWN_LEN);
		}
		else if (body == NULL && body_len == 0) {
			/* length check only */
			len = DOT11_FTM_CIVIC_UNKNOWN_LEN;
		}
		FTM_LOG_STATUS(ftm, BCME_NOMEM, (("wl%d: ERROR: MALLOC failed: %s, size %d\n",
			wlc->pub->unit, __FUNCTION__, malloc_len)));
		return len;
	}

	err = wlc_iovar_op(wlc, WL_RRM_CONFIG_NAME, (void *)&param, sizeof(param),
			(void *)rrm_config,
		malloc_len, IOV_GET, sn->bsscfg->wlcif);
	if (err != BCME_OK) {
		if (body && (body_len >= DOT11_FTM_CIVIC_UNKNOWN_LEN)) {
			len = DOT11_FTM_CIVIC_UNKNOWN_LEN;
			memcpy(body, (uint8 *)&unknown_civic, DOT11_FTM_CIVIC_UNKNOWN_LEN);
		}
		else if (body == NULL && body_len == 0) {
			/* length check only */
			len = DOT11_FTM_CIVIC_UNKNOWN_LEN;
		}
		FTM_LOG_STATUS(ftm, err, (("wl%d: ERR %d wlc_iovar_op failed: "
			"\"rrm_config civic\"\n",
			wlc->pub->unit, err)));
	}
	else {
		if (body && (body_len >= rrm_config->len)) {
			len = rrm_config->len;
			memcpy(body, &rrm_config->data[0], len);
		}
		else if (body == NULL && body_len == 0) {
			/* length check only */
			len = rrm_config->len;
		}
		else {
			ASSERT(body != NULL && body_len != 0);
		}
	}
	MFREE(wlc->osh, rrm_config, malloc_len);
	return len;
}

/* Fill LCI report
 * body and body_len are changed if report is filled
 */
static void
ftm_proto_fill_lci(pdftm_t *ftm, pdftm_session_t *sn, uint8 **body, uint *body_len)
{
	dot11_rm_ie_t *rmrep_ie;
	/* As we return based on FTM_SESSION_LCI_REQ_RCVD
	 * no check is added for body_len -= sizeof(*rmrep_ie)
	 */
	if (!(sn->flags & FTM_SESSION_LCI_REQ_RCVD))
		return;

	ASSERT(*body_len >= sizeof(*rmrep_ie));

	rmrep_ie = (dot11_rm_ie_t *)(*body);
	rmrep_ie->id = DOT11_MNG_MEASURE_REPORT_ID;
	rmrep_ie->type = DOT11_MEASURE_TYPE_LCI;
	rmrep_ie->mode = 0;
	rmrep_ie->token = sn->lci_req->token;

	*body += sizeof(*rmrep_ie);
	*body_len -= sizeof(*rmrep_ie);

	if (FTM_TX_LCI_ENABLED(ftm->ftm_cmn->config->flags) &&
		isset(sn->bsscfg->ext_cap, DOT11_EXT_CAP_LCI) &&
		(sn->lci_req->subj == DOT11_FTM_LOCATION_SUBJ_REMOTE)) {
		uint8 lci_len = ftm_proto_get_lci(ftm, sn, *body, *body_len);
		rmrep_ie->len = DOT11_RM_IE_LEN - TLV_HDR_LEN + lci_len;
		*body += lci_len;
		*body_len -= lci_len;
		FTM_CNT(ftm->ftm_cmn, sn, lci_rep_tx);
	}
	else {
		/* indicate INCAPABLE if config does not allow */
		rmrep_ie->mode = DOT11_RMREP_MODE_INCAPABLE;
		rmrep_ie->len = DOT11_RM_IE_LEN - TLV_HDR_LEN;
	}
}

/* Fill civic location report
 * body and body_len are changed if report is filled
 */
static void
ftm_proto_fill_civic(pdftm_t *ftm, pdftm_session_t *sn,	uint8 **body, uint *body_len)
{
	dot11_rm_ie_t *rmrep_ie;
	if (!(sn->flags & FTM_SESSION_CIVIC_REQ_RCVD))
		return;

	ASSERT(*body_len >= sizeof(*rmrep_ie));

	rmrep_ie = (dot11_rm_ie_t *)(*body);
	rmrep_ie->id = DOT11_MNG_MEASURE_REPORT_ID;
	rmrep_ie->type = DOT11_MEASURE_TYPE_CIVICLOC;
	rmrep_ie->mode = 0;
	rmrep_ie->token = sn->civic_req->token;

	*body += sizeof(*rmrep_ie);
	*body_len -= sizeof(*rmrep_ie);

	if (FTM_TX_CIV_LOC_ENABLED(ftm->ftm_cmn->config->flags) &&
		isset(sn->bsscfg->ext_cap, DOT11_EXT_CAP_CIVIC_LOC) &&
		(sn->civic_req->subj == DOT11_FTM_LOCATION_SUBJ_REMOTE)) {
		uint8 civic_len = ftm_proto_get_civic(ftm, sn, *body, *body_len);
		rmrep_ie->len = DOT11_RM_IE_LEN - TLV_HDR_LEN + civic_len;
		*body += civic_len;
		*body_len -= civic_len;
		FTM_CNT(ftm->ftm_cmn, sn, civic_rep_tx);
	}
	else {
		/* indicate INCAPABLE if config does not allow */
		rmrep_ie->mode = DOT11_RMREP_MODE_INCAPABLE;
		rmrep_ie->len = DOT11_RM_IE_LEN - TLV_HDR_LEN;
	}
}

/* Fill LCI request
 * body and body_len are changed if request is filled
 */
static void
ftm_proto_fill_lci_req(pdftm_session_t *sn,	uint8 **body, uint *body_len)
{
	dot11_rmreq_ftm_lci_t *rmreq_ie;
	/* lci request */
	if (!(sn->config->flags & WL_PROXD_SESSION_FLAG_REQ_LCI))
		return;

	ASSERT(*body_len >= sizeof(*rmreq_ie));
	rmreq_ie = (dot11_rmreq_ftm_lci_t *)(*body);
	rmreq_ie->id = DOT11_MNG_MEASURE_REQUEST_ID;
	rmreq_ie->type = DOT11_MEASURE_TYPE_LCI;
	rmreq_ie->len = DOT11_MNG_IE_MREQ_LCI_FIXED_LEN;
	rmreq_ie->mode = 0;
	rmreq_ie->token = sn->idx + 1;
	rmreq_ie->subj = DOT11_FTM_LOCATION_SUBJ_REMOTE;

	*body += (TLV_HDR_LEN + DOT11_MNG_IE_MREQ_LCI_FIXED_LEN);
	*body_len -= (TLV_HDR_LEN + DOT11_MNG_IE_MREQ_LCI_FIXED_LEN);
	FTM_CNT(sn->ftm->ftm_cmn, sn, lci_req_tx);
}

/* Fill civic location request
 * body and body_len are changed if request is filled
 */
static void
ftm_proto_fill_civic_req(pdftm_session_t *sn, uint8 **body, uint *body_len)
{
	dot11_rmreq_ftm_civic_t *rmreq_ie;
	/* location civic request */
	if (!(sn->config->flags & WL_PROXD_SESSION_FLAG_REQ_CIV)) {
		return;
	}

	ASSERT(*body_len >= (int)sizeof(*rmreq_ie));
	rmreq_ie = (dot11_rmreq_ftm_civic_t *)(*body);
	rmreq_ie->id = DOT11_MNG_MEASURE_REQUEST_ID;
	rmreq_ie->len = DOT11_MNG_IE_MREQ_CIVIC_FIXED_LEN;
	rmreq_ie->type = DOT11_MEASURE_TYPE_CIVICLOC;
	rmreq_ie->mode = 0;
	rmreq_ie->token = sn->idx + 2;
	rmreq_ie->subj = DOT11_FTM_LOCATION_SUBJ_REMOTE;
	rmreq_ie->civloc_type = DOT11_FTM_CIVIC_LOC_TYPE_RFC4776;
	rmreq_ie->siu = DOT11_FTM_CIVIC_LOC_SI_NONE;
	rmreq_ie->si = DOT11_FTM_CIVIC_LOC_SI_NONE;

	if (*body_len < (TLV_HDR_LEN + DOT11_MNG_IE_MREQ_CIVIC_FIXED_LEN)) {
		return;
	}
	*body += (TLV_HDR_LEN + DOT11_MNG_IE_MREQ_CIVIC_FIXED_LEN);
	*body_len -= (TLV_HDR_LEN + DOT11_MNG_IE_MREQ_CIVIC_FIXED_LEN);
	FTM_CNT(sn->ftm->ftm_cmn, sn, civic_req_tx);
}

/* Returns length needed for self LCI/civic in request or meas frame */
static int
ftm_lci_civic_len(pdftm_session_t *sn, pdburst_frame_type_t type)
{
	uint len = 0;

	if (sn->flags & FTM_SESSION_FTM1_SENT)
		return 0;

	if (type == PDBURST_FRAME_TYPE_REQ) {
		/* lci/civic request */
		if (sn->config->flags & WL_PROXD_SESSION_FLAG_REQ_LCI) {
			len += (TLV_HDR_LEN + DOT11_MNG_IE_MREQ_LCI_FIXED_LEN);
		}
		if (sn->config->flags & WL_PROXD_SESSION_FLAG_REQ_CIV) {
			len += (TLV_HDR_LEN + DOT11_MNG_IE_MREQ_CIVIC_FIXED_LEN);
		}
	}
	else if (type == PDBURST_FRAME_TYPE_MEAS) {
		/* lci report */
		if (sn->flags & FTM_SESSION_LCI_REQ_RCVD) {

			len += sizeof(dot11_rm_ie_t);

			if (FTM_TX_LCI_ENABLED(sn->ftm->ftm_cmn->config->flags) &&
				isset(sn->bsscfg->ext_cap, DOT11_EXT_CAP_LCI) &&
				(sn->lci_req->subj == DOT11_FTM_LOCATION_SUBJ_REMOTE)) {
				len += ftm_proto_get_lci(sn->ftm, sn, NULL, 0);
			}
		}

		/* location civic report */
		if (sn->flags & FTM_SESSION_CIVIC_REQ_RCVD) {
			len += sizeof(dot11_rm_ie_t);

			if (FTM_TX_CIV_LOC_ENABLED(sn->ftm->ftm_cmn->config->flags) &&
				isset(sn->bsscfg->ext_cap, DOT11_EXT_CAP_CIVIC_LOC) &&
				(sn->civic_req->subj == DOT11_FTM_LOCATION_SUBJ_REMOTE)) {
				len += ftm_proto_get_civic(sn->ftm, sn, NULL, 0);
			}
		}
	}

	return len;
}

/* Find LCI/civic measurement request
 * Returns pointer to LCI/civic request tlv, if found,
 * body and body_len are changed only if request is found
 * Returns NULL if not found
 */
static bcm_tlv_t *
ftm_proto_find_loc_req(pdftm_t *ftm, uint8 meas_type,
	const uint8 **body, uint *body_len)
{
	const dot11_rm_ie_t *meas_req = NULL;
	bcm_tlv_t *tlv = NULL;
	bcm_tlv_t *req_tlv = NULL;

	tlv = bcm_parse_tlvs((const uint8 *)*body, *body_len, DOT11_MNG_MEASURE_REQUEST_ID);
	if (tlv) {
		if (tlv->len < DOT11_MNG_IE_MREQ_LCI_FIXED_LEN) {
			goto done;
		}
		uint tlv_size = TLV_HDR_LEN + tlv->len;
		if (tlv_size < sizeof(*meas_req)) {
			goto done;
		}

		meas_req = (const dot11_rm_ie_t *)tlv;
		if (meas_req->type == meas_type) {
			FTM_LOGPROTO(ftm, (("wl%d: %s: recvd %s req, size %d\n",
				FTM_UNIT(ftm), __FUNCTION__,
				(meas_type == DOT11_MEASURE_TYPE_LCI ? "LCI" : "civic"),
				tlv_size)));
			*body_len = FTM_BODY_LEN_AFTER_TLV(*body, *body_len, tlv, tlv_size);
			*body = (uint8 *)tlv + tlv_size;
			req_tlv = tlv;
		}
		else if ((meas_req->type != DOT11_MEASURE_TYPE_LCI) &&
				(meas_req->type != DOT11_MEASURE_TYPE_CIVICLOC)) {
			FTM_LOGPROTO(ftm, (("wl%d: %s: recvd unexpected meas type %d, size %d\n",
				FTM_UNIT(ftm), __FUNCTION__, meas_req->type,
				tlv_size)));
		}
	}

done:
	return req_tlv;
}

/* Find LCI/civic measurement report
 * body and body_len are changed only if report is found
 */
static int
ftm_proto_find_lci_civic_rep(pdftm_t *ftm, pdftm_session_t *sn, uint8 meas_type,
	const uint8 **body, uint *body_len)
{
	bcm_tlv_t *tlv;
	int err = BCME_OK;
	const dot11_rm_ie_t *meas_rep = NULL;
	pdftm_notify_info_t notify_info;

	tlv = bcm_parse_tlvs((const uint8 *)*body, *body_len, DOT11_MNG_MEASURE_REPORT_ID);
	if (tlv) {
		if (tlv->len < DOT11_MNG_IE_MREQ_LCI_FIXED_LEN) {
			goto done;
		}
		uint tlv_size = TLV_HDR_LEN + tlv->len;
		if (tlv_size < sizeof(*meas_rep)) {
			FTM_LOGPROTO(ftm, (("wl%d: %s: recvd measure-rep,  invalid len %d\n",
				FTM_UNIT(ftm), __FUNCTION__, tlv_size)));

			err = WL_PROXD_E_PROTO;
			goto done;
		}

		meas_rep = (const dot11_rm_ie_t *)tlv;
		if (meas_rep->type == meas_type) {
			FTM_LOGPROTO(ftm, (("wl%d: %s: recvd %s rep, tok %d, len %d\n",
				FTM_UNIT(ftm), __FUNCTION__,
				(meas_type == DOT11_MEASURE_TYPE_LCI ? "LCI" : "civic"),
				meas_rep->token, meas_rep->len)));

			/* note: don't decrement body len by tlv_size because other
			 * elements may be intervening
			 */
			*body_len = FTM_BODY_LEN_AFTER_TLV(*body, *body_len,
				tlv, tlv_size);
			*body = (uint8 *)tlv + tlv_size;
			if (meas_rep->mode & DOT11_RMREP_MODE_INCAPABLE) {
				FTM_LOGPROTO(ftm, (("wl%d: %s:incap of meas req, type %d, tok %d\n",
					FTM_UNIT(ftm), __FUNCTION__, meas_rep->type,
					meas_rep->token)));
			}
			else if (meas_rep->mode & DOT11_RMREP_MODE_REFUSED) {
				FTM_LOGPROTO(ftm, (("wl%d: %s:refused meas req, type %d, tok %d\n",
					FTM_UNIT(ftm), __FUNCTION__, meas_rep->type,
					meas_rep->token)));
			}
			if (meas_type == DOT11_MEASURE_TYPE_LCI)
				FTM_CNT(ftm->ftm_cmn, sn, lci_rep_rx);
			else
				FTM_CNT(ftm->ftm_cmn, sn, civic_rep_rx);
			/* save a copy for reference later */
			err = pdftm_session_set_lci_civic_rpt(sn,
				(meas_type == DOT11_MEASURE_TYPE_LCI) ?
				WL_PROXD_TLV_ID_LCI : WL_PROXD_TLV_ID_CIVIC,
				(uint8 *) tlv, tlv_size);
			if (err != BCME_OK)
				goto done;

			FTM_INIT_SESSION_NOTIFY_INFO(&notify_info, sn,
				&sn->config->burst_config->peer_mac,
				meas_type == DOT11_MEASURE_TYPE_LCI ?
				WL_PROXD_EVENT_LCI_MEAS_REP : WL_PROXD_EVENT_CIVIC_MEAS_REP,
				0, 0, (uint8 *) tlv, tlv_size);
			pdftm_notify(ftm, sn->bsscfg, PDFTM_NOTIF_EVENT_TYPE, &notify_info);
		}
		else if ((meas_rep->type != DOT11_MEASURE_TYPE_LCI) &&
				(meas_rep->type != DOT11_MEASURE_TYPE_CIVICLOC)) {
			FTM_LOGPROTO(ftm, (("wl%d: %s: recvd unexpected meas type %d, size %d\n",
				FTM_UNIT(ftm), __FUNCTION__, meas_rep->type,
				tlv_size)));
		}
	}

done:
	return err;
}
#endif /* WL_FTM_11K */

/* resolve the chanspec based on received and requested channel params.
 * note: b/w can only be reduced.
 */
static int
ftm_proto_resolve_chanspec(pdftm_t *ftm, pdftm_session_t *sn,
	chanspec_t rx_cspec, uint8 chaninfo, chanspec_t *ftm_cspec)
{
	int err = BCME_OK;
	chanspec_t cspec = 0;
	uint8 ctl_chan;
	uint16 bw;
	scb_t *scb = NULL;

	ctl_chan = wf_chspec_ctlchan(rx_cspec);
	bw = CHSPEC_BW(rx_cspec);
	switch (chaninfo) {
	case FTM_PARAMS_CHAN_INFO_NON_HT_20: /* fall through */
	case FTM_PARAMS_CHAN_INFO_HT_MF_20:
	case FTM_PARAMS_CHAN_INFO_VHT_20:
		bw = WL_CHANSPEC_BW_20;
		break;
	case FTM_PARAMS_CHAN_INFO_HT_MF_40: /* fall through */
	case FTM_PARAMS_CHAN_INFO_VHT_40:
		bw = WL_CHANSPEC_BW_40;
		break;
	case FTM_PARAMS_CHAN_INFO_VHT_80:
		bw = WL_CHANSPEC_BW_80;
		break;
	case FTM_PARAMS_CHAN_INFO_VHT_80_80:
		if (bw == WL_CHANSPEC_BW_8080) {
			cspec = rx_cspec;
			goto done;
		}

		/* fallback to 80 MHz */
		bw = WL_CHANSPEC_BW_80;
		break;
	case FTM_PARAMS_CHAN_INFO_VHT_160:
		bw = WL_CHANSPEC_BW_160;
		break;
	case FTM_PARAMS_CHAN_INFO_VHT_160_2_RFLOS:
		err = BCME_UNSUPPORTED;
		goto done;
	case FTM_PARAMS_CHAN_INFO_NON_HT_10:
		bw = WL_CHANSPEC_BW_10;
		break;
	case FTM_PARAMS_CHAN_INFO_NON_HT_5:
		bw = WL_CHANSPEC_BW_5;
		break;
	case FTM_PARAMS_CHAN_INFO_DMG_2160:
		err = BCME_UNSUPPORTED;
		break;
	case FTM_PARAMS_CHAN_INFO_NO_PREF: /* fall through */
	default:
		break;
	}

	if (err != BCME_OK)
		goto done;

	/* for AP and associated case, retrict b/w to operating params */
	scb = wlc_scbfindband(ftm->wlc, sn->bsscfg, &sn->config->burst_config->peer_mac,
		CHSPEC_BANDUNIT(rx_cspec));
	if ((BSSCFG_AP(sn->bsscfg) && sn->bsscfg->up) ||
		(scb != NULL && SCB_ASSOCIATED(scb))) {
		uint16 a_bw;
		uint8 a_ctl_chan;
		chanspec_t a_cspec;

		a_cspec = sn->bsscfg->current_bss->chanspec;
		a_ctl_chan = wf_chspec_ctlchan(a_cspec);
		if (a_ctl_chan != ctl_chan) {
			err = BCME_BADCHAN;
			goto done;
		}

		a_bw = CHSPEC_BW(a_cspec);
		if (a_bw < bw)
			bw = a_bw;
	}

	/* compose the chanspec */
	cspec = wf_channel2chspec(ctl_chan, bw);

	FTM_LOGPROTO(ftm, (("wl%d: %s: composed chanspec 0x%04x "
		"from ctl_chan %d bw 0x%04x\n",
		FTM_UNIT(ftm), __FUNCTION__, cspec, (int)ctl_chan, bw)));

	if (!wlc_valid_chanspec_db(FTM_CMI(ftm), cspec)) {
		err = BCME_BADCHAN;
		goto done;
	}

done:
	if (err == BCME_OK)
		*ftm_cspec = cspec;

	FTM_LOGPROTO(ftm, (("wl%d: %s: status %d rx cspec 0x%04x, cspec 0x%04x chaninfo 0x%02x\n",
		FTM_UNIT(ftm), __FUNCTION__, err, rx_cspec, cspec, chaninfo)));
	return err;
}

static int
ftm_proto_resolve_ratespec(pdftm_t *ftm, chanspec_t cspec, uint8 chaninfo,
	ratespec_t rx_rspec, ratespec_t *rspec)
{
	int err = BCME_OK;
	ratespec_t ratespec = 0;
#ifdef WL11AC
	int bandtype;
#endif /* WL11AC */

	ASSERT(rspec != NULL);

	/* set rate b/w based on chaninfo */
	switch (chaninfo) {
	case FTM_PARAMS_CHAN_INFO_NO_PREF:
		ratespec = rx_rspec;
		break;
	case FTM_PARAMS_CHAN_INFO_NON_HT_20: /* fall through */
	case FTM_PARAMS_CHAN_INFO_HT_MF_20:
	case FTM_PARAMS_CHAN_INFO_VHT_20:
		ratespec |= WL_RSPEC_BW_20MHZ;
		break;
	case FTM_PARAMS_CHAN_INFO_HT_MF_40: /* fall through */
	case FTM_PARAMS_CHAN_INFO_VHT_40:
		ratespec |= WL_RSPEC_BW_40MHZ;
		break;
	case FTM_PARAMS_CHAN_INFO_VHT_80:
		ratespec |= WL_RSPEC_BW_80MHZ;
		break;
	case FTM_PARAMS_CHAN_INFO_VHT_80_80: /* fall through */
	case FTM_PARAMS_CHAN_INFO_VHT_160:
	case FTM_PARAMS_CHAN_INFO_VHT_160_2_RFLOS:
		ratespec |= WL_RSPEC_BW_160MHZ;
		break;
	case FTM_PARAMS_CHAN_INFO_NON_HT_10:
	case FTM_PARAMS_CHAN_INFO_NON_HT_5:
	case FTM_PARAMS_CHAN_INFO_DMG_2160:
	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	if ((err != BCME_OK) || (chaninfo == FTM_PARAMS_CHAN_INFO_NO_PREF))
		goto done;
#ifdef WL11AC
	/* set rate based on chaninfo if allowed by band */
	bandtype = CHSPEC_IS5G(cspec) ? WLC_BAND_5G : WLC_BAND_2G;
#endif /* WL11AC */
	switch (chaninfo) {
	case FTM_PARAMS_CHAN_INFO_NON_HT_20:
		ratespec |= LEGACY_RSPEC(WLC_RATE_6M);
		break;
	case FTM_PARAMS_CHAN_INFO_HT_MF_20: /* fall through */
	case FTM_PARAMS_CHAN_INFO_HT_MF_40:
		if (!N_ENAB(FTM_PUB(ftm))) {
			err = BCME_UNSUPPORTED; /* no fallback to legacy */
			break;
		}
		ratespec |=  HT_RSPEC(0);
		break;
	case FTM_PARAMS_CHAN_INFO_VHT_20: /* fall through */
	case FTM_PARAMS_CHAN_INFO_VHT_40:
	case FTM_PARAMS_CHAN_INFO_VHT_80:
	case FTM_PARAMS_CHAN_INFO_VHT_80_80:
	case FTM_PARAMS_CHAN_INFO_VHT_160:
	case FTM_PARAMS_CHAN_INFO_VHT_160_2_RFLOS:
		if (!VHT_ENAB_BAND(FTM_PUB(ftm), bandtype)) {
			if (N_ENAB(FTM_PUB(ftm))) /* fallback to HT */
				ratespec |=  HT_RSPEC(0);
			else
				err = BCME_UNSUPPORTED;
			break;
		}
		ratespec |= VHT_RSPEC(0, FTM_NSS);
		break;
	default:
		ASSERT(FALSE);
		err = BCME_UNSUPPORTED;
		break;
	}

done:
	*rspec =  ratespec;
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d, cspec 0x%04x ci 0x%02x "
		"rx_rspec 0x%08x ratespec 0x%08x\n", FTM_UNIT(ftm), __FUNCTION__, err,
		cspec, chaninfo, rx_rspec, ratespec)));
	return err;
}

#if defined(BCMDBG) || defined(FTM_DONGLE_DEBUG)
static char dbg_buf[FTM_PKT_DUMP_BUFSZ];
#endif /* BCMDBG || FTM_DONGLE_DEBUG */

static int
ftm_proto_partial_tsf_to_local(pdftm_t *ftm, pdftm_session_t *sn,
    uint16 r_partial_tsf, uint64 *out_tsf)
{
	uint64 sn_tsf = 0;
	uint64 cur_tsf;
	uint64 local_tsf = 0;
	uint16 l_partial_tsf;
	int err = BCME_OK;
	uint32 delta = 0;

	err = pdftm_get_session_tsf(sn, &sn_tsf);
	if (err != BCME_OK)
		goto done;

	cur_tsf = sn_tsf;

	l_partial_tsf = (uint16)((sn_tsf & FTM_PARAMS_PARTIAL_TSF_MASK) >>
		FTM_PARAMS_PARTIAL_TSF_SHIFT);
	sn_tsf &= ~FTM_PARAMS_PARTIAL_TSF_MASK;
	sn_tsf |= (uint64)FTM_TU2MICRO(r_partial_tsf);

	/* adjust session tsf for partial tsf rollover */
	if (r_partial_tsf <  l_partial_tsf)
		sn_tsf += FTM_TSF_64KTU;

	delta = (uint32)(sn_tsf - cur_tsf);
	if (delta > FTM_PARAMS_TSF_FW_HI && delta < FTM_PARAMS_TSF_BW_LOW) {
		err = WL_PROXD_E_BAD_PARTIAL_TSF;
		goto done;
	} else if (delta >= FTM_PARAMS_TSF_BW_LOW && delta <= FTM_PARAMS_TSF_BW_HI) {
		sn_tsf -= FTM_TSF_64KTU;
	} /* else valid future */

	local_tsf = pdftm_convert_tsf(sn, sn_tsf, FALSE);
	*out_tsf = local_tsf;
done:
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d session idx %d "
		"r_partial_tsf %d delta %d session tsf %u.%u, local tsf %u.%u\n",
		FTM_UNIT(ftm), __FUNCTION__, err, sn->idx, r_partial_tsf, delta,
		FTM_LOG_TSF_ARG(sn_tsf), FTM_LOG_TSF_ARG(local_tsf))));
	return err;
}

static int
ftm_proto_session_config_from_params(pdftm_t *ftm, wlc_bsscfg_t *rx_bsscfg,
	const dot11_management_header_t *hdr, const wlc_d11rxhdr_t *wrxh,
	const dot11_ftm_params_t *params, ratespec_t rx_rspec, pdftm_session_t *sn)
{
	pdftm_session_config_t *sncfg;
	uint8 chaninfo;
	int err = BCME_OK;
	chanspec_t chanspec = 0;
	ratespec_t ratespec;
	uint8 asap;
	uint16 req_sep, min_sep, sep;
	uint16 num_ftm, req_num_ftm;
	uint32 dur_usec, req_dur;
	uint8 burst_tmo;
	int ret_err = BCME_OK;

	sncfg = sn->config;
	ASSERT(!(sncfg->flags & WL_PROXD_SESSION_FLAG_INITIATOR));

	/* note: peer mac is used to resolve chanspec, later (ordering) */
	sncfg->burst_config->peer_mac = hdr->sa;
	sncfg->burst_config->cur_ether_addr = hdr->da;
	sncfg->burst_config->bssid = hdr->bssid;
	sncfg->flags |= WL_PROXD_SESSION_FLAG_TARGET;
	sncfg->burst_config->chanspec =
		D11RXHDR_ACCESS_VAL(&wrxh->rxhdr, ftm->wlc->pub->corerev, RxChan);
	sncfg->burst_config->ratespec = rx_rspec;

	if (ltoh16_ua(&hdr->fc) & FC_WEP) {
		sncfg->flags |= WL_PROXD_SESSION_FLAG_SECURE;
	}

	if (!params) {
		/* Rxed Request without parameter, create a temp session to terminate initiator */
		sn->flags |= FTM_SESSION_FTM1_SENT;
		goto done;
	}

	if (FTM_PARAMS_FTM1(params))
		sncfg->flags |= WL_PROXD_SESSION_FLAG_TS1;

	chaninfo = FTM_PARAMS_CHAN_INFO(params);
	do {
		uint8 sn_chaninfo;
		chanspec_t rx_cspec =
			D11RXHDR_ACCESS_VAL(&wrxh->rxhdr, ftm->wlc->pub->corerev, RxChan);

		err = ftm_proto_resolve_chanspec(ftm, sn, rx_cspec, chaninfo, &chanspec);
		if (err != BCME_OK)
			break;

		sn->config->burst_config->chanspec = chanspec;
		err = ftm_proto_resolve_ratespec(ftm, chanspec, chaninfo, rx_rspec, &ratespec);
		if (err != BCME_OK)
			break;

		sn->config->burst_config->ratespec = ratespec;
		err = pdftm_validate_ratespec(ftm, sn);
		if (err != BCME_OK)
			break;
		sn_chaninfo = ftm_proto_get_chaninfo(ftm, sn);
		if (chaninfo != sn_chaninfo)
			sn->flags |= FTM_SESSION_PARAM_OVERRIDE;
	} while (0);

	if (err != BCME_OK) {
		ret_err = WL_PROXD_E_INCAPABLE;
		FTM_LOGPROTO(ftm, (("wl%d: %s: status %d, ret %d, "
			"chanspec/ratespec validation failed\n",
			FTM_UNIT(ftm), __FUNCTION__, err, ret_err)));
	}

	/* inherit tx power */

	/* set initial delay based of partial tsf; scheduler adjusts for chansw time */
	asap = FTM_PARAMS_ASAP(params);
	if (asap && (ftm->ftm_cmn->config->flags & WL_PROXD_FLAG_ASAP_CAPABLE)) {
		FTM_INIT_INTVL(&sncfg->init_delay, 0, WL_PROXD_TMU_MILLI_SEC);
		sncfg->flags |= WL_PROXD_SESSION_FLAG_ASAP;
	} else {
		uint64 local_tsf, cur_tsf;

		FTM_GET_TSF(ftm, cur_tsf);
		if (FTM_PARAMS_PTSFNOPREF(params)) {
			local_tsf = cur_tsf;	/* no initiator preference */
		} else {
			err = ftm_proto_partial_tsf_to_local(ftm, sn,
				FTM_PARAMS_PARTIAL_TSF(params), &local_tsf);
			if (err != BCME_OK) {
				ret_err = err;
				goto do_sep;
			}
		}

		if (local_tsf < cur_tsf)
			local_tsf = cur_tsf;

		FTM_INIT_INTVL(&sncfg->init_delay, local_tsf - cur_tsf, WL_PROXD_TMU_MICRO_SEC);
		sncfg->flags &= ~WL_PROXD_SESSION_FLAG_ASAP;
	}

	/* ftm sep of 0 indicates no preference from initiator; resolve it, also
	 * enforce a minimum separation
	 */
do_sep:
	req_sep = FTM_PARAMS_MINDELTA_USEC(params);
	min_sep = pdftm_resolve_ftm_sep(ftm, sncfg->flags, chanspec);
	sep = MAX(req_sep, min_sep);
	sep = ROUNDUP(sep, 100);
	FTM_INIT_INTVL(&sncfg->burst_config->ftm_sep, sep, WL_PROXD_TMU_MICRO_SEC);

	if (FTM_PARAMS_BURST_PERIOD_MS(params)) {
		/* if initiator specified burst_period */
		FTM_INIT_INTVL(&sncfg->burst_period, FTM_PARAMS_BURST_PERIOD_MS(params),
			WL_PROXD_TMU_MILLI_SEC);
	}

	req_num_ftm = num_ftm = FTM_PARAMS_FTMS_PER_BURST(params);
	if (!req_num_ftm)
		num_ftm = pdftm_resolve_num_ftm(ftm, sncfg->flags, chanspec);
	else
		num_ftm = req_num_ftm;
	ASSERT(num_ftm != 0);
	sncfg->burst_config->num_ftm = num_ftm;

	/* burst duration > 128ms - no preference. Allow for number of ftms, with
	 * some buffer for two retries
	 */
	burst_tmo = FTM_PARAMS_BURSTTMO(params);
	burst_tmo = FTM_PARAMS_BURSTTMO_VALID(burst_tmo) ? burst_tmo :
		FTM_PARAMS_BURSTTMO_NOPREF;
	req_dur = FTM_PARAMS_BURSTTMO_USEC(burst_tmo);
	FTM_INIT_INTVL(&sncfg->burst_config->duration, req_dur,
		WL_PROXD_TMU_MICRO_SEC);
	FTM_SESSION_INIT_BURST_DURATION_US(dur_usec, num_ftm, sep,
		sncfg->burst_config->ftm_retries);
	if (burst_tmo != FTM_PARAMS_BURSTTMO_NOPREF) {
		/* Change duration only when requested duration < needed duration and
		 * requested num_ftm indicates a preference
		 */
		if ((req_num_ftm != 0) && (dur_usec > req_dur) &&
			(dur_usec <= FTM_PARAMS_BURSTTMO_MAX_USEC)) {
			FTM_INIT_INTVL(&sncfg->burst_config->duration, dur_usec,
				WL_PROXD_TMU_MICRO_SEC);
		}
	} else {
		FTM_INIT_INTVL(&sncfg->burst_config->duration, dur_usec, WL_PROXD_TMU_MICRO_SEC);
	}

	/* Trigger frame should be rx to the AP in the first 50% of the burst instance */
	if (!asap) {
		uint32 timeout_local = FTM_INTVL2USEC(&sncfg->burst_config->timeout);
		uint32 timeout_thresh = dur_usec/2;
		FTM_INIT_INTVL(&sncfg->burst_config->timeout,
			(MAX(timeout_local, timeout_thresh)),
			WL_PROXD_TMU_MICRO_SEC);
	}

	/* num burts - 2^15 is no preference */
	sncfg->num_burst = FTM_PARAMS_NBURST(params);
	if (sncfg->num_burst == FTM_NUM_BURST_NO_PREFERENCE)
		sncfg->num_burst = FTM_SESSION_RX_DEFAULT_NUM_BURST;
	else if (sncfg->num_burst > ftm->ftm_cmn->config->rx_max_burst) {
		sn->flags |= FTM_SESSION_PARAM_OVERRIDE;
		sncfg->num_burst = ftm->ftm_cmn->config->rx_max_burst;
	}

	/* leave retries at default */
done:

#if defined(BCMDBG) || defined(FTM_DONGLE_DEBUG)
	/* dump out the received config */
	{
		struct bcmstrbuf b;
		bcm_binit(&b, dbg_buf, sizeof(dbg_buf));
		bzero(dbg_buf, sizeof(dbg_buf));
		pdftm_dump_session_config(sn->sid, sncfg, &b);
		dbg_buf[sizeof(dbg_buf) - 1] = 0;
		FTM_LOGPROTO(ftm, (("wl%d: %s: session config:\n%s\n",
			FTM_UNIT(ftm), __FUNCTION__, dbg_buf)));
	}
#endif /* BCMDBG || FTM_DONGLE_DEBUG */

	return ret_err;
}

static void
ftm_proto_update_rx_cnt(pdftm_t *ftm, pdftm_session_t *sn, int err)
{
	FTM_CNT(ftm->ftm_cmn, sn, rx);

	switch (err) {
	case BCME_OK:
		break;
	case WL_PROXD_E_PROTO:
		FTM_CNT(ftm->ftm_cmn, sn, protoerr);
		break;
	case WL_PROXD_E_POLICY: /* fall through */
	case WL_PROXD_E_SCHED_FAIL:
		FTM_CNT(ftm->ftm_cmn, sn, sched_fail);
		break;
	default:
		break;
	}
}

static void
ftm_proto_update_tx_cnt(pdftm_t *ftm, pdftm_session_t *sn, int err)
{
	switch (err) {
	case BCME_OK:
		FTM_CNT(ftm->ftm_cmn, sn, tx);
		break;
	case WL_PROXD_E_NOACK:
		FTM_CNT(ftm->ftm_cmn, sn, noack);
		break;
	case WL_PROXD_E_TIMEOUT:
		FTM_CNT(ftm->ftm_cmn, sn, timeouts);
		break;
	case WL_PROXD_E_PROTO:
		FTM_CNT(ftm->ftm_cmn, sn, protoerr);
		break;
	default:
		break;
	}

	if (err != BCME_OK) {
		FTM_CNT(ftm->ftm_cmn, sn, txfail);
	}
}

static int
ftm_proto_new_session(pdftm_t *ftm, wlc_bsscfg_t *bsscfg,
	const dot11_management_header_t *hdr, const wlc_d11rxhdr_t *wrxh,
	const dot11_ftm_params_t *params, ratespec_t rspec,
	const uint8 *body, uint body_len,
	dot11_rmreq_ftm_lci_t **lci_req,
	dot11_rmreq_ftm_civic_t **civic_req,
	pdftm_session_t **out_sn)
{
	bool created;
	int err = BCME_OK;
	pdftm_session_t *sn;

	ASSERT(out_sn && !*out_sn);

	/* check if rx is enabled (for new sessions) */
	if (!(ftm->ftm_cmn->config->flags & WL_PROXD_FLAG_RX_ENABLED)) {
		err = BCME_DISABLED;
		goto done;
	}

	err = pdftm_alloc_session(ftm, bsscfg, WL_PROXD_SESSION_ID_GLOBAL, &sn, &created);
	if (err != BCME_OK)
		goto done;

	ASSERT(sn && created);
	*out_sn = sn; /* caller is expected cleanup on errors - if any */

	/* note: session is reclaimed on all configuration errors */
	sn->flags |= FTM_SESSION_DELETE_ON_STOP;
	sn->config->flags |= WL_PROXD_SESSION_FLAG_AUTO_BURST;

	/* configure the session */
	err = ftm_proto_session_config_from_params(ftm, bsscfg, hdr,
		wrxh, params, rspec, sn);
	if (err != BCME_OK)
		goto done;

	if (!params) {
		/* no need to notify the initiator session to terminate */
		err = WL_PROXD_E_PROTO;
		goto done;
	}
#ifdef WL_FTM_11K
	if (*lci_req != NULL) {
		if (sn->lci_req != NULL) {
			MFREE(FTM_OSH(ftm), sn->lci_req, BCM_TLV_SIZE(sn->lci_req));
		}
		sn->lci_req = *lci_req;
		*lci_req = NULL;
		sn->flags |= FTM_SESSION_LCI_REQ_RCVD;
		FTM_CNT(ftm->ftm_cmn, sn, lci_req_rx);
	}

	if (*civic_req != NULL) {
		if (sn->civic_req != NULL) {
			MFREE(FTM_OSH(ftm), sn->civic_req, BCM_TLV_SIZE(sn->civic_req));
		}
		sn->civic_req = *civic_req;
		*civic_req = NULL;
		sn->flags |= FTM_SESSION_CIVIC_REQ_RCVD;
		FTM_CNT(ftm->ftm_cmn, sn, civic_req_rx);
	}
#endif /* WL_FTM_11K */
	err = pdftm_change_session_state(sn, BCME_OK, WL_PROXD_SESSION_STATE_CONFIGURED);
	if (err != BCME_OK)
		goto done;

	err = pdftm_start_session(sn);
	if (err != BCME_OK)
		goto done;

	/* we are target, set up the burst */
	err = pdftm_setup_burst(ftm, sn, body, body_len, wrxh, rspec);
	if (err != BCME_OK)
		goto done;

	/* for non-asap we need to return a status response, that is not part
	 * of the first measurement exchange. that is done once burst start is
	 * computed when transitioning into delay state (see pdftm_proto_notify).
	 * we also need to wait for another trigger for non-asap.
	 */
	if (!FTM_SESSION_IS_ASAP(sn))
		sn->ftm_state->flags |= FTM_SESSION_STATE_NEED_TRIGGER;

done:
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d\n",
		FTM_UNIT(ftm), __FUNCTION__, err)));
	return err;
}

static int
ftm_proto_handle_req(pdftm_t *ftm, wlc_bsscfg_t *bsscfg, const dot11_management_header_t *hdr,
	const uint8 *body, uint body_len, const wlc_d11rxhdr_t *wrxh, ratespec_t rspec)
{
	const dot11_ftm_req_t *req;
	uint req_len;
	int err = BCME_OK;
	uint16 fc;
	const dot11_ftm_params_t *params = NULL;
	dot11_rmreq_ftm_lci_t *lci_req = NULL;
	dot11_rmreq_ftm_civic_t *civic_req = NULL;
	const bcm_tlv_t *tlv;
	pdftm_session_t *sn = NULL;

	FTM_LOGPROTO(ftm, (("wl%d.%d: %s: handling request from " MACF "\n",
		FTM_UNIT(ftm), WLC_BSSCFG_IDX(bsscfg), __FUNCTION__,
		ETHERP_TO_MACF(&hdr->sa))));

	if (body_len < sizeof(*req)) {
		err = BCME_BUFTOOSHORT;
		goto done;
	}

	if (!FTM_BSSCFG_FTM_RX_ENABLED(ftm->ftm_cmn, bsscfg)) {
		err = BCME_DISABLED;
		goto done;
	}

	fc = ltoh16(hdr->fc);
	if (((fc & FC_WEP) && !WLC_BSSCFG_SECURE_FTM(bsscfg)) ||
		(!(fc & FC_WEP) && WLC_BSSCFG_SECURE_FTM(bsscfg))) {
		goto done;
	}

	req = (const dot11_ftm_req_t *)body;
	req_len = body_len;

	body += sizeof(*req);
	body_len -= sizeof(*req);

#ifdef WL_FTM_11K
	/* find (optional) lci meas req */
	tlv = ftm_proto_find_loc_req(ftm, DOT11_MEASURE_TYPE_LCI,
			&body, &body_len);
	if (tlv && !(lci_req = (dot11_rmreq_ftm_lci_t *)pdftm_tlvdup(ftm, tlv))) {
		err = BCME_NOMEM;
		goto done;
	}

	/* find (optional) civic meas req */
	tlv = ftm_proto_find_loc_req(ftm, DOT11_MEASURE_TYPE_CIVICLOC,
			&body, &body_len);
	if (tlv && !(civic_req = (dot11_rmreq_ftm_civic_t *)pdftm_tlvdup(ftm, tlv))) {
		err = BCME_NOMEM;
		goto done;
	}
#endif /* WL_FTM_11K */
	/* parse tlvs of interest - ftm param */
	tlv = bcm_parse_tlvs((const uint8 *)(body), body_len, DOT11_MNG_FTM_PARAMS_ID);
	if (tlv) {
		uint tlv_size = TLV_HDR_LEN + tlv->len;
		if (tlv_size < sizeof(*params)) {
			err = WL_PROXD_E_PROTO;
			goto done;
		}

		params = (const dot11_ftm_params_t *)tlv;
		body_len = FTM_BODY_LEN_AFTER_TLV(body, body_len, tlv, tlv_size);
		body = (const uint8 *)tlv + tlv_size;
	}

	/* body and body_len now specify data transparent to us */
	sn = FTM_SESSION_FOR_PEER(ftm, &hdr->sa, WL_PROXD_SESSION_FLAG_TARGET);
	if (sn != NULL) {
		/* check security policy - note: needed even for unsecure session */
		err = pdftm_sec_check_rx_policy(ftm, sn, hdr);
		if (err != BCME_OK)
			goto done;

		/* cancellation when trigger is 0 or with params */
		if ((req->trigger == DOT11_PUB_ACTION_FTM_REQ_TRIGGER_STOP) || (params != NULL)) {
			FTM_LOGPROTO(ftm,
				(("wl%d.%d: %s: canceling sn for " MACF
				", trig=%d; params %spresent\n",
				FTM_UNIT(ftm), WLC_BSSCFG_IDX(bsscfg), __FUNCTION__,
				ETHERP_TO_MACF(&hdr->sa), req->trigger,
				((params != NULL) ? "" : "not "))));
			if (sn->config->flags & WL_PROXD_SESSION_FLAG_AUTO_BURST) {
				sn->flags |= FTM_SESSION_DELETE_ON_STOP;
			}
			(void)pdftm_change_session_state(sn, WL_PROXD_E_REMOTE_CANCEL,
					WL_PROXD_SESSION_STATE_STOPPING);
			if (req->trigger == DOT11_PUB_ACTION_FTM_REQ_TRIGGER_STOP) {
				goto done;
			}

			/* lose this session when params are specified */
			if (!(sn->flags & FTM_SESSION_DELETE_ON_STOP)) {
				pdftm_notify_info_t notify_info;
				uint corerev = ftm->wlc->pub->corerev;
				chanspec_t rx_cspec =
					D11RXHDR_ACCESS_VAL(&wrxh->rxhdr, corerev, RxChan);
				FTM_INIT_SESSION_NOTIFY_INFO(&notify_info, NULL, &hdr->sa,
					WL_PROXD_EVENT_FTM_REQ, rx_cspec,
					rspec, (const uint8 *)req, req_len);
				pdftm_notify(ftm, sn->bsscfg, PDFTM_NOTIF_EVENT_TYPE, &notify_info);
				goto done;
			}

			err = pdftm_sched_process_session_end(ftm, sn->idx, sn->idx);
			sn = NULL;
		}

		if (params == NULL) { /* continue session */
			/* in non-auto burst case, setup burst - so we don't need to store req */
			if (!(sn->config->flags & WL_PROXD_SESSION_FLAG_RX_AUTO_BURST)) {
				err = pdftm_setup_burst(ftm, sn, body, body_len, wrxh, rspec);
				if (err == BCME_OK) {
					pdftm_notify_info_t notify_info;
					FTM_INIT_SESSION_NOTIFY_INFO(&notify_info, NULL,
						&hdr->sa, WL_PROXD_EVENT_FTM_REQ, 0, 0, NULL, 0);
					pdftm_notify(ftm, sn->bsscfg,
						PDFTM_NOTIF_EVENT_TYPE, &notify_info);
				}
				goto done;
			}

			/* next burst (in delay) or continue (in burst) */
			switch (sn->state) {
			case WL_PROXD_SESSION_STATE_DELAY:
				ASSERT(sn->ftm_state);
				FTM_GET_TSF(ftm, sn->ftm_state->delay_exp);
				sn->ftm_state->flags &= ~FTM_SESSION_STATE_NEED_TRIGGER;
				err = pdftm_sched_session(ftm, sn);
				if (err != BCME_OK)
					break;
				/* fall through */
			case WL_PROXD_SESSION_STATE_BURST:
				err = pdftm_setup_burst(ftm, sn, body, body_len, wrxh, rspec);
				break;
			default:
				err = WL_PROXD_E_SCHED_FAIL;
				(void)pdftm_change_session_state(sn, err,
					WL_PROXD_SESSION_STATE_STOPPING);
				break;
			}

			if (err != BCME_OK)
				goto done;
		} else {
			if (!(ftm->ftm_cmn->config->flags &  WL_PROXD_FLAG_RX_AUTO_BURST)) {
				pdftm_notify_info_t notify_info;
				uint corerev = ftm->wlc->pub->corerev;
				chanspec_t rx_cspec =
					D11RXHDR_ACCESS_VAL(&wrxh->rxhdr, corerev, RxChan);
				FTM_INIT_SESSION_NOTIFY_INFO(&notify_info, NULL /* new session */,
					&hdr->sa, WL_PROXD_EVENT_FTM_REQ, rx_cspec,
					rspec, (const uint8 *)req, req_len);
				pdftm_notify(ftm, bsscfg, PDFTM_NOTIF_EVENT_TYPE, &notify_info);
				goto done;
			}
		}
	}

	if (!sn) {
		if (req->trigger != DOT11_PUB_ACTION_FTM_REQ_TRIGGER_START) {
			err = WL_PROXD_E_PROTO;
			goto done;
		}

		if (params) {
			pdftm_notify_info_t notify_info;
			uint corerev = ftm->wlc->pub->corerev;
			chanspec_t rx_cspec =
					D11RXHDR_ACCESS_VAL(&wrxh->rxhdr, corerev, RxChan);
			if (!(ftm->ftm_cmn->config->flags & WL_PROXD_FLAG_RX_AUTO_BURST)) {
				FTM_INIT_SESSION_NOTIFY_INFO(&notify_info, NULL /* new session */,
					&hdr->sa, WL_PROXD_EVENT_FTM_REQ, rx_cspec,
					rspec, (const uint8 *)req, req_len);
				pdftm_notify(ftm, bsscfg, PDFTM_NOTIF_EVENT_TYPE, &notify_info);
				goto done;
			}

			err = ftm_proto_new_session(ftm, bsscfg, hdr, wrxh, params, rspec,
				body, body_len, &lci_req, &civic_req, &sn);
			FTM_INIT_SESSION_NOTIFY_INFO(&notify_info, sn /* new session */,
				&hdr->sa, WL_PROXD_EVENT_FTM_REQ, rx_cspec,
				rspec, (const uint8 *)req, req_len);
			pdftm_notify(ftm, bsscfg, PDFTM_NOTIF_EVENT_TYPE, &notify_info);
			if (err != BCME_OK)
				goto done;

			/* check security policy - note: needed even for unsecure session */
			err = pdftm_sec_check_rx_policy(ftm, sn, hdr);
			if (err != BCME_OK)
				goto done;
		} else {
			err = WL_PROXD_E_PROTO;
			goto done;
		}
	}

#ifdef WL_FTM_TSF_SYNC
	/* store tsf for this trigger to return to initiator */
	ASSERT((sn != NULL) && (sn->ftm_state != NULL));
	if (FTM_TSF_SYNC_SESSION(sn)) {
		FTM_GET_TSF(ftm, sn->ftm_state->trig_tsf);
		sn->ftm_state->flags |= FTM_SESSION_STATE_TSF_SYNC;	/* send sync */
		ASSERT(sn->ftm_state->trig_delta == 0); /* not used on target */
	} else {
		sn->ftm_state->trig_tsf = (pdftm_time_t)0;
		sn->ftm_state->flags &= ~FTM_SESSION_STATE_TSF_SYNC;
	}
#endif /* WL_FTM_TSF_SYNC */

done:
	ftm_proto_update_rx_cnt(ftm, sn, err);
	if (err != BCME_OK && sn) {
		(void)pdftm_stop_session(sn, err);
	}
#ifdef WL_FTM_11K
	if (lci_req) {
		MFREE(FTM_OSH(ftm), lci_req, (lci_req->len + TLV_HDR_LEN));
	}
	if (civic_req) {
		MFREE(FTM_OSH(ftm), civic_req, (civic_req->len + TLV_HDR_LEN));
	}
#endif /* WL_FTM_11K */
	return err;
}

/* handle overrides from responder/target */
static int
ftm_proto_handle_overrides(pdftm_t *ftm, pdftm_session_t *sn,
	wlc_bsscfg_t *bsscfg, const dot11_ftm_params_t *params,
	const wlc_d11rxhdr_t *wrxh, ratespec_t rspec)
{
	int err = BCME_OK;
	pdftm_session_config_t *sncfg;
	bool ovrdok;
	ftm_param_ovrd_t ovrd;
	uint16 num_burst;
	uint32 ftm_sep;
	uint32 dur;
	uint8 asap;
	uint8 burst_tmo;

	sncfg = sn->config;
	ovrd = FTM_PARAM_OVRD_NONE;
	ovrdok = (sncfg->flags & WL_PROXD_SESSION_FLAG_NO_PARAM_OVRD) == 0;

	/* 11mc 4.0 removed override indication - check each param for override */
	do {
		/* only asap->non-asap allowed */
		asap = FTM_PARAMS_ASAP(params);
		if (FTM_SESSION_IS_ASAP(sn)) {
			if (!asap) {
				ovrd |= FTM_PARAM_OVRD_ASAP;
				if (!ovrdok) {
					break;
				}
				sncfg->flags &= ~WL_PROXD_SESSION_FLAG_ASAP;
			}
		} else {
			if (asap) {
				err = WL_PROXD_E_PROTO;
				goto done;
			}
		}

		/* Allow chanspec to be overridden on non-ap if associated */
		if (FTM_INITIATOR_ALLOW_CIOVRD(ftm, bsscfg)) {
			chanspec_t cspec;
			uint8 ci;
			uint corerev = ftm->wlc->pub->corerev;

			ci =  FTM_PARAMS_CHAN_INFO(params);
			err = ftm_proto_resolve_chanspec(ftm, sn,
				D11RXHDR_ACCESS_VAL(&wrxh->rxhdr, corerev, RxChan), ci, &cspec);

			if (err != BCME_OK) {
				goto done;
			}
			if (cspec != sncfg->burst_config->chanspec) {
				ovrd |= FTM_PARAM_OVRD_CHANSPEC;
				if (!ovrdok) {
					break;
				}
				sncfg->burst_config->chanspec = cspec;
			}

			err = ftm_proto_resolve_ratespec(ftm,
				sncfg->burst_config->chanspec,
				ci, rspec, &sncfg->burst_config->ratespec);
			if (err != BCME_OK) {
				goto done;
			}

			err = pdftm_validate_ratespec(ftm, sn);
			if (err != BCME_OK) {
				goto done;
			}
		}

			/* min-delta must be greater than what is configured */
		ftm_sep = FTM_PARAMS_MINDELTA_USEC(params);
		if (ftm_sep < FTM_INTVL2USEC(&sncfg->burst_config->ftm_sep)) {
			err = WL_PROXD_E_PROTO;
			goto done;
		} else if (ftm_sep !=
				FTM_INTVL2USEC(&sncfg->burst_config->ftm_sep)) {
			ovrd |= FTM_PARAM_OVRD_FTM_SEP;
			if (!ovrdok) {
				break;
			}
			FTM_INIT_INTVL(&sncfg->burst_config->ftm_sep, ftm_sep,
				WL_PROXD_TMU_MICRO_SEC);
		}

		/* adopt duration (if allowed) */
		burst_tmo = FTM_PARAMS_BURSTTMO(params);
		burst_tmo = FTM_PARAMS_BURSTTMO_VALID(burst_tmo) ? burst_tmo :
			FTM_PARAMS_BURSTTMO_NOPREF;
		dur = FTM_PARAMS_BURSTTMO_USEC(burst_tmo);
		if (FTM_INITIATOR_ALLOW_DUROVRD(ftm, bsscfg, asap)) {
			if (dur != FTM_INTVL2USEC(&sncfg->burst_config->duration)) {
				ovrd |= FTM_PARAM_OVRD_BURST_DURATION;
				if (!ovrdok) {
					break;
				}
				FTM_INIT_INTVL(&sncfg->burst_config->duration,
					dur > FTM_PARAMS_BURSTTMO_MAX_USEC ?
					FTM_SESSION_RX_DEFAULT_BURST_TIMEOUT_USEC : dur,
					WL_PROXD_TMU_MICRO_SEC);
			}
		}

		/* adopt burst period */
		if (FTM_INITIATOR_ALLOW_BPEROVRD(ftm, bsscfg)) {
			int bp;
			bp = FTM_PARAMS_BURST_PERIOD_MS(params);
			if (bp != (int)FTM_INTVL2MSEC(&sncfg->burst_period)) {
				ovrd |= FTM_PARAM_OVRD_BURST_PERIOD;
				if (!ovrdok) {
					break;
				}
				FTM_INIT_INTVL(&sncfg->burst_period, bp, WL_PROXD_TMU_MILLI_SEC);
			}
		}

		/* adopt num ftm; burst may be terminated before this if duration does
		 * not allow it
		 */
		{
			int num_ftm;
			num_ftm = FTM_PARAMS_FTMS_PER_BURST(params);
			if (num_ftm != sncfg->burst_config->num_ftm) {
				ovrd |= FTM_PARAM_OVRD_NUM_FTM;
				if (!ovrdok) {
					break;
				}
				sncfg->burst_config->num_ftm = num_ftm;
			}
		}

		/* adjust number of bursts */
		num_burst = FTM_PARAMS_NBURST(params);
		if (FTM_INITIATOR_ALLOW_NBURSTOVRD(ftm, bsscfg,
				num_burst, sncfg->num_burst)) {
			if (num_burst !=  sncfg->num_burst) {
				ovrd |= FTM_PARAM_OVRD_NUM_BURST;
				if (!ovrdok) {
					break;
				}
				sncfg->num_burst = num_burst;
			}
		}
	} while (0);

	/* check if override is allowed - schedule can be overridden for !asap */
	if (!ovrdok && ovrd) {
		err = WL_PROXD_E_OVERRIDDEN;
		goto done;
	} else if (ovrd) {
		sn->flags |= FTM_SESSION_PARAM_OVERRIDE;
	}

done:
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d override 0x%08x\n",
		FTM_UNIT(ftm), __FUNCTION__, err, ovrd)));
	return err;
}

static int
ftm_proto_init_tsi(pdftm_t *ftm, pdftm_session_t *sn,
	const dot11_ftm_t *meas, pdburst_tsinfo_t *tsi)
{
	int err = BCME_OK;
	pdftm_session_state_t *sst;

	/* for non-follow up, tsi fields other than ids are not meaningful */
	bzero(tsi, sizeof(*tsi));
	tsi->tx_id = meas->dialog;
	tsi->ts_id = meas->follow_up;
	if (!meas->follow_up)
		goto done;

	FTM_PROTO_TS2PICO(sn, meas->tod, tsi->tod);
	FTM_PROTO_TS2PICO(sn, meas->toa, tsi->toa);

	/* not doing anything special if timestamps are not contiguous */
	(void)DOT11_FTM_ERR_NOT_CONT(meas->tod_err);
	(void)DOT11_FTM_ERR_NOT_CONT(meas->toa_err);

	tsi->tod_err = FTM_HZ2PICO(sn, DOT11_FTM_ERR_MAX_ERR(meas->tod_err));
	tsi->toa_err = FTM_HZ2PICO(sn, DOT11_FTM_ERR_MAX_ERR(meas->toa_err));

done:
	sst = sn->ftm_state;

	/* 8.4.21.18 11mcD4.0 - this needs to be within 32us of tx, but
	 * this is the best we can do - tbd multi-burst, retries etc.
	 */
	if ((sst->meas_start < sst->burst_start) &&
		(sst->rtt_status ==  WL_PROXD_E_NOTSTARTED)) {
		uint64 cur_tsf;
		FTM_GET_TSF(sn->ftm, cur_tsf);
		sst->meas_start = cur_tsf;
	}

	return err;
}

static int
ftm_proto_process_meas_tlvs(pdftm_t *ftm, pdftm_session_t *sn,
	const uint8 **out_body, uint *out_body_len, const dot11_ftm_params_t **params)
{
	const uint8 *body;
	uint body_len;
	int err = BCME_OK;
	bcm_tlv_t *tlv;
	dot11_ftm_sync_info_t *tsf_si = NULL;

	(void)BCM_REFERENCE(tsf_si);

	/* note: ordering of the tlvs is ascending by element id, except for vendor ie as
	 * specified for the frame in 802.11 spec (> 11mc D5.2)
	 */
	body = *out_body;
	body_len = *out_body_len;

#ifdef WL_FTM_11K
	/* Find (optional) LCI/civic meas report */
	err = ftm_proto_find_lci_civic_rep(ftm, sn, DOT11_MEASURE_TYPE_LCI, &body, &body_len);
	if (err != BCME_OK) {
		goto done;
	}

	err = ftm_proto_find_lci_civic_rep(ftm, sn, DOT11_MEASURE_TYPE_CIVICLOC, &body, &body_len);
	if (err != BCME_OK) {
		goto done;
	}
#endif /* WL_FTM_11K */

	/* parse tlvs of interest - ftm param */
	tlv = bcm_parse_tlvs((const uint8 *)body, body_len, DOT11_MNG_FTM_PARAMS_ID);
	if (tlv) {
		uint tlv_size = TLV_HDR_LEN + tlv->len;
		if (tlv_size < sizeof(**params)) {
			err = WL_PROXD_E_PROTO;
			goto done;
		}

		if (tlv_size > body_len) {
			err = WL_PROXD_E_PROTO;
			goto done;
		}
		*params = (const dot11_ftm_params_t *)tlv;
		body_len = FTM_BODY_LEN_AFTER_TLV(body, body_len, tlv, tlv_size);
		body = (uint8 *)tlv + tlv_size;
	}

#ifdef WL_FTM_TSF_SYNC
	/* search for sync info - WFA interop: allow ie ids that aren't ascending */
	tlv =  bcm_parse_tlvs_dot11((const uint8 *)body, body_len,
			DOT11_MNG_FTM_SYNC_INFO_ID, TRUE);
	if (!tlv) {
		tlv = bcm_parse_tlvs_dot11((const void *)*out_body, body - *out_body,
			DOT11_MNG_FTM_SYNC_INFO_ID, TRUE);
	}
	if (tlv) {
		uint tlv_size = TLV_HDR_LEN + tlv->len;
		if (tlv_size < sizeof(*tsf_si)) {
			err = WL_PROXD_E_PROTO;
			goto done;
		}

		if (tlv_size > body_len) {
			err = WL_PROXD_E_PROTO;
			goto done;
		}

		tsf_si = (dot11_ftm_sync_info_t *)tlv;
		err = pdftm_session_trig_tsf_update(sn, tsf_si);
		if (err != BCME_OK) {
			goto done;
		}

		/* don't adjust body/body_len if we found the tlv before the body */
		if ((uint8 *)tlv >= body) {
			body_len = FTM_BODY_LEN_AFTER_TLV(body, body_len, tlv, tlv_size);
			body = (uint8 *)tlv + tlv_size;
		}
	}
#endif /* WL_FTM_TSF_SYNC */

done:
	if (err == BCME_OK) {
		*out_body =  body;
		*out_body_len = body_len;
	}

	return err;
}

static int
ftm_proto_handle_meas(pdftm_t *ftm, wlc_bsscfg_t *bsscfg, const dot11_management_header_t *hdr,
	const uint8 *body, uint body_len, const wlc_d11rxhdr_t *wrxh, ratespec_t rspec)
{
	const dot11_ftm_t *meas;
	int err = BCME_OK;
	uint16 fc;
	pdftm_session_t *sn = NULL;
	const dot11_ftm_params_t *params = NULL;
	pdburst_tsinfo_t tsi;

	FTM_LOGPROTO(ftm, (("wl%d.%d: %s: handling measurement from " MACF "\n",
		FTM_UNIT(ftm), WLC_BSSCFG_IDX(bsscfg), __FUNCTION__,
		ETHERP_TO_MACF(&hdr->sa))));

	meas = (const dot11_ftm_t *)body;
	if (body_len < sizeof(*meas)) {
		err = BCME_BUFTOOSHORT;
		goto done;
	}

	fc = ltoh16(hdr->fc);
	if (((fc & FC_WEP) && !WLC_BSSCFG_SECURE_FTM(bsscfg)) ||
		(!(fc & FC_WEP) && WLC_BSSCFG_SECURE_FTM(bsscfg))) {
		goto done;
	}

	if (!(sn = FTM_SESSION_FOR_PEER(ftm, &hdr->sa, WL_PROXD_SESSION_FLAG_INITIATOR))) {
		err = WL_PROXD_E_PROTO; /* not found, but count as protocol error */
		goto done;
	}

	/* check security policy - note: needed even for unsecure session */
	err = pdftm_sec_check_rx_policy(ftm, sn, hdr);
	if (err != BCME_OK)
		goto done;

	if (sn->state != WL_PROXD_SESSION_STATE_BURST) {
		FTM_LOGPROTO(ftm, (("wl%d: %s: cannot handle measurement in state %d\n",
			FTM_UNIT(ftm), __FUNCTION__, sn->state)));
		err = WL_PROXD_E_PROTO; /* invalid state */
		goto done;
	}

	ASSERT(sn->ftm_state->burst != NULL);

	/* ignore measurement frames for one-way rtt */
	if ((sn->config->flags & WL_PROXD_SESSION_FLAG_ONE_WAY)) {
		FTM_LOGPROTO(ftm, (("wl%d: %s: ignoring ftm meas frame for 1-way sn idx %d.\n",
			FTM_UNIT(ftm), __FUNCTION__, sn->idx)));
		goto done;
	}

	body += sizeof(*meas);
	body_len -= sizeof(*meas);

	err = ftm_proto_process_meas_tlvs(ftm, sn, &body, &body_len, &params);
	if (err != BCME_OK)
		goto done;

	err = ftm_proto_init_tsi(ftm, sn, meas, &tsi);
	if (err != BCME_OK)
		goto done;

	sn->ftm_state->dialog = meas->dialog;
	if (!params && !meas->dialog && !meas->follow_up) {
		/* meas w/ dialog id 0 is a termination */
		err = WL_PROXD_E_REMOTE_CANCEL;
		goto done;
	}

	if (params != NULL) {
		uint8 asap;

		/* only the first measurement has params with status */
		if (sn->flags & FTM_SESSION_FTM1_RCVD) {
			/* Rxed retransmission of the initial measurement pkt,
			 * process with the packet and ignore the parameter.
			 */
			goto param_ok;
		}

		switch (FTM_PARAMS_STATUS(params)) {
		case FTM_PARAMS_STATUS_SUCCESSFUL:
			break; /* some params may have changed */
		case FTM_PARAMS_STATUS_INCAPABLE:
			err = WL_PROXD_E_REMOTE_INCAPABLE;
			break;
		case FTM_PARAMS_STATUS_FAILED:
			FTM_INIT_INTVL(&sn->ftm_state->retry_after,  FTM_PARAMS_VALUE(params),
				WL_PROXD_TMU_SEC);
			err = WL_PROXD_E_REMOTE_FAIL;
			break;
		default:
			err = WL_PROXD_E_PROTO;
			break;
		}

		if (err != BCME_OK)
			goto done;

		if (!meas->dialog) {
			err = WL_PROXD_E_REMOTE_CANCEL; /* E_PROTO ? */
			goto done;
		}

		err = ftm_proto_handle_overrides(ftm, sn, bsscfg, params, wrxh, rspec);
		if (err != BCME_OK)
			goto done;

		asap = FTM_SESSION_IS_ASAP(sn);

		/* for non-asap, check if need to reschedule the burst. otherwise update
		 * burst parameters and continue the burst
		 */
		if (!asap) {
			pdftm_expiration_t exp;
			uint64 cur_tsf;

			err = ftm_proto_partial_tsf_to_local(ftm, sn,
				FTM_PARAMS_PARTIAL_TSF(params), &exp);
			if (err != BCME_OK)
				goto done;

			/* disallow preponing the burst, otherwise reschedule if necessary */
			if ((exp + FTM_SESSION_RESCHED_BACKOFF_USEC) < sn->ftm_state->burst_start) {
				err = WL_PROXD_E_PROTO;
				goto done;
			}

			ASSERT(sn->ftm_state->burst != NULL);
			err = pdburst_suspend(sn->ftm_state->burst);
			if (err != BCME_OK)
				goto done;

			/* if we are too close, reschedule the burst using the scheduler.
			 * Otherwise restart the burst.
			 */
			FTM_GET_TSF(ftm, cur_tsf);
			if ((cur_tsf + FTM_SESSION_RESCHED_BACKOFF_USEC) <= exp) {
				sn->ftm_state->delay_exp =  exp;
				if (sn->ftm_state->delay_exp < cur_tsf)
					sn->ftm_state->delay_exp = cur_tsf;

				err = pdftm_change_session_state(sn, BCME_OK,
					WL_PROXD_SESSION_STATE_DELAY);
				if (err != BCME_OK)
					goto done;
			}
		}

		/* update overrides */
		err = pdftm_setup_burst(ftm, sn, body, body_len, wrxh, rspec);
		if (err != BCME_OK)
			goto done;

		/* restart the burst for non-asap, so a fresh trigger will be sent.
		 * if the session is in delay state, burst will be restarted on
		 * transition to the burst state
		 */
		if (!asap && (sn->state == WL_PROXD_SESSION_STATE_BURST)) {
			err = pdburst_start(sn->ftm_state->burst);
			/* done, as burst has no use for this meas frame */
			goto done;
		}
	} else {
		if (!(sn->flags & FTM_SESSION_FTM1_RCVD)) {
			err = WL_PROXD_E_PROTO;
			goto done;
		}
	}

param_ok:
	err = pdburst_rx(sn->ftm_state->burst, bsscfg, hdr, body, body_len, wrxh, rspec, &tsi);
	if (err != BCME_OK)
		goto done;

	/* we received ftm1. note: this is done after pdburst receives the frame */
	sn->flags |= FTM_SESSION_FTM1_RCVD;

	/* rx can cause state change. done if no longer in burst - error/stop or end */
	if (sn->state != WL_PROXD_SESSION_STATE_BURST)
		goto done;

done:
	ftm_proto_update_rx_cnt(ftm, sn, err);
	if (err != BCME_OK && sn != NULL) {
		if ((sn->state != WL_PROXD_SESSION_STATE_BURST) ||
			(err == WL_PROXD_E_REMOTE_CANCEL)) {
			(void)pdftm_stop_session(sn, err);
		} else {
			if (sn->ftm_state != NULL) {
				pdburst_cancel(sn->ftm_state->burst, err);
			}
		}
	}
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: err %d sn->state %d \n",
		FTM_UNIT(ftm), __FUNCTION__, err, sn ? sn->state : 0)));
	return err;
}

static int
ftm_proto_fill_ftm(pdftm_t *ftm, pdftm_session_t *sn,
	const pdburst_tsinfo_t *tsi, int req_err,
	uint8 *body, uint body_len)
{
	dot11_ftm_t *rsp;
	int err = BCME_OK;

	ASSERT(body_len >= sizeof(*rsp));

	rsp = (dot11_ftm_t *)body;

#define FTM_TOD_NOT_CONT 1

	if (tsi != NULL) {

		rsp->dialog = tsi->tx_id;
		rsp->follow_up = tsi->ts_id;

		FTM_PROTO_PICO2TS(sn, rsp->tod, tsi->tod);
		FTM_PROTO_PICO2TS(sn, rsp->toa, tsi->toa);
		if (rsp->follow_up) {
			FTM_PROTO_PICO2TSERR(sn, rsp->tod_err, tsi->tod_err);
			FTM_PROTO_PICO2TSERR(sn, rsp->toa_err, tsi->toa_err);
			DOT11_FTM_ERR_SET_NOT_CONT(rsp->tod_err, FTM_TOD_NOT_CONT);
			DOT11_FTM_ERR_SET_NOT_CONT(rsp->toa_err, 0); /* toa not cont is reserved */
		}
		else {
			bzero(&rsp->tod_err, sizeof(rsp->tod_err));
			bzero(&rsp->toa_err, sizeof(rsp->toa_err));
		}

		if (sn->ftm_state) {
			pdftm_session_state_t *sst = sn->ftm_state;
			sst->dialog = rsp->dialog;

			/* indicate measurement start needs update if we have valid timestamps
			 * and the burst has started. Update happens is tx done callback
			 */
			if ((sst->meas_start < sst->burst_start) &&
				(tsi->tod != (pdburst_ts_t)0 || tsi->toa != (pdburst_ts_t)0)) {
				sst->flags |= FTM_SESSION_STATE_UPD_MEAS_START;
			}
		}
	} else {
		bzero(&rsp->dialog, sizeof(dot11_ftm_t) - OFFSETOF(dot11_ftm_t, dialog));
	}

	body += sizeof(*rsp);
	body_len -= sizeof(*rsp);

	/* insert lci, civic, params and tsf sync - in that order note: changed post WFA PF3 */
	if (!(sn->flags & FTM_SESSION_FTM1_SENT)) {
		dot11_ftm_params_t *params;

		if (sn->ftm_state && (req_err != BCME_OK ||
				!(sn->config->flags & WL_PROXD_SESSION_FLAG_ASAP))) {
			sn->ftm_state->dialog++;
			if (!sn->ftm_state->dialog)
				sn->ftm_state->dialog = 1;
			rsp->dialog = sn->ftm_state->dialog;
		}
#ifdef WL_FTM_11K
		ftm_proto_fill_lci(ftm, sn, &body, &body_len);
		ftm_proto_fill_civic(ftm, sn, &body, &body_len);
#endif /* WL_FTM_11K */

		ASSERT(body_len >= sizeof(*params));

		params = (dot11_ftm_params_t *)body;
		bzero(params, sizeof(*params));
		params->id = DOT11_MNG_FTM_PARAMS_ID;
		params->len = DOT11_FTM_PARAMS_IE_LEN;
		err = ftm_proto_get_ftm1_params(ftm, sn, req_err, params);
		body += sizeof(*params);
		body_len -= sizeof(*params);
	}

#ifdef WL_FTM_TSF_SYNC
	if (FTM_TSF_SYNC_SESSION(sn) && FTM_SESSION_SEND_TSF_SYNC(sn)) {
		dot11_ftm_sync_info_t *tsf_si;
		pdftm_time_t sn_tsf;
		uint32 tsf_si_val;

		sn_tsf = pdftm_convert_tsf(sn, sn->ftm_state->trig_tsf, TRUE);
		tsf_si_val = (uint32)(sn_tsf & 0xffffffffULL);

		tsf_si = (dot11_ftm_sync_info_t *)body;
		DOT11_MNG_IE_ID_EXT_INIT(tsf_si, DOT11_MNG_FTM_SYNC_INFO_ID,
			DOT11_FTM_SYNC_INFO_IE_LEN);
		htol32_ua_store(tsf_si_val, tsf_si->tsf_sync_info);

		FTM_LOGPROTO(sn->ftm, (("wl%d: %s: trig tsf %u.%u sn_tsf %u.%u sync tsf 0x%08x "
			"ie %d.%d.%d.%02x%02x%02x%02x\n",
			FTM_UNIT(sn->ftm), __FUNCTION__,
			FTM_LOG_TSF_ARG(sn->ftm_state->trig_tsf),
			FTM_LOG_TSF_ARG(sn_tsf), tsf_si_val, tsf_si->id,
			tsf_si->len, tsf_si->id_ext,
			tsf_si->tsf_sync_info[0], tsf_si->tsf_sync_info[1],
			tsf_si->tsf_sync_info[2], tsf_si->tsf_sync_info[3])));

		body += sizeof(*tsf_si);
		body_len -= sizeof(*tsf_si);
	}
#endif /* WL_FTM_TSF_SYNC */

	return err;
}

static void
ftm_proto_req_cancel(pdftm_t *ftm, pdftm_session_t *sn)
{
	void *pkt;
	uint body_len;
	uint8 *body;
	dot11_ftm_req_t *req;
	int err = BCME_OK;

	body_len = sizeof(dot11_ftm_req_t);
	pkt = proxd_alloc_action_frame(ftm->pd, sn->bsscfg,
		&sn->config->burst_config->peer_mac,
		&sn->config->burst_config->cur_ether_addr,
		&sn->config->burst_config->bssid,
		body_len, &body, FTM_SESSION_ACTION_CAT(sn), DOT11_PUB_ACTION_FTM_REQ);
	if (!pkt) {
		err = BCME_NOMEM;
		goto done;
	}

	req = (dot11_ftm_req_t *)body;
	req->trigger = DOT11_PUB_ACTION_FTM_REQ_TRIGGER_STOP;
	if (!proxd_tx(ftm->pd, pkt, sn->bsscfg,
		sn->config->burst_config->ratespec, sn->status)) {
		err = BCME_TXFAIL;
		goto done;
	}

done:
	BCM_REFERENCE(err);
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d for session idx %d\n",
		FTM_UNIT(ftm), __FUNCTION__, err, sn->idx)));
}

/* burst callback support */
static int
ftm_proto_get_frame_len(pdburst_method_ctx_t ctx, pdburst_frame_type_t type,
	uint16 *dot11_fc_kind)
{
	uint len = 0;
	pdftm_session_t *sn = (pdftm_session_t *)ctx;

	ASSERT(sn != NULL);
	ASSERT(dot11_fc_kind != NULL);

	*dot11_fc_kind = FC_ACTION;
	if (type == PDBURST_FRAME_TYPE_REQ) {
		ASSERT(sn->config->flags & WL_PROXD_SESSION_FLAG_INITIATOR);
		len += sizeof(dot11_ftm_req_t);
		if (!(sn->flags & FTM_SESSION_FTM1_SENT))
			len += sizeof(dot11_ftm_params_t);

	} else if (type == PDBURST_FRAME_TYPE_MEAS) {
		/* note: measurement frames are used on target or initiator where
		 * it is one-way, or auto/two-way is attempted but tried one-way due
		 * to lack of resp.
		 */
		len += sizeof(dot11_ftm_t);

#ifdef WL_FTM_TSF_SYNC
		if (FTM_TSF_SYNC_SESSION(sn) && FTM_SESSION_SEND_TSF_SYNC(sn)) {
			len += sizeof(dot11_ftm_sync_info_t);
		}
#endif /* WL_FTM_TSF_SYNC */

		if (!(sn->flags & FTM_SESSION_FTM1_SENT))
			len += sizeof(dot11_ftm_params_t);
	}

#ifdef WL_FTM_11K
	len += ftm_lci_civic_len(sn, type);
#endif /* WL_FTM_11K */

	return len;
}

/* callback notes:
	status values 1 (success), 2 (overridden) during tx callback for
	measurement frames
 */

static int
ftm_proto_fill_req(pdftm_t *ftm, pdftm_session_t *sn,
	uint8 *body, uint body_len)
{
	int err = BCME_OK;
	dot11_ftm_req_t *req;
	dot11_ftm_params_t *params;

	ASSERT(body_len >= sizeof(*req));

	req = (dot11_ftm_req_t *)body;

	req->trigger = DOT11_PUB_ACTION_FTM_REQ_TRIGGER_START;
	if (sn->flags & FTM_SESSION_FTM1_SENT)
		goto done;

	body += sizeof(*req);
	body_len -= sizeof(*req);

#ifdef WL_FTM_11K
	ftm_proto_fill_lci_req(sn, &body, &body_len);
	ftm_proto_fill_civic_req(sn, &body, &body_len);
#endif /* WL_FTM_11K */

	ASSERT(body_len >= sizeof(*params));
	params = (dot11_ftm_params_t *)body;
	bzero(params, sizeof(*params));
	params->id = DOT11_MNG_FTM_PARAMS_ID;
	params->len = DOT11_FTM_PARAMS_IE_LEN;
	err = ftm_proto_get_ftm1_params(ftm, sn, BCME_OK, params);

done:
	return err;
}

static int
ftm_proto_prep_tx(pdburst_method_ctx_t ctx, pdburst_frame_type_t type, void *pkt, uint8 *body,
    uint body_max, uint *body_len, const pdburst_tsinfo_t *tsinfo)
{
	int err = BCME_OK;
	pdftm_session_t *sn = (pdftm_session_t *)ctx;
	dot11_action_frmhdr_t *afhdr;
	uint len;

	ASSERT(FTM_VALID_SESSION(sn));

	ASSERT(body_len != NULL);
	ASSERT(sn->state == WL_PROXD_SESSION_STATE_BURST);

	len = 0;
	if (body_max < DOT11_ACTION_FRMHDR_LEN) {
		err = BCME_BUFTOOSHORT;
		goto done;
	}

	afhdr = (dot11_action_frmhdr_t *)body;
	afhdr->category = FTM_SESSION_ACTION_CAT(sn);

	if (type == PDBURST_FRAME_TYPE_REQ) {
		afhdr->action = DOT11_PUB_ACTION_FTM_REQ;
		len += sizeof(dot11_ftm_req_t);
		if (!(sn->flags & FTM_SESSION_FTM1_SENT)) {
			len += sizeof(dot11_ftm_params_t);
#ifdef WL_FTM_11K
			len += ftm_lci_civic_len(sn, type);
#endif /* WL_FTM_11K */
		}

		if (body_max < len) {
			err = BCME_BUFTOOSHORT;
			goto done;
		}

		err = ftm_proto_fill_req(sn->ftm, sn, body, len);
	} else if (type == PDBURST_FRAME_TYPE_MEAS) {
		afhdr->action = DOT11_PUB_ACTION_FTM;
		len += sizeof(dot11_ftm_t);
		if (!(sn->flags & FTM_SESSION_FTM1_SENT))
			len += sizeof(dot11_ftm_params_t);

#ifdef WL_FTM_11K
		len += ftm_lci_civic_len(sn, type);
#endif /* WL_FTM_11K */

#ifdef WL_FTM_TSF_SYNC
		if (FTM_TSF_SYNC_SESSION(sn) && FTM_SESSION_SEND_TSF_SYNC(sn)) {
			len += sizeof(dot11_ftm_sync_info_t);
		}
#endif /* WL_FTM_TSF_SYNC */

		if (body_max < len) {
			err = BCME_BUFTOOSHORT;
			goto done;
		}

		err = ftm_proto_fill_ftm(sn->ftm, sn, tsinfo, BCME_OK, body, len);
	}
	else {
		/* Invalid frame type */
		err = WL_PROXD_E_PROTO;
		ASSERT(0);
	}

done:
	ftm_proto_update_tx_cnt(sn->ftm, sn, err);
	*body_len = (err == BCME_OK) ? len : 0;
	FTM_LOG_STATUS(sn->ftm, err, (("wl%d: %s: status %d for session idx %d\n",
		FTM_UNIT(sn->ftm), __FUNCTION__, err, sn->idx)));
	return err;
}

static int
ftm_proto_tx_done(pdburst_method_ctx_t ctx, wl_proxd_status_t status)
{
	pdftm_session_t *sn = (pdftm_session_t *)ctx;
	uint64 cur_tsf = 0;
	pdftm_session_state_t *sst;

	ASSERT(FTM_VALID_SESSION(sn));

	FTM_GET_TSF(sn->ftm, cur_tsf);

	/* always update mesaurement start if required. Clear the flag only after
	 * first successful measurement.
	 */
	sst = sn->ftm_state;
	if (sst && (sst->flags & FTM_SESSION_STATE_UPD_MEAS_START)) {
		sst->meas_start = cur_tsf;
	}

	if (status == BCME_OK) {
#ifdef WL_FTM_TSF_SYNC
		if (FTM_TSF_SYNC_SESSION(sn) && sst) {
			if (FTM_SESSION_IS_INITIATOR(sn)) {
				sst->trig_tsf = cur_tsf;
				sst->trig_delta = 0;
				sst->flags |= FTM_SESSION_STATE_TSF_SYNC; /* need sync */
			} else {
				sn->ftm_state->flags &= ~FTM_SESSION_STATE_TSF_SYNC;
			}
		}
#endif /* WL_FTM_TSF_SYNC */
		sn->flags |= FTM_SESSION_FTM1_SENT;
		if (sst != NULL)
			sst->flags &= ~FTM_SESSION_STATE_UPD_MEAS_START;

		goto done;
	}

	if (sst != NULL)
		sst->rtt_status = status;

	/* note: tx counter updated when tx starts, here we update failures */
	ftm_proto_update_tx_cnt(sn->ftm, sn, status);

done:
	FTM_LOGPROTO(sn->ftm, (("wl%d: %s: status %d for session idx %d "
		"sst flags x%02x at tsf %u.%u\n", FTM_UNIT(sn->ftm), __FUNCTION__,
		status, sn->idx,  sst ? sst->flags : 0, FTM_LOG_TSF_ARG(cur_tsf))));
	return BCME_OK;
}

static int
ftm_proto_meas_done(pdburst_method_ctx_t ctx, const wl_proxd_rtt_sample_t *rtt)
{
	int err = BCME_OK;
	pdftm_session_t *sn = (pdftm_session_t *)ctx;
	pdftm_session_state_t *sst;
	wl_proxd_rtt_sample_t *sst_rtt;
	pdftm_t *ftm;

	ASSERT(FTM_VALID_SESSION(sn));
	ASSERT(sn->state == WL_PROXD_SESSION_STATE_BURST);
	ASSERT(rtt != NULL);
	ASSERT(sn->config->flags & WL_PROXD_SESSION_FLAG_INITIATOR);

	ftm = sn->ftm;
	sst = sn->ftm_state;

	if (!rtt->id) {
		FTM_LOGPROTO(ftm, (("wl%d: %s: invalid id %d  for sample\n",
			FTM_UNIT(ftm), __FUNCTION__, rtt->id)));
		goto done;
	}

	/* allocate rtt if needed */
	if (!sst->rtt) {
		sst->max_rtt = sn->config->burst_config->num_ftm;
		sst->rtt = MALLOCZ(FTM_OSH(ftm), sizeof(*sst->rtt) * sst->max_rtt);
		if (!sst->rtt) {
			err = BCME_NOMEM;
			goto done;
		}
	}

	/* accept first sample */
	if (!sst->num_rtt && (sst->rtt[0].id == 0)) {
		sst->rtt[0] = *rtt;
		sst->num_rtt++;
		goto done;
	}

	ASSERT(sst->num_rtt > 0);
	sst_rtt = &sst->rtt[sst->num_rtt -1];

	/* replace sample if id is same */
	if (sst_rtt->id == rtt->id) {
		*sst_rtt = *rtt;
		goto done;
	}

	if (sst->num_rtt >= sst->max_rtt) {
		err = BCME_NORESOURCE;
		goto done;
	}

	*(++sst_rtt) = *rtt;
	sst->num_rtt++;

done:
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d for session idx %d num rtt %d\n",
		FTM_UNIT(ftm), __FUNCTION__, err, sn->idx, sst->num_rtt)));
	return err;
}

static int
ftm_proto_burst_done(pdburst_method_ctx_t ctx, const pdburst_results_t *res)
{
	int err = BCME_OK;
	pdftm_session_t *sn = (pdftm_session_t *)ctx;
	pdftm_session_state_t *sst = NULL;
	int i;
	wl_proxd_session_state_t new_state;

	ASSERT(FTM_VALID_SESSION(sn));
	ASSERT(sn->state == WL_PROXD_SESSION_STATE_BURST);

	new_state = sn->state;
	sst = sn->ftm_state;

	if (sst == NULL) {
		err = BCME_ERROR;
		ASSERT(0);
		goto ret;
	}

	sst->rtt_status = res ? res->status : BCME_OK;

	/* no results for target */
	if (sn->config->flags & WL_PROXD_SESSION_FLAG_TARGET)
		goto done;

	ASSERT(res != NULL);

	if (res != NULL) {
		FTM_CNT_INCR(sn->ftm, sn, num_meas, res->num_meas);
		sst->num_meas = res->num_meas;
		sst->result_flags = res->flags;

		if (res->flags & WL_PROXD_RESULT_FLAG_FATAL) {
			ASSERT(res->status != BCME_OK);
			err = res->status;
			goto done;
		}

		sst->avg_rtt = res->avg_rtt;
		sst->num_valid_rtt = res->num_valid_rtt;
		sst->sd_rtt = res->sd_rtt;
		FTM_SET_DIST(res->avg_dist, res->dist_unit, sst->avg_dist);

		for (i = 0; i < (int)res->num_rtt; ++i) {
			err = ftm_proto_meas_done((pdburst_method_ctx_t)sn, &res->rtt[i]);
			if (err != BCME_OK)
				break;
		}
	}

	/* transmit a FTM vendor-specific initiator report to target if applicable */
	if (sn->config->flags & WL_PROXD_SESSION_FLAG_INITIATOR_RPT) {
		err = pdftm_vs_tx_initiator_rpt(sn->ftm, sn);
	}

done:
	sst->flags |= FTM_SESSION_STATE_BURST_DONE;
	new_state = sn->state;
	if (err != BCME_OK) {
		new_state = WL_PROXD_SESSION_STATE_STOPPING;
	} else {
		wl_proxd_session_flags_t flags = sn->config->flags;

		/* keep burst if canceled to be rescheduled. Otherwise
		 * end the session if all bursts or done - if not continue
		 * either in delay (auto mode) or user_wait state
		 */
		if (sst->rtt_status == WL_PROXD_E_CANCELED) {
			sst->flags &= ~FTM_SESSION_STATE_BURST_DONE;
		} else if (sst->burst_num >= sn->config->num_burst) {
			new_state = WL_PROXD_SESSION_STATE_ENDED;
		} else if (((flags & WL_PROXD_SESSION_FLAG_INITIATOR) &&
			(flags & WL_PROXD_SESSION_FLAG_TX_AUTO_BURST)) ||
			((flags & WL_PROXD_SESSION_FLAG_TARGET) &&
			(flags & WL_PROXD_SESSION_FLAG_RX_AUTO_BURST))) {
			new_state =  WL_PROXD_SESSION_STATE_DELAY;
#ifdef WL_FTM_MSCH
			pdftm_msch_end(sn->ftm, sn);
#endif /* WL_FTM_MSCH */
		} else {
			new_state =  WL_PROXD_SESSION_STATE_USER_WAIT;
		}
	}

	if (new_state != sn->state)
		pdftm_change_session_state(sn, err, new_state);

ret:
	FTM_LOG_STATUS(sn->ftm, err,
		(("wl%d: %s: status %d rtt_status %d for session idx %d\n",
		FTM_UNIT(sn->ftm), __FUNCTION__, err,
		res ? res->status : BCME_OK, sn->idx)));
	return err;
}

static int
ftm_proto_get_session_info(pdburst_method_ctx_t ctx, pdburst_session_info_t *infop)
{
	pdftm_session_t *sn = (pdftm_session_t *)ctx;
	pdftm_session_state_t *sst;
	pdftm_session_config_t *sncfg;
	int err = BCME_OK;

	ASSERT(FTM_VALID_SESSION(sn));
	ASSERT(infop != NULL);

	sst = sn->ftm_state;
	sncfg = sn->config;
	if (!sst || !sncfg) {
		err = BCME_NOTFOUND;
		goto done;
	}

	bzero(infop, sizeof(*infop));
	if (sst->burst_num >= sncfg->num_burst)
		infop->flags = PDBURST_SESSION_FLAGS_LAST_BURST;
	if (sn->ftm->ftm_cmn->config->flags & WL_PROXD_FLAG_MBURST_FOLLOWUP)
		infop->flags |= PDBURST_SESSION_FLAGS_MBURST_FOLLOWUP;
	if (FTM_SESSION_SEQ_EN(sn))
		infop->flags |= PDBURST_SESSION_FLAGS_SEQ_EN;
	if (FTM_SESSION_IS_VHTACK(sn))
		infop->flags |= PDBURST_SESSION_FLAGS_VHTACK;
	if (FTM_SESSION_SECURE(sn))
		infop->flags |= PDBURST_SESSION_FLAGS_SECURE;

done:
	return err;
}

static int
ftm_proto_set_session_info(pdburst_method_ctx_t ctx, const pdburst_session_info_t *infop)
{
	pdftm_session_t *sn = (pdftm_session_t *)ctx;
	pdftm_session_state_t *sst;
	pdftm_session_config_t *sncfg;
	int err = BCME_OK;

	ASSERT(FTM_VALID_SESSION(sn));
	ASSERT(infop != NULL);

	sst = sn->ftm_state;
	sncfg = sn->config;
	if (!sst || !sncfg)
		goto done;

	if (infop->flags & PDBURST_SESSION_FLAGS_SEQ_EN)
		sncfg->flags |= WL_PROXD_SESSION_FLAG_SEQ_EN;
	else
		sncfg->flags &= ~WL_PROXD_SESSION_FLAG_SEQ_EN;

	if (infop->flags & PDBURST_SESSION_FLAGS_VHTACK)
		sncfg->flags |= WL_PROXD_SESSION_FLAG_VHTACK;
	else
		sncfg->flags &= ~WL_PROXD_SESSION_FLAG_VHTACK;

	if (infop->flags & PDBURST_SESSION_FLAGS_SECURE)
		sncfg->flags |= WL_PROXD_SESSION_FLAG_SECURE;
	else
		sncfg->flags &= ~WL_PROXD_SESSION_FLAG_SECURE;

	/* no vs attributes returned here */

done:
	return err;
}

/* external interface */
bool
pdftm_is_ftm_action(pdftm_t *ftm, const dot11_management_header_t *hdr,
	uint8 *body, uint body_len)
{
	bool is_ftm;

	ASSERT(FTM_VALID(ftm));
	ASSERT(body != NULL);

	ASSERT((ltoh16(hdr->fc) & FC_KIND_MASK) == FC_ACTION);

	is_ftm =  body_len >= DOT11_ACTION_HDR_LEN &&
		(body[DOT11_ACTION_CAT_OFF] == DOT11_ACTION_CAT_PUBLIC ||
		body[DOT11_ACTION_CAT_OFF] == DOT11_ACTION_CAT_PDPA) &&
		(body[DOT11_ACTION_ACT_OFF] == DOT11_PUB_ACTION_FTM_REQ ||
		body[DOT11_ACTION_ACT_OFF] == DOT11_PUB_ACTION_FTM);
	return is_ftm;
}

int
pdftm_rx(pdftm_t *ftm, wlc_bsscfg_t *bsscfg, const dot11_management_header_t *hdr,
	const uint8 *body, uint body_len, const wlc_d11rxhdr_t *wrxh, ratespec_t rspec)
{
	uint8 action = 0;
	int err;

	ASSERT(bsscfg != NULL);
	if (body_len < DOT11_ACTION_HDR_LEN) {
		err = BCME_BUFTOOSHORT;
		goto done;
	}

#if defined(FTM_DONGLE_DEBUG)
	{
		struct bcmstrbuf b;
		bzero(dbg_buf, sizeof(dbg_buf));
		bcm_binit(&b, dbg_buf, sizeof(dbg_buf));
		pdftm_dump_pkt_body(ftm, body, body_len, &b);
		dbg_buf[sizeof(dbg_buf) - 1] = 0;
		FTM_LOGPKT(ftm, (("%s\n", dbg_buf)));
	}
#endif // endif

	action = body[DOT11_ACTION_ACT_OFF];
	switch (action) {
	case DOT11_PUB_ACTION_FTM_REQ:
		err = ftm_proto_handle_req(ftm, bsscfg, hdr, body, body_len, wrxh, rspec);
		break;
	case DOT11_PUB_ACTION_FTM:
		err = ftm_proto_handle_meas(ftm, bsscfg, hdr, body, body_len, wrxh, rspec);
		break;
	default:
		err = BCME_UNSUPPORTED;
		break;
	}

done:
	BCM_REFERENCE(err);
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d for action with body length %d\n",
		FTM_UNIT(ftm), __FUNCTION__, err, body_len)));
	/* we handled the errors if any, say okay */
	return BCME_OK;
}

/* retry FTM_1 packet */
static void ftm_proto_ftm1_tx_timer(void *ctx)
{
	pdftm_session_t *sn = (pdftm_session_t *)ctx;

	if (sn) {
		(void)pdftm_tx_ftm(sn->ftm, sn, NULL, sn->status);
	}
}

static void ftm_proto_ftm1_tx_complete(wlc_info_t *wlc, uint txstatus, void *arg)
{
	pdftm_session_t *sn = (pdftm_session_t *)arg;
	wl_proxd_status_t status = BCME_OK;

	proxd_undeaf_phy(wlc, (txstatus & TX_STATUS_ACK_RCV));

	sn->flags &= ~FTM_SESSION_TX_PENDING;

	/* handle any destroy while tx of pkt is pending */
	if (sn->state == WL_PROXD_SESSION_STATE_DESTROYING) {
		(void)pdftm_change_session_state(sn, sn->status, WL_PROXD_SESSION_STATE_NONE);
		return;
	}

	status = (txstatus & TX_STATUS_ACK_RCV) ? BCME_OK : WL_PROXD_E_NOACK;
	if (!sn->ftm1_tx_timer)
		goto done;

	if (!(txstatus & TX_STATUS_ACK_RCV)) {
		FTM_LOG(sn->ftm, (("wl%d: %s Lost FTM1 ack txstatus %x\n",
			FTM_UNIT(sn->ftm), __FUNCTION__, txstatus)));
		if (++sn->ftm1_retrycnt <= sn->config->burst_config->ftm_retries) {
			/* start timer to retry */
			wlc_hrt_add_timeout(sn->ftm1_tx_timer,
			FTM_INTVL2USEC(&sn->config->burst_config->ftm_sep),
			ftm_proto_ftm1_tx_timer, (void *)sn);
		} else {
			/* too many retries failed */
			wlc_hrt_del_timeout(sn->ftm1_tx_timer);
			pdftm_stop_session(sn, WL_PROXD_E_NOACK);
		}
	} else {
		sn->ftm1_retrycnt = 0;
		wlc_hrt_del_timeout(sn->ftm1_tx_timer);
	}

done:
	ftm_proto_tx_done(sn, status);
}

int
pdftm_tx_ftm(pdftm_t *ftm, pdftm_session_t *sn, const pdburst_tsinfo_t *tsi, int req_err)
{
	void *pkt;
	uint body_len;
	uint8 *body;
	int err = BCME_OK;
	uint16 fc_kind;
	wlc_pkttag_t *pkttag;

	body_len = ftm_proto_get_frame_len((void *)sn, PDBURST_FRAME_TYPE_MEAS, &fc_kind);
	pkt = proxd_alloc_action_frame(ftm->pd, sn->bsscfg,
		&sn->config->burst_config->peer_mac,
		&sn->config->burst_config->cur_ether_addr,
		&sn->config->burst_config->bssid,
		body_len, &body, FTM_SESSION_ACTION_CAT(sn), DOT11_PUB_ACTION_FTM);
	if (!pkt) {
		err = BCME_NOMEM;
		goto done;
	}

	err = ftm_proto_fill_ftm(ftm, sn, tsi, req_err, body, body_len);
	if (err != BCME_OK) {
		PKTFREE(FTM_OSH(ftm), pkt, FALSE);
		goto done;
	}

	/* disable FTM1 hardware retry */
	wlc_pcb_fn_register(ftm->wlc->pcb, ftm_proto_ftm1_tx_complete, sn, pkt);
	pkttag = WLPKTTAG(pkt);
	pkttag->shared.packetid = sn->config->burst_config->chanspec;
	pkttag->shared.packetid |= (PROXD_FTM_PACKET_TAG | PROXD_MEASUREMENT_PKTID);

	if (!proxd_tx(ftm->pd, pkt, sn->bsscfg, sn->config->burst_config->ratespec, req_err)) {
		err = BCME_TXFAIL;
		goto done;
	}
	sn->flags |= FTM_SESSION_TX_PENDING;

done:
	ftm_proto_update_tx_cnt(ftm, sn, err);
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d for session idx %d "
		"body_len %d req err %d\n", FTM_UNIT(ftm), __FUNCTION__, err, sn->idx,
		body_len, req_err)));
	return err;
}

void
pdftm_proto_notify(pdftm_t *ftm, pdftm_session_t *sn, wl_proxd_event_type_t event_type)
{
	pdftm_session_config_t *sncfg;
	int status;
	bool dotx = FALSE;

	ASSERT(FTM_VALID_SESSION(sn));
	sncfg = sn->config;
	status = sn->status;

	if ((event_type == WL_PROXD_EVENT_SESSION_END) &&
		(status == WL_PROXD_E_INCAPABLE || status == WL_PROXD_E_CANCELED)) {
		/* notify peer if session ended via local cancel if session onchan */
		if (pdftm_session_is_onchan(sn)) {
			if (FTM_SESSION_IS_INITIATOR(sn) &&
				(sn->flags & FTM_SESSION_FTM1_SENT)) {
				ftm_proto_req_cancel(ftm, sn);
			} else if (FTM_SESSION_IS_TARGET(sn)) {
				dotx = TRUE;
			}
		}
	} else if ((event_type == WL_PROXD_EVENT_DELAY) && FTM_SESSION_IS_TARGET(sn)) {
		/* on target report errors, delays etc */
		bool asap;
		do {
			/* no support for status reporting once target sends ftm1 */
			if (sn->flags & FTM_SESSION_FTM1_SENT)
				break;

			asap = FTM_SESSION_IS_ASAP(sn);
			if (asap) {
				pdftm_expiration_t delay;
				delay = sn->ftm_state->burst_start - sn->ftm_state->delay_exp;
				if (delay > FTM_INTVL2USEC(&sncfg->burst_config->timeout)) {
					status = WL_PROXD_E_SCHED_FAIL;
					FTM_CNT(ftm->ftm_cmn, sn, sched_fail);
				}
			} else {
				uint64 cur_tsf;
				FTM_GET_TSF(ftm, cur_tsf);

				/* we can only indicate one partial tsf rollover */
				if ((cur_tsf + FTM_PARAMS_TSF_FW_HI)
						< sn->ftm_state->burst_start) {
					status = WL_PROXD_E_SCHED_FAIL;
					FTM_CNT(ftm->ftm_cmn, sn, sched_fail);
				}
			}

			if (status != BCME_OK || !asap) {
				dotx = TRUE;
				break;
			}
		} while (0);
	}

	if (dotx)
		status = pdftm_tx_ftm(ftm, sn, NULL, status);

	if (status != BCME_OK && status != sn->status)
		(void)pdftm_stop_session(sn, status);
}

const pdburst_callbacks_t pdftm_burst_callbacks = {
	ftm_proto_get_frame_len,
	ftm_proto_prep_tx,
	ftm_proto_tx_done,
	NULL,
	ftm_proto_burst_done,
	ftm_proto_get_session_info,
	ftm_proto_set_session_info,
	pdftm_vs_get_frame_len,
	pdftm_vs_prep_tx,
	pdftm_vs_rx
};
