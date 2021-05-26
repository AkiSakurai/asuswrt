/*
 * wlc_ifstats.c for per interface stats collection
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
 * $Id: $
 */

/* Some design decisions:
 * 1. Keep the infrastructure as simple as possible.
 * 2. Infra_sta and soft AP are handled in the infrastructure although
 * they are "actually" clients of the infrastructure.
 * 3. Don't go into managing clients reportable stats types.
 * Leave it to clients on how to report their stats.
 * 4. Give clients enough tools to formulate their stats and report them.
 * Often times, clients need help in formulating reportable structure.
 *
 */

#include <wlc_cfg.h>

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <802.11.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_bmac.h>
#include <wlc_assoc.h>
#include <wlc_ifstats.h>
#include <bcmtlv.h>
#include <wlc_sta.h>
#include <wlc_ap.h>

/* Structure for infra_sta and soft AP stats reporting. Passed to the
 * XTLV callback function.
 */
typedef struct {
	wlc_info_t *wlc;
	struct wlc_if *wlcif;
	wlc_bsscfg_t *bsscfg;
	bcm_xtlvbuf_t *input_xtlv_context;
} wl_ifstats_sta_ap_xtlv_cb_ctx_t;

/* called when parameter xtlvs are parsed */
static int wl_ifstats_sta_ap_xtlv_cbfn(void *ctx, const uint8 *data,
	uint16 type, uint16 len);

/* FOr infra and STA only */
int
wlc_ifstats_infra_cnt_incr(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	int32 offset)
{
	int rc = BCME_OK;
	wl_if_infra_stats_t *infra_stats;

	if (!bsscfg)
		return BCME_UNSUPPORTED;

	//if ((AP_ENAB(wlc->pub)) && (BSSCFG_AP(bsscfg))) {
	if (BSSCFG_AP(bsscfg)) {
		infra_stats = wlc_ap_infra_stats_get(wlc->ap, bsscfg);
	}
	else if (BSSCFG_INFRA_STA(bsscfg)) {
		infra_stats =
			wlc_sta_infra_stats_get(wlc->sta_info, bsscfg);
	}
	else {
		return BCME_ERROR;
	}

	/* Need to check if offset is right in the right context */
	if (offset < sizeof(wl_if_infra_stats_t)) {
		(*(uint32 *)(((uint8*)infra_stats) + offset))++;
	}
	else {
		rc = BCME_ERROR;
	}
	return rc;
}

int
wlc_ifstats_mgt_cnt_incr(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	int32 offset)
{
	/* Get the WLC instance associated with this bsscfg */
	int rc = BCME_OK;
	wl_if_mgt_stats_t *mgt_stats;

	if (!bsscfg || !(BSSCFG_AP(bsscfg) || BSSCFG_STA(bsscfg)))
		return BCME_UNSUPPORTED;

	//if ((AP_ENAB(wlc->pub)) && (BSSCFG_AP(bsscfg))) {
	if (BSSCFG_AP(bsscfg)) {
		mgt_stats = wlc_ap_mgt_stats_get(wlc->ap, bsscfg);
	}
	else if (BSSCFG_INFRA_STA(bsscfg)) {
		mgt_stats =
			wlc_sta_mgt_stats_get(wlc->sta_info, bsscfg);
	}
	else {
		return BCME_ERROR;
	}

	/* Need to check if offset is right in the right context */
	if ((offset > 0) && (offset < sizeof(wl_if_mgt_stats_t))) {
		(*(uint32 *)(((uint8*)mgt_stats) + offset))++;
	}
	else {
		rc = BCME_ERROR;
	}
	return rc;
}

/* When an iovar is issued, the top level container is in XTLV format
 * of type STATS_REPORT_CMD. Since each iovar comes on an interface,
 * an interface specific handler know how to parse the value portion of
 * the XTLV and report correspondign stats.
 * Note that the following function does not care about the XTLV. It
 * simply passes it on to registered interface function for processing
 */
int
wlc_ifstats_get(wlc_info_t *wlc, struct wlc_if *wlcif, void *arg, uint len,
	void *param, uint p_len)
{
	wlc_ifstats_reporting_func_t stats_fn;
	uint8 *dest_ptr;
	bcm_xtlvbuf_t xtlvbuf, local_xtlvbuf;
	wl_stats_report_t *request, *report;
	uint16 request_len;
	void *ctx = NULL;
	int rc = BCME_OK;

	request = (wl_stats_report_t *)param;

	/* version check */
	if (request->version != WL_STATS_REPORT_REQUEST_VERSION_V2) {
		return BCME_VERSION;
	}

	/* This is the total size of the request that needs to be stored at the
	 * end of the output buffer. The available size of the output buffer reduces
	 * by this amount.
	 */
	request_len = ROUNDUP(request->length, sizeof(uint32));

	/* The length mentioned in the request is larger than length of the
	 * argument buffer
	 */
	if (request_len >= p_len) {
		return BCME_BADLEN;
	}

	/* Get the stats function for the interface on which this ivoar came */
	stats_fn = wlc_bsscfg_iface_stats_fn_get(wlc, wlcif, &ctx);

	if (stats_fn == NULL) {
		return BCME_NOTFOUND;
	}

	/* Copy the parameters from param buffer to the end of the
	 * argument buffer for use by clients. This is to prevent clients
	 * overwriting their parameters when they are creating stats.
	 */
	/* Round down length of the output buffer */
	len = ROUNDDN(len, sizeof(uint32));

	/* We can't have too short output buffer length */
	if (request_len >= len) {
		return BCME_BADLEN;
	}

	/* Carve out space to copy request parameters */
	dest_ptr = (uint8*)arg + len - request_len;

	/* The output buffer must be sufficiently big so parameters
	 * can be completely copied to end.
	 */
	if ((dest_ptr - (uint8*) param) < request_len) {
		return BCME_BADLEN;
	}

	/* copy parameters */
	memcpy(dest_ptr, param, request_len);

	/* Length available to write output */
	len -= request_len;

	report = (wl_stats_report_t *)arg;
	report->version = WL_STATS_REPORT_RESPONSE_VERSION_V2;

	/* request is now relocated at the end of the buffer so
	 * get the pointer accordingly
	 */
	request = (wl_stats_report_t *)dest_ptr;

	/* Leave space for report version, length, and top level XTLV
	 * WL_IFSTATS_XTLV_IF.
	 */
	rc = bcm_xtlv_buf_init(&local_xtlvbuf,
		(uint8*) arg + OFFSETOF(wl_stats_report_t, data) + BCM_XTLV_HDR_SIZE,
		len - OFFSETOF(wl_stats_report_t, data) - BCM_XTLV_HDR_SIZE,
		BCM_XTLV_OPTION_ALIGN32);

	if (rc)
		goto fail;

	/* To populate top level container and its length */
	rc = bcm_xtlv_buf_init(&xtlvbuf,
		(uint8*) arg + OFFSETOF(wl_stats_report_t, data),
		len - OFFSETOF(wl_stats_report_t, data),
		BCM_XTLV_OPTION_ALIGN32);

	if (rc)
		goto fail;

	/* Put the interface index for which these stats are meant for */
	if ((rc = bcm_xtlv_put_data(&local_xtlvbuf, WL_IFSTATS_XTLV_IF_INDEX,
		&wlcif->index, sizeof(uint8))) != BCME_OK) {
		goto fail;
	}

	/* Pass destination pointer where params are copied and the
	 * original length of the param container
	 */
	rc = stats_fn(wlc, wlcif, ctx, &local_xtlvbuf,
		/* start of the XTLVs to report */
		request->data,
		/* Adjust the request length by the offset of data field */
		request->length - OFFSETOF(wl_stats_report_t, data));

	if (rc)
		goto fail;

	/* Complete the outer container with type and length
	 * only.
	 */
	rc = bcm_xtlv_put_data(&xtlvbuf,
		WL_IFSTATS_XTLV_IF,
		NULL, bcm_xtlv_buf_len(&local_xtlvbuf));

	if (rc) {
		goto fail;
	}

	/* Stamp the length of entire container */
	report->length = bcm_xtlv_buf_len(&xtlvbuf) +
		OFFSETOF(wl_stats_report_t, data);

fail:
	return rc;
}

/* Simply generate requested stats */
static int wl_ifstats_sta_ap_xtlv_cbfn(void *ctx, const uint8 *data,
	uint16 type, uint16 len)
{
	int rc = BCME_OK;

	wl_ifstats_sta_ap_xtlv_cb_ctx_t *cb_ctx;
	wl_if_mgt_stats_t *mgt_stats;
	wl_if_infra_stats_t *infra_stats;
	wl_if_stats_t *if_stats;

	cb_ctx = (wl_ifstats_sta_ap_xtlv_cb_ctx_t *)ctx;

	switch (type) {
		case WL_IFSTATS_XTLV_GENERIC: {
			/* Add the error stats and report */
			if_stats = cb_ctx->bsscfg->wlcif->_cnt;
			if_stats->rxerror =
				if_stats->rxnobuf + if_stats->rxrunt +
				if_stats->rxfragerr;

			if_stats->txerror =
				if_stats->txnobuf + if_stats->txrunt +
				if_stats->txfail;

			/* Copy generic stats */
			rc = bcm_xtlv_put_data(cb_ctx->input_xtlv_context,
				WL_IFSTATS_XTLV_GENERIC,
				(const uint8*) if_stats,
				sizeof(wl_if_stats_t));
		}
		break;

		case WL_IFSTATS_XTLV_INFRA_SPECIFIC: {
			if (BSSCFG_AP(cb_ctx->bsscfg)) {
				infra_stats =
					wlc_ap_infra_stats_get(cb_ctx->wlc->ap,
						cb_ctx->bsscfg);
			} else if (BSSCFG_INFRA_STA(cb_ctx->bsscfg)) {
				infra_stats =
					wlc_sta_infra_stats_get(
						cb_ctx->wlc->sta_info,
						cb_ctx->bsscfg);
			} else {
				rc = BCME_ERROR;
				break;
			}

			rc = bcm_xtlv_put_data(cb_ctx->input_xtlv_context,
				WL_IFSTATS_XTLV_INFRA_SPECIFIC,
				(const uint8*) infra_stats,
				sizeof(wl_if_infra_stats_t));
		}
		break;

		case WL_IFSTATS_XTLV_MGT_CNT: {
			//if ((AP_ENAB(cb_ctx->wlc->pub)) && (BSSCFG_AP(cb_ctx->bsscfg))) {
			if (BSSCFG_AP(cb_ctx->bsscfg)) {
				mgt_stats =
					wlc_ap_mgt_stats_get(cb_ctx->wlc->ap,
						cb_ctx->bsscfg);
			} else if (BSSCFG_INFRA_STA(cb_ctx->bsscfg)) {
				mgt_stats =
					wlc_sta_mgt_stats_get(
						cb_ctx->wlc->sta_info,
						cb_ctx->bsscfg);
			} else {
				rc = BCME_ERROR;
				break;
			}

			rc = bcm_xtlv_put_data(cb_ctx->input_xtlv_context,
				WL_IFSTATS_XTLV_MGT_CNT,
				(const uint8*) mgt_stats,
				sizeof(wl_if_mgt_stats_t));
		}
		break;

		default:
			rc = BCME_NOTFOUND;
			break;
	}
	return rc;
}

int
wlc_ifstats_sta_ap(wlc_info_t* wlc, struct wlc_if *wlcif, void *ctx,
	bcm_xtlvbuf_t *xtlv_context, void *param, uint p_len)
{
	wlc_bsscfg_t *bsscfg;
	wl_ifstats_sta_ap_xtlv_cb_ctx_t cb_ctx;
	bcm_xtlv_t *xtlv;
	bcm_xtlv_unpack_cbfn_t *cbfn = wl_ifstats_sta_ap_xtlv_cbfn;
	uint16 buflen;
	int rc = BCME_OK;

	if (!xtlv_context)
		return BCME_ERROR;

	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);

	if (!bsscfg) {
		return BCME_NOTFOUND;
	}

	/* For infra_sta and soft AP interfaces, simply remove the top level container
	 * to get the XTLVs to report.
	 */
	 xtlv = (bcm_xtlv_t *)(param);

	cb_ctx.bsscfg = bsscfg;
	cb_ctx.input_xtlv_context = xtlv_context;
	cb_ctx.wlcif = wlcif;
	cb_ctx.wlc = bsscfg->wlc;

	/* unwarp the top level container. For iovars only
	 * If issued by iovar, the request comes encapsulated in
	 * WL_IFSTATS_XTLV_IF container.
	 */
	if (BCM_XTLV_ID(xtlv) == WL_IFSTATS_XTLV_IF) {
		param = (uint8*)param + BCM_XTLV_HDR_SIZE;
		buflen = BCM_XTLV_LEN(xtlv);
	} else {
		buflen = p_len;
	}

	/* Report what is requested using the xtlv context passed above */
	rc = bcm_unpack_xtlv_buf(&cb_ctx, (const uint8*) param,
		buflen, BCM_XTLV_OPTION_ALIGN32,
		cbfn);

	return rc;
}
