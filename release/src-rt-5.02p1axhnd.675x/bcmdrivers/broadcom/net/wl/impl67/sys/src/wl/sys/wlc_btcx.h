/*
 * BT Coex module interface
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
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_btcx.h 783333 2020-01-23 21:23:05Z $
 */

#ifndef _wlc_btcx_h_
#define _wlc_btcx_h_

#define BTC_RXGAIN_FORCE_OFF            0
#define BTC_RXGAIN_FORCE_2G_MASK        0x1
#define BTC_RXGAIN_FORCE_2G_ON          0x1
#define BTC_RXGAIN_FORCE_5G_MASK        0x2
#define BTC_RXGAIN_FORCE_5G_ON          0x2

/* 4364 - GCI chip ctrl prisel/Txconf mask */
#define GCI_CHIP_CTRL_PRISEL_MASK		(1 << 15 | 1 << 16)
#define GCI_CHIP_CTRL_PRISEL_MASK_CORE0		(1 << 15)	/* 4364: 3x3 core */
#define GCI_CHIP_CTRL_PRISEL_MASK_CORE1		(1 << 16)	/* 4364: 1x1 core */

/* 4347 - GCI chip ctrl prisel/Txconf mask */
#define GCI_CHIP_CTRL_PRISEL_MASK_4347       (1 << 27 | 1 << 28)
#define GCI_CHIP_CTRL_PRISEL_MASK_CORE0_4347     (1 << 27)   /* 4347: core 0 */
#define GCI_CHIP_CTRL_PRISEL_MASK_CORE1_4347     (1 << 28)   /* 4347: core 1 */

/* WLAN UP[only 2G] indication to BT via GCI - bit 34 */
#define GCI_WL_2G_UP_INFO	0x0008
#define GCI_WL_RESET_2G_UP_INFO 0x0

/* WLAN UP[only 5G] indication to BT via GCI - bit 35 */
#define GCI_WL_5G_UP_INFO	0x0004
#define GCI_WL_RESET_5G_UP_INFO 0x0
/* WLAN active[only 5G] indication to BT via GCI - bit 57 */
#define GCI_WL_5G_ACTIVE_INFO	(1 << 25)
#define GCI_WL_RESET_5G_ACTIVE_INFO 0x0
/* COEX IO_MASK block */
typedef enum {
	COEX_IOMASK_TXCONF_POS = 0,
	COEX_IOMASK_PRISEL_POS = 1,
	COEX_IOMASK_WLPRIO_POS = 4,
	COEX_IOMASK_WLTXON_POS = 5
} coex_io_mask_t;

/* Definitions for SW controlled GPIO coex. Host writes to ECI shadow copies in ucode shm */
/* Type is cached in ECI1 */
#define BT_TYPE_LENGTH			6
#define BT_TYPE_MASK			0x3f
#define BT_TYPE_OFFSET			0
#define BT_TYPE_ADDITIONAL_OFFSET	7
/* Dur & Period are cached in ECI2 */
#define BT_DUR_OFFSET			0
#define BT_PERIOD_OFFSET		8
#define BT_PERIOD_LENGTH		6
#define BT_BLE_SCAN_DUR_OFFSET		9
/* A2DP buffer len is cached in ECI1 */
#define BT_A2DP_BUFFER_OFFSET		8
#define BT_A2DP_BUFFER_LENGTH		4
/* BT Master bit is cached in ECI2 */
#define BT_MASTER_OFFSET		15
/* MLPRIO info is cached in ECI1 */
#define BT_MLPRIO_SYNC_OFFSET		6
#define BT_MLPRIO_HIDACK_OFFSET		7
/* BLE event info is cached in ECI1 */
#define BT_BLE_EVENT_OFFSET		8	/* Additional BLE event info */
/* LEA fields are cached in ECI1 */
#define BT_LEA_NLNKS_OFFSET		13	/* Number of LEA links (0/1/2) */
#define BT_LEA_NLNKS_NBITS		2
#define BT_LEA_TYPE_OFFSET		11	/* !LEA/LEA_1/LEA_2 (0/1/2) */
#define BT_LEA_TYPE_NBITS		2
/* Multi data info and LE Scan Priority are cached in ECI2 */
#define BT_MULTI_DATA_OFFSET		5
#define LE_SCAN_PRIORITY_BIT_OFFSET	14

extern wlc_btc_info_t *wlc_btc_attach(wlc_info_t *wlc);
extern void wlc_btc_init(wlc_info_t *wlc);
extern void wlc_btc_detach(wlc_btc_info_t *btc);
extern int wlc_btc_wire_get(wlc_info_t *wlc);
extern int wlc_btc_mode_get(wlc_info_t *wlc);
extern int wlc_btc_task_set(wlc_info_t *wlc, int int_val);
extern void wlc_btc_set_ps_protection(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);
extern uint wlc_btc_frag_threshold(wlc_info_t *wlc, struct scb *scb);
extern void wlc_btc_mode_sync(wlc_info_t *wlc);
extern uint8 wlc_btc_save_host_requested_pm(wlc_info_t *wlc, uint8 val);
extern bool wlc_btc_get_bth_active(wlc_info_t *wlc);
extern uint16 wlc_btc_get_bth_period(wlc_info_t *wlc);
extern void wlc_btcx_read_btc_params(wlc_info_t *wlc);
extern int wlc_btc_params_set(wlc_info_t *wlc, int int_val, int int_val2);
extern int wlc_btc_params_get(wlc_info_t *wlc, int int_val);
extern bool wlc_btc_turnoff_aggr(wlc_info_t *wlc);
extern bool wlc_btc_mode_not_parallel(int btc_mode);
extern bool wlc_btc_active(wlc_info_t *wlc);
extern bool wlc_btcx_pri_activity(wlc_info_t *wlc);
extern void wlc_btcx_upd_gci_chanspec_indications(wlc_info_t *wlc);
extern void wlc_btcx_active_in_5g(wlc_info_t *wlc);
#ifdef WLRSDB
extern void wlc_btcx_update_coex_iomask(wlc_info_t *wlc);
#endif /* WLRSDB */
extern int wlc_btc_siso_ack_set(wlc_info_t *wlc, int16 int_val, bool force);
extern void wlc_btc_hflg(wlc_info_t *wlc, bool set, uint16 val);
extern void wlc_btc_ampdu_aggr(wlc_info_t *wlc, struct scb *scb);
extern void wlc_btcx_ltecx_apply_agg_state(wlc_info_t *wlc);

/* BTCOEX profile data structures */
#define BTC_SUPPORT_BANDS	2

#define BTC_PROFILE_2G		0
#define BTC_PROFILE_5G		1

#define BTC_PROFILE_OFF		0
#define BTC_PROFILE_DISABLE 1
#define BTC_PROFILE_ENABLE	2

#define MAX_BT_DESENSE_LEVELS	8
#define BTC_WL_RSSI_DEFAULT -70
#define BTC_BT_RSSI_DEFAULT -70
#define BTC_WL_RSSI_HYST_DEFAULT_4350 5
#define BTC_BT_RSSI_HYST_DEFAULT_4350 5
#define MAX_BT_DESENSE_LEVELS_4350 2
#define BTC_WL_MAX_SISO_RESP_POWER_TDD_DEFAULT 127
#define BTC_WL_MAX_SISO_RESP_POWER_HYBRID_DEFAULT 127
#define BTC_WL_MAX_SISO_RESP_POWER_TDD_DEFAULT_4350 8
#define BTC_WL_MAX_SISO_RESP_POWER_HYBRID_DEFAULT_4350 16

/* WLBTCPROF info */
struct wlc_btc_profile {
	uint32 mode;
	uint32 desense;
	int desense_level;
	int desense_thresh_high;
	int desense_thresh_low;
	uint32 num_chains;
	uint32 chain_ack[WL_NUM_TXCHAIN_MAX];
	int chain_power_offset[WL_NUM_TXCHAIN_MAX];
	int8 btc_wlrssi_thresh;
	uint8 btc_wlrssi_hyst;
	int8 btc_btrssi_thresh;
	uint8 btc_btrssi_hyst;
	uint8 btc_num_desense_levels;
	uint8 btc_siso_resp_en[MAX_BT_DESENSE_LEVELS];
	int8 btc_max_siso_resp_power[MAX_BT_DESENSE_LEVELS];
};
typedef struct wlc_btc_profile wlc_btc_profile_t;

struct wlc_btc_prev_connect {
	int prev_band;
	int prev_2G_mode;
	int prev_5G_mode;
	struct wlc_btc_profile prev_2G_profile;
	struct wlc_btc_profile prev_5G_profile;
};
typedef struct wlc_btc_prev_connect wlc_btc_prev_connect_t;

struct wlc_btc_select_profile {
	int enable;
	struct wlc_btc_profile select_profile;
};
typedef struct wlc_btc_select_profile wlc_btc_select_profile_t;

/*  ----------- dynamic desense mode switching ---------------- */

/* #define DYNCTL_DBG */
#ifdef	DYNCTL_DBG
	#define BTCDBG(x) printf x
	#define DBG_BTPWR_HOLES
#else
	#define BTCDBG(x)
#endif /* DYNCTL_DBG */
#define DYNCTL_ERROR(x) printf x

/* simulation of BT activity */
#define IS_DRYRUN_ON(prof)  ((prof->flags & DCTL_FLAGS_DRYRUN) != 0)

#define SHM_BTC_MASK_TXPWR			0X7
/*  bit-field position in shm word */
#define SHM_BTC_SHFT_TXPWR_SCO		0
#define SHM_BTC_SHFT_TXPWR_A2DP		3
#define SHM_BTC_SHFT_TXPWR_SNIFF	6
#define SHM_BTC_SHFT_TXPWR_ACL		9
/*
	BT uses 3 bits to report current Tx power
	0 = invalid (no connection), 1 <= -12dBm, 2 = -8dBm,..., 6 = 8dBm, 7 >= 12dBm
	TxPwr = 3bitValue * TxPwrStep + TxPwrOffset, where TxPwrStep = 4 and TxPwrOffset = -16
*/
#define BT_TX_PWR_STEP				4
#define BT_TX_PWR_OFFSET			(-16)
#define BT_INVALID_TX_PWR			-127
#define BTCDYN_DFLT_BTRSSI_HYSTER	1
/* time threshold to avoid incorrect btpwr readings */
#define DYNCTL_MIN_PERIOD 950

/* dynamic tx power control based on RSSI for hybrid coex */
#define DYN_PWR_MAX_STEPS       8
#define DYN_PWR_DFLT_STEPS      6
#define DYN_PWR_DFLT_QDBM       127

typedef enum {
	C_BTCX_HFLG_NO_A2DP_BFR_NBIT	= 0, /* no check a2dp buffer */
	C_BTCX_HFLG_NO_CCK_NBIT			= 1, /* no cck rate for null or cts2self */
	C_BTCX_HFLG_NO_OFDM_FBR_NBIT	= 2, /* no ofdm fbr for null or cts2self */
	C_BTCX_HFLG_NO_INQ_DEF_NBIT		= 3, /* no defer inquery */
	C_BTCX_HFLG_GRANT_BT_NBIT		= 4, /* always grant bt */
	C_BTCX_HFLG_ANT2WL_NBIT			= 5, /* always prisel to wl */
	C_BTCX_HFLG_TXDATEN_NBIT		= 6, /* If dlna Allow Wlan TX during BT Tx slot */
	C_BTCX_HFLG_PS4ACL_NBIT			= 7, /* use ps null for unsniff acl */
	C_BTCX_HFLG_DYAGG_NBIT			= 8, /* dynamic tx agg durn */
	C_BTCX_COEX_NOA_SUPPORT_NBIT		= 11, /* enables COEX NOA support */
	C_BTCX_HFLG_PU_OFF_TPC_HOLD		= 12, /* enables TPC loop hold when PU is off */
	C_BTCX_HFLG_BLE_GRANT_NBIT		= 13, /* BLE scan grant during A2DP bit */
} shm_btcx_hflg_t;

#define BTCX_HFLG_DLNA_MASK		(1 << C_BTCX_HFLG_ANT2WL_NBIT)
#define BTCX_HFLG_DLNA_DFLT_VAL		0x20
#define BTCX_HFLG_DLNA_TDM_VAL		0x0

/*  central desense & switching decision function type */
typedef uint8 (* btcx_dynctl_calc_t)(wlc_info_t *wlc, int8 wl_rssi, int8 bt_pwr, int8 bt_rssi);
#ifdef WL_BTCDYN
/* override built-in  dynctl_calc function  with an external one */
extern int btcx_set_ext_desense_calc(wlc_info_t *wlc, btcx_dynctl_calc_t cbfn);
/* override built-in  dynctl mode swithcing with an external one */
extern int btcx_set_ext_mswitch_calc(wlc_info_t *wlc, btcx_dynctl_calc_t cbfn);
/* initialize dynctl profile data with user's provided data (like from nvram.txt */
extern int btcx_init_dynctl_profile(wlc_info_t *wlc,  void *profile_data);
/*  WLC MAC -> notify BTCOEX about chanspec change   */
extern void wlc_btcx_chspec_change_notify(wlc_info_t *wlc, chanspec_t chanspec);
#endif /* WL_BTCDYN */

#ifdef WLBTCPROF

#define WL_BTCPROF WL_INFORM

extern int wlc_btc_profile_set(wlc_info_t *wlc, int int_val, int iovar_id);
extern int wlc_btc_profile_get(wlc_info_t *wlc, int iovar_id);
extern int wlc_btcx_select_profile_set(wlc_info_t *wlc, uint8 *pref, int len);
extern int wlc_btcx_select_profile_get(wlc_info_t *wlc, uint8 *pref, int len);
extern int wlc_btcx_set_btc_profile_param(struct wlc_info *wlc, chanspec_t chanspec, bool force);
extern int wlc_btcx_set_ext_profile_param(wlc_info_t *wlc);
#else
#define WL_BTCPROF WL_NONE
#endif /* WLBTCPROF */
#ifdef WL_UCM
extern void wlc_stf_arb_btc_2g_call_back(void *ctx, uint8 txc_active, uint8 rxc_active);
extern int wlc_btc_2g_chain_request_apply(wlc_info_t *wlc);
extern int wlc_btc_chain_request_cancel(wlc_info_t *wlc);
extern void wlc_btc_rxchain_update(wlc_info_t *wlc);
#endif /* WL_UCM */

extern void wlc_btc_ble_grants_enable(wlc_btc_info_t *btc, bool on);
extern void wlc_btc_ble_grants_re_enable(wlc_info_t *wlc);
#ifdef WL_UCM
#define UCM_TRACE WL_INFORM
extern int wlc_btc_apply_profile(struct wlc_info *wlc, wlc_bsscfg_t *cfg);
extern void wlc_btc_get_matched_chanspec_bsscfg(wlc_info_t *wlc,
       chanspec_t chanspec, wlc_bsscfg_t **cfg);
#else
#define UCM_TRACE WL_NONE
#endif /* WL_UCM */

extern uint8 wlc_btcx_get_ba_rx_wsize(wlc_info_t *wlc);
#endif /* _wlc_btcx_h_ */
