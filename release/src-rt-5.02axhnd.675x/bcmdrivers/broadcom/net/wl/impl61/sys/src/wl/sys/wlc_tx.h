/*
 * wlc_tx.h
 *
 * Common headers for transmit datapath components
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
 * $Id: wlc_tx.h 779338 2019-09-25 15:26:15Z $
 *
 */
#ifndef _wlc_tx_h_
#define _wlc_tx_h_

#include <d11.h>

/* Place holder for tx datapath functions
 * Refer to RB http://wlan-rb.sj.broadcom.com/r/18439/ and
 * TWIKI http://hwnbu-twiki.broadcom.com/bin/view/Mwgroup/WlDriverTxQueuingUpdate2013
 * for details on datapath restructuring work
 * These functions will be moved in stages from wlc.[ch]
 */

/*
 * pktq filter support
 */

/** filter function return values */
typedef enum {
	PKT_FILTER_NOACTION = 0,    /**< restore the pkt to its position in the queue */
	PKT_FILTER_DELETE = 1,      /**< delete the pkt */
	PKT_FILTER_REMOVE = 2,      /**< do not restore the pkt to the queue,
	                             *   filter fn has taken ownership of the pkt
	                             */
} pktq_filter_result_t;

/**
 * Caller supplied filter function to pktq_pfilter(), pktq_filter().
 * Function filter(ctx, pkt) is called with its ctx pointer on each pkt in the
 * pktq.  When the filter function is called, the supplied pkt will have been
 * unlinked from the pktq.  The filter function returns a pktq_filter_result_t
 * result specifying the action pktq_filter()/pktq_pfilter() should take for
 * the pkt.
 * Here are the actions taken by pktq_filter/pfilter() based on the supplied
 * filter function's return value:
 *
 * PKT_FILTER_NOACTION - The filter will re-link the pkt at its
 *     previous location.
 *
 * PKT_FILTER_DELETE - The filter will not relink the pkt and will
 *     call the user supplied defer_free_pkt fn on the packet.
 *
 * PKT_FILTER_REMOVE - The filter will not relink the pkt. The supplied
 *     filter fn took ownership (or deleted) the pkt.
 *
 * WARNING: pkts inserted by the user (in pkt_filter and/or flush callbacks
 * and chains) in the prec queue will not be seen by the filter, and the prec
 * queue will be temporarily be removed from the queue hence there're side
 * effects including pktq_n_pkts_tot() on the queue won't reflect the correct number
 * of packets in the queue.
 */
typedef pktq_filter_result_t (*pktq_filter_t)(void* ctx, void* pkt);

/**
 * The defer_free_pkt callback is invoked when the the pktq_filter callback
 * returns PKT_FILTER_DELETE decision, which allows the user to deposite
 * the packet appropriately based on the situation (free the packet or
 * save it in a temporary queue etc.).
 */
typedef void (*defer_free_pkt_fn_t)(void *ctx, void *pkt);

/**
 * The flush_free_pkt callback is invoked when all packets in the pktq
 * are processed.
 */
typedef void (*flush_free_pkt_fn_t)(void *ctx);

#if defined(PROP_TXSTATUS)
/* this callback will be invoked when in low_txq_scb flush()
 *  two back-to-back pkts has same epoch value.
 */
typedef void (*flip_epoch_t)(void *ctx, void *pkt, uint8 *flipEpoch, uint8 *lastEpoch);
#endif /* PROP_TXSTATUS */

/** filter a pktq, using the caller supplied filter/deposition/flush functions */
extern void  pktq_filter(struct pktq *pq, pktq_filter_t fn, void* arg,
	defer_free_pkt_fn_t defer, void *defer_ctx, flush_free_pkt_fn_t flush, void *flush_ctx);
/** filter a particular precedence in pktq, using the caller supplied filter function */
extern void  pktq_pfilter(struct pktq *pq, int prec, pktq_filter_t fn, void* arg,
	defer_free_pkt_fn_t defer, void *defer_ctx, flush_free_pkt_fn_t flush, void *flush_ctx);
/** filter a simple non-precedence in spktq, using the caller supplied filter function */
#if defined(PROP_TXSTATUS)
extern void spktq_filter(struct spktq *spq, pktq_filter_t fltr, void* fltr_ctx,
	defer_free_pkt_fn_t defer, void *defer_ctx, flush_free_pkt_fn_t flush, void *flush_ctx,
	flip_epoch_t flip_epoch_callback);
#else
extern void spktq_filter(struct spktq *spq, pktq_filter_t fltr, void* fltr_ctx,
	defer_free_pkt_fn_t defer, void *defer_ctx, flush_free_pkt_fn_t flush, void *flush_ctx);
#endif /* PROP_TXSTATUS */

extern chanspec_t wlc_txh_get_chanspec(wlc_info_t* wlc, wlc_txh_info_t* tx_info);

extern uint16 wlc_get_txmaxpkts(wlc_info_t *wlc);
extern void wlc_set_txmaxpkts(wlc_info_t *wlc, uint16 txmaxpkts);
extern void wlc_set_default_txmaxpkts(wlc_info_t *wlc);

extern bool wlc_low_txq_empty(txq_t *txq);
#define WLC_TXQ_OCCUPIED(w) \
	(!pktq_empty(WLC_GET_CQ((w)->active_queue)) || \
	!wlc_low_txq_empty((w)->active_queue->low_txq))
extern void wlc_low_txq_enq(txq_info_t *txqi,
	txq_t *txq, uint fifo_idx, void *pkt);
extern void wlc_low_txq_enq_list(txq_info_t *txqi, txq_t *txq,
	uint fifo_idx, struct spktq *list, int pkt_cnt);

/* used in bmac_fifo: acct for pkts that don't go through normal tx path
 * best example is pkteng
*/
extern int wlc_low_txq_buffered_inc(wlc_txq_info_t *qi, uint fifo_idx, int pkt_cnt);
extern int wlc_low_txq_buffered_dec(wlc_txq_info_t *qi, uint fifo_idx, int pkt_cnt);

extern uint wlc_txq_nfifo(txq_t *txq);

extern txq_info_t * wlc_txq_attach(wlc_info_t *wlc);
extern void wlc_txq_detach(txq_info_t *txq);
extern void wlc_low_txq_free(txq_info_t *txqi, txq_t* txq);
extern void wlc_low_txq_flush(txq_info_t *txqi, txq_t* txq);

extern void wlc_tx_fifo_scb_flush(wlc_info_t *wlc, struct scb *remove);
extern void wlc_txq_fill(txq_info_t *txqi, txq_t *txq);
extern void wlc_txq_complete(txq_info_t *txqi, txq_t *txq, uint fifo_idx,
                             int complete_pkts);
extern uint8 txq_stopped_map(txq_t *txq);
extern int txq_stopped(txq_t *txq, uint fifo_idx);
extern int txq_space(txq_t *txq, uint fifo_idx);
extern int wlc_txq_buffered_time(txq_t *txq, uint fifo_idx);
extern void wlc_tx_fifo_attach_complete(wlc_info_t *wlc);
/**
 * @brief Sanity check on the Flow Control state of the TxQ
 */
extern int wlc_txq_fc_verify(txq_info_t *txqi, txq_t *txq);
extern int txq_hw_stopped(txq_t *txq, uint fifo_idx);
extern void txq_hw_hold_set(txq_t *txq, uint fifo_idx);
extern void txq_hw_hold_clr(txq_t *txq, uint fifo_idx);
extern void txq_hw_fill(txq_info_t *txqi, txq_t *txq, uint fifo_idx);
extern void txq_hw_fill_active(txq_info_t *txqi, txq_t *txq);
extern wlc_txq_info_t* wlc_txq_alloc(wlc_info_t *wlc, osl_t *osh);
extern void wlc_txq_free(wlc_info_t *wlc, osl_t *osh, wlc_txq_info_t *qi);
extern void wlc_send_q(wlc_info_t *wlc, wlc_txq_info_t *qi);
extern void wlc_send_active_q(wlc_info_t *wlc);
extern int wlc_prep_pdu(wlc_info_t *wlc, struct scb *scb, void *pdu, uint fifo);
extern int wlc_prep_sdu(wlc_info_t *wlc, struct scb *scb, void **sdu, int *nsdu, uint fifo);
extern int wlc_prep_sdu_fast(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb,
	void *sdu, uint fifo, wlc_key_t **key, wlc_key_info_t *key_info);
extern void* wlc_hdr_proc(wlc_info_t *wlc, void *sdu, struct scb *scb);

extern void wlc_scb_determine_mu_txfifo(wlc_info_t *wlc, scb_t *scb, uint *pfifo);
extern void wlc_scb_get_txfifo(wlc_info_t *wlc, struct scb *scb, void *pkt, uint *pfifo);

/* wlc_d11hdrs() variant - unify with WLC_D11HDRS_FN #define */
#if defined(BCMDBG) || defined(WLMSG_PRHDRS) || defined(WLMSG_PRPKT)
#define WLC_D11HDRS_DBG
#endif // endif

uint16 wlc_d11hdrs_pre40(wlc_info_t *wlc, void *pkt, struct scb *scb,
	uint txparams_flags, uint frag, uint nfrags, uint queue, uint next_frag_len,
	wlc_key_t *key, const wlc_key_info_t *key_info, ratespec_t rspec_override);
uint16 wlc_d11hdrs_rev40(wlc_info_t *wlc, void *pkt, struct scb *scb,
	uint txparams_flags, uint frag, uint nfrags, uint queue, uint next_frag_len,
	wlc_key_t *key, const wlc_key_info_t *key_info, ratespec_t rspec_override);
uint16 wlc_d11hdrs_rev128(wlc_info_t *wlc, void *pkt, struct scb *scb,
	uint txparams_flags, uint frag, uint nfrags, uint queue, uint next_frag_len,
	wlc_key_t *key, const wlc_key_info_t *key_info, ratespec_t rspec_override);

#ifdef DONGLEBUILD
#if D11CONF_GE(128)
#define WLC_D11HDRS_FN(wlc, pkt, scb, \
		txparams_flags, frag, nfrags, queue, next_frag_len, \
		key, key_info, rspec_override) \
	wlc_d11hdrs_rev128(wlc, pkt, scb, \
		txparams_flags, frag, nfrags, queue, next_frag_len, \
		key, key_info, rspec_override)
#elif D11CONF_GE(40)
#define WLC_D11HDRS_FN(wlc, pkt, scb, \
		txparams_flags, frag, nfrags, queue, next_frag_len, \
		key, key_info, rspec_override) \
	wlc_d11hdrs_rev40(wlc, pkt, scb, \
		txparams_flags, frag, nfrags, queue, next_frag_len, \
		key, key_info, rspec_override)
#else /* D11CONF_LT(40) */
#define WLC_D11HDRS_FN(wlc, pkt, scb, \
		txparams_flags, frag, nfrags, queue, next_frag_len, \
		key, key_info, rspec_override) \
	wlc_d11hdrs_pre40(wlc, pkt, scb, \
		txparams_flags, frag, nfrags, queue, next_frag_len, \
		key, key_info, rspec_override)
#endif /* D11CONF_LT(40) */
#else /* !DONGLEBUILD */
/* TODO: #include <wlc.h> in order to dereference wlc! */
#define WLC_D11HDRS_FN(wlc, pkt, scb, \
		txparams_flags, frag, nfrags, queue, next_frag_len, \
		key, key_info, rspec_override) \
	((wlc)->wlc_d11hdrs_fn)(wlc, pkt, scb, \
		txparams_flags, frag, nfrags, queue, next_frag_len, \
		key, key_info, rspec_override)
#endif /* !DONGLEBUILD */

#ifdef WLC_D11HDRS_DBG
extern uint16 wlc_d11hdrs(wlc_info_t *wlc, void *p, struct scb *scb, uint txparams_flags,
	uint frag, uint nfrags, uint queue, uint next_frag_len,
	wlc_key_t *key, const wlc_key_info_t *key_info, ratespec_t rspec_override);
#else /* !WLC_D11HDRS_DBG */
#define wlc_d11hdrs(wlc, pkt, scb, \
		txparams_flags, frag, nfrags, queue, next_frag_len, \
		key, key_info, rspec_override) \
	WLC_D11HDRS_FN(wlc, pkt, scb,	\
		txparams_flags, frag, nfrags, queue, next_frag_len, \
		key, key_info, rspec_override)
#endif /* !WLC_D11HDRS_DBG */

extern void wlc_txprep_pkt_get_hdrs(wlc_info_t* wlc, void* p,
	uint8** ptxd, d11txhdr_t** ptxh, struct dot11_header** d11_hdr);
extern bool wlc_txh_get_isSGI(wlc_info_t* wlc, const wlc_txh_info_t* txh_info);
extern bool wlc_txh_get_isSTBC(wlc_info_t* wlc, const wlc_txh_info_t* txh_info);
extern bool wlc_txh_info_is5GHz(wlc_info_t* wlc, wlc_txh_info_t* txh_info);
extern bool wlc_txh_has_rts(wlc_info_t* wlc, wlc_txh_info_t* txh_info);
extern bool wlc_txh_get_isAMPDU(wlc_info_t* wlc, const wlc_txh_info_t* txh_info);
extern void wlc_txh_set_ft(wlc_info_t* wlc, uint8* pdata, uint16 fc_ft);
extern void wlc_txh_set_frag_allow(wlc_info_t* wlc, uint8* pdata, bool frag_allow);
extern void wlc_txh_set_epoch(wlc_info_t* wlc, uint8* pdata, uint8 epoch);
extern uint8 wlc_txh_get_epoch(wlc_info_t* wlc, wlc_txh_info_t* txh_info);
extern void wlc_txfifo_complete(wlc_info_t *wlc, uint fifo, uint16 txpktpend);
#ifndef DONGLEBUILD
extern int wlc_txfifo_freed_pkt_cnt_noaqm(wlc_info_t *wlc, uint fifo_idx);
#endif // endif
extern void wlc_tx_open_datafifo(wlc_info_t *wlc);
#define WLC_LOWEST_SCB_RSPEC(scb) 0
#define WLC_LOWEST_BAND_RSPEC(band) 0
#ifdef AP
extern void wlc_tx_fifo_sync_bcmc_reset(wlc_info_t *wlc);
#endif /* AP */
extern void wlc_get_txh_info(wlc_info_t* wlc, void* pkt, wlc_txh_info_t* tx_info);
extern void wlc_block_datafifo(wlc_info_t *wlc, uint32 mask, uint32 val);
extern void wlc_txflowcontrol(wlc_info_t *wlc, wlc_txq_info_t *qi, bool on, int prio);
extern void wlc_txflowcontrol_override(wlc_info_t *wlc, wlc_txq_info_t *qi, bool on, uint override);
extern bool wlc_txflowcontrol_prio_isset(wlc_info_t *wlc, wlc_txq_info_t *qi, int prio);
extern bool wlc_txflowcontrol_override_isset(wlc_info_t *wlc, wlc_txq_info_t *qi, uint override);
extern void wlc_txflowcontrol_reset(wlc_info_t *wlc);
extern void wlc_txflowcontrol_reset_qi(wlc_info_t *wlc, wlc_txq_info_t *qi);
extern uint16 wlc_compute_frameid(wlc_info_t *wlc, uint16 frameid_le, uint fifo);

#define WLC_BLOCK_DATAFIFO_SET(wlc, val) wlc_block_datafifo((wlc), (val), (val))
#define WLC_BLOCK_DATAFIFO_CLEAR(wlc, val) wlc_block_datafifo((wlc), (val), 0)

extern void wlc_tx_fifo_sync_complete(wlc_info_t *wlc, void *fifo_bitmap, uint8 flag);
extern void wlc_excursion_start(wlc_info_t *wlc);
extern void wlc_excursion_end(wlc_info_t *wlc);
extern void wlc_primary_queue_set(wlc_info_t *wlc, wlc_txq_info_t *new_primary_queue);
extern void wlc_active_queue_set(wlc_info_t *wlc, wlc_txq_info_t *new_active_queue);
extern void wlc_sync_txfifo(wlc_info_t *wlc, wlc_txq_info_t *qi, uint fifo, uint8 flag);
extern void wlc_sync_txfifos(wlc_info_t *wlc, wlc_txq_info_t *qi, void *fifo_bitmap, uint8 flag);
extern void wlc_sync_txfifo_all(wlc_info_t *wlc, wlc_txq_info_t *qi, uint8 flag);

#ifdef STA
extern void *wlc_sdu_to_pdu(wlc_info_t *wlc, void *sdu, struct scb *scb, bool is_8021x);
#endif /* STA */

extern void wlc_low_txq_map_pkts(wlc_info_t *wlc, wlc_txq_info_t *qi, map_pkts_cb_fn cb, void *ctx);
extern void wlc_txq_map_pkts(wlc_info_t *wlc, wlc_txq_info_t *qi, map_pkts_cb_fn cb, void *ctx);
extern void wlc_bsscfg_psq_map_pkts(wlc_info_t *wlc, struct pktq *q, map_pkts_cb_fn cb, void *ctx);
#ifdef AP
extern void wlc_scb_psq_map_pkts(wlc_info_t *wlc, struct pktq *q, map_pkts_cb_fn cb, void *ctx);
#endif // endif
extern void wlc_tx_map_pkts(wlc_info_t *wlc, struct pktq *q, int prec,
            map_pkts_cb_fn cb, void *ctx);

/** Compute and fill the rate mem entry (Rev 128) for an SCB, called by RateLinkMem module. */
extern void wlc_tx_fill_rate_entry(wlc_info_t *wlc, scb_t *scb,
	d11ratemem_rev128_entry_t *rate_entry, void* rate_store, void *rateset);
/** Compute and fill the link mem entry (Rev 128) for an SCB, called by RateLinkMem module. */
extern void wlc_tx_fill_link_entry(wlc_info_t *wlc, scb_t *scb, d11linkmem_entry_t *link_entry);

void wlc_beacon_phytxctl_txant_upd(wlc_info_t *wlc, ratespec_t bcn_rate);
void wlc_beacon_phytxctl(wlc_info_t *wlc, ratespec_t bcn_rspec, chanspec_t chanspec);
extern void wlc_beacon_upddur(wlc_info_t *wlc, ratespec_t bcn_rspec, uint16 len);

uint16 wlc_phytxctl1_calc(wlc_info_t *wlc, ratespec_t rspec, chanspec_t chanspec);

void wlc_txq_scb_free(wlc_info_t *wlc, struct scb *from_scb);
void wlc_txq_scb_remove_mfp_frames(wlc_info_t *wlc, struct scb *from_scb);

/* generic tx pktq filter */
void wlc_txq_pktq_pfilter(wlc_info_t *wlc, struct pktq *pq, int prec,
	pktq_filter_t fltr, void *ctx);
void wlc_txq_pktq_filter(wlc_info_t *wlc, struct pktq *pq, pktq_filter_t fltr, void *ctx);
void wlc_low_txq_pktq_filter(wlc_info_t *wlc, struct spktq *spq, pktq_filter_t fltr, void *ctx);

/* bsscfg based tx pktq filter */
void wlc_txq_pktq_cfg_filter(wlc_info_t *wlc, struct pktq *pq, wlc_bsscfg_t *cfg);
/* scb based tx pktq filter */
void wlc_txq_pktq_scb_filter(wlc_info_t *wlc, struct pktq *pq, struct scb *scb);

/* flush tx pktq */
void wlc_txq_pktq_pflush(wlc_info_t *wlc, struct pktq *pq, int prec);
void wlc_txq_pktq_flush(wlc_info_t *wlc, struct pktq *pq);
extern void wlc_epoch_upd(wlc_info_t *wlc, void *pkt, uint8 *flipEpoch, uint8 *lastEpoch);
extern void wlc_epoch_wrapper(void *ctx, void *pkt, uint8 *flipEpoch, uint8 *lastEpoch);
void wlc_tx_datapath_log_dump(wlc_info_t *wlc, int tag);

bool wlc_is_packet_fragmented(wlc_info_t *wlc, struct scb *scb, void *lb);

/* Below are the different reasons for packet toss in both
 * the Rx and Tx path.
 * WARNING !!! Whenever a toss reason is added to this enum,
 * make sure to add the corresponding toss reason string in
 * TX_STS_REASON_STRINGS and RX_STS_REASON_STRINGS
 */
typedef enum {
	WLC_TX_STS_QUEUED                   = 0,
	WLC_TX_STS_SUCCESS                  = 0,  /* 0 also indicates TX status success */
	WLC_TX_STS_FAILURE                  = 1,  /* 1 also indicatesgenerix TX failure */
	WLC_TX_STS_SUPPRESS                 = 1,
	WLC_TX_STS_SUPR_PMQ                 = 2,  /* PMQ entry */
	WLC_TX_STS_SUPR_FLUSH               = 3,  /* flush request */
	WLC_TX_STS_SUPR_FRAG_TBTT           = 4,  /* previous frag failure */
	WLC_TX_STS_SUPR_BADCH               = 5,  /* channel mismatch */
	WLC_TX_STS_SUPR_EXPTIME             = 6,  /* lifetime expiry */
	WLC_TX_STS_SUPR_UF                  = 7,  /* underflow */
	WLC_TX_STS_SUPR_NACK_ABS            = 8,  /* BSS entered ABSENCE period */
	WLC_TX_STS_SUPR_PPS                 = 9,  /* Pretend PS */
	WLC_TX_STS_SUPR_PHASE1_KEY          = 10, /* Req new TKIP phase-1 key */
	WLC_TX_STS_SUPR_UNUSED              = 11, /* Unused */
	WLC_TX_STS_SUPR_INT_ERR             = 12, /* Internal DMA xfer error */
	WLC_TX_STS_SUPR_RES1                = 13, /* Reserved */
	WLC_TX_STS_SUPR_RES2                = 14, /* Reserved */
	WLC_TX_STS_TX_Q_FULL                = 15,
	WLC_TX_STS_RETRY_TIMEOUT            = 16,
	WLC_TX_STS_BAD_CHAN                 = 17,
	WLC_TX_STS_PHY_ERROR                = 18,
	WLC_TX_STS_TOSS_INV_SEQ             = 19,
	WLC_TX_STS_TOSS_BSSCFG_DOWN         = 20,
	WLC_TX_STS_TOSS_CSA_BLOCK           = 21,
	WLC_TX_STS_TOSS_MFP_CCX             = 22,
	WLC_TX_STS_TOSS_NOT_PSQ1            = 23,
	WLC_TX_STS_TOSS_NOT_PSQ2            = 24,
	WLC_TX_STS_TOSS_L2_FILTER           = 25,
	WLC_TX_STS_TOSS_WMF_DROP            = 26,
	WLC_TX_STS_TOSS_WNM_DROP3           = 27,
	WLC_TX_STS_TOSS_PSTA_DROP           = 28,
	WLC_TX_STS_TOSS_CLASS3_BSS          = 29,
	WLC_TX_STS_TOSS_NULL_SCB1           = 31,
	WLC_TX_STS_TOSS_NULL_SCB2           = 32,
	WLC_TX_STS_TOSS_INV_MCAST_FRAME     = 34,
	WLC_TX_STS_TOSS_SCB_DELETED         = 35,
	WLC_TX_STS_TOSS_HDR_CONV_FAILED     = 36,
	WLC_TX_STS_TOSS_CRYPTO_ALGO_OFF     = 37,
	WLC_TX_STS_TOSS_DROP_CAC_PKT        = 38,
	WLC_TX_STS_TOSS_WL_DOWN             = 39,
	WLC_TX_STS_TOSS_INV_CLASS3_NO_ASSOC = 40,
	WLC_TX_STS_TOSS_INV_CLASS4_BT_AMP   = 41,
	WLC_TX_STS_TOSS_INV_BAND            = 42,
	WLC_TX_STS_TOSS_FIFO_LOOKUP_FAIL    = 43,
	WLC_TX_STS_TOSS_RUNT_FRAME          = 44,
	WLC_TX_STS_TOSS_MCAST_PKT_ROAM      = 45,
	WLC_TX_STS_TOSS_STA_NOTAUTH         = 46,
	WLC_TX_STS_TOSS_NWAI_FRAME_NOTAUTH  = 47,
	WLC_TX_STS_TOSS_UNENCRYPT_FRAME1    = 48,
	WLC_TX_STS_TOSS_UNENCRYPT_FRAME2    = 49,
	WLC_TX_STS_TOSS_KEY_PREP_FAIL       = 50,
	WLC_TX_STS_TOSS_SUPR_PKT            = 51,
	WLC_TX_STS_TOSS_DOWNGRADE_FAIL      = 52,
	WLC_TX_STS_TOSS_UNKNOWN             = 53,
	WLC_TX_STS_TOSS_NON_AMPDU_SCB       = 54,
	WLC_TX_STS_TOSS_BAD_INI             = 55,
	WLC_TX_STS_TOSS_BAD_ADDR            = 56,
	WLC_TX_STS_TOSS_WET                 = 57,
	WLC_TX_STS_UNKNOWN                  = 58,
	WLC_TX_STS_NO_BUF                   = 59,
	WLC_TX_STS_COMP_NO_SCB              = 60,
	WLC_TX_STS_BAD_NCONS                = 61,
	WLC_TX_STS_TOSS_WLCIF_REMOVED       = 62,
	WLC_TX_STS_LAST                     = 63
} wlc_tx_status_t;

#define TX_STS_REASON_STRINGS \
	"tx",                       /* WLC_TX_STS_QUEUED                   = 0,  */ \
	"tx_fail",                  /* WLC_TX_STS_FAILURE                  = 1,  */ \
	"supr_pmq",                 /* WLC_TX_STS_SUPR_PMQ                 = 2,  */ \
	"supr_flush",               /* WLC_TX_STS_SUPR_FLUSH               = 3,  */ \
	"supr_frag_tbtt",           /* WLC_TX_STS_SUPR_FRAG_TBTT           = 4,  */ \
	"supr_badch",               /* WLC_TX_STS_SUPR_BADCH               = 5,  */ \
	"supr_exp",                 /* WLC_TX_STS_SUPR_EXPTIME             = 6,  */ \
	"supr_uf",                  /* WLC_TX_STS_SUPR_UF                  = 7,  */ \
	"supr_nack_abs",            /* WLC_TX_STS_SUPR_NACK_ABS            = 8,  */ \
	"supr_pps",                 /* WLC_TX_STS_SUPR_PPS                 = 9,  */ \
	"supr_phase1_key",          /* WLC_TX_STS_SUPR_PHASE1_KEY          = 10, */ \
	"supr_unused",              /* WLC_TX_STS_SUPR_UNUSED              = 11, */ \
	"supr_int_err",             /* WLC_TX_STS_SUPR_INT_ERR             = 12, */ \
	"supr_res1",                /* WLC_TX_STS_SUPR_RES1                = 13, */ \
	"supr_res2",                /* WLC_TX_STS_SUPR_RES2                = 14, */ \
	"tx_q_full",                /* WLC_TX_STS_TX_Q_FULL                = 15, */ \
	"retry_tmo",                /* WLC_TX_STS_RETRY_TIMEOUT            = 16, */ \
	"bad_ch",                   /* WLC_TX_STS_BAD_CHAN                 = 17, */ \
	"phy_err",                  /* WLC_TX_STS_PHY_ERROR                = 18, */ \
	"toss_inv_seq",             /* WLC_TX_STS_TOSS_INV_SEQ             = 19, */ \
	"toss_bss_down",            /* WLC_TX_STS_TOSS_BSSCFG_DOWN         = 20, */ \
	"toss_csa_blk",             /* WLC_TX_STS_TOSS_CSA_BLOCK           = 21, */ \
	"toss_mfp_ccx",             /* WLC_TX_STS_TOSS_MFP_CCX             = 22, */ \
	"toss_not_psq1",            /* WLC_TX_STS_TOSS_NOT_PSQ1            = 23, */ \
	"toss_not_psq2",            /* WLC_TX_STS_TOSS_NOT_PSQ2            = 24, */ \
	"toss_l2_filter",           /* WLC_TX_STS_TOSS_L2_FILTER           = 25, */ \
	"toss_wmf_drop",            /* WLC_TX_STS_TOSS_WMF_DROP            = 26, */ \
	"toss_wnm_drop",            /* WLC_TX_STS_TOSS_WNM_DROP3           = 27, */ \
	"toss_psta",                /* WLC_TX_STS_TOSS_PSTA_DROP           = 28, */ \
	"toss_class3",              /* WLC_TX_STS_TOSS_CLASS3_BSS          = 29, */ \
	"toss_awdl_dis",            /* WLC_TX_STS_TOSS_AWDL_DISABLED       = 30, */ \
	"toss_null_scb1",           /* WLC_TX_STS_TOSS_NULL_SCB1           = 31, */ \
	"toss_null_scb2",           /* WLC_TX_STS_TOSS_NULL_SCB2           = 32, */ \
	"toss_awdl_no_assoc",       /* WLC_TX_STS_TOSS_AWDL_NO_ASSOC       = 33, */ \
	"toss_inv_mcast",           /* WLC_TX_STS_TOSS_INV_MCAST_FRAME     = 34, */ \
	"toss_scb_deleted"          /* WLC_TX_STS_TOSS_SCB_DELETED         = 35, */ \
	"toss_hdr_conv_fail",       /* WLC_TX_STS_TOSS_HDR_CONV_FAILED     = 36, */ \
	"toss_crypto_off",          /* WLC_TX_STS_TOSS_CRYPTO_ALGO_OFF     = 37, */ \
	"toss_cac_pkt",             /* WLC_TX_STS_TOSS_DROP_CAC_PKT        = 38, */ \
	"toss_wl_down",             /* WLC_TX_STS_TOSS_WL_DOWN             = 39, */ \
	"toss_inv_class3",          /* WLC_TX_STS_TOSS_INV_CLASS3_NO_ASSOC = 40, */ \
	"toss_inv_class4",          /* WLC_TX_STS_TOSS_INV_CLASS4_BT_AMP   = 41, */ \
	"toss_inv_band",            /* WLC_TX_STS_TOSS_INV_BAND            = 42, */ \
	"toss_fifo_lookup",         /* WLC_TX_STS_TOSS_FIFO_LOOKUP_FAIL    = 43, */ \
	"toss_runt",                /* WLC_TX_STS_TOSS_RUNT_FRAME          = 44, */ \
	"toss_mcast_roam",          /* WLC_TX_STS_TOSS_MCAST_PKT_ROAM      = 45, */ \
	"toss_not_auth",            /* WLC_TX_STS_TOSS_STA_NOTAUTH         = 46, */ \
	"toss_nwai_no_auth",        /* WLC_TX_STS_TOSS_NWAI_FRAME_NOTAUTH  = 47, */ \
	"toss_unencr1",             /* WLC_TX_STS_TOSS_UNENCRYPT_FRAME1    = 48, */ \
	"toss_unencr2",             /* WLC_TX_STS_TOSS_UNENCRYPT_FRAME2    = 49, */ \
	"toss_key_fail",            /* WLC_TX_STS_TOSS_KEY_PREP_FAIL       = 50, */ \
	"toss_supr",                /* WLC_TX_STS_TOSS_SUPR_PKT            = 51, */ \
	"toss_downgrade_fail",      /* WLC_TX_STS_TOSS_DOWNGRADE_FAIL      = 52, */ \
	"toss_unknown",             /* WLC_TX_STS_TOSS_UNKNOWN             = 53, */ \
	"toss_non_ampdu_scb",       /* WLC_TX_STS_TOSS_NON_AMPDU_SCB       = 54, */ \
	"toss_bad_ini",             /* WLC_TX_STS_TOSS_BAD_INI             = 55, */ \
	"toss_bad_addr",            /* WLC_TX_STS_TOSS_BAD_ADDR            = 56, */ \
	"toss_wet",                 /* WLC_TX_STS_TOSS_WET                 = 57, */ \
	"txs_unknown",              /* WLC_TX_STS_UNKNOWN                  = 58, */ \
	"tx_nobuf",                 /* WLC_TX_STS_NO_BUF                   = 59, */ \
	"tx_comp_no_scb",           /* WLC_TX_STS_COMP_NO_SCB              = 60, */ \
	"tx_bad_ncons",             /* WLC_TX_STS_BAD_NCONS                = 61, */ \
	"toss_wlcif_removed"        /* WLC_TX_STS_TOSS_WLCIF_REMOVED       = 62, */

/** Remap HW assigned suppress code to software suppress code */
int wlc_tx_status_map_hw_to_sw_supr_code(wlc_info_t * wlc, int supr_status);

/** In case of non full dump, only total tx and total failures are counted.
 * For any failure only the failure correspondign to index 1 will
 * be incremented.
 * WLC_TX_STS_LAST can be updated to required amount of granularity.
 * In order to achieve this entries in wlc_tx_status_t need to be
 * rearranged to have required field within the required MIN STATS range
 */
#ifdef WLC_TXSTALL_FULL_STATS
#define WLC_TX_STS_MAX WLC_TX_STS_LAST
#else
#define WLC_TX_STS_MAX	2
#endif /* WLC_TXSTALL_FULL_STATS */

#define TX_STS_RSN_MAX	(WLC_TX_STS_LAST)

#ifdef WL_TX_STALL
typedef struct wlc_tx_stall_error_info {
	uint32 stall_reason;
	uint32 stall_bitmap;
	uint32 stall_bitmap1;
	uint32 failure_ac;
	uint32 timeout;
	uint32 sample_len;
	uint32 threshold;
	uint32 tx_all;
	uint32 tx_failure_all;
#define WLC_TX_STALL_REASON_STRING_LEN 128
	char reason[WLC_TX_STALL_REASON_STRING_LEN];
#if defined(STA) && defined(WL_TX_CONT_FAILURE)
	uint32 txfail_consecutive_total;
	uint32 txfail_consecutive_flags;
	uint32 txfail_consecutive_seq;
	uint16 txfail_legacy_cnt[WL_NUM_RATES_CCK + WL_NUM_RATES_OFDM];
	uint16 txfail_mcs_cnt[BCMPHYCORENUM][WL_NUM_RATES_VHT + WL_NUM_RATES_EXTRA_VHT];
#endif /* STA && WL_TX_CONT_FAILURE */
} wlc_tx_stall_error_info_t;

/** Update the tx statistics for global counters, per bsscfg, per scb and per AC */
int wlc_tx_status_update_counters(wlc_info_t * wlc,
	void * pkt, scb_t * scb, wlc_bsscfg_t * bsscfg, int tx_status, int count);

/**
 * Input:
 *  - counters structure
 *  - reset_counters if sample len is more than threshold
 *  - Prefix to be used for logging and failure reason
 * Return
 *  - max failure rate
 *  - Failure info corresponding to max failure AC
 * This API will clear the counters per AC if sample count more than sample_count
 */
uint32
wlc_tx_status_calculate_failure_rate(wlc_info_t * wlc,
	wlc_tx_stall_counters_t * counters,
	bool reset_counters, const char * prefix, wlc_tx_stall_error_info_t * error);

/** TX Stall health check.
 * Verifies TX stall conditions and triggers fatal error on detecting the stall
 */
int wlc_tx_status_health_check(wlc_info_t * wlc);

/** Get the last report error */
wlc_tx_stall_error_info_t *
wlc_tx_status_get_error_info(wlc_info_t * wlc);

/** Reset TX stall statemachine */
int wlc_tx_status_reset(wlc_info_t * wlc, bool clear_all, wlc_bsscfg_t * cfg, scb_t * scb);

/** trigger a forced rx stall */
int
wlc_tx_status_force_stall(wlc_info_t * wlc, uint32 stall_type);

wlc_tx_stall_info_t *
BCMATTACHFN(wlc_tx_stall_attach)(wlc_info_t * wlc);
void
BCMATTACHFN(wlc_tx_stall_detach)(wlc_tx_stall_info_t * tx_stall);

int32
wlc_tx_status_params(wlc_tx_stall_info_t * tx_stall, bool set, int param, int value);
#define WLC_TX_STALL_IOV_THRESHOLD   (1)
#define WLC_TX_STALL_IOV_SAMPLE_LEN  (2)
#define WLC_TX_STALL_IOV_TIME        (3)
#define WLC_TX_STALL_IOV_FORCE       (4)
#define WLC_TX_STALL_IOV_EXCLUDE     (5)
#define WLC_TX_STALL_IOV_EXCLUDE1    (6)
#define WLC_TX_DELAY_IOV_TO_TRAP     (7)
#define WLC_TX_DELAY_IOV_TO_RPT      (8)
#define WLC_TX_FAILURE_IOV_TO_RPT    (9)

#define WL_TX_STS_UPDATE(a, b) (a) = (b)

#else /* WL_TX_STALL */

#define wlc_tx_status_update_counters(a, b, c, d, e, f)
#define wlc_tx_status_health_check(a)

#define WL_TX_STS_UPDATE(a, b)

#endif /* WL_TX_STALL */

#ifdef WL_TX_STALL
/** Holds total TX and failures.
 * index 0 - Total TX
 * Others - failure count of each type
 */
struct wlc_tx_stall_counters
{
	uint32 sysup_time[AC_COUNT];

	/* Total consecutive stalls detected */
	uint32 stall_count[AC_COUNT];

	uint16 tx_stats[AC_COUNT][WLC_TX_STS_MAX];
};

/* Stall detected for 9 seconds ( > beacon loss) will trigger fatal error */
#ifndef WLC_TX_STALL_PEDIOD
#define WLC_TX_STALL_PEDIOD         (12)
#endif // endif

/* Minimum sample len to be collected to validate stall  */
#ifndef WLC_TX_STALL_SAMPLE_LEN
#define WLC_TX_STALL_SAMPLE_LEN     (50)
#endif // endif

/* >75% total vs failed TX will treated as stall         */
#ifndef WLC_TX_STALL_THRESHOLD
#define WLC_TX_STALL_THRESHOLD      (90)
#endif // endif

#ifndef WLC_TX_STALL_EXCLUDE
#define WLC_TX_STALL_EXCLUDE         ((1 << WLC_TX_STS_TOSS_BSSCFG_DOWN) | \
					(1 << WLC_TX_STS_SUPR_BADCH)     | \
					(1 << WLC_TX_STS_SUPR_FRAG_TBTT) | \
					(1 << WLC_TX_STS_SUPR_EXPTIME)   | \
					(1 << WLC_TX_STS_RETRY_TIMEOUT)  | \
					(1 << WLC_TX_STS_SUPR_PMQ))
#endif // endif

#ifndef WLC_TX_STALL_EXCLUDE1
#define WLC_TX_STALL_EXCLUDE1        (0)
#endif // endif

#ifndef WLC_TX_STALL_ASSERT_ON_ERROR
#ifdef WL_DATAPATH_HC_NOASSERT
#define WLC_TX_STALL_ASSERT_ON_ERROR 0
#else
#define WLC_TX_STALL_ASSERT_ON_ERROR 1
#endif // endif
#endif /* WLC_TX_STALL_ASSERT_ON_ERROR */

#define WLC_TX_STALL_REASON_TX_ERROR            (1 << 0)
#define WLC_TX_STALL_REASON_TXQ_NO_PROGRESS     (1 << 1)
#define WLC_TX_STALL_REASON_RXQ_NO_PROGRESS     (1 << 2)

/* History of last 60 seconds */
#ifndef WLC_TX_STALL_COUNTERS_HISTORY_LEN
#define WLC_TX_STALL_COUNTERS_HISTORY_LEN   (64)
#endif // endif

struct wlc_tx_stall_info {
	wlc_info_t * wlc;
	int scb_handle;
	int cfg_handle;

	/* TX counters given validation period  */
	wlc_tx_stall_counters_t *counters;

	/* TX DROP/FAILURE reasons to be excluded */
	uint32 exclude_bitmap;
	uint32 exclude_bitmap1;

	/* History of N periods   */
	wlc_tx_stall_counters_t *history;
	uint32 history_idx;

	/* timeout value. 0 - Disable validation */
	uint32 timeout;

	/* Minimum number of packets to be accumulated before health check */
	uint32 sample_len;

	/* Success vs failure percentage to trigger failure   */
	uint32 stall_threshold;

	/* Test code to pretend tx_stall */
	bool force_tx_stall;

	/* Trigger fatal error on stall detection */
	bool assert_on_error;

	/* Stall info */
	wlc_tx_stall_error_info_t *error;

	/* More threshold */
	uint32	delay_to_trap;
	uint32	delay_to_rpt;
	uint32	failure_to_rpt;
};

#endif /* WL_TX_STALL */

#if defined(WL_TX_STALL)
int wlc_hc_tx_set(void *ctx, const uint8 *buf, uint16 type, uint16 len);
int wlc_hc_tx_get(wlc_info_t *wlc, wlc_if_t *wlcif,
	bcm_xtlv_t *params, void *out, uint o_len);
#endif /* WL_TX_STALL */

#if defined(BCMDBG)
void wlc_txq_dump_ampdu_txq(wlc_info_t *wlc,
	wlc_bsscfg_t * cfg, struct scb *scb, struct bcmstrbuf *b);
void wlc_txq_dump_ampdu_rxq(wlc_info_t *wlc,
	wlc_bsscfg_t * cfg, struct scb *scb, struct bcmstrbuf *b);
void wlc_txq_dump_amsdu_txq(wlc_info_t *wlc,
	wlc_bsscfg_t * cfg, struct scb *scb, struct bcmstrbuf *b);
void wlc_txq_dump_ap_psq(wlc_info_t *wlc,
	wlc_bsscfg_t * cfg, struct scb *scb, struct bcmstrbuf *b);
void wlc_txq_dump_supr_txq(wlc_info_t *wlc,
	wlc_bsscfg_t * cfg, struct scb *scb, struct bcmstrbuf *b);
#endif // endif

#ifdef WL_TXQ_STALL
int wlc_txq_health_check(wlc_info_t *wlc);
int wlc_txq_hc_global_txq(wlc_info_t *wlc);

int wlc_txq_hc_ampdu_txq(wlc_info_t *wlc, wlc_bsscfg_t * cfg, struct scb *scb);
int wlc_txq_hc_ampdu_rxq(wlc_info_t *wlc, wlc_bsscfg_t * cfg, struct scb *scb);
int wlc_txq_hc_awdl_rcq(wlc_info_t *wlc, wlc_bsscfg_t * cfg, struct scb *scb);
int wlc_txq_hc_ap_psq(wlc_info_t *wlc, wlc_bsscfg_t * cfg, struct scb *scb);
int wlc_txq_hc_supr_txq(wlc_info_t *wlc, wlc_bsscfg_t * cfg, struct scb *scb);
#endif /* WL_TXQ_STALL */
#ifdef WLATF_DONGLE
extern ratespec_t wlc_ravg_get_scb_cur_rspec(wlc_info_t *wlc, struct scb *scb);
extern uint32 wlc_scb_dot11hdrsize(struct scb *scb);
#endif /* WLATF_DONGLE */

/**
 * Calculate Length for long format TXD
 */
#define D11_TXH_LEN_EX(wlc)		(D11REV_GE((wlc)->pub->corerev, 128) ? \
					(D11_REV128_TXH_LEN_EX) : \
					(D11REV_GE((wlc)->pub->corerev, 40) ? \
					(D11AC_TXH_LEN) : \
					(D11_TXH_LEN + D11_PHY_HDR_LEN)))

/* Calculate Length for short format TXD */
#define D11_TXH_SHORT_LEN(wlc)		(D11REV_GE((wlc)->pub->corerev, 128) ? \
					(D11_REV128_TXH_SHORT_LEN) : \
					(D11REV_GE((wlc)->pub->corerev, 40) ? \
					(D11AC_TXH_SHORT_LEN) : \
					(D11_TXH_LEN + D11_PHY_HDR_LEN)))

/* Convenience macros to extract often used fields from (d11txhdr_t *) TXH */
#define D11_REV128_GET_MACLOW_PTR(txh)	&((txh)->rev128.MacControl_0)
#define D11_REV128_GET_MACHIGH_PTR(txh)	&((txh)->rev128.MacControl_1)
#define D11_REV128_GET_FRAMEID_PTR(txh)	&((txh)->rev128.FrameID)
#define D11_REV40_GET_MACLOW_PTR(txh)	&((txh)->rev40.PktInfo.MacTxControlLow)
#define D11_REV40_GET_MACHIGH_PTR(txh)	&((txh)->rev40.PktInfo.MacTxControlHigh)
#define D11_REV40_GET_FRAMEID_PTR(txh)	&((txh)->rev40.PktInfo.TxFrameID)
#define D11_PRE40_GET_MACLOW_PTR(txh)	&((txh)->pre40.MacTxControlLow)
#define D11_PRE40_GET_MACHIGH_PTR(txh)	&((txh)->pre40.MacTxControlHigh)
#define D11_PRE40_GET_FRAMEID_PTR(txh)	&((txh)->pre40.TxFrameID)
#define D11_TXH_GET_MACLOW_PTR(wlc, txh)	\
	(D11REV_GE((wlc)->pub->corerev, 128) ? D11_REV128_GET_MACLOW_PTR(txh) : \
	(D11REV_GE((wlc)->pub->corerev, 40) ? D11_REV40_GET_MACLOW_PTR(txh) : \
	D11_PRE40_GET_MACLOW_PTR(txh)))
#define D11_TXH_GET_MACHIGH_PTR(wlc, txh)	\
	(D11REV_GE((wlc)->pub->corerev, 128) ? D11_REV128_GET_MACHIGH_PTR(txh) : \
	(D11REV_GE((wlc)->pub->corerev, 40) ? D11_REV40_GET_MACHIGH_PTR(txh) : \
	D11_PRE40_GET_MACHIGH_PTR(txh)))
#define D11_TXH_GET_FRAMEID_PTR(wlc, txh)	\
	(D11REV_GE((wlc)->pub->corerev, 128) ? D11_REV128_GET_FRAMEID_PTR(txh) : \
	(D11REV_GE((wlc)->pub->corerev, 40) ? D11_REV40_GET_FRAMEID_PTR(txh) : \
	D11_PRE40_GET_FRAMEID_PTR(txh)))

#ifdef WLCFP
#define IS_COMMON_QUEUE(q) ((q)->common_queue)
#else
#define IS_COMMON_QUEUE(q) FALSE
#endif // endif

extern int BCMFASTPATH wlc_txq_enq_pkt(wlc_info_t *wlc, struct scb *scb, void *sdu, uint prec);
extern void BCMFASTPATH wlc_txq_enq_flowcontrol_ampdu(wlc_info_t *wlc, struct scb *scb, int prio);

#if defined(WL_PRQ_RAND_SEQ)
extern int
wlc_tx_prq_rand_seq_enable(wlc_info_t *wlc, wlc_bsscfg_t *cfg, bool enable);
#endif /* WL_PRQ_RAND_SEQ */

void *cpktq_deq(struct cpktq *cpktq, int *prec_out);
void *cpktq_pdeq(struct cpktq *cpktq, int prec);
void *cpktq_deq_tail(struct cpktq *cpktq, int *prec_out);
void *cpktq_pdeq_tail(struct cpktq *cpktq, int prec);
void *cpktq_mdeq(struct cpktq *cpktq, uint prec_bmp, int *prec_out);
void *cpktq_penq(struct cpktq *cpktq, int prec, void *p);
void *cpktq_penq_head(struct cpktq *pq, int prec, void *p);
bool cpktq_prec_enq(wlc_info_t *wlc, struct cpktq *cpktq, void *p, int prec);
bool cpktq_prec_enq_head(wlc_info_t *wlc, struct cpktq *q, void *p, int prec, bool head);
void cpktq_append(struct cpktq *cpktq, int prec, struct spktq *list);
void cpktq_flush(wlc_info_t *wlc, struct cpktq *cpktq);
void cpktq_dec(struct pktq *pq, void *pkt, uint32 npkt, int prec);

#if defined(WLCFP) && defined(BCM_DHDHDR)
extern void wlc_tx_adjust_hostfrag(wlc_info_t *wlc, void* p, uint16 len);
#endif // endif

/**
 * HW expects the TOE header to contain Frametypes in the following format
 * - Data Frames = 0
 * - Management = 1
 * - Control = 2
 * But the FC Types have different values.
 *
 * Use this Macro for converting FC frametype to TXH expected frametype
 */
#define GE128_TXH_FT_DATA	0
#define GE128_TXH_FT_MNG		1
#define GE128_TXH_FT_CTL		2

#define GE128_FC_2_TXH_FT(fc_type)	((fc_type == (FC_TYPE_DATA)) ? (GE128_TXH_FT_DATA) : \
					(fc_type == (FC_TYPE_MNG)) ? (GE128_TXH_FT_MNG) : \
					(GE128_TXH_FT_CTL))

extern uint16 wlc_get_txh_frameid(wlc_info_t *wlc, void *p);
extern void wlc_set_txh_frameid(wlc_info_t *wlc, void *p, uint16 frameid);
extern uint16 wlc_get_txh_mactxcontrolhigh(wlc_info_t *wlc, void *p);
extern void wlc_set_txh_mactxcontrolhigh(wlc_info_t *wlc, void *p, uint16 mactxcontrolhigh);
extern uint16 wlc_get_txh_abi_mimoantsel(wlc_info_t *wlc, void *p);
extern void wlc_set_txh_abi_mimoantsel(wlc_info_t *wlc, void *p, uint16 abi_mimoantsel);

/* TX power offset format translation */
#define SCALE_5_1_TO_5_2_FORMAT(x)	(((x) == (uint8)WL_RATE_DISABLED) ? \
	(uint8)WL_RATE_DISABLED : ((x) << 1))

extern uint8 wlc_get_bcnprs_txpwr_offset(wlc_info_t *wlc, uint8 txpwrs);

#define MUAGG_VHTMU_BUCKET 0
#define MUAGG_HEMUMIMO_BUCKET 1
#define MUAGG_HEOFDMA_BUCKET 2
#define MUAGG_SUM_BUCKET 3
#define MUAGG_NUM_BUCKETS (MUAGG_SUM_BUCKET+1)

#define MAX_VHTMU_USERS 4
#define MAX_HEMUMIMO_USERS 4
#define MAX_HEOFDMA_USERS 16
#define MAX_MUAGG_USERS 16

typedef struct {
	uint32 sum;
	uint32 num_ppdu[MAX_MUAGG_USERS];
} muagg_histo_bucket_t;

typedef struct {
	muagg_histo_bucket_t bucket[MUAGG_NUM_BUCKETS];
} muagg_histo_t;

extern void wlc_get_muagg_histo(wlc_info_t *wlc, muagg_histo_t *histo);
extern void wlc_clear_muagg_histo(wlc_info_t *wlc);

#ifdef WLWRR
extern void wlc_tx_wrr_update_ncons(struct scb *scb, uint8 ac, uint32 ncons);
#endif // endif

/* enum for different types Multi Users technologies */
typedef enum {
	MU_TYPE_NONE             = 0, /* No MU means SU */
	MU_TYPE_VHTMU            = 1, /* VHT MUMIMO */
	MU_TYPE_HEMU             = 2, /* HE MUMIMO */
	MU_TYPE_OFDMA            = 3, /* HE MU OFDMA */
	MU_TYPE_HEMOM            = 4, /* HE MUMIMO + OFDMA */
	MU_TYPE_TWT              = 5, /* TWT */
	MU_TYPE_ULOFDMA		 = 6, /* Uplink OFDMA */
} mutype_t;

#endif /* _wlc_tx_h_ */
