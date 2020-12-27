/*
 * A-MPDU Tx (with extended Block Ack protocol) source file
 * Broadcom 802.11abg Networking Device Driver
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
 * $Id: wlc_ampdu.c 777641 2019-08-07 02:27:22Z $
 */

/**
 * Preprocessor defines used in this file:
 *
 * WLAMPDU:       firmware/driver image supports AMPDU functionality
 * WLAMPDU_UCODE: aggregation by ucode
 * WLAMPDU_AQM:   aggregation by AQM module in d11 core (AC chips only)
 * WLAMPDU_PRECEDENCE: transmit certain traffic earlier than other traffic
 * PSPRETEND:     increase robustness against bad signal conditions by performing resends later
 */

/**
 * @file
 * @brief
 * XXX Twiki: [AmpduUcode] [AmpduAQM]
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>

#ifndef WLAMPDU
#error "WLAMPDU is not defined"
#endif	/* WLAMPDU */
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <802.11.h>
#include <wlioctl.h>
#ifdef WLOVERTHRUSTER        /* Opportunistic Tcp Ack Consolidation */
#include <ethernet.h>
#endif // endif
#include <sbhndpio.h>
#include <sbhnddma.h>
#include <hnddma.h>
#include <bcmdevs.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_phy_hal.h>
#include <wlc_antsel.h>
#include <wlc_scb.h>         /* one SCB represents one remote party */
#include <wlc_frmutil.h>
#include <wlc_p2p.h>
#include <wlc_apps.h>
#ifdef WLTOEHW           /* TCP segmentation / checksum hardware assisted offload. AC \
	chips only. */
#include <wlc_tso.h>
#endif // endif
#include <wlc_ampdu.h>
#include <wlc_ampdu_cmn.h>
#if defined(WLAMSDU) || defined(WLAMSDU_TX)
#include <wlc_amsdu.h>
#endif // endif
#if defined(EVENT_LOG_COMPILE)
#include <event_log.h>
#if defined(ECOUNTERS)
#include <ecounters.h>
#endif // endif
#endif /* EVENT_LOG_COMPILE */
#include <wlc_scb_ratesel.h>
#include <wl_export.h>
#include <wlc_rm.h>

#ifdef PROP_TXSTATUS         /* saves dongle memory by queuing tx packets on the host \
	*/
#include <wlc_wlfc.h>
#endif // endif
#ifdef WLP2P
#include <wlc_p2p.h>
#endif // endif
#include <wlc_pcb.h>
#ifdef WL_MU_TX
#include <wlc_mutx.h>
#endif // endif
#if defined(WLATF) || defined(PKTQ_LOG)                 /* Air Time Fairness */
#include <wlc_airtime.h>
#include <wlc_prot.h>
#include <wlc_prot_g.h>
#include <wlc_prot_n.h>
#endif /* WLATF */
#include <wlc_btcx.h>
#include <wlc_txc.h>
#include <wlc_objregistry.h>
#ifdef WL11AC
#include <wlc_vht.h>
#endif /* WL11AC */
#include <wlc_ht.h>
#include <wlc_he.h>
#if defined(SCB_BS_DATA)     /* band steering */
#include <wlc_bs_data.h>
#endif /* SCB_BS_DATA */
#ifdef WLC_SW_DIVERSITY
#include <wlc_swdiv.h>
#endif /* WLC_SW_DIVERSITY */

/* Enable NON AQM code only when builds require SW agg or Ucode Agg */
/* Dongle 11ac builds will define WLAMPDU_AQM */
#ifndef WLAMPDU_AQM
#define AMPDU_NON_AQM
#endif // endif
#include <wlc_tx.h>
#include <wlc_bsscfg_psq.h>
#include <wlc_txmod.h>
#include <wlc_pspretend.h>
#include <wlc_csrestrict.h>

#if defined(BCMDBG) /* temporary code to catch rare phenomenon using UTF */
/* UTF: 43602 NIC assertion "WLPKTTAG(p)->flags & WLF_AMPDU_MPDU" failed */
#define BCMDBG_SWWLAN_38455
#endif /* BCMDBG */
#include <wlc_macdbg.h>

#include <wlc_txbf.h>
#include <wlc_bmac.h>
#include <wlc_hw.h>
#include <wlc_rspec.h>
#include <wlc_txs.h>
#include <wlc_qoscfg.h>
#include <wlc_perf_utils.h>
#include <wlc_dbg.h>
#include <wlc_pktc.h>
#include <wlc_dump.h>
#include <wlc_monitor.h>
#include <wlc_stf.h>
#include <wlc_assoc.h>
#include <wlc_bmac.h>
#include <phy_api.h>
#ifdef WL11K
#include <wlc_rrm.h>
#endif // endif
#include <wlc_ratelinkmem.h>
#ifdef WLCFP
#include <wlc_cfp.h>
#include <d11_cfg.h>
#include <wlc_cfp_priv.h>
#include <wlfc_proto.h>
#endif // endif
#ifdef WLSQS
#include <wlc_sqs.h>
#endif // endif
#include <wlc_keymgmt.h>
#include <wlc_musched.h>
#include <wlc_hw_priv.h>
#ifdef WLNAR
#include <wlc_nar.h>
#endif /* WLNAR */

#if defined(WLTEST) || defined(WLPKTENG)
#include <wlc_test.h>
#endif // endif

#ifdef WLTAF
#include <wlc_taf.h>
#endif // endif

#ifdef WLAMPDU_PRECEDENCE
#define ampdu_pktqprec_n_pkts(pq, tid) \
	(pktqprec_n_pkts(pq, WLC_PRIO_TO_HI_PREC(tid))\
	+ pktqprec_n_pkts(pq, WLC_PRIO_TO_PREC(tid)))
static bool wlc_ampdu_prec_enq(wlc_info_t *wlc, struct pktq *pq, void *pkt, int tid);
static void * _wlc_ampdu_pktq_pdeq(struct pktq *pq, int tid);
static void wlc_ampdu_pktq_pflush(wlc_info_t *wlc, struct pktq *pq, int tid);
#ifdef PROP_TXSTATUS
static void *ampdu_pktqprec_peek(struct pktq *pq, int tid);
#endif /* PROP_TXSTATUS */
#define wlc_ampdu_pktqlog_cnt(q, tid, prec)	(q)->pktqlog->_prec_cnt[(prec)]
#else
#define wlc_ampdu_prec_enq(wlc, q, pkt, prec)	wlc_prec_enq_head(wlc, q, pkt, prec, FALSE)
#define ampdu_pktqprec_n_pkts			pktqprec_n_pkts
#define _wlc_ampdu_pktq_pdeq			pktq_pdeq
#define wlc_ampdu_pktq_pflush(wlc, pq, prec)	wlc_txq_pktq_pflush(wlc, pq, prec)
#define wlc_ampdu_pktqlog_cnt(q, tid, prec)	(q)->pktqlog->_prec_cnt[(tid)]
#define ampdu_pktqprec_peek			pktqprec_peek
#endif /* WLAMPDU_PRECEDENCE */

#define DEFAULT_ECOUNTER_AMPDU_TX_DUMP_SIZE	3
#define ECOUNTER_AMPDU_TX_WLAMPDU_SIZE	1

#if defined(WL11AC)
#define ECOUNTER_AMPDU_TX_WLAMPDU_WL11AC_SIZE	4
#else
#define ECOUNTER_AMPDU_TX_WLAMPDU_WL11AC_SIZE	0
#endif // endif

#if defined(WL11AC)
#define ECOUNTER_AMPDU_TX_WL11AC_SIZE	2
#else
#define ECOUNTER_AMPDU_TX_WL11AC_SIZE	0
#endif // endif
#define ECOUNTER_AMPDU_TX_DUMP_SIZE	(DEFAULT_ECOUNTER_AMPDU_TX_DUMP_SIZE + \
	ECOUNTER_AMPDU_TX_WLAMPDU_SIZE + ECOUNTER_AMPDU_TX_WLAMPDU_WL11AC_SIZE + \
	ECOUNTER_AMPDU_TX_WL11AC_SIZE)

#if defined(WLAMPDU) && defined(BCMDBG)
extern void wlc_ampdu_txq_prof_enab(void);
extern void wlc_ampdu_txq_prof_print_histogram(int entries);
extern void wlc_ampdu_txq_prof_add_entry(wlc_info_t *wlc, struct scb *scb,
	const char * func, uint32 line);
#define WLC_AMPDU_TXQ_PROF_ADD_ENTRY(wlc, scb) \
	wlc_ampdu_txq_prof_add_entry(wlc, scb, __FUNCTION__, __LINE__);
#else
#define WLC_AMPDU_TXQ_PROF_ADD_ENTRY(wlc, scb)
#endif /* WLAMPDU && BCMDBG */

#define WLAMPDU_PREC_TID2PREC(p, tid) \
	((WLPKTTAG(p)->flags3 & WLF3_FAVORED) ? WLC_PRIO_TO_HI_PREC(tid) : WLC_PRIO_TO_PREC(tid))

static void BCMFASTPATH
wlc_ampdu_dotxstatus_aqm_complete(ampdu_tx_info_t *ampdu_tx, struct scb *scb,
                                  void *p, tx_status_t *txs, wlc_txh_info_t *txh_info);

static void BCMFASTPATH
wlc_frmburst_dotxstatus(ampdu_tx_info_t *ampdu_tx, tx_status_t *txs);

static INLINE
void wlc_ampdu_fill_rs_txs(ratesel_txs_t* rs_txs, tx_status_t *txs, uint8 ac,
	uint16 ncons, uint16 nlost);

#if defined(WL_DATAPATH_LOG_DUMP)
static void wlc_ampdu_datapath_summary(void *ctx, struct scb *scb, int tag);
#endif // endif

#ifdef IQPLAY_DEBUG
static void wlc_set_sequmber(wlc_info_t *wlc, uint16 seq_num);
#endif /* IQPLAY_DEBUG */

#ifdef BCMDBG
static int	_txq_prof_enab;
#endif // endif
#if defined(WLCFP) && defined(WLSQS)
static void wlc_cfp_ampdu_agg_complete(wlc_info_t *wlc, ampdu_tx_info_t *ampdu_tx,
	scb_ampdu_tx_t *scb_ampdu, scb_ampdu_tid_ini_t *ini, bool force_release);
#endif /* WLCFP && WLSQS */
#ifdef BCMDBG
#define WLC_AMPDU_EPOCH_REASON(str, reason) (str = reason)
#else
/* Str is not used if BCMDBG not defined */
#define WLC_AMPDU_EPOCH_REASON(str, reason)
#endif // endif

#ifdef WLATF
/* Default AMPDU txq time allowance */
#define AMPDU_TXQ_TIME_ALLOWANCE_US	4000
#define AMPDU_TXQ_TIME_MIN_ALLOWANCE_US	1000

/* Time elapse without any packet */
#define ATM_IDLE_TIME_USEC		350000
#define ATM_IDLE_TIME_SEC		2

/* Minimum retry threshold after which we will compensate for the retries */
#define AMPDU_RETRY_COMPENSATE_THRESH	10

/* Low water mark packet count threshold for ampdu release per scb per tid */
#define AMPDU_ATF_LOWAT_REL_CNT		24
/* Low water mark threshold over aggregate packet count for ampdu release */
#define AMPDU_ATF_AGG_LOWAT_REL_CNT	256

/* Note: Structure members are referred directly to reduce the overhead as these macros are used
 * in the per-packet part of the datapath
 */

#define AMPDU_ATF_STATE(ini) (&(ini)->atf_state)
#define AMPDU_ATF_ENABLED(ini) ((ini)->atf_state.atf != 0)
#ifdef WLCNT
#define AMPDU_ATF_STATS(ini) (&(ini)->atf_state.atf_stats)
#else
#define AMPDU_ATF_STATS(ini) 0 /* This will force a kernel panic if WLCNT has not been defined */
#endif /* WLCNT */
#endif /* WLATF */

#ifdef WLWRR
#define AMPDU_WRR_STATS(ini) (&(ini)->wrr_stats)
#else
#define AMPDU_WRR_STATS(ini) 0 /* This will force a kernel panic if WLCNT has not been defined */
#endif /* WRR */

#ifndef AMPDU_DYNFB_RTSPER_OFF
#define AMPDU_DYNFB_RTSPER_OFF 30 /* Frameburst disable RTS PER threshold (percent) */
#endif // endif
#ifndef AMPDU_DYNFB_RTSPER_ON
#define AMPDU_DYNFB_RTSPER_ON  20 /* Frameburst enable RTS PER threshold (percent) */
#endif // endif
#ifndef AMPDU_DYNFB_MINRTSCNT
#define AMPDU_DYNFB_MINRTSCNT  80 /* Min number of Tx RTS after which RTS PER is calculated */
#endif // endif

#define WAR_MIN_NUM_DLIM_PADDING	12 /* min dlim pad to prevent tx uflow */

/* For rev128, mindur is configured in unit 1/4 of usec, 2^(mpdu_density-1) */
#define MPDU_DENSITY_REV128(mpdu_density)	(1 << ((mpdu_density) - 1))

#ifdef WLCFP
#define WLC_AMPDU_AGG_COMPLETE(wlc, ampdu_tx, scb, scb_ampdu, ini, force)		\
({											\
	if (CFP_ENAB(wlc->pub) == TRUE) {						\
		wlc_cfp_tcb_upd_pause_state(wlc, scb, TRUE);				\
		wlc_cfp_ampdu_agg_complete(wlc, ampdu_tx, scb_ampdu, ini, force);	\
		wlc_cfp_tcb_upd_pause_state(wlc, scb, FALSE);				\
	} else {									\
		wlc_ampdu_agg_complete(wlc, ampdu_tx, scb_ampdu, ini, force);		\
	}										\
})
#else /* !WLCFP */
#define WLC_AMPDU_AGG_COMPLETE(wlc, ampdu_tx, scb, scb_ampdu, ini, force) \
({\
	wlc_ampdu_agg_complete(wlc, ampdu_tx, scb_ampdu, ini, force); \
})
#endif /* WLCFP */

/** iovar table */
enum wlc_ampdu_iov {
	IOV_AMPDU_TX = 1,		/* enable/disable ampdu tx */
	IOV_AMPDU_TID = 2,		/* enable/disable per-tid ampdu */
	IOV_AMPDU_TX_DENSITY = 3,	/* ampdu density */
	IOV_AMPDU_SEND_ADDBA = 4,	/* send addba req to specified scb and tid */
	IOV_AMPDU_SEND_DELBA = 5,	/* send delba req to specified scb and tid */
	IOV_AMPDU_MANUAL_MODE = 6,	/* addba tx to be driven by cmd line */
	IOV_AMPDU_NO_BAR = 7,		/* do not send bars */
	IOV_AMPDU_MPDU = 8,		/* max number of mpdus in an ampdu */
	IOV_AMPDU_DUR = 9,		/* max duration of an ampdu (in usec) */
	IOV_AMPDU_RTS = 10,		/* use rts for ampdu */
	IOV_AMPDU_TX_BA_WSIZE = 11,	/* ampdu TX ba window size */
	IOV_AMPDU_RETRY_LIMIT = 12,	/* ampdu retry limit */
	IOV_AMPDU_RR_RETRY_LIMIT = 13,	/* regular rate ampdu retry limit */
	IOV_AMPDU_TX_LOWAT = 14,	/* ampdu tx low wm (watermark) */
	IOV_AMPDU_HIAGG_MODE = 15,	/* agg mpdus with diff retry cnt */
	IOV_AMPDU_PROBE_MPDU = 16,	/* number of mpdus/ampdu for rate probe frames */
	IOV_AMPDU_TXPKT_WEIGHT = 17,	/* weight of ampdu in the txfifo; helps rate lag */
	IOV_AMPDU_FFPLD_RSVD = 18,	/* bytes to reserve in pre-loading info */
	IOV_AMPDU_FFPLD = 19,		/* to display pre-loading info */
	IOV_AMPDU_MAX_TXFUNFL = 20,	/* inverse of acceptable proportion of tx fifo underflows */
	IOV_AMPDU_RETRY_LIMIT_TID = 21,	/* ampdu retry limit per-tid */
	IOV_AMPDU_RR_RETRY_LIMIT_TID = 22,	/* regular rate ampdu retry limit per-tid */
	IOV_AMPDU_MFBR = 23,		/* Use multiple fallback rate */
	IOV_AMPDU_TCP_ACK_RATIO = 24, 	/* max number of ack to suppress in a row. 0 disable */
	IOV_AMPDU_TCP_ACK_RATIO_DEPTH = 25,	/* max number of multi-ack. 0 disable */
	IOV_AMPDUMAC = 26,		/* Use ucode or hw assisted agregation */
	IOV_AMPDU_AGGMODE = 27,		/* agregation mode: HOST or MAC */
	/* 28 free */
	IOV_AMPDU_TXFAIL_EVENT = 29,	/* Tx failure event to the host */
	IOV_AMPDU_FRAMEBURST_OVR = 30,	/* Override framebursting in absense of ampdu */
	IOV_AMPDU_TXQ_PROFILING_START = 31,	/* start sampling TXQ profiling data */
	IOV_AMPDU_TXQ_PROFILING_DUMP = 32,	/* dump TXQ histogram */
	IOV_AMPDU_TXQ_PROFILING_SNAPSHOT = 33,	/* take a snapshot of TXQ histogram */
	IOV_AMPDU_RELEASE = 34,		/* max # of mpdus released at a time */
	IOV_AMPDU_ATF_US = 35,
	IOV_AMPDU_ATF_MIN_US = 36,
	IOV_AMPDU_CS_PKTRETRY = 37,
	IOV_AMPDU_TXAGGR = 38,
	IOV_AMPDU_ADDBA_TIMEOUT = 39,
	IOV_AMPDU_SET_SEQNUMBER = 40,
	IOV_DYNFB_RTSPER_ON = 41,
	IOV_DYNFB_RTSPER_OFF = 42,
	IOV_AMPDU_ATF_LOWAT = 43,
	IOV_AMPDU_LAST
};

static const bcm_iovar_t ampdu_iovars[] = {
	{"ampdu_tx", IOV_AMPDU_TX, (IOVF_SET_DOWN|IOVF_RSDB_SET), 0, IOVT_BOOL, 0},
	{"ampdu_tid", IOV_AMPDU_TID, (0), 0, IOVT_BUFFER, sizeof(struct ampdu_tid_control)},
	{"ampdu_tx_density", IOV_AMPDU_TX_DENSITY, (0), 0, IOVT_UINT8, 0},
	{"ampdu_send_addba", IOV_AMPDU_SEND_ADDBA, (0), 0, IOVT_BUFFER,
	sizeof(struct ampdu_ea_tid)},
	{"ampdu_send_delba", IOV_AMPDU_SEND_DELBA, (0), 0, IOVT_BUFFER,
	sizeof(struct ampdu_ea_tid)},
	{"ampdu_manual_mode", IOV_AMPDU_MANUAL_MODE, (IOVF_SET_DOWN), 0, IOVT_BOOL, 0},
	{"ampdu_mpdu", IOV_AMPDU_MPDU, (0), 0, IOVT_INT8, 0}, /* need to mark IOVF2_RSDB */
#ifdef WLOVERTHRUSTER
	{"ack_ratio", IOV_AMPDU_TCP_ACK_RATIO, (0), 0, IOVT_UINT8, 0},
	{"ack_ratio_depth", IOV_AMPDU_TCP_ACK_RATIO_DEPTH, (0), 0, IOVT_UINT8, 0},
#endif /* WLOVERTHRUSTER */
	{"ampdumac", IOV_AMPDUMAC, (0), 0, IOVT_UINT8, 0},
	{"ampdu_aggmode", IOV_AMPDU_AGGMODE, (IOVF_SET_DOWN|IOVF_RSDB_SET), 0, IOVT_INT8, 0},
	{"frameburst_override", IOV_AMPDU_FRAMEBURST_OVR, (0), 0, IOVT_BOOL, 0},
	{"ampdu_txq_prof_start", IOV_AMPDU_TXQ_PROFILING_START, (0), 0, IOVT_VOID, 0},
	{"ampdu_txq_prof_dump", IOV_AMPDU_TXQ_PROFILING_DUMP, (0), 0, IOVT_VOID, 0},
	{"ampdu_txq_ss", IOV_AMPDU_TXQ_PROFILING_SNAPSHOT, (0), 0, IOVT_VOID, 0},
	{"ampdu_release", IOV_AMPDU_RELEASE, (0), 0, IOVT_UINT8, 0},
#ifdef WLATF
	{"ampdu_atf_us", IOV_AMPDU_ATF_US, (IOVF_NTRL), 0, IOVT_UINT32, 0},
	{"ampdu_atf_min_us", IOV_AMPDU_ATF_MIN_US, (IOVF_NTRL), 0, IOVT_UINT32, 0},
#if defined(BCMDBG) || defined(BCMDBG_AMPDU)
	{"ampdu_atf_lowat", IOV_AMPDU_ATF_LOWAT, (IOVF_NTRL), 0, IOVT_UINT32, 0},
#endif /* defined(BCMDBG) || defined(BCMDBG_AMPDU) */
#endif /* WLATF */
	{"ampdu_txaggr", IOV_AMPDU_TXAGGR, IOVF_BSS_SET_DOWN, 0, IOVT_BUFFER,
	sizeof(struct ampdu_aggr)},
	{"ampdu_addba_to", IOV_AMPDU_ADDBA_TIMEOUT, (0), 0, IOVT_UINT32, 0},
#ifdef AMPDU_NON_AQM
#if defined(BCMDBG) || defined(WL_EXPORT_AMPDU_RETRY)
	{"ampdu_retry_limit_tid", IOV_AMPDU_RETRY_LIMIT_TID, (0), 0, IOVT_BUFFER,
	sizeof(struct ampdu_retry_tid)},
	{"ampdu_rr_retry_limit_tid", IOV_AMPDU_RR_RETRY_LIMIT_TID, (0), 0, IOVT_BUFFER,
	sizeof(struct ampdu_retry_tid)},
#endif /* defined(BCMDBG) || defined (WL_EXPORT_AMPDU_RETRY) */
#ifdef BCMDBG
	{"ampdu_ffpld", IOV_AMPDU_FFPLD, (0), 0, IOVT_UINT32, 0},
#endif // endif
#endif /* AMPDU_NON_AQM */
#ifdef IQPLAY_DEBUG
	{"set_seqnumber", IOV_AMPDU_SET_SEQNUMBER,
	(IOVF_OPEN_ALLOW), 0, IOVT_BUFFER, sizeof(uint32),
	},
#endif /* IQPLAY_DEBUG */
#if (defined(BCMDBG) || defined(TUNE_FBOVERRIDE))
	{"dyn_fb_rtsper_on", IOV_DYNFB_RTSPER_ON, (0), 0, IOVT_UINT8, 0},
	{"dyn_fb_rtsper_off", IOV_DYNFB_RTSPER_OFF, (0), 0, IOVT_UINT8, 0},
#endif // endif
	{NULL, 0, 0, 0, 0, 0}
};

#define AMPDU_DEF_PROBE_MPDU	2		/**< def number of mpdus in a rate probe ampdu */

#ifndef AMPDU_TX_BA_MAX_WSIZE
#define AMPDU_TX_BA_MAX_WSIZE	64		/**< max Tx ba window size (in pdu) */
#endif /* AMPDU_TX_BA_MAX_WSIZE */
#ifndef AMPDU_TX_BA_DEF_WSIZE
#define AMPDU_TX_BA_DEF_WSIZE	64		/**< default Tx ba window size (in pdu) */
#endif /* AMPDU_TX_BA_DEF_WSIZE */

#define	AMPDU_BA_BITS_IN_BITMAP	64		/**< number of bits in bitmap */
#define AMPDU_MAX_RETRY_LIMIT	32		/**< max tx retry limit */
#define AMPDU_DEF_RETRY_LIMIT	5		/**< default tx retry limit */
#define AMPDU_DEF_RR_RETRY_LIMIT	2	/**< default tx retry limit at reg rate */
#define AMPDU_DEF_TX_LOWAT	1		/**< default low transmit wm (water mark) */
#define AMPDU_DEF_TXPKT_WEIGHT	2		/**< default weight of ampdu in txfifo */
#define AMPDU_DEF_FFPLD_RSVD	2048		/**< default ffpld reserved bytes */
#define AMPDU_MIN_FFPLD_RSVD	512		/**< minimum ffpld reserved bytes */
#define AMPDU_BAR_RETRY_CNT	50		/**< # of bar retries before delba */
#define AMPDU_ADDBA_REQ_RETRY_CNT	4	/**< # of addbareq retries before delba */
#define AMPDU_INI_OFF_TIMEOUT	60		/**< # of sec in off state */
#define	AMPDU_SCB_MAX_RELEASE	32		/**< max # of mpdus released at a time */
#ifndef AMPDU_SCB_MAX_RELEASE_AQM
#define	AMPDU_SCB_MAX_RELEASE_AQM	64	/**< max # of mpdus released at a time */
#endif /* AMPDU_SCB_MAX_RELEASE_AQM */
#define AMPDU_ADDBA_REQ_RETRY_TIMEOUT	200	/* ADDBA retry timeout msecs */
#define AMPDU_MIN_DUR_IDX_16us	0x7		/**< min dur index 2 ^ (idx - 1) if idx > 0 */
#define AMPDU_DEF_MAX_RX_FACTOR	0x0		/**< max ampdu len exponent */

#define AMPDU_INI_DEAD_TIMEOUT		2	/**< # of sec without ini progress */
#define AMPDU_INI_CLEANUP_TIMEOUT	2	/**< # of sec in pending off state */
#define AMPDU_INI_TWT_DEAD_TIMEOUT	30	/**< # of sec without ini progress with TWT */

#define AMPDU_TXNOPROG_LIMIT		10	/**< max # of sec with any ini in pending off state
						 **< while ucode is consuming no pkts before
						 **< we dump more debugging messages
						 */

/* internal BA states */
#define	AMPDU_TID_STATE_BA_OFF		0x00	/**< block ack OFF for tid */
#define	AMPDU_TID_STATE_BA_ON		0x01	/**< block ack ON for tid */
#define	AMPDU_TID_STATE_BA_PENDING_ON	0x02	/**< block ack pending ON for tid */
#define	AMPDU_TID_STATE_BA_PENDING_OFF	0x03	/**< block ack pending OFF for tid */

#define NUM_FFPLD_FIFO 4                        /**< number of fifo concerned by pre-loading */
#define FFPLD_TX_MAX_UNFL   200                 /**< default value of the average number of ampdu
						 * without underflows
						 */
#define FFPLD_MPDU_SIZE 1800                    /**< estimate of maximum mpdu size */
#define FFPLD_PLD_INCR 1000                     /**< increments in bytes */
#define FFPLD_MAX_AMPDU_CNT 5000                /**< maximum number of ampdu we
						 * accumulate between resets.
						 */
/* retry BAR in watchdog reason codes */
#define AMPDU_R_BAR_DISABLED		0	/**< disabled retry BAR in watchdog */
#define AMPDU_R_BAR_NO_BUFFER		1	/**< retry BAR due to out of buffer */
#define AMPDU_R_BAR_CALLBACK_FAIL	2	/**< retry BAR due to callback register fail */
#define AMPDU_R_BAR_HALF_RETRY_CNT	3	/**< retry BAR due to reach to half
						 * AMPDU_BAR_RETRY_CNT
						 */
#define AMPDU_R_BAR_BLOCKED		4	/**< retry BAR due to blocked data fifo */

#define AMPDU_DEF_TCP_ACK_RATIO		2	/**< default TCP ACK RATIO */
#define AMPDU_DEF_TCP_ACK_RATIO_DEPTH	0	/**< default TCP ACK RATIO DEPTH */

#ifndef AMPDU_SCB_DRAIN_CNT
#define AMPDU_SCB_DRAIN_CNT	8
#endif // endif

/* maximum number of frames can be released to HW (in-transit limit) */
#define AMPDU_AQM_RELEASE_DEFAULT	256
#define AMPDU_AQM_RELEASE_MIN		32

#define	BTCX_AMPDU_MAX_DUR		2500	/* max dur of tx ampdu with coex
						 * profile (in usec)
						 */

#define SEQNUM_CHK	0xFFFF

/**
 * Helper to determine d11 seq number wrapping. Returns FALSE if 'a' has wrapped
 * but 'b' not yet. Note that the assumption is that 'a' and 'b' are never more
 * than (SEQNUM_MAX / 2) sequences apart.
 */
#define IS_SEQ_ADVANCED(a, b) \
	(MODSUB_POW2((a), (b), SEQNUM_MAX) < SEQNUM_MAX / 2)

int wl_ampdu_drain_cnt = AMPDU_SCB_DRAIN_CNT;

/* useful macros */
#define NEXT_SEQ(seq) MODINC_POW2((seq), SEQNUM_MAX)
#define NEXT_TX_INDEX(index) MODINC_POW2((index), (ampdu_tx->config->ba_max_tx_wsize))
#define PREV_TX_INDEX(index) MODDEC_POW2((index), (ampdu_tx->config->ba_max_tx_wsize))
#define TX_SEQ_TO_INDEX(seq) ((seq) & ((ampdu_tx->config->ba_max_tx_wsize) - 1))

/* max possible overhead per mpdu in the ampdu; 3 is for roundup if needed */
#define AMPDU_MAX_MPDU_OVERHEAD (DOT11_FCS_LEN + DOT11_ICV_AES_LEN + AMPDU_DELIMITER_LEN + 3 \
	+ DOT11_A4_HDR_LEN + DOT11_QOS_LEN + DOT11_IV_MAX_LEN)

/** ampdu related transmit stats */
typedef struct wlc_ampdu_cnt {
	/* initiator stat counters */
	uint32 txampdu;		/**< ampdus sent */
#ifdef WLCNT
	uint32 txmpdu;		/**< mpdus sent */
	uint32 txregmpdu;	/**< regular(non-ampdu) mpdus sent */
	union {
		uint32 txs;		/**< MAC agg: txstatus received */
		uint32 txretry_mpdu;	/**< retransmitted mpdus */
	} u0;
	uint32 txretry_ampdu;	/**< retransmitted ampdus */
	uint32 txfifofull;	/**< release ampdu due to insufficient tx descriptors */
	uint32 txfbr_mpdu;	/**< retransmitted mpdus at fallback rate */
	uint32 txfbr_ampdu;	/**< retransmitted ampdus at fallback rate */
	union {
		uint32 txampdu_sgi;	/**< ampdus sent with sgi */
		uint32 txmpdu_sgi;	/**< ucode agg: mpdus sent with sgi */
	} u1;
	union {
		uint32 txampdu_stbc;	/**< ampdus sent with stbc */
		uint32 txmpdu_stbc;	/**< ucode agg: mpdus sent with stbc */
	} u2;
	uint32 txampdu_mfbr_stbc; /**< ampdus sent at mfbr with stbc */
	uint32 txrel_wm;	/**< mpdus released due to lookahead wm (water mark) */
	uint32 txrel_size;	/**< mpdus released due to max ampdu size (in mpdu's) */
	uint32 sduretry;	/**< sdus retry returned by sendsdu() */
	uint32 sdurejected;	/**< sdus rejected by sendsdu() */
	uint32 txdrop;		/**< dropped packets */
	uint32 txr0hole;	/**< lost packet between scb and sendampdu */
	uint32 txrnhole;	/**< lost retried pkt */
	uint32 txrlag;		/**< laggard pkt (was considered lost) */
	uint32 txreg_noack;	/**< no ack for regular(non-ampdu) mpdus sent */
	uint32 txaddbareq;	/**< addba req sent */
	uint32 rxaddbaresp;	/**< addba resp recd */
	uint32 txlost;		/**< lost packets reported in txs */
	uint32 txbar;		/**< bar sent */
	uint32 rxba;		/**< ba recd */
	uint32 noba;            /**< ba missing */
	uint32 txstuck;		/**< watchdog bailout for stuck state */
	uint32 orphan;		/**< orphan pkts where scb/ini has been cleaned */

	uint32 epochdelta;	/**< How many times epoch has changed */
	uint32 echgr1;          /**< epoch change reason -- plcp */
	uint32 echgr2;          /**< epoch change reason -- rate_probe */
	uint32 echgr3;          /**< epoch change reason -- a-mpdu as regmpdu */
	uint32 echgr4;          /**< epoch change reason -- regmpdu */
	uint32 echgr5;          /**< epoch change reason -- dest/tid */
	uint32 echgr6;          /**< epoch change reason -- seq no */
	uint32 echgr7;          /**< epoch change reason -- htc+ */
#ifdef WLTAF
	uint32 echgr8;          /**< epoch change reason -- TAF star marker */
#endif // endif
	uint32 tx_mrt, tx_fbr;  /**< number of MPDU tx at main/fallback rates */
	uint32 txsucc_mrt;      /**< number of successful MPDU tx at main rate */
	uint32 txsucc_fbr;      /**< number of successful MPDU tx at fallback rate */
	uint32 enq;             /**< totally enqueued into aggfifo */
	uint32 cons;            /**< totally reported in txstatus */
	uint32 pending;         /**< number of entries currently in aggfifo or txsq */

	/* general: both initiator and responder */
	uint32 rxunexp;		/**< unexpected packets */
	uint32 txdelba;		/**< delba sent */
	uint32 rxdelba;		/**< delba recd */

	uint32 ampdu_wds;       /**< AMPDU watchdogs */

#ifdef WLPKTDLYSTAT
	/* PER (per mcs) statistics */
	uint32 txmpdu_cnt[AMPDU_HT_MCS_ARRAY_SIZE];		/**< MPDUs per mcs */
	uint32 txmpdu_succ_cnt[AMPDU_HT_MCS_ARRAY_SIZE];	/**< acked MPDUs per MCS */
#ifdef WL11AC
	uint32 txmpdu_vht_cnt[AMPDU_MAX_VHT];			/**< MPDUs per vht */
	uint32 txmpdu_vht_succ_cnt[AMPDU_MAX_VHT];		 /**< acked MPDUs per vht */
#endif /* WL11AC */
#ifdef WL11AX
	uint32 txmpdu_he_cnt[AMPDU_MAX_HE];			/**< MPDUs per HE */
	uint32 txmpdu_he_succ_cnt[AMPDU_MAX_HE];		/**< acked MPDUs per HE */
#endif /* WL11AX */
#endif /* WLPKTDLYSTAT */

#ifdef WL_CS_PKTRETRY
	uint32 cs_pktretry_cnt;
#endif // endif
	uint32	txampdubyte_h;		/* tx ampdu data bytes */
	uint32	txampdubyte_l;
#endif /* WLCNT */
} wlc_ampdu_tx_cnt_t;

#ifdef AMPDU_NON_AQM
/**
 * structure to hold tx fifo information and pre-loading state counters specific to tx underflows of
 * ampdus. Some counters might be redundant with the ones in wlc or ampdu structures.
 * This allows to maintain a specific state independently of how often and/or when the wlc counters
 * are updated.
 */
typedef struct wlc_fifo_info {
	uint16 ampdu_pld_size;	/**< number of bytes to be pre-loaded */
	uint8 mcs2ampdu_table[AMPDU_HT_MCS_ARRAY_SIZE]; /**< per-mcs max # of mpdus in an ampdu */
	uint16 prev_txfunfl;	/**< num of underflows last read from the HW macstats counter */
	uint32 accum_txfunfl;	/**< num of underflows since we modified pld params */
	uint32 accum_txampdu;	/**< num of tx ampdu since we modified pld params  */
	uint32 prev_txampdu;	/**< previous reading of tx ampdu */
	uint32 dmaxferrate;	/**< estimated dma avg xfer rate in kbits/sec */
} wlc_fifo_info_t;
#endif /* AMPDU_NON_AQM */

/** this struct is not used in case of host aggregation */
typedef struct ampdumac_info {
#ifdef WLAMPDU_UCODE
	uint16 txfs_addr_strt;
	uint16 txfs_addr_end;
	uint16 txfs_wptr;
	uint16 txfs_rptr;
	uint16 txfs_wptr_addr;
	uint8  txfs_wsize;
#endif // endif
	uint8 epoch;
	bool change_epoch;
	/* any change of following elements will change epoch */
	struct scb *prev_scb;
	uint8 prev_tid;
	uint8 prev_ft;		/**< eg AMPDU_11VHT */
	uint16 prev_txphyctl0, prev_txphyctl1;
	/* To keep ordering consistent with pre-rev40 prev_plcp[] use,
	 * plcp to prev_plcp mapping is not straightforward
	 *
	 * prev_plcp[0] holds plcp[0] (all revs)
	 * prev_plcp[1] holds plcp[3] (all revs)
	 * prev_plcp[2] holds plcp[2] (rev >= 40)
	 * prev_plcp[3] holds plcp[1] (rev >= 40)
	 * prev_plcp[4] holds plcp[4] (rev >= 40)
	 */
	uint8 prev_plcp[5];

	/* stats */
	int in_queue;
	uint8 depth;
	uint8 prev_shdr;
	uint8 prev_cache_gen;		/* Previous cache gen number */
	bool prev_htc;
} ampdumac_info_t;

#ifdef WLOVERTHRUSTER
typedef struct wlc_tcp_ack_info {
	uint8 tcp_ack_ratio;
	uint32 tcp_ack_total;
	uint32 tcp_ack_dequeued;
	uint32 tcp_ack_multi_dequeued;
	uint32 current_dequeued;
	uint8 tcp_ack_ratio_depth;
} wlc_tcp_ack_info_t;
#endif // endif

typedef struct {
	uint32 mpdu_histogram[AMPDU_MAX_MPDU+1];	/**< mpdus per ampdu histogram */
	/* reason for suppressed err code as reported by ucode/aqm, see enum 'TX_STATUS_SUPR...' */
	uint32 supr_reason[NUM_TX_STATUS_SUPR];

	/* txmcs in sw agg is ampdu cnt, and is mpdu cnt for mac agg */
	uint32 txmcs[AMPDU_HT_MCS_ARRAY_SIZE];		/**< mcs of tx pkts */
	uint32 txmcssgi[AMPDU_HT_MCS_ARRAY_SIZE];	/**< mcs of tx pkts */
	uint32 txmcsstbc[AMPDU_HT_MCS_ARRAY_SIZE];	/**< mcs of tx pkts */

	/* used by aqm_agg to get PER */
	uint32 txmcs_succ[AMPDU_HT_MCS_ARRAY_SIZE];	/**< succ mpdus tx per mcs */

#ifdef WL11AC
	uint32 txvht[AMPDU_MAX_VHT];			/**< vht of tx pkts */
	uint32 txvhtsgi[AMPDU_MAX_VHT];			/**< vht of tx pkts */
	uint32 txvhtstbc[AMPDU_MAX_VHT];		/**< vht of tx pkts */

	/* used by aqm_agg to get PER */
	uint32 txvht_succ[AMPDU_MAX_VHT];		/**< succ mpdus tx per vht */
#endif /* WL11AC */
#ifdef WL11AX
	uint32 txhe[AMPDU_MAX_HE_GI][AMPDU_MAX_HE];	/**< HE TX pkt count per GI */
	uint32 txhestbc[AMPDU_MAX_HE];

	/* used by aqm_agg to get PER */
	uint32 txhe_succ[AMPDU_MAX_HE_GI][AMPDU_MAX_HE]; /**< succ mpdus tx per he */
#endif /* WL11AX */

	uint32 txmpdu[AMPDU_PPDU_FT_MAX];	/**< tot mpdus tx per frame type */
	uint32 txmpdu_succ[AMPDU_PPDU_FT_MAX];	/**< succ mpdus tx per frame type */

#ifdef AMPDU_NON_AQM
	uint32 retry_histogram[AMPDU_MAX_MPDU+1];	/**< histogram for retried pkts */
	uint32 end_histogram[AMPDU_MAX_MPDU+1];		/**< errs till end of ampdu */

#ifdef WLAMPDU_UCODE
	uint32 schan_depth_histo[AMPDU_MAX_MPDU+1];	/**< side channel depth */
#endif /* WLAMPDU_UCODE */
#endif /* AMPDU_NON_AQM */
} ampdu_dbg_t;

typedef struct {
	uint16 txallfrm;
	uint16 txbcn;
	uint16 txrts;
	uint16 rxcts;
	uint16 rsptmout;
	uint16 rxstrt;
	uint32 txop;
} mac_dbg_t;

/** ampdu config info, mostly config information that is common across WLC */
typedef struct ampdu_tx_config {
	bool   manual_mode;	        /**< addba tx to be driven by user */
	bool   no_bar;                  /**< do not send bar on failure */
	uint8  ini_enable[AMPDU_MAX_SCB_TID]; /**< per-tid initiator enable/disable of ampdu */
	uint8  ba_policy;               /**< ba policy; immediate vs delayed */
	uint8  ba_max_tx_wsize;         /**< Max Tx ba window size (in pdu) used at attach time */
	uint8  ba_tx_wsize;             /**< Tx ba window size (in pdu) (up to ba_max_tx_wsize) */
	int8   max_pdu;                 /**< max pdus allowed in ampdu (up to ba_tx_wsize) */
	uint8  probe_mpdu;              /**< max mpdus allowed in ampdu for probe pkts */
	uint8  mpdu_density;            /**< min mpdu spacing (0-7) ==> 2^(x-1)/8 usec */
	uint16 dur;                     /**< max duration of an ampdu (in usec) */
	uint16 addba_retry_timeout;     /* Retry timeout for addba requests (ms) */
	uint8  delba_timeout;           /**< timeout after which to send delba (sec) */

	int8   ampdu_aggmode;           /**< aggregation mode, HOST or MAC */
	int8   default_pdu;             /**< default pdus allowed in ampdu */
	bool   fb_override;		/* override for frame bursting */
	bool   fb_override_enable;      /**< configuration to enable/disable ampd_no_frameburst */
	bool   btcx_dur_flag;           /* TRUE if BTCOEX needs TX-AMPDU clamped to 2.5ms */

#ifdef WLATF
	uint  txq_time_allowance_us;
	uint  txq_time_min_allowance_us;
	/* AMPDU atf low water mark release count threshold */
	uint	ampdu_atf_lowat_rel_cnt;
#endif /* WLATF */

	uint8 release;          /**< # of mpdus released at a time */
	uint8 tx_rel_lowat;     /**< low watermark for num of pkts in transit */
	uint8 txpkt_weight;     /**< weight of ampdu in txfifo; reduces rate lag */

#ifdef AMPDU_NON_AQM
	uint8 hiagg_mode;	/**< agg mpdus with different retry cnt */
	uint8 retry_limit;	/**< mpdu transmit retry limit */
	uint8 rr_retry_limit;	/**< mpdu transmit retry limit at regular rate */
	uint8 retry_limit_tid[AMPDU_MAX_SCB_TID];	/**< per-tid mpdu transmit retry limit */
	/* per-tid mpdu transmit retry limit at regular rate */
	uint8 rr_retry_limit_tid[AMPDU_MAX_SCB_TID];

	uint32 ffpld_rsvd;	/**< number of bytes to reserve for preload */
#if defined(WLPROPRIETARY_11N_RATES)
	uint32 max_txlen[AMPDU_HT_MCS_ARRAY_SIZE][2][2]; /**< max size of ampdu per [mcs,bw,sgi] */
#else
	uint32 max_txlen[MCS_TABLE_SIZE][2][2];
#endif /* WLPROPRIETARY_11N_RATES */

	bool mfbr;		/**< enable multiple fallback rate */
	uint32 tx_max_funl;     /**< underflows should be kept such that
	                         * (tx_max_funfl*underflows) < tx frames
	                         */
	wlc_fifo_info_t fifo_tb[NUM_FFPLD_FIFO]; /**< table of fifo infos  */

	uint8  aggfifo_depth;   /**< soft capability of AGGFIFO */
#endif /* non-AQM */
	/* dynamic frameburst variables */
	uint8  dyn_fb_rtsper_on;
	uint8  dyn_fb_rtsper_off;
	uint32 dyn_fb_minrtscnt;

} ampdu_tx_config_t;

/* DBG and counters are replicated per WLC */
/** AMPDU tx module specific state */
struct ampdu_tx_info {
	wlc_info_t *wlc;	/**< pointer to main wlc structure */
	int scb_handle;		/**< scb cubby handle to retrieve data from scb (=remote party) */
#ifdef WLCNT
	wlc_ampdu_tx_cnt_t *cnt;	/**< counters/stats */
#endif // endif
	ampdumac_info_t hwagg[NFIFO_EXT_MAX]; /**< not used in case of host aggregation */
	ampdu_dbg_t *amdbg;
	mac_dbg_t *mdbg;
#ifdef WLOVERTHRUSTER
	wlc_tcp_ack_info_t tcp_ack_info;  /**< stores a mix of config & runtime */
#endif // endif
	bool    txaggr_support;         /**< Support ampdu tx aggregation */
	uint8   cubby_name_id;          /**< cubby ID */
	uint16  aqm_max_release[AMPDU_MAX_SCB_TID];
	struct ampdu_tx_config *config;
	int     bsscfg_handle;          /**< BSSCFG cubby offset */
	/* dynamic frameburst variables */
	uint32 rtstxcnt;	/* # rts sent */
	uint32 ctsrxcnt;	/* # cts received */
	uint8  rtsper_avg;	/* avg rtsper for stats */
	uint8 txnoprog_cnt;		/**< # of sec having no pkt consumed while PENDING_OFF */
	bool cfp_head_frame;		/* Indicate head frame in a chain of packets */
#if defined(RAVG_HISTORY) || defined(RAVG_SIMPLE)
	uint32 ravg_algo;	/* Running Average algorithm: None, Simple, History */
#endif // endif
#ifdef WLATF
	uint32	tx_intransit;		/**< over *all* remote parties */
#endif /* WLATF */
};

/**
 * @brief a structure to hold parameters for packet tx time calculations
 *
 * This structure holds parameters for the time estimate calculation of an MPDU in an AQM A-MPDU.
 * This allows less work per-packet to calculation tx time.
 */
typedef struct ampdu_aqm_timecalc {
	ratespec_t rspec;               /**< @brief ratespec used in calculation */
	uint32 max_bytes;               /**< @brief maximum bytes in an AMPDU at current rate */
	uint16 Ndbps;                   /**< @brief bits to 4us symbol ratio */
	uint8 sgi;                      /**< @brief 1 if tx uses SGI */
	uint16 fixed_overhead_us;       /**< @brief length independent fixed time overhead */
	uint16 fixed_overhead_pmpdu_us; /**< @brief length independent fixed time overhead
	                                 *          minimum per-mpdu
	                                 */
	uint16 min_limit;               /**< @brief length boundary for fixed overhead
	                                 *          contribution methods
	                                 */
} ampdu_aqm_timecalc_t;

typedef struct ampdu_stats_range {
	uint32	max; /* Maximum value */
	uint32	min; /* Minimum value */
	uint32	avg; /* Average value */
	unsigned long accum; /* Accmulator for average calculations */
	uint32	iter; /* Number of samples in acccmulator */
} ampdu_stats_range_t;

#ifdef WLWRR
typedef struct wrr_ampdu_stats  {
	ampdu_stats_range_t ncons;
	ampdu_stats_range_t txdur;
	ampdu_stats_range_t frmsz;
} wrr_ampdu_stats_t;
#endif // endif

#ifdef WLATF
typedef struct atf_stats {
	uint32 timelimited; /* Times AMPDU release aborted due to time limit */
	uint32 framelimited; /* number of times full time limit was not released */
	uint32 reloverride; /* number of times ATF PMODE overrode release limit */
	uint32 uflow; /* number of times input queue went empty */
	uint32 oflow; /* nmber of times output queue was full */
	uint32 npkts; /* number of packets dequeued */
	uint32 ndequeue; /* Number of dequeue attempts */
	uint32 qempty; /* Incremented when packets in flight hit zero */
	uint32 minrel; /* Num times a dequeue request was below minimum time release */
	uint32 singleton; /* Count of single packets released */
	uint32 flush; /* Flush ATF accounting due to previous ATF reset */
	uint32 eval; /* number times ATF evaluated for packet release */
	uint32 neval; /* num times ATF stopped release */
	uint32 rskip; /* Release of packet skipped */
	uint32 cache_hit; /* ratespec cache hit */
	uint32 cache_miss; /* ratespec cache miss */
	uint32 proc; /* Number of packets completed */
	uint32 reproc; /* Times a completed packet was reprocessed */
	uint32 reg_mpdu; /* Packet sent as a regular MPDU from the AMPDU path */
	ampdu_stats_range_t chunk_bytes;
	ampdu_stats_range_t chunk_pkts;
	ampdu_stats_range_t rbytes;
	ampdu_stats_range_t inflight;
	ampdu_stats_range_t pdu;
	ampdu_stats_range_t release;
	ampdu_stats_range_t transit;
	ampdu_stats_range_t inputq;
	ampdu_stats_range_t input_rel;
	ampdu_stats_range_t ncons;
	ampdu_stats_range_t last_est_rate;
} atf_stats_t;

typedef struct atf_rbytes {
	uint max;
	uint min;
} atf_rbytes_t;

/* CSTYLED */
typedef ratespec_t (*atf_rspec_fn_t)(void *);

/* ATF rate calculation function.
 * Can be changed in realtime if fixed rates are imposed in runtime
 */
typedef struct {
	atf_rspec_fn_t	function;
	void *arg;
} atf_rspec_action_t;

typedef struct atf_state {
	uint32 		released_bytes_inflight; /* Number of bytes pending in bytes */
	uint32 		released_packets_inflight; /* Number of pending pkts,
						 * the AMPDU TID structure has a similar counter
						 * but its used is overloaded to
						 * track powersave packets
						 */
	uint32 		last_est_rate;	 /* Last estimated rate */
	uint		released_bytes_target;
	uint		released_minbytes_target;
	uint   		ac;
	wlcband_t 	*band;
	uint 		atf;
	uint 		txq_time_allowance_us;
	uint 		txq_time_min_allowance_us;
	uint16		last_ampdu_fid;
	atf_rbytes_t	rbytes_target;
	uint 		reset; /* Number times ATF state was reset */
	rcb_t 		*rcb; /* Pointer to rate control block for this TID */
	ampdu_tx_info_t *ampdu_tx;	/* Back pointer to ampdu_tx structure */
	scb_ampdu_tx_t 	*scb_ampdu;	/* Back pointer to scb_ampdu structure */
	scb_ampdu_tid_ini_t *ini;	/* Back pointer to ini structure */
	wlc_info_t	*wlc;
	struct scb *scb;

	/* Pointer to access function that returns rate to be used */
	atf_rspec_action_t	rspec_action;	/* Get current rspec function */
	ratespec_t		rspec_override;	/* Fixed rate rspec override */
#ifdef WLATF_DONGLE
	wlc_atfd_t *atfd; /* ATF dongle metadata */
#endif // endif
#ifdef WLCNT
	atf_stats_t	atf_stats;
#endif /* WLCNT */
} atf_state_t;
#endif /* WLATF */

/** structure to store per-tid state for the ampdu initiator */
typedef struct scb_ampdu_tid_ini_non_aqm {
	uint8 txretry[AMPDU_BA_MAX_WSIZE];	   /**< tx retry count; indexed by seq modulo */
	uint8 ackpending[AMPDU_BA_MAX_WSIZE/NBBY]; /**< bitmap: set if ack is pending */
	uint8 barpending[AMPDU_BA_MAX_WSIZE/NBBY]; /**< bitmap: set if bar is pending */
	uint16 rem_window;			   /**< !AQM only: remaining ba window (in pdus)
						    *	that can be txed.
						    */
	uint16 retry_seq[AMPDU_BA_MAX_WSIZE];	   /**< seq of released retried pkts */
	uint16 retry_head;			   /**< head of queue ptr for retried pkts */
	uint16 retry_tail;			   /**< tail of queue ptr for retried pkts */
	uint16 retry_cnt;			   /**< cnt of retried pkts */
} scb_ampdu_tid_ini_non_aqm_t;

struct scb_ampdu_tid_ini {
	uint8 ba_state;		/**< ampdu ba state */
	uint8 ba_wsize;		/**< negotiated ba window size (in pdu) */
	uint8 tid;		/**< initiator tid for easy lookup */
	uint16 tx_in_transit;	/**< #packets have left the AMPDU module and haven't been freed */
	uint16 barpending_seq;	/**< seqnum for bar */
	uint16 acked_seq;	/**< last ack received */
	uint16 start_seq;	/**< seqnum of first unack'ed mpdu, increments when window moves */
	uint16 max_seq;		/**< max unacknowledged seqnum released towards hardware */
	uint16 tx_exp_seq;	/**< next exp seq in sendampdu */
	uint16 next_enq_seq;    /**< last pkt seq that has been sent to txfifo */
	uint16 bar_ackpending_seq; /**< seqnum of bar for which ack is pending */
	bool bar_ackpending;	/**< true if there is a bar for which ack is pending */
	bool alive;		/**< true if making forward progress */
	uint8 retry_bar;	/**< reason code if bar to be retried at watchdog */
	uint8 bar_cnt;		/**< number of bars sent with no progress */
	uint8 addba_req_cnt;	/**< number of addba_req sent with no progress */
	uint8 cleanup_ini_cnt;	/**< number of sec waiting in pending off state */
	uint8 dead_cnt;		/**< number of sec without the window moving */
	uint8 off_cnt;		/**< number of sec in off state before next try */
	struct scb *scb;	/**< backptr for easy lookup */
	ampdu_aqm_timecalc_t timecalc[AC_COUNT];
	uint32	last_addba_ts;	/* timestamp of last addba req sent */
	/* rem_window is used to record the remaining ba window for new packets.
	 * when suppression happened, some holes may exist in current ba window,
	 * but they are not counted in rem_window.
	 * Then suppr_window is introduced to record suppressed packet counts inside ba window.
	 * Using suppr_window, we can keep rem_window untouched during suppression.
	 * When suppressed packets are resent by host, we need take both rem_window and
	 * suppr_window into account for decision of packet release.
	 */
	uint16 suppr_window; /**< suppr packet count inside ba window, including reg mpdu's */

	scb_ampdu_tid_ini_non_aqm_t *non_aqm;

	uint16 last_suppr_seq;  /* last or highest pkt seq that is suppressed to host */

#ifdef WLATF
	atf_state_t atf_state;
#endif // endif
#ifdef WLWRR
	wrr_ampdu_stats_t wrr_stats;
#endif // endif
};
#ifdef WLAMPDU_AQM
#define SCB_AMPDU_TID_INI_SZ	sizeof(struct scb_ampdu_tid_ini)
#else
#define SCB_AMPDU_TID_INI_SZ	sizeof(struct scb_ampdu_tid_ini) + \
				sizeof(scb_ampdu_tid_ini_non_aqm_t)
#endif /* WLAMPDU_AQM */

#ifdef BCMDBG
typedef struct scb_ampdu_cnt_tx {
	uint32 txampdu;
	uint32 txmpdu;
	uint32 txdrop;
	uint32 txstuck;
	uint32 txaddbareq;
	uint32 txrlag;
	uint32 txnoroom;
	uint32 sduretry;
	uint32 sdurejected;
	uint32 txlost;
	uint32 txbar;
	uint32 txreg_noack;
	uint32 noba;
	uint32 rxaddbaresp;
	uint32 rxdelba;
	uint32 rxba;

} scb_ampdu_cnt_tx_t;
#endif	/* BCMDBG */

#if defined(RAVG_HISTORY) || defined(RAVG_SIMPLE)
enum {
	RAVG_ALGO_NONE = 0,
	RAVG_ALGO_SIMPLE = 1,
	RAVG_ALGO_HISTORY = 2
};

#define RAVG_PKT_OCTET_INIT	(1024)

#ifdef RAVG_HISTORY
#define RAVG_HISTORY_PKTNUM	(8)

typedef struct pkt2oct {
	uint16 pkt_avg_octet;
	uint16 pkt_idx;
	uint16 pktlen[RAVG_HISTORY_PKTNUM];
} pkt2oct_t;
#endif /* RAVG_HISTORY */
#endif /* RAVG_HISTORY | RAVG_SIMPLE */

/**
 * Scb cubby structure. Ini and resp are dynamically allocated if needed. A lot of instances of this
 * structure can be generated on e.g. APs, so be careful with respect to memory size of this struct.
 */
struct scb_ampdu_tx {
	struct scb *scb;                /**< back pointer for easy reference */
	ampdu_tx_info_t *ampdu_tx;      /**< back ref to main ampdu */
	scb_ampdu_tid_ini_t *ini[AMPDU_MAX_SCB_TID];    /**< initiator info */
	uint8 tidmask;			/**< indicates which tids have BA on; used for NAR */
	uint8 mpdu_density;		/**< mpdu density */
	uint8 max_pdu;			/**< max pdus allowed in ampdu */
	uint8 release;			/**< # of mpdus released at a time */
	uint16 min_len;			/**< min mpdu len to support the density */
	uint32 max_rxlen;		/**< max ampdu rcv length; 8k, 16k, 32k, 64k */
	struct pktq txq;		/**< sdu transmit (multi-prio) queue pending aggregation */
	uint16 min_lens[AMPDU_HT_MCS_ARRAY_SIZE]; /* min mpdu lens per mcs */
	uint8 max_rxfactor;             /**< max ampdu length exponent + 13 */
	                                /**< (see Table 8-183v in IEEE Std 802.11ac-2012) */
	uint16 module_max_dur[NUM_MODULES]; /**< the maximum ampdu duration for each module */
	uint8 min_of_max_dur_idx;	/**< index of minimum of the maximum ampdu duration */
	ampdu_tx_scb_stats_t *ampdu_scb_stats;
#ifdef BCMDBG
	scb_ampdu_cnt_tx_t cnt;
#endif	/* BCMDBG */
#ifdef WLATF
	uint32 txretry_pkt;
	uint32 tot_pkt;
#endif // endif
#ifdef RAVG_SIMPLE
	uint16 pkt_avg_octet[PKTQ_MAX_PREC];
#endif // endif
#ifdef RAVG_HISTORY
	pkt2oct_t *pkt2oct[PKTQ_MAX_PREC];
#endif // endif
};

struct ampdu_tx_cubby {
	scb_ampdu_tx_t *scb_tx_cubby;
};

#define SCB_AMPDU_INFO(ampdu, scb) (SCB_CUBBY((scb), (ampdu)->scb_handle))
#define SCB_AMPDU_TX_CUBBY(ampdu, scb) \
	(((struct ampdu_tx_cubby *)SCB_AMPDU_INFO(ampdu, scb))->scb_tx_cubby)

/** bsscfg cubby structure. */
typedef struct bsscfg_ampdu_tx {
	int8 txaggr_override;	/**< txaggr override for all TIDs */
	uint16 txaggr_TID_bmap; /**< aggregation enabled TIDs bitmap */
} bsscfg_ampdu_tx_t;

#define BSSCFG_AMPDU_TX_CUBBY(ampdu_tx, bsscfg) \
	((bsscfg_ampdu_tx_t *)BSSCFG_CUBBY((bsscfg), (ampdu_tx)->bsscfg_handle))

/* local prototypes */
/* scb cubby */
#ifdef WLRSDB
static int scb_ampdu_tx_update(void *context, struct scb *scb, wlc_bsscfg_t* new_cfg);
#endif /* WLRSDB */
static int scb_ampdu_tx_init(void *context, struct scb *scb);
static void scb_ampdu_tx_deinit(void *context, struct scb *scb);
/* bsscfg cubby */
static int bsscfg_ampdu_tx_init(void *context, wlc_bsscfg_t *cfg);
static void bsscfg_ampdu_tx_deinit(void *context, wlc_bsscfg_t *cfg);
static void scb_ampdu_txflush(void *context, struct scb *scb);
static int wlc_ampdu_doiovar(void *hdl, uint32 actionid,
        void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif);
static void wlc_ampdu_watchdog(void *hdl);
static int wlc_ampdu_up(void *hdl);
static int wlc_ampdu_down(void *hdl);
static void wlc_ampdu_init_min_lens(scb_ampdu_tx_t *scb_ampdu);

static void wlc_ampdu_ini_move_window(ampdu_tx_info_t *ampdu_tx,
	scb_ampdu_tx_t *scb_ampdu, scb_ampdu_tid_ini_t *ini);

static void wlc_ampdu_assoc_state_change(void *client, bss_assoc_state_data_t *notif_data);

#ifdef AMPDU_NON_AQM
static void ampdu_update_max_txlen_noaqm(ampdu_tx_config_t *ampdu_tx_cfg, uint16 dur);
static void wlc_ffpld_init_noaqm(ampdu_tx_config_t *ampdu_tx_cfg);
static int wlc_ffpld_check_txfunfl_noaqm(wlc_info_t *wlc, int f, wlc_bsscfg_t *bsscfg);
static void wlc_ffpld_calc_mcs2ampdu_table_noaqm(ampdu_tx_info_t *ampdu_tx, int f,
                                                 wlc_bsscfg_t *bsscfg);
#ifdef BCMDBG
static void wlc_ffpld_show_noaqm(ampdu_tx_info_t *ampdu_tx);
#endif /* BCMDBG */
#ifdef WLAMPDU_UCODE
static int aggfifo_enque_noaqm(ampdu_tx_info_t *ampdu_tx, int length, int qid);
static uint16 wlc_ampdu_calc_maxlen_noaqm(ampdu_tx_info_t *ampdu_tx, uint8 plcp0, uint plcp3,
              uint32 txop);
#if defined(BCMDBG)
static uint get_aggfifo_space_noaqm(ampdu_tx_info_t *ampdu_tx, int qid);
#endif /* BCMDBG */
#endif /* WLAMPDU_UCODE */
#endif /* AMPDU_NON_AQM */

#ifdef WLOVERTHRUSTER
static void wlc_ampdu_tx_set_tcp_ack_ratio(ampdu_tx_info_t *ampdu_tx, uint8 tcp_ack_ratio);
static void wlc_ampdu_tcp_ack_suppress(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tx_t *scb_ampdu,
                                       void *p, uint8 tid);
#else
static bool wlc_ampdu_is_tcpack(ampdu_tx_info_t *ampdu_tx, void *p);
#endif /* WLOVERTHRUSTER */

#ifdef AMPDU_NON_AQM
#ifdef BCMDBG
static void wlc_print_ampdu_txstatus_noaqm(ampdu_tx_info_t *ampdu_tx,
	tx_status_t *pkg1, uint32 s1, uint32 s2);
#endif // endif
#endif /* AMPDU_NON_AQM */

static int BCMFASTPATH
wlc_sendampdu_aqm(ampdu_tx_info_t *ampdu_tx, wlc_txq_info_t *qi, void **pdu, int prec,
	struct spktq *output_q, int *pkt_cnt, uint fifo);

#ifdef AMPDU_NON_AQM
static int BCMFASTPATH
wlc_sendampdu_noaqm(ampdu_tx_info_t *ampdu_tx, wlc_txq_info_t *qi, void **pdu, int prec,
	struct spktq *output_q, int *pkt_cnt, uint fifo);
static bool BCMFASTPATH ampdu_is_exp_seq_noaqm(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tid_ini_t *ini,
	uint16 seq, bool suppress_pkt);
#endif /* AMPDU_NON_AQM */

static void wlc_ampdu_dotxstatus_regmpdu_aqm(ampdu_tx_info_t *ampdu_tx, struct scb *scb,
	void *p, tx_status_t *txs, wlc_txh_info_t *txh_info);

static bool BCMFASTPATH ampdu_is_exp_seq_aqm(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tid_ini_t *ini,
	uint16 seq, bool suppress_pkt);

static INLINE void wlc_ampdu_ini_move_window_aqm(ampdu_tx_info_t *ampdu_tx,
	scb_ampdu_tx_t *scb_ampdu, scb_ampdu_tid_ini_t *ini);

static void wlc_ampdu_dotxstatus_regmpdu_noaqm(ampdu_tx_info_t *ampdu_tx, struct scb *scb,
	void *p, tx_status_t *txs, wlc_txh_info_t *txh_info);

static INLINE void wlc_ampdu_ini_move_window_noaqm(ampdu_tx_info_t *ampdu_tx,
	scb_ampdu_tx_t *scb_ampdu, scb_ampdu_tid_ini_t *ini);

static void wlc_ampdu_send_bar(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tid_ini_t *ini, uint16 bar_seq);

#if (defined(PKTC) || defined(PKTC_TX_DONGLE))
static void wlc_ampdu_agg_pktc(void *ctx, struct scb *scb, void *p, uint prec);
#else
static void wlc_ampdu_agg(void *ctx, struct scb *scb, void *p, uint prec);
#endif // endif
static scb_ampdu_tid_ini_t *wlc_ampdu_init_tid_ini(ampdu_tx_info_t *ampdu_tx,
	scb_ampdu_tx_t *scb_ampdu, uint8 tid, bool override);
static void ampdu_ba_state_off(scb_ampdu_tx_t *scb_ampdu, scb_ampdu_tid_ini_t *ini);

static bool wlc_ampdu_txeval(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tx_t *scb_ampdu,
	scb_ampdu_tid_ini_t *ini, bool force);
static uint16 wlc_ampdu_release(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tx_t *scb_ampdu,
	scb_ampdu_tid_ini_t *ini, uint16 release, uint16 qavail, taf_scheduler_public_t* taf);
static void wlc_ampdu_free_chain(ampdu_tx_info_t *ampdu_tx, void *p, tx_status_t *txs);

static void wlc_send_bar_complete(wlc_info_t *wlc, uint txstatus, void *arg);

#if defined(DONGLEBUILD)
static void wlc_ampdu_txflowcontrol(wlc_info_t *wlc, scb_ampdu_tx_t *scb_ampdu,
	scb_ampdu_tid_ini_t *ini);
#else
#define wlc_ampdu_txflowcontrol(a, b, c)	do {} while (0)
#endif // endif

#ifdef AMPDU_NON_AQM
static void wlc_ampdu_dotxstatus_complete_noaqm(ampdu_tx_info_t *ampdu_tx, struct scb *scb, void *p,
	tx_status_t *txs, uint32 frmtxstatus, uint32 frmtxstatus2);
#endif /* AMPDU_NON_AQM */

#ifdef BCMDBG
static void ampdu_dump_ini(wlc_info_t *wlc, scb_ampdu_tid_ini_t *ini);
#else
#define	ampdu_dump_ini(a, b)
#endif // endif

#if defined(BCMDBG)
static int wlc_dyn_fb_dump(wlc_info_t *wlc, struct bcmstrbuf *b);
#else
#define wlc_dyn_fb_dump(wlc, b)
#endif // endif

static uint wlc_ampdu_txpktcnt(void *hdl);

#ifdef WLATF
static atf_rbytes_t* BCMFASTPATH wlc_ampdu_atf_calc_rbytes(atf_state_t *atf_state,
	ratespec_t rspec);
static void wlc_ampdu_atf_tid_set_release_time(scb_ampdu_tid_ini_t *ini,
	uint txq_time_allowance_us);
static void wlc_ampdu_atf_scb_set_release_time(ampdu_tx_info_t *ampdu_tx,
	struct scb *scb, uint txq_time_allowance_us);
static void wlc_ampdu_atf_set_default_release_time(ampdu_tx_info_t *ampdu_tx,
	scb_module_t *scbstate, uint txq_time_allowance_us);

static void wlc_ampdu_atf_tid_set_release_mintime(scb_ampdu_tid_ini_t *ini,
	uint txq_time_allowance_us);
static void wlc_ampdu_atf_scb_set_release_mintime(ampdu_tx_info_t *ampdu_tx,
	struct scb *scb, uint txq_time_allowance_us);
static void wlc_ampdu_atf_set_default_release_mintime(ampdu_tx_info_t *ampdu_tx,
	scb_module_t *scbstate, uint txq_time_allowance_us);

static void wlc_ampdu_atf_tid_setmode(scb_ampdu_tid_ini_t *ini, uint32 mode);
static void wlc_ampdu_atf_scb_setmode(ampdu_tx_info_t *ampdu_tx, struct scb *scb, uint32 mode);
static void wlc_ampdu_atf_ini_set_rspec_action(atf_state_t *atf_state, ratespec_t rspec);
#ifdef WLSQS
static uint32 wlc_sqs_get_atf_max_limit(scb_ampdu_tx_t *scb_ampdu, uint8 prio, uint8 aggsf);
#endif // endif
#if (defined(WLATF) && defined(WLCNT) && defined(BCMDBG))

#endif /* WLATF ... */

#ifdef WLC_AMPDU_DUMP
#if (defined(WLATF) && defined(WLCNT) && defined(BCMDBG))
static BCMFASTPATH void wlc_ampdu_update_rstat(ampdu_stats_range_t *range, uint32 val);
static void wlc_ampdu_print_rstat(struct bcmstrbuf *b,
	char *hdr, ampdu_stats_range_t *rstat, char *tlr);
static uint wlc_ampdu_rstat_avg(ampdu_stats_range_t *rstat);
#endif /* WLATF ... */
#endif /* WLC_AMPDU_DUMP */

#if defined(WLCNT) && defined(BCMDBG)
static void wlc_ampdu_atf_dump(atf_state_t *atf_state, struct bcmstrbuf *b);
static void wlc_ampdu_atf_clear_counters(scb_ampdu_tid_ini_t *ini);
#define AMPDU_ATF_INCRSTAT(r, v) wlc_ampdu_update_rstat((r), (v))
#define AMPDU_ATF_CLRCNT(ini) wlc_ampdu_atf_clear_counters((ini))
#else
#define AMPDU_ATF_INCRSTAT(r, v)
#define AMPDU_ATF_CLRCNT(ini)
#endif /* WLCNT */
#else
#define AMPDU_ATF_INCRSTAT(r, v)
#define AMPDU_ATF_CLRCNT(ini)
#endif /* WLATF */

#ifdef WLWRR
static void wlc_ampdu_wrr_update_rstat(ampdu_stats_range_t *range, uint32 val);
static void wlc_ampdu_wrr_clear_counters(scb_ampdu_tid_ini_t *ini);

#ifdef WLC_AMPDU_DUMP
static void wlc_ampdu_wrr_print_rstat(struct bcmstrbuf *b,
	char *hdr, ampdu_stats_range_t *rstat, char *tlr);
static uint wlc_ampdu_wrr_rstat_avg(ampdu_stats_range_t *rstat);
#endif /* WLC_AMPDU_DUMP */
#endif /* WLWRR */

#ifdef WLSCB_HISTO
static void BCMFASTPATH wlc_update_rate_histogram(struct scb *scb, ratesel_txs_t *rs_txs,
	uint16 succ_mpdu, uint16 succ_msdu, uint8 amsdu_sf);
#endif /* WLSCB_HISTO */

static void wlc_ampdu_ini_adjust(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tid_ini_t *ini, void *pkt);
static void wlc_ampdu_pkt_freed(wlc_info_t *wlc, void *pkt, uint txs);

static void wlc_ampdu_retry_ba_session(wlc_info_t *wlc, struct scb *scb, scb_ampdu_tid_ini_t *ini);
static void wlc_ampdu_ba_pending_off(wlc_info_t *wlc, struct scb *scb,
	scb_ampdu_tid_ini_t *ini, uint8 tid);
static int
wlc_ampdu_handle_ba_session_on(wlc_info_t *wlc, struct scb *scb,
	scb_ampdu_tid_ini_t *ini, uint8 tid);
static void wlc_ampdu_ba_off(wlc_info_t *wlc, struct scb *scb,
	scb_ampdu_tid_ini_t *ini, uint8 tid);

#ifdef WL_CS_RESTRICT_RELEASE
static void wlc_ampdu_txeval_all_tid(wlc_info_t *wlc, uint8 tid);
#endif /* WL_CS_RESTRICT_RELEASE */

static txmod_fns_t BCMATTACHDATA(ampdu_txmod_fns) = {
#if (defined(PKTC) || defined(PKTC_TX_DONGLE)) /* packet chaining */
	wlc_ampdu_agg_pktc,	/* Process the packet */
#else
	wlc_ampdu_agg,
#endif // endif
	wlc_ampdu_txpktcnt,	/* Return the packet count */
	scb_ampdu_txflush,	/* Handle the deactivation of the feature */
	NULL			/* Handle the activation of the feature */
};

typedef struct {
	struct scb *scb;
	uint8 tid;
	ampdu_tx_info_t *ampdu_tx;
} ampdu_tx_map_pkts_cb_params_t;

static bool wlc_ampdu_map_pkts_cb(void *ctx, void *pkt);
static void wlc_ampdu_cancel_pkt_callbacks(ampdu_tx_info_t *ampdu_tx, struct scb *scb, uint8 tid);

/** ncons 'number consumed' marks last packet in tx chain that can be freed */
static uint16
wlc_ampdu_rawbits_to_ncons(uint16 raw_bits)
{
	return ((raw_bits & TX_STATUS40_NCONS) >> TX_STATUS40_NCONS_SHIFT);
}

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

/* Updates the max ampdu duration array where each element represents the duration per module
* and finds the minimum duration in the array.
*/
void wlc_ampdu_tx_max_dur(ampdu_tx_info_t *ampdu_tx, struct scb *scb,
	scb_ampdu_module_t module_id, uint16 dur)
{
	uint8 i;
	uint8 min_idx;
	uint16 min_dur;
	scb_ampdu_tx_t *scb_ampdu;

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);

	ASSERT(scb_ampdu != NULL);

	scb_ampdu->module_max_dur[module_id] = dur;
	min_idx = AMPDU_MAXDUR_INVALID_IDX;
	min_dur = AMPDU_MAXDUR_INVALID_VAL;

	for (i = 0; i < NUM_MODULES; i++) {
		if (scb_ampdu->module_max_dur[i] < min_dur) {
			min_dur = scb_ampdu->module_max_dur[i];
			min_idx = i;
		}
	}
	scb_ampdu->min_of_max_dur_idx = min_idx;
}

/** Selects the max Tx PDUs tunables for 2 stream and 3 stream depending on the hardware support. */
static void BCMATTACHFN(wlc_set_ampdu_tunables)(wlc_info_t *wlc)
{
	if (D11REV_GE(wlc->pub->corerev, D11AQM_CORE)) {
		wlc->pub->tunables->ampdunummpdu1stream  = AMPDU_NUM_MPDU_1SS_D11AQM;
		wlc->pub->tunables->ampdunummpdu2streams = AMPDU_NUM_MPDU_2SS_D11AQM;
		wlc->pub->tunables->ampdunummpdu3streams = AMPDU_NUM_MPDU_3SS_D11AQM;
	} else {
		wlc->pub->tunables->ampdunummpdu1stream  = AMPDU_NUM_MPDU_1SS_D11HT;
		wlc->pub->tunables->ampdunummpdu2streams = AMPDU_NUM_MPDU_2SS_D11HT;
		wlc->pub->tunables->ampdunummpdu3streams = AMPDU_NUM_MPDU_3SS_D11HT;
	}
}

/** as part of system init, a transmit ampdu related data structure has to be initialized */
static void
BCMATTACHFN(wlc_ampdu_tx_cfg_init)(wlc_info_t *wlc, ampdu_tx_config_t *ampdu_tx_cfg)
{
	int i;

	ampdu_tx_cfg->ba_max_tx_wsize = AMPDU_TX_BA_MAX_WSIZE;

	/* initialize all priorities to allow AMPDU aggregation */
	for (i = 0; i < AMPDU_MAX_SCB_TID; i++)
		ampdu_tx_cfg->ini_enable[i] = TRUE;

	/* For D11 revs < 40, disable AMPDU on some priorities due to underlying fifo space
	 * allocation.
	 * D11 rev >= 40 has a shared tx fifo space managed by BMC hw that allows AMPDU agg
	 * to be used for any fifo.
	 */
	if (D11REV_LT(wlc->pub->corerev, 40)) {
		/* Disable ampdu for VO by default */
		ampdu_tx_cfg->ini_enable[PRIO_8021D_VO] = FALSE;
		ampdu_tx_cfg->ini_enable[PRIO_8021D_NC] = FALSE;

		/* Disable ampdu for BK by default since not enough fifo space */
		ampdu_tx_cfg->ini_enable[PRIO_8021D_NONE] = FALSE;
		ampdu_tx_cfg->ini_enable[PRIO_8021D_BK] = FALSE;
	}
#ifdef WL_DISABLE_VO_AGG
	/* Disable ampdu for voice traffic */
	ampdu_tx_cfg->ini_enable[PRIO_8021D_VO] = FALSE;
	ampdu_tx_cfg->ini_enable[PRIO_8021D_NC] = FALSE;
#endif /* WL_DISABLE_VO_AGG */
	ampdu_tx_cfg->ba_policy = DOT11_ADDBA_POLICY_IMMEDIATE;
	ampdu_tx_cfg->ba_tx_wsize = AMPDU_TX_BA_DEF_WSIZE; /* Tx ba window size (in pdu) */

	if (ampdu_tx_cfg->ba_tx_wsize > ampdu_tx_cfg->ba_max_tx_wsize) {
		WL_ERROR(("wl%d: The Default AMPDU_TX_BA_WSIZE is greater than MAX value\n",
			wlc->pub->unit));
		ampdu_tx_cfg->ba_tx_wsize = ampdu_tx_cfg->ba_max_tx_wsize;
	}

	ampdu_tx_cfg->mpdu_density = AMPDU_DEF_MPDU_DENSITY;
	ampdu_tx_cfg->max_pdu = AUTO;
	ampdu_tx_cfg->default_pdu =
		(wlc->stf->op_txstreams < 3) ? AMPDU_NUM_MPDU_LEGACY : AMPDU_MAX_MPDU;
	ampdu_tx_cfg->dur = AMPDU_MAX_DUR;
	ampdu_tx_cfg->probe_mpdu = AMPDU_DEF_PROBE_MPDU;
	ampdu_tx_cfg->btcx_dur_flag = FALSE;
	ampdu_tx_cfg->fb_override_enable = FRAMEBURST_OVERRIDE_DEFAULT;

	ampdu_tx_cfg->delba_timeout = 0; /* AMPDUXXX: not yet supported */
	ampdu_tx_cfg->tx_rel_lowat = AMPDU_DEF_TX_LOWAT;

#ifdef WLATF
	ampdu_tx_cfg->txq_time_allowance_us = AMPDU_TXQ_TIME_ALLOWANCE_US;
	ampdu_tx_cfg->txq_time_min_allowance_us = AMPDU_TXQ_TIME_MIN_ALLOWANCE_US;
	ampdu_tx_cfg->ampdu_atf_lowat_rel_cnt = AMPDU_ATF_LOWAT_REL_CNT;
#endif // endif

	ampdu_tx_cfg->ampdu_aggmode = AMPDU_AGGMODE_AUTO;

#ifdef AMPDU_NON_AQM
	ampdu_tx_cfg->hiagg_mode = FALSE;

	ampdu_tx_cfg->retry_limit = AMPDU_DEF_RETRY_LIMIT;
	ampdu_tx_cfg->rr_retry_limit = AMPDU_DEF_RR_RETRY_LIMIT;

	for (i = 0; i < AMPDU_MAX_SCB_TID; i++) {
		ampdu_tx_cfg->retry_limit_tid[i] = ampdu_tx_cfg->retry_limit;
		ampdu_tx_cfg->rr_retry_limit_tid[i] = ampdu_tx_cfg->rr_retry_limit;
	}

	ampdu_update_max_txlen_noaqm(ampdu_tx_cfg, ampdu_tx_cfg->dur);
	ampdu_tx_cfg->ffpld_rsvd = AMPDU_DEF_FFPLD_RSVD;
	ampdu_tx_cfg->mfbr = TRUE;
	ampdu_tx_cfg->tx_max_funl = FFPLD_TX_MAX_UNFL;
	wlc_ffpld_init_noaqm(ampdu_tx_cfg);
#endif /* non-AQM */
} /* wlc_ampdu_tx_cfg_init */

/* cfg inits that must be done after certain variables/states are initialized:
 * 1) wlc->pub->_ampdumac
 */
static void
BCMATTACHFN(wlc_ampdu_tx_cfg_init_post)(wlc_info_t *wlc, ampdu_tx_config_t *ampdu_tx_cfg)
{
	if (AMPDU_MAC_ENAB(wlc->pub))
		ampdu_tx_cfg->txpkt_weight = 1;
	else
		ampdu_tx_cfg->txpkt_weight = AMPDU_DEF_TXPKT_WEIGHT;

	if (!AMPDU_AQM_ENAB(wlc->pub))
		ampdu_tx_cfg->release = AMPDU_SCB_MAX_RELEASE;
	else
		ampdu_tx_cfg->release = AMPDU_SCB_MAX_RELEASE_AQM;

	ampdu_tx_cfg->addba_retry_timeout = AMPDU_ADDBA_REQ_RETRY_TIMEOUT;
	ampdu_tx_cfg->dyn_fb_rtsper_on  = AMPDU_DYNFB_RTSPER_ON;
	ampdu_tx_cfg->dyn_fb_rtsper_off = AMPDU_DYNFB_RTSPER_OFF;
	ampdu_tx_cfg->dyn_fb_minrtscnt  = AMPDU_DYNFB_MINRTSCNT;
}

ampdu_tx_info_t *
BCMATTACHFN(wlc_ampdu_tx_attach)(wlc_info_t *wlc)
{
	scb_cubby_params_t cubby_params;
	ampdu_tx_info_t *ampdu_tx;
	int i;

	/* some code depends on packed structures */
	STATIC_ASSERT(sizeof(struct dot11_bar) == DOT11_BAR_LEN);
	STATIC_ASSERT(sizeof(struct dot11_ba) == DOT11_BA_LEN + DOT11_BA_BITMAP_LEN);
	STATIC_ASSERT(sizeof(struct dot11_ctl_header) == DOT11_CTL_HDR_LEN);
	STATIC_ASSERT(sizeof(struct dot11_addba_req) == DOT11_ADDBA_REQ_LEN);
	STATIC_ASSERT(sizeof(struct dot11_addba_resp) == DOT11_ADDBA_RESP_LEN);
	STATIC_ASSERT(sizeof(struct dot11_delba) == DOT11_DELBA_LEN);
	STATIC_ASSERT(DOT11_MAXNUMFRAGS == NBITS(uint16));
	STATIC_ASSERT(ISPOWEROF2(AMPDU_TX_BA_MAX_WSIZE));

	wlc_set_ampdu_tunables(wlc);

	ASSERT(wlc->pub->tunables->ampdunummpdu2streams <= AMPDU_MAX_MPDU);
	ASSERT(wlc->pub->tunables->ampdunummpdu2streams > 0);
	ASSERT(wlc->pub->tunables->ampdunummpdu3streams <= AMPDU_MAX_MPDU);
	ASSERT(wlc->pub->tunables->ampdunummpdu3streams > 0);

	if (!(ampdu_tx = (ampdu_tx_info_t *)MALLOC(wlc->osh, sizeof(ampdu_tx_info_t)))) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}
	bzero((char *)ampdu_tx, sizeof(ampdu_tx_info_t));
	ampdu_tx->wlc = wlc;

#if !defined(WLRSDB_DVT) && !defined(SEPARATE_AMPDU_TXCFG)
	ampdu_tx->config = (ampdu_tx_config_t*) obj_registry_get(wlc->objr,
		OBJR_AMPDUTX_CONFIG);
#endif // endif
	if (ampdu_tx->config == NULL) {
		if ((ampdu_tx->config =  (ampdu_tx_config_t*) MALLOCZ(wlc->pub->osh,
			sizeof(ampdu_tx_config_t))) == NULL) {
			WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes", wlc->pub->unit,
				__FUNCTION__, MALLOCED(wlc->pub->osh)));
			goto fail;
		}
		obj_registry_set(wlc->objr, OBJR_AMPDUTX_CONFIG, ampdu_tx->config);
		wlc_ampdu_tx_cfg_init(wlc, ampdu_tx->config);
	}
#ifndef WLRSDB_DVT
	(void)obj_registry_ref(wlc->objr, OBJR_AMPDUTX_CONFIG);
#endif // endif

#ifdef WLCNT
	if (!(ampdu_tx->cnt = (wlc_ampdu_tx_cnt_t *)MALLOC(wlc->osh, sizeof(wlc_ampdu_tx_cnt_t)))) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	bzero((char *)ampdu_tx->cnt, sizeof(wlc_ampdu_tx_cnt_t));
#endif /* WLCNT */

	/* Read nvram param to see if it disables AMPDU tx aggregation */
	if ((getintvar(wlc->pub->vars, "11n_disable") &
		WLFEATURE_DISABLE_11N_AMPDU_TX)) {
		ampdu_tx->txaggr_support = FALSE;
	} else {
		ampdu_tx->txaggr_support = TRUE;
	}

	for (i = 0; i < AMPDU_MAX_SCB_TID; i++) {
		ampdu_tx->aqm_max_release[i] = AMPDU_AQM_RELEASE_DEFAULT;
	}

#ifdef WLOVERTHRUSTER
	if ((WLCISACPHY(wlc->band) || WLCISLCN20PHY(wlc->band)) && (OVERTHRUST_ENAB(wlc->pub))) {
		wlc_ampdu_tx_set_tcp_ack_ratio(ampdu_tx, AMPDU_DEF_TCP_ACK_RATIO);
		ampdu_tx->tcp_ack_info.tcp_ack_ratio_depth = AMPDU_DEF_TCP_ACK_RATIO_DEPTH;
	}
#endif // endif

	/* reserve cubby in the bsscfg container for private data */
	if ((ampdu_tx->bsscfg_handle = wlc_bsscfg_cubby_reserve(wlc,
		sizeof(bsscfg_ampdu_tx_t), bsscfg_ampdu_tx_init, bsscfg_ampdu_tx_deinit,
		NULL, (void *)ampdu_tx)) < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

#if defined(WL_DATAPATH_LOG_DUMP)
	ampdu_tx->cubby_name_id = wlc_scb_cubby_name_register(wlc->scbstate, "AMPDU");
	if (ampdu_tx->cubby_name_id == WLC_SCB_NAME_ID_INVALID) {
		WL_ERROR(("wl%d: wlc_scb_cubby_name_register() failed\n", wlc->pub->unit));
		goto fail;
	}
#endif // endif
	/* reserve cubby in the scb container for per-scb private data */
	bzero(&cubby_params, sizeof(cubby_params));

	cubby_params.context = ampdu_tx;
	cubby_params.fn_init = scb_ampdu_tx_init;
	cubby_params.fn_deinit = scb_ampdu_tx_deinit;
#if defined(WL_DATAPATH_LOG_DUMP)
	cubby_params.fn_data_log_dump = wlc_ampdu_datapath_summary;
#endif // endif
#ifdef WLRSDB
	cubby_params.fn_update = scb_ampdu_tx_update;
#endif /* WLRSDB */
	ampdu_tx->scb_handle = wlc_scb_cubby_reserve_ext(wlc, sizeof(struct ampdu_tx_cubby),
		&cubby_params);

	if (ampdu_tx->scb_handle < 0) {
		WL_ERROR(("wl%d: wlc_scb_cubby_reserve_ext() failed\n", wlc->pub->unit));
		goto fail;
	}

#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU) || defined(WL_LINKSTAT)
	if (!(ampdu_tx->amdbg = (ampdu_dbg_t *)MALLOCZ(wlc->osh, sizeof(ampdu_dbg_t)))) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
#endif /*  defined(BCMDBG) || defined(WLTEST) */

	/* register assoc state change callback for reassociation */
	if (wlc_bss_assoc_state_register(wlc, wlc_ampdu_assoc_state_change, ampdu_tx) != BCME_OK) {
		WL_ERROR(("wl%d: wlc_bss_assoc_state_register() failed\n", wlc->pub->unit));
		goto fail;
	}

#if defined(BCMDBG) || defined(BCMDBG_ERR) || defined(BCMDBG_AMPDU)
	if (!(ampdu_tx->mdbg = (mac_dbg_t *)MALLOCZ(wlc->osh, sizeof(mac_dbg_t)))) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes for mac_dbg_t\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
#endif /*  defined(BCMDBG) ... */

#ifdef WL_CS_RESTRICT_RELEASE
	if (wlc_restrict_attach(wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_restrict_attach failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#endif // endif

	/* register packet class callback */
	if (wlc_pcb_fn_set(wlc->pcb, 3, WLF2_PCB4_AMPDU, wlc_ampdu_pkt_freed) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_pcb_fn_set() failed\n", wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* register module -- needs to be last failure prone operation in this function */
	if (wlc_module_register(wlc->pub, ampdu_iovars, "ampdu_tx", ampdu_tx, wlc_ampdu_doiovar,
	                        wlc_ampdu_watchdog, wlc_ampdu_up, wlc_ampdu_down)) {
		WL_ERROR(("wl%d: ampdu_tx wlc_module_register() failed\n", wlc->pub->unit));
		goto fail;
	}

	/* register txmod function */
	wlc_txmod_fn_register(wlc->txmodi, TXMOD_AMPDU, ampdu_tx, ampdu_txmod_fns);

	/* try to set ampdu to the default value */
	wlc_ampdu_tx_set(ampdu_tx, wlc->pub->_ampdu_tx);

	wlc_ampdu_tx_cfg_init_post(wlc, ampdu_tx->config);

	/* Attach debug function for dynamic framebursting */
#if defined(BCMDBG)
	wlc_dump_register(wlc->pub, "dynfb", (dump_fn_t)wlc_dyn_fb_dump, (void *)wlc);
#endif // endif
#ifdef RAVG_HISTORY
	ampdu_tx->ravg_algo = RAVG_ALGO_HISTORY;
#endif // endif
#ifdef RAVG_SIMPLE
	ampdu_tx->ravg_algo = RAVG_ALGO_SIMPLE;
#endif // endif

#if defined(BCMDBG)
#endif // endif

	return ampdu_tx;

fail:
	if (obj_registry_unref(wlc->objr, OBJR_AMPDUTX_CONFIG) == 0) {
		obj_registry_set(wlc->objr, OBJR_AMPDUTX_CONFIG, NULL);
		if (ampdu_tx->config != NULL) {
			MFREE(wlc->osh, ampdu_tx->config, sizeof(ampdu_tx_config_t));
		}
	}
#ifdef WLCNT
	if (ampdu_tx->cnt)
		MFREE(wlc->osh, ampdu_tx->cnt, sizeof(wlc_ampdu_tx_cnt_t));
#endif /* WLCNT */
	MFREE(wlc->osh, ampdu_tx, sizeof(ampdu_tx_info_t));
	return NULL;
} /* wlc_ampdu_tx_attach */

void
BCMATTACHFN(wlc_ampdu_tx_detach)(ampdu_tx_info_t *ampdu_tx)
{
	ampdu_tx_config_t *config;
	wlc_info_t *wlc;

	if (!ampdu_tx)
		return;

	wlc = ampdu_tx->wlc;
	config = ampdu_tx->config;

	if (obj_registry_unref(wlc->objr, OBJR_AMPDUTX_CONFIG) == 0) {
		obj_registry_set(wlc->objr, OBJR_AMPDUTX_CONFIG, NULL);
		MFREE(wlc->osh, config, sizeof(ampdu_tx_config_t));
	}

#ifdef WLCNT
	if (ampdu_tx->cnt)
		MFREE(wlc->osh, ampdu_tx->cnt, sizeof(wlc_ampdu_tx_cnt_t));
#endif // endif
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU) || defined(WL_LINKSTAT)
	if (ampdu_tx->amdbg) {
		MFREE(wlc->osh, ampdu_tx->amdbg, sizeof(ampdu_dbg_t));
		ampdu_tx->amdbg = NULL;
	}
#endif // endif

#if defined(BCMDBG) || defined(BCMDBG_ERR) || defined(BCMDBG_AMPDU)
	if (ampdu_tx->mdbg) {
		MFREE(wlc->osh, ampdu_tx->mdbg, sizeof(mac_dbg_t));
		ampdu_tx->mdbg = NULL;
	}
#endif // endif

#ifdef WL_CS_RESTRICT_RELEASE
	wlc_restrict_detach(wlc);
#endif // endif

	wlc_bss_assoc_state_unregister(wlc, wlc_ampdu_assoc_state_change, ampdu_tx);
	wlc_module_unregister(wlc->pub, "ampdu_tx", ampdu_tx);
	MFREE(wlc->osh, ampdu_tx, sizeof(ampdu_tx_info_t));
} /* wlc_ampdu_tx_detach */

/** @return   Multi-priority packet queue */
struct pktq *
scb_ampdu_prec_pktq(wlc_info_t *wlc, struct scb *scb)
{
	struct pktq *q;
	scb_ampdu_tx_t *scb_ampdu;

	scb_ampdu = SCB_AMPDU_TX_CUBBY(wlc->ampdu_tx, scb);
	q = &scb_ampdu->txq;

	return q;
}

/**
 * Per 'conversation partner' an SCB is maintained. The 'cubby' in this SCB contains ampdu tx info
 * for a specific conversation partner.
 */
/* scb cubby init fn */
static int
scb_ampdu_tx_init(void *context, struct scb *scb)
{
	ampdu_tx_info_t *ampdu_tx = (ampdu_tx_info_t *)context;
	struct ampdu_tx_cubby *cubby_info = (struct ampdu_tx_cubby *)SCB_AMPDU_INFO(ampdu_tx, scb);
	scb_ampdu_tx_t *scb_ampdu;
	wlc_tunables_t *tunables = ampdu_tx->wlc->pub->tunables;
	int i, num_prec;
#ifdef RAVG_HISTORY
	pkt2oct_t *pkt2oct;
	int pkt_idx;
#endif // endif

	if (scb && !SCB_INTERNAL(scb)) {
#ifdef WLAMPDU_PRECEDENCE
		num_prec = WLC_PREC_COUNT;
#else
		num_prec = AMPDU_MAX_SCB_TID;
#endif /* WLAMPDU_PRECEDENCE */
#ifdef RAVG_HISTORY
		scb_ampdu = MALLOCZ(ampdu_tx->wlc->osh, sizeof(scb_ampdu_tx_t) +
			sizeof(ampdu_tx_scb_stats_t) + sizeof(pkt2oct_t)*num_prec);
#else
		scb_ampdu = MALLOCZ(ampdu_tx->wlc->osh, sizeof(scb_ampdu_tx_t) +
			sizeof(ampdu_tx_scb_stats_t));
#endif /* RAVG_HISTORY */
		if (!scb_ampdu) {
			WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(ampdu_tx->wlc), __FUNCTION__,
				(int)sizeof(scb_ampdu_tx_t), MALLOCED(ampdu_tx->wlc->osh)));
			return BCME_NOMEM;
		}
		cubby_info->scb_tx_cubby = scb_ampdu;
		scb_ampdu->scb = scb;
		scb_ampdu->ampdu_tx = ampdu_tx;
		scb_ampdu->ampdu_scb_stats = (ampdu_tx_scb_stats_t *)((char *)scb_ampdu +
			sizeof(scb_ampdu_tx_t));

#ifdef WLAMPDU_PRECEDENCE
#ifdef DONGLEBUILD
		pktq_init(&scb_ampdu->txq, WLC_PREC_COUNT,
			MAX(tunables->ampdu_pktq_size, tunables->ampdu_pktq_fav_size));
#else

		/* Set the overall queue packet limit to maximum number of packets, just rely on
		 * per-prec limits.
		 */
		pktq_init(&scb_ampdu->txq, WLC_PREC_COUNT, PKTQ_LEN_MAX);
#endif /* DONGLEBUILD */
		for (i = 0; i < WLC_PREC_COUNT; i += 2) {
			pktq_set_max_plen(&scb_ampdu->txq, i, tunables->ampdu_pktq_size);
			pktq_set_max_plen(&scb_ampdu->txq, i+1, tunables->ampdu_pktq_fav_size);
		}
#else /* WLAMPDU_PRECEDENCE */
#ifdef DONGLEBUILD
		pktq_init(&scb_ampdu->txq, AMPDU_MAX_SCB_TID, tunables->ampdu_pktq_size);
#else
		/* Set the overall queue packet limit to the max, just rely on per-prec limits */
		pktq_init(&scb_ampdu->txq, AMPDU_MAX_SCB_TID, PKTQ_LEN_MAX);
		for (i = 0; i < AMPDU_MAX_SCB_TID; i++) {
			pktq_set_max_plen(&scb_ampdu->txq, i, tunables->ampdu_pktq_size);
		}
#endif /* DONGLEBUILD */
#endif /* WLAMPDU_PRECEDENCE */
#ifdef RAVG_HISTORY
		pkt2oct = (pkt2oct_t *)((char *)scb_ampdu->ampdu_scb_stats +
			sizeof(ampdu_tx_scb_stats_t));
#endif /* RAVG_HISTORY */
		for (i = 0; i < num_prec; i++) {
#ifdef RAVG_SIMPLE
			scb_ampdu->pkt_avg_octet[i] = RAVG_PKT_OCTET_INIT;
#endif // endif
#ifdef RAVG_HISTORY
			scb_ampdu->pkt2oct[i] = pkt2oct;
			pkt2oct->pkt_avg_octet = RAVG_PKT_OCTET_INIT;
			pkt2oct->pkt_idx = 0;
			for (pkt_idx = 0; pkt_idx < RAVG_HISTORY_PKTNUM; pkt_idx++) {
				pkt2oct->pktlen[pkt_idx] = RAVG_PKT_OCTET_INIT;
			}
			pkt2oct++;
#endif /* RAVG_HISTORY */
		}
	}
	return 0;
} /* scb_ampdu_tx_init */

/** Call back function, frees queues related to a specific remote party ('scb') */
static void
scb_ampdu_tx_deinit(void *context, struct scb *scb)
{
	ampdu_tx_info_t *ampdu_tx = (ampdu_tx_info_t *)context;
	struct ampdu_tx_cubby *cubby_info = (struct ampdu_tx_cubby *)SCB_AMPDU_INFO(ampdu_tx, scb);
	scb_ampdu_tx_t *scb_ampdu = NULL;

	WL_AMPDU_UPDN(("scb_ampdu_deinit: enter\n"));

	ASSERT(cubby_info);

	if (cubby_info)
		scb_ampdu = cubby_info->scb_tx_cubby;
	if (!scb_ampdu)
		return;

	scb_ampdu_tx_flush(ampdu_tx, scb);
#ifdef PKTQ_LOG
	wlc_pktq_stats_free(ampdu_tx->wlc, &scb_ampdu->txq);
#endif // endif

	pktq_deinit(&scb_ampdu->txq);

	MFREE(ampdu_tx->wlc->osh, scb_ampdu, sizeof(scb_ampdu_tx_t) +
		sizeof(ampdu_tx_scb_stats_t));
	cubby_info->scb_tx_cubby = NULL;
}

#ifdef WLRSDB
static int
scb_ampdu_tx_update(void *context, struct scb *scb, wlc_bsscfg_t* new_cfg)
{
	ampdu_tx_info_t *ampdu_tx = (ampdu_tx_info_t *)context;
	wlc_info_t *new_wlc = new_cfg->wlc;
	struct ampdu_tx_cubby *cubby_info = (struct ampdu_tx_cubby *)SCB_AMPDU_INFO(ampdu_tx, scb);
	scb_ampdu_tx_t *scb_ampdu = cubby_info->scb_tx_cubby;
	int idx = 0;
	scb_ampdu->ampdu_tx = new_wlc->ampdu_tx;
#ifdef WLTAF
	wlc_taf_scb_state_update(ampdu_tx->wlc->taf_handle, scb, new_cfg,
		TAF_SCBSTATE_UPDATE_BSSCFG);
#endif // endif
	for (idx = 0; idx < AMPDU_MAX_SCB_TID; idx++) {
		if (scb_ampdu->ini[idx]) {
#ifdef WLATF
			AMPDU_ATF_STATE(scb_ampdu->ini[idx])->band = new_wlc->band;
#endif /* WLATF */
		}
	}
	return BCME_OK;
}
#endif /* WLRSDB */

/** Callback function, bsscfg cubby init fn */
static int
bsscfg_ampdu_tx_init(void *context, wlc_bsscfg_t *bsscfg)
{
	ampdu_tx_info_t *ampdu_tx = (ampdu_tx_info_t *)context;
	bsscfg_ampdu_tx_t *bsscfg_ampdu = BSSCFG_AMPDU_TX_CUBBY(ampdu_tx, bsscfg);
	ASSERT(bsscfg != NULL);

	if (ampdu_tx->txaggr_support) {
		/* Enable for all TID by default */
		bsscfg_ampdu->txaggr_override = AUTO;
		bsscfg_ampdu->txaggr_TID_bmap = AMPDU_ALL_TID_BITMAP;
	} else {
		/* AMPDU TX module does not allow tx aggregation */
		bsscfg_ampdu->txaggr_override = OFF;
		bsscfg_ampdu->txaggr_TID_bmap = 0;
	}
#ifdef WLTAF
	wlc_taf_bss_state_update(ampdu_tx->wlc->taf_handle, bsscfg,
		TAF_PARAM(bsscfg_ampdu->txaggr_override), TAF_BSS_STATE_AMPDU_AGGREGATE_OVR);

	wlc_taf_bss_state_update(ampdu_tx->wlc->taf_handle, bsscfg,
		TAF_PARAM(bsscfg_ampdu->txaggr_TID_bmap), TAF_BSS_STATE_AMPDU_AGGREGATE_TID);
#endif // endif
	return BCME_OK;
}

/** Callback function, bsscfg cubby deinit fn */
static void
bsscfg_ampdu_tx_deinit(void *context, wlc_bsscfg_t *bsscfg)
{
	ampdu_tx_info_t *ampdu_tx = (ampdu_tx_info_t *)context;
	bsscfg_ampdu_tx_t *bsscfg_ampdu = BSSCFG_AMPDU_TX_CUBBY(ampdu_tx, bsscfg);

	WL_AMPDU_UPDN(("bsscfg_ampdu_tx_deinit: enter\n"));

	bsscfg_ampdu->txaggr_override = OFF;
	bsscfg_ampdu->txaggr_TID_bmap = 0;
#ifdef WLTAF
	wlc_taf_bss_state_update(ampdu_tx->wlc->taf_handle, bsscfg,
		TAF_PARAM(bsscfg_ampdu->txaggr_override), TAF_BSS_STATE_AMPDU_AGGREGATE_OVR);

	wlc_taf_bss_state_update(ampdu_tx->wlc->taf_handle, bsscfg,
		TAF_PARAM(bsscfg_ampdu->txaggr_TID_bmap),  TAF_BSS_STATE_AMPDU_AGGREGATE_TID);
#endif // endif
}

/** Callback function, part of the tx module (txmod_fns_t) interface */
static void
scb_ampdu_txflush(void *context, struct scb *scb)
{
	ampdu_tx_info_t *ampdu_tx = (ampdu_tx_info_t *)context;

	scb_ampdu_tx_flush(ampdu_tx, scb);
}

/** flush is needed on deinit, ampdu deactivation, etc. */
void
scb_ampdu_tx_flush(ampdu_tx_info_t *ampdu_tx, struct scb *scb)
{
	scb_ampdu_tx_t *scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	uint8 tid;

	WL_AMPDU_UPDN(("scb_ampdu_tx_flush: enter\n"));

	for (tid = 0; tid < AMPDU_MAX_SCB_TID; tid++) {
		ampdu_cleanup_tid_ini(ampdu_tx, scb, tid, TRUE);
	}

	/* scb_ampdu->txq must be empty */
	BCM_REFERENCE(scb_ampdu);
	ASSERT(pktq_empty(&scb_ampdu->txq));
}

/**
 * (Re)initialize tx config related to a specific 'conversation partner'. Called when setting up a
 * new conversation, as a result of a wl IOV_AMPDU_MPDU command or transmit underflow.
 */
void
scb_ampdu_update_config(ampdu_tx_info_t *ampdu_tx, struct scb *scb)
{
	scb_ampdu_tx_t *scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	ampdu_tx_config_t *ampdu_tx_cfg = ampdu_tx->config;
	int peer3ss = 0, peer2ss = 0;
	wlc_info_t *wlc = ampdu_tx->wlc;

	/* The maximum number of TX MPDUs supported for the device is determined at the
	  * AMPDU attach time
	 * At this point we know the largest size the device can send, here the intersection
	 * with the peer's capability is done.
	 */
	if (D11REV_LT(wlc->pub->corerev, 128)) {

		/* 3SS peer check */
		if (SCB_VHT_CAP(scb)) {
			/* VHT capable peer */
			peer3ss = VHT_MCS_SS_SUPPORTED(3, scb->rateset.vht_mcsmap);
			peer2ss = VHT_MCS_SS_SUPPORTED(2, scb->rateset.vht_mcsmap);
		} else {
			/* HT Peer */
			peer3ss = (scb->rateset.mcs[2] != 0);
			peer2ss = (scb->rateset.mcs[1] != 0);
		}

		if ((scb->flags & SCB_BRCM) && !(scb->flags2 & SCB2_RX_LARGE_AGG) && !(peer3ss))
		{
			scb_ampdu->max_pdu = /* max pdus allowed in ampdu */
				MIN(ampdu_tx->wlc->pub->tunables->ampdunummpdu2streams,
				AMPDU_NUM_MPDU_LEGACY);
		} else {
			if (peer3ss) {
				scb_ampdu->max_pdu =
					(uint8)ampdu_tx->wlc->pub->tunables->ampdunummpdu3streams;
			} else if (peer2ss) {
				/* to increase throughput for multi-client tests */
				scb_ampdu->max_pdu = SCB_AMSDU_IN_AMPDU(scb) ?
					(uint8)ampdu_tx->wlc->pub->tunables->ampdunummpdu2streams :
					(uint8)ampdu_tx->wlc->pub->tunables->ampdunummpdu3streams;
			} else {
				scb_ampdu->max_pdu =
					(uint8)ampdu_tx->wlc->pub->tunables->ampdunummpdu1stream;
			}
#ifdef WL_MU_TX
			if (SCB_MU(scb)) {
				/* XXX: In case of MU client, if tx amsdu_in_ampdu, set the 2nd
				 * highest ampdu_mpdu, otherwise the highest ampdu_mpdu.
				 * This is experimental conclusion, and once more studied,
				 * a dynamic tuning mechanism will be applied.
				 */
				scb_ampdu->max_pdu = SCB_AMSDU_IN_AMPDU(scb) ?
					(uint8)ampdu_tx->wlc->pub->tunables->ampdunummpdu2streams :
					(uint8)ampdu_tx->wlc->pub->tunables->ampdunummpdu3streams;
			}
#endif // endif
		}
	} else {
		ratespec_t rspec;
		uint32 current_rate;

		rspec = wlc_scb_ratesel_get_primary(wlc, scb, NULL);
		current_rate = wf_rspec_to_rate(rspec);
		if ((current_rate < AMPDU_LOW_RATE_THRESH) && !SCB_MU(scb))
			scb_ampdu->max_pdu = MIN(AMPDU_MAX_MPDU, AMPDU_NUM_MPDU_LEGACY);
		else
			scb_ampdu->max_pdu = AMPDU_MAX_MPDU;
		WL_TRACE(("current_rate<%u> max_pdu<%u>\n", current_rate, scb_ampdu->max_pdu));
	}

#ifdef WLTAF
	wlc_taf_scb_state_update(ampdu_tx->wlc->taf_handle, scb, TAF_AMPDU,
		TAF_SCBSTATE_SOURCE_UPDATE);
#endif // endif

	ampdu_tx_cfg->default_pdu = scb_ampdu->max_pdu;

#ifdef AMPDU_NON_AQM
	if (!AMPDU_AQM_ENAB(ampdu_tx->wlc->pub)) {
		int i;
		/* go back to legacy size if some preloading is occuring */
		for (i = 0; i < NUM_FFPLD_FIFO; i++) {
			if (ampdu_tx_cfg->fifo_tb[i].ampdu_pld_size > FFPLD_PLD_INCR)
				scb_ampdu->max_pdu = ampdu_tx_cfg->default_pdu;
		}
	}
#endif /* AMPDU_NON_AQM */

	/* apply user override */
	if (ampdu_tx_cfg->max_pdu != AUTO)
		scb_ampdu->max_pdu = (uint8)ampdu_tx_cfg->max_pdu;

	scb_ampdu->release = MIN(scb_ampdu->max_pdu, ampdu_tx_cfg->release);

#ifdef AMPDU_NON_AQM
	if (!AMPDU_AQM_ENAB(ampdu_tx->wlc->pub)) {

		if (scb_ampdu->max_rxlen)
			scb_ampdu->release =
			        MIN(scb_ampdu->release, scb_ampdu->max_rxlen/1600);

		/* XXX: limit based on BE preload restriction for highest MCS. May not be
		 * ideal, but is a good approximation.
		 */
		scb_ampdu->release = MIN(scb_ampdu->release,
			ampdu_tx_cfg->fifo_tb[TX_AC_BE_FIFO].mcs2ampdu_table[AMPDU_MAX_MCS]);

	}
#endif /* AMPDU_NON_AQM */

	ASSERT(scb_ampdu->release);
} /* scb_ampdu_update_config */

/**
 * Update max_pdu and txq time allowance, called whenever MU group state Changes
 */
void scb_ampdu_update_config_mu(ampdu_tx_info_t *ampdu_tx, scb_t *scb)
{
	scb_ampdu_update_config(ampdu_tx, scb);
#ifdef WLATF
	wlc_ampdu_atf_scb_set_release_time(ampdu_tx, scb, ampdu_tx->config->txq_time_allowance_us);
#endif // endif
}

/**
 * Check the max_pdu size after ratesel, and update the link mem entry if changed.
 */
void
scb_ampdu_check_config(wlc_info_t *wlc, struct scb *scb)
{
	int8 max_pdu;
	ampdu_tx_info_t *ampdu_tx = wlc->ampdu_tx;
	scb_ampdu_tx_t *scb_ampdu;

	if (!SCB_AMPDU(scb)) {
		return;
	}

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	max_pdu = scb_ampdu->max_pdu;
	scb_ampdu_update_config(ampdu_tx, scb);
	if (scb_ampdu->max_pdu != max_pdu) {
		if (RATELINKMEM_ENAB(wlc->pub)) {
			wlc_ratelinkmem_update_link_entry(wlc, scb);
		}
	}
}

/**
 * Initialize tx config related to all 'conversation partners'. Called as a result of a
 * wl IOV_AMPDU_MPDU command or transmit underflow.
 */
void
scb_ampdu_update_config_all(ampdu_tx_info_t *ampdu_tx)
{
	struct scb *scb;
	struct scb_iter scbiter;
	wlc_info_t* wlc = ampdu_tx->wlc;

	WL_AMPDU_UPDN(("scb_ampdu_update_config_all: enter\n"));
	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (SCB_AMPDU(scb)) {
			scb_ampdu_update_config(ampdu_tx, scb);
		}
	}
	if (RATELINKMEM_ENAB(wlc->pub)) {
		wlc_ratelinkmem_update_link_entry_all(wlc, NULL, TRUE, FALSE);
	}

	/* The ampdu config affects values that are in the tx headers of outgoing packets.
	 * Invalidate tx header cache entries to clear out the stale information
	 */
	if (WLC_TXC_ENAB(wlc))
		wlc_txc_inv_all(wlc->txc);
}

/**
 * Callback function, returns the number of transmit packets held by AMPDU. Part of the txmod_fns_t
 * interface.
 */
static uint
wlc_ampdu_txpktcnt(void *hdl)
{
	ampdu_tx_info_t *ampdu_tx = (ampdu_tx_info_t *)hdl;
	struct scb *scb;
	struct scb_iter scbiter;
	int pktcnt = 0;
	scb_ampdu_tx_t *scb_ampdu;

	FOREACHSCB(ampdu_tx->wlc->scbstate, &scbiter, scb) {
		if (SCB_AMPDU(scb)) {
			scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
			pktcnt += pktq_n_pkts_tot(&scb_ampdu->txq);
		}
	}

	return pktcnt;
}

/** Returns the number of transmit packets held by AMPDU per BSS */
uint wlc_ampdu_bss_txpktcnt(ampdu_tx_info_t *ampdu_tx, wlc_bsscfg_t *bsscfg)
{
	uint pktcnt = 0;
	struct scb *scb;
	struct scb_iter scbiter;
	scb_ampdu_tx_t *scb_ampdu;

	FOREACH_BSS_SCB(ampdu_tx->wlc->scbstate, &scbiter, bsscfg, scb) {
		if (SCB_AMPDU(scb)) {
			scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
			pktcnt += pktq_n_pkts_tot(&scb_ampdu->txq);
		}
	}

	return pktcnt;
}

/** frees all the buffers and cleanup everything on wireless interface down */
static int
wlc_ampdu_down(void *hdl)
{
	ampdu_tx_info_t *ampdu_tx = (ampdu_tx_info_t *)hdl;
	struct scb *scb;
	struct scb_iter scbiter;

	WL_AMPDU_UPDN(("wlc_ampdu_down: enter\n"));

	if (WOWL_ACTIVE(ampdu_tx->wlc->pub))
		return 0;

	FOREACHSCB(ampdu_tx->wlc->scbstate, &scbiter, scb) {
		if (SCB_AMPDU(scb))
			scb_ampdu_tx_flush(ampdu_tx, scb);
	}

#ifdef AMPDU_NON_AQM
	/* we will need to re-run the pld tuning */
	wlc_ffpld_init_noaqm(ampdu_tx->config);
#endif // endif

	return 0;
}

/** Init/Cleanup after Reinit/BigHammer */
static int
wlc_ampdu_up(void *hdl)
{
	ampdu_tx_info_t *ampdu_tx = (ampdu_tx_info_t *)hdl;
	wlc_info_t *wlc = ampdu_tx->wlc;
	struct scb_iter scbiter;
	struct scb *scb;
	scb_ampdu_tx_t *scb_ampdu;
	scb_ampdu_tid_ini_t *ini;
	uint tid;

	if (!wlc->pub->up)
		return BCME_OK;

	/* reinit/bighammer */

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (SCB_AMPDU(scb) &&
		    (scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb)) != NULL) {
			for (tid = 0; tid < AMPDU_MAX_SCB_TID; tid ++) {
				if ((ini = scb_ampdu->ini[tid]) != NULL) {
					/* Send ampdu BAR to inform receiver on
					 * current status.
					 */
					wlc_ampdu_send_bar(ampdu_tx, ini, ini->start_seq);
				}
			}
		}
	}

	return BCME_OK;
}

/** Reset BA agreement after a reassociation (roam/reconnect) */
static void
wlc_ampdu_assoc_state_change(void *client, bss_assoc_state_data_t *notif_data)
{
	ampdu_tx_info_t *ampdu_tx = (ampdu_tx_info_t *)client;
	wlc_info_t *wlc = ampdu_tx->wlc;
	wlc_bsscfg_t *cfg = notif_data->cfg;
	struct scb *scb;

	/* force BA reset */

	if (BSSCFG_STA(cfg) &&
	    notif_data->type == AS_ROAM &&
	    /* We should do it on entering AS_JOIN_INIT state but
	     * unfortunately we don't have it for roam/reconnect.
	     */
	    notif_data->state == AS_JOIN_ADOPT) {
		if ((scb = wlc_scbfindband(wlc, cfg, &cfg->BSSID,
			CHSPEC_WLCBANDUNIT(cfg->current_bss->chanspec))) != NULL &&
		    SCB_AMPDU(scb)) {
			wlc_scb_ampdu_disable(wlc, scb);

		        /* Based on wsec for the link, update AMPDU feature in the transmission path
		         * By spec, 11n device can send AMPDU only with Open or CCMP crypto
		         */
			if (N_ENAB(wlc->pub) && scb->wsec != WEP_ENABLED &&
				scb->wsec != TKIP_ENABLED) {
				wlc_scb_ampdu_enable(wlc, scb);
	                }
		}
	}
}

/**
 * When tx aggregation is disabled by a higher software layer, AMPDU connections for all
 * 'conversation partners' in the caller supplied bss have to be torn down.
 */
/* If wish to do for all TIDs, input AMPDU_ALL_TID_BITMAP for conf_TDI_bmap */
static void
wlc_ampdu_tx_cleanup(ampdu_tx_info_t *ampdu_tx, wlc_bsscfg_t *bsscfg,
	uint16 conf_TID_bmap)
{
	uint8 tid;
	scb_ampdu_tx_t *scb_ampdu = NULL;
	struct scb *scb;
	struct scb_iter scbiter;
	wlc_info_t *wlc = ampdu_tx->wlc;

	scb_ampdu_tid_ini_t *ini;

	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
		if (!SCB_AMPDU(scb))
			continue;

		if (!SCB_ASSOCIATED(scb))
			continue;

		scb_ampdu = SCB_AMPDU_TX_CUBBY(wlc->ampdu_tx, scb);
		ASSERT(scb_ampdu);
		for (tid = 0; tid < AMPDU_MAX_SCB_TID; tid++) {
			if (!(isbitset(conf_TID_bmap, tid))) {
				continue;
			}

			ini = scb_ampdu->ini[tid];

			if (ini != NULL)
			{
				if ((ini->ba_state == AMPDU_TID_STATE_BA_ON) ||
					(ini->ba_state == AMPDU_TID_STATE_BA_PENDING_ON)) {

					/* Send delba */
					wlc_send_delba(ampdu_tx->wlc, scb, tid, TRUE,
						DOT11_RC_TIMEOUT);

					WLCNTINCR(ampdu_tx->cnt->txdelba);
				}

				/* Set state to off, and push the packets in ampdu_pktq to next */
				ampdu_ba_state_off(scb_ampdu, ini);

				/* Force cleanup */
				ampdu_cleanup_tid_ini(ampdu_tx, scb, tid, TRUE);
			}
		}
	}
} /* wlc_ampdu_tx_cleanup */

static int
wlc_ampdu_frameburst_override_per_tid(ampdu_tx_info_t *ampdu_tx, struct scb *scb)
{

	scb_ampdu_tx_t *scb_ampdu = NULL;
	scb_ampdu_tid_ini_t *ini;
	uint8 tid;
	bool override_allowed = TRUE;

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	for (tid = 0; tid < AMPDU_MAX_SCB_TID; tid++) {
		ini = scb_ampdu->ini[tid];
		if (!ini)
			continue;
		if (ini->ba_state == AMPDU_TID_STATE_BA_ON) {
			/* don't override FB as BA session is found */
			override_allowed = FALSE;
			break;
		}
	}
	return override_allowed;
}
/**
 * frame bursting waits a shorter time (RIFS) between tx frames which increases throughput but
 * potentially decreases airtime fairness.
 */
bool
wlc_ampdu_frameburst_override(ampdu_tx_info_t *ampdu_tx, struct scb *scb)
{
	if (ampdu_tx == NULL) {
		return TRUE; /* Don't override; avoid FB */
	}

	if (AMPDU_ENAB(ampdu_tx->wlc->pub) && SCB_AMPDU(scb) && SCB_HT_CAP(scb)) {
		ampdu_tx_config_t *ampdu_tx_cfg = ampdu_tx->config;
		if (ampdu_tx_cfg->fb_override_enable) {
			bool override_allowed = TRUE;
			override_allowed = wlc_ampdu_frameburst_override_per_tid(ampdu_tx, scb);
#ifndef FRAMEBURST_RTSCTS_PER_AMPDU
			return (override_allowed | ampdu_tx_cfg->fb_override);
#else
			return (override_allowed);
#endif /* FRAMEBURST_RTSCTS_PER_AMPDU */
		} else {
			return FALSE; /* Don't override; allow FB */
		}
	} else {
		/* AMPDU disabled or Legacy Client. FB override is allowed */
		return TRUE; /* avoid FB */
	}
}

#if defined(BCMDBG) || defined(BCMDBG_ERR) || defined(BCMDBG_AMPDU)
/* offset of cntmember by sizeof(uint32) from the first cnt variable. */
#define NUM_MCST_IN_MAC_DBG_T 6
static const uint8 mac_dbg_t_to_macstat_t[NUM_MCST_IN_MAC_DBG_T] = {
	MCSTOFF_TXFRAME,
	MCSTOFF_TXBCNFRM,
	MCSTOFF_TXRTSFRM,
	MCSTOFF_RXCTSUCAST,
	MCSTOFF_RXRSPTMOUT,
	MCSTOFF_RXSTRT
};

static void
wlc_ampdu_dbg_stats(wlc_info_t *wlc, struct scb *scb, scb_ampdu_tid_ini_t *ini,
	uint8 ampdu_ini_dead_timeout)
{
	uint8 i;
	mac_dbg_t now_mdbg;
	mac_dbg_t *mdbg;

	BCM_REFERENCE(scb);

	if (ini->dead_cnt == 0) return;

	mdbg = wlc->ampdu_tx->mdbg;
	if (!mdbg)
		return;

	memset(&now_mdbg, 0, sizeof(now_mdbg));
	for (i = 0; i < NUM_MCST_IN_MAC_DBG_T; i++) {
		((uint16 *)&now_mdbg)[i] =
			wlc_read_shm(wlc, MACSTAT_ADDR(wlc, mac_dbg_t_to_macstat_t[i]));
	}
	now_mdbg.txop = wlc_bmac_cca_read_counter(wlc->hw,
		M_CCA_TXOP_L_OFFSET(wlc), M_CCA_TXOP_H_OFFSET(wlc));

	if (ini->dead_cnt >= ampdu_ini_dead_timeout) {
		osl_t *osh;
		osh = wlc->osh;

		WL_ERROR(("ampdu_dbg: wl%d.%d "MACF" scb:%p tid:%d \n",
			wlc->pub->unit, WLC_BSSCFG_IDX(SCB_BSSCFG(scb)),
			ETHER_TO_MACF(scb->ea), scb, ini->tid));

		if (D11REV_GE(wlc->pub->corerev, 128)) {
			WL_PRINT(("ampdu_dbg: wl%d.%d dead_cnt %d tx_in_transit %d "
				"aqmqmap 0x%x aqmfifo_status 0x%x\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(SCB_BSSCFG(scb)),
				ini->dead_cnt, ini->tx_in_transit,
				R_REG(osh, D11_AQM_QMAP(wlc)),
				R_REG(osh, D11_MQF_STATUS(wlc))));
		} else if (D11REV_GE(wlc->pub->corerev, 65)) {
			WL_PRINT(("ampdu_dbg: wl%d.%d dead_cnt %d tx_in_transit %d "
				"psm_reg_mux 0x%x aqmqmap 0x%x aqmfifo_status 0x%x\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(SCB_BSSCFG(scb)),
				ini->dead_cnt, ini->tx_in_transit,
				R_REG(osh, D11_PSM_REG_MUX(wlc)),
				R_REG(osh, D11_AQMQMAP(wlc)),
				R_REG(osh, D11_AQMFifo_Status(wlc))));
		} else {
			uint16 fifordy, frmcnt, fifosel;
			if (D11REV_GE(wlc->pub->corerev, 40)) {
				fifordy = R_REG(osh, D11_AQMFifoReady(wlc));
				fifosel = R_REG(osh, D11_BMCCmd(wlc));
				frmcnt = R_REG(osh, D11_XmtFifoFrameCnt(wlc));

			} else {
				fifordy = R_REG(osh, D11_xmtfifordy(wlc));
				fifosel = R_REG(osh, D11_xmtsel(wlc));
				frmcnt = R_REG(osh, D11_XmtFifoFrameCnt(wlc));
			}
			WL_PRINT(("ampdu_dbg: wl%d.%d dead_cnt %d tx_in_transit %d "
				"fifordy 0x%x frmcnt 0x%x fifosel 0x%x\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(SCB_BSSCFG(scb)),
				ini->dead_cnt, ini->tx_in_transit,
				fifordy, frmcnt, fifosel));
			BCM_REFERENCE(fifordy);
			BCM_REFERENCE(fifosel);
			BCM_REFERENCE(frmcnt);
		}

		WL_PRINT(("ampdu_dbg: ifsstat 0x%x nav_stat 0x%x txop %u\n",
			R_REG(osh, D11_IFS_STAT(wlc)),
			R_REG(osh, D11_NAV_STAT(wlc)),
			now_mdbg.txop - mdbg->txop));

		WL_PRINT(("ampdu_dbg: pktpend: %d %d %d %d %d ap %d\n",
			TXPKTPENDGET(wlc, TX_AC_BK_FIFO), TXPKTPENDGET(wlc, TX_AC_BE_FIFO),
			TXPKTPENDGET(wlc, TX_AC_VI_FIFO), TXPKTPENDGET(wlc, TX_AC_VO_FIFO),
			TXPKTPENDGET(wlc, TX_BCMC_FIFO), AP_ENAB(wlc->pub)));

		WL_PRINT(("ampdu_dbg: txall %d txbcn %d txrts %d rxcts %d rsptmout %d rxstrt %d\n",
			now_mdbg.txallfrm - mdbg->txallfrm, now_mdbg.txbcn - mdbg->txbcn,
			now_mdbg.txrts - mdbg->txrts, now_mdbg.rxcts - mdbg->rxcts,
			now_mdbg.rsptmout - mdbg->rsptmout, now_mdbg.rxstrt - mdbg->rxstrt));

		WL_PRINT(("ampdu_dbg: cwcur0-3 %x %x %x %x bslots cur/0-3 %d %d %d %d %d "
			 "ifs_boff %d\n",
			 wlc_read_shm(wlc, 0x240 + 3*2), wlc_read_shm(wlc, 0x260 + 3*2),
			 wlc_read_shm(wlc, 0x280 + 3*2), wlc_read_shm(wlc, 0x2a0 + 3*2),
			 wlc_read_shm(wlc, 0x16f*2),
			 wlc_read_shm(wlc, 0x240 + 5*2), wlc_read_shm(wlc, 0x260 + 5*2),
			 wlc_read_shm(wlc, 0x280 + 5*2), wlc_read_shm(wlc, 0x2a0 + 5*2),
			 R_REG(osh, D11_IFS_BOFF_CTR(wlc))));

		for (i = 0; i < 2; i++) {
			WL_PRINT(("ampdu_dbg: again%d ifsstat 0x%x nav_stat 0x%x\n",
				i + 1,
				R_REG(osh, D11_IFS_STAT(wlc)),
				R_REG(osh, D11_NAV_STAT(wlc))));
		}
	}

	/* Copy macstat_t counters */
	memcpy(mdbg, &now_mdbg, sizeof(now_mdbg));

#if defined(BCM_DMA_CT) && !defined(BCM_DMA_CT_DISABLED)
	if (BCM_DMA_CT_ENAB(wlc) && (ini->dead_cnt >= ampdu_ini_dead_timeout)) {
		int npkts;
		for (i = 0; i < WLC_HW_NFIFO_INUSE(wlc); i++) {
			npkts = TXPKTPENDGET(wlc, i);
			if (npkts == 0) continue;
			WL_ERROR(("FIFO-%d TXPEND = %d TX-DMA%d =>\n", i, npkts, i));
#if defined(BCMDBG)
			if (ini->dead_cnt >= 8 || wlc->ampdu_tx->txnoprog_cnt > 0) {
				hnddma_t *di = WLC_HW_DI(wlc, i);
				dma_dumptx(di, NULL, FALSE);
				WL_ERROR(("CT-DMA%d =>\n", i));
				di = WLC_HW_AQM_DI(wlc, i);
				dma_dumptx(di, NULL, FALSE);
			}
#endif // endif
		}
	}
#endif /* BCM_DMA_CT && !BCM_DMA_CT_DISABLED */
} /* wlc_ampdu_dbg_stats */
#else
static void
wlc_ampdu_dbg_stats(wlc_info_t *wlc, struct scb *scb, scb_ampdu_tid_ini_t *ini,
	uint8 ampdu_ini_dead_timeout)
{
}
#endif /* defined(BCMDBG) || defined(BCMDBG_ERR) */

#if defined(WLATF) && defined(WLATF_PERC)
/**
 * Find the scb with maximum airtime percentage
 */
static void wlc_ampdu_update_atf_perc_max(wlc_info_t *wlc, uint32 now)
{
	struct scb *scb;
	struct scb_iter scbiter;
	uint16 perc_max = 0;

	if (((wlc->pub->now - wlc->atm_ref_time_sec) > ATM_IDLE_TIME_SEC) ||
		((now - wlc->atm_ref_time_usec) > ATM_IDLE_TIME_USEC))  {

		FOREACHSCB(wlc->scbstate, &scbiter, scb) {

			if (((wlc->pub->now - scb->used) < ATM_IDLE_TIME_SEC) &&
				((now - scb->last_active_usec) < ATM_IDLE_TIME_USEC) &&
				(scb->sched_staperc > perc_max)) {
				perc_max = scb->sched_staperc;
			}
		}
		wlc->perc_max = perc_max;
		wlc->atm_ref_time_usec = now;
		wlc->atm_ref_time_sec = wlc->pub->now;
	}
}

/**
 * Update ampdu release time allowance
 */
static void wlc_ampdu_atf_update_release_duration(wlc_info_t *wlc)
{
	struct scb *scb;
	struct scb_iter scbiter;
	scb_ampdu_tid_ini_t *ini;
	atf_state_t *atf_state;
	ratespec_t rspec;
	scb_ampdu_tx_t *scb_ampdu;
	uint8 tid;
	uint32 txq_time_allowance_us;
	FOREACHSCB(wlc->scbstate, &scbiter, scb) {

		if (wlc->perc_max && (scb->sched_staperc <= wlc->perc_max)) {
			txq_time_allowance_us = (AMPDU_TXQ_TIME_ALLOWANCE_US *
				scb->sched_staperc)/wlc->perc_max;

			if (txq_time_allowance_us < AMPDU_TXQ_TIME_MIN_ALLOWANCE_US) {
				WL_ERROR(("AMPDU release time allowance %d less than %d us"
					" TCP may underperform re-adjust ATF percentage\n",
					txq_time_allowance_us, AMPDU_TXQ_TIME_ALLOWANCE_US));
			}
			scb_ampdu = SCB_AMPDU_TX_CUBBY(wlc->ampdu_tx, scb);
			/* Update atf state release bytes */
			for (tid = 0; tid < AMPDU_MAX_SCB_TID; tid++) {

				ini = scb_ampdu->ini[tid];
				if (!ini)
					continue;
				atf_state = AMPDU_ATF_STATE(ini);
				atf_state->txq_time_allowance_us = txq_time_allowance_us;
				rspec = atf_state->rspec_action.
					function(atf_state->rspec_action.arg);
				wlc_ampdu_atf_calc_rbytes(atf_state, rspec);
			}
		}
	}
}
#endif /* WLATF && WLATF_PERC */

/**
 * Timer function, called approx every second. Resends ADDBA-Req if the ADDBA-Resp has not come
 * back.
 */
static void
wlc_ampdu_watchdog(void *hdl)
{
	ampdu_tx_info_t *ampdu_tx = (ampdu_tx_info_t *)hdl;
	wlc_info_t *wlc = ampdu_tx->wlc;
	scb_ampdu_tx_t *scb_ampdu;
	scb_ampdu_tid_ini_t *ini;
	int idx;
	wlc_bsscfg_t *cfg;
	struct scb *scb;
	struct scb_iter scbiter;
	uint8 tid;
	uint8 rtsper_avg;
	ampdu_tx_config_t *ampdu_tx_cfg = ampdu_tx->config;
#if defined(WLATF) && defined(WLATF_AMPDU_RETRY_COMP_THRESH)
	atf_state_t *atf_state;
	uint16  retry_percentage = 0;
	uint32	compensate = 0;
	ratespec_t rspec;
#endif // endif
	FOREACH_BSS(wlc, idx, cfg) {
	    FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {

			if (!SCB_AMPDU(scb))
				continue;
			scb_ampdu = SCB_AMPDU_TX_CUBBY(wlc->ampdu_tx, scb);
			ASSERT(scb_ampdu);
			for (tid = 0; tid < AMPDU_MAX_SCB_TID; tid++) {

				ini = scb_ampdu->ini[tid];
				if (!ini)
					continue;

#if defined(WLATF) && defined(WLATF_AMPDU_RETRY_COMP_THRESH)
				atf_state = AMPDU_ATF_STATE(ini);
				retry_percentage = scb_ampdu->tot_pkt ?
						(scb_ampdu->txretry_pkt*100)/scb_ampdu->tot_pkt: 0;

				/* Compensate only if retry is greater than compensate threshold */
				if (retry_percentage > AMPDU_RETRY_COMPENSATE_THRESH) {
					compensate = ((ampdu_tx_cfg->txq_time_allowance_us *
					  (retry_percentage - AMPDU_RETRY_COMPENSATE_THRESH))/100);
				} else {
					compensate = 0;
				}

				if (compensate < (ampdu_tx_cfg->txq_time_allowance_us -
						ampdu_tx_cfg->txq_time_min_allowance_us)) {
					atf_state->txq_time_allowance_us =
						ampdu_tx_cfg->txq_time_allowance_us - compensate;
				} else {
					atf_state->txq_time_allowance_us =
						ampdu_tx_cfg->txq_time_min_allowance_us;
				}

				scb_ampdu->tot_pkt = 0;
				scb_ampdu->txretry_pkt = 0;

				/* Update release byte based on new txq allowance */
				rspec = atf_state->rspec_action.function(
						atf_state->rspec_action.arg);
				wlc_ampdu_atf_calc_rbytes(atf_state, rspec);
#endif /* WLATF && WLATF_AMPDU_RETRY_COMP_THRESH */
				switch (ini->ba_state) {
					case AMPDU_TID_STATE_BA_ON: {
						wlc_ampdu_handle_ba_session_on(wlc, scb, ini, tid);
						break;
					}

					case AMPDU_TID_STATE_BA_PENDING_ON: {
						wlc_ampdu_retry_ba_session(wlc, scb, ini);
						break;
					}

					case AMPDU_TID_STATE_BA_PENDING_OFF: {
						wlc_ampdu_ba_pending_off(wlc, scb, ini, tid);
						break;
					}

					case AMPDU_TID_STATE_BA_OFF: {
						wlc_ampdu_ba_off(wlc, scb, ini, tid);
						break;
					}
					default:
						break;
				}
				/* dont do anything with ini here since it might be freed */
			}
		}
	}

	/* Dynamic Frameburst */
	if (ampdu_tx->wlc->hti->frameburst && ampdu_tx_cfg->fb_override_enable) {
#ifdef WLMCHAN
		/* dynamic fb currently not supported with MCHAN */
		if (MCHAN_ACTIVE(wlc->pub)) {
			ampdu_tx_cfg->fb_override = FALSE;
			return;
		}
#endif // endif
		if (ampdu_tx->rtstxcnt >= ampdu_tx_cfg->dyn_fb_minrtscnt) {
			/* Calculate average RTS PER percentage
			 * rts per % = 100 - rts success percentage
			 */
			rtsper_avg = 100 - ((ampdu_tx->ctsrxcnt * 100) /
					ampdu_tx->rtstxcnt);
		} else {
			rtsper_avg = 0;
		}

		/* record rtsper_avg for stats */
		ampdu_tx->rtsper_avg = rtsper_avg;

		/* If frameburst is currently enabled (fb_override = false) &&
		 * rtsper exceeds threshold, disable FB (fb_override = true).
		 * OR
		 * If frameburst is currently disabled (fb_override = true) &&
		 * rtsper falls below threshold, re-enable FB (fb_override = false).
		 */
		if ((ampdu_tx_cfg->fb_override == FALSE) &&
				rtsper_avg >= ampdu_tx_cfg->dyn_fb_rtsper_off) {
			ampdu_tx_cfg->fb_override = TRUE;	/* frameburst = 0 */
			WL_AMPDU_TX(("%s: frameburst disabled (rtsper_avg %d%%)\n",
				__FUNCTION__, rtsper_avg));
		} else if (ampdu_tx_cfg->fb_override &&
				rtsper_avg <= ampdu_tx_cfg->dyn_fb_rtsper_on) {
			ampdu_tx_cfg->fb_override = FALSE;	/* frameburst = 1 */
			WL_AMPDU_TX(("%s: frameburst enabled (rtsper_avg %d%%)\n",
				__FUNCTION__, rtsper_avg));
		}

#ifdef FRAMEBURST_RTSCTS_PER_AMPDU
		/* Frameburst always ON */
		if (ampdu_tx_cfg->fb_override == TRUE) {
			/* Do not disable frame burst instead enable RTS/CTS per ampdu */
			wlc_mhf(wlc, MHF4, MHF4_RTS_INFB, TRUE, WLC_BAND_ALL);
		} else {
			/* Enable frame burst without RTS/CTS per AMPDU,
			 * RTS/CTS only for first AMPDU.
			 */
			wlc_mhf(wlc, MHF4, MHF4_RTS_INFB, FALSE, WLC_BAND_ALL);
		}
#endif // endif
		/* Reset accumulated rts/cts counters */
		ampdu_tx->ctsrxcnt = 0;
		ampdu_tx->rtstxcnt = 0;
	}
} /* wlc_ampdu_watchdog */

static int
wlc_ampdu_handle_ba_session_on(wlc_info_t *wlc, struct scb *scb,
	scb_ampdu_tid_ini_t *ini, uint8 tid)

{
	ampdu_tx_info_t *ampdu_tx = wlc->ampdu_tx;
	scb_ampdu_tx_t *scb_ampdu;
	uint8 ampdu_ini_dead_timeout;

	WL_TRACE(("%s\n", __FUNCTION__));

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	ASSERT(scb_ampdu);

	if (ini->bar_cnt >= AMPDU_BAR_RETRY_CNT) {
		wlc_ampdu_tx_send_delba(ampdu_tx, scb, tid,
			TRUE, DOT11_RC_TIMEOUT);
		return 0;
	}

	if (ini->retry_bar) {
		/* Free queued packets when system is short of memory;
		 * release 1/4 buffered and at most wl_ampdu_drain_cnt pkts.
		 */
		if (ini->retry_bar == AMPDU_R_BAR_NO_BUFFER) {
			int rel_cnt, i;

			WL_ERROR(("wl%d: %s(): no memory\n",
				wlc->pub->unit, __FUNCTION__));
			rel_cnt = ampdu_pktqprec_n_pkts(&scb_ampdu->txq,
				ini->tid) >> 2;
			if (wl_ampdu_drain_cnt <= 0)
				wl_ampdu_drain_cnt = AMPDU_SCB_DRAIN_CNT;
			rel_cnt = MIN(rel_cnt, wl_ampdu_drain_cnt);
			for (i = 0; i < rel_cnt; i++) {
				void *p = NULL;
				p = _wlc_ampdu_pktq_pdeq(&scb_ampdu->txq,
					ini->tid);
				ASSERT(p != NULL);
				ASSERT(PKTPRIO(p) == ini->tid);

				wlc_tx_status_update_counters(wlc, p,
					scb, NULL, WLC_TX_STS_NO_BUF, 1);

				PKTFREE(wlc->pub->osh, p, TRUE);
				AMPDUSCBCNTINCR(scb_ampdu->cnt.txdrop);
				WLCNTINCR(ampdu_tx->cnt->txdrop);
#ifdef WL11K
				wlc_rrm_stat_qos_counter(wlc, scb, tid,
					OFFSETOF(rrm_stat_group_qos_t,
					txdrop));
#endif // endif
			}
			/* Check if ini is still in use, or if it has been freed. */
			if (scb_ampdu->ini[tid] == NULL) {
				WL_ERROR(("%s(): ini for tid %d is freed!\n", __FUNCTION__, tid));
				return 0;
			}
		}

		if (!AMPDU_AQM_ENAB(wlc->pub)) {
			wlc_ampdu_send_bar(ampdu_tx, ini, SEQNUM_CHK);
		} else {
			wlc_ampdu_send_bar(ampdu_tx, ini, ini->barpending_seq);
		}
	}

#ifdef WLTAF
	if (!wlc_taf_in_use(wlc->taf_handle))
#endif /* WLTAF */
	{
	/* tickle the sm and release whatever pkts we can */
	wlc_ampdu_txeval(ampdu_tx, scb_ampdu, ini, TRUE);
	}

	ampdu_ini_dead_timeout = wlc_twt_scb_active(wlc->twti, scb) ? AMPDU_INI_TWT_DEAD_TIMEOUT :
		AMPDU_INI_DEAD_TIMEOUT;

	/* check on forward progress */
	if (ini->alive) {
		ini->alive = FALSE;
		ini->dead_cnt = 0;
	} else {
		if (ini->tx_in_transit)
			ini->dead_cnt++;

		wlc_ampdu_dbg_stats(wlc, scb, ini, ampdu_ini_dead_timeout);
	}

	if (ini->dead_cnt >= ampdu_ini_dead_timeout) {
#if defined(AP) && defined(BCMDBG)
		if (SCB_PS(scb)) {
			uint32 i;
			char* mode = "PS";
			wlc_bsscfg_t *bsscfg = SCB_BSSCFG(scb);
			ASSERT(bsscfg != NULL);

#ifdef PSPRETEND
			if (SCB_PS_PRETEND(scb)) {
				mode = "PPS";
			}
#endif /* PSPRETEND */

			if (!BSSCFG_HAS_PSINFO(bsscfg)) {
				WL_ERROR(("wl%d.%d: %s: "MACF" in %s "
				     "mode may be stuck or receiver died\n",
				     wlc->pub->unit,
				     WLC_BSSCFG_IDX(SCB_BSSCFG(scb)),
				     __FUNCTION__, ETHER_TO_MACF(scb->ea),
				     mode));
			} else {
				WL_ERROR(("wl%d.%d: %s: "MACF" in %s "
				     "mode may be stuck(inPS: %d, inPVB %d) or receiver died\n",
				     wlc->pub->unit,
				     WLC_BSSCFG_IDX(SCB_BSSCFG(scb)),
				     __FUNCTION__, ETHER_TO_MACF(scb->ea),
				     mode, wlc_apps_scb_in_ps_time(wlc, scb),
				     wlc_apps_scb_in_pvb_time(wlc, scb)));

				WL_ERROR(("\tcurtime: %d\n", wlc_lifetime_now(wlc)));
				WL_ERROR(("\tAPPS PSQ len: %d\n", wlc_apps_psq_len(wlc, scb)));

				for (i = 0; i < NUMPRIO; i++) {
					if (SCB_PKTS_INFLT_FIFOCNT_VAL(scb, i)) {
						WL_ERROR(("\tpkts_inflt_fifocnt: prio-%d =>%d\n", i,
							SCB_PKTS_INFLT_FIFOCNT_VAL(scb, i)));
					}
				}

				for (i = 0; i < (NUMPRIO * 2); i++) {
					if (SCB_PKTS_INFLT_CQCNT_VAL(scb, i)) {
						WL_ERROR(("\tpkts_inflt_cqcnt: prec-%d =>%d\n", i,
							SCB_PKTS_INFLT_CQCNT_VAL(scb, i)));
					}
				}
			}
		}
#endif /* AP && BCMDBG */

		WL_ERROR(("wl%d: %s: cleaning up ini"
			" tid %d due to no progress for %d secs"
			" tx_in_transit %d\n",
			wlc->pub->unit, __FUNCTION__,
			tid, ini->dead_cnt, ini->tx_in_transit));
		WLCNTINCR(ampdu_tx->cnt->txstuck);
		AMPDUSCBCNTINCR(scb_ampdu->cnt.txstuck);
		/* AMPDUXXX: unknown failure, send delba */

		ampdu_dump_ini(wlc, ini);
		wlc_ampdu_tx_send_delba(ampdu_tx, scb, tid,
			TRUE, DOT11_RC_TIMEOUT);
		ampdu_cleanup_tid_ini(ampdu_tx, scb, tid, TRUE);
	}

	return 0;
}

static void wlc_ampdu_retry_ba_session(wlc_info_t *wlc, struct scb *scb, scb_ampdu_tid_ini_t *ini)
{
	ampdu_tx_info_t *ampdu_tx = wlc->ampdu_tx;
	scb_ampdu_tx_t *scb_ampdu;
	int ampdu_addba_req_retry = AMPDU_ADDBA_REQ_RETRY_CNT;
	ampdu_tx_config_t *ampdu_tx_cfg = ampdu_tx->config;
	uint32 elapsed_ts_addbatx = (OSL_SYSUPTIME() - ini->last_addba_ts);
	int err;

	WL_TRACE(("%s\n", __FUNCTION__));

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	/* For non-awdl scb stop after addba_req_cnt max count is reached */
	if (ini->addba_req_cnt >= ampdu_addba_req_retry) {
		ampdu_ba_state_off(scb_ampdu, ini);
	} else {
		/* - Retry on the scbs with retries < 10 with addba_retry_timeout buffer
		 * - Else retry with with 1 second buffer
		 */
		if ((ini->addba_req_cnt < 10 &&
			(elapsed_ts_addbatx >= ampdu_tx_cfg->addba_retry_timeout)) ||
			(elapsed_ts_addbatx >= 1000)) {
#ifdef MFP
			if (!WLC_MFP_ENAB(wlc->pub) ||
				(scb->wsec == 0) ||
				(WLC_MFP_ENAB(wlc->pub) && SCB_AUTHORIZED(scb)))
#endif /* MFP */
			{
				WL_AMPDU_ERR(("addba timed out %d\n", ini->addba_req_cnt));
				err = wlc_send_addba_req(wlc, scb, ini->tid,
					ampdu_tx_cfg->ba_tx_wsize, ampdu_tx_cfg->ba_policy,
					ampdu_tx_cfg->delba_timeout);
				if (! err) {
					ini->addba_req_cnt++;
					ini->last_addba_ts = OSL_SYSUPTIME();
				} else {
					WL_AMPDU_TX(("wl%d "MACF":addba blocked: %d try(%d/%d)\n",
						wlc->pub->unit, ETHER_TO_MACF(scb->ea), err,
						ini->addba_req_cnt, ampdu_addba_req_retry));
				}
			}
			WLCNTINCR(ampdu_tx->cnt->txaddbareq);
			AMPDUSCBCNTINCR(scb_ampdu->cnt.txaddbareq);
		}
	}
}
void wlc_ampdu_check_pending_ba_for_bsscfg(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	struct scb *scb;
	struct scb_iter scbiter;

	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
		if (!SCB_AMPDU(scb))
			continue;
		wlc_ampdu_check_pending_ba_for_scb(wlc, scb);
	}
}

void wlc_ampdu_check_pending_ba_for_scb(wlc_info_t *wlc, struct scb *scb)
{
	uint8 tid;
	ampdu_tx_info_t *ampdu_tx = wlc->ampdu_tx;
	scb_ampdu_tx_t *scb_ampdu;
	scb_ampdu_tid_ini_t *ini;

	if (!SCB_AMPDU(scb))
		return;
	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	for (tid = 0; tid < AMPDU_MAX_SCB_TID; tid++) {
		ini = scb_ampdu->ini[tid];
		if (!ini ||
			(ini->ba_state != AMPDU_TID_STATE_BA_PENDING_ON))
			continue;
		wlc_ampdu_retry_ba_session(wlc, scb, ini);
	}
}

/**
 * After this side has send a DELBA to the remote party, it waits for the remote party to
 * acknowledge. In the mean time, this function is called every second to deal with that situation.
 */
static
void wlc_ampdu_ba_pending_off(wlc_info_t *wlc, struct scb *scb,
	scb_ampdu_tid_ini_t *ini, uint8 tid)
{
	ampdu_tx_info_t *ampdu_tx = wlc->ampdu_tx;
	scb_ampdu_tx_t *scb_ampdu;
	uint32 phydebug, txop, fifordy, fifordy1 = 0, fifordy2 = 0;
	osl_t *osh = wlc->osh;

	WL_TRACE(("%s\n", __FUNCTION__));

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	ASSERT(scb_ampdu);
	BCM_REFERENCE(scb_ampdu);

	phydebug = R_REG(osh, D11_PHY_DEBUG(wlc));
	txop = wlc_bmac_cca_read_counter(wlc->hw, M_CCA_TXOP_L_OFFSET(wlc),
		M_CCA_TXOP_L_OFFSET(wlc));
	if (D11REV_GE(wlc->pub->corerev, 128)) {
		fifordy = R_REG(osh, D11_AQMF_READY0(wlc));
		fifordy |= (R_REG(osh, D11_AQMF_READY1(wlc)) << 16);
		fifordy1 = R_REG(osh, D11_AQMF_READY2(wlc));
		fifordy1 |= (R_REG(osh, D11_AQMF_READY3(wlc)) << 16);
		fifordy2 = R_REG(osh, D11_AQMF_READY4(wlc));
	} else {
		if (D11REV_GE(wlc->pub->corerev, 65)) {
			fifordy = R_REG(osh, D11_AQMFifoRdy_L(wlc));
			fifordy |= (R_REG(osh, D11_AQMFifoRdy_H(wlc)) << 16);
		} else if (D11REV_GE(wlc->pub->corerev, 40)) {
			fifordy = R_REG(osh, D11_AQMFifoReady(wlc));
		} else {
			fifordy = R_REG(osh, D11_xmtfifordy(wlc));
		}
	}
	WL_ERROR(("wl%d: %s: cleaning up tid %d from poff"
		" phydebug %08x txop %08x fifordy %08x fifordy1 %08x "
		"fifordy2 %08x txnoprog_cnt %d\n",
		wlc->pub->unit, __FUNCTION__, tid, phydebug, txop, fifordy,
		fifordy1, fifordy2, ampdu_tx->txnoprog_cnt));
	BCM_REFERENCE(phydebug);
	BCM_REFERENCE(txop);
	BCM_REFERENCE(fifordy);
	BCM_REFERENCE(fifordy1);
	BCM_REFERENCE(fifordy2);
	ini->cleanup_ini_cnt++;
	WLCNTINCR(ampdu_tx->cnt->txstuck);
	AMPDUSCBCNTINCR(scb_ampdu->cnt.txstuck);
	ampdu_dump_ini(wlc, ini);
	wlc_ampdu_dbg_stats(wlc, scb, ini, wlc_twt_scb_active(wlc->twti, scb) ?
			AMPDU_INI_TWT_DEAD_TIMEOUT : AMPDU_INI_DEAD_TIMEOUT);

	if (ini->cleanup_ini_cnt >= AMPDU_INI_CLEANUP_TIMEOUT) {
		/* cleans up even if there are tx packets in transit */
		ampdu_cleanup_tid_ini(ampdu_tx, scb, tid, TRUE);
	} else { /* cleans up if there are no more tx packets in transit */
		ampdu_cleanup_tid_ini(ampdu_tx, scb, tid, FALSE);
	}
} /* wlc_ampdu_ba_pending_off */

static
void wlc_ampdu_ba_off(wlc_info_t *wlc, struct scb *scb,
	scb_ampdu_tid_ini_t *ini, uint8 tid)
{
	ampdu_tx_info_t *ampdu_tx = wlc->ampdu_tx;
	scb_ampdu_tx_t *scb_ampdu;

	WL_TRACE(("%s\n", __FUNCTION__));

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	ASSERT(scb_ampdu);

	ini->off_cnt++;

	if (ini->off_cnt >= AMPDU_INI_OFF_TIMEOUT) {
#ifdef WL_AMDPU_BAOFF_LMT
		/* Limited to a single re-attempt */
		if (ini->off_cnt++ < 2*AMPDU_INI_OFF_TIMEOUT) {
			ini = wlc_ampdu_init_tid_ini(ampdu_tx, scb_ampdu, tid, FALSE);
			/* make a single attempt so reload addba_req_cnt and off_cnt  */
			if (ini) {
				ini->addba_req_cnt = AMPDU_ADDBA_REQ_RETRY_CNT;
				ini->off_cnt = 2*AMPDU_INI_OFF_TIMEOUT;
			}
		} else {
			/* Transition into BA_PENDING_OFF, derstroy the ini */
			wlc_ampdu_tx_send_delba(ampdu_tx, scb, tid, TRUE, DOT11_RC_TIMEOUT);
		}
#else
		ini = wlc_ampdu_init_tid_ini(ampdu_tx, scb_ampdu,
			tid, FALSE);
		/* make a single attempt only */
		if (ini)
			ini->addba_req_cnt = AMPDU_ADDBA_REQ_RETRY_CNT;
#endif /* WL_AMDPU_BAOFF_LMT */
	}
}

/** handle AMPDU related iovars */
static int
wlc_ampdu_doiovar(void *hdl, uint32 actionid,
	void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif)
{
	ampdu_tx_info_t *ampdu_tx = (ampdu_tx_info_t *)hdl;
	ampdu_tx_config_t *ampdu_tx_cfg = ampdu_tx->config;
	int32 int_val = 0;
	int32 *ret_int_ptr = (int32 *) a;
	bool bool_val;
	int err = 0;
	wlc_info_t *wlc;
	wlc_bsscfg_t *bsscfg;

	BCM_REFERENCE(alen);
	BCM_REFERENCE(vsize);
	BCM_REFERENCE(wlcif);

	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	bool_val = (int_val != 0) ? TRUE : FALSE;
	wlc = ampdu_tx->wlc;
	ASSERT(ampdu_tx == wlc->ampdu_tx);

	if (ampdu_tx->txaggr_support == FALSE) {
		WL_OID(("wl%d: %s: ampdu_tx->txaggr_support is FALSE\n",
			wlc->pub->unit, __FUNCTION__));
		return BCME_UNSUPPORTED;
	}

	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	switch (actionid) {
	case IOV_GVAL(IOV_AMPDU_TX):
		*ret_int_ptr = (int32)wlc->pub->_ampdu_tx;
		break;

	case IOV_SVAL(IOV_AMPDU_TX):
		return wlc_ampdu_tx_set(ampdu_tx, bool_val);

	case IOV_GVAL(IOV_AMPDU_TID): {
		struct ampdu_tid_control *ampdu_tid = (struct ampdu_tid_control *)p;

		if (ampdu_tid->tid >= AMPDU_MAX_SCB_TID) {
			err = BCME_BADARG;
			break;
		}
		ampdu_tid->enable = ampdu_tx_cfg->ini_enable[ampdu_tid->tid];
		bcopy(ampdu_tid, a, sizeof(*ampdu_tid));
		break;
		}

	case IOV_SVAL(IOV_AMPDU_TID): {
		struct ampdu_tid_control *ampdu_tid = (struct ampdu_tid_control *)a;

		if (ampdu_tid->tid >= AMPDU_MAX_SCB_TID) {
			err = BCME_BADARG;
			break;
		}
		ampdu_tx_cfg->ini_enable[ampdu_tid->tid] = ampdu_tid->enable ? TRUE : FALSE;
		break;
		}

	case IOV_GVAL(IOV_AMPDU_TX_DENSITY):
		*ret_int_ptr = (int32)ampdu_tx_cfg->mpdu_density;
		break;

	case IOV_SVAL(IOV_AMPDU_TX_DENSITY):
		if (int_val > AMPDU_MAX_MPDU_DENSITY) {
			err = BCME_RANGE;
			break;
		}

		if (int_val < AMPDU_DEF_MPDU_DENSITY) {
			err = BCME_RANGE;
			break;
		}
		wlc_ampdu_tx_set_mpdu_density(ampdu_tx, (uint8)int_val);
		break;

	case IOV_SVAL(IOV_AMPDU_SEND_ADDBA):
	{
		struct ampdu_ea_tid *ea_tid = (struct ampdu_ea_tid *)a;
		struct scb *scb;
		scb_ampdu_tx_t *scb_ampdu;

		if (!AMPDU_ENAB(wlc->pub) || (ea_tid->tid >= AMPDU_MAX_SCB_TID)) {
			err = BCME_BADARG;
			break;
		}

		if (!(scb = wlc_scbfind(wlc, bsscfg, &ea_tid->ea))) {
			err = BCME_NOTFOUND;
			break;
		}

		scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
		if (!scb_ampdu || !SCB_AMPDU(scb)) {
			err = BCME_UNSUPPORTED;
			break;
		}

		if (scb_ampdu->ini[ea_tid->tid]) {
			err = BCME_NOTREADY;
			break;
		}

		if (!wlc_ampdu_init_tid_ini(ampdu_tx, scb_ampdu, ea_tid->tid, TRUE)) {
			err = BCME_ERROR;
			break;
		}

		break;
	}

	case IOV_SVAL(IOV_AMPDU_SEND_DELBA):
	{
		struct ampdu_ea_tid *ea_tid = (struct ampdu_ea_tid *)a;
		struct scb *scb;
		scb_ampdu_tx_t *scb_ampdu;

		if (!AMPDU_ENAB(wlc->pub) || (ea_tid->tid >= AMPDU_MAX_SCB_TID)) {
			err = BCME_BADARG;
			break;
		}

		if (!(scb = wlc_scbfind(wlc, bsscfg, &ea_tid->ea))) {
			err = BCME_NOTFOUND;
			break;
		}

		scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
		if (!scb_ampdu || !SCB_AMPDU(scb)) {
			err = BCME_UNSUPPORTED;
			break;
		}

		wlc_ampdu_tx_send_delba(ampdu_tx, scb, ea_tid->tid,
			(ea_tid->initiator == 0 ? FALSE : TRUE),
			DOT11_RC_BAD_MECHANISM);

		break;
	}

	case IOV_GVAL(IOV_AMPDU_MANUAL_MODE):
		*ret_int_ptr = (int32)ampdu_tx_cfg->manual_mode;
		break;

	case IOV_SVAL(IOV_AMPDU_MANUAL_MODE):
		ampdu_tx_cfg->manual_mode = (uint8)int_val;
		break;

#ifdef WLOVERTHRUSTER
	case IOV_SVAL(IOV_AMPDU_TCP_ACK_RATIO):
		wlc_ampdu_tx_set_tcp_ack_ratio(ampdu_tx, (uint8)int_val);
		break;
	case IOV_GVAL(IOV_AMPDU_TCP_ACK_RATIO):
		*ret_int_ptr = (uint32)ampdu_tx->tcp_ack_info.tcp_ack_ratio;
		break;
	case IOV_SVAL(IOV_AMPDU_TCP_ACK_RATIO_DEPTH):
		ampdu_tx->tcp_ack_info.tcp_ack_ratio_depth = (uint8)int_val;
		break;
	case IOV_GVAL(IOV_AMPDU_TCP_ACK_RATIO_DEPTH):
		*ret_int_ptr = (uint32)ampdu_tx->tcp_ack_info.tcp_ack_ratio_depth;
		break;
#endif /* WLOVERTHRUSTER */

	case IOV_GVAL(IOV_AMPDU_MPDU):
		*ret_int_ptr = (int32)ampdu_tx_cfg->max_pdu;
		break;

	case IOV_SVAL(IOV_AMPDU_MPDU):
		if ((!int_val) || (int_val > AMPDU_MAX_MPDU)) {
			err = BCME_RANGE;
			break;
		}
		ampdu_tx_cfg->max_pdu = (int8)int_val;
		scb_ampdu_update_config_all(ampdu_tx);
		break;

#ifdef AMPDU_NON_AQM
#if defined(BCMDBG) || defined(WL_EXPORT_AMPDU_RETRY)
	case IOV_GVAL(IOV_AMPDU_RETRY_LIMIT_TID):
	{
		struct ampdu_retry_tid *retry_tid = (struct ampdu_retry_tid *)p;

		if (retry_tid->tid >= AMPDU_MAX_SCB_TID) {
			err = BCME_BADARG;
			break;
		}

		retry_tid->retry = ampdu_tx_cfg->retry_limit_tid[retry_tid->tid];
		bcopy(retry_tid, a, sizeof(*retry_tid));
		break;
	}

	case IOV_SVAL(IOV_AMPDU_RETRY_LIMIT_TID):
	{
		struct ampdu_retry_tid *retry_tid = (struct ampdu_retry_tid *)a;

		if (retry_tid->tid >= AMPDU_MAX_SCB_TID ||
			retry_tid->retry > AMPDU_MAX_RETRY_LIMIT) {
			err = BCME_RANGE;
			break;
		}

		ampdu_tx_cfg->retry_limit_tid[retry_tid->tid] = retry_tid->retry;
		break;
	}

	case IOV_GVAL(IOV_AMPDU_RR_RETRY_LIMIT_TID):
	{
		struct ampdu_retry_tid *retry_tid = (struct ampdu_retry_tid *)p;

		if (retry_tid->tid >= AMPDU_MAX_SCB_TID) {
			err = BCME_BADARG;
			break;
		}

		retry_tid->retry = ampdu_tx_cfg->rr_retry_limit_tid[retry_tid->tid];
		bcopy(retry_tid, a, sizeof(*retry_tid));
		break;
	}

	case IOV_SVAL(IOV_AMPDU_RR_RETRY_LIMIT_TID):
	{
		struct ampdu_retry_tid *retry_tid = (struct ampdu_retry_tid *)a;

		if (retry_tid->tid >= AMPDU_MAX_SCB_TID ||
			retry_tid->retry > AMPDU_MAX_RETRY_LIMIT) {
			err = BCME_RANGE;
			break;
		}

		ampdu_tx_cfg->rr_retry_limit_tid[retry_tid->tid] = retry_tid->retry;
		break;
	}
#endif /* defined(BCMDBG) || defined (WL_EXPORT_AMPDU_RETRY) */

#ifdef BCMDBG
	case IOV_GVAL(IOV_AMPDU_FFPLD):
		wlc_ffpld_show_noaqm(ampdu_tx);
		*ret_int_ptr = 0;
		break;
#endif /* BCMDBG */

#endif /* AMPDU_NON_AQM */
	case IOV_GVAL(IOV_AMPDUMAC):
		*ret_int_ptr = (int32)wlc->pub->_ampdumac;
		break;

	case IOV_GVAL(IOV_AMPDU_AGGMODE):
		*ret_int_ptr = (int32)ampdu_tx_cfg->ampdu_aggmode;
		break;

	case IOV_SVAL(IOV_AMPDU_AGGMODE):
		if (int_val != AMPDU_AGGMODE_AUTO &&
		    int_val != AMPDU_AGGMODE_HOST &&
		    int_val != AMPDU_AGGMODE_MAC) {
			err = BCME_RANGE;
			break;
		}
		ampdu_tx_cfg->ampdu_aggmode = (int8)int_val;
		wlc_ampdu_tx_set(ampdu_tx, wlc->pub->_ampdu_tx);
		break;

	case IOV_GVAL(IOV_AMPDU_FRAMEBURST_OVR):
		*ret_int_ptr = ampdu_tx_cfg->fb_override_enable;
		break;

	case IOV_SVAL(IOV_AMPDU_FRAMEBURST_OVR):
		ampdu_tx_cfg->fb_override_enable = bool_val;
#ifdef FRAMEBURST_RTSCTS_PER_AMPDU
		if (!ampdu_tx_cfg->fb_override_enable) {
			/* Clear RTS/CTS per ampdu flag */
			wlc_mhf(wlc, MHF4, MHF4_RTS_INFB, FALSE, WLC_BAND_ALL);
		}
#endif /* FRAMEBURST_RTSCTS_PER_AMPDU */
		break;

#ifdef BCMDBG
	case IOV_SVAL(IOV_AMPDU_TXQ_PROFILING_START):
		/* one shot mode, stop once circular buffer is full */
		wlc_ampdu_txq_prof_enab();
		break;

	case IOV_SVAL(IOV_AMPDU_TXQ_PROFILING_DUMP):
		wlc_ampdu_txq_prof_print_histogram(-1);
		break;

	case IOV_SVAL(IOV_AMPDU_TXQ_PROFILING_SNAPSHOT): {
		int tmp = _txq_prof_enab;
		WLC_AMPDU_TXQ_PROF_ADD_ENTRY(ampdu_tx->wlc, NULL);
		wlc_ampdu_txq_prof_print_histogram(1);
		_txq_prof_enab = tmp;
	}
		break;
#endif /* BCMDBG */

#ifdef WLATF
		case IOV_GVAL(IOV_AMPDU_ATF_US):
			*ret_int_ptr = ampdu_tx_cfg->txq_time_allowance_us;
			break;

		case IOV_SVAL(IOV_AMPDU_ATF_US):
			wlc_ampdu_atf_set_default_release_time(ampdu_tx, wlc->scbstate, int_val);
			ampdu_tx_cfg->txq_time_allowance_us = int_val;
			break;

		case IOV_SVAL(IOV_AMPDU_ATF_MIN_US):
			wlc_ampdu_atf_set_default_release_mintime(ampdu_tx, wlc->scbstate, int_val);
			ampdu_tx_cfg->txq_time_min_allowance_us = int_val;
			break;

		case IOV_GVAL(IOV_AMPDU_ATF_MIN_US):
			*ret_int_ptr = ampdu_tx_cfg->txq_time_min_allowance_us;
			break;

#if defined(BCMDBG) || defined(BCMDBG_AMPDU)
		case IOV_GVAL(IOV_AMPDU_ATF_LOWAT):
			*ret_int_ptr = (int32)ampdu_tx_cfg->ampdu_atf_lowat_rel_cnt;
			break;

		case IOV_SVAL(IOV_AMPDU_ATF_LOWAT):
			ampdu_tx_cfg->ampdu_atf_lowat_rel_cnt = int_val;
			break;
#endif /* defined(BCMDBG) || defined(BCMDBG_AMPDU) */
#endif /* WLATF */

	case IOV_GVAL(IOV_AMPDU_RELEASE):
		*ret_int_ptr = (uint32)ampdu_tx_cfg->release;
		break;

	case IOV_SVAL(IOV_AMPDU_RELEASE):
		ampdu_tx_cfg->release = (int8)int_val;
		break;

	case IOV_GVAL(IOV_AMPDU_TXAGGR):
	{
		struct ampdu_aggr *txaggr = p;
		bsscfg_ampdu_tx_t *bsscfg_ampdu = BSSCFG_AMPDU_TX_CUBBY(ampdu_tx, bsscfg);
		bzero(txaggr, sizeof(*txaggr));
		txaggr->aggr_override = bsscfg_ampdu->txaggr_override;
		txaggr->enab_TID_bmap = bsscfg_ampdu->txaggr_TID_bmap;
		bcopy(txaggr, a, sizeof(*txaggr));
		break;
	}
	case IOV_SVAL(IOV_AMPDU_TXAGGR):
	{
		struct ampdu_aggr *txaggr = a;
		uint16  enable_TID_bmap =
			(txaggr->enab_TID_bmap & txaggr->conf_TID_bmap) & AMPDU_ALL_TID_BITMAP;
		uint16  disable_TID_bmap =
			((~txaggr->enab_TID_bmap) & txaggr->conf_TID_bmap) & AMPDU_ALL_TID_BITMAP;

		if (enable_TID_bmap) {
			wlc_ampdu_tx_set_bsscfg_aggr(ampdu_tx, bsscfg, ON, enable_TID_bmap);
		}
		if (disable_TID_bmap) {
			wlc_ampdu_tx_set_bsscfg_aggr(ampdu_tx, bsscfg, OFF, disable_TID_bmap);
		}
		break;
	}
	case IOV_GVAL(IOV_AMPDU_ADDBA_TIMEOUT):
		*ret_int_ptr = (uint32)ampdu_tx_cfg->addba_retry_timeout;
		break;

	case IOV_SVAL(IOV_AMPDU_ADDBA_TIMEOUT):
		ampdu_tx_cfg->addba_retry_timeout = (int16)int_val;
		break;

#ifdef IQPLAY_DEBUG
	case IOV_SVAL(IOV_AMPDU_SET_SEQNUMBER):
		wlc_set_sequmber(wlc, (uint16)int_val);
		OSL_DELAY(30000);
		break;
#endif /* IQPLAY_DEBUG */

#if (defined(BCMDBG) || defined(TUNE_FBOVERRIDE))
	case IOV_SVAL(IOV_DYNFB_RTSPER_ON):
		ampdu_tx_cfg->dyn_fb_rtsper_on = (uint8) int_val;
		break;
	case IOV_GVAL(IOV_DYNFB_RTSPER_ON):
		*ret_int_ptr = ampdu_tx_cfg->dyn_fb_rtsper_on;
		break;
	case IOV_SVAL(IOV_DYNFB_RTSPER_OFF):
		ampdu_tx_cfg->dyn_fb_rtsper_off = (uint8) int_val;
		break;
	case IOV_GVAL(IOV_DYNFB_RTSPER_OFF):
		*ret_int_ptr = ampdu_tx_cfg->dyn_fb_rtsper_off;
		break;
#endif // endif
	default:
		err = BCME_UNSUPPORTED;
	}

	return err;
} /* wlc_ampdu_doiovar */

/** Minimal spacing between transmit MPDU's in an AMPDU, used to avoid overflow at the receiver */
void
wlc_ampdu_tx_set_mpdu_density(ampdu_tx_info_t *ampdu_tx, uint8 mpdu_density)
{
	wlc_info_t *wlc = ampdu_tx->wlc;
	ampdu_tx_config_t *ampdu_tx_cfg = ampdu_tx->config;
	struct scb *scb;
	struct scb_iter scbiter;

	ampdu_tx_cfg->mpdu_density = mpdu_density;

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (SCB_AMPDU(scb)) {
			scb_ampdu_tx_t *scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
			uint8 peer_density;
			uint32 max_rxlen;
			uint8 max_rxlen_factor;

			wlc_ht_get_scb_ampdu_params(wlc->hti, scb, &peer_density,
				&max_rxlen, &max_rxlen_factor);

			scb_ampdu->mpdu_density = MAX(peer_density, ampdu_tx_cfg->mpdu_density);
			/* should call after mpdu_density is init'd */
			wlc_ampdu_init_min_lens(scb_ampdu);
		}
	}
	if (RATELINKMEM_ENAB(wlc->pub)) {
		wlc_ratelinkmem_update_link_entry_all(wlc, NULL, TRUE, FALSE);
	}
	if (WLC_TXC_ENAB(wlc)) {
		wlc_txc_inv_all(wlc->txc);
	}
}

/** Limits max outstanding transmit PDU's */
void
wlc_ampdu_tx_set_ba_tx_wsize(ampdu_tx_info_t *ampdu_tx, uint8 wsize)
{
	ampdu_tx->config->ba_tx_wsize = wsize; /* tx ba window size (in pdu) */
}

uint8
wlc_ampdu_tx_get_ba_tx_wsize(ampdu_tx_info_t *ampdu_tx)
{
	return (ampdu_tx->config->ba_tx_wsize);
}

uint8
wlc_ampdu_tx_get_ba_max_tx_wsize(ampdu_tx_info_t *ampdu_tx)
{
	return (ampdu_tx->config->ba_max_tx_wsize);
}

uint8
wlc_ampdu_get_txpkt_weight(ampdu_tx_info_t *ampdu_tx)
{
	return (ampdu_tx->config->txpkt_weight);
}

#ifdef AMPDU_NON_AQM

/**
 * Non AQM only, PR37664 : avoids D11 tx underflow by initializing preloading parameters and tuning
 */
static void
wlc_ffpld_init_noaqm(ampdu_tx_config_t *ampdu_tx_cfg)
{
	int i, j;
	wlc_fifo_info_t *fifo;

	for (j = 0; j < NUM_FFPLD_FIFO; j++) {
		fifo = (ampdu_tx_cfg->fifo_tb + j);
		fifo->ampdu_pld_size = 0;
		for (i = 0; i <= AMPDU_HT_MCS_LAST_EL; i++)
			fifo->mcs2ampdu_table[i] = 255;
		fifo->dmaxferrate = 0;
		fifo->accum_txampdu = 0;
		fifo->prev_txfunfl = 0;
		fifo->accum_txfunfl = 0;

	}
}

/**
 * Non AQM only, called when the d11 core signals tx completion, and the tx completion status
 * indicates that a tx underflow condition occurred.
 *
 * Evaluate the dma transfer rate using the tx underflows as feedback.
 * If necessary, increase tx fifo preloading. If not enough,
 * decrease maximum ampdu_tx size for each mcs till underflows stop
 * Return 1 if pre-loading not active, -1 if not an underflow event,
 * 0 if pre-loading module took care of the event.
 */
static int
wlc_ffpld_check_txfunfl_noaqm(wlc_info_t *wlc, int fid, wlc_bsscfg_t *bsscfg)
{
	ampdu_tx_info_t *ampdu_tx = wlc->ampdu_tx;
	uint32 phy_rate;
	ratespec_t max_rspec;
	uint32 txunfl_ratio;
	uint8  max_mpdu;
	uint32 current_ampdu_cnt = 0;
	uint16 max_pld_size;
	uint32 new_txunfl;
	wlc_fifo_info_t *fifo = (ampdu_tx->config->fifo_tb + fid);
	uint xmtfifo_sz;
	uint16 cur_txunfl;

	/* return if we got here for a different reason than underflows */
	cur_txunfl = wlc_read_shm(wlc, MACSTAT_ADDR(wlc, MCSTOFF_TXFUNFL + fid));
	new_txunfl = (uint16)(cur_txunfl - fifo->prev_txfunfl);
	if (new_txunfl == 0) {
		WL_FFPLD(("check_txunfl : TX status FRAG set but no tx underflows\n"));
		return -1;
	}
	fifo->prev_txfunfl = cur_txunfl;

	if (!ampdu_tx->config->tx_max_funl)
		return 1;

	/* check if fifo is big enough */
	ASSERT(fid < NFIFO_LEGACY);
	xmtfifo_sz = wlc->xmtfifo_szh[fid];

	if ((TXFIFO_SIZE_UNIT(wlc->pub->corerev) * (uint32)xmtfifo_sz) <=
	    ampdu_tx->config->ffpld_rsvd)
		return 1;

	max_rspec = wlc_get_current_highest_rate(bsscfg);
	phy_rate = RSPEC2KBPS(max_rspec);

	max_pld_size =
	        TXFIFO_SIZE_UNIT(wlc->pub->corerev) * xmtfifo_sz - ampdu_tx->config->ffpld_rsvd;
#ifdef WLCNT
	if (AMPDU_MAC_ENAB(wlc->pub) && !AMPDU_AQM_ENAB(wlc->pub))
		ampdu_tx->cnt->txampdu = wlc_read_shm(wlc, MACSTAT_ADDR(wlc, MCSTOFF_TXAMPDU));
	current_ampdu_cnt = ampdu_tx->cnt->txampdu - fifo->prev_txampdu;
#endif /* WLCNT */
	fifo->accum_txfunfl += new_txunfl;

	/* we need to wait for at least 10 underflows */
	if (fifo->accum_txfunfl < 10)
		return 0;

	WL_FFPLD(("ampdu_count %d  tx_underflows %d\n",
		current_ampdu_cnt, fifo->accum_txfunfl));

	/*
	   compute the current ratio of tx unfl per ampdu.
	   When the current ampdu count becomes too
	   big while the ratio remains small, we reset
	   the current count in order to not
	   introduce too big of a latency in detecting a
	   large amount of tx underflows later.
	*/

	txunfl_ratio = current_ampdu_cnt/fifo->accum_txfunfl;

	if (txunfl_ratio > ampdu_tx->config->tx_max_funl) {
		if (current_ampdu_cnt >= FFPLD_MAX_AMPDU_CNT) {
#ifdef WLCNT
			fifo->prev_txampdu = ampdu_tx->cnt->txampdu;
#endif // endif
			fifo->accum_txfunfl = 0;
		}
		return 0;
	}
	max_mpdu = MIN(fifo->mcs2ampdu_table[AMPDU_MAX_MCS], ampdu_tx->config->default_pdu);

	/* In case max value max_pdu is already lower than
	   the fifo depth, there is nothing more we can do.
	*/
	/* XXX : if max_mpdu happens to fit entirely inside the fifo
	 * we should assume that tx underflows are due to higher ampdu values for lower
	 * mcs than MAX_MCS. For now, just declare us done if it happens. TO BE FIXED.
	 */

	if (fifo->ampdu_pld_size >= max_mpdu * FFPLD_MPDU_SIZE) {
		WL_FFPLD(("tx fifo pld : max ampdu fits in fifo\n)"));
#ifdef WLCNT
		fifo->prev_txampdu = ampdu_tx->cnt->txampdu;
#endif // endif
		fifo->accum_txfunfl = 0;
		return 0;
	}

	if (fifo->ampdu_pld_size < max_pld_size) {

		/* increment by TX_FIFO_PLD_INC bytes */
		fifo->ampdu_pld_size += FFPLD_PLD_INCR;
		if (fifo->ampdu_pld_size > max_pld_size)
			fifo->ampdu_pld_size = max_pld_size;

		/* update scb release size */
		scb_ampdu_update_config_all(ampdu_tx);

		/*
		   compute a new dma xfer rate for max_mpdu @ max mcs.
		   This is the minimum dma rate that
		   can acheive no unferflow condition for the current mpdu size.
		*/
		/* note : we divide/multiply by 100 to avoid integer overflows */
		fifo->dmaxferrate =
		        (((phy_rate/100)*(max_mpdu*FFPLD_MPDU_SIZE-fifo->ampdu_pld_size))
		         /(max_mpdu * FFPLD_MPDU_SIZE))*100;

		WL_FFPLD(("DMA estimated transfer rate %d; pre-load size %d\n",
		          fifo->dmaxferrate, fifo->ampdu_pld_size));
	} else {

		/* decrease ampdu size */
		if (fifo->mcs2ampdu_table[AMPDU_MAX_MCS] > 1) {
			if (fifo->mcs2ampdu_table[AMPDU_MAX_MCS] == 255)
				fifo->mcs2ampdu_table[AMPDU_MAX_MCS] =
					ampdu_tx->config->default_pdu - 1;
			else
				fifo->mcs2ampdu_table[AMPDU_MAX_MCS] -= 1;

			/* recompute the table */
			wlc_ffpld_calc_mcs2ampdu_table_noaqm(ampdu_tx, fid, bsscfg);

			/* update scb release size */
			scb_ampdu_update_config_all(ampdu_tx);
		}
	}
#ifdef WLCNT
	fifo->prev_txampdu = ampdu_tx->cnt->txampdu;
#endif // endif
	fifo->accum_txfunfl = 0;
	return 0;
} /* wlc_ffpld_check_txfunfl_noaqm */

/** non AQM only, tx underflow related */
static void
wlc_ffpld_calc_mcs2ampdu_table_noaqm(ampdu_tx_info_t *ampdu_tx, int f, wlc_bsscfg_t *bsscfg)
{
	int i;
	uint32 phy_rate, dma_rate, tmp;
	ratespec_t max_rspec;
	uint8 max_mpdu;
	wlc_fifo_info_t *fifo = (ampdu_tx->config->fifo_tb + f);

	/* recompute the dma rate */
	/* note : we divide/multiply by 100 to avoid integer overflows */
	max_mpdu = MIN(fifo->mcs2ampdu_table[AMPDU_MAX_MCS], ampdu_tx->config->default_pdu);
	max_rspec = wlc_get_current_highest_rate(bsscfg);
	phy_rate = RSPEC2KBPS(max_rspec);
	dma_rate = (((phy_rate/100) * (max_mpdu*FFPLD_MPDU_SIZE-fifo->ampdu_pld_size))
	         /(max_mpdu*FFPLD_MPDU_SIZE))*100;
	fifo->dmaxferrate = dma_rate;

	/* fill up the mcs2ampdu table; do not recalc the last mcs */
	dma_rate = dma_rate >> 7;

#if defined(WLPROPRIETARY_11N_RATES)
	/* iterate through both standard and prop ht rates */
	for (i = NEXT_MCS(-1); i <= WLC_11N_LAST_PROP_MCS; i = NEXT_MCS(i))
#else
	for (i = 0; i < AMPDU_MAX_MCS; i++)
#endif /* WLPROPRIETARY_11N_RATES */
	{
		/* shifting to keep it within integer range */
		phy_rate = MCS_RATE(i, TRUE, FALSE) >> 7;
		if (phy_rate > dma_rate) {
			tmp = ((fifo->ampdu_pld_size * phy_rate) /
				((phy_rate - dma_rate) * FFPLD_MPDU_SIZE)) + 1;
			tmp = MIN(tmp, 255);
			fifo->mcs2ampdu_table[MCS2IDX(i)] = (uint8)tmp;
		}
	}

#ifdef BCMDBG
	wlc_ffpld_show_noaqm(ampdu_tx);
#endif /* BCMDBG */
} /* wlc_ffpld_calc_mcs2ampdu_table_noaqm */

#ifdef BCMDBG
/** non AQM only */
static void
wlc_ffpld_show_noaqm(ampdu_tx_info_t *ampdu_tx)
{
	int i, j;
	wlc_fifo_info_t *fifo;

	WL_PRINT(("MCS to AMPDU tables:\n"));
	for (j = 0; j < NUM_FFPLD_FIFO; j++) {
		fifo = ampdu_tx->config->fifo_tb + j;
		WL_PRINT(("  FIFO %d : Preload settings: size %d dmarate %d kbps\n",
		          j, fifo->ampdu_pld_size, fifo->dmaxferrate));
		for (i = 0; i <= AMPDU_HT_MCS_LAST_EL; i++) {
			WL_PRINT(("  %d", fifo->mcs2ampdu_table[i]));
		}
		WL_PRINT(("\n\n"));
	}
}
#endif /* BCMDBG */
#endif /* AMPDU_NON_AQM */

#if defined(DONGLEBUILD)
/** enable or disable flow of tx packets from a higher layer to this layer to prevent overflow */
static void
wlc_ampdu_txflowcontrol(wlc_info_t *wlc, scb_ampdu_tx_t *scb_ampdu, scb_ampdu_tid_ini_t *ini)
{
	wlc_txq_info_t *qi = SCB_WLCIFP(scb_ampdu->scb)->qi;

	if (wlc_txflowcontrol_override_isset(wlc, qi, TXQ_STOP_FOR_AMPDU_FLOW_CNTRL)) {
		if ((pktq_n_pkts_tot(&scb_ampdu->txq) + ini->tx_in_transit) <=
		    wlc->pub->tunables->ampdudatahiwat) {
			wlc_txflowcontrol_override(wlc, qi, OFF, TXQ_STOP_FOR_AMPDU_FLOW_CNTRL);
		}
	} else {
		if ((pktq_n_pkts_tot(&scb_ampdu->txq) + ini->tx_in_transit) >=
		    wlc->pub->tunables->ampdudatahiwat) {
#ifdef WLTAF
			if (wlc_taf_in_use(wlc->taf_handle)) {
				return;
			}
#endif /* WLTAF */
			wlc_txflowcontrol_override(wlc, qi, ON, TXQ_STOP_FOR_AMPDU_FLOW_CNTRL);
		}
	}
}
#endif	/* DONGLEBUILD */

/**
 * enable/disable txaggr_override control.
 * AUTO: txaggr operates according to per-TID per-bsscfg control()
 * OFF: turn txaggr off for all TIDs.
 * ON: Not supported and treated the same as AUTO.
 */
void
wlc_ampdu_tx_set_bsscfg_aggr_override(ampdu_tx_info_t *ampdu_tx, wlc_bsscfg_t *bsscfg, int8 txaggr)
{
	bsscfg_ampdu_tx_t *bsscfg_ampdu = BSSCFG_AMPDU_TX_CUBBY(ampdu_tx, bsscfg);

	if (ampdu_tx->txaggr_support == FALSE) {
		/* txaggr_override should already be OFF */
		ASSERT(bsscfg_ampdu->txaggr_override == OFF);
		return;
	}

	/* txaggr_override ON would mean that tx aggregation will be allowed for all TIDs
	 * even if bsscfg_ampdu->txaggr_TID_bmap is set OFF for some TIDs.
	 * As there is no requirement of such txaggr_override ON, just treat it as AUTO.
	 */
	if (txaggr == ON) {
		txaggr = AUTO;
	}

	if (bsscfg_ampdu->txaggr_override == txaggr) {
		return;
	}

	bsscfg_ampdu->txaggr_override = txaggr;

#ifdef WLTAF
	wlc_taf_bss_state_update(ampdu_tx->wlc->taf_handle, bsscfg,
		TAF_PARAM(bsscfg_ampdu->txaggr_override), TAF_BSS_STATE_AMPDU_AGGREGATE_OVR);
#endif // endif
	if (txaggr == OFF) {
		wlc_ampdu_tx_cleanup(ampdu_tx, bsscfg, AMPDU_ALL_TID_BITMAP);
	}
}

/** returns a bitmap */
uint16
wlc_ampdu_tx_get_bsscfg_aggr(ampdu_tx_info_t *ampdu_tx, wlc_bsscfg_t *bsscfg)
{
	bsscfg_ampdu_tx_t *bsscfg_ampdu = BSSCFG_AMPDU_TX_CUBBY(ampdu_tx, bsscfg);

	if (ampdu_tx->txaggr_support == FALSE) {
		/* txaggr should be OFF for all TIDs */
		ASSERT(bsscfg_ampdu->txaggr_TID_bmap == 0);
	}
	return (bsscfg_ampdu->txaggr_TID_bmap & AMPDU_ALL_TID_BITMAP);
}

/** Configure ampdu tx aggregation per-TID and per-bsscfg */
void
wlc_ampdu_tx_set_bsscfg_aggr(ampdu_tx_info_t *ampdu_tx, wlc_bsscfg_t *bsscfg,
	bool txaggr, uint16 conf_TID_bmap)
{
	bsscfg_ampdu_tx_t *bsscfg_ampdu = BSSCFG_AMPDU_TX_CUBBY(ampdu_tx, bsscfg);

	if (ampdu_tx->txaggr_support == FALSE) {
		/* txaggr should already be OFF for all TIDs,
		 * and do not set txaggr_TID_bmap.
		 */
		ASSERT(bsscfg_ampdu->txaggr_TID_bmap == 0);
		return;
	}

	if (txaggr == ON) {
		bsscfg_ampdu->txaggr_TID_bmap |= (conf_TID_bmap & AMPDU_ALL_TID_BITMAP);
#ifdef WLTAF
		wlc_taf_bss_state_update(ampdu_tx->wlc->taf_handle, bsscfg,
			TAF_PARAM(bsscfg_ampdu->txaggr_TID_bmap),
			TAF_BSS_STATE_AMPDU_AGGREGATE_TID);
#endif // endif
	} else {
		uint16 stateChangedTID = bsscfg_ampdu->txaggr_TID_bmap;
		bsscfg_ampdu->txaggr_TID_bmap &= ((~conf_TID_bmap) & AMPDU_ALL_TID_BITMAP);
		stateChangedTID ^= bsscfg_ampdu->txaggr_TID_bmap;
		stateChangedTID &= AMPDU_ALL_TID_BITMAP;

#ifdef WLTAF
		wlc_taf_bss_state_update(ampdu_tx->wlc->taf_handle, bsscfg,
			TAF_PARAM(bsscfg_ampdu->txaggr_TID_bmap),
			TAF_BSS_STATE_AMPDU_AGGREGATE_TID);
#endif // endif
		/* Override should have higher priority if not AUTO */
		if (bsscfg_ampdu->txaggr_override == AUTO && stateChangedTID) {
			wlc_ampdu_tx_cleanup(ampdu_tx, bsscfg, stateChangedTID);
		}
	}
}

/**
 * Enable A-MPDU aggregation for the specified remote party ('scb').
 *
 * This function is used to enable the Tx A-MPDU aggregation path for a given SCB
 * and is called by the AMPDU common config code.
 */
void
wlc_ampdu_tx_scb_enable(ampdu_tx_info_t *ampdu_tx, struct scb *scb)
{
	wlc_info_t *wlc = ampdu_tx->wlc;

#ifdef WLCFP
	/**
	 * CFP states have a tight dependcy on SCB AMPDU capability
	 * Update the CFP states  along with AMPDU updates
	 */
	wlc_cfp_scb_state_upd(wlc->cfp, scb);
#endif /* WLCFP */
	/* AMPDU inserts a TxModule to process and queue ampdu packets to an SCB ampdu queue
	 * before releasing the packets onto the driver TxQ. Then as the TxQ is processed by
	 * wlc_pull_q() (formerly wlc_sendq()), WPF_AMPDU_MPDU tagged packets were processed by
	 * wlc_sendampdu(). This function configures the TXMOD_AMPDU for the given SCB to enable
	 * the AMPDU path.
	 */

	/* to enable AMPDU, configure the AMPDU TxModule for this SCB */
	wlc_txmod_config(wlc->txmodi, scb, TXMOD_AMPDU);

	/* When packet chaining is enabled then AMPDU will take care of AMSDU. So take AMSDU
	 * out of the txmod path.
	 */
	if (AMSDU_TX_ENAB(wlc->pub) && PKTC_ENAB(wlc->pub) && PKTC_TX_ENAB(wlc->pub) &&
	    (scb->flags & SCB_AMSDUCAP)) {
		wlc_txmod_unconfig(wlc->txmodi, scb, TXMOD_AMSDU);
	}
#ifdef WLTAF
	/* Update TAF state */
	wlc_taf_scb_state_update(wlc->taf_handle, scb, TAF_AMPDU, TAF_SCBSTATE_SOURCE_ENABLE);
#endif // endif
} /* wlc_ampdu_tx_scb_enable */

/**
 * Disable A-MPDU aggregation for the specified remote party 'scb'.
 *
 * This function is used to disable the Tx A-MPDU aggregation path for a given SCB
 * and is called by the AMPDU common config code.
 */
void
wlc_ampdu_tx_scb_disable(ampdu_tx_info_t *ampdu_tx, struct scb *scb)
{
	wlc_info_t *wlc = ampdu_tx->wlc;

#ifdef WLTAF
	/* Update TAF state */
	wlc_taf_scb_state_update(wlc->taf_handle, scb, TAF_AMPDU, TAF_SCBSTATE_SOURCE_DISABLE);
#endif // endif
#ifdef WLCFP
	/**
	 * CFP states have a  tight dependcy on SCB AMPDU capability
	 * Update the CFP states  along with AMPDU updates
	 */
	wlc_cfp_scb_state_upd(wlc->cfp, scb);
#endif /* WLCFP */

	/* When packet chaining is enabled, and AMSDU is enabled then AMSDU should be inserted
	 * in the txmod path. It was disabled because AMPDU deals with AMSDU when PKTC
	 * is enabled.
	 */
	if (AMSDU_TX_ENAB(wlc->pub) && PKTC_ENAB(wlc->pub) && PKTC_TX_ENAB(wlc->pub) &&
	    (scb->flags & SCB_AMSDUCAP)) {
		wlc_txmod_config(wlc->txmodi, scb, TXMOD_AMSDU);
	}

	/* to disable AMPDU, unconfigure the AMPDU TxModule for this SCB */
	wlc_txmod_unconfig(wlc->txmodi, scb, TXMOD_AMPDU);
}

/*
 * Changes txaggr btcx_dur_flag global flag
 */
void
wlc_ampdu_btcx_tx_dur(wlc_info_t *wlc, bool btcx_dur_flag)
{
	ampdu_tx_config_t *ampdu_tx_cfg = wlc->ampdu_tx->config;
	ampdu_tx_cfg->btcx_dur_flag = btcx_dur_flag;
}

/**
 * Called after a higher software layer gave this AMPDU layer one or more MSDUs,
 * and the AMPDU layer put the MSDU on an AMPDU internal software queue.
 */
/* XXX SUNNY_DAY_FUNCTIONALITY:
 * - enable or disable flow of tx packets into firmware
 * - optionally releases packets from AMPDU SW internal queue into wlc/common
 *   queue. See function wlc_ampdu_txeval() for details.
 */
INLINE static void
wlc_ampdu_agg_complete(wlc_info_t *wlc, ampdu_tx_info_t *ampdu_tx,
	scb_ampdu_tx_t *scb_ampdu, scb_ampdu_tid_ini_t *ini, bool force_release)
{
	BCM_REFERENCE(wlc);

#if defined(WLTAF) && defined(WLCFP)
	if (wlc_taf_in_use(wlc->taf_handle)) {
		return;
	}
#endif // endif
	wlc_ampdu_txflowcontrol(wlc, scb_ampdu, ini);
	wlc_ampdu_txeval(ampdu_tx, scb_ampdu, ini, force_release);
}

#if defined(WLCFP) && defined(WLSQS)
/* Legacy slow ampdu release data path.
 * Used in SQS mode if CFP TCB states are not established after V2R conversion.
 * TX eval operations are already completed for these frames.
 * Try to release the real  packets as soon as possible
 */
INLINE static void
wlc_cfp_ampdu_agg_complete(wlc_info_t *wlc, ampdu_tx_info_t *ampdu_tx,
	scb_ampdu_tx_t *scb_ampdu, scb_ampdu_tid_ini_t *ini, bool force_release)
{
	struct scb *scb;
	wlc_txq_info_t *qi; /**< refers to the 'common' transmit queue */
	struct pktq *common_q;		/**< multi-priority packet queue */
	uint16 wlc_txq_avail; /**< #mpdus in common/wlc queue */
	uint16 ampdu_pktq_n_pkts = 0;

	scb = scb_ampdu->scb;

	if (!wlc_scb_restrict_can_txeval(wlc)) {
		return; /* channel switch / data block related */
	}
	ASSERT(ini->ba_state == AMPDU_TID_STATE_BA_ON);

	/* Current real packet count */
	ampdu_pktq_n_pkts = ampdu_pktqprec_n_pkts(&scb_ampdu->txq, ini->tid);

	/* In CFP mode AMPDU Qs carry packet count instead of MPDU count */
	ampdu_pktq_n_pkts = WLC_AMPDU_PKTS_TO_MPDUS(wlc, scb, ini->tid, ampdu_pktq_n_pkts);

	ASSERT(ampdu_pktq_n_pkts);

	/* Common Q availability */
	qi = SCB_WLCIFP(scb)->qi;
	common_q = WLC_GET_CQ(qi);
	wlc_txq_avail = MIN(pktq_avail(common_q),
		pktqprec_avail_pkts(common_q, WLC_PRIO_TO_PREC(ini->tid)));

#ifdef WLTAF
	if (wlc_taf_in_use(wlc->taf_handle)) {
		if (wlc_taf_schedule(wlc->taf_handle, ini->tid, scb, FALSE)) {
		return;
	}
		/* fall through to default data path if wlc_taf_schedule return FALSE */
	}
#endif // endif
	/* Finally release the packets through legacy slow path */
	wlc_ampdu_release(wlc->ampdu_tx, scb_ampdu, ini, ampdu_pktq_n_pkts, wlc_txq_avail, NULL);
}
#else /* ! (WLCFP && WLSQS) */
/* NON SQS version which triggers legacy ampdu tx eval routine */
#define wlc_cfp_ampdu_agg_complete(wlc, ampdu_tx, scb_ampdu, ini, force) \
	wlc_ampdu_agg_complete(wlc, ampdu_tx, scb_ampdu, ini, force)
#endif /* ! (WLCFP && WLSQS) */

#if (defined(PKTC) || defined(PKTC_TX_DONGLE))

/**
 * AMPDU aggregation function called through txmod
 *    @param[in] p     a chain of 802.3 ? packets
 */

/* XXX SUNNY_DAY_FUNCTIONALITY:
 * - downgrade fifo used if applicable
 * - sets up connection (ADDBA) if not yet present
 * - recognize TCP Acks and take action accordingly (WLOVERTHRUSTER)
 * - enqueues caller supplied packets onto a AMPDU SW internal queue
 * - determines if aggregated packets should be 'released' to the wlc/common
 *   queue and if so, does that.
 */
static void BCMFASTPATH
wlc_ampdu_agg_pktc(void *ctx, struct scb *scb, void *p, uint prec)
{
	ampdu_tx_info_t *ampdu_tx = (ampdu_tx_info_t *)ctx;
	scb_ampdu_tx_t *scb_ampdu;
	wlc_info_t *wlc;
	scb_ampdu_tid_ini_t *ini;
	uint8 tid;
	void *n;
	uint32 lifetime = 0;
	uint32 drop_cnt;
	bool amsdu_in_ampdu;
	bool force_release = FALSE;

	bsscfg_ampdu_tx_t *bsscfg_ampdu = BSSCFG_AMPDU_TX_CUBBY(ampdu_tx, SCB_BSSCFG(scb));
#if defined(WLAMSDU_TX)
	amsdu_info_t *ami;
#endif // endif

	ASSERT(ampdu_tx);
	wlc = ampdu_tx->wlc;

#if defined(WLAMSDU_TX)
	ami = wlc->ami;
#endif // endif

	ASSERT((PKTNEXT(wlc->osh, p) == NULL) || !PKTISCHAINED(p) || PKTISFRWDPKT(wlc->osh, p));

#if defined(WLAMSDU_TX)
	BCM_REFERENCE(ami);
#endif /* WLAMSDU_TX */

	BCM_REFERENCE(amsdu_in_ampdu);
	tid = (uint8)PKTPRIO(p);

	ASSERT(tid < AMPDU_MAX_SCB_TID);
	if (tid >= AMPDU_MAX_SCB_TID) {
		WL_AMPDU(("%s: Received tid is wrong -- returning\n", __FUNCTION__));
		goto txmod_ampdu;
	}

	/* Set packet exptime */
	lifetime = wlc->lifetime[(SCB_WME(scb) ? WME_PRIO2AC(tid) : AC_BE)];

	if ((bsscfg_ampdu->txaggr_override == OFF) ||
		(!isbitset(bsscfg_ampdu->txaggr_TID_bmap, tid)) ||
		(WLPKTTAG(p)->flags3 & WLF3_BYPASS_AMPDU)) {
		WL_AMPDU(("%s: tx agg off (txaggr ovrd %d, TID bmap 0x%x)-- returning\n",
			__FUNCTION__,
			bsscfg_ampdu->txaggr_override,
			bsscfg_ampdu->txaggr_TID_bmap));
		goto txmod_ampdu;
	}

	ASSERT(SCB_AMPDU(scb));

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	ASSERT(scb_ampdu);

	/* initialize initiator on first packet; sends addba req */
	if (!(ini = scb_ampdu->ini[tid])) {
		ini = wlc_ampdu_init_tid_ini(ampdu_tx, scb_ampdu, tid, FALSE);
		if (!ini) {
			WL_AMPDU(("%s: ini NULL -- returning\n", __FUNCTION__));
			goto txmod_ampdu;
		}
	}

	if ((ini->ba_state != AMPDU_TID_STATE_BA_ON) |
		(WLF2_PCB1(p) & WLF2_PCB1_NAR)) {
		goto txmod_ampdu;
	}

#ifdef PROP_TXSTATUS
	/* If FW packet received when suppr_window > 0, send it as non-ampdu */
	if (AMPDU_HOST_ENAB(wlc->pub) && PROP_TXSTATUS_ENAB(wlc->pub) &&
		(ini->suppr_window > 0) &&
		!(WL_TXSTATUS_GET_FLAGS(WLPKTTAG(p)->wl_hdr_information) &
		WLFC_PKTFLAG_PKTFROMHOST)) {
		goto txmod_ampdu;
	}
#endif /* PROP_TXSTATUS */

	/* using bit-wise logic here for speed optimization; only works for boolean 0/1 values */
	amsdu_in_ampdu = WLC_AMSDU_IN_AMPDU(wlc, scb, tid);

	while (1) { /* loop over each packet in the caller supplied chain */
		ASSERT(PKTISCHAINED(p) || (PKTCLINK(p) == NULL));
		ASSERT(!PKTISCFP(p));
		n = PKTCLINK(p);
		PKTSETCLINK(p, NULL); /* 'unlink' the current packet */

#ifdef PROP_TXSTATUS
		/* If Non AMPDU packet was suppresed, send it as Non AMPDU again */
		if (AMPDU_HOST_ENAB(wlc->pub) && PROP_TXSTATUS_ENAB(wlc->pub) &&
			GET_DRV_HAS_ASSIGNED_SEQ(WLPKTTAG(p)->seq) &&
			MODSUB_POW2(WL_SEQ_GET_NUM(WLPKTTAG(p)->seq), ini->start_seq, SEQNUM_MAX) >=
			ini->ba_wsize) {
			PKTCLRCHAINED(wlc->osh, p);
			if (n != NULL)
				wlc_pktc_sdu_prep(wlc, scb, p, n, lifetime);
			SCB_TX_NEXT(TXMOD_AMPDU, scb, p, prec);
			p = n;
			if (p)
				continue;
			else
				return;
		}
#endif /* PROP_TXSTATUS */

		/* Avoid fn call if the packet is too long to be an ACK */
		if (pkttotlen(wlc->osh, p) <= TCPACKSZSDU) {
#ifdef WLOVERTHRUSTER
			if (OVERTHRUST_ENAB(wlc->pub)) {
				wlc_ampdu_tcp_ack_suppress(ampdu_tx, scb_ampdu, p, tid);
			}
#else
			if (wlc_ampdu_is_tcpack(ampdu_tx, p))
				WLPKTTAG(p)->flags3 |= WLF3_DATA_TCP_ACK;
#endif // endif
		}

#ifdef PROP_TXSTATUS
		/* If any suppress pkt in the chain, force ampdu release (=push to hardware) */
		if (GET_DRV_HAS_ASSIGNED_SEQ(WLPKTTAG(p)->seq)) {
			force_release = TRUE;
		}
#endif /* PROP_TXSTATUS */

		/* If any pkt in the chain is TCPACK, force ampdu release */
		if (ampdu_tx->wlc->tcpack_fast_tx && (WLPKTTAG(p)->flags3 & WLF3_DATA_TCP_ACK))
			force_release = TRUE;

		/* Queue could overflow while walking the chain */
		if (!wlc_ampdu_prec_enq(wlc, &scb_ampdu->txq, p, tid)) {
			PKTSETCLINK(p, n);
			WLC_AMPDU_AGG_COMPLETE(wlc, ampdu_tx, scb, scb_ampdu, ini, force_release);
			break;
		}

		PKTCNTR_INC(wlc, PKTCNTR_MPDU_ENQ_SCB_AMPDU, 1);

		PKTCLRCHAINED(wlc->osh, p);
#ifdef	WLCFP
		if (CFP_ENAB(wlc->pub) == TRUE) {
			/* With CFP enabled, AMSDU agg is done at SCB ampdu release time.
			 * Enqueue the packets and return immediately
			 */
			if (n == NULL) {
				WLC_AMPDU_AGG_COMPLETE(wlc, ampdu_tx, scb, scb_ampdu,
					ini, force_release);
				return;
			} else {
				wlc_pktc_sdu_prep(wlc, scb, p, n, lifetime);
			}
		} else
#endif /* WLCFP */
		{ /* ! CFP_ENAB(wlc->pub) */
			/* Last packet in chain */
			if ((n == NULL) ||
				(wlc_pktc_sdu_prep(wlc, scb, p, n, lifetime),
				((amsdu_in_ampdu ||
#ifdef PROP_TXSTATUS
					WL_SEQ_IS_AMSDU(WLPKTTAG(p)->seq)) &&
#else
					FALSE) &&
#endif /* PROP_TXSTATUS */
#ifdef WLOVERTHRUSTER
				(OVERTHRUST_ENAB(wlc->pub) ? (
					pkttotlen(wlc->osh, p) > TCPACKSZSDU) : 1) &&
#endif // endif
#ifdef WLAMSDU_TX
				/* p - chain going to be extended with AMSDUs contained in 'n'
				 * n - chain containing MSDUs to aggregate to 'p'
				 */
				((n = wlc_amsdu_pktc_agg(ami, scb, p, n, tid, lifetime)) == NULL) &&
#endif  /* WLAMSDU_TX */
				TRUE))) {
					WLC_AMPDU_AGG_COMPLETE(wlc, ampdu_tx, scb, scb_ampdu,
						ini, force_release);
					return;
			}
		}
		p = n; /* advances to next packet in caller supplied chain */
	} /* while (packets) */

	WL_AMPDU_ERR(("wl%d: %s: txq overflow\n", wlc->pub->unit, __FUNCTION__));

	drop_cnt = 0;
	FOREACH_CHAINED_PKT(p, n) { /* part of error flow handling */
		PKTCLRCHAINED(wlc->osh, p);
		PKTFREE(wlc->osh, p, TRUE);
		WLCIFCNTINCR(scb, txnobuf);
		drop_cnt++;
	}

	if (drop_cnt) {
#ifdef PKTQ_LOG
		struct pktq *q = &scb_ampdu->txq;
		if (q->pktqlog) {
			pktq_counters_t *prec_cnt = wlc_ampdu_pktqlog_cnt(q, tid, prec);
			WLCNTCONDADD(prec_cnt, prec_cnt->dropped, drop_cnt - 1);
		}
#endif // endif
		WLCNTADD(wlc->pub->_cnt->txnobuf, drop_cnt);
		WLCNTADD(ampdu_tx->cnt->txdrop, drop_cnt);
		AMPDUSCBCNTADD(scb_ampdu->cnt.txdrop, drop_cnt);
		WLCNTSCBADD(scb->scb_stats.tx_failures, drop_cnt);
	}

	return;

txmod_ampdu:
	FOREACH_CHAINED_PKT(p, n) {
		PKTCLRCHAINED(wlc->osh, p);
		if (n != NULL) {
			wlc_pktc_sdu_prep(wlc, scb, p, n, lifetime);
		}
#ifdef WLCFP
		if (CFP_ENAB(wlc->pub) == TRUE) {
			/**
			 * Apply TX cache only for AMPDU flows
			 * Delay cache creation till ini is setup
			 */
			WLPKTTAG(p)->flags |= WLF_BYPASS_TXC;
		}
#endif /* WLCFP */

		SCB_TX_NEXT(TXMOD_AMPDU, scb, p, prec);
	}
} /* wlc_ampdu_agg_pktc */

#else /* PKTC || PKTC_TX_DONGLE */

/**
 * A higher layer calls this callback function (using the 'txmod' infrastructure) to queue an MSDU
 * on a software queue in the AMPDU subsystem (scb_ampdu->txq). The MSDU is not forwarded to the d11
 * core in this function.
 */
static void BCMFASTPATH
wlc_ampdu_agg(void *ctx, struct scb *scb, void *p, uint prec)
{
	ampdu_tx_info_t *ampdu_tx = (ampdu_tx_info_t *)ctx;
	scb_ampdu_tx_t *scb_ampdu;
	wlc_info_t *wlc;
	scb_ampdu_tid_ini_t *ini;
	uint8 tid;
	bool force_release = FALSE;
	bsscfg_ampdu_tx_t *bsscfg_ampdu = BSSCFG_AMPDU_TX_CUBBY(ampdu_tx, SCB_BSSCFG(scb));

	ASSERT(ampdu_tx);
	wlc = ampdu_tx->wlc;

	tid = (uint8)PKTPRIO(p);

	if ((bsscfg_ampdu->txaggr_override == OFF) ||
		(!isbitset(bsscfg_ampdu->txaggr_TID_bmap, tid)) ||
		(WLPKTTAG(p)->flags3 & WLF3_BYPASS_AMPDU)) {
		WL_AMPDU(("%s: tx agg off (txaggr ovrd %d, TID bmap 0x%x)-- returning\n",
			__FUNCTION__,
			bsscfg_ampdu->txaggr_override,
			bsscfg_ampdu->txaggr_TID_bmap));
		SCB_TX_NEXT(TXMOD_AMPDU, scb, p, prec);
		return;
	}

	ASSERT(tid < AMPDU_MAX_SCB_TID);
	if (tid >= AMPDU_MAX_SCB_TID) {
		SCB_TX_NEXT(TXMOD_AMPDU, scb, p, prec);
		WL_AMPDU(("%s: tid wrong -- returning\n", __FUNCTION__));
		return;
	}

	ASSERT(SCB_AMPDU(scb));
	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	ASSERT(scb_ampdu);

	/* initialize initiator on first packet; sends addba req */
	if (!(ini = scb_ampdu->ini[tid])) {
		ini = wlc_ampdu_init_tid_ini(ampdu_tx, scb_ampdu, tid, FALSE);
		if (!ini) {
			SCB_TX_NEXT(TXMOD_AMPDU, scb, p, prec);
			WL_AMPDU(("%s: ini NULL -- returning\n", __FUNCTION__));
			return;
		}
	}

	if (ini->ba_state != AMPDU_TID_STATE_BA_ON) {
		SCB_TX_NEXT(TXMOD_AMPDU, scb, p, prec);
		return;
	}

	if (WLF2_PCB1(p) & WLF2_PCB1_NAR) {
		SCB_TX_NEXT(TXMOD_AMPDU, scb, p, prec);
		return;
	}

	/* Avoid fn call if the packet is too long to be an ACK */
	if (PKTLEN(wlc->osh, p) <= TCPACKSZSDU) {
#ifdef WLOVERTHRUSTER
		if (OVERTHRUST_ENAB(wlc->pub)) {
			wlc_ampdu_tcp_ack_suppress(ampdu_tx, scb_ampdu, p, tid);
		}
#else
		if (wlc_ampdu_is_tcpack(ampdu_tx, p))
			WLPKTTAG(p)->flags3 |= WLF3_DATA_TCP_ACK;
#endif /* !WLOVERTHRUSTER */
	}

#ifdef PROP_TXSTATUS
	/* If any suppress pkt in the chain, force ampdu release */
	if (GET_DRV_HAS_ASSIGNED_SEQ(WLPKTTAG(p)->seq)) {
		force_release = TRUE;
	}
#endif /* PROP_TXSTATUS */

	/* If any TCPACK pkt, force ampdu release */
	if (ampdu_tx->wlc->tcpack_fast_tx && (WLPKTTAG(p)->flags3 & WLF3_DATA_TCP_ACK))
		force_release = TRUE;

	if (wlc_ampdu_prec_enq(ampdu_tx->wlc, &scb_ampdu->txq, p, tid)) { /* on sdu queue */
		WLC_AMPDU_AGG_COMPLETE(wlc, ampdu_tx, scb, scb_ampdu, ini, force_release);
		return;
	}

	WL_AMPDU_ERR(("wl%d: %s: txq overflow\n", wlc->pub->unit, __FUNCTION__));

	PKTFREE(wlc->osh, p, TRUE);
	WLCNTINCR(wlc->pub->_cnt->txnobuf);
	WLCNTINCR(ampdu_tx->cnt->txdrop);
	AMPDUSCBCNTINCR(scb_ampdu->cnt.txdrop);
	WLCIFCNTINCR(scb, txnobuf);
	WLCNTSCBINCR(scb->scb_stats.tx_failures);
#ifdef WL11K
	wlc_rrm_stat_qos_counter(wlc, scb, tid, OFFSETOF(rrm_stat_group_qos_t, txdrop));
	wlc_rrm_stat_qos_counter(wlc, scb, tid, OFFSETOF(rrm_stat_group_qos_t, txfail));
#endif // endif

} /* wlc_ampdu_agg */
#endif /* PKTC || PKTC_TX_DONGLE */

#ifdef WLOVERTHRUSTER

uint
wlc_ampdu_tx_get_tcp_ack_ratio(ampdu_tx_info_t *ampdu_tx)
{
	return (uint)ampdu_tx->tcp_ack_info.tcp_ack_ratio;
}

static void
wlc_ampdu_tx_set_tcp_ack_ratio(ampdu_tx_info_t *ampdu_tx, uint8 tcp_ack_ratio)
{
#if defined(BCMPCIEDEV)
	if (BCMPCIEDEV_ENAB()) {
		BCM_REFERENCE(tcp_ack_ratio);
		/* XXX: For PCIe full-dongle, we are using host buffers so no access to data.
		   Only allow setting ack_ratio when not using PCIE Full dongle
		 */
		ampdu_tx->tcp_ack_info.tcp_ack_ratio = 0;
	} else
#endif /* BCMPCIEDEV */
	{
		ampdu_tx->tcp_ack_info.tcp_ack_ratio = tcp_ack_ratio;
	}
}

/** overthruster related. */
static void BCMFASTPATH
wlc_ampdu_tcp_ack_suppress(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tx_t *scb_ampdu,
                           void *p, uint8 tid)
{
	wlc_tcp_ack_info_t *tcp_info = &(ampdu_tx->tcp_ack_info);
	uint8 *ip_header;
	uint8 *tcp_header;
	uint32 ack_num;
	void *prev_p;
	uint16 totlen;
	uint32 ip_hdr_len;
	uint32 tcp_hdr_len;
	uint32 pktlen;
	uint16 ethtype;
	uint8 prec;
	osl_t *osh = ampdu_tx->wlc->pub->osh;

	/* Even with tcp_ack_ratio set to 0, we want to examine the packet
	 * to find TCP ACK packet in case tcpack_fast_tx is true
	 */
	if ((tcp_info->tcp_ack_ratio == 0) && !ampdu_tx->wlc->tcpack_fast_tx)
		return;

	ethtype = wlc_sdu_etype(ampdu_tx->wlc, p);
	if (ethtype != ntoh16(ETHER_TYPE_IP))
		return;

	pktlen = pkttotlen(osh, p);

	ip_header = wlc_sdu_data(ampdu_tx->wlc, p);
	ip_hdr_len = 4*(ip_header[0] & 0x0f);

	if (pktlen < ETHER_HDR_LEN + DOT11_LLC_SNAP_HDR_LEN + ip_hdr_len)
		return;

	if (ip_header[9] != 0x06)
		return;

	tcp_header = ip_header + ip_hdr_len;

#ifdef WLAMPDU_PRECEDENCE
	prec = WLC_PRIO_TO_PREC(tid);
#else
	prec = tid;
#endif // endif

	if (tcp_header[13] & 0x10) {
		totlen =  (ip_header[3] & 0xff) + (ip_header[2] << 8);
		tcp_hdr_len = 4*((tcp_header[12] & 0xf0) >> 4);

		if (totlen ==  ip_hdr_len + tcp_hdr_len) {
			tcp_info->tcp_ack_total++;
			if (ampdu_tx->tcp_ack_info.tcp_ack_ratio_depth) {
				WLPKTTAG(p)->flags3 |= WLF3_FAVORED;
#ifdef WLAMPDU_PRECEDENCE
				prec = WLC_PRIO_TO_HI_PREC(tid);
#endif // endif
			}
			if (tcp_header[13] == 0x10) {
				WLPKTTAG(p)->flags3 |= WLF3_DATA_TCP_ACK;
				if (tcp_info->current_dequeued >= tcp_info->tcp_ack_ratio) {
					/* XXX We reached ack_ratio limit, don't mark the pkt
					 * and so "forget" the ack we are
					 * enqueing so it won't get
					 * suppressed the next time around
					 */
					tcp_info->current_dequeued = 0;
					return;
				}
			} else if (ampdu_tx->wlc->tcpack_fast_tx) {
				/* Here, we want to set TCP ACK flag even to SYN ACK packets,
				 * as we are not in this function to suppress any packet.
				 */
				WLPKTTAG(p)->flags3 |= WLF3_DATA_TCP_ACK;
				return;
			}
		}
	}

	/* XXX not tcp ack to consider
	 * In case tcp_ack_ratio is 0, this function may return even before coming here,
	 * but check tcp_ack_ratio again for sure.
	 */
	if (!(WLPKTTAG(p)->flags3 & WLF3_DATA_TCP_ACK) ||
		tcp_info->tcp_ack_ratio == 0) {
		return;
	}

	ack_num = (tcp_header[8] << 24) + (tcp_header[9] << 16) +
	        (tcp_header[10] << 8) +  tcp_header[11];

	if ((prev_p = pktqprec_peek_tail(&scb_ampdu->txq, prec)) &&
		(WLPKTTAG(prev_p)->flags3 & WLF3_DATA_TCP_ACK) &&
		!(WLPKTTAG(prev_p)->flags & WLF_AMSDU)) {
		uint8 *prev_ip_hdr = wlc_sdu_data(ampdu_tx->wlc, prev_p);
		uint8 *prev_tcp_hdr = prev_ip_hdr + 4*(prev_ip_hdr[0] & 0x0f);
		uint32 prev_ack_num = (prev_tcp_hdr[8] << 24) +
			(prev_tcp_hdr[9] << 16) +
			(prev_tcp_hdr[10] << 8) +  prev_tcp_hdr[11];

#ifdef PROP_TXSTATUS
		/* Don't drop suppress packets */
		if (GET_DRV_HAS_ASSIGNED_SEQ(WLPKTTAG(prev_p)->seq)) {
			return;
		}
#endif /* PROP_TXSTATUS */

		/* XXX is it the same dest/src IP addr, port # etc ??
		 * IPs: compare 8 bytes at IP hdrs offset 12
		 * compare tcp hdrs for 8 bytes : includes both ports and
		 * sequence number.
		 */
		if ((ack_num > prev_ack_num) &&
			!memcmp(prev_ip_hdr+12, ip_header+12, 8) &&
			!memcmp(prev_tcp_hdr, tcp_header, 4)) {
			prev_p = pktq_pdeq_tail(&scb_ampdu->txq, prec);
			PKTFREE(ampdu_tx->wlc->pub->osh, prev_p, TRUE);
			tcp_info->tcp_ack_dequeued++;
			tcp_info->current_dequeued++;
			return;
		}
	}

	/* XXX nothing worked so far, now check beginning of the queue, if no pkt were removed yet
	 * We only do this if the queue is smaller than an ampdu to avoid
	 * removing an ack that might delay the tcp_ack transmission
	 */
	if (pktqprec_n_pkts(&scb_ampdu->txq, prec) < ampdu_tx->tcp_ack_info.tcp_ack_ratio_depth) {
		int count = 0;
		void *previous_p = NULL;
		prev_p = pktqprec_peek(&scb_ampdu->txq, prec);

		while (prev_p && (count < ampdu_tx->tcp_ack_info.tcp_ack_ratio_depth)) {
			if ((WLPKTTAG(prev_p)->flags3 & WLF3_DATA_TCP_ACK) &&
			    !(WLPKTTAG(prev_p)->flags & WLF_AMSDU)) {
				uint8 *prev_ip_hdr = wlc_sdu_data(ampdu_tx->wlc, prev_p);
				uint8 *prev_tcp_hdr = prev_ip_hdr +
					4*(prev_ip_hdr[0] & 0x0f);
				uint32 prev_ack_num = (prev_tcp_hdr[8] << 24) +
					(prev_tcp_hdr[9] << 16) +
					(prev_tcp_hdr[10] << 8) +  prev_tcp_hdr[11];

#ifdef PROP_TXSTATUS
				/* Don't drop suppress packets */
				if (GET_DRV_HAS_ASSIGNED_SEQ(WLPKTTAG(prev_p)->seq)) {
					return;
				}
#endif /* PROP_TXSTATUS */
				/* is it the same dest/src IP addr, port # etc ??
				 * IPs: compare 8 bytes at IP hdrs offset 12
				 * compare tcp hdrs for 8 bytes : includes both ports and
				 * sequence number.
				 */
				if ((ack_num > prev_ack_num) &&
					(!memcmp(prev_ip_hdr+12, ip_header+12, 8)) &&
					(!memcmp(prev_tcp_hdr, tcp_header, 4))) {
					if (!previous_p) {
						prev_p = pktq_pdeq(&scb_ampdu->txq, prec);
					} else {
						prev_p = pktq_pdeq_prev(&scb_ampdu->txq,
							prec, previous_p);
					}
					if (prev_p) {
						PKTFREE(ampdu_tx->wlc->pub->osh,
							prev_p, TRUE);
						tcp_info->tcp_ack_multi_dequeued++;
						tcp_info->current_dequeued++;
					}
					return;
				}
			}
			previous_p = prev_p;
			prev_p = PKTLINK(prev_p);
			count++;
		}
	}

	tcp_info->current_dequeued = 0;
} /* wlc_ampdu_tcp_ack_suppress */

#else /* WLOVERTHRUSTER */

/** send TCP ACKs earlier to increase throughput */
static bool BCMFASTPATH
wlc_ampdu_is_tcpack(ampdu_tx_info_t *ampdu_tx, void *p)
{
	uint8 *ip_header;
	uint8 *tcp_header;
	uint16 totlen;
	uint32 ip_hdr_len;
	uint32 tcp_hdr_len;
	uint32 pktlen;
	uint16 ethtype;
	osl_t *osh = ampdu_tx->wlc->pub->osh;
	bool ret = FALSE;

	/* No reason to find TCP ACK packets */
	if (!ampdu_tx->wlc->tcpack_fast_tx)
		goto done;

	ethtype = wlc_sdu_etype(ampdu_tx->wlc, p);
	if (ethtype != ntoh16(ETHER_TYPE_IP))
		goto done;

	pktlen = pkttotlen(osh, p);

	ip_header = wlc_sdu_data(ampdu_tx->wlc, p);
	ip_hdr_len = 4*(ip_header[0] & 0x0f);

	if (pktlen < ETHER_HDR_LEN + DOT11_LLC_SNAP_HDR_LEN + ip_hdr_len)
		goto done;

	if (ip_header[9] != 0x06)
		goto done;

	tcp_header = ip_header + ip_hdr_len;

	if (tcp_header[13] & 0x10) {
		totlen =  (ip_header[3] & 0xff) + (ip_header[2] << 8);
		tcp_hdr_len = 4*((tcp_header[12] & 0xf0) >> 4);

		if (totlen ==  ip_hdr_len + tcp_hdr_len)
			ret = TRUE;
	}

done:
	return ret;
} /* wlc_ampdu_is_tcpack */

#endif /* WLOVERTHRUSTER */

/** Find basic rate for a given rate */
#define AMPDU_BASIC_RATE(band, rspec)	((band)->basic_rate[RSPEC_REFERENCE_RATE(rspec)])

#ifdef WLATF
/**
 * Air Time Fairness. Calculate number of bytes to be released in a time window at the current tx
 * rate for the maximum and minimum txq release time window.
 */
static atf_rbytes_t* BCMFASTPATH
wlc_ampdu_atf_calc_rbytes(atf_state_t *atf_state, ratespec_t rspec)
{
	ampdu_tx_info_t *ampdu_tx;
	scb_ampdu_tid_ini_t *ini;
	struct scb *scb;
	wlc_info_t *wlc;
	bool gProt;
	bool nProt;
	uint ctl_rspec;
	uint ack_rspec;
	uint pkt_overhead_us;
	uint max_pdu_time_us;
	uint ampdu_rate_kbps;
	wlc_bsscfg_t *bsscfg;
	wlcband_t *band;
	uint dur;
	uint8 max_pdu;
	uint subframe_time_us;
	uint adjusted_max_pdu;
	uint flags;
	uint32 pkt_size;
#ifdef WLCNT
	atf_stats_t *atf_stats = &atf_state->atf_stats;
#endif // endif

	BCM_REFERENCE(max_pdu);
	BCM_REFERENCE(pkt_size);

	ampdu_tx = atf_state->ampdu_tx;
	ini = atf_state->ini;
	scb = ini->scb;
	wlc = ampdu_tx->wlc;

	ASSERT(scb->bsscfg);

	bsscfg = scb->bsscfg;
	band = atf_state->band;
#if defined(WLATF) && defined(WLATF_PERC)
	dur = atf_state->txq_time_allowance_us;
#else
	dur = ampdu_tx->config->dur;
	max_pdu = atf_state->scb_ampdu->max_pdu;
#endif // endif
	flags = WLC_AIRTIME_AMPDU;

	/* XXX  Assume Mixed mode as the cost of calculating  is too high
	 * Can change if we find an advantage to GF calculation
	 *
	 */
	flags |= (WLC_AIRTIME_MIXEDMODE);

	gProt = (band->gmode && !RSPEC_ISCCK(rspec) && WLC_PROT_G_CFG_G(wlc->prot_g, bsscfg));
	nProt =	((WLC_PROT_N_CFG_N(wlc->prot_n, bsscfg) == WLC_N_PROTECTION_20IN40) &&
		RSPEC_IS40MHZ(rspec));

	if (WLC_HT_GET_AMPDU_RTS(wlc->hti) || nProt || gProt)
		flags |= (WLC_AIRTIME_RTSCTS);

	ack_rspec = AMPDU_BASIC_RATE(band, rspec);

	/* Short preamble */
	if (WLC_PROT_CFG_SHORTPREAMBLE(wlc->prot, bsscfg) &&
		(scb->flags & SCB_SHORTPREAMBLE) && RSPEC_ISCCK(ack_rspec)) {
			ack_rspec |= WL_RSPEC_SHORT_PREAMBLE;
	}

	if (gProt) {
		/*
		  * Use long preamble and long slots if Protection bit is set
		  * per Sect 9.23.2 IEEE802.11-2012
		  * IEEE Clause 16 stuff is all long slot
		  *
		  * Use 11Mbps or lower for ACKs in 2G Band
		  */
		ctl_rspec = AMPDU_BASIC_RATE(band, LEGACY_RSPEC(WLC_RATE_11M));
	} else {
		ctl_rspec = ack_rspec;
	}

	/*
	 * Short slot for ERP STAs
	 * The AP bsscfg will keep track of all sta's shortslot/longslot cap,
	 * and keep current_bss->capability up to date.
	 */
	if (BAND_5G(band->bandtype) || bsscfg->current_bss->capability & DOT11_CAP_SHORTSLOT)
			flags |= (WLC_AIRTIME_SHORTSLOT);

	pkt_overhead_us =
		wlc_airtime_pkt_overhead_us(flags, ctl_rspec, ack_rspec, bsscfg, atf_state->ac);

	/* XXX
	* Calculate the number of maxsize AMPDUS will fit in max duration
	* If less than orig max pdu, update max_pdu.
	* This tradeoff releases more small sized packets than expected
	* as the overhead is underestimated.
	* PLCP overhead not included.
	*/
#if defined(WLATF) && defined(WLATF_PERC)
	pkt_size = (WLC_AMSDU_IN_AMPDU(wlc, scb, ini->tid)) ?
		2 * ETHER_MAX_DATA : ETHER_MAX_DATA;
	subframe_time_us = wlc_airtime_payload_time_us(flags, rspec,
		(pkt_size + wlc_airtime_dot11hdrsize(scb->wsec)));

	adjusted_max_pdu = (dur - wlc_airtime_plcp_time_us(rspec, flags))/subframe_time_us;

	AMPDU_ATF_INCRSTAT(&atf_stats->pdu, adjusted_max_pdu);

	/* Max PDU time including header */
	max_pdu_time_us = (adjusted_max_pdu * subframe_time_us) + pkt_overhead_us +
		wlc_airtime_plcp_time_us(rspec, flags);

	ampdu_rate_kbps = ((adjusted_max_pdu * pkt_size)/max_pdu_time_us) * 8000;
#else
	subframe_time_us = wlc_airtime_payload_time_us(flags, rspec,
		(ETHER_MAX_DATA + wlc_airtime_dot11hdrsize(scb->wsec)));

	adjusted_max_pdu = (dur - wlc_airtime_plcp_time_us(rspec, flags))/subframe_time_us;
	max_pdu = MIN(adjusted_max_pdu, max_pdu);
	AMPDU_ATF_INCRSTAT(&atf_stats->pdu, max_pdu);

	/* Max PDU time including header */
	max_pdu_time_us = (max_pdu * subframe_time_us) + pkt_overhead_us;

	ampdu_rate_kbps = ((max_pdu * ETHER_MAX_DATA * 8000)/max_pdu_time_us);
#endif /* WLATF && WLATF_PERC */

	atf_state->rbytes_target.max = (ampdu_rate_kbps/8000) *
		atf_state->txq_time_allowance_us;
	atf_state->rbytes_target.min = (ampdu_rate_kbps/8000) *
		atf_state->txq_time_min_allowance_us;

	atf_state->last_est_rate = rspec;
	WLCNTINCR(atf_stats->cache_miss);

	return (&atf_state->rbytes_target);

} /* wlc_ampdu_atf_calc_rbytes */

static INLINE atf_rbytes_t*
wlc_ampdu_atf_rbytes(atf_state_t *atf_state)
{
	ratespec_t rspec = atf_state->rspec_action.function(atf_state->rspec_action.arg);

	/* If rspec is the same use the previously calculated result to reduce CPU overhead */
	if (rspec == atf_state->last_est_rate) {
		WLCNTINCR(atf_state->atf_stats.cache_hit);
		return (&atf_state->rbytes_target);
	} else {
		return  wlc_ampdu_atf_calc_rbytes(atf_state, rspec);
	}
}

static INLINE bool
wlc_ampdu_atf_holdrelease(atf_state_t *atf_state)
{
	if (atf_state->atf != 0) {
		atf_rbytes_t *rbytes = wlc_ampdu_atf_rbytes(atf_state);

		WLCNTINCR(atf_state->atf_stats.eval);

		if (atf_state->released_bytes_inflight > rbytes->max) {
			WLCNTINCR(atf_state->atf_stats.neval);
			return TRUE;
		}
	}
	return FALSE;
}

static INLINE bool
wlc_ampdu_atf_lowat_release(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tid_ini_t *ini)
{
	if (AMPDU_AQM_ENAB(ampdu_tx->wlc->pub)) {
		/* ATF adaptive watermark tested only with AQM devices only */
		return (AMPDU_ATF_ENABLED(ini) &&
			(AMPDU_ATF_STATE(ini)->released_bytes_inflight <=
			AMPDU_ATF_STATE(ini)->rbytes_target.min) &&
			(AMPDU_ATF_STATE(ini)->released_packets_inflight <=
			ampdu_tx->config->ampdu_atf_lowat_rel_cnt) &&
			(ampdu_tx->tx_intransit <=
			AMPDU_ATF_AGG_LOWAT_REL_CNT));
	} else {
		return FALSE;
	}
}

static void
wlc_ampdu_atf_reset_bytes_inflight(scb_ampdu_tid_ini_t *ini)
{
	WLCNTINCR(AMPDU_ATF_STATE(ini)->reset);
	AMPDU_ATF_STATE(ini)->released_bytes_inflight = 0;
	AMPDU_ATF_STATE(ini)->released_packets_inflight = 0;
}

static void
wlc_ampdu_atf_reset_state(scb_ampdu_tid_ini_t *ini)
{
	atf_state_t *atf_state = AMPDU_ATF_STATE(ini);
	wlc_ampdu_atf_reset_bytes_inflight(ini);
	atf_state->last_est_rate = 0;
	atf_state->last_ampdu_fid = wme_ac2fifo[atf_state->ac];
#ifdef WLATF_DONGLE
	atf_state->atfd = (ATFD_ENAB(atf_state)) ?
		wlfc_get_atfd(atf_state->wlc, ini->scb) : NULL;
#endif // endif
	memset(&atf_state->rbytes_target, 0, sizeof(atf_state->rbytes_target));
	wlc_ampdu_atf_ini_set_rspec_action(atf_state, atf_state->band->rspec_override);
	WLCNTINCR(atf_state->reset);
	AMPDU_ATF_CLRCNT(ini);
}

static void BCMFASTPATH
wlc_ampdu_atf_dec_bytes_inflight(scb_ampdu_tid_ini_t *ini, void *p)
{
	atf_state_t *atf_state;
	uint32 inflight;
	uint32 pktlen;
#if defined(WLCNT)
	atf_stats_t *atf_stats;
#endif // endif

	ASSERT(p);
	pktlen = WLPKTTAG(p)->pktinfo.atf.pkt_len;

	/* Pkttag has to be cleared to make sure the completed packets are cleaned up
	 * in the atf dynamic transition from on->off
	 */
	WLPKTTAG(p)->pktinfo.atf.pkt_len = 0;

	if (!AMPDU_ATF_ENABLED(ini)) {
		return;
	}

#if defined(WLCNT)
	atf_stats = AMPDU_ATF_STATS(ini);
#endif // endif
	if (!pktlen) {
		WLCNTINCR(atf_stats->reproc);
		return;
	}

	atf_state = AMPDU_ATF_STATE(ini);

	WLCNTINCR(atf_stats->proc);

	inflight = atf_state->released_bytes_inflight;

	if (inflight >= pktlen) {
		inflight -= pktlen;
		AMPDU_ATF_INCRSTAT(&atf_stats->inflight, inflight);
	} else {
		inflight = 0;
		WLCNTINCR(atf_stats->flush);
	}

	if (atf_state->released_packets_inflight) {
		atf_state->released_packets_inflight--;
	} else {
		WLCNTINCR(atf_stats->qempty);
	}

	atf_state->released_bytes_inflight = inflight;
}

#define wlc_ampdu_dec_bytes_inflight(ini, p) wlc_ampdu_atf_dec_bytes_inflight((ini), (p))

#else

#define wlc_ampdu_dec_bytes_inflight(ini, p) do {} while (0)
#define wlc_ampdu_atf_lowat_release(ampdu_tx, ini) (FALSE)
#define wlc_ampdu_atf_holdrelease(atf_state) (FALSE)
#endif /* WLATF */

#ifdef WLTAF
void * BCMFASTPATH
wlc_ampdu_get_taf_scb_info(void *ampdu_h, struct scb* scb)
{
	ampdu_tx_info_t* ampdu_tx = (ampdu_tx_info_t*)ampdu_h;
	void *scb_ampdu = NULL;
	bsscfg_ampdu_tx_t *bsscfg_ampdu;

	if (!scb || !ampdu_tx || !(SCB_AMPDU(scb)))
		goto exit;

	bsscfg_ampdu = BSSCFG_AMPDU_TX_CUBBY(ampdu_tx, SCB_BSSCFG(scb));

	/* If overrided OFF or aggregation enabled for no TID, return NULL */
	if (bsscfg_ampdu->txaggr_override == OFF || bsscfg_ampdu->txaggr_TID_bmap == 0)
		goto exit;

	scb_ampdu = (void*)SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
exit:
	return scb_ampdu;
}

void * BCMFASTPATH
wlc_ampdu_get_taf_scb_tid_info(void *scb_h, int tid)
{
	scb_ampdu_tx_t *scb_ampdu = (scb_ampdu_tx_t *) scb_h;
	if (scb_ampdu) {
		scb_ampdu_tid_ini_t* ini = scb_ampdu->ini[tid];

			return (void*)ini;
		}
	return NULL;
}

#ifdef PKTQ_LOG
struct pktq * BCMFASTPATH
wlc_ampdu_get_taf_scb_pktq(void *scbh)
{
	scb_ampdu_tx_t *scb_ampdu = (scb_ampdu_tx_t *) scbh;

	return scb_ampdu ? &scb_ampdu->txq : NULL;
}
#endif // endif

uint16 BCMFASTPATH
wlc_ampdu_get_taf_scb_tid_pktlen(void *scbh, void *tidh)
{
	scb_ampdu_tx_t *scb_ampdu = (scb_ampdu_tx_t *) scbh;
	scb_ampdu_tid_ini_t *ini = (scb_ampdu_tid_ini_t *) tidh;

	return (scb_ampdu && ini) ? ampdu_pktqprec_n_pkts(&scb_ampdu->txq, ini->tid) : 0;
}

uint8 BCMFASTPATH
wlc_ampdu_get_taf_scb_tid_rel(scb_ampdu_tx_t *scb_ampdu)
{
	return (scb_ampdu && scb_ampdu->ampdu_tx) ? scb_ampdu->release : 0;
}

uint8 BCMFASTPATH
wlc_ampdu_get_taf_txq_fullness_pct(scb_ampdu_tx_t *scb_ampdu, scb_ampdu_tid_ini_t *ini)
{
	wlc_txq_info_t *qi;
	struct pktq *q;		/**< multi priority packet queue */
	unsigned plen;
	unsigned pmax;
	unsigned prec;

	if (!scb_ampdu || !ini) {
		return 0;
	}

	qi = SCB_WLCIFP((scb_ampdu->scb))->qi;
	q = &qi->cpktq.cq;
	prec = WLC_PRIO_TO_PREC(ini->tid);
	plen = pktqprec_n_pkts(q, prec);
	pmax = pktqprec_max_pkts(q, prec);
	return (uint8)(plen * 100 / pmax);
}

uint8 BCMFASTPATH
wlc_ampdu_get_taf_max_pdu(scb_ampdu_tx_t *scb_ampdu)
{
	return (scb_ampdu && scb_ampdu->ampdu_tx) ? scb_ampdu->max_pdu : 0;
}

#ifdef WLSQS
void * wlc_ampdu_taf_sqs_link_init(wlc_info_t *wlc, struct scb *scb, uint8 prio)
{
	ampdu_tx_info_t* ampdu_tx = wlc->ampdu_tx;
	scb_ampdu_tx_t *scb_ampdu;
	scb_ampdu_tid_ini_t *ini = NULL;

	if (!(scb_ampdu = (void*)SCB_AMPDU_TX_CUBBY(ampdu_tx, scb))) {
		return NULL;
	}

	/* initialize initiator on first packet; sends addba req */
	if (!(ini = scb_ampdu->ini[prio])) {
		ini = wlc_ampdu_init_tid_ini(ampdu_tx, scb_ampdu, prio, FALSE);
	}
	return (void*)ini;
}
#endif /* WLSQS */
#endif /* WLTAF */

/** AMPDU packet class callback */
static void BCMFASTPATH
wlc_ampdu_pkt_freed(wlc_info_t *wlc, void *pkt, uint txs)
{
	ampdu_tx_info_t *ampdu_tx = wlc->ampdu_tx;
	struct scb *scb;
	scb_ampdu_tx_t *scb_ampdu_tx;
	scb_ampdu_tid_ini_t *ini;
	bool acked = (txs & TX_STATUS_ACK_RCV) != 0;
	wlc_pkttag_t *pkttag = WLPKTTAG(pkt);
	uint16 seq = pkttag->seq;
	bool aqm = AMPDU_AQM_ENAB(wlc->pub);

#ifdef BCMDBG_SWWLAN_38455
	if ((pkttag->flags & WLF_AMPDU_MPDU) == 0) {
		WL_PRINT(("%s flags=%x seq=%d aqm=%d acked=%d\n", __FUNCTION__,
			pkttag->flags, pkttag->seq, aqm, acked));
	}
#endif /* BCMDBG_SWWLAN_38455 */
	ASSERT((pkttag->flags & WLF_AMPDU_MPDU) != 0);

	scb = WLPKTTAGSCBGET(pkt);
	if (scb == NULL || !SCB_AMPDU(scb))
		return;

	scb_ampdu_tx = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	if (scb_ampdu_tx == NULL)
		return;

	ini = scb_ampdu_tx->ini[PKTPRIO(pkt)];
	if (!ini) {
		WL_AMPDU_ERR(("wl%d: ampdu_pkt_freed: NULL ini or pon state for tid %d\n",
		              wlc->pub->unit, PKTPRIO(pkt)));
		return;
	}

	if ((ini->ba_state != AMPDU_TID_STATE_BA_ON) &&
		(ini->ba_state != AMPDU_TID_STATE_BA_PENDING_OFF)) {
		WL_AMPDU_ERR(("wl%d: ampdu_pkt_freed: ba state %d for tid %d\n",
		              wlc->pub->unit, ini->ba_state, PKTPRIO(pkt)));
		return;
	}

#ifdef PROP_TXSTATUS
	seq = WL_SEQ_GET_NUM(seq);
#endif /* PROP_TXSTATUS */

	/* This seq # check could possibly affect the performance so keep it
	 * in a narrow scope i.e. only for legacy d11 core revs...also start_seq
	 * and max_seq may not apply to aqm anyway...
	 */
	if (!aqm &&
	    MODSUB_POW2(seq, ini->start_seq, SEQNUM_MAX) >
	    MODSUB_POW2(ini->max_seq, ini->start_seq, SEQNUM_MAX)) {
		WL_AMPDU_ERR(("wl%d: %s: unexpected seq 0x%x, start seq 0x%x, max seq %0x, "
		          "tx_in_transit %u\n", ampdu_tx->wlc->pub->unit, __FUNCTION__,
		          seq, ini->start_seq, ini->max_seq, ini->tx_in_transit));
		return;
	}

#ifdef WLTAF
	if (!wlc_taf_txpkt_status(wlc->taf_handle, scb, ini->tid, pkt, TAF_TXPKT_STATUS_PKTFREE))
#endif // endif
	{
		wlc_ampdu_dec_bytes_inflight(ini, pkt);
	}

	ASSERT(ini->tx_in_transit != 0);
	ini->tx_in_transit--;
#ifdef WLATF
	ASSERT(ampdu_tx->tx_intransit != 0);
	ampdu_tx->tx_intransit--;
#endif /* WLATF */

	/* If we are at PENDING OFF state, do not run through other states and
	 * return after we have accounted intransit packets
	 */
	if (ini->ba_state == AMPDU_TID_STATE_BA_PENDING_OFF)
		return;

	/* If the last packet for ampdu is freed before being transmitted, set alive to TRUE.
	 * New packets can arrive and be released before next call to
	 * wlc_ampdu_handle_ba_session_on(), causing tx_in_transit to be set again. This is
	 * causing issues with dead_cnt when STA is going in and out of PS and packets are
	 * being suppressed.
	 */
	if (ini->tx_in_transit == 0) {
		ini->alive = TRUE;
		/* Actually send bar if needed */
		if (!aqm) {
			wlc_ampdu_send_bar(wlc->ampdu_tx, ini, SEQNUM_CHK);
		} else {
			if (ini->barpending_seq != SEQNUM_MAX) {
				wlc_ampdu_send_bar(wlc->ampdu_tx, ini, ini->barpending_seq);
			}
		}
	}

	if (!acked) {
		/* send bar to move the window */
		wlc_ampdu_ini_adjust(ampdu_tx, ini, pkt);
	}
	else if (pkttag->flags3 & WLF3_AMPDU_REGMPDU) {
		/* packet treated as regular MPDU */
		wlc_ampdu_ini_move_window(ampdu_tx, scb_ampdu_tx, ini);
	}

	/* Check whether the packets on tx side
	 * is less than watermark level and disable flow
	 */
	wlc_ampdu_txflowcontrol(wlc, scb_ampdu_tx, ini);
} /* wlc_ampdu_pkt_freed */

#ifdef PROP_TXSTATUS

/*
 * If rem_window is 0 and suppr_window is 16 it means we can send 16 packets (real meaning is that
 * we can send 16 previously suppressed packets).
 */

#if !defined(WLAMPDU_AQM)
/** returns how many packets can be dequeued from the aggregation q towards the wlc transmit q */
#define wlc_ampdu_release_count(ini, qlen, scb_ampdu) \
	MIN(((ini)->non_aqm->rem_window + (ini)->suppr_window), MIN((qlen), (scb_ampdu)->release))

/** returns bool */
#define wlc_ampdu_window_is_passed(count, ini) \
	((count) > ((ini)->non_aqm->rem_window + (ini)->suppr_window))

#define wlc_ampdu_get_rem_window(ini) (ini)->non_aqm->rem_window
#else /* !WLAMPDU_AQM */
/** returns how many packets can be dequeued from the aggregation q towards the wlc transmit q
 *  ba_wsize is intentionally used here.
 */
#define wlc_ampdu_release_count(ini, qlen, scb_ampdu) \
	MIN((ini)->ba_wsize, MIN((qlen), (scb_ampdu)->release))

/** returns bool */
#define wlc_ampdu_window_is_passed(count, ini) \
	((count) > ((ini)->ba_wsize))

#define wlc_ampdu_get_rem_window(ini) (ini)->ba_wsize
#endif /* !WLAMPDU_AQM */
#else /* PROP_TXSTATUS */

#define wlc_ampdu_release_count(ini, qlen, scb_ampdu) \
	MIN((ini)->non_aqm->rem_window, MIN((qlen), (scb_ampdu)->release))

#define wlc_ampdu_window_is_passed(count, ini) \
		((count) > (ini)->non_aqm->rem_window)

#define wlc_ampdu_get_rem_window(ini) (ini)->non_aqm->rem_window
#endif /* PROP_TXSTATUS */

#ifdef WLCFP
/* 2. SCHEDULE: Attempt to release packets for aggregation. */
static INLINE void wlc_cfp_ampdu_schedule(scb_cfp_t *scb_cfp,
	scb_ampdu_tx_t *scb_ampdu, scb_ampdu_tid_ini_t *ini,
	const bool force_release);
#endif // endif

/**
 * Function which contains the smarts of when to 'release' (wlc_ampdu_release) aggregated sdu's
 * (that, at this stage, have not been sent yet) from an AMPDU internal software queue towards the
 * wlc/common transmit queue, bringing them closer to the d11 core. This function is called from
 * various places, for both AQM and non-AQM capable hardware.
 *
 * @param[in] ini   Connection with a specific remote party and a specific TID/QoS category.
 *
 * @return          TRUE if released else FALSE.
 */

/* XXX SUNNY_DAY_FUNCTIONALITY:
 * - determines if there are temporary conditions that block packets from being
 *   released.
 * - decides if and how many packets are to be 'released' to the next_fid (common/apps) queue.
 *   decision factors include:
 *       - #packets already pending in wlc/common (see variable in_transit)
 *       - fairness to other AMPDU connections, to prevent starvation
 *       - availability of memory (queue) to complete the operation
 * - if applicable, releases the packets (see wlc_ampdu_release())
 */
bool BCMFASTPATH
wlc_ampdu_txeval_action(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tx_t *scb_ampdu,
	scb_ampdu_tid_ini_t *ini, bool force, taf_scheduler_public_t *taf)
{
	uint16 qlen, input_rel;
	/**
	 * #mpdus that can be dequeued from the aggregation queue towards the
	 * wlc 'common' transmit queue:
	 */
	uint16 n_mpdus_to_release = 0;
	uint16 in_transit = 0;  /**< #mpdus pending in wlc/dma/d11 for given tid/ini */
	wlc_txq_info_t *qi; /**< refers to the 'common' transmit queue */
	struct pktq *common_q = NULL; /**< multi priority packet queue */
	uint16 next_fid_release; /**< #mpdus in nexfid (common/APPS) queue */
	ampdu_tx_config_t *ampdu_tx_cfg = ampdu_tx->config;
	wlc_info_t *wlc = ampdu_tx->wlc;
	wlc_bsscfg_t *cfg;
	struct scb *scb;
	uint next_fid;

	ASSERT(scb_ampdu);
	ASSERT(ini);
	BCM_REFERENCE(input_rel);

	scb = scb_ampdu->scb;
	ASSERT(scb != NULL);

#ifdef WLSQS
	/* SQS should never use legacy ampdu eval path */
	ASSERT(0);
#endif // endif

	if (!wlc_scb_restrict_can_txeval(wlc)) {
		return FALSE; /* channel switch / data block related */
	}

	if (ini->ba_state != AMPDU_TID_STATE_BA_ON)
		return FALSE;

	qlen = ampdu_pktqprec_n_pkts(&scb_ampdu->txq, ini->tid);

#ifdef WLCFP
	if (CFP_ENAB(wlc->pub) == TRUE) {
		/* In CFP mode AMPDU Qs carry packet count instead of MPDU count */
		qlen = WLC_AMPDU_PKTS_TO_MPDUS(wlc, scb, ini->tid, qlen);
	}
#endif /* WLCFP */

	if (qlen == 0) {
#ifdef WLTAF
		if (taf && taf->how == TAF_RELEASE_LIKE_IAS) {
			taf->ias.was_emptied = TRUE;
		}
		if (taf && taf->how == TAF_RELEASE_LIKE_DEFAULT) {
			taf->def.was_emptied = TRUE;
	}
#endif // endif
		return FALSE;
	}

	cfg = SCB_BSSCFG(scb);
	ASSERT(cfg != NULL);
	BCM_REFERENCE(cfg);

#ifdef PROP_TXSTATUS
	if (!AMPDU_AQM_ENAB(ampdu_tx->wlc->pub) &&
		PROP_TXSTATUS_ENAB(ampdu_tx->wlc->pub) &&
		wlc_wlfc_suppr_status_query(ampdu_tx->wlc, ini->scb)) {
		wlc_ampdu_flush_ampdu_q(ampdu_tx, SCB_BSSCFG(ini->scb));
		return FALSE;
	}
#endif /* PROP_TXSTATUS */

	n_mpdus_to_release = taf ? qlen : wlc_ampdu_release_count(ini, qlen, scb_ampdu);
	input_rel = n_mpdus_to_release;
	in_transit = ini->tx_in_transit;

	next_fid = SCB_TXMOD_NEXT_FID(scb, TXMOD_AMPDU);
	if (next_fid == TXMOD_APPS) {
		next_fid_release = wlc_apps_release_count(wlc, scb, WLC_PRIO_TO_PREC(ini->tid));
	} else {
		qi = SCB_WLCIFP(scb)->qi;
		common_q = WLC_GET_CQ(qi);
		next_fid_release = MIN(pktq_avail(common_q),
			pktqprec_avail_pkts(common_q, WLC_PRIO_TO_PREC(ini->tid)));
	}

	/*
	 * For AQM, limit the number of packets in transit simultaneously so that other
	 * stations get a chance to transmit as well.
	 */
	if (AMPDU_AQM_ENAB(wlc->pub)) {
		bool ps_pretend_limit_transit = SCB_PS_PRETEND(scb);
		/* max_seq is updated when an MPDU is released
		 * start_seq is updated when the window moves.
		 * in an idle situation, in which no packets are in 'release' state, max==start-1.
		 */
		if (IS_SEQ_ADVANCED(ini->max_seq, ini->start_seq)) {
			// there are unacknowledged packet(s) that were released.
			in_transit = MODSUB_POW2(ini->max_seq, ini->start_seq, SEQNUM_MAX);
#ifdef PROP_TXSTATUS
			if (in_transit >= ini->suppr_window) {
				/* suppr_window : suppr packet count inside ba window */
				in_transit -= ini->suppr_window;
			} else {
				in_transit = 0;
			}
#endif /* PROP_TXSTATUS */
		} else {
			/* here start_seq is 'ahead' of max_seq because of wrapping */
			in_transit = 0;
		}

		ASSERT(in_transit < SEQNUM_MAX/2);

#ifdef PSPRETEND
		/* scb->ps_pretend_start contains the TSF time for when ps pretend was
		 * last activated. In poor link conditions, there is a high chance that
		 * ps pretend will be re-triggered.
		 * Within a short time window following ps pretend, this code is trying to
		 * constrain the number of packets in transit and so avoid a large number of
		 * packets repetitively going between transmit and ps mode.
		 */
		if (!ps_pretend_limit_transit && SCB_PS_PRETEND_WAS_RECENT(scb)) {
			ps_pretend_limit_transit =
			        wlc_pspretend_limit_transit(wlc->pps_info, cfg, scb,
					in_transit, TRUE);
		}
#ifdef PROP_TXSTATUS
		if (PSPRETEND_ENAB(wlc->pub) &&
			SCB_TXMOD_ACTIVE(scb, TXMOD_APPS) &&
		    PROP_TXSTATUS_ENAB(wlc->pub) &&
		    HOST_PROPTXSTATUS_ACTIVATED(wlc)) {
			/* If TXMOD_APPS is active, and proptxstatus is enabled, force release.
			 * With proptxstatus enabled, they are meant to go to wlc_apps_ps_enq()
			 * to get their pktbufs freed in dongle, and instead stored in the host.
			 */
			ASSERT(SCB_PS(scb));
			ps_pretend_limit_transit = FALSE;
		}
#endif /* PROP_TXSTATUS */
#endif /* PSPRETEND */

		/*
		 * To prevent starvation of other tx traffic from this transmitter,
		 * domination of the 'common' queue with packets from one TID should be
		 * avoided. Unless forced, ensure that we do not have too many packets
		 * in transit for this priority.
		 *
		 * XXX Also, leave at least AMPDU_AQM_RELEASE_DEFAULT(256) or 50% room
		 * on the common Q.
		 */
		if (!taf && !force && (
			(wlc_ampdu_atf_holdrelease(AMPDU_ATF_STATE(ini))) ||
			(in_transit > ampdu_tx->aqm_max_release[ini->tid]) ||
			((next_fid == TXMOD_TRANSMIT) &&
				(next_fid_release <  MIN(AMPDU_AQM_RELEASE_DEFAULT,
				(pktqprec_max_pkts(common_q, WLC_PRIO_TO_PREC(ini->tid)) / 2)))) ||
			ps_pretend_limit_transit)) {

			WL_AMPDU_TX(("wl%d: txeval: Stop Releasing %d in_transit %d tid %d "
				"wm %d rem_wdw %d qlen %d force %d fid_release %d pps %d\n",
				wlc->pub->unit, n_mpdus_to_release, in_transit,
				ini->tid, ampdu_tx_cfg->tx_rel_lowat, wlc_ampdu_get_rem_window(ini),
				qlen, force, next_fid_release, ps_pretend_limit_transit));
			AMPDUSCBCNTINCR(scb_ampdu->cnt.txnoroom);
			n_mpdus_to_release = 0;
		}
	}

	if (next_fid_release <= n_mpdus_to_release) {
		/* change decision, as we are running out of space in next_fid tx queue */
#ifdef WLTAF
		if (taf) {
			WL_TAFF(wlc, "common queue full (%u/%u)\n", n_mpdus_to_release,
				next_fid_release);
			n_mpdus_to_release = next_fid_release ? next_fid_release - 1 : 0;

			if (n_mpdus_to_release == 0) {
				AMPDUSCBCNTINCR(scb_ampdu->cnt.txnoroom);
			}
		}
		else
#endif // endif
		{
			n_mpdus_to_release = 0;
			AMPDUSCBCNTINCR(scb_ampdu->cnt.txnoroom);
		}
	}

	if (n_mpdus_to_release == 0)
		return FALSE;

	/* release mpdus if any one of the following conditions are met */
	if ((taf || in_transit < ampdu_tx_cfg->tx_rel_lowat) ||
		wlc_ampdu_atf_lowat_release(ampdu_tx, ini) ||
	    (n_mpdus_to_release == MIN(scb_ampdu->release, ini->ba_wsize)) ||
	    force /* || AMPDU_AQM_ENAB(wlc->pub) */ )
	{
		if (!taf) {
			AMPDU_ATF_INCRSTAT(&AMPDU_ATF_STATS(ini)->inputq, qlen);
			AMPDU_ATF_INCRSTAT(&AMPDU_ATF_STATS(ini)->input_rel, input_rel);
			if (in_transit < ampdu_tx_cfg->tx_rel_lowat) {
				/* mpdus released due to lookahead watermark */
				WLCNTADD(ampdu_tx->cnt->txrel_wm, n_mpdus_to_release);
			}
			if (n_mpdus_to_release == scb_ampdu->release)
				WLCNTADD(ampdu_tx->cnt->txrel_size, n_mpdus_to_release);

			WL_AMPDU_TX(("wl%d: wlc_ampdu_txeval: Releasing %d mpdus: in_transit %d "
				"tid %d wm %d rem_wdw %d qlen %d force %d start_seq %x "
				"max_seq %x\n",
				wlc->pub->unit, n_mpdus_to_release, in_transit,
				ini->tid, ampdu_tx_cfg->tx_rel_lowat, wlc_ampdu_get_rem_window(ini),
				qlen, force, ini->start_seq, ini->max_seq));
		}
		wlc_ampdu_release(ampdu_tx, scb_ampdu, ini,
			n_mpdus_to_release, next_fid_release, taf);
#ifdef WLTAF
		if (taf) {
			uint16 new_qlen = ampdu_pktqprec_n_pkts(&scb_ampdu->txq, ini->tid);
			if (new_qlen == 0) {
				if (taf->how == TAF_RELEASE_LIKE_IAS) {
					taf->ias.was_emptied = TRUE;
				}
				if (taf->how == TAF_RELEASE_LIKE_DEFAULT) {
					taf->def.was_emptied = TRUE;
				}
				WL_NONE(("%s "MACF" tid %u is empty\n", __FUNCTION__,
				         ETHER_TO_MACF(scb_ampdu->scb->ea), ini->tid));
			}
		}
#endif // endif
#ifdef BCMDBG
		WLC_AMPDU_TXQ_PROF_ADD_ENTRY(wlc, scb);
#endif // endif
		return TRUE;
	}

	if ((ini->tx_in_transit == 0) && (in_transit != 0)) {
		WL_AMPDU_ERR(("wl%d.%d Cannot release: in_transit %d tid %d "
			"start_seq 0x%x barpending_seq 0x%x max_seq 0x%x\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(cfg),
			in_transit, ini->tid, ini->start_seq, ini->barpending_seq, ini->max_seq));
	}

	WL_AMPDU_TX(("wl%d: wlc_ampdu_txeval: Cannot release: in_transit %d tid %d wm %d "
	             "rem_wdw %d qlen %d release %d\n",
	             wlc->pub->unit, in_transit, ini->tid, ampdu_tx_cfg->tx_rel_lowat,
	             wlc_ampdu_get_rem_window(ini), qlen, n_mpdus_to_release));

	return FALSE;
} /* wlc_ampdu_txeval_action */

/* Select one of the eval routines
 * 1. TAF based unified scheduler[ Used with TAF enabled]
 * 2. SQS based ampdu scheduler  [ Used in non TAF builds]
 * 3. CFP based ampdu scheduler  [ Used in non SQS FD builds + CFP NIC builds]
 * 4. Legacy ampdu scheduler     [ Used  when CFP, SQS, TAF not enabled]
 */
static bool BCMFASTPATH
wlc_ampdu_txeval(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tx_t *scb_ampdu,
                 scb_ampdu_tid_ini_t *ini, bool force)

{
#ifdef WLTAF
	struct wlc_taf_info* taf_handle = ampdu_tx->wlc->taf_handle;
	bool taf_enable = wlc_taf_in_use(taf_handle);

	if (taf_enable) {
		bool finished = wlc_taf_schedule(taf_handle, ini->tid, scb_ampdu->scb,
		                                 force);
		if (finished) {
			return TRUE;
		}
		/* falls through here if TAF wants normal TX path to pick this up:
		* for example, in taf bypass mode
		*/
	}
#endif /* WLTAF */

#if defined(WLSQS)
	{
		int release;
		release = wlc_sqs_ampdu_evaluate(ampdu_tx, scb_ampdu, ini->tid, force, NULL);
		if (release > 0) {
			wlc_wlfc_sqs_stride_resume(ampdu_tx->wlc, scb_ampdu->scb, ini->tid);
		}

		return (release > 0) ? TRUE : FALSE;
	}
#endif /* WLSQS */

#ifdef WLCFP
	if (CFP_ENAB(ampdu_tx->wlc->pub) == TRUE) {
		scb_cfp_t *scb_cfp;	/* CFP cubby */
		/* Check if CFP TCB is established */
		if (wlc_cfp_tcb_is_EST(ampdu_tx->wlc, scb_ampdu->scb, ini->tid, &scb_cfp)) {
			/* 2. SCHEDULE: Schedule the release of packets for aggregation. */
			wlc_cfp_ampdu_schedule(scb_cfp, scb_ampdu, ini, force);
			return TRUE;
		}
	}
#endif /* WLCFP */

	return wlc_ampdu_txeval_action(ampdu_tx, scb_ampdu,
	                               ini,  force, NULL);
} /* wlc_ampdu_txeval */

/**
 * Releases transmit mpdu's for the caller supplied tid/ini, from an AMPDU internal software queue
 * (scb_ampdu->txq), forwarding them to the wlc managed transmit queue (see wlc_sendampdu()), which
 * brings the packets a step closer towards the d11 core.
 *
 * Function parameters:
 *     release: number of packets to release
 */

/* XXX SUNNY_DAY_FUNCTIONALITY:
 * - determine if less than the caller requested number of packets are to be
 *   released (does the caller always handle this correctly?)
 * - dequeues packets from AMPDU SW internal 'aggregation' queue
 * For each packet:
 *    - assigns d11 seq number
 *    - calls transmit module (wlc_txq_enq(), leads to wlc_sendampdu())
 *    - calls a packet callback (WLF2_PCB4_AMPDU)
 *    - maintain ATF bookkeeping
 */
static uint16 BCMFASTPATH
wlc_ampdu_release(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tx_t *scb_ampdu,
	scb_ampdu_tid_ini_t *ini, uint16 release, uint16 q_avail, taf_scheduler_public_t* taf)
{
	void* p = NULL;
	uint16 seq = 0; /* d11 sequence */
	uint16 indx;   /* not used when AQM is enabled */
	uint16 delta;  /* is zero when AQM is enabled */
	bool do_release;
	uint16 chunk_pkts = 0;
	bool pkts_remaining;
	uint16 suppr_count = 0; /* PROP_TXSTATUS related */
	bool aqm_enab;
	bool reset_suppr_count = FALSE;
	uint pktbytes = 0;
	uint8 tid = ini->tid;
	wlc_info_t *wlc = ampdu_tx->wlc;
	wlc_pub_t *pub = wlc->pub;
	osl_t *osh = wlc->osh;
	wlc_pkttag_t *pkttag = NULL;
	struct pktq *txq = &scb_ampdu->txq;		/**< multi priority packet queue */
	struct scb *scb = ini->scb;
#ifdef WLATF
	atf_state_t *atf_state;
	atf_rbytes_t *atf_rbytes;
	bool use_atf;
	uint32 max_rbytes;
	uint32 chunk_bytes;
	uint32 atf_rbytes_min;
	uint atf_mode;
#if defined(WLATF) && defined(WLATF_PERC)
	uint32 now;
#endif // endif

#ifdef WLCNT
	atf_stats_t *atf_stats;
	uint32 npkts;
	uint32 uflow;
	uint32 oflow;
	uint32 minrel;
	uint32 reloverride;
#endif /* WLCNT */
#endif /* WLATF */
#if defined(WLATF_DONGLE)
	bool atfd_enab = FALSE;
	uint16 frmbytes;
	ratespec_t cur_rspec = 0;
	/* The variables below are used in ampdu do_release cycle. */
	uint8 fl;
	uint32 dot11hdrsize;
	wlc_ravg_info_t *ravg_info;
	wlc_atfd_t *atfd = NULL;
#endif /* WLATF_DONGLE */
	uint next_fid;
#ifdef RAVG_HISTORY
	pkt2oct_t *pkt2oct;
#endif // endif
#if defined(WLCFP) && defined(WLAMSDU_TX)
	amsdu_info_t *ami;
#endif /* WLCFP */

	BCM_REFERENCE(suppr_count);
	BCM_REFERENCE(reset_suppr_count);
	BCM_REFERENCE(osh);
	BCM_REFERENCE(pub);

	WL_AMPDU_TX(("%s start release %d%s\n", __FUNCTION__, release, taf ? " TAF":""));

	next_fid = SCB_TXMOD_NEXT_FID(scb, TXMOD_AMPDU);

#ifdef WLCFP
	if (CFP_ENAB(wlc->pub) == TRUE) {
		scb_cfp_t *scb_cfp;	/* CFP cubby */
		/* Check if CFP TCB is established */
		if (wlc_cfp_tcb_is_EST(wlc, scb_ampdu->scb, ini->tid, &scb_cfp)) {
			WL_ERROR(("%s: CFP session for scb %p is already established \n",
				__FUNCTION__, scb_ampdu->scb));
			ASSERT(0);
		}
	}
#endif /* WLCFP */

	if (release && AMPDU_AQM_ENAB(pub)) {
		/* this reduces packet drop after a channel switch (until rate has stabilized) */
		release = wlc_scb_restrict_can_ampduq(wlc, scb, ini->tx_in_transit, release);
	}

	do_release = (release != 0) && (q_avail > 0);
	if (!do_release) {
#if defined(WLTAF) && defined(BCMDBG)
		if (taf && release == 0) {
			taf->complete = TAF_REL_COMPLETE_NOTHING;
		} else if (taf && q_avail == 0) {
			taf->complete = TAF_REL_COMPLETE_FULL;
		}
#endif /* WLTAF && BCMDBG */
		goto release_done;
	}

	if (!AMPDU_AQM_ENAB(pub)) {
		seq = SCB_SEQNUM(scb, tid) & (SEQNUM_MAX - 1);
		delta = MODSUB_POW2(seq, MODADD_POW2(ini->max_seq, 1, SEQNUM_MAX), SEQNUM_MAX);

		/* do not release past window (seqnum based check). There might be pkts
		 * in txq which are not marked AMPDU so will use up a seq number
		 */
		if (wlc_ampdu_window_is_passed(delta + release, ini)) {
			WL_AMPDU_ERR(("wl%d: wlc_ampdu_release: cannot release %d"
				"(out of window)\n", pub->unit, release));
			goto release_done;
		}
	} else {
		delta = 0;
	}

	aqm_enab = AMPDU_AQM_ENAB(pub);
	pkts_remaining = FALSE;

	BCM_REFERENCE(aqm_enab);
#if defined(TINY_PKTJOIN)
	struct pktq tmp_q;		/**< multi priority packet queue */
	bool tiny_join = (release > 1);

	if (TINY_PKTJOIN_ENAB(pub)) {
		if (tiny_join) {
			uint8 count = release;
			void *prev_p = NULL;
			int plen;

			pktq_init(&tmp_q, 1, AMPDU_PKTQ_LEN);
			while (count--) {
				p = _wlc_ampdu_pktq_pdeq(txq, tid);
				ASSERT(p != NULL);

				plen = PKTLEN(osh, p);
				if ((plen <= TCPACKSZSDU) && (prev_p  != NULL)) {
					void *new_pkt;
					uint16 round_len;
					uint16 prev_len;

					ASSERT(PKTTAILROOM(osh, prev_p) > 512);

					prev_len = PKTLEN(osh, prev_p);
					round_len = 64 +
						ROUNDUP(prev_len, sizeof(uint16)) - prev_len;
					new_pkt = hnd_pkt_clone(osh,
						prev_p,
						PKTLEN(osh, prev_p) + round_len,
						PKTTAILROOM(osh, prev_p)-round_len);
					if (new_pkt) {
						PKTPULL(osh, new_pkt, 192);
						bcopy(PKTDATA(osh, p),
						   PKTDATA(osh, new_pkt),
						   plen);
						PKTSETLEN(osh, new_pkt, plen);

						wlc_pkttag_info_move(wlc, p, new_pkt);
						PKTSETPRIO(new_pkt, PKTPRIO(p));

						PKTSETNEXT(osh, new_pkt, PKTNEXT(osh, p));
						PKTSETNEXT(osh, p, NULL);

						PKTFREE(osh, p, TRUE);
						p = new_pkt;
					}

					/* Used so clear to re-load */
					prev_p = NULL;
				}

				pktq_penq(&tmp_q, 0, p);

				/*
				 * Traverse in case of AMSDU pkt chain
				 * and only set prev_p with enough tail room
				 * If prev_p != NULL, it was not used so don't update
				 * since there's no guarantee the pkt immediately
				 * before it has tail room.
				 */
				if (prev_p == NULL) {
					/* No tail room in phdr */
					prev_p = p;

					while (prev_p) {
						if (PKTTAILROOM(osh, prev_p) > 512)
							break;

						prev_p = PKTNEXT(osh, prev_p);
					}
				}
			}
		}
	}
#endif /* TINY_PKTJOIN */
#ifdef WLATF
	chunk_bytes = 0;
#ifdef WLCNT
	atf_stats = AMPDU_ATF_STATS(ini);
	npkts = 0;
	uflow = 0;
	oflow = 0;
	minrel = 0;
	reloverride = 0;
#endif /* WLCNT */

	if (!taf && AMPDU_ATF_ENABLED(ini)) {
#if defined(WLATF) && defined(WLATF_PERC)
		if (wlc->atm_perc) {
			wlc_read_tsf(wlc, &now, NULL);
			scb_ampdu->scb->last_active_usec = now;
			wlc_ampdu_update_atf_perc_max(wlc, now);
			if (wlc->perc_max != wlc->perc_max_last) {
				wlc->perc_max_last = wlc->perc_max;
				wlc_ampdu_atf_update_release_duration(wlc);
			}
		}
#endif /* WLATF && WLATF_PERC */
		atf_state = AMPDU_ATF_STATE(ini);
		atf_rbytes = wlc_ampdu_atf_rbytes(atf_state);
		use_atf  = TRUE;
		atf_rbytes_min = atf_rbytes->min;
		atf_mode = atf_state->atf;
		max_rbytes = (atf_rbytes->max > atf_state->released_bytes_inflight) ?
			(atf_rbytes->max - atf_state->released_bytes_inflight) :  0;
#if defined(WLATF_DONGLE)
		atfd_enab = (use_atf && ATFD_ENAB(wlc));
		if (atfd_enab) {
			cur_rspec = atf_state->last_est_rate;
			atfd = atf_state->atfd;
			ASSERT(atfd);
		}
#endif /* WLATF_DONGLE */
	} else {
	/* XXX
	 * Unfortunate that this init has to be done as gcc is not smart enough
	 * to figure out this is never used unless use_atf is TRUE
	 * it mindlessly issues a warning causing the compile to fail
	 */
		use_atf = FALSE;
		atf_state = NULL;
		atf_rbytes = NULL;
		max_rbytes = 0;
		pktbytes = 0;
		chunk_bytes = 0;
		atf_rbytes_min = 0;
		atf_mode = 0;
	}
#endif /* WLATF */

#if defined(WLATF_DONGLE)
	if (atfd_enab) {
		if (!cur_rspec) {
			cur_rspec = wlc_scb_ratesel_get_primary(wlc, scb, NULL);
		}
		fl = RAVG_PRIO2FLR(PRIOMAP(wlc), tid);
		dot11hdrsize = wlc_scb_dot11hdrsize(scb);
		ravg_info = TXPKTLEN_RAVG(atfd, fl);
	}
#endif /* WLATF_DONGLE */

	while (do_release && (q_avail > 0)) { /* loop over each packet to release */
#if defined(TINY_PKTJOIN)
		if (TINY_PKTJOIN_ENAB(pub) && tiny_join) {
			p = pktq_pdeq(&tmp_q, 0);
		} else
#endif // endif
		{
			p = _wlc_ampdu_pktq_pdeq(txq, tid);
		}

#ifdef WLATF
		if (p == NULL) {
			WLCNTINCR(uflow);
#if defined(WLTAF) && defined(BCMDBG)
			if (taf) {
				taf->complete = taf->ias.is_ps_mode ?
					TAF_REL_COMPLETE_EMPTIED_PS : TAF_REL_COMPLETE_EMPTIED;
			}
#endif /* WLTAF && BCMDBG */
			break;
		}
		WLCNTINCR(npkts);
#else
		ASSERT(p != NULL);
		if (p == NULL) {
			break;
		}
#endif /* WLATF */

#ifdef WLCFP
		if (CFP_ENAB(wlc->pub) == TRUE) {
#ifdef WLAMSDU_TX
			ami = wlc->ami;

			/* Attempt AMSDU aggregation now.
			 * In CFP mode AMSDU agg is done during SCB AMPDU release
			 * To make SCB Q a common landing region,
			 * change the legacy ampdu release paths
			 */

			if (WLC_AMSDU_IN_AMPDU(wlc, scb, tid) ||
#ifdef PROP_TXSTATUS
				WL_SEQ_IS_AMSDU(WLPKTTAG(p)->seq))
#else
				FALSE)
#endif /* PROP_TXSTATUS */
			{
				wlc_amsdu_agg_attempt(ami, scb, p, tid);
			}
#endif /* WLAMSDU_TX */

			if (PKTISCFP(p)) {
				/* Packet enqueued by CFP: Release by legacy path */
				/* Reset CFP flags now */
				PKTCLRCFPFLOWID(p, CFP_FLOWID_INVALID);
			}
			wlc_cfp_incr_legacy_tx_cnt(wlc, scb, tid);
		}
#endif /* WLCFP */
		chunk_pkts++;

		ASSERT(PKTPRIO(p) == tid);

		pkttag = WLPKTTAG(p);

		/** Note: despite setting this flag, ucode is still allowed to send the packet as
		 * 'regular', so not part of an aggregate, depending on the selected rate.
		 */
		pkttag->flags |= WLF_AMPDU_MPDU;

#ifdef PROP_TXSTATUS

		if (GET_DRV_HAS_ASSIGNED_SEQ(pkttag->seq)) {
			seq = WL_SEQ_GET_NUM(pkttag->seq);
#ifdef PROP_TXSTATUS_SUPPR_WINDOW
			suppr_count++;
#endif // endif
		} else
#endif /* PROP_TXSTATUS */
		{
			/* assign seqnum and save in pkttag */
			seq = SCB_SEQNUM(scb, tid) & (SEQNUM_MAX - 1);
			SCB_SEQNUM(scb, tid)++;
			pkttag->seq = seq;
			ini->max_seq = seq;
			reset_suppr_count = TRUE;
		}

#ifdef PROP_TXSTATUS
		if (PROP_TXSTATUS_ENAB(pub) &&
			WLFC_GET_REUSESEQ(wlfc_query_mode(wlc->wlfc))) {
			SET_WL_HAS_ASSIGNED_SEQ(pkttag->seq);
		}
#endif /* PROP_TXSTATUS */

		/* ASSERT(!isset(ini->ackpending, indx)); */
		/* ASSERT(ini->txretry[indx] == 0); */
		if (!AMPDU_AQM_ENAB(pub)) {
			indx = TX_SEQ_TO_INDEX(seq);
			setbit(ini->non_aqm->ackpending, indx);
		}

		pktbytes = pkttotlen(ampdu_tx->wlc->osh, p);
		BCM_REFERENCE(pktbytes);
		WLCNTSCBINCR(scb_ampdu->ampdu_scb_stats->tx_pkts[ini->tid]);
		WLCNTSCBADD(scb_ampdu->ampdu_scb_stats->tx_bytes[ini->tid], pktbytes);

#ifdef WLTAF
		if (taf && taf->how == TAF_RELEASE_LIKE_IAS) {
			uint32 taf_pkt_time_units;
			uint32 taf_pkt_tag;

			ASSERT(!use_atf);

			if (!taf->ias.is_ps_mode)  {

				taf_pkt_time_units = TAF_PKTBYTES_TO_UNITS((uint16)pktbytes,
					taf->ias.pkt_rate, taf->ias.byte_rate);

				if (taf_pkt_time_units == 0) {
					taf_pkt_time_units = 1;
				}
				taf->ias.actual.released_bytes += (uint16)pktbytes;

				taf_pkt_tag = TAF_UNITS_TO_PKTTAG(taf_pkt_time_units,
					&taf->ias.adjust);
				taf->ias.actual.released_units += TAF_PKTTAG_TO_UNITS(taf_pkt_tag);

				if ((taf->ias.actual.released_units +
					taf->ias.total.released_units) >=
					taf->ias.time_limit_units) {

					release = 0;
					do_release = FALSE;
#ifdef BCMDBG
					taf->complete = TAF_REL_COMPLETE_TIME_LIMIT;
#endif // endif
				}

				if ((taf->ias.released_units_limit > 0) &&
					(taf->ias.actual.released_units >=
					taf->ias.released_units_limit)) {

					release = 0;
					do_release = FALSE;
#ifdef BCMDBG
					taf->complete = TAF_REL_COMPLETE_REL_LIMIT;
#endif // endif
				}
			} else {
				taf_pkt_tag = TAF_PKTTAG_PS;
			}
			taf->ias.actual.release++;
			WLPKTTAG(p)->pktinfo.taf.ias.index = taf->ias.index;
			WLPKTTAG(p)->pktinfo.taf.ias.units = taf_pkt_tag;
			WLPKTTAG(p)->pktinfo.taf.ias.tagged = TAF_TAGGED;
		}
		else if (taf && taf->how == TAF_RELEASE_LIKE_DEFAULT) {
			ASSERT(!use_atf);

			WLPKTTAG(p)->pktinfo.taf.def.tag = SCB_PS(scb) ?
				TAF_PKTTAG_PS : TAF_PKTTAG_DEFAULT;
			WLPKTTAG(p)->pktinfo.taf.def.tagged = 1;

			taf->def.actual.release++;
		}
#endif /* WLTAF */
#ifdef WLATF
		/* The downstream TXMOD may delete and free the packet, do the ATF
		 * accounting before the TXMOD to prevent the counter from going
		 * out of sync.
		 * Similarly the pkttag update, updating the pkttag after it has
		 * been freed is to be avoided
		 */
		if (use_atf) {
			ASSERT(!taf);
			pktbytes = pkttotlen(osh, p);
			pkttag->pktinfo.atf.pkt_len = (uint16)pktbytes;
			atf_state->released_bytes_inflight += pktbytes;
			chunk_bytes += pktbytes;
		}
		ampdu_tx->tx_intransit++;
#endif /* WLATF */

		/* enable packet callback for every MPDU in AMPDU */
		WLF2_PCB4_REG(p, WLF2_PCB4_AMPDU);
		ini->tx_in_transit++;

#if defined(WLATF_DONGLE)
		if (atfd_enab) {
			frmbytes = pkttotlen(osh, p) + dot11hdrsize;
			/* adding pktlen into the moving average buffer */
			RAVG_ADD(ravg_info, frmbytes);
		}
#endif /* WLATF_DONGLE */

		if (next_fid == TXMOD_TRANSMIT) {
			wlc_txq_enq_pkt(wlc, scb, p, WLC_PRIO_TO_PREC(tid));
		} else {
			/* calls apps module (wlc_apps_ps_enq) : */
			SCB_TX_NEXT(TXMOD_AMPDU, scb, p, WLC_PRIO_TO_PREC(tid));
		}

		/* Out of space on outbound queue. We are done. */
		if (--q_avail == 0) {
#if defined(WLTAF) && defined(BCMDBG)
			if (taf) {
				taf->complete = TAF_REL_COMPLETE_FULL;
			}
#endif /* WLTAF && BCMDBG */
#if defined(WLATF)
			if (use_atf) {
				WLCNTINCR(oflow);
			}
#endif // endif
			do_release = FALSE;
		}

		/* pkts_remaining is TRUE is we are below the frame release limit */
		pkts_remaining = (chunk_pkts < release);

		/* Logic in the remainder of the scope
		 * evaluates if the next dequeue is to be done
		 */

#ifdef WLATF
		/* Apply ATF release overrides */
		if (use_atf && do_release) {
			/* Assumes SNAP header already present */
			/* XXX this is a shortcut we are taking to reduce calculation time,
			 * We also do not know the final packet sizes without computing so the
			 * amount of intra-AMPDU frame padding cannot be determined.
			 *
			 * ATF mode 2 is not applicable to non AQM devices
			 * as the required code is not done.
			 */
			/* can_release is TRUE below ATF byte time limit */
			bool can_release = (chunk_bytes <= max_rbytes);

			/* Primary release condition
			 * Meet release up to ATF time low watermark, AQM only.
			 * Non AQM devices rely on the driver to track the BA window
			 * so we may end up releasing too many packets and thus have
			 * to add another check to make sure it does not happen,
			 * better to avoid the matter entirely in the
			 * time critical loop.
			 */
#if defined(WLATF) && defined(WLATF_PERC)
			do_release = ((aqm_enab) &&
				(chunk_bytes <= atf_rbytes_min) && (can_release));
#else
			do_release = ((aqm_enab) &&
				(chunk_bytes <= atf_rbytes_min));
#endif // endif

			/* Secondary release condition,
			 * there packets remaining to be
			 * released and we are below ATF time limit.
			 */
			do_release |= ((pkts_remaining) && (can_release));

			/* Tertiary release.
			 * ATF_PMODE release up to ATF time limit
			 */
			do_release |= ((can_release) &&
				(atf_mode == WLC_AIRTIME_PMODE));
#ifdef WLCNT
			/* Update stats */

			if (!pkts_remaining && can_release) {
				/* Log minimum release if all frames
				 * have been released and we are still
				 * below the time low watermark
				 */
				if (chunk_bytes <= atf_rbytes_min) {
					WLCNTINCR(minrel);
				}
				/* Log PMODE override count */
				else if (atf_mode == WLC_AIRTIME_PMODE) {
					WLCNTINCR(reloverride);
				} /* (!pkts_remaining && can_release) */
			}
#endif /* WLCNT */

#ifdef RAVG_SIMPLE
			scb_ampdu->pkt_avg_octet[tid] += pktbytes;
			scb_ampdu->pkt_avg_octet[tid] >>= 1;
#endif /* RAVG_SIMPLE */
#ifdef RAVG_HISTORY
			pkt2oct = scb_ampdu->pkt2oct[tid];
			pkt2oct->pkt_avg_octet -= (pkt2oct->pktlen[pkt2oct->pkt_idx] >> 3);
			pkt2oct->pkt_avg_octet += (pktbytes >> 3);
			pkt2oct->pktlen[pkt2oct->pkt_idx] = pktbytes;
			pkt2oct->pkt_idx = (pkt2oct->pkt_idx + 1) & (RAVG_HISTORY_PKTNUM - 1);
#endif /* RAVG_HISTORY */
		} else
#endif /* WLATF */
		{
			do_release = do_release && pkts_remaining;
		}
	} /* while (do_release) */

	if (next_fid == TXMOD_TRANSMIT) {
		wlc_txq_enq_flowcontrol_ampdu(wlc, scb, tid);
	}

#ifdef WLATF
	if (use_atf) {
		atf_state->released_packets_inflight += chunk_pkts;
	}
#endif /* WLATF */

#ifdef PROP_TXSTATUS
	if (PROP_TXSTATUS_ENAB(pub) &&
		WLFC_GET_REUSESEQ(wlfc_query_mode(wlc->wlfc))) {
		if (reset_suppr_count || (ini->suppr_window <= suppr_count)) {
			ini->suppr_window = 0; /* suppr packet count inside ba window */
		} else {
			ini->suppr_window -= suppr_count;
		}
	}
#endif /* PROP_TXSTATUS */

	if (!AMPDU_AQM_ENAB(pub)) {
		ini->non_aqm->rem_window -= (chunk_pkts + delta - suppr_count);
	}

#if defined(WLATF_DONGLE)
	if (atfd_enab && (chunk_pkts > 0)) {
		fl = RAVG_PRIO2FLR(PRIOMAP(wlc), tid);
		ASSERT(atfd);
		wlc_ravg_add_weight(atfd, fl, cur_rspec);
	}
#endif /* WLATF_DONGLE */

#if defined(WLATF) && defined(WLCNT)
	if (use_atf) {
		WLCNTADD(atf_stats->npkts, npkts);
		WLCNTADD(atf_stats->uflow, uflow);
		WLCNTADD(atf_stats->oflow, oflow);
		WLCNTADD(atf_stats->uflow, minrel);
		WLCNTADD(atf_stats->oflow, reloverride);

		WLCNTINCR(atf_stats->ndequeue);
		if (chunk_pkts == 1) {
			WLCNTINCR(atf_stats->singleton);
		}
		if (p != NULL) {
			if (pkts_remaining)
				WLCNTINCR(atf_stats->timelimited);
			if (atf_rbytes->max > atf_state->released_bytes_inflight)
				WLCNTINCR(atf_stats->framelimited);
		}

		AMPDU_ATF_INCRSTAT(&atf_stats->rbytes, max_rbytes);
		AMPDU_ATF_INCRSTAT(&atf_stats->chunk_bytes, chunk_bytes);
		AMPDU_ATF_INCRSTAT(&atf_stats->chunk_pkts, chunk_pkts);
		AMPDU_ATF_INCRSTAT(&atf_stats->inflight, atf_state->released_bytes_inflight);
		AMPDU_ATF_INCRSTAT(&atf_stats->transit, atf_state->released_packets_inflight);
		AMPDU_ATF_INCRSTAT(&atf_stats->last_est_rate,
			wf_rspec_to_rate(atf_state->last_est_rate));

	}
#endif /* WLATF  && WLCNT */
release_done:
	return chunk_pkts;
} /* wlc_ampdu_release */

/**
 * Returns TRUE if 'seq' contains the next expected d11 sequence. If not, sends a Block Ack Req to
 * the remote party. AC chip specific function.
 */
static bool BCMFASTPATH
ampdu_is_exp_seq_aqm(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tid_ini_t *ini,
	uint16 seq, bool suppress_pkt)
{
	uint16 offset;
#ifdef BCMDBG
	scb_ampdu_tx_t *scb_ampdu;
	ASSERT(ini);

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, ini->scb);
	ASSERT(scb_ampdu);
#endif // endif
	BCM_REFERENCE(suppress_pkt);

#ifdef PROP_TXSTATUS

	if (suppress_pkt) {
		if (IS_SEQ_ADVANCED(seq, ini->tx_exp_seq)) {
			ini->tx_exp_seq = MODINC_POW2(seq, SEQNUM_MAX);
		}
		return TRUE;
	}
#endif /* PROP_TXSTATUS */

	if (ini->tx_exp_seq != seq) {
		offset = MODSUB_POW2(seq, ini->tx_exp_seq, SEQNUM_MAX);
		WL_AMPDU_ERR(("wl%d: r0hole: tx_exp_seq 0x%x, seq 0x%x\n",
			ampdu_tx->wlc->pub->unit, ini->tx_exp_seq, seq));

		/* pkts bypassed (or lagging) on way down */
		if ((offset > SEQNUM_MAX / 2) &&
			!(MODSUB_POW2(ini->max_seq, seq, SEQNUM_MAX) <=
			MODSUB_POW2(ini->max_seq, ini->start_seq, SEQNUM_MAX))) {
			WL_AMPDU_ERR(("wl%d: rlag: seq %d tx_exp_seq %d max_seq %d"
				" start_seq %d acked_seq %d tx_in_transit %d\n"
				"barpending_seq %d bar_ackpending_seq %d"
				" bar_ackpending %d\n", ampdu_tx->wlc->pub->unit, seq,
				ini->tx_exp_seq, ini->max_seq, ini->start_seq, ini->acked_seq,
				ini->tx_in_transit, ini->barpending_seq, ini->bar_ackpending_seq,
				ini->bar_ackpending));
			WLCNTINCR(ampdu_tx->cnt->txrlag);
			AMPDUSCBCNTINCR(scb_ampdu->cnt.txrlag);
			return FALSE;
		}

		WLCNTADD(ampdu_tx->cnt->txr0hole, offset);
	}
	ini->tx_exp_seq = MODINC_POW2(seq, SEQNUM_MAX);
	return TRUE;
} /* ampdu_is_exp_seq_aqm */

/** only called in case of AQM aggregation */
static bool BCMFASTPATH
ampdu_detect_seq_hole(ampdu_tx_info_t *ampdu_tx, uint16 seq, scb_ampdu_tid_ini_t *ini)
{
	bool has_hole = FALSE;
	BCM_REFERENCE(ampdu_tx);

	if (seq != ini->next_enq_seq) {
		WL_AMPDU_TX(("%s: seq hole detected! 0x%x 0x%x\n",
			__FUNCTION__, seq, ini->next_enq_seq));
		has_hole = TRUE;
	}
	ini->next_enq_seq = NEXT_SEQ(seq);
	return has_hole;
}

#ifdef AMPDU_NON_AQM

/**
 * Returns TRUE if 'seq' contains the next expected d11 sequence. If not, sends a Block Ack Req to
 * the remote party. Non AC chip specific function.
 */
static bool BCMFASTPATH
ampdu_is_exp_seq_noaqm(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tid_ini_t *ini,
	uint16 seq, bool suppress_pkt)
{
	uint16 txretry, offset, indx;
	uint i;
	bool found;
	scb_ampdu_tid_ini_non_aqm_t *non_aqm = ini->non_aqm;
#ifdef BCMDBG
	scb_ampdu_tx_t *scb_ampdu;
	ASSERT(ini);

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, ini->scb);
	ASSERT(scb_ampdu);
#endif // endif

	BCM_REFERENCE(suppress_pkt);

#ifdef PROP_TXSTATUS
	if (suppress_pkt) {
		if (IS_SEQ_ADVANCED(seq, ini->tx_exp_seq)) {
			ini->tx_exp_seq = MODINC_POW2(seq, SEQNUM_MAX);
		}
		return TRUE;
	}
#endif /* PROP_TXSTATUS */

	txretry = non_aqm->txretry[TX_SEQ_TO_INDEX(seq)];
	if (txretry == 0) {
		if (ini->tx_exp_seq != seq) {
			offset = MODSUB_POW2(seq, ini->tx_exp_seq, SEQNUM_MAX);
			WL_AMPDU_ERR(("wl%d: r0hole: tx_exp_seq 0x%x, seq 0x%x\n",
				ampdu_tx->wlc->pub->unit, ini->tx_exp_seq, seq));

			/* pkts bypassed (or lagging) on way down */
			if (offset > SEQNUM_MAX / 2) {
				WL_AMPDU_ERR(("wl%d: rlag: tx_exp_seq 0x%x, seq 0x%x\n",
					ampdu_tx->wlc->pub->unit, ini->tx_exp_seq, seq));
				WLCNTINCR(ampdu_tx->cnt->txrlag);
				AMPDUSCBCNTINCR(scb_ampdu->cnt.txrlag);
				return FALSE;
			}

			if (MODSUB_POW2(seq, ini->start_seq, SEQNUM_MAX) >= ini->ba_wsize) {
				WL_AMPDU_ERR(("wl%d: unexpected seq: start_seq 0x%x, seq 0x%x\n",
					ampdu_tx->wlc->pub->unit, ini->start_seq, seq));
				WLCNTINCR(ampdu_tx->cnt->txrlag);
				AMPDUSCBCNTINCR(scb_ampdu->cnt.txrlag);
				return FALSE;
			}

			WLCNTADD(ampdu_tx->cnt->txr0hole, offset);
			/* send bar to move the window */
			indx = TX_SEQ_TO_INDEX(ini->tx_exp_seq);
			for (i = 0; i < offset; i++, indx = NEXT_TX_INDEX(indx))
				setbit(non_aqm->barpending, indx);
			wlc_ampdu_send_bar(ampdu_tx, ini, SEQNUM_CHK);
		}

		ini->tx_exp_seq = MODINC_POW2(seq, SEQNUM_MAX);
	} else {
		if (non_aqm->retry_cnt == 0 ||
			non_aqm->retry_seq[non_aqm->retry_head] != seq) {
			found = FALSE;
			indx = non_aqm->retry_head;
			for (i = 0; i < non_aqm->retry_cnt; i++, indx = NEXT_TX_INDEX(indx)) {
				if (non_aqm->retry_seq[indx] == seq) {
					found = TRUE;
					break;
				}
			}
			if (!found) {
				WL_AMPDU_ERR(("wl%d: rnlag: tx_exp_seq 0x%x, seq 0x%x\n",
					ampdu_tx->wlc->pub->unit, ini->tx_exp_seq, seq));
				WLCNTINCR(ampdu_tx->cnt->txrlag);
				AMPDUSCBCNTINCR(scb_ampdu->cnt.txrlag);
				return FALSE;
			}
			while (non_aqm->retry_seq[non_aqm->retry_head] != seq) {
				WL_AMPDU_ERR(("wl%d: rnhole: retry_seq 0x%x, seq 0x%x\n",
					ampdu_tx->wlc->pub->unit,
					non_aqm->retry_seq[non_aqm->retry_head], seq));
				setbit(non_aqm->barpending,
					TX_SEQ_TO_INDEX(non_aqm->retry_seq[non_aqm->retry_head]));
				WLCNTINCR(ampdu_tx->cnt->txrnhole);
				non_aqm->retry_seq[non_aqm->retry_head] = 0;
				non_aqm->retry_head = NEXT_TX_INDEX(non_aqm->retry_head);
				ASSERT(non_aqm->retry_cnt > 0);
				non_aqm->retry_cnt--;
			}
			wlc_ampdu_send_bar(ampdu_tx, ini, SEQNUM_CHK);
		}
		non_aqm->retry_seq[non_aqm->retry_head] = 0;
		non_aqm->retry_head = NEXT_TX_INDEX(non_aqm->retry_head);
		ASSERT(non_aqm->retry_cnt > 0);
		non_aqm->retry_cnt--;
		ASSERT(non_aqm->retry_cnt < ampdu_tx->config->ba_max_tx_wsize);
	}

	/* discard the frame if it is a retry and we are in pending off state */
	if (txretry && (ini->ba_state == AMPDU_TID_STATE_BA_PENDING_OFF)) {
		setbit(non_aqm->barpending, TX_SEQ_TO_INDEX(seq));
		return FALSE;
	}

	if (MODSUB_POW2(seq, ini->start_seq, SEQNUM_MAX) >= ini->ba_wsize) {
		ampdu_dump_ini(ampdu_tx->wlc, ini);
		ASSERT(MODSUB_POW2(seq, ini->start_seq, SEQNUM_MAX) < ini->ba_wsize);
	}
	return TRUE;
} /* ampdu_is_exp_seq_noaqm */

#endif /* AMPDU_NON_AQM */

/**
 * The epoch field indicates whether the frame can be aggregated into the same A-MPDU as the
 * previous MPDU. Not used in case of host aggregation.
 */
void
wlc_ampdu_change_epoch(ampdu_tx_info_t *ampdu_tx, int fifo, int reason_code)
{
	const char *str = "Undefined";
	BCM_REFERENCE(str);

	switch (reason_code) {
	case AMU_EPOCH_CHG_PLCP:
		WLC_AMPDU_EPOCH_REASON(str, "PLCP");
		WLCNTINCR(ampdu_tx->cnt->echgr1);
		break;
	case AMU_EPOCH_CHG_FID:
		WLCNTINCR(ampdu_tx->cnt->echgr2);
		WLC_AMPDU_EPOCH_REASON(str, "FRAMEID");
		break;
	case AMU_EPOCH_CHG_NAGG:
		WLCNTINCR(ampdu_tx->cnt->echgr3);
		WLC_AMPDU_EPOCH_REASON(str, "ampdu_tx OFF");
		break;
	case AMU_EPOCH_CHG_MPDU:
		WLCNTINCR(ampdu_tx->cnt->echgr4);
		WLC_AMPDU_EPOCH_REASON(str, "reg mpdu");
		break;
	case AMU_EPOCH_CHG_DSTTID:
		WLCNTINCR(ampdu_tx->cnt->echgr5);
		WLC_AMPDU_EPOCH_REASON(str, "DST/TID");
		break;
	case AMU_EPOCH_CHG_SEQ:
		WLCNTINCR(ampdu_tx->cnt->echgr6);
		WLC_AMPDU_EPOCH_REASON(str, "SEQ No");
		break;
	case AMU_EPOCH_CHG_TXC_UPD:
		WLCNTINCR(ampdu_tx->cnt->echgr6);
		WLC_AMPDU_EPOCH_REASON(str, "TXC UPD");
		break;
	case AMU_EPOCH_CHG_TXHDR:
		WLCNTINCR(ampdu_tx->cnt->echgr6);
		WLC_AMPDU_EPOCH_REASON(str, "TX_S_L_HDR");
		break;
	case AMU_EPOCH_CHG_HTC:
		WLCNTINCR(ampdu_tx->cnt->echgr7);
		WLC_AMPDU_EPOCH_REASON(str, "HTC+");
		break;
#ifdef WLTAF
	case AMU_EPOCH_CHG_TAF_STAR:
		WLCNTINCR(ampdu_tx->cnt->echgr8);
		WLC_AMPDU_EPOCH_REASON(str, "TAF*");
		break;
#endif /* WLTAF */
	default:
		ASSERT(0);
		break;
	}

	WL_AMPDU_HWDBG(("wl%d: %s: fifo %d: change epoch for %s\n",
		ampdu_tx->wlc->pub->unit, __FUNCTION__, fifo, str));

	if (fifo < WLC_HW_NFIFO_INUSE(ampdu_tx->wlc)) {
		if (!AMPDU_AQM_ENAB(ampdu_tx->wlc->pub)) {
			ampdu_tx->hwagg[fifo].change_epoch = TRUE;
		} else {
			ampdu_tx->hwagg[fifo].epoch = (ampdu_tx->hwagg[fifo].epoch) ? 0 : 1;
			WL_NONE(("wl%d: %s: fifo %d: change epoch for %s to %d\n",
			         ampdu_tx->wlc->pub->unit, __FUNCTION__, fifo, str,
			         ampdu_tx->hwagg[fifo].epoch));
		}
	}
} /* wlc_ampdu_change_epoch */

#ifdef WLAMPDU_UCODE
static uint16
wlc_ampdu_calc_maxlen_noaqm(ampdu_tx_info_t *ampdu_tx, uint8 plcp0, uint plcp3, uint32 txop)
{
	uint8 is40, sgi;
	uint8 mcs = 0;
	uint16 txoplen = 0;
	uint16 maxlen = 0xffff;
	ampdu_tx_config_t *ampdu_tx_cfg = ampdu_tx->config;

	is40 = (plcp0 & MIMO_PLCP_40MHZ) ? 1 : 0;
	sgi = PLCP3_ISSGI(plcp3) ? 1 : 0;
	mcs = plcp0 & ~MIMO_PLCP_40MHZ;
	ASSERT(VALID_MCS(mcs));

	if (txop) {
		/* rate is in Kbps; txop is in usec
		 * ==> len = (rate * txop) / (1000 * 8)
		 */
		txoplen = (MCS_RATE(mcs, is40, sgi) >> 10) * (txop >> 3);

		maxlen = MIN((txoplen),
			(ampdu_tx_cfg->max_txlen[MCS2IDX(mcs)][is40][sgi] *
			ampdu_tx_cfg->ba_tx_wsize));
	}
	WL_AMPDU_HW(("wl%d: %s: txop %d txoplen %d maxlen %d\n",
		ampdu_tx->wlc->pub->unit, __FUNCTION__, txop, txoplen, maxlen));

	return maxlen;
}
#endif /* WLAMPDU_UCODE */

extern bool
wlc_ampdu_was_ampdu(ampdu_tx_info_t *ampdu_tx, int fifo)
{
	return TRUE;
}

/**
 * Change epoch and save epoch determining params. Return updated epoch.
 * Not used in case of host aggregation.
 */
uint8
wlc_ampdu_chgnsav_epoch(ampdu_tx_info_t *ampdu_tx, int fifo, int reason_code,
	struct scb *scb, uint8 tid, bool htc, wlc_txh_info_t* txh_info)
{
	ampdumac_info_t *hwagg;
	bool isAmpdu;
	int8* tsoHdrPtr = txh_info->tsoHdrPtr;

	/* when switch from non-aggregate ampdu to aggregate ampdu just change
	 * epoch determining parameters. do not change epoch.
	 */
	if (reason_code != AMU_EPOCH_NO_CHANGE) {
		wlc_ampdu_change_epoch(ampdu_tx, fifo, reason_code);
	}

	isAmpdu = wlc_txh_get_isAMPDU(ampdu_tx->wlc, txh_info);

	hwagg = &(ampdu_tx->hwagg[fifo]);
	hwagg->prev_scb = scb;
	hwagg->prev_tid = tid;
	hwagg->prev_ft = AMPDU_NONE;
	hwagg->prev_htc = htc;
	if (isAmpdu) {
		if (RATELINKMEM_ENAB(ampdu_tx->wlc->pub)) {
			/** XXX
			 * Not differentiating between AMPDU_11VHT & AMPDU_11N etc.
			 * Reasoning:
			 * Only use case is AMPDU_NONE or not to flip epoch
			 */
			hwagg->prev_ft = AMPDU_11VHT;
		} else {
			uint16 tmp = ltoh16(txh_info->PhyTxControlWord0) &
				D11_PHY_TXC_FT_MASK(ampdu_tx->wlc->pub->corerev);

			ASSERT(tmp ==  PHY_TXC_FT_HT ||
			       tmp == PHY_TXC_FT_VHT ||
			       tmp == PHY_TXC_FT_HE);

			if (tmp == PHY_TXC_FT_HT) {
				hwagg->prev_ft = AMPDU_11N;
			} else if (tmp == PHY_TXC_FT_VHT) {
				hwagg->prev_ft = AMPDU_11VHT;
			} else if (tmp == PHY_TXC_FT_HE) {
				hwagg->prev_ft = AMPDU_11HE;
			} else {
				ASSERT(0);
			}
		}
	}

	if (!RATELINKMEM_ENAB(ampdu_tx->wlc->pub)) {
		/** XXX
		 * Only used in wlc_ampdu_epoch_params_chg, to determine epoch flip on txparams
		 * [For D11REV >= 128 ucode will handle epoch completion due to txparams change]
		 */
		hwagg->prev_txphyctl0 = txh_info->PhyTxControlWord0;
		hwagg->prev_txphyctl1 = txh_info->PhyTxControlWord1;
		hwagg->prev_plcp[0] = txh_info->plcpPtr[0];
		hwagg->prev_plcp[1] = txh_info->plcpPtr[3];
#if D11CONF_GE(40)
		if (hwagg->prev_ft == AMPDU_11VHT) {
			hwagg->prev_plcp[3] = txh_info->plcpPtr[1];
			hwagg->prev_plcp[2] = txh_info->plcpPtr[2];
			hwagg->prev_plcp[4] = txh_info->plcpPtr[4];
		}
#endif /* D11CONF_GE(40) */
	}

	hwagg->prev_shdr = (tsoHdrPtr[2] & TOE_F2_TXD_HEAD_SHORT);

	WL_AMPDU_HWDBG(("%s fifo %d epoch %d\n", __FUNCTION__, fifo, hwagg->epoch));

	return hwagg->epoch;
} /* wlc_ampdu_chgnsav_epoch */

/**
 * Check whether txparams have changed for ampdu tx, this would be a reason to complete the current
 * epoch. [For D11REV >= 128 ucode will handle epoch completion due to txparams change]
 *
 * txparams include <frametype: 11n or 11vht>, <rate info in plcp>, <antsel>
 * Call this function *only* for aggregatable frames
 * This function is not called in case of host aggregation.
 */
static bool
wlc_ampdu_epoch_params_chg(wlc_info_t *wlc, ampdumac_info_t *hwagg, d11txhdr_t *txh_u)
{
	uint8 *plcpPtr = NULL;
	bool chg;
	uint8 phy_ft, frametype = AMPDU_NONE;
	uint16 PhyTxControlWord_0 = 0;
	uint16 PhyTxControlWord_1 = 0;

	ASSERT(D11REV_LT(wlc->pub->corerev, 128));
	{ /* revs 40 - 128 */
		d11actxh_t *txh = &txh_u->rev40;
		d11actxh_rate_t *rateInfo;

		rateInfo = WLC_TXD_RATE_INFO_GET(txh, wlc->pub->corerev);
		plcpPtr = rateInfo[0].plcp;
		PhyTxControlWord_0 = rateInfo[0].PhyTxControlWord_0;
		PhyTxControlWord_1 = rateInfo[0].PhyTxControlWord_1;

		/* map phy dependent frametype to independent frametype enum */
		phy_ft = ltoh16(PhyTxControlWord_0) & D11AC_PHY_TXC_FT_MASK;

		if (phy_ft == PHY_TXC_FT_HT)
			frametype = AMPDU_11N;
		else if (phy_ft == PHY_TXC_FT_VHT)
			frametype = AMPDU_11VHT;
		else {
			ASSERT(0);
			return TRUE;
		}
	}

	chg = (frametype != hwagg->prev_ft) ||
		(PhyTxControlWord_0 != hwagg->prev_txphyctl0) ||
		(PhyTxControlWord_1 != hwagg->prev_txphyctl1);

	if (chg) {
		return TRUE;
	}

#if D11CONF_GE(40)
	if (frametype == AMPDU_11VHT) {
		/* VHT frame: compare plcp0-4 */
		if (plcpPtr[0] != hwagg->prev_plcp[0] ||
		    plcpPtr[1] != hwagg->prev_plcp[3] ||
		    plcpPtr[2] != hwagg->prev_plcp[2] ||
		    plcpPtr[3] != hwagg->prev_plcp[1] ||
		    plcpPtr[4] != hwagg->prev_plcp[4]) {
			chg = TRUE;
		}
	} else
#endif // endif
	{
		/* HT frame: only care about plcp0 and plcp3 */
		if (plcpPtr[0] != hwagg->prev_plcp[0] ||
		    plcpPtr[3] != hwagg->prev_plcp[1])
			chg = TRUE;
	}

	return chg;
} /* wlc_ampdu_epoch_params_chg */

typedef struct ampdu_creation_params
{
	wlc_bsscfg_t *cfg;
	scb_ampdu_tx_t *scb_ampdu;
	wlc_txh_info_t* tx_info;

	scb_ampdu_tid_ini_t *ini;
	ampdu_tx_info_t *ampdu;

} ampdu_create_info_t;

/**
 * Only called for AQM (AC) chips. Returns TRUE if MPDU 'idx' within a TxStatus
 * was acked by a remote party.
 */
static INLINE bool
wlc_txs_was_acked(wlc_info_t *wlc, tx_status_macinfo_t *status, uint16 idx)
{
	BCM_REFERENCE(wlc);
	ASSERT(idx < 64);
	/* Byte 4-11 of 2nd pkg */
	if (idx < 32) {
		return (status->ack_map1 & (1 << idx)) ? TRUE : FALSE;
	} else {
		idx -= 32;
		return (status->ack_map2 & (1 << idx)) ? TRUE : FALSE;
	}
}

#ifdef WL_MUPKTENG
void
wlc_ampdu_mupkteng_fill_percache_info(ampdu_tx_info_t *ampdu_tx, struct scb *scb, uint8 tid,
	d11actxh_cache_t *txh_cache_info)
{
	scb_ampdu_tx_t *scb_ampdu;
	uint16 ampdu_dur;
	d11txh_cache_common_t	*cache_info;

	cache_info = &txh_cache_info->common;

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	BCM_REFERENCE(scb_ampdu);

	cache_info->PrimeMpduMax = AMPDU_DEF_MAX_RELEASE_AQM;
	cache_info->FallbackMpduMax = AMPDU_DEF_MAX_RELEASE_AQM;

	/* maximum duration is in 16 usec, putting into upper 12 bits
	 * ampdu density is in low 4 bits
	 */

	ampdu_dur = (ampdu_tx->config->dur & D11AC_AMPDU_MAX_DUR_MASK);
	ampdu_dur |= (AMPDU_DEF_MPDU_DENSITY << D11AC_AMPDU_MIN_DUR_IDX_SHIFT);
	cache_info->AmpduDur = htol16(ampdu_dur);

	/* Fill in as BAWin of recvr -1 */
	cache_info->BAWin = AMPDU_DEF_MAX_RELEASE_AQM - 1;
	cache_info->MaxAggLen = AMPDU_DEF_MAX_RX_FACTOR;
} /* wlc_ampdu_mupkteng_fill_percache_info */

/** MUPKTENG function to fill link mem entry (Rev 128) content, called by wlc_tx_fill_link_entry */
void
wlc_ampdu_mupkteng_fill_link_entry_info(ampdu_tx_info_t *ampdu_tx, struct scb *scb,
	d11linkmem_entry_t* link_entry)
{
	int tid;

	ASSERT(D11REV_GE(ampdu_tx->wlc->pub->corerev, 128));
	ASSERT(link_entry != NULL);

	link_entry->ampdu_info = (256 << D11_REV128_AMPDUMPDUALL_SHIFT);

	link_entry->PPET_AmpMinDur &= ~D11_REV128_AMPMINDUR_MASK;
	link_entry->PPET_AmpMinDur |= htol32(MPDU_DENSITY_REV128(AMPDU_DEF_MPDU_DENSITY)
					<< D11_REV128_AMPMINDUR_SHIFT);
	link_entry->AmpMaxDurIdx = htol16(ampdu_tx->config->dur);

	for (tid = 0; tid < AMPDU_MAX_SCB_TID; tid++) {
		/* Fill in as BAWin of recvr -1 */
		link_entry->ampdu_ptid[tid].BAWin = 255;
		link_entry->ampdu_ptid[tid].ampdu_mpdu = 255;
	}
} /* wlc_ampdu_mupkteng_fill_link_entry_info */
#endif /* WL_MUPKTENG */

/** Compute and fill the link mem entry (Rev 128) content, called by wlc_tx_fill_link_entry. */
void
wlc_ampdu_fill_link_entry_info(ampdu_tx_info_t *ampdu_tx, struct scb *scb,
	d11linkmem_entry_t* link_entry)
{
	scb_ampdu_tx_t *scb_ampdu;
	scb_ampdu_tid_ini_t *ini;
	uint16 ampdu_dur;
	uint16 ampdu_info;
	uint16 scb_ampdu_min;
	int tid;
	uint8 mpdu_density;

	ASSERT(D11REV_GE(ampdu_tx->wlc->pub->corerev, 128));
	ASSERT(link_entry != NULL);

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	ASSERT(scb_ampdu != NULL);

	mpdu_density = MPDU_DENSITY_REV128(scb_ampdu->mpdu_density);
	if (scb_ampdu->min_of_max_dur_idx == AMPDU_MAXDUR_INVALID_IDX) {
		scb_ampdu_min = (uint16)(mpdu_density);
	} else {
		uint16 max_dur = scb_ampdu->module_max_dur[scb_ampdu->min_of_max_dur_idx];
		scb_ampdu_min = MIN(mpdu_density, max_dur);
	}
	link_entry->PPET_AmpMinDur |= htol32(scb_ampdu_min << D11_REV128_AMPMINDUR_SHIFT);

	if (BAND_2G(ampdu_tx->wlc->band->bandtype) &&
		(ampdu_tx->config->btcx_dur_flag)) {
		ampdu_dur = BTCX_AMPDU_MAX_DUR;
	} else {
		ampdu_dur = ampdu_tx->config->dur;
	}
	link_entry->AmpMaxDurIdx = htol16(ampdu_dur);

	for (tid = 0; tid < AMPDU_MAX_SCB_TID; tid++) {
		if (((ini = scb_ampdu->ini[tid]) != NULL) && (ini->ba_wsize > 0)) {
			link_entry->ampdu_ptid[tid].BAWin = ini->ba_wsize - 1;
			link_entry->ampdu_ptid[tid].ampdu_mpdu = scb_ampdu->max_pdu - 1;
		} else {
			link_entry->ampdu_ptid[tid].BAWin = 0;
			link_entry->ampdu_ptid[tid].ampdu_mpdu = 0;
		}
	}
	ampdu_info = (scb_ampdu->max_pdu << D11_REV128_AMPDUMPDUALL_SHIFT);
	ampdu_info |= (scb_ampdu->max_rxfactor & D11_REV128_MAXRXFACTOR_MASK);
	link_entry->ampdu_info = htol16(ampdu_info);
} /* wlc_ampdu_fill_link_entry_info */

void
wlc_ampdu_fill_percache_info(ampdu_tx_info_t *ampdu_tx, struct scb *scb, uint8 tid,
	d11actxh_cache_t *txh_cache_info)
{
	scb_ampdu_tx_t *scb_ampdu;
	scb_ampdu_tid_ini_t *ini;
	uint16 ampdu_dur;
	uint16 scb_ampdu_min;
	d11txh_cache_common_t *cache_info;

	cache_info = &txh_cache_info->common;

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);

	ASSERT(scb_ampdu != NULL);
	ASSERT(tid < AMPDU_MAX_SCB_TID);

	ini = scb_ampdu->ini[tid];

	cache_info->PrimeMpduMax = scb_ampdu->max_pdu; /* max pdus allowed in ampdu */
	cache_info->FallbackMpduMax = scb_ampdu->max_pdu;

	if (scb_ampdu->min_of_max_dur_idx == AMPDU_MAXDUR_INVALID_IDX) {
		scb_ampdu_min = (uint16)(scb_ampdu->mpdu_density);
	} else {
		uint16 max_dur = scb_ampdu->module_max_dur[scb_ampdu->min_of_max_dur_idx];
		scb_ampdu_min = MIN(scb_ampdu->mpdu_density, max_dur);
	}

	if (BAND_2G(ampdu_tx->wlc->band->bandtype) &&
		ampdu_tx->config->btcx_dur_flag) {
		ampdu_dur = (BTCX_AMPDU_MAX_DUR & D11AC_AMPDU_MAX_DUR_MASK);
	} else {
		ampdu_dur = (ampdu_tx->config->dur & D11AC_AMPDU_MAX_DUR_MASK);
	}

	ampdu_dur |= (scb_ampdu_min << D11AC_AMPDU_MIN_DUR_IDX_SHIFT);

	cache_info->AmpduDur = htol16(ampdu_dur);

	/* Fill in as BAWin of recvr -1 */
	cache_info->BAWin = ini->ba_wsize - 1;
	/* max agg len in bytes, specified as 2^X */
	cache_info->MaxAggLen = scb_ampdu->max_rxfactor;
} /* wlc_ampdu_fill_percache_info */

/**
 * Called by higher layer when it wants to send an MSDU to the d11 core's DMA/FIFOs. Typically, an
 * MSDU is first given by a higher layer to the AMPDU subsystem for aggregation and subsequent
 * queuing in an AMPDU internal software queue (see wlc_ampdu_agg()). Next, the AMPDU subsystem
 * decides to 'release' the packet, which means that it forwards the packet to wlc.c, which adds it
 * to the wlc transmit queue.
 *
 * Subsequently, wlc.c calls this function in order to forward the packet towards the d11 core. The
 * MSDU is transformed into an MPDU in this function.
 *
 * @param output_q : single priority packet queue
 *
 * Returns BCME_BUSY when there is no room to queue the packet.
 */
int BCMFASTPATH
wlc_sendampdu(ampdu_tx_info_t *ampdu_tx, wlc_txq_info_t *qi, void **pdu, int prec,
	struct spktq *output_q, int *pkt_cnt, uint fifo)
{
	wlc_info_t *wlc;
	osl_t *osh;
	void *p;
	struct scb *scb;
	int err = 0;
	scb_ampdu_tx_t *scb_ampdu;
	scb_ampdu_tid_ini_t *ini;
	uint8 tid;

	ASSERT(ampdu_tx);
	ASSERT(qi);
	ASSERT(pdu);

	wlc = ampdu_tx->wlc;
	osh = wlc->osh;
	p = *pdu;
	ASSERT(p);
	if (p == NULL)
		return err;
	scb = WLPKTTAGSCBGET(p);
	ASSERT(scb != NULL);

	ASSERT(!SCB_DEL_IN_PROGRESS(scb)); /* SCB should not be marked for deletion */

	/* SCB setting may have changed during transition, so just discard the frame */
	if (!SCB_AMPDU(scb)) {
		WL_AMPDU(("wlc_sendampdu: entry func: scb ampdu flag null -- error\n"));
		/* ASSERT removed for 4360 as */
		/* the much longer tx pipeline (amsdu+ampdu) made it even more likely */
		/* to see this case */
		WLCNTINCR(ampdu_tx->cnt->orphan);
		wlc_tx_status_update_counters(wlc, p, scb,
			NULL, WLC_TX_STS_TOSS_NON_AMPDU_SCB, 1);
		PKTFREE(osh, p, TRUE);
		*pdu = NULL;
#ifdef WLTAF
		wlc_taf_scb_state_update(ampdu_tx->wlc->taf_handle, scb, NULL, TAF_SCBSTATE_RESET);
#endif // endif
		return err;
	}

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);

	ASSERT(scb_ampdu);

	tid = (uint8)PKTPRIO(p);
	ASSERT(tid < AMPDU_MAX_SCB_TID);

	ini = scb_ampdu->ini[tid];

	if (!ini || (ini->ba_state == AMPDU_TID_STATE_BA_PENDING_ON)) {
		WL_AMPDU(("wlc_sendampdu: bailing out for bad ini (%p)/bastate\n",
			OSL_OBFUSCATE_BUF(ini)));
		WLCNTINCR(ampdu_tx->cnt->orphan);
#ifdef WLTAF
		wlc_taf_link_state(ampdu_tx->wlc->taf_handle, scb, tid, NULL,
			TAF_LINKSTATE_HARD_RESET);
#endif // endif
		wlc_tx_status_update_counters(wlc, p, scb,
			NULL, WLC_TX_STS_TOSS_BAD_INI, 1);
		PKTFREE(osh, p, TRUE);
		*pdu = NULL;
		return err;
	}

	/* Something is blocking data packets */
	if (wlc->block_datafifo) {
		WL_AMPDU(("wlc_sendampdu: datafifo blocked\n"));
		return BCME_BUSY;
	}

	if (AMPDU_AQM_ENAB(ampdu_tx->wlc->pub)) {
		return wlc_sendampdu_aqm(ampdu_tx, qi, pdu, prec, output_q, pkt_cnt, fifo);
	} else
#ifdef AMPDU_NON_AQM
	{
		return wlc_sendampdu_noaqm(ampdu_tx, qi, pdu, prec, output_q, pkt_cnt, fifo);
	}
#endif /* AMPDU_NON_AQM */

	return err;
} /* wlc_sendampdu */

#ifdef AMPDU_NON_AQM

#if AMPDU_MAX_MPDU > (TXFS_WSZ_AC_BE + TXFS_WSZ_AC_BK + TXFS_WSZ_AC_VI + \
	TXFS_WSZ_AC_VO)
#define AMPDU_MAX_PKTS	AMPDU_MAX_MPDU
#else
#define AMPDU_MAX_PKTS	(TXFS_WSZ_AC_BE + TXFS_WSZ_AC_BK + TXFS_WSZ_AC_VI + TXFS_WSZ_AC_VO)
#endif // endif

/**
 * Invoked when higher layer tries to send a transmit SDU, or an MPDU (in case of retransmit), to
 * the d11 core. Not called for AC chips unless AQM is disabled. Returns BCME_BUSY when there is no
 * room to queue the packet.
 *
 * @param output_q  Single priority packet queue
 */
static int BCMFASTPATH
wlc_sendampdu_noaqm(ampdu_tx_info_t *ampdu_tx, wlc_txq_info_t *qi, void **pdu, int prec,
	struct spktq *output_q, int *pkt_cnt, uint fifo)
{
	wlc_info_t *wlc;
	osl_t *osh;
	void *p;
	void *pkt[AMPDU_MAX_PKTS];
	uint8 tid, ndelim, ac;
	int err = 0, txretry = -1;
	uint8 preamble_type = WLC_GF_PREAMBLE;
	uint8 fbr_preamble_type = WLC_GF_PREAMBLE;
	uint8 rts_preamble_type = WLC_LONG_PREAMBLE;
	uint8 rts_fbr_preamble_type = WLC_LONG_PREAMBLE;

	bool rr = FALSE, fbr = FALSE, vrate_probe = FALSE;
	uint i, count = 0, npkts, nsegs, seg_cnt = 0, next_fifo;
	uint16 plen, len, seq, mcl, mch, indx, frameid, dma_len = 0;
	uint32 ampdu_len, maxlen = 0;
#ifdef WL11K
	uint32 pktlen = 0;
#endif // endif
	d11txh_pre40_t *txh = NULL;
	uint8 *plcp;
	struct dot11_header *h;
	struct scb *scb;
	scb_ampdu_tx_t *scb_ampdu;
	scb_ampdu_tid_ini_t *ini;
	scb_ampdu_tid_ini_non_aqm_t *non_aqm;
	uint8 mcs = 0;
	bool regmpdu = FALSE;
	bool use_rts = FALSE, use_cts = FALSE;
	ratespec_t rspec = 0, rspec_fallback = 0;
	ratespec_t rts_rspec = 0, rts_rspec_fallback = 0;
	struct dot11_rts_frame *rts = NULL;
	uint8 rr_retry_limit;
	wlc_fifo_info_t *f;
	bool fbr_iscck, use_mfbr = FALSE;
	uint32 txop = 0;
#ifdef WLAMPDU_UCODE
	ampdumac_info_t *hwagg;
	int sc; /* Side channel index */
	uint aggfifo_space = 0;
#endif // endif
	wlc_bsscfg_t *cfg;
	uint16 txburst_limit;
	ampdu_tx_config_t *ampdu_tx_cfg = ampdu_tx->config;
	bool suppress_pkt = FALSE;
#ifdef PKTC
	wlc_key_t *key = NULL;
	wlc_key_info_t key_info;
#endif /* PKTC */

	wlc = ampdu_tx->wlc;
	osh = wlc->osh;
	p = *pdu;

	tid = (uint8)PKTPRIO(p);
	ASSERT(tid < AMPDU_MAX_SCB_TID);
	ac = WME_PRIO2AC(tid);
	ASSERT(ac < AC_COUNT);
	f = ampdu_tx_cfg->fifo_tb + prio2fifo[tid];

	scb = WLPKTTAGSCBGET(p);
	ASSERT(scb != NULL);

	cfg = SCB_BSSCFG(scb);
	ASSERT(cfg != NULL);

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	ASSERT(scb_ampdu);

	ini = scb_ampdu->ini[tid];
	non_aqm = ini->non_aqm;
	rr_retry_limit = ampdu_tx_cfg->rr_retry_limit_tid[tid];

	ASSERT(ini->scb == scb);

	ASSERT(!RATELINKMEM_ENAB(wlc->pub));

#ifdef WLAMPDU_UCODE
	sc = prio2fifo[tid];
	hwagg = &ampdu_tx->hwagg[sc];

	/* agg-fifo space */
	if (AMPDU_UCODE_ENAB(wlc->pub)) {
		aggfifo_space = get_aggfifo_space_noaqm(ampdu_tx, sc);
		WL_AMPDU_HWDBG(("%s: aggfifo_space %d txfifo %d\n", __FUNCTION__, aggfifo_space,
			R_REG(wlc->osh, D11_XmtFifoFrameCnt(wlc))));
#if defined(BCMDBG) || defined(BCMDBG_AMPDU)
		ampdu_tx->amdbg->schan_depth_histo[MIN(AMPDU_MAX_MPDU, aggfifo_space)]++;
#endif // endif
		if (aggfifo_space == 0) {
			return BCME_BUSY;
		}
		/* check if there are enough descriptors available */
		nsegs = pktsegcnt(osh, p);
		if (TXAVAIL(wlc, sc) <= nsegs) {
			WLCNTINCR(ampdu_tx->cnt->txfifofull);
			return BCME_BUSY;
		}
	}
#endif /* WLAMPDU_UCODE */

	/* compute txop once for all */
	txop = cfg->wme->edcf_txop[WME_PRIO2AC(tid)];
	txburst_limit = WLC_HT_CFG_TXBURST_LIMIT(wlc->hti, cfg);

	if (txop && txburst_limit)
		txop = MIN(txop, txburst_limit);
	else if (txburst_limit)
		txop = txburst_limit;

	ampdu_len = 0;
	dma_len = 0;
	while (p) {
		ASSERT(WLPKTTAG(p)->flags & WLF_AMPDU_MPDU);
		seq = WLPKTTAG(p)->seq;
#ifdef PROP_TXSTATUS
		seq = WL_SEQ_GET_NUM(seq);
#endif /* PROP_TXSTATUS */

		/* N.B.: WLF_TXHDR flag indicates the MPDU is being retransmitted
		 * due to ampdu retry or d11 suppression.
		 */
		if (WLPKTTAG(p)->flags & WLF_TXHDR) {
			err = wlc_prep_pdu(wlc, scb, p, fifo);
		} else
#ifdef PKTC
		if ((err = wlc_prep_sdu_fast(wlc, cfg, scb, p, fifo,
			&key, &key_info)) == BCME_UNSUPPORTED)
#endif /* PKTC */
		{
			/* prep_sdu converts an SDU into MPDU (802.11) format */
			err = wlc_prep_sdu(wlc, scb, &p, (int*)&npkts, fifo);
		}

		if (err) {
			if (err == BCME_BUSY) {
				WL_AMPDU_TX(("wl%d: wlc_sendampdu: prep_xdu retry; seq 0x%x\n",
					wlc->pub->unit, seq));
				WLCNTINCR(ampdu_tx->cnt->sduretry);
				*pdu = p;
				break;
			}

			/* error in the packet; reject it */
			WL_AMPDU_ERR(("wl%d: wlc_sendampdu: prep_xdu rejected; seq 0x%x\n",
				wlc->pub->unit, seq));
			WLCNTINCR(ampdu_tx->cnt->sdurejected);
			AMPDUSCBCNTINCR(scb_ampdu->cnt.sdurejected);

			/* send bar since this may be blocking pkts in scb */
			if (ini->tx_exp_seq == seq) {
				setbit(non_aqm->barpending, TX_SEQ_TO_INDEX(seq));
				wlc_ampdu_send_bar(ampdu_tx, ini, SEQNUM_CHK);
				ini->tx_exp_seq = MODINC_POW2(seq, SEQNUM_MAX);
			}
			*pdu = NULL;
			break;
		}

		WL_AMPDU_TX(("wl%d: wlc_sendampdu: prep_xdu success; seq 0x%x tid %d\n",
			wlc->pub->unit, seq, tid));

		/* pkt is good to be aggregated */

		ASSERT(WLPKTTAG(p)->flags & WLF_TXHDR);
		ASSERT(WLPKTTAG(p)->flags & WLF_AMPDU_MPDU);

		txh = (d11txh_pre40_t *)PKTDATA(osh, p);
		plcp = (uint8 *)(txh + 1);
		h = (struct dot11_header*)(plcp + D11_PHY_HDR_LEN);
		seq = ltoh16(h->seq) >> SEQNUM_SHIFT;
		indx = TX_SEQ_TO_INDEX(seq);

		/* during roam we get pkts with null a1. kill it here else
		 * does not find scb on dotxstatus and things gets out of sync
		 */
		if (ETHER_ISNULLADDR(&h->a1)) {
			WL_AMPDU_ERR(("wl%d: sendampdu; dropping frame with null a1\n",
				wlc->pub->unit));
			WLCNTINCR(ampdu_tx->cnt->txdrop);
			if (ini->tx_exp_seq == seq) {
				setbit(non_aqm->barpending, indx);
				wlc_ampdu_send_bar(ampdu_tx, ini, SEQNUM_CHK);
				ini->tx_exp_seq = MODINC_POW2(seq, SEQNUM_MAX);
			}

			wlc_tx_status_update_counters(wlc, p,
				scb, NULL, WLC_TX_STS_TOSS_BAD_ADDR, 1);

			PKTFREE(osh, p, TRUE);
			err = BCME_ERROR;
			*pdu = NULL;
			break;
		}

	BCM_REFERENCE(suppress_pkt);

#ifdef PROP_TXSTATUS
		if (GET_DRV_HAS_ASSIGNED_SEQ(WLPKTTAG(p)->seq)) {
			suppress_pkt = TRUE;
			/* Now the dongle owns the packet */
			RESET_DRV_HAS_ASSIGNED_SEQ(WLPKTTAG(p)->seq);
		}
#endif /* PROP_TXSTATUS */

		/* If the packet was not retrieved from HW FIFO before attempting
		 * over the air for multi-queue or p2p
		 */
		if (WLPKTTAG(p)->flags & WLF_FIFOPKT) {
			/* seq # in suppressed frames have been validated before
			 * so don't validate them again...unless there are queuing
			 * issue(s) causing them to be out-of-order.
			 */
		} else if (!ampdu_is_exp_seq_noaqm(ampdu_tx, ini, seq, suppress_pkt)) {
			wlc_tx_status_update_counters(wlc, p,
				scb, NULL, WLC_TX_STS_TOSS_INV_SEQ, 1);

			PKTFREE(osh, p, TRUE);
			err = BCME_ERROR;
			*pdu = NULL;
			break;
		}

		if (!isset(non_aqm->ackpending, indx)) {
			WL_AMPDU_ERR(("wl%d: %s: seq 0x%x\n",
			          ampdu_tx->wlc->pub->unit, __FUNCTION__, seq));
			ampdu_dump_ini(wlc, ini);
			ASSERT(isset(non_aqm->ackpending, indx));
		}

		/* check mcl fields and test whether it can be agg'd */
		mcl = ltoh16(txh->MacTxControlLow);
		mcl &= ~TXC_AMPDU_MASK;
		fbr_iscck = !(ltoh16(txh->XtraFrameTypes) & 0x3);
		txh->PreloadSize = 0; /* always default to 0 */

		/*  Handle retry limits */
		if (AMPDU_HOST_ENAB(wlc->pub)) {

			/* first packet in ampdu */
			if (txretry == -1) {
			    txretry = non_aqm->txretry[indx];
			    vrate_probe = (WLPKTTAG(p)->flags & WLF_VRATE_PROBE) ? TRUE : FALSE;
			}

			ASSERT(txretry == non_aqm->txretry[indx]);

			if ((txretry == 0) ||
			    (txretry < rr_retry_limit &&
			     !(vrate_probe && ampdu_tx_cfg->probe_mpdu))) {
				rr = TRUE;
				ASSERT(!fbr);
			} else {
				/* send as regular mpdu if not a mimo frame or in ps mode
				 * or we are in pending off state
				 * make sure the fallback rate can be only HT or CCK
				 */
				fbr = TRUE;
				ASSERT(!rr);

				regmpdu = fbr_iscck;
				if (regmpdu) {
					mcl &= ~(TXC_SENDRTS | TXC_SENDCTS | TXC_LONGFRAME);
					mcl |= TXC_STARTMSDU;
					txh->MacTxControlLow = htol16(mcl);

					mch = ltoh16(txh->MacTxControlHigh);
					mch |= TXC_AMPDU_FBR;
					txh->MacTxControlHigh = htol16(mch);
					break;
				}
			}
		} /* AMPDU_HOST_ENAB */

		/* extract the length info */
		len = fbr_iscck ? WLC_GET_CCK_PLCP_LEN(txh->FragPLCPFallback)
			: WLC_GET_MIMO_PLCP_LEN(txh->FragPLCPFallback);

		if (!(WLPKTTAG(p)->flags & WLF_MIMO) ||
		    (ini->ba_state == AMPDU_TID_STATE_BA_PENDING_OFF) ||
		    (ini->ba_state == AMPDU_TID_STATE_BA_OFF) ||
		    SCB_PS(scb)) {
			/* clear ampdu related settings if going as regular frame */
			if ((WLPKTTAG(p)->flags & WLF_MIMO) &&
			    (SCB_PS(scb) ||
			     (ini->ba_state == AMPDU_TID_STATE_BA_PENDING_OFF) ||
			     (ini->ba_state == AMPDU_TID_STATE_BA_OFF))) {
				WLC_CLR_MIMO_PLCP_AMPDU(plcp);
				WLC_SET_MIMO_PLCP_LEN(plcp, len);
			}
			regmpdu = TRUE;
			mcl &= ~(TXC_SENDRTS | TXC_SENDCTS | TXC_LONGFRAME);
			txh->MacTxControlLow = htol16(mcl);
			break;
		}

		/* pkt is good to be aggregated */

		if (non_aqm->txretry[indx]) {
			WLCNTINCR(ampdu_tx->cnt->u0.txretry_mpdu);
			if (AMPDU_HOST_ENAB(wlc->pub)) {
				WLCNTSCBINCR(scb->scb_stats.tx_pkts_retries);
				WLCNTINCR(wlc->pub->_cnt->txretrans);
			}
		}

		/* retrieve null delimiter count */
		ndelim = txh->RTSPLCPFallback[AMPDU_FBR_NULL_DELIM];
		seg_cnt += pktsegcnt(osh, p);

		if (!WL_AMPDU_HW_ON())
			WL_AMPDU_TX(("wl%d: wlc_sendampdu: mpdu %d plcp_len %d\n",
				wlc->pub->unit, count, len));

#ifdef WLAMPDU_UCODE
		/*
		 * aggregateable mpdu. For ucode agg,
		 * test whether need to break or change the epoch
		 */
		if (AMPDU_UCODE_ENAB(wlc->pub)) {
			uint8 max_pdu;

			ASSERT((uint)sc == fifo);

			/* check if link or tid has changed */
			if (scb != hwagg->prev_scb || tid != hwagg->prev_tid) {
				hwagg->prev_scb = scb;
				hwagg->prev_tid = tid;
				wlc_ampdu_change_epoch(ampdu_tx, sc, AMU_EPOCH_CHG_DSTTID);
			}

			/* mark every MPDU's plcp to indicate ampdu */
			WLC_SET_MIMO_PLCP_AMPDU(plcp);

			/* rate/sgi/bw/etc phy info */
			if (plcp[0] != hwagg->prev_plcp[0] ||
			    (plcp[3] & 0xf0) != hwagg->prev_plcp[1]) {
				hwagg->prev_plcp[0] = plcp[0];
				hwagg->prev_plcp[1] = plcp[3] & 0xf0;
				wlc_ampdu_change_epoch(ampdu_tx, sc, AMU_EPOCH_CHG_PLCP);
			}
			/* frameid -- force change by the rate selection algo */
			frameid = ltoh16(txh->TxFrameID);
			if (D11_TXFID_IS_RATE_PROBE(corerev, frameid)) {
				wlc_ampdu_change_epoch(ampdu_tx, sc, AMU_EPOCH_CHG_FID);
				wlc_scb_ratesel_probe_ready(wlc->wrsi, scb, frameid, TRUE, 0, ac);
			}

			/* Set bits 9:10 to any nonzero value to indicate to
			 * ucode that this packet can be aggregated
			 */
			mcl |= (TXC_AMPDU_FIRST << TXC_AMPDU_SHIFT);
			/* set sequence number in tx-desc */
			txh->AmpduSeqCtl = htol16(h->seq);

			/*
			 * compute aggregation parameters
			 */
			/* limit on number of mpdus per ampdu */
			mcs = plcp[0] & ~MIMO_PLCP_40MHZ;
			max_pdu = scb_ampdu->max_pdu;

			/* MaxAggSize in duration(usec) and bytes for ucode agg */

			/* HW PR#37644 WAR : stop aggregating if mpdu
			 * count above a threshold for current rate
			 */
			if (MCS_RATE(mcs, TRUE, FALSE) >= f->dmaxferrate) {
				uint16 preloadsize = f->ampdu_pld_size;
				if (D11REV_GE(wlc->pub->corerev, 42))
					preloadsize >>= 2;
				txh->PreloadSize = htol16(preloadsize);
				if (max_pdu > f->mcs2ampdu_table[MCS2IDX(mcs)])
					max_pdu = f->mcs2ampdu_table[MCS2IDX(mcs)];
			}
			txh->u1.MaxAggLen = htol16(wlc_ampdu_calc_maxlen_noaqm(ampdu_tx,
				plcp[0], plcp[3], txop));
			if (!fbr_iscck)
				txh->u2.MaxAggLen_FBR =
					htol16(wlc_ampdu_calc_maxlen_noaqm(ampdu_tx,
					txh->FragPLCPFallback[0], txh->FragPLCPFallback[3],
					txop));
			else
				txh->u2.MaxAggLen_FBR = 0;

			txh->MaxNMpdus = htol16((max_pdu << 8) | max_pdu);
			/* retry limits - overload MModeFbrLen */
			if (WLPKTTAG(p)->flags & WLF_VRATE_PROBE) {
				txh->MModeFbrLen = htol16((1 << 8) |
					(ampdu_tx_cfg->retry_limit_tid[tid] & 0xff));
				txh->MaxNMpdus = htol16((max_pdu << 8) |
					(ampdu_tx_cfg->probe_mpdu));
			} else {
				txh->MModeFbrLen = htol16(((rr_retry_limit & 0xff) << 8) |
					(ampdu_tx_cfg->retry_limit_tid[tid] & 0xff));
			}

			WL_AMPDU_HW(("wl%d: %s len %d MaxNMpdus 0x%x null delim %d MinMBytes %d "
				     "MaxAggDur %d MaxRNum %d rr_lmnt 0x%x\n", wlc->pub->unit,
				     __FUNCTION__, len, txh->MaxNMpdus, ndelim, txh->MinMBytes,
				     txh->u1.MaxAggDur, txh->u2.s1.MaxRNum, txh->MModeFbrLen));

			/* Ucode agg:
			 * length to be written into side channel includes:
			 * leading delimiter, mac hdr to fcs, padding, null delimiter(s)
			 * i.e. (len + 4 + 4*num_paddelims + padbytes)
			 * padbytes is num required to round to 32 bits
			 */
			if (ndelim) {
				len = ROUNDUP(len, 4);
				len += (ndelim  + 1) * AMPDU_DELIMITER_LEN;
			} else {
				len += AMPDU_DELIMITER_LEN;
			}

			/* flip the epoch? */
			if (hwagg->change_epoch) {
				hwagg->epoch = !hwagg->epoch;
				hwagg->change_epoch = FALSE;
				WL_AMPDU_HWDBG(("sendampdu: fifo %d new epoch: %d frameid %x\n",
					sc, hwagg->epoch, frameid));
				WLCNTINCR(ampdu_tx->cnt->epochdelta);
			}
			aggfifo_enque_noaqm(ampdu_tx, len, sc);
		} else
#endif /* WLAMPDU_UCODE */
		{ /* host agg */
			if (count == 0) {
				uint16 fc;
				mcl |= (TXC_AMPDU_FIRST << TXC_AMPDU_SHIFT);
				/* refill the bits since might be a retx mpdu */
				mcl |= TXC_STARTMSDU;
				rts = (struct dot11_rts_frame*)&txh->rts_frame;
				fc = ltoh16(rts->fc);
				if ((fc & FC_KIND_MASK) == FC_RTS) {
					mcl |= TXC_SENDRTS;
					use_rts = TRUE;
				}
				if ((fc & FC_KIND_MASK) == FC_CTS) {
					mcl |= TXC_SENDCTS;
					use_cts = TRUE;
				}
			} else {
				mcl |= (TXC_AMPDU_MIDDLE << TXC_AMPDU_SHIFT);
				mcl &= ~(TXC_STARTMSDU | TXC_SENDRTS | TXC_SENDCTS);
			}

			len = ROUNDUP(len, 4);
			ampdu_len += (len + (ndelim + 1) * AMPDU_DELIMITER_LEN);
			ASSERT(ampdu_len <= scb_ampdu->max_rxlen);
			dma_len += (uint16)pkttotlen(osh, p);

			WL_AMPDU_TX(("wl%d: wlc_sendampdu: ampdu_len %d seg_cnt %d null delim %d\n",
				wlc->pub->unit, ampdu_len, seg_cnt, ndelim));
		}
		txh->MacTxControlLow = htol16(mcl);

		/* this packet is added */
		pkt[count++] = p;
		SCB_PKTS_INFLT_FIFOCNT_INCR(scb, tid);
#ifdef WL11K
		if (WL11K_ENAB(wlc->pub) && wlc_rrm_stats_enabled(wlc->rrm_info, cfg)) {
			pktlen += pkttotlen(osh, p);
		}
#endif // endif

		/* patch the first MPDU */
		if (AMPDU_HOST_ENAB(wlc->pub) && count == 1) {
			uint8 plcp0, plcp3, is40, sgi;
			uint32 txoplen = 0;

			if (rr) {
				plcp0 = plcp[0];
				plcp3 = plcp[3];
			} else {
				plcp0 = txh->FragPLCPFallback[0];
				plcp3 = txh->FragPLCPFallback[3];

				/* Support multiple fallback rate:
				 * For txtimes > rr_retry_limit, use fallback -> fallback.
				 * To get around the fix-rate case check if primary == fallback rate
				 */
				if (non_aqm->txretry[indx] > rr_retry_limit &&
					ampdu_tx_cfg->mfbr && plcp[0] != plcp0) {
					ratespec_t fbrspec;
					uint8 fbr_mcs;
					uint16 phyctl1;

					use_mfbr = TRUE;
					txh->FragPLCPFallback[3] &=  ~PLCP3_STC_MASK;
					plcp3 = txh->FragPLCPFallback[3];
					fbrspec = wlc_scb_ratesel_getmcsfbr(wlc->wrsi, scb,
						ac, plcp0);
					/* XXX Host aggregation was supported
					 * only on pre AC chips
					 */
					ASSERT(RSPEC_ISHT(fbrspec));
					fbr_mcs = WL_RSPEC_HT_MCS_MASK & fbrspec;

					/* XXX: below is an awkward way to re-construct phyCtrl_1,
					 * and can cause inconsistence if the logic in wlc.c has
					 * changed. Need to find an elegant way ... later.
					 */
					/* restore bandwidth */
					fbrspec &= ~WL_RSPEC_BW_MASK;
					switch (ltoh16(txh->PhyTxControlWord_1_Fbr)
						 & PHY_TXC1_BW_MASK)
					{
						case PHY_TXC1_BW_20MHZ:
						case PHY_TXC1_BW_20MHZ_UP:
							fbrspec |= WL_RSPEC_BW_20MHZ;
						break;
						case PHY_TXC1_BW_40MHZ:
						case PHY_TXC1_BW_40MHZ_DUP:
							fbrspec |= WL_RSPEC_BW_40MHZ;
						break;
						default:
							ASSERT(0);
					}
					if ((RSPEC_IS40MHZ(fbrspec) &&
						(CHSPEC_IS20(wlc->chanspec))) ||
						(RSPEC_ISCCK(fbrspec))) {
						fbrspec &= ~WL_RSPEC_BW_MASK;
						fbrspec |= WL_RSPEC_BW_20MHZ;
					}

					if (IS_SINGLE_STREAM(fbr_mcs)) {
						fbrspec &= ~(WL_RSPEC_TXEXP_MASK | WL_RSPEC_STBC);

						/* For SISO MCS use STBC if possible */
						if (WLC_IS_STBC_TX_FORCED(wlc) ||
							(RSPEC_ISHT(fbrspec) &&
							WLC_STF_SS_STBC_HT_AUTO(wlc, scb))) {
							uint8 stc = 1;

							ASSERT(WLC_STBC_CAP_PHY(wlc));
							fbrspec |= WL_RSPEC_STBC;

							/* also set plcp bits 29:28 */
							plcp3 = (plcp3 & ~PLCP3_STC_MASK) |
							        (stc << PLCP3_STC_SHIFT);
							txh->FragPLCPFallback[3] = plcp3;
							WLCNTINCR(ampdu_tx->cnt->txampdu_mfbr_stbc);
						} else if (wlc->stf->ss_opmode == PHY_TXC1_MODE_CDD)
						{
							fbrspec |= (1 << WL_RSPEC_TXEXP_SHIFT);
						}
					}
					/* rspec returned from getmcsfbr() doesn't have SGI info. */
					if (WLC_HT_GET_SGI_TX(wlc->hti) == ON)
						fbrspec |= WL_RSPEC_SGI;
					phyctl1 = wlc_phytxctl1_calc(wlc, fbrspec, wlc->chanspec);
					txh->PhyTxControlWord_1_Fbr = htol16(phyctl1);

					txh->FragPLCPFallback[0] = fbr_mcs |
						(plcp0 & MIMO_PLCP_40MHZ);
					plcp0 = txh->FragPLCPFallback[0];
				}
			}
			is40 = (plcp0 & MIMO_PLCP_40MHZ) ? 1 : 0;
			sgi = PLCP3_ISSGI(plcp3) ? 1 : 0;
			mcs = plcp0 & ~MIMO_PLCP_40MHZ;
			ASSERT(VALID_MCS(mcs));
			maxlen = MIN(scb_ampdu->max_rxlen,
				ampdu_tx_cfg->max_txlen[MCS2IDX(mcs)][is40][sgi]);

			/* take into account txop and tx burst limit restriction */
			if (txop) {
				/* rate is in Kbps; txop is in usec
				 * ==> len = (rate * txop) / (1000 * 8)
				 */
				txoplen = (MCS_RATE(mcs, is40, sgi) >> 10) * (txop >> 3);
				maxlen = MIN(maxlen, txoplen);
			}

			/* rebuild the rspec and rspec_fallback */
			rspec = HT_RSPEC(plcp[0] & MIMO_PLCP_MCS_MASK);

			if (plcp[0] & MIMO_PLCP_40MHZ)
				rspec |= WL_RSPEC_BW_40MHZ;

			if (fbr_iscck) /* CCK */
				rspec_fallback =
					CCK_RSPEC(CCK_PHY2MAC_RATE(txh->FragPLCPFallback[0]));
			else { /* MIMO */
				rspec_fallback =
				        HT_RSPEC(txh->FragPLCPFallback[0] & MIMO_PLCP_MCS_MASK);
				if (txh->FragPLCPFallback[0] & MIMO_PLCP_40MHZ)
					rspec_fallback |= WL_RSPEC_BW_40MHZ;
			}

			if (use_rts || use_cts) {
				uint16 fc;
				rts_rspec = wlc_rspec_to_rts_rspec(cfg, rspec, FALSE);
				rts_rspec_fallback = wlc_rspec_to_rts_rspec(cfg, rspec_fallback,
				                                            FALSE);
				/* need to reconstruct plcp and phyctl words for rts_fallback */
				if (use_mfbr) {
					int rts_phylen;
					uint8 rts_plcp_fallback[D11_PHY_HDR_LEN], old_ndelim;
					uint16 phyctl1, xfts;

					old_ndelim = txh->RTSPLCPFallback[AMPDU_FBR_NULL_DELIM];
					phyctl1 = wlc_phytxctl1_calc(wlc, rts_rspec_fallback,
						wlc->chanspec);
					txh->PhyTxControlWord_1_FbrRts = htol16(phyctl1);
					if (use_cts)
						rts_phylen = DOT11_CTS_LEN + DOT11_FCS_LEN;
					else
						rts_phylen = DOT11_RTS_LEN + DOT11_FCS_LEN;

					fc = ltoh16(rts->fc);
					wlc_compute_plcp(wlc, cfg, rts_rspec_fallback, rts_phylen,
					fc, rts_plcp_fallback);

					bcopy(rts_plcp_fallback, (char*)&txh->RTSPLCPFallback,
						sizeof(txh->RTSPLCPFallback));

					/* restore it */
					txh->RTSPLCPFallback[AMPDU_FBR_NULL_DELIM] = old_ndelim;

					/* update XtraFrameTypes in case it has changed */
					xfts = ltoh16(txh->XtraFrameTypes);
					xfts &= ~(PHY_TXC_FT_MASK << XFTS_FBRRTS_FT_SHIFT);
					xfts |= ((RSPEC_ISCCK(rts_rspec_fallback) ?
					          FT_CCK : FT_OFDM) << XFTS_FBRRTS_FT_SHIFT);
					txh->XtraFrameTypes = htol16(xfts);
				}
			}
		} /* if (first mpdu for host agg) */

		/* test whether to add more */
		if (AMPDU_HOST_ENAB(wlc->pub)) {
			/* HW PR#37644 WAR : stop aggregating if mpdu
			 * count above a threshold for current rate
			 */
			if ((MCS_RATE(mcs, TRUE, FALSE) >= f->dmaxferrate) &&
			    (count == f->mcs2ampdu_table[MCS2IDX(mcs)])) {
				WL_AMPDU_ERR(("wl%d: PR 37644: stopping ampdu at %d for mcs %d\n",
					wlc->pub->unit, count, mcs));
				break;
			}

			/* limit the size of ampdu for probe packets */
			if (vrate_probe && (count == ampdu_tx_cfg->probe_mpdu))
				break;

			if (count == scb_ampdu->max_pdu)
				break;
		}
#ifdef WLAMPDU_UCODE
		else {
			if (count >= aggfifo_space) {
				uint space;
				space = get_aggfifo_space_noaqm(ampdu_tx, sc);
				if (space > 0)
					aggfifo_space += space;
				else {
					WL_AMPDU_HW(("sendampdu: break due to aggfifo full."
					     "count %d space %d\n", count, aggfifo_space));
					break;
				}
			}
		}
#endif /* WLAMPDU_UCODE */

		/* You can process maximum of AMPDU_MAX_PKTS packets at a time. */
		if (count >= AMPDU_MAX_PKTS)
			break;

		/* check to see if the next pkt is a candidate for aggregation */
		p = pktqprec_peek(WLC_GET_CQ(qi), prec);

		if (ampdu_tx_cfg->hiagg_mode) {
			if (!p && (prec & 1)) {
				prec = prec & ~1;
				p = pktqprec_peek(WLC_GET_CQ(qi), prec);
			}
		}
		if (p) {
			/* See if the next packet is for the same fifo */
			wlc_scb_get_txfifo(wlc, scb, p, &next_fifo);
			if (WLPKTFLAG_AMPDU(WLPKTTAG(p)) && (WLPKTTAGSCBGET(p) == scb) &&
			    ((uint8)PKTPRIO(p) == tid) && (fifo == next_fifo)) {
				nsegs = pktsegcnt(osh, p);
				WL_NONE(("%s: txavail %d  < seg_cnt %d nsegs %d\n", __FUNCTION__,
					TXAVAIL(wlc, fifo), seg_cnt, nsegs));
				/* check if there are enough descriptors available */
				if (TXAVAIL(wlc, fifo) <= (seg_cnt + nsegs)) {
					WLCNTINCR(ampdu_tx->cnt->txfifofull);
					p = NULL;
					continue;
				}

				if (AMPDU_HOST_ENAB(wlc->pub)) {
					plen = pkttotlen(osh, p) + AMPDU_MAX_MPDU_OVERHEAD;
					plen = MAX(scb_ampdu->min_len, plen);
					if ((plen + ampdu_len) > maxlen) {
						p = NULL;
						continue;
					}

					ASSERT((rr && !fbr) || (!rr && fbr));
#ifdef PROP_TXSTATUS
					indx = TX_SEQ_TO_INDEX(WL_SEQ_GET_NUM(WLPKTTAG(p)->seq));
#else
					indx = TX_SEQ_TO_INDEX(WLPKTTAG(p)->seq);
#endif /* PROP_TXSTATUS */
					if (ampdu_tx_cfg->hiagg_mode) {
						if (((non_aqm->txretry[indx] < rr_retry_limit) &&
						     fbr) ||
						    ((non_aqm->txretry[indx] >= rr_retry_limit) &&
						     rr)) {
							p = NULL;
							continue;
						}
					} else {
						if (txretry != non_aqm->txretry[indx]) {
							p = NULL;
							continue;
						}
					}
				}

				p = cpktq_pdeq(&qi->cpktq, prec);
				ASSERT(p);
			} else {
#ifdef WLAMPDU_UCODE
				if (AMPDU_MAC_ENAB(wlc->pub))
					wlc_ampdu_change_epoch(ampdu_tx, sc, AMU_EPOCH_CHG_DSTTID);
#endif // endif
				p = NULL;
			}
		}
	}  /* end while(p) */

	if (count) {

		WL_AMPDU_HWDBG(("%s: tot_mpdu %d txfifo %d\n", __FUNCTION__, count,
			R_REG(wlc->osh, D11_XmtFifoFrameCnt(wlc))));
		WLCNTADD(ampdu_tx->cnt->txmpdu, count);
		AMPDUSCBCNTADD(scb_ampdu->cnt.txmpdu, count);
#ifdef WL11K
		if (pktlen) {
			WLCNTADD(wlc->ampdu_tx->cnt->txampdubyte_l, pktlen);
			if (wlc->ampdu_tx->cnt->txampdubyte_l < pktlen)
				WLCNTINCR(wlc->ampdu_tx->cnt->txampdubyte_h);
		}
#endif // endif
		if (AMPDU_HOST_ENAB(wlc->pub)) {
#ifdef WLCNT
			ampdu_tx->cnt->txampdu++;
#endif // endif
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
			if (ampdu_tx->amdbg)
				ampdu_tx->amdbg->txmcs[MCS2IDX(mcs)]++;
#endif /* defined(BCMDBG) || defined(WLTEST) */
#ifdef WLPKTDLYSTAT
			WLCNTADD(ampdu_tx->cnt->txmpdu_cnt[MCS2IDX(mcs)], count);
#endif /* WLPKTDLYSTAT */

			/* patch up the last txh */
			txh = (d11txh_pre40_t *)PKTDATA(osh, pkt[count - 1]);
			mcl = ltoh16(txh->MacTxControlLow);
			mcl &= ~TXC_AMPDU_MASK;
			mcl |= (TXC_AMPDU_LAST << TXC_AMPDU_SHIFT);
			txh->MacTxControlLow = htol16(mcl);

			/* remove the null delimiter after last mpdu */
			ndelim = txh->RTSPLCPFallback[AMPDU_FBR_NULL_DELIM];
			txh->RTSPLCPFallback[AMPDU_FBR_NULL_DELIM] = 0;
			ampdu_len -= ndelim * AMPDU_DELIMITER_LEN;

			/* remove the pad len from last mpdu */
			fbr_iscck = ((ltoh16(txh->XtraFrameTypes) & 0x3) == 0);
			len = fbr_iscck ? WLC_GET_CCK_PLCP_LEN(txh->FragPLCPFallback)
				: WLC_GET_MIMO_PLCP_LEN(txh->FragPLCPFallback);
			ampdu_len -= ROUNDUP(len, 4) - len;

			/* patch up the first txh & plcp */
			txh = (d11txh_pre40_t *)PKTDATA(osh, pkt[0]);
			plcp = (uint8 *)(txh + 1);

			WLC_SET_MIMO_PLCP_LEN(plcp, ampdu_len);
			/* mark plcp to indicate ampdu */
			WLC_SET_MIMO_PLCP_AMPDU(plcp);

			/* reset the mixed mode header durations */
			if (txh->MModeLen) {
				uint16 mmodelen = wlc_calc_lsig_len(wlc, rspec, ampdu_len);
				txh->MModeLen = htol16(mmodelen);
				preamble_type = WLC_MM_PREAMBLE;
			}
			if (txh->MModeFbrLen) {
				uint16 mmfbrlen = wlc_calc_lsig_len(wlc, rspec_fallback, ampdu_len);
				txh->MModeFbrLen = htol16(mmfbrlen);
				fbr_preamble_type = WLC_MM_PREAMBLE;
			}

			/* set the preload length */
			if (MCS_RATE(mcs, TRUE, FALSE) >= f->dmaxferrate) {
				dma_len = MIN(dma_len, f->ampdu_pld_size);
				if (D11REV_GE(wlc->pub->corerev, 42))
					dma_len >>= 2;
				txh->PreloadSize = htol16(dma_len);
			} else
				txh->PreloadSize = 0;

			mch = ltoh16(txh->MacTxControlHigh);

			/* update RTS dur fields */
			if (use_rts || use_cts) {
				uint16 durid;
				rts = (struct dot11_rts_frame*)&txh->rts_frame;
				if ((mch & TXC_PREAMBLE_RTS_MAIN_SHORT) ==
				    TXC_PREAMBLE_RTS_MAIN_SHORT)
					rts_preamble_type = WLC_SHORT_PREAMBLE;

				if ((mch & TXC_PREAMBLE_RTS_FB_SHORT) == TXC_PREAMBLE_RTS_FB_SHORT)
					rts_fbr_preamble_type = WLC_SHORT_PREAMBLE;

				durid = wlc_compute_rtscts_dur(wlc,
					CHSPEC2WLC_BAND(cfg->current_bss->chanspec), use_cts,
					rts_rspec, rspec, rts_preamble_type, preamble_type,
					ampdu_len, TRUE);
				rts->durid = htol16(durid);
				durid = wlc_compute_rtscts_dur(wlc,
					CHSPEC2WLC_BAND(cfg->current_bss->chanspec), use_cts,
					rts_rspec_fallback, rspec_fallback, rts_fbr_preamble_type,
					fbr_preamble_type, ampdu_len, TRUE);
				txh->RTSDurFallback = htol16(durid);
				/* set TxFesTimeNormal */
				txh->TxFesTimeNormal = rts->durid;
				/* set fallback rate version of TxFesTimeNormal */
				txh->TxFesTimeFallback = txh->RTSDurFallback;
			}

			/* set flag and plcp for fallback rate */
			if (fbr) {
				WLCNTADD(ampdu_tx->cnt->txfbr_mpdu, count);
				WLCNTINCR(ampdu_tx->cnt->txfbr_ampdu);
				mch |= TXC_AMPDU_FBR;
				txh->MacTxControlHigh = htol16(mch);
				WLC_SET_MIMO_PLCP_AMPDU(plcp);
				WLC_SET_MIMO_PLCP_AMPDU(txh->FragPLCPFallback);
				if (PLCP3_ISSGI(txh->FragPLCPFallback[3])) {
					WLCNTINCR(ampdu_tx->cnt->u1.txampdu_sgi);
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
					if (ampdu_tx->amdbg)
						ampdu_tx->amdbg->txmcssgi[MCS2IDX(mcs)]++;
#endif /* defined(BCMDBG) || defined(WLTEST) */
				}
				if (PLCP3_ISSTBC(txh->FragPLCPFallback[3])) {
					WLCNTINCR(ampdu_tx->cnt->u2.txampdu_stbc);
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
					if (ampdu_tx->amdbg)
						ampdu_tx->amdbg->txmcsstbc[MCS2IDX(mcs)]++;
#endif /* defined(BCMDBG) || defined(WLTEST) */
				}
			} else {
				if (PLCP3_ISSGI(plcp[3])) {
					WLCNTINCR(ampdu_tx->cnt->u1.txampdu_sgi);
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
					if (ampdu_tx->amdbg)
						ampdu_tx->amdbg->txmcssgi[MCS2IDX(mcs)]++;
#endif /* defined(BCMDBG) || defined(WLTEST) */
				}
				if (PLCP3_ISSTBC(plcp[3])) {
					WLCNTINCR(ampdu_tx->cnt->u2.txampdu_stbc);
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
					if (ampdu_tx->amdbg)
						ampdu_tx->amdbg->txmcsstbc[MCS2IDX(mcs)]++;
#endif // endif
				}
			}

			if (txretry)
				WLCNTINCR(ampdu_tx->cnt->txretry_ampdu);

			WL_AMPDU_TX(("wl%d: wlc_sendampdu: count %d ampdu_len %d\n",
				wlc->pub->unit, count, ampdu_len));

			/* inform rate_sel if it this is a rate probe pkt */
			frameid = ltoh16(txh->TxFrameID);
			if (D11_TXFID_IS_RATE_PROBE(wlc->pub->corerev, frameid)) {
				wlc_scb_ratesel_probe_ready(wlc->wrsi, scb, frameid, TRUE,
					(uint8)txretry, ac);
			}
		}

		/* pass on all packets to the consumer */
		for (i = 0; i < count; i++) {
			spktenq(output_q, pkt[i]);
		}
		/* If SW Agg, fake a time of txpkt_weight for the ampdu,
		 * otherwise MAC Agg uses just the pkt count
		 */
		*pkt_cnt += ((AMPDU_MAC_ENAB(wlc->pub)) ? count : ampdu_tx_cfg->txpkt_weight);
	} /* endif (count) */

	if (regmpdu) {
		WL_AMPDU_TX(("wl%d: wlc_sendampdu: sending regular mpdu\n", wlc->pub->unit));

		/* This will block the ucode from sending the packet as part of an aggregate */
		WLPKTTAG(p)->flags3 |= WLF3_AMPDU_REGMPDU;

		/* inform rate_sel if it this is a rate probe pkt */
		txh = (d11txh_pre40_t *)PKTDATA(osh, p);
		frameid = ltoh16(txh->TxFrameID);
		if (D11_TXFID_IS_RATE_PROBE(wlc->pub->corerev, frameid)) {
			wlc_scb_ratesel_probe_ready(wlc->wrsi, scb, frameid, FALSE, 0, ac);
		}

		WLCNTINCR(ampdu_tx->cnt->txregmpdu);

		SCB_PKTS_INFLT_FIFOCNT_INCR(scb, tid);

		spktenq(output_q, p);
		/* fake a time of 1 for each pkt */
		*pkt_cnt += 1;

#ifdef WLAMPDU_UCODE
		if (AMPDU_MAC_ENAB(wlc->pub))
			wlc_ampdu_change_epoch(ampdu_tx, sc, AMU_EPOCH_CHG_NAGG);
#endif // endif
	}

	return err;
} /* wlc_sendampdu_noaqm */

#endif /* AMPDU_NON_AQM */

#ifdef IQPLAY_DEBUG
static void
wlc_set_sequmber(wlc_info_t *wlc, uint16 seq_num)
{
	scb_ampdu_tid_ini_t *ini;
	scb_ampdu_tx_t *scb_ampdu;
	int idx;
	wlc_bsscfg_t *cfg;
	struct scb_iter scbiter;
	struct scb *scb;
	ampdu_tx_info_t *ampdu_tx = wlc->ampdu_tx;
	uint8 tid;

	FOREACH_BSS(wlc, idx, cfg) {
		FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {
			if (!SCB_AMPDU(scb))
				continue;
			scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
			ASSERT(scb_ampdu);

			for (tid = 0; tid < AMPDU_MAX_SCB_TID; tid++) {
				ini = scb_ampdu->ini[tid];
				if (!ini)
					continue;
				ini->scb->seqnum[tid] = seq_num;
				wlc_send_addba_req(wlc, ini->scb, tid,
					ampdu_tx->config->ba_tx_wsize, ampdu_tx->config->ba_policy,
					ampdu_tx->config->delba_timeout);
			}
		}
	}
}
#endif /* IQPLAY_DEBUG */

#ifdef WL_CS_PKTRETRY
INLINE static void
wlc_txh_set_chanspec(wlc_info_t* wlc, wlc_txh_info_t* tx_info, chanspec_t new_chanspec)
{
	if (D11REV_GE(wlc->pub->corerev, 128)) {
		tx_info->hdrPtr->rev128.Chanspec = htol16(new_chanspec);
	} else if (D11REV_GE(wlc->pub->corerev, 40)) {
		tx_info->hdrPtr->rev40.PktInfo.Chanspec = htol16(new_chanspec);
	} else {
		d11txh_pre40_t* nonVHTHdr = (d11txh_pre40_t*)(tx_info->hdrPtr);
		uint16 xtra_frame_types = ltoh16(nonVHTHdr->XtraFrameTypes);

		xtra_frame_types &= ~(CHSPEC_CHANNEL(0xFFFF) << XFTS_CHANNEL_SHIFT);
		xtra_frame_types |= CHSPEC_CHANNEL(new_chanspec) << XFTS_CHANNEL_SHIFT;

		nonVHTHdr->XtraFrameTypes = htol16(xtra_frame_types);
	}
}

/** PSPRETEND related feature */
INLINE static void
wlc_ampdu_chanspec_update(wlc_info_t *wlc, void *p)
{
	wlc_txh_info_t txh_info;

	wlc_get_txh_info(wlc, p, &txh_info);

	wlc_txh_set_chanspec(wlc, &txh_info, wlc->chanspec);
}

/** PSPRETEND related feature */
INLINE static bool
wlc_ampdu_cs_retry(wlc_info_t *wlc, tx_status_t *txs, void *p)
{
	const uint supr_status = txs->status.suppr_ind;

	bool pktRetry = TRUE;
#ifdef PROP_TXSTATUS
	/* pktretry will return TRUE on basis of following conditions:
	 * 1: if PROP_TX_STATUS is not enabled OR
	 * 2: if HOST_PROP_TXSTATUS is not enabled OR
	 * 3: if suppresed packet is not from host OR
	 * 4: if suppresed packet is from host AND requested one from Firmware
	 */

	if (!PROP_TXSTATUS_ENAB(wlc->pub) ||
		!HOST_PROPTXSTATUS_ACTIVATED(wlc) ||
		!(WL_TXSTATUS_GET_FLAGS(WLPKTTAG(p)->wl_hdr_information) &
		WLFC_PKTFLAG_PKTFROMHOST) ||
		((WL_TXSTATUS_GET_FLAGS(WLPKTTAG(p)->wl_hdr_information) &
		WLFC_PKTFLAG_PKTFROMHOST) &&
		(WL_TXSTATUS_GET_FLAGS(WLPKTTAG(p)->wl_hdr_information) &
		WLFC_PKTFLAG_PKT_REQUESTED))) {
		pktRetry = !txs->status.was_acked;
	} else {
		pktRetry = FALSE;
	}
#endif /* ifdef PROP_TXSTATUS */

	if (!(wlc->block_datafifo & DATA_BLOCK_QUIET)) {
		/* Not during channel transition time */
		return FALSE;
	}

	switch (supr_status) {
	case TX_STATUS_SUPR_BADCH:
	case TX_STATUS_SUPR_PPS:
		return pktRetry;
	case 0:
		/*
		 * Packet was not acked.
		 * Likely STA already has changed channel.
		 */
		return (pktRetry ? !txs->status.was_acked : pktRetry);
	default:
		return FALSE;
	}
} /* wlc_ampdu_cs_retry */
#endif /* WL_CS_PKTRETRY */

/**
 * Called when a higher layer wants to send an SDU (or suppressed PDU) to the d11 core's FIFOs/DMA.
 * Only called for AC chips when AQM is enabled. Typically, an MSDU is first given by a higher layer
 * to the AMPDU subsystem for aggregation and subsequent queuing in an AMPDU internal software queue
 * (see ampdu_agg()). Next, the AMPDU subsystem decides to 'release' the packet, which means that it
 * forwards the MSDU to wlc.c, which adds it to the wlc transmit queue. Subsequently, wlc.c calls
 * the AMPDU subsytem which calls this function in order to forward the packet towards the d11 core.
 * In the course of this function, the caller provided MSDU is transformed into an MPDU.
 *
 * Please note that this function may decide to send MPDU(s) as 'regular' (= not contained in an
 * AMPDU) MPDUs.
 *
 * Parameters:
 *     output_q : queue to which the caller supplied packet is added
 *
 * @param output_q  Single priority packet queue
 *
 * Returns BCME_BUSY when there is no room to queue the packet.
 */
/* XXX SUNNY_DAY_FUNCTIONALITY:
 * For each MPDU:
 *    - Handle MPDUs that are part of an AMSDU
 *    - Handle 'regular' MPDUs
 *    - Add 802.11 header if not already in place
 *    - Add TSO/ucode header if not present in TXC (tx header cache)
 *    - Detects sequence 'holes'
 *    - Enqueue MPDUs on 'output_q'
 */
static int BCMFASTPATH
wlc_sendampdu_aqm(ampdu_tx_info_t *ampdu_tx, wlc_txq_info_t *qi, void **pdu, int prec,
	struct spktq *output_q, int *pkt_cnt, uint fifo)
{
	wlc_info_t *wlc;
	osl_t* osh;
	void* p;
	struct scb *scb;
	uint8 tid, ac;
	scb_ampdu_tid_ini_t *ini;
	wlc_bsscfg_t *cfg = NULL;
	scb_ampdu_tx_t *scb_ampdu;
	ampdumac_info_t *hwagg;
	wlc_txh_info_t tx_info;
	int err = 0;
	uint npkts,  fifoavail = 0;
	uint next_fifo;
	int count = 0;
	uint16 seq;
	struct dot11_header* h;
	bool fill_txh_info = FALSE;
#ifdef WL11K
	uint32 pktlen = 0;
#endif // endif
	wlc_pkttag_t* pkttag;
	bool is_suppr_pkt = FALSE;
	d11txhdr_t* txh = NULL;
	wlc_key_t *key = NULL;
	wlc_key_info_t key_info;
	uint16 *maclow;
	uint16 *machigh;
#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
	uint16 d11buf_avail;
	uint16 success_cnt;
#endif // endif
	uint corerev;
	uint16 fc_type;

	BCM_REFERENCE(fifoavail);

	wlc = ampdu_tx->wlc;
	corerev = wlc->pub->corerev;
	osh = wlc->osh;
	p = *pdu;

	ASSERT(p != NULL);

	/*
	 * since we obtain <scb, tid, fifo> etc outside the loop,
	 * have to break and return if either of them has changed
	 */
	tid = (uint8)PKTPRIO(p);
	ASSERT(tid < AMPDU_MAX_SCB_TID);
	ac = WME_PRIO2AC(tid);
	ASSERT(ac < AC_COUNT);
	scb = WLPKTTAGSCBGET(p);
	cfg = SCB_BSSCFG(scb);

#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
	/* Keep a count of available d11 buffers */
	success_cnt = 0;
	d11buf_avail = lfbufpool_avail(D11_LFRAG_BUF_POOL);
	if (PKTISTXFRAG(osh, p) && !d11buf_avail) {
		return BCME_BUSY;
	}
#endif // endif

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	ini = scb_ampdu->ini[tid];

	/* loop over MPDUs. loop exits via break. */
	while (TRUE) {
		bool regmpdu_pkt = FALSE;
		bool reque_pkt = FALSE;
		bool has_seq_hole = FALSE;
		bool s_mpdu_pdu = FALSE;

		pkttag = WLPKTTAG(p);

#ifdef BCMDBG_SWWLAN_38455
		if ((pkttag->flags & WLF_AMPDU_MPDU) == 0) {
			WL_PRINT(("%s flags=%x seq=%d fifo=%d\n", __FUNCTION__,
				pkttag->flags, pkttag->seq, fifo));
		}
#endif /* BCMDBG_SWWLAN_38455 */

		ASSERT(pkttag->flags & WLF_AMPDU_MPDU);

		seq = pkttag->seq;
#ifdef PROP_TXSTATUS
		seq = WL_SEQ_GET_NUM(seq);

#endif /* PROP_TXSTATUS */

		if (!wlc_scb_restrict_can_txq(wlc, scb)) {
			err = BCME_BUSY;
		}
		/* N.B.: WLF_TXHDR flag indicates the MPDU is being retransmitted
		 * due to d11 suppression
		 */
		else if (pkttag->flags & WLF_TXHDR) {
			err = wlc_prep_pdu(wlc, scb, p, fifo);
			if (!err) {
				reque_pkt = TRUE;
				wlc_get_txh_info(wlc, p, &tx_info);
				if (!wlc_txh_get_isAMPDU(wlc, &tx_info))
					s_mpdu_pdu = TRUE;
			}
		} else if ((err = wlc_prep_sdu_fast(wlc, cfg, scb, p, fifo,
			&key, &key_info)) == BCME_UNSUPPORTED) {
			/* prep_sdu converts an SDU into MPDU (802.11) format */
			err = wlc_prep_sdu(wlc, scb, &p, (int*)&npkts, fifo);
			pkttag = WLPKTTAG(p);
		}

		/* Update the ampdumac pointer to point correct fifo */
		hwagg = &ampdu_tx->hwagg[fifo];

		if (err) {
			if (err == BCME_BUSY) {
				WL_AMPDU_TX(("wl%d: %s prep_xdu retry; seq 0x%x\n",
					wlc->pub->unit, __FUNCTION__, seq));
				/* XXX 4360: need a way to differentiate
				 * between txmaxpkts and txavail fail
				 */
				WLCNTINCR(ampdu_tx->cnt->txfifofull);
				WLCNTINCR(ampdu_tx->cnt->sduretry);
				*pdu = p;
				break;
			}

			/* error in the packet; reject it */
			WL_AMPDU_ERR(("wl%d: wlc_sendampdu: prep_xdu rejected; seq 0x%x\n",
				wlc->pub->unit, seq));
			WLCNTINCR(ampdu_tx->cnt->sdurejected);
			AMPDUSCBCNTINCR(scb_ampdu->cnt.sdurejected);

			/* let ampdu_is_exp_seq_aqm() catch it, update tx_exp_seq, and send_bar */
			*pdu = NULL;
			break;
		}
		WL_AMPDU_TX(("wl%d: %s: prep_xdu success; seq 0x%x tid %d\n",
			wlc->pub->unit, __FUNCTION__, seq, tid));

		regmpdu_pkt = !(pkttag->flags & WLF_MIMO) ||
		        (ini->ba_state == AMPDU_TID_STATE_BA_PENDING_OFF) ||
		        (ini->ba_state == AMPDU_TID_STATE_BA_OFF) ||
		        SCB_PS(scb) || s_mpdu_pdu;

		/* if no error, populate tx hdr (TSO/ucode header) info and continue */
		ASSERT(pkttag->flags & WLF_TXHDR);
		if (!fill_txh_info) {
			fill_txh_info = (count == 0 ||
			                 reque_pkt ||
			                 regmpdu_pkt ||
			                 (pkttag->flags & WLF_TXCMISS) != 0);
		}
		if (fill_txh_info) {
			wlc_get_txh_info(wlc, p, &tx_info);

			txh = tx_info.hdrPtr;
		} else {
			int tsoHdrSize = wlc_pkt_get_txh_hdr(wlc, p, &txh);

#ifdef WLTOEHW
			tx_info.tsoHdrSize = tsoHdrSize;
			tx_info.tsoHdrPtr = (void*)((tsoHdrSize != 0) ?
				PKTDATA(wlc->osh, p) : NULL);
#endif /* WLTOEHW */

			tx_info.hdrPtr = (d11txhdr_t *)(PKTDATA(wlc->osh, p) + tsoHdrSize);

			if (D11REV_LT(corerev, 128)) {
				d11actxh_rate_t * rateInfo;
				/**
				* XXX 4360: items past PktInfo only available
				* when using long txd header.
				*
				* Need to get from txc cache if short txd
				*/
				tx_info.TxFrameID = txh->rev40.PktInfo.TxFrameID;
				tx_info.MacTxControlLow = txh->rev40.PktInfo.MacTxControlLow;
				tx_info.MacTxControlHigh = txh->rev40.PktInfo.MacTxControlHigh;
				rateInfo = WLC_TXD_RATE_INFO_GET(&(txh->rev40), corerev);
				tx_info.hdrSize = D11AC_TXH_LEN;
				tx_info.d11HdrPtr = ((uint8 *)tx_info.hdrPtr) + tx_info.hdrSize;
				tx_info.plcpPtr = (rateInfo[0].plcp);
				tx_info.TxFrameRA = (uint8*)(((struct dot11_header *)
					(tx_info.d11HdrPtr))->a1.octet);
			} else { /* GE 128 */
				tx_info.TxFrameID = txh->rev128.FrameID;
				tx_info.MacTxControlLow = txh->rev128.MacControl_0;
				tx_info.MacTxControlHigh = txh->rev128.MacControl_1;

				tx_info.hdrSize = D11_REV128_TXH_LEN;
				tx_info.d11HdrPtr = ((uint8 *)tx_info.hdrPtr) + tx_info.hdrSize;
				tx_info.plcpPtr = NULL; /* not used */
				tx_info.TxFrameRA = (uint8*)(((struct dot11_header *)
					(tx_info.d11HdrPtr))->a1.octet);
			}
		}

		h = (struct dot11_header*)(tx_info.d11HdrPtr);
		seq = ltoh16(h->seq) >> SEQNUM_SHIFT;

#ifdef PROP_TXSTATUS
		if (GET_DRV_HAS_ASSIGNED_SEQ(pkttag->seq)) {
			is_suppr_pkt = TRUE;
			/* Now the dongle owns the packet */
			RESET_DRV_HAS_ASSIGNED_SEQ(pkttag->seq);
		} else {
			is_suppr_pkt = FALSE;
		}

#endif /* PROP_TXSTATUS */

		/* during roam we get pkts with null a1. kill it here else
		 * does not find scb on dotxstatus and things gets out of sync
		 */
		if (ETHER_ISNULLADDR(&h->a1)) {
			WL_AMPDU_ERR(("wl%d: %s: dropping frame with null a1\n",
				wlc->pub->unit, __FUNCTION__));
			err = BCME_ERROR;
			wlc_tx_status_update_counters(wlc, p,
				scb, NULL, WLC_TX_STS_TOSS_BAD_ADDR, 1);

		} else if (pkttag->flags & WLF_FIFOPKT) {
#ifdef WL_CS_PKTRETRY
			wlc_ampdu_chanspec_update(wlc, p);
#endif // endif
			/* seq # in suppressed frames have been validated before
			 * so don't validate them again...unless there are queuing
			 * issue(s) causing them to be out-of-order.
			 */
#ifdef PROP_TXSTATUS
#endif /* PROP_TXSTATUS */
		} else if (!ampdu_is_exp_seq_aqm(ampdu_tx, ini, seq, is_suppr_pkt)) {
			/* If the packet was not retrieved from HW FIFO before
			 * attempting over the air for multi-queue
			 */
			WL_ERROR(("%s: p %p seq %x pktflags %x\n", __FUNCTION__,
				p, WLPKTTAG(p)->seq, WLPKTTAG(p)->flags));
			WL_AMPDU_TX(("%s: multi-queue error\n", __FUNCTION__));
			err = BCME_ERROR;

			wlc_tx_status_update_counters(wlc, p,
				scb, NULL, WLC_TX_STS_TOSS_INV_SEQ, 1);
		}

		if (err == BCME_ERROR) {
			WLCNTINCR(ampdu_tx->cnt->txdrop);
#ifdef WLTAF
			if (!wlc_taf_txpkt_status(ampdu_tx->wlc->taf_handle, scb, ini->tid, p,
				TAF_TXPKT_STATUS_PKTFREE_RESET))
#endif // endif
			{
				wlc_ampdu_dec_bytes_inflight(ini, p);
			}
			PKTFREE(osh, p, TRUE);
			*pdu = NULL;
			break;
		}

		/* Add frame type to TSO header for (D11 core rev >= 80) AQM design */
		fc_type = FC_TYPE(ltoh16(h->fc));
		if (D11REV_GE(corerev, 128)) {
			wlc_txh_set_ft(wlc, tx_info.tsoHdrPtr, fc_type);
			wlc_txh_set_frag_allow(wlc, tx_info.tsoHdrPtr, (fc_type == FC_TYPE_DATA));
		}

		/* 'hole' if sequence does not succeed pre-pending MPDU */
		has_seq_hole = ampdu_detect_seq_hole(ampdu_tx, seq, ini);

		maclow = D11_TXH_GET_MACLOW_PTR(wlc, txh);
		machigh = D11_TXH_GET_MACHIGH_PTR(wlc, txh);
		if (regmpdu_pkt) {
			WL_AMPDU_TX(("%s: reg mpdu found\n", __FUNCTION__));

			/* clear ampdu bit, set svht bit */
			tx_info.MacTxControlLow &= htol16(~D11AC_TXC_AMPDU);
			/* non-ampdu must have svht bit set */
			tx_info.MacTxControlHigh |= htol16(D11AC_TXC_SVHT);

			*maclow = tx_info.MacTxControlLow;
			*machigh = tx_info.MacTxControlHigh;

			if (hwagg->prev_ft != AMPDU_NONE) {
				/* ampdu switch to regmpdu */
				wlc_ampdu_chgnsav_epoch(ampdu_tx, fifo,
					AMU_EPOCH_CHG_NAGG, scb, tid, FALSE, &tx_info);
				wlc_txh_set_epoch(ampdu_tx->wlc, tx_info.tsoHdrPtr, hwagg->epoch);
				hwagg->prev_ft = AMPDU_NONE;
			}
			/* if prev is regmpdu already, don't bother to set epoch at all */

			/* mark the MPDU is regmpdu */
			pkttag->flags3 |= WLF3_AMPDU_REGMPDU;
		} else {
			bool change = FALSE;
			bool htc = FALSE;
			int8 reason_code = 0;

			/* clear svht bit, set ampdu bit */
			tx_info.MacTxControlLow |= htol16(D11AC_TXC_AMPDU);
			/* for real ampdu, clear vht single mpdu ampdu bit */
			tx_info.MacTxControlHigh &= htol16(~D11AC_TXC_SVHT);

			*maclow = tx_info.MacTxControlLow;
			*machigh = tx_info.MacTxControlHigh;

			/* clear regmpdu bit */
			pkttag->flags3 &= ~WLF3_AMPDU_REGMPDU;

			if (fill_txh_info) {
				uint16 frameid;
				bool rate_probe = FALSE;
				int8* tsoHdrPtr = tx_info.tsoHdrPtr;

				htc = HE_FC_IS_HTC(ltoh16(h->fc));
				change = TRUE;
				frameid = ltoh16(tx_info.TxFrameID);
				rate_probe = D11_TXFID_IS_RATE_PROBE(corerev, frameid);
				/* For rev128, rate probe always return 0 */
				if (rate_probe) {
					/* check vrate probe flag
					 * this flag is only get the frame to become first mpdu
					 * of ampdu and no need to save epoch_params
					 */
					wlc_scb_ratesel_probe_ready(wlc->wrsi, scb, frameid,
					                            FALSE, 0, ac);
					reason_code = AMU_EPOCH_CHG_FID;
				} else if (scb != hwagg->prev_scb || tid != hwagg->prev_tid) {
					reason_code = AMU_EPOCH_CHG_DSTTID;
				} else if (!RATELINKMEM_ENAB(wlc->pub) &&
					wlc_ampdu_epoch_params_chg(wlc, hwagg, txh)) {
					/* rate/sgi/bw/stbc/antsel tx params */
					reason_code = AMU_EPOCH_CHG_PLCP;
				} else if (ltoh16(tx_info.MacTxControlLow) & D11AC_TXC_UPD_CACHE) {
					reason_code = AMU_EPOCH_CHG_TXC_UPD;
				} else if ((tsoHdrPtr != NULL) &&
				           ((tsoHdrPtr[2] & TOE_F2_TXD_HEAD_SHORT) !=
				            hwagg->prev_shdr)) {
					reason_code = AMU_EPOCH_CHG_TXHDR;
				} else if (htc || hwagg->prev_htc) {
					reason_code = AMU_EPOCH_CHG_HTC;
				} else {
					change = FALSE;
				}
				fill_txh_info = FALSE;
			}

			if (!change) {
				if (hwagg->prev_ft == AMPDU_NONE) {
					/* switching from non-aggregate to aggregate ampdu,
					 * just update the epoch changing parameters(hwagg) but
					 * not the epoch.
					 */
					change = TRUE;
					reason_code = AMU_EPOCH_NO_CHANGE;
				} else if (has_seq_hole) {
					/* Flip the epoch if there is a hole in sequence of pkts
					 * sent to txfifo. It is a AQM HW requirement
					 * not to have the holes in txfifo.
					 */
					change = TRUE;
					reason_code = AMU_EPOCH_CHG_SEQ;
				}
#ifdef WLTAF
				else if (pkttag->flags3 & WLF3_TAF_STAR_MARKED) {
					change = TRUE;
					reason_code = AMU_EPOCH_CHG_TAF_STAR;
				}
#endif /* WLTAF */
			}

			if (change) {
				if (ltoh16(tx_info.MacTxControlLow) & D11AC_TXC_UPD_CACHE) {
					fill_txh_info = TRUE;
				}
				wlc_ampdu_chgnsav_epoch(ampdu_tx, fifo, reason_code,
					scb, tid, htc, &tx_info);
			}

			/* always set epoch for ampdu */
			wlc_txh_set_epoch(ampdu_tx->wlc, tx_info.tsoHdrPtr, hwagg->epoch);
		}

		/*
		 * XXX 4360 : pending change to: not commit in while loop
		 * and commit with null p after loop
		 */
		spktenq(output_q, p);
		(*pkt_cnt)++;
		count++;
#ifdef WL11K
		if (WL11K_ENAB(wlc->pub) && wlc_rrm_stats_enabled(wlc->rrm_info, cfg)) {
			pktlen += pkttotlen(osh, p);
		}
#endif // endif

#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
		if (++success_cnt >= d11buf_avail) {
			/* DHDHDR feature requires a d11buffer to be available
			 * for filling tx headers.
			 */
			break;
		}
#endif /* BCM_DHDHDR && DONGLEBUILD */

		/* check to see if the next pkt is a candidate for aggregation */
		p = pktqprec_peek(WLC_GET_CQ(qi), prec);

		if (p != NULL) {
			/* XXX 4360: as long as we don't switch fifo, *can* continue
			 * the fifo limit kicks in because of txavail check.
			 * but should we hold to accumulate to high water mark?
			 */
#ifdef BCMPCIEDEV_ENABLED
			if (!wlc->cmn->hostmem_access_enabled &&
				BCMLFRAG_ENAB() && PKTISTXFRAG(osh, p)) {
				/* Do not process the Host originated LFRAG tx since no host
				* memory access is allowed in PCIE D3 suspend
				*/
				break;
			}
#endif /* BCMPCIEDEV_ENABLED */
			/* See if the next packet is for the same fifo */
			wlc_scb_get_txfifo(wlc, scb, p, &next_fifo);
			if (WLPKTFLAG_AMPDU(WLPKTTAG(p)) && (WLPKTTAGSCBGET(p) == scb) &&
			    ((uint8)PKTPRIO(p) == tid) && (fifo == next_fifo)) {
				p = cpktq_pdeq(&qi->cpktq, prec);
				ASSERT(p);
			} else {
				break;
			}
		} else {
			break;
		}
	} /* end while(TRUE): loop over MPDUs */

	if (count) {
		SCB_PKTS_INFLT_FIFOCNT_ADD(scb, tid, count);
#ifdef WLCNT
		ASSERT(ampdu_tx->cnt);
#endif // endif
		AMPDUSCBCNTADD(scb_ampdu->cnt.txmpdu, count);
#ifdef WL11K
		if (pktlen) {
			WLCNTADD(wlc->ampdu_tx->cnt->txampdubyte_l, pktlen);
			if (wlc->ampdu_tx->cnt->txampdubyte_l < pktlen)
				WLCNTINCR(wlc->ampdu_tx->cnt->txampdubyte_h);
		}
#endif // endif
	}

	WL_AMPDU_TX(("%s: fifo %d count %d epoch %d txpktpend %d",
		__FUNCTION__, fifo, count, ampdu_tx->hwagg[fifo].epoch,
		TXPKTPENDGET(wlc, fifo)));
	if (WL_AMPDU_HW_ON()) {
		if (D11REV_GE(wlc->pub->corerev, 128)) {
			WL_AMPDU_TX((" aqmqmap 0x%x aqmfifo_status 0x%x\n",
				R_REG(osh, D11_AQM_QMAP(wlc)),
				R_REG(osh, D11_MQF_STATUS(wlc))));
		} else if (D11REV_GE(wlc->pub->corerev, 65)) {
			WL_AMPDU_TX((" psm_reg_mux 0x%x aqmqmap 0x%x aqmfifo_status 0x%x\n",
				R_REG(osh, D11_PSM_REG_MUX(wlc)),
				R_REG(osh, D11_AQMQMAP(wlc)),
				R_REG(osh, D11_AQMFifo_Status(wlc))));
		} else {
			WL_AMPDU_TX((" fifocnt 0x%x\n",
				R_REG(wlc->osh, D11_XmtFifoFrameCnt(wlc))));
		}
	}

	return err;
} /* wlc_sendampdu_aqm */

/**
 * Requests the 'communication partner' to send a block ack, so that on reception the transmit
 * window can be advanced. This function will check if the intended bar_seq meets the
 * prerequisites and will update the barpending_seq if that is the case. The actual transmit of
 * the block ack request will be postponed until there are no more packets in transit.
 */
static void
wlc_ampdu_send_bar(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tid_ini_t *ini, uint16 bar_seq)
{
	int indx, i;
	scb_ampdu_tx_t *scb_ampdu;
	void *p = NULL;
	ampdu_tx_config_t *ampdu_tx_cfg = ampdu_tx->config;

	if (ini->ba_state != AMPDU_TID_STATE_BA_ON)
		return;

	if (ini->bar_cnt >= AMPDU_BAR_RETRY_CNT) {
		/* Postpone cleanup of tid and defer to ampdu_watchdog, else every call to
		 * this function needs to be protected against ini == NULL.
		 */
		return;
	}

	if (ini->bar_cnt == AMPDU_BAR_RETRY_CNT / 2) {
		ini->bar_cnt++;
		/* retry sending bar from ampdu watchdog */
		ini->retry_bar = AMPDU_R_BAR_HALF_RETRY_CNT;
		return;
	}

	if (AMPDU_AQM_ENAB(ampdu_tx->wlc->pub)) {
		ASSERT(bar_seq < SEQNUM_MAX);

		/* Check if bar_seq falls within current window of released sequence numbers */
		if (IS_SEQ_ADVANCED(MODSUB_POW2(ini->start_seq, 1, SEQNUM_MAX), bar_seq) ||
			IS_SEQ_ADVANCED(bar_seq, MODADD_POW2(ini->max_seq, 2, SEQNUM_MAX))) {
			WL_AMPDU_CTL(("%s bar_seq %x outside released: acked_seq %x, max_seq %x\n",
				__FUNCTION__, bar_seq, ini->acked_seq, ini->max_seq));
			goto abort_send_bar;
		}

		/* If bar_seq is more than ba_wsize away from last released PDU with max_seq,
		 * newer ampdu packets are already waiting to be transmitted.
		 * These packets should advance receiver window, so no need to transmit bar frame.
		 */
		if (MODSUB_POW2(ini->max_seq+1, bar_seq, SEQNUM_MAX) >= ini->ba_wsize) {
			WL_AMPDU_CTL(("wl%d: %s: skipping BAR %x, max_seq %x, acked_seq %x\n",
				ampdu_tx->wlc->pub->unit, __FUNCTION__, bar_seq,
				ini->max_seq, ini->acked_seq));
			goto abort_send_bar;
		} else if (ini->bar_ackpending && !IS_SEQ_ADVANCED(bar_seq, ini->barpending_seq)) {
			/*
			 * A bar with newer seq is pending to be sent out due to other condition,
			 * like unexpected tx seq
			 */
			return;
		}

		/* If new bar_seq is provided, than this is no longer considered a retry. */
		if (bar_seq != ini->barpending_seq) {
			ini->retry_bar = AMPDU_R_BAR_DISABLED;
		}

		/* Already update the barpending_seq, so that when callback wlc_send_bar_complete
		 * happens, a new bar frame will be sent.
		 */
		ini->barpending_seq = bar_seq;
	} else { /* !AQM */
		if (bar_seq == SEQNUM_CHK) {
			int offset = -1;
			scb_ampdu_tid_ini_non_aqm_t *non_aqm = ini->non_aqm;
			indx = TX_SEQ_TO_INDEX(ini->start_seq);

			for (i = 0; i < (ini->ba_wsize - non_aqm->rem_window); i++) {
				if (isset(non_aqm->barpending, indx))
					offset = i;
				indx = NEXT_TX_INDEX(indx);
			}
			if (offset == -1) {
				ini->bar_cnt = 0;
				return;
			}
			bar_seq = MODADD_POW2(ini->start_seq, offset + 1, SEQNUM_MAX);
		} else {
			bar_seq = ini->start_seq;
		}
	}

	/* Check if there is already a frame in transit.
	 */
	if (ini->bar_ackpending ||
		(ini->tx_in_transit && (ini->retry_bar == AMPDU_R_BAR_DISABLED))) {
		return;
	}

	if (ini->retry_bar != AMPDU_R_BAR_DISABLED) {
		WL_AMPDU_CTL(("retry bar for reason %x, bar_seq 0x%04x, max_seq 0x%04x,"
			"acked_seq 0x%04x, bar_cnt %d, tx_in_transit %d\n",
			ini->retry_bar, bar_seq, ini->max_seq, ini->acked_seq,
			ini->bar_cnt, ini->tx_in_transit));
	}
	ini->retry_bar = AMPDU_R_BAR_DISABLED;

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, ini->scb);
	ASSERT(scb_ampdu);
	BCM_REFERENCE(scb_ampdu);

	if (ampdu_tx_cfg->no_bar == FALSE) {
		bool blocked;

		p = wlc_send_bar(ampdu_tx->wlc, ini->scb, ini->tid, bar_seq,
		                 DOT11_BA_CTL_POLICY_NORMAL, FALSE, &blocked);
		if (blocked) {
			/* retry sending bar from ampdu watchdog */
			ini->retry_bar = AMPDU_R_BAR_BLOCKED;
			return;
		}

		if (p == NULL) {
			/* retry sending bar from ampdu watchdog */
			ini->retry_bar = AMPDU_R_BAR_NO_BUFFER;
			return;
		}

		/* Cancel any pending wlc_send_bar_complete packet callbacks. */
		wlc_pcb_fn_find(ampdu_tx->wlc->pcb, wlc_send_bar_complete, ini, TRUE);

		/* pcb = packet tx complete callback management */
		if (wlc_pcb_fn_register(ampdu_tx->wlc->pcb, wlc_send_bar_complete, ini, p)) {
			/* retry sending bar from ampdu watchdog */
			ini->retry_bar = AMPDU_R_BAR_CALLBACK_FAIL;
			return;
		}

		WLCNTINCR(ampdu_tx->cnt->txbar);
		AMPDUSCBCNTINCR(scb_ampdu->cnt.txbar);
		WLCNTINCR(ampdu_tx->wlc->pub->_cnt->txbar);
	}

	ini->bar_cnt++;
	ini->bar_ackpending = TRUE;
	ini->bar_ackpending_seq = bar_seq;

	if (ampdu_tx_cfg->no_bar == TRUE) {
		/* Cancel any pending wlc_send_bar_complete packet callbacks. */
		wlc_pcb_fn_find(ampdu_tx->wlc->pcb, wlc_send_bar_complete, ini, TRUE);
		wlc_send_bar_complete(ampdu_tx->wlc, TX_STATUS_ACK_RCV, ini);
	}
	return;

abort_send_bar:
	if (bar_seq == ini->barpending_seq) {
		ini->barpending_seq = SEQNUM_MAX;
		/* stop retry BAR if subsequent tx has made the BAR seq stale */
		ini->retry_bar = AMPDU_R_BAR_DISABLED;
	}
} /* wlc_ampdu_send_bar */

static INLINE void
wlc_ampdu_ini_move_window_aqm(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tx_t *scb_ampdu,
	scb_ampdu_tid_ini_t *ini)
{
	ASSERT(ini);
	ASSERT(ampdu_tx);
	ASSERT(ampdu_tx->wlc);
	ASSERT(ampdu_tx->wlc->pub);

	/* If window moves beyond intended bar_seq, reset it */
	if ((ini->barpending_seq != SEQNUM_MAX) &&
		IS_SEQ_ADVANCED(ini->acked_seq, ini->barpending_seq)) {
		ini->barpending_seq = SEQNUM_MAX;
		/* stop retry BAR if subsequent tx has made the BAR seq stale */
		ini->retry_bar = AMPDU_R_BAR_DISABLED;
	}

	if ((ini->acked_seq == ini->start_seq) ||
	    IS_SEQ_ADVANCED(ini->acked_seq, ini->start_seq)) {
		ini->start_seq = MODINC_POW2(ini->acked_seq, SEQNUM_MAX);
		ini->alive = TRUE;
	}
	/* if possible, release some buffered pdu's */
	wlc_ampdu_txeval(ampdu_tx, scb_ampdu, ini, FALSE);

	return;
}

/**
 * Called upon tx packet completion indication of d11 core. Not called for AC chips with AQM
 * enabled.
 */
static INLINE void
wlc_ampdu_ini_move_window_noaqm(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tx_t *scb_ampdu,
	scb_ampdu_tid_ini_t *ini)
{
	uint16 indx, i, range;
	scb_ampdu_tid_ini_non_aqm_t *non_aqm = ini->non_aqm;

	ASSERT(ini);
	ASSERT(ampdu_tx);
	ASSERT(ampdu_tx->wlc);
	ASSERT(ampdu_tx->wlc->pub);

	/* ba_wsize can be zero in AMPDU_TID_STATE_BA_OFF or PENDING_ON state */
	if (ini->ba_wsize == 0)
		return;

	range = MODSUB_POW2(MODADD_POW2(ini->max_seq, 1, SEQNUM_MAX), ini->start_seq, SEQNUM_MAX);
	for (i = 0, indx = TX_SEQ_TO_INDEX(ini->start_seq);
		(i < range) && (!isset(non_aqm->ackpending, indx)) &&
		(!isset(non_aqm->barpending, indx));
		i++, indx = NEXT_TX_INDEX(indx));

	ini->start_seq = MODADD_POW2(ini->start_seq, i, SEQNUM_MAX);
	/* mark alive only if window moves forward */
	if (i > 0) {
		non_aqm->rem_window += i;
		ini->alive = TRUE;
	}

	/* if possible, release some buffered pdus */
	wlc_ampdu_txeval(ampdu_tx, scb_ampdu, ini, FALSE);

	WL_AMPDU_TX(("wl%d: wlc_ampdu_ini_move_window: tid %d start_seq bumped to 0x%x\n",
		ampdu_tx->wlc->pub->unit, ini->tid, ini->start_seq));

	if (non_aqm->rem_window > ini->ba_wsize) {
		WL_AMPDU_ERR(("wl%d: %s: rem_window %d, ba_wsize %d "
			"i %d range %d sseq 0x%x\n",
			ampdu_tx->wlc->pub->unit, __FUNCTION__, non_aqm->rem_window, ini->ba_wsize,
			i, range, ini->start_seq));
		ASSERT(non_aqm->rem_window <= ini->ba_wsize);
	}
} /* wlc_ampdu_ini_move_window_noaqm */

/**
 * Called upon tx packet completion indication of d11 core.
 * Bumps up the start seq to move the window.
 */
static void
wlc_ampdu_ini_move_window(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tx_t *scb_ampdu,
	scb_ampdu_tid_ini_t *ini)
{
	if (AMPDU_AQM_ENAB(ampdu_tx->wlc->pub))
		wlc_ampdu_ini_move_window_aqm(ampdu_tx, scb_ampdu, ini);
	else
		wlc_ampdu_ini_move_window_noaqm(ampdu_tx, scb_ampdu, ini);
}

/**
 * Called when the D11 core indicates that transmission of a 'block ack request' packet completed.
 */
static void
wlc_send_bar_complete(wlc_info_t *wlc, uint txstatus, void *arg)
{
	scb_ampdu_tid_ini_t *ini = (scb_ampdu_tid_ini_t *)arg;
	ampdu_tx_info_t *ampdu_tx = wlc->ampdu_tx;
	uint16 indx, s_seq;
	bool aqm_no_bitmap = AMPDU_AQM_ENAB(ampdu_tx->wlc->pub);

	WL_AMPDU_CTL(("wl%d: wlc_send_bar_complete for tid %d status 0x%x,"
		" start 0x%x, bar_ack 0x%x, bar 0x%x, max 0x%x\n",
		wlc->pub->unit, ini->tid, txstatus,
		ini->start_seq, ini->bar_ackpending_seq, ini->barpending_seq, ini->max_seq));

	/* There's a corner case that if pervious BAR sending failed,
	 *  this function might be invoked by PKTFREE during wl_down.
	 */
	if (!ampdu_tx->wlc->pub->up) {
		WL_INFORM(("%s:interface is not up,do nothing.", __FUNCTION__));
		return;
	}

	/* The BAR packet hasn't been attempted by the ucode tx function
	 * Retry send BAR to confirm that receipient move BA window
	 * if not bar_ackpending will never be recovered.
	 * Do not put assertion here since tx suppression case need to be consider
	 */
	if (((txstatus & TX_STATUS_FRM_RTX_MASK) >> TX_STATUS_FRM_RTX_SHIFT) == 0) {
		WL_AMPDU_ERR(("wl%d: %s: no tx attempted, txstatus: 0x%x\n",
			ampdu_tx->wlc->pub->unit, __FUNCTION__, txstatus));
	}

	ASSERT(ini->bar_ackpending);
	ini->bar_ackpending = FALSE;

	/* ack received */
	if (txstatus & TX_STATUS_ACK_RCV) {
		if (!aqm_no_bitmap) {
			scb_ampdu_tid_ini_non_aqm_t *non_aqm = ini->non_aqm;
			for (s_seq = ini->start_seq;
			     s_seq != ini->bar_ackpending_seq;
			     s_seq = NEXT_SEQ(s_seq)) {
				indx = TX_SEQ_TO_INDEX(s_seq);
				if (isset(non_aqm->barpending, indx)) {
					clrbit(non_aqm->barpending, indx);
					if (isset(non_aqm->ackpending, indx)) {
						clrbit(non_aqm->ackpending, indx);
						non_aqm->txretry[indx] = 0;
					}
				}
			}
			/* bump up the start seq to move the window */
			wlc_ampdu_ini_move_window(ampdu_tx,
				SCB_AMPDU_TX_CUBBY(ampdu_tx, ini->scb), ini);
		} else {
			if (IS_SEQ_ADVANCED(ini->bar_ackpending_seq, ini->start_seq) &&
				((IS_SEQ_ADVANCED(ini->max_seq, ini->bar_ackpending_seq) ||
				MODINC_POW2(ini->max_seq, SEQNUM_MAX) == ini->bar_ackpending_seq)))
			{
				ini->acked_seq = MODSUB_POW2(ini->bar_ackpending_seq,
					1, SEQNUM_MAX);
				wlc_ampdu_ini_move_window(ampdu_tx,
					SCB_AMPDU_TX_CUBBY(ampdu_tx, ini->scb), ini);
			}

			if (ini->barpending_seq == ini->bar_ackpending_seq)
				ini->barpending_seq = SEQNUM_MAX;
			ini->bar_cnt--;
		}
	}

	wlc_ampdu_txflowcontrol(wlc, SCB_AMPDU_TX_CUBBY(ampdu_tx, ini->scb), ini);
	if (aqm_no_bitmap) {
		/* not acked or need to send  bar for another seq no.  */
		if (ini->barpending_seq != SEQNUM_MAX) {
			wlc_ampdu_send_bar(wlc->ampdu_tx, ini, ini->barpending_seq);
		}
		/* barpending_seq may be changed by call to wlc_ampdu_send_bar */
		if (ini->barpending_seq == SEQNUM_MAX) {
			ini->bar_cnt = 0;
			ini->retry_bar = AMPDU_R_BAR_DISABLED;
		}
	} else {
		wlc_ampdu_send_bar(wlc->ampdu_tx, ini, SEQNUM_CHK);
	}
} /* wlc_send_bar_complete */

/**
 * Higher layer invokes this function as a result of the d11 core indicating tx completion. Function
 * is only active for AC chips that have AQM enabled. 'Regular' tx MPDU is a non-AMPDU contained
 * MPDU.
 */
static void
wlc_ampdu_dotxstatus_regmpdu_aqm(ampdu_tx_info_t *ampdu_tx, struct scb *scb,
	void *p, tx_status_t *txs, wlc_txh_info_t *txh_info)
{
	scb_ampdu_tx_t *scb_ampdu;
	scb_ampdu_tid_ini_t *ini;
	wlc_info_t *wlc = ampdu_tx->wlc;
	uint16 seq;

	BCM_REFERENCE(wlc);

#ifdef BCMDBG_SWWLAN_38455
	if ((WLPKTTAG(p)->flags & WLF_AMPDU_MPDU) == 0) {
		WL_PRINT(("%s flags=%x seq=%d\n", __FUNCTION__,
			WLPKTTAG(p)->flags, WLPKTTAG(p)->seq));
	}
#endif /* BCMDBG_SWWLAN_38455 */
	ASSERT(WLPKTTAG(p)->flags & WLF_AMPDU_MPDU);

	if (!scb)
		return;

	SCB_PKTS_INFLT_FIFOCNT_DECR(scb, PKTPRIO(p));

	if (!SCB_AMPDU(scb))
		return;

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	ASSERT(scb_ampdu);

	ini = scb_ampdu->ini[PKTPRIO(p)];
	if (!ini || (ini->ba_state == AMPDU_TID_STATE_BA_PENDING_ON)) {
		WL_AMPDU_ERR(("wl%d: ampdu_dotxstatus_regmpdu: NULL ini or pon state for tid %d\n",
			wlc->pub->unit, PKTPRIO(p)));
		return;
	}
	ASSERT(ini->scb == scb);
#ifdef WLTAF
	if (!wlc_taf_txpkt_status(wlc->taf_handle, scb, ini->tid, p, TAF_TXPKT_STATUS_REGMPDU))
#endif // endif
	{
		wlc_ampdu_dec_bytes_inflight(ini, p);
	}

#ifdef WLATF
		/* Some of the devices may stop aggregating packets under noisy conditions
		 * This counter tracks the time a MPDU is sent from the AMPDU path
		 */
		WLCNTINCR(AMPDU_ATF_STATS(ini)->reg_mpdu);
#endif // endif

	seq = txh_info->seq;

	if (txs->status.was_acked) {
		ini->acked_seq = seq;
	} else {
#ifdef WL11K
		uint8 tid = PKTPRIO(p);
		wlc_rrm_stat_qos_counter(wlc, scb, tid, OFFSETOF(rrm_stat_group_qos_t, ackfail));
#endif // endif
		WLCNTINCR(ampdu_tx->cnt->txreg_noack);
		AMPDUSCBCNTINCR(scb_ampdu->cnt.txreg_noack);
		WL_AMPDU_TX(("wl%d: wlc_ampdu_dotxstatus_regmpdu: ack not recd for "
			"seq 0x%x tid %d status 0x%x\n",
			wlc->pub->unit, seq, ini->tid, txs->status.raw_bits));

		if ((AP_ENAB(wlc->pub) &&
			(txs->status.suppr_ind == TX_STATUS_SUPR_PMQ)) ||
			P2P_ABS_SUPR(wlc, txs->status.suppr_ind)) {
			/* N.B.: wlc_dotxstatus() will continue
			 * the rest of suppressed packet processing...
			 */
			return;
		}
	}
	return;
} /* wlc_ampdu_dotxstatus_regmpdu_aqm */

/**
 * Higher layer invokes this function as a result of the d11 core indicating tx completion. Function
 * is not active for AC chips that have AQM enabled. 'Regular' tx MPDU is a non-AMPDU contained
 * MPDU.
 */
static void
wlc_ampdu_dotxstatus_regmpdu_noaqm(ampdu_tx_info_t *ampdu_tx, struct scb *scb,
	void *p, tx_status_t *txs, wlc_txh_info_t *txh_info)
{
	scb_ampdu_tx_t *scb_ampdu;
	scb_ampdu_tid_ini_t *ini;
	uint16 indx, seq;

	ASSERT(WLPKTTAG(p)->flags & WLF_AMPDU_MPDU);

	if (!scb)
		return;

	SCB_PKTS_INFLT_FIFOCNT_DECR(scb, PKTPRIO(p));

	if (!SCB_AMPDU(scb))
		return;

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	ASSERT(scb_ampdu);

	ini = scb_ampdu->ini[PKTPRIO(p)];
	if (!ini || (ini->ba_state == AMPDU_TID_STATE_BA_PENDING_ON)) {
		WL_AMPDU_ERR(("wl%d: ampdu_dotxstatus_regmpdu: NULL ini or pon state for tid %d\n",
			ampdu_tx->wlc->pub->unit, PKTPRIO(p)));
		return;
	}
	ASSERT(ini->scb == scb);

	wlc_ampdu_dec_bytes_inflight(ini, p);
#ifdef WLATF
	/* Some of the devices like the 43226 and 43228 may stop
	 * aggregating packets under noisy conditions
	 * This counter tracks the time a MPDU is sent from the AMPDU path
	 */
	WLCNTINCR(AMPDU_ATF_STATS(ini)->reg_mpdu);
#endif // endif

	seq = txh_info->seq;

	if (MODSUB_POW2(seq, ini->start_seq, SEQNUM_MAX) >= ini->ba_wsize) {
		WL_AMPDU_ERR(("wl%d: %s: unexpected completion: seq 0x%x, "
			"start seq 0x%x\n",
			ampdu_tx->wlc->pub->unit, __FUNCTION__, seq, ini->start_seq));
		return;
	}

	indx = TX_SEQ_TO_INDEX(seq);
	if (!isset(ini->non_aqm->ackpending, indx)) {
		WL_AMPDU_ERR(("wl%d: %s: seq 0x%x\n",
			ampdu_tx->wlc->pub->unit, __FUNCTION__, seq));
		ampdu_dump_ini(ampdu_tx->wlc, ini);
		ASSERT(isset(ini->non_aqm->ackpending, indx));
		return;
	}

	if (txs->status.was_acked) {
		clrbit(ini->non_aqm->ackpending, indx);
		if (isset(ini->non_aqm->barpending, indx)) {
			clrbit(ini->non_aqm->barpending, indx);
		}
		ini->non_aqm->txretry[indx] = 0;

#ifdef PROP_TXSTATUS
		if (PROP_TXSTATUS_ENAB(ampdu_tx->wlc->pub)) {
			if (wlc_wlfc_suppr_status_query(ampdu_tx->wlc, scb)) {
				uint32 wlhinfo = WLPKTTAG(p)->wl_hdr_information;
				if ((WL_TXSTATUS_GET_FLAGS(wlhinfo) & WLFC_PKTFLAG_PKTFROMHOST) &&
					(WLPKTTAG(p)->flags & WLF_PROPTX_PROCESSED)) {
					wlfc_process_txstatus(ampdu_tx->wlc->wlfc,
						WLFC_CTL_PKTFLAG_SUPPRESS_ACKED, p, txs, FALSE);
				}
			}
		}
#endif /* PROP_TXSTATUS */

	} else {
#ifdef WL11K
		uint8 tid = PKTPRIO(p);
		wlc_rrm_stat_qos_counter(ampdu_tx->wlc, scb, tid,
			OFFSETOF(rrm_stat_group_qos_t, ackfail));
#endif // endif
		WLCNTINCR(ampdu_tx->cnt->txreg_noack);
		AMPDUSCBCNTINCR(scb_ampdu->cnt.txreg_noack);
		WL_AMPDU_TX(("wl%d: wlc_ampdu_dotxstatus_regmpdu: ack not recd for "
			"seq 0x%x tid %d status 0x%x\n",
			ampdu_tx->wlc->pub->unit, seq, ini->tid, txs->status.raw_bits));

		/* suppressed pkts are resent; so dont move window */
		if ((AP_ENAB(ampdu_tx->wlc->pub) &&
		     (txs->status.suppr_ind == TX_STATUS_SUPR_PMQ)) ||
		     P2P_ABS_SUPR(ampdu_tx->wlc, txs->status.suppr_ind)) {
			/* N.B.: wlc_dotxstatus() will continue
			 * the rest of suppressed packet processing...
			 */
			return;
		}

	}
} /* wlc_ampdu_dotxstatus_regmpdu_noaqm */

/** function to process tx completion of a 'regular' mpdu. Called by higher layer. */
void
wlc_ampdu_dotxstatus_regmpdu(ampdu_tx_info_t *ampdu_tx, struct scb *scb,
	void *p, tx_status_t *txs, wlc_txh_info_t *txh_info)
{
	if (AMPDU_AQM_ENAB(ampdu_tx->wlc->pub))
		wlc_ampdu_dotxstatus_regmpdu_aqm(ampdu_tx, scb, p, txs, txh_info);
	else
		wlc_ampdu_dotxstatus_regmpdu_noaqm(ampdu_tx, scb, p, txs, txh_info);
}

/**
 * Upon the d11 core indicating transmission of a packet, AMPDU packet(s) can be freed.
 */
static void
wlc_ampdu_free_chain(ampdu_tx_info_t *ampdu_tx, void *p, tx_status_t *txs)
{
	wlc_info_t *wlc = ampdu_tx->wlc;
	uint8 queue;
	uint16 count = 0;
	struct scb *scb = NULL;
	uint prio = 0;
#ifdef TX_STATUS_DBG
	bool mixed_cfp_legacy = FALSE;
#endif /* TX_STATUS_DBG */

	uint16 ncons = 0; /**< AQM. Last d11 seq that was processed by d11 core. */

	if (AMPDU_AQM_ENAB(ampdu_tx->wlc->pub)) {
		ncons = wlc_ampdu_rawbits_to_ncons(txs->status.raw_bits);
		if (!ncons || (ncons > 64)) {
			WL_ERROR(("%s ncons unexpected: %d, txs: %p, p:%p\n", __FUNCTION__, ncons,
				txs, p));
			wlc_print_txstatus(wlc, txs);
		}
		ASSERT(ncons && (ncons <= 64));
	} else {
		ncons = txs->sequence;
	}

	if (p != NULL) {
		scb = WLPKTTAGSCBGET(p);
		prio = PKTPRIO(p);
#ifdef TX_STATUS_DBG
		mixed_cfp_legacy = PKTISCFP(p);
#endif /* TX_STATUS_DBG */
	}

	WL_AMPDU_TX(("wl%d: wlc_ampdu_free_chain: free ampdu_tx chain\n", wlc->pub->unit));

	queue = D11_TXFID_GET_FIFO(wlc, txs->frameid);

	/* loop through all packets in the dma ring and free */
	while (p) {
#ifdef BCMDBG_SWWLAN_38455
		if ((WLPKTTAG(p)->flags & WLF_AMPDU_MPDU) == 0) {
			WL_PRINT(("%s flags=%x seq=%d queue=%d\n", __FUNCTION__,
				WLPKTTAG(p)->flags, WLPKTTAG(p)->seq, queue));
		}
#endif /* BCMDBG_SWWLAN_38455 */
		ASSERT(WLPKTTAG(p)->flags & WLF_AMPDU_MPDU);

		count++;

		/* Update pkttag's scb from the first packet */
		WLPKTTAGSCBSET(p, scb);

		wlc_tx_status_update_counters(wlc, p, scb, NULL,
			WLC_TX_STS_COMP_NO_SCB, 1);

		WLCNTINCR(ampdu_tx->cnt->orphan);

		if (AMPDU_HOST_ENAB(wlc->pub)) {
			d11txhdr_t *txh;
			uint16 mcl;
			if (D11REV_LT(wlc->pub->corerev, 40)) {
				txh = (d11txhdr_t *)PKTDATA(wlc->osh, p);
			} else {
				uint8 *pkt_data = PKTDATA(wlc->osh, p);
				uint tsoHdrSize = 0;
#ifdef WLTOEHW
				tsoHdrSize = WLC_TSO_HDR_LEN(wlc, (d11ac_tso_t*)pkt_data);
#endif /* WLTOEHW */
				txh = (d11txhdr_t *)(pkt_data + tsoHdrSize);
			}
			mcl = ltoh16(*D11_TXH_GET_MACLOW_PTR(wlc, txh));

			PKTFREE(wlc->osh, p, TRUE);

			/* if not the last ampdu, take out the next packet from dma chain */
			if (((mcl & TXC_AMPDU_MASK) >> TXC_AMPDU_SHIFT) == TXC_AMPDU_LAST)
				break;
		} else {
			PKTFREE(wlc->osh, p, TRUE);
			if (count >= ncons) {
				break;
			}
		}

		p = GETNEXTTXP(wlc, queue);
	}

	if (scb) {
		SCB_PKTS_INFLT_FIFOCNT_SUB(scb, prio, count);
	}

	if (AMPDU_ENAB(wlc->pub) && AMPDU_HOST_ENAB(wlc->pub)) {
		wlc_txfifo_complete(wlc, queue, ampdu_tx->config->txpkt_weight);
	} else {
		wlc_txfifo_complete(wlc, queue, count);
	}

	WL_AMPDU_TX(("%s: fifo %d pkts %d txpktpend %d\n",
		__FUNCTION__, queue, count, TXPKTPENDGET(wlc, queue)));

	/* for corerev >= 40, all txstatus have been read already */
	if (D11REV_GE(wlc->pub->corerev, 40))
		return;

	/* retrieve the next status if it is there */
	if (txs->status.was_acked) {
		wlc_hw_info_t *wlc_hw = wlc->hw;
		wlc_txs_pkg8_t pkg;
		int txserr;
		uint8 status_delay = 0;

		ASSERT(txs->status.is_intermediate);

		/* wait till the next 8 bytes of txstatus is available */
		while ((txserr = wlc_bmac_read_txs_pkg8(wlc_hw, &pkg)) == BCME_NOTREADY) {
			OSL_DELAY(1);
			status_delay++;
			if (status_delay > 10) {
				ASSERT(0);
				return;
			}
		}

		ASSERT(!(pkg.word[0] & TX_STATUS_INTERMEDIATE));
		ASSERT(pkg.word[0] & TX_STATUS_AMPDU);

		/* clear the txs pkg2 read required flag */
		if ((txs->procflags & TXS_PROCFLAG_AMPDU_BA_PKG2_READ_REQD) && txserr == BCME_OK) {
			txs->procflags &= ~TXS_PROCFLAG_AMPDU_BA_PKG2_READ_REQD;
		}
	}
} /* wlc_ampdu_free_chain */

/**
 * Called by higher layer when d11 core indicates transmit completion of an MPDU
 * that has its 'AMPDU' packet flag set.
 */
bool BCMFASTPATH
wlc_ampdu_dotxstatus(ampdu_tx_info_t *ampdu_tx, struct scb *scb, void *p, tx_status_t *txs,
	wlc_txh_info_t *txh_info)
{
	scb_ampdu_tx_t *scb_ampdu;
	wlc_info_t *wlc = ampdu_tx->wlc;
#ifdef AMPDU_NON_AQM
	wlc_txs_pkg8_t pkg;
#endif /* AMPDU_NON_AQM */
	scb_ampdu_tid_ini_t *ini;
	/* For low driver, we don't want this function to return TRUE for certain err conditions
	 * returning true will cause a wlc_fatal_error() to get called.
	 * we want to reserve wlc_fatal_error() call for hw related err conditions.
	 */
	bool need_reset = FALSE;
	BCM_REFERENCE(wlc);
	ASSERT(p);
	ASSERT(WLPKTTAG(p)->flags & WLF_AMPDU_MPDU);

#ifdef WLAMPDU_UCODE
	/* update side_channel upfront in case we bail out early */
	if (AMPDU_UCODE_ENAB(wlc->pub)) {
		ampdumac_info_t *hwagg;
		uint16 max_entries = 0;
		uint16 queue;
		uint16 ncons; /**< number of consumed packets */

		queue = D11_TXFID_GET_FIFO(wlc, txs->frameid);
		ASSERT(queue < AC_COUNT);
		hwagg = &(ampdu_tx->hwagg[queue]);
		ncons = txs->sequence;

		max_entries = hwagg->txfs_addr_end - hwagg->txfs_addr_strt + 1;
		hwagg->txfs_rptr += ncons;
		if (hwagg->txfs_rptr > hwagg->txfs_addr_end)
			hwagg->txfs_rptr -= max_entries;

		ASSERT(ncons > 0 && ncons < max_entries && ncons <= hwagg->in_queue);
		BCM_REFERENCE(max_entries);
		hwagg->in_queue -= ncons;
	}
#endif /* WLAMPDU_UCODE */

	if (!scb || !SCB_AMPDU(scb)) {
		/* null scb may happen if pkts were in txq when scb removed */
		if (!scb) {
			WL_AMPDU(("wl%d: %s: scb is null\n",
				wlc->pub->unit, __FUNCTION__));
			/* pkttag must have been set null in wlc_dotxstatus */
			ASSERT(WLPKTTAGSCBGET(p) == NULL);
		} else {
			WLPKTTAGSCBSET(p, NULL);
		}
		wlc_ampdu_free_chain(ampdu_tx, p, txs);
		return need_reset;
	}

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	ASSERT(scb_ampdu);

	ini = scb_ampdu->ini[(uint8)PKTPRIO(p)];
	if (!ini || (ini->ba_state == AMPDU_TID_STATE_BA_PENDING_ON)) {
		WL_AMPDU_ERR(("wl%d: %s: bail: bad ini (%p) or ba_state (%d) prio %d\n",
			wlc->pub->unit, __FUNCTION__, OSL_OBFUSCATE_BUF(ini),
			(ini ? ini->ba_state : -1), (int)PKTPRIO(p)));
		wlc_ampdu_free_chain(ampdu_tx, p, txs);
		return need_reset;
	}
	ASSERT(ini->scb == scb);
	ASSERT(WLPKTTAGSCBGET(p) == scb);

	if (D11REV_GE(wlc->pub->corerev, 40)) {
		wlc_ampdu_dotxstatus_aqm_complete(ampdu_tx, scb, p, txs, txh_info);
		/* agg may be half done here */
		wlc_ampdu_agg_complete(wlc, ampdu_tx, scb_ampdu, ini, FALSE);
#ifdef BCMDBG
		WLC_AMPDU_TXQ_PROF_ADD_ENTRY(ampdu_tx->wlc, scb_ampdu->scb);
#endif // endif
		return FALSE;
	}

#ifdef AMPDU_NON_AQM
	bzero(&pkg, sizeof(pkg));

	/* BMAC_NOTE: For the split driver, second level txstatus comes later
	 * So if the ACK was received then wait for the second level else just
	 * call the first one
	 */
	if (txs->status.was_acked) {
		wlc_hw_info_t *wlc_hw = wlc->hw;
		int txserr;
		uint8 status_delay = 0;

		scb->used = wlc->pub->now;
		/* wait till the next 8 bytes of txstatus is available */
		while ((txserr = wlc_bmac_read_txs_pkg8(wlc_hw, &pkg)) == BCME_NOTREADY) {
			OSL_DELAY(1);
			status_delay++;
			if (status_delay > 10) {
				WL_PRINT(("wl%d func %s status_delay %u frameId %u",
					wlc->pub->unit, __FUNCTION__,
					status_delay, txs->frameid));
				ASSERT(0);
				wlc_ampdu_free_chain(ampdu_tx, p, txs);
				return TRUE;
			}
		}
		/* clear the txs pkg2 read required flag */
		if ((txs->procflags & TXS_PROCFLAG_AMPDU_BA_PKG2_READ_REQD) && txserr == BCME_OK) {
			txs->procflags &= ~TXS_PROCFLAG_AMPDU_BA_PKG2_READ_REQD;
		}

		WL_AMPDU_TX(("wl%d: wlc_ampdu_dotxstatus: 2nd status in delay %d\n",
			wlc->pub->unit, status_delay));

		ASSERT(!(pkg.word[0] & TX_STATUS_INTERMEDIATE));
		ASSERT(pkg.word[0] & TX_STATUS_AMPDU);
	}

	wlc_ampdu_dotxstatus_complete_noaqm(ampdu_tx, scb, p, txs,
		pkg.word[0], pkg.word[1]);

	wlc_ampdu_txflowcontrol(wlc, scb_ampdu, ini);

#endif /* AMPDU_NON_AQM */

	return FALSE;
} /* wlc_ampdu_dotxstatus */

#ifdef WL_CS_PKTRETRY
static INLINE void
wlc_ampdu_cs_pktretry(struct scb *scb, void *p, uint8 tid)
{
	wlc_pkttag_t *pkttag = WLPKTTAG(p);

	pkttag->flags |= WLF_FIFOPKT; /* retrieved from HW FIFO */
	pkttag->flags3 |= WLF3_SUPR; /* suppressed */
	WLPKTTAGSCBSET(p, scb);

	SCB_TX_NEXT(TXMOD_AMPDU, scb, p, WLC_PRIO_TO_HI_PREC(tid));
}
#endif /* WL_CS_PKTRETRY */

/**
 * Updating the RTS CTS counts to be used by dynamic frameburst.
 */
static void BCMFASTPATH
wlc_frmburst_dotxstatus(ampdu_tx_info_t *ampdu_tx, tx_status_t *txs)
{
	if (ampdu_tx->wlc->hti->frameburst && ampdu_tx->config->fb_override_enable &&
	    WLC_HT_GET_AMPDU_RTS(ampdu_tx->wlc->hti)) {
		ampdu_tx->rtstxcnt += txs->status.rts_tx_cnt;
		ampdu_tx->ctsrxcnt += txs->status.cts_rx_cnt;
	} /* if (ampdu_tx->wlc->hti->frameburst ...) */
} /* wlc_frmburst_dotxstatus */

/*
 * Function to populate rs_txs based on txs
 */
static INLINE void
wlc_ampdu_fill_rs_txs(ratesel_txs_t* rs_txs, tx_status_t *txs, uint8 ac, uint16 ncons, uint16 nlost)
{
	rs_txs->ncons = (ncons > 0xff) ? 0xff : (uint8)ncons;
	rs_txs->nlost = nlost;
	rs_txs->txrts_cnt = txs->status.rts_tx_cnt;
	rs_txs->rxcts_cnt = txs->status.cts_rx_cnt;
	rs_txs->ack_map1 = txs->status.ack_map1;
	rs_txs->ack_map2 = txs->status.ack_map2;
	rs_txs->frameid = txs->frameid;
	/* zero for aqm */
	rs_txs->antselid = 0;
	rs_txs->ac = ac;
}

#if defined(WL_MU_TX)
void BCMFASTPATH
wlc_ampdu_aqm_mutx_dotxinterm_status(ampdu_tx_info_t *ampdu_tx, tx_status_t *txs)
{
	bool txs_mu;
	wlc_info_t *wlc = ampdu_tx->wlc;
	void *p;
	struct scb *scb = NULL;
	wlc_bsscfg_t *bsscfg = NULL;
	wlc_txh_info_t txh_info;
	int err;
	uint16 ncons;
	ratesel_txs_t rs_txs;
	uint8 tid, ac, queue;
	uint8 rtidx = 0;
	uint8 txs_mutype = TX_STATUS_MUTP_VHTMU;
	const wlc_rlm_rate_store_t *rstore = NULL;
	uint8 ampdu_ft;
	bool update_ratesel = FALSE;

	BCM_REFERENCE(err);
	BCM_REFERENCE(queue);
	BCM_REFERENCE(rstore);
	BCM_REFERENCE(ampdu_ft);

	txs_mu = (txs->status.s5 & TX_STATUS64_MUTX) ? TRUE : FALSE;
	if (!txs_mu)
		return;

	queue = D11_TXFID_GET_FIFO(wlc, txs->frameid);
	ASSERT(queue < WLC_HW_NFIFO_TOTAL(wlc));

	p = wlc_bmac_dmatx_peeknexttxp(wlc, queue);
	if (p == NULL) {
		return;
	}

	if (!(WLPKTTAG(p)->flags & WLF_TXHDR)) {
		return;
	}

	tid = (uint8)PKTPRIO(p);
	ac = WME_PRIO2AC(tid);

	wlc_get_txh_info(wlc, p, &txh_info);
	if (txs->frameid != htol16(txh_info.TxFrameID)) {
		WL_ERROR(("wl%d: %s: txs->frameid 0x%x txh->TxFrameID 0x%x\n",
			wlc->pub->unit, __FUNCTION__,
			txs->frameid, htol16(txh_info.TxFrameID)));
		return;
	}

	if (RATELINKMEM_ENAB(wlc->pub)) {
		/* rev 128 and up */
		d11txh_rev128_t *h = &txh_info.hdrPtr->rev128;
		uint16 rmem_idx = ltoh16(h->RateMemIdxRateIdx) &
			D11_REV128_RATEIDX_MASK;
		rstore = wlc_ratelinkmem_retrieve_cur_rate_store(wlc, rmem_idx, FALSE);
	}

	txs_mutype = TX_STATUS_MUTYP(wlc->pub->corerev, txs->status.s5);

	if (txs_mutype == TX_STATUS_MUTP_HEOM) {
		rtidx = TX_STATUS128_HEOM_RTIDX(txs->status.s4);
	} else {
		rtidx = 0;
	}
	ASSERT(rtidx < RATESEL_MFBR_NUM);

	bzero(&rs_txs, sizeof(rs_txs));

	rs_txs.tx_cnt[rtidx]     = TX_STATUS40_TXCNT_RT0(txs->status.s3);
	rs_txs.txsucc_cnt[rtidx] = TX_STATUS40_ACKCNT_RT0(txs->status.s3);

#ifdef WL11AX
	/* Update ampdu stats */
	if (txs_mutype == TX_STATUS_MUTP_HEOM) {
		uint gi, mcs, nss, rateidx;

		BCM_REFERENCE(gi);
		BCM_REFERENCE(mcs);
		BCM_REFERENCE(nss);
		BCM_REFERENCE(rateidx);

		if (rstore) {
			rs_txs.txrspec[rtidx] = rstore->rspec[rtidx];
		}

		mcs = TX_STATUS64_MU_MCS(txs->status.s4);
		nss = TX_STATUS64_MU_NSS(txs->status.s4) + 1;
		gi = TX_STATUS128_HEOM_CPLTF(txs->status.s4);

		rateidx = (nss - 1) * MAX_HE_RATES + mcs;

		ASSERT(gi < AMPDU_MAX_HE_GI);
		ASSERT(rateidx < AMPDU_MAX_HE);
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU) || defined(WL_LINKSTAT)
		WLCNTADD(ampdu_tx->amdbg->txhe[gi][rateidx], rs_txs.tx_cnt[rtidx]);
		WLCNTADD(ampdu_tx->amdbg->txhe_succ[gi][rateidx], rs_txs.txsucc_cnt[rtidx]);
		if (rs_txs.txrspec[rtidx] & WL_RSPEC_STBC) {
			WLCNTADD(ampdu_tx->amdbg->txhestbc[rateidx], rs_txs.tx_cnt[rtidx]);
		}
#endif /*  defined(BCMDBG) || defined(WLTEST) */
		if (rs_txs.txrspec[rtidx] & WL_RSPEC_STBC) {
			WLCNTADD(ampdu_tx->cnt->u2.txmpdu_stbc, rs_txs.tx_cnt[rtidx]);
		}

		/* Decide if it is qualified for rate sel update or not */
		update_ratesel = (WLPKTTAG(p)->flags & WLF_RATE_AUTO) ? TRUE : FALSE;
		update_ratesel = (txs->phyerr == 0) ? TRUE : FALSE;
		/* XXX: No need to check suppression status as intermmediate txs update does
		 * not have suppression info
		 */
	}
#endif /* WL11AX */

	ampdu_ft = AMPDU_PPDU_FT(txs_mu, txs_mutype);
	ASSERT(ampdu_ft < AMPDU_PPDU_FT_MAX);
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU) || defined(WL_LINKSTAT)
	WLCNTADD(ampdu_tx->amdbg->txmpdu[ampdu_ft], rs_txs.tx_cnt[rtidx]);
	WLCNTADD(ampdu_tx->amdbg->txmpdu_succ[ampdu_ft], rs_txs.txsucc_cnt[rtidx]);
#endif /*  defined(BCMDBG) || defined(WLTEST) */

	ncons = wlc_ampdu_rawbits_to_ncons(txs->status.raw_bits);
	wlc_ampdu_fill_rs_txs(&rs_txs, txs, ac, ncons, 0);

	if (!ETHER_ISMULTI(txh_info.TxFrameRA)) {
		/* use the bsscfg index in the packet tag to find out which
		 * bsscfg this packet belongs to
		 */
		bsscfg = wlc_bsscfg_find(wlc, WLPKTTAGBSSCFGGET(p), &err);
		/* For ucast frames, lookup the scb directly by the RA.
		 * The scb may not be found if we sent a frame with no scb, or if the
		 * scb was deleted while the frame was queued.
		 */
		if (bsscfg != NULL) {
			scb_ampdu_tx_t *scb_ampdu;
			scb_ampdu_tid_ini_t *ini;

			scb = wlc_scbfindband(wlc, bsscfg,
				(struct ether_addr*)txh_info.TxFrameRA,
				(wlc_txh_info_is5GHz(wlc, &txh_info)
				?
				BAND_5G_INDEX : BAND_2G_INDEX));
			if (scb == NULL) {
				return;
			}

			scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
			ini = scb_ampdu->ini[tid];
			if (!ini || (ini->ba_state == AMPDU_TID_STATE_BA_PENDING_ON)) {
				return;
			}

			wlc_macdbg_dtrace_log_txs(wlc->macdbg, scb, &rs_txs.txrspec[0], txs);

			wlc_mutx_upd_interm_counters(wlc->mutx, scb, txs);

			if (update_ratesel) {
				wlc_scb_ratesel_upd_itxs(wlc->wrsi, scb, &rs_txs, txs, 0);
			}

			if (txs_mutype == TX_STATUS_MUTP_HEOM) {
#if defined(BCMDBG) || defined(DL_RU_STATS_DUMP)
				wlc_musched_upd_ru_stats(wlc->musched, scb, txs);
#endif // endif
			}
		}
	}
}
#endif /* defined(WL_MU_TX) */

/**
 * Process txstatus which reports how many frames are consumed in txs::ncons.
 * The <ncons> pkts have been either
 * -- suppressed, or
 * -- successfully transmitted and acked, or
 * -- transmitted upto retry limits and been given up.
 * If suppressed, driver should find a way to retry them with seqnumber
 * handled carefully (and change epoch properly).
 * XXX: this path is *not* handled. (Chunyu: I don't know how.)
 *
 * Missing or unverified parts:
 * 1. p2p retry
 * 2. pktlog
 * 3. txmonitor
 * 4. pktdelay
 *
 * The d11 AC/AQM core generates two 'txstatus packages' for every A-MPDU it
 * processes. These packages are contained in parameter 'txs'.
 *
 * This function will not only process the caller-supplied mpdu 'p', but also
 * any subsequent mpdus on the respective d11 dma/fifo queue.
 */

/* XXX SUNNY_DAY_FUNCTIONALITY:
 * - determine if a retry is required
 *     (e.g. when packet was suppressed by the d11 core)
 *     (e.g. for PSPRETEND)
 * - re-queue MPDU when a retry is required
 * - send a BAR in case of one or more unacked MPDUs
 * - update AMPDU SW internal bookkeeping
 *     (e.g. ini->tx_in_transit)
 *     (e.g. move AMPDU 'window')
 * - Frees individual MPDUs unless they must be retried
 * - dequeues packets from respective d11/DMA queue
 * - updates tallies
 */
static void BCMFASTPATH
wlc_ampdu_dotxstatus_aqm_complete(ampdu_tx_info_t *ampdu_tx, struct scb *scb,
                                  void *p, tx_status_t *txs, wlc_txh_info_t *txh_info)
{
	scb_ampdu_tx_t *scb_ampdu;
	wlc_info_t *wlc = ampdu_tx->wlc;
	scb_ampdu_tid_ini_t *ini;
	wlc_bsscfg_t *bsscfg;

	uint8 queue; /**< d11 dma/fifo queue from where subsequent pkts are drawn */
	uint8 tid, tot_mpdu = 0;

#ifdef WLSCB_HISTO
	uint16 succ_msdu = 0;
	uint16 succ_mpdu = 0;
	uint8 amsdu_sf;
#endif /* WLSCB_HISTO */
	/* per txstatus info */
	uint ncons; /**< #consumed mpdus, as reported by d11 core through txs */
	uint supr_status;
	bool update_ratesel;
	bool retry_suppr_pkts = FALSE;
	bool fix_rate = FALSE;

	/* per packet info */
	bool was_acked = FALSE, send_bar = FALSE;
	uint16 seq = 0, bar_seq = 0;

	/* for first packet */
	int rnum = 0;
	ratesel_txs_t rs_txs;

	bool txs_mu = FALSE;	/* If txed as MU */
#if defined(WL_MU_TX) && defined(WLCNT)
	bool is_mu = FALSE;	/* If queued as MU */
#endif /* WL_MU_TX && WLCNT */
	uint8 txs_mutype = TX_STATUS_MUTP_VHTMU;
#ifdef PKTQ_LOG
	uint16 mcs_txrate[RATESEL_MFBR_NUM];
#endif // endif
#if defined(SCB_BS_DATA)
	wlc_bs_data_counters_t *bs_data_counters = NULL;
#endif // endif

	uint16 start_seq = 0xFFFF; /* invalid */

	uint8 ac;
#if defined(WLPKTDLYSTAT) || defined(WL11K)
	uint32 delay, now;
#endif // endif
#if defined WLPKTDLYSTAT
	uint tr = 0;
	scb_delay_stats_t *delay_stats;
#endif // endif

	int k;
	uint16 nlost = 0;
#ifdef PKTQ_LOG
	pktq_counters_t *prec_cnt = NULL;
	pktq_counters_t *actq_cnt = NULL;
	uint32           prec_pktlen = 0;
	uint32		hdr_len = 0;
	uint32		acked_pktlen = 0;
	uint32		ack_cnt = 0;
#endif // endif
#ifdef PSPRETEND
	bool pps_retry = FALSE;
	bool pps_recvd_ack = FALSE;
	uint16	macCtlHigh = 0;
#endif /* PSPRETEND */
#if defined(PKTQ_LOG) || defined(PSPRETEND)
	d11txhdr_t* txh = NULL;
#endif /* PKTQ_LOG || PSPRETEND */
	wlc_pkttag_t* pkttag;
#if defined(STA) && defined(WL_TX_STALL) && defined(WL_TX_CONT_FAILURE)
	ratespec_t tx_rspec;
#endif /* STA && WL_TX_STALL && WL_TX_CONT_FAILURE */
	bool from_host = TRUE;
#ifdef WL_CS_PKTRETRY
	bool cs_retry = wlc_ampdu_cs_retry(wlc, txs, p);
#endif // endif
#ifdef WL_TX_STALL
	wlc_tx_status_t tx_status = WLC_TX_STS_SUCCESS;
#endif // endif
	struct scb *scb1;
#ifdef BULK_PKTLIST
	void *list_head;
#endif // endif
	const wlc_rlm_rate_store_t *rstore = NULL;

	uint16 ft[RATESEL_MFBR_NUM] = { 0 };
	uint16 ft_fmt[RATESEL_MFBR_NUM] = { 0 };
	uint bw;
	uint tainted = TRUE; /* used in SW cache-coherent systems */

	bool fastmode;
	struct spktq spq; /**< Single priority packet queue */

	if (BCMPCIEDEV_ENAB()) {
		fastmode = TRUE;
		spktq_init(&spq, PKTQ_LEN_MAX);
	} else {
		fastmode = FALSE;
	}

	bzero(&rs_txs, sizeof(rs_txs));

	BCM_REFERENCE(start_seq);
	BCM_REFERENCE(bsscfg);
	BCM_REFERENCE(from_host);
	BCM_REFERENCE(bw);
#if defined(PKTQ_LOG) || defined(PSPRETEND)
	BCM_REFERENCE(txh);
#endif // endif
	BCM_REFERENCE(tainted);

	ncons = wlc_ampdu_rawbits_to_ncons(txs->status.raw_bits);

	if (!ncons) {
		WL_AMPDU_ERR(("ncons is %d in %s", ncons, __FUNCTION__));
		wlc_tx_status_update_counters(wlc, p,
			scb, NULL, WLC_TX_STS_BAD_NCONS, 1);
		PKTFREE(wlc->osh, p, TRUE);
		ASSERT(ncons);
		return;
	} else if (ncons > 64) {
		WL_ERROR(("%s; ERROR ncons %d, raw_bits: 0x%x frameid %d\n", __FUNCTION__, ncons,
			txs->status.raw_bits, txs->frameid));
		wlc_print_txstatus(wlc, txs);
		ASSERT(0);
	}

	WLCNTADD(ampdu_tx->cnt->cons, ncons);

	if (!scb || !SCB_AMPDU(scb)) {
		if (!scb) {
			WL_AMPDU(("wl%d: %s: scb is null\n",
				wlc->pub->unit, __FUNCTION__));
			/* pkttag must have been set null in wlc_dotxstatus */
			ASSERT(WLPKTTAGSCBGET(p) == NULL);
		} else {
			WLPKTTAGSCBSET(p, NULL);
		}
		wlc_ampdu_free_chain(ampdu_tx, p, txs);
		return;
	}

	SCB_BS_DATA_CONDFIND(bs_data_counters, wlc, scb);

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	bsscfg = SCB_BSSCFG(scb);
	ASSERT(scb_ampdu);
	ASSERT(p);
	tid = (uint8)PKTPRIO(p);

	queue = D11_TXFID_GET_FIFO(wlc, txs->frameid);
	ASSERT(queue < WLC_HW_NFIFO_TOTAL(wlc));

	ac = WME_PRIO2AC(tid);
#ifdef WLPKTDLYSTAT
	delay_stats = scb->delay_stats + ac;
#endif // endif
	if (RATELINKMEM_ENAB(wlc->pub)) {
		/* rev 128 and up */
		d11txh_rev128_t *h = &txh_info->hdrPtr->rev128;
		uint16 rmem_idx = ltoh16(h->RateMemIdxRateIdx) &
			D11_REV128_RATEIDX_MASK;
		rstore = wlc_ratelinkmem_retrieve_cur_rate_store(wlc, rmem_idx, FALSE);
		if (rstore == NULL) {
			/* can happen when SCB is being cleaned up */
			wlc_ampdu_free_chain(ampdu_tx, p, txs);
			return;
		}
	} else {
		BCM_REFERENCE(rstore);
	}

	/*
	2	RTS tx count
	3	CTS rx count
	11:4	Acked bitmap for each of consumed mpdus reported in this txstatus, sequentially.
	That is, bit 0 is for the first consumed mpdu, bit 1 for the second consumed frame.
	*/
	ini = scb_ampdu->ini[tid];
	if (!ini) {
		WL_AMPDU_ERR(("%s: bad ini error\n", __FUNCTION__));
		wlc_ampdu_free_chain(ampdu_tx, p, txs);
		return;
	}

	update_ratesel = (WLPKTTAG(p)->flags & WLF_RATE_AUTO) ? TRUE : FALSE;
	supr_status = txs->status.suppr_ind; /* status can be for multiple MPDUs */

	if (supr_status != TX_STATUS_SUPR_NONE) {
		if ((supr_status == TX_STATUS_SUPR_UF) && BCM_DMA_CT_ENAB(wlc) &&
		    wlc_bmac_is_pcielink_slowspeed(wlc->hw)) {
			/* only for known slowspeed and cut-through mode,
			 * let rate sel to throttle the rate
			 * update_ratesel will be left as what it is / TRUE
			 */
		} else {
			update_ratesel = FALSE;
		}
#if defined(BCMDBG) || defined(BCMDBG_AMPDU)
		ampdu_tx->amdbg->supr_reason[supr_status] += ncons;
#endif // endif
		WL_AMPDU_TX(("wl%d: %s: supr_status 0x%x\n",
			wlc->pub->unit, __FUNCTION__, supr_status));
		if (supr_status == TX_STATUS_SUPR_LMEM_INVLD) {
			/* stub, to avoid below of retrying these suppressed packets */
		} else if (supr_status == TX_STATUS_SUPR_BADCH) {
			WLCNTADD(wlc->pub->_cnt->txchanrej, ncons);
		} else if (supr_status == TX_STATUS_SUPR_EXPTIME) {
			/* XXX 4360 FIXME: retry - there is a need
			 * to update the epoch and make sure the send ampdu code will
			 * correctly handle the sequence numbers
			 */
			WLCNTADD(wlc->pub->_cnt->txexptime, ncons);
#ifdef WL11K
			wlc_rrm_tscm_upd(wlc, scb, tid,
				OFFSETOF(rrm_tscm_t, msdu_exp), ncons);
#endif // endif
			/* Interference detected */
			if (wlc->rfaware_lifetime)
				wlc_exptime_start(wlc);
		} else if (supr_status == TX_STATUS_SUPR_FRAG) {
			WL_AMPDU_ERR(("wl%d: %s: tx underflow?!\n",
					wlc->pub->unit, __FUNCTION__));
		} else if (supr_status == TX_STATUS_SUPR_TWT) {
			ASSERT(ltoh16(*D11_TXH_GET_MACHIGH_PTR(wlc, txh_info->hdrPtr)) &
				D11REV128_TXC_TWT);
			/* update_ratesel is already FALSE at this point, just let the packet
			 * get suppressed byt twt suppress logic.
			 */
			retry_suppr_pkts = TRUE;
		} else if (wlc_should_retry_suppressed_pkt(wlc, p, supr_status)) {
			/* N.B.: we'll transmit the packet when coming out of the
			 * absence period....use the retry logic below to reenqueue
			 * the packet.
			 */
			retry_suppr_pkts = TRUE;
		}
	} else if (txs->phyerr) {
		update_ratesel = FALSE;
		WLCNTADD(wlc->pub->_cnt->txphyerr, ncons);
		WL_ERROR(("wl%d: %s: tx phy error (0x%x)\n",
			wlc->pub->unit, __FUNCTION__, txs->phyerr));
#ifdef BCMDBG
		wlc_dump_phytxerr(wlc, txs->phyerr);
#endif /* BCMDBG */
	}

#if defined(BCMDBG) && defined(PSPRETEND)
	/* we are draining the fifo yet we received a tx status which isn't suppress - this
	 * is an error we should trap if we are still in the same ps pretend instance
	 */
	if ((BSSCFG_AP(bsscfg) || BSSCFG_IBSS(bsscfg)) &&
	    PS_PRETEND_ENABLED(bsscfg)) {
		wlc_pspretend_supr_upd(wlc->pps_info, bsscfg, scb, supr_status);
	}
#endif /* BCMDBG && PSPRETEND */

	/* Check if interference is still there */
	if (wlc->rfaware_lifetime && wlc->exptime_cnt && (supr_status != TX_STATUS_SUPR_EXPTIME))
		wlc_exptime_check_end(wlc);

	if (supr_status == TX_STATUS_SUPR_NONE) {
		wlc_frmburst_dotxstatus(ampdu_tx, txs);
	}

#if defined(WLPKTDLYSTAT) || defined(WL11K)
	/* Get the current time */
	now = WLC_GET_CURR_TIME(wlc);
#endif // endif

#ifdef PROP_TXSTATUS
	from_host = WL_TXSTATUS_GET_FLAGS(WLPKTTAG(p)->wl_hdr_information) &
		WLFC_PKTFLAG_PKTFROMHOST;
#endif	/* PROP_TXSTATUS */
#if defined(STA) && defined(WL_TX_STALL) && defined(WL_TX_CONT_FAILURE)
	tx_rspec = WLPKTTAG(p)->rspec;
#endif /* STA && WL_TX_STALL && WL_TX_CONT_FAILURE */

#ifdef BULK_PKTLIST
	/* Use BULK TX DMA completion if there are at least 2 packets to consume */
	if (ncons > 1) {
		/* In the non-ASSERT builds we would have had to add a redundant BCM_REFERENCE()
		 * directive to shut the compiler up, it consumes unnecessary cycles
		 */
		wlc_bmac_dma_bulk_txcomplete(wlc, queue, (ncons - 1),
				&list_head, NULL, HNDDMA_RANGE_ALL, FALSE);
	}
#endif /* BULK_PKTLIST */
#ifdef PKTQ_LOG
	/* XXX FIXME Re-check. ?
	 * Assumption: All MPDUs marked by BA to have same tid
	 */
	if (scb_ampdu->txq.pktqlog) {
		prec_cnt = wlc_ampdu_pktqlog_cnt(&scb_ampdu->txq, tid,
			WLAMPDU_PREC_TID2PREC(p, tid));
	}

	if ((WLC_GET_CQ(wlc->active_queue))->pktqlog) {
		actq_cnt =
			(WLC_GET_CQ(wlc->active_queue))->
				pktqlog->_prec_cnt[WLC_PRIO_TO_PREC(tid)];
	}
#endif /* PKTQ_LOG */

#ifdef WLSCB_HISTO
	amsdu_sf = (AMSDU_TX_ENAB(wlc->pub) & (SCB_AMSDU_IN_AMPDU(scb) != 0)) ?
		wlc_amsdu_scb_max_sframes(wlc->ami, scb, tid) : 1;
#endif /* WLSCB_HISTO */

	/* ncons is non-zero, so can enter unconditionally. exit via break in loop
	 * body.
	 */

#ifdef WLWRR
	wlc_ampdu_wrr_update_rstat(&AMPDU_WRR_STATS(ini)->ncons, ncons);
	if (WRR_ADAPTIVE(wlc->pub)) {
		wlc_tx_wrr_update_ncons(scb, ac, ncons);
	}
#endif // endif

	while (TRUE) { /* loops over each tx MPDU in the caller supplied tx sts */
		bool free_mpdu = TRUE;

		/* Make sure all MPDUs are of same tid */
		ASSERT(ini->tid == PKTPRIO(p));
		SCB_PKTS_INFLT_FIFOCNT_DECR(scb, tid);

		pkttag = WLPKTTAG(p);

#ifdef BCMDBG_SWWLAN_38455
		if ((WLPKTTAG(p)->flags & WLF_AMPDU_MPDU) == 0) {
			WL_PRINT(("%s flags=%x seq=%d supr_status=%d queue=%d\n", __FUNCTION__,
				WLPKTTAG(p)->flags, WLPKTTAG(p)->seq, supr_status, queue));
		}
#endif /* BCMDBG_SWWLAN_38455 */

		ASSERT(pkttag->flags & WLF_AMPDU_MPDU);

		seq = pkttag->seq;
#ifdef PROP_TXSTATUS
		seq = WL_SEQ_GET_NUM(seq);
#endif /* PROP_TXSTATUS */

#ifdef PKTQ_LOG
		if (supr_status != TX_STATUS_SUPR_NONE) {
			WLCNTCONDINCR(prec_cnt, prec_cnt->suppress);
			WLCNTCONDINCR(actq_cnt, actq_cnt->suppress);
		}
#endif /* PKTQ_LOG */

		if (tot_mpdu == 0) { /* first MPDU in a list of MPDUs */
			bool last_rate;
			d11actxh_rate_t *rev40_rinfo = NULL;
			uint16 PhyTxControlWord_0, PhyTxControlWord_1, PhyTxControlWord_2;
			uint8 *plcp;

#ifdef PKTQ_LOG
			prec_pktlen = txh_info->d11FrameSize - DOT11_LLC_SNAP_HDR_LEN -
			              DOT11_MAC_HDR_LEN - DOT11_QOS_LEN;
			/* Take a snapshot of headerlength */
			hdr_len = pkttotlen(wlc->osh, p) - prec_pktlen;
#endif // endif

#ifdef BCMDBG
			if (txs->frameid != htol16(txh_info->TxFrameID)) {
				WL_ERROR(("wl%d: %s: txs->frameid 0x%x txh->TxFrameID 0x%x p %p\n",
					wlc->pub->unit, __FUNCTION__,
					txs->frameid, htol16(txh_info->TxFrameID), p));
				ASSERT(txs->frameid == htol16(txh_info->TxFrameID));
			}
#endif // endif

			start_seq = seq;
			fix_rate = ltoh16(txh_info->MacTxControlHigh) & D11AC_TXC_FIX_RATE;

			if (D11REV_GE(wlc->pub->corerev, 128)) {
				ASSERT(rstore != NULL); /* should have been set before */
			} else {
				rev40_rinfo = (d11actxh_rate_t *)
					(txh_info->plcpPtr - OFFSETOF(d11actxh_rate_t, plcp[0]));
			}
			rnum = 0;

			/* this loop collects information for debug/tallies purposes. It
			 * loops over rates and is executed only for the first MPDU.
			 */
			do {
				if (D11REV_GE(wlc->pub->corerev, 128)) {
					uint entry = rnum;
					ASSERT(rstore != NULL);

					if (fix_rate) {
						d11txh_rev128_t *h = &txh_info->hdrPtr->rev128;
						entry = (ltoh16(h->RateMemIdxRateIdx) &
							D11_REV128_RATE_SPECRATEIDX_MASK) >>
							D11_REV128_RATE_SPECRATEIDX_SHIFT;
					}
					rs_txs.txrspec[rnum] =  rstore->rspec[entry];
					if (fix_rate || (rnum == (RATESEL_MFBR_NUM - 1)) ||
						(rstore->rspec[rnum + 1] == 0)) {
						last_rate = TRUE;
					} else {
						last_rate = FALSE;
					}
					ft[rnum] = WLC_RSPEC_ENC2FT(rstore->rspec[entry]);
					plcp = NULL; /* avoid compiler warning */
					PhyTxControlWord_0 = PhyTxControlWord_1 =
						PhyTxControlWord_2 = 0;
				} else {
					ASSERT(rev40_rinfo != NULL);

					PhyTxControlWord_0 = ltoh16
						(rev40_rinfo->PhyTxControlWord_0);
					PhyTxControlWord_1 = ltoh16
						(rev40_rinfo->PhyTxControlWord_1);
					PhyTxControlWord_2 = ltoh16
						(rev40_rinfo->PhyTxControlWord_2);
					plcp = rev40_rinfo->plcp;
					last_rate = (ltoh16(rev40_rinfo->RtsCtsControl) &
						D11AC_RTSCTS_LAST_RATE) ? TRUE : FALSE;

					ASSERT(plcp != NULL);
					BCM_REFERENCE(PhyTxControlWord_1);

					ft[rnum] = PhyTxControlWord_0 &
						D11_PHY_TXC_FT_MASK(wlc->pub->corerev);

					ft_fmt[rnum] = PhyTxControlWord_0 &
						D11_PHY_TXC_FTFMT_MASK(wlc->pub->corerev);

					if (wf_plcp_to_rspec(ft_fmt[rnum], plcp,
						&rs_txs.txrspec[rnum]) == FALSE) {
							WL_ERROR(("%s: invalid frametype: 0x%x\n",
								__FUNCTION__, ft[rnum]));
							ASSERT(0);
					}

					if (ft[rnum] == FT_HT) {
						/* Special handling for HT */
						rs_txs.txrspec[rnum] &= ~WL_RSPEC_HT_MCS_MASK;
						rs_txs.txrspec[rnum] |= HT_RSPEC(
							PhyTxControlWord_2 &
							D11AC_PHY_TXC_PHY_RATE_MASK);
#if defined(WLPROPRIETARY_11N_RATES)
						if (D11AC2_PHY_SUPPORT(wlc)) {
							if (PhyTxControlWord_2 &
								D11AC2_PHY_TXC_11N_PROP_MCS) {
								rs_txs.txrspec[rnum] |=
									(1 << HT_MCS_BIT6_SHIFT);
							}
						} else {
							if (PhyTxControlWord_1 &
								D11AC_PHY_TXC_11N_PROP_MCS) {
								rs_txs.txrspec[rnum] |=
									(1 << HT_MCS_BIT6_SHIFT);
							}
						}
#endif /* WLPROPRIETARY_11N_RATES */
					}
				}

#ifdef PKTQ_LOG
				if (D11REV_GE(wlc->pub->corerev, 128)) {
					mcs_txrate[rnum] =
						wf_rspec_to_rate(rs_txs.txrspec[rnum]) / 500;
				} else {
					mcs_txrate[rnum] = ltoh16(rev40_rinfo->TxRate);
				}
#endif /* PKT_QLOG */

				/* Next rate info */
				if (D11REV_GE(wlc->pub->corerev, 128)) {
					/* nothing to be done */
				} else {
					rev40_rinfo++;
				}

				rnum ++;
			} while (!last_rate && rnum < RATESEL_MFBR_NUM); /* loop over rates */

#if defined(WL_MU_TX)
			txs_mu = (txs->status.s5 & TX_STATUS64_MUTX) ? TRUE : FALSE;
#if defined(WLCNT)
			is_mu = ((ltoh16(txh_info->MacTxControlHigh) & D11AC_TXC_MU) != 0);
#endif /* WLCNT */
#endif /* WL_MU_TX */
		} /* tot_mpdu == 0 */
		else
		{
#ifndef BCMDBG
			tainted = FALSE;
#endif // endif
		}

		was_acked = wlc_txs_was_acked(wlc, &(txs->status), tot_mpdu);

#ifdef WL_CS_PKTRETRY
		if (cs_retry) { /* PSPRETEND related functionality */
			/* Retry packet */
			wlc_ampdu_cs_pktretry(scb, p, tid);

			/* Mark that packet retried */
			free_mpdu = FALSE;
			was_acked = FALSE;

			WLCNTINCR(ampdu_tx->cnt->cs_pktretry_cnt);
		}
#endif /* WL_CS_PKTRETRY */

#ifdef PSPRETEND
		/* pre-logic: we test if tx status has TX_STATUS_SUPR_PPS - if so, clear
		 * the ack status regardless
		 */
		if (PSPRETEND_ENAB(wlc->pub) && supr_status == TX_STATUS_SUPR_PPS) {
			was_acked = FALSE;
		}

		if ((BSSCFG_AP(bsscfg) || BSSCFG_IBSS(bsscfg)) &&
		    PS_PRETEND_ENABLED(bsscfg)) {
			if (was_acked) {
				pps_recvd_ack = TRUE;
			}

			/* if packet retried make it invisible for ps pretend
			 * the packet has to actually not have been sent ok (not acked)
			 */
			if (!was_acked &&
#ifdef WL_CS_PKTRETRY
				!cs_retry &&
#endif /* WL_CS_PKTRETRY */
				TRUE) {
				pps_retry = wlc_pspretend_pps_retry(wlc, scb, p, txs);
			} else {
				pps_retry = FALSE;
			}

			if (pps_retry) {
				tainted = wlc_pkt_get_txh_hdr(wlc, p, &txh);
				macCtlHigh = ltoh16(*D11_TXH_GET_MACHIGH_PTR(wlc, txh));
			} else {
				/* unset vhtHdr, macCtlHigh for next p, because we assume they are
				 * * only valid when pps_retry is TRUE
				 */
				txh = NULL;
				macCtlHigh = 0;
			}
		}
#endif /* PSPRETEND */

		if (!was_acked && (supr_status == TX_STATUS_SUPR_NONE) &&
#ifdef PSPRETEND
			!pps_retry &&
#endif /* PSPRETEND */
			TRUE) {
			nlost ++;
#ifdef WLC_SW_DIVERSITY
			if (WLSWDIV_ENAB(wlc) && WLSWDIV_BSSCFG_SUPPORT(bsscfg)) {
				wlc_swdiv_txfail(wlc->swdiv, scb);
			}
#endif /* WLC_SW_DIVERSITY */
		}

#ifdef PKTQ_LOG
		if (!was_acked && supr_status) {
			WLCNTCONDINCR(prec_cnt, prec_cnt->suppress);
			WLCNTCONDINCR(actq_cnt, actq_cnt->suppress);
		}
#endif // endif
#ifdef WLTAF
		if (!was_acked && supr_status) {
			wlc_taf_txpkt_status(wlc->taf_handle, scb, tid,  p,
				TAF_TXPKT_STATUS_SUPPRESSED);
		}
#endif // endif
#ifdef WL_TX_STALL
		if (!was_acked) {
			if (supr_status != TX_STATUS_SUPR_NONE) {
				WL_TX_STS_UPDATE(tx_status,
					wlc_tx_status_map_hw_to_sw_supr_code(wlc, supr_status));
			} else if (txs->phyerr) {
				WL_TX_STS_UPDATE(tx_status, WLC_TX_STS_PHY_ERROR);
			} else {
				WL_TX_STS_UPDATE(tx_status, WLC_TX_STS_RETRY_TIMEOUT);
			}

			wlc_tx_status_update_counters(wlc, p, scb, bsscfg, tx_status, 1);

			/* Clear status, next packet will have its own status */
			WL_TX_STS_UPDATE(tx_status, WLC_TX_STS_SUCCESS);
		}
#endif /* WL_TX_STALL */

		if (was_acked) { /* if current MPDU was acked by remote party */
			WL_AMPDU_TX(("wl%d: %s pkt ack seq 0x%04x idx %d\n",
				wlc->pub->unit, __FUNCTION__, seq, tot_mpdu));
			ini->acked_seq = seq;

			/* update the scb used time */
			scb->used = wlc->pub->now;

#ifdef PKTQ_LOG
			ack_cnt++;
			acked_pktlen += prec_pktlen;
#endif // endif
			/* for *each* acked MPDU within an AMPDU, call a packet callback */
			wlc_pcb_fn_invoke(wlc->pcb, p, TX_STATUS_ACK_RCV);

		}

#ifdef PSPRETEND
		if (pps_retry) {
			ASSERT(!was_acked);

			if (!supr_status) {
#ifdef PKTQ_LOG
				WLCNTCONDINCR(prec_cnt, prec_cnt->ps_retry);
				WLCNTCONDINCR(actq_cnt, actq_cnt->ps_retry);
#endif // endif
			}

			/* re-enqueue the packet suppressed due to ps pretend */
			free_mpdu = wlc_pspretend_pkt_retry(wlc->pps_info, bsscfg, scb, p,
			                                   txs, txh, macCtlHigh);

			/* unset txh, macCtlHigh for next p, because we assume they are
			 * only valid when pps_retry is TRUE
			 */
			txh = NULL;
			macCtlHigh = 0;
		}
#endif /* PSPRETEND */

		/* either retransmit or send bar if no ack received for this MPDU */
		if (!was_acked &&
#ifdef PSPRETEND
		    !pps_retry &&
#endif /* PSPRETEND */
#ifdef WL_CS_PKTRETRY
		    !cs_retry &&
#endif // endif
		    TRUE) {
			/* re-queues MPDU if applicable */
			if (retry_suppr_pkts) {
				tainted = TRUE;
				if (AP_ENAB(wlc->pub) && (supr_status == TX_STATUS_SUPR_PMQ)) {
					/* last_frag TRUE as fragmentation not allowed for AMPDU */
					free_mpdu = wlc_apps_suppr_frame_enq(wlc, p, txs, TRUE);
				} else if (AP_ENAB(wlc->pub) &&
					(supr_status == TX_STATUS_SUPR_TWT)) {

					free_mpdu = wlc_apps_suppr_twt_frame_enq(wlc, p);
				} else if (P2P_ABS_SUPR(wlc, supr_status) &&
					BSS_TX_SUPR_ENAB(bsscfg)) {

					/* This is possible if we got a packet suppression
					 * before getting ABS interrupt
					 */
					if (!BSS_TX_SUPR(bsscfg)) {
						wlc_bsscfg_tx_stop(wlc->psqi, bsscfg);
					}

					if (BSSCFG_AP(bsscfg) &&
					    SCB_ASSOCIATED(scb) && SCB_P2P(scb))
						wlc_apps_scb_tx_block(wlc, scb, 1, TRUE);
					/* With Proptxstatus enabled in dongle and host,
					 * pkts sent by host will not come here as retry is set
					 * to FALSE by wlc_should_retry_suppressed_pkt().
					 * With Proptxstatus disabled in dongle or
					 * not active in host, all the packets will come here
					 * and need to be re-enqueued.
					 */
					free_mpdu = wlc_pkt_abs_supr_enq(wlc, scb, p);
				}
				/* If the frame was not successfully enqueued for retry, handle
				 * the failure
				 */
				if (free_mpdu) {
					goto not_retry;
				}
			} else {
not_retry:		/* don't retry an unacked MPDU, but send BAR if applicable */
				bar_seq = seq;
#ifdef PROP_TXSTATUS
				if (PROP_TXSTATUS_ENAB(wlc->pub) &&
					(!WLFC_GET_REUSESEQ(wlfc_query_mode(wlc->wlfc)) ||
					TXS_SUPR_MAGG_DONE(supr_status)))
#endif /* PROP_TXSTATUS */
				{
					send_bar = TRUE;
				}

				WL_AMPDU_TX(("wl%d: %s: pkt seq 0x%04x not acked\n",
					wlc->pub->unit, __FUNCTION__, seq));

#ifdef PKTQ_LOG
				if (supr_status == TX_STATUS_SUPR_NONE) {
					WLCNTCONDINCR(prec_cnt, prec_cnt->retry_drop);
					WLCNTCONDINCR(actq_cnt, actq_cnt->retry_drop);
					SCB_BS_DATA_CONDINCR(bs_data_counters, retry_drop);
				}
#endif // endif
			} // if (retry_suppr_pkts)
		} /* if (not acked) */

#ifdef PROP_TXSTATUS
		if (PROP_TXSTATUS_ENAB(wlc->pub)) {
			void *cur;
			uint8 wlfc_status =  wlc_txstatus_interpret(&txs->status, was_acked);
			tainted = TRUE;
#ifdef PSPRETEND
			if (pps_retry && (wlfc_status == WLFC_CTL_PKTFLAG_DISCARD_NOACK))
				wlfc_status = WLFC_CTL_PKTFLAG_D11SUPPRESS;
#endif /* PSPRETEND */
			if (!was_acked && scb && (wlfc_status == WLFC_CTL_PKTFLAG_D11SUPPRESS)) {
				if (WLFC_CONTROL_SIGNALS_TO_HOST_ENAB(wlc->pub))
					wlc_suppress_sync_fsm(wlc, scb, p, TRUE);
				wlc_process_wlhdr_txstatus(wlc, wlfc_status, p, FALSE);
			} else if (wlfc_status) {
				wlfc_process_txstatus(wlc->wlfc, wlfc_status, p,
					txs, ((tot_mpdu < (ncons - 1)) ? TRUE : FALSE));
			}

			/* Hold pciedev_schedule_flow_ring_read_buffer for packets in
			 * the middle of ampdu.  Only the latest mpdu/msdu need to invoke
			 * pciedev_schedule_flow_ring_read_buffer.
			 */
			if (tot_mpdu < (ncons - 1)) {
				/* In the middle of ampdu */
				for (cur = p; cur; cur = PKTNEXT(wlc->osh, cur))
					PKTSETTXSHOLD(wlc->osh, cur);
			} else {
				/* Latest mpdu/msdu */
				for (cur = p; cur; cur = PKTNEXT(wlc->osh, cur)) {
					if (PKTNEXT(wlc->osh, cur))
						PKTSETTXSHOLD(wlc->osh, cur);
				}
			}
		}
#endif /* PROP_TXSTATUS */
free_and_next:

#ifdef WLWRR2
		{
			wlc_ampdu_wrr_update_rstat(&AMPDU_WRR_STATS(ini)->frmsz,
				pkttotlen(wlc->osh, p));
		}
#endif // endif

#ifdef WLSCB_HISTO
		if (was_acked) {
			succ_msdu += WLPKTTAG_AMSDU(p) ? amsdu_sf : 1;
			succ_mpdu++;
		}
#endif /* WLSCB_HISTO */

		if (free_mpdu) {
#if defined(WLPKTDLYSTAT) || defined(WL11K)
			/* calculate latency and packet loss statistics */
			/* Ignore wrap-around case and error case */
			if (now > pkttag->shared.enqtime) {
				delay = (now - pkttag->shared.enqtime);
#ifdef WLPKTDLYSTAT
				wlc_delay_stats_upd(delay_stats, delay, tr, was_acked);
				WL_AMPDU_STAT(("Enq %d retry %d cnt %d: acked %d delay/min/"
					"max/sum %d %d %d %d\n", pkttag->shared.enqtime,
					tr, delay_stats->txmpdu_cnt[tr], was_acked, delay,
					delay_stats->delay_min, delay_stats->delay_max,
					delay_stats->delay_sum[tr]));
#endif /* WLPKTDLYSTAT */
#ifdef WL11K
				if (was_acked) {
					wlc_rrm_delay_upd(wlc, scb, tid, delay);
				}
#endif /* WL11K */
			}
#endif /* WLPKTDLYSTAT || WL11K */
			if (supr_status == TX_STATUS_SUPR_AGG0) {
				if (D11REV_GE(wlc->pub->corerev, 128)) {
					/* This actually is TX_STATUS_SUPR_AGG0 */
					WL_ERROR(("wl%d %s: suppressed by continuous AGG0\n",
						wlc->pub->unit, __FUNCTION__));
				} else {
					WL_ERROR(("wl%d %s: acked %d undefined supr code %d\n",
						wlc->pub->unit, __FUNCTION__,
						was_acked, TX_STATUS_SUPR_AGG0));
				}
			}

			if (fastmode) {
				if (was_acked) {
					/*
					 * Chain the packets so we can free the whole
					 * bundle for fastmode processing.
					 */
					spktq_enq(&spq, p);
				} else {
					SPKTQFREE(&spq);
					spktq_deinit(&spq);
					PKTFREE(wlc->osh, p, TRUE);
					fastmode = FALSE;
				}
			} else {
#if defined(BCM_NBUFF_PKT) && defined(CC_BPM_SKB_POOL_BUILD)
				void *msdu; /* MSDUs are never accessed on TxStatus */
				for (msdu = PKTNEXT(wlc->osh, p); msdu != NULL;
					       msdu = PKTNEXT(wlc->osh, msdu)) {
					PKTDATAPRISTINE(wlc->osh, msdu);
				}
				if (!tainted) { /* Non first MPDU */
					PKTDATAPRISTINE(wlc->osh, p);
				}
#endif // endif
				PKTCNTR_INC(wlc, PKTCNTR_MPDU_TXS_AQMC_FREE, 1);
				PKTFREE(wlc->osh, p, TRUE);
			}
		} else {
			/* Packet requeued, update the counters */
			wlc_tx_status_update_counters(wlc, p,
				scb, bsscfg, WLC_TX_STS_QUEUED, 1);
		}

		tot_mpdu++;

		/* only loop for the #mpdus that the d11 core indicated in txs */
		if (tot_mpdu >= ncons) {
			break;
		}

		/* get next pkt from d11 dma/fifo queue -- check and handle NULL */

#ifdef BULK_PKTLIST
		if (ncons > 1) {
			BULK_GETNEXTTXP(list_head, p);
		}
#else
		p = GETNEXTTXP(wlc, queue);
#endif // endif

#ifdef BCMDBG
		if (ini->tid != PKTPRIO(p)) {
			WL_ERROR(("%s tid mismatch ini tid:%d p prio:%d queue:%d\n",
				__FUNCTION__, ini->tid, PKTPRIO(p), queue));
			ASSERT(ini->tid == PKTPRIO(p));
		}
#endif // endif

		/* if tot_mpdu < ncons, should have pkt in queue */
		if (p == NULL) {
			WL_AMPDU_ERR(("%s: p is NULL. tot_mpdu: %d, ncons: %d\n",
				__FUNCTION__, tot_mpdu, ncons));
			ASSERT(p);
			break;
		}

		/* validate scb pointer */
		scb1 = WLPKTTAGSCBGET(p);
		if (scb1 != scb) {
			wlc_txh_info_t txh_info1;
			int8 *tsoHdrPtr, *tsoHdrPtr1;

			wlc_get_txh_info(wlc, p, &txh_info1);
			tsoHdrPtr = txh_info->tsoHdrPtr;
			tsoHdrPtr1 = txh_info1.tsoHdrPtr;
			WL_ERROR(("%s:scb 0x%p fid 0x%04x epoch 0x%x scb1 0x%p "
				"fid1 0x%04x epoch1 0x%x, txh_info1 0x%p ncons (%d/%d)\n",
				__FUNCTION__, scb, txh_info->TxFrameID,
				((tsoHdrPtr[2] & TOE_F2_EPOCH_MASK) >> TOE_F2_EPOCH_SHIFT),
				scb1, txh_info1.TxFrameID,
				((tsoHdrPtr1[2] & TOE_F2_EPOCH_MASK) >> TOE_F2_EPOCH_SHIFT),
				&txh_info1, tot_mpdu+1, ncons));
			BCM_REFERENCE(tsoHdrPtr);
			BCM_REFERENCE(tsoHdrPtr1);
			ASSERT(0);
			free_mpdu = TRUE;
			goto free_and_next;
		}

#ifdef PKTQ_LOG
		if (prec_cnt) {
			/* hdrlength should be same across all MPDUs */
			prec_pktlen = pkttotlen(wlc->osh, p) - hdr_len;
#ifdef BCMDBG
			{
				/* get info from next header to give accurate packet length */
				int tsoHdrSize = wlc_pkt_get_txh_hdr(wlc, p, &txh);
				int hdrSize;
				uint16 *maclow = D11_TXH_GET_MACLOW_PTR(wlc, txh);

				if (*maclow & htol16(D11AC_TXC_HDR_FMT_SHORT)) {
					hdrSize = D11_TXH_SHORT_LEN(wlc);
				} else {
					hdrSize = D11_TXH_LEN_EX(wlc);
				}

				ASSERT(hdr_len == (tsoHdrSize + hdrSize + DOT11_LLC_SNAP_HDR_LEN +
					DOT11_MAC_HDR_LEN + DOT11_QOS_LEN));
				BCM_REFERENCE(tsoHdrSize);

				txh = NULL;
			}
#endif /* BCMDBG */
		}
#endif /* PKTQ_LOG */
	} /* while (TRUE) -> for each AMPDU in caller supplied txs */
#ifdef WLATF
	scb_ampdu->tot_pkt += tot_mpdu;
#endif // endif
#ifdef WLTAF
	wlc_taf_txpkt_status(wlc->taf_handle, scb, tid, TAF_PARAM(tot_mpdu),
		TAF_TXPKT_STATUS_UPDATE_PACKET_COUNT);
#endif // endif
#ifdef PKTQ_LOG
	if (ack_cnt) {
		WLCNTCONDADD(prec_cnt, prec_cnt->acked, ack_cnt);
		WLCNTCONDADD(actq_cnt, actq_cnt->acked, ack_cnt);
		SCB_BS_DATA_CONDADD(bs_data_counters, acked, ack_cnt);
		WLCNTCONDADD(prec_cnt, prec_cnt->throughput, acked_pktlen);
		WLCNTCONDADD(actq_cnt, actq_cnt->throughput, acked_pktlen);
		SCB_BS_DATA_CONDADD(bs_data_counters, throughput, acked_pktlen);
		WLCNTSCBADD(scb_ampdu->ampdu_scb_stats->tx_bytes_total[ini->tid],
			acked_pktlen);
	}
#endif /* PKTQ_LOG */
	if (fastmode) {
		SPKTQFREE(&spq);
		spktq_deinit(&spq);
	}

	/* In case of RATELINKMEM, update rspec history with used rate now */
	if (RATELINKMEM_ENAB(wlc->pub)) {
		bsscfg->txrspec[bsscfg->txrspecidx][0] = rs_txs.txrspec[0]; /* rspec */
		bsscfg->txrspec[bsscfg->txrspecidx][1] = tot_mpdu; /* nfrags */
		bsscfg->txrspecidx = (bsscfg->txrspecidx + 1) % NTXRATE;
		/* Update scb stats */
		WLCNTSCBSET(scb->scb_stats.tx_rate, rs_txs.txrspec[0]);
	}
	wlc_txfifo_complete(wlc, queue, tot_mpdu);

#ifdef PKTQ_LOG
	/* cannot distinguish hi or lo prec hereafter - so just standardise lo prec */
	if (scb_ampdu->txq.pktqlog) {
		prec_cnt = wlc_ampdu_pktqlog_cnt(&scb_ampdu->txq, tid, WLC_PRIO_TO_PREC(tid));
	}

	/* we are in 'aqm' complete function so expect at least corerev 40 to get this */
	WLCNTCONDADD(prec_cnt, prec_cnt->airtime, TX_STATUS40_TX_MEDIUM_DELAY(txs));
#endif // endif

#ifdef WLWRR
	if (D11REV_GE(wlc->pub->corerev, 40)) {
		uint txtime = TX_STATUS40_TX_MEDIUM_DELAY(txs);
		if (txtime != (uint)0xFFFF) {
			wlc_ampdu_wrr_update_rstat(&AMPDU_WRR_STATS(ini)->txdur, txtime);
		}
	}
#endif // endif

	if (send_bar && (ini->retry_bar != AMPDU_R_BAR_BLOCKED)) {
		bar_seq = MODINC_POW2(seq, SEQNUM_MAX);
		wlc_ampdu_send_bar(ampdu_tx, ini, bar_seq);
	}

#if defined(BCMDBG) || (defined(WLTEST) && !defined(WLTEST_DISABLED)) || \
	defined(BCMDBG_AMPDU) || defined(WL_LINKSTAT)
	if (nlost) {
		AMPDUSCBCNTADD(scb_ampdu->cnt.txlost, nlost);
		WLCNTADD(ampdu_tx->cnt->txlost, nlost);
		WLCNTADD(wlc->pub->_cnt->txlost, nlost);
		WL_AMPDU_TX(("wl%d aqm_txs: nlost %d send_bar %d "
			"ft %d mcs[0-3] %04x %04x %04x %04x\n",
			wlc->pub->unit, nlost, send_bar,
			ft[0], rs_txs.txrspec[0], rs_txs.txrspec[1],
			rs_txs.txrspec[2], rs_txs.txrspec[3]));
		WL_AMPDU_TX(("raw txstatus %04X %04X %04X | s3-5 %08X %08X %08X | "
			"%08X %08X | s8 %08X\n",
			txs->status.raw_bits, txs->frameid, txs->sequence,
			txs->status.s3, txs->status.s4, txs->status.s5,
			txs->status.ack_map1, txs->status.ack_map2, txs->status.s8));
		if (send_bar) {
			WL_AMPDU_CTL(("wl%d: %s pkt ack_seq 0x%x bar_seq 0x%04x start 0x%x"
				" max 0x%x\n",
				wlc->pub->unit,	__FUNCTION__, ini->acked_seq,
				bar_seq, ini->start_seq, ini->max_seq));
		}
	}
#endif /* BCMDBG || WLTEST !WLTEST_DISABLED || BCMDBG_AMPDU || WL_LINKSTAT */

#ifdef PSPRETEND
	if ((BSSCFG_AP(bsscfg) || BSSCFG_IBSS(bsscfg)) &&
	    PS_PRETEND_ENABLED(bsscfg)) {
		wlc_pspretend_dotxstatus(wlc->pps_info, bsscfg, scb, pps_recvd_ack);
	}
#endif /* PSPRETEND */

	/* bump up the start seq to move the window */
	wlc_ampdu_ini_move_window(ampdu_tx, scb_ampdu, ini);

	WL_AMPDU_TX(("wl%d: %s: ncons %d tot_mpdus %d start_seq 0x%04x\n",
		wlc->pub->unit, __FUNCTION__, ncons, tot_mpdu, start_seq));

#ifdef STA
	/* PM state change */
	wlc_update_pmstate(bsscfg, (uint)(wlc_txs_alias_to_old_fmt(wlc, &(txs->status))));
#endif /* STA */

#ifdef PKTQ_LOG
	WLCNTCONDADD(prec_cnt, prec_cnt->rtsfail, txs->status.rts_tx_cnt - txs->status.cts_rx_cnt);
	WLCNTCONDADD(actq_cnt, actq_cnt->rtsfail, txs->status.rts_tx_cnt - txs->status.cts_rx_cnt);
	SCB_BS_DATA_CONDADD(bs_data_counters,
		rtsfail, txs->status.rts_tx_cnt - txs->status.cts_rx_cnt);
#endif // endif
	if (nlost) {
		/* Per-scb statistics: scb must be valid here */
		WLCNTADD(wlc->pub->_cnt->txfail, nlost);
		if (!from_host) {
			WLCNTSCBADD(scb->scb_stats.tx_pkts_fw_retry_exhausted, nlost);
		} else {
			WLCNTSCBADD(scb->scb_stats.tx_pkts_retry_exhausted, nlost);
		}
		WLCIFCNTADD(scb, txfail, nlost);
		WLCNTSCBADD(scb->scb_stats.tx_failures, nlost);
#ifdef WL11K
		wlc_rrm_tscm_upd(wlc, scb, tid, OFFSETOF(rrm_tscm_t, msdu_fail), nlost);
#endif // endif
#if defined(STA) && defined(WL_TX_STALL) && defined(WL_TX_CONT_FAILURE)
		if (wlc->tx_stall->failure_to_rpt && BSSCFG_INFRA_STA(bsscfg)) {
			wlc_txfail_cnt_inc(wlc, TRUE, tx_rspec);
		}
	} else if (wlc->tx_stall->error->txfail_consecutive_total && BSSCFG_INFRA_STA(bsscfg)) {
		wlc_txfail_cnt_reset(wlc);
#endif /* STA && WL_TX_STALL && WL_TX_CONT_FAILURE */
	}

	/* Update rs_txs except for txrspec which is already updated */
	wlc_ampdu_fill_rs_txs(&rs_txs, txs, ac, ncons, nlost);

	/* MU txstatus uses RT1~RT3 for other purposes */
	if (fix_rate && !txs_mu) {
		/* if using fix rate, retrying 64 mpdus >=4 times can overflow 8-bit cnt.
		 * So ucode treats fix rate specially.
		 */
		uint32    retry_count;
#ifdef PKTQ_LOG
		uint32    txrate_succ = 0;
#endif // endif
		rs_txs.tx_cnt[0]     = txs->status.s3 & 0xffff;
		rs_txs.txsucc_cnt[0] = (txs->status.s3 >> 16) & 0xffff;
		retry_count = rs_txs.tx_cnt[0] - rs_txs.txsucc_cnt[0];
		BCM_REFERENCE(retry_count);

		WLCNTADD(wlc->pub->_cnt->txfrag, rs_txs.txsucc_cnt[0]);
		WLCNTADD(wlc->pub->_cnt->txfrmsnt, rs_txs.txsucc_cnt[0]);
		WLCNTADD(wlc->pub->_cnt->txretrans, retry_count);
		if (!from_host) {
			WLCNTSCBADD(scb->scb_stats.tx_pkts_fw_total, rs_txs.txsucc_cnt[0]);
			WLCNTSCBADD(scb->scb_stats.tx_pkts_fw_retries, retry_count);
		} else {
			WLCNTSCBADD(scb->scb_stats.tx_pkts_total, rs_txs.txsucc_cnt[0]);
			WLCNTSCBADD(scb->scb_stats.tx_pkts_retries, retry_count);
		}
#ifdef WL11K
		wlc_rrm_tscm_upd(wlc, scb, tid,
			OFFSETOF(rrm_tscm_t, msdu_tx), rs_txs.txsucc_cnt[0]);
#endif // endif

		WLCNTSCBADD(scb_ampdu->ampdu_scb_stats->tx_pkts_total[ini->tid],
			rs_txs.txsucc_cnt[0]);
		WLCIFCNTADD(scb, txfrmsnt, rs_txs.txsucc_cnt[0]);
#ifdef PKTQ_LOG
		if (rs_txs.txsucc_cnt[0]) {
			txrate_succ = mcs_txrate[0] * rs_txs.txsucc_cnt[0];
		}

		WLCNTCONDADD(prec_cnt, prec_cnt->retry, retry_count);
		WLCNTCONDADD(actq_cnt, actq_cnt->retry, retry_count);
		SCB_BS_DATA_CONDADD(bs_data_counters, retry, retry_count);
		WLCNTCONDADD(prec_cnt, prec_cnt->txrate_main, mcs_txrate[0] * rs_txs.tx_cnt[0]);
		WLCNTCONDADD(actq_cnt, actq_cnt->txrate_main, mcs_txrate[0] * rs_txs.tx_cnt[0]);
		SCB_BS_DATA_CONDADD(bs_data_counters, txrate_main,
			mcs_txrate[0] * rs_txs.tx_cnt[0]);
		WLCNTCONDADD(prec_cnt, prec_cnt->txrate_succ, txrate_succ);
		WLCNTCONDADD(actq_cnt, actq_cnt->txrate_succ, txrate_succ);
		SCB_BS_DATA_CONDADD(bs_data_counters, txrate_succ, txrate_succ);
#endif // endif
#ifdef WLATF
		scb_ampdu->txretry_pkt += retry_count;
#endif // endif
#ifdef WLTAF
		wlc_taf_txpkt_status(wlc->taf_handle, scb, tid, TAF_PARAM(retry_count),
			TAF_TXPKT_STATUS_UPDATE_RETRY_COUNT);
#endif // endif
#ifdef WLSCB_HISTO
		WLSCB_HISTO_TX_IF_HTVHT(scb, rs_txs.txrspec[0], succ_msdu);
#endif /* WLSCB_HISTO */
	} else {
		uint32    succ_count;
		uint32    try_count;
		uint32    retry_count;
#ifdef PKTQ_LOG
		uint32    txrate_succ = 0;
#endif // endif
		uint8 rtidx = 0;

		txs_mutype = TX_STATUS_MUTYP(wlc->pub->corerev, txs->status.s5);
		if (txs_mu) {
			if (txs_mutype == TX_STATUS_MUTP_HEOM) {
				rtidx = TX_STATUS128_HEOM_RTIDX(txs->status.s4);
				ASSERT(rtidx < RATESEL_MFBR_NUM);
#if defined(BCMDBG) || defined(DL_RU_STATS_DUMP)
				wlc_musched_upd_ru_stats(wlc->musched, scb, txs);
#endif // endif
			} else if (txs_mutype == TX_STATUS_MUTP_VHTMU) {
				/* vhtmu does not need to update ratesel */
				update_ratesel = FALSE;
			}
		} else {
			rs_txs.tx_cnt[1]     = (txs->status.s3 >> 16) & 0xff;
			rs_txs.txsucc_cnt[1] = (txs->status.s3 >> 24) & 0xff;
			rs_txs.tx_cnt[2]     = (txs->status.s4 >>  0) & 0xff;
			rs_txs.txsucc_cnt[2] = (txs->status.s4 >>  8) & 0xff;
			rs_txs.tx_cnt[3]     = (txs->status.s4 >> 16) & 0xff;
			rs_txs.txsucc_cnt[3] = (txs->status.s4 >> 24) & 0xff;
		}

		rs_txs.tx_cnt[rtidx]     = txs->status.s3 & 0xff;
		rs_txs.txsucc_cnt[rtidx] = (txs->status.s3 >> 8) & 0xff;

		succ_count = rs_txs.txsucc_cnt[0] + rs_txs.txsucc_cnt[1] +
			rs_txs.txsucc_cnt[2] + rs_txs.txsucc_cnt[3];
		try_count = rs_txs.tx_cnt[0] + rs_txs.tx_cnt[1] +
			rs_txs.tx_cnt[2] + rs_txs.tx_cnt[3];
		retry_count = try_count - succ_count;
		BCM_REFERENCE(succ_count);
		BCM_REFERENCE(try_count);
		BCM_REFERENCE(retry_count);

		WLCNTADD(wlc->pub->_cnt->txfrag, succ_count);
		WLCNTADD(wlc->pub->_cnt->txfrmsnt, succ_count);
		WLCNTADD(wlc->pub->_cnt->txretrans, retry_count);
		if (!from_host) {
			WLCNTSCBADD(scb->scb_stats.tx_pkts_fw_total, succ_count);
			WLCNTSCBADD(scb->scb_stats.tx_pkts_fw_retries, retry_count);
		} else {
			WLCNTSCBADD(scb->scb_stats.tx_pkts_total, succ_count);
			WLCNTSCBADD(scb->scb_stats.tx_pkts_retries, retry_count);
		}
#ifdef WL11K
		wlc_rrm_tscm_upd(wlc, scb, tid, OFFSETOF(rrm_tscm_t, msdu_tx), succ_count);
#endif // endif

		WLCNTSCBADD(scb_ampdu->ampdu_scb_stats->tx_pkts_total[ini->tid], succ_count);
		WLCIFCNTADD(scb, txfrmsnt, succ_count);

#ifdef PKTQ_LOG
		if (succ_count) {
			txrate_succ = mcs_txrate[0] * rs_txs.txsucc_cnt[0] +
			              mcs_txrate[1] * rs_txs.txsucc_cnt[1] +
			              mcs_txrate[2] * rs_txs.txsucc_cnt[2] +
			              mcs_txrate[3] * rs_txs.txsucc_cnt[3];
		}

		WLCNTCONDADD(prec_cnt, prec_cnt->retry, retry_count);
		WLCNTCONDADD(actq_cnt, actq_cnt->retry, retry_count);
		SCB_BS_DATA_CONDADD(bs_data_counters, retry, retry_count);
		WLCNTCONDADD(prec_cnt, prec_cnt->txrate_main, mcs_txrate[0] * try_count);
		WLCNTCONDADD(actq_cnt, actq_cnt->txrate_main, mcs_txrate[0] * try_count);
		SCB_BS_DATA_CONDADD(bs_data_counters, txrate_main, mcs_txrate[0] * try_count);
		WLCNTCONDADD(prec_cnt, prec_cnt->txrate_succ, txrate_succ);
		WLCNTCONDADD(actq_cnt, actq_cnt->txrate_succ, txrate_succ);
		SCB_BS_DATA_CONDADD(bs_data_counters, txrate_succ, txrate_succ);
#endif /* PKTQ_LOG */
#ifdef WLATF
		scb_ampdu->txretry_pkt += retry_count;
#endif // endif
#ifdef WLTAF
		wlc_taf_txpkt_status(wlc->taf_handle, scb, tid, TAF_PARAM(retry_count),
			TAF_TXPKT_STATUS_UPDATE_RETRY_COUNT);
#endif // endif
#ifdef WLSCB_HISTO
		wlc_update_rate_histogram(scb,  &rs_txs, succ_mpdu, succ_msdu, amsdu_sf);
#endif /* WLSCB_HISTO */
	}
#ifdef BCMDBG
	for (k = 0; k < rnum; k++) {
		if (rs_txs.txsucc_cnt[k] > rs_txs.tx_cnt[k]) {
			WL_AMPDU_ERR(("%s: rs_txs.txsucc_cnt[%d] > rs_txs.tx_cnt[%d]: %d > %d\n",
				__FUNCTION__, k, k, rs_txs.txsucc_cnt[k], rs_txs.tx_cnt[k]));
			WL_AMPDU_ERR(("ncons %d raw txstatus %08X | %04X %04X | %08X %08X || "
				 "%08X | %08X %08X\n", ncons,
				  txs->status.raw_bits, txs->sequence, txs->phyerr,
				  txs->status.s3, txs->status.s4, txs->status.s5,
				  txs->status.ack_map1, txs->status.ack_map2));
		}
	}
#endif // endif

#if defined(BCMDBG) || (defined(WLTEST) && !defined(WLTEST_DISABLED)) || \
	defined(BCMDBG_AMPDU) || defined(WL_LINKSTAT)

	ASSERT(ampdu_tx->amdbg != NULL);

	for (k = 0; k < rnum; k++) {
		uint gi, mcsidx, mcs, nss, rateidx;
		uint8 ampdu_ft;

		BCM_REFERENCE(gi);
		BCM_REFERENCE(mcs);
		BCM_REFERENCE(nss);
		BCM_REFERENCE(rateidx);

		switch (ft[k]) {
#ifdef WL11AX
		case FT_HE:
			mcs = rs_txs.txrspec[k] & WL_RSPEC_HE_MCS_MASK;
			nss = (rs_txs.txrspec[k] & WL_RSPEC_HE_NSS_MASK) >> WL_RSPEC_HE_NSS_SHIFT;
			rateidx = (nss - 1) * MAX_HE_RATES + mcs;

			if (txs_mu && txs_mutype == TX_STATUS_MUTP_HEOM) {
				/* XXX: For DL OFDMA, gi used for tx may not be the same as that
				 * in ratemem. Use the gi from tx status diretcly.
				 */
				gi = TX_STATUS128_HEOM_CPLTF(txs->status.s4);
			} else {
				gi = RSPEC_HE_LTF_GI(rs_txs.txrspec[k]);
			}

			ASSERT(gi < AMPDU_MAX_HE_GI);
			ASSERT(rateidx < AMPDU_MAX_HE);
			WLCNTADD(ampdu_tx->amdbg->txhe[gi][rateidx], rs_txs.tx_cnt[k]);
			WLCNTADD(ampdu_tx->amdbg->txhe_succ[gi][rateidx], rs_txs.txsucc_cnt[k]);
			if (rs_txs.txrspec[k] & WL_RSPEC_STBC) {
				WLCNTADD(ampdu_tx->cnt->u2.txmpdu_stbc, rs_txs.tx_cnt[k]);
				WLCNTADD(ampdu_tx->amdbg->txhestbc[rateidx], rs_txs.tx_cnt[k]);
			}
			break;
#endif /* WL11AX */
#ifdef WL11AC
		case FT_VHT:
			mcs = rs_txs.txrspec[k] & WL_RSPEC_VHT_MCS_MASK;
			nss = (rs_txs.txrspec[k] & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT;
			rateidx = (nss - 1) * MAX_VHT_RATES + mcs;

#ifdef WLPKTDLYSTAT
			WLCNTADD(ampdu_tx->cnt->txmpdu_vht_cnt[rateidx], rs_txs.tx_cnt[k]);
			WLCNTADD(ampdu_tx->cnt->txmpdu_vht_succ_cnt[rateidx], rs_txs.txsucc_cnt[k]);
#endif /* WLPKTDLYSTAT */
			WLCNTADD(ampdu_tx->amdbg->txvht[rateidx], rs_txs.tx_cnt[k]);
			WLCNTADD(ampdu_tx->amdbg->txvht_succ[rateidx], rs_txs.txsucc_cnt[k]);
			if (rs_txs.txrspec[k] & WL_RSPEC_SGI) {
				WLCNTADD(ampdu_tx->cnt->u1.txmpdu_sgi, rs_txs.tx_cnt[k]);
				WLCNTADD(ampdu_tx->amdbg->txvhtsgi[rateidx], rs_txs.tx_cnt[k]);
			}
			if (rs_txs.txrspec[k] & WL_RSPEC_STBC) {
				WLCNTADD(ampdu_tx->cnt->u2.txmpdu_stbc, rs_txs.tx_cnt[k]);
				WLCNTADD(ampdu_tx->amdbg->txvhtstbc[rateidx], rs_txs.tx_cnt[k]);
			}
			break;

#endif /* WL11AC */
		case FT_HT:
			mcsidx = MCS2IDX(rs_txs.txrspec[k] & WL_RSPEC_HT_MCS_MASK);
			ASSERT(mcsidx < AMPDU_HT_MCS_ARRAY_SIZE);
#ifdef WLPKTDLYSTAT
			WLCNTADD(ampdu_tx->cnt->txmpdu_cnt[mcsidx], rs_txs.tx_cnt[k]);
			WLCNTADD(ampdu_tx->cnt->txmpdu_succ_cnt[mcsidx], rs_txs.txsucc_cnt[k]);
#endif /* WLPKTDLYSTAT */
			WLCNTADD(ampdu_tx->amdbg->txmcs[mcsidx], rs_txs.tx_cnt[k]);
			WLCNTADD(ampdu_tx->amdbg->txmcs_succ[mcsidx], rs_txs.txsucc_cnt[k]);
			if (rs_txs.txrspec[k] & WL_RSPEC_SGI) {
				WLCNTADD(ampdu_tx->cnt->u1.txmpdu_sgi, rs_txs.tx_cnt[k]);
				WLCNTADD(ampdu_tx->amdbg->txmcssgi[mcsidx], rs_txs.tx_cnt[k]);
			}
			if (rs_txs.txrspec[k] & WL_RSPEC_STBC) {
				WLCNTADD(ampdu_tx->cnt->u2.txmpdu_stbc, rs_txs.tx_cnt[k]);
				WLCNTADD(ampdu_tx->amdbg->txmcsstbc[mcsidx],
					rs_txs.tx_cnt[k]);
			}
			break;

		default:
			break;
		}

		ampdu_ft = AMPDU_PPDU_FT(txs_mu, txs_mutype);
		ASSERT(ampdu_ft < AMPDU_PPDU_FT_MAX);
		WLCNTADD(ampdu_tx->amdbg->txmpdu[ampdu_ft], rs_txs.tx_cnt[k]);
		WLCNTADD(ampdu_tx->amdbg->txmpdu_succ[ampdu_ft], rs_txs.txsucc_cnt[k]);
	}
#endif /* BCMDBG || WLTEST || BCMDBG_AMPDU || WL_LINKSTAT */

#if defined(WL_MU_TX) && defined(WLCNT)
	if (((MU_TX_ENAB(wlc) && SCB_VHTMU(scb)) || (HE_MMU_ENAB(wlc->pub) && (SCB_HEMMU(scb)))) &&
		is_mu && (supr_status == TX_STATUS_SUPR_NONE) && (txs->phyerr == 0)) {
		wlc_mutx_update_txcounters(wlc->mutx, scb,
			txs_mu, txs, &rs_txs, rnum);
	}
#endif /* WL_MU_TX && WLCNT */

	wlc_macdbg_dtrace_log_txs(wlc->macdbg, scb, &rs_txs.txrspec[0], txs);
	if (update_ratesel) {
		uint fbr_cnt = 0, fbr_succ = 0;

		if (rs_txs.tx_cnt[0] > 0xff) rs_txs.tx_cnt[0] = 0xff;
		if (rs_txs.txsucc_cnt[0] > 0xff) rs_txs.txsucc_cnt[0] = 0xff;

		for (k = 1; k < rnum; k++) {
			fbr_cnt += rs_txs.tx_cnt[k];
			fbr_succ += rs_txs.txsucc_cnt[k];
		}

		if (rs_txs.tx_cnt[0] == 0 && fbr_cnt == 0) {
			/*
			 * XXX 4360 TO DO: the frames must've failed because of rts failure
			 * 1. we could feedback rate sel <rts_txcnt> and <cts_rxcnt>.
			 * 2. once we put 4 rates in, we should also put tx_fbr2/tx_fbr3 in.
			 */
			rs_txs.tx_cnt[0] = (ncons > 0xff) ? 0xff : (uint8)ncons;
			rs_txs.txsucc_cnt[0] = 0;
			rs_txs.tx_cnt[1] = 0;
			rs_txs.txsucc_cnt[1] = 0;
#ifdef PSPRETEND
			if (pps_retry) {
				WL_PS(("wl%d.%d: "MACF" pps retry with only RTS failure\n",
				        wlc->pub->unit,
				        WLC_BSSCFG_IDX(bsscfg), ETHER_TO_MACF(scb->ea)));
			}
#endif /* PSPRETEND */
		} else {
#ifdef PSPRETEND
			/* we have done successive attempts to send the packet and it is
			 * not working, despite RTS going through. Suspect that the rate
			 * info is wrong so reset it
			 */
			/* XXX if we were reprogramming tx header between ps pretend transmissions,
			 * we would do this rate reset before reaching the retry limit
			 */
			if (SCB_PS_PRETEND(scb) && !SCB_PS_PRETEND_THRESHOLD(scb)) {
				wlc_pspretend_rate_reset(wlc->pps_info, bsscfg, scb,
					WME_PRIO2AC(tid));
			}
#endif /* PSPRETEND */
		}

		/* notify rate selection */
		wlc_scb_ratesel_upd_txs_ampdu(wlc->wrsi, scb, &rs_txs, txs, 0);
	} else {
		if (txs_mu && (txs_mutype == TX_STATUS_MUTP_HEOM)) {
			wlc_scb_ratesel_upd_txs_ampdu(wlc->wrsi, scb, &rs_txs, txs,
				RATESEL_UPDFLAG_NORTSELUPD);
		}
	}

	if (!supr_status) {
		/* if primary succeeds, no restrict */
		wlc_scb_restrict_txstatus(scb, rs_txs.txsucc_cnt[0] != 0);
	}
} /* wlc_ampdu_dotxstatus_aqm_complete */

#ifdef AMPDU_NON_AQM

/** only called in case of host aggregation */
static void BCMFASTPATH
wlc_ampdu_dotxstatus_complete_noaqm(ampdu_tx_info_t *ampdu_tx, struct scb *scb, void *p,
                                    tx_status_t *txs, uint32 s1, uint32 s2)
{
	scb_ampdu_tx_t *scb_ampdu;
	wlc_info_t *wlc = ampdu_tx->wlc;
	ampdu_tx_config_t *ampdu_tx_cfg = ampdu_tx->config;
	scb_ampdu_tid_ini_t *ini;
	scb_ampdu_tid_ini_non_aqm_t *non_aqm;
	uint8 bitmap[8], queue, tid, requeue = 0;
	d11txh_pre40_t *txh = NULL;
	uint8 *plcp = NULL;
	struct dot11_header *h;
	uint16 seq, start_seq = 0, bindex, indx, mcl;
	uint8 mcs = 0;
	bool is_sgi = FALSE, is40 = FALSE;
	bool ba_recd = FALSE, ack_recd = FALSE, send_bar = FALSE;
	uint8 suc_mpdu = 0, tot_mpdu = 0;
	uint supr_status;
	bool update_rate = TRUE, retry = TRUE;
	int txretry = -1;
	uint16 mimoantsel = 0;
	uint8 antselid = 0;
	uint8 retry_limit, rr_retry_limit;
	wlc_bsscfg_t *bsscfg;
	bool pkt_freed = FALSE;
#if defined(WLPKTDLYSTAT) || defined(PKTQ_LOG)
	uint8 mcs_fbr = 0;
#endif /* WLPKTDLYSTAT || PKTQ_LOG */
	uint8 ac;
#if defined(WLPKTDLYSTAT) || defined(WL11K)
	uint32 delay, now;
#endif // endif
#ifdef WLPKTDLYSTAT
	scb_delay_stats_t *delay_stats;
	uint tr;
#endif /* WLPKTDLYSTAT */
#ifdef PKTQ_LOG
	int prec_index;

	pktq_counters_t *prec_cnt = NULL;
	pktq_counters_t *actq_cnt = NULL;
	uint32           prec_pktlen;
	uint32 tot_len = 0, pktlen = 0;
	uint32 txrate_succ = 0, current_rate, airtime = 0;
	uint bw;
	ratespec_t rspec;
	uint ctl_rspec;
	uint flg;
	uint32 rts_sifs, cts_sifs, ba_sifs;
	uint32 ampdu_dur;
#endif /* PKTQ_LOG */
#if defined(SCB_BS_DATA)
	wlc_bs_data_counters_t *bs_data_counters = NULL;
#endif /* SCB_BS_DATA */
	int pdu_count = 0;
	uint16 first_frameid = 0xffff;
	uint16 ncons = 0; /**< number of consumed packets */
	ratesel_txs_t rs_txs;

#ifdef PROP_TXSTATUS
	uint8 wlfc_suppr_acked_status = 0;
#endif /* PROP_TXSTATUS */
#ifdef WL_TX_STALL
	wlc_tx_status_t tx_status = WLC_TX_STS_SUCCESS;
#endif	/* WL_TX_STALL */

#if defined(BCMDBG) || defined(BCMDBG_AMPDU) || defined(WLTEST)
	uint8 hole[AMPDU_MAX_MPDU];
	uint8 idx = 0;
	memset(hole, 0, sizeof(hole));
#endif /* defined(BCMDBG) || defined(BCMDBG_AMPDU) || defined(WLTEST) */

	bzero(&rs_txs, sizeof(rs_txs));

	ASSERT(wlc);
	ASSERT(!RATELINKMEM_ENAB(wlc->pub));

	if (p != NULL && (WLPKTTAG(p)->flags & WLF_AMPDU_MPDU) == 0) {
		WL_PRINT(("%s flags=%x seq=%d s1=%d s2=%d\n", __FUNCTION__,
			WLPKTTAG(p)->flags, WLPKTTAG(p)->seq, s1, s2));
	}

	ASSERT(p && (WLPKTTAG(p)->flags & WLF_AMPDU_MPDU));

	if (!scb || !SCB_AMPDU(scb)) {
		if (!scb) {
			WL_AMPDU_ERR(("wl%d: %s: scb is null\n",
				wlc->pub->unit, __FUNCTION__));
			/* pkttag must have been set null in wlc_dotxstatus */
			ASSERT(WLPKTTAGSCBGET(p) == NULL);
		} else {
			WLPKTTAGSCBSET(p, NULL);
		}
		wlc_ampdu_free_chain(ampdu_tx, p, txs);
		return;
	}

	SCB_BS_DATA_CONDFIND(bs_data_counters, wlc, scb);

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	ASSERT(scb_ampdu);

	tid = (uint8)PKTPRIO(p);
	ac = WME_PRIO2AC(tid);
#ifdef WLPKTDLYSTAT
	delay_stats = scb->delay_stats + ac;
#endif /* WLPKTDLYSTAT */
	ini = scb_ampdu->ini[tid];
	retry_limit = ampdu_tx_cfg->retry_limit_tid[tid];
	rr_retry_limit = ampdu_tx_cfg->rr_retry_limit_tid[tid];

	if (!ini || (ini->ba_state == AMPDU_TID_STATE_BA_PENDING_ON)) {
		wlc_ampdu_free_chain(ampdu_tx, p, txs);
		return;
	}

	ASSERT(ini->scb == scb);
	ASSERT(WLPKTTAGSCBGET(p) == scb);
	non_aqm = ini->non_aqm;

	bzero(bitmap, sizeof(bitmap));
	queue = D11_TXFID_GET_FIFO(wlc, txs->frameid);
	ASSERT(queue < AC_COUNT);

	update_rate = (WLPKTTAG(p)->flags & WLF_RATE_AUTO) ? TRUE : FALSE;
	supr_status = txs->status.suppr_ind;

	bsscfg = SCB_BSSCFG(scb);
	ASSERT(bsscfg != NULL);
	BCM_REFERENCE(bsscfg);

#ifdef PKTQ_LOG
	prec_index = WLAMPDU_PREC_TID2PREC(p, tid);

	if (scb_ampdu->txq.pktqlog) {
		prec_cnt = wlc_ampdu_pktqlog_cnt(&scb_ampdu->txq, tid, prec_index);
	}

	if ((WLC_GET_CQ(wlc->active_queue))->pktqlog) {
		actq_cnt =
		(WLC_GET_CQ(wlc->active_queue))->pktqlog->_prec_cnt[prec_index];
	}
#endif /* PKTQ_LOG */

	if (AMPDU_HOST_ENAB(wlc->pub)) {
		if (txs->status.was_acked) {
			/*
			 * Underflow status is reused  for BTCX to indicate AMPDU preemption.
			 * This prevents un-necessary rate fallback.
			 */
			if (TX_STATUS_SUPR_UF == supr_status) {
				WL_AMPDU_TX(("wl%d: wlc_ampdu_dotxstatus: BT preemption, skip rate "
					     "fallback\n", wlc->pub->unit));
				update_rate = FALSE;
			}

			ASSERT(txs->status.is_intermediate);
			start_seq = txs->sequence >> SEQNUM_SHIFT;
			bitmap[0] = (txs->status.raw_bits & TX_STATUS_BA_BMAP03_MASK) >>
				TX_STATUS_BA_BMAP03_SHIFT;

			ASSERT(!(s1 & TX_STATUS_INTERMEDIATE));
			ASSERT(s1 & TX_STATUS_AMPDU);

			bitmap[0] |= (s1 & TX_STATUS_BA_BMAP47_MASK) << TX_STATUS_BA_BMAP47_SHIFT;
			bitmap[1] = (s1 >> 8) & 0xff;
			bitmap[2] = (s1 >> 16) & 0xff;
			bitmap[3] = (s1 >> 24) & 0xff;

			/* remaining 4 bytes in s2 */
			bitmap[4] = s2 & 0xff;
			bitmap[5] = (s2 >> 8) & 0xff;
			bitmap[6] = (s2 >> 16) & 0xff;
			bitmap[7] = (s2 >> 24) & 0xff;

			ba_recd = TRUE;

#if defined(STA) && defined(DBG_BCN_LOSS)
			scb->dbg_bcn.last_tx = wlc->pub->now;
#endif /* defined(STA) && defined(DBG_BCN_LOSS) */

			WL_AMPDU_TX(("%s: Block ack received: start_seq is 0x%x bitmap "
				"%02x %02x %02x %02x %02x %02x %02x %02x\n",
				__FUNCTION__, start_seq, bitmap[0], bitmap[1], bitmap[2],
				bitmap[3], bitmap[4], bitmap[5], bitmap[6], bitmap[7]));

		}
	} else {
		/* AMPDU_HW txstatus package */

		ASSERT(txs->status.is_intermediate);
#ifdef BCMDBG
		if (WL_AMPDU_HWTXS_ON()) {
			wlc_print_ampdu_txstatus_noaqm(ampdu_tx, txs, s1, s2);
		}
#endif /* BCMDBG */

		ASSERT(!(s1 & TX_STATUS_INTERMEDIATE));
		start_seq = ini->start_seq;
		ncons = txs->sequence;
		bitmap[0] = (txs->status.raw_bits & TX_STATUS_BA_BMAP03_MASK) >>
			TX_STATUS_BA_BMAP03_SHIFT;
		bitmap[0] |= (s1 & TX_STATUS_BA_BMAP47_MASK) << TX_STATUS_BA_BMAP47_SHIFT;
		bitmap[1] = (s1 >> 8) & 0xff;
		bitmap[2] = (s1 >> 16) & 0xff;
		bitmap[3] = (s1 >> 24) & 0xff;
		ba_recd = bitmap[0] || bitmap[1] || bitmap[2] || bitmap[3];

		/* remaining 4 bytes in s2 */
		rs_txs.tx_cnt[0]	= (uint8)(s2 & 0xff);
		rs_txs.txsucc_cnt[0]	= (uint8)((s2 >> 8) & 0xff);
		rs_txs.tx_cnt[1]	= (uint8)((s2 >> 16) & 0xff);
		rs_txs.txsucc_cnt[1]	= (uint8)((s2 >> 24) & 0xff);

		WLCNTINCR(ampdu_tx->cnt->u0.txs);
		WLCNTADD(ampdu_tx->cnt->tx_mrt, rs_txs.tx_cnt[0]);
		WLCNTADD(ampdu_tx->cnt->txsucc_mrt, rs_txs.txsucc_cnt[0]);
		WLCNTADD(ampdu_tx->cnt->tx_fbr, rs_txs.tx_cnt[1]);
		WLCNTADD(ampdu_tx->cnt->txsucc_fbr, rs_txs.txsucc_cnt[1]);
		WLCNTADD(wlc->pub->_cnt->txfrag, rs_txs.txsucc_cnt[0] + rs_txs.txsucc_cnt[1]);
		WLCNTADD(wlc->pub->_cnt->txfrmsnt, rs_txs.txsucc_cnt[0] + rs_txs.txsucc_cnt[1]);
		WLCNTADD(wlc->pub->_cnt->txretrans, (rs_txs.tx_cnt[0] - rs_txs.txsucc_cnt[0]) +
			(rs_txs.tx_cnt[1] - rs_txs.txsucc_cnt[1]));
#ifdef PROP_TXSTATUS
		if (!(WL_TXSTATUS_GET_FLAGS(WLPKTTAG(p)->wl_hdr_information) &
			WLFC_PKTFLAG_PKTFROMHOST)) {
			WLCNTSCBADD(scb->scb_stats.tx_pkts_fw_total,
				rs_txs.txsucc_cnt[0] + rs_txs.txsucc_cnt[1]);
			WLCNTSCBADD(scb->scb_stats.tx_pkts_fw_retries,
				(rs_txs.tx_cnt[0] - rs_txs.txsucc_cnt[0]) +
				(rs_txs.tx_cnt[1] - rs_txs.txsucc_cnt[1]));
		} else
#endif	/* PROP_TXSTATUS */
		{
			WLCNTSCBADD(scb->scb_stats.tx_pkts_total,
				rs_txs.txsucc_cnt[0] + rs_txs.txsucc_cnt[1]);
			WLCNTSCBADD(scb->scb_stats.tx_pkts_retries,
				(rs_txs.tx_cnt[0] - rs_txs.txsucc_cnt[0]) +
				(rs_txs.tx_cnt[1] - rs_txs.txsucc_cnt[1]));
		}
#ifdef WL11K
		wlc_rrm_tscm_upd(wlc, scb, tid, OFFSETOF(rrm_tscm_t, msdu_tx),
			(rs_txs.txsucc_cnt[0] + rs_txs.txsucc_cnt[1]));
#endif // endif

		WLCNTSCBADD(scb_ampdu->ampdu_scb_stats->tx_pkts_total[ini->tid],
			rs_txs.txsucc_cnt[0] + rs_txs.txsucc_cnt[1]);
		WLCIFCNTADD(scb, txfrmsnt, rs_txs.txsucc_cnt[0] + rs_txs.txsucc_cnt[1]);
#ifdef WLSCB_HISTO
		WLSCB_HISTO_TX_IF_HTVHT(scb, rs_txs.txrspec[0], rs_txs.txsucc_cnt[0] <<
				WLPKTTAG_AMSDU(p));
		WLSCB_HISTO_TX_IF_HTVHT(scb, rs_txs.txrspec[1], rs_txs.txsucc_cnt[1] <<
				WLPKTTAG_AMSDU(p));
#endif /* WLSCB_HISTO */
		if (!(txs->status.was_acked)) {

			WL_PRINT(("Status: "));
			if ((txs->status.was_acked) == 0)
				WL_PRINT(("Not ACK_RCV "));
			WL_PRINT(("\n"));
			WL_PRINT(("Total attempts %d succ %d\n", rs_txs.tx_cnt[0],
				rs_txs.txsucc_cnt[0]));
		}
	}

	if (!ba_recd || AMPDU_MAC_ENAB(wlc->pub)) {
		if (!ba_recd) {
			WLCNTINCR(ampdu_tx->cnt->noba);
			AMPDUSCBCNTINCR(scb_ampdu->cnt.noba);
			if (AMPDU_MAC_ENAB(wlc->pub)) {
				WL_AMPDU_TX(("wl%d: wlc_ampdu_dotxstatus: error txstatus 0x%x\n",
					wlc->pub->unit, txs->status.raw_bits));
			}
		}
		if (supr_status != TX_STATUS_SUPR_NONE) {
#ifdef PKTQ_LOG
			WLCNTCONDINCR(prec_cnt, prec_cnt->suppress);
			WLCNTCONDINCR(actq_cnt, actq_cnt->suppress);
#endif /* PKTQ_LOG */

			update_rate = FALSE;
#if defined(BCMDBG) || defined(BCMDBG_AMPDU)
			ampdu_tx->amdbg->supr_reason[supr_status]++;
#endif // endif
			WL_AMPDU_TX(("wl%d: wlc_ampdu_dotxstatus: supr_status 0x%x\n",
				wlc->pub->unit, supr_status));
			/* no need to retry for badch; will fail again */
			if (supr_status == TX_STATUS_SUPR_BADCH) {
				retry = FALSE;
				WLCNTINCR(wlc->pub->_cnt->txchanrej);
			} else if (supr_status == TX_STATUS_SUPR_EXPTIME) {

				WLCNTINCR(wlc->pub->_cnt->txexptime);
#ifdef WL11K
				wlc_rrm_tscm_upd(wlc, scb, tid, OFFSETOF(rrm_tscm_t, msdu_exp), 1);
#endif // endif

				/* Interference detected */
				if (wlc->rfaware_lifetime)
					wlc_exptime_start(wlc);
			/* TX underflow : try tuning pre-loading or ampdu size */
			} else if (supr_status == TX_STATUS_SUPR_FRAG) {
				/* if there were underflows, but pre-loading is not active,
				   notify rate adaptation.
				*/
				if (wlc_ffpld_check_txfunfl_noaqm(wlc, prio2fifo[tid], bsscfg) > 0)
				{
				}
			}
		} else if (txs->phyerr) {
			update_rate = FALSE;
			WLCNTINCR(wlc->pub->_cnt->txphyerr);
			WL_ERROR(("wl%d: %s: tx phy error (0x%x)\n",
				wlc->pub->unit, __FUNCTION__, txs->phyerr));

#ifdef BCMDBG
			if (WL_ERROR_ON()) {
				prpkt("txpkt (AMPDU)", wlc->osh, p);
				if (AMPDU_HOST_ENAB(wlc->pub)) {
					wlc_print_txdesc(wlc, PKTDATA(wlc->osh, p));
					wlc_print_txstatus(wlc, txs);
				} else {
					wlc_print_ampdu_txstatus_noaqm(ampdu_tx, txs, s1, s2);
					bcm_bprintf_bypass = TRUE;
					wlc_dump_aggfifo(wlc, NULL);
					bcm_bprintf_bypass = FALSE;
				}
			}
#endif /* BCMDBG */
		}
	}

	/* Check if interference is still there */
	if (wlc->rfaware_lifetime && wlc->exptime_cnt && (supr_status != TX_STATUS_SUPR_EXPTIME))
		wlc_exptime_check_end(wlc);

#if defined(WLPKTDLYSTAT) || defined(WL11K)
	/* Get the current time */
	now = WLC_GET_CURR_TIME(wlc);
#endif // endif

	/* loop through all pkts and retry if not acked */
	while (p) {
		if ((WLPKTTAG(p)->flags & WLF_AMPDU_MPDU) == 0) {
			WL_PRINT(("%s flags=%x seq=%d\n", __FUNCTION__,
				WLPKTTAG(p)->flags, WLPKTTAG(p)->seq));
		}

		ASSERT(WLPKTTAG(p)->flags & WLF_AMPDU_MPDU);
		SCB_PKTS_INFLT_FIFOCNT_DECR(scb, tid);

		txh = (d11txh_pre40_t *)PKTDATA(wlc->osh, p);
		mcl = ltoh16(txh->MacTxControlLow);
		plcp = (uint8 *)(txh + 1);
		h = (struct dot11_header*)(plcp + D11_PHY_HDR_LEN);
		seq = ltoh16(h->seq) >> SEQNUM_SHIFT;
		h->fc &= (~htol16(FC_RETRY));
		pkt_freed = FALSE;
#ifdef PROP_TXSTATUS
		wlfc_suppr_acked_status = 0;
#endif /* PROP_TXSTATUS */
#ifdef PKTQ_LOG
		pktlen = pkttotlen(wlc->osh, p) - D11_TXH_LEN;
		prec_pktlen = pktlen - DOT11_LLC_SNAP_HDR_LEN - DOT11_MAC_HDR_LEN - DOT11_QOS_LEN;
		tot_len += pktlen;
#endif /* PKTQ_LOG */
		if (tot_mpdu == 0) {
			mcs = plcp[0] & MIMO_PLCP_MCS_MASK;
			mimoantsel = ltoh16(txh->ABI_MimoAntSel);
			is_sgi = PLCP3_ISSGI(plcp[3]);
			is40 = (plcp[0] & MIMO_PLCP_40MHZ) ? 1 : 0;
			BCM_REFERENCE(is40);

			if (AMPDU_HOST_ENAB(wlc->pub)) {
				/* this is only needed for host agg */
#if defined(WLPKTDLYSTAT) || defined(PKTQ_LOG)
				BCM_REFERENCE(mcs_fbr);
				mcs_fbr = txh->FragPLCPFallback[0] & MIMO_PLCP_MCS_MASK;
#endif /* WLPKTDLYSTAT || PKTQ_LOG */
			} else {
				first_frameid = ltoh16(txh->TxFrameID);
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
				if (PLCP3_ISSGI(plcp[3])) {
					WLCNTADD(ampdu_tx->cnt->u1.txmpdu_sgi, rs_txs.tx_cnt[0]);
					if (ampdu_tx->amdbg)
						WLCNTADD(ampdu_tx->amdbg->txmcssgi[MCS2IDX(mcs)],
							rs_txs.tx_cnt[0]);
				}
				if (PLCP3_ISSTBC(plcp[3])) {
					WLCNTADD(ampdu_tx->cnt->u2.txmpdu_stbc, rs_txs.tx_cnt[0]);
					if (ampdu_tx->amdbg)
						WLCNTADD(ampdu_tx->amdbg->txmcsstbc[MCS2IDX(mcs)],
							rs_txs.tx_cnt[0]);
				}
				if (ampdu_tx->amdbg) {
					WLCNTADD(ampdu_tx->amdbg->txmcs[MCS2IDX(mcs)],
						rs_txs.tx_cnt[0]);
					WLCNTADD(ampdu_tx->amdbg->txmcs_succ[MCS2IDX(mcs)],
						rs_txs.txsucc_cnt[0]);
				}
#ifdef WLPKTDLYSTAT
				WLCNTADD(ampdu_tx->cnt->txmpdu_cnt[MCS2IDX(mcs)], rs_txs.tx_cnt[0]);
				WLCNTADD(ampdu_tx->cnt->txmpdu_succ_cnt[MCS2IDX(mcs)],
					rs_txs.txsucc_cnt[0]);
#endif /* WLPKTDLYSTAT */
				if ((ltoh16(txh->XtraFrameTypes) & 0x3) != 0) {
#ifdef WLCNT
					uint8 mcs_fb;
					/* if not fbr_cck */
					mcs_fb = txh->FragPLCPFallback[0] & MIMO_PLCP_MCS_MASK;
#endif /* WLCNT */
					if (ampdu_tx->amdbg)
						WLCNTADD(ampdu_tx->amdbg->txmcs[MCS2IDX(mcs_fb)],
							rs_txs.tx_cnt[1]);
#ifdef WLPKTDLYSTAT
					WLCNTADD(ampdu_tx->cnt->txmpdu_cnt[MCS2IDX(mcs_fb)],
						rs_txs.tx_cnt[1]);
					WLCNTADD(ampdu_tx->cnt->txmpdu_succ_cnt[MCS2IDX(mcs_fb)],
						rs_txs.txsucc_cnt[1]);
#endif /* WLPKTDLYSTAT */
					if (PLCP3_ISSGI(txh->FragPLCPFallback[3])) {
						WLCNTADD(ampdu_tx->cnt->u1.txmpdu_sgi,
							rs_txs.tx_cnt[1]);
						if (ampdu_tx->amdbg)
							WLCNTADD(ampdu_tx->amdbg->txmcssgi
								[MCS2IDX(mcs_fb)],
								rs_txs.tx_cnt[1]);
					}
					if (PLCP3_ISSTBC(txh->FragPLCPFallback[3])) {
						WLCNTADD(ampdu_tx->cnt->u2.txmpdu_stbc,
							rs_txs.tx_cnt[1]);
						if (ampdu_tx->amdbg)
							WLCNTADD(ampdu_tx->amdbg->txmcsstbc[
							       MCS2IDX(mcs_fb)], rs_txs.tx_cnt[1]);
					}
				}
#endif /* BCMDBG || WLTEST */
			}
		}

		indx = TX_SEQ_TO_INDEX(seq);
		if (AMPDU_HOST_ENAB(wlc->pub)) {
			txretry = non_aqm->txretry[indx];
		}

		if (MODSUB_POW2(seq, ini->start_seq, SEQNUM_MAX) >= ini->ba_wsize) {
			WL_NONE(("wl%d: %s: unexpected completion: seq 0x%x, "
				"start seq 0x%x\n",
				wlc->pub->unit, __FUNCTION__, seq, ini->start_seq));
			pkt_freed = TRUE;
#ifdef BCMDBG
			wlc_print_ampdu_txstatus_noaqm(ampdu_tx, txs, s1, s2);
#endif /* BCMDBG */
			goto nextp;
		}

		if (!isset(non_aqm->ackpending, indx)) {
			WL_AMPDU_ERR(("wl%d: %s: seq 0x%x\n",
				wlc->pub->unit, __FUNCTION__, seq));
			ampdu_dump_ini(wlc, ini);
			ASSERT(isset(non_aqm->ackpending, indx));
		}

		ack_recd = FALSE;
		if (ba_recd || AMPDU_MAC_ENAB(wlc->pub)) {
			if (AMPDU_MAC_ENAB(wlc->pub))
				/* use position bitmap */
				bindex = tot_mpdu;
			else
				bindex = MODSUB_POW2(seq, start_seq, SEQNUM_MAX);

			WL_AMPDU_TX(("%s: tid %d seq is 0x%x, start_seq is 0x%x, "
			          "bindex is %d set %d, txretry %d, index %d\n",
			          __FUNCTION__, tid, seq, start_seq, bindex,
			          isset(bitmap, bindex), txretry, indx));

			/* if acked then clear bit and free packet */
			if ((bindex < AMPDU_BA_BITS_IN_BITMAP) && isset(bitmap, bindex)) {
				if (isset(non_aqm->ackpending, indx)) {
					clrbit(non_aqm->ackpending, indx);
					if (isset(non_aqm->barpending, indx)) {
						clrbit(non_aqm->barpending, indx);
					}
					non_aqm->txretry[indx] = 0;
				}

#ifdef PROP_TXSTATUS
				if (PROP_TXSTATUS_ENAB(wlc->pub) && (!AMPDU_AQM_ENAB(wlc->pub))) {
					if (wlc_wlfc_suppr_status_query(wlc, scb)) {
						wlc_pkttag_t *pkttag = WLPKTTAG(p);
						uint32 wlhinfo = pkttag->wl_hdr_information;
						uint32 flags = WL_TXSTATUS_GET_FLAGS(wlhinfo);
						if ((flags & WLFC_PKTFLAG_PKTFROMHOST) &&
							(pkttag->flags & WLF_PROPTX_PROCESSED)) {
							wlfc_suppr_acked_status =
								WLFC_CTL_PKTFLAG_SUPPRESS_ACKED;
						}
					}
				}
#endif /* PROP_TXSTATUS */
				wlc_pcb_fn_invoke(wlc->pcb, p, TX_STATUS_ACK_RCV);

#ifdef WLTXMONITOR
				if (MONITOR_ENAB(wlc) || PROMISC_ENAB(wlc->pub))
					wlc_tx_monitor(wlc, (d11txhdr_t *)txh, txs, p, NULL);
#endif /* WLTXMONITOR */
				pkt_freed = TRUE;
				ack_recd = TRUE;
#ifdef PKTQ_LOG
				WLCNTCONDINCR(prec_cnt, prec_cnt->acked);
				WLCNTCONDINCR(actq_cnt, actq_cnt->acked);
				SCB_BS_DATA_CONDINCR(bs_data_counters, acked);
				WLCNTCONDADD(prec_cnt, prec_cnt->throughput, prec_pktlen);
				WLCNTCONDADD(actq_cnt, actq_cnt->throughput, prec_pktlen);
				SCB_BS_DATA_CONDADD(bs_data_counters, throughput, prec_pktlen);
				WLCNTSCBADD(scb_ampdu->ampdu_scb_stats->tx_bytes_total[ini->tid],
					prec_pktlen);
#endif /* PKTQ_LOG */
				suc_mpdu++;
			}
		}
		/* either retransmit or send bar if ack not recd */
		if (!ack_recd) {
			bool free_pkt = FALSE;
			bool relist = FALSE;

#if defined(PSPRETEND)
			if (BSSCFG_AP(bsscfg) && PS_PRETEND_ENABLED(bsscfg)) {
				relist = wlc_pspretend_pkt_relist(wlc->pps_info, bsscfg, scb,
				                                  retry_limit, txretry);
			}
#endif /* PSPRETEND */

#if defined(PROP_TXSTATUS) && defined(WLP2P_UCODE)
			/* Durring suppress state or suppressed packet
			 * return retry and suppressed packets to host
			 * as suppresed, don't resend to Ucode.
			 */
			if (AMPDU_HOST_ENAB(wlc->pub) && PROP_TXSTATUS_ENAB(wlc->pub)) {
				if ((wlc_wlfc_suppr_status_query(wlc, scb) ||
					!TXS_SUPR_MAGG_DONE(txs->status.suppr_ind)) &&
					!wlc_should_retry_suppressed_pkt(wlc, p, supr_status)) {
					/* change retry to suppres status */
					if (TXS_SUPR_MAGG_DONE(txs->status.suppr_ind)) {
						supr_status = TX_STATUS_SUPR_NACK_ABS;
						txs->status.suppr_ind = TX_STATUS_SUPR_NACK_ABS;
					}
					retry = FALSE;
				}
			}
#endif /* PROP_TXSTATUS && WLP2P_UCODE */

			if (AMPDU_HOST_ENAB(wlc->pub) &&
			    retry) {

				if (AP_ENAB(wlc->pub) &&
				    (supr_status == TX_STATUS_SUPR_PMQ)) {
					/* last_frag TRUE as fragmentation not allowed for AMPDU */
					free_pkt = wlc_apps_suppr_frame_enq(wlc, p, txs, TRUE);
				} else if (P2P_ABS_SUPR(wlc, supr_status) &&
				         BSS_TX_SUPR_ENAB(bsscfg)) {
					/* This is possible if we got a packet suppression
					 * before getting ABS interrupt
					 */
					if (!BSS_TX_SUPR(bsscfg)) {
						wlc_bsscfg_tx_stop(wlc->psqi, bsscfg);
					}

					if (BSSCFG_AP(bsscfg) &&
					    SCB_ASSOCIATED(scb) && SCB_P2P(scb))
						wlc_apps_scb_tx_block(wlc, scb, 1, TRUE);
					/* With Proptxstatus enabled in dongle and host,
					 * pkts sent by host will not come here as retry is set
					 * to FALSE by wlc_should_retry_suppressed_pkt().
					 * With Proptxstatus disabled in dongle or not active
					 * in host, all the packets will come here and
					 * need to be re-enqueued.
					 */
					free_pkt = wlc_pkt_abs_supr_enq(wlc, scb, p);
				} else if ((txretry < (int)retry_limit) || relist) {
					/* Set retry bit */
					h->fc |= htol16(FC_RETRY);

					non_aqm->txretry[indx]++;
					ASSERT(non_aqm->retry_cnt < ampdu_tx_cfg->ba_max_tx_wsize);
					ASSERT(non_aqm->retry_seq[non_aqm->retry_tail] == 0);
					non_aqm->retry_seq[non_aqm->retry_tail] = seq;
					non_aqm->retry_tail = NEXT_TX_INDEX(non_aqm->retry_tail);
					non_aqm->retry_cnt++;

#ifdef PKTQ_LOG
					WLCNTCONDINCR(prec_cnt, prec_cnt->retry);
					WLCNTCONDINCR(actq_cnt, actq_cnt->retry);
					SCB_BS_DATA_CONDINCR(bs_data_counters, retry);
#endif /* PKTQ_LOG */

#if defined(PSPRETEND)
					if (relist) {

						/* not using pmq for ps pretend, so indicate
						 * no block flag
						 */
						if (!SCB_PS(scb)) {
							wlc_pspretend_on(wlc->pps_info, scb,
							                 PS_PRETEND_NO_BLOCK);
						}
#ifdef PKTQ_LOG
						WLCNTCONDINCR(prec_cnt, prec_cnt->ps_retry);
						WLCNTCONDINCR(actq_cnt, actq_cnt->ps_retry);
#endif /* PKTQ_LOG */
					}
#endif /* PSPRETEND */
					SCB_TX_NEXT(TXMOD_AMPDU, scb, p, WLC_PRIO_TO_HI_PREC(tid));
				} else {
					WL_NONE(("%s: # retries exceeds limit\n", __FUNCTION__));
					free_pkt = TRUE;
				}

				if (free_pkt)
					goto not_retry;

				requeue++;
			} else {
			not_retry:
#ifdef PROP_TXSTATUS
			/* Host suppressed packets */
			if (PROP_TXSTATUS_ENAB(wlc->pub) &&
				WLFC_GET_REUSESEQ(wlfc_query_mode(wlc->wlfc)) &&
				!TXS_SUPR_MAGG_DONE(supr_status) &&
				(WL_TXSTATUS_GET_FLAGS(WLPKTTAG(p)->wl_hdr_information) &
					WLFC_PKTFLAG_PKTFROMHOST)) {
				non_aqm->txretry[indx] = 0;
				ini->suppr_window++;
			} else
#endif /* PROP_TXSTATUS */
			{
				setbit(non_aqm->barpending, indx);
				send_bar = TRUE;
			}

#ifdef WLTXMONITOR
				if (MONITOR_ENAB(wlc) || PROMISC_ENAB(wlc->pub))
					wlc_tx_monitor(wlc, (d11txhdr_t *)txh, txs, p, NULL);
#endif /* WLTXMONITOR */
#ifdef PKTQ_LOG
				WLCNTCONDINCR(prec_cnt, prec_cnt->retry_drop);
				WLCNTCONDINCR(actq_cnt, actq_cnt->retry_drop);
				SCB_BS_DATA_CONDINCR(bs_data_counters, retry_drop);
#endif /* PKTQ_LOG */
				WLCNTINCR(wlc->pub->_cnt->txfail);
#ifdef PROP_TXSTATUS
				if (!(WL_TXSTATUS_GET_FLAGS(WLPKTTAG(p)->wl_hdr_information) &
					WLFC_PKTFLAG_PKTFROMHOST)) {
					WLCNTSCBINCR(scb->scb_stats.tx_pkts_fw_retry_exhausted);
				} else
#endif	/* PROP_TXSTATUS */
				{
					WLCNTSCBINCR(scb->scb_stats.tx_pkts_retry_exhausted);
				}
				WLCNTSCBINCR(scb->scb_stats.tx_failures);
				WLCIFCNTINCR(scb, txfail);
#ifdef WL11K
				wlc_rrm_tscm_upd(wlc, scb, tid,
					OFFSETOF(rrm_tscm_t, msdu_fail), 1);
#endif // endif
				pkt_freed = TRUE;
			}
		} else if (AMPDU_MAC_ENAB(wlc->pub)) {
			ba_recd = TRUE;
		}
#if defined(BCMDBG) || defined(WLTEST)
		/* tot_mpdu may exceeds AMPDU_MAX_MPDU if doing ucode agg */
		if (AMPDU_HOST_ENAB(wlc->pub) && ba_recd && !ack_recd)
			hole[idx]++;
#endif /* defined(BCMDBG) || defined(WLTEST) */

nextp:
#if defined(BCMDBG) || defined(WLTEST)
		if ((idx = tot_mpdu) >= AMPDU_MAX_MPDU) {
			WL_AMPDU_ERR(("%s: idx out-of-bound to array size (%d)\n",
					__FUNCTION__, idx));
			idx = AMPDU_MAX_MPDU - 1;
		}
#endif /* defined(BCMDBG) || defined(WLTEST) */
		tot_mpdu++;
#ifdef PROP_TXSTATUS
		if (PROP_TXSTATUS_ENAB(wlc->pub) && ((h->fc & htol16(FC_RETRY)) == 0)) {
			uint8 wlfc_status = wlc_txstatus_interpret(&txs->status, ack_recd);
			if (!ack_recd && scb &&
			    WLFC_CONTROL_SIGNALS_TO_HOST_ENAB(wlc->pub) &&
			    (wlfc_status == WLFC_CTL_PKTFLAG_D11SUPPRESS)) {
				wlc_suppress_sync_fsm(wlc, scb, p, TRUE);
			}
			if (wlfc_suppr_acked_status)
				wlfc_status = wlfc_suppr_acked_status;
			wlfc_process_txstatus(wlc->wlfc, wlfc_status, p, txs, FALSE);
		}
#endif // endif

#ifdef WL_TX_STALL
		if (!ack_recd) {
			if (supr_status != TX_STATUS_SUPR_NONE) {
				WL_TX_STS_UPDATE(tx_status,
					wlc_tx_status_map_hw_to_sw_supr_code(wlc, supr_status));
			} else if (txs->phyerr) {
				WL_TX_STS_UPDATE(tx_status, WLC_TX_STS_PHY_ERROR);
			} else {
				WL_TX_STS_UPDATE(tx_status, WLC_TX_STS_RETRY_TIMEOUT);
			}

			wlc_tx_status_update_counters(wlc, p, scb, bsscfg, tx_status, 1);

			/* Clear status, next packet will have its own status */
			WL_TX_STS_UPDATE(tx_status, WLC_TX_STS_SUCCESS);
		}
#endif /* WL_TX_STALL */
		/* calculate latency and packet loss statistics */
		if (pkt_freed) {
#if defined(WLPKTDLYSTAT) || defined(WL11K)
			/* Ignore wrap-around case and error case */
			if (now > WLPKTTAG(p)->shared.enqtime) {
				delay = now - WLPKTTAG(p)->shared.enqtime;
#ifdef WLPKTDLYSTAT
				if (AMPDU_HOST_ENAB(ampdu_tx->wlc->pub))
					tr = (txretry >= AMPDU_DEF_RETRY_LIMIT) ?
						AMPDU_DEF_RETRY_LIMIT : txretry;
				else
					tr = 0;

				wlc_delay_stats_upd(delay_stats, delay, tr, ack_recd);
				WL_AMPDU_STAT(("Enq %d retry %d cnt %d: acked %d delay/min/"
					"max/sum %d %d %d %d\n", WLPKTTAG(p)->shared.enqtime,
					tr, delay_stats->txmpdu_cnt[tr], ack_recd, delay,
					delay_stats->delay_min, delay_stats->delay_max,
					delay_stats->delay_sum[tr]));
#endif /* WLPKTDLYSTAT */
#ifdef WL11K
				if (ack_recd) {
					wlc_rrm_delay_upd(wlc, scb, tid, delay);
				}
#endif // endif
			}
#endif /* WLPKTDLYSTAT || WL11K */
			PKTFREE(wlc->osh, p, TRUE);
		} else {
			/* Packet requeued, update the counters */
			wlc_tx_status_update_counters(wlc, p,
				scb, bsscfg, WLC_TX_STS_QUEUED, 1);
		}
		/* break out if last packet of ampdu */
		if (AMPDU_MAC_ENAB(wlc->pub)) {
			if (tot_mpdu == ncons) {
				break;
			}
			if (++pdu_count >= ampdu_tx_cfg->ba_max_tx_wsize) {
				WL_AMPDU_ERR(("%s: Reach max num of MPDU without finding frameid\n",
					__FUNCTION__));
				ASSERT(pdu_count >= ampdu_tx_cfg->ba_max_tx_wsize);
			}
		} else {
			if (((mcl & TXC_AMPDU_MASK) >> TXC_AMPDU_SHIFT) == TXC_AMPDU_LAST)
				break;
		}

		p = GETNEXTTXP(wlc, queue);
		/* For external use big hammer to restore the sync */
		if (p == NULL) {
			WL_AMPDU_ERR(("%s: p is NULL. tot_mpdu %d suc %d pdu %d first_frameid %x"
				"fifocnt %d\n", __FUNCTION__, tot_mpdu, suc_mpdu, pdu_count,
				first_frameid,
				R_REG(wlc->osh, D11_XmtFifoFrameCnt(wlc))));

#ifdef BCMDBG
			wlc_print_ampdu_txstatus_noaqm(ampdu_tx, txs, s1, s2);
			bcm_bprintf_bypass = TRUE;
			wlc_dump_aggfifo(wlc, NULL);
			bcm_bprintf_bypass = FALSE;
#endif	/* BCMDBG */
			ASSERT(p);
			break;
		}

		WLPKTTAGSCBSET(p, scb);

	} /* while(p) */

	if (AMPDU_HOST_ENAB(wlc->pub)) {
		WLCNTADD(wlc->pub->_cnt->txfrag, suc_mpdu);
		WLCNTADD(wlc->pub->_cnt->txfrmsnt, suc_mpdu);
		WLCNTSCBADD(scb->scb_stats.tx_pkts_total, suc_mpdu);
	}

#if defined(BCMDBG) || defined(BCMDBG_AMPDU) || defined(WLTEST)
	/* post-process the statistics */
	if (AMPDU_HOST_ENAB(wlc->pub)) {
		int i, j;
		ampdu_tx->amdbg->mpdu_histogram[idx]++;
		for (i = 0; i < idx; i++) {
			if (hole[i])
				ampdu_tx->amdbg->retry_histogram[i]++;
			if (hole[i]) {
				for (j = i + 1; (j < idx) && hole[j]; j++)
					;
				if (j == idx)
					ampdu_tx->amdbg->end_histogram[i]++;
			}
		}

#ifdef WLPKTDLYSTAT
		if (txretry < rr_retry_limit)
			WLCNTADD(ampdu_tx->cnt->txmpdu_succ_cnt[MCS2IDX(mcs)], suc_mpdu);
		else
			WLCNTADD(ampdu_tx->cnt->txmpdu_succ_cnt[MCS2IDX(mcs_fbr)], suc_mpdu);
#endif /* WLPKTDLYSTAT */
	}
#endif /* defined(BCMDBG) || defined(BCMDBG_AMPDU) || defined(WLTEST) */

#ifdef PKTQ_LOG
#define BPS_TO_500K(rate)		((rate)/500)		/**< Convert in uinits of 500kbps */
#define MCS_TO_RSPEC(mcs, bw, plcp)	((HT_RSPEC(mcs)) | ((bw) << WL_RSPEC_BW_SHIFT)| \
		((PLCP3_ISSGI(plcp)) ? WL_RSPEC_SGI : 0))
	ASSERT(txh && plcp);
	bw = (txh->PhyTxControlWord_1 & PHY_TXC1_BW_MASK) >> 1;

	/* Note HOST AMPDU path is HT only, no 10MHz support */
	ASSERT((bw >= BW_20MHZ) && (bw <= BW_40MHZ));

	if (txretry < rr_retry_limit) {
		/* Primary rate computation */
		rspec = MCS_TO_RSPEC(mcs, bw, plcp[3]);
		current_rate = wf_rspec_to_rate(rspec);
	} else {
		/* Fall-back rate computation */
		rspec = MCS_TO_RSPEC(mcs_fbr, bw, plcp[3]);
		current_rate = wf_rspec_to_rate(rspec);
	}

	if (suc_mpdu) {
		txrate_succ = BPS_TO_500K(current_rate) * suc_mpdu;
		WLCNTCONDADD(prec_cnt, prec_cnt->txrate_succ, txrate_succ);
		WLCNTCONDADD(actq_cnt, actq_cnt->txrate_succ, txrate_succ);
	}

	/* Air Time Computation */

	flg = WLC_AIRTIME_AMPDU;

	if (WLC_HT_GET_AMPDU_RTS(wlc->hti)) {
		flg |= (WLC_AIRTIME_RTSCTS);
	}
	ctl_rspec = AMPDU_BASIC_RATE(wlc->band, rspec);

	/* Short preamble */
	if (WLC_PROT_CFG_SHORTPREAMBLE(wlc->prot, bsscfg) &&
			(scb->flags & SCB_SHORTPREAMBLE) && RSPEC_ISCCK(ctl_rspec)) {
		ctl_rspec |= WL_RSPEC_SHORT_PREAMBLE;
	}

	rts_sifs = airtime_rts_usec(flg, ctl_rspec);
	cts_sifs = airtime_cts_usec(flg, ctl_rspec);
	ba_sifs = airtime_ba_usec(flg, ctl_rspec);
	ampdu_dur = wlc_airtime_packet_time_us(flg, rspec, tot_len);

	if (ba_recd) {
		/* If BA received */
		airtime = (txs->status.rts_tx_cnt + 1)* rts_sifs +
			cts_sifs + ampdu_dur + ba_sifs;
	} else {
		/* No Block Ack received
		 * 1. AMPDU transmitted but block ack not rcvd (RTS enabled/RTS disabled).
		 * 2. AMPDU not transmitted at all (RTS retrie timeout)
		 */
		if (txs->status.frag_tx_cnt) {
			airtime = (txs->status.rts_tx_cnt + 1)* rts_sifs +
				cts_sifs + ampdu_dur;
		} else {
			airtime = (txs->status.rts_tx_cnt + 1)* rts_sifs;
		}
	}
	WLCNTCONDADD(actq_cnt, actq_cnt->airtime, airtime);
#endif /* PKTQ_LOG */
	/* send bar to move the window */
	if (send_bar) {
		wlc_ampdu_send_bar(ampdu_tx, ini, SEQNUM_CHK);
	}

	/* update rate state */
	if (WLANTSEL_ENAB(wlc))
		antselid = wlc_antsel_antsel2id(wlc->asi, mimoantsel);

	rs_txs.txrts_cnt = txs->status.rts_tx_cnt;
	rs_txs.rxcts_cnt = txs->status.cts_rx_cnt;
	rs_txs.ncons = ncons;
	rs_txs.nlost = tot_mpdu - suc_mpdu;
	rs_txs.ack_map1 = *(uint32 *)&bitmap[0];
	rs_txs.ack_map2 = *(uint32 *)&bitmap[4];
	/* init now to txs->frameid for host agg; for hw agg it is
	 * first_frameid.
	 */
	rs_txs.frameid = txs->frameid;
	rs_txs.antselid = antselid;
	WL_AMPDU_HWDBG(("%s: consume %d txfifo_cnt %d\n", __FUNCTION__, tot_mpdu,
		R_REG(wlc->osh, D11_XmtFifoFrameCnt(wlc))));

	if (AMPDU_MAC_ENAB(wlc->pub)) {
		/* primary rspec */
		rs_txs.txrspec[0] = HT_RSPEC(mcs);
		/* use primary rate's sgi & bw info */
		rs_txs.txrspec[0] |= is_sgi ? WL_RSPEC_SGI : 0;
		rs_txs.txrspec[0] |= (is40 ? WL_RSPEC_BW_40MHZ : WL_RSPEC_BW_20MHZ);
		rs_txs.frameid = first_frameid;

		if (rs_txs.tx_cnt[0] + rs_txs.tx_cnt[1] == 0) {
			/* must have failed because of rts failure */
			ASSERT(rs_txs.txsucc_cnt[0] + rs_txs.txsucc_cnt[1] == 0);
#ifdef BCMDBG
			if (rs_txs.txsucc_cnt[0] + rs_txs.txsucc_cnt[1] != 0) {
				WL_AMPDU_ERR(("%s: wrong condition\n", __FUNCTION__));
				wlc_print_ampdu_txstatus_noaqm(ampdu_tx, txs, s1, s2);
			}
			if (WL_AMPDU_ERR_ON()) {
				wlc_print_ampdu_txstatus_noaqm(ampdu_tx, txs, s1, s2);
			}
#endif /* BCMDBG */
			/* fix up tx_cnt */
			rs_txs.tx_cnt[0] = ncons;
		}

		if (update_rate)
			wlc_scb_ratesel_upd_txs_ampdu(wlc->wrsi, scb, &rs_txs, txs, 0);
	} else if (update_rate) {
		wlc_scb_ratesel_upd_txs_blockack(wlc->wrsi, scb,
			txs, suc_mpdu, tot_mpdu, !ba_recd, (uint8)txretry,
			rr_retry_limit, FALSE, wlc_rate_ht_to_nonht(mcs & MIMO_PLCP_MCS_MASK),
			is_sgi, antselid, ac);
	}

	WL_AMPDU_TX(("wl%d: wlc_ampdu_dotxstatus: requeueing %d packets, consumed %d\n",
		wlc->pub->unit, requeue, tot_mpdu));

	/* Call only once per ampdu_tx for SW agg */
	if (AMPDU_MAC_ENAB(wlc->pub)) {
		WLCNTADD(ampdu_tx->cnt->pending, -tot_mpdu);
		WLCNTADD(ampdu_tx->cnt->cons, tot_mpdu);
		wlc_txfifo_complete(wlc, queue, tot_mpdu);
	} else {
		wlc_txfifo_complete(wlc, queue, ampdu_tx_cfg->txpkt_weight);
	}

#ifdef PSPRETEND
	if ((BSSCFG_AP(bsscfg) || BSSCFG_IBSS(bsscfg)) &&
	    PS_PRETEND_ENABLED(bsscfg)) {
		wlc_pspretend_dotxstatus(wlc->pps_info, bsscfg, scb, ba_recd);
	}
#endif /* PSPRETEND */
	/* bump up the start seq to move the window */
	wlc_ampdu_ini_move_window(ampdu_tx, scb_ampdu, ini);

#ifdef STA
	/* PM state change */
	wlc_update_pmstate(bsscfg, (uint)(wlc_txs_alias_to_old_fmt(wlc, &(txs->status))));
#endif /* STA */
} /* wlc_ampdu_dotxstatus_complete_noaqm */

#if defined(BCMDBG)

static void
wlc_print_ampdu_txstatus_noaqm(ampdu_tx_info_t *ampdu_tx,
                               tx_status_t *pkg1, uint32 s1, uint32 s2)
{
	uint16 *p = (uint16 *)pkg1;
	printf("%s: txstatus 0x%04x\n", __FUNCTION__, pkg1->status.raw_bits);
	if (AMPDU_MAC_ENAB(ampdu_tx->wlc->pub) && !AMPDU_AQM_ENAB(ampdu_tx->wlc->pub)) {
		uint16 last_frameid, txstat;
		uint8 bitmap[8];
		uint16 txcnt_mrt, succ_mrt, txcnt_fbr, succ_fbr;
		uint16 ncons = pkg1->sequence;

		last_frameid = p[2]; txstat = p[3];

		bitmap[0] = (txstat & TX_STATUS_BA_BMAP03_MASK) >> TX_STATUS_BA_BMAP03_SHIFT;
		bitmap[0] |= (s1 & TX_STATUS_BA_BMAP47_MASK) << TX_STATUS_BA_BMAP47_SHIFT;
		bitmap[1] = (s1 >> 8) & 0xff;
		bitmap[2] = (s1 >> 16) & 0xff;
		bitmap[3] = (s1 >> 24) & 0xff;
		txcnt_mrt = s2 & 0xff;
		succ_mrt = (s2 >> 8) & 0xff;
		txcnt_fbr = (s2 >> 16) & 0xff;
		succ_fbr = (s2 >> 24) & 0xff;
		printf("\t\t ncons %d last frameid: 0x%x\n", ncons, last_frameid);
		printf("\t\t txcnt: mrt %2d succ %2d fbr %2d succ %2d\n",
		       txcnt_mrt, succ_mrt, txcnt_fbr, succ_fbr);
		printf("\t\t bitmap: %02x %02x %02x %02x\n",
		       bitmap[0], bitmap[1], bitmap[2], bitmap[3]);

#ifdef WLAMPDU_UCODE
		if (txcnt_mrt + txcnt_fbr == 0) {
			uint16 strt, end, wptr, rptr, rnum, qid;
			uint16 base =
				(wlc_read_shm(ampdu_tx->wlc, M_TXFS_PTR(ampdu_tx->wlc)) +
				C_TXFSD_WOFFSET) * 2;
			qid = D11_TXFID_GET_FIFO(ampdu_tx->wlc, last_frameid);
			strt = wlc_read_shm(ampdu_tx->wlc, C_TXFSD_STRT_POS(base, qid));
			end = wlc_read_shm(ampdu_tx->wlc, C_TXFSD_END_POS(base, qid));
			wptr = wlc_read_shm(ampdu_tx->wlc, C_TXFSD_WPTR_POS(base, qid));
			rptr = wlc_read_shm(ampdu_tx->wlc, C_TXFSD_RPTR_POS(base, qid));
			rnum = wlc_read_shm(ampdu_tx->wlc, C_TXFSD_RNUM_POS(base, qid));
			printf("%s: side channel %d ---- \n", __FUNCTION__, qid);
			printf("\t\t strt : shm(0x%x) = 0x%x\n", C_TXFSD_STRT_POS(base, qid), strt);
			printf("\t\t end  : shm(0x%x) = 0x%x\n", C_TXFSD_END_POS(base, qid), end);
			printf("\t\t wptr : shm(0x%x) = 0x%x\n", C_TXFSD_WPTR_POS(base, qid), wptr);
			printf("\t\t rptr : shm(0x%x) = 0x%x\n", C_TXFSD_RPTR_POS(base, qid), rptr);
			printf("\t\t rnum : shm(0x%x) = 0x%x\n", C_TXFSD_RNUM_POS(base, qid), rnum);
		}
#endif /* WLAMPDU_UCODE */
	}
}

#endif  /* BCMDBG */

#endif /* AMPDU_NON_AQM */

int
wlc_dump_aggfifo(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	osl_t	*osh = wlc->osh;
	int	ret = BCME_OK;
	char	*buf = NULL;
	struct  bcmstrbuf bstr;

	if (!b) {
#define AMPDU_DUMP_LEN (4 * 1024)

		buf = MALLOCZ(wlc->osh, AMPDU_DUMP_LEN);
		bcm_binit(&bstr, buf, AMPDU_DUMP_LEN);
		b = &bstr;
	}

	if (!wlc->clk) {
		bcm_bprintf(b, "%s: clk off\n", __FUNCTION__);
		goto done;
	}

	if (!buf) {
		bcm_bprintf(b, "%s:\n", __FUNCTION__);
	}

	if (AMPDU_AQM_ENAB(wlc->pub)) {
		if (D11REV_GE(wlc->pub->corerev, 128)) {
			bcm_bprintf(b, "aqmqmap 0x%x aqmfifo_status 0x%x\n",
				R_REG(osh, D11_AQM_QMAP(wlc)),
				R_REG(osh, D11_MQF_STATUS(wlc)));
			bcm_bprintf(b, "AQM agg params 0x%x maxlen hi/lo 0x%x 0x%x "
				"minlen 0x%x maxnum 0x%x\n",
				R_REG(osh, D11_AQM_AGGPARM(wlc)),
				R_REG(osh, D11_AQM_MAXAGGRLEN_H(wlc)),
				R_REG(osh, D11_AQM_MAXAGGRLEN_L(wlc)),
				R_REG(osh, D11_AQM_MINMLEN(wlc)),
				R_REG(osh, D11_AQM_MAXAGGRNUM(wlc)));
			bcm_bprintf(b, "AQM agg results 0x%x num %d len hi/lo 0x%x 0x%x "
				"dmlen 0x%x mmlen 0x%x cmlen 0x%x\n",
				R_REG(osh, D11_AQM_QAGGSTATS(wlc)),
				R_REG(osh, D11_AQM_QAGGNUM(wlc)),
				R_REG(osh, D11_AQM_QAGGLEN_H(wlc)),
				R_REG(osh, D11_AQM_QAGGLEN_L(wlc)),
				R_REG(osh, D11_AQM_DMLEN(wlc)),
				R_REG(osh, D11_AQM_MMLEN(wlc)),
				R_REG(osh, D11_AQM_CMLEN(wlc)));
			bcm_bprintf(b, "AQM agg basel 0x%x BAbitmap(0-3) %x %x %x %x\n",
				R_REG(osh, D11_AQM_BASEL(wlc)),
				R_REG(osh, D11_AQM_UPDBA0(wlc)),
				R_REG(osh, D11_AQM_UPDBA1(wlc)),
				R_REG(osh, D11_AQM_UPDBA2(wlc)),
				R_REG(osh, D11_AQM_UPDBA3(wlc)));
			bcm_bprintf(b, "AQM agg rdptr 0x%x mpdu_len 0x%x txcnt 0x%x "
				"mpdu_info0 0x%x mpdu_info1 0x%x q_info 0x%x\n",
				R_REG(osh, D11_AQM_AGGRPTR(wlc)),
				R_REG(osh, D11_AQM_QMPDULEN(wlc)),
				R_REG(osh, D11_AQM_QTXCNT(wlc)),
				R_REG(osh, D11_AQM_QMPDUINFO0(wlc)),
				R_REG(osh, D11_AQM_QMPDUINFO1(wlc)),
				R_REG(osh, D11_AQM_QAGGINFO(wlc)));
		} else if (D11REV_GE(wlc->pub->corerev, 64)) {
			bcm_bprintf(b, "psm_reg_mux 0x%x aqmqmap 0x%x aqmfifo_status 0x%x\n",
				R_REG(osh, D11_PSM_REG_MUX(wlc)),
				R_REG(osh, D11_AQMQMAP(wlc)),
				R_REG(osh, D11_AQMFifo_Status(wlc)));
			bcm_bprintf(b, "AQM agg params 0x%x maxlen hi/lo 0x%x 0x%x "
				"minlen 0x%x adjlen 0x%x\n",
				R_REG(osh, D11_AQMAggParams(wlc)),
				R_REG(osh, D11_AQMMaxAggLenHi(wlc)),
				R_REG(osh, D11_AQMMaxAggLenLow(wlc)),
				R_REG(osh, D11_AQMMinMpduLen(wlc)),
				R_REG(osh, D11_AQMMacAdjLen(wlc)));
			bcm_bprintf(b, "AQM agg results 0x%x num %d len hi/lo 0x%x 0x%x "
				"BAbitmap(0-3) %x %x %x %x\n",
				R_REG(osh, D11_AQMAggStats(wlc)),
				R_REG(osh, D11_AQMAggNum(wlc)),
				R_REG(osh, D11_AQMAggLenHi(wlc)),
				R_REG(osh, D11_AQMAggLenLow(wlc)),
				R_REG(osh, D11_AQMUpdBA0(wlc)),
				R_REG(osh, D11_AQMUpdBA1(wlc)),
				R_REG(osh, D11_AQMUpdBA2(wlc)),
				R_REG(osh, D11_AQMUpdBA3(wlc)));
			bcm_bprintf(b, "AQM agg rdptr %d mpdu_len 0x%x idx 0x%x info 0x%x\n",
			       R_REG(osh, D11_AQMAggRptr(wlc)),
			       R_REG(osh, D11_AQMMpduLen(wlc)),
			       R_REG(osh, D11_AQMAggIdx(wlc)),
			       R_REG(osh, D11_AQMAggEntry(wlc)));
		} else {
			bcm_bprintf(b, "framerdy 0x%x bmccmd %d framecnt 0x%x \n",
				R_REG(osh, D11_AQMFifoReady(wlc)),
				R_REG(osh, D11_BMCCmd(wlc)),
				R_REG(osh, D11_XmtFifoFrameCnt(wlc)));
			bcm_bprintf(b, "AQM agg params 0x%x maxlen hi/lo 0x%x 0x%x "
				"minlen 0x%x adjlen 0x%x\n",
				R_REG(osh, D11_AQMAggParams(wlc)),
				R_REG(osh, D11_AQMMaxAggLenHi(wlc)),
				R_REG(osh, D11_AQMMaxAggLenLow(wlc)),
				R_REG(osh, D11_AQMMinMpduLen(wlc)),
				R_REG(osh, D11_AQMMacAdjLen(wlc)));
			bcm_bprintf(b, "AQM agg results 0x%x len hi/lo 0x%x 0x%x "
				"BAbitmap(0-3) %x %x %x %x\n",
				R_REG(osh, D11_AQMAggStats(wlc)),
				R_REG(osh, D11_AQMAggLenHi(wlc)),
				R_REG(osh, D11_AQMAggLenLow(wlc)),
				R_REG(osh, D11_AQMUpdBA0(wlc)),
				R_REG(osh, D11_AQMUpdBA1(wlc)),
				R_REG(osh, D11_AQMUpdBA2(wlc)),
				R_REG(osh, D11_AQMUpdBA3(wlc)));
		}
		goto done;
	}

#ifdef AMPDU_NON_AQM

#ifdef WLAMPDU_UCODE
	if (AMPDU_UCODE_ENAB(wlc->pub)) {
		int i;
		ampdu_tx_info_t* ampdu_tx = wlc->ampdu_tx;
		for (i = 0; i < 4; i++) {
			int k = 0, addr;
			bcm_bprintf(b, "fifo %d: rptr %x wptr %x\n",
			       i, ampdu_tx->hwagg[i].txfs_rptr,
			       ampdu_tx->hwagg[i].txfs_wptr);
			for (addr = ampdu_tx->hwagg[i].txfs_addr_strt;
			     addr <= ampdu_tx->hwagg[i].txfs_addr_end; addr++) {
				bcm_bprintf(b, "\tentry %d addr 0x%x: 0x%x\n",
				       k, addr, wlc_read_shm(wlc, addr * 2));
				k++;
			}
		}
	}
#endif /* WLAMPDU_UCODE */
#if defined(WLCNT)
	bcm_bprintf(b, "driver statistics: aggfifo pending %d enque/cons %d %d\n",
	       wlc->ampdu_tx->cnt->pending,
	       wlc->ampdu_tx->cnt->enq,
	       wlc->ampdu_tx->cnt->cons);
#endif // endif

#endif /* AMPDU_NON_AQM */

done:
	if (buf) {
		MFREE(wlc->osh, buf, AMPDU_DUMP_LEN);
	}

	return ret;
}

/**
 * Called when the number of ADDBA retries has been exceeded, thus the remote party is not
 * responding, thus transmit packets for that remote party 'ini' have to be flushed.
 */
static void
ampdu_ba_state_off(scb_ampdu_tx_t *scb_ampdu, scb_ampdu_tid_ini_t *ini)
{
	void *p;
	struct scb *scb = scb_ampdu->scb;

	ini->ba_state = AMPDU_TID_STATE_BA_OFF;
	scb_ampdu->tidmask &= ~(1 << ini->tid);

	/* release all buffered pkts */
	while ((p = _wlc_ampdu_pktq_pdeq(&scb_ampdu->txq, ini->tid))) {
		ASSERT(PKTPRIO(p) == ini->tid);
#ifdef WLCFP
		if (PKTISCFP(p)) {
			/* Packet enqueued by CFP: Release by legacy path */
			/* Reset CFP flags now */
			PKTCLRCFPFLOWID(p, CFP_FLOWID_INVALID);
		}
#endif /* WLCFP */

		SCB_TX_NEXT(TXMOD_AMPDU, scb, p, WLC_PRIO_TO_PREC(ini->tid));
	}
	/* XXX SWWLAN-56360 after wlc_detach gets called wlc->txqueues's parameters will be set to
	 * 0xdeadbeefdeadbeef.  However we see an instant where TXQ_STOP_FOR_AMPDU_FLOW_CNTRL
	 * bit get cleared afterward which causes 0xdeadbeefdeadbeef to become 0xdeadbeefdeadbcef,
	 * the ifdef below is a work around to prevent this clear from happening unless it's for
	 * DONGLEBUILD.
	 */
#if defined(DONGLEBUILD)
	wlc_txflowcontrol_override(scb_ampdu->ampdu_tx->wlc, SCB_WLCIFP(scb_ampdu->scb)->qi,
		OFF, TXQ_STOP_FOR_AMPDU_FLOW_CNTRL);
#endif /* DONGLEBUILD */
#ifdef WLTAF
	wlc_taf_link_state(scb_ampdu->ampdu_tx->wlc->taf_handle, scb, ini->tid, TAF_AMPDU,
		TAF_LINKSTATE_NOT_ACTIVE);
#ifdef WLSQS
	wlc_taf_link_state(scb_ampdu->ampdu_tx->wlc->taf_handle, scb, ini->tid, TAF_SQSHOST,
		TAF_LINKSTATE_NOT_ACTIVE);
#endif /* WLSQS */
#endif /* WLTAF */
}

#ifdef PROP_TXSTATUS
/** clear the supression related data in flowring when the ampdu ini is deleted */
static int
ampdu_clear_supr_data(ampdu_tx_info_t *ampdu_tx, uint8 *ea, uint8 tid)
{
	/* space for type(1), length(1) and value */
	uint8	results[1+1+WLFC_CTL_VALUE_LEN_SUPR];

	results[0] = WLFC_CTL_TYPE_CLEAR_SUPPR;
	results[1] = WLFC_CTL_VALUE_LEN_SUPR;
	results[2] = tid;
	memcpy(&results[3], ea, ETHER_ADDR_LEN);

	return wlfc_push_signal_data(ampdu_tx->wlc->wlfc, results, sizeof(results), FALSE);
}
#endif /* PROP_TXSTATUS */

/** Callback. Cancels the callback for a specific packet. */
static bool
wlc_ampdu_map_pkts_cb(void *ctx, void *pkt)
{
	ampdu_tx_map_pkts_cb_params_t *cb_params = ctx;
	struct scb *scb = cb_params->scb;
	int tid = cb_params->tid;

	if ((scb != WLPKTTAGSCBGET(pkt)) ||
		(tid != PKTPRIO(pkt)) ||
		(!WLF2_PCB4(pkt)) ||
		!(WLPKTTAG(pkt)->flags & WLF_AMPDU_MPDU)) {
		return FALSE;
	}
	else {
		ampdu_tx_info_t *ampdu_tx;
		scb_ampdu_tx_t *scb_ampdu;
		scb_ampdu_tid_ini_t *ini;

		/* Disable the AMPDU packet callback for the packet */
		WLF2_PCB4_UNREG(pkt);

		/* Clear tx_in_transit */
		ampdu_tx = cb_params->ampdu_tx;
		scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
		ASSERT(scb_ampdu);

		ini = scb_ampdu->ini[tid];
		if (ini == NULL) {
			WL_ERROR(("%s: ini is NULL\n", __FUNCTION__));
			return FALSE;
		}

		ASSERT(ini->tx_in_transit);
		ini->tx_in_transit--;

#ifdef WLATF
		ASSERT(ampdu_tx->tx_intransit != 0);
		ampdu_tx->tx_intransit--;
#endif /* WLATF */

#ifdef PROP_TXSTATUS
		if (PROP_TXSTATUS_ENAB(ampdu_tx->wlc->pub) &&
		   WLFC_GET_REUSESEQ(wlfc_query_mode(ampdu_tx->wlc->wlfc))) {
			wlc_pkttag_t *tag = WLPKTTAG(pkt);
			RESET_WL_HAS_ASSIGNED_SEQ(tag->seq);
		}
#endif /* PROP_TXSTATUS */
	}

	return FALSE;
} /* wlc_ampdu_map_pkts_cb */

/** Cancellation of packet callbacks for a specific TID/QoS category */
static void
wlc_ampdu_cancel_pkt_callbacks(ampdu_tx_info_t *ampdu_tx, struct scb *scb, uint8 tid)
{
	ampdu_tx_map_pkts_cb_params_t cb_params;
	wlc_info_t *wlc = ampdu_tx->wlc;
	wlc_txq_info_t *qi;

	cb_params.scb = scb;
	cb_params.tid = tid;
	cb_params.ampdu_tx = ampdu_tx;

	for (qi = wlc->tx_queues; qi != NULL; qi = qi->next) {
		wlc_low_txq_map_pkts(wlc, qi, wlc_ampdu_map_pkts_cb, &cb_params);
		wlc_txq_map_pkts(wlc, qi, wlc_ampdu_map_pkts_cb, &cb_params);
	}

#ifdef AP
	/* traverses tx packets on low q, high q, etc */
	wlc_apps_map_pkts(wlc, scb, wlc_ampdu_map_pkts_cb, &cb_params);
#endif // endif

#ifdef WL_BSSCFG_TX_SUPR
	if (scb->bsscfg != NULL && wlc_bsscfg_get_psq(wlc->psqi, scb->bsscfg) != NULL)
		wlc_bsscfg_psq_map_pkts(wlc, wlc_bsscfg_get_psq(wlc->psqi, scb->bsscfg),
			wlc_ampdu_map_pkts_cb, &cb_params);
#endif // endif

	wlc_dma_map_pkts(wlc->hw, wlc_ampdu_map_pkts_cb, &cb_params);
} /* wlc_ampdu_cancel_pkt_callbacks */

/**
 * Called when tearing down a connection with a conversation partner (DELBA or otherwise)
 *
 * @param[in] force  When this site has sent a DELBA to the remote party, it waits for the remote
 *                   party to acknowledge that DELBA, so that an orderly teardown takes place.
 *                   During that phase, force=FALSE. In all other cases, force=TRUE.
 */
void
ampdu_cleanup_tid_ini(ampdu_tx_info_t *ampdu_tx, struct scb *scb, uint8 tid, bool force)
{
	wlc_info_t *wlc = ampdu_tx->wlc;
	scb_ampdu_tx_t *scb_ampdu;
	scb_ampdu_tid_ini_t *ini;

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	ASSERT(scb_ampdu);
	ASSERT(scb_ampdu->scb);

	AMPDU_VALIDATE_TID(ampdu_tx, tid, "ampdu_cleanup_tid_ini");

	if (!(ini = scb_ampdu->ini[tid]))
		return;

	WL_AMPDU_CTL(("wl%d: ampdu_cleanup_tid_ini: tid %d force %d tx_in_transit %u rem_window %u "
	              "ba_state %u\n",
	              ampdu_tx->wlc->pub->unit, tid, force, ini->tx_in_transit,
	              wlc_ampdu_get_rem_window(ini), ini->ba_state));

	ini->ba_state = AMPDU_TID_STATE_BA_PENDING_OFF;
	scb_ampdu->tidmask &= ~(1 << tid);

	WL_AMPDU_CTL(("wl%d: ampdu_cleanup_tid_ini: tid %d force %d\n",
		wlc->pub->unit, tid, force));
#ifdef WLCFP
	/**
	 * Since ini is getting removed from this point,
	 * Mark the appropriate state changes into CFP layer too
	 */
	if (CFP_ENAB(wlc->pub) == TRUE) {
		wlc_cfp_tcb_upd_ini_state(wlc, scb, ini->tid, FALSE);
	}
#endif /* WLCFP */

#ifdef WLTAF
	wlc_taf_link_state(wlc->taf_handle, scb, tid, TAF_AMPDU, TAF_LINKSTATE_CLEAN);
#ifdef WLSQS
	wlc_taf_link_state(wlc->taf_handle, scb, tid, TAF_SQSHOST, TAF_LINKSTATE_CLEAN);
#endif /* WLSQS */
#endif /* WLTAF */

#ifdef WLWRR
	wlc_ampdu_wrr_clear_counters(ini);
#endif // endif

#ifdef WLATF
	wlc_ampdu_atf_reset_state(ini);
#endif /* WLATF */

	if (RATELINKMEM_ENAB(wlc->pub)) {
		wlc_ratelinkmem_update_link_entry(wlc, scb);
	}

	if (ini->tx_in_transit && !force)
		return;

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, ini->scb);

	if (scb_ampdu == NULL) {
		WL_ERROR(("wl%d:%s: scb_ampdu == NULL, bail out\n", wlc->pub->unit, __FUNCTION__));
		ASSERT(scb_ampdu);
	}

	ASSERT(ini == scb_ampdu->ini[ini->tid]);

	/* free all buffered tx packets */
	wlc_ampdu_pktq_pflush(wlc, &scb_ampdu->txq, ini->tid);

	/* Cancel any pending wlc_send_bar_complete packet callbacks. */
	wlc_pcb_fn_find(ampdu_tx->wlc->pcb, wlc_send_bar_complete, ini, TRUE);

	/* Disable the AMPDU packet callback for packets found in the map */
	wlc_ampdu_cancel_pkt_callbacks(ampdu_tx, scb, tid);

	/* tx_in_transit should now be 0. If called from wlc_down, packet callbacks are disabled
	 * and tx_in_transit might not be 0.
	 */
#ifdef WLATF // JIRA:BCAWLAN-205516 RB:157899 Bug fix related to low water mark check
	if (ini->tx_in_transit && force) {
		ASSERT(ampdu_tx->tx_intransit >= ini->tx_in_transit);
		ampdu_tx->tx_intransit -= ini->tx_in_transit;
	}
#endif /* WLATF */

	ASSERT(wlc->state == WLC_STATE_GOING_DOWN || ini->tx_in_transit == 0 ||
	       SCB_DEL_IN_PROGRESS(scb));

	/* Free ini immediately as no callbacks are pending */
	scb_ampdu->ini[ini->tid] = NULL;

	MFREE(ampdu_tx->wlc->osh, ini, SCB_AMPDU_TID_INI_SZ);

#ifdef WLTAF
	wlc_taf_link_state(wlc->taf_handle, scb, tid, TAF_AMPDU, TAF_LINKSTATE_REMOVE);
#ifdef WLSQS
	wlc_taf_link_state(wlc->taf_handle, scb, tid, TAF_SQSHOST, TAF_LINKSTATE_REMOVE);
#endif /* WLSQS */
#endif /* WLTAF */

	/* XXX SWWLAN-56360 after wlc_detach gets called wlc->txqueues's parameters will be set to
	 * 0xdeadbeefdeadbeef.  However we see an instant where TXQ_STOP_FOR_AMPDU_FLOW_CNTRL
	 * bit get cleared afterward which causes 0xdeadbeefdeadbeef to become 0xdeadbeefdeadbcef,
	 * the ifdef below is a work around to prevent this clear from happening unless it's for
	 * DONGLEBUILD.
	 */
#if defined(DONGLEBUILD)
	wlc_txflowcontrol_override(ampdu_tx->wlc, SCB_WLCIFP(scb_ampdu->scb)->qi,
	                           OFF, TXQ_STOP_FOR_AMPDU_FLOW_CNTRL);
#endif /* DONGLEBUILD */
#ifdef PROP_TXSTATUS
	if (PROP_TXSTATUS_ENAB(ampdu_tx->wlc->pub))
		ampdu_clear_supr_data(ampdu_tx, &scb->ea.octet[0], tid);
#endif /* PROP_TXSTATUS */
} /* ampdu_cleanup_tid_ini */

void
wlc_ampdu_recv_addba_resp(ampdu_tx_info_t *ampdu_tx, struct scb *scb, uint8 *body, int body_len)
{
	scb_ampdu_tx_t *scb_ampdu_tx;
	wlc_info_t *wlc = ampdu_tx->wlc;
	ampdu_tx_config_t *ampdu_tx_cfg = ampdu_tx->config;
	struct pktq *common_q;			/**< multi priority packet queue */
	dot11_addba_resp_t *addba_resp;
	scb_ampdu_tid_ini_t *ini;
	uint16 param_set, status;
	uint8 tid, wsize, policy;
	uint16 current_wlctxq_len = 0, i = 0;
	void *p = NULL;
	wlc_txq_info_t *qi = NULL;

	BCM_REFERENCE(body_len);

	ASSERT(scb);

	scb_ampdu_tx = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	ASSERT(scb_ampdu_tx);

	addba_resp = (dot11_addba_resp_t *)body;

	if (addba_resp->category & DOT11_ACTION_CAT_ERR_MASK) {
		WL_AMPDU_ERR(("wl%d: %s: unexp error action frame\n",
				wlc->pub->unit, __FUNCTION__));
		WLCNTINCR(ampdu_tx->cnt->rxunexp);
		return;
	}

	status = ltoh16_ua(&addba_resp->status);
	param_set = ltoh16_ua(&addba_resp->addba_param_set);

	wsize =	(param_set & DOT11_ADDBA_PARAM_BSIZE_MASK) >> DOT11_ADDBA_PARAM_BSIZE_SHIFT;
	policy = (param_set & DOT11_ADDBA_PARAM_POLICY_MASK) >> DOT11_ADDBA_PARAM_POLICY_SHIFT;
	tid = (param_set & DOT11_ADDBA_PARAM_TID_MASK) >> DOT11_ADDBA_PARAM_TID_SHIFT;
	AMPDU_VALIDATE_TID(ampdu_tx, tid, "wlc_ampdu_recv_addba_resp");

	ini = scb_ampdu_tx->ini[tid];

	if ((ini == NULL) || (ini->ba_state != AMPDU_TID_STATE_BA_PENDING_ON)) {
		WL_AMPDU_CTL(("wl%d: wlc_ampdu_recv_addba_resp: unsolicited packet\n",
			wlc->pub->unit));
		WLCNTINCR(ampdu_tx->cnt->rxunexp);
		return;
	}

	if ((status != DOT11_SC_SUCCESS) ||
	    (policy != ampdu_tx_cfg->ba_policy) ||
	    (wsize > ampdu_tx_cfg->ba_max_tx_wsize)) {
		WL_AMPDU_ERR(("wl%d: %s: Failed. status %d wsize %d policy %d\n",
			wlc->pub->unit, __FUNCTION__, status, wsize, policy));
		ampdu_ba_state_off(scb_ampdu_tx, ini);
		WLCNTINCR(ampdu_tx->cnt->rxunexp);
		return;
	}

#ifdef WLAMSDU_TX
	if (AMSDU_TX_ENAB(wlc->pub)) {
		/* Record whether this scb supports amsdu over ampdu */
		scb->flags2 &= ~SCB2_AMSDU_IN_AMPDU_CAP;
		if ((param_set & DOT11_ADDBA_PARAM_AMSDU_SUP) != 0) {
			scb->flags2 |= SCB2_AMSDU_IN_AMPDU_CAP;

#ifdef WL_MU_TX
			if (SCB_MU(scb)) {
				int8 prev_max_pdu = scb_ampdu_tx->max_pdu;
				/* With amsdu_in_ampdu, scb max_pdu may change */
				scb_ampdu_update_config(ampdu_tx, scb);
				if (WLC_TXC_ENAB(ampdu_tx->wlc) &&
					(prev_max_pdu != scb_ampdu_tx->max_pdu)) {
					/* If max_pdu changes, invalid tx cache */
					wlc_txc_inv_all(ampdu_tx->wlc->txc);
				}
			}
#endif // endif
		}
	}
#endif /* WLAMSDU_TX */

	ini->ba_wsize = wsize;
#ifndef WLAMPDU_AQM
	ini->non_aqm->rem_window = wsize;
#endif /* !WLAMPDU_AQM */

#ifdef PROP_TXSTATUS
	ini->suppr_window = 0; /* suppr packet count inside ba window */
#endif /* PROP_TXSTATUS */

	ini->start_seq = (SCB_SEQNUM(scb, tid) & (SEQNUM_MAX - 1));
	ini->tx_exp_seq = ini->start_seq;
	ini->acked_seq = ini->max_seq = MODSUB_POW2(ini->start_seq, 1, SEQNUM_MAX);
	ini->ba_state = AMPDU_TID_STATE_BA_ON;
	ini->barpending_seq = SEQNUM_MAX;
	scb_ampdu_tx->tidmask |= (1 << tid);
#ifdef WLATF
	wlc_ampdu_atf_reset_state(ini);
#endif /* WLATF */
#ifdef WLTAF
	wlc_taf_link_state(wlc->taf_handle, scb, tid, TAF_AMPDU, TAF_LINKSTATE_ACTIVE);
#ifdef WLSQS
	wlc_taf_link_state(wlc->taf_handle, scb, tid, TAF_SQSHOST, TAF_LINKSTATE_ACTIVE);
#endif /* WLSQS */
#endif /* WLTAF */

#ifdef WLWRR
	wlc_ampdu_wrr_clear_counters(ini);
#endif // endif

	if (RATELINKMEM_ENAB(wlc->pub)) {
		wlc_ratelinkmem_update_link_entry(wlc, scb);
	}

	WLCNTINCR(ampdu_tx->cnt->rxaddbaresp);
	AMPDUSCBCNTINCR(scb_ampdu_tx->cnt.rxaddbaresp);

	WL_AMPDU_CTL(("wl%d: wlc_ampdu_recv_addba_resp: Turning BA ON: tid %d wsize %d\n",
		wlc->pub->unit, tid, wsize));

	/* send bar to set initial window */
	wlc_ampdu_send_bar(ampdu_tx, ini, ini->start_seq);

#ifdef WLCFP
	if (CFP_ENAB(wlc->pub) == TRUE) {
		/*
		 *  CFP path depends AMSDU_IN_AMPDU Capability
		 *  Update CFP state if scb flags reflect this change
		 */
		wlc_cfp_scb_state_upd(wlc->cfp, scb);

		/**
		 * Since ADD-BA exchanges are over,
		 * Set ini valid for the CFP cubby state
		 */
		wlc_cfp_tcb_upd_ini_state(wlc, scb, tid, TRUE);
	}
#endif /* WLCFP */

	/* XXX:TXQ
	 * WES: This code scrubs the TxQ for pkts with this SCB, but now the big TxQ is eliminated
	 * WES: and pkts should already be on the SCB specific Q.
	 * WES: Only pkts in the low TxQ and DMA ring might
	 * WES: exist. The existing code does not clean these up,
	 * WES: so ditching this code when we switch
	 * WES: to SCB specific Qs should not introduce any new problems.
	 * WES: Maybe there are not packets previously queued in the TxQ,
	 * WES: in practice since AMPDU is configured.
	 * WES: early and holds traffic until ADDBA is done?
	 * WES: When would there be a transition between non-BA frames and BA agreement starting?
	*/

	/* if packets already exist in wlcq, conditionally transfer them to ampduq
		to avoid barpending stuck issue.
	*/
	qi = SCB_WLCIFP(scb)->qi;
	common_q = WLC_GET_CQ(qi);

	current_wlctxq_len = pktqprec_n_pkts(common_q, WLC_PRIO_TO_PREC(tid));

	for (i = 0; i < current_wlctxq_len; i++) {
		p = cpktq_pdeq(&qi->cpktq, WLC_PRIO_TO_PREC(tid));

		if (!p) break;

		/* omit control/mgmt frames and queue them to the back
		 * only the frames relevant to this scb should be moved to ampduq;
		 * otherwise, queue them back.
		 */
		if (!(WLPKTTAG(p)->flags & WLF_DATA) || /* mgmt/ctl */
		    scb != WLPKTTAGSCBGET(p) || /* frame belong to some other scb */
#if defined(PROP_TXSTATUS)
			(AMPDU_HOST_ENAB(wlc->pub) &&
			GET_DRV_HAS_ASSIGNED_SEQ(WLPKTTAG(p)->seq)) || /* suppressed packet */
#endif /* PROP_TXSTATUS */
			(WLPKTTAG(p)->flags & WLF_AMPDU_MPDU)) { /* possibly retried AMPDU */

			/* WES: Direct TxQ reference */
			if (cpktq_prec_enq(wlc, &qi->cpktq, p, WLC_PRIO_TO_PREC(tid))) {
				continue;
			}
		} else { /* XXXCheck if Flow control/watermark issues */
			/* WLF_BYPASS_TXC is set when BA session is still not ready.
			 * Here since we have allready recieved BA response,
			 * this packet could take up a cached path
			 */
			WLPKTTAG(p)->flags &= ~WLF_BYPASS_TXC;
			if (wlc_ampdu_prec_enq(wlc, &scb_ampdu_tx->txq, p, tid))
				continue;
		}

		/* Toss packet as queuing failed */
		WL_AMPDU_ERR(("wl%d: txq overflow\n", wlc->pub->unit));
		PKTFREE(wlc->osh, p, TRUE);
		WLCNTINCR(wlc->pub->_cnt->txnobuf);
		WLCNTINCR(ampdu_tx->cnt->txdrop);
		AMPDUSCBCNTINCR(scb_ampdu_tx->cnt.txdrop);
		WLCIFCNTINCR(scb, txnobuf);
		WLCNTSCBINCR(scb->scb_stats.tx_failures);
	}

#ifdef WLNAR
	wlc_nar_release_all_from_queue(wlc->nar_handle, scb, WLC_PRIO_TO_PREC(tid));
#endif /* WLNAR */

#ifdef WLTAF
	if (wlc_taf_in_use(wlc->taf_handle)) {
		return;
	}
#endif /* WLTAF */
	/* release pkts */
	wlc_ampdu_txeval(ampdu_tx, scb_ampdu_tx, ini, FALSE);
} /* wlc_ampdu_recv_addba_resp */

void
wlc_ampdu_recv_ba(ampdu_tx_info_t *ampdu_tx, struct scb *scb, uint8 *body, int body_len)
{
	BCM_REFERENCE(scb);
	BCM_REFERENCE(body);
	BCM_REFERENCE(body_len);
	/* AMPDUXXX: silently ignore the ba since we don't handle it for immediate ba */
	WLCNTINCR(ampdu_tx->cnt->rxba);
}

#ifdef WLC_AMPDU_DUMP
#if (defined(WLATF) && defined(WLCNT) && defined(BCMDBG))
static BCMFASTPATH void wlc_ampdu_update_rstat(ampdu_stats_range_t *range, uint32 val)
{
	unsigned long accum = range->accum + val;
	uint32 iter = range->iter + 1;

	if (val > range->max) {
		range->max = val;
	}

	if ((val < range->min) || (range->min == 0)) {
		range->min = val;
	}

	/* Overflow check */
	if ((accum < val) || (!iter)) {
		accum = val;
		iter = 1;
	}
	range->accum = accum;
	range->iter = iter;
}

static uint wlc_ampdu_rstat_avg(ampdu_stats_range_t *rstat)
{
	rstat->avg = (rstat->iter) ? rstat->accum/rstat->iter : 0;
	return (rstat->avg);
}

static void wlc_ampdu_print_rstat(struct bcmstrbuf *b,
	char *hdr, ampdu_stats_range_t *rstat, char *tlr)
{
	bcm_bprintf(b, "%s(%u/%u/%u)%s", hdr, wlc_ampdu_rstat_avg(rstat),
		rstat->min, rstat->max, tlr);
}
#endif /* ((defined(WLATF) && defined(WLCNT) && defined(BCMDBG)) || defined (WLWRR)) */
#endif /* WLA_AMPDU_DUMP */

#ifdef WLWRR
static BCMFASTPATH void wlc_ampdu_wrr_update_rstat(ampdu_stats_range_t *range, uint32 val)
{
	unsigned long accum = range->accum + val;
	uint32 iter = range->iter + 1;

	if (val > range->max) {
		range->max = val;
	}

	if ((val < range->min) || (range->min == 0)) {
		range->min = val;
	}

	/* Overflow check */
	if ((accum < val) || (!iter)) {
		accum = val;
		iter = 1;
	}
	range->accum = accum;
	range->iter = iter;
}
static void wlc_ampdu_wrr_clear_counters(scb_ampdu_tid_ini_t *ini)
{
	bzero(AMPDU_WRR_STATS(ini), sizeof(wrr_ampdu_stats_t));
}

#ifdef WLC_AMPDU_DUMP
static uint wlc_ampdu_wrr_rstat_avg(ampdu_stats_range_t *rstat)
{
	rstat->avg = (rstat->iter) ? rstat->accum/rstat->iter : 0;
	return (rstat->avg);
}

static void wlc_ampdu_wrr_print_rstat(struct bcmstrbuf *b,
	char *hdr, ampdu_stats_range_t *rstat, char *tlr)
{
	bcm_bprintf(b, "%s(%u/%u/%u)%s", hdr, wlc_ampdu_wrr_rstat_avg(rstat),
		rstat->min, rstat->max, tlr);
}
static void wlc_ampdu_wrr_dump(scb_ampdu_tid_ini_t *ini, struct bcmstrbuf *b)
{

	wrr_ampdu_stats_t *wrr_stats = &ini->wrr_stats;

	bcm_bprintf(b, "\t WRR Stats: avg/min/max\n");

	wlc_ampdu_wrr_print_rstat(b,
				"\t  ncons", &wrr_stats->ncons, "\n");
	wlc_ampdu_wrr_print_rstat(b,
				"\t  meas txdur", &wrr_stats->txdur, " ");
	wlc_ampdu_wrr_print_rstat(b,
				"frmsz", &wrr_stats->frmsz, "\n");

#if (defined(WLATF) && defined(WLCNT) && defined(BCMDBG))
	if (AMPDU_ATF_STATE(ini)->atf) {
		atf_stats_t *atf_stats =  AMPDU_ATF_STATS(ini);

		bcm_bprintf(b, "\t ATF Stats: avg/min/max\n");

		wlc_ampdu_print_rstat(b,
					"\t  rbytes", &atf_stats->rbytes, "\n");
		wlc_ampdu_print_rstat(b,
					"\t  in_flt", &atf_stats->inflight, "\n");
		wlc_ampdu_print_rstat(b,
					"\t  inputq", &atf_stats->inputq, "\n");

		wlc_ampdu_print_rstat(b,
					"\t  input_rel", &atf_stats->input_rel, "\n");

		wlc_ampdu_print_rstat(b,
					"\t  rpkts", &atf_stats->release, "  ");
		wlc_ampdu_print_rstat(b,
					"transit", &atf_stats->transit, "\n");

		wlc_ampdu_print_rstat(b,
					"\t  chunk bytes", &atf_stats->chunk_bytes, " ");
		wlc_ampdu_print_rstat(b,
					"pkts", &atf_stats->chunk_pkts, " ");

		wlc_ampdu_print_rstat(b,
					"pdu", &atf_stats->pdu, "\n");

		wlc_ampdu_print_rstat(b,
			"\t  rate", &atf_stats->last_est_rate, "\n");
		}
#endif /* WLATF */

}

int
wlc_ampdu_dump_wrr(void *ctx, struct bcmstrbuf *b)
{
	wlc_info_t *wlc = ctx;
	struct scb *scb;
	struct scb_iter scbiter;
	uint32 count = 0;

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		char eabuf[ETHER_ADDR_STR_LEN];
		scb_ampdu_tx_t *scb_ampdu;
		uint8 tid;
		scb_ampdu_tid_ini_t *ini;
		if (!SCB_AMPDU(scb)) {
			continue;
		}

		scb_ampdu = SCB_AMPDU_TX_CUBBY(wlc->ampdu_tx, scb);
		tid = PRIO_8021D_BE; /* Dump best effort only */
		ini = scb_ampdu->ini[tid];

		if ((ini) && (ini->ba_state == AMPDU_TID_STATE_BA_ON)) {
			char *txtype;
			if (SCB_DLOFDMA(scb)) {
				txtype = "HE";
			} else if (SCB_MU(scb)) {
				txtype = "MU";
			} else {
				txtype = "SU";
			}
			bcm_bprintf(b, "%s: (%s)\n",
				bcm_ether_ntoa(&scb->ea, eabuf), txtype);
			wlc_ampdu_wrr_dump(ini, b);
			count++;
		}
	}
	bcm_bprintf(b, "WRR active: %u\n", count);
	return BCME_OK;
}

#endif /* WLC_AMPDU_DUMP */
#endif /* WLWRR */

#ifdef WLATF
/* ATF counter clearing code */
#if defined(WLCNT) && defined(BCMDBG)
static void wlc_ampdu_atf_clear_counters(scb_ampdu_tid_ini_t *ini)
{
	bzero(AMPDU_ATF_STATS(ini), sizeof(atf_stats_t));
}

static void wlc_ampdu_atf_dump(atf_state_t *atf_state, struct bcmstrbuf *b)
{
				bcm_bprintf(b, "\tatf %d ta %dus tm %dus rso:0x%x\n",
					atf_state->atf,
					atf_state->txq_time_allowance_us,
					atf_state->txq_time_min_allowance_us,
					atf_state->rspec_override);
				if (atf_state->atf) {
				atf_stats_t *atf_stats = &atf_state->atf_stats;

				bcm_bprintf(b, "\t tlimit %d flimit %d uflow %d oflow %d\n",
					atf_stats->timelimited, atf_stats->framelimited,
					atf_stats->uflow, atf_stats->oflow);
				bcm_bprintf(b, "\t reset %d flush %d po %d minrel %d sgl %d\n",
					atf_state->reset, atf_stats->flush,
					atf_stats->reloverride, atf_stats->minrel,
					atf_stats->singleton);
				bcm_bprintf(b, "\t eval %d neval %d dq %d skp %d npkts %d\n",
					atf_stats->eval, atf_stats->neval, atf_stats->ndequeue,
					atf_stats->rskip, atf_stats->npkts);
				bcm_bprintf(b, "\t qe %d proc %d rp %d nA %d\n",
					atf_stats->qempty, atf_stats->proc,
					atf_stats->reproc, atf_stats->reg_mpdu);
				bcm_bprintf(b, "\t cache hit/miss(%d/%d)\n",
					atf_stats->cache_hit, atf_stats->cache_miss);
				bcm_bprintf(b, "\t avg/min/max\n");

				wlc_ampdu_print_rstat(b,
					"\t  rbytes", &atf_stats->rbytes, "\n");
				wlc_ampdu_print_rstat(b,
					"\t  in_flt", &atf_stats->inflight, "\n");
				wlc_ampdu_print_rstat(b,
					"\t  inputq", &atf_stats->inputq, "\n");

				wlc_ampdu_print_rstat(b,
					"\t  input_rel", &atf_stats->input_rel, "\n");
				wlc_ampdu_print_rstat(b,
					"\t  rpkts", &atf_stats->release, "  ");
				wlc_ampdu_print_rstat(b,
					"transit", &atf_stats->transit, "\n");

				wlc_ampdu_print_rstat(b,
					"\t  chunk bytes", &atf_stats->chunk_bytes, " ");
				wlc_ampdu_print_rstat(b,
					"pkts", &atf_stats->chunk_pkts, " ");
				wlc_ampdu_print_rstat(b,
					"pdu", &atf_stats->pdu, "\n");
				}
}
#endif /* if defined(WLCNT) && defined(BCMDBG) */
/* Set per TID atf mode */
static void wlc_ampdu_atf_tid_setmode(scb_ampdu_tid_ini_t *ini, uint32 mode)
{
	wlc_ampdu_atf_reset_state(ini);
	AMPDU_ATF_STATE(ini)->atf = mode;
}

/* Set per SCB atf mode */
static void wlc_ampdu_atf_scb_setmode(ampdu_tx_info_t *ampdu_tx, struct scb *scb, uint32 mode)
{
	uint32 n = 0;
	scb_ampdu_tx_t *scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	for (n = 0; n < AMPDU_MAX_SCB_TID; n++) {
		if (scb_ampdu && scb_ampdu->ini[n])
			wlc_ampdu_atf_tid_setmode(scb_ampdu->ini[n], mode);
	}
}

/* Set global ATF default mode, used each time a TID for a SCB is created */
void wlc_ampdu_atf_set_default_mode(ampdu_tx_info_t *ampdu_tx, scb_module_t *scbstate, uint32 mode)
{
	struct scb *scb;
	struct scb_iter scbiter;

	FOREACHSCB(scbstate, &scbiter, scb) {
		if (SCB_AMPDU(scb)) {
			wlc_ampdu_atf_scb_setmode(ampdu_tx, scb, mode);
#if defined(WLATF) && defined(WLATF_PERC)
			wlc_ampdu_atf_scb_set_release_time(ampdu_tx, scb,
				ampdu_tx->config->txq_time_allowance_us);
#endif // endif
		}
	}
}

/* Set per TID atf txq release time allowance in microseconds */
static void wlc_ampdu_atf_tid_set_release_time(scb_ampdu_tid_ini_t *ini, uint txq_time_allowance_us)
{
	AMPDU_ATF_STATE(ini)->txq_time_allowance_us = txq_time_allowance_us;

#ifdef WL_MU_TX
	ASSERT(ini->scb);
	if ((MU_TX_ENAB(ini->scb->bsscfg->wlc) && SCB_VHTMU(ini->scb)) ||
	    (HE_MMU_ENAB(ini->scb->bsscfg->wlc->pub) && SCB_HEMMU(ini->scb))) {
		/* For MU clients, allow double time to meet performance requirements */
		AMPDU_ATF_STATE(ini)->txq_time_allowance_us *= 2;
	}
#endif /* WL_MU_TX */

	/* Reset the last cached ratespec value in order to force to recalculate the
	 * rbytes using new value of txq_time_allowance_us.
	 */
	AMPDU_ATF_STATE(ini)->last_est_rate = 0;
}

/* Set per SCB atf txq release time allowance in microseconds */
static void wlc_ampdu_atf_scb_set_release_time(ampdu_tx_info_t *ampdu_tx,
	struct scb *scb, uint txq_time_allowance_us)
{
	uint32 n = 0;
	scb_ampdu_tx_t *scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	for (n = 0; n < AMPDU_MAX_SCB_TID; n++) {
		if (scb_ampdu && scb_ampdu->ini[n])
			wlc_ampdu_atf_tid_set_release_time(
				scb_ampdu->ini[n], txq_time_allowance_us);
	}
}

/* Set per SCB atf txq release time allowance in microseconds for all SCBs */
static void wlc_ampdu_atf_set_default_release_time(ampdu_tx_info_t *ampdu_tx,
	scb_module_t *scbstate, uint txq_time_allowance_us)
{
	struct scb *scb;
	struct scb_iter scbiter;
	FOREACHSCB(scbstate, &scbiter, scb) {
		if (SCB_AMPDU(scb)) {
			wlc_ampdu_atf_scb_set_release_time(ampdu_tx, scb, txq_time_allowance_us);
		}
	}
}

/* Set per TID atf txq release minimum time allowance in microseconds */
static void wlc_ampdu_atf_tid_set_release_mintime(scb_ampdu_tid_ini_t *ini, uint allowance_us)
{
	AMPDU_ATF_STATE(ini)->txq_time_min_allowance_us = allowance_us;
	/* Reset the last cached ratespec value in order to force to recalculate the
	 * rbytes using new value of txq_time_min_allowance_us.
	 */
	AMPDU_ATF_STATE(ini)->last_est_rate = 0;
}

/* Set per SCB atf txq release time allowance in microseconds */
static void wlc_ampdu_atf_scb_set_release_mintime(ampdu_tx_info_t *ampdu_tx,
	struct scb *scb, uint allowance_us)
{
	uint32 n = 0;
	scb_ampdu_tx_t *scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	for (n = 0; n < AMPDU_MAX_SCB_TID; n++) {
		if (scb_ampdu && scb_ampdu->ini[n])
			wlc_ampdu_atf_tid_set_release_mintime(
				scb_ampdu->ini[n], allowance_us);
	}
}

/* Set per SCB atf txq release time allowance in microseconds for all SCBs */
static void wlc_ampdu_atf_set_default_release_mintime(ampdu_tx_info_t *ampdu_tx,
	scb_module_t *scbstate, uint allowance_us)
{
	struct scb *scb;
	struct scb_iter scbiter;

	FOREACHSCB(scbstate, &scbiter, scb) {
		if (SCB_AMPDU(scb)) {
			wlc_ampdu_atf_scb_set_release_mintime(ampdu_tx, scb, allowance_us);
		}
	}
}

/* Set the function pointer that returns rate state information */
static void
wlc_ampdu_atf_set_rspec_action(atf_state_t *atf_state, atf_rspec_fn_t fn, void *arg)
{
	atf_state->rspec_action.function = fn;
	atf_state->rspec_action.arg = arg;
}

/* Alternate rate function, used whe the rate selection engine is manually over-ridden */
static ratespec_t BCMFASTPATH
wlc_ampdu_atf_ratespec_override(void *ptr)
{
#ifdef WLATF_FASTRATE
	return ((atf_state_t *)(ptr))->rspec_override;
#else
	atf_state_t *atf_state = (atf_state_t *)(ptr);
	return wlc_scb_ratesel_get_primary(atf_state->wlc, atf_state->scb, NULL);
#endif // endif
}

/* Set the rate override condition in ATF */
static void
wlc_ampdu_atf_ini_set_rspec_action(atf_state_t *atf_state, ratespec_t rspec)
{
	/* Sets the rspec action which is a function and an argument
	 * If rspec is zero default to wlc_ratesel_rawcurspec()
	 */

	if (rspec) {
		wlc_ampdu_atf_set_rspec_action(atf_state,
			wlc_ampdu_atf_ratespec_override, atf_state);
	} else {
		/* Set the default function pointer that returns rate state information */
		wlc_ampdu_atf_set_rspec_action(atf_state,
			(atf_rspec_fn_t)wlc_ratesel_rawcurspec, (void *)atf_state->rcb);
	}

	atf_state->rspec_override = rspec;
}

/* Wrapper function to enable/disable rate overide for all ATF tids in the SCB
 * Called by per-SCB rate override code.
 */
void
wlc_ampdu_atf_scb_rate_override(ampdu_tx_info_t *ampdu_tx, struct scb *scb, ratespec_t rspec)
{
	uint32 n = 0;
	scb_ampdu_tx_t *scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);

	if (!scb_ampdu)
		return;

	for (n = 0; n < AMPDU_MAX_SCB_TID; n++) {
		if (scb_ampdu->ini[n])
			wlc_ampdu_atf_ini_set_rspec_action(
			AMPDU_ATF_STATE(scb_ampdu->ini[n]), rspec);
	}
}
/* Wrapper function to enable/disable rate overide for the entire driver,
 * called by rate override code
 */
void
wlc_ampdu_atf_rate_override(wlc_info_t *wlc, ratespec_t rspec, wlcband_t *band)
{
	/* Process rate update notification */
	struct scb *scb;
	struct scb_iter scbiter;

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (band->bandtype == wlc_scbband(wlc, scb)->bandtype) {
			wlc_ampdu_atf_scb_rate_override(wlc->ampdu_tx, scb, rspec);
		}
	}
}

static void wlc_ampdu_atf_tid_ini(ampdu_tx_info_t *ampdu_tx,
	scb_ampdu_tx_t *scb_ampdu, scb_ampdu_tid_ini_t *ini)
{
	wlc_info_t *wlc = ampdu_tx->wlc;
	atf_state_t *atf_state = AMPDU_ATF_STATE(ini);

	atf_state->ac = WME_PRIO2AC(ini->tid);
	atf_state->band = wlc->bandstate[
		CHSPEC_WLCBANDUNIT(scb_ampdu->scb->bsscfg->current_bss->chanspec)];
	atf_state->rcb = wlc_scb_ratesel_getrcb(wlc, scb_ampdu->scb, WME_PRIO2AC(ini->tid));
	ASSERT(atf_state->rcb);
	atf_state->ampdu_tx = ampdu_tx;
	atf_state->scb_ampdu = scb_ampdu;
	atf_state->ini = ini;
	atf_state->wlc = wlc;
	atf_state->scb = ini->scb;

	/* Set operating state, this resets state machine */
	wlc_ampdu_atf_tid_setmode(ini, wlc->atf);

	/* Reset ATF state machine again, it is assumed the tid is coming from an
	 * unknown and possibly invalid state.
	 */
	wlc_ampdu_atf_reset_state(ini);
	/* Set other operating parameters */
	wlc_ampdu_atf_tid_set_release_time(ini, ampdu_tx->config->txq_time_allowance_us);
	wlc_ampdu_atf_tid_set_release_mintime(ini, ampdu_tx->config->txq_time_min_allowance_us);
}
#else
#define wlc_ampdu_atf_tid_ini(ampdu_tx, scb_ampdu, ini)
#define wlc_ampdu_atf_dump(atf_state, b)
#endif /* WLATF */

/** initialize the initiator code for tid. Called around ADDBA exchange. */
static scb_ampdu_tid_ini_t*
wlc_ampdu_init_tid_ini(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tx_t *scb_ampdu, uint8 tid,
	bool override)
{
	scb_ampdu_tid_ini_t *ini;
	wlc_info_t *wlc = ampdu_tx->wlc;
	ampdu_tx_config_t *ampdu_tx_cfg = ampdu_tx->config;
	struct scb *scb;
	uint8 peer_density;
	uint32 max_rxlen;
	uint8 max_rxlen_factor;
	uint8 i;

	ASSERT(scb_ampdu);
	ASSERT(scb_ampdu->scb);
	ASSERT(SCB_AMPDU(scb_ampdu->scb));
	ASSERT(tid < AMPDU_MAX_SCB_TID);
	scb = scb_ampdu->scb;

	if (scb->mark & (SCB_DEL_IN_PROG | SCB_MARK_TO_DEL)) {
		WL_AMPDU(("%s: scb->mark(%x) SCB DEL_IN_PROG or MARK_TO_DEL set--returning\n",
			__FUNCTION__, scb->mark));
		return NULL;
	}

	if (ampdu_tx_cfg->manual_mode && !override)
		return NULL;

	/* check for per-tid control of ampdu */
	if (!ampdu_tx_cfg->ini_enable[tid])
		return NULL;

	/* AMPDUXXX: No support for dynamic update of density/len from peer */
	/* retrieve the density and max ampdu len from scb */
	wlc_ht_get_scb_ampdu_params(wlc->hti, scb, &peer_density,
		&max_rxlen, &max_rxlen_factor);

	/* our density requirement is for tx side as well */
	scb_ampdu->mpdu_density = MAX(peer_density, ampdu_tx_cfg->mpdu_density);

	/* should call after mpdu_density is init'd */
	wlc_ampdu_init_min_lens(scb_ampdu);

	/* max_rxlen is only used for host aggregation (corerev < 40) */
	scb_ampdu->max_rxlen = max_rxlen;

	scb_ampdu->max_rxfactor = max_rxlen_factor;

	/* initializing the ampdu max duration per module */
	for (i = 0; i < NUM_MODULES; i++)
		scb_ampdu->module_max_dur[i] = AMPDU_MAXDUR_INVALID_VAL;

	scb_ampdu->min_of_max_dur_idx = AMPDU_MAXDUR_INVALID_IDX;

#if defined(WL11AC)
	if (VHT_ENAB_BAND(wlc->pub, wlc->band->bandtype) && SCB_VHT_CAP(scb)) {
		scb_ampdu->max_rxfactor = wlc_vht_get_scb_ampdu_max_exp(wlc->vhti, scb) +
			AMPDU_RX_FACTOR_BASE_PWR;
	}
#endif /* WL11AC */

	scb_ampdu_update_config(ampdu_tx, scb);

	if (scb_ampdu->ini[tid]) {
		ampdu_cleanup_tid_ini(ampdu_tx, scb, tid, TRUE);
	}

	if (!scb_ampdu->ini[tid]) {
		ini = MALLOCZ(wlc->osh, SCB_AMPDU_TID_INI_SZ);
		if (ini == NULL) {
			WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(ampdu_tx->wlc), __FUNCTION__,
				(int)sizeof(scb_ampdu_tid_ini_t), MALLOCED(wlc->osh)));
			return NULL;
		}
#ifdef WLAMPDU_AQM
		ini->non_aqm = NULL;
#else
		ini->non_aqm = (scb_ampdu_tid_ini_non_aqm_t*) ((uint8*)ini +
				sizeof(scb_ampdu_tid_ini_t));
#endif /* WLAMPDU_AQM */
		scb_ampdu->ini[tid] = ini;
#ifdef WLTAF
		wlc_taf_link_state(wlc->taf_handle, scb, tid, TAF_AMPDU, TAF_LINKSTATE_INIT);
#ifdef WLSQS
		wlc_taf_link_state(wlc->taf_handle, scb, tid, TAF_SQSHOST, TAF_LINKSTATE_INIT);
#endif /* WLSQS */
#endif /* WLTAF */
	} else {
		return NULL;
	}

	ini->ba_state = AMPDU_TID_STATE_BA_PENDING_ON;
	ini->tid = tid;
	ini->scb = scb;
	ini->retry_bar = AMPDU_R_BAR_DISABLED;

#ifdef WLATF
	wlc_ampdu_atf_tid_ini(ampdu_tx, scb_ampdu, ini);
#endif // endif

	if (RATELINKMEM_ENAB(wlc->pub)) {
		wlc_ratelinkmem_update_link_entry(wlc, scb);
	}
#if defined(WL_MU_TX)
	wlc_scb_ratesel_init_rcb_itxs(wlc->wrsi, scb, WME_PRIO2AC(tid));
#endif // endif
	wlc_send_addba_req(wlc, ini->scb, tid, ampdu_tx_cfg->ba_tx_wsize,
		ampdu_tx->config->ba_policy, ampdu_tx_cfg->delba_timeout);
	ini->last_addba_ts = OSL_SYSUPTIME();

	WLCNTINCR(ampdu_tx->cnt->txaddbareq);
	AMPDUSCBCNTINCR(scb_ampdu->cnt.txaddbareq);

	return ini;
} /* wlc_ampdu_init_tid_ini */

/** Remote side sent us a ADDBA request */
void
wlc_ampdu_recv_addba_req_ini(ampdu_tx_info_t *ampdu_tx, struct scb *scb,
	dot11_addba_req_t *addba_req, int body_len)
{
	scb_ampdu_tx_t *scb_ampdu_tx;
	scb_ampdu_tid_ini_t *ini;
	uint16 param_set;
	uint8 tid;

	BCM_REFERENCE(body_len);

	scb_ampdu_tx = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	ASSERT(scb_ampdu_tx);

	param_set = ltoh16_ua(&addba_req->addba_param_set);
	tid = (param_set & DOT11_ADDBA_PARAM_TID_MASK) >> DOT11_ADDBA_PARAM_TID_SHIFT;
	AMPDU_VALIDATE_TID(ampdu_tx, tid, "wlc_ampdu_recv_addba_req_ini");

	/* check if it is action err frame */
	if (addba_req->category & DOT11_ACTION_CAT_ERR_MASK) {
		ini = scb_ampdu_tx->ini[tid];
		if ((ini != NULL) && (ini->ba_state == AMPDU_TID_STATE_BA_PENDING_ON)) {
			ampdu_ba_state_off(scb_ampdu_tx, ini);
			WL_AMPDU_ERR(("wl%d: %s: error action frame\n",
				ampdu_tx->wlc->pub->unit, __FUNCTION__));
		} else {
			WL_AMPDU_ERR(("wl%d: %s: unexp error action "
				"frame\n", ampdu_tx->wlc->pub->unit, __FUNCTION__));
		}
		WLCNTINCR(ampdu_tx->cnt->rxunexp);
		return;
	}
}

/** called due to 'wl' utility or as part of the attach phase */
int
wlc_ampdu_tx_set(ampdu_tx_info_t *ampdu_tx, bool on)
{
	wlc_info_t *wlc = ampdu_tx->wlc;
	uint8 ampdu_mac_agg = AMPDU_AGG_OFF;
	uint16 value = 0;
	int err = BCME_OK;
	int8 aggmode;

	wlc->pub->_ampdu_tx = FALSE;

	if (ampdu_tx->config->ampdu_aggmode == AMPDU_AGGMODE_AUTO) {
		if (D11REV_GE(wlc->pub->corerev, 40))
			aggmode = AMPDU_AGGMODE_MAC;
		else
			aggmode = AMPDU_AGGMODE_HOST;
	} else
		aggmode = ampdu_tx->config->ampdu_aggmode;

	if (on) {
		if (!N_ENAB(wlc->pub)) {
			WL_AMPDU_ERR(("wl%d: driver not nmode enabled\n", wlc->pub->unit));
			err = BCME_UNSUPPORTED;
			goto exit;
		}
		if (PIO_ENAB(wlc->pub)) {
			WL_AMPDU_ERR(("wl%d: driver is pio mode\n", wlc->pub->unit));
			err = BCME_UNSUPPORTED;
			goto exit;
		}

		if (aggmode == AMPDU_AGGMODE_MAC) {
			if (D11REV_GE(wlc->pub->corerev, 40)) {
				ampdu_mac_agg = AMPDU_AGG_AQM;
				WL_AMPDU(("wl%d: Corerev is 40 aqm agg\n",
					wlc->pub->unit));
			}

			/* Catch building a particular mode which is not supported by ucode rev */
#if defined(DONGLEBUILD)
#ifdef WLAMPDU_UCODE
			ASSERT(ampdu_mac_agg == AMPDU_AGG_UCODE);
#endif /* WLAMPDU_UCODE */
#ifdef WLAMPDU_AQM
			ASSERT(ampdu_mac_agg == AMPDU_AGG_AQM);
#endif /* WLAMPDU_AQM */
#endif /* DONGLEBUILD */
		}
	}

	/* if aggmode define as MAC, but mac_agg is set to OFF,
	 * then set aggmode to HOST as defaultw
	 */
	if ((aggmode == AMPDU_AGGMODE_MAC) && (ampdu_mac_agg == AMPDU_AGG_OFF))
		aggmode = AMPDU_AGGMODE_HOST;

	/* setup ucode tx-retrys for MAC mode */
	if ((aggmode == AMPDU_AGGMODE_MAC) && (ampdu_mac_agg != AMPDU_AGG_AQM)) {
		value = MHF3_UCAMPDU_RETX;
	}

	if (wlc->pub->_ampdu_tx != on) {
#ifdef WLCNT
		bzero(ampdu_tx->cnt, sizeof(wlc_ampdu_tx_cnt_t));
#endif // endif
		wlc->pub->_ampdu_tx = on;
	}

exit:
	if (wlc->pub->_ampdumac != ampdu_mac_agg) {
		wlc->pub->_ampdumac = ampdu_mac_agg;
		wlc_mhf(wlc, MHF3, MHF3_UCAMPDU_RETX, value, WLC_BAND_ALL);
	}

	/* setup max pkt aggr for MAC mode */
	wlc_set_default_txmaxpkts(wlc);

	WL_AMPDU(("wl%d: %s: %s txmaxpkts %d\n", wlc->pub->unit, __FUNCTION__,
	          ((aggmode == AMPDU_AGGMODE_HOST) ? " AGG Mode = HOST" :
	           ((ampdu_mac_agg == AMPDU_AGG_UCODE) ?
	            "AGG Mode = MAC+Ucode" : "AGG Mode = MAC+AQM")),
	          wlc_get_txmaxpkts(wlc)));

	return err;
} /* wlc_ampdu_tx_set */

#ifdef AMPDU_NON_AQM

/** called during attach phase and as a result of a 'wl' command */
static void
ampdu_update_max_txlen_noaqm(ampdu_tx_config_t *ampdu_tx_cfg, uint16 dur)
{
	uint32 rate;
	int mcs = -1;

	dur = dur >> 10;

#if defined(WLPROPRIETARY_11N_RATES)
	while (TRUE) {
		mcs = NEXT_MCS(mcs); /* iterate through both standard and prop ht rates */
		if (mcs > WLC_11N_LAST_PROP_MCS)
			break;
#else
	for (mcs = 0; mcs < MCS_TABLE_SIZE; mcs++) {
#endif /* WLPROPRIETARY_11N_RATES */
		/* rate is in Kbps; dur is in msec ==> len = (rate * dur) / 8 */
		/* 20MHz, No SGI */
		rate = MCS_RATE(mcs, FALSE, FALSE);
		ampdu_tx_cfg->max_txlen[MCS2IDX(mcs)][0][0] = (rate * dur) >> 3;
		/* 40 MHz, No SGI */
		rate = MCS_RATE(mcs, TRUE, FALSE);
		ampdu_tx_cfg->max_txlen[MCS2IDX(mcs)][1][0] = (rate * dur) >> 3;
		/* 20MHz, SGI */
		rate = MCS_RATE(mcs, FALSE, TRUE);
		ampdu_tx_cfg->max_txlen[MCS2IDX(mcs)][0][1] = (rate * dur) >> 3;
		/* 40 MHz, SGI */
		rate = MCS_RATE(mcs, TRUE, TRUE);
		ampdu_tx_cfg->max_txlen[MCS2IDX(mcs)][1][1] = (rate * dur) >> 3;
	}
}

#endif /* AMPDU_NON_AQM */

/**
 * Applicable to pre-AC chips. Reduces tx underflows on lower MCS rates (e.g. MCS 15) on 40Mhz with
 * frame bursting enabled. If frameburst is enabled, and 40MHz BW, and ampdu_density is smaller than
 * 7, transmit uses adjusted min_bytes.
 *
 * Called during AMPDU connection setup (ADDBA related).
 */
static void
wlc_ampdu_init_min_lens(scb_ampdu_tx_t *scb_ampdu)
{
	int mcs, tmp;
	tmp = 1 << (16 - scb_ampdu->mpdu_density);
#if defined(WLPROPRIETARY_11N_RATES)
	mcs = -1;
	while (TRUE) {
		mcs = NEXT_MCS(mcs); /* iterate through both standard and prop ht rates */
		if (mcs > WLC_11N_LAST_PROP_MCS)
			break;
#else
	for (mcs = 0; mcs <= AMPDU_MAX_MCS; mcs++) {
#endif /* WLPROPRIETARY_11N_RATES */
		scb_ampdu->min_lens[MCS2IDX(mcs)] =  0;
		if (scb_ampdu->mpdu_density > AMPDU_DENSITY_4_US)
			continue;
		if (mcs >= 12 && mcs <= 15) {
			/* use rate for mcs21, 40Mhz bandwidth, no sgi */
			scb_ampdu->min_lens[MCS2IDX(mcs)] = CEIL(MCS_RATE(21, 1, 0), tmp);
		} else if (mcs == 21 || mcs == 22) {
			/* use mcs23 */
			scb_ampdu->min_lens[MCS2IDX(mcs)] = CEIL(MCS_RATE(23, 1, 0), tmp);
#if !defined(WLPROPRIETARY_11N_RATES)
		}
#else
		} else if (mcs >= WLC_11N_FIRST_PROP_MCS && mcs <= WLC_11N_LAST_PROP_MCS) {
			scb_ampdu->min_lens[MCS2IDX(mcs)] = CEIL(MCS_RATE(mcs, 1, 0), tmp);
		}
#endif /* WLPROPRIETARY_11N_RATES */
	}

	if (WLPROPRIETARY_11N_RATES_ENAB(scb_ampdu->scb->bsscfg->wlc->pub) &&
		scb_ampdu->scb->bsscfg->wlc->pub->ht_features != WLC_HT_FEATURES_PROPRATES_DISAB) {
			scb_ampdu->min_len = CEIL(MCS_RATE(WLC_11N_LAST_PROP_MCS, 1, 1), tmp);
	} else {
		scb_ampdu->min_len = CEIL(MCS_RATE(AMPDU_MAX_MCS, 1, 1), tmp);
	}
} /* wlc_ampdu_init_min_lens */

/** Called by higher software layer. Related to AMPDU density (gaps between PDUs in AMPDU) */
uint8 BCMFASTPATH
wlc_ampdu_null_delim_cnt(ampdu_tx_info_t *ampdu_tx, struct scb *scb, ratespec_t rspec,
	int phylen, uint16* minbytes)
{
	scb_ampdu_tx_t *scb_ampdu;
	wlc_info_t *wlc;
	int bytes = 0, cnt, tmp;
	uint8 tx_density;

	ASSERT(scb);
	ASSERT(SCB_AMPDU(scb));

	wlc = ampdu_tx->wlc;
	ASSERT(wlc);

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	ASSERT(scb_ampdu);

	if (scb_ampdu->mpdu_density == 0)
		return 0;

	/* RSPEC2KBPS is in kbps units ==> ~RSPEC2KBPS/2^13 is in bytes/usec
	   density x is in 2^(x-3) usec
	   ==> # of bytes needed for req density = rate/2^(16-x)
	   ==> # of null delimiters = ceil(ceil(rate/2^(16-x)) - phylen)/4)
	 */

	tx_density = scb_ampdu->mpdu_density;

	/* XXX: ucode with BTC 3-wire adds some additional instructions; so need to
	 * bump up TX ampdu density to 8us
	 */
	if (wlc_btc_wire_get(ampdu_tx->wlc) >= WL_BTC_3WIRE)
		tx_density = AMPDU_DENSITY_8_US;

	ASSERT(tx_density <= AMPDU_MAX_MPDU_DENSITY);
	if (tx_density < 6 && WLC_HT_GET_FRAMEBURST(ampdu_tx->wlc->hti) &&
		RSPEC_IS40MHZ(rspec)) {
		uint mcs;
		ASSERT(RSPEC_ISHT(rspec));
		mcs = rspec & WL_RSPEC_HT_MCS_MASK;
		ASSERT(mcs <= AMPDU_MAX_MCS || mcs == 32 || IS_PROPRIETARY_11N_MCS(mcs));
		if (WLPROPRIETARY_11N_RATES_ENAB(ampdu_tx->wlc->pub)) {
			if (mcs <= AMPDU_MAX_MCS || mcs == 32 || IS_PROPRIETARY_11N_MCS(mcs))
				bytes = scb_ampdu->min_lens[MCS2IDX(mcs)];
		} else {
			if (mcs <= AMPDU_MAX_MCS)
				bytes = scb_ampdu->min_lens[mcs];
		}
	}
	if (bytes == 0) {
		tmp = 1 << (16 - tx_density);
		bytes = CEIL(RSPEC2KBPS(rspec), tmp);
	}
	*minbytes = (uint16)bytes;

	if (bytes > phylen) {
		cnt = CEIL(bytes - phylen, AMPDU_DELIMITER_LEN);
		ASSERT(cnt <= 255);
	} else {
		cnt = 0;
	}

	/*
	 * WAR FWUCODE-1803 for tx underflow in macrev30
	 * Problem only seen at MCS14 and MCS15 of BW40 and small payload size
	 * (100, kbps/1000), where kbps/1000 is 8us density worth of byte given
	 * the MCS phyrate.
	 * Fix is to pad at least WAR_MIN_NUM_DLIM_PADDING to avoid tx underflow
	 */
	if (D11REV_IS(wlc->pub->corerev, 30)) {
		int rate_kbps = RSPEC2KBPS(rspec);
		if ((rate_kbps >= 243000) && (phylen < rate_kbps / 1000) &&
		(phylen > 100)) {
			cnt = MAX(WAR_MIN_NUM_DLIM_PADDING, cnt);
		}
	}

	return (uint8)cnt;
} /* wlc_ampdu_null_delim_cnt */

void
wlc_ampdu_macaddr_upd(wlc_info_t *wlc)
{
	char template[T_RAM_ACCESS_SZ * 2];

	/* driver needs to write the ta in the template; ta is at offset 16 */
	if (D11REV_LT(wlc->pub->corerev, 40)) {
		bzero(template, sizeof(template));
		bcopy((char*)wlc->pub->cur_etheraddr.octet, template, ETHER_ADDR_LEN);
		wlc_write_template_ram(wlc, (T_BA_TPL_BASE + 16), (T_RAM_ACCESS_SZ * 2), template);
	}
}

#if defined(WLNAR)
/**
 * WLNAR: provides balance amongst MPDU and AMPDU traffic by regulating the number of in-transit
 * packets for non-aggregating stations.
 */

/**
 * wlc_ampdu_ba_on_tidmask() - return a bitmask of aggregating TIDs (priorities).
 *
 * Inputs:
 *	scb	pointer to station control block to examine.
 *
 * Returns:
 *	The return value is a bitmask of TIDs for which a block ack agreement exists.
 *	Bit <00> = TID 0, bit <01> = TID 1, etc. A set bit indicates a BA agreement.
 */
uint8 BCMFASTPATH
wlc_ampdu_ba_on_tidmask(struct scb *scb)
{
	wlc_info_t *wlc;
	uint8 mask = 0;
	scb_ampdu_tx_t *scb_ampdu;

	/*
	 * If AMPDU_MAX_SCB_TID changes to a larger value, we will need to adjust the
	 * return value from this function, as well as the calling functions.
	 */
	STATIC_ASSERT(AMPDU_MAX_SCB_TID <= NBITS(uint8));
	ASSERT(scb);
	ASSERT(SCB_AMPDU(scb));

	/* Figure out the wlc this scb belongs to. */
	wlc = SCB_BSSCFG(scb)->wlc;

	ASSERT(wlc);
	ASSERT(wlc->ampdu_tx);

	/* Locate the ampdu scb cubby, then check all TIDs */
	scb_ampdu = SCB_AMPDU_TX_CUBBY(wlc->ampdu_tx, scb);
	ASSERT(wlc->ampdu_tx->txaggr_support);
	if (scb_ampdu) {
		mask = scb_ampdu->tidmask;
	}

	return mask;
} /* wlc_ampdu_ba_on_tidmask */

#endif /* WLNAR */

#if defined(WL_DATAPATH_LOG_DUMP)
/**
 * Cubby datapath log callback fn
 * Use EVENT_LOG to dump a summary of the AMPDU datapath state
 * @param ctx   context pointer to ampdu_tx_info_t state structure
 * @param scb   scb of interest for the dump
 * @param tag   EVENT_LOG tag for output
 */
static void
wlc_ampdu_datapath_summary(void *ctx, struct scb *scb, int tag)
{
	int buf_size;
	int i;
	uint prec;
	uint num_prec;
	scb_subq_summary_t *sum;
	scb_ampdu_tx_summary_t *tid_sum;
	ampdu_tx_info_t *ampdu_tx = (ampdu_tx_info_t *)(ctx);
	scb_ampdu_tx_t *scb_ampdu;
	scb_ampdu_tid_ini_t *ini;
	osl_t *osh = ampdu_tx->wlc->osh;

	if (!SCB_AMPDU(scb)) {
		/* nothing to report on for AMPDU */
		return;
	}

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	ASSERT(scb_ampdu);

	/*
	 * allcate a scb_subq_summary struct to dump ampdu information to the EVENT_LOG
	 */

	/* allocate a size large enough for max precidences */
	buf_size = SCB_SUBQ_SUMMARY_FULL_LEN(PKTQ_MAX_PREC);

	sum = MALLOCZ(osh, buf_size);
	if (sum == NULL) {
		EVENT_LOG(tag,
		          "wlc_ampdu_datapath_summary(): MALLOC %d failed, malloced %d bytes\n",
		          buf_size, MALLOCED(osh));
		/* nothing to do if event cannot be allocated */
		return;
	}

	num_prec = MIN(scb_ampdu->txq.num_prec, PKTQ_MAX_PREC);

	/* Report the multi-prec queue summary */
	sum->id = EVENT_LOG_XTLV_ID_SCBDATA_SUM;
	sum->len = SCB_SUBQ_SUMMARY_FULL_LEN(num_prec) - BCM_XTLV_HDR_SIZE;
	sum->cubby_id = ampdu_tx->cubby_name_id;
	sum->prec_count = num_prec;
	for (prec = 0; prec < num_prec; prec++) {
		sum->plen[prec] = pktqprec_n_pkts(&scb_ampdu->txq, prec);
	}

	EVENT_LOG_BUFFER(tag, (uint8*)sum, sum->len + BCM_XTLV_HDR_SIZE);

	MFREE(osh, sum, buf_size);

	/* allocate the TID xtlv */
	buf_size = sizeof(*tid_sum);

	tid_sum = MALLOCZ(osh, buf_size);
	if (tid_sum == NULL) {
		EVENT_LOG(tag,
		          "wlc_ampdu_datapath_summary(): MALLOC %d failed, malloced %d bytes\n",
		          buf_size, MALLOCED(osh));
		/* nothing to do if event cannot be allocated */
		return;
	}

	/* for each TID, report ampdu specific data summary */
	for (i = 0; i < AMPDU_MAX_SCB_TID; i++) {
		if ((ini = scb_ampdu->ini[i]) == NULL) {
			continue;
		}

		/* fill out event log record */
		tid_sum->id = EVENT_LOG_XTLV_ID_SCBDATA_AMPDU_TX_SUM;
		tid_sum->len = buf_size - BCM_XTLV_HDR_SIZE;
		tid_sum->flags = ini->bar_ackpending ? SCBDATA_AMPDU_TX_F_BAR_ACKPEND : 0;
		tid_sum->ba_state = ini->ba_state;
		tid_sum->bar_cnt = ini->ba_state;
		tid_sum->retry_bar = ini->retry_bar;
		tid_sum->bar_ackpending_seq = ini->bar_ackpending_seq;
		tid_sum->barpending_seq = ini->barpending_seq;
		tid_sum->start_seq = ini->start_seq;
		tid_sum->max_seq = ini->max_seq;
#ifdef WLATF
		tid_sum->released_bytes_inflight = AMPDU_ATF_STATE(ini)->released_bytes_inflight;
		tid_sum->released_bytes_target = AMPDU_ATF_STATE(ini)->released_bytes_target;
#endif /* WLATF */
		EVENT_LOG_BUFFER(tag, (uint8*)tid_sum, tid_sum->len + BCM_XTLV_HDR_SIZE);
	}

	MFREE(osh, tid_sum, buf_size);
}
#endif /* WL_DATAPATH_LOG_DUMP */

#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
#if defined(EVENT_LOG_COMPILE)
int
wlc_ampdu_stats_e_report(uint16 tag, uint16 type, uint16 len, uint32 *counters)
{
	wl_ampdu_stats_generic_t ampdu_stats;
	int total_len;
	ASSERT(len <= WL_AMPDU_STATS_MAX_CNTS);

	ampdu_stats.type = type;
	ampdu_stats.len = MIN(len, WL_AMPDU_STATS_MAX_CNTS) * sizeof(uint32);
	memcpy(&ampdu_stats.counters[0], counters, ampdu_stats.len);
	total_len = sizeof(ampdu_stats.type) + sizeof(ampdu_stats.len) + ampdu_stats.len;

#ifdef ECOUNTERS
	if (ec) {
		/* Report using ecounter when requested */
		return ecounters_write(tag, (void *)(&ampdu_stats), total_len);
	}
#endif /* ECOUNTERS */

	/* Report using event log without specified buffer */
	EVENT_LOG_BUFFER(tag, (void *)(&ampdu_stats), total_len);

	return BCME_OK;
}

void
wlc_ampdu_stats_range(uint32 *stats, int max_counters, int *first, int *last)
{
	int i;

	for (i = 0, *first = -1, *last = 0; i < max_counters; i++) {
		if (stats[i] == 0) continue;
		if (*first < 0) {
			*first = i;
		}
		*last = i;
	}
}

static int
wlc_ampdu_sum_slice_txmcs_counters(ampdu_tx_info_t *ampdu_tx, uint32 *scratch_buffer, uint16 type)
{
	uint32 i, idx, *ptr;
	ampdu_dbg_t *local_ampdu_dbg;
	wlc_info_t *wlc_iter;
	uint32 array_size;
	wlc_info_t *wlc = (wlc_info_t *)ampdu_tx->wlc;
	wlc_cmn_info_t *wlc_cmn = wlc->cmn;

	switch (type) {
#ifdef WL11AX
		case AMPDU_11HE:
			array_size = ARRAYSIZE(ampdu_tx->amdbg->txhe[WL_RSPEC_HE_2x_LTF_GI_1_6us]);
			ptr = ampdu_tx->amdbg->txhe[WL_RSPEC_HE_2x_LTF_GI_1_6us];
			break;
#endif /* WL11AX */
#ifdef WL11AC
		case AMPDU_11VHT:
			array_size = ARRAYSIZE(ampdu_tx->amdbg->txvht);
			ptr = ampdu_tx->amdbg->txvht;
			break;
#endif /* WL11AC */
		case AMPDU_11N:
			array_size = ARRAYSIZE(ampdu_tx->amdbg->txmcs);
			ptr = ampdu_tx->amdbg->txmcs;
			break;
		default:
			return BCME_BADARG;
	}

	/* Copy first set of stats in the scratch buffer
	 * This is done to reduce addition operations
	 */
	memcpy(scratch_buffer, ptr, array_size);

	/* Do the sum over remaining WLCs */
	if (RSDB_ENAB(wlc->pub)) {
		for (idx = 0; idx < MAX_RSDB_MAC_NUM; idx++) {
			if (idx == wlc->pub->unit)
				continue;

			wlc_iter = wlc_cmn->wlc[idx];
			if (wlc_iter == NULL) {
				continue;
			}
			local_ampdu_dbg = wlc_iter->ampdu_tx->amdbg;
			switch (type) {
#ifdef WL11AX
				case AMPDU_11HE:
					ptr = local_ampdu_dbg->txhe[WL_RSPEC_HE_2x_LTF_GI_1_6us];
					break;
#endif /* WL11AX */
#ifdef WL11AC
				case AMPDU_11VHT:
					ptr = local_ampdu_dbg->txvht;
					break;
#endif /* WL11AX */
				case AMPDU_11N:
					ptr = local_ampdu_dbg->txmcs;
					break;
				default:
					return BCME_BADARG;
			}

			for (i = 0; i < array_size; i++) {
				scratch_buffer[i] += *ptr++;
			}
		}
	}

	return BCME_OK;
}

int
wlc_ampdu_txmcs_counter_report(ampdu_tx_info_t *ampdu_tx, uint16 tag)
{
	int i, j, first, last, num_rates;
	uint32 scratch_buffer[AMPDU_MAX_VHT];
	int err = BCME_OK;
	const uint16 MCS_RATE_TYPE[4] = {WL_AMPDU_STATS_TYPE_TXMCSx1,
			WL_AMPDU_STATS_TYPE_TXMCSx2,
			WL_AMPDU_STATS_TYPE_TXMCSx3,
			WL_AMPDU_STATS_TYPE_TXMCSx4};
#ifdef WL11AC
	const uint16 VHT_RATE_TYPE[4] = {WL_AMPDU_STATS_TYPE_TXVHTx1,
			WL_AMPDU_STATS_TYPE_TXVHTx2,
			WL_AMPDU_STATS_TYPE_TXVHTx3,
			WL_AMPDU_STATS_TYPE_TXVHTx4};
#endif /* WL11AC */

	memset(scratch_buffer, 0, sizeof(scratch_buffer));
	/* Get HT sum across all slices */
	err = wlc_ampdu_sum_slice_txmcs_counters(ampdu_tx, scratch_buffer, AMPDU_11N);
	if (err != BCME_OK) {
		return err;
	}

	wlc_ampdu_stats_range(scratch_buffer,
		ARRAYSIZE(ampdu_tx->amdbg->txmcs), &first, &last);

	num_rates = WL_NUM_RATES_MCS_1STREAM;

	for (i = 0, j = 0; (first >= 0) && (i <= last); i += num_rates, j++) {
		if (err == BCME_OK && first < (i + num_rates)) {
			err = wlc_ampdu_stats_e_report(tag,
				MCS_RATE_TYPE[j], num_rates, &scratch_buffer[i]);
		}
	}

#ifdef WL11AC
	/* Get VHT sum across all slices */
	err = wlc_ampdu_sum_slice_txmcs_counters(ampdu_tx, scratch_buffer, AMPDU_11VHT);
	if (err != BCME_OK) {
		return err;
	}

	wlc_ampdu_stats_range(scratch_buffer,
		ARRAYSIZE(ampdu_tx->amdbg->txvht), &first, &last);

	/* Process in chunks of 12 For VHT 0 - 9 are standard rates,
	 * 10 - 11 are prop rates.
	 */
	num_rates = WL_NUM_RATES_VHT + WL_NUM_RATES_EXTRA_VHT;

	for (i = 0, j = 0; (first >= 0) && (i <= last); i += num_rates, j++) {
		if (err == BCME_OK && first < (i + num_rates)) {
			err = wlc_ampdu_stats_e_report(tag,
				VHT_RATE_TYPE[j], num_rates, &scratch_buffer[i]);
		}
	}
#endif /* WL11AC */
	return err;
}

#ifdef ECOUNTERS
int
wlc_ampdu_ecounter_tx_dump(ampdu_tx_info_t *ampdu_tx, uint16 tag)
{
	int mcs_first, mcs_last;
#ifdef WL11AC
	int vht_first, vht_last;
#endif // endif
	wl_ampdu_stats_aggrsz_t tx_dist;
	int err = BCME_OK;
	uint16 i = 0;
	xtlv_desc_t xtlv_desc[ECOUNTER_AMPDU_TX_DUMP_SIZE];

	wlc_ampdu_stats_range(ampdu_tx->amdbg->txmcs, ARRAYSIZE(ampdu_tx->amdbg->txmcs),
		&mcs_first, &mcs_last);
#ifdef WL11AC
	wlc_ampdu_stats_range(ampdu_tx->amdbg->txvht, ARRAYSIZE(ampdu_tx->amdbg->txvht),
		&vht_first, &vht_last);
#endif /* WL11AC */

	/* MCS rate */
	if (mcs_first >= 0 && i < ARRAYSIZE(xtlv_desc)) {
		xtlv_desc[i].type = WL_AMPDU_STATS_TYPE_TXMCSALL;
		xtlv_desc[i].ptr = ampdu_tx->amdbg->txmcs;
		xtlv_desc[i].len = mcs_last + 1;
		i++;
	}

#ifdef WL11AC
	/* VHT rate */
	if (vht_first >= 0 && i < ARRAYSIZE(xtlv_desc)) {
		xtlv_desc[i].type = WL_AMPDU_STATS_TYPE_TXVHTALL;
		xtlv_desc[i].ptr = ampdu_tx->amdbg->txvht;
		xtlv_desc[i].len = vht_last + 1;
		i++;
	}
#endif /* WL11AC */

	if (AMPDU_AQM_ENAB(ampdu_tx->wlc->pub)) {
		if (mcs_first >= 0 && i < ARRAYSIZE(xtlv_desc)) {
			xtlv_desc[i].type = WL_AMPDU_STATS_TYPE_TXMCSOK;
			xtlv_desc[i].ptr = ampdu_tx->amdbg->txmcs_succ;
			xtlv_desc[i].len = mcs_last + 1;
			i++;
		}
#ifdef WL11AC
		if (vht_first >= 0 && i < ARRAYSIZE(xtlv_desc)) {
			xtlv_desc[i].type = WL_AMPDU_STATS_TYPE_TXVHTOK;
			xtlv_desc[i].ptr = ampdu_tx->amdbg->txvht_succ;
			xtlv_desc[i].len = vht_last + 1;
			i++;
		}
#endif /* WL11AC */
	}

	if (mcs_first >= 0 && i < ARRAYSIZE(xtlv_desc)) {
		xtlv_desc[i].type = WL_AMPDU_STATS_TYPE_TXMCSSGI;
		xtlv_desc[i].ptr = ampdu_tx->amdbg->txmcssgi;
		xtlv_desc[i].len = mcs_last + 1;
		i++;
	}

#ifdef WL11AC
	if (vht_first >= 0 && i < ARRAYSIZE(xtlv_desc)) {
		xtlv_desc[i].type = WL_AMPDU_STATS_TYPE_TXVHTSGI;
		xtlv_desc[i].ptr = ampdu_tx->amdbg->txvhtsgi;
		xtlv_desc[i].len = vht_last + 1;
		i++;
	}
#endif /* WL11AC */

	/* Update AMPDU histogram when possible */
	if (ampdu_tx->wlc->clk) {
		uint16 stat_addr = 0;
		uint16 i, val, nbins;
		int total_ampdu = 0, total_mpdu = 0;

		nbins = ARRAYSIZE(ampdu_tx->amdbg->mpdu_histogram);
		if (nbins > C_MPDUDEN_NBINS)
			nbins = C_MPDUDEN_NBINS;
		if (AMPDU_AQM_ENAB(ampdu_tx->wlc->pub)) {
			stat_addr = 2 * wlc_read_shm(ampdu_tx->wlc,
			                             M_AMP_STATS_PTR(ampdu_tx->wlc));
			for (i = 0; i < nbins; i++) {
				val = wlc_read_shm(ampdu_tx->wlc, stat_addr + i * 2);
				ampdu_tx->amdbg->mpdu_histogram[i] = val;
				total_ampdu += val;
				total_mpdu += (val * (i+1));
			}
			ampdu_tx->cnt->txampdu = total_ampdu;
			WLCNTSET(ampdu_tx->cnt->txmpdu, total_mpdu);
		}
	}

	wlc_ampdu_stats_range(ampdu_tx->amdbg->mpdu_histogram,
		ARRAYSIZE(ampdu_tx->amdbg->mpdu_histogram),
		&mcs_first, &mcs_last);
	if (mcs_first >= 0 && i < ARRAYSIZE(xtlv_desc)) {
		tx_dist.type = WL_AMPDU_STATS_TYPE_TXDENS;
		tx_dist.len = (mcs_last + 3) * sizeof(uint32);	/* Plus total_ampdu & total_mpdu */
		tx_dist.total_ampdu = ampdu_tx->cnt->txampdu;
#ifdef WLCNT
		tx_dist.total_mpdu = ampdu_tx->cnt->txmpdu;
#else
		tx_dist.total_mpdu = 0;
#endif // endif
		memcpy(&tx_dist.aggr_dist[0], ampdu_tx->amdbg->mpdu_histogram,
			(mcs_last + 1) * sizeof(uint32));

		xtlv_desc[i].type = tx_dist.type;
		xtlv_desc[i].ptr = &tx_dist.total_ampdu;
		xtlv_desc[i].len = tx_dist.len;
	}
	err = wlc_ampdu_ecounter_pack_xtlv(tag, xtlv_desc, xtlvbuf, cookie,
		attempted_write_len, i);
	return err;
}
#endif /* ECOUNTERS */
#endif /* EVENT_LOG_COMPILE */

/* This approach avoids overflow of a 32 bits integer */
#define PERCENT(total, part) (((total) > 10000) ? \
	((part) / ((total) / 100)) : \
	(((part) * 100) / (total)))

int
wlc_ampdu_tx_dump(ampdu_tx_info_t *ampdu_tx, struct bcmstrbuf *b)
{
#ifdef WLCNT
	wlc_ampdu_tx_cnt_t *cnt = ampdu_tx->cnt;
#endif // endif
	wlc_info_t *wlc = ampdu_tx->wlc;
	wlc_pub_t *pub = wlc->pub;
	int i, last;
	uint32 max_val, total = 0, total_succ = 0;
	struct scb *scb;
	struct scb_iter scbiter;
	scb_ampdu_tx_t *scb_ampdu;
	scb_ampdu_tid_ini_t *ini;
	int inic = 0, ini_on = 0, ini_off = 0, ini_pon = 0, ini_poff = 0;
	int nbuf = 0;
#ifdef AMPDU_NON_AQM
	int j;
	wlc_fifo_info_t *fifo;
#endif // endif
#ifdef  WLOVERTHRUSTER
	wlc_tcp_ack_info_t *tcp_ack_info = &ampdu_tx->tcp_ack_info;
#endif // endif
#ifdef WL11AX
	uint gi;
	const char *gi_name[] = { "1/0.8", "2/0.8", "2/1.6", "4/3.2" }; /* LTF/GI */
#endif /* WL11AX */
	ampdu_dbg_t *amdbg = ampdu_tx->amdbg; /* for short hand */

	BCM_REFERENCE(pub);

	bcm_bprintf(b, "HOST_ENAB %d UCODE_ENAB %d HW_ENAB 0 AQM_ENAB %d\n",
		AMPDU_HOST_ENAB(pub), AMPDU_UCODE_ENAB(pub), AMPDU_AQM_ENAB(pub));

	bcm_bprintf(b, "AMPDU Tx counters:\n");
	if (wlc->clk && AMPDU_MAC_ENAB(pub) && !AMPDU_AQM_ENAB(pub)) {

		uint32 hwampdu, hwmpdu, hwrxba, hwnoba;
		hwampdu = wlc_read_shm(wlc, MACSTAT_ADDR(wlc, MCSTOFF_TXAMPDU));
		hwmpdu = wlc_read_shm(wlc, MACSTAT_ADDR(wlc, MCSTOFF_TXMPDU));
		hwrxba = wlc_read_shm(wlc, MACSTAT_ADDR(wlc, MCSTOFF_RXBACK));
		hwnoba = (hwampdu < hwrxba) ? 0 : (hwampdu - hwrxba);
		bcm_bprintf(b, "%s: txampdu %d txmpdu %d txmpduperampdu %d noba %d (%d%%)\n",
			"Ucode",
			hwampdu, hwmpdu,
			hwampdu ? CEIL(hwmpdu, hwampdu) : 0, hwnoba,
			hwampdu ? CEIL(hwnoba*100, hwampdu) : 0);
	}

#ifdef WLCNT
	if (wlc->clk && AMPDU_MAC_ENAB(pub)) {
		if (!AMPDU_AQM_ENAB(pub))
		bcm_bprintf(b, "txmpdu(enq) %d txs %d txmrt/succ %d %d (f %d%%) "
			"txfbr/succ %d %d (f %d%%)\n", cnt->txmpdu, cnt->u0.txs,
			cnt->tx_mrt, cnt->txsucc_mrt,
			cnt->tx_mrt ? CEIL((cnt->tx_mrt - cnt->txsucc_mrt)*100, cnt->tx_mrt) : 0,
			cnt->tx_fbr, cnt->txsucc_fbr,
			cnt->tx_fbr ? CEIL((cnt->tx_fbr - cnt->txsucc_fbr)*100, cnt->tx_fbr) : 0);
	} else
	{
		bcm_bprintf(b, "txampdu %d txmpdu %d txmpduperampdu %d noba %d (%d%%)\n",
			cnt->txampdu, cnt->txmpdu,
			cnt->txampdu ? CEIL(cnt->txmpdu, cnt->txampdu) : 0, cnt->noba,
			cnt->txampdu ? CEIL(cnt->noba*100, cnt->txampdu) : 0);
		bcm_bprintf(b, "retry_ampdu %d retry_mpdu %d (%d%%) txfifofull %d\n",
		  cnt->txretry_ampdu, cnt->u0.txretry_mpdu,
		  (cnt->txmpdu ? CEIL(cnt->u0.txretry_mpdu*100, cnt->txmpdu) : 0), cnt->txfifofull);
		bcm_bprintf(b, "fbr_ampdu %d fbr_mpdu %d\n",
			cnt->txfbr_ampdu, cnt->txfbr_mpdu);
	}
	bcm_bprintf(b, "txregmpdu %d txreg_noack %d txfifofull %d txdrop %d txstuck %d orphan %d\n",
		cnt->txregmpdu, cnt->txreg_noack, cnt->txfifofull,
		cnt->txdrop, cnt->txstuck, cnt->orphan);
	bcm_bprintf(b, "txrel_wm %d txrel_size %d sduretry %d sdurejected %d\n",
		cnt->txrel_wm, cnt->txrel_size, cnt->sduretry, cnt->sdurejected);

	if (AMPDU_MAC_ENAB(pub)) {
		bcm_bprintf(b, "aggfifo_w %d epochdeltas %d mpduperepoch %d\n",
			cnt->enq, cnt->epochdelta,
			(cnt->epochdelta+1) ? CEIL(cnt->enq, (cnt->epochdelta+1)) : 0);
		bcm_bprintf(b, "epoch_change reason: plcp %d rate %d fbr %d reg %d link %d"
			" seq no %d\n", cnt->echgr1, cnt->echgr2, cnt->echgr3,
			cnt->echgr4, cnt->echgr5, cnt->echgr6);
	}

	bcm_bprintf(b, "txr0hole %d txrnhole %d txrlag %d rxunexp %d\n",
		cnt->txr0hole, cnt->txrnhole, cnt->txrlag, cnt->rxunexp);
	bcm_bprintf(b, "txaddbareq %d rxaddbaresp %d txlost %d txbar %d rxba %d txdelba %d "
		"rxdelba %d \n",
		cnt->txaddbareq, cnt->rxaddbaresp, cnt->txlost, cnt->txbar,
		cnt->rxba, cnt->txdelba, cnt->rxdelba);

	if (AMPDU_MAC_ENAB(pub))
		bcm_bprintf(b, "txmpdu_sgi %d txmpdu_stbc %d\n",
		  cnt->u1.txmpdu_sgi, cnt->u2.txmpdu_stbc);
	else
		bcm_bprintf(b, "txampdu_sgi %d txampdu_stbc %d "
			"txampdu_mfbr_stbc %d\n", cnt->u1.txampdu_sgi,
			cnt->u2.txampdu_stbc, cnt->txampdu_mfbr_stbc);

#ifdef WL_CS_PKTRETRY
	bcm_bprintf(b, "cs_pktretry_cnt %u\n", cnt->cs_pktretry_cnt);
#endif // endif

#endif /* WLCNT */

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (SCB_AMPDU(scb)) {
			scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
			ASSERT(scb_ampdu);
			for (i = 0; i < AMPDU_MAX_SCB_TID; i++) {
				if ((ini = scb_ampdu->ini[i])) {
					inic++;
					if (ini->ba_state == AMPDU_TID_STATE_BA_OFF)
						ini_off++;
					if (ini->ba_state == AMPDU_TID_STATE_BA_ON)
						ini_on++;
					if (ini->ba_state == AMPDU_TID_STATE_BA_PENDING_OFF)
						ini_poff++;
					if (ini->ba_state == AMPDU_TID_STATE_BA_PENDING_ON)
						ini_pon++;
				}
			}
			nbuf += pktq_n_pkts_tot(&scb_ampdu->txq);
		}
	}

	bcm_bprintf(b, "\n");
	bcm_bprintf(b, "ini %d ini_off %d ini_on %d ini_poff %d ini_pon %d nbuf %d ampdu_wds %u\n",
		inic, ini_off, ini_on, ini_poff, ini_pon, nbuf, cnt->ampdu_wds);
#ifdef WLOVERTHRUSTER
	if ((OVERTHRUST_ENAB(pub)) && (tcp_ack_info->tcp_ack_ratio)) {
		bcm_bprintf(b, "tcp_ack_ratio %d/%d total %d/%d dequeued %d multi_dequeued %d\n",
			tcp_ack_info->tcp_ack_ratio, tcp_ack_info->tcp_ack_ratio_depth,
			tcp_ack_info->tcp_ack_total - tcp_ack_info->tcp_ack_dequeued
			- tcp_ack_info->tcp_ack_multi_dequeued,
			tcp_ack_info->tcp_ack_total,
			tcp_ack_info->tcp_ack_dequeued, tcp_ack_info->tcp_ack_multi_dequeued);
	}
#endif /* WLOVERTHRUSTER */

	bcm_bprintf(b, "Supr Reason: pmq(%d) flush(%d) frag(%d) badch(%d) exptime(%d) uf(%d)",
		amdbg->supr_reason[TX_STATUS_SUPR_PMQ],
		amdbg->supr_reason[TX_STATUS_SUPR_FLUSH],
		amdbg->supr_reason[TX_STATUS_SUPR_FRAG],
		amdbg->supr_reason[TX_STATUS_SUPR_BADCH],
		amdbg->supr_reason[TX_STATUS_SUPR_EXPTIME],
		amdbg->supr_reason[TX_STATUS_SUPR_UF]);

#ifdef WLP2P
	if (P2P_ENAB(pub))
		bcm_bprintf(b, " abs(%d)",
			amdbg->supr_reason[TX_STATUS_SUPR_NACK_ABS]);
#endif // endif
#ifdef AP
	bcm_bprintf(b, " pps(%d)",
		amdbg->supr_reason[TX_STATUS_SUPR_PPS]);
#endif // endif
	bcm_bprintf(b, "\n\n");

	/* determines highest MCS array *index* on which a transmit took place */
	for (i = 0, total = 0, last = 0; i <= AMPDU_HT_MCS_LAST_EL; i++) {
		total += amdbg->txmcs[i];
		if (amdbg->txmcs[i]) last = i;
	}

	if (last <= AMPDU_MAX_MCS) { /* skips proprietary 11N MCS'es */
		/* round up to highest MCS array index with same number of spatial streams */
		last = 8 * (last / 8 + 1) - 1;
	}

	if (total) {
		bcm_bprintf(b, "TX HT   :");
		for (i = 0; i <= last; i++) {
			bcm_bprintf(b, "  %d(%d%%)", amdbg->txmcs[i],
				(amdbg->txmcs[i] * 100) / total);
			if ((i % 8) == 7 && i != last)
				bcm_bprintf(b, "\n        :");
		}
		bcm_bprintf(b, "\n");
	}

	if (total) {
		bcm_bprintf(b, "HT PER  :");
		for (i = 0; i <= last; i++) {
			int unacked = 0, per = 0;
			unacked = amdbg->txmcs[i] -
				amdbg->txmcs_succ[i];
			if (unacked < 0) unacked = 0;
			if ((unacked > 0) && (amdbg->txmcs[i]))
				per = (unacked * 100) / amdbg->txmcs[i];
			bcm_bprintf(b, "  %d(%d%%)", unacked, per);
			if ((i % 8) == 7 && i != last)
				bcm_bprintf(b, "\n        :");
		}
		bcm_bprintf(b, "\n");
	}

#ifdef WL11AC
	for (i = 0, total = 0, last = 0; i < AMPDU_MAX_VHT; i++) {
		total += amdbg->txvht[i];
		if (amdbg->txvht[i]) last = i;
	}
	last = MAX_VHT_RATES * (last/MAX_VHT_RATES + 1) - 1;
	if (total) {
		bcm_bprintf(b, "TX VHT  :");
		for (i = 0; i <= last; i++) {
			bcm_bprintf(b, "  %d(%d%%)", amdbg->txvht[i],
				(amdbg->txvht[i] * 100) / total);
			if ((i % MAX_VHT_RATES) == (MAX_VHT_RATES - 1) && i != last)
				bcm_bprintf(b, "\n        :");
		}
		bcm_bprintf(b, "\n");
	}

	if (AMPDU_AQM_ENAB(pub)) {
		if (total) {
			bcm_bprintf(b, "VHT PER :");
			for (i = 0; i <= last; i++) {
				int unacked = 0, per = 0;
				unacked = amdbg->txvht[i] -
					amdbg->txvht_succ[i];
				if (unacked < 0) unacked = 0;
				if ((unacked > 0) && (amdbg->txvht[i]))
					per = (unacked * 100) / amdbg->txvht[i];
				bcm_bprintf(b, "  %d(%d%%)", unacked, per);
				if (((i % MAX_VHT_RATES) == (MAX_VHT_RATES - 1)) && (i != last))
					bcm_bprintf(b, "\n        :");
			}
			bcm_bprintf(b, "\n");
		}
	}
#endif /* WL11AC */

#ifdef WL11AX
	for (gi = 0; gi < AMPDU_MAX_HE_GI; gi++) {
		for (i = 0, total = 0, last = 0; i < AMPDU_MAX_HE; i++) {
			total += amdbg->txhe[gi][i];
			if (amdbg->txhe[gi][i]) last = i;
		}
		if (total) {
			last = MAX_HE_RATES * (last/MAX_HE_RATES + 1) - 1;
			bcm_bprintf(b, "TX HE  %s:", gi_name[gi]);
			for (i = 0; i <= last; i++) {
				bcm_bprintf(b, "  %d(%d%%)", amdbg->txhe[gi][i],
					(amdbg->txhe[gi][i] * 100) / total);
				if ((i % MAX_HE_RATES) == (MAX_HE_RATES - 1) && i != last)
					bcm_bprintf(b, "\n            :");
			}
			bcm_bprintf(b, "\n");
			if (AMPDU_AQM_ENAB(pub)) {
				bcm_bprintf(b, "HE PER %s:", gi_name[gi]);
				for (i = 0; i <= last; i++) {
					int unacked;
					uint txtotal, per = 0;
					txtotal = amdbg->txhe[gi][i];
					unacked = txtotal - amdbg->txhe_succ[gi][i];
					if (unacked < 0) unacked = 0;
					if (unacked != 0 && txtotal != 0) {
						per = (unacked * 100) / txtotal;
					}
					bcm_bprintf(b, "  %d(%d%%)", unacked, per);
					if (((i % MAX_HE_RATES) == MAX_HE_RATES - 1) && i != last)
						bcm_bprintf(b, "\n            :");
				}
				bcm_bprintf(b, "\n");
			}
		}
	}
#endif /* WL11AX */

	for (i = 0, total = 0; i < AMPDU_PPDU_FT_MAX; i++) {
		total += amdbg->txmpdu[i];
		total_succ += amdbg->txmpdu_succ[i];
	}
	if (total) {
		char tmp_buf[20];
		const char ampdu_ft_str[][6] = {
			"su",
			"vhtmu",
			"hemmu",
			"heomu",
			"hemom",
		};
		sprintf(tmp_buf, "%d", total);
		bcm_bprintf(b, "Total: txmpdu %-12s ", tmp_buf);
		if (MU_TX_ENAB(wlc) || HE_DLMU_ENAB(wlc->pub) || HE_MMU_ENAB(wlc->pub)) {
			for (i = 0; i < AMPDU_PPDU_FT_MAX; i++) {
				if (amdbg->txmpdu[i]) {
					sprintf(tmp_buf, "%d(%d%%)", amdbg->txmpdu[i],
						PERCENT(total, amdbg->txmpdu[i]));
					bcm_bprintf(b, "txmpdu_%s %-12s ",
						ampdu_ft_str[i], tmp_buf);
				}
			}
		}
		bcm_bprintf(b, "\n");
		sprintf(tmp_buf, "%d(%d%%)", total-total_succ, 100-PERCENT(total, total_succ));
		bcm_bprintf(b, "Fail : txmpdu %-12s ",  tmp_buf);
		if (MU_TX_ENAB(wlc) || HE_DLMU_ENAB(wlc->pub) || HE_MMU_ENAB(wlc->pub)) {
			for (i = 0; i < AMPDU_PPDU_FT_MAX; i++) {
				if (amdbg->txmpdu[i]) {
					sprintf(tmp_buf, "%d(%d%%)", amdbg->txmpdu[i] -
					    amdbg->txmpdu_succ[i],
					    100-PERCENT(amdbg->txmpdu[i], amdbg->txmpdu_succ[i]));
					bcm_bprintf(b, "txmpdu_%s %-12s ",
						ampdu_ft_str[i], tmp_buf);
				}
			}
		}
		bcm_bprintf(b, "\n");
	}

#if defined(WLPKTDLYSTAT) && defined(WLCNT)

	/* Report PER statistics (for each MCS) */
	for (i = 0, max_val = 0, last = 0; i <= AMPDU_HT_MCS_LAST_EL; i++) {
		max_val += cnt->txmpdu_cnt[i];
		if (cnt->txmpdu_cnt[i])
			last = i; /* highest used MCS array index */
	}

	if (last <= AMPDU_MAX_MCS) { /* skips proprietary 11N MCS'es */
		/* round up to highest MCS array index with same number of spatial streams */
		last = 8 * (last / 8 + 1) - 1;
	}

	if (max_val) {
		bcm_bprintf(b, "PER : ");
		for (i = 0; i <= last; i++) {
			int unacked = 0;
			unacked = cnt->txmpdu_cnt[i] - cnt->txmpdu_succ_cnt[i];
			if (unacked < 0) {
				unacked = 0;
			}
			bcm_bprintf(b, "  %d/%d (%d%%)",
			            (cnt->txmpdu_cnt[i] - cnt->txmpdu_succ_cnt[i]),
			            cnt->txmpdu_cnt[i],
			            ((cnt->txmpdu_cnt[i] > 0) ?
			             (unacked * 100) / cnt->txmpdu_cnt[i] : 0));
			if ((i % 8) == 7 && i != last) {
				bcm_bprintf(b, "\n    : ");
			}
		}
		bcm_bprintf(b, "\n");
	}
#endif /* WLPKTDLYSTAT && WLCNT */

	if (wlc->clk) {
		uint16 stat_addr = 0;
		uint16 val;
		int cnt1 = 0, cnt2 = 0;
		uint16 nbins;

		nbins = ARRAYSIZE(amdbg->mpdu_histogram);
		if (nbins > C_MPDUDEN_NBINS)
			nbins = C_MPDUDEN_NBINS;
		if (AMPDU_AQM_ENAB(pub)) {
			uint16 stop_len, stop_mpdu, stop_bawin, stop_epoch, stop_fempty;

			stat_addr = 2 * wlc_read_shm(wlc, M_AMP_STATS_PTR(wlc));
			for (i = 0, total = 0; i < nbins; i++) {
				val = wlc_read_shm(wlc, stat_addr + i * 2);
				amdbg->mpdu_histogram[i] = val;
				total += val;
				cnt1 += (val * (i+1));
			}
			cnt->txampdu = total;
			WLCNTSET(cnt->txmpdu, cnt1);
			bcm_bprintf(b, "--------------------------\n");
			bcm_bprintf(b, "tot_mpdus %d tot_ampdus %d mpduperampdu %d\n",
			            cnt1, total, (total == 0) ? 0 : CEIL(cnt1, total));

			/* print out agg stop reason */
			stat_addr = M_AGGSTOP_HISTO(wlc);
			stop_len    = wlc_read_shm(wlc, stat_addr);
			stop_mpdu   = wlc_read_shm(wlc, stat_addr + 2);
			stop_bawin  = wlc_read_shm(wlc, stat_addr + 4);
			stop_epoch  = wlc_read_shm(wlc, stat_addr + 6);
			stop_fempty = wlc_read_shm(wlc, stat_addr + 8);
			total = stop_len + stop_mpdu + stop_bawin + stop_epoch + stop_fempty;
			bcm_bprintf(b, "agg stop reason: len %d (%d%%) ampdu_mpdu %d (%d%%)"
				" bawin %d (%d%%) epoch %d (%d%%) fempty %d (%d%%)\n",
				stop_len, (total == 0) ? 0 : (stop_len * 100) / total,
				stop_mpdu, (total == 0) ? 0 : (stop_mpdu * 100) / total,
				stop_bawin, (total == 0) ? 0 : (stop_bawin * 100) / total,
				stop_epoch, (total == 0) ? 0 : (stop_epoch * 100) / total,
				stop_fempty, (total == 0) ? 0 : (stop_fempty * 100) / total);
			stat_addr = M_MBURST_HISTO(wlc);
		}

		if (WLC_HT_GET_FRAMEBURST(ampdu_tx->wlc->hti) && AMPDU_AQM_ENAB(pub)) {
			/* burst size */
			cnt1 = 0;
			bcm_bprintf(b, "Frameburst histogram:");
			for (i = 0; i < C_MBURST_NBINS; i++) {
				val = wlc_read_shm(wlc, stat_addr + i * 2);
				cnt1 += val;
				cnt2 += (i+1) * val;
				bcm_bprintf(b, "  %d", val);
			}
			bcm_bprintf(b, " avg %d\n", (cnt1 == 0) ? 0 : CEIL(cnt2, cnt1));
			bcm_bprintf(b, "--------------------------\n");
		}
	}

	for (i = 0, last = 0, total = 0; i < AMPDU_MAX_MPDU; i++) {
		total += amdbg->mpdu_histogram[i];
		if (amdbg->mpdu_histogram[i])
			last = i;
	}
	last = 8 * (last/8 + 1) - 1;
	bcm_bprintf(b, "MPDUdens:");
	for (i = 0; i <= last; i++) {
		bcm_bprintf(b, " %3d (%d%%)", amdbg->mpdu_histogram[i],
			(total == 0) ? 0 :
			(amdbg->mpdu_histogram[i] * 100 / total));
		if ((i % 8) == 7 && i != last)
			bcm_bprintf(b, "\n        :");
	}
	bcm_bprintf(b, "\n");

#ifdef AMPDU_NON_AQM
	if (AMPDU_HOST_ENAB(pub)) {
		for (i = 0, last = 0; i <= AMPDU_MAX_MPDU; i++)
			if (amdbg->retry_histogram[i])
				last = i;
		bcm_bprintf(b, "Retry   :");
		for (i = 0; i <= last; i++) {
			bcm_bprintf(b, " %3d", amdbg->retry_histogram[i]);
			if ((i % 8) == 7 && i != last)
				bcm_bprintf(b, "\n        :");
		}
		bcm_bprintf(b, "\n");

		for (i = 0, last = 0; i <= AMPDU_MAX_MPDU; i++)
			if (amdbg->end_histogram[i])
				last = i;
		bcm_bprintf(b, "Till End:");
		for (i = 0; i <= last; i++) {
			bcm_bprintf(b, " %3d", amdbg->end_histogram[i]);
			if ((i % 8) == 7 && i != last)
				bcm_bprintf(b, "\n        :");
		}
		bcm_bprintf(b, "\n");
	}
#endif /* AMPDU_NON_AQM */

	if (WLC_SGI_CAP_PHY(wlc)) {
		for (i = 0, max_val = 0; i <= AMPDU_HT_MCS_LAST_EL; i++) {
			max_val += amdbg->txmcssgi[i];
			if (amdbg->txmcssgi[i]) last = i;
		}

		if (last <= AMPDU_MAX_MCS) { /* skips proprietary 11N MCS'es */
			/* round up to highest MCS array idx with same number of spatial streams */
			last = 8 * (last / 8 + 1) - 1;
		}

		if (max_val) {
			bcm_bprintf(b, "TX HT SGI:");
			for (i = 0; i <= last; i++) {
				bcm_bprintf(b, "  %d(%d%%)", amdbg->txmcssgi[i],
				            (amdbg->txmcssgi[i] * 100) / max_val);
				if ((i % 8) == 7 && i != last)
					bcm_bprintf(b, "\n          :");
			}
			bcm_bprintf(b, "\n");
		}
#ifdef WL11AC
		for (i = 0, max_val = 0; i < AMPDU_MAX_VHT; i++) {
			max_val += amdbg->txvhtsgi[i];
			if (amdbg->txvhtsgi[i]) last = i;
		}
		last = MAX_VHT_RATES * (last/MAX_VHT_RATES + 1) - 1;
		if (max_val) {
			bcm_bprintf(b, "TX VHT SGI:");
			for (i = 0; i <= last; i++) {
					bcm_bprintf(b, "  %d(%d%%)", amdbg->txvhtsgi[i],
						(amdbg->txvhtsgi[i] * 100) / max_val);
				if (((i % MAX_VHT_RATES) == (MAX_VHT_RATES - 1)) && i != last)
						bcm_bprintf(b, "\n          :");
			}
			bcm_bprintf(b, "\n");
		}
#endif /* WL11AC */

		if (WLC_STBC_CAP_PHY(wlc)) {
			for (i = 0, max_val = 0; i <= AMPDU_HT_MCS_LAST_EL; i++)
				max_val += amdbg->txmcsstbc[i];
			if (max_val) {
				bcm_bprintf(b, "TX HT STBC:");
				for (i = 0; i <= 7; i++)
					bcm_bprintf(b, "  %d(%d%%)", amdbg->txmcsstbc[i],
						(amdbg->txmcsstbc[i] * 100) / max_val);
				bcm_bprintf(b, "\n");
			}
#ifdef WL11AC
			for (i = 0, max_val = 0; i < AMPDU_MAX_VHT; i++)
				max_val += amdbg->txvhtstbc[i];
			if (max_val) {
				bcm_bprintf(b, "TX VHT STBC:");
				for (i = 0; i < MAX_VHT_RATES; i++) {
					bcm_bprintf(b, "  %d(%d%%)", amdbg->txvhtstbc[i],
					(amdbg->txvhtstbc[i] * 100) / max_val);
				}
				bcm_bprintf(b, "\n");
			}
#endif /* WL11AC */
#ifdef WL11AX
			for (i = 0, max_val = 0; i < AMPDU_MAX_HE; i++) {
				max_val += amdbg->txhestbc[i];
			}
			if (max_val) {
				bcm_bprintf(b, "TX HE STBC:");
				for (i = 0; i < MAX_HE_RATES; i++) {
					bcm_bprintf(b, "  %d(%d%%)", amdbg->txhestbc[i],
					(amdbg->txhestbc[i] * 100) / max_val);
				}
				bcm_bprintf(b, "\n");
			}
#endif /* WL11AX */
		}
	}

#ifdef AMPDU_NON_AQM
	for (j = 0, total = 0; j < NUM_FFPLD_FIFO; j++) {
		fifo = ampdu_tx->config->fifo_tb + j;
		if (fifo->ampdu_pld_size || fifo->dmaxferrate) {
			total = 1; /* total is used as is_print */
			break;
		}
	}
	if (total) {
		bcm_bprintf(b, "MCS to AMPDU tables:\n");
		for (j = 0; j < NUM_FFPLD_FIFO; j++) {
			fifo = ampdu_tx->config->fifo_tb + j;
			if (fifo->ampdu_pld_size || fifo->dmaxferrate) {
				bcm_bprintf(b, " FIFO %d: Preload settings: size %d "
					"dmarate %d kbps\n",
					j, fifo->ampdu_pld_size, fifo->dmaxferrate);
				bcm_bprintf(b, "       :");
				for (i = 0; i <= AMPDU_HT_MCS_LAST_EL; i++) {
					bcm_bprintf(b, " %d", fifo->mcs2ampdu_table[i]);
					if ((i % 8) == 7 && i != AMPDU_HT_MCS_LAST_EL)
						bcm_bprintf(b, "\n       :");
				}
				bcm_bprintf(b, "\n");
			}
		}
		bcm_bprintf(b, "\n");
	}
#endif /* AMPDU_NON_AQM */

#if defined(BCMDBG) && defined(WLATF) && defined(AP)
	bcm_bprintf(b, "ampdu_tx: intransit %d\n", ampdu_tx->tx_intransit);
#endif /* BCMDBG && WLATF && AP */

#ifdef BCMHWA
	/* Dump HWA 3a schedule command density */
	HWA_TXPOST_EXPR(hwa_txpost_schecmd_dens(hwa_dev, b, FALSE));
#endif // endif

	return 0;
} /* wlc_ampdu_tx_dump */

static int
wlc_ampdu_tx_scb_dump_scb(ampdu_tx_info_t *ampdu_tx, struct bcmstrbuf *b, struct scb *scb)
{
	scb_ampdu_tx_t *scb_ampdu;
#ifdef BCMDBG
	int tid;
#endif // endif

	if (SCB_AMPDU(scb)) {
		scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
		bcm_bprintf(b, MACF": max_pdu %d release %d\n",
			ETHER_TO_MACF(scb->ea),
			scb_ampdu->max_pdu, scb_ampdu->release);
#ifdef BCMDBG
		bcm_bprintf(b, "\ttxdrop %u txstuck %u "
				  "txaddbareq %u txrlag %u sdurejected %u txnoroom %u\n"
				  "\ttxmpdu %u txlost %u txbar %d txreg_noack %u noba %u "
				  "rxaddbaresp %u\n",
				  scb_ampdu->cnt.txdrop, scb_ampdu->cnt.txstuck,
				  scb_ampdu->cnt.txaddbareq, scb_ampdu->cnt.txrlag,
				  scb_ampdu->cnt.sdurejected, scb_ampdu->cnt.txnoroom,
				  scb_ampdu->cnt.txmpdu,
				  scb_ampdu->cnt.txlost, scb_ampdu->cnt.txbar,
				  scb_ampdu->cnt.txreg_noack, scb_ampdu->cnt.noba,
				  scb_ampdu->cnt.rxaddbaresp);
		for (tid = 0; tid < AMPDU_MAX_SCB_TID; tid++) {
			scb_ampdu_tid_ini_t *ini = scb_ampdu->ini[tid];
			if (!ini)
				continue;
			if (!(ini->ba_state == AMPDU_TID_STATE_BA_ON))
				continue;
			bcm_bprintf(b, "\tba_state %d ba_wsize %d tx_in_transit %d "
				"tid %d rem_window %d\n",
				ini->ba_state, ini->ba_wsize, ini->tx_in_transit,
				ini->tid, wlc_ampdu_get_rem_window(ini));
			bcm_bprintf(b, "\tstart_seq 0x%x max_seq 0x%x tx_exp_seq 0x%x"
				" bar_ackpending_seq 0x%x\n",
				ini->start_seq, ini->max_seq, ini->tx_exp_seq,
				ini->bar_ackpending_seq);
			bcm_bprintf(b, "\tbar_ackpending %d alive %d retry_bar %d\n",
				ini->bar_ackpending, ini->alive, ini->retry_bar);
			wlc_ampdu_atf_dump(AMPDU_ATF_STATE(ini), b);
#ifdef RAVG_SIMPLE
			bcm_bprintf(b, "SQS Ravg algo: Simple  avg_octet %d \n",
				scb_ampdu->pkt_avg_octet[ini->tid]);
#endif // endif
#ifdef RAVG_HISTORY
			bcm_bprintf(b, "SQS Ravg algo: History  avg_octet %d \n",
				scb_ampdu->pkt2oct[ini->tid]->pkt_avg_octet);
#endif // endif
		}
#endif /* BCMDBG */
	}

#ifdef WLPKTDLYSTAT
	wlc_scb_dlystat_dump(scb, b);
#endif /* WLPKTDLYSTAT */

	return 0;
}

int
wlc_ampdu_tx_scb_dump(ampdu_tx_info_t *ampdu_tx, struct bcmstrbuf *b)
{
	wlc_info_t *wlc = ampdu_tx->wlc;
	struct scb *scb;
	struct scb_iter scbiter;

	/* Only supported argument is
	 * -mac xx:xx:xx:xx:xx:xx -mac yy:yy:yy:yy:yy:yy (...)
	 */
	if (wlc->dump_args) {
		#define DUMP_AMPDU_ARGV_MAX 255
		char *args = wlc->dump_args;
		uint argc = 0;
		struct ether_addr ea;
		wlc_bsscfg_t *cfg;

		while ((!strncmp(args, "-mac ", 5)) && (argc < DUMP_AMPDU_ARGV_MAX)) {
			/* Convert following input into mac addr - assuming it is one  */
			args += 5;
			argc += 5;
			bcm_ether_atoe((const char*)args, &ea);
			scb = wlc_scbapfind(wlc, &ea, &cfg);

			/* Look for an scb with provided mac addr. Stop if none found  */
			if (scb != NULL) {
				wlc_ampdu_tx_scb_dump_scb(ampdu_tx, b, scb);
				args += 17;
				argc += 17;

				/* Remove leading spaces before looking for next argument  */
				while ((*args == ' ') && (argc < DUMP_AMPDU_ARGV_MAX)) {
					args++;
					argc++;
				}
			}
			else {
				goto exit;
			}
		}

		goto exit;
	}

	/* If no argument provided, dump all known scbs */
	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		wlc_ampdu_tx_scb_dump_scb(ampdu_tx, b, scb);
	}

exit:
	bcm_bprintf(b, "\n");
	return 0;
} /* wlc_ampdu_tx_sqs_dump */

#endif /* BCMDBG || WLTEST */

#ifdef BCMDBG
static void
ampdu_dump_ini(wlc_info_t *wlc, scb_ampdu_tid_ini_t *ini)
{
	printf("ba_state %d ba_wsize %d tx_in_transit %d tid %d rem_window %d\n",
		ini->ba_state, ini->ba_wsize, ini->tx_in_transit, ini->tid,
		wlc_ampdu_get_rem_window(ini));
	printf("start_seq 0x%x max_seq 0x%x tx_exp_seq 0x%x bar_ackpending_seq 0x%x\n",
		ini->start_seq, ini->max_seq, ini->tx_exp_seq, ini->bar_ackpending_seq);
	printf("bar_ackpending %d alive %d retry_bar %d\n",
		ini->bar_ackpending, ini->alive, ini->retry_bar);
	if (!AMPDU_AQM_ENAB(wlc->pub)) {
		scb_ampdu_tid_ini_non_aqm_t *non_aqm = ini->non_aqm;
		printf("retry_head %d retry_tail %d retry_cnt %d\n",
			non_aqm->retry_head, non_aqm->retry_tail,
			non_aqm->retry_cnt);
		prhex("ackpending", &non_aqm->ackpending[0], sizeof(non_aqm->ackpending));
		prhex("barpending", &non_aqm->barpending[0], sizeof(non_aqm->barpending));
		prhex("txretry", &non_aqm->txretry[0], sizeof(non_aqm->txretry));
		prhex("retry_seq", (uint8 *)&non_aqm->retry_seq[0], sizeof(non_aqm->retry_seq));
	}
}
#endif /* BCMDBG */

#if defined(WLAMPDU_UCODE)
/**
 * Sidechannel is a firmware<->ucode interface, intended to prepare the ucode for upcoming
 * transmits, so ucode can allocate data structures.
 */
void
wlc_sidechannel_init(wlc_info_t *wlc)
{
	ampdu_tx_info_t *ampdu_tx = wlc->ampdu_tx;
	uint16 txfs_waddr; /* side channel base addr in 16 bit units */
	uint16 txfsd_addr; /* side channel descriptor base addr */
	/* Size of side channel fifos in 16 bit units */
	uint8 txfs_wsz[AC_COUNT] =
	        {TXFS_WSZ_AC_BK, TXFS_WSZ_AC_BE, TXFS_WSZ_AC_VI, TXFS_WSZ_AC_VO};
	ampdumac_info_t *hwagg;
	int i;

	WL_TRACE(("wl%d: %s\n", wlc->pub->unit, __FUNCTION__));

	if (!AMPDU_MAC_ENAB(wlc->pub) || AMPDU_AQM_ENAB(wlc->pub)) {
		return;
	}

	ASSERT(ampdu_tx);

	if (AMPDU_UCODE_ENAB(wlc->pub)) {
		if (!(ampdu_tx->config->ini_enable[PRIO_8021D_VO] ||
			ampdu_tx->config->ini_enable[PRIO_8021D_NC])) {
			txfs_wsz[1] = txfs_wsz[1] + txfs_wsz[3];
			txfs_wsz[3] = 0;
		}
		if (!(ampdu_tx->config->ini_enable[PRIO_8021D_NONE] ||
			ampdu_tx->config->ini_enable[PRIO_8021D_BK])) {
				txfs_wsz[1] = txfs_wsz[1] + txfs_wsz[0];
				txfs_wsz[0] = 0;
		}
		ASSERT(txfs_wsz[0] + txfs_wsz[1] + txfs_wsz[2] + txfs_wsz[3] <= TOT_TXFS_WSIZE);
		txfs_waddr = wlc_read_shm(wlc, M_TXFS_PTR(wlc));
		txfsd_addr = (txfs_waddr + C_TXFSD_WOFFSET) * 2;
		for (i = 0; i < AC_COUNT; i++) {
			/* 16 bit word arithmetic */
			hwagg = &(ampdu_tx->hwagg[i]);
			hwagg->txfs_addr_strt = txfs_waddr;
			hwagg->txfs_addr_end = txfs_waddr + txfs_wsz[i] - 1;
			hwagg->txfs_wptr = hwagg->txfs_addr_strt;
			hwagg->txfs_rptr = hwagg->txfs_addr_strt;
			hwagg->txfs_wsize = txfs_wsz[i];
			/* write_shmem takes a 8 bit address and 16 bit pointer */
			wlc_write_shm(wlc, C_TXFSD_STRT_POS(txfsd_addr, i),
				hwagg->txfs_addr_strt);
			wlc_write_shm(wlc, C_TXFSD_END_POS(txfsd_addr, i),
				hwagg->txfs_addr_end);
			wlc_write_shm(wlc, C_TXFSD_WPTR_POS(txfsd_addr, i), hwagg->txfs_wptr);
			wlc_write_shm(wlc, C_TXFSD_RPTR_POS(txfsd_addr, i), hwagg->txfs_rptr);
			wlc_write_shm(wlc, C_TXFSD_RNUM_POS(txfsd_addr, i), 0);
			WL_AMPDU_HW(("%d: start 0x%x 0x%x, end 0x%x 0x%x, w 0x%x 0x%x,"
				" r 0x%x 0x%x, sz 0x%x 0x%x\n",
				i,
				C_TXFSD_STRT_POS(txfsd_addr, i), hwagg->txfs_addr_strt,
				C_TXFSD_END_POS(txfsd_addr, i),  hwagg->txfs_addr_end,
				C_TXFSD_WPTR_POS(txfsd_addr, i), hwagg->txfs_wptr,
				C_TXFSD_RPTR_POS(txfsd_addr, i), hwagg->txfs_rptr,
				C_TXFSD_RNUM_POS(txfsd_addr, i), 0));
			hwagg->txfs_wptr_addr = C_TXFSD_WPTR_POS(txfsd_addr, i);
			txfs_waddr += txfs_wsz[i];
		}
	}

#ifdef WLCNT
	ampdu_tx->cnt->pending = 0;
	ampdu_tx->cnt->cons = 0;
	ampdu_tx->cnt->enq = 0;
#endif /* WLCNT */

} /* wlc_sidechannel_init */

/**
 * For ucode aggregation: enqueue an entry to side channel (a.k.a. aggfifo) and inc the wptr \
 * For hw agg: it simply enqueues an entry to aggfifo.
 *
 * Only called for non-AC (non_aqm) chips.
 *
 * The actual capacity of the buffer is one less than the actual length
 * of the buffer so that an empty and a full buffer can be
 * distinguished.  An empty buffer will have the readPostion and the
 * writePosition equal to each other.  A full buffer will have
 * the writePosition one less than the readPostion.
 *
 * The Objects available to be read go from the readPosition to the writePosition,
 * wrapping around the end of the buffer.  The space available for writing
 * goes from the write position to one less than the readPosition,
 * wrapping around the end of the buffer.
 */
static int
aggfifo_enque_noaqm(ampdu_tx_info_t *ampdu_tx, int length, int qid)
{
	ampdumac_info_t *hwagg = &(ampdu_tx->hwagg[qid]);
	uint16 epoch = hwagg->epoch;
	uint32 entry;

	if (length > (MPDU_LEN_MASK >> MPDU_LEN_SHIFT)) {
		WL_AMPDU_ERR(("%s: Length too long %d\n", __FUNCTION__, length));
	}

	if (AMPDU_UCODE_ENAB(ampdu_tx->wlc->pub)) {
		uint16 rptr = hwagg->txfs_rptr;
		uint16 wptr_next;

		entry = length | (((epoch ? 1 : 0)) << MPDU_EPOCH_SHIFT);

		/* wptr always points to the next available entry to be written */
		if (hwagg->txfs_wptr == hwagg->txfs_addr_end) {
			wptr_next = hwagg->txfs_addr_strt;	/* wrap */
		} else {
			wptr_next = hwagg->txfs_wptr + 1;
		}

		if (wptr_next == rptr) {
			WL_ERROR(("%s: side channel %d is full !!!\n", __FUNCTION__, qid));
			ASSERT(0);
			return -1;
		}

		/* Convert word addr to byte addr */
		wlc_write_shm(ampdu_tx->wlc, hwagg->txfs_wptr * 2, entry);

		WL_AMPDU_HW(("%s: aggfifo %d rptr 0x%x wptr 0x%x entry 0x%04x\n",
			__FUNCTION__, qid, rptr, hwagg->txfs_wptr, entry));

		hwagg->txfs_wptr = wptr_next;
		wlc_write_shm(ampdu_tx->wlc, hwagg->txfs_wptr_addr, hwagg->txfs_wptr);
	}

	WLCNTINCR(ampdu_tx->cnt->enq);
	WLCNTINCR(ampdu_tx->cnt->pending);
	hwagg->in_queue ++;
	return 0;
} /* aggfifo_enque_noaqm */

#if defined(BCMDBG)
/**
 * Called for non AC chips only.
 * For ucode agg:
 *     Side channel queue is setup with a read and write ptr.
 *     - R == W means empty.
 *     - (W + 1 % size) == R means full.
 *     - Max buffer capacity is size - 1 elements (one element remains unused).
 */
static uint
get_aggfifo_space_noaqm(ampdu_tx_info_t *ampdu_tx, int qid)
{
	uint ret = 0;

	if (AMPDU_UCODE_ENAB(ampdu_tx->wlc->pub)) {
		if (ampdu_tx->hwagg[qid].txfs_wptr < ampdu_tx->hwagg[qid].txfs_rptr)
			ret = ampdu_tx->hwagg[qid].txfs_rptr - ampdu_tx->hwagg[qid].txfs_wptr - 1;
		else
			ret = (ampdu_tx->hwagg[qid].txfs_wsize - 1) -
			      (ampdu_tx->hwagg[qid].txfs_wptr - ampdu_tx->hwagg[qid].txfs_rptr);
		ASSERT(ret < ampdu_tx->hwagg[qid].txfs_wsize);
		WL_AMPDU_HW(("%s: fifo %d rptr %04x wptr %04x availcnt %d\n", __FUNCTION__,
			qid, ampdu_tx->hwagg[qid].txfs_rptr, ampdu_tx->hwagg[qid].txfs_wptr, ret));
	}

	return ret;
} /* get_aggfifo_space_noaqm */
#endif /* BCMDBG */

#endif /* WLAMPDU_UCODE */

#ifdef WLAMPDU_PRECEDENCE

/**
 * @param q   Multi-priority packet queue
 * returns TRUE on queueing succeeded
 */
static bool BCMFASTPATH
wlc_ampdu_prec_enq(wlc_info_t *wlc, struct pktq *q, void *pkt, int tid)
{
	return wlc_prec_enq_head(wlc, q, pkt, WLAMPDU_PREC_TID2PREC(pkt, tid), FALSE);
}

/** @param pq   Multi-priority packet queue
 * Internal function to dequeue packets from ampdu precendece queue
 */
static void * BCMFASTPATH
_wlc_ampdu_pktq_pdeq(struct pktq *pq, int tid)
{
	void *p;

	p = pktq_pdeq(pq, WLC_PRIO_TO_HI_PREC(tid));

	if (p == NULL)
		p = pktq_pdeq(pq, WLC_PRIO_TO_PREC(tid));

	return p;
}

/** @param pq   Multi-priority packet queue */
static void
wlc_ampdu_pktq_pflush(wlc_info_t *wlc, struct pktq *pq, int tid)
{
	wlc_txq_pktq_pflush(wlc, pq, WLC_PRIO_TO_HI_PREC(tid));
	wlc_txq_pktq_pflush(wlc, pq, WLC_PRIO_TO_PREC(tid));
}

#ifdef PROP_TXSTATUS
/** @param pq   Multi-priority packet queue */
static void
*ampdu_pktqprec_peek(struct pktq *pq, int tid)
{
	if (pktqprec_peek(pq, WLC_PRIO_TO_HI_PREC(tid)))
		return pktqprec_peek(pq, WLC_PRIO_TO_HI_PREC(tid));
	else
		return pktqprec_peek(pq, WLC_PRIO_TO_PREC(tid));
}
#endif /* PROP_TXSTATUS */
#endif /* WLAMPDU_PRECEDENCE */

#ifdef PROP_TXSTATUS
void wlc_ampdu_flush_pkts(wlc_info_t *wlc, struct scb *scb, uint8 tid)
{
	scb_ampdu_tx_t *scb_ampdu;
	scb_ampdu = SCB_AMPDU_TX_CUBBY(wlc->ampdu_tx, scb);
	wlc_ampdu_pktq_pflush(wlc, &scb_ampdu->txq, tid);
#ifdef WLTAF
	WL_TAFF(wlc, "soft reset\n");
	if (scb_ampdu->ini[tid]) {
		wlc_taf_link_state(wlc->taf_handle, scb, tid, TAF_AMPDU, TAF_LINKSTATE_SOFT_RESET);
#ifdef WLSQS
		wlc_taf_link_state(wlc->taf_handle, scb, tid, TAF_SQSHOST,
			TAF_LINKSTATE_SOFT_RESET);
#endif /* WLSQS */
	}
#endif /* WLTAF */
}

void
wlc_ampdu_flush_flowid_pkts(wlc_info_t *wlc, struct scb *scb, uint16 flowid)
{
	uint8 tid;
	scb_ampdu_tid_ini_t *ini;
	scb_ampdu_tx_t *scb_ampdu;
	void *p = NULL;
	scb_ampdu = SCB_AMPDU_TX_CUBBY(wlc->ampdu_tx, scb);
	for (tid = 0; tid < AMPDU_MAX_SCB_TID; tid++) {
		ini = scb_ampdu->ini[tid];
		if (!ini)
			continue;
		if (ampdu_pktqprec_n_pkts(&scb_ampdu->txq, tid)) {
			p = ampdu_pktqprec_peek(&scb_ampdu->txq, tid);
			/* All packets in the queue have the same flowid */
			if (p && flowid == PKTFRAGFLOWRINGID(wlc->osh, p)) {
				wlc_ampdu_flush_pkts(wlc, scb, tid);
				continue;
			}
		}
	}
}

#endif /* PROP_TXSTATUS */

#if defined(BCMDBG) || defined(WLTEST) || defined(WLPKTDLYSTAT) || \
	defined(BCMDBG_AMPDU)
#ifdef WLCNT
void
wlc_ampdu_clear_tx_dump(ampdu_tx_info_t *ampdu_tx)
{
#if defined(BCMDBG) || defined(WLPKTDLYSTAT)
	struct scb *scb;
	struct scb_iter scbiter;
#endif /* BCMDBG || WLPKTDLYSTAT */
#ifdef BCMDBG
	scb_ampdu_tx_t *scb_ampdu;
#endif /* BCMDBG */

	/* zero the counters */
	bzero(ampdu_tx->cnt, sizeof(wlc_ampdu_tx_cnt_t));
	/* reset the histogram as well */
	if (ampdu_tx->amdbg) {
#ifdef AMPDU_NON_AQM
		bzero(ampdu_tx->amdbg->retry_histogram, sizeof(ampdu_tx->amdbg->retry_histogram));
		bzero(ampdu_tx->amdbg->end_histogram, sizeof(ampdu_tx->amdbg->end_histogram));
#endif /* AMPDU_NON_AQM */
		bzero(ampdu_tx->amdbg->mpdu_histogram, sizeof(ampdu_tx->amdbg->mpdu_histogram));
		bzero(ampdu_tx->amdbg->supr_reason, sizeof(ampdu_tx->amdbg->supr_reason));
		bzero(ampdu_tx->amdbg->txmcs, sizeof(ampdu_tx->amdbg->txmcs));
		bzero(ampdu_tx->amdbg->txmcssgi, sizeof(ampdu_tx->amdbg->txmcssgi));
		bzero(ampdu_tx->amdbg->txmcsstbc, sizeof(ampdu_tx->amdbg->txmcsstbc));
		bzero(ampdu_tx->amdbg->txmcs_succ, sizeof(ampdu_tx->amdbg->txmcs_succ));

#ifdef WL11AC
		bzero(ampdu_tx->amdbg->txvht, sizeof(ampdu_tx->amdbg->txvht));
		bzero(ampdu_tx->amdbg->txvhtsgi, sizeof(ampdu_tx->amdbg->txvhtsgi));
		bzero(ampdu_tx->amdbg->txvhtstbc, sizeof(ampdu_tx->amdbg->txvhtstbc));
		bzero(ampdu_tx->amdbg->txvht_succ, sizeof(ampdu_tx->amdbg->txvht_succ));
#endif /* WL11AC */
#ifdef WL11AX
		bzero(ampdu_tx->amdbg->txhe, sizeof(ampdu_tx->amdbg->txhe));
		bzero(ampdu_tx->amdbg->txhe_succ, sizeof(ampdu_tx->amdbg->txhe_succ));
#endif /* WL11AX */
		bzero(ampdu_tx->amdbg->txmpdu, sizeof(ampdu_tx->amdbg->txmpdu));
		bzero(ampdu_tx->amdbg->txmpdu_succ, sizeof(ampdu_tx->amdbg->txmpdu_succ));
	}
#ifdef WLOVERTHRUSTER
	ampdu_tx->tcp_ack_info.tcp_ack_total = 0;
	ampdu_tx->tcp_ack_info.tcp_ack_dequeued = 0;
	ampdu_tx->tcp_ack_info.tcp_ack_multi_dequeued = 0;
#endif /* WLOVERTHRUSTER */
#if defined(BCMDBG) || defined(WLPKTDLYSTAT)
#ifdef WLPKTDLYSTAT
		/* reset the per-SCB delay statistics */
		wlc_dlystats_clear(ampdu_tx->wlc);
#endif /* WLPKTDLYSTAT */
	FOREACHSCB(ampdu_tx->wlc->scbstate, &scbiter, scb) {
#ifdef BCMDBG
		if (SCB_AMPDU(scb)) {
			/* reset the per-SCB statistics */
			scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
			bzero(&scb_ampdu->cnt, sizeof(scb_ampdu_cnt_tx_t));
		}
#endif /* BCMDBG */
	}
#endif /* BCMDBG || WLPKTDLYSTAT */

	if (ampdu_tx->wlc->clk) {
#ifdef WLAMPDU_UCODE
		if (ampdu_tx->amdbg) {
			bzero(ampdu_tx->amdbg->schan_depth_histo,
				sizeof(ampdu_tx->amdbg->schan_depth_histo));
		}
#endif // endif
		/* zero out shmem counters */
		if (AMPDU_UCODE_ENAB(ampdu_tx->wlc->pub)) {
			wlc_write_shm(ampdu_tx->wlc,
				MACSTAT_ADDR(ampdu_tx->wlc, MCSTOFF_TXMPDU), 0);
			wlc_write_shm(ampdu_tx->wlc,
				MACSTAT_ADDR(ampdu_tx->wlc, MCSTOFF_TXAMPDU), 0);
		}

		if (AMPDU_AQM_ENAB(ampdu_tx->wlc->pub)) {
			int i;
			uint16 stat_addr = wlc_read_shm(ampdu_tx->wlc,
				M_AMP_STATS_PTR(ampdu_tx->wlc));
			uint16 nbins = ARRAYSIZE(ampdu_tx->amdbg->mpdu_histogram);
			if (nbins > C_MPDUDEN_NBINS) {
				nbins = C_MPDUDEN_NBINS;
			}
			for (i = 0; i < nbins; i++)
				wlc_write_shm(ampdu_tx->wlc, shm_addr(stat_addr, i), 0);

			stat_addr = M_AGGSTOP_HISTO(ampdu_tx->wlc);
			for (i = 0; i <= 4; i ++)
				//stop_len, stop_mpdu, stop_bawin, stop_epoch, stop_fempty
				wlc_write_shm(ampdu_tx->wlc, stat_addr + i * 2, 0);

			stat_addr = M_MBURST_HISTO(ampdu_tx->wlc);
			for (i = 0; i < C_MBURST_NBINS; i++)
				wlc_write_shm(ampdu_tx->wlc, stat_addr + i * 2, 0);
		}
	}

#ifdef BCMHWA
	/* Clear HWA 3a schedule command histogram */
	HWA_TXPOST_EXPR(hwa_txpost_schecmd_dens(hwa_dev, NULL, TRUE));
#endif // endif
} /* wlc_ampdu_clear_tx_dump */

#endif /* WLCNT */
#endif // endif

/** Sends remote party a request to tear down the block ack connection. Does not wait for reply. */
void
wlc_ampdu_tx_send_delba(ampdu_tx_info_t *ampdu_tx, struct scb *scb, uint8 tid,
	uint16 initiator, uint16 reason)
{
	wlc_info_t *wlc = ampdu_tx->wlc;

	ampdu_cleanup_tid_ini(ampdu_tx, scb, tid, FALSE);

	WL_AMPDU(("wl%d: %s: tid %d initiator %d reason %d\n",
		wlc->pub->unit, __FUNCTION__, tid, initiator, reason));

	wlc_send_delba(wlc, scb, tid, initiator, reason);

	WLCNTINCR(ampdu_tx->cnt->txdelba);
}

void
wlc_ampdu_tx_recv_delba(ampdu_tx_info_t *ampdu_tx, struct scb *scb, uint8 tid, uint8 category,
	uint16 initiator, uint16 reason)
{
	scb_ampdu_tx_t *scb_ampdu_tx;
	scb_ampdu_tid_ini_t *ini;

	ASSERT(scb);

	scb_ampdu_tx = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	BCM_REFERENCE(scb_ampdu_tx);
	BCM_REFERENCE(initiator);
	BCM_REFERENCE(reason);
	ASSERT(scb_ampdu_tx);

	if (category & DOT11_ACTION_CAT_ERR_MASK) {
		WL_AMPDU_ERR(("wl%d: %s: unexp error action frame\n",
			ampdu_tx->wlc->pub->unit, __FUNCTION__));
		WLCNTINCR(ampdu_tx->cnt->rxunexp);
		return;
	}

	/* Set state to off, and push the packets in ampdu_pktq to next */
	ini = scb_ampdu_tx->ini[tid];
	if (ini) {
		/* Push the packets in ampdu_pktq to next to
		 * prevent packet lost
		 */
		ampdu_ba_state_off(scb_ampdu_tx, ini);
	}

	ampdu_cleanup_tid_ini(ampdu_tx, scb, tid, TRUE);

	WLCNTINCR(ampdu_tx->cnt->rxdelba);
	AMPDUSCBCNTINCR(scb_ampdu_tx->cnt.rxdelba);

	WL_AMPDU(("wl%d: %s: AMPDU OFF: tid %d initiator %d reason %d\n",
		ampdu_tx->wlc->pub->unit, __FUNCTION__, tid, initiator, reason));
}

void wlc_ampdu_upd_pm(wlc_info_t *wlc, uint8 PM_mode)
{
	int i;

	for (i = 0; i < AMPDU_MAX_SCB_TID; i++) {
		if (PM_mode == 1) {
			wlc->ampdu_tx->aqm_max_release[i] = AMPDU_AQM_RELEASE_MIN;
		} else {
			wlc->ampdu_tx->aqm_max_release[i] = AMPDU_AQM_RELEASE_DEFAULT;
		}
	}
}

/**
 * This function moves the window for the pkt freed during DMA TX reclaim
 * for which status is not received till flush for channel switch
 */
static void
wlc_ampdu_ini_adjust(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tid_ini_t *ini, void *pkt)
{
	uint16 seq = WLPKTTAG(pkt)->seq;
	uint16 bar_seq;

#ifdef PROP_TXSTATUS
	seq = WL_SEQ_GET_NUM(seq);
#endif /* PROP_TXSTATUS */

#ifdef PROP_TXSTATUS
	if (PROP_TXSTATUS_ENAB(ampdu_tx->wlc->pub) &&
		WLFC_GET_REUSESEQ(wlfc_query_mode(ampdu_tx->wlc->wlfc))) {
#ifdef PROP_TXSTATUS_SUPPR_WINDOW
		/* adjust the pkts in transit during flush */
		/* For F/W packets clear vectors */
		if (AMPDU_HOST_ENAB(ampdu_tx->wlc->pub) &&
			!(WL_TXSTATUS_GET_FLAGS(WLPKTTAG(pkt)->wl_hdr_information) &
			WLFC_PKTFLAG_PKTFROMHOST)) {
				uint16 indx = TX_SEQ_TO_INDEX(seq);
				if (isset(ini->non_aqm->ackpending, indx)) {
					clrbit(ini->non_aqm->ackpending, indx);
					if (isset(ini->non_aqm->barpending, indx)) {
						clrbit(ini->non_aqm->barpending, indx);
					}
				}
		} else { /* Suppresed packets */
			ini->suppr_window++;
		}
#endif /* PROP_TXSTATUS_SUPPR_WINDOW */
		/* clear retry array on return to host packets */
		if (AMPDU_HOST_ENAB(ampdu_tx->wlc->pub)) {
			uint16 indx = TX_SEQ_TO_INDEX(seq);
			if (ini->non_aqm->retry_cnt > 0) {
				ini->non_aqm->txretry[indx] = 0;
				ini->non_aqm->retry_tail = PREV_TX_INDEX(ini->non_aqm->retry_tail);
				ini->non_aqm->retry_seq[ini->non_aqm->retry_tail] = 0;
				ini->non_aqm->retry_cnt--;
			}
		}
		return;
	}
#endif /* PROP_TXSTATUS */

	if (!AMPDU_AQM_ENAB(ampdu_tx->wlc->pub)) {
		uint16 index = TX_SEQ_TO_INDEX(seq);
		if (!isset(ini->non_aqm->ackpending, index)) {
			return;
		}
		setbit(ini->non_aqm->barpending, index);
		bar_seq = SEQNUM_CHK;
	} else {
		bar_seq = MODINC_POW2(seq, SEQNUM_MAX);
	}
	wlc_ampdu_send_bar(ampdu_tx, ini, bar_seq);
} /* wlc_ampdu_ini_adjust */

#ifdef PROP_TXSTATUS
void wlc_ampdu_flush_ampdu_q(ampdu_tx_info_t *ampdu, wlc_bsscfg_t *cfg)
{
	struct scb_iter scbiter;
	struct scb *scb = NULL;
	scb_ampdu_tx_t *scb_ampdu;

	FOREACHSCB(ampdu->wlc->scbstate, &scbiter, scb) {
		if (scb->bsscfg == cfg) {
			if (!SCB_AMPDU(scb))
				continue;
			scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu, scb);
			wlc_wlfc_flush_queue(ampdu->wlc, &scb_ampdu->txq);
			wlc_check_ampdu_fc(ampdu, scb);
#ifdef WLTAF
			if (scb_ampdu) {
				int tid;
				WL_TAFF(ampdu->wlc, "soft reset "MACF"\n", ETHER_TO_MACF(scb->ea));
				for (tid = 0; tid < AMPDU_MAX_SCB_TID; tid ++) {
					if (scb_ampdu->ini[tid]) {
						wlc_taf_link_state(ampdu->wlc->taf_handle,
							scb, tid, TAF_AMPDU,
							TAF_LINKSTATE_SOFT_RESET);
#ifdef WLSQS
						wlc_taf_link_state(ampdu->wlc->taf_handle,
							scb, tid, TAF_SQSHOST,
							TAF_LINKSTATE_SOFT_RESET);
#endif /* WLSQS */
					}
				}
			}
#endif /* WLTAF */
		}
	}

}

void wlc_ampdu_send_bar_cfg(ampdu_tx_info_t * ampdu, struct scb *scb)
{
	scb_ampdu_tx_t *scb_ampdu;
	scb_ampdu_tid_ini_t *ini;
	uint32 pktprio;
	if (!scb || !SCB_AMPDU(scb))
		return;

	/* when reuse seq, no need to send bar */
	if (WLFC_GET_REUSESEQ(wlfc_query_mode(ampdu->wlc->wlfc))) {
		return;
	}

	if (SCB_DEL_IN_PROGRESS(scb))
		return;

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu, scb);

	for (pktprio = 0; pktprio <  AMPDU_MAX_SCB_TID; pktprio++) {
		ini = scb_ampdu->ini[pktprio];
		if (!ini) {
			continue;
		}

		if (!AMPDU_AQM_ENAB(ampdu->wlc->pub)) {
			wlc_ampdu_send_bar(ampdu, ini, SEQNUM_CHK);
		} else {
			/* We have no bitmap for AQM so use barpending_seq */
			if (ini->barpending_seq != SEQNUM_MAX) {
				wlc_ampdu_send_bar(ampdu, ini, ini->barpending_seq);
			}
		}
	}
}

#endif /* PROP_TXSTATUS */

/** returns (multi-prio) tx queue to use for a caller supplied scb (= one remote party) */
struct pktq* wlc_ampdu_txq(ampdu_tx_info_t *ampdu, struct scb *scb)
{
	scb_ampdu_tx_t *scb_ampdu;
	if (!scb || !SCB_AMPDU(scb))
		return NULL;
	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu, scb);
	return &scb_ampdu->txq;
}

/** PROP_TXSTATUS, flow control related */
void wlc_check_ampdu_fc(ampdu_tx_info_t *ampdu, struct scb *scb)
{
	scb_ampdu_tx_t *scb_ampdu;
	scb_ampdu_tid_ini_t *ini;
	int i;

	if (!scb || !SCB_AMPDU(scb)) {
		return;
	}

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu, scb);
	if (!scb_ampdu) {
		return;
	}

	for (i = 0; i < AMPDU_MAX_SCB_TID; i++) {
		ini = scb_ampdu->ini[i];
		if (ini) {
			wlc_ampdu_txflowcontrol(ampdu->wlc, scb_ampdu, ini);
		}
	}
}

#ifdef BCMDBG
struct wlc_ampdu_txq_prof_entry {
	uint32 timestamp;
	struct scb *scb;

	uint32 ampdu_q_len;
	uint32 rem_window;
	uint32 tx_in_transit;
	uint32 wlc_txq_len;	/* qlen of all precedence */
	uint32 wlc_pktq_qlen;	/* qlen of a single precedence */
	uint32 prec_map;
	uint32 txpkt_pend; /* TXPKTPEND */
	uint32 dma_desc_avail;
	uint32 dma_desc_pending; /* num of descriptor that has not been processed */
	const char * func;
	uint32 line;
	uint32 tid;
};

#define AMPDU_TXQ_HIS_MAX_ENTRY (1 << 7)
#define AMPDU_TXQ_HIS_MASK (AMPDU_TXQ_HIS_MAX_ENTRY - 1)

static  struct wlc_ampdu_txq_prof_entry _txq_prof[AMPDU_TXQ_HIS_MAX_ENTRY];
static  uint32 _txq_prof_index;
static  struct scb *_txq_prof_last_scb;
static  int 	_txq_prof_cnt;

void wlc_ampdu_txq_prof_enab(void)
{
	_txq_prof_enab = 1;
}

/** tx queue profiling */
void wlc_ampdu_txq_prof_add_entry(wlc_info_t *wlc, struct scb *scb, const char * func, uint32 line)
{
	struct wlc_ampdu_txq_prof_entry *p_entry;
	scb_ampdu_tx_t *scb_ampdu;
	scb_ampdu_tid_ini_t *ini;
	wlc_txq_info_t *qi;
	struct pktq *common_q;			/**< multi priority packet queue */

	if (!_txq_prof_enab)
		return;

	if (!scb)
		scb = _txq_prof_last_scb;

	if (!scb)
		return;

	scb_ampdu = SCB_AMPDU_TX_CUBBY(wlc->ampdu_tx, scb);
	if (!scb_ampdu) {
		return;
	}

	if (!(ini = scb_ampdu->ini[PRIO_8021D_BE])) {
		return;
	}

	/* only gather statistics for BE */
	if (ini->tid !=  PRIO_8021D_BE) {
		return;
	}

	if (prio2fifo[ini->tid] != TX_AC_BE_FIFO)
		return;

	_txq_prof_cnt++;
	if (_txq_prof_cnt == AMPDU_TXQ_HIS_MAX_ENTRY) {
		_txq_prof_enab = 0;
		_txq_prof_cnt = 0;
	}
	_txq_prof_index = (_txq_prof_index + 1) & AMPDU_TXQ_HIS_MASK;
	p_entry = &_txq_prof[_txq_prof_index];

	p_entry->timestamp = R_REG(wlc->osh, D11_TSFTimerLow(wlc));
	p_entry->scb = scb;
	p_entry->func = func;
	p_entry->line = line;
	p_entry->ampdu_q_len =
		ampdu_pktqprec_n_pkts((&scb_ampdu->txq), (ini->tid));
	p_entry->rem_window = wlc_ampdu_get_rem_window(ini);
	p_entry->tx_in_transit = ini->tx_in_transit;
	qi = SCB_WLCIFP(scb)->qi;
	common_q = WLC_GET_CQ(qi);
	p_entry->wlc_txq_len = pktq_n_pkts_tot(common_q);
	p_entry->wlc_pktq_qlen = pktqprec_n_pkts(common_q, WLC_PRIO_TO_PREC(ini->tid));
	p_entry->prec_map = txq_stopped_map(qi->low_txq);

	p_entry->txpkt_pend = TXPKTPENDGET(wlc, (prio2fifo[ini->tid])),
	p_entry->dma_desc_avail = TXAVAIL(wlc, (prio2fifo[ini->tid]));
	p_entry->dma_desc_pending = dma_txpending(WLC_HW_DI(wlc, (prio2fifo[ini->tid])));

	_txq_prof_last_scb = scb;

	p_entry->tid = ini->tid;
} /* wlc_ampdu_txq_prof_add_entry */

void wlc_ampdu_txq_prof_print_histogram(int entries)
{
	int i, index;
	struct wlc_ampdu_txq_prof_entry *p_entry;

	printf("TXQ HISTOGRAM\n");

	index = _txq_prof_index;
	if (entries == -1)
		entries = AMPDU_TXQ_HIS_MASK;
	for (i = 0; i < entries; i++) {
		p_entry = &_txq_prof[index];
		printf("ts: %u	@ %s:%d\n", p_entry->timestamp, p_entry->func, p_entry->line);
		printf("ampdu_q  rem_win   in_trans    wlc_q prec_map pkt_pend "
			"Desc_avail Desc_pend\n");
		printf("%7d  %7d  %7d  %7d %7x %7d %7d   %7d\n",
			p_entry->ampdu_q_len,
			p_entry->rem_window,
			p_entry->tx_in_transit,
			p_entry->wlc_txq_len,
			p_entry->prec_map,
			p_entry->txpkt_pend,
			p_entry->dma_desc_avail,
			p_entry->dma_desc_pending);
		index = (index - 1) & AMPDU_TXQ_HIS_MASK;
	}
}

#endif /* BCMDBG */

#ifdef WL_CS_RESTRICT_RELEASE
static void
wlc_ampdu_txeval_all_tid(wlc_info_t *wlc, uint8 tid)
{
	struct scb *scb;
	struct scb_iter scbiter;

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (SCB_AMPDU(scb)) {
			scb_ampdu_tx_t *scb_ampdu = SCB_AMPDU_TX_CUBBY(wlc->ampdu_tx, scb);
			if (scb_ampdu) {
				scb_ampdu_tid_ini_t *ini = scb_ampdu->ini[tid];
				if (ini) {
					wlc_ampdu_txeval(wlc->ampdu_tx,
						scb_ampdu, ini, FALSE);
				}
			}
		}
	}
}

void
wlc_ampdu_txeval_all(wlc_info_t *wlc)
{
	uint8 tid;
#ifdef WLTAF
	bool taf = wlc_taf_reset_scheduling(wlc->taf_handle, ALLPRIO, FALSE);
	BCM_REFERENCE(taf);
	WL_TAFF(wlc, "%u\n", taf);
#endif /* WLTAF */

	for (tid = 0; tid < AMPDU_MAX_SCB_TID; tid++) {
		wlc_ampdu_txeval_all_tid(wlc, tid);
	}
}
#endif /* WL_CS_RESTRICT_RELEASE */

void
wlc_ampdu_agg_state_update_tx_all(wlc_info_t *wlc, bool aggr)
{
	int idx;
	wlc_bsscfg_t *cfg;

	FOREACH_AS_BSS(wlc, idx, cfg) {
		wlc_ampdu_tx_set_bsscfg_aggr_override(wlc->ampdu_tx, cfg, aggr);
	}
}

#if defined(WL_LINKSTAT)
void
wlc_ampdu_txrates_get(ampdu_tx_info_t *ampdu_tx, wifi_rate_stat_t *rate, int i, int rs)
{
	switch (rs) {
#ifdef WL11AX
	case AMPDU_11HE:
		if (i < AMPDU_MAX_HE) {
			rate->tx_mpdu = ampdu_tx->amdbg->txhe[WL_RSPEC_HE_2x_LTF_GI_1_6us][i];
			rate->mpdu_lost = rate->retries = rate->tx_mpdu -
				ampdu_tx->amdbg->txhe_succ[WL_RSPEC_HE_2x_LTF_GI_1_6us][i];
		}
		break;
#endif /* WL11AX */
#ifdef WL11AC
	case AMPDU_11VHT:
		if (i < AMPDU_MAX_VHT) {
			rate->tx_mpdu = ampdu_tx->amdbg->txvht[i];
			rate->mpdu_lost = rate->retries = rate->tx_mpdu -
				ampdu_tx->amdbg->txvht_succ[i];
		}
		break;
#endif /* WL11AC */
	case AMPDU_11N:
		if (i < AMPDU_HT_MCS_ARRAY_SIZE) {
			rate->tx_mpdu = ampdu_tx->amdbg->txmcs[MCS2IDX(i)];
			rate->mpdu_lost = rate->retries = rate->tx_mpdu -
				ampdu_tx->amdbg->txmcs_succ[MCS2IDX(i)];
		}
		break;
	default:
		break;
	}
}
#endif /* WL_LINKSTAT */

#ifdef BCMDBG_TXSTUCK
void
wlc_ampdu_print_txstuck(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	ampdu_tx_info_t *ampdu_tx = wlc->ampdu_tx;
	scb_ampdu_tx_t *scb_ampdu;
	scb_ampdu_tid_ini_t *ini;
	struct scb *scb;
	struct scb_iter scbiter;
	uint8 tid;

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {

		if (!SCB_AMPDU(scb)) {
			continue;
		}

		scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);

		ASSERT(scb_ampdu);

		for (tid = 0; tid < AMPDU_MAX_SCB_TID; tid++) {
			ini = scb_ampdu->ini[tid];
			if (!ini) {
				continue;
			}

			bcm_bprintf(b, "tid(%d): %d in transit\n", tid, ini->tx_in_transit);
		}
	}
}
#endif /* BCMDBG_TXSTUCK */

void
wlc_ampdu_set_epoch(ampdu_tx_info_t *ampdu_tx, int fifo, uint8 epoch)
{
	ASSERT(fifo < NFIFO_EXT_MAX);
	ASSERT(AMPDU_AQM_ENAB(ampdu_tx->wlc->pub));

	ampdu_tx->hwagg[fifo].epoch = epoch;
	/* set prev_ft to AMPDU_11VHT, so wlc_ampdu_chgnsav_epoch will always be called. */
	ampdu_tx->hwagg[fifo].prev_ft = AMPDU_11VHT;
} /* wlc_ampdu_set_epoch */

#ifdef WL11K_ALL_MEAS
void wlc_ampdu_get_stats(wlc_info_t *wlc, rrm_stat_group_12_t *g12)
{
	ASSERT(wlc);
	ASSERT(g12);

	if (wlc->clk && AMPDU_MAC_ENAB(wlc->pub)) {
		if (AMPDU_AQM_ENAB(wlc->pub)) {
			uint16 stat_addr = 0;
			uint16 val;
			uint32 total;
			int cnt = 0, i;

			stat_addr = 2 * wlc_read_shm(wlc, M_AMP_STATS_PTR(wlc));
			for (i = 0, total = 0; i < C_MPDUDEN_NBINS; i++) {
				val = wlc_read_shm(wlc, stat_addr + i * 2);
				total += val;
				cnt += (val * (i+1));
			}
			g12->txampdu = total;
			g12->txmpdu = cnt;
		} else {
			g12->txampdu = wlc_read_shm(wlc, MACSTAT_ADDR(wlc, MCSTOFF_TXAMPDU));
			g12->txmpdu = wlc_read_shm(wlc, MACSTAT_ADDR(wlc, MCSTOFF_TXMPDU));
		}
	} else {
		g12->txampdu = wlc->ampdu_tx->cnt->txampdu;
		g12->txmpdu = wlc->ampdu_tx->cnt->txmpdu;
	}
	g12->txampdubyte_h = wlc->ampdu_tx->cnt->txampdubyte_h;
	g12->txampdubyte_l = wlc->ampdu_tx->cnt->txampdubyte_l;

	/* from wlc_ampdu_rx.c */
	g12->rxampdu = wlc_ampdu_getstat_rxampdu(wlc);
	g12->rxmpdu = wlc_ampdu_getstat_rxmpdu(wlc);
	g12->rxampdubyte_h = wlc_ampdu_getstat_rxampdubyte_h(wlc);
	g12->rxampdubyte_l = wlc_ampdu_getstat_rxampdubyte_l(wlc);
	g12->ampducrcfail = wlc_ampdu_getstat_ampducrcfail(wlc);
}
#endif /* WL11K_ALL_MEAS */

#ifdef WL_MU_TX
bool
wlc_ampdu_scbstats_get_and_clr(wlc_info_t *wlc, struct scb *scb,
	ampdu_tx_scb_stats_t *ampdu_scb_stats)
{
	ampdu_tx_info_t *ampdu_tx = wlc ? wlc->ampdu_tx : NULL;
	scb_ampdu_tx_t *scb_ampdu = NULL;

	if ((!ampdu_scb_stats) || (!ampdu_tx))
		return FALSE;

	if (SCB_AMPDU(scb)) {
		scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
		memcpy(ampdu_scb_stats, scb_ampdu->ampdu_scb_stats, sizeof(ampdu_tx_scb_stats_t));
		memset(scb_ampdu->ampdu_scb_stats, 0, sizeof(ampdu_tx_scb_stats_t));
		return TRUE;
	}

	return FALSE;
}
#endif /* WL_MU_TX */

void
wlc_ampdu_reset_txnoprog(ampdu_tx_info_t *ampdu_tx)
{
	if (!ampdu_tx) {
		return;
	}
	ampdu_tx->txnoprog_cnt = 0;
}

void
wlc_ampdu_reqmpdu_to_aqm_aggregatable(wlc_info_t *wlc, void *p)
{
	d11txhdr_t* txh = (d11txhdr_t*)PKTDATA(wlc->osh, p);
	d11txh_cache_common_t *cacheInfo_common;
	wlc_pkttag_t *tag = WLPKTTAG(p);
	uint16 *maclow;
	uint16 *machigh;

	ASSERT(AMPDU_AQM_ENAB(wlc->pub));

	tag->flags |= WLF_AMPDU_MPDU;
	maclow = D11_TXH_GET_MACLOW_PTR(wlc, txh);
	machigh = D11_TXH_GET_MACHIGH_PTR(wlc, txh);

	*maclow |= htol16((D11AC_TXC_AMPDU | D11AC_TXC_IACK));
	*machigh &= htol16(~(D11AC_TXC_SVHT));
	if (D11REV_GE(wlc->pub->corerev, 128)) {
		ASSERT(RATELINKMEM_ENAB(wlc->pub));
		return; /* nothing can be done here, link_entry should be ok already */
	} else if (D11REV_GE(wlc->pub->corerev, 64)) {
		cacheInfo_common = &txh->rev40.rev64.CacheInfo.common;
	} else {
		cacheInfo_common = &txh->rev40.rev40.CacheInfo.common;
	}
	cacheInfo_common->PrimeMpduMax = AMPDU_DEF_MAX_RELEASE_AQM;
	cacheInfo_common->FallbackMpduMax = AMPDU_DEF_MAX_RELEASE_AQM;
	cacheInfo_common->AmpduDur = AMPDU_MAX_DUR;
	cacheInfo_common->AmpduDur |= AMPDU_MIN_DUR_IDX_16us;
	cacheInfo_common->BAWin = AMPDU_DEF_MAX_RELEASE_AQM - 1;
	cacheInfo_common->MaxAggLen = AMPDU_DEF_MAX_RX_FACTOR;
}

/** PKTENG function to fill link mem entry (rev ge128), called by wlc_tx_fill_link_entry */
void
wlc_ampdu_fill_link_entry_pkteng(wlc_info_t *wlc, d11linkmem_entry_t *link_entry)
{
	int tid;

	ASSERT(link_entry != NULL);

#if defined(WLTEST) || defined(WLPKTENG)
	link_entry->PPET_AmpMinDur = htol32(0xe38e38);
#endif // endif

	link_entry->ampdu_info = htol16((AMPDU_DEF_MAX_RELEASE_AQM <<
		D11_REV128_AMPDUMPDUALL_SHIFT) |
		(AMPDU_DEF_MAX_RX_FACTOR & D11_REV128_MAXRXFACTOR_MASK));
	link_entry->PPET_AmpMinDur |= htol32(MPDU_DENSITY_REV128(AMPDU_DEF_MPDU_DENSITY)
					<< D11_REV128_AMPMINDUR_SHIFT);
	link_entry->AmpMaxDurIdx = htol16(AMPDU_MAX_DUR);
#if defined(WLTEST) || defined(WLPKTENG)
	/* overwrite max/min dur with pkteng settings if ptkeng is enabled */
	if (wlc_test_pkteng_en(wlc->testi)) {
		int pktengcfg_max_dur, pktengcfg_min_dur;
		pktengcfg_max_dur = wlc_test_pkteng_get_max_dur(wlc->testi);
		pktengcfg_min_dur = wlc_test_pkteng_get_min_dur(wlc->testi);
		if (pktengcfg_max_dur != 0) {
			link_entry->AmpMaxDurIdx = htol16(pktengcfg_max_dur);
		}
		link_entry->PPET_AmpMinDur &= htol32(~D11_REV128_AMPMINDUR_MASK);
		link_entry->PPET_AmpMinDur |=
			htol32(pktengcfg_min_dur << D11_REV128_AMPMINDUR_SHIFT);
	}
#endif // endif

	for (tid = 0; tid < AMPDU_MAX_SCB_TID; tid++) {
		link_entry->ampdu_ptid[tid].BAWin = AMPDU_DEF_MAX_RELEASE_AQM - 1;
		link_entry->ampdu_ptid[tid].ampdu_mpdu = AMPDU_DEF_MAX_RELEASE_AQM - 1;
	}
}

/* Evaluate and release all pkts for a given scb
 * should be called for AMPDU capable scb only
 */
void
wlc_ampdu_txeval_alltid(wlc_info_t *wlc, struct scb* scb, bool force)
{
	scb_ampdu_tx_t *scb_ampdu = NULL;
	uint8 tid;

	ASSERT(wlc->ampdu_tx);
#ifdef WLTAF
	if (wlc_taf_in_use(wlc->taf_handle)) {
		return;
	}
#endif /* WLTAF */

	scb_ampdu = SCB_AMPDU_TX_CUBBY(wlc->ampdu_tx, scb);

	/* scb_ampdu will be NULL if called from wl down path */
	if (!scb_ampdu) {
	    return;
	}

	for (tid = 0; tid < AMPDU_MAX_SCB_TID; tid++) {
		scb_ampdu_tid_ini_t *ini = scb_ampdu->ini[tid];

		if (ini) {
			wlc_ampdu_txeval(wlc->ampdu_tx, scb_ampdu, ini,	force);
		}
	}

	return;
}

#ifdef WLCFP
/** CFP Module Debug formatting */
#define WLC_CFPAMPDU_FMT \
	"%-10s %-10s: scb_cfp %p scb_ampdu %p ini %p tid %u "
#define WLC_CFPAMPDU_ARG(str) \
	"CFP_AMPDU", str, scb_cfp, scb_ampdu, ini, ini->tid

/**
 * Return an opaque pointer to AMPDU SCB cubby. Members of the cubby won't be
 * accessed (data hiding).  Opaque pointer is stored in TCB block for easy
 * access upon CFP TX fastpath entry into AMPDU module.
 */
void *
wlc_cfp_get_ampdu_tx_cubby(wlc_info_t *wlc, struct scb *scb)
{
	ASSERT(wlc->ampdu_tx);
	ASSERT(scb);

	return (SCB_AMPDU(scb) ? SCB_AMPDU_TX_CUBBY(wlc->ampdu_tx, scb) : NULL);
}
/* Get scb ampdu queue length for given prio */
uint16
wlc_cfp_get_scbq_len(scb_cfp_t *scb_cfp, uint8 prio)
{
	scb_ampdu_tx_t *scb_ampdu;

	ASSERT(scb_cfp);
	ASSERT(prio < AMPDU_MAX_SCB_TID);

	/* Fetch SCB AMPDU cubby and INI saved in TCB */
	scb_ampdu = SCB_CFP_AMPDU_TX(scb_cfp);
	ASSERT(scb_ampdu);

	return pktq_mlen(&scb_ampdu->txq, (1 << prio));
}
/*
 * ------------- Section: CFP AMPDU Transmit Fastpath --------------------------
 *
 * CFP AMPDU TXENTRY : ENQUEUE -> SCHEDULE -> EVALUATE -> RELEASE
 */
extern void wlc_cfp_ampdu_reenque_pkts(wlc_info_t *wlc, scb_ampdu_tx_t *scb_ampdu,
	uint8 tid, void* head);

#define wlc_cfp_ampdu_txflowcontrol(scb_cfp, scb_ampdu, ini)    WLC_CFP_NOOP

/* 5.3 AMPDU epoch update */
static INLINE uint8 wlc_cfp_ampdu_update_epoch(ampdu_tx_info_t *ampdu_tx,
	int fifo, int reason_code, struct scb *scb, uint8 tid, bool htc, uint8 cache_gen,
	uint8* tsoHdrPtr);

/* 5.2 TXD fixups after cache updates for MACREV >= 40 & < 128 */
static INLINE void wlc_cfp_ampdu_txd_ge40_fixup(scb_cfp_t *scb_cfp,
	scb_ampdu_tx_t *scb_ampdu, scb_ampdu_tid_ini_t *ini, void* pkt,
	uint8 fifo, d11txhdr_t *txh, uint8 *iv_body);
/* 5.2 TXD fixups after cache updates for MACREV >= 128 */
static INLINE void wlc_cfp_ampdu_txd_ge128_fixup(scb_cfp_t *scb_cfp,
	scb_ampdu_tx_t *scb_ampdu, scb_ampdu_tid_ini_t *ini, void* pkt,
	uint8 fifo, d11txhdr_t *txh, uint8 *iv_body);

/* 5.1 TXCache Fixups[ dot11 header Section] */
static INLINE void wlc_cfp_ampdu_d11hdr_fixups(scb_cfp_t *scb_cfp,
	scb_ampdu_tid_ini_t *ini, void* pkt,
	d11txhdr_t *txh, char *da, char *sa);

/* 5. PREPARE: Prepare MPDU for transmission to AQM */
static INLINE void wlc_cfp_ampdu_prepare(scb_cfp_t *scb_cfp,
	scb_ampdu_tx_t *scb_ampdu, scb_ampdu_tid_ini_t *ini, void *pkt, uint8 fifo);

/* 4. RELEASE: Release packets from AMPDU Q with aggregation. */
static INLINE uint16 wlc_cfp_ampdu_release(scb_cfp_t *scb_cfp,
	scb_ampdu_tx_t *scb_ampdu, scb_ampdu_tid_ini_t *ini,
	uint16 release_cnt, bool amsdu_in_ampdu, taf_scheduler_public_t* taf);

/* 3. EVALUATE: Evaluate number of packets that may be released. */
static INLINE void wlc_cfp_ampdu_evaluate(scb_cfp_t *scb_cfp,
	scb_ampdu_tx_t *scb_ampdu, scb_ampdu_tid_ini_t *ini, bool force);

/* 2. SCHEDULE: Attempt to release packets for aggregation. */
static INLINE void wlc_cfp_ampdu_schedule(scb_cfp_t *scb_cfp,
	scb_ampdu_tx_t *scb_ampdu, scb_ampdu_tid_ini_t *ini,
	const bool force_release);

/* 1. ENQUEUE: Append packet lists to AMPDU precedence queue */
static INLINE int wlc_cfp_ampdu_enqueue(struct pktq *pq, uint8 prec,
	void *pktlist_head, void *pktlist_tail, uint16 pktlist_count);

/**
 * 5.3 Update epoch & save parameters used to determine epoch
 */
static INLINE uint8
wlc_cfp_ampdu_update_epoch(ampdu_tx_info_t *ampdu_tx, int fifo, int reason_code,
	struct scb *scb, uint8 tid, bool htc, uint8 cache_gen, uint8* tsoHdrPtr)
{
	ampdumac_info_t *hwagg;

	hwagg = &(ampdu_tx->hwagg[fifo]);

	/* when switch from non-aggregate ampdu to aggregate ampdu just change
	 * epoch determining parameters. do not change epoch.
	 */
	if (reason_code != AMU_EPOCH_NO_CHANGE) {
		wlc_ampdu_change_epoch(ampdu_tx, fifo, reason_code);
	}

	/* Store current agg specifics */
	hwagg->prev_scb = scb;
	hwagg->prev_tid = tid;
	hwagg->prev_htc = htc;

	/** XXX
	 * Not differentiating between AMPDU_11VHT & AMPDU_11N
	 * Reasoning:
	 * Only use case seem to be if AMPDU_NONE or not to flip epoch
	 * No value add in differntiating between VHT or HT
	 * bring back if issues seen in epoch flipping
	 */
	hwagg->prev_ft = AMPDU_11VHT;
	hwagg->prev_cache_gen = cache_gen;
	hwagg->prev_shdr = (tsoHdrPtr[2] & TOE_F2_TXD_HEAD_SHORT);

	WL_AMPDU_HWDBG(("%s fifo %d epoch %d\n", __FUNCTION__, fifo, hwagg->epoch));

	return hwagg->epoch;
}
/**
 * TXD fixups for MACREV >= 128
 *
 *  5.2 TX Descriptor Fixups
 * 1. TxFrameID Fixups
 * 2. Time stamp Fixups
 * 3. Sequence number fixups
 * 4. Framelen Fixups
 * 5. cacheinfo[TSCPN], kreymgmt fixups
 * 6. Maccontrol word fixups
 * 7. Epoch Fixups
 */
/* XXX  Assumptions:
 * 1. Ignoring fifo based calculation[BCMC not expected]
 * 2. Only for 11AC + chips
 */
static INLINE void
wlc_cfp_ampdu_txd_ge128_fixup(scb_cfp_t *scb_cfp, scb_ampdu_tx_t *scb_ampdu,
	scb_ampdu_tid_ini_t *ini, void* pkt, uint8 fifo, d11txhdr_t *txh, uint8 *iv_body)
{
	uint16 frameid; /* frameID fixup */
	uint16 pktlen;	/* tot pktlen */
	uint tsoHdrSize; /* TSO header size */
	bool change; /* epoch change */
	int8 reason_code; /* reason for epoch flip */
	ampdumac_info_t *hwagg; /* epoch Update info */
	wlc_info_t *wlc; /* WLC info */
	struct dot11_header *h; /* dot11 header ptr */
	struct scb *scb;	/* Station control block */
	uint16 *maclow;		/* Mac control word low */
	uint16 *machigh;	/* mac control word high */
	uint8 ac;
	uint8* tsoHdrPtr;
	uint16 fc;
	bool htc;

	/* Initialize */
	wlc = scb_ampdu->ampdu_tx->wlc;
	scb = SCB_CFP_SCB(scb_cfp);
	hwagg = &(scb_ampdu->ampdu_tx->hwagg[fifo]);
	ac = WME_PRIO2AC(ini->tid);
	ASSERT(ac < AC_COUNT);

	BCM_REFERENCE(ac);

	/* Check for MAC REV */
	ASSERT(D11REV_GE(wlc->pub->corerev, 128));

	/* dot11 hdr ptr */
	h = (struct dot11_header*) (((char*)txh) + D11_REV128_TXH_LEN_EX);

	 /* Fixup counter in TxFrameID */
	ASSERT(fifo != TX_BCMC_FIFO);
	frameid = wlc_compute_frameid(wlc, 0, fifo);
	txh->rev128.FrameID = htol16(frameid);

	/* Update TXD sequence number: copy from dot11 */
	txh->rev128.SeqCtl = h->seq;

	/* Get the packet length after adding d11 header. */
	pktlen = pkttotlen(wlc->osh, pkt);

	/* Update frame len (fc to fcs) */
#ifdef WLTOEHW
	tsoHdrSize = WLC_TSO_HDR_LEN(wlc, (d11ac_tso_t*)PKTDATA(wlc->osh, pkt));
	tsoHdrPtr = PKTDATA(wlc->osh, pkt);
#else
	tsoHdrSize = 0;
	tsoHdrPtr = NULL;
#endif // endif
	txh->rev128.FrmLen =
		htol16((uint16)(pktlen +  DOT11_FCS_LEN - D11_TXH_LEN_EX(wlc) - tsoHdrSize));

	/* Keymgmt updates on iv and txd */
	if (SCB_CFP_IV_LEN(scb_cfp)) {
		/*
		 * 'iv_body' was pointing to end of d11header
		 * decrement it by iv length to get to the start of iv
		 */
		iv_body = iv_body - SCB_CFP_IV_LEN(scb_cfp);

		/* key updates for AES
		 * 1. increment sequence number
		 * 2. Update pkt body[8 bytes iv]
		 * 3. update tx descriptot cache info
		 */
		key_aes_tx_mpdu_fast(SCB_CFP_KEY(scb_cfp), iv_body, txh);
	}

	//ASSERT((pktflags & WLF_FIFOPKT) == 0);

	fc = ltoh16(h->fc);
#ifdef WLTOEHW
	/* Check for DATA packets only */
	/* TSO headers should be updated from cache. No change expected per frame */
	ASSERT(FC_TYPE(fc) == FC_TYPE_DATA);
	ASSERT(GE128_FC_2_TXH_FT(FC_TYPE(fc)) ==
		((tsoHdrPtr[1] & TOE_F1_FT_MASK)  >> TOE_F1_FT_SHIFT));
	/**
	 * 43684 does support, so FRAG_ALLOW should be set, but till the moment the functionality
	 * is completely verified it is disabled, see wlc_txh_set_frag_allow (wlc_tx.c).
	*/
	ASSERT(!(tsoHdrPtr[1] & TOE_F1_FRAG_ALLOW));
#endif /* WLTOEHW */

	/* XXX Optimize: remove these AMPDU specific fixups
	 * cache should have been setup with AMPDU settings
	 */

	maclow = D11_TXH_GET_MACLOW_PTR(wlc, txh);
	machigh = D11_TXH_GET_MACHIGH_PTR(wlc, txh);

	ASSERT(!(WLPKTTAG(pkt)->flags3 & WLF3_AMPDU_REGMPDU));

	/* clear svht bit, set ampdu bit */
	*maclow |= htol16(D11AC_TXC_AMPDU);

	/* for real ampdu, clear vht single mpdu ampdu bit */
	*machigh &= htol16(~D11AC_TXC_SVHT);

	/* Update Epoch */
	change = FALSE;

	htc = FALSE;
	if ((scb_ampdu->ampdu_tx->cfp_head_frame) || (hwagg->prev_htc)) {
		/* Evaluate epoch update changes for just the head frame */
		change = TRUE;
		htc = HE_FC_IS_HTC(fc);

		/* Reset head frame flag */
		scb_ampdu->ampdu_tx->cfp_head_frame = FALSE;
		if ((scb != hwagg->prev_scb) | (ini->tid != hwagg->prev_tid)) {
			reason_code = AMU_EPOCH_CHG_DSTTID;
		} else if (ltoh16(*maclow) & D11AC_TXC_UPD_CACHE) {
			reason_code = AMU_EPOCH_CHG_TXC_UPD;
#ifdef WLTOEHW
		} else if ((tsoHdrPtr[2] & TOE_F2_TXD_HEAD_SHORT) !=
			hwagg->prev_shdr) {
			reason_code = AMU_EPOCH_CHG_TXHDR;
#endif // endif
		} else if (htc || hwagg->prev_htc) {
			reason_code = AMU_EPOCH_CHG_HTC;
		} else {
			change = FALSE;
		}
	}

	/* Check prev_ft == AMPDU_NONE */
	if (!change && hwagg->prev_ft == AMPDU_NONE) {
		/* switching from non-aggregate to aggregate ampdu,
		 * just update the epoch changing parameters(hwagg) but
		 * not the epoch.
		 */
		change = TRUE;
		reason_code = AMU_EPOCH_NO_CHANGE;
	}

	if (change) {
		/* If there are cache changes, consider next frame as head again */
		if (ltoh16(*maclow) & D11AC_TXC_UPD_CACHE) {
			scb_ampdu->ampdu_tx->cfp_head_frame = TRUE;
		}

		wlc_cfp_ampdu_update_epoch(scb_ampdu->ampdu_tx, fifo, reason_code,
			scb, ini->tid, htc, SCB_CFP_CACHE_GEN(scb_cfp), tsoHdrPtr);
	}

	/* always set epoch for ampdu */
	wlc_txh_set_epoch(wlc, PKTDATA(PKT_OSH_NA, pkt), hwagg->epoch);

	/* Update MU counters */
	if (ltoh16(*machigh) & D11AC_TXC_MU)
	        SCB_CFP_MUPKT_CNT(scb_cfp, ini->tid)++;
}
/**
 * TXD fixups for MACREV >= 40 and < 128
 *
 *  5.2 TX Descriptor Fixups
 * 1. TxFrameID Fixups
 * 2. Time stamp Fixups
 * 3. Sequence number fixups
 * 4. Framelen Fixups
 * 5. cacheinfo[TSCPN], kreymgmt fixups
 * 6. Maccontrol word fixups
 * 7. Epoch Fixups
 */
/* XXX  Assumptions:
 * 1. Ignoring fifo based calculation[BCMC not expected]
 * 2. Only for 11AC + chips
 */
static INLINE void
wlc_cfp_ampdu_txd_ge40_fixup(scb_cfp_t *scb_cfp, scb_ampdu_tx_t *scb_ampdu,
	scb_ampdu_tid_ini_t *ini, void* pkt, uint8 fifo, d11txhdr_t *txh, uint8 *iv_body)
{
	uint16 frameid; /* frameID fixup */
	uint tsoHdrSize; /* TSO header size */
	bool change; /* epoch change */
	int8 reason_code; /* reason for epoch flip */
	ampdumac_info_t *hwagg; /* epoch Update info */
	wlc_info_t *wlc; /* WLC info */
	d11actxh_pkt_t *pktinfo; /* PKTINFO section in TXD */
	struct dot11_header *h; /* dot11 header ptr */
	struct scb *scb; /* Station control block */
	uint8* tsoHdrPtr;
	uint8 ac;
	uint8 corerev;

	/* Initialize */
	wlc = scb_ampdu->ampdu_tx->wlc;
	corerev = wlc->pub->corerev;
	scb = SCB_CFP_SCB(scb_cfp);
	hwagg = &(scb_ampdu->ampdu_tx->hwagg[fifo]);
	ac = WME_PRIO2AC(ini->tid);
	ASSERT(ac < AC_COUNT);

	/* Check for MAC REV */
	ASSERT(D11REV_GE(corerev, 40));
	ASSERT(D11REV_LT(corerev, 128));

	/* Pktinfo */
	pktinfo = &txh->rev40.PktInfo;
	/* Cache Info */

	/* dot11 hdr ptr */
	h = (struct dot11_header*)(((char*)txh) + D11AC_TXH_LEN);

	/* Fixup counter in TxFrameID */
	frameid = wlc_compute_frameid(wlc, pktinfo->TxFrameID, fifo);
	pktinfo->TxFrameID = htol16(frameid);

	/* Update TXD sequence number: copy from dot11 */
	pktinfo->Seq = h->seq;

	/* Update frame len (fc to fcs) */
#ifdef WLTOEHW
	tsoHdrSize = WLC_TSO_HDR_LEN(wlc, (d11ac_tso_t*)PKTDATA(wlc->osh, pkt));
	tsoHdrPtr = PKTDATA(wlc->osh, pkt);
#else
	tsoHdrSize = 0;
	tsoHdrPtr = NULL;
#endif // endif

	pktinfo->FrameLen = htol16((uint16)(pkttotlen(PKT_OSH_NA, pkt) +
		DOT11_FCS_LEN - tsoHdrSize - D11AC_TXH_LEN));

	/* Keymgmt updates on iv and txd */
	if (SCB_CFP_IV_LEN(scb_cfp)) {
		/*
		 * 'iv_body' was pointing to end of d11header
		 * decrement it by iv length to get to the start of iv
		 */
		iv_body = iv_body - SCB_CFP_IV_LEN(scb_cfp);

		/* key updates for AES
		 * 1. increment sequence number
		 * 2. Update pkt body[8 bytes iv]
		 * 3. update tx descriptot cache info
		 */
		key_aes_tx_mpdu_fast(SCB_CFP_KEY(scb_cfp), iv_body, txh);
	}
	/* Rate histogram */
	/** TODO */

	//ASSERT((pktflags & WLF_FIFOPKT) == 0);

	/* XXX optimize: remove these AMPDU specific fixups
	 * cache should have been setup with AMPDU settings
	 */

	/* clear svht bit, set ampdu bit */
	pktinfo->MacTxControlLow |= htol16(D11AC_TXC_AMPDU);

	/* for real ampdu, clear vht single mpdu ampdu bit */
	pktinfo->MacTxControlHigh &= htol16(~D11AC_TXC_SVHT);

	/* Update Epoch */
	change = FALSE;

	if (scb_ampdu->ampdu_tx->cfp_head_frame) {
		/* Evaluate epoch update changes for just the head frame */
		change = TRUE;

		/* Reset head frame flag */
		scb_ampdu->ampdu_tx->cfp_head_frame = FALSE;

		if (D11_TXFID_IS_RATE_PROBE(corerev, frameid)) {
			/* check vrate probe flag
			 * this flag is only get the frame to become first mpdu
			 * of ampdu and no need to save epoch_params
			 */
			wlc_scb_ratesel_probe_ready(wlc->wrsi, scb, frameid,
				FALSE, 0, ac);
			reason_code = AMU_EPOCH_CHG_FID;
		} else if ((scb != hwagg->prev_scb) | (ini->tid != hwagg->prev_tid)) {
			reason_code = AMU_EPOCH_CHG_DSTTID;
		} else if (SCB_CFP_CACHE_GEN(scb_cfp) != hwagg->prev_cache_gen) {
			/* rate/sgi/bw/stbc/antsel tx params */
			reason_code = AMU_EPOCH_CHG_PLCP;
		} else if (ltoh16(pktinfo->MacTxControlLow) & D11AC_TXC_UPD_CACHE) {
			reason_code = AMU_EPOCH_CHG_TXC_UPD;
#ifdef WLTOEHW
		} else if ((tsoHdrPtr[2] & TOE_F2_TXD_HEAD_SHORT) !=
			hwagg->prev_shdr) {
			reason_code = AMU_EPOCH_CHG_TXHDR;
#endif // endif

		} else {
			change = FALSE;
		}
	}

	/* Check prev_ft == AMPDU_NONE */
	if (!change && hwagg->prev_ft == AMPDU_NONE) {
		/* switching from non-aggregate to aggregate ampdu,
		 * just update the epoch changing parameters(hwagg) but
		 * not the epoch.
		 */
		change = TRUE;
		reason_code = AMU_EPOCH_NO_CHANGE;
	}

	if (change) {
		/* If there are cache changes, consider next frame as head again */
		if (ltoh16(pktinfo->MacTxControlLow) & D11AC_TXC_UPD_CACHE) {
			scb_ampdu->ampdu_tx->cfp_head_frame = TRUE;
		}
		wlc_cfp_ampdu_update_epoch(scb_ampdu->ampdu_tx, fifo, reason_code,
			scb, ini->tid, FALSE, SCB_CFP_CACHE_GEN(scb_cfp), tsoHdrPtr);
	}

	/* always set epoch for ampdu */
	wlc_txh_set_epoch(wlc, PKTDATA(PKT_OSH_NA, pkt), hwagg->epoch);

	/* Update MU counters */
	if (ltoh16(pktinfo->MacTxControlHigh) & D11AC_TXC_MU)
	        SCB_CFP_MUPKT_CNT(scb_cfp, ini->tid)++;
}

/**
 * 5.1 TXCache Fixups
 * Fixups dot11 header for current frame
 * 1. Fixup A3
 * 2. Fixup QoS
 * 3. Fixup Seq
 */
static INLINE void
wlc_cfp_ampdu_d11hdr_fixups(scb_cfp_t *scb_cfp, scb_ampdu_tid_ini_t *ini,
	void* pkt, d11txhdr_t *txh, char *da, char *sa)
{
	uint16 seq; /* dot11 seq */
	uint16 *qos; /* Pointer to qos */
	wlc_bsscfg_t *bsscfg; /* BSSCFG pointer */
	struct scb *scb; /* Sattion control block */
	struct dot11_header *h; /* dot11 header ptr */
	wlc_info_t *wlc;	/* wlc info */

	/* Initialize */
	bsscfg = SCB_CFP_CFG(scb_cfp);
	scb = SCB_CFP_SCB(scb_cfp);
	wlc = bsscfg->wlc;

	/* dot11 hdr ptr */
	h = (struct dot11_header*) (((char*)txh) + D11_TXH_LEN_EX(wlc));
	/* A3 Fixups */

	/* For reference, DS bits on Data frames and addresses:
	 *
	 *         ToDS FromDS   a1    a2    a3    a4
	 *
	 * IBSS      0      0    DA    SA   BSSID  --
	 * To STA    0      1    DA   BSSID  SA    --
	 * To AP     1      0   BSSID  SA    DA    --
	 * WDS       1      1    RA    TA    DA    SA
	 *
	 * For A-MSDU frame
	 *         ToDS FromDS   a1    a2    a3    a4
	 *
	 * IBSS      0      0    DA    SA   BSSID  --
	 * To STA    0      1    DA   BSSID BSSID  --
	 * To AP     1      0   BSSID  SA   BSSID  --
	 * WDS       1      1    RA    TA   BSSID  BSSID
	 */

	if (BSSCFG_STA(bsscfg)) {
		/* Normal STA to AP, fixup a3 with DA */
		if (da == NULL) {
			/* Passed DA is NULL for AMSDU case */
			eacopy(&scb->ea, &h->a3);
		} else {
			/* Non AMSDU case */
			eacopy((char*)da, (char*)&h->a3);
		}
	} else {
		/* normal AP to normal STA, fixup a3 with SA */
		if (sa == NULL) {
			/* Passed SA is NULL for AMSDU case */
			eacopy(&bsscfg->cur_etheraddr, &h->a3);
		} else {
			/* Non AMSDU case */
			eacopy((char*)sa, (char*)&h->a3);
		}
	}

	/* fixup qos control field to indicate it is an AMSDU frame */
	qos = (uint16 *)((uint8 *)h + DOT11_A3_HDR_LEN);

	/* set or clear the A-MSDU bit */
	if ((WLPKTTAG(pkt)->flags) & WLF_AMSDU) {
		*qos |= htol16(QOS_AMSDU_MASK);
	} else {
		*qos &= ~htol16(QOS_AMSDU_MASK);
	}

	 /* fix up tid */
	*qos &= ~htol16(QOS_TID_MASK);
	*qos |= htol16((uint16)PKTPRIO(pkt));

	/* Sequence number fixups  in dot11 header */
	/* XXX Assumptions:
	 * 1. Only AMPDu frames are expected
	 * 2. PROPTXSTATUS REUSE seqnumber not accounted for
	 */
	seq = (WLPKTTAG(pkt)->seq & WL_SEQ_NUM_MASK);

	/*  XXX CFP can not handle out of order packets
	 * Make sure CFP is turned off for these packets
	 */
	ASSERT(ini->tx_exp_seq == seq);
	ASSERT(ini->next_enq_seq == seq);

	/* Increment exp seq number */
	ini->tx_exp_seq = NEXT_SEQ(seq);
	ini->next_enq_seq  = NEXT_SEQ(seq);

	seq = seq << SEQNUM_SHIFT;

	/* Update dot11 seq number */
	h->seq = htol16(seq);

}
/**
 * CFP AMPDU PREPARE: Prepare MPDU for transmission to AQM
 * Slowpath equivalent: wlc_sendampdu_aqm
 */
static INLINE void
wlc_cfp_ampdu_prepare(scb_cfp_t *scb_cfp,
	scb_ampdu_tx_t *scb_ampdu, scb_ampdu_tid_ini_t *ini, void *pkt, uint8 fifo)
{
	d11txhdr_t	*txh;		/* TX header updated by cache */
	wlc_info_t	*wlc;		/* WLC INFO */
	struct scb	*scb;		/* Sattion control block */
	uint8		*iv_body;	/* start of iv section in dot11 header */
	uint		flags;		/* Flags to be copied from TX cache */
	struct ether_addr da, sa;	/* temporary storage for DA/SA */
	struct dot3_mac_llc_snap_header *dot3lsh;	/* dot3lsh header */
	bool		is_amsdu;	/* Send out as AMSDU */

	if (!PKTISCFP(pkt)) {
		WL_ERROR(("%s : Non CFP packet : %p ::: \n", __FUNCTION__, pkt));
		ASSERT(0);
	}

	/* All pkts should be AMPDU tagged */
	ASSERT(WLPKTTAG(pkt)->flags & WLF_AMPDU_MPDU);

	/* Initialize locals */
	/**
	 * Fetch key management handle saved in scb cfp cubby
	 * CFP is allowed only for open and AES modes with KEYS_IN_HW
	 * XXX Skip all algorithm based checks in fast path
	 */
	scb = SCB_CFP_SCB(scb_cfp);

	wlc = scb_ampdu->ampdu_tx->wlc;

	/* Packet shouldnt have been packet fetched from host */
	ASSERT(!PKTISPKTFETCHED(wlc->osh, pkt));
	is_amsdu = (WLPKTTAG(pkt)->flags & WLF_AMSDU);

#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
	/* It's time to swap to use D11_BUFFER to construct mpdu */
	if (PKTISTXFRAG(PKT_OSH_NA, pkt) && PKTSWAPD11BUF(PKT_OSH_NA, pkt) != BCME_OK) {
		/* Should not Fail */
		ASSERT(0);
	}
	/* Subframe header allready present in host */
	if (is_amsdu) {
		PKTPULL(PKT_OSH_NA, pkt, ETHER_HDR_LEN);
	}
#endif // endif

	/* dot3lsh is going to be overwritten, save DA and SA for later fixup
	 * For AMSDU cases, a3 can be copied from sc/bsscfg
	 * So skip this 12 bytes copy
	 */
	if (is_amsdu == 0) {
		dot3lsh = (struct dot3_mac_llc_snap_header *)PKTDATA(PKT_OSH_NA, pkt);
		eacopy((char*)(dot3lsh->ether_dhost), &da);
		eacopy((char*)(dot3lsh->ether_shost), &sa);

		/**
		 *  For non AMSDU frames, strip off ethernet header
		 *  Unlike trunk code[wlc_amsdu_pktc_agg], we didint attach an extra
		 *  ether header for AMSDU head frame
		 */
		PKTPULL(PKT_OSH_NA, pkt, ETHER_HDR_LEN);
	}

	/**
	 * Take a snap shot of PKTDATA here
	 * This will be end of d11 header after the cache
	 * is copied by wlc_txc_cp.
	 */
	iv_body = PKTDATA(PKT_OSH_NA, pkt);
	ASSERT(WLC_TXC_ENAB(wlc));

	/* Update TXC hit counters: Figure out right place in the function */
	WLCNTINCR(wlc->pub->_cnt->txchit);

	/* Copy the cache from the TXC */
	txh = wlc_txc_cp(wlc->txc, scb, pkt, &flags);

	/** 5.2 TXCache Fixups [dot11header] */
	if (is_amsdu == 0) {
		/* NON AMSDU: Pickup DA/SA for A3 fixup  from incoming pkt header */
		wlc_cfp_ampdu_d11hdr_fixups(scb_cfp, ini, pkt, txh, (char*)&da, (char*)&sa);
	} else {
		/* AMSDU: Pickup DA/SA for A3 fixup from bsscfg/scb */
		wlc_cfp_ampdu_d11hdr_fixups(scb_cfp, ini, pkt, txh, NULL, NULL);
	}

	/* TXD fixups */
	if (D11REV_GE((wlc)->pub->corerev, 128)) {
		wlc_cfp_ampdu_txd_ge128_fixup(scb_cfp, scb_ampdu, ini, pkt, fifo, txh, iv_body);
	} else {
		wlc_cfp_ampdu_txd_ge40_fixup(scb_cfp, scb_ampdu, ini, pkt, fifo, txh, iv_body);
	}

	/* update packet tag with the saved flags */
	WLPKTTAG(pkt)->flags |= flags;

#if defined(PROP_TXSTATUS)
#ifdef WLCNTSCB
	WLPKTTAG(pkt)->rspec = scb->scb_stats.tx_rate;
#endif // endif
#endif /* PROP_TXSTATUS */

}

/**
 * 4, CFP AMPDU RELEASE: Release packets from AMPDU Q with aggregation.
 * Slowpath equivalent: wlc_ampdu_release
 *
 * FIXME: Need to add WLATF_DONGLE functionality for "atfd" builds!
 */
static INLINE uint16
wlc_cfp_ampdu_release(scb_cfp_t *scb_cfp, scb_ampdu_tx_t *scb_ampdu,
	scb_ampdu_tid_ini_t *ini, uint16 release_cnt, bool amsdu_in_ampdu,
	taf_scheduler_public_t* taf)
{
	wlc_info_t *wlc;
	void *pkt, *sf0, *sf1;
	struct scb *scb;
	wlc_bsscfg_t *bsscfg; /* BSSCFG pointer */
	uint fifo;
	uint16 chunk_pkts, *scb_seqnum;
	bool is_amsdu, is_amsdu_sf0, continue_release, can_release_frames;
#ifdef WLATF
	uint32 chunk_bytes = 0U;
	const bool use_atf = !taf && AMPDU_ATF_ENABLED(ini);
	atf_state_t *atf_state = !taf ? AMPDU_ATF_STATE(ini) : NULL;
	uint32 max_rbytes, pktbytes;
	uint32 atf_rbytes_min;
	const atf_rbytes_t *atf_rbytes;
#ifdef RAVG_HISTORY
	pkt2oct_t *pkt2oct;
#endif // endif
#ifdef WLCNT
	uint32 npkts, uflow, oflow, minrel, reloverride;
#endif /* WLCNT */
#endif /* WLATF */
	wlc_txq_info_t *qi;
#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
	uint16 d11buf_avail;
#endif // endif
	wlc_pkttag_t *pkttag;
	struct spktq temp_q;	/* simple, non-priority pkt queue */
	uint32 mpdu_cnt;	/* no of mpdu released */
	uint32 mpdu_bytes;	/* byte count of no of mpdus released */
	uint8 tid;
	bool out_of_order = FALSE;
#ifdef PKTQ_LOG
	struct pktq *q;			/**< multi priority queue */
	pktq_counters_t* prec_cnt = 0;
	uint8 prec;
#endif // endif
	amsdu_info_t *ami;
	uint32 msdu_cnt = 0;
	uint pad, totlen = 0;
	uint amsdu_max_sframes;
	uint amsdu_max_agg_len;

	/* Initialize locals */
	wlc = scb_ampdu->ampdu_tx->wlc;
	ami = wlc->ami;
	bsscfg = SCB_CFP_CFG(scb_cfp);
	qi = wlc->active_queue;		/* Active Queue */
	sf0 = NULL;
	sf1 = NULL;
	chunk_pkts = 0;
	mpdu_cnt = 0;
	mpdu_bytes = 0;
	tid = ini->tid;
	ASSERT(tid < AMPDU_MAX_SCB_TID);

	fifo = prio2fifo[tid];

	ASSERT(wlc->txfifo_detach_pending == FALSE);
	ASSERT(!wlc->block_datafifo);
	ASSERT(!isset(wlc->hw->suspended_fifos, fifo));

	/* Init the temporary q */
	spktqinit(&temp_q, PKTQ_LEN_MAX);

	is_amsdu = FALSE;
	continue_release = TRUE;
	is_amsdu_sf0 = TRUE;
	can_release_frames = TRUE;
	scb = scb_ampdu->scb;
	scb_seqnum = &SCB_SEQNUM(scb, tid);
	amsdu_max_sframes = wlc_amsdu_scb_aggsf(ami, scb, tid);
	amsdu_max_agg_len = wlc_amsdu_scb_max_agg_len(ami, scb, tid);

	/* Re-assign fifo for MU AP */
	wlc_scb_determine_mu_txfifo(wlc, scb, &fifo);

	ASSERT(fifo < WLC_HW_NFIFO_INUSE(wlc));

#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
	d11buf_avail = lfbufpool_avail(D11_LFRAG_BUF_POOL);
	if (!d11buf_avail) {
#if defined(WLTAF) && defined(BCMDBG)
		if (taf) {
			taf->complete = TAF_REL_COMPLETE_NO_BUF;
		}
#endif /* WLTAF && BCMDBG */
		goto release_done;
	}
#endif /* BCM_DHDHDR && DONGLEBUILD */

#ifdef WLCNTSCB
	// ASSERT(RSPEC_ISVHT(scb->scb_stats.tx_rate));
#endif // endif

	/* Check if low TXQ is blocked */
	if (txq_stopped(qi->low_txq, fifo)) {
#if defined(WLTAF) && defined(BCMDBG)
		if (taf) {
			taf->complete = TAF_REL_COMPLETE_BLOCKED;
		}
#endif /* WLTAF && BCMDBG */
		goto release_done;
	}

#ifdef WLATF
	if (use_atf) {
#if defined(WLATF) && defined(WLATF_PERC)
				if (wlc->atm_perc) {
					uint32 now;

					wlc_read_tsf(wlc, &now, NULL);
					scb_ampdu->scb->last_active_usec = now;
					wlc_ampdu_update_atf_perc_max(wlc, now);
					if (wlc->perc_max != wlc->perc_max_last) {
						wlc->perc_max_last = wlc->perc_max;
						wlc_ampdu_atf_update_release_duration(wlc);
					}
				}
				atf_rbytes = wlc_ampdu_atf_rbytes(atf_state);
#else
				atf_rbytes = &atf_state->rbytes_target;
#endif /* WLATF && WLATF_PERC */

		ASSERT(atf_rbytes != NULL);
		max_rbytes = (atf_rbytes->max > atf_state->released_bytes_inflight) ?
			(atf_rbytes->max - atf_state->released_bytes_inflight) : 0;
		atf_rbytes_min = atf_rbytes->min;
#ifdef WLCNT
		pkt = NULL;
		npkts = uflow = oflow = minrel = reloverride = 0U;
#endif // endif
	}
#endif /* WLATF */
	/* Mark the start of the chain */
	scb_ampdu->ampdu_tx->cfp_head_frame =  TRUE;

	while (continue_release && (pkt = pktq_pdeq(&scb_ampdu->txq, tid))) {
#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
		ASSERT(lfbufpool_avail(D11_LFRAG_BUF_POOL));
#endif // endif
	        ASSERT(PKTPRIO(pkt) == tid);

		/* next_enq_seq doesn't match with scb seq implies out of order seq.
		 * Came across a case where tx_exp_seq is not incrmented in legacy
		 * path because gap between this and scb seq is more than SEQNUM_MAX/2.
		 * Such case is treated as loss and we free the packet in legacy path.
		 * Let's not take CFP path for such packets - Let legacy path free them
		 * after proper accounting.
		 */
		out_of_order = GET_DRV_HAS_ASSIGNED_SEQ(WLPKTTAG(pkt)->seq) |
			(ini->next_enq_seq != ((*scb_seqnum) & (SEQNUM_MAX - 1))) |
			(ini->tx_exp_seq != ((*scb_seqnum) & (SEQNUM_MAX - 1))) |
			PKTISPKTFETCHED(wlc->osh, pkt);

		/* Out of order handling through legacy path */
		if (out_of_order) {
			/* Out of orders packets should take the slow legacy
			 * path. So enqueueed it back to scb queue and call
			 * legacy txeval function after releaseing
			 * in order packets through CFP path
			 */
#if defined(DONGLEBUILD)
			ASSERT(PKTNEXT(PKT_OSH_NA, pkt) == NULL);
#endif // endif
			pktq_penq_head(&scb_ampdu->txq, tid, pkt);

			if (!is_amsdu_sf0) {
				/* Re enque partially formed ASMDU back into SCB Q */
				ASSERT(sf0);
				wlc_cfp_ampdu_reenque_pkts(wlc, scb_ampdu, tid, sf0);
			}

			/* Update cfp out of order count */
			SCB_CFP_SLOWPATH_CNT(scb_cfp, tid)++;
#if defined(WLTAF) && defined(BCMDBG)
			if (taf) {
				taf->complete = TAF_REL_COMPLETE_ERR;
			}
#endif /* WLTAF && BCMDBG */
			break;
		}

		/* Packets not handled by CFP release */
		ASSERT((WLPKTTAG(pkt)->flags &
			(WLF_CTLMGMT | WLF_TXHDR | WLF_AMPDU_MPDU |
			WLF_8021X | WLF_WAI | WLF_BYPASS_TXC)) == 0);

		chunk_pkts++;

		if (!PKTISCFP(pkt)) {
			PKTSETCFPFLOWID(pkt, SCB_CFP_FLOWID(scb_cfp));
		}

		WLC_CFP_DEBUG((WLC_CFPAMPDU_FMT "pkt %p scb_seqnum %d\n",
			WLC_CFPAMPDU_ARG("RELEASE"), pkt, *scb_seqnum));

		if (amsdu_in_ampdu) {
			void * peek_pkt = pktqprec_peek(&scb_ampdu->txq, tid); // Not dequeued
			is_amsdu = FALSE;

			/* Enter AMSDU agg logic if there is atleast 1 more pkt pending in SCb Q
			 * Or if its the last pkt in the Q , it better not be a head subframe.
			 */
			if ((peek_pkt != NULL) | (!is_amsdu_sf0)) {
				if (is_amsdu_sf0) {
#if !defined(DONGLEBUILD)
					/* In following cases, packet will be fragmented and
					 * linked with PKTNEXT
					 * - No enough headroom - TXOFF (wlc_hdr_proc())
					 * - Shared packets (wlc_hdr_proc()/ WMF)
					 *  Skip AMSDU agg for these packets.
					 */
					if ((PKTNEXT(PKT_OSH_NA, pkt) == NULL) &&
						(PKTNEXT(PKT_OSH_NA, peek_pkt) == NULL))
#endif /* ! DONGLEBUILD */
					{
						sf0 = sf1 = pkt;
						is_amsdu_sf0 = FALSE;
						msdu_cnt = 1;
						totlen = pkttotlen(PKT_OSH_NA, pkt);
						continue; /* dequeue next pkt for AMSDU agg */
					}
				} else {
					/* Pad first A-MSDU sub-frame to 4 Bytes total length */
					pad = (uint)((-(int)totlen) & 3);

					if (pad) {
#if defined(BCM_DHDHDR)
						/* pad data will be the garbage at the
						 * end of host data
						 */
						PKTSETFRAGLEN(PKT_OSH_NA, sf1, 1,
							PKTFRAGLEN(PKT_OSH_NA, sf1, 1) +
							pad);
						PKTSETFRAGTOTLEN(PKT_OSH_NA, sf1,
							PKTFRAGTOTLEN(PKT_OSH_NA, sf1) +
							pad);
#else /* ! BCM_DHDHDR */
						/* If a padding was required for the
						 * previous packet, apply it here
						 */
						PKTSETLEN(PKT_OSH_NA, sf1,
							pkttotlen(PKT_OSH_NA, sf1) + pad);
#endif /* !BCM_DHDHDR */
					}

					PKTSETNEXT(PKT_OSH_NA, pktlast(PKT_OSH_NA, sf1), pkt);
					totlen += pad + pkttotlen(PKT_OSH_NA, pkt);
					msdu_cnt++;
					sf1 = pkt;
					if ((msdu_cnt < amsdu_max_sframes) &&
						(peek_pkt != NULL) &&
						((totlen + pkttotlen(PKT_OSH_NA, peek_pkt)) <
						amsdu_max_agg_len) &&
#if !defined(DONGLEBUILD)
						(PKTNEXT(PKT_OSH_NA, peek_pkt) == NULL) &&
#endif /* ! DONGLEBUILD */
						TRUE) {
						continue;
					}
					pkt = sf0;
					is_amsdu = TRUE;
					/* msdu count per amsdu */
					wlc_cfp_amsdu_tx_counter_upd(wlc, msdu_cnt, totlen);
				}
			}

			is_amsdu_sf0 = TRUE; /* setup for next pairs of MSDU in AMSDU */

			/* Tag the pkt as AMSDU pkt */
			if (is_amsdu) {
				WLPKTTAG(pkt)->flags |= WLF_AMSDU;

#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
				/* Add subframe headers */
				wlc_amsdu_subframe_insert(wlc, pkt);
#endif /* BCM_DHDHDR && DONGLEBUILD */
			}
		}

		pkttag = WLPKTTAG(pkt);

	        pkttag->flags |= WLF_AMPDU_MPDU;

	        /* assign seqnum and save in pkttag */
		ASSERT(pkttag->seq == 0);
		pkttag->seq = ini->max_seq = ((*scb_seqnum) & (SEQNUM_MAX - 1));
	        (*scb_seqnum)++;

		SET_WL_HAS_ASSIGNED_SEQ(pkttag->seq);

	        ASSERT(!PKTISCHAINED(pkt));
#ifdef WLTAF
		if (taf && taf->how == TAF_RELEASE_LIKE_IAS) {
			uint32 taf_pkt_time_units;
			uint32 taf_pkt_tag;

			if (!taf->ias.is_ps_mode)  {

				pktbytes = is_amsdu ? totlen : pkttotlen(wlc->osh, pkt);

				taf_pkt_time_units = TAF_PKTBYTES_TO_UNITS((uint16)pktbytes,
					taf->ias.pkt_rate, taf->ias.byte_rate);

				if (taf_pkt_time_units == 0) {
					taf_pkt_time_units = 1;
				}

				taf->ias.actual.released_bytes += (uint16)pktbytes;

				taf_pkt_tag = TAF_UNITS_TO_PKTTAG(taf_pkt_time_units,
					&taf->ias.adjust);
				taf->ias.actual.released_units += TAF_PKTTAG_TO_UNITS(taf_pkt_tag);

				if ((taf->ias.actual.released_units +
					taf->ias.total.released_units) >=
					taf->ias.time_limit_units) {
#ifdef BCMDBG
					taf->complete = TAF_REL_COMPLETE_TIME_LIMIT;
#endif // endif
					/* set release_cnt to zero to finish */
					release_cnt = 0;
				}

				if ((taf->ias.released_units_limit > 0) &&
					(taf->ias.actual.released_units >=
					taf->ias.released_units_limit)) {
#ifdef BCMDBG
					taf->complete = TAF_REL_COMPLETE_REL_LIMIT;
#endif // endif
					/* set release_cnt to zero to finish */
					release_cnt = 0;
				}
			} else {
				taf_pkt_tag = TAF_PKTTAG_PS;
			}
			taf->ias.actual.release++;
			pkttag->pktinfo.taf.ias.index = taf->ias.index;
			pkttag->pktinfo.taf.ias.units = taf_pkt_tag;
			pkttag->pktinfo.taf.ias.tagged = TAF_TAGGED;
		}
		else if (taf && taf->how == TAF_RELEASE_LIKE_DEFAULT) {

			pkttag->pktinfo.taf.def.tag = SCB_PS(scb) ?
				TAF_PKTTAG_PS : TAF_PKTTAG_DEFAULT;
			pkttag->pktinfo.taf.def.tagged = 1;
			taf->def.actual.release++;

			/* used for PS mode handling only */
			WL_TAFF(wlc, MACF" tid %u TAF_RELEASE_LIKE_DEFAULT (count %u)\n",
				ETHER_TO_MACF(scb->ea), tid, taf->def.actual.release);
			ASSERT(SCB_PS(scb));
		}
#endif /* WLTAF */
#ifdef WLATF
		/* The downstream TXMOD may delete and free the packet, do the ATF
		 * accounting before the TXMOD to prevent the counter from going
		 * out of sync.
		 * Similarly the pkttag update, updating the pkttag after it has
		 * been freed is to be avoided
		 */
		if (use_atf) {
			pktbytes = PKTLEN(PKT_OSH_NA, pkt);
			if (PKTISFRAG(PKT_OSH_NA, pkt))
				pktbytes += PKTFRAGLEN(PKT_OSH_NA, pkt, 0);

			pkttag->pktinfo.atf.pkt_len = (uint16)pktbytes;
			atf_state->released_bytes_inflight += pktbytes;

			chunk_bytes += pktbytes;
			WL_AMPDU_TX(("Tot:%u Pkt:%u\n", chunk_bytes, pktbytes));
#ifdef RAVG_SIMPLE
			scb_ampdu->pkt_avg_octet[tid] += pktbytes;
			scb_ampdu->pkt_avg_octet[tid] >>= 1;
#endif /* RAVG_SIMPLE */
#ifdef RAVG_HISTORY
			pkt2oct = scb_ampdu->pkt2oct[tid];
			pkt2oct->pkt_avg_octet -= (pkt2oct->pktlen[pkt2oct->pkt_idx] >> 3);
			pkt2oct->pkt_avg_octet += (pktbytes >> 3);
			pkt2oct->pktlen[pkt2oct->pkt_idx] = pktbytes;
			pkt2oct->pkt_idx = (pkt2oct->pkt_idx + 1) & (RAVG_HISTORY_PKTNUM - 1);
#endif /* RAVG_HISTORY */
		}
#endif /* WLATF */

#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
		d11buf_avail--;
#if defined(WLTAF) && defined(BCMDBG)
		if (taf && d11buf_avail == 0 && taf->complete == TAF_REL_COMPLETE_NULL) {
			taf->complete = TAF_REL_COMPLETE_NO_BUF;
		}
#endif /* WLTAF && BCMDBG */
#endif /* BCM_DHDHDR && DONGLEBUILD */

		mpdu_cnt++;
		mpdu_bytes += pkttotlen(wlc->osh, pkt);

		/* In Release fn there should not be any more overrides.
		* Should always release pkts requested by the caller.
		* All resource availability should be checked by caller.
		*/
		can_release_frames = (mpdu_cnt < release_cnt);
#if defined(WLTAF) && defined(BCMDBG)
		if (taf && !can_release_frames && taf->complete == TAF_REL_COMPLETE_NULL) {
			taf->complete = TAF_REL_COMPLETE_FULFILLED;
		}
#endif /* WLTAF && BCMDBG */

#ifdef WLATF
		if (use_atf) { /* Apply ATF release overrides */
			bool can_release_bytes = (chunk_bytes <= max_rbytes);

			continue_release = (((chunk_bytes <= atf_rbytes_min) |
				(can_release_frames & can_release_bytes) |
				(can_release_bytes & (atf_state->atf == WLC_AIRTIME_PMODE))) &&
#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
				(d11buf_avail > 0) &&
#endif // endif
				TRUE);

#ifdef WLCNT
			/* Update stats */
			if ((!can_release_frames) && can_release_bytes) {
				if (chunk_bytes <= atf_rbytes_min) {
					WLCNTINCR(minrel);
				} else if (atf_state->atf == WLC_AIRTIME_PMODE) {
					WLCNTINCR(reloverride);
				}
			}
#endif  /* WLCNT */
		} else
#endif /* WLATF */
		{
			continue_release = can_release_frames &&
#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
				(d11buf_avail > 0) &&
#endif // endif
				TRUE;
		}

		/* enable packet callback for every MPDU in AMPDU */
		WLF2_PCB4_REG(pkt, WLF2_PCB4_AMPDU);

		ini->tx_in_transit++;
#ifdef WLATF
		scb_ampdu->ampdu_tx->tx_intransit++;
#endif /* WLATF */
		SCB_CFP_PKT_CNT(scb_cfp, tid)++;
		SCB_PKTS_INFLT_FIFOCNT_INCR(scb, tid);

		/* 5. Prepare an MPDU for transmission to AQM/MAC */
		wlc_cfp_ampdu_prepare(scb_cfp, scb_ampdu, ini, pkt, fifo);

		/* Add packets to temporary queue */
		spktenq(&temp_q, pkt);
	} /* while continue_release loop */

#if defined(WLTAF) && defined(BCMDBG)
	if (taf && pkt == NULL) {
		taf->complete = TAF_REL_COMPLETE_EMPTIED;
	}
#endif /* WLTAF && BCMDBG */

	/*
	 * ============== Done preparing chain of packets from scb ampdu Q====================
	 */

	WL_AMPDU_TX(("%s: fifo %d count %d epoch %d txpktpend %d",
		__FUNCTION__, fifo, mpdu_cnt, scb_ampdu->ampdu_tx->hwagg[fifo].epoch,
		TXPKTPENDGET(wlc, fifo)));

#ifdef PKTQ_LOG
	q = WLC_GET_CQ(qi);
	/* Update Common queue counters */
	if (q->pktqlog) {
		prec = WLC_PRIO_TO_PREC(tid);
		prec_cnt = q->pktqlog->_prec_cnt[prec];
		/* check for auto enabled logging */
		if (prec_cnt == NULL && (q->pktqlog->_prec_log & PKTQ_LOG_AUTO)) {
			prec_cnt = wlc_txq_prec_log_enable(wlc, q, (uint32)prec);
		}
		WLCNTCONDADD(prec_cnt, prec_cnt->requested, mpdu_cnt);
		WLCNTCONDADD(prec_cnt, prec_cnt->stored, mpdu_cnt);
	}
#endif /* PKTQ_LOG */

	/* Enqueue the temporary list of pkts to Low txq */
	if (mpdu_cnt) {
#ifdef WLATF
		if (use_atf) {
			atf_state->released_packets_inflight += mpdu_cnt;
		}
#endif /* WLATF */
		wlc_low_txq_enq_list(wlc->txqi,
			qi->low_txq, fifo, &temp_q, mpdu_cnt);
	}

	/* Update SCB stats counters */
	WLCNTSCBADD(scb->scb_stats.tx_pkts, mpdu_cnt);
	WLCNTSCBADD(scb->scb_stats.tx_ucast_bytes, mpdu_bytes);

	/* Update SCB AMPDU stats counters */
	AMPDUSCBCNTADD(scb_ampdu->cnt.txmpdu, mpdu_cnt);
	WLCNTSCBADD(scb_ampdu->ampdu_scb_stats->tx_pkts[tid], mpdu_cnt);
	WLCNTSCBADD(scb_ampdu->ampdu_scb_stats->tx_bytes[tid], mpdu_bytes);

	/* Update txpkt success counts */
	wlc_update_txpktsuccess_stats(wlc, scb, mpdu_bytes, tid, mpdu_cnt);

	/* ----------------------Rate historgram-----------------------
	 * Fill up the cirular buffer used for rate histogram reporting
	 * 2 dimensional array with column-0 corresponding to rspec
	 * column-1 corresponding to nfrags.
	 * In fast path rate doesn't change, so fill with previous entry
	 */

	/* In case of RATELINKMEM, don't update rspec history, instead do so on TXS */
	if (!RATELINKMEM_ENAB(wlc->pub)) {
		bsscfg->txrspec[bsscfg->txrspecidx][0] =
			bsscfg->txrspec[(bsscfg->txrspecidx - 1) % NTXRATE][0]; /* rspec */
		bsscfg->txrspec[bsscfg->txrspecidx][1] = mpdu_cnt; /* nfrags */
		bsscfg->txrspecidx = (bsscfg->txrspecidx + 1) % NTXRATE;
	}

#ifdef WLATF
	if (use_atf) {
		const uint32 released_bytes_inflight =
			atf_state->released_bytes_inflight;
#ifdef WLCNT
		atf_stats_t *atf_stats;
		atf_stats = AMPDU_ATF_STATS(ini);

		WLCNTADD(atf_stats->npkts, npkts);
		WLCNTADD(atf_stats->uflow, uflow);
		WLCNTADD(atf_stats->oflow, oflow);
		WLCNTADD(atf_stats->uflow, minrel);
		WLCNTADD(atf_stats->oflow, reloverride);
		WLCNTINCR(atf_stats->ndequeue);

		if (chunk_pkts == 1) {
		    WLCNTINCR(atf_stats->singleton);
		}

		if (pkt != NULL) {
			if (can_release_frames)
				WLCNTINCR(atf_stats->timelimited);

			if (atf_rbytes->max > released_bytes_inflight)
				WLCNTINCR(atf_stats->framelimited);
		}

		AMPDU_ATF_INCRSTAT(&atf_stats->rbytes, max_rbytes);
		AMPDU_ATF_INCRSTAT(&atf_stats->chunk_bytes, chunk_bytes);
		AMPDU_ATF_INCRSTAT(&atf_stats->chunk_pkts, chunk_pkts);
		AMPDU_ATF_INCRSTAT(&atf_stats->release, release_cnt);
		AMPDU_ATF_INCRSTAT(&atf_stats->inflight, released_bytes_inflight);
#endif /* WLCNT */

		WL_AMPDU_TX(("%s release left:%d Bytes:%d Inflight:%u\n", __FUNCTION__,
			release_cnt, chunk_bytes, released_bytes_inflight));
	}
#endif /* WLATF */

	/* Post packets to fifo */
	txq_hw_fill_active(wlc->txqi, qi->low_txq);

	/* Deinit the temporary queue */
	spktqdeinit(&temp_q);

	/* Call legacy path in case there is any out of order sequence */
	if (out_of_order) {
		/* Temporary pause CFP. Otherwise wlc_ampdu_txeval will
		 * call wlc_cfp_ampdu_scedule
		 */
#ifdef WLTAF
		if (taf) {
			/* for TAF we got into this function via scheduler path; so return back
			 * and inform TAF that it needs to follow up with slow_path
			 * variant (TAF_RELEASE_MODE_REAL)
			 */
			taf->mode = TAF_RELEASE_MODE_REAL;
		} else
#endif /* WLTAF */
		{
			/* XXX "WLC_AMPDU_AGG_COMPLETE" MUST NOT be done for TAF because this
			 * invokes scheduler again and we are here through scheduler, so it would
			 * all go a bit loopy
			 */
			WLC_AMPDU_AGG_COMPLETE(wlc, scb_ampdu->ampdu_tx, scb, scb_ampdu, ini, TRUE);
		}

	}
release_done:
	return mpdu_cnt;
}

/**
 * 3. CFP AMPDU EVALUATE: Evaluate number of packets that may be released.
 * Slowpath equivalent: wlc_ampdu_txeval
 */
static INLINE void
wlc_cfp_ampdu_evaluate(scb_cfp_t *scb_cfp, scb_ampdu_tx_t *scb_ampdu,
	scb_ampdu_tid_ini_t *ini, const bool force_release)
{
	wlc_info_t *wlc;
	ampdu_tx_info_t *ampdu_tx;
	struct scb * scb;
	/* Wlc Core Tx FIFO state */
	uint fifo; /* FIFO # */
	uint16 ampdu_pktq_len, release_cnt, release_lowat, in_transit;
	uint16 low_txq_space;
	wlc_txq_info_t *qi;

	/* Initialize */

	ASSERT(scb_ampdu);
	ASSERT(ini != NULL);
	ASSERT(ini->ba_state == AMPDU_TID_STATE_BA_ON); /* CFP capable */
	ASSERT(scb_cfp->cq_cnt[ini->tid] == 0);	/* TXQ is empty */

	ampdu_tx = scb_ampdu->ampdu_tx;
	wlc = ampdu_tx->wlc;
	scb = scb_ampdu->scb;
	qi = wlc->active_queue;		/* Active Queue */
	fifo = prio2fifo[ini->tid];

	/* Re-assign fifo for MU AP */
	wlc_scb_determine_mu_txfifo(wlc, scb, &fifo);

	ASSERT(!wlc->block_datafifo);
	ASSERT(!wlc->txfifo_detach_pending);

	ampdu_pktq_len = pktqprec_n_pkts(&scb_ampdu->txq, ini->tid);

	if (ampdu_pktq_len == 0)
		return;

	/* In CFP mode AMPDU Qs carry packet count instead of MPDU count */
	ampdu_pktq_len = WLC_AMPDU_PKTS_TO_MPDUS(wlc, scb, ini->tid, ampdu_pktq_len);

	/* Apply window and release max counts */
	release_cnt = wlc_ampdu_release_count(ini, ampdu_pktq_len, scb_ampdu);

	/* Available space in Low txq */
	low_txq_space = txq_space(qi->low_txq, fifo);

	in_transit = ini->tx_in_transit;

	/*
	 * For AQM, limit the number of packets in transit simultaneously so other
	 * stations get a chance to transmit as well.
	 */
	if (IS_SEQ_ADVANCED(ini->max_seq, ini->start_seq)) {
		in_transit = MODSUB_POW2(ini->max_seq, ini->start_seq, SEQNUM_MAX);
#ifdef PROP_TXSTATUS
		if (in_transit >= ini->suppr_window) {
			/* suppr_window : suppr packet count inside ba window */
			in_transit -= ini->suppr_window;
		} else {
			in_transit = 0;
		}
#endif /* PROP_TXSTATUS */

	} else {
		in_transit = 0;
	}

	ASSERT(in_transit < SEQNUM_MAX/2);

	WLC_CFP_DEBUG((WLC_CFPAMPDU_FMT "fifo %d release %d in_transit %d"
		" txpktpend %d \n", WLC_CFPAMPDU_ARG("INTRANSIT"), fifo, release_cnt,
		in_transit, TXPKTPENDGET(wlc, fifo)));

	release_lowat = ampdu_tx->config->tx_rel_lowat;

	/*
	 * To prevent starvation of other tx traffic from this transmitter,
	 * domination of the 'common' queue with packets from one TID should be
	 * avoided. Unless forced, ensure that we do not have too many packets
	 * in transit for this priority.
	 *
	 * XXX Also, leave at least AMPDU_AQM_RELEASE_DEFAULT(256) or 50% room on
	 * the common Q.
	 */
	if (!force_release && (
#ifdef WLATF
		(wlc_ampdu_atf_holdrelease(AMPDU_ATF_STATE(ini))) ||
#endif // endif
		(in_transit > ampdu_tx->aqm_max_release[ini->tid]))) {

		WL_AMPDU_TX(("wl%d: Stop Releasing %d in_transit %d tid %d "
			"wm %d rem_wdw %d qlen %d force %d\n",
			wlc->pub->unit, release_cnt, in_transit, ini->tid, release_lowat,
			wlc_ampdu_get_rem_window(ini), ampdu_pktq_len, force_release));

		release_cnt = 0;
	}

	/* Enqueue as much as we can if there is no space for all */
	if (low_txq_space < release_cnt) {
		release_cnt = low_txq_space;
		if (!low_txq_space)
			AMPDUSCBCNTINCR(scb_ampdu->cnt.txnoroom);
	}

	if (release_cnt == 0)
		return;

	/* release mpdus if any one of the following conditions are met */
	if ((in_transit < release_lowat) ||
#ifdef WLATF
		wlc_ampdu_atf_lowat_release(ampdu_tx, ini) ||
#endif // endif
		(release_cnt == MIN(scb_ampdu->release, ini->ba_wsize)) ||
		(force_release))
	{
		AMPDU_ATF_INCRSTAT(&AMPDU_ATF_STATS(ini)->inputq, ampdu_pktq_len);

		if (in_transit < release_lowat) {
			/* mpdus released due to lookahead watermark */
			WLCNTADD(ampdu_tx->cnt->txrel_wm, release_cnt);
		}

		if (release_cnt == scb_ampdu->release)
			WLCNTADD(ampdu_tx->cnt->txrel_size, release_cnt);

		WL_AMPDU_TX(("wl%d: Releasing %d mpdus in_transit %d tid %d wm %d "
			"rem_wdw %d qlen %d force %d start_seq %x, max_seq %x\n",
			wlc->pub->unit, release_cnt, in_transit,
			ini->tid, release_lowat, wlc_ampdu_get_rem_window(ini),
			ampdu_pktq_len, force_release, ini->start_seq, ini->max_seq));

		WLC_CFP_DEBUG((WLC_CFPAMPDU_FMT "fifo %d release %d \n",
			WLC_CFPAMPDU_ARG("RELEASE"), fifo, release_cnt));

		/* 4. Release packets from AMPDU Q with aggregation. */
		wlc_cfp_ampdu_release(scb_cfp, scb_ampdu, ini,
			release_cnt,
			WLC_AMSDU_IN_AMPDU(wlc, scb, ini->tid), NULL);
		return;
	}

	if ((ini->tx_in_transit == 0) && (in_transit != 0)) {
		WL_AMPDU_ERR(("wl%d.%d Cannot release: in_transit %d tid %d "
			"start_seq 0x%x max_seq 0x%x\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(SCB_BSSCFG(scb_ampdu->scb)),
			in_transit, ini->tid, ini->start_seq, ini->max_seq));
	}

	WL_AMPDU_TX(("wl%d: Cannot release: in_transit %d tid %d wm %d "
		"rem_wdw %d qlen %d release %d\n",
		wlc->pub->unit, in_transit, ini->tid, release_lowat,
		wlc_ampdu_get_rem_window(ini), ampdu_pktq_len, release_cnt));
	return;
}

/**
 * 2. CFP AMPDU SCHEDULE: Schedule the release of packets for aggregation.
 * Slowpath equivalent: wlc_ampdu_agg_complete
 */
static INLINE void
wlc_cfp_ampdu_schedule(scb_cfp_t *scb_cfp, scb_ampdu_tx_t *scb_ampdu,
	scb_ampdu_tid_ini_t *ini, const bool force_release)
{
	wlc_info_t *wlc;
	ampdu_tx_info_t *ampdu_tx;
	uint16 in_transit, max_release;

#ifdef WLSQS
	/* CFP schedule function not to be used with SQS */
	ASSERT(0);
#endif // endif
	ampdu_tx = scb_ampdu->ampdu_tx;
	wlc = ampdu_tx->wlc;

	ASSERT(SCB_AMPDU(scb));
	ASSERT(ini);
	ASSERT(SCB_CFP_AMPDU_TX(scb_cfp));
	ASSERT(SCB_CFP_AMPDU_TX(scb_cfp) == scb_ampdu);
	ASSERT(!SCB_PS(scb));

	/* Avoid out of order submission to MAC fifo.
	 * Make sure txq is drained out of all given scb packets before starting CFP release
	 */
	if (scb_cfp->cq_cnt[ini->tid]) {
		WL_INFORM(("TX pkts pending in TXQ : scb %p prio %d cnt %d  \n",
			scb_cfp->scb, ini->tid, scb_cfp->cq_cnt[ini->tid]));
		/* Drain the TXQ  packets now */
		wlc_send_active_q(wlc);
		SCB_CFP_TXQ_DRAIN_WAIT(scb_cfp, ini->tid)++;

		return;
	}

	in_transit = ini->tx_in_transit;
	max_release = scb_ampdu->ampdu_tx->aqm_max_release[ini->tid];

	WLC_CFP_DEBUG((WLC_CFPAMPDU_FMT "in_transit %u max_release %u\n",
		WLC_CFPAMPDU_ARG("SCHEDULE"), in_transit, max_release));

	if (in_transit > max_release) {
		return; /* Too many in transit, do not release anymore */
	}

	wlc_cfp_ampdu_txflowcontrol(scb_cfp, scb_ampdu, ini);

	/* 3. Evaluate number of packets that may be released. */
	(void) wlc_cfp_ampdu_evaluate(scb_cfp, scb_ampdu, ini, force_release);
}

#if defined(WLAMPDU_PRECEDENCE)
#error "wlc_cfp_ampdu_enqueue: Add CFP Support for WLAMPDU_PRECEDENCE"
#endif // endif

/**
 * 1. CFP AMPDU ENQUEUE: Append a packet list to AMPDU precedence queue.
 * Slowpath equivalent: wlc_ampdu_prec_enq
 *
 * @param pq   Multi-priority packet queue
 */
static INLINE int
wlc_cfp_ampdu_enqueue(struct pktq *pq, uint8 prec,
	void *pktlist_head, void *pktlist_tail, uint16 pktlist_count)
{
	struct pktq_prec *q;			/**< single precedence packet queue */
#ifdef PKTQ_LOG
	pktq_counters_t* prec_cnt = 0;

	if (pq->pktqlog) {
		prec_cnt = pq->pktqlog->_prec_cnt[prec];
	}
	WLCNTCONDADD(prec_cnt, prec_cnt->requested, pktlist_count);
#endif // endif
	ASSERT(prec < pq->num_prec);
	ASSERT(pktlist_head != NULL);
	ASSERT(PKTLINK(pktlist_tail) == NULL);

	/* do not check for pktq prec full in Full dongle */

	q = &pq->q[prec];
#if !defined(DONGLEBUILD)
	/* Check for pktq prec avail */
	/* Should we append avail len?? */
	if (pktqprec_avail_pkts(pq, prec) < pktlist_count) {
#ifdef PKTQ_LOG
		WLCNTCONDADD(prec_cnt, prec_cnt->dropped, pktlist_count);
#endif // endif
		return BCME_NORESOURCE;
	}
#endif /* DONGLEBUILD */
	if (q->head)
		PKTSETLINK(q->tail, pktlist_head);
	else
		q->head = pktlist_head;

	q->tail = pktlist_tail;
	q->n_pkts += pktlist_count;
	pq->n_pkts_tot += pktlist_count;

#ifdef PKTQ_LOG
	WLCNTCONDADD(prec_cnt, prec_cnt->stored, pktlist_count);
#endif // endif

	if (pq->hi_prec < prec)
		pq->hi_prec = prec;

	/* no need to zero out pktlist_xyz */

	return (q->n_pkts);
}

/**
 * 0. CFP AMPDU ENTRY: CFP Transmit fastpath main entry point into AMPDU module.
 * Slowpath equivalent: wlc_ampdu_agg_pktc/wlc_ampdu_agg
 */
void
wlc_cfp_ampdu_entry(wlc_info_t *wlc, uint8 prio,
	void *pktlist_head, void *pktlist_tail, uint16 pktlist_count,
	scb_cfp_t *scb_cfp)
{
	const uint8 tid = prio; /* same as pkt prio */
	const bool force_release = FALSE;
	scb_ampdu_tx_t *scb_ampdu;
	scb_ampdu_tid_ini_t *ini;
#ifdef PKTQ_LOG
	struct pktq *pq;		/**< multi priority queue */
#endif // endif
	int enqueue_status;

#ifdef WLSQS
	ASSERT(0);
#endif // endif
	/* Fetch SCB AMPDU cubby and INI saved in TCB */
	scb_ampdu = SCB_CFP_AMPDU_TX(scb_cfp);
	ASSERT(scb_ampdu);

	ini = scb_ampdu->ini[tid];
	ASSERT(ini != NULL);
	ASSERT(ini->ba_state == AMPDU_TID_STATE_BA_ON);

	WLC_CFP_DEBUG((WLC_CFPAMPDU_FMT "pkts q%u + %u ampdu_tx %p wl%u\n",
		WLC_CFPAMPDU_ARG("ENQUEUE"), pktqprec_n_pkts(&scb_ampdu->txq, tid),
		pktlist_count, scb_ampdu->ampdu_tx,
		scb_ampdu->ampdu_tx->wlc->pub->unit));

	/* 1. ENQUEUE: Append a packet list to AMPDU precedence queue. */
	enqueue_status = wlc_cfp_ampdu_enqueue(&scb_ampdu->txq, tid,
		pktlist_head, pktlist_tail, pktlist_count);
	BCM_REFERENCE(enqueue_status);
#ifdef PKTQ_LOG
	/* pktq_stats: ampdu q accounting
	 * FIXME:
	 * 1. requested counter will be doubled compared to
	 * non cfp path since we are counting on non amsdu frames
	 * 2. No overflow handling in CFP path so far. So
	 * skipping drop update
	 */
	pq = &scb_ampdu->txq;
	if (pq->pktqlog) {
		pktq_counters_t* prec_cnt = pq->pktqlog->_prec_cnt[tid];

		/* check for auto enabled logging */
		if (prec_cnt == NULL && (pq->pktqlog->_prec_log & PKTQ_LOG_AUTO)) {
			prec_cnt = wlc_txq_prec_log_enable(wlc, pq, (uint32)tid);
		}

		WLCNTCONDADD(prec_cnt, prec_cnt->requested, pktlist_count);
		if (enqueue_status) {
			WLCNTCONDADD(prec_cnt, prec_cnt->stored, pktlist_count);
		} else {
			WLCNTCONDADD(prec_cnt, prec_cnt->dropped, pktlist_count);
		}
	}
#endif /* PKTQ_LOG */

#if !defined(DONGLEBUILD)
	/* Free CFP pktlist if AMPDU precedence queue is full */
	if (enqueue_status < 0) {
		wlc_cfp_pktlist_free(wlc, pktlist_head);
	}
#endif /* DONGLEBUILD */

	/* 2. SCHEDULE: Schedule the release of packets for aggregation. */
	wlc_cfp_ampdu_schedule(scb_cfp, scb_ampdu, ini, force_release);
}
/* Re-enqueue  packets to SCB ampdu Q on detecting CFP bypass path failure
 * @head : Head packet from the temporary list of packets
 * @tid  : Current priority
 */
void
wlc_cfp_ampdu_reenque_pkts(wlc_info_t *wlc, scb_ampdu_tx_t *scb_ampdu, uint8 tid, void* head)
{
	void *p = NULL;
	ASSERT(head);
	ASSERT(scb_ampdu);
	ASSERT(&scb_ampdu->txq);

	do {
		/* Packets are removed from the tail of temporary packet chain */
		p = pktchain_deq_tail(wlc->osh, head);
		ASSERT(p);

		/* Enque back to ampdu queue */
		pktq_penq_head(&scb_ampdu->txq, tid, p);
	} while (p != head);
}
#endif /* WLCFP */

#if defined(BCMDBG)
static int
wlc_dyn_fb_dump(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	ampdu_tx_info_t *ampdu_tx = wlc->ampdu_tx;
	ampdu_tx_config_t *ampdu_tx_cfg;
	bool dynfb = 0;
	if (ampdu_tx) {
		ampdu_tx_cfg = ampdu_tx->config;
		if (wlc->hti->frameburst && ampdu_tx_cfg->fb_override_enable) {
			dynfb = 1;
		}
#ifdef WLMCHAN
		if (MCHAN_ACTIVE(wlc->pub)) {
			bcm_bprintf(b, "Dynamic framebursting is unsupported for MCHAN\n");
			return 0;
		}
#endif // endif
		bcm_bprintf(b, "dynfb configured %d, rtsper %d%%, fb state %s\n",
				dynfb,
				ampdu_tx->rtsper_avg,
				(dynfb && !ampdu_tx_cfg->fb_override) ? "ON":"OFF");
	}
	return 0;
}
#endif /* BCMDBG */

#ifdef WLSQS
bool
wlc_sqs_ampdu_capable(wlc_info_t *wlc, scb_cfp_t *scb_cfp, uint8 prio)
{
	struct scb *scb;
	scb_ampdu_tx_t *scb_ampdu;
	ampdu_tx_info_t *ampdu_tx = wlc->ampdu_tx;
	ampdu_tx_config_t *ampdu_tx_cfg;

	ASSERT(ampdu_tx);
	ampdu_tx_cfg = ampdu_tx->config;
	ASSERT(ampdu_tx_cfg);

	scb = SCB_CFP_SCB(scb_cfp);
	if (!SCB_AMPDU(scb)) {
		return FALSE;
	}
	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	if (scb_ampdu == NULL) {
		return FALSE;
	}

	/* No established ini and creating an ini for this tid is disallowed */
	if (!scb_ampdu->ini[prio] && !ampdu_tx_cfg->ini_enable[prio]) {
		return FALSE;
	}

	/* In DWDS mode, dhd will create two flowring with the same cfp_flowid
	 * because they link the same SCB.
	 * This will cause vpkts accounting issue for SQS, so disable SQS for DWDS.
	 */
	if (SCB_DWDS(scb)) {
		return FALSE;
	}

	return TRUE;
}

uint16
wlc_sqs_ampdu_vpkts(wlc_info_t *wlc, scb_cfp_t *scb_cfp, uint8 prio)
{
	struct scb *scb;
	scb_ampdu_tx_t *scb_ampdu;

	scb = SCB_CFP_SCB(scb_cfp);
	if (SCB_INTERNAL(scb)) {
		return 0;
	}
	scb_ampdu = SCB_AMPDU_TX_CUBBY(wlc->ampdu_tx, scb);
	ASSERT(scb_ampdu);

	return (scb_ampdu->txq.q[prio].v_pkts);
}

uint16
wlc_sqs_ampdu_v2r_pkts(wlc_info_t *wlc, scb_cfp_t *scb_cfp, uint8 prio)
{
	struct scb *scb;
	scb_ampdu_tx_t *scb_ampdu;

	scb = SCB_CFP_SCB(scb_cfp);
	if (SCB_INTERNAL(scb)) {
		return 0;
	}
	scb_ampdu = SCB_AMPDU_TX_CUBBY(wlc->ampdu_tx, scb);
	ASSERT(scb_ampdu);

	return (scb_ampdu->txq.q[prio].v2r_pkts);
}
/* Return ampdu cubby intransit packets */
uint16
wlc_sqs_ampdu_in_transit_pkts(wlc_info_t *wlc, scb_cfp_t *scb_cfp, uint8 prio)
{
	struct scb *scb;
	scb_ampdu_tx_t *scb_ampdu;
	scb_ampdu_tid_ini_t *ini = NULL;

	scb = SCB_CFP_SCB(scb_cfp);
	if (SCB_INTERNAL(scb)) {
		return 0;
	}
	scb_ampdu = SCB_AMPDU_TX_CUBBY(wlc->ampdu_tx, scb);
	ASSERT(scb_ampdu);
	ini = scb_ampdu->ini[prio];

	if (ini == NULL)
		return 0;

	return (ini->tx_in_transit);
}
/* Return the real packets in scb ampdu Q */
uint16
wlc_sqs_ampdu_n_pkts(wlc_info_t *wlc, scb_cfp_t *scb_cfp, uint8 prio)
{
	struct scb *scb;
	scb_ampdu_tx_t *scb_ampdu;

	scb = SCB_CFP_SCB(scb_cfp);
	if (SCB_INTERNAL(scb)) {
		return 0;
	}
	scb_ampdu = SCB_AMPDU_TX_CUBBY(wlc->ampdu_tx, scb);
	ASSERT(scb_ampdu);

	return (scb_ampdu->txq.q[prio].n_pkts);
}
/* Return the real packets in scb ampdu Q */
uint16
wlc_sqs_ampdu_tbr_pkts(wlc_info_t *wlc, scb_cfp_t *scb_cfp, uint8 prio)
{
	struct scb *scb;
	scb_ampdu_tx_t *scb_ampdu;

	scb = SCB_CFP_SCB(scb_cfp);
	if (SCB_INTERNAL(scb)) {
		return 0;
	}
	scb_ampdu = SCB_AMPDU_TX_CUBBY(wlc->ampdu_tx, scb);
	ASSERT(scb_ampdu);

	return (scb_ampdu->txq.q[prio].tbr_pkts);
}
void
wlc_sqs_pktq_v2r_revert(wlc_info_t *wlc, scb_cfp_t *scb_cfp, uint8 prio, uint16 v2r_reverts)
{
	struct pktq *pktq;		/**< multi priority packet queue */
	struct pktq_prec *pktqp;	/**< single precedence packet queue */
	struct scb *scb;	/* Station control block */
	scb_ampdu_tx_t *scb_ampdu;

	scb = SCB_CFP_SCB(scb_cfp);
	if (SCB_INTERNAL(scb)) {
		return;
	}
	scb_ampdu = SCB_AMPDU_TX_CUBBY(wlc->ampdu_tx, scb);
	ASSERT(scb_ampdu);

	pktq = &scb_ampdu->txq;
	pktqp = &pktq->q[prio];
	ASSERT(pktqp->v2r_pkts >= v2r_reverts);
	pktq->v_pkts_tot += v2r_reverts;
	pktqp->v_pkts    += v2r_reverts;
	pktqp->v2r_pkts  -= v2r_reverts;
}

void
wlc_sqs_pktq_vpkts_enqueue(wlc_info_t *wlc, scb_cfp_t *scb_cfp, uint8 prio, uint16 v_pkts)
{
	struct scb *scb;	/* Station control block */
	scb_ampdu_tx_t *scb_ampdu;
	struct pktq *pktq = NULL;

	scb = SCB_CFP_SCB(scb_cfp);
	if (SCB_INTERNAL(scb)) {
		return;
	}
	scb_ampdu = SCB_AMPDU_TX_CUBBY(wlc->ampdu_tx, scb);
	ASSERT(scb_ampdu);

	/* AMPDU precendece Queue */
	pktq = &scb_ampdu->txq;
	ASSERT(pktq);

	pktq->v_pkts_tot += v_pkts;
	pktq->q[prio].v_pkts += v_pkts;
}

void
wlc_sqs_pktq_vpkts_rewind(wlc_info_t *wlc, scb_cfp_t *scb_cfp, uint8 prio, uint16 count)
{
	struct pktq *pktq;	/**< multi priority packet queue */
	struct pktq_prec *pktqp;	/**< single precedence packet queue */
	struct scb *scb;	/* Station control block */
	scb_ampdu_tx_t *scb_ampdu;

	scb = SCB_CFP_SCB(scb_cfp);
	if (SCB_INTERNAL(scb)) {
		return;
	}
	scb_ampdu = SCB_AMPDU_TX_CUBBY(wlc->ampdu_tx, scb);
	ASSERT(scb_ampdu);

	pktq = &scb_ampdu->txq;
	pktqp = &pktq->q[prio];
	pktq->v_pkts_tot += count;
	pktqp->v_pkts    += count;
}

void
wlc_sqs_pktq_vpkts_forward(wlc_info_t *wlc, scb_cfp_t *scb_cfp, uint8 prio, uint16 count)
{
	struct pktq *pktq;			/**< multi-priority packet queue */
	struct pktq_prec *pktqp;		/**< single precedence packet queue */
	struct scb *scb;	/* Station control block */
	scb_ampdu_tx_t *scb_ampdu;

	scb = SCB_CFP_SCB(scb_cfp);
	if (SCB_INTERNAL(scb)) {
		return;
	}
	scb_ampdu = SCB_AMPDU_TX_CUBBY(wlc->ampdu_tx, scb);
	ASSERT(scb_ampdu);

	pktq = &scb_ampdu->txq;
	pktqp = &pktq->q[prio];

	ASSERT(pktqp->v_pkts >= count);
	ASSERT(pktq->v_pkts_tot >= count);

	pktq->v_pkts_tot -= count;
	pktqp->v_pkts    -= count;
}
#ifdef WLATF
/* Returns max no of packets allowed by ATF for a given ampdu session
 * This Function assumes 2 msdu in amsdu when amsdu_in_ampdu is TRUE
 */
static uint32
wlc_sqs_get_atf_max_limit(scb_ampdu_tx_t *scb_ampdu, uint8 prio, uint8 aggsf)
{
	uint32 max_rbytes, atf_rbytes_min;
	uint32 max_rel_cnt;
	uint32 min_rel_cnt;
	scb_ampdu_tid_ini_t *ini = NULL;
	atf_rbytes_t *atf_rbytes;
	atf_state_t *atf_state;
	uint32 pkt_avg_octet;
	ampdu_tx_info_t *ampdu_tx = scb_ampdu->ampdu_tx;
	wlc_info_t *wlc = ampdu_tx->wlc;

	ini = scb_ampdu->ini[prio];

	if (!ini) {
		/* NOP: Release Max */
	       return AMPDU_MAX_MPDU;
	}
	atf_state = AMPDU_ATF_STATE(ini);

	if (!atf_state) {
		/* NOP: Release Max */
	       return AMPDU_MAX_MPDU;
	}

#if defined(WLATF) && defined(WLATF_PERC)
			if (wlc->atm_perc) {
				uint32 now;

				wlc_read_tsf(wlc, &now, NULL);
				scb_ampdu->scb->last_active_usec = now;
				wlc_ampdu_update_atf_perc_max(wlc, now);
				if (wlc->perc_max != wlc->perc_max_last) {
					wlc->perc_max_last = wlc->perc_max;
					wlc_ampdu_atf_update_release_duration(wlc);
				}
			}
#endif /* WLATF && WLATF_PERC */

	atf_rbytes = wlc_ampdu_atf_rbytes(atf_state);

	ASSERT(atf_rbytes != NULL);
	ASSERT(scb_ampdu->ampdu_tx->ravg_algo);

#ifdef RAVG_SIMPLE
	pkt_avg_octet = scb_ampdu->pkt_avg_octet[ini->tid];
#endif // endif
#ifdef RAVG_HISTORY
	pkt_avg_octet = scb_ampdu->pkt2oct[ini->tid]->pkt_avg_octet;
#endif // endif
	ASSERT(pkt_avg_octet);

	max_rbytes = (atf_rbytes->max > atf_state->released_bytes_inflight) ?
			(atf_rbytes->max - atf_state->released_bytes_inflight) : 0;

	atf_rbytes_min = atf_rbytes->min;

	/* We should always release one amsdu worth data
	 * (min_rel_cnt) irrespective of ATF limit. the
	 * 'do while' loop in ampdu_relase function ensures
	 * that.
	 */
	min_rel_cnt = aggsf;
	max_rel_cnt = max_rbytes / pkt_avg_octet;
	max_rel_cnt = MAX(min_rel_cnt, (MAX((atf_rbytes_min / pkt_avg_octet), max_rel_cnt)));

	return PKTS_TO_MPDUS(aggsf, max_rel_cnt);
}
#endif /* WLATF */

/**
 * An SQS version of the original wlc_ampdu_txeval function
 */
int
wlc_sqs_ampdu_evaluate(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tx_t *scb_ampdu,
	uint8 prio, bool force, taf_scheduler_public_t* taf)
{
	wlc_info_t *wlc = ampdu_tx->wlc;
	scb_ampdu_tid_ini_t *ini = NULL;
	uint16 n_mpdus_to_release = 0;
	uint16 in_transit = 0;  /**< #mpdus pending in wlc/dma/d11 for given tid/ini */
	ampdu_tx_config_t *ampdu_tx_cfg = ampdu_tx->config;
	wlc_bsscfg_t *cfg;
	struct scb *scb;
	uint16 qlen;
	uint16 tbr_pkts;
	bool amsdu_in_ampdu = FALSE;
	uint8 aggsf = 1;
	uint16 ampdu_pktq_n_pkts = 0;
	uint16 ampdu_pktq_total_pkts = 0;

	ASSERT(scb_ampdu);
	ASSERT(scb_ampdu->scb);

	if (!SCB_AMPDU(scb_ampdu->scb)) {
		/* Note that with a WDS scb, the AMPDU capable flag could be cleared in
		 * wlc_ht_update_scbstate upon receiving a beacon before ampdu_cleanup_tid_ini
		 * is called and this evaluate function is invoked.
		 */
		goto eval_done;
	}

	/* initialize initiator on first packet; sends addba req */
	if (!(ini = scb_ampdu->ini[prio])) {
		/* error handling */
		ini = wlc_ampdu_init_tid_ini(ampdu_tx, scb_ampdu, prio, FALSE);
		if (!ini) {
			WL_ERROR(("%s: ini NULL -- returning\n", __FUNCTION__));
			goto eval_done;
		}
	}

	if (!wlc_scb_restrict_can_txeval(wlc)) {
		WL_ERROR(("%s: wlc_scb_restrict_can_txeval returns FALSE\n", __FUNCTION__));
		goto eval_done; /* channel switch / data block related */
	}

	/* Use the number of to-be-released packets in last evaluation */
	tbr_pkts = scb_ampdu->txq.q[prio].tbr_pkts;
	if (!force && (tbr_pkts > 0)) {
		n_mpdus_to_release = tbr_pkts;
		goto eval_done;
	}

	scb = scb_ampdu->scb;

	amsdu_in_ampdu = WLC_AMSDU_IN_AMPDU(wlc, scb, ini->tid);
	aggsf = amsdu_in_ampdu ? wlc_amsdu_scb_max_sframes((wlc)->ami, (scb), (ini->tid)) : 1;

	ampdu_pktq_n_pkts = ampdu_pktqprec_n_pkts(&scb_ampdu->txq, prio);
	ampdu_pktq_total_pkts = SQS_AMPDU_PKTLEN(&scb_ampdu->txq, prio);

	/* In CFP mode AMPDU Qs carry packet count instead of MPDU count */
	ampdu_pktq_n_pkts = PKTS_TO_MPDUS(aggsf, ampdu_pktq_n_pkts);
	ampdu_pktq_total_pkts = PKTS_TO_MPDUS(aggsf, ampdu_pktq_total_pkts);

#ifdef WLTAF
	if (taf) {
		qlen = ampdu_pktq_n_pkts;
		taf->ias.was_emptied = (qlen == 0);

	} else
#endif // endif
	if (!force || ((qlen = ampdu_pktq_n_pkts) == 0)) {
		qlen = ampdu_pktq_total_pkts;
	}

	WL_AMPDU_TX(("%s: qlen<%u> vpkts<%u> nptks<%u>\n", __FUNCTION__, qlen,
		scb_ampdu->txq.q[prio].v_pkts, scb_ampdu->txq.q[prio].n_pkts));
	if (qlen == 0) {
		goto eval_done;
	}

	cfg = SCB_BSSCFG(scb);
	ASSERT(cfg != NULL);
	BCM_REFERENCE(cfg);

#ifdef PROP_TXSTATUS
	if (!AMPDU_AQM_ENAB(ampdu_tx->wlc->pub) &&
		PROP_TXSTATUS_ENAB(ampdu_tx->wlc->pub) &&
		wlc_wlfc_suppr_status_query(ampdu_tx->wlc, ini->scb)) {
		wlc_ampdu_flush_ampdu_q(ampdu_tx, SCB_BSSCFG(ini->scb));
		goto eval_done;
	}
#endif /* PROP_TXSTATUS */

	n_mpdus_to_release = wlc_ampdu_release_count(ini, qlen, scb_ampdu);
	in_transit = ini->tx_in_transit;

	if (ini->ba_state != AMPDU_TID_STATE_BA_ON) {
		n_mpdus_to_release = qlen;
		goto eval_done;
	}

	/*
	 * For AQM, limit the number of packets in transit simultaneously so that other
	 * stations get a chance to transmit as well.
	 */
	if (AMPDU_AQM_ENAB(wlc->pub)) {
		bool ps_pretend_limit_transit = SCB_PS_PRETEND(scb);
		bool limit_transit;

		if (IS_SEQ_ADVANCED(ini->max_seq, ini->start_seq)) {
			/* here start_seq is not 'ahead' of max_seq because of wrapping */
			in_transit = MODSUB_POW2(ini->max_seq, ini->start_seq, SEQNUM_MAX);
#ifdef PROP_TXSTATUS
			if (in_transit >= ini->suppr_window) {
				/* suppr_window : suppr packet count inside ba window */
				in_transit -= ini->suppr_window;
			} else {
				in_transit = 0;
			}
#endif /* PROP_TXSTATUS */
		} else {
			/* here start_seq is 'ahead' of max_seq because of wrapping */
			in_transit = 0;
		}

		ASSERT(in_transit < SEQNUM_MAX/2);

#ifdef PSPRETEND
		/* scb->ps_pretend_start contains the TSF time for when ps pretend was
		 * last activated. In poor link conditions, there is a high chance that
		 * ps pretend will be re-triggered.
		 * Within a short time window following ps pretend, this code is trying to
		 * constrain the number of packets in transit and so avoid a large number of
		 * packets repetitively going between transmit and ps mode.
		 */
		if (!ps_pretend_limit_transit && SCB_PS_PRETEND_WAS_RECENT(scb)) {
			ps_pretend_limit_transit =
			        wlc_pspretend_limit_transit(wlc->pps_info, cfg, scb,
					in_transit, TRUE);
		}
#ifdef PROP_TXSTATUS
		if (PSPRETEND_ENAB(wlc->pub) &&
			SCB_TXMOD_ACTIVE(scb, TXMOD_APPS) &&
		    PROP_TXSTATUS_ENAB(wlc->pub) &&
		    HOST_PROPTXSTATUS_ACTIVATED(wlc)) {
			/* If TXMOD_APPS is active, and proptxstatus is enabled, force release.
			 * With proptxstatus enabled, they are meant to go to wlc_apps_ps_enq()
			 * to get their pktbufs freed in dongle, and instead stored in the host.
			 */
			ps_pretend_limit_transit = FALSE;
		}
#endif /* PROP_TXSTATUS */
#endif /* PSPRETEND */

		/*
		 * To prevent starvation of other tx traffic from this transmitter,
		 * domination of the 'common' queue with packets from one TID should be
		 * avoided. Unless forced, ensure that we do not have too many packets
		 * in transit for this priority.
		 *
		 * XXX Also, leave at least AMPDU_AQM_RELEASE_DEFAULT(256) or 50% room
		 * on the common Q.
		 */
		if (ps_pretend_limit_transit && !force) {
			limit_transit = TRUE;
		} else if (!taf && !force) {
			limit_transit =
#ifdef WLATF
				(wlc_ampdu_atf_holdrelease(AMPDU_ATF_STATE(ini))) ||
#endif // endif
				(in_transit > ampdu_tx->aqm_max_release[ini->tid]);
		} else {
			limit_transit = FALSE;
		}

		if (limit_transit) {
			WL_AMPDU_TX(("wl%d: %s: Stop Releasing %d in_transit %d tid %d "
				"wm %d rem_wdw %d qlen %d force %d pps %d\n",
				wlc->pub->unit, __FUNCTION__, n_mpdus_to_release, in_transit,
				ini->tid, ampdu_tx_cfg->tx_rel_lowat, wlc_ampdu_get_rem_window(ini),
				qlen, force, ps_pretend_limit_transit));
			AMPDUSCBCNTINCR(scb_ampdu->cnt.txnoroom);
			n_mpdus_to_release = 0;
		}
	}

	if (n_mpdus_to_release == 0) {
		goto eval_done;
	}

	/* release mpdus if any one of the following conditions are met */
	if (taf || (in_transit < ampdu_tx_cfg->tx_rel_lowat) ||
#ifdef WLATF
	    (wlc_ampdu_atf_lowat_release(ampdu_tx, ini)) ||
#endif // endif
	    (n_mpdus_to_release == MIN(scb_ampdu->release, ini->ba_wsize)) ||
	    force /* || AMPDU_AQM_ENAB(wlc->pub) */ )
	{
		AMPDU_ATF_INCRSTAT(&AMPDU_ATF_STATS(ini)->inputq, qlen);

		if (in_transit < ampdu_tx_cfg->tx_rel_lowat) {
			/* mpdus released due to lookahead watermark */
			WLCNTADD(ampdu_tx->cnt->txrel_wm, n_mpdus_to_release);
		}
		if (n_mpdus_to_release == scb_ampdu->release)
			WLCNTADD(ampdu_tx->cnt->txrel_size, n_mpdus_to_release);

		WL_AMPDU_TX(("wl%d: %s: Releasing %d mpdus: in_transit %d "
			"tid %d wm %d rem_wdw %d qlen %d force %d "
			"start_seq %x, max_seq %x\n",
			wlc->pub->unit, __FUNCTION__, n_mpdus_to_release, in_transit,
			ini->tid, ampdu_tx_cfg->tx_rel_lowat, wlc_ampdu_get_rem_window(ini),
			qlen, force, ini->start_seq, ini->max_seq));

		/* If there is enough real packets, just release it. */
		if (n_mpdus_to_release <= ampdu_pktq_n_pkts) {
			scb_cfp_t *scb_cfp;
			/* Check CFP state before release */
			if (CFP_ENAB(wlc->pub) &&
				wlc_cfp_tcb_is_EST(wlc, scb_ampdu->scb, ini->tid, &scb_cfp)) {
				/* Avoid out of order submission to MAC fifo.
				 * Make sure txq is drained out of all given scb packets
				 * before starting CFP release
				 */
				if (scb_cfp->cq_cnt[ini->tid]) {
					WL_INFORM(("TX pkts pending in TXQ : "
						"scb %p prio %d cnt %d\n",
						scb_cfp->scb, ini->tid,
						scb_cfp->cq_cnt[ini->tid]));
					/* Drain the TXQ  packets now */
					wlc_send_active_q(wlc);
					SCB_CFP_TXQ_DRAIN_WAIT(scb_cfp, ini->tid)++;
					n_mpdus_to_release = 0;
					goto eval_done;
				}
				wlc_cfp_ampdu_release(scb_cfp, scb_ampdu, ini,
					n_mpdus_to_release, amsdu_in_ampdu, NULL);

				if (!taf) {
					/* TO DO check cfp for taf */
					n_mpdus_to_release = 0;
				}
			} else {
				wlc_txq_info_t *qi; /**< refers to the 'common' transmit queue */
				struct pktq *common_q;		/**< multi-priority packet queue */
				uint16 wlc_txq_avail; /**< #mpdus in common/wlc queue */

				qi = SCB_WLCIFP(scb)->qi;
				common_q = WLC_GET_CQ(qi);
				wlc_txq_avail = MIN(pktq_avail(common_q),
					pktqprec_avail_pkts(common_q, WLC_PRIO_TO_PREC(ini->tid)));

				wlc_ampdu_release(wlc->ampdu_tx, scb_ampdu, ini,
					n_mpdus_to_release, wlc_txq_avail, taf);

				if (!taf) {
					n_mpdus_to_release = 0;
				}
			}
		}
		goto eval_done;
	}

	if ((ini->tx_in_transit == 0) && (in_transit != 0)) {
		WL_AMPDU_TX(("wl%d.%d %s: Cannot release: in_transit %d tid %d "
			"start_seq 0x%x barpending_seq 0x%x max_seq 0x%x\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
			in_transit, ini->tid, ini->start_seq, ini->barpending_seq, ini->max_seq));
	}

	WL_AMPDU_TX(("wl%d: %s: Cannot release: in_transit %d tid %d wm %d "
	             "rem_wdw %d qlen %d release %d\n",
	             wlc->pub->unit, __FUNCTION__, in_transit, ini->tid, ampdu_tx_cfg->tx_rel_lowat,
	             wlc_ampdu_get_rem_window(ini), qlen, n_mpdus_to_release));

	n_mpdus_to_release = 0;

eval_done:
#ifdef WLATF
	if (!taf && !force && ini && AMPDU_ATF_ENABLED(ini)) {
		/* Adjust based on atf rbytes */
		n_mpdus_to_release = MIN(n_mpdus_to_release,
			wlc_sqs_get_atf_max_limit(scb_ampdu, ini->tid, aggsf));
	}
#endif /* WLATF */
	if (!taf) {
		scb_ampdu->txq.q[prio].tbr_pkts = n_mpdus_to_release;
	}
	return n_mpdus_to_release;
}

/** SQS AMPDU release functions */
#ifdef WLTAF
/* This function is NOT used for TAF "PULL" SQS, it is only used in pseudo-NIC working model */
int wlc_sqs_taf_admit(wlc_info_t *wlc, struct scb *scb, uint8 prio, uint16 v_pkts)
{
	int v2r_request = 0;
	int max_q = 0;
	struct pktq *pktq = NULL;	/**< multi precedence SCB AMPDU queue */
	scb_ampdu_tx_t *scb_ampdu;
	scb_cfp_t *scb_cfp;

	if (!wlc->pub->up) {
		return 0;
	}

	scb_ampdu = SCB_AMPDU_TX_CUBBY(wlc->ampdu_tx, scb);
	scb_cfp = wlc_scb_cfp_cubby(wlc, scb);

	if (!scb_ampdu) {
		return 0;

	}
	pktq = &scb_ampdu->txq;

	wlc_sqs_pktq_vpkts_enqueue(wlc, scb_cfp, prio, v_pkts);

	/* Check if WL blocked */
	if (wlc_sqs_fifo_paused(prio)) {
		WL_TAFF(wlc, MACF" FIFO paused\n", ETHER_TO_MACF(scb->ea));
		return 0;
	}

	max_q = (wlc->ampdu_tx->aqm_max_release[prio]) - pktqprec_n_pkts(pktq, prio);

	if (max_q > 0) {
		/* TO DO - something more clever might be required */
		v2r_request = MIN(pktq->q[prio].v_pkts, max_q);
	}

	return v2r_request;
}
#endif /* WLTAF */

/** SQS AMPDU release functions */
int
wlc_sqs_ampdu_admit(wlc_info_t *wlc, scb_cfp_t *scb_cfp, uint8 prio, uint16 v_pkts)
{
	int v2r_request = 0;
	int release;	/* all releases is in units of packets and not octets */
	scb_ampdu_tid_ini_t *ini;
	struct pktq *pktq;	/**< multi precedence SCB AMPDU queue */
	void *pktlist_emit;	/* linked list of real packets that have been emitted
				 * from a sqs qulified queue.
				 */
	struct scb *scb;
	scb_ampdu_tx_t *scb_ampdu;
	uint16 tbr_pkts;
	bool amsdu_in_ampdu = FALSE;
	scb_cfp_t *scb_cfp_out;
	uint8 aggsf;
#ifdef SQS_DEBUG
	char addr_str[ETHER_ADDR_STR_LEN];

	BCM_REFERENCE(addr_str);
#endif // endif

	if (!wlc->pub->up)
		goto admit_done;

	scb = SCB_CFP_SCB(scb_cfp);
	ASSERT(SCB_AMPDU(scb));
	scb_ampdu = SCB_AMPDU_TX_CUBBY(wlc->ampdu_tx, scb);
	ASSERT(scb_ampdu);
	pktq = &scb_ampdu->txq;	/* multi precedence SCB AMPDU queue */

	if (v_pkts)
		wlc_sqs_pktq_vpkts_enqueue(wlc, scb_cfp, prio, v_pkts);

	/* Check if WL blocked */
	if (wlc_sqs_fifo_paused(prio)) {
		WL_TRACE(("%s: FIFO paused\n", __FUNCTION__));
		goto admit_done;
	}

	ini = scb_ampdu->ini[prio];

	if (ini) {
		/* Check if amsdu_in_ampdu supported  */
		amsdu_in_ampdu = WLC_AMSDU_IN_AMPDU(wlc, scb, ini->tid);
	}

	aggsf = amsdu_in_ampdu ? wlc_amsdu_scb_max_sframes(wlc->ami, scb_ampdu->scb, ini->tid) : 1;

	tbr_pkts = scb_ampdu->txq.q[prio].tbr_pkts;

	if (tbr_pkts && (tbr_pkts >= scb_ampdu->release)) {
		/* Already have an estimated to-be-released count.
		 * Attempt the release now.
		 */
		release = tbr_pkts;

		/* reset to-be-released pkt count */
		scb_ampdu->txq.q[prio].tbr_pkts = 0;
	} else {
		/* Re-evaluation is needed, so invalidate the to-be-released pkt count */
		scb_ampdu->txq.q[prio].tbr_pkts = 0;
		/* proceed immediately to trigger #1 */
		release = wlc_sqs_ampdu_evaluate(wlc->ampdu_tx, scb_ampdu, prio, FALSE, NULL);
	}

	if (release == 0) {
		goto admit_done;
	}

	/* returns a list of real packets and updates any sqs state in pktq */
	pktlist_emit = wlc_sqs_pktq_release(scb, pktq, prio,
		MPDUS_TO_PKTS(aggsf, release),
		&v2r_request, amsdu_in_ampdu, scb_ampdu->max_pdu);

	if (pktlist_emit != NULL) {
		/* real packets available, so send them through */
		ASSERT(ini);
		/* Check CFP state before release */
		if (CFP_ENAB(wlc->pub) &&
			wlc_cfp_tcb_is_EST(wlc, scb_ampdu->scb, ini->tid, &scb_cfp_out)) {
			ASSERT(scb_cfp_out == scb_cfp);
			/* Avoid out of order submission to MAC fifo.
			 * Make sure txq is drained out of all given scb packets before starting CFP
			 * release
			 */
			if (scb_cfp->cq_cnt[ini->tid]) {
				WL_INFORM(("TX pkts pending in TXQ : scb %p prio %d cnt %d  \n",
					scb_cfp->scb, ini->tid, scb_cfp->cq_cnt[ini->tid]));
				/* Drain the TXQ  packets now */
				wlc_send_active_q(wlc);
				SCB_CFP_TXQ_DRAIN_WAIT(scb_cfp, ini->tid)++;
				goto admit_done;
			}
			ASSERT(scb_cfp_out == scb_cfp);
			wlc_cfp_ampdu_release(scb_cfp, scb_ampdu, ini, release, amsdu_in_ampdu,
				NULL);
		} else {
			wlc_txq_info_t *qi; /**< refers to the 'common' transmit queue */
			struct pktq *common_q;		/**< multi-priority packet queue */
			uint16 wlc_txq_avail; /**< #mpdus in common/wlc queue */

			qi = SCB_WLCIFP(scb)->qi;
			common_q = WLC_GET_CQ(qi);
			wlc_txq_avail = MIN(pktq_avail(common_q),
				pktqprec_avail_pkts(common_q, WLC_PRIO_TO_PREC(ini->tid)));
			wlc_ampdu_release(wlc->ampdu_tx, scb_ampdu, ini,
				release, wlc_txq_avail, NULL);
		}
	}

admit_done:
	return v2r_request;
}

void
wlc_sqs_v2r_ampdu_sendup(wlc_info_t *wlc, uint8 prio,
	void *pktlist_head, void *pktlist_tail, uint16 pkt_count, scb_cfp_t *scb_cfp)
{
	const uint8 tid = prio; /* same as pkt prio */
	scb_ampdu_tx_t *scb_ampdu;
	scb_ampdu_tid_ini_t *ini;
	int release = pkt_count;
	struct scb *scb;
#ifdef WLTAF
	bool use_taf = wlc_taf_in_use(wlc->taf_handle);
#else
	const bool use_taf = FALSE;
#endif // endif

	ASSERT(pkt_count);

	scb = SCB_CFP_SCB(scb_cfp);
	scb_ampdu = SCB_AMPDU_TX_CUBBY(wlc->ampdu_tx, scb);
	ASSERT(scb_ampdu);

	/* real packets available, send them through */
	ini = scb_ampdu->ini[tid];
	ASSERT(ini);

	/* Enqueue  Real packets to SCB precedence Queues. */
	release = wlc_cfp_ampdu_enqueue(&scb_ampdu->txq, tid, pktlist_head, pktlist_tail,
		pkt_count);

	/* Avoid out of order submission to MAC fifo.
	 * Make sure txq is drained out of all given scb packets before starting CFP release
	 */
	if (scb_cfp->cq_cnt[tid]) {
		WL_INFORM(("TX pkts pending in TXQ : scb %p prio %d cnt %d  \n",
			scb_cfp->scb, tid, scb_cfp->cq_cnt[tid]));
		/* Drain the TXQ  packets now */
		wlc_send_active_q(wlc);
		SCB_CFP_TXQ_DRAIN_WAIT(scb_cfp, tid)++;

		return;
	}
#ifdef WLTAF
	if (use_taf) {
		/* kick the TAF scheduler */
		use_taf = wlc_taf_schedule(wlc->taf_handle, prio, scb, FALSE);

	} /* this falls though if wlc_taf_schedule returns FALSE */
#endif // endif
	if (!use_taf) {
		/* In CFP mode AMPDU Qs carry packet count instead of MPDU count */
		release = WLC_AMPDU_PKTS_TO_MPDUS(wlc, scb, tid, release);

		wlc_cfp_ampdu_release(scb_cfp, scb_ampdu, ini, release,
			WLC_AMSDU_IN_AMPDU(wlc, scb, ini->tid), NULL);
	}
}

void
wlc_sqs_ampdu_v2r_enqueue(wlc_info_t *wlc, scb_cfp_t *scb_cfp, uint8 prio, uint16 v2r_count)
{
	scb_ampdu_tx_t *scb_ampdu;
	struct scb *scb;
	struct pktq *pktq;		/**< Multi-priority packet queue */
	struct pktq_prec *pktqp;	/**< single precedence packet queue */

	scb = SCB_CFP_SCB(scb_cfp);

	if (SCB_INTERNAL(scb)) {
		return;
	}
	scb_ampdu = SCB_AMPDU_TX_CUBBY(wlc->ampdu_tx, scb);
	ASSERT(scb_ampdu);
	pktq = &scb_ampdu->txq;
	pktqp = &pktq->q[prio];	/**< single precedence packet queue */

	/* Sanity check for the virtual packets */
	ASSERT(pktqp->v_pkts >= v2r_count);
	ASSERT(pktq->v_pkts_tot >= v2r_count);

	/* Account for v2r in progress, by transferring from v_pkts to v2r_pkts */
	pktqp->v_pkts    -= v2r_count;
	pktqp->v2r_pkts  += v2r_count;

	/* Cummulative across all precedences */
	pktq->v_pkts_tot -= v2r_count;
}

void
wlc_sqs_ampdu_v2r_dequeue(wlc_info_t *wlc, scb_cfp_t *scb_cfp, uint8 prio, uint16 pkt_count)
{
	scb_ampdu_tx_t *scb_ampdu;
	struct scb *scb;
	struct pktq *pktq;		/**< Multi-priority packet queue */

	scb = SCB_CFP_SCB(scb_cfp);

	if (SCB_INTERNAL(scb)) {
		return;
	}
	scb_ampdu = SCB_AMPDU_TX_CUBBY(wlc->ampdu_tx, scb);
	ASSERT(scb_ampdu);
	pktq = &scb_ampdu->txq;

	ASSERT(pktq);

	ASSERT(pktq->q[prio].v2r_pkts >= pkt_count);
	pktq->q[prio].v2r_pkts -= pkt_count;
}

#ifdef WLATF
int
wlc_ampdu_tx_intransit_get(wlc_info_t *wlc)
{
	return (wlc->ampdu_tx->tx_intransit);
}
#endif /* WLATF */
#endif /* WLSQS */

#ifdef WLSCB_HISTO
static void BCMFASTPATH
wlc_update_rate_histogram(struct scb *scb, ratesel_txs_t *rs_txs,
		uint16 succ_mpdu, uint16 succ_msdu, uint8 amsdu_sf)
{
	uint16 distr_cnt, txsucc_cnt, temp, r;

	/* We do not have per msdu rate informantion from txstatus
	 * packages, therefore approximating the msdu per rate count.
	 * At first, fill all the per rate msdu count and then
	 * distribute the remaining of total msdu count to each rate.
	 */

	distr_cnt = succ_msdu - succ_mpdu;

	for (r = 0; r < RATESEL_MFBR_NUM; r++) {

		txsucc_cnt = rs_txs->txsucc_cnt[r];
		if (amsdu_sf > 1) {
			temp = MIN(txsucc_cnt*(amsdu_sf -1), distr_cnt);
			txsucc_cnt += temp;
			distr_cnt -= temp;
		}

		WLSCB_HISTO_TX_IF_HTVHT(scb, rs_txs->txrspec[r], txsucc_cnt);

		if (!distr_cnt)
			break;
	}
	if (distr_cnt) {
		/* For sanity check, add the remaining count to main rate */
		WLSCB_HISTO_TX_IF_HTVHT(scb, rs_txs->txrspec[0], distr_cnt);
	}

}
#endif /* WLSCB_HISTO */
#ifdef WLSQS
#ifdef WLTAF
/* AMPDU packet pull API exposed to TAF
 * If there are enough real packets, this would trigger immediate release[ CFP or legacy].
 * If not this would trigger a V2R fetch from host side
 *
 * @param request_pkt_cnt : No of packets to be released.
 * @return Successful count of MPDUs released
 */
uint16
wlc_ampdu_pull_packets(wlc_info_t* wlc, struct scb* scb, uint8 tid, uint16 request_pkt_cnt,
	taf_scheduler_public_t* taf)
{
	scb_ampdu_tx_t* scb_ampdu;
	struct pktq *pktq;	/**< multi precedence SCB AMPDU queue */
	uint16 release_cnt;

	/* Initialize */
	scb_ampdu = SCB_AMPDU_TX_CUBBY(wlc->ampdu_tx, scb);

	if (!scb_ampdu) {
		return 0;
	}

	pktq = &scb_ampdu->txq;	/* multi precedence SCB AMPDU queue */

	WL_AMPDU(("%s : SQS AMPDU PULL packets   scb %p "MACF" tid %u PKT cnt %d \n",
		__FUNCTION__, scb, ETHER_TO_MACF(scb->ea), tid,
		request_pkt_cnt));

	/* Real packets not available. Raise V2R request and exit */
	release_cnt = wlc_sqs_ampdu_pull_packets(wlc, scb, pktq,
		tid, request_pkt_cnt);

	/* Report back successfull release packet count */
	return (release_cnt);
}
#endif /* WLTAF */
#endif /* WLSQS */

/* AMPDU pktq dequeue function exposed outside */
void*
wlc_ampdu_pktq_pdeq(wlc_info_t *wlc, struct scb* scb, uint8 prio)
{
	scb_ampdu_tx_t *scb_ampdu;
	struct pktq *pktq = NULL;
	void* p;

	scb_ampdu = SCB_AMPDU_TX_CUBBY(wlc->ampdu_tx, scb);
	ASSERT(scb_ampdu);

	/* AMPDU precendece Queue */
	pktq = &scb_ampdu->txq;
	ASSERT(pktq);
	p = _wlc_ampdu_pktq_pdeq(pktq, prio);

	return p;
}
/* AMPDU pktq enqueue function exposed outside */
void
wlc_ampdu_pktq_penq(wlc_info_t *wlc, struct scb* scb, uint8 prio, void* p)
{
	scb_ampdu_tx_t *scb_ampdu;
	struct pktq *pktq = NULL;

	scb_ampdu = SCB_AMPDU_TX_CUBBY(wlc->ampdu_tx, scb);
	ASSERT(scb_ampdu);

	/* AMPDU precendece Queue */
	pktq = &scb_ampdu->txq;
	ASSERT(pktq);

	pktq_penq(pktq, prio, p);
}
/* AMPDU pktq enqueue function exposed outside */
void
wlc_ampdu_pktq_penq_head(wlc_info_t *wlc, struct scb* scb, uint8 prio, void* p)
{
	scb_ampdu_tx_t *scb_ampdu;
	struct pktq *pktq = NULL;

	scb_ampdu = SCB_AMPDU_TX_CUBBY(wlc->ampdu_tx, scb);
	ASSERT(scb_ampdu);

	/* AMPDU precendece Queue */
	pktq = &scb_ampdu->txq;
	ASSERT(pktq);

	pktq_penq_head(pktq, prio, p);
}

#ifdef WLTAF
/* try to simplify for TAF */
bool
wlc_ampdu_taf_release(void* ampduh, void* scbh, void* tidh, bool force, taf_scheduler_public_t* taf)
{
	ampdu_tx_info_t *ampdu_tx = ampduh;
	scb_ampdu_tx_t *scb_ampdu = scbh;
	scb_ampdu_tid_ini_t *ini = tidh;

	wlc_info_t *wlc = ampdu_tx->wlc;
	uint16 n_mpdus_to_release = 0;
	uint16 cfp_pkts_released = 0;
	uint16 in_transit = 0;  /**< #mpdus pending in wlc/dma/d11 for given tid/ini */

	struct scb *scb;
	uint16 qlen;
	int tid = ini->tid;

	uint32 aggsf = 1;

	if (!wlc_scb_restrict_can_txeval(wlc)) {
		WL_ERROR(("%s: wlc_scb_restrict_can_txeval returns FALSE\n", __FUNCTION__));
		return FALSE;
	}
	if (ini->ba_state != AMPDU_TID_STATE_BA_ON) {
		return FALSE;
	}
	scb = scb_ampdu->scb;

	qlen = ampdu_pktqprec_n_pkts(&scb_ampdu->txq, tid);

	in_transit = ini->tx_in_transit;

#ifdef WLCFP
	/* AMSDU happens now within CFP, so control how it is done */
	if (qlen && (CFP_ENAB(wlc->pub) == TRUE)) {
		aggsf = WLC_AMSDU_IN_AMPDU(wlc, scb, tid) ?
			wlc_amsdu_scb_max_sframes(wlc->ami, scb, tid) : 1;

		/* AMSDU optimisation based upon whether more traffic is expected to arrive */
		if (taf->how == TAF_RELEASE_LIKE_IAS && taf->ias.opt_aggs) {
			/* round down as more data is coming next */
			n_mpdus_to_release = qlen / aggsf;

		} else {
			/* round up as no more data is expected */
			n_mpdus_to_release = PKTS_TO_MPDUS(aggsf, qlen);
		}
	} else /* fall through to non-CFP case */
#endif /* WLCFP */
	{
		n_mpdus_to_release = qlen;
	}

	/* test whether to hold back small releases to improve AMPDU agg efficiency */
	if (taf->how == TAF_RELEASE_LIKE_IAS && taf->ias.opt_aggp) {
		if (qlen <= taf->ias.opt_aggp_limit) {
			n_mpdus_to_release = 0;
		}
	}

	if (qlen > (n_mpdus_to_release * aggsf)) {
		WL_TAFF(wlc, "agg hold (total qlen %u) release %u mpdus\n", qlen,
			n_mpdus_to_release);
	}

	if (n_mpdus_to_release == 0) {
#ifdef BCMDBG
		taf->complete = TAF_REL_COMPLETE_NOTHING;
#endif // endif

		if (taf->how == TAF_RELEASE_LIKE_IAS) {
			taf->ias.was_emptied = TRUE;
		}
		if (taf->how == TAF_RELEASE_LIKE_DEFAULT) {
			taf->def.was_emptied = TRUE;
		}
		return FALSE;
	}

	if (AMPDU_AQM_ENAB(wlc->pub)) {
		bool ps_pretend_limit_transit = SCB_PS_PRETEND(scb);
#ifdef PSPRETEND
		/* In poor link conditions, there is a high chance that
		 * ps pretend will be re-triggered.
		 * Within a time window following ps pretend, this code is trying to
		 * constrain the number of packets in transit and so avoid a large number of
		 * packets repetitively going between transmit and ps mode.
		 */
#ifdef PROP_TXSTATUS
		if (PSPRETEND_ENAB(wlc->pub) &&
				SCB_TXMOD_ACTIVE(scb, TXMOD_APPS) &&
				PROP_TXSTATUS_ENAB(wlc->pub) &&
				HOST_PROPTXSTATUS_ACTIVATED(wlc)) {
			/* If TXMOD_APPS is active, and proptxstatus is enabled, force release.
			 * With proptxstatus enabled, they are meant to go to wlc_apps_ps_enq()
			 * to get their pktbufs freed in dongle, and instead stored in the host.
			 */
			ps_pretend_limit_transit = FALSE;
		} else
#endif /* PROP_TXSTATUS */
			if (!ps_pretend_limit_transit && SCB_PS_PRETEND_WAS_RECENT(scb)) {
				if (IS_SEQ_ADVANCED(ini->max_seq, ini->start_seq)) {
					/* here start_seq is not 'ahead' of max_seq because of
					 * wrapping
					 */
					in_transit = MODSUB_POW2(ini->max_seq, ini->start_seq,
						SEQNUM_MAX);
#ifdef PROP_TXSTATUS
					if (in_transit >= ini->suppr_window) {
						/* suppr_window : suppr packet count inside ba
						 * window
						 */
						in_transit -= ini->suppr_window;
					} else {
						in_transit = 0;
					}
#endif /* PROP_TXSTATUS */
				} else {
					/* here start_seq is ahead of max_seq because of wrapping */
					in_transit = 0;
				}
				ASSERT(in_transit < SEQNUM_MAX/2);

				ps_pretend_limit_transit =
					wlc_pspretend_limit_transit(wlc->pps_info, SCB_BSSCFG(scb),
					scb, in_transit, TRUE);
			}
#endif /* PSPRETEND */
		if (ps_pretend_limit_transit && !force) {
			WL_AMPDU_TX(("wl%d: %s: Stop Releasing in_transit %d tid %d "
				     "qlen %d pps %d\n",
				     wlc->pub->unit, __FUNCTION__, in_transit, tid, qlen,
				     ps_pretend_limit_transit));
			return FALSE;
		}
	}
	WL_AMPDU_TX(("wl%d: %s: Releasing %d: in_transit %d tid %d force %d start_seq %x, "
		     "max_seq %x\n", wlc->pub->unit, __FUNCTION__, qlen, in_transit,
		     tid, force, ini->start_seq, ini->max_seq));

	/* If there are any real packets, just release them. All of them. In one go. Why not ?
	 * TAF will stop release when scheduling window has filled regardless if more packets
	 * were requested.
	 */
#ifdef WLCFP
	if (taf->mode == TAF_RELEASE_MODE_REAL_FAST) {
		scb_cfp_t *scb_cfp;

		/* Check CFP state before release */
		if (CFP_ENAB(wlc->pub) && wlc_cfp_tcb_is_EST(wlc, scb, tid, &scb_cfp)) {

			/* Avoid out of order submission to MAC fifo.
			 * Make sure txq is drained out of all given scb packets
			 * before starting CFP release
			 */
			if (scb_cfp->cq_cnt[tid]) {
				WL_INFORM(("TX pkts pending in TXQ : scb %p prio %d cnt %d\n",
					scb_cfp->scb, tid, scb_cfp->cq_cnt[tid]));
				/* Drain the TXQ  packets now */
				wlc_send_active_q(wlc);
				SCB_CFP_TXQ_DRAIN_WAIT(scb_cfp, tid)++;
				return FALSE;
			}
			cfp_pkts_released = wlc_cfp_ampdu_release(scb_cfp, scb_ampdu, ini,
				n_mpdus_to_release, WLC_AMSDU_IN_AMPDU(wlc, scb, tid), taf);

			/* taf->mode might be set to TAF_RELEASE_MODE_REAL during CFP release in
			 * case of issue
			 */

			if (taf->mode == TAF_RELEASE_MODE_REAL &&
					n_mpdus_to_release > cfp_pkts_released) {

				WL_TAFF(wlc, MACF" tid %u continue slow path (%u released fast, "
					"%d outstanding, %d)\n", ETHER_TO_MACF(scb->ea), tid,
					cfp_pkts_released, n_mpdus_to_release - cfp_pkts_released,
					taf->mode);

				n_mpdus_to_release -= cfp_pkts_released;
			} else {
				n_mpdus_to_release = 0;
			}
		} else {
			/* TAF_RELEASE_MODE_REAL_FAST isn't setup, fallback to normal */
			taf->mode = TAF_RELEASE_MODE_REAL;
		}
	}
#endif /* WLCFP */
	/* we might get here if we fall through from CFP release or else we never took CFP */
	if (taf->mode == TAF_RELEASE_MODE_REAL && n_mpdus_to_release) {
		struct pktq *common_q = WLC_GET_CQ(SCB_WLCIFP(scb)->qi);
		uint16 wlc_txq_avail = MIN(pktq_avail(common_q), pktqprec_avail_pkts(common_q,
			WLC_PRIO_TO_PREC(tid)));

		if (wlc_txq_avail == 0) {
			AMPDUSCBCNTINCR(scb_ampdu->cnt.txnoroom);
		}
		n_mpdus_to_release = MIN(n_mpdus_to_release, wlc_txq_avail);

		if (n_mpdus_to_release > 0) {
#ifdef WLCFP
			wlc_cfp_tcb_upd_pause_state(wlc, scb, TRUE);
#endif /* WLCFP */
			n_mpdus_to_release = wlc_ampdu_release(ampdu_tx, scb_ampdu, ini,
				n_mpdus_to_release, wlc_txq_avail, taf);
#ifdef WLCFP
			wlc_cfp_tcb_upd_pause_state(wlc, scb, FALSE);
#endif /* WLCFP */
		}
	}

	if (ampdu_pktqprec_n_pkts(&scb_ampdu->txq, tid) == 0) {
		if (taf->how == TAF_RELEASE_LIKE_IAS) {
			taf->ias.was_emptied = TRUE;
#ifdef BCMDBG
			taf->complete = taf->ias.is_ps_mode ?
				TAF_REL_COMPLETE_EMPTIED_PS : TAF_REL_COMPLETE_EMPTIED;
#endif // endif
		}
		if (taf->how == TAF_RELEASE_LIKE_DEFAULT) {
#ifdef BCMDBG
			taf->complete = TAF_REL_COMPLETE_EMPTIED;
#endif // endif
			taf->def.was_emptied = TRUE;
		}
	}
	return (n_mpdus_to_release + cfp_pkts_released > 0);
}
#endif /* WLTAF */
