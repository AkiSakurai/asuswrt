/*
 * 802.11ax MU scheduler and scheduler statistics module.
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id$
 */

#ifdef WL_MUSCHEDULER

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <wlioctl.h>
#include <802.11.h>
#include <wl_dbg.h>
#include <wlc_types.h>
#include <wlc_pub.h>
#include <wlc.h>
#include <wlc_dump.h>
#include <wlc_iocv_cmd.h>
#include <wlc_bsscfg.h>
#include <wlc_scb.h>
#include <wlc_musched.h>
#include <wlc_ht.h>
#include <wlc_he.h>
#include <wlc_scb_ratesel.h>
#include <wlc_bmac.h>
#include <wlc_hw_priv.h>
#include <wlc_ratelinkmem.h>
#include <wlc_lq.h>
#include <wlc_ampdu_cmn.h>
#include <wlc_txbf.h>
#if defined(TESTBED_AP_11AX) || defined(BCMDBG)
#include <wlc_tx.h>
#endif /* TESTBED_AP_11AX || BCMDBG */
#include <wlc_macreq.h>
#include <wlc_txcfg.h>
#include <wlc_mutx.h>
#include <wlc_fifo.h>
#include <wlc_ampdu_rx.h>

/* forward declaration */
#define MUSCHED_RUCFG_ROW		16
#define MUSCHED_RUCFG_COL		16
#define MUSCHED_RU_TYPE_NUM		7
#define MUSCHED_RU_SCB_STATS_NUM	16
#define MUSCHED_RU_IDX_NUM		HE_MAX_2x996_TONE_RU_INDX /* 68 */
#define MUSCHED_RU_IDX_MASK		0x7f
#define MUSCHED_RU_IDX(ru)		((ru) & MUSCHED_RU_IDX_MASK)
#define MUSCHED_RU_UPPER_MASK		0x80
#define MUSCHED_RU_UPPER_SHIFT		7
#define MUSCHED_RU_UPPER(ru)	\
	(((ru) & MUSCHED_RU_UPPER_MASK) >> MUSCHED_RU_UPPER_SHIFT)

#define MUSCHED_RUCFG_LE8_NUM_ROWS	8
#define MUSCHED_RUCFG_GT8_NUM_ROWS	8
#define MUSCHED_RUCFG_LE8_NUM_COLS	8
#define MUSCHED_RUCFG_GT8_NUM_COLS	16
#define MUSCHED_RUCFG_GT8_STRT_IDX	8
#define MUSCHED_RUCFG_ACK_SHIFT		8

#define D11AX_RU26_TYPE		0
#define D11AX_RU52_TYPE		1
#define D11AX_RU106_TYPE	2
#define D11AX_RU242_TYPE	3
#define D11AX_RU484_TYPE	4
#define D11AX_RU996_TYPE	5
#define D11AX_RU1992_TYPE	6

/* Internal use by ucode for vht mumimo: */
#define MUSCHED_RUCFG_RSVD0_RTMEM		AMT_IDX_VHTMU_RSVD_START
#define MUSCHED_RUCFG_RSVD1_RTMEM		(AMT_IDX_VHTMU_RSVD_START + 1)
/* Used for DL OFDMA rates: */
#define MUSCHED_RUCFG_LE8_BW20_40_RTMEM		AMT_IDX_DLOFDMA_RSVD_START
#define MUSCHED_RUCFG_LE8_BW80_160_RTMEM	(AMT_IDX_DLOFDMA_RSVD_START + 1)
#define MUSCHED_RUCFG_GT8_BW20_RTMEM		(AMT_IDX_DLOFDMA_RSVD_START + 2)
#define MUSCHED_RUCFG_GT8_BW40_RTMEM		(AMT_IDX_DLOFDMA_RSVD_START + 3)
#define MUSCHED_RUCFG_GT8_BW80_RTMEM		(AMT_IDX_DLOFDMA_RSVD_START + 4)
#define MUSCHED_RUCFG_GT8_BW160_RTMEM		(AMT_IDX_DLOFDMA_RSVD_START + 5)

#define MUSCHED_RUCFG_TBL_WSZ		(MUSCHED_RUCFG_GT8_NUM_ROWS * MUSCHED_RUCFG_GT8_NUM_COLS)
#define MUSCHED_RUCFG_TBL_BSZ		(MUSCHED_RUCFG_TBL_WSZ * 2) // 256B

#define MUSCHED_RU_80MHZ		1
#define MUSCHED_RU_160MHZ		2
#define MUSCHED_RU_BMP_ROW_SZ		MUSCHED_RU_160MHZ
#define MUSCHED_RU_BMP_COL_SZ		((MUSCHED_RU_IDX_NUM - 1) / 8 + 1) /* 9 */

#define MUSCHED_26TONE_RU4	4 // 26-tone-ru 4
#define MUSCHED_26TONE_RU13	13 // 26-tone-ru 13
#define MUSCHED_26TONE_RU18	18 // center-26-tone-ru 18
#define MUSCHED_26TONE_RU23	23 // 26-tone-ru 23
#define MUSCHED_26TONE_RU32	32 // 26-tone-ru 32

/* for dl ofdma */
#define MUSCHED_DLOFDMA_MINUSER_SZ		2	/* 0: <=80 MHz 1: 160MHz */
#define MUSCHED_ULOFDMA_MIN_USERS		2
#define MUSCHED_DLOFDMA_BW20_MAX_NUM_USERS	8	/* Default DL OFDMA BW20 max clients */
#define MUSCHED_DLOFDMA_MAX_NUM_USERS		16	/* Default DL OFDMA max clients */

/* for ul ofdma */
#define MUSCHED_ULOFDMA_MAX_NUM_TRIGGER_USERS	8
#define MUSCHED_ULOFDMA_DFLT_NUM_TRIGGER_USERS	8
#define MUSCHED_ULOFDMA_INVALID_SCHPOS		-1
#define MUSCHED_ULOFDMA_MAX_DUR			5380
#define MUSCHED_ULOFDMA_AVG_TXDUR		0
#define MUSCHED_ULOFDMA_MAX_TXDUR		1
#define MUSCHED_ULOFDMA_MIN_TXDUR		2
#define MUSCHED_ULOFDMA_FIRST_USER		AMT_IDX_ULOFDMA_RSVD_START
#define MUSCHED_ULOFDMA_LAST_USER		\
	(MUSCHED_ULOFDMA_FIRST_USER + (AMT_IDX_ULOFDMA_RSVD_SIZE - 1))

#define OPERAND_SHIFT			4
#define MUSCHED_TRIGFMT			16 // trigger frame types
#define MUSCHED_UL_SCB_STATS_NUM	16
#define MAX_USRHIST_PERLINE		8
#define AVG_ALHA_SHIFT			3
#define AVG_ALPHA_MUL			((1 << AVG_ALHA_SHIFT) - 1)

#define MUSCHED_SCB_IDLEPRD		10	/* The default amount of seconds an scb can receive
						 * number of packets below packet count threshold
						 * before being evicted.
						 */
#define MUSCHED_SCB_RX_PKTCNT_THRSH	100	/* The default number of packets an scb must
						 * receive each second to be considered for
						 * UL-OFDMA.
						 */
#define MUSCHED_SCB_MIN_RSSI		-80	/* The default for the minimum rssi threshold for
						 * an scb to be considered for UL-OFDMA.
						 */

typedef enum {
	MUSCHED_SCB_INIT = 0,
	MUSCHED_SCB_ADMT = 1,
	MUSCHED_SCB_EVCT = 2
} ulmuScbAdmitStates;

typedef struct musched_ru_stats {
	uint32	tx_cnt[MUSCHED_RU_TYPE_NUM]; /* total tx cnt per ru size */
	uint32	txsucc_cnt[MUSCHED_RU_TYPE_NUM]; /* succ tx cnt per ru size */
	uint8	ru_idx_use_bmap[MUSCHED_RU_BMP_ROW_SZ][MUSCHED_RU_BMP_COL_SZ];
} musched_ru_stats_t;

typedef struct musched_ul_gstats {
	uint32 usrhist[MUSCHED_ULOFDMA_MAX_NUM_TRIGGER_USERS];	/* histogram of N_usr in trigger */
	uint32 lcnt[AMPDU_MAX_HE];		/* total lfifo cnt per mcs and nss */
	uint32 gdfcscnt[AMPDU_MAX_HE];		/* total good FCS cnt per mcs and nss */
	uint32 tx_cnt[MUSCHED_RU_TYPE_NUM];	/* total tx cnt per ru size */
	uint32 txsucc_cnt[MUSCHED_RU_TYPE_NUM];	/* succ tx cnt per ru size */
	uint8 ru_idx_use_bmap[MUSCHED_RU_BMP_ROW_SZ][MUSCHED_RU_BMP_COL_SZ];
} musched_ul_gstats_t;

typedef struct musched_ul_rssi_stats {
	int16 min_rssi; /* min rssi */
	int16 max_rssi; /* max rssi */
	int32 avg_rssi; /* avg rssi */
} musched_ul_rssi_stats_t;

typedef struct musched_ul_stats {
	uint32 trigtp_cnt[MUSCHED_TRIGFMT];	/* placeholder cnts for trigger types */
	uint32 qncnt;				/* total qos-null cnts */
	uint32 agglen;				/* sum mpdu len */
	uint32 nupd;				/* total txstatus upd */
	uint32 nfail;				/* counter for lcnt = 0 */
	uint32 nbadfcs;				/* counter for badfcs > 0 */
	uint32 txop;				/* avg TXOP */
	uint32 sum_lcnt;			/* sum of lfifo cnts */
	uint32 lcnt[AMPDU_MAX_HE];		/* total lfifo cnt per mcs and nss */
	uint32 gdfcscnt[AMPDU_MAX_HE];		/* total good FCS cnt per mcs and nss */
	musched_ul_rssi_stats_t ul_rssi_stats;
	uint32 tx_cnt[MUSCHED_RU_TYPE_NUM]; /* total tx cnt per ru size */
	uint32 txsucc_cnt[MUSCHED_RU_TYPE_NUM]; /* succ tx cnt per ru size */
	uint8 ru_idx_use_bmap[MUSCHED_RU_BMP_ROW_SZ][MUSCHED_RU_BMP_COL_SZ];
} musched_ul_stats_t;

/* ul ofdma info */
typedef struct wlc_musched_ulofdma_info {
	wlc_muscheduler_info_t *musched; /* back pointer */
	uint16	maxn[D11_REV128_BW_SZ]; /* max num of users triggered for ul ofdma tx */
	uint16	num_usrs;		/* number of users admitted to ul ofdma scheduler */
	uint16	txlmt;
	scb_t	*scb_list[MUSCHED_ULOFDMA_MAX_NUM_USERS];
	bool	is_start;
	d11ulotxd_rev128_t txd;
	musched_ul_gstats_t ul_gstats;
	int8	num_scb_ulstats;
	uint8	min_ulofdma_usrs;	/* minimum number of users to start ul ofdma */
	uint16	csthr0;			/* min lsig len to not clear CS for basic/brp trig */
	uint16	csthr1;			/* min lsig len to not clear CS for all other trig */

	/* additional admit / evict params */
	int	min_rssi;
	uint32	rx_pktcnt_thrsh;	/* threshold of num of pkts per second to enable ul-ofdma */
	uint16	idle_prd;
	bool	ban_nonbrcm_160sta; /* No ulofdma admission for non-brcm 160Mhz-capable sta */
	bool	brcm_only;		/* Only admit brcm client for ulofdma */
	uint8	always_admit;		/* 0: use additional admit/evict params like
					 * min_rssi, rx_pktcnt_thrsh, wlc_scb_ampdurx_on();
					 * 1: always admit ulofdma capable station;
					 * 2: admit trig-enab twt users only
					 */
	 uint16 g_ucfg;			/* global ucfg info */
} wlc_musched_ulofdma_info_t;

/* module info */
struct wlc_muscheduler_info {
	wlc_info_t *wlc;
	uint16	flags;
	int	scbh;
	int16	dl_policy;
	int8	dl_schidx; /* decided by dl_policy; internal used by ucode */
	int16	ul_policy;
	int8	sndtp; /* sounding type */
	int8	ack_policy;
	uint8	lowat[D11_REV128_BW_SZ];
	uint8	maxn[D11_REV128_BW_SZ];
	uint8	rucfg[D11_REV128_BW_SZ][MUSCHED_RUCFG_ROW][MUSCHED_RUCFG_COL];
	uint8	rucfg_ack[D11_REV128_BW_SZ][MUSCHED_RUCFG_ROW][MUSCHED_RUCFG_COL];
	bool	rucfg_fixed; /* = TRUE: fixed/set by iovar */
	bool	use_murts;   /* = TRUE: use murts; FALSE: use regular rts */
	uint16	tmout;
	int8	num_scb_stats;
	musched_ru_stats_t ru_stats;
	uint16	num_dlofdma_users;
	uint16	min_dlofdma_users[MUSCHED_DLOFDMA_MINUSER_SZ]; /* min users to enable dlofdma */
	wlc_musched_ulofdma_info_t *ulosched;
};

#define MUSCHED_DLOFDMA_CLIENTS_MAX	0xFFFF

#define MUSCHED_DLOFDMA_MASK			0x0001
#define MUSCHED_DLOFDMA_SHIFT			0
#define MUSCHED_AUTO_DLSCH_MASK			0x0002

#define MUSCHED_DLOFDMA(a)		((a)->flags & MUSCHED_DLOFDMA_MASK)
#define MUSCHED_DLOFDMA_ENABLE(a)	((a)->flags |= MUSCHED_DLOFDMA_MASK)
#define MUSCHED_DLOFDMA_DISABLE(a)	((a)->flags &= ~MUSCHED_DLOFDMA_MASK)
#define MUSCHED_AUTO_DLSCH(a)		((a)->flags & MUSCHED_AUTO_DLSCH_MASK)
#define MUSCHED_AUTO_DLSCH_ENABLE(a)	((a)->flags |= MUSCHED_AUTO_DLSCH_MASK)
#define MUSCHED_AUTO_DLSCH_DISABLE(a)	((a)->flags &= ~MUSCHED_AUTO_DLSCH_MASK)

#define MUSCHED_HEMSCH_STP_MASK		0x001C
#define MUSCHED_HEMSCH_MURTS_MASK	0x0020
#define MUSCHED_HEMSCH_STP_SHIFT	2
#define MUSCHED_HEMSCH_MURTS_SHIFT	5
#define MUSCHED_HEMSCH_STP(a)		((a & MUSCHED_HEMSCH_STP_MASK) >> MUSCHED_HEMSCH_STP_SHIFT)
#define MUSCHED_HEMSCH_MURTS(a)		((a & MUSCHED_HEMSCH_MURTS_MASK) >> \
		MUSCHED_HEMSCH_MURTS_SHIFT)

#define MUSCHED_ACKPOLICY_SERIAL	0
#define MUSCHED_ACKPOLICY_TRIGINAMP	1
#define MUSCHED_ACKPOLICY_MUBAR		2
#define MUSCHED_ACKPOLICY_MAX		(MUSCHED_ACKPOLICY_MUBAR)

#define MUSCHED_MURTS_DFLT		FALSE
#define MUSCHED_TRVLSCH_TMOUT_DFLT		128

#define MUSCHED_TRVLSCH_LOWAT_DFLT		4
#define MUSCHED_TRVLSCH_MINN_DFLT		2

#define MUSCHED_TRVLSCH_RUCFG_RUSTRT_POS	0

#define MUSCHED_MAX_MMU_USERS_COUNT(wlc) \
	(D11REV_IS((wlc)->pub->corerev, 131) ? MUCLIENT_NUM_4 : \
	(D11REV_IS((wlc)->pub->corerev, 130) ? MUCLIENT_NUM_8 : \
	MUCLIENT_NUM_16))

/* scb cubby */
typedef struct {
	int8 dl_schpos;		/* HEMU DL scheduler pos */
	int8 ul_schpos;		/* HEMU UL scheduler pos */
	musched_ru_stats_t *scb_ru_stats; /* pointer to ru usage stats */
	/* For UL OFDMA */
	uint16 ul_rmemidx;	/* HEMU UL ratemem idx */
	uint16 ucfg;		/* per scb ucfg info */
	bool upd_ul_rmem;	/* if need to update ul_rmem */
	d11ratemem_ulrx_rate_t *ul_rmem;
	musched_ul_stats_t *scb_ul_stats; /* pointer to ru usage stats */
	bool dlul_assoc;	/* eligible on / off, determined on (de}assoc/auth */

	/* admit / evict params */
	uint32	last_rx_pkts;
	uint8	idle_cnt;
	uint8	state; /* 0: init, 1: admit, 2: evict */
} scb_musched_t;

#define C_SNDREQ_FSEQINF_POS	3
#define	C_SNDREQ_HESER_NBIT	15
#define C_BFIGEN_HESER_NBIT	15

/* cubby access macros */
#define SCB_MUSCHED_CUBBY(musched, scb)	(scb_musched_t **)SCB_CUBBY(scb, (musched)->scbh)
#define SCB_MUSCHED(musched, scb)	*SCB_MUSCHED_CUBBY(musched, scb)

/* local declarations */

/* wlc module */
static int wlc_muscheduler_wlc_init(void *ctx);
static int wlc_muscheduler_doiovar(void *context, uint32 actionid,
	void *params, uint plen, void *arg, uint alen, uint vsize, wlc_if_t *wlcif);
#if defined(BCMDBG) || defined(DL_RU_STATS_DUMP)
static int wlc_musched_dump(void *ctx, bcmstrbuf_t *b);
static int wlc_musched_dump_clr(void *ctx);
static void wlc_musched_dump_ru_stats(wlc_muscheduler_info_t *musched, bcmstrbuf_t *b);
static void wlc_musched_scb_rustats_init(wlc_muscheduler_info_t *musched, scb_t *scb);
static void wlc_musched_print_ru_stats(musched_ru_stats_t* ru_stats, bcmstrbuf_t *b,
	bool is_160);
#endif // endif

#if defined(BCMDBG) || defined(UL_RU_STATS_DUMP)
/* for ul ofdma scheduler */
static int wlc_umusched_dump(void *ctx, bcmstrbuf_t *b);
static int wlc_umusched_dump_clr(void *ctx);
static void wlc_musched_print_ul_stats(musched_ul_stats_t* ul_stats,
	scb_t *scb, bcmstrbuf_t *b);
static void wlc_musched_print_ul_gstats(musched_ul_gstats_t* ul_gstats,
	bcmstrbuf_t *b);
static void wlc_musched_print_ul_ru_stats(musched_ul_stats_t* ul_stats,
	scb_t *scb, bcmstrbuf_t *b, bool is_160);
static void wlc_musched_print_ul_ru_gstats(musched_ul_gstats_t* ul_gstats,
	bcmstrbuf_t *b, bool is_160);
static void wlc_musched_scb_ulstats_init(wlc_muscheduler_info_t *musched, scb_t *scb);
#endif // endif
static void wlc_musched_config_he_sounding_type(wlc_muscheduler_info_t *musched);
static void wlc_musched_scb_state_upd(void *ctx, scb_state_upd_data_t *notif_data);
static void wlc_musched_bsscfg_state_upd(void *ctx, bsscfg_state_upd_data_t *notif_data);
static void wlc_musched_admit_users_reset(wlc_muscheduler_info_t *musched,
		wlc_bsscfg_t *cfg);
static void wlc_musched_set_ulpolicy(wlc_muscheduler_info_t *musched, int16 ulsch_policy);
static int wlc_musched_get_dlpolicy(wlc_muscheduler_info_t *musched);
static int wlc_musched_get_ulpolicy(wlc_muscheduler_info_t *musched);
static int wlc_musched_write_ack_policy(wlc_muscheduler_info_t *musched);
static int wlc_musched_write_use_murts(wlc_muscheduler_info_t *musched);
static int wlc_musched_write_lowat(wlc_muscheduler_info_t *musched);
static int wlc_musched_write_maxn(wlc_muscheduler_info_t *musched);
static int wlc_musched_write_mindluser(wlc_muscheduler_info_t *musched);
static int wlc_musched_write_tmout(wlc_muscheduler_info_t *musched);
static int wlc_musched_upd_rucfg_rmem(wlc_muscheduler_info_t *musched, uint8 bw_idx);
static uint8 wlc_musched_ruidx_mapping(uint8 ru_idx, int8 scale_level, bool toBw160);
static uint8 wlc_musched_ackruidx_mapping(uint8 data_ruidx, uint8 ackru_type);
#ifdef MAC_AUTOTXV_OFF
static void wlc_musched_admit_users_reinit(wlc_muscheduler_info_t *musched, wlc_bsscfg_t *cfg);
#endif // endif
static bool wlc_scbmusched_is_dlofdma(wlc_muscheduler_info_t *musched, scb_t* scb);
static bool wlc_scbmusched_is_ulofdma(wlc_muscheduler_info_t *musched, scb_t* scb);
static bool wlc_musched_ulofdma_add_usr(wlc_musched_ulofdma_info_t *ulosched, scb_t *scb);
static bool wlc_musched_ulofdma_del_usr(wlc_musched_ulofdma_info_t *ulosched, scb_t *scb);
static void wlc_musched_ul_oper_state_upd(wlc_muscheduler_info_t *musched, scb_t *scb, uint8 state);
static void wlc_musched_watchdog(void *ctx);

/* scheduler admit control */
static void wlc_musched_admit_dlclients(wlc_muscheduler_info_t *musched);
static bool wlc_musched_ulofdma_del_usr(wlc_musched_ulofdma_info_t *ulosched, scb_t *scb);

/* ul ofdma scheduler */
static void wlc_musched_ulofdma_commit_change(wlc_musched_ulofdma_info_t* ulosched);
static void wlc_musched_ulofdma_commit_csreq(wlc_musched_ulofdma_info_t* ulosched);
uint16 wlc_musched_scb_get_ulofdma_ratemem_idx(wlc_info_t *wlc, scb_t *scb);
static int wlc_musched_ulofdma_write_maxn(wlc_musched_ulofdma_info_t* ulosched);

/* scb cubby */
static int wlc_musched_scb_init(void *ctx, scb_t *scb);
static void wlc_musched_scb_deinit(void *ctx, scb_t *scb);
static uint wlc_musched_scb_secsz(void *, scb_t *);
#if defined(BCMDBG) || defined(DL_RU_STATS_DUMP)
static void wlc_musched_scb_dump(void *ctx, scb_t *scb, bcmstrbuf_t *b);
#endif // endif
static void wlc_musched_scb_schedule_init(wlc_muscheduler_info_t *musched, scb_t *scb);
static bool wlc_musched_scb_isulofdma_eligible(wlc_musched_ulofdma_info_t *ulosched, scb_t* scb);

/* ul ofdma scb cubby */
static int8 wlc_musched_ulofdma_find_scb(wlc_musched_ulofdma_info_t *ulosched, scb_t *scb);
static int8 wlc_musched_ulofdma_find_addr(wlc_musched_ulofdma_info_t *ulosched,
	struct ether_addr *ea, scb_t *scb);

/* dump */
static void wlc_musched_dump_policy(wlc_muscheduler_info_t *musched, bcmstrbuf_t *b);
static void wl_musched_dump_ackpolicy(wlc_muscheduler_info_t *musched, bcmstrbuf_t *b);
static void wlc_musched_dump_ulofdma(wlc_musched_ulofdma_info_t* ulosched,
	bcmstrbuf_t *b, bool verbose);
/* iovar table */
enum {
	IOV_MUSCHEDULER		= 0,
	IOV_UMUSCHEDULER	= 1,
	IOV_LAST
};

static const bcm_iovar_t muscheduler_iovars[] = {
	{"msched", IOV_MUSCHEDULER, IOVF_RSDB_SET, 0, IOVT_BUFFER, 0},
	{"umsched", IOV_UMUSCHEDULER, IOVF_RSDB_SET, 0, IOVT_BUFFER, 0},
	{NULL, 0, 0, 0, 0, 0}
};

static const uint8 rucfg_numrus[MUSCHED_RU_TYPE_NUM] = {37, 16, 8, 4, 2, 1, 1};

/* Function to scale up and down on given ru index
 * level = 1: BW80->BW160
 * level = -1: BW80->BW40
 * level = -2: BW80->BW20
 * For example, given that input ru=61:
 *	BW80->BW40, level = -1, output is 53
 *	BW80->BW20, level = -2, output is 37
 *	BW80->BW160, level = 1, output is 65
 * XXX: Note: the best pratice of using this API is to do conversion from BW80 to other BWs.
 */
static uint8
wlc_musched_ruidx_mapping(uint8 ru_idx, int8 scale_level, bool toBw160)
{
	uint8 ru_type;
	int16 new_ruidx = ru_idx;
	int8 new_level;
	bool upper_sector = FALSE;

	if (ru_idx <= HE_MAX_26_TONE_RU_INDX) {
		ru_type = 0;
		upper_sector = ru_idx >= 19 ? TRUE : FALSE;
	} else if (ru_idx <= HE_MAX_52_TONE_RU_INDX) {
		ru_type = 1;
		upper_sector = ru_idx >= 45 ? TRUE : FALSE;
	} else if (ru_idx <= HE_MAX_106_TONE_RU_INDX) {
		ru_type = 2;
		upper_sector = ru_idx >= 57 ? TRUE : FALSE;
	} else if (ru_idx <= HE_MAX_242_TONE_RU_INDX) {
		ru_type = 3;
		upper_sector = ru_idx >= 63 ? TRUE : FALSE;
	} else if (ru_idx <= HE_MAX_484_TONE_RU_INDX) {
		ru_type = 4;
		upper_sector = ru_idx >= 66 ? TRUE : FALSE;
	} else if (ru_idx <= HE_MAX_996_TONE_RU_INDX) {
		ru_type = 5;
	} else if (ru_idx <= HE_MAX_2x996_TONE_RU_INDX) {
		ru_type = 6;
	} else {
		return new_ruidx;
	}
	new_level = ru_type + scale_level;
	if (new_level < 0 || new_level >= MUSCHED_RU_TYPE_NUM) {
		new_level = 0;
	}

	if (scale_level > 0) {
		/* scale up case */
		if (upper_sector && toBw160) {
			new_ruidx = (ru_idx | MUSCHED_RU_UPPER_MASK);
		}
		while (ru_type < new_level) {
			if (upper_sector && toBw160) {
				new_ruidx += (rucfg_numrus[ru_type++] / 2);
			} else {
				new_ruidx += rucfg_numrus[ru_type++];
			}
		}
	} else {
		/* scale down case */
		while (ru_type > new_level) {
			new_ruidx -= rucfg_numrus[--ru_type];
		}
		/* Skip some 26 tone RUs to avoid overlapping RUs */
		if (ru_type == 0) {
			if (new_ruidx < MUSCHED_26TONE_RU4) {
				/* pass */
			} else if (new_ruidx < MUSCHED_26TONE_RU13 - 1) {
				new_ruidx += 1;
			} else if (new_ruidx < MUSCHED_26TONE_RU18 - 2) {
				new_ruidx += 2;
			} else if (new_ruidx < MUSCHED_26TONE_RU23 - 3) {
				new_ruidx += 3;
			} else if (new_ruidx < MUSCHED_26TONE_RU32 - 4) {
				new_ruidx += 4;
			}
		}
	}

	return (new_ruidx < 0 ? 0 : new_ruidx);
}

/*
 * Function to return ru idx for hetb ack given data ru idx and ack ru type
 */
static uint8
wlc_musched_ackruidx_mapping(uint8 data_ruidx, uint8 ackru_type)
{
	uint8 ru_type, ruidx_start, ruidx_offset, ru_gap;
	int16 ack_ruidx = data_ruidx;
	int8 ru_upper = data_ruidx & MUSCHED_RU_UPPER_MASK;

	/* mask out upper bit */
	data_ruidx &= MUSCHED_RU_IDX_MASK;

	if (data_ruidx <= HE_MAX_26_TONE_RU_INDX) {
		return ack_ruidx;
	} else if (data_ruidx <= HE_MAX_52_TONE_RU_INDX) {
		ru_type = 1;
		ruidx_start = HE_MAX_26_TONE_RU_INDX + 1;
	} else if (data_ruidx <= HE_MAX_106_TONE_RU_INDX) {
		ru_type = 2;
		ruidx_start = HE_MAX_52_TONE_RU_INDX + 1;
	} else if (data_ruidx <= HE_MAX_242_TONE_RU_INDX) {
		ru_type = 3;
		ruidx_start = HE_MAX_106_TONE_RU_INDX + 1;
	} else if (data_ruidx <= HE_MAX_484_TONE_RU_INDX) {
		ru_type = 4;
		ruidx_start = HE_MAX_242_TONE_RU_INDX + 1;
	} else if (data_ruidx <= HE_MAX_996_TONE_RU_INDX) {
		ru_type = 5;
		ruidx_start = HE_MAX_484_TONE_RU_INDX + 1;
	} else if (data_ruidx <= HE_MAX_2x996_TONE_RU_INDX) {
		ru_type = 6;
		ruidx_start = HE_MAX_996_TONE_RU_INDX + 1;
	} else {
		return ack_ruidx;
	}

	if (ackru_type >= ru_type) {
		return ack_ruidx;
	}

	ru_gap = ackru_type - ru_type;
	ruidx_offset = data_ruidx - ruidx_start;
	ack_ruidx = wlc_musched_ruidx_mapping(ruidx_start, ru_gap, 0);
	ack_ruidx += (rucfg_numrus[ackru_type] / rucfg_numrus[ru_type]) * ruidx_offset;
	ack_ruidx += ru_upper;

	return ack_ruidx;
}

static const uint8 rucfg_init_tbl[MUSCHED_RUCFG_ROW][MUSCHED_RUCFG_COL] = {
	{67},
	{65, 66},
	{65, 63, 64},
	{61, 62, 63, 64},
	{61, 62, 63, 59, 60},
	{61, 62, 57, 58, 59, 60},
	{61, 55, 56, 57, 58, 59, 60},
	{53, 54, 55, 56, 57, 58, 59, 60},
	{53, 54, 55, 56, 57, 58, 59, 51, 52},
	{53, 54, 55, 56, 57, 58, 49, 50, 51, 52},
	{53, 54, 55, 56, 57, 47, 48, 49, 50, 51, 52},
	{53, 54, 55, 56, 45, 46, 47, 48, 49, 50, 51, 52},
	{53, 54, 55, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52},
	{53, 54, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52},
	{53, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52},
	{37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52},
};

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
*/
#include <wlc_patch.h>

/* ======== attach/detach ======== */

wlc_muscheduler_info_t *
BCMATTACHFN(wlc_muscheduler_attach)(wlc_info_t *wlc)
{
	wlc_muscheduler_info_t *musched;
	wlc_musched_ulofdma_info_t *ulosched;
	scb_cubby_params_t cubby_params;
	int i, j, tbl_idx;
	int8 scale_level;
	bool toBw160 = FALSE;
	uint16 max_mmu_usrs = wlc_txcfg_max_mmu_clients_get(wlc->txcfg);

	/* allocate private module info */
	if ((musched = MALLOCZ(wlc->osh, sizeof(*musched))) == NULL) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n", wlc->pub->unit,
			__FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	musched->wlc = wlc;

	/* HE is attached before this module, so can rely on HE_ENAB here */
	if (!HE_ENAB(wlc->pub)) {
		WL_INFORM(("wl%d: %s: MU scheduler disabled\n", wlc->pub->unit,	__FUNCTION__));
		return musched;
	}

	musched->dl_policy = MUSCHED_DL_POLICY_AUTO;
	musched->ul_policy = MUSCHED_UL_POLICY_AUTO;
	musched->sndtp = MUSCHED_HESNDTP_AUTO;
	musched->tmout = MUSCHED_TRVLSCH_TMOUT_DFLT;
	musched->ack_policy = MUSCHED_ACKPOLICY_TRIGINAMP;
	musched->use_murts = MUSCHED_MURTS_DFLT;
	for (i = 0; i < D11_REV128_BW_SZ; i++) {
		musched->lowat[i] = MUSCHED_TRVLSCH_LOWAT_DFLT;
	}
	wlc_txcfg_dlofdma_maxn_init(wlc, musched->maxn);
	musched->rucfg_fixed = FALSE;
	/* init the different BWs' rucfg tbls based on BW80 template */
	for (tbl_idx = 0; tbl_idx < D11_REV128_BW_SZ; tbl_idx++) {
		scale_level = tbl_idx - 2;
		toBw160 = (tbl_idx == D11_REV128_BW_160MHZ) ? TRUE : FALSE;
		for (i = 0; i < MUSCHED_RUCFG_ROW; i++) {
			for (j = 0; j < MUSCHED_RUCFG_COL; j++) {
				musched->rucfg[tbl_idx][i][j] = wlc_musched_ruidx_mapping(
					rucfg_init_tbl[i][j], scale_level, toBw160);
				/* init 106T RU for HETB ACK */
				musched->rucfg_ack[tbl_idx][i][j] =
					wlc_musched_ackruidx_mapping(musched->rucfg
					[tbl_idx][i][j], D11AX_RU106_TYPE);
			}
		}
	}
	/* 2 dlofdma capable users are needed to enable dlofdma by default */
	for (i = 0; i < MUSCHED_DLOFDMA_MINUSER_SZ; i++) {
		musched->min_dlofdma_users[i] = MUSCHED_TRVLSCH_MINN_DFLT;
	}

	/* If maximum number of runtime MU-MIMO users is as same max MU-MIMO clients system can
	 * handle, then all MU FIFOs will be allocated for MU-MIMO clients.
	 * System will not be able to support OFDMA cleints.
	 */
	if (max_mmu_usrs == MUSCHED_MAX_MMU_USERS_COUNT(wlc)) {
		wlc_txcfg_max_clients_set(wlc->txcfg, DLOFDMA, 0);
		MUSCHED_DLOFDMA_DISABLE(musched);
	}
	/* init the ul ofdma scheduler info */
	if ((ulosched = MALLOCZ(wlc->osh, sizeof(*ulosched))) == NULL) {
		WL_ERROR(("wl%d: %s: out of memory for ulosched, malloced %d bytes\n",
			wlc->pub->unit,
			__FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	/* Init ulosched */
	ulosched->musched = musched; /* set the back pointer */
	musched->ulosched = ulosched;
	wlc_txcfg_ulofdma_maxn_init(wlc, ulosched->maxn);
	ulosched->txd.macctl = ((0 << D11_ULOTXD_MACTL_MODE_NBIT) |
		(0 << D11_ULOTXD_MACTL_FIXDUR_NBIT) |
		(HE_TRIG_TYPE_BASIC_FRM << D11_ULOTXD_MACTL_PTYPE_SHIFT) |
		((1 << AC_BK) << D11_ULOTXD_MACTL_ACBMP_SHIFT) |
		(MUSCHED_ULOFDMA_AVG_TXDUR << D11_ULOTXD_MACTL_DURCFG_SHIFT)); /* 0x20 */
	ulosched->txd.maxdur = MUSCHED_ULOFDMA_MAX_DUR;
	ulosched->txd.burst = 2;
	ulosched->txd.maxtw = 5;
	ulosched->txd.txcnt = 0;
	ulosched->txd.interval = -1; /* Turn on auto trigger interval scheme */
	ulosched->txd.minidle = 100; /* Init to 100us */
	ulosched->txd.rxlowat0 = 3;
	ulosched->txd.rxlowat1 = 30;
	wlc_umusched_set_fb(musched, WLC_HT_GET_FRAMEBURST(wlc->hti));
	ulosched->txd.txctl = (DOT11_HETB_2XLTF_1U6S_GI << D11_ULOTXD_TXCTL_CPF_SHIFT) |
		(DOT11_HETB_2XHELTF_NLTF << D11_ULOTXD_TXCTL_NLTF_SHIFT);
	ulosched->txlmt = 0x206;
	ulosched->min_ulofdma_usrs = MUSCHED_ULOFDMA_MIN_USERS;
	ulosched->csthr0 = 76;
	ulosched->csthr1 = 418;
	ulosched->is_start = TRUE;
	ulosched->brcm_only = FALSE;
	ulosched->ban_nonbrcm_160sta = FALSE;

	D11_ULOTXD_UCFG_SET_MCSNSS(ulosched->g_ucfg, 0x17); /* auto rate by default */
	/* register module up/down, watchdog, and iovar callbacks */
	if (wlc_module_register(wlc->pub, muscheduler_iovars, "muscheduler", musched,
		wlc_muscheduler_doiovar, wlc_musched_watchdog, wlc_muscheduler_wlc_init, NULL)) {
		WL_ERROR(("wl%d: %s: wlc_module_register failed\n", wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* reserve some space in scb for private data */
	bzero(&cubby_params, sizeof(cubby_params));

	cubby_params.context = musched;
	cubby_params.fn_init = wlc_musched_scb_init;
	cubby_params.fn_deinit = wlc_musched_scb_deinit;
	cubby_params.fn_secsz = wlc_musched_scb_secsz;
#if defined(BCMDBG) || defined(DL_RU_STATS_DUMP)
	cubby_params.fn_dump = wlc_musched_scb_dump;
#endif // endif

	if ((musched->scbh =
		wlc_scb_cubby_reserve_ext(wlc, sizeof(scb_musched_t *), &cubby_params)) < 0) {
		WL_ERROR(("wl%d: %s: wlc_scb_cubby_reserve_ext() failed\n", wlc->pub->unit,
			__FUNCTION__));
		goto fail;
	}

	if (wlc_scb_state_upd_register(wlc, wlc_musched_scb_state_upd, musched) != BCME_OK) {
		WL_ERROR(("wl%d: %s: unable to register callback wlc_musched_scb_state_upd\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	if ((wlc_bsscfg_state_upd_register(wlc,
			wlc_musched_bsscfg_state_upd, musched)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_state_upd_register failed\n",
		          wlc->pub->unit, __FUNCTION__));
			goto fail;
	}

	/* debug dump */
#if defined(BCMDBG) || defined(DL_RU_STATS_DUMP)
	wlc_dump_add_fns(wlc->pub, "msched", wlc_musched_dump, wlc_musched_dump_clr, musched);
#endif // endif
#if defined(BCMDBG) || defined(UL_RU_STATS_DUMP)
	wlc_dump_add_fns(wlc->pub, "umsched", wlc_umusched_dump, wlc_umusched_dump_clr, musched);
#endif // endif

	return musched;

fail:
	wlc_muscheduler_detach(musched);
	return NULL;
}

void
BCMATTACHFN(wlc_muscheduler_detach)(wlc_muscheduler_info_t *musched)
{
	wlc_info_t *wlc;

	if (musched == NULL) {
		return;
	}

	wlc = musched->wlc;

	wlc_scb_state_upd_unregister(wlc, wlc_musched_scb_state_upd, musched);

	wlc_bsscfg_state_upd_unregister(wlc, wlc_musched_bsscfg_state_upd, musched);

	wlc_module_unregister(wlc->pub, "muscheduler", musched);

	if (musched->ulosched != NULL) {
		MFREE(wlc->osh, musched->ulosched, sizeof(wlc_musched_ulofdma_info_t));
	}

	musched->ulosched = NULL;
	MFREE(wlc->osh, musched, sizeof(*musched));
}

/* ======== iovar dispatch ======== */
static int
wlc_musched_cmd_get_dispatch(wlc_muscheduler_info_t *musched, wl_musched_cmd_params_t *params,
	char *outbuf, int outlen)
{
	int err = BCME_OK;
	wlc_info_t *wlc;
	bcmstrbuf_t bstr;
	int i, j, bw, start, end;
	uint16 uval16 = 0;
	uint offset;
	int8 direction; /* indicates 1:dl 2:ul 3:bi */

	BCM_REFERENCE(i);
	BCM_REFERENCE(offset);
	BCM_REFERENCE(direction);

	bcm_binit(&bstr, outbuf, outlen);
	wlc = musched->wlc;

	if (D11REV_LE(wlc->pub->corerev, 128))
		return BCME_UNSUPPORTED;

	if (!strncmp(params->keystr, "lowat", strlen("lowat"))) {
		bcm_bprintf(&bstr, "low watermark ");
		for (i = 0; i < D11_REV128_BW_SZ; i++) {
			bw = 20 << i;
			bcm_bprintf(&bstr, "bw%d: %d ", bw, musched->lowat[i]);
		}
		bcm_bprintf(&bstr, "\n");
#if defined(WL_PSMX)
		if (wlc->clk) {
			offset = MX_OQEXPECTN_BLK(wlc);
			for (i = 0; i < D11_REV128_BW_SZ; i++) {
				bw = 20 << i;
				uval16 = wlc_read_shmx(wlc, offset+(i*2));
				if (uval16 != musched->lowat[i]) {
					bcm_bprintf(&bstr, "shmx bw%d: %d ", bw, uval16);
				}
			}
			bcm_bprintf(&bstr, "\n");
		}
#endif // endif
	} else if (!strncmp(params->keystr, "maxn", strlen("maxn"))) {
		bcm_bprintf(&bstr, "maxN ");
		for (i = 0; i < D11_REV128_BW_SZ; i++) {
			bw = 20 << i;
			bcm_bprintf(&bstr, "bw%d: %d ", bw, musched->maxn[i]);
		}
		bcm_bprintf(&bstr, "\n");
#if defined(WL_PSMX)
		if (wlc->clk) {
			offset = MX_OQMAXN_BLK(wlc);
			for (i = 0; i < D11_REV128_BW_SZ; i++) {
				uval16 = wlc_read_shmx(wlc, offset+(i*2));
				if (uval16 != musched->maxn[i]) {
					bcm_bprintf(&bstr, "shmx bw%d: %d ", bw, uval16);
				}
			}
			bcm_bprintf(&bstr, "\n");
		}
#endif // endif
	} else if (!strncmp(params->keystr, "tmout", strlen("tmout"))) {
		bcm_bprintf(&bstr, "tmout %d us\n", musched->tmout);

#if defined(WL_PSMX)
		if (wlc->clk) {
			offset = MX_OMSCH_TMOUT(wlc);
			uval16 = wlc_read_shmx(wlc, offset);
			if (uval16 != musched->tmout) {
				bcm_bprintf(&bstr, "shmx tmout %d us\n", uval16);
			}
		}
#endif // endif
	} else if (!strncmp(params->keystr, "rucfg_ack", strlen("rucfg_ack")) ||
			!strncmp(params->keystr, "rucfg", strlen("rucfg"))) {
		int8 row, tbl_idx;
		uint8 (*ptr)[MUSCHED_RUCFG_ROW][MUSCHED_RUCFG_COL];
		if (!strcmp(params->keystr, "rucfg")) {
			ptr = musched->rucfg;
		} else {
			ptr = musched->rucfg_ack;
		}
		if (params->bw == -1) {
			start = 0;
			end = D11_REV128_BW_SZ-1;
		} else {
			start = end = params->bw;
		}
		for (tbl_idx = start; tbl_idx <= end; tbl_idx++) {
			row = params->row;
			if (row <= 0 || row > MUSCHED_RUCFG_ROW) {
				row = MUSCHED_RUCFG_ROW;
				i = 0;
			} else {
				i = row - 1;
			}
			bw = 20 << tbl_idx;
			bcm_bprintf(&bstr, "Nusr: ru allocation (%s) BW %dMHz\n",
				musched->rucfg_fixed ? "manual" : "default", bw);
			for (; i < row; i++) {
				bcm_bprintf(&bstr, "%2d:  ", i+1);
				for (j = MUSCHED_TRVLSCH_RUCFG_RUSTRT_POS;
					j <= i+MUSCHED_TRVLSCH_RUCFG_RUSTRT_POS; j++) {
					uval16 = ptr[tbl_idx][i][j];
					bcm_bprintf(&bstr, " %2d", uval16);
				}
				bcm_bprintf(&bstr, "\n");
			}
		}
	} else if (!strncmp(params->keystr, "ackpolicy", strlen("ackpolicy"))) {
		wl_musched_dump_ackpolicy(musched, &bstr);
	} else if (!strncmp(params->keystr, "murts", strlen("murts"))) {
		bcm_bprintf(&bstr, "Use MU-RTS: %d\n", musched->use_murts);
	} else if (!strncmp(params->keystr, "snd", strlen("snd"))) {
		bcm_bprintf(&bstr, "HE sounding sequence: %d\n", musched->sndtp);
	} else if (!strncmp(params->keystr, "mindlusers", strlen("mindlusers"))) {
		bcm_bprintf(&bstr, "dlofdma_users: min %d (<=80MHz) %d (160MHz) max %d num %d\n",
			musched->min_dlofdma_users[0],
			musched->min_dlofdma_users[1],
			wlc_txcfg_max_clients_get(wlc->txcfg, DLOFDMA),
			musched->num_dlofdma_users);
	} else if (!strncmp(params->keystr, "maxofdmausers", strlen("maxofdmausers"))) {
		bcm_bprintf(&bstr, "Max DL ofdma users: %d\n",
			wlc_txcfg_max_clients_get(wlc->txcfg, DLOFDMA));
	} else {
		wlc_musched_dump_policy(musched, &bstr);
	}

	return err;
}

static int
wlc_musched_write_ack_policy(wlc_muscheduler_info_t *musched)
{
#if defined(WL_PSMX)
	wlc_info_t *wlc;
	uint offset;
	uint16 uval16;

	wlc = musched->wlc;
	if (!wlc->clk) {
		return BCME_NOCLK;
	}
	if (musched->dl_schidx == MCTL2_FIXED_SCHIDX) {
		offset = MX_HEMSCH0_BLK(wlc);
	} else if (musched->dl_schidx == MCTL2_TRIVIAL_SCHIDX) {
		offset = MX_HEMSCH1_BLK(wlc);
	} else {
		return BCME_ERROR;
	}
	uval16 = wlc_read_shmx(wlc, offset);
	uval16 &= ~MUSCHED_HEMSCH_STP_MASK;
	uval16 |= ((musched->ack_policy << MUSCHED_HEMSCH_STP_SHIFT) &
		MUSCHED_HEMSCH_STP_MASK);
	wlc_write_shmx(wlc, offset, uval16);
#endif /* defined(WL_PSMX) */
	return BCME_OK;
}

static int
wlc_musched_write_use_murts(wlc_muscheduler_info_t *musched)
{
#if defined(WL_PSMX)
	wlc_info_t *wlc;
	uint offset;
	uint16 uval16;

	wlc = musched->wlc;
	if (!wlc->clk) {
		return BCME_NOCLK;
	}
	if (musched->dl_schidx == MCTL2_FIXED_SCHIDX) {
		offset = MX_HEMSCH0_BLK(wlc);
	} else if (musched->dl_schidx == MCTL2_TRIVIAL_SCHIDX) {
		offset = MX_HEMSCH1_BLK(wlc);
	} else {
		return BCME_ERROR;
	}

	uval16 = wlc_read_shmx(wlc, offset);
	if (musched->use_murts) {
		uval16 |= MUSCHED_HEMSCH_MURTS_MASK;
	} else {
		uval16 &= ~MUSCHED_HEMSCH_MURTS_MASK;
	}
	wlc_write_shmx(wlc, offset, uval16);
#endif /* defined(WL_PSMX) */
	return BCME_OK;
}

static int
wlc_musched_write_tmout(wlc_muscheduler_info_t *musched)
{
	wlc_info_t *wlc;
	wlc = musched->wlc;
	if (!wlc->clk)
		return BCME_NOCLK;
#if defined(WL_PSMX)
	wlc_write_shmx(wlc, MX_OMSCH_TMOUT(wlc), musched->tmout);
#endif // endif
	return BCME_OK;
}

static int
wlc_musched_write_mindluser(wlc_muscheduler_info_t *musched)
{
#if defined(WL_PSMX)
	wlc_info_t *wlc;
	uint offset;
	int i;
	wlc = musched->wlc;
	if (!wlc->clk)
		return BCME_NOCLK;
	offset = MX_OQMINN_BLK(wlc);
	for (i = 0; i < MUSCHED_DLOFDMA_MINUSER_SZ; i++) {
		wlc_write_shmx(wlc, offset+(i*2), musched->min_dlofdma_users[i]);
	}
	wlc_write_shmx(wlc, MX_UCX2R_FLAGS(wlc),
		wlc_read_shmx(wlc, MX_UCX2R_FLAGS(wlc)) & ~D11_UCX2R_CONS_MASK);
#endif // endif
	return BCME_OK;
}

static int
wlc_musched_write_lowat(wlc_muscheduler_info_t *musched)
{
#if defined(WL_PSMX)
	wlc_info_t *wlc;
	uint offset;
	int bw;

	wlc = musched->wlc;
	if (!wlc->clk) {
		return BCME_NOCLK;
	}

	offset = MX_OQEXPECTN_BLK(wlc);
	for (bw = 0; bw < D11_REV128_BW_SZ; bw++) {
		wlc_write_shmx(wlc, offset+(bw*2), musched->lowat[bw]);
	}
#endif /* defined(WL_PSMX) */
	return BCME_OK;
}

static int
wlc_musched_write_maxn(wlc_muscheduler_info_t *musched)
{
#if defined(WL_PSMX)
	wlc_info_t *wlc;
	uint offset;
	int bw;

	wlc = musched->wlc;
	if (!wlc->clk) {
		return BCME_NOCLK;
	}

	offset = MX_OQMAXN_BLK(wlc);
	for (bw = 0; bw < D11_REV128_BW_SZ; bw++) {
		wlc_write_shmx(wlc, offset+(bw*2), musched->maxn[bw]);
	}
#endif /* defined(WL_PSMX) */
	return BCME_OK;
}

static int
wlc_musched_cmd_set_dispatch(wlc_muscheduler_info_t *musched, wl_musched_cmd_params_t *params)
{
	int err = BCME_OK;
	wlc_info_t *wlc;
	uint16 uval16;
	uint offset;
	int i, start, end;

	BCM_REFERENCE(i);
	BCM_REFERENCE(start);
	BCM_REFERENCE(end);
	BCM_REFERENCE(offset);

	wlc = musched->wlc;

	if (D11REV_LE(wlc->pub->corerev, 128))
		return BCME_UNSUPPORTED;

	if (!strncmp(params->keystr, "lowat", strlen("lowat"))) {
		uval16 = params->vals[0] >= 0 ? params->vals[0] : 0;

		if (params->bw == -1) {
			start = 0;
			end = D11_REV128_BW_SZ-1;
		} else {
			start = end = params->bw;
		}
		for (i = start; i <= end; i++) {
			musched->lowat[i] = uval16;
		}

		wlc_musched_write_lowat(musched);

	} else if (!strncmp(params->keystr, "maxn", strlen("maxn"))) {
		uval16 = params->vals[0] >= 0 ? params->vals[0] : 0;

		if (params->bw == -1) {
			start = 0;
			end = D11_REV128_BW_SZ-1;
		} else {
			start = end = params->bw;
		}
		for (i = start; i <= end; i++) {
			musched->maxn[i] = MIN(uval16, wlc_txcfg_ofdma_maxn_upperbound(wlc, i));
		}

		wlc_musched_write_maxn(musched);
	} else if (!strncmp(params->keystr, "tmout", strlen("tmout"))) {
		musched->tmout = (params->vals[0] >= 0) ? params->vals[0] : 0;
		wlc_musched_write_tmout(musched);
	} else if (!strncmp(params->keystr, "rucfg_ack", strlen("rucfg_ack")) ||
			!strncmp(params->keystr, "rucfg", strlen("rucfg"))) {
		int8 row, tbl_idx;
		uint8 (*ptr)[MUSCHED_RUCFG_ROW][MUSCHED_RUCFG_COL];
		if (!strcmp(params->keystr, "rucfg")) {
			ptr = musched->rucfg;
		} else {
			ptr = musched->rucfg_ack;
		}
#if !defined(TESTBED_AP_11AX) && !defined(BCMDBG)
		if (wlc->pub->up) {
			return BCME_NOTDOWN;
		}
#endif /* !TESTBED_AP_11AX && !BCMDBG */
		row = params->row;
		if (row == -1 || row > MUSCHED_RUCFG_ROW) {
			return BCME_USAGE_ERROR;
		}
		if (params->bw == -1) {
			start = 0;
			end = D11_REV128_BW_SZ-1;
		} else {
			start = end = params->bw;
		}
		musched->rucfg_fixed = TRUE;
		for (tbl_idx = start; tbl_idx <= end; tbl_idx++) {
			for (i = 0; i < params->num_vals; i++) {
				ptr[tbl_idx][row-1][i+MUSCHED_TRVLSCH_RUCFG_RUSTRT_POS]=
					params->vals[i];
			}
		}
#if defined(TESTBED_AP_11AX) || defined(BCMDBG)
		if (wlc->pub->up && RATELINKMEM_ENAB(wlc->pub)) {
			uint txpktpendtot = 0; /* # of packets handed over to d11 DMA */
			/* If there are pending packets on the fifo, then stop the fifo
			 * processing and re-enqueue packets
			 */
			for (i = 0; i < WLC_HW_NFIFO_INUSE(wlc); i++) {
				txpktpendtot += TXPKTPENDGET(wlc, i);
			}
			if (txpktpendtot > 0) {
				wlc_sync_txfifo_all(wlc, wlc->active_queue, SYNCFIFO);
			}
			/* write rucfg to ratemem */
			for (tbl_idx = start; tbl_idx <= end; tbl_idx++) {
				if ((err = wlc_musched_upd_rucfg_rmem(musched, tbl_idx))
					!= BCME_OK) {
					WL_ERROR(("wl%d: %s: fail to write rucfg "
						"bw_idx %d err %d\n",
						wlc->pub->unit, __FUNCTION__, tbl_idx, err));
					ASSERT(0);
				}
			}
		}
#endif /* TESTBED_AP_11AX || BCMDBG */
	} else if (!strncmp(params->keystr, "policy", strlen("policy"))) {
		if ((wlc_musched_get_dlpolicy(musched) != MUSCHED_DL_POLICY_DISABLE &&
			params->vals[0] == MUSCHED_DL_POLICY_DISABLE) ||
			(wlc_musched_get_dlpolicy(musched) == MUSCHED_DL_POLICY_DISABLE &&
			params->vals[0] != MUSCHED_DL_POLICY_DISABLE)) {
				wlc_musched_set_dlpolicy(musched, params->vals[0]);
			}
		else if (wlc->pub->up) {
			return BCME_NOTDOWN;
		}
	} else if (!strncmp(params->keystr, "ackpolicy", strlen("ackpolicy"))) {
		if (params->vals[0] > MUSCHED_ACKPOLICY_MAX || params->vals[0] < 0) {
			return BCME_RANGE;
		}

		musched->ack_policy = params->vals[0];
		wlc_musched_write_ack_policy(musched);
	} else if (!strncmp(params->keystr, "murts", strlen("murts"))) {
		musched->use_murts = params->vals[0] != 0 ? TRUE : FALSE;
		wlc_musched_write_use_murts(musched);
	} else if (!strncmp(params->keystr, "snd", strlen("snd"))) {
		if (wlc->pub->up) {
			return BCME_NOTDOWN;
		}

		if (params->vals[0] > MUSCHED_HESNDTP_MAX ||
			params->vals[0] < MUSCHED_HESNDTP_AUTO) {
			return BCME_RANGE;
		}

		musched->sndtp = params->vals[0];
	} else if (!strncmp(params->keystr, "mindlusers", strlen("mindlusers"))) {
		uval16 = params->vals[0] >= 0 ? params->vals[0] : 0;
		if (params->bw == -1) {
			start = D11_REV128_BW_20MHZ;
			end = MIN(D11_REV128_BW_160MHZ, MUSCHED_DLOFDMA_MINUSER_SZ-1);
		} else if (params->bw == D11_REV128_BW_160MHZ) {
			if (uval16 < MUSCHED_TRVLSCH_MINN_DFLT) {
				/* MU 1 160MHz limitation */
				return BCME_UNSUPPORTED;
			}
			start = end = MUSCHED_DLOFDMA_MINUSER_SZ - 1;
		} else {
			start = end = 0;
		}
		for (i = start; i <= end; i++) {
			musched->min_dlofdma_users[i] = uval16;
		}
		if (wlc->pub->up) {
			wlc_musched_admit_dlclients(musched);
		}
		wlc_musched_write_mindluser(musched);
	} else {
		err = BCME_BADARG;
	}

	return err;
}

static int
wlc_umusched_cmd_get_dispatch(wlc_muscheduler_info_t *musched, wl_musched_cmd_params_t *params,
	char *outbuf, int outlen)
{
	int err = BCME_OK;
	wlc_info_t *wlc;
	bcmstrbuf_t bstr;
	int i, bw;
	uint16 uval16;
	uint offset;
	int8 direction; /* indicates 1:dl 2:ul 3:bi */
	wlc_musched_ulofdma_info_t *ulosched = musched->ulosched;

	BCM_REFERENCE(i);
	BCM_REFERENCE(offset);
	BCM_REFERENCE(direction);

	bcm_binit(&bstr, outbuf, outlen);
	wlc = musched->wlc;

	if (D11REV_LE(wlc->pub->corerev, 128))
		return BCME_UNSUPPORTED;

	if (!ulosched) {
		bcm_bprintf(&bstr, "Ul OFDMA scheduler is not initilized\n");
		return BCME_ERROR;
	}
	if (!strncmp(params->keystr, "maxn", strlen("maxn"))) {
		bcm_bprintf(&bstr, "maxN ");
		for (i = 0; i < D11_REV128_BW_SZ; i++) {
			bw = 20 << i;
			bcm_bprintf(&bstr, "bw%d: %d ", bw, ulosched->maxn[i]);
		}
		bcm_bprintf(&bstr, "\n");
#if defined(WL_PSMX)
		if (wlc->clk) {
			offset = MX_ULOMAXN_BLK(wlc);
			for (i = 0; i < D11_REV128_BW_SZ; i++) {
				uval16 = wlc_read_shmx(wlc, offset+(i*2));
				if (uval16 != ulosched->maxn[i]) {
					bcm_bprintf(&bstr, "shmx bw%d: %d ", bw, uval16);
				}
			}
			bcm_bprintf(&bstr, "\n");
		}
#endif // endif
	} else if (!strncmp(params->keystr, "maxclients", strlen("maxclients"))) {
		bcm_bprintf(&bstr, "max num of admitted clients: %d\n",
		wlc_txcfg_max_clients_get(wlc->txcfg, ULOFDMA));
	} else if (!strncmp(params->keystr, "ban_nonbrcm_160sta", strlen("ban_nonbrcm_160sta"))) {
		bcm_bprintf(&bstr, "ban nonbrcm 160sta: %d\n", ulosched->ban_nonbrcm_160sta);
	} else if (!strncmp(params->keystr, "brcm_only", strlen("brcm_only"))) {
		bcm_bprintf(&bstr, "brcm only: %d\n", ulosched->brcm_only);
	} else if (!strncmp(params->keystr, "start", strlen("start"))) {
		bcm_bprintf(&bstr, "%d\n", ulosched->is_start);
	} else {
		wlc_musched_dump_ulofdma(ulosched, &bstr, WL_MUSCHED_FLAGS_VERBOSE(params));
	}

	return err;
}

static void
wlc_umusched_start(wlc_muscheduler_info_t *musched, bool enable)
{
	wlc_info_t *wlc = musched->wlc;

	ASSERT(wlc != NULL);

	/* Set host flag to enable UL OFDMA */
	wlc_bmac_mhf(wlc->hw, MXHF0, MXHF0_ULOFDMA,
		enable ? MXHF0_ULOFDMA : 0, WLC_BAND_ALL);
	/* Set host flag to send trigger status to driver. It is on by default */
	wlc_bmac_mhf(wlc->hw, MHF5, MHF5_TRIGTXS,
		enable ? MHF5_TRIGTXS : 0, WLC_BAND_ALL);
}

static int
wlc_umusched_cmd_set_dispatch(wlc_muscheduler_info_t *musched, wl_musched_cmd_params_t *params)
{
	int err = BCME_OK;
	wlc_info_t *wlc;
	uint offset;
	int i, start, end;
	int16 val16;
	wlc_musched_ulofdma_info_t *ulosched = musched->ulosched;
	scb_t *scb = NULL;
	scb_musched_t* musched_scb;
	int8 schpos;
	bool upd = TRUE;
	d11ulotxd_rev128_t *txd = &ulosched->txd;
	uint16 max_ulofdma_usrs;

	BCM_REFERENCE(i);
	BCM_REFERENCE(offset);
	BCM_REFERENCE(txd);

	wlc = musched->wlc;
	if (D11REV_LE(wlc->pub->corerev, 128))
		return BCME_UNSUPPORTED;

	if (!ulosched) {
		return BCME_ERROR;
	}

	max_ulofdma_usrs = wlc_txcfg_max_clients_get(wlc->txcfg, ULOFDMA);

	val16 = params->vals[0];

	if (!strncmp(params->keystr, "start", strlen("start"))) {
		if (!HE_ULMU_ENAB(wlc->pub)) {
			return BCME_EPERM;
		} else {
			ulosched->is_start = val16 ? TRUE : FALSE;
			wlc_umusched_start(musched, ulosched->is_start);
		}
	} else if (!strncmp(params->keystr, "fb", strlen("fb"))) {
		if ((val16 < 0) || (val16 > 1)) {
			return BCME_RANGE;
		}
		wlc_umusched_set_fb(musched, val16 ? TRUE : FALSE);
	} else if (!strncmp(params->keystr, "mctl_mode", strlen("mctl_mode"))) {
		txd->macctl &= ~D11_ULOTXD_MACTL_MODE_NBIT;
		txd->macctl |= ((val16 == 0 ? 0 : 1) << D11_ULOTXD_MACTL_MODE_NBIT);
	} else if (!strncmp(params->keystr, "mctl_ptype", strlen("mctl_ptype"))) {
		txd->macctl &= ~D11_ULOTXD_MACTL_PTYPE_MASK;
		txd->macctl |= ((val16 << D11_ULOTXD_MACTL_PTYPE_SHIFT) &
			D11_ULOTXD_MACTL_PTYPE_MASK);
	} else if (!strncmp(params->keystr, "mctl", strlen("mctl"))) {
		txd->macctl = val16;
	} else if (!strncmp(params->keystr, "txcnt", strlen("txcnt"))) {
		txd->txcnt = val16;
	} else if (!strncmp(params->keystr, "interval", strlen("interval"))) {
		txd->interval = val16;
	} else if (!strncmp(params->keystr, "burst", strlen("burst"))) {
		if ((val16 > txd->maxtw)) {
			return BCME_RANGE;
		}
		txd->burst = val16;
	} else if (!strncmp(params->keystr, "maxn", strlen("maxn"))) {
		if (val16 > MUSCHED_ULOFDMA_MAX_NUM_TRIGGER_USERS ||
			val16 > max_ulofdma_usrs) {
			return BCME_RANGE;
		}
		val16 = params->vals[0] >= 0 ? params->vals[0] : 0;

		if (params->bw == -1) {
			start = 0;
			end = D11_REV128_BW_SZ-1;
		} else {
			start = end = params->bw;
		}
		for (i = start; i <= end; i++) {
			ulosched->maxn[i] = MIN(val16, wlc_txcfg_ofdma_maxn_upperbound(wlc, i));
		}

		wlc_musched_ulofdma_write_maxn(ulosched);
	} else if (!strncmp(params->keystr, "maxdur", strlen("maxdur"))) {
		if (val16 > MUSCHED_ULOFDMA_MAX_DUR || val16 <= 0) {
			return BCME_RANGE;
		}
		txd->maxdur = val16;
	} else if (!strncmp(params->keystr, "maxtw", strlen("maxtw"))) {
		if ((val16 != -1) && (val16 < txd->burst)) {
			return BCME_RANGE;
		}
		txd->maxtw = val16;
	} else if (!strncmp(params->keystr, "minidle", strlen("minidle"))) {
		txd->minidle = val16;
	} else if (!strncmp(params->keystr, "txlowat0", strlen("txlowat0"))) {
		txd->txlowat0 = val16;
	} else if (!strncmp(params->keystr, "txlowat1", strlen("txlowat1"))) {
		txd->txlowat1 = val16;
	} else if (!strncmp(params->keystr, "rxlowat0", strlen("rxlowat0"))) {
		txd->rxlowat0 = val16;
	} else if (!strncmp(params->keystr, "rxlowat1", strlen("rxlowat1"))) {
		txd->rxlowat1 = val16;
	} else if (!strncmp(params->keystr, "autorate", strlen("autorate"))) {
		if (val16 != 1 && val16 != 0) {
			return BCME_RANGE;
		}
		ulosched->g_ucfg &= (val16 == 0) ? (uint16) -1 :
			~D11_ULOTXD_UCFG_FIXRT_MASK;
		for (schpos = 0; schpos < max_ulofdma_usrs; schpos++) {
			if (WL_MUSCHED_FLAGS_MACADDR(params) && (schpos !=
				wlc_musched_ulofdma_find_addr(ulosched, &params->ea, scb))) {
				/* only update the scb with given ether addr matched */
					continue;
			}
			if ((scb = ulosched->scb_list[schpos]) == NULL) {
				continue;
			}
			musched_scb = SCB_MUSCHED(musched, scb);
			musched_scb->ucfg &= (val16 == 0) ? (uint16) -1 :
				~D11_ULOTXD_UCFG_FIXRT_MASK;
		}
	} else if (!strncmp(params->keystr, "mcs", strlen("mcs"))) {
		if (!(val16 >= 0 && val16 <= 11)) {
			return BCME_RANGE;
		}
		D11_ULOTXD_UCFG_SET_MCS(ulosched->g_ucfg, val16);
		ulosched->g_ucfg |= D11_ULOTXD_UCFG_FIXRT_MASK;
		for (schpos = 0; schpos < max_ulofdma_usrs; schpos++) {
			if (WL_MUSCHED_FLAGS_MACADDR(params) && (schpos !=
				wlc_musched_ulofdma_find_addr(ulosched, &params->ea, scb))) {
				/* only update the scb with given ether addr matched */
					continue;
			}
			if ((scb = ulosched->scb_list[schpos]) == NULL) {
				continue;
			}
			musched_scb = SCB_MUSCHED(musched, scb);
			D11_ULOTXD_UCFG_SET_MCS(musched_scb->ucfg, val16);
			musched_scb->ucfg |= D11_ULOTXD_UCFG_FIXRT_MASK;
		}
	} else if (!strncmp(params->keystr, "nss", strlen("nss"))) {
		if (val16 < 1 || val16 > 4) {
			return BCME_RANGE;
		}
		D11_ULOTXD_UCFG_SET_NSS(ulosched->g_ucfg, val16 - 1);
		ulosched->g_ucfg |= D11_ULOTXD_UCFG_FIXRT_MASK;
		for (schpos = 0; schpos < max_ulofdma_usrs; schpos++) {
			if (WL_MUSCHED_FLAGS_MACADDR(params) && (schpos !=
				wlc_musched_ulofdma_find_addr(ulosched, &params->ea, scb))) {
				/* only update the scb with given ether addr matched */
					continue;
			}
			if ((scb = ulosched->scb_list[schpos]) == NULL) {
				continue;
			}
			musched_scb = SCB_MUSCHED(musched, scb);
			D11_ULOTXD_UCFG_SET_NSS(musched_scb->ucfg, val16 - 1);
			musched_scb->ucfg |= D11_ULOTXD_UCFG_FIXRT_MASK;
		}
	} else if (!strncmp(params->keystr, "trssi", strlen("trssi"))) {
		D11_ULOTXD_UCFG_SET_TRSSI(ulosched->g_ucfg, val16);
		ulosched->g_ucfg |= D11_ULOTXD_UCFG_FIXRSSI_MASK;
		for (schpos = 0; schpos < max_ulofdma_usrs; schpos++) {
			if (WL_MUSCHED_FLAGS_MACADDR(params) && (schpos !=
				wlc_musched_ulofdma_find_addr(ulosched, &params->ea, scb))) {
				/* only update the scb with given ether addr matched */
					continue;
			}
			if ((scb = ulosched->scb_list[schpos]) == NULL) {
				continue;
			}
			musched_scb = SCB_MUSCHED(musched, scb);
			D11_ULOTXD_UCFG_SET_TRSSI(musched_scb->ucfg, val16);
			musched_scb->ucfg |= D11_ULOTXD_UCFG_FIXRSSI_MASK;
		}
	} else if (!strncmp(params->keystr, "cpltf", strlen("cpltf"))) {
		if ((val16 < DOT11_HETB_2XLTF_1U6S_GI || val16 >= DOT11_HETB_RSVD_LTF_GI)) {
			return BCME_RANGE;
		}
		txd->txctl &= ~D11_ULOTXD_TXCTL_CPF_MASK;
		txd->txctl |= ((val16 << D11_ULOTXD_TXCTL_CPF_SHIFT) &
			D11_ULOTXD_TXCTL_CPF_MASK);
	} else if (!strncmp(params->keystr, "nltf", strlen("nltf"))) {
		if ((val16 <= DOT11_HETB_1XHELTF_NLTF || val16 >= DOT11_HETB_RSVD_NLTF)) {
			return BCME_RANGE;
		}
		txd->txctl &= ~D11_ULOTXD_TXCTL_NLTF_MASK;
		txd->txctl |= ((val16 << D11_ULOTXD_TXCTL_NLTF_SHIFT) &
			D11_ULOTXD_TXCTL_NLTF_MASK);
	} else if (!strncmp(params->keystr, "txlmt", strlen("txlmt"))) {
		ulosched->txlmt = val16;
	} else if (!strncmp(params->keystr, "mmlen", strlen("mmlen"))) {
		txd->mmlen = val16;
	} else if (!strncmp(params->keystr, "mlen", strlen("mlen"))) {
		txd->mlen = val16;
	} else if (!strncmp(params->keystr, "aggn", strlen("aggn"))) {
		txd->aggnum = val16;
	} else if (!strncmp(params->keystr, "ban_nonbrcm_160sta", strlen("ban_nonbrcm_160sta"))) {
		if (wlc->pub->up) {
			return BCME_NOTDOWN;
		}
		ulosched->ban_nonbrcm_160sta = (val16 != 0) ? TRUE : FALSE;
	} else if (!strncmp(params->keystr, "brcm_only", strlen("brcm_only"))) {
		if (wlc->pub->up) {
			return BCME_NOTDOWN;
		}
		ulosched->brcm_only = (val16 != 0) ? TRUE : FALSE;
	} else if (!strncmp(params->keystr, "minulusers", strlen("minulusers"))) {
		if (val16 >= 0 && (val16 <= (uint8) -1)) {
			ulosched->min_ulofdma_usrs = val16;
		} else {
			return BCME_RANGE;
		}
	} else if (!strncmp(params->keystr, "csthr0", strlen("csthr0"))) {
		ulosched->csthr0 = val16;
	} else if (!strncmp(params->keystr, "csthr1", strlen("csthr1"))) {
		ulosched->csthr1 = val16;
	} else if (!strncmp(params->keystr, "minrssi", strlen("minrssi"))) {
		ulosched->min_rssi = val16;
	} else if (!strncmp(params->keystr, "pktthrsh", strlen("pktthrsh"))) {
		ulosched->rx_pktcnt_thrsh = val16;
	} else if (!strncmp(params->keystr, "idleprd", strlen("idleprd"))) {
		ulosched->idle_prd = val16;
	} else if (!strncmp(params->keystr, "always_admit", strlen("always_admit"))) {
		if ((uint16)val16 > 2) {
			return BCME_RANGE;
		}
		ulosched->always_admit = val16;
	} else {
		upd = FALSE;
		err = BCME_BADARG;
	}

	if (upd && wlc->pub->up) {
		wlc_musched_ulofdma_commit_change(ulosched);
		wlc_musched_ulofdma_commit_csreq(ulosched);
	}
	return err;
}

static int
wlc_muscheduler_doiovar(void *ctx, uint32 actionid,
	void *params, uint plen, void *arg, uint alen, uint vsize, wlc_if_t *wlcif)
{
	int err = BCME_OK;
	int32 *ret_int_ptr;
	wlc_muscheduler_info_t *musched = ctx;
	wlc_info_t *wlc = musched->wlc;
	wl_musched_cmd_params_t musched_cmd_params;
	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	BCM_REFERENCE(vsize);
	BCM_REFERENCE(wlc);
	BCM_REFERENCE(ret_int_ptr);

	if (D11REV_LT(wlc->pub->corerev, 129)) return BCME_ERROR;

	bcopy(params, &musched_cmd_params, sizeof(wl_musched_cmd_params_t));
	switch (actionid) {
	case IOV_GVAL(IOV_MUSCHEDULER):
		err = wlc_musched_cmd_get_dispatch(musched, &musched_cmd_params, arg, alen);
		break;
	case IOV_GVAL(IOV_UMUSCHEDULER):
		err = wlc_umusched_cmd_get_dispatch(musched, &musched_cmd_params, arg, alen);
		break;
	case IOV_SVAL(IOV_MUSCHEDULER):
		err = wlc_musched_cmd_set_dispatch(musched, params);
		break;
	case IOV_SVAL(IOV_UMUSCHEDULER):
		err = wlc_umusched_cmd_set_dispatch(musched, params);
		break;

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

/* ======== wlc module hooks ========= */

/* wlc init callback */
static int
wlc_muscheduler_wlc_init(void *ctx)
{
	wlc_muscheduler_info_t *musched = ctx;
	int err = BCME_OK;
	wlc_info_t *wlc = musched->wlc;
	uint16 max_mmu_usrs = wlc_txcfg_max_mmu_clients_get(wlc->txcfg);

	BCM_REFERENCE(max_mmu_usrs);
	if (!HE_ENAB(wlc->pub)) {
		return BCME_OK;
	}

	wlc_musched_set_dlpolicy(musched, musched->dl_policy);
	wlc_musched_set_ulpolicy(musched, musched->ul_policy);
	wlc_musched_config_he_sounding_type(musched);

	wlc->musched->ulosched->min_rssi = MUSCHED_SCB_MIN_RSSI;
	wlc->musched->ulosched->rx_pktcnt_thrsh = MUSCHED_SCB_RX_PKTCNT_THRSH;
	wlc->musched->ulosched->idle_prd = MUSCHED_SCB_IDLEPRD;
	wlc->musched->ulosched->always_admit = 0;

	wlc_musched_write_lowat(musched);
	wlc_musched_write_maxn(musched);
	wlc_musched_write_mindluser(musched);
	wlc_musched_write_ack_policy(musched);
	wlc_musched_write_use_murts(musched);
	if (RATELINKMEM_ENAB(wlc->pub)) {
		int bw_idx;
		/* write rucfg to ratemem */
		for (bw_idx = 0; bw_idx < D11_REV128_BW_SZ; bw_idx++) {
			if ((err = wlc_musched_upd_rucfg_rmem(musched, bw_idx)) != BCME_OK) {
				WL_ERROR(("wl%d: %s: fail to write rucfg bw_idx %d err %d\n",
				wlc->pub->unit, __FUNCTION__, bw_idx, err));
				ASSERT(0);
			}
		}
	}
	ASSERT(max_mmu_usrs <= MUSCHED_MAX_MMU_USERS_COUNT(wlc));

#ifdef MAC_AUTOTXV_OFF
	/* initialize bfi shm config */
	wlc_musched_admit_users_reinit(musched, NULL);
#endif // endif

	/* start or stop UL scheduler, depending on value of is_start */
	if (HE_ULMU_ENAB(wlc->pub) && musched->ulosched->is_start) {
		wlc_umusched_start(musched, TRUE);
	} else {
		wlc_umusched_start(musched, FALSE);
	}

	if (!wlc->pub->up) {
		wlc_musched_max_clients_set(musched);
		wlc_musched_admit_users_reset(musched, NULL);
	}
	wlc->musched->ulosched->txd.chanspec = wlc->home_chanspec;
	wlc_musched_ulofdma_commit_change(musched->ulosched);
	wlc_musched_ulofdma_commit_csreq(musched->ulosched);
	wlc_musched_ulofdma_write_maxn(musched->ulosched);

	return err;
}

/**
 * ul-ofdma admission control
 *
 * Registered as musched module watchdog callback. called once per second.
 * Iterates through each scb and see if there is any new active user that needs to be
 * admitted as ul-ofdma or to evict any existing user
 *
 * @param ctx		handle to msched_info context
 * @return		none
 */
static void
wlc_musched_watchdog(void *ctx)
{
	wlc_muscheduler_info_t *musched = ctx;
	wlc_info_t *wlc = musched->wlc;
	scb_t *scb;
	scb_iter_t scbiter;
	int rssi, admit_cnt = 0;
	uint16 schpos;
	uint16 max_ulofdma_usrs = wlc_txcfg_max_clients_get(wlc->txcfg, ULOFDMA);
	scb_musched_t* musched_scb;
	wlc_musched_ulofdma_info_t *ulosched = musched->ulosched;
	bool admit_clients = TRUE;
	bool commit_change = FALSE;

	if (ulosched->always_admit != 1) {
		/* eviction */
		for (schpos = 0; schpos < max_ulofdma_usrs; schpos++) {
			if ((scb = ulosched->scb_list[schpos]) == NULL) {
				continue;
			}

			if (!SCB_ULOFDMA(scb)) {
				continue;
			}
			musched_scb = SCB_MUSCHED(musched, scb);
			ASSERT(musched_scb != NULL);
			if (!musched_scb) {
				continue;
			}
#ifdef WLCNTSCB
			musched_scb->idle_cnt++;

			if ((uint32)scb->scb_stats.rx_ucast_pkts -
				(uint32)musched_scb->last_rx_pkts >=
				ulosched->rx_pktcnt_thrsh) {
				musched_scb->idle_cnt = 0;
			}

			musched_scb->last_rx_pkts = scb->scb_stats.rx_ucast_pkts;
#else
			musched_scb->idle_cnt = 0;
#endif /* WLCNTSCB */
			/* Do not evict trigger based twt user */
			if (wlc_twt_scb_is_trig_mode(wlc->twti, scb)) {
				continue;
			} else if (ulosched->always_admit == 2) {
				/* admit twt only mode */
				wlc_musched_ulofdma_del_usr(ulosched, scb);
				commit_change = TRUE;
				continue;
			}

			rssi = wlc_lq_rssi_get(wlc, SCB_BSSCFG(scb), scb);
			if ((musched_scb->idle_cnt >= ulosched->idle_prd) ||
				(rssi < ulosched->min_rssi)) {
				wlc_musched_ulofdma_del_usr(ulosched, scb);
				commit_change = TRUE;
			}
		}
	}

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		/* Verify for each client if it is eligible to be admitted, even if it has
		 * just been evicted.
		 */
		if (!wlc_musched_scb_isulofdma_eligible(ulosched, scb)) {
			continue;
		}
		admit_cnt++;
	}

	if (ulosched->num_usrs + admit_cnt < ulosched->min_ulofdma_usrs) {
		/* If number of users dropped below minimum, evict all other clients as well.
		 */
		if (ulosched->num_usrs) {
			for (schpos = 0; schpos < max_ulofdma_usrs; schpos++) {
				if ((scb = ulosched->scb_list[schpos]) == NULL) {
					continue;
				}

				if (SCB_ULOFDMA(scb)) {
					wlc_musched_ulofdma_del_usr(ulosched, scb);
					commit_change = TRUE;
				}
			}
		}
		admit_clients = FALSE;
	}

	/* admission */
	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (admit_clients && wlc_musched_scb_isulofdma_eligible(ulosched, scb)) {
			if (!wlc_musched_ulofdma_add_usr(ulosched, scb)) {
				wlc_musched_ul_oper_state_upd(musched, scb, MUSCHED_SCB_INIT);
			}
			commit_change = TRUE;
		}

#ifdef WLCNTSCB
		musched_scb = SCB_MUSCHED(musched, scb);

		/* musched_scb->last_rx_pkts is used to determine eligibility, so keep this below
		 * all calls to wlc_musched_scb_isulofdma_eligible.
		 */
		if (musched_scb) {
			musched_scb->last_rx_pkts = scb->scb_stats.rx_ucast_pkts;
		}
#endif /* WLCNTSCB */
	}

	if (commit_change) {
		/* if a user has been added or deleted, update the scheduler block */
		wlc_musched_ulofdma_commit_change(ulosched);
	}

	return;
}

#if defined(WL11AX) && defined(WL_PSMX)
void
wlc_musched_chanspec_upd(wlc_info_t *wlc)
{
	if (!wlc->musched || D11REV_LT(wlc->pub->corerev, 128) || !HE_ULMU_ENAB(wlc->pub)) {
		return;
	}

	if (!wlc->clk) {
		return;
	}

	if (!PSMX_ENAB(wlc->pub)) {
		return;
	}

	if (wlc->musched->ulosched->txd.chanspec != wlc->home_chanspec) {
		wlc_bmac_suspend_macx_and_wait(wlc->hw);
		wlc->musched->ulosched->txd.chanspec = wlc->home_chanspec;
		wlc_write_shmx(wlc, MX_TRIG_TXCFG(wlc) +
			OFFSETOF(d11ulotxd_rev128_t, chanspec), wlc->home_chanspec);
		wlc_bmac_enable_macx(wlc->hw);
	}
}
#endif /* defined(WL11AX) && defined(WL_PSMX) */

#ifdef MAC_AUTOTXV_OFF
static void wlc_musched_admit_users_reinit(wlc_muscheduler_info_t *musched, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = musched->wlc;
	scb_iter_t scbiter;
	scb_t *scb;
	wlc_bsscfg_t *bsscfg;
	scb_musched_t *musched_scb;
	int i;

	/* re-init all the already admitted dl/ul ofdma users */
	FOREACH_BSS(wlc, i, bsscfg) {
		if (cfg && cfg != bsscfg) {
			continue;
		}

		FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
			musched_scb = SCB_MUSCHED(musched, scb);
			if (!wlc_musched_scb_isdlofdma_eligible(musched, scb)) {
				continue;
			}

			if (!musched_scb->dlul_assoc) {
				continue;
			}
			wlc_txbf_tbcap_update(wlc->txbf, scb);
			wlc_macreq_upd_bfi(wlc, scb, wlc_fifo_index_peek(wlc->fifo, scb, AC_BE),
				TRUE);

			WL_INFORM(("wl%d: %s: sta "MACF"\n",
				wlc->pub->unit, __FUNCTION__, ETHER_TO_MACF(scb->ea)));
		}

	}
}
#endif /* MAC_AUTOTXV_OFF */

static void
wl_musched_dump_ackpolicy(wlc_muscheduler_info_t *musched, bcmstrbuf_t *b)
{
	uint16 uval16 = 0;
	uint offset;
	wlc_hw_info_t *wlc_hw = musched->wlc->hw;
	wlc_info_t *wlc = musched->wlc;
	char ackpolicy_str [][13] = {
		"serial",
		"trig_in_amp",
		"mu_bar",
		"mubar_in_amp",
		"invalid",
	};

	BCM_REFERENCE(offset);

	uval16 = MIN(musched->ack_policy, ARRAYSIZE(ackpolicy_str)-1);
	bcm_bprintf(b, "ack policy: %s (%d)\n",
		ackpolicy_str[uval16], musched->ack_policy);
	if (!wlc->clk) {
		return;
	}

	if (musched->dl_schidx == MCTL2_FIXED_SCHIDX) {
		offset = MX_HEMSCH0_BLK(wlc);
#if defined(WL_PSMX)
		uval16 = wlc_bmac_read_shmx(wlc_hw, offset);
#endif // endif
	} else if (musched->dl_schidx == MCTL2_TRIVIAL_SCHIDX) {
		offset = MX_HEMSCH1_BLK(wlc);
#if defined(WL_PSMX)
		uval16 = wlc_bmac_read_shmx(wlc_hw, offset);
#endif // endif
	} else {
		uval16 = ARRAYSIZE(ackpolicy_str) - 1;
		goto print;
	}

	uval16 = MUSCHED_HEMSCH_STP(uval16);
print:
	if (uval16 != musched->ack_policy) {
		bcm_bprintf(b, "shmx ack policy: %d\n", uval16);
	}
}

static void
wlc_musched_dump_policy(wlc_muscheduler_info_t *musched, bcmstrbuf_t *b)
{
	wlc_info_t *wlc = musched->wlc;
	int i;
#ifdef NOT_YET
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* NOT_YET */
	uint16 max_dlofdma_users = wlc_txcfg_max_clients_get(wlc->txcfg, DLOFDMA);
	char musched_dl_policy_str[][20] = {
		"DISABLE",
		"FIXED",
		"TRIVIAL",
		"AUTO",
	};
	char musched_ul_policy_str[][20] = {
		"DISABLE",
		"AUTO",
	};
	char musched_snd_tp_str[][6] = {
		"HETB",
		"HESU",
		"BFP",
	};
	uint dl_policy_idx = MIN((uint) wlc_musched_get_dlpolicy(musched),
		ARRAYSIZE(musched_dl_policy_str)-1);
	uint ul_policy_idx = MIN((uint) wlc_musched_get_ulpolicy(musched),
		ARRAYSIZE(musched_ul_policy_str)-1);

	bcm_bprintf(b, "MU scheduler:\n");
	bcm_bprintf(b, "DL policy: %s (%d)",
		musched_dl_policy_str[dl_policy_idx], wlc_musched_get_dlpolicy(musched));
	bcm_bprintf(b, "  ofdma_en: %d schidx: %d flag: 0x%04x ",
		MUSCHED_DLOFDMA(musched), musched->dl_schidx, musched->flags);
	wl_musched_dump_ackpolicy(musched, b);
	bcm_bprintf(b, "           HE sounding sequence: %s (%d) mu-rts: %d\n"
		"           dlofdma_users min %d max %d num %d\n",
		musched_snd_tp_str[wlc_musched_get_he_sounding_type(musched)],
		musched->sndtp, musched->use_murts,
		musched->min_dlofdma_users[0],
		max_dlofdma_users,
		musched->num_dlofdma_users);
	bcm_bprintf(b, "           maxn ");
	for (i = 0; i < D11_REV128_BW_SZ; i++) {
		bcm_bprintf(b, "bw%d: %d ", 20 << i, musched->maxn[i]);
	}
	bcm_bprintf(b, "\n");
	bcm_bprintf(b, "UL policy: %s (%d)\n",
		musched_ul_policy_str[ul_policy_idx], wlc_musched_get_ulpolicy(musched));
	bcm_bprintf(b, "           ulo_num_usrs %d\n", musched->ulosched->num_usrs);
#ifdef NOT_YET
	bcm_bprintf(b, "max_dlofdma_users: %d, assigned MU FIFO slots:\n",
		max_dlofdma_users);
	for (i = 0; i < max_dlofdma_users; i++) {
		scb_t *scb = musched->dlofdma_scb[i];
		bcm_bprintf(b, "\t[%d]: %p (%s) ofdma: %d\n", i, scb, scb ?
			bcm_ether_ntoa(&scb->ea, eabuf) : "/",
			scb ? SCB_DLOFDMA(scb) : 0);
	}
#endif /* NOT_YET */
	bcm_bprintf(b, "\n");
}

static void
wlc_musched_dump_ulofdma(wlc_musched_ulofdma_info_t *ulosched, bcmstrbuf_t *b, bool verbose)
{
	int8 schpos;
	scb_t *scb;
	d11ulotxd_rev128_t *txd = &ulosched->txd;
	wlc_info_t *wlc = ulosched->musched->wlc;
	scb_musched_t* musched_scb;
	uint8 mcs, nss;
	int i;
	uint16 max_ulofdma_usrs = wlc_txcfg_max_clients_get(wlc->txcfg, ULOFDMA);

	bcm_bprintf(b, "UL OFDMA admitted %d maxclients %d minulusers %d "
		"fb %d brcm_only %d ban_nonbrcm_160sta %d\n",
		ulosched->num_usrs, max_ulofdma_usrs, ulosched->min_ulofdma_usrs,
		(txd->mctl0 & D11AC_TXC_MBURST) ? 1 : 0, ulosched->brcm_only,
		ulosched->ban_nonbrcm_160sta);
	bcm_bprintf(b, "maxn ");
	for (i = 0; i < D11_REV128_BW_SZ; i++) {
		bcm_bprintf(b, "bw%d: %d ", 20 << i, ulosched->maxn[i]);
	}
	bcm_bprintf(b, "trigger: %s\n", (ulosched->is_start && (ulosched->num_usrs >=
		ulosched->min_ulofdma_usrs)) ? "ON" : "OFF");

	bcm_bprintf(b, "Scheduler parameters\n"
		"  mctl 0x%04x txcnt %d burst %d maxtw %d\n"
		"  interval %d maxdur %d minidle %d cpltf %d nltf %d\n"
		"  txlmt 0x%x txlowat0 %d txlowat1 %d rxlowat0 %d rxlowat1 %d\n"
		"  mlen %d mmlen %d aggn %d csthr0 %d csthr1 %d\n"
		"  minrssi %d pktthrsh %d idleprd %d always_admit %d\n",
		txd->macctl, txd->txcnt, txd->burst, txd->maxtw,
		txd->interval, txd->maxdur, txd->minidle,
		(txd->txctl & D11_ULOTXD_TXCTL_CPF_MASK),
		((txd->txctl & D11_ULOTXD_TXCTL_NLTF_MASK) >> D11_ULOTXD_TXCTL_NLTF_SHIFT),
		ulosched->txlmt, txd->txlowat0, txd->txlowat1, txd->rxlowat0, txd->rxlowat1,
		txd->mlen, txd->mmlen, txd->aggnum, ulosched->csthr0, ulosched->csthr1,
		ulosched->min_rssi, ulosched->rx_pktcnt_thrsh, ulosched->idle_prd,
		ulosched->always_admit);
	if (ulosched->num_usrs) {
		bcm_bprintf(b, "List of admitted clients:\n");
	}
	for (schpos = 0; schpos < max_ulofdma_usrs; schpos++) {
		if ((scb = ulosched->scb_list[schpos]) != NULL) {
			bcm_bprintf(b, "  "MACF" (aid 0x%x) idx %d rmem %d lmem %d ulmu_allow %d",
				ETHER_TO_MACF(scb->ea), scb->aid & HE_STAID_MASK,
				schpos,
				wlc_musched_scb_get_ulofdma_ratemem_idx(wlc, scb),
				wlc_ratelinkmem_get_scb_link_index(ulosched->musched->wlc, scb),
				wlc_he_get_ulmu_allow(wlc->hei, scb));
				musched_scb = SCB_MUSCHED(ulosched->musched, scb);
				if (musched_scb->ucfg & D11_ULOTXD_UCFG_FIXRT_MASK) {
					mcs = D11_ULOTXD_UCFG_GET_MCS(musched_scb->ucfg);
					nss = D11_ULOTXD_UCFG_GET_NSS(musched_scb->ucfg) + 1;
					bcm_bprintf(b, " fixed rate %dx%d\n", mcs, nss);
				} else {
					bcm_bprintf(b, "\n");
				}
		}
	}
	if (WL_MUTX_ON()) {
		prhex("Dump ul ofdma scheduler block:", (const uint8 *)&ulosched->txd,
			sizeof(ulosched->txd));
	}
}

#if defined(BCMDBG) || defined(DL_RU_STATS_DUMP)
/* debug dump for dl ofdma scheduler */
static int
wlc_musched_dump(void *ctx, bcmstrbuf_t *b)
{
	wlc_muscheduler_info_t *musched = ctx;

	wlc_musched_dump_policy(musched, b);

	if (HE_DLMU_ENAB(musched->wlc->pub)) {
		wlc_musched_dump_ru_stats(musched, b);
	}
	return BCME_OK;
}

static int
wlc_musched_dump_clr(void *ctx)
{
	wlc_muscheduler_info_t *musched = ctx;
	scb_iter_t scbiter;
	scb_t *scb;
	scb_musched_t* musched_scb;
	wlc_info_t *wlc = musched->wlc;

	/* clear global musched_ru_stats */
	memset(&musched->ru_stats, 0, sizeof(musched_ru_stats_t));

	/* clear musched_ru_stats for scb */
	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (!scb || !SCB_ASSOCIATED(scb) || !SCB_HE_CAP(scb)) {
			continue;
		}
		musched_scb = SCB_MUSCHED(musched, scb);
		if (musched_scb && musched_scb->scb_ru_stats) {
			memset(musched_scb->scb_ru_stats, 0, sizeof(musched_ru_stats_t));
		}
	}

	return BCME_OK;
}

static void
wlc_musched_dump_ru_stats(wlc_muscheduler_info_t *musched, bcmstrbuf_t *b)
{
	scb_iter_t scbiter;
	scb_t *scb;
	scb_musched_t* musched_scb;
	wlc_info_t *wlc = musched->wlc;
	uint scb_bw;

	bcm_bprintf(b, "DL RU stats:\n");
	wlc_musched_print_ru_stats(&musched->ru_stats, b, CHSPEC_IS160(musched->wlc->chanspec));

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (!scb || !SCB_ASSOCIATED(scb) || !SCB_HE_CAP(scb)) {
			continue;
		}
		musched_scb = SCB_MUSCHED(musched, scb);
		if (musched_scb && musched_scb->scb_ru_stats) {
			scb_bw = wlc_scb_ratesel_get_link_bw(wlc, scb);
			bcm_bprintf(b, ""MACF" (aid 0x%x) is_dlofdma: %d\n",
				ETHER_TO_MACF(scb->ea), scb->aid & HE_STAID_MASK,
				SCB_DLOFDMA(scb));
			wlc_musched_print_ru_stats(musched_scb->scb_ru_stats, b,
				(scb_bw == BW_160MHZ));
		}
	}

}

static void
wlc_musched_print_ru_stats(musched_ru_stats_t* ru_stats, bcmstrbuf_t *b, bool is_160)
{
	int i, k, cnt;
	uint32 total = 0;
	int32 unacked;
	int pad_space;

	for (i = 0; i < MUSCHED_RU_TYPE_NUM; i++) {
		total += ru_stats->tx_cnt[i];
	}

	if (total) {
		bcm_bprintf(b, "    TX : ");
		for (i = 0; i < MUSCHED_RU_TYPE_NUM; i++) {
			bcm_bprintf(b, "%d(%d%%)  ", ru_stats->tx_cnt[i],
				(ru_stats->tx_cnt[i]*100)/total);
		}
		bcm_bprintf(b, "\n    PER: ");
		for (i = 0; i < MUSCHED_RU_TYPE_NUM; i++) {
			unacked = ru_stats->tx_cnt[i] - ru_stats->txsucc_cnt[i];
			bcm_bprintf(b, "%d(%d%%)  ", unacked, (ru_stats->tx_cnt[i] == 0) ?
				0 : unacked*100/ru_stats->tx_cnt[i]);
			pad_space = unacked ? ru_stats->tx_cnt[i]/unacked : ru_stats->tx_cnt[i]/10;
			while (pad_space) {
				bcm_bprintf(b, " ");
				pad_space /= 10;
			}
		}
		bcm_bprintf(b, "\n");
		bcm_bprintf(b, "    RU : ");
		cnt = 0;
		for (i = 0; i < MUSCHED_RU_BMP_ROW_SZ; i++) {
			if (!is_160 && i > 0) {
				continue;
			}
			for (k = 0; k <= MUSCHED_RU_IDX_NUM; k++) {
				if ((cnt > 0) && (cnt % 16 == 0)) {
					bcm_bprintf(b, "\n");
				}
				if (getbits(ru_stats->ru_idx_use_bmap[i], MUSCHED_RU_BMP_COL_SZ,
					k, 1)) {
					bcm_bprintf(b, "%d%s ", k, i == 0 ? "" : "s");
					cnt++;
				}
			}
		}
		bcm_bprintf(b, "\n");
	}
}

int
wlc_musched_upd_ru_stats(wlc_muscheduler_info_t *musched, scb_t *scb, tx_status_t *txs)
{
	scb_musched_t* musched_scb = SCB_MUSCHED(musched, scb);
	uint8 ru, rutype_idx, ruidx, upper, tx_cnt, txsucc_cnt;

	ru = TX_STATUS128_HEOM_RUIDX(txs->status.s4);
	tx_cnt = TX_STATUS40_TXCNT_RT0(txs->status.s3);
	txsucc_cnt = TX_STATUS40_ACKCNT_RT0(txs->status.s3);
	ruidx = MUSCHED_RU_IDX(ru);

	if (ruidx <= HE_MAX_26_TONE_RU_INDX) {
		rutype_idx = 0;
	} else if (ruidx <= HE_MAX_52_TONE_RU_INDX) {
		rutype_idx = 1;
	} else if (ruidx <= HE_MAX_106_TONE_RU_INDX) {
		rutype_idx = 2;
	} else if (ruidx <= HE_MAX_242_TONE_RU_INDX) {
		rutype_idx = 3;
	} else if (ruidx <= HE_MAX_484_TONE_RU_INDX) {
		rutype_idx = 4;
	} else if (ruidx <= HE_MAX_996_TONE_RU_INDX) {
		rutype_idx = 5;
	} else if (ruidx <= HE_MAX_2x996_TONE_RU_INDX) {
		rutype_idx = 6;
	} else {
		WL_ERROR(("wl%d: %s: Invalid ru type. ru idx %d txs\n"
			"  %04X %04X | %04X %04X | %08X %08X || %08X %08X | %08X\n",
			musched->wlc->pub->unit, __FUNCTION__, ruidx,
			txs->status.raw_bits, txs->frameid, txs->sequence, txs->phyerr,
			txs->status.s3, txs->status.s4, txs->status.s5,
			txs->status.ack_map1, txs->status.ack_map2));
		ASSERT(!"Invalid ru type");
		return BCME_ERROR;
	}

	upper = MUSCHED_RU_UPPER(ru);

	musched->ru_stats.ru_idx_use_bmap[upper][ruidx / 8] |= 1 << (ruidx % 8);
	WLCNTADD(musched->ru_stats.tx_cnt[rutype_idx], tx_cnt);
	WLCNTADD(musched->ru_stats.txsucc_cnt[rutype_idx], txsucc_cnt);

	if (musched_scb->scb_ru_stats) {
		musched_scb->scb_ru_stats->ru_idx_use_bmap[upper][ruidx / 8] |= 1 << (ruidx % 8);
		WLCNTADD(musched_scb->scb_ru_stats->tx_cnt[rutype_idx], tx_cnt);
		WLCNTADD(musched_scb->scb_ru_stats->txsucc_cnt[rutype_idx], txsucc_cnt);
	}

	return BCME_OK;
}
#endif // endif

#if defined(BCMDBG) || defined(UL_RU_STATS_DUMP)
static void
wlc_musched_dump_ul_stats(wlc_muscheduler_info_t *musched, bcmstrbuf_t *b)
{
	scb_iter_t scbiter;
	scb_t *scb;
	scb_musched_t* musched_scb;
	wlc_musched_ulofdma_info_t *ulosched;
	uint scb_bw;
	wlc_info_t *wlc = musched->wlc;
	ulosched = musched->ulosched;

	wlc_musched_print_ul_ru_gstats(&ulosched->ul_gstats, b,
		CHSPEC_IS160(musched->wlc->chanspec));
	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (!scb || !SCB_ASSOCIATED(scb) || !SCB_HE_CAP(scb)) {
			continue;
		}
		musched_scb = SCB_MUSCHED(musched, scb);
		if (musched_scb && musched_scb->scb_ul_stats) {
			scb_bw = wlc_scb_ratesel_get_link_bw(wlc, scb);
			wlc_musched_print_ul_ru_stats(musched_scb->scb_ul_stats, scb, b,
				(scb_bw == BW_160MHZ));
		}
	}

	bcm_bprintf(b, "UL stats:\n");
	wlc_musched_print_ul_gstats(&ulosched->ul_gstats, b);
	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (!scb || !SCB_ASSOCIATED(scb) || !SCB_HE_CAP(scb)) {
			continue;
		}
		musched_scb = SCB_MUSCHED(musched, scb);
		if (musched_scb && musched_scb->scb_ul_stats) {
			wlc_musched_print_ul_stats(musched_scb->scb_ul_stats, scb, b);
		}
	}
}

int
wlc_musched_upd_ul_stats(wlc_muscheduler_info_t *musched, scb_t *scb, tx_status_t *txs)
{
	uint8 nusrs, tx_cnt, txsucc_cnt;
	uint8 mcs, nss, rateidx, last;
	int32 phyrssi;
	uint32 txop, qncnt, agglen;
	scb_musched_t* musched_scb;
	wlc_musched_ulofdma_info_t *ulosched = musched->ulosched;
	musched_scb = SCB_MUSCHED(musched, scb);

	/* 3*txop = ((lsig + 5) << 2) + 51 */
	txop = (((TGTXS_LSIG(txs->status.s2) + 5) << 2) + 51) << OPERAND_SHIFT;
	mcs = TGTXS_MCS(txs->status.s4);
	nss = TGTXS_NSS(txs->status.s4);
	rateidx = nss * MAX_HE_RATES + mcs;
	ASSERT(rateidx < AMPDU_MAX_HE);
	qncnt = TGTXS_QNCNT(txs->status.ack_map1);
	agglen = TGTXS_AGGLEN(txs->status.ack_map1);
	phyrssi = TGTXS_PHYRSSI(txs->status.s8);
	ASSERT(phyrssi < PHYRSSI_2SCOMPLEMENT);
	phyrssi = (phyrssi - ((phyrssi >= PHYRSSI_SIGN_MASK) <<
		PHYRSSI_2SCOMPLEMENT_SHIFT)) << OPERAND_SHIFT;
	nusrs = TGTXS_NUSR(txs->status.s1);
	ASSERT(nusrs > 0);
	last = TGTXS_LAST(txs->status.s5);
	tx_cnt = TGTXS_LCNT(txs->status.s5);
	txsucc_cnt = TGTXS_GDFCSCNT(txs->status.s5);

	/* update ul ofdma global stats */
	if (tx_cnt > 0) {
		WLCNTADD(ulosched->ul_gstats.lcnt[rateidx], tx_cnt);
		WLCNTADD(ulosched->ul_gstats.gdfcscnt[rateidx], txsucc_cnt);
	}
	if (last == 1)
		WLCNTADD(ulosched->ul_gstats.usrhist[nusrs-1], 1);

	/* update ul ofdma per user stats */
	if (!musched_scb->scb_ul_stats) {
		return BCME_OK;
	}
	WLCNTADD(musched_scb->scb_ul_stats->nupd, 1);

	if (tx_cnt == 0) {
		WLCNTADD(musched_scb->scb_ul_stats->nfail, 1);
	} else {
		WLCNTADD(musched_scb->scb_ul_stats->sum_lcnt, tx_cnt);
		WLCNTADD(musched_scb->scb_ul_stats->lcnt[rateidx], tx_cnt);
		WLCNTADD(musched_scb->scb_ul_stats->gdfcscnt[rateidx], txsucc_cnt);
	}

	if (txsucc_cnt == 0) {
		WLCNTADD(musched_scb->scb_ul_stats->nbadfcs, 1);
	} else {
		if (musched_scb->scb_ul_stats->ul_rssi_stats.min_rssi > phyrssi) {
			musched_scb->scb_ul_stats->ul_rssi_stats.min_rssi = phyrssi;
		}

		if (musched_scb->scb_ul_stats->ul_rssi_stats.max_rssi < phyrssi) {
			musched_scb->scb_ul_stats->ul_rssi_stats.max_rssi = phyrssi;
		}
		WLCNTADD(musched_scb->scb_ul_stats->ul_rssi_stats.avg_rssi, phyrssi);
		WLCNTADD(musched_scb->scb_ul_stats->txop, txop);
		WLCNTADD(musched_scb->scb_ul_stats->agglen, agglen);
		WLCNTADD(musched_scb->scb_ul_stats->qncnt, qncnt);
	}

	return BCME_OK;
}

int
wlc_musched_upd_ul_rustats(wlc_muscheduler_info_t *musched, scb_t *scb, tx_status_t *txs)
{
	wlc_musched_ulofdma_info_t *ulosched = musched->ulosched;
	scb_musched_t* musched_scb = SCB_MUSCHED(musched, scb);
	wlc_info_t *wlc;
	uint8 rutype_idx, ruidx, upper, tx_cnt, txsucc_cnt;
	uint8 rualloc_b12, primary80;

	wlc = musched->wlc;
	ruidx = TGTXS_RUIDX(txs->status.s4);
	rualloc_b12 = TGTXS_RUALLOCLSB(txs->status.s4);
	primary80 = (CHSPEC_SB(wlc->chanspec) > 4)? 1: 0;
	upper = primary80 ^ rualloc_b12;
	tx_cnt = TGTXS_LCNT(txs->status.s5);
	txsucc_cnt = TGTXS_GDFCSCNT(txs->status.s5);

	if (ruidx <= HE_MAX_26_TONE_RU_INDX) {
		rutype_idx = 0;
	} else if (ruidx <= HE_MAX_52_TONE_RU_INDX) {
		rutype_idx = 1;
	} else if (ruidx <= HE_MAX_106_TONE_RU_INDX) {
		rutype_idx = 2;
	} else if (ruidx <= HE_MAX_242_TONE_RU_INDX) {
		rutype_idx = 3;
	} else if (ruidx <= HE_MAX_484_TONE_RU_INDX) {
		rutype_idx = 4;
	} else if (ruidx <= HE_MAX_996_TONE_RU_INDX) {
		rutype_idx = 5;
	} else if (ruidx <= HE_MAX_2x996_TONE_RU_INDX) {
		rutype_idx = 6;
	} else {
		WL_ERROR(("wl%d: %s: Invalid ru type. ru idx %d txs\n"
			"  %08X %08X | %08X %08X | %08X %08X || %08X %08X\n",
			musched->wlc->pub->unit, __FUNCTION__, ruidx,
			txs->status.s1, txs->status.s2, txs->status.s3, txs->status.s4,
			txs->status.s5, txs->status.ack_map1, txs->status.ack_map2,
				txs->status.s8));
		ASSERT(!"Invalid ru type");
		return BCME_ERROR;
	}
	ulosched->ul_gstats.ru_idx_use_bmap[upper][ruidx / 8] |= 1 << (ruidx % 8);
	if (tx_cnt > 0) {
		WLCNTADD(ulosched->ul_gstats.tx_cnt[rutype_idx], tx_cnt);
		WLCNTADD(ulosched->ul_gstats.txsucc_cnt[rutype_idx], txsucc_cnt);
	}

	if (musched_scb->scb_ul_stats) {
		musched_scb->scb_ul_stats->ru_idx_use_bmap[upper][ruidx / 8] |= 1 << (ruidx % 8);
		if (tx_cnt > 0) {
			WLCNTADD(musched_scb->scb_ul_stats->tx_cnt[rutype_idx], tx_cnt);
			WLCNTADD(musched_scb->scb_ul_stats->txsucc_cnt[rutype_idx], txsucc_cnt);
		}
	}

	return BCME_OK;
}

static void
wlc_musched_print_ul_ru_gstats(musched_ul_gstats_t* ul_gstats, bcmstrbuf_t *b, bool is_160)
{
	int i, k, cnt;
	uint32 total = 0;
	int32 unacked;
	int pad_space;

	for (i = 0; i < MUSCHED_RU_TYPE_NUM; i++) {
		total += ul_gstats->tx_cnt[i];
	}

	if (!total) {
		return;
	}
	bcm_bprintf(b, "UL RU stats: ");
	cnt = 0;
	for (i = 0; i < MUSCHED_RU_BMP_ROW_SZ; i++) {
		if (!is_160 && i > 0) {
			continue;
		}
		for (k = 0; k <= MUSCHED_RU_IDX_NUM; k++) {
			if ((cnt > 0) && (cnt % 16 == 0)) {
				bcm_bprintf(b, "\n");
			}
			if (getbits(ul_gstats->ru_idx_use_bmap[i], MUSCHED_RU_BMP_COL_SZ,
				k, 1)) {
				bcm_bprintf(b, "%d%s ", k, i == 0 ? "" : "s");
				cnt++;
			}
		}
	}
	bcm_bprintf(b, "\n");
	bcm_bprintf(b, "    RX : ");
	for (i = 0; i < MUSCHED_RU_TYPE_NUM; i++) {
		bcm_bprintf(b, "%d(%d%%)  ", ul_gstats->tx_cnt[i],
			(ul_gstats->tx_cnt[i]*100)/total);
	}
	bcm_bprintf(b, "\n    PER: ");
	for (i = 0; i < MUSCHED_RU_TYPE_NUM; i++) {
		unacked = ul_gstats->tx_cnt[i] - ul_gstats->txsucc_cnt[i];
		bcm_bprintf(b, "%d(%d%%)  ", unacked, (ul_gstats->tx_cnt[i] == 0) ?
			0 : unacked*100/ul_gstats->tx_cnt[i]);
		pad_space = unacked? ul_gstats->tx_cnt[i]/unacked:
			ul_gstats->tx_cnt[i]/10;
		while (pad_space) {
			bcm_bprintf(b, " ");
			pad_space /= 10;
		}
	}
	bcm_bprintf(b, "\n");
}

static void
wlc_musched_print_ul_gstats(musched_ul_gstats_t* ul_gstats, bcmstrbuf_t *b)
{
	int i, last, num_perline;
	uint32 total = 0;
	int32 unacked;

	for (i = 0, total = 0, last = 0; i < MUSCHED_ULOFDMA_MAX_NUM_TRIGGER_USERS; i++) {
		total += ul_gstats->usrhist[i];
		if (ul_gstats->usrhist[i]) last = i;
	}
	if (total) {
		num_perline = MUSCHED_ULOFDMA_MAX_NUM_TRIGGER_USERS < MAX_USRHIST_PERLINE?
			MUSCHED_ULOFDMA_MAX_NUM_TRIGGER_USERS: MAX_USRHIST_PERLINE;
		last = num_perline * (last/num_perline + 1) - 1;
		bcm_bprintf(b, "  HIST : ");
		for (i = 0; i <= last; i++) {
			bcm_bprintf(b, "  %d(%d%%)", ul_gstats->usrhist[i],
				(ul_gstats->usrhist[i] * 100) / total);
			if ((i % num_perline) == (num_perline - 1) && i != last)
				bcm_bprintf(b, "\n       : ");
		}
		bcm_bprintf(b, "\n");
	}

	for (i = 0, total = 0, last = 0; i < AMPDU_MAX_HE; i++) {
		total += ul_gstats->lcnt[i];
		if (ul_gstats->lcnt[i]) last = i;
	}

	if (total) {
		last = MAX_HE_RATES * (last/MAX_HE_RATES + 1) - 1;
		bcm_bprintf(b, "  RX   : ");
		for (i = 0; i <= last; i++) {
			bcm_bprintf(b, "  %d(%d%%)", ul_gstats->lcnt[i],
				(ul_gstats->lcnt[i] * 100) / total);
			if ((i % MAX_HE_RATES) == (MAX_HE_RATES - 1) && i != last)
				bcm_bprintf(b, "\n       : ");
		}
		bcm_bprintf(b, "\n");

		bcm_bprintf(b, "  PER  : ");
		for (i = 0; i <= last; i++) {
			uint txtotal, per = 0;
			txtotal = ul_gstats->lcnt[i];
			unacked = txtotal - ul_gstats->gdfcscnt[i];
			if (unacked < 0) unacked = 0;
			if (unacked != 0 && txtotal != 0) {
				per = (unacked * 100) / txtotal;
			}
			bcm_bprintf(b, "  %d(%d%%)", unacked, per);
			if (((i % MAX_HE_RATES) == MAX_HE_RATES - 1) && i != last)
				bcm_bprintf(b, "\n       : ");
		}
		bcm_bprintf(b, "\n");
	}
}

static void
wlc_musched_print_ul_stats(musched_ul_stats_t* ul_stats, scb_t *scb, bcmstrbuf_t *b)
{
	int i, last;
	uint32 total = 0, tmp_aggnum, tmp_agglen, tmp_txop;
	uint32 ngoodfcs, nposlcnt;
	int32 unacked, tmp;

	for (i = 0, total = 0, last = 0; i < AMPDU_MAX_HE; i++) {
		total += ul_stats->lcnt[i];
		if (ul_stats->lcnt[i]) last = i;
	}

	if (!total) {
		return;
	}

	bcm_bprintf(b, ""MACF" (aid %d):\n",
		ETHER_TO_MACF(scb->ea), scb->aid & HE_STAID_MASK);
	last = MAX_HE_RATES * (last/MAX_HE_RATES + 1) - 1;
	bcm_bprintf(b, "  RX   : ");
	for (i = 0; i <= last; i++) {
		bcm_bprintf(b, "  %d(%d%%)", ul_stats->lcnt[i],
			(ul_stats->lcnt[i] * 100) / total);
		if ((i % MAX_HE_RATES) == (MAX_HE_RATES - 1) && i != last)
		bcm_bprintf(b, "\n       : ");
	}
	bcm_bprintf(b, "\n");

	bcm_bprintf(b, "  PER  : ");
	for (i = 0; i <= last; i++) {
		uint txtotal, per = 0;
		txtotal = ul_stats->lcnt[i];
		unacked = txtotal - ul_stats->gdfcscnt[i];
		if (unacked < 0) unacked = 0;
		if (unacked != 0 && txtotal != 0) {
			per = (unacked * 100) / txtotal;
		}
		bcm_bprintf(b, "  %d(%d%%)", unacked, per);
		if (((i % MAX_HE_RATES) == MAX_HE_RATES - 1) && i != last)
			bcm_bprintf(b, "\n       : ");
	}
	bcm_bprintf(b, "\n");
	if (ul_stats->ul_rssi_stats.min_rssi < 0) {
		tmp = (-ul_stats->ul_rssi_stats.min_rssi) >> OPERAND_SHIFT;
		bcm_bprintf(b, "  RSSI : min(-%d.%d) ", tmp >> 2, (tmp & 0x3) * 25);
	} else {
		tmp = ul_stats->ul_rssi_stats.min_rssi >> OPERAND_SHIFT;
		bcm_bprintf(b, "  RSSI : min(%d.%d) ", tmp >> 2, (tmp & 0x3) * 25);
	}
	if (ul_stats->ul_rssi_stats.max_rssi < 0) {
		tmp = (-ul_stats->ul_rssi_stats.max_rssi) >> OPERAND_SHIFT;
		bcm_bprintf(b, "max(-%d.%d) ", tmp >> 2, (tmp & 0x3) * 25);
	} else {
		tmp = ul_stats->ul_rssi_stats.max_rssi >> OPERAND_SHIFT;
		bcm_bprintf(b, "max(%d.%d) ", tmp >> 2, (tmp & 0x3) * 25);
	}
	ngoodfcs = ul_stats->nupd - ul_stats->nbadfcs;
	nposlcnt = ul_stats->nupd - ul_stats->nfail;
	if (ngoodfcs > 0) {
		tmp = ul_stats->ul_rssi_stats.avg_rssi;
		if (tmp < 0) {
			tmp = (-tmp / (int32) ngoodfcs) >> OPERAND_SHIFT;
			bcm_bprintf(b, "avg(-%d.%d)\n", tmp >> 2, (tmp & 0x3) * 25);
		} else {
			tmp = (tmp / (int32) ngoodfcs) >> OPERAND_SHIFT;
			bcm_bprintf(b, "avg(%d.%d)\n", tmp >> 2, (tmp & 0x3) * 25);
		}
	} else {
		bcm_bprintf(b, "avg(%d.%d)\n", 0, 0);
	}
	bcm_bprintf(b, "  total: qosnull %d ", ul_stats->qncnt);
	bcm_bprintf(b, "lcnt %d ", ul_stats->sum_lcnt);
	bcm_bprintf(b, "nfail %d ", ul_stats->nfail);
	bcm_bprintf(b, "nupd %d\n", ul_stats->nupd);
	if (nposlcnt) {
		tmp_aggnum = ul_stats->sum_lcnt / nposlcnt;
	} else {
		tmp_aggnum = 0;
	}
	if (ngoodfcs) {
		tmp_agglen = ul_stats->agglen / ngoodfcs;
		tmp_txop = ((ul_stats->txop / ngoodfcs) / 3) >> OPERAND_SHIFT;
	} else {
		tmp_agglen = 0;
		tmp_txop = 0;
	}
	bcm_bprintf(b, "  avg  : aggnum %d agglen %d txdur %d\n",
		tmp_aggnum, tmp_agglen, tmp_txop);
}

static void
wlc_musched_print_ul_ru_stats(musched_ul_stats_t* ul_stats, scb_t *scb,
	bcmstrbuf_t *b, bool is_160)
{
	int i, k, cnt;
	uint32 total = 0;
	int32 unacked;
	int pad_space;

	for (i = 0; i < MUSCHED_RU_TYPE_NUM; i++) {
		total += ul_stats->tx_cnt[i];
	}

	if (!total) {
		return;
	}

	bcm_bprintf(b, ""MACF" (aid %d): RU ",
		ETHER_TO_MACF(scb->ea), scb->aid & HE_STAID_MASK);
	cnt = 0;
	for (i = 0; i < MUSCHED_RU_BMP_ROW_SZ; i++) {
		if (!is_160 && i > 0) {
			continue;
		}
		for (k = 0; k <= MUSCHED_RU_IDX_NUM; k++) {
			if ((cnt > 0) && (cnt % 16 == 0)) {
				bcm_bprintf(b, "\n");
			}
			if (getbits(ul_stats->ru_idx_use_bmap[i], MUSCHED_RU_BMP_COL_SZ,
				k, 1)) {
				bcm_bprintf(b, "%d%s ", k, i == 0 ? "" : "s");
				cnt++;
			}
		}
	}
	bcm_bprintf(b, "\n");
	bcm_bprintf(b, "    RX : ");
	for (i = 0; i < MUSCHED_RU_TYPE_NUM; i++) {
		bcm_bprintf(b, "%d(%d%%)  ", ul_stats->tx_cnt[i],
			(ul_stats->tx_cnt[i]*100)/total);
	}
	bcm_bprintf(b, "\n    PER: ");
	for (i = 0; i < MUSCHED_RU_TYPE_NUM; i++) {
		unacked = ul_stats->tx_cnt[i] - ul_stats->txsucc_cnt[i];
		bcm_bprintf(b, "%d(%d%%)  ", unacked, (ul_stats->tx_cnt[i] == 0) ?
			0 : unacked*100/ul_stats->tx_cnt[i]);
		pad_space = unacked ? ul_stats->tx_cnt[i]/unacked : ul_stats->tx_cnt[i]/10;
		while (pad_space) {
			bcm_bprintf(b, " ");
			pad_space /= 10;
		}
	}
	bcm_bprintf(b, "\n");
}

/* debug dump for ul ofdma scheduler */
static int
wlc_umusched_dump(void *ctx, bcmstrbuf_t *b)
{
	wlc_muscheduler_info_t *musched = ctx;

	if (HE_ULMU_ENAB(musched->wlc->pub)) {
		wlc_musched_dump_ulofdma(musched->ulosched, b, TRUE);
		wlc_musched_dump_ul_stats(musched, b);
	}

	return BCME_OK;
}

static int
wlc_umusched_dump_clr(void *ctx)
{
	wlc_musched_ulofdma_info_t *ulosched;
	scb_iter_t scbiter;
	scb_t *scb;
	scb_musched_t* musched_scb;
	wlc_info_t *wlc;

	wlc_muscheduler_info_t *musched = ctx;
	BCM_REFERENCE(musched);
	wlc = musched->wlc;
	ulosched = musched->ulosched;

	/* clear global musched_ul_gstats */
	memset(&ulosched->ul_gstats, 0, sizeof(musched_ul_gstats_t));

	/* clear musched_ul_stats for scb */
	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (!scb || !SCB_ASSOCIATED(scb) || !SCB_HE_CAP(scb)) {
			continue;
		}
		musched_scb = SCB_MUSCHED(musched, scb);
		if (musched_scb && musched_scb->scb_ul_stats) {
			memset(musched_scb->scb_ul_stats, 0, sizeof(musched_ul_stats_t));
			musched_scb->scb_ul_stats->ul_rssi_stats.min_rssi =
				PHYRSSI_2SCOMPLEMENT << OPERAND_SHIFT;
			musched_scb->scb_ul_stats->ul_rssi_stats.max_rssi =
				-PHYRSSI_2SCOMPLEMENT << OPERAND_SHIFT;
		}
	}
	return BCME_OK;
}
#endif // endif

static void
wlc_musched_admit_dlclients(wlc_muscheduler_info_t *musched)
{
	wlc_info_t *wlc = musched->wlc;
	scb_t *scb;
	scb_iter_t scbiter;
	bool onoff;

	onoff = (musched->num_dlofdma_users >= musched->min_dlofdma_users[0]) ? TRUE : FALSE;
	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		scb_musched_t *musched_scb = SCB_MUSCHED(musched, scb);
		bool dlmu_on;
		BCM_REFERENCE(musched_scb);

		dlmu_on = wlc_musched_scb_isdlofdma_eligible(musched, scb);

		/* also check dlul_assoc which include max ofdma count info */
		dlmu_on &= musched_scb->dlul_assoc;

		if (!dlmu_on || (onoff == wlc_scbmusched_is_dlofdma(musched, scb))) {
			/* skip ineligible or already set SCB */
			continue;
		}
		if (dlmu_on) {
			/* disable/enable dlmu explicitly */
			wlc_scbmusched_set_dlofdma(musched, scb, onoff);
		} else {
			if (onoff) {
				WL_MUTX(("wl%d: %s: Fail to enable dlofdma STA "MACF" he_cap %x "
					"dlmu %x\n", wlc->pub->unit, __FUNCTION__,
					ETHER_TO_MACF(scb->ea), SCB_HE_CAP(scb),
					HE_DLMU_ENAB(musched->wlc->pub)));
			}
			wlc_scbmusched_set_dlofdma(musched, scb, FALSE);
		}
		wlc_scbmusched_set_dlschpos(musched, scb, 0);
		/* re-initialize rate info */
		wlc_scb_ratesel_init(wlc, scb);
	}
}

static void wlc_musched_admit_users_reset(wlc_muscheduler_info_t *musched, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = musched->wlc;
	scb_iter_t scbiter;
	scb_t *scb;
	wlc_bsscfg_t *bsscfg;
	scb_musched_t *musched_scb;
	int i;

	/* re-init all the already admitted dl/ul ofdma users */
	FOREACH_BSS(wlc, i, bsscfg) {
		if (cfg && cfg != bsscfg) {
			continue;
		}
		FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
			musched_scb = SCB_MUSCHED(musched, scb);

			if (!musched_scb->dlul_assoc) {
				continue;
			}

			if (wlc_musched_scb_isdlofdma_eligible(musched, scb)) {
				musched_scb->dlul_assoc = FALSE;
				ASSERT(musched->num_dlofdma_users >= 1);
				musched->num_dlofdma_users--;
			}
			if (SCB_ULOFDMA(scb)) {
				wlc_musched_ulofdma_del_usr(musched->ulosched, scb);
			}
		}
	}
	WL_INFORM(("wl%d: %s: max_dlofdma_users: %d admitted %d %d\n",
		wlc->pub->unit, __FUNCTION__, wlc_txcfg_max_clients_get(wlc->txcfg, DLOFDMA),
		musched->num_dlofdma_users, musched->ulosched->num_usrs));

	if (!wlc->pub->up) {
		ASSERT(!musched->num_dlofdma_users && !musched->ulosched->num_usrs);
	}
}

static void
wlc_musched_bsscfg_state_upd(void *ctx, bsscfg_state_upd_data_t *notif_data)
{
	wlc_muscheduler_info_t *musched = (wlc_muscheduler_info_t *) ctx;
	wlc_bsscfg_t *cfg = notif_data->cfg;

	/* up -> down */
	if (notif_data->old_up && !cfg->up) {
		wlc_musched_admit_users_reset(musched, cfg);
	}
}

static void
wlc_musched_scb_state_upd(void *ctx, scb_state_upd_data_t *notif_data)
{
	wlc_muscheduler_info_t *musched = (wlc_muscheduler_info_t *) ctx;
	wlc_info_t *wlc = musched->wlc;
	scb_t *scb;
	wlc_bsscfg_t *bsscfg;
	uint8 oldstate, bw;
	scb_musched_t *musched_scb;
	bool ofdma_en = TRUE;
	ASSERT(notif_data);

	scb = notif_data->scb;
	ASSERT(scb);
	oldstate = notif_data->oldstate;
	bsscfg = scb->bsscfg;

	BCM_REFERENCE(wlc);
	BCM_REFERENCE(oldstate);

	musched_scb = SCB_MUSCHED(musched, scb);

	if (D11REV_LT(wlc->pub->corerev, 129) || !HE_DLMU_ENAB(wlc->pub) ||
		!SCB_HE_CAP(scb) || !BSSCFG_AP(bsscfg) ||
#ifndef WL11AX
		/* force to return if WL11AX compilation flag is off */
		TRUE ||
#endif /* WL11AX */
		SCB_INTERNAL(scb) || !musched_scb) {
		return;
	}

	bw = wlc_scb_ratesel_get_link_bw(wlc, scb);
	/* if the sta is already admitted as HE MU-MIMO, skip */
	if (wlc_txbf_is_hemmu_enab(wlc->txbf, scb) && SCB_HEMMU(scb)) {
		ofdma_en = FALSE;
	}

	/* skip admitting as dlofdma sta, if hemmu is yet to be enabled */
	if (HE_MMU_ENAB(wlc->pub) && !wlc_mutx_is_hemmu_enab(wlc->mutx, bw)) {
		ofdma_en = FALSE;
	}
	/* only DL OFDMA capable SCBs come here. */
	if ((!WSEC_ENABLED(bsscfg->wsec) && !(oldstate & ASSOCIATED) &&
		SCB_AUTHENTICATED(scb) && SCB_ASSOCIATED(scb)) ||
		(WSEC_ENABLED(bsscfg->wsec) && SCB_AUTHENTICATED(scb) &&
		SCB_ASSOCIATED(scb) && SCB_AUTHORIZED(scb) && !(oldstate & AUTHORIZED))) {
		/* open security: not assoc -> assoc
		 * security: assoc -> authorized
		 * Allocate OFDMA FIFO if all of these are true
		 * 1. PIO is disabled.
		 * 2. SCB does not require 4 address data frame as in the case of DWDS/WDS.
		 * 3. SCB has less than or equal number of max allowed streams OFDMA.
		 * 4. Num of DL OFDMA clients is less than max allowed OFDMA clients.
		 */
		if (ofdma_en) {
			if (!PIO_ENAB_HW(wlc->wlc_hw) && !SCB_A4_DATA(scb) &&
				(wlc_he_get_omi_tx_nsts(musched->wlc->hei, scb) <=
					MUMAX_NSTS_ALLOWED) &&
				(musched->num_dlofdma_users <
					wlc_txcfg_max_clients_get(wlc->txcfg, DLOFDMA))) {
				/* In case of re-association, AC_BE FIFO was previosuly allocated
				 * No need to allocate again. Otherwise allocate a new FIFO.
				 */
				if (wlc_fifo_index_peek(musched->wlc->fifo, scb, AC_BE) == 0)
					wlc_fifo_alloc(wlc->fifo, scb, MU_TYPE_OFDMA, AC_BE);
				/* If SCB has a MU FIFO then record it as OFDMA client */
				if (wlc_fifo_index_peek(musched->wlc->fifo, scb, AC_BE) != 0) {
					musched->num_dlofdma_users++;
					ASSERT(musched->num_dlofdma_users <=
						wlc->pub->tunables->maxscb);
					musched_scb->dlul_assoc = TRUE;
				}
			}
		}
		/* Let the rx monitoring trigger the UL MU admission */
	} else if ((oldstate & ASSOCIATED) && !(SCB_ASSOCIATED(scb) && SCB_AUTHENTICATED(scb))) {
		if (musched_scb->dlul_assoc == TRUE && ofdma_en) {
			ASSERT(musched->num_dlofdma_users >= 1);
			musched->num_dlofdma_users--;
			musched_scb->dlul_assoc = FALSE;
		}
		wlc_musched_admit_ulclients(wlc, scb, FALSE);
	} else {
		/* pass */
	}

	/* XXX: now let's just use simple policy to enable dl ofdma
	 * Current policy to enable dl ofdma: for AP: AP HE features = 7;
	 * for STA: 1) HE cap; 2) Associated; 3) total # of HE STA > 1
	 */
	if (ofdma_en) {
		wlc_musched_admit_dlclients(musched);
	}
}

int
wlc_musched_set_dlpolicy(wlc_muscheduler_info_t *musched, int16 dl_policy)
{
	int err = BCME_OK;

	if (dl_policy > MUSCHED_DL_POLICY_MAX) {
		musched->dl_policy = MUSCHED_DL_POLICY_AUTO;
	} else {
		musched->dl_policy = dl_policy;
	}

	if (!HE_DLMU_ENAB(musched->wlc->pub)) {
		MUSCHED_DLOFDMA_DISABLE(musched);
	} else {
		switch (musched->dl_policy) {
			case MUSCHED_DL_POLICY_DISABLE:
				musched->dl_schidx = MCTL2_INVALID_SCHIDX;
				MUSCHED_DLOFDMA_DISABLE(musched);
				break;
			case MUSCHED_DL_POLICY_FIXED:
				musched->dl_schidx = MCTL2_FIXED_SCHIDX;
				MUSCHED_DLOFDMA_ENABLE(musched);
				break;
			case MUSCHED_DL_POLICY_AUTO:
				/* auto is default to trivial scheduler */
			case MUSCHED_DL_POLICY_TRIVIAL:
				musched->dl_schidx = MCTL2_TRIVIAL_SCHIDX;
				MUSCHED_DLOFDMA_ENABLE(musched);
				break;
			default:
				musched->dl_schidx = MCTL2_INVALID_SCHIDX;
				MUSCHED_DLOFDMA_DISABLE(musched);
				WL_ERROR(("wl%d: %s: incorrect musched dl ofdma sch policy %d\n",
					musched->wlc->pub->unit, __FUNCTION__, musched->dl_policy));
				ASSERT(0);
				break;
		}
	}

	WL_MUTX(("wl%d: %s: set musched dl ofdma sch policy %d schidx %d\n",
		musched->wlc->pub->unit, __FUNCTION__,
		musched->dl_policy, musched->dl_schidx));

	return err;
}

bool
wlc_musched_is_dlofdma_allowed(wlc_muscheduler_info_t *musched)
{
	return MUSCHED_DLOFDMA(musched);
}

static int
wlc_musched_get_dlpolicy(wlc_muscheduler_info_t *musched)
{
	return musched->dl_policy;
}

static void
wlc_musched_set_ulpolicy(wlc_muscheduler_info_t *musched, int16 ul_policy)
{
	if (ul_policy > MUSCHED_UL_POLICY_MAX) {
		musched->ul_policy = MUSCHED_UL_POLICY_AUTO;
	} else {
		musched->ul_policy = ul_policy;
	}
}

static int
wlc_musched_get_ulpolicy(wlc_muscheduler_info_t *musched)
{
	return musched->ul_policy;
}

/* ======== scb cubby ======== */

static int
wlc_musched_scb_init(void *ctx, scb_t *scb)
{
	wlc_muscheduler_info_t *musched = ctx;
	wlc_info_t *wlc = musched->wlc;
	scb_musched_t **psh = SCB_MUSCHED_CUBBY(musched, scb);
	scb_musched_t *sh = SCB_MUSCHED(musched, scb);

	ASSERT(sh == NULL);

	*psh = wlc_scb_sec_cubby_alloc(wlc, scb, sizeof(*sh));

	wlc_musched_scb_schedule_init(musched, scb);

#if defined(BCMDBG) || defined(DL_RU_STATS_DUMP)
	if (HE_DLMU_ENAB(wlc->pub)) {
		wlc_musched_scb_rustats_init(musched, scb);
	}
#endif // endif

#if defined(BCMDBG) || defined(UL_RU_STATS_DUMP)
	if (HE_ULMU_ENAB(wlc->pub)) {
		wlc_musched_scb_ulstats_init(musched, scb);
	}
#endif // endif

	return BCME_OK;
}

static void
wlc_musched_scb_deinit(void *ctx, scb_t *scb)
{
	wlc_muscheduler_info_t *musched = ctx;
	wlc_info_t *wlc = musched->wlc;
	scb_musched_t **psh = SCB_MUSCHED_CUBBY(musched, scb);
	scb_musched_t *sh = SCB_MUSCHED(musched, scb);
	int i;
	uint16 max_ulofdma_usrs = wlc_txcfg_max_clients_get(wlc->txcfg, ULOFDMA);

	/* Memory not allocated for scb, return */
	if (!sh) {
		return;
	}

	if (sh->dlul_assoc == TRUE) {
		WL_MUTX(("wl%d: %s STA "MACF" removed\n",
			wlc->pub->unit, __FUNCTION__, ETHER_TO_MACF(scb->ea)));
		ASSERT(musched->num_dlofdma_users >= 1);
		musched->num_dlofdma_users--;
	}

	for (i = 0; i < max_ulofdma_usrs; i++) {
		if (musched->ulosched->scb_list[i] == scb) {
			ASSERT(musched->ulosched->num_usrs >= 1);
			musched->ulosched->num_usrs--;
			musched->ulosched->scb_list[i] = NULL;
		}
	}

	if (sh->scb_ru_stats != NULL) {
		MFREE(wlc->osh, sh->scb_ru_stats, sizeof(musched_ru_stats_t));
		--musched->num_scb_stats;
		ASSERT(musched->num_scb_stats >= 0);
	}

	if (sh->scb_ul_stats != NULL) {
		MFREE(wlc->osh, sh->scb_ul_stats, sizeof(musched_ul_stats_t));
		ASSERT(musched->ulosched != NULL);
		--musched->ulosched->num_scb_ulstats;
		ASSERT(musched->ulosched->num_scb_ulstats >= 0);
	}

	if (sh->ul_rmem != NULL) {
		MFREE(wlc->osh, sh->ul_rmem, sizeof(d11ratemem_ulrx_rate_t));
		sh->ul_rmem = NULL;
	}

	wlc_scb_sec_cubby_free(wlc, scb, sh);
	*psh = NULL;
}

static uint
wlc_musched_scb_secsz(void *ctx, scb_t *scb)
{
	scb_musched_t *sh;
	return sizeof(*sh);
}

#if defined(BCMDBG) || defined(DL_RU_STATS_DUMP)
static void
wlc_musched_scb_dump(void *ctx, scb_t *scb, bcmstrbuf_t *b)
{
	char musched_str[64];
	wlc_muscheduler_info_t *musched = ctx;
	scb_musched_t *sh = SCB_MUSCHED(musched, scb);

	if (sh == NULL) {
		return;
	}

	bcm_bprintf(b, "     musched_info: %s ", musched_str);
	bcm_bprintf(b, "     DL schpos %d UL schpos %d\n",
		sh->dl_schpos, sh->ul_schpos);
}

static void
wlc_musched_scb_rustats_init(wlc_muscheduler_info_t *musched, scb_t *scb)
{
	scb_musched_t *musched_scb;

	if (PIO_ENAB_HW(musched->wlc->wlc_hw)) {
		return;
	}

	if (SCB_INTERNAL(scb)) {
		return;
	}

	musched_scb = SCB_MUSCHED(musched, scb);

	if (musched_scb && musched->num_scb_stats < MUSCHED_RU_SCB_STATS_NUM) {
		if ((musched_scb->scb_ru_stats =
			MALLOCZ(musched->wlc->osh, sizeof(musched_ru_stats_t))) != NULL) {
			++musched->num_scb_stats;
		}
	} else {
		musched_scb->scb_ru_stats = NULL;
	}
}
#endif // endif

#if defined(BCMDBG) || defined(UL_RU_STATS_DUMP)
static void
wlc_musched_scb_ulstats_init(wlc_muscheduler_info_t *musched, scb_t *scb)
{
	scb_musched_t *musched_scb;

	if (PIO_ENAB_HW(musched->wlc->wlc_hw)) {
		return;
	}

	if (SCB_INTERNAL(scb)) {
		return;
	}

	musched_scb = SCB_MUSCHED(musched, scb);

	ASSERT(musched->ulosched != NULL);
	if (musched_scb && musched->ulosched->num_scb_ulstats < MUSCHED_UL_SCB_STATS_NUM) {
		if ((musched_scb->scb_ul_stats =
			MALLOCZ(musched->wlc->osh, sizeof(musched_ul_stats_t))) != NULL) {
			++musched->ulosched->num_scb_ulstats;
			musched_scb->scb_ul_stats->ul_rssi_stats.min_rssi =
				PHYRSSI_2SCOMPLEMENT << OPERAND_SHIFT;
			musched_scb->scb_ul_stats->ul_rssi_stats.max_rssi =
				-PHYRSSI_2SCOMPLEMENT << OPERAND_SHIFT;
		}
	} else {
		musched_scb->scb_ul_stats = NULL;
	}
}
#endif // endif

static void
wlc_musched_scb_schedule_init(wlc_muscheduler_info_t *musched, scb_t *scb)
{
	scb_musched_t *musched_scb;

	if (PIO_ENAB_HW(musched->wlc->wlc_hw)) {
		return;
	}

	if (SCB_INTERNAL(scb)) {
		return;
	}

	musched_scb = SCB_MUSCHED(musched, scb);
	if (!musched_scb) {
		return;
	}

	musched_scb->dl_schpos = MCTL2_INVALID_SCHPOS;
	musched_scb->ul_schpos = MUSCHED_ULOFDMA_INVALID_SCHPOS;
}

bool
wlc_scbmusched_is_dlofdma(wlc_muscheduler_info_t *musched, scb_t* scb)
{
	if (musched->dl_policy == MUSCHED_DL_POLICY_DISABLE ||
		SCB_INTERNAL(scb)) {
		return FALSE;
	}

	return  (SCB_DLOFDMA(scb));
}

/* Function to check eligibility of a SCB/client to be admitted as DL-OFDMA */
bool
wlc_musched_scb_isdlofdma_eligible(wlc_muscheduler_info_t *musched, scb_t* scb)
{
	bool ret = FALSE;
	scb_musched_t *musched_scb = SCB_MUSCHED(musched, scb);

	if (musched_scb && SCB_HE_CAP(scb) && HE_DLMU_ENAB(musched->wlc->pub) &&
		BSSCFG_AP(SCB_BSSCFG(scb)) && wlc_fifo_index_peek(musched->wlc->fifo, scb, AC_BE)) {
		ret = TRUE;
	}
	return ret;
}

/* API: turn on/off DLOFDMA for scb */
void
wlc_scbmusched_set_dlofdma(wlc_muscheduler_info_t *musched, scb_t* scb, bool enable)
{
	scb_musched_t *musched_scb;
	wlc_info_t *wlc;
#ifdef MAC_AUTOTXV_OFF
	uint8 fifo_idx;
#endif // endif

	wlc = musched->wlc;
	BCM_REFERENCE(wlc);

	if (SCB_INTERNAL(scb)) {
		return;
	}

	if ((musched_scb = SCB_MUSCHED(musched, scb)) == NULL) {
		WL_ERROR(("wl%d: %s: Fail to get musched scb cubby STA "MACF"\n",
			wlc->pub->unit, __FUNCTION__,
			ETHER_TO_MACF(scb->ea)));
		return;
	}

	if (enable) {
		SCB_DLOFDMA_ENABLE(scb);
	} else {
		SCB_DLOFDMA_DISABLE(scb);

	}
	WL_MUTX(("wl%d: %s: %s dl ofdma STA "MACF" %x\n", wlc->pub->unit,
		__FUNCTION__, enable ? "Enable" : "Disable", ETHER_TO_MACF(scb->ea),
		SCB_DLOFDMA(scb)));

#ifdef MAC_AUTOTXV_OFF
	if (wlc_scbmusched_fifogrpidx_get(musched, scb, &fifo_idx) == BCME_OK) {
		/* set tbcap in bfi config */
		wlc_txbf_tbcap_update(wlc->txbf, scb);
		if (wlc_macreq_upd_bfi(wlc, scb, fifo_idx, enable) != BCME_OK) {
			WL_ERROR(("wl%d: %s MAC command failed to update bfi"
			" for STA "MACF" index %d\n", wlc->pub->unit, __FUNCTION__,
			ETHER_TO_MACF(scb->ea), fifo_idx));
		}
	}
#endif // endif
}

/* function to configure musched scb dl mu scheduler position for scb */
int
wlc_scbmusched_set_dlschpos(wlc_muscheduler_info_t *musched, scb_t* scb, int8 schpos)
{
	scb_musched_t *musched_scb;
	int err;

	err = BCME_OK;

	if (SCB_INTERNAL(scb)) {
		return BCME_UNSUPPORTED;
	}

	musched_scb = SCB_MUSCHED(musched, scb);

	if (musched_scb == NULL) {
		return BCME_ERROR;
	}

	musched_scb->dl_schpos = schpos;

	return err;
}

/* function to get musched scb dl mu scheduler info */
int
wlc_scbmusched_get_dlsch(wlc_muscheduler_info_t *musched, scb_t* scb, int8* schidx, int8* schpos)
{
	scb_musched_t *musched_scb;
	int err;

	err = BCME_OK;

	if (SCB_INTERNAL(scb)) {
		return BCME_UNSUPPORTED;
	}

	musched_scb = SCB_MUSCHED(musched, scb);

	if (musched_scb == NULL) {
		return BCME_ERROR;
	}

	*schidx = musched->dl_schidx;
	*schpos = musched_scb->dl_schpos;

	return err;
}

static void
wlc_musched_config_he_sounding_type(wlc_muscheduler_info_t *musched)
{
	uint16 uval16;
	wlc_info_t *wlc = musched->wlc;

	/* Unset the magic bit for bfp based HE MU frame sequence */
	uval16 = wlc_read_shm(wlc, M_BFI_GENCFG(wlc));
	uval16 &= ~(1 << C_BFIGEN_HESER_NBIT);
	wlc_write_shm(wlc, M_BFI_GENCFG(wlc), uval16);

	if (wlc_musched_get_he_sounding_type(musched) == MUSCHED_HESNDTP_SEQHESU) {
		/* do nothing */
	} else if (wlc_musched_get_he_sounding_type(musched) == MUSCHED_HESNDTP_BFP) {
		uval16 = wlc_read_shm(wlc, M_BFI_GENCFG(wlc));
		uval16 |= (1 << C_BFIGEN_HESER_NBIT);
		wlc_write_shm(wlc, M_BFI_GENCFG(wlc), uval16);
	}
}

uint8
wlc_musched_get_he_sounding_type(wlc_muscheduler_info_t *musched)
{
	return (musched->sndtp <= MUSCHED_HESNDTP_AUTO) ? MUSCHED_HESNDTP_SEQHESU : musched->sndtp;
}

static int
wlc_musched_upd_rucfg_rmem(wlc_muscheduler_info_t *musched, uint8 bw_idx)
{
	wlc_info_t *wlc = musched->wlc;
	int i, j, rate_blk_le8, rate_blk_gt8;
	uint16 *rutbl_buf = NULL;
	uint16 *ptr = NULL;
	int ret = BCME_OK;

	if ((rutbl_buf = MALLOCZ(wlc->osh, MUSCHED_RUCFG_TBL_BSZ)) == NULL) {
		WL_ERROR(("wl%d: %s: out of memory for rucfg, %d / %d bytes\n", wlc->pub->unit,
			__FUNCTION__, MALLOCED(wlc->osh), MUSCHED_RUCFG_TBL_BSZ));
		ret = BCME_NOMEM;
		goto done;
	}

	STATIC_ASSERT(MUSCHED_RUCFG_LE8_BW20_40_RTMEM >= AMT_IDX_RLM_RSVD_START &&
		MUSCHED_RUCFG_LE8_BW20_40_RTMEM < AMT_IDX_SIZE_11AX);
	STATIC_ASSERT(MUSCHED_RUCFG_LE8_BW80_160_RTMEM >= AMT_IDX_RLM_RSVD_START &&
		MUSCHED_RUCFG_LE8_BW80_160_RTMEM < AMT_IDX_SIZE_11AX);
	STATIC_ASSERT(MUSCHED_RUCFG_GT8_BW20_RTMEM >= AMT_IDX_RLM_RSVD_START &&
		MUSCHED_RUCFG_GT8_BW20_RTMEM < AMT_IDX_SIZE_11AX);
	STATIC_ASSERT(MUSCHED_RUCFG_GT8_BW40_RTMEM >= AMT_IDX_RLM_RSVD_START &&
		MUSCHED_RUCFG_GT8_BW40_RTMEM < AMT_IDX_SIZE_11AX);
	STATIC_ASSERT(MUSCHED_RUCFG_GT8_BW80_RTMEM >= AMT_IDX_RLM_RSVD_START &&
		MUSCHED_RUCFG_GT8_BW80_RTMEM < AMT_IDX_SIZE_11AX);
	STATIC_ASSERT(MUSCHED_RUCFG_GT8_BW160_RTMEM >= AMT_IDX_RLM_RSVD_START &&
		MUSCHED_RUCFG_GT8_BW160_RTMEM < AMT_IDX_SIZE_11AX);

	switch (bw_idx) {
	case D11_REV128_BW_20MHZ:
		rate_blk_le8 = MUSCHED_RUCFG_LE8_BW20_40_RTMEM;
		rate_blk_gt8 = MUSCHED_RUCFG_GT8_BW20_RTMEM;
		break;
	case D11_REV128_BW_40MHZ:
		rate_blk_le8 = MUSCHED_RUCFG_LE8_BW20_40_RTMEM;
		rate_blk_gt8 = MUSCHED_RUCFG_GT8_BW40_RTMEM;
		break;
	case D11_REV128_BW_80MHZ:
		rate_blk_le8 = MUSCHED_RUCFG_LE8_BW80_160_RTMEM;
		rate_blk_gt8 = MUSCHED_RUCFG_GT8_BW80_RTMEM;
		break;
	case D11_REV128_BW_160MHZ:
		rate_blk_le8 = MUSCHED_RUCFG_LE8_BW80_160_RTMEM;
		rate_blk_gt8 = MUSCHED_RUCFG_GT8_BW160_RTMEM;
		break;
	default:
		ret = BCME_ERROR;
		goto done;
	}

	/* Preparing table for row 1 - 8 */
	ptr = rutbl_buf;
	for (i = 0; i < MUSCHED_RUCFG_ROW; i++) {
		for (j = 0; j < MUSCHED_RUCFG_LE8_NUM_COLS; j++) {
			if (i < MUSCHED_RUCFG_LE8_NUM_ROWS) {
				/* write upper half of the table. bw20/bw80 is on upper half */
				*ptr++ = (musched->rucfg_ack[bw_idx & (~1)][i][j] <<
					MUSCHED_RUCFG_ACK_SHIFT) |
					musched->rucfg[bw_idx & (~1)][i][j];
			} else {
				/* write bottmo half of the table. bw40/bw160 is on lower half */
				*ptr++ = (musched->rucfg_ack[bw_idx | 1][i-
					MUSCHED_RUCFG_LE8_NUM_ROWS][j] << MUSCHED_RUCFG_ACK_SHIFT) |
					musched->rucfg[bw_idx | 1][i-MUSCHED_RUCFG_LE8_NUM_ROWS][j];
			}
		}
	}

	wlc_bmac_suspend_macx_and_wait(wlc->hw);
	/* Writing table for rows 1 - 8 */
	wlc_ratelinkmem_write_rucfg(wlc, (uint8*)rutbl_buf, MUSCHED_RUCFG_TBL_BSZ, rate_blk_le8);

	/* Preparing table for rows 9 - 16 */
	ptr = rutbl_buf;
	for (i = 0; i < MUSCHED_RUCFG_GT8_NUM_ROWS; i++) {
		for (j = 0; j < MUSCHED_RUCFG_GT8_NUM_COLS; j++) {
			*ptr++ = (musched->rucfg_ack[bw_idx][i+MUSCHED_RUCFG_LE8_NUM_ROWS][j] <<
				MUSCHED_RUCFG_ACK_SHIFT) |
				musched->rucfg[bw_idx][i+MUSCHED_RUCFG_LE8_NUM_ROWS][j];
		}
	}

	/* Writing table for rows 9 - 16 */
	wlc_ratelinkmem_write_rucfg(wlc, (uint8*)rutbl_buf, MUSCHED_RUCFG_TBL_BSZ, rate_blk_gt8);
	wlc_bmac_enable_macx(wlc->hw);

done:
	if (rutbl_buf) {
		MFREE(wlc->osh, rutbl_buf, MUSCHED_RUCFG_TBL_BSZ);
	}
	return ret;
}

/* Function to get empty slot in ul_usr_list
 * If there is empty slot return the index
 * Otherwise, return -1
 */
static int8
wlc_musched_ulofdma_get_empty_schpos(wlc_musched_ulofdma_info_t *ulosched)
{
	int8 schpos;
	wlc_info_t *wlc = ulosched->musched->wlc;
	uint16 max_ulofdma_usrs = wlc_txcfg_max_clients_get(wlc->txcfg, ULOFDMA);

	if (ulosched->num_usrs >= max_ulofdma_usrs) {
		return  MUSCHED_ULOFDMA_INVALID_SCHPOS;
	}
	for (schpos = 0; schpos < max_ulofdma_usrs; schpos++) {
		if (ulosched->scb_list[schpos] == NULL) {
			return schpos;
		}
	}
	ASSERT(!"ULOFDMA NO EMPTY SLOT");
	return MUSCHED_ULOFDMA_INVALID_SCHPOS;
}

/* Function to find if a given scb in ul_usr_list
 * If yes, then return the schpos; Otherwise, return -1
 */
static int8
wlc_musched_ulofdma_find_scb(wlc_musched_ulofdma_info_t *ulosched, scb_t *scb)
{
	int8 i, schpos = MUSCHED_ULOFDMA_INVALID_SCHPOS;
	wlc_info_t *wlc = ulosched->musched->wlc;
	for (i = 0; i < wlc_txcfg_max_clients_get(wlc->txcfg, ULOFDMA); i++) {
		if (ulosched->scb_list[i] == scb) {
			schpos = i;
			break;
		}
	}
	return schpos;
}

/* Function to find if a given ether addr in ul_usr_list
 * If yes, then return the schpos and set scb pointer; Otherwise, return -1 and NULL scb pointer
 */
static int8
wlc_musched_ulofdma_find_addr(wlc_musched_ulofdma_info_t *ulosched, struct ether_addr *ea,
	scb_t *scb)
{
	int8 idx, schpos = MUSCHED_ULOFDMA_INVALID_SCHPOS;
	wlc_bsscfg_t *bsscfg;
	wlc_info_t *wlc = ulosched->musched->wlc;
	char eabuf[ETHER_ADDR_STR_LEN];

	BCM_REFERENCE(eabuf);

	scb = NULL;

	FOREACH_BSS(wlc, idx, bsscfg) {
		if ((scb = wlc_scbfind(wlc, bsscfg, ea))) {
			schpos = wlc_musched_ulofdma_find_scb(ulosched, scb);
			break;
		}
	}
	WL_MUTX(("wl%d: %s: addr %s schpos %d scb %p\n",
		wlc->pub->unit, __FUNCTION__,
		scb ? bcm_ether_ntoa(&scb->ea, eabuf) : "null", schpos, scb));
	return schpos;
}

/* Add the new user to ul ofdma scheduler. Return TRUE if added; otherwise FALSE */
static bool
wlc_musched_ulofdma_add_usr(wlc_musched_ulofdma_info_t *ulosched, scb_t *scb)
{
	scb_musched_t *musched_scb;
	wlc_muscheduler_info_t *musched = ulosched->musched;
	wlc_info_t *wlc = musched->wlc;
	d11ratemem_ulrx_rate_t *rmem;
	int i;
	uint8 nss;
	int8 schpos;

	STATIC_ASSERT((MUSCHED_ULOFDMA_FIRST_USER >= AMT_IDX_RLM_RSVD_SIZE) &&
		(MUSCHED_ULOFDMA_FIRST_USER < AMT_IDX_SIZE_11AX));
	STATIC_ASSERT((MUSCHED_ULOFDMA_LAST_USER >= AMT_IDX_RLM_RSVD_SIZE) &&
		(MUSCHED_ULOFDMA_LAST_USER < AMT_IDX_SIZE_11AX));
	STATIC_ASSERT(MUSCHED_ULOFDMA_FIRST_USER <= MUSCHED_ULOFDMA_LAST_USER);
	STATIC_ASSERT(MUSCHED_ULOFDMA_LAST_USER < MUSCHED_RUCFG_LE8_BW20_40_RTMEM);

	if (ulosched->num_usrs >= wlc_txcfg_max_clients_get(wlc->txcfg, ULOFDMA)) {
		WL_MUTX(("wl%d: %s: number of ul ofdma users %d exceeds limit %d\n",
			wlc->pub->unit, __FUNCTION__,
			ulosched->num_usrs, wlc_txcfg_max_clients_get(wlc->txcfg, ULOFDMA)));
		return FALSE;
	}

	if ((musched_scb = SCB_MUSCHED(musched, scb)) == NULL) {
		WL_MUTX(("wl%d: %s: Fail to get musched scb cubby STA "MACF"\n",
			wlc->pub->unit, __FUNCTION__,
			ETHER_TO_MACF(scb->ea)));
		return FALSE;
	}

	ASSERT(musched_scb->ul_rmem == NULL);
	schpos = wlc_musched_ulofdma_get_empty_schpos(ulosched);
	if (schpos == MUSCHED_ULOFDMA_INVALID_SCHPOS) {
		return FALSE;
	}

	if (!(rmem = MALLOCZ(wlc->osh, sizeof(d11ratemem_ulrx_rate_t)))) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n", wlc->pub->unit,
			__FUNCTION__, MALLOCED(wlc->osh)));
		ASSERT(1);
		return FALSE;
	}

	musched_scb->ul_schpos = schpos;
	musched_scb->ul_rmemidx = MUSCHED_ULOFDMA_FIRST_USER + schpos;
	SCB_ULOFDMA_ENABLE(scb);

	/* populate rate block to ucfg: autorate by default */
	D11_ULOTXD_UCFG_SET_MCSNSS(musched_scb->ucfg, 0x17);
	D11_ULOTXD_UCFG_SET_TRSSI(musched_scb->ucfg,
		wlc_lq_rssi_get(wlc, SCB_BSSCFG(scb), scb));

	/* check if need to override with global fixed settings */
	if ((ulosched->g_ucfg & D11_ULOTXD_UCFG_FIXRT_MASK)) {
		D11_ULOTXD_UCFG_SET_MCSNSS(musched_scb->ucfg,
			D11_ULOTXD_UCFG_GET_MCSNSS(ulosched->g_ucfg));
		musched_scb->ucfg |= D11_ULOTXD_UCFG_FIXRT_MASK;
	}

	if ((ulosched->g_ucfg & D11_ULOTXD_UCFG_FIXRSSI_MASK)) {
		D11_ULOTXD_UCFG_SET_TRSSI(musched_scb->ucfg,
			D11_ULOTXD_UCFG_GET_TRSSI(ulosched->g_ucfg));
		musched_scb->ucfg |= D11_ULOTXD_UCFG_FIXRSSI_MASK;
	}

	/* TODO: now just use hardcoded bw80 mcsmap */
	for (i = 0; i < ARRAYSIZE(rmem->mcsbmp); i++) {
		rmem->mcsbmp[i] = HE_MAX_MCS_TO_MCS_MAP(
			(((scb->rateset.he_bw80_tx_mcs_nss >> (i*2)) & 0x3)));
	}
	nss = HE_MAX_SS_SUPPORTED(scb->rateset.he_bw80_tx_mcs_nss); /* 1-based NSS */
	D11_ULORMEM_RTCTL_SET_MCS(rmem->rtctl,
		HE_MAX_MCS_TO_INDEX(HE_MCS_MAP_TO_MAX_MCS(rmem->mcsbmp[0])));
	D11_ULORMEM_RTCTL_SET_NSS(rmem->rtctl, nss == 0 ? 0 : nss-1);
	WL_MUTX(("wl%d: %s: rtctl 0x%x mcsmap 0x%x sta addr "MACF"\n",
		wlc->pub->unit, __FUNCTION__,
		rmem->rtctl, scb->rateset.he_bw80_tx_mcs_nss,
		ETHER_TO_MACF(scb->ea)));
	rmem->aggnma = (wlc_ampdu_rx_get_ba_max_rx_wsize(wlc->ampdu_rx) << 4);
	rmem->mlenma[0] = 0;
	rmem->mlenma[1] = 0x30; // AMSDU byte size 3072 << 10

	ulosched->scb_list[schpos] = scb;
	musched_scb->ul_rmem = rmem;
	musched_scb->upd_ul_rmem = TRUE;
	ulosched->num_usrs++;
	WL_MUTX(("wl%d: %s: add sta "MACF" schpos %d state 0x%x into ul ofdma list\n",
		wlc->pub->unit, __FUNCTION__,
		ETHER_TO_MACF(scb->ea), schpos, scb->state));
	musched_scb->state = MUSCHED_SCB_ADMT;

	return TRUE;
}

/* Remove a given user from ul ofdma scheduler. Return TRUE if removed; otherwise FALSE */
static bool
wlc_musched_ulofdma_del_usr(wlc_musched_ulofdma_info_t *ulosched, scb_t *scb)
{
	wlc_muscheduler_info_t *musched = ulosched->musched;
	wlc_info_t *wlc = musched->wlc;
	bool ret = FALSE;
	scb_musched_t *musched_scb;
	int8 schpos;

	if (SCB_INTERNAL(scb)) {
		return ret;
	}

	if ((musched_scb = SCB_MUSCHED(musched, scb)) == NULL) {
		WL_MUTX(("wl%d: %s: Fail to get musched scb cubby STA "MACF"\n",
			wlc->pub->unit, __FUNCTION__,
			ETHER_TO_MACF(scb->ea)));
		return ret;
	}

	if ((schpos = wlc_musched_ulofdma_find_scb(ulosched, scb)) !=
		MUSCHED_ULOFDMA_INVALID_SCHPOS) {
		SCB_ULOFDMA_DISABLE(scb);
		wlc_musched_ul_oper_state_upd(musched, scb, MUSCHED_SCB_INIT);
		musched_scb->ul_schpos = MUSCHED_ULOFDMA_INVALID_SCHPOS;
		ulosched->scb_list[schpos] = NULL;
		ASSERT(ulosched->num_usrs >= 1);
		ulosched->num_usrs--;
		if (musched_scb->ul_rmem != NULL) {
			MFREE(wlc->osh, musched_scb->ul_rmem, sizeof(d11ratemem_ulrx_rate_t));
			musched_scb->ul_rmem = NULL;
		}
		WL_MUTX(("wl%d: %s: Remove sta "MACF" schpos %d state %x from ul ofdma list\n",
			wlc->pub->unit, __FUNCTION__,
			ETHER_TO_MACF(scb->ea), schpos, scb->state));
		ret = TRUE;
	}
	return ret;
}

/* Determine if a STA is eligible to be admitted into ul ofdma list */
static bool
wlc_musched_scb_isulofdma_eligible(wlc_musched_ulofdma_info_t *ulosched, scb_t* scb)
{
	bool ret;
	wlc_muscheduler_info_t* musched = ulosched->musched;
	wlc_info_t* wlc = musched->wlc;
	ret = (HE_ULMU_ENAB(wlc->pub) && scb && !SCB_INTERNAL(scb) &&
		SCB_HE_CAP(scb) && SCB_AUTHENTICATED(scb) && SCB_ASSOCIATED(scb) &&
		(SCB_IS_BRCM(scb) || (!ulosched->brcm_only)) &&
		(!ulosched->ban_nonbrcm_160sta || !wlc_he_is_nonbrcm_160sta(wlc->hei, scb)) &&
		(!WSEC_ENABLED(SCB_BSSCFG(scb)->wsec) ||
		(WSEC_ENABLED(SCB_BSSCFG(scb)->wsec) && SCB_AUTHORIZED(scb))) &&
		!SCB_ULOFDMA(scb) && !BSSCFG_STA(SCB_BSSCFG(scb)) && !SCB_DWDS(scb) &&
		wlc_he_get_ulmu_allow(wlc->hei, scb) &&
		(musched->ul_policy != MUSCHED_UL_POLICY_DISABLE) &&
		((wlc_scb_ampdurx_on(scb) && ulosched->always_admit != 2) ||
		ulosched->always_admit == 1 ||
		wlc_twt_scb_is_trig_mode(wlc->twti, scb)) &&
		(wlc_he_get_omi_tx_nsts(wlc->hei, scb) <= MUMAX_NSTS_ALLOWED));
	/* unconditionally admit twt user in trigger mode */
	if (ret && !ulosched->always_admit && !wlc_twt_scb_is_trig_mode(wlc->twti, scb)) {
		int rssi;
		/* additional admit criteria */
		/* 1) a-mpdu traffic meets certain threshold
		 * 2) rssi > min_rssi
		 */
#ifdef WLCNTSCB
		scb_musched_t *musched_scb;

		if ((musched_scb = SCB_MUSCHED(musched, scb)) == NULL) {
			WL_MUTX(("wl%d: %s: Fail to get musched scb cubby STA "MACF"\n",
				wlc->pub->unit, __FUNCTION__, ETHER_TO_MACF(scb->ea)));
			return ret;
		}
		if (((uint32)scb->scb_stats.rx_ucast_pkts - (uint32)musched_scb->last_rx_pkts <
			ulosched->rx_pktcnt_thrsh) || musched_scb->last_rx_pkts == 0) {
			ret = FALSE;
		}

#endif /* WLCNTSCB */

		rssi = wlc_lq_rssi_get(wlc, SCB_BSSCFG(scb), scb);
		if (rssi < ulosched->min_rssi) {
			ret = FALSE;
		}
	}
	return ret;
}

/* Function to try to add or delete a usr from ULOFDMA usr list
 * If a user is successfully added or deleted, return TRUE
 * otherwise FALSE
 */
static bool
wlc_scbmusched_set_ulofdma(wlc_musched_ulofdma_info_t *ulosched, scb_t* scb, bool ulofdma)
{
	wlc_muscheduler_info_t *musched = ulosched->musched;
	scb_musched_t *musched_scb;
	wlc_info_t *wlc;
	bool ret = FALSE;

	wlc = musched->wlc;
	BCM_REFERENCE(wlc);

	if (SCB_INTERNAL(scb)) {
		return ret;
	}

	if ((musched_scb = SCB_MUSCHED(musched, scb)) == NULL) {
		WL_MUTX(("wl%d: %s: Fail to get musched scb cubby STA "MACF"\n",
			wlc->pub->unit, __FUNCTION__, ETHER_TO_MACF(scb->ea)));
		return ret;
	}

	if (wlc_scbmusched_is_ulofdma(musched, scb) == ulofdma) {
		/* no change */
		return ret;
	}

	if (ulofdma) {
		/* Let the rx monitoring trigger the UL MU admission */
	} else {
		ret = wlc_musched_ulofdma_del_usr(ulosched, scb);
		WL_MUTX(("wl%d: %s: Disable ul ofdma STA "MACF" state 0x%x\n", wlc->pub->unit,
			__FUNCTION__, ETHER_TO_MACF(scb->ea), scb->state));
	}
	return ret;
}

uint16
wlc_musched_scb_get_ulofdma_ratemem_idx(wlc_info_t *wlc, scb_t *scb)
{
	scb_musched_t* musched_scb;

	if (scb == NULL || wlc->musched == NULL ||
		((musched_scb = SCB_MUSCHED(wlc->musched, scb)) == NULL)) {
		WL_ERROR(("wl%d: %s: Fail to get musched scb cubby STA "MACF"\n",
			wlc->pub->unit, __FUNCTION__,
			ETHER_TO_MACF(scb->ea)));
		return D11_RATE_LINK_MEM_IDX_INVALID;
	}

	return ((musched_scb->ul_schpos == MUSCHED_ULOFDMA_INVALID_SCHPOS) ?
		D11_RATE_LINK_MEM_IDX_INVALID :	musched_scb->ul_rmemidx);
}

static void
wlc_musched_ulofdma_commit_csreq(wlc_musched_ulofdma_info_t* ulosched)
{
	wlc_info_t *wlc = ulosched->musched->wlc;
	wlc_write_shm(wlc, M_HETB_CSTHRSH_LO(wlc), ulosched->csthr0);
	wlc_write_shm(wlc, M_HETB_CSTHRSH_HI(wlc), ulosched->csthr1);
}
/* Function to commit ulofdma scheduler changes
 * 1. populate ul ofdma txd
 * 2. update the ratemem block for the newly added user
 */
static void
wlc_musched_ulofdma_commit_change(wlc_musched_ulofdma_info_t* ulosched)
{
	int schpos, idx;
	wlc_info_t *wlc = ulosched->musched->wlc;
	d11ulotxd_rev128_t *txd = &ulosched->txd;
	scb_t *scb;
	uint16 lmem_idx;
	uint16 rmem_idx;
	uint16 *ptr;
	uint offset;
	scb_musched_t* musched_scb;
	uint16 max_ulofdma_usrs = wlc_txcfg_max_clients_get(wlc->txcfg, ULOFDMA);

	BCM_REFERENCE(ptr);
	BCM_REFERENCE(offset);
	BCM_REFERENCE(musched_scb);

	if (!RATELINKMEM_ENAB(wlc->pub)) {
		WL_ERROR(("wl%d: %s: Fail to fill up ul ofdma txd. ratelinkmem_enab %x\n",
			wlc->pub->unit, __FUNCTION__, RATELINKMEM_ENAB(wlc->pub)));
		return;
	}

	for (schpos = 0, idx = 0; schpos < max_ulofdma_usrs; schpos++) {
		if ((scb = ulosched->scb_list[schpos]) == NULL) {
			continue;
		}

		lmem_idx = wlc_ratelinkmem_get_scb_link_index(wlc, scb);
		rmem_idx = wlc_musched_scb_get_ulofdma_ratemem_idx(wlc, scb);
		if ((lmem_idx == D11_RATE_LINK_MEM_IDX_INVALID) ||
			(rmem_idx == D11_RATE_LINK_MEM_IDX_INVALID)) {
			WL_ERROR(("wl%d: %s: Fail to fill up ul ofdma txd. schpos %d addr "MACF"\n",
				wlc->pub->unit, __FUNCTION__, schpos,
				ETHER_TO_MACF(scb->ea)));
			ASSERT(0);
			return;
		}
		txd->rlmem[idx] = ((rmem_idx << 8) | lmem_idx);
		musched_scb = SCB_MUSCHED(ulosched->musched, scb);
		txd->ucfg[idx] = musched_scb->ucfg;
		idx++;
	}

	ASSERT(idx == ulosched->num_usrs);

	for (; idx < D11_ULOFDMA_MAX_NUSERS; idx++) {
		txd->rlmem[idx] = 0;
	}

	/* Don't enable UL OFDMA if the number of users is less than minimum */
	if (ulosched->num_usrs < ulosched->min_ulofdma_usrs) {
		txd->nvld = 0;
	} else {
		txd->nvld = ulosched->num_usrs;
	}

	/* set init bit */
	txd->nvld |= ((ulosched->num_usrs) && ulosched->is_start) ? (1 << 15) : 0;

#if defined(WL_PSMX)
	/* suspend psmx */
	wlc_bmac_suspend_macx_and_wait(wlc->hw);

	/* update rate mem for the usr got changed */
	for (schpos = 0; schpos < max_ulofdma_usrs; schpos++) {
		if ((scb = ulosched->scb_list[schpos]) != NULL) {
			musched_scb = SCB_MUSCHED(ulosched->musched, scb);
			if (musched_scb->upd_ul_rmem) {
				wlc_ratelinkmem_write_rucfg(wlc, (uint8*)musched_scb->ul_rmem,
					sizeof(d11ratemem_ulrx_rate_t), musched_scb->ul_rmemidx);
				musched_scb->upd_ul_rmem = FALSE;
			}
		}
	}

	WL_MUTX(("wl%d: %s: interval 0x%x burst %d maxtw %d\n", wlc->pub->unit, __FUNCTION__,
		ulosched->txd.interval, ulosched->txd.burst, ulosched->txd.maxtw));

	/* copy the info to MX_TRIG_TXCFG block */
	for (offset = MX_TRIG_TXCFG(wlc), ptr = (uint16 *) &ulosched->txd;
		offset < MX_TRIG_TXCFG(wlc) + sizeof(d11ulotxd_rev128_t);
		offset += 2, ptr++) {
		wlc_write_shmx(wlc, offset, *ptr);
	}

	/* write trigger txlmt value */
	wlc_write_shmx(wlc, MX_TRIG_TXLMT(wlc), ulosched->txlmt);

	wlc_bmac_enable_macx(wlc->hw);
#endif /* defined(WL_PSMX) */
}

/**
 * admit / evict a ul-ofdma user
 *
 * Called from admission control component / omi transition
 *
 * @param wlc		handle to wlc_info context
 * @param scb		pointer to scb
 * @param admit		true to admit, false to evict
 * @return		true if successful, false otherwise
 */
void
wlc_musched_admit_ulclients(wlc_info_t* wlc, scb_t *scb, bool admit)
{
	wlc_musched_ulofdma_info_t* ulosched = wlc->musched->ulosched;

	if (!HE_ULMU_ENAB(wlc->pub)) {
		WL_MUTX(("wl%d: %s: UL OFDMA feature is not enabled\n",
			wlc->pub->unit, __FUNCTION__));
		return;
	}

	if (wlc_scbmusched_set_ulofdma(ulosched, scb, admit)) {
		/* if a user has been added or deleted, update the scheduler block */
		wlc_musched_ulofdma_commit_change(ulosched);
	} else {
		/* no change on ul ofdma user list, pass */
	}
}

static bool
wlc_scbmusched_is_ulofdma(wlc_muscheduler_info_t *musched, scb_t* scb)
{
	if (musched->ul_policy == MUSCHED_UL_POLICY_DISABLE ||
		SCB_INTERNAL(scb)) {
		return FALSE;
	}

	return (SCB_ULOFDMA(scb));
}

void
wlc_musched_upd_ul_nss(wlc_muscheduler_info_t *musched, scb_t* scb, uint8 tx_nss)
{
	scb_musched_t* musched_scb;

	musched_scb = SCB_MUSCHED(musched, scb);

	musched_scb->upd_ul_rmem = TRUE;
	D11_ULORMEM_RTCTL_SET_INIT(musched_scb->ul_rmem->rtctl, 0);
	D11_ULORMEM_RTCTL_SET_NSS(musched_scb->ul_rmem->rtctl, tx_nss);
	D11_ULOTXD_UCFG_SET_NSS(musched_scb->ucfg, tx_nss);

	wlc_musched_ulofdma_commit_change(musched->ulosched);
	return;
}

void
wlc_musched_max_clients_set(wlc_muscheduler_info_t *musched)
{
	wlc_info_t *wlc;
	uint16 max_mmu_usrs;

	BCM_REFERENCE(wlc);
	if (!musched) {
		return;
	}

	wlc = musched->wlc;
	if (D11REV_LT(wlc->pub->corerev, 128)) {
		return;
	}

	max_mmu_usrs = wlc_txcfg_max_mmu_clients_get(wlc->txcfg);

	/* if max VHT-MU and HE-MU adds upto to max MU clients allowed in the system,
	 * then all FIFOs will be taken by MUMIMO clients. No FIFOs will be avialable for
	 * OFDMA clients. Set max OFDMA clients to zero and disable OFDMA.
	*/
	if (MUSCHED_MAX_MMU_USERS_COUNT(wlc) == max_mmu_usrs) {
		wlc_txcfg_max_clients_set(wlc->txcfg, DLOFDMA, 0);
		MUSCHED_DLOFDMA_DISABLE(musched);
	}
}

void
wlc_umusched_set_fb(wlc_muscheduler_info_t *musched, bool enable)
{
	wlc_musched_ulofdma_info_t *ulosched = musched->ulosched;
	d11ulotxd_rev128_t *txd = &ulosched->txd;
	txd->mctl0 &= ~D11AC_TXC_MBURST;
	txd->mctl0 |= (enable ? D11AC_TXC_MBURST: 0);
}

static int wlc_musched_ulofdma_write_maxn(wlc_musched_ulofdma_info_t* ulosched)
{
#if defined(WL_PSMX)
	wlc_info_t *wlc;
	uint offset;
	int bw;

	wlc = ulosched->musched->wlc;
	if (!wlc->clk) {
		return BCME_NOCLK;
	}

	offset = MX_ULOMAXN_BLK(wlc);
	for (bw = 0; bw < D11_REV128_BW_SZ; bw++) {
		wlc_write_shmx(wlc, offset+(bw*2), ulosched->maxn[bw]);
	}
#endif /* defined(WL_PSMX) */
	return BCME_OK;
}

static void
wlc_musched_ul_oper_state_upd(wlc_muscheduler_info_t *musched, scb_t *scb, uint8 state)
{
	scb_musched_t *musched_scb;
	wlc_musched_ulofdma_info_t *ulosched = musched->ulosched;

	if (scb == NULL || ulosched == NULL ||
		((musched_scb = SCB_MUSCHED(musched, scb)) == NULL)) {
		WL_ERROR(("wl%d: %s: Fail to get ulmu scb cubby STA "MACF"\n",
			musched->wlc->pub->unit, __FUNCTION__,
			ETHER_TO_MACF(scb->ea)));
		return;
	}

	musched_scb->state = state;
	switch (state) {
		case MUSCHED_SCB_INIT:
			musched_scb->idle_cnt = 0;
			musched_scb->last_rx_pkts = 0;
			break;

		case MUSCHED_SCB_ADMT:
		case MUSCHED_SCB_EVCT:
			/* do nothing for now */
			break;

		default:
			break;
	}
}

/* set ul ofdma admission params for TWT setting */
void
wlc_musched_ul_twt_params(wlc_muscheduler_info_t *musched, bool on)
{
	wlc_musched_ulofdma_info_t *ulosched = musched->ulosched;

	WL_MUTX(("wl%d: %s: on is %d\n",
		musched->wlc->pub->unit, __FUNCTION__, on));
	if (on) {
		/* For TWT
		 * - admit even 1 user as ul ofdma
		 * - set the trigger interval to 1ms
		 * - guarantee to tx at least 1 trigger per interval
		 * - tx max 2 triggers per interval
		 */
		ulosched->min_ulofdma_usrs = 1;
	} else {
		/* there is no trig based twt user so restore default settings */
		ulosched->min_ulofdma_usrs = MUSCHED_ULOFDMA_MIN_USERS;
	}
	if (musched->wlc->pub->up) {
		wlc_musched_ulofdma_commit_change(ulosched);
	}
}

#endif /* WL_MUSCHEDULER */
