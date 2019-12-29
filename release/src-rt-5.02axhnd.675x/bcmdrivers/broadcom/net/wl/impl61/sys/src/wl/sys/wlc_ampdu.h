/*
 * A-MPDU Tx (with extended Block Ack) related header file
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
 * $Id: wlc_ampdu.h 776996 2019-07-16 02:57:25Z $
*/

#ifndef _wlc_ampdu_tx_h_
#define _wlc_ampdu_tx_h_

#if defined(WL_LINKSTAT)
#include <wlc_linkstats.h>
#endif // endif
#include <wlc_tx.h>

#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
#define WLC_AMPDU_DUMP
#endif // endif

#ifdef WLTAF
typedef struct taf_scheduler_public taf_scheduler_public_t;
#else
typedef void taf_scheduler_public_t;
#endif /* WLTAF */
#define	AMPDU_MAX_DUR		5416		/**< max dur of tx ampdu (in usec) */
#define	AMPDU_DEF_MAX_RELEASE_AQM	64	/**< max # of mpdus released at a time */

typedef enum {
				BTCX_MODULE,
				SLOTTED_BSS_MODULE,
				P2P_MODULE,
				NUM_MODULES
} scb_ampdu_module_t;

typedef struct scb_ampdu_tx scb_ampdu_tx_t;
typedef struct scb_ampdu_tid_ini scb_ampdu_tid_ini_t;

extern ampdu_tx_info_t *wlc_ampdu_tx_attach(wlc_info_t *wlc);
extern void wlc_ampdu_tx_detach(ampdu_tx_info_t *ampdu_tx);

/**
 * @brief Enable A-MPDU aggregation for the specified SCB
 *
 */
extern void wlc_ampdu_tx_scb_enable(ampdu_tx_info_t *ampdu_tx, struct scb *scb);

/**
 * @brief Disable A-MPDU aggregation for the specified SCB
 *
 */
extern void wlc_ampdu_tx_scb_disable(ampdu_tx_info_t *ampdu_tx, struct scb *scb);

extern int wlc_sendampdu(ampdu_tx_info_t *ampdu_tx, wlc_txq_info_t *qi, void **aggp, int prec,
	struct spktq *output_q, int *supplied_time, uint fifo);
extern bool wlc_ampdu_dotxstatus(ampdu_tx_info_t *ampdu_tx, struct scb *scb, void *p,
	tx_status_t *txs, wlc_txh_info_t *txh_info);
extern void wlc_ampdu_dotxstatus_regmpdu(ampdu_tx_info_t *ampdu_tx, struct scb *scb, void *p,
	tx_status_t *txs, wlc_txh_info_t *txh_info);

extern void wlc_ampdu_macaddr_upd(wlc_info_t *wlc);
extern uint8 wlc_ampdu_null_delim_cnt(ampdu_tx_info_t *ampdu_tx, struct scb *scb,
	ratespec_t rspec, int phylen, uint16* minbytes);
extern bool wlc_ampdu_frameburst_override(ampdu_tx_info_t *ampdu_tx, struct scb *scb);
extern void wlc_ampdu_tx_set_bsscfg_aggr_override(ampdu_tx_info_t *ampdu_tx,
	wlc_bsscfg_t *bsscfg, int8 txaggr);
extern uint16 wlc_ampdu_tx_get_bsscfg_aggr(ampdu_tx_info_t *ampdu_tx, wlc_bsscfg_t *bsscfg);
extern void wlc_ampdu_tx_set_bsscfg_aggr(ampdu_tx_info_t *ampdu_tx, wlc_bsscfg_t *bsscfg,
	bool txaggr, uint16 conf_TID_bmap);
extern void wlc_ampdu_btcx_tx_dur(wlc_info_t *wlc, bool btcx_ampdu_dur);

extern int wlc_ampdumac_set(ampdu_tx_info_t *ampdu_tx, uint8 on);
extern void wlc_ampdu_tx_max_dur(ampdu_tx_info_t *ampdu_tx, struct scb *scb,
	scb_ampdu_module_t module_id, uint16 dur);
extern void wlc_ampdu_check_pending_ba_for_bsscfg(wlc_info_t *wlc, wlc_bsscfg_t	*bsscfg);
extern void wlc_ampdu_check_pending_ba_for_scb(wlc_info_t *wlc, struct scb *scb);

#define AMU_EPOCH_NO_CHANGE		-1	/**< no-epoch change but change params */
#define AMU_EPOCH_CHG_PLCP		0	/**< epoch change due to plcp */
#define AMU_EPOCH_CHG_FID		1	/**< epoch change due to rate flag in frameid */
#define AMU_EPOCH_CHG_NAGG		2	/**< epoch change due to ampdu off */
#define AMU_EPOCH_CHG_MPDU		3	/**< epoch change due to mpdu */
#define AMU_EPOCH_CHG_DSTTID		4	/**< epoch change due to dst/tid */
#define AMU_EPOCH_CHG_SEQ		5	/**< epoch change due to discontinuous seq no */
#define AMU_EPOCH_CHG_TXC_UPD		6	/**< Epoch change due to txcache update  */
#define AMU_EPOCH_CHG_TXHDR		7	/**< Epoch change due to Long to short hdr change */
#define AMU_EPOCH_CHG_HTC		8	/**< Epoch change due to HTC+ */
#define AMU_EPOCH_CHG_TAF_STAR		9	/**< Epoch change due to TAF scheduling marker */

extern void wlc_ampdu_change_epoch(ampdu_tx_info_t *ampdu_tx, int fifo, int reason_code);
extern uint8 wlc_ampdu_chgnsav_epoch(ampdu_tx_info_t *, int fifo,
	int reason_code, struct scb *, uint8 tid, bool htc, wlc_txh_info_t*);
extern bool wlc_ampdu_was_ampdu(ampdu_tx_info_t *, int fifo);
extern int wlc_dump_aggfifo(wlc_info_t *wlc, struct bcmstrbuf *b);
extern void wlc_ampdu_fill_percache_info(ampdu_tx_info_t *ampdu_tx, struct scb *scb,
	uint8 tid, d11actxh_cache_t *cache_info);
extern void wlc_ampdu_fill_link_entry_info(ampdu_tx_info_t *ampdu_tx, struct scb *scb,
	d11linkmem_entry_t* link_entry);
extern void wlc_ampdu_fill_link_entry_pkteng(wlc_info_t *wlc, d11linkmem_entry_t *link_entry);
#ifdef WL_MUPKTENG
/** MUPKTENG function to fill link mem entry (Rev 128) content, called by wlc_tx_fill_link_entry */
extern void wlc_ampdu_mupkteng_fill_link_entry_info(ampdu_tx_info_t *ampdu_tx, struct scb *scb,
	d11linkmem_entry_t* link_entry);
/** Compute and fill the link mem entry (Rev 128) content, called by wlc_tx_fill_link_entry. */
extern void wlc_ampdu_mupkteng_fill_percache_info(ampdu_tx_info_t *ampdu_tx, struct scb *scb,
        uint8 tid, d11actxh_cache_t *txh_cache_info);
#endif /* WL_MUPKTENG */

#define TXFS_WSZ_AC_BE	32
#define TXFS_WSZ_AC_BK	10
#define TXFS_WSZ_AC_VI	4
#define TXFS_WSZ_AC_VO	4

#define AMPDU_MAXDUR_INVALID_VAL 0xffff
#define AMPDU_MAXDUR_INVALID_IDX 0xff

#if defined(WLAMPDU_UCODE)
extern void wlc_sidechannel_init(wlc_info_t *wlc);
#endif /* WLAMPDU_UCODE */

extern void ampdu_cleanup_tid_ini(ampdu_tx_info_t *ampdu_tx, struct scb *scb,
	uint8 tid, bool force);

extern void scb_ampdu_tx_flush(ampdu_tx_info_t *ampdu_tx, struct scb *scb);

extern void wlc_ampdu_clear_tx_dump(ampdu_tx_info_t *ampdu_tx);

extern void wlc_ampdu_recv_ba(ampdu_tx_info_t *ampdu_tx, struct scb *scb, uint8 *body,
	int body_len);
extern void wlc_ampdu_recv_addba_req_ini(ampdu_tx_info_t *ampdu_tx, struct scb *scb,
	dot11_addba_req_t *addba_req, int body_len);
extern void wlc_ampdu_recv_addba_resp(ampdu_tx_info_t *ampdu_tx, struct scb *scb,
	uint8 *body, int body_len);

#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
extern int wlc_ampdu_tx_dump(ampdu_tx_info_t *ampdu_tx, struct bcmstrbuf *b);
extern int wlc_ampdu_tx_scb_dump(ampdu_tx_info_t *ampdu_tx, struct bcmstrbuf *b);
#if defined(EVENT_LOG_COMPILE)
extern int wlc_ampdu_txmcs_counter_report(ampdu_tx_info_t *ampdu_tx, uint16 tag);
#ifdef ECOUNTERS
extern int wlc_ampdu_ecounter_tx_dump(ampdu_tx_info_t *ampdu_tx, uint16 tag,
	struct bcm_xtlvbuf *xtlvbuf, uint32 *cookie, uint16 *attempted_write_len);
#endif // endif
#endif /* EVENT_LOG_COMPILE */
#endif /* BCMDBG || WLTEST */

extern void wlc_ampdu_tx_set_mpdu_density(ampdu_tx_info_t *ampdu_tx, uint8 mpdu_density);
extern void wlc_ampdu_tx_set_ba_tx_wsize(ampdu_tx_info_t *ampdu_tx, uint8 wsize);
extern uint8 wlc_ampdu_tx_get_ba_tx_wsize(ampdu_tx_info_t *ampdu_tx);
extern uint8 wlc_ampdu_tx_get_ba_max_tx_wsize(ampdu_tx_info_t *ampdu_tx);
extern void wlc_ampdu_tx_send_delba(ampdu_tx_info_t *ampdu_tx, struct scb *scb, uint8 tid,
	uint16 initiator, uint16 reason);
extern void wlc_ampdu_tx_recv_delba(ampdu_tx_info_t *ampdu_tx, struct scb *scb, uint8 tid,
	uint8 category, uint16 initiator, uint16 reason);
extern int wlc_ampdu_tx_set(ampdu_tx_info_t *ampdu_tx, bool on);
extern uint wlc_ampdu_tx_get_tcp_ack_ratio(ampdu_tx_info_t *ampdu_tx);
extern uint8 wlc_ampdu_get_txpkt_weight(ampdu_tx_info_t *ampdu_tx);

#if defined(WLNAR)
extern uint8 BCMFASTPATH wlc_ampdu_ba_on_tidmask(struct scb *scb);
#endif // endif

extern struct pktq* wlc_ampdu_txq(ampdu_tx_info_t *ampdu, struct scb *scb);

#ifdef WLTAF
extern bool wlc_ampdu_taf_release(void* ampduh, void* scbh, void* tidh, bool force,
	taf_scheduler_public_t* taf);
extern void * BCMFASTPATH
wlc_ampdu_get_taf_scb_info(void *ampdu_h, struct scb* scb);
extern void * BCMFASTPATH
wlc_ampdu_get_taf_scb_tid_info(void *scb_h, int tid);
extern uint16 BCMFASTPATH
wlc_ampdu_get_taf_scb_tid_pktlen(void *scbh, void *tidh);
#ifdef PKTQ_LOG
extern struct pktq * BCMFASTPATH
wlc_ampdu_get_taf_scb_pktq(void *scbh);
#endif // endif

extern uint8 BCMFASTPATH
wlc_ampdu_get_taf_scb_tid_rel(scb_ampdu_tx_t *scb_ampdu);
extern uint8 BCMFASTPATH
wlc_ampdu_get_taf_txq_fullness_pct(scb_ampdu_tx_t *scb_ampdu, scb_ampdu_tid_ini_t *ini);
extern uint8 BCMFASTPATH
wlc_ampdu_get_taf_max_pdu(scb_ampdu_tx_t *scb_ampdu);
#endif /* WLTAF */

#ifdef PROP_TXSTATUS
extern void wlc_ampdu_send_bar_cfg(ampdu_tx_info_t * ampdu, struct scb *scb);
extern void wlc_ampdu_flush_ampdu_q(ampdu_tx_info_t *ampdu, wlc_bsscfg_t *cfg);
extern void wlc_ampdu_flush_pkts(wlc_info_t *wlc, struct scb *scb, uint8 tid);
extern void wlc_ampdu_flush_flowid_pkts(wlc_info_t *wlc, struct scb *scb, uint16 flowid);

#endif /* PROP_TXSTATUS */

extern void wlc_check_ampdu_fc(ampdu_tx_info_t *ampdu, struct scb *scb);

#ifdef WL_CS_RESTRICT_RELEASE
extern void wlc_ampdu_txeval_all(wlc_info_t *wlc);
#endif /* WL_CS_RESTRICT_RELEASE */

void wlc_ampdu_txeval_alltid(wlc_info_t *wlc, struct scb *scb, bool force);
extern void wlc_ampdu_agg_state_update_tx_all(wlc_info_t *wlc, bool aggr);

extern bool BCMFASTPATH
wlc_ampdu_txeval_action(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tx_t *scb_ampdu,
                        scb_ampdu_tid_ini_t* ini, bool force, taf_scheduler_public_t* taf);
extern void wlc_ampdu_set_epoch(ampdu_tx_info_t *ampdu_tx, int fifo, uint8 epoch);

#if defined(WL_LINKSTAT)
extern void wlc_ampdu_txrates_get(ampdu_tx_info_t *ampdu_tx, wifi_rate_stat_t *rate,
	int i, int rs);
#endif // endif

#ifdef BCMDBG_TXSTUCK
extern void wlc_ampdu_print_txstuck(wlc_info_t *wlc, struct bcmstrbuf *b);
#endif /* BCMDBG_TXSTUCK */

struct pktq *scb_ampdu_prec_pktq(wlc_info_t *wlc, struct scb *scb);

#ifdef WLATF
extern void wlc_ampdu_atf_set_default_mode(ampdu_tx_info_t *ampdu_tx,
	scb_module_t *scbstate, uint32 mode);
extern void wlc_ampdu_atf_rate_override(wlc_info_t *, ratespec_t, wlcband_t *);
extern void wlc_ampdu_atf_scb_rate_override(ampdu_tx_info_t *, struct scb *, ratespec_t);
#endif /* WLATF */

#if (defined(WLC_AMPDU_DUMP) && defined(WLWRR))
extern int wlc_ampdu_dump_wrr(void *ctx, struct bcmstrbuf *b);
#endif // endif

extern void scb_ampdu_update_config_mu(ampdu_tx_info_t *ampdu_tx, scb_t *scb);
extern void scb_ampdu_update_config(ampdu_tx_info_t *ampdu_tx, struct scb *scb);
extern void scb_ampdu_check_config(wlc_info_t *wlc, struct scb *scb);
extern void scb_ampdu_update_config_all(ampdu_tx_info_t *ampdu_tx);

typedef struct ampdu_tx_scb_stats {
	uint32 tx_pkts[NUMPRIO];
	uint32 tx_bytes[NUMPRIO];
	uint32 tx_pkts_total[NUMPRIO];
	uint32 tx_bytes_total[NUMPRIO];
} ampdu_tx_scb_stats_t;

#if defined(WL_MU_TX)
extern bool wlc_ampdu_scbstats_get_and_clr(wlc_info_t *wlc, struct scb *scb,
	ampdu_tx_scb_stats_t *ampdu_scb_stats);
extern void BCMFASTPATH
wlc_ampdu_aqm_mutx_dotxinterm_status(ampdu_tx_info_t *ampdu_tx, tx_status_t *txs);
#endif /* defined(WL_MU_TX) */
extern void wlc_ampdu_reqmpdu_to_aqm_aggregatable(wlc_info_t *wlc, void *p);
extern void wlc_ampdu_reset_txnoprog(ampdu_tx_info_t *ampdu_tx);
extern uint wlc_ampdu_bss_txpktcnt(ampdu_tx_info_t *ampdu_tx, wlc_bsscfg_t *bsscfg);

#ifdef WLCFP
/** Cached Flow Processing Interface to AMPDU Module */

struct scb_cfp;

extern void* wlc_cfp_get_ampdu_tx_cubby(wlc_info_t *wlc, struct scb *scb);
#define GET_AMPDU_TX_CUBBY(wlc, scb)	\
	wlc_cfp_get_ampdu_tx_cubby(wlc, scb)
extern uint16 wlc_cfp_get_scbq_len(scb_cfp_t *scb_cfp, uint8 prio);
#define CFP_SCB_QLEN(scb_cfp, prio) \
	wlc_cfp_get_scbq_len(scb_cfp, prio)
/**
 * CFP AMPDU TX ENTRY: CFP Module -> AMPDU Module handoff
 * CFP Transmit fastpath entry point into AMPDU module.
 */
extern void wlc_cfp_ampdu_entry(wlc_info_t *wlc, uint8 prio,
	void *pktlist_head, void *pktlist_tail, uint16 pktlist_count,
	struct scb_cfp *scb_cfp);

#endif /* WLCFP */

#ifdef WLSQS
/** Single stage Queuing and Scheduling module */
extern bool wlc_sqs_ampdu_capable(wlc_info_t *wlc, scb_cfp_t *scb_cfp, uint8 prio);
extern uint16 wlc_sqs_ampdu_vpkts(wlc_info_t *wlc, scb_cfp_t *scb_cfp, uint8 prio);
extern uint16 wlc_sqs_ampdu_v2r_pkts(wlc_info_t *wlc, scb_cfp_t *scb_cfp, uint8 prio);
extern uint16 wlc_sqs_ampdu_n_pkts(wlc_info_t *wlc, scb_cfp_t *scb_cfp, uint8 prio);
extern uint16 wlc_sqs_ampdu_tbr_pkts(wlc_info_t *wlc, scb_cfp_t *scb_cfp, uint8 prio);
extern uint16 wlc_sqs_ampdu_in_transit_pkts(wlc_info_t *wlc, scb_cfp_t *scb_cfp, uint8 prio);
extern void wlc_sqs_pktq_v2r_revert(wlc_info_t *wlc, scb_cfp_t *scb_cfp, uint8 prio,
	uint16 v2r_reverts);
extern void wlc_sqs_pktq_vpkts_enqueue(wlc_info_t *wlc, scb_cfp_t *scb_cfp, uint8 prio,
	uint16 v_pkts);
extern void wlc_sqs_pktq_vpkts_rewind(wlc_info_t *wlc, scb_cfp_t *scb_cfp, uint8 prio,
	uint16 count);
extern void wlc_sqs_pktq_vpkts_forward(wlc_info_t *wlc, scb_cfp_t *scb_cfp, uint8 prio,
	uint16 count);
extern int wlc_sqs_ampdu_evaluate(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tx_t *scb_ampdu,
		uint8 prio, bool force, taf_scheduler_public_t* taf);
extern int wlc_sqs_ampdu_admit(wlc_info_t *wlc, scb_cfp_t *scb_cfp, uint8 prio, uint16 v_pkts);
#ifdef WLTAF
extern int wlc_sqs_taf_admit(wlc_info_t *wlc, struct scb *scb, uint8 prio, uint16 v_pkts);
extern void* wlc_ampdu_taf_sqs_link_init(wlc_info_t *wlc, struct scb *scb, uint8 prio);
#endif // endif
extern void wlc_sqs_v2r_ampdu_sendup(wlc_info_t *wlc, uint8 prio,
	void *pktlist_head, void *pktlist_tail, uint16 pkt_count, scb_cfp_t *scb_cfp);
extern void wlc_sqs_ampdu_v2r_enqueue(wlc_info_t *wlc, scb_cfp_t *scb_cfp, uint8 prio,
	uint16 v2r_count);
extern void wlc_sqs_ampdu_v2r_dequeue(wlc_info_t *wlc, scb_cfp_t *scb_cfp, uint8 prio,
	uint16 pkt_count);
#ifdef WLATF
extern int wlc_ampdu_tx_intransit_get(wlc_info_t *wlc);
#endif // endif
#define SQS_AMPDU_V_PKTS(pktq, prio) ((pktq)->q[(prio)].v_pkts)
#define SQS_AMPDU_V2R_PKTS(pktq, prio) ((pktq)->q[(prio)].v2r_pkts)
#define SQS_AMPDU_R_PKTS(pktq, prio) ((pktq)->q[(prio)].n_pkts)

#define SQS_AMPDU_PKTLEN(pktq, prio) \
	(SQS_AMPDU_V_PKTS((pktq), (prio)) + (SQS_AMPDU_V2R_PKTS((pktq), (prio)) ? 0 : \
		SQS_AMPDU_R_PKTS((pktq), (prio))))
#define SQS_AMPDU_RELEASE_LEN(pktq, prio, release) \
	((release) - (SQS_AMPDU_V2R_PKTS(pktq, prio) ? 0 : SQS_AMPDU_R_PKTS(pktq, prio)))

#endif /* WLSQS */

#define PKTS_TO_MPDUS(aggsf, pkt_cnt) (((pkt_cnt) + ((aggsf) - 1)) / (aggsf))
#define MPDUS_TO_PKTS(aggsf, mpdu_cnt)	((mpdu_cnt) * (aggsf))

#ifdef WLCNTSCB
#define WLC_AMSDU_IN_AMPDU(wlc, scb, tid) \
	((AMSDU_TX_ENAB((wlc)->pub) & (SCB_AMSDU_IN_AMPDU(scb) != 0) & \
	(SCB_AMSDU(scb) != 0) & AMSDU_TX_AC_ENAB((wlc)->ami, (tid))) & \
	(RSPEC_ISVHT((scb)->scb_stats.tx_rate) || RSPEC_ISHE((scb)->scb_stats.tx_rate)))
#else
#define WLC_AMSDU_IN_AMPDU(wlc, scb, tid) (FALSE)
#endif // endif

#define WLC_AMPDU_MPDUS_TO_PKTS(wlc, scb, tid, mpdu_cnt) \
	({ \
		uint8 aggsf = WLC_AMSDU_IN_AMPDU((wlc), (scb), (tid)) ? \
			wlc_amsdu_scb_max_sframes((wlc)->ami, (scb), (tid)) : 1; \
		MPDUS_TO_PKTS(aggsf, mpdu_cnt); \
	})

#define WLC_AMPDU_PKTS_TO_MPDUS(wlc, scb, tid, pkt_cnt)\
	({ \
		uint8 aggsf = WLC_AMSDU_IN_AMPDU((wlc), (scb), (tid)) ? \
			wlc_amsdu_scb_max_sframes((wlc)->ami, (scb), (tid)) : 1; \
		PKTS_TO_MPDUS(aggsf, pkt_cnt); \
	})

#if defined(WLSQS) && defined(WLTAF)
extern uint16 wlc_ampdu_pull_packets(wlc_info_t* wlc, struct scb* scb,
	uint8 tid, uint16 request_cnt, taf_scheduler_public_t* taf);
#endif /* WLSQS && WLTAF */

/* AMPDU precedence queue functions exposed outside */
extern void* wlc_ampdu_pktq_pdeq(wlc_info_t *wlc, struct scb* scb, uint8 prio);
extern void wlc_ampdu_pktq_penq(wlc_info_t *wlc, struct scb* scb, uint8 prio, void* p);
extern void wlc_ampdu_pktq_penq_head(wlc_info_t *wlc, struct scb* scb, uint8 prio, void* p);

#endif /* _wlc_ampdu_tx_h_ */
