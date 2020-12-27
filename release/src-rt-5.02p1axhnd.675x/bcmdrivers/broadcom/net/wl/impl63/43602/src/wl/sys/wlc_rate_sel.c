/**
 * @file
 * @brief
 * Common [OS-independent] rate selection algorithm of Broadcom
 * 802.11 Networking Adapter Device Driver.
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
 * $Id: wlc_rate_sel.c 656756 2016-08-30 00:27:39Z $
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

#include <proto/802.11.h>
#include <d11.h>

#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc.h>

#include <wlc_phy_hal.h>
#include <wlc_antsel.h>
#include <wlc_rate_sel.h>
#ifdef WL_LPC
#include <wlc_scb_powersel.h>
#endif // endif

#include <wl_dbg.h>

/*
 * First nibble to rate;
 * Second nibble to spatial/extended spatial probing;
 * Third to rx antenna.
 */
static uint8 ratesel_msglevel = 0;
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
#define RL_CHG(args)	do {if (WL_RATE_ON() && (ratesel_msglevel & RATESEL_MSG_CHG_VAL)) \
				WL_PRINT(args);} while (0)
#define RL_TXS(args)	do {if (WL_RATE_ON() && (ratesel_msglevel & RATESEL_MSG_TXS_VAL)) \
				WL_PRINT(args);} while (0)

#ifdef WL11N
#define MAXRATERECS	224		/* Maximum rate updates */
#else
#define MAXRATERECS	128		/* Maximum rate updates */
#define RATEREC_BYTE(a)	((a) >> 5)	/* raterec is 32-bit uint */
#define RATEREC_BITS(a)	((a) & 0x1F)
#endif /* WL11N */

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

#ifdef WL11N
#define	MPDU_MCSENAB(state)	(state->mcs_streams > 0 ? TRUE : FALSE)
#else
#define	MPDU_MCSENAB(state)	(FALSE)
#endif /* WL11N */

#ifdef WL11N
#define MAXSPRECS		24	/* Maximum spatial AMPDU updates (1 bit values) */
#define MAX_EXTSPRECS		30	/* Maximum extended spatial AMPDU updates (1 bit values) */

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

/* Warning: use this micro only when not care sgi */
#define RATESPEC_OF_I(state, i) (known_rspec[state->select_rates[(i)]] | \
				 ((state)->bw[(i)] << RSPEC_BW_SHIFT))

#define RATEKBPS_OF_I(state, i)	(wlc_rate_rspec2rate(RATESPEC_OF_I(state, i)))
/* care sgi */
#define CUR_RATESPEC(state)	((RATESPEC_OF_I(state, state->rateid)) | \
				(state->sgi ? RSPEC_SHORT_GI : 0))
#else  /* WL11N */
#define RATESPEC_OF_I(state, i) (known_rspec[state->select_rates[(i)]] | RSPEC_BW_20MHZ)
#define CUR_RATESPEC(state)	((RATESPEC_OF_I(state, state->rateid)))
#endif /* WL11N */

#define MCS_STREAM_MAX_UNDEF	0	/* not specified */
#define MCS_STREAM_MAX_1       	1	/* pick mcs rates with one streams */
#define MCS_STREAM_MAX_2       	2	/* pick mcs rates with up to two streams */
#define MCS_STREAM_MAX_3       	3	/* pick mcs rates with up to three streams */
#define MCS_STREAM_MAX_4       	4	/* pick mcs rates with up to four streams */

/* MAX_MCS_NUM is the ultimate limit per spec but that wastes lots of memory, increase as needed */
#define MAX_MCS_NUM_NOW		(1 + 8 * MCS_STREAM_MAX_3) /* 1 for m32. Total 25 */
#define MAX_SHORT_AMPDUS	6	/* max number of short AMPDUs when switching to
					 * next higher rate in vertical rate control
					 */

#ifndef UINT32_MAX
#define UINT32_MAX		0xFFFFFFFF
#endif // endif

#define NOW(state)		(state->rsi->pub->now)

#ifdef WL11N
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

#define PSR_UNINIT			0xFFFFFFFF	/* Mark the PSR value as uninitialized */
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
#endif /* WL11N */

#define DUMMY_UINT8			(0xFF)
#define FIXPRI_FLAG			(0x80)
#define RATEID_AUTO			(0xFF)

/* local macros */
#define MODDECR(i, k)		(((i) > 0)       ? (i)-1 : (k)-1)
#define MODINCR(i, k)		(((i) < ((k)-1)) ? (i)+1 : 0)
#define LIMDEC(i, k)		(((i) > ((k)+1)) ? (i)-1 : (k))
#define LIMINC(i, k)		(((i) < ((k)-1)) ? (i)+1 : (k)-1)
#define LIMSUB(i, k, s)		(((i) > ((k)+(s))) ? (i)-(s) : (k))
#define LIMADD(i, k, s)		(((i) < ((k)-(s))) ? (i)+(s) : (k))
#define LIMINC_UINT32(k)	if ((k) != UINT32_MAX) (k)++

#define RATES_NUM_CCK		(4)		/* cck */
#define RATES_NUM_CCKOFDM	(12)		/* cck/ofdm */

#ifdef WL11N

#ifdef WL11AC /* define superset of 11ac (mcs0-9) and 11n (mcs0-23, 23) */
#define RATES_NUM_MCS		(1 + 10 * MCS_STREAM_MAX_3)
#else
#define RATES_NUM_MCS		(1 + 8 * MCS_STREAM_MAX_3)
#endif /* WL11AC */
#else
/* legacy ofdm/cck only */
#define RATES_NUM_MCS		0
#endif /* WL11N */

#define RATES_NUM_ALL           (RATES_NUM_CCKOFDM + RATES_NUM_MCS)
#define MAX_RATESEL_NUM		(RATES_NUM_CCK + RATES_NUM_MCS) /* we use at most this many rates */

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
#define RSSI_PRI_LMT		10  /* for no mfbr case */
#define RSSI_PRI_LMT_MFBR	20  /* for mfbr */
#define RSSI_STEP		6   /* step size to lower primary */
#endif // endif

#ifdef WL11N
#define RSPEC2RATE500K(rspec)	(wlc_rate_rspec2rate(rspec)/500)
#else
#define RSPEC2RATE500K(rspec)	((rspec) & RSPEC_RATE_MASK)
#endif // endif

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

/* Size optimization macros to help with dead code elimination */
#ifdef WL11N_20MHZONLY
#define IS_20BW(state)	1
#else
#define IS_20BW(state)	((state)->bwcap == BW_20MHZ)
#endif // endif

#ifdef WL11N_SINGLESTREAM
#define	SP_MODE(state)	SP_NONE
#else
#define	SP_MODE(state)	((state)->spmode)
#endif // endif

/* iovar table */
enum {
	IOV_RATESEL_DUMMY, /* dummy one in order to register the module */
	IOV_RATESEL_MSGLEVEL,
	IOV_RATESEL_NSS,
	IOV_RATESEL_MAXSHORTAMPDUS,
	IOV_RATESEL_USEFBR,
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
	IOV_RATESEL_RFBR,
	IOV_RATESEL_ALERT,
	IOV_RATESEL_IDLE,
	IOV_RATESEL_RSSI_RFBR,
	IOV_RATESEL_RSSI_PRI
};

static const bcm_iovar_t ratesel_iovars[] = {
	{"ratesel_dummy", IOV_RATESEL_DUMMY, (IOVF_SET_DOWN), IOVT_BOOL, 0},
	{NULL, 0, 0, 0, 0}
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

#ifdef WL11N
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
	bool    usefbr;
	uint8   nss_lmt;
	uint8   max_short_ampdus;
	/* leave this iovar for debugging purpose */
	bool    hopping;		/* allow hopping 2 other stream family upon dropping rate */
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
#endif /* WL11N */
	uint8 ncf_lmt;
	uint8   sgi_tmin;               /* mininum time-out */
	uint8   sgi_tmax;               /* maximum time-out */
};

/** rcb: Ratesel Control Block is per link rate control block. */
struct rcb {
	struct scb	*scb; /* back pointer to scb */
	ratesel_info_t 	*rsi;	/* back pointer to ratesel module local structure */
	uint8   select_rates[MAX_RATESEL_NUM];	/* the rateset in use */
	uint8   fbrid[MAX_RATESEL_NUM];	/* corresponding fallback rateid (prev_rate - 2) */
	uint8   uprid[MAX_RATESEL_NUM]; /* next up rateid (prev_rate + 1) */
	uint8   dnrid[MAX_RATESEL_NUM]; /* next down rateid (prev_rate - 1) */
	uint8   bw[MAX_RATESEL_NUM];    /* bw for each rate in select_rates */
	uint8	active_rates_num;	/* total active rates */
	uint32	nskip;		/* number of txstatus skipped due to epoch mismatch */
	uint32	nupd;		/* number of txstatus used since last rate change */
	uint8	epoch;		/* flag to filter out stale rate_selection info */
	uint8	rateid;		/* current rate id */
	uint8	gotpkts;	/* this is a bit flag, other flags can go here if needed */
	uint	*clronupd;	/* pointer to external uint we clear on epoch/rateindex change */

#ifndef WL11N
	uint32	nofb[RATEREC_BYTE(MAXRATERECS)];	/* bit array to store fb/nonfb history */
	int32	nofbtotdn;	/* num of nofb update for rate_down algorithm */
	int32	nofbtotup;	/* num of nofb update for rate_up algorithm */
	uint8	nofbid;		/* last position updated in nofb ring */
	uint8	Mup;		/* windows size for computing nofbtotup */
	uint8	Mdn;		/* windows size for computing nofbtotdn */
	uint8	Kup;		/* threshold to step up rate */
	uint8	Kdn;		/* threshold to step down rate */
#endif /* WL11N */

#ifdef WL11N
	bool    vht;		/* using vht rate */
	uint	spmode;		/* spatial probe mode common to all scb */
	uint8   mcs_baseid;	/* starting from this index the rate becomes MCS */
	uint8	bwcap;		/* 1/2/3 for 20/40/80MHz */
	bool   	has_sgi;	/* supports sgi (must be NONGF_CAP) */
	bool	sgi;		/* current rate is using SGI (must be mcs7 or mcs15) */
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
	uint8	extsp_stats[MAX_EXTSPRECS >> 3];	/* bit array to store ext spatial history */
	uint8   extsp_ixtoggle; /* toggle between intra-probe and cross-probe */
	uint16  extsp_Pmod;	    /* poll modulo */
	uint16  extsp_Poff;	    /* poll offset */
	/* Antcfg history */
	uint16  txs_cnt;
	/* Throughput-based rate selection */
	psr_info_t	psri[PSR_ARR_SIZE];	/* PSR information used in the legacy rate algo. */
	uint8	ncfails; /* consecutive ampdu transmission failures since the rate changes. */
	uint32  mcs_sp_thrt0;	/* thruput across diff. rates in the spatial family being probed */
	uint32  mcs_sp_thrt1;	/* thrtuput across diff. rates in the primary spatial family */
	uint32  extsp_thrt0; /* thruput across diff. rates in the spatial family being probed */
	uint32  extsp_thrt1; /* thruput across diff. rates in the other spatial family */
#ifdef WL_LPC
	rate_lcb_info_t lcb_info; /* LPC related info in the rate selection cubby */
#endif // endif
#endif /* WL11N */

	int lastxs_rssi;	/* rssi noted down at last good tx */
	uint lastxs_time;	/* last good tx time */
	int16 rssi_lastdrop;	/* rssi for the latest pri drop */

	int8    sgi_cnt;        /* counter how many txstatus have updated sgi_state */
	uint32  psr_sgi;        /* PSR of the primary rate + SGI */
	uint    sgi_timeout;    /* next reset time */
	uint8   sgi_timer;      /* timer */
	bool    sgi_lastprobe_ok; /* last sgi probe result : 0 -- failure, 1 -- succ */
};

/* XXX: wlc_txc_inv_ptr exposes a pointer to the host side &scb_txc_info::txhlen
 *
 * NOTE: The INVALIDATE_TXH_CACHE macro will update the host side txhlen.
 * Ensure that a cached scb_txc_info in a scbtag is "also" flushed/invalidated.
 */
#if defined(BCM_HOST_MEM_TXCTAG)
#include <wlc_txc.h>                     /* scbtag_flush_txc_info() */
#define SCBTAG_FLUSH_TXC_INFO()          scbtag_flush_txc_info()
#else
#define SCBTAG_FLUSH_TXC_INFO()          do { /* noop */ } while (0)
#endif /* ! BCM_HOST_MEM_TXCTAG */

/* XXX:
 * state->clronupd is a pointer to scb_txc_info::txhlen, that was initialized
 * using wlc_txc_inv_ptr()
 * When an update to txhlen occurs, it is assumed that the scb_txc_info object
 * is indeed intact.
 */
#define INVALIDATE_TXH_CACHE(state) \
	({ \
		if (state->clronupd) { \
			SCBTAG_FLUSH_TXC_INFO(); \
			*state->clronupd = 0; \
		} \
	})

#ifdef WL11N
#define IS_SP_PKT(state, cur_mcs, tx_mcs) \
	(SP_MODE(state) == SP_NORMAL && (cur_mcs) != (tx_mcs))
#define IS_EXTSP_PKT(state, cur_mcs, tx_mcs, antselid) (SP_MODE(state) == SP_EXT && \
	((cur_mcs) != (tx_mcs) || (antselid) != (state->antselid)))
#endif /* WL11N */

/** table of known rspecs for legacy rate selection (not monotonically ordered) */
static const ratespec_t known_rspec[RATES_NUM_ALL] = {
	CCK_RSPEC(2),
	CCK_RSPEC(4),
	CCK_RSPEC(11),
	OFDM_RSPEC(12),
	OFDM_RSPEC(18),
	CCK_RSPEC(22),
	OFDM_RSPEC(24),
	OFDM_RSPEC(36),
	OFDM_RSPEC(48),
	OFDM_RSPEC(72),
	OFDM_RSPEC(96),
	OFDM_RSPEC(108),
#ifdef WL11N
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
	VHT_RSPEC(0, 3),  /* MCS  0: SS 2, MOD: BPSK,  CR 1/2 */
	VHT_RSPEC(1, 3),  /* MCS  1: SS 2, MOD: QPSK,  CR 1/2 */
	VHT_RSPEC(2, 3),  /* MCS  2: SS 2, MOD: QPSK,  CR 3/4 */
	VHT_RSPEC(3, 3),  /* MCS  3: SS 2, MOD: 16QAM, CR 1/2 */
	VHT_RSPEC(4, 3),  /* MCS  4: SS 2, MOD: 16QAM, CR 3/4 */
	VHT_RSPEC(5, 3),  /* MCS  5: SS 2, MOD: 64QAM, CR 2/3 */
	VHT_RSPEC(6, 3),  /* MCS  6: SS 2, MOD: 64QAM, CR 3/4 */
	VHT_RSPEC(7, 3),  /* MCS  7: SS 2, MOD: 64QAM, CR 5/6 */
	VHT_RSPEC(8, 3),  /* MCS  8: SS 2, MOD: 256QAM,CR 3/4 */
	VHT_RSPEC(9, 3),  /* MCS  9: SS 2, MOD: 256QAM,CR 5/6 */
#else /* WL11AC */
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
#endif /* WL11AC */
#endif /* WL11N */
	};

#ifndef WL11N
static uint8 M_params[RATE_UDSZ][RATES_NUM_ALL] = {
	/* window size for rate to move up */
	{ 16,  16,  24,  32,  32,  32,  32,  48,  48,  96,  96, 106 }, /* CCK OFDM */
	/* window size for rate to move down */
	{ 16,  16,  16,  16,  16,  16,  16,  16,  16,  24,  32,  32 }  /* CCK OFDM */
};

static uint8 K_params[RATE_UDSZ][RATES_NUM_ALL] = {
	/* threshold for rate to move up */
	{  3,   3,   3,   2,   2,   2,   2,   2,   2,   2,   2,   3},
	/* threshold for rate to move down */
	{  3,   4,   4,   4,   4,   4,   4,   4,   4,   3,   2,   2}
};
#endif /* !WL11N */

#ifdef WL11N
/** spatial_MCS, spatial_K, spatial_Pmod, spatial_Poff */
enum {
	SPATIAL_MCS_COL1,
	SPATIAL_MCS_COL2,
	SPATIAL_K_COL,
	SPATIAL_PMOD_COL,
	SPATIAL_POFF_COL,
	SPATIAL_PARAMS_NUM
};

#ifdef WL11AC
/* Use new representation of mcs as <nss,mcs> */
/* mcs 32 and mcs 0...9 x 3 */
static int mcs_sp_params[RATES_NUM_MCS][SPATIAL_PARAMS_NUM] = {
	{-1, -1, 2, 17, 6},    /* mcs 32 and legacy rates */
	{0x20, -1, 2, 17, 6},  /* c0s1 */
	{0x20, -1, 2, 17, 6},  /* c1s1 */
	{0x21, -1, 2, 18, 6},  /* c2s1 */
	{0x21, -1, 2, 18, 6},  /* c3s1 */
	{0x22, -1, 2, 19, 6},  /* c4s1 */
	{0x23, -1, 2, 20, 6},  /* c5s1 */
	{0x24, -1, 2, 20, 6},  /* c6s1 */
	{0x24, -1, 2, 20, 6},  /* c7s1 */
	{0x25, -1, 2, 24, 6},  /* c8s1 */
	{0x25, -1, 2, 27, 6},  /* c9s1 */
	{0x12, 0x30, 2, 17, 6},  /* c0s2 */
	{0x13, 0x31, 2, 18, 6},  /* c1s2 */
	{0x14, 0x31, 2, 19, 7},  /* c2s2 */
	{0x15, 0x32, 2, 20, 7},  /* c3s2 */
	{0x17, 0x33, 2, 30, 7},  /* c4s2 */
	{0x19, 0x34, 2, 40, 7},  /* c5s2 */
	{  -1, 0x34, 2, 50, 7},  /* c6s2 */
	{  -1, 0x35, 2, 60, 7},  /* c7s2 */
	{  -1, 0x36, 2, 72, 7},  /* c8s2 */
	{  -1, 0x37, 2, 80, 7},  /* c9s2 */
	{-1, 0x21, 2, 19, 8},  /* c0s3 */
	{-1, 0x22, 2, 19, 8},  /* c1s3 */
	{-1, 0x23, 2, 23, 8},  /* c2s3 */
	{-1, 0x24, 2, 40, 8},  /* c3s3 */
	{-1, 0x26, 2, 50, 8},  /* c4s3 */
	{-1, 0x27, 2, 60, 8},  /* c5s3 */
	{-1, 0x28, 2, 72, 8},  /* c6s3 */
	{-1, 0x29, 2, 80, 8},  /* c7s3 */
	{-1, -1, 2, 21, 8},  /* c8s3 */
	{-1, -1, 2, 21, 8}   /* c9s3 */
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
	{-1,  9, 2, 19, 8},  /* mcs 16 */
	{-1, 10, 2, 19, 8},
	{-1, 11, 2, 23, 8},
	{-1, 12, 2, 40, 8},  /* mcs 19 */
	{-1, 14, 2, 50, 8},  /* mcs 20 */
	{-1, 15, 2, 60, 8},  /* mcs 21 */
	{-1, -1, 2, 21, 8},  /* mcs 22 */
	{-1, -1, 2, 21, 8}   /* mcs 23 */
};
#endif /* WL11AC */
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
	{-1, 2, 300, 8}   /* c9s3 */
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

#endif /* WL11N */

#if defined(BCM_HOST_MEM_RESTORE) && defined(BCM_HOST_MEM_SCB)
void
swap_cubby_ratesel_scb(rcb_t *scb_rate_cubby, struct scb* scb_new);
#endif // endif

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static int wlc_ratesel_dump(ratesel_info_t *rsi, struct bcmstrbuf *b);
#endif /* BCMDBG || BCMDBG_DUMP */

static int wlc_ratesel_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *p, uint plen, void *a, int alen, int val_size, struct wlc_if *wlcif);

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
static void wlc_ratesel_pick_rate(rcb_t *state, bool is_probe);

static int wlc_ratesel_use_txs(rcb_t *state, tx_status_t *txs, uint16 SFBL, uint16 LFBL,
	uint8 tx_mcs, uint8 antselid, bool fbr);

#ifdef WL11N
static bool wlc_ratesel_filter_mcsset(wlc_rateset_t *rateset, uint8 nss_limit, bool en_sp,
	uint8 bw, bool vht_ldpc, uint8 vht_ratemask, uint16 dst[], uint8 *mcs_streams);
static int wlc_ratesel_mcsbw2id(rcb_t *state, uint8 mcs, uint8 bw);
static void wlc_ratesel_init_defantsel(rcb_t *state, uint8 txant_num, uint8 antselid_init);
static void wlc_ratesel_sanitycheck_psr(rcb_t *state);
static bool wlc_ratesel_next_rate(rcb_t *state, int8 incdec, uint8 *next_rateid,
       ratespec_t *next_rspec);

static bool wlc_ratesel_upd_spstat(rcb_t *state, bool blockack, uint8 sp_mcs,
	uint ntx_succ, uint ntx);
static bool wlc_ratesel_upd_extspstat(rcb_t *state, bool blockack,
	uint8 sp_mcs, uint8 antselid, uint ntx_succ, uint ntx);

static bool wlc_ratesel_start_probe(rcb_t *state);
static bool wlc_ratesel_change_sp(rcb_t *state);
static bool wlc_ratesel_change_extsp(rcb_t *state);
static void wlc_ratesel_clear_spstat(rcb_t *state);

static int wlc_ratesel_use_txs_blockack(rcb_t *state, tx_status_t *txs, uint8 suc_mpdu,
	uint8 tot_mpdu, bool ba_lost, uint8 retry, uint8 fb_lim, uint8 mcs, bool tx_error,
	uint8 antselid);
#if (defined(WLAMPDU_MAC) || defined(D11AC_TXD))
static int wlc_ratesel_use_txs_ampdu(rcb_t *state, uint16 frameid,
	uint mrt, uint mrt_succ, uint fbr, uint fbr_succ,
	bool tx_error, uint8 tx_mcs, uint8 antselid);
#endif // endif

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

#define	print_psri(state) \
	RL_MORE(("time %d UP [%sx%02x t %d p %d] CUR [%sx%02x t %d p %d] " \
		"DN [%sx%02x t %d p %d] FB [%sx%02x t %d p %d]\n", \
		NOW(state), \
		/* UP */ \
		IS_MCS(upp->rspec) ? "m" : "", \
		RSPEC_RATE_MASK & upp->rspec, upp->timestamp, upp->psr, \
		/* CUR */ \
		IS_MCS(cur->rspec) ? "m" : "", \
		RSPEC_RATE_MASK & cur->rspec, cur->timestamp, cur->psr, \
		/* DN */ \
		IS_MCS(dnp->rspec) ? "m" : "", \
		RSPEC_RATE_MASK & dnp->rspec, dnp->timestamp, dnp->psr, \
		/* FBR */ \
		IS_MCS(fbr->rspec) ? "m" : "", \
		RSPEC_RATE_MASK & fbr->rspec, dnp->timestamp, dnp->psr))
#endif /* WL11N */

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

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
	wlc_dump_register(rsi->pub, "ratesel", (dump_fn_t)wlc_ratesel_dump, (void *)rsi);
#endif // endif

#ifdef WL11N
	rsi->nss_lmt = wlc->stf->txstreams;
	rsi->usefbr = TRUE;
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

	rsi->ncf_lmt = NCFAIL_LMT;

	WL_RATE(("%s: rsi %p psr_ema_alpha0 %d psr_ema_alpha %d rssi_lmt %d\n",
		__FUNCTION__, rsi, rsi->psr_ema_alpha0, rsi->psr_ema_alpha, rsi->rssi_lmt));

#endif /* WL11N */
	return rsi;

fail:
	MFREE(wlc->osh, rsi, sizeof(ratesel_info_t));
	return NULL;
}

#ifdef WL11N
void
BCMATTACHFN(wlc_ratesel_rssi_attach)(ratesel_info_t *rsi, enable_rssi_t en_fn,
	disable_rssi_t dis_fn, get_rssi_t get_fn)
{
	ASSERT(rsi);

	rsi->enable_rssi_fn = en_fn;
	rsi->disable_rssi_fn = dis_fn;
	rsi->get_rssi_fn = get_fn;
}
#endif // endif

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
wlc_ratesel_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *p, uint plen, void *a, int alen, int val_size, struct wlc_if *wlcif)
{
	ratesel_info_t *rsi = (ratesel_info_t *)hdl;
	int err = 0;
	wlc_info_t *wlc;

	wlc = rsi->wlc;
	BCM_REFERENCE(wlc);

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

	bcm_bprintf(b, "\tAC[%d] --- ", ac);

	rateid = rcb->rateid;
	if (rateid < rcb->active_rates_num)
		cur_rspec = RATESPEC_OF_I(rcb, rateid);
#ifndef WL11N
	if (rcb->rateid < rcb->active_rates_num) {
		bcm_bprintf(b,  "%s 0x%x bw %d epoch %d skips %u nupds %u\n",
		IS_MCS(cur_rspec) ? "mcs" : "rate",
		RSPEC_RATE_MASK & cur_rspec, (RSPEC_BW_MASK & cur_rspec) >> RSPEC_BW_SHIFT,
		rcb->epoch, rcb->nskip, rcb->nupd);
	} else {
		/* if current rateid is not in current select_rate_set */
		bcm_bprintf(b,  "rate NA epoch %d skips %u nupds %u\n",
		    rcb->epoch, rcb->nskip, rcb->nupd);
	}
#else
	bcm_bprintf(b, "spmode %d ", rcb->spmode);
	if (rcb->rateid < rcb->active_rates_num) {
		bcm_bprintf(b,  "%s 0x%x sgi %d epoch %d ncf %u skips %u nupds %u\n",
		IS_MCS(cur_rspec) ? "mcs" : "rate", RSPEC_RATE_MASK & cur_rspec,
		rcb->sgi, rcb->epoch, rcb->ncfails, rcb->nskip, rcb->nupd);
	} else {
		/* if current rateid is not in current select_rate_set */
		bcm_bprintf(b,  "rate NA sgi %d epoch %d ncf %u skips %u nupds %u\n",
		   rcb->sgi, rcb->epoch, rcb->ncfails, rcb->nskip, rcb->nupd);
	}
#endif /* WL11N */

	return;
}
#endif /* BCMDBG */

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static int
wlc_ratesel_dump(ratesel_info_t *rsi, struct bcmstrbuf *b)
{

#ifdef WL11N
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
#endif /* WL11N */
	return 0;
}

#endif /* BCMDGB || BCMDBG_DUMP */

void
wlc_ratesel_dump_rateset(rcb_t *state, struct bcmstrbuf *b)
{
	int i;
	ratespec_t rspec0;

	if (b) {
		bcm_bprintf(b, "state 0x%p active_rates_num %d", state, state->active_rates_num);
#ifdef WL11N
		if (state->active_antcfg_num >= 1)
			bcm_bprintf(b, " bwcap %d spmode %d use_sgi %d antsel ON antcfg %d",
				state->bwcap, state->spmode, state->has_sgi,
				state->active_antcfg_num);
		else
			bcm_bprintf(b, " bwcap %d spmode %d use_sgi %d antsel OFF",
				state->bwcap, state->spmode, state->has_sgi);
#endif // endif
		bcm_bprintf(b, "\nrate index : rate   rspec 500Kbps fbr/dn/up\n");
		/* debug output of selected rate set */
		for (i = 0; i < state->active_rates_num; i++) {
			rspec0 = RATESPEC_OF_I(state, i);
			bcm_bprintf(b, "rate_id %2d : %sx%02x 0x%02x %4d  %2d %2d %2d\n",
				i, (IS_MCS(rspec0) ? "m" : ""), RSPEC_RATE_MASK & rspec0,
				rspec0, RSPEC2RATE500K(rspec0),
				state->fbrid[i], state->dnrid[i], state->uprid[i]);
		}
	} else {
		printf("state 0x%p active_rates_num %d", state, state->active_rates_num);
#ifdef WL11N
		if (state->active_antcfg_num >= 1)
			printf(" bwcap %d spmode %d use_sgi %d antsel ON antcfg %d",
				state->bwcap, state->spmode, state->has_sgi,
				state->active_antcfg_num);
		else
			printf(" bwcap %d spmode %d use_sgi %d antsel OFF",
				state->bwcap, state->spmode, state->has_sgi);
#endif // endif
		printf("\nrate index : rate   rspec 500Kbps fbr/dn/up\n");
		/* debug output of selected rate set */
		for (i = 0; i < state->active_rates_num; i++) {
			rspec0 = RATESPEC_OF_I(state, i);
			printf("rate_id %2d : %sx%02x 0x%02x %4d  %2d %2d %2d\n",
				i, (IS_MCS(rspec0) ? "m" : ""), RSPEC_RATE_MASK & rspec0,
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

#ifdef WL11N
	limit = RATES_NUM_ALL;
#else /* WL11N */
	limit = RATES_NUM_CCKOFDM;
#endif /* WL11N */

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
	if (start_rate) {
		/* get the best match */
		/* state->selectrates are sorted within each stream family */
		for (rate_entry = 0; rate_entry < state->active_rates_num; rate_entry++) {
			/* We would always endup with a single stream rate */
			rspec = RATESPEC_OF_I(state, rate_entry);
			phy_rate = wlc_rate_rspec2rate(rspec);
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

	return ((state->gotpkts & GOTPKTS_INIT) ? state->fbrid[rateid] : 0);

}

#ifdef WL11N
/**
 * init and tool functions for WL11N
 */
/* return <mcs>'s offset in select_rates[] whose's bw is >= bw in input */
static int
wlc_ratesel_mcsbw2id(rcb_t *state, uint8 mcs, uint8 bw)
{
	uint8 i;
	for (i = state->mcs_baseid; i < state->active_rates_num; i++) {
		if (state->bw[i] >= bw &&
		   (RSPEC_RATE_MASK & known_rspec[state->select_rates[i]]) == mcs)
			return i;
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
 * Output:
 *     mcs bitmap in uint16[]
 *     maximum number of streams
 *     whether this is ht or vht (return value)
 */
static bool
wlc_ratesel_filter_mcsset(wlc_rateset_t *rateset,
	uint8 nss_limit, bool en_sp, uint8 bw, bool vht_ldpc, uint8 ratemask,
	uint16 dst[], uint8 *mcs_streams)
{
	uint8 i;
	bool vht = FALSE;

	bzero(dst, MCSSET_LEN);
	*mcs_streams = 0;

#ifdef WL11AC
	/* check if vht mcs rates are used or not */
	if (VHT_MCS_MAP_GET_MCS_PER_SS(1, rateset->vht_mcsmap) != VHT_CAP_MCS_MAP_NONE) {
		vht = TRUE;
	}

	if (vht) {
		uint8 mcs_code;
		uint8 nss;

		nss = MIN(VHT_CAP_MCS_MAP_NSS_MAX, nss_limit);
		/* find number of nss streams based on rateset->vht_mcsmap */
		for (i = 1; i <= nss; i++) {
			mcs_code = VHT_MCS_MAP_GET_MCS_PER_SS(i, rateset->vht_mcsmap);
			if (mcs_code == VHT_CAP_MCS_MAP_NONE) {
				/* bail out in order to cap at this stream - 1 */
				break;
			}
			*mcs_streams = i;
		}

		for (i = 1; i <= *mcs_streams; i++) {
			mcs_code = VHT_MCS_MAP_GET_MCS_PER_SS(i, rateset->vht_mcsmap);
			dst[i-1] = wlc_get_valid_vht_mcsmap(mcs_code, bw, vht_ldpc, i, ratemask);
		}
	} else
#endif /* WL11AC */
	{
		for (i = 0; i < MIN(MCSSET_LEN, nss_limit); i++) {
			dst[i] = (uint16)rateset->mcs[i];
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

	RL_INFO(("%s: vht %d sp %d nss %d ldpc %d bw %d rm 0x%x dst %04x %04x %04x %04x\n",
		__FUNCTION__, vht, en_sp, *mcs_streams, vht_ldpc, bw, ratemask,
		dst[0], dst[1], dst[2], dst[3]));

	return vht;
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
	cur_rate = wlc_rate_rspec2rate(rspec);
	/* filter mcs rate out of range */
	if (cur_rate < minr || (maxr != 0 && cur_rate > maxr)) {
		return FALSE;
	}

	if (maxr != 0) {
		cur_rate = wlc_rate_rspec2rate(rspec | RSPEC_SHORT_GI);
		if (cur_rate > maxr)
			state->has_sgi = FALSE;
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
wlc_ratesel_getmcsfbr(rcb_t *state, uint16 frameid, uint8 plcp0)
{
	uint8 mcs, bw;
	int mcsid, fbrid;
	ratespec_t fbrspec;

	mcs = plcp0 & ~MIMO_PLCP_40MHZ;
	bw = (plcp0 & MIMO_PLCP_40MHZ) ? BW_40MHZ : BW_20MHZ;
#ifdef WL11AC
	ASSERT(state->vht == FALSE);
	mcs = wlc_rate_ht2vhtmcs(mcs);
#endif // endif

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
		mcs = RSPEC_VHT_MCS_MASK & fbrspec;
		nss = wlc_ratespec_nss(fbrspec) - 1;
		fbrspec = HT_RSPEC(8*nss + mcs);
	}
#endif // endif
	fbrspec |= (state->bw[fbrid] << RSPEC_BW_SHIFT);
	return fbrspec;
}

/**
 * Function: return the rateid of the next up/down rate but doesn't change it.
 * <incdec> decides the up/down direction.
 */
static bool BCMFASTPATH
wlc_ratesel_next_rate(rcb_t *state, int8 incdec, uint8 *next_rateid, ratespec_t *next_rspec)
{
	bool set_sgi = FALSE;
	ratespec_t rspec = RATESPEC_OF_I(state, state->rateid);

	if (incdec == +1) {
		*next_rateid = state->uprid[state->rateid];
		if (*next_rateid == state->rateid) {
			if (IS_MCS(rspec) && (state->has_sgi && !state->sgi))
				set_sgi = TRUE;
			else
				return FALSE;
		}
	}
	else if (incdec == -1) {
		if (state->sgi) {
			/* always drop sgi first. Don't change rateid yet */
			*next_rateid = state->rateid; /* restore rateid */
		} else {
			/* drop the rate */
			*next_rateid = state->dnrid[state->rateid];
			if (*next_rateid == state->rateid)
				return FALSE;

			/* special handling to use rssi hold rate */
			if (IS_CCK(rspec) && state->rsi->use_rssi && MPDU_MCSENAB(state)) {
				if (state->rsi->get_rssi_fn &&
					((state->rsi->get_rssi_fn(state->scb)) >=
					state->rsi->rssi_lmt)) {
					*next_rateid = state->rateid; /* restore rateid */
					return FALSE;
				}
			}
		}
	} else {
		/* shall not be called at all */
		return FALSE;
	}

	/* generate new rspec */
	*next_rspec = RATESPEC_OF_I(state, *next_rateid);

	ASSERT(rspec != *next_rspec || set_sgi != state->sgi);

	if (set_sgi)
		*next_rspec |= RSPEC_SHORT_GI;
	else
		*next_rspec &= (~RSPEC_SHORT_GI);

	return TRUE;

}
#endif /* WL11N init functions */

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

#ifdef WL11N
	uint16 filter_mcs_bitarray[MCSSET_LEN];
#endif // endif

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

#ifdef WL11N
	/* make sure they have good default values */
	state->spmode = SP_NONE;

	/* 1) scrub out mcs set entries that often aren't worth using */
	/* always do SISO/MIMO spatial probing if possible */
	state->vht = wlc_ratesel_filter_mcsset(rateset, state->rsi->nss_lmt, TRUE, state->bwcap,
		state->vht_ldpc, state->vht_ratemask, filter_mcs_bitarray, &state->mcs_streams);

	if (state->mcs_streams > 0) {
		uint8 tbl_legacy_withMcs_20Mhz[] = {2, 4, 11};
		uint8 tbl_legacy_withMcs_40Mhz[] = {2, 4, 11, 22};
		ratespec_t cur_rspec;

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
#ifdef WL11N
					state->bw[state->active_rates_num] = BW_20MHZ;
#endif // endif
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
		if (state->bwcap != BW_20MHZ && state->rsi->wlc->mimo_40txbw == AUTO &&
		    state->mcs_baseid == 0) {
			/* Add MCS 0 and 1 in bw20 in replace of mcs0/bw40 */
			for (i = 0; i <= 1; i++) {
				if (filter_mcs_bitarray[0] & (1 << i)) {
					state->bw[state->active_rates_num] = BW_20MHZ;
#ifdef WL11AC
					cur_rspec = VHT_RSPEC(i, 1);
#else
					cur_rspec = HT_RSPEC(i);
#endif // endif
					if (wlc_ratesel_chklmt(state,
						cur_rspec | RSPEC_BW_20MHZ, max_rate, min_rate)) {
						state->select_rates[state->active_rates_num++] =
							wlc_ratesel_rspec2idx(state, cur_rspec);
					}
				}
			}
#ifdef WL11AC
			if (state->bwcap != BW_40MHZ) {
				/* Add MCS 1 and 2 in bw40 in replace of mcs0/bw40 */
				for (i = 1; i <= 2; i++) {
					if (filter_mcs_bitarray[0] & (1 << i)) {
						cur_rspec = VHT_RSPEC(i, 1);
						if (!wlc_ratesel_chklmt(state,
							cur_rspec | RSPEC_BW_40MHZ,
							max_rate, min_rate))
							continue;
						state->bw[state->active_rates_num] = BW_40MHZ;
						state->select_rates[state->active_rates_num++] =
							wlc_ratesel_rspec2idx(state, cur_rspec);
					}
				}
			}
#endif /* WL11AC */
		}

		if (state->mcs_baseid != state->active_rates_num) {
			/* we have appended rate with lower bw. remove mcs0 at primary bw. */
			filter_mcs_bitarray[0] &= ~1;
		}
#ifdef WL11AC
		for (i = 1; i <= state->mcs_streams; i++) {
			for (k = 0; k < 16; k++) {
				if (filter_mcs_bitarray[i-1] & (1 << k)) {
					cur_rspec = VHT_RSPEC(k, i);
					if (!wlc_ratesel_chklmt(state,
						cur_rspec | (state->bwcap << RSPEC_BW_SHIFT),
						max_rate, min_rate))
						continue;
					state->bw[state->active_rates_num] = state->bwcap;
					state->select_rates[state->active_rates_num++] =
						wlc_ratesel_rspec2idx(state, cur_rspec);
				}
			}
		}
#else
		for (i = 0; i < (uint)(8 * state->mcs_streams); i++) {
			if (isset(filter_mcs_bitarray, i)) {
				cur_rspec = HT_RSPEC(i);
				if (!wlc_ratesel_chklmt(state,
					cur_rspec | (state->bwcap << RSPEC_BW_SHIFT),
					max_rate, min_rate))
					continue;

				state->bw[state->active_rates_num] = state->bwcap;
				state->select_rates[state->active_rates_num++] =
					wlc_ratesel_rspec2idx(state, cur_rspec);
			}
		}
#endif /* WL11AC */
		if (state->active_rates_num == 0)
			state->mcs_streams = 0;

		max_rate /= 500; min_rate /= 500;
	}
	if (state->mcs_streams == 0)
		/* no mcs: either a legacy link or max_rate filters out all mcs rates */
#endif	/* WL11N */
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
#ifdef WL11N
					state->bw[state->active_rates_num] = BW_20MHZ;
#endif // endif
					state->select_rates[state->active_rates_num++] =
						wlc_ratesel_rspec2idx(state, LEGACY_RSPEC(ptbl[k]));
					break;
				}
			}
		}
#ifdef WL11N
		state->mcs_baseid = state->active_rates_num;
#endif	/* WL11N */
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
#ifdef WL11N
	ratespec_t cur_rspec;
#endif // endif
	for (i = 0; i < state->active_rates_num; i++) {
		/* default fallback rate is two rate id down */
		fbr_id = LIMSUB(i, 0, 2);
#ifdef WL11N
		cur_rspec = known_rspec[state->select_rates[i]];
		if (IS_MCS(cur_rspec)) {
			int mcs, fbr_mcs;
			int nss, fbr_nss;
			int bw, k;
			bool bw_auto = FALSE;

			if (state->rsi->wlc->mimo_40txbw == AUTO && state->mcs_baseid == 0)
				bw_auto = TRUE;

			mcs = wlc_ratespec_mcs(cur_rspec);
			nss = wlc_ratespec_nss(cur_rspec);
			bw = state->bw[i];

			if (i > 0 && (nss > 1 || mcs >= 1)) {
				uint32 fbr_rate, tmp_rate;

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
				} else if (bw_auto && nss == 1 && mcs <= 2) {
					/* special handling due to using 20/40 in 80 */
					if (bw == BW_80MHZ && mcs == 2) {
						fbr_mcs = 2;
						bw = BW_40MHZ;
					} else if (bw == BW_40MHZ && mcs == 1) {
						fbr_mcs = 1;
						bw = BW_20MHZ;
					}
				}

				/* search the largest index whose corresponding mcs/rate
				 * is lower than fbr_mcs/rate
				 */
				fbr_rate = wlc_rate_mcs2rate(fbr_mcs, fbr_nss,
					bw << RSPEC_BW_SHIFT, FALSE);
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
#endif /* WL11N */
		state->fbrid[i] = fbr_id;

		ASSERT(state->select_rates[i] > state->fbrid[i] || i == 0);
#ifdef WL11N
		ASSERT(RATEKBPS_OF_I(state, i) >= RATEKBPS_OF_I(state, fbr_id));
#else
		ASSERT(wlc_rate_rspec2rate(known_rspec[(state)->select_rates[(i)]])
		       >= wlc_rate_rspec2rate(known_rspec[(state)->select_rates[(fbr_id)]]));
#endif // endif
	}
} /* wlc_ratesel_init_fbrates */

/** init the next up/dn rate look-up table */
static void
wlc_ratesel_init_nextrates(rcb_t *state)
{
	int i, next;
#ifdef WL11N
	ratespec_t cur_rspec;
#endif // endif
	/* next up rate */
	for (i = 0; i < state->active_rates_num-1; i++) {
		next = i+1;
#ifdef WL11N
		cur_rspec = RATESPEC_OF_I(state, i);
		if (SP_MODE(state) != SP_NONE && IS_MCS(cur_rspec)) {
			if (wlc_ratespec_nss(cur_rspec) !=
			    wlc_ratespec_nss(RATESPEC_OF_I(state, next)))
				/* stay at the highest modulation with the same # streams */
				next = i;
		}
#endif // endif
		state->uprid[i] = (uint8)next;
	}
	state->uprid[i] = (uint8)i; /* highest one */

	/* next down rate */
	state->dnrid[0] = 0; /* lowest one */
	for (i = 1; i < state->active_rates_num; i++) {
		next = i - 1;
#ifdef WL11N
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
#endif /* WL11N */
		state->dnrid[i] = (uint8)next;
	}
}

bool
wlc_ratesel_minrate(rcb_t *state, tx_status_t *txs)
{
	uint8 epoch;

	epoch = (txs->frameid & TXFID_RATE_MASK) >> TXFID_RATE_SHIFT;

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
wlc_ratesel_init(ratesel_info_t *rsi, rcb_t *state, void *scb, void *clronupd,
	wlc_rateset_t *rateset, /* [in] */
	uint8 bw, int8 sgi_tx, int8 ldpc_tx, int8 vht_ldpc_tx,
	uint8 vht_ratemask, uint8 active_antcfg_num, uint8 antselid_init,
	uint32 max_rate, uint32 min_rate)
{
	uint init_rate = 0;

	if (state == NULL) {
		ASSERT(0);
		return;
	}

	bzero((char *)state, sizeof(rcb_t));

	/* store pointer to ratesel module */
	state->rsi = rsi;
	state->scb = scb;

	state->clronupd = clronupd;
	INVALIDATE_TXH_CACHE(state);

#ifdef WL11N
	state->bwcap = bw;
	/* SGI: Init whether we can use SGI for single and dual stream rates */
	if (sgi_tx == AUTO) {
		state->has_sgi = TRUE;
	}
#endif /* WL11N */

#ifdef WL11AC
	/* LDPC,  distinguish between the VHT and not-VHT STA */
	if ((vht_ldpc_tx == ON) || (vht_ldpc_tx == AUTO)) {
		state->vht_ldpc = TRUE;
	} else {
		state->vht_ldpc = FALSE;
	}
	state->vht_ratemask = vht_ratemask;
#endif // endif

	/* init rate set: have to do after initing bw */
	wlc_ratesel_filter(state, rateset, max_rate, min_rate, init_rate);

#ifdef WL11N
	/* in extended spatial probing, set probe antennas based on initial antselid */
	state->active_antcfg_num = active_antcfg_num;
	if (active_antcfg_num > 1) {
		state->spmode = SP_EXT;
		state->antselid = antselid_init;
		state->antselid_extsp_ipr =
			MODINCR(antselid_init, state->active_antcfg_num);
		state->antselid_extsp_xpr = antselid_init;
		state->extsp_ixtoggle = I_PROBE;
	}

	/* init default antenna config selection */
	wlc_ratesel_init_defantsel(state, active_antcfg_num, antselid_init);

	/* init max number of short AMPDUs when going up in rate */
	state->rsi->max_short_ampdus = MAX_SHORT_AMPDUS;

	/* init psri to zeros */
	bzero((uchar*) state->psri, PSR_ARR_SIZE * sizeof(psr_info_t));
	state->psri[PSR_CUR_IDX].psr = PSR_UNINIT;

	/* init spatial probing family */
	if (state->spmode == SP_NORMAL) {
		state->mcs_sp = -1;
		state->mcs_sp_col = SPATIAL_MCS_COL1;
		/* if start from 2 or 3 stream rates, probe between 2 and 3 stream rates */
		if (state->mcs_streams >= MCS_STREAM_MAX_3 &&
		    wlc_ratespec_nss(RATESPEC_OF_I(state, state->rateid) > 1))
			state->mcs_sp_col = SPATIAL_MCS_COL2;
	}

	WL_RATE(("%s: start_rateid %d, antsel init [num id] [%d %d], sgi %s\n",
		__FUNCTION__, state->rateid, active_antcfg_num, antselid_init,
		(sgi_tx == AUTO) ? "AUTO" : "OFF"));
#else
	WL_RATE(("%s: start_rateid %d\n", __FUNCTION__, state->rateid));
#endif  /* WL11N */

	if (WL_RATE_ON())
		wlc_ratesel_dump_rateset(state, NULL);

	/* After setting up rateset and sp_mode, load the parameters for current rate */
	wlc_ratesel_load_params(state);
	state->lastxs_time = NOW(state);
	state->lastxs_rssi = WLC_RSSI_INVALID;

#ifdef WL_LPC
	if (LPC_ENAB(rsi->wlc))
		wlc_ratesel_lpc_init(state);
#endif // endif
} /* wlc_ratesel_init */

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
	/*
	 * - bump the epoch
	 * - clear the skip count
	 * - clear the update count
	 * - set the bitfield to all 0's
	 * - set the counts to 0
	 * - lookup the new up/down decision thresholds
	 */
	if (change_epoch)
		state->epoch = MODINCR(state->epoch, 1 + (TXFID_RATE_MASK >> TXFID_RATE_SHIFT));

	state->nskip = 0;
	state->nupd = 0;
#ifdef WL11N
	state->mcs_nupd = 0;
	state->cur_sp_nupd = 0;
#else
	bzero((uchar *)state->nofb, sizeof(state->nofb));
	state->nofbtotdn = 0;
	state->nofbtotup = 0;
	state->nofbid = 0;
#endif /* WL11N */

	wlc_ratesel_load_params(state);

	/* clear the external 'clear-on-update' variable */
	INVALIDATE_TXH_CACHE(state);

	return;
}

static void
wlc_ratesel_load_params(rcb_t *state)
{
#ifdef WL11N
	psr_info_t *upp = NULL, *cur = NULL, *dnp = NULL, *fbr = NULL;
	ratespec_t cur_rspec;
	int mcs, tblidx;

	cur_rspec = CUR_RATESPEC(state);

#ifdef WL11AC
	mcs = IS_MCS(cur_rspec) ?
		RSPEC_VHT_MCS_MASK & cur_rspec : -1;
	tblidx = wlc_ratespec_nss(cur_rspec) - 1;
	tblidx = 10 * tblidx + mcs + 1;
#else
	mcs = IS_MCS(cur_rspec) ? (int)(RSPEC_RATE_MASK & cur_rspec) : -1;
	tblidx = mcs + 1;
#endif // endif
	ASSERT(tblidx >= 0 && tblidx < RATES_NUM_MCS);

	/* get params for spatial probing */
	if (SP_MODE(state) == SP_NORMAL) {
		int mcs_sp = -1, mcs_sp_id = -1;
		uint8 tmp_col = state->mcs_sp_col;
		if (mcs != -1) {
			ASSERT(state->mcs_sp_col == SPATIAL_MCS_COL1 ||
			       state->mcs_sp_col == SPATIAL_MCS_COL2);
			/* switch only if the current one is not valid */
			mcs_sp = mcs_sp_params[tblidx][state->mcs_sp_col];
			if (mcs_sp == -1) {
				if (state->mcs_streams >= MCS_STREAM_MAX_3)
					tmp_col = (state->mcs_sp_col == SPATIAL_MCS_COL1) ?
						SPATIAL_MCS_COL2 : SPATIAL_MCS_COL1;
				mcs_sp = mcs_sp_params[tblidx][tmp_col];
			}
		}
		if (mcs_sp != -1) {
			mcs_sp_id = wlc_ratesel_mcsbw2id(state, (uint8) mcs_sp,
				state->bw[state->rateid]);
		}

		state->mcs_sp_flgs = 0; /* clear all the flags */
		if (mcs_sp_id >= 0) {
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
			state->mcs_sp_K = (uint8) mcs_sp_params[tblidx][SPATIAL_K_COL];
			state->mcs_sp_Pmod = (uint8) mcs_sp_params[tblidx][SPATIAL_PMOD_COL];
			state->mcs_sp_Poff = (uint8) mcs_sp_params[tblidx][SPATIAL_POFF_COL];
			RL_SP1(("%s: mcs 0x%x sp_col %d mcs_sp 0x%x sp_id %d\n", __FUNCTION__,
				mcs, state->mcs_sp_col, state->mcs_sp, mcs_sp_id));

		} else { /* don't probe at legacy rates or this mcs is not allowed */
			state->mcs_sp_id = 0xFF; /* this is dummy entry */
			state->mcs_sp_flgs &= ~SPFLGS_VAL_MASK;
		}

	} else if (SP_MODE(state) == SP_EXT) {
		/* X-Probe */
		int mcs_extsp_xpr = -1, mcs_extsp_xpr_sel = -1;

		if (mcs != -1)
			mcs_extsp_xpr = mcs_extsp_params[tblidx][EXTSPATIAL_MCS_COL];
		if (mcs_extsp_xpr > 0)
			mcs_extsp_xpr_sel = wlc_ratesel_mcsbw2id(state,
				(uint8) mcs_extsp_xpr, state->bw[state->rateid]);

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
		state->extsp_K = (uint8) mcs_extsp_params[tblidx][EXTSPATIAL_K_COL];
		state->extsp_Pmod = (uint16) mcs_extsp_params[tblidx][EXTSPATIAL_PMOD_COL];
		state->extsp_Poff = (uint16) mcs_extsp_params[tblidx][EXTSPATIAL_POFF_COL];

		RL_SP1(("%s: mcs %x tblidx %d xpr %x sel %x flgs %x K %d pmod %d poff %d\n",
			__FUNCTION__, mcs, tblidx, mcs_extsp_xpr, mcs_extsp_xpr_sel,
			state->extsp_flgs, state->extsp_K, state->extsp_Pmod, state->extsp_Poff));

	}

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

	print_psri(state);

#else /* WL11N */
	{
	uint8 rateidx = state->select_rates[state->rateid];
	/* udpate decision parameters */
	state->Mup = M_params[RATE_UP_ROW][rateidx];
	state->Mdn = M_params[RATE_DN_ROW][rateidx];
	state->Kup = K_params[RATE_UP_ROW][rateidx];
	state->Kdn = K_params[RATE_DN_ROW][rateidx];

	{
	uint8 up = 0, dn = 0;
	dn = state->select_rates[LIMDEC(state->rateid, 0)];
	up = state->select_rates[LIMINC((int)state->rateid, RATES_NUM_CCKOFDM)];
	RL_MORE(("%s: cr %sx%02x dn %sx%02x up %sx%02x rateidx %d"
		 " Mdn %d Mup %d Kdn %d Kup %d e %d\n", __FUNCTION__,
		 IS_MCS(known_rspec[rateidx]) ? "m" : "",
		 RSPEC_RATE_MASK & known_rspec[rateidx],
		 IS_MCS(known_rspec[dn]) ? "m" : "", RSPEC_RATE_MASK & known_rspec[dn],
		 IS_MCS(known_rspec[up]) ? "m" : "", RSPEC_RATE_MASK & known_rspec[up],
		 rateidx, state->Mdn, state->Mup,
		 state->Kdn, state->Kup, state->epoch));
	}
	}
#endif /* WL11N */
	return;
} /* wlc_ratesel_load_params */

#ifdef WL11N
void
wlc_ratesel_aci_change(ratesel_info_t *rsi, bool aci_state)
{
	if (rsi->ema_fixed)
		return;

	if (aci_state) {
		RL_INFO(("%s: aci ON. alpha0: %d --> 4, alpha: %d --> 5\n",
			__FUNCTION__, rsi->psr_ema_alpha0, rsi->psr_ema_alpha));
	} else {
		RL_INFO(("%s: aci OFF. alpha0: %d --> %d, alpha: %d --> %d\n",
			__FUNCTION__, rsi->psr_ema_alpha0, RATESEL_EMA_ALPHA0_DEFAULT,
			rsi->psr_ema_alpha, RATESEL_EMA_ALPHA_DEFAULT));
	}

	if (aci_state) {
		rsi->psr_ema_alpha0 = 4;
		rsi->psr_ema_alpha = 5;
	} else {
		rsi->psr_ema_alpha0 = RATESEL_EMA_ALPHA0_DEFAULT;
		rsi->psr_ema_alpha = RATESEL_EMA_ALPHA_DEFAULT;
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
			state->psri[PSR_CUR_IDX].rspec, state->rateid, state->sgi));
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
wlc_ratesel_upd_spstat(rcb_t *state, bool blockack, uint8 sp_mcs, uint ntx_succ, uint ntx)
{
	if (state->mcs_sp_id == 0xFF || sp_mcs == MCS_INVALID || ntx == 0)
		return FALSE;
	/* if # of streams being probed doesn't match current mcs_sp, discard */
	if ((sp_mcs & 0x78) != (state->mcs_sp & 0x78))
		return FALSE;

	if (state->rsi->ratesel_sp_algo) { /* throughput-based sp algo */

		uint32 cu_rate, sp_rate;
		uint32 thrt0, thrt1;
		ratespec_t sp_rspec;

		/* update throughput estimate. Don't count sgi in computing cu_rate */
#ifdef WL11AC
		sp_rspec = VHT_RSPEC(sp_mcs & RSPEC_VHT_MCS_MASK,
			(sp_mcs & RSPEC_VHT_NSS_MASK) >> RSPEC_VHT_NSS_SHIFT);
#else
		sp_rspec = HT_RSPEC(sp_mcs);
#endif // endif
		sp_rspec |= (state->bwcap << RSPEC_BW_SHIFT);
		sp_rate = RSPEC2RATE500K(sp_rspec);
		cu_rate = RSPEC2RATE500K(RATESPEC_OF_I(state, state->rateid));

		thrt0 = (ntx_succ << RATESEL_EMA_NF) * sp_rate / ntx;
		thrt1 = state->psri[PSR_CUR_IDX].psr * cu_rate;

		/* In case of different rate, scale down the higher one
		 * since big PER difference can lead to poor aggregation density.
		 */
		if (cu_rate > sp_rate)
			thrt1 = (thrt1 * 29) >> 5; /* 29/32 ~= 0.90625 */
		else if (cu_rate < sp_rate)
			thrt0 = (thrt0 * 29) >> 5;

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
			state->sp_nupd, RSPEC_RATE_MASK & CUR_RATESPEC(state), cu_rate,
			RSPEC_RATE_MASK & sp_rspec, sp_rate, thrt1, thrt0,
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
		cur_mcs = RSPEC_RATE_MASK & cur_rspec;
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
		valid_probe = (cur_mcs != sp_mcs && (cur_mcs & 0x78) != (sp_mcs & 0x78) &&
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
	if (state->gotpkts == GOTPKTS_ALL)
		ncf_lmt = rsi->ncf_lmt;
	else if (state->gotpkts & GOTPKTS_INIT)
		ncf_lmt = rsi->ncf_lmt >> 1;
	else
		ncf_lmt = NCFAIL_LMT0;
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
		state->sgi = RSPEC_ISSGI(down_rspec) ? TRUE : FALSE;
		if (SP_MODE(state) == SP_EXT) {
			if (IS_MCS(prv_rspec) && (RSPEC_RATE_MASK & prv_rspec) == MCS8) {
				wlc_ratesel_clear_extspstat(state);
				state->antselid_extsp_ipr = MODINCR(state->antselid,
					state->active_antcfg_num);
				state->antselid_extsp_xpr = state->antselid; /* reset prb */
			}
		}

#if defined(AP) || defined(WLDPT) || defined(WLTDLS)
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
				state->sgi = FALSE;
				RL_CHG(("%s hop to sp id 0x%x thrt 0x%x sp_nupd %d. "
					 "DN rate %d thrt 0x%x\n", __FUNCTION__,
					 state->last_mcs_sp_id, state->mcs_sp_thrt0,
					 state->sp_nupd, prate_dn, prate_dn * psr_dn));
			}
		}
	}
	else
		RL_MORE(("%s: rate_cur/fbr/dn %d %d %d psr_cur/fbr/dn %d %d %d nthrt_cur/dn "
			"0x%x 0x%x\n", __FUNCTION__, prate_cur, prate_fbr, prate_dn, cur->psr,
			fbr->psr, psr_dn, prate_cur * cur->psr, prate_dn * psr_dn));

	return decision;
} /* wlc_ratesel_godown */

static bool
wlc_ratesel_goup(rcb_t *state)
{
	bool decision = FALSE, use_hist = FALSE;
	uint32 psr_up, prate_cur, prate_up, prate_fbr;
	uint8 up_rateid = 0xFF;
	ratespec_t up_rspec, cur_rspec;
	psr_info_t *fbr, *cur, *upp;

	cur = (psr_info_t*)(state->psri + PSR_CUR_IDX);
	upp = (psr_info_t*)(state->psri + PSR_UP_IDX);
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

	if ((prate_cur != prate_fbr) && !use_hist) {
		if (fbr->psr <= cur->psr) {
			/* if PSR at fbr is even lower than that at the current rate */
			psr_up = cur->psr;
		} else {
			/* derive psr_up from fallback rate using linear extrapolation */

			/* have to assume certain monitonicity */
			ASSERT(prate_up > prate_cur && prate_cur > prate_fbr);
			psr_up = (cur->psr * (prate_up - prate_fbr)
				  - fbr->psr * (prate_up - prate_cur)) / (prate_cur - prate_fbr);

			if (psr_up > PSR_MAX)
				psr_up = 0; /* negative */
		}
	}
	else if (use_hist) {
		RL_MORE(("%s: psr_up_recent %u\n", __FUNCTION__, psr_up));
	}

	if (prate_fbr == prate_cur && !use_hist) {
		/* probe once a while regardless the PSR. */
		decision = TRUE;
	} else {
		/* throughput criteria */
		decision = (prate_up * psr_up) > prate_cur * cur->psr;
		if (psr_up > PSR_MAX)
			WL_ERROR(("%s: psr_up %d exceeds maximum %d\n",
				__FUNCTION__, psr_up, PSR_MAX));
		ASSERT(psr_up <= PSR_MAX);
	}

	/* action time */
	if (decision) {
		/* change rate FIRST and do clean up work if necessary */
		ratespec_t prv_rspec = CUR_RATESPEC(state);
		uint8 prev_mcs = RSPEC_RATE_MASK & prv_rspec;
		state->rateid = up_rateid;
		state->sgi = RSPEC_ISSGI(up_rspec) ? TRUE : FALSE;
		if (IS_MCS(prv_rspec) && (prev_mcs == MCS12 || prev_mcs == MCS21) &&
		    (state->mcs_sp == -1 || state->mcs_sp < prev_mcs)) {
			if (SP_MODE(state) == SP_EXT &&
			    state->extsp_ixtoggle == X_PROBE) {
				/* always go from X to fresh I probe on way to mcs13 */
				WL_RATE(("%s: clearing extsp stats on way to m13/22\n",
					__FUNCTION__));
				wlc_ratesel_clear_extspstat(state);
				state->extsp_ixtoggle = I_PROBE;
			}
			else if (SP_MODE(state) == SP_NORMAL) {
				WL_RATE(("%s: clearing sp stats on way to m13/22\n", __FUNCTION__));
				wlc_ratesel_clear_spstat(state);
			}
		}

#if defined(AP) || defined(WLDPT) || defined(WLTDLS)
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
		state->mcs_short_ampdu = TRUE;

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
	}
	else {
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
	return decision;
} /* wlc_ratesel_goup */

static bool
wlc_ratesel_start_probe(rcb_t *state)
{
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
				RL_SP1(("nupd %d start spatial probe to id %d (pri: %d) "
					"[%d, %d %d]\n", state->nupd,
					state->mcs_sp_id - state->mcs_baseid,
					state->rateid - state->mcs_baseid, state->mcs_sp_K,
					state->mcs_sp_Pmod, state->mcs_sp_Poff));
				return TRUE;
			}
		}
	} else if (SP_MODE(state) == SP_EXT && (state->extsp_flgs & EXTSPFLGS_EN_MASK) == 0) {
		/* don't start if already started */
		ASSERT(state->extsp_Pmod != 0);
		if (state->extsp_Pmod == 0)
			return FALSE;
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
				cur_rate = RSPEC_RATE_MASK & cur_rspec;
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
						 RSPEC_RATE_MASK & extsp_rspec,
						 state->antselid_extsp_xpr, cur_rate,
						 state->antselid, state->extsp_K,
						 state->extsp_Pmod, state->extsp_Poff));
				}
				}
			return TRUE;
			}
		}
	}

	return FALSE;
}

void
wlc_ratesel_probe_ready(rcb_t *state, uint16 frameid,
	bool is_ampdu, uint8 ampdu_txretry)
{
	/* stop probing as soon as first probe is ready for DMA */
	if (ampdu_txretry == 0) {

		/* clear the external 'clear-on-update' variable */
		if ((state->mcs_sp_flgs & SPFLGS_EN_MASK) == 1)
			INVALIDATE_TXH_CACHE(state);

		/* stop spatial probe */
		state->mcs_sp_flgs &= ~SPFLGS_EN_MASK;

		/* clear the external 'clear-on-update' variable */
		if ((state->extsp_flgs & EXTSPFLGS_EN_MASK) == 1)
			INVALIDATE_TXH_CACHE(state);

		/* stop spatial probe */
		state->extsp_flgs &= ~EXTSPFLGS_EN_MASK;

		RL_SP0(("%s: frameid x%04x\n", __FUNCTION__, frameid));
	}
	RL_SP0(("%s: ampdu_txretry %d mcs_sp_flgs 0x%x extsp_flgs 0x%x\n",
		__FUNCTION__, ampdu_txretry, state->mcs_sp_flgs, state->extsp_flgs));
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
				         RSPEC_RATE_MASK & prv_rspec, RSPEC2BW(prv_rspec),
				         RSPEC_RATE_MASK & cur_rspec, RSPEC2BW(cur_rspec),
				         state->rateid, RSPEC2RATE500K(cur_rspec),
				         state->nupd, state->sp_nupd, state->nskip,
				         state->sp_nskip,
				         state->mcs_sp_thrt1, state->mcs_sp_thrt0));
			} else {
				WL_RATE(("rate_changed(spatially): mx%02xb%d to mx%02xb%d (%d %d) "
				         "after %2u (%u) txs with %u (%u) skipped."
				         "mcs_sp_statc %d M %d K %d\n",
				         RSPEC_RATE_MASK & prv_rspec, RSPEC2BW(prv_rspec),
				         RSPEC_RATE_MASK & cur_rspec, RSPEC2BW(cur_rspec),
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
		int mcs, tblidx, mcs_sp, mcs_sp_id = -1;
		uint8 tmp_col;
		/* the current rate has to be mcs so need to check */
#ifdef WL11AC
		mcs = RSPEC_VHT_MCS_MASK & cur_rspec;
		tblidx = wlc_ratespec_nss(cur_rspec) - 1;
		tblidx = 10 * tblidx + mcs + 1;
#else
		mcs = cur_rspec & RSPEC_RATE_MASK;
		tblidx = mcs + 1;
#endif // endif
		if (tblidx < 0 || tblidx >= RATES_NUM_MCS)
			return FALSE;

		if (state->mcs_streams >= MCS_STREAM_MAX_3) {
			/* swap only if supported nss >= 3 */
			tmp_col = (state->mcs_sp_col == SPATIAL_MCS_COL1) ?
				SPATIAL_MCS_COL2 : SPATIAL_MCS_COL1;
		} else
			tmp_col = SPATIAL_MCS_COL1;

		if ((mcs_sp = mcs_sp_params[tblidx][tmp_col]) != -1) {
			mcs_sp_id = wlc_ratesel_mcsbw2id(state, (uint8) mcs_sp,
				state->bw[state->rateid]);
		}
		if (mcs_sp_id > 0) {
			/* switch */
			RL_SP0(("%s: switching probing stream family: 0x%x --> 0x%x\n",
				__FUNCTION__, state->mcs_sp, mcs_sp));
			state->mcs_sp_col = tmp_col;
			state->mcs_sp = (uint8)mcs_sp;
			state->mcs_sp_id = (uint8) mcs_sp_id;
			wlc_ratesel_clear_spstat(state);
		}
	}
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
					"thrt_pri/sp = 0x%x 0x%x\n", __FUNCTION__, RSPEC_RATE_MASK &
					known_rspec[state->select_rates[state->extsp_xpr_id]],
					state->antselid_extsp_xpr, state->sp_nupd,
					state->extsp_thrt1, state->extsp_thrt0));
			else
				RL_SP1(("%s: aborting X-PROBE, probe mcs/antselid = 0x%x %d "
					 "nupd/statc/K = %d %d %d\n", __FUNCTION__, RSPEC_RATE_MASK&
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
				__FUNCTION__, state->antselid_extsp_xpr, RSPEC_RATE_MASK &
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
				 IS_MCS(prv_rspec) ? "m" : "", RSPEC_RATE_MASK & prv_rspec,
				 IS_MCS(cur_rspec) ? "m" : "", RSPEC_RATE_MASK & cur_rspec,
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

#else /* WL11N state clear-up, update, change rate */

static void
wlc_ratesel_upd_windows(rcb_t *state, bool up)
{
	uint32 old_valid;
	int byte_id, bit_id, id;
	uint8 window_size;

	window_size = up ? state->Mup : state->Mdn;
	id = state->nofbid - window_size - 1;
	if (id < 0)
		id += MAXRATERECS;

	byte_id   = RATEREC_BYTE(id);
	bit_id    = RATEREC_BITS(id);
	old_valid =  state->nofb[byte_id] & (1 << bit_id);
	if (old_valid) {
		if (up)
			state->nofbtotup--;
		else
			state->nofbtotdn--;
	}

	RL_MORE(("%s: id %d byte_id %d bit_id %d old_valid %u tot %d\n", __FUNCTION__,
		id, byte_id, bit_id, old_valid, (up ? state->nofbtotup : state->nofbtotdn)));
}

/** Function to first determine whether to go down in the rateset select_rates[]; and then do it. */
static bool
#if RSSI_PRI_ON
wlc_ratesel_godown(rcb_t *state, uint8 down_rateid)
#else
wlc_ratesel_godown(rcb_t *state)
#endif // endif
{
	if ((state->rateid > 0) &&
	    ((MIN(state->Mdn, state->nupd) - state->nofbtotdn) >= state->Kdn)) {
		state->rateid --;
		RL_MORE(("%s: rate DOWN. nupd %u nofbtotdn %d Mdn %d Kdn %d\n",
			__FUNCTION__,
			state->nupd, state->nofbtotdn, state->Mdn, state->Kdn));
		return TRUE;
	}
	return FALSE;
}

static bool
wlc_ratesel_goup(rcb_t *state)
{
	if ((state->rateid < state->active_rates_num-1) && (state->nupd >= state->Mup) &&
	    ((state->Mup - state->nofbtotup) <= state->Kup)) {
			state->rateid ++;
			RL_MORE(("%s: rate UP. nupd %u nofbtotup %d Mup %d Kup %d\n", __FUNCTION__,
				state->nupd, state->nofbtotup, state->Mup, state->Kup));
			return TRUE;
	}
	return FALSE;
}
#endif /* WL11N */

/**
 * <is_probe>: Indicate this calling is due to a probe packet or not.
 */
static void BCMFASTPATH
wlc_ratesel_pick_rate(rcb_t *state, bool is_probe)
{
	uint8 prv_rateid;
	ratespec_t cur_rspec, prv_rspec;
#ifdef WL11N
	bool prv_sgi;
#endif // endif
	bool change_epoch = FALSE, change_rate = FALSE;

	/* sanity check */
	ASSERT(state->rateid < state->active_rates_num);

	prv_rateid = state->rateid;
	prv_rspec = CUR_RATESPEC(state);

#ifdef WL11N
	prv_sgi = state->sgi;
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
		if (change_rate)
			state->sgi = FALSE;
	} else
#endif /* WL11N */
	{
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

#ifdef WL11N
		if (!change_rate && wlc_ratesel_start_probe(state))
			INVALIDATE_TXH_CACHE(state);
#endif // endif

		if (WL_RATE_ON() && change_rate) {
			/* the rate has changed, vertically */
			uint8 cur_rate;
			cur_rspec = CUR_RATESPEC(state);

			ASSERT(cur_rspec != prv_rspec);
			cur_rate = RSPEC_RATE_MASK & cur_rspec;

			if (prv_rateid != state->rateid) {
				/* the rate has just changed */
				WL_RATE(("rate_changed: %sx%02xb%d %s %sx%02xb%d (%d %d) after "
					"%2u used and %u skipped txs epoch %d\n",
					IS_MCS(prv_rspec) ? "m" : "", RSPEC_RATE_MASK & prv_rspec,
					RSPEC2BW(prv_rspec),
					(prv_rateid > state->rateid) ? "DN" : "UP",
					IS_MCS(cur_rspec) ? "m" : "", cur_rate,
					RSPEC2BW(cur_rspec), state->rateid,
					RSPEC2RATE500K(cur_rspec), state->nupd,
					state->nskip, state->epoch));
#ifdef WL11N
			} else if (prv_sgi != state->sgi) {
				/* the rate is changed due to sgi on/off */
				WL_RATE(("rate_changed(sgi): mx%02xb%d (%d %d) sgi %d to %d "
					"after %2u used and %u skipped txs epoch %d\n", cur_rate,
					state->rateid, RSPEC2RATE500K(cur_rspec),
					RSPEC2BW(cur_rspec), prv_sgi, state->sgi, state->nupd,
					state->nskip, state->epoch));
#endif /* WL11N */
			} else {
				/* the rate has just changed due to a rateset change */
				WL_RATE(("rate_changed(from-oob): %sx%02x DN %sx%02x (%d %d) "
					"after %2u used and %u skipped txs\n",
					IS_MCS(prv_rspec) ? "m" : "", RSPEC_RATE_MASK & prv_rspec,
					IS_MCS(cur_rspec) ? "m" : "", cur_rate, state->rateid,
					RSPEC2RATE500K(cur_rspec), state->nupd, state->nskip));
			}
			BCM_REFERENCE(cur_rate);
		}
	}

	if (change_rate) {
		wlc_ratesel_clear_ratestat(state, change_epoch);
	}
	return;
}

#ifdef WL11N
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
		INVALIDATE_TXH_CACHE(state);
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
		WL_RATE(("%s gotpkts %d -> %d\n", __FUNCTION__, gotpkts_old, state->gotpkts));
		RL_MORE(("time: %d -> %d rssi: %d -> %d\n", state->lastxs_time, NOW(state),
			state->lastxs_rssi, rssi_cur));
	}
#endif // endif
}
#else
static void BCMFASTPATH
wlc_ratesel_alert_upd(rcb_t *state)
{
	if (state->gotpkts != GOTPKTS_ALL) {
		state->gotpkts = GOTPKTS_ALL;
		INVALIDATE_TXH_CACHE(state);
	}
}
#endif /* WL11N */

/**
 * all of the following should only apply to directed traffic,
 * do not call me on group-addressed frames.
 * SFBL/LFBL is short/long retry limit for fallback rate
 */
static int
wlc_ratesel_use_txs(rcb_t *state, tx_status_t *txs, uint16 SFBL, uint16 LFBL,
	uint8 tx_mcs, uint8 antselid, bool fbr)
{
	uint8 nftx, nrtx;
	bool nofb, acked;
	int ret_val = TXS_DISC;
#ifdef WL_LPC
	uint32 old_psr = 0;
#endif // endif
#ifdef WL11N
	int ntx_start_fb;
	uint32 *p, *pfbr;
	uint8 alpha;
	uint8 cur_mcs, ntx_mrt;
	ratespec_t cur_rspec;

	/* Do sanity check before changing gotpkts */
	wlc_ratesel_sanitycheck_psr(state);
#else
	uint32 bit_mask, new_valid, old_valid;
	int byte_id, bit_id;
#endif /* WL11N */

	ASSERT(!TX_STATUS_UNEXP(txs->status));

	/* extract fields and bail if nothing was transmitted
	 * if the frame indicates <fbr> fake nrtx
	 */
	nrtx = fbr ? (SFBL + 1) :
		(uint8) ((txs->status.rts_tx_cnt));
	nftx = (uint8) ((txs->status.frag_tx_cnt));
	if ((nftx == 0) && (nrtx == 0))
		return ret_val;

	/* if this packet was received at the target rate, we
	 * can switch to the steady state rate fallback calculation of target rate -1
	 * AND it needs to clean the external 'clear-on-update' variable
	 */
	nofb = (nrtx > 0) ? (nrtx <= SFBL && nftx <= LFBL) : (nftx <= SFBL);
	acked = (txs->status.was_acked) ? TRUE : FALSE;
	if (nofb && acked) {
		wlc_ratesel_alert_upd(state);
	}

#ifdef WL11N
	/* Determine after which transmission the fall-back rate is used. */
	ntx_start_fb = (nrtx > SFBL) ? 0 : (nrtx == 0 ? SFBL : LFBL);

	cur_rspec = CUR_RATESPEC(state);
	cur_mcs = IS_MCS(cur_rspec) ? (RSPEC_RATE_MASK & cur_rspec) : MCS_INVALID;
	if (IS_SP_PKT(state, cur_mcs, tx_mcs)) {
		uint8 ntx_succ;
		ntx_mrt = MIN(nftx, ntx_start_fb); /* can be 0 */
		ntx_succ = (acked && nftx <= ntx_start_fb) ? 1 : 0;
		if (wlc_ratesel_upd_spstat(state, FALSE, tx_mcs, ntx_succ, ntx_mrt))
			ret_val = TXS_PROBE;
		RL_SP1(("fid 0x%x spatial probe rcvd: mx%02x (pri: mx%02x) [nrtx nftx ack] = "
			"[%d %d %d] Upd: %c\n", txs->frameid, tx_mcs, cur_mcs,
			nrtx, nftx, acked, (ret_val == TXS_PROBE) ? 'Y' : 'N'));
		return ret_val;
	}
	else if (IS_EXTSP_PKT(state, cur_mcs, tx_mcs, antselid)) {
		uint8 ntx_succ;
		ntx_mrt = MIN(nftx, ntx_start_fb); /* can be 0 */
		ntx_succ = (acked && nftx <= ntx_start_fb) ? 1 : 0;
		if (wlc_ratesel_upd_extspstat(state, FALSE, tx_mcs, antselid, ntx_succ, ntx_mrt))
			ret_val = TXS_PROBE;
		RL_SP1(("txs_ack: ext_spatial probe rcvd: %s%02x ant %d (pri: %s%02x ant %d) "
			"[nrtx nftx ack] = [%d %d %d] Upd: %c\n",
			(tx_mcs == MCS_INVALID) ? "" : "m", tx_mcs, antselid,
			(cur_mcs == MCS_INVALID) ? "" : "m", cur_mcs, state->antselid,
			nrtx, nftx, acked, (ret_val == TXS_PROBE) ? 'Y' : 'N'));
		return ret_val;
	}

	/* All transmissions before the last are failures. */
	alpha = state->gotpkts ? ((state->nupd < state->rsi->ema_nupd_thres) ?
		state->rsi->psr_ema_alpha0 : state->rsi->psr_ema_alpha) : RATESEL_EMA_ALPHA_INIT;
	pfbr = (state->psri[PSR_CUR_IDX].rspec == state->psri[PSR_FBR_IDX].rspec) ?
		&(state->psri[PSR_CUR_IDX].psr) : &(state->psri[PSR_FBR_IDX].psr);
	p = &(state->psri[PSR_CUR_IDX].psr);
#ifdef WL_LPC
	if (LPC_ENAB(state->rsi->wlc))
		old_psr = state->psri[PSR_CUR_IDX].psr;
#endif // endif
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
	if (pfbr == &(state->psri[PSR_FBR_IDX].psr))
		state->psri[PSR_FBR_IDX].timestamp = NOW(state);

	/* for fast drop */
	if (acked) {
		state->ncfails = 0;
	} else {
		state->ncfails ++;
		wlc_ratesel_rfbr(state);
	}
	RL_MORE(("%s fid 0x%x: rid %d %d psr_cur/fbr %d %d [nrtx nftx acked ant fbr] ="
		"[%d %d %d %d %d] nupd %u\n", __FUNCTION__, txs->frameid, state->rateid,
		RSPEC2RATE500K(CUR_RATESPEC(state)), state->psri[PSR_CUR_IDX].psr,
		state->psri[PSR_FBR_IDX].psr, nrtx, nftx,
		acked, antselid, fbr ? 1 : 0, state->nupd));

#else /* WL11N */
	/* Still PER-based for non-WL11N case */
	/* update bitfield, [0, 1] -> [fallback, no-fallback] */
	byte_id   = RATEREC_BYTE(state->nofbid);
	bit_id    = RATEREC_BITS(state->nofbid);
	bit_mask  = 1 << bit_id;

	new_valid = (nofb) ? bit_mask : 0;
	old_valid =  state->nofb[byte_id] & bit_mask;
	if (new_valid != old_valid)
		state->nofb[byte_id] ^= bit_mask;
	/* move pointer to next update position */
	state->nofbid = MODINCR(state->nofbid, MAXRATERECS);

	/* update up/down window totals */
	if (new_valid) {
		state->nofbtotdn++;
		state->nofbtotup++;
	}

	/* update down window totals */
	wlc_ratesel_upd_windows(state, FALSE);

	/* update up window totals */
	wlc_ratesel_upd_windows(state, TRUE);

	RL_MORE(("%s: nupd %u nrtx %u nftx %u nofb %d new_valid %u ACKed %d\n",
		__FUNCTION__, state->nupd, nrtx, nftx, nofb, new_valid, acked));
#endif /* WL11N */

	/* update the count of updates since the last state flush, saturating at max byte val */
	LIMINC_UINT32(state->nupd);
	return TXS_REG;
}

/** update per-scb state upon received tx status */
/* non-AMPDU txstatus rate update, default to use non-mcs rates only */
void
wlc_ratesel_upd_txstatus_normalack(rcb_t *state, tx_status_t *txs, uint16 sfbl, uint16 lfbl,
	uint8 tx_mcs, uint8 antselid, bool fbr)
{
	uint8 epoch;
	int txs_res = TXS_DISC;
	ASSERT(state != NULL);

#ifdef WL11AC
	tx_mcs = wlc_rate_ht2vhtmcs(tx_mcs);
#endif // endif

	/* extract epoch cookie from FrameID */
	epoch = (txs->frameid & TXFID_RATE_MASK) >> TXFID_RATE_SHIFT;

	if (epoch == state->epoch) {
#ifdef WL11N
		/* always update antenna histogram */
		wlc_ratesel_upd_deftxant(state, state->antselid);
#endif // endif
		txs_res = wlc_ratesel_use_txs(state, txs, sfbl, lfbl, tx_mcs, antselid, fbr);
		if (txs_res != TXS_DISC)
			wlc_ratesel_pick_rate(state, txs_res == TXS_PROBE);
	}
	else {
		RL_MORE(("%s: frm_e %d (id %x) [nrtx nftx acked ant] = [%u %u %d %d] nupd %u "
			"e %d\n", __FUNCTION__, epoch, txs->frameid,
			(uint8) ((txs->status.rts_tx_cnt)),
			(uint8) ((txs->status.frag_tx_cnt)),
			(txs->status.was_acked) ? 1 : 0, antselid, state->nupd, state->epoch));
	}

	if (txs_res == TXS_DISC)
		LIMINC_UINT32(state->nskip);
}

#ifdef WL11N
/**
 * all of the following should only apply to directed traffic,
 * do not call me on group-addressed frames.
 * Return TRUE to indicate that this is a blockack for probe.
 */
static int BCMFASTPATH
wlc_ratesel_use_txs_blockack(rcb_t *state, tx_status_t *txs, uint8 suc_mpdu, uint8 tot_mpdu,
	bool ba_lost, uint8 retry, uint8 fb_lim, uint8 tx_mcs, bool tx_error, uint8 antselid)
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
	ASSERT(IS_MCS(cur_rspec));
	cur_mcs = RSPEC_RATE_MASK & cur_rspec;
	ret_val = TXS_DISC;
	if (IS_SP_PKT(state, cur_mcs, tx_mcs)) {
		RL_SP1(("%s fid 0x%x: spatial probe rcvd: mx%02x (pri: mx%02x) "
			"[bal suc/tot_mpdu r l] = [%s %d/%d %d %d]\n",
			__FUNCTION__, txs->frameid, tx_mcs, cur_mcs,
			(ba_lost ? "yes" : "no"), suc_mpdu, tot_mpdu, retry, fb_lim));
		if (retry >= fb_lim) {
			LIMINC_UINT32(state->sp_nskip);
		} else {
			if (wlc_ratesel_upd_spstat(state, TRUE, tx_mcs, suc_mpdu, tot_mpdu))
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

	RL_MORE(("fid 0x%x gotpkts = %d [bal suc/tot_mpdu r l e] = [%s %d %d %d %d %d] nupd %d\n",
		txs->frameid, state->gotpkts, (ba_lost ? "yes" : "no"), suc_mpdu, tot_mpdu,
		retry, fb_lim, state->epoch, state->nupd));

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

	if (!state->rsi->usefbr && retry >= fb_lim)
		return TXS_DISC;

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
		INVALIDATE_TXH_CACHE(state);
	}

	/* Update per A-MPDU */
	p = ((retry < fb_lim) || (state->psri[PSR_FBR_IDX].rspec == cur_rspec)) ?
		&(state->psri[PSR_CUR_IDX].psr)
		: &(state->psri[PSR_FBR_IDX].psr);

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

	RL_MORE(("%s: rid %d %d psr_cur/fbr %d %d\n",
		__FUNCTION__, state->rateid, RSPEC2RATE500K(state->psri[PSR_CUR_IDX].rspec),
		state->psri[PSR_CUR_IDX].psr, state->psri[PSR_FBR_IDX].psr));

	return TXS_REG;
}

/** update state upon received BA */
void BCMFASTPATH
wlc_ratesel_upd_txs_blockack(rcb_t *state, tx_status_t *txs,
	uint8 suc_mpdu, uint8 tot_mpdu, bool ba_lost, uint8 retry, uint8 fb_lim, bool tx_error,
	uint8 tx_mcs, uint8 antselid)
{
	uint8 epoch;
	int txs_res;

	/* sanity check: mcs set may not be empty */
	if ((state == NULL) || (state->active_rates_num == 0)) {
		ASSERT(0);
		return;
	}

#ifdef WL11AC
	tx_mcs = wlc_rate_ht2vhtmcs(tx_mcs);
#endif // endif

	/* update history. extract epoch cookie from FrameID */
	epoch = (txs->frameid & TXFID_RATE_MASK) >> TXFID_RATE_SHIFT;
	txs_res = TXS_DISC;
	if (epoch == state->epoch) {
#ifdef BCMDBG
		/* to accommodate the discrepancy due to "wl nrate" in the debug driver */
		if (!IS_MCS(CUR_RATESPEC(state))) {
			WL_RATE(("%s: blockack received for non-MCS rate.\n", __FUNCTION__));
			return;
		}
#endif // endif

		/* always update antenna histogram */
		if (WLANTSEL_ENAB(rsi->wlc))
			wlc_ratesel_upd_deftxant(state, state->antselid);

		txs_res = wlc_ratesel_use_txs_blockack(state, txs, suc_mpdu, tot_mpdu,
			ba_lost, retry, fb_lim, tx_mcs, tx_error, antselid);
		if (txs_res != TXS_DISC)
			wlc_ratesel_pick_rate(state, txs_res == TXS_PROBE);
	}
	else {
		RL_MORE(("%s: frm_e %d (id %x) [bal suc tot_mpdu r l e] = [%s %d %d %d %d %d] "
			"mcs 0x%x ant %d\n", __FUNCTION__, epoch, txs->frameid,
			ba_lost ? "yes" : "no", suc_mpdu, tot_mpdu, retry, fb_lim,
			state->epoch, tx_mcs, antselid));
	}

	if (txs_res == TXS_DISC) {
		LIMINC_UINT32(state->nskip);
	}
}

#if (defined(WLAMPDU_MAC) || defined(D11AC_TXD))
/**
 * Following two functions are for the ampdu ucode/hw aggregation case
 */
static int
wlc_ratesel_use_txs_ampdu(rcb_t *state, uint16 frameid, uint mrt, uint mrt_succ,
	uint fbr, uint fbr_succ, bool tx_error, uint8 tx_mcs, uint8 antselid)
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
	ASSERT(IS_MCS(cur_rspec));
	cur_mcs = RSPEC_RATE_MASK & cur_rspec;
	ret_val = TXS_DISC;
	if (IS_SP_PKT(state, cur_mcs, tx_mcs)) {
		RL_SP1(("%s fid 0x%04x: spatial probe rcvd: mx%x (pri: mx%x) [mrt succ fbr succ] ="
			" [%d %d %d %d]\n", __FUNCTION__, frameid, tx_mcs, cur_mcs,
			mrt, mrt_succ, fbr, fbr_succ));
		if (mrt == 0) {
			LIMINC_UINT32(state->sp_nskip);
		} else {
			if (wlc_ratesel_upd_spstat(state, TRUE, tx_mcs, mrt_succ, mrt))
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

	RL_TXS(("fid 0x%04x gotpkts %d [mrt succ fbr succ nf e] = [%d %d %d %d %d %d] nupd %d\n",
		frameid, state->gotpkts, mrt, mrt_succ, fbr, fbr_succ,
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
	 * ncf:   rst |  ++ |  rst |  ++  |  ++
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

	if (!state->rsi->usefbr && mrt == 0)
		return TXS_DISC;

	/* this packet was received at the target rate, so we
	 * can switch to the steady state rate fallback calculation
	 * of target rate - 1
	 */
	if (mrt > 0 && (mrt_succ == mrt))
		wlc_ratesel_alert_upd(state);

	/* stop sending short A-MPDUs (if we received L frames) */
	if (state->mcs_short_ampdu && state->mcs_nupd >= state->rsi->max_short_ampdus) {
		state->mcs_short_ampdu = FALSE;
		INVALIDATE_TXH_CACHE(state);
	}

	/* Update per A-MPDU */
	alpha = state->gotpkts ? ((state->nupd <= state->rsi->ema_nupd_thres) ?
		state->rsi->psr_ema_alpha0 : state->rsi->psr_ema_alpha) : RATESEL_EMA_ALPHA_INIT;
	if (mrt > 0) {
		cur_succ = (mrt_succ << RATESEL_EMA_NF) / mrt;
		UPDATE_MOVING_AVG(&(state->psri[PSR_CUR_IDX].psr), cur_succ, alpha);
#ifdef WL_LPC
		if (LPC_ENAB(state->rsi->wlc)) {
			/* update the state */
			wlc_ratesel_upd_la(state, cur_succ, state->psri[PSR_CUR_IDX].psr);
		}
#endif /* WL_LPC */
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

	ASSERT(state->psri[PSR_CUR_IDX].psr <= PSR_MAX);
	ASSERT(state->psri[PSR_FBR_IDX].psr <= PSR_MAX);

	RL_TXS(("%s: rid %d %d psr_cur/fbr %d %d\n",
		__FUNCTION__, state->rateid, RSPEC2RATE500K(state->psri[PSR_CUR_IDX].rspec),
		state->psri[PSR_CUR_IDX].psr, state->psri[PSR_FBR_IDX].psr));

	return TXS_REG;
}

/**
 * The case that (mrt+fbr) == 0 is handled as RTS transmission failure.
 */
void BCMFASTPATH
wlc_ratesel_upd_txs_ampdu(rcb_t *state, uint16 frameid,
	uint mrt, uint mrt_succ, uint fbr, uint fbr_succ,
	bool tx_error, uint8 tx_mcs, uint8 antselid)
{
	uint8 epoch;
	int txs_res;

	/* sanity check: mcs set may not be empty */
	if ((state == NULL) || (state->active_rates_num == 0)) {
		ASSERT(0);
		return;
	}

	if (mrt + fbr == 0) {
		/* fake one transmission at main rate to obtain the expected update */
		mrt = 1;
		ASSERT(mrt_succ + fbr_succ == 0);
	}

#ifdef WL11AC
	tx_mcs = wlc_rate_ht2vhtmcs(tx_mcs);
#endif // endif

	/* update history. extract epoch cookie from FrameID */
	epoch = (frameid & TXFID_RATE_MASK) >> TXFID_RATE_SHIFT;
	txs_res = TXS_DISC;
	if (epoch == state->epoch) {
#ifdef BCMDBG
		/* to accommodate the discrepancy due to "wl nrate" in the debug driver */
		if (!IS_MCS(CUR_RATESPEC(state))) {
			WL_RATE(("%s: blockack received for non-MCS rate.\n", __FUNCTION__));
			return;
		}
#endif // endif

		/* always update antenna histogram */
		if (WLANTSEL_ENAB(rsi->wlc))
			wlc_ratesel_upd_deftxant(state, state->antselid);

		txs_res = wlc_ratesel_use_txs_ampdu(state, frameid, mrt, mrt_succ, fbr, fbr_succ,
			tx_error, tx_mcs, antselid);
		if (txs_res != TXS_DISC)
			wlc_ratesel_pick_rate(state, txs_res == TXS_PROBE);
	}
	else {
		RL_MORE(("%s: frm_e %d (fid %x) [mrt succ fbr succ e] = [%d %d %d %d %d] "
			"mcs 0x%02x ant %d e %d\n", __FUNCTION__, epoch, frameid, mrt,
			mrt_succ, fbr, fbr_succ, state->epoch, tx_mcs, antselid, state->epoch));
	}

	if (txs_res == TXS_DISC) {
		LIMINC_UINT32(state->nskip);
	}
}
#endif /* WLAMPDU_MAC || D11AC_TXD */

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

	rxant_id = (rxstatus2 >> RXS_RXANT_SHIFT) & RXS_RXANT_MASK;
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
#endif /* WL11N */

#ifdef WLATF
ratespec_t BCMFASTPATH
wlc_ratesel_rawcurspec(rcb_t *state)
{
	return CUR_RATESPEC(state);
}
#endif /* WLATF */

ratespec_t
wlc_ratesel_getcurspec(rcb_t *state)
{
	ratespec_t cur_rspec = CUR_RATESPEC(state);
#ifdef WL11AC
	if (IS_MCS(cur_rspec) && !state->vht) {
		/* convert the rspec from vht format to ht format */
		int mcs, nss;
		ratespec_t new_rspec;
		new_rspec =  ((RSPEC_SHORT_GI | RSPEC_BW_MASK) & cur_rspec) | RSPEC_ENCODE_HT;
		mcs = RSPEC_VHT_MCS_MASK & cur_rspec;
		nss = ((cur_rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT) - 1;
		cur_rspec = new_rspec | (8*nss + mcs);
	}
#endif // endif
	return cur_rspec;
}

/** select transmit rate given per-scb state */
void BCMFASTPATH
wlc_ratesel_gettxrate(rcb_t *state, uint16 *frameid,
	ratesel_txparams_t *cur_rate, uint16 *flags)
{
	uint8 rateid, fbrateid;
#ifdef D11AC_TXD
	uint8 tmp;
#endif /* D11AC_TXD */
#ifdef WL11N
	uint32 rspec_sgi;
#endif // endif

	wlc_ratesel_alert(state, (cur_rate->num > 2) ? TRUE : FALSE);

	/* clear all flags */
	*flags = 0;

#ifdef WL11N
	/* get initial values for antenna selection, prim might be overridden for probe */
	cur_rate->antselid[0] = state->antselid;
	cur_rate->antselid[1] = state->antselid;

	rspec_sgi = state->sgi ? RSPEC_SHORT_GI : 0;

#endif /* WL11N */

	rateid = state->rateid;

	/* drop a cookie containing the current rate epoch in the
	 * appropriate portion of the FrameID
	 */
	*frameid = *frameid & (~TXFID_RATE_MASK);
	*frameid = *frameid | ((state->epoch << TXFID_RATE_SHIFT) & TXFID_RATE_MASK);

	/* clear probe flag */
	*frameid = *frameid & (~TXFID_RATE_PROBE_MASK);

	/* sanity check */
	if (rateid >= state->active_rates_num) {
		rateid = state->active_rates_num - 1;
		state->rateid = rateid;
	}

	cur_rate->rspec[0] = RATESPEC_OF_I(state, rateid);

#ifdef WL11N

	/* fill in rates & antennas for the case of probes */
	if ((SP_MODE(state) == SP_NORMAL) && ((state->mcs_sp_flgs & SPFLGS_EN_MASK) == 1)) {

		rateid = state->mcs_sp_id; /* select spatial probe rate */
		*frameid = *frameid | TXFID_RATE_PROBE_MASK; /* flag probes */
		rspec_sgi = 0; /* don't probe with SGI in general */

		RL_SP1(("%s: probe frameid 0x%04x\n", __FUNCTION__, *frameid));

	} else if ((SP_MODE(state) == SP_EXT) &&
		((state->extsp_flgs & EXTSPFLGS_EN_MASK) == 1)) {
		if (state->extsp_ixtoggle == I_PROBE) {
			cur_rate->antselid[0] = state->antselid_extsp_ipr;
		} else {
			rateid = state->extsp_xpr_id;
			cur_rate->antselid[0] = state->antselid_extsp_xpr;
			rspec_sgi = 0; /* don't x-probe with SGI */
		}

		/* flag probes */
		*frameid = *frameid | TXFID_RATE_PROBE_MASK;
	}
	if (rateid != state->rateid && rateid >= state->active_rates_num) {
		rateid = state->active_rates_num - 1;
		state->rateid = rateid;
	}

	/* mark frames that should go out as short A-MPDUs */
	if (state->mcs_short_ampdu) {
		*flags |= RATESEL_VRATE_PROBE;
	}

	/* set primary rate */
	cur_rate->rspec[0] = RATESPEC_OF_I(state, rateid) | rspec_sgi;

#endif /* WL11N */

	/* fallback based on primary rate */
	fbrateid = wlc_ratesel_getfbrateid(state, state->rateid);
	cur_rate->rspec[1] = RATESPEC_OF_I(state, fbrateid);

#ifdef WL11N
	if (SP_MODE(state) == SP_EXT)
		RL_MORE(("%s: rspec_pri/fbr 0x%08x 0x%08x rate_pri/fbr %u %u ant_pri/fbr %d %d "
			"sgi %d fid 0x%04x\n", __FUNCTION__, cur_rate->rspec[0], cur_rate->rspec[1],
			RSPEC2RATE500K(cur_rate->rspec[0]), RSPEC2RATE500K(cur_rate->rspec[1]),
			cur_rate->antselid[0], cur_rate->antselid[1], state->sgi, *frameid));
	else
		RL_MORE(("%s: rspec_pri/fbr 0x%08x 0x%08x rate_pri/fbr %u %u sgi %d fid 0x%04x\n",
			__FUNCTION__, cur_rate->rspec[0], cur_rate->rspec[1],
			RSPEC2RATE500K(cur_rate->rspec[0]), RSPEC2RATE500K(cur_rate->rspec[1]),
			state->sgi, *frameid));
#else /* WL11N */
	RL_MORE(("%s: rspec_pri/fbr 0x%08x 0x%08x rate_pri/fbr %u %u\n",
		__FUNCTION__, cur_rate->rspec[0], cur_rate->rspec[1],
		RSPEC2RATE500K(cur_rate->rspec[0]), RSPEC2RATE500K(cur_rate->rspec[1])));
#endif /* WL11N */

#ifdef D11AC_TXD
	tmp = cur_rate->num;
#endif // endif
	if ((cur_rate->rspec[0] == cur_rate->rspec[1]) &&
	   (cur_rate->antselid[0] == cur_rate->antselid[1]))
		cur_rate->num = 1;
	else
		cur_rate->num = 2;

#ifdef D11AC_TXD
	/* obtain another two fallback rates only when being asked */
	if (tmp > 2) {
		rateid = fbrateid;
		while (cur_rate->num >= 2 && cur_rate->num < 4) {
			/*
			 * Get next fallback rate from lookup table:
			 * which is next rate down.
			 * Use the same antenna for all fallback rates.
			 */
			fbrateid = state->dnrid[fbrateid];
			if (fbrateid == rateid)
				break;
			if (cur_rate->num == 3 &&
			    (state->rsi->rfbr || (state->gotpkts & GOTPKTS_RFBR) == 0))
				/* reliable fallback: set the last rate to be lowest */
				fbrateid = 0;
			cur_rate->rspec[cur_rate->num] = RATESPEC_OF_I(state, fbrateid);
			cur_rate->antselid[cur_rate->num] = cur_rate->antselid[1];
			cur_rate->num ++;
			rateid = fbrateid;
		}
	}
#endif /* D11AC_TXD */

#ifdef WL11AC
	for (rateid = 0; rateid < cur_rate->num; rateid++) {
		ratespec_t cur_rspec = cur_rate->rspec[rateid];
		if (IS_MCS(cur_rspec) && !state->vht) {
			/* convert the rspec from vht format to ht format */
			int mcs, nss;
			mcs = RSPEC_VHT_MCS_MASK & cur_rspec;
			nss = ((cur_rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT) - 1;
			cur_rspec = RSPEC_ENCODE_HT | ((RSPEC_SHORT_GI|RSPEC_BW_MASK) & cur_rspec);
			cur_rate->rspec[rateid] = cur_rspec | (8*nss + mcs);
		}
		RL_MORE(("rate %d: rspec 0x%08x rate %d\n", rateid,
			cur_rate->rspec[rateid], RSPEC2RATE500K(cur_rate->rspec[rateid])));
	}
	if (cur_rate->num == 1) {
		/* copy to the first fallback rate anyways
		 * since the caller may not be using cur_rate->num for pre-ac d11hdr construction
		 */
		cur_rate->rspec[1] = cur_rate->rspec[0];
	}
#endif /* WL11AC */
	return;
} /* wlc_ratesel_gettxrate */

int wlc_ratesel_rcb_sz(void)
{
	return (sizeof(rcb_t));
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
	INVALIDATE_TXH_CACHE(state);
}
#endif /* WL_LPC */

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
#ifdef WL11N
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
#endif /* WL11N */
	return;
}

#if defined(BCM_HOST_MEM_RESTORE) && defined(BCM_HOST_MEM_SCB)
void
swap_cubby_ratesel_scb(rcb_t *scb_rate_cubby, struct scb* scb_new)
{
	scb_rate_cubby->scb = scb_new;
}
#endif // endif
