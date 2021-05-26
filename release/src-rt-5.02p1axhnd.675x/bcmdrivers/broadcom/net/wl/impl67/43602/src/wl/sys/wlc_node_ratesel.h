/*
 * Net80211 rate selection algorithm wrapper of Broadcom
 * algorithm of Broadcom 802.11b DCF-only Networking Adapter.
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
 * $Id: wlc_node_ratesel.h 453919 2014-02-06 23:10:30Z $
 */

#ifndef	_WLC_NODE_RATESEL_H_
#define	_WLC_NODE_RATESEL_H_

#include <wlc_rate_sel.h>
struct ieee80211_node;
struct ieee80211_key;
struct ieee80211_txparam;
struct ieee80211_channel;
struct ieee80211com;
struct wmeParams;
struct mbuf;

extern wlc_ratesel_info_t *wlc_node_ratesel_attach(wlc_info_t *wlc);
extern void wlc_node_ratesel_detach(wlc_ratesel_info_t *wrsi);

/* select transmit rate given per-scb state */
extern void wlc_node_ratesel_gettxrate(wlc_ratesel_info_t *wrsi, void *node_ratesel,
	uint16 *frameid, ratesel_txparams_t *cur_rate, uint16 *flags);

/* update per-scb state upon received tx status */
extern void wlc_node_ratesel_upd_txstatus_normalack(wlc_ratesel_info_t *wrsi, void *node_ratesel,
	tx_status_t *txs, uint16 sfbl, uint16 lfbl,
	uint8 mcs, uint8 antselid, bool fbr);

#ifdef WL11N
/* change the throughput-based algo parameters upon ACI mitigation state change */
extern void wlc_node_ratesel_aci_change(wlc_ratesel_info_t *wrsi, bool aci_state);

/* update per-scb state upon received tx status for ampdu */
extern void wlc_node_ratesel_upd_txs_blockack(wlc_ratesel_info_t *wrsi, void *node_ratesel,
	tx_status_t *txs, uint8 suc_mpdu, uint8 tot_mpdu,
	bool ba_lost, uint8 retry, uint8 fb_lim, bool tx_error,
	uint8 mcs, uint8 antselid);

#ifdef WLAMPDU_MAC
extern void wlc_node_ratesel_upd_txs_ampdu(wlc_ratesel_info_t *wrsi, void *node_ratesel,
	uint16 frameid, uint8 mrt, uint8 mrt_succ, uint8 fbr, uint8 fbr_succ,
	bool tx_error, uint8 tx_mcs, uint8 antselid);
#endif // endif

/* update rate_sel if a PPDU (ampdu or a reg pkt) is created with probe values */
extern void wlc_node_ratesel_probe_ready(wlc_ratesel_info_t *wrsi, void *node_ratesel,
	uint16 frameid, bool is_ampdu, uint8 ampdu_txretry);

extern void wlc_node_ratesel_upd_rxstats(wlc_ratesel_info_t *wrsi, ratespec_t rx_rspec,
	uint16 rxstatus2);

/* get the fallback rate of the specified mcs rate */
extern ratespec_t wlc_node_ratesel_getmcsfbr(wlc_ratesel_info_t *wrsi, void *node_ratesel,
	uint16 frameid, uint8 mcs);
#endif /* WL11N */

extern bool wlc_node_ratesel_minrate(wlc_ratesel_info_t *wrsi, void *node_ratesel,
	tx_status_t *txs);

extern void wlc_node_ratesel_init(wlc_info_t *wlc, void *node_ratesel);
extern void wlc_scb_ratesel_init_all(wlc_info_t *wlc);

extern void *wlc_ratesel_node_alloc(wlc_info_t *wlc);
extern void wlc_ratesel_node_free(wlc_info_t *wlc, void *node_ratesel);
extern void wlc_ratesel_init_node(wlc_info_t *wlc, struct ieee80211_node *ni,
	void *node_ratesel, wlc_rateset_t *rateset);

#endif	/* _WLC_NODE_RATESEL_H_ */
