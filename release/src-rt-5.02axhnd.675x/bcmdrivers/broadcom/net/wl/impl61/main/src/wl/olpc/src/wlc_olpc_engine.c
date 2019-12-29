/*
 * Open Loop phy calibration SW module for
 * Broadcom 802.11bang Networking Device Driver
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
 * $Id: wlc_olpc_engine.c 377976 2013-01-23 20:57:10Z $
 */

#include <wlc_cfg.h>
#ifdef WLOLPC
#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmdevs.h>
#include <pcicfg.h>
#include <siutils.h>
#include <bcmendian.h>
#include <nicpci.h>
#include <wlioctl.h>
#include <pcie_core.h>
#include <sbhnddma.h>
#include <hnddma.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_hw.h>
#include <wlc_hw_priv.h>
#include <wlc_bmac.h>
#include <wlc_scb.h>
#include <wl_export.h>
#include <wl_dbg.h>
#include <wlc_olpc_engine.h>
#include <wlc_channel.h>
#include <wlc_stf.h>
#include <wlc_scan.h>
#include <wlc_rm.h>
#include <wlc_pcb.h>
#include <wlc_tx.h>
#include <wlc_rspec.h>
#include <wlc_assoc.h>
#include <phy_tpc_api.h>
#include <phy_tssical_api.h>
#if (defined(PHYCAL_CACHING) || defined(WLMCHAN))
#include <phy_chanmgr_api.h>
#include <phy_cache_api.h>
#endif /* (defined(PHYCAL_CACHING) || defined(WLMCHAN)) */
#include <wlc_dump.h>
#include <wlc_ratelinkmem.h>
#include <wlc_rate_sel.h>
#include <wlc_keymgmt.h>

#define WLC_OLPC_DEF_NPKTS 16
#define WL_OLPC_PKT_LEN (sizeof(struct dot11_header)+1)

#if defined(BCMDBG) || defined(WLTEST)
#define OLPC_MSG_LVL_IMPORTANT 1
#define OLPC_MSG_LVL_CHATTY 2
#define OLPC_MSG_LVL_ENTRY 4
#define WL_OLPC(olpc, args) if (olpc && (olpc->msglevel & \
	OLPC_MSG_LVL_IMPORTANT) != 0) WL_PRINT(args)
#define WL_OLPC_DBG(olpc, args) if (olpc && (olpc->msglevel & \
	OLPC_MSG_LVL_CHATTY) != 0) WL_PRINT(args)
#define WL_OLPC_ENTRY(olpc, args) if (olpc && (olpc->msglevel & \
		OLPC_MSG_LVL_ENTRY) != 0) WL_PRINT(args)
#else
#define WL_OLPC(olpc, args)
#define WL_OLPC_DBG(olpc, args)
#define WL_OLPC_ENTRY(olpc, args)
#endif /* defined(BCMDBG) || defined(WLTEST) */
/* max num of cores supported */
#define WL_OLPC_MAX_NUM_CORES 4
/* max valueof core mask with one core is 0x4 */
#define WL_OLPC_MAX_CORE 8

/* index for shmem access */
#define WL_OLPC_TXPWRIDX (D11AX_TXC_TXPWRIDX_0 * 2)

/* Need to shave 1.5 (6 coz units are .25 dBm) off ppr power to comp TSSI pwr */
#define WL_OLPC_PPR_TO_TSSI_PWR(pwr) (pwr - 6)

#define WLC_TO_CHANSPEC(wlc) (wlc->chanspec)
#define WLC_OLPC_INVALID_TXCHAIN 0xff

#ifndef WL_OLPC_IOVARS_ENAB
#if defined(BCMDBG) || defined(WLTEST) || defined(WLOLPC)
#define WL_OLPC_IOVARS_ENAB 1
#else
#define WL_OLPC_IOVARS_ENAB 0
#endif /* BCMDBG || WLTEST */
#endif /* WL_OLPC_IOVARS_ENAB */

enum {
	IOV_OLPC = 0,
	IOV_OLPC_CAL_ST,
	IOV_OLPC_FORCE_CAL,
	IOV_OLPC_MSG_LVL,
	IOV_OLPC_CHAN,
	IOV_OLPC_CAL_PKTS,
	IOV_OLPC_CHAN_DBG,
	IOV_OLPC_OVERRIDE,
	IOV_OLPC_CAL_DONE,
	IOV_OLPC_LAST
};

static const bcm_iovar_t olpc_iovars[] = {
	{"olpc", IOV_OLPC, 0, 0, IOVT_UINT32, 0},
	{"olpc_cal_mask", IOV_OLPC_CAL_ST, IOVF_SET_UP | IOVF_GET_UP, 0, IOVT_UINT32, 0},
	{"olpc_cal_force", IOV_OLPC_FORCE_CAL, IOVF_GET_UP, 0, IOVT_UINT32, 0},
	{"olpc_msg_lvl", IOV_OLPC_MSG_LVL, 0, 0, IOVT_UINT32, 0},
	{"olpc_chan", IOV_OLPC_CHAN, IOVF_GET_UP, 0, IOVT_UINT32, 0},
	{"olpc_cal_pkts_mask", IOV_OLPC_CAL_PKTS, IOVF_GET_UP, 0, IOVT_UINT32, 0},
	{"olpc_chan_dbg", IOV_OLPC_CHAN_DBG, 0, 0, IOVT_UINT32, 0},
	{"olpc_chan_override", IOV_OLPC_OVERRIDE, 0, 0, IOVT_UINT32, 0},
	{"olpc_cal_done", IOV_OLPC_CAL_DONE, 0, 0, IOVT_UINT32, 0},
	{NULL, 0, 0, 0, 0, 0}
};

typedef struct wlc_olpc_eng_chan {
	struct wlc_olpc_eng_chan *next;
	chanspec_t cspec; /* chanspec, containing chan number the struct refers to */
	uint8 pkts_sent[WL_OLPC_MAX_NUM_CORES];
	uint8 cores_cal; /* which cores have been calibrated */
	uint8 cores_cal_active; /* which cores have active cal */
	uint8 cores_cal_to_cmplt; /* cores we just calibrated */
	uint8 cal_pkts_outstanding; /* pkts outstanding on the chan */
#if WL_OLPC_IOVARS_ENAB
	uint8 cores_cal_pkts_sent; /* mask of cal pkts sent  and txstatus cmplted */
	bool dbg_mode; /* TRUE: ignore cal cache b4 telling phy cal is done; FALSE:  don't ignore */
#endif /* WL_OLPC_IOVARS_ENAB */
} wlc_olpc_eng_chan_t;

struct wlc_olpc_eng_info_t {
	wlc_info_t *wlc;
	/* status info */
	wlc_olpc_eng_chan_t *chan_list;
	bool up;
	bool updn; /* bsscfg updown cb registered */

	/* configuration */
	uint8 npkts; /* number of pkts used to calibrate per core */
	uint8 num_hw_cores; /* save this value to avoid repetitive calls BITSCNT */

	/* txcore mgmt */
	bool restore_perrate_stf_state;
	wlc_stf_txchain_st stf_saved_perrate;

	/* OLPC PPR cache */
	uint8 last_txchain_chk; /* txchain used for last ppr query */
	chanspec_t last_chan_chk; /* chanspec used for last ppr query */
	bool last_chan_olpc_state; /* cache result of ppr query */

	/* track our current channel - detect channel changes */
	wlc_olpc_eng_chan_t* cur_chan;

	uint8 msglevel;
	bool assoc_notif_reg; /* bsscfg assoc state chg notif reg */

	uint16 shmem_save;
	uint8 olpc_override; /* TRUE: do olpc cal, without chan info */
	uint8 olpc_cal_done; /* TRUE: olpc cal is done */
	uint last_txpwr_user_target; /* used by WL_OLPC */
	struct wl_timer *cal_timer;	/* timer for calibration operations */
	bool timer_active;		/* cal timer is active */
	uint8 core_idx; /* current active calibration core */
};

/* static functions and internal defines */
#define WLC_TO_CHANSPEC(wlc) (wlc->chanspec)

/* use own mac addr for src and dest */
#define WLC_MACADDR(wlc) (char*)(wlc->pub->cur_etheraddr.octet)

static bool
wlc_olpc_chan_needs_cal(wlc_olpc_eng_info_t *olpc, wlc_olpc_eng_chan_t* chan);

static uint8
wlc_olpc_chan_num_cores_cal_needed(wlc_info_t *wlc, wlc_olpc_eng_chan_t* chan);

static int
wlc_olpc_send_dummy_pkts(wlc_info_t *wlc,
	wlc_olpc_eng_chan_t* chan, uint8 npkts);

static int wlc_olpc_eng_recal_ex(wlc_info_t *wlc, uint8 npkts);

static int wlc_olpc_stf_override(wlc_info_t *wlc, uint8 core_idx);

static void wlc_olpc_stf_perrate_changed(wlc_info_t *wlc);

/* EVENT: one calibration packet just completed tx */
static void wlc_olpc_eng_pkt_complete(wlc_info_t *wlc, void *pkt, uint txs);

static wlc_olpc_eng_chan_t*
wlc_olpc_get_chan(wlc_info_t *wlc, chanspec_t cspec, int *err);

static wlc_olpc_eng_chan_t*
wlc_olpc_get_chan_ex(wlc_info_t *wlc, chanspec_t cspec, int *err, bool create);

static INLINE bool
wlc_olpc_chan_needs_olpc(wlc_olpc_eng_info_t *olpc, chanspec_t cspec);

static void wlc_olpc_chan_terminate_active_cal(wlc_info_t *wlc,
	wlc_olpc_eng_chan_t* cur_chan);

static int wlc_olpc_eng_terminate_cal(wlc_info_t *wlc);

static bool wlc_olpc_chan_has_active_cal(wlc_olpc_eng_info_t *olpc,
	wlc_olpc_eng_chan_t* chan);
static int wlc_olpc_stf_override_revert(wlc_info_t *wlc);

static bool wlc_olpc_eng_ready(wlc_olpc_eng_info_t *olpc_info);

static int wlc_olpc_eng_hdl_chan_update_ex(wlc_info_t *wlc, uint8 npkts, bool force_cal);

static int wlc_olpc_eng_up(void *hdl);

static int wlc_olpc_eng_down(void *hdl);

static void wlc_olpc_bsscfg_updn(void *ctx, bsscfg_up_down_event_data_t *evt);
static void
wlc_olpc_bsscfg_assoc_state_notif(void *arg, bss_assoc_state_data_t *notif_data);

#if defined(BCMDBG) || defined(WLTEST)
static void wlc_olpc_dump_channels(wlc_olpc_eng_info_t *olpc_info, struct bcmstrbuf *b);
static int wlc_dump_olpc(wlc_info_t *wlc, struct bcmstrbuf *b);
#endif // endif

static int wlc_olpc_get_min_2ss3ss_sdm_pwr(wlc_olpc_eng_info_t *olpc_info,
	ppr_t *txpwr, chanspec_t channel);

static bool wlc_olpc_in_cal_prohibit_state(wlc_info_t *wlc);

static int wlc_olpc_chan_rm(wlc_olpc_eng_info_t *olpc,
	wlc_olpc_eng_chan_t* chan);

static bool
wlc_olpc_find_other_up_on_chan(wlc_info_t *wlc, wlc_bsscfg_t *exclude);

static void
wlc_olpc_cal_timer(void *arg);

static uint8
wlc_olpc_find_next_active_core(wlc_olpc_eng_info_t *olpc);

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

/* debug timer used in OLPC module - uncomment the following line */
/* #define DEBUG_OLPC_TIMER */
#ifdef DEBUG_OLPC_TIMER
static void
wlc_olpc_add_timer_dbg(wlc_olpc_eng_info_t *olpc, uint to, bool prd, const char *fname, int line)
{
	WL_OLPC_DBG(olpc, ("wl%d: %s(%d): wl_add_timer: timeout %u tsf %u\n",
		olpc->wlc->pub->unit, fname, line, to,
		R_REG(olpc->wlc->osh, D11_TSFTimerLow(olpc->wlc))));
	wl_add_timer(olpc->wlc->wl, olpc->cal_timer, to, prd);
}

static bool
wlc_olpc_del_timer_dbg(wlc_olpc_eng_info_t *olpc, const char *fname, int line)
{
	WL_OLPC_DBG(olpc, ("wl%d: %s(%d): wl_del_timer: tsf %u\n",
		olpc->wlc->pub->unit, fname, line,
		R_REG(olpc->wlc->osh, D11_TSFTimerLow(olpc->wlc))));
	return wl_del_timer(olpc->wlc->wl, olpc->cal_timer);
}
#define WLC_OLPC_ADD_TIMER(olpc, to, prd) \
	wlc_olpc_add_timer_dbg(olpc, to, prd, __FUNCTION__, __LINE__)
#define WLC_OLPC_DEL_TIMER(olpc) \
	wlc_olpc_del_timer_dbg(olpc, __FUNCTION__, __LINE__)
#else /* DEBUG_OLPC_TIMER */
#define WLC_OLPC_ADD_TIMER(olpc, to, prd) \
	wl_add_timer((olpc)->wlc->wl, (olpc)->cal_timer, (to), (prd))
#define WLC_OLPC_DEL_TIMER(olpc) wl_del_timer((olpc)->wlc->wl, (olpc)->cal_timer)
#endif /* DEBUG_OLPC_TIMER */
#define WLC_OLPC_FREE_TIMER(olpc)	wl_free_timer((olpc)->wlc->wl, (olpc)->cal_timer)

/* check if wlc is in state which prohibits calibration */
static bool wlc_olpc_in_cal_prohibit_state(wlc_info_t *wlc)
{
	return (SCAN_IN_PROGRESS(wlc->scan) || WLC_RM_IN_PROGRESS(wlc));
}

int
wlc_olpc_get_cal_state(wlc_olpc_eng_info_t *olpc)
{
	int err;
	int cores_cal = -1;
	wlc_info_t *wlc	= olpc->wlc;
	wlc_olpc_eng_chan_t* chan_info = NULL;
	chanspec_t cspec = wlc->chanspec;

	if (wlc_olpc_chan_needs_olpc(olpc, wlc->chanspec)) {
		chan_info = wlc_olpc_get_chan_ex(wlc, cspec, &err, FALSE);
		if (chan_info) {
			cores_cal = chan_info->cores_cal;
		} else {
			cores_cal = 0;
		}
	}

	return cores_cal;
}

static void
wlc_olpc_bsscfg_assoc_state_notif(void *arg, bss_assoc_state_data_t *notif_data)
{
	if (notif_data->state == AS_JOIN_ADOPT) {
		wlc_olpc_eng_info_t *olpc = (wlc_olpc_eng_info_t *)arg;

		WL_OLPC_DBG(olpc,
			("%s: JOIN_ADOPT state\n", __FUNCTION__));
		wlc_olpc_eng_hdl_chan_update(olpc);
	}
}

static void wlc_olpc_bsscfg_updn(void *ctx, bsscfg_up_down_event_data_t *evt)
{
	/* seeing a bsscfg up or down */
	wlc_olpc_eng_info_t *olpc = (wlc_olpc_eng_info_t *)ctx;
	wlc_info_t* wlc;
	wlc_olpc_eng_chan_t* chan;
	chanspec_t cspec;
	int err;
	ASSERT(ctx != NULL);
	ASSERT(evt != NULL);
	WL_OLPC_ENTRY(olpc, ("Entry:%s\n", __FUNCTION__));

	wlc = olpc->wlc;
	/* if any bsscfg just went up, then force calibrate on channel */
	if (evt->up) {
		WL_OLPC_ENTRY(olpc, ("%s: cfg (%p) up on chan now\n",
			__FUNCTION__, OSL_OBFUSCATE_BUF(evt->bsscfg)));
		wlc_olpc_eng_hdl_chan_update_ex(olpc->wlc, olpc->npkts, TRUE);
	} else if (!wlc_olpc_find_other_up_on_chan(olpc->wlc, evt->bsscfg)) {
		WL_OLPC_ENTRY(olpc, ("No cfg up on chan now:%s\n", __FUNCTION__));
		/* going down -- free unused channel info */
		if (evt->bsscfg->current_bss) {
			cspec = evt->bsscfg->current_bss->chanspec;
			chan = wlc_olpc_get_chan_ex(wlc, cspec, &err, FALSE);
			if (chan) {
				WL_OLPC_ENTRY(olpc, ("%s: FREEing chan struct\n", __FUNCTION__));
				wlc_olpc_chan_rm(olpc, chan);
				WL_OLPC_DBG(olpc, ("%s: calling olpc_eng_reset\n", __FUNCTION__));
				wlc_olpc_eng_reset(olpc);
			}
		}
	}

	WL_OLPC_ENTRY(olpc, ("Exit:%s\n", __FUNCTION__));
}

static void
wlc_olpc_chan_terminate_active_cal(wlc_info_t *wlc, wlc_olpc_eng_chan_t* cur_chan)
{
	wlc_olpc_stf_override_revert(wlc);
	if (cur_chan) {
		bzero(cur_chan->pkts_sent, sizeof(cur_chan->pkts_sent));
		cur_chan->cores_cal_active = 0;
		/* if we hadn't yet saved cal result, forget we calibrated */
		cur_chan->cores_cal &= ~cur_chan->cores_cal_to_cmplt;
		cur_chan->cores_cal_to_cmplt = 0;
#if WL_OLPC_IOVARS_ENAB
		cur_chan->cores_cal_pkts_sent = 0;
#endif /* WL_OLPC_IOVARS_ENAB */
	}
	/* delete any timer */
	WLC_OLPC_DEL_TIMER(wlc->olpc_info);
	/* make sure core index is reset, in case of early termination/restarting cal */
	wlc->olpc_info->core_idx = 0;
	wlc->olpc_info->timer_active = FALSE;
}

static int
wlc_olpc_eng_terminate_cal(wlc_info_t *wlc)
{
	struct wlc_olpc_eng_info_t* olpc_info = wlc->olpc_info;
	WL_OLPC_DBG(wlc->olpc_info, ("%s: Entry\n", __FUNCTION__));

	/* if on olpc chan + [wlc chanspec changed or we're in scan/rm] + an active calibration */
	if (olpc_info->cur_chan &&
		(olpc_info->cur_chan->cspec != wlc->chanspec ||
		wlc_olpc_in_cal_prohibit_state(wlc)) &&
		wlc_olpc_chan_has_active_cal(wlc->olpc_info, olpc_info->cur_chan)) {
		wlc_olpc_chan_terminate_active_cal(wlc, olpc_info->cur_chan);
	}
	if (olpc_info->timer_active) {
		wlc_olpc_chan_terminate_active_cal(wlc, olpc_info->cur_chan);
	}
	return BCME_OK;
}

/* return TRUE iff txchain has cores that need calibrating */
static bool
wlc_olpc_chan_needs_cal(wlc_olpc_eng_info_t *olpc, wlc_olpc_eng_chan_t* chan)
{
	/* if cores not calibrated nor calibrating (~(chan->cores_cal | chan->cores_cal_active)) */
	/* that are in txchain (& olpc->wlc->stf->txchain), then we need some calibration */
	return (~(chan->cores_cal | chan->cores_cal_active) & olpc->wlc->stf->txchain) != 0;
}

static bool
wlc_olpc_chan_has_active_cal(wlc_olpc_eng_info_t *olpc, wlc_olpc_eng_chan_t* chan)
{
	BCM_REFERENCE(olpc);

	return ((chan->cores_cal_active || olpc->timer_active) != 0);
}

static void
wlc_olpc_stf_perrate_changed(wlc_info_t *wlc)
{
	if (wlc->olpc_info) {
		WL_OLPC_DBG(wlc->olpc_info, ("prerrate chged not restore\n"));
		wlc->olpc_info->restore_perrate_stf_state = FALSE;
	}
}

bool
wlc_olpc_eng_has_active_cal(wlc_olpc_eng_info_t *olpc)
{
	if (!wlc_olpc_eng_ready(olpc)) {
		return FALSE;
	}

	return (olpc->cur_chan && wlc_olpc_chan_has_active_cal(olpc, olpc->cur_chan));
}

static int
wlc_olpc_stf_override(wlc_info_t *wlc, uint8 tgt_chains)
{
	int err = BCME_OK;
	if ((wlc->stf->txcore_override[OFDM_IDX] == tgt_chains &&
		wlc->stf->txcore_override[CCK_IDX] == tgt_chains)) {
		/* NO-OP */
		return err;
	}
	if (!wlc->olpc_info->restore_perrate_stf_state) {
		wlc_stf_txchain_get_perrate_state(wlc, &(wlc->olpc_info->stf_saved_perrate),
			wlc_olpc_stf_perrate_changed);
	}

	wlc->olpc_info->restore_perrate_stf_state = TRUE;

	/* set value to have all cores on */
	/* we may use OFDM or CCK for cal pkts */
	WL_OLPC_DBG(wlc->olpc_info, ("%s: txcore_override current[ofdm,cck]=[%x,%x] override=%d\n",
		__FUNCTION__, wlc->stf->txcore_override[OFDM_IDX],
		wlc->stf->txcore_override[CCK_IDX],
		tgt_chains));
	wlc->stf->txcore_override[OFDM_IDX] = tgt_chains;
	wlc->stf->txcore_override[CCK_IDX] = tgt_chains;

	wlc_stf_spatial_policy_set(wlc, wlc->stf->spatialpolicy);

	return err;
}

/* called when calibration is done */
static int
wlc_olpc_stf_override_perrate_revert(wlc_info_t *wlc)
{
	if (wlc->olpc_info->restore_perrate_stf_state) {
		wlc->olpc_info->restore_perrate_stf_state = FALSE;
		wlc_stf_txchain_restore_perrate_state(wlc, &(wlc->olpc_info->stf_saved_perrate));
	}
	return BCME_OK;
}

/* called when calibration is done - if stf saved state still valid, restore it */
static int
wlc_olpc_stf_override_revert(wlc_info_t *wlc)
{
	/* restore override on txcore */
	wlc_olpc_stf_override_perrate_revert(wlc);
	/* txchain is not changed */
	return BCME_OK;
}

#define WL_OLPC_BW_ITER_HAS_NEXT(txbw) (txbw != WL_TX_BW_ALL)

static wl_tx_bw_t
wlc_olpc_bw_iter_next(wl_tx_bw_t txbw)
{
	wl_tx_bw_t ret;

	switch (txbw) {
		case WL_TX_BW_40:
			ret = WL_TX_BW_20IN40;
		break;
		case WL_TX_BW_80:
			ret = WL_TX_BW_40IN80;
		break;
		case WL_TX_BW_40IN80:
			ret = WL_TX_BW_20IN80;
		break;
		case WL_TX_BW_160:
			ret = WL_TX_BW_80IN160;
		break;
		case WL_TX_BW_80IN160:
			ret = WL_TX_BW_40IN160;
		break;
		case WL_TX_BW_40IN160:
			ret = WL_TX_BW_20IN160;
		break;
		case WL_TX_BW_20IN40:
		case WL_TX_BW_20IN80:
		case WL_TX_BW_20:
		case WL_TX_BW_20IN160:
			ret = WL_TX_BW_ALL;
		break;
		default:
			ASSERT(0);
			ret = WL_TX_BW_ALL;
		break;
	}
	return ret;
}

static int
wlc_olpc_get_min_2ss3ss_sdm_pwr(wlc_olpc_eng_info_t *olpc,
	ppr_t *txpwr, chanspec_t channel)
{
	int8 sdmmin;
	int min_txpwr_limit = 0xffff;
	int err = 0;
	uint txchains = WLC_BITSCNT(olpc->wlc->stf->txchain);
	wl_tx_bw_t txbw = PPR_CHSPEC_BW(channel);

	while (WL_OLPC_BW_ITER_HAS_NEXT(txbw)) {
		/* get min of 2x2 */
		if (txchains > 1) {
			err = ppr_get_vht_mcs_min(txpwr, txbw,
				WL_TX_NSS_2, WL_TX_MODE_NONE, WL_TX_CHAINS_2, &sdmmin);

			if (err == BCME_OK) {
				WL_OLPC_ENTRY(olpc, ("wl%d:%s: nss2 min=%d sdmmin=%d\n",
					olpc->wlc->pub->unit, __FUNCTION__,
					min_txpwr_limit, sdmmin));

				if (sdmmin != WL_RATE_DISABLED) {
					min_txpwr_limit = MIN(min_txpwr_limit, sdmmin);
				}
			}
		}
		/* get min of 3x3 */
		if (txchains > 2) {
			err = ppr_get_vht_mcs_min(txpwr, txbw,
				WL_TX_NSS_3, WL_TX_MODE_NONE, WL_TX_CHAINS_3, &sdmmin);
			if (err == BCME_OK) {
				WL_OLPC_ENTRY(olpc, ("wl%d:%s: nss3 min=%d sdmmin=%d\n",
					olpc->wlc->pub->unit, __FUNCTION__,
					min_txpwr_limit, sdmmin));
				if (sdmmin != WL_RATE_DISABLED) {
					min_txpwr_limit = MIN(min_txpwr_limit, sdmmin);
				}
			}
		}
		/* get min of 4x4 */
		if (txchains > 3) {
			err = ppr_get_vht_mcs_min(txpwr, txbw,
				WL_TX_NSS_4, WL_TX_MODE_NONE, WL_TX_CHAINS_4, &sdmmin);
			if (err == BCME_OK) {
				WL_OLPC_ENTRY(olpc, ("wl%d:%s: nss4 min=%d sdmmin=%d\n",
					olpc->wlc->pub->unit, __FUNCTION__,
					min_txpwr_limit, sdmmin));
				if (sdmmin != WL_RATE_DISABLED) {
					min_txpwr_limit = MIN(min_txpwr_limit, sdmmin);
				}
			}
		}
		txbw = wlc_olpc_bw_iter_next(txbw);
	}

	return min_txpwr_limit;
}

/* use ppr to find min tgt power (2ss/3ss sdm) in .25dBm units */
static int16
wlc_olpc_get_min_tgt_pwr(wlc_olpc_eng_info_t *olpc, chanspec_t channel)
{
	wlc_info_t* wlc = olpc->wlc;
	int cur_min = 0xFFFF;
	wlc_phy_t *pi = WLC_PI(wlc);
	ppr_t *txpwr;
	ppr_t *srommax;
	int8 min_srom;
	if ((txpwr = ppr_create(wlc->pub->osh, PPR_CHSPEC_BW(channel))) == NULL) {
		return WL_RATE_DISABLED;
	}
	if ((srommax = ppr_create(wlc->pub->osh, PPR_CHSPEC_BW(channel))) == NULL) {
		ppr_delete(wlc->pub->osh, txpwr);
		return WL_RATE_DISABLED;
	}
	/* use the control channel to get the regulatory limits and srom max/min */
	wlc_channel_reg_limits(wlc->cmi, channel, txpwr, NULL);

	wlc_phy_txpower_sromlimit(pi, channel, (uint8*)&min_srom, srommax, 0);

	/* bound the regulatory limit by srom min/max */
	ppr_apply_vector_ceiling(txpwr, srommax);

	WL_NONE(("min_srom %d\n", min_srom));
	cur_min = wlc_olpc_get_min_2ss3ss_sdm_pwr(olpc, txpwr, channel);

	ppr_delete(wlc->pub->osh, srommax);
	ppr_delete(wlc->pub->osh, txpwr);

	return (int16)(cur_min);
}

/* is channel one that needs open loop phy cal? */
static INLINE bool
wlc_olpc_chan_needs_olpc(wlc_olpc_eng_info_t *olpc, chanspec_t chan)
{
	int16 min_pwr, max_pwr, tssi_thresh;
	uint8 cores, core_idx = 0;
	int8 qdbm;
	bool override;
	bool force_check = FALSE;

	if (olpc && olpc->wlc->band && WLC_PI(olpc->wlc)) {
		if (olpc->olpc_override) {
			olpc->core_idx = 0;
			return TRUE;
		}
		wlc_phy_txpower_get(WLC_PI(olpc->wlc), &qdbm, &override);
		if (olpc->last_txpwr_user_target != qdbm) {
			olpc->last_txpwr_user_target = qdbm;
			force_check = TRUE;
			/* Reset Enterprise's forced olpc_state, which can be set by
			 * the TSSI check. It gets updated to the correct value, herein.
			 */
			olpc->last_chan_olpc_state = FALSE;
		}

		if (olpc->last_chan_chk != chan ||
			force_check ||
			olpc->last_txchain_chk != olpc->wlc->stf->txchain) {
			min_pwr = wlc_olpc_get_min_tgt_pwr(olpc, chan);
			max_pwr = 127;
			cores = olpc->num_hw_cores;
			for (; core_idx < cores; core_idx++) {
				max_pwr = MIN(max_pwr,
					wlc_phy_calc_ppr_pwr_cap(olpc->wlc->pi,
					core_idx));
			}
			tssi_thresh = (int16) wlc_phy_tssivisible_thresh(olpc->wlc->pi);
			if (min_pwr == WL_RATE_DISABLED) {
				/* assume not need olpc */
				WL_ERROR(("%s: min pwr lookup failed -- assume not olpc\n",
					__FUNCTION__));
				olpc->last_chan_olpc_state = FALSE;
			} else if (max_pwr < tssi_thresh) {
				/* If ALL powers are below OLPC thresh,
				 * disable OLPC for this channel
				 */
				olpc->last_chan_olpc_state = FALSE;
			} else {
				/* adjust by -1.5dBm to reconcile ppr and tssi */
				min_pwr = WL_OLPC_PPR_TO_TSSI_PWR(min_pwr);
				WL_OLPC_DBG(olpc, ("chanspec=0x%x mintgtpwr=%d tssithresh=%d\n",
					chan, min_pwr, tssi_thresh));
				/* this channel needs open loop pwr cal iff the below is true */
				olpc->last_chan_olpc_state = (min_pwr < tssi_thresh);
			}
			olpc->last_chan_chk = chan;
			olpc->last_txchain_chk = olpc->wlc->stf->txchain;
			WL_OLPC_DBG(olpc, ("%s: last_txchain_chk=%x\n", __FUNCTION__,
				olpc->last_txchain_chk));
		}
		if (qdbm < wlc_phy_tssivisible_thresh(WLC_PI(olpc->wlc))) {
			/* For Enterprise APs, we want OLPC calibration to occur on all channels
			 * if target power is less than TSSI visibility threshold.
			 */
			olpc->last_chan_olpc_state = TRUE;
		}

		return olpc->last_chan_olpc_state;
	} else {
		WL_REGULATORY(("%s: needs olpc FALSE/skip due to null phy info\n",
			__FUNCTION__));
	}
	return FALSE;
}

static void
wlc_olpc_chan_init(wlc_olpc_eng_chan_t* chan, chanspec_t cspec)
{
	bzero(chan->pkts_sent, sizeof(chan->pkts_sent));

	/* chanspec, containing chan number the struct refers to */
	chan->cspec = cspec;
	chan->next = NULL;
	chan->cores_cal = 0;
	chan->cores_cal_active = 0;
	chan->cores_cal_to_cmplt = 0;
	chan->cal_pkts_outstanding = 0;
#if WL_OLPC_IOVARS_ENAB
	chan->cores_cal_pkts_sent = 0;
	chan->dbg_mode = FALSE;
#endif /* WL_OLPC_IOVARS_ENAB */
}

static int
wlc_olpc_chan_rm(wlc_olpc_eng_info_t *olpc,
	wlc_olpc_eng_chan_t* chan)
{
	int err = BCME_OK;
	wlc_olpc_eng_chan_t *iter = olpc->chan_list;
	wlc_olpc_eng_chan_t *trail_iter = NULL;
	ASSERT(chan);
	WL_OLPC_ENTRY(olpc, ("Entry:%s\n", __FUNCTION__));

	while (iter != NULL) {
		if (iter == chan) {
			break;
		}
		trail_iter = iter;
		iter = iter->next;
	}
	if (iter) {
		/* cut from list */
		if (trail_iter) {
			trail_iter->next = iter->next;
		} else {
			olpc->chan_list = iter->next;
		}
		/* cut from olpc */
		if (olpc->cur_chan == iter) {
			olpc->cur_chan = NULL;
		}
		/* free memory */
		WL_OLPC_DBG(olpc, ("%s: removing chan_list.\n", __FUNCTION__));
		MFREE(olpc->wlc->osh, iter, sizeof(wlc_olpc_eng_chan_t));
	} else {
		/* attempt to free failed */
		ASSERT(iter);
		err = BCME_ERROR;
	}
	WL_OLPC_ENTRY(olpc, ("Exit:%s\n", __FUNCTION__));

	return err;
}

/*
* cspec - chanspec to search for
* err - to return error values
* create - TRUE to create if not found; FALSE otherwise
* return pointer to channel info structure; NULL if not found/not created
*/
static wlc_olpc_eng_chan_t*
wlc_olpc_get_chan_ex(wlc_info_t *wlc, chanspec_t cspec, int *err, bool create)
{
	wlc_olpc_eng_chan_t* chan = NULL;
	*err = BCME_OK;

	chan = wlc->olpc_info->chan_list;
	/* find cspec in list */
	while (chan) {
		/* get chan struct through which comparison? */
		if (chan->cspec == cspec) {
			return chan;
		}
		chan = chan->next;
	}
	if (!create) {
		return NULL;
	}
	/* create new channel on demand */
	chan = MALLOC(wlc->osh, sizeof(wlc_olpc_eng_chan_t));
	if (chan) {
		/* init and insert into list */
		wlc_olpc_chan_init(chan, cspec);
		chan->next = wlc->olpc_info->chan_list;
		wlc->olpc_info->chan_list = chan;
	} else {
		*err = BCME_NOMEM;
	}

	return chan;
}

/* get olpc chan from list - create it if not there */
static wlc_olpc_eng_chan_t*
wlc_olpc_get_chan(wlc_info_t *wlc, chanspec_t cspec, int *err)
{
	return wlc_olpc_get_chan_ex(wlc, cspec, err, TRUE);
}

int
wlc_olpc_eng_hdl_txchain_update(wlc_olpc_eng_info_t *olpc)
{
	WL_OLPC_ENTRY(olpc, ("%s\n", __FUNCTION__));
	if (olpc->restore_perrate_stf_state &&
		!wlc_stf_saved_state_is_consistent(olpc->wlc, &olpc->stf_saved_perrate))
	{
		/* our saved txcore_override may no longer be valid, so don't restore */
		/* logic in stf prevents txchain and txcore_override from colliding */
		/* if saved state is 0 (clear override), */
		/* then restore unless txcore_override changes */
		olpc->restore_perrate_stf_state = FALSE;
	}
	return wlc_olpc_eng_hdl_chan_update(olpc);
}

int
wlc_olpc_eng_reset(wlc_olpc_eng_info_t *olpc)
{
	/* clear internal state and process new info */
	/* equivalent to down followed by up */
	if (wlc_olpc_eng_ready(olpc)) {
		WL_OLPC_DBG(olpc, ("%s - exec down/up\n", __FUNCTION__));
		wlc_olpc_eng_down((void*)olpc);
		wlc_olpc_eng_up((void*)olpc);
	}
	/* else wait until up and then process */
	return BCME_OK;
}

int
wlc_olpc_eng_hdl_chan_update(wlc_olpc_eng_info_t *olpc)
{
	if (!olpc) {
		return BCME_OK;
	}
	WL_OLPC_ENTRY(olpc, ("%s\n", __FUNCTION__));

	return wlc_olpc_eng_hdl_chan_update_ex(olpc->wlc, olpc->npkts, FALSE);
}

/* Is there a cfg up on our channel? (Ignoring exclude parm which may be NULL) */
static bool
wlc_olpc_find_other_up_on_chan(wlc_info_t *wlc, wlc_bsscfg_t *exclude)
{
	int idx;
	wlc_bsscfg_t *cfg;

	FOREACH_BSS(wlc, idx, cfg) {
		if (cfg->up && cfg != exclude) {
			if (cfg->current_bss &&
				cfg->current_bss->chanspec ==
				wlc->chanspec) {
				return TRUE;
			}
			WL_OLPC_ENTRY(wlc->olpc_info,
				("%s: unmatch bsscfg(%p) up%d curbss%p cspec%x\nwlc->cspec=%x\n",
				__FUNCTION__, OSL_OBFUSCATE_BUF(cfg),
				cfg->up, OSL_OBFUSCATE_BUF(cfg->current_bss),
				cfg->current_bss ? cfg->current_bss->chanspec : -1,
				wlc->chanspec));
		}
	}
	return FALSE;
}

/* kick-off open loop phy cal */
static int
wlc_olpc_eng_hdl_chan_update_ex(wlc_info_t *wlc, uint8 npkts, bool force_cal)
{
	int err = BCME_OK;
	chanspec_t cspec = WLC_TO_CHANSPEC(wlc);
	bool olpc_chan = FALSE;
	wlc_olpc_eng_chan_t* chan_info = NULL;
	wlc_olpc_eng_info_t* olpc_info = wlc->olpc_info;

	if (!wlc_olpc_eng_ready(olpc_info)) {
		WL_NONE(("wl%d:%s: olpc module not up\n", wlc->pub->unit, __FUNCTION__));
		return BCME_ERROR;
	}
	WL_OLPC_ENTRY(olpc_info, ("%s\n", __FUNCTION__));

	/* check for any condition that may cause us to terminate our current calibration */
	/* and terminate it if needed */
	wlc_olpc_eng_terminate_cal(wlc);

	/* do nothing if scan or rm is running */
	if (wlc_olpc_in_cal_prohibit_state(wlc)) {
		olpc_info->cur_chan = NULL;
		goto exit;
	}

	olpc_chan = wlc_olpc_chan_needs_olpc(olpc_info, wlc->chanspec);
	WL_OLPC_DBG(olpc_info, ("%s: chan=%x home=%x olpc_chan=%d force_cal=%d\n",
		__FUNCTION__, wlc->chanspec, wlc->home_chanspec, olpc_chan, force_cal));

	if (olpc_chan) {
		/* phytx ctl word power offset needs to be set */
		chan_info = wlc_olpc_get_chan(wlc, cspec, &err);
		olpc_info->cur_chan = chan_info;

		/* pretend not calibrated */
		if (chan_info && force_cal) {
			olpc_info->cur_chan->cores_cal = 0;
#if WL_OLPC_IOVARS_ENAB
			olpc_info->cur_chan->cores_cal_pkts_sent = 0;
#endif /* WL_OLPC_IOVARS_ENAB */
		}

		/* if null here, there was an out of mem condition */
		if (!chan_info) {
			err = BCME_NOMEM;
			WL_OLPC(olpc_info, ("%s: chan info not found\n", __FUNCTION__));

			goto exit;
		} else if (!wlc_olpc_chan_needs_cal(olpc_info, chan_info)) {
			/* no cal needed */
			WL_OLPC_DBG(olpc_info, ("%s: no cal needed at chan update notify\n",
				__FUNCTION__));
			goto exit;
		} else {
			/* cal needed -- limit all rates for now; force implies cfg up on channel */
			if (!force_cal && !wlc_olpc_find_other_up_on_chan(wlc, NULL)) {
				WL_OLPC_DBG(olpc_info, ("%s: NO BSS FOUND UP - exiting\n",
					__FUNCTION__));
				/* avoid calibrations when no cfg up, as cal caching not avail */
				goto exit;
			}

			WL_OLPC_DBG(olpc_info, ("%s: calibration needed\n",
				__FUNCTION__));
		}
		/* now kick off cal/recal */
		WL_OLPC_DBG(olpc_info, ("%s: calibration needed, starting recal\n", __FUNCTION__));
		err = wlc_olpc_eng_recal_ex(wlc, npkts);
	} else {
		if (olpc_info->cur_chan) {
			wlc_olpc_stf_override_revert(wlc);
			olpc_info->cur_chan = NULL;
		}
	}

exit:
	return err;
}

static int
wlc_olpc_eng_recal_ex(wlc_info_t *wlc, uint8 npkts)
{
	int err = BCME_OK;
	wlc_olpc_eng_chan_t* chan_info = wlc->olpc_info->cur_chan;
	if (chan_info) {
		WL_OLPC(wlc->olpc_info, ("%s - send dummies\n", __FUNCTION__));
		err = wlc_olpc_send_dummy_pkts(wlc, chan_info, npkts);
	} else {
		WL_OLPC(wlc->olpc_info,
			("%s: chan info NULL - no cal\n", __FUNCTION__));
	}
	WL_NONE(("%s - end\n", __FUNCTION__));

	return err;
}

/* kick off new open loop phy cal */
int
wlc_olpc_eng_recal(wlc_olpc_eng_info_t *olpc)
{
	WL_OLPC_ENTRY(olpc, ("%s\n", __FUNCTION__));

	/* begin new cal if we can */
	return wlc_olpc_eng_hdl_chan_update_ex(olpc->wlc, olpc->npkts, TRUE);
}

static void
wlc_olpc_modify_pkt(wlc_info_t *wlc, void* p)
{
	uint16 *mcl;

	/* no ACK */
	wlc_pkt_set_ack(wlc, p, FALSE);

	if (D11REV_LT(wlc->pub->corerev, 128)) {
		/* Can only modify power per-packet if corerev < 128. */
		/* set pwr offset 0 */
		wlc_pkt_set_txpwr_offset(wlc, p, 0);
	}
	else if (D11REV_GE(wlc->pub->corerev, 128)) {
		uint16 rate_idx, scb_rate_index;
		d11txhdr_t* txh = NULL;
		wlc_pkt_get_txh_hdr(wlc, p, &txh);

		rate_idx = ltoh16(txh->rev128.RateMemIdxRateIdx);
		scb_rate_index = WLC_RLM_SPECIAL_RATE_IDX;

		/* preserve block index and just set rate index */
		rate_idx &= ~D11_REV128_RATEIDX_MASK;
		rate_idx |= scb_rate_index;
		txh->rev128.RateMemIdxRateIdx = htol16(rate_idx & D11_REV128_RATEIDX_MASK);

		/* AX ucode will check the D11AC_TXC_OLPC bit and use the M_OLPC_TXPWR
		 * shm location to set the power offset as set in M_OLPC_TXPWR.
		 */
		mcl = D11_TXH_GET_MACLOW_PTR(wlc, txh);
		*mcl |= D11AX_TXC_FIXTXPWR;	/* set txpwr override bit */
		*mcl &= ~D11AX_TXC_TXPWRIDX_MASK;
		*mcl |= D11AX_TXC_TXPWRIDX_0 << D11AX_TXC_TXPWRIDX_SHIFT;
	}

}

/* totally bogus pkt -- d11 hdr only + tx hdrs */
static void *
wlc_olpc_get_pkt(wlc_info_t *wlc, uint ac)
{
	int buflen = (TXOFF + WL_OLPC_PKT_LEN);
	void* p = NULL;
	osl_t *osh = wlc->osh;
	const char* macaddr = NULL;
	struct dot11_header *hdr = NULL;
	ratespec_t rspec;

	if ((p = PKTGET(osh, buflen, TRUE)) == NULL) {
		WL_ERROR(("wl%d: %s: pktget error for len %d \n",
			wlc->pub->unit, __FUNCTION__, buflen));
		goto fatal;
	}
	macaddr = WLC_MACADDR(wlc);

	WL_NONE(("pkt manip\n"));
	/* reserve TXOFF bytes of headroom */
	PKTPULL(osh, p, TXOFF);
	PKTSETLEN(osh, p, WL_OLPC_PKT_LEN);

	WL_NONE(("d11_hdr\n"));
	hdr = (struct dot11_header*)PKTDATA(osh, p);
	bzero((char*)hdr, WL_OLPC_PKT_LEN);
	hdr->fc = htol16(FC_DATA);
	hdr->durid = 0;
	bcopy((const char*)macaddr, (char*)&(hdr->a1.octet), ETHER_ADDR_LEN);
	bcopy((const char*)macaddr, (char*)&(hdr->a2.octet), ETHER_ADDR_LEN);
	bcopy((const char*)macaddr, (char*)&(hdr->a3.octet), ETHER_ADDR_LEN);
	hdr->seq = 0;
	WL_NONE(("prep raw 80211\n"));

	/* use lowest basic rate supported - only for OLPC cal pkts */
	rspec = wlc_lowest_basic_rspec(wlc, &(wlc->band->hw_rateset));

	(void)wlc_prep80211_raw(wlc, NULL, ac, p, rspec, NULL);

	return p;
fatal:
	return (NULL);
}

/* process one pkt send complete */
static void
wlc_olpc_eng_pkt_complete(wlc_info_t *wlc, void *pkt, uint txs)
{
	chanspec_t chanspec;

	wlc_olpc_eng_chan_t* olpc_chan = NULL;
	wlc_txh_info_t tx_info;
	int err;
	uint8 coreMask;
	uint8 cidx = 0;
#if (defined(PHYCAL_CACHING) || defined(WLMCHAN))
	wlc_phy_t *pi = WLC_PI(wlc);
#endif // endif
	int8 qdbm;
	bool override;
	ppr_t *txpwr;
	int tssi_visi_thresh;
	wlc_olpc_eng_info_t *olpc = wlc->olpc_info;

	BCM_REFERENCE(txs);

	if ((WLPKTTAG(pkt)->flags & WLF_TXHDR) == 0) {
		WL_ERROR(("%s: pkt header not fully formed.\n", __FUNCTION__));
		return;
	}
	wlc_get_txh_info(wlc, pkt, &tx_info);

	/* one calibration packet was finished */
	/* look at packet header to find - channel, antenna, etc. */
	chanspec = wlc_txh_get_chanspec(wlc, &tx_info);
	olpc_chan = wlc_olpc_get_chan_ex(wlc, chanspec, &err, FALSE);
	WL_OLPC_ENTRY(wlc->olpc_info, ("%s\n", __FUNCTION__));

	if (!olpc_chan || err != BCME_OK) {
		WL_OLPC_DBG(wlc->olpc_info, ("%s: err: NO-OP chanspec=%x\n",
			__FUNCTION__, chanspec));
		return;
	}
	if (olpc_chan->cal_pkts_outstanding) {
		olpc_chan->cal_pkts_outstanding--;
	}
	if (olpc_chan->cores_cal_active == 0) {
		WL_OLPC_DBG(wlc->olpc_info, ("%s: NO-OP (no cal was active) chanspec=%x\n",
			__FUNCTION__, chanspec));
		return;
	}

	WL_OLPC_DBG(wlc->olpc_info, ("%s: entry status=%d  -  chanspec=%x\n",
		__FUNCTION__, txs, chanspec));

	/* get core mask, index */
	coreMask = 1 << olpc->core_idx;
	cidx = olpc->core_idx;

	WL_OLPC_DBG(wlc->olpc_info, ("%s: core_idx=%d, coreNum=%x\n",
		__FUNCTION__, cidx, coreMask));

	/* decrement counters */
	if (olpc_chan->pkts_sent[cidx]) {
		olpc_chan->pkts_sent[cidx]--;
	} else {
		WL_NONE(("wl%d: %s: tried decrementing counter of 0, idx=%d\n",
			wlc->pub->unit, __FUNCTION__, olpc->core_idx));
	}
	/* if done on core, update info */
	if (olpc_chan->pkts_sent[cidx] == 0) {
		olpc_chan->cores_cal_active &= ~coreMask;
		olpc_chan->cores_cal |= coreMask;
		olpc_chan->cores_cal_to_cmplt |= coreMask;
#if WL_OLPC_IOVARS_ENAB
		olpc_chan->cores_cal_pkts_sent |= coreMask;
#endif /* WL_OLPC_IOVARS_ENAB */

		WL_OLPC(wlc->olpc_info, ("%s: exit: open loop phy CAL done mask=%x!\n",
			__FUNCTION__, coreMask));
		WL_OLPC(wlc->olpc_info, ("%s: exit: open loop phy CAL done done=%x active=%x!\n",
			__FUNCTION__, olpc_chan->cores_cal, olpc_chan->cores_cal_active));
	}

	if (olpc_chan->cores_cal == wlc->stf->hw_txchain) {
		WL_OLPC(wlc->olpc_info, ("%s: exit: open loop phy CAL done for all chains!\n",
			__FUNCTION__));
		wlc_write_shm(wlc, M_COREMASK_BPHY(wlc), wlc->olpc_info->shmem_save);
	}

	if (wlc->hw->reinit) {
		WL_ERROR(("%s: reinit is %d, so bail \n", __FUNCTION__, wlc->hw->reinit));
		return;
	}

	/* Check "calibrated" vs. active txchain */
	if (olpc_chan->cores_cal == wlc->stf->txchain) {
		WL_OLPC(wlc->olpc_info, ("%s: no more cores to calibrate!\n", __FUNCTION__));
		/* cache calibration results so following ops don't mess it up */
#if (defined(PHYCAL_CACHING) || defined(WLMCHAN) || defined(WLOLPC))
		/* phy_chanmgr_create_ctx() returns if context already exists */
		phy_chanmgr_create_ctx((phy_info_t *) pi, wlc->chanspec);
#ifdef WLOLPC
		/* PHYCAL_CACHING may be included, but caching is not used on all PHYs.
		 * Only act on the phycal cache if it's actually in-use
		 */
		if (!wlc_bmac_get_phycal_cache_flag(wlc->hw)) {
			/* Do nothing if caching is disabled */
		}
		else
#endif /* WLOLPC */
		phy_cache_cal((phy_info_t *) pi);
#endif /* PHYCAL_CACHING || WLMCHAN */
		wlc->olpc_info->olpc_override = 0;
		wlc->olpc_info->olpc_cal_done = 0;
		wlc_olpc_stf_override_revert(wlc);
#if defined WLTXPWR_CACHE && defined(WL11N)
		wlc_phy_txpwr_cache_invalidate(phy_tpc_get_txpwr_cache(WLC_PI(wlc)));
#endif	/* WLTXPWR_CACHE */
#if (defined(PHYCAL_CACHING) || defined(WLMCHAN) || defined(WLOLPC))
#ifdef WLOLPC
		/* PHYCAL_CACHING may be included, but caching is not used on all PHYs.
		 * If the phycal cache is not enabled, its errors do not
		 * invalidate olpc's calibration
		 */
		if (!wlc_bmac_get_phycal_cache_flag(wlc->hw)) {
			/* Spoof a good "phy cache restore cal" used below */
			err = BCME_OK;
		}
		else
#endif /* WLOLPC */
		if ((err = phy_cache_restore_cal((phy_info_t *) pi)) != BCME_OK) {
			WL_ERROR(("wl%d:%s: error from phy_cache_restore_cal=%d\n",
				wlc->pub->unit, __FUNCTION__, err));
			/* mark as not calibrated - calibration values hosed */
			olpc_chan->cores_cal = 0;
		}
		if (err == BCME_OK ||
#if WL_OLPC_IOVARS_ENAB
			olpc_chan->dbg_mode ||
#endif /* WL_OLPC_IOVARS_ENAB */
			FALSE) {
			/* inform the phy we are calibrated */
			/* if dbg mode is set, we ignore cal cache errors and tell the phy */
			/* to use dbg storage for cal result */
			wlc_phy_update_olpc_cal(pi, TRUE,
#if WL_OLPC_IOVARS_ENAB
				olpc_chan->dbg_mode);
#else
				FALSE);
#endif /* WL_OLPC_IOVARS_ENAB */
		}
#endif /* PHYCAL_CACHING || WLMCHAN */
		wlc_phy_txpower_get(WLC_PI(wlc), &qdbm, &override);
		WL_OLPC_DBG(wlc->olpc_info,
			("%s: WL_OLPC get txpwr %d qdbm (%d.%d dBm)\n",
			__FUNCTION__, qdbm, qdbm >> 2,
			0 == (qdbm & 3) ? 0 : 1 == (qdbm & 3) ? 25 : 2 == (qdbm & 3) ? 5 : 75));
		tssi_visi_thresh = wlc_phy_tssivisible_thresh(WLC_PI(wlc));
		WL_OLPC_DBG(wlc->olpc_info,
			("%s: WL_OLPC tssi_visi_thresh %d qdbm (%d.%d dBm)\n",
			__FUNCTION__, tssi_visi_thresh, tssi_visi_thresh >> 2,
			0 == (tssi_visi_thresh & 3) ? 0 : 1 == (tssi_visi_thresh & 3) ? 25 :
			2 == (tssi_visi_thresh & 3) ? 5 : 75));
		if (qdbm < tssi_visi_thresh) {
			WL_OLPC_DBG(wlc->olpc_info, ("%s: WL_OLPC do OLPC %d < %d\n",
				__FUNCTION__, qdbm, tssi_visi_thresh));
			if ((txpwr = ppr_create(wlc->osh, PPR_CHSPEC_BW(wlc->chanspec))) != NULL) {
				wlc_channel_reg_limits(wlc->cmi, wlc->chanspec, txpwr, NULL);
				ppr_apply_max(txpwr, WLC_TXPWR_MAX);
				err = wlc_phy_txpower_set(WLC_PI(wlc), qdbm, FALSE, txpwr);
				WL_OLPC_DBG(wlc->olpc_info,
					("%s - setting power to %d qdbm (%d.%d dBm), err = %d\n",
					__FUNCTION__, qdbm, qdbm >> 2,
					0 == (qdbm & 3) ? 0 : 1 == (qdbm & 3) ? 25 :
					2 == (qdbm & 3) ? 5 : 75, err));
				ppr_delete(wlc->osh, txpwr);
			}
		} else {
			WL_OLPC_DBG(wlc->olpc_info,
				("%s: WL_OLPC No OLPC!  %d >= %d\n",
				__FUNCTION__, qdbm, tssi_visi_thresh));
		}

		olpc_chan->cores_cal_to_cmplt = 0;
	}

	/* if reached the last outstanding pkt, then cal if needed */
	if (olpc_chan->cal_pkts_outstanding == 0) {
		olpc->cur_chan = olpc_chan;
		WLC_OLPC_ADD_TIMER(olpc, 0, 0);
		olpc->timer_active = TRUE;
	}
}

static bool
wlc_olpc_eng_ready(wlc_olpc_eng_info_t *olpc_info)
{
	if (!olpc_info || !olpc_info->up) {
		return FALSE;
	}
	return TRUE;
}

/* return number of cores needing cal */
static uint8
wlc_olpc_chan_num_cores_cal_needed(wlc_info_t *wlc, wlc_olpc_eng_chan_t* chan)
{
	uint8 cores = wlc->olpc_info->num_hw_cores;
	uint8 core_idx = 0;
	uint8 needed = 0;
	for (; core_idx < cores; core_idx++) {
		/* if chain is off or calibration done/in progress then skip */
		if ((wlc->stf->txchain & (1 << core_idx)) == 0 ||
			((chan->cores_cal | chan->cores_cal_active) & (1 << core_idx)) != 0) {
			continue; /* not needed */
		}
		needed++;
	}
	return needed;
}

/* return BCME_OK if all npkts * num_cores get sent out */
static int
wlc_olpc_send_dummy_pkts(wlc_info_t *wlc, wlc_olpc_eng_chan_t* chan, uint8 npkts)
{
	int err = BCME_OK;
	void *pkt = NULL;
	uint8 cores;
	uint8 pktnum;
	int prec;
	wlc_olpc_eng_info_t *olpc_info = wlc->olpc_info;
	uint8 core_idx = olpc_info->core_idx;

	ASSERT(wlc->stf);
	cores = olpc_info->num_hw_cores;

	if (!chan) {
		WL_ERROR(("wl%d: %s: null channel - not sending\n", wlc->pub->unit, __FUNCTION__));
		return BCME_NOMEM;
	}
	/* Detaching queue is still pending, don't queue packets to FIFO */
	if (wlc->txfifo_detach_pending) {
		WL_INFORM(("wl%d: wlc->txfifo_detach_pending %d\n",
			wlc->pub->unit, wlc->txfifo_detach_pending));
		return BCME_NOTREADY;
	}

	/* if send pkts would exceed npkts * tot cores, then wait for cal pkts to all return */
	if (chan->cal_pkts_outstanding +
		(wlc_olpc_chan_num_cores_cal_needed(wlc, chan) * npkts) >
		(npkts * cores)) {
		/* wait enough pkts completed, then calibrate */
		return BCME_OK;
	}

	/* txq */
	/* if chain is off or calibration done/in progress then skip */
	if ((wlc->stf->txchain & (1 << core_idx)) == 0 ||
		((chan->cores_cal | chan->cores_cal_active) & (1 << core_idx)) != 0) {
		/* skip this one - already calibrated/calibrating or txchain not on */
		WL_OLPC(olpc_info,
			("%s: skip core %d for calibrating. txchain=%x : cores_cal = 0x%x :"
			" cores_cal_active = 0x%x. find next core\n",
			__FUNCTION__, core_idx, wlc->stf->txchain,
			chan->cores_cal, chan->cores_cal_active));

		core_idx = wlc_olpc_find_next_active_core(olpc_info);
		/* same core? then already running */
		if (core_idx == olpc_info->core_idx) {
			WL_OLPC_DBG(olpc_info, ("%s: active core %d returned for cal.\n",
				__FUNCTION__, core_idx));
			return BCME_OK;
		} else {
			olpc_info->core_idx = core_idx;
		}

		WL_OLPC(olpc_info, ("%s: next core %d for calibrating.\n",
			__FUNCTION__, core_idx));
	}
	if ((err = wlc_olpc_stf_override(wlc, (1 << core_idx))) != BCME_OK) {
		WL_ERROR(("%s: abort olpc cal; err=%d\n", __FUNCTION__, err));
		return err;
	}
	/*  In the above function wlc_olpc_stf_override, shmem M_COREMASK_BPHY is
	 * written to be the current core. ucode overrides the core info in tx ctrl word
	 * with the entry in SHMEM. Because of asynchronous timing between queing and
	 * OTA tx, packets intended to be sent on core 0 can be sent on core 1 and viceversa
	 * This fix from ucode ignores the core overrides if 12th bit of M_COREMASK_BPHY is
	 * set.
	 * So, after functions "wlc_olpc_stf_override" and "wlc_olpc_stf_override_revert"
	 * 12th bit of M_COREMASK_BPHY is set. Correct fix is to change the value written in
	 * stf code directly.
	 */
	wlc->olpc_info->shmem_save = wlc_read_shm(wlc, M_COREMASK_BPHY(wlc));
	wlc_write_shm(wlc, M_COREMASK_BPHY(wlc), (wlc->olpc_info->shmem_save | 0x1000));
	if (D11REV_GE(wlc->pub->corerev, 128)) {
		/* set OLPC tx power offset to zero in shmem location for MAC */
		wlc_write_shm(wlc, (M_TXPWR_BLK(wlc) + WL_OLPC_TXPWRIDX), 0);
	}

	for (pktnum = 0; pktnum < npkts; pktnum++) {
		WL_NONE(("%s: getting test frame\n", __FUNCTION__));
		pkt = wlc_olpc_get_pkt(wlc, AC_VI);
		if (pkt == NULL) {
			WL_OLPC_DBG(olpc_info, ("wl%d: %s: null pkt - not sending\n",
				wlc->pub->unit,
				__FUNCTION__));
			err = BCME_NOMEM;
			break;
		}

		/* modify tx headers, make sure it is no ack and on the right antenna */
		WL_NONE(("%s: modify pkt\n", __FUNCTION__));
		wlc_olpc_modify_pkt(wlc, pkt);

		/* done modifying - now register for pcb */
		WLF2_PCB1_REG(pkt, WLF2_PCB1_OLPC);

		WL_NONE(("%s: send pkt\n", __FUNCTION__));
		prec = wlc_prio2prec(wlc, PKTPRIO(pkt));

		if (!cpktq_prec_enq_head(wlc, &wlc->active_queue->cpktq,
			pkt, prec, TRUE)) {
			WL_OLPC(olpc_info, ("wl%d: %s: txq full, frame discarded\n",
				wlc->pub->unit, __FUNCTION__));
			WLCNTINCR(wlc->pub->_cnt->txnobuf);
			PKTFREE(wlc->osh, pkt, TRUE);
			err = BCME_NOMEM;
			break;
		}
		WL_NONE(("olpc prec=%d prio=%d\n", prec, PKTPRIO(pkt)));
		chan->pkts_sent[core_idx]++;
		chan->cal_pkts_outstanding++;
	}
	/* successful here, so modify cal_active variable */
	chan->cores_cal_active |= (1 << core_idx);
	/* execute these for now, coz cal is over */
	wlc_olpc_stf_override_revert(wlc);

	wlc_write_shm(wlc, M_COREMASK_BPHY(wlc), (wlc->olpc_info->shmem_save | 0x1000));
	if (err != BCME_OK) {
		WL_ERROR(("wl%d: %s: err - cal not done\n",
			wlc->pub->unit, __FUNCTION__));
	}
	/* WL_EAP: kick-out the OLPC cal pkts immediately to support
	 * automated testing scripts
	 */
#if defined(WLTEST)
	else {
		WL_NONE(("wl%d: %s: sendq\n",
			wlc->pub->unit, __FUNCTION__));
		wlc_send_q(wlc, wlc->active_queue);
	}
#endif /* WLTEST */
	return err;
}

static int
wlc_olpc_doiovar(void *context, uint32 actionid,
	void *params, uint p_len, void *arg, uint alen, uint vsize, struct wlc_if *wlcif)
{
	int err = BCME_UNSUPPORTED;
	wlc_olpc_eng_info_t *olpc = (wlc_olpc_eng_info_t *)context;
	wlc_info_t *wlc = olpc->wlc;

	wlc_olpc_eng_chan_t* chan = NULL;
	bool is_olpc_chan = FALSE;
	bool is_scanning = FALSE;

	int32 int_val = 0;
	int32 *ret_int_ptr;
	bool bool_val;
	err = BCME_OK;

	BCM_REFERENCE(wlcif);
	BCM_REFERENCE(vsize);
	BCM_REFERENCE(alen);
	BCM_REFERENCE(context);
	BCM_REFERENCE(chan);
	BCM_REFERENCE(ret_int_ptr);
	BCM_REFERENCE(bool_val);

	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	bool_val = (int_val != 0) ? TRUE : FALSE;
	if (olpc->up) {
		is_olpc_chan = wlc_olpc_chan_needs_olpc(olpc, wlc->chanspec);
		is_scanning = wlc_olpc_in_cal_prohibit_state(wlc);
		if (is_olpc_chan) {
			chan = wlc_olpc_get_chan_ex(wlc, wlc->chanspec, &err, !is_scanning);
		}
	}

	switch (actionid) {
#if WL_OLPC_IOVARS_ENAB
		case IOV_SVAL(IOV_OLPC):
			wlc->pub->_olpc = bool_val;
			break;
		case IOV_GVAL(IOV_OLPC):
			*ret_int_ptr = OLPC_ENAB(wlc);
			break;
		case IOV_GVAL(IOV_OLPC_CAL_PKTS):
			if (is_scanning) {
				err = BCME_BUSY;
			} else if (!is_olpc_chan || !chan) {
				WL_OLPC(olpc,
					("%s: OLPC_CAL_PKTS: not valid olpc chan (%x)\n",
					__FUNCTION__, wlc->chanspec));
				err = BCME_BADCHAN;
			} else {
				*ret_int_ptr = chan->cores_cal_pkts_sent;
			}
			break;
		case IOV_SVAL(IOV_OLPC_CAL_ST):
			if (is_scanning) {
				err = BCME_BUSY;
			} else if (!is_olpc_chan || !chan) {
				WL_OLPC(olpc,
					("%s: SET_CAL_STATE: not valid olpc chanspec (%x)\n",
					__FUNCTION__, wlc->chanspec));
				err = BCME_BADCHAN;
			} else {
				chan->cores_cal = (uint8)int_val;
			}
			break;
		case IOV_GVAL(IOV_OLPC_CAL_ST):
			if (is_scanning) {
				err = BCME_BUSY;
			} else if (!is_olpc_chan || !chan) {
				WL_OLPC(olpc,
					("%s: GET_CAL_STATE: not valid olpc chan (%x)\n",
					__FUNCTION__, wlc->chanspec));
				err = BCME_BADCHAN;
			} else {
				*ret_int_ptr = chan->cores_cal;
			}
			break;
		case IOV_GVAL(IOV_OLPC_FORCE_CAL):
			if (is_scanning || wlc_olpc_eng_has_active_cal(olpc)) {
				err = BCME_BUSY;
				*ret_int_ptr = -1;
				return err;
			}
			if (!chan) {
				/* always force, even if not olpc channel */
				chan = wlc_olpc_get_chan_ex(wlc, wlc->chanspec, &err, TRUE);
			}
			if (chan) {
				chan->cores_cal = 0;
				chan->cores_cal_active = 0;
				wlc->olpc_info->cur_chan = chan;
				wlc->olpc_info->core_idx = 0;
				wlc_olpc_eng_recal_ex(wlc, olpc->npkts);
				*ret_int_ptr = wlc_olpc_chan_has_active_cal(olpc, chan);
			} else {
				WL_ERROR(("%s: Couldn't create chan struct\n", __FUNCTION__));
				err = BCME_NOMEM;
				*ret_int_ptr = -1;
			}
			break;
		case IOV_SVAL(IOV_OLPC_MSG_LVL):
			olpc->msglevel = (uint8)int_val;
			break;
		case IOV_GVAL(IOV_OLPC_MSG_LVL):
			*ret_int_ptr = olpc->msglevel;
			break;
		case IOV_GVAL(IOV_OLPC_CHAN):
			*ret_int_ptr =
				wlc_olpc_chan_needs_olpc(olpc, wlc->chanspec) ? 1 : 0;
			break;
		case IOV_SVAL(IOV_OLPC_CHAN_DBG):
			if (is_scanning) {
				err = BCME_BUSY;
			} else if (!is_olpc_chan) {
				WL_OLPC(olpc,
					("wl%d:%s: OLPC_CHAN_DBG: not valid olpc chan (%x)\n",
					wlc->pub->unit, __FUNCTION__, wlc->chanspec));
				err = BCME_BADCHAN;
			} else if (chan) {
				chan->dbg_mode = bool_val;
			} else {
				WL_ERROR(("wl%d:%s: out of memory for SVAL(OLPC_CHAN_DBG)\n",
					wlc->pub->unit, __FUNCTION__));
				err = BCME_NOMEM;
			}
			break;
		case IOV_GVAL(IOV_OLPC_CHAN_DBG):
			if (is_scanning) {
				err = BCME_BUSY;
			} else if (!is_olpc_chan) {
				WL_OLPC(olpc,
					("wl%d:%s: OLPC_CHAN_DBG: not valid olpc chan (%x)\n",
					wlc->pub->unit, __FUNCTION__, wlc->chanspec));
				err = BCME_BADCHAN;
			} else if (chan) {
				*ret_int_ptr = chan->dbg_mode;
			} else {
				WL_ERROR(("wl%d:%s: out of memory for GVAL(OLPC_CHAN_DBG)\n",
					wlc->pub->unit, __FUNCTION__));
				err = BCME_NOMEM;
			}
			break;
		case IOV_SVAL(IOV_OLPC_OVERRIDE):
			if (is_scanning) {
				err = BCME_BUSY;
			}
			olpc->olpc_override = bool_val;
			if (olpc->olpc_override) {
				wlc_olpc_eng_hdl_chan_update_ex(wlc, olpc->npkts, TRUE);
			}
			break;
		case IOV_GVAL(IOV_OLPC_OVERRIDE):
			*ret_int_ptr = olpc->olpc_override;
			break;

		case IOV_GVAL(IOV_OLPC_CAL_DONE):
			*ret_int_ptr = olpc->olpc_cal_done;
			break;
#endif /* WL_OLPC_IOVARS_ENAB */
		default:
			err = BCME_UNSUPPORTED;
			break;
	}

	return err;
}

#if defined(BCMDBG) || defined(WLTEST)
static void
wlc_olpc_dump_channels(wlc_olpc_eng_info_t *olpc, struct bcmstrbuf *b)
{
	wlc_olpc_eng_chan_t* cur_chan = olpc->chan_list;

	while (cur_chan) {
		bcm_bprintf(b, "ptr=%p chan=%x pkts[%d,%d,%d,%d]\n"
		"core_cal=%x core_cal_active=%x core_cal_need_cmplt=%x cal_pkts_outst=%d\n",
			OSL_OBFUSCATE_BUF(cur_chan), cur_chan->cspec, cur_chan->pkts_sent[0],
			cur_chan->pkts_sent[1], cur_chan->pkts_sent[2], cur_chan->pkts_sent[3],
			cur_chan->cores_cal, cur_chan->cores_cal_active,
			cur_chan->cores_cal_to_cmplt, cur_chan->cal_pkts_outstanding);
		cur_chan = cur_chan->next;
	}
}

static int
wlc_dump_olpc(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	wlc_olpc_eng_info_t *olpc = wlc->olpc_info;
	if (!olpc) {
		bcm_bprintf(b, "olpc NULL (not attached)!\n");
		return 0;
	}
	bcm_bprintf(b, "up=%d npkts=%d", olpc->up, olpc->npkts);
	bcm_bprintf(b, "\nnum_hw_cores=%d restr_txcr=%d saved_txcr=%x", olpc->num_hw_cores,
		olpc->restore_perrate_stf_state, olpc->stf_saved_perrate);
	bcm_bprintf(b, "\nPPR: last_txchain_chk=%x last_chan_chk=%x last_chan_olpc_state=%d\n"
		"cur_chan=%p\n",
		olpc->last_txchain_chk, olpc->last_chan_chk, olpc->last_chan_olpc_state,
		OSL_OBFUSCATE_BUF(olpc->cur_chan));

	wlc_olpc_dump_channels(olpc, b);
	return 0;
}
#endif // endif

/* module attach */
wlc_olpc_eng_info_t*
BCMATTACHFN(wlc_olpc_eng_attach)(wlc_info_t *wlc)
{
	int err;
	wlc_olpc_eng_info_t *olpc_info = NULL;
	WL_NONE(("%s\n", __FUNCTION__));
	if (!wlc) {
		WL_ERROR(("%s - null wlc\n", __FUNCTION__));
		goto fail;
	}
	if ((olpc_info = (wlc_olpc_eng_info_t *)MALLOCZ(wlc->osh, sizeof(wlc_olpc_eng_info_t)))
		== NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}

	olpc_info->wlc = wlc;
	olpc_info->chan_list = NULL;
	olpc_info->npkts = WLC_OLPC_DEF_NPKTS;
	olpc_info->up = FALSE;
	olpc_info->last_chan_olpc_state = FALSE;
	olpc_info->last_chan_chk = INVCHANSPEC;
	olpc_info->last_txchain_chk = WLC_OLPC_INVALID_TXCHAIN;

	olpc_info->restore_perrate_stf_state = FALSE;
	olpc_info->cur_chan = NULL;
	olpc_info->updn = FALSE;
	olpc_info->assoc_notif_reg = FALSE;
	olpc_info->msglevel = 0;
	olpc_info->olpc_cal_done = 0;
	olpc_info->olpc_override = 0;
	olpc_info->cal_timer = wl_init_timer(wlc->wl, wlc_olpc_cal_timer, olpc_info, "olpctimer");

	if (olpc_info->cal_timer == NULL) {
		WL_ERROR(("wl%d: %s: wl_init_timer for OLPC cal timer failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* register module up/down, watchdog, and iovar callbacks */
	if (wlc_module_register(wlc->pub, olpc_iovars, "olpc", olpc_info, wlc_olpc_doiovar,
	                        NULL, wlc_olpc_eng_up, wlc_olpc_eng_down)) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	WL_NONE(("%s - end\n", __FUNCTION__));
	/* bsscfg up/down callback */
	if (wlc_bsscfg_updown_register(wlc, wlc_olpc_bsscfg_updn, olpc_info) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_updown_register() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	olpc_info->updn = TRUE;
	if (wlc_bss_assoc_state_register(wlc,
		wlc_olpc_bsscfg_assoc_state_notif, olpc_info) !=
		BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_assoc_state_notif_register() failed\n",
			wlc->pub->unit, __FUNCTION__));
	}
	olpc_info->assoc_notif_reg = TRUE;

#if defined(BCMDBG) || defined(WLTEST)
	wlc_dump_register(wlc->pub, "olpc", (dump_fn_t)wlc_dump_olpc, (void *)wlc);
#endif // endif
	err = wlc_pcb_fn_set(wlc->pcb, 0, WLF2_PCB1_OLPC, wlc_olpc_eng_pkt_complete);
	if (err != BCME_OK) {
		WL_ERROR(("%s: wlc_pcb_fn_set err=%d\n", __FUNCTION__, err));
		goto fail;
	}

	return olpc_info;
fail:
	MODULE_DETACH(olpc_info, wlc_olpc_eng_detach);
	return NULL;
}

/* go through and free all chan info */
static void
wlc_olpc_free_chans(struct wlc_olpc_eng_info_t *olpc_info)
{
	wlc_olpc_eng_chan_t* cur_chan = olpc_info->chan_list;
	wlc_info_t *wlc = olpc_info->wlc;

	while (cur_chan) {
		cur_chan = cur_chan->next;
		MFREE(wlc->osh, olpc_info->chan_list, sizeof(wlc_olpc_eng_chan_t));
		olpc_info->chan_list = cur_chan;
	}
	/* no longer a valid field */
	olpc_info->cur_chan = NULL;
	olpc_info->core_idx = 0;
}

/* module detach, up, down */
void
BCMATTACHFN(wlc_olpc_eng_detach)(struct wlc_olpc_eng_info_t *olpc_info)
{
	wlc_info_t *wlc;

	if (olpc_info == NULL) {
		return;
	}
	if (olpc_info->cal_timer) {
		WLC_OLPC_FREE_TIMER(olpc_info);
		olpc_info->cal_timer = NULL;
	}

	wlc = olpc_info->wlc;
	wlc_olpc_free_chans(olpc_info);
	wlc_module_unregister(wlc->pub, "olpc", olpc_info);
	if (olpc_info->updn) {
		wlc_bsscfg_updown_unregister(olpc_info->wlc, wlc_olpc_bsscfg_updn, olpc_info);
	}
	if (olpc_info->assoc_notif_reg) {
		wlc_bss_assoc_state_unregister(wlc,
			wlc_olpc_bsscfg_assoc_state_notif, olpc_info);
	}
	MFREE(wlc->osh, olpc_info, sizeof(struct wlc_olpc_eng_info_t));
}

static int
wlc_olpc_eng_up(void *hdl)
{
	struct wlc_olpc_eng_info_t *olpc_info;
	olpc_info = (struct wlc_olpc_eng_info_t *)hdl;
	if (olpc_info->up) {
		return BCME_OK;
	}
	olpc_info->num_hw_cores = (uint8)WLC_BITSCNT(olpc_info->wlc->stf->hw_txchain);

	/* call chan_update before setting 'up' flag to avoid doing chan_update before channel
	 * settings are plumbed in initial 'wl up' scenario; chan_update will get called later
	 * on 'BSS up' and/or channel change. chan_update call is kept here for reinit scenarios.
	 */
	wlc_olpc_eng_hdl_chan_update(olpc_info);
	olpc_info->up = TRUE;

	return BCME_OK;
}

static int
wlc_olpc_eng_down(void *hdl)
{
	struct wlc_olpc_eng_info_t *olpc_info;
	olpc_info = (struct wlc_olpc_eng_info_t *)hdl;
	if (!olpc_info->up) {
		return BCME_OK;
	}
	olpc_info->up = FALSE;
	/* clear ppr query cache */
	olpc_info->last_chan_olpc_state = FALSE;
	olpc_info->last_chan_chk = INVCHANSPEC;

	/* clear cal info */
	wlc_olpc_free_chans(olpc_info);

	/* delete any timer */
	WLC_OLPC_DEL_TIMER(olpc_info);

	return BCME_OK;
}

/** OLPC timer callback function */
static void
wlc_olpc_cal_timer(void *arg)
{
	wlc_olpc_eng_info_t *olpc = (wlc_olpc_eng_info_t *)arg;
	int ret = BCME_OK;

	WL_OLPC(olpc, ("%s: cores_cal = 0x%x, txchain = 0x%x, last_txchain_chk = 0x%x\n",
		__FUNCTION__, olpc->cur_chan->cores_cal, olpc->wlc->stf->txchain,
		olpc->last_txchain_chk));

	olpc->core_idx++;

	if (olpc->cur_chan->cores_cal == olpc->last_txchain_chk) {
		WL_OLPC(olpc, ("%s: exit: open loop phy CAL done for all chains!\n",
			__FUNCTION__));
		olpc->core_idx = 0;
		olpc->cur_chan->cores_cal = 0;
		WLC_OLPC_DEL_TIMER(olpc);
		olpc->timer_active = FALSE;
	} else {
		olpc->core_idx = wlc_olpc_find_next_active_core(olpc);
		WL_OLPC_DBG(olpc, ("%s: starting cal for core %d\n", __FUNCTION__, olpc->core_idx));
		ret = wlc_olpc_eng_recal_ex(olpc->wlc, olpc->npkts);
	}

	if (ret != BCME_OK) {
		WL_OLPC_DBG(olpc, ("%s: cal broke, resetting OLPC engine\n", __FUNCTION__));
		wlc_olpc_eng_reset(olpc);
	}
}

static uint8
wlc_olpc_find_next_active_core(wlc_olpc_eng_info_t *olpc)
{
	uint8 txchain = olpc->wlc->stf->txchain;
	uint8 core_idx = olpc->core_idx;

	while (((1 << core_idx) & txchain) == 0) {
		core_idx++;
		if (core_idx > olpc->num_hw_cores) {
			WL_OLPC_DBG(olpc, ("%s: we should not get here!!!\n", __FUNCTION__));
			ASSERT(0);
			return -1;
		}
	}
	return core_idx;
}
#endif /* WLOLPC */
