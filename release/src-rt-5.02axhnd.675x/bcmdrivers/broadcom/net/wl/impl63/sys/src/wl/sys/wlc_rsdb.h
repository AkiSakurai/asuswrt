/*
 * WLC RSDB API definition
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
 * $Id: wlc_rsdb.h 778655 2019-09-06 11:27:56Z $
 */

#ifndef _wlc_rsdb_h_
#define _wlc_rsdb_h_
#include <wlc_bsscfg.h>
#ifdef WLRSDB
typedef enum {
	WLC_RSDB_MODE_AUTO = AUTO,
	WLC_RSDB_MODE_2X2,
	WLC_RSDB_MODE_RSDB,
	WLC_RSDB_MODE_80P80,
	WLC_RSDB_MODE_MAX
} wlc_rsdb_modes_t;

typedef enum {
	WLC_INFRA_MODE_AUTO = AUTO,
	WLC_INFRA_MODE_MAIN,
	WLC_INFRA_MODE_AUX
} wlc_infra_modes_t;

/* Indicates the reason for calling callback of rsdb_clone_timer in mode switch. */
typedef enum {
	WLC_MODESW_LIST_START = 0,
	WLC_MODESW_CLONE_TIMER = 1, /* Timer callback should handle cloning */
	WLC_MODESW_UPGRADE_TIMER = 2, /* Timer callback should handle R->V upgrade */
	WLC_MODESW_LIST_END
} wlc_modesw_cb_type;

#define CFG_CLONE_START		0x01
#define CFG_CLONE_END		0x02

typedef struct rsdb_cfg_clone_upd_data
{
	wlc_bsscfg_t *cfg;
	wlc_info_t *from_wlc;
	wlc_info_t *to_wlc;
	uint	type;
} rsdb_cfg_clone_upd_t;

typedef void (*rsdb_cfg_clone_fn_t)(void *ctx, rsdb_cfg_clone_upd_t *evt_data);
extern int wlc_rsdb_cfg_clone_register(wlc_rsdb_info_t *rsdb,
	rsdb_cfg_clone_fn_t fn, void *arg);
extern int wlc_rsdb_cfg_clone_unregister(wlc_rsdb_info_t *rsdb,
	rsdb_cfg_clone_fn_t fn, void *arg);
extern void wlc_rsdb_suppress_pending_tx_pkts(wlc_bsscfg_t *from_cfg);
extern bool wlc_rsdb_get_core_band_lock(wlc_rsdb_info_t *rsdbinfo);
extern void wlc_rsdb_link_cfg(wlc_rsdb_info_t * rsdbinfo, wlc_bsscfg_t *cfg_mac0,
	wlc_bsscfg_t *cfg_mac1, int index);

#define MODE_RSDB 0
#define MODE_VSDB 1
#define WL_RSDB_VSDB_CLONE_WAIT 2
#define WL_RSDB_ZERO_DELAY 0
#define WL_RSDB_UPGRADE_TIMER_DELAY 2

/* Flags to restrict wlc selection */
#define WL_RSDB_CORE_MATCH_BAND		0x1
#define WL_RSDB_CORE_MATCH_BW		0x2
#define WL_RSDB_CORE_MATCH_CHAINS	0x4

/* RSDB Auto mode override flags */
#define WLC_RSDB_AUTO_OVERRIDE_RATE			0x01
#define WLC_RSDB_AUTO_OVERRIDE_SCAN			0x02
#define WLC_RSDB_AUTO_OVERRIDE_USER			0x04
#define WLC_RSDB_AUTO_OVERRIDE_MFGTEST			0x08
#define WLC_RSDB_AUTO_OVERRIDE_BANDLOCK			0x10
#define WLC_RSDB_AUTO_OVERRIDE_P2PSCAN			0x20
#define WLC_RSDB_AUTO_OVERRIDE_ULB			0x40

/* Default primary cfg index for Core 1 */
#define RSDB_INACTIVE_CFG_IDX	1

#define WLC_RSDB_GET_PRIMARY_WLC(curr_wlc)	((curr_wlc)->cmn->wlc[0])

#ifdef WL_DUALMAC_RSDB
/* In case of asymmetric dual mac chip only RSDB mode is supported */
#define WLC_RSDB_DUAL_MAC_MODE(mode)	(1)
#define WLC_RSDB_SINGLE_MAC_MODE(mode)	(0)
#else
#define WLC_RSDB_DUAL_MAC_MODE(mode)	((mode) == WLC_RSDB_MODE_RSDB)
#define WLC_RSDB_SINGLE_MAC_MODE(mode)	(((mode) == WLC_RSDB_MODE_2X2) ||	\
	((mode) == WLC_RSDB_MODE_80P80))
#endif // endif

#define WLC_RSDB_IS_AUTO_MODE(wlc_any) (wlc_any->cmn->rsdb_mode & WLC_RSDB_MODE_AUTO_MASK)
#define WLC_RSDB_IS_AP_AUTO_MODE(wlc_any) (wlc_any->cmn->ap_rsdb_mode == AUTO)

#define SCB_MOVE /* Added flag temporarily to enable scb move during bsscfg clone */
#define WLC_RSDB_CURR_MODE(wlc) WLC_RSDB_EXTRACT_MODE((wlc)->cmn->rsdb_mode)
#define DUALMAC_RSDB_LOAD_THRESH	70
enum rsdb_move_chains {
	RSDB_MOVE_DYN = -1,
	RSDB_MOVE_CHAIN0 = 0,
	RSDB_MOVE_CHAIN1
};
extern int wlc_rsdb_assoc_mode_change(wlc_bsscfg_t **cfg, wlc_bss_info_t *bi);
typedef int (*rsdb_assoc_mode_change_cb_t)(void *ctx, wlc_bsscfg_t **pcfg, wlc_bss_info_t *bi);
typedef int (*rsdb_get_wlcs_cb_t)(void *ctx, wlc_info_t *wlc, wlc_info_t **wlc_2g,
	wlc_info_t **wlc_5g);
extern void wlc_rsdb_register_get_wlcs_cb(wlc_rsdb_info_t *rsdbinfo,
	rsdb_get_wlcs_cb_t cb, void *ctx);
extern void wlc_rsdb_register_mode_change_cb(wlc_rsdb_info_t *rsdbinfo,
        rsdb_assoc_mode_change_cb_t cb, void *ctx);
extern int wlc_rsdb_change_mode(wlc_info_t *wlc, int8 to_mode);
extern uint8 wlc_rsdb_association_count(wlc_info_t* wlc);
#ifdef WL_MODESW
extern uint8 wlc_rsdb_downgrade_wlc(wlc_info_t *wlc);
extern uint8 wlc_rsdb_upgrade_wlc(wlc_info_t *wlc);
extern bool wlc_rsdb_upgrade_allowed(wlc_info_t *wlc);
extern int wlc_rsdb_check_upgrade(wlc_info_t *wlc);
extern int wlc_rsdb_dyn_switch(wlc_info_t *wlc, bool mode);
#endif // endif
extern int wlc_rsdb_handle_sta_csa(wlc_rsdb_info_t* rsdbinfo, wlc_bsscfg_t *bsscfg,
	chanspec_t chanspec);
bool wlc_rsdb_get_mimo_cap(wlc_bss_info_t *bi);
extern int wlc_rsdb_ap_bringup(wlc_rsdb_info_t* rsdbinfo, wlc_bsscfg_t** cfg);
extern int wlc_rsdb_ibss_bringup(wlc_info_t* wlc, wlc_bsscfg_t** cfg);
int wlc_rsdb_get_wlcs(wlc_info_t *wlc, wlc_info_t **wlc_2g, wlc_info_t **wlc_5g);
wlc_info_t * wlc_rsdb_get_other_wlc(wlc_info_t *wlc);
int wlc_rsdb_any_wlc_associated(wlc_info_t *wlc);
wlc_info_t* wlc_rsdb_find_wlc_for_chanspec(wlc_info_t *wlc, wlc_bss_info_t *bi,
	chanspec_t chanspec, wlc_info_t *skip_wlc, uint32 match_mask);
wlc_info_t* wlc_rsdb_find_wlc_for_band(wlc_rsdb_info_t *rsdbinfo, enum wlc_bandunit bandunit,
	wlc_info_t *skip_wlc);
uint8 wlc_rsdb_num_wlc_for_band(wlc_rsdb_info_t *rsdbinfo, enum wlc_bandunit bandunit);
wlc_bsscfg_t* wlc_rsdb_cfg_for_chanspec(wlc_info_t *wlc, wlc_bsscfg_t *cfg, wlc_bss_info_t *bi);
int wlc_rsdb_auto_get_wlcs(wlc_info_t *wlc, wlc_info_t **wlc_2g, wlc_info_t **wlc_5g);
void wlc_rsdb_update_wlcif(wlc_info_t *wlc, wlc_bsscfg_t *from, wlc_bsscfg_t *to);
int wlc_rsdb_join_prep_wlc(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint8 *SSID, int len,
	wl_join_scan_params_t *scan_params,
	wl_join_assoc_params_t *assoc_params, int assoc_params_len);
wlc_bsscfg_t* wlc_rsdb_bsscfg_clone(wlc_info_t *from_wlc, wlc_info_t *to_wlc,
	wlc_bsscfg_t *cfg, int *ret);
wlc_rsdb_info_t* wlc_rsdb_attach(wlc_info_t* wlc);
void wlc_rsdb_detach(wlc_rsdb_info_t* rsdbinfo);
bool wlc_rsdb_update_active(wlc_info_t *wlc, bool *old_state);
extern uint16 wlc_rsdb_mode(void *hdl);
bool wlc_rsdb_chkiovar(wlc_info_t *wlc, const bcm_iovar_t  *vi_ptr, uint32 actid, int32 wlc_indx);
struct wlc_if* wlc_rsdb_linked_cfg_chkiovar(wlc_info_t *wlc, const bcm_iovar_t  *vi_ptr,
		uint32 actid, int32 wlc_indx, struct wlc_if *wlcif);
bool wlc_rsdb_is_other_chain_idle(void *hdl);
void wlc_rsdb_force_rsdb_mode(wlc_info_t *wlc);
bool wlc_rsdb_reinit(wlc_info_t *wlc);

#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_RSDB)
void wlc_rsdb_chan_switch_dump(wlc_info_t *wlc, chanspec_t chanspec, uint32 dwell_time);
#endif // endif
wlc_bsscfg_t *wlc_rsdb_bsscfg_get_linked_cfg(wlc_rsdb_info_t *rsdbinfo, wlc_bsscfg_t *cfg);
#if defined(WLSLOTTED_BSS)
wlc_bsscfg_t *wlc_rsdb_bsscfg_create_params(wlc_rsdb_info_t *rsdbinfo,
	wlc_bsscfg_create_param_t *params, int *error);
int wlc_rsdb_bsscfg_delete(wlc_rsdb_info_t *rsdbinfo, wlc_bsscfg_t *bsscfg);
int wlc_rsdb_bsscfg_init(wlc_rsdb_info_t *rsdbinfo, wlc_bsscfg_t *bsscfg);
int wlc_rsdb_bsscfg_deinit(wlc_rsdb_info_t *rsdbinfo, wlc_bsscfg_t *bsscfg);
int wlc_rsdb_bsscfg_enable(wlc_rsdb_info_t *rsdbinfo, wlc_bsscfg_t *bsscfg);
int wlc_rsdb_bsscfg_disable(wlc_rsdb_info_t *rsdbinfo, wlc_bsscfg_t *bsscfg);
int wlc_rsdb_bsscfg_set_wsec(wlc_rsdb_info_t *rsdbinfo, wlc_bsscfg_t *cfg, uint32 val);
#endif /* WLSLOTTED_BSS */
#else /* WLRSDB */
#define wlc_rsdb_mode(hdl)			(PHYMODE_MIMO)
#define wlc_rsdb_any_wlc_associated(wlc)	((wlc)->pub->associated)
#define wlc_rsdb_bsscfg_delete(rsdbinfo, bsscfg)
#define wlc_rsdb_bsscfg_disable(rsdbinfo, bsscfg)
#define WLC_RSDB_GET_PRIMARY_WLC(wlc) (wlc)
#endif /* WLRSDB */
void wlc_rsdb_do_band_lock(wlc_info_t *wlc, bool is_bandswitch);
#ifdef DONGLEBUILD
#if (defined(WLRSDB) && !defined(WLRSDB_DISABLED)) && defined(WL_DUALMAC_RSDB)
#define WLC_DUALMAC_RSDB(cmn) (1)
#define WLC_DUALMAC_RSDB_WRAP(dm_rsdb) (1)
#else
#define WLC_DUALMAC_RSDB(cmn) (0)
#define WLC_DUALMAC_RSDB_WRAP(dm_rsdb) (0)
#endif // endif
#else /* DONGLEBUILD */
#define WLC_DUALMAC_RSDB(cmn)	(cmn->dualmac_rsdb)
#define WLC_DUALMAC_RSDB_WRAP(dm_rsdb) (dm_rsdb)
#endif /* !DONGLEBUILD */

#if defined(WLRSDB) && !defined(WL_DUALNIC_RSDB)&& !defined(WL_DUALMAC_RSDB)
void wlc_rsdb_bmc_smac_template(void *wlc, int tplbuf, uint32 bufsize);
extern void wlc_rsdb_set_phymode(void *hdl, uint32 phymode);
#else
#define wlc_rsdb_bmc_smac_template(hdl, tplbuf, bufsize)  do {} while (0)
#define wlc_rsdb_set_phymode(a, b) do {} while (0)
#endif /* defined(WLRSDB) && !defined(WL_DUALNIC_RSDB) */

extern bool wlc_rsdb_up_allowed(wlc_info_t *wlc);
extern void wlc_rsdb_init_max_rateset(wlc_info_t *wlc, wlc_bss_info_t *bi);
extern int wlc_rsdb_wlc_cmn_attach(wlc_info_t *wlc);
extern void wlc_rsdb_cmn_detach(wlc_info_t *wlc);
extern void wlc_rsdb_config_auto_mode_switch(wlc_info_t *wlc, uint32 reason, uint32 override);
extern int wlc_rsdb_move_connection(wlc_bsscfg_t *bscfg, int8 chain, bool dynswitch);
extern void wlc_rsdb_pm_move_override(wlc_info_t *wlc, bool ovr);
extern void wlc_rsdb_suppress_pending_tx_pkts(wlc_bsscfg_t *from_cfg);
extern wlc_bsscfg_t * wlc_rsdb_get_infra_cfg(wlc_info_t *wlc);
extern bool wlc_rsdb_change_band_allowed(wlc_info_t *wlc, enum wlc_bandunit bandunit);
extern uint8 wlc_rsdb_get_cmn_bwcap(wlc_info_t *wlc, int bandtype);
extern int wlc_rsdb_scb_clone(wlc_bsscfg_t * from_cfg, wlc_bsscfg_t * to_cfg);
extern uint8 wlc_rsdb_any_aps_active(wlc_info_t* wlc);
extern uint8 wlc_rsdb_any_go_active(wlc_info_t* wlc);
int wlc_rsdb_move_scb(wlc_bsscfg_t * from_cfg, wlc_bsscfg_t * to_cfg,
		struct scb *scb, bool suppress);
bool wlc_rsdb_bsscfg_is_linked_cfg(wlc_rsdb_info_t *rsdbinfo, wlc_bsscfg_t *cfg);
extern bool wlc_rsdb_band_match(wlc_info_t *wlc, wlcband_t *band);
#endif /* _wlc_rsdb_h_ */
