/*
 * WLC RSDB API Implementation
 * Broadcom 802.11abg Networking Device Driver
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
 * $Id: wlc_rsdb.c 780883 2019-11-05 14:19:22Z $
 */

/**
 * @file
 * @brief
 * Real Simultaneous Dual Band operation
 */

/**
 * @file
 * @brief
 * XXX Twiki: [RealSimultaneousDualBand]
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */

#ifdef WLRSDB

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <bcmdevs.h>
#include <osl.h>
#include <bcmutils.h>
#include <wlioctl.h>
#include <802.11.h>
#include <wl_dbg.h>
#include <wlc_types.h>
#include <siutils.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_rsdb.h>
#include <wl_export.h>
#include <wlc_scan.h>
#include <wlc_assoc.h>
#include <wlc_rrm.h>
#include <wlc_stf.h>
#include <wlc_bmac.h>
#include <phy_api.h>
#include <wlc_scb.h>
#include <wlc_vht.h>
#include <wlc_ht.h>
#include <wlc_bmac.h>
#include <wlc_scb_ratesel.h>
#include <wlc_txc.h>
#include <wlc_tx.h>
#include <wlc_txbf.h>
#include <wlc_ap.h>
#include <wlc_amsdu.h>
#include <wlc_rsdb_policymgr.h>
#ifdef PROP_TXSTATUS
#include <wlc_wlfc.h>
#endif /* PROP_TXSTATUS */
#ifdef WLAMPDU
#include <wlc_ampdu.h>
#include <wlc_ampdu_rx.h>
#include <wlc_ampdu_cmn.h>
#endif // endif
#ifdef WLP2P
#include <wlc_p2p.h>
#endif // endif
#ifdef WLMCHAN
#include <wlc_mchan.h>
#endif // endif
#ifdef WL_MODESW
#include <wlc_modesw.h>
#endif // endif
#include <wlc_11h.h>
#include <wlc_btcx.h>
#ifdef BCMLTECOEX
#include <wlc_ltecx.h>
#endif /* BCMLTECOEX */
#include <phy_radio_api.h>
#ifdef DONGLEBUILD
#include <rte_trap.h>
#endif // endif

#include <wlc_dfs.h>
#include <wlc_ie_mgmt.h>
#include <wlc_ie_mgmt_ft.h>

#ifdef BCMRESVFRAGPOOL
#include <hnd_resvpool.h>
#endif // endif

#ifdef WL_ASDB
#include <wlc_asdb.h>
#endif // endif

#include <wlc_hw.h>
#include <wlc_event.h>
#include <wlc_keymgmt.h>

#ifdef WLTDLS
#include <wlc_tdls.h>
#endif // endif
#include <wlc_pm.h>
#include <wlc_rm.h>
#include <wlc_dump.h>
#include <wlc_iocv.h>
#include <wlc_ampdu_cmn.h>
#include <wlc_lq.h>
#include <wlc_txmod.h>
#ifdef WL_MU_RX
#include <wlc_murx.h>
#endif /* WL_MU_RX */
#if defined(PKTC) || defined(PKTC_DONGLE)
#include <wlc_pktc.h>
#endif /* defined(PKTC) || defined(PKTC_DONGLE) */

#ifdef BCMDBG
#define WLRSDB_DBG(x) WL_NONE(x)
#else
#define WLRSDB_DBG(x)
#endif // endif

#include<phy_tpc_api.h>
#include <wlc_rsdb_priv.h>

int phymode2swmode(int16 mode);
#if defined(WLCHANIM)
static void wlc_rsdb_check_load_based_upgarde(wlc_rsdb_info_t* rsdbinfo);
#endif /* WLCHANIM */

#define SWMODE2PHYMODE(x) swmode2phymode[(x)]
#define PHYMODE2SWMODE(x) phymode2swmode(x)
#define UNIT(ptr)	((ptr)->pub->unit)

typedef enum rsdb_core_cap_bw {
	WL_RSDB_CORE_CAP_BW_NONE = 0x00,
	WL_RSDB_CORE_CAP_BW_2P5 = 0x01,
	WL_RSDB_CORE_CAP_BW_5 = 0x02,
	WL_RSDB_CORE_CAP_BW_10 = 0x04,
	WL_RSDB_CORE_CAP_BW_20 = 0x08,
	WL_RSDB_CORE_CAP_BW_40 = 0x10,
	WL_RSDB_CORE_CAP_BW_80 = 0x20,
	WL_RSDB_CORE_CAP_BW_160 = 0x40,
	WL_RSDB_CORE_CAP_BW_8080 = 0x80
} rsdb_core_cap_bw_t;

typedef enum rsdb_core_cap_band {
	WL_RSDB_CORE_CAP_BAND_NONE = 0x00,
	WL_RSDB_CORE_CAP_BAND_2G = 0x01,
	WL_RSDB_CORE_CAP_BAND_3G = 0x02,
	WL_RSDB_CORE_CAP_BAND_4G = 0x04,
	WL_RSDB_CORE_CAP_BAND_5G = 0x08
} rsdb_core_cap_band_t;

enum rsdb_clone_tmr_type {
	RSDB_TMR_TYPE_AP_CLONE = 0,
	RSDB_TMR_TYPE_STA_ASSOC = 1,
	RSDB_TMR_TYPE_STA_CLONE = 2,
	RSDB_TMR_TYPE_NONE = 3,
	RSDB_TMR_TYPE_IBSS_CLONE = 4
} rsdb_clone_tmr_type_t;

/* The offsets should match the values of
* bw inside chanspec defined in bcmwifi_channels.h
*/
rsdb_core_cap_bw_t rsdb_core_bw_map[] = {
	WL_RSDB_CORE_CAP_BW_5,
	WL_RSDB_CORE_CAP_BW_10,
	WL_RSDB_CORE_CAP_BW_20,
	WL_RSDB_CORE_CAP_BW_40,
	WL_RSDB_CORE_CAP_BW_80,
	WL_RSDB_CORE_CAP_BW_160,
	WL_RSDB_CORE_CAP_BW_8080,
	WL_RSDB_CORE_CAP_BW_2P5
};

/* The offsets should match the values of
* band inside chanspec defined in bcmwifi_channels.h
*/
rsdb_core_cap_band_t rsdb_core_band_map[] = {
	WL_RSDB_CORE_CAP_BAND_2G,
	WL_RSDB_CORE_CAP_BAND_3G,
	WL_RSDB_CORE_CAP_BAND_4G,
	WL_RSDB_CORE_CAP_BAND_5G
};

typedef struct wlc_rsdb_core_cap {
	rsdb_core_cap_band_t band;
	rsdb_core_cap_bw_t bw;
	uint8 chains;
} wlc_rsdb_core_cap_t;

typedef struct rsdb_assoc_cb_info {
	rsdb_assoc_mode_change_cb_t cb; /* assoc mode change callback */
	void * ctx;
} rsdb_assoc_cb_info_t;

typedef struct rsdb_get_wlc_cb_info {
	rsdb_get_wlcs_cb_t cb; /* get_wlcs callback */
	void * ctx;
} rsdb_get_wlc_cb_info_t;

typedef struct rsdb_timer_cb {
	wlc_info_t *wlc;
	wlc_bsscfg_t *cfg;
	uint8 notif_type;
} rsdb_timer_cb_t;

struct wlc_rsdb_info {
	wlc_info_t *wlc;
	int scb_handle;			/* scb cubby handle */
	rsdb_cmn_info_t *cmn;		/* common rsdb info structure */
	rsdb_assoc_cb_info_t *as_cbinfo;
	rsdb_get_wlc_cb_info_t *rsdb_wlc_cbinfo;
	uint32	load_threshold;		/* Threshold for moving to 3x3 chain */
	rsdb_chan_sw_info_t *chan_sw_info;	/* */
};

#define RSDB_IOV_MAX_REGS 2

struct rsdb_common_info {
	int8 def_ampdu_mpdu;		/* default num of mpdu in ampdu */
	bcm_notif_h	cfg_clone_notif_hdl;
	struct wl_timer *rsdb_clone_timer;
	rsdb_timer_cb_t *timer_ctx;
	uint8		cmn_bwcap[MAXBANDS];	/**< Bandwidth bitmask for each band */
	wlc_rsdb_core_cap_t core_cap[MAX_RSDB_MAC_NUM]; /* Core capabilites for each slice */

	/* RSDB linked cfg indices, array size is WLC_MAXBSSCFG
	 * if linked bsscfgs are 2 and 5, arr[2] = 5, arr[5] = 2
	 */
	int8 *linked_cfg_indices;
	/* RSDB linked cfg host if indices, array size is WLC_MAXBSSCFG
	 * if linked bsscfgs are 2 and 5, arr[2] = arr[5] = host_if_idx
	 * which is notified to host on cfg creation.
	 */
	int *linked_cfg_hostif_indices;
	bcm_iov_parse_context_t *iov_parse_ctx;
};

/* Maximum number of channel switch information stored per WLC */
#define MAX_RSDB_CHANSW_INFO	16

/*
 * Structure to store the last N channels on a particular core
 */
struct rsdb_chan_sw_info {
	uint32 last_switch_time;
	uint16 w_ptr;
	uint32 dwelltime[MAX_RSDB_CHANSW_INFO];
	chanspec_t chanspec[MAX_RSDB_CHANSW_INFO];
};

#define RSDB_AMPDU_MPDU_INIT_VAL	0

/* scb cubby functions and fields */

typedef struct wlc_rsdb_scb_cubby
{
	uint8	mcs[MCSSET_LEN];
	bool	isapmimo;
} wlc_rsdb_scb_cubby_t;

/* iovar table */
enum {
	IOV_RSDB = 0,
	IOV_RSDB_MODE,		/* Possible Values: -1(auto), 0(2x2), 1(rsdb), 2(80p80) */
	IOV_RSDB_COREMPC,		/* Possible Values: 0(Disable), 1(Enable) mpc per chain */
	IOV_RSDB_SWITCH,
	IOV_RSDB_MOVE,
	IOV_RSDB_LOAD_THRESH,
	IOV_RSDB_PMMOVE_OVR,
	IOV_RSDB_LAST
};

#define RSDB_IEM_BUILD_CB_MAX 0
#define RSDB_IEM_PARSE_CB_MAX 1

/* Num antennas */
#define NON_DUALMAC_NUM_CHAINS_CORE0 1

static const bcm_iovar_t rsdb_iovars[] = {
	/* Used for new xtlv format */
	{"rsdb", IOV_RSDB, 0, 0, IOVT_BUFFER, 0},
	{"rsdb_mode", IOV_RSDB_MODE, IOVF_SET_DOWN, 0, IOVT_BUFFER, sizeof(wl_config_t)},
#if defined(WLTEST)
	{"rsdb_corempc", IOV_RSDB_COREMPC, IOVF_OPEN_ALLOW, 0, IOVT_BOOL, 0},
#endif // endif
#if defined(WLTEST) || defined(RSDB_SWITCH)
	{"rsdb_switch", IOV_RSDB_SWITCH, 0, 0, IOVT_UINT32, 0},
#if defined(RSDB_PM_MODESW)
	{"rsdb_move", IOV_RSDB_MOVE, (0), 0, IOVT_UINT32, 0},
#endif // endif
#endif // endif
	{"rsdb_load_thresh", IOV_RSDB_LOAD_THRESH, (0), 0, IOVT_UINT32, 0},
	{"rsdb_pmmove_ovr", IOV_RSDB_PMMOVE_OVR, (0), 0, IOVT_UINT32, 0},
	{NULL, 0, 0, 0, 0, 0}
};

/* bcmiov related functions */
static void * rsdb_iov_context_alloc(void *ctx, uint size);
static void rsdb_iov_context_free(void *ctx, void *iov_ctx, uint size);
static int BCMATTACHFN(rsdb_iov_get_digest_cb)(void *ctx, bcm_iov_cmd_digest_t **dig);
static int rsdb_get_ver(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen);
static int rsdb_get_caps(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen);
static int rsdb_get_bands(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen);

static const bcm_iov_cmd_info_t rsdb_sub_cmds[] = {
	{WL_RSDB_CMD_VER, BCM_IOV_CMD_FLAG_NONE,
	0, BCM_XTLV_OPTION_ALIGN32,
	rsdb_iov_cmd_validate, rsdb_get_ver, NULL, 0,
	0, 0, 0, 0
	},
	{WL_RSDB_CMD_CAPS, BCM_IOV_CMD_FLAG_NONE,
	0, BCM_XTLV_OPTION_ALIGN32,
	rsdb_iov_cmd_validate, rsdb_get_caps, NULL, 0,
	0, 0, 0, 0
	},
	{WL_RSDB_CMD_BANDS, BCM_IOV_CMD_FLAG_NONE,
	0, BCM_XTLV_OPTION_ALIGN32,
	rsdb_iov_cmd_validate, rsdb_get_bands, NULL, 0,
	0, 0, 0, 0
	},
};

#ifdef WLRSDB_DVT
static const char *rsdb_gbl_iovars[] = {"mpc", "apsta", "country", "event_msgs"};
#endif // endif

static wlc_info_t * wlc_rsdb_find_wlc(wlc_info_t *wlc, wlc_bsscfg_t *join_cfg, wlc_bss_info_t *bi);
static int wlc_rsdb_get_max_chain_cap(wlc_info_t *wlc);
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_RSDB)
#include <wlc_dbg.h>
static int wlc_rsdb_dump(void *wlc, struct bcmstrbuf *b);
#endif // endif

#ifdef WL_MODESW
static void wlc_rsdb_opmode_change_cb(void *ctx, wlc_modesw_notif_cb_data_t *notif_data);
static void wlc_rsdb_clone_timer_cb(void *arg);
#endif // endif
static void wlc_rsdb_mode_change_complete(wlc_info_t *wlc);
static int wlc_rsdb_update_hw_mode(wlc_info_t *from_wlc, uint32 to_mode);
static void wlc_rsdb_watchdog(void *context);
static bool wlc_rsdb_peerap_ismimo(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
static int wlc_rsdb_down(void *context);
static int swmode2phymode[] = {PHYMODE_MIMO, PHYMODE_RSDB, PHYMODE_80P80, -1};
static void wlc_rsdb_override_auto_mode_switch(wlc_info_t *wlc, uint32 reason);
static void wlc_rsdb_restore_auto_mode_switch(wlc_info_t *wlc, uint32 reason);
static int wlc_rsdb_ht_parse_cap_ie(void *ctx, wlc_iem_parse_data_t *parse);
static int wlc_rsdb_vht_parse_cap_ie(void *ctx, wlc_iem_parse_data_t *parse);
static void wlc_rsdb_update_scb_rateset(wlc_rsdb_info_t *rsdbinfo, struct scb *scb);
static int
wlc_rsdb_auto_assoc_mode_change(wlc_bsscfg_t **pcfg, wlc_bss_info_t *bi);
static void wlc_rsdb_clone_assoc(wlc_bsscfg_t *from_cfg, wlc_bsscfg_t *to_cfg);
static void wlc_rsdb_scb_reassoc(wlc_info_t *to_wlc, wlc_bsscfg_t *to_cfg);
static void wlc_rsdb_sta_assoc_upd(wlc_bsscfg_t *cfg, bool state);
static void wlc_rsdb_scb_state_upd_cb(void *ctx, scb_state_upd_data_t *notif_data);
static int wlc_rsdb_trigger_modeswitch(wlc_bsscfg_t *bsscfg, int8 chain);
static int wlc_dualmac_rsdb_monitor(wlc_info_t *wlc);
static void wlc_rsdb_get_bands(wlc_info_t *wlc, rsdb_bands_v1_t *rsdb_ptr);
static void wlc_rsdb_schedule_timer(wlc_rsdb_info_t* rsdbinfo, struct wl_timer* tmr,
	uint ms, int periodic);
static uint8 wlc_rsdb_map_core_bw(chanspec_t chanspec);
static uint8 wlc_rsdb_map_core_band(chanspec_t chanspec);
static void wlc_rsdb_create_reqd_cap(wlc_info_t *wlc, chanspec_t chanspec,
	wlc_bss_info_t *bi, wlc_rsdb_core_cap_t* out_cap);
static int wlc_rsdb_preferred_wlc(wlc_rsdb_info_t *rsdbinfo, wlc_info_t** list, chanspec_t chanspec,
	wlc_bss_info_t *bi,	uint32 match_mask);
static void wlc_rsdb_derive_core_cap(wlc_rsdb_info_t *rsdbinfo);
static rsdb_core_cap_bw_t wlc_rsdb_get_core_bw(wlc_rsdb_info_t *rsdbinfo);
static rsdb_core_cap_band_t wlc_rsdb_get_core_band(wlc_rsdb_info_t *rsdbinfo);
#if defined(RSDB_CMN_BANDSTATE) && !defined(RSDB_CMN_BANDSTATE_DISABLED)
static void wlc_rsdb_derive_cmn_bwcap(wlc_rsdb_info_t *rsdbinfo, int bandtype);
#endif /* defined(RSDB_CMN_BANDSTATE) && !defined(RSDB_CMN_BANDSTATE_DISABLED) */
static int wlc_rsdb_scb_update(wlc_info_t *wlc_from, wlc_info_t *wlc_to, wlc_bsscfg_t *cfg_to,
	struct scb *scb);
static void wlc_rsdb_link_teardown(wlc_info_t *wlc, struct scb *scb, uint8 *activefid);
static void wlc_rsdb_link_rebind(wlc_info_t *wlc, struct scb *scb, uint8 *activefid);
static void wlc_rsdb_bsscfg_post_clone(wlc_info_t *from_wlc, wlc_info_t *to_wlc,
	wlc_bsscfg_t* to_cfg);
static void wlc_rsdb_bsscfg_pre_assoc_clone(wlc_info_t *from_wlc, wlc_info_t *to_wlc,
	wlc_bsscfg_t *from_cfg);
static void wlc_rsdb_bsscfg_post_assoc_clone(wlc_info_t *from_wlc, wlc_info_t *to_wlc,
	wlc_bsscfg_t* to_cfg);
static void wlc_rsdb_bsscfg_pre_clone(wlc_info_t *from_wlc, wlc_info_t *to_wlc,
	wlc_bsscfg_t *from_cfg);
static int wlc_rsdb_cleanup_from_bsscfg(wlc_info_t *from_wlc, wlc_info_t *to_wlc,
	wlc_bsscfg_t *from_cfg,	wlc_bsscfg_t* to_cfg, int idx, int8 unused_idx);
static wlc_bsscfg_t * wlc_rsdb_allocate_clone_bsscfg(wlc_info_t *from_wlc, wlc_info_t *to_wlc,
	wlc_bsscfg_t *from_cfg, int8 *out_inactive_idx);
#if defined(WLTEST)
static bool wlc_rsdb_set_core_band_lock(wlc_rsdb_info_t *rsdbinfo, bool is_bandswitch);
#endif /* WLTEST */

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

int phymode2swmode(int16 mode)
{
	uint8 i = 0;
	while (swmode2phymode[i] != -1) {
		if (swmode2phymode[i] == mode)
			return i;
		i++;
	}
	return -1;
}

static void
wlc_rsdb_bss_state_upd(void *ctx, bsscfg_state_upd_data_t *evt_data)
{
	wlc_rsdb_info_t *rsdbinfo = (wlc_rsdb_info_t *)ctx;
	wlc_info_t *wlc = rsdbinfo->wlc;
	wlc_bsscfg_t *bsscfg = evt_data->cfg;
	UNUSED_PARAMETER(wlc);
	/* Ensure that the CFG is being DISABLED */
	if (!bsscfg->enable && !bsscfg->up && evt_data->old_enable) {
		if (!(BSSCFG_IS_RSDB_CLONE(bsscfg)) &&
			TRUE) {
#if defined(WL_MODESW) && defined(WLRSDB)
			if (RSDB_ENAB(wlc->pub) && WLC_MODESW_ENAB(wlc->pub)) {
				wlc_modesw_move_cfgs_for_mimo(wlc->modesw);
			}
#endif /* WL_MODESW && WLRSDB */
		}
	}
}

/* This API deletes and adds the given timer */
static void
wlc_rsdb_schedule_timer(wlc_rsdb_info_t* rsdbinfo, struct wl_timer* tmr, uint ms, int periodic)
{
	wlc_info_t *wlc = rsdbinfo->wlc;

	wl_del_timer(wlc->wl, tmr);
	wl_add_timer(wlc->wl, tmr, ms, periodic);
	return;
}

int
wlc_rsdb_handle_sta_csa(wlc_rsdb_info_t* rsdbinfo, wlc_bsscfg_t *bsscfg, chanspec_t chanspec)
{
	wlc_info_t* wlc = rsdbinfo->wlc;
	wlc_info_t* to_wlc = wlc_rsdb_find_wlc_for_chanspec(wlc, NULL, chanspec, NULL, 0);
	if (to_wlc == wlc) {
		return BCME_OK;
	}
	if (wlc->rsdbinfo->cmn->timer_ctx->notif_type == RSDB_TMR_TYPE_NONE) {
		wlc->rsdbinfo->cmn->timer_ctx->wlc = wlc;
		wlc->rsdbinfo->cmn->timer_ctx->cfg = bsscfg;
		wlc->rsdbinfo->cmn->timer_ctx->notif_type = RSDB_TMR_TYPE_STA_CLONE;
		wlc_rsdb_schedule_timer(wlc->cmn->wlc[1]->rsdbinfo,
			rsdbinfo->cmn->rsdb_clone_timer, 0, FALSE);
	}
	/* Updating the current chanspec for the cfg
	* because associated clone will try to program
	* the current chanspec on the new core.
	*/
	bsscfg->current_bss->chanspec = chanspec;

	return BCME_ASSOCIATED;
}

int
wlc_rsdb_ap_bringup(wlc_rsdb_info_t* rsdbinfo, wlc_bsscfg_t** cfg_p)
{
	wlc_info_t* wlc = rsdbinfo->wlc;
	wlc_info_t *to_wlc = NULL;
	wlc_bsscfg_t *from_cfg;
	int ret = BCME_OK;
	wlc_bsscfg_t* cfg = NULL;

	/* If rsdb mode is not auto and in mimo  */
	if (!(wlc->cmn->rsdb_mode & WLC_RSDB_MODE_AUTO_MASK) &&
		WLC_RSDB_SINGLE_MAC_MODE(WLC_RSDB_CURR_MODE(wlc)))
		return BCME_OK;

	cfg = *cfg_p;
	to_wlc = wlc_rsdb_find_wlc_for_chanspec(wlc, cfg->target_bss,
		cfg->target_bss->chanspec, NULL, WL_RSDB_CORE_MATCH_BW);

	if (WLC_DUALMAC_RSDB(wlc->cmn) && (wlc != to_wlc)) {
		WLRSDB_DBG(("wl%d: Defer AP bringup due to"
			"RSDB scenario.\n", WLCWLUNIT(wlc)));
		if (WLC_DUALMAC_RSDB(wlc->cmn) && (wlc != to_wlc)) {
			wlc->rsdbinfo->cmn->timer_ctx->wlc = wlc;
			wlc->rsdbinfo->cmn->timer_ctx->cfg = cfg;
			wlc->rsdbinfo->cmn->timer_ctx->notif_type = RSDB_TMR_TYPE_AP_CLONE;
			WLRSDB_DBG(("wl%d.%d: Schedule the timer for clone of the AP CFG\n",
				WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg)));
			wlc_rsdb_schedule_timer(wlc->cmn->wlc[1]->rsdbinfo,
				rsdbinfo->cmn->rsdb_clone_timer, 0, FALSE);
		}
		return BCME_ASSOCIATED;
	}

#ifdef WL_MODESW
	/* Update the oper mode of the cfg based on chanspec */
	if (WLC_MODESW_ENAB(wlc->pub) && cfg->oper_mode_enabled) {
		cfg->oper_mode = wlc_modesw_derive_opermode(wlc->modesw,
			cfg->target_bss->chanspec, cfg, wlc->stf->op_rxstreams);
	}
	/* Not valid for non-RSDB mac as there wont be any modeswitch in that
	 * case
	 */
	if (wlc->cmn->rsdb_mode & WLC_RSDB_MODE_AUTO_MASK) {
		/* When AP comes up as the first connection in wlc[0], */
		/* check for MIMO upgrade from RSDB */
		if ((to_wlc == wlc->cmn->wlc[0]) &&
			(wlc_rsdb_association_count(wlc) == 0)) {
			if (WLC_RSDB_DUAL_MAC_MODE(WLC_RSDB_CURR_MODE(wlc))) {
				ret = wlc_rsdb_upgrade_wlc(wlc);
				return ret;
			}
		}
	}
#endif /* WL_MODESW */

	/* When we bringup AP in wlc[0] and if STA is already associated in it */
	/* first STA downgrades to RSDB if in MIMO */
	/* then the STA is moved to other_wlc. */
	if ((wlc != to_wlc) && (wlc->stas_associated == 1)) {

#ifdef WL_MODESW
		if (WLC_MODESW_ENAB(wlc->pub) &&
			(WLC_RSDB_SINGLE_MAC_MODE(WLC_RSDB_CURR_MODE(wlc))) &&
			(wlc->cmn->rsdb_mode & WLC_RSDB_MODE_AUTO_MASK)) {
				/* Allow downgarde of wlc's only if mchan is not active */
				if (!MCHAN_ACTIVE(wlc->pub)) {
					wlc_rsdb_downgrade_wlc(wlc);
				} else {
					return BCME_OK;
				}
		}
#endif // endif
		if (WLC_RSDB_DUAL_MAC_MODE(WLC_RSDB_CURR_MODE(wlc))) {
			wlc_bsscfg_t *icfg;
			wlc_bsscfg_t *to_cfg;
			WLRSDB_DBG(("associated cfg move from wlc %d to wlc %d",
				wlc->pub->unit, to_wlc->pub->unit));

			icfg = wlc_bsscfg_primary(wlc);
			from_cfg = (BSSCFG_AS_STA(icfg) ? icfg : NULL);

			if (from_cfg != NULL) {
				WLRSDB_DBG(("Found AS-STA cfg[wl%d.%d] for clone\n",
					wlc->pub->unit, WLC_BSSCFG_IDX(from_cfg)));
				to_cfg = wlc_rsdb_bsscfg_clone(wlc, to_wlc, from_cfg, &ret);
				ASSERT(to_cfg != NULL);
				if (ret != BCME_OK || to_cfg == NULL)
					return BCME_ERROR;
			}
			else {
				WL_ERROR(("wl%d Unsupported RSDB operation\n",
					WLCWLUNIT(wlc)));
				ASSERT(FALSE);
				return BCME_ERROR;
			}

		}
	}
	/* When we bringup AP in wlc[0] and if AP is already associated in it */
	/* first AP downgrades to RSDB if in MIMO */
	/* then the AP is moved to other_wlc. */
	else if ((wlc != to_wlc) && BSSCFG_AP(cfg) &&
		(!to_wlc->aps_associated) &&
			(wlc->aps_associated) &&
			(!wlc->stas_associated) &&
			(!to_wlc->stas_associated)) {

#ifdef WL_MODESW
			/* first AP downgrades to RSDB if in MIMO */
			if (WLC_MODESW_ENAB(wlc->pub) &&
				(WLC_RSDB_SINGLE_MAC_MODE(WLC_RSDB_CURR_MODE(wlc))) &&
				(wlc->cmn->rsdb_mode & WLC_RSDB_MODE_AUTO_MASK)) {

				/* Allow downgarde of wlc's only if mchan is not active */
					if (!MCHAN_ACTIVE(wlc->pub)) {
						wlc_rsdb_downgrade_wlc(wlc);
					} else {
						return BCME_OK;
					}
			}
#endif // endif
			/* If the system in RSDB mode */
			if (WLC_RSDB_DUAL_MAC_MODE(WLC_RSDB_CURR_MODE(wlc))) {
				wlc_bsscfg_t *to_cfg;

				/* Clone AP cfg to other_wlc. */
				to_cfg = wlc_rsdb_bsscfg_clone(wlc, to_wlc, cfg, &ret);
				ASSERT(to_cfg != NULL);
				if (ret != BCME_OK || to_cfg == NULL)
					return BCME_ERROR;
				*cfg_p = to_cfg;
			}
	} else if ((wlc != to_wlc) && (to_wlc == wlc->cmn->wlc[1])) {
			WL_ERROR(("wl%d Unsupported RSDB operation\n",
				WLCWLUNIT(wlc)));
			return BCME_OK;
	}
	return BCME_OK;
}

/* This function check if RSDB conditions favour bringup of IBSS connection. If possible then it
 * returns BCME_OK else IBSS connection is deferred to clone timer/modesw module
 */
int
wlc_rsdb_ibss_bringup(wlc_info_t* wlc, wlc_bsscfg_t** cfg_p)
{
	wlc_info_t *to_wlc = NULL;
	wlc_bsscfg_t *from_cfg;
	int ret;
	wlc_bsscfg_t* cfg = NULL;
	/* If rsdb mode is not auto and in mimo  */
	if (!(WLC_RSDB_IS_AUTO_MODE(wlc)) &&
		WLC_RSDB_SINGLE_MAC_MODE(WLC_RSDB_CURR_MODE(wlc))) {
		return BCME_OK;
	}

	cfg = *cfg_p;
	to_wlc = wlc_rsdb_find_wlc_for_chanspec(wlc, cfg->target_bss,
			cfg->target_bss->chanspec, NULL, WL_RSDB_CORE_MATCH_BW);

	if (WLC_DUALMAC_RSDB(wlc->cmn) && (to_wlc != wlc)) {
		ASSERT(wlc->rsdbinfo->cmn->timer_ctx->notif_type == RSDB_TMR_TYPE_NONE);

		wlc->rsdbinfo->cmn->timer_ctx->wlc = wlc;
		wlc->rsdbinfo->cmn->timer_ctx->cfg = cfg;
		wlc->rsdbinfo->cmn->timer_ctx->notif_type = RSDB_TMR_TYPE_IBSS_CLONE;
		WLRSDB_DBG(("wl%d.%d: Schedule the timer for completing"
			" STA assoc\n", WLCWLUNIT(wlc),
			WLC_BSSCFG_IDX(cfg)));
		cfg->ibss_up_pending = TRUE;
		wlc_rsdb_schedule_timer(wlc->cmn->wlc[1]->rsdbinfo,
			wlc->rsdbinfo->cmn->rsdb_clone_timer, 0, FALSE);
		return BCME_NOTREADY;
	} else if (!WLC_DUALMAC_RSDB(wlc->cmn) && WLC_MODESW_ENAB(wlc->pub) &&
			WLC_RSDB_IS_AUTO_MODE(wlc)) {
		/* Update the oper mode of the cfg based on chanspec */
		if (cfg->oper_mode_enabled) {
			cfg->oper_mode = wlc_modesw_derive_opermode(wlc->modesw,
				cfg->target_bss->chanspec, cfg, wlc->stf->op_rxstreams);
		}

		/* When an IBSS comes up as the first connection in wlc[0],
		 * check for MIMO upgrade from RSDB
		 */
		if ((to_wlc == wlc->cmn->wlc[0]) &&
			(wlc_rsdb_association_count(wlc) == 0) &&
			WLC_RSDB_DUAL_MAC_MODE(WLC_RSDB_CURR_MODE(wlc)) &&
			wlc_rsdb_upgrade_allowed(wlc)) {
			wlc_rsdb_upgrade_wlc(wlc);

			/* If STA upgrade is not complete, set ibss_up_pending */
			if (WLC_RSDB_DUAL_MAC_MODE(WLC_RSDB_CURR_MODE(wlc))) {
				cfg->ibss_up_pending = TRUE;
				return BCME_NOTREADY;
			}

		}
		/* When we bringup IBSS if STA is already associated in wlc[0]
		 * first downgrade wlc to RSDB (DUAL MAC MODE) if in MIMO(SINGLE MAC MODE)
		 * After downgrade completes, move STA to other wlc and bringup IBSS in
		 * wlc[0]
		 */
		else if ((wlc != to_wlc) && (wlc_rsdb_association_count(wlc) == 1)) {
			/* Target wlc for rsdb is different and STA is associated in current
			 * wlc STA need to be moved to to_wlc in DUAL_MAC_MODE.
			 * Downgrade STA if RSDB mode is AUTO
			 */
			if (WLC_RSDB_SINGLE_MAC_MODE(WLC_RSDB_CURR_MODE(wlc))) {
				wlc_rsdb_downgrade_wlc(wlc);
			}
			/* Once downgrade is complete MAC is in DUAL_MAC_MODE
			 * carry out STA bsscfg cloning from from_cfg in wlc to to_cfg in to_wlc
			 */
			if (WLC_RSDB_DUAL_MAC_MODE(WLC_RSDB_CURR_MODE(wlc))) {
				wlc_bsscfg_t *to_cfg;
				WLRSDB_DBG(("associated primary move from wlc %d to wlc %d",
					wlc->pub->unit, to_wlc->pub->unit));
				if (WLC_MODESW_ENAB(wlc->pub)) {
					cfg->ibss_up_pending = FALSE;
				}
				from_cfg = wlc_bsscfg_primary(wlc);
				to_cfg = wlc_rsdb_bsscfg_clone(wlc, to_wlc, from_cfg, &ret);
				ASSERT(to_cfg != NULL);
				if (ret != BCME_OK || to_cfg == NULL) {
					WL_ERROR(("IBSS Clone Failed from wlc %d to wlc %d",
						wlc->pub->unit, to_wlc->pub->unit));
					wlc_join_done(cfg, NULL, DOT11_BSSTYPE_INDEPENDENT,
					              WLC_E_STATUS_FAIL);
					return BCME_NORESOURCE;
				}
				*cfg_p = to_cfg;
			}
			/* If STA downgrade is not complete, set ibss_up_pending */
			else {
				cfg->ibss_up_pending = TRUE;
				return BCME_NOTREADY;
			}
		}
	}
	return BCME_OK;
}

uint8
wlc_rsdb_association_count(wlc_info_t* wlc)
{

	wlc_info_t *wlc_iter;
	uint8 idx = 0, associations_count = 0;
	wlc_cmn_info_t *wlc_cmn = wlc->cmn;
	FOREACH_WLC(wlc_cmn, idx, wlc_iter) {
		associations_count = associations_count + wlc_iter->aps_associated +
			wlc_iter->stas_associated;
	}

	return associations_count;
}

/* This function checks if the BSS Info is capable
 * of MIMO or not. This is useful to identify if it is
 * required to JOIN in MIMO or RSDB mode.
 */
bool
wlc_rsdb_get_mimo_cap(wlc_bss_info_t *bi)
{
	bool mimo_cap = FALSE;
	if (bi->flags2 & WLC_BSS_VHT) {
		/* VHT capable peer */
		if ((VHT_MCS_SS_SUPPORTED(2, bi->vht_rxmcsmap))&&
			(VHT_MCS_SS_SUPPORTED(2, bi->vht_txmcsmap))) {
			mimo_cap = TRUE;
		}
	} else {
		/* HT Peer */
		/* mcs[1] tells the support for 2 spatial streams for Rx
		 * mcs[12] tells the support for 2 spatial streams for Tx
		 * which is populated if different from Rx MCS set.
		 * Refer Spec Section 8.4.2.58.4
		 */
		if (bi->rateset.mcs[1]) {
			/* Is Rx and Tx MCS set not equal ? */
			if (bi->rateset.mcs[12] & 0x2) {
				/* Check the Tx stream support */
				if (bi->rateset.mcs[12] & 0xC) {
					mimo_cap = TRUE;
				}
			}
			/* Tx MCS and Rx MCS
			 * are equel
			 */
			else {
				mimo_cap = TRUE;
			}
		}
	}
	return mimo_cap;
}

void wlc_rsdb_register_mode_change_cb(wlc_rsdb_info_t *rsdbinfo,
		rsdb_assoc_mode_change_cb_t cb, void *ctx)
{
	rsdbinfo->as_cbinfo->cb = cb;
	rsdbinfo->as_cbinfo->ctx = ctx;
	return;
}
void wlc_rsdb_register_get_wlcs_cb(wlc_rsdb_info_t *rsdbinfo,
	rsdb_get_wlcs_cb_t cb, void *ctx)
{
	rsdbinfo->rsdb_wlc_cbinfo->cb = cb;
	rsdbinfo->rsdb_wlc_cbinfo->ctx = ctx;
	return;
}

int
wlc_rsdb_assoc_mode_change(wlc_bsscfg_t **pcfg, wlc_bss_info_t *bi)
{
	wlc_bsscfg_t *cfg = *pcfg;
	wlc_rsdb_info_t  *rsdbinfo = cfg->wlc->rsdbinfo;
	int err = BCME_OK;
	if (!(rsdbinfo->wlc->cmn->rsdb_mode & WLC_RSDB_MODE_AUTO_MASK) &&
			WLRSDB_POLICY_MGR_ENAB(rsdbinfo->wlc->pub) &&
			rsdbinfo->as_cbinfo->cb) {
		err = rsdbinfo->as_cbinfo->cb(rsdbinfo->as_cbinfo->ctx, pcfg, bi);
	} else {
		err = wlc_rsdb_auto_assoc_mode_change(pcfg, bi);
	}
	return err;
}

static int
wlc_rsdb_auto_assoc_mode_change(wlc_bsscfg_t **pcfg, wlc_bss_info_t *bi)
{
	wlc_bsscfg_t *cfg = *pcfg;
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	wlc_info_t *to_wlc = NULL;
	wlc_info_t *from_wlc = wlc;
	wlc_info_t *other_wlc;
	to_wlc = wlc_rsdb_find_wlc(from_wlc, cfg, bi);
	other_wlc = wlc_rsdb_get_other_wlc(to_wlc);

	if (other_wlc == NULL) {
		WL_ERROR(("wl%d: other wlc is NULL\n", WLCWLUNIT(wlc)));
		return BCME_ERROR;
	}
#ifdef WL_MODESW
	/* Update the oper mode of the cfg based on chanspec */
	if (WLC_MODESW_ENAB(wlc->pub) && bi && cfg->oper_mode_enabled) {
		cfg->oper_mode = wlc_modesw_derive_opermode(wlc->modesw,
			bi->chanspec, cfg, wlc->stf->op_rxstreams);
	}
#endif /* WL_MODESW */
	if (as->state != AS_MODE_SWITCH_FAILED) {
		if (!WLC_DUALMAC_RSDB(wlc->cmn)) {

		/* Check if need to switch to MIMO mode.
		 * This is done if we are connecting in
		 * primary WLC and secondary WLC has
		 * no active connection.
		 */
		if ((to_wlc == to_wlc->cmn->wlc[0]) &&
			(!other_wlc->stas_associated)) {
			/* Check if we need to MOVE the cfg across wlc */
			if ((to_wlc != from_wlc) &&
				(WLC_RSDB_DUAL_MAC_MODE(WLC_RSDB_CURR_MODE(wlc)))) {
				cfg =  wlc_rsdb_cfg_for_chanspec(from_wlc, cfg, bi);
				/* Mute any AP present in to_wlc */
				if (to_wlc->aps_associated != 0) {
					wlc_ap_mute(to_wlc, TRUE, cfg, -1);
				}
				wlc = to_wlc;
				ASSERT(wlc == cfg->wlc);
				wlc_assoc_set_as_cfg(wlc, cfg);
				wlc_rsdb_join_prep_wlc(wlc, cfg, cfg->SSID,
					cfg->SSID_len, cfg->scan_params,
					cfg->assoc_params,
					cfg->assoc_params_len);
				/* XXX sync h/w and s/w for old wlc
				 * to avoid wake_ok assert.
				 */
				wlc_set_wake_ctrl(from_wlc);
			}
#ifdef WL_MODESW
			/* Switch to MIMO */
			if (WLC_MODESW_ENAB(wlc->pub)) {
				if (wlc->cmn->rsdb_mode & WLC_RSDB_MODE_AUTO_MASK) {
					if (WLC_RSDB_DUAL_MAC_MODE(WLC_RSDB_CURR_MODE(wlc))) {
#ifdef WL_MU_RX
						/* All upgrade to MIMO mode is blocked when:
						 * - Active MURX connection or
						 * - Peer is MU-TX capable and MURX is enabled by
						 * user (Static Capability)
						 *
						 * We need update oper_mode of Mode Switch module to
						 * ensure that we send appropriate operating mode
						 * IE in assoc-request
						 */
						if (MU_RX_ENAB(wlc) &&
							(wlc_murx_anymurx_active(wlc->murx) ||
							(WLC_VHT_GET_MU_BFE_CAP(wlc) &&
							WLC_MURX_BI_MU_BFR_CAP(wlc, bi) &&
#ifdef WLP2P
							!BSS_P2P_ENAB(wlc, cfg) &&
#endif /* WLP2P */
							BSSCFG_INFRA_STA(cfg)))) {
							wlc_murx_sync_oper_mode(wlc->murx, cfg, bi);
							*pcfg = cfg;
							return BCME_OK;
						}
#endif /* WL_MU_RX */
						WL_MODE_SWITCH(("wl%d.%d: Wait for the mode"
							"switch %p\n",
							WLCWLUNIT(cfg->wlc), cfg->_idx,
							OSL_OBFUSCATE_BUF(cfg)));
						/* Increment the BI index back to original so
						 * that it can be used while re-entering wlc_join
						 * attempt function after completing mode switch.
							 */
						wl_add_timer(wlc->wl, as->timer,
							MSW_MODE_SWITCH_TIMEOUT, FALSE);
						wlc_assoc_change_state(cfg,
							AS_MODE_SWITCH_START);
						if (WLC_RSDB_SINGLE_MAC_MODE(
							WLC_RSDB_CURR_MODE(wlc)) &&
							!MODE_SWITCH_IN_PROGRESS(wlc->modesw)) {
							wlc_assoc_change_state(cfg, AS_SCAN);
						}
						else {
							return BCME_NOTREADY;
						}
					}
					else if (WLC_RSDB_DUAL_MAC_MODE(WLC_RSDB_CURR_MODE(wlc)) &&
							(wlc_bss_get_80p80_cap(bi))) {
						WL_MODE_SWITCH(("Switch mode to 80p80"
							"initiated\n"));
						wlc->cmn->rsdb_mode_target = WLC_RSDB_MODE_80P80;
						wl_add_timer(wlc->wl, as->timer,
							MSW_MODE_SWITCH_TIMEOUT, FALSE);
						wlc_assoc_change_state(cfg,
							AS_MODE_SWITCH_START);
						if (WLC_RSDB_SINGLE_MAC_MODE(
							WLC_RSDB_CURR_MODE(wlc))) {
							WL_MODE_SWITCH(("Staring scan request\n"));
							wlc_assoc_change_state(cfg, AS_SCAN);
						}
						else {
							return BCME_NOTREADY;
						}
					}
#ifdef WL_MU_RX
					/* If the chip is currently in MIMO mode and peer-BSS is
					 * MU-Beamformer capable, then down-grade is initiated even
					 * for same band operation. This is done to ensure we are
					 * always in RSDB Mode for all MU associations.
					 *
					 * Note: Both Dynamic and Static MU-BFE Capability should
					 * be enabled for this to done
					 */
					else if (MU_RX_ENAB(wlc) &&
						WLC_VHT_GET_MU_BFE_CAP(wlc) &&
						WLC_RSDB_SINGLE_MAC_MODE(WLC_RSDB_CURR_MODE(wlc)) &&
						wlc_bss_get_mimo_cap(bi) &&
						WLC_MURX_BI_MU_BFR_CAP(wlc, bi) &&
#ifdef WLP2P
						!BSS_P2P_ENAB(wlc, cfg) &&
#endif /* WLP2P */
						BSSCFG_INFRA_STA(cfg)) {

						WL_MODE_SWITCH(("wl%d.%d: Wait for the mode"
							"switch %p\n",
							WLCWLUNIT(cfg->wlc), cfg->_idx,
							cfg));
						/* Increment the BI index back to original so
						 * that it can be used while re-entering wlc_join
						 * attempt function after completing mode switch.
						 */
						wl_add_timer(wlc->wl, as->timer,
							MSW_MODE_SWITCH_TIMEOUT, FALSE);

						wlc_assoc_change_state(cfg,
							AS_MODE_SWITCH_START);
						if (WLC_RSDB_DUAL_MAC_MODE(
							WLC_RSDB_CURR_MODE(wlc))) {
							wlc_assoc_change_state(cfg, AS_SCAN);
						}
						else {
							return BCME_NOTREADY;
						}

					}
#endif /* WL_MU_RX */
				}
			}
#endif /* WL_MODESW */
		}
		else {
			WL_MODE_SWITCH(("RSDB:wlc mismatch,making join attempt to"
				"wl%d.%d from wl%d.%d\n",
				to_wlc->pub->unit, cfg->_idx, wlc->pub->unit, cfg->_idx));
			/* Since join is going to happen in a different wlc, */
			/* unmute the muted AP in from_wlc */
			wlc_ap_mute(from_wlc, FALSE, cfg, -1);

			/* Make sure that all the active BSSCFG's in the from_wlc
			 * are downgraded to 1x1 80Mhz operation in case they were
			 * operating at 2x2 or 80p80 mode.
			 */
#ifdef WL_MODESW
			 /* Switch to RSDB */
			if (WLC_MODESW_ENAB(wlc->pub)) {
				if ((wlc->cmn->rsdb_mode & WLC_RSDB_MODE_AUTO_MASK) &&
					(!WLC_RSDB_DUAL_MAC_MODE(WLC_RSDB_CURR_MODE(wlc))) &&
					(!MCHAN_ACTIVE(wlc->pub)) &&
#ifdef WL_ASDB
					(ASDB_ENAB(wlc->pub)) &&
					((wlc_asdb_active(wlc->asdb)) ?
						wlc_asdb_modesw_required(wlc->asdb) : TRUE) &&
#endif /* WL_ASDB */
					(TRUE)) {
					WL_MODE_SWITCH(("wl%d.%d: Wait for the mode"
						"switch %p\n",
						WLCWLUNIT(cfg->wlc), cfg->_idx,
						OSL_OBFUSCATE_BUF(cfg)));
					/* Increment the BI index back to original so
					 * that it can be used while re-entering wlc_join
					 * attempt function after completing mode switch.
					 */
					wl_add_timer(wlc->wl, as->timer,
						MSW_MODE_SWITCH_TIMEOUT, FALSE);
					wlc_assoc_change_state(cfg,
						AS_MODE_SWITCH_START);
					/* Changing assoc state to AS_MODE_SWITCH_START will try
					 * downgrade. If downgrade is complete, MAC_MODE will be in
					 * DUAL mode and assoc will move to AS_SCAN state. If not
					 * complete, for IBSS, set up pending and return NOT_READY
					 */
					if (WLC_RSDB_DUAL_MAC_MODE(WLC_RSDB_CURR_MODE(wlc))) {
						wlc_assoc_change_state(cfg, AS_SCAN);
					}
					else {
						if (BSSCFG_IBSS(cfg) && !BSSCFG_NAN_MGMT(cfg)) {
							cfg->ibss_up_pending = TRUE;
						}
						return BCME_NOTREADY;
					}
				}
			}
#endif /* WL_MODESW */
			if ((to_wlc != from_wlc) &&
				(WLC_RSDB_DUAL_MAC_MODE(WLC_RSDB_CURR_MODE(wlc)))) {
				cfg =  wlc_rsdb_cfg_for_chanspec(from_wlc, cfg, bi);
				wlc = to_wlc;
				ASSERT(wlc == cfg->wlc);
				wlc_assoc_set_as_cfg(wlc, cfg);
				wlc_rsdb_join_prep_wlc(wlc, cfg, cfg->SSID,
					cfg->SSID_len, cfg->scan_params,
					cfg->assoc_params,
					cfg->assoc_params_len);
				/* XXX sync h/w and s/w for old wlc
				 * to avoid wake_ok assert.
				 */
				wlc_set_wake_ctrl(from_wlc);
			}
		}
		wlc_rsdb_init_max_rateset(wlc, bi);
	} else {
			/* Cases to handle in non-rsdb dual mac case
			 *
			 * Case1: If there is no other connection - Nothing to do
			 * Case2: There is an active connection in 1x1 mode
			 * - If original connection is 1x1 or new connection is
			 * 3x3, then Nothing to be done
			 * - If original connection in 3x3 and new connection is
			 * 1x1, Do a clone and modeswitch and then join.
			 * Case3: There is an active connection in 3x3 mode
			 * - If the new connection required in 3x3 move the
			 * initial connection to 1x1
			 */
			if (to_wlc != from_wlc) {
				WL_MODE_SWITCH(("RSDB:wlc mismatch,making join attempt to"
					"wl%d.%d from wl%d.%d\n",
					to_wlc->pub->unit, cfg->_idx, wlc->pub->unit, cfg->_idx));
				/* Since join is going to happen in a different wlc, */
				/* unmute the muted AP in from_wlc */
				wlc_ap_mute(from_wlc, FALSE, cfg, -1);
				ASSERT(wlc->rsdbinfo->cmn->timer_ctx->notif_type
					== RSDB_TMR_TYPE_NONE);
				wlc->rsdbinfo->cmn->timer_ctx->wlc = wlc;
				wlc->rsdbinfo->cmn->timer_ctx->cfg = cfg;
				wlc->rsdbinfo->cmn->timer_ctx->notif_type
					= RSDB_TMR_TYPE_STA_ASSOC;
				WLRSDB_DBG(("wl%d.%d: Schedule the timer for completing"
						" STA assoc\n", WLCWLUNIT(wlc),
						WLC_BSSCFG_IDX(cfg)));
				wlc_rsdb_schedule_timer(wlc->cmn->wlc[1]->rsdbinfo,
					wlc->rsdbinfo->cmn->rsdb_clone_timer, 0, FALSE);
				return BCME_NOTREADY;

			} else {
				if (!other_wlc->bandlocked) {
					int other_bandunit = (CHSPEC_BANDUNIT(bi->chanspec) ==
						BAND_5G_INDEX) ? BAND_2G_INDEX : BAND_5G_INDEX;

					if (wlc_rsdb_change_band_allowed(other_wlc,
						other_bandunit)) {
						wlc_change_band(other_wlc, other_bandunit);
					}
				}
			}
			/* MPC delay off is forced to zero to avoid
			 * other chain go down due to delayed mpc.
			 * Also wl_down for non-mpc is avoided. This
			 * will be required in case of current
			 * measurement for non-mpc case, that should be
			 * achievable using core_mpc wl command.
			 */
			if (other_wlc) {
				other_wlc->mpc_delay_off = 0;
				if (other_wlc->mpc) {
					wlc_radio_mpc_upd(other_wlc);
				}
			}
		}
	}

	*pcfg = cfg;
	return BCME_OK;
}

#ifdef WL_MODESW

bool
wlc_rsdb_upgrade_allowed(wlc_info_t *wlc)
{
	bool ret = FALSE;

	if (!(wlc->cmn->rsdb_mode & WLC_RSDB_MODE_AUTO_MASK)) {
		return ret;
	}

#if defined(WL_MODESW) && !defined(WL_MODESW_DISABLED)
	if (WLC_MODESW_ENAB(wlc->pub) && MODE_SWITCH_IN_PROGRESS(wlc->modesw)) {
		return ret;
	}
#endif /* WL_MODESW && !WL_MODESW_DISABLED */

	if (AS_IN_PROGRESS(wlc) || ANY_SCAN_IN_PROGRESS(wlc->scan) ||
#ifdef WL_MU_RX
		(MU_RX_ENAB(wlc) && wlc_murx_anymurx_active(wlc->murx)) ||
#endif /* WL_MU_RX */
		FALSE) {
		return ret;
	}

	if (
#ifdef WLRM
	(WLRM_ENAB(wlc->pub) && wlc_rminprog(wlc)) ||
#endif /* WLRM */
#ifdef WL11K
	(wlc_rrm_inprog(wlc)) ||
#endif /* WL11K  */
	FALSE) {
		return ret;
	}

#ifdef WLMCHAN
	if (MCHAN_ENAB(wlc->pub) && MCHAN_ACTIVE(wlc->pub)) {
		if (!wlc_modesw_pending_check(wlc->modesw) ||
			wlc_mchan_get_modesw_pending(wlc->mchan) ||
			!wlc_mchan_check_tbtt_valid(wlc->mchan)) {
			return ret;
		}
	}
#endif /* WLMCHAN */

#ifdef WL_MODESW
	if (WLC_MODESW_ENAB(wlc->pub) &&
		(wlc_modesw_get_switch_type(wlc->modesw) == MODESW_VSDB_TO_RSDB)) {
		WL_MODE_SWITCH(("wl%d: Upgrading during V -> R switch..Dont Allow..\n",
			WLCWLUNIT(wlc)));
		return ret;
	}
#endif // endif
	return TRUE;
}

int
wlc_rsdb_check_upgrade(wlc_info_t *wlc)
{
	wlc_info_t *wlc0, *wlc1;
	int ret = BCME_OK, wlc0_as_count, wlc1_as_count, idx;
	wlc_bsscfg_t *cfg;

	/* DUALMAC not supposed to hit here */
	if (WLC_DUALMAC_RSDB(wlc->cmn))
		return ret;

	/* Don't allow upgrade if band locked */
	if (wlc->bandlocked) {
	       return ret;
	}

	wlc0 =  wlc->cmn->wlc[0];
	wlc1 =  wlc->cmn->wlc[1];
	ASSERT(wlc == wlc0);

	if (wlc1 == NULL) {
		WL_ERROR(("wl%d: wlc1 is NULL\n", WLCWLUNIT(wlc)));
		return ret;
	}
	wlc0_as_count = wlc0->aps_associated + wlc0->stas_associated;
	wlc1_as_count = wlc1->aps_associated + wlc1->stas_associated;

	BCM_REFERENCE(wlc0_as_count);

	if (wlc_rsdb_upgrade_allowed(wlc) == FALSE) {
		return ret;
	}
#ifdef WL_MODESW
	if (WLC_MODESW_ENAB(wlc->pub) &&
		!MCHAN_ACTIVE(wlc->pub) && WLC_MODESW_ANY_SWITCH_IN_PROGRESS(wlc->modesw)) {
		WL_ERROR(("wl%d: Old ASDB switch is pending .. Cleanup...\n",
			WLCWLUNIT(wlc)));
		wlc_modesw_set_switch_type(wlc0->modesw, MODESW_NO_SW_IN_PROGRESS);
		return ret;
	}
#endif /* WL_MODESW */
	/* Move back all un-associated cfg's from wlc1
	* to wlc0 excluding bsscfg[1], which is inactive
	* default primary cfg in wlc1.
	*/
	FOREACH_BSS(wlc1, idx, cfg) {
		/* Avoid moving default inactive primary cfg in core 1 */
		if ((cfg->_idx != RSDB_INACTIVE_CFG_IDX) &&
			!cfg->associated &&
			!BSSCFG_IS_TDLS(cfg) &&
			(cfg->assoc->state == AS_IDLE)) {
			WL_MODE_SWITCH(("wl%d: Moving Un-associated CFG %d\n", WLCWLUNIT(wlc),
				cfg->_idx));
			cfg = wlc_rsdb_bsscfg_clone(wlc1, wlc_rsdb_get_other_wlc(wlc1),
				cfg, &ret);
		}
	}

	if (wlc0->pub->associated && !wlc1->pub->associated &&
		WLC_RSDB_DUAL_MAC_MODE(WLC_RSDB_CURR_MODE(wlc))) {
		/* Upgrade the wlc0 */
		wlc_rsdb_upgrade_wlc(wlc0);
	}
	else if (wlc1->pub->associated && wlc1_as_count &&
		(!wlc0->pub->associated)) {
		wlc_modesw_move_cfgs_for_mimo(wlc->modesw);
	}
#ifdef WLP2P
	if (P2P_ENAB(wlc0->pub)) {
		(void)wlc_p2p_rsdb_disc_config(wlc0->p2p);
	}
#endif /* WLP2P */

	return ret;
}
/* Monitor for upgrade/downgrade possibilities */
/* single connection and peer mimo capable, upgrade to core -0 */
/* single connection and peer siso, downgrade to core-1 */
static int
wlc_dualmac_rsdb_monitor(wlc_info_t *wlc)
{
	wlc_info_t *wlc0, *wlc1;
	int ret = BCME_OK, idx;
	wlc_bsscfg_t *cfg;
	wlc0 =  wlc->cmn->wlc[0];
	wlc1 =  wlc->cmn->wlc[1];

	if (wlc_rsdb_upgrade_allowed(wlc) == FALSE) {
		return ret;
	}

	/* Check the conditions for bsscfg move */
	/* if wlc[0] having STA asssociated & no
	 * association in wlc[1] & if connection
	 * in wlc[0] is SISO capable, move it to wlc[0]
	 */
	if ((wlc0->pub->associated && !wlc1->pub->associated) &&
		(wlc0->aps_associated == 0 && wlc1->aps_associated == 0)) {
		/* check if cfg in wlc0 needs move to the wlc1 */
		bool cfg_moveable = TRUE;
		FOREACH_AS_BSS(wlc0, idx, cfg) {
			if (wlc_rsdb_peerap_ismimo(wlc0, cfg)) {
				cfg_moveable = FALSE;
			}
		}
		if (cfg_moveable) {
			FOREACH_AS_BSS(wlc0, idx, cfg) {
				if (WLC_BSS_ASSOC_NOT_ROAM(cfg) && cfg_moveable) {
					/* move siso in wlc 0 to wlc1 */
					WLRSDB_DBG(("wlc0 connection is SISO capable,"
						"Move it to wlc1 \n"));
					ret = wlc_rsdb_trigger_modeswitch(cfg, RSDB_MOVE_CHAIN1);
				}
			}
		}
	}
	/* if wlc[1] having STA asssociated & no
	 * association in wlc[0]  & if connection in
	 * wlc[1] is MIMO capable move, it to wlc[0]
	 * & upgrade
	 */
	else if ((wlc1->pub->associated && !wlc0->pub->associated) &&
		(wlc0->aps_associated == 0 && wlc1->aps_associated == 0)) {

		/* check if cfg in wlc1 needs move to the wlc0 */
		bool cfg_moveable = FALSE;
		FOREACH_AS_BSS(wlc1, idx, cfg) {
			if (wlc_rsdb_peerap_ismimo(wlc1, cfg)) {
				cfg_moveable = TRUE;
			}
		}
		if (cfg_moveable) {
			FOREACH_AS_BSS(wlc1, idx, cfg) {
				if (WLC_BSS_CONNECTED(cfg) && cfg_moveable) {
					/* move mimo in wlc 1 to wlc0 */
					WLRSDB_DBG(("wlc1 connection is MIMO capable,"
						"Move it to wlc0 \n"));
					ret = wlc_rsdb_trigger_modeswitch(cfg, RSDB_MOVE_CHAIN1);
				}
			}
		}
	}
	return ret;
}

uint8
wlc_rsdb_upgrade_wlc(wlc_info_t *wlc)
{
	int idx = 0;
	wlc_bsscfg_t *bsscfg;
	bool mode_switch_sched = FALSE, is160_8080 = FALSE;
	uint8 new_oper_mode, curr_oper_mode, bw, nss;
	uint8 chip_mode = WLC_RSDB_CURR_MODE(wlc);

	if (!WLC_DUALMAC_RSDB(wlc->cmn)) {
	if ((!(wlc->cmn->rsdb_mode & WLC_RSDB_MODE_AUTO_MASK) &&
#ifdef WLRSDB_POLICY_MGR
	!WLRSDB_POLICY_MGR_ENAB(wlc->pub) &&
#endif // endif
	TRUE) || (chip_mode == WLC_RSDB_MODE_2X2)) {

		WL_MODE_SWITCH(("wl%d: Not possible to upgrade... mode = %x\n",
			WLCWLUNIT(wlc), chip_mode));
		return BCME_OK;
	}

#ifdef WLTDLS
	if (TDLS_ENAB(wlc->pub)) {
		wlc_cmn_info_t *wlc_cmn = wlc->cmn;
		wlc_info_t *wlc_iter;
		FOREACH_WLC(wlc_cmn, idx, wlc_iter) {
			wlc_tdls_set(wlc_iter->tdls, FALSE);
		}
	}
#endif // endif

	FOREACH_BSS(wlc, idx, bsscfg) {
		if (WLC_BSS_ASSOC_NOT_ROAM(bsscfg)) {
			/* If oper mode is enabled, bw and nss is calculated from it */
			/* otherwise oper mode is derived from chanspec */
			if (BSSCFG_STA(bsscfg) && (bsscfg->assoc->state != AS_IDLE)) {
				WL_MODE_SWITCH(("wl%d: cfg %d is in %d assoc state..."
					" try upgrade later..\n", WLCWLUNIT(wlc),
					WLC_BSSCFG_IDX(bsscfg), bsscfg->assoc->state));
				goto exit;
			}
			if (bsscfg->oper_mode_enabled) {
				bw = DOT11_OPER_MODE_CHANNEL_WIDTH(bsscfg->oper_mode);
				nss = DOT11_OPER_MODE_RXNSS(bsscfg->oper_mode);
				is160_8080 = DOT11_OPER_MODE_160_8080(
						bsscfg->oper_mode);
			}
			else {
				curr_oper_mode = wlc_modesw_derive_opermode(wlc->modesw,
					bsscfg->current_bss->chanspec, bsscfg,
					wlc->stf->op_rxstreams);
				bw = DOT11_OPER_MODE_CHANNEL_WIDTH(curr_oper_mode);
				nss = DOT11_OPER_MODE_RXNSS(curr_oper_mode);
				is160_8080 = DOT11_OPER_MODE_160_8080(curr_oper_mode);
			}
			if (nss < wlc_rsdb_get_max_chain_cap(wlc))	{
				/* Use the Rx max streams for both Rx and Tx */
				new_oper_mode = DOT11_D8_OPER_MODE(0,
				wlc_rsdb_get_max_chain_cap(wlc), 0, is160_8080, bw);
			}
			else {
				/* Already at max Bw and Nss */
				continue;
			}
			wlc_modesw_handle_oper_mode_notif_request(wlc->modesw, bsscfg,
				new_oper_mode, TRUE, 0);
			mode_switch_sched = TRUE;
		}
		else {
			/* If mode switch to 80p80 then set the operating mode to 80p80 */
			if (wlc->cmn->rsdb_mode_target == WLC_RSDB_MODE_80P80) {
				new_oper_mode = DOT11_D8_OPER_MODE(0,
				WLC_BITSCNT(wlc->stf->hw_rxchain), 0, TRUE, DOT11_OPER_MODE_80MHZ);
				wlc_modesw_handle_oper_mode_notif_request(wlc->modesw, bsscfg,
					new_oper_mode, TRUE, 0);
			}
			/* Use the Rx max streams for both Rx and Tx */
			else {
			new_oper_mode = DOT11_D8_OPER_MODE(0,
				wlc_rsdb_get_max_chain_cap(wlc), 0, FALSE, DOT11_OPER_MODE_80MHZ);
			wlc_modesw_handle_oper_mode_notif_request(wlc->modesw, bsscfg,
				new_oper_mode, TRUE, 0);
			}
		}
	}
	if (!mode_switch_sched) {
		WL_MODE_SWITCH(("wl%d: No Operating mode triggered\n",
			WLCWLUNIT(wlc)));
		if (wlc->cmn->rsdb_mode_target == WLC_RSDB_MODE_80P80) {
			WL_MODE_SWITCH(("Moving mode to 80p80 \n"));
			wlc_rsdb_change_mode(wlc, (WLC_RSDB_MODE_80P80 | (WLC_RSDB_IS_AUTO_MODE(wlc)
					? WLC_RSDB_MODE_AUTO_MASK : 0)));
			wlc->cmn->rsdb_mode_target = WLC_RSDB_MODE_MAX;
		}
		else {
			wlc_rsdb_change_mode(wlc, (WLC_RSDB_MODE_2X2 | (WLC_RSDB_IS_AUTO_MODE(wlc)
					? WLC_RSDB_MODE_AUTO_MASK : 0)));
		}
	}
	} else {
		/* Do the bsscfg clone and opmode change */
		if (!(wlc->cmn->rsdb_mode & WLC_RSDB_MODE_AUTO_MASK)) {
			WL_MODE_SWITCH(("wl%d: Not possible to upgrade... mode = %x\n",
				WLCWLUNIT(wlc), chip_mode));
			return BCME_OK;
		}
		FOREACH_BSS(wlc, idx, bsscfg) {
			if (WLC_BSS_ASSOC_NOT_ROAM(bsscfg)) {
				/* If oper mode is enabled, bw and nss is calculated from it */
				/* otherwise oper mode is derived from chanspec */
				if (BSSCFG_STA(bsscfg) && (bsscfg->assoc->state != AS_IDLE)) {
					WL_MODE_SWITCH(("wl%d: cfg %d is in %d assoc state..."
						" try upgrade later..\n", WLCWLUNIT(wlc),
						WLC_BSSCFG_IDX(bsscfg), bsscfg->assoc->state));
					goto exit;
				}
			}
		}
		/* Do the bsscfg clone and opmode change */
		WLRSDB_DBG(("RSDB upgrade \n"));
		FOREACH_BSS(wlc, idx, bsscfg) {
			if (WLC_BSS_ASSOC_NOT_ROAM(bsscfg)) {
				/* If oper mode is enabled, bw and nss is calculated from it */
				/* otherwise oper mode is derived from chanspec */
				if (BSSCFG_STA(bsscfg) && (bsscfg->assoc->state != AS_IDLE)) {
					WL_MODE_SWITCH(("wl%d: cfg %d is in %d assoc state..."
						" try upgrade later..\n", WLCWLUNIT(wlc),
						WLC_BSSCFG_IDX(bsscfg), bsscfg->assoc->state));
					goto exit;
				}
				if (bsscfg->oper_mode_enabled) {
					bw = DOT11_OPER_MODE_CHANNEL_WIDTH(bsscfg->oper_mode);
					nss = DOT11_OPER_MODE_RXNSS(bsscfg->oper_mode);
					is160_8080 = DOT11_OPER_MODE_160_8080(
							bsscfg->oper_mode);
				} else {
					curr_oper_mode = wlc_modesw_derive_opermode(wlc->modesw,
						bsscfg->current_bss->chanspec, bsscfg,
						wlc->stf->op_rxstreams);
					bw = DOT11_OPER_MODE_CHANNEL_WIDTH(curr_oper_mode);
					nss = DOT11_OPER_MODE_RXNSS(curr_oper_mode);
					is160_8080 = DOT11_OPER_MODE_160_8080(
							curr_oper_mode);
				}
				if ((nss == 1) && (wlc_rsdb_get_max_chain_cap(wlc) > 1))	{
					/* Use the Rx max streams for both Rx and Tx */
					new_oper_mode = DOT11_D8_OPER_MODE(0,
						wlc_rsdb_get_max_chain_cap(wlc), 0, is160_8080,
						bw);
				} else {
					new_oper_mode = DOT11_D8_OPER_MODE(0, 3, 0, is160_8080,
						DOT11_OPER_MODE_80MHZ);
				}
				wlc_modesw_handle_oper_mode_notif_request(wlc->modesw, bsscfg,
					new_oper_mode, TRUE, MODESW_CTRL_RSDB_MOVE);
			} else {
				/* Use the Rx max streams for both Rx and Tx */
				new_oper_mode = DOT11_D8_OPER_MODE(0,
					wlc_rsdb_get_max_chain_cap(wlc), 0, is160_8080,
					DOT11_OPER_MODE_80MHZ);
				wlc_modesw_handle_oper_mode_notif_request(wlc->modesw, bsscfg,
					new_oper_mode, TRUE, MODESW_CTRL_RSDB_MOVE);
			}
		}
	}
	return BCME_OK;
exit:
	return BCME_NOTREADY;
}

uint8
wlc_rsdb_downgrade_wlc(wlc_info_t *wlc)
{
	int idx = 0;
	wlc_bsscfg_t *bsscfg;
	uint8 new_oper_mode, curr_oper_mode, bw, nss;
	uint8 chip_mode = WLC_RSDB_CURR_MODE(wlc);
	bool mode_switch_sched = FALSE, is160_8080 = FALSE;

	if (!WLC_DUALMAC_RSDB(wlc->cmn)) {
	if ((wlc->cmn->rsdb_mode & WLC_RSDB_MODE_AUTO_MASK) &&
		(chip_mode == WLC_RSDB_MODE_RSDB)) {
		WL_MODE_SWITCH(("wl%d: Not possible to Downgrade... mode = %x\n",
			WLCWLUNIT(wlc), chip_mode));
		return BCME_OK;
	}

#ifdef WLTDLS
	if (TDLS_ENAB(wlc->pub)) {
		wlc_cmn_info_t *wlc_cmn = wlc->cmn;
		wlc_info_t *wlc_iter;
		FOREACH_WLC(wlc_cmn, idx, wlc_iter) {
			wlc_tdls_set(wlc_iter->tdls, FALSE);
		}
	}
#endif // endif

	FOREACH_BSS(wlc, idx, bsscfg)
	{
		if (WLC_BSS_ASSOC_NOT_ROAM(bsscfg) &&
			(!BSSCFG_IBSS(bsscfg) || BSSCFG_NAN_MGMT(bsscfg))) {
			curr_oper_mode = wlc_modesw_derive_opermode(wlc->modesw,
				bsscfg->current_bss->chanspec, bsscfg, wlc->stf->op_rxstreams);
			bw = DOT11_OPER_MODE_CHANNEL_WIDTH(curr_oper_mode);
			nss = DOT11_OPER_MODE_RXNSS(curr_oper_mode);
			is160_8080 = DOT11_OPER_MODE_160_8080(curr_oper_mode);
			if (nss != 1)	{
				new_oper_mode = DOT11_D8_OPER_MODE(0, 1, 0, is160_8080, bw);
			}
			else {
				new_oper_mode = DOT11_D8_OPER_MODE(0, 1, 0, is160_8080,
						DOT11_OPER_MODE_80MHZ);
			}
			wlc_modesw_handle_oper_mode_notif_request(wlc->modesw, bsscfg,
				new_oper_mode, TRUE, FALSE);
			mode_switch_sched = TRUE;
		}
		else {
			new_oper_mode = DOT11_OPER_MODE(0, 1, DOT11_OPER_MODE_80MHZ);
			wlc_modesw_handle_oper_mode_notif_request(wlc->modesw, bsscfg,
				new_oper_mode, TRUE, FALSE);
		}
	}
	if (!mode_switch_sched) {
		wlc_rsdb_change_mode(wlc, (WLC_RSDB_MODE_RSDB | (WLC_RSDB_IS_AUTO_MODE(wlc) ?
				WLC_RSDB_MODE_AUTO_MASK : 0)));
	}
	} else {
		/*
		 * Downgrade from 3x3 to 1x1.
		 * first do modeswitch and then do bsscfg clone
		 */
		uint32 ctrl_flags;
		if (!(wlc->cmn->rsdb_mode & WLC_RSDB_MODE_AUTO_MASK)) {
			WL_MODE_SWITCH(("wl%d: Not possible to Downgrade... mode = %x\n",
				WLCWLUNIT(wlc), chip_mode));
			return BCME_OK;
		}
		FOREACH_BSS(wlc, idx, bsscfg)
		{
			ctrl_flags = BSSCFG_NAN_MGMT(bsscfg) ? MODESW_CTRL_DN_SILENT_DNGRADE : 0;
			if (WLC_BSS_CONNECTED(bsscfg) &&
				(!BSSCFG_IBSS(bsscfg) || BSSCFG_NAN_MGMT(bsscfg))) {
				curr_oper_mode = wlc_modesw_derive_opermode(wlc->modesw,
					bsscfg->current_bss->chanspec, bsscfg,
					wlc->stf->op_rxstreams);
				bw = DOT11_OPER_MODE_CHANNEL_WIDTH(curr_oper_mode);
				nss = DOT11_OPER_MODE_RXNSS(curr_oper_mode);
				is160_8080 = DOT11_OPER_MODE_160_8080(curr_oper_mode);
				if (nss != 1)	{
					new_oper_mode = DOT11_D8_OPER_MODE(0, 1, 0, is160_8080, bw);
				}
				else {
					new_oper_mode = DOT11_D8_OPER_MODE(0, 1, 0, is160_8080,
						DOT11_OPER_MODE_80MHZ);
				}
				wlc_modesw_handle_oper_mode_notif_request(wlc->modesw, bsscfg,
					new_oper_mode, TRUE, MODESW_CTRL_RSDB_MOVE);
			}
			else {
				new_oper_mode = DOT11_D8_OPER_MODE(0, 1, 0, is160_8080,
						DOT11_OPER_MODE_80MHZ);
				wlc_modesw_handle_oper_mode_notif_request(wlc->modesw, bsscfg,
					new_oper_mode, TRUE, ctrl_flags);
			}
		}
	}
	return BCME_OK;
}

static void
wlc_rsdb_clone_timer_cb(void *arg)
{
	rsdb_timer_cb_t *ctx = (rsdb_timer_cb_t*) arg;
	wlc_info_t *wlc = ctx->wlc;
	wlc_bsscfg_t *to_cfg = NULL;
	int ret;
	uint8 notif_type = ctx->notif_type;
	wlc_bsscfg_t* cfg = ctx->cfg;
	UNUSED_PARAMETER(cfg);
#ifdef WL_MODESW
	if (wlc_modesw_get_switch_type(wlc->cmn->wlc[0]->modesw) ==
			MODESW_RSDB_TO_VSDB) {
#endif // endif
#ifdef WLMCHAN
		/* clone all bsscfg's sharing the chan context of this cfg */
		wlc_mchan_clone_context_all(wlc->cmn->wlc[1], wlc->cmn->wlc[0], cfg);
#endif // endif
#ifdef WL_MODESW
	} else if (WLC_RSDB_IS_AUTO_MODE(wlc)) {
		wlc_modesw_move_cfgs_for_mimo(wlc->modesw);
	} else
#endif // endif
	if (WLC_DUALMAC_RSDB(wlc->cmn)) {
		if (notif_type == RSDB_TMR_TYPE_AP_CLONE) {
			WLRSDB_DBG(("wl%d.%d: Clone the AP CFG\n", WLCWLUNIT(wlc),
				WLC_BSSCFG_IDX(cfg)));
			to_cfg = wlc_rsdb_bsscfg_clone(wlc, wlc_rsdb_get_other_wlc(wlc), cfg, &ret);
			if (to_cfg != NULL) {
				WLRSDB_DBG(("wl%d.%d: Enable the AP cfg\n", WLCWLUNIT(to_cfg->wlc),
					WLC_BSSCFG_IDX(cfg)));
				(void)wlc_bsscfg_enable(to_cfg->wlc, to_cfg);
				ASSERT(to_cfg->enable);
			}
		} else if (notif_type == RSDB_TMR_TYPE_STA_ASSOC) {
#ifdef STA
				if (cfg->assoc->state == AS_WAIT_FOR_AP_CSA) {
					WLRSDB_DBG(("wl%d.%d: Continue STA cfg assoc\n",
						WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg)));
					wlc_join_bss_prep(cfg);
				}
				else if (cfg->assoc->type == AS_ASSOCIATION ||
						cfg->assoc->type == AS_ROAM) {
					wlc_info_t *to_wlc = wlc_rsdb_get_other_wlc(wlc);
					wlc_rsdb_bsscfg_clone(wlc, to_wlc, cfg, NULL);
				}
				else {
					WLRSDB_DBG(("wl%d.%d: Continue STA cfg roam\n",
						WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg)));
					wlc_join_done(cfg, &cfg->BSSID, cfg->target_bss->bss_type,
					              WLC_E_STATUS_FAIL);
				}
#endif /* STA */
		} else if (notif_type == RSDB_TMR_TYPE_STA_CLONE) {
			WLRSDB_DBG(("wl%d.%d: Clone the STA CFG for CSA\n", WLCWLUNIT(wlc),
				WLC_BSSCFG_IDX(cfg)));
			wlc_rsdb_bsscfg_clone(wlc, wlc_rsdb_get_other_wlc(wlc), cfg, &ret);
		} else if (ctx->notif_type == RSDB_TMR_TYPE_IBSS_CLONE) {
			wlc_info_t *to_wlc = wlc_rsdb_get_other_wlc(wlc);
			WLRSDB_DBG(("wl%d.%d: Clone the IBSS STA CFG\n", WLCWLUNIT(wlc),
				WLC_BSSCFG_IDX(cfg)));

			if (!wlc_rsdb_bsscfg_clone(wlc, to_wlc, cfg, NULL)) {
				WL_ERROR(("wl%d.%d: Clone Failed\n", WLCWLUNIT(wlc),
					WLC_BSSCFG_IDX(cfg)));
				ASSERT(0);
			}
		}
		/* Reset the notif type to NONE */
		if (notif_type == wlc->rsdbinfo->cmn->timer_ctx->notif_type) {
			wlc->rsdbinfo->cmn->timer_ctx->notif_type = RSDB_TMR_TYPE_NONE;
		}
	}
}
#endif /* WL_MODESW */

int
wlc_rsdb_change_mode(wlc_info_t *wlc, int8 to_mode)
{
	int err = BCME_OK;
	int idx;
	wlc_info_t *wlc_iter;
	wlc_cmn_info_t *wlc_cmn = wlc->cmn;
	mbool phy_measure_val;

	if (WLC_DUALMAC_RSDB(wlc->cmn)) {
		/* XXX: SWWLAN-110939: Setting auto caused crash but
		 * XXX: there is only rsdb mode for dualmac
		 */
		return BCME_UNSUPPORTED;
	}

	/* Make sure sw mode and phy mode are in sync */
	ASSERT(PHYMODE2SWMODE(phy_get_phymode((phy_info_t *)WLC_PI(wlc)))
		== WLC_RSDB_CURR_MODE(wlc));

	/* Check if the current mode and the new mode are the same */
	if (to_mode == wlc_cmn->rsdb_mode)
		return err;

	/* Check if the current mode is auto and the new mode is also auto */
	if ((to_mode == WLC_RSDB_MODE_AUTO) && ((wlc_cmn->rsdb_mode & WLC_RSDB_MODE_AUTO_MASK)
		== WLC_RSDB_MODE_AUTO_MASK)) {
		return err;
	}

	/* Check if AUTO is overrided, then
	* dont allow AUTO operation
	*/
	if ((to_mode == WLC_RSDB_MODE_AUTO) &&
		wlc->cmn->rsdb_auto_override) {
		return BCME_NOTENABLED;
	}

	/* Check if the current mode is auto + ZZZ and the new mode is ZZZ */
	if ((wlc_cmn->rsdb_mode & WLC_RSDB_MODE_AUTO_MASK) &&
		(WLC_RSDB_CURR_MODE(wlc) == to_mode)) {
		wlc_cmn->rsdb_mode = to_mode;
		/* Change the max rateset as per new mode. */
		wlc_rsdb_init_max_rateset(wlc, NULL);
		return err;
	}

	/* TODO: Check if mode can be changed */

	if (to_mode == WLC_RSDB_MODE_AUTO) {
		wlc_cmn->rsdb_mode |= WLC_RSDB_MODE_AUTO_MASK;
		/* Change the max rateset as per new mode. */
		wlc_rsdb_init_max_rateset(wlc, NULL);
		return err;
	}

	/* In Automode, currently we are not changing the actual mode. So, no need to update
	   anything. If any actual mode change happens, the following if condition need to be
	   re-looked at
	 */
	if (to_mode != WLC_RSDB_MODE_AUTO) {
		wlc_info_t *wlc1 = wlc_cmn->wlc[MAC_CORE_UNIT_1];
		/* Detach the active queue */
		if (wlc->pub->associated) {
			/* flush fifo pkts: sync active_queue */
			wlc_sync_txfifo_all(wlc, wlc->active_queue, SYNCFIFO);
		}

		/* Do Save-Restore for measure hold status for both WLC's */
		phy_measure_val = phy_get_measure_hold_status((phy_info_t *)wlc->pi);
		if (WLC_RSDB_EXTRACT_MODE(to_mode) == WLC_RSDB_MODE_RSDB) {
			/* SWITCHING TO RSDB */
			/* Confirm that no one has tried to bringup
			* WLC1 in MIMO mode.
			 */
			if (wlc1->pub->up) {
				WL_ERROR(("wl%d:ERROR:wlc1 is UP before entering RSDB mode\n",
					WLCWLUNIT(wlc)));
				WL_ERROR(("wl%d associated %d SCAN_IN_PROGRESS %d"
					"MONITOR_ENAB %d\n mpc_out %d mpc_scan %d mpc_join"
					"%d mpc_oidscan %d mpc_oidjoin %d mpc_oidnettype %d"
					"\n delayed_down %d "
					"txfifo_detach_pending %d mpc_off_req %d\n",
					wlc1->pub->unit, wlc1->pub->associated,
					SCAN_IN_PROGRESS(wlc1->scan), MONITOR_ENAB(wlc1),
					wlc1->mpc_out, wlc1->mpc_scan, wlc1->mpc_join,
					wlc1->mpc_oidscan, wlc1->mpc_oidjoin,
					wlc1->mpc_oidnettype, wlc1->pub->delayed_down,
					wlc1->txfifo_detach_pending,
					wlc1->mpc_off_req));
				ASSERT(FALSE);
				OSL_SYS_HALT();
			}
			wlc_cmn->rsdb_mode = to_mode;
			if (wlc->pub->up) {
				wlc_bmac_suspend_mac_and_wait(wlc->hw);
			}
			if (wlc->pub->up) {
				err = wlc_rsdb_update_hw_mode(wlc,
					SWMODE2PHYMODE(WLC_RSDB_MODE_RSDB));
				wlc_bmac_rsdb_mode_param_to_shmem(wlc->hw);
			}
			/* For RSDB case, apply changes on all wlcs */
			FOREACH_WLC(wlc_cmn, idx, wlc_iter) {
				wlc_stf_phy_chain_calc_set(wlc_iter);
			}
			FOREACH_WLC(wlc_cmn, idx, wlc_iter) {
				wlc_rsdb_mode_change_complete(wlc_iter);
			}
			/* In MPC is disabled, then bring up WLC1 here, if WLC0 is UP */
			if (wlc->pub->up && !wlc1->mpc && !wlc1->pub->up) {
				wl_up(wlc1->wl);
			}
		} else {
		    bool core0_is_up = FALSE;
		    bool core1_is_up = FALSE;
			/* MPC delay off must be forced to zero irrespective of mpc mode */
			wlc1->mpc_delay_off = 0;
			/* SWITCHING TO MIMO */
			if (wlc1->pub->up) {
				/* Flush WLC1 Tx FIFO's before DOWN if packets pending. */
				wlc_sync_txfifo_all(wlc1, wlc1->active_queue, FLUSHFIFO);
				core1_is_up = TRUE;
				/* Bring DOWN WLC[1], while switching to MIMO mode. */
				if (wlc1->mpc) {
					wlc_radio_mpc_upd(wlc1);
					wlc_radio_upd(wlc1);
				} else {
					wl_down(wlc1->wl);
				}
				if (wlc1->pub->up) {
					WL_ERROR(("wl%d:ERROR: wlc1 is UP in MIMO\n",
						WLCWLUNIT(wlc)));
					WL_ERROR(("wl%d associated %d SCAN_IN_PROGRESS %d"
						"MONITOR_ENAB %d\n mpc_out %d mpc_scan %d mpc_join"
						"%d mpc_oidscan %d mpc_oidjoin %d mpc_oidnettype %d"
						"\n delayed_down %d "
						"txfifo_detach_pending %d mpc_off_req %d\n",
						wlc1->pub->unit, wlc1->pub->associated,
						SCAN_IN_PROGRESS(wlc1->scan), MONITOR_ENAB(wlc1),
						wlc1->mpc_out, wlc1->mpc_scan, wlc1->mpc_join,
						wlc1->mpc_oidscan, wlc1->mpc_oidjoin,
						wlc1->mpc_oidnettype, wlc1->pub->delayed_down,
						wlc1->txfifo_detach_pending,
						wlc1->mpc_off_req));
					ASSERT(FALSE);
					OSL_SYS_HALT();
				}
			}
			wlc_cmn->rsdb_mode = to_mode;
#ifdef WLTDLS
			if (TDLS_ENAB(wlc->pub)) {
				FOREACH_WLC(wlc_cmn, idx, wlc_iter) {
					wlc_tdls_set(wlc_iter->tdls, TRUE);
				}
			}
#endif // endif
			if (wlc->pub->up) {
				wlc_bmac_suspend_mac_and_wait(wlc->hw);
			}
			/* For non RSDB cases, apply changes only on wlc0 */
			if (wlc->pub->up) {
				err = wlc_rsdb_update_hw_mode(wlc,
					SWMODE2PHYMODE(WLC_RSDB_EXTRACT_MODE(to_mode)));
				wlc_bmac_rsdb_mode_param_to_shmem(wlc->hw);
				core0_is_up = TRUE;
			}
			wlc_stf_phy_chain_calc_set(wlc_cmn->wlc[0]);
			wlc_rsdb_mode_change_complete(wlc_cmn->wlc[0]);
			/* hwreqoff on core1 should happen if core0 is up */
			if (core1_is_up && core0_is_up) {
				wlc_bmac_4349_core1_hwreqoff(wlc_cmn->wlc[0]->hw, TRUE);
			}
		}
		if (wlc->pub->up) {
			wlc_enable_mac(wlc);
		}
		phy_set_measure_hold_status((phy_info_t*)WLC_PI(wlc), phy_measure_val);
		phy_set_measure_hold_status((phy_info_t*)WLC_PI(wlc1), FALSE);
	}

	if (!ANY_SCAN_IN_PROGRESS(wlc->scan)) {
#ifdef WLAMPDU
		/* Reset ampdu ini state */
		wlc_ampdu_agg_state_update_all(wlc, 0);
#endif // endif
#ifdef WLAMSDU
		/* Update amsdu state depending on mchan/rsdb modes */
		wlc_amsdu_update_state(wlc);
#endif // endif
#ifdef WLAMPDU
		/* reset ampdu ini states */
		wlc_ampdu_agg_state_update_all(wlc, 1);
#endif // endif
#ifdef DONGLEBUILD
		if (BCMPCIEDEV_ENAB() && !MCHAN_ACTIVE(wlc->pub)) {
			wl_busioctl(wlc->wl, BUS_UPDATE_FLOW_PKTS_MAX,
				NULL, 0, NULL, NULL, FALSE);
		}
#endif // endif

		if (WLC_RSDB_SINGLE_MAC_MODE(WLC_RSDB_CURR_MODE(wlc)))
			wlc_bmac_update_rxpost_rxbnd(wlc->hw, NRXBUFPOST, WL_RXBND);
		else
			wlc_bmac_update_rxpost_rxbnd(wlc->hw, NRXBUFPOST_SMALL, WL_RXBND_SMALL);

		if (wlc->pub->up)
			wlc_bmac_update_rxfill(wlc->hw);
	}
	/* Change the max rateset as per new mode. */
	wlc_rsdb_init_max_rateset(wlc, NULL);
#ifdef WLTDLS
	if (TDLS_ENAB(wlc->pub)) {
		bool tdls_set = FALSE;
		if (wlc_tdls_disabled_for_rsdb(wlc->tdls)) {
			if (WLC_RSDB_SINGLE_MAC_MODE(WLC_RSDB_CURR_MODE(wlc))) {
				tdls_set = TRUE;
			} else {
				tdls_set = FALSE;
			}
		} else {
			tdls_set = TRUE;
		}
		FOREACH_WLC(wlc_cmn, idx, wlc_iter) {
			wlc_tdls_set(wlc_iter->tdls, tdls_set);
		}
	}
#endif /* WLTDLS */
	return err;
}

static void wlc_rsdb_mode_change_complete(wlc_info_t *wlc)
{
	uint16 htcap;
	struct scb *scb;
	struct scb_iter scbiter;
#if defined(AP) && defined(WL_MODESW)
	uint idx;
	wlc_bsscfg_t *icfg = NULL;
#endif /* AP && MODESW */
	enum wlc_bandunit bandunit;
	wlcband_t *band;

	wlc->stf->pwr_throttle_mask = wlc->stf->hw_txchain;

	/* push to BMAC driver */
	wlc_stf_chain_init(wlc);
	wlc_reprate_init(wlc);
	/* init per-band default rateset, depend on band->gmode */
#ifdef WL11AC
	wlc_rateset_vhtmcs_build(&wlc->default_bss->rateset, wlc->stf->op_rxstreams);
#endif // endif
	wlc_rateset_mcs_build(&wlc->default_bss->rateset, wlc->stf->op_rxstreams);
	wlc_vht_update_mcs_cap(wlc->vhti);
	wlc_vht_update_mcs_cap_ext(wlc->vhti);
	wlc_stf_phy_txant_upd(wlc);
	FOREACH_WLC_BAND(wlc, bandunit) {
		wlc_stf_ss_update(wlc, wlc->bandstate[bandunit]);
	}
	if (WLC_TXC_ENAB(wlc) && wlc->txc != NULL)
		wlc_txc_inv_all(wlc->txc);
	if (wlc->pub->up) {
		wlc_set_ratetable(wlc);
	}
#if defined(WL_BEAMFORMING)
	if (TXBF_ENAB(wlc->pub)) {
		wlc_txbf_txchain_upd(wlc->txbf);
		wlc_txbf_delete_all_link(wlc->txbf);
	}
#endif /* defined(WL_BEAMFORMING) */

#ifdef WLTXPWR_CACHE
	wlc_phy_txpwr_cache_invalidate(phy_tpc_get_txpwr_cache(WLC_PI(wlc)));
#endif /* WLTXPWR_CACHE */
	wlc_bandinit_ordered(wlc, wlc->home_chanspec);

	if (wlc->pub->up) {
		/* update coex_io_mask on mode switch */
		wlc_btcx_update_coex_iomask(wlc);
	}

	/* Init per band default rateset to retain new values during reinit */
	FOREACH_WLC_BAND(wlc, bandunit) {
		band = wlc->bandstate[bandunit];
		wlc_rateset_mcs_build(&band->defrateset, wlc->stf->op_rxstreams);
#ifdef WL11AC
		wlc_rateset_vhtmcs_build(&band->defrateset, wlc->stf->op_rxstreams);
#endif // endif
	}
#if defined(AP) && defined(WL_MODESW)
	/* Set target BSS for AP for bringup */
	FOREACH_AP(wlc, idx, icfg) {
		wlc_default_rateset(wlc, &icfg->current_bss->rateset);
	}
#endif /* AP && WL_MODESW */
	wlc_scb_ratesel_chanspec_clear(wlc);

	if (wlc->stf->hw_rxchain == 1) {
		htcap = wlc_ht_get_cap(wlc->hti);
		htcap &= ~HT_CAP_MIMO_PS_MASK;
		htcap |= (HT_CAP_MIMO_PS_ON << HT_CAP_MIMO_PS_SHIFT);
		wlc_ht_set_cap(wlc->hti, htcap);
	} else {
		/* MIMO PS is set to OFF by default for MIMO devices in HTCAP */
		htcap = wlc_ht_get_cap(wlc->hti);
		htcap &= ~HT_CAP_MIMO_PS_MASK;
		htcap |= (HT_CAP_MIMO_PS_OFF << HT_CAP_MIMO_PS_SHIFT);
		wlc_ht_set_cap(wlc->hti, htcap);
	}

	/* Initialize the scb's rateset with max values
	* supported by the peer device.
	*/
	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (BSSCFG_AP(scb->bsscfg)) {
			wlc_rsdb_update_scb_rateset(wlc->rsdbinfo, scb);
		}
	}

	/* Do ratesel init only when excursion is *NOT* active.
	 * When excursion is active ratesel init is done, it will clear the rspec
	 * history which is not desired.
	 */
	if (!wlc->excursion_active)
		wlc_scb_ratesel_init_all(wlc);

	if (wlc->pub->up) {
		wlc_bmac_update_synthpu(wlc->hw);
		/* Update beacon/probe response for any of the existing  APs */
		wlc_update_beacon(wlc);
		wlc_update_probe_resp(wlc, TRUE);
	}
	/* one core beacon rx feature needs to be enabled in MIMO and disabled for RSDB
	 * Calling this function once mode switch is done in either modes
	 */
#ifdef WLPM_BCNRX
	if (PM_BCNRX_ENAB(wlc->pub) && wlc->pub->up)
		wlc_pm_bcnrx_init(wlc);
#endif /* WLPM_BCNRX */

	return;
}

bool
wlc_rsdb_get_core_band_lock(wlc_rsdb_info_t *rsdbinfo)
{
	wlc_info_t *wlc = rsdbinfo->wlc;
	rsdb_core_cap_band_t band_cap[MAX_RSDB_MAC_NUM];
	uint i = 0;

	for (i = 0; i < MAX_RSDB_MAC_NUM; i++) {
		band_cap[i] = wlc->rsdbinfo->cmn->core_cap[i].band;
	}
	if ((band_cap[MAC_CORE_UNIT_0] ^ band_cap[MAC_CORE_UNIT_1]) ==
		(WL_RSDB_CORE_CAP_BAND_5G | WL_RSDB_CORE_CAP_BAND_2G)) {
		return TRUE;
	} else {
		return FALSE;
	}
}

/* Iovar handlers */
static int
rsdb_get_ver(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen)
{
	int err = BCME_OK;
	uint16 version;

	if (*olen < sizeof(version)) {
		*olen = 0;
		err = BCME_BUFTOOSHORT;
		goto done;
	}
	version = WL_RSDB_IOV_VERSION;

	*(uint16*)obuf = version;
	*olen = sizeof(version);
done:
	return err;
}

static int
rsdb_get_caps(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen)
{
	int ret = BCME_OK;
	int i;
	int xtlv_size;
	wlc_rsdb_info_t *rsdbinfo = (wlc_rsdb_info_t *)dig->cmd_ctx;
	wlc_info_t *wlc = rsdbinfo->wlc;
	rsdb_caps_response_v1_t *rsdb_caps = (rsdb_caps_response_v1_t *)obuf;

	xtlv_size = bcm_xtlv_size_for_data(sizeof(rsdb_caps_response_v1_t),
			BCM_XTLV_OPTION_ALIGN32);

	if (xtlv_size > *olen) {
		*olen = 0;
		ret = BCME_BUFTOOSHORT;
		goto fail;
	}
	/* fill the data */
	rsdb_caps->rsdb = (uint8)wlc->pub->cmn->_isrsdb;
	rsdb_caps->num_of_cores = (uint8)wlc->cmn->num_d11_cores;

	if (!WLC_DUALMAC_RSDB(wlc->cmn)) {
		/* 4355 is special case in rsdb chips which has reconfigurable mac
		 * and phy, capable of switching between 2x2 MIMO and 1+1 RSDB
		 */
		rsdb_caps->flags |= SYNCHRONOUS_OPERATION_TRUE;
		rsdb_caps->num_chains[0] = rsdb_caps->num_chains[1]
			= NON_DUALMAC_NUM_CHAINS_CORE0;
	} else {
		rsdb_caps->flags &= ~SYNCHRONOUS_OPERATION_TRUE;
		for (i = 0; i < wlc->cmn->num_d11_cores; i++) {
			rsdb_caps->num_chains[i] =
				wlc_stf_txchain_get_cap(wlc->cmn->wlc[i]);
			rsdb_caps->band_cap[i] = wlc->rsdbinfo->cmn->core_cap[i].band;
		}
	}
	*olen = sizeof(*rsdb_caps);
fail:
	return ret;
}

static int
rsdb_get_bands(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen)
{
	int ret = BCME_OK;
	int xtlv_size;
	wlc_rsdb_info_t *rsdbinfo = (wlc_rsdb_info_t *)dig->cmd_ctx;
	wlc_info_t *wlc = rsdbinfo->wlc;
	rsdb_bands_v1_t *rsdb_bands = (rsdb_bands_v1_t *)obuf;

	xtlv_size = bcm_xtlv_size_for_data(sizeof(rsdb_bands_v1_t),
			BCM_XTLV_OPTION_ALIGN32);
	if (xtlv_size > *olen) {
		*olen = 0;
		ret = BCME_BUFTOOSHORT;
		goto fail;
	}
	/* fill the data */
	wlc_rsdb_get_bands(wlc, rsdb_bands);

	*olen = sizeof(*rsdb_bands);
fail:
	return ret;
}

static int
wlc_rsdb_doiovar(void *hdl, uint32 actionid,
	void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif)
{
	wlc_rsdb_info_t *rsdbinfo = (wlc_rsdb_info_t *)hdl;
	wlc_info_t *wlc = rsdbinfo->wlc;
	int32 int_val = 0;
	bool bool_val;
	int err = BCME_OK;

	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	bool_val = (int_val != 0) ? TRUE : FALSE;
	BCM_REFERENCE(bool_val);

	switch (actionid) {
		case IOV_GVAL(IOV_RSDB):
		case IOV_SVAL(IOV_RSDB):
		{
			err = bcm_iov_doiovar(rsdbinfo->cmn->iov_parse_ctx, actionid, p,
					plen, a, alen, vsize, wlcif);
			break;
		}
		/* TODO: move all the below iovars to the new xtlv format */
		case IOV_GVAL(IOV_RSDB_MODE): {
			wl_config_t cfg;
			cfg.config = (wlc->cmn->rsdb_mode & WLC_RSDB_MODE_AUTO_MASK) ? AUTO : 0;
			cfg.status = WLC_RSDB_CURR_MODE(wlc);
			memcpy(a, &cfg, sizeof(wl_config_t));
			break;
		}
		case IOV_SVAL(IOV_RSDB_MODE): {
			wl_config_t cfg;
#if !defined(WLTEST)
			int arg;
#endif /* !WLTEST */
			if (plen >= (int)sizeof(int_val)) {
				bcopy(p, &cfg, sizeof(wl_config_t));
				int_val = cfg.config;
			} else {
				err = BCME_BADARG;
				break;
			}

			if ((int_val < WLC_RSDB_MODE_AUTO) || (int_val >= WLC_RSDB_MODE_MAX)) {
				err = BCME_BADARG;
				break;
			}

			if (int_val != WLC_RSDB_MODE_AUTO) {
				mboolset(wlc->cmn->rsdb_auto_override,
					WLC_RSDB_AUTO_OVERRIDE_USER);
			} else {
				mboolclr(wlc->cmn->rsdb_auto_override,
					WLC_RSDB_AUTO_OVERRIDE_USER);
			}

			err = wlc_rsdb_change_mode(wlc, int_val);

#if !defined(WLTEST)
				arg = !WLC_RSDB_SINGLE_MAC_MODE(int_val);
				wlc_iovar_op(wlc, "scan_parallel", NULL, 0, &arg, sizeof(arg),
					IOV_SET, wlcif);
#endif /* !WLTEST */
				break;
		}
#if defined(WLTEST)

		case IOV_GVAL(IOV_RSDB_COREMPC): {
			int32 tmp_radio_percore_dsabl;
			tmp_radio_percore_dsabl =
				mboolisset(wlc->pub->radio_disabled, WL_RADIO_PERCORE_DISABLE);
			memcpy(a, &tmp_radio_percore_dsabl, sizeof(tmp_radio_percore_dsabl));
			break;
		}

		case IOV_SVAL(IOV_RSDB_COREMPC): {
			bool bool_sval;
			if (plen >= (int)sizeof(int_val)) {
				bcopy(p, &int_val, sizeof(int_val));
			} else {
				err = BCME_BADARG;
				break;
			}
			bool_sval = (int_val != 0) ? TRUE : FALSE;
			if (bool_sval && !wlc_mpccap(wlc)) {
				err = BCME_UNSUPPORTED;
			break;
		}

			if (bool_sval == TRUE) {
				mboolset(wlc->pub->radio_disabled, WL_RADIO_PERCORE_DISABLE);
			} else {
				mboolclr(wlc->pub->radio_disabled, WL_RADIO_PERCORE_DISABLE);
			}
#ifdef STA
			wlc_radio_upd(wlc);	/* To go down for mpc 1 */
#endif /* STA */
			break;
		}
#endif // endif
#if defined(WLTEST) || defined(RSDB_SWITCH)
#if defined(WLRSDB) && defined(WL_MODESW)
		case IOV_GVAL(IOV_RSDB_SWITCH): {
			if (WLC_DUALMAC_RSDB(wlc->cmn)) {
				/* Dynamic switch is not not required in case
				 * of non-RSDB mac.
				 */
				err = BCME_UNSUPPORTED;
			} else if (AS_IN_PROGRESS(wlc)) {
				WL_ERROR(("wl%d: VSDB<->RSDB switch blocked."
					"Assoc is in progress\n",	WLCWLUNIT(wlc)));
				err = BCME_BUSY;
			} else if (RSDB_ACTIVE(wlc->pub) && !MCHAN_ACTIVE(wlc->pub)) {
				err = wlc_rsdb_dyn_switch(wlc, MODE_VSDB);
			} else if (!RSDB_ACTIVE(wlc->pub) && MCHAN_ACTIVE(wlc->pub)) {
				err = wlc_rsdb_dyn_switch(wlc, MODE_RSDB);
			} else {
				err = BCME_UNSUPPORTED;
			}
			*((uint32*)a) = BCME_OK;
			break;
		}
		case IOV_SVAL(IOV_RSDB_SWITCH):
			break;
#endif /* WLRSDB && WL_MODESW */
#ifdef RSDB_PM_MODESW
		case IOV_SVAL(IOV_RSDB_MOVE):
		{
			wlc_bsscfg_t *bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
			if (int_val == RSDB_MOVE_DYN) {
				/* go back to dynamic Mode */
				mboolclr(wlc->cmn->rsdb_move_ovr, WLC_RSDB_MOVE_FORCE_IOVAR);
			} else {
				mboolset(wlc->cmn->rsdb_move_ovr, WLC_RSDB_MOVE_FORCE_IOVAR);
				err = wlc_rsdb_move_connection(bsscfg, int_val, FALSE);
			}
			break;
		}
#endif	/* RSDB_PM_MODESW */
#endif // endif
		case IOV_SVAL(IOV_RSDB_PMMOVE_OVR):
		{
			if (bool_val)
				mboolset(wlc->cmn->rsdb_move_ovr, WLC_RSDB_MOVE_OVR_IOVAR);
			else
				mboolclr(wlc->cmn->rsdb_move_ovr, WLC_RSDB_MOVE_OVR_IOVAR);

			break;
		}
		case IOV_GVAL(IOV_RSDB_PMMOVE_OVR):
		{
			*((uint32*)a) = wlc->cmn->rsdb_move_ovr;
			break;
		}
		case IOV_SVAL(IOV_RSDB_LOAD_THRESH):
		{
			/* load is in % */
			if (int_val < 0 || int_val > 100) {
				WL_ERROR(("Value out of range \n"));
				err = BCME_RANGE;
				break;
			}
			rsdbinfo->load_threshold = (uint32)int_val;
			break;
		}
		case IOV_GVAL(IOV_RSDB_LOAD_THRESH):
		{
			memcpy(a, &rsdbinfo->load_threshold, sizeof(uint32));
			break;
		}
		default:
			err = BCME_UNSUPPORTED;
			break;
	}
	return err;
}
/* Move connection across chains in Dual MAC RSDB */
int
wlc_rsdb_move_connection(wlc_bsscfg_t *bsscfg, int8 chain, bool dynswitch)
{
	int err = BCME_OK;
	wlc_info_t * wlc = NULL;

	ASSERT(bsscfg);

	wlc = bsscfg->wlc;

	if ((!wlc->pub->up) || (!bsscfg->associated)) {
		return BCME_BUSY;
	}

	if (dynswitch == TRUE) {
		/* If in override Mode, return */
		if (wlc->cmn->rsdb_move_ovr) {
			WL_TRACE(("in ovr mode: return %x  \n", wlc->cmn->rsdb_move_ovr));
			return err;
		}
	}

	/* check if Dual MAC MODESWITCH enabled */
	if (!WLC_RSDB_PM_MODESW_ENAB(wlc->pub)) {
		return BCME_UNSUPPORTED;
	}
	err = wlc_rsdb_trigger_modeswitch(bsscfg, chain);

	return err;
}

/* initiate move to requested chain */
static int
wlc_rsdb_trigger_modeswitch(wlc_bsscfg_t *bsscfg, int8 mac_chain)
{
	uint8 new_nss = (mac_chain ? 0x1: 0x3);
	wlc_modesw_info_t *modesw = NULL;
	wlc_info_t * wlc = NULL;
	int err = BCME_OK;
	uint8 cur_bw, oper_mode, cur_mode = 0;
	bool rxnss_type = FALSE, is160_8080 = FALSE; /* Currently only type 0 is supported */

	ASSERT(bsscfg);

	wlc = bsscfg->wlc;
	modesw = wlc->modesw;

	ASSERT(modesw);

	/* check for valid/busy states */
	if (wlc_modesw_is_req_valid(modesw, bsscfg) != TRUE) {
		return BCME_BUSY;
	}

	/* get current mode */
	cur_mode = wlc_modesw_derive_opermode(modesw,
		bsscfg->current_bss->chanspec, bsscfg, wlc->stf->op_rxstreams);

	/* Update cur opermode */
	if (bsscfg->up) {
		bsscfg->oper_mode = cur_mode;
		bsscfg->oper_mode_enabled = TRUE;
	}

	cur_bw = DOT11_OPER_MODE_CHANNEL_WIDTH(cur_mode);

	/* Form new oper mode */
	oper_mode =  DOT11_D8_OPER_MODE(rxnss_type, new_nss, 0, is160_8080, cur_bw);

	WL_MODE_SWITCH(("%s: current mode %x requested mode %x \n",
		__FUNCTION__, cur_mode, oper_mode));

	/* Start opermode changes + bsscfg move */
	err = wlc_modesw_handle_oper_mode_notif_request(modesw,
		bsscfg, oper_mode, TRUE, MODESW_CTRL_RSDB_MOVE);

	if (err != BCME_OK) {
		WL_ERROR(("%d : RSDB move failied : current mode %x requested mode %x \n",
			err, cur_mode, oper_mode));
	}

	return err;
}
static void
wlc_rsdb_get_bands(wlc_info_t *wlc, rsdb_bands_v1_t *rsdb_ptr)
{
	wlc_info_t *wlc_primary = WLC_RSDB_GET_PRIMARY_WLC(wlc);
	wlc_cmn_info_t *wlc_cmn = wlc->cmn;

	rsdb_ptr->num_cores = (uint8)wlc_cmn->num_d11_cores;

	if (WLC_RSDB_DUAL_MAC_MODE(WLC_RSDB_CURR_MODE(wlc)) &&
		RSDB_ACTIVE(wlc->pub)) {
		/* RSDB mode and rsdb is active */
		wlc_info_t *wlc_2g, *wlc_5g;

		/* Get wlc_2g and wlc_5g given one wlc */
		wlc_rsdb_get_wlcs(wlc_primary, &wlc_2g, &wlc_5g);
		if (wlc_2g->pub->unit == 0)
		{
			rsdb_ptr->band[0] = WLC_BAND_2G;
			rsdb_ptr->band[1] = WLC_BAND_5G;
		} else {
			rsdb_ptr->band[0] = WLC_BAND_5G;
			rsdb_ptr->band[1] = WLC_BAND_2G;
		}
	} else {
		/* MIMO mode or main only */
		rsdb_ptr->band[0] = (wlc_primary->band->bandunit) ? WLC_BAND_5G: WLC_BAND_2G;
		/* wlc-1 band is invalid in mimo mode */
		rsdb_ptr->band[1] = WLC_BAND_INVALID;
	}
}
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_RSDB)
static int
wlc_rsdb_dump(void *w, struct bcmstrbuf *b)
{
	wlc_info_t *wlc_iter;
	wlc_bsscfg_t *bsscfg;
	wlc_info_t *wlc = (wlc_info_t*)w;
	wlc_cmn_info_t *wlc_cmn = wlc->cmn;

	int idx = 0, i;
	uint8 active = 0, w_ptr;
	int mode = wlc->cmn->rsdb_mode;
	char *automode = "";

	bcm_bprintf(b, "RSDB active:%d\n", RSDB_ACTIVE(wlc->pub));
	automode = (mode & WLC_RSDB_MODE_AUTO_MASK) ? "RSDB Mode: Auto, " : "";
	bcm_bprintf(b, "%sRSDB Mode : %d\n", automode, WLC_RSDB_EXTRACT_MODE(mode));
	bcm_bprintf(b, "PHY mode:%d\n", phy_get_phymode((phy_info_t *)WLC_PI(wlc)));
	bcm_bprintf(b, "HW mode:0x%x\n",
		(si_core_cflags(wlc->pub->sih, 0, 0) & SICF_PHYMODE) >> SICF_PHYMODE_SHIFT);

	FOREACH_WLC(wlc_cmn, idx, wlc_iter) {
	bcm_bprintf(b, "wl%d: ucode mode:0x%x\n",
		idx, wlc_bmac_shmphymode_dump(wlc_iter->hw));
	}

	bcm_bprintf(b, "active WLCs:");
	FOREACH_WLC(wlc_cmn, idx, wlc_iter) {
		if (wlc_iter->pub->associated) {
			bcm_bprintf(b, "wlc[%d] ", idx);
			active++;
		}
	}
	if (active)
		bcm_bprintf(b, "(%d)\n", active);
	else
		bcm_bprintf(b, "none\n");

	FOREACH_WLC(wlc_cmn, idx, wlc_iter) {
		char perm[32];

		bcm_bprintf(b, "\nwlc:%d\n", idx);

		bcm_bprintf(b, "wl%d: hw tx chain %d\n", idx,
			WLC_BITSCNT(wlc_iter->stf->hw_txchain));
		FOREACH_BSS(wlc_iter, i, bsscfg) {
			char ifname[32];
			char ssidbuf[SSID_FMT_BUF_LEN];

			bcm_bprintf(b, "\n");
			wlc_format_ssid(ssidbuf, bsscfg->SSID, bsscfg->SSID_len);
			strncpy(ifname, wl_ifname(wlc_iter->wl, bsscfg->wlcif->wlif),
				sizeof(ifname));
			ifname[sizeof(ifname) - 1] = '\0';
			bcm_bprintf(b, "BSSCFG %d: \"%s\"\n", WLC_BSSCFG_IDX(bsscfg), ssidbuf);
			bcm_bprintf(b, "IF-index:%d\n", bsscfg->wlcif->index);

			bcm_bprintf(b, "%s enable %d up %d \"%s\"\n",
				BSSCFG_AP(bsscfg)?"AP":"STA",
				bsscfg->enable, bsscfg->up, ifname);
			bcm_bprintf(b, "cur_bss->BSSID %s\n",
				bcm_ether_ntoa(&bsscfg->current_bss->BSSID, (char*)perm));

			wlc_format_ssid(ssidbuf, bsscfg->current_bss->SSID,
				bsscfg->current_bss->SSID_len);
			bcm_bprintf(b, "cur_bss->SSID \"%s\"\n", ssidbuf);
#ifdef STA
			if (BSSCFG_STA(bsscfg))
				bcm_bprintf(b, "assoc_state %d\n", bsscfg->assoc->state);

#endif /* STA */
		} /* FOREACH_BSS */
		bcm_bprintf(b, "\n");

#ifdef STA
		bcm_bprintf(b, "AS_IN_PROGRESS() %d stas_associated %d\n", AS_IN_PROGRESS(wlc_iter),
			wlc_iter->stas_associated);
#endif /* STA */

		bcm_bprintf(b, "aps_associated %d\n", wlc_iter->aps_associated);

		w_ptr = wlc_iter->rsdbinfo->chan_sw_info->w_ptr;
		bcm_bprintf(b, "\nChanspec | Dwell(in ms)\n");
		for (i = 0; i < MAX_RSDB_CHANSW_INFO; i++, w_ptr++) {
			w_ptr &= (MAX_RSDB_CHANSW_INFO - 1);
			bcm_bprintf(b, " 0x%x  | %dms\n",
				wlc_iter->rsdbinfo->chan_sw_info->chanspec[w_ptr],
				wlc_iter->rsdbinfo->chan_sw_info->dwelltime[w_ptr]);
		}
		/* Current channel and time since the core is in this channel */
		bcm_bprintf(b, "*0x%x  | %dms\n", wlc_iter->chanspec,
			(OSL_SYSUPTIME() - wlc_iter->rsdbinfo->chan_sw_info->last_switch_time));
	} /* FOREACH_WLC */
	return 0;
}

void
wlc_rsdb_chan_switch_dump(wlc_info_t *wlc, chanspec_t chanspec, uint32 dwell_time)
{
	if (wlc->rsdbinfo->chan_sw_info) {
		wlc->rsdbinfo->chan_sw_info->dwelltime[wlc->rsdbinfo->chan_sw_info->w_ptr] =
			dwell_time;
		wlc->rsdbinfo->chan_sw_info->chanspec[wlc->rsdbinfo->chan_sw_info->w_ptr++] =
			chanspec;
		wlc->rsdbinfo->chan_sw_info->w_ptr &= (MAX_RSDB_CHANSW_INFO - 1);

		/* Save the current time which is time for last channel switch time */
		wlc->rsdbinfo->chan_sw_info->last_switch_time = OSL_SYSUPTIME();
	}
}
#endif // endif

#ifdef WL_MODESW
int
wlc_rsdb_dyn_switch(wlc_info_t *wlc, bool mode)
{
	wlc_info_t *wlc1;
	wlc_info_t *wlc0;
	int ret = BCME_OK;

	wlc1 =  wlc->cmn->wlc[1];
	wlc0 =  wlc->cmn->wlc[0];
	if (!(wlc->cmn->rsdb_mode & WLC_RSDB_MODE_AUTO_MASK)) {
		return ret;
	}

	if (wlc_modesw_get_switch_type(wlc->cmn->wlc[0]->modesw) != MODESW_NO_SW_IN_PROGRESS) {
		WL_ERROR(("%s: Returning as modesw is currently active = %d !\n", __func__,
			wlc_modesw_get_switch_type(wlc->cmn->wlc[0]->modesw)));
		return BCME_BUSY;
	}
	WL_MODE_SWITCH(("wl%d: Start ASDB Mode Switch to %s\n", WLCWLUNIT(wlc),
		((mode == MODE_VSDB) ? "VDSB":"RSDB")));

	if (mode == MODE_VSDB) {
		if (wlc1->pub->associated) {
			wlc_modesw_set_switch_type(wlc0->modesw, MODESW_RSDB_TO_VSDB);
			wlc->rsdbinfo->cmn->timer_ctx->wlc = wlc;
			wlc_rsdb_schedule_timer(wlc1->rsdbinfo,
				wlc->rsdbinfo->cmn->rsdb_clone_timer,
				WL_RSDB_VSDB_CLONE_WAIT, FALSE);
		}
	}
	else if (mode == MODE_RSDB) {
		if (WLC_RSDB_SINGLE_MAC_MODE(WLC_RSDB_CURR_MODE(wlc))) {
#ifdef WL_MODESW
			if (wlc0->modesw) {
				wlc_modesw_set_switch_type(wlc0->modesw, MODESW_VSDB_TO_RSDB);
				wlc_rsdb_downgrade_wlc(wlc0);
			}
#endif // endif
		}
	}
#ifdef WL_MODESW
	/* TODO: this has to be moved to last function called in modeswitch */
	wlc_set_modesw_bw_init(wlc->modesw, TRUE);
#endif /* WL_MODESW */
	return ret;
}

static void
wlc_rsdb_opmode_change_cb(void *ctx, wlc_modesw_notif_cb_data_t *notif_data)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;
	wlc_bsscfg_t *bsscfg = NULL;
	wlc_bsscfg_t *icfg = NULL, *join_pend_cfg = NULL, *up_pend_cfg = NULL;
	uint8 new_rxnss, old_rxnss, new_bw, old_bw;
	int idx;
	ASSERT(wlc);
	ASSERT(notif_data);
	WL_MODE_SWITCH(("%s: MODESW Callback status = %d oper_mode_old = %x oper_mode_new = %x "
			"signal = %d\n", __FUNCTION__, notif_data->status, notif_data->opmode_old,
			notif_data->opmode, notif_data->signal));
	bsscfg = notif_data->cfg;

	switch (notif_data->signal) {
		case MODESW_DN_AP_COMPLETE:
		case MODESW_DN_STA_COMPLETE:
		case MODESW_UP_STA_COMPLETE:
		case MODESW_UP_AP_COMPLETE:
		case MODESW_ACTION_FAILURE:
		{
			/* Assoc state machine transition */
			if (RSDB_ENAB(wlc->pub) &&
				(wlc->cmn->rsdb_mode & WLC_RSDB_MODE_AUTO_MASK)) {
				FOREACH_BSS(wlc, idx, icfg) {
#ifdef AP
					if (BSSCFG_AP(icfg)) {
					}
					else
#endif // endif
					if (BSSCFG_IBSS(icfg) && !BSSCFG_NAN_MGMT(icfg)) {
						if (icfg->ibss_up_pending == TRUE)
							up_pend_cfg = icfg;
					}
					else
					if (BSSCFG_STA(icfg) &&
						icfg->assoc->state == AS_MODE_SWITCH_START) {
							join_pend_cfg = icfg;
					}
				}
				if (join_pend_cfg) {
					wlc_assoc_change_state(join_pend_cfg,
						AS_MODE_SWITCH_COMPLETE);
				}
				if (up_pend_cfg) {
					if (BSSCFG_IBSS(up_pend_cfg))
						wlc_rsdb_ibss_bringup(wlc, &up_pend_cfg);
					else
						wlc_rsdb_ap_bringup(wlc->rsdbinfo, &up_pend_cfg);

					(void)wlc_bsscfg_enable(up_pend_cfg->wlc, up_pend_cfg);
				}
			}
		}
		break;
		case MODESW_PHY_DN_COMPLETE:
			old_rxnss = DOT11_OPER_MODE_RXNSS(notif_data->opmode_old);
			new_rxnss = DOT11_OPER_MODE_RXNSS(notif_data->opmode);
			if (old_rxnss == new_rxnss) {
				break;
			}
			WL_MODE_SWITCH(("wl%d: Change chip mode to RSDB(%d) by cfg = %d\n",
				WLCWLUNIT(wlc), WLC_RSDB_MODE_RSDB, bsscfg->_idx));
			wlc_rsdb_change_mode(wlc, (WLC_RSDB_MODE_RSDB | (WLC_RSDB_IS_AUTO_MODE(wlc)
					? WLC_RSDB_MODE_AUTO_MASK : 0)));
		break;
		case MODESW_PHY_UP_START:
			old_rxnss = DOT11_OPER_MODE_RXNSS(notif_data->opmode_old);
			new_rxnss = DOT11_OPER_MODE_RXNSS(notif_data->opmode);
			old_bw = DOT11_OPER_MODE_CHANNEL_WIDTH(bsscfg->oper_mode);
			new_bw = DOT11_OPER_MODE_CHANNEL_WIDTH(notif_data->opmode);
			if (old_rxnss != new_rxnss) {
				WL_MODE_SWITCH(("wl%d: Change chip mode to MIMO(%d) by cfg = %d\n",
					WLCWLUNIT(wlc), WLC_RSDB_MODE_2X2, bsscfg->_idx));
				wlc_rsdb_change_mode(wlc, (WLC_RSDB_MODE_2X2 |
					(WLC_RSDB_IS_AUTO_MODE(wlc) ? WLC_RSDB_MODE_AUTO_MASK :
						0)));
			} else if ((old_bw != new_bw) && (new_bw == DOT11_OPER_MODE_8080MHZ)) {
				WL_MODE_SWITCH(("wl%d: Change chip mode to 80p80(%d) by cfg = %d\n",
					WLCWLUNIT(wlc), WLC_RSDB_MODE_80P80, bsscfg->_idx));
				wlc_rsdb_change_mode(wlc, (WLC_RSDB_MODE_80P80 |
					(WLC_RSDB_IS_AUTO_MODE(wlc) ? WLC_RSDB_MODE_AUTO_MASK :
						0)));
			}
		break;
	}
	return;
}

/* To keep track of STAs which get associated or disassociated */
static void
wlc_rsdb_scb_state_upd_cb(void *ctx, scb_state_upd_data_t *notif_data)
{
	wlc_rsdb_info_t *rsdbinfo = (wlc_rsdb_info_t *) ctx;
	wlc_info_t *wlc = rsdbinfo->wlc;
	wlc_bsscfg_t *bsscfg;
	struct scb *scb;
	uint8 oldstate = notif_data->oldstate;
	wlc_info_t *wlc1 = wlc->cmn->wlc[1];

	scb = notif_data->scb;
	bsscfg = scb->bsscfg;

	if (AS_IN_PROGRESS(wlc) || !WLC_RSDB_IS_AUTO_MODE(wlc) ||
		SCB_INTERNAL(scb)) {
		return;
	}

	if (BSSCFG_STA(bsscfg)) {
		if (((oldstate & ASSOCIATED) && !SCB_ASSOCIATED(scb)) ||
			((oldstate & AUTHENTICATED) && !SCB_AUTHENTICATED(scb))) {
			/* In case STA got a deauth, then it needs
			* to move to Core 0.
			*/
			if (wlc1 && (bsscfg->wlc == wlc1) &&	BSSCFG_STA(bsscfg) &&
				!BSS_P2P_ENAB(wlc1, bsscfg)) {
				WLRSDB_DBG(("wl%d: cfg %d Deauth Done. Check for Core 0 move\n",
					WLCWLUNIT(bsscfg->wlc), bsscfg->_idx));
				wlc->rsdbinfo->cmn->timer_ctx->wlc = wlc;
				wlc_rsdb_schedule_timer(wlc1->rsdbinfo,
					wlc->rsdbinfo->cmn->rsdb_clone_timer, 0, FALSE);
			}
		}
	}
}

static
void wlc_rsdb_assoc_state_cb(void *ctx, bss_assoc_state_data_t *notif_data)
{
	wlc_rsdb_info_t *rsdbinfo = (wlc_rsdb_info_t *) ctx;
	wlc_info_t *wlc = rsdbinfo->wlc;
	wlc_bsscfg_t *cfg = notif_data->cfg;
	uint8 fall_through = FALSE;
	bool PM = cfg->pm->PM;
#ifdef WLMCHAN
	int res = BCME_ERROR;
#endif // endif
	wlc_info_t *wlc0 = wlc->cmn->wlc[0];
	wlc_info_t *wlc1 = wlc->cmn->wlc[1];

	switch (notif_data->type) {
	case AS_VERIFY_TIMEOUT:
		/* fall through */
		fall_through = TRUE;
	case AS_RECREATE:
		if (WLC_MODESW_ENAB(wlc->pub) && (((notif_data->state == AS_ASSOC_VERIFY) &&
			WLC_MODESW_ANY_SWITCH_IN_PROGRESS(wlc0->modesw)) || fall_through)) {
			if (wlc_modesw_get_switch_type(wlc0->modesw) ==	MODESW_VSDB_TO_RSDB) {
				/* If we are currently switching from VSDB to RSDB, then this
				 * function confirms that modesw is done. Hence clear the
				 * modesw type.
				 */
				wlc_modesw_set_switch_type(wlc0->modesw, MODESW_NO_SW_IN_PROGRESS);
				WL_MODE_SWITCH(("wl%d: ASDB(V -> R) Switch Complete\n",
					WLCWLUNIT(wlc)));

			} else if (wlc_modesw_get_switch_type(wlc0->modesw) ==
					MODESW_RSDB_TO_VSDB) {
#ifdef WLMCHAN
				/* Before initiating the timer, set the call-back context */
				res = wlc_mchan_modesw_set_cbctx(wlc0, WLC_MODESW_UPGRADE_TIMER);

				if (res == BCME_OK) {
					/* Add timer with decent delay which is enough to receive
					 * the ACK for the null packet sent above.
					 */
					wlc_rsdb_schedule_timer(wlc0->rsdbinfo,
						wlc0->cmn->mchan_clone_timer,
						WL_RSDB_UPGRADE_TIMER_DELAY, FALSE);
				} else {
					WL_ERROR(("%s: Unable to add upgrade timer as "
					"wlc_mchan_modesw_set_cbctx returned with error code %d\n",
						__func__, res));
				}
#endif /* WLMCHAN */
			}
		}
		/* Reset the verify_timeout to its default value */
		notif_data->cfg->assoc->verify_timeout = WLC_ASSOC_VERIFY_TIMEOUT;
		mboolclr(cfg->pm->PMblocked, WLC_PM_BLOCK_MODESW);
		if (BSSCFG_IS_RSDB_CLONE(cfg)) {
			/* PM is restored from old cfg to be
			* set once RSDB move is complete.
			*/
			PM = cfg->pm->PM;
			cfg->pm->PM = 0;
			wlc_set_pm_mode(wlc, PM, cfg);
			cfg->flags &= ~WLC_BSSCFG_RSDB_CLONE;
		}
		break;
		case AS_NONE:
		if (notif_data->state == AS_IDLE) {
			/* In case STA was roaming/associating in Core 1
			* when Core 0 was brought down, then it needs
			* to move to Core 0 upon entering IDLE.
			*/
			if (wlc1 && !AS_IN_PROGRESS(wlc) && !wlc0->pub->up &&
				wlc1->pub->up && (cfg->wlc == wlc1) &&	BSSCFG_STA(cfg) &&
				!BSS_P2P_ENAB(wlc1, cfg) && WLC_RSDB_IS_AUTO_MODE(wlc)) {
				wlc->rsdbinfo->cmn->timer_ctx->wlc = wlc;
				wlc_rsdb_schedule_timer(wlc1->rsdbinfo,
					wlc->rsdbinfo->cmn->rsdb_clone_timer, 0, FALSE);
			}
		}
		break;
	default:
		/* other cases not handled */
		break;
	}
}
#endif /* WL_MODESW */

/*
 * Wrapper routine to allocate IOV context. Used as a callback by
 * the IOV framework.
 */
static void *
rsdb_iov_context_alloc(void *ctx, uint size)
{
	uint8 *buf = NULL;
	wlc_rsdb_info_t *rsdbinfo = (wlc_rsdb_info_t *)ctx;

	ASSERT(rsdbinfo != NULL);

	if ((buf = MALLOCZ(rsdbinfo->wlc->osh, size)) == NULL) {
		WL_ERROR(("%s: MALLOC failed, size %d malloced bytes %d\n",
				__FUNCTION__, size, MALLOCED(rsdbinfo->wlc->osh)));
	}
	return buf;
}

/*
 * Used as a callback when destroying
 * the context.
 */
static void
rsdb_iov_context_free(void *ctx, void *buf, uint size)
{
	wlc_rsdb_info_t *rsdbinfo = (wlc_rsdb_info_t *)ctx;

	ASSERT(rsdbinfo != NULL);

	MFREE(rsdbinfo->wlc->osh, buf, size);
}

int
rsdb_iov_cmd_validate(const bcm_iov_cmd_digest_t *dig, uint32 actionid,
		const uint8 *ibuf, size_t ilen, uint8 *obuf, size_t *olen)
{
	int err = BCME_OK;
	wlc_rsdb_info_t *rsdbinfo;
	wlc_info_t *wlc;

	ASSERT(dig);
	rsdbinfo = (wlc_rsdb_info_t *)(dig->cmd_ctx);
	ASSERT(rsdbinfo != NULL);

	wlc = rsdbinfo->wlc;
	UNUSED_PARAMETER(wlc);

	/* look up rsdb cmd handler in each submodule  */
	if (!RSDB_ENAB(wlc->pub) &&
		(dig->cmd_info->cmd != WL_RSDB_CMD_VER)) {
		err = BCME_UNSUPPORTED;
		goto fail;
	}
fail:
	return err;
}

static int
BCMATTACHFN(rsdb_iov_get_digest_cb)(void *ctx, bcm_iov_cmd_digest_t **dig)
{
	wlc_rsdb_info_t *rsdbinfo = (wlc_rsdb_info_t *)ctx;
	int err = BCME_OK;
	bcm_iov_cmd_digest_t *cmd_dig = NULL;

	ASSERT(rsdbinfo != NULL);

	cmd_dig = MALLOCZ(rsdbinfo->wlc->osh, sizeof(bcm_iov_cmd_digest_t));
	if (cmd_dig == NULL) {
		WL_ERROR(("wl%d: %s: MALLOC failed, malloced %d bytes\n",
			rsdbinfo->wlc->pub->unit, __FUNCTION__, MALLOCED(rsdbinfo->wlc->osh)));
		err = BCME_NOMEM;
		goto fail;
	}
	*dig = cmd_dig;
fail:
	return err;
}

int
BCMATTACHFN(rsdb_iov_register_cmd_handler)(wlc_rsdb_info_t *rsdbinfo,
	void *cmd_ctx, const bcm_iov_cmd_info_t *cmd_info, size_t max_cmds)
{
	int err;

	ASSERT(cmd_ctx != NULL);
	ASSERT(cmd_info != NULL);

	err = bcm_iov_register_commands(rsdbinfo->cmn->iov_parse_ctx, cmd_ctx,
		cmd_info, max_cmds,
		NULL, 0);
	return err;
}
int
BCMATTACHFN(wlc_rsdb_cfg_clone_register)(wlc_rsdb_info_t *rsdb,
	rsdb_cfg_clone_fn_t fn, void *arg)
{
	bcm_notif_h hdl = rsdb->cmn->cfg_clone_notif_hdl;
	return bcm_notif_add_interest(hdl, (bcm_notif_client_callback)fn, arg);
}

int
BCMATTACHFN(wlc_rsdb_cfg_clone_unregister)(wlc_rsdb_info_t *rsdb,
	rsdb_cfg_clone_fn_t fn, void *arg)
{
	bcm_notif_h hdl = rsdb->cmn->cfg_clone_notif_hdl;
	return bcm_notif_remove_interest(hdl, (bcm_notif_client_callback)fn, arg);
}

static void
wlc_rsdb_cfg_clone_notif(wlc_rsdb_info_t *rsdb, wlc_bsscfg_t *cfg,
	wlc_info_t *f_wlc, wlc_info_t *t_wlc, uint type)
{
	bcm_notif_h hdl = rsdb->cmn->cfg_clone_notif_hdl;
	rsdb_cfg_clone_upd_t notif_data;

	notif_data.cfg = cfg;
	notif_data.from_wlc = f_wlc;
	notif_data.to_wlc = t_wlc;
	notif_data.type = type;

	bcm_notif_signal(hdl, &notif_data);
}

wlc_rsdb_info_t *
BCMATTACHFN(wlc_rsdb_attach)(wlc_info_t* wlc)
{
	wlc_rsdb_info_t *rsdbinfo = NULL;
	bcm_iov_parse_context_t *parse_ctx = NULL;
	bcm_iov_parse_config_t parse_cfg;
	uint16 rsdbfstbmp =
#ifdef STA
		FT2BMP(FC_ASSOC_REQ) |
		FT2BMP(FC_REASSOC_REQ) |
		FT2BMP(FC_BEACON) |
#endif // endif
		0;
	uint16 bcvhtfstbmp =
#ifdef STA
		FT2BMP(FC_BEACON) |
#endif // endif
		0;

	/* Allocate RSDB module structure */
	if ((rsdbinfo = MALLOCZ(wlc->osh, sizeof(wlc_rsdb_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}
	rsdbinfo->wlc = wlc;

#if defined(WLRSDB_POLICY_MGR) && !defined(WLRSDB_POLICY_MGR_DISABLED)
	if ((rsdbinfo->as_cbinfo = MALLOCZ(wlc->osh, sizeof(rsdb_assoc_cb_info_t))) ==
	NULL) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	if ((rsdbinfo->rsdb_wlc_cbinfo = MALLOCZ(wlc->osh,
		sizeof(rsdb_get_wlc_cb_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
		wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
#endif /* WLRSDB_POLICY_MGR && !WLRSDB_POLICY_MGR_DISABLED */
	if (WLC_RSDB_IS_AUTO_MODE(wlc)) {
		/* Init stuff for WLC0 */
			if (wlc == WLC_RSDB_GET_PRIMARY_WLC(wlc)) {
#ifdef WL_MODESW
				if (WLC_MODESW_ENAB(wlc->pub)) {
					if (!(wlc->cmn->max_rateset = (wlc_rateset_t *)MALLOCZ(
							wlc->osh, sizeof(wlc_rateset_t)))) {
						goto fail;
					}
				}
#endif /* WL_MODESW */
		}
	}
	rsdbinfo->cmn = (rsdb_cmn_info_t *) obj_registry_get(wlc->objr, OBJR_RSDB_CMN_INFO);
	if (rsdbinfo->cmn == NULL) {
		/* Object not found ! so alloc new object here and set the object */

		if (!(rsdbinfo->cmn = (rsdb_cmn_info_t *)MALLOCZ(wlc->osh,
			sizeof(*(rsdbinfo->cmn))))) {
			WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			goto fail;
		}

		if (!(rsdbinfo->cmn->linked_cfg_indices = (int8 *)MALLOC(wlc->osh,
			sizeof(*(rsdbinfo->cmn->linked_cfg_indices)) * WLC_MAXBSSCFG))) {
			WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			goto fail;
		}
		memset(rsdbinfo->cmn->linked_cfg_indices, 0xff,
				(sizeof(*(rsdbinfo->cmn->linked_cfg_indices)) * WLC_MAXBSSCFG));

		if (!(rsdbinfo->cmn->linked_cfg_hostif_indices = (int *)MALLOC(wlc->osh,
			sizeof(*(rsdbinfo->cmn->linked_cfg_hostif_indices)) * WLC_MAXBSSCFG))) {
			WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			goto fail;
		}

		memset(rsdbinfo->cmn->linked_cfg_hostif_indices, 0xff,
				(sizeof(*(rsdbinfo->cmn->linked_cfg_hostif_indices)) *
				WLC_MAXBSSCFG));
		rsdbinfo->cmn->def_ampdu_mpdu = RSDB_AMPDU_MPDU_INIT_VAL;

		/* Create notification list for cfg clone event */
		if (bcm_notif_create_list(wlc->notif,
			&rsdbinfo->cmn->cfg_clone_notif_hdl) != BCME_OK) {
			WL_ERROR(("wl%d: %s: bcm_notif_create_list failed \n",
				wlc->pub->unit, __FUNCTION__));
			goto fail;
		}
		/* parse config */
		parse_cfg.alloc_fn = (bcm_iov_malloc_t)rsdb_iov_context_alloc;
		parse_cfg.free_fn = (bcm_iov_free_t)rsdb_iov_context_free;
		parse_cfg.dig_fn = (bcm_iov_get_digest_t)rsdb_iov_get_digest_cb;
		parse_cfg.max_regs = RSDB_IOV_MAX_REGS;
		parse_cfg.alloc_ctx = rsdbinfo;

		/* parse context */
		if ((bcm_iov_create_parse_context(&parse_cfg,
			&parse_ctx)) != BCME_OK) {
			WL_ERROR(("wl%d: %s parse context creation failed\n",
				wlc->pub->unit, __FUNCTION__));
			goto fail;
		}
		rsdbinfo->cmn->iov_parse_ctx = parse_ctx;
		/*
		* register rsdb sub commands
		*/
		if ((rsdb_iov_register_cmd_handler(rsdbinfo,
			rsdbinfo, &rsdb_sub_cmds[0],
			(size_t)CMD_TBL_SZ(rsdb_sub_cmds))) != BCME_OK) {
			WL_ERROR(("wl%d: rsdb sub command register failed\n", wlc->pub->unit));
			goto fail;
		}
		obj_registry_set(wlc->objr, OBJR_RSDB_CMN_INFO, rsdbinfo->cmn);
	}

	/* Hit a reference for this object */
	(void) obj_registry_ref(wlc->objr, OBJR_RSDB_CMN_INFO);

#if defined(BCMDBG) || defined(WLTEST) || defined(DNG_DBGDUMP) || defined(BCMDBG_RSDB)
	/* Allocate structure to store the last N channels on a particular core */
	if ((rsdbinfo->chan_sw_info = MALLOCZ(wlc->osh, sizeof(rsdb_chan_sw_info_t))) ==
			NULL) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
#endif // endif

	/* register scb cubby */
	rsdbinfo->scb_handle = wlc_scb_cubby_reserve(wlc, sizeof(wlc_rsdb_scb_cubby_t),
		NULL, NULL, NULL, (void *)rsdbinfo);

	if (rsdbinfo->scb_handle < 0) {
		WL_ERROR(("wl%d: wlc_scb_cubby_reserve() failed\n", wlc->pub->unit));
		goto fail;
	}

	/* Register HT Cap IE parser routine */
	if (wlc_iem_add_parse_fn_mft(wlc->iemi, rsdbfstbmp, DOT11_MNG_HT_CAP,
		wlc_rsdb_ht_parse_cap_ie, rsdbinfo) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_ier_add_parse_fn failed, "
			"ht cap ie in setupconfirm\n", wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	if (wlc_iem_add_parse_fn_mft(wlc->iemi, bcvhtfstbmp, DOT11_MNG_VHT_CAP_ID,
		wlc_rsdb_vht_parse_cap_ie, rsdbinfo) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_ier_add_parse_fn failed, "
			"vht cap ie in setupconfirm\n", wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_RSDB)
	if (wlc_dump_register(wlc->pub, "rsdb", wlc_rsdb_dump, (void*)wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_dumpe_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
	}
#endif // endif

	if (wlc_module_register(wlc->pub, rsdb_iovars, "rsdb", rsdbinfo, wlc_rsdb_doiovar,
	                        wlc_rsdb_watchdog, NULL, wlc_rsdb_down)) {
		WL_ERROR(("wl%d: wlc_rsdb_iovar_attach failed\n", wlc->pub->unit));
		goto fail;
	}

#ifdef WL_MODESW
	if (WLC_MODESW_ENAB(wlc->pub)) {
		if (wlc_modesw_notif_cb_register(wlc->modesw, wlc_rsdb_opmode_change_cb, wlc)) {
			WL_ERROR(("wl%d: MODESW module callback registration failed\n",
				wlc->pub->unit));
			goto fail;
		}

		if (wlc_bss_assoc_state_register(wlc, wlc_rsdb_assoc_state_cb, rsdbinfo)) {
			WL_ERROR(("wl%d: ASSOC module callback registration failed\n",
				wlc->pub->unit));
			goto fail;
		}

		/* Add client callback to the scb state notification list */
		if ((wlc_scb_state_upd_register(wlc, (scb_state_upd_cb_t)
				wlc_rsdb_scb_state_upd_cb, rsdbinfo)) != BCME_OK) {
			WL_ERROR(("wl%d: %s: unable to register callback %p\n",
				wlc->pub->unit, __FUNCTION__, wlc_rsdb_scb_state_upd_cb));
			goto fail;
		}
	}

	/* Init stuff for WLC1 */
	if (wlc != WLC_RSDB_GET_PRIMARY_WLC(wlc)) {
		if (!(rsdbinfo->cmn->timer_ctx =
			(rsdb_timer_cb_t *)MALLOCZ(wlc->osh,
			sizeof(rsdb_timer_cb_t)))) {
			WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			goto fail;
		}
		if (!(rsdbinfo->cmn->rsdb_clone_timer = wl_init_timer(wlc->wl,
			wlc_rsdb_clone_timer_cb, rsdbinfo->cmn->timer_ctx, "rsdb_clone_timer"))) {
			WL_ERROR(("wl%d: %s rsdb_clone_timer failed\n",
				wlc->pub->unit, __FUNCTION__));
			goto fail;
		}
		rsdbinfo->cmn->timer_ctx->notif_type = RSDB_TMR_TYPE_NONE;
	}
#endif /* WL_MODESW */

	/* Register bss state change callback handler */
	if (wlc_bsscfg_state_upd_register(wlc, wlc_rsdb_bss_state_upd, rsdbinfo) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_state_upd_register failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	wlc->pub->_assoc_recreate = TRUE;

	/* initialize the air time load threshold in % */
	/* Switch to 3x3 for load above this threshold */
	rsdbinfo->load_threshold = DUALMAC_RSDB_LOAD_THRESH;

	/* Disable dynamic move by default */
	/* PM & IOVAR based ovrs by default */
	mboolset(wlc->cmn->rsdb_move_ovr, WLC_RSDB_MOVE_OVR_IOVAR);
	mboolset(wlc->cmn->rsdb_move_ovr, WLC_RSDB_MOVE_OVR_PM);

#if defined(RSDB_CMN_BANDSTATE) && !defined(RSDB_CMN_BANDSTATE_DISABLED)
	wlc->pub->cmn->_isrsdb_cmn_bandstate = TRUE;

	/* Initialize common BW cap on the last core attach */
	if (wlc->pub->unit == (MAX_RSDB_MAC_NUM - 1)) {
		wlc_rsdb_derive_cmn_bwcap(rsdbinfo, WLC_BAND_2G);
		wlc_rsdb_derive_cmn_bwcap(rsdbinfo, WLC_BAND_5G);
	}
#endif /* defined(RSDB_CMN_BANDSTATE) && !defined(RSDB_CMN_BANDSTATE_DISABLED) */
	wlc_rsdb_derive_core_cap(rsdbinfo);
	return rsdbinfo;
fail:
	MODULE_DETACH(rsdbinfo, wlc_rsdb_detach);
	return NULL;
}

void
BCMATTACHFN(wlc_rsdb_detach)(wlc_rsdb_info_t* rsdbinfo)
{
	wlc_info_t *wlc = rsdbinfo->wlc;

	/* Unregister bss state change callback handler */
	(void)wlc_bsscfg_state_upd_unregister(wlc, wlc_rsdb_bss_state_upd, rsdbinfo);

#ifdef WL_MODESW
	if (WLC_MODESW_ENAB(wlc->pub)) {
		/* De-init stuff for WLC0 */
		if (wlc == WLC_RSDB_GET_PRIMARY_WLC(wlc)) {
			if (wlc->cmn->max_rateset) {
				MFREE(wlc->osh, wlc->cmn->max_rateset, sizeof(wlc_rateset_t));
			}
		}
		/* Remove client callback for the scb state notification list */
		wlc_scb_state_upd_unregister(wlc, (scb_state_upd_cb_t)
			wlc_rsdb_scb_state_upd_cb, rsdbinfo);
		wlc_modesw_notif_cb_unregister(wlc->modesw, wlc_rsdb_opmode_change_cb, wlc->modesw);
	}
#endif /* WL_MODESW */
#if defined(BCMDBG) || defined(WLTEST) || defined(DNG_DBGDUMP) || defined(BCMDBG_RSDB)
	if (rsdbinfo->chan_sw_info) {
		MFREE(rsdbinfo->wlc->osh, rsdbinfo->chan_sw_info,
			sizeof(rsdb_chan_sw_info_t));
	}
#endif // endif
	if (rsdbinfo->cmn != NULL) {
		if (wlc != WLC_RSDB_GET_PRIMARY_WLC(wlc)) {
			if (rsdbinfo->cmn->rsdb_clone_timer) {
				wl_free_timer(wlc->wl, rsdbinfo->cmn->rsdb_clone_timer);
				rsdbinfo->cmn->rsdb_clone_timer = NULL;
			}
			if (rsdbinfo->cmn->timer_ctx != NULL) {
				MFREE(wlc->osh, rsdbinfo->cmn->timer_ctx,
						sizeof(rsdb_timer_cb_t));
				rsdbinfo->cmn->timer_ctx = NULL;
			}
		}

		if (obj_registry_unref(rsdbinfo->wlc->objr, OBJR_RSDB_CMN_INFO) == 0) {
			obj_registry_set(rsdbinfo->wlc->objr, OBJR_RSDB_CMN_INFO, NULL);
			if (rsdbinfo->cmn->cfg_clone_notif_hdl != NULL) {
				bcm_notif_delete_list(&rsdbinfo->cmn->cfg_clone_notif_hdl);
			}

			if (rsdbinfo->cmn->linked_cfg_indices != NULL) {
				MFREE(wlc->osh, rsdbinfo->cmn->linked_cfg_indices,
						(sizeof(*rsdbinfo->cmn->linked_cfg_indices) *
						WLC_MAXBSSCFG));
				rsdbinfo->cmn->linked_cfg_indices = NULL;
			}
			if (rsdbinfo->cmn->linked_cfg_hostif_indices != NULL) {
				MFREE(wlc->osh, rsdbinfo->cmn->linked_cfg_hostif_indices,
						(sizeof(*rsdbinfo->cmn->linked_cfg_hostif_indices) *
						WLC_MAXBSSCFG));
				rsdbinfo->cmn->linked_cfg_hostif_indices = NULL;
			}
			bcm_iov_free_parse_context(&rsdbinfo->cmn->iov_parse_ctx,
				(bcm_iov_free_t)rsdb_iov_context_free);
			MFREE(rsdbinfo->wlc->osh, rsdbinfo->cmn, sizeof(rsdb_cmn_info_t));
		}
	}
	if (rsdbinfo->as_cbinfo)
		MFREE(wlc->osh, rsdbinfo->as_cbinfo, sizeof(rsdb_assoc_cb_info_t));
	if (rsdbinfo->rsdb_wlc_cbinfo)
		MFREE(wlc->osh, rsdbinfo->rsdb_wlc_cbinfo, sizeof(rsdb_get_wlc_cb_info_t));

	wlc_module_unregister(wlc->pub, "rsdb", rsdbinfo);
	MFREE(wlc->osh, rsdbinfo, sizeof(wlc_rsdb_info_t));
}

int
wlc_rsdb_get_wlcs(wlc_info_t *wlc, wlc_info_t **wlc_2g, wlc_info_t **wlc_5g)
{
	wlc_rsdb_info_t  *rsdbinfo = wlc->rsdbinfo;
	int err = BCME_OK;
	wlc_info_t *wlc_iter = NULL;
	wlc_info_t *wlc_associated = NULL;
	int idx = 0;
	if (!(rsdbinfo->wlc->cmn->rsdb_mode & WLC_RSDB_MODE_AUTO_MASK) &&
		WLRSDB_POLICY_MGR_ENAB(rsdbinfo->wlc->pub) &&
		rsdbinfo->rsdb_wlc_cbinfo->cb) {
		/* Check for infra association on primary cfg */
		FOREACH_WLC(wlc->cmn, idx, wlc_iter) {
			if (wlc_iter->cfg->associated) {
				wlc_associated = wlc_iter;
				break;
			}
		}
		if (wlc_associated) {
			err = wlc_rsdb_auto_get_wlcs(wlc, &(*wlc_2g), &(*wlc_5g));
		} else {
			err = rsdbinfo->rsdb_wlc_cbinfo->cb(rsdbinfo->rsdb_wlc_cbinfo->ctx,
			wlc, &(*wlc_2g), &(*wlc_5g));
		}
	} else {
		err = wlc_rsdb_auto_get_wlcs(wlc, &(*wlc_2g), &(*wlc_5g));
	}

	if (SRPWR_ENAB()) {
		wlc_srpwr_request_on_all(wlc, FALSE);
	}

	return err;
}

/* API to get both wlc pointers given single wlc.
 * API return wlc_2g and wlc_5g assuming preferred
 * band for that wlc
 */
int
wlc_rsdb_auto_get_wlcs(wlc_info_t *wlc, wlc_info_t **wlc_2g, wlc_info_t **wlc_5g)
{
	int idx = 0;
	wlc_bsscfg_t *cfg;
	wlc_info_t *wlc_2g_l = NULL, *wlc_5g_l = NULL;
	/* Get the preferred core for 5G and 2G bands */
	wlc_5g_l = wlc_rsdb_find_wlc_for_band(wlc->rsdbinfo, BAND_5G_INDEX, NULL);
	wlc_2g_l = wlc_rsdb_find_wlc_for_band(wlc->rsdbinfo, BAND_2G_INDEX, wlc_5g_l);
	*wlc_2g = wlc_2g_l;
	*wlc_5g = wlc_5g_l;
	/* overwrite based on current association. Do we really need this section */
	/*	If it is a bandlocked device, no need to check according to chanspec */
	if (!(wlc_rsdb_get_core_band_lock(wlc->rsdbinfo))) {
		for (idx = 0; idx < WLC_MAXBSSCFG; idx++) {
			cfg = wlc->bsscfg[idx];
			if (cfg && cfg->associated) {
				if (CHSPEC_IS2G(cfg->current_bss->chanspec)) {
					*wlc_2g = cfg->wlc;
					*wlc_5g = wlc_rsdb_get_other_wlc(*wlc_2g);
				} else {
					*wlc_5g = cfg->wlc;
					*wlc_2g = wlc_rsdb_get_other_wlc(*wlc_5g);
			}
				break;
			}
		}
	}
	/*
	 *	This condifion should be accessed only if RSDB is active.
	 *	The condition should never be reached if RSDB is not active.
	 *	Need to verify why this consition is reached when RSDB is
	 *	not active.
	 */
	if (RSDB_ACTIVE(wlc->pub)) {
		ASSERT((wlc_2g != wlc_5g) && *wlc_2g && *wlc_5g);
	}
	return BCME_OK;
}

/* API to get the other wlc given one wlc */
wlc_info_t *
wlc_rsdb_get_other_wlc(wlc_info_t *wlc)
{
	wlc_cmn_info_t* wlc_cmn;
	wlc_info_t *to_wlc;
	int idx;

	WL_TRACE(("wl%d:%s:%d\n", wlc->pub->unit, __FUNCTION__, __LINE__));
	wlc_cmn = wlc->cmn;
	FOREACH_WLC(wlc_cmn, idx, to_wlc) {
		if (wlc != to_wlc) {

			/* Other wlc might be in power down so if we need to
			 * access registers/etc., need to request power
			 */
			if (SRPWR_ENAB()) {
				wlc_srpwr_request_on(to_wlc);
			}

			return to_wlc;
		}
	}
	return NULL;
}

int
wlc_rsdb_any_wlc_associated(wlc_info_t *wlc)
{
	int idx;
	wlc_cmn_info_t *wlc_cmn = wlc->cmn;
	wlc_info_t *wlc_iter;
	int assoc_count = 0;

	FOREACH_WLC(wlc_cmn, idx, wlc_iter) {
		if (wlc_iter && wlc_iter->pub && wlc_iter->pub->associated) {
			assoc_count++;
		}
	}
	return assoc_count;
}

static uint8
BCMRAMFN(wlc_rsdb_map_core_bw)(chanspec_t chanspec)
{
	uint32 bw;
	bw = (chanspec & WL_CHANSPEC_BW_MASK) >> WL_CHANSPEC_BW_SHIFT;
	return rsdb_core_bw_map[bw];
}

static uint8
BCMRAMFN(wlc_rsdb_map_core_band)(chanspec_t chanspec)
{
	uint32 band;
	band = (chanspec & WL_CHANSPEC_BAND_MASK) >> WL_CHANSPEC_BAND_SHIFT;
	return rsdb_core_band_map[band];
}

static void
wlc_rsdb_create_reqd_cap(wlc_info_t *wlc, chanspec_t chanspec, wlc_bss_info_t *bi,
	wlc_rsdb_core_cap_t* out_cap)
{
	out_cap->band = wlc_rsdb_map_core_band(chanspec);
	out_cap->bw = wlc_rsdb_map_core_bw(chanspec);
	out_cap->chains = WLC_BITSCNT(wlc->stf->hw_txchain_cap);
	if (bi && !wlc_bss_get_mimo_cap(bi)) {
		out_cap->chains = 1;
	}
	return;
}

/* This function populates the RSDB core BW capabilites based upon
* the given phy capabilities bit map.
*/
static rsdb_core_cap_bw_t
BCMATTACHFN(wlc_rsdb_get_core_bw)(wlc_rsdb_info_t *rsdbinfo)
{
	rsdb_core_cap_bw_t out_bw = WL_RSDB_CORE_CAP_BW_NONE;
	wlc_info_t *wlc = rsdbinfo->wlc;
	/* 20 Mhz is mandatory */
	out_bw |= WL_RSDB_CORE_CAP_BW_20;
	out_bw |= WLC_40MHZ_CAP_PHY(wlc) ? WL_RSDB_CORE_CAP_BW_40 : 0;
	out_bw |= WLC_80MHZ_CAP_PHY(wlc) ? WL_RSDB_CORE_CAP_BW_80 : 0;
	out_bw |= WLC_160MHZ_CAP_PHY(wlc) ? WL_RSDB_CORE_CAP_BW_160 : 0;
	out_bw |= WLC_8080MHZ_CAP_PHY(wlc) ? WL_RSDB_CORE_CAP_BW_8080 : 0;
	out_bw |= WLC_10MHZ_CAP_PHY(wlc) ? WL_RSDB_CORE_CAP_BW_10 : 0;
	out_bw |= WLC_5MHZ_CAP_PHY(wlc) ? WL_RSDB_CORE_CAP_BW_5 : 0;
	out_bw |= WLC_2P5MHZ_CAP_PHY(wlc) ? WL_RSDB_CORE_CAP_BW_2P5 : 0;
	return out_bw;
}

/* Map board's fem cap to core_band */
static rsdb_core_cap_band_t
BCMATTACHFN(wlc_rsdb_get_core_band)(wlc_rsdb_info_t *rsdbinfo)
{
	wlc_info_t *wlc = rsdbinfo->wlc;
	rsdb_core_cap_band_t band_cap = WL_RSDB_CORE_CAP_BAND_NONE;

	if (PHY_BOARD_2G_FEM((phy_info_t *)WLC_PI(wlc))) {
		band_cap |= WL_RSDB_CORE_CAP_BAND_2G;
	}

	if (PHY_BOARD_5G_FEM((phy_info_t *)WLC_PI(wlc))) {
		band_cap |= WL_RSDB_CORE_CAP_BAND_5G;
	}
	ASSERT(band_cap);
	return band_cap;
}

/* This function derives the HW capabilities of the core based upon the
* PHY inputs and boardtype settings.
*/

static void
BCMATTACHFN(wlc_rsdb_derive_core_cap)(wlc_rsdb_info_t *rsdbinfo)
{
	wlc_rsdb_core_cap_t *core_cap;
	wlc_info_t *wlc = rsdbinfo->wlc;
	uint idx = WLCWLUNIT(wlc);
	uint8 chains = WLC_BITSCNT(wlc->stf->hw_txchain_cap);
	core_cap = rsdbinfo->cmn->core_cap;
	core_cap[idx].band = wlc_rsdb_get_core_band(rsdbinfo);
	core_cap[idx].bw = wlc_rsdb_get_core_bw(rsdbinfo);
	core_cap[idx].chains = chains;
	return;
}
#if defined(WLTEST)
/* Set band lock and return true if posible else return false */
static bool
wlc_rsdb_set_core_band_lock(wlc_rsdb_info_t *rsdbinfo, bool is_bandswitch)
{
	wlc_info_t *wlc = rsdbinfo->wlc;
	rsdb_core_cap_band_t band;
	enum wlc_bandunit bandunit;
	uint idx = WLCWLUNIT(wlc);

	band = rsdbinfo->cmn->core_cap[idx].band;
	ASSERT(band);
	wlc->bandlocked = FALSE;
	band &= (WL_RSDB_CORE_CAP_BAND_2G | WL_RSDB_CORE_CAP_BAND_5G);

	/* if core with dual-band cap, then we skip band locking */
	if ((!band) || ((band & WL_RSDB_CORE_CAP_BAND_2G) && (band & WL_RSDB_CORE_CAP_BAND_5G)))
		return FALSE;

	if (is_bandswitch) {
		bandunit = (band & WL_RSDB_CORE_CAP_BAND_5G) ? BAND_5G_INDEX : BAND_2G_INDEX;

		/* switch to first channel in the new band */
		wlc_change_band(wlc, bandunit);
	}
	wlc->bandlocked = TRUE;

	return TRUE;
}
#endif /* WLTEST */

/* API to lock band for both slices */
void
BCMRAMFN(wlc_rsdb_do_band_lock)(wlc_info_t *wlc, bool is_bandswitch)
{
#if defined(WLTEST)
	if ((WLCWLUNIT(wlc) == (MAX_RSDB_MAC_NUM - 1)) && RSDB_ENAB(wlc->pub)) {
		wlc_info_t *wlc_iter;
		int idx;
		FOREACH_WLC(wlc->cmn, idx, wlc_iter) {
			wlc_rsdb_set_core_band_lock(wlc_iter->rsdbinfo, is_bandswitch);
		}
	}
#endif /* WLTEST */
}
/* Provides the list of WLC's as per the FEM configuration.
 * match_mask (bit mask): WL_RSDB_CORE_MATCH_BW, WL_RSDB_CORE_MATCH_CHAINS
 * WL_RSDB_CORE_MATCH_BW: Return the most optimal core to use based upon the BW
 * WL_RSDB_CORE_MATCH_CHAINS: Return the most optimal core to use based upon the NSS
 * Match_mask of ZERO means that BW and NSS values can be ignored. In that case, the list of
 * all WLC's supporting the given band are returned.
 */
static int
wlc_rsdb_preferred_wlc(wlc_rsdb_info_t *rsdbinfo, wlc_info_t** out_list, chanspec_t chanspec,
	wlc_bss_info_t *bi, uint32 match_mask)
{
	wlc_info_t *wlc = rsdbinfo->wlc;
	int found = 0;
	int idx = 0, bw_bit = 0, count = 0;
	wlc_info_t* iwlc = NULL;
	wlc_info_t* filter_list[MAX_RSDB_MAC_NUM];
	wlc_rsdb_core_cap_t req_cap;
	wlc_rsdb_create_reqd_cap(wlc, chanspec, bi, &req_cap);
	memset(filter_list, 0, sizeof(*filter_list) * MAX_RSDB_MAC_NUM);
	if (match_mask & WL_RSDB_CORE_MATCH_BAND) {
		/* Filter the list to get a wlc which can only work
		* on the requested band. If such wlc is not used
		* then it will not be usable for later connections.
		*/
		FOREACH_WLC(wlc->cmn, idx, iwlc) {
			if (rsdbinfo->cmn->core_cap[idx].band == req_cap.band) {
				out_list[idx] = iwlc;
				found++;
			}
		}
		if (!found) {
			/* Get the basic list which contains wlc's based
			* upon HW capabilites without any filtering.
			*/
			FOREACH_WLC(wlc->cmn, idx, iwlc) {
				if (req_cap.band & rsdbinfo->cmn->core_cap[idx].band) {
					out_list[idx] = iwlc;
					found++;
				}
			}
		}
		bw_bit = bcm_find_fsb(req_cap.bw);
		/* We can try to filter the wlc based upon BW if we have more
		* than one possible option.
		*/
		if ((match_mask & WL_RSDB_CORE_MATCH_BW) && bw_bit && (found > 1)) {
			/* We have to check for band of chanspec
			* to get to the most optimal option of WLC.
			* Without this, we may endanger future
			* other band connections, who may need
			* the WLC with higher or lower bandwidth.
			*/
			if (CHSPEC_IS2G(chanspec)) {
				/* Find the core which do not support
				* higher than the requested bandwidth.
				*/
				count = 0;
				FOREACH_WLC(wlc->cmn, idx, iwlc) {
					if (!(rsdbinfo->cmn->core_cap[idx].bw >> bw_bit)) {
						filter_list[idx] = iwlc;
						count++;
					}
				}
			} else {
				/* Find the core which supports
				* higher than or equal to requested
				* bandwidth.
				*/
				count = 0;
				FOREACH_WLC(wlc->cmn, idx, iwlc) {
					if ((rsdbinfo->cmn->core_cap[idx].bw >> (bw_bit - 1))) {
						filter_list[idx] = iwlc;
						count++;
					}
				}
			}
			if (count) {
				/* We found something. So previous
				* list can be cleared and the new list
				* can be used now.
				*/
				found = count;
				FOREACH_WLC(wlc->cmn, idx, iwlc) {
					out_list[idx] = filter_list[idx];
				}
				/* XXX: Do we need to try to find
				* the exact match. May be not.
				*/
			}
			if ((match_mask & WL_RSDB_CORE_MATCH_CHAINS) && (found > 1)) {
				count = 0;
				/* Seems we found multiple options even after BW filtering */
				FOREACH_WLC(wlc->cmn, idx, iwlc) {
					filter_list[idx] = NULL;
					if (rsdbinfo->cmn->core_cap[idx].chains == req_cap.chains) {
						/* We found exact match */
						filter_list[idx] = iwlc;
						count++;
					}
				}
				/* Check if we found something.
				* Otherwise existing list is good
				* to go.
				*/
				if (count) {
					/* Overwrite the main list
					* with new entries.
					*/
					found = count;
					FOREACH_WLC(wlc->cmn, idx, iwlc) {
						out_list[idx] = filter_list[idx];
					}
				}
			}
		}
	} else {
		/* All wlc's can be returned */
		FOREACH_WLC(wlc->cmn, idx, iwlc) {
			out_list[idx] = iwlc;
			found++;
		}
	}
	return found;
}

/* Finds the WLC for RSDB usage based on chanspec depending upon match_mask
 * Return any free / un-associated WLC. Returns associated wlc if operating chanspec
 * matches.
 *
 * bi: bss info for which the WLC is required. Chanspec is also used from bi, if bi is not NULL.
 *
 * Chanspec: chanspec which would be used on the given wlc. Used only if (bi == NULL)
 *
 * Skip_wlc: The wlc which should not be provided as an option. It may happen that the
 * caller already has one wlc with it, and it wants another one.
 *
 * match_mask (bit mask): WL_RSDB_CORE_MATCH_BW, WL_RSDB_CORE_MATCH_CHAINS
 * WL_RSDB_CORE_MATCH_BW: Return the most optimal core to use based upon the BW
 * WL_RSDB_CORE_MATCH_CHAINS: Return the most optimal core to use based upon the NSS
 * Match_mask of ZERO means that BW and NSS values can be ignored. In that case, the first
 * WLC supporting the specified band in chanspec will be provided.
 */
wlc_info_t*
wlc_rsdb_find_wlc_for_chanspec(wlc_info_t *wlc, wlc_bss_info_t *bi,	chanspec_t chanspec,
	wlc_info_t *skip_wlc, uint32 match_mask)
{
	int idx, ret;
	wlc_info_t* iwlc = NULL;
	wlc_info_t* to_wlc = NULL;
	int wlc_busy = 0;
	DBGONLY(char chanbuf[CHANSPEC_STR_LEN]; )
	wlc_info_t *list_wlc[MAX_RSDB_MAC_NUM];
#ifdef WLTEST
	WLRSDB_DBG(("Return wlc:%d For chspec:%x\n", wlc->pub->unit, chanspec));
	return wlc;
#endif // endif
	/* XXX: DVT builds, that pickup release targets use band-locked interfaces
	 * Avoid clone if band is locked
	*/
	if (wlc->bandlocked) {
		wlc_rsdb_core_cap_t *core_cap = &(wlc->rsdbinfo->cmn->core_cap[wlc->pub->unit]);
		if (core_cap->band & wlc_rsdb_map_core_band(chanspec)) {
			WLRSDB_DBG(("Band Locked Return wlc:%d For chspec:%x\n",
				wlc->pub->unit, chanspec));
			return wlc;
		} else {
			/* Unsupported / core-band-cap mismatch */
			WL_PRINT(("RSDB:Unsupported core-band/bw in bandlocked mode\n"));
			OSL_SYS_HALT();
		}
	}
	/* Band/BW matching is mandatory for this API */
	match_mask |= (WL_RSDB_CORE_MATCH_BAND | WL_RSDB_CORE_MATCH_BW);
	memset(list_wlc, 0, (MAX_RSDB_MAC_NUM * sizeof(*list_wlc)));

	WLRSDB_DBG(("%s: Incoming wlc:%d Request wlc For chspec:%s \n", __FUNCTION__,
		wlc->pub->unit, wf_chspec_ntoa_ex(chanspec, chanbuf)));

	do {
		ret = wlc_rsdb_preferred_wlc(wlc->rsdbinfo, list_wlc, chanspec, bi, match_mask);
		/* No wlc could be found. So return incoming wlc.
		* This is very unlikely and incoming wlc is best
		* option we are left with at this point of time.
		*/
		if (!ret) {
			WLRSDB_DBG(("wl%d: Error no matching wlc found for"
				" chanspec = %x\n", WLCWLUNIT(wlc), chanspec));
			ASSERT(FALSE);
			return NULL;
		}
		/* Looks like we have some wlc in hand.
		* Check if we were told to avoid this wlc.
		*/
		if (skip_wlc) {
			list_wlc[skip_wlc->pub->unit] = NULL;
		}
		/* Remove the wlc's from the list which have
		* different band connections.
		*/
		FOREACH_WLC(wlc->cmn, idx, iwlc) {
			if (list_wlc[idx]) {
				wlc_busy = 0;
				if (iwlc->pub->associated) {
					int bss_idx;
					wlc_bsscfg_t *cfg;
					FOREACH_AS_BSS(iwlc, bss_idx, cfg) {
					    if (cfg->assoc->type == AS_NONE) { /* cfg not in Roam */
							wlc_busy = 1;
							if ((CHSPEC_BAND(cfg->current_bss->chanspec)
								== CHSPEC_BAND(chanspec))) {
							    /* Band matched with as'd wlc. MCHAN. */
						        WLRSDB_DBG(("Return MCNX wlc:%d For "
						        "chspec:%s\n", iwlc->pub->unit,
						        wf_chspec_ntoa_ex(chanspec, chanbuf)));
								return iwlc;
							}
						}
					} /* FOREACH_AS_BSS */
				}
				if (wlc_busy) {
					list_wlc[idx] = NULL;
				}
			}
		} /* FOREACH_WLC */
		/* Pick the incoming wlc if it is part
		* of the list. This helps in avoiding
		* cloning when not required.
		*/
		idx = 0;
		while (idx < wlc->cmn->num_d11_cores) {
			if (list_wlc[idx] == wlc) {
				to_wlc = wlc;
			}
			idx++;
		}
		/* Lets pick the first wlc from list */
		idx = 0;
		while ((to_wlc == NULL) && (idx < wlc->cmn->num_d11_cores)) {
			to_wlc = list_wlc[idx++];
		}
	} while ((to_wlc == NULL) && (match_mask >>= 1) && match_mask);
	WLRSDB_DBG(("Return wlc:%d For chspec:%x\n", to_wlc->pub->unit, chanspec));
	ASSERT(to_wlc);
	return to_wlc;
}

/* Finds the WLC for RSDB usage based on given band depending upon match_mask
 * and the HW capabilities of the WLC's.
 *
 * bi: bss info for which the WLC is required. Chanspec is also used from bi, if bi is not NULL.
 *
 * Chanspec: chanspec which would be used on the given wlc. Used only if (bi == NULL)
 *
 * Skip_wlc: The wlc which should not be provided as an option. It may happen that the
 * caller already has one wlc with it, and it wants another one.
 *
 */
wlc_info_t*
wlc_rsdb_find_wlc_for_band(wlc_rsdb_info_t *rsdbinfo, enum wlc_bandunit bandunit,
                           wlc_info_t *skip_wlc)
{
	int idx, ret;
	wlc_info_t *wlc = rsdbinfo->wlc;
	wlc_info_t *to_wlc = NULL;
	chanspec_t chanspec;
	uint32 match_mask = 0;
	wlc_info_t *list_wlc[MAX_RSDB_MAC_NUM];
	uint start_channel;
	if (bandunit == BAND_2G_INDEX) {
		start_channel = BAND_2G_START_CHANNEL;
	} else {
		start_channel = BAND_5G_START_CHANNEL;
	}
	chanspec = CH20MHZ_CHSPEC(start_channel);
	/* Band matching is mandatory */
	match_mask |= WL_RSDB_CORE_MATCH_BAND;
	memset(list_wlc, 0, (MAX_RSDB_MAC_NUM * sizeof(*list_wlc)));
	ret = wlc_rsdb_preferred_wlc(wlc->rsdbinfo, list_wlc, chanspec, NULL, match_mask);
	/* No wlc could be found. So return incoming wlc.
	* This is very unlikely and incoming wlc is best
	* option we are left with at this point of time.
	*/
	if (!ret) {
		WLRSDB_DBG(("wl%d: Error no matching wlc found for"
			" chanspec = %x\n", WLCWLUNIT(wlc), chanspec));
		return NULL;
	}
	/* Looks like we have some wlc in hand.
	* Check if we were told to avoid this wlc.
	*/
	if (skip_wlc) {
		list_wlc[skip_wlc->pub->unit] = NULL;
	}
	/* Lets pick the first wlc from list */
	idx = 0;
	do {
		to_wlc = list_wlc[idx++];
	} while ((to_wlc == NULL) && (idx < wlc->cmn->num_d11_cores));
	return to_wlc;
}

/* Finds no of WLC's which can work on the specified bandunit.
 * bandunit : BAND_5G_INDEX or BAND_2G_INDEX
 */
uint8
wlc_rsdb_num_wlc_for_band(wlc_rsdb_info_t *rsdbinfo, enum wlc_bandunit bandunit)
{
	int idx;
	wlc_info_t *wlc = rsdbinfo->wlc, *iwlc = NULL;
	chanspec_t chanspec;
	uint8 found = 0;
	wlc_rsdb_core_cap_t req_cap;
	chanspec = wlc_default_chanspec_by_band(wlc->cmi, bandunit);
	wlc_rsdb_create_reqd_cap(wlc, chanspec, NULL, &req_cap);

	FOREACH_WLC(wlc->cmn, idx, iwlc) {
		if (req_cap.band & rsdbinfo->cmn->core_cap[idx].band) {
			found++;
		}
	}
	return found;
}

/* RSDB bsscfg allocation based on WLC band allocation
 * For RSDB, this involves creating a new cfg in the target WLC
 * Typically, called from JOIN/ROAM-attempt, AP-up
*/
wlc_bsscfg_t* wlc_rsdb_cfg_for_chanspec(wlc_info_t *wlc, wlc_bsscfg_t *cfg, wlc_bss_info_t *bi)
{
	wlc_info_t *to_wlc = wlc_rsdb_find_wlc_for_chanspec(wlc, bi, bi->chanspec, NULL,
		WL_RSDB_CORE_MATCH_BW);
	wlc_bsscfg_t *to_cfg;

	if (to_wlc != wlc) {
		/* valid clone check for wlc 0 */
		if ((wlc == WLC_RSDB_GET_PRIMARY_WLC(wlc))) {
			int bss_idx;
			int from_wlc_as_count = 0;
			int to_wlc_as_count = 0;
			wlc_bsscfg_t *icfg;

			/* Get a list of other cfgs in the from_wlc */
			FOREACH_AS_BSS(wlc, bss_idx, icfg) {
				if (cfg == icfg)
					continue;
				from_wlc_as_count++;
			}

			/* Get a list cfgs on the to_wlc */
			FOREACH_AS_BSS(to_wlc, bss_idx, icfg) {
				to_wlc_as_count++;
			}
			/* If we are moving a bsscfg from wlc0
			* to wlc1 when there is no other active
			* assoc on wlc0 thats an error so TRAP
			*/
			if (!from_wlc_as_count) {
				printf("wl%d:%d ERROR moving from_wlc(as_cnt:%d)"
					"to_wlc(as_cnt:%d)\n", wlc->pub->unit,
					cfg->_idx,
					from_wlc_as_count, to_wlc_as_count);
			}
		}
		to_cfg = wlc_rsdb_bsscfg_clone(wlc, to_wlc, cfg, NULL);
		return to_cfg;
	}
	return cfg;
}

#ifdef SCB_MOVE
/*
*This functions allocates and sets up the scb parameters
* required to create a reassoc after bsscfg move
*/
static void
wlc_rsdb_scb_reassoc(wlc_info_t *to_wlc, wlc_bsscfg_t *to_cfg)
{
	/* Do a SCB reassoc only if the input cfg is an associated STA */
	if (WLC_BSS_ASSOC_NOT_ROAM(to_cfg)) {
		struct scb *to_scb;
		to_cfg->up = TRUE;

		/* TODO: Security parameters copy bettwen SCB */
		to_scb = wlc_scblookupband(to_wlc, to_cfg,
		&to_cfg->current_bss->BSSID, CHSPEC_BANDUNIT(to_cfg->current_bss->chanspec));

		/* Setting the state parameters for SCB */
		if (to_scb) {
			to_scb->state = AUTHENTICATED|ASSOCIATED;
			to_scb->flags |= SCB_MYAP;
		} else {
			WL_ERROR(("wl%d: %s: to_scb is not allocated"
				"Skipping Join Recreate", WLCWLUNIT(to_wlc), __FUNCTION__));
			return;
		}

		/* join re-creates assumes cfg is associated */
		wlc_sta_assoc_upd(to_cfg, TRUE);
		to_cfg->assoc->state = AS_IDLE;
		to_cfg->assoc->type = AS_RECREATE;
		wlc_join_recreate(to_wlc, to_cfg);
	}
}

/*
* This functions copies the cfg association parameters from
*  source bsscfg to destination bsscfg
* It copies current bss, target bss and chanspec variables
* from the source to destination bsscfg
*/

static void
wlc_rsdb_clone_assoc(wlc_bsscfg_t *from_cfg, wlc_bsscfg_t *to_cfg)
{
	struct scb *to_scb;
	struct scb *from_scb;

	/* Deep copy for associated */
	bcopy(from_cfg->target_bss, to_cfg->target_bss, sizeof(wlc_bss_info_t));
	to_cfg->target_bss->bcn_prb = NULL;
	to_cfg->target_bss->bcn_prb_len = 0;

	bcopy(from_cfg->current_bss, to_cfg->current_bss, sizeof(wlc_bss_info_t));
	to_cfg->current_bss->bcn_prb = NULL;
	to_cfg->current_bss->bcn_prb_len = 0;

	/* Lookup for scb which is connected */
	from_scb = wlc_scbfind(from_cfg->wlc, from_cfg,
			&from_cfg->current_bss->BSSID);

	if (from_scb == NULL) {
		WL_ERROR(("wl%d: %s Source SCB is NULL for clone\n",
				WLCWLUNIT(from_cfg->wlc), __FUNCTION__));
		return;
	}

	to_scb = wlc_scblookupband(to_cfg->wlc, to_cfg,
			&to_cfg->current_bss->BSSID,
			CHSPEC_BANDUNIT(to_cfg->current_bss->chanspec));

	if (to_scb == NULL) {
		WL_ERROR(("wl%d: %s Target SCB is NULL for clone\n", WLCWLUNIT(to_cfg->wlc),
				__FUNCTION__));
		return;
	}

	to_scb->flags = from_scb->flags;
	to_scb->flags2 = from_scb->flags2;
	to_scb->flags3 = from_scb->flags3;

	to_cfg->up = from_cfg->up;
	to_cfg->associated = from_cfg->associated;

	/* Copy Association ID */
	to_cfg->AID = from_cfg->AID;

	if (from_cfg->wsec) {
		int ret;

		to_scb->state = from_scb->state;
		to_scb->WPA_auth = from_scb->WPA_auth;

		if ((ret = wlc_keymgmt_clone_bsscfg(from_cfg->wlc->keymgmt,
				to_cfg->wlc->keymgmt, from_cfg, to_cfg)) == BCME_OK) {
			/* Update 'to_cfg->current_bss' parameters from 'from_cfg' .
			 * This copies rsn/wpa ies to 'to_cfg->current_bss'
			 */
			bcopy(from_cfg->current_bss, to_cfg->current_bss,
					sizeof(wlc_bss_info_t));
			to_cfg->current_bss->bcn_prb = MALLOC(to_cfg->wlc->osh,
					from_cfg->current_bss->bcn_prb_len);
			if (to_cfg->current_bss->bcn_prb) {
				bcopy(from_cfg->current_bss->bcn_prb,
						to_cfg->current_bss->bcn_prb,
						from_cfg->current_bss->bcn_prb_len);
				to_cfg->current_bss->bcn_prb_len =
					from_cfg->current_bss->bcn_prb_len;
			}
			to_cfg->wsec_portopen = from_cfg->wsec_portopen;
		}
		else if (ret == BCME_NOTFOUND) {
			WL_ERROR(("SCB not found\n"));
		}
	}

	/* Copy Association ID */
	to_cfg->AID = from_cfg->AID;
	to_cfg->flags |= WLC_BSSCFG_RSDB_CLONE;

	bcopy(from_cfg->BSSID.octet, to_cfg->BSSID.octet, sizeof(from_cfg->BSSID.octet));
	/* copy the RSSI value to avoid roam after move */
	to_cfg->roam->prev_rssi = from_cfg->roam->prev_rssi;
	/* During rsdb clone, verify timeout should happen immediately as we know
	 * that AP to which cfg is associated would not be thrown out as cloning is
	 * only internal to us.
	 */
	to_cfg->assoc->verify_timeout = 0;
}

void
wlc_rsdb_suppress_pending_tx_pkts(wlc_bsscfg_t *from_cfg)
{
	wlc_info_t *from_wlc = from_cfg->wlc;

	/* Do a SCB reassoc only if the input cfg is an associated STA */
	if ((WLC_BSS_ASSOC_NOT_ROAM(from_cfg) && !BSSCFG_IBSS(from_cfg)) ||
			(BSSCFG_SLOTTED_BSS(from_cfg))) {
		/* If there are pending packets on the fifo, then stop the fifo
		 * processing and re-enqueue packets
		 */
		wlc_sync_txfifo_all(from_wlc, from_cfg->wlcif->qi, SYNCFIFO);
#ifdef PROP_TXSTATUS
		if (PROP_TXSTATUS_ENAB(from_wlc->pub)) {
			wlc_suspend_mac_and_wait(from_wlc);
			wlc_wlfc_flush_pkts_to_host(from_wlc, from_cfg);
			wlc_enable_mac(from_wlc);
		}
#endif /* PROP_TXSTATUS */
	}
}
#endif /* SCB_MOVE */

/*
 * This function should be called only for AS_ROAM.
 * This function uses minimal set of code from wlc_sta_assoc_upd to
 * update the associated count on wlc1 as part of wlc_rsdb_bsscfg_clone().
*/
static void
wlc_rsdb_sta_assoc_upd(wlc_bsscfg_t *cfg, bool state)
{
	wlc_info_t *wlc = cfg->wlc;
	int idx;
	wlc_bsscfg_t *bc;

	cfg->associated = state;

	wlc->stas_associated = 0;
	wlc->ibss_bsscfgs = 0;
	FOREACH_AS_STA(wlc, idx, bc) {
	        wlc->stas_associated ++;
		if (!bc->BSS)
			wlc->ibss_bsscfgs ++;
	}

	wlc->pub->associated = (wlc->stas_associated != 0 || wlc->aps_associated != 0);

	wlc->stas_connected = wlc_stas_connected(wlc);

#if defined(PROP_TXSTATUS) && defined(WLMCHAN)
	if (MCHAN_ENAB(wlc->pub) && PROP_TXSTATUS_ENAB(wlc->pub)) {
		wlc_wlfc_mchan_interface_state_update(wlc, cfg,
			WLFC_CTL_TYPE_INTERFACE_OPEN, TRUE);
	}
#endif /* PROP_TXSTATUS && WLMCHAN */

	if (RSDB_ENAB(wlc->pub))
		wlc_rsdb_update_active(wlc, NULL);
}

static int
wlc_rsdb_cleanup_from_bsscfg(wlc_info_t *from_wlc, wlc_info_t *to_wlc, wlc_bsscfg_t *from_cfg,
	wlc_bsscfg_t* to_cfg, int idx, int8 unused_idx)
{
	bool primary_cfg_move = (wlc_bsscfg_primary(from_wlc) == from_cfg);

	/* Free/Reinit 'from_cfg'
	* In case of secondary config - Free 'from_cfg'
	* In case of primary config - deinit and then init 'from_cfg'
	*/
	if (!primary_cfg_move) {
		/* XXX Override the bsscfg[idx] with old config
		 * index before calling bsscfg_disable for old CFG.
		 * This is to ensure that the array access inside the function
		 * wlc_bsscfg_disble should give old cfg
		 */
		from_wlc->bsscfg[idx] = from_cfg;
		/* Disable the source config */
		wlc_bsscfg_disable(from_wlc, from_cfg);
		/* Free does deinit as well */
		wlc_bsscfg_free(from_wlc, from_cfg);
	}
	else {
		/* Disable the source config */
		wlc_bsscfg_disable(from_wlc, from_cfg);

		/* In case the cfg is associated and wlc is not associated
		* disable wont clear association flag.
		*/
		from_cfg->associated = FALSE;

		/* Clear the stale references of cfg if any */
		wlc_bsscfg_deinit(from_wlc, from_cfg);
		/* Update the cfg index and init the bsscfg */
		from_cfg->_idx = unused_idx;
		if (wlc_bsscfg_init(from_wlc, from_cfg) != BCME_OK) {
			WL_ERROR(("wl%d.%d: %s: cannot init bsscfg\n",
				from_wlc->pub->unit, unused_idx, __FUNCTION__));
			return BCME_ERROR;
		}
		wlc_rsdb_update_wlcif(to_wlc, from_cfg, to_cfg);
		to_wlc->bsscfg[from_cfg->_idx] = from_cfg;
	}
	return BCME_OK;
}

static void
wlc_rsdb_bsscfg_post_assoc_clone(wlc_info_t *from_wlc, wlc_info_t *to_wlc, wlc_bsscfg_t* to_cfg)
{
#ifdef SCB_MOVE
	/* When moving from wlc1->wlc0 check and power-down wlc1. This will prevent
	 * any possible configuraton of both the wlcs on same channel.
	 */
	if (WLC_BSS_ASSOC_NOT_ROAM(to_cfg) && !BSSCFG_AP(to_cfg)) {
		if ((from_wlc != WLC_RSDB_GET_PRIMARY_WLC(from_wlc)) && from_wlc->pub->up) {
			from_wlc->mpc_delay_off = 0;
			if (from_wlc->mpc) {
				wlc_radio_mpc_upd(from_wlc);
				wlc_radio_upd(from_wlc);
			} else {
				wl_down(from_wlc->wl);
			}
		}
		wlc_rsdb_scb_reassoc(to_wlc, to_cfg);
	}
#endif /* SCB_MOVE */
}

static void
wlc_rsdb_bsscfg_pre_assoc_clone(wlc_info_t *from_wlc, wlc_info_t *to_wlc, wlc_bsscfg_t *from_cfg)
{
	struct scb *from_scb;

	if (from_cfg->assoc->type != AS_ROAM) {
		/* Clone pending tx packets from 'from_cfg' to 'to_cfg' */
		wlc_rsdb_suppress_pending_tx_pkts(from_cfg);
	}

	/* Lookup for scb which is connected */
	from_scb = wlc_scbfind(from_cfg->wlc, from_cfg,
		&from_cfg->current_bss->BSSID);

	/* Free up the remaining packets in old wlc. */
	if (from_scb) {
		wlc_txq_scb_free(from_wlc, from_scb);
	}
}

static void
wlc_rsdb_bsscfg_post_clone(wlc_info_t *from_wlc, wlc_info_t *to_wlc, wlc_bsscfg_t* to_cfg)
{
	if (to_cfg->associated) {
		wlc_bsscfg_up(to_wlc, to_cfg);
		wlc_rsdb_sta_assoc_upd(to_cfg, TRUE);
	}
	/* since cfg->_idx could have changed because of clone delete and add multimac entry */
	wlc_bsscfg_delete_multmac_entry(to_cfg, &to_cfg->cur_etheraddr);
	wlc_bsscfg_upd_multmac_tbl(to_cfg, &to_cfg->cur_etheraddr,
			&to_cfg->BSSID);
	wlc_rsdb_cfg_clone_notif(from_wlc->rsdbinfo, to_cfg, from_wlc, to_wlc, CFG_CLONE_END);
	/* XXX sync h/w and s/w for old wlc
	* to avoid wake_ok assert.
	*/
	wlc_set_wake_ctrl(from_wlc);
	return;
}

static void
wlc_rsdb_bsscfg_pre_clone(wlc_info_t *from_wlc, wlc_info_t *to_wlc, wlc_bsscfg_t *from_cfg)
{
	/* Never clone Cfg 1 which is the inactive cfg. */
	if (from_cfg->_idx == RSDB_INACTIVE_CFG_IDX) {
#ifdef _RTE_
		HND_DIE();
#endif // endif
	}

	wlc_rsdb_cfg_clone_notif(from_wlc->rsdbinfo, from_cfg, from_wlc, to_wlc, CFG_CLONE_START);

#if defined(PROP_TXSTATUS) && defined(WLMCHAN)
	if (PROP_TXSTATUS_ENAB(from_wlc->pub) && from_cfg->associated) {
		wlc_wlfc_mchan_interface_state_update(from_wlc, from_cfg,
			WLFC_CTL_TYPE_INTERFACE_CLOSE, FALSE);
	}
#endif // endif

#ifdef SCB_MOVE
	if (WLC_BSS_ASSOC_NOT_ROAM(from_cfg) && !BSSCFG_AP(from_cfg)) {
		if (ANY_SCAN_IN_PROGRESS(from_wlc->scan)) {
			/* Abort any SCAN in progress */
			printf("wl%d: Clone during SCAN...Aborting SCAN flag %d usage = %d\n",
			WLCWLUNIT(from_wlc),
			from_wlc->scan->flag,
			from_wlc->scan->wlc_scan_cmn->usage);
			wlc_scan_abort(from_wlc->scan, WLC_E_STATUS_ABORT);
		}
		/* Say PM1 to AP */
		mboolset(from_cfg->pm->PMblocked, WLC_PM_BLOCK_MODESW);
		if (from_cfg->pm->PMenabled != TRUE) {
			wlc_set_pmstate(from_cfg, TRUE);
		}
	}
#endif /* SCB_MOVE */
	/* Make to_wlc chain up from down (due to mpc) */
	to_wlc->mpc_join = TRUE;
	wlc_radio_mpc_upd(to_wlc);
	if (!to_wlc->pub->up) {
		/* Try to bring UP again.
		* May be we did force down
		* during mode switch.
		*/
		printf("wl%d: radio_disabled %x radio_monitor %d delay_off = %d"
			"last_radio_disabled = %d\n", to_wlc->pub->unit,
			to_wlc->pub->radio_disabled, to_wlc->radio_monitor,
			to_wlc->mpc_delay_off, to_wlc->pub->last_radio_disabled);
		wlc_radio_upd(to_wlc);
	}
	to_wlc->mpc_join = FALSE;
	if (!to_wlc->pub->up) {
		WL_ERROR(("wl%d: ERROR: Target core is NOT up\n", WLCWLUNIT(to_wlc)));
	}

	/* TODO: What about the data packets in TXQ during cfg move? */
	/* process event queue */
	wlc_eventq_flush(from_wlc->eventq);
	return;
}

static wlc_bsscfg_t *
wlc_rsdb_allocate_clone_bsscfg(wlc_info_t *from_wlc, wlc_info_t *to_wlc, wlc_bsscfg_t *from_cfg,
	int8 *out_inactive_idx)
{
	bool primary_cfg_move = (wlc_bsscfg_primary(from_wlc) == from_cfg);
	int8 unused_idx = -1;
	wlc_bsscfg_t *new_cfg = NULL;
	int alloc_idx = from_cfg->_idx;
	/* set up 'to_cfg'
	* in case of secondary move - allocate a new bsscfg structure for 'to_cfg'
	* incase of primary move- set the primary bsscfg of 'to_wlc' to 'to_cfg'
	*/
	if (!primary_cfg_move) {
		wlc_bsscfg_type_t type;
		wlc_bsscfg_type_get(from_wlc, from_cfg, &type);
		new_cfg = wlc_bsscfg_alloc(to_wlc, alloc_idx, &type,
			(from_cfg->flags | WLC_BSSCFG_RSDB_CLONE),
			from_cfg->flags2, &(from_cfg->cur_etheraddr));
		if (new_cfg == NULL) {
			WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
				from_wlc->pub->unit, __FUNCTION__, MALLOCED(from_wlc->osh)));
			return NULL;
		}
	}
	else {
		/* Movement is for primary cfg.. Swap _idx */
		new_cfg = wlc_bsscfg_primary(to_wlc);
		ASSERT(new_cfg);
		wlc_bsscfg_deinit(to_wlc, new_cfg);
		ASSERT(from_cfg->_idx != new_cfg->_idx);
		unused_idx = new_cfg->_idx;
		/* Update the cfg index and init the bsscfg */
		new_cfg->_idx = alloc_idx;
		/* Add CLONE flag to skip the OS interface free */
		new_cfg->flags |= WLC_BSSCFG_RSDB_CLONE;
	}

	/* Copy all the flags from from_cfg to ensure
	* flags dependent bsscfg init happens
	* correctly on to_cfg
	*/
	new_cfg->flags = from_cfg->flags;
	new_cfg->flags2 = from_cfg->flags2;

	/* Initialize the bsscfg */
	new_cfg->_ap = from_cfg->_ap;
	if (wlc_bsscfg_init(to_wlc, new_cfg) != BCME_OK) {
		WL_ERROR(("wl%d.%d: %s: cannot init bsscfg\n",
			to_wlc->pub->unit, alloc_idx, __FUNCTION__));
		return NULL;
	}
	*out_inactive_idx = unused_idx;
	return new_cfg;
}

wlc_bsscfg_t*
wlc_rsdb_bsscfg_clone(wlc_info_t *from_wlc, wlc_info_t *to_wlc, wlc_bsscfg_t *from_cfg, int *ret)
{
	int err = BCME_OK;
	int active_idx;
	wlc_bsscfg_t *to_cfg = NULL;
	int8 inactive_idx = -1;
	bool primary_cfg_move;
	bool block_disassoc_tx = 0;

	ASSERT(from_cfg);
	active_idx = from_cfg->_idx;
	primary_cfg_move = (wlc_bsscfg_primary(from_wlc) == from_cfg);

	WL_ERROR(("RSDB clone %s cfg[%d][%p] from wlc[%d] to wlc[%d] AS = %d CONN = %d"
		" stas/ap/pub_assoc %d/%d/%d\n", primary_cfg_move ?"primary":"virtual",
		active_idx, from_cfg, UNIT(from_wlc), UNIT(to_wlc),
		WLC_BSS_ASSOC_NOT_ROAM(from_cfg), WLC_BSS_CONNECTED(from_cfg),
		from_wlc->stas_associated, from_wlc->aps_associated, from_wlc->pub->associated));

	wlc_rsdb_bsscfg_pre_clone(from_wlc, to_wlc, from_cfg);

	if (BSSCFG_STA(from_cfg) && from_cfg->assoc != NULL) {
		block_disassoc_tx = from_cfg->assoc->block_disassoc_tx;
	}

	if ((to_cfg = wlc_rsdb_allocate_clone_bsscfg(from_wlc, to_wlc,
		from_cfg, &inactive_idx)) == NULL) {
		*ret = BCME_ERROR;
		return NULL;
	}

	/* transfer the cubby data of cfg to clonecfg */
	if ((err = wlc_bsscfg_configure_from_bsscfg(from_cfg, to_cfg)) != BCME_OK) {
		*ret = BCME_ERROR;
		return NULL;
	}

	if (!primary_cfg_move) {
		ASSERT(to_cfg->wlc == to_wlc);
		ASSERT(to_wlc->cfg != to_cfg);

		/* Fixup per port layer references, ONLY if we are OS interface */
		if (!BSSCFG_HAS_NOIF(from_cfg)) {
			WLRSDB_DBG(("RSDB clone: move WLCIF from wlc[%d] to wlc[%d]\n",
			from_wlc->pub->unit, to_wlc->pub->unit));
			wlc_rsdb_update_wlcif(to_wlc, from_cfg, to_cfg);
		}

		/* XXX FIXME: a hack to let disable pass thru
		 * FIXME: Store back the from_cfg in to the allocated location index
		*/
		to_wlc->bsscfg[active_idx] = from_cfg;
	}
	else {
		from_cfg->flags2 |= WLC_BSSCFG_FL2_RSDB_NOT_ACTIVE;
	}

	if (BSSCFG_STA(from_cfg) && (from_cfg->assoc != NULL)) {
		from_cfg->assoc->state = AS_IDLE;
		from_cfg->assoc->block_disassoc_tx = TRUE;
	}

#ifdef SCB_MOVE
	if (WLC_BSS_CONNECTED(from_cfg) && !BSSCFG_AP(from_cfg)) {
		wlc_rsdb_bsscfg_pre_assoc_clone(from_wlc, to_wlc, from_cfg);
		/* Clone the assoc information from source bsscfg to destination bsscfg */
		wlc_rsdb_clone_assoc(from_cfg, to_cfg);
	} else if (BSSCFG_AP(from_cfg)) {
		wlc_keymgmt_clone_bsscfg(from_cfg->wlc->keymgmt, to_cfg->wlc->keymgmt,
			from_cfg, to_cfg);
		wlc_rsdb_scb_clone(from_cfg, to_cfg);
	}
#endif /* SCB_MOVE */

	if (wlc_rsdb_cleanup_from_bsscfg(from_wlc, to_wlc,
		from_cfg, to_cfg, active_idx, inactive_idx) != BCME_OK) {
		return NULL;
	}

	/* wlc->bsscfg[] array is shared. Update bsscfg array. */
	to_wlc->bsscfg[to_cfg->_idx] = to_cfg;

	/* If an SCB was associated to an AP in the cloned cfg recreate the association
	 *	TODO:IBSS CFG scb move, primary bsscfg scb move
	 */
	wlc_rsdb_bsscfg_post_assoc_clone(from_wlc, to_wlc, to_cfg);

	if (primary_cfg_move)
		from_cfg->flags &= ~WLC_BSSCFG_RSDB_CLONE;

	if (!WLC_BSS_ASSOC_NOT_ROAM(to_cfg))
		to_cfg->flags &= ~WLC_BSSCFG_RSDB_CLONE;

	ASSERT(to_wlc->bsscfg[active_idx] == from_wlc->bsscfg[active_idx]);

	if (ret)
		*ret = err;

	wlc_rsdb_bsscfg_post_clone(from_wlc, to_wlc, to_cfg);

	/* Restore the value of block_disassoc_tx */
	if ((from_cfg->assoc != NULL) && primary_cfg_move && BSSCFG_STA(from_cfg)) {
		from_cfg->assoc->block_disassoc_tx = block_disassoc_tx;
	}

	if (BSSCFG_STA(to_cfg) && (to_cfg->assoc != NULL)) {
		to_cfg->assoc->block_disassoc_tx = block_disassoc_tx;
	}

	return to_cfg;
}

static int
wlc_rsdb_scb_update(wlc_info_t *wlc_from, wlc_info_t *wlc_to, wlc_bsscfg_t *cfg_to,
	struct scb *scb)
{
	uint8 active_fid[TXMOD_LAST];
	ASSERT(wlc_to == cfg_to->wlc);

	/* XXX: This action only need for non-adhoc type
	 * XXX: Slotted bss need to keep the AMPDU session
	 */
	if (!BSSCFG_SLOTTED_BSS(cfg_to)) {
		wlc_rsdb_link_teardown(wlc_from, scb, active_fid);
	}

#if defined(PKTC) || defined(PKTC_DONGLE)
	/* Ensure we invalidate RFC before moving the SCB to new WLC */
	if (SCB_PKTC_ENABLED(scb)) {
		wlc_scb_pktc_disable(wlc_from->pktc_info, scb);
		SCB_PKTC_ENABLE(scb);
	}
#endif /* defined(PKTC) || defined(PKTC_DONGLE) */

	wlc_scb_update_multmac_idx(cfg_to, scb);
	/* update wlc and bsscfg corresponding to the new wlc */
	wlc_scb_wlc_cfg_update(wlc_from, wlc_to, cfg_to, scb);

	if (!BSSCFG_SLOTTED_BSS(cfg_to)) {
		wlc_rsdb_link_rebind(wlc_to, scb, active_fid);
	}
	return BCME_OK;
}

static void
wlc_rsdb_link_teardown(wlc_info_t *wlc, struct scb *scb,
	uint8 *activefid)
{
	txmod_id_t fid;

#if defined(WL_BEAMFORMING)
	if (TXBF_ENAB(wlc->pub)) {
		wlc_txbf_delete_link(wlc->txbf, scb);
	}
#endif /* defined(WL_BEAMFORMING) */

	/* Tearing down the AMPDU session is pending.
	* Not sure if the client will be present after CSA
	* to receive the session teardown.
	*/

	/* Unconfig the txmod functions on from wlc.
	* XXX: Do we need to use txmodi->txmod_last?
	*/
	for (fid = TXMOD_START; fid < TXMOD_LAST; fid++) {
		if (SCB_TXMOD_ACTIVE(scb, fid)) {
			*(activefid + fid) = TRUE;
			wlc_txmod_unconfig(wlc->txmodi, scb, fid);
		}
	}
}

static void
wlc_rsdb_link_rebind(wlc_info_t *wlc, struct scb *scb,
	uint8 *activefid)
{
	txmod_id_t fid;

	if (!VHT_ENAB_BAND(wlc->pub, wlc->band->bandtype)) {
		wlc_vht_update_scb_state(wlc->vhti, wlc->band->bandtype,
			scb, NULL, NULL, NULL, 0);
	}

	wlc_scb_ratesel_init(wlc, scb);

	/* invalidate txc */
	if (WLC_TXC_ENAB(wlc))
		wlc_txc_inv(wlc->txc, scb);

	/* Config the txmod functions on to wlc */

	for (fid = TXMOD_START; fid < TXMOD_LAST; fid++) {
		if (*(activefid + fid) == TRUE) {
			wlc_txmod_config(wlc->txmodi, scb, fid);
		}
	}

#if defined(WL_BEAMFORMING)
	if (TXBF_ENAB(wlc->pub)) {
		uint32 cap_info = 0;
		wlc_txbf_scb_state_upd(wlc->txbf, scb, (uint8 *) &cap_info, sizeof(cap_info),
			TXBF_CAP_TYPE_VHT);
		wlc_txbf_init_link(wlc->txbf, scb);
	}
#endif /* defined(WL_BEAMFORMING) */
}

int
wlc_rsdb_scb_clone(wlc_bsscfg_t * from_cfg, wlc_bsscfg_t * to_cfg)
{
	struct scb *scb;
	struct scb_iter scbiter;
	int ret = BCME_OK;
	bool suppress = TRUE;

	/* Get all the scbs which needs to be moved to other wlc */
	FOREACH_BSS_SCB(from_cfg->wlc->scbstate, &scbiter, from_cfg, scb) {
		ret = wlc_rsdb_move_scb(from_cfg, to_cfg, scb, suppress);
		if (ret != BCME_OK) {
			WL_ERROR(("%s failed err=%d\n", __FUNCTION__, ret));
			break;
		}
		suppress = FALSE;
	}
	return ret;
}

/* Call this function to move scb from from_cfg to to_cfg.
 * This also suppresses the pending tx pkts on from_cfg.
 *
 * Typically called in slotted bss functionality where scb need to be moved
 * from one bandunit to another.
 */
int
wlc_rsdb_move_scb(wlc_bsscfg_t * from_cfg, wlc_bsscfg_t * to_cfg, struct scb *scb, bool suppress)
{
	int ret = BCME_OK;

	/* Flush packets in txq, scb ampduq */
	if (suppress) {
		wlc_rsdb_suppress_pending_tx_pkts(from_cfg);
	}

	/* Get all the scbs which needs to be moved to other wlc */
	/* Assuming we have one bsscfg tried to a wlcx, do not switch *
	 * scb's wlc and other info
	 */
	if (scb->bsscfg == to_cfg) {
		return ret;
	}

	/* Free up the remaining packets in old wlc. */
	wlc_txq_scb_free(from_cfg->wlc, scb);

	ret = wlc_rsdb_scb_update(from_cfg->wlc, to_cfg->wlc, to_cfg, scb);

	if (ret != BCME_OK) {
		WL_ERROR(("%s failed err=%d\n", __FUNCTION__, ret));
	}
	return ret;
}

/* Call this function to update the CFG's wlcif reference
 * Typically called after JOIN-success or AP-up
*/
void wlc_rsdb_update_wlcif(wlc_info_t *wlc, wlc_bsscfg_t *from_cfg, wlc_bsscfg_t *to_cfg)
{
	wlc_info_t *from_wlc = from_cfg->wlc;
	wlc_info_t *to_wlc = to_cfg->wlc;
	int primary_cfg_move = FALSE;
	wlc_if_t * from_wlcif = from_cfg->wlcif;
	wlc_if_t * to_wlcif = to_cfg->wlcif;

	if (from_wlc == to_wlc) {
		WLRSDB_DBG(("%s: already updated \n", __FUNCTION__));
		return;
	}
	primary_cfg_move = (wlc_bsscfg_primary(from_wlc) == from_cfg);

	ASSERT(from_wlcif->type == WLC_IFTYPE_BSS);
	WLRSDB_DBG(("%s:wl%d updating wlcif[%d] cfg/idx(%p/%d->%p/%d) from wl%d to wl%d\n",
		__FUNCTION__, wlc->pub->unit, from_wlcif->index,
		OSL_OBFUSCATE_BUF(from_cfg), WLC_BSSCFG_IDX(from_cfg), OSL_OBFUSCATE_BUF(to_cfg),
		WLC_BSSCFG_IDX(to_cfg), from_wlc->pub->unit, to_wlc->pub->unit));

	if (primary_cfg_move) {
		uint8 tmp_idx;
		tmp_idx = from_wlcif->index;
		from_wlcif->index = to_wlcif->index;
		to_wlcif->index = tmp_idx;
		memcpy(to_wlcif->_cnt, from_wlcif->_cnt, sizeof(wl_if_stats_t));
	}
	/* update host interface reference for wlcif */
	wl_update_if(from_wlc->wl, to_wlc->wl, from_wlcif->wlif, to_wlcif);
	to_cfg->wlcif->if_flags = from_cfg->wlcif->if_flags;
	if (primary_cfg_move) {
		/* New cfg uses the existing host interface */
		to_wlcif->wlif = from_wlcif->wlif;
		/* We can nolonger reference the wlif, in the old cfg  */
		from_wlcif->wlif = NULL;
	}

}

/* Call this function to update rsdb_active status
* currently called from sta_assoc_upd() and ap_up_upd()
*/
bool
wlc_rsdb_update_active(wlc_info_t *wlc, bool *old_state)
{
	uint8 active = 0;
	int idx;
	wlc_cmn_info_t *wlc_cmn = wlc->cmn;
	wlc_info_t *wlc_iter;
	int aps_active = 0;

	if (old_state)
		*old_state = wlc->pub->cmn->_rsdb_active;

	wlc->pub->cmn->_rsdb_active = FALSE;

	FOREACH_WLC(wlc_cmn, idx, wlc_iter) {
		if (wlc_iter && wlc_iter->pub && wlc_iter->pub->associated) {
			active++;
			aps_active += wlc_iter->aps_associated;
		}
	}
	if (active > 1)
		wlc->pub->cmn->_rsdb_active = TRUE;

#ifdef BCMRESVFRAGPOOL
	if (BCMRESVFRAGPOOL_ENAB()) {
		if (aps_active) {
			hnd_resv_pool_disable(wlc->pub->resv_pool_info);
			/*
			 * When resv pool is disabled, less no. of tx buffers will be available,
			 * which requires lesser ampdu_mpdu value. Reduce ampdu_mpdu to rpool
			 * disabled ampdu_mpdu value. BUT before doing so,
			 * save the def_ampdu_mpdu in rsdb cmn info structure to update later.
			 */
			wlc_iovar_getint(wlc, "ampdu_mpdu",
					(int *)&wlc->rsdbinfo->cmn->def_ampdu_mpdu);
			wlc_iovar_setint(wlc, "ampdu_mpdu",
				wlc->pub->tunables->rpool_disabled_ampdu_mpdu);
		} else {
			hnd_resv_pool_enable(wlc->pub->resv_pool_info);
			/* if STA(non-AP) is baught up first, then query def_ampdu_mpdu & set it */
			if (wlc->rsdbinfo->cmn->def_ampdu_mpdu == RSDB_AMPDU_MPDU_INIT_VAL)
				wlc_iovar_getint(wlc, "ampdu_mpdu",
					(int *)&wlc->rsdbinfo->cmn->def_ampdu_mpdu);
			/* set back the def ampdu mpdu */
			wlc_iovar_setint(wlc, "ampdu_mpdu",
					(int)wlc->rsdbinfo->cmn->def_ampdu_mpdu);
		}
		/* As we added / deleted pools, inform bus layer of the same */
		wl_busioctl(wlc->cmn->wlc[0]->wl, BUS_UPDATE_FLOW_PKTS_MAX,
				NULL, 0, NULL, NULL, FALSE);
	}
#endif /* BCMRESVFRAGPOOL */

#ifdef WLP2P
	/* If a P2P-GO/GC interface has been created, then move the discovery cfg to
	 * P2P-GO/GC`s WLC
	 */
	if (P2P_ENAB(wlc->pub)) {
		(void)wlc_p2p_rsdb_disc_config(wlc->p2p);
	}
#endif /* WLP2P */

	return wlc->pub->cmn->_rsdb_active;
}

int
wlc_rsdb_join_prep_wlc(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint8 *SSID, int len,
	wl_join_scan_params_t *scan_params,
	wl_join_assoc_params_t *assoc_params, int assoc_params_len)
{
	wlc_assoc_t *as = bsscfg->assoc;
	int err = BCME_OK;

	WLRSDB_DBG(("wl%d.%d %s Join Prep on SSID %s\n", wlc->pub->unit, bsscfg->_idx,
		__FUNCTION__, (char *)SSID));

	ASSERT(bsscfg != NULL);
	ASSERT(BSSCFG_STA(bsscfg));
	ASSERT(wlc->pub->corerev >= 15);
	BCM_REFERENCE(as);

	wlc_set_mac(bsscfg);

#ifdef WLRM
	if (wlc_rminprog(wlc)) {
		WL_INFORM(("wl%d: association request while radio "
				   "measurement is in progress, aborting measurement\n",
				   wlc->pub->unit));
		wlc_rm_stop(wlc);
	}
#endif /* WLRM */

#ifdef WL11K
		if (wlc_rrm_inprog(wlc)) {
			WL_INFORM(("wl%d: association request while radio "
				   "measurement is in progress, aborting RRM\n",
				   wlc->pub->unit));
			wlc_rrm_stop(wlc);
		}
#endif /* WL11K */

	wlc_set_wake_ctrl(wlc);

	/* save SSID and assoc params for later use in a different context and retry */
	wlc_bsscfg_SSID_set(bsscfg, SSID, len);
	wlc_bsscfg_assoc_params_set(wlc, bsscfg, assoc_params, assoc_params_len);

	memcpy(bsscfg->target_bss, wlc->default_bss, sizeof(*bsscfg->target_bss));
	bsscfg->target_bss->SSID_len = bsscfg->SSID_len;
	if (bsscfg->SSID_len > 0)
		memcpy(bsscfg->target_bss->SSID, bsscfg->SSID, bsscfg->SSID_len);
	if ((err = wlc_bsscfg_enable(wlc, bsscfg)) != BCME_OK) {
		WLRSDB_DBG(("wl%d.%d: Cannot Enable cfg\n", UNIT(wlc), bsscfg->_idx));
		return err;
	}
	WLRSDB_DBG(("wl%d.%d %s Join Prep Done\n", UNIT(wlc), bsscfg->_idx, SSID));

	return err;
}

uint16
wlc_rsdb_mode(void *hdl)
{
	wlc_info_t *wlc = (wlc_info_t *)hdl;

	if (WLC_DUALMAC_RSDB(wlc->cmn)) {
		return PHYMODE_MIMO;
	}

	ASSERT(WLC_RSDB_CURR_MODE(wlc) < WLC_RSDB_MODE_MAX);

	return (uint16)SWMODE2PHYMODE(WLC_RSDB_CURR_MODE(wlc));
}

#if !defined(WL_DUALNIC_RSDB) && !defined(WL_DUALMAC_RSDB)
void
wlc_rsdb_set_phymode(void *hdl, uint32 phymode)
{
	wlc_info_t *wlc = (wlc_info_t *)hdl;
	wlc_pub_t	*pub;
	uint sicoreunit;
	uint32 backup_mask;

	if (!wlc || !wlc->pub)
		return;

	pub = wlc->pub;
	sicoreunit = si_coreunit(pub->sih);
	WL_TRACE(("wl%d: %s\n", pub->unit, __FUNCTION__));
	ASSERT(wlc_bmac_rsdb_cap(wlc->hw));
	/* PHY Mode has to be written only in Core 0 cflags.
	 * For Core 1 override, switch to core-0 and write it.
	 */
	phymode = phymode << SICF_PHYMODE_SHIFT;

	/* Turn off interrupts before switching core */
	backup_mask = wlc_intrsoff(wlc);
	si_d11_switch_addrbase(pub->sih, MAC_CORE_UNIT_0);
	/* Set up the PHY MODE for RSDB PHY 4349 */
	si_core_cflags(pub->sih, SICF_PHYMODE, phymode);
	si_d11_switch_addrbase(pub->sih, sicoreunit);
	wlc_intrsrestore(wlc, backup_mask);
	OSL_DELAY(10);
}

void
wlc_rsdb_bmc_smac_template(void *hdl, int tplbuf, uint32 doublebufsize)
{
	wlc_info_t *wlc = (wlc_info_t *)hdl;
	osl_t *osh;
	int i;
	uint sicoreunit;
	wlc_pub_t	*pub;
	d11regs_t *sregs;

	if (!wlc || !wlc->pub)
		return;

	pub = wlc->pub;
	WL_TRACE(("wl%d: %s\n", pub->unit, __FUNCTION__));
	ASSERT(wlc_bmac_rsdb_cap(wlc->hw));

	/* 4349 Template init for Core 1. Just duplicate for now.
	 * Make the code a proc later and call with different base
	 */
	osh = (osl_t *)wlc->osh;
	sicoreunit = si_coreunit(pub->sih);

	sregs = si_d11_switch_addrbase(pub->sih, MAC_CORE_UNIT_1);
	BCM_REFERENCE(sregs);
	OR_REG(osh, D11_MACCONTROL_ALTBASE(sregs, wlc->regoffsets), MCTL_IHR_EN);
	/* init template */
	for (i = 0; i < tplbuf; i ++) {
		int end_idx;
		end_idx = tplbuf + i + 2 + doublebufsize;
		if (end_idx >= 2 * tplbuf)
			end_idx = (2 * tplbuf) - 1;
		W_REG(osh, D11_MSDUEntryStartIdx_ALTBASE(sregs, wlc->regoffsets),
		    (uint16)(tplbuf + i));
		W_REG(osh, D11_MSDUEntryEndIdx_ALTBASE(sregs, wlc->regoffsets), (uint16)end_idx);
		W_REG(osh, D11_MSDUEntryBufCnt_ALTBASE(sregs, wlc->regoffsets),
		    (uint16)(end_idx - (tplbuf + i) + 1));
		W_REG(osh, D11_PsmMSDUAccess_ALTBASE(sregs, wlc->regoffsets),
		    (uint16)((1 << PsmMSDUAccess_WriteBusy_SHIFT) |
		    (i << PsmMSDUAccess_MSDUIdx_SHIFT) |
		    (D11MAC_BMC_TPL_IDX(wlc->pub->corerev) << PsmMSDUAccess_TIDSel_SHIFT)));

		SPINWAIT((R_REG(pub->osh, D11_PsmMSDUAccess_ALTBASE(sregs, wlc->regoffsets)) &
			(1 << PsmMSDUAccess_WriteBusy_SHIFT)), 200);
		if (R_REG(pub->osh, D11_PsmMSDUAccess_ALTBASE(sregs, wlc->regoffsets)) &
		    (1 << PsmMSDUAccess_WriteBusy_SHIFT))
			{
				WL_ERROR(("wl%d: PSM MSDU init not done yet :-(\n",
				(pub->unit + 1)));
			}
	}

	si_d11_switch_addrbase(pub->sih, sicoreunit);
}
#endif /* WL_DUALNIC_RSDB */

/* This function evaluates if current iovar needs to be applied on both
* cores or single core during dongle-MFGTEST & dualNIC DVT scenario
*/
bool BCMRAMFN(wlc_rsdb_chkiovar)(wlc_info_t *wlc,
	const bcm_iovar_t *vi_ptr, uint32 actid, int32 wlc_indx)
{
	bool result = FALSE;
#ifndef WLRSDB_DVT
	result = (((vi_ptr->flags) & IOVF_RSDB_SET) && (IOV_ISSET(actid)) && (wlc_indx < 0));
#else
	if (IOV_ISSET(actid) && WLC_RSDB_DUAL_MAC_MODE(WLC_RSDB_CURR_MODE(wlc))) {
		int i, size;
		size = ARRAYSIZE(rsdb_gbl_iovars);
		for (i = 0; i < size; i++) {
			if (!strcmp(vi_ptr->name, rsdb_gbl_iovars[i])) {
				result = TRUE;
				break;
			}
		}
	}
#endif	/* !WLRSDB_DVT */
	return result;
}

/* This function evaluates if current iovar needs to be applied on linked bsscfgs
 * on both cores or single core
 */
struct wlc_if *
BCMRAMFN(wlc_rsdb_linked_cfg_chkiovar)(wlc_info_t *wlc,
	const bcm_iovar_t *vi_ptr, uint32 actid, int32 wlc_indx, struct wlc_if *wlcif)
{
#ifdef WLSLOTTED_BSS
	if (((vi_ptr->flags2) & IOVF2_RSDB_LINKED_CFG) && (IOV_ISSET(actid)) &&
			(wlc_indx < 0) && (wlcif->type == WLC_IFTYPE_BSS) &&
			IS_BSSCFG_LINKED(wlcif->u.bsscfg)) {
		wlc_bsscfg_t *linked_cfg;

		linked_cfg = wlc_rsdb_bsscfg_get_linked_cfg(wlc->rsdbinfo,
				wlcif->u.bsscfg);

		if (linked_cfg && linked_cfg->wlcif &&
				(linked_cfg->wlcif->type == WLC_IFTYPE_BSS)) {
			return linked_cfg->wlcif;
		}
	}
#endif /* WLSLOTTED_BSS */
	return NULL;
}

static int
wlc_rsdb_update_hw_mode(wlc_info_t *from_wlc, uint32 to_mode)
{
	wlc_info_t *wlc = from_wlc;
	int isscore = 0;
	bool err = 0;

	if (wlc_bmac_rsdb_cap(from_wlc->hw) == FALSE) {
		return BCME_OK;
	}

	isscore = si_coreunit(from_wlc->pub->sih);

	if (isscore == MAC_CORE_UNIT_1) {
		si_d11_switch_addrbase(wlc->pub->sih,  MAC_CORE_UNIT_0);
		wlc = wlc_rsdb_get_other_wlc(from_wlc);
	}

	if (si_iscoreup(wlc->pub->sih) == FALSE) {
		return BCME_NOTUP;
	}

	/* Confirm that below happens for Core 1. Currently
	* dma Rx clean happens in wl_down().
	*/
	err = wlc_bmac_reset_txrx(wlc->hw);

	err = wlc_bmac_bmc_dyn_reinit(wlc->hw, FALSE);

	/* Switch OFF radio in current mode */
	phy_radio_switch((phy_info_t *)WLC_PI(wlc), OFF);
	wlc_rsdb_set_phymode(wlc, to_mode);

	wlc_bmac_phy_reset(wlc->hw);

	if (isscore == MAC_CORE_UNIT_1) {
		si_d11_switch_addrbase(wlc->pub->sih,  MAC_CORE_UNIT_1);
	}

	return err;
}

/* Core disable callback. This is full down. wl up should happen always in RSDB mode.
 * So, force the mode to rsdb if device is in auto mode.
 * XXX We see PSM watchdog if full up happens in MIMO mode. So, put the device in
 * RSDB mode.
 */
void
wlc_rsdb_force_rsdb_mode(wlc_info_t *wlc)
{
	wlc_cmn_info_t *wlc_cmn;

	wlc_cmn = wlc->cmn;
	if (wlc_cmn->rsdb_mode & WLC_RSDB_MODE_AUTO_MASK) {
		wlc_rsdb_change_mode(wlc, WLC_RSDB_MODE_RSDB);
		wlc_rsdb_change_mode(wlc, WLC_RSDB_MODE_AUTO);
	}
}

/* This function evaluates other chain is idle or not for MPC.
* return true
*	rsdb not enabled.
*	or rsdb enabled and other chain is idle.
*    or for chain 0 during reinit call
*return false
*	rsdb enabled and other chain is not idle.
*/
bool wlc_rsdb_is_other_chain_idle(void *hdl)
{
	wlc_info_t *wlc, *other_wlc;
	if (!hdl) {
		return TRUE;
	} else {
		wlc = (wlc_info_t *)hdl;
	}

	if (wlc->pub == NULL) {
		return TRUE;
	}

#ifdef WL_DUALNIC_RSDB
	return TRUE;
#endif // endif

	other_wlc = wlc_rsdb_get_other_wlc(wlc);

	if (!wlc->cmn->hwreinit) {
		if ((RSDB_ENAB(wlc->pub) == FALSE) || (other_wlc == NULL)) {
			return TRUE;
		}
		if ((other_wlc != NULL) && (other_wlc->pub->up)) {
			return FALSE;
		} else {
			return TRUE;
		}
	} else {
		/* chip reset is from core 0 only when all other chains are idle */
		/* During reinit it is not really the case so the override is required */
		return TRUE;
	}
}

/* This functions evaluates if it is case of RSDB initialization and returns
	TRUE: if RSDB initialisation is identified and performed
	FALSE: if it was MIMO image
*/

bool
wlc_rsdb_reinit(wlc_info_t *wlc)
{
	uint8 idx;
	wlc_info_t* wlc_iter;

	if (!RSDB_ENAB(wlc->pub)) {
		/* MIMO image */
		return FALSE;
	}

	/* reinit on particular core is enough for dual mac rsdb chips like 4347/4364 */
	if (WLC_DUALMAC_RSDB(wlc->cmn)) {
		wl_init(wlc->wl);
		return TRUE;
	}

	/* check curent RSDB mode */
	/* chip reset is triggered from core 0 only if all other chains are down */
	if (WLC_RSDB_CURR_MODE(wlc) == WLC_RSDB_MODE_RSDB) {
		FOREACH_WLC(wlc->cmn, idx, wlc_iter) {
			/* do soft reset on the chain that is up
			* This is followed by doing the big hammer
			* to handle the hardwae initialization later
			*/
			if (wlc_iter->pub->up) {
				wlc_bmac_set_noreset(wlc_iter->hw, TRUE);
				wl_reset(wlc_iter->wl);
				wlc_bmac_set_noreset(wlc_iter->hw, FALSE);
			}
		}
		FOREACH_WLC(wlc->cmn, idx, wlc_iter) {
			/* overriding condition to allow chip reset from wlc 0 */
			wlc_iter->cmn->hwreinit = ((idx == 0) ? TRUE : FALSE);
			/* do reinit only on the chain that is UP */
			/* else hw&sw states mismatch happens */
			/* incase of forced rsdb mode with single association */
			if (wlc_iter->pub->up)
				wl_init(wlc_iter->wl);
		}
		return TRUE;
	} else {
		/* mimo mode */
		wl_init(wlc->wl);
		return TRUE;
	}
}
/* Check if connection can be moved to 3x3 using load thresh */
/* if user has overriden , skip watchdog */
/* if dynamic switch disabled, go through monitor module for upgrade/downgrade */
static void
wlc_rsdb_dualmac_watchdog(wlc_info_t *wlc)
{
	/* if override cmd is active, skip watchdog */
	if (mboolisset(wlc->cmn->rsdb_move_ovr, WLC_RSDB_MOVE_FORCE_IOVAR)) {
		return;
	} else
#if defined(WLCHANIM)
	/* Dual mac modeswitch overrides other dynamic algorithms */
	if (WLC_RSDB_PM_MODESW_ENAB(wlc->pub) && !RSDB_ACTIVE(wlc->pub) &&
		(!wlc->cmn->rsdb_move_ovr)) {
		/* check for upgrade if we are secondary wlc */
		if (wlc != WLC_RSDB_GET_PRIMARY_WLC(wlc)) {
			wlc_rsdb_check_load_based_upgarde(wlc->rsdbinfo);
		}
	} else
#endif // endif
	{
		/* dyn switch not enabled, enter peer cap monitor mode */
		wlc_dualmac_rsdb_monitor(wlc);
	}
	return;

}
static void
wlc_rsdb_watchdog(void *context)
{
	wlc_rsdb_info_t* rsdbinfo = (wlc_rsdb_info_t *)context;
	wlc_info_t *wlc = rsdbinfo->wlc;

	/* Don't allow upgrade if band locked */
	if (wlc->bandlocked) {
		return;
	}

	/* Check for RSDB PM upgrade */
	if (WLC_DUALMAC_RSDB(wlc->cmn)) {
		wlc_rsdb_dualmac_watchdog(wlc);
		return;
	}

	/* Avoid watchdog execution for wlc 1 with "mpc 0" in
	* single MAC mode. With MPC 1, we may not enter watchdog
	* for wlc 1 after moving to MIMO/80p80 operation
	*/
	if ((wlc != WLC_RSDB_GET_PRIMARY_WLC(wlc)) &&
		(WLC_RSDB_SINGLE_MAC_MODE(WLC_RSDB_CURR_MODE(wlc)))) {
		return;
	}
#ifdef WL_MODESW
	/* Try to see if we can move to single mac mode. */
	if (WLC_MODESW_ENAB(wlc->pub)) {
		if (wlc == WLC_RSDB_GET_PRIMARY_WLC(wlc)) {
			wlc_rsdb_check_upgrade(wlc);
		}
	}
#endif /* WL_MODESW */
	return;
}

static int
wlc_rsdb_down(void *context)
{
	/* Do Nothing */
	return BCME_OK;
}

bool
wlc_rsdb_up_allowed(wlc_info_t *wlc)
{
	/* For RSDB, Do not allow UP of Core1 in MIMO mode */
	if (RSDB_ENAB(wlc->pub) &&
		(WLC_RSDB_SINGLE_MAC_MODE(WLC_RSDB_CURR_MODE(wlc))) &&
		(wlc != wlc->cmn->wlc[MAC_CORE_UNIT_0])) {
		return FALSE;
	}
	return TRUE;
}

/*
* This functions initializes the max possible rateset
* for the chip. This is required in RSDB chips for
* allowing a dynamic change in supported bw and nss
* depending upon various parameters post association.
* So the capabiliies are populated from max_rateset and
* operating bw and nss values are always equal or lower
* than the capabilities reported during assoc.
*/
void
wlc_rsdb_init_max_rateset(wlc_info_t *wlc, wlc_bss_info_t *bi)
{
	uint8 streams;
	uint8 mcsallow = 0;

	if (!wlc->cmn->max_rateset)
		return;

	if (WLC_RSDB_IS_AUTO_MODE(wlc) || WLC_RSDB_IS_AP_AUTO_MODE(wlc)) {
		streams = wlc_rsdb_get_max_chain_cap(wlc);
	} else {
		streams = WLC_BITSCNT(wlc->stf->hw_rxchain);
	}

	/* Do default update of max_rateset */
	if (!bi) {
		if (N_ENAB(wlc->pub))
			mcsallow |= WLC_MCS_ALLOW;
		if (VHT_ENAB(wlc->pub))
			mcsallow |= WLC_MCS_ALLOW_VHT;
		if (WLPROPRIETARY_11N_RATES_ENAB(wlc->pub) &&
			wlc->pub->ht_features != WLC_HT_FEATURES_PROPRATES_DISAB)
			mcsallow |= WLC_MCS_ALLOW_PROP_HT;
#ifdef WL11AC
		wlc_rateset_vhtmcs_build(wlc->cmn->max_rateset, streams);
#endif // endif
		wlc_rateset_mcs_build(wlc->cmn->max_rateset, streams);
		wlc_rateset_filter(wlc->cmn->max_rateset, wlc->cmn->max_rateset,
			FALSE, WLC_RATES_CCK_OFDM, RATE_MASK_FULL, mcsallow);
	} else {
		/* Update max_rateset using bi info */
		wlcband_t *band;
		bool cck_only;
		band = wlc->bandstate[CHSPEC_BANDUNIT(bi->chanspec)];

		if (BAND_2G(band->bandtype) && band->gmode == GMODE_LEGACY_B)
			cck_only = TRUE;
		else
			cck_only = FALSE;

		wlc_rateset_default(wlc, wlc->cmn->max_rateset, NULL, band->phytype,
			band->bandtype, cck_only, RATE_MASK_FULL,
			cck_only ? 0 : wlc_get_mcsallow(wlc, NULL),
			CHSPEC_WLC_BW(bi->chanspec), streams);
	}
	return;
}

/*
* This is a generic function to perform init in wlc->cmn
* members which are specific to RSDB module.
*/
int
BCMATTACHFN(wlc_rsdb_wlc_cmn_attach)(wlc_info_t *wlc)
{
	wlc->cmn->rsdb_mode_target = WLC_RSDB_MODE_MAX;
#if !defined(WLRSDB_DISABLED)
#if !defined(WLTEST)
	if (WLC_DUALMAC_RSDB(wlc->cmn)) {
		wlc->cmn->rsdb_mode = WLC_RSDB_MODE_RSDB;
	} else {
		wlc->cmn->rsdb_mode = WLC_RSDB_MODE_RSDB | WLC_RSDB_MODE_AUTO_MASK;
	}
#else
	wlc->cmn->rsdb_mode = WLC_RSDB_MODE_RSDB;
	mboolset(wlc->cmn->rsdb_auto_override, WLC_RSDB_AUTO_OVERRIDE_MFGTEST);
#endif /* WLTEST */
#elif defined(RSDB_MODE_80P80)
	wlc->cmn->rsdb_mode = WLC_RSDB_MODE_80P80;
#else
	wlc->cmn->rsdb_mode = WLC_RSDB_MODE_2X2;
#endif /* !WLRSDB_DISABLED */
	return BCME_OK;
}

/*
* This is a generic function to perform de-init of wlc->cmn
* members which are specific to RSDB module.
*/
void
BCMATTACHFN(wlc_rsdb_cmn_detach)(wlc_info_t *wlc)
{
	return;
}

/* Get MAX capability chain  */
static int
wlc_rsdb_get_max_chain_cap(wlc_info_t *wlc)
{
	wlc_info_t *wlc0 = wlc->cmn->wlc[0];
	wlc_info_t *wlc1 = wlc->cmn->wlc[1];
	int wlc_max_chain;

	WL_TRACE(("wl%d:%s:%d\n", wlc->pub->unit, __FUNCTION__, __LINE__));
	/* Find wlc with has max chain capability */
	wlc_max_chain = MAX((WLC_BITSCNT(wlc0->stf->hw_rxchain_cap)),
		(WLC_BITSCNT(wlc1->stf->hw_rxchain_cap)));
	return wlc_max_chain;
}

/* Finds the WLC based on chanspec in RSDB case.
 * if there is no existing connection  find if AP is
 * MIMO/SISO capable & return corresponding wlc.
 * if connection exists check if in coming request is
 * for same band or not & return corresponding wlc.
 */
static wlc_info_t*
wlc_rsdb_find_wlc(wlc_info_t *wlc, wlc_bsscfg_t *join_cfg, wlc_bss_info_t *bi)
{
	wlc_info_t* to_wlc = wlc;
	DBGONLY(char chanbuf[CHANSPEC_STR_LEN]; )
	WLRSDB_DBG(("%s: Incoming wlc:%d Request wlc for: is MIMO mode %d, chspec:%s\n",
		__FUNCTION__, wlc->pub->unit, wlc_bss_get_mimo_cap(bi),
		wf_chspec_ntoa_ex(bi->chanspec, chanbuf)));

	if (!WLC_DUALMAC_RSDB(wlc->cmn)) {
		to_wlc = wlc_rsdb_find_wlc_for_chanspec(wlc, bi, bi->chanspec, NULL,
			WL_RSDB_CORE_MATCH_BW);
	} else if (wlc_rsdb_any_wlc_associated(wlc) == 0) {
		/* If there is no existing connection, find if AP is MIMO/SISO capable
		 * & return corresponding wlc
		 */
		WLRSDB_DBG(("Non-Associated case: \n"));

#if defined(WLTEST)
		to_wlc = wlc_rsdb_find_wlc_for_chanspec(wlc, bi, bi->chanspec, NULL,
			WL_RSDB_CORE_MATCH_BW);
#else
		if (WLC_RSDB_IS_AUTO_MODE(wlc) && !wlc->bandlocked) {
				to_wlc = wlc_rsdb_find_wlc_for_chanspec(wlc, bi,
					bi->chanspec, NULL, (WL_RSDB_CORE_MATCH_BW |
					WL_RSDB_CORE_MATCH_CHAINS));
		} else {
			to_wlc = wlc_rsdb_find_wlc_for_chanspec(wlc, bi, bi->chanspec, NULL,
				WL_RSDB_CORE_MATCH_BW);
		}
#endif // endif
	} else {
		WLRSDB_DBG(("Associated case: \n"));
		to_wlc = wlc_rsdb_find_wlc_for_chanspec(wlc, bi, bi->chanspec, NULL,
			WL_RSDB_CORE_MATCH_BW);
	}

#if defined(BCMDBG)
	if (to_wlc) {
		int chainbit_count = (WLC_BITSCNT(to_wlc->stf->hw_txchain));
		WLRSDB_DBG(("    Return wlc:%d : Chain %d :", to_wlc->pub->unit, chainbit_count));
		if (chainbit_count && chainbit_count != 1) {
			WLRSDB_DBG(("to_wlc chain is MIMO\n"));
		} else {
			WLRSDB_DBG(("to_wlc chain is SISO\n"));
		}
	}
#endif // endif
	return to_wlc;
}

/* This function is a wrapper to restore/override the
* AUTO mode switching
*/
void
wlc_rsdb_config_auto_mode_switch(wlc_info_t *wlc, uint32 reason, uint32 override)
{
	if (override) {
		wlc_rsdb_override_auto_mode_switch(wlc, reason);
	} else {
		wlc_rsdb_restore_auto_mode_switch(wlc, reason);
	}
	return;
}

/* This function overrides the AUTO mode switching */
static void
wlc_rsdb_override_auto_mode_switch(wlc_info_t *wlc, uint32 reason)
{
	if (WLC_RSDB_IS_AUTO_MODE(wlc)) {
		wlc_rsdb_change_mode(wlc, WLC_RSDB_CURR_MODE(wlc));
	}
	mboolset(wlc->cmn->rsdb_auto_override, reason);
}

/* This function restores the overrides of AUTO mode switching */
static void
wlc_rsdb_restore_auto_mode_switch(wlc_info_t *wlc, uint32 reason)
{
	if ((reason == WLC_RSDB_AUTO_OVERRIDE_RATE) &&
		WLC_RSPEC_OVERRIDE_ANY(wlc))
		return;
	mboolclr(wlc->cmn->rsdb_auto_override, reason);
	/* Restore AUTO if no overrides are set */
	if (!wlc->cmn->rsdb_auto_override)
		wlc_rsdb_change_mode(wlc, WLC_RSDB_MODE_AUTO);
}

static int
wlc_rsdb_ht_parse_cap_ie(void *ctx, wlc_iem_parse_data_t *parse)
{
	wlc_rsdb_info_t *rsdbinfo = (wlc_rsdb_info_t *)ctx;
	wlc_info_t *wlc = rsdbinfo->wlc;
	wlc_iem_ft_pparm_t *ftpparm = parse->pparm->ft;
	ht_cap_ie_t *cap_ie;
	scb_t* scb = NULL;
	wlc_rsdb_scb_cubby_t *scb_cubby = NULL;

	if (parse->ie == NULL)
		return BCME_OK;

	switch (parse->ft) {
		case FC_ASSOC_REQ:
		case FC_REASSOC_REQ:
			scb = ftpparm->assocreq.scb;
			scb_cubby = SCB_CUBBY(scb, rsdbinfo->scb_handle);
			if ((cap_ie = wlc_read_ht_cap_ie(wlc, parse->ie, parse->ie_len)) != NULL) {
				/* copy the raw mcs set (supp_mcs) into the peer scb rateset. */
				memcpy(&scb_cubby->mcs[0], &cap_ie->supp_mcs[0], MCSSET_LEN);
			}
		break;
		case FC_BEACON:
			scb = ftpparm->bcn.scb;
			scb_cubby = SCB_CUBBY(scb, rsdbinfo->scb_handle);
			scb_cubby->isapmimo = FALSE;
			if ((cap_ie = wlc_read_ht_cap_ie(wlc, parse->ie, parse->ie_len)) != NULL) {
				/* use cap_ie->supp_mcs to check if peer AP is
				 * MIMO capable or not
				 */
				if (HT_CAP_MCS_BITMASK(cap_ie->supp_mcs)) {
					/* Is Rx and Tx MCS set not equal ? */
					if (HT_CAP_MCS_TX_RX_UNEQUAL(cap_ie->supp_mcs)) {
					/* Check the Tx stream support */
						if (HT_CAP_MCS_TX_STREAM_SUPPORT(
							cap_ie->supp_mcs)) {
							scb_cubby->isapmimo = TRUE;
						}
					}
					/* Tx MCS and Rx MCS
					 * are equel
					 */
					else {
						scb_cubby->isapmimo = TRUE;
					}
				}
			}
		break;
	}
	return BCME_OK;
}

static int
wlc_rsdb_vht_parse_cap_ie(void *ctx, wlc_iem_parse_data_t *parse)
{
	wlc_rsdb_info_t *rsdbinfo = (wlc_rsdb_info_t *)ctx;
	wlc_info_t *wlc = rsdbinfo->wlc;
	wlc_iem_ft_pparm_t *ftpparm = parse->pparm->ft;
	vht_cap_ie_t vht_cap_ie;
	vht_cap_ie_t *vht_cap_ie_p = NULL;
	scb_t* scb = NULL;
	wlc_rsdb_scb_cubby_t *scb_cubby = NULL;

	if (parse->ie == NULL)
		return BCME_OK;

	switch (parse->ft) {
		case FC_BEACON:
			scb = ftpparm->bcn.scb;
			ASSERT(scb != NULL);
			scb_cubby = SCB_CUBBY(scb, rsdbinfo->scb_handle);
			scb_cubby->isapmimo = FALSE;
			/* use rx_mcs_map, tx_mcs_map to check if peer AP is MIMO capable or not */
			if ((vht_cap_ie_p = wlc_read_vht_cap_ie(wlc->vhti, parse->ie,
				parse->ie_len, &vht_cap_ie)) != NULL) {
				if ((VHT_MCS_SS_SUPPORTED(2, vht_cap_ie_p->rx_mcs_map))&&
					(VHT_MCS_SS_SUPPORTED(2, vht_cap_ie_p->tx_mcs_map))) {
					scb_cubby->isapmimo = TRUE;
				}
			}
		break;
	}
	return BCME_OK;
}

/* This function populates the scb rateset with
* the max rates supported by our peer
*/
static void
wlc_rsdb_update_scb_rateset(wlc_rsdb_info_t *rsdbinfo, struct scb *scb)
{
	wlc_rsdb_scb_cubby_t * cubby_info = SCB_CUBBY(scb, rsdbinfo->scb_handle);
	memcpy(&scb->rateset.mcs[0], &cubby_info->mcs[0], MCSSET_LEN);
	wlc_rateset_filter(&scb->rateset, &scb->rateset, FALSE, FALSE, RATE_MASK_FULL,
			wlc_get_mcsallow(rsdbinfo->wlc, NULL));
	return;
}
static bool
wlc_rsdb_peerap_ismimo(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	struct scb *scb;
	wlc_rsdb_scb_cubby_t * cubby_info;
	scb = wlc_scbfind(wlc, cfg, &cfg->current_bss->BSSID);
	cubby_info = SCB_CUBBY(scb, wlc->rsdbinfo->scb_handle);
	return cubby_info->isapmimo;

}
#if defined(WLCHANIM)
/* Check to upgrade to 3x3 chain */
static void
wlc_rsdb_check_load_based_upgarde(wlc_rsdb_info_t* rsdbinfo)
{
	wlc_info_t *wlc = rsdbinfo->wlc;
	uint32 load = 0;
	wlc_bsscfg_t *cfg = NULL;

	/* Get effective load from LQ module */
	load  = wlc_rsdb_get_lq_load(wlc);

	/* Initiate the move if load exceeds threshold */
	if (load > rsdbinfo->load_threshold) {
		bool cfg_moveable = FALSE;
		int idx = 0;
		struct scb * scb = NULL;
		FOREACH_AS_BSS(wlc, idx, cfg) {
			wlc_rsdb_scb_cubby_t * cubby_info;
			scb = wlc_scbfind(wlc, cfg, &cfg->current_bss->BSSID);
			cubby_info = SCB_CUBBY(scb, wlc->rsdbinfo->scb_handle);
			if (cubby_info->isapmimo) {
				cfg_moveable = TRUE;
			}
		}
		if (cfg_moveable) {
			FOREACH_AS_BSS(wlc, idx, cfg) {
				if (WLC_BSS_CONNECTED(cfg) && cfg_moveable) {
					/* move mimo in wlc 1 to wlc0
					 * call uprgade which will send
					 * oper mode & does bsscfg clone
					 */
					WLRSDB_DBG(("%s : Dyn Upgrade : load %d  cfg %p \n",
						__FUNCTION__, load, cfg));
					wlc_rsdb_move_connection(cfg, RSDB_MOVE_CHAIN0, TRUE);
				}
			}
		}
	}
}
#endif /* WLCHANIM */

bool
wlc_rsdb_change_band_allowed(wlc_info_t *wlc, enum wlc_bandunit bandunit)
{
	return CHSPEC_BANDUNIT(wlc->chanspec) != bandunit;
}

/* return the wlc on which there is an infra connection
** In case of no infra connection return NULL
*/
wlc_bsscfg_t *
wlc_rsdb_get_infra_cfg(wlc_info_t *wlc)
{
	int idx;
	wlc_info_t *wlc_iter;
	FOREACH_WLC(wlc->cmn, idx, wlc_iter) {
		if (wlc_iter->cfg->associated) {
			return wlc_iter->cfg;
		}
	}
	return NULL;
}

#if defined(RSDB_CMN_BANDSTATE) && !defined(RSDB_CMN_BANDSTATE_DISABLED)
/* Function handles updating of Common BW cap for RSDB chips. cmn_bwcap is common BW capability
 * of all wlc cores. Currently this is derived to be max of all cores BW capability.
 *
 * cmn_bwcap is used instead of pub->phy_bwXXX_capable in various IOVARs like "bw_cap",
 * "chanspecs", etc., to return overall chip capability irrespective of the WLC it is
 * queried on. This was done to specifically solve issues related BW a
 * SoftAP/IBSS is started due a interface getting cloned between asymmetric
 * dual-mac RSDB slices (aux with 20MHz)
 */
static void
BCMATTACHFN(wlc_rsdb_derive_cmn_bwcap)(wlc_rsdb_info_t *rsdbinfo, int bandtype)
{
	wlc_info_t* wlc = rsdbinfo->wlc;
	wlc_info_t* iwlc;
	uint8 *cmn_bwcap;
	int idx;
	bool phy_bw40_capable = FALSE;
	bool phy_bw80_capable = FALSE;
	bool phy_bw8080_capable = FALSE;
	bool phy_bw160_capable = FALSE;

	/* XXX All RSDB chips are assumed to be dualband capable. Will attempt updating common
	 * bwcap on 2G and 5G band
	 */
	if (bandtype == WLC_BAND_5G) {
		cmn_bwcap = &rsdbinfo->cmn->cmn_bwcap[BAND_5G_INDEX];
	} else if (bandtype == WLC_BAND_2G) {
		cmn_bwcap = &rsdbinfo->cmn->cmn_bwcap[BAND_2G_INDEX];
	} else {
		WL_ERROR(("wl%d: %s: In valid bandtype %d\n", wlc->pub->unit, __FUNCTION__,
			bandtype));
		return;
	}

	/* Ensure that for RSDB chips the PHY capability is best of both cores */
	FOREACH_WLC(wlc->cmn, idx, iwlc) {
		if (WLC_40MHZ_CAP_PHY(iwlc)) {
			phy_bw40_capable = TRUE;
		}
		if (WLC_80MHZ_CAP_PHY(iwlc)) {
			phy_bw80_capable = TRUE;
		}
		if (WLC_8080MHZ_CAP_PHY(iwlc)) {
			phy_bw8080_capable = TRUE;
		}
		if (WLC_160MHZ_CAP_PHY(iwlc)) {
			phy_bw160_capable = TRUE;
		}
	}

	if (bandtype == WLC_BAND_2G) {
		*cmn_bwcap = phy_bw40_capable ? WLC_BW_CAP_40MHZ : WLC_BW_CAP_20MHZ;
	} else {
		if (phy_bw160_capable || phy_bw8080_capable) {
			*cmn_bwcap = WLC_BW_CAP_160MHZ;
		} else if (phy_bw80_capable) {
			*cmn_bwcap = WLC_BW_CAP_80MHZ;
		} else if (phy_bw40_capable) {
			*cmn_bwcap = WLC_BW_CAP_40MHZ;
		} else {
			*cmn_bwcap = WLC_BW_CAP_20MHZ;
		}
	}
}

uint8
wlc_rsdb_get_cmn_bwcap(wlc_info_t *wlc, int bandtype)
{
	uint8 bwcap = 0;
	int bandindex;

	if (bandtype == WLC_BAND_5G) {
		bandindex = BAND_5G_INDEX;
	} else if (bandtype == WLC_BAND_2G) {
		bandindex = BAND_2G_INDEX;
	} else {
		WL_ERROR(("wl%d: %s: In valid bandtype %d\n", wlc->pub->unit, __FUNCTION__,
			bandtype));
		ASSERT(0);
		goto done;
	}

	/* If bandstate is shared across WLCs then we return common BW cap */
	if (RSDB_CMN_BANDSTATE_ENAB(wlc->pub)) {
		bwcap = wlc->rsdbinfo->cmn->cmn_bwcap[bandindex];
	}

done:
	return bwcap;
}
#endif /* defined(RSDB_CMN_BANDSTATE) && !defined(RSDB_CMN_BANDSTATE_DISABLED) */

uint8
wlc_rsdb_any_go_active(wlc_info_t* wlc)
{
	wlc_info_t *wlc_iter;
	uint8 idx1 = 0;
	uint8 num_gos = 0;
	wlc_cmn_info_t *wlc_cmn = wlc->cmn;

	FOREACH_WLC(wlc_cmn, idx1, wlc_iter) {
		uint8 idx2 = 0;
		wlc_bsscfg_t *cfg;
		FOREACH_UP_AP(wlc_iter, idx2, cfg) {
			if (P2P_GO(wlc_iter, cfg))
				num_gos++;
		}
	}
	return num_gos;
}

uint8
wlc_rsdb_any_aps_active(wlc_info_t* wlc)
{
	wlc_info_t *wlc_iter;
	uint8 idx = 0;
	uint8 num_aps = 0;
	wlc_cmn_info_t *wlc_cmn = wlc->cmn;

	FOREACH_WLC(wlc_cmn, idx, wlc_iter) {
		if AP_ACTIVE(wlc_iter) {
			num_aps += wlc_iter->aps_associated;
		}
	}

	return num_aps;
}

bool
wlc_rsdb_bsscfg_is_linked_cfg(wlc_rsdb_info_t *rsdbinfo, wlc_bsscfg_t *cfg)
{
	if (!cfg || !(cfg->flags2 & WLC_BSSCFG_FL2_RSDB_LINKED)) {
		return FALSE;
	}
	return TRUE;
}

/* This function returns the RSDB linked bsscfg for the given cfg */
wlc_bsscfg_t *
wlc_rsdb_bsscfg_get_linked_cfg(wlc_rsdb_info_t *rsdbinfo, wlc_bsscfg_t *cfg)
{
	wlc_bsscfg_t *bsscfg;
	wlc_info_t *wlc;
	int16 linked_cfg_idx;

	if (wlc_rsdb_bsscfg_is_linked_cfg(rsdbinfo, cfg) == FALSE) {
		/* Linked bsscfg is either NULL or doesn't have
		 * WLC_BSSCFG_FL2_RSDB_LINKED flag set.
		 */
		ASSERT(0);
		goto fail;
	}

	ASSERT(rsdbinfo != NULL);
	wlc = rsdbinfo->wlc;
	ASSERT(wlc != NULL);

	linked_cfg_idx = rsdbinfo->cmn->linked_cfg_indices[cfg->_idx];

	if (linked_cfg_idx == WLC_BSSCFG_INVALID_IDX) {
		goto fail;
	}

	bsscfg = wlc->bsscfg[linked_cfg_idx];
	return bsscfg;

fail:
	return NULL;
}
#if defined(WLSLOTTED_BSS)
/* This function is used to create a pair of bsscfgs one for each band and will create OS interface
 * based on the flags.
 *
 * Modules which operate on both the bands either simultaneously or more frequently
 * will have to use this function
 *
 * If flags has WLC_BSSCFG_RSDB_IF set, then host interface will be created for bsscfg on
 * Core-0. Only wlcif, wlif will be created for cfg on Core-1.
 *
 * If flags has WLC_BSSCFG_NOIF set, then both bsscfgs won't be having any OS interface created.
 * if_idx is returned from this function is not valid in this case.
 *
 * else if, none of the above flags is set, OS interfaces will be created for both the bsscfgs.
 * But, only if_idx for the core-0 will be returned. If this option is really used and if_idx is
 * needed, please get it from corresponding cfg.
 */
wlc_bsscfg_t *
wlc_rsdb_bsscfg_create_params(wlc_rsdb_info_t *rsdbinfo, wlc_bsscfg_create_param_t *params,
		int *error)
{
	wlc_info_t *wlc_iter;
	int wlc_idx;
	int idx;
	int32 err = BCME_OK;
	wlc_bsscfg_t *cfg_arr[MAX_RSDB_MAC_NUM];
	wlc_bsscfg_t *bsscfg = NULL;
	int cfg_indices[MAX_RSDB_MAC_NUM];
	int if_index = 0;

	memset(cfg_indices, 0, sizeof(cfg_indices));

	FOREACH_WLC(rsdbinfo->wlc->cmn, wlc_idx, wlc_iter) {
		/* Assign NULL : To handle the cases where caller sends cfg_arr[]
		 * argument uninitialized.
		 */
		cfg_arr[wlc_idx] = NULL;

		/* If cores are more than 2, revisit this code */
		ASSERT(wlc_iter->pub->unit <= MAC_CORE_UNIT_1);
		/* Always ensuring that OS interface gets created on core 0 only
		 */
		if (wlc_iter->pub->unit == MAC_CORE_UNIT_1) {
			params->bsscfg_flag |= WLC_BSSCFG_RSDB_IF;
			/*
			 * For non primary interface set if_index to 0 in order to auto assign the
			 * if_index.
			 */
			params->if_buf->if_index = 0;
		}
		/* allocate bsscfg */
		if ((idx = wlc_bsscfg_get_free_idx(wlc_iter)) == -1) {
			WL_ERROR(("wl%d: no free bsscfg\n", wlc_iter->pub->unit));
			err = BCME_NORESOURCE;
			goto fail;
		}

		params->bsscfg_idx = (int8)idx;
		bsscfg = wlc_bsscfg_alloc_iface_create(wlc_iter, params, &err);
		if (bsscfg == NULL) {
			WL_ERROR(("wl%d: cannot create bsscfg\n", wlc_iter->pub->unit));
			err = BCME_NOMEM;
			goto fail;
		}

		ASSERT(bsscfg != NULL);

		cfg_arr[wlc_idx] = bsscfg;
		cfg_indices[wlc_idx] = idx;

		if (wlc_iter->pub->unit == MAC_CORE_UNIT_0) {
			if (bsscfg->wlcif->wlif) {
				/* Get the interface idx created for the core-0 */
				if_index = wl_find_if(bsscfg->wlcif->wlif);
				if (if_index <= 0) {
					WL_ERROR(("%s wl%d can't find interface index\n",
							__FUNCTION__, wlc_iter->pub->unit));
					ASSERT(0);
					err = BCME_ERROR;
					goto fail;
				}
			}
		} else {
			if (bsscfg->wlcif->wlif) {
				/* Try rebinding this if_idx to other bsscfg */
				err = wl_rebind_if(bsscfg->wlcif->wlif, if_index, FALSE);
				if (err != BCME_OK) {
					/* Rebind if failed */
					WL_ERROR(("%s wl%d rebind interface failed with err %d",
							__FUNCTION__, wlc_iter->pub->unit, err));
					ASSERT(0);
					err = BCME_ERROR;
					goto fail;
				}
			}
		}
	}
	/* Do the initialization of linked bsscfg indices.
	 * Initialize the linked cfg idx in the cubby.
	 */
	if (cfg_arr[MAC_CORE_UNIT_0] && cfg_arr[MAC_CORE_UNIT_1]) {
		wlc_rsdb_link_cfg(rsdbinfo, cfg_arr[MAC_CORE_UNIT_0], cfg_arr[MAC_CORE_UNIT_1],
			if_index);
	}
	if (*error != NULL) {
		*error = err;
	}
	return cfg_arr[MAC_CORE_UNIT_0];

fail:
	for (idx = 0; idx < MAX_RSDB_MAC_NUM; idx++) {
		if (cfg_arr[idx]) {
			wlc_bsscfg_free(cfg_arr[idx]->wlc, cfg_arr[idx]);
			cfg_arr[idx] = NULL;
		}
	}
	if (*error != NULL) {
		*error = err;
	}
	return NULL;
}

void wlc_rsdb_link_cfg(wlc_rsdb_info_t * rsdbinfo, wlc_bsscfg_t *cfg_mac0,
	wlc_bsscfg_t *cfg_mac1, int index)
{
	int8 *linked_cfg_indices = rsdbinfo->cmn->linked_cfg_indices;
	int *linked_cfg_hostif_indices =
			rsdbinfo->cmn->linked_cfg_hostif_indices;

	ASSERT(cfg_mac0->wlc->pub->unit == 0);
	ASSERT(cfg_mac1->wlc->pub->unit == 1);

	cfg_mac0->flags2 |= WLC_BSSCFG_FL2_RSDB_LINKED;
	cfg_mac1->flags2 |= WLC_BSSCFG_FL2_RSDB_LINKED;
	linked_cfg_indices[WLC_BSSCFG_IDX(cfg_mac0)] = WLC_BSSCFG_IDX(cfg_mac1);
	linked_cfg_indices[WLC_BSSCFG_IDX(cfg_mac1)] = WLC_BSSCFG_IDX(cfg_mac0);

	/* Store the ifindex that is notified to host for this linked cfgs */
	linked_cfg_hostif_indices[WLC_BSSCFG_IDX(cfg_mac0)] =
		linked_cfg_hostif_indices[WLC_BSSCFG_IDX(cfg_mac1)] = index;
}

/* This function is used to delete the pair of bsscfgs created using wlc_rsdb_bsscfg_create().
 */
int
wlc_rsdb_bsscfg_delete(wlc_rsdb_info_t *rsdbinfo, wlc_bsscfg_t *bsscfg)
{
	int err = BCME_OK, i;
	wlc_bsscfg_t *linked_cfg = wlc_rsdb_bsscfg_get_linked_cfg(rsdbinfo, bsscfg);
	wlc_bsscfg_t *cfg[MAX_RSDB_MAC_NUM] = {bsscfg, linked_cfg};
	int8 *linked_cfg_indices = rsdbinfo->cmn->linked_cfg_indices;
	int *linked_cfg_hostif_indices = rsdbinfo->cmn->linked_cfg_hostif_indices;

	if (linked_cfg == NULL) {
		ASSERT(0);
		err = BCME_ERROR;
		goto exit;
	}

	for (i = 0; i < MAX_RSDB_MAC_NUM; i++) {
		wlc_bsscfg_disable(cfg[i]->wlc, cfg[i]);

		/* Mark the linked cfg indices to invalid indices */
		linked_cfg_indices[cfg[i]->_idx] = WLC_BSSCFG_INVALID_IDX;
		linked_cfg_hostif_indices[cfg[i]->_idx] = WLC_BSSCFG_INVALID_IFIDX;

		cfg[i]->flags2 &= ~WLC_BSSCFG_FL2_RSDB_LINKED;
		/* delete the linked bsscfg */
		wlc_bsscfg_free(cfg[i]->wlc, cfg[i]);
	}

exit:
	return err;
}

/* This function is used to enable the pair of bsscfgs created using wlc_rsdb_bsscfg_create().
 */
int
wlc_rsdb_bsscfg_enable(wlc_rsdb_info_t *rsdbinfo, wlc_bsscfg_t *bsscfg)
{
	int err = BCME_OK, i;
	wlc_bsscfg_t *linked_cfg = wlc_rsdb_bsscfg_get_linked_cfg(rsdbinfo, bsscfg);
	wlc_bsscfg_t *cfg[MAX_RSDB_MAC_NUM] = {bsscfg, linked_cfg};

	if (linked_cfg == NULL) {
		ASSERT(0);
		err = BCME_ERROR;
		goto exit;
	}

	for (i = 0; i < MAX_RSDB_MAC_NUM; i++) {
		if ((err = wlc_bsscfg_enable(cfg[i]->wlc, cfg[i])) != BCME_OK) {
			WL_ERROR(("wl%d: %s: wlc_bsscfg_enable failed %d\n",
					cfg[i]->wlc->pub->unit, __FUNCTION__, err));
		}
	}

exit:
	return err;
}

/* This function is used to disable the pair of bsscfgs created using wlc_rsdb_bsscfg_create().
 */
int
wlc_rsdb_bsscfg_disable(wlc_rsdb_info_t *rsdbinfo, wlc_bsscfg_t *bsscfg)
{
	int err = BCME_OK, i;
	wlc_bsscfg_t *linked_cfg = wlc_rsdb_bsscfg_get_linked_cfg(rsdbinfo, bsscfg);
	wlc_bsscfg_t *cfg[MAX_RSDB_MAC_NUM] = {bsscfg, linked_cfg};

	if (linked_cfg == NULL) {
		ASSERT(0);
		err = BCME_ERROR;
		goto exit;
	}

	for (i = 0; i < MAX_RSDB_MAC_NUM; i++) {
		wlc_bsscfg_disable(cfg[i]->wlc, cfg[i]);
	}

exit:
	return err;
}

/* This function is used to initialize the pair of bsscfgs created using wlc_rsdb_bsscfg_create().
 */
int
wlc_rsdb_bsscfg_init(wlc_rsdb_info_t *rsdbinfo, wlc_bsscfg_t *bsscfg)
{
	int err = BCME_OK, i;
	wlc_bsscfg_t *linked_cfg = wlc_rsdb_bsscfg_get_linked_cfg(rsdbinfo, bsscfg);
	wlc_bsscfg_t *cfg[MAX_RSDB_MAC_NUM] = {bsscfg, linked_cfg};

	if (linked_cfg == NULL) {
		ASSERT(0);
		err = BCME_ERROR;
		goto exit;
	}

	for (i = 0; i < MAX_RSDB_MAC_NUM; i++) {
		if ((err = wlc_bsscfg_init(cfg[i]->wlc, cfg[i])) != BCME_OK) {
			WL_ERROR(("wl%d: %s: bsscfg init failed\n",
					cfg[i]->wlc->pub->unit, __FUNCTION__));
			goto exit;
		}
	}

exit:
	return err;
}

/* This function is used to de-init the pair of bsscfgs created using wlc_rsdb_bsscfg_create().
 */
int
wlc_rsdb_bsscfg_deinit(wlc_rsdb_info_t *rsdbinfo, wlc_bsscfg_t *bsscfg)
{
	int err = BCME_OK, i;
	wlc_bsscfg_t *linked_cfg = wlc_rsdb_bsscfg_get_linked_cfg(rsdbinfo, bsscfg);
	wlc_bsscfg_t *cfg[MAX_RSDB_MAC_NUM] = {bsscfg, linked_cfg};

	if (linked_cfg == NULL) {
		ASSERT(0);
		err = BCME_ERROR;
		goto exit;
	}

	for (i = 0; i < MAXBANDS; i++) {
		wlc_bsscfg_disable(cfg[i]->wlc, cfg[i]);
		wlc_bsscfg_deinit(cfg[i]->wlc, cfg[i]);
	}

exit:
	return err;
}
#endif /* WLSLOTTED_BSS */

/* match wlc band check to avoid config other band
 */
bool
wlc_rsdb_band_match(wlc_info_t *wlc, wlcband_t *band)
{
	if (!(RSDB_CMN_BANDSTATE_ENAB(wlc->pub))) {
		return TRUE;
	} else if (wlc->band == band) {
		return TRUE;
	} else {
		return FALSE;
	}
}
#endif /* WLRSDB */
