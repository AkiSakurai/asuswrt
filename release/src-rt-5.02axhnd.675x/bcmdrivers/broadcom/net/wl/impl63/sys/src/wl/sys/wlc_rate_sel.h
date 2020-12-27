/*
 * Common OS-independent driver header file for rate selection
 * algorithm of Broadcom 802.11b DCF-only Networking Adapter.
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
 *
 * $Id: wlc_rate_sel.h 779517 2019-10-01 14:48:46Z $
 */

#ifndef	_WLC_RATE_SEL_H_
#define	_WLC_RATE_SEL_H_

/* flags returned by wlc_ratesel_gettxrate() */
#define RATESEL_VRATE_PROBE	0x0001	/* vertical rate probe pkt; use small ampdu */

/* constants for alert mode */
#define RATESEL_ALERT_IDLE	1	/* watch out for idle time only */
#define RATESEL_ALERT_RSSI	2	/* watch out for rssi change only */
#define RATESEL_ALERT_PRI	4	/* adjust primary rate: only if alert_rssi is ON */
#define RATESEL_ALERT_ALL	0x7

#define RATESEL_UPDFLAG_TXERR		0x0001	/* txerr */
#define RATESEL_UPDFLAG_NORTSELUPD	0x0002	/* no ratesel upd, just reset txepoch */

#define PSR_UNINIT                     0xFFFFFFFF      /* Mark the PSR value as uninitialized */

/* flags used by wlc_ratesel_updinfo_xx as input */
#define RTSTAT_UPD_DL	(0 << 0) /* update dllink info */
#define RTSTAT_UPD_UL	(1 << 0) /* update uplink info */
#define RTSTAT_UPD_TX   (0 << 1) /* update from tx */
#define RTSTAT_UPD_TXS  (1 << 1) /* update from txs */
#define RTSTAT_UPD_SU   (1 << 2) /* update SU expliclity -- from txs for normalack */
/* flags used to get stats */
#define RTSTAT_GET_DIR  (1 << 0) /* = 0/1: DL/UL */
#define RTSTAT_GET_TXS  (1 << 1) /* = 0/1: get stats from tx/txs */
#define RTSTAT_GET_TYPE_SHIFT 2
#define RTSTAT_GET_TYPE_MASK (3 << 2) /* = 0/1/2: get any/su/mu */
#define RTSTAT_GET_TYPE_ANY   0
#define RTSTAT_GET_TYPE_SU    1
#define RTSTAT_GET_TYPE_MU    2

enum {
	RTSTAT_TYPE_INIT = 0,  /* just init, hasn't got any txs */
	RTSTAT_TYPE_SU   = 1,  /* last txs is for su */
	RTSTAT_TYPE_MU   = 2,  /* last txs is mu: any-mu starts from here */
};
#define RTSTAT_TYPE_VHTMU	(RTSTAT_TYPE_MU + TX_STATUS_MUTP_VHTMU)
#define RTSTAT_TYPE_HEMM	(RTSTAT_TYPE_MU + TX_STATUS_MUTP_HEMM)
#define RTSTAT_TYPE_HEOM	(RTSTAT_TYPE_MU + TX_STATUS_MUTP_HEOM)
#define RTSTAT_TYPE_HEMOM	(RTSTAT_TYPE_MU + TX_STATUS_MUTP_HEMOM)

typedef void rssi_ctx_t;
typedef struct rcb rcb_t;
typedef struct rcb_itxs rcb_itxs_t;
typedef struct rcb_rate_stats rcb_rate_stats_t;

#if defined(D11AC_TXD)
/* D11 rev >= 40 supports 4 txrates, earlier revs support 2 */
#define RATESEL_TXRATES_MAX 4
#else
#define RATESEL_TXRATES_MAX 2
#endif /* defined(D11AC_TXD) */

#define RATESEL_MFBR_NUM      4

typedef struct ratesel_txparams {
	uint8 num; /* effective # of rates */
	ratespec_t rspec[RATESEL_TXRATES_MAX];
	uint8 antselid[RATESEL_TXRATES_MAX];
	uint8 ac;
} ratesel_txparams_t;

typedef struct ratesel_txs {
	ratespec_t txrspec[RATESEL_MFBR_NUM];
	uint16 tx_cnt[RATESEL_MFBR_NUM];
	uint16 txsucc_cnt[RATESEL_MFBR_NUM];
	uint16 txrts_cnt;
	uint16 rxcts_cnt;
	uint16 ncons;
	uint16 nlost;
	uint32 ack_map1;
	uint32 ack_map2;
	uint16 frameid;
	uint8 antselid;
	uint8 ac;
} ratesel_txs_t;

/* for trigger txstatus */
typedef struct ratesel_tgtxs {
	ratespec_t txrspec;
	uint8  rtidx;
	uint8  txcnt;
	uint8  ruidx;
	uint16 gdelim;
	uint16 gfcs;
} ratesel_tgtxs_t;

typedef struct ratesel_init {
	rcb_t		*state;		/* pointer to cubby area where rate sel state is kept */
	uint		rcb_sz;
	void		*scb;		/* pointer to scb */
	wlc_rateset_t	*rateset;	/* pointer to rates */
	uint8		bw;		/* bandwidth */
	int8		sgi_tx;		/* SGI flags */
	uint8		ldpc_tx;	/* flag indicate if ldpc is on */
	uint8		vht_ratemask;	/* permission bitmap for rates */
	uint8		active_antcfg_num; /* number of antenna configured */
	uint8		antselid_init;	/* index to max mimo antconfig */
	uint32		max_rate;	/* maximum rate rate */
	uint32		min_rate;	/* minimum rate */
	chanspec_t	chspec;		/* channel spec */
	bool		bw_auto;	/* if use bw_auto policy */
} ratesel_init_t;

#define HT_LDPC		(1<<0)
#define VHT_LDPC	(1<<1)
#define HE_LDPC		(1<<2)

extern ratesel_info_t *wlc_ratesel_attach(wlc_info_t *wlc);
extern void wlc_ratesel_detach(ratesel_info_t *rsi);

#if defined(BCMDBG)
extern int wlc_ratesel_scbdump(rcb_t *state, struct bcmstrbuf *b);
extern int wlc_ratesel_get_fixrate(rcb_t *state, int ac, struct bcmstrbuf *b);
extern int wlc_ratesel_set_fixrate(rcb_t *state, int ac, uint8 val);
#endif // endif
extern void wlc_ratesel_dump_rateset(rcb_t *state, struct bcmstrbuf *b);
extern bool wlc_ratesel_set_alert(ratesel_info_t *rsi, uint8 mode);

/* initialize per-scb state utilized by rate selection */
extern void wlc_ratesel_init(ratesel_info_t *rsi, ratesel_init_t * ratesel_init);
#if defined(WL_MU_TX)
extern void wlc_ratesel_init_rcb_itxs(rcb_itxs_t *rcb_itxs);
#endif /* defined(WL_MU_TX) */

extern void wlc_ratesel_rfbr(rcb_t *state);
/* update per-scb state upon received tx status */
extern void wlc_ratesel_upd_txstatus_normalack(rcb_t *state,
	tx_status_t *txs, uint16 sfbl, uint16 lfbl,
	uint8 tx_mcs, bool sgi, uint8 antselid, bool fbr);

/* change the throughput-based algo parameters upon ACI mitigation state change */
extern void wlc_ratesel_aci_change(ratesel_info_t *rsi, bool aci_state);

/* update per-scb state upon received tx status for ampdu */
extern void wlc_ratesel_upd_txs_blockack(rcb_t *state,
	tx_status_t *txs, uint8 suc_mpdu, uint8 tot_mpdu,
	bool ba_lost, uint8 retry, uint8 fb_lim, bool tx_error,
	uint8 tx_mcs, bool sgi, uint8 antselid);

#if (defined(WLAMPDU_MAC) || defined(D11AC_TXD))
extern void wlc_ratesel_upd_txs_ampdu(rcb_t *state, rcb_itxs_t *rcb_itxs,
	ratesel_txs_t *rs_txs, tx_status_t *txs, uint16 flags);
#endif /* (defined(WLAMPDU_MAC) || defined(D11AC_TXD)) */

#if defined(WL_MU_TX)
extern void wlc_ratesel_upd_itxs(rcb_t *state, rcb_itxs_t *rcb_itxs,
	ratesel_txs_t *rs_txs, tx_status_t *txs, uint16 flags);
#if defined(WL11AX)
extern void wlc_ratesel_upd_txs_trig(rcb_t *state, ratesel_tgtxs_t *tgtxs, tx_status_t *txs);
extern ratespec_t wlc_ratesel_get_ulrt_rspec(rcb_t *state, uint8 rtidx);
extern uint8 wlc_ratesel_get_ulrt_trssi(rcb_t *state);
#endif /* defined(WL11AX) */
#endif /* defined(WL_MU_TX) */

/* update rate_sel if a PPDU (ampdu or a reg pkt) is created with probe values */
extern void wlc_ratesel_probe_ready(rcb_t *state, uint16 frameid,
	bool is_ampdu, uint8 ampdu_txretry);
extern void wlc_ratesel_upd_rxstats(ratesel_info_t *rsi, ratespec_t rx_rspec, uint16 rxstatus2);

bool wlc_ratesel_sync(rcb_t *state, uint now, int rssi);
ratespec_t wlc_ratesel_getcurspec(rcb_t *state);

/* select transmit rate given per-scb state */
extern bool wlc_ratesel_gettxrate(rcb_t *state,
	uint16 *frameid, ratesel_txparams_t *cur_rate, uint16 *flags);
extern ratespec_t wlc_ratesel_get_opstats(rcb_t *state, uint16 flags);

/* get the fallback rate of the specified mcs rate */
extern ratespec_t wlc_ratesel_getmcsfbr(rcb_t *state, uint8 ac, uint8 plcp0);

extern bool wlc_ratesel_minrate(rcb_t *state, tx_status_t *txs);

extern void wlc_ratesel_rcb_sz(uint16 *rcb_sz, uint16 *rcb_itxs_sz);

/* rssi context function pointers */
typedef void (*disable_rssi_t)(rssi_ctx_t *ctx);
typedef int (*get_rssi_t)(rssi_ctx_t *);
typedef void (*enable_rssi_t)(rssi_ctx_t *ctx);
extern void wlc_ratesel_rssi_attach(ratesel_info_t *rsi, enable_rssi_t en_fn,
	disable_rssi_t dis_fn, get_rssi_t get_fn);

#ifdef BCMDBG
extern void wlc_ratesel_dump_rcb(rcb_t *rcb, int32 ac, struct bcmstrbuf *b);
#endif // endif

#define RATESEL_MSG_INFO_VAL	0x01 /* concise rate change msg in addition to WL_RATE */
#define RATESEL_MSG_MORE_VAL	0x02 /* verbose rate change msg */
#define RATESEL_MSG_SP0_VAL	0x04 /* concise spatial/tx_antenna probing msg */
#define RATESEL_MSG_SP1_VAL	0x08 /* verbose spatial/tx_antenna probing msg */
#define RATESEL_MSG_RXA0_VAL	0x10 /* concise rx atenna msg */
#define RATESEL_MSG_RXA1_VAL	0x20 /* verbose rx atenna msg */
#define RATESEL_MSG_SGI0_VAL	0x40 /* concise sgi probe msg */
#define RATESEL_MSG_SGI1_VAL	0x80 /* verbose sgi probe msg */
#define RATESEL_MSG_TXS_VAL	0x100 /* verbose txs msg */
#define RATESEL_MSG_CHG_VAL	0x200 /* verbose rate_chg msg */
#define RATESEL_MSG_RMEM_VAL	0x400 /* concise ratemem msg */
#define RATESEL_MSG_RMEM1_VAL	0x800 /* verbose ratemem msg */
#define RATESEL_MSG_ULRX_VAL	0x1000 /* ulrt msg */
#define RATESEL_MSG_DTPC_COMPACT_VAL  0x2000
#define RATESEL_MSG_DTPC_DBG_VAL      0x4000
#define RATESEL_MSG_RTSTAT_VAL   0x8000

#ifdef WL_LPC
extern void	wlc_ratesel_lpc_update(ratesel_info_t *rsi, rcb_t *state);
extern void wlc_ratesel_lpc_init(rcb_t *state);
extern void wlc_ratesel_get_info(rcb_t *state, uint8 rate_stab_thresh, uint32 *new_rate_kbps,
	bool *rate_stable, rate_lcb_info_t *lcb_info);
extern void wlc_ratesel_clr_cache(rcb_t *state);
#endif /* WL_LPC */
extern void wlc_ratesel_filter_minrateset(wlc_rateset_t *rateset, wlc_rateset_t *new_rateset,
	bool is40bw, uint8 min_rate);
extern void wlc_ratesel_set_link_bw(rcb_t *rcb, uint8 link_bw);
extern uint8 wlc_ratesel_get_link_bw(rcb_t *rcb);
#ifdef WLATF
/* Get raw unprocessed current ratespec, used by ATF */
ratespec_t wlc_ratesel_rawcurspec(rcb_t *state);
#endif /* WLATF */
extern void wlc_ratesel_get_ratecap(rcb_t * state, uint8 *sgi, uint16 mcs_bitmap[]);

#ifdef WLC_DTPC
extern uint8 wlc_ratesel_change_epoch(rcb_t *state);
extern int wlc_ratesel_upd_pwroffset(rcb_t *state);

#endif /* WLC_DTPC */

/*
 * First nibble to rate;
 * Second nibble to spatial/extended spatial probing;
 * Third to rx antenna.
 */
extern uint16 ratesel_msglevel;
#define RL_INFO(args)	do {if (WL_RATE_ON() && (ratesel_msglevel & RATESEL_MSG_INFO_VAL)) \
				WL_PRINT(args);} while (0)
#define RL_MORE(args)	do {if (WL_RATE_ON() && (ratesel_msglevel & RATESEL_MSG_MORE_VAL)) \
				WL_PRINT(args);} while (0)
#define RL_SP0(args)	do {if (WL_RATE_ON() && (ratesel_msglevel & RATESEL_MSG_SP0_VAL)) \
				WL_PRINT(args);} while (0)
#define RL_SP1(args)	do {if (WL_RATE_ON() && (ratesel_msglevel & RATESEL_MSG_SP1_VAL)) \
				WL_PRINT(args);} while (0)
#define RL_RXA0(args)	do {if (WL_RATE_ON() && (ratesel_msglevel & RATESEL_MSG_RXA0_VAL)) \
				WL_PRINT(args);} while (0)
#define RL_RXA1(args)	do {if (WL_RATE_ON() && (ratesel_msglevel & RATESEL_MSG_RXA1_VAL)) \
				WL_PRINT(args);} while (0)
#define RL_SGI0(args)	do {if (WL_RATE_ON() && (ratesel_msglevel & RATESEL_MSG_SGI0_VAL)) \
				WL_ERROR(args);} while (0)
#define RL_SGI1(args)	do {if (WL_RATE_ON() && (ratesel_msglevel & RATESEL_MSG_SGI1_VAL)) \
				WL_PRINT(args);} while (0)
#define RL_CHG(args)	do {if (WL_RATE_ON() && (ratesel_msglevel & RATESEL_MSG_CHG_VAL)) \
				WL_PRINT(args);} while (0)
#define RL_TXS(args)	do {if (WL_RATE_ON() && (ratesel_msglevel & RATESEL_MSG_TXS_VAL)) \
				WL_PRINT(args);} while (0)
#define RL_RMEM(args)	do {if (WL_RATE_ON() && (ratesel_msglevel & RATESEL_MSG_RMEM_VAL)) \
				WL_PRINT(args);} while (0)
#define RL_RMEM1(args)	do {if (WL_RATE_ON() && (ratesel_msglevel & RATESEL_MSG_RMEM1_VAL)) \
				WL_PRINT(args);} while (0)
#define RL_ULRX(args)	do {if (WL_RATE_ON() && (ratesel_msglevel & RATESEL_MSG_ULRX_VAL)) \
				WL_PRINT(args);} while (0)
#define RL_DTPC_CMP(args)	do {if (WL_RATE_ON() && (ratesel_msglevel & \
				RATESEL_MSG_DTPC_COMPACT_VAL))  WL_PRINT(args);} while (0)
#define RL_DTPC_DBG(args)	do {if (WL_RATE_ON() && (ratesel_msglevel & \
				RATESEL_MSG_DTPC_DBG_VAL)) WL_PRINT(args);} while (0)
#define RL_STAT(args)	do {if (WL_RATE_ON() && (ratesel_msglevel & RATESEL_MSG_RTSTAT_VAL)) \
				WL_PRINT(args);} while (0)
#endif	/* _WLC_RATE_H_ */
