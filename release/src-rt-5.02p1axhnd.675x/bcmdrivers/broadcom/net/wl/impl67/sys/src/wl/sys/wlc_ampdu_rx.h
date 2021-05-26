/*
 * A-MPDU Rx (with extended Block Ack) related header file
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
 * $Id: wlc_ampdu_rx.h 785800 2020-04-07 07:22:55Z $
*/

#ifndef _wlc_ampdu_rx_h_
#define _wlc_ampdu_rx_h_

#if defined(WL_LINKSTAT)
#include <wlc_linkstats.h>
#endif // endif

extern uint32 wlc_ampdu_ulmu_rxwinsize(wlc_info_t *wlc);
extern ampdu_rx_info_t *wlc_ampdu_rx_attach(wlc_info_t *wlc);
extern void wlc_ampdu_rx_detach(ampdu_rx_info_t *ampdu_rx);
extern void scb_ampdu_rx_flush(ampdu_rx_info_t *ampdu_rx, struct scb *scb);

extern int wlc_ampdu_rx_set(ampdu_rx_info_t *ampdu_rx, bool on);
extern void wlc_ampdu_rx_set_bsscfg_aggr_override(ampdu_rx_info_t *ampdu_rx,
	wlc_bsscfg_t *bsscfg, int8 rxaggr);
extern void wlc_ampdu_rx_set_bsscfg_aggr(ampdu_rx_info_t *ampdu_rx, wlc_bsscfg_t *bsscfg,
	bool rxaggr, uint16 conf_TID_bmap);
extern void wlc_ampdu_shm_upd(ampdu_rx_info_t *ampdu_rx);

extern void ampdu_cleanup_tid_resp(ampdu_rx_info_t *ampdu_rx, struct scb *scb,
	uint8 tid);

extern void wlc_ampdu_recvdata(ampdu_rx_info_t *ampdu_rx, struct scb *scb, struct wlc_frminfo *f);

extern void wlc_ampdu_clear_rx_dump(ampdu_rx_info_t *ampdu_rx);
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
extern int wlc_ampdu_rx_dump(ampdu_rx_info_t *ampdu_rx, struct bcmstrbuf *b);
#endif /* BCMDBG || WLTEST */

extern int wlc_send_addba_resp(wlc_info_t *wlc, struct scb *scb, uint16 status,
	uint8 token, uint16 timeout, uint16 param_set);
extern void wlc_ampdu_recv_addba_req_resp(ampdu_rx_info_t *ampdu_rx, struct scb *scb,
	dot11_addba_req_t *addba_req, int body_len);
extern void wlc_ampdu_recv_bar(ampdu_rx_info_t *ampdu_rx, struct scb *scb, uint8 *body,
	int body_len);

#define AMPDU_RESP_NO_BAPOLICY_TIMEOUT	3	/* # of sec rcving ampdu wo bapolicy */

extern uint8 wlc_ampdu_get_rx_factor(wlc_info_t *wlc);
extern void wlc_ampdu_update_rx_factor(wlc_info_t *wlc, int vht_he_mode);
extern void wlc_ampdu_update_ie_param(ampdu_rx_info_t *ampdu_rx);

extern uint8 wlc_ampdu_rx_get_mpdu_density(ampdu_rx_info_t *ampdu_rx);
extern void wlc_ampdu_rx_set_mpdu_density(ampdu_rx_info_t *ampdu_rx, uint8 mpdu_density);
extern void wlc_ampdu_rx_set_ba_rx_wsize(ampdu_rx_info_t *ampdu_rx, uint8 wsize);
extern uint8 wlc_ampdu_rx_get_ba_rx_wsize(ampdu_rx_info_t *ampdu_rx);
extern uint8 wlc_ampdu_rx_get_ba_max_rx_wsize(ampdu_rx_info_t *ampdu_rx);
extern void wlc_ampdu_rx_recv_delba(ampdu_rx_info_t *ampdu_rx, struct scb *scb, uint8 tid,
	uint8 category, uint16 initiator, uint16 reason);
extern void wlc_ampdu_rx_send_delba(ampdu_rx_info_t *ampdu_rx, struct scb *scb, uint8 tid,
	uint16 initiator, uint16 reason);
extern int wlc_ampdu_rx_queued_pkts(ampdu_rx_info_t * ampdu_rx,
	struct scb * scb, int tid);
extern void wlc_ampdu_rx_dump_queued_pkts(ampdu_rx_info_t * ampdu_rx,
	struct scb * scb, int tid, struct bcmstrbuf *b);
#if defined(PKTC) || defined(PKTC_DONGLE)
extern bool wlc_ampdu_chainable(ampdu_rx_info_t *ampdu_rx, void *p, struct scb *scb,
	uint16 seq, uint16 tid);
#endif // endif

void wlc_ampdu_update_rxcounters(ampdu_rx_info_t *ampdu_rx, uint32 ft_fmt, struct scb *scb,
	uint8 *plcp, void *p, uint8 prio, d11rxhdr_t *rxh, ratespec_t rspec, bool is_first_mpdu);

extern void wlc_ampdu_agg_state_update_rx_all(wlc_info_t *wlc, bool aggr);

#if defined(WL_LINKSTAT)
void wlc_ampdu_rxrates_get(ampdu_rx_info_t *ampdu_rx, wifi_rate_stat_t *rate, int i, int rs);
#endif // endif
extern bool wlc_scb_ampdurx_on(struct scb *scb);
extern bool wlc_scb_ampdurx_on_tid(struct scb *scb, uint8 prio);

#ifdef WLCFP
extern void* wlc_cfp_get_ampdu_rx_cubby(wlc_info_t *wlc, struct scb *scb);
#define GET_AMPDU_RX_CUBBY(wlc, scb)	\
	wlc_cfp_get_ampdu_rx_cubby(wlc, scb)

extern bool wlc_cfp_ampdu_chainable(wlc_info_t *wlc, void* scb_ampdu, uint16 seq, uint16 tid);
#endif // endif

#if defined(BCMHWA) && defined(HWA_RXFILL_BUILD)
extern void wlc_ampdu_rx_hwa_rxbm_suppress(wlc_info_t *wlc);
extern void wlc_ampdu_rx_flush_all(wlc_info_t *wlc);
#endif // endif
extern bool wlc_ampdu_block_acked_frame(wlc_info_t *wlc, d11rxhdr_t *rxh, uint8 *plcp);
extern void wlc_ampdu_ulmu_reqbytes_upd(ampdu_rx_info_t *ampdu_rx, scb_t *scb);
extern uint32 wlc_ampdu_ulmu_reqbytes_get(ampdu_rx_info_t *ampdu_rx, scb_t *scb);

#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
#endif // endif

#endif /* _wlc_ampdu_rx_h_ */
