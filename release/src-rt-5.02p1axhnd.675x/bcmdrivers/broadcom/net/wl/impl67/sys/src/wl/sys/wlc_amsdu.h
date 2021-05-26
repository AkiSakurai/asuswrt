/*
 * MSDU aggregation related header file
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
 * $Id: wlc_amsdu.h 787762 2020-06-11 04:57:56Z $
*/

#ifndef _wlc_amsdu_h_
#define _wlc_amsdu_h_

/* A-MSDU policy limits */

extern amsdu_info_t *wlc_amsdu_attach(wlc_info_t *wlc);
extern void wlc_amsdu_detach(amsdu_info_t *ami);
extern bool wlc_amsdu_tx_cap(amsdu_info_t *ami);
extern bool wlc_amsdurx_cap(amsdu_info_t *ami);
extern uint wlc_amsdu_mtu_get(amsdu_info_t *ami); /* amsdu MTU, depend on rx fifo limit */
extern void wlc_amsdu_flush(amsdu_info_t *ami);
extern void *wlc_recvamsdu(amsdu_info_t *ami, wlc_d11rxhdr_t *wrxh, void *p, uint16 *padp,
		bool chained_sendup);
extern void wlc_amsdu_deagg_hw(amsdu_info_t *ami, struct scb *scb,
	struct wlc_frminfo *f);
#ifdef WLAMSDU_SWDEAGG
extern void wlc_amsdu_deagg_sw(amsdu_info_t *ami, struct scb *scb,
	struct wlc_frminfo *f);
#endif /* SWDEAGG */

#ifdef WLAMSDU_TX
extern int wlc_amsdu_set(amsdu_info_t *ami, bool val);
extern void wlc_amsdu_agglimit_frag_upd(amsdu_info_t *ami);
extern void wlc_amsdu_txop_upd(amsdu_info_t *ami);
extern void wlc_amsdu_scb_agglimit_calc(amsdu_info_t *ami, struct scb *scb);
extern void wlc_amsdu_tx_scb_set_max_agg_size(amsdu_info_t *ami, struct scb *scb, uint16 n_bytes);
extern void wlc_amsdu_txpolicy_upd(amsdu_info_t *ami);
#ifdef WL11AC
extern void wlc_amsdu_scb_vht_agglimit_upd(amsdu_info_t *ami, struct scb *scb);
#endif /* WL11AC */
extern void wlc_amsdu_scb_ht_agglimit_upd(amsdu_info_t *ami, struct scb *scb);
extern void wlc_amsdu_agg_flush(amsdu_info_t *ami);
extern uint wlc_amsdu_bss_txpktcnt(amsdu_info_t *ami, wlc_bsscfg_t *bsscfg);
extern bool wlc_amsdu_chk_priority_enable(amsdu_info_t *ami, uint8 tid);
extern void wlc_amsdu_tx_scb_aggsf_upd(amsdu_info_t *ami, struct scb *scb);
extern void wlc_amsdu_tx_scb_enable(wlc_info_t *wlc, struct scb *scb);
extern void wlc_amsdu_tx_scb_disable(wlc_info_t *wlc, struct scb *scb);
#else
#define wlc_amsdu_tx_scb_enable(a, b)
#define wlc_amsdu_tx_scb_disable(a, b)
#endif /* WLAMSDU_TX */

#if defined(PKTC) || defined(PKTC_TX_DONGLE)
extern void *wlc_amsdu_pktc_agg(amsdu_info_t *ami, struct scb *scb, void *p,
	void *n, uint8 tid, uint32 lifetime);
#endif // endif
#if defined(PKTC) || defined(PKTC_DONGLE)
extern int32 wlc_amsdu_pktc_deagg_hw(amsdu_info_t *ami, void **pp, wlc_rfc_t *rfc,
        uint16 *index, bool *chained_sendup, struct scb *scb, uint16 sec_offset);
#endif /* defined(PKTC) || defined(PKTC_TX_DONGLE) */

extern bool wlc_amsdu_is_rxmax_valid(amsdu_info_t *ami);

uint8 wlc_amsdu_tx_queued_pkts(amsdu_info_t *ami, struct scb * scb, int tid);

#ifdef WLCFP
extern void* wlc_cfp_get_amsdu_cubby(wlc_info_t *wlc, struct scb *scb);
#define GET_AMSDU_CUBBY(wlc, scb) \
	wlc_cfp_get_amsdu_cubby(wlc, scb)
extern void * wlc_cfp_recvamsdu(amsdu_info_t *ami, wlc_d11rxhdr_t *wrxh, void *p,
	bool* chainable, struct scb *scb);
extern void wlc_cfp_amsdu_tx_counter_histogram_upd(wlc_info_t *wlc, uint32 nmsdu, uint32 totlen);
#if !defined(BCMDBG) && !defined(BCMDBG_AMSDU)
extern void wlc_cfp_amsdu_tx_counter_upd(wlc_info_t *wlc, uint32 tot_nmsdu, uint32 tot_namsdu);
#endif /* !BCMDBG && !BCMDBG_AMSDU */

#if defined(DONGLEBUILD)
#if defined(BCMDBG) || defined(BCMDBG_AMSDU)
extern void wlc_cfp_amsdu_rx_histogram_upd(wlc_info_t *wlc, uint16 msdu_count, uint16 amsdu_bytes);
#endif /* BCMDBG || BCMDBG_AMSDU */
#else /* ! DONGLEBUILD */
extern int32 wlc_cfp_amsdu_deagg_hw(amsdu_info_t *ami, void *p, uint32 *index, struct scb *scb);
#endif /* ! DONGLEBUILD */

#endif /* WLCFP */

#ifdef WL11K_ALL_MEAS
extern void wlc_amsdu_get_stats(wlc_info_t *wlc, rrm_stat_group_11_t *g11);
#endif /* WL11K_ALL_MEAS */

extern bool wlc_rx_amsdu_in_ampdu_override(amsdu_info_t *ami);

extern uint8 wlc_amsdu_scb_max_sframes(amsdu_info_t *ami, struct scb *scb);
extern uint32 wlc_amsdu_scb_max_agg_len(amsdu_info_t *ami, struct scb *scb, uint8 tid);
extern uint32 wlc_amsdu_scb_aggsf(amsdu_info_t *ami, struct scb *scb, uint8 tid);
extern void wlc_amsdu_max_agg_len_upd(amsdu_info_t *ami);
#ifdef	WLCFP
extern uint8 wlc_amsdu_agg_attempt(amsdu_info_t *ami, struct scb *scb, void *p, uint8 tid);
#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
extern void wlc_amsdu_subframe_insert(wlc_info_t* wlc, void* head);
#endif /* BCM_DHDHDR && DONGLEBUILD */
#endif /* WLCFP */

#if defined(HWA_PKTPGR_BUILD)
extern void wlc_amsdu_single_txlfrag_fixup(wlc_info_t *wlc, void *head);
#endif // endif

#endif /* _wlc_amsdu_h_ */
