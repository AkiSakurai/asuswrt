/*
 * Key Management Module Implementation - bsscfg support
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
 * $Id: km_bsscfg.c 783430 2020-01-28 14:19:14Z $
 */

#include "km_pvt.h"
#include "km_hw_impl.h"
#include <wlc_ratelinkmem.h>

/* Internal interface */
static void
km_bsscfg_sync_bssid(keymgmt_t *km, wlc_bsscfg_t *bsscfg);

static void
km_bsscfg_cleanup(keymgmt_t *km, wlc_bsscfg_t *bsscfg)
{
	km_bsscfg_t *bss_km;

	KM_LOG(("wl%d.%d: %s: enter\n",  KM_UNIT(km), WLC_BSSCFG_IDX(bsscfg),
		__FUNCTION__));

	bss_km = KM_BSSCFG(km, bsscfg);
	bss_km->flags |= KM_BSSCFG_FLAG_CLEANUP;

	if (bss_km->flags & KM_BSSCFG_FLAG_INITIALIZED) {
		km_free_key_block(km, WLC_KEY_FLAG_NONE, bss_km->key_idx,
			WLC_KEYMGMT_NUM_GROUP_KEYS);

#ifdef MFP
		{
			wlc_key_id_t key_id;

			bss_km->igtk_tx_key_id = WLC_KEY_ID_INVALID;
			for (key_id = WLC_KEY_ID_IGTK_1; key_id <= WLC_KEY_ID_IGTK_2; ++key_id) {
				if (bss_km->igtk_key_idx[KM_BSSCFG_IGTK_IDX_POS(key_id)] ==
					WLC_KEY_INDEX_INVALID) {
					continue;
				}

				km_free_key_block(km, WLC_KEY_FLAG_NONE,
					&bss_km->igtk_key_idx[KM_BSSCFG_IGTK_IDX_POS(key_id)], 1);
				KM_DBG_ASSERT(
					bss_km->igtk_key_idx[KM_BSSCFG_IGTK_IDX_POS(key_id)] ==
					WLC_KEY_INDEX_INVALID);
			}
		}
#endif /* MFP */

#ifdef STA
		/* clean up b4m4 keys, if applicable */
		if (KM_BSSCFG_B4M4_ENABLED(bss_km))
			km_b4m4_reset_keys(km, bsscfg);
#endif /* STA */

		/* bss_km->sta_key_idx will be cleared below */

		km_bsscfg_sync_bssid(km, bsscfg);
	}

	memset(bss_km, 0, sizeof(*bss_km));
	KM_LOG(("wl%d.%d: %s: exit\n",  KM_UNIT(km), WLC_BSSCFG_IDX(bsscfg),
		__FUNCTION__));
}

static int
km_bsscfg_init_internal(wlc_keymgmt_t *km, wlc_bsscfg_t *bsscfg)
{
	int err = BCME_OK;
	km_bsscfg_t *bss_km;
	wlc_key_index_t key_idx_arr[WLC_KEYMGMT_NUM_GROUP_KEYS];
	wlc_key_index_t key_idx;
	wlc_key_t *key;
	wlc_key_info_t key_info;
	wlc_key_flags_t alloc_flags;
	km_pvt_key_t *km_pvt_key;
	wlc_key_id_t key_id;
	size_t num_keys;
	km_flags_t km_flags;

	KM_LOG(("wl%d.%d: %s: enter\n",  KM_UNIT(km), WLC_BSSCFG_IDX(bsscfg),
		__FUNCTION__));

	bss_km = KM_BSSCFG(km, bsscfg);
	bss_km->wsec = bsscfg->wsec;
	bss_km->tx_key_id = WLC_KEY_ID_INVALID;
	bss_km->flags = KM_BSSCFG_FLAG_NONE;
	bss_km->flags |= (bsscfg->wsec & WSEC_SWFLAG) ? KM_BSSCFG_FLAG_SWKEYS : 0;
	bss_km->flags |= KM_BSSCFG_FLAG_B4M4;
	bss_km->algo = CRYPTO_ALGO_NONE;
	bss_km->amt_idx = KM_HW_AMT_IDX_INVALID;
	bss_km->cfg_amt_idx = KM_HW_AMT_IDX_INVALID;
	bss_km->tkip_cm_detected = -(WPA_TKIP_CM_DETECT + 1);

	/* initialize key indicies so bailing on errors is safe */
	for (key_id = 0; key_id < WLC_KEYMGMT_NUM_GROUP_KEYS; ++key_id) {
		bss_km->key_idx[key_id] = WLC_KEY_INDEX_INVALID;
		key_idx_arr[key_id] = WLC_KEY_INDEX_INVALID;
	}

#ifdef MFP
	bss_km->igtk_tx_key_id = WLC_KEY_ID_INVALID;
	for (key_id = WLC_KEY_ID_IGTK_1; key_id <= WLC_KEY_ID_IGTK_2; ++key_id) {
		bss_km->igtk_key_idx[KM_BSSCFG_IGTK_IDX_POS(key_id)]
				= WLC_KEY_INDEX_INVALID;
	}
#endif /* MFP */

	bss_km->scb_key_idx = WLC_KEY_INDEX_INVALID;

	/*
	 * At this point, mark the state as "initialized". We do this here
	 * so key resources can be freed during cubby deinit, even if a resource
	 * allocaion fail.
	 */
	bss_km->flags |= KM_BSSCFG_FLAG_INITIALIZED;

	/* reserve a key block for bss keys, not all of them are used by
	 * all features. PSTA does not use them, STA uses only 2 group keys
	 */
	alloc_flags = WLC_KEY_FLAG_NONE;
	num_keys = WLC_KEYMGMT_NUM_GROUP_KEYS;
	alloc_flags |= WLC_KEY_FLAG_GROUP;
	alloc_flags |= KM_WLC_DEFAULT_BSSCFG(km, bsscfg) ?
		WLC_KEY_FLAG_DEFAULT_BSS : 0;
	if (BSSCFG_AP(bsscfg))
		alloc_flags |= WLC_KEY_FLAG_AP;

	err = km_alloc_key_block(km, alloc_flags, key_idx_arr, num_keys);
	if (err != BCME_OK)
		goto done;

	/* create bss keys, as necessary */
	for (key_id = 0; key_id < num_keys; ++key_id) {
		key_idx = key_idx_arr[key_id];
		if (key_idx == WLC_KEY_INDEX_INVALID)
			continue;

		/* create the key */
		memset(&key_info, 0, sizeof(key_info));
		key_info.key_idx = key_idx;
		/* address NULL for group keys */

		/* set key flags. All default keys can be used for RX on a STA , but
		 * tx key is designated and only on AP. This  may be changed
		 * later if key algo changes to WEP
		 */
		key_info.flags = alloc_flags;
		if (BSSCFG_STA(bsscfg))
			key_info.flags |= WLC_KEY_FLAG_RX;
		if (BSSCFG_AP(bsscfg) && (key_id == bss_km->tx_key_id))
			key_info.flags |= WLC_KEY_FLAG_TX;
		if (KM_BSSCFG_IS_IBSS(bsscfg))
			key_info.flags |= (WLC_KEY_FLAG_IBSS | WLC_KEY_FLAG_TX);

		key_info.key_id = key_id;
		err = km_key_create(km, &key_info, &key);
		if (err != BCME_OK) {
			KM_ALLOC_ERR(("wl%d.%d: %s: key create status %d\n",  KM_UNIT(km),
				WLC_BSSCFG_IDX(bsscfg), __FUNCTION__, err));
			km_free_key_block(km, WLC_KEY_FLAG_NONE, &key_idx_arr[key_id], 1);
			continue; /* so we initialize all bss keys */
		}

		/* assign the key to  BSS and key table */
		bss_km->key_idx[key_id] = key_idx;

		km_pvt_key = &km->keys[key_idx];
		km_flags = KM_FLAG_NONE;
		if (key_info.flags & WLC_KEY_FLAG_TX)
			km_flags |= KM_FLAG_TX_KEY;
		km_flags |= KM_FLAG_BSS_KEY;
		if (bss_km->flags & KM_BSSCFG_FLAG_SWKEYS)
			km_flags |= KM_FLAG_SWONLY_KEY;

		km_init_pvt_key(km, km_pvt_key, CRYPTO_ALGO_NONE, key, km_flags, bsscfg, NULL);
	}

#ifdef MFP
	if (KM_MFP_ENAB(km)) {
		int mfp_err = BCME_OK;

		/* reserve a block of keys for igtk keys. PSTA shares the
		 * keys from primary STA.
		 * XXX IBSS do we need per-SCB IGTKs?
		 */
		if (BSSCFG_PSTA(bsscfg))
			goto mfp_done;

		num_keys = WLC_KEYMGMT_NUM_BSS_IGTK;
		alloc_flags = WLC_KEY_FLAG_NONE;
		if (BSSCFG_AP(bsscfg))
			alloc_flags |= WLC_KEY_FLAG_AP;

		alloc_flags |= WLC_KEY_FLAG_MGMT_GROUP;
		alloc_flags |= KM_WLC_DEFAULT_BSSCFG(km, bsscfg) ?
			WLC_KEY_FLAG_DEFAULT_BSS : 0;

		mfp_err = km_alloc_key_block(km, alloc_flags, key_idx_arr, num_keys);
		if (mfp_err != BCME_OK)
			goto done;

		/* create bss igtk default keys */
		for (key_id = WLC_KEY_ID_IGTK_1; key_id <= WLC_KEY_ID_IGTK_2; ++key_id) {
			size_t pos = KM_BSSCFG_IGTK_IDX_POS(key_id);

			key_idx = key_idx_arr[pos];
			if (key_idx == WLC_KEY_INDEX_INVALID)
				continue;

			/* create the key */
			memset(&key_info, 0, sizeof(key_info));
			key_info.key_idx = key_idx;
			/* address is NULL for default keys */
			key_info.flags = alloc_flags;

			/* IGTK is used only for TX on AP, and RX on non-AP STA */
			if (BSSCFG_AP(bsscfg)) {
				if (key_id == bss_km->igtk_tx_key_id)
					 key_info.flags |= WLC_KEY_FLAG_TX;
			} else {
				key_info.flags |= WLC_KEY_FLAG_RX;
			}
			key_info.key_id = key_id;
			mfp_err = km_key_create(km, &key_info, &key);
			if (mfp_err != BCME_OK) {
				KM_ALLOC_ERR(("wl%d.%d: %s: igtk key create status %d\n",
					KM_UNIT(km), WLC_BSSCFG_IDX(bsscfg), __FUNCTION__,
					mfp_err));
				km_free_key_block(km, WLC_KEY_FLAG_NONE, &key_idx_arr[pos], 1);
				if (err == BCME_OK)
					err = mfp_err;
				continue; /* so we initialize all default keys */
			}

			/* assign the key to  BSS and key table */
			bss_km->igtk_key_idx[pos] = key_idx;
			km_pvt_key = &km->keys[key_idx];
			km_flags = KM_FLAG_NONE;
			if (key_info.flags & WLC_KEY_FLAG_TX)
				km_flags |= KM_FLAG_TX_KEY;

			km_flags |= KM_FLAG_BSS_KEY;
			/* mfp keys are sw-only for now */
			km_flags |= KM_FLAG_SWONLY_KEY;

			km_init_pvt_key(km, km_pvt_key, CRYPTO_ALGO_NONE,
				key, km_flags, bsscfg, NULL);
		}
	}

mfp_done:

#endif /* MFP */

done:
	KM_LOG(("wl%d.%d: %s: exit status %d\n",  KM_UNIT(km),
		WLC_BSSCFG_IDX(bsscfg), __FUNCTION__, err));
	return err;
}

static uint32
km_bsscfg_sync_wsec(keymgmt_t *km, wlc_bsscfg_t *bsscfg)
{
	km_bsscfg_t *bss_km;
	uint32 wsec;

	wsec = bsscfg->wsec;
	bss_km = KM_BSSCFG(km, bsscfg);

	if (wsec == bss_km->wsec)
		goto done;

	/* if transitioning to open or from open, scb key state needs to be reset.
	 * this allows lazy allocation of key blocks and not waste keys for scbs
	 * belonging to open bss.
	 */
	if (!bss_km->wsec || !wsec) {
		scb_t *scb;
		struct scb_iter scbiter;
		FOREACH_BSS_SCB(km->wlc->scbstate, &scbiter, bsscfg, scb) {
			km_scb_reset(km, scb);
		}
	}

	KM_SWAP(uint32, wsec, bss_km->wsec);
	bss_km->flags &= ~KM_BSSCFG_FLAG_SWKEYS;
	bss_km->flags |= (bss_km->wsec & WSEC_SWFLAG) ? KM_BSSCFG_FLAG_SWKEYS : 0;

done:
	return wsec;
}

static void
km_bsscfg_sync_bssid(keymgmt_t *km, wlc_bsscfg_t *bsscfg)
{
	km_bsscfg_t *bss_km;
#ifdef WLMCNX
	km_amt_idx_t amt_idx = KM_HW_AMT_IDX_INVALID;
#endif /* WLMCNX */

	bss_km = KM_BSSCFG(km, bsscfg);
	if (KM_WLC_DEFAULT_BSSCFG(km, bsscfg) || MCNX_ENAB(KM_PUB(km)) ||
		!BSSCFG_STA(bsscfg) || BSSCFG_IBSS(bsscfg)) {
#ifdef WLMCNX
		if (MCNX_ENAB(KM_PUB(km))) {
			amt_idx = wlc_mcnx_rcmta_bssid_idx(KM_MCNX(km), bsscfg);
			if (km_hw_amt_idx_valid(km->hw, amt_idx)) {
				km_hw_amt_reserve(km->hw, amt_idx, 1, TRUE);
			}
		}
#endif /* WLMCNX */
		goto done;
	}

	/* just in case bssid has changed, release and alloc another */
	if (bss_km->amt_idx != KM_HW_AMT_IDX_INVALID)
		km_hw_amt_release(km->hw, &bss_km->amt_idx);

	KM_DBG_ASSERT(bss_km->amt_idx == KM_HW_AMT_IDX_INVALID);

	/* when cleaning up or bssid is cleared, disable bssid filtering if there is
	 * no non-default bss with associated stations
	 */
	if ((bss_km->flags & KM_BSSCFG_FLAG_CLEANUP) ||
		ETHER_ISNULLADDR(&bsscfg->BSSID)) {
		uint i;
		wlc_bsscfg_t *bsscfg2;
		bool clear_mhf4;

		clear_mhf4 = TRUE;
		FOREACH_AS_STA(km->wlc, i, bsscfg2) {
			if (bsscfg2->BSS && bsscfg != bsscfg2 &&
				!KM_WLC_DEFAULT_BSSCFG(km, bsscfg2)) {
				clear_mhf4 = FALSE;
				break;
			}
		}

		if (clear_mhf4)
			wlc_mhf(km->wlc, MHF4, MHF4_RCMTA_BSSID_EN, 0, WLC_BAND_ALL);
		goto done;
	}

	/* reserve an amt for this bssid, update amt attributes */
	bss_km->amt_idx = km_hw_amt_alloc(km->hw, &bsscfg->BSSID);
	if (bss_km->amt_idx == KM_HW_AMT_IDX_INVALID) {
		KM_ALLOC_ERR(("wl%d.%d: %s: amt idx alloc failed\n", KM_UNIT(km),
			WLC_BSSCFG_IDX(bsscfg), __FUNCTION__));
		goto done;
	}

	/* Need to clear amt first to prevent double linking CFP flow ID */
	if (isset(km->hw->used, bss_km->amt_idx)) {
		wlc_clear_addrmatch(km->wlc, bss_km->amt_idx);
	}
	/* set A3 to indicate bssid. km_hw only sets A2 as required */
	wlc_set_addrmatch(km->wlc, bss_km->amt_idx, &bsscfg->BSSID,
		AMT_ATTR_VALID | AMT_ATTR_A3 | AMT_ATTR_A2);
	wlc_mhf(km->wlc, MHF4, MHF4_RCMTA_BSSID_EN, MHF4_RCMTA_BSSID_EN, WLC_BAND_ALL);

done:
	KM_LOG(("wl%d.%d: %s: amt idx %d\n",  KM_UNIT(km), WLC_BSSCFG_IDX(bsscfg),
		__FUNCTION__, (int)bss_km->amt_idx));
}

#ifdef WOWL
static void
km_bsscfg_wowl(keymgmt_t *km, wlc_bsscfg_t *bsscfg, bool going_down)
{
	km_bsscfg_t *bss_km;
	bss_km = KM_BSSCFG(km, bsscfg);
	if (going_down)
		bss_km->flags |= KM_BSSCFG_FLAG_WOWL_DOWN;
	else
		bss_km->flags &= ~KM_BSSCFG_FLAG_WOWL_DOWN;
}
#endif // endif

km_amt_idx_t
km_bsscfg_get_amt_idx(keymgmt_t *km, wlc_bsscfg_t *bsscfg)
{
	km_bsscfg_t *bss_km;
	km_amt_idx_t amt_idx;

	KM_DBG_ASSERT(KM_VALID(km) && bsscfg != NULL);
	amt_idx = KM_HW_AMT_IDX_INVALID;
	if (!BSSCFG_STA(bsscfg))
		goto done;

	if (KM_IS_DEFAULT_BSSCFG(km, bsscfg) && KM_COREREV_GE40(km) &&
		!PSTA_ENAB(KM_PUB(km)) && KM_STA_USE_BSSID_AMT(km)) {
		amt_idx = AMT_IDX_BSSID;
		goto done;
	}

#ifdef WLMCNX
	if (MCNX_ENAB(KM_PUB(km))) {
		amt_idx = wlc_mcnx_rcmta_bssid_idx(KM_MCNX(km), bsscfg);
		if (km_hw_amt_idx_valid(km->hw, amt_idx))
			goto done;
	}
#endif /* WLMCNX */

#ifdef PSTA
	/* amt reservation support for psta */
	if (PSTA_ENAB(KM_PUB(km))) {
		km_hw_amt_reserve(km->hw, PSTA_TA_STRT_INDX, PSTA_RA_PRIM_INDX, TRUE);
		km_hw_amt_reserve(km->hw, PSTA_RA_PRIM_INDX, 1, TRUE);

		amt_idx =  wlc_psta_rcmta_idx(KM_PSTA(km), bsscfg);
		if (km_hw_amt_idx_valid(km->hw, amt_idx)) {
			km_hw_amt_reserve(km->hw, amt_idx, 1, TRUE);
			goto done;
		}
	}
#endif /* PSTA */

	bss_km = KM_BSSCFG(km, bsscfg);
#if defined(WET) || defined(WET_DONGLE)
	/* amt reservation support for wet */
	if (WET_ENAB(km->wlc) || WET_DONGLE_ENAB(km->wlc)) {
		km_hw_amt_reserve(km->hw, WET_TA_STRT_INDX, WET_RA_PRIM_INDX, TRUE);
		km_hw_amt_reserve(km->hw, WET_RA_PRIM_INDX, 1, TRUE);
		/* To support URE_MBSS, need more amt_idx for other bsscfg,
		* not just only WET_RA_STRT_INDX
		*/
		if (bss_km->cfg_amt_idx == KM_HW_AMT_IDX_INVALID)
			bss_km->cfg_amt_idx = km_hw_amt_find_and_resrv(km->hw);

		amt_idx	= bss_km->cfg_amt_idx;

		if (km_hw_amt_idx_valid(km->hw, amt_idx)) {
			km_hw_amt_reserve(km->hw, amt_idx, 1, TRUE);
			goto done;
		}
	}
#endif /* WET || WET_DONGLE */

	amt_idx = bss_km->amt_idx;

done:
	return amt_idx;
}

/* public interface */

int
km_bsscfg_init(void *ctx, wlc_bsscfg_t *bsscfg)
{
	keymgmt_t *km = (keymgmt_t *)ctx;
	km_bsscfg_t *bss_km;

	KM_DBG_ASSERT(KM_VALID(km) && bsscfg != NULL);

	bss_km = KM_BSSCFG(km, bsscfg);
	memset(bss_km, 0, sizeof(*bss_km));
	if (BSSCFG_IS_NO_KM(bsscfg)) {
		return BCME_OK;
	}

	return km_bsscfg_init_internal(km, bsscfg);
}

void
km_bsscfg_deinit(void *ctx,  wlc_bsscfg_t *bsscfg)
{
	keymgmt_t *km = (keymgmt_t *)ctx;
	KM_DBG_ASSERT(KM_VALID(km) && bsscfg != NULL);
	if (!BSSCFG_IS_NO_KM(bsscfg)) {
		km_bsscfg_cleanup(km, bsscfg);
	}
}

void
km_bsscfg_up_down(void *ctx, bsscfg_up_down_event_data_t *evt_data)
{
	keymgmt_t *km = (keymgmt_t *)ctx;
	km_bsscfg_t *bss_km;
	wlc_keymgmt_notif_t notif;

	KM_DBG_ASSERT(KM_VALID(km));
	if (evt_data == NULL || evt_data->bsscfg == NULL) {
		KM_DBG_ASSERT(evt_data != NULL && evt_data->bsscfg != NULL);
		return;
	}
	if (BSSCFG_IS_NO_KM(evt_data->bsscfg)) {
		return;
	}
	notif = WLC_KEYMGMT_NOTIF_NONE;
	bss_km = KM_BSSCFG(km, evt_data->bsscfg);
	if (KM_BSS_IS_UP(bss_km)) {
		 if (!evt_data->up)
			notif = WLC_KEYMGMT_NOTIF_BSS_DOWN;
	} else {
		if (evt_data->up)
			notif = WLC_KEYMGMT_NOTIF_BSS_UP;
	}

	if (notif == WLC_KEYMGMT_NOTIF_NONE)
		goto done;

	km_notify(km, notif, evt_data->bsscfg, NULL, NULL, NULL);
	if (notif == WLC_KEYMGMT_NOTIF_BSS_UP)
		bss_km->flags |= KM_BSSCFG_FLAG_UP;
	else
		bss_km->flags &= ~KM_BSSCFG_FLAG_UP;

	/* Do not remove GTK while bss is going down, as hostapd
	 * does not instruct firmware to reinstall GTK when
	 * first STA reassociates. This used to work with NAS
	 * as it installs GTK when first STA associates.
	 */

	/* XXX Keeping the code commented for now. Will be removed
	 * in future, once complete testing is done.
	 */
#ifdef RETAIN_GTK_ON_BSS_DOWN
#ifndef BCMAUTH_PSK
	if (notif == WLC_KEYMGMT_NOTIF_BSS_DOWN) {
		if (BSSCFG_AP(evt_data->bsscfg)) {
			wlc_keymgmt_reset(km, evt_data->bsscfg, NULL);
		}
	}
#endif // endif
#endif /* RETAIN_GTK_ON_BSS_DOWN */

done:
	KM_LOG(("wl%d.%d: %s: exit up: %d, notif: %d \n",  KM_UNIT(km),
		WLC_BSSCFG_IDX(evt_data->bsscfg), __FUNCTION__, evt_data->up, notif));
}

void km_bsscfg_reset(keymgmt_t *km, wlc_bsscfg_t *bsscfg, bool all)
{
	wlc_key_index_t key_idx;
	wlc_key_id_t key_id;
	km_bsscfg_t *bss_km;
	km_pvt_key_t *km_pvt_key;
	wlc_key_info_t key_info;
	int err;

	KM_DBG_ASSERT(KM_VALID(km) && bsscfg != NULL);
	KM_LOG(("wl%d.%d: %s: enter\n",  KM_UNIT(km), WLC_BSSCFG_IDX(bsscfg),
		__FUNCTION__));

	if (BSSCFG_IS_NO_KM(bsscfg)) {
		return;
	}

	if (all) {
		/* reset tx key id */
		err = wlc_keymgmt_set_bss_tx_key_id(km, bsscfg, 0, FALSE);
		if (err !=  BCME_OK)
		{
			KM_ERR(("wl%d.%d: %s: wlc_keymgmt_set_bss_tx_key_id status %d\n",
				KM_UNIT(km), WLC_BSSCFG_IDX(bsscfg), __FUNCTION__, err));
			KM_DBG_ASSERT(0);
		}
	}

	bss_km = KM_BSSCFG(km, bsscfg);
	for (key_id = 0; key_id < WLC_KEYMGMT_NUM_GROUP_KEYS; ++key_id) {
		key_idx = bss_km->key_idx[key_id];
		if (key_idx == WLC_KEY_INDEX_INVALID)
			continue;

		KM_ASSERT(KM_VALID_KEY_IDX(km, key_idx));
		KM_DBG_ASSERT(KM_VALID_KEY(&km->keys[key_idx]));

		km_pvt_key = &km->keys[key_idx];

		/* update any flags to take bsscfg change into account  */
		wlc_key_get_info(km_pvt_key->key, &key_info);

		bss_km->flags &= ~KM_BSSCFG_FLAG_SWKEYS;
		bss_km->flags |= (bsscfg->wsec & WSEC_SWFLAG) ? KM_BSSCFG_FLAG_SWKEYS : 0;

		km_pvt_key->flags &= ~KM_FLAG_SWONLY_KEY;
		km_pvt_key->flags |= (bss_km->flags & KM_BSSCFG_FLAG_SWKEYS) ?
			KM_FLAG_SWONLY_KEY : 0;
		key_info.flags &= ~WLC_KEY_FLAG_TX;
		if (key_id == bss_km->tx_key_id) {
			if (BSSCFG_AP(bsscfg) ||
				(bsscfg->WPA_auth == WPA_AUTH_DISABLED) ||
				KM_BSSCFG_IS_BSS(bsscfg)) {
				key_info.flags |= WLC_KEY_FLAG_TX;
			}
		}

		if (KM_BSSCFG_IS_IBSS(bsscfg))
			key_info.flags |= (WLC_KEY_FLAG_IBSS | WLC_KEY_FLAG_TX);
		else
			key_info.flags &= ~WLC_KEY_FLAG_IBSS;

		key_info.flags &= ~WLC_KEY_FLAG_NO_REPLAY_CHECK;
		if (KM_BSSCFG_IS_IBSS(bsscfg) && (bsscfg->WPA_auth == WPA_AUTH_NONE)) {
			 key_info.flags |= WLC_KEY_FLAG_NO_REPLAY_CHECK;
		}

		km_key_set_flags(km_pvt_key->key, key_info.flags);

		if (km_pvt_key->key_algo != CRYPTO_ALGO_OFF) {
			bool clear_key = all;

			/* always clear wpa keys on infra sta */
			if (!BSSCFG_AP(bsscfg) && KM_BSSCFG_IS_BSS(bsscfg) &&
				(bsscfg->WPA_auth != WPA_AUTH_DISABLED) &&
				!(bsscfg->WPA_auth & WPA_AUTH_NONE)) {
				clear_key = TRUE;
			}

			if (clear_key) {
				wlc_key_set_data(km_pvt_key->key, km_pvt_key->key_algo, NULL, 0);
			} else {
				/* reevaluate h/w key - e.g. default wep */
				km_notify(km, WLC_KEYMGMT_NOTIF_KEY_UPDATE, bsscfg, NULL,
					km_pvt_key->key, NULL);
			}
		}
	}

#ifdef MFP
	if (all) {
		/* reset igtk tx key id */
		err = wlc_keymgmt_set_bss_tx_key_id(km, bsscfg, WLC_KEY_ID_IGTK_1, TRUE);
		if (err !=  BCME_OK)
		{
			KM_ERR(("wl%d.%d: %s: wlc_keymgmt_set_bss_tx_key_id status %d\n",
				KM_UNIT(km), WLC_BSSCFG_IDX(bsscfg), __FUNCTION__, err));
			KM_DBG_ASSERT(FALSE);
		}
	}

	for (key_id = WLC_KEY_ID_IGTK_1; key_id <= WLC_KEY_ID_IGTK_2; ++key_id) {
		size_t pos = KM_BSSCFG_IGTK_IDX_POS(key_id);
		key_idx = bss_km->igtk_key_idx[pos];
		if (key_idx == WLC_KEY_INDEX_INVALID)
			continue;

		km_pvt_key = &km->keys[key_idx];
		wlc_key_get_info(km_pvt_key->key, &key_info);
		if (km_pvt_key->key_algo != CRYPTO_ALGO_OFF) {
			wlc_key_set_data(km_pvt_key->key, km_pvt_key->key_algo, NULL, 0);
		}

		if (KM_BSSCFG_IS_IBSS(bsscfg))
			key_info.flags |= WLC_KEY_FLAG_IBSS;
		else
			key_info.flags &= ~WLC_KEY_FLAG_IBSS;

		km_key_set_flags(km_pvt_key->key, key_info.flags);
	}
#endif /* MFP */

	/* reset b4m4 state */
	bss_km->flags &= ~(KM_BSSCFG_FLAG_M1RX | KM_BSSCFG_FLAG_M4TX);

	/* sec port is closed */
	bsscfg->wsec_portopen = FALSE;
	if (bsscfg->wlc->pub->up)
		wlc_set_ps_ctrl(bsscfg);

	KM_LOG(("wl%d.%d: %s: exit\n",  KM_UNIT(km), WLC_BSSCFG_IDX(bsscfg),
		__FUNCTION__));
}

void km_bsscfg_reset_sta_info(keymgmt_t *km, wlc_bsscfg_t *bsscfg,
	wlc_key_info_t *scb_key_info)
{
	km_bsscfg_t *bss_km;

	KM_DBG_ASSERT(KM_VALID(km) && bsscfg != NULL);

	KM_LOG(("wl%d.%d: %s: enter\n",  KM_UNIT(km), WLC_BSSCFG_IDX(bsscfg),
		__FUNCTION__));

	bss_km = KM_BSSCFG(km, bsscfg);
	bss_km->scb_key_idx = WLC_KEY_INDEX_INVALID;

	if (!KM_BSSCFG_NEED_STA_GROUP_KEYS(km, bsscfg))
		goto done;

	if (scb_key_info != NULL)
		bss_km->scb_key_idx = scb_key_info->key_idx;

	km_bsscfg_reset(km, bsscfg, FALSE);

done:
	KM_LOG(("wl%d.%d: %s: exit\n",  KM_UNIT(km),
		WLC_BSSCFG_IDX(bsscfg), __FUNCTION__));
}

wlc_key_t*
wlc_keymgmt_get_bss_key(keymgmt_t *km, const wlc_bsscfg_t *bsscfg,
	wlc_key_id_t key_id, wlc_key_info_t *key_info)
{
	wlc_key_index_t key_idx = WLC_KEY_INDEX_INVALID;
	wlc_key_t *key = NULL;
	const km_bsscfg_t *bss_km;
	size_t pos;

	KM_DBG_ASSERT(KM_VALID(km) && bsscfg != NULL);

	bss_km = KM_CONST_BSSCFG(km, bsscfg);
	if (bss_km == NULL)
		goto done;

	if (KM_VALID_DATA_KEY_ID(key_id)) {
		pos = KM_BSSCFG_GTK_IDX_POS(km, bsscfg, key_id);
		key_idx = bss_km->key_idx[pos];
	}

#ifdef MFP
	else if (KM_VALID_MGMT_KEY_ID(key_id)) {
		pos = KM_BSSCFG_IGTK_IDX_POS(key_id);
		key_idx = bss_km->igtk_key_idx[pos];
	}
#endif // endif
	if (key_idx == WLC_KEY_INDEX_INVALID)
		goto done;

	KM_ASSERT(KM_VALID_KEY_IDX(km, key_idx));
	KM_DBG_ASSERT(KM_VALID_KEY(&km->keys[key_idx]));
	key = km->keys[key_idx].key;

done:
	/* check for NULL */
	if (!key) {
		key = km->null_key;
	}
	wlc_key_get_info(key, key_info);
	return key;
}

wlc_key_t*
wlc_keymgmt_get_bss_tx_key(keymgmt_t *km, const wlc_bsscfg_t *bsscfg,
	bool igtk, wlc_key_info_t *key_info)
{
	wlc_key_id_t key_id;

	KM_DBG_ASSERT(KM_VALID(km) && bsscfg != NULL);
	key_id = wlc_keymgmt_get_bss_tx_key_id(km, bsscfg, igtk);
	return wlc_keymgmt_get_bss_key(km, bsscfg, key_id, key_info);
}

wlc_key_id_t
wlc_keymgmt_get_bss_tx_key_id(keymgmt_t *km,
	const wlc_bsscfg_t *bsscfg, bool igtk)
{
	const km_bsscfg_t *bss_km;
	wlc_key_id_t key_id;

	KM_DBG_ASSERT(KM_VALID(km) && bsscfg != NULL);

	key_id = WLC_KEY_ID_INVALID;
	if (!bsscfg->wsec)		/* security not configured */
		goto done;

	bss_km = KM_CONST_BSSCFG(km, bsscfg);
	if (bss_km == NULL)
		goto done;

	if (igtk) {
#ifdef MFP
		key_id = bss_km->igtk_tx_key_id;
#endif // endif
	} else {
		key_id = bss_km->tx_key_id;
	}
done:
	return key_id;
}

int
wlc_keymgmt_set_bss_tx_key_id(keymgmt_t *km, wlc_bsscfg_t *bsscfg,
	wlc_key_id_t key_id, bool igtk)
{
	km_bsscfg_t *bss_km;
	wlc_key_t *key;
	wlc_key_info_t key_info;
	wlc_key_id_t prev_key_id;
	km_pvt_key_t *km_pvt_key;
	int err = BCME_OK;

	KM_DBG_ASSERT(KM_VALID(km) && bsscfg != NULL);

	bss_km = KM_BSSCFG(km, bsscfg);
	if (bss_km == NULL) {
		err = BCME_BADARG;
		goto done;
	}

	prev_key_id = WLC_KEY_ID_INVALID;
	if (igtk) {
#ifdef MFP
		if (KM_VALID_MGMT_KEY_ID(key_id)) {
			prev_key_id =  bss_km->igtk_tx_key_id;
			bss_km->igtk_tx_key_id = key_id;
		} else
			err = BCME_BADARG;
#else
		err = BCME_UNSUPPORTED;
#endif /* MFP */
	} else {
		if (KM_VALID_DATA_KEY_ID(key_id)) {
			prev_key_id = bss_km->tx_key_id;
			bss_km->tx_key_id = key_id;
		} else
			err = BCME_BADARG;
	}

	if (err != BCME_OK)
		goto done;

	/* could bail if prev & cur are same, but take this opportunity to fix up */

	/* now update TX flag - add to new and remove from current, if any */
	key  = wlc_keymgmt_get_bss_key(km, bsscfg, key_id, &key_info);

	if (!(KM_VALID_KEY_IDX(km, key_info.key_idx))) {
		KM_ERR(("wl%d.%d: %s:key id %02x, key index %d is out of range.\n",
		KM_UNIT(km), WLC_BSSCFG_IDX(bsscfg), __FUNCTION__, key_id, key_info.key_idx));
		goto done;
	}

	km_pvt_key = &km->keys[key_info.key_idx];
	km_pvt_key->flags |= KM_FLAG_TX_KEY;
	km_key_set_flags(key, key_info.flags | WLC_KEY_FLAG_TX);

	if (prev_key_id == key_id || prev_key_id == WLC_KEY_ID_INVALID)
		goto done;

	key = wlc_keymgmt_get_bss_key(km, bsscfg, prev_key_id, &key_info);
	km_pvt_key = &km->keys[key_info.key_idx];
	km_pvt_key->flags &= ~KM_FLAG_TX_KEY;
	km_key_set_flags(key, (key_info.flags & ~WLC_KEY_FLAG_TX));

	/* update link entry */
	if (RATELINKMEM_ENAB(KM_PUB(km)) == TRUE) {
		wlc_ratelinkmem_update_link_entry(km->wlc,
			WLC_RLM_BSSCFG_LINK_SCB(km->wlc, bsscfg));
	}

done:
	return err;
}

wlc_key_algo_t
wlc_keymgmt_get_bss_key_algo(wlc_keymgmt_t *km,
	const wlc_bsscfg_t *bsscfg, bool igtk)
{
	const km_bsscfg_t *bss_km;
	wlc_key_id_t key_id;
	wlc_key_algo_t key_algo;
	wlc_key_info_t key_info;

	KM_DBG_ASSERT(KM_VALID(km) && bsscfg != NULL);
	key_algo = CRYPTO_ALGO_NONE;
	bss_km = KM_CONST_BSSCFG(km, bsscfg);
	if (igtk) {
#ifdef MFP
		for (key_id = WLC_KEY_ID_IGTK_1; key_id <= WLC_KEY_ID_IGTK_2; ++key_id) {
			wlc_keymgmt_get_bss_key(km, bsscfg, key_id, &key_info);
			key_algo = key_info.algo;
			if (key_algo != CRYPTO_ALGO_NONE)
				goto done;
		}
#endif /* MFP */
	} else {
		wlc_bsscfg_t *w_bsscfg;
		km_bsscfg_t *w_bss_km;

		/* use cached algorithm for the BSS. Otherwise use any available key */
		key_algo = bss_km->algo;
		if (key_algo != CRYPTO_ALGO_NONE)
			goto done;

		w_bsscfg = KM_WLC_BSSCFG(km->wlc, WLC_BSSCFG_IDX(bsscfg));
		KM_ASSERT(bsscfg == w_bsscfg);
		w_bss_km = KM_BSSCFG(km, w_bsscfg);

		for (key_id = 0; key_id < WLC_KEYMGMT_NUM_GROUP_KEYS; ++key_id) {
			if (!WLC_KEY_ID_IS_STA_GROUP(key_id))
				continue;
			wlc_keymgmt_get_bss_key(km, bsscfg, key_id, &key_info);
			w_bss_km->algo = key_algo = key_info.algo;
			if (key_algo != CRYPTO_ALGO_NONE)
				goto done;
		}

		wlc_keymgmt_get_bss_key(km, bsscfg, 0, &key_info);
		w_bss_km->algo = key_algo = key_info.algo;
	}

done:
	return key_algo;
}

km_bsscfg_flags_t
km_get_bsscfg_flags(keymgmt_t *km, const struct wlc_bsscfg *bsscfg)
{
	const km_bsscfg_t *bss_km;

	KM_DBG_ASSERT(KM_VALID(km) && bsscfg != NULL);
	bss_km = KM_CONST_BSSCFG(km, bsscfg);
	return bss_km->flags;
}

bool
km_bsscfg_swkeys(wlc_keymgmt_t *km, const struct wlc_bsscfg *bsscfg)
{
	const km_bsscfg_t *bss_km;

	KM_DBG_ASSERT(KM_VALID(km) && bsscfg != NULL);
	bss_km = KM_CONST_BSSCFG(km, bsscfg);
	return (bss_km->flags & KM_BSSCFG_FLAG_SWKEYS);
}

#ifdef BRCMAPIVTW
int
km_bsscfg_ivtw_enable(keymgmt_t *km, wlc_bsscfg_t *bsscfg, bool enable)
{
	wlc_key_id_t key_id;
	wlc_key_index_t key_idx;
	km_bsscfg_t *bss_km;

	KM_DBG_ASSERT(KM_VALID(km));

	bss_km = KM_BSSCFG(km, bsscfg);
	for (key_id = 0; key_id < WLC_KEYMGMT_NUM_GROUP_KEYS; ++key_id) {
		key_idx = bss_km->key_idx[key_id];
		if (!WLC_KEY_ID_IS_STA_GROUP(key_id))
			continue;
		km_ivtw_enable(km->ivtw, key_idx, enable);
	}

	return BCME_OK;
}
#endif /* BRCMAPIVTW */

uint32
km_bsscfg_get_wsec(keymgmt_t *km, const wlc_bsscfg_t *bsscfg)
{
	KM_DBG_ASSERT(KM_VALID(km));
	KM_DBG_ASSERT(bsscfg != NULL);
	return KM_CONST_BSSCFG(km, bsscfg)->wsec;
}

#ifdef WOWL
bool
km_bsscfg_wowl_down(keymgmt_t *km, const wlc_bsscfg_t *bsscfg)
{
	const km_bsscfg_t *bss_km;

	KM_DBG_ASSERT(KM_VALID(km));
	KM_DBG_ASSERT(bsscfg != NULL);
	bss_km = KM_CONST_BSSCFG(km, bsscfg);
	return (bss_km->flags & KM_BSSCFG_FLAG_WOWL_DOWN);
}
#endif /* WOWL */

void
km_bsscfg_update(keymgmt_t *km, wlc_bsscfg_t *bsscfg, km_bsscfg_change_t change)
{
	KM_DBG_ASSERT(KM_VALID(km));
	KM_DBG_ASSERT(bsscfg != NULL);

	switch (change) {
	case KM_BSSCFG_WSEC_CHANGE:
		km_bsscfg_sync_wsec(km, bsscfg);
		break;
	case KM_BSSCFG_BSSID_CHANGE:
		km_bsscfg_sync_bssid(km, bsscfg);
		break;
#ifdef WOWL
	case KM_BSSCFG_WOWL_DOWN:
		km_bsscfg_wowl(km, bsscfg, TRUE);
		break;
	case KM_BSSCFG_WOWL_UP:
		km_bsscfg_wowl(km, bsscfg, FALSE);
		break;
#endif // endif
	default:
		break;
	}

	KM_LOG(("wl%d.%d: %s: change type %d\n",  KM_UNIT(km), WLC_BSSCFG_IDX(bsscfg),
		__FUNCTION__, change));

	/* update link entry */
	if (RATELINKMEM_ENAB(KM_PUB(km)) == TRUE) {
		wlc_ratelinkmem_update_link_entry(km->wlc,
			WLC_RLM_BSSCFG_LINK_SCB(km->wlc, bsscfg));
	}
}
