/*
 * AP Radio power save with NOA source file
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
 * $Id$
 */

#ifdef RADIONOA_PWRSAVE
#if !defined(WLP2P) || defined(WLP2P_DISABLED)
#error "WLP2P needs to be defined for AP NOA Power save"
#endif // endif

#ifndef RADIO_PWRSAVE
#error "RADIO_PWRSAVE needs to be defined for RADIONOA_PWRSAVE"
#endif // endif

#if defined(MBSS) && !defined(MBSS_DISABLED)
#error "MBSS needs not to be defined for RADIONOA_PWRSAVE"
#endif // endif

#include <typedefs.h>
#include <bcmutils.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_cfg.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_mcnx.h>
#include <wlc_p2p.h>
#include <wlc_hw_priv.h>
#include <wlc_rpsnoa.h>
#include <wlc_ap.h>
#include <wlc_bsscfg_psq.h>
#include <wlc_scb.h>
#include <wlc_apps.h>
#include <wlc_wlfc.h>
#include <wlfc_proto.h>
#include <wlc_ampdu.h>
#include <wlc_iocv.h>
#include <wlc_objregistry.h>
#include <wlioctl.h>

#define CMD_FLAG_GET    0x01
#define CMD_FLAG_SET    0x02

/* IOVar table */
enum {
	IOV_RPSNOA,
	IOV_LAST
};

/* "wl rpsnoa <subcmd> [params ...]  */
static const bcm_iovar_t wlc_rpsnoa_iovars[] = {
	{"rpsnoa", IOV_RPSNOA, (0), 0, IOVT_BUFFER, 0},
	{NULL, 0, 0, 0, 0, 0}
};

/* rpsnoa info accessor */
#define RPSNOA_BSSCFG_CUBBY_LOC(rpsnoa, cfg)\
	((bss_rpsnoa_info_t **)BSSCFG_CUBBY((cfg), (rpsnoa)->cfgh))
#define RPSNOA_BSSCFG_CUBBY(rpsnoa, cfg) (*(RPSNOA_BSSCFG_CUBBY_LOC(rpsnoa, cfg)))
#define BSS_RPSNOA_INFO(rpsnoa, cfg) RPSNOA_BSSCFG_CUBBY(rpsnoa, cfg)

#define CONVERT_LEVEL_TO_START(bcnint, level)	(bcnint * (100 - (level * 10)) / 100)
#define CONVERT_LEVEL_TO_DUR(bcnint, level)		(bcnint * (level * 10) / 100)

#define RPS_NOA_MAX_CNT		((1U << 16) - 1)		/* max NoA count */
#define RPS_NOA_MAX_LEVEL					5
#define RPS_NOA_MIN_LEVEL					1
#define RPS_NOA_DEFAULT_LEVEL				3
#define RPS_NOA_DEFAULT					FALSE
#define RPS_NOA_DEFAULT_PPS				10
#define RPS_NOA_DEFAULT_QUIET				10
#define RPS_NOA_DEFAULT_ASSOC_CHECK		FALSE
#define RPS_NOA_MAX_ASSOC_CHECK			1

#define RPSNOA_IOV_MAJOR_VER 1
#define RPSNOA_IOV_MINOR_VER 0
#define RPSNOA_IOV_MAJOR_VER_SHIFT 8
#define RPSNOA_IOV_VERSION \
	((RPSNOA_IOV_MAJOR_VER << RPSNOA_IOV_MAJOR_VER_SHIFT)| RPSNOA_IOV_MINOR_VER)

typedef enum {
	RPSNOA_STATUS_DISABLED = 0,
	RPSNOA_STATUS_ENABLED,
	RPSNOA_STATUS_PENDING
} rpsnoa_status_t;

typedef struct rpsnoa_cmn_params {
	bool enable[MAX_BANDS];
	uint8 level[MAX_BANDS];
	uint32 pps[MAX_BANDS];
	uint32 quiet[MAX_BANDS];
	bool assoc_check[MAX_BANDS];
} rpsnoa_cmn_params_t;

typedef struct wlc_rpsnoa_info {
	wlc_info_t	*wlc;
	wlc_bsscfg_t	*bsscfg;
	int	cfgh;			/* handle to private data in bsscfg */
	rpsnoa_cmn_params_t *params;
	bool	pending;
} wlc_rpsnoa_info_t;

typedef struct bss_rpsnoa_info {
	wl_p2p_sched_desc_t	cur;	/* active schedule in h/w */
	/* back pointer to bsscfg */
	wlc_bsscfg_t		*bsscfg;
	bool enable;
	bool in_absence;
	uint32 noa_count;
} bss_rpsnoa_info_t;

typedef int (rpsnoa_iov_handler_t)(wlc_rpsnoa_info_t *rpsi, void *params,
	uint16 paramslen, void *result, uint16 *result_len, bool set);

typedef struct rpsnoa_subcmd {
	uint16 id;
	uint16 flags;
	uint16 min;
	rpsnoa_iov_handler_t *handler;
} rpsnoa_subcmd_t;

static int wlc_rpsnoa_doiovar(void *ctx, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif);
static void wlc_rpsnoa_intr_cb(void *ctx, wlc_mcnx_intr_data_t *notif_data);
static int wlc_rpsnoa_info_init(void *ctx, wlc_bsscfg_t *cfg);
static void wlc_rpsnoa_info_deinit(void *ctx, wlc_bsscfg_t *cfg);
static wlc_bsscfg_t * wlc_rpsnoa_find_rps_bsscfg_in_wlc(wlc_info_t *wlc);
static void wlc_rpsnoa_state_upd_cb(void *ctx, bsscfg_state_upd_data_t *evt);
static int wlc_rpsnoa_schedule(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
static int wlc_rpsnoa_params_init(wlc_info_t *wlc, rpsnoa_cmn_params_t *params);
static rpsnoa_subcmd_t *find_rpsnoa_subcmd_handler(rpsnoa_subcmd_t *array, uint16 id);
static int rpsnoa_subcmd_enable(wlc_rpsnoa_info_t *rpsi, void *params, uint16 paramlen,
	void *result, uint16 *result_len, bool set);
static int rpsnoa_subcmd_status(wlc_rpsnoa_info_t *rpsi, void *params, uint16 paramlen,
	void *result, uint16 *result_len, bool set);
static int rpsnoa_subcmd_params(wlc_rpsnoa_info_t *rpsi, void *params, uint16 paramlen,
	void *result, uint16 *result_len, bool set);
static int _rpsnoa_cmnparams_set(bool set, uint8 band_idx,
	rpsnoa_param_t *iovar_param, rpsnoa_cmn_params_t *cmnparam);
static rpsnoa_subcmd_t * get_rpsnoa_subcmds(void);
static uint16 get_num_of_rpsnoa_subcmds(void);

rpsnoa_subcmd_t rpsnoa_subcmds[] = {
	{WL_RPSNOA_CMD_ENABLE, (CMD_FLAG_SET | CMD_FLAG_GET),
	sizeof(rpsnoa_iovar_t), rpsnoa_subcmd_enable},
	{WL_RPSNOA_CMD_STATUS, (CMD_FLAG_GET),
	sizeof(rpsnoa_iovar_t), rpsnoa_subcmd_status},
	{WL_RPSNOA_CMD_PARAMS, (CMD_FLAG_SET | CMD_FLAG_GET),
	sizeof(rpsnoa_iovar_params_t), rpsnoa_subcmd_params},
};

/* module attach/detach */
wlc_rpsnoa_info_t *
BCMATTACHFN(wlc_rpsnoa_attach)(wlc_info_t *wlc)
{
	wlc_rpsnoa_info_t	*rpsi;
	rpsnoa_cmn_params_t *rpsp;
	wlc_mcnx_info_t	*mcnx = wlc->mcnx;
	bsscfg_cubby_params_t cubby_params;

	/* module states */
	if ((rpsi = (wlc_rpsnoa_info_t *)MALLOCZ(wlc->osh, sizeof(*rpsi))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}

	rpsi->wlc = wlc;
	ASSERT(mcnx != NULL);

	rpsp = (rpsnoa_cmn_params_t *)obj_registry_get(wlc->objr, OBJR_RPS_INFO);
	if (rpsp == NULL) {
		/* Core 0 case */
		if ((rpsp = (rpsnoa_cmn_params_t *)MALLOCZ(wlc->osh, sizeof(rpsnoa_cmn_params_t)))
				== NULL) {
			WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			goto fail;
		}
		rpsi->params = rpsp;
		if (wlc_rpsnoa_params_init(wlc, rpsp) != BCME_OK) {
			WL_ERROR(("wl%d: %s: fail to initialize rps params\n",
				wlc->pub->unit, __FUNCTION__));
			goto fail;
		}

		obj_registry_set(wlc->objr, OBJR_RPS_INFO, rpsp);
	} else {
		/* Core 1 case */
		rpsi->params = rpsp;
	}

	(void)obj_registry_ref(wlc->objr, OBJR_RPS_INFO);

	if (wlc_mcnx_intr_register(mcnx, wlc_rpsnoa_intr_cb, rpsi) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_mcnx_intr_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* reserve the bsscfg cubby for any bss specific private data */
	memset(&cubby_params, 0, sizeof(cubby_params));

	cubby_params.context = rpsi;
	cubby_params.fn_init = wlc_rpsnoa_info_init;
	cubby_params.fn_deinit = wlc_rpsnoa_info_deinit;
	cubby_params.fn_dump = NULL;

	rpsi->cfgh = wlc_bsscfg_cubby_reserve_ext(wlc,
			sizeof(bss_rpsnoa_info_t), &cubby_params);

	if (rpsi->cfgh < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve_ext failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	if (wlc_module_register(wlc->pub, wlc_rpsnoa_iovars, "rpsnoa", rpsi, wlc_rpsnoa_doiovar,
		NULL, NULL, NULL) != BCME_OK) {
			WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
				wlc->pub->unit, __FUNCTION__));
			goto fail;
	};

	if (wlc_bsscfg_state_upd_register(wlc, wlc_rpsnoa_state_upd_cb, rpsi) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_updown_register() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	wlc->pub->cmn->_rpsnoa = TRUE;

	return rpsi;

fail:
	/* error handling */
	MODULE_DETACH(rpsi, wlc_rpsnoa_detach);
	return NULL;
}

void
BCMATTACHFN(wlc_rpsnoa_detach)(wlc_rpsnoa_info_t *rpsi)
{
	wlc_info_t	*wlc;
	wlc_mcnx_info_t	*mcnx;

	if (rpsi == NULL)
		return;

	wlc = rpsi->wlc;
	ASSERT(wlc != NULL);

	mcnx = wlc->mcnx;
	ASSERT(mcnx != NULL);

	wlc_bsscfg_state_upd_unregister(wlc, wlc_rpsnoa_state_upd_cb, (void *)rpsi);
	wlc_mcnx_intr_unregister(mcnx, wlc_rpsnoa_intr_cb, rpsi);
	wlc_module_unregister(wlc->pub, "rpsnoa", rpsi);

	if (obj_registry_unref(wlc->objr, OBJR_RPS_INFO) == 0) {
		obj_registry_set(wlc->objr, OBJR_RPS_INFO, NULL);
		if (rpsi->params != NULL) {
			MFREE(wlc->osh, rpsi->params, sizeof(rpsnoa_cmn_params_t));
			rpsi->params = NULL;
		}
	}
	MFREE(wlc->osh, rpsi, sizeof(*rpsi));

	wlc->pub->cmn->_rpsnoa = FALSE;
}

static int
BCMINITFN(wlc_rpsnoa_params_init)(wlc_info_t *wlc, rpsnoa_cmn_params_t *params)
{
	if (wlc == NULL || params == NULL)
		return BCME_BADARG;

	params->enable[BAND_2G_INDEX] = RPS_NOA_DEFAULT;
	params->level[BAND_2G_INDEX] = RPS_NOA_DEFAULT_LEVEL;
	params->pps[BAND_2G_INDEX] = RPS_NOA_DEFAULT_PPS;
	params->quiet[BAND_2G_INDEX] = RPS_NOA_DEFAULT_QUIET;
	params->assoc_check[BAND_2G_INDEX] = RPS_NOA_DEFAULT_ASSOC_CHECK;

	params->enable[BAND_5G_INDEX] = RPS_NOA_DEFAULT;
	params->level[BAND_5G_INDEX] = RPS_NOA_DEFAULT_LEVEL;
	params->pps[BAND_5G_INDEX] = RPS_NOA_DEFAULT_PPS;
	params->quiet[BAND_5G_INDEX] = RPS_NOA_DEFAULT_QUIET;
	params->assoc_check[BAND_5G_INDEX] = RPS_NOA_DEFAULT_ASSOC_CHECK;

	return BCME_OK;
}

/* bsscfg cubby */
static int
wlc_rpsnoa_info_init(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_rpsnoa_info_t	*rpsi = (wlc_rpsnoa_info_t *)ctx;
	wlc_info_t		*wlc = rpsi->wlc;
	bss_rpsnoa_info_t	**pbrpsi = RPSNOA_BSSCFG_CUBBY_LOC(rpsi, cfg);
	bss_rpsnoa_info_t	*brpsi;
	int err = BCME_OK;

	if (BSSCFG_AP(cfg) && !BSS_P2P_ENAB(wlc, cfg)) {
		if ((brpsi = MALLOCZ(wlc->osh, sizeof(*brpsi))) == NULL) {
			WL_ERROR(("wl%d: %s: wlc_rpsnoa_info_init() malloc failed\n",
			          wlc->pub->unit, __FUNCTION__));
			err = BCME_NOMEM;
			goto fail;
		}
		brpsi->bsscfg = cfg;
		brpsi->in_absence = FALSE;
		*pbrpsi = brpsi;

		/* This will make bsscfg to have a psq in wlc_bsscfg_psq_bss_init */
		cfg->flags |= WLC_BSSCFG_TX_SUPR_ENAB;
	}

	return BCME_OK;

fail:
	wlc_rpsnoa_info_deinit(rpsi, cfg);
	return err;
}

static void
wlc_rpsnoa_info_deinit(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_rpsnoa_info_t	*rpsi = (wlc_rpsnoa_info_t *)ctx;
	wlc_info_t		*wlc = rpsi->wlc;
	bss_rpsnoa_info_t	**pbrpsi = RPSNOA_BSSCFG_CUBBY_LOC(rpsi, cfg);
	bss_rpsnoa_info_t	*brpsi = *pbrpsi;

	if (brpsi == NULL)
		return;

	MFREE(wlc->osh, brpsi, sizeof(*brpsi));
	*pbrpsi = NULL;

	if (!BSS_P2P_ENAB(wlc, cfg))
		cfg->flags &= ~WLC_BSSCFG_TX_SUPR_ENAB;
}

rpsnoa_subcmd_t *
BCMRAMFN(get_rpsnoa_subcmds)(void)
{
	return rpsnoa_subcmds;
}

uint16
BCMRAMFN(get_num_of_rpsnoa_subcmds)(void)
{
	return sizeof(rpsnoa_subcmds) / sizeof(*rpsnoa_subcmds);
}

static void
_rpsnoa_set_cmnparams_to_wlc(wlc_info_t *wlc, uint band, bool pending)
{
	wlc_rpsnoa_info_t *rpsi;
	bool enable, assoc_check;
	uint32 pps, quiet;
	rpsnoa_cmn_params_t *params_cmn;

	if (!wlc)
		return;

	rpsi = wlc->rpsnoa;
	if (!rpsi)
		return;

	params_cmn = rpsi->params;
	if (!params_cmn)
		return;

	if (band > BAND_5G_INDEX)
		return;

	if (!pending) {
		enable = params_cmn->enable[band];
		pps = params_cmn->pps[band];
		quiet = params_cmn->quiet[band];
		assoc_check = params_cmn->assoc_check[band];
		rpsi->pending = FALSE;
	} else {
		WL_TRACE(("wl%d: rpsnoa pending\n", wlc->pub->unit));
		rpsi->pending = TRUE;
		enable = pps = quiet = assoc_check = 0;
	}

	wlc_radio_pwrsave_update_params(wlc->ap, enable,
		pps, quiet, assoc_check);
}

static void
_rpsnoa_update_cmnparams_from_band(wlc_rpsnoa_info_t *rpsi, uint band)
{
	wlc_cmn_info_t *wlc_cmn = rpsi->wlc->cmn;
	wlc_info_t *current_wlc;
	wlc_bsscfg_t *ap_cfg = NULL;
	bool pending = FALSE;
	int idx, band_idx;

	FOREACH_WLC(wlc_cmn, idx, current_wlc) {
		ap_cfg = wlc_rpsnoa_bsscfgs_condition_in_wlc(current_wlc, &pending);

		if (ap_cfg) {
			band_idx = CFG_BAND_INDEX(ap_cfg);
			if (band_idx == band) {
				_rpsnoa_set_cmnparams_to_wlc(current_wlc, band_idx, pending);
				break;
			}
		}
	}
}

static void
_rpsnoa_update_cmnparams_to_wlc(wlc_info_t *wlc, wlc_rpsnoa_info_t *rpsi)
{
	int band_idx;
	wlc_bsscfg_t *ap_cfg = NULL;
	bool pending = FALSE;

	ap_cfg = wlc_rpsnoa_bsscfgs_condition_in_wlc(wlc, &pending);

	/* ap bsscfg down case(ap_cfg is null) will be handled in wlc_ap_down */
	if (ap_cfg) {
		band_idx = CFG_BAND_INDEX(ap_cfg);
		_rpsnoa_set_cmnparams_to_wlc(wlc, band_idx, pending);
	}
}

static rpsnoa_status_t
_rpsnoa_get_rps_status(wlc_rpsnoa_info_t *rpsi, uint band)
{
	wlc_info_t *wlc = rpsi->wlc;
	wlc_info_t *current_wlc;
	wlc_cmn_info_t *wlc_cmn = wlc->cmn;
	int idx, bss_idx, band_idx;
	wlc_bsscfg_t *bsscfg;
	rpsnoa_status_t res = RPSNOA_STATUS_DISABLED;

	FOREACH_WLC(wlc_cmn, idx, current_wlc) {
		FOREACH_UP_AP(current_wlc, bss_idx, bsscfg) {
			if (BSS_P2P_ENAB(current_wlc, bsscfg)) {
				/* go to next wlc */
				break;
			}

			band_idx = CFG_BAND_INDEX(bsscfg);
			if (band == band_idx) {
				res = (rpsnoa_status_t)
					wlc_radio_pwrsave_in_power_save(current_wlc->ap);
				if (res == RPSNOA_STATUS_DISABLED && rpsi->pending)
					res = RPSNOA_STATUS_PENDING;
				goto exit;
			} else {
				/* go to next wlc */
				break;
			}
		}
	}
exit:
	return res;
}

static int
rpsnoa_subcmd_enable(wlc_rpsnoa_info_t *rpsi, void *params, uint16 paramlen, void *result,
		uint16 *result_len, bool set)
{
	int res = BCME_OK;
	rpsnoa_iovar_t *in, *out;
	rpsnoa_cmn_params_t *params_cmn = rpsi->params;
	int band, idx;
	bool enable;

	ASSERT(rpsi);

	in = (rpsnoa_iovar_t*)params;
	out = (rpsnoa_iovar_t*)result;
	band = in->data->band;

	if (band > WLC_BAND_ALL || band < WLC_BAND_INVALID) {
		res = BCME_BADARG;
		goto exit;
	}

	if (set) {
		enable = (in->data->value) ? TRUE : FALSE;
		if (band == WLC_BAND_2G || band == WLC_BAND_ALL) {
			if (params_cmn->enable[BAND_2G_INDEX] != enable) {
				params_cmn->enable[BAND_2G_INDEX] = enable;
				_rpsnoa_update_cmnparams_from_band(rpsi, BAND_2G_INDEX);
			}
		}
		if (band == WLC_BAND_5G || band == WLC_BAND_ALL) {
			if (params_cmn->enable[BAND_5G_INDEX] != enable) {
				params_cmn->enable[BAND_5G_INDEX] = enable;
				_rpsnoa_update_cmnparams_from_band(rpsi, BAND_5G_INDEX);
			}
		}
	} else {
		/* Band all requires more buffer length */
		if (band == WLC_BAND_ALL &&
			paramlen < (sizeof(rpsnoa_iovar_t) + sizeof(rpsnoa_data_t))) {
			res = BCME_BUFTOOSHORT;
			goto exit;
		}

		idx = 0;
		memcpy(out, in, sizeof(rpsnoa_cmnhdr_t));
		if (band == WLC_BAND_2G || band == WLC_BAND_ALL) {
			out->data[idx].band =  WLC_BAND_2G;
			out->data[idx].value = params_cmn->enable[BAND_2G_INDEX];
			idx++;
		}
		if (band == WLC_BAND_5G || band == WLC_BAND_ALL) {
			out->data[idx].band =  WLC_BAND_5G;
			out->data[idx].value = params_cmn->enable[BAND_5G_INDEX];
			idx++;
		}
		out->hdr.cnt = idx;
	}
exit:
	return res;
}

static int
rpsnoa_subcmd_status(wlc_rpsnoa_info_t *rpsi, void *params, uint16 paramlen,
	void *result, uint16 *result_len, bool set)
{
	int res = BCME_OK;
	rpsnoa_iovar_t *in, *out;
	int band, idx = 0;

	in = (rpsnoa_iovar_t*)params;
	out = (rpsnoa_iovar_t*)result;
	band = in->data->band;

	if (band > WLC_BAND_ALL || band < WLC_BAND_INVALID) {
		res = BCME_BADARG;
		goto exit;
	}

	/* Band all requires more buffer length */
	if (band == WLC_BAND_ALL &&
		paramlen < (sizeof(rpsnoa_iovar_t) + sizeof(rpsnoa_data_t))) {
		res = BCME_BUFTOOSHORT;
		goto exit;
	}

	memcpy(out, in, sizeof(rpsnoa_cmnhdr_t));
	if (band == WLC_BAND_2G || band == WLC_BAND_ALL) {
		out->data[idx].band = WLC_BAND_2G;
		out->data[idx].value =	_rpsnoa_get_rps_status(rpsi, BAND_2G_INDEX);
		idx++;
	}
	if (band == WLC_BAND_5G || band == WLC_BAND_ALL) {
		out->data[idx].band =  WLC_BAND_5G;
		out->data[idx].value =	_rpsnoa_get_rps_status(rpsi, BAND_5G_INDEX);
		idx++;
	}
	out->hdr.cnt = idx;
exit:
	return res;
}

static int
_rpsnoa_cmnparams_set(uint8 dir, uint8 band_idx,
	rpsnoa_param_t *iovar_param, rpsnoa_cmn_params_t *cmnparam)
{
	int ret = BCME_OK;

	if (iovar_param == NULL || cmnparam == NULL) {
		ret = BCME_BADARG;
		goto exit;
	}

	if (dir == CMD_FLAG_SET) {
		cmnparam->pps[band_idx] = iovar_param->pps;
		cmnparam->quiet[band_idx] = iovar_param->quiet_time;
		cmnparam->level[band_idx] = iovar_param->level;
		cmnparam->assoc_check[band_idx] = iovar_param->stas_assoc_check;
	} else if (dir == CMD_FLAG_GET) {
		iovar_param->pps = cmnparam->pps[band_idx];
		iovar_param->quiet_time = cmnparam->quiet[band_idx];
		iovar_param->level = cmnparam->level[band_idx];
		iovar_param->stas_assoc_check = cmnparam->assoc_check[band_idx];
	} else {
		ret = BCME_BADARG;
	}
exit:
	return ret;
}

static int
rpsnoa_subcmd_params(wlc_rpsnoa_info_t *rpsi, void *params, uint16 paramlen,
	void *result, uint16 *result_len, bool set)
{
	int res = BCME_OK;
	rpsnoa_iovar_params_t *in, *out;
	rpsnoa_cmn_params_t *params_cmn = rpsi->params;
	int band, idx;

	ASSERT(rpsi);

	in = (rpsnoa_iovar_params_t*)params;
	out = (rpsnoa_iovar_params_t*)result;
	band = in->param->band;

	if (band > WLC_BAND_ALL || band < WLC_BAND_INVALID) {
		res = BCME_BADARG;
		goto exit;
	}

	if (set) {
		if (in->param->level > RPS_NOA_MAX_LEVEL ||
			in->param->level < RPS_NOA_MIN_LEVEL ||
			in->param->stas_assoc_check > RPS_NOA_MAX_ASSOC_CHECK) {
			res = BCME_RANGE;
			goto exit;
		}

		if (band == WLC_BAND_2G || band == WLC_BAND_ALL) {
			_rpsnoa_cmnparams_set(CMD_FLAG_SET, BAND_2G_INDEX,
				in->param, params_cmn);
			_rpsnoa_update_cmnparams_from_band(rpsi, BAND_2G_INDEX);
		}
		if (band == WLC_BAND_5G || band == WLC_BAND_ALL) {
			_rpsnoa_cmnparams_set(CMD_FLAG_SET, BAND_5G_INDEX,
				in->param, params_cmn);
			_rpsnoa_update_cmnparams_from_band(rpsi, BAND_5G_INDEX);
		}
	} else {
		/* Band all requires more buffer length */
		if (band == WLC_BAND_ALL &&
			paramlen < (sizeof(rpsnoa_iovar_params_t) + sizeof(rpsnoa_param_t))) {
			res = BCME_BUFTOOSHORT;
			goto exit;
		}

		idx = 0;
		memcpy(out, in, sizeof(rpsnoa_cmnhdr_t));
		if (band == WLC_BAND_2G || band == WLC_BAND_ALL) {
			out->param[idx].band =  WLC_BAND_2G;
			_rpsnoa_cmnparams_set(CMD_FLAG_GET, BAND_2G_INDEX,
				&(out->param[idx]), params_cmn);
			idx++;
		}
		if (band == WLC_BAND_5G || band == WLC_BAND_ALL) {
			out->param[idx].band =  WLC_BAND_5G;
			_rpsnoa_cmnparams_set(CMD_FLAG_GET, BAND_5G_INDEX,
				&(out->param[idx]), params_cmn);
			idx++;
		}
		out->hdr.cnt = idx;
	}
exit:
	return res;

}

static
rpsnoa_subcmd_t *find_rpsnoa_subcmd_handler(rpsnoa_subcmd_t *array, uint16 id)
{
	rpsnoa_subcmd_t *iov_subcmd_item = NULL;
	uint16 array_size = get_num_of_rpsnoa_subcmds();

	while (array_size-- != 0) {
		if (array->id == id) {
			/* found */
			iov_subcmd_item = array;
			break;
		}
		array++;
	}
	return iov_subcmd_item;
}

static int
wlc_rpsnoa_doiovar(void *ctx, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif)
{
	wlc_rpsnoa_info_t *rpsi = (wlc_rpsnoa_info_t *)ctx;
	rpsnoa_cmnhdr_t *iovar_in, *iovar_resp;
	rpsnoa_subcmd_t *subcmd_ptr, *subcmds_ptr;
	bool isset;
	int err = BCME_BADARG;

	BCM_REFERENCE(val_size);

	if (params && (p_len > sizeof(rpsnoa_cmnhdr_t))) {
		iovar_in = params;
	} else {
		WL_ERROR(("rps noa ioc error params == NULL || less than cmn header size\n"));
		goto fail;
	}

	if (iovar_in->ver != RPSNOA_IOV_VERSION)
		goto fail;

	if (iovar_in->subcmd > WL_RPSNOA_CMD_LAST - 1)
		goto fail;

	subcmds_ptr = get_rpsnoa_subcmds();
	if (subcmds_ptr == NULL) {
		err = BCME_NOTFOUND;
		goto fail;
	}

	subcmd_ptr = find_rpsnoa_subcmd_handler(subcmds_ptr, iovar_in->subcmd);
	if (!subcmd_ptr || !subcmd_ptr->handler)
		goto fail;

	if (p_len < subcmd_ptr->min)
		goto fail;

	isset = IOV_ISSET(actionid);
	if ((subcmd_ptr->flags == CMD_FLAG_GET && isset) ||
		(subcmd_ptr->flags == CMD_FLAG_SET && !isset))
		goto fail;

	iovar_resp = arg;
	return subcmd_ptr->handler(rpsi, iovar_in, p_len,
		iovar_resp, &iovar_resp->len, isset);

fail:
	return err;
}

wlc_bsscfg_t *
wlc_rpsnoa_bsscfgs_condition_in_wlc(wlc_info_t *wlc, bool* pending)
{
	rpsnoa_cmn_params_t *params_cmn = wlc->rpsnoa->params;
	wlc_bsscfg_t *cfg, *ap_cfg = NULL;
	int idx;
	uint up_bsscfg_cnt = 0;
	uint ap_cnt = 0;

	FOREACH_BSS(wlc, idx, cfg) {
		if (cfg->up == TRUE) {
			if (BSSCFG_AP(cfg) && !BSS_P2P_ENAB(wlc, cfg)) {
				ap_cnt ++;
				ap_cfg = cfg;
				continue;
			}
			up_bsscfg_cnt++;
		}
	}

	/* SCC is not supported as of now,
	 * so limit rpsnoa condition to 1 ap bsscfg and 0 other bsscfg
	 */
	if (ap_cnt == 1 && up_bsscfg_cnt == 0) {
		if (pending)
			*pending = FALSE;
	} else if (ap_cnt == 1 && up_bsscfg_cnt > 0) {
		if (pending)
			*pending = (params_cmn->enable[CFG_BAND_INDEX(ap_cfg)]) ? TRUE : FALSE;
	} else {
		ap_cfg = NULL;
		if (pending)
			*pending = FALSE;
	}

	return ap_cfg;
}

/* returns the bsscfg if rps activated bsscfg is found */
static wlc_bsscfg_t *
wlc_rpsnoa_find_rps_bsscfg_in_wlc(wlc_info_t *wlc)
{
	wlc_bsscfg_t *cfg;
	int idx;
	bss_rpsnoa_info_t	*brpsi;

	if (!wlc_radio_pwrsave_in_power_save(wlc->ap)) {
		return NULL;
	}

	FOREACH_UP_AP(wlc, idx, cfg) {
		if (!BSS_P2P_ENAB(wlc, cfg)) {
			brpsi = BSS_RPSNOA_INFO(wlc->rpsnoa, cfg);
			if (brpsi->enable) {
				break;
			}
		}
	}

	if (idx == WLC_MAXBSSCFG) {
		cfg = NULL;
	}

	return cfg;
}

static void
wlc_rpsnoa_state_upd_cb(void *ctx, bsscfg_state_upd_data_t *evt)
{
	wlc_rpsnoa_info_t       *rpsi = (wlc_rpsnoa_info_t *)ctx;
	wlc_info_t              *wlc = rpsi->wlc;
	wlc_bsscfg_t *cfg = evt->cfg;

	if ((!evt->old_up && cfg->up) || (evt->old_up && !cfg->up))
		_rpsnoa_update_cmnparams_to_wlc(wlc, rpsi);
}

static int
wlc_rpsnoa_schedule(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	uint32 tsf_l, tsf_h;
	uint32 bcnint;
	uint32 start;
	uint32 dur;
	uint32 tsfo_h, tsfo_l;
	uint32 tbtt_h, tbtt_l;
	uint32 level;
	wlc_mcnx_abs_t abs;
	bss_rpsnoa_info_t	*brpsi;
	rpsnoa_cmn_params_t *params_cmn = wlc->rpsnoa->params;

	if (cfg == NULL)
		return BCME_BADARG;

	if (!BSSCFG_AP(cfg) || P2P_GO(wlc, cfg))
		return BCME_BADARG;

	if (!wlc_radio_pwrsave_in_power_save(wlc->ap))
		return BCME_ERROR;

	wlc_read_tsf(wlc, &tsf_l, &tsf_h);

	ASSERT(cfg->associated);

	/* convert beacon interval percentage based schedule into
	 * the normal schedule format
	 */
	bcnint = cfg->current_bss->beacon_period << 10;
	brpsi = BSS_RPSNOA_INFO(wlc->rpsnoa, cfg);
	level = params_cmn->level[CFG_BAND_INDEX(cfg)];

	if (level > RPS_NOA_MAX_LEVEL ||
		level < RPS_NOA_MIN_LEVEL)
		return BCME_RANGE;

	brpsi->enable = TRUE;

	/* radio_pwrsave.level 3 means 30% absence */
	start = CONVERT_LEVEL_TO_START(bcnint, level);
	dur = CONVERT_LEVEL_TO_DUR(bcnint, level);

	/* offset to the last tbtt */
	tsfo_l = wlc_calc_tbtt_offset(bcnint >> 10, tsf_h, tsf_l);
	/* the last tbtt */
	tbtt_h = tsf_h;
	tbtt_l = tsf_l;
	wlc_uint64_sub(&tbtt_h, &tbtt_l, 0, tsfo_l);

	/* the next tbtt if the tsf is past the absence period and
	 * in the presence period
	 */
	tsfo_h = tsf_h;
	tsfo_l = tsf_l;
	wlc_uint64_sub(&tsfo_h, &tsfo_l, tbtt_h, tbtt_l);
	if (tsfo_l >= start + dur)
		wlc_uint64_add(&tbtt_h, &tbtt_l, 0, bcnint);

	abs.start = tbtt_l + start;
	abs.interval = bcnint;
	abs.duration = dur;

	abs.count = RPS_NOA_MAX_CNT;
	brpsi->noa_count = RPS_NOA_MAX_CNT;

	WL_INFORM(("wl%d: %s, normalize at tick 0x%x: "
			"start 0x%x duration %u interval %u, count %u"
			"(bi %u tbtt 0x%x start 0x%x)\n",
			wlc->pub->unit, __FUNCTION__, tsf_l,
			abs.start, abs.duration, abs.interval, abs.count,
			bcnint, tbtt_l, start));

	/* Mimic GO when NOA is in operation */
	wlc_mcnx_ap_ps_enab(wlc->mcnx, cfg, TRUE);

	/* No CTW for AP */
	wlc_mcnx_ctw_upd(wlc->mcnx, cfg, FALSE, 0);

	/* update NoA schedule for AP interface
	 * No need to convert remote to local for AP interface
	 */
	wlc_mcnx_abs_upd(wlc->mcnx, cfg, TRUE, &abs);

	return BCME_OK;
}

static void
wlc_rpsnoa_intr_cb(void *ctx, wlc_mcnx_intr_data_t *notif_data)
{
	wlc_rpsnoa_info_t	*rpsnoa = (wlc_rpsnoa_info_t *)ctx;
	wlc_info_t		*wlc = rpsnoa->wlc;
	wlc_ap_info_t		*ap = (wlc_ap_info_t *)wlc->ap;
	bss_rpsnoa_info_t	*brpsi;
	wlc_bsscfg_t *cfg;
	uint32 tsf_h;
	uint32 tsf_l;
	int bss;
	uint intr;

	cfg = notif_data->cfg;
	ASSERT(cfg != NULL);

	if (!BSSCFG_AP(cfg) || P2P_GO(wlc, cfg))
		return;

	if (!wlc_radio_pwrsave_in_power_save(ap))
		return;

	bss = wlc_mcnx_BSS_idx(wlc->mcnx, cfg);
	ASSERT(bss < M_P2P_BSS_MAX);

	tsf_h = notif_data->tsf_h;
	tsf_l = notif_data->tsf_l;
	intr = notif_data->intr;

	tsf_h = tsf_h;
	tsf_l = tsf_l;

	brpsi = BSS_RPSNOA_INFO(wlc->rpsnoa, cfg);
	if (!brpsi->enable)
		return;

	/* process each P2P interrupt (generated by ucode) */
	switch (intr) {
		case M_P2P_I_PRE_TBTT:
		case M_P2P_I_CTW_END:
			break;

		case M_P2P_I_ABS:
			brpsi->in_absence = TRUE;
			/* turn suppression queue on */
			wlc_bsscfg_tx_stop(wlc->psqi, cfg);
			/* XXX Is disabling APSD delivery and PS poll response frames necessary?
			 * In P2P GO, wlc_apps_scb_tx_block doesn't get called for STA
			 */
#ifdef PROP_TXSTATUS
			if (PROP_TXSTATUS_ENAB(wlc->pub))
				wlc_wlfc_mchan_interface_state_update(wlc, cfg,
					WLFC_CTL_TYPE_INTERFACE_CLOSE, FALSE);
			wlc_wlfc_flush_pkts_to_host(wlc, cfg);
#endif /* PROP_TXSTATUS */
			wlc_mcnx_hps_upd(wlc->mcnx, cfg, M_P2P_HPS_NOA(bss), TRUE);
			break;

		case M_P2P_I_PRS:
			brpsi->in_absence = FALSE;
			wlc_mcnx_hps_upd(wlc->mcnx, cfg, M_P2P_HPS_NOA(bss), FALSE);
			/* XXX Is allowing APSD delivery and PS poll response frames necessary?
			 * In P2P GO, wlc_apps_scb_tx_block doesn't get called for STA
			 */
			wlc_bsscfg_tx_start(wlc->psqi, cfg);
#ifdef PROP_TXSTATUS
			if (PROP_TXSTATUS_ENAB(wlc->pub)) {
				wlc_wlfc_mchan_interface_state_update(wlc, cfg,
					WLFC_CTL_TYPE_INTERFACE_OPEN, FALSE);
#ifdef WLAMPDU
				if (AMPDU_ENAB(wlc->pub)) {
					/* packet supression during close will reset ampdu-seq */
					struct scb_iter scbiter;
					struct scb *scb;

					FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {
						wlc_ampdu_send_bar_cfg(wlc->ampdu_tx, scb);
					}
				}
#endif /* WLAMPDU */
			}
#endif /* PROP_TXSTATUS */
			/* If NOA count is zero update NOA count */
			brpsi->noa_count--;
			if (wlc_mcnx_read_shm(wlc->mcnx,
				M_P2P_BSS_NOA_CNT(wlc, bss)) == 0) {
				if (brpsi->noa_count != 0)
					WL_ERROR(("wl%d: wlc_rpsnoa_intr_cb: "
						"BSS %d SHM and NoA count mismatch, noa count %u\n",
						wlc->pub->unit, bss, brpsi->noa_count));
				if (wlc_radio_pwrsave_in_power_save(wlc->ap))
					wlc_rpsnoa_schedule(wlc, cfg);
			}

			break;
		default:
			ASSERT(0);
			break;
	}
}

int
wlc_set_rpsnoa(wlc_info_t *wlc)
{
	wlc_bsscfg_t *cfg;
	bool pending = FALSE;

	WL_TRACE(("wl%d: wlc_set_rpsnoa\n", wlc->pub->unit));

	if (!wlc_radio_pwrsave_in_power_save(wlc->ap)) {
		return BCME_ERROR;
	}

	/* Check other bsscfgs in wlc.
	 * If any other activated bsscfg exists,
	 * do not set rpsnoa until only one active ap bsscfg exist in this wlc
	 */
	cfg = wlc_rpsnoa_bsscfgs_condition_in_wlc(wlc, &pending);
	if (!cfg || pending) {
		return BCME_BUSY;
	}

	return wlc_rpsnoa_schedule(wlc, cfg);
}

int
wlc_reset_rpsnoa(wlc_info_t *wlc)
{
	wlc_bsscfg_t *cfg;
	int bss;
	bss_rpsnoa_info_t	*brpsi;

	WL_TRACE(("wl%d: wlc_reset_rpsnoa\n", wlc->pub->unit));

	if (!wlc_radio_pwrsave_in_power_save(wlc->ap))
		return BCME_ERROR;

	/* This might not be an error */
	cfg = wlc_rpsnoa_find_rps_bsscfg_in_wlc(wlc);
	if (!cfg)
		return BCME_ERROR;

	bss = wlc_mcnx_BSS_idx(wlc->mcnx, cfg);
	ASSERT(bss < M_P2P_BSS_MAX);

	brpsi = BSS_RPSNOA_INFO(wlc->rpsnoa, cfg);
	if (!brpsi->enable)
		return BCME_ERROR;

	if (brpsi->in_absence) {
		brpsi->in_absence = FALSE;
		wlc_mcnx_hps_upd(wlc->mcnx, cfg, M_P2P_HPS_NOA(bss), FALSE);
		/* XXX Is allowing APSD delivery and PS poll response frames necessary?
		 * In P2P GO, wlc_apps_scb_tx_block doesn't get called for STA
		 */
		wlc_bsscfg_tx_start(wlc->psqi, cfg);
#ifdef PROP_TXSTATUS
		if (PROP_TXSTATUS_ENAB(wlc->pub)) {
			wlc_wlfc_mchan_interface_state_update(wlc, cfg,
				WLFC_CTL_TYPE_INTERFACE_OPEN, FALSE);
#ifdef WLAMPDU
			if (AMPDU_ENAB(wlc->pub)) {
				/* packet supression during close will reset ampdu-seq */
				struct scb_iter scbiter;
				struct scb *scb;

				FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {
					wlc_ampdu_send_bar_cfg(wlc->ampdu_tx, scb);
				}

			}
#endif /* WLAMPDU */
		}
#endif /* PROP_TXSTATUS */
	}

	wlc_mcnx_ap_ps_enab(wlc->mcnx, cfg, FALSE);
	wlc_mcnx_abs_upd(wlc->mcnx, cfg, FALSE, NULL);
	brpsi->enable = FALSE;

	return BCME_OK;
}
#endif /* RADIONOA_PWRSAVE */
