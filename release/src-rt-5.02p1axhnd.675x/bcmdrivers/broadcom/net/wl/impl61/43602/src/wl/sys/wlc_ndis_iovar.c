/**
 * @file
 * @brief
 * WLC NDIS IOVAR module (iovars/ioctls that are used in between wlc and per port) of
 * Broadcom 802.11bang Networking Device Driver
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
 * $Id: wlc_ndis_iovar.c 393352 2013-03-27 07:03:27Z $*
 *
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <siutils.h>
#include <d11.h>
#include <wlioctl.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_ndis_iovar.h>
#include <wlc_ap.h>
#include <bcmendian.h>
#include <bcmwpa.h>
#include <epivers.h>
#include <wlc_scan.h>

static int wlc_ndis_iovar_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid,
	const char *name, void *params, uint p_len, void *arg, int len, int val_size,
	wlc_if_t *wlcif);
static void wlc_wlif_stats_get(wlc_info_t *wlc, wlc_if_t *wlcif,
	wlc_if_stats_t *wlcif_stats);
static void wlc_iov_get_wlc_ver(wl_wlc_version_t *ver);

enum {
	IOV_KEY_REMOVE_ALL = 1,
	IOV_IF_ADD,
	IOV_IF_DEL,
	IOV_IF_PARAMS,
	IOV_BSSCFG,
	IOV_DEFAULT_RATESET,
	IOV_APS_ASSOC,
	IOV_STA_ASSOC,
	IOV_ENCRYPT_STRENGTH,
	IOV_ENCRYPT_ALGO,
	IOV_BSS_TX_KEY_ID,
	IOV_BSSCFG_FLAGS,
	IOV_IF_COUNTERS,
	IOV_WLC_VER,
	IOV_SCAN_IN_PROGRESS,
	IOV_BCN_PRB,
	IOV_LAST        /* In case of a need to check max ID number */
};

const bcm_iovar_t ndis_iovars[] = {
	{"key_remove_all", IOV_KEY_REMOVE_ALL,
	(0), IOVT_BUFFER, 0
	},
	{"if_add", IOV_IF_ADD,
	(IOVF_OPEN_ALLOW), IOVT_BUFFER, (sizeof(wl_if_add_t))
	},
	{"if_del", IOV_IF_DEL,
	(IOVF_OPEN_ALLOW), IOVT_BUFFER, (sizeof(int32))
	},
	{"if_params", IOV_IF_PARAMS,
	(0), IOVT_BOOL, 0
	},
	{"bsscfg", IOV_BSSCFG,
	(0), IOVT_BUFFER, sizeof(wl_bsscfg_t)
	},
	{"default_rateset", IOV_DEFAULT_RATESET,
	(0), IOVT_BUFFER, (sizeof(wl_rates_info_t))
	},
	{"aps_assoc", IOV_APS_ASSOC,
	(0), IOVT_UINT8, 0
	},
	{"stas_assoc", IOV_STA_ASSOC,
	(0), IOVT_UINT8, 0
	},
	{"encrypt_strength", IOV_ENCRYPT_STRENGTH,
	(0), IOVT_UINT32, 0
	},
	{ "encrypt_algo", IOV_ENCRYPT_ALGO,
	(0), IOVT_UINT32, 0
	},
	{ "bss_tx_key_id", IOV_BSS_TX_KEY_ID,
	(0), IOVT_UINT32, 0
	},
	{"bsscfg_flags", IOV_BSSCFG_FLAGS,
	(0), IOVT_UINT32, 0
	},
	{ "if_counters", IOV_IF_COUNTERS,
	(IOVF_OPEN_ALLOW), IOVT_BUFFER, sizeof(wlc_if_stats_t)
	},
	{ "wlc_ver", IOV_WLC_VER,
	(0), IOVT_BUFFER, sizeof(wl_wlc_version_t)
	},
	{ "scan_in_progress", IOV_SCAN_IN_PROGRESS,
	(0), IOVT_BOOL, 0
	},
	{ "bcn_prb", IOV_BCN_PRB,
	(0), IOVT_BUFFER, sizeof(int)
	},
	{ NULL, 0, 0, 0, 0 }
};

int
BCMATTACHFN(wlc_ndis_iovar_attach)(wlc_info_t *wlc)
{
	wlc_pub_t *pub = wlc->pub;
	if (wlc_module_register(pub, ndis_iovars, "ndis_iovar", wlc, wlc_ndis_iovar_doiovar,
	                        NULL, NULL, NULL)) {
		WL_ERROR(("wl%d: ndis_iovar wlc_module_register() failed\n", pub->unit));
		return BCME_ERROR;
	}

	return BCME_OK;
}

int
BCMATTACHFN(wlc_ndis_iovar_detach)(wlc_info_t *wlc)
{
	if (wlc_module_unregister(wlc->pub, "ndis_iovar", wlc)) {
		WL_ERROR(("wl: ndis_iovar wlc_module_unregister() failed\n"));
		return BCME_ERROR;
	}

	return BCME_OK;
}

static int
wlc_ndis_iovar_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif)
{
	wlc_info_t * wlc = (wlc_info_t *)hdl;
	int err = BCME_OK;
	bool bool_val;
	int32 int_val = 0;
	wlc_bsscfg_t *bsscfg;
	int32 *ret_int_ptr;

	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	bool_val = (int_val != 0) ? TRUE : FALSE;
	ret_int_ptr = (int32 *)arg;

	BCM_REFERENCE(bool_val);

	switch (actionid) {

	case IOV_SVAL(IOV_KEY_REMOVE_ALL):
		if (wlc->pub->up) {
			wlc_key_remove_all(wlc, bsscfg);
		}
		break;

	case IOV_SVAL(IOV_IF_ADD): {
		int idx;
		wlc_bsscfg_t *cfg;
		wl_if_add_t if_buf;

		if ((uint)len < sizeof(wl_if_add_t)) {
			WL_ERROR(("wl%d: input buffer too short\n", wlc->pub->unit));
			err = BCME_BUFTOOSHORT;
			break;
		}

		bcopy((char *)arg, (char*)&if_buf, sizeof(wl_if_add_t));
		/* allocate bsscfg */
		if ((idx = wlc_bsscfg_get_free_idx(wlc)) == BCME_ERROR) {
			WL_ERROR(("wl%d: no free bsscfg\n", wlc->pub->unit));
			return BCME_NORESOURCE;
		}

		cfg = wlc_bsscfg_alloc(wlc, idx, if_buf.bsscfg_flags, &if_buf.mac_addr,
			(bool)if_buf.ap);
		if (!cfg) {
			WL_ERROR(("wl%d: can not allocate bsscfg\n", wlc->pub->unit));
			break;
		}

		/* init as sta to match init op mode */
		cfg->_ap = (bool)if_buf.ap;
		if (wlc_bsscfg_init(wlc, cfg) != BCME_OK) {
			WL_ERROR(("wl%d: can not init bsscfg\n", wlc->pub->unit));
			wlc_bsscfg_free(wlc, cfg);
			break;
		}

		cfg->wlcif->if_flags |= if_buf.if_flags;
		/* Win7 does BSS bridging */
		cfg->ap_isolate = TRUE;
#if BCMDBG
		printf("wl%d: if_add success\n", wlc->pub->unit);
#endif // endif
		break;
	}

	case IOV_SVAL(IOV_IF_DEL): {
		if (WLC_BSSCFG_IDX(bsscfg) != 0) {
			if (bsscfg->enable) {
				wlc_bsscfg_disable(wlc, bsscfg);
			}
			wlc_bsscfg_free(wlc, bsscfg);
		} else {
			WL_ERROR(("wl%d: if_del failed: not delete primary bsscfg\n",
				wlc->pub->unit));
		}
		break;
	}

	case IOV_SVAL(IOV_IF_PARAMS):
		bsscfg->wlcif->if_flags |= int_val;
		break;

	case IOV_GVAL(IOV_BSSCFG): {
		wl_bsscfg_t *p = (wl_bsscfg_t *)arg;
		ASSERT(p);

		if ((uint)len < sizeof(wl_bsscfg_t)) {
			WL_ERROR(("wl%d: input buffer too short: len %d\n", wlc->pub->unit, len));
			err = BCME_BUFTOOSHORT;
			break;
		}
		p->bsscfg_idx = (uint32)bsscfg->_idx;
		p->wsec = (uint32)bsscfg->wsec;
		p->WPA_auth = (uint32)bsscfg->WPA_auth;
		p->wsec_index = (uint32)bsscfg->wsec_index;
		p->associated = (uint32)bsscfg->associated;
		p->BSS = (uint32)bsscfg->BSS;
#ifdef WLC_HIGH
		p->phytest_on = (uint32)wlc->pub->phytest_on;
#else
		p->phytest_on = 0;
#endif /* WLC_HIGH */
		p->assoc_state = bsscfg->assoc ? bsscfg->assoc->state : AS_VALUE_NOT_SET;
		p->assoc_type = bsscfg->assoc ? bsscfg->assoc->type : AS_VALUE_NOT_SET;
		bcopy(&bsscfg->prev_BSSID, &p->prev_BSSID, sizeof(struct ether_addr));
		bcopy(&bsscfg->BSSID, &p->BSSID, sizeof(struct ether_addr));
		p->targetbss_wpa2_flags = 0;
		if (bsscfg->target_bss) {
			p->targetbss_wpa2_flags = bsscfg->target_bss->wpa2.flags;
		}
		break;
	}

	case IOV_GVAL(IOV_DEFAULT_RATESET): {
		wl_rates_info_t *rates_info = (wl_rates_info_t *)params;
		wl_rates_info_t *rates_out = (wl_rates_info_t *)arg;
		wlc_rateset_t rs_tgt;

		bzero(&rs_tgt, sizeof(wlc_rateset_t));

		wlc_rateset_default(&rs_tgt, NULL, rates_info->phy_type,
			rates_info->bandtype, rates_info->cck_only, rates_info->rate_mask,
			rates_info->mcsallow, rates_info->bw, rates_info->txstreams);

		/* copy fields to return struct */
		rates_out->rs_tgt.count = rs_tgt.count;
		memcpy(&rates_out->rs_tgt.rates, &rs_tgt.rates, WLC_NUMRATES * sizeof(uint8));
		break;
	}

	case IOV_GVAL(IOV_APS_ASSOC):
		*ret_int_ptr = wlc->aps_associated;
		break;

	case IOV_GVAL(IOV_STA_ASSOC):
		*ret_int_ptr = wlc->stas_associated;
		break;

	case IOV_GVAL(IOV_ENCRYPT_STRENGTH):
		if (bsscfg->wsec_index >= 0) {
			*ret_int_ptr = wlc->wsec_keys[bsscfg->wsec_index]->len;
		} else {
			*ret_int_ptr = 0;
		}
		break;

	case IOV_GVAL(IOV_ENCRYPT_ALGO):
		if (bsscfg->wsec_index >= 0) {
			*ret_int_ptr = wlc->wsec_keys[bsscfg->wsec_index]->algo;
		}
		else {
			*ret_int_ptr = 0;
		}
		break;

	case IOV_GVAL(IOV_BSS_TX_KEY_ID):
		if (bsscfg->wsec_index >= 0) {
			*ret_int_ptr = wlc->wsec_keys[bsscfg->wsec_index]->id;
		}
		else {
			*ret_int_ptr = 0;
		}
		break;
	case IOV_GVAL(IOV_BSSCFG_FLAGS):
		*ret_int_ptr = bsscfg->flags;
		break;

	case IOV_SVAL(IOV_BSSCFG_FLAGS):
		bsscfg->flags |= int_val;
		break;

	case IOV_GVAL(IOV_IF_COUNTERS):
		wlc_wlif_stats_get(wlc, wlcif, (wlc_if_stats_t *)arg);
		break;

	case IOV_GVAL(IOV_WLC_VER):
		wlc_iov_get_wlc_ver((wl_wlc_version_t *)arg);
		break;

	case IOV_GVAL(IOV_SCAN_IN_PROGRESS): {
		*ret_int_ptr = SCAN_IN_PROGRESS(wlc->scan);
		break;
	}

	case IOV_GVAL(IOV_BCN_PRB):	{
		if ((uint32)len < sizeof(uint32)+bsscfg->current_bss->bcn_prb_len) {
			return BCME_BUFTOOSHORT;
		}
		bcopy(&bsscfg->current_bss->bcn_prb_len, (char*)arg, sizeof(uint32));
		bcopy((char*)bsscfg->current_bss->bcn_prb, (char*)arg + sizeof(uint32),
			bsscfg->current_bss->bcn_prb_len);
		break;
	}

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

static void
wlc_wlif_stats_get(wlc_info_t *wlc, wlc_if_t *wlcif, wlc_if_stats_t *wlc_if_stats)
{
	if ((wlcif == NULL) || (wlc_if_stats == NULL))
		return;

	/*
	 * Aggregate errors from other errors
	 * These other errors are only updated when it makes sense
	 * that the error should be charged to a logical interface.
	 */
	wlcif->_cnt.txerror = wlcif->_cnt.txnobuf + wlcif->_cnt.txrunt;
	wlcif->_cnt.rxerror = wlcif->_cnt.rxnobuf + wlcif->_cnt.rxrunt + wlcif->_cnt.rxfragerr;

	memcpy(wlc_if_stats, &(wlcif->_cnt), sizeof(wlc_if_stats_t));
}

static void
wlc_iov_get_wlc_ver(wl_wlc_version_t *ver)
{
	ASSERT(ver);

	ver->version = (uint16)WL_WLC_VERSION_T_VERSION;
	ver->length = (uint16)sizeof(wl_wlc_version_t);

	/* set epi version numbers */
	ver->epi_ver_major = (uint16)EPI_MAJOR_VERSION;
	ver->epi_ver_minor = (uint16)EPI_MINOR_VERSION;
	ver->epi_rc_num = (uint16)EPI_RC_NUMBER;
	ver->epi_incr_num = (uint16)EPI_INCREMENTAL_NUMBER;

	/* set WLC interface version numbers */
	ver->wlc_ver_major = (uint16)WLC_VERSION_MAJOR;
	ver->wlc_ver_minor = (uint16)WLC_VERSION_MINOR;
}

si_t *
wlc_get_sih(wlc_info_t *wlc)
{
	return wlc->pub->sih;
}

bool
wlc_cfg_associated(wlc_info_t *wlc)
{
	return wlc->cfg->associated;
}
