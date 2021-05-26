/*
 * 802.11k definitions for
 * Broadcom 802.11abgn Networking Device Driver
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
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id:$
 */

/** Radio Resource Management. Twiki: [WlDriver80211k] */

#ifndef _wlc_rrm_priv_h_
#define _wlc_rrm_priv_h_

#include <wlc_cfg.h>
#include <typedefs.h>
#include <wlc_utils.h>
#include <hndd11.h>
#include <wlc_scan_utils.h>

#define WLC_RRM_REQUEST_ID_MAX		20
struct wlc_rrm_cmn_info {
	/* Beacon report related variables */
	wl_bcn_report_cfg_t *bcn_rpt_cfg; /* Beacon report configuration variables */
	struct wl_timer *bcn_rpt_cache_timer; /* Timer pointer for bcn report timer callback */
	wlc_scandb_t	*assoc_sdb; /* scan database for assoc scan information */
	uint32 bcn_rpt_cache_time; /* Time at which bcn_rpt_cache_timer is started */
	vndr_ie_t *bcn_rpt_vndr_ie;
};
typedef struct wlc_rrm_cmn_info wlc_rrm_cmn_info_t;

typedef struct wlc_rrm_req {
	int16 type;		/* type of measurement */
	int8 flags;
	uint8 token;		/* token for this particular measurement */
	chanspec_t chanspec;	/* channel for the measurement */
	uint32 tsf_h;		/* TSF high 32-bits of Measurement Request start time */
	uint32 tsf_l;		/* TSF low 32-bits */
	uint16 dur;		/* TUs */
	uint16 intval;		/* Randomization interval in TUs */
} wlc_rrm_req_t;

typedef struct wlc_rrm_req_state {
	int report_class;	/* type of report to generate */
	bool broadcast;		/* was the request DA broadcast */
	int token;		/* overall token for measurement set */
	uint step;		/* current state of RRM state machine */
	chanspec_t chanspec_return;	/* channel to return to after the measurements */
	bool ps_pending;	/* true if we need to wait for PS to be announced before
				 * off-channel measurement
				 */
	int dur;		/* TUs, min duration of current parallel set measurements */
	uint16 reps;		/* Number of repetitions */
	uint32 actual_start_h;	/* TSF high 32-bits of actual start time */
	uint32 actual_start_l;	/* TSF low 32-bits */
	int cur_req;		/* index of current measure request */
	int req_count;		/* number of measure requests */
	wlc_rrm_req_t *req;	/* array of requests */
	/* CCA measurements */
	bool cca_active;	/* true if measurement in progress */
	int cca_dur;		/* TU, specified duration */
	int cca_idle;		/* idle carrier time reported by ucode */
	uint8 cca_busy;		/* busy fraction */
	/* Beacon measurements */
	bool scan_active;

	/* RPI measurements */
	bool rpi_active;	/* true if measurement in progress */
	bool rpi_end;		/* signal last sample */
	int rpi_dur;		/* TU, specified duration */
	int rpi_sample_num;	/* number of samples collected */
	uint16 rpi[WL_RPI_REP_BIN_NUM];	/* rpi/rssi measurement values */
	int rssi_sample_num;	/* count of samples in averaging total */
	int rssi_sample;	/* current sample averaging total */
	void *cb;		/* completion callback fn: may be NULL */
	void *cb_arg;		/* single arg to callback function */

	cca_ucode_counts_t cca_cnt_initial;
	cca_ucode_counts_t cca_cnt_final;
} wlc_rrm_req_state_t;

typedef struct rrm_bcnreq {
	uint8 token;
	uint8 reg;
	uint8 channel;

	uint8 rep_detail;			/* Reporting Detail */
	uint8 req_eid[WLC_RRM_REQUEST_ID_MAX];	/* Request elem id */

	uint8 req_eid_num;
	uint16 duration_tu;
	uint16 duration;	/* convert to ms (for beacon request only) */
	chanspec_t chanspec_list[MAXCHANNEL];
	uint16 channel_num;
	uint32 start_tsf_l;
	uint32 start_tsf_h;
	uint32 scan_type;
	wlc_ssid_t ssid;
	struct ether_addr bssid;
	int scan_status;
	wlc_bss_list_t scan_results;
	uint8 last_bcn_rpt_ind_req;	/* last bcn rpt indicn requested ? */
	uint8 flags;
} rrm_bcnreq_t;

#define WLC_RRM_LAST_BCNREP_REQ  0x01

typedef struct rrm_framereq {
	uint8 token;
	uint8 reg;
	uint8 channel;

	uint16 duration;
	uint32 start_tsf_l;
	uint32 start_tsf_h;
	uint32 frame_req_type; /* MUST be 1 */
	struct ether_addr mac; /* broadcast or specific */
	int32 avg_rcpi;
	uint32 rxframe_begin;
} rrm_framereq_t;

typedef struct rrm_chloadreq {
	uint8 token;
	uint8 reg;
	uint8 channel;

	uint16 duration;
	uint16 interval;
	uint32 start_tsf_l;
	uint32 start_tsf_h;
} rrm_chloadreq_t;

#define WL_RRM_IPI_BINS_NUM		11
#define WL_RRM_IPI_BUFF_TIME		50 /* TUs */
#define WLC_RRM_NOISE_IPI_INTERVAL	20 /* ms */
#define WLC_RRM_NOISE_IPI_SAMPLE_TIMES	4
typedef struct rrm_noisereq {
	uint8 token;
	uint8 reg;
	uint8 channel;

	uint16 duration;
	uint32 start_tsf_l;
	uint32 start_tsf_h;

	bool ipi_active;		/* true if measurement in progress */
	bool ipi_end;			/* signal last sample */
	int ipi_sample_num;		/* number of samples collected */
	uint16 ipi_dens[WL_RRM_IPI_BINS_NUM];
	int noise_sample_num;	/* count of samples in averaging total */
	int noise_sample;		/* current sample averaging total */
} rrm_noisereq_t;

typedef struct rrm_statreq {
	uint8 token;
	uint8 reg;

	struct ether_addr peer;
	uint16 duration;
	uint8 group_id;
	union {
		rrm_stat_group_0_t group0;
		rrm_stat_group_1_t group1;
		rrm_stat_group_qos_t group_qos;
		rrm_stat_group_10_t group10;
		rrm_stat_group_11_t group11;
		rrm_stat_group_12_t group12;
		rrm_stat_group_13_t group13;
		rrm_stat_group_14_t group14;
		rrm_stat_group_15_t group15;
		rrm_stat_group_16_t group16;
	} sta_data;
} rrm_statreq_t;

#define DOT11K_STA_STATS_MAX_GROUP		17
static const uint8 stat_group_length[DOT11K_STA_STATS_MAX_GROUP] = {
	DOT11_RRM_STATS_RPT_LEN_GRP_ID_0,
	DOT11_RRM_STATS_RPT_LEN_GRP_ID_1,
	DOT11_RRM_STATS_RPT_LEN_GRP_ID_2_9,
	DOT11_RRM_STATS_RPT_LEN_GRP_ID_2_9,
	DOT11_RRM_STATS_RPT_LEN_GRP_ID_2_9,
	DOT11_RRM_STATS_RPT_LEN_GRP_ID_2_9,
	DOT11_RRM_STATS_RPT_LEN_GRP_ID_2_9,
	DOT11_RRM_STATS_RPT_LEN_GRP_ID_2_9,
	DOT11_RRM_STATS_RPT_LEN_GRP_ID_2_9,
	DOT11_RRM_STATS_RPT_LEN_GRP_ID_2_9,
	DOT11_RRM_STATS_RPT_LEN_GRP_ID_10,
	DOT11_RRM_STATS_RPT_LEN_GRP_ID_11,
	DOT11_RRM_STATS_RPT_LEN_GRP_ID_12,
	DOT11_RRM_STATS_RPT_LEN_GRP_ID_13,
	DOT11_RRM_STATS_RPT_LEN_GRP_ID_14,
	DOT11_RRM_STATS_RPT_LEN_GRP_ID_15,
	DOT11_RRM_STATS_RPT_LEN_GRP_ID_16
};

typedef struct rrm_txstrmreq {
	uint8 token;

	uint16 duration;
	uint32 start_tsf_l;
	uint32 start_tsf_h;
	struct ether_addr peer;
	uint8 tid;
	uint8 bin0_range;

	/* TODO: trigger reporting */

	uint32 txmsdu_cnt;
	uint32 msdu_discarded_cnt;
	uint32 msdufailed_cnt;
	uint32 msduretry_cnt;
	uint32 cfpolls_lost_cnt;
	uint32 avrqueue_delay;
	uint32 avrtx_delay;
	uint32 bin0;
	uint32 bin1;
	uint32 bin2;
	uint32 bin3;
	uint32 bin4;
	uint32 bin5;
} rrm_txstrmreq_t;

typedef struct rrm_ftmreq {
	uint8 token;
	frngreq_t *frng_req;
	frngrep_t *frng_rep;
	wlc_ftm_ranging_ctx_t *rctx;
} rrm_ftmreq_t;

struct wlc_rrm_info {
	wlc_info_t *wlc;
	/* The radio measurements that use state machine use global variables as follows */
	uint8 req_token;		/* token used in measure requests from us */
	uint8 dialog_token;		/* Dialog token received in measure req */
	struct ether_addr da;
	rrm_bcnreq_t *bcnreq;
	uint8 req_elt_token;           /* element token in measure requests */
	wlc_rrm_req_state_t *rrm_state;
	struct wl_timer *rrm_timer;     /* 11h radio resource measurement timer */
	int cfgh;                       /* rrm bsscfg cubby handle */
	wlc_bsscfg_t *cur_cfg;	/* current BSS in the rrm state machine */
	int scb_handle;			/* rrm scb cubby handle */
	uint32 bcn_req_thrtl_win; /* Window (secs) in which off-chan time is computed */
	uint32 bcn_req_thrtl_win_sec; /* Seconds in to throttle window */
	uint32 bcn_req_win_scan_cnt; /* Count of scans due to bcn_req in current window */
	uint32 bcn_req_off_chan_time_allowed; /* Max scan time allowed in window (ms) */
	uint32 bcn_req_off_chan_time; /* Total ms scan time completed in throttle window */
	uint32 bcn_req_scan_start_timestamp; /* Intermediate timestamp to mark scan times */
	uint32 bcn_req_traff_meas_prd; /* ms period for checking traffic */
	uint32 data_activity_ts; /* last data activity ts */
	rrm_framereq_t *framereq;   /* frame request/report data */
	rrm_chloadreq_t *chloadreq; /* chanload request/report data */
	rrm_noisereq_t *noisereq;   /* noise histogram request/report data */
	rrm_statreq_t *statreq;     /* STA stats request/report data */
	rrm_txstrmreq_t *txstrmreq; /* Transmit stream/category request/report data */
	struct wl_timer *rrm_noise_ipi_timer; /* measurement for noise ipi */
	rrm_ftmreq_t *rrm_frng_req;
	bool direct_ranging_mode; /* TRUE = driver handles request/report, FALSE means app does */
	wlc_rrm_cmn_info_t *rrm_cmn;
};

#endif /* _wlc_rrm_priv_h_ */
