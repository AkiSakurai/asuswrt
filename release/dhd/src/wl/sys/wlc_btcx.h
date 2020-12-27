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
 * $Id: wlc_btcx.h 467328 2014-04-03 01:23:40Z $
 */


#ifndef _wlc_btcx_h_
#define _wlc_btcx_h_

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
extern bool wlc_btc_turnoff_aggr(wlc_info_t *wlc);
extern bool wlc_btc_mode_not_parallel(int btc_mode);
extern bool wlc_btc_active(wlc_info_t *wlc);


#endif /* _wlc_btcx_h_ */
