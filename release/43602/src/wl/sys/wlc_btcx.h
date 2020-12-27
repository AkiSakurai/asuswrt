/*
 * BT Coex module interface
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id$
 */


#ifndef _wlc_btcx_h_
#define _wlc_btcx_h_

#ifdef BCMLTECOEX
#include "wlioctl.h"
#endif

#define BT_AMPDU_THRESH		10000	/* if BT period < this threshold, turn off ampdu */

#define BTC_RXGAIN_FORCE_OFF            0
#define BTC_RXGAIN_FORCE_2G_MASK        0x1
#define BTC_RXGAIN_FORCE_2G_ON          0x1
#define BTC_RXGAIN_FORCE_5G_MASK        0x2
#define BTC_RXGAIN_FORCE_5G_ON          0x2

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

#endif /* _wlc_btcx_h_ */
