/**
 * @file
 * @brief
 * Common [OS-independent] rate selection algorithm of Broadcom
 * 802.11 Networking Adapter Device Driver.
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
 * $Id: wlc_rate_sel.c 780843 2019-11-05 02:01:23Z $
 */

/**
 * @file
 * @brief
 * XXX Twiki: [WlRateSelection]
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <wlioctl.h>

#include <802.11.h>
#include <d11.h>

#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wlc_txc.h>

#include <wlc_phy_hal.h>
#include <wlc_antsel.h>
#include <wlc_rate_sel.h>
#ifdef WL_LPC
#include <wlc_scb_powersel.h>
#endif // endif

#include <wl_dbg.h>
#include <wlc_ht.h>
#include <wlc_dump.h>
#include <wlc_stf.h>

#include <wlc_ratelinkmem.h>
#include <wlc_ampdu.h>
#ifdef WLAMSDU_TX
#include <wlc_amsdu.h>
#endif /* WLAMSDU_TX */
#include <wlc_macdbg.h>
#include <wlc_musched.h>
#include <wlc_ulmu.h>

#ifdef WLC_DTPC
#include <wlc_dtpc.h>
#endif /* WLC_dtpc */

#if defined(WLPROPRIETARY_11N_RATES)
#define WL11N_256QAM     /* 11n proprietary rates: mcs8/9 for each stream */
#endif // endif

#define MAXRATERECS	224		/* Maximum rate updates */

enum {
	SP_NONE,	/* no spatial probing */
	SP_NORMAL,	/* family probing without antenna select */
	SP_EXT		/* extended spatial probing (with antenna select) */
};

enum {
	TXS_REG,
	TXS_PROBE,
	TXS_DISC
};

#define	MPDU_MCSENAB(state)	(state->mcs_streams > 0 ? TRUE : FALSE)

#define MAXSPRECS		24 /* Maximum spatial AMPDU updates (1 bit values) */
#define MAX_EXTSPRECS		30 /* Maximum extended spatial AMPDU updates (1 bit values) */

#define SPATIAL_M		18	/* Spatial probe window size */
#define SPFLGS_EN_MASK		0x0001
#define SPFLGS_VAL_MASK		0x0010
#define EXTSPATIAL_M		18	/* Extented Spatial Probing window size */
#define EXTSPFLGS_EN_MASK	0x0001  /* requesting extended spatial probing frames */
#define EXTSPFLGS_XPR_VAL_MASK	0x0010  /* x-probe setup valid */
#define EXTSPFLGS_IPR_VAL_MASK	0x0100  /* i-probe setup valid */
#define ANT_SELCFG_MAX_NUM	4

#define I_PROBE          0       /* ext spatial toggle value for i-probe */
#define X_PROBE          1       /* ext spatial toggle value for x-probe */

#define MAXTXANTRECS		24	/* Maximum antenna history updated */
#define TXANTHIST_M		18	/* Antenna history window size */
#define TXANTHIST_K		14	/* Threshold, must be > TXANTHIST_M */
#define TXANTHIST_NMOD		10	/* Number of BA between history updates */

/* Warning: use this macro only when don't care about sgi */
#define RATESPEC_OF_I(state, i) (known_rspec[state->select_rates[(i)]] | \
				 ((state)->bw[(i)] << WL_RSPEC_BW_SHIFT))

#define RATEKBPS_OF_I(state, i)	(wf_rspec_to_rate(RATESPEC_OF_I(state, i)))
/* care sgi */
#define CUR_RATESPEC(state)	(RATESPEC_OF_I(state, state->rateid))

/* SGI probing related */
#define SGI_TMIN_DEFAULT		1
#define SGI_TMAX_DEFAULT		16
#define SGI_PROBE_NUM			10
enum {
	SGI_DISABLED = -1,
	SGI_RESET = 0,
	SGI_PROBING,
	SGI_PROBE_END = SGI_PROBING + SGI_PROBE_NUM, /* 11 */
	SGI_ENABLED /* 12 */
};

#define SGI_PROBING(state)	((state)->sgi_state >= SGI_PROBING && \
				 (state)->sgi_state <= SGI_PROBE_END)
#define SP_PROBING(state)	((state)->mcs_sp_flgs & SPFLGS_EN_MASK)
#define EXTSP_PROBING(state)	((state)->extsp_flgs & EXTSPFLGS_EN_MASK)

#define MCS_STREAM_MAX_UNDEF	0	/* not specified */
#define MCS_STREAM_MAX_1	1	/* pick mcs rates with one streams */
#define MCS_STREAM_MAX_2	2	/* pick mcs rates with up to two streams */
#define MCS_STREAM_MAX_3	3	/* pick mcs rates with up to three streams */
#define MCS_STREAM_MAX_4	4	/* pick mcs rates with up to four streams */

#ifndef MCS_STREAM_MAX
#define MCS_STREAM_MAX MCS_STREAM_MAX_4
#endif // endif

/* MAX_MCS_NUM is the ultimate limit per spec but that wastes lots of memory, increase as needed */
#ifdef WL11N_256QAM
#define MAX_MCS_NUM_NOW		(1 + 10 * MCS_STREAM_MAX_3) /* 1 for m32. Total 31 */
#else
#define MAX_MCS_NUM_NOW		(1 + 8 * MCS_STREAM_MAX_3) /* 1 for m32. Total 25 */
#endif // endif
#define MAX_SHORT_AMPDUS	6	/* max number of short AMPDUs when switching to
					 * next higher rate in vertical rate control
					 */

#ifndef UINT32_MAX
#define UINT32_MAX		0xFFFFFFFF
#endif // endif

#define NOW(state)		(state->rsi->pub->now)

#define RSSI_LMT_CCK		(-81)

/* This is how we update PSR (Packet Success Ratio, = 1 - PER).
 * Notation:
 *     Xn -- new value, Xn-1 -- old value, Xt -- current sample, all in [0, 1]
 *     NF -- normalize factor.
 * Update equation (original form):
 *   Xn <-- Xn-1 + (Xt - Xn-1) / 2^alpha
 *
 * (1) Scale up Xn, Xn-1 and Xt by 2^NF since we don't use float.
 *     Let Y <-- (X << NF), we have
 *     Yn <-- Yn-1 + Yt/2^alpha - (Yn-1)/2^alpha.
 * (2) We need to carefully choose NF over alpha to retain certain
 *     accuracy as well as to avoid overflowing the uint32 representation
 *     when multiplying the phy rate in unit of 500Kbps (maximum 540, 0x21C).
 * (2-1) Choose alpha = 4 that provides the actual alpha in EWA, 0.9375.
 * (2-2) Choose NF such that Xt = 1/64 (64 is the maximum MPDUs per A-MPDU)
 *     will be counted in the update. Hence, NF >= 6+alpha = 10.
 * (2-3) Meanwhile, NF shall satisfy 0x21C << NF <= 0xFFFFFFFF
 *       ==> NF < 20 is a stricter condition.
 * (2-4) As the result, choose NF = 12 to allow certain range of fine tuning alpha.
 */
/* exponential moving average coefficients. Actual alpha = 15/16 = 0.9375. */
#define RATESEL_EMA_NF			12	/* normalization factor */
#define RATESEL_EMA_ALPHA_INIT		2	/* value to use before gotpkts becomes true */
#define RATESEL_EMA_ALPHA_DEFAULT	4	/* stage 1 alpha value */
#define RATESEL_EMA_ALPHA0_DEFAULT	2	/* stage 0 alpha value */
#define RATESEL_EMA_ALPHA_MAX		8
#define RATESEL_EMA_ALPHA_MIN		2
#define RATESEL_EMA_NUPD_THRES_DEF	10
#define MIN_NUPD_DEFAULT		30	/* in align with alpha_default */

#define PSR_MAX				(1 << RATESEL_EMA_NF)
#define AGE_LMT(state)			(state->rsi->age_lmt)	/* 1 seconds, max cache time. */
#define NCFAIL_LMT0			4
#define NCFAIL_LMT			16

/* Moving average equation:
 *  Y_n <-- Y_n-1 + (Y_t - Y_n-1) >> alpha
 * Input:
 *   psr = Y_n-1
 *   psr_cur = Y_t (= ntx_succ << NF / ntx)
 */
#define UPDATE_MOVING_AVG(p, cur, alpha) {*p -= (*p) >> alpha; *p += cur >> alpha;}

#define DUMMY_UINT8			(0xFF)
#define FIXPRI_FLAG			(0x80)
#define RATEID_AUTO			(0xFF)
#define DUMMY_UINT16			(0xFFFF)

/* local macros */
#define MODDECR(i, k)		(((i) > 0)       ? (i)-1 : (k)-1)
#define MODINCR(i, k)		(((i) < ((k)-1)) ? (i)+1 : 0)
#define LIMDEC(i, k)		(((i) > ((k)+1)) ? (i)-1 : (k))
#define LIMINC(i, k)		(((i) < ((k)-1)) ? (i)+1 : (k)-1)
#define LIMSUB(i, k, s)		(((i) > ((k)+(s))) ? (i)-(s) : (k))
#define LIMADD(i, k, s)		(((i) < ((k)-(s))) ? (i)+(s) : (k))
#define LIMINC_UINT32(k)	if ((k) != BCM_UINT32_MAX) (k)++

#define RATES_NUM_CCK		(4)		/* cck */
#define RATES_NUM_CCKOFDM	(12)		/* cck/ofdm */

/* define superset of 11ac & 11ax (mcs0-11) and 11n (mcs0-23, 23), and proprietary mcs8-9 */
#ifdef WL11AX
#define RATES_NUM_MCS		(1 + MAX_HE_RATES * MCS_STREAM_MAX_4)
#elif defined(WL11AC) || defined(WL11N_256QAM)
#define RATES_NUM_MCS		(1 + MAX_VHT_RATES * MCS_STREAM_MAX_4)
#else
#define RATES_NUM_MCS		(1 + 8 * MCS_STREAM_MAX_3)
#endif /* WL11AX */

#define RATES_NUM_ALL           (RATES_NUM_CCKOFDM + RATES_NUM_MCS)
/* XXX:
 * XXX: Several factors contributes the rate size:
 * XXX:  at band b, upto 4 CCK rates are padded to rate set
 * XXX:  at band a, bw2040in80 are in place.
 * XXX:             nss1 has (2/4/6 -1) rates added to bw40/80/160, respectively
 * XXX:             nss2 has (3/6/8 -1) rates added to bw40/80/160, respectively
 * XXX:             bw160 only supports nss2 so far.
 */
#define MAX_RATESEL_NUM_OLD	(RATES_NUM_CCK + RATES_NUM_MCS)	/* max # rates to use */
#define MAX_RATESEL_NUM		(RATES_NUM_CCK + RATES_NUM_MCS + 6+4-2-RATES_NUM_CCK) /* add 4 */
#define PAD_RATESEL_NUM		(MAX_RATESEL_NUM_OLD - sizeof(uint8 *))

#ifdef WL11AC
#define SPMASK_IN_MCS		0xf0
#define MCS8			0x20
#define MCS12			0x24
#define MCS21			0x35
#else
#define SPMASK_IN_MCS		0x78
#define MCS8			8
#define MCS12			12
#define MCS21			21
#endif /* WL11AC */

#ifdef WL11N_256QAM
const uint8 mcs256q_map[MCS_STREAM_MAX_3][2] = {
	{87, 88}, {99, 100}, {101, 102}
};
#endif // endif

#define GOTPKTS_INIT		0x1  /* cleared if in init */
#define GOTPKTS_RFBR		0x2  /* cleared if in rfbr */
#define GOTPKTS_PRICHG		0x4  /* cleared if using chicken pri-rate */
#define GOTPKTS_ALL		(GOTPKTS_INIT | GOTPKTS_RFBR | GOTPKTS_PRICHG)

#define IDLE_LMT		3	/* in seconds */
#define RSSI_PEAK_LMT		(-65)	/* expect rate start to drop from peak below this rssi */
#define RSSI_RFBR_LMT		10	/* for no mfbr case */
#define RSSI_RFBR_LMT_MFBR	15	/* for mfbr */

#define RSSI_PRI_ON 0

#if RSSI_PRI_ON
#define RSSI_PRI_LMT		10	/* for no mfbr case */
#define RSSI_PRI_LMT_MFBR	20	/* for mfbr */
#define RSSI_STEP		6	/* step size to lower primary */
#endif // endif

#define RSPEC2RATE500K(rspec)	(wf_rspec_to_rate(rspec)/500)

enum {
	RATE_UP_ROW,
	RATE_DN_ROW,
	RATE_UDSZ
};

/*  psr_info_array_index */
enum {
	PSR_CUR_IDX,	/* current rate */
	PSR_DN_IDX,	/* current rateid-1 */
	PSR_UP_IDX,	/* current rateid+1 */
	PSR_FBR_IDX,	/* the fallback rate */
	PSR_ARR_SIZE	/* number of records */
};

typedef struct ratesel_dlstats {
	ratespec_t last_txrspec; /* from tx/control path */
	uint8 last_type;       /* last su/mu type: rtstat_type_xxx enum */
	ratespec_t last_rspec; /* last rspec -- any rate, su or mu */
	ratespec_t last_su;    /* last su rspec */
	ratespec_t last_mu;    /* last mu rspec */
	uint32 avg_su100kbps;  /* avg su rate in 100kbps */
	uint32 avg_mu100kbps;  /* avg mu rate in 100kbps */
	uint32 avg_rt100kbps;  /* avg any rate */
} ratesel_dlstats_t;

#define IS_20BW(state)	((state)->bwcap == BW_20MHZ)

#ifdef WL11N_SINGLESTREAM
#define	SP_MODE(state)	SP_NONE
#else
#define	SP_MODE(state)	((state)->spmode)
#endif // endif

#define RATE_EPOCH_MASK		TXFID_RATE_MASK // 0x00e0
#define RATE_EPOCH_SHIFT	TXFID_RATE_SHIFT // 5

#define RATE_EPOCH_MAX(_rev) ((D11REV_GE(_rev, 128)) ? \
	(D11_REV128_RATE_EPOCH_MASK >> D11_REV128_RATE_EPOCH_SHIFT) : \
	(RATE_EPOCH_MASK >> RATE_EPOCH_SHIFT))

#define TXS_RATE_EPOCH(_rev, txs) ((D11REV_GE(_rev, 128)) ? \
	((TX_STATUS128_RATEEPOCH((txs)->status.s8) & D11_REV128_RATE_EPOCH_MASK) >> \
		D11_REV128_RATE_EPOCH_SHIFT) : \
	((((txs)->frameid) & RATE_EPOCH_MASK) >> RATE_EPOCH_SHIFT))

/* iovar table */
enum {
	IOV_RATESEL_DUMMY, /* dummy one in order to register the module */
	IOV_RATESEL_MSGLEVEL,
	IOV_RATESEL_NSS,
	IOV_RATESEL_BWLMT,
	IOV_RATESEL_MAXSHORTAMPDUS,
	IOV_RATESEL_TEST,
	IOV_RATESEL_SP_ALGO,
	IOV_RATESEL_EMA_ALPHA,
	IOV_RATESEL_EMA_ALPHA0,
	IOV_RATESEL_EMA_FIXED,
	IOV_RATESEL_EMA_NUPD,
	IOV_RATESEL_MIN_NUPD,
	IOV_RATESEL_AGE_LMT,
	IOV_RATESEL_MEASURE_MODE,
	IOV_RATESEL_REFRATEID,
	IOV_RATESEL_RSSI,
	IOV_RATESEL_RSSI_LMT,
	IOV_RATESEL_RXANT,
	IOV_RATESEL_RXANT_PON_TT,
	IOV_RATESEL_RXANT_PON_TR,
	IOV_RATESEL_RXANT_POFF_TT,
	IOV_RATESEL_RXANT_POFF_TR,
	IOV_RATESEL_RXANT_AVGRATE,
	IOV_RATESEL_RXANT_CLEAR_AVGRATE,
	IOV_RATESEL_HOPPING,
	IOV_RATESEL_NCFAIL,
	IOV_RATESEL_RFBR,
	IOV_RATESEL_ALERT,
	IOV_RATESEL_IDLE,
	IOV_RATESEL_RSSI_RFBR,
	IOV_RATESEL_RSSI_PRI,
	IOV_RATESEL_SGI_TMIN,
	IOV_RATESEL_SGI_TMAX
};

static const bcm_iovar_t ratesel_iovars[] = {
	{"ratesel_dummy", IOV_RATESEL_DUMMY, (IOVF_SET_DOWN), 0, IOVT_BOOL, 0},
	{NULL, 0, 0, 0, 0, 0}
};

typedef struct psr_info {
	ratespec_t	rspec;
	uint32	psr;		/* packet succ rate, 1-per */
	uint	timestamp;	/* cache time */
} psr_info_t;

/** principle ratesel module local structure per device instance */
struct ratesel_info {
	wlc_info_t	*wlc;		/* pointer to main wlc structure */
	wlc_pub_t	*pub;		/* public common code handler */
	bool    ratesel_sp_algo;	/* true to indicate throughput based algo */
	bool    measure_mode;		/* default: FALSE */
	uint8	ref_rateid;		/* For measure mode: 0-29, rateid, 30- mcs_id */
	uint8	psr_ema_alpha;		/* default: 4. the EMA algo param for <psr> */
	uint8	psr_ema_alpha0;		/* default: 2. alpha value for the first x updates. */
	uint8   ema_nupd_thres;		/* default: 10. threshold afterwards use alpha */
	bool    ema_fixed;		/* default: TRUE. whether allows to change alpha0 */
	uint32	min_nupd;		/* default: 30 */
	uint32	age_lmt;		/* default: 1 */
	int16	rssi_lmt;
	bool	use_rssi;		/* use rssi to limit the lowest rate to go */
	bool    test;
	uint8   nss_lmt;
	uint8   bwlmt;
	uint8   max_short_ampdus;
	/* leave this iovar for debugging purpose */
	bool    hopping;	/* allow hopping 2 other stream family upon dropping rate */
	/* tx antenna selection */
	bool	txant_sel;		/* enable/valid for tx antenna selection */
	uint8	txant_stats[8];		/* mimo antenna selection stats */
	uint8	txant_stats_num;	/* number of used stats (mimo antconfig's) */
	uint8	txant_max_idx;		/* index to max mimo antconfig */
	uint8	txant_hist[MAXTXANTRECS];
	uint8	txant_hist_nupd;
	uint8   txant_hist_id;

	/* rx antenna selection */
	bool    rxant_sel;		/* enable rx antenna selection */
	uint8   rxant_id;		/* current default rx antenna */
	uint8   rxant_probe_id;		/* rx antenna in probing */
	uint32  rxant_stats;		/* rx'd throughput at rxant_id */
	uint32  rxant_probe_stats;	/* rx'd throughput at rxant_probe_id */
	uint32  rxant_rxcnt;		/* rxpkt counter */
	uint32  rxant_txcnt;		/* txpkt update counter */
	uint16  rxant_pon_tt;		/* txcnt threshold to start probe */
	uint16  rxant_pon_tr;		/* rxcnt threshold to start probe */
	uint16  rxant_poff_tt;		/* txcnt threshold to stop probe */
	uint16  rxant_poff_tr;		/* rxcnt threshold to stop probe */
	bool    rxant_rate_en;		/* enable history record */
	uint32  rxant_rate[ANT_SELCFG_MAX_NUM];		/* collect history rate info */
	uint32  rxant_rate_cnt[ANT_SELCFG_MAX_NUM];	/* counter for each bin */
	enable_rssi_t enable_rssi_fn;
	disable_rssi_t disable_rssi_fn;
	get_rssi_t get_rssi_fn;

	bool    rfbr;			/* use the lowest rate as the lowest fbr. For 11ac only */
	/* alert feature : support 11N only because get_rssi_fn */
	uint8   alert;			/* handle radical rssi change and/or long idle time
					 * bit 0: enable idle check, bit 1: enable rssi check
					 */
	int16  rssi_rfbr;		/* rssi drop threshold to trigger rfbr */
	int16  rssi_pri;		/* rssi drop threshold to trigger primary rate drop */
	uint16 idle_lmt;		/* maximum idle time to trigger rfbr */
	uint8	ncf_lmt;
	uint8   sgi_tmin;		/* mininum time-out */
	uint8   sgi_tmax;		/* maximum time-out */
};

typedef struct rcb_ulrt {
	uint16 state;			/* not in use yet */
	uint16 nfail;			/* # of (gdelim=0) */
	uint8  ruidx;			/* last ru idx */
	uint8  trssi;			/* trssi used in last trigger */
	uint32 tot_nupd;
	ratespec_t  txrspec[RATESEL_MFBR_NUM];	/* nss/mcs */
	uint32 nupd[RATESEL_MFBR_NUM];		/* # of updates */
	uint32 gdelim[RATESEL_MFBR_NUM];	/* # of good delimiters */
	uint32 gfcs[RATESEL_MFBR_NUM];		/* # of goodfcs */
} rcb_ulrt_t;

#define RCB_TXEPOCH_INVALID	-1

/** rcb: Ratesel Control Block is per link rate control block. */
struct rcb {
	struct scb	*scb;	/* back pointer to scb */
	ratesel_info_t	*rsi;	/* back pointer to ratesel module local structure */
	wlc_txc_info_t	*txc;	/* pointer to txc structure for efficient reference */
	uint8   *select_rates;	/* the rateset in use */
	uint8   *fbrid;		/* corresponding fallback rateid (prev_rate - 2) */
	uint8   *uprid;		/* next up rateid (prev_rate + 1) */
	uint8   *dnrid;		/* next down rateid (prev_rate - 1) */
	uint8   *bw;		/* bw for each rate in select_rates */
	uint8	active_rates_num;	/* total active rates */
	uint32	nskip;		/* number of txstatus skipped due to epoch mismatch */
	uint32	nupd;		/* number of txstatus used since last rate change */
	uint8	epoch;		/* flag to filter out stale rate_selection info */
	uint8	rateid;		/* current rate id, which is an index into array known_rspec[] */
	uint8	gotpkts;	/* this is a bit flag, other flags can go here if needed */

	uint8   fixrate;	/* fixed rate, 0xFF means auto */
	bool    upd_pend;	/* rate to be updated to take effect, used only with ratemem */

	bool    vht;		/* using vht rate */
	uint	spmode;		/* spatial probe mode common to all scb */
	uint8   mcs_baseid;	/* starting from this index the rate becomes MCS */
	uint8	bwcap;		/* 1/2/3 for 20/40/80MHz */
	bool	vht_ldpc;	/* supports ldpc for vht */
	uint8	vht_ratemask;	/* Permissions bitmap for BRCM proprietary rates */
	/* BA (A-MPDU) rate selection */
	uint8   mcs_streams;	/* mcs rate streams */
	uint32	mcs_nupd;	/* count # of ampdu txstatus used since last rate change */
	uint8	mcs_flags;	/* flags */
	uint32	sp_nupd;	/* spatial probe functionality */
	uint32  cur_sp_nupd;	/* # of spatial/ext_spatial txstatus processed at current rate */
	uint32	sp_nskip;	/* number of txstatus skipped in spatial probing */
	uint8	mcs_sp_stats[MAXSPRECS >> 3];	/* bit array to store spatial history */
	int8    mcs_sp_statc;	/* total spatial stats */
	uint8   mcs_sp_statid;	/* last position updated in mcs_sp_stats ring */
	int8    mcs_sp_col;     /* spatial probing column */
	int8    mcs_sp;         /* the mcs being probed */
	uint8   mcs_sp_id;	/* current spatial mcs id */
	uint8   last_mcs_sp_id; /* most recent one that has been actually probed */
	uint8   mcs_sp_flgs;	/* flags */
	uint8   mcs_sp_Pmod;	/* poll modulo */
	uint8   mcs_sp_Poff;	/* poll offset */
	uint8	mcs_sp_K;	/* threshold to switch to other family */
	bool    mcs_short_ampdu;
	/* antenna selection */
	uint8   active_antcfg_num;    /* number of antenna configs */
	uint8   antselid;             /* current antenna selection id */
	uint8   antselid_extsp_ipr;   /* antenna select id for intra-family probe */
	uint8   antselid_extsp_xpr;   /* antenna select id for cross-family probe */
	uint8   extsp_xpr_id;     /* current spatial mcs id for cross-probe */
	uint16  extsp_flgs;       /* flags for extended spatial probing */
	uint8   extsp_statid;
	int8    extsp_statc;      /* total extended spatial stats */
	uint8   extsp_K;          /* threshold to transition to next config */
	uint8	extsp_stats[MAX_EXTSPRECS >> 3]; /* bit array to store ext spatial history */
	uint8   extsp_ixtoggle; /* toggle between intra-probe and cross-probe */
	uint16  extsp_Pmod;	    /* poll modulo */
	uint16  extsp_Poff;	    /* poll offset */
	/* Antcfg history */
	uint16  txs_cnt;
	/* Throughput-based rate selection */
	psr_info_t	psri[PSR_ARR_SIZE]; /* PSR information used in the legacy rate algo. */
	uint8	ncfails; /* consecutive ampdu transmission failures since the rate changes. */
	uint8   pncfails; /* consecutive failure at primary rate */
	uint32  mcs_sp_thrt0;	/* tput across diff. rates in the spatial family being probed */
	uint32  mcs_sp_thrt1;	/* tput across diff. rates in the primary spatial family */
	uint32  extsp_thrt0;	/* tput across diff. rates in the spatial family being probed */
	uint32  extsp_thrt1;	/* tput across diff. rates in the other spatial family */

	/* rssi based rfbr feature */
	int     lastxs_rssi;	/* rssi noted down at last good tx */
	uint    lastxs_time;	/* last good tx time */
	int16   rssi_lastdrop;	/* rssi for the latest pri drop */

#ifdef WL_LPC
	rate_lcb_info_t lcb_info; /* LPC related info in the rate selection cubby */
#endif // endif
	uint8   sgi_bwen;	/* per-bw sgi enable */
	int8    sgi_state;	/* sgi state machine status */
	int8    sgi_cnt;	/* counter how many txstatus have updated sgi_state */
	uint32  psr_sgi;	/* PSR of the primary rate + SGI */
	uint    sgi_timeout;	/* next reset time */
	uint8   sgi_timer;	/* timer */
	bool    sgi_lastprobe_ok; /* last sgi probe result : 0 -- failure, 1 -- succ */
	uint8   max_mcs[MCS_STREAM_MAX_4]; /* per-stream max mcs */
	uint16  mcs_bitarray[MCSSET_LEN];
	bool    he;		/* using he rate */
	bool	he_ldpc;	/* supports ldpc for he */
	bool	txc_inv_reqd;	/* keep track of whether TXC invalidation is required */
	uint8	link_bw;	/* current link bandwidth */
	int8    sgi_tx;
	bool	bw_auto;	/* set to TRUE, follows internal computation logic to decide if
				 *  there are any bw2040in80 rates. Otherwise, force to off
				 */
	ratesel_dlstats_t dlstats; /* stats */
	uint16 aid;
	rcb_ulrt_t ulrt;	/* for ultx on behalf of peer */
	bool	dtpc;
};

#ifdef WLC_DTPC
#define DTPC_ACTIVE(state)	(state)->dtpc
#else
#define DTPC_ACTIVE(state)	FALSE
#endif // endif

/** for intermediate txstatus reported by mutx */
struct rcb_itxs {
	uint16 txepoch;
	uint16 tx_cnt[RATESEL_MFBR_NUM];
	uint16 txsucc_cnt[RATESEL_MFBR_NUM];
};

/* this is intended to be invoked by rate selection algorithm state machine only */
#ifdef WLAMSDU_TX
#define INVALIDATE_TXH_CACHE(state)	do {					\
		scb_ampdu_check_config((state)->rsi->wlc, (state)->scb);	\
		wlc_txc_inv((state)->txc, (state)->scb);			\
		(state)->upd_pend = TRUE;					\
		(state)->txc_inv_reqd = FALSE;					\
		wlc_amsdu_scb_agglimit_calc((state)->rsi->wlc->ami, (state)->scb); \
	} while (0)
#else /* !WLAMSDU_TX */
#define INVALIDATE_TXH_CACHE(state)	do {					\
		scb_ampdu_check_config((state)->rsi->wlc, (state)->scb);	\
		wlc_txc_inv((state)->txc, (state)->scb);			\
		(state)->upd_pend = TRUE;					\
		(state)->txc_inv_reqd = FALSE; } while (0)
#endif /* !WLAMSDU_TX */

#define IS_SP_PKT(state, cur_mcs, tx_mcs) \
	(SP_MODE(state) == SP_NORMAL && (cur_mcs) != (tx_mcs))
#define IS_EXTSP_PKT(state, cur_mcs, tx_mcs, antselid) (SP_MODE(state) == SP_EXT && \
	((cur_mcs) != (tx_mcs) || (antselid) != (state->antselid)))

uint16 ratesel_msglevel = 0;

/** table of known rspecs for rate selection (not monotonically ordered) */
static const ratespec_t known_rspec[RATES_NUM_ALL] = {
	CCK_RSPEC(WLC_RATE_1M),
	CCK_RSPEC(WLC_RATE_2M),
	CCK_RSPEC(WLC_RATE_5M5),
	OFDM_RSPEC(WLC_RATE_6M),
	OFDM_RSPEC(WLC_RATE_9M),
	CCK_RSPEC(WLC_RATE_11M),
	OFDM_RSPEC(WLC_RATE_12M),
	OFDM_RSPEC(WLC_RATE_18M),
	OFDM_RSPEC(WLC_RATE_24M),
	OFDM_RSPEC(WLC_RATE_36M),
	OFDM_RSPEC(WLC_RATE_48M),
	OFDM_RSPEC(WLC_RATE_54M),
	HT_RSPEC(32), /* MCS 32: SS 1, MOD: BPSK,  CR 1/2 DUP 40MHz only */
#ifdef WL11AC
	VHT_RSPEC(0, 1),  /* MCS  0: SS 1, MOD: BPSK,  CR 1/2 */
	VHT_RSPEC(1, 1),  /* MCS  1: SS 1, MOD: QPSK,  CR 1/2 */
	VHT_RSPEC(2, 1),  /* MCS  2: SS 1, MOD: QPSK,  CR 3/4 */
	VHT_RSPEC(3, 1),  /* MCS  3: SS 1, MOD: 16QAM, CR 1/2 */
	VHT_RSPEC(4, 1),  /* MCS  4: SS 1, MOD: 16QAM, CR 3/4 */
	VHT_RSPEC(5, 1),  /* MCS  5: SS 1, MOD: 64QAM, CR 2/3 */
	VHT_RSPEC(6, 1),  /* MCS  6: SS 1, MOD: 64QAM, CR 3/4 */
	VHT_RSPEC(7, 1),  /* MCS  7: SS 1, MOD: 64QAM, CR 5/6 */
	VHT_RSPEC(8, 1),  /* MCS  8: SS 1, MOD: 256QAM,CR 3/4 */
	VHT_RSPEC(9, 1),  /* MCS  9: SS 1, MOD: 256QAM,CR 5/6 */
	VHT_RSPEC(10, 1), /* MCS 10: SS 1, MOD: 1024QAM,CR 3/4 */
	VHT_RSPEC(11, 1), /* MCS 11: SS 1, MOD: 1024QAM,CR 5/6 */
	VHT_RSPEC(0, 2),  /* MCS  0: SS 2, MOD: BPSK,  CR 1/2 */
	VHT_RSPEC(1, 2),  /* MCS  1: SS 2, MOD: QPSK,  CR 1/2 */
	VHT_RSPEC(2, 2),  /* MCS  2: SS 2, MOD: QPSK,  CR 3/4 */
	VHT_RSPEC(3, 2),  /* MCS  3: SS 2, MOD: 16QAM, CR 1/2 */
	VHT_RSPEC(4, 2),  /* MCS  4: SS 2, MOD: 16QAM, CR 3/4 */
	VHT_RSPEC(5, 2),  /* MCS  5: SS 2, MOD: 64QAM, CR 2/3 */
	VHT_RSPEC(6, 2),  /* MCS  6: SS 2, MOD: 64QAM, CR 3/4 */
	VHT_RSPEC(7, 2),  /* MCS  7: SS 2, MOD: 64QAM, CR 5/6 */
	VHT_RSPEC(8, 2),  /* MCS  8: SS 2, MOD: 256QAM,CR 3/4 */
	VHT_RSPEC(9, 2),  /* MCS  9: SS 2, MOD: 256QAM,CR 5/6 */
	VHT_RSPEC(10, 2), /* MCS 10: SS 2, MOD: 1024QAM,CR 3/4 */
	VHT_RSPEC(11, 2), /* MCS 11: SS 2, MOD: 1024QAM,CR 5/6 */
	VHT_RSPEC(0, 3),  /* MCS  0: SS 3, MOD: BPSK,  CR 1/2 */
	VHT_RSPEC(1, 3),  /* MCS  1: SS 3, MOD: QPSK,  CR 1/2 */
	VHT_RSPEC(2, 3),  /* MCS  2: SS 3, MOD: QPSK,  CR 3/4 */
	VHT_RSPEC(3, 3),  /* MCS  3: SS 3, MOD: 16QAM, CR 1/2 */
	VHT_RSPEC(4, 3),  /* MCS  4: SS 3, MOD: 16QAM, CR 3/4 */
	VHT_RSPEC(5, 3),  /* MCS  5: SS 3, MOD: 64QAM, CR 2/3 */
	VHT_RSPEC(6, 3),  /* MCS  6: SS 3, MOD: 64QAM, CR 3/4 */
	VHT_RSPEC(7, 3),  /* MCS  7: SS 3, MOD: 64QAM, CR 5/6 */
	VHT_RSPEC(8, 3),  /* MCS  8: SS 3, MOD: 256QAM,CR 3/4 */
	VHT_RSPEC(9, 3),  /* MCS  9: SS 3, MOD: 256QAM,CR 5/6 */
	VHT_RSPEC(10, 3), /* MCS 10: SS 3, MOD: 1024QAM,CR 3/4 */
	VHT_RSPEC(11, 3), /* MCS 11: SS 3, MOD: 1024QAM,CR 5/6 */
	VHT_RSPEC(0, 4),  /* MCS  0: SS 4, MOD: BPSK,  CR 1/2 */
	VHT_RSPEC(1, 4),  /* MCS  1: SS 4, MOD: QPSK,  CR 1/2 */
	VHT_RSPEC(2, 4),  /* MCS  2: SS 4, MOD: QPSK,  CR 3/4 */
	VHT_RSPEC(3, 4),  /* MCS  3: SS 4, MOD: 16QAM, CR 1/2 */
	VHT_RSPEC(4, 4),  /* MCS  4: SS 4, MOD: 16QAM, CR 3/4 */
	VHT_RSPEC(5, 4),  /* MCS  5: SS 4, MOD: 64QAM, CR 2/3 */
	VHT_RSPEC(6, 4),  /* MCS  6: SS 4, MOD: 64QAM, CR 3/4 */
	VHT_RSPEC(7, 4),  /* MCS  7: SS 4, MOD: 64QAM, CR 5/6 */
	VHT_RSPEC(8, 4),  /* MCS  8: SS 4, MOD: 256QAM,CR 3/4 */
	VHT_RSPEC(9, 4),  /* MCS  9: SS 4, MOD: 256QAM,CR 5/6 */
	VHT_RSPEC(10, 4), /* MCS 10: SS 4, MOD: 1024QAM,CR 3/4 */
	VHT_RSPEC(11, 4), /* MCS 11: SS 4, MOD: 1024QAM,CR 5/6 */
#else /* !WL11AC */
	HT_RSPEC(0),  /* MCS  0: SS 1, MOD: BPSK,  CR 1/2 */
	HT_RSPEC(1),  /* MCS  1: SS 1, MOD: QPSK,  CR 1/2 */
	HT_RSPEC(2),  /* MCS  2: SS 1, MOD: QPSK,  CR 3/4 */
	HT_RSPEC(3),  /* MCS  3: SS 1, MOD: 16QAM, CR 1/2 */
	HT_RSPEC(4),  /* MCS  4: SS 1, MOD: 16QAM, CR 3/4 */
	HT_RSPEC(5),  /* MCS  5: SS 1, MOD: 64QAM, CR 2/3 */
	HT_RSPEC(6),  /* MCS  6: SS 1, MOD: 64QAM, CR 3/4 */
	HT_RSPEC(7),  /* MCS  7: SS 1, MOD: 64QAM, CR 5/6 */
	HT_RSPEC(8),  /* MCS  8: SS 2, MOD: BPSK,  CR 1/2 */
	HT_RSPEC(9),  /* MCS  9: SS 2, MOD: QPSK,  CR 1/2 */
	HT_RSPEC(10), /* MCS 10: SS 2, MOD: QPSK,  CR 3/4 */
	HT_RSPEC(11), /* MCS 11: SS 2, MOD: 16QAM, CR 1/2 */
	HT_RSPEC(12), /* MCS 12: SS 2, MOD: 16QAM, CR 3/4 */
	HT_RSPEC(13), /* MCS 13: SS 2, MOD: 64QAM, CR 2/3 */
	HT_RSPEC(14), /* MCS 14: SS 2, MOD: 64QAM, CR 3/4 */
	HT_RSPEC(15), /* MCS 15: SS 2, MOD: 64QAM, CR 5/6 */
	HT_RSPEC(16), /* MCS 16: SS 3, MOD: BPSK,  CR 1/2 */
	HT_RSPEC(17), /* MCS 17: SS 3, MOD: QPSK,  CR 1/2 */
	HT_RSPEC(18), /* MCS 18: SS 3, MOD: QPSK,  CR 3/4 */
	HT_RSPEC(19), /* MCS 19: SS 3, MOD: 16QAM, CR 1/2 */
	HT_RSPEC(20), /* MCS 20: SS 3, MOD: 16QAM, CR 3/4 */
	HT_RSPEC(21), /* MCS 21: SS 3, MOD: 64QAM, CR 2/3 */
	HT_RSPEC(22), /* MCS 22: SS 3, MOD: 64QAM, CR 3/4 */
	HT_RSPEC(23), /* MCS 23: SS 3, MOD: 64QAM, CR 5/6 */
#ifdef WL11N_256QAM
	HT_RSPEC(87), /* MCS 87: SS 1, MOD: 256QAM,CR 3/4 */
	HT_RSPEC(88), /* MCS 88: SS 1, MOD: 256QAM,CR 5/6 */
#endif /* WL11N_256QAM */
#endif /* WL11AC */
};

/* Define special fallback rate table for 2040in80
 * -- in pair of (primary, fallback) rspec
 */
#define FBRSPEC_NUM		12
static const ratespec_t fbr_rspec[FBRSPEC_NUM][2] = {
	/* for nss1 bw40 */
	{ VHT_RSPEC_BW(2, 1, BW_40MHZ), VHT_RSPEC_BW(1, 1, BW_20MHZ) },
	{ VHT_RSPEC_BW(1, 1, BW_40MHZ), VHT_RSPEC_BW(0, 1, BW_20MHZ) },
	/* for nss1 bw80 */
	{ VHT_RSPEC_BW(2, 1, BW_80MHZ), VHT_RSPEC_BW(2, 1, BW_40MHZ) },
	{ VHT_RSPEC_BW(1, 1, BW_80MHZ), VHT_RSPEC_BW(1, 1, BW_40MHZ) },
	/* for nss1 bw160 */
	{ VHT_RSPEC_BW(2, 1, BW_160MHZ), VHT_RSPEC_BW(2, 1, BW_80MHZ) },
	{ VHT_RSPEC_BW(1, 1, BW_160MHZ), VHT_RSPEC_BW(1, 1, BW_80MHZ) },
	/* for nss2 bw40 */
	{ VHT_RSPEC_BW(2, 2, BW_40MHZ), VHT_RSPEC_BW(2, 2, BW_20MHZ) },
	{ VHT_RSPEC_BW(1, 2, BW_40MHZ), VHT_RSPEC_BW(1, 2, BW_20MHZ) },
	/* for nss2 bw80 */
	{ VHT_RSPEC_BW(2, 2, BW_80MHZ), VHT_RSPEC_BW(3, 2, BW_40MHZ) },
	{ VHT_RSPEC_BW(1, 2, BW_80MHZ), VHT_RSPEC_BW(2, 2, BW_40MHZ) },
	/* for nss2 bw160 */
	{ VHT_RSPEC_BW(2, 2, BW_160MHZ), VHT_RSPEC_BW(2, 2, BW_80MHZ) },
	{ VHT_RSPEC_BW(1, 2, BW_160MHZ), VHT_RSPEC_BW(1, 2, BW_80MHZ) },
};

/** spatial_MCS, spatial_K, spatial_Pmod, spatial_Poff */
enum {
	SPATIAL_MCS_COL1,  /* probing mcs in (nss-1) family */
	SPATIAL_MCS_COL2,  /* probing mcs in (nss+1) family */
	SPATIAL_K_COL,     /* failure threshold - not used by sp_algo=1 */
	SPATIAL_PMOD_COL,  /* probing period tickled by (nupd+sp_nupd) */
	SPATIAL_POFF_COL,  /* probing offset */
	SPATIAL_PARAMS_NUM
};

#ifdef WL11AC
/* Use new representation of mcs as <nss,mcs> */
/* mcs 32 and mcs 0...11 x 4 */
/* the 1st column has lower probing nss
 * the 2nd column has higher probing nss
 */
static int mcs_sp_params[RATES_NUM_MCS][SPATIAL_PARAMS_NUM] = {
	{-1, -1, 2, 17, 6},    /* mcs 32 and legacy rates */
	/* nss1 */
	{0x20, -1, 2, 17, 6},    /* c0s1 */
	{0x20, -1, 2, 17, 6},    /* c1s1 */
	{0x21, -1, 2, 18, 6},    /* c2s1 */
	{0x21, -1, 2, 18, 6},    /* c3s1 */
	{0x22, -1, 2, 19, 6},    /* c4s1 */
	{0x23, -1, 2, 20, 6},    /* c5s1 */
	{0x24, -1, 2, 20, 6},    /* c6s1 */
	{0x24, -1, 2, 20, 6},    /* c7s1 */
	{0x25, -1, 2, 24, 6},    /* c8s1 */
	{0x25, -1, 2, 27, 6},    /* c9s1 */
	{0x25, -1, 2, 35, 8},   /* c10s1 */
	{0x25, -1, 2, 35, 8},   /* c11s1 */
	/* nss2 */
	{0x12, 0x30, 2, 17,  6},  /* c0s2 */
	{0x13, 0x31, 2, 18,  6},  /* c1s2 */
	{0x14, 0x31, 2, 19,  6},  /* c2s2 */
	{0x15, 0x32, 2, 20,  6},  /* c3s2 */
	{0x17, 0x33, 2, 24,  6},  /* c4s2 */
	{0x1a, 0x34, 2, 35,  8},  /* c5s2 */
	{0x1b, 0x34, 2, 35,  8},  /* c6s2 */
	{  -1, 0x35, 2, 40, 10},  /* c7s2 */
	{  -1, 0x36, 2, 45, 15},  /* c8s2 */
	{  -1, 0x37, 2, 45, 15},  /* c9s2 */
	{  -1, 0x37, 2, 50, 15}, /* c10s2 */
	{  -1, 0x38, 2, 50, 15}, /* c11s2 */
	/* nss3 */
	{0x21, 0x40, 2, 18,  8},  /* c0s3 */
	{0x22, 0x41, 2, 19,  8},  /* c1s3 */
	{0x24, 0x42, 2, 20,  8},  /* c2s3 */
	{0x24, 0x43, 2, 24,  8},  /* c3s3 */
	{0x26, 0x44, 2, 35, 10},  /* c4s3 */
	{0x27, 0x44, 2, 45, 15},  /* c5s3 */
	{0x28, 0x45, 2, 45, 15},  /* c6s3 */
	{0x2a, 0x45, 2, 45, 15},  /* c7s3 */
	{  -1, 0x46, 2, 50, 15},  /* c8s3 */
	{  -1, 0x47, 2, 50, 15},  /* c9s3 */
	{  -1, 0x49, 2, 55, 18}, /* c10s3 */
	{  -1, 0x4a, 2, 55, 18}, /* c11s3 */
	/* nss4 */
	{0x31, -1, 2, 18,  6},  /* c0s4 */
	{0x32, -1, 2, 20,  6},  /* c1s4 */
	{0x33, -1, 2, 25,  8},  /* c2s4 */
	{0x34, -1, 2, 30, 10},  /* c3s4 */
	{0x35, -1, 2, 35, 10},  /* c4s4 */
	{0x38, -1, 2, 40, 12},  /* c5s4 */
	{0x38, -1, 2, 45, 15},  /* c6s4 */
	{0x39, -1, 2, 50, 15},  /* c7s4 */
	{0x3a, -1, 2, 55, 15},  /* c8s4 */
	{0x3b, -1, 2, 60, 20},  /* c9s4 */
	{-1, -1, -1, -1, -1},  /* c10s4 */
	{-1, -1, -1, -1, -1}   /* c11s4 */
};
#else
/* mcs 32 and mcs 0...23 */
static int mcs_sp_params[RATES_NUM_MCS][SPATIAL_PARAMS_NUM] = {
	{-1, -1, 2, 17, 6}, /* mcs 32 and legacy rates */
	{ 8, -1, 2, 17, 6}, /* mcs 0 */
	{ 8, -1, 2, 17, 6},
	{ 9, -1, 2, 18, 6},
	{ 9, -1, 2, 18, 6},
	{10, -1, 2, 19, 6},
	{11, -1, 2, 20, 6},
	{12, -1, 2, 20, 6},
	{12, -1, 2, 20, 6},
	{ 2, 16, 2, 17, 6},  /* mcs 8 */
	{ 3, 17, 2, 18, 6},  /* mcs 9 */
	{ 4, 17, 2, 19, 7},
	{ 5, 18, 2, 20, 7},  /* mcs 11 */
	{ 7, 19, 2, 30, 7},
	{-1, 20, 2, 40, 7},
	{-1, 20, 2, 50, 7},
	{-1, 21, 2, 60, 7},  /* mcs 15 */
	{ 9, -1, 2, 19, 8},  /* mcs 16 */
	{10, -1, 2, 19, 8},
	{12, -1, 2, 23, 8},
	{12, -1, 2, 40, 8},  /* mcs 19 */
	{14, -1, 2, 50, 8},  /* mcs 20 */
	{15, -1, 2, 60, 8},  /* mcs 21 */
	{-1, -1, 2, 21, 8},  /* mcs 22 */
	{-1, -1, 2, 21, 8}   /* mcs 23 */
};
#endif /* WL11AC */

static int
BCMRAMFN(wlc_mcs_sp_params)(int row, int col)
{
	return mcs_sp_params[row][col];
}

/**
 * extended spatial probing: extspatial_MCS,
 * extspatial_K, extspatial_Pmod, extspatial_Poff
 */
enum {
	EXTSPATIAL_MCS_COL,
	EXTSPATIAL_K_COL,
	EXTSPATIAL_PMOD_COL,
	EXTSPATIAL_POFF_COL,
	EXTSPATIAL_PARAMS_NUM
};

#ifdef WL11AC
static int mcs_extsp_params[RATES_NUM_MCS][EXTSPATIAL_PARAMS_NUM] = {
	{-1, 2, 17, 6}, /* mcs 32 */
	{0x20, 2, 17, 6},  /* c0s1 */
	{0x20, 2, 17, 6},  /* c1s1 */
	{0x21, 2, 18, 6},  /* c2s1 */
	{0x21, 2, 18, 6},  /* c3s1 */
	{0x22, 2, 19, 6},  /* c4s1 */
	{0x23, 2, 20, 6},  /* c5s1 */
	{0x24, 2, 20, 6},  /* c6s1 */
	{0x24, 2, 20, 6},  /* c7s1 */
	{0x25, 2, 24, 6},  /* c8s1 */
	{0x25, 2, 27, 6},  /* c9s1 */
	{-1, -1, -1, -1},  /* c10s1 */
	{-1, -1, -1, -1},  /* c11s1 */
	{0x12, 2, 17, 6},  /* c0s2 */
	{0x13, 2, 18, 6},  /* c1s2 */
	{0x14, 2, 19, 7},  /* c2s2 */
	{0x15, 2, 20, 7},  /* c3s2 */
	{0x17, 2, 30, 7},  /* c4s2 */
	{0x19, 2, 40, 7},  /* c5s2 */
	{-1, 2, 100, 7},  /* c6s2 */
	{-1, 2, 200, 7},  /* c7s2 */
	{-1, 2, 200, 7},  /* c8s2 */
	{-1, 2, 200, 7},  /* c9s2 */
	{-1, -1, -1, -1},  /* c10s2 */
	{-1, -1, -1, -1},  /* c11s2 */
	/* no antenna probe for 3-stream yet */
	{-1, 2, 300, 8},  /* c0s3 */
	{-1, 2, 300, 8},  /* c1s3 */
	{-1, 2, 300, 8},  /* c2s3 */
	{-1, 2, 300, 8},  /* c3s3 */
	{-1, 2, 300, 8},  /* c4s3 */
	{-1, 2, 300, 8},  /* c5s3 */
	{-1, 2, 300, 8},  /* c6s3 */
	{-1, 2, 300, 8},  /* c7s3 */
	{-1, 2, 300, 8},  /* c8s3 */
	{-1, 2, 300, 8},   /* c9s3 */
	{-1, -1, -1, -1},  /* c10s3 */
	{-1, -1, -1, -1},  /* c11s3 */
	{-1, 2, 300, 8},  /* c0s4 */
	{-1, 2, 300, 8},  /* c1s4 */
	{-1, 2, 300, 8},  /* c2s4 */
	{-1, 2, 300, 8},  /* c3s4 */
	{-1, 2, 300, 8},  /* c4s4 */
	{-1, 2, 300, 8},  /* c5s4 */
	{-1, 2, 300, 8},  /* c6s4 */
	{-1, 2, 300, 8},  /* c7s4 */
	{-1, 2, 300, 8},  /* c8s4 */
	{-1, 2, 300, 8},   /* c9s4 */
	{-1, -1, -1, -1},  /* c10s4 */
	{-1, -1, -1, -1}  /* c11s4 */
};
#else
static int mcs_extsp_params[RATES_NUM_MCS][EXTSPATIAL_PARAMS_NUM] = {
	{-1, 2, 17, 6}, /* mcs 32 */
	{ 8, 2, 17, 6}, /* mcs 0 */
	{ 8, 2, 17, 6},
	{ 9, 2, 18, 6},
	{ 9, 2, 18, 6},
	{10, 2, 19, 6},
	{11, 2, 20, 6},
	{12, 2, 20, 6},
	{12, 2, 20, 6}, /* mcs 7 */
	{ 2, 2, 17, 6}, /* mcs 8 */
	{ 3, 2, 18, 6},
	{ 4, 2, 19, 7},
	{ 5, 2, 20, 7},
	{ 7, 2, 30, 7},
	{-1, 2, 40, 7},
	{-1, 2, 100, 7},
	{-1, 2, 200, 7}, /* mcs 15 */
	/* no antenna probe for 3-stream yet */
	{-1, 2, 300, 8}, /* mcs 16 */
	{-1, 2, 300, 8},
	{-1, 2, 300, 8},
	{-1, 2, 300, 8},
	{-1, 2, 300, 8},
	{-1, 2, 300, 8},
	{-1, 2, 300, 8},
	{-1, 2, 300, 8}  /* mcs 23 */
};
#endif /* WL11AC */

static int
BCMRAMFN(wlc_mcs_extsp_params)(int row, int col)
{
	return mcs_extsp_params[row][col];
}

enum {
	RXANT_UPD_REASON_TXCHG,
	RXANT_UPD_REASON_TXUPD,
	RXANT_UPD_REASON_RXUPD
};

#define RXANT_PON_TXCNT_MIN	40
#define RXANT_PON_RXCNT_MIN	200
#define RXANT_PON_TXCNT_MAX	(RXANT_PON_TXCNT_MIN * 16)
#define RXANT_PON_RXCNT_MAX	(RXANT_PON_RXCNT_MIN * 16)
#define RXANT_POFF_TXCNT_THRD	10
#define RXANT_POFF_RXCNT_THRD	20
#define RXANT_POFF_RXCNT_M	5

#ifdef WL11AX
#define HE_MCS_PRESENT		2
#endif /* WL11AX */
#define VHT_MCS_PRESENT		1
#define HT_MCS_PRESENT		0

#if defined(BCMDBG)
static int wlc_ratesel_dump(ratesel_info_t *rsi, struct bcmstrbuf *b);
#endif // endif

static int wlc_ratesel_doiovar(void *hdl, uint32 actionid,
	void *p, uint plen, void *a, uint alen, uint val_size, struct wlc_if *wlcif);

static void wlc_ratesel_filter(rcb_t *state, wlc_rateset_t *rateset,
	uint32 max_rate, uint32 min_rate, uint start_rate);
static void wlc_ratesel_init_fbrates(rcb_t *state);
static void wlc_ratesel_init_nextrates(rcb_t *state);
static void wlc_ratesel_load_params(rcb_t *state);
static uint8 wlc_ratesel_getfbrateid(rcb_t *state, uint8 rateid);
static void wlc_ratesel_clear_ratestat(rcb_t *state, bool change_epoch);

#if RSSI_PRI_ON
static bool wlc_ratesel_godown(rcb_t *state, uint8 down_rateid);
#else
static bool wlc_ratesel_godown(rcb_t *state);
#endif // endif
static bool wlc_ratesel_goup(rcb_t *state);
static void wlc_ratesel_pick_rate(rcb_t *state, bool is_probe, bool is_sgi);

static int wlc_ratesel_use_txs(rcb_t *state, tx_status_t *txs, uint16 SFBL, uint16 LFBL,
	uint8 tx_mcs, bool sgi, uint8 antselid, bool fbr);
static int wlc_ratesel_use_txs_blockack(rcb_t *state, tx_status_t *txs, uint8 suc_mpdu,
	uint8 tot_mpdu, bool ba_lost, uint8 retry, uint8 fb_lim, uint8 mcs,
	bool sgi, bool tx_error, uint8 antselid);
static int wlc_ratesel_use_txs_ampdu(rcb_t *state, uint16 frameid,
	uint mrt, uint mrt_succ, uint fbr, uint fbr_succ, bool tx_error, uint8 tx_mcs,
	bool sgi, uint8 antselid);
static void wlc_ratesel_init_opstats(rcb_t *state);
static int wlc_ratesel_updinfo_fromtx(rcb_t *state, ratespec_t curspec);
static int wlc_ratesel_updinfo_maggtxs(rcb_t *state, tx_status_t *txs,
	ratesel_txs_t *rs_txs, uint8 flags);
static int wlc_ratesel_updinfo_sutxs(rcb_t *state, tx_status_t *txs,
	ratespec_t curspec, uint8 flags);
static ratespec_t wlc_ratesel_updinfo_mutxs(rcb_t *state,
	tx_status_t *txs, ratesel_txs_t *rs_txs, uint8 flags);

static uint8 wlc_ratesel_filter_mcsset(rcb_t *state, wlc_rateset_t *rateset, uint8 nss_limit,
	bool en_sp, uint8 bw, bool vht_ldpc, uint8 vht_ratemask, uint16 dst[], uint8 *mcs_streams);
static int wlc_ratesel_mcsbw2id(rcb_t *state, uint8 mcs, uint8 bw);
static void wlc_ratesel_init_defantsel(rcb_t *state, uint8 txant_num, uint8 antselid_init);
static void wlc_ratesel_sanitycheck_psr(rcb_t *state);
static bool wlc_ratesel_next_rate(rcb_t *state, int8 incdec, uint8 *next_rateid,
       ratespec_t *next_rspec);

static uint8 wlc_ratesel_get_ncflmt(rcb_t *state);
static bool wlc_ratesel_upd_spstat(rcb_t *state, bool blockack, uint8 sp_mcs,
	uint ntx_succ, uint ntx, uint16 txbw);
static bool wlc_ratesel_upd_extspstat(rcb_t *state, bool blockack,
	uint8 sp_mcs, uint8 antselid, uint ntx_succ, uint ntx);

static bool wlc_ratesel_start_probe(rcb_t *state);
static bool wlc_ratesel_change_sp(rcb_t *state);
static bool wlc_ratesel_change_extsp(rcb_t *state);
static void wlc_ratesel_clear_spstat(rcb_t *state);
static bool wlc_ratesel_checksgi(rcb_t *state, bool is_sgi);

static void wlc_ratesel_alert_upd(rcb_t *state);
static void wlc_ratesel_upd_deftxant(rcb_t *state, uint8 txant_new_idx);
static void wlc_ratesel_upd_rxantprb(ratesel_info_t *rsi, int reason_code);
static void wlc_ratesel_rxant_pon(ratesel_info_t *rsi);
static void wlc_ratesel_rxant_poff(ratesel_info_t *rsi, bool force_off);

#ifdef WL_LPC
static void wlc_ratesel_upd_tpr(rcb_t *state, uint32 prate_cur,
	uint32 cur_psr, uint32 prate_dn, uint32 psr_dn);
static void wlc_ratesel_upd_la(rcb_t *state, uint32 curr_psr, uint32 old_psr);
#endif // endif

#if defined(WL_MU_TX)
static void wlc_ratesel_upd_itxs_block(rcb_t *state, rcb_itxs_t *itxs,
	ratesel_txs_t *rs_txs, tx_status_t *txs);
#endif /* defined(WL_MU_TX) */

#define IS_MCS(rspec)	(!RSPEC_ISLEGACY(rspec))

#define	print_psri(state) \
	RL_MORE(("time %d UP [%sx%02x t %d p %d] CUR [%sx%02x t %d p %d] " \
		"DN [%sx%02x t %d p %d] FB [%sx%02x t %d p %d]\n", \
		NOW(state), \
		/* UP */ \
		IS_MCS(upp->rspec) ? "m" : "", \
		WL_RSPEC_RATE_MASK & upp->rspec, upp->timestamp, upp->psr, \
		/* CUR */ \
		IS_MCS(cur->rspec) ? "m" : "", \
		WL_RSPEC_RATE_MASK & cur->rspec, cur->timestamp, cur->psr, \
		/* DN */ \
		IS_MCS(dnp->rspec) ? "m" : "", \
		WL_RSPEC_RATE_MASK & dnp->rspec, dnp->timestamp, dnp->psr, \
		/* FBR */ \
		IS_MCS(fbr->rspec) ? "m" : "", \
		WL_RSPEC_RATE_MASK & fbr->rspec, fbr->timestamp, fbr->psr))

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

static const char BCMATTACHDATA(rstr_ratesel)[] = "ratesel";
ratesel_info_t *
BCMATTACHFN(wlc_ratesel_attach)(wlc_info_t *wlc)
{
	ratesel_info_t *rsi;

	if (!(rsi = (ratesel_info_t *)MALLOCZ(wlc->osh, sizeof(ratesel_info_t)))) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}

	rsi->wlc = wlc;
	rsi->pub = wlc->pub;

	/* register module */
	if (wlc_module_register(rsi->pub, ratesel_iovars, rstr_ratesel, rsi, wlc_ratesel_doiovar,
	                        NULL, NULL, NULL)) {
		WL_ERROR(("wl%d: %s:wlc_module_register failed\n", wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

#if defined(BCMDBG)
	wlc_dump_register(rsi->pub, "ratesel", (dump_fn_t)wlc_ratesel_dump, (void *)rsi);
#endif // endif

	rsi->nss_lmt = wlc->stf->op_txstreams;
	rsi->bwlmt = DUMMY_UINT8;
	rsi->test = FALSE;
	rsi->ratesel_sp_algo = TRUE;
	rsi->min_nupd = MIN_NUPD_DEFAULT;
	rsi->age_lmt = 1;
	rsi->psr_ema_alpha = RATESEL_EMA_ALPHA_DEFAULT;
	rsi->psr_ema_alpha0 = RATESEL_EMA_ALPHA0_DEFAULT;
	rsi->ema_fixed = FALSE;
	rsi->ema_nupd_thres = RATESEL_EMA_NUPD_THRES_DEF;
	rsi->measure_mode = FALSE;
	rsi->ref_rateid = 0;

	rsi->rssi_lmt = RSSI_LMT_CCK;
	rsi->use_rssi = TRUE;
	rsi->hopping = TRUE;
	rsi->sgi_tmin = SGI_TMIN_DEFAULT;
	rsi->sgi_tmax = SGI_TMAX_DEFAULT;
	rsi->ncf_lmt = NCFAIL_LMT;

	WL_RATE(("%s: rsi %p psr_ema_alpha0 %d psr_ema_alpha %d rssi_lmt %d\n",
		__FUNCTION__, OSL_OBFUSCATE_BUF(rsi), rsi->psr_ema_alpha0,
			rsi->psr_ema_alpha, rsi->rssi_lmt));

	/* ensure some rev128 flags and masks don't overlap */
	STATIC_ASSERT((D11_REV128_RATE_EPOCH_MASK & D11_REV128_RATE_PROBE_FLAG) == 0);
	STATIC_ASSERT((D11_REV128_RATE_EPOCH_MASK & D11_REV128_RATE_NUPD_MASK) == 0);
	STATIC_ASSERT((D11_REV128_RATE_PROBE_FLAG & D11_REV128_RATE_NUPD_MASK) == 0);

	/* ensure all above flags/masks fit in byte, as rev128 caller passes byte iso uint16 */
	STATIC_ASSERT(((D11_REV128_RATE_EPOCH_MASK | D11_REV128_RATE_PROBE_FLAG |
			D11_REV128_RATE_NUPD_MASK) & 0xFF00) == 0);

	return rsi;

fail:
	MFREE(wlc->osh, rsi, sizeof(ratesel_info_t));
	return NULL;
}

void
BCMATTACHFN(wlc_ratesel_rssi_attach)(ratesel_info_t *rsi, enable_rssi_t en_fn,
	disable_rssi_t dis_fn, get_rssi_t get_fn)
{
	ASSERT(rsi);

	rsi->enable_rssi_fn = en_fn;
	rsi->disable_rssi_fn = dis_fn;
	rsi->get_rssi_fn = get_fn;
}

void
BCMATTACHFN(wlc_ratesel_detach)(ratesel_info_t *rsi)
{
	if (!rsi)
		return;

	wlc_module_unregister(rsi->pub, rstr_ratesel, rsi);

	MFREE(rsi->pub->osh, rsi, sizeof(ratesel_info_t));
}

/** handle RATESEL related iovars */
static int
wlc_ratesel_doiovar(void *hdl, uint32 actionid,
	void *p, uint plen, void *a, uint alen, uint val_size, struct wlc_if *wlcif)
{
	ratesel_info_t *rsi = (ratesel_info_t *)hdl;
	int err = 0;
	wlc_info_t *wlc;

	BCM_REFERENCE(p);
	BCM_REFERENCE(plen);
	BCM_REFERENCE(a);

	wlc = rsi->wlc;
	BCM_REFERENCE(wlc);
	BCM_REFERENCE(alen);
	BCM_REFERENCE(val_size);
	BCM_REFERENCE(wlcif);

	switch (actionid) {
		/* Note that if the IOV_GVAL() case returns an error, wl
		   will call wlc_ioctrl() and come here again. Doesn't hurt.
		*/
	default:
		err = BCME_UNSUPPORTED;
	}

	return err;
}

#ifdef BCMDBG
/** avoid adding too much for default rcb dumping */
extern void
wlc_ratesel_dump_rcb(rcb_t *rcb, int32 ac, struct bcmstrbuf *b)
{
	uint8 rateid;
	ratespec_t cur_rspec = 0;

	if (!rcb)
		return;

	bcm_bprintf(b, "\tAC[%d]: ", ac);

	rateid = rcb->rateid;
	if (rateid < rcb->active_rates_num)
		cur_rspec = RATESPEC_OF_I(rcb, rateid);
	bcm_bprintf(b, "spmode %d ", rcb->spmode);
	if (rcb->rateid < rcb->active_rates_num) {
		bcm_bprintf(b,  "%s 0x%x sgi %d epoch %d ncf %u pncf %u skips %u nupds %u\n",
		IS_MCS(cur_rspec) ? "mcs" : "rate", WL_RSPEC_RATE_MASK & cur_rspec,
		rcb->sgi_state, rcb->epoch, rcb->ncfails, rcb->pncfails, rcb->nskip, rcb->nupd);
	} else {
		/* if current rateid is not in current select_rate_set */
		bcm_bprintf(b,  "rate NA sgi %d epoch %d ncf %u pncf %u skips %u nupds %u\n",
		   rcb->sgi_state, rcb->epoch, rcb->ncfails, rcb->pncfails, rcb->nskip, rcb->nupd);
	}

	return;
}
#endif /* BCMDBG */

#if defined(BCMDBG)
static int
wlc_ratesel_dump(ratesel_info_t *rsi, struct bcmstrbuf *b)
{

	uint8 i;

	if (rsi->measure_mode)
		bcm_bprintf(b, "In the measure mode. Ref_rateid %d\n", rsi->ref_rateid);

	bcm_bprintf(b, "EMA parameters: NF = %d alpha0 = %d alpha %d\n",
		RATESEL_EMA_NF, rsi->psr_ema_alpha0, rsi->psr_ema_alpha);

	if (rsi->txant_sel) {
		bcm_bprintf(b, "txant_stats = [");
		for (i = 0; i < rsi->txant_stats_num; i++) {
			bcm_bprintf(b, " %d", rsi->txant_stats[i]);
		}
		bcm_bprintf(b, "]\n");
		bcm_bprintf(b, "txant_max_idx = %d\n", rsi->txant_max_idx);
	}

	if (rsi->rxant_sel) {
		bcm_bprintf(b, "rxant: cnt_rx/tx %d %d id_main/probe %d %d "
			    "stats_main/probe %d %d\n", rsi->rxant_rxcnt,
			    rsi->rxant_txcnt, rsi->rxant_id, rsi->rxant_probe_id,
			    rsi->rxant_stats, rsi->rxant_probe_stats);
		if (rsi->rxant_rate_en) {
			for (i = 0; i < ANT_SELCFG_MAX_NUM; i++) {
				bcm_bprintf(b, "\nantid %d: cnt %d hist_rxrate %d",
					i, rsi->rxant_rate_cnt[i], rsi->rxant_rate[i]);
			}
		}
	}
	return 0;
}

#endif // endif

#if defined(BCMDBG)
static void
wlc_ratesel_set_primary(rcb_t *state, uint8 rateid)
{
	ratespec_t cur_rspec;

	if (rateid == state->rateid)
		return;

	state->rateid = rateid;
	state->sgi_state = SGI_RESET;
	state->mcs_short_ampdu = FALSE;
	wlc_ratesel_clear_ratestat(state, TRUE);

	cur_rspec = CUR_RATESPEC(state);
	printf("%s: rateid %d %sx%02x %d 500Kbps\n", __FUNCTION__,
	      state->rateid, IS_MCS(cur_rspec) ? "m" : "",
	      (WL_RSPEC_RATE_MASK & cur_rspec), RSPEC2RATE500K(cur_rspec));
}

int
wlc_ratesel_get_fixrate(rcb_t *state, int ac, struct bcmstrbuf *b)
{
	if (state->fixrate == DUMMY_UINT8)
		bcm_bprintf(b, "AC[%d]: auto  ", ac);
	else if (state->fixrate & FIXPRI_FLAG) {
		uint8 rateid = state->fixrate & ~FIXPRI_FLAG;
		bcm_bprintf(b, "AC[%d]: 0x%x %d (500Kbps) primary", ac,
			rateid, RSPEC2RATE500K(RATESPEC_OF_I(state, rateid)));
	} else {
		bcm_bprintf(b, "AC[%d]: 0x%x %d (500Kbps)  ", ac, state->fixrate,
			RSPEC2RATE500K(RATESPEC_OF_I(state, state->fixrate)));
	}
	bcm_bprintf(b, "\n");

	return 0;
}

int
wlc_ratesel_set_fixrate(rcb_t *state, int ac, uint8 val)
{
	bool setrate = FALSE;
	uint8 rateid = val & ~FIXPRI_FLAG;

	printf("%s: ac %d val 0x%x rateid %d\n", __FUNCTION__, ac, val, rateid);

	if (val != DUMMY_UINT8 && rateid >= state->active_rates_num) {
		printf("rate id %d out of range [0, %d]\n",
			rateid, state->active_rates_num-1);
		return BCME_BADARG;
	} else {
		state->fixrate = val; /* keep the flag */
		if (val != DUMMY_UINT8) {
			state->rateid = rateid;
			if (state->rateid >= state->mcs_baseid)
				printf("ac %d %srate is fixed to "
					  "id %d mx%02x %d (500Kbps)\n",
					  ac, (val & FIXPRI_FLAG) ? "primary " : "",
					  state->rateid,
					  WL_RSPEC_RATE_MASK & CUR_RATESPEC(state),
					  RSPEC2RATE500K(CUR_RATESPEC(state)));
			else
				printf("ac %d %srate is fixed to "
					  "id %d rate %d (500Kbps)\n",
					  ac, (val & FIXPRI_FLAG) ? "primary " : "",
					  state->rateid,
					  RSPEC2RATE500K(CUR_RATESPEC(state)));
		} else {
			printf("ac %d rate is set to auto\n", ac);
		}
		setrate = TRUE;
	}

	if (setrate) {
		if (state->fixrate & FIXPRI_FLAG)
			wlc_ratesel_set_primary(state, rateid);

		BCM_REFERENCE(state);

		if (state->mcs_short_ampdu) {
			state->mcs_short_ampdu = FALSE;
		}
		INVALIDATE_TXH_CACHE(state);
	}
	return 0;
}

/** dump per-scb state, tunable parameters, upon request, (say by wl ratedump) */
int
wlc_ratesel_scbdump(rcb_t *state, struct bcmstrbuf *b)
{
	int i, id;
	uint8 ndisp;
	uint8 shift;

	bcm_bprintf(b, "time %d\n", NOW(state));
	if (state->rsi->measure_mode) {
		bcm_bprintf(b, "\tref_rateid %d rate %d ",
			state->rsi->ref_rateid, RSPEC2RATE500K(CUR_RATESPEC(state)));
		if (IS_MCS(CUR_RATESPEC(state)))
			bcm_bprintf(b, "mcs 0x%x ",
				WL_RSPEC_RATE_MASK & state->psri[PSR_CUR_IDX].rspec);
		bcm_bprintf(b, "psr %d\n", state->psri[PSR_CUR_IDX].psr);
		return 0;
	}

	/* summary */
	if (state->rateid < state->active_rates_num) {
		if (state->fixrate != DUMMY_UINT8) {
			if (state->fixrate & FIXPRI_FLAG)
				bcm_bprintf(b, "Primary fixed: ");
			else
				bcm_bprintf(b, "Fixed: ");
		}
		bcm_bprintf(b, "rid %d %s 0x%x sgi_sts %d epoch %d ncf %u pncf %u"
			"skips %u nupds %u\n",
			state->rateid, IS_MCS(CUR_RATESPEC(state)) ? "mcs" : "rate",
			WL_RSPEC_RATE_MASK & CUR_RATESPEC(state), state->sgi_state,
			state->epoch, state->ncfails, state->pncfails, state->nskip, state->nupd);
	} else {
		/* if current rateid is not in current select_rate_set */
		bcm_bprintf(b, "rate NA rid %d sgi %d epoch %d ncf %u pncf %u skips %u nupds %u\n",
		    state->rateid, state->sgi_state, state->epoch, state->ncfails, state->pncfails,
		    state->nskip, state->nupd);
	}

	/* report on vertical rate */
	bcm_bprintf(b, "\tcur [%sx%02xb%d t %d p %d]\n\tfbr [%sx%02xb%d t %d p %d]"
		    "\n\tdn  [%sx%02xb%d t %d p %d]\n\tup  [%sx%02xb%d t %d p %d]\n",
		    /* CUR */
		    IS_MCS(state->psri[PSR_CUR_IDX].rspec) ? "m" : "",
		    WL_RSPEC_RATE_MASK & state->psri[PSR_CUR_IDX].rspec,
		    RSPEC2BW(state->psri[PSR_CUR_IDX].rspec),
		    state->psri[PSR_CUR_IDX].timestamp, state->psri[PSR_CUR_IDX].psr,
		    /* FBR */
		    IS_MCS(state->psri[PSR_FBR_IDX].rspec) ? "m" : "",
		    WL_RSPEC_RATE_MASK & state->psri[PSR_FBR_IDX].rspec,
		    RSPEC2BW(state->psri[PSR_FBR_IDX].rspec),
		    state->psri[PSR_FBR_IDX].timestamp, state->psri[PSR_FBR_IDX].psr,
		    /* DN */
		    IS_MCS(state->psri[PSR_DN_IDX].rspec) ? "m" : "",
		    WL_RSPEC_RATE_MASK & state->psri[PSR_DN_IDX].rspec,
		    RSPEC2BW(state->psri[PSR_DN_IDX].rspec),
		    state->psri[PSR_DN_IDX].timestamp, state->psri[PSR_DN_IDX].psr,
		    /* UP */
		    IS_MCS(state->psri[PSR_UP_IDX].rspec) ? "m" : "",
		    WL_RSPEC_RATE_MASK & state->psri[PSR_UP_IDX].rspec,
		    RSPEC2BW(state->psri[PSR_UP_IDX].rspec),
		    state->psri[PSR_UP_IDX].timestamp, state->psri[PSR_UP_IDX].psr);

	/* report on the spatial part */
	bcm_bprintf(b, "Spatial probing mode: %d (%s)\n", state->spmode,
		(state->spmode == SP_NONE) ? "NONE" :
		(state->spmode == SP_NORMAL ? "NORMAL" : "EXT"));

	if (state->spmode == SP_NORMAL) {
		if (state->mcs_sp_id >= state->active_rates_num) {
			/* if current mcs_id is not in current select_mcs_set */
			bcm_bprintf(b, "spatial mcs NA\n");
		} else {
			bcm_bprintf(b, "\tspatial mcs x%02x bw %u sp_nupd %u\n\t",
				WL_RSPEC_RATE_MASK & RATESPEC_OF_I(state, state->mcs_sp_id),
				RSPEC2BW(RATESPEC_OF_I(state, state->mcs_sp_id)),
				state->sp_nupd);

			if (state->rsi->ratesel_sp_algo) {
				/* throughput-based spatial probing */
				bcm_bprintf(b, "sp_thrt 0x%x cur_thrt 0x%x\n",
				  state->mcs_sp_thrt0, state->mcs_sp_thrt1);
			} else {
				/* per-based */
				ndisp = (uint8) MIN(MAXSPRECS, state->sp_nupd);
				bcm_bprintf(b, "prev %d mcs_sp_stats: ", ndisp);
				id = state->mcs_sp_statid - ndisp;
				if (id < 0) id += MAXSPRECS;
				for (i = 0; i < ndisp; i++) {
					shift = (id & 0x7);
					bcm_bprintf(b, "%d ",
						((state->mcs_sp_stats[id >> 3] &
						(0x1 << shift)) >> shift));
					id = MODINCR(id, MAXSPRECS);
				}
				bcm_bprintf(b, "\n\t%d in prev %d mcs_sp_stats.\n",
					state->mcs_sp_statc, MIN(SPATIAL_M, ndisp));
			}
		}
	} else if (state->spmode == SP_EXT) {
		/* SP_EXT */
		bcm_bprintf(b, "\tProbe freq %d offset %d window %d\n",
			state->extsp_Pmod, state->extsp_Poff, state->extsp_K);
		bcm_bprintf(b, "\tcurrent %s 0x%x antsel_id %d sp_nupd %d\n\t",
			IS_MCS(CUR_RATESPEC(state)) ? "mcs" : "rate",
			WL_RSPEC_RATE_MASK & CUR_RATESPEC(state),
			state->antselid, state->sp_nupd);

		if (state->extsp_ixtoggle == I_PROBE)
			bcm_bprintf(b, "I_PROBE: antsel_id %d ",
				state->antselid_extsp_ipr);
		else {
			ratespec_t spr;
			spr = RATESPEC_OF_I(state, state->extsp_xpr_id);
			bcm_bprintf(b, "X_PROBE: antsel_id %d sp_%s 0x%x ",
				state->antselid_extsp_xpr, IS_MCS(spr) ? "mcs" : "rate",
				WL_RSPEC_RATE_MASK & spr);
		}

		if (state->rsi->ratesel_sp_algo) /* throughput-based spatial probing */
			bcm_bprintf(b, "extsp_thrt 0x%x cur_thrt 0x%x\n",
			  state->extsp_thrt0, state->extsp_thrt1);
		else {
			ndisp = (uint8) MIN(MAXSPRECS, state->sp_nupd);
			bcm_bprintf(b, "prev %d extsp_stats: ", ndisp);
			id = state->extsp_statid - ndisp;
			if (id < 0) id += MAXSPRECS;
			for (i = 0; i < ndisp; i++) {
				shift = (id & 0x7);
				bcm_bprintf(b, "%d ", ((state->extsp_stats[id >> 3] &
					(0x1 << shift)) >> shift));
				id = MODINCR(id, MAXSPRECS);
			}
			bcm_bprintf(b, "\n\t%d in prev %d extsp_stats.\n",
				state->extsp_statc, MIN(EXTSPATIAL_M, ndisp));
		}
	}
	if (state->rsi->alert) {
		bcm_bprintf(b, "alert: mode %x gotpkts %x lastxs time %u rssi %d\n",
			state->rsi->alert, state->gotpkts,
			state->lastxs_time, state->lastxs_rssi);
	}

	if (state->ulrt.state != DUMMY_UINT16) {
		int k;
		uint32 curd, curf, curp;

		bcm_bprintf(b, "ulrt: aid %d state 0x%x last RU %d nfail %d nupd %d\n",
			state->aid, state->ulrt.state, state->ulrt.ruidx,
			state->ulrt.nfail, state->ulrt.tot_nupd);
		for (k = 0; k < RATESEL_MFBR_NUM; k++) {
			curd = state->ulrt.gdelim[k];
			curf = state->ulrt.gfcs[k];
			curp = (curd == 0) ? 0 : (curf << RATESEL_EMA_NF) / curd;
			bcm_bprintf(b, "\trt%d [0x%02x %d / %d p %4d] n %d\n",
				k, WL_RSPEC_RATE_MASK & state->ulrt.txrspec[k],
				curf, curd, curp, state->ulrt.nupd[k]);
		}
	}
	return 0;
}
#endif // endif

void
wlc_ratesel_dump_rateset(rcb_t *state, struct bcmstrbuf *b)
{
	int i;
	ratespec_t rspec0;

	if (b) {
		bcm_bprintf(b, "state 0x%p active_rates_num %d",
			OSL_OBFUSCATE_BUF(state), state->active_rates_num);
		if (state->active_antcfg_num >= 1)
			bcm_bprintf(b, " bwcap %d spmode %d sgi_bwen %d ldpc %d "
				"antsel ON antcfg %d",
				state->bwcap, state->spmode, state->sgi_bwen,
				state->vht_ldpc, state->active_antcfg_num);
		else
			bcm_bprintf(b, " bwcap %d spmode %d sgi_bwen %d ldpc %d antsel OFF",
				state->bwcap, state->spmode, state->sgi_bwen, state->vht_ldpc);
		bcm_bprintf(b, "\nrate index : rate   rspec 500Kbps fbr/dn/up\n");
		/* debug output of selected rate set */
		for (i = 0; i < state->active_rates_num; i++) {
			rspec0 = RATESPEC_OF_I(state, i);
			bcm_bprintf(b, "rate_id %2d : %sx%02x 0x%02x %4d  %2d %2d %2d\n",
				i, (IS_MCS(rspec0) ? "m" : ""), WL_RSPEC_RATE_MASK & rspec0,
				rspec0, RSPEC2RATE500K(rspec0),
				state->fbrid[i], state->dnrid[i], state->uprid[i]);
		}
	} else {
		printf("state 0x%p active_rates_num %d",
			OSL_OBFUSCATE_BUF(state), state->active_rates_num);
		if (state->active_antcfg_num >= 1)
			printf(" bwcap %d spmode %d sgi_bwen %d antsel ON antcfg %d",
				state->bwcap, state->spmode, state->sgi_bwen,
				state->active_antcfg_num);
		else
			printf(" bwcap %d spmode %d sgi_bwen %d antsel OFF",
				state->bwcap, state->spmode, state->sgi_bwen);
		printf("\nrate index : rate   rspec 500Kbps fbr/dn/up\n");
		/* debug output of selected rate set */
		for (i = 0; i < state->active_rates_num; i++) {
			rspec0 = RATESPEC_OF_I(state, i);
			printf("rid %2d : %sx%02x 0x%02x %4d  %2d %2d %2d\n",
				i, (IS_MCS(rspec0) ? "m" : ""), WL_RSPEC_RATE_MASK & rspec0,
				rspec0, RSPEC2RATE500K(rspec0),
				state->fbrid[i], state->dnrid[i], state->uprid[i]);
		}
	}
}

/** return index of rspec in array known_rspec[] */
static uint8
wlc_ratesel_rspec2idx(rcb_t *state, ratespec_t rspec)
{
	uint8 i;
	uint8 limit;
	BCM_REFERENCE(state);

	limit = sizeof(known_rspec)/sizeof(ratespec_t);

	/* search mcs in known_rspec to find the row number */
	for (i = 0; i < limit; i++) {
		if (known_rspec[i] == rspec)
			break;
	}

	if (i >= limit) {
		ASSERT(0);
		return 0;
	}

	return i;
}

static uint8
wlc_ratesel_find_startidx(rcb_t *state, uint start_rate)
{
	uint8 rate_entry;
	ratespec_t rspec;
	uint32 phy_rate;

	/* Default is start_rate == 0, indicates start with highest rate */
	rate_entry = state->active_rates_num;
	if (state->active_rates_num == 0) return 0;
	if (start_rate) {
		/* get the best match */
		/* state->selectrates are sorted within each stream family */
		for (rate_entry = 0; rate_entry < state->active_rates_num; rate_entry++) {
			/* We would always endup with a single stream rate */
			rspec = RATESPEC_OF_I(state, rate_entry);
			phy_rate = wf_rspec_to_rate(rspec);
			if (start_rate <= phy_rate)
				break;
		}
	}
	if (rate_entry == state->active_rates_num)
		rate_entry --;

	return rate_entry;
}

/**
 * Get the fallback rateid of the <rateid>-th rate.
 * It is read from a look-up table constructed during the ratesel_init;
 * with an exception:
 *     If we just start (the main rate is the highest available mcs),
 *     use the lowest rate in the rate set.
 */
static uint8
wlc_ratesel_getfbrateid(rcb_t *state, uint8 rateid)
{

#if defined(D11AC_TXD)
	return state->fbrid[rateid];
#else
	return ((state->gotpkts & GOTPKTS_INIT) ? state->fbrid[rateid] : 0);
#endif /* D11AC_TXD */
}

/* return <mcs>'s offset in select_rates[] whose's bw is >= bw in input
 * Input: <mcs> = [nss, mcs], <bw> -- targetting probing (nss, mcs, bw)
 */
static int
wlc_ratesel_mcsbw2id(rcb_t *state, uint8 mcs, uint8 bw)
{
	uint8 i, k, nss0, nss1;
	uint32 rate500kbps;
	for (i = state->mcs_baseid; i < state->active_rates_num; i++) {
		if ((WL_RSPEC_RATE_MASK & known_rspec[state->select_rates[i]]) == mcs) {
			if (state->bw[i] == bw) {
				return i;
			}
		}
	}

	/* Probing rate has higher BW:
	 * go search the rate id in the same nss family
	 * whose phyrate is <= (1.2 * current primary phyrate)
	 */
	/* primary rate in 500kbps including bw excluding sgi */
	rate500kbps = (90 * RSPEC2RATE500K(CUR_RATESPEC(state))) / 100;
	nss0 = SPMASK_IN_MCS & mcs;
	for (k = state->mcs_baseid; k < state->active_rates_num; k++) {
		nss1 = (SPMASK_IN_MCS & (RATESPEC_OF_I(state, k)));
		if (nss1 > nss0) {
			break;
		} else if (nss1 < nss0) {
			continue;
		} else {
			/* nss1 == nss0 */
			uint32 tmp = (uint32)RSPEC2RATE500K(RATESPEC_OF_I(state, k));
			if (tmp >= rate500kbps) {
				return k;
			}
		}
	}
	return -1;
}

/**
 * Get mcs from rateset and filter them into a mcs bitmap
 * based on nss_limit, and en_sp (enable spatial probing)
 * Note on input :
 * rateset->mcs[] is htmcs[] bitmap
 * rateset->vht_mcsmap is coded vhtmcs bitmaps having the following format:
 *     vht_mcsmap has 16 bits, each of two bits is mcs info for corresponding nss:
 *     = 0 : mcs0-7
 *     = 1 : mcs0-8
 *     = 2 : mcs0-9
 *     = 3 : not enabled
 * rateset->he_bw[xx]_tx_mcs_nss is coded basic he mcs & nss set having the following format:
 *     he_mcs_nss has 16 bits, each of two bits is mcs info for corresponding nss:
 *     = 0 : mcs0-7
 *     = 1 : mcs0-9
 *     = 2 : mcs0-11
 *     = 3 : not enabled
 * Output:
 *     mcs bitmap in uint16[]
 *     maximum number of streams
 *     whether this is ht(0) or vht(1) or he(2) (return value)
 */
static uint8
wlc_ratesel_filter_mcsset(rcb_t *state, wlc_rateset_t *rateset, /* [in] */
	uint8 nss_limit, bool en_sp, uint8 bw, bool vht_ldpc, uint8 ratemask,
	uint16 dst[],       /* [out] */
	uint8 *mcs_streams) /* [out] */
{
	uint8 i;
	uint8 ht_vht_he = HT_MCS_PRESENT;
#ifdef WL11AX
	uint16 he_mcs_nss_set;
#endif /* WL11AX */

	bzero(dst, MCSSET_LEN * sizeof(uint16));

	*mcs_streams = 0;

#ifdef WL11AC
#ifdef WL11AX
	/* check if he mcs rates are used or not */
	if ((HE_CAP_MAX_MCS_NSS_GET_MCS(1, rateset->he_bw80_tx_mcs_nss) != HE_CAP_MAX_MCS_NONE) &&
		(bw <= BW_80MHZ)) {
		he_mcs_nss_set = rateset->he_bw80_tx_mcs_nss;
		ht_vht_he = HE_MCS_PRESENT;
	} else if ((HE_CAP_MAX_MCS_NSS_GET_MCS(1, rateset->he_bw160_tx_mcs_nss) !=
		HE_CAP_MAX_MCS_NONE) && (bw == BW_160MHZ)) {
		he_mcs_nss_set = rateset->he_bw160_tx_mcs_nss;
		ht_vht_he = HE_MCS_PRESENT;
	} else
#endif /* WL11AX */
	/* check if vht mcs rates are used or not */
	if (VHT_MCS_MAP_GET_MCS_PER_SS(1, rateset->vht_mcsmap) != VHT_CAP_MCS_MAP_NONE) {
		ht_vht_he = VHT_MCS_PRESENT;
	}
#ifdef WL11AX
	if (ht_vht_he == HE_MCS_PRESENT) {
		uint8 mcs_code, nss;

		nss = MIN(HE_CAP_MCS_MAP_NSS_MAX, nss_limit);

		/* find number of nss streams based on he_mcs_nss_set */
		for (i = 1; i <= nss; i++) {
			mcs_code = HE_CAP_MAX_MCS_NSS_GET_MCS(i, he_mcs_nss_set);

			if ((mcs_code == HE_CAP_MAX_MCS_NONE)) {
				/* bail out in order to cap at this stream - 1 */
				break;
			}
			dst[i - 1] = HE_MAX_MCS_TO_MCS_MAP(mcs_code);
			*mcs_streams = i;
		}
	} else
#endif /* WL11AX */
	if (ht_vht_he == VHT_MCS_PRESENT) {
		uint8 mcs_code, prop_mcs_code;
		uint8 nss;

		if ((bw == BW_160MHZ) && (state->scb->ext_nss_bw_sup != 0)) {
			/* any NSS_BW_HALF option set */
			for (i = 1; i <= VHT_CAP_MCS_MAP_NSS_MAX; i++) {
				mcs_code = VHT_MCS_MAP_GET_MCS_PER_SS(i, rateset->vht_mcsmap);
				if (mcs_code == VHT_CAP_MCS_MAP_NONE) {
					break;
				}
			}
			nss_limit = MIN (nss_limit, i / 2); /* cap to receiver capability */
		}
		nss = MIN(VHT_CAP_MCS_MAP_NSS_MAX, nss_limit);
		/* find number of nss streams based on rateset->vht_mcsmap */
		for (i = 1; i <= nss; i++) {
			mcs_code = VHT_MCS_MAP_GET_MCS_PER_SS(i, rateset->vht_mcsmap);
			if (mcs_code == VHT_CAP_MCS_MAP_NONE) {
				/* bail out in order to cap at this stream - 1 */
				break;
			}
			prop_mcs_code = VHT_MCS_MAP_GET_MCS_PER_SS(i, rateset->vht_mcsmap_prop);
			dst[i - 1] = wlc_get_valid_vht_mcsmap(mcs_code, prop_mcs_code, bw, vht_ldpc,
				i, ratemask);
			*mcs_streams = i;
		}
	} else
#endif /* WL11AC */
	{
		for (i = 0; i < MIN(MCSSET_LEN, nss_limit); i++) {
			dst[i] = (uint16)rateset->mcs[i];
#ifdef WL11N_256QAM
			if (i < MCS_STREAM_MAX_3) {
				uint mcs11n;
				mcs11n = mcs256q_map[i][0];
				if (rateset->mcs[mcs11n/NBBY] & (1 << (mcs11n % NBBY)))
					dst[i] |= (1 << 8);
				mcs11n = mcs256q_map[i][1];
				if (rateset->mcs[mcs11n/NBBY] & (1 << (mcs11n % NBBY)))
					dst[i] |= (1 << 9);
			}
#endif // endif
			if (dst[i])
				*mcs_streams = i+1;
		}
	}

	if (!en_sp) {
		/* For 2-stream:
		 * include MCS 8 - 11 if spatial probing is enabled,
		 * otherwise just 12 - 15
		 * For 3-stream:
		 * on top of that, only include mcs21-23
		 */
		if (*mcs_streams >= 2) {
			dst[1] = rateset->mcs[1] & ~0xF;
			if (*mcs_streams >= 3)
				dst[2] = rateset->mcs[2] & ~0x1f; /* filter m16-20 out */
		}
	}

	RL_INFO(("%s: ht(0)/vht(1)/he(2) : %d en_sp %d nss %d vht_ldpc %d bw %d rm 0x%x dst"
		"%04x %04x %04x %04x\n", __FUNCTION__, ht_vht_he, en_sp, *mcs_streams,
		vht_ldpc, bw, ratemask, dst[0], dst[1], dst[2], dst[3]));

#if !defined(WL11AC) && defined(WL11N_256QAM)
	if (*mcs_streams > 1 && (dst[0] & (3 << 8))) {
		WL_PRINT(("%s: only support 1-stream 11n_256QAM for non-11ac compiled device!\n",
			__FUNCTION__));
		ASSERT(*mcs_streams <= 1);
	}
#endif // endif
	return ht_vht_he;
}

/**
 *    check if rate from rspec is in range specified by [min, max]
 *    If in range, check rspec+sgi in range and disabel has_sgi if not in range
 * Input:
 *    rspec: has valid bw but sgi is not filled out
 *    maxr : max_rate
 *    minr : min_rate
 */
static bool
wlc_ratesel_chklmt(rcb_t *state, ratespec_t rspec, uint32 maxr, uint32 minr)
{
	uint32 cur_rate;
	cur_rate = wf_rspec_to_rate(rspec);
	BCM_REFERENCE(state);
	/* filter mcs rate out of range */
	if (cur_rate < minr || (maxr != 0 && cur_rate > maxr)) {
		return FALSE;
	}

	return TRUE;
}

/** default antenna configuration code */
static void
wlc_ratesel_init_defantsel(rcb_t *state, uint8 txant_num, uint8 antselid_init)
{
	/* initialize if we haven't already */
	ratesel_info_t *rsi = state->rsi;
	if ((txant_num <= 1) || rsi->txant_sel) {
		return;
	} else {
		int i;
		/* txant selection */
		rsi->txant_sel = TRUE;
		bzero((uchar *)rsi->txant_stats, sizeof(rsi->txant_stats));
		rsi->txant_stats_num = txant_num;
		rsi->txant_max_idx = antselid_init;

		rsi->txant_hist_id = 0;
		state->txs_cnt = 0;

		/* rxant selection */
		rsi->rxant_sel = FALSE;
		rsi->rxant_probe_id = rsi->rxant_id = antselid_init;
		rsi->rxant_pon_tt = RXANT_PON_TXCNT_MIN;
		rsi->rxant_pon_tr = RXANT_PON_RXCNT_MIN;
		rsi->rxant_poff_tt = RXANT_POFF_TXCNT_THRD;
		rsi->rxant_poff_tr = RXANT_POFF_RXCNT_THRD;

		rsi->rxant_rate_en = FALSE;
		for (i = 0; i < ANT_SELCFG_MAX_NUM; i++) {
			rsi->rxant_rate_cnt[i] = 0;
			rsi->rxant_rate[i] = 0;
		}
	}
}

/**
 * Return the fallback rate of the specified mcs rate.
 * Ensure that is a mcs rate too.
 * Input mcs has to be HT MCS!
 */
ratespec_t
wlc_ratesel_getmcsfbr(rcb_t *state, uint8 ac, uint8 plcp0)
{
	uint8 mcs, bw;
	int mcsid, fbrid;
	ratespec_t fbrspec;

	BCM_REFERENCE(ac);

	mcs = plcp0 & ~MIMO_PLCP_40MHZ;
	bw = (plcp0 & MIMO_PLCP_40MHZ) ? BW_40MHZ : BW_20MHZ;
#ifdef WL11AC
	ASSERT(state->vht == FALSE);
	ASSERT(state->he == FALSE);

	mcs = wlc_rate_ht_to_nonht(mcs);
#endif /* WL11AC */

	mcsid = wlc_ratesel_mcsbw2id(state, mcs, bw);
	if (mcsid != -1)
		fbrid = wlc_ratesel_getfbrateid(state, (uint8)mcsid);

	if (mcsid == -1 || mcsid < state->mcs_baseid)
		fbrid = state->mcs_baseid;

	fbrspec = known_rspec[state->select_rates[fbrid]];

#ifdef WL11AC
	/* translate to ht format */
	{
		int nss;
		mcs = WL_RSPEC_VHT_MCS_MASK & fbrspec;
		nss = wlc_ratespec_nss(fbrspec) - 1;
		fbrspec = HT_RSPEC(8*nss + mcs);
	}
#endif // endif
	fbrspec |= (state->bw[fbrid] << WL_RSPEC_BW_SHIFT);
	return fbrspec;
}

/**
 * Function: return the rateid of the next up/down rate but doesn't change it.
 * <incdec> decides the up/down direction.
 */
static bool BCMFASTPATH
wlc_ratesel_next_rate(rcb_t *state, int8 incdec, uint8 *next_rateid, ratespec_t *next_rspec)
{
	ratespec_t rspec = RATESPEC_OF_I(state, state->rateid);

	if (incdec == +1) {
		*next_rateid = state->uprid[state->rateid];
		if (*next_rateid == state->rateid) {
				return FALSE;
		}
	} else if (incdec == -1) {
		/* drop the rate */
		*next_rateid = state->dnrid[state->rateid];
		if (*next_rateid == state->rateid)
			return FALSE;

		/* special handling to use rssi hold rate */
		if (RSPEC_ISCCK(rspec) && state->rsi->use_rssi && MPDU_MCSENAB(state)) {
			if (state->rsi->get_rssi_fn &&
			   ((state->rsi->get_rssi_fn(state->scb)) >= state->rsi->rssi_lmt)) {
				*next_rateid = state->rateid; /* restore rateid */
				return FALSE;
			}
		}
	} else {
		/* shall not be called at all */
		return FALSE;
	}

	/* generate new rspec */
	*next_rspec = RATESPEC_OF_I(state, *next_rateid);

	return TRUE;

}

/*
 * Add a new rate described by (mcs, nss, bw)
 * Return: next rate_id in rate set, -1 if not able to add
 * Input:
 *   max_rate = 0 : don't check range
 *   mcs: 0-11 in case of WL11AC & WL11AX; otherwise is the HT mcs0-31 enumeration
 *   nss: # of streams. index-1 based.
 *   bw : bw in enum BW_xxMHZ space
 * This function is not applicable to legacy rate, for which call wlc_ratesel_addrate()
 */
static int
wlc_ratesel_addmcs(rcb_t *state, uint16 *mcs_filter,
	uint32 min_rate, uint32 max_rate, uint8 mcs, uint8 nss, uint8 bw)
{
	ratespec_t new_rspec;

#ifdef WL11AC
	if ((mcs_filter[nss-1] & (1 << mcs)) == 0) {
		/* if mcs is not allowed, then skip */
		return -1;
	}
	new_rspec = VHT_RSPEC(mcs, nss);
#else /* HT */
	if (!(isset(mcs_filter, mcs))) {
		return -1;
	}
	new_rspec = HT_RSPEC(mcs);
#endif /* !WL11AC */

	/* if need to check range, return -1 if fall out of range */
	if (max_rate > 0) {
		if (wlc_ratesel_chklmt(state, new_rspec | (bw << WL_RSPEC_BW_SHIFT),
				max_rate, min_rate) == FALSE) {
			return -1;
		}
	}

	state->bw[state->active_rates_num] = bw;
	state->select_rates[state->active_rates_num++] =
		wlc_ratesel_rspec2idx(state, new_rspec);
	state->max_mcs[nss-1] = mcs;

	return state->active_rates_num;
}

/*
 * Add nss-1 rates into rate set based on
 * -- allowed mcs at nss-1
 * -- min/max rate
 * -- bw_auto
 */
static void
wlc_ratesel_init_nss1(rcb_t *state, uint16 *mcs_filter, bool bw_auto, uint32 minrt, uint32 maxrt)
{
	uint k;
	const uint8 nss = 1;

	if (bw_auto) {
		uint8 rid = state->active_rates_num;

		/* add c0s1/c1s1 @bw20 in replace of c0s1/bw40 */
		wlc_ratesel_addmcs(state, mcs_filter, minrt, maxrt,
				   0, nss, BW_20MHZ); /* c0s1_b20 */
		wlc_ratesel_addmcs(state, mcs_filter, minrt, maxrt,
				   1, nss, BW_20MHZ); /* c1s1_b20 */

		if (state->bwcap >= BW_80MHZ) {
			/* add c1s1/c2s1 @bw40 */
			wlc_ratesel_addmcs(state, mcs_filter, minrt, maxrt,
					   1, nss, BW_40MHZ); /* c1s1_b40 */
			wlc_ratesel_addmcs(state, mcs_filter, minrt, maxrt,
					   2, nss, BW_40MHZ); /* c2s1_b40 */
		}
		if (state->bwcap >= BW_160MHZ) {
			/* add c1s1/c2s1 @bw80 */
			wlc_ratesel_addmcs(state, mcs_filter, minrt, maxrt,
					   1, nss, BW_80MHZ); /* c1s1_b80 */
			wlc_ratesel_addmcs(state, mcs_filter, minrt, maxrt,
					   2, nss, BW_80MHZ); /* c2s1_b80 */
		}
		/* add remaining mcs rates in nss-1 rate family */
		if (rid != state->active_rates_num) {
			/* we have appended rate with lower bw. remove mcs0 at primary bw. */
			mcs_filter[0] &= ~1;
		}
	}

	for (k = 0; k < 16; k++) {
		wlc_ratesel_addmcs(state, mcs_filter, minrt, maxrt,
				(uint8)k, nss, state->bwcap);
	}
	WL_RATE(("%s: max_mcs[%d] = %d\n", __FUNCTION__, nss-1, state->max_mcs[nss-1]));
}

/*
 * Add nss-2 rates into rate set based on
 * -- allowed mcs at nss-2
 * -- min/max rate
 * -- bw_auto: bw has to be > 20 to be true
 */
static void
wlc_ratesel_init_nss2(rcb_t *state, uint16 *mcs_filter, bool bw_auto, uint32 minrt, uint32 maxrt)
{
	uint k;
	const uint8 nss = 2;

	if (bw_auto) {
		uint8 rid;

		rid = state->active_rates_num;
		/* for all bw's, add c0-2 s2_b20 (13/26/39 Mbps) */
		wlc_ratesel_addmcs(state, mcs_filter, minrt, maxrt, 0, nss, BW_20MHZ);
		wlc_ratesel_addmcs(state, mcs_filter, minrt, maxrt, 1, nss, BW_20MHZ);
		wlc_ratesel_addmcs(state, mcs_filter, minrt, maxrt, 2, nss, BW_20MHZ);

		/* for bw80, add: c1-3s2_b40 (54/81/108 Mbps) */
		if (state->bwcap >= BW_80MHZ) {
			/* c1s2@40(54), c4s2@20(78), c5s2@20(104) */
			wlc_ratesel_addmcs(state, mcs_filter, minrt, maxrt, 3, nss, BW_20MHZ);
			wlc_ratesel_addmcs(state, mcs_filter, minrt, maxrt, 4, nss, BW_20MHZ);
		}

		/* for bw160, add: c1-2s2_b80 (117/175.5 Mbps) */
		if (state->bwcap >= BW_160MHZ) {
			rid = state->active_rates_num;
			wlc_ratesel_addmcs(state, mcs_filter, minrt, maxrt, 1, nss, BW_80MHZ);
			wlc_ratesel_addmcs(state, mcs_filter, minrt, maxrt, 2, nss, BW_80MHZ);
		}
		if (rid != state->active_rates_num) {
			/* remove c0s2_xx from mcs_filter */
#if defined(WL11AC) || defined(WL11AX)
			mcs_filter[nss-1] &= ~1; /* clear bit-0 */
#else
			clrbit(mcs_filter, 8);
#endif /* WL11AC || WL11AX */
		}
	}
	for (k = 0; k < 16; k++) {
		wlc_ratesel_addmcs(state, mcs_filter, minrt, maxrt, (uint8)k, nss, state->bwcap);
	}
	WL_RATE(("%s: max_mcs[%d] = %d\n", __FUNCTION__, nss-1, state->max_mcs[nss-1]));
}

/*
 * Add nss-3 rates into rate set based on
 * -- allowed mcs at nss-3
 * -- min/max rate
 * -- bw_auto: bw has to be > 20 to be true
 *    XXX: TBD
 */
static void
wlc_ratesel_init_nss3(rcb_t *state, uint16 *mcs_filter, bool bw_auto, uint32 minrt, uint32 maxrt)
{
	uint k;
	const uint8 nss = 3;
	for (k = 0; k < 16; k++) {
		wlc_ratesel_addmcs(state, mcs_filter, minrt, maxrt, (uint8)k, nss, state->bwcap);
	}
	WL_RATE(("%s: max_mcs[%d] = %d\n", __FUNCTION__, nss-1, state->max_mcs[nss-1]));
}

/*
 * Add nss-4 rates into rate set based on
 * -- allowed mcs at nss-4
 * -- min/max rate
 * -- bw_auto: bw has to be > 20 to be true
 *    XXX: TBD
 */
static void
wlc_ratesel_init_nss4(rcb_t *state, uint16 *mcs_filter, bool bw_auto, uint32 minrt, uint32 maxrt)
{
	uint k;
	const uint8 nss = 4;
	for (k = 0; k < 16; k++) {
		wlc_ratesel_addmcs(state, mcs_filter, minrt, maxrt, (uint8)k, nss, state->bwcap);
	}
	WL_RATE(("%s: max_mcs[%d] = %d\n", __FUNCTION__, nss-1, state->max_mcs[nss-1]));
}

static ratespec_t
wlc_ratesel_find_fbrspec(ratespec_t cur_rspec)
{
	uint k;
	for (k = 0; k < FBRSPEC_NUM; k++) {
		if (fbr_rspec[k][0] == cur_rspec) {
			return fbr_rspec[k][1];
		}
	}
	return 0;
}

/**
 * On e.g. association, a scb with a corresponding Rate Control Block is created. This rcb needs to
 * be initialized with the transmit rates that the rate selection algorithm is allowed to use for
 * the specified remote party.
 *
 * Initializes a caller supplied Rate Control Block with a caller supplied rateset, taking  a set of
 * constraining parameters into account. This subset is written to array state->select_rates[].
 */
static void
wlc_ratesel_filter(rcb_t *state, /* [in/out] */
	wlc_rateset_t *rateset,  /* [in] */
	uint32 max_rate,         /* all rates in [512Kbps] units, '0' if unused */
	uint32 min_rate, uint start_rate)
{
	bool found_11mbps = FALSE;
	uint i, k;

	/* ordered list of legacy rates with/without 11Mbps */
	uint8 tbl_legacy_with11Mbps[] = {2, 4, 11, 22, 36, 48, 72, 96, 108};
	uint8 tbl_legacy_no11Mbps[] = {2, 4, 11, 12, 18, 24, 36, 48, 72, 96, 108};
	uint8 *ptbl;
	uint8 tbl_size;

	uint16 filter_mcs_bitarray[MCSSET_LEN];
	uint8 txstreams;
	uint8 ht_vht_he;

	ASSERT(rateset->count > 0);

	/* check if we have 11Mbps CCK */
	for (i = 0; i < rateset->count; i++) {
		ASSERT(!(rateset->rates[i] & WLC_RATE_FLAG));
		if (rateset->rates[i] == WLC_RATE_11M) {
			found_11mbps = TRUE;
			break;
		}
	}

	/* init legacy rate adaptation for legacy and mcs rates :
	 *
	 * 1) scrub out legacy rateset entries that often aren't worth using.
	 *    Note: implemented as table lookup
	 * 2) initialize select_rates[] array with monotonic legacy rate set
	 */

	state->active_rates_num = 0;

	/* make sure they have good default values */
	state->spmode = SP_NONE;

	/* Update the nss_lmt according to the values of current txstreams,
	* which could change dynamically using oper_mode and txchain iovars
	*/
	state->rsi->nss_lmt = state->rsi->wlc->stf->op_txstreams;

	/* 1) scrub out mcs set entries that often aren't worth using */
	/* always do SISO/MIMO spatial probing if possible */
	txstreams = MIN(state->rsi->wlc->stf->op_txstreams, state->rsi->nss_lmt);
	ht_vht_he = wlc_ratesel_filter_mcsset(state, rateset, txstreams, TRUE, state->bwcap,
		state->vht_ldpc, state->vht_ratemask, filter_mcs_bitarray, &state->mcs_streams);
#ifdef WL11AX
	if (ht_vht_he == HE_MCS_PRESENT) {
		state->he = TRUE;
	} else
#endif /* WL11AX */
	if (ht_vht_he == VHT_MCS_PRESENT) {
		state->vht = TRUE;
	}

	/* capture mcs bitmap for link rate cabalities */
	for (i = 0; i < MCSSET_LEN; i++) {
		state->mcs_bitarray[i] = filter_mcs_bitarray[i];
	}

	if (state->mcs_streams > 0) {
		uint8 tbl_legacy_withMcs_20Mhz[] = {2, 4, 11};
		uint8 tbl_legacy_withMcs_40Mhz[] = {2, 4, 11, 22};
		bool bw_auto;

		if ((found_11mbps) && IS_20BW(state)) {
			tbl_size = ARRAYSIZE(tbl_legacy_withMcs_20Mhz);
			ptbl = tbl_legacy_withMcs_20Mhz;
		} else {
			tbl_size = ARRAYSIZE(tbl_legacy_withMcs_40Mhz);
			ptbl = tbl_legacy_withMcs_40Mhz;
		}

		for (k = 0; k < tbl_size; k++) {

			/* filter rate out of range */
			if ((max_rate != 0 && ptbl[k] > max_rate) || ptbl[k] < min_rate) {
				continue;
			}
			/* check if rate is available */
			for (i = 0; i < rateset->count; i++) {
				if (rateset->rates[i] == ptbl[k]) {
					state->bw[state->active_rates_num] = BW_20MHZ;
					state->select_rates[state->active_rates_num++] =
						wlc_ratesel_rspec2idx(state, LEGACY_RSPEC(ptbl[k]));
					break;
				}
			}
		}

		state->mcs_baseid = state->active_rates_num;

		if (state->mcs_streams >= 2)
			state->spmode = SP_NORMAL;

		/* 2) extend select_rates[] array with monotonic mcs rate set
		 *    Please note MCS32 is completely wiped out from rate sel's knowledge.
		 * 2.1) if no CCK: add MCS with lower bw first
		 */

		/* convert max/min rate to KBps */
		max_rate *= 500; min_rate *= 500;

		/* XXX: state->bw_auto: when set to TRUE, follows internal computation logic to
		 * decide if there are any bw2040in80 rates;
		 * when set to FALSE, then it's forced OFF.
		 */
		bw_auto = (state->bw_auto) && ((state->mcs_baseid == 0) &&
			(state->bwcap != BW_20MHZ) &&
			(WLC_HT_GET_MIMO_40TXBW(state->rsi->wlc->hti) == AUTO));
		if (bw_auto && (filter_mcs_bitarray[0] == 0x1 || filter_mcs_bitarray[1] == 0x1)) {
			/* no room to add more bw */
			bw_auto = FALSE;
		}
		wlc_ratesel_init_nss1(state, filter_mcs_bitarray, bw_auto, min_rate, max_rate);
		if (SP_MODE(state) == SP_NORMAL) {
			wlc_ratesel_init_nss2(state, filter_mcs_bitarray,
					(bw_auto && (state->mcs_streams == 2)) ? TRUE : FALSE,
					min_rate, max_rate);
			if (state->mcs_streams >= 3) {
				wlc_ratesel_init_nss3(state, filter_mcs_bitarray, bw_auto,
						min_rate, max_rate);
			}
			if (state->mcs_streams >= 4) {
				wlc_ratesel_init_nss4(state, filter_mcs_bitarray, bw_auto,
						min_rate, max_rate);
			}
		}

		if (state->active_rates_num == state->mcs_baseid) {
			state->mcs_streams = 0;
		}

		max_rate /= 500; min_rate /= 500;
	}
	if (state->mcs_streams == 0)
		/* no mcs: either a legacy link or max_rate filters out all mcs rates */
	{
		if (found_11mbps) {
			tbl_size = ARRAYSIZE(tbl_legacy_with11Mbps);
			ptbl = tbl_legacy_with11Mbps;
		} else {
			tbl_size = ARRAYSIZE(tbl_legacy_no11Mbps);
			ptbl = tbl_legacy_no11Mbps;
		}

		for (k = 0; k < tbl_size; k++) {
			/* filter rate out of range */
			if ((max_rate != 0 && ptbl[k] > max_rate) || ptbl[k] < min_rate)
				continue;

			/* check if rate is available */
			for (i = 0; i < rateset->count; i++) {
				if (rateset->rates[i] == ptbl[k]) {
					state->bw[state->active_rates_num] = BW_20MHZ;
					state->select_rates[state->active_rates_num++] =
						wlc_ratesel_rspec2idx(state, LEGACY_RSPEC(ptbl[k]));
					break;
				}
			}
		}
		state->mcs_baseid = state->active_rates_num;
	}

	/* make sure we have at least one rate available */
	ASSERT(state->active_rates_num > 0 && state->active_rates_num < MAX_RATESEL_NUM);

	wlc_ratesel_init_fbrates(state);
	wlc_ratesel_init_nextrates(state);

	/* default behaviour: start at high rate, relying on ability to collapse quickly */
	state->rateid = wlc_ratesel_find_startidx(state, start_rate);
	state->gotpkts = 0;
} /* wlc_ratesel_filter */

/** init the fallback rate look-up table */
static void
wlc_ratesel_init_fbrates(rcb_t *state)
{
	int i;
	uint8 fbr_id;
	ratespec_t cur_rspec;
	for (i = 0; i < state->active_rates_num; i++) {
		/* default fallback rate is two rate id down */
		fbr_id = LIMSUB(i, 0, 2);
		cur_rspec = RATESPEC_OF_I(state, i);
		if (IS_MCS(cur_rspec)) {
			int mcs, fbr_mcs;
			int nss, fbr_nss;
			int bw, k;
			bool bw_auto;

			mcs = wlc_ratespec_mcs(cur_rspec);
#if !defined(WL11AC) && defined(WL11N_256QAM)
			if (mcs == 87) mcs = 8;
			else if (mcs == 88) mcs = 9;
#endif // endif
			nss = wlc_ratespec_nss(cur_rspec);
			bw = state->bw[i];
			bw_auto = (state->mcs_baseid == 0) && (state->bwcap != BW_20MHZ) &&
				(WLC_HT_GET_MIMO_40TXBW(state->rsi->wlc->hti) == AUTO);

			if (i > 0 && (nss > 1 || mcs >= 1)) {
				uint32 fbr_rate, tmp_rate;
				ratespec_t fbr_ratespec = 0;

				/* General rule of fbr */
				if (mcs == 7)
					/* mcs 5/6/7 are 64-QAM. Fallback to 16-QAM at least */
					fbr_mcs = 4;
				else
					fbr_mcs = LIMSUB(mcs, 0, 2);
				fbr_nss = nss;
				/* case by case for:
				 * 0x20 -> 0x10, 0x30 -> 0x10
				 */
				if (mcs == 0) {
					fbr_mcs = mcs;
					fbr_nss = 1;
				} else if (bw_auto) {
					/* special handling due to using 20/40 in 80 */
					fbr_ratespec = wlc_ratesel_find_fbrspec(cur_rspec);
				}

				/* search the largest index whose corresponding mcs/rate
				 * is lower than fbr_mcs/rate
				 */
				if (fbr_ratespec == 0) {
					fbr_rate = wf_mcs_to_rate(fbr_mcs, fbr_nss,
							bw << WL_RSPEC_BW_SHIFT, FALSE);
				} else {
					/* fbr is from lookup table */
					fbr_rate = wf_rspec_to_rate(fbr_ratespec);
				}

				fbr_id = 0;
				for (k = i-1; k >= 0; k--) {
					tmp_rate = RATEKBPS_OF_I(state, k);
					if (tmp_rate <= fbr_rate) {
						/* try to find one with same rate but fewer nss */
						fbr_id = (uint8)k;
						if (wlc_ratespec_nss(RATESPEC_OF_I(state, k))
						    <= (uint)fbr_nss)
							break;
					}
				}
			}
		}
		state->fbrid[i] = fbr_id;

		ASSERT(state->select_rates[i] > state->fbrid[i] || i == 0);
		ASSERT(RATEKBPS_OF_I(state, i) >= RATEKBPS_OF_I(state, fbr_id));
	}
} /* wlc_ratesel_init_fbrates */

/** init the next up/dn rate look-up table */
static void
wlc_ratesel_init_nextrates(rcb_t *state)
{
	int i, next;
	ratespec_t cur_rspec;
	/* next up rate */
	for (i = 0; i < state->active_rates_num-1; i++) {
		next = i+1;
		cur_rspec = RATESPEC_OF_I(state, i);
		if (SP_MODE(state) != SP_NONE && IS_MCS(cur_rspec)) {
			if (wlc_ratespec_nss(cur_rspec) !=
			    wlc_ratespec_nss(RATESPEC_OF_I(state, next)))
				/* stay at the highest modulation with the same # streams */
				next = i;
		}
		state->uprid[i] = (uint8)next;
	}
	state->uprid[i] = (uint8)i; /* highest one */

	/* next down rate */
	state->dnrid[0] = 0; /* lowest one */
	for (i = 1; i < state->active_rates_num; i++) {
		next = i - 1;
		cur_rspec = RATESPEC_OF_I(state, i);
		if (SP_MODE(state) != SP_NONE && IS_MCS(cur_rspec)) {
			/* search downwards the highest rate that is lower than current one */
			int k;
			uint32 cur_rate = RATEKBPS_OF_I(state, i);

			next = 0;
			for (k = i-1; k >= 0; k--) {
				if ((uint32)RATEKBPS_OF_I(state, k) < cur_rate) {
					next = k;
					break;
				}
			}
		}
		state->dnrid[i] = (uint8)next;
	}
}

bool
wlc_ratesel_minrate(rcb_t *state, tx_status_t *txs)
{
	uint8 epoch;
	wlc_info_t *wlc = state->rsi->wlc;

	epoch = TXS_RATE_EPOCH(wlc->pub->corerev, txs);

	return ((epoch == state->epoch) && (state->rateid == 0)) ? TRUE : FALSE;
}

/**
 * initialize per-scb state utilized by rate selection
 *   ATTEN: this fcn can be called to "reinit", avoid dup MALLOC
 *   this new design makes this function the single entry points for any select_rates changes
 *   this function should be called when any its parameters changed: like bw or stream
 *   this function will build select_rspec[] with all constraint and rateselection will
 *      be operating on this constant array with reference to known_rspec[] for threshold
 */
void
wlc_ratesel_init(ratesel_info_t *rsi, ratesel_init_t *init)
{
	uint init_rate = 0;
	rcb_t *state;
	uint rcb_sz = (sizeof(rcb_t) + sizeof(uint8) * MAX_RATESEL_NUM * 5);

	if ((init->state == NULL) || (init->rcb_sz < rcb_sz)) {
		ASSERT(0);
		return;
	}

	state = init->state;
	bzero((char *)state, rcb_sz);

	/* store pointer to ratesel module */
	state->rsi = rsi;
	state->scb = init->scb;
	state->txc = rsi->wlc->txc;
	state->sgi_tx = init->sgi_tx;
	state->sgi_timer = rsi->sgi_tmin;

	state->select_rates = (uint8 *)state + sizeof(rcb_t);
	state->fbrid = state->select_rates + MAX_RATESEL_NUM;
	state->uprid = state->fbrid + MAX_RATESEL_NUM;
	state->dnrid = state->uprid + MAX_RATESEL_NUM;
	state->bw = state->dnrid + MAX_RATESEL_NUM;

	state->bwcap = (uint8)MIN(init->bw, rsi->bwlmt);
	state->sgi_bwen = (init->sgi_tx << BW_20MHZ);
	state->link_bw = init->bw;

	state->aid = state->scb->aid;

#ifdef WL11AC
	/* LDPC,  distinguish between the VHT and not-VHT STA */
	state->vht_ldpc = (init->ldpc_tx & VHT_LDPC) ? TRUE : FALSE;
	state->vht_ratemask = init->vht_ratemask;
#ifdef WL11AX
	/* HE LDPC Support */
	state->he_ldpc = (init->ldpc_tx & HE_LDPC) ? TRUE : FALSE;
#endif /* WL11AX */
#endif /* WL11AC */

	state->bw_auto = init->bw_auto;

	/**
	 * Rate Control Block 'state' is initialized within constraints with the provided rateset.
	 * This has to be done after initing bandwidth (bw).
	 */
	wlc_ratesel_filter(state, init->rateset, init->max_rate, init->min_rate, init_rate);

	/* in extended spatial probing, set probe antennas based on initial antselid */
	state->active_antcfg_num = init->active_antcfg_num;
	if (init->active_antcfg_num > 1) {
		state->spmode = SP_EXT;
		state->antselid = init->antselid_init;
		state->antselid_extsp_ipr =
			MODINCR(init->antselid_init, state->active_antcfg_num);
		state->antselid_extsp_xpr = init->antselid_init;
		state->extsp_ixtoggle = I_PROBE;
	}

	/* init default antenna config selection */
	wlc_ratesel_init_defantsel(state, init->active_antcfg_num, init->antselid_init);

	/* init max number of short AMPDUs when going up in rate */
	state->rsi->max_short_ampdus = MAX_SHORT_AMPDUS;

	/* init psri to zeros */
	bzero((uchar*) state->psri, PSR_ARR_SIZE * sizeof(psr_info_t));
	state->psri[PSR_CUR_IDX].psr = PSR_UNINIT;
	state->lastxs_time = NOW(state);
	state->lastxs_rssi = WLC_RSSI_INVALID;

	/* init spatial probing family */
	if (state->spmode == SP_NORMAL) {
		int nss;
		state->mcs_sp = -1;
		state->mcs_sp_col = SPATIAL_MCS_COL1;
		nss = wlc_ratespec_nss(RATESPEC_OF_I(state, state->rateid));
		/* start from probing into higher stream rate */
		if (nss > 1 && nss < state->mcs_streams)
			state->mcs_sp_col = SPATIAL_MCS_COL2;
	}

	WL_RATE(("%s: start_rateid %d, antsel init [num id] [%d %d], sgi %s\n",
		__FUNCTION__, state->rateid, init->active_antcfg_num, init->antselid_init,
		(init->sgi_tx == AUTO) ? "AUTO" : "OFF"));

	if (WL_RATE_ON())
		wlc_ratesel_dump_rateset(state, NULL);

	/* After setting up rateset and sp_mode, load the parameters for current rate */
	wlc_ratesel_load_params(state);

	state->fixrate = DUMMY_UINT8; /* auto rate */

	wlc_ratesel_init_opstats(state);
#ifdef WL_LPC
	if (LPC_ENAB(rsi->wlc))
		wlc_ratesel_lpc_init(state);
#endif // endif
	/* Init ul rate */
	state->ulrt.state = DUMMY_UINT16;
	state->ulrt.txrspec[0] = CUR_RATESPEC(state); /* init with tx rate */
	state->ulrt.trssi = ULMU_OFDMA_TRSSI_INIT;

#ifdef WLC_DTPC
	if (SCB_DTPC_CAP(state->scb) && DTPC_ENAB(rsi->wlc->pub) &&
		DTPC_SUPPORTED(rsi->wlc->dtpc)) {
		state->dtpc = TRUE;
	}
#endif // endif

	/* invalidate CACHE and trigger RATELINKMEM as last step */
	INVALIDATE_TXH_CACHE(state);
} /* wlc_ratesel_init */

#if defined(WL_MU_TX)
void
wlc_ratesel_init_rcb_itxs(rcb_itxs_t *rcb_itxs)
{
	rcb_itxs->txepoch = RCB_TXEPOCH_INVALID;
}
#endif /* defined(WL_MU_TX) */

/**
 *  Reset gotpkts to go back to init state, i.e.,
 *  uses the lowest rate for the fallback rate (reliable-fbr)
 */
void
wlc_ratesel_rfbr(rcb_t *state)
{
	if (state->gotpkts & GOTPKTS_RFBR) {
		RL_MORE(("%s gotpkts %d -> %d\n", __FUNCTION__,
			state->gotpkts, state->gotpkts & ~GOTPKTS_RFBR));
		state->gotpkts &= ~GOTPKTS_RFBR;
		INVALIDATE_TXH_CACHE(state);
	}
}

static void
wlc_ratesel_clear_ratestat(rcb_t *state, bool change_epoch)
{
	wlc_info_t *wlc = state->rsi->wlc;
	/*
	 * - bump the epoch
	 * - clear the skip count
	 * - clear the update count
	 * - set the bitfield to all 0's
	 * - set the counts to 0
	 * - lookup the new up/down decision thresholds
	 */
	if (change_epoch) {
		state->epoch = MODINCR(state->epoch, 1 + RATE_EPOCH_MAX(wlc->pub->corerev));
	}

	state->nskip = 0;
	state->nupd = 0;
	state->mcs_nupd = 0;
	state->cur_sp_nupd = 0;

	wlc_ratesel_load_params(state);

	/* remember to clear the TXC cache */
	state->txc_inv_reqd = TRUE;

	return;
}
#ifdef WLC_DTPC
uint8
wlc_ratesel_change_epoch(rcb_t *state)
{
	state->epoch = MODINCR(state->epoch,
		1u + RATE_EPOCH_MAX(state->rsi->wlc->pub->corerev));
	state->txc_inv_reqd = TRUE;
	return state->epoch;
}
#endif // endif
static void
wlc_ratesel_load_params(rcb_t *state)
{
	psr_info_t *upp = NULL, *cur = NULL, *dnp = NULL, *fbr = NULL;
	ratespec_t cur_rspec;
	int mcs, tblidx;

	cur_rspec = CUR_RATESPEC(state);
#ifdef WL11AC
	mcs = IS_MCS(cur_rspec) ?
		(WL_RSPEC_VHT_MCS_MASK & cur_rspec) : -1;
	tblidx = wlc_ratespec_nss(cur_rspec) - 1;
	tblidx = MAX_VHT_RATES * tblidx + mcs + 1;
#else
	mcs = IS_MCS(cur_rspec) ? (int)(WL_RSPEC_RATE_MASK & cur_rspec) : -1;
#if defined(WL11N_256QAM)
	if (mcs == 87) mcs = 8;
	else if (mcs == 88) mcs = 9;
#endif /* WL11N_256QAM */
	tblidx = mcs + 1;
#endif /* WL11AC */
	ASSERT(tblidx >= 0 && tblidx < RATES_NUM_MCS);

	/* get params for spatial probing */
	if (SP_MODE(state) == SP_NORMAL) {
		int mcs_sp = -1, mcs_sp_id = -1, nss_sp = -1;
		uint8 tmp_col = state->mcs_sp_col;
		bool flag_allowsw = TRUE;
		if (mcs != -1) {
			ASSERT(state->mcs_sp_col == SPATIAL_MCS_COL1 ||
			       state->mcs_sp_col == SPATIAL_MCS_COL2);
			/* switch only if the current one is not valid */
			mcs_sp = wlc_mcs_sp_params(tblidx, state->mcs_sp_col);
			if (mcs_sp == -1) {
				tmp_col = (state->mcs_sp_col == SPATIAL_MCS_COL1) ?
					SPATIAL_MCS_COL2 : SPATIAL_MCS_COL1;
				mcs_sp = wlc_mcs_sp_params(tblidx, tmp_col);
				flag_allowsw = FALSE;
			}
		}
		if (mcs_sp != -1) {
#ifdef WL11AC
			/* WL11AC uses VHT format */
			nss_sp = (mcs_sp & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT;
#else
			/* use HT format otherwise */
			nss_sp = 1 + ((mcs_sp & WLC_RSPEC_HT_NSS_MASK) >> WLC_RSPEC_HT_NSS_SHIFT);
#endif // endif
			/* 1. locate right column to start from */
			if (flag_allowsw && nss_sp > state->mcs_streams) {
				tmp_col = (state->mcs_sp_col == SPATIAL_MCS_COL1) ?
					SPATIAL_MCS_COL2 : SPATIAL_MCS_COL1;
				mcs_sp = wlc_mcs_sp_params(tblidx, tmp_col);
				flag_allowsw = FALSE;
			}
#ifdef WL11AC
			/* 2. adjust probing mcs in case c10 doesn't exist */
			if ((mcs_sp & WL_RSPEC_VHT_MCS_MASK) == 10 &&
			    state->max_mcs[nss_sp-1] == 9) {
				mcs_sp --; /* c10 do not exist, then fall back to c9 */
			}
#endif // endif
			/* 3. if probing mcs exceeds max mcs, then switch column */
			if (flag_allowsw &&
			    ((mcs_sp & WL_RSPEC_VHT_MCS_MASK) > state->max_mcs[nss_sp-1])) {
				tmp_col = (state->mcs_sp_col == SPATIAL_MCS_COL1) ?
					SPATIAL_MCS_COL2 : SPATIAL_MCS_COL1;
				mcs_sp = wlc_mcs_sp_params(tblidx, tmp_col);
			}
		} else {
			RL_SP0(("%s mcs %d mcs_sp %d mcs_sp_id %d tblidx %d mcs_sp_col %d\n",
				__FUNCTION__, mcs, mcs_sp, mcs_sp_id, tblidx, state->mcs_sp_col));
		}
		if (mcs_sp != -1) {
			mcs_sp_id = wlc_ratesel_mcsbw2id(state, (uint8) mcs_sp,
				state->bw[state->rateid]);
#ifdef WL11AC
			/* if probing into c6s3 but it doesn't exist due to spec limit
			 * then adjust probe rate to c5s3
			 */
			if (state->vht && mcs_sp_id == -1 && mcs_sp == 0x36) {
				mcs_sp --; /* c6s3 do not exist, then fall back to c5s3 */
				mcs_sp_id = wlc_ratesel_mcsbw2id(state, (uint8) mcs_sp,
					state->bw[state->rateid]);
			}
#endif // endif
		}
		state->mcs_sp_flgs = 0; /* clear all the flags */
		if (mcs_sp_id >= 0) {
			mcs_sp = RATESPEC_OF_I(state, mcs_sp_id) & WL_RSPEC_RATE_MASK;
			/* if the mcs stream family being probed has changed, clear stats */
			if ((mcs_sp & SPMASK_IN_MCS) != (state->mcs_sp & SPMASK_IN_MCS)) {
				RL_SP0(("%s: switching probing stream family: 0x%x --> 0x%x\n",
					__FUNCTION__, state->mcs_sp, mcs_sp));
				wlc_ratesel_clear_spstat(state);
			}
			state->mcs_sp_col = tmp_col;
			state->mcs_sp = (uint8)mcs_sp;
			state->mcs_sp_id = (uint8) mcs_sp_id;
			state->mcs_sp_flgs |= SPFLGS_VAL_MASK;
			state->mcs_sp_K = (uint8) wlc_mcs_sp_params(tblidx, SPATIAL_K_COL);
			state->mcs_sp_Pmod = (uint8) wlc_mcs_sp_params(tblidx, SPATIAL_PMOD_COL);
			state->mcs_sp_Poff = (uint8) wlc_mcs_sp_params(tblidx, SPATIAL_POFF_COL);
			RL_SP1(("%s: mcs 0x%x sp_col %d mcs_sp 0x%x sp_id %d\n", __FUNCTION__,
				mcs, state->mcs_sp_col, state->mcs_sp, mcs_sp_id));

		} else { /* don't probe at legacy rates or this mcs is not allowed */
			RL_SP1(("%s: mcs_sp %#x nss_sp %d mcs_sp_id %d\n",
				__FUNCTION__, mcs_sp, nss_sp, mcs_sp_id));
			state->mcs_sp_id = 0xFF; /* this is dummy entry */
			state->mcs_sp_flgs &= ~SPFLGS_VAL_MASK;
		}

	} else if (SP_MODE(state) == SP_EXT) {
		/* X-Probe */
		int mcs_extsp_xpr = -1, mcs_extsp_xpr_sel = -1;

		if (mcs != -1)
			mcs_extsp_xpr = wlc_mcs_extsp_params(tblidx, EXTSPATIAL_MCS_COL);
		if (mcs_extsp_xpr > 0)
			mcs_extsp_xpr_sel = wlc_ratesel_mcsbw2id(state,
				(uint8) mcs_extsp_xpr, state->bw[state->rateid]);
		if (mcs_extsp_xpr_sel > 0) {
			mcs_extsp_xpr =
			        RATESPEC_OF_I(state, mcs_extsp_xpr_sel) & WL_RSPEC_RATE_MASK;
		}

		state->extsp_flgs = 0; /* clear all the flags */

		/* ext spatial x-probe only if corresp rate available */
		if (mcs_extsp_xpr_sel >= 0) {
			state->extsp_xpr_id = (uint8) mcs_extsp_xpr_sel;
			state->extsp_flgs |= EXTSPFLGS_XPR_VAL_MASK;
		} else {
			state->extsp_xpr_id = 0xFF; /* this is dummy entry */
			state->extsp_flgs &= ~EXTSPFLGS_XPR_VAL_MASK;
		}
		/* I-Probe */
		state->extsp_flgs |= EXTSPFLGS_IPR_VAL_MASK;
		state->extsp_K = (uint8) wlc_mcs_extsp_params(tblidx, EXTSPATIAL_K_COL);
		state->extsp_Pmod = (uint16) wlc_mcs_extsp_params(tblidx, EXTSPATIAL_PMOD_COL);
		state->extsp_Poff = (uint16) wlc_mcs_extsp_params(tblidx, EXTSPATIAL_POFF_COL);

		RL_SP1(("%s: mcs %x tblidx %d xpr %x sel %x flgs %x K %d pmod %d poff %d\n",
			__FUNCTION__, mcs, tblidx, mcs_extsp_xpr, mcs_extsp_xpr_sel,
			state->extsp_flgs, state->extsp_K, state->extsp_Pmod, state->extsp_Poff));

	}

#ifdef WLC_DTPC
	if (DTPC_ACTIVE(state)) {
		wlc_dtpc_get_ready(state->rsi->wlc->dtpc, state->scb);
	}
#endif /* WLC_DTPC */

	/* init per info for the new rate */
	upp = (psr_info_t*)(state->psri + PSR_UP_IDX);
	cur = (psr_info_t*)(state->psri + PSR_CUR_IDX);
	dnp = (psr_info_t*)(state->psri + PSR_DN_IDX);
	fbr = (psr_info_t*)(state->psri + PSR_FBR_IDX);

	print_psri(state);

	/*
	 * It is not always true that cur becomes up or down rate,
	 * e.g. when doing the spatial probing: mcs 6/7 <--> mcs 12.
	 * The validity of up/dn will be checked when using it.
	 */
	if (cur->psr != PSR_UNINIT && RSPEC2RATE500K(cur_rspec) > RSPEC2RATE500K(cur->rspec)) {
		/* rate up : dn->fbr, cur->dn, up->cur and reset up */
		if (dnp->psr != PSR_UNINIT && (dnp->timestamp - NOW(state)) < AGE_LMT(state))
			bcopy((char*)dnp, (char*)fbr, sizeof(psr_info_t));
		else {
			fbr->psr = PSR_UNINIT;
		}
		bcopy((char*)cur, (char*)dnp, sizeof(psr_info_t));
		dnp->timestamp = NOW(state);
		if (upp->psr != PSR_UNINIT && (upp->timestamp - NOW(state)) < AGE_LMT(state) &&
		    upp->rspec == cur_rspec)
			bcopy((char*)upp, (char*)cur, sizeof(psr_info_t));
		else {
			cur->psr = PSR_MAX;
		}
		upp->psr = PSR_UNINIT;
	} else if (cur->psr != PSR_UNINIT &&
		RSPEC2RATE500K(cur_rspec) < RSPEC2RATE500K(cur->rspec)) {
		/* rate down : cur->up, dn->cur, fbr->dn and reset fbr */
		bcopy((char*)cur, (char*)upp, sizeof(psr_info_t));
		upp->timestamp = NOW(state);
		if (dnp->psr != PSR_UNINIT && (dnp->timestamp - NOW(state)) < AGE_LMT(state) &&
		    dnp->rspec == cur_rspec)
			bcopy((char*)dnp, (char*)cur, sizeof(psr_info_t));
		else {
			cur->psr = PSR_MAX;
		}
		bcopy((char*)fbr, (char*)dnp, sizeof(psr_info_t));
		fbr->psr = PSR_UNINIT;
	} else {
		/* First time come here OR the new rateid is the same as the current one
		 * Init <cur>, reset <up>, <dn> and <fbr>
		 */
		cur->psr = PSR_MAX;
		fbr->psr = PSR_UNINIT;
		dnp->psr = PSR_UNINIT;
		upp->psr = PSR_UNINIT;
	}

	cur->rspec = cur_rspec;

	/* reset failed tx counter */
	state->ncfails = 0;
	state->pncfails = 0;

	print_psri(state);

	return;
} /* wlc_ratesel_load_params */

void
wlc_ratesel_aci_change(ratesel_info_t *rsi, bool aci_state)
{
	if (rsi->ema_fixed)
		return;

	if (aci_state) {
		RL_INFO(("%s: aci ON. alpha0: %d --> %d, alpha: %d --> %d, thres: %d --> %d\n",
			__FUNCTION__, rsi->psr_ema_alpha0, RATESEL_EMA_ALPHA0_DEFAULT + 2,
			rsi->psr_ema_alpha, RATESEL_EMA_ALPHA_DEFAULT + 1,
			rsi->ema_nupd_thres, RATESEL_EMA_NUPD_THRES_DEF + 5));
	} else {
		RL_INFO(("%s: aci OFF. alpha0: %d --> %d, alpha: %d --> %d, thres: %d --> %d\n",
			__FUNCTION__, rsi->psr_ema_alpha0, RATESEL_EMA_ALPHA0_DEFAULT,
			rsi->psr_ema_alpha, RATESEL_EMA_ALPHA_DEFAULT, rsi->ema_nupd_thres,
			RATESEL_EMA_NUPD_THRES_DEF));
	}

	if (aci_state) {
		rsi->psr_ema_alpha0 = RATESEL_EMA_ALPHA0_DEFAULT + 2;
		rsi->psr_ema_alpha = RATESEL_EMA_ALPHA_DEFAULT + 1;
		rsi->ema_nupd_thres = RATESEL_EMA_NUPD_THRES_DEF + 5;
	} else {
		rsi->psr_ema_alpha0 = RATESEL_EMA_ALPHA0_DEFAULT;
		rsi->psr_ema_alpha = RATESEL_EMA_ALPHA_DEFAULT;
		rsi->ema_nupd_thres = RATESEL_EMA_NUPD_THRES_DEF;
	}
}

/**
 * The sanity check on cur_rspec and fbr_rspec to consider
 * fallback rate frames after switching the rate without
 * pumping up the epoch (as the result of spatial probing).
 */
static void BCMFASTPATH
wlc_ratesel_sanitycheck_psr(rcb_t *state)
{
	ratespec_t fbrspec;
	uint8 fbrid = wlc_ratesel_getfbrateid(state, state->rateid);
	fbrspec = RATESPEC_OF_I(state, fbrid);

	if (state->psri[PSR_CUR_IDX].rspec != CUR_RATESPEC(state)) {
		RL_MORE(("%s: mismatch cur_rspec 0x%x record 0x%x rateid %d sgi %d\n",
			__FUNCTION__, CUR_RATESPEC(state),
			state->psri[PSR_CUR_IDX].rspec, state->rateid, state->sgi_state));
		state->psri[PSR_CUR_IDX].rspec = CUR_RATESPEC(state);
		state->psri[PSR_CUR_IDX].psr = PSR_MAX;
	}

	if (state->psri[PSR_FBR_IDX].rspec != fbrspec ||
	    state->psri[PSR_FBR_IDX].psr == PSR_UNINIT) {
		RL_MORE(("%s: mismatch/uninit fbrspec 0x%x record: fbrspec 0x%x psr %d\n",
			__FUNCTION__, fbrspec, state->psri[PSR_FBR_IDX].rspec,
			state->psri[PSR_FBR_IDX].psr));
		state->psri[PSR_FBR_IDX].rspec = fbrspec;
		state->psri[PSR_FBR_IDX].psr = PSR_MAX;
	}
}

static void
wlc_ratesel_clear_spstat(rcb_t *state)
{
	/* clear AMPDU spatial stats */
	state->sp_nskip = 0;
	state->sp_nupd = 0;
	bzero((uchar *)state->mcs_sp_stats, sizeof(state->mcs_sp_stats));
	state->mcs_sp_statc = 0;
	state->mcs_sp_statid = 0;

	/* throughput-based algorithm */
	state->mcs_sp_thrt0 = 0;
	state->mcs_sp_thrt1 = 0;

	RL_SP1(("%s\n", __FUNCTION__));
}

static void
wlc_ratesel_clear_extspstat(rcb_t *state)
{
	/* clear AMPDU spatial stats for the currently active probe (i or x) */
	state->sp_nskip = 0;
	state->sp_nupd = 0;
	bzero((uchar *)state->extsp_stats, sizeof(state->extsp_stats));
	state->extsp_statc = 0;
	state->extsp_statid = 0;

	/* throughput-based algorithm */
	state->extsp_thrt0 = 0;
	state->extsp_thrt1 = 0;
}

/**
 * Update the spatial probing statistics
 * Return TRUE if the returned txstatus is a spatial probing frame.
 * Input:
 *   <blockack> : indicates whether it's an update on blockack
 * Note that we may have unmatched tx_mcs coming because of
 * 1. it is a spatial probing frame
 * 2. epoch has wrapped around and we are getting wrong sp_mcs,
 * and/or sp is not valid for current mcs
 * Try to discard false feedback from case 2.
 */
static bool
wlc_ratesel_upd_spstat(rcb_t *state, bool blockack, uint8 sp_mcs,
	uint ntx_succ, uint ntx, uint16 txbw)
{
	BCM_REFERENCE(blockack);

	if (state->mcs_sp_id == 0xFF || sp_mcs == MCS_INVALID || ntx == 0)
		return FALSE;
	/* if # of streams being probed doesn't match current mcs_sp, discard */
	if ((sp_mcs & SPMASK_IN_MCS) != (state->mcs_sp & SPMASK_IN_MCS))
		return FALSE;

	if (state->rsi->ratesel_sp_algo) { /* throughput-based sp algo */

		uint32 cu_rate, sp_rate;
		uint32 thrt0, thrt1, *pthrt, beta;
		ratespec_t sp_rspec;

		/* update throughput estimate. Don't count sgi in computing cu_rate */
#ifdef WL11AC
		sp_rspec = VHT_RSPEC(sp_mcs & WL_RSPEC_VHT_MCS_MASK,
			(sp_mcs & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT);
#else
		sp_rspec = HT_RSPEC(sp_mcs);
#endif // endif
		if (txbw == 0) {
			sp_rspec |= (state->bwcap << WL_RSPEC_BW_SHIFT);
		} else {
			sp_rspec |= (txbw << WL_RSPEC_BW_SHIFT);
		}

		sp_rate = RSPEC2RATE500K(sp_rspec);
		cu_rate = RSPEC2RATE500K(RATESPEC_OF_I(state, state->rateid));

		thrt0 = (ntx_succ << RATESEL_EMA_NF) * sp_rate / ntx;
		thrt1 = state->psri[PSR_CUR_IDX].psr * cu_rate;

		/* In case of different rate, scale down the higher one
		 * since big PER difference can lead to poor aggregation density.
		 * But note if the (lower rate / higher rate) ratio is smaller
		 * than the scaling ratio (29/32), then
		 * use a smaller scaling factor (31/32).
		 */

		pthrt = NULL;
		beta = 29; /* 29/32 ~= 0.90625 */
		if (cu_rate > sp_rate) {
			pthrt = &thrt1;
			if ((cu_rate * 29) < (sp_rate << 5)) {
				beta = 31; /* 31/32 = 0.96875 */
			}
		} else if (cu_rate < sp_rate) {
			pthrt = &thrt0;
			if ((sp_rate * 29) < (cu_rate << 5)) {
				beta = 31;
			}
		}
		if (pthrt) {
			*pthrt = ((*pthrt) * beta) >> 5;
		}

		if (state->sp_nupd == 0) {
			state->mcs_sp_thrt0 = thrt0;
			state->mcs_sp_thrt1 = thrt1;
		} else if (state->sp_nupd < SPATIAL_M) {
			/* use accumulative average */
			state->mcs_sp_thrt0 = (state->mcs_sp_thrt0 * (state->sp_nupd - 1)
					       + thrt0) / state->sp_nupd;
			state->mcs_sp_thrt1 = (state->mcs_sp_thrt1 * (state->sp_nupd - 1)
					       + thrt1)	/ state->sp_nupd;
		} else {
			/* use moving average */
			UPDATE_MOVING_AVG(&state->mcs_sp_thrt0, thrt0, state->rsi->psr_ema_alpha);
			UPDATE_MOVING_AVG(&state->mcs_sp_thrt1, thrt1, state->rsi->psr_ema_alpha);
		}
		RL_SP0(("sp_nupd %u : (mx%02x %d) -> (mx%02x %d) thrt_pri/sp 0x%x 0x%x "
			"sp_thrt_pri/sp 0x%x 0x%x\n",
			state->sp_nupd, WL_RSPEC_RATE_MASK & CUR_RATESPEC(state), cu_rate,
			WL_RSPEC_RATE_MASK & sp_rspec, sp_rate, thrt1, thrt0,
			state->mcs_sp_thrt1, state->mcs_sp_thrt0));
	} else { /* absolute PER base spatial probing algo */
		uint8 idx, shift, mask, val;
		uint16 stat = (ntx == ntx_succ) ? 1 : 0;
		int id;

		idx = state->mcs_sp_statid;
		shift = (idx & 0x7);
		mask = (0x1 << shift);
		val = ((uint8) (stat & 0x1)) << shift;
		state->mcs_sp_stats[idx >> 3] =
			(state->mcs_sp_stats[idx >> 3] & ~mask) | (val & mask);
		state->mcs_sp_statid = MODINC(state->mcs_sp_statid, MAXSPRECS);

		/* update spatial window totals */
		state->mcs_sp_statc += stat;

		id = state->mcs_sp_statid - SPATIAL_M - 1;
		if (id < 0)
			id += MAXSPRECS;
		shift = (id & 0x7);
		mask = (0x1 << shift);
		val = (state->mcs_sp_stats[id >> 3] & mask) >> shift;
		state->mcs_sp_statc -= val;
	}

	/* update the count of updates since the last state flush, which
	 * saturates at max word val
	 */
	LIMINC_UINT32(state->sp_nupd);
	LIMINC_UINT32(state->cur_sp_nupd);
	state->last_mcs_sp_id = state->mcs_sp_id;
	return TRUE;
} /* wlc_ratesel_upd_spstat */

/**
 * Update the extended spatial probing statistics
 * Return TRUE if the returned txstatus is a spatial probing frame.
 * Input:
 *   <blockack> : indicates whether update on blockack
 *   <sp_mcs> : mcs if the pkt was tx'd at MCS rate, otherwise mark it as invalid (MCS_INVALID)
 */
static bool
wlc_ratesel_upd_extspstat(rcb_t *state, bool blockack, uint8 sp_mcs, uint8 antselid,
	uint ntx_succ, uint ntx)
{
	bool valid_probe = FALSE;
	uint8 cur_mcs = MCS_INVALID; /* sp_mcs will be the same if not MCS */
	ratespec_t cur_rspec = CUR_RATESPEC(state);
	uint8 extsp_rateid;

	if (ntx == 0)
		return FALSE;

	if (blockack)
		ASSERT(sp_mcs != MCS_INVALID && IS_MCS(cur_rspec));

	if (IS_MCS(cur_rspec))
		cur_mcs = WL_RSPEC_RATE_MASK & cur_rspec;
	else
		ASSERT(state->extsp_ixtoggle == I_PROBE);

	/* is probe ACK of the type it needs to be? */
	if (state->extsp_ixtoggle == I_PROBE)
		valid_probe = (cur_mcs == sp_mcs && antselid == state->antselid_extsp_ipr);
	else if (state->extsp_ixtoggle == X_PROBE) {
		/* the later case in assertion should've been filter out by epoch */
		if (cur_mcs == MCS_INVALID || sp_mcs == MCS_INVALID) {
			WL_ERROR(("%s: cur_mcs %d sp_mcs %d antselid %d %d ntx_succ %d ntx %d e %d",
				__FUNCTION__, cur_mcs, sp_mcs, state->antselid, antselid,
				ntx_succ, ntx, state->epoch));
		}
		ASSERT(cur_mcs != MCS_INVALID && sp_mcs != MCS_INVALID);
		/* make sure neither mcs/# of streams matches and but antselid do */
		valid_probe = (cur_mcs != sp_mcs &&
			(cur_mcs & SPMASK_IN_MCS) != (sp_mcs & SPMASK_IN_MCS) &&
			antselid == state->antselid_extsp_xpr);
	}

	if (!valid_probe)
		return FALSE;

	extsp_rateid = (state->extsp_ixtoggle == I_PROBE) ?
		state->rateid : (state->extsp_xpr_id);

	if (state->rsi->ratesel_sp_algo) { /* throughput-based sp algo */

		uint32 cu_rate, sp_rate;
		uint32 thrt0, thrt1;

		/* update throughput estimate. Don't count sgi in computing cu_rate */
		sp_rate = RSPEC2RATE500K(RATESPEC_OF_I(state, extsp_rateid));
		cu_rate = RSPEC2RATE500K(RATESPEC_OF_I(state, state->rateid));

		thrt0 = (ntx_succ << RATESEL_EMA_NF) * sp_rate / ntx;
		thrt1 = state->psri[PSR_CUR_IDX].psr * cu_rate;

		/* In case of inequal rate scale down the higher one
		 * since big PER difference can lead to poor aggregation density.
		 */
		if (cu_rate > sp_rate)
			thrt1 = (thrt1 * 29) >> 5; /* 29/32 ~= 0.90625 */
		else if (cu_rate < sp_rate)
			thrt0 = (thrt0 * 29) >> 5;

		if (state->sp_nupd == 0) {
			state->extsp_thrt0 = thrt0;
			state->extsp_thrt1 = thrt1;
		} else if (state->sp_nupd < EXTSPATIAL_M) {
			/* use accumulative average */
			state->extsp_thrt0 = (state->extsp_thrt0 * (state->sp_nupd - 1)
					      + thrt0)/state->sp_nupd;
			state->extsp_thrt1 = (state->extsp_thrt1 * (state->sp_nupd - 1)
					      + thrt1)/state->sp_nupd;
		} else {
			/* use moving average -- actually due to early abortion, won't come here */
			UPDATE_MOVING_AVG(&state->extsp_thrt0, thrt0, state->rsi->psr_ema_alpha);
			UPDATE_MOVING_AVG(&state->extsp_thrt1, thrt1, state->rsi->psr_ema_alpha);
		}
		RL_SP0(("sp_nupd %u : (%s%u %d %d) -> (%s%u %d %d) thrt_pri/sp 0x%x 0x%x "
			"extsp_thrt_pri/sp 0x%x 0x%x\n", state->sp_nupd,
			(cur_mcs != MCS_INVALID) ? "m" : "", cur_mcs, cu_rate, state->antselid,
			(sp_mcs != MCS_INVALID) ? "m" : "", sp_mcs, sp_rate, antselid,
			thrt1, thrt0, state->extsp_thrt1, state->extsp_thrt0));
	} else {
		/* update extended spatial history */
		uint16 stat;
		uint8 idx, shift, mask, val;
		int id;

		stat = (ntx == ntx_succ) ? 1 : 0; /* stat=1 when o.k. */

		idx = state->extsp_statid;
		shift = (idx & 0x7);
		mask = (0x1 << shift);
		val = ((uint8) (stat & 0x1)) << shift;
		state->extsp_stats[idx >> 3] =
			(state->extsp_stats[idx >> 3] & ~mask) | (val & mask);
		state->extsp_statid = MODINC(state->extsp_statid, MAX_EXTSPRECS);
		state->extsp_statc += stat;

		id = state->extsp_statid - EXTSPATIAL_M - 1;
		if (id < 0)
			id += MAX_EXTSPRECS;
		shift = (id & 0x7);
		mask = (0x1 << shift);
		val = (state->extsp_stats[id >> 3] & mask) >> shift;
		state->extsp_statc -= val;

		/* Comment: Here, could consider immediately reenabling
		 * the probe for the case that the probe was successul
		 * ie for stat == 1; this might be even more useful if
		 * extended spatial probing targets higher than current
		 * rates.
		 */
	}

	/* update the count of updates since the last state flush, which
	 * saturates at max byte val
	 */
	LIMINC_UINT32(state->sp_nupd);
	LIMINC_UINT32(state->cur_sp_nupd);
	return TRUE;
} /* wlc_ratesel_upd_extspstat */

static uint8 BCMFASTPATH
wlc_ratesel_get_ncflmt(rcb_t *state)
{
	ratesel_info_t *rsi;
	uint8 ncf_lmt;

	rsi = state->rsi;
	if (state->gotpkts == GOTPKTS_ALL)
		ncf_lmt = rsi->ncf_lmt;
	else if (state->gotpkts & GOTPKTS_INIT)
		ncf_lmt = rsi->ncf_lmt >> 1;
	else
		ncf_lmt = NCFAIL_LMT0;
	return ncf_lmt;
}

/*
 * (1) any state => SGI_PROBING, don't do anything
 * (2) state *change* => SGI_RESET/SGI_DISABLED, change cur_rspec
 * (3) state *change* => SGI_ENABLED, change cur_rspec
 * As the result of this function call,
 * if return FALSE, the caller shall resume as if this func doesn't exist
 * if TRUE, indicates some sgi state change and the caller shan't do anything else further.
 */
static bool
wlc_ratesel_checksgi(rcb_t *state, bool is_sgi)
{
	bool hold_rate, clr_cache;
	int sgi_prev;
	uint8 ncf_lmt;
	wlc_info_t *wlc = state->rsi->wlc;

	if (state->sgi_bwen == 0 || state->rateid < state->mcs_baseid)
		return FALSE;

	sgi_prev = state->sgi_state;
	hold_rate = FALSE;
	clr_cache = FALSE;
	ncf_lmt = wlc_ratesel_get_ncflmt(state);
	if ((sgi_prev == SGI_DISABLED || sgi_prev == SGI_ENABLED)) {
		/* sgi_state will become reset or remains unchanged */

		if (NOW(state) >= state->sgi_timeout ||
		   (state->sgi_timeout - NOW(state)) > state->sgi_timer) {
			state->sgi_state = SGI_RESET;
		} else if (is_sgi && sgi_prev == SGI_ENABLED && state->pncfails >= ncf_lmt) {
			/* Upon 4 consecutive failures, clear SGI and return FALSE
			 * so that wlc_rate_godown_mcs will handle rate down
			 */
			state->sgi_state = SGI_RESET;
			/* reduce penality.
			   state->sgi_lastprobe_ok = FALSE;
			   state->sgi_timer = state->rsi->sgi_tmin;
			   state->sgi_timeout = NOW(state) + state->sgi_timer;
			*/
		}
		if (is_sgi && state->sgi_state == SGI_RESET)
			clr_cache = TRUE;

	} else if (sgi_prev != SGI_RESET) {
		/* probing: can come here with sgi=0
		 * since we don't change epoch when start probing
		 */

		bool cur_probe_ok = FALSE;

		/* if currently at probe, hold rate */
		hold_rate = TRUE;

		if (!is_sgi || state->sgi_cnt == state->sgi_state)
			/* the txstatus didn't update sgi_state */
			return hold_rate;

		state->sgi_cnt = state->sgi_state;

		/* tolerate some margin */
		if (state->ncfails >= ncf_lmt ||
		    state->psr_sgi * 10 < state->psri[PSR_CUR_IDX].psr * 9) {
			/* probe fails */
			state->sgi_state = SGI_DISABLED;
		} else if (state->sgi_state == SGI_PROBE_END) {
			/* probe succeeds */
			state->sgi_state = SGI_ENABLED;
			cur_probe_ok = TRUE;
		}

		if (!SGI_PROBING(state)) {
			/* state has changed, double or reset the timer */
			state->sgi_timeout = NOW(state) + state->sgi_timer;
			if (state->sgi_lastprobe_ok == cur_probe_ok)
				state->sgi_timer = MIN(2*state->sgi_timer, state->rsi->sgi_tmax);
			else {
				state->sgi_timer = state->rsi->sgi_tmin;
				state->sgi_lastprobe_ok = cur_probe_ok;
			}
			/* probing --> enabled */
			if (state->sgi_state == SGI_ENABLED) {
				state->psri[PSR_CUR_IDX].psr = state->psr_sgi;
			}
			/* probing has sgi on, now is enabled/disabled, need to clear cache */
			clr_cache = TRUE;
		}
	}

	if (state->sgi_state == SGI_RESET) {
		if (is_sgi) {
			/* quitely reset the statistics */
			state->nupd = 0;
			/* state->psri[PSR_CUR_IDX].psr = PSR_MAX; */
			/* state->psri[PSR_FBR_IDX].psr = PSR_MAX; */
			/* state->psri[PSR_FBR_IDX].timestamp = NOW(state); */
			/* state->psri[PSR_UP_IDX].psr = PSR_UNINIT; */
			/* state->psri[PSR_DN_IDX].psr = PSR_UNINIT; */
			/* change epoch to force reset stays for several txstatus */
			state->epoch = MODINCR(state->epoch, 1+(RATE_EPOCH_MAX(wlc->pub->corerev)));
			clr_cache = TRUE;
		}

		/* start to probe if enter stage 2 AND the current tx is good
		 * AND not spatial probing
		 */
		if (state->nupd >= state->rsi->ema_nupd_thres &&
		    (state->ncfails == 0 && state->pncfails <= (NCFAIL_LMT0/2)) &&
		    !SP_PROBING(state) && !EXTSP_PROBING(state)) {
			/* start to probe only if current bw allows sgi */
			if (state->sgi_bwen & (1 << state->bw[state->rateid])) {
				state->sgi_state = SGI_PROBING;
				state->sgi_cnt = state->sgi_state;
				/* start from current primary psr */
				state->psr_sgi = state->psri[PSR_CUR_IDX].psr;
				/* hold rate while start to probe */
				clr_cache = TRUE;
				hold_rate = TRUE;
			}
		}
	}

	if (clr_cache)
		state->txc_inv_reqd = TRUE;

#ifdef BCMDBG
	if (WL_RATE_ON() && state->sgi_state != sgi_prev) {
		if (state->sgi_state == SGI_DISABLED || state->sgi_state == SGI_ENABLED)
			RL_SGI0(("%s: state %2d->%2d nupd %d is_sgi %d psr_sgi/cur %d %d "
				 "ncfails %u %u timeout %d now %d timer %d\n", __FUNCTION__,
				 sgi_prev, state->sgi_state, state->nupd, is_sgi,
				 state->psr_sgi, state->psri[PSR_CUR_IDX].psr,
				 state->ncfails, state->pncfails,
				 state->sgi_timeout, NOW(state), state->sgi_timer));
		else if (SGI_PROBING(state))
			RL_SGI0(("%s: state %2d->%2d nupd %d is_sgi %d psr_sgi/cur %d %d "
				 "ncfails %u %u hold_rate %d\n", __FUNCTION__, sgi_prev,
				 state->sgi_state, state->nupd, is_sgi, state->psr_sgi,
				 state->psri[PSR_CUR_IDX].psr,
				 state->ncfails, state->pncfails, hold_rate));
		else
			RL_SGI0(("%s: state %2d->%2d timeout %d now %d\n", __FUNCTION__,
				sgi_prev, state->sgi_state, state->sgi_timeout, NOW(state)));
	}
	if (SGI_PROBING(state))
		RL_SGI1(("%s: sgi %d state %d nupd %d clr %d hold %d psr_sgi/cur %d %d "
			 "timer %d timeout %d now %d\n", __FUNCTION__,
			 is_sgi, state->sgi_state, state->nupd, clr_cache, hold_rate,
			 state->psr_sgi, state->psri[PSR_CUR_IDX].psr,
			 state->sgi_timer, state->sgi_timeout, NOW(state)));
#endif /* BCMDBG */
	WL_DPRINT((state->rsi->wlc->macdbg, state->scb,
		"%s: sgi %d state %d->%d nupd %d clr %d hold %d psr_sgi/cur %d %d "
		"timer %d timeout %d now %d\n", __FUNCTION__,
		is_sgi, sgi_prev, state->sgi_state, state->nupd, clr_cache, hold_rate,
		state->psr_sgi, state->psri[PSR_CUR_IDX].psr,
		state->sgi_timer, state->sgi_timeout, NOW(state)));

	return hold_rate;
}

/** Function to determine whether to move up in the rateset select_rates[] */
static bool
#if RSSI_PRI_ON
wlc_ratesel_godown(rcb_t *state, uint8 down_rateid)
#else
wlc_ratesel_godown(rcb_t *state)
#endif // endif
{
	ratesel_info_t *rsi;
	bool decision = FALSE, down_ncf = FALSE;
	uint32 psr_dn = 0, prate_cur = 0, prate_dn = 0, prate_fbr = 0;
	uint ncf_lmt;
#if RSSI_PRI_ON
	bool forced = FALSE;
#else
	uint8 down_rateid;
#endif // endif
	ratespec_t down_rspec;
	psr_info_t *fbr, *cur, *dnp;

#if RSSI_PRI_ON
	if (down_rateid != RATEID_AUTO) {
		/* forced rate down */
		down_rspec = RATESPEC_OF_I(state, down_rateid);
		forced = TRUE;
	} else
#endif // endif
	if (!wlc_ratesel_next_rate(state, -1, &down_rateid, &down_rspec))
		return FALSE;

	rsi = state->rsi;
	prate_cur = RSPEC2RATE500K(CUR_RATESPEC(state));
	prate_dn = RSPEC2RATE500K(down_rspec);

	cur = (psr_info_t*)(state->psri + PSR_CUR_IDX);
	dnp = (psr_info_t*)(state->psri + PSR_DN_IDX);
	fbr = (psr_info_t*)(state->psri + PSR_FBR_IDX);

#if RSSI_PRI_ON
	if (forced) {
		decision = TRUE;
		goto make_decision;
	}
#endif // endif
	ncf_lmt = wlc_ratesel_get_ncflmt(state);
	if (state->ncfails >= ncf_lmt) {
		down_ncf = TRUE;
		decision = TRUE;
		goto make_decision;
	}

	/* estimate the nominal throughput at the next rate */
	ASSERT(fbr->psr != PSR_UNINIT);

	/* it is possible when a spatial probe happened. */
	if (dnp->rspec != down_rspec && dnp->psr != PSR_UNINIT)
		dnp->psr = PSR_UNINIT;

	/* derive psr_dn from the fallback rate using linear interpolation */
	prate_fbr = RSPEC2RATE500K(fbr->rspec);

	if (prate_cur == prate_fbr) {
		RL_MORE(("%s: current rate == fallback rate 0x%x\n", __FUNCTION__, prate_cur));
		return FALSE;
	}

	ASSERT((prate_cur > prate_dn) && (prate_dn >= prate_fbr));
	if (prate_cur <= prate_dn || prate_dn < prate_fbr) {
		psr_dn = PSR_MAX;
	} else {
		psr_dn = (cur->psr * (prate_dn - prate_fbr) + fbr->psr * (prate_cur - prate_dn)) /
			(prate_cur - prate_fbr);
	}

	/* use the smaller of estimate and recent (if avail.) */
	if (dnp->psr != PSR_UNINIT && (NOW(state) - dnp->timestamp) <= AGE_LMT(state)) {
		RL_MORE(("%s: psr_dn use recent %d instead of estimate %d\n",
			__FUNCTION__, dnp->psr, psr_dn));
		psr_dn = dnp->psr;
	}
	ASSERT(psr_dn <= PSR_MAX);

	/* compare nominal throughputs at current and down rate */
	decision = (prate_dn * psr_dn > prate_cur * cur->psr);

#ifdef WL_LPC
	if (LPC_ENAB(rsi->wlc) && (prate_dn > 0))
		wlc_ratesel_upd_tpr(state, prate_cur, cur->psr, prate_dn, psr_dn);
#endif // endif

make_decision:
	/* action time */
	if (decision) {
		/* change rate FIRST and do clean up work if necessary */
		ratespec_t prv_rspec = CUR_RATESPEC(state);
		state->rateid = down_rateid;
		if (SP_MODE(state) == SP_EXT) {
			if (IS_MCS(prv_rspec) && (WL_RSPEC_RATE_MASK & prv_rspec) == MCS8) {
				wlc_ratesel_clear_extspstat(state);
				state->antselid_extsp_ipr = MODINCR(state->antselid,
					state->active_antcfg_num);
				state->antselid_extsp_xpr = state->antselid; /* reset prb */
			}
		}

#if defined(AP) || defined(WLTDLS)
		if (rsi->use_rssi) {
			uint32 thresh = IS_20BW(state) ? 11 : 22;
			if (prate_cur > thresh && prate_dn <= thresh)
				/* start to compute rssi */
				if (rsi->enable_rssi_fn)
					rsi->enable_rssi_fn(state->scb);
		}
#endif // endif
		/* stop sending short A-MPDUs (if we still do) */
		state->mcs_short_ampdu = FALSE;

#if RSSI_PRI_ON
		if (forced)
			RL_INFO(("%s: force DOWN\n", __FUNCTION__));
		else
#endif /* RSSI_PRI_ON */
		if (down_ncf)
			RL_CHG(("%s: psr_cur %d DOWN upon %d consecutive failed tx\n",
				__FUNCTION__, cur->psr, state->ncfails));
		else
			RL_CHG(("%s: rate_cur/fbr/dn %d %d %d psr_cur/fbr/dn %d %d %d "
				"nthrt_cur/dn 0x%x 0x%x DOWN\n", __FUNCTION__,
				prate_cur, prate_fbr, prate_dn, cur->psr, fbr->psr,
				psr_dn, prate_cur * cur->psr, prate_dn * psr_dn));

		if (rsi->hopping && SP_MODE(state) == SP_NORMAL && state->sp_nupd > 0) {
			uint sp_stream = wlc_ratespec_nss(
					RATESPEC_OF_I(state, state->last_mcs_sp_id));
			uint cur_stream =  wlc_ratespec_nss(prv_rspec);

			RL_CHG(("%s: spatial info last_sp_id %d sp_nupd %d sp_skipped %d "
				"nthrut_dn/sp 0x%x 0x%x\n",
				__FUNCTION__, state->last_mcs_sp_id, state->sp_nupd,
				state->sp_nskip, prate_dn * psr_dn, state->mcs_sp_thrt0));

			if (sp_stream < cur_stream &&
			    (state->mcs_sp_thrt0 * 9 > prate_dn * psr_dn * 10)) {
				state->rateid = state->last_mcs_sp_id;
				RL_CHG(("%s hop to sp id 0x%x thrt 0x%x sp_nupd %d. "
					 "DN rate %d thrt 0x%x\n", __FUNCTION__,
					 state->last_mcs_sp_id, state->mcs_sp_thrt0,
					 state->sp_nupd, prate_dn, prate_dn * psr_dn));
			}
		}
		/* reset sgi if probing */
		if (SGI_PROBING(state))
			state->sgi_state = SGI_RESET;
	} else {
		RL_MORE(("%s: rate_cur/fbr/dn %d %d %d psr_cur/fbr/dn %d %d %d nthrt_cur/dn "
			"0x%x 0x%x\n", __FUNCTION__, prate_cur, prate_fbr, prate_dn, cur->psr,
			fbr->psr, psr_dn, prate_cur * cur->psr, prate_dn * psr_dn));
	}
	WL_DPRINT((state->rsi->wlc->macdbg, state->scb,
		"%s: decision %d rate_cur/fbr/dn %d %d %d psr_cur/fbr/dn %d %d %d nthrt_cur/dn "
		"0x%x 0x%x\n", __FUNCTION__, decision, prate_cur, prate_fbr, prate_dn, cur->psr,
		fbr->psr, psr_dn, prate_cur * cur->psr, prate_dn * psr_dn));

	return decision;
} /* wlc_ratesel_godown */

static bool
wlc_ratesel_goup(rcb_t *state)
{
	bool decision = FALSE, use_hist = FALSE;
	uint32 psr_up, prate_cur, prate_up, prate_dn, prate_fbr;
	uint8 up_rateid = 0xFF;
	ratespec_t up_rspec, cur_rspec;
	psr_info_t *fbr, *cur, *upp, *dnp;

	cur = (psr_info_t*)(state->psri + PSR_CUR_IDX);
	upp = (psr_info_t*)(state->psri + PSR_UP_IDX);
	dnp = (psr_info_t*)(state->psri + PSR_DN_IDX);
	fbr = (psr_info_t*)(state->psri + PSR_FBR_IDX);

	ASSERT(fbr->psr != PSR_UNINIT);

	/* have to collect enough sample for EMA to follow up */
	if (state->nupd < state->rsi->min_nupd)
		return FALSE;

	/* obtain the next-up rate first. If no change, return false. */
	if (!wlc_ratesel_next_rate(state, +1, &up_rateid, &up_rspec))
		return FALSE;

	cur_rspec = CUR_RATESPEC(state);
	prate_cur = RSPEC2RATE500K(cur_rspec);
	prate_up = RSPEC2RATE500K(up_rspec);
	prate_fbr = RSPEC2RATE500K(fbr->rspec);

	/* it is possible when a spatial probe happened. */
	if (upp->rspec != up_rspec && upp->psr != PSR_UNINIT)
		upp->psr = PSR_UNINIT;

	/* use the up rate psri if it is still recent */
	if (state->rateid != 0)
		ASSERT(fbr->rspec != cur_rspec);

	psr_up = upp->psr;
	if (psr_up != PSR_UNINIT && (NOW(state) - upp->timestamp) <= AGE_LMT(state))
		use_hist = TRUE;

	if (use_hist) {
		/* use history */
		RL_MORE(("%s: psr_up_recent %u\n", __FUNCTION__, psr_up));
	} else if (prate_fbr == prate_cur) {
		/* probe once a while regardless the PSR. */
		decision = TRUE;
	} else if (dnp->psr != PSR_UNINIT && (NOW(state) - dnp->timestamp) <= AGE_LMT(state)) {
		/* derive psr_up from down rate using linear extrapolation */
		if (dnp->psr <= cur->psr)
			psr_up = cur->psr;
		else {
			/* have to assume certain monitonicity */
			prate_dn = RSPEC2RATE500K(dnp->rspec);
			if (prate_up <= prate_cur || prate_cur <= prate_dn) {
				printf("%s: prate_up/cur/dn: %d %d %d\n", __FUNCTION__,
					prate_up, prate_cur, prate_dn);
				ASSERT(prate_up > prate_cur && prate_cur > prate_dn);
				return FALSE;
			} else {
				psr_up = (cur->psr * (prate_up - prate_dn)
					  - dnp->psr * (prate_up - prate_cur)) /
					  (prate_cur - prate_dn);
			}
		}
	} else {
		/* derive psr_up from fallback rate */
		if (fbr->psr <= cur->psr) {
			/* if PSR at fbr is even lower than that at the current rate */
			psr_up = cur->psr;
		} else {
			/* have to assume certain monitonicity */
			ASSERT(prate_up > prate_cur && prate_cur > prate_fbr);
			psr_up = (cur->psr * (prate_up - prate_fbr)
				  - fbr->psr * (prate_up - prate_cur)) / (prate_cur - prate_fbr);

		}
	}
	if (psr_up > PSR_MAX) {
		psr_up = 0; /* negative */
	}

	if (!decision) {
		/* throughput criteria */
		decision = (prate_up * psr_up) > prate_cur * cur->psr;
	}

	/* action time */
	if (decision) {
		/* change rate FIRST and do clean up work if necessary */
		ratespec_t prv_rspec = CUR_RATESPEC(state);
		uint8 prev_mcs = WL_RSPEC_RATE_MASK & prv_rspec;
		state->rateid = up_rateid;
		if (IS_MCS(prv_rspec) && (prev_mcs == MCS12 || prev_mcs == MCS21) &&
		    (state->mcs_sp == -1 || state->mcs_sp < prev_mcs)) {
			if (SP_MODE(state) == SP_EXT &&
			    state->extsp_ixtoggle == X_PROBE) {
				/* always go from X to fresh I probe on way to mcs13 */
				WL_RATE(("%s: clearing extsp stats on way to m13/22\n",
					__FUNCTION__));
				wlc_ratesel_clear_extspstat(state);
				state->extsp_ixtoggle = I_PROBE;
			} else if (SP_MODE(state) == SP_NORMAL) {
				WL_RATE(("%s: clearing sp stats on way to m13/22\n", __FUNCTION__));
				wlc_ratesel_clear_spstat(state);
			}
		}

#if defined(AP) || defined(WLTDLS)
		if (state->rsi->use_rssi) {
			uint32 thresh = IS_20BW(state) ? 11 : 22;
			if (prate_cur <= thresh && prate_up > thresh) {
				/* stop to compute rssi */
				if (state->rsi->disable_rssi_fn)
					state->rsi->disable_rssi_fn(state->scb);
			}
		}
#endif // endif

		/* start with short A-MPDUs */
		if (!RATELINKMEM_ENAB(state->rsi->pub)) {
			/* short a-mpdu feature is only without ratemem feature */
			state->mcs_short_ampdu = TRUE;
		}

		if (fbr->rspec == cur_rspec) {
			if (use_hist)
				RL_CHG(("%s: rate_cur/up %d %d psr_cur/up %d %d nthrt_cur/up "
					"0x%x 0x%x\n", __FUNCTION__, prate_cur, prate_up, cur->psr,
					psr_up, prate_cur * cur->psr, prate_up * psr_up));
			else
				RL_CHG(("%s: rate_cur/up %d %d psr_cur %d\n", __FUNCTION__,
					prate_cur, prate_up, cur->psr));
		} else {
			prate_fbr = RSPEC2RATE500K(fbr->rspec); /* wasn't init'd if using history */
			RL_CHG(("%s: rate_cur/fbr/up %d %d %d psr_cur/fbr/up %d %d %d "
				"nthrt_cur/up 0x%x 0x%x \n", __FUNCTION__, prate_cur, prate_fbr,
				prate_up, cur->psr, fbr->psr, psr_up, prate_cur * cur->psr,
				prate_up * psr_up));
		}
		/* reset sgi if probing */
		if (state->sgi_state >= SGI_PROBING)
			state->sgi_state = SGI_RESET;
	} else {
		if (fbr->rspec == cur_rspec) {
			if (use_hist)
				RL_MORE(("%s: rate_cur/up %d %d psr_cur/up %d %d nthrt_cur/up "
					"0x%x 0x%x\n", __FUNCTION__, prate_cur, prate_up, cur->psr,
					psr_up, prate_cur * cur->psr, prate_up * psr_up));
			else
				RL_MORE(("%s: rate_cur/up %d %d psr_cur %d\n", __FUNCTION__,
					prate_cur, prate_up, cur->psr));
		} else {
			prate_fbr = RSPEC2RATE500K(fbr->rspec); /* wasn't init'd if using history */
			RL_MORE(("%s: rate_cur/fbr/up %d %d %d psr_cur/fbr/up %d %d %d "
				"nthrt_cur/up 0x%x 0x%x \n", __FUNCTION__, prate_cur, prate_fbr,
				prate_up, cur->psr, fbr->psr, psr_up, prate_cur * cur->psr,
				prate_up * psr_up));
		}
	}
	WL_DPRINT((state->rsi->wlc->macdbg, state->scb,
		"%s: decision %d rate_cur/fbr/up %d %d %d psr_cur/fbr/up %d %d %d "
		"nthrt_cur/up 0x%x 0x%x \n", __FUNCTION__, decision, prate_cur, prate_fbr,
		prate_up, cur->psr, fbr->psr, psr_up, prate_cur * cur->psr, prate_up * psr_up));

	return decision;
} /* wlc_ratesel_goup */

static bool
wlc_ratesel_start_probe(rcb_t *state)
{
	bool sp_start = FALSE;
	if (SP_MODE(state) == SP_NORMAL) {
		/* inject spatial probe packet for current mcs_id, if available */
		if ((state->mcs_sp_flgs & SPFLGS_VAL_MASK) &&
		    (state->mcs_sp_flgs & SPFLGS_EN_MASK) == 0) {
			/* don't start if not enabled and already started */
			ASSERT(state->mcs_sp_Pmod != 0);
			ASSERT(state->mcs_sp_id < state->active_rates_num);
			if (((state->nupd + state->cur_sp_nupd) % state->mcs_sp_Pmod)
			    == state->mcs_sp_Poff) {
				state->mcs_sp_flgs |= SPFLGS_EN_MASK;
				RL_SP1(("aid %d nupd %d cur_sp_nupd %d "
					"start spatial probe to id %d (pri: %d) [%d, %d %d]\n",
					state->aid, state->nupd, state->cur_sp_nupd,
					state->mcs_sp_id - state->mcs_baseid,
					state->rateid - state->mcs_baseid, state->mcs_sp_K,
					state->mcs_sp_Pmod, state->mcs_sp_Poff));
				RL_SGI0(("%s: aid %d nupd %d sgi_state %d\n", __FUNCTION__,
					state->aid, state->nupd, state->sgi_state));
				sp_start = TRUE;
			}
		}
	} else if (SP_MODE(state) == SP_EXT && (state->extsp_flgs & EXTSPFLGS_EN_MASK) == 0) {
		/* don't start if already started */
		ASSERT(state->extsp_Pmod != 0);
		if (state->extsp_Pmod == 0) {
			sp_start = FALSE;
		} else {
		if (((state->nupd + state->cur_sp_nupd) % state->extsp_Pmod) == state->extsp_Poff) {
			/* inject extended spatial probe packet for current mcs_id, if available */
			uint16 valid;

			if (state->extsp_ixtoggle == I_PROBE)
				valid = state->extsp_flgs & EXTSPFLGS_IPR_VAL_MASK;
			else
				valid = state->extsp_flgs & EXTSPFLGS_XPR_VAL_MASK;
			if (valid) {
				state->extsp_flgs |= EXTSPFLGS_EN_MASK;
				if (WL_RATE_ON()) { /* no enough room to indent */

				ratespec_t cur_rspec;
				uint32 cur_rate;
				bool is_mcs;
				ratespec_t extsp_rspec;
				cur_rspec = CUR_RATESPEC(state);
				cur_rate = WL_RSPEC_RATE_MASK & cur_rspec;
				is_mcs = IS_MCS(cur_rspec);
				extsp_rspec = RATESPEC_OF_I(state, state->extsp_xpr_id);
				if (state->extsp_ixtoggle == I_PROBE)
					RL_SP1(("start ext_spatial I-probe to ant %d "
						 "(pri: %sx%02x %d) [%d, %d %d]\n",
						 state->antselid_extsp_ipr, is_mcs ? "m" : "",
						 cur_rate, state->antselid, state->extsp_K,
						 state->extsp_Pmod, state->extsp_Poff));
				else {
					RL_SP1(("start ext_spatial X-probe to mx%x ant %d "
						 "(pri: mx%x %d) [%d, %d %d]\n",
						 WL_RSPEC_RATE_MASK & extsp_rspec,
						 state->antselid_extsp_xpr, cur_rate,
						 state->antselid, state->extsp_K,
						 state->extsp_Pmod, state->extsp_Poff));
				}
				}
			sp_start = TRUE;
			}
		}
	}
	}
#ifdef WLC_DTPC
	if (DTPC_ACTIVE(state)) {
		wlc_dtpc_upd_sp(state->rsi->wlc->dtpc, state->scb, sp_start);
	}
#endif /* WLC_DTPC */

	return sp_start;
}

void
wlc_ratesel_probe_ready(rcb_t *state, uint16 frameid,
	bool is_ampdu, uint8 ampdu_txretry)
{
	BCM_REFERENCE(is_ampdu);

	/* stop probing as soon as first probe is ready for DMA */
	if (ampdu_txretry == 0) {

		/* remember to clear the TXC cache */
		if ((state->mcs_sp_flgs & SPFLGS_EN_MASK) == 1)
			state->txc_inv_reqd = TRUE;

		/* stop spatial probe */
		state->mcs_sp_flgs &= ~SPFLGS_EN_MASK;

		/* remember to clear the TXC cache */
		if ((state->extsp_flgs & EXTSPFLGS_EN_MASK) == 1)
			state->txc_inv_reqd = TRUE;

		/* stop spatial probe */
		state->extsp_flgs &= ~EXTSPFLGS_EN_MASK;
	}
	RL_SP0(("%s: aid %d frameid 0x%x ampdu_txretry %d mcs_sp_flgs 0x%x extsp_flgs 0x%x "
		"txc_inv_reqd %d nupd %d cur_sp_nupd %d\n",
		__FUNCTION__, state->aid, frameid, ampdu_txretry, state->mcs_sp_flgs,
		state->extsp_flgs, state->txc_inv_reqd,
		state->nupd, state->cur_sp_nupd));

	if (state->txc_inv_reqd) {
		INVALIDATE_TXH_CACHE(state);
	}

	if (state->upd_pend && (RATELINKMEM_ENAB(state->rsi->pub))) {
		/* trigger ratemem update */
		ratesel_txparams_t ratesel_rates;
		uint16 flag0, flag1;
		ratesel_rates.num = 4;
		ratesel_rates.ac = 0;
		wlc_ratesel_gettxrate(state, &flag0, &ratesel_rates, &flag1);
	}
}

static void
wlc_ratesel_toggle_probe(rcb_t *state)
{
	/* clear spatial stats */
	wlc_ratesel_clear_extspstat(state);

	/* toggle probe */
	state->extsp_ixtoggle = (1 - state->extsp_ixtoggle);

	/* enforce i-probe if x-probe not valid */
	if ((state->extsp_ixtoggle == X_PROBE) &&
	    ((state->extsp_flgs & EXTSPFLGS_XPR_VAL_MASK) == 0)) {
		state->extsp_ixtoggle = I_PROBE;
	}

}

/**
 * Normal spatial probing between SISO and MIMO rates.
 * Return: TRUE if decide to switch the family.
 */
static bool
wlc_ratesel_change_sp(rcb_t *state)
{
	ratespec_t cur_rspec;

	/* fill spatial window before any family switch decision */
	if (state->sp_nupd < SPATIAL_M || state->mcs_sp_id == state->rateid ||
		state->mcs_sp_id >= state->active_rates_num)
		return FALSE;

	cur_rspec = RATESPEC_OF_I(state, state->rateid);

	/* switch to other family if prev M A-MPDUs statuses <= K */
	if ((state->rsi->ratesel_sp_algo && (state->mcs_sp_thrt0 > state->mcs_sp_thrt1)) ||
	   (!state->rsi->ratesel_sp_algo && (SPATIAL_M - state->mcs_sp_statc) <= state->mcs_sp_K)) {
		/* switch */
		state->rateid = state->mcs_sp_id;

#ifdef BCMDBG
		/* below is debug info */
		if (WL_RATE_ON()) {
			ratespec_t prv_rspec = cur_rspec;
			cur_rspec = RATESPEC_OF_I(state, state->rateid);
			if (state->rsi->ratesel_sp_algo) {
				WL_RATE(("rate_changed(spatially): mx%02xb%d to mx%02xb%d (%d %d) "
				         "after %2u (%u) txs with %u (%u) skipped."
				         "nthrt_pri/sp 0x%x 0x%x\n",
				         WL_RSPEC_RATE_MASK & prv_rspec, RSPEC2BW(prv_rspec),
				         WL_RSPEC_RATE_MASK & cur_rspec, RSPEC2BW(cur_rspec),
				         state->rateid, RSPEC2RATE500K(cur_rspec),
				         state->nupd, state->sp_nupd, state->nskip,
				         state->sp_nskip,
				         state->mcs_sp_thrt1, state->mcs_sp_thrt0));
			} else {
				WL_RATE(("rate_changed(spatially): mx%02xb%d to mx%02xb%d (%d %d) "
				         "after %2u (%u) txs with %u (%u) skipped."
				         "mcs_sp_statc %d M %d K %d\n",
				         WL_RSPEC_RATE_MASK & prv_rspec, RSPEC2BW(prv_rspec),
				         WL_RSPEC_RATE_MASK & cur_rspec, RSPEC2BW(cur_rspec),
				         state->rateid, RSPEC2RATE500K(cur_rspec),
				         state->nupd, state->sp_nupd, state->nskip,
				         state->sp_nskip, state->mcs_sp_statc,
				         SPATIAL_M, state->mcs_sp_K));
			}
			/* please compiler */
			BCM_REFERENCE(prv_rspec);
		}
#endif /* BCMDBG */
		return TRUE;
	} else {
		/* check whether we need to switch probing stream */
		int mcs, nss, tblidx, mcs_sp, mcs_sp_id = -1;
		uint8 tmp_col;
		/* the current rate has to be mcs so need to check */
		nss = wlc_ratespec_nss(cur_rspec);
#ifdef WL11AC
		mcs = WL_RSPEC_VHT_MCS_MASK & cur_rspec;
		tblidx = nss - 1;
		tblidx = MAX_VHT_RATES * tblidx + mcs + 1;
#else
		mcs = cur_rspec & WL_RSPEC_RATE_MASK;
		tblidx = mcs + 1;
#endif // endif
		if (tblidx < 0 || tblidx >= RATES_NUM_MCS) {
#ifdef WLC_DTPC
			if (DTPC_ACTIVE(state)) {
				wlc_dtpc_upd_sp(state->rsi->wlc->dtpc, state->scb, FALSE);
			}
#endif /* WLC_DTPC */
			return FALSE;
		}

		if (nss > 1 && nss < state->mcs_streams) {
			/* swap only if current nss is in [2, mcs_stream-1] */
			tmp_col = (state->mcs_sp_col == SPATIAL_MCS_COL1) ?
				SPATIAL_MCS_COL2 : SPATIAL_MCS_COL1;
		} else
			tmp_col = SPATIAL_MCS_COL1;

		mcs_sp = wlc_mcs_sp_params(tblidx, tmp_col);
		if (mcs_sp != -1) {
			mcs_sp_id = wlc_ratesel_mcsbw2id(state, (uint8) mcs_sp,
				state->bw[state->rateid]);
#ifdef WL11AC
			if (mcs_sp_id == -1 && (mcs_sp & WL_RSPEC_VHT_MCS_MASK) == 10) {
				/* c10 is not in the rate set then try the next lower one */
				mcs_sp --;
				mcs_sp_id = wlc_ratesel_mcsbw2id(state, (uint8) mcs_sp,
						state->bw[state->rateid]);
			}
#endif // endif
		}
		if (mcs_sp_id > 0) {
			/* switch */
			RL_SP0(("%s: switching probing stream family: 0x%x --> 0x%x\n",
				__FUNCTION__, state->mcs_sp, mcs_sp));
			mcs_sp = RATESPEC_OF_I(state, mcs_sp_id) & WL_RSPEC_RATE_MASK;
			state->mcs_sp_col = tmp_col;
			state->mcs_sp = (uint8)mcs_sp;
			state->mcs_sp_id = (uint8) mcs_sp_id;
			wlc_ratesel_clear_spstat(state);
		}
	}
#ifdef WLC_DTPC
	if (DTPC_ACTIVE(state)) {
		wlc_dtpc_upd_sp(state->rsi->wlc->dtpc, state->scb, FALSE);
	}
#endif // endif
	return FALSE;
}

/**
 * Extended spatial probing between SISO and MIMO rates.
 * Return: TRUE if decide to switch the family.
 */
static bool
wlc_ratesel_change_extsp(rcb_t *state)
{
	/*
	 *  check for early abort: stop this probing run, config probe to next ant choice,
	 *  flush spatial probe, toggle probe, and goto ratehold.
	 */
	if ((state->rsi->ratesel_sp_algo && state->extsp_thrt0 < state->extsp_thrt1) ||
	    (!state->rsi->ratesel_sp_algo &&
	     (state->sp_nupd - state->extsp_statc) > state->extsp_K)) {

		if (state->extsp_ixtoggle == I_PROBE) {
			if (state->rsi->ratesel_sp_algo)
				RL_SP1(("%s: aborting I-PROBE, probe antselid = %d nupd %d "
					 "thrt_pri/sp = 0x%x 0x%xd\n", __FUNCTION__,
					 state->antselid_extsp_ipr, state->sp_nupd,
					 state->extsp_thrt1, state->extsp_thrt0));
			else
				RL_SP1(("%s: aborting I-PROBE, probe antselid = %d nupd/statc/K = "
					 "%d %d %d\n", __FUNCTION__, state->antselid_extsp_ipr,
					 state->sp_nupd, state->extsp_statc, state->extsp_K));
			state->antselid_extsp_ipr = MODINCR(state->antselid_extsp_ipr,
				state->active_antcfg_num);
			/* avoid i-probe to self: */
			if (state->antselid_extsp_ipr == state->antselid)
				state->antselid_extsp_ipr = MODINCR(state->antselid,
					state->active_antcfg_num);
		} else {
			if (state->rsi->ratesel_sp_algo)
				RL_SP1(("%s: aborting X-PROBE probe mcs/antselid = 0x%x %d nupd %d "
					"thrt_pri/sp = 0x%x 0x%x\n", __FUNCTION__,
					WL_RSPEC_RATE_MASK &
					known_rspec[state->select_rates[state->extsp_xpr_id]],
					state->antselid_extsp_xpr, state->sp_nupd,
					state->extsp_thrt1, state->extsp_thrt0));
			else
				RL_SP1(("%s: aborting X-PROBE, probe mcs/antselid = 0x%x %d "
					 "nupd/statc/K = %d %d %d\n", __FUNCTION__,
					 WL_RSPEC_RATE_MASK &
					 known_rspec[state->select_rates[state->extsp_xpr_id]],
					 state->antselid_extsp_xpr, state->sp_nupd,
					 state->extsp_statc, state->extsp_K));
			state->antselid_extsp_xpr = MODINCR(state->antselid_extsp_xpr,
				state->active_antcfg_num);
		}
		if (state->extsp_ixtoggle == I_PROBE)
			RL_SP1(("%s: start I-Probe antselid %d\n",
				__FUNCTION__, state->antselid_extsp_ipr));
		else
			RL_SP1(("%s: start X-Probe antselid %d sp_mcs 0x%x\n",
				__FUNCTION__, state->antselid_extsp_xpr, WL_RSPEC_RATE_MASK &
				known_rspec[state->select_rates[state->extsp_xpr_id]]));
		wlc_ratesel_toggle_probe(state); /* clears stats and toggle probe if possible */

		return FALSE;
	}

	/*
	 *  if window filled & no abort in previous section,
	 *  do full-blown transition
	 */
	if  (state->sp_nupd >= EXTSPATIAL_M) { /* window filled */
		uint8 prv_rateid = state->rateid;
		uint8 prv_antselid = state->antselid;

		/* looks promising, carry out transition */
		if (state->extsp_ixtoggle == I_PROBE) {
			state->antselid = state->antselid_extsp_ipr;
		} else {
			state->rateid = state->extsp_xpr_id;
			state->antselid = state->antselid_extsp_xpr;
		}
		state->antselid_extsp_ipr = MODINCR(state->antselid, state->active_antcfg_num);
		state->antselid_extsp_xpr = state->antselid;

		if (WL_RATE_ON()) {
			uint32 prv_rspec, cur_rspec;
			prv_rspec = RATESPEC_OF_I(state, prv_rateid);
			cur_rspec = RATESPEC_OF_I(state, state->rateid);
			if (prv_antselid == state->antselid) {

			RL_INFO(("rate_changed(ext_spat): %sx%02x to %sx%02x (%d %d) after %2u (%u)"
				 " txs with %u (%u) skipped. nthrt_pri/extsp 0x%x 0x%x\n",
				 IS_MCS(prv_rspec) ? "m" : "", WL_RSPEC_RATE_MASK & prv_rspec,
				 IS_MCS(cur_rspec) ? "m" : "", WL_RSPEC_RATE_MASK & cur_rspec,
				 state->rateid, RSPEC2RATE500K(cur_rspec), state->nupd,
				 state->sp_nupd, state->nskip, state->sp_nskip,
				 state->extsp_thrt1, state->extsp_thrt0));
			} else {

			RL_INFO(("rate_changed(ext_spat): antid %d to %d after %2u (%u) "
				 "txs with %u (%u) skipped. nthrt_pri/extsp 0x%x 0x%x\n",
				 prv_antselid, state->antselid, state->nupd,
				 state->sp_nupd, state->nskip, state->sp_nskip,
				 state->extsp_thrt1, state->extsp_thrt0));
			}
		} else {
			/* please compiler */
			BCM_REFERENCE(prv_rateid);
			BCM_REFERENCE(prv_antselid);
		}
		/* toggle probe if possible (includes clearing statistics) */
		wlc_ratesel_toggle_probe(state);

		return TRUE;
	}
	return FALSE;
}

/**
 * <is_probe>: Indicate this calling is due to a probe packet or not.
 */
static void BCMFASTPATH
wlc_ratesel_pick_rate(rcb_t *state, bool is_probe, bool is_sgi)
{
	uint8 prv_rateid;
	ratespec_t cur_rspec, prv_rspec;
	bool change_epoch = FALSE, change_rate = FALSE;

	/* sanity check */
	ASSERT(state->rateid < state->active_rates_num);

	prv_rateid = state->rateid;
	prv_rspec = CUR_RATESPEC(state);

	if (is_probe) {
		if (SP_MODE(state) == SP_NORMAL)
			change_rate = wlc_ratesel_change_sp(state);
		else if (SP_MODE(state) == SP_EXT) {
			if ((change_rate = wlc_ratesel_change_extsp(state))) {
				/* check if need to change epoch */
				cur_rspec = CUR_RATESPEC(state);
				if (!IS_MCS(prv_rspec) || !IS_MCS(cur_rspec))
					change_epoch = TRUE;
			}
		}
	} else if (wlc_ratesel_checksgi(state, is_sgi)) {
		/* need to hold at current rate */
	} else
	{
		BCM_REFERENCE(is_sgi);
		/* rate down */
#if RSSI_PRI_ON
		if (wlc_ratesel_godown(state, RATEID_AUTO)) {
#else
		if (wlc_ratesel_godown(state)) {
#endif // endif
			change_rate = TRUE;
		}
		/* rate up */
		else if (state->gotpkts == GOTPKTS_ALL) {
			if (wlc_ratesel_goup(state))
				change_rate = TRUE;
		}

		/* Force to pump up the epoch if rate has changed */
		change_epoch = change_rate;

		if (!change_rate && wlc_ratesel_start_probe(state))
			state->txc_inv_reqd = TRUE;

		if (WL_RATE_ON() && change_rate) {
			/* the rate has changed, vertically */
			uint8 cur_rate;
			cur_rspec = CUR_RATESPEC(state);

			ASSERT(cur_rspec != prv_rspec);
			cur_rate = WL_RSPEC_RATE_MASK & cur_rspec;

			if (prv_rateid != state->rateid) {
				/* the rate has just changed */
				WL_RATE(("rate_changed: %sx%02xb%d %s %sx%02xb%d (%d %d) after "
					"%2u used and %u skipped txs epoch %d\n",
					IS_MCS(prv_rspec) ? "m" : "",
					WL_RSPEC_RATE_MASK & prv_rspec,
					RSPEC2BW(prv_rspec),
					(prv_rateid > state->rateid) ? "DN" : "UP",
					IS_MCS(cur_rspec) ? "m" : "", cur_rate,
					RSPEC2BW(cur_rspec), state->rateid,
					RSPEC2RATE500K(cur_rspec), state->nupd,
					state->nskip, state->epoch));
			} else {
				/* the rate has just changed due to a rateset change */
				WL_RATE(("rate_changed(from-oob): %sx%02x DN %sx%02x (%d %d) "
					"after %2u used and %u skipped txs\n",
					IS_MCS(prv_rspec) ? "m" : "",
					WL_RSPEC_RATE_MASK & prv_rspec,
					IS_MCS(cur_rspec) ? "m" : "", cur_rate, state->rateid,
					RSPEC2RATE500K(cur_rspec), state->nupd, state->nskip));
			}
			BCM_REFERENCE(cur_rate);
		}
	}

	if (change_rate) {
		wlc_ratesel_clear_ratestat(state, change_epoch);
#ifdef WLC_DTPC
		if (DTPC_ACTIVE(state)) {
			uint8 upper_rateid = 0xFF;
			ratespec_t upper_rspec;
			wlc_dtpc_rate_change(state->rsi->wlc->dtpc, state->scb,
				CUR_RATESPEC(state), !(prv_rateid > state->rateid), state->epoch,
				!(wlc_ratesel_next_rate(state, +1, &upper_rateid, &upper_rspec)),
				is_probe);
		}
#endif // endif
	}

	return;
}

bool
wlc_ratesel_set_alert(ratesel_info_t *rsi, uint8 mode)
{
	if (mode & RATESEL_ALERT_RSSI && !rsi->get_rssi_fn) {
		WL_ERROR(("ALERT_RSSI mode unsupported: empty get_rssi_fn!\n"));
		return FALSE;
#if RSSI_PRI_ON
	} else if ((mode & RATESEL_ALERT_PRI) && (mode & RATESEL_ALERT_RSSI) == 0) {
		WL_ERROR(("Cannot turn on ALERT_PRI without ALERT_RSSI!\n"));
		return FALSE;
#endif // endif
	} else {
		rsi->alert = (mode & RATESEL_ALERT_ALL);
	}

	if (rsi->alert) {
		/* if threshold un-init'd, init to default value */
		if (rsi->idle_lmt == 0)
			rsi->idle_lmt = IDLE_LMT;
		if (rsi->rssi_rfbr == 0)
			rsi->rssi_rfbr = RSSI_RFBR_LMT_MFBR;
#if RSSI_PRI_ON
		if (rsi->rssi_pri == 0)
			rsi->rssi_pri = RSSI_PRI_LMT_MFBR;
#endif // endif
	}
	return TRUE;
}

static void BCMFASTPATH
wlc_ratesel_alert_upd(rcb_t *state)
{
	ratesel_info_t *rsi;

	if (state->gotpkts != GOTPKTS_ALL) {
		RL_MORE(("%s: gotpkts %d -> %d\n", __FUNCTION__, state->gotpkts, GOTPKTS_ALL));
		state->gotpkts = GOTPKTS_ALL;
		state->txc_inv_reqd = TRUE;
	}

	rsi = state->rsi;
	if (!rsi->alert)
		return;

	if (rsi->alert & RATESEL_ALERT_IDLE)
		state->lastxs_time = NOW(state);
	if (rsi->alert & RATESEL_ALERT_RSSI) {
		ASSERT(rsi->get_rssi_fn);
		state->lastxs_rssi = rsi->get_rssi_fn(state->scb);
	}

	RL_MORE(("%s: lastxs time %d rssi %d\n", __FUNCTION__,
		state->lastxs_time, state->lastxs_rssi));
}

/**
 * this function is called to watch out if
 * been idle too long and/or rssi has changed too much
 */
bool
wlc_ratesel_sync(rcb_t *state, uint now, int rssi)
{
	ratesel_info_t *rsi;
	bool clr;
	int delta;

	rsi = state->rsi;
	clr = FALSE;

	/* do nothing if feature is off or in init mode */
	if (!rsi->alert || (state->gotpkts & GOTPKTS_INIT) == 0)
		goto exit;

	if (rsi->alert & RATESEL_ALERT_IDLE) {
		if ((int)(now - state->lastxs_time) > (int)rsi->idle_lmt)
			clr = TRUE;
	}

	/* come here either clr is TRUE or alert doesn't care idle time */
	if (!clr && rsi->alert & RATESEL_ALERT_RSSI) {
		if (state->lastxs_rssi != WLC_RSSI_INVALID && rssi != WLC_RSSI_INVALID &&
		    rssi < RSSI_PEAK_LMT && rssi < state->lastxs_rssi) {
			/*
			 * both old and current rssi are valid, and
			 * rssi has dropped below threshold.
			 */
			if (state->lastxs_rssi >= RSSI_PEAK_LMT)
				delta = RSSI_PEAK_LMT - rssi;
			else
				delta = state->lastxs_rssi - rssi;
			if (delta > rsi->rssi_rfbr)
				clr = TRUE;
		}
	}
exit:
	if (clr) {
		INVALIDATE_TXH_CACHE(state);
	}
	return clr;
}

/**
 * if alert on rssi change or idle alone, only use rfbr (reliable fbr)
 * if alert on both, then possible drop primary rate
 */
static void BCMFASTPATH
wlc_ratesel_alert(rcb_t *state, bool mfbr)
{
	ratesel_info_t *rsi;
	int rssi_cur = WLC_RSSI_INVALID, rssi_drop;
	bool flag_rfbr;
#if RSSI_PRI_ON
	bool flag_pri;
#endif // endif

#if defined(BCMDBG)
	uint8 gotpkts_old = state->gotpkts;
#endif // endif

	/* do nothing if feature is off or in init mode already */
	rsi = state->rsi;
	if (!rsi->alert || (state->gotpkts & GOTPKTS_INIT) == 0)
		return;

#if RSSI_PRI_ON
	/* if in rfbr mode AND not in primary drop: do nothing */
	if ((state->gotpkts & GOTPKTS_RFBR) == 0 && (state->gotpkts & GOTPKTS_PRICHG))
		return;
#endif // endif

	/* in primary drop or alert hasn't triggered at all */
	if (rsi->alert & RATESEL_ALERT_IDLE) {
		if ((int)(NOW(state) - state->lastxs_time) < (int)rsi->idle_lmt) {
			/* idle too short, do nothing */
			return;
		}
	}
	flag_rfbr = TRUE;
#if RSSI_PRI_ON
	flag_pri = FALSE;
#endif // endif
	if (rsi->alert & RATESEL_ALERT_RSSI) {
		if (state->lastxs_rssi == WLC_RSSI_INVALID)
			return;
		if ((rssi_cur = rsi->get_rssi_fn(state->scb)) == WLC_RSSI_INVALID)
			return;

		RL_MORE(("%s time %d %d rssi %d %d\n", __FUNCTION__,
			NOW(state), state->lastxs_time, state->lastxs_rssi, rssi_cur));

		if (rssi_cur >= RSSI_PEAK_LMT)
			return;
		if (state->lastxs_rssi >= RSSI_PEAK_LMT)
			rssi_drop = RSSI_PEAK_LMT - rssi_cur;
		else
			rssi_drop =  state->lastxs_rssi - rssi_cur;
		if (rssi_drop <= rsi->rssi_rfbr)
			flag_rfbr = FALSE;
#if RSSI_PRI_ON
		if (rsi->alert & RATESEL_ALERT_PRI && rssi_drop > rsi->rssi_pri)
			flag_pri = TRUE;
#endif // endif
	}

	if (flag_rfbr) {
		/* might be in rfbr mode already but nevermind ... */
		state->gotpkts &= ~GOTPKTS_RFBR;
		if (!mfbr)
			state->gotpkts &= ~GOTPKTS_INIT;
	}

#if RSSI_PRI_ON
	if (flag_pri) {
		bool change = FALSE;
		uint8 curid, down_id = RATEID_AUTO;
		if (state->gotpkts & GOTPKTS_PRICHG) {
			state->rssi_lastdrop = state->lastxs_rssi;
		} else {
			/* already dropped primary */
			rssi_drop = state->rssi_lastdrop - rssi_cur;
		}
		WL_RATE(("%s rssi_last cur drop %d %d %d\n", __FUNCTION__,
			state->lastxs_rssi, rssi_cur, rssi_drop));

		curid = state->rateid;
		while (rssi_drop > RSSI_STEP) {
			/* lower primary to next fallback rate per rssi_step size */
			down_id = state->fbrid[curid];
			if (down_id == state->rateid)
				break;
			change = TRUE;
			rssi_drop -= RSSI_STEP;
			state->rssi_lastdrop -= RSSI_STEP;
			curid = down_id;
			WL_RATE(("rateid %d down_id %d rssi_drop %d lastdrop %d\n",
				state->rateid, down_id, rssi_drop, state->rssi_lastdrop));
		}
		if (change) {
			/* change primary rate */
			wlc_ratesel_godown(state, down_id);
			state->gotpkts &= ~GOTPKTS_PRICHG;
		}
	}
#endif /* RSSI_PRI_ON */

#if defined(BCMDBG)
	if (WL_RATE_ON() && gotpkts_old != state->gotpkts) {
		WL_RATE(("%s aid %d gotpkts %d -> %d\n", __FUNCTION__,
			state->aid, gotpkts_old, state->gotpkts));
		RL_MORE(("time: %d -> %d rssi: %d -> %d\n", state->lastxs_time, NOW(state),
			state->lastxs_rssi, rssi_cur));
	}
#endif // endif
}

/**
 * all of the following should only apply to directed traffic,
 * do not call me on group-addressed frames.
 * SFBL/LFBL is short/long retry limit for fallback rate
 */
static int
wlc_ratesel_use_txs(rcb_t *state, tx_status_t *txs, uint16 SFBL, uint16 LFBL,
	uint8 tx_mcs, bool sgi, uint8 antselid, bool fbr)
{
	uint8 nftx, nrtx, ncrx;
	bool nofb, acked;
	int ret_val = TXS_DISC;
#ifdef WL_LPC
	uint32 old_psr = 0;
#endif // endif
	int ntx_start_fb;
	uint32 *p, *pfbr;
	uint8 alpha;
	uint8 cur_mcs, ntx_mrt;
	ratespec_t cur_rspec;

	/* Do sanity check before changing gotpkts */
	wlc_ratesel_sanitycheck_psr(state);

	ASSERT(!TX_STATUS_UNEXP(txs->status));

	/* extract fields and bail if nothing was transmitted.
	 * if the frame indicates <fbr> fake nrtx.
	 * fbr is always FALSE for AMPDU_AQM_ENAB()
	 */
	nrtx = fbr ? (SFBL + 1) : (uint8) ((txs->status.rts_tx_cnt));
	nftx = (uint8) ((txs->status.frag_tx_cnt));

	if (nftx + nrtx == 0) {
		return ret_val;
	}

	ncrx = (uint8) ((txs->status.cts_rx_cnt)); /* only used for dbg print */
	BCM_REFERENCE(ncrx);

	acked = (txs->status.was_acked) ? TRUE : FALSE;

	/* Determine if fall-back rate was tried and
	 * after which transmission the fall-back rate is used.
	 */
	if (AMPDU_AQM_ENAB(state->rsi->wlc->pub)) {
		uint8 txcnt0;
		txcnt0 = TX_STATUS40_TXCNT_RT0(txs->status.s3);
		nofb = (nftx == txcnt0);
		ntx_start_fb = txcnt0;
		/* if nftx == 0, it means only rts transmissions happened
		 * but no special treat is needed for it as acked is FALSE and
		 * ntx_start_fb is not used for PSR calculation.
		 * Note that rate2/3 if tried will also change psri[PSR_FBR_IDX].psr.
		 */
	} else {
		nofb = (nrtx > 0) ? (nrtx <= SFBL && nftx <= LFBL) : (nftx <= SFBL);
		ntx_start_fb = (nrtx > SFBL) ? 0 : (nrtx == 0 ? SFBL : LFBL);
	}

	/* if this packet was received at the target rate, we
	 * can switch to the steady state rate fallback calculation of target rate -1
	 * AND it needs to clean the external 'clear-on-update' variable
	 */
	if (nofb && acked) {
		wlc_ratesel_alert_upd(state);
	}

	cur_rspec = CUR_RATESPEC(state);
	cur_mcs = IS_MCS(cur_rspec) ? (WL_RSPEC_RATE_MASK & cur_rspec) : MCS_INVALID;
	if (IS_SP_PKT(state, cur_mcs, tx_mcs)) {
		uint8 ntx_succ;
		ntx_mrt = MIN(nftx, ntx_start_fb); /* can be 0 */
		ntx_succ = (acked && nftx <= ntx_start_fb) ? 1 : 0;
		if (wlc_ratesel_upd_spstat(state, FALSE, tx_mcs, ntx_succ, ntx_mrt, 0))
			ret_val = TXS_PROBE;
		RL_SP1(("fid 0x%x spatial probe rcvd: mx%02x (pri: mx%02x) [nrtx ncrx nftx ack] = "
			"[%d %d %d %d] Upd: %c\n", txs->frameid, tx_mcs, cur_mcs,
			nrtx, ncrx, nftx, acked, (ret_val == TXS_PROBE) ? 'Y' : 'N'));
		return ret_val;
	} else if (IS_EXTSP_PKT(state, cur_mcs, tx_mcs, antselid)) {
		uint8 ntx_succ;
		ntx_mrt = MIN(nftx, ntx_start_fb); /* can be 0 */
		ntx_succ = (acked && nftx <= ntx_start_fb) ? 1 : 0;
		if (wlc_ratesel_upd_extspstat(state, FALSE, tx_mcs, antselid, ntx_succ, ntx_mrt))
			ret_val = TXS_PROBE;
		RL_SP1(("txs_ack: ext_spatial probe rcvd: %s%02x ant %d (pri: %s%02x ant %d) "
			"[nrtx ncrx nftx ack] = [%d %d %d %d] Upd: %c\n",
			(tx_mcs == MCS_INVALID) ? "" : "m", tx_mcs, antselid,
			(cur_mcs == MCS_INVALID) ? "" : "m", cur_mcs, state->antselid,
			nrtx, ncrx, nftx, acked, (ret_val == TXS_PROBE) ? 'Y' : 'N'));
		return ret_val;
	}

	/* All transmissions before the last are failures. */
	alpha = state->gotpkts ? ((state->nupd < state->rsi->ema_nupd_thres) ?
		state->rsi->psr_ema_alpha0 : state->rsi->psr_ema_alpha) : RATESEL_EMA_ALPHA_INIT;
	pfbr = (state->psri[PSR_CUR_IDX].rspec == state->psri[PSR_FBR_IDX].rspec) ?
		&(state->psri[PSR_CUR_IDX].psr) : &(state->psri[PSR_FBR_IDX].psr);
	if (sgi && SGI_PROBING(state)) {
		p = &(state->psr_sgi);
		state->sgi_state++;
	} else {
		p = &(state->psri[PSR_CUR_IDX].psr);
#ifdef WL_LPC
		if (LPC_ENAB(state->rsi->wlc))
			old_psr = state->psri[PSR_CUR_IDX].psr;
#endif // endif
	}

	if (nftx == 0) {
		/* handle the case of RTS-CTS-DATA and RTS-CTS fails: only treat as one failure */
		ASSERT(!acked);
		*p -= *p >> alpha;
	} else {
		int k;
		for (k = 1; k <= nftx; k++) {
			if (k > ntx_start_fb)
				p = pfbr;
			*p -= *p >> alpha;
		}
		if (acked) {
			/* Count the successful tx, which must be the last one. */
			*p += 1 << (RATESEL_EMA_NF - alpha);
			if ((nftx == 1) && (p != pfbr)) {
				/* if primary is very good then assume fbr is good as well.
				 * update to avoid stale pfbr.
				 */
				*pfbr -= (*pfbr >> alpha);
				*pfbr += 1 << (RATESEL_EMA_NF - alpha);
			}
		}
	}

#ifdef WL_LPC
	if (LPC_ENAB(state->rsi->wlc)) {
		/* update the state */
		wlc_ratesel_upd_la(state, state->psri[PSR_CUR_IDX].psr, old_psr);
	}
#endif /* WL_LPC */

	/* fbr is not always updated so need to timestamp it every time update it */
	if (pfbr == &(state->psri[PSR_FBR_IDX].psr)) {
		state->psri[PSR_FBR_IDX].timestamp = NOW(state);
	} else {
		if (!acked) state->pncfails ++;
	}

	/* for fast drop */
	if (acked) {
		state->ncfails = 0;
		state->pncfails = 0;
	} else {
		state->ncfails ++;
		wlc_ratesel_rfbr(state);
	}
	RL_MORE(("%s fid 0x%x: rid %d %d psr_cur/fbr %d %d [nrtx ncrx nftx acked ant fbr sgi] ="
		"[%d %d %d %d %d %d %d] nupd %u\n", __FUNCTION__, txs->frameid, state->rateid,
		RSPEC2RATE500K(CUR_RATESPEC(state)), state->psri[PSR_CUR_IDX].psr,
		state->psri[PSR_FBR_IDX].psr, nrtx, ncrx, nftx,
		acked, antselid, fbr ? 1 : 0, sgi, state->nupd));
	WL_DPRINT((state->rsi->wlc->macdbg, state->scb,
		"%s fid 0x%x: rid %d %d psr_cur/fbr %d %d [nrtx ncrx nftx acked ant fbr sgi] ="
		"[%d %d %d %d %d %d %d] nupd %u\n", __FUNCTION__, txs->frameid, state->rateid,
		RSPEC2RATE500K(CUR_RATESPEC(state)), state->psri[PSR_CUR_IDX].psr,
		state->psri[PSR_FBR_IDX].psr, nrtx, ncrx, nftx,
		acked, antselid, fbr ? 1 : 0, sgi, state->nupd));

	/* update the count of updates since the last state flush, saturating at max byte val */
	LIMINC_UINT32(state->nupd);
	return TXS_REG;
}

/** update per-scb state upon received tx status */
/* non-AMPDU txstatus rate update, default to use non-mcs rates only */
void
wlc_ratesel_upd_txstatus_normalack(rcb_t *state, tx_status_t *txs, uint16 sfbl, uint16 lfbl,
	uint8 tx_mcs /* non-HT format */, bool sgi, uint8 antselid, bool fbr)
{
	uint8 epoch;
	int txs_res = TXS_DISC;
	ASSERT(state != NULL);

	epoch = TXS_RATE_EPOCH(state->rsi->wlc->pub->corerev, txs);
	if (epoch == state->epoch) {
		/* always update antenna histogram */
		wlc_ratesel_upd_deftxant(state, state->antselid);
		txs_res = wlc_ratesel_use_txs(state, txs, sfbl, lfbl, tx_mcs, sgi, antselid, fbr);
		if (txs_res != TXS_DISC)
			wlc_ratesel_pick_rate(state, txs_res == TXS_PROBE, sgi);
	} else {
		RL_MORE(("%s: frm_e %d (id %x) [nrtx nftx acked ant] = [%u %u %d %d] nupd %u "
			"e %d\n", __FUNCTION__, epoch, txs->frameid,
			(uint8) ((txs->status.rts_tx_cnt)),
			(uint8) ((txs->status.frag_tx_cnt)),
			(txs->status.was_acked) ? 1 : 0, antselid, state->nupd, state->epoch));
	}

	if (txs_res == TXS_DISC)
		LIMINC_UINT32(state->nskip);

	if (state->txc_inv_reqd) {
		INVALIDATE_TXH_CACHE(state);
	}

	if (state->upd_pend) {
		if (RATELINKMEM_ENAB(state->rsi->pub)) {
			/* trigger ratemem update */
			ratesel_txparams_t ratesel_rates;
			uint16 flag0, flag1;
			ratesel_rates.num = 4;
			ratesel_rates.ac = 0;
			wlc_ratesel_gettxrate(state, &flag0, &ratesel_rates, &flag1);
		} else {
			wlc_ratesel_updinfo_fromtx(state, CUR_RATESPEC(state));
		}
	}
}

/**
 * all of the following should only apply to directed traffic,
 * do not call me on group-addressed frames.
 * Return TRUE to indicate that this is a blockack for probe.
 */
static int BCMFASTPATH
wlc_ratesel_use_txs_blockack(rcb_t *state, tx_status_t *txs, uint8 suc_mpdu, uint8 tot_mpdu,
	bool ba_lost, uint8 retry, uint8 fb_lim,
	uint8 tx_mcs, bool sgi, bool tx_error, uint8 antselid)
{
	uint8 cur_mcs;
	uint32 *p, cur_succ;
	uint8 alpha;
	ratespec_t cur_rspec;
	int ret_val;

	if (!tx_error) {
		ASSERT(!TX_STATUS_UNEXP_AMPDU(txs->status));
	} else {
		/* tx fifo underflow error, treat as missed block ack */
		ba_lost = TRUE;
	}

	/* Do sanity check before changing gotpkts */
	wlc_ratesel_sanitycheck_psr(state);

	/* spatial probe functionality
	 * check if this is spatial probe packet block ack:
	 * if true: update spatial statistics and return
	 * else: continue with regular block ack processing
	 */
	cur_rspec = CUR_RATESPEC(state);

	cur_mcs = WL_RSPEC_RATE_MASK & cur_rspec;
	ret_val = TXS_DISC;
	if (IS_SP_PKT(state, cur_mcs, tx_mcs)) {
		RL_SP1(("%s fid 0x%x: spatial probe rcvd: mx%02x (pri: mx%02x) "
			"[bal suc/tot_mpdu r l] = [%s %d/%d %d %d]\n",
			__FUNCTION__, txs->frameid, tx_mcs, cur_mcs,
			(ba_lost ? "yes" : "no"), suc_mpdu, tot_mpdu, retry, fb_lim));
		if (retry >= fb_lim) {
			LIMINC_UINT32(state->sp_nskip);
		} else {
			if (wlc_ratesel_upd_spstat(state, TRUE, tx_mcs, suc_mpdu, tot_mpdu, 0))
				ret_val = TXS_PROBE;
		}
		return ret_val;
	}

	/* extended spatial probe functionality
	 * check if this is extended spatial probe packet block ack:
	 * if true: update spatial statistics and return
	 * else: continue with regular block ack processing
	 */
	if (IS_EXTSP_PKT(state, cur_mcs, tx_mcs, antselid)) {
		RL_SP1(("txs_ba: ext_spatial probe rcvd: mx%02x ant %d (pri: mx%02x ant %d) "
			"[bal suc/tot_mpdu r l] = [%s %d/%d %d %d]\n",
			tx_mcs, antselid, cur_mcs, state->antselid,
			(ba_lost ? "yes" : "no"), suc_mpdu, tot_mpdu, retry, fb_lim));
		/* disregard if this was a fallback probe transmission */
		if (retry >= fb_lim) {
			LIMINC_UINT32(state->sp_nskip);
		} else {
			if (wlc_ratesel_upd_extspstat(state, TRUE, tx_mcs, antselid,
				suc_mpdu, tot_mpdu))
				ret_val = TXS_PROBE;
		}
		return ret_val;
	} /* end extended spatial probing blockack eval */

	RL_MORE(("aid %d fid 0x%x gotpkts = %d [bal suc/tot_mpdu r l e] = [%s %d %d %d %d %d] "
		"nupd %d\n",
		state->aid, txs->frameid, state->gotpkts, (ba_lost ? "yes" : "no"), suc_mpdu,
		tot_mpdu, retry, fb_lim, state->epoch, state->nupd));

	if (tx_mcs != cur_mcs) {
		RL_INFO(("%s: discard mismatch tx_mcs/cur_mcs %d %d sp_mode %d\n",
			__FUNCTION__, tx_mcs, cur_mcs, SP_MODE(state)));
		return TXS_DISC;
	}

	/* for fast drop:
	 * Reset ncfails if any success on primary or first FBR..
	 * Increment if no success on first FBR and beyond.
	 */
	if ((suc_mpdu >= 1) && (retry <= fb_lim))
		state->ncfails = 0;
	else if ((suc_mpdu == 0) && (retry >= fb_lim))
		state->ncfails ++;
	if (retry <= fb_lim) {
		if (suc_mpdu == 0) {
			state->pncfails ++;
		} else {
			state->pncfails = 0;
		}
	}

	/* for retry = fb_lim + 1, +2 etc, uses fallback rate of fallback. Discard it if failed. */
	if (retry > fb_lim)
		return ((suc_mpdu << 1) < tot_mpdu) ? TXS_REG : TXS_DISC;

	/* this packet was received at the target rate, so we
	 * can switch to the steady state rate fallback calculation
	 * of target rate - 1
	 */
	if ((!ba_lost) && (tot_mpdu > 0) && (suc_mpdu == tot_mpdu) && (retry < fb_lim)) {
		wlc_ratesel_alert_upd(state);
	}

	/* stop sending short A-MPDUs (if we received L frames) */
	if (state->mcs_short_ampdu && state->mcs_nupd >= state->rsi->max_short_ampdus) {
		state->mcs_short_ampdu = FALSE;
		state->txc_inv_reqd = TRUE;
	}

	/* Update per A-MPDU */
	if ((retry < fb_lim) || (state->psri[PSR_FBR_IDX].rspec == cur_rspec)) {
		if (sgi && SGI_PROBING(state)) {
			p = &(state->psr_sgi);
			state->sgi_state++;
		} else
			p = &(state->psri[PSR_CUR_IDX].psr);
	} else {
		p = &(state->psri[PSR_FBR_IDX].psr);
	}

	cur_succ = (suc_mpdu << RATESEL_EMA_NF) / tot_mpdu;
	alpha = state->gotpkts ? ((state->nupd <= state->rsi->ema_nupd_thres) ?
		state->rsi->psr_ema_alpha0 : state->rsi->psr_ema_alpha) : RATESEL_EMA_ALPHA_INIT;

#ifdef WL_LPC
	/* Consider only if for primary rate. for (txs_res != TXS_DISC) */
	if ((LPC_ENAB(state->rsi->wlc)) && (p == &(state->psri[PSR_CUR_IDX].psr))) {
		wlc_ratesel_upd_la(state, cur_succ, *p);
	}
#endif /* WL_LPC */

	UPDATE_MOVING_AVG(p, cur_succ, alpha);

	/* fbr is not always updated so need to timestamp it every time update it */
	if (p == &(state->psri[PSR_FBR_IDX].psr))
		state->psri[PSR_FBR_IDX].timestamp = NOW(state);

	/* update the count of updates since the last state flush, which
	 * saturates at max word val
	 */
	LIMINC_UINT32(state->mcs_nupd); /* for purpose of mcs_short_ampdu */
	LIMINC_UINT32(state->nupd);

	ASSERT(state->psri[PSR_CUR_IDX].psr <= PSR_MAX);
	ASSERT(state->psri[PSR_FBR_IDX].psr <= PSR_MAX);

	if (WL_RATE_ON()) {
		if (sgi && SGI_PROBING(state))
			RL_MORE(("%s: rid %d %d sgi %d state %d psr_sgi/cur/fbr %d %d %d\n",
				__FUNCTION__, state->rateid,
				RSPEC2RATE500K(state->psri[PSR_CUR_IDX].rspec),
				sgi, state->sgi_state, state->psr_sgi,
				state->psri[PSR_CUR_IDX].psr, state->psri[PSR_FBR_IDX].psr));
		else
			RL_MORE(("%s: rid %d %d psr_cur/fbr %d %d\n",
				__FUNCTION__, state->rateid,
				RSPEC2RATE500K(state->psri[PSR_CUR_IDX].rspec),
				state->psri[PSR_CUR_IDX].psr, state->psri[PSR_FBR_IDX].psr));
	}

	return TXS_REG;
}

/** update state upon received BA */
void BCMFASTPATH
wlc_ratesel_upd_txs_blockack(rcb_t *state, tx_status_t *txs,
	uint8 suc_mpdu, uint8 tot_mpdu, bool ba_lost, uint8 retry, uint8 fb_lim, bool tx_error,
	uint8 tx_mcs /* non-HT format */, bool sgi, uint8 antselid)
{
	uint8 epoch;
	wlc_info_t *wlc;
	int txs_res;

	/* sanity check: mcs set may not be empty */
	if ((state == NULL) || (state->active_rates_num == 0)) {
		ASSERT(0);
		return;
	}
	wlc = state->rsi->wlc;

	/* update history. extract epoch cookie from FrameID */
	epoch = TXS_RATE_EPOCH(wlc->pub->corerev, txs);
	txs_res = TXS_DISC;
	if (epoch == state->epoch) {
		/* to accommodate the discrepancy due to "wl nrate" in the debug driver */
		/* XXX: one other case is if the ratesel changed the rate output rapidly and
		* code ends up using same epoch for the previously generated mcs rate frames
		* and now that epoch represents some legacy rate
		*/
		if (!IS_MCS(CUR_RATESPEC(state))) {
			WL_RATE(("%s: blockack received for non-MCS rate.\n", __FUNCTION__));
			return;
		}

		/* always update antenna histogram */
		if (WLANTSEL_ENAB(wlc)) {
			wlc_ratesel_upd_deftxant(state, state->antselid);
		}

		txs_res = wlc_ratesel_use_txs_blockack(state, txs, suc_mpdu, tot_mpdu,
			ba_lost, retry, fb_lim, tx_mcs, sgi, tx_error, antselid);
		if (txs_res != TXS_DISC)
			wlc_ratesel_pick_rate(state, txs_res == TXS_PROBE, sgi);
	} else {
		RL_MORE(("%s: frm_e %d (id %x) [bal suc tot_mpdu r l e] = [%s %d %d %d %d %d] "
			"mcs 0x%x ant %d\n", __FUNCTION__, epoch, txs->frameid,
			ba_lost ? "yes" : "no", suc_mpdu, tot_mpdu, retry, fb_lim,
			state->epoch, tx_mcs, antselid));
	}

	if (txs_res == TXS_DISC) {
		LIMINC_UINT32(state->nskip);
	}

	if (state->txc_inv_reqd) {
		INVALIDATE_TXH_CACHE(state);
	}
	if (state->upd_pend) {
		wlc_ratesel_updinfo_fromtx(state, CUR_RATESPEC(state));
	}
}

/**
 * Following two functions are for the ampdu ucode/hw aggregation case
 */
static int
wlc_ratesel_use_txs_ampdu(rcb_t *state, uint16 frameid, uint mrt, uint mrt_succ,
	uint fbr, uint fbr_succ, bool tx_error, uint8 tx_mcs, bool sgi, uint8 antselid)
{
	uint8 cur_mcs;
	uint32 cur_succ;
	uint8 alpha;
	ratespec_t cur_rspec;
	int ret_val;

	/* tx fifo underflow error, treat as failure at mrt */
	if (tx_error)
		mrt_succ = 0;

	/* Do sanity check before changing gotpkts */
	wlc_ratesel_sanitycheck_psr(state);

	if (mrt_succ > mrt || fbr_succ > fbr) {
		/* this can happen if the receiver uses full BA state */
		RL_INFO(("%s: txs cnt: mrt/succ %d %d, fbr/succ %d %d\n",
			__FUNCTION__, mrt, mrt_succ, fbr, fbr_succ));
		if (mrt_succ > mrt) mrt_succ = mrt;
		if (fbr_succ > fbr) fbr_succ = fbr;
	}

	/* spatial probe functionality
	 * check if this is spatial probe packet block ack:
	 * if true: update spatial statistics and return
	 * else: continue with regular block ack processing
	 */
	cur_rspec = CUR_RATESPEC(state);
	cur_mcs = WL_RSPEC_RATE_MASK & cur_rspec;
	ret_val = TXS_DISC;
	if (IS_SP_PKT(state, cur_mcs, tx_mcs)) {
		RL_SP1(("%s aid %d fid 0x%04x: spatial probe rcvd: mx%x (pri: mx%x) "
			"[mrt succ fbr succ] = [%d %d %d %d]\n",
			__FUNCTION__, state->aid, frameid, tx_mcs, cur_mcs,
			mrt, mrt_succ, fbr, fbr_succ));
		if (mrt == 0) {
			LIMINC_UINT32(state->sp_nskip);
		} else {
			uint16 txbw = frameid >> 13;
			if (wlc_ratesel_upd_spstat(state, TRUE, tx_mcs, mrt_succ, mrt, txbw))
				ret_val = TXS_PROBE;
		}
		return ret_val;
	}

	/* extended spatial probe functionality
	 * check if this is extended spatial probe packet block ack:
	 * if true: update spatial statistics and return
	 * else: continue with regular block ack processing
	 */
	if (IS_EXTSP_PKT(state, cur_mcs, tx_mcs, antselid)) {
		RL_SP1(("txs_ba fid 0x%04x: ext_spatial probe rcvd: mx%02x ant %d "
			"(pri: mx%02x ant %d) [mrt succ fbr fbr_succ] = [%d %d %d %d]\n",
			frameid, tx_mcs, antselid, cur_mcs, state->antselid,
			mrt, mrt_succ, fbr, fbr_succ));
		/* disregard if this was no tx at mrt */
		if (mrt == 0) {
			LIMINC_UINT32(state->sp_nskip);
		} else {
			if (wlc_ratesel_upd_extspstat(state, TRUE, tx_mcs, antselid, mrt_succ, mrt))
				ret_val = TXS_PROBE;
		}
		return ret_val;
	} /* end extended spatial probing blockack eval */

	RL_TXS(("aid %d fid 0x%04x gotpkts %d [mrt succ fbr succ nf pnf e] = "
		"[%d %d %d %d %u %u %d] nupd %d\n", state->aid,
		frameid, state->gotpkts, mrt, mrt_succ, fbr, fbr_succ,
		state->ncfails, state->pncfails, state->epoch, state->nupd));
	WL_DPRINT((state->rsi->wlc->macdbg, state->scb,
		"aid %d fid 0x%04x gotpkts %d [mrt succ fbr succ nf e] = "
		"[%d %d %d %d %d %d] nupd %d\n",
		state->aid, frameid, state->gotpkts, mrt, mrt_succ, fbr, fbr_succ,
		state->ncfails, state->epoch, state->nupd));

	if (tx_mcs != cur_mcs) {
		RL_INFO(("%s: discard mismatch tx_mcs/cur_mcs %d %d sp_mode %d\n",
			__FUNCTION__, tx_mcs, cur_mcs, SP_MODE(state)));
		return TXS_DISC;
	}

	/* for fast drop:
	 * If succ ratio > 50% ==> good
	 *        (1) | (2) | (3)  | (4)  | (5)
	 * mrt:  good | bad |  bad |  0   |  0
	 * fbr:    x  | bad | good | good | bad
	 * ncf:   rst |  ++ |  rst |   ++ |  ++
	 */
	if (mrt_succ == 0 && fbr_succ == 0) {
		/* pkt loss */
		state->ncfails ++;
		wlc_ratesel_rfbr(state);
	}
	/* case 1 and 3 */
	if ((mrt_succ << 1) > mrt || (mrt > 0 && (fbr_succ << 1) > fbr))
		state->ncfails = 0;
	else if (fbr > 0) {
		state->ncfails ++;
	}
	/* for sgi probing:
	 *  use same 'good' def as above
	 *  if mrt is good, then reset pncfails; else ++.
	 */
	if ((mrt_succ << 1) > mrt) {
		state->pncfails = 0;
	} else {
		state->pncfails ++;
	}

	/* this packet was received at the target rate, so we
	 * can switch to the steady state rate fallback calculation
	 * of target rate - 1
	 */
	if (mrt > 0 && (mrt_succ == mrt))
		wlc_ratesel_alert_upd(state);

	/* stop sending short A-MPDUs (if we received L frames) */
	if (state->mcs_short_ampdu && state->mcs_nupd >= state->rsi->max_short_ampdus) {
		state->mcs_short_ampdu = FALSE;
		state->txc_inv_reqd = TRUE;
	}

	/* Update per A-MPDU */
	alpha = state->gotpkts ? ((state->nupd <= state->rsi->ema_nupd_thres) ?
		state->rsi->psr_ema_alpha0 : state->rsi->psr_ema_alpha) : RATESEL_EMA_ALPHA_INIT;
	if (mrt > 0) {
		cur_succ = (mrt_succ << RATESEL_EMA_NF) / mrt;
		if (sgi && SGI_PROBING(state)) {
			UPDATE_MOVING_AVG(&(state->psr_sgi), cur_succ, alpha);
			state->sgi_state++;
		} else {
			UPDATE_MOVING_AVG(&(state->psri[PSR_CUR_IDX].psr), cur_succ, alpha);
#ifdef WL_LPC
			if (LPC_ENAB(state->rsi->wlc)) {
				/* update the state */
				wlc_ratesel_upd_la(state, cur_succ, state->psri[PSR_CUR_IDX].psr);
			}
#endif /* WL_LPC */
		}
	}
	if (fbr > 0) {
		cur_succ = (fbr_succ << RATESEL_EMA_NF) / fbr;
		UPDATE_MOVING_AVG(&(state->psri[PSR_FBR_IDX].psr), cur_succ, alpha);
		/* fbr is not always updated so need to timestamp it every time update it */
		state->psri[PSR_FBR_IDX].timestamp = NOW(state);
	}

#ifdef BCMDBG
	if (state->psri[PSR_CUR_IDX].psr > PSR_MAX || state->psri[PSR_FBR_IDX].psr > PSR_MAX) {
		WL_ERROR(("%s line %d: gotpkts %d fid 0x%04x [mrt succ fbr succ e] "
			  "= [%d %d %d %d %d] nupd %d\n", __FUNCTION__, __LINE__,
			  state->gotpkts, frameid, mrt, mrt_succ, fbr, fbr_succ,
			  state->epoch, state->nupd));
	}
#endif // endif
	/* update the count of updates at primary rate since the last state flush
	 * which saturates at max word val
	 */
	if (mrt > 0) {
		LIMINC_UINT32(state->mcs_nupd); /* for purpose of mcs_short_ampdu */
		LIMINC_UINT32(state->nupd);
	}

#ifdef WLC_DTPC
	if (DTPC_ACTIVE(state)) {
		wlc_dtpc_collect_psr(state->rsi->wlc->dtpc, state->scb,
			CUR_RATESPEC(state), mrt, mrt_succ, fbr, fbr_succ,
			state->psri[PSR_CUR_IDX].psr, state->psri[PSR_FBR_IDX].psr);
	}
#endif /* WLC_DTPC */

	ASSERT(state->psri[PSR_CUR_IDX].psr <= PSR_MAX);
	ASSERT(state->psri[PSR_FBR_IDX].psr <= PSR_MAX);

	RL_TXS(("%s: rid %d %d psr_cur/fbr %d %d\n",
		__FUNCTION__, state->rateid, RSPEC2RATE500K(state->psri[PSR_CUR_IDX].rspec),
		state->psri[PSR_CUR_IDX].psr, state->psri[PSR_FBR_IDX].psr));
	WL_DPRINT((state->rsi->wlc->macdbg, state->scb,
		"%s: rid %d %d psr_cur/fbr %d %d\n",
		__FUNCTION__, state->rateid, RSPEC2RATE500K(state->psri[PSR_CUR_IDX].rspec),
		state->psri[PSR_CUR_IDX].psr, state->psri[PSR_FBR_IDX].psr));

	return TXS_REG;
}

/**
 * The case that (mrt+fbr) == 0 is handled as RTS transmission failure.
 */
void BCMFASTPATH
wlc_ratesel_upd_txs_ampdu(rcb_t *state, rcb_itxs_t *rcb_itxs,
	ratesel_txs_t *rs_txs, tx_status_t *txs, uint16 flags)
{
	uint8 epoch;
	int txs_res;
	bool txs_mu = FALSE;
	uint8 txs_mutype = TX_STATUS_MUTP_VHTMU; /* dflt to VHTMM */
	bool tx_error;

	uint16 frameid = rs_txs->frameid;
	uint mrt = 0, mrt_succ = 0, fbr = 0, fbr_succ = 0;
	uint8 tx_mcs = rs_txs->txrspec[0] & WL_RSPEC_RATE_MASK;
	bool sgi;
	uint8 antselid = rs_txs->antselid;
	wlc_info_t *wlc;
#ifdef WLC_DTPC
	uint8 dtpc_status = 0;
	BCM_REFERENCE(dtpc_status);
#endif // endif

	wlc = state->rsi->wlc;
	BCM_REFERENCE(txs);

	if (flags & RATESEL_UPDFLAG_NORTSELUPD) {
#if defined(WL_MU_TX)
		rcb_itxs->txepoch = (uint16) RCB_TXEPOCH_INVALID;
#endif // endif
		return;
	}

	tx_error = (flags & RATESEL_UPDFLAG_TXERR) ? TRUE : FALSE;
	txs_mu = (txs->status.s5 & TX_STATUS64_MUTX) ? TRUE : FALSE;
	txs_mutype = TX_STATUS_MUTYP(wlc->pub->corerev, txs->status.s5);

	wlc_ratesel_updinfo_maggtxs(state, txs, rs_txs, RTSTAT_UPD_DL);
	if (!txs_mu) {
		mrt = rs_txs->tx_cnt[0];
		mrt_succ = rs_txs->txsucc_cnt[0];
		fbr = rs_txs->tx_cnt[1];
		fbr_succ = rs_txs->txsucc_cnt[1];
#if defined(WL_MU_TX)
		if (rcb_itxs->txepoch != (uint16) RCB_TXEPOCH_INVALID) {
			rcb_itxs->txepoch = (uint16) RCB_TXEPOCH_INVALID;
		}
#endif /* defined(WL_MU_TX) */
	} else {
		/* Tx as MU cases */
		if (txs_mutype == TX_STATUS_MUTP_VHTMU) {
			return;
		}
#if defined(WL_MU_TX)
		else if (txs_mutype == TX_STATUS_MUTP_HEOM) {
			int i;
			/* last tx status is also used to update rcb_itxs */
			wlc_ratesel_upd_itxs_block(state, rcb_itxs, rs_txs, txs);

			mrt = rcb_itxs->tx_cnt[0];
			mrt_succ = rcb_itxs->txsucc_cnt[0];

			fbr = fbr_succ = 0;
			for (i = 1; i < RATESEL_MFBR_NUM; i++) {
				fbr += rcb_itxs->tx_cnt[i];
				fbr_succ += rcb_itxs->txsucc_cnt[i];
			}
			rcb_itxs->txepoch = (uint16) RCB_TXEPOCH_INVALID;
			RL_INFO(("%s: aid %d frameid %x mrt/succ/fbr/succ %d %d %d %d\n",
				__FUNCTION__, state->aid, frameid, mrt, mrt_succ, fbr, fbr_succ));
		} else {
			return;
		}
#endif /* defined(WL_MU_TX) */
	}

	/* sanity check: mcs set may not be empty */
	if ((state == NULL) || (state->active_rates_num == 0)) {
		ASSERT(0);
		return;
	}

	/* Convert MCS in ratespec to non-HT format if needed */
	if (RSPEC_ISHT(rs_txs->txrspec[0])) {
		tx_mcs = wlc_rate_ht_to_nonht(tx_mcs);
	}

	if (mrt + fbr == 0) {
		/* fake one transmission at main rate to obtain the expected update */
		mrt = 1;
		ASSERT(mrt_succ + fbr_succ == 0);
	}

	/* update history. extract epoch cookie from FrameID */
	epoch = TXS_RATE_EPOCH(wlc->pub->corerev, txs);
	txs_res = TXS_DISC;
	if (RSPEC_ISHE(rs_txs->txrspec[0])) {
		sgi = (RSPEC_HE_LTF_GI(rs_txs->txrspec[0]) <= WL_RSPEC_HE_2x_LTF_GI_0_8us);
	} else {
		sgi = (rs_txs->txrspec[0] & WL_RSPEC_SGI) ? TRUE: FALSE;
	}

#ifdef WLC_DTPC
	if (DTPC_ACTIVE(state)) {
		bool go_prb = (!(sgi && SGI_PROBING(state)) && !SP_PROBING(state));
		uint8 upper_rateid = 0xFF;
		ratespec_t upper_rspec;
		bool highest_mcs = !(wlc_ratesel_next_rate(state, +1, &upper_rateid, &upper_rspec));
		dtpc_status = wlc_dtpc_check_txs(wlc->dtpc, state->scb, txs, rs_txs->txrspec[0],
			RSPEC2RATE500K(state->psri[PSR_CUR_IDX].rspec),
			state->psri[PSR_CUR_IDX].psr, RSPEC2RATE500K(state->psri[PSR_DN_IDX].rspec),
			state->psri[PSR_DN_IDX].psr, state->psri[PSR_UP_IDX].psr, state->epoch,
			state->nupd, mrt, mrt_succ, go_prb, highest_mcs);
		if (dtpc_status & DTPC_TXC_INV) {
			state->txc_inv_reqd = TRUE;
		}
		if (dtpc_status & DTPC_UPD_PSR) {
			uint32 final_psr = wlc_dtpc_get_psr(wlc->dtpc, state->scb);
			if (final_psr != PSR_UNINIT) {
			state->psri[PSR_CUR_IDX].timestamp = NOW(state);
			state->psri[PSR_CUR_IDX].psr = final_psr;
			}
		}
		txs_res = TXS_REG;
	}
	if (dtpc_status == DTPC_INACTIVE) {
#endif /* WLC_DTPC */

	if (epoch == state->epoch) {
		/* to accommodate the discrepancy due to "wl nrate" in the debug driver */
		if (!IS_MCS(CUR_RATESPEC(state))) {
			WL_ERROR(("%s: epoch meant mcs rate when packet is queued, not at txs\n",
				__FUNCTION__));
			return;
		}

		/* always update antenna histogram */
		if (WLANTSEL_ENAB(wlc)) {
			wlc_ratesel_upd_deftxant(state, state->antselid);
		}

		frameid = frameid & 0x1fff;
		frameid |= (RSPEC2BW(rs_txs->txrspec[0]) << 13);
		txs_res = wlc_ratesel_use_txs_ampdu(state, frameid, mrt, mrt_succ, fbr, fbr_succ,
			tx_error, tx_mcs, sgi, antselid);
		if (txs_res != TXS_DISC) {
			wlc_ratesel_pick_rate(state, txs_res == TXS_PROBE, sgi);
		}
	} else {
		RL_MORE(("%s: frm_e %d (fid %x) [mrt succ fbr succ e] = [%d %d %d %d %d] "
			"mcs 0x%02x ant %d e %d\n", __FUNCTION__, epoch, frameid, mrt,
			mrt_succ, fbr, fbr_succ, state->epoch, tx_mcs, antselid, state->epoch));
	}
#ifdef WLC_DTPC
	}
#endif // endif

	if (txs_res == TXS_DISC) {
		LIMINC_UINT32(state->nskip);
	}

	if (state->txc_inv_reqd) {
		INVALIDATE_TXH_CACHE(state);
	}

	if (state->upd_pend) {
		if (RATELINKMEM_ENAB(state->rsi->pub)) {
			/* trigger ratemem update */
			ratesel_txparams_t ratesel_rates;
			uint16 flag0, flag1;
			ratesel_rates.num = 4;
			ratesel_rates.ac = 0;
			wlc_ratesel_gettxrate(state, &flag0, &ratesel_rates, &flag1);
		} else {
			wlc_ratesel_updinfo_fromtx(state, CUR_RATESPEC(state));
		}
	}
}

#if defined(WL_MU_TX)
static void
wlc_ratesel_upd_itxs_block(rcb_t *state, rcb_itxs_t *rcb_itxs,
	ratesel_txs_t *rs_txs, tx_status_t *txs)
{
	int i;
	uint8 rtidx;
	uint16 frameid = rs_txs->frameid;

	/* sanity check: same frameid as before */
	if (rcb_itxs->txepoch != frameid &&
		rcb_itxs->txepoch != (uint16) RCB_TXEPOCH_INVALID) {
		WL_ERROR(("%s: ratesel txepoch mismatch. itx.txepoch=%X txs.frameid=%X txs\n"
			"  %04X %04X | %04X %04X | %08X %08X || %08X %08X | %08X\n",
			__FUNCTION__, rcb_itxs->txepoch, frameid,
			txs->status.raw_bits, txs->frameid, txs->sequence, txs->phyerr,
			txs->status.s3, txs->status.s4, txs->status.s5,
			txs->status.ack_map1, txs->status.ack_map2));
		/* ASSERT (0); */
	}

	if (rcb_itxs->txepoch == (uint16) RCB_TXEPOCH_INVALID ||
		rcb_itxs->txepoch != frameid) {
		rcb_itxs->txepoch = frameid;
		for (i = 0; i < RATESEL_MFBR_NUM; i++) {
			rcb_itxs->tx_cnt[i] = 0;
			rcb_itxs->txsucc_cnt[i] = 0;
		}
	}

	rtidx = TX_STATUS128_HEOM_RTIDX(txs->status.s4);

	ASSERT(rtidx < RATESEL_MFBR_NUM);

	/* update rcb using intermediate txs */
	rcb_itxs->tx_cnt[rtidx] += rs_txs->tx_cnt[rtidx];
	rcb_itxs->txsucc_cnt[rtidx] += rs_txs->txsucc_cnt[rtidx];
	RL_INFO(("%s: rtidx %d tx_cnt %d txsucc_cnt %d frameid %x\n",
		__FUNCTION__, rtidx, rcb_itxs->tx_cnt[rtidx],
		rcb_itxs->txsucc_cnt[rtidx], frameid));
	WL_DPRINT((state->rsi->wlc->macdbg, state->scb,
		"%s: rtidx %d tx_cnt %d txsucc_cnt %d frameid %x\n",
		__FUNCTION__, rtidx, rcb_itxs->tx_cnt[rtidx],
		rcb_itxs->txsucc_cnt[rtidx], frameid));
}

void BCMFASTPATH
wlc_ratesel_upd_itxs(rcb_t *state, rcb_itxs_t *rcb_itxs,
	ratesel_txs_t *rs_txs, tx_status_t *txs, uint16 flags)
{
	wlc_info_t *wlc;
	bool txs_mu;
	uint8 txs_mutype;

	txs_mu = FALSE;
	txs_mutype = TX_STATUS_MUTP_VHTMU; /* dflt to VHTMM */
	wlc = state->rsi->wlc;

	txs_mu = (txs->status.s5 & TX_STATUS64_MUTX) ? TRUE : FALSE;
	txs_mutype = TX_STATUS_MUTYP(wlc->pub->corerev, txs->status.s5);

	if (txs_mu) {
		wlc_ratesel_updinfo_mutxs(state, txs, rs_txs, RTSTAT_UPD_DL);
		if (txs_mutype != TX_STATUS_MUTP_HEOM) {
			return;
		}
	} else {
		return;
	}

	wlc_ratesel_upd_itxs_block(state, rcb_itxs, rs_txs, txs);

}

#if defined(WL11AX)
void BCMFASTPATH
wlc_ratesel_upd_txs_trig(rcb_t *state, ratesel_tgtxs_t *tgtxs, tx_status_t *txs)
{
	int k;
	int idx = tgtxs->rtidx;
	uint32 curd, curf, curpsr;
	ASSERT(tgtxs->rtidx < RATESEL_MFBR_NUM);

	if (idx == 0 && tgtxs->txrspec != state->ulrt.txrspec[0]) {
		curd = state->ulrt.gdelim[0];
		curf = state->ulrt.gfcs[0];
		curpsr = (curd == 0) ? 0 : (curf << RATESEL_EMA_NF) / curd;

		WL_RATE(("%s: aid %3d rate_prev/cur 0x%02x -> 0x%02x "
			 "nf %2d gdelim0 %3d gfcs0 %3d PSR %d (%02d) nupd %d\n",
			 __FUNCTION__, state->aid,
			 WL_RSPEC_RATE_MASK & state->ulrt.txrspec[0],
			 WL_RSPEC_RATE_MASK & tgtxs->txrspec,
			 state->ulrt.nfail, curd, curf, curpsr,
			 curpsr * 100 / (1 << RATESEL_EMA_NF),
			 state->ulrt.tot_nupd));

		for (k = 0; k < RATESEL_MFBR_NUM; k++) {
			curd = state->ulrt.gdelim[k];
			curf = state->ulrt.gfcs[k];
			curpsr = (curd == 0) ? 0 : (curf << RATESEL_EMA_NF) / curd;
			RL_CHG(("rt%d [0x%02x %d / %d p %4d] n %d\n",
				k, WL_RSPEC_RATE_MASK & state->ulrt.txrspec[k],
				curd, curf, curpsr, state->ulrt.nupd[k]));
		}
		BCM_REFERENCE(curd);
		BCM_REFERENCE(curf);
		BCM_REFERENCE(curpsr);

		bzero((char *)&state->ulrt, sizeof(rcb_ulrt_t));
	}
	if (tgtxs->gdelim == 0) {
		state->ulrt.nfail ++;
	} else {
		state->ulrt.gdelim[idx] = LIMADD(state->ulrt.gdelim[idx],
				UINT32_MAX, tgtxs->gdelim);
		state->ulrt.gfcs[idx] = LIMADD(state->ulrt.gfcs[idx], UINT32_MAX, tgtxs->gfcs);
	}
	state->ulrt.state = TGTXS_TXCNT(txs->status.s8);
	state->ulrt.txrspec[idx] = tgtxs->txrspec;
	state->ulrt.ruidx = tgtxs->ruidx;
	state->ulrt.trssi = D11TRIGUI_TRSSI(TGTXS_USRINFOP3(txs->status.ack_map2));
	LIMINC_UINT32(state->ulrt.nupd[idx]);
	LIMINC_UINT32(state->ulrt.tot_nupd);

	RL_ULRX(("%s: txs aid %3d rtidx %d txcnt %d rspec 0x%06x gdelim %d gfcs %d "
		"nupd_cur/tot %d %d\n",
		__FUNCTION__, state->aid, tgtxs->rtidx, tgtxs->txcnt,
		tgtxs->txrspec, tgtxs->gdelim, tgtxs->gfcs,
		state->ulrt.nupd[idx], state->ulrt.tot_nupd));
}

ratespec_t
wlc_ratesel_get_ulrt_rspec(rcb_t *state, uint8 rtidx)
{
	return state->ulrt.txrspec[rtidx];
}

uint8
wlc_ratesel_get_ulrt_trssi(rcb_t *state)
{
	return state->ulrt.trssi;
}
#endif /* defined(WL11AX) */
#endif /* defined(WL_MU_TX) */

static void
wlc_ratesel_upd_deftxant(rcb_t *state, uint8 txant_new_idx)
{
	uint8 idx;
	int id;
	uint8 txant_old_idx;
	uint8 new_max_idx;
	uint8 new_cnt;
	ratesel_info_t *rsi = state->rsi;

	if (rsi->txant_stats_num <= 1)
		return;

	state->txs_cnt += 1;
	if (state->txs_cnt <= TXANTHIST_NMOD)
		return;

	state->txs_cnt = 0;

	/* update txant history */
	idx = rsi->txant_hist_id;
	rsi->txant_hist[idx] = (txant_new_idx | 0x80);
	rsi->txant_hist_id = MODINC(rsi->txant_hist_id, MAXTXANTRECS);

	/* update txant history totals */
	rsi->txant_stats[(txant_new_idx & 0x07)] += 1;
	id = rsi->txant_hist_id - TXANTHIST_M - 1;
	if (id < 0)
		id += MAXTXANTRECS;
	txant_old_idx = rsi->txant_hist[id];
	if ((txant_old_idx & 0x80) != 0) {
		txant_old_idx &= ~0x80;
		rsi->txant_stats[(txant_old_idx & 0x07)] -= 1;
	}

	/* find best configuration (maximum) */
	new_cnt = rsi->txant_stats[(txant_new_idx & 0x07)];
	new_max_idx = rsi->txant_max_idx;
	if (new_cnt >= TXANTHIST_K) new_max_idx = (txant_new_idx & 0x07);

	/* check if default config has to be changed */
	if (rsi->txant_max_idx != new_max_idx) {
		/* update default antenna config and call phy update function */
		rsi->txant_max_idx = new_max_idx;
		if (rsi->rxant_sel)
			wlc_ratesel_upd_rxantprb(rsi, RXANT_UPD_REASON_TXCHG);
		else {
			wlc_antsel_upd_dflt(rsi->wlc->asi, new_max_idx);
			WL_RATE(("%s: new default txantid %d\n", __FUNCTION__, rsi->txant_max_idx));
		}

	} else if (rsi->rxant_sel) {
		rsi->rxant_txcnt ++;
		wlc_ratesel_upd_rxantprb(rsi, RXANT_UPD_REASON_TXUPD);
	}
}

/**
 * Run the rxant probe state machine:
 * basically to determine whether to start/stop the probing,
 * and set the default rxant accordingly.
 * <reason_code> represents three reasons to call this function:
 * = 0, the dominating txant changes
 * = 1, tx cnt increments (actually every X tx)
 * = 2, rx cnt increments
 */
void BCMFASTPATH
wlc_ratesel_upd_rxantprb(ratesel_info_t *rsi, int reason_code)
{
	bool not_probing = TRUE;

	ASSERT(rsi->rxant_sel);

	not_probing = rsi->rxant_id == rsi->rxant_probe_id;

	if (not_probing) {
		/* Do we need to start probing? */
		if (rsi->rxant_id == rsi->txant_max_idx)
			return;

		switch (reason_code) {
		case RXANT_UPD_REASON_TXCHG:
			/* reset threshold and start immediately */
			rsi->rxant_pon_tt = RXANT_PON_TXCNT_MIN;
			rsi->rxant_pon_tr = RXANT_PON_RXCNT_MIN;
			wlc_ratesel_rxant_pon(rsi);
			RL_RXA1(("%s: reset pon threshold: tx/rx %d %d\n",
				__FUNCTION__, rsi->rxant_pon_tt, rsi->rxant_pon_tr));
			break;
		case RXANT_UPD_REASON_TXUPD:
			if (rsi->rxant_txcnt >= rsi->rxant_pon_tt)
				wlc_ratesel_rxant_pon(rsi);
			break;
		case RXANT_UPD_REASON_RXUPD:
			if (rsi->rxant_rxcnt >= rsi->rxant_pon_tr)
				wlc_ratesel_rxant_pon(rsi);
			break;
		default:
			ASSERT(0);
		}
	} else {
		/* Do we need to stop probing? */
		switch (reason_code) {
		case RXANT_UPD_REASON_TXCHG:
			/* Hey, txant switches too fast. Let's skip this one */
			RL_RXA1(("%s: dominating txant switches while rx in probing.\n",
				__FUNCTION__));
			break;
		case RXANT_UPD_REASON_TXUPD:
			if (rsi->rxant_txcnt >= rsi->rxant_poff_tt)
				wlc_ratesel_rxant_poff(rsi, TRUE);
			break;
		case RXANT_UPD_REASON_RXUPD:
			if (rsi->rxant_rxcnt >= rsi->rxant_poff_tr)
				wlc_ratesel_rxant_poff(rsi, TRUE);
			else if (rsi->rxant_rxcnt >= RXANT_POFF_RXCNT_M)
				wlc_ratesel_rxant_poff(rsi, FALSE);
			break;
		default:
			ASSERT(0);
		}
	}
}

static void BCMFASTPATH
wlc_ratesel_rxant_pon(ratesel_info_t *rsi)
{
	rsi->rxant_probe_id = rsi->txant_max_idx;

	RL_RXA0(("%s : cnt_tx/rx %d %d start to probe rxant id %d\n", __FUNCTION__,
		rsi->rxant_txcnt, rsi->rxant_rxcnt, rsi->rxant_probe_id));

	rsi->rxant_txcnt = 0;
	rsi->rxant_rxcnt = 0;
	rsi->rxant_probe_stats = 0;
	wlc_antsel_upd_dflt(rsi->wlc->asi, rsi->rxant_probe_id);
}

/**
 * <force_off> is set to FALSE when doing rxcnt periodic check
 *             is set to TRUE when either txcnt or rxcnt reach threshold.
 * <force_off> == FALSE
 *             abort probing if probe_stats < stats (primary) by 10%
 *             == TRUE
 *             abort probing regardlessly
 *             if probe_stats >= stats (primary), switch
 */
static void BCMFASTPATH
wlc_ratesel_rxant_poff(ratesel_info_t *rsi, bool force_off)
{
	bool probe_done, probe_succ = FALSE;
	probe_done = force_off;

	if (force_off)
		/* final decision time */
		probe_succ = (rsi->rxant_probe_stats >= rsi->rxant_stats);
	else
		/* early abort probing */
		probe_done = ((rsi->rxant_probe_stats << 5) < rsi->rxant_stats * 29);

	if (!probe_done) {
		RL_RXA0(("%s: cnt_tx/rx %d %d stats_prb/pri %d %d. Probe %d continues.\n",
			__FUNCTION__, rsi->rxant_txcnt, rsi->rxant_rxcnt, rsi->rxant_probe_stats,
			rsi->rxant_stats, rsi->rxant_probe_id));
		return;
	}

	if (probe_succ) {
		/* use the new one */
		RL_RXA0(("%s: cnt_tx/rx %d %d stats_prb/pri %d %d. Probe %d succ. Leave %d\n",
			__FUNCTION__, rsi->rxant_txcnt, rsi->rxant_rxcnt, rsi->rxant_probe_stats,
			rsi->rxant_stats, rsi->rxant_probe_id, rsi->rxant_id));
		rsi->rxant_id = rsi->rxant_probe_id;
		rsi->rxant_stats = 0;

	} else {
		/* go back to our old one: but don't clear the old stats */
		RL_RXA0(("%s: cnt_tx/rx %d %d stats_prb/pri %d < %d. Probe %d fails. Stay@ %d\n",
			__FUNCTION__, rsi->rxant_txcnt, rsi->rxant_rxcnt, rsi->rxant_probe_stats,
			rsi->rxant_stats, rsi->rxant_probe_id, rsi->rxant_id));
		rsi->rxant_probe_id = rsi->rxant_id;
		wlc_antsel_upd_dflt(rsi->wlc->asi, rsi->rxant_id);
		rsi->rxant_pon_tt = MIN(rsi->rxant_pon_tt*2, RXANT_PON_TXCNT_MAX);
		rsi->rxant_pon_tr = MIN(rsi->rxant_pon_tr*2, RXANT_PON_RXCNT_MAX);
	}

	rsi->rxant_rxcnt = 0;
	rsi->rxant_txcnt = 0;
}

/** collect statistics for rx antenna selection */
void BCMFASTPATH
wlc_ratesel_upd_rxstats(ratesel_info_t *rsi, ratespec_t rx_rspec, uint16 rxstatus2)
{
	uint32 rxrate, *pstats;
	bool probe;
	uint8 rxant_id;

	if (!rsi->rxant_sel)
		return;

	if (D11REV_GE(rsi->pub->corerev, 128))
		rxant_id = (rxstatus2 >> RXS_RXANT_SHIFT_GE80) & RXS_RXANT_MASK;
	else
		rxant_id = (rxstatus2 >> RXS_RXANT_SHIFT_LT80) & RXS_RXANT_MASK;

	probe = (rsi->rxant_id != rsi->rxant_probe_id);

	/* filter out frames received with unmatched ant due to queuing delay */
	if ((probe && rxant_id != rsi->rxant_probe_id) || (!probe && rxant_id != rsi->rxant_id))
		return;

	pstats = probe ? &rsi->rxant_probe_stats : &rsi->rxant_stats;
	rxrate = RSPEC2KBPS(rx_rspec) / 500;

	/* collect history info */
	if (rsi->rxant_rate_en) {
		ASSERT(rxant_id < ANT_SELCFG_MAX_NUM);
		if (rsi->rxant_rate_cnt[rxant_id] == 0)
			rsi->rxant_rate[rxant_id] = rxrate;
		else
			UPDATE_MOVING_AVG(&rsi->rxant_rate[rxant_id], rxrate, rsi->psr_ema_alpha);
	}

	if (rsi->rxant_rxcnt == 0)
		*pstats = rxrate;
	else {
		uint32 old = *pstats;
		UPDATE_MOVING_AVG(pstats, rxrate, rsi->psr_ema_alpha);
		if (*pstats == old && rxrate > old)
			(*pstats) ++;
	}
	LIMINC_UINT32(rsi->rxant_rxcnt);

	if (probe)
		RL_RXA1(("Upd_rxstats: prb rxant %d rxcnt %d rspec 0x%x rate %d stats %d "
			"rxant %d\n", rsi->rxant_probe_id, rsi->rxant_rxcnt,
			rx_rspec, rxrate, rsi->rxant_probe_stats, rxant_id));
	else
		RL_RXA1(("Upd_rxstats: cur rxant %d rxcnt %d rspec 0x%x rate %d stats %d "
			"rxant %d\n", rsi->rxant_id, rsi->rxant_rxcnt,
			rx_rspec, rxrate, rsi->rxant_stats, rxant_id));

	wlc_ratesel_upd_rxantprb(rsi, RXANT_UPD_REASON_RXUPD);
}

/**
 * Translate VHT rspec to HT or HE rspec based on state.
 * Incoming CCK/OFDM rates are not changed, other incoming rates (HT/HE) are not allowed.
 *
 * rspec is to be filled based on values present in known_rspec[],
 * and shall have CCK, OFDM & VHT  RSPECs (if WL11AC is defined).
 *
 * For cases where RSPEC has MCS (i.e VHT_RSPECs at this point), we would need to suitably convert
 * these VHT_RSPECs to corresponding HT / HE variants based on state->he & state->vht.
 */
static ratespec_t
wlc_ratesel_convertrspec(rcb_t *state, ratespec_t rspec)
{
	ratespec_t new_rspec = rspec;
#ifdef WL11AC
	if (IS_MCS(rspec)) {
		int mcs, nss;

		ASSERT(RSPEC_ISVHT(rspec));

		mcs = rspec & WL_RSPEC_VHT_MCS_MASK;
		nss = (rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT;
#ifdef WL11AX
		if (state->he) {
			/* convert the rspec from vht format to he format */
			new_rspec = HE_RSPEC(mcs, nss);
			if (RSPEC_ISSGI(rspec)) {
				new_rspec |= (HE_GI_TO_RSPEC(WL_RSPEC_HE_2x_LTF_GI_0_8us));
			} else {
				new_rspec |= (HE_GI_TO_RSPEC(WL_RSPEC_HE_2x_LTF_GI_1_6us));
			}
			new_rspec |= (rspec & WL_RSPEC_BW_MASK);
		} else
#endif /* WL11AX */
		if (!state->vht) {
			/* convert the rspec from vht format to ht format */
			nss--;
			new_rspec = WL_RSPEC_ENCODE_HT | (rspec & (WL_RSPEC_SGI|WL_RSPEC_BW_MASK));
#ifdef WL11N_256QAM
			if ((mcs > 7) && (nss < MCS_STREAM_MAX_3)) {
				new_rspec |= mcs256q_map[nss][mcs - 8];
			} else
#endif // endif
			{
				new_rspec |= (8 * nss + mcs);
			}

		}
	}
#endif /* WL11AC */

	return new_rspec;
}

ratespec_t
wlc_ratesel_getcurspec(rcb_t *state)
{
	ratespec_t cur_rspec = CUR_RATESPEC(state);
#ifdef WL11AC
	cur_rspec = wlc_ratesel_convertrspec(state, cur_rspec);
#endif // endif
	return cur_rspec;
}

/**
 * Each remote party may require a different combination of primary and fallback transmit rates.
 * Selects transmit rate given per-scb state.
 * Note, for rev GE128 frameid is really ratesel_info and flags is not used.
 */
bool BCMFASTPATH
wlc_ratesel_gettxrate(rcb_t *state, uint16 *frameid,
	ratesel_txparams_t *ratesel_rates, /* a set of primary and optionally fallback rates */
	uint16 *flags)
{
	ratesel_info_t *rsi;
	uint8 rateid, fbrateid;
	bool rate_probe = FALSE;
#ifdef D11AC_TXD
	uint8 tmp;
#endif /* D11AC_TXD */
	uint32 rspec_sgi;

	/* clear all flags */
	*flags = 0;

	wlc_ratesel_alert(state, (ratesel_rates->num > 2) ? TRUE : FALSE);
	rsi = state->rsi;

	if (RATELINKMEM_ENAB(rsi->pub)) {
		if (!state->upd_pend) {
			return FALSE;
		}
	}

	/* get initial values for antenna selection, prim might be overridden for probe */
	ratesel_rates->antselid[0] = state->antselid;
	ratesel_rates->antselid[1] = state->antselid;

	rspec_sgi = (state->sgi_state >= SGI_PROBING) ? WL_RSPEC_SGI : 0;

	rateid = state->rateid;

	/* sanity check */
	if (rateid >= state->active_rates_num) {
		rateid = state->active_rates_num - 1;
		state->rateid = rateid;
	}

	ratesel_rates->rspec[0] = RATESPEC_OF_I(state, rateid);

	/* fill in rates & antennas for the case of probes */
	if ((SP_MODE(state) == SP_NORMAL) && ((state->mcs_sp_flgs & SPFLGS_EN_MASK) == 1)) {

		rateid = state->mcs_sp_id; /* select spatial probe rate */
		rate_probe = TRUE; /* flag probes */
		ASSERT(rspec_sgi == 0 || state->sgi_state == SGI_ENABLED);
		rspec_sgi = 0; /* don't probe with SGI in general */

		RL_SP1(("%s: aid %d probe frameid 0x%04x\n", __FUNCTION__, state->aid, *frameid));

	} else if ((SP_MODE(state) == SP_EXT) &&
		((state->extsp_flgs & EXTSPFLGS_EN_MASK) == 1)) {
		if (state->extsp_ixtoggle == I_PROBE) {
			ratesel_rates->antselid[0] = state->antselid_extsp_ipr;
		} else {
			rateid = state->extsp_xpr_id;
			ratesel_rates->antselid[0] = state->antselid_extsp_xpr;
			ASSERT(rspec_sgi == 0 || state->sgi_state == SGI_ENABLED);
			rspec_sgi = 0; /* don't x-probe with SGI */
		}

		rate_probe = TRUE; /* flag probes */
	}
	if (rateid != state->rateid && rateid >= state->active_rates_num) {
		rateid = state->active_rates_num - 1;
		state->rateid = rateid;
	}

	/* drop a cookie containing the current rate epoch in the appropriate portion of
	 * the FrameID for revs < 128, and in rate_entry for rev >= 128
	 */
	if (RATELINKMEM_ENAB(rsi->pub)) {
		*frameid = ((state->epoch << D11_REV128_RATE_EPOCH_SHIFT) &
			D11_REV128_RATE_EPOCH_MASK);
		if (rate_probe) {
			*frameid |= D11_REV128_RATE_PROBE_FLAG;
		}
	} else {
		/* clear ratesel bits in frameid, but preserve other parts (seq, fifo) */
		*frameid &= ~(TXFID_RATE_MASK | TXFID_RATE_PROBE_MASK);
		*frameid |= ((state->epoch << TXFID_RATE_SHIFT) & TXFID_RATE_MASK);

		if (rate_probe) {
			*frameid |= TXFID_RATE_PROBE_MASK;
		}
		/* mark frames that should go out as short A-MPDUs */
		if (state->mcs_short_ampdu) {
			*flags |= RATESEL_VRATE_PROBE;
		}
	}

	/* set primary rate */
	if (rateid < state->mcs_baseid || !(state->sgi_bwen & (1 << state->bw[rateid])))
		rspec_sgi = 0;

	ratesel_rates->rspec[0] = RATESPEC_OF_I(state, rateid) | rspec_sgi;

	/* fallback based on primary rate */
	fbrateid = wlc_ratesel_getfbrateid(state, state->rateid);
	ratesel_rates->rspec[1] = RATESPEC_OF_I(state, fbrateid);

	if (rspec_sgi && state->sgi_state == SGI_ENABLED &&
	    fbrateid >= state->mcs_baseid && (state->sgi_bwen & (1 << state->bw[fbrateid])))
		ratesel_rates->rspec[1] |= rspec_sgi;
	if (SP_MODE(state) == SP_EXT)
		RL_MORE(("%s: aid %d e %d rspec_pri/fbr 0x%08x 0x%08x rate_pri/fbr %u %u "
			"ant_pri/fbr %d %d sgi %d fid 0x%04x\n",
			__FUNCTION__, state->aid, state->epoch,
			ratesel_rates->rspec[0], ratesel_rates->rspec[1],
			RSPEC2RATE500K(ratesel_rates->rspec[0]),
			RSPEC2RATE500K(ratesel_rates->rspec[1]),
			ratesel_rates->antselid[0], ratesel_rates->antselid[1],
			state->sgi_state, *frameid));
	else
		RL_MORE(("%s: aid %d e %d rspec_pri/fbr 0x%08x 0x%08x rate_pri/fbr %u %u "
			"sgi %d fid 0x%04x\n",
			__FUNCTION__, state->aid, state->epoch,
			ratesel_rates->rspec[0], ratesel_rates->rspec[1],
			RSPEC2RATE500K(ratesel_rates->rspec[0]),
			RSPEC2RATE500K(ratesel_rates->rspec[1]),
			state->sgi_state, *frameid));

#ifdef D11AC_TXD
	tmp = ratesel_rates->num;
#endif // endif

#ifndef FALLBACK_RATE_EQUALS_PRI_RATE
	if ((ratesel_rates->rspec[0] == ratesel_rates->rspec[1]) &&
	   (ratesel_rates->antselid[0] == ratesel_rates->antselid[1]))
		ratesel_rates->num = 1;
	else
		ratesel_rates->num = 2;
#endif /* FALLBACK_RATE_EQUALS_PRI_RATE */

#ifdef D11AC_TXD
	/* obtain another two fallback rates only when being asked */
	if (tmp > 2) {
		for (rateid = fbrateid; ratesel_rates->num >= 2 && ratesel_rates->num < 4;
		     ratesel_rates->num++) {
			/*
			 * Get next fallback rate from lookup table:
			 * which is next rate down.
			 * Use the same antenna for all fallback rates.
			 */
			fbrateid = state->dnrid[fbrateid];
			if (fbrateid == rateid)
				break;
			if (ratesel_rates->num == 3 &&
			    (rsi->rfbr || (state->gotpkts & GOTPKTS_RFBR) == 0)) {
				/* reliable fallback: set the last rate to be lowest */
				fbrateid = 0;
				rspec_sgi = 0;
			}
			ratesel_rates->rspec[ratesel_rates->num] = RATESPEC_OF_I(state, fbrateid);
			ratesel_rates->antselid[ratesel_rates->num] = ratesel_rates->antselid[1];
			rateid = fbrateid;
			if (rspec_sgi && state->sgi_state == SGI_ENABLED &&
			    fbrateid >= state->mcs_baseid &&
			    (state->sgi_bwen & (1 << state->bw[fbrateid])))
				ratesel_rates->rspec[ratesel_rates->num] |= rspec_sgi;
		}
	}
#endif /* D11AC_TXD */

#ifdef WL11AC
	for (rateid = 0; rateid < ratesel_rates->num; rateid++) {
		ratesel_rates->rspec[rateid] =
				wlc_ratesel_convertrspec(state, ratesel_rates->rspec[rateid]);
		RL_MORE(("aid %d rate %d: rspec 0x%08x rate %d\n",
			state->aid, rateid, ratesel_rates->rspec[rateid],
			RSPEC2RATE500K(ratesel_rates->rspec[rateid])));
	}
	if (ratesel_rates->num == 1) {
		/* copy to the first fallback rate anyways since the caller may not be using
		 * ratesel_rates->num for pre-ac d11hdr construction
		 */
		ratesel_rates->rspec[1] = ratesel_rates->rspec[0];
	}
#endif /* WL11AC */

	if (RATELINKMEM_ENAB(rsi->pub)) {
		/* publish new rate set to scb->ratemem.
		 * ac is no longer a care but still could be if want to.
		 */
		if (wlc_ratelinkmem_update_rate_entry(rsi->wlc, state->scb, ratesel_rates,
			(uint8)*frameid) == BCME_OK) {
			state->upd_pend = FALSE;
			wlc_ratesel_updinfo_fromtx(state, CUR_RATESPEC(state));
		}
		RL_MORE(("%s aid %d scb %p epoch %d upd_pend %d\n",
			__FUNCTION__, state->aid, state->scb, state->epoch, state->upd_pend));
	}

	return TRUE;
} /* wlc_ratesel_gettxrate */

void wlc_ratesel_rcb_sz(uint16 *rcb_sz, uint16 *rcb_itxs_sz)
{
	/* size of the rcb struct plus the arrays of select_rates/bfrid/uprid/dnrid/bw */
	*rcb_sz = (sizeof(rcb_t) + sizeof(uint8) * MAX_RATESEL_NUM * 5);
#if defined(WL_MU_TX)
	/* size of the rcb_itxs struct. the block is always per AC */
	*rcb_itxs_sz = sizeof(rcb_itxs_t);
#else
	*rcb_itxs_sz = 0;
#endif /* defined(WL_MU_TX) */
}

void
wlc_ratesel_init_opstats(rcb_t *state)
{
	ratesel_dlstats_t *dlstats;

	dlstats = &state->dlstats;
	/* init un-zero ones */
	dlstats->last_rspec = CUR_RATESPEC(state);
	dlstats->last_su = CUR_RATESPEC(state);
}

/*
 * update info/opstats from tx direction, assuming dl su so no flags
 */
static int
wlc_ratesel_updinfo_fromtx(rcb_t *state, ratespec_t curspec)
{
	RL_STAT(("%s: aid %d last_txrspec 0x%x\n", __FUNCTION__, state->aid, curspec));
	state->dlstats.last_txrspec = curspec;
	return BCME_OK;
}

/*
 * update dl opstats from mac-agg txstatus direction
 */
static int
wlc_ratesel_updinfo_maggtxs(rcb_t *state, tx_status_t *txs,
	ratesel_txs_t *rs_txs, uint8 flags)
{
	bool txs_mu = FALSE;

	if ((flags & RTSTAT_UPD_SU) == 0) {
		/* if not forced, auto -- decide from txs */
		txs_mu = (txs->status.s5 & TX_STATUS64_MUTX) ? TRUE : FALSE;
	}

	if (txs_mu) {
		wlc_ratesel_updinfo_mutxs(state, txs, rs_txs, flags);
	} else {
		wlc_ratesel_updinfo_sutxs(state, txs, rs_txs->txrspec[0], flags);
	}

	return BCME_OK;
}

/*
 * update dl opstats from txs of a regular mpdu or known-su mpdu
 *  this has to be SU based on current drv data path impl
 *  this covers aqm / non-aqm path su-ampdu, and regmpdu txs format
 */
static int
wlc_ratesel_updinfo_sutxs(rcb_t *state, tx_status_t *txs,
	ratespec_t curspec, uint8 flags)
{
	uint32 rate100kbps;
	ratesel_dlstats_t *dlstats = &state->dlstats;

	dlstats->last_type = RTSTAT_TYPE_SU;
	dlstats->last_rspec = curspec;
	dlstats->last_su = curspec;
	rate100kbps = wf_rspec_to_rate(curspec)/100;
	if (dlstats->avg_su100kbps == 0) {
		dlstats->avg_su100kbps = rate100kbps;
	} else {
		UPDATE_MOVING_AVG(&dlstats->avg_su100kbps, rate100kbps, RATESEL_EMA_ALPHA_DEFAULT);
	}
	if (dlstats->avg_rt100kbps == 0) {
		dlstats->avg_rt100kbps = rate100kbps;
	} else {
		UPDATE_MOVING_AVG(&dlstats->avg_rt100kbps, rate100kbps, RATESEL_EMA_ALPHA_DEFAULT);
	}

	RL_STAT(("%s: aid %d su_rspec 0x%x avg_su/rt_100kbps %d %d\n", __FUNCTION__,
		state->aid, dlstats->last_su, dlstats->avg_su100kbps, dlstats->avg_rt100kbps));

	return BCME_OK;
}

/*
 * update dl opstats from mu-txs (which must be mac-agg txs format
 * for dlofdma ignore ruidx/rutype for now
 */
static ratespec_t
wlc_ratesel_updinfo_mutxs(rcb_t *state, tx_status_t *txs,
	ratesel_txs_t *rs_txs, uint8 flags)
{
	uint8 txs_mutype;
	uint8 mutxcnt; /* frame retry_txcnt */
	uint8 mcs, nss, he_gi, bw;
	bool is_sgi;
	ratespec_t mu_rspec;
	uint32 rate100kbps;
	ratesel_dlstats_t *dlstats = &state->dlstats;

	mutxcnt = TX_STATUS64_MU_TXCNT(txs->status.s4);
	if (mutxcnt > 1) return 0; /* not update retried one */

	/* only update on 'primary' rate */
	txs_mutype = TX_STATUS_MUTYP(state->rsi->wlc->pub->corerev, txs->status.s5);
	mcs = TX_STATUS64_MU_MCS(txs->status.s4);
	nss = TX_STATUS64_MU_NSS(txs->status.s4) + 1;
	bw = state->link_bw;

	if (txs_mutype == TX_STATUS_MUTP_VHTMU) {
		mu_rspec = VHT_RSPEC(mcs, nss) | (bw << WL_RSPEC_BW_SHIFT);
		is_sgi = TX_STATUS64_MU_SGI(txs->status.s3);
		if (is_sgi) {
			mu_rspec |= WL_RSPEC_SGI;
		}
	} else { /* is he */
		mu_rspec = HE_RSPEC(mcs, nss) | (bw << WL_RSPEC_BW_SHIFT);
		if (txs_mutype == TX_STATUS_MUTP_HEOM) {
			he_gi = TX_STATUS128_HEOM_CPLTF(txs->status.s4);
		} else {
			is_sgi = TX_STATUS64_MU_SGI(txs->status.s3);
			he_gi = is_sgi ? WL_RSPEC_HE_2x_LTF_GI_0_8us : WL_RSPEC_HE_2x_LTF_GI_1_6us;
		}
		mu_rspec |= (HE_GI_TO_RSPEC(he_gi));
	}

	dlstats->last_type = RTSTAT_TYPE_MU + txs_mutype;
	dlstats->last_rspec = mu_rspec;
	dlstats->last_mu = mu_rspec;
	rate100kbps = wf_rspec_to_rate(mu_rspec)/100;
	if (dlstats->avg_mu100kbps == 0) {
		dlstats->avg_mu100kbps = rate100kbps;
	} else {
		UPDATE_MOVING_AVG(&dlstats->avg_mu100kbps, rate100kbps, RATESEL_EMA_ALPHA_DEFAULT);
	}
	if (dlstats->avg_rt100kbps == 0) {
		dlstats->avg_rt100kbps = rate100kbps;
	} else {
		UPDATE_MOVING_AVG(&dlstats->avg_rt100kbps, rate100kbps, RATESEL_EMA_ALPHA_DEFAULT);
	}
	RL_STAT(("%s: aid %d sumu %d mu_rspec 0x%x avg_mu/rt_100kbps %d %d\n", __FUNCTION__,
		state->aid, dlstats->last_type, dlstats->last_mu,
		dlstats->avg_mu100kbps, dlstats->avg_rt100kbps));
	return mu_rspec;
}

/*
 * Based on flags, provide some rate stats
 * Input:
 *   flags->dlul = 0/1 : (DL) tx / UL mu rx
 *   flags->fromtxs = 0: get primary per rate sel's choice
 *   flags->fromtxs = 1: get last (primary) rate reported from txstatus
 *   below sumu is effective only when fromtxs=1
 *   flags->sumu    = 0: get any rate from txstatus
 *   flags->sumu    = 1: get su rate from txstatus
 *   flags->sumu    = 2: get mu rate from txstatus
 *   flags->get_avg = 0: get last snapshot rate
 *   flags->get_avg = 1: get avg su/mu/any rate
 * Output:
 *   last_rspec  provides the instant last dl/ul su/mu/any ratespec
 *   rate100kbps provides the avg or instant dl/ul su/mu/any rate in 100kbps
 *   psr         provides last/avg dl/ul su/mu/any succ ratio in 1000 percentage unit
 */
ratespec_t
wlc_ratesel_get_opstats(rcb_t *state, uint16 flags)
{
	int type;
	ratesel_dlstats_t *dlstats;

	if (flags & RTSTAT_GET_DIR) {
		/* unsupported now */
		return 0;
	}

	dlstats = &state->dlstats;
	if ((flags & RTSTAT_GET_TXS) == 0) {
		return dlstats->last_txrspec;
	}

	type = (flags & RTSTAT_GET_TYPE_MASK) >> RTSTAT_GET_TYPE_SHIFT;
	switch (type) {
	case RTSTAT_GET_TYPE_MU:
		/* if never seen any txs yet, then return any_rspec */
		if (dlstats->last_type == RTSTAT_TYPE_INIT) {
			return dlstats->last_rspec;
		} else {
			return dlstats->last_mu;
		}
		break;
	case RTSTAT_GET_TYPE_SU:
		return dlstats->last_su;
		break;
	case RTSTAT_GET_TYPE_ANY:
		return dlstats->last_rspec;
	default:
		break;
	}

	return 0;
}

#ifdef WL_LPC
/** Power selection algo specific internal functions */
static void
wlc_ratesel_upd_tpr(rcb_t *state, uint32 prate_cur,
	uint32 cur_psr, uint32 prate_dn, uint32 psr_dn)
{
	uint8 tpr_max;

#ifdef WL_LPC_DEBUG
	/* Note current thruput ratio value */
	state->lcb_info.tpr_val = ((prate_cur * cur_psr) * 100)/(prate_dn * psr_dn);
#endif // endif

	/* Update the TPR threshold (half of rate ratio) */
	tpr_max = prate_cur * 100 / prate_dn;
	state->lcb_info.tpr_thresh = (tpr_max - 100) / 2 + 100;

	/* Compare the current TP ratio value with the threshold value */
	state->lcb_info.tpr_good = (state->lcb_info.tpr_thresh <=
		((prate_cur * cur_psr) * 100)/(prate_dn * psr_dn));
	state->lcb_info.tpr_good_valid = TRUE;
	return;
}

static void
wlc_ratesel_upd_la(rcb_t *state, uint32 curr_psr, uint32 old_psr)
{
	state->lcb_info.la_good = (curr_psr >= old_psr);
	state->lcb_info.la_good_valid = TRUE;
	return;
}

/* External functions */
void
wlc_ratesel_lpc_update(ratesel_info_t *rsi, rcb_t *state)
{
	state->rsi = rsi;
	return;
}

void
wlc_ratesel_lpc_init(rcb_t *state)
{
	/* Reset the LPC related data as well */
	state->lcb_info.tpr_good = FALSE;
	state->lcb_info.tpr_good_valid = FALSE;
	state->lcb_info.la_good = FALSE;
	state->lcb_info.la_good_valid = FALSE;
	state->lcb_info.tpr_thresh = 100;
	state->lcb_info.hi_rate_kbps =
		(RATEKBPS_OF_I(state, state->active_rates_num - 1));
#ifdef WL_LPC_DEBUG
	state->lcb_info.tpr_val = 0;
#endif // endif
	return;
}

void
wlc_ratesel_get_info(rcb_t *state, uint8 rate_stab_thresh, uint32 *new_rate_kbps,
	bool *rate_stable, rate_lcb_info_t *lcb_info)
{
	*new_rate_kbps = (500 * RSPEC2RATE500K(CUR_RATESPEC(state)));
	*rate_stable = ((state->gotpkts == GOTPKTS_ALL) && (state->nupd > rate_stab_thresh));
	bcopy(&state->lcb_info, lcb_info, sizeof(rate_lcb_info_t));
}

void
wlc_ratesel_clr_cache(rcb_t *state)
{
	RL_MORE(("%s: scb %p state %p\n", __FUNCTION__, state->scb, state));
	/* don't use the internal invalidate_txh_cache */
	wlc_txc_inv(state->txc, state->scb);
	state->txc_inv_reqd = FALSE;
}
#endif /* WL_LPC */

#ifdef WLATF
ratespec_t BCMFASTPATH
wlc_ratesel_rawcurspec(rcb_t *state)
{
	return CUR_RATESPEC(state);
}
#endif // endif

/** Minimum Rate; below the specified input value are removed from rateset */
void
wlc_ratesel_filter_minrateset(wlc_rateset_t *rateset, wlc_rateset_t *new_rateset,
	bool is40bw, uint8 min_rate)
{
	uint i, j = 0;

	bcopy(rateset, new_rateset, sizeof(wlc_rateset_t));
	/* No rate limiting required when min rate is zero */
	if (min_rate == 0)
		return;

	/* Eliminate the rates less than min rate */
	for (i = 0; ((i < rateset->count) && (rateset->rates[i] != 0)); i++)
		if ((rateset->rates[i] & RATE_MASK) >= min_rate)
			new_rateset->rates[j++] = rateset->rates[i];
	if (j > 0)
		new_rateset->count = j;
	/* Eliminate the MCS rates less than min rate */
	for (i = 0; i < MCSSET_LEN; i++) {
		if (isset(rateset->mcs, i)) {
			uint32 phy_rate;

			if (is40bw)
				phy_rate = ((mcs_table[i].phy_rate_40 * 2) / 1000);
			else
				phy_rate = ((mcs_table[i].phy_rate_20 * 2) / 1000);
			if ((phy_rate < min_rate))
				clrbit(new_rateset->mcs, i);
		}
	}
	return;
}

void
wlc_ratesel_get_ratecap(rcb_t *state, uint8 *sgi, uint16 mcs_bitmap[])
{
	wlc_ht_info_t *hti = state->rsi->wlc->hti;
	int i;

	memset(mcs_bitmap, 0x0, MCSSET_LEN * sizeof(uint16));

	if (WLC_HT_GET_SGI_TX(hti) == AUTO)
		*sgi = 2; /* 2 = AUTO */
	else {
		*sgi = WLC_HT_GET_SGI_TX(hti)? ON: OFF;
	}

	for (i = 0; i < MCSSET_LEN; i++) {
		mcs_bitmap[i] = state->mcs_bitarray[i];
	}

	return;
}

void
wlc_ratesel_set_link_bw(rcb_t *rcb, uint8 link_bw)
{
	rcb->link_bw = link_bw;
}

uint8
wlc_ratesel_get_link_bw(rcb_t *rcb)
{
	return rcb->link_bw;
}

#ifdef WLC_DTPC
int
wlc_ratesel_upd_pwroffset(rcb_t *state)
{
	ratesel_txparams_t ratesel_rates;
	uint16 flag0, flag1;

	ratesel_rates.num = 4;
	ratesel_rates.ac = 0;

	INVALIDATE_TXH_CACHE(state);

	return wlc_ratesel_gettxrate(state, &flag0, &ratesel_rates, &flag1);
}
#endif // endif
