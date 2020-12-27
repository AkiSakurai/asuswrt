/*
 * WiFi multi channel source file
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
 * $Id: wlc_mchan.c 771216 2019-01-18 14:11:18Z $
 */

/**
 * @file
 * @brief
 * XXX Twiki: [WlMultiChannel]
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>

#ifdef WLMCHAN

#ifndef WLMCNX
#error "WLMCNX must be defined when WLMCHAN is defined!"
#endif // endif

#ifndef WLP2P
#error "WLP2P must be defined when WLMCHAN is defined!"
#endif // endif

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmendian.h>
#include <bcmutils.h>
#include <siutils.h>
#include <wlioctl.h>
#include <bcmwpa.h>
#include <bcmdevs.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wlc_scan.h>
#include <wlc_bmac.h>
#include <wl_export.h>
#include <wlc_utils.h>
#include <wlc_mcnx.h>
#include <wlc_tbtt.h>
#include <wlc_p2p.h>
#include <wlc_mchan.h>
#include <wlc_rsdb.h>
#include <wlc_ie_misc_hndlrs.h>
#include <wlc_bsscfg_psq.h>
#include <phy_cache_api.h>
#include <wlc_hrt.h>
#ifdef PROP_TXSTATUS
#include <wlc_ampdu.h>
#include <wlc_apps.h>
#include <wlc_wlfc.h>
#endif // endif
#ifdef WL_MODESW
#include <wlc_modesw.h>
#endif // endif

#include <wlc_btcx.h>
#include <wlc_tx.h>
#include <wlc_ie_mgmt.h>
#include <wlc_lq.h>
#ifdef WLAMSDU
#include <wlc_amsdu.h>
#endif // endif
#include <wlc_assoc.h>
#include <wlc_pm.h>
#include <wlc_chanctxt.h>
#include <wlc_sta.h>
#include <wlc_ap.h>
#include <wlc_hw.h>
#include <wlc_dump.h>

/* iovar table */
enum {
	IOV_MCHAN,		/* enable/disable multi channel feature */
	IOV_MCHAN_STAGO_DISAB,	/* enable/disable multi channel sta-go feature */
	IOV_MCHAN_PRETBTT,	/* get/set mchan pretbtt time */
	IOV_MCHAN_PROC_TIME,	/* get/set mchan proc time */
	IOV_MCHAN_CHAN_SWITCH_TIME,	/* get/set mchan chan switch time */
	IOV_MCHAN_BLOCKING_ENAB,	/* get/set mchan blocking enab */
	IOV_MCHAN_BLOCKING_THRESH,	/* get/set mchan blocking thresh */
	IOV_MCHAN_BYPASS_PM,	/* get/set mchan bypass pm feature */
	IOV_MCHAN_BYPASS_DTIM,	/* get/set mchan bypass dtim rule feature for chan sw */
	IOV_MCHAN_TEST,		/* JHCHIANG test iovar */
	IOV_MCHAN_SCHED_MODE,	/* get/set mchan sched mode */
	IOV_MCHAN_MIN_DUR,	/* get/set mchan minimum duration in a channel */
	IOV_MCHAN_SWTIME,
	IOV_MCHAN_ALGO,
	IOV_MCHAN_BW,
	IOV_MCHAN_BCNPOS,
	IOV_MCHAN_BCN_OVLP_OPT
};

static const bcm_iovar_t mchan_iovars[] = {
	{"mchan", IOV_MCHAN, 0, 0, IOVT_BOOL, 0},
	{"mchan_stago_disab", IOV_MCHAN_STAGO_DISAB, 0, 0, IOVT_BOOL, 0},
	{"mchan_pretbtt", IOV_MCHAN_PRETBTT, 0, 0, IOVT_UINT16, 0},
	{"mchan_proc_time", IOV_MCHAN_PROC_TIME, 0, 0, IOVT_UINT16, 0},
	{"mchan_chan_switch_time", IOV_MCHAN_CHAN_SWITCH_TIME, 0, 0, IOVT_UINT16, 0},
	{"mchan_blocking_enab", IOV_MCHAN_BLOCKING_ENAB, 0, 0, IOVT_BOOL, 0},
	{"mchan_blocking_thresh", IOV_MCHAN_BLOCKING_THRESH, 0, 0, IOVT_UINT8, 0},
	{"mchan_bypass_pm", IOV_MCHAN_BYPASS_PM, 0, 0, IOVT_BOOL, 0},
	{"mchan_bypass_dtim", IOV_MCHAN_BYPASS_DTIM, 0, 0, IOVT_BOOL, 0},
	{"mchan_test", IOV_MCHAN_TEST, 0, 0, IOVT_UINT32, 0},
	{"mchan_sched_mode", IOV_MCHAN_SCHED_MODE, 0, 0, IOVT_UINT32, 0},
	{"mchan_min_dur", IOV_MCHAN_MIN_DUR, 0, 0, IOVT_UINT32, 0},
	{"mchan_swtime", IOV_MCHAN_SWTIME, 0, 0, IOVT_UINT32, 0},
	{"mchan_algo", IOV_MCHAN_ALGO, 0, 0, IOVT_UINT32, 0},
	{"mchan_bw", IOV_MCHAN_BW, 0, 0, IOVT_UINT32, 0},
	{"mchan_bcnpos", IOV_MCHAN_BCNPOS, 0, 0, IOVT_UINT32, 0},
	{"mchan_bcnovlpopt", IOV_MCHAN_BCN_OVLP_OPT, 0, 0, IOVT_BOOL, 0},
	{NULL, 0, 0, 0, 0, 0}
};

/* Used to indicate the type of tbtt adjustment */
typedef enum {
	MCHAN_TSF_ADJ_REGULAR,
	MCHAN_TSF_ADJ_SI
} mchan_tsf_adj;

/* Used to indicate the different types of schedules */
typedef enum {
	WLC_MCHAN_SCHED_MODE_FAIR = 0,	/* fair schedule */
	WLC_MCHAN_SCHED_MODE_STA,	/* schedule to favor sta(BSS) interface */
	WLC_MCHAN_SCHED_MODE_P2P,	/* schedule to favor p2p(GO/GC) interface */
	WLC_MCHAN_SCHED_MODE_MAX
} wlc_mchan_sched_mode_t;

#define MCHAN_LOAD_GREATER_THAN_MAX(load) \
	(load >= MCHAN_MAX_BYTE_THRESHOLD)

#define MCHAN_MAX_LOAD_ON_BOTH_IF(prim_load, sec_load) \
	(MCHAN_LOAD_GREATER_THAN_MAX(prim_load) && \
	MCHAN_LOAD_GREATER_THAN_MAX(sec_load))

#define WLC_MAP_LOAD_TO_MIMO(load) \
	(load >>= 1)

#define MCHAN_HOST_LOAD_ON_SINGLE_IF(prim_load, sec_load) \
	(((prim_load > MCHAN_NO_HOST_LOAD_BYTES) && \
	(sec_load <= MCHAN_NO_HOST_LOAD_BYTES)) || \
	((prim_load <= MCHAN_NO_HOST_LOAD_BYTES) && \
	(sec_load > MCHAN_NO_HOST_LOAD_BYTES)))

#define MCHAN_NO_HOST_LOAD_ON_BOTH_IF(prim_load, sec_load) \
	(prim_load <= MCHAN_NO_HOST_LOAD_BYTES) && \
	(sec_load <= MCHAN_NO_HOST_LOAD_BYTES)

#define MCHAN_IS_FAST_INCREMENT_POSSIBLE(load1, load2) \
	((load1 > MCHAN_NO_HOST_LOAD_BYTES) && \
	(load1 < MCHAN_MIN_BYTE_THRESHOLD) && \
	(load2 >= MCHAN_MAX_BYTE_THRESHOLD))

#define MCHAN_LOAD_GREATER_THAN_OFFSET(load1, load2, offset) \
	(load1 > (load2 + offset))

/* constants */
#define WLC_MCHAN_PRETBTT_TIME_US(m)		wlc_mchan_pretbtt_time_us(m)
#define WLC_MCHAN_PROC_TIME_BMAC_US(m)		wlc_mchan_proc_time_bmac_us(m)
#define WLC_MCHAN_PROC_TIME_LONG_US(m)		(17000 - WLC_MCHAN_PRETBTT_TIME_US(m))
#define WLC_MCHAN_PROC_TIME_US(m)			wlc_proc_time_us(m)
#define WLC_MCHAN_CHAN_SWITCH_TIME_US(m)	WLC_MCHAN_PROC_TIME_US(m)

#define MCHAN_MIN_BCN_RX_TIME_US	3000U
#define MCHAN_MIN_DTIM_RX_TIME_US	12000U
#define MCHAN_MIN_RX_TX_TIME		12000U
#define MCHAN_MIN_CHANNEL_PERIOD(m)	(WLC_MCHAN_PRETBTT_TIME_US(m) + MCHAN_MIN_RX_TX_TIME)
#define MCHAN_MIN_TBTT_GAP_US_NEW(m)	(MCHAN_MIN_RX_TX_TIME + WLC_MCHAN_PRETBTT_TIME_US(m))
#define MCHAN_MIN_CHANNEL_TIME(m)	(MCHAN_MIN_RX_TX_TIME + WLC_MCHAN_PRETBTT_TIME_US(m))

#define MCHAN_TBTT_SINCE_BCN_THRESHOLD	10
#define MCHAN_CFG_IDX_INVALID		(-1)

#define MCHAN_MIN_AP_TBTT_GAP_US	(30000)
#define MCHAN_FAIR_SCHED_DUR_ADJ_US	(100)
#define MCHAN_MIN_STA_TBTT_GAP_US	(30000)
#define MCHAN_MIN_TBTT_TX_TIME_US	(12000)
#define MCHAN_MIN_TBTT_RX_TIME_US	(12000)

/* If RSDB to VSDB switch requirement is seen for this dur, then switch */
#define MODESW_R2V_SAMPLE_DURATION	10
/* If VSDB to RSDB switch requirement is seen for this dur, then switch */
#define MODESW_V2R_SAMPLE_DURATION	10

#ifdef DONGLEBUILD
#define MCHAN_PROC_DELAY_US		(0)
#else
#define MCHAN_PROC_DELAY_US		(500)
#endif // endif

#define MCHAN_ABS_TIME_ADJUST		(500)
#define MCHAN_MAX_TBTT_GAP_DRIFT_US	(1000)
#define MCHAN_MAX_ONCHAN_GAP_DRIFT_US	(2000)
#define MCHAN_MIN_DUR_US		(5000)

#define MCHAN_TIME_UNIT_1024US		10
#define MCHAN_TIMING_ALIGN(time)	((time) & (~((1 << P2P_UCODE_TIME_SHIFT) - 1)))

#define MCHAN_DEFAULT_ALGO 0
#define MCHAN_BANDWIDTH_ALGO 1
#define MCHAN_SI_ALGO 2
#define MCHAN_DYNAMIC_BW_ALGO 3
#define MCHAN_ALTERNATE_SWITCHING 4

#define DEFAULT_SI_TIME (25 * 1024)
#define MCHAN_BCN_INVALID 0
#define MCHAN_BCN_OVERLAP 1
#define MCHAN_BCN_NONOVERLAP 2
#ifdef STB_SOC_WIFI
#define MCHAN_BLOCKING_CNT 5		/* Value to be changed for enhancement  */
#else
#define MCHAN_BLOCKING_CNT 3		/* Value to be changed for enhancement  */
#endif // endif
#define MCHAN_BCN_MULTI_IF 4		/* beacon position in multi interface scenario */

#define MCHAN_MAX_PERCENTAGE_BW	80 /* Maximum Bandwidth allocated for a interface */
#define MCHAN_MIN_PERCENTAGE_BW	20 /* Minimum Bandwidth allocated for a interface */
#define MCHAN_DEFAULT_BW	50 /* default bandwidth for an interface */
#define AVG_MAX_SISO_LOAD	39 /* Avg. maximum achievable load in SISO w.o.Frameburst */
#define MCHAN_MIN_MIMO_LOAD	34 /* Minimum load on MIMO interface in MCHAN =>mchan_bw 30 */
#define MCHAN_V2R_SW_LOAD_MARK	60 /* Load below which RSDB would be better than VSDB */
#define MCHAN_NO_HOST_LOAD_BYTES	2000 /* Load for driver generated pkts */
#define MCHAN_RTS_TIME_uSEC       88  /* RTS time in microseconds */
#define MCHAN_VO_DEFAULT_BYTE_THRES   3840 /* Byte threshold for Voice traffic */
#define MCHAN_VI_DEFAULT_BYTE_THRES   16384 /* Byte threshold for Video traffic  */

/* this enum holds different type of actions that can be taken in
 * dynamic bandwidth algo
 */
typedef enum {
MCHAN_BW_INC_TYPE_SLOW,		/* BW updated by 3% in this type */
MCHAN_BW_INC_TYPE_REG,		/* BW updated by 5% in this type */
MCHAN_BW_INC_TYPE_FAST,		/* BW updated by 10% in this type */
MCHAN_BW_INIT,		/* Initialize the BW to 50-50 */
MCHAN_BW_INIT_NO_LOAD,		/* Initilaize BW to 50, NO load on both i/f */
MCHAN_BW_INIT_ONE_IF_TP,	/* If the only one i/f has t/p requirement */
MCHAN_BW_INC_LOAD_ANOMALY,		/* Anomaly is seen use saved_bw */
MCHAN_BW_INC_NO_ACTION,		/* No Action to take */
} mchan_dyn_algo_action;

#define SWAP(a, b) do {  a ^= b; b ^= a; a ^= b; } while (0)
#define SWAP_PTR(T, a, b) do \
	{ \
		T* temp; \
		temp = a; a = b; b = temp; \
	} while (0)
#define MCHAN_OFFSET_BYTES        75000 /* Offset byte counts */
#define MCHAN_DYN_ALGO_SAMPLE     10 /* No of sample in case of dynamic algo */
#define MCHAN_MIN_BYTE_THRESHOLD  50000  /* Minimum byte counts */
#define MCHAN_MAX_BYTE_THRESHOLD  100000 /* Maximum byte counts */
#define MCHAN_INC_BW_FASTRATE     10  /* stepsize to increment BW  in Fasterrate */
#define MCHAN_INC_BW_MEDIUMRATE   5  /* stepsize to increment BW  in Medium Rate */
#define MCHAN_DEFAULT_BW          50 /* Default bandwidth */
#define MCHAN_INC_BW_SLOWRATE     2  /* stepsize to increment BW  in slowphase */

#define DYN_ENAB(mchan) (mchan->switch_algo == MCHAN_DYNAMIC_BW_ALGO)
#define WLC_MCHAN_DYN_ALGO_SAMPLE 10 /* No of sample in case of dynamic algo */
#define WLC_DYN_ALGO_MAX_IF 2
#define IF_PRIMARY 0
#define IF_SECONDARY 1

typedef struct _mchan_air_time {
	uint32 time[WLC_MCHAN_DYN_ALGO_SAMPLE]; /* Actual air time */
	uint32 w;
} mchan_air_time_t;

typedef struct _mchan_fw_time {
	uint32 period[WLC_MCHAN_DYN_ALGO_SAMPLE];	/* Allocated time */
	uint32 w;
} mchan_fw_time_t;

typedef struct _mchan_dyn_algo {
	cca_ucode_counts_t if_count[WLC_DYN_ALGO_MAX_IF];	/* Hold stats from ucode */
	mchan_fw_time_t if_time[WLC_DYN_ALGO_MAX_IF];	/* Holds value for period and
							 * exec time per interface
							 */
	mchan_air_time_t if_air[WLC_DYN_ALGO_MAX_IF];	/* Holds value for
							 * actual time per interface
							 */
} mchan_dyn_algo_t;
#define NUM_QOS_FIFO    2 /* Number of Qos FIFO's */
const uint8 wme_indx2fifo[NUM_QOS_FIFO] = {TX_AC_VI_FIFO, TX_AC_VO_FIFO};
#define VI_WME_IDX                0 /* QoS info index where video traffic stats are stored */
#define VO_WME_IDX                1 /* QoS info index where voice traffic stats are stored */

/* This structure defines the timer-callback-context for RSDB<->VSDB
 * mode switch. timer_type indicates the type of the timer for which
 * the callback is called.
 */
typedef struct {
	wlc_info_t *wlc;
	uint8 timer_type;
} wlc_modesw_cb_ctx;
typedef struct _mchan_qos_modesw_info {
	uint32 qos_bytes_cached[2];  /* no. of bytes TXed / Rxed till last watchdog */
	uint32 qos_bytes_delta[2];	  /* no. of bytes TXed in last watchdog */
	/* Amount of Voice traffic in bytes above which we decide to switch to VSDB */
	uint32 VO_traffic_threshold;
	/* Amount of Video traffic in bytes above which we decide to switch to VSDB */
	uint32 VI_traffic_threshold;
} mchan_qos_modesw_info_t;

typedef struct _mchan_dynalgo_params_info {
	uint32 primary_bss_load_old ;  /* Cached Primary interface load */
	uint32 secondary_bss_load_old; /* Cached Secondary interface load */
	uint8 dyn_algo_counter;        /* Counter used ro wait for specified time */
	uint8 saved_bw;                /* Cached BandWidth */
	bool slow_upd_phase;           /* Flag to indicate slow update phase */
	bool bw_init_flag;             /* Flag to inidicate bw initialied to 50% */
	bool anomaly;                  /* Flag to indicate when both the interface load is */
				       /* increasing or decreasing at the same time */
	bool last_inc_is_primary;      /* Flag to indicate the bw increment */
				       /* was done to Primary Interface */
	uint8 modesw_sample_duration;  /* duration during which modesw req is seen */
	bool bw_init_oneif_tp;         /* When traffic is running only on one interface */
	/* qos info params for deciding on mode of operation */
	void *qos_info;
	/* Amount of Voice traffic in bytes above which we decide
	 * to switch to VSDB this is part of the QoS based ASDB
	 */
	uint32 VO_traffic_threshold;
	/* Amount of Video traffic in bytes above which we decide
	 * to switch to VSDB this is part of the QoS based ASDB
	 */
	uint32 VI_traffic_threshold;
} mchan_dynalgo_params_info_t;

/* local structure defines */
/** contains mchan algo related info */
typedef struct {
	mchan_info_t *mchan;
	uint32 curr_trig_tbtt;          /* current AP tbtt time */
	uint32 curr_oth_tbtt;           /* curr sta tbtt time */
	uint32 next_trig_tbtt;          /* next AP tbtt ime */
	uint32 next_oth_tbtt;           /* next sta tbtt time */
	uint32 trig_bcn_per;            /* trigger beacon interval */
	uint32 oth_bcn_per;             /* other beacon interval */
	uint32 tbtt_gap;                /* time gap between sta and AP beacons */
	uint32 proc_time;               /* Processing time during channel switch */
	mchan_tsf_adj adj_type;         /* indicates tsf adjustment type */
	uint32 tsf_h;                   /* higher order 32 bits of tsf */
	uint32 tsf_l;                   /* lower order 32 bits of tsf */
} wlc_mchan_algo_info_t;

/** mchan module specific state */
struct mchan_info {
	wlc_info_t *wlc;			/* wlc structure */
	int8 onchan_cfg_idx;			/* on channel bsscfg index */
	int8 trigger_cfg_idx;			/* bsscfg idx used for scheduling */
	int8 other_cfg_idx;			/* bsscfg idx used for scheduling
						 * XXX: this assumes we have at most
						 * 2 active bsscfg's
						 */
	uint32 tbtt_gap;			/* saved tbtt gap info between trig and other */
	uint16 pretbtt_time_us;			/* pretbtt time in uS */
	uint16 proc_time_us;			/* processing time for mchan in uS
						 * includes channel switch time and
						 * pm transition time
						 */
	uint16 chan_switch_time_us;		/* channel switch time in uS */
	bool disable_stago_mchan;		/* disable sta go multi-channel operation */
	uint8 tbtt_since_bcn_thresh;		/* threshold for num tbtt w/o beacon */
	wlc_bsscfg_t *blocking_bsscfg;		/* bsscfg that is blocking channel switch */
	bool blocking_enab;
	bool bypass_pm;				/* whether we wake sta when returing to
						 * sta's channel.
						 */
	bool bypass_dtim;			/* whether we ignore dtim rules when
						 * performing channel switches.
						 */
	wlc_mchan_sched_mode_t sched_mode;	/* schedule mode: fair,
	                                         * favor sta, favor p2p
						 */
	uint32 min_dur;				/* minimum duration to stay in a channel */

	uint32 switch_interval;
	uint32 switch_algo;
	uint32 percentage_bw;
	uint32 bcn_position;
	uint8 trigg_algo;         /* trigger algo's only if there is a change in parameters */
	bool alternate_switching;
	uint32 si_algo_last_switch_time;
	bool bcnovlpopt;		/* Variable to store ioctl value  */
	bool overlap_algo_switch;	/* whether overlap beacon caused a switch to SI algo. */
	uint32 abs_cnt;		/* The number of abscences within a given beacon int */
	uint8 blocking_cnt;
	uint8 tsf_adj_wait_cnt;			/* cnt to wait for after tsf adj */
	uint32 chansw_timestamp;
	int cfgh;	/* bsscfg cubby handle */
	int wait_sta_valid;
	bool multi_if_adjustment; /* to determine if multi interface adjustement required */
	/* N-Channel concurrency parameters */
	bool timer_running; /* shared lock for schduling timers betwwen abs and psc */
	bool adjusted_for_nchan; /* param to inform N Channel active or reset */
	wlc_mchan_algo_info_t *mchan_algo_info; /* mchan algo related info */
	wlc_modesw_cb_ctx *modesw_ctx; /* Mode switch timer context */
	uint32 mchan_err_counter;	/* Error counter for MCHAN */
	mchan_dynalgo_params_info_t *dyn_algo_params;	/* Pointer to vsdb params */
};

/* local macros */
#define MCHAN_SI_ALGO_BCN_LOW_PSC_1 (28 * 1024)	/* Lower limit for GO beacon in 1st psc */
#define MCHAN_SI_ALGO_BCN_LOW_PSC_2 (36 * 1024)	/* Lower limit for GO beacon in 2nd psc */

/*  Optimum pos for GO bcn */
/* - beacon 25 to 50ms - psc  */
#define MCHAN_SI_ALGO_BCN_OPT_PSC_1 (32 * 1024)

#define MAX_WAIT_STA_VALID			10
#define MAX_TBTT_VALID_WAIT			5

#define DYN_LOAD_MARK 40 /* The low load threshold for either interfaces */
#define DYN_ADJ_FACTR 2  /* Adjustment factor to alter BW percentage by */
#define DYN_OPT_LOAD 80  /* Optimum Load to be achieved */
#define DYN_INI_BW 50  /* Initial Bandwidth assigned to each interface */

typedef struct _mchan_stat {
	uint32	cycle_cnt;		/* to calculate the number of switches per interface */
	uint32	cycle_time_min;		/* To calculate minimum time per interface */
	uint32	cycle_time_max;		/* To calculate maximum time per interface */
	uint32	cycle_time_cumul;	/* To calculate cumulative time of both interfaces */
} mchan_stat_t;

typedef struct _mchan_airtime_stats {
	cca_ucode_counts_t if_count;			/* Hold stats from ucode */
	uint32 *time;		/* Actual air time */
	uint32 *period;		/* Allocated time */
	uint32 rts;		/* Cached RTS */
	uint32 sample;
} mchan_airtime_stats_t;

typedef struct _mchan_if_bytes {
	uint32 *txbytes;		/* Total Number of TX bytes per if */
	uint32 *rxbytes;		/* Total Number of RX bytes per if */
	uint32 tot_txbytes;		/* Cached TX bytes */
	uint32 tot_rxbytes;		/* Cached RX bytes */
	uint8 sample;			/* sample Index */
} mchan_if_bytes_t;

/** per BSS data */
typedef struct {
	uint32 start_tsf;			/* msch start time */
	chanspec_t chanspec;			/* chanspec specified when starting GO */
	uint16 sw_dtim_cnt;			/* dtim cnt kept upto date by sw */
	bool in_psc;				/* currently in psc period */
	uint8 mchan_tbtt_since_bcn;		/* number of tbtt since last bcn */
	uint32 duration;                        /* msch interval */
	uint32 interval;                        /* msch interval */
	int time_gap;
	bool clone_pending;
	bool modesw_pending;
	/* track whether this interface was the one with max load req */
	bool cfg_req_modesw;
	mchan_airtime_stats_t *air_stats;	/* airtime stats for load acalculation */
	mchan_if_bytes_t *bytes_stats;		/* byte counts stats for load calculation */
	uint32 load;				/* indicates current acttivity load */
	mchan_stat_t	*timing_stats;		/* per bss stats */
} mchan_bss_info_t;

/* locate per BSS data */
#define MCHAN_BSS_INFO(mchan, cfg) (mchan_bss_info_t *)BSSCFG_CUBBY((cfg), (mchan)->cfgh)

/* macros */
#define WLC_MCHAN_SAME_CTLCHAN(c1, c2) (wf_chspec_ctlchan(c1) == wf_chspec_ctlchan(c2))

/* local prototypes */
static int wlc_mchan_doiovar(void *hdl, uint32 actionid,
                             void *p, uint plen, void *a,
                             uint alen, uint vsize, struct wlc_if *wlcif);
static void wlc_mchan_watchdog(void *context);
static int wlc_mchan_up(void *context);
static int wlc_mchan_down(void *context);
static void wlc_mchan_enab(mchan_info_t *mchan, bool enable);
#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static int wlc_mchan_dump(void *context, struct bcmstrbuf *b);
#endif // endif

static int wlc_mchan_bsscfg_up(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
static int wlc_mchan_bsscfg_down(wlc_info_t *wlc, wlc_bsscfg_t *cfg);

static void wlc_mchan_sched_add(mchan_info_t *mchan, wlc_bsscfg_t *bsscfg, uint32 duration,
	uint32 interval, uint32 start_tsf);
static void wlc_mchan_switch_chan_context(mchan_info_t *mchan, wlc_bsscfg_t *target_cfg,
	wlc_bsscfg_t *other_cfg);
static void wlc_mchan_multichannel_upd(wlc_info_t *wlc, mchan_info_t *mchan);

static int8 wlc_mchan_set_other_cfg_idx(mchan_info_t *mchan, wlc_bsscfg_t *del_cfg);
static void wlc_mchan_swap_cfg_idx(mchan_info_t *mchan);
static wlc_bsscfg_t *wlc_mchan_get_other_cfg_frm_cur(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);
static void wlc_mchan_correction_update(mchan_info_t *mchan, wlc_bsscfg_t *bsscfg);
static void wlc_mchan_return_home_channel(mchan_info_t *mchan, wlc_bsscfg_t *bsscfg);
#if defined(WLRSDB) && defined(WL_MODESW)
static int
wlc_mchan_get_cfg_req_modesw(mchan_info_t* mchan, wlc_bsscfg_t *bsscfg, bool* val);
static void
wlc_mchan_set_cfg_req_modesw(mchan_info_t* mchan, wlc_bsscfg_t *bsscfg, bool setflag);
static void wlc_mchan_off_channel_done(mchan_info_t *mchan, wlc_bsscfg_t *bsscfg);
static bool
wlc_mchan_is_qos_traffic_present(mchan_info_t *mchan);
static void wlc_mchan_dyn_modesw_init_params(mchan_info_t *mchan, bool bw_init);
#ifdef WLCNT
static void
wlc_mchan_dyn_algo_update_wme_cntrs(mchan_info_t *mchan);
#endif /* WLCNT */

#endif /* WLRSDB && WL_MODESW */
static void wlc_mchan_calc_fair_chan_switch_sched(uint32 next_tbtt1,
                                                  uint32 curr_tbtt2,
                                                  uint32 next_tbtt2,
                                                  uint32 bcn_per,
                                                  bool tx,
                                                  uint32 *start_time1,
                                                  uint32 *duration1);
static int wlc_mchan_noa_setup(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint32 bcn_per,
                               uint32 curr_ap_tbtt, uint32 next_ap_tbtt,
                               uint32 next_sta_tbtt, uint32 tbtt_gap, bool create);
static int wlc_mchan_noa_setup_multiswitch(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	uint32 interval, uint32 tbtt_gap, uint32 sel_dur, uint32 startswitch_for_abs,
	uint8 cnt);
static int wlc_mchan_ap_pretbtt_proc(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
                                     uint32 tsf_l, uint32 tsf_h);
static int wlc_mchan_sta_pretbtt_proc(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
                                      uint32 tsf_l, uint32 tsf_h);

static bool wlc_mchan_client_noa_valid(mchan_info_t * mchan);
static bool wlc_mchan_algo_change_allowed(mchan_info_t *mchan, int algo);
static int wlc_mchan_sta_default_algo(mchan_info_t *mchan);
static int wlc_mchan_sta_bandwidth_algo(mchan_info_t *mchan);
static int wlc_mchan_sta_si_algo(mchan_info_t *mchan);
static int wlc_mchan_alternate_switching(mchan_info_t *mchan, uint32 trigger_bw);
static int wlc_mchan_algo_selection(mchan_info_t *mchan);
static void wlc_mchan_get_pretbtts(mchan_info_t *mchan);
static int wlc_mchan_go_si_algo(mchan_info_t *mchan);
static int wlc_mchan_go_bandwidth_algo(mchan_info_t *mchan);
static int wlc_mchan_tbtt_gap_adj(mchan_info_t *mchan);
static int wlc_mchan_tsf_adj(mchan_info_t *mchan, int32 tsf_adj);
static void wlc_mchan_disassoc_clbk(void *ctx, bss_disassoc_notif_data_t *notif_data);

static void wlc_mchan_assoc_state_clbk(void *arg,
	bss_assoc_state_data_t *notif_data);
static void wlc_mchan_bsscfg_up_down(void *ctx, bsscfg_up_down_event_data_t *evt);
static int wlc_mchan_if_time(wlc_info_t *wlc, wlc_bsscfg_t *curr_cfg, wlc_bsscfg_t *other_cfg);
static void wlc_mchan_psc_cleanup(mchan_info_t *mchan, wlc_bsscfg_t *cfg);
static void wlc_mchan_set_adjusted_for_nchan(mchan_info_t *mchan);
static void wlc_mchan_pretbtt_cb(void *ctx, wlc_mcnx_intr_data_t *notif_data);
static void _wlc_mchan_pretbtt_cb(void *ctx, wlc_mcnx_intr_data_t *notif_data);
static uint16 wlc_mchan_pretbtt_time_us(mchan_info_t *mchan);
static int wlc_mchan_dyn_algo(mchan_info_t *mchan);
static int wlc_mchan_if_byte(wlc_info_t *wlc, wlc_bsscfg_t *curr_cfg, wlc_bsscfg_t *other_cfg);
static int
wlc_mchan_vsdb_dyn_algo(mchan_info_t *mchan);
static int
wlc_mchan_bss_init(void *context, wlc_bsscfg_t *cfg);
static void
wlc_mchan_bss_deinit(void *context, wlc_bsscfg_t *cfg);
static void
wlc_mchan_update_stats(wlc_info_t *wlc, wlc_bsscfg_t *on_chan_cfg);

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static void wlc_mchan_bss_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b);
#else
#define wlc_mchan_bss_dump NULL
#endif // endif
static bool wlc_mchan_load_inc_or_dec(mchan_info_t *mchan, uint32 primary_bss_load,
	uint32 secondary_bss_load);
static void wlc_mchan_inc_bw(mchan_info_t *mchan, uint8 bw_inc_type, bool inc_primary);
static void wlc_mchan_set_dyn_algo_params(mchan_info_t *mchan, bool bw_init,
	bool bw_one_if_init, bool slowupd, uint8 algo_cntr);
#ifdef WLRSDB
static void wlc_mchan_rsdb_dyn_algo(wlc_info_t *wlc);
#endif // endif
static mchan_dyn_algo_action
wlc_mchan_check_sec_if(mchan_info_t *mchan, uint32 primary_bss_load,
	uint32 secondary_bss_load, bool* act_on_primary);
static mchan_dyn_algo_action
wlc_mchan_check_primary_if(mchan_info_t *mchan, uint32 primary_bss_load,
	uint32 secondary_bss_load, bool* act_on_primary);
static mchan_dyn_algo_action wlc_vsdb_dyn_algo_get_action(mchan_info_t *mchan,
	bool *act_on_primary, uint32 prim_bss_load, uint32 sec_bss_load);

static int wlc_mchan_parse_tim_ie(void *ctx, wlc_iem_parse_data_t *data);
#ifdef WLRSDB
static int
wlc_mchan_bsscfg_config_set(void *hdl, wlc_bsscfg_t *bsscfg, const uint8 *data, int len);
static int
wlc_mchan_bsscfg_config_get(void *hdl, wlc_bsscfg_t *bsscfg, uint8 *data, int *len);
#endif /* WLRSDB */

static uint32 mchan_test_var = 0x0401;

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

#if defined(WLRSDB) && defined(WL_MODESW)
static void wlc_mchan_opermode_change_cb(void *ctx, wlc_modesw_notif_cb_data_t *notif_data);
static void wlc_mchan_clone_timer_cb(void *arg);
#endif // endif

static bool
wlc_mchan_client_noa_valid(mchan_info_t * mchan)
{
	wlc_info_t *wlc = mchan->wlc;
	int idx = 0;
	wlc_bsscfg_t *cfg = NULL;
	bool ret = FALSE;

	if (!(MCHAN_ENAB(wlc->pub) && MCHAN_ACTIVE(wlc->pub))) {
		return FALSE;
	}
	if (BSSCFG_AP(WLC_BSSCFG(wlc, mchan->trigger_cfg_idx)))
		return FALSE;

	FOREACH_AS_STA(wlc, idx, cfg) {
		if (!BSS_P2P_ENAB(wlc, cfg))
			continue;
		if (wlc_p2p_noa_valid(wlc->p2p, cfg)) {
			ret = TRUE;
			break;
		} else
			continue;
	}
	return ret;
}

/** get value for mchan stago feature */
bool wlc_mchan_stago_is_disabled(mchan_info_t *mchan)
{
	return (mchan->disable_stago_mchan);
}

bool wlc_mchan_bypass_pm(mchan_info_t *mchan)
{
	return (mchan->bypass_pm);
}

int8
wlc_mchan_curr_idx(mchan_info_t *mchan)
{
	return mchan->onchan_cfg_idx;
}

/** module attach/detach */
mchan_info_t *
BCMATTACHFN(wlc_mchan_attach)(wlc_info_t *wlc)
{
	mchan_info_t *mchan;
	bsscfg_cubby_params_t cubby_params;

	/* sanity check */

	/* module states */
	if ((mchan = (mchan_info_t *)MALLOCZ(wlc->osh, sizeof(mchan_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}

	/* setup tbtt_since_bcn threshold */
	mchan->tbtt_since_bcn_thresh = MCHAN_TBTT_SINCE_BCN_THRESHOLD;
	/* enable chansw blocking */
	mchan->blocking_enab = TRUE;
	/* invalidate the cfg idx */
	mchan->onchan_cfg_idx = MCHAN_CFG_IDX_INVALID;
	mchan->trigger_cfg_idx = MCHAN_CFG_IDX_INVALID;
	mchan->other_cfg_idx = MCHAN_CFG_IDX_INVALID;

	mchan->wlc = wlc;

	if ((mchan->mchan_algo_info = (wlc_mchan_algo_info_t*)MALLOC(wlc->osh,
		sizeof(wlc_mchan_algo_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}

	/* initialize pretbtt time */
	mchan->pretbtt_time_us = WLC_MCHAN_PRETBTT_TIME_US(mchan);
	/* pretbtt_time_us must be integer times of (1<<P2P_UCODE_TIME_SHIFT)us */
	mchan->pretbtt_time_us = MCHAN_TIMING_ALIGN(mchan->pretbtt_time_us);

	/* initialize mchan proc time */
	if ((wlc->pub->sih->boardvendor == VENDOR_APPLE && wlc->pub->sih->boardtype == 0x0093)) {
		mchan->proc_time_us = WLC_MCHAN_PROC_TIME_LONG_US(mchan);
	}
	else {
		mchan->proc_time_us = WLC_MCHAN_PROC_TIME_US(mchan->wlc);
	}

	/* initialize mchan channel switch time */
	mchan->chan_switch_time_us = WLC_MCHAN_CHAN_SWITCH_TIME_US(mchan->wlc);

	/* schedule mode is fair by default */
	mchan->sched_mode = WLC_MCHAN_SCHED_MODE_FAIR;
	mchan->min_dur = MCHAN_MIN_DUR_US;

#ifdef STA
	if (wlc_iem_add_parse_fn(wlc->iemi, FC_BEACON, DOT11_MNG_TIM_ID,
	                         wlc_mchan_parse_tim_ie, mchan) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_parse_fn failed, tim ie in bcn\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#endif /* STA */

#if defined(WL_MODESW) && defined(WLRSDB)
	if (WLC_MODESW_ENAB(wlc->pub) && RSDB_ENAB(wlc->pub)) {
		if (wlc_modesw_notif_cb_register(wlc->modesw,
			wlc_mchan_opermode_change_cb, wlc) != BCME_OK) {
			WL_ERROR(("wl%d: %s: wlc_modesw_notif_cb_register failed! \n",
				wlc->pub->unit, __FUNCTION__));
			goto fail;
		}

	}
#endif /* WLRSDB && WL_MODESW */
	bzero(&cubby_params, sizeof(cubby_params));
	cubby_params.context = mchan;
	cubby_params.fn_init = wlc_mchan_bss_init;
	cubby_params.fn_deinit = wlc_mchan_bss_deinit;
#if defined(BCMDBG) || defined(BCMDBG_DUMP)
	cubby_params.fn_dump = wlc_mchan_bss_dump;
#endif // endif
#ifdef WLRSDB
	cubby_params.fn_get = wlc_mchan_bsscfg_config_get;
	cubby_params.fn_set = wlc_mchan_bsscfg_config_set;
	cubby_params.config_size = sizeof(chanspec_t);
#endif /* WLRSDB */

	/* reserve cubby in the bsscfg container for per-bsscfg private data */
	mchan->cfgh = wlc_bsscfg_cubby_reserve_ext(wlc, sizeof(mchan_bss_info_t), &cubby_params);
	if (mchan->cfgh < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	if (wlc_module_register(wlc->pub, mchan_iovars, "mchan", mchan, wlc_mchan_doiovar,
		wlc_mchan_watchdog, wlc_mchan_up, wlc_mchan_down)) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
	wlc_dump_register(wlc->pub, "mchan", wlc_mchan_dump, (void *)mchan);
#endif // endif

#if defined(WLRSDB) && defined(WL_MODESW)
	if (wlc == WLC_RSDB_GET_PRIMARY_WLC(wlc)) {
		if (!(mchan->modesw_ctx = MALLOCZ(wlc->osh, sizeof(wlc_modesw_cb_ctx)))) {
			WL_ERROR(("wl%d: %s: Out of memory, allocated %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			goto fail;
		}
	}

	if (wlc == WLC_RSDB_GET_PRIMARY_WLC(wlc)) {
		if (!(wlc->cmn->mchan_clone_timer = wl_init_timer(wlc->wl,
			wlc_mchan_clone_timer_cb, mchan->modesw_ctx, "mchan_clone_timer"))) {
			WL_ERROR(("wl%d: %s mchan_clone_timer failed\n",
				wlc->pub->unit, __FUNCTION__));
			goto fail;
		}
	}
#endif /* WLRSDB && WL_MODEsW */

	/* register preTBTT callback */
	if ((wlc_mcnx_intr_register(wlc->mcnx, wlc_mchan_pretbtt_cb, wlc)) != BCME_OK) {
		WL_ERROR(("wl%d: wlc_mcnx_intr_register failed (mchan)\n",
		          wlc->pub->unit));
		goto fail;
	}
	if ((mchan->dyn_algo_params = (mchan_dynalgo_params_info_t*)MALLOCZ(wlc->osh,
		sizeof(mchan_dynalgo_params_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	/* enable multi channel */
	wlc_mchan_enab(mchan, TRUE);
	mchan->switch_interval = 50 * 1024;
	mchan->switch_algo = MCHAN_DEFAULT_ALGO;
	mchan->percentage_bw = MCHAN_DEFAULT_BW;

	if (wlc_bss_disassoc_notif_register(wlc, wlc_mchan_disassoc_clbk, mchan)
		!= BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bss_disassoc_notif_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	if (wlc_bss_assoc_state_register(wlc, wlc_mchan_assoc_state_clbk, mchan)
		!= BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bss_assoc_state_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	if (wlc_bsscfg_updown_register(wlc, wlc_mchan_bsscfg_up_down, mchan) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_state_upd_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#if defined(WLRSDB)&& defined(WL_MODESW) && defined(WLCNT)
	if ((mchan->dyn_algo_params->qos_info = MALLOCZ(wlc->osh,
		(MAX_RSDB_MAC_NUM * sizeof(mchan_qos_modesw_info_t)))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
#endif /* WLRSDB && WL_MODESW && WLCNT */

#if defined(WLRSDB)&& defined(WL_MODESW) && defined(WLCNT)
	/* the following values have been derived by assuming that the major use case as Skype */
	/* Minimum BW that the voice call will take is 30Kbps */
	/* Minimum BW that the Video chat will take is 128Kbps */
	mchan->dyn_algo_params->VO_traffic_threshold = MCHAN_VO_DEFAULT_BYTE_THRES;
	mchan->dyn_algo_params->VI_traffic_threshold = MCHAN_VI_DEFAULT_BYTE_THRES;
#endif /* WLRSDB && WL_MODESW && WLCNT */

	return mchan;

fail:
	/* error handling */
	MODULE_DETACH(mchan, wlc_mchan_detach);

	return NULL;
}

/* bsscfg cubby */
static int
wlc_mchan_bss_init(void *context, wlc_bsscfg_t *cfg)
{
	mchan_info_t *mchan = (mchan_info_t *)context;
	wlc_info_t *wlc = mchan->wlc;
	mchan_bss_info_t *bmi = MCHAN_BSS_INFO(mchan, cfg);
	int err;

	if (!(BSSCFG_INFRA_STA(cfg) || P2P_GO(wlc, cfg) || P2P_CLIENT(wlc, cfg))) {
		/* MCHAN is required only for  INFRA_STA or GO or GC, hence returning
		* for other bsscfg types.
		*/
		return BCME_OK;
	}

	/* Initialize air stats per bss */
	bmi->air_stats = (mchan_airtime_stats_t *)MALLOCZ(wlc->osh, sizeof(mchan_airtime_stats_t));
	if (bmi->air_stats == NULL) {
		WL_ERROR(("wl%d.%d: %s: MALLOC(%d) failed, malloced %d bytes\n", WLCWLUNIT(wlc),
			WLC_BSSCFG_IDX(cfg), __FUNCTION__, (int)sizeof(mchan_airtime_stats_t),
			MALLOCED(wlc->osh)));
		err = BCME_NOMEM;
		goto fail;
	}
	bmi->air_stats->time = (uint32 *)MALLOCZ(wlc->osh,
		(sizeof(*bmi->air_stats->time) * MCHAN_DYN_ALGO_SAMPLE));
	if (bmi->air_stats->time == NULL) {
		WL_ERROR(("wl%d.%d: %s: MALLOC(%d) failed, malloced %d bytes\n", WLCWLUNIT(wlc),
			WLC_BSSCFG_IDX(cfg), __FUNCTION__,
			(int)(sizeof(*bmi->air_stats->time) * MCHAN_DYN_ALGO_SAMPLE),
			MALLOCED(wlc->osh)));
		err = BCME_NOMEM;
		goto fail;
	}
	bmi->air_stats->period = (uint32 *)MALLOCZ(wlc->osh,
		(sizeof(*bmi->air_stats->period) * MCHAN_DYN_ALGO_SAMPLE));
	if (bmi->air_stats->period == NULL) {
		WL_ERROR(("wl%d.%d: %s: MALLOC(%d) failed, malloced %d bytes\n", WLCWLUNIT(wlc),
			WLC_BSSCFG_IDX(cfg), __FUNCTION__,
			(int)(sizeof(*bmi->air_stats->period) * MCHAN_DYN_ALGO_SAMPLE),
			MALLOCED(wlc->osh)));
		err = BCME_NOMEM;
		goto fail;
	}
	/* Initialize debug timing stats per bss */
	bmi->timing_stats = (mchan_stat_t *)MALLOCZ(wlc->osh, sizeof(mchan_stat_t));
	if (bmi->timing_stats == NULL) {
		WL_ERROR(("wl%d.%d: %s: MALLOC(%d) failed, malloced %d bytes\n", WLCWLUNIT(wlc),
			WLC_BSSCFG_IDX(cfg), __FUNCTION__, (int)sizeof(mchan_stat_t),
			MALLOCED(wlc->osh)));
		err = BCME_NOMEM;
		goto fail;
	}
	bmi->bytes_stats = (mchan_if_bytes_t *)MALLOCZ(wlc->osh, sizeof(mchan_if_bytes_t));
	if (bmi->bytes_stats == NULL) {
		WL_ERROR(("wl%d.%d: %s: MALLOC(%d) failed, malloced %d bytes\n", WLCWLUNIT(wlc),
			WLC_BSSCFG_IDX(cfg), __FUNCTION__, (int)sizeof(mchan_if_bytes_t),
			MALLOCED(wlc->osh)));
		err = BCME_NOMEM;
		goto fail;
	}
	bmi->bytes_stats->txbytes = (uint32 *)MALLOCZ(wlc->osh,
		(sizeof(uint32) * MCHAN_DYN_ALGO_SAMPLE));
	if (bmi->bytes_stats->txbytes == NULL) {
		WL_ERROR(("wl%d.%d: %s: MALLOC(%d) failed, malloced %d bytes\n", WLCWLUNIT(wlc),
			WLC_BSSCFG_IDX(cfg), __FUNCTION__,
			(int)(sizeof(uint32) * MCHAN_DYN_ALGO_SAMPLE), MALLOCED(wlc->osh)));
		err = BCME_NOMEM;
		goto fail;
	}
	bmi->bytes_stats->rxbytes = (uint32 *)MALLOCZ(wlc->osh,
		(sizeof(uint32) * MCHAN_DYN_ALGO_SAMPLE));
	if (bmi->bytes_stats->rxbytes == NULL) {
		WL_ERROR(("wl%d.%d: %s: MALLOC(%d) failed, malloced %d bytes\n", WLCWLUNIT(wlc),
			WLC_BSSCFG_IDX(cfg), __FUNCTION__,
			(int)(sizeof(uint32) * MCHAN_DYN_ALGO_SAMPLE), MALLOCED(wlc->osh)));
		err = BCME_NOMEM;
		goto fail;
	}
	return BCME_OK;

fail:
	wlc_mchan_bss_deinit(context, cfg);
	return err;
}

static void
wlc_mchan_bss_deinit(void *context, wlc_bsscfg_t *cfg)
{
	mchan_info_t *mchan = (mchan_info_t *)context;
	wlc_info_t *wlc = mchan->wlc;
	mchan_bss_info_t *bmi = MCHAN_BSS_INFO(mchan, cfg);

	if (bmi == NULL)
		return;
	if (bmi->air_stats != NULL) {
		if (bmi->air_stats->time != NULL) {
			MFREE(wlc->osh, bmi->air_stats->time, (sizeof(*bmi->air_stats->time)
				* MCHAN_DYN_ALGO_SAMPLE));
		}
		if (bmi->air_stats->period != NULL) {
			MFREE(wlc->osh, bmi->air_stats->period, (sizeof(*bmi->air_stats->period)
				* MCHAN_DYN_ALGO_SAMPLE));
		}
		MFREE(wlc->osh, bmi->air_stats, sizeof(*bmi->air_stats));
	}

	if (bmi->timing_stats != NULL) {
		MFREE(wlc->osh, bmi->timing_stats, sizeof(*bmi->timing_stats));
	}
	if (bmi->bytes_stats != NULL) {
		if (bmi->bytes_stats->txbytes != NULL) {
			MFREE(wlc->osh, bmi->bytes_stats->txbytes,
				(sizeof(*bmi->bytes_stats->txbytes) * MCHAN_DYN_ALGO_SAMPLE));
		}
		if (bmi->bytes_stats->rxbytes != NULL) {
			MFREE(wlc->osh, bmi->bytes_stats->rxbytes,
				(sizeof(*bmi->bytes_stats->rxbytes) * MCHAN_DYN_ALGO_SAMPLE));
		}
		MFREE(wlc->osh, bmi->bytes_stats, sizeof(*bmi->bytes_stats));
	}
}

static int wlc_mchan_dyn_algo(mchan_info_t *mchan)
{
	wlc_info_t *wlc = mchan->wlc;
	BCM_REFERENCE(wlc);

	if (WLC_DUALMAC_RSDB(wlc->cmn))
		return BCME_EPERM;
	/* No use dynamic algo if both RSDB and VSDB is not active */
	ASSERT(RSDB_ACTIVE(wlc->pub) || MCHAN_ACTIVE(wlc->pub));
	if (!RSDB_ACTIVE(wlc->pub) && !MCHAN_ACTIVE(wlc->pub)) {
		return BCME_EPERM;
	}

#if defined(WLRSDB) && defined(WL_MODESW) && defined(WLCNT)
	if (WLC_MODESW_ENAB(wlc->pub) && WME_ENAB(wlc->pub)) {
		wlc_mchan_dyn_algo_update_wme_cntrs(wlc->mchan);
	}
#endif /* WLRSDB && WL_MODESW && WLCNT */

	/* Find avg values of data collected over last WLC_MCHAN_DYN_ALGO_SAMPLE samples */
	if (MCHAN_ACTIVE(wlc->pub)) {
		if (wlc_mchan_vsdb_dyn_algo(mchan) == BCME_NOTREADY) {
			WL_INFORM(("wl%d: %s: dyn counter not yet elapsed\n",
				mchan->dyn_algo_params->dyn_algo_counter, __FUNCTION__));
		}
	}
#if defined(WLRSDB) && defined(WL_MODESW)
	else if (RSDB_ACTIVE(wlc->pub) && WLC_MODESW_ENAB(wlc->pub)) {
		wlc_mchan_rsdb_dyn_algo(wlc);
	}
#endif /* WLRSDB && WL_MODESW */
	return BCME_OK;
}

void
BCMATTACHFN(wlc_mchan_detach)(mchan_info_t *mchan)
{
	wlc_info_t *wlc;

	if (mchan == NULL) {
		return;
	}

	wlc = mchan->wlc;

	/* disable multi channel */
	wlc_mchan_enab(mchan, FALSE);

#ifdef WLRSDB
	if (wlc == WLC_RSDB_GET_PRIMARY_WLC(wlc)) {
		if (wlc->cmn->mchan_clone_timer) {
			wl_free_timer(wlc->wl, wlc->cmn->mchan_clone_timer);
			wlc->cmn->mchan_clone_timer = NULL;
		}
	}
#endif // endif

	if (mchan->dyn_algo_params) {
#if defined(WLRSDB) && defined(WL_MODESW) && defined(WLCNT)
		if (mchan->dyn_algo_params->qos_info) {
			MFREE(wlc->osh, mchan->dyn_algo_params->qos_info,
				(MAX_RSDB_MAC_NUM * sizeof(mchan_qos_modesw_info_t)));
		}
#endif /* WLRSDB && WL_MODESW && WLCNT */
		/* Free memory used for mchan dynamic algo */
		MFREE(wlc->osh, mchan->dyn_algo_params, sizeof(*mchan->dyn_algo_params));
	}
	/* Free memory allocated to mchan algo info variables */
	if (mchan->mchan_algo_info) {
		MFREE(wlc->osh, mchan->mchan_algo_info, sizeof(wlc_mchan_algo_info_t));
	}

#if defined(WL_MODESW) && defined(WLRSDB)
	if (WLC_MODESW_ENAB(wlc->pub) && RSDB_ENAB(wlc->pub)) {
		wlc_modesw_notif_cb_unregister(wlc->modesw,
			wlc_mchan_opermode_change_cb, wlc);
	}
#endif /* WLRSDB && WL_MODESW */

	(void)wlc_bss_disassoc_notif_unregister(wlc, wlc_mchan_disassoc_clbk, mchan);
	(void)wlc_bss_assoc_state_unregister(wlc, wlc_mchan_assoc_state_clbk, mchan);
	(void)wlc_bsscfg_updown_unregister(wlc, wlc_mchan_bsscfg_up_down, mchan);

	/* unregister callback */
	wlc_mcnx_intr_unregister(wlc->mcnx, wlc_mchan_pretbtt_cb, wlc);

	/* unregister module */
	wlc_module_unregister(wlc->pub, "mchan", mchan);

	MFREE(wlc->osh, mchan, sizeof(mchan_info_t));
}

static void
wlc_mchan_disassoc_clbk(void *ctx, bss_disassoc_notif_data_t *notif_data)
{
	wlc_bsscfg_t *cfg;

	ASSERT(ctx != NULL);
	ASSERT(notif_data != NULL);
	ASSERT(notif_data->cfg != NULL);
	ASSERT(notif_data->cfg->wlc != NULL);

	cfg = notif_data->cfg;

	if (!BSSCFG_STA(cfg) || BSSCFG_SPECIAL(cfg->wlc, cfg) || !cfg->BSS) {
		return;
	}
	if (notif_data->state == DAN_ST_DISASSOC_CMPLT) {
		wlc_mchan_bsscfg_down(cfg->wlc, cfg);
	}
}

static void
wlc_mchan_assoc_state_clbk(void *arg, bss_assoc_state_data_t *notif_data)
{
	wlc_bsscfg_t *cfg;

	ASSERT(notif_data != NULL);

	cfg = notif_data->cfg;
	ASSERT(cfg != NULL);

	if (!BSSCFG_STA(cfg) || BSSCFG_SPECIAL(cfg->wlc, cfg) || !cfg->BSS) {
		return;
	}
	if ((notif_data->state == AS_IDLE) && cfg->associated) {
		wlc_mchan_bsscfg_up(cfg->wlc, cfg);
	}
}

static void
wlc_mchan_bsscfg_up_down(void *ctx, bsscfg_up_down_event_data_t *evt)
{
	wlc_bsscfg_t *cfg = evt->bsscfg;
	ASSERT(cfg != NULL);

	if (BSSCFG_SPECIAL(cfg->wlc, cfg) || !cfg->BSS ||
		BSS_P2P_DISC_ENAB(cfg->wlc, cfg)) {
		return;
	}

	if (BSSCFG_AP(cfg) && evt->up) {
		wlc_mchan_bsscfg_up(cfg->wlc, cfg);
	} else if (!evt->up) {
		wlc_mchan_bsscfg_down(cfg->wlc, cfg);
	}
}

static uint32
read_tsf_low(wlc_info_t *wlc)
{
	bool prev_awake;
	uint32 tsf_l;

	wlc_force_ht(wlc, TRUE, &prev_awake);
	wlc_read_tsf(wlc, &tsf_l, NULL);
	wlc_force_ht(wlc, prev_awake, NULL);

	return tsf_l;
}

static void
wlc_mchan_sched_add(mchan_info_t *mchan, wlc_bsscfg_t *bsscfg, uint32 duration,
	uint32 interval, uint32 start_tsf)
{
	mchan_bss_info_t *mbi = MCHAN_BSS_INFO(mchan, bsscfg);

	WL_MCHAN(("wl%d.%d: %s: start %u, duration %u, interval %u\n",
		mchan->wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), __FUNCTION__,
		start_tsf, duration, interval));

	mbi->start_tsf = start_tsf;
	mbi->duration = duration;
	mbi->interval = interval;
	mbi->time_gap = 0;

	if (BSSCFG_STA(bsscfg))
		wlc_sta_timeslot_update(bsscfg, start_tsf, interval);
	else
		wlc_ap_timeslot_update(bsscfg, start_tsf, interval);
}

static void
wlc_mchan_watchdog(void *context)
{
	mchan_info_t *mchan = (mchan_info_t *)context;
	wlc_info_t *wlc = mchan->wlc;
	int idx;
	wlc_bsscfg_t *cfg;
	uint32 now = R_REG(wlc->osh, D11_TSFTimerLow(wlc));

	if (!MCHAN_ACTIVE(wlc->pub) && RSDB_ACTIVE(wlc->pub)) {
			FOREACH_AS_BSS(wlc, idx, cfg) {
			/* Since in RSDB mode there is no switching
			* between the cfg's, we shall pass the same
			* bsscfg as the current and other_cfg to
			* wlc_mchan_if_time, thereby ensuring the
			* capturing, recording and saving of stats in the
			* same bsscfg's BMI
			*/
			if (wlc_mchan_if_time(wlc, cfg, cfg) != BCME_OK) {
				mchan->mchan_err_counter++;
				WL_ERROR(("wl%d:%d:%s: mchan_if_time failed\n",
					wlc->pub->unit, mchan->mchan_err_counter, __FUNCTION__));
			}
			mchan->chansw_timestamp = now;
		}
	}
	/* If DYN_ALGO is active then invoke wlc_mchan_dyn_algo */
	/* TODO - VSDB has been tested and verified,
	* RSDB functionality is not tested and needs to be revisited
	*/
	if (DYN_ENAB(wlc->mchan) && (MCHAN_ACTIVE(wlc->pub) ||
#ifdef WLRSDB
		(RSDB_ACTIVE(wlc->pub) && (wlc == WLC_RSDB_GET_PRIMARY_WLC(wlc))) ||
#endif // endif
		FALSE)) {
		if (wlc_mchan_dyn_algo(mchan) != BCME_OK) {
			mchan->mchan_err_counter++;
			WL_MCHAN(("wl%d: %d:%s: mchan_dyn_algo failed\n",
				wlc->pub->unit, mchan->mchan_err_counter, __FUNCTION__));
		}
	}

#if defined(STA)
	if (MCHAN_ENAB(wlc->pub) && MCHAN_ACTIVE(wlc->pub) && !AP_ACTIVE(wlc) &&
	    wlc->reset_triggered_pmoff) {
		uint32 end_time = read_tsf_low(wlc) + MSCH_MAX_OFFCB_DUR;
		uint32 bcn_per;
		uint32 interval = 0;

		/* If mchan is active and a reset was triggered,
		 * we need to schedule channel switches manually
		 * to allow every bsscfg to receive their beacons.
		 * During reset, all p2p shm blocks would get reset
		 * so we need the reception of beacons to reprogram
		 * the p2p shm blocks again in order for regular tbtt
		 * based channel switches to continue.
		 */
		FOREACH_AS_STA(wlc, idx, cfg) {
			if (cfg->up) {
				bcn_per = cfg->current_bss->beacon_period << 10;
				bcn_per *= (cfg->current_bss->dtim_period + 1);
				interval += bcn_per;
			}
		}

		FOREACH_AS_STA(wlc, idx, cfg) {
			if (cfg->up) {
				bcn_per = cfg->current_bss->beacon_period << 10;
				bcn_per *= (cfg->current_bss->dtim_period + 1);
				/* schedule a channel switch to rx other beacon */
				wlc_mchan_sched_add(mchan, cfg, bcn_per, interval,
					end_time);
				end_time += bcn_per;
			}
		}
	}
#endif /* STA */
}

static int
wlc_mchan_up(void *context)
{
	mchan_info_t *mchan = (mchan_info_t *)context;
	wlc_info_t *wlc = mchan->wlc;
	UNUSED_PARAMETER(wlc);

	if (!MCHAN_ENAB(wlc->pub))
		return BCME_OK;

	return BCME_OK;
}

static int
wlc_mchan_down(void *context)
{
	mchan_info_t *mchan = (mchan_info_t *)context;
	uint callbacks = 0;
	wlc_info_t* wlc = mchan->wlc;
	UNUSED_PARAMETER(wlc);

#ifdef WLRSDB
	if (wlc == WLC_RSDB_GET_PRIMARY_WLC(wlc)) {
		/* primary wlc, del mchan_clone_timer */
		wl_del_timer(wlc->wl, wlc->cmn->mchan_clone_timer);
	}
#endif /* WLRSDB */
	return (callbacks);
}

/** handle mchan related iovars */
static int
wlc_mchan_doiovar(void *hdl, uint32 actionid,
	void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif)
{
	mchan_info_t *mchan = (mchan_info_t *)hdl;
	wlc_info_t *wlc = mchan->wlc;
	int32 int_val = 0;
	int err = BCME_OK;

	BCM_REFERENCE(alen);
	BCM_REFERENCE(vsize);
	BCM_REFERENCE(wlcif);

	ASSERT(mchan == wlc->mchan);

	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	/* all iovars require p2p being enabled */
	switch (actionid) {
	case IOV_GVAL(IOV_MCHAN):
	case IOV_SVAL(IOV_MCHAN):
		break;
	default:
		if (MCHAN_ENAB(wlc->pub))
			break;

		return BCME_ERROR;
	}

	switch (actionid) {
	case IOV_GVAL(IOV_MCHAN):
		*((uint32*)a) = wlc->pub->_mchan;
		break;
	case IOV_SVAL(IOV_MCHAN):
		wlc_mchan_enab(mchan, int_val != 0);
		break;
	case IOV_GVAL(IOV_MCHAN_STAGO_DISAB):
		*((uint32*)a) = mchan->disable_stago_mchan;
		break;
	case IOV_SVAL(IOV_MCHAN_STAGO_DISAB):
	        mchan->disable_stago_mchan = (int_val != 0);
		break;
	case IOV_GVAL(IOV_MCHAN_PRETBTT):
	        *((uint32*)a) = (uint32)mchan->pretbtt_time_us;
		break;
	case IOV_SVAL(IOV_MCHAN_PRETBTT):
	        mchan->pretbtt_time_us = MCHAN_TIMING_ALIGN(int_val);
		break;
	case IOV_GVAL(IOV_MCHAN_PROC_TIME):
	        *((uint32*)a) = (uint32)mchan->proc_time_us;
		break;
	case IOV_SVAL(IOV_MCHAN_PROC_TIME): {
		uint32 prev_proc_time = mchan->proc_time_us;

	        mchan->proc_time_us = (uint16)int_val;
		if (mchan->proc_time_us != prev_proc_time) {
			/* clear the saved tbtt_gap value to force go to recreate noa sched */
			mchan->tbtt_gap = 0;
		}
		break;
	}
	case IOV_GVAL(IOV_MCHAN_CHAN_SWITCH_TIME):
		*((uint32*)a) = (uint32)mchan->chan_switch_time_us;
		break;
	case IOV_SVAL(IOV_MCHAN_CHAN_SWITCH_TIME): {
		uint32 prev_chan_switch_time = mchan->chan_switch_time_us;

		mchan->chan_switch_time_us = (uint16)int_val;
		if (mchan->chan_switch_time_us != prev_chan_switch_time) {
			/* clear the saved tbtt_gap value to force go to recreate noa sched */
			mchan->tbtt_gap = 0;
		}
		break;
	}
	case IOV_GVAL(IOV_MCHAN_BLOCKING_ENAB):
	        *((uint32*)a) = (uint32)mchan->blocking_enab;
	        break;
	case IOV_SVAL(IOV_MCHAN_BLOCKING_ENAB):
	        mchan->blocking_enab = (int_val != 0);
	        break;
	case IOV_GVAL(IOV_MCHAN_BLOCKING_THRESH):
	        *((uint32*)a) = (uint32)mchan->tbtt_since_bcn_thresh;
	        break;
	case IOV_SVAL(IOV_MCHAN_BLOCKING_THRESH):
	        mchan->tbtt_since_bcn_thresh = (uint8)int_val;
	        break;
	case IOV_GVAL(IOV_MCHAN_BYPASS_PM):
	        *((uint32*)a) = (uint32)mchan->bypass_pm;
	        break;
	case IOV_SVAL(IOV_MCHAN_BYPASS_PM):
	        mchan->bypass_pm = (int_val != 0);
		break;
	case IOV_GVAL(IOV_MCHAN_BYPASS_DTIM):
		*((uint32*)a) = (uint32)mchan->bypass_dtim;
		break;
	case IOV_SVAL(IOV_MCHAN_BYPASS_DTIM):
		mchan->bypass_dtim = (int_val != 0);
		break;
	case IOV_GVAL(IOV_MCHAN_SCHED_MODE):
		*((uint32*)a) = (uint32)mchan->sched_mode;
		break;
	case IOV_SVAL(IOV_MCHAN_SCHED_MODE): {
		wlc_mchan_sched_mode_t prev_mode = mchan->sched_mode;

		if (int_val >= WLC_MCHAN_SCHED_MODE_MAX) {
			err = BCME_ERROR;
			break;
		}
		mchan->sched_mode = (wlc_mchan_sched_mode_t)int_val;

		if (mchan->sched_mode != prev_mode) {
			/* clear the saved tbtt_gap value to force go to recreate noa sched */
			mchan->tbtt_gap = 0;
		}
		break;
	}
	case IOV_GVAL(IOV_MCHAN_MIN_DUR):
		*((uint32*)a) = (uint32)mchan->min_dur;
		break;
	case IOV_SVAL(IOV_MCHAN_MIN_DUR):
		mchan->min_dur = (uint32)int_val;
		break;
	case IOV_GVAL(IOV_MCHAN_TEST):
		*((uint32*)a) = mchan_test_var;
		break;
	case IOV_SVAL(IOV_MCHAN_TEST):
		mchan_test_var = (uint32)int_val;
	        /* code below is used for tx flow control testing */
	        if (mchan_test_var & 0x20) {
			if (WLC_BSSCFG(wlc, mchan_test_var & 0xF) == NULL ||
			    WLC_BSSCFG(wlc, mchan_test_var & 0xF)->associated == 0) {
				break;
			}
			wlc_txflowcontrol_override(wlc,
				WLC_BSSCFG(wlc, mchan_test_var & 0xF)->wlcif->qi,
			        (mchan_test_var & 0x10), TXQ_STOP_FOR_PKT_DRAIN);
		}
		break;
	case IOV_GVAL(IOV_MCHAN_SWTIME):
	        *((uint32*)a) = mchan->switch_interval;
		break;
	case IOV_SVAL(IOV_MCHAN_SWTIME):
		if ((uint32)int_val > (MCHAN_MIN_RX_TX_TIME + WLC_MCHAN_PRETBTT_TIME_US(mchan)) &&
			(int_val < 50000))
			mchan->switch_interval = (uint32)int_val;
		else
			err = BCME_RANGE;
		mchan->trigg_algo = TRUE;
		break;

	case IOV_GVAL(IOV_MCHAN_ALGO):
	        *((uint32*)a) = mchan->switch_algo;
		break;
	case IOV_SVAL(IOV_MCHAN_ALGO):
		if ((int_val >= 0) && (int_val < 4)) {
			if (wlc_mchan_algo_change_allowed(mchan, int_val)) {
				mchan->switch_algo = (uint32)int_val;
				mchan->trigg_algo = TRUE;
			} else {
				err = BCME_BUSY;
			}
		}
		else
			err = BCME_RANGE;
		break;
	case IOV_GVAL(IOV_MCHAN_BW):
		*((uint32 *)a) = mchan->percentage_bw;
		break;
	case IOV_SVAL(IOV_MCHAN_BW):
		if (DYN_ENAB(mchan))
			break;
		if ((int_val >= 0) && (int_val <= 100))
			mchan->percentage_bw = (uint32)int_val;
		else
			err = BCME_RANGE;
		mchan->trigg_algo = TRUE;
		break;
	case IOV_GVAL(IOV_MCHAN_BCNPOS):
		*((uint32 *)a) = mchan->bcn_position;
		break;
	case IOV_GVAL(IOV_MCHAN_BCN_OVLP_OPT):
		*((uint32 *)a) = mchan->bcnovlpopt;
		break;
	case IOV_SVAL(IOV_MCHAN_BCN_OVLP_OPT):
		if (int_val) {
			if (mchan->switch_algo == MCHAN_DEFAULT_ALGO)
				mchan->bcnovlpopt = TRUE;
		}
		else {
			if (mchan->overlap_algo_switch == TRUE) {
				mchan->bcnovlpopt = FALSE;
				mchan->overlap_algo_switch = FALSE;
				mchan->switch_algo = MCHAN_DEFAULT_ALGO;
			}
		}
		break;
	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

static void
wlc_mchan_enab(mchan_info_t *mchan, bool enable)
{
	wlc_info_t *wlc = mchan->wlc;

	wlc->pub->_mchan = enable;
}

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static int
wlc_mchan_dump(void *context, struct bcmstrbuf *b)
{
	mchan_info_t *mchan = (mchan_info_t *)context;
	wlc_info_t *wlc = mchan->wlc;
	uint8 cnt = 0;
	wlc_txq_info_t *qi = wlc->tx_queues;
	int idx;
	wlc_bsscfg_t *cfg;
	char chanbuf[CHANSPEC_STR_LEN];

	bcm_bprintf(b, "\nmchan is %sABLED\n", MCHAN_ENAB(wlc->pub) ? "EN" : "DIS");
	bcm_bprintf(b, "\nmchan is %sACTIVE\n", MCHAN_ACTIVE(wlc->pub) ? "" : "IN");
	bcm_bprintf(b, "\nmchan->trigger_cfg_idx = %d, mchan->other_cfg_idx = %d\n",
	    mchan->trigger_cfg_idx, mchan->other_cfg_idx);
	bcm_bprintf(b, "current_idx = %d\n", mchan->onchan_cfg_idx);
	bcm_bprintf(b, "\ndisable_stago_mchan = %d\n", mchan->disable_stago_mchan);
	bcm_bprintf(b, "blocking_enab = %d, blocking_thresh = %d\n",
	            mchan->blocking_enab, mchan->tbtt_since_bcn_thresh);
	bcm_bprintf(b, "bypass_pm = %d\n", mchan->bypass_pm);
	bcm_bprintf(b, "bypas_dtim = %d\n", mchan->bypass_dtim);
	bcm_bprintf(b, "sched_mode = %s\n",
	            (mchan->sched_mode == WLC_MCHAN_SCHED_MODE_STA) ? "sta" :
	            (mchan->sched_mode == WLC_MCHAN_SCHED_MODE_P2P) ? "p2p" : "fair");
	bcm_bprintf(b, "\nlisting all tx_queues:\n");
	while (qi) {
		bcm_bprintf(b, "queue(%d) = %p, stopped = 0x%x\n",
			cnt++, OSL_OBFUSCATE_BUF(qi), qi->stopped);
		FOREACH_BSS(wlc, idx, cfg) {
			if (cfg->wlcif->qi == qi) {
				char ifname[32];

				strncpy(ifname, wl_ifname(wlc->wl, cfg->wlcif->wlif),
					sizeof(ifname));
				ifname[sizeof(ifname) - 1] = '\0';
				bcm_bprintf(b, "\tbsscfg %d (%s)\n", WLC_BSSCFG_IDX(cfg), ifname);
			}
		}
		qi = qi->next;
	}
	bcm_bprintf(b, "wlc->active_queue = %p\n", OSL_OBFUSCATE_BUF(wlc->active_queue));
	bcm_bprintf(b, "wlc->primary_queue = %p\n", OSL_OBFUSCATE_BUF(wlc->primary_queue));
	bcm_bprintf(b, "wlc->excursion_queue = %p\n", OSL_OBFUSCATE_BUF(wlc->excursion_queue));
	bcm_bprintf(b, "wlc->home_chanspec = %s\n\n",
	    wf_chspec_ntoa(wlc->home_chanspec, chanbuf));

	return 0;
}
#endif /* BCMDBG || BCMDBG_DUMP */

uint16 wlc_mchan_get_pretbtt_time(mchan_info_t *mchan)
{
	wlc_info_t *wlc = mchan->wlc;

	(void)wlc;

	return MCHAN_ENAB(wlc->pub) && MCHAN_ACTIVE(wlc->pub) ? mchan->pretbtt_time_us : 0;
}

static int8
wlc_mchan_set_other_cfg_idx(mchan_info_t *mchan, wlc_bsscfg_t *del_cfg)
{
	wlc_info_t *wlc = mchan->wlc;
	wlc_bsscfg_t *bsscfg, *cfg;
	chanspec_t chanspec;
	int idx;
	int8 cfgidx;

	if (mchan->trigger_cfg_idx == MCHAN_CFG_IDX_INVALID) {
		return (MCHAN_CFG_IDX_INVALID);
	}

	cfgidx = MCHAN_CFG_IDX_INVALID;

	/*
	* MCHAN does not work with AP +STA. Set other_cfg_idx to INVALID to prevent
	* MCHAN from being ACTIVE and resulting in false MCHAN activity
	*/
	if (wlc_ap_count(wlc->ap, FALSE)) {
		/* AP is active other than P2P-GO */
		return cfgidx;
	}

	/* SKIP BSSCFGs having any one of the following cond satisfied :
	*   1. It is trigger_cfg
	*   2. The chan_context is equal to the trigger_cfg's chan_context
	*   3. It is the bsscfg is being removed / deleted
	*
	*   Return the bsscfg's idx which does not satisfy any of the above.
	*/
	bsscfg = WLC_BSSCFG(wlc, mchan->trigger_cfg_idx);
	chanspec = bsscfg->current_bss->chanspec;

	FOREACH_BSS(wlc, idx, cfg) {
		if (!cfg->up) {
			continue;
		}
		if (BSSCFG_SPECIAL(wlc, cfg) || BSS_P2P_DISC_ENAB(wlc, cfg)) {
			continue;
		}
		if (WLC_MCHAN_SAME_CTLCHAN(cfg->current_bss->chanspec, chanspec)) {
			continue;
		}
		if (del_cfg && (cfg == del_cfg)) {
			continue;
		}
		cfgidx = WLC_BSSCFG_IDX(cfg);
		break;
	}
	return (cfgidx);
}

/** Verify and ensure P2P is always trigger_cfg_idx */
static void wlc_mchan_swap_cfg_idx(mchan_info_t *mchan)
{
	wlc_info_t *wlc = mchan->wlc;
	wlc_bsscfg_t *trigger_cfg, *other_cfg;

	if (mchan->trigger_cfg_idx == MCHAN_CFG_IDX_INVALID ||
		mchan->other_cfg_idx == MCHAN_CFG_IDX_INVALID)
		return;

	trigger_cfg = WLC_BSSCFG(wlc, mchan->trigger_cfg_idx);
	other_cfg = WLC_BSSCFG(wlc, mchan->other_cfg_idx);

	if (BSSCFG_AP(trigger_cfg))
		return;

	/* Swap if other_cfg is P2P and Trigger is not P2P */
	if (BSS_P2P_ENAB(wlc, other_cfg) && !BSS_P2P_ENAB(wlc, trigger_cfg))
		SWAP(mchan->other_cfg_idx, mchan->trigger_cfg_idx);
}

static wlc_bsscfg_t *
wlc_mchan_get_other_cfg_frm_cur(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	mchan_info_t *mchan = wlc->mchan;

	if (MCHAN_ACTIVE(wlc->pub)) {
		chanspec_t chanspec = bsscfg->current_bss->chanspec;
		wlc_bsscfg_t *cfg;

		cfg = WLC_BSSCFG(wlc, mchan->trigger_cfg_idx);
		if (WLC_MCHAN_SAME_CTLCHAN(cfg->current_bss->chanspec, chanspec))
			return WLC_BSSCFG(wlc, mchan->other_cfg_idx);

		cfg = WLC_BSSCFG(wlc, mchan->other_cfg_idx);
		if (WLC_MCHAN_SAME_CTLCHAN(cfg->current_bss->chanspec, chanspec))
			return WLC_BSSCFG(wlc, mchan->trigger_cfg_idx);
	}
	return NULL;
}

/**
 * Called on association or disassociation. If operation went from single to multichannel or vice
 * versa, ucode and firmware administration have to be updated.
 */
static void
wlc_mchan_multichannel_upd(wlc_info_t *wlc, mchan_info_t *mchan)
{
	bool oldstate = wlc->pub->_mchan_active;
	wlc_bsscfg_t *cfg;
	int idx;

	wlc->pub->_mchan_active = (mchan->trigger_cfg_idx != MCHAN_CFG_IDX_INVALID &&
		mchan->other_cfg_idx != MCHAN_CFG_IDX_INVALID);

	if (!wlc->pub->_mchan_active) {
		mchan->multi_if_adjustment = FALSE;
	}

	/* need to reset mchan->wait_sta_valid upon update */
	mchan->wait_sta_valid = 0;

	mchan->tbtt_gap = 0;
	mchan->trigg_algo = TRUE;
	mchan->timer_running = FALSE;

	if (!wlc->pub->_mchan_active) {
		wlc_mchan_reset_params(mchan);
		FOREACH_AP(wlc, idx, cfg) {
			if (P2P_GO(wlc, cfg)) {
				/* if mchan noa present, remove it if mchan not active */
				int err;
				/* cancel the mchan noa schedule */
				err = wlc_mchan_noa_setup(wlc, cfg, 0, 0, 0, 0, 0, FALSE);
				BCM_REFERENCE(err);
				WL_MCHAN(("%s: cancel noa schedule, err = %d\n",
					__FUNCTION__, err));
			}
			wlc_ap_mute(wlc, FALSE, cfg, -1);
		}

		mchan->onchan_cfg_idx = MCHAN_CFG_IDX_INVALID;
	}

	FOREACH_AS_BSS(wlc, idx, cfg) {
		wlc_tbtt_ent_init(wlc->tbtt, cfg);
	}

	if (oldstate == wlc->pub->_mchan_active)
		return;

#if defined(WLRSDB) && defined(WL_MODESW)
	if (RSDB_ENAB(wlc->pub) && WLC_MODESW_ENAB(wlc->pub)) {
		if (wlc->pub->_mchan_active && !WLC_DUALMAC_RSDB(wlc->cmn)) {
			/* Should not have MCHAN operating in core 1 */
			ASSERT(wlc == WLC_RSDB_GET_PRIMARY_WLC(wlc));
			if (wlc != WLC_RSDB_GET_PRIMARY_WLC(wlc)) {
				/* TDB : Need to find a way to bail out from here */
				WL_ERROR(("wl%d MCHAN_ACTIVE in Core 1\n", WLCWLUNIT(wlc)));
			}
		}
	}
#endif /* WLRSDB && WL_MODESW */

#ifdef PROP_TXSTATUS
	if (PROP_TXSTATUS_ENAB(wlc->pub)) {
		if (!wlc->pub->_mchan_active &&
			mchan->trigger_cfg_idx != MCHAN_CFG_IDX_INVALID) {
			wlc_bsscfg_t *trig_cfg = WLC_BSSCFG(wlc, mchan->trigger_cfg_idx);
			FOREACH_BSS(wlc, idx, cfg) {
				if (cfg->up && wlc_shared_chanctxt(wlc, cfg, trig_cfg)) {
					wlc_wlfc_mchan_interface_state_update(wlc, cfg,
						WLFC_CTL_TYPE_INTERFACE_OPEN, TRUE);
				}
			}
		}
	}
#endif /* PROP_TXSTATUS */

	wlc_mcnx_tbtt_adj_all(wlc->mcnx, 0, 0);

	wlc_bmac_enable_tbtt(wlc->hw, TBTT_MCHAN_MASK, wlc->pub->_mchan_active? TBTT_MCHAN_MASK:0);

#if defined(STA)
	/* if we bypassed PM, we would like to restore it upon existing operation */
	if (mchan->bypass_pm && !wlc->pub->_mchan_active) {
		FOREACH_AS_STA(wlc, idx, cfg) {
			wlc_pm_st_t *pm = cfg->pm;
			if (!pm->PMenabled) {
				if (pm->PM == PM_FAST) {
					/* Start the PM2 tick timer for the next tick */
					wlc_pm2_sleep_ret_timer_start(cfg, 0);
				}
				else if (pm->PM == PM_MAX) {
					/* reenable ps mode */
					wlc_set_pmstate(cfg, TRUE);
				}
			}
		}
	}
#endif /* STA */
}

static void
wlc_mchan_switch_chan_context(mchan_info_t *mchan, wlc_bsscfg_t *target_cfg,
	wlc_bsscfg_t *other_cfg)
{
	wlc_info_t *wlc = mchan->wlc;
	uint32 start_tsf = read_tsf_low(wlc);
	uint32 interval = (target_cfg->current_bss->beacon_period <<
		MCHAN_TIME_UNIT_1024US);

	WL_MCHAN(("wl%d.%d: %s: swicth chanctx for 0x%x, start tsf %u, interval %u\n",
		wlc->pub->unit, WLC_BSSCFG_IDX(target_cfg), __FUNCTION__,
		target_cfg->current_bss->chanspec, start_tsf, interval));

	wlc_mchan_sched_add(mchan, target_cfg, interval + MSCH_MIN_FREE_SLOT, 2 * interval,
		start_tsf);
	wlc_mchan_sched_add(mchan, other_cfg, interval - MSCH_MIN_FREE_SLOT, 2 * interval,
		start_tsf + interval + MSCH_MIN_FREE_SLOT);
}

/** when bsscfg becomes associated, call this function */
static int
wlc_mchan_bsscfg_up(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	mchan_info_t *mchan = wlc->mchan;
	mchan_bss_info_t *mbi = MCHAN_BSS_INFO(mchan, cfg);
	DBGONLY(char chanbuf[CHANSPEC_STR_LEN]; )

	WL_MCHAN(("wl%d.%d: %s: chanspec %s, called on %s\n",
		wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
		wf_chspec_ntoa(cfg->current_bss->chanspec, chanbuf),
		BSSCFG_AP(cfg) ? "AP" : "STA"));

	if (BSSCFG_SPECIAL(wlc, cfg))
		return (BCME_ERROR);

	mbi->start_tsf = 0;
	mbi->duration = 0;
	mbi->interval = 0;

	/* select trigger idx */
	if (BSSCFG_AP(cfg)) {
		mchan->trigger_cfg_idx = WLC_BSSCFG_IDX(cfg);
	}
	else if ((mchan->trigger_cfg_idx == MCHAN_CFG_IDX_INVALID) ||
		(WLC_BSSCFG(wlc, mchan->trigger_cfg_idx) &&
		(!BSSCFG_AP(WLC_BSSCFG(wlc, mchan->trigger_cfg_idx))) &&
		(cfg->current_bss->beacon_period <
		WLC_BSSCFG(wlc, mchan->trigger_cfg_idx)->current_bss->beacon_period))) {
		mchan->trigger_cfg_idx = WLC_BSSCFG_IDX(cfg);
	}

	/* set the other idx */
	mchan->other_cfg_idx = wlc_mchan_set_other_cfg_idx(mchan, NULL);
	wlc_mchan_swap_cfg_idx(mchan);

	/* update mchan active status */
	wlc_mchan_multichannel_upd(wlc, mchan);

	return (BCME_OK);
}

/** Everytime bsscfg becomes disassociated, call this function */
static int
wlc_mchan_bsscfg_down(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	mchan_info_t *mchan = wlc->mchan;
	bool oldstate = wlc->pub->_mchan_active;
	chanspec_t new_chanspec = INVCHANSPEC;
	wlc_bsscfg_t *other_bsscfg = wlc_mchan_get_other_cfg_frm_cur(wlc, cfg);
	DBGONLY(char chanbuf[CHANSPEC_STR_LEN]; )

	WL_MCHAN(("wl%d.%d: %s: chanspec %s, called on %s\n",
		wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
		wf_chspec_ntoa(cfg->current_bss->chanspec, chanbuf),
		BSSCFG_AP(cfg) ? "AP" : "STA"));

	/* if bsscfg going down is blocking bsscfg, clear it */
	if (cfg == mchan->blocking_bsscfg) {
		wlc_mchan_reset_blocking_bsscfg(mchan);
	}

	/* reset the last_witch_time, in case the device is again in vsdb mode, */
	/* use fresh last_witch_time */
	mchan->si_algo_last_switch_time = 0;

	/* clear trigger idx */
	if (mchan->trigger_cfg_idx == WLC_BSSCFG_IDX(cfg)) {
		wlc_bsscfg_t *icfg;
		int idx;
		int8 selected_idx = -1;
		uint16 shortest_bi = 0xFFFF;

		FOREACH_BSS(wlc, idx, icfg) {
			if (!icfg->up) {
				continue;
			}
			if (BSSCFG_SPECIAL(wlc, icfg) || (icfg == cfg) ||
				BSS_P2P_DISC_ENAB(wlc, icfg)) {
				continue;
			}
			if (BSSCFG_AP(icfg)) {
				selected_idx = WLC_BSSCFG_IDX(icfg);
				break;
			}
			if (WLC_MCHAN_SAME_CTLCHAN(icfg->current_bss->chanspec,
				cfg->current_bss->chanspec)) {
				selected_idx = WLC_BSSCFG_IDX(icfg);
				break;
			}
			if (icfg->current_bss->beacon_period < shortest_bi) {
				selected_idx = WLC_BSSCFG_IDX(icfg);
				shortest_bi = icfg->current_bss->beacon_period;
			}
		}
		if (selected_idx != MCHAN_CFG_IDX_INVALID) {
			mchan->trigger_cfg_idx = selected_idx;
		}
		else {
			mchan->trigger_cfg_idx = mchan->other_cfg_idx;
			mchan->other_cfg_idx = MCHAN_CFG_IDX_INVALID;
		}
	}

	/* set the other idx */
	mchan->other_cfg_idx = wlc_mchan_set_other_cfg_idx(mchan, cfg);
	/* since in pm mode primary interface does not give pretbtt
	** interrupt every beacon interval, trigger cfg should always
	** be for p2p interface
	*/
	wlc_mchan_swap_cfg_idx(mchan);

	/* update mchan active status */
	wlc_mchan_multichannel_upd(wlc, mchan);

	if (!MCHAN_ACTIVE(wlc->pub) && oldstate) {
		/* if packets present for the other bsscfg (which is not getting deleted) in PSQ,
		 * flush them out now, since we will not be able to get a chance to toucg PSQ
		 * once we are out of MCHAN mode.
		 */
		if (other_bsscfg && wlc_has_chanctxt(wlc, other_bsscfg)) {
			wlc_bsscfg_t *icfg;
			int idx;
			FOREACH_BSS(wlc, idx, icfg) {
				if (icfg->up && wlc_shared_chanctxt(wlc, icfg, other_bsscfg)) {
#ifdef STA
					if (P2P_CLIENT(wlc, icfg)) {
						if (wlc_p2p_noa_valid(wlc->p2p, icfg)) {
							mboolclr(icfg->pm->PMblocked,
								WLC_PM_BLOCK_MCHAN_ABS);
						}
					} else {
						mboolclr(icfg->pm->PMblocked, WLC_PM_BLOCK_CHANSW);
					}
					wlc_update_pmstate(icfg, TX_STATUS_NO_ACK);
#endif /* STA */
					wlc_bsscfg_tx_start(wlc->psqi, icfg);
				}
			}
		}

		/* find the new chanspec from the triggering BSSCFG */
		ASSERT(mchan->trigger_cfg_idx != MCHAN_CFG_IDX_INVALID);
		new_chanspec = wlc_get_chanspec(wlc, WLC_BSSCFG(wlc, mchan->trigger_cfg_idx));

		/* update the home chanspec with the new chanspec we are moving to */
		ASSERT(new_chanspec != INVCHANSPEC);
		wlc->home_chanspec = new_chanspec;
	}

	return BCME_OK;
}

#if defined(WLRSDB) && defined(WL_MODESW)

int wlc_mchan_modesw_set_cbctx(wlc_info_t *wlc, uint8 type)
{
	wlc_info_t *wlc0;
	mchan_info_t *mchan;

	if (!wlc || (type >= WLC_MODESW_LIST_END))
		return BCME_BADARG;

	wlc0 = wlc->cmn->wlc[0];
	mchan = wlc0->mchan;

	/* if request is for clearing the context then, set the wlc to null
	 * and type to list start.
	 */
	if (type == WLC_MODESW_LIST_START) {
		mchan->modesw_ctx->wlc = NULL;
		mchan->modesw_ctx->timer_type = type;
		return BCME_OK;
	}

	/* if a timer is already existing then we should not add / wait till it finishes,
	 * hence return NOT PERMITTED.
	 */
	if (mchan->modesw_ctx->timer_type != WLC_MODESW_LIST_START) {
		WL_ERROR(("%s: Updating timer ctx while the timer is still armed \n",
			__func__));
		return BCME_EPERM;
	}

	mchan->modesw_ctx->wlc = wlc0;
	mchan->modesw_ctx->timer_type = type;

	return BCME_OK;
}

/* This function acts as a generic timer call back function for RSDB <-> VBSDB
 * mode switch. The name is unchanged to avoid abandons. Hence please just don't go
 * by the name. Currently it does clone as well as upgrade.
 */
static void
wlc_mchan_clone_timer_cb(void *arg)
{
	wlc_modesw_cb_ctx *modesw_ctx = (wlc_modesw_cb_ctx *) arg;
	wlc_info_t  *wlc = modesw_ctx->wlc;
	mchan_info_t* mchan = wlc->mchan;

	switch (modesw_ctx->timer_type) {
	case WLC_MODESW_CLONE_TIMER:
		wlc_mchan_set_clone_pending(mchan, FALSE);
		if (!MCHAN_ACTIVE(wlc->pub))
			return;
		wlc_mchan_clone_context_all(wlc->cmn->wlc[0], wlc->cmn->wlc[1],
			WLC_BSSCFG(wlc, mchan->other_cfg_idx));
		break;
	case WLC_MODESW_UPGRADE_TIMER:
		if (WLC_MODESW_ENAB(wlc->pub)) {
			wlc_rsdb_check_upgrade(wlc);
		}
		break;
	default:
		WL_ERROR(("%s: Timer called with invalid context !\n", __func__));
		break;
	}
	/* Clear the context to allow any other timer requests to run */
	wlc_mchan_modesw_set_cbctx(wlc, WLC_MODESW_LIST_START);
}

void
wlc_mchan_clone_context_all(wlc_info_t *from_wlc, wlc_info_t *to_wlc, wlc_bsscfg_t *cfg)
{
	wlc_bsscfg_t *icfg;
	int idx, ret;

	FOREACH_AS_STA(from_wlc, idx, icfg) {
		if (WLC_BSS_ASSOC_NOT_ROAM(icfg)) {
			if (wlc_shared_chanctxt(from_wlc, icfg, cfg)) {
				wlc_rsdb_bsscfg_clone(from_wlc, to_wlc, icfg, &ret);
			}
		}
	}
	return;
}
#endif /* WLRSDB && WL_MODESW */

int
wlc_mchan_msch_clbk(void* handler_ctxt, wlc_msch_cb_info_t *cb_info)
{
	wlc_bsscfg_t *cfg = (wlc_bsscfg_t *)handler_ctxt;
	wlc_info_t *wlc = cfg->wlc;
	uint32 type = cb_info->type;
	DBGONLY(char chanbuf[CHANSPEC_STR_LEN]; )

	WL_MCHAN(("wl%d.%d: %s: tsf = %u, chanspec %s, type 0x%04x\n",
		cfg->wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
		read_tsf_low(wlc),
		wf_chspec_ntoa(cb_info->chanspec, chanbuf), type));

	if (type & MSCH_CT_ON_CHAN) {
		wlc_mchan_return_home_channel(wlc->mchan, cfg);
	}
	if (type & MSCH_CT_OFF_CHAN) {
		/* Nothing todo */
	}

#if defined(WLRSDB) && defined(WL_MODESW)
	if (type & MSCH_CT_OFF_CHAN_DONE) {
		wlc_mchan_off_channel_done(wlc->mchan, cfg);
	}
#endif /* WLRSDB && WL_MODESW */
	return BCME_OK;

}

static void
wlc_mchan_correction_update(mchan_info_t *mchan, wlc_bsscfg_t *bsscfg)
{
	wlc_info_t *wlc = mchan->wlc;
	mchan_bss_info_t *mbi = MCHAN_BSS_INFO(mchan, bsscfg);
	int gap = 0;
	uint32 tsf_l = read_tsf_low(wlc);

	if (mbi->interval) {
		/*
		* no need to report delta when scan is in progress
		* or if any mchan initiated correction is in progress
		*/
		if (!SCAN_IN_PROGRESS(mchan->wlc->scan) &&
			!mchan->blocking_bsscfg &&
			!mchan->trigg_algo) {
			gap = ABS((int)(tsf_l - mbi->start_tsf));
			if (gap > (mchan->proc_time_us +
				MCHAN_MAX_ONCHAN_GAP_DRIFT_US)) {
				/* force schedule update */
				mchan->trigg_algo = TRUE;
				mchan->timer_running = FALSE;
				/* don't process further correction till updated */
				mbi->interval = 0;
			} else {
				/* not much drift, adopt start_tsf */
				mbi->start_tsf += mbi->interval;
			}
		} else {
			/* no action needed report problem next time */
		}
	}
}

static void
wlc_mchan_return_home_channel(mchan_info_t *mchan, wlc_bsscfg_t *bsscfg)
{
	wlc_info_t *wlc = mchan->wlc;
	mchan_bss_info_t *mbi = MCHAN_BSS_INFO(mchan, bsscfg);

	if (WLC_BSSCFG_IDX(bsscfg) == mchan->trigger_cfg_idx) {
		/* update mchan-msch related correction params */
		wlc_mchan_correction_update(mchan, bsscfg);
	}

	mchan->onchan_cfg_idx = WLC_BSSCFG_IDX(bsscfg);

	if (P2P_CLIENT(wlc, bsscfg) && (mbi->in_psc == TRUE)) {
		/* channel switch to P2P-GC cfg occurred post psc proc
		* need to bring P2P-GC out of PS for allowing traffic.
		* This is because P2P-GC is in PS with respect to P2P-GO before
		* going off channel in Multi-Device VSDB scenario.
		*/
		wlc_mchan_psc_cleanup(mchan, bsscfg);
	}

	/* Update stats needed for dynamic algo */
	wlc_mchan_update_stats(wlc, bsscfg);

#if defined(WLRSDB) && defined(WL_MODESW)
	if (WLC_MODESW_ENAB(wlc->pub) && RSDB_ENAB(wlc->pub)) {
		mbi = MCHAN_BSS_INFO(wlc->mchan, bsscfg);
		if (mbi) {
			if (wlc_modesw_mchan_hw_switch_pending(wlc, bsscfg, FALSE)) {
				wlc_modesw_mchan_switch_process(wlc->modesw, bsscfg);
				mbi->modesw_pending = TRUE;
			}
			if (mbi->modesw_pending &&
				wlc_modesw_mchan_hw_switch_complete(wlc, bsscfg)) {
				mbi->modesw_pending = FALSE;
#if defined(BCMPCIEDEV)
				if (BCMPCIEDEV_ENAB()) {
					wl_busioctl(wlc->wl,
						BUS_UPDATE_FLOW_PKTS_MAX, NULL, 0,
						NULL, NULL, FALSE);
				}
#endif /* BCMPCIEDEV */
			}
		}

		if (WLC_RSDB_DUAL_MAC_MODE(WLC_RSDB_CURR_MODE(wlc))) {
			if (wlc_mchan_get_clone_pending(mchan) &&
				!wlc_mchan_get_modesw_pending(mchan)) {
				if (WLC_BSSCFG_IDX(bsscfg) == mchan->other_cfg_idx &&
					wlc_mchan_modesw_set_cbctx(wlc,
						WLC_MODESW_CLONE_TIMER) == BCME_OK) {
					wl_add_timer(wlc->cmn->wlc[0]->wl,
						wlc->cmn->mchan_clone_timer,
						WL_RSDB_VSDB_CLONE_WAIT, FALSE);
				}
			}
		}
	}
#endif /* WLRSDB && WL_MODESW */

}

#if defined(WLRSDB) && defined(WL_MODESW)
static void
wlc_mchan_off_channel_done(mchan_info_t *mchan, wlc_bsscfg_t *bsscfg)
{
	wlc_info_t *wlc = mchan->wlc;

	if (WLC_MODESW_ENAB(wlc->pub) && RSDB_ENAB(wlc->pub) && MCHAN_ACTIVE(wlc->pub)) {
		wlc_bsscfg_t *oth_bsscfg = wlc_mchan_get_other_cfg_frm_cur(wlc, bsscfg);
		if ((wlc_modesw_mchan_hw_switch_pending(wlc, bsscfg, TRUE) ||
			wlc_modesw_mchan_hw_switch_pending(wlc, oth_bsscfg, TRUE)) &&
			!wlc_mchan_get_modesw_pending(wlc->mchan)) {
			/* invalidate phy chan ctxt for both cfg's */
			wlc_phy_invalidate_chanctx(WLC_PI(wlc),
				wlc_get_chanspec(wlc, bsscfg));
			wlc_phy_invalidate_chanctx(WLC_PI(wlc),
				wlc_get_chanspec(wlc, oth_bsscfg));
		}
	}
}
#endif /* WLRSDB && WL_MODESW */
/**
 * Given a bcn_per and tbtt times from two entities, calculate a fair schedule and provide the
 * schedule start time and duration for the first entity.  The schedule start time and duration
 * for the second entity can be inferred from start_time1, duration1, and bcn_per passed in.
 *
 * start_time2 = start_time1 + duration1.
 * duration2 = bcn_per - duration1.
 *
 * NOTE: tbtt1 < tbtt2 in time.
 */
static void
wlc_mchan_calc_fair_chan_switch_sched(uint32 next_tbtt1,
                                      uint32 curr_tbtt2,
                                      uint32 next_tbtt2,
                                      uint32 bcn_per,
                                      bool tx,
                                      uint32 *start_time1,
                                      uint32 *duration1)
{
	uint32 start1, dur1, end1, fair_dur, min_start1, min_end1, overhead;

	/* calculate a fair schedule */
	start1 = next_tbtt1;
	end1 = next_tbtt2;
	dur1 = end1 - start1;
	/* fair time is half of the bcn_per */
	fair_dur = bcn_per >> 1;
	/* XXX: min start1 and end1 times need to take into account ctwin, bc
	 * traffic tx time, beacon tx time, etc.
	 * For now, just use a fixed number.
	 */
	overhead = tx ? MCHAN_MIN_TBTT_TX_TIME_US : MCHAN_MIN_TBTT_RX_TIME_US;
	min_start1 = curr_tbtt2 + overhead;
	min_end1 = next_tbtt1 + overhead;
	/* This is when entity 1 gets too little time, correct it by moving start1
	 * closer to min_start1.
	 */
	if (dur1 < (fair_dur - MCHAN_FAIR_SCHED_DUR_ADJ_US)) {
		dur1 = fair_dur - dur1;
		start1 -= dur1;
		if (start1 < min_start1) {
			start1 = min_start1;
		}
	}
	/* This is when entity 2 gets too little time, correct it by moving
	 * end1 closer to min_end1.
	 */
	else if (dur1 > (fair_dur + MCHAN_FAIR_SCHED_DUR_ADJ_US)) {
		dur1 = dur1 - fair_dur;
		end1 -= dur1;
		if (end1 < min_end1) {
			end1 = min_end1;
		}
	}

	/* re-calculate duration */
	dur1 = end1 - start1;

	/* set the return values */
	*start_time1 = start1;
	*duration1 = dur1;
}

static int
wlc_mchan_noa_setup(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint32 bcn_per,
                    uint32 curr_ap_tbtt, uint32 next_ap_tbtt, uint32 next_sta_tbtt,
                    uint32 tbtt_gap, bool create)
{
	wl_p2p_sched_t *noa_sched;
	wl_p2p_sched_desc_t *desc_ptr;
	int noa_sched_len;
	uint32 noa_start, noa_dur, noa_cnt;
	int err = 0;
	mchan_info_t *mchan = wlc->mchan;

	/* default value for single abs period per beacon interval */
	/* Save actual abs count value */
	mchan->abs_cnt = 1;

	/* allocate memory for schedule */
	noa_sched_len = sizeof(wl_p2p_sched_t);
	if ((noa_sched = (wl_p2p_sched_t *)MALLOC(wlc->osh, noa_sched_len)) == NULL) {
		return (BCME_NOMEM);
	}
	ASSERT(noa_sched != NULL);
	/* set schedule type to abs */
	noa_sched->type = WL_P2P_SCHED_TYPE_ABS;

	/* cancel schedule first */
	noa_sched->action = WL_P2P_SCHED_ACTION_RESET;
	err = wlc_p2p_mchan_noa_set(wlc->p2p, bsscfg, noa_sched, WL_P2P_SCHED_FIXED_LEN);

	WL_MCHAN(("%s: cancel noa schedule, err = %d\n", __FUNCTION__, err));

	if (create) {
		uint32 proc_time = mchan->proc_time_us;
		uint32 interval;
		wlc_bsscfg_t *ap_cfg = WLC_BSSCFG(wlc, mchan->trigger_cfg_idx);
		wlc_bsscfg_t *sta_cfg = WLC_BSSCFG(wlc, mchan->other_cfg_idx);

		noa_cnt = WL_P2P_SCHED_REPEAT;

		/* calculate a fair schedule */
		wlc_mchan_calc_fair_chan_switch_sched(next_sta_tbtt, curr_ap_tbtt,
				next_ap_tbtt, bcn_per, TRUE, &noa_start, &noa_dur);
		/* pull back noa_start by proc_time */
		noa_start -= proc_time;
		/* include channel switch time in noa_dur for p2p setup */
		noa_dur += proc_time;
		interval = bcn_per;

		/* noa_dur must be integer times of (1<<P2P_UCODE_TIME_SHIFT)us */
		noa_dur = MCHAN_TIMING_ALIGN(noa_dur);

		if (bsscfg->pm->PMenabled)
			noa_sched->action = WL_P2P_SCHED_ACTION_DOZE;
		else
			noa_sched->action = WL_P2P_SCHED_ACTION_NONE;

		noa_sched->option = WL_P2P_SCHED_OPTION_NORMAL;
		desc_ptr = noa_sched->desc;
		desc_ptr->start = ltoh32_ua(&noa_start);
		desc_ptr->interval = ltoh32_ua(&interval);
		desc_ptr->duration = ltoh32_ua(&noa_dur);
		desc_ptr->count = ltoh32_ua(&noa_cnt);

		err = wlc_p2p_mchan_noa_set(wlc->p2p, bsscfg, noa_sched, noa_sched_len);
		if (err != BCME_OK) {
			WL_ERROR(("%s: wlc_p2p_noa_set returned err = %d\n", __FUNCTION__, err));
		}

		/* save the tbtt_gap value passed in */
		mchan->tbtt_gap = tbtt_gap;

		WL_MCHAN(("wl%d.%d: %s: NOA start %u, end %u, duration %u interval %u\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), __FUNCTION__,
			noa_start, noa_start + noa_dur, noa_dur, interval));

		wlc_mchan_sched_add(mchan, sta_cfg, noa_dur - 2*proc_time, interval, noa_start);
		wlc_mchan_sched_add(mchan, ap_cfg, interval - noa_dur, interval,
			noa_start + noa_dur - proc_time);
	}
	else {
		/* clear the saved tbtt_gap value */
		mchan->tbtt_gap = 0;
	}

	MFREE(wlc->osh, noa_sched, noa_sched_len);

	return (err);
}

static int
wlc_mchan_noa_setup_multiswitch(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint32 interval,
	uint32 tbtt_gap, uint32 sel_dur, uint32 startswitch_for_abs, uint8 cnt)
{
	wl_p2p_sched_t *noa_sched;
	wl_p2p_sched_desc_t *desc_ptr;
	int noa_sched_len;
	uint32 noa_start, noa_dur, noa_cnt;
	int err;
	mchan_info_t *mchan = wlc->mchan;
	uint8 abs_cnt = cnt/2;
	wlc_bsscfg_t *ap_cfg = WLC_BSSCFG(wlc, mchan->trigger_cfg_idx);
	wlc_bsscfg_t *sta_cfg = WLC_BSSCFG(wlc, mchan->other_cfg_idx);
	uint32 proc_time = mchan->proc_time_us;

	/* Save actual abs count value */
	mchan->abs_cnt = abs_cnt;

	/* allocate memory for schedule */
	noa_sched_len = sizeof(wl_p2p_sched_t);
	if ((noa_sched = (wl_p2p_sched_t *)MALLOCZ(wlc->osh, noa_sched_len)) == NULL) {
		WL_ERROR((WLC_BSS_MALLOC_ERR, WLCWLUNIT(wlc), WLC_BSSCFG_IDX(bsscfg), __FUNCTION__,
			noa_sched_len, MALLOCED(wlc->osh)));
		return (BCME_NOMEM);
	}
	ASSERT(noa_sched != NULL);
	ASSERT(sel_dur != 0);
	ASSERT(startswitch_for_abs != 0);

	/* set schedule type to abs */
	noa_sched->type = WL_P2P_SCHED_TYPE_ABS;

	/* cancel schedule first */
	noa_sched->action = WL_P2P_SCHED_ACTION_RESET;
	err = wlc_p2p_mchan_noa_set(wlc->p2p, bsscfg, noa_sched, WL_P2P_SCHED_FIXED_LEN);

	WL_MCHAN(("%s: cancel noa schedule, err = %d\n", __FUNCTION__, err));

	/* for unequal bandwidth cases, modify the noa_dur */
	noa_dur = sel_dur;
	/* For handling multiple abscence periods in one beacon interval */
	interval /= abs_cnt;

	noa_start = startswitch_for_abs;
	noa_cnt = WL_P2P_SCHED_REPEAT;

	/* pull back noa_start by proc_time */
	noa_start -= proc_time;
	/* include channel switch time in noa_dur for p2p setup */
	noa_dur += proc_time;
	/* noa_dur must be integer times of (1<<P2P_UCODE_TIME_SHIFT)us */
	noa_dur = MCHAN_TIMING_ALIGN(noa_dur);

	noa_sched->action = WL_P2P_SCHED_ACTION_NONE;
	noa_sched->option = WL_P2P_SCHED_OPTION_NORMAL;

	desc_ptr = noa_sched->desc;
	desc_ptr->start = ltoh32_ua(&noa_start);
	desc_ptr->interval = ltoh32_ua(&interval);
	desc_ptr->duration = ltoh32_ua(&noa_dur);
	desc_ptr->count = ltoh32_ua(&noa_cnt);

	err = wlc_p2p_mchan_noa_set(wlc->p2p, bsscfg, noa_sched, noa_sched_len);
	if (err != BCME_OK) {
		WL_ERROR(("%s: wlc_p2p_noa_set returned err = %d\n", __FUNCTION__, err));
	}

	WL_MCHAN(("wl%d.%d: %s: NOA start %u, end %u, duration %u interval %u\n",
		wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), __FUNCTION__,
		noa_start, noa_start + noa_dur, noa_dur, interval));

	wlc_mchan_sched_add(mchan, sta_cfg, noa_dur - 2*proc_time, interval, noa_start);
	wlc_mchan_sched_add(mchan, ap_cfg, interval - noa_dur, interval,
		noa_start + noa_dur - proc_time);

	/* save the tbtt_gap value passed in */
	mchan->tbtt_gap = tbtt_gap;
	MFREE(wlc->osh, noa_sched, noa_sched_len);
	return (err);
}

static int
wlc_mchan_ap_pretbtt_proc(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint32 tsf_l, uint32 tsf_h)
{
	/* This code handles P2P GO related scheduling */
	mchan_info_t *mchan = wlc->mchan;
	wlc_mchan_algo_info_t *mchan_algo_info = mchan->mchan_algo_info;
	wlc_bsscfg_t *bsscfg_sta;
	int err = BCME_OK;
	bool tbtt_drifted = FALSE;

	/* Initialis the tsf_l and tsf_h values in mchan_algo_info structure */
	mchan_algo_info->tsf_h = tsf_h;
	mchan_algo_info->tsf_l = tsf_l;

	/* Don't do anything if not up */
	if (!bsscfg->up) {
		return (BCME_NOTUP);
	}

	/* check whether tbtt has bee adjusted recently or not.
	 * If adjusted recently wait until the new tsf is populated
	 */
	if (mchan->tsf_adj_wait_cnt) {
		WL_MCHAN(("%s: tbtt_gap, wait %d after tsf_adj\n",
			__FUNCTION__, mchan->tsf_adj_wait_cnt));
		mchan->tsf_adj_wait_cnt--;
		return (BCME_NOTREADY);
	}

	/* Indicates whether the other interface bsscfg index is valid */
	ASSERT(wlc->mchan->other_cfg_idx != MCHAN_CFG_IDX_INVALID);

	/* find the sta_bsscfg */
	bsscfg_sta = WLC_BSSCFG(wlc, wlc->mchan->other_cfg_idx);

	ASSERT(bsscfg_sta != NULL);
	if (!bsscfg_sta) {
		return (BCME_ERROR);
	}

	/* if sta bsscfg not associated, return */
	if (!bsscfg_sta->associated) {
		return (BCME_NOTASSOCIATED);
	}

	if (!wlc_mcnx_tbtt_valid(wlc->mcnx, bsscfg_sta)) {
		if (mchan->wait_sta_valid < MAX_WAIT_STA_VALID) {
			wlc_mchan_switch_chan_context(mchan, bsscfg_sta, bsscfg);
			mchan->wait_sta_valid++;
		}
		else {
			/* STA's AP not found after repeated trials.
			 * switch back to the GO channel
			 */
			WL_MCHAN(("wl%d.%d: %s: STA's AP not found, delete chanctx for 0x%x\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), __FUNCTION__,
				bsscfg->current_bss->chanspec));
			wlc_mchan_switch_chan_context(mchan, bsscfg, bsscfg_sta);
		}
		mchan->tbtt_gap = 0;
		return (BCME_NOTREADY);
	}

	mchan->wait_sta_valid = 0;

	if (wlc->stas_associated > 1 && !mchan->multi_if_adjustment) {
		mchan->multi_if_adjustment = TRUE;
		mchan->trigg_algo = TRUE;
		mchan->switch_algo = MCHAN_DEFAULT_ALGO;
	}

	if (wlc->stas_associated == 1) {
		if (mchan->multi_if_adjustment) {
			mchan->trigg_algo = TRUE;
			mchan->multi_if_adjustment = FALSE;
		}
	}

	/* fucntion for getting following values
	 * 1. curr_ap_tbtt
	 * 2. curr_sta_tbtt
	 * 3. next_ap_tbtt
	 * 4. next_sta_tbtt
	 * 5. tbtt_gap
	 */
	wlc_mchan_get_pretbtts(mchan);

	/* In case the computed next_oth_tbtt is less than curr_trig_tbtt
	 * then keep adding oth_bcn_per to next_oth_tbtt and curr_oth_tbtt
	 * until 'curr_oth_tbtt < next_oth_tbtt < next_trig_tbtt'.
	 */
	while (mchan_algo_info->next_oth_tbtt < mchan_algo_info->curr_trig_tbtt) {
		mchan_algo_info->next_oth_tbtt += (bsscfg_sta->current_bss->beacon_period << 10);
		mchan_algo_info->curr_oth_tbtt += (bsscfg_sta->current_bss->beacon_period << 10);
	}

	WL_MCHAN(("%s: tbtt_gap = %d, c_ap_tbtt = %u, c_sta_tbtt = %u, "
		"n_ap_tbtt = %u, n_sta_tbtt = %u\n", __FUNCTION__,
		mchan_algo_info->tbtt_gap,
		(((1<<21)-1)& mchan_algo_info->curr_trig_tbtt),
		(((1<<21)-1)& mchan_algo_info->curr_oth_tbtt),
		(((1<<21)-1)& mchan_algo_info->next_trig_tbtt),
		(((1<<21)-1)& mchan_algo_info->next_oth_tbtt)));

	/* Has beacon's alignment drifted beyond MCHAN_MAX_TBTT_GAP_DRIFT_US ? */
	tbtt_drifted = !(mchan->tbtt_gap &&
		(ABS((int32)(mchan->tbtt_gap - mchan_algo_info->tbtt_gap)) <=
		MCHAN_MAX_TBTT_GAP_DRIFT_US));

	WL_MCHAN(("%s: tbtt_drift = %d, drifted = %d, trigg_algo = %d, algo %d\n",
		__FUNCTION__, ABS((int32)(mchan->tbtt_gap - mchan_algo_info->tbtt_gap)),
		tbtt_drifted, mchan->trigg_algo, mchan->switch_algo));

	/* if tbtt_gap meets requirement, we are guaranteed next_sta_tbtt < next_ap_tbtt */
	/* following only works for same beacon period for now */

	/* if GO, setup noa */
	if (BSS_P2P_ENAB(wlc, bsscfg)) {

		if (!tbtt_drifted && (mchan->trigg_algo == FALSE) &&
			(wlc_p2p_noa_valid(wlc->p2p, bsscfg))) {
			/* schedule already setup */
			return (err);
		}

		/* Selects the appropriate algo for GO */
		err = wlc_mchan_algo_selection(mchan);
	}

	return (err);
}

/*
 * This function populates the values which are in wlc_mchan_algo_info_t structure to local
 * variables.
 */
static void
wlc_mchan_populate_values(mchan_info_t *mchan, uint32 *curr_trig_tbtt,
		uint32 *curr_oth_tbtt, uint32 *next_trig_tbtt,
		uint32 *next_oth_tbtt, uint32 *oth_bcn_per,
		uint32 *trig_bcn_per, uint32 *tbtt_gap, uint32 *tsf_h, uint32 *tsf_l)
{
	wlc_mchan_algo_info_t *mchan_algo_info;
	mchan_algo_info = mchan->mchan_algo_info;

	/* Copy values from structure variable to teh local variables */
	*curr_trig_tbtt = mchan_algo_info->curr_trig_tbtt;
	*next_trig_tbtt = mchan_algo_info->next_trig_tbtt;
	*curr_oth_tbtt = mchan_algo_info->curr_oth_tbtt;
	*next_oth_tbtt = mchan_algo_info->next_oth_tbtt;
	*trig_bcn_per = mchan_algo_info->trig_bcn_per;
	*oth_bcn_per = mchan_algo_info->oth_bcn_per;
	*tbtt_gap = mchan_algo_info->tbtt_gap;
	*tsf_h = mchan_algo_info->tsf_h;
	*tsf_l = mchan_algo_info->tsf_l;
}

/* Used for selecting the switch algo for GO/STA/GC and calling the respective function */
static int
wlc_mchan_algo_selection(mchan_info_t *mchan)
{
	wlc_info_t *wlc = mchan->wlc;
	wlc_bsscfg_t *bsscfg;
	wlc_mchan_algo_info_t *mchan_algo_info;
	uint32 curr_ap_tbtt, curr_sta_tbtt, next_ap_tbtt, next_sta_tbtt;
	uint32 oth_bcn_per, trig_bcn_per, tsf_l, tsf_h, tbtt_gap;
	uint32 ap_bcn_per;
	int err = BCME_OK;

	/* get the required values to use locally within the function */
	wlc_mchan_populate_values(mchan, &curr_ap_tbtt, &curr_sta_tbtt, &next_ap_tbtt,
		&next_sta_tbtt, &oth_bcn_per, &trig_bcn_per, &tbtt_gap, &tsf_h, &tsf_l);

	bsscfg = WLC_BSSCFG(wlc, mchan->trigger_cfg_idx);
	mchan_algo_info = mchan->mchan_algo_info;
	ap_bcn_per = trig_bcn_per;

	if (BSSCFG_AP(bsscfg)) {
		/* In either AP or GO case the trigger cfg is AP.
		 * STA or GC is the other cfg.
		 */
		if (mchan->switch_algo == MCHAN_SI_ALGO) {
			/* Adjusting the GO beacon according to switch times.
			 * Moving the tsf means moving the sta tbtt wrt our local tsf.
			 * AP's tbtt stays the same wrt our local tsf.
			 */
			mchan_algo_info->adj_type = MCHAN_TSF_ADJ_SI;
		}
		else {
			/* Thus, if sta tbtt is > ap tbtt, move by (ap_bcn_per/2)-tbtt_gap.
			 * Otherwise, move by (ap_bcn_per/2)+tbtt_gap.
			 * tsf_adj > 0 == sta_tbtt > ap_tbtt
			 */
			mchan_algo_info->adj_type = MCHAN_TSF_ADJ_REGULAR;
		}

		if ((err = wlc_mchan_tbtt_gap_adj(mchan)) == BCME_NOTREADY)
			return err;
	}

	mchan->trigg_algo = FALSE;

	/* Decides which algo to invoke.
	 * Also it checks whether the bsscfg is AP or STA
	 * and call respective functions
	 */
	switch (mchan->switch_algo) {
		case MCHAN_DYNAMIC_BW_ALGO: {
			/* Return if DYN_ENAB is not true */
			if (!DYN_ENAB(mchan))
				return BCME_UNSUPPORTED;
		}

		case MCHAN_BANDWIDTH_ALGO: {
			/* MCHAN BW algorithm for GO/STA cases */
			if (BSSCFG_AP(bsscfg))
				err = wlc_mchan_go_bandwidth_algo(mchan);
			else {
				if (trig_bcn_per == oth_bcn_per)
					err = wlc_mchan_sta_bandwidth_algo(mchan);
				else
					err = BCME_UNSUPPORTED;
			}
			break;
		}

		case MCHAN_SI_ALGO:{
			/* MCHAN SI algorithm for GO/STA cases */
			if (BSSCFG_AP(bsscfg))
				err = wlc_mchan_go_si_algo(mchan);
			else
				err = wlc_mchan_sta_si_algo(mchan);
			break;
		}

		default: {
			/* Default algo */
			if (BSSCFG_AP(bsscfg))
				err = wlc_mchan_noa_setup(wlc, bsscfg, ap_bcn_per,
					curr_ap_tbtt, next_ap_tbtt, next_sta_tbtt,
					tbtt_gap, TRUE);
			else
				err = wlc_mchan_sta_default_algo(mchan);
			break;
		 }
	}
	return err;
}

/* Performs the tbtt adjustment part */
static int
wlc_mchan_tsf_adj(mchan_info_t *mchan, int32 tsf_adj)
{
	wlc_info_t *wlc = mchan->wlc;
	wlc_bsscfg_t *bsscfg;
	uint32 tgt_h, tgt_l, tbtt_h, tbtt_l;
	uint32 curr_ap_tbtt, curr_sta_tbtt, next_ap_tbtt, next_sta_tbtt;
	uint32 sta_bcn_per, ap_bcn_per, tsf_l, tsf_h, tbtt_gap;

	wlc_mchan_populate_values(mchan, &curr_ap_tbtt, &curr_sta_tbtt, &next_ap_tbtt,
		&next_sta_tbtt, &sta_bcn_per, &ap_bcn_per, &tbtt_gap, &tsf_h, &tsf_l);

	bsscfg = WLC_BSSCFG(wlc, mchan->trigger_cfg_idx);

	tgt_h = tsf_h;
	tgt_l = tsf_l;
	wlc_uint64_add(&tgt_h, &tgt_l, tsf_adj < 0 ? -1 : 0,
		tsf_adj);
	wlc_mcnx_l2r_tsf64(wlc->mcnx, bsscfg, tgt_h, tgt_l,
		&tbtt_h, &tbtt_l);
	wlc_tsf64_to_next_tbtt64(bsscfg->current_bss->beacon_period,
		&tbtt_h, &tbtt_l);
	wlc_mcnx_r2l_tsf64(wlc->mcnx, bsscfg, tbtt_h, tbtt_l, &tbtt_h,
		&tbtt_l);
	wlc_tsf_adj(wlc, bsscfg, tgt_h, tgt_l, tsf_h, tsf_l, tbtt_l,
		ap_bcn_per, FALSE);
	mchan->tsf_adj_wait_cnt = 2;

	return (BCME_NOTREADY);
}

/*
 * Calculates the amount of tbtt adjusment that is required based on the adjustment type specified
 */
static int
wlc_mchan_tbtt_gap_adj(mchan_info_t *mchan)
{
	wlc_mchan_algo_info_t *mchan_algo_info;
	int32 tsf_adj;
	bool adjusted = FALSE;
	int err = BCME_OK;
	uint32 curr_ap_tbtt, curr_sta_tbtt, next_ap_tbtt, next_sta_tbtt;
	uint32 sta_bcn_per, ap_bcn_per, tsf_l, tsf_h, tbtt_gap;

	wlc_mchan_populate_values(mchan, &curr_ap_tbtt, &curr_sta_tbtt, &next_ap_tbtt,
		&next_sta_tbtt, &sta_bcn_per, &ap_bcn_per, &tbtt_gap, &tsf_h, &tsf_l);

	mchan_algo_info = mchan->mchan_algo_info;
	/* reform ap interface */
	WL_MCHAN(("%s: Reform AP interface, TBTT gap %d, adj %d\n", __FUNCTION__,
		tbtt_gap, (int32)(next_sta_tbtt - curr_ap_tbtt)));

	tsf_adj = (int32)(next_sta_tbtt - curr_ap_tbtt);

	if (mchan_algo_info->adj_type == MCHAN_TSF_ADJ_REGULAR) {
		if (tbtt_gap <= MCHAN_MIN_AP_TBTT_GAP_US &&
			mchan->switch_algo != MCHAN_SI_ALGO) {
			if (!((tsf_adj == (int32)tbtt_gap) ||
				(tsf_adj == -((int32)tbtt_gap)))) {
				tsf_adj = (int32)(next_sta_tbtt - next_ap_tbtt);
			}
			/* Moving the tsf means moving the sta tbtt wrt our local tsf.
			 * AP's tbtt stays the same wrt our local tsf.
			 * Thus, if sta tbtt is > ap tbtt, move by (ap_bcn_per/2)-tbtt_gap.
			 * Otherwise, move by (ap_bcn_per/2)+tbtt_gap.
			 * tsf_adj > 0 == sta_tbtt > ap_tbtt
			 */
			tsf_adj = (tsf_adj > 0) ? ((ap_bcn_per>>1) - tbtt_gap) :
				(ap_bcn_per>>1) + tbtt_gap;

			/* update the GO's tsf */
			WL_MCHAN(("%s: (tbtt_gap) adjust tsf by %d\n", __FUNCTION__, tsf_adj));
			err = wlc_mchan_tsf_adj(mchan, tsf_adj);
		}
	}
	else if (mchan_algo_info->adj_type == MCHAN_TSF_ADJ_SI) {

		/* Checking is the GO beacon is within the bounds - 33 to 42
		 * Adjusting to the centre of the presence period ~ 38ms
		 */

		if (tsf_adj < MCHAN_SI_ALGO_BCN_LOW_PSC_1) {
			tsf_adj = MCHAN_SI_ALGO_BCN_OPT_PSC_1 - tsf_adj;
			adjusted = TRUE;
		}
		else if (tsf_adj > MCHAN_SI_ALGO_BCN_LOW_PSC_2) {
			tsf_adj = MCHAN_SI_ALGO_BCN_OPT_PSC_1 - tsf_adj	+
				ap_bcn_per;
			adjusted = TRUE;
		}
		/* If the beacon is found to be outside the limits
		 * then adjust the beacon accordingly
		 */
		if (adjusted) {
			err = wlc_mchan_tsf_adj(mchan, tsf_adj);
			WL_MCHAN(("%s: (tbtt_gap) adjust tsf by %d\n", __FUNCTION__, tsf_adj));
		}
	}
	else {
		WL_MCHAN(("%s: Invalid adjustment type\n", __FUNCTION__));
	}
	return err;
}

/* Dynamic bandwidth algo for AP(basically for P2P GO) */
static int
wlc_mchan_go_bandwidth_algo(mchan_info_t *mchan)
{
	wlc_info_t *wlc = mchan->wlc;
	wlc_bsscfg_t *bsscfg;
	int32 err;
	uint32 switchtime_1, sel_dur, switchtime_2;
	uint32 primary_ch_time;
	uint32 availtime_1, availtime_2;
	uint32 adj_remtime = 0;
	uint32 secondary_ch_percentage = 100 - mchan->percentage_bw;
	uint32 curr_ap_tbtt, curr_sta_tbtt, next_ap_tbtt, next_sta_tbtt;
	uint32 sta_bcn_per, ap_bcn_per, tsf_l, tsf_h, tbtt_gap;

	wlc_mchan_populate_values(mchan, &curr_ap_tbtt, &curr_sta_tbtt, &next_ap_tbtt,
		&next_sta_tbtt, &sta_bcn_per, &ap_bcn_per, &tbtt_gap, &tsf_h, &tsf_l);

	bsscfg = WLC_BSSCFG(wlc, mchan->trigger_cfg_idx);

	/* Putting min-max limits for the BW range (25%-75%) only */
	/* Beacon (failure to send beacon) losses if primary BW>75% */
	if (secondary_ch_percentage > 75)
		secondary_ch_percentage = 75;
	else if (secondary_ch_percentage < 25)
		secondary_ch_percentage = 25;

	primary_ch_time = sta_bcn_per *	(100 - secondary_ch_percentage) / 100;

	WL_MCHAN(("%s: tbtt_gap:%d, tsf:%u\n", __FUNCTION__, tbtt_gap, (((1<<21)-1) & tsf_l)));
	WL_MCHAN(("%s: c_ap_tbtt = %u, c_sta_tbtt = %u, "
		"n_ap_tbtt = %u, n_sta_tbtt = %u\n", __FUNCTION__,
		(((1<<21)-1) & curr_ap_tbtt), (((1<<21)-1) & curr_sta_tbtt),
		(((1<<21)-1) & next_ap_tbtt), (((1<<21)-1) & next_sta_tbtt)));

	/*
	 *   Graphical representation of the diffrent tbtt's in time
	 *
	 *                  swtime_1         swtime_2
	 *          ^ ^       |      ^         |         ^
	 *   ^      | |       |      |         |         |
	 *   |      | |       |      |         |         |
	 * -------------------------------------------------------
	 *curr_sta_tbtt curr time(tsf) next_sta_tbtt         next_ap_tbtt
	 *         ^
	 *         |
	 *         curr_ap_tbtt
	 */

	availtime_1 = next_sta_tbtt - curr_ap_tbtt;
	availtime_2 = next_ap_tbtt - next_sta_tbtt;

	/* we need to handle availtime_1 > availtime_2 in a different manner
	 * compared to availtime_1 <= availtime_2. The default algorithm was failing
	 * in the former case when say for ex. secondary_ch_time = 80ms and
	 * primary_ch_time = 20ms. One can fit in the values and check assuming
	 * availtime_1 = 65ms and availtime_2 = 35ms
	 */

	if (availtime_1 <= availtime_2) {
		switchtime_1 = next_sta_tbtt;
		switchtime_2 = switchtime_1 + primary_ch_time;
		if (switchtime_2 > next_ap_tbtt)
			adj_remtime = switchtime_2 - next_ap_tbtt;
		if (adj_remtime) {
			switchtime_1 -= adj_remtime;
			switchtime_2 -= adj_remtime;
		}
	}
	else {
		switchtime_2 = next_ap_tbtt;
		switchtime_1 = switchtime_2 - primary_ch_time;
		if (switchtime_1 > next_sta_tbtt)
			adj_remtime = switchtime_1 - next_sta_tbtt;

		if (adj_remtime) {
			switchtime_1 -= adj_remtime;
			switchtime_2 -= adj_remtime;
		}
	}

	sel_dur = switchtime_2 - switchtime_1;

	WL_MCHAN(("%s: swtime_1:%u swtime_2:%u sel_dur:%u\n",
		__FUNCTION__, (((1<<21)-1)& switchtime_1),
		(((1<<21)-1)&switchtime_2), sel_dur));

	err = wlc_mchan_noa_setup_multiswitch(wlc, bsscfg, ap_bcn_per, tbtt_gap,
		sel_dur, switchtime_1, 2);

	return err;
}

/* Service interval algo for AP(basically for P2P case) */
static int
wlc_mchan_go_si_algo(mchan_info_t *mchan)
{
	/* This only Works for 25ms service interval */
	wlc_info_t *wlc = mchan->wlc;
	wlc_bsscfg_t *bsscfg;
	uint32 si_time;
	uint8 interval_slots;
	int err = BCME_OK;
	uint32 curr_ap_tbtt, curr_sta_tbtt, next_ap_tbtt, next_sta_tbtt;
	uint32 sta_bcn_per, ap_bcn_per, tsf_l, tsf_h, tbtt_gap;

	wlc_mchan_populate_values(mchan, &curr_ap_tbtt, &curr_sta_tbtt, &next_ap_tbtt,
		&next_sta_tbtt, &sta_bcn_per, &ap_bcn_per, &tbtt_gap, &tsf_h, &tsf_l);

	bsscfg = WLC_BSSCFG(wlc, mchan->trigger_cfg_idx);

	mchan->switch_interval = DEFAULT_SI_TIME;
	si_time = mchan->switch_interval;

	WL_MCHAN(("%s: tbtt_gap:%d, tsf:%u\n", __FUNCTION__, tbtt_gap, (((1<<21)-1) & tsf_l)));
	WL_MCHAN(("%s: c_ap_tbtt = %u, c_sta_tbtt = %u, "
		"n_ap_tbtt = %u, n_sta_tbtt = %u\n", __FUNCTION__,
		(((1<<21)-1) & curr_ap_tbtt), (((1<<21)-1) & curr_sta_tbtt),
		(((1<<21)-1) & next_ap_tbtt), (((1<<21)-1) & next_sta_tbtt)));

	/* Number of Interval slots in one beacon period */
	interval_slots = ap_bcn_per / si_time;

	/* Here we are sure of correct beacon positioning */
	/* Create schedule and dispatch */

	err = wlc_mchan_noa_setup_multiswitch(wlc, bsscfg, ap_bcn_per, tbtt_gap, si_time,
		curr_ap_tbtt + si_time, interval_slots);

	return err;
}

/* This function calculates the pretbtt times and returns it along with tbtt gap */
static void
wlc_mchan_get_pretbtts(mchan_info_t *mchan)
{
	wlc_info_t *wlc = mchan->wlc;
	wlc_bsscfg_t *bsscfg, *bsscfg_sta;
	wlc_mchan_algo_info_t *mchan_algo_info;
	uint32 tbtt_h, tbtt_l;
	uint32 curr_trig_tbtt, curr_oth_tbtt, next_trig_tbtt, next_oth_tbtt;
	uint32 oth_bcn_per, trig_bcn_per, tsf_l, tsf_h, tbtt_gap;

	wlc_mchan_populate_values(mchan, &curr_trig_tbtt, &curr_oth_tbtt, &next_trig_tbtt,
		&next_oth_tbtt, &oth_bcn_per, &trig_bcn_per, &tbtt_gap, &tsf_h, &tsf_l);
	bsscfg = WLC_BSSCFG(wlc, mchan->trigger_cfg_idx);
	bsscfg_sta = WLC_BSSCFG(wlc, mchan->other_cfg_idx);
	mchan_algo_info = mchan->mchan_algo_info;

	/* get sta tbtt info */
	oth_bcn_per = bsscfg_sta->current_bss->beacon_period << 10;
	wlc_mcnx_l2r_tsf64(wlc->mcnx, bsscfg_sta, tsf_h,
		tsf_l, &tbtt_h, &tbtt_l);
	wlc_tsf64_to_next_tbtt64(bsscfg_sta->current_bss->beacon_period,
		&tbtt_h, &tbtt_l);
	next_oth_tbtt = wlc_mcnx_r2l_tsf32(wlc->mcnx, bsscfg_sta, tbtt_l);

	/* get trigger tbtt info */
	trig_bcn_per = bsscfg->current_bss->beacon_period << 10;
	wlc_mcnx_l2r_tsf64(wlc->mcnx, bsscfg, tsf_h,
		tsf_l, &tbtt_h, &tbtt_l);
	wlc_tsf64_to_next_tbtt64(bsscfg->current_bss->beacon_period, &tbtt_h, &tbtt_l);
	curr_trig_tbtt = wlc_mcnx_r2l_tsf32(wlc->mcnx, bsscfg, tbtt_l);

	/* calculate the next tbtt */
	curr_oth_tbtt = next_oth_tbtt -
		oth_bcn_per;
	next_trig_tbtt = curr_trig_tbtt +
		trig_bcn_per;

	/* we got tbbt times. Make them pre_tbtt */
	curr_trig_tbtt -= WLC_MCHAN_PRETBTT_TIME_US(mchan);
	curr_oth_tbtt -= WLC_MCHAN_PRETBTT_TIME_US(mchan);
	next_trig_tbtt -= WLC_MCHAN_PRETBTT_TIME_US(mchan);
	next_oth_tbtt -= WLC_MCHAN_PRETBTT_TIME_US(mchan);

	/* figure out gap between the 2 to make sure it is enough for operation */
	tbtt_gap = MIN(ABS((int32) (next_oth_tbtt - curr_trig_tbtt)),
		ABS((int32)(next_trig_tbtt - next_oth_tbtt)));

	/* write the calculated values into the structure */
	mchan_algo_info->curr_trig_tbtt = curr_trig_tbtt;
	mchan_algo_info->curr_oth_tbtt = curr_oth_tbtt;
	mchan_algo_info->next_trig_tbtt = next_trig_tbtt;
	mchan_algo_info->next_oth_tbtt = next_oth_tbtt;
	mchan_algo_info->trig_bcn_per = trig_bcn_per;
	mchan_algo_info->oth_bcn_per = oth_bcn_per;
	mchan_algo_info->tbtt_gap = tbtt_gap;
}

wlc_bsscfg_t *
wlc_mchan_get_other_cfg_frm_q(wlc_info_t *wlc, wlc_txq_info_t *qi)
{
	wlc_bsscfg_t *cfg;
	if (MCHAN_ACTIVE(wlc->pub)) {
		cfg = WLC_BSSCFG(wlc, wlc->mchan->trigger_cfg_idx);
		if (cfg && cfg->wlcif->qi == qi)
			return WLC_BSSCFG(wlc, wlc->mchan->other_cfg_idx);

		cfg = WLC_BSSCFG(wlc, wlc->mchan->other_cfg_idx);
		if (cfg && cfg->wlcif->qi == qi)
			return WLC_BSSCFG(wlc, wlc->mchan->trigger_cfg_idx);
	}
	return NULL;
}

wlc_bsscfg_t *
wlc_mchan_get_cfg_frm_q(wlc_info_t *wlc, wlc_txq_info_t *qi)
{
	wlc_bsscfg_t *cfg;
	if (MCHAN_ACTIVE(wlc->pub)) {
		cfg = WLC_BSSCFG(wlc, wlc->mchan->trigger_cfg_idx);
		if (cfg && cfg->wlcif->qi == qi)
			return cfg;

		cfg = WLC_BSSCFG(wlc, wlc->mchan->other_cfg_idx);
		if (cfg && cfg->wlcif->qi == qi)
			return cfg;
	}
	return NULL;
}

static int
wlc_mchan_sta_pretbtt_proc(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg_sel, uint32 tsf_l, uint32 tsf_h)
{
	int err = BCME_OK;
	mchan_info_t *mchan = wlc->mchan;
	wlc_bsscfg_t *bsscfg_oth;
	uint32 proc_time = (uint32)mchan->proc_time_us;
	mchan_bss_info_t *mbi_sel;
	wlc_mchan_algo_info_t *mchan_algo_info;
	uint32 curr_sel_tbtt, curr_oth_tbtt, next_sel_tbtt, next_oth_tbtt;
	uint32 sel_bcn_per, oth_bcn_per, tbtt_gap;
	uint32 bcn_position, switch_algo;

	mchan_algo_info = mchan->mchan_algo_info;
	/* initialise the tsf_l and tsf_h value in the
	 * mchan_algo_info structure
	 */
	mchan_algo_info->tsf_l = tsf_l;
	mchan_algo_info->tsf_h = tsf_h;

	/* Don't do anything if not up */
	if (!bsscfg_sel->up) {
		return (BCME_NOTUP);
	}

	/* if selected bsscfg not associated, return */
	if (!bsscfg_sel->associated) {
		return (BCME_NOTASSOCIATED);
	}

	ASSERT(mchan->other_cfg_idx != MCHAN_CFG_IDX_INVALID);

	/* find the selected and other bss */
	bsscfg_oth = WLC_BSSCFG(wlc, mchan->other_cfg_idx);

	ASSERT(bsscfg_oth != NULL);
	if (!bsscfg_oth) {
		return (BCME_ERROR);
	}

	mbi_sel = MCHAN_BSS_INFO(mchan, bsscfg_sel);

	/* if other bsscfg not associated, return */
	if (!bsscfg_oth->associated) {
		WL_ERROR(("%s: bss idx %d not associated.\n", __FUNCTION__,
			WLC_BSSCFG_IDX(bsscfg_oth)));
		return (BCME_NOTASSOCIATED);
	}

	if (!wlc_mcnx_tbtt_valid(wlc->mcnx, bsscfg_oth)) {
		WL_NONE(("%s: bss idx %d WLC_P2P_INFO_VAL_TBTT not set\n", __FUNCTION__,
			WLC_BSSCFG_IDX(bsscfg_oth)));
		return (BCME_NOTREADY);
	}

	mchan_algo_info->proc_time = proc_time;

	/* fucntion for getting following values
	 * 1. curr_ap_tbtt
	 * 2. curr_sta_tbtt
	 * 3. next_ap_tbtt
	 * 4. next_sta_tbtt
	 * 5. tbtt_gap
	 */
	wlc_mchan_get_pretbtts(mchan);

	/* get the listed values form the structure mchan_algo_info
	 * to use within function
	 */
	wlc_mchan_populate_values(mchan, &curr_sel_tbtt, &curr_oth_tbtt, &next_sel_tbtt,
		&next_oth_tbtt, &oth_bcn_per, &sel_bcn_per, &tbtt_gap, &tsf_h, &tsf_l);

	/*
	 *	Graphical representation of the different tbtt's in time
	 *
	 *			^				^
	 *	^		|		^		|
	 *	|		|		|		|
	 * -------------------------------------------------------
	 *  curr_oth_tbtt   curr_sel_tbtt   next_oth_tbtt   next_sel_tbtt
	 *			^
	 *			|
	 *			curr time (tsf)
	 */

	/* Look for special case to correct curr_oth and next_oth tbtt.
	 * Always assume curr_sel_tbtt and next_sel_tbtt are correct.
	 */
	if (curr_sel_tbtt < curr_oth_tbtt) {
		/* By the time we got here, next_oth_tbtt already happened.
		 * This means, curr_oth_tbtt is really next_oth_tbtt and
		 * next_oth_tbtt is really next next_oth_tbtt.
		 * Correct for this by subtracting oth_bcn_per from them.
		 */
		curr_oth_tbtt -= oth_bcn_per;
		next_oth_tbtt -= oth_bcn_per;
	}

	/* update sel_bsscfg sw_dtim_cnt */
	mbi_sel->sw_dtim_cnt = (mbi_sel->sw_dtim_cnt) ?
		(mbi_sel->sw_dtim_cnt - 1) : (bsscfg_sel->current_bss->dtim_period - 1);

	/* figure out gap between the 2 to determine what to do next */
	tbtt_gap = MIN(ABS((int32)(next_oth_tbtt - curr_sel_tbtt)),
		ABS((int32)(curr_sel_tbtt - curr_oth_tbtt)));

	bcn_position = mchan->bcn_position;
	switch_algo = mchan->switch_algo;

	/* If the beacon period for trigger and other is same and
	 * the tbtt gap is less than the threshold value then
	 * set the switch algo to SI  ALGO and set as overlapping beacons.
	 * If tbtt gap is fine then set switch algo to DEFAULT ALGO
	 */
	if (sel_bcn_per == oth_bcn_per) {
		if (wlc->stas_associated > 2) {
			mchan->bcn_position = MCHAN_BCN_MULTI_IF;
			mchan->switch_algo = MCHAN_DEFAULT_ALGO;
		} else if (tbtt_gap <= MCHAN_MIN_TBTT_GAP_US_NEW(mchan)) {
			mchan->bcn_position = MCHAN_BCN_OVERLAP;
			if ((mchan->switch_algo == MCHAN_DEFAULT_ALGO) && (mchan->bcnovlpopt) &&
				(mchan->overlap_algo_switch == FALSE)) {
				mchan->overlap_algo_switch = TRUE;
				mchan->switch_algo = MCHAN_SI_ALGO;
				mchan->si_algo_last_switch_time = 0;
				mchan->switch_interval = DEFAULT_SI_TIME;
			}
		}
		else {
			mchan->bcn_position = MCHAN_BCN_NONOVERLAP;
			if ((mchan->overlap_algo_switch) && (mchan->bcnovlpopt)) {
				mchan->overlap_algo_switch = FALSE;
				mchan->switch_algo = MCHAN_DEFAULT_ALGO;
			}
		}
	} else {
		mchan->bcn_position = MCHAN_BCN_INVALID;
		/* Switch to default algo when BIs for trigger and other cfgs are unequal. */
		mchan->switch_algo = MCHAN_DEFAULT_ALGO;
	}

	if (sel_bcn_per > oth_bcn_per) {
		/* swap the two indices - In case of different BI use the one with
		 * smallest BI as the trigger idx
		 */
		SWAP(mchan->trigger_cfg_idx, mchan->other_cfg_idx);
		mchan->trigg_algo = TRUE;
		return (BCME_NOTREADY);
	}

	if (sel_bcn_per == oth_bcn_per) {
		if ((ABS((int32)(curr_sel_tbtt - curr_oth_tbtt)) <=
			(int32)MCHAN_MIN_TBTT_GAP_US_NEW(mchan)) &&
			(curr_sel_tbtt > curr_oth_tbtt) &&
			(mchan->bcn_position != MCHAN_BCN_MULTI_IF)) {
			WL_MCHAN(("trigger idx change from %d to %d",
				mchan->trigger_cfg_idx, mchan->other_cfg_idx));
			/* swap the two indices */
			SWAP(mchan->trigger_cfg_idx, mchan->other_cfg_idx);
			mchan->trigg_algo = TRUE;
			return (BCME_NOTREADY);
		}
	}

	if ((wlc->stas_associated > 2) || (mchan->switch_algo == MCHAN_ALTERNATE_SWITCHING)) {
		if (!mchan->alternate_switching || mchan->trigg_algo) {
			err = wlc_mchan_alternate_switching(mchan, MCHAN_DEFAULT_BW);
		}
		return (err);
	}

	if (sel_bcn_per == oth_bcn_per) {
		bool tbtt_drifted = FALSE;

		/* Has beacon's alignment drifted beyond MCHAN_MAX_TBTT_GAP_DRIFT_MS ? */
		tbtt_drifted = !(mchan->tbtt_gap &&
			(ABS((int32)(mchan->tbtt_gap - mchan_algo_info->tbtt_gap)) <=
			MCHAN_MAX_TBTT_GAP_DRIFT_US));

		WL_MCHAN(("%s: tbtt_drift = %d\n", __FUNCTION__,
			ABS((int32)(mchan->tbtt_gap - mchan_algo_info->tbtt_gap))));

		if (bcn_position != mchan->bcn_position || switch_algo != mchan->switch_algo) {
			mchan->trigg_algo = TRUE;
		}

		if (mchan->alternate_switching && (mchan->trigg_algo == TRUE)) {
			mchan->alternate_switching = FALSE;
		}

		if (!tbtt_drifted && (mchan->trigg_algo == FALSE)) {
			/* schedule already setup */
			return err;
		}
	}

	/* Based on the switch algo invoke the respective function */
	err = wlc_mchan_algo_selection(mchan);

	return err;
}

/** Default algo for STA/GC case */
static int
wlc_mchan_sta_default_algo(mchan_info_t *mchan)
{
	wlc_info_t *wlc = mchan->wlc;
	int err = BCME_OK;
	uint32 curr_sel_tbtt, curr_oth_tbtt, next_sel_tbtt, next_oth_tbtt;
	uint32 sel_bcn_per, oth_bcn_per, tsf_h, tsf_l, tbtt_gap;
	uint8 cur_idx;
	uint32 other_start, other_dur;
	uint32 sel_dur, proc_time;
	uint32 start1, start2;
	bool special_return;
	bool use_sel_ctxt;
	uint32 next_toggle_switch_time = 0;
	mchan_bss_info_t *mbi_oth;
	wlc_bsscfg_t *bsscfg_oth;
	wlc_bsscfg_t *bsscfg_sel;
	wlc_mchan_algo_info_t *mchan_algo_info;

	mchan_algo_info = mchan->mchan_algo_info;

	wlc_mchan_populate_values(mchan, &curr_sel_tbtt, &curr_oth_tbtt, &next_sel_tbtt,
		&next_oth_tbtt, &oth_bcn_per, &sel_bcn_per, &tbtt_gap, &tsf_h, &tsf_l);

	proc_time = mchan_algo_info->proc_time;
	bsscfg_sel = WLC_BSSCFG(wlc, mchan->trigger_cfg_idx);
	bsscfg_oth = WLC_BSSCFG(wlc, mchan->other_cfg_idx);
	mbi_oth = MCHAN_BSS_INFO(mchan, bsscfg_oth);
	cur_idx = mchan->onchan_cfg_idx;

	/* figure out gap between the 2 to determine what to do next */
	special_return = 0;

	/* save the tbtt_gap value passed in */
	mchan->tbtt_gap = tbtt_gap;

	if (sel_bcn_per == oth_bcn_per) {
		/* (next_oth_tbtt - curr_sel_tbtt) and
		 * (curr_sel_tbtt - curr_oth_tbtt) are periodic here.
		 */
		/* update oth_bsscfg sw_dtim_cnt */
		mbi_oth->sw_dtim_cnt = (mbi_oth->sw_dtim_cnt) ?
			(mbi_oth->sw_dtim_cnt - 1) :
			(bsscfg_oth->current_bss->dtim_period - 1);

		if (tbtt_gap == (uint32)ABS((int32)(next_oth_tbtt - curr_sel_tbtt))) {
			next_toggle_switch_time = next_sel_tbtt;
		}
		else {
			next_toggle_switch_time = next_oth_tbtt;
		}
	}
	else {
		uint32 next_next_oth_tbtt = next_oth_tbtt + oth_bcn_per;

		/* For cases of different bcn_per, need to look at
		 * (next_sel_tbtt - next_oth_tbtt) and (next_next_oth_tbtt - next_sel_tbtt)
		 */
		if (next_oth_tbtt >= next_sel_tbtt) {
			if ((next_oth_tbtt - next_sel_tbtt) <=
				MCHAN_MIN_TBTT_GAP_US_NEW(mchan)) {
				special_return = 1;
			}
			/* reset next_oth_tbtt as if it is midway between sel_bcn_per */
			next_oth_tbtt = next_sel_tbtt - (sel_bcn_per >> 1);
			tbtt_gap = next_sel_tbtt - next_oth_tbtt;
		}
		else {
			/* update oth_bsscfg sw_dtim_cnt */
			mbi_oth->sw_dtim_cnt = (mbi_oth->sw_dtim_cnt) ?
				(mbi_oth->sw_dtim_cnt - 1) :
				(bsscfg_oth->current_bss->dtim_period - 1);

			if ((next_oth_tbtt - curr_sel_tbtt) <=
				MCHAN_MIN_TBTT_GAP_US_NEW(mchan)) {
				if ((next_next_oth_tbtt - next_sel_tbtt) <=
					MCHAN_MIN_TBTT_GAP_US_NEW(mchan)) {
					tbtt_gap = next_next_oth_tbtt - next_sel_tbtt;
					next_toggle_switch_time = next_sel_tbtt;
				}
				else {
					tbtt_gap = (uint32)(-1);
				}
			}
			else {
				tbtt_gap = next_oth_tbtt - curr_sel_tbtt;
				if ((next_sel_tbtt - next_oth_tbtt) <=
					MCHAN_MIN_TBTT_GAP_US_NEW(mchan)) {
					special_return = 1;
				}
			}
		}
	}

	WL_MCHAN(("%s: tbtt_gap = %d, n_sel_tbtt = 0x%x, c_sel_tbtt = 0x%x, "
		"n_oth_tbtt = 0x%x, c_oth_tbtt = 0x%x,"
		"next_toggle_switch_time = 0x%x\n",
		__FUNCTION__, tbtt_gap, next_sel_tbtt, curr_sel_tbtt, next_oth_tbtt,
		curr_oth_tbtt, next_toggle_switch_time));

	if (tbtt_gap == (uint32)(-1)) {
		return (err);
	}

	use_sel_ctxt = (cur_idx == mchan->other_cfg_idx);

	if (sel_bcn_per != oth_bcn_per) {
		if (tbtt_gap <= MCHAN_MIN_TBTT_GAP_US_NEW(mchan)) {
			/* add mchan sched element for bsscfg */
			wlc_mchan_sched_add(mchan, use_sel_ctxt ? bsscfg_sel : bsscfg_oth,
				sel_bcn_per, 2 * sel_bcn_per,
				next_toggle_switch_time - proc_time);
			wlc_mchan_sched_add(mchan, use_sel_ctxt ? bsscfg_oth : bsscfg_sel,
				sel_bcn_per, 2 * sel_bcn_per,
				next_toggle_switch_time + sel_bcn_per - proc_time);
		}
		else {
			/* current tbtts not too close to each other
			 * calculate a fair schedule
			 * XXX: refer to wlc_mchan.txt and note that
			 * minstart or minend times have a condition based
			 * on which tbtt is dtim
			 */
			wlc_mchan_calc_fair_chan_switch_sched(next_oth_tbtt,
				curr_sel_tbtt, next_sel_tbtt, sel_bcn_per, FALSE,
				&other_start, &other_dur);
			WL_MCHAN(("%s: sel_dur = %u, other_dur = %u\n",
				__FUNCTION__, sel_bcn_per - other_dur, other_dur));
			WL_MCHAN(("%s: oth_start = 0x%x, curr_sel_tbtt = 0x%x, sel_dur=%u\n",
				__FUNCTION__, other_start, curr_sel_tbtt,
				(other_start-curr_sel_tbtt)));

			/* This case means that we're switching
			 * midway to other channel and staying there
			 */
			if (special_return && !use_sel_ctxt) {
				wlc_mchan_sched_add(mchan, bsscfg_oth, sel_bcn_per,
					2 * sel_bcn_per, other_start - proc_time);
				wlc_mchan_sched_add(mchan, bsscfg_sel, sel_bcn_per,
					2 * sel_bcn_per, other_start + sel_bcn_per - proc_time);
			}
			/* This case means we switch midway and
			 * go back to our channel again
			 */
			else {
				/* add mchan sched element for other bsscfg */
				wlc_mchan_sched_add(mchan, bsscfg_oth, other_dur,
					sel_bcn_per, other_start - proc_time);
				wlc_mchan_sched_add(mchan, bsscfg_sel, sel_bcn_per - other_dur,
					sel_bcn_per, other_start + other_dur - proc_time);
			}
		}
	}
	else {
		if (tbtt_gap <= (uint32)MCHAN_MIN_TBTT_GAP_US_NEW(mchan)) {
			/* alternate switching based on default 50% bandwidth */
			err = wlc_mchan_alternate_switching(mchan, MCHAN_DEFAULT_BW);
		} else {
			/* current tbtts not too close to each other
			 * calculate a fair schedule
			 * XXX: refer to wlc_mchan.txt and note that minstart
			 * or minend times have a
			 * condition based on which tbtt is dtim.
			 */
			sel_dur = (sel_bcn_per / 2);
			if ((curr_sel_tbtt + sel_dur) < next_oth_tbtt) {
				start1 = curr_sel_tbtt + sel_dur;
			} else {
				start1 = next_oth_tbtt;
			}
			start2 = start1 + sel_dur;

			wlc_mchan_sched_add(mchan, bsscfg_oth, sel_dur, sel_bcn_per,
				start1 - proc_time);
			wlc_mchan_sched_add(mchan, bsscfg_sel, sel_dur, sel_bcn_per,
				start2 - proc_time);
		}
	}

	mchan->si_algo_last_switch_time = 0;

	return err;
}

/** Dynamic bandwidth algo for STA/GC case */
static int
wlc_mchan_sta_bandwidth_algo(mchan_info_t *mchan)
{
	wlc_info_t *wlc = mchan->wlc;
	int err = BCME_OK;
	uint32 proc_time;
	uint32 curr_sel_tbtt, curr_oth_tbtt, next_sel_tbtt, next_oth_tbtt;
	uint32 sel_bcn_per, oth_bcn_per, tsf_h, tsf_l, tbtt_gap;
	mchan_bss_info_t *mbi_oth;
	wlc_bsscfg_t *bsscfg_oth;
	wlc_bsscfg_t *bsscfg_sel;
	wlc_mchan_algo_info_t *mchan_algo_info;

	mchan_algo_info = mchan->mchan_algo_info;

	wlc_mchan_populate_values(mchan, &curr_sel_tbtt, &curr_oth_tbtt, &next_sel_tbtt,
		&next_oth_tbtt, &oth_bcn_per, &sel_bcn_per, &tbtt_gap, &tsf_h, &tsf_l);
	proc_time = mchan_algo_info->proc_time;

	bsscfg_sel = WLC_BSSCFG(wlc, mchan->trigger_cfg_idx);
	bsscfg_oth = WLC_BSSCFG(wlc, mchan->other_cfg_idx);
	mbi_oth = MCHAN_BSS_INFO(mchan, bsscfg_oth);
	mbi_oth->sw_dtim_cnt = (mbi_oth->sw_dtim_cnt) ?
		(mbi_oth->sw_dtim_cnt - 1) :
		(bsscfg_oth->current_bss->dtim_period - 1);

	/* save the tbtt_gap value passed in */
	mchan->tbtt_gap = tbtt_gap;

	if (sel_bcn_per < MCHAN_MIN_TBTT_GAP_US_NEW(mchan)) {
		ASSERT(0);
	} else if (sel_bcn_per < 2 * MCHAN_MIN_TBTT_GAP_US_NEW(mchan)) {
		mchan->switch_algo = MCHAN_ALTERNATE_SWITCHING;
		return (BCME_NOTREADY);
	} else {
		uint32 trigger_ch_percetage = (mchan->trigger_cfg_idx ==
				WLC_BSSCFG_IDX(wlc->cfg)) ?
			mchan->percentage_bw :
			100 - mchan->percentage_bw;

		if (tbtt_gap <= MCHAN_MIN_TBTT_GAP_US_NEW(mchan)) {
			if (trigger_ch_percetage < 30)
				trigger_ch_percetage = 30;
			if (trigger_ch_percetage > 70)
				trigger_ch_percetage = 70;
		}

		if (tbtt_gap < MCHAN_MIN_TBTT_GAP_US_NEW(mchan)) {
			/* alternate switching based on bandwidth desired */
			err = wlc_mchan_alternate_switching(mchan, trigger_ch_percetage);
		} else {
			uint32 trigger_ch_time, other_ch_time;
			uint32 switchtime_1, switchtime_2;
			trigger_ch_time = sel_bcn_per * trigger_ch_percetage / 100;
			other_ch_time = sel_bcn_per - trigger_ch_time;
			if (curr_sel_tbtt + trigger_ch_time < next_oth_tbtt) {
				switchtime_1 = curr_sel_tbtt + trigger_ch_time;
			} else {
				switchtime_1 = next_oth_tbtt;
			}
			switchtime_2 = switchtime_1 + other_ch_time;

			wlc_mchan_sched_add(mchan, bsscfg_oth, other_ch_time,
				sel_bcn_per, switchtime_1 - proc_time);
			wlc_mchan_sched_add(mchan, bsscfg_sel, trigger_ch_time,
				sel_bcn_per, switchtime_2 - proc_time);
		}
	}

	mchan->si_algo_last_switch_time = 0;

	return err;
}

/** SI algo for STA/GC case */
static int
wlc_mchan_sta_si_algo(mchan_info_t *mchan)
{
	wlc_info_t *wlc = mchan->wlc;
	int err = BCME_OK;
	uint32 proc_time;
	uint8 cur_idx, switch_idx = 0;
	uint32 curr_sel_tbtt, curr_oth_tbtt, next_sel_tbtt, next_oth_tbtt;
	uint32 sel_bcn_per, oth_bcn_per, tsf_h, tsf_l, tbtt_gap;
	uint32 last_switch_time = 0;
	uint32 next_tbtt_gap, next_switch_time;
	uint32 now;
	bool low_gap;
	uint32 rx_wait_time;
	bool same_bcn_inside, other_bcn_inside;
	uint8 cur_ch_idx, oth_ch_idx;
	uint32 cur_ch_tbtt, oth_ch_tbtt;
	uint32 next_switch_time_old;
	uint8 next_switch_idx;
	bool init_timer = FALSE;
	wlc_bsscfg_t *bsscfg_cur, *bsscfg_othr;
	uint32 sel_tbtt, oth_tbtt;
	wlc_bsscfg_t *bsscfg_oth;
	wlc_bsscfg_t *bsscfg_sel;
	mchan_bss_info_t *mbi_oth;
	uint32 sched_time[4], cnt = 0;

	wlc_mchan_populate_values(mchan, &curr_sel_tbtt, &curr_oth_tbtt, &next_sel_tbtt,
			&next_oth_tbtt, &oth_bcn_per, &sel_bcn_per, &tbtt_gap, &tsf_h, &tsf_l);
	proc_time = mchan->proc_time_us;

	now = tsf_l;
	bsscfg_sel = WLC_BSSCFG(wlc, mchan->trigger_cfg_idx);
	bsscfg_oth = WLC_BSSCFG(wlc, mchan->other_cfg_idx);
	mbi_oth = MCHAN_BSS_INFO(mchan, bsscfg_oth);
	cur_idx = mchan->onchan_cfg_idx;

	sel_tbtt = curr_sel_tbtt;
	if (curr_oth_tbtt > now) {
		oth_tbtt = curr_oth_tbtt;
	} else {
		oth_tbtt = next_oth_tbtt;
	}

	if (((curr_sel_tbtt < curr_oth_tbtt) && (curr_oth_tbtt < next_sel_tbtt)) ||
			((curr_sel_tbtt < next_oth_tbtt) && (next_oth_tbtt < next_sel_tbtt)))
		mbi_oth->sw_dtim_cnt = (mbi_oth->sw_dtim_cnt) ?
			(mbi_oth->sw_dtim_cnt - 1) :
			(bsscfg_oth->current_bss->dtim_period - 1);

	/* Check whether the last_switch_time value is valid or not -
	 * The value is expected to be greater than the tbtt time
	 */
	last_switch_time =  mchan->si_algo_last_switch_time;
	if (last_switch_time == 0 || last_switch_time < now)
		last_switch_time = now;

	next_switch_time = last_switch_time;

	if (last_switch_time > now && last_switch_time < next_sel_tbtt) {
		next_switch_idx = (mchan->trigger_cfg_idx == cur_idx) ?
			mchan->other_cfg_idx : mchan->trigger_cfg_idx;
		init_timer = TRUE;
		switch_idx = next_switch_idx;

	} else {
		next_switch_idx = cur_idx;
	}

	next_switch_time_old = next_switch_time;

	cur_ch_tbtt = (next_switch_idx == mchan->trigger_cfg_idx) ? sel_tbtt : oth_tbtt;
	cur_ch_idx = (next_switch_idx == mchan->trigger_cfg_idx) ?
		mchan->trigger_cfg_idx : mchan->other_cfg_idx;
	oth_ch_tbtt = (next_switch_idx == mchan->trigger_cfg_idx) ? oth_tbtt : sel_tbtt;
	oth_ch_idx = (next_switch_idx == mchan->trigger_cfg_idx) ?
		mchan->other_cfg_idx : mchan->trigger_cfg_idx;
	bsscfg_cur =  (next_switch_idx == mchan->trigger_cfg_idx) ?
		bsscfg_sel : bsscfg_oth;
	bsscfg_othr =  (next_switch_idx == mchan->trigger_cfg_idx) ?
		bsscfg_oth : bsscfg_sel;

	while (cnt < 4) {
		next_switch_idx = (mchan->trigger_cfg_idx == next_switch_idx) ?
			mchan->other_cfg_idx : mchan->trigger_cfg_idx;
		if (!init_timer) {
			init_timer = TRUE;
			switch_idx = next_switch_idx;
		} else {
			sched_time[cnt++] = next_switch_time_old;
		}

		next_tbtt_gap = ABS((int32)(oth_ch_tbtt - cur_ch_tbtt));

		rx_wait_time = MCHAN_MIN_BCN_RX_TIME_US;
		rx_wait_time += WLC_MCHAN_PRETBTT_TIME_US(mchan);

		low_gap = (next_tbtt_gap < (rx_wait_time));

		same_bcn_inside = (cur_ch_tbtt > next_switch_time) &&
			(cur_ch_tbtt < (next_switch_time + mchan->switch_interval +
			MCHAN_MIN_CHANNEL_PERIOD(mchan)));
		other_bcn_inside = (oth_ch_tbtt > next_switch_time) &&
			(oth_ch_tbtt < (next_switch_time + mchan->switch_interval));

		if (low_gap) {
			if (!(same_bcn_inside || other_bcn_inside)) {
				next_switch_time = MAX(next_switch_time +
						mchan->switch_interval, now);
			} else {
				next_switch_time = MAX(next_switch_time +
						mchan->switch_interval,
						cur_ch_tbtt + rx_wait_time);
			}
		} else {
			if (!(same_bcn_inside || other_bcn_inside)) {
				next_switch_time = MAX(mchan->switch_interval +
						next_switch_time, now);
			} else if (other_bcn_inside && !same_bcn_inside) {
				if (oth_ch_tbtt > next_switch_time +
						MCHAN_MIN_CHANNEL_PERIOD(mchan)) {
					next_switch_time = oth_ch_tbtt;
				} else {
					next_switch_time = next_switch_time +
						mchan->switch_interval;
				}
			} else if (!other_bcn_inside && same_bcn_inside) {
				if (oth_ch_tbtt > cur_ch_tbtt) {
					next_switch_time = MIN(oth_ch_tbtt,
							MAX(next_switch_time +
							mchan->switch_interval,
							cur_ch_tbtt + rx_wait_time));
				} else {
					next_switch_time = MAX(next_switch_time +
							mchan->switch_interval,
							cur_ch_tbtt + rx_wait_time);
				}
			} else {
				if (oth_ch_tbtt < next_switch_time +
						MCHAN_MIN_CHANNEL_PERIOD(mchan)) {
					next_switch_time = MAX(next_switch_time +
							mchan->switch_interval,
							cur_ch_tbtt + rx_wait_time);
				} else {
					next_switch_time = oth_ch_tbtt;
				}
			}
		}

		if (cur_ch_tbtt > next_switch_time_old && cur_ch_tbtt < next_switch_time)
			cur_ch_tbtt = cur_ch_tbtt +
				(bsscfg_cur->current_bss->beacon_period << 10);
		if (oth_ch_tbtt > next_switch_time_old && oth_ch_tbtt < next_switch_time)
			oth_ch_tbtt = oth_ch_tbtt +
				(bsscfg_othr->current_bss->beacon_period << 10);

		if (next_switch_time > next_sel_tbtt) {
			mchan->si_algo_last_switch_time = next_switch_time;
			break;
		}

		next_switch_time_old = next_switch_time;
		last_switch_time = next_switch_time;

		SWAP(cur_ch_tbtt, oth_ch_tbtt);
		SWAP(cur_ch_idx, oth_ch_idx);
		SWAP_PTR(wlc_bsscfg_t, bsscfg_cur, bsscfg_othr);
	}

	WL_MCHAN(("%s: cnt = %d, switch_id %d: time %u, %u, %u, %u\n",
		__FUNCTION__, cnt, switch_idx, cnt > 0? sched_time[0] - now : 0,
		cnt > 1? sched_time[1] - now : 0,
		cnt > 2? sched_time[2] - now : 0,
		cnt > 3? sched_time[3] - now : 0));

	if (cnt > 1) {
		wlc_mchan_sched_add(mchan, (switch_idx == mchan->trigger_cfg_idx)?
			bsscfg_sel : bsscfg_oth, sched_time[1] - sched_time[0],
			(cnt > 2)? sched_time[2] - sched_time[0]: sel_bcn_per,
			sched_time[0] - proc_time);
		wlc_mchan_sched_add(mchan, (switch_idx == mchan->trigger_cfg_idx)?
			bsscfg_oth : bsscfg_sel, (cnt > 2)? (sched_time[1] - sched_time[0]) :
			(sel_bcn_per - (sched_time[1] - sched_time[0])),
			(cnt > 3)? sched_time[3] - sched_time[1]: sel_bcn_per,
			sched_time[1] - proc_time);
	}

	/* save the tbtt_gap value passed in */
	mchan->tbtt_gap = (cnt == 2)? tbtt_gap : 0;

	return err;
}

static int
wlc_mchan_alternate_switching(mchan_info_t *mchan, uint32 trigger_bw)
{
	wlc_info_t *wlc = mchan->wlc;
	uint32 proc_time;
	uint32 curr_sel_tbtt, curr_oth_tbtt, next_sel_tbtt, next_oth_tbtt;
	uint32 sel_bcn_per, oth_bcn_per, tsf_h, tsf_l, tbtt_gap;
	wlc_bsscfg_t *bsscfg_oth;
	wlc_bsscfg_t *bsscfg_sel;
	wlc_mchan_algo_info_t *mchan_algo_info;
	uint32 trigger_ch_time, other_ch_time;
	uint32 switchtime_1, switchtime_2;

	mchan_algo_info = mchan->mchan_algo_info;

	wlc_mchan_populate_values(mchan, &curr_sel_tbtt, &curr_oth_tbtt, &next_sel_tbtt,
			&next_oth_tbtt, &oth_bcn_per, &sel_bcn_per, &tbtt_gap, &tsf_h, &tsf_l);
	proc_time = mchan_algo_info->proc_time;

	mchan->alternate_switching = TRUE;
	mchan->trigg_algo = FALSE;

	bsscfg_sel = WLC_BSSCFG(wlc, mchan->trigger_cfg_idx);
	bsscfg_oth = WLC_BSSCFG(wlc, mchan->other_cfg_idx);

	trigger_ch_time = 2 * sel_bcn_per * trigger_bw/100;
	other_ch_time = (2 * sel_bcn_per) - trigger_ch_time;

	if (trigger_ch_time >= other_ch_time) {
		if (tbtt_gap == ABS((int32)(next_oth_tbtt - curr_sel_tbtt))) {
			switchtime_1 = next_sel_tbtt;
			switchtime_2 = switchtime_1 + other_ch_time;
			wlc_mchan_sched_add(mchan, bsscfg_oth,
				other_ch_time, 2 * sel_bcn_per,
				switchtime_1 - proc_time);
			wlc_mchan_sched_add(mchan, bsscfg_sel,
				trigger_ch_time, 2 * sel_bcn_per,
				switchtime_2 - proc_time);
		} else {
			switchtime_1 = next_oth_tbtt;
			switchtime_2 = switchtime_1 + other_ch_time;
			wlc_mchan_sched_add(mchan, bsscfg_oth,
				other_ch_time, 2 * sel_bcn_per,
				switchtime_1 - proc_time);
			wlc_mchan_sched_add(mchan, bsscfg_sel,
				trigger_ch_time, 2 * sel_bcn_per,
				switchtime_2 - proc_time);
		}
	}
	else {
		/* trigger_ch_time < other_ch_time */
		if (tbtt_gap == ABS((int32)(next_oth_tbtt - curr_sel_tbtt))) {
			switchtime_1 = next_sel_tbtt;
			switchtime_2 = switchtime_1 + trigger_ch_time;
			wlc_mchan_sched_add(mchan, bsscfg_sel,
				trigger_ch_time, 2 * sel_bcn_per,
				switchtime_1 - proc_time);
			wlc_mchan_sched_add(mchan, bsscfg_oth,
				other_ch_time, 2 * sel_bcn_per,
				switchtime_2 - proc_time);
		} else {
			switchtime_1 = next_oth_tbtt;
			switchtime_2 = switchtime_1 + trigger_ch_time;
			wlc_mchan_sched_add(mchan, bsscfg_sel,
				trigger_ch_time, 2 * sel_bcn_per,
				switchtime_1 - proc_time);
			wlc_mchan_sched_add(mchan, bsscfg_oth,
				other_ch_time, 2 * sel_bcn_per,
				switchtime_2 - proc_time);
		}
	}
	return BCME_OK;
}

bool
wlc_mchan_check_tbtt_valid(mchan_info_t *mchan)
{
	wlc_info_t *wlc = mchan->wlc;
	if (MCHAN_ACTIVE(wlc->pub)) {
		wlc_bsscfg_t* trigg_cfg;
		wlc_bsscfg_t* oth_cfg;

		trigg_cfg = WLC_BSSCFG(wlc, mchan->trigger_cfg_idx);
		oth_cfg = WLC_BSSCFG(wlc, mchan->other_cfg_idx);
		ASSERT(trigg_cfg && oth_cfg);

		if (wlc_mcnx_tbtt_valid(wlc->mcnx, trigg_cfg) &&
			wlc_mcnx_tbtt_valid(wlc->mcnx, oth_cfg)) {
			return TRUE;
		} else {
			return FALSE;
		}
	} else {
		return TRUE;
	}
}

static bool
wlc_mchan_wait_tbtt_valid(mchan_info_t *mchan)
{
	wlc_info_t* wlc = mchan->wlc;

	if (!MCHAN_ACTIVE(wlc->pub))
		return TRUE;
	else {
		if (wlc_mchan_check_tbtt_valid(mchan)) {
			/* both cfg have valid tbtt, reset and return */
			mchan->wait_sta_valid = 0;
			return FALSE;
		} else {
			wlc_bsscfg_t* trigg_cfg = NULL;
			wlc_bsscfg_t* oth_cfg = NULL;

			trigg_cfg = WLC_BSSCFG(wlc, mchan->trigger_cfg_idx);
			oth_cfg = WLC_BSSCFG(wlc, mchan->other_cfg_idx);
			ASSERT(trigg_cfg && oth_cfg);

			if (!wlc_mcnx_tbtt_valid(wlc->mcnx, trigg_cfg)) {
				if (mchan->wait_sta_valid < MAX_TBTT_VALID_WAIT) {
					wlc_mchan_switch_chan_context(mchan, trigg_cfg, oth_cfg);
					mchan->wait_sta_valid++;
				}
				else {
					/* TRIGG bcn not found after repeated trials.
					 * switch back to the OTH channel and let roam occur
					 */
					WL_MCHAN(("wl%d.%d: %s: Trigg CFG not found, "
						"delete chanctx for 0x%x\n",
						wlc->pub->unit, WLC_BSSCFG_IDX(trigg_cfg),
						__FUNCTION__, trigg_cfg->current_bss->chanspec));
					wlc_mchan_switch_chan_context(mchan, oth_cfg, trigg_cfg);
				}
			} else if (!wlc_mcnx_tbtt_valid(wlc->mcnx, oth_cfg)) {
				if (mchan->wait_sta_valid < MAX_TBTT_VALID_WAIT) {
					wlc_mchan_switch_chan_context(mchan, oth_cfg, trigg_cfg);
					mchan->wait_sta_valid++;
				}
				else {
					/* OTH bcn not found after repeated trials.
					 * switch back to the TRIG channel and let roam occur
					 */
					WL_MCHAN(("wl%d.%d: %s: Other CFG not found, "
						"delete chanctx for 0x%x\n",
						wlc->pub->unit, WLC_BSSCFG_IDX(oth_cfg),
						__FUNCTION__, oth_cfg->current_bss->chanspec));
					wlc_mchan_switch_chan_context(mchan, trigg_cfg, oth_cfg);
				}
			}
			return TRUE;
		}
	}
}

static void
wlc_mchan_pretbtt_cb(void *ctx, wlc_mcnx_intr_data_t *notif_data)
{
	if (notif_data->intr != M_P2P_I_PRE_TBTT)
		return;

	/* prevent unnecessary local variable initialization
	 *in case not M_P2P_I_PRE_TBTT
	 */
	_wlc_mchan_pretbtt_cb(ctx, notif_data);
}

static void wlc_mchan_blocking_bsscfg(mchan_info_t *mchan)
{
	if (mchan->blocking_bsscfg && !SCAN_IN_PROGRESS(mchan->wlc->scan)) {
#ifdef STB_SOC_WIFI
		if (mchan->blocking_cnt == MCHAN_BLOCKING_CNT) {
			wlc_mchan_switch_chan_context(mchan, mchan->blocking_bsscfg,
				wlc_mchan_get_other_cfg_frm_cur(mchan->wlc,
				mchan->blocking_bsscfg));
		}
#else
		wlc_mchan_switch_chan_context(mchan, mchan->blocking_bsscfg,
			wlc_mchan_get_other_cfg_frm_cur(mchan->wlc, mchan->blocking_bsscfg));
#endif // endif
		mchan->blocking_cnt--;
		if (mchan->blocking_cnt == 0) {
			WL_MCHAN(("wl%d %s: chan switch blocked by bss %d\n",
				mchan->wlc->pub->unit,  __FUNCTION__,
				WLC_BSSCFG_IDX(mchan->blocking_bsscfg)));
			mchan->blocking_bsscfg = NULL;
		}
	}
	else {
		mchan->blocking_bsscfg = NULL;
		mchan->blocking_cnt = 0;
	}
}

static void
wlc_mchan_set_adjusted_for_nchan(mchan_info_t *mchan)
{
	mchan->adjusted_for_nchan = TRUE;
	mchan->timer_running = FALSE;
	mchan->blocking_bsscfg = NULL;
	mchan->blocking_cnt = 0;
	mchan->trigg_algo = TRUE;
}

static void
_wlc_mchan_pretbtt_cb(void *ctx, wlc_mcnx_intr_data_t *notif_data)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;
	mchan_info_t *mchan = wlc->mchan;
	wlc_bsscfg_t *bsscfg = notif_data->cfg;
	uint32 tsf_h = notif_data->tsf_h;
	uint32 tsf_l = notif_data->tsf_l;
	mchan_bss_info_t *mbi = MCHAN_BSS_INFO(mchan, bsscfg);

	if (!(MCHAN_ENAB(wlc->pub) && MCHAN_ACTIVE(wlc->pub))) {
		return;
	}

	if (BSSCFG_SPECIAL(wlc, bsscfg) || !bsscfg->BSS) {
		/* no need to handle this case */
		return;
	}

	if (wlc_mchan_wait_tbtt_valid(mchan))
		return;

	/* if bsscfg missed too many beacons, block 1 chan switch to help
	 * with bcn reception.
	 * Only do this if there are no AP's active.
	 * If AP's active, then we don't want to block channel switch to AP channel
	 */
	if (!AP_ACTIVE(wlc) &&
	    (mbi->mchan_tbtt_since_bcn > mchan->tbtt_since_bcn_thresh) &&
	    (mchan->blocking_bsscfg == NULL) &&
	    (!wlc_mchan_client_noa_valid(mchan) || P2P_CLIENT(wlc, bsscfg))) {
		WL_MCHAN(("wl%d.%d: need to block chansw, mchan_tbtt_since_bcn = %u\n",
		wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg),	mbi->mchan_tbtt_since_bcn));
		mchan->blocking_bsscfg = bsscfg;
		mchan->blocking_cnt = MCHAN_BLOCKING_CNT;
	}

#ifdef STA
	/* Start monitoring at tick 1 */
	if (BSSCFG_STA(bsscfg) && bsscfg->BSS && mchan->blocking_enab) {
		mbi->mchan_tbtt_since_bcn++;
	}
#endif // endif

	if (wlc_mchan_client_noa_valid(mchan)) {
		if (!mchan->adjusted_for_nchan) {
			mchan->trigg_algo = TRUE;
		}
		return;
	}
	else {
		/* this will occur for the case when GO associated to GC+STA device
		* comes out of NOA. in that case we want to reset the N-Channel params
		* on the GC +STA device
		*/
		if (mchan->adjusted_for_nchan)
			wlc_mchan_reset_params(mchan);
	}

	if (mchan->trigger_cfg_idx != WLC_BSSCFG_IDX(bsscfg)) {
		return;
	}

	/* check for blocking_bsscfg */
	if (mchan->blocking_bsscfg) {
		wlc_mchan_blocking_bsscfg(mchan);
	}
	else if (BSSCFG_AP(bsscfg)) {
		wlc_mchan_ap_pretbtt_proc(wlc, bsscfg, tsf_l, tsf_h);
	}
	else {
		wlc_mchan_sta_pretbtt_proc(wlc, bsscfg, tsf_l, tsf_h);
	}
}

void
wlc_mchan_recv_process_beacon(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb,
	wlc_d11rxhdr_t *wrxh, uint8 *plcp, uint8 *body, int bcn_len)
{
	if (scb) {
		mchan_info_t *mchan = wlc->mchan;
		mchan_bss_info_t *mbi;

		BCM_REFERENCE(bcn_len);

		ASSERT(bsscfg != NULL);
		ASSERT(BSSCFG_STA(bsscfg) && bsscfg->BSS);

		mbi = MCHAN_BSS_INFO(mchan, bsscfg);

		if (mbi->mchan_tbtt_since_bcn > mchan->tbtt_since_bcn_thresh) {
			/* force p2p module to update tbtt */
			wlc_mcnx_tbtt_inv(wlc->mcnx, bsscfg);
			WL_MCHAN(("wl%d.%d: force tbtt upd, mchan_tbtt_since_bcn = %u\n",
			          wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg),
			          mbi->mchan_tbtt_since_bcn));
		}
		if (bsscfg == mchan->blocking_bsscfg) {
			wlc_mchan_reset_blocking_bsscfg(mchan);
		}
		mbi->mchan_tbtt_since_bcn = 0;
	}
}

void
wlc_mchan_client_noa_clear(mchan_info_t *mchan, wlc_bsscfg_t *cfg)
{
	mchan_bss_info_t *mbi = MCHAN_BSS_INFO(mchan, cfg);

	WL_MCHAN(("wl%d.%d: %s: clean up mchan abs/psc states\n", mchan->wlc->pub->unit,
		WLC_BSSCFG_IDX(cfg), __FUNCTION__));
	mchan->timer_running = FALSE;
	wlc_mchan_psc_cleanup(mchan, cfg);
	mbi->in_psc = FALSE;
}

static int wlc_mchan_if_time(wlc_info_t *wlc, wlc_bsscfg_t *curr_cfg, wlc_bsscfg_t *other_cfg)
{
	mchan_info_t *mchan = wlc->mchan;
	mchan_bss_info_t *mbi, *obmi; /* Current bss and other bss info */
	uint32 tx_time = 0;
	uint32 rx_time = 0;
	uint32 air_time = 0;
	cca_ucode_counts_t if_new;
	cca_ucode_counts_t if_old;
	uint32 now = R_REG(wlc->osh, D11_TSFTimerLow(wlc));

	if (curr_cfg && other_cfg) {
		/* Current bss mchan info "just switched" current channel */
		mbi = MCHAN_BSS_INFO(mchan, curr_cfg);
		/* Other bss mchan info */
		obmi = MCHAN_BSS_INFO(mchan, other_cfg);
	}
	else {
		return BCME_BADARG;
	}
	/* Populate the "other bss" statistics */
	if (mbi && obmi && mbi->air_stats && obmi->air_stats) {
		mchan_airtime_stats_t *bss_air_stats = mbi->air_stats;
		mchan_airtime_stats_t *otherbss_air_stats = obmi->air_stats;
		uint32 old_rts = 0;
		uint32 new_rts = 0;
		uint32 rts_diff = 0;
		uint8 index;
		uint16 sifs = SIFS(wlc->band->bandtype);

		wlc_statsupd(wlc);
		sifs *= 3;

		/* old_rts = old bss RTS captured in last iteration */
		old_rts = otherbss_air_stats->rts;
		/* if_new is current bss RTS captured immediately after channel switch */
		new_rts = wlc->pub->_cnt->txrts;
		rts_diff = new_rts - old_rts;

		/* if_old = old bss statistics captured in last iteration */
		if_old = otherbss_air_stats->if_count;
		wlc_bmac_cca_stats_read(wlc->hw, &bss_air_stats->if_count);
		/* if_new is current bss statistics captured immediately after channel switch */
		if_new = bss_air_stats->if_count;

		/* other_bss_statistics = if_new - if_old */
		tx_time = if_new.txdur - if_old.txdur;
		/* RX time without obss */

		/* rx_time = (if_new.ibss - if_old.ibss ); */
		rx_time = (if_new.ibss + if_new.obss) - (if_old.ibss + if_old.obss);
		air_time = tx_time + rx_time;

		/* Removing RTS + CTS + BA + (3*SIFS) time from air_time */
		air_time -= ((rts_diff * MCHAN_RTS_TIME_uSEC) + (rts_diff * sifs));
		index = (uint8)otherbss_air_stats->sample;
		otherbss_air_stats->time[index] = air_time;
		otherbss_air_stats->period[index] = now - mchan->chansw_timestamp;
		bss_air_stats->rts = new_rts;
		otherbss_air_stats->sample = (((index) +
			1) % MCHAN_DYN_ALGO_SAMPLE);
		bss_air_stats->sample = (((index) +
			1) % MCHAN_DYN_ALGO_SAMPLE);
	}
	return BCME_OK;
}

/* Function to compute the load on a per BSS basis
* The Load is calculated as (Air_time / Period) and is
* stored in the corresponding BMI
*/
static void
wlc_mchan_calculate_bss_load(mchan_info_t * mchan, mchan_bss_info_t * bmi)
{
	uint8 sample;
	uint32 if_txbytes = 0;
	uint32 if_rxbytes = 0;
	uint32 if_totbytes = 0;
#ifdef WLRSDB
	uint32 if_air_time = 0;
#endif // endif
	/* Find avg values of data collected over last MCHAN_DYN_ALGO_SAMPLE samples */
	if (MCHAN_ENAB(mchan->wlc->pub) && MCHAN_ACTIVE(mchan->wlc->pub)) {
		mchan_if_bytes_t *byte_stats = bmi->bytes_stats;
		/* Find avg values of data collected over last MCHAN_DYN_ALGO_SAMPLE samples */
		for (sample = 0; sample < MCHAN_DYN_ALGO_SAMPLE; sample++) {
			 if_txbytes += (byte_stats->txbytes[sample]
			    / MCHAN_DYN_ALGO_SAMPLE);
			 if_rxbytes += (byte_stats->rxbytes[sample]
			    / MCHAN_DYN_ALGO_SAMPLE);
		}
		/* Calculate Total Bytes on each interface as RX+TX */
		if_totbytes += (if_txbytes + if_rxbytes);
		bmi->load = if_totbytes;
	}
#ifdef WLRSDB
	else if ((RSDB_ENAB(mchan->wlc->pub)) && (RSDB_ACTIVE(mchan->wlc->pub)))
	{ /* RSDB CASE */
		mchan_airtime_stats_t *bss_airstats = bmi->air_stats;
		if (bss_airstats->sample == 0) {
			sample = (MCHAN_DYN_ALGO_SAMPLE - 1);
		}
		else {
			sample = (bss_airstats->sample - 1);
		}
		if_air_time = bss_airstats->time[sample];
		bmi->load = ((if_air_time * 100) / bss_airstats->period[sample]);
	}
#endif // endif
}

#if defined(WL_MODESW) && defined(WLRSDB)
/* this function clears all the dynamic mode sw related flags */
static void
wlc_mchan_dyn_modesw_init_params(mchan_info_t *mchan, bool bw_init)
{
	wlc_info_t* wlc = mchan->wlc;
	bool primary_cfg_req_modesw = FALSE;
	wlc_bsscfg_t *cfg;
	uint8 cfg_idx;

	if (BCME_ERROR != wlc_mchan_get_cfg_req_modesw(mchan, wlc_bsscfg_primary(wlc),
		&primary_cfg_req_modesw)) {
		if (bw_init) {
			mchan->percentage_bw = (primary_cfg_req_modesw) ?
				MCHAN_MAX_PERCENTAGE_BW : MCHAN_MIN_PERCENTAGE_BW;
			FOREACH_AS_BSS(wlc, cfg_idx, cfg) {
				wlc_mchan_set_cfg_req_modesw(mchan, cfg, FALSE);
			}
			wlc_set_modesw_bw_init(wlc->modesw, FALSE);
		}
		mchan->dyn_algo_params->last_inc_is_primary = FALSE;
		wlc_mchan_set_dyn_algo_params(mchan, FALSE, FALSE, FALSE, 0);
		wlc_modesw_set_sample_dur(wlc->modesw, 0);
	} else {
		WL_ERROR(("%s get_cfg_req_modesw failed\n", __FUNCTION__));
		ASSERT(0);
	}
}

/* this function sets the value which would specify
 * that the cfg passed is the one with max load req
 */
static void
wlc_mchan_set_cfg_req_modesw(mchan_info_t* mchan, wlc_bsscfg_t *bsscfg,
    bool setflag)
{
	mchan_bss_info_t* bmi;

	bmi = MCHAN_BSS_INFO(mchan, bsscfg);
	if (bmi != NULL) {
		bmi->cfg_req_modesw = setflag;
	}
}

/* this function returns the value which would specify
 * whether the cfg passed is the one with max load req
 */
static int
wlc_mchan_get_cfg_req_modesw(mchan_info_t* mchan, wlc_bsscfg_t *bsscfg,
    bool* val)
{
	mchan_bss_info_t* bmi;

	bmi = MCHAN_BSS_INFO(mchan, bsscfg);
	if (bmi != NULL) {
		*val = bmi->cfg_req_modesw;
		return BCME_OK;
	} else {
		ASSERT(bmi != NULL);
		return BCME_ERROR;
	}
}

#endif /* WL_MODESW && WLRSDB */
static void
wlc_mchan_set_dyn_algo_params(mchan_info_t *mchan, bool bw_init,
    bool bw_one_if_init, bool slowupd, uint8 algo_cntr)
{
	mchan->dyn_algo_params->bw_init_flag = bw_init;
	mchan->dyn_algo_params->slow_upd_phase = slowupd;
	mchan->dyn_algo_params->dyn_algo_counter = algo_cntr;
	mchan->dyn_algo_params->bw_init_oneif_tp = bw_one_if_init;
}

static int
wlc_mchan_vsdb_dyn_algo(mchan_info_t *mchan)
{
	wlc_info_t * wlc = mchan->wlc;
	uint32 primary_bss_load = 0, secondary_bss_load = 0;
	mchan_bss_info_t *bmi;
	wlc_bsscfg_t *cfg;
	uint8 cfg_idx;
	mchan_dynalgo_params_info_t* dyn_algo_params;
	uint8 action;
	bool act_on_primary = FALSE;

	dyn_algo_params = mchan->dyn_algo_params;

	FOREACH_AS_BSS(wlc, cfg_idx, cfg) {
		bmi = MCHAN_BSS_INFO(mchan, cfg);
		if (bmi) {
			wlc_mchan_calculate_bss_load(mchan, bmi);
			/* below assignment to be changed in case there are more */
			/* interfaces/channels	to be compared */
			if (cfg == wlc_bsscfg_primary(wlc)) {
				primary_bss_load = bmi->load;
			} else {
				secondary_bss_load = bmi->load;
			}
		}
	}
	WL_MCHAN(("%s secondary_load %d primary_load %d\n",
		__FUNCTION__, secondary_bss_load, primary_bss_load));
	if (MCHAN_ACTIVE(wlc->pub)) {
#if defined(WLRSDB)&& defined(WL_MODESW)
		/* If we switched from RSDB to VSDB implies that there
		 * was a max load requirement on one of the interfaces,
		 * provide maximum bandwidth to that Interface
		 */
		if (RSDB_ENAB(wlc->pub) && WLC_MODESW_ENAB(wlc->pub) &&
			wlc_get_modesw_bw_init(wlc->modesw)) {
			wlc_mchan_dyn_modesw_init_params(mchan, TRUE);
			return BCME_NOTREADY;
		}
#endif /* WLRSDB && WL_MODESW */

		if (dyn_algo_params->dyn_algo_counter < 3 &&
			(dyn_algo_params->bw_init_flag ||
			dyn_algo_params->bw_init_oneif_tp)) {
			dyn_algo_params->dyn_algo_counter++;
			return BCME_NOTREADY;
		}

#if defined(WLRSDB)&& defined(WL_MODESW)
		if (RSDB_ENAB(wlc->pub) && WLC_MODESW_ENAB(wlc->pub)) {
			if ((wlc_modesw_get_sample_dur(wlc->modesw)
				== MODESW_V2R_SAMPLE_DURATION) ||
#if defined(WLCNT)
				(WME_ENAB(wlc->pub) ?
				(wlc_mchan_is_qos_traffic_present(wlc->mchan)) : FALSE) ||
#endif /* WLCNT */
				FALSE) {
				wlc_rsdb_dyn_switch(wlc, MODE_RSDB);
				wlc_mchan_dyn_modesw_init_params(mchan, FALSE);
				return BCME_OK;
			} else if ((mchan->percentage_bw > MCHAN_V2R_SW_LOAD_MARK) ||
				((100 - mchan->percentage_bw) > MCHAN_V2R_SW_LOAD_MARK)) {
				wlc_modesw_set_sample_dur(wlc->modesw, 0);
			} else {
				wlc_modesw_set_sample_dur(wlc->modesw,
					wlc_modesw_get_sample_dur(wlc->modesw) + 1);
			}
		}
#endif /* WLRSDB && WL_MODESW */

		action = wlc_vsdb_dyn_algo_get_action(mchan, &act_on_primary,
				primary_bss_load, secondary_bss_load);

		switch (action) {
			case MCHAN_BW_INC_TYPE_FAST:
				wlc_mchan_inc_bw(mchan, MCHAN_BW_INC_TYPE_FAST, act_on_primary);
				break;
			case MCHAN_BW_INC_TYPE_REG:
				wlc_mchan_inc_bw(mchan, MCHAN_BW_INC_TYPE_REG, act_on_primary);
				break;
			case MCHAN_BW_INC_TYPE_SLOW:
				wlc_mchan_inc_bw(mchan, MCHAN_BW_INC_TYPE_SLOW, act_on_primary);
				break;
			case MCHAN_BW_INIT_NO_LOAD:
				mchan->percentage_bw = 50;
				mchan->trigg_algo = TRUE;
				wlc_mchan_set_dyn_algo_params(mchan, FALSE, FALSE, FALSE, 0);
				break;
			case MCHAN_BW_INIT:
				mchan->percentage_bw = 50;
				mchan->trigg_algo = TRUE;
				wlc_mchan_set_dyn_algo_params(mchan, TRUE, FALSE, FALSE, 0);
				break;
			case MCHAN_BW_INC_LOAD_ANOMALY:
				if (!mchan->dyn_algo_params->anomaly) {
					mchan->dyn_algo_params->anomaly = TRUE;
					mchan->dyn_algo_params->dyn_algo_counter = 1;
				} else {
					mchan->dyn_algo_params->last_inc_is_primary =
					((mchan->percentage_bw > mchan->dyn_algo_params->saved_bw) ?
						FALSE : TRUE);
					mchan->percentage_bw = mchan->dyn_algo_params->saved_bw;
					mchan->trigg_algo = TRUE;
					mchan->dyn_algo_params->dyn_algo_counter = 0;
				}
				break;
			case MCHAN_BW_INIT_ONE_IF_TP:
				mchan->percentage_bw = 50;
				mchan->trigg_algo = TRUE;
				wlc_mchan_set_dyn_algo_params(mchan, FALSE, TRUE, FALSE, 0);
				break;
			case MCHAN_BW_INC_NO_ACTION:
				break;
			default:
				WL_MCHAN(("%s Unhandled action %d\n", __FUNCTION__, action));
				break;
		}
		dyn_algo_params->saved_bw = (uint8)mchan->percentage_bw;
		if (!dyn_algo_params->anomaly) {
			dyn_algo_params->primary_bss_load_old = primary_bss_load;
			dyn_algo_params->secondary_bss_load_old = secondary_bss_load;
		}
	}
	return BCME_OK;
}

/* function decides whether bw has to be increased or decreased based
 * on set of conditions
 */
static mchan_dyn_algo_action
wlc_vsdb_dyn_algo_get_action(mchan_info_t *mchan,
    bool *act_on_primary, uint32 prim_bss_load, uint32 sec_bss_load)
{
	uint8 slow_phase = mchan->dyn_algo_params->slow_upd_phase;
	mchan_dyn_algo_action action_to_take = MCHAN_BW_INC_NO_ACTION;
	mchan_dynalgo_params_info_t *dyn_algo_params;

	dyn_algo_params = mchan->dyn_algo_params;

	/* if Primary Interface load is greater than Max byte threshold
	 * and Secondary Interface load is less than Min byte threshold
	 * and greater than MCHAN_NO_HOST_LOAD_BYTES then provide
	 * maximum bandwidth to Primary Interface
	 */
	if (MCHAN_IS_FAST_INCREMENT_POSSIBLE(sec_bss_load, prim_bss_load)) {
		action_to_take = MCHAN_BW_INC_TYPE_FAST;
		*act_on_primary = TRUE;
	}
	/* if Secondary Interface load is greater than Max byte threshold
	 * and Primary Interface load is less than Min byte threshold
	 * and greater than MCHAN_NO_HOST_LOAD_BYTES then provide
	 * maximum bandwidth to Secondary Interface
	 */
	else if (MCHAN_IS_FAST_INCREMENT_POSSIBLE(prim_bss_load, sec_bss_load)) {
		action_to_take = MCHAN_BW_INC_TYPE_FAST;
		*act_on_primary = FALSE;
	}
	/* If both Primary Interface and Secondary Interface load
	 * is greater than Max byte threshold and if bw init flag is zero
	 * provide both the interface with 50% BW and set
	 * bw_init_flag so that this happens only once
	 */
	else if (MCHAN_MAX_LOAD_ON_BOTH_IF(prim_bss_load, sec_bss_load)) {
		if (!dyn_algo_params->bw_init_flag) {
			action_to_take = MCHAN_BW_INIT;
		} else {
			if (mchan->percentage_bw == 50) {
				/* If Primary interface load is greater than secondary interface
				 * load increment the BW of the Primary interface
				 */
				if (MCHAN_LOAD_GREATER_THAN_OFFSET(prim_bss_load,
					sec_bss_load, MCHAN_OFFSET_BYTES)) {
					action_to_take = (slow_phase) ?
					MCHAN_BW_INC_TYPE_SLOW : MCHAN_BW_INC_TYPE_REG;
					*act_on_primary = (slow_phase) ? FALSE: TRUE;
				}
				/* If Secondary interface load is greater than Primary interface
				 * load increment the BW of the Secondary interface
				 */
				else if (MCHAN_LOAD_GREATER_THAN_OFFSET(sec_bss_load,
					prim_bss_load, MCHAN_OFFSET_BYTES)) {
					action_to_take = (slow_phase) ?
					MCHAN_BW_INC_TYPE_SLOW : MCHAN_BW_INC_TYPE_REG;
					*act_on_primary = FALSE;
				}
			} else if (dyn_algo_params->last_inc_is_primary) {
				/* the last inc was primary, but the load did not increase
				 * by OFFSET bytesthen start slow update phase this way the
				 * percentage bw is maintained propotional to the load
				 */
				action_to_take = wlc_mchan_check_primary_if(mchan, prim_bss_load,
					sec_bss_load, act_on_primary);
			} else {
				/* the last inc was secondary, but the load did not increase
				 * by OFFSET bytesthen start slow update phase this way the
				 * percentage bw is maintained propotional to the load
				 */
				action_to_take = wlc_mchan_check_sec_if(mchan, prim_bss_load,
					sec_bss_load, act_on_primary);
			}
		}
	}
	/* If there is NO HOST Load on both interfaces then do bw_init
	 * If MODESW is enabled then the incerement the sample duration
	 */
	else if (MCHAN_NO_HOST_LOAD_ON_BOTH_IF(prim_bss_load, sec_bss_load)) {
		action_to_take =  MCHAN_BW_INIT_NO_LOAD;
	} else if (MCHAN_HOST_LOAD_ON_SINGLE_IF(prim_bss_load, sec_bss_load)) {
		if (!dyn_algo_params->bw_init_oneif_tp) {
			action_to_take = MCHAN_BW_INIT_ONE_IF_TP;
		} else {
			if (mchan->percentage_bw == 50) {
				/* If Primary interface load is greater than secondary interface
				 * load increment the BW of the Primary interface
				 */
				if (MCHAN_LOAD_GREATER_THAN_OFFSET(prim_bss_load,
					sec_bss_load, MCHAN_OFFSET_BYTES)) {
					action_to_take = (slow_phase) ?
					MCHAN_BW_INC_TYPE_SLOW : MCHAN_BW_INC_TYPE_REG;
					*act_on_primary = (slow_phase) ? FALSE: TRUE;
				}
				/* If Secondary interface load is greater than Primary interface */
				/* load increment the BW of the Secondary interface  */
				else if (MCHAN_LOAD_GREATER_THAN_OFFSET(sec_bss_load,
					prim_bss_load, MCHAN_OFFSET_BYTES)) {
					action_to_take = (slow_phase) ?
					MCHAN_BW_INC_TYPE_SLOW : MCHAN_BW_INC_TYPE_REG;
					*act_on_primary = FALSE;
				}
			} else if (dyn_algo_params->last_inc_is_primary) {
				/* NO HOST load on secondary, the last inc was primary,
				 * but the load did not increase by OFFSET bytes then
				 * start slow update phase this way the percentage bw is
				 * maintained propotional to the load
				 */
				action_to_take = wlc_mchan_check_primary_if(mchan, prim_bss_load,
					sec_bss_load, act_on_primary);
			} else {
				/* NO HOST load on primary, the last inc was secondary, but the load
				 * did not increase by OFFSET bytes then start slow update phase
				 * this way the percentage bw is maintained propotional to the load
				 */
				action_to_take = wlc_mchan_check_sec_if(mchan, prim_bss_load,
					sec_bss_load, act_on_primary);
			}
		}
	}
	return action_to_take;
}

#if defined(WLRSDB) && defined(WL_MODESW)
/* The function below will update the qos packet counters of mchan with the number of pkts
 * that were sent in the last watchdog duration
 */
#ifdef WLCNT
static void
wlc_mchan_dyn_algo_update_wme_cntrs(mchan_info_t *mchan)
{
	wl_wme_cnt_t* wme_cnts;
	uint8 wme_indx;
	mchan_qos_modesw_info_t *qos_info;
	uint32 tot_bytes;
	uint8 idx = 0;
	wlc_info_t *wlc_iter;

	qos_info = (mchan_qos_modesw_info_t *)mchan->dyn_algo_params->qos_info;

	if (WME_ENAB(mchan->wlc->pub)) {
		FOREACH_WLC(mchan->wlc->cmn, idx, wlc_iter) {
			for (wme_indx = 0; wme_indx < NUM_QOS_FIFO; wme_indx++) {
				wme_cnts = wlc_iter->pub->_wme_cnt;

				if ((wme_cnts != NULL) && qos_info) {
					tot_bytes = wme_cnts->tx[wme_indx2fifo[wme_indx]].bytes +
						wme_cnts->rx[wme_indx2fifo[wme_indx]].bytes;

					/* update the counters with number of new
					 * tx/rx qos packets in this second
					 */
					if (qos_info[idx].qos_bytes_cached[wme_indx] &&
						(tot_bytes <
						qos_info[idx].qos_bytes_cached[wme_indx])) {

						memset(qos_info[idx].qos_bytes_delta, 0,
							sizeof(qos_info[idx].qos_bytes_delta));
						memset(qos_info[idx].qos_bytes_cached, 0,
							sizeof(qos_info[idx].qos_bytes_cached));
					}

					qos_info[idx].qos_bytes_delta[wme_indx] =
						(tot_bytes -
						qos_info[idx].qos_bytes_cached[wme_indx]);

					WL_MCHAN(("%s tx+rx: %d delta %d cached:%d \n",
						__func__, tot_bytes,
						qos_info[idx].qos_bytes_delta[wme_indx],
						qos_info[idx].qos_bytes_cached[wme_indx]));

					/* update the old counters */
					qos_info[idx].qos_bytes_cached[wme_indx] = tot_bytes;
				}
			}
		}
	}
}

static bool
wlc_mchan_is_qos_traffic_present(mchan_info_t *mchan)
{
	mchan_qos_modesw_info_t *qos_info;
	uint8 idx = 0;
	wlc_info_t *wlc_iter;

	qos_info = (mchan_qos_modesw_info_t *)mchan->dyn_algo_params->qos_info;

	/* if there is QOS traffic that is VOICE/ VIDEO traffic
	 * return TRUE, FALSE otherwise;
	 */
	FOREACH_WLC(mchan->wlc->cmn, idx, wlc_iter) {

		if (qos_info) {
			WL_MCHAN(("%s (VI %d VO %d)\n",
				__func__, qos_info[idx].qos_bytes_delta[VI_WME_IDX],
				qos_info[idx].qos_bytes_delta[VO_WME_IDX]));

			if ((qos_info[idx].qos_bytes_delta[VI_WME_IDX] >=
				mchan->dyn_algo_params->VI_traffic_threshold) ||
					(qos_info[idx].qos_bytes_delta[VO_WME_IDX] >=
					mchan->dyn_algo_params->VO_traffic_threshold)) {
				return TRUE;
			}
		} else {
			return FALSE;
		}
	}

	return FALSE;
}

#endif /* WLCNT */
#endif /* WLRSDB && WL_MODESW */

#if defined(WLRSDB) && defined(WL_MODESW)
static void
wlc_mchan_rsdb_dyn_algo(wlc_info_t *wlc)
{
	wlc_info_t *primary_wlc, *secondary_wlc;
	wlc_bsscfg_t *primary_bss, *secondary_bss;
	uint32 primary_bss_load = 0, secondary_bss_load = 0;
	uint8 cfg_idx;
	mchan_bss_info_t *bmi;
	primary_wlc = WLC_RSDB_GET_PRIMARY_WLC(wlc);
	secondary_wlc = wlc_rsdb_get_other_wlc(primary_wlc);
	FOREACH_AS_BSS(primary_wlc, cfg_idx, primary_bss)
	{
		bmi = MCHAN_BSS_INFO(primary_wlc->mchan, primary_bss);
		if (bmi) {
			wlc_mchan_calculate_bss_load(primary_wlc->mchan, bmi);
			primary_bss_load = bmi->load;
			break;
		}
	}

	FOREACH_AS_BSS(secondary_wlc, cfg_idx, secondary_bss)
	{
		bmi = MCHAN_BSS_INFO(secondary_wlc->mchan, secondary_bss);
		if (bmi) {
			wlc_mchan_calculate_bss_load(secondary_wlc->mchan, bmi);
			secondary_bss_load = bmi->load;
			break;
		}
	}

	/* load is calculated as the percentage of MIMO */
	WLC_MAP_LOAD_TO_MIMO(primary_bss_load);
	WLC_MAP_LOAD_TO_MIMO(secondary_bss_load);

	if ((primary_bss_load >= AVG_MAX_SISO_LOAD) ||
		(secondary_bss_load >= AVG_MAX_SISO_LOAD)) {
		uint8 last_req_primary = wlc_modesw_get_last_load_req_if(wlc->modesw);
		uint8 max_load_req_dur = wlc_modesw_get_sample_dur(wlc->modesw);
		bool primary_if_with_max_load, secondary_if_with_max_load;

		secondary_if_with_max_load = ((primary_bss_load < MCHAN_MIN_MIMO_LOAD) &&
		(secondary_bss_load >= AVG_MAX_SISO_LOAD));

		primary_if_with_max_load = ((secondary_bss_load < MCHAN_MIN_MIMO_LOAD) &&
		(primary_bss_load >= AVG_MAX_SISO_LOAD));

		/* load on one interface is hitting the max t/p numbers
		 * load on other interface is less than MCHAN_MIN_MIMO_LOAD
		 * switch to VSDB inorder to give entropy to secondary i/f
		 * to meet its max tx requirement
		 */
		if ((secondary_if_with_max_load || primary_if_with_max_load) &&
#if defined(WLCNT)
			(WME_ENAB(wlc->pub) ?
			(!wlc_mchan_is_qos_traffic_present(wlc->mchan)) : TRUE) &&
#endif /* WLCNT */
			TRUE) {
			/* Await till the load requirement is seen for
			 * MODESW_R2V_SAMPLE_DURATION
			 */
			if (max_load_req_dur == MODESW_R2V_SAMPLE_DURATION) {
				wlc_mchan_set_cfg_req_modesw(wlc->mchan, secondary_bss,
					secondary_if_with_max_load);
				wlc_mchan_set_cfg_req_modesw(wlc->mchan, primary_bss,
					primary_if_with_max_load);
				wlc_rsdb_dyn_switch(wlc, MODE_VSDB);
				wlc_modesw_set_sample_dur(wlc->modesw, 0);
			} else if ((secondary_if_with_max_load && !last_req_primary) ||
			    (primary_if_with_max_load && last_req_primary)) {
				/* if the last load requirement is seen on same i/f
				 * inc the sample duration for R2V
				 */
				wlc_modesw_set_sample_dur(wlc->modesw,
					wlc_modesw_get_sample_dur(wlc->modesw) + 1);
			} else {
				/* if last load requirement is seen on diff i/f than
				 * current one reset the sample duration
				 * In fluctuating throughput, the load req might also
				 * fluctuate across interfaces
				 */
				wlc_modesw_set_sample_dur(wlc->modesw, 0);
			}
			wlc_modesw_set_last_load_req_if(wlc->modesw, primary_if_with_max_load);
		}
	} else {
		/* need not switch if the max load requirement
		 * is not seen on either of interfaces
		 */
		/* if there is QOS based traffic on any of the bsscfg's
		 * stay in RSDB as QOS data will suffer in VSDB
		 */
		wlc_modesw_set_sample_dur(wlc->modesw, 0);
	}
}
#endif /* WLRSDB && WL_MODESW */

/* this function checks whether if both the interface laod is either increasing or decreasing */
static bool
wlc_mchan_load_inc_or_dec(mchan_info_t *mchan, uint32 primary_bss_load, uint32 secondary_bss_load)
{
	bool inc_or_dec;
	uint32 pri_old_load, sec_old_load;
	pri_old_load = mchan->dyn_algo_params->primary_bss_load_old;
	sec_old_load = mchan->dyn_algo_params->secondary_bss_load_old + MCHAN_OFFSET_BYTES;
	inc_or_dec = FALSE;
	if ((primary_bss_load > (pri_old_load + MCHAN_OFFSET_BYTES)) &&
		(secondary_bss_load > (sec_old_load + MCHAN_OFFSET_BYTES))) {
		return (inc_or_dec = TRUE);
	}
	else if (((primary_bss_load + MCHAN_OFFSET_BYTES) < pri_old_load) &&
		((secondary_bss_load + MCHAN_OFFSET_BYTES) < sec_old_load)) {
		return (inc_or_dec = TRUE);
	}
	else {
		return (inc_or_dec = FALSE);
	}
}

/* This function is used to increment the bandwidth for the specified interface depending
 * on the increment type passed as the parameter, if it is within the BW limits
 */
static void wlc_mchan_inc_bw(mchan_info_t *mchan, uint8 bw_inc_type, bool inc_primary)
{
	if (bw_inc_type == MCHAN_BW_INC_TYPE_FAST) {
		if (inc_primary) {
			mchan->percentage_bw += MCHAN_INC_BW_FASTRATE;
		} else {
			mchan->percentage_bw -= MCHAN_INC_BW_FASTRATE;
		}
		mchan->dyn_algo_params->slow_upd_phase = FALSE;
		mchan->dyn_algo_params->bw_init_flag = FALSE;
		mchan->dyn_algo_params->bw_init_oneif_tp = FALSE;
	}
	else if (bw_inc_type == MCHAN_BW_INC_TYPE_REG) {
		/* Increment BW for Primary Interface in steps of 5% */
		if (inc_primary) {
			mchan->percentage_bw += MCHAN_INC_BW_MEDIUMRATE;
			mchan->dyn_algo_params->last_inc_is_primary = TRUE;
		}
		/* Increment BW for Secondary Interface in steps of  5% */
		else {
			mchan->percentage_bw -= MCHAN_INC_BW_MEDIUMRATE;
			mchan->dyn_algo_params->last_inc_is_primary = FALSE;
		}
		mchan->dyn_algo_params->dyn_algo_counter = 1;
		mchan->dyn_algo_params->slow_upd_phase = FALSE;
	}
	else if (bw_inc_type == MCHAN_BW_INC_TYPE_SLOW) {
		if (mchan->percentage_bw > MCHAN_DEFAULT_BW) {
			mchan->percentage_bw -= MCHAN_INC_BW_SLOWRATE;
			mchan->dyn_algo_params->last_inc_is_primary = FALSE;
		} else if (mchan->percentage_bw < MCHAN_DEFAULT_BW) {
			mchan->percentage_bw += MCHAN_INC_BW_SLOWRATE;
			mchan->dyn_algo_params->last_inc_is_primary = TRUE;
		}
		mchan->dyn_algo_params->slow_upd_phase = TRUE;
		mchan->dyn_algo_params->dyn_algo_counter = 0;
	}
	/* Keep the BW increment within limit, so that VSDB conn is maintained */
	if (mchan->percentage_bw < MCHAN_MIN_PERCENTAGE_BW) {
		mchan->percentage_bw = MCHAN_MIN_PERCENTAGE_BW;
	} else if (mchan->percentage_bw > MCHAN_MAX_PERCENTAGE_BW) {
		mchan->percentage_bw = MCHAN_MAX_PERCENTAGE_BW;
	}
	mchan->trigg_algo = TRUE;
	mchan->dyn_algo_params->anomaly = FALSE;
}

static mchan_dyn_algo_action
	wlc_mchan_check_primary_if(mchan_info_t *mchan, uint32 primary_bss_load,
	uint32 secondary_bss_load, bool* act_on_primary)
{
	/* If Primary interface load is greater than secondary interface
	 * load increment the BW of the Primary interface
	 */
	if (MCHAN_LOAD_GREATER_THAN_OFFSET(primary_bss_load,
		mchan->dyn_algo_params->primary_bss_load_old,
		MCHAN_OFFSET_BYTES)) {
		if (MCHAN_LOAD_GREATER_THAN_OFFSET(mchan->dyn_algo_params->secondary_bss_load_old,
			secondary_bss_load, MCHAN_OFFSET_BYTES)) {
			*act_on_primary = FALSE;
			return MCHAN_BW_INC_TYPE_SLOW;
		}
		/* if primary interface and secondary intf is
		 * increasing  at the same time OR decresing
		 * at the same time  restore the BW with
		 * previous to this behaviour
		 */
		else if (wlc_mchan_load_inc_or_dec(mchan, primary_bss_load,
			secondary_bss_load)) {
			return MCHAN_BW_INC_LOAD_ANOMALY;
		}
		else {
			if (mchan->dyn_algo_params->slow_upd_phase) {
				*act_on_primary = FALSE;
				return MCHAN_BW_INC_TYPE_SLOW;
			} else {
				*act_on_primary = TRUE;
				return MCHAN_BW_INC_TYPE_REG;
			}
		}
	}
	else {
		if (mchan->dyn_algo_params->slow_upd_phase) {
			mchan->percentage_bw = mchan->dyn_algo_params->saved_bw;
			mchan->trigg_algo = TRUE;
		} else {
			*act_on_primary = FALSE;
			return MCHAN_BW_INC_TYPE_REG;
		}
	}
	return MCHAN_BW_INC_NO_ACTION;
}

/* In this function we check if secondary interface load is incresing or if both
 * the interface load are incrementing and decrementing based on this we increment
 * the bandwidth for the respective interface
 */
static mchan_dyn_algo_action
wlc_mchan_check_sec_if(mchan_info_t *mchan, uint32 primary_bss_load,
	uint32 secondary_bss_load, bool*act_on_primary)
{
	/* If secondary interface load is greater than	Primary
	 * interface load increment the BW of the secondary intf
	 */
	if (MCHAN_LOAD_GREATER_THAN_OFFSET(secondary_bss_load,
		mchan->dyn_algo_params->secondary_bss_load_old,
		MCHAN_OFFSET_BYTES)) {
		if (MCHAN_LOAD_GREATER_THAN_OFFSET(mchan->dyn_algo_params->primary_bss_load_old,
			primary_bss_load, MCHAN_OFFSET_BYTES)) {
			*act_on_primary = FALSE;
			return MCHAN_BW_INC_TYPE_SLOW;
		}
		/* if primary interface and secondary interface is
		 * increasing at the same time OR decresing at
		 * the same time  restore the BW which
		 * previous to this behaviour
		 */
		else if (wlc_mchan_load_inc_or_dec(mchan, primary_bss_load,
			secondary_bss_load)) {
			return MCHAN_BW_INC_LOAD_ANOMALY;
		}
		else {
			if (mchan->dyn_algo_params->slow_upd_phase) {
				*act_on_primary = FALSE;
				return MCHAN_BW_INC_TYPE_SLOW;
			} else {
				*act_on_primary = FALSE;
				return MCHAN_BW_INC_TYPE_REG;
			}
		}
	}
	else {
		if (mchan->dyn_algo_params->slow_upd_phase) {
			mchan->percentage_bw = mchan->dyn_algo_params->saved_bw;
			mchan->trigg_algo = TRUE;
		}
		else {
			*act_on_primary = TRUE;
			return MCHAN_BW_INC_TYPE_REG;
		}
	}
	return MCHAN_BW_INC_NO_ACTION;
}

void
wlc_mchan_abs_proc(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint32 tsf_l)
{
	mchan_info_t *mchan = wlc->mchan;
	mchan_bss_info_t *mbi;

	/* return if not mchan not active or AP bsscfg */
	if (!(MCHAN_ENAB(wlc->pub) && MCHAN_ACTIVE(wlc->pub)) || BSSCFG_AP(cfg)) {
		return;
	}

	mbi = MCHAN_BSS_INFO(mchan, cfg);
	mbi->in_psc = FALSE;

	if (!mchan->adjusted_for_nchan) {
		wlc_mchan_set_adjusted_for_nchan(mchan);
	}

	/* schedule timers only if the other cfg is not in blocking state
	 * if not, then just do prepare_pm_mode and let that cfg come
	 * out of blocking mode. simple return in that case.
	 */
	if (mchan->blocking_bsscfg && mchan->timer_running &&
		!SCAN_IN_PROGRESS(mchan->wlc->scan)) {
		wlc_mchan_blocking_bsscfg(mchan);
		mchan->timer_running = FALSE;
		return;
	}

	if (!mchan->timer_running) {
		wlc_bsscfg_t *other_cfg;
		uint32 abs_dur;
		uint32 bcn_per;
		uint32 proc_time = mchan->proc_time_us;
		uint32 t1, t2;

		mchan->timer_running = TRUE;
		mchan->trigg_algo = FALSE;

		other_cfg = wlc_mchan_get_other_cfg_frm_cur(wlc, cfg);

		/* For GC, we need to put GC in PS mode just before abs comes */
		bcn_per = cfg->current_bss->beacon_period << 10;

		abs_dur = wlc_mcnx_read_shm(wlc->mcnx,
			M_P2P_BSS_NOA_DUR(wlc, wlc_mcnx_BSS_idx(wlc->mcnx, cfg)));
		abs_dur <<= P2P_UCODE_TIME_SHIFT;

		t1 = tsf_l + abs_dur - 2*proc_time;
		t2 = tsf_l + bcn_per;

		WL_MCHAN(("%s: abs_dur %u, start %u, end %u\n",  __FUNCTION__,
		          (t2 - t1), t1, t2));

		/* add mchan sched element for p2p cfg */
		wlc_mchan_sched_add(mchan, cfg, bcn_per - abs_dur + proc_time,
			2*bcn_per, t1);
		wlc_mchan_sched_add(mchan, other_cfg, bcn_per + abs_dur - 3*proc_time,
			2*bcn_per, t2);
	}
}

static void
wlc_mchan_psc_cleanup(mchan_info_t *mchan, wlc_bsscfg_t *cfg)
{
	if (mboolisset(cfg->pm->PMblocked, WLC_PM_BLOCK_MCHAN_ABS)) {
		/* unblock pm for abs */
		mboolclr(cfg->pm->PMblocked, WLC_PM_BLOCK_MCHAN_ABS);

		/* on channel and pm still enabled, get us out of pm enabled state */
		if ((cfg->pm->PM == PM_OFF || cfg->pm->WME_PM_blocked || mchan->bypass_pm) &&
		    cfg->pm->PMenabled && wlc_shared_current_chanctxt(mchan->wlc, cfg)) {
			wlc_set_pmstate(cfg, FALSE);
			WL_MCHAN(("wl%d.%d: in psc, bring cfg out of ps mode\n",
				mchan->wlc->pub->unit, WLC_BSSCFG_IDX(cfg)));
		}
	}
}

void
wlc_mchan_psc_proc(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint32 tsf_l)
{
	mchan_info_t *mchan = wlc->mchan;
	mchan_bss_info_t *mbi;

	/* return if not mchan not active or AP bsscfg */
	if (!(MCHAN_ENAB(wlc->pub) && MCHAN_ACTIVE(wlc->pub)) || BSSCFG_AP(cfg)) {
		return;
	}

	mbi = MCHAN_BSS_INFO(mchan, cfg);
	mbi->in_psc = TRUE;

	if (!mchan->adjusted_for_nchan) {
		wlc_mchan_set_adjusted_for_nchan(mchan);
	}

	/* schedule timers only if the other cfg is not in blocking state
	 * if not, then just do prepare_pm_mode and let that cfg come
	 * out of blocking mode. simple return in that case.
	 */
	if (mchan->blocking_bsscfg && mchan->timer_running &&
		!SCAN_IN_PROGRESS(mchan->wlc->scan)) {
		wlc_mchan_blocking_bsscfg(mchan);
		mchan->timer_running = FALSE;
		return;
	}

#ifdef STA
	if (mchan->onchan_cfg_idx == WLC_BSSCFG_IDX(cfg)) {
		/* Send PS=0 Null Frame for P2P GC cfg */
		wlc_mchan_psc_cleanup(mchan, cfg);
	}
#endif /* STA */

	if (!mchan->timer_running) {
		wlc_bsscfg_t *other_cfg;
		uint32 next_abs_time;
		uint32 abs_dur;
		uint32 bcn_per;
		uint32 proc_time = mchan->proc_time_us;
		uint32 t1, t2;

		mchan->timer_running = TRUE;
		mchan->trigg_algo = FALSE;

		other_cfg = wlc_mchan_get_other_cfg_frm_cur(wlc, cfg);

		/* For GC, we need to put GC in PS mode just before abs comes */
		bcn_per = cfg->current_bss->beacon_period << 10;

		next_abs_time = wlc_mcnx_read_shm(wlc->mcnx,
			M_P2P_BSS_N_NOA(wlc, wlc_mcnx_BSS_idx(wlc->mcnx, cfg)));
		next_abs_time <<= P2P_UCODE_TIME_SHIFT;
		wlc_tbtt_ucode_to_tbtt32(tsf_l, &next_abs_time);

		abs_dur = wlc_mcnx_read_shm(wlc->mcnx,
			M_P2P_BSS_NOA_DUR(wlc, wlc_mcnx_BSS_idx(wlc->mcnx, cfg)));
		abs_dur <<= P2P_UCODE_TIME_SHIFT;

		t1 = next_abs_time;
		t2 = next_abs_time + bcn_per + abs_dur - 2*proc_time;

		WL_MCHAN(("%s: abs_dur %u, start %u, end %u\n",  __FUNCTION__,
		          (t2 - t1), t1, t2));

		/* add mchan sched element for p2p cfg */
		wlc_mchan_sched_add(mchan, other_cfg, bcn_per + abs_dur - 3*proc_time,
			2*bcn_per, t1);
		wlc_mchan_sched_add(mchan, cfg, bcn_per - abs_dur + proc_time,
			2*bcn_per, t2);
	}
}

static uint16 wlc_mchan_pretbtt_time_us(mchan_info_t *mchan)
{
	BCM_REFERENCE(mchan);

#ifdef DONGLEBUILD
	return 8000;
#else
	if (SRHWVSDB_ENAB(mchan->wlc->pub)) {
		return 6000;
	}
	return 2000;
#endif /* DONGLEBUILD */
}

static bool
wlc_mchan_algo_change_allowed(mchan_info_t * mchan, int algo)
{
	wlc_info_t *wlc = mchan->wlc;
	wlc_bsscfg_t *trigg_cfg, *oth_cfg;

	if (!MCHAN_ACTIVE(wlc->pub))
		return TRUE;

	trigg_cfg = WLC_BSSCFG(wlc, mchan->trigger_cfg_idx);

	if (BSSCFG_AP(trigg_cfg)) {
		/* GO+STA Multi-IF */
		if (mchan->multi_if_adjustment && (wlc->stas_connected > 1))
			return FALSE;
	} else {
		/* GC+STA Multi-IF */
		if (wlc->stas_associated > 2)
			return FALSE;
		if (algo == MCHAN_BANDWIDTH_ALGO) {
			oth_cfg = WLC_BSSCFG(wlc, mchan->other_cfg_idx);
			if (trigg_cfg->current_bss->beacon_period !=
				oth_cfg->current_bss->beacon_period) {
				return FALSE;
			}
		}
	}
	/* more cases to be added as and when needed */
	return TRUE;
}

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static void
wlc_mchan_bss_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b)
{
	mchan_info_t *mchan = (mchan_info_t *)ctx;
	wlc_info_t *wlc = mchan->wlc;
	mchan_bss_info_t *mbi = MCHAN_BSS_INFO(mchan, cfg);
	char chanbuf[CHANSPEC_STR_LEN];

	bcm_bprintf(b, "\tmchan: sw_dtim_cnt %u "
	            "in_psc %d mchan_tbtt_since_bcn %u\n",
	            mbi->sw_dtim_cnt, mbi->in_psc,
	            mbi->mchan_tbtt_since_bcn);
	bcm_bprintf(b, "\tcontext: %d chanspec %s wlcif->qi %p\n",
	            wlc_has_chanctxt(wlc, cfg), (wlc_has_chanctxt(wlc, cfg) ?
	             wf_chspec_ntoa(wlc_get_chanspec(wlc, cfg), chanbuf) :
	             "0"),
	            cfg->wlcif->qi);
	bcm_bprintf(b, "\tconfig: chanspec %s\n",
	            wf_chspec_ntoa(mbi->chanspec, chanbuf));
}
#endif /* BCMDBG || BCMDBG_DUMP */

/** TIM */
static int
wlc_mchan_parse_tim_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	mchan_info_t *mchan = (mchan_info_t *)ctx;
	wlc_bsscfg_t *cfg = data->cfg;

	/* ignore malformed TIM */
	if (data->ie != NULL &&
	    data->ie_len >= TLV_HDR_LEN + DOT11_MNG_TIM_FIXED_LEN) {
		mchan_bss_info_t *mbi = MCHAN_BSS_INFO(mchan, cfg);

		/* fetch dtim count from beacon */
		mbi->sw_dtim_cnt = data->ie[TLV_HDR_LEN + DOT11_MNG_TIM_DTIM_COUNT];
	}

	return BCME_OK;
}

void
wlc_mchan_config_go_chanspec(mchan_info_t *mchan, wlc_bsscfg_t *cfg, chanspec_t chspec)
{
	mchan_bss_info_t *mbi = MCHAN_BSS_INFO(mchan, cfg);

	mbi->chanspec = chspec;
}

chanspec_t
wlc_mchan_configd_go_chanspec(mchan_info_t *mchan, wlc_bsscfg_t *cfg)
{
	mchan_bss_info_t *mbi = MCHAN_BSS_INFO(mchan, cfg);

	return mbi->chanspec;
}

/**
 * This function is used for ap bsscfg setup and assumes STA is associated first.
 * The ap bsscfg will take over the tsf registers but takes into account primary STA's
 * tbtt and bcn_period info.
 * The AP tsf will have its 0 time located in the middle of the primary STA's current beacon
 * period.
 * Per BSS pretbtt block will be setup for both AP and primary STA, based on the new tsf.
 */
bool
wlc_mchan_ap_tbtt_setup(wlc_info_t *wlc, wlc_bsscfg_t *ap_cfg)
{
	uint32 sta_bcn_period, ap_bcn_period, tsf_l, tsf_h;
	uint32 tbtt_h, tbtt_l;
	uint32 now_l, now_h;
	wlc_bsscfg_t *sta_cfg = NULL;
	wlc_bsscfg_t *cfg;
	chanspec_t ap_chanspec = ap_cfg->current_bss->chanspec;
	uint32 bcn_factor = 1;
	int i;
	wlc_mcnx_info_t *mcnx = wlc->mcnx;

	/* make sure we're not STA */
	if (BSSCFG_STA(ap_cfg)) {
		WL_ERROR(("wl%d: %s cannot setup tbtt for STA bsscfg!\n",
		  wlc->pub->unit, __FUNCTION__));
		return FALSE;
	}

	/* find a sta that we can use as reference point */
	FOREACH_AS_STA(wlc, i, cfg) {
		if (!WLC_MCHAN_SAME_CTLCHAN(cfg->current_bss->chanspec, ap_chanspec)) {
			break;
		}
	}
	if (cfg == NULL)
		return FALSE;

	sta_cfg = cfg;

	/* if ap is created on different channel than associated sta,
	 * copy bcn_per from STA.
	 * XXX: may want to copy dtim_period also but need to do more
	 * because currently, in wlc_BSSinit, dtim_period always programmed
	 * to default_bss->dtim_period and it is done before this function is
	 * called.
	 */
	if (!WLC_MCHAN_SAME_CTLCHAN(sta_cfg->current_bss->chanspec, ap_chanspec)) {
		WL_MCHAN(("wl%d %s: ap bsscfg adopting assoc sta's bcn period %u\n",
		          wlc->pub->unit, __FUNCTION__, sta_cfg->current_bss->beacon_period));
		ap_cfg->current_bss->beacon_period = sta_cfg->current_bss->beacon_period;
	}

	/* want last STA tbtt - 1/2 STA bcn period to be time (n*ap_bcn_period) for new tsf
	 * The idea is for n to be large enough so n * ap_bcn_period >= 1/2 sta bcn peirod.
	 * This ensures that our new tsf is always positive.
	 */
	wlc_mcnx_read_tsf64(mcnx, sta_cfg, &tsf_h, &tsf_l);
	wlc_mcnx_r2l_tsf64(mcnx, sta_cfg, tsf_h, tsf_l, &now_h, &now_l);
	wlc_tsf64_to_next_tbtt64(sta_cfg->current_bss->beacon_period, &tsf_h, &tsf_l);
	wlc_mcnx_r2l_tsf64(mcnx, sta_cfg, tsf_h, tsf_l, &tsf_h, &tsf_l);

	/* get the sta's bcn period */
	sta_bcn_period = sta_cfg->current_bss->beacon_period << 10;

	/* get the ap's bcn period */
	ap_bcn_period = ap_cfg->current_bss->beacon_period << 10;

	/* calculate the tsf offset between new and old tsf times */
	WL_MCHAN(("%s: sta tbtt = 0x%08x%08x, sta_bcn_period = %u\n",
	          __FUNCTION__, tsf_h, tsf_l, sta_bcn_period));
	wlc_uint64_sub(&tsf_h, &tsf_l, 0, (sta_bcn_period >> 1));
	while ((bcn_factor * ap_bcn_period) < (sta_bcn_period >> 1)) {
		bcn_factor++;
	}
	wlc_uint64_sub(&tsf_h, &tsf_l, 0, (bcn_factor * ap_bcn_period));
	{
	uint32 off_l;
	off_l = wlc_calc_tbtt_offset(ap_cfg->current_bss->beacon_period, tsf_h, tsf_l);
	wlc_uint64_sub(&tsf_h, &tsf_l, 0, off_l);
	}
	/* tsf_h and tsf_l now contain the new tsf times */

	WL_MCHAN(("%s: old tsf = 0x%08x%08x, new tsf = 0x%08x%08x, bcn_factor = %d\n",
	          __FUNCTION__, now_h, now_l, tsf_h, tsf_l, bcn_factor));

	/* setup the AP tbtt */
	WL_MCHAN(("%s: curr tsf = 0x%08x%08x\n", __FUNCTION__, now_h, now_l));

	/* N.B.: tbtt_l is what's going to be programmed into register tsf_cfpstart! */
	tbtt_h = tsf_h;
	tbtt_l = tsf_l;
	wlc_tsf64_to_next_tbtt64(ap_cfg->current_bss->beacon_period  * (bcn_factor + 1),
	                         &tbtt_h, &tbtt_l);

	wlc_tsf_adj(wlc, ap_cfg, tsf_h, tsf_l, now_h, now_l, tbtt_l, ap_bcn_period, FALSE);

	/* setup per bss block for ap */
	/* This will be done when wlc_p2p_bss_upd is called */

	return TRUE;
}

void
wlc_mchan_reset_params(mchan_info_t * mchan)
{
	mchan->adjusted_for_nchan = FALSE;
	mchan->timer_running = FALSE;
	mchan->trigg_algo = TRUE;
}

#if defined(WLRSDB) && defined(WL_MODESW)
void
wlc_mchan_set_clone_pending(mchan_info_t* mchan, bool value)
{
	wlc_info_t* wlc = mchan->wlc;
	wlc_bsscfg_t* cfg;
	mchan_bss_info_t *mbi;

	if (!MCHAN_ACTIVE(wlc->pub))
		return;

	cfg = WLC_BSSCFG(wlc, mchan->other_cfg_idx);
	mbi = MCHAN_BSS_INFO(mchan, cfg);
	mbi->clone_pending = value;
}

bool
wlc_mchan_get_clone_pending(mchan_info_t* mchan)
{
	wlc_info_t* wlc = mchan->wlc;
	wlc_bsscfg_t* cfg;
	mchan_bss_info_t *mbi;

	if (!MCHAN_ACTIVE(wlc->pub))
		return FALSE;

	cfg = WLC_BSSCFG(wlc, mchan->other_cfg_idx);
	ASSERT(cfg != NULL);
	mbi = MCHAN_BSS_INFO(mchan, cfg);
	ASSERT(mbi != NULL);
	return mbi->clone_pending;
}

bool
wlc_mchan_get_modesw_pending(mchan_info_t *mchan)
{
	wlc_info_t *wlc = mchan->wlc;
	if (!MCHAN_ACTIVE(wlc->pub))
		return FALSE;
	else {
		mchan_bss_info_t *mbi = NULL;
		wlc_bsscfg_t *cfg = WLC_BSSCFG(wlc, mchan->trigger_cfg_idx);
		mbi = MCHAN_BSS_INFO(mchan, cfg);
		if (mbi && !mbi->modesw_pending) {
			cfg = WLC_BSSCFG(wlc, mchan->other_cfg_idx);
			mbi = MCHAN_BSS_INFO(mchan, cfg);
			if (mbi && !mbi->modesw_pending)
				return FALSE;
			else
				return TRUE;
		}
		else
			return TRUE;
	}
}

static void
wlc_mchan_opermode_change_cb(void *ctx, wlc_modesw_notif_cb_data_t *notif_data)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;
	wlc_info_t *wlc0 = wlc->cmn->wlc[0];
	ASSERT(wlc);
	ASSERT(notif_data);

	if (!MCHAN_ACTIVE(wlc->pub) && WLC_MODESW_ANY_SWITCH_IN_PROGRESS(wlc->modesw)) {
		WL_MODE_SWITCH(("wl%d: Old ASDB switch stuck clear\n", WLCWLUNIT(wlc)));
		wlc_modesw_set_switch_type(wlc0->modesw, MODESW_NO_SW_IN_PROGRESS);
		return;
	}

	switch (notif_data->signal) {
		case MODESW_DN_AP_COMPLETE:
		case MODESW_DN_STA_COMPLETE:
			if (MCHAN_ACTIVE(wlc->pub)) {
				if (wlc_modesw_pending_check(wlc->modesw) &&
					(notif_data->status == BCME_OK)) {
					wlc_mchan_set_clone_pending(wlc->mchan, TRUE);
				}
			}
			break;
		case MODESW_UP_AP_COMPLETE:
		case MODESW_UP_STA_COMPLETE:
			if (MCHAN_ACTIVE(wlc->pub)) {
				if (wlc_modesw_pending_check(wlc->modesw)) {
					/* If the current mode switch is from RSDB to
					* VSDB then, this callback indicates that mode
					* switch completes here. Hence clear the modesw
					* type.
					*/
					if (wlc_modesw_get_switch_type(wlc0->modesw)
						== MODESW_RSDB_TO_VSDB) {
						wlc_modesw_set_switch_type(wlc0->modesw,
							MODESW_NO_SW_IN_PROGRESS);
						WL_MODE_SWITCH(("wl%d: ASDB (R -> V) Switch"
							" Complete\n", WLCWLUNIT(wlc)));
					}
				}
			}
			break;
		case MODESW_ACTION_FAILURE:
			if (MCHAN_ACTIVE(wlc->pub)) {
				if (wlc_modesw_pending_check(wlc->modesw)) {
					if (WLC_MODESW_ANY_SWITCH_IN_PROGRESS(wlc->modesw)) {
						WL_MODE_SWITCH(("wl%d: ASDB %s Switch AF Failed\n",
							WLCWLUNIT(wlc),
							((wlc_modesw_get_switch_type(wlc->modesw)
							== MODESW_RSDB_TO_VSDB) ? "(R -> V)" :
							"(V -> R)")));
						wlc_modesw_set_switch_type(wlc0->modesw,
							MODESW_NO_SW_IN_PROGRESS);
					}
				}
			}
		default:
			WL_MCHAN(("wl%d %s need not handle any other case\n",
				wlc->pub->unit, __FUNCTION__));
	}
	return;
}
#endif /* WLRSDB && WL_MODESW */

/* This function returns chanspec associated to cfg */
uint16
wlc_mchan_get_chanspec(mchan_info_t *mchan, wlc_bsscfg_t *bsscfg)
{
	mchan_bss_info_t *mbi = MCHAN_BSS_INFO(mchan, bsscfg);
	if (mbi == NULL)
		return 0;

	return mbi->chanspec;
}

static void wlc_mchan_update_stats(wlc_info_t *wlc, wlc_bsscfg_t *on_chan_cfg)
{
	wlc_bsscfg_t *other_cfg = wlc_mchan_get_other_cfg_frm_cur(wlc, on_chan_cfg);
	mchan_info_t *mchan = wlc->mchan;
	mchan_bss_info_t *ombi = MCHAN_BSS_INFO(mchan, other_cfg);
	mchan_stat_t *timing_stats;

	uint32 now = R_REG(wlc->osh, D11_TSFTimerLow(wlc));
	uint32 diff;

	/* "other bss" time_in_channel = now - prev_now(mchan->chansw_timestamp)  */
	/* populate min, max based on time_in_channel */
	if (ombi) {
		timing_stats = ombi->timing_stats;
		timing_stats->cycle_cnt++;
		if (mchan->chansw_timestamp) {
			diff = now - mchan->chansw_timestamp;
			timing_stats->cycle_time_cumul += diff;
			if (diff > timing_stats->cycle_time_max)
				timing_stats->cycle_time_max = diff;
			if ((diff < timing_stats->cycle_time_min) ||
				(timing_stats->cycle_time_min == 0))
				timing_stats->cycle_time_min = diff;

			/* airtime statistics for load calculation */
			if (wlc_mchan_if_byte(wlc, on_chan_cfg, other_cfg) != BCME_OK) {
				mchan->mchan_err_counter++;
				WL_ERROR(("wl%d:%d: %s: mchan_if_byte failed\n",
					wlc->pub->unit, mchan->mchan_err_counter, __FUNCTION__));
			}
		}
		mchan->chansw_timestamp = now;
	}
}

static int
wlc_mchan_if_byte(wlc_info_t *wlc, wlc_bsscfg_t *curr_cfg, wlc_bsscfg_t *other_cfg)
{
	mchan_info_t *mchan = wlc->mchan;
	mchan_bss_info_t *mbi, *obmi; /* Current bss and other bss info */
	uint32 new_txbytes, new_rxbytes, old_txbytes, old_rxbytes;
	uint8 index;
	if (curr_cfg && other_cfg) {
		/* Current bss mchan info "just switched" current channel */
		mbi = MCHAN_BSS_INFO(mchan, curr_cfg);
		/* Other bss mchan info */
		obmi = MCHAN_BSS_INFO(mchan, other_cfg);
	}
	else {
		return BCME_ERROR;
	}
	if (mbi && obmi) {
		mchan_if_bytes_t *bss_stats = mbi->bytes_stats;
		mchan_if_bytes_t *otherbss_byte_stats = obmi->bytes_stats;

		new_txbytes = wlc->pub->_cnt->txbyte;
		new_rxbytes = wlc->pub->_cnt->rxbyte;
		old_txbytes = bss_stats->tot_txbytes;
		old_rxbytes = bss_stats->tot_rxbytes;
		index = otherbss_byte_stats->sample;

		otherbss_byte_stats->txbytes[index] = new_txbytes - old_txbytes;
		otherbss_byte_stats->rxbytes[index] = new_rxbytes - old_rxbytes;
		otherbss_byte_stats->sample = ((otherbss_byte_stats->sample +
			1) % MCHAN_DYN_ALGO_SAMPLE);
		/* Cache the new TX-RX byte values */
		otherbss_byte_stats->tot_txbytes = wlc->pub->_cnt->txbyte;
		otherbss_byte_stats->tot_rxbytes = wlc->pub->_cnt->rxbyte;
	}
	return BCME_OK;
}

#ifdef WLRSDB
static int
wlc_mchan_bsscfg_config_get(void *hdl, wlc_bsscfg_t *bsscfg, uint8 *data, int *len)
{
	mchan_info_t *mchan = (mchan_info_t *)hdl;
	mchan_bss_info_t *mbi;
	if (len == NULL) {
		WL_ERROR(("wl%d: %s: Null length passed\n", mchan->wlc->pub->unit, __FUNCTION__));
		return BCME_ERROR;
	}

	if ((data == NULL) || (*len < sizeof(chanspec_t))) {
		WL_ERROR(("wl%d: %s: Buffer too short\n", mchan->wlc->pub->unit, __FUNCTION__));
		*len = sizeof(chanspec_t);
		return BCME_BUFTOOSHORT;
	}

	ASSERT(bsscfg != NULL);

	mbi =  MCHAN_BSS_INFO(mchan, bsscfg);

	if (mbi == NULL) {
		WL_ERROR(("wl%d: %s: NULL MCHAN Cubby\n", mchan->wlc->pub->unit, __FUNCTION__));
		*len = 0;
		return BCME_OK;
	}
	/* If the current STA CFG is roaming don't copy */
	if (BSSCFG_STA(bsscfg) && !WLC_BSS_ASSOC_NOT_ROAM(bsscfg)) {
		WL_INFORM(("wl%d: %s: Not copying cubby\n", mchan->wlc->pub->unit, __FUNCTION__));
		*len = 0;
		return BCME_OK;
	}
	memcpy(data, (&mbi->chanspec), sizeof(chanspec_t));
	*len = sizeof(chanspec_t);
	return BCME_OK;
}

static int
wlc_mchan_bsscfg_config_set(void *hdl, wlc_bsscfg_t *bsscfg, const uint8 *data, int len)
{
	mchan_info_t *mchan = (mchan_info_t *)hdl;
	mchan_bss_info_t *mbi;

	if ((data == NULL) || (len < sizeof(chanspec_t))) {
		WL_ERROR(("wl%d: %s: data(%p) len(%d)\n", mchan->wlc->pub->unit, __FUNCTION__,
			data, len));
		return BCME_ERROR;
	}

	ASSERT(bsscfg != NULL);
	mbi = MCHAN_BSS_INFO(mchan, bsscfg);

	if (mbi == NULL) {
		WL_ERROR(("wl%d: %s: Not copying cubby\n", mchan->wlc->pub->unit, __FUNCTION__));
		return BCME_OK;
	}
	memcpy((&mbi->chanspec), data, sizeof(chanspec_t));
	return BCME_OK;
}
#endif /* WLRSDB */

wlc_bsscfg_t *
wlc_mchan_get_blocking_bsscfg(mchan_info_t *mchan)
{
	return mchan->blocking_bsscfg;
}

void
wlc_mchan_reset_blocking_bsscfg(mchan_info_t *mchan)
{
	mchan->blocking_bsscfg = NULL;
	mchan->blocking_cnt = 0;
	mchan->trigg_algo = TRUE;
}
#endif /* WLMCHAN */
