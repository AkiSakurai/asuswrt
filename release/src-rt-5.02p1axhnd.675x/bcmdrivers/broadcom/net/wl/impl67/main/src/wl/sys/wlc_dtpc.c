/*
 * Dynamic Tx power control module for Broadcom 802.11
 * Networking Adapter Device Drivers
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id$
 *
 *
 * @file wlc_dtpc.c
 * @brief
 * To increase tput and range, this module control additive powers
 * along with the selected rate decided by rate selection algorithm.
 * EVM(distortion) mergin is considerably enough to play the additional
 * power increasing with transmit beamforming. Non-beamformed pkts are
 * out of scope of this module coverage.
 *
 */

#ifdef WLC_DTPC

#include <wlc_cfg.h>   /* feature dependency should be on top */
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <wlioctl.h>
#include <bcmdevs.h>
#include <bcmiov.h>
#include <d11.h>
#include <wlc.h>
#include <wlc_pub.h>
#include <wlc_types.h>
#include <wlc_scb.h>
#include <wl_dbg.h>
#include <wlc_dump.h>
#include <wlc_dtpc.h>
#include <wlc_tx.h>
#include <wlc_scb_ratesel.h>
#include <wlc_txbf.h>
#include <wlc_rate_sel.h>
#include <wlc_ratelinkmem.h>
#include <phy_api.h>
#include <phy_tpc_api.h>
#include <phy_utils_api.h>
#include <wlc_stf.h>   /* for get_pwr_from_targets API */
#include <wlc_tpc.h>   /* for wlc_tpc_get_target_offset API */
#include <wlc_rate_sel.h>
#include <wlc_phy_hal.h>
#include <wlc_phy_int.h>

/** private scope definitions and macros */
#define DTPC_ACTIVE_DEFAULT     (1)    /**< dtpc active */
#define DTPC_THRESH_DEFAULT     (8u)    /**< tx-status threshold */
#define DTPC_PNUMTHRESH_DEFAULT (2u)    /**< searching depth */
#define DTPC_PROBE_STEP_DEFAULT (1u)    /**< qdB steps */
#define DTPC_PSR_THRESH         (4096u) /**< psr threshold to skip probing */
#define DTPC_PKTS_THRESH        (100u)  /**< number of pkts required along with psr threshold */
#define POSITIVE_PROBING	(0u)    /**< positive probing direction */
#define NEGATIVE_PROBING	(1u)    /**< negative probing direction */
#define DTPC_MOVING_AVG_ALPHA   (2u)    /**< alpha used in psr moving average */
#define DTPC_TRIG_PERIOD        (1000u) /**< pwr probing trigger - ms */
#define DTPC_TXS_AGING          (1u)    /**< Aging timer for tx-status - in sec */
#define INVALID_PWR             (0xFF)  /**< invalid power */
#define DTPC_BLKLISTED          (-127)  /**< Blacklisting: probing is skipped for this rate */
#define DTPC_BLKLIST_TIMEOUT    (10u)  /**< 10s black listing timeout */
#define DTPC_PTBL_VALID_DUR     (4u)   /**< pwr table cache valid duration - sec */
#define DTPC_MAX_HEADROOM       (40u)  /**< headroom cap, qdB */

/** internal tools */
#define BOUNDINC(i, k)		(((i) < ((k)-1)) ? (i)+1 : 0)
#define DTPC_NOWTIME(dtpc)    ((dtpc)->wlc->pub->now)
#define UPDATE_MOVING_AVG(p, cur, alpha)  \
	do { *(p) -= ((*p)) >> (alpha); *(p) += (cur) >> (alpha); } while (0)
#define DTPC_PERCNT_CONV(val, tot) (((val) && (tot)) ? (val) * 100u / (tot) : 0)

#define DTPC_RATESEL_EMA_NF        (12u)
#define DTPC_EMA_CONV(val, tot)    (((val) && (tot)) ? ((val) << DTPC_RATESEL_EMA_NF) / (tot) : 0)

#define VALIDATE_PWR(val, max)     (((val) && ((val) != DTPC_BLKLISTED) && (val) > (max)) ?  \
	(max) : (((val) == DTPC_BLKLISTED) ? INVALID_PWR : (val)))

/** iovar enum definition. */
enum {
	IOV_DTPC = 0x0,
	IOV_DTPC_HEADROOM = 0x1,
	IOV_DTPC_HEADROOM_ALL = 0x2,
	IOV_DTPC_LAST
};

typedef struct dtpc_txpwr_ctxt {
	wl_tx_bw_t ppr_bw;
	ppr_t *board_min;
	ppr_t *clm_max;
} dtpc_txpwr_ctxt_t;

typedef struct dtpc_prb_pwr_tbl {
	uint32 tstamp;       /**< last probed time stamp */
	int8  qdb;           /**< last optimal power offset in qdB */
} dtpc_prb_pwr_tbl_t;

/** per-slice contexts */
struct wlc_dtpc_info {
    wlc_info_t  *wlc;                         /**< backward ref pointer to wlc instance */
    int  scbh;                                /**< scb cubby ref pointer */
    bcm_iov_parse_context_t  *iov_parse_ctx;  /**< bcm iovar module handler */
    uint16  thresh;                           /**< power probing window */
    uint8  refine_step;                       /**< step used for refining cached offset */
    uint8  pnum_thresh;                       /**< threshold to limit the number of power probes */
    uint8  alpha;                             /**< EMA alpha used in psr collection */
    uint8  active;                            /**< runtime active/deactive control flag */
    uint8  active_override;                   /**< 0: use default, 1: set active, 2: unset active */
    uint32  trig_intval;                      /**< pwr probing trigger interval - ms */
    dtpc_txpwr_ctxt_t  *pwrctxt;              /**< PPR copy for board limit and CLM limit */
    uint8  extra_backoff_factor;              /**< an extra backoff added for safety */
    int8  clmlimit_ovr;                       /**< CLM limit overriding - test only */
};

#define DTPC_SAMPLE_WINDOW    (20u)
#define DTPC_RATE_ARRAYSIZE   (12u)
#define DTPC_BOOST_SAMPLE     (9u)

/** dbg indicator per rate */
typedef struct dtpc_dbgstats_pack {
	uint32 mrtsucc;
	uint32 fbsucc;
	uint32 cur_psr;
	uint32 curmv_psr;
	uint32 fbr_psr;
	uint32 fbrmv_psr;
	uint32 boosted_qdb;
	uint32 tstamp;
	bool carriedover;
} dtpc_dbgstats_pack_t;

typedef struct dtpc_sample_pack {
	uint8 offset[DTPC_SAMPLE_WINDOW];
} dtpc_sample_pack_t;

typedef struct dtpc_stats {
	uint32  prob_strt[DTPC_RATE_ARRAYSIZE];
	uint32  prob_totpkt[DTPC_RATE_ARRAYSIZE];
	uint32  ratesel_totpkt[DTPC_RATE_ARRAYSIZE];
	uint32  ratesel_tstmp[DTPC_RATE_ARRAYSIZE];
	uint32  boost_hist[DTPC_BOOST_SAMPLE][DTPC_RATE_ARRAYSIZE];
	uint32  ovrlimitcnt;
	uint8  samp_ref_idx[DTPC_RATE_ARRAYSIZE];
	uint8  samp_tot_cnt[DTPC_RATE_ARRAYSIZE];
	uint8  boost_idx;
	dtpc_sample_pack_t samp[DTPC_RATE_ARRAYSIZE];
	dtpc_dbgstats_pack_t bd[DTPC_RATE_ARRAYSIZE];
} dtpc_stats_t;

/** per-scb storage */
typedef struct dtpc_scb_cubby {
	wlc_dtpc_info_t  *dtpc;       /**< back pointer to dtpc info */
	ratespec_t  dtpc_rspec;       /**< cached rate spec for primary reference */
	uint32  psr_pro;              /**< PSR moving average */
	uint32  psr_init;             /**< PSR initial value */
	uint16  state;                /**< DTPC state machine status */
	uint8  prb_txpwroff;          /**< txpwr offset to probe */
	uint8  best_txpwroff;         /**< the best txpwr offset as outcome of probing */
	uint8  start_txpwroff;        /**< start txpwroff point used in txpwroff search method */
	uint8  end_txpwroff;          /**< end txpwroff point used in txpwroff search method */
	uint8  num_pro;               /**< number of probes used in power probing cycle */
	uint8  cur_backoff;           /**< current backoff amount from TxD */
	uint8  epoch;                 /**< valid epoch for probing */
	uint8  num_txs;               /**< pwr probing txs count */
	uint8  non_bf_txs;            /**< non-beamformed txs count */
	uint8  probe_dir;             /**< probing direction used in refining cached value */
	int16  pwroff;                /**< latest total pwroffset applied */
	bool  sp_started;             /**< spatial probing marker */
	bool  rate_up;                /**< rate up event indication flag */
	uint32  aging_time;           /**< tx status aging time out */
	uint32  last_prb_time;        /**< last pwr probing time - ms */
	phy_tx_targets_per_core_t minpwr;   /**< phy nominal power per core */
	phy_tx_targets_per_core_t headroom; /**< headroom per core */
	dtpc_prb_pwr_tbl_t ptbl[DTPC_RATE_ARRAYSIZE][PHY_CORE_MAX];
	dtpc_stats_t *logs;           /**< dtpc stat storage */
} dtpc_scb_cubby_t;
#define DTPC_SCB_CUBBY_LOC(dtpc, scb)	((dtpc_scb_cubby_t **)SCB_CUBBY((scb), (dtpc)->scbh))
#define DTPC_SCB_CUBBY(dtpc, scb)	(*(DTPC_SCB_CUBBY_LOC((dtpc), (scb))))

typedef struct wl_dtpc_cfg_v1 wl_dtpc_cfg_t;

/** function prototypes - private API and callbacks */
static void dtpc_collect_stats(wlc_dtpc_info_t *dtpci, scb_t *scb, ratespec_t rspec,
	uint32 mrt_succ);
static bool dtpc_select_pwr(wlc_dtpc_info_t *dtpci, scb_t *scb, ratespec_t rspec,
	uint8 epoch, uint32 psr, uint32 pktcnt, bool highest_mcs);
static bool dtpc_txbf_en(wlc_dtpc_info_t *dtpci, scb_t *scb, ratespec_t rspec);
static bool dtpc_is_bfm_pkt(wlc_dtpc_info_t *dtpci, tx_status_t *txs);
static bool dtpc_start_pwr_prb(wlc_dtpc_info_t *dtpci, scb_t *scb, uint32 pktcnt,
	ratespec_t rspec);
static void dtpc_upd_stat(wlc_dtpc_info_t *dtpci, scb_t *scb, tx_status_t *txs,
		uint32 prate_prim, uint32 psr_prim, uint32 prate_dn, uint32 psr_dn, uint32 psr_up,
		uint32 cur, uint8 epoch, uint32 pktcnt, bool highest_mcs);
static void dtpc_monitor(wlc_dtpc_info_t *dtpci, scb_t *scb, ratespec_t rspec,
		bool goup_rateid, uint8 epoch, bool highest_mcs, bool nss_chg);
static int16 dtpc_recalc_txpwroff(wlc_dtpc_info_t *dtpci, scb_t *scb,
	uint8 backoff, ratespec_t rspec);
static int dtpc_find_txpwroff(wlc_dtpc_info_t *dtpci, scb_t *scb);
static void dtpc_adj_bsrch_params(wlc_dtpc_info_t *dtpci, scb_t *scb, bool highest_mcs);
static bool dtpc_adj_lsrch_params(wlc_dtpc_info_t *dtpci, scb_t *scb);
static void dtpc_get_prb_txpwroff(wlc_dtpc_info_t *dtpci, scb_t *scb,
	ratespec_t rspec, uint8 epoch, uint8 max_txpwroff);
static void dtpc_collect_prb_results(wlc_dtpc_info_t *dtpci, scb_t *scb,
	tx_status_t *txs, dtpc_prb_pwr_tbl_t *txpwroff_cache, uint8 epoch,
	uint32 cur_psr, uint32 psr_prim, uint32 prate_prim, uint32 psr_dn,
	uint32 prate_dn, uint32 pktcnt, bool highest_mcs);
static void dtpc_cleanup_stat(wlc_dtpc_info_t *dtpci, scb_t *scb);
static void dtpc_watchdog_cb(void *ctx);
static uint dtpc_scb_secsz(void *ctx, struct scb *scb);
static int dtpc_scb_init(void *ctx, struct scb *scb);
static void dtpc_scb_deinit(void *ctx, struct scb *scb);
static void dtpc_scb_init_state(dtpc_scb_cubby_t *cubby);
static int dtpc_doiovar(void *hdl, uint32 actionid, void *p, uint plen,
	void *a, uint alen, uint vsize, struct wlc_if *wlcif);
static void dtpc_chansw_notif_cb(void *arg, wlc_chansw_notif_data_t *data);
static void dtpc_bsscfg_updown_cb(void *ctx, bsscfg_up_down_event_data_t *evt_data);
static void dtpc_power_context_cleanup(wlc_dtpc_info_t *dtpci);

/* power context control APIs */
static dtpc_txpwr_ctxt_t *dtpc_power_context_create(wlc_info_t *wlc);
static void dtpc_power_context_delete(wlc_dtpc_info_t *dtpci);

static uint8 dtpc_get_headroom(wlc_dtpc_info_t *dtpci, scb_t *scb, ratespec_t rspec);
static int32 dtpc_power_context_get_limits(wlc_dtpc_info_t *dtpci, scb_t *scb,
	ratespec_t rspec);
static int32 dtpc_calc_nominal_pwr(wlc_dtpc_info_t *dtpci, scb_t *scb,
	ratespec_t rspec);
static bool dtpc_power_context_set_channel(wlc_dtpc_info_t *dtpci, chanspec_t chanspec);
static int dtpc_upd_pwroffset(wlc_info_t *wlc, struct scb *scb, ratespec_t rspec);

#if defined(BCMDBG) || defined(WLTEST)
static int dtpc_dump_clr(void *ctx);
static void dtpc_clear_logs(wlc_dtpc_info_t *dtpci, struct scb *scb);
static void dtpc_scb_dump(void *ctx, struct scb *scb, struct bcmstrbuf *b);
static void wlc_dtpc_collect_dbg(wlc_dtpc_info_t *dtpci, scb_t *scb, ratespec_t rspec,
	uint32 mrt, uint32 mrt_succ, uint32 fbr, uint32 fbr_succ,
	uint32 prim_psr, uint32 fbr_psr);
static int wlc_dtpc_dump(wlc_dtpc_info_t *dtpci, struct bcmstrbuf *b);
static void _dtpc_scb_dump_by_cubby(wlc_dtpc_info_t *dtpci, struct scb *scb,
	struct bcmstrbuf *b);
#endif // endif

/** bcm iovar module APIs */
static void *dtpc_iov_alloc_ctxt(void *ctx, uint size);
static void dtpc_iov_free_ctxt(void *ctx, void *iov_ctx, uint size);
static int BCMATTACHFN(dtpc_iov_get_digest_cb)(void *ctx, bcm_iov_cmd_digest_t **dig);
static int wlc_dtpc_wlc_init(void *ctx);
static int dtpc_iov_validate_cmds(const bcm_iov_cmd_digest_t *dig, uint32 actionid,
	const uint8 *ibuf, size_t ilen, uint8 *obuf, size_t *olen);

/** sub-cmds handler */
static int dtpc_iov_get_en(const bcm_iov_cmd_digest_t *dig,
	const uint8 *ibuf, size_t ilen, uint8 *obuf, size_t *olen);
static int dtpc_iov_set_en(const bcm_iov_cmd_digest_t *dig,
	const uint8 *ibuf, size_t ilen, uint8 *obuf, size_t *olen);
static int dtpc_iov_get_txsthres(const bcm_iov_cmd_digest_t *dig,
	const uint8 *ibuf, size_t ilen, uint8 *obuf, size_t *olen);
static int dtpc_iov_set_txsthres(const bcm_iov_cmd_digest_t *dig,
	const uint8 *ibuf, size_t ilen, uint8 *obuf, size_t *olen);
static int dtpc_iov_get_probstep(const bcm_iov_cmd_digest_t *dig,
	const uint8 *ibuf, size_t ilen, uint8 *obuf, size_t *olen);
static int dtpc_iov_set_probstep(const bcm_iov_cmd_digest_t *dig,
	const uint8 *ibuf, size_t ilen, uint8 *obuf, size_t *olen);

/**  Experimental IOVARs */
static int dtpc_iov_get_alpha(const bcm_iov_cmd_digest_t *dig,
	const uint8 *ibuf, size_t ilen, uint8 *obuf, size_t *olen);
static int dtpc_iov_set_alpha(const bcm_iov_cmd_digest_t *dig,
	const uint8 *ibuf, size_t ilen, uint8 *obuf, size_t *olen);
static int dtpc_iov_get_trigint(const bcm_iov_cmd_digest_t *dig,
	const uint8 *ibuf, size_t ilen, uint8 *obuf, size_t *olen);
static int dtpc_iov_set_trigint(const bcm_iov_cmd_digest_t *dig,
	const uint8 *ibuf, size_t ilen, uint8 *obuf, size_t *olen);
static int dtpc_iov_get_headroom(wlc_dtpc_info_t *dtpci,
	int chanspec_bw, uint num_txchains, int32 rspec, uint8 *output);
static int dtpc_iov_get_headroom_all(wlc_dtpc_info_t *dtpci,
	int chanspec_bw, uint num_txchains, void *output);
static int dtpc_iov_get_clmovr(const bcm_iov_cmd_digest_t *dig,
	const uint8 *ibuf, size_t ilen, uint8 *obuf, size_t *olen);
static int dtpc_iov_set_clmovr(const bcm_iov_cmd_digest_t *dig,
	const uint8 *ibuf, size_t ilen, uint8 *obuf, size_t *olen);

static const bcm_iovar_t dtpc_iovars[] = {
	{"dtpc", IOV_DTPC, (IOVF_SET_DOWN), 0, IOVT_BUFFER, 0},
	{"dtpc_headroom", IOV_DTPC_HEADROOM, (IOVF_GET_UP), 0, IOVT_UINT32, 0},
	{"dtpc_headroom_all", IOV_DTPC_HEADROOM_ALL, (IOVF_GET_UP), 0, IOVT_BUFFER, 0},
	{NULL, 0, 0, 0, 0, 0}
};

static const bcm_iov_cmd_info_t dtpc_sub_cmds[] = {
	{ WL_DTPC_CMD_EN,
	BCM_IOV_CMD_FLAG_XTLV_DATA,
	0,
	BCM_XTLV_OPTION_ALIGN32,
	dtpc_iov_validate_cmds,
	dtpc_iov_get_en,
	dtpc_iov_set_en,
	0, 0,
	WLC_IOCTL_SMLEN, 0, WLC_IOCTL_SMLEN
	},

	{ WL_DTPC_CMD_TXS_THRES,
	BCM_IOV_CMD_FLAG_XTLV_DATA,
	0,
	BCM_XTLV_OPTION_ALIGN32,
	dtpc_iov_validate_cmds,
	dtpc_iov_get_txsthres,
	dtpc_iov_set_txsthres,
	0, 0,
	WLC_IOCTL_SMLEN, 0, WLC_IOCTL_SMLEN
	},

	{ WL_DTPC_CMD_PROBSTEP,
	BCM_IOV_CMD_FLAG_XTLV_DATA,
	0,
	BCM_XTLV_OPTION_ALIGN32,
	dtpc_iov_validate_cmds,
	dtpc_iov_get_probstep,
	dtpc_iov_set_probstep,
	0, 0,
	WLC_IOCTL_SMLEN, 0, WLC_IOCTL_SMLEN
	},

	{ WL_DTPC_CMD_ALPHA,
	BCM_IOV_CMD_FLAG_XTLV_DATA,
	0,
	BCM_XTLV_OPTION_ALIGN32,
	dtpc_iov_validate_cmds,
	dtpc_iov_get_alpha,
	dtpc_iov_set_alpha,
	0, 0,
	WLC_IOCTL_SMLEN, 0, WLC_IOCTL_SMLEN
	},

	{ WL_DTPC_CMD_TRIGINT,
	BCM_IOV_CMD_FLAG_XTLV_DATA,
	0,
	BCM_XTLV_OPTION_ALIGN32,
	dtpc_iov_validate_cmds,
	dtpc_iov_get_trigint,
	dtpc_iov_set_trigint,
	0, 0,
	WLC_IOCTL_SMLEN, 0, WLC_IOCTL_SMLEN
	},

	{ WL_DTPC_CMD_CLMOVR,
	BCM_IOV_CMD_FLAG_XTLV_DATA,
	0,
	BCM_XTLV_OPTION_ALIGN32,
	dtpc_iov_validate_cmds,
	dtpc_iov_get_clmovr,
	dtpc_iov_set_clmovr,
	0, 0,
	WLC_IOCTL_SMLEN, 0, WLC_IOCTL_SMLEN
	}
};

/** public APIs */

uint8
wlc_dtpc_active(wlc_dtpc_info_t *dtpci)
{
	return dtpci->active;
}

uint8
wlc_dtpc_check_txs(wlc_dtpc_info_t *dtpci, scb_t *scb, tx_status_t *txs, ratespec_t rspec,
		uint32 prate_prim, uint32 psr_prim, uint32 prate_dn, uint32 psr_dn,
		uint32 psr_up, uint8 epoch, uint32 nupd, uint32 mrt, uint32 mrt_succ,
		bool go_prb, bool highest_mcs)
{
	dtpc_scb_cubby_t *cubby = DTPC_SCB_CUBBY(dtpci, scb);
	uint8 ret = DTPC_INACTIVE;
	if (cubby->state == DTPC_PROBE) {
		uint32 cur_succ = 0;
		cur_succ = (mrt) ? (mrt_succ << DTPC_RATESEL_EMA_NF) / mrt : 0;
		dtpc_upd_stat(dtpci, scb, txs, prate_prim, psr_prim, prate_dn,
				psr_dn, psr_up, cur_succ, epoch, nupd, highest_mcs);
		ret = ret | DTPC_INPROG;
	}
	if (cubby->state == DTPC_READY) {
		if (go_prb) {
			if (dtpc_start_pwr_prb(dtpci, scb, nupd, rspec)) {
				ret = ret | DTPC_INPROG;
			}
		}
	} else if (cubby->state == DTPC_START) {
		if (dtpc_txbf_en(dtpci, scb, rspec)) {
			if (dtpc_select_pwr(dtpci, scb, rspec, epoch,
				psr_prim, nupd, highest_mcs)) {
				ret = ret | DTPC_INPROG;
				cubby->epoch = wlc_scb_ratesel_change_epoch(dtpci->wlc->wrsi, scb);
				dtpc_upd_pwroffset(dtpci->wlc, scb, rspec);
			}
		} else {
			cubby->state = DTPC_READY;
		}
	}

	if (cubby->state == DTPC_STOP) {
		ret = DTPC_UPD_PSR | DTPC_TXC_INV | DTPC_INPROG;
		cubby->state = DTPC_READY;
	}

	if (ret != 0) {
		dtpc_collect_stats(dtpci, scb, rspec, mrt_succ);
	}
	return ret;
}

void
wlc_dtpc_get_ready(wlc_dtpc_info_t *dtpci, scb_t *scb)
{

	dtpc_scb_cubby_t *cubby = DTPC_SCB_CUBBY(dtpci, scb);
	if (cubby->state == DTPC_STOP) {
		cubby->state = DTPC_READY;
	}

	return;
}

void
wlc_dtpc_upd_sp(wlc_dtpc_info_t *dtpci, scb_t *scb, bool sp_started)
{
	dtpc_scb_cubby_t *cubby = DTPC_SCB_CUBBY(dtpci, scb);
	if (cubby) {
		cubby->sp_started = sp_started;

	}
	return;
}

int32
wlc_dtpc_get_psr(wlc_dtpc_info_t *dtpci, scb_t *scb)
{
	dtpc_scb_cubby_t *cubby = DTPC_SCB_CUBBY(dtpci, scb);
	if (cubby) {
		return cubby->psr_init;
	} else {
		return PSR_UNINIT;
	}
}

bool
wlc_dtpc_is_pwroffset_changed(wlc_dtpc_info_t *dtpci, scb_t *scb, ratespec_t rspec)
{
	uint8 offset = DTPC_BACKOFF_INVALID;

	if (wlc_tpc_get_target_offset(dtpci->wlc, rspec, &offset) == BCME_OK) {
		return (dtpc_recalc_txpwroff(dtpci, scb, offset, rspec) != DTPC_PWROFFSET_NOCHANGE);
	}

	return FALSE;
}

int16
wlc_dtpc_upd_pwroffset(wlc_dtpc_info_t *dtpci, scb_t *scb, ratespec_t rspec,
		d11ratemem_rev128_rate_t *ratemem)
{
	int16 pwrupd = DTPC_PWROFFSET_NOCHANGE;
	int16 txpwr_offset;
	dtpc_scb_cubby_t *cubby = DTPC_SCB_CUBBY(dtpci, scb);
	uint8 offset = DTPC_BACKOFF_INVALID;

	/* apply txpwroff */
	if (!cubby || !cubby->dtpc_rspec) {
		return pwrupd;
	}

	if (wlc_tpc_get_target_offset(dtpci->wlc, rspec, &offset) == BCME_OK) {
		cubby->cur_backoff = offset;
		if ((cubby->state == DTPC_PROBE) &&
			((DTPC_NOWTIME(dtpci) - cubby->aging_time)
			>= DTPC_TXS_AGING)) {
			cubby->state = DTPC_STOP;
			cubby->last_prb_time = OSL_SYSUPTIME();
		}
		txpwr_offset = dtpc_recalc_txpwroff(dtpci, scb, offset, rspec);
		if (cubby->state == DTPC_PROBE) {
			ratemem->RateCtl |= htol16(D11AX_RTCTL_DTPCPROBE);
			ratemem->BFM0 = htol16(txpwr_offset);
		} else {
			ratemem->RateCtl &= htol16(~D11AX_RTCTL_DTPCPROBE);
			ratemem->TxPhyCtl[3] = ratemem->TxPhyCtl[4] =
				htol16((txpwr_offset << PHYCTL_TXPWR_CORE1_SHIFT) |
				txpwr_offset);
		}
		/* save the updated power offset */
		pwrupd = cubby->pwroff = txpwr_offset;
	} else {
		cubby->cur_backoff = DTPC_BACKOFF_INVALID;
		ASSERT(cubby->cur_backoff != DTPC_BACKOFF_INVALID);
	}

	return pwrupd;
}

void
wlc_dtpc_rate_change(wlc_dtpc_info_t *dtpci, scb_t *scb, ratespec_t rspec,
		bool goup_rateid, uint8 epoch, bool highest_mcs, bool nss_chg)
{
	dtpc_scb_cubby_t *cubby = DTPC_SCB_CUBBY(dtpci, scb);
	cubby->state = DTPC_READY;
	dtpc_monitor(dtpci, scb, rspec, goup_rateid, epoch, highest_mcs, nss_chg);
	return;
}

wlc_dtpc_info_t *
BCMATTACHFN(wlc_dtpc_attach)(wlc_info_t *wlc)
{
	wlc_dtpc_info_t *dtpci;
	bcm_iov_parse_context_t *parse_ctx = NULL;
	bcm_iov_parse_config_t parse_cfg;
	int ret = BCME_OK;
	scb_cubby_params_t scb_cubby_params;

	ASSERT(wlc != NULL);

	/* storage allocation */
	if ((dtpci = (wlc_dtpc_info_t *)MALLOCZ(wlc->osh, sizeof(*dtpci))) == NULL) {
		WL_ERROR(("wl%d: out of mem, malloced %d bytes\n", WLCWLUNIT(wlc),
			MALLOCED(wlc->osh)));
		return NULL;
	}

	/* module functional support validation for HW dependency */
	if (D11REV_GE(wlc->pub->corerev, 128)) {
		wlc->pub->_dtpc = TRUE;
	} else {
		wlc->pub->_dtpc = FALSE;
		dtpci->wlc = wlc;
		return dtpci;
	}

	/* default value assignments */
	dtpci->thresh = DTPC_THRESH_DEFAULT;
	dtpci->pnum_thresh = DTPC_PNUMTHRESH_DEFAULT;
	dtpci->refine_step = DTPC_PROBE_STEP_DEFAULT;
	dtpci->alpha = DTPC_MOVING_AVG_ALPHA;
	dtpci->trig_intval = DTPC_TRIG_PERIOD;
	dtpci->active_override = 0;
	dtpci->clmlimit_ovr = -1;  // disable by default

	/* module registration */
	dtpci->wlc = wlc;
	if (wlc_module_register(wlc->pub, dtpc_iovars, "dtpc", dtpci, dtpc_doiovar,
		dtpc_watchdog_cb, wlc_dtpc_wlc_init, NULL) != BCME_OK) {
		goto fail;
	}

	/* bcm iovar parser configuration */
	parse_cfg.alloc_fn = (bcm_iov_malloc_t)dtpc_iov_alloc_ctxt;
	parse_cfg.free_fn = (bcm_iov_free_t)dtpc_iov_free_ctxt;
	parse_cfg.dig_fn = (bcm_iov_get_digest_t)dtpc_iov_get_digest_cb;
	parse_cfg.max_regs = 1u;
	parse_cfg.options = 0u;
	parse_cfg.alloc_ctx = (void *)dtpci;
	if (bcm_iov_create_parse_context((const bcm_iov_parse_config_t *)&parse_cfg,
		&parse_ctx) != BCME_OK) {
		WL_ERROR(("wl%d: dynamic tx power ctrl iovar parser creation failed\n",
			WLCWLUNIT(wlc)));
		goto fail;
	}
	dtpci->iov_parse_ctx = parse_ctx;

	/* register bsscfg up/down callbacks */
	if ((ret = wlc_bsscfg_updown_register(wlc, dtpc_bsscfg_updown_cb, dtpci)) != BCME_OK) {
		WL_ERROR(("wl%d: wlc_dtpc_bsscfg_updown_cb reg fails, err=%d\n",
			WLCWLUNIT(wlc), ret));
		goto fail;
	}

	/* bcm iovar sub-commands registration */
	if ((ret = bcm_iov_register_commands(dtpci->iov_parse_ctx, (void *)dtpci, &dtpc_sub_cmds[0],
		(size_t)ARRAYSIZE(dtpc_sub_cmds), NULL, 0)) != BCME_OK) {
		WL_ERROR(("wl%d: bcm_iov_register_commands failes, err=%d\n",
			WLCWLUNIT(wlc), ret));
		goto fail;
	}

	/* reserve cubby in the scb container for per-scb private data */
	bzero(&scb_cubby_params, sizeof(scb_cubby_params));

	scb_cubby_params.context = dtpci;
	scb_cubby_params.fn_init = dtpc_scb_init;
	scb_cubby_params.fn_deinit = dtpc_scb_deinit;
	scb_cubby_params.fn_secsz = dtpc_scb_secsz;
#if defined(BCMDBG) || defined(WLTEST)
	scb_cubby_params.fn_dump = dtpc_scb_dump;
#endif // endif
	dtpci->scbh = wlc_scb_cubby_reserve_ext(wlc, sizeof(dtpc_scb_cubby_t *),
		&scb_cubby_params);
	if (dtpci->scbh < 0) {
		WL_ERROR(("wl%d: dynamic txpower ctrl scb_cubby_reserve_ext() failed\n",
			WLCWLUNIT(wlc)));
		goto fail;
	}

	/* subscribe channel switch event */
	if (wlc_chansw_notif_register(wlc, dtpc_chansw_notif_cb, dtpci) != BCME_OK) {
		WL_ERROR(("wl%d: dtpc_chansw_notif_cb registration failed\n",
			WLCWLUNIT(wlc)));
		goto fail;
	}

	/* dtpc power context creation */
	dtpci->pwrctxt = dtpc_power_context_create(wlc);
	if (!dtpci->pwrctxt) {
		WL_ERROR(("wl%d: dtpc_power_context_create() failed\n", WLCWLUNIT(wlc)));
		goto fail;
	}

#if defined(BCMDBG) || defined(WLTEST)
	wlc_dump_add_fns(wlc->pub, "dtpc", (dump_fn_t)wlc_dtpc_dump,
		(clr_fn_t)dtpc_dump_clr, (void *)dtpci);
#endif // endif

	return dtpci;
fail:
	MODULE_DETACH(dtpci, wlc_dtpc_detach);
	return NULL;
}

void
BCMATTACHFN(wlc_dtpc_detach)(wlc_dtpc_info_t *dtpci)
{
	wlc_info_t *wlc;
	if (dtpci == NULL) {
		return;
	}
	wlc = dtpci->wlc;
	if (D11REV_LT(wlc->pub->corerev, 128)) {
		goto dtpc_free;
	}
	/* BCM IOVAR de-allocation */
	if (dtpci->iov_parse_ctx) {
		bcm_iov_free_parse_context(&dtpci->iov_parse_ctx,
			(bcm_iov_free_t)dtpc_iov_free_ctxt);
	}
	/* un-subscribe bsscfg event callback */
	wlc_bsscfg_updown_unregister(wlc, dtpc_bsscfg_updown_cb, dtpci);

	/* pwr context de-allocation */
	if (dtpci->pwrctxt) {
		dtpc_power_context_delete(dtpci);
	}

	/* un-subscribe channel switch event callback */
	wlc_chansw_notif_unregister(wlc, dtpc_chansw_notif_cb, dtpci);

	wlc_module_unregister(wlc->pub, "dtpc", dtpci);
dtpc_free:
	MFREE(wlc->osh, dtpci, sizeof(*dtpci));
	return;
}

static int
wlc_dtpc_wlc_init(void *ctx)
{
	wlc_dtpc_info_t *dtpci = ctx;
	wlc_info_t *wlc;
	wlc_bsscfg_t *cfg;
	int idx;

	if (dtpci == NULL) {
		return BCME_OK;
	}

	wlc = dtpci->wlc;
	if (D11REV_LT(wlc->pub->corerev, 128)) {
		return BCME_OK;
	}

	if (dtpci->active_override == 2) {
		dtpci->active = FALSE;
	} else if (DTPC_ENAB(wlc->pub)) {
		FOREACH_AP(wlc, idx, cfg) {
			dtpci->active = (dtpci->active_override == 1) ?
				TRUE : DTPC_ACTIVE_DEFAULT;
			break;
		}
	}

	return BCME_OK;
}

void
wlc_dtpc_collect_psr(wlc_dtpc_info_t *dtpci, scb_t *scb, ratespec_t rspec,
		uint32 mrt, uint32 mrt_succ, uint32 fbr, uint32 fbr_succ,
		uint32 prim_psr, uint32 fbr_psr)
{
	if (mrt_succ > 0) {
		dtpc_collect_stats(dtpci, scb, rspec, mrt_succ);
	}
#if defined(BCMDBG) || defined(WLTEST)
	if (mrt > 0 || fbr > 0) {
		wlc_dtpc_collect_dbg(dtpci, scb, rspec, mrt, mrt_succ,
			fbr, fbr_succ, prim_psr, fbr_psr);
	}
#endif // endif
	return;
}

#if defined(BCMDBG) || defined(WLTEST)
int
wlc_dtpc_dump(wlc_dtpc_info_t *dtpci, struct bcmstrbuf *b)
{
	dtpc_scb_dump(dtpci, NULL, b);
	return BCME_OK;
}
#endif // endif

/* -------------------------------------------- */
/* Private function definitions                 */
/* -------------------------------------------- */

static void
dtpc_collect_prb_results(wlc_dtpc_info_t *dtpci, scb_t *scb, tx_status_t *txs,
	dtpc_prb_pwr_tbl_t *txpwroff_cache, uint8 epoch, uint32 cur_psr,
	uint32 psr_prim, uint32 prate_prim, uint32 psr_dn,
	uint32 prate_dn, uint32 pktcnt, bool highest_mcs)
{
	uint32 tput_dn, tput_pro;
	dtpc_scb_cubby_t *cubby = DTPC_SCB_CUBBY(dtpci, scb);
	if (cubby->state == DTPC_STOP) {
		return;
	}
	if (dtpc_is_bfm_pkt(dtpci, txs)) {
		if (epoch == cubby->epoch) {
			cubby->non_bf_txs = 0;
			cubby->aging_time = DTPC_NOWTIME(dtpci);
			/* 1st prob stats case */
			if ((cubby->psr_pro == 0) && (cubby->num_pro == 0)) {
				cubby->psr_init = psr_prim;
				cubby->psr_pro = cubby->psr_init;
			}
			/* degradation quick detection */
			if (cur_psr == 0) {
				if  (txpwroff_cache->qdb == 0) {
					txpwroff_cache->qdb = DTPC_BLKLISTED;
					txpwroff_cache->tstamp = DTPC_NOWTIME(dtpci);
				} else {
					cubby->probe_dir = (cubby->probe_dir == POSITIVE_PROBING) ?
						NEGATIVE_PROBING : POSITIVE_PROBING;
				}
				cubby->state = DTPC_STOP;
				return;
			} else if (cur_psr < (psr_prim >> 1u)) {
				cubby->num_txs = dtpci->thresh;
			}
			cubby->num_txs++;
			UPDATE_MOVING_AVG(&(cubby->psr_pro), cur_psr, DTPC_MOVING_AVG_ALPHA);
			if (cubby->num_txs >= dtpci->thresh) {
				cubby->num_txs = 0;
				if (txpwroff_cache->qdb == 0) {
					dtpc_adj_bsrch_params(dtpci, scb, highest_mcs);
					if (cubby->rate_up) {
						/* Make sure that tput is better than lower rate */
						tput_dn = psr_dn * prate_dn;
						tput_pro = cubby->psr_pro * prate_prim;
						if (tput_pro >= tput_dn) {
							cubby->state = DTPC_STOP;
							/* update cache with the new boosted pwr */
							txpwroff_cache->qdb =
								cubby->best_txpwroff;
							txpwroff_cache->tstamp =
								DTPC_NOWTIME(dtpci);
							return;
						}
					}
				} else {
					if (dtpc_adj_lsrch_params(dtpci, scb)) {
						return;
					}
				}
				RL_DTPC_CMP(("[DTPC]: pwr boosting: bestpsr=%d, best_txpwroff=%d\n",
					cubby->psr_init, cubby->best_txpwroff));
				/* select next power offset to probe */
				dtpc_select_pwr(dtpci, scb, cubby->dtpc_rspec, cubby->epoch,
					cubby->psr_init, pktcnt, highest_mcs);
				/* update ratemem cache */
				if (dtpc_select_pwr(dtpci, scb, cubby->dtpc_rspec, cubby->epoch,
					cubby->psr_init, pktcnt, highest_mcs)) {
					/* update ratemem cache */
					cubby->epoch =
						wlc_scb_ratesel_change_epoch(dtpci->wlc->wrsi, scb);

					dtpc_upd_pwroffset(dtpci->wlc, scb, cubby->dtpc_rspec);
				}

			}
		}
	} else if (cubby->non_bf_txs < dtpci->thresh) {
		cubby->non_bf_txs++;
	} else {
		cubby->state = DTPC_STOP;
	}

	return;
}

static uint8
dtpc_get_headroom(wlc_dtpc_info_t *dtpci, scb_t *scb, ratespec_t rspec)
{
	uint8 pwroff = 0;
	int idx = 0;
	int32 err = BCME_OK;
	dtpc_scb_cubby_t *cubby = DTPC_SCB_CUBBY(dtpci, scb);
	if (cubby) {
#if defined(BCMDBG) || defined(WLTEST)
		if (dtpci->clmlimit_ovr >= 0) {
			/* override max headroom - test only */
			pwroff = dtpci->clmlimit_ovr + dtpci->extra_backoff_factor;
		} else
#endif // endif
		{
		err = dtpc_power_context_get_limits(dtpci, scb, rspec);
		if (err == BCME_OK) {
			/* pick the valid headroom from active phychain.
			 * currently we don't have per-core distiction to calculate
			 * power offset. may introduce later.
			 */
			for (idx = 0; idx < WLC_BITSCNT(dtpci->wlc->stf->hw_txchain) &&
					idx < PHY_CORE_MAX; idx++) {
				if (cubby->headroom.pwr[idx] > 0) {
					pwroff = cubby->headroom.pwr[idx];
					break;
				}
			}
		} else {
			WL_ERROR(("wl:%d DTPC disable, power context get failed\n",
				WLCWLUNIT(dtpci->wlc)));
			dtpci->active = 0;
		}
	}
	}
	return pwroff;
}

static void
dtpc_get_prb_txpwroff(wlc_dtpc_info_t *dtpci, scb_t *scb, ratespec_t rspec,
	uint8 epoch, uint8 max_txpwroff)
{
	int dtpc_mcs, dtpc_nss, mcs, nss;
	dtpc_scb_cubby_t *cubby = DTPC_SCB_CUBBY(dtpci, scb);
	dtpc_prb_pwr_tbl_t *txpwroff_cache;
#ifdef WL11AC
	dtpc_mcs = cubby->dtpc_rspec & WL_RSPEC_VHT_MCS_MASK;
	dtpc_nss = (cubby->dtpc_rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT;
	mcs = rspec & WL_RSPEC_VHT_MCS_MASK;
	nss = (rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT;
#else
	dtpc_mcs = cubby->dtpc_rspec & WL_RSPEC_RATE_MASK;
	dtpc_nss = (cubby->dtpc_rspec & WLC_RSPEC_HT_NSS_MASK) >> WLC_RSPEC_HT_NSS_SHIFT;
	mcs = rspec & WL_RSPEC_RATE_MASK;
	nss = (rspec & WLC_RSPEC_HT_NSS_MASK) >> WLC_RSPEC_HT_NSS_SHIFT;
#endif // endif

	txpwroff_cache = &cubby->ptbl[mcs][nss];
	if (cubby->state == DTPC_START) {
		dtpc_cleanup_stat(dtpci, scb);
		cubby->epoch = epoch;
		cubby->psr_pro = 0;
		if (dtpc_mcs | dtpc_nss) {
			if (txpwroff_cache->qdb && (dtpc_mcs == mcs) && (dtpc_nss == nss)) {
				/* probing same rate to refine txpwroff based on previous one */
				if ((cubby->probe_dir == POSITIVE_PROBING) &&
					((txpwroff_cache->qdb + dtpci->refine_step)
					<= max_txpwroff)) {
					cubby->prb_txpwroff = txpwroff_cache->qdb
						+ dtpci->refine_step;
				} else if ((cubby->probe_dir == NEGATIVE_PROBING) &&
					(txpwroff_cache->qdb >= dtpci->refine_step)) {
					cubby->prb_txpwroff = txpwroff_cache->qdb
						- dtpci->refine_step;
				} else {
					/* No txpwroff to probe */
					cubby->state = DTPC_STOP;
				}
			} else {
				/* rate has changed */
				if (txpwroff_cache->qdb == 0) {
					/* no cached value */
					cubby->start_txpwroff = 0;
					cubby->end_txpwroff = max_txpwroff;
					cubby->prb_txpwroff = dtpc_find_txpwroff(dtpci, scb);
					if (cubby->rate_up) {
						dtpc_prb_pwr_tbl_t *dn_tbl = NULL;
						if (mcs) {
							uint8 temp_pwr = 0;
							dn_tbl = &cubby->ptbl[mcs - 1u][nss];
							temp_pwr = VALIDATE_PWR(dn_tbl->qdb,
								max_txpwroff);
							if (temp_pwr != INVALID_PWR) {
								cubby->prb_txpwroff = temp_pwr;
							} else {
								cubby->prb_txpwroff = max_txpwroff;
							}
						}
					}
				} else {
					/* there is cached txpwroff */
					cubby->prb_txpwroff  =  txpwroff_cache->qdb;
				}
			}
			cubby->end_txpwroff = max_txpwroff;
		} else {
			/* first time ever DTPC is running per this link */
			cubby->start_txpwroff = 0;
			cubby->end_txpwroff = max_txpwroff;
			cubby->probe_dir = POSITIVE_PROBING;
			cubby->prb_txpwroff = dtpc_find_txpwroff(dtpci, scb);
		}
		/* cahched value is the best so far */
		cubby->best_txpwroff  =  txpwroff_cache->qdb;
		RL_DTPC_CMP(("[DTPC]: txpwroff=%d,best_txpwroff=%d,start=%d,end=%d\n",
			cubby->prb_txpwroff, cubby->best_txpwroff,
			cubby->start_txpwroff, cubby->end_txpwroff));
		cubby->dtpc_rspec = rspec;
	} else if (cubby->state == DTPC_PROBE) {
		if (txpwroff_cache->qdb == 0) {
			cubby->prb_txpwroff = dtpc_find_txpwroff(dtpci, scb);
		} else {
			if (cubby->probe_dir == NEGATIVE_PROBING &&
					cubby->best_txpwroff >= dtpci->refine_step) {
				cubby->prb_txpwroff = cubby->best_txpwroff - dtpci->refine_step;
			} else if (cubby->probe_dir == POSITIVE_PROBING &&
					((cubby->best_txpwroff + dtpci->refine_step)
						<= max_txpwroff)) {
				cubby->prb_txpwroff = cubby->best_txpwroff + dtpci->refine_step;
			} else {
				cubby->state = DTPC_STOP;
			}
		}
	}
	return;
}

static void
dtpc_collect_stats(wlc_dtpc_info_t *dtpci, scb_t *scb, ratespec_t rspec,
		uint32 mrt_succ)
{
	dtpc_scb_cubby_t *cubby = DTPC_SCB_CUBBY(dtpci, scb);
	if (cubby) {
#ifdef WL11AC
		uint8 mcs = rspec & WL_RSPEC_VHT_MCS_MASK;
		uint8 nss = (rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT;
#else
		uint8 mcs = rspec & WL_RSPEC_RATE_MASK;
		uint8 nss = (rspec & WLC_RSPEC_HT_NSS_MASK) >> WLC_RSPEC_HT_NSS_SHIFT;
#endif // endif
		dtpc_prb_pwr_tbl_t *txpwroff_cache = &cubby->ptbl[mcs][nss];
#if defined(BCMDBG) || defined(WLTEST)
		dtpc_stats_t *lg = cubby->logs;
		if (lg) {
			uint8 bidx = lg->boost_idx;
			/* only for NSS2 case for now */
			if (nss == 0x2) {
				lg->ratesel_totpkt[mcs]++;
				lg->ratesel_tstmp[mcs] = DTPC_NOWTIME(dtpci);
			/* increase total success MPDUs in current applied boost index */
				lg->boost_hist[bidx][mcs] += mrt_succ;
			}
		}
#endif // endif
		/* refresh black listing aging */
		if (txpwroff_cache->qdb == DTPC_BLKLISTED &&
			(txpwroff_cache->tstamp + DTPC_BLKLIST_TIMEOUT)
				<= DTPC_NOWTIME(dtpci)) {
			txpwroff_cache->qdb = 0;
			txpwroff_cache->tstamp = DTPC_NOWTIME(dtpci);
		}
	}
	return;
}

/** pick the candidate boosting power and apply it to ratemem.
 *  Function executes when:
 *  a) Rates changed to a higher rate
 *  b) timer tick
 *  c) Probing active
 */
static bool
dtpc_select_pwr(wlc_dtpc_info_t *dtpci, scb_t *scb, ratespec_t rspec,
	uint8 epoch, uint32 psr, uint32 pktcnt, bool highest_mcs)
{
	dtpc_scb_cubby_t *cubby = DTPC_SCB_CUBBY(dtpci, scb);
	uint8 extra_backoff_factor = dtpci->extra_backoff_factor;
	if (cubby) {
		uint8 max_txpwroff = 0;
		if ((pktcnt >= DTPC_PKTS_THRESH) && (psr >= DTPC_PSR_THRESH) && highest_mcs) {
			cubby->state = DTPC_STOP;
			cubby->psr_init = psr;
		} else {
			if (cubby->state == DTPC_START) {
				cubby->psr_init = psr;
				cubby->aging_time = DTPC_NOWTIME(dtpci);
			}
			max_txpwroff = dtpc_get_headroom(dtpci, scb, rspec);
			/* cur_backoff is already updated corresponding ratespec from here */
			max_txpwroff = MIN(max_txpwroff, DTPC_MAX_HEADROOM);
			max_txpwroff = MIN(max_txpwroff, cubby->cur_backoff);
			max_txpwroff = (max_txpwroff > extra_backoff_factor) ?
				max_txpwroff - extra_backoff_factor : 0;
			RL_DTPC_DBG(("[DTPC-PWRLIMIT] backoff offset=%d, max_offset=%d\n",
				cubby->cur_backoff, max_txpwroff));

			/* if max_txpwroff doesn't have headroom, stop the probing */
			if (max_txpwroff == 0) {
				cubby->state = DTPC_STOP;
			}
			cubby->num_pro++;
			dtpc_get_prb_txpwroff(dtpci, scb, rspec, epoch, max_txpwroff);
			if (cubby->state != DTPC_STOP) {
				if (cubby->prb_txpwroff <= max_txpwroff &&
					cubby->num_pro < dtpci->pnum_thresh) {
					RL_DTPC_CMP(("[DTPC]: txpwroff=%d, num_pro=%d\n",
						cubby->prb_txpwroff, cubby->num_pro));
					cubby->state = DTPC_PROBE;
				} else {
					cubby->state = DTPC_STOP;
				}
			}
		}
	}

	if (cubby->state != DTPC_STOP) {
		return TRUE;
	} else {
		cubby->last_prb_time = OSL_SYSUPTIME();
		return FALSE;
	}
}

/** check the current tx status input with bfm flag. */
static bool
dtpc_is_bfm_pkt(wlc_dtpc_info_t *dtpci, tx_status_t *txs)
{
	bool ret = FALSE;
	UNUSED_PARAMETER(dtpci);
	if ((txs->status.s5 & TX_STATUS40_IMPBF_MASK) ||
		(txs->status.s5 & TX_STATUS40_BFTX)) {
		ret = TRUE;
	}
	return ret;
}

/** txbf active checking wrapper. logically wlc_txbf_sel has final checking for
 *  additive txpwr backoff for txbf rate, but it should be ignored with this feature.
 *  only check CLM rate database whether the given input rate has txbf.
 */
static bool
dtpc_txbf_en(wlc_dtpc_info_t *dtpci, scb_t *scb, ratespec_t rspec)
{
	bool ret = TRUE;
	wlc_info_t *wlc = dtpci->wlc;
	txpwr204080_t txpwrs;
	uint8 shidx = 0;
	if (rspec & WL_RSPEC_TXBF) {
		ret = TRUE;
	} else if (wlc_tpc_get_txpwrs(wlc, rspec, &txpwrs) == BCME_OK) {
		ret = wlc_txbf_sel(wlc->txbf, rspec, scb, &shidx, &txpwrs);
	} else {
		ret = FALSE;
	}

	return ret;
}
static bool
dtpc_start_pwr_prb(wlc_dtpc_info_t *dtpci, scb_t *scb, uint32 pktcnt,
		ratespec_t rspec)
{
	dtpc_scb_cubby_t *cubby = DTPC_SCB_CUBBY(dtpci, scb);
	dtpc_prb_pwr_tbl_t *tbl;
	bool ret = FALSE;
#ifdef WL11AC
	uint8 mcs = rspec & WL_RSPEC_VHT_MCS_MASK;
	uint8 nss = (rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT;
#else
	uint8 mcs = rspec & WL_RSPEC_RATE_MASK;
	uint8 nss = (rspec & WLC_RSPEC_HT_NSS_MASK) >> WLC_RSPEC_HT_NSS_SHIFT;
#endif // endif
	if (cubby) {
		tbl = &cubby->ptbl[mcs][nss];
		if (tbl->qdb == DTPC_BLKLISTED) {
			ret = FALSE;
		} else {
			/* max target should not over the phy max */
			if (wlc_tpc_get_target_offset(dtpci->wlc, rspec,
				&cubby->cur_backoff) != BCME_OK) {
				ASSERT(0);
			}
			if (cubby->cur_backoff < dtpci->extra_backoff_factor) {
				ret = FALSE;
			} else if ((cubby->last_prb_time + dtpci->trig_intval) <= OSL_SYSUPTIME()) {
				/* trigger aging case */
				cubby->state = DTPC_START;
				cubby->last_prb_time = OSL_SYSUPTIME();
#if defined(BCMDBG) || defined(WLTEST)
				if (cubby->logs) {
					cubby->logs->prob_strt[mcs]++;
				}
#endif // endif
				cubby->rate_up = FALSE;
				ret = TRUE;

				RL_DTPC_CMP(("[DTPC-START] pktcnt=%d, rspec=0x%x, rsn=aging\n",
					pktcnt, rspec));
			}
		}
	}
	return ret;
}
/** dtpc pwr boosting engine to find the optimal amount. TX status event driven.
 */
static void
dtpc_upd_stat(wlc_dtpc_info_t *dtpci, scb_t *scb, tx_status_t *txs,
		uint32 prate_prim, uint32 psr_prim, uint32 prate_dn, uint32 psr_dn, uint32 psr_up,
		uint32 cur_psr, uint8 epoch, uint32 pktcnt, bool highest_mcs)
{
	uint8 mcs, nss;
	dtpc_scb_cubby_t *cubby = DTPC_SCB_CUBBY(dtpci, scb);
#ifdef WL11AC
	mcs = cubby->dtpc_rspec & WL_RSPEC_VHT_MCS_MASK;
	nss = (cubby->dtpc_rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT;
#else
	mcs = cubby->dtpc_rspec & WL_RSPEC_RATE_MASK;
	nss = (cubby->dtpc_rspec & WLC_RSPEC_HT_NSS_MASK) >> WLC_RSPEC_HT_NSS_SHIFT;
#endif // endif
	if (cubby) {
		dtpc_prb_pwr_tbl_t *txpwroff_cache = &cubby->ptbl[mcs][nss];

		ASSERT(cubby->state == DTPC_PROBE);
#if defined(BCMDBG) || defined(WLTEST)
		/* only for NSS2 case for now */
		if (cubby->dtpc_rspec && (nss == 0x2)) {
			cubby->logs->prob_totpkt[mcs]++;
		}
#endif // endif

		if (DTPC_NOWTIME(dtpci) - cubby->aging_time >= DTPC_TXS_AGING) {
			cubby->state = DTPC_STOP;
			cubby->aging_time = DTPC_NOWTIME(dtpci);
			RL_DTPC_CMP(("[DTPC]: stop due to aging timer\n"));
		}
		dtpc_collect_prb_results(dtpci, scb, txs, txpwroff_cache, epoch, cur_psr,
				psr_prim, prate_prim, psr_dn, prate_dn, pktcnt, highest_mcs);
		/* moving avg and sampling stat */
		if (cubby && cubby->state == DTPC_STOP) {
			uint8 nss_idx = nss;
			uint8 stat_idx = mcs;
			dtpc_prb_pwr_tbl_t *tbl = &cubby->ptbl[stat_idx][nss_idx];
			/* record pwr probing qdb */
			if (tbl->qdb != DTPC_BLKLISTED && cubby->best_txpwroff != 0) {
				tbl->qdb = cubby->best_txpwroff;
				tbl->tstamp = DTPC_NOWTIME(dtpci);
			}
			/* set to current rate until next rate change */
			cubby->rate_up = FALSE;
			cubby->last_prb_time = OSL_SYSUPTIME();
#if defined(BCMDBG) || defined(WLTEST)
			if (cubby->best_txpwroff && cubby->logs) {
				dtpc_stats_t *pl = cubby->logs;
				uint8 sidx = pl->samp_ref_idx[stat_idx];
				dtpc_sample_pack_t *entry = &pl->samp[stat_idx];

				entry->offset[sidx] = cubby->best_txpwroff;
				sidx = BOUNDINC(sidx, DTPC_SAMPLE_WINDOW);
				pl->samp_tot_cnt[stat_idx] = MODINC(pl->samp_tot_cnt[stat_idx],
					DTPC_SAMPLE_WINDOW);
				pl->samp_ref_idx[stat_idx] = sidx;
			}
#endif // endif
		}
	}
	return;
}

static void
dtpc_monitor(wlc_dtpc_info_t *dtpci, scb_t *scb, ratespec_t rspec,
		bool goup_rateid, uint8 epoch, bool highest_mcs, bool nss_chg)
{
	dtpc_scb_cubby_t *cubby = DTPC_SCB_CUBBY(dtpci, scb);
	uint8 temp_pwr = 0;
	uint8 max_txpwroff = 0;
	if (cubby) {
#ifdef WL11AC
		uint8 mcs = rspec & WL_RSPEC_VHT_MCS_MASK;
		uint8 nss = (rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT;
#else
		uint8 mcs = rspec & WL_RSPEC_RATE_MASK;
		uint8 nss = (rspec & WLC_RSPEC_HT_NSS_MASK) >> WLC_RSPEC_HT_NSS_SHIFT;
#endif // endif
		dtpc_prb_pwr_tbl_t *tbl = &cubby->ptbl[mcs][nss];
		if (nss_chg || !dtpc_txbf_en(dtpci, scb, rspec)) {
			/* skip any boosting activity when NSS is changed or txbf is not
			 * activated
			 */
			goto exit;
		}
		if (goup_rateid) {
			/* rate is going up */
			if (!cubby->sp_started && tbl->qdb == 0) {
				cubby->rate_up = TRUE;
				cubby->state = DTPC_START;
				dtpc_select_pwr(dtpci, scb, rspec, epoch, DTPC_PSR_THRESH,
					0, highest_mcs);
				RL_DTPC_CMP(("[DTPC] Started probing on go up rpsec=0x%x\n",
					(rspec & 0xFF)));
			}
		} else {
			dtpc_prb_pwr_tbl_t *up_tbl = NULL;
			if (!highest_mcs) {
				ASSERT(mcs < (DTPC_RATE_ARRAYSIZE - 1u));
				up_tbl = &cubby->ptbl[mcs + 1u][nss];
			}
			if (tbl->qdb == 0 && up_tbl) {
				uint8 extra_backoff_factor = dtpci->extra_backoff_factor;
				if (wlc_tpc_get_target_offset(dtpci->wlc, rspec,
					&cubby->cur_backoff) != BCME_OK) {
					ASSERT(0);
				}
				/* regulatory limit boundary check */
				max_txpwroff = dtpc_get_headroom(dtpci, scb, rspec);
				max_txpwroff = (max_txpwroff > extra_backoff_factor) ?
					max_txpwroff - extra_backoff_factor : 0;
				temp_pwr = VALIDATE_PWR(up_tbl->qdb, MIN(cubby->cur_backoff,
					max_txpwroff));
				if (temp_pwr != INVALID_PWR) {
					tbl->qdb = temp_pwr;
					tbl->tstamp = DTPC_NOWTIME(dtpci);
				}
			}
		}
	}
exit:
	return;
}

/** add extra qdB pwroffset. phy final target power will be backed-off mostly with
 *  backoff input here, and this function will add the extra power amount
 *  based on the backoff.
 */
static int16
dtpc_recalc_txpwroff(wlc_dtpc_info_t *dtpci, scb_t *scb, uint8 backoff, ratespec_t rspec)
{
	dtpc_scb_cubby_t *cubby = DTPC_SCB_CUBBY(dtpci, scb);
	int16 txpwr_off = 0;
	uint16 txpwroff = 0;
	uint8 temp_pwr = 0;
	uint8 dtpc_mcs, dtpc_nss, mcs, nss;
	uint8 extra_backoff_factor = dtpci->extra_backoff_factor;

	if (cubby) {
#ifdef WL11AC
		dtpc_mcs = cubby->dtpc_rspec & WL_RSPEC_VHT_MCS_MASK;
		dtpc_nss = (cubby->dtpc_rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT;
		mcs = rspec & WL_RSPEC_VHT_MCS_MASK;
		nss = (rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT;
#else
		dtpc_mcs = cubby->dtpc_rspec & WL_RSPEC_RATE_MASK;
		dtpc_nss = (cubby->dtpc_rspec & WLC_RSPEC_HT_NSS_MASK) >> WLC_RSPEC_HT_NSS_SHIFT;
		mcs = rspec & WL_RSPEC_RATE_MASK;
		nss = (rspec & WLC_RSPEC_HT_NSS_MASK) >> WLC_RSPEC_HT_NSS_SHIFT;
#endif // endif
		if ((cubby->state == DTPC_PROBE && (dtpc_mcs == mcs) && (dtpc_nss == nss))) {
			txpwroff = cubby->prb_txpwroff;
		} else {
			dtpc_prb_pwr_tbl_t *txpwroff_cache = &cubby->ptbl[mcs][nss];
			if (backoff < extra_backoff_factor) {
				txpwroff = 0;
			} else if (txpwroff_cache->qdb != DTPC_BLKLISTED && txpwroff_cache->qdb) {
				/* regulatory limit boundary check */
				temp_pwr = MIN(backoff, dtpc_get_headroom(dtpci, scb, rspec));
				if (txpwroff_cache->qdb > temp_pwr) {
					if (temp_pwr > extra_backoff_factor) {
						temp_pwr -= extra_backoff_factor;
					}
					txpwroff = temp_pwr;
#if defined(BCMDBG) || defined(WLTEST)
					cubby->logs->ovrlimitcnt++;
#endif // endif
				} else {
					txpwroff = txpwroff_cache->qdb;
				}
			}
		}
		/* the amount of txpwroff here is accumulative */
		txpwr_off = -(int16)txpwroff;
	}
#if defined(BCMDBG) || defined(WLTEST)
	/* histogram statistics */
	if ((txpwroff >> 2u) < DTPC_BOOST_SAMPLE) {
		cubby->logs->boost_idx = txpwroff >> 2u;
	}
#endif // endif
	txpwr_off += backoff;
	return txpwr_off;
}

static bool
dtpc_adj_lsrch_params(wlc_dtpc_info_t *dtpci, scb_t *scb)
{
	bool ret = FALSE;
	dtpc_scb_cubby_t *cubby = DTPC_SCB_CUBBY(dtpci, scb);
	if (cubby->psr_pro >= cubby->psr_init) {
		if (cubby->psr_pro == cubby->psr_init) {
			cubby->best_txpwroff = MAX(cubby->prb_txpwroff, cubby->best_txpwroff);
		} else {
			cubby->best_txpwroff = cubby->prb_txpwroff;
		}
		cubby->psr_init = cubby->psr_pro;
		cubby->state = DTPC_STOP;
		ret = TRUE;
	} else {
		cubby->probe_dir = (cubby->probe_dir == POSITIVE_PROBING) ?
			NEGATIVE_PROBING : POSITIVE_PROBING;
	}
	return ret;
}

static void
dtpc_adj_bsrch_params(wlc_dtpc_info_t *dtpci, scb_t *scb, bool highest_mcs)
{
	dtpc_scb_cubby_t *cubby = DTPC_SCB_CUBBY(dtpci, scb);
	if ((cubby->end_txpwroff - cubby->start_txpwroff) > 2u) {
		if ((cubby->psr_pro > cubby->psr_init) |
			((cubby->psr_pro == cubby->psr_init) && (!highest_mcs)))   {
			cubby->start_txpwroff = cubby->prb_txpwroff;
			cubby->best_txpwroff = cubby->prb_txpwroff;
			cubby->psr_init = cubby->psr_pro;
		} else if (cubby->psr_pro < cubby->psr_init) {
			cubby->end_txpwroff = cubby->prb_txpwroff;
		}
	} else {
		cubby->state = DTPC_STOP;
	}

	RL_DTPC_DBG(("[DTPC] start txpwroff%d|end txpwroff=%d qdb, best_txpwroff %d\n",
		cubby->start_txpwroff, cubby->end_txpwroff, cubby->best_txpwroff));
	return;
}

static int
dtpc_find_txpwroff(wlc_dtpc_info_t *dtpci, scb_t *scb)
{
	dtpc_scb_cubby_t *cubby = DTPC_SCB_CUBBY(dtpci, scb);

	cubby->prb_txpwroff = (cubby->start_txpwroff + cubby->end_txpwroff) >> 1u;
	return cubby->prb_txpwroff;
}

static void
dtpc_cleanup_stat(wlc_dtpc_info_t *dtpci, scb_t *scb)
{
	dtpc_scb_cubby_t *cubby = DTPC_SCB_CUBBY(dtpci, scb);

	cubby->prb_txpwroff = 0;
	cubby->num_pro = 0;
	cubby->num_txs = 0;
	cubby->non_bf_txs = 0;
	cubby->psr_pro = 0;
	cubby->start_txpwroff = 0;
	memset(&cubby->headroom, 0, sizeof(phy_tx_targets_per_core_t));
	memset(&cubby->minpwr, 0, sizeof(phy_tx_targets_per_core_t));
	return;
}

/* bsscfg event update notification callback */
static void
dtpc_bsscfg_updown_cb(void *ctx, bsscfg_up_down_event_data_t *evt_data)
{
	wlc_dtpc_info_t *dtpci = (wlc_dtpc_info_t *)ctx;
	wlc_info_t *wlc = dtpci->wlc;

	if (!evt_data->up) {
		/* bsscfg down */
		dtpc_power_context_cleanup(dtpci);
	} else {
		/* bsscfg is up */
		if (!dtpci->pwrctxt) {
			dtpci->pwrctxt = dtpc_power_context_create(wlc);
		}
		if (dtpci->pwrctxt) {
			if (!dtpc_power_context_set_channel(dtpci,
				evt_data->bsscfg->current_bss->chanspec)) {
				dtpci->active = FALSE;
				WL_ERROR(("wl%d: dtpc_power_context ppr creation failed\n",
					WLCWLUNIT(wlc)));
			}
		}

		dtpci->extra_backoff_factor = wlc->pi->tx_pwr_backoff;
	}
}

/* channel switch notification callback */
static void
dtpc_chansw_notif_cb(void *arg, wlc_chansw_notif_data_t *data)
{
	wlc_dtpc_info_t *dtpci = (wlc_dtpc_info_t *)arg;

	if (!wlc_dtpc_active(dtpci))
		return;

	if (!dtpc_power_context_set_channel(dtpci, data->new_chanspec)) {
		/* no valid DTPC context with the given channel, disable DTPC */
		dtpci->active = FALSE;
		ASSERT(dtpci->active);
		WL_ERROR(("[DTPC-CHAN] wl:%d DTPC disable no valid pwrctxt, chanspec=0x%x\n",
			WLCWLUNIT(dtpci->wlc), data->new_chanspec));
	} else {
		/* clean up every active scbs */
		int32 idx;
		wlc_bsscfg_t *cfg;
		FOREACH_BSS(dtpci->wlc, idx, cfg) {
			struct scb *pick_scb;
			struct scb_iter scbiter;
			FOREACH_BSS_SCB(dtpci->wlc->scbstate, &scbiter, cfg, pick_scb) {
				if (pick_scb && !SCB_INTERNAL(pick_scb)) {
					dtpc_scb_cubby_t *cubby = DTPC_SCB_CUBBY(dtpci, pick_scb);
					dtpc_scb_init_state(cubby);
				}
			}
		}
	}
	return;
}

static void
dtpc_watchdog_cb(void *ctx)
{
	/* periodic update and processing per system watchdog mostly per 1 sec */
	UNUSED_PARAMETER(ctx);
	return;
}

static uint
dtpc_scb_secsz(void *ctx, struct scb *scb)
{
	uint size = 0;
	if (scb && !SCB_INTERNAL(scb)) {
		size = ALIGN_SIZE(sizeof(dtpc_scb_cubby_t), sizeof(uint32));
#if defined(BCMDBG) || defined(WLTEST)
		size += ALIGN_SIZE(sizeof(dtpc_stats_t), sizeof(uint32));
#endif // endif
	}
	return size;
}

static int
dtpc_scb_init(void *ctx, struct scb *scb)
{
	wlc_dtpc_info_t *dtpci = (wlc_dtpc_info_t *)ctx;
	dtpc_scb_cubby_t **pcubby = DTPC_SCB_CUBBY_LOC(dtpci, scb);
	wlc_info_t *wlc = dtpci->wlc;
	int ret = BCME_OK;
	dtpc_scb_cubby_t *cubby;
	uint8 *secptr = wlc_scb_sec_cubby_alloc(wlc, scb, dtpc_scb_secsz(ctx, scb));

	cubby = *pcubby = (dtpc_scb_cubby_t *)secptr;
	if (cubby != NULL) {
		secptr += ALIGN_SIZE(sizeof(dtpc_scb_cubby_t), sizeof(uint32));
#if defined(BCMDBG) || defined(WLTEST)
		cubby->logs = (dtpc_stats_t *)secptr;
#endif // endif
		dtpc_scb_init_state(cubby);
	}
	return ret;
}

static void
dtpc_scb_init_state(dtpc_scb_cubby_t *cubby)
{
	if (cubby != NULL) {
		cubby->state = DTPC_READY;
		cubby->cur_backoff = DTPC_BACKOFF_INVALID;
		cubby->pwroff = DTPC_PWROFFSET_INVALID;
	}
}

static void
dtpc_scb_deinit(void *ctx, struct scb *scb)
{
	wlc_dtpc_info_t *dtpci = (wlc_dtpc_info_t *)ctx;
	dtpc_scb_cubby_t **pcubby = DTPC_SCB_CUBBY_LOC(dtpci, scb);
	dtpc_scb_cubby_t *cubby = *pcubby;
	if (cubby) {
		wlc_scb_sec_cubby_free(dtpci->wlc, scb, cubby);
	}
	*pcubby = NULL;
	return;
}

static dtpc_txpwr_ctxt_t *
dtpc_power_context_create(wlc_info_t *wlc)
{
	dtpc_txpwr_ctxt_t *ret =
		(dtpc_txpwr_ctxt_t *)MALLOCZ(wlc->osh, sizeof(dtpc_txpwr_ctxt_t));
	if (ret == NULL) {
		WL_ERROR(("wl%d: dtpc_power_context_create(): out of mem, malloced %d bytes\n",
			WLCWLUNIT(wlc), MALLOCED(wlc->osh)));
	} else {
		ret->board_min = NULL;
		ret->clm_max = NULL;
	}
	return ret;
}

static void
dtpc_power_context_delete(wlc_dtpc_info_t *dtpci)
{
	dtpc_power_context_cleanup(dtpci);
	if (dtpci->pwrctxt) {
		MFREE(dtpci->wlc->osh, dtpci->pwrctxt, sizeof(*dtpci->pwrctxt));
		dtpci->pwrctxt = NULL;
	}
}

static bool
dtpc_power_context_set_channel(wlc_dtpc_info_t *dtpci, chanspec_t chanspec)
{
	dtpc_txpwr_ctxt_t *power_context = dtpci->pwrctxt;
	ppr_t *ppr_new;
	wl_tx_bw_t new_ppr_bw = PPR_CHSPEC_BW(chanspec);

	if (power_context == NULL) {
		ASSERT(power_context != NULL);
		return FALSE;
	}

	if (power_context->board_min != NULL) {
		if (power_context->ppr_bw != new_ppr_bw) {
			ppr_delete(dtpci->wlc->osh, power_context->board_min);
			power_context->board_min = NULL;
		}
	}
	if (power_context->board_min == NULL) {
		if ((ppr_new = ppr_create(dtpci->wlc->osh, new_ppr_bw)) == NULL) {
			goto fail;
		}
		power_context->board_min = ppr_new;
	}
	if (power_context->clm_max != NULL) {
		if (power_context->ppr_bw != new_ppr_bw) {
			ppr_delete(dtpci->wlc->osh, power_context->clm_max);
			power_context->clm_max = NULL;
		}
	}
	if (power_context->clm_max == NULL) {
		if ((ppr_new = ppr_create(dtpci->wlc->osh, new_ppr_bw)) == NULL) {
			goto fail;
		}
		power_context->clm_max = ppr_new;
	}
	power_context->ppr_bw = new_ppr_bw;
	/* CLM limit */
	/* TODO: Last update from Fedor for 4th input parameter to support RU case */
	wlc_channel_reg_limits(dtpci->wlc->cmi, chanspec, power_context->clm_max, NULL);
	/* board limit */
	wlc_phy_txpower_sromlimit(dtpci->wlc->pi, chanspec, NULL,
		power_context->board_min, 0);

	return TRUE;

fail:
	dtpc_power_context_cleanup(dtpci);
	return FALSE;
}

static void
dtpc_power_context_cleanup(wlc_dtpc_info_t *dtpci)
{
	dtpc_txpwr_ctxt_t *power_context = dtpci->pwrctxt;

	if (power_context) {
		if (power_context->board_min) {
			ppr_delete(dtpci->wlc->osh, power_context->board_min);
			power_context->board_min = NULL;
		}
		if (power_context->clm_max) {
			ppr_delete(dtpci->wlc->osh, power_context->clm_max);
			power_context->clm_max = NULL;
		}
	}
}

static int32
dtpc_calc_nominal_pwr(wlc_dtpc_info_t *dtpci, scb_t *scb, ratespec_t rspec)
{
	int32 ret = BCME_OK;
	wlc_info_t *wlc = dtpci->wlc;
	dtpc_scb_cubby_t *cubby = DTPC_SCB_CUBBY(dtpci, scb);
	if (cubby) {
		ret = wlc_tpc_get_tx_target_per_core(wlc, rspec, &cubby->minpwr);
	}
	return ret;
}

static int32
dtpc_power_context_get_limits(wlc_dtpc_info_t *dtpci, scb_t *scb,
		ratespec_t rspec)
{
	int32 ret = BCME_OK;
	dtpc_scb_cubby_t *cubby = DTPC_SCB_CUBBY(dtpci, scb);
	dtpc_txpwr_ctxt_t *pwr = dtpci->pwrctxt;
	int8 clm_max_pwr = 0;
	int idx = 0;

	if (cubby == NULL || pwr == NULL) {
		ASSERT(0);
		ret = BCME_BADARG;
	} else {
		if (pwr->clm_max == NULL) {
			if (!dtpc_power_context_set_channel(dtpci,
				phy_utils_get_chanspec(WLC_PI(dtpci->wlc)))) {
				return BCME_NOMEM;
			}
		}
		if (pwr->clm_max) {
			/* regulatory limit */
			clm_max_pwr = (int8)get_pwr_from_targets(dtpci->wlc, rspec, pwr->clm_max);
			/* phy target power */
			ret = dtpc_calc_nominal_pwr(dtpci, scb, rspec);
			if (ret == BCME_OK) {
				for (idx = 0; idx < PHY_CORE_MAX; idx++) {
					if (clm_max_pwr > cubby->minpwr.pwr[idx]) {
						cubby->headroom.pwr[idx] = clm_max_pwr -
							cubby->minpwr.pwr[idx];
					} else {
						cubby->headroom.pwr[idx] = 0;
					}
				}
			}
		}
	}
	return ret;
}

#if defined(BCMDBG) || defined(WLTEST)
static int
dtpc_dump_clr(void *ctx)
{
	wlc_dtpc_info_t *dtpci = (wlc_dtpc_info_t *)ctx;
	/* clear all scb infos */
	dtpc_clear_logs(dtpci, NULL);
	return BCME_OK;
}

static void
dtpc_clear_logs(wlc_dtpc_info_t *dtpci, struct scb *scb)
{
	dtpc_scb_cubby_t *cubby = NULL;
	if (scb) {
		cubby = DTPC_SCB_CUBBY(dtpci, scb);
		if (!SCB_INTERNAL(scb) && cubby) {
			if (cubby->logs) {
				memset(cubby->logs, 0u, sizeof(dtpc_stats_t));
			}
		}
	} else {
		int32 idx;
		wlc_bsscfg_t *cfg;

		FOREACH_BSS(dtpci->wlc, idx, cfg) {
		struct scb *pick_scb;
		struct scb_iter scbiter;
		FOREACH_BSS_SCB(dtpci->wlc->scbstate, &scbiter, cfg, pick_scb) {
			if (pick_scb && !SCB_INTERNAL(pick_scb)) {
				cubby = DTPC_SCB_CUBBY(dtpci, pick_scb);
				if (cubby && cubby->logs) {
					memset(cubby->logs, 0u, sizeof(dtpc_stats_t));
				}
			}
		}
		}
	}
	return;
}

static void
wlc_dtpc_collect_dbg(wlc_dtpc_info_t *dtpci, scb_t *scb, ratespec_t rspec,
	uint32 mrt, uint32 mrt_succ, uint32 fbr, uint32 fbr_succ,
	uint32 prim_psr, uint32 fbr_psr)
{
	dtpc_scb_cubby_t *cubby = DTPC_SCB_CUBBY(dtpci, scb);
	if (cubby) {
		dtpc_stats_t *log = cubby->logs;
#ifdef WL11AC
		uint8 mcs = rspec & WL_RSPEC_VHT_MCS_MASK;
		uint8 nss = (rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT;
#else
		uint8 mcs = rspec & WL_RSPEC_RATE_MASK;
		uint8 nss = (rspec & WLC_RSPEC_HT_NSS_MASK) >> WLC_RSPEC_HT_NSS_SHIFT;
#endif // endif
		dtpc_prb_pwr_tbl_t *tbl = &cubby->ptbl[mcs][nss];
		/* only for NSS2 case for now */
		if (nss == 2u) {
			log->bd[mcs].mrtsucc = DTPC_PERCNT_CONV(mrt_succ, mrt);
			log->bd[mcs].cur_psr = DTPC_EMA_CONV(mrt_succ, mrt);
			log->bd[mcs].curmv_psr = prim_psr;
			log->bd[mcs].fbsucc = DTPC_PERCNT_CONV(fbr_succ, fbr);
			log->bd[mcs].fbr_psr = DTPC_EMA_CONV(fbr_succ, fbr);
			log->bd[mcs].fbrmv_psr = fbr_psr;
			log->bd[mcs].boosted_qdb = tbl->qdb;
			log->bd[mcs].tstamp = OSL_SYSUPTIME();
		}
	}
	return;
}

static void
_dtpc_scb_dump_by_cubby(wlc_dtpc_info_t *dtpci, struct scb *scb, struct bcmstrbuf *b)
{
	dtpc_scb_cubby_t *cubby = DTPC_SCB_CUBBY(dtpci, scb);
	dtpc_stats_t *pl;

	if (!cubby)
		return;

	if (SCB_DTPC_CAP(scb)) {
		int i, y;
		uint8 nss_idx = PHY_MAX_CORES;
		pl = cubby->logs;
		bcm_bprintf(b, "\n");
		bcm_bprintf(b, "[DTPC LOG] "MACF"\n\n", ETHER_TO_MACF(scb->ea));
		bcm_bprintf(b, "  [MCS]   :\t11\t10\t9\t8\t7\t6\t5\t4\t3\t2\t1\t0\n");
		bcm_bprintf(b, "pb strt   :\t");
		for (i = DTPC_RATE_ARRAYSIZE - 1; i >= 0; i--) {
			bcm_bprintf(b, "%d\t", pl->prob_strt[i]);
		}
		bcm_bprintf(b, "\n");
		bcm_bprintf(b, "pb pktcnt :\t");
		for (i = DTPC_RATE_ARRAYSIZE - 1; i >= 0; i--) {
			bcm_bprintf(b, "%d\t", pl->prob_totpkt[i]);
		}
		bcm_bprintf(b, "\n");
		bcm_bprintf(b, "ratetotal :\t");
		for (i = DTPC_RATE_ARRAYSIZE - 1; i >= 0; i--) {
			bcm_bprintf(b, "%d\t", pl->ratesel_totpkt[i]);
		}
		bcm_bprintf(b, "\n");
		bcm_bprintf(b, "==== boost hist (dB) ====\n");
		bcm_bprintf(b, "   \t<1\t<2\t<3\t<4\t<5\t<6\t<7\t<8\t8+\n");
		for (y = 11; y >= 0; y--) {
			bcm_bprintf(b, "%2d:", y);
			for (i = 0; i < DTPC_BOOST_SAMPLE; i++) {
				bcm_bprintf(b, "\t%d", pl->boost_hist[i][y]);
			}
			bcm_bprintf(b, "\n");
		}
		bcm_bprintf(b, "\n==== samples trace ====\n");
		for (i = DTPC_RATE_ARRAYSIZE - 1; i >= 0; i--) {
			if (pl->prob_strt[i]) {
				bcm_bprintf(b, "[MCS %dx2]: ", i);
				for (y = 0; y < DTPC_SAMPLE_WINDOW; y++) {
					if (pl->samp_ref_idx[i] == y) {
						bcm_bprintf(b, "|%d| ", pl->samp[i].offset[y]);
					} else {
						bcm_bprintf(b, "%d ", pl->samp[i].offset[y]);
					}
				}
				bcm_bprintf(b, "\n");
			}
		}
		bcm_bprintf(b, "\n");
		/* dbg statistics */
		bcm_bprintf(b, "\n==== dbg trace ====\n");
		bcm_bprintf(b, "  [MCS]   :\t11\t10\t9\t8\t7\t6\t5\t4\t3\t2\t1\t0\n");
		bcm_bprintf(b, "prim succ :\t");
		for (i = DTPC_RATE_ARRAYSIZE - 1; i >= 0; i--) {
			dtpc_dbgstats_pack_t *bad = &pl->bd[i];
			bcm_bprintf(b, "%d\t", bad->mrtsucc);
		}
		bcm_bprintf(b, "\nfb  succ :\t");
		for (i = DTPC_RATE_ARRAYSIZE - 1; i >= 0; i--) {
			dtpc_dbgstats_pack_t *bad = &pl->bd[i];
			bcm_bprintf(b, "%d\t", bad->fbsucc);
		}
		bcm_bprintf(b, "\nprim psr :\t");
		for (i = DTPC_RATE_ARRAYSIZE - 1; i >= 0; i--) {
			dtpc_dbgstats_pack_t *bad = &pl->bd[i];
			bcm_bprintf(b, "%d\t", bad->cur_psr);
		}
		bcm_bprintf(b, "\nprim mv  :\t");
		for (i = DTPC_RATE_ARRAYSIZE - 1; i >= 0; i--) {
			dtpc_dbgstats_pack_t *bad = &pl->bd[i];
			bcm_bprintf(b, "%d\t", bad->curmv_psr);
		}
		bcm_bprintf(b, "\nfb  psr  :\t");
		for (i = DTPC_RATE_ARRAYSIZE - 1; i >= 0; i--) {
			dtpc_dbgstats_pack_t *bad = &pl->bd[i];
			bcm_bprintf(b, "%d\t", bad->fbr_psr);
		}
		bcm_bprintf(b, "\nfb  mv   :\t");
		for (i = DTPC_RATE_ARRAYSIZE - 1; i >= 0; i--) {
			dtpc_dbgstats_pack_t *bad = &pl->bd[i];
			bcm_bprintf(b, "%d\t", bad->fbrmv_psr);
		}
		bcm_bprintf(b, "\nqdb/carry:\t");
		for (i = DTPC_RATE_ARRAYSIZE - 1; i >= 0; i--) {
			dtpc_dbgstats_pack_t *bad = &pl->bd[i];
			bcm_bprintf(b, "%d,%d\t", bad->boosted_qdb, bad->carriedover);
		}
		bcm_bprintf(b, "\ntime stmp:\t");
		/* limit to 4sec duration */
		for (i = DTPC_RATE_ARRAYSIZE - 1; i >= 0; i--) {
			dtpc_dbgstats_pack_t *bad = &pl->bd[i];
			bcm_bprintf(b, "%d\t", bad->tstamp & 0x7FFF);
		}
		bcm_bprintf(b, "\n");
		bcm_bprintf(b, "\n==== cached pwr table ====\n");
		for (nss_idx = PHY_MAX_CORES; nss_idx != 0; nss_idx--) {
			bcm_bprintf(b, "[NSS %d]:", nss_idx);
			for (i = DTPC_RATE_ARRAYSIZE - 1; i >= 0; i--) {
				dtpc_prb_pwr_tbl_t *tbl = &cubby->ptbl[i][nss_idx];
				bcm_bprintf(b, "%d,%d\t", tbl->qdb, tbl->tstamp & 0x3FF);
			}
			bcm_bprintf(b, "\n");
		}
		bcm_bprintf(b, "\nPWRLIMIT fail cnt: %d\n", pl->ovrlimitcnt);
		bcm_bprintf(b, "\n");
	} else {
		bcm_bprintf(b, "[DTPC LOG] "MACF" no DTPC support \n", ETHER_TO_MACF(scb->ea));

	}
	return;
}

static void
dtpc_scb_dump(void *ctx, struct scb *scb, struct bcmstrbuf *b)
{
	wlc_dtpc_info_t *dtpci = (wlc_dtpc_info_t *)ctx;

	if (scb) {
		_dtpc_scb_dump_by_cubby(dtpci, scb, b);
	} else {
		int32 idx;
		wlc_bsscfg_t *cfg;

		FOREACH_BSS(dtpci->wlc, idx, cfg) {
			struct scb *pick_scb;
			struct scb_iter scbiter;
			FOREACH_BSS_SCB(dtpci->wlc->scbstate, &scbiter, cfg, pick_scb) {
				if (pick_scb && !SCB_INTERNAL(pick_scb)) {
					_dtpc_scb_dump_by_cubby(dtpci, pick_scb, b);
				}
			}
		}
	}

	return;
}
#endif // endif

static void *
dtpc_iov_alloc_ctxt(void *ctx, uint size)
{
	uint8 *iov_ctx = NULL;
	wlc_info_t *wlc;
	wlc_dtpc_info_t *dtpci = (wlc_dtpc_info_t *)ctx;

	ASSERT(dtpci != NULL);
	wlc = dtpci->wlc;
	if ((iov_ctx = (uint8 *)MALLOCZ(wlc->osh, size)) == NULL) {
		WL_ERROR(("wl%d: dtpc_iov_alloc_ctxt: mallc fails, malloced %d bytes\n",
			WLCWLUNIT(wlc), MALLOCED(wlc->osh)));
	}
	return iov_ctx;
}

static void
dtpc_iov_free_ctxt(void *ctx, void *iov_ctx, uint size)
{
	wlc_dtpc_info_t *dtpci = (wlc_dtpc_info_t *)ctx;
	ASSERT(dtpci != NULL);
	if (iov_ctx) {
		MFREE(dtpci->wlc->osh, iov_ctx, size);
	}
	return;
}

static int
BCMATTACHFN(dtpc_iov_get_digest_cb)(void *ctx, bcm_iov_cmd_digest_t **dig)
{
	uint8 *iov_cmd_dig = NULL;
	wlc_info_t *wlc;
	wlc_dtpc_info_t *dtpci = (wlc_dtpc_info_t *)ctx;
	int ret = BCME_OK;

	ASSERT(dtpci != NULL);
	wlc = dtpci->wlc;
	if ((iov_cmd_dig = (uint8 *)MALLOCZ(wlc->osh, sizeof(bcm_iov_cmd_digest_t)))
		== NULL) {
		WL_ERROR(("wl%d: dtpc_iov_get_digest_cb: malloc fails. malloced %d bytes\n",
			WLCWLUNIT(wlc), MALLOCED(wlc->osh)));
		ret = BCME_NOMEM;
		goto exit;
	}
	*dig = (bcm_iov_cmd_digest_t *)iov_cmd_dig;
exit:
	return ret;
}

static int
dtpc_doiovar(void *hdl, uint32 actionid, void *p, uint plen,
	void *a, uint alen, uint vsize, struct wlc_if *wlcif)
{
	wlc_dtpc_info_t *dtpci = (wlc_dtpc_info_t *)hdl;
	int32 int_val = 0;
	uint8 *ret_uint8_val;
	int ret = BCME_OK;
	wlc_bsscfg_t *bsscfg;
	chanspec_t chanspec;
	int chanspec_bw;
	int iov_headroom_all = 0;
	uint num_txchains = 0;

	ret_uint8_val = (uint8 *)a;
	if (plen >= (int)sizeof(int_val)) {
		bcopy(p, &int_val, sizeof(int_val));
	}

	switch (actionid) {
		case IOV_GVAL(IOV_DTPC):
		case IOV_SVAL(IOV_DTPC):
		{
			ret = bcm_iov_doiovar(dtpci->iov_parse_ctx, actionid, p, plen, a, alen,
				vsize, wlcif);
			break;
		}
		case IOV_GVAL(IOV_DTPC_HEADROOM_ALL):
			if (alen < sizeof(wl_dtpc_cfg_headroom_t)) {
				return BCME_NOMEM;
			}
			iov_headroom_all = 1;
			/* fall through */
		case IOV_GVAL(IOV_DTPC_HEADROOM):
		{
			bsscfg = wlc_bsscfg_find_by_wlcif(dtpci->wlc, wlcif);
			chanspec = bsscfg->current_bss->chanspec;
			switch (CHSPEC_BW(chanspec)) {
				case WL_CHANSPEC_BW_20:
					chanspec_bw = BW_20MHZ;
					break;
				case WL_CHANSPEC_BW_40:
					chanspec_bw = BW_40MHZ;
					break;
				case WL_CHANSPEC_BW_80:
					chanspec_bw = BW_80MHZ;
					break;
				case WL_CHANSPEC_BW_160:
					chanspec_bw = BW_160MHZ;
					break;
				default:
					WL_ERROR(("unknown chanspec bandwidth\n"));
					return BCME_UNSUPPORTED;
			}
			num_txchains = WLC_BITSCNT(dtpci->wlc->stf->txchain);

			if (iov_headroom_all) {
				ret = dtpc_iov_get_headroom_all(dtpci, chanspec_bw,
					num_txchains, a);
			} else {
				ret = dtpc_iov_get_headroom(dtpci, chanspec_bw,
					num_txchains, int_val, ret_uint8_val);
			}
			break;
		}
		default:
			ret = BCME_UNSUPPORTED;
			break;
	}
	return ret;
}

static int
dtpc_iov_validate_cmds(const bcm_iov_cmd_digest_t *dig, uint32 actionid,
	const uint8 *ibuf, size_t ilen, uint8 *obuf, size_t *olen)
{
	int ret = BCME_OK;
	wlc_info_t *wlc = NULL;
	wlc_dtpc_info_t *dtpci = NULL;
	/* version pick should be in sync with wlc_types.h */
	wl_dtpc_cfg_v1_t *dtpc_cfg;

	UNUSED_PARAMETER(dtpc_cfg);

	dtpci = (wlc_dtpc_info_t *)dig->cmd_ctx;
	if (dtpci) {
		wlc = dtpci->wlc;
		/* version check */
		if (dig->version != WL_DTPC_IOV_VERSION) {
			printf("wl%d: iovar version mismatch req:0x%x != dongle:0x%x\n",
				wlc->pub->unit, dig->version, WL_DTPC_IOV_VERSION);
			ret = BCME_VERSION;
			goto fail;
		}
		/* length check */
		if (ilen < sizeof(*dtpc_cfg)) {
			ret = BCME_BUFTOOSHORT;
			goto fail;
		}
	} else {
		ret = BCME_BADARG;
		goto fail;
	}

fail:
	return ret;
}

/** sub-cmds handler functions */

static int
dtpc_iov_get_en(const bcm_iov_cmd_digest_t *dig,
	const uint8 *ibuf, size_t ilen, uint8 *obuf, size_t *olen)
{
	wlc_dtpc_info_t *dtpci = (wlc_dtpc_info_t *)dig->cmd_ctx;
	const wl_dtpc_cfg_t *dtpc_cfg = (const wl_dtpc_cfg_t *)ibuf;
	int ret = BCME_OK;
	uint32 *outval = (uint32 *)obuf;
	UNUSED_PARAMETER(dtpc_cfg);

	*outval = dtpci->active;
	return ret;
}

static int
dtpc_iov_set_en(const bcm_iov_cmd_digest_t *dig,
	const uint8 *ibuf, size_t ilen, uint8 *obuf, size_t *olen)
{
	wlc_dtpc_info_t *dtpci = (wlc_dtpc_info_t *)dig->cmd_ctx;
	const wl_dtpc_cfg_t *dtpc_cfg = (const wl_dtpc_cfg_t *)ibuf;
	int ret = BCME_OK;
	uint32 *input_buf = (uint32 *)obuf;
	UNUSED_PARAMETER(dtpc_cfg);

	if (DTPC_ENAB(dtpci->wlc->pub)) {
		dtpci->active_override = (*input_buf == 1) ? 1 : 2;
	} else {
		ret = BCME_UNSUPPORTED;
	}

#if defined(BCMDBG) || defined(WLTEST)
	if (*input_buf) {
		/* iterate all scb to clean the stats */
		dtpc_clear_logs(dtpci, NULL);
	}
#endif // endif

	return ret;
}

static int
dtpc_iov_get_txsthres(const bcm_iov_cmd_digest_t *dig,
	const uint8 *ibuf, size_t ilen, uint8 *obuf, size_t *olen)
{
	wlc_dtpc_info_t *dtpci = (wlc_dtpc_info_t *)dig->cmd_ctx;
	const wl_dtpc_cfg_t *dtpc_cfg = (const wl_dtpc_cfg_t *)ibuf;
	int ret = BCME_OK;
	uint32 *outval = (uint32 *)obuf;
	UNUSED_PARAMETER(dtpc_cfg);

	*outval = dtpci->thresh;
	return ret;
}

static int
dtpc_iov_set_txsthres(const bcm_iov_cmd_digest_t *dig,
	const uint8 *ibuf, size_t ilen, uint8 *obuf, size_t *olen)
{
	wlc_dtpc_info_t *dtpci = (wlc_dtpc_info_t *)dig->cmd_ctx;
	const wl_dtpc_cfg_t *dtpc_cfg = (const wl_dtpc_cfg_t *)ibuf;
	int ret = BCME_OK;
	uint32 *input_buf = (uint32 *)obuf;
	UNUSED_PARAMETER(dtpc_cfg);

	if (*input_buf != 0 && *input_buf < 1000) {
		dtpci->thresh = (uint16)*input_buf;
		ret = BCME_OK;
	} else {
		ret = BCME_RANGE;
	}
	return ret;
}
static int
dtpc_iov_get_probstep(const bcm_iov_cmd_digest_t *dig,
	const uint8 *ibuf, size_t ilen, uint8 *obuf, size_t *olen)
{
	wlc_dtpc_info_t *dtpci = (wlc_dtpc_info_t *)dig->cmd_ctx;
	const wl_dtpc_cfg_t *dtpc_cfg = (const wl_dtpc_cfg_t *)ibuf;
	int ret = BCME_OK;
	uint32 *outval = (uint32 *)obuf;
	UNUSED_PARAMETER(dtpc_cfg);

	*outval = dtpci->refine_step;
	return ret;
}

static int
dtpc_iov_set_probstep(const bcm_iov_cmd_digest_t *dig,
	const uint8 *ibuf, size_t ilen, uint8 *obuf, size_t *olen)
{
	wlc_dtpc_info_t *dtpci = (wlc_dtpc_info_t *)dig->cmd_ctx;
	const wl_dtpc_cfg_t *dtpc_cfg = (const wl_dtpc_cfg_t *)ibuf;
	int ret = BCME_OK;
	uint32 *input_buf = (uint32 *)obuf;
	UNUSED_PARAMETER(dtpc_cfg);
	if (*input_buf > 8u) {
		ret = BCME_RANGE;
	} else {
		dtpci->refine_step = *input_buf;
		ret = BCME_OK;
	}
	return ret;
}

/** Experimental IOVARs */
static int
dtpc_iov_get_alpha(const bcm_iov_cmd_digest_t *dig,
	const uint8 *ibuf, size_t ilen, uint8 *obuf, size_t *olen)
{
	wlc_dtpc_info_t *dtpci = (wlc_dtpc_info_t *)dig->cmd_ctx;
	const wl_dtpc_cfg_t *dtpc_cfg = (const wl_dtpc_cfg_t *)ibuf;
	int ret = BCME_OK;
	uint32 *outval = (uint32 *)obuf;
	UNUSED_PARAMETER(dtpc_cfg);

	*outval = dtpci->alpha;
	return ret;
}

static int
dtpc_iov_set_alpha(const bcm_iov_cmd_digest_t *dig,
	const uint8 *ibuf, size_t ilen, uint8 *obuf, size_t *olen)
{
	wlc_dtpc_info_t *dtpci = (wlc_dtpc_info_t *)dig->cmd_ctx;
	const wl_dtpc_cfg_t *dtpc_cfg = (const wl_dtpc_cfg_t *)ibuf;
	int ret = BCME_OK;
	uint32 *input_buf = (uint32 *)obuf;
	UNUSED_PARAMETER(dtpc_cfg);

	if (*input_buf == 0 || *input_buf > 8u) {
		/* weighting <0.001 is not practical */
		ret = BCME_RANGE;
	} else {
		dtpci->alpha = (uint8)*input_buf;
		ret = BCME_OK;
	}
	return ret;
}

static int
dtpc_iov_get_trigint(const bcm_iov_cmd_digest_t *dig,
	const uint8 *ibuf, size_t ilen, uint8 *obuf, size_t *olen)
{
	wlc_dtpc_info_t *dtpci = (wlc_dtpc_info_t *)dig->cmd_ctx;
	const wl_dtpc_cfg_t *dtpc_cfg = (const wl_dtpc_cfg_t *)ibuf;
	int ret = BCME_OK;
	uint32 *outval = (uint32 *)obuf;
	UNUSED_PARAMETER(dtpc_cfg);

	*outval = dtpci->trig_intval;
	return ret;
}

static int
dtpc_iov_set_trigint(const bcm_iov_cmd_digest_t *dig,
	const uint8 *ibuf, size_t ilen, uint8 *obuf, size_t *olen)
{
	wlc_dtpc_info_t *dtpci = (wlc_dtpc_info_t *)dig->cmd_ctx;
	const wl_dtpc_cfg_t *dtpc_cfg = (const wl_dtpc_cfg_t *)ibuf;
	int ret = BCME_OK;
	uint32 *input_buf = (uint32 *)obuf;
	UNUSED_PARAMETER(dtpc_cfg);

	/* >10s isn't meaningful */
	if (*input_buf > 0 && *input_buf < 10000u) {
		dtpci->trig_intval = *input_buf;
		ret = BCME_OK;
	} else {
		ret = BCME_RANGE;
	}
	return ret;
}

static int
dtpc_iov_get_headroom(wlc_dtpc_info_t *dtpci, int chanspec_bw, uint num_txchains,
	int32 input, uint8 *output)
{
	dtpc_txpwr_ctxt_t *pwr;
	int ret = BCME_OK;
	ratespec_t rspec;
	uint8 max_txpwroff;
	int8 clm_max_pwr, headroom_pwr;
	phy_tx_targets_per_core_t target_per_core;
	int rspec_bw, rspec_nss, rspec_txexp;
	uint8 extra_backoff_factor;

#if defined(BCMDBG) || defined(WLTEST)
	UNUSED_PARAMETER(extra_backoff_factor);
	UNUSED_PARAMETER(target_per_core);
	UNUSED_PARAMETER(max_txpwroff);
	UNUSED_PARAMETER(headroom_pwr);
#endif // endif
	ASSERT(dtpci != NULL);
	pwr = dtpci->pwrctxt;
	if (pwr == NULL || pwr->clm_max == NULL) {
		WL_ERROR(("wl%d: DTPC power context null\n", WLCWLUNIT(dtpci->wlc)));
		return BCME_ERROR;
	}

	rspec = (ratespec_t)input;
	if (RSPEC_ISHE(rspec) || RSPEC_ISVHT(rspec)) {
		rspec_bw = (rspec & WL_RSPEC_BW_MASK) >> WL_RSPEC_BW_SHIFT;
		rspec_nss = (rspec & WL_RSPEC_HE_NSS_MASK) >> WL_RSPEC_HE_NSS_SHIFT;
		rspec_txexp = (rspec & WL_RSPEC_TXEXP_MASK) >> WL_RSPEC_TXEXP_SHIFT;

		if (rspec_bw > chanspec_bw ||
			num_txchains != (rspec_nss + rspec_txexp)) {
			return BCME_BADARG;
		}
	}

	if ((clm_max_pwr = (int8)get_pwr_from_targets(dtpci->wlc,
		rspec, pwr->clm_max)) == BCME_BADARG) {
		return BCME_BADARG;
	}
#if defined(BCMDBG) || defined(WLTEST)
	if (dtpci->clmlimit_ovr >= 0) {
		/* override max headroom - DVT test script specific */
		*output = dtpci->clmlimit_ovr;
	} else
#endif // endif
	{
	wlc_tpc_get_tx_target_per_core(dtpci->wlc, rspec, &target_per_core);
	/* TODO: Find a correct target_per_core.pwr when txchain != 15 */
	if (clm_max_pwr > target_per_core.pwr[0]) {
		headroom_pwr = clm_max_pwr - target_per_core.pwr[0];
	} else {
		headroom_pwr = 0;
	}

	max_txpwroff = headroom_pwr;
	extra_backoff_factor = dtpci->extra_backoff_factor;
	max_txpwroff = (max_txpwroff > extra_backoff_factor) ?
		max_txpwroff - extra_backoff_factor : 0;
	*output = max_txpwroff;
	}

	return ret;
}

static int
dtpc_iov_get_headroom_all(wlc_dtpc_info_t *dtpci, int chanspec_bw,
	uint num_txchains, void *a)
{
	wl_dtpc_cfg_headroom_t *dtpc_headroom = a;
	int nss, mcs, bw, len, ret;
	ratespec_t rspec;
	uint8 headroom;

	if (num_txchains * 12 * chanspec_bw * 2 > MAX_NUM_KNOWN_RATES) {
		return BCME_NOMEM;
	}

	len = dtpc_headroom->len = 0;
	dtpc_headroom->max_bw = chanspec_bw;
	dtpc_headroom->max_nss = num_txchains;

	/* HE */
	for (nss = 1; nss <= num_txchains; nss++) {
		for (mcs = 0; mcs <= 11; mcs++) {
			for (bw = 1; bw <= chanspec_bw; bw++) {
				rspec = HE_RSPEC(mcs, nss);
				rspec |= WL_RSPEC_TXBF;
				rspec |= (bw << WL_RSPEC_BW_SHIFT);
				rspec |= ((num_txchains-nss) << WL_RSPEC_TXEXP_SHIFT);
				if ((ret = dtpc_iov_get_headroom(dtpci, chanspec_bw,
					num_txchains, rspec, &headroom)) != BCME_OK) {
					return ret;
				}
				dtpc_headroom->rspec[len] = rspec;
				dtpc_headroom->headroom[len++] = headroom;
			}
		}
	}

	/* VHT */
	for (nss = 1; nss <= num_txchains; nss++) {
		for (mcs = 0; mcs <= 11; mcs++) {
			for (bw = 1; bw <= chanspec_bw; bw++) {
				rspec = VHT_RSPEC(mcs, nss);
				rspec |= WL_RSPEC_TXBF;
				rspec |= (bw << WL_RSPEC_BW_SHIFT);
				rspec |= ((num_txchains-nss) << WL_RSPEC_TXEXP_SHIFT);
				if ((ret = dtpc_iov_get_headroom(dtpci, chanspec_bw,
					num_txchains, rspec, &headroom)) != BCME_OK) {
					return ret;
				}
				dtpc_headroom->rspec[len] = rspec;
				dtpc_headroom->headroom[len++] = headroom;
			}
		}
	}
	dtpc_headroom->len = len;
	/* TODO: HT */
	return BCME_OK;
}

/* This function provides immediate power offset update during power probing based on scb input */
static int
dtpc_upd_pwroffset(wlc_info_t *wlc, struct scb *scb,
        ratespec_t rspec)
{
	int ret = BCME_OK;

	/* scb validation check */
	if (SCB_INTERNAL(scb) || ETHER_ISMULTI(&scb->ea) ||
		ETHER_ISNULLADDR(&scb->ea)) {
		/* skip these specific scbs */
		goto exit;
	}

	if (wlc_dtpc_is_pwroffset_changed(wlc->dtpc, scb, rspec)) {
		rcb_t *state;
		state = wlc_scb_ratesel_getrcb(wlc, scb, 0);
		ret = wlc_ratesel_upd_pwroffset(state);
	}

exit:
	return ret;
}
static int
dtpc_iov_get_clmovr(const bcm_iov_cmd_digest_t *dig,
	const uint8 *ibuf, size_t ilen, uint8 *obuf, size_t *olen)
{
	int ret = BCME_OK;
#if defined(BCMDBG) || defined(WLTEST)
	wlc_dtpc_info_t *dtpci = (wlc_dtpc_info_t *)dig->cmd_ctx;
	int32 *outval = (int32 *)obuf;
	*outval = dtpci->clmlimit_ovr;
#else
	ret = BCME_UNSUPPORTED;
#endif // endif
	return ret;
}

static int
dtpc_iov_set_clmovr(const bcm_iov_cmd_digest_t *dig,
	const uint8 *ibuf, size_t ilen, uint8 *obuf, size_t *olen)
{
	int ret = BCME_OK;
#if defined(BCMDBG) || defined(WLTEST)
	wlc_dtpc_info_t *dtpci = (wlc_dtpc_info_t *)dig->cmd_ctx;
	int8 *input_buf = (int8 *)obuf;
	if (*input_buf <= 40 && *input_buf >= -1) {
		dtpci->clmlimit_ovr = *input_buf;
		ret = BCME_OK;
	} else {
		ret = BCME_RANGE;
	}
#else
	ret = BCME_UNSUPPORTED;
#endif // endif
	return ret;
}
#endif /* WLC_DTPC */
