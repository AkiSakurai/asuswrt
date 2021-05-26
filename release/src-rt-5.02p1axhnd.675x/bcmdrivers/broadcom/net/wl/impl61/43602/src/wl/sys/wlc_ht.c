/*
 * Common (OS-independent) portion of
 * Broadcom 802.11 Networking Device Driver
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
 * $Id$
 */

/** 802.11n (High Throughput) */

#include <wlc_cfg.h>
#include <wlc_types.h>
#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <siutils.h>
#include <wlioctl.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <d11.h>
#include <wlc_bsscfg.h>
#include <wlc_rate.h>
#include <wlc.h>
#include <wlc_ie_misc_hndlrs.h>
#include <wlc_ht.h>
#include <wlc_prot_n.h>
#include <wlc_scb.h>
#include <wlc_ie_mgmt.h>
#include <wlc_ie_mgmt_ft.h>
#include <wlc_ie_mgmt_vs.h>
#include <wlc_ie_reg.h>

/* IE mgmt */
static uint wlc_ht_calc_cap_ie_len(void *ctx, wlc_iem_calc_data_t *data);
static int wlc_ht_write_cap_ie(void *ctx, wlc_iem_build_data_t *data);
static uint wlc_ht_calc_op_ie_len(void *ctx, wlc_iem_calc_data_t *data);
static int wlc_ht_write_op_ie(void *ctx, wlc_iem_build_data_t *data);
static uint wlc_ht_calc_obss_scan_ie_len(void *ctx, wlc_iem_calc_data_t *data);
static int wlc_ht_write_obss_scan_ie(void *ctx, wlc_iem_build_data_t *data);
static uint wlc_ht_calc_brcm_cap_ie_len(void *ctx, wlc_iem_calc_data_t *data);
static int wlc_ht_write_brcm_cap_ie(void *ctx, wlc_iem_build_data_t *data);
#ifdef AP
static int wlc_ht_parse_cap_ie(void *ctx, wlc_iem_parse_data_t *data);
#endif // endif
static int wlc_ht_scan_parse_cap_ie(void *ctx, wlc_iem_parse_data_t *data);
static int wlc_ht_scan_parse_op_ie(void *ctx, wlc_iem_parse_data_t *data);
static int wlc_ht_scan_parse_brcm_ie(void *ctx, wlc_iem_parse_data_t *data);
#ifdef WLTDLS
static uint wlc_ht_tdls_calc_cap_ie_len(void *ctx, wlc_iem_calc_data_t *data);
static int wlc_ht_tdls_write_cap_ie(void *ctx, wlc_iem_build_data_t *data);
#ifdef IEEE2012_TDLSSEPC
static uint wlc_ht_tdls_calc_op_ie_len(void *ctx, wlc_iem_calc_data_t *data);
static int wlc_ht_tdls_write_op_ie(void *ctx, wlc_iem_build_data_t *data);
#endif // endif
#endif /* AP */
#ifdef WLTDLS
static int wlc_ht_tdls_parse_cap_ie(void *ctx, wlc_iem_parse_data_t *parse);
#ifdef IEEE2012_TDLSSEPC
static int wlc_ht_tdls_parse_op_ie(void *ctx, wlc_iem_parse_data_t *parse);
#endif /* IEEE2012_TDLSSEPC */
#endif /* WLTDLS */

struct wlc_ht_info {
	wlc_info_t *wlc;
};

wlc_ht_info_t *
BCMATTACHFN(wlc_ht_attach)(wlc_info_t *wlc)
{
	wlc_ht_info_t *hti;
	uint16 capfstbmp =
	        FT2BMP(FC_BEACON) |
	        FT2BMP(FC_PROBE_RESP) |
#ifdef STA
	        FT2BMP(FC_ASSOC_REQ) |
	        FT2BMP(FC_REASSOC_REQ) |
#endif // endif
#ifdef AP
	        FT2BMP(FC_ASSOC_RESP) |
	        FT2BMP(FC_REASSOC_RESP) |
#endif // endif
	        FT2BMP(FC_PROBE_REQ) |
	        0;
	uint16 opfstbmp =
	        FT2BMP(FC_BEACON) |
	        FT2BMP(FC_PROBE_RESP) |
#ifdef AP
	        FT2BMP(FC_ASSOC_RESP) |
	        FT2BMP(FC_REASSOC_RESP) |
#endif // endif
	        0;
	uint16 obssfstbmp =
	        FT2BMP(FC_BEACON) |
	        FT2BMP(FC_PROBE_RESP) |
#ifdef AP
	        FT2BMP(FC_ASSOC_RESP) |
	        FT2BMP(FC_REASSOC_RESP) |
#endif // endif
	        0;
	uint16 brcmfstbmp =
#ifdef STA
	        FT2BMP(FC_ASSOC_REQ) |
	        FT2BMP(FC_REASSOC_REQ) |
#endif // endif
	        FT2BMP(FC_PROBE_REQ) |
	        0;
#ifdef AP
	uint16 cap_parse_fstbmp =
	        FT2BMP(FC_ASSOC_REQ) |
	        FT2BMP(FC_REASSOC_REQ) |
	        0;
#endif // endif
	uint16 scan_parse_fstbmp =
	        FT2BMP(WLC_IEM_FC_SCAN_BCN) |
	        FT2BMP(WLC_IEM_FC_SCAN_PRBRSP) |
	        0;

	/* allocate private states */
	if ((hti = MALLOC(wlc->osh, sizeof(wlc_ht_info_t))) == NULL) {
		goto fail;
	}
	hti->wlc = wlc;

	/* register IE mgmt callbacks */
	/* calc/build */
	/* bcn/prbrsp/assocreq/reassocreq/assocresp/reassocresp/prbreq */
	if (wlc_iem_add_build_fn_mft(wlc->iemi, capfstbmp, DOT11_MNG_HT_CAP,
	      wlc_ht_calc_cap_ie_len, wlc_ht_write_cap_ie, hti) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_build_fn failed, ht cap ie\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	/* bcn/prbrsp/assocresp/reassocresp */
	if (wlc_iem_add_build_fn_mft(wlc->iemi, opfstbmp, DOT11_MNG_HT_ADD,
	      wlc_ht_calc_op_ie_len, wlc_ht_write_op_ie, hti) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_build_fn failed, ht op ie\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	if (wlc_iem_add_build_fn_mft(wlc->iemi, obssfstbmp, DOT11_MNG_HT_OBSS_ID,
	      wlc_ht_calc_obss_scan_ie_len, wlc_ht_write_obss_scan_ie, hti) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_build_fn failed, obss ie\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	/* assocreq/reassocreq/prbreq */
	if (wlc_iem_vs_add_build_fn_mft(wlc->iemi, brcmfstbmp, WLC_IEM_VS_IE_PRIO_BRCM_HT,
	      wlc_ht_calc_brcm_cap_ie_len, wlc_ht_write_brcm_cap_ie, hti) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_vs_add_build_fn failed, brcm ie\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#ifdef WLTDLS
	/* tdlssetupreq */
	if (TDLS_SUPPORT(wlc->pub)) {
		if (wlc_ier_add_build_fn(wlc->ier_tdls_srq, DOT11_MNG_HT_CAP,
			wlc_ht_tdls_calc_cap_ie_len, wlc_ht_tdls_write_cap_ie, hti)
			!= BCME_OK) {
			WL_ERROR(("wl%d: %s: wlc_ier_add_build_fn failed, "
				"ht cap ie in tdls setupreq\n", wlc->pub->unit, __FUNCTION__));
			goto fail;
		}
		/* tdlssetupresp */
		if (wlc_ier_add_build_fn(wlc->ier_tdls_srs, DOT11_MNG_HT_CAP,
			wlc_ht_tdls_calc_cap_ie_len, wlc_ht_tdls_write_cap_ie, hti) != BCME_OK) {
			WL_ERROR(("wl%d: %s: wlc_ier_add_build_fn failed, "
				"ht cap ie in tdls setupresp\n", wlc->pub->unit, __FUNCTION__));
			goto fail;
		}
	/* tdlssetupconfirm */
#ifdef IEEE2012_TDLSSEPC
		if (wlc_ier_add_build_fn(wlc->ier_tdls_scf, DOT11_MNG_HT_ADD,
			wlc_ht_tdls_calc_op_ie_len, wlc_ht_tdls_write_op_ie, hti) != BCME_OK) {
			WL_ERROR(("wl%d: %s: wlc_ier_add_build_fn failed, "
				"ht op ie in tdls setupconfirm\n", wlc->pub->unit, __FUNCTION__));
			goto fail;
		}
#else
		if (wlc_ier_add_build_fn(wlc->ier_tdls_scf, DOT11_MNG_HT_CAP,
			wlc_ht_tdls_calc_cap_ie_len, wlc_ht_tdls_write_cap_ie, hti) != BCME_OK) {
			WL_ERROR(("wl%d: %s: wlc_ier_add_build_fn failed, "
				"ht cap ie in tdls setupconfirm\n", wlc->pub->unit, __FUNCTION__));
			goto fail;
		}
#endif // endif
		/* tdlsdiscresp */
		if (wlc_ier_add_build_fn(wlc->ier_tdls_drs, DOT11_MNG_HT_CAP,
			wlc_ht_tdls_calc_cap_ie_len, wlc_ht_tdls_write_cap_ie, hti) != BCME_OK) {
			WL_ERROR(("wl%d: %s: wlc_ier_add_build_fn failed, "
				"ht cap ie in tdls discresp\n", wlc->pub->unit, __FUNCTION__));
			goto fail;
		}
	}
#endif /* WLTDLS */
	/* parse */
#ifdef AP
	/* assocreq/reassocreq */
	if (wlc_iem_add_parse_fn_mft(wlc->iemi, cap_parse_fstbmp, DOT11_MNG_HT_CAP,
		wlc_ht_parse_cap_ie, hti) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_parse_fn failed, ht cap ie\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#endif // endif
	/* bcn/prbrsp */
	if (wlc_iem_add_parse_fn_mft(wlc->iemi, scan_parse_fstbmp, DOT11_MNG_HT_CAP,
	                             wlc_ht_scan_parse_cap_ie, hti) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_parse_fn failed, ht cap ie in scan\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	if (wlc_iem_add_parse_fn_mft(wlc->iemi, scan_parse_fstbmp, DOT11_MNG_HT_ADD,
	                             wlc_ht_scan_parse_op_ie, hti) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_parse_fn failed, ht op ie in scan\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	if (wlc_iem_vs_add_parse_fn_mft(wlc->iemi, scan_parse_fstbmp, WLC_IEM_VS_IE_PRIO_BRCM_HT,
	                                wlc_ht_scan_parse_brcm_ie, hti) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_vsadd_parse_fn failed, brcm ie in scan\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#ifdef WLTDLS
	/* tdls */
	if (TDLS_SUPPORT(wlc->pub)) {
		if (wlc_ier_add_parse_fn(wlc->ier_tdls_srq, DOT11_MNG_HT_CAP,
			wlc_ht_tdls_parse_cap_ie, hti) != BCME_OK) {
			WL_ERROR(("wl%d: %s: wlc_ier_add_parse_fn failed, "
				"ht cap ie in setupreq\n", wlc->pub->unit, __FUNCTION__));
			goto fail;
		}
		if (wlc_ier_add_parse_fn(wlc->ier_tdls_srs, DOT11_MNG_HT_CAP,
			wlc_ht_tdls_parse_cap_ie, hti) != BCME_OK) {
			WL_ERROR(("wl%d: %s: wlc_ier_add_parse_fn failed, "
				"ht cap ie in setupresp\n", wlc->pub->unit, __FUNCTION__));
			goto fail;
		}
#ifdef IEEE2012_TDLSSEPC
		if (wlc_ier_add_parse_fn(wlc->ier_tdls_scf, DOT11_MNG_HT_ADD,
			wlc_ht_tdls_parse_op_ie, hti) != BCME_OK) {
			WL_ERROR(("wl%d: %s: wlc_ier_add_parse_fn failed, "
				"ht op ie in setupconfirm\n", wlc->pub->unit, __FUNCTION__));
			goto fail;
		}
#else
		if (wlc_ier_add_parse_fn(wlc->ier_tdls_scf, DOT11_MNG_HT_CAP,
			wlc_ht_tdls_parse_cap_ie, hti) != BCME_OK) {
			WL_ERROR(("wl%d: %s: wlc_ier_add_parse_fn failed, "
				"ht cap ie in setupconfirm\n", wlc->pub->unit, __FUNCTION__));
			goto fail;
		}
#endif // endif
	}
#endif /* WLTDLS */

	return hti;

fail:
	wlc_ht_detach(hti);
	return NULL;
} /* wlc_ht_attach */

void
BCMATTACHFN(wlc_ht_detach)(wlc_ht_info_t *hti)
{
	wlc_info_t *wlc;

	if (hti == NULL)
		return;

	wlc = hti->wlc;

	MFREE(wlc->osh, hti, sizeof(wlc_ht_info_t));
}

static uint8 *
wlc_ht_get_mcs(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint16 ft, wlc_iem_ft_cbparm_t *ftcbparm)
{
	uint8 *mcs = NULL;

	switch (ft) {
	case FC_ASSOC_REQ:
	case FC_REASSOC_REQ:
		mcs = ftcbparm->assocreq.target->rateset.mcs;
		break;
	case FC_ASSOC_RESP:
	case FC_REASSOC_RESP:
		ASSERT(ftcbparm->assocresp.mcs != NULL);
		mcs = ftcbparm->assocresp.mcs;
		break;
	case FC_PROBE_REQ:
		ASSERT(ftcbparm->prbreq.mcs != NULL);
		mcs = ftcbparm->prbreq.mcs;
		break;
	case FC_BEACON:
	case FC_PROBE_RESP:
		if (ftcbparm->bcn.mcs != NULL)
			mcs = ftcbparm->bcn.mcs;
		else if (wlc->stf->throttle_state)
			mcs = wlc->band->hw_rateset.mcs;
		else
			mcs = cfg->current_bss->rateset.mcs;
		break;
	default:
		break;
	}

	return mcs;
}

/** HT Capability */
static uint
wlc_ht_calc_cap_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	if (data->cbparm->ht)
		return TLV_HDR_LEN + HT_CAP_IE_LEN;

	return 0;
}

static int
wlc_ht_write_cap_ie(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_ht_info_t *hti = (wlc_ht_info_t *)ctx;
	wlc_info_t *wlc = hti->wlc;
	wlc_bsscfg_t *cfg = data->cfg;

	if (data->cbparm->ht) {
		ht_cap_ie_t cap_ie;
		uint8 *mcs;

		mcs = wlc_ht_get_mcs(wlc, cfg, data->ft, data->cbparm->ft);
		ASSERT(mcs != NULL);

		/* Prop mcs rate should NOT be advertised under HT cap */
#if defined(WLPROPRIETARY_11N_RATES)
		if (WLPROPRIETARY_11N_RATES_ENAB(wlc->pub)) {
			wlc_rate_clear_prop_11n_mcses(mcs);
		}
#endif // endif

		wlc_ht_build_cap_ie(cfg, &cap_ie, mcs, BAND_2G(wlc->band->bandtype));

		bcm_write_tlv(DOT11_MNG_HT_CAP, &cap_ie, HT_CAP_IE_LEN, data->buf);
	}

	return BCME_OK;
}

#ifdef AP
static int
wlc_ht_parse_cap_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_ht_info_t *hti = (wlc_ht_info_t *)ctx;
	wlc_info_t *wlc = hti->wlc;
	wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;

	if (!data->pparm->ht)
		return BCME_OK;

	if (data->ie == NULL) {
		switch (data->ft) {
		case FC_ASSOC_REQ:
		case FC_REASSOC_REQ:
			if (N_REQD(wlc->pub)) {
				/* non N client trying to associate, reject them */
				ftpparm->assocreq.status = DOT11_SC_ASSOC_RATE_MISMATCH;
				return BCME_ERROR;
			}
			break;
		}
		return BCME_OK;
	}

	/* find the HT cap IE, if found copy the mcs set into the requested rates */
	switch (data->ft) {
	case FC_ASSOC_REQ:
	case FC_REASSOC_REQ:
		ftpparm->assocreq.ht_cap_ie = data->ie;
		break;
	}

	return BCME_OK;
}
#endif /* AP */

#ifdef WLTDLS
static uint
wlc_ht_tdls_calc_cap_ie_len(void *ctx, wlc_iem_calc_data_t *calc)
{
	if (calc->cbparm->ht)
		return TLV_HDR_LEN + HT_CAP_IE_LEN;

	return 0;
}

static int
wlc_ht_tdls_write_cap_ie(void *ctx, wlc_iem_build_data_t *build)
{
	wlc_ht_info_t *hti = (wlc_ht_info_t *)ctx;
	wlc_info_t *wlc = hti->wlc;
	wlc_bsscfg_t *cfg = build->cfg;

	if (build->cbparm->ht) {
		ht_cap_ie_t cap_ie;
		uint8 *mcs;

		mcs = wlc->band->hw_rateset.mcs;
		ASSERT(mcs != NULL);

		wlc_ht_build_cap_ie(cfg, &cap_ie, mcs, BAND_2G(wlc->band->bandtype));

		bcm_write_tlv(DOT11_MNG_HT_CAP, &cap_ie, HT_CAP_IE_LEN, build->buf);
	}

	return BCME_OK;
}
#endif /* WLTDLS */

/** HT Operation */
static uint
wlc_ht_calc_op_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	if (data->cbparm->ht)
		return TLV_HDR_LEN + HT_ADD_IE_LEN;

	return 0;
}

static int
wlc_ht_write_op_ie(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_ht_info_t *hti = (wlc_ht_info_t *)ctx;
	wlc_info_t *wlc = hti->wlc;
	wlc_bsscfg_t *cfg = data->cfg;

	if (data->cbparm->ht) {
		ht_add_ie_t add_ie;

		wlc_prot_n_build_add_ie(wlc->prot_n, cfg, &add_ie);

		bcm_write_tlv(DOT11_MNG_HT_ADD, &add_ie, HT_ADD_IE_LEN, data->buf);
	}

	return BCME_OK;
}

#ifdef WLTDLS
#ifdef IEEE2012_TDLSSEPC
static uint
wlc_ht_tdls_calc_op_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	wlc_iem_ft_cbparm_t *ftcbparm = data->cbparm->ft;

	if (!ftcbparm->tdls.ht_op_ie)
		return 0;

	if (data->cbparm->ht)
		return TLV_HDR_LEN + HT_ADD_IE_LEN;

	return 0;
}

static int
wlc_ht_tdls_write_op_ie(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_ht_info_t *hti = (wlc_ht_info_t *)ctx;
	wlc_info_t *wlc = hti->wlc;
	wlc_bsscfg_t *cfg = data->cfg;

	if (data->cbparm->ht) {
		ht_add_ie_t add_ie;

		wlc_prot_n_build_add_ie(wlc->prot_n, cfg, &add_ie);

		bcm_write_tlv(DOT11_MNG_HT_ADD, &add_ie, HT_ADD_IE_LEN, data->buf);
	}

	return BCME_OK;
}
#endif /* IEEE2012_TDLSSEPC */
#endif /* WLTDLS */

/** OBSS Scan */
static uint
wlc_ht_calc_obss_scan_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	wlc_bsscfg_t *cfg = data->cfg;

	if (!data->cbparm->ht)
		return 0;
	if (!COEX_ACTIVE(cfg))
		return 0;

	return TLV_HDR_LEN + DOT11_OBSS_SCAN_IE_LEN;
}

static int
wlc_ht_write_obss_scan_ie(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_ht_info_t *hti = (wlc_ht_info_t *)ctx;
	wlc_info_t *wlc = hti->wlc;
	wlc_bsscfg_t *cfg = data->cfg;
	obss_params_t params;

	if (!data->cbparm->ht)
		return BCME_OK;
	if (!COEX_ACTIVE(cfg))
		return BCME_OK;

	/* need to convert to 802.11 little-endian format */
	bcopy((uint8 *)&cfg->obss->params, (uint8 *)&params, WL_OBSS_SCAN_PARAM_LEN);

	/* convert params to 802.11 network order */
	wlc_ht_obss_scanparams_hostorder(wlc, &params, FALSE);

	bcm_write_tlv(DOT11_MNG_HT_OBSS_ID, &params, DOT11_OBSS_SCAN_IE_LEN, data->buf);

	/* Support for HT Information 20/40MHz Exchange */

	return BCME_OK;
}

/** BRCM HT Cap */
static uint
wlc_ht_calc_brcm_cap_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	if (!data->cbparm->ht)
		return 0;
	if (data->cfg->target_bss->flags & WLC_BSS_BRCM)
		return TLV_HDR_LEN + HT_CAP_IE_LEN + HT_PROP_IE_OVERHEAD;
	else
		return 0;
}

static int
wlc_ht_write_brcm_cap_ie(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_ht_info_t *hti = (wlc_ht_info_t *)ctx;
	wlc_info_t *wlc = hti->wlc;
	wlc_bsscfg_t *cfg = data->cfg;
	ht_cap_ie_t cap_ie;
	uint8 *mcs;

	if (!data->cbparm->ht)
		return BCME_OK;
	if (data->cfg->target_bss->flags & WLC_BSS_BRCM) {
		mcs = wlc_ht_get_mcs(wlc, cfg, data->ft, data->cbparm->ft);
		ASSERT(mcs != NULL);

		wlc_ht_build_cap_ie(cfg, &cap_ie, mcs, BAND_2G(wlc->band->bandtype));
		wlc_write_brcm_ht_cap_ie(wlc, data->buf, data->buf_len, &cap_ie);
	}
	return BCME_OK;
}

static void
wlc_ht_scan_parse_cap(ht_cap_ie_t *cap, wlc_iem_ft_pparm_t *ftpparm)
{
	wlc_bss_info_t *bi = ftpparm->scan.result;
	uint16 ht_cap = ltoh16_ua(&cap->cap);

	/* Mark the BSS as HT capable */
	bi->flags |= WLC_BSS_HT;

	/* Mark the BSS as 40 Intolerant if bit is set */
	if (ht_cap & HT_CAP_40MHZ_INTOLERANT)
		bi->flags |= WLC_BSS_40INTOL;

	/* Set SGI flags */
	if (ht_cap & HT_CAP_SHORT_GI_20)
		bi->flags |= WLC_BSS_SGI_20;
	if (ht_cap & HT_CAP_SHORT_GI_40)
		bi->flags |= WLC_BSS_SGI_40;

	/* copy the raw mcs set (supp_mcs) into the bss rateset struct */
	bcopy(&cap->supp_mcs[0], &bi->rateset.mcs[0], MCSSET_LEN);

	if (ht_cap & HT_CAP_40MHZ)
		ftpparm->scan.cap_bw_40 = TRUE;

	/* Set 40MHZ employed bit on bss */
	if (ftpparm->scan.op_bw_any && ftpparm->scan.cap_bw_40)
		bi->flags |= WLC_BSS_40MHZ;
}

/** callback function */
static int
wlc_ht_scan_parse_cap_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_ht_info_t *hti = (wlc_ht_info_t *)ctx;
	wlc_info_t *wlc = hti->wlc;
	ht_cap_ie_t *cap;
	wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;

	if (data->ie == NULL)
		return BCME_OK;

	/* find the 11n HT capability ie, mark the bss as 11n (HT) and save the mcs set */
	if ((cap = wlc_read_ht_cap_ie(wlc, data->ie, data->ie_len)) != NULL)
		wlc_ht_scan_parse_cap(cap, ftpparm);

	return BCME_OK;
}

/** callback function */
static int
wlc_ht_scan_parse_op_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_ht_info_t *hti = (wlc_ht_info_t *)ctx;
	wlc_info_t *wlc = hti->wlc;
	ht_add_ie_t *op;
	wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;
	wlc_bss_info_t *bi = ftpparm->scan.result;
	int err = BCME_ERROR;

	if (data->ie == NULL)
		return BCME_OK;

	if ((op = wlc_read_ht_add_ie(wlc, data->ie, data->ie_len)) != NULL) {
		bi->chanspec = wlc_ht_chanspec(wlc, op->ctl_ch, op->byte1);

		if (op->byte1 & HT_BW_ANY)
			ftpparm->scan.op_bw_any = TRUE;

		/* Set 40MHZ employed bit on bss */
		if (ftpparm->scan.op_bw_any && ftpparm->scan.cap_bw_40)
			bi->flags |= WLC_BSS_40MHZ;

		err = BCME_OK;
	}

	return err;
}

/** callback function. Should be named 'wlc_ht_scan_parse_brcm_ht_ie' instead. */
static int
wlc_ht_scan_parse_brcm_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_ht_info_t *hti = (wlc_ht_info_t *)ctx;
	wlc_info_t *wlc = hti->wlc;
	ht_cap_ie_t *cap;
	wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;

	if (data->ie == NULL)
		return BCME_OK;

	/*
	 * Find the 11n HT capability ie, mark the bss as 11n (HT) and save the mcs set.
	 * brcm prop ht cap ie is only parsed if 'standard' ht cap ie is not present.
	 */
	if ((cap = wlc_read_ht_cap_ies(wlc, data->ie, data->ie_len)) != NULL)
		wlc_ht_scan_parse_cap(cap, ftpparm);

	return BCME_OK;
}

#ifdef WLTDLS
/* TODO: fold this into 'scan' processing if possible... */
static int
wlc_ht_tdls_parse_cap_ie(void *ctx, wlc_iem_parse_data_t *parse)
{
	wlc_ht_info_t *hti = (wlc_ht_info_t *)ctx;
	wlc_info_t *wlc = hti->wlc;
	wlc_iem_ft_pparm_t *ftpparm = parse->pparm->ft;
	wlc_bss_info_t *bi = ftpparm->tdls.result;
	ht_cap_ie_t *cap_ie;

	if (parse->ie == NULL)
		return BCME_OK;

	/* find the 11n HT capability ie, mark the bss as 11n (HT) and save the mcs set */
	if ((cap_ie = wlc_read_ht_cap_ie(wlc, parse->ie, parse->ie_len)) != NULL) {
		uint16 ht_cap = ltoh16_ua(&cap_ie->cap);

		/* Mark the BSS as HT capable */
		bi->flags |= WLC_BSS_HT;

		/* Mark the BSS as 40 Intolerant if bit is set */
		if (ht_cap & HT_CAP_40MHZ_INTOLERANT) {
			bi->flags |= WLC_BSS_40INTOL;
		}

		/* Set SGI flags */
		if (ht_cap & HT_CAP_SHORT_GI_20)
			bi->flags |= WLC_BSS_SGI_20;
		if (ht_cap & HT_CAP_SHORT_GI_40)
			bi->flags |= WLC_BSS_SGI_40;

		/* copy the raw mcs set into the bss rateset struct */
		bcopy(&cap_ie->supp_mcs[0], &bi->rateset.mcs[0], MCSSET_LEN);
	}

	return BCME_OK;
}

#ifdef IEEE2012_TDLSSEPC
static int
wlc_ht_tdls_parse_op_ie(void *ctx, wlc_iem_parse_data_t *parse)
{
	wlc_ht_info_t *hti = (wlc_ht_info_t *)ctx;
	wlc_iem_ft_pparm_t *ftpparm = parse->pparm->ft;
	wlc_bss_info_t *bi = ftpparm->tdls.result;
	wlc_info_t *wlc = hti->wlc;
	ht_add_ie_t	*ht_op_ie;
	chanspec_t chspec;

	if (parse->ie == NULL)
		return BCME_OK;

	ht_op_ie = wlc_read_ht_add_ie(wlc, parse->ie, parse->ie_len);

	if (ht_op_ie) {
		chspec = wlc_ht_chanspec(wlc, ht_op_ie->ctl_ch, ht_op_ie->byte1);
		if (chspec != INVCHANSPEC) {
			bi->chanspec = chspec;
			bi->flags |= WLC_BSS_40MHZ;
		}
	}
	return BCME_OK;
}
#endif /* IEEE2012_TDLSSEPC */
#endif /* WLTDLS */
