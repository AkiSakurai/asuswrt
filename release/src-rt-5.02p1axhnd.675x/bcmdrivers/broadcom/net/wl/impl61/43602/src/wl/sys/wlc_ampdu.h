/*
 * A-MPDU Tx (with extended Block Ack) related header file
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
 * $Id: wlc_ampdu.h 676784 2016-12-24 16:10:17Z $
*/

#ifndef _wlc_ampdu_tx_h_
#define _wlc_ampdu_tx_h_

#ifdef WLTAF
#include <wlc_taf.h>
#else
typedef void taf_scheduler_public_t;
#endif /* WLTAF */

typedef struct scb_ampdu_tx scb_ampdu_tx_t;
typedef struct scb_ampdu_tid_ini scb_ampdu_tid_ini_t;

extern ampdu_tx_info_t *wlc_ampdu_tx_attach(wlc_info_t *wlc);
extern void wlc_ampdu_tx_detach(ampdu_tx_info_t *ampdu_tx);
extern int wlc_sendampdu(ampdu_tx_info_t *ampdu_tx, wlc_txq_info_t *qi, void **aggp, int prec);
extern bool wlc_ampdu_dotxstatus(ampdu_tx_info_t *ampdu_tx, struct scb *scb, void *p,
	tx_status_t *txs, wlc_txh_info_t *txh_info);
extern void wlc_ampdu_dotxstatus_regmpdu(ampdu_tx_info_t *ampdu_tx, struct scb *scb, void *p,
	tx_status_t *txs, wlc_txh_info_t *txh_info);
extern void wlc_ampdu_tx_reset(ampdu_tx_info_t *ampdu_tx);
extern void wlc_ampdu_macaddr_upd(wlc_info_t *wlc);
extern uint8 wlc_ampdu_null_delim_cnt(ampdu_tx_info_t *ampdu_tx, struct scb *scb,
	ratespec_t rspec, int phylen, uint16* minbytes);
extern bool wlc_ampdu_frameburst_override(ampdu_tx_info_t *ampdu_tx);
#ifdef WLC_HIGH_ONLY
extern void wlc_ampdu_txstatus_complete(ampdu_tx_info_t *ampdu_tx, uint32 s1, uint32 s2);
#endif // endif
extern void wlc_ampdu_tx_set_bsscfg_aggr_override(ampdu_tx_info_t *ampdu_tx,
	wlc_bsscfg_t *bsscfg, int8 txaggr);
extern uint16 wlc_ampdu_tx_get_bsscfg_aggr(ampdu_tx_info_t *ampdu_tx, wlc_bsscfg_t *bsscfg);
extern void wlc_ampdu_tx_set_bsscfg_aggr(ampdu_tx_info_t *ampdu_tx, wlc_bsscfg_t *bsscfg,
	bool txaggr, uint16 conf_TID_bmap);
extern void wlc_ampdu_ini_adjust(ampdu_tx_info_t *ampdu_tx, struct scb *scb, void *p);
extern void wlc_ampdu_dec_tx_in_transit(ampdu_tx_info_t *ampdu_tx, struct scb *scb, void *p);

extern bool wlc_ampdu_tx_cap(ampdu_tx_info_t *ampdu_tx);
extern int wlc_ampdumac_set(ampdu_tx_info_t *ampdu_tx, uint8 on);

#ifdef WLAMPDU_MAC
#define AMU_EPOCH_NO_CHANGE		-1	/* no-epoch change but change params */
#define AMU_EPOCH_CHG_PLCP		0	/* epoch change due to plcp */
#define AMU_EPOCH_CHG_FID		1	/* epoch change due to rate flag in frameid */
#define AMU_EPOCH_CHG_NAGG		2	/* epoch change due to ampdu off */
#define AMU_EPOCH_CHG_MPDU		3	/* epoch change due to mpdu */
#define AMU_EPOCH_CHG_DSTTID		4	/* epoch change due to dst/tid */
#define AMU_EPOCH_CHG_SEQ		5	/* epoch change due to discontinuous seq no */
#define AMU_EPOCH_CHG_TXC_UPD  6    /* Epoch change due to txcache update  */
#define AMU_EPOCH_CHG_TXHDR    7    /* Epoch change due to Long hdr to short hdr transition */

extern void wlc_ampdu_change_epoch(ampdu_tx_info_t *ampdu_tx, int fifo, int reason_code);
extern uint8 wlc_ampdu_chgnsav_epoch(ampdu_tx_info_t *, int fifo,
	int reason_code, struct scb *, uint8 tid, wlc_txh_info_t*);
extern bool wlc_ampdu_was_ampdu(ampdu_tx_info_t *, int fifo);
extern int wlc_dump_aggfifo(wlc_info_t *wlc, struct bcmstrbuf *b);
extern void wlc_ampdu_fill_percache_info(ampdu_tx_info_t *ampdu_tx, struct scb *scb, uint8 tid,
	d11actxh_t *txh);
#endif /* WLAMPDU_MAC */

#define TXFS_WSZ_AC_BE	32
#define TXFS_WSZ_AC_BK	10
#define TXFS_WSZ_AC_VI	4
#define TXFS_WSZ_AC_VO	4

extern void wlc_sidechannel_init(ampdu_tx_info_t *ampdu_tx);

extern void ampdu_cleanup_tid_ini(ampdu_tx_info_t *ampdu_tx, struct scb *scb,
	uint8 tid, bool force);

extern void scb_ampdu_tx_flush(ampdu_tx_info_t *ampdu_tx, struct scb *scb);

extern void BCMATTACHFN(wlc_ampdu_tx_set_txaggr_support)(ampdu_tx_info_t *ampdu_tx,
	bool txaggr);

extern void wlc_ampdu_clear_tx_dump(ampdu_tx_info_t *ampdu_tx);

extern void wlc_ampdu_recv_ba(ampdu_tx_info_t *ampdu_tx, struct scb *scb, uint8 *body,
	int body_len);
extern void wlc_ampdu_recv_addba_req_ini(ampdu_tx_info_t *ampdu_tx, struct scb *scb,
	dot11_addba_req_t *addba_req, int body_len);
extern void wlc_ampdu_recv_addba_resp(ampdu_tx_info_t *ampdu_tx, struct scb *scb,
	uint8 *body, int body_len);

#if defined(BCMDBG) || defined(BCMDBG_DUMP) || defined(WLTEST) || defined(BCMDBG_AMPDU)
extern int wlc_ampdu_tx_dump(ampdu_tx_info_t *ampdu_tx, struct bcmstrbuf *b);
#if defined(EVENT_LOG_COMPILE)
extern int wlc_ampdu_txmcs_counter_report(ampdu_tx_info_t *ampdu_tx, uint16 tag);
#endif /* EVENT_LOG_COMPILE */
#endif /* BCMDBG || WLTEST */

extern void wlc_ampdu_tx_set_mpdu_density(ampdu_tx_info_t *ampdu_tx, uint8 mpdu_density);
extern void wlc_ampdu_tx_set_ba_tx_wsize(ampdu_tx_info_t *ampdu_tx, uint8 wsize);
extern uint8 wlc_ampdu_tx_get_ba_tx_wsize(ampdu_tx_info_t *ampdu_tx);
extern uint8 wlc_ampdu_tx_get_ba_max_tx_wsize(ampdu_tx_info_t *ampdu_tx);
extern void wlc_ampdu_tx_recv_delba(ampdu_tx_info_t *ampdu_tx, struct scb *scb, uint8 tid,
	uint8 category, uint16 initiator, uint16 reason);
extern void wlc_ampdu_tx_send_delba(ampdu_tx_info_t *ampdu_tx, struct scb *scb, uint8 tid,
	uint16 initiator, uint16 reason);

extern int wlc_ampdu_tx_set(ampdu_tx_info_t *ampdu_tx, bool on);

extern uint wlc_ampdu_tx_get_tcp_ack_ratio(ampdu_tx_info_t *ampdu_tx);

#ifdef MACOSX
extern bool wlc_ampdu_tid_enabled(wlc_info_t *wlc, uint tid);
extern uint wlc_ampdu_txpktcnt_tid(ampdu_tx_info_t *ampdu, uint tid);
extern uint wlc_ampdu_pmax_tid(ampdu_tx_info_t *ampdu, uint tid);
#endif /* MACOSX */

#if defined(WLNAR)
extern uint8 BCMFASTPATH wlc_ampdu_ba_on_tidmask(const struct scb *scb);
#endif // endif

#ifdef WLTAF
extern void * BCMFASTPATH
wlc_ampdu_get_taf_scb_info(ampdu_tx_info_t *ampdu_tx, struct scb* scb);
extern void * BCMFASTPATH
wlc_ampdu_get_taf_scb_tid_info(scb_ampdu_tx_t *scb_ampdu, int tid);
extern uint16 BCMFASTPATH
wlc_ampdu_get_taf_scb_tid_pktlen(scb_ampdu_tx_t *scb_ampdu, scb_ampdu_tid_ini_t *ini);
extern uint8 BCMFASTPATH
wlc_ampdu_get_taf_scb_tid_rel(scb_ampdu_tx_t *scb_ampdu);
extern uint8 BCMFASTPATH
wlc_ampdu_get_taf_txq_fullness_pct(scb_ampdu_tx_t *scb_ampdu, scb_ampdu_tid_ini_t *ini);
#endif /* WLTAF */

extern bool BCMFASTPATH
wlc_ampdu_txeval_action(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tx_t *scb_ampdu,
                        scb_ampdu_tid_ini_t* ini, bool force, taf_scheduler_public_t* taf);

extern void wlc_wlfc_flush_queue(wlc_info_t *wlc, struct pktq *q);

extern struct pktq* wlc_ampdu_txq(ampdu_tx_info_t *ampdu, struct scb *scb);
#ifdef PROP_TXSTATUS
extern void wlc_ampdu_send_bar_cfg(ampdu_tx_info_t * ampdu, struct scb *scb);
extern void wlc_ampdu_flush_ampdu_q(ampdu_tx_info_t *ampdu, wlc_bsscfg_t *cfg);
extern void wlc_ampdu_flush_pkts(wlc_info_t *wlc, struct scb *scb, uint8 tid);
extern void wlc_ampdu_flush_flowid_pkts(wlc_info_t *wlc, struct scb *scb, uint16 flowid);
#endif /* PROP_TXSTATUS */
#ifdef WLAWDL
extern void wlc_awdl_ampdu_txeval(void *hdl, bool awdlorbss);
#endif // endif

#ifdef WLATF
extern void wlc_ampdu_atf_set_default_mode(ampdu_tx_info_t *ampdu_tx,
	scb_module_t *scbstate, uint32 mode);
extern void wlc_ampdu_atf_rate_override(wlc_info_t *, ratespec_t, wlcband_t *);
extern void wlc_ampdu_atf_scb_rate_override(ampdu_tx_info_t *, struct scb *, ratespec_t);
#endif /* WLATF */

extern void wlc_check_ampdu_fc(ampdu_tx_info_t *ampdu, struct scb *scb);

extern void wlc_ampdu_txeval_all(wlc_info_t *wlc);
#ifdef BCMDBG_TXSTUCK
extern void wlc_ampdu_print_txstuck(wlc_info_t *wlc, struct bcmstrbuf *b);
#endif /* BCMDBG_TXSTUCK */
#endif /* _wlc_ampdu_tx_h_ */
