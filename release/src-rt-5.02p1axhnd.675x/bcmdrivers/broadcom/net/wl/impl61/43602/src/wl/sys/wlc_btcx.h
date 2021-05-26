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
 * $Id$
 */

#ifndef _wlc_btcx_h_
#define _wlc_btcx_h_

#ifdef BCMLTECOEX
#include "wlioctl.h"
#endif // endif

#define BTC_RXGAIN_FORCE_OFF            0
#define BTC_RXGAIN_FORCE_2G_MASK        0x1
#define BTC_RXGAIN_FORCE_2G_ON          0x1
#define BTC_RXGAIN_FORCE_5G_MASK        0x2
#define BTC_RXGAIN_FORCE_5G_ON          0x2

/* BTCX_HOSTFLAGS: btc_params 82 */
#define BTCX_HFLG_NO_A2DP_BFR		0x0001  /* no check a2dp buffer */
#define BTCX_HFLG_NO_CCK			0x0002  /* no cck rate for null or cts2self */
#define HFLG_NO_OFDM_FBR			0x0004  /* no ofdm fbr for null or cts2self */
#define BTCX_HFLG_NO_INQ_DEF		0x0008  /* no defer inquery */
#define BTCX_HFLG_GRANT_BT			0x0010  /* always grant bt */
#define BTCX_HFLG_ANT2WL			0x0020  /* always set bt_prisel to wl */
#define BTCX_HFLG_TXDATEN			0x0040  /* allow data Tx scheduling during BT Tx */
#define BTCX_HFLG_PS4ACL			0x0080  /* use ps null for unsniff acl */
#define BTCX_HFLG_DYAGG				0x0100  /* dynamic tx agg durn */

#define BTCX_HFLG_DLNA_MASK		(BTCX_HFLG_ANT2WL | BTCX_HFLG_TXDATEN)
#define BTCX_HFLG_DLNA_DFLT_VAL		0x60
#define BTCX_HFLG_DLNA_TDM_VAL		0x0

/*  ----------- dynamic desense mode switching ---------------- */
#ifdef WL_BTCDYN
/*  #define DYNCTL_DBG */
#ifdef	DYNCTL_DBG
	#define BTCDBG(x) printf x
	#define DBG_BTPWR_HOLES
#else
	#define BTCDBG(x)
#endif /* DYNCTL_DBG */
#define DYNCTL_ERROR(x) printf x

/* simulation of BT activity */
#define IS_DRYRUN_ON(prof)  ((prof->flags & DCTL_FLAGS_DRYRUN) != 0)

/* shm that contains current bt power for each task */
#define M_BTCX_BT_TXPWR	(118 * 2)
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
/* mode switching default hysteresis (in dBm) */
#define BTCDYN_DFLT_BTRSSI_HYSTER	1
/* time threshold to avoid incorrect btpwr readings */
#define DYNCTL_MIN_PERIOD 950

/*  central desense & switching decision function type */
typedef uint8 (* btcx_dynctl_calc_t)(wlc_info_t *wlc, int8 wl_rssi, int8 bt_pwr, int8 bt_rssi);
/* override built-in  dynctl_calc function  with an external one */
extern int btcx_set_ext_desense_calc(wlc_info_t *wlc, btcx_dynctl_calc_t cbfn);
/* override built-in  dynctl mode swithcing with an external one */
extern int btcx_set_ext_mswitch_calc(wlc_info_t *wlc, btcx_dynctl_calc_t cbfn);
/* initialize dynctl profile data with user's provided data (like from nvram.txt */
extern int btcx_init_dynctl_profile(wlc_info_t *wlc,  void *profile_data);
/*  WLC MAC -> notify BTCOEX about chanspec change   */
extern void wlc_btcx_chspec_change_notify(wlc_info_t *wlc, chanspec_t chanspec, bool switchband);
#endif /* WL_BTCDYN */
extern wlc_btc_info_t *wlc_btc_attach(wlc_info_t *wlc);
extern void wlc_btc_detach(wlc_btc_info_t *btc);
extern int wlc_btc_wire_get(wlc_info_t *wlc);
extern int wlc_btc_mode_get(wlc_info_t *wlc);
extern void wlc_enable_btc_ps_protection(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, bool protect);
extern uint wlc_btc_frag_threshold(wlc_info_t *wlc, struct scb *scb);
extern void wlc_btc_mode_sync(wlc_info_t *wlc);
extern uint8 wlc_btc_save_host_requested_pm(wlc_info_t *wlc, uint8 val);
extern bool wlc_btc_get_bth_active(wlc_info_t *wlc);
extern uint16 wlc_btc_get_bth_period(wlc_info_t *wlc);
extern void wlc_btc_4313_gpioctrl_init(wlc_info_t *wlc);
extern void wlc_btcx_read_btc_params(wlc_info_t *wlc);
extern int wlc_btc_params_set(wlc_info_t *wlc, int int_val, int int_val2);
extern int wlc_btc_params_get(wlc_info_t *wlc, int int_val);
extern void wlc_btc_hflg(wlc_info_t *wlc, bool set, uint16 val);
extern uint8 wlc_btcx_get_ba_rx_wsize(wlc_info_t *wlc);
#ifdef BCMDBG
extern void wlc_btc_dump_status(wlc_info_t *wlc,  struct bcmstrbuf *b);
#endif // endif
extern int wlc_btc_siso_ack_set(wlc_info_t *wlc, int16 int_val, bool force);

/* LTE coex data structures */
typedef struct {
	uint8 loopback_type;
	uint8 packet;
	uint16 repeat_ct;
} wci2_loopback_t;

typedef struct {
	uint16 nbytes_tx;
	uint16 nbytes_rx;
	uint16 nbytes_err;
} wci2_loopback_rsp_t;

struct wlc_ltecx_info {
	wlc_info_t	*wlc;
	/* Unused. Keep for ROM compatibility. */
	mws_params_t	mws_params;
	wci2_config_t	mws_config;
};

#ifdef BCMLTECOEX
/* LTE coex functions */
extern wlc_ltecx_info_t *wlc_ltecx_attach(wlc_info_t *wlc);
extern void wlc_ltecx_detach(wlc_ltecx_info_t *ltecx);
extern void wlc_ltecx_init(wlc_hw_info_t *wlc_hw);

/* LTE coex functions */
extern void wlc_ltecx_check_chmap(wlc_info_t *wlc);
extern void wlc_ltecx_set_wlanrx_prot(wlc_hw_info_t *wlc_hw);
extern void wlc_ltecx_update_ltetx_adv(wlc_info_t *wlc_hw);
extern void wlc_ltecx_update_lterx_prot(wlc_info_t *wlc_hw);
extern void wlc_ltecx_update_im3_prot(wlc_info_t *wlc_hw);
extern void wlc_ltecx_scanjoin_prot(wlc_info_t *wlc);
extern void wlc_ltecx_host_interface(wlc_info_t *wlc);
extern void wlc_ltetx_indication(wlc_info_t *wlc);
extern void wlc_ltecx_set_wlanrxpri_thresh(wlc_info_t *wlc);
extern int wlc_ltecx_get_lte_status(wlc_info_t *wlc);
extern int wlc_ltecx_get_lte_map(wlc_info_t *wlc);
extern void wlc_ltecx_update_wl_rssi_thresh(wlc_info_t *wlc);
extern void wlc_ltecx_update_wlanrx_ack(wlc_info_t *wlc);
extern int wlc_ltecx_chk_elna_bypass_mode(wlc_info_t * wlc);
extern void wlc_ltecx_update_status(wlc_info_t *wlc);
extern void wlc_ltecx_wifi_sensitivity(wlc_info_t *wlc);
extern void wlc_ltecx_update_debug_msg(wlc_info_t *wlc);
extern void wlc_ltecx_update_debug_mode(wlc_info_t *wlc);
extern void wlc_ltecx_assoc_in_prog(wlc_info_t *wlc, int val);
#endif /* BCMLTECOEX */

#define BT_RSSI_STEP	5
#define BT_RSSI_OFFSET	10
#define BT_RSSI_INVALID	0
#define BTC_BTRSSI_SIZE    8 /* max num of samples for moving average, must be 2^n */
#define BTC_BTRSSI_MIN_NUM  4 /* min num of samples for moving average */
#define BTC_BTRSSI_THRESH  (-70)
#define BTC_BTRSSI_HYST     2 /* hysteresis of bt rssi in mode switch */
#define BTC_WLRSSI_HYST     2 /* hysteresis of wl rssi in mode switch */

#define BTC_SISOACK_CORES_MASK    0x00FF
#define BTC_SISOACK_TXPWR_MASK    0xFF00
#define BTC_SISOACK_MAXPWR_POS    8
#define BTC_SISOACK_MAXPWR_FLD    0x0F00
#define BTC_SISOACK_CURPWR_POS    12
#define BTC_SISOACK_CURPWR_FLD    0xF000

#endif /* _wlc_btcx_h_ */
