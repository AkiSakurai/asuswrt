/*
 * Common (OS-independent) portion of
 * Broadcom 802.11abg Networking Device Driver
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
 * $Id: wlc_sta.c 785981 2020-04-13 06:09:29Z $
 */

/* Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
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
#include <wlc_tx.h>
#include <wlc_btcx.h>
#ifdef WLMCHAN
#include <wlc_mchan.h>
#endif /* WLMCHAN */
#ifdef WLP2P
#include <wlc_p2p.h>
#endif /* WLP2P */
#include <wlc_ie_misc_hndlrs.h>
#ifdef PROP_TXSTATUS
#include <wlc_ampdu.h>
#include <wlc_apps.h>
#include <wlc_wlfc.h>
#include <wlc_scb.h>
#endif /* PROP_TXSTATUS */
#ifdef WLMCNX
#include <wlc_mcnx.h>
#endif /* WLMCNX */
#include <wlc_msch.h>
#include <wlc_chanctxt.h>
#include <wlc_sta.h>
#include <wlc_ap.h>
#include <wlc_dfs.h>
#ifdef WL_MODESW
#include <wlc_modesw.h>
#endif // endif
#include <wlc_hw.h>
#include <wlc_pm.h>
#include <wlc_scan_utils.h>
#include <wlc_he.h>
#include <wlc_iocv.h>
#include <phy_chanmgr_api.h>
#include <phy_calmgr_api.h>
#ifdef WL_LEAKY_AP_STATS
#include <wlc_leakyapstats.h>
#endif /* WL_LEAKY_AP_STATS */

/* iovar table */
enum {
	IOV_AWD_DATA = 1,
	IOV_LAST
};

static const bcm_iovar_t sta_iovars[] = {
	{"awd_data_info", IOV_AWD_DATA, 0, 0, IOVT_BUFFER, 0},
	{NULL, 0, 0, 0, 0, 0}
};

struct wlc_sta_info {
	wlc_info_t *wlc;		/* pointer to main wlc structure */
	int32 cfg_cubby_hdl;		/* BSSCFG cubby offset */
};

typedef struct sta_cfg_cubby {
	wlc_msch_req_handle_t *msch_req_hdl;
	uint32 msch_state;
	wlc_msch_req_param_t req;
	uint32 join_flags;
} sta_cfg_cubby_t;

/* bsscfg states access macros */
#define BSSCFG_STA_CUBBY_LOC(sta_info, cfg) \
	((sta_cfg_cubby_t **)BSSCFG_CUBBY((cfg), (sta_info)->cfg_cubby_hdl))
#define BSSCFG_STA_CUBBY(sta_info, cfg) (*BSSCFG_STA_CUBBY_LOC(sta_info, cfg))

/* STA join flags */
#define STA_JOIN_FLAGS_NEW_JOIN		0x1	/* STA new association */

/* STA Channel Ctx states (msch_state) */
#define	WLC_MSCH_ON_CHANNEL		0x1
#define	WLC_MSCH_OFFCHANNEL_PREP	0x2
#define	WLC_MSCH_PM_NOTIF_PEND		0x3
#define	WLC_MSCH_PM_DONE		0x4
#define	WLC_MSCH_SUSPEND_FIFO_PEND	0x5
#define	WLC_MSCH_OFFCHANNEL		0x6
#define WLC_MSCH_REQ_START		0x7

static int wlc_sta_msch_clbk(void* handler_ctxt, wlc_msch_cb_info_t *cb_info);
static bool wlc_sta_prepare_pm_mode(wlc_bsscfg_t *cfg);
static void wlc_sta_return_home_channel(wlc_bsscfg_t *cfg);
static void wlc_sta_prepare_off_channel(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
static void wlc_sta_off_channel_done(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
static int sta_bss_init(void *ctx, wlc_bsscfg_t *cfg);
static void sta_bss_deinit(void *ctx, wlc_bsscfg_t *cfg);
static void wlc_sta_assoc_state_clbk(void *arg,
	bss_assoc_state_data_t *notif_data);
static void wlc_sta_bsscfg_state_upd(void *ctx, bsscfg_state_upd_data_t *evt);
static wlc_bsscfg_t* wlc_sta_iface_create(void *module_ctx, wl_interface_create_t *if_buf,
	wl_interface_info_t *wl_info, int32 *err);
static int32 wlc_sta_iface_remove(wlc_info_t *wlc, wlc_if_t *wlcif);

static int
wlc_sta_doiovar(void *hdl, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif)
{
	wlc_sta_info_t *sta_info = hdl;
	wlc_info_t *wlc = sta_info->wlc;
	int err = BCME_OK;
	int32 int_val = 0;
	wlc_bsscfg_t *bsscfg;

	/* update bsscfg pointer */
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	/* Do the actual parameter implementation */
	switch (actionid) {

	case IOV_GVAL(IOV_AWD_DATA):
		/* For now supported only join info, can be extended in the futute. */
		if (int_val == AWD_DATA_JOIN_INFO) {
			err = wlc_join_attempt_info_get(bsscfg, arg, len);
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
} /* wlc_sta_doiovar */

static int
wlc_sta_down(void *ctx)
{
	wlc_sta_info_t *sta_info = (wlc_sta_info_t *)ctx;
	wlc_info_t *wlc = sta_info->wlc;
	wlc_bsscfg_t *cfg;
	int idx;

	FOREACH_STA(wlc, idx, cfg) {
		wlc_sta_timeslot_unregister(cfg);
	}
	return BCME_OK;
}

wlc_sta_info_t *
BCMATTACHFN(wlc_sta_attach)(wlc_info_t *wlc)
{
	wlc_sta_info_t *sta_info;

	sta_info = MALLOCZ(wlc->osh, sizeof(wlc_sta_info_t));

	if (!sta_info) {
		return NULL;
	}
	sta_info->wlc = wlc;

	/* reserve cubby in the bsscfg container for private data */
	if ((sta_info->cfg_cubby_hdl = wlc_bsscfg_cubby_reserve(wlc, sizeof(sta_cfg_cubby_t *),
		sta_bss_init, sta_bss_deinit, NULL, (void *)sta_info)) < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* Add client callback to the bss associate notification list */
	if (wlc_bss_assoc_state_register(wlc, wlc_sta_assoc_state_clbk, sta_info)
		!= BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bss_assoc_state_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* Add client callback to the bsscfg updown notification list */
	if (wlc_bsscfg_state_upd_register(wlc, wlc_sta_bsscfg_state_upd, sta_info) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_state_upd_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* Add callback function for STA interface creation */
	if (wlc_bsscfg_iface_register(wlc, WL_INTERFACE_TYPE_STA, wlc_sta_iface_create,
		wlc_sta_iface_remove, sta_info) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_iface_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	if (wlc_module_register(wlc->pub, sta_iovars, "sta", sta_info, wlc_sta_doiovar,
			NULL, NULL, wlc_sta_down)) {
		WL_ERROR(("wl%d: %s wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	return sta_info;

fail:
	MODULE_DETACH(sta_info, wlc_sta_detach);
	return NULL;
}

void
BCMATTACHFN(wlc_sta_detach)(wlc_sta_info_t *sta_info)
{
	wlc_info_t *wlc;

	ASSERT(sta_info);

	wlc = sta_info->wlc;

	(void)wlc_bss_assoc_state_unregister(wlc, wlc_sta_assoc_state_clbk, sta_info);
	(void)wlc_bsscfg_state_upd_unregister(wlc, wlc_sta_bsscfg_state_upd, sta_info);
	(void)wlc_module_unregister(wlc->pub, "sta", sta_info);

	/* Unregister the STA interface during detach */
	if (wlc_bsscfg_iface_unregister(wlc, WL_INTERFACE_TYPE_STA) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_iface_unregister() failed\n",
			WLCWLUNIT(wlc), __FUNCTION__));
	}

	MFREE(wlc->osh, sta_info, sizeof(wlc_sta_info_t));
}

static void
wlc_sta_assoc_state_clbk(void *arg, bss_assoc_state_data_t *notif_data)
{
	wlc_bsscfg_t *cfg;
	sta_cfg_cubby_t *sta_cfg_cubby = NULL;

	ASSERT(notif_data != NULL);

	cfg = notif_data->cfg;
	ASSERT(cfg != NULL);

	if (!BSSCFG_STA(cfg) || BSSCFG_SPECIAL(cfg->wlc, cfg)) {
		return;
	}

	sta_cfg_cubby = BSSCFG_STA_CUBBY(cfg->wlc->sta_info, cfg);
	ASSERT(sta_cfg_cubby != NULL);

	if (notif_data->state == AS_IBSS_CREATE || notif_data->state == AS_JOIN_ADOPT) {
		sta_cfg_cubby->join_flags |= STA_JOIN_FLAGS_NEW_JOIN;
		/* --- register STA request --- */
		wlc_sta_timeslot_register(cfg);

		WL_INFORM(("wlc_sta_assoc_state_clbk: timeslot registered:"
			" cfg_idx:%d, type:%d, notif_state:%d\n",
			WLC_BSSCFG_IDX(cfg), cfg->type, notif_data->state));
		/* When successfully associated AP, update multi-DTIM capability bit and
		 * beacon listen interval parameters.
		 * Note that, this is done only for the STA interface and
		 * the following call relies on !BSSCFG_STA(cfg) check above.
		 */
		if (notif_data->state == AS_JOIN_ADOPT) {
			if (!P2P_CLIENT(cfg->wlc, cfg)) {
				wlc_bcn_li_upd(cfg->wlc);
			}
		}
	} else if (notif_data->state == AS_SCAN && !cfg->associated) {
		/* --- unregister STA request --- */
		wlc_sta_timeslot_unregister(cfg);
	}
}

static void
wlc_sta_bsscfg_state_upd(void *ctx, bsscfg_state_upd_data_t *evt)
{
	wlc_bsscfg_t *cfg = evt->cfg;
	ASSERT(cfg != NULL);

	if (BSSCFG_AP(cfg)) {
		/* some AP cfg state changed, handle watchdog alignment appropriately for STA */
		wlc_watchdog_upd(cfg, WLC_WATCHDOG_TBTT(cfg->wlc));
		return;
	}

	if (BSSCFG_SPECIAL(cfg->wlc, cfg)) {
		return;
	}

	/* Unregister the STA timeslot when disabling bsscfg */
	if (evt->old_enable && !cfg->enable) {
		wlc_sta_timeslot_unregister(cfg);
	}
}

/*
 * Function:	wlc_sta_iface_create
 *
 * Purpose:	Function to create sta interface through interface create command.
 *
 * Parameters:
 * module_ctx	: Module context
 * if_buf	: interface create buffer
 * wl_info	: out parameter of created interface
 * err		: pointer to store the error status
 *
 * Returns:	cfg pointer - If success
 *		NULL on failure
 */
static wlc_bsscfg_t*
wlc_sta_iface_create(void *module_ctx, wl_interface_create_t *if_buf, wl_interface_info_t *wl_info,
	int32 *err)
{
	wlc_bsscfg_t *cfg;
	wlc_sta_info_t *sta_info = module_ctx;
	wlc_bsscfg_type_t type = {BSSCFG_TYPE_GENERIC, BSSCFG_GENERIC_STA};

	ASSERT(sta_info->wlc);

	cfg = wlc_iface_create_generic_bsscfg(sta_info->wlc, if_buf, &type, err);
	wlc_set_iface_info(cfg, wl_info, if_buf);
	return (cfg);
}

/*
 * Function:	wlc_sta_iface_remove
 *
 * Purpose:	Function to remove sta interface(s) through interface_remove IOVAR.
 *
 * Input Parameters:
 *	wlc	: Pointer to wlc
 *	bsscfg	: Pointer to bsscfg corresponding to the interface
 *
 * Returns:	BCME_OK - If success
 *		Respective error code on failures.
 */
static int32
wlc_sta_iface_remove(wlc_info_t *wlc, wlc_if_t *wlcif)
{
	int32 ret;
	wlc_bsscfg_t *bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	if (bsscfg == NULL) {
		ret = BCME_NOTFOUND;
		goto done;
	}
	if (bsscfg->subtype != BSSCFG_GENERIC_STA) {
		ret = BCME_NOTSTA;
	} else {
		if (bsscfg->enable) {
			wlc_bsscfg_disable(wlc, bsscfg);
		}
		wlc_bsscfg_free(wlc, bsscfg);
		ret = BCME_OK;
	}
done:
	return (ret);
}

static int
sta_bss_init(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_sta_info_t *sta_info = (wlc_sta_info_t *)ctx;
	wlc_info_t *wlc = sta_info->wlc;
	sta_cfg_cubby_t **ista_cfg_loc;
	sta_cfg_cubby_t *ista_cfg_info;

	if (!(BSSCFG_INFRA_STA(cfg))) {
		return BCME_OK;
	}

	/* allocate cubby info */
	if ((ista_cfg_info = MALLOCZ(wlc->osh, sizeof(sta_cfg_cubby_t))) == NULL) {
		WL_ERROR(("wl%d: %s: MALLOC failed, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return BCME_NOMEM;
	}

	ista_cfg_loc = BSSCFG_STA_CUBBY_LOC(sta_info, cfg);
	*ista_cfg_loc = ista_cfg_info;

	return BCME_OK;
}

static void
sta_bss_deinit(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_sta_info_t *sta_info = (wlc_sta_info_t *)ctx;
	wlc_info_t *wlc = sta_info->wlc;
	sta_cfg_cubby_t *sta_cfg_cubby;

	if (SCAN_IN_PROGRESS(wlc->scan))
		wlc_scan_abort_ex(wlc->scan, cfg, WLC_E_STATUS_ABORT);
	sta_cfg_cubby = BSSCFG_STA_CUBBY(sta_info, cfg);
	if (sta_cfg_cubby != NULL) {
		if (sta_cfg_cubby->msch_req_hdl) {
			wlc_msch_timeslot_unregister(wlc->msch_info,
				&sta_cfg_cubby->msch_req_hdl);
			sta_cfg_cubby->msch_req_hdl = NULL;
		}

		MFREE(wlc->osh, sta_cfg_cubby, sizeof(sta_cfg_cubby_t));
	}
}

int
wlc_sta_timeslot_register(wlc_bsscfg_t *bsscfg)
{
	wlc_info_t *wlc;
	wlc_assoc_t *as;
	sta_cfg_cubby_t *sta_cfg_cubby;
	wlc_msch_req_param_t *req;
	uint32 tbtt_h, tbtt_l;
	uint32 err = 0;
	uint32 time_to_dtim, dtim_period, min_dur;
	uint32 max_away_dur, passive_time;
	uint64 start_time;
	bool prev_awake;
	DBGONLY(char chanbuf[CHANSPEC_STR_LEN]; )

	ASSERT(bsscfg);
	wlc = bsscfg->wlc;

	if (!BSSCFG_STA(bsscfg) || BSSCFG_SPECIAL(wlc, bsscfg)) {
		return BCME_ERROR;
	}

	WL_INFORM(("wl%d.%d: %s: chanspec %s\n", wlc->pub->unit,
		WLC_BSSCFG_IDX(bsscfg), __FUNCTION__,
		wf_chspec_ntoa(bsscfg->current_bss->chanspec, chanbuf)));

	sta_cfg_cubby = BSSCFG_STA_CUBBY(wlc->sta_info, bsscfg);
	ASSERT(sta_cfg_cubby != NULL);

	ASSERT(bsscfg->current_bss);

	req = &sta_cfg_cubby->req;
	memset(req, 0, sizeof(wlc_msch_req_param_t));

	req->req_type = MSCH_RT_BOTH_FLEX;
	req->flags = MSCH_REQ_FLAGS_PREMTABLE;
	req->duration = bsscfg->current_bss->beacon_period;
	req->duration <<= MSCH_TIME_UNIT_1024US;
	req->interval = req->duration;
	req->priority = MSCH_RP_CONNECTION;

	wlc_force_ht(wlc, TRUE, &prev_awake);
	wlc_read_tsf(wlc, &tbtt_l, &tbtt_h);
	wlc_force_ht(wlc, prev_awake, NULL);
#ifdef WLMCNX
	if (MCNX_ENAB(wlc->pub))
		wlc_mcnx_l2r_tsf64(wlc->mcnx, bsscfg, tbtt_h, tbtt_l, &tbtt_h, &tbtt_l);
#endif // endif
	wlc_tsf64_to_next_tbtt64(bsscfg->current_bss->beacon_period, &tbtt_h, &tbtt_l);
#ifdef WLMCNX
	if (MCNX_ENAB(wlc->pub))
		wlc_mcnx_r2l_tsf64(wlc->mcnx, bsscfg, tbtt_h, tbtt_l, &tbtt_h, &tbtt_l);
#endif // endif
	start_time = msch_tsf_to_mschtime(wlc->msch_info, tbtt_l, tbtt_h, &req->start_time_l,
		&req->start_time_h);

	dtim_period = bsscfg->current_bss->dtim_period ? bsscfg->current_bss->dtim_period : 1;

	err = wlc_iovar_op(wlc, "scan_home_time", NULL, 0,
			&min_dur, sizeof(min_dur), IOV_GET, NULL);
	if (err != BCME_OK) {
		min_dur = DEFAULT_HOME_TIME;
	}
	min_dur = MS_TO_USEC(min_dur);

	err = wlc_iovar_op(wlc, "scan_home_away_time", NULL, 0,
			&max_away_dur, sizeof(max_away_dur), IOV_GET, NULL);
	if (err != BCME_OK) {
		max_away_dur = DEFAULT_HOME_AWAY_TIME;
	}
	max_away_dur = MS_TO_USEC(max_away_dur);

	err = wlc_iovar_op(wlc, "scan_passive_time", NULL, 0,
			&passive_time, sizeof(passive_time), IOV_GET, NULL);
	if (err != BCME_OK) {
		passive_time = DEFAULT_SCAN_PASSIVE_TIME;
	}
	passive_time = MS_TO_USEC(passive_time) + MSCH_MIN_ONCHAN_TIME;

	req->flex.bf.min_dur = min_dur;
	req->flex.bf.max_away_dur = MAX(max_away_dur, passive_time) +
		MSCH_EXTRA_DELAY_FOR_MAX_AWAY_DUR;

	/* time_to_dtim from **next** tbtt, since we have used
	 * wlc_tsf64_to_next_tbtt64
	 */
	as = bsscfg->assoc;
	if (as->dtim_count) {
		time_to_dtim = (as->dtim_count - 1) * bsscfg->current_bss->beacon_period;
	} else {
		time_to_dtim = (dtim_period - 1) * bsscfg->current_bss->beacon_period;
	}
	time_to_dtim <<= MSCH_TIME_UNIT_1024US;
	start_time += time_to_dtim;

	req->flex.bf.hi_prio_time_l = (uint32)(start_time & 0xFFFFFFFFU);
	req->flex.bf.hi_prio_time_h = (uint32)(start_time >> 32);
	req->flex.bf.hi_prio_interval = req->duration;
	req->flex.bf.hi_prio_interval *= dtim_period;

	if (as->req_msch_hdl) {
		if (!wlc_sta_timeslot_onchannel(bsscfg)) {
			wlc_txqueue_end(wlc, bsscfg);
		}
		wlc_msch_timeslot_unregister(wlc->msch_info, &as->req_msch_hdl);
		as->req_msch_hdl = NULL;
	}

	if (sta_cfg_cubby->msch_req_hdl) {
		if (sta_cfg_cubby->msch_state == WLC_MSCH_ON_CHANNEL) {
			wlc_sta_prepare_off_channel(wlc, bsscfg);
		}
		wlc_txqueue_end(wlc, bsscfg);
		wlc_msch_timeslot_unregister(wlc->msch_info, &sta_cfg_cubby->msch_req_hdl);
		sta_cfg_cubby->msch_req_hdl = NULL;
	}

	sta_cfg_cubby->msch_state = WLC_MSCH_OFFCHANNEL;
	err = wlc_msch_timeslot_register(wlc->msch_info, &bsscfg->current_bss->chanspec, 1,
		wlc_sta_msch_clbk, bsscfg, req, &sta_cfg_cubby->msch_req_hdl);

	if (err	!= BCME_OK) {
		WL_ERROR(("%s request failed error %d\n", __FUNCTION__, err));
		ASSERT(0);
	} else {
		wlc_msch_set_chansw_reason(wlc->msch_info, sta_cfg_cubby->msch_req_hdl,
			CHANSW_STA);
	}

	return err;
}

void
wlc_sta_timeslot_unregister(wlc_bsscfg_t *bsscfg)
{
	wlc_info_t *wlc;
	sta_cfg_cubby_t *sta_cfg_cubby;
	DBGONLY(char chanbuf[CHANSPEC_STR_LEN]; )

	ASSERT(bsscfg);
	wlc = bsscfg->wlc;

	if (!BSSCFG_STA(bsscfg) || BSSCFG_SPECIAL(wlc, bsscfg)) {
		return;
	}

	/* Ensure that we unregister all home channel MSCH registrations here */
	wlc_assoc_homech_req_unregister(bsscfg);

	WL_INFORM(("wl%d.%d: %s: chanspec %s\n", wlc->pub->unit,
		WLC_BSSCFG_IDX(bsscfg), __FUNCTION__,
		wf_chspec_ntoa(bsscfg->current_bss->chanspec, chanbuf)));

	sta_cfg_cubby = BSSCFG_STA_CUBBY(wlc->sta_info, bsscfg);
	if ((sta_cfg_cubby != NULL) && (sta_cfg_cubby->msch_req_hdl != NULL)) {
		/* If we were in the middle of transitioning to PM state because of this
		 * BSS, then cancel it
		 */
		if (sta_cfg_cubby->msch_state == WLC_MSCH_PM_NOTIF_PEND) {
			wlc_update_pmstate(bsscfg, TX_STATUS_BE);
		}
		sta_cfg_cubby->msch_state = WLC_MSCH_OFFCHANNEL;
		wlc_txqueue_end(wlc, bsscfg);
		wlc_msch_timeslot_unregister(wlc->msch_info, &sta_cfg_cubby->msch_req_hdl);
		sta_cfg_cubby->msch_req_hdl = NULL;

#ifdef WL_DTS
		/* disable DTS tx suppression */
		if (DTS_ENAB(wlc->pub)) {
			wlc_set_cfg_tx_stop_time(bsscfg->wlc, bsscfg, -1);
		}
#endif // endif
	}
}

bool
wlc_sta_timeslot_registed(wlc_bsscfg_t *bsscfg)
{
	wlc_info_t *wlc;
	sta_cfg_cubby_t *sta_cfg_cubby;

	ASSERT(bsscfg);
	wlc = bsscfg->wlc;

	if (!BSSCFG_STA(bsscfg) || BSSCFG_SPECIAL(wlc, bsscfg)) {
		return FALSE;
	}

	sta_cfg_cubby = BSSCFG_STA_CUBBY(wlc->sta_info, bsscfg);
	ASSERT(sta_cfg_cubby != NULL);

	if (sta_cfg_cubby->msch_req_hdl) {
		return TRUE;
	}

	return FALSE;
}

bool
wlc_sta_timeslot_onchannel(wlc_bsscfg_t *bsscfg)
{
	wlc_info_t *wlc;
	sta_cfg_cubby_t *sta_cfg_cubby;

	ASSERT(bsscfg);
	wlc = bsscfg->wlc;

	if (!BSSCFG_STA(bsscfg) || BSSCFG_SPECIAL(wlc, bsscfg)) {
		return FALSE;
	}

	sta_cfg_cubby = BSSCFG_STA_CUBBY(wlc->sta_info, bsscfg);
	ASSERT(sta_cfg_cubby != NULL);

	if (sta_cfg_cubby->msch_req_hdl && sta_cfg_cubby->msch_state != WLC_MSCH_OFFCHANNEL) {
		return TRUE;
	}

	return FALSE;
}

void
wlc_sta_timeslot_update(wlc_bsscfg_t *bsscfg, uint32 start_tsf, uint32 interval)
{
	wlc_info_t *wlc;
	sta_cfg_cubby_t *sta_cfg_cubby;
	wlc_msch_req_param_t *req;
	uint32 update_mask;
	uint32 tsf_l;
	int to;
	uint64 start_time;
	bool prev_awake;
	DBGONLY(char chanbuf[CHANSPEC_STR_LEN]; )

	ASSERT(bsscfg);
	wlc = bsscfg->wlc;

	if (!BSSCFG_STA(bsscfg) || BSSCFG_SPECIAL(wlc, bsscfg)) {
		return;
	}

	sta_cfg_cubby = BSSCFG_STA_CUBBY(wlc->sta_info, bsscfg);
	ASSERT(sta_cfg_cubby != NULL);
	ASSERT(sta_cfg_cubby->msch_req_hdl);

	req = &sta_cfg_cubby->req;

	update_mask = MSCH_UPDATE_START_TIME;
	if (interval != req->interval) {
		update_mask |= MSCH_UPDATE_INTERVAL;
	}

	wlc_force_ht(wlc, TRUE, &prev_awake);
	wlc_read_tsf(wlc, &tsf_l, NULL);
	wlc_force_ht(wlc, prev_awake, NULL);

	to = (int)(start_tsf + MSCH_ONCHAN_PREPARE - tsf_l);
	if (to < MSCH_MIN_ONCHAN_TIME) {
		to = MSCH_MIN_ONCHAN_TIME;
	}

	start_time = msch_future_time(wlc->msch_info, (uint32)to);

	WL_INFORM(("wl%d.%d: %s: chanspec %s, start_tsf %d, tsf %d, to %d,"
		"interval %d\n", wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), __FUNCTION__,
		wf_chspec_ntoa(wlc_get_chanspec(wlc, bsscfg), chanbuf),
		start_tsf, tsf_l, to, interval));

	req->start_time_l = (uint32)(start_time & 0xFFFFFFFFU);
	req->start_time_h = (uint32)(start_time >> 32);
	req->interval = interval;

	wlc_msch_timeslot_update(wlc->msch_info, sta_cfg_cubby->msch_req_hdl, req, update_mask);
}

/** called back by multiple channel scheduler */
static int
wlc_sta_msch_clbk(void* handler_ctxt, wlc_msch_cb_info_t *cb_info)
{
	wlc_bsscfg_t *cfg = (wlc_bsscfg_t *)handler_ctxt;
	wlc_info_t *wlc;
	uint32 type = cb_info->type;
	sta_cfg_cubby_t *sta_cfg_cubby;
	DBGONLY(char chanbuf[CHANSPEC_STR_LEN]; )

	if (!cfg || !cfg->up || !cfg->associated) {
		/* The request has been cancelled, ignore the Clbk */
		return BCME_OK;
	}

	wlc = cfg->wlc;
	if (!BSSCFG_STA(cfg) || BSSCFG_SPECIAL(wlc, cfg)) {
		return BCME_OK;
	}

	sta_cfg_cubby = BSSCFG_STA_CUBBY(wlc->sta_info, cfg);
	ASSERT(sta_cfg_cubby != NULL);

	WL_INFORM(("wl%d.%d: %s: chanspec %s, type 0x%04x\n",
		wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
		wf_chspec_ntoa(cb_info->chanspec, chanbuf), type));

	/* ASSERT START & END combination in same callback */
	ASSERT(((type & (MSCH_CT_ON_CHAN | MSCH_CT_SLOT_START)) == 0) ||
		((type & (MSCH_CT_OFF_CHAN | MSCH_CT_SLOT_END | MSCH_CT_OFF_CHAN_DONE)) == 0));

#ifdef WLMCHAN
	if (MCHAN_ENAB(wlc->pub) && MCHAN_ACTIVE(wlc->pub)) {
		wlc_mchan_msch_clbk(handler_ctxt, cb_info);
	}
#endif /* WLMCHN */

	if (type & MSCH_CT_REQ_START) {
		/* Delete pending assoc slot */
		if (cfg->assoc->req_msch_hdl) {
			if (!wlc_sta_timeslot_onchannel(cfg)) {
				wlc_txqueue_end(wlc, cfg);
			}
			wlc_msch_timeslot_unregister(wlc->msch_info, &cfg->assoc->req_msch_hdl);
			cfg->assoc->req_msch_hdl = NULL;
		}

		/* STA channel can be changed due to Channel switch announcement
		 * In CSA scenario a PHY cal has to be started
		 */
		if (!(sta_cfg_cubby->join_flags & STA_JOIN_FLAGS_NEW_JOIN)) {
#ifdef PHYCAL_CACHING
			phy_chanmgr_create_ctx(WLC_PI(wlc), cb_info->chanspec);
			wlc->last_CSA_chanspec = cb_info->chanspec;
#endif // endif
			/* the full phy cal can take up to 90msec. */
			wlc_full_phy_cal(wlc, cfg, PHY_PERICAL_JOIN_BSS);

			/* register SCAN TimeSlot calling wlc_reassoc after lengthy STA CLBK func */
			if (cfg->assoc->flags & AS_F_DO_CHANSWITCH) {
				wl_reassoc_params_t assoc_params;

				memset(&assoc_params, 0, sizeof(wl_reassoc_params_t));

				/* If we got disconnected by now try
				 * non-directeed scan.
				 */
				assoc_params.bssid = (ETHER_ISNULLADDR(&cfg->BSSID)) ?
					ether_bcast:cfg->BSSID;
				assoc_params.chanspec_num = 1;
				assoc_params.chanspec_list[0] = wlc_get_home_chanspec(cfg);
				wlc_reassoc(cfg, &assoc_params);
			}
		}
		sta_cfg_cubby->join_flags &= ~STA_JOIN_FLAGS_NEW_JOIN;
		/* Reset the msch state */
		sta_cfg_cubby->msch_state = WLC_MSCH_REQ_START;
	}

	if (type & MSCH_CT_ON_CHAN) {
		if (sta_cfg_cubby->msch_state != WLC_MSCH_ON_CHANNEL) {
#ifdef WL_DTS
			if (DTS_ENAB(wlc->pub)) {
				wlc_set_cfg_tx_stop_time(wlc, cfg,
					msch_calc_slot_duration(wlc->msch_info, cb_info));
			}
#endif // endif
			sta_cfg_cubby->msch_state = WLC_MSCH_ON_CHANNEL;
			wlc_txqueue_start(wlc, cfg, cb_info->chanspec);
			wlc_sta_return_home_channel(cfg);
			/* On scan completion, get into ISM state if home channel is DFS */
			if (!SCAN_IN_PROGRESS(wlc->scan) &&
					WL11H_STA_ENAB(wlc) &&
					wlc_radar_chanspec(wlc->cmi, cb_info->chanspec) &&
					wlc_dfs_valid_ap_chanspec(wlc, cb_info->chanspec)) {
				wlc_set_dfs_cacstate(wlc->dfs, ON, cfg);
			}
		}
		return BCME_OK;
	}

	if ((type & MSCH_CT_SLOT_END) || (type & MSCH_CT_OFF_CHAN)) {
		if (!(type & MSCH_CT_SLOT_CONTINUE) &&
			(sta_cfg_cubby->msch_state == WLC_MSCH_ON_CHANNEL)) {
			sta_cfg_cubby->msch_state = WLC_MSCH_OFFCHANNEL_PREP;
			wlc_sta_prepare_off_channel(wlc, cfg);
			if (wlc_sta_prepare_pm_mode(cfg)) {
				WL_INFORM(("wl%d.%d: %s: waiting for PM indication to"
					"complete\n", cfg->wlc->pub->unit,
					WLC_BSSCFG_IDX(cfg), __FUNCTION__));
			}
		}
		return BCME_OK;
	}

	if (type & MSCH_CT_OFF_CHAN_DONE) {
		if ((!(type & MSCH_CT_SLOT_CONTINUE)) ||
			(sta_cfg_cubby->msch_state != WLC_MSCH_ON_CHANNEL)) {
			if (sta_cfg_cubby->msch_state == WLC_MSCH_PM_NOTIF_PEND) {
				wlc_update_pmstate(cfg, TX_STATUS_BE);
			}
			sta_cfg_cubby->msch_state = WLC_MSCH_OFFCHANNEL;
			wlc_txqueue_end(wlc, cfg);
			wlc_sta_off_channel_done(wlc, cfg);
#ifdef WL_DTS
			if (DTS_ENAB(wlc->pub)) {
				wlc_set_cfg_tx_stop_time(wlc, cfg, -1);
			}
#endif // endif
		return BCME_OK;
		}
	}

	if (type & MSCH_CT_REQ_END) {
		/* The msch hdl is no more valid */
		WL_INFORM(("wl%d.%d: %s: The msch hdl is no more valid\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
		sta_cfg_cubby->msch_req_hdl = NULL;
		return BCME_OK;
	}

	if (type & MSCH_CT_SLOT_START) {
		/* TBD : pre-tbtt handling */
		return BCME_OK;
	}
	if (type & MSCH_CT_SLOT_SKIP) {
		/* Do Nothing for STA */
		return BCME_OK;
	}

	return BCME_OK;
}

/* prepare to leave home channel */
static void
wlc_sta_prepare_off_channel(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	WL_INFORM(("wl%d.%d: %s\n", wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));

	if (BSSCFG_IBSS(cfg)) {
		wlc_ibss_disable(cfg);
		wlc_ap_mute(wlc, TRUE, cfg, -1);
	}

#ifdef PROP_TXSTATUS
	if (PROP_TXSTATUS_ENAB(wlc->pub)) {
		wlc_wlfc_mchan_interface_state_update(wlc, cfg,
			WLFC_CTL_TYPE_INTERFACE_CLOSE, FALSE);
	}
#endif /* PROP_TXSTATUS */
}

/* Indicate PM 1 to associated APs.
 * return TRUE if completion is pending; FALSE otherwise.
 */
static bool
wlc_sta_prepare_pm_mode(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	sta_cfg_cubby_t *sta_cfg_cubby = BSSCFG_STA_CUBBY(wlc->sta_info, cfg);
	bool quiet_channel = FALSE;
	DBGONLY(char chanbuf[CHANSPEC_STR_LEN]; )

	WL_INFORM(("wl%d.%d: %s: chanspec %s pm_pending: %d\n",
	          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
	          wf_chspec_ntoa(cfg->current_bss->chanspec, chanbuf),
	          wlc->PMpending));

	ASSERT(sta_cfg_cubby);

	if (WL11H_ENAB(wlc) &&
		wlc_quiet_chanspec(wlc->cmi, cfg->current_bss->chanspec)) {
		quiet_channel = TRUE;
	}

	if (wlc->exptime_cnt == 0 && !quiet_channel) {
		/* block any PSPoll operations for holding off AP traffic */
#if defined(WLMCHAN) && defined(WLP2P)
		if (MCHAN_ENAB(wlc->pub) && MCHAN_ACTIVE(wlc->pub) &&
			/* check for NOA on P2P GC. If yes, lock as WLC_PM_BLOCK_MCHAN_ABS */
			wlc_p2p_noa_valid(wlc->p2p, cfg))
			mboolset(cfg->pm->PMblocked, WLC_PM_BLOCK_MCHAN_ABS);
		else
#endif /* WLMCHAN & WLP2P */
			mboolset(cfg->pm->PMblocked, WLC_PM_BLOCK_CHANSW);

		if (cfg->associated && !cfg->pm->PMenabled &&
			BSSCFG_STA(cfg) && cfg->BSS) {
			WL_PS(("wl%d.%d %s: wlc_infra_sta_prepare_pm_mode: "
				"entering PM mode\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
#ifdef WLMCHAN
			if (MCHAN_ACTIVE(wlc->pub) &&
				(wlc_mchan_curr_idx(wlc->mchan) != WLC_BSSCFG_IDX(cfg))) {
				/* we can't go to PS now as channel has already been switched
				* no need to hold ucode wake for a fake power save notif tx.
				*/
			} else {
#endif /* WLMCHAN */
			WL_INFORM(("wl%d.%d: %s: "
				"entering PM mode for off channel operation for %s\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
				wf_chspec_ntoa(cfg->current_bss->chanspec, chanbuf)));

			if (cfg->pm->PM == PM_FAST) {
				/* Stop the pm2 timer as traffic is there */
				wlc_pm2_sleep_ret_timer_stop(cfg);
			}
			wlc_module_set_pmstate(cfg, TRUE, WLC_BSSCFG_PS_REQ_CHANSW);
#ifdef WLMCHAN
			}
#endif /* WLMCHAN */
		}
	}

	/* PSPEND completed immediately */
	if (sta_cfg_cubby->msch_state != WLC_MSCH_PM_NOTIF_PEND) {
		WL_PS(("wl%d.%d: %s: PM completed.\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));

		return FALSE;
	}

#ifdef WL_LEAKY_AP_STATS
	if (WL_LEAKYAPSTATS_ENAB(wlc->pub)) {
		wlc_leakyapstats_gt_reason_upd(wlc, cfg, WL_LEAKED_GUARD_TIME_INFRA_STA);
	}
#endif /* WL_LEAKY_AP_STATS */
	return TRUE;
}

void
wlc_sta_set_pmpending(wlc_info_t *wlc, wlc_bsscfg_t *cfg, bool pmpending)
{
	sta_cfg_cubby_t *sta_cfg_cubby = BSSCFG_STA_CUBBY(wlc->sta_info, cfg);

	if (!BSSCFG_STA(cfg) || BSSCFG_SPECIAL(wlc, cfg)) {
		return;
	}

	ASSERT(sta_cfg_cubby);

	WL_INFORM(("wl%d.%d: %s: PM pending %d\n", wlc->pub->unit,
		WLC_BSSCFG_IDX(cfg), __FUNCTION__, pmpending));

	if (sta_cfg_cubby->msch_state == WLC_MSCH_OFFCHANNEL_PREP ||
		sta_cfg_cubby->msch_state == WLC_MSCH_PM_NOTIF_PEND) {
		if (pmpending)
			sta_cfg_cubby->msch_state = WLC_MSCH_PM_NOTIF_PEND;
		else
			sta_cfg_cubby->msch_state = WLC_MSCH_PM_DONE;
	}
}

void
wlc_sta_pm_pending_complete(wlc_info_t *wlc)
{
	sta_cfg_cubby_t *sta_cfg_cubby;
	int idx;
	wlc_bsscfg_t *cfg;

	WL_INFORM(("wl%d: %s\n", wlc->pub->unit, __FUNCTION__));

	FOREACH_AS_STA(wlc, idx, cfg) {
		sta_cfg_cubby = BSSCFG_STA_CUBBY(wlc->sta_info, cfg);

		if (!BSSCFG_STA(cfg) || BSSCFG_SPECIAL(wlc, cfg)) {
			continue;
		}

		ASSERT(sta_cfg_cubby);

		/* Skip the ones that are not on curr channel */
		if (!wlc_shared_current_chanctxt(wlc, cfg)) {
			WL_INFORM(("wl%d.%d: %s: Skip the ones that are not on curr channel\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
			continue;
		}

		if (sta_cfg_cubby->msch_state == WLC_MSCH_OFFCHANNEL_PREP ||
			sta_cfg_cubby->msch_state == WLC_MSCH_PM_DONE ||
			sta_cfg_cubby->msch_state == WLC_MSCH_PM_NOTIF_PEND) {
			sta_cfg_cubby->msch_state = WLC_MSCH_PM_DONE;
		}
	}
}

static void
wlc_sta_return_home_channel(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
#ifdef WLMCHAN
	int btc_flags = wlc_bmac_btc_flags_get(wlc->hw);
	uint16 protections = 0;
	uint16 ps = 0;
	uint16 active = 0;
#endif /* WLMCHAN */

	WL_INFORM(("wl%d.%d: %s\n", wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));

#ifdef PROP_TXSTATUS
	if (PROP_TXSTATUS_ENAB(wlc->pub)) {
#if defined(WLMCHAN)
		bool if_state_upd_open = !mboolisset(cfg->pm->PMblocked, WLC_PM_BLOCK_MCHAN_ABS);

		if (if_state_upd_open)
#endif /* WLMCHAN */
		/* Open the active interface so that host
		 * start sending pkts to dongle
		 */
		wlc_wlfc_mchan_interface_state_update(wlc,
			cfg, WLFC_CTL_TYPE_INTERFACE_OPEN, FALSE);
	}
#endif /* PROP_TXSTATUS */

	wlc_suspend_mac_and_wait(wlc);

	if (BSSCFG_IBSS(cfg)) {
		wlc_ibss_enable(cfg);
		wlc_ap_mute(wlc, FALSE, cfg, -1);
	}

#ifdef PROP_TXSTATUS
	if (PROP_TXSTATUS_ENAB(wlc->pub)) {
#ifdef WLAMPDU
		/* send_bar on interface being opened */
		if (AMPDU_ENAB(wlc->pub)) {
			struct scb_iter scbiter;
			struct scb *scb = NULL;

			FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {
				wlc_ampdu_send_bar_cfg(wlc->ampdu_tx, scb);
			}
		}
#endif /* WLAMPDU */
		wlfc_sendup_ctl_info_now(wlc->wlfc);
	}
#endif /* PROP_TXSTATUS */

#ifdef WLMCHAN
	/*
	 * Setup BTCX protection mode according to the BSS that is
	 * being switched in.
	 */
	if (btc_flags & WL_BTC_FLAG_ACTIVE_PROT)
		active = MHF3_BTCX_ACTIVE_PROT;
	ps = (btc_flags & WL_BTC_FLAG_PS_PROTECT) ? MHF3_BTCX_PS_PROTECT : 0;

	if (cfg->BSS) {
		protections = ps | active;
	}
#endif /* WLMCHAN */

#ifdef WLMCNX
	if (MCNX_ENAB(wlc->pub)) {
		int bss_idx;
		/* Set the current BSS's Index in SHM (bits 10:8 of M_P2P_GO_IND_BMP_OFFSET
		   contain the current BSS Index
		 */
		bss_idx = wlc_mcnx_BSS_idx(wlc->mcnx, cfg);
		wlc_mcnx_shm_bss_idx_set(wlc->mcnx, bss_idx);
	}
#endif /* WLMCNX */
#ifdef WLMCHAN
	if (MCHAN_ACTIVE(wlc->pub))
		wlc_mhf(wlc, MHF3, MHF3_BTCX_ACTIVE_PROT | MHF3_BTCX_PS_PROTECT,
			protections, WLC_BAND_2G);
	else
#endif /* WLMCHAN */
		wlc_btc_set_ps_protection(wlc, cfg); /* enable */

	wlc_enable_mac(wlc);

	/* Update PS modes for clients on new channel if their
	* user settings so desire
	*/
	if (!mboolisset(cfg->pm->PMblocked, WLC_PM_BLOCK_MCHAN_ABS)) {
		/* PM unblock any cfg with chan context that matches chan_ctxt */
		mboolclr(cfg->pm->PMblocked, WLC_PM_BLOCK_CHANSW);

		if (cfg->associated) {
			/* come out of PS mode if appropriate */
			/* come out of PS if last time STA did not 'complete' the transition */
			/* Logic is as follows:
			 * pmenabled is true &&
			 *	1. PM mode is OFF /PM_FAST or
			 *	2. WME PM blocked or
			 *	3. bypass_pm feature is on
			 */
			if ((cfg->pm->PM == PM_OFF || cfg->pm->PM == PM_FAST ||
#ifdef WLMCHAN
				(MCHAN_ENAB(wlc->pub) && wlc_mchan_bypass_pm(wlc->mchan)) ||
#endif /* WLMCHAN */
				cfg->pm->WME_PM_blocked) && cfg->pm->PMenabled) {
				if (cfg->pm->PM == PM_FAST &&
					mboolisset(cfg->pm->PMenabledModuleId,
					WLC_BSSCFG_PS_REQ_CHANSW)) {
					/* inform AP that STA's is ready for receiving
					 * traffic without waiting for beacon and start the
					 * pm2 timer using remaining idle time accounting
					 * from absolute to on-channel time.
					 */
					wlc_bcn_tim_ie_pm2_action(wlc, cfg);
				} else {
					wlc_set_pmstate(cfg, FALSE);
				}
			}
		}

		wlc_set_wake_ctrl(wlc);
	} else {
		/* GC cfg is still in ABS, cannot bring it out of PS.
		* Simply return
		*/
	}
} /* wlc_sta_return_home_channel */

/*  We are off home channel now */
static void
wlc_sta_off_channel_done(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
#ifdef PROP_TXSTATUS
	WL_INFORM(("wl%d.%d: %s\n", wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));

	if (PROP_TXSTATUS_ENAB(wlc->pub)) {
		wlc_wlfc_mchan_interface_state_update(wlc, cfg,
			WLFC_CTL_TYPE_INTERFACE_CLOSE, FALSE);
		wlc_wlfc_flush_pkts_to_host(wlc, cfg);
		wlfc_sendup_ctl_info_now(wlc->wlfc);
	}
#endif /* PROP_TXSTATUS */

#ifdef WL_LEAKY_AP_STATS
	if (WL_LEAKYAPSTATS_ENAB(wlc->pub)) {
		wlc_leakyapstats_gt_event(wlc, cfg);
	}
#endif /* WL_LEAKY_AP_STATS */
}

int
wlc_sta_get_infra_bcn(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, void *buf, uint *len)
{
	struct dot11_management_header *hdr;
	wlc_bss_info_t *current_bss = bsscfg->current_bss;

	ASSERT(current_bss);

	if (!bsscfg->associated) {
		return BCME_NOTASSOCIATED;
	}

	if (!bsscfg->assoc || bsscfg->assoc->state == AS_WAIT_RCV_BCN) {
		return BCME_NOTREADY;
	}

	if (ETHER_ISNULLADDR(&current_bss->BSSID) ||
	    !current_bss->bcn_prb || !current_bss->bcnlen) {
		return BCME_NOTFOUND;
	}

	if (*len < (current_bss->bcnlen + (uint)sizeof(*hdr))) {
		return BCME_BUFTOOSHORT;
	}

	/* Construct beacon frame from original content */
	*len = current_bss->bcnlen + sizeof(*hdr);
	hdr = (struct dot11_management_header *)buf;
	hdr->fc = htol16((uint16)FC_BEACON);
	hdr->durid = 0;

	memset(&hdr->da, 0xff, sizeof(hdr->da));
	memcpy(&hdr->sa, &current_bss->BSSID, sizeof(hdr->sa));
	memcpy(&hdr->bssid, &current_bss->BSSID, sizeof(hdr->bssid));
	hdr->seq = 0;
	memcpy((uint8*)buf + sizeof(*hdr), current_bss->bcn_prb, current_bss->bcnlen);

	return BCME_OK;
}

void
wlc_sta_update_bw(wlc_info_t *wlc,
	wlc_bsscfg_t *bsscfg, chanspec_t old_chanspec, chanspec_t chanspec)
{
	sta_cfg_cubby_t *sta_cfg_cubby = BSSCFG_STA_CUBBY(wlc->sta_info, bsscfg);
	ASSERT(sta_cfg_cubby != NULL);
	if (sta_cfg_cubby->msch_req_hdl == NULL) {
		return;
	}
	msch_update_bw(wlc->msch_info, sta_cfg_cubby->msch_req_hdl, old_chanspec, chanspec);
}

/* wrapper API to check if the STA is operating on its own channel */
bool
wlc_sta_msch_check_on_chan(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	sta_cfg_cubby_t *sta_cfg_cubby;
	sta_cfg_cubby = BSSCFG_STA_CUBBY(wlc->sta_info, cfg);

	/* check the status only if the cubby is valid */
	if (sta_cfg_cubby && (sta_cfg_cubby->msch_state == WLC_MSCH_ON_CHANNEL))
		return TRUE;

	return FALSE;
}
