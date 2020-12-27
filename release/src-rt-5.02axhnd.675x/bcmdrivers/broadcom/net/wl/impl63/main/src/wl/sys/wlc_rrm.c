/*
 * 802.11k protocol implementation for
 * Broadcom 802.11bang Networking Device Driver
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
 * $Id: wlc_rrm.c 781790 2019-11-28 15:18:39Z $
 */

/**
 * @file
 * @brief
 * Radio Resource Management
 * Wireless LAN (WLAN) Radio Measurements enable STAs to understand the radio environment in which
 * they exist. WLAN Radio Measurements enable STAs to observe and gather data on radio link
 * performance and on the radio environment. A STA may choose to make measurements locally, request
 * a measurement from another STA, or may be requested by another STA to make one or more
 * measurements and return the results. Radio Measurement data is made available to STA management
 * and upper protocol layers where it may be used for a range of applications. The measurements
 * enable adjustment of STA operation to better suit the radio environment.
 */

/**
 * @file
 * @brief
 * XXX Twiki: [WlDriver80211k]
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
#include <802.11.h>
#include <wlioctl.h>
#include <sbhndpio.h>
#include <sbhnddma.h>
#include <hnddma.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_channel.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wlc_scan.h>
#include <wlc_rrm.h>
#include <bcmwpa.h>
#include <wlc_assoc.h>
#include <wl_dbg.h>
#include <wlc_tpc.h>
#include <wlc_11h.h>
#include <wl_export.h>
#include <wlc_utils.h>
#include <wlc_bmac.h>
#include <wlc_ie_mgmt.h>
#include <wlc_ie_mgmt_ft.h>
#include <wlc_ie_helper.h>
#include <wlc_hw.h>
#include <wlc_pm.h>
#include <wlc_scan_utils.h>
#include <wlc_event_utils.h>
#include <phy_misc_api.h>
#include <wlc_iocv.h>
#include <wlc_cntry.h>
#if defined(WL_PROXDETECT) && defined(WL_FTM)
#include <wlc_ftm.h>
#include <wlc_ht.h>
#include <wlc_vht.h>
#endif /* WL_PROXDETECT && WL_FTM */
#include <bcmwifi_rclass.h>
#include <wlc_rrm_priv.h>
#ifdef WLMCNX
#include <wlc_mcnx.h>
#endif // endif
#ifdef MFP
#include <wlc_mfp.h>
#endif // endif
#include <wlc_stf.h>
#ifdef WLAMSDU
#include <wlc_amsdu.h>
#endif // endif
#ifdef WLAMPDU
#include <wlc_ampdu.h>
#include <wlc_ampdu_rx.h>
#include <wlc_ampdu_cmn.h>
#endif // endif
#include <wlc_lq.h>
#include <phy_noise_api.h>
#include <phy_stf_api.h>

#include <wlc_wnm.h>
#if defined(WL_MBO) && defined(MBO_AP)
#include <mbo.h>
#include <wlc_mbo.h>
#include <wlc_bsscfg_viel.h>
#endif /* WL_MBO && MBO_AP */

#define WLC_RRM_MS_IN_SEC 1000

#define WLC_RRM_UPDATE_TOKEN(i) \
	{ ++i; if (i == 0) ++i; }

#define WLC_RRM_ELEMENT_LIST_ADD(s, e) \
	do { \
		(e)->next = (s); \
		(s) = (e); \
	} while (0)

/* Beacon report modes. */
#define WLC_RRM_CHANNEL_REPORT_MODE(a) (a->channel == 255)
#define WLC_RRM_REPORT_ALL_CHANNELS(a) (a->channel == 0)

#define WLC_RRM_MIN_TIMER   20 /* (ms) min time for a measure */
#define WLC_RRM_PREP_MARGIN 30 /* (ms) time to prepare for a measurement on a different channel */
#define WLC_RRM_HOME_TIME   40 /* (ms) min time on home channel between off-channel measurements */
#define WLC_RRM_MAX_REP_NUM 65535 /* Max number of repetitions */

#define WLC_RRM_MIN_MEAS_INTVL  0x0200  /* TUs - a little time to settle before measure-start */
#define WLC_RRM_MAX_MEAS_DUR 0x0400 /* MAx meas-dur if channel switch -- about 1sec */

#define MAX_NEIGHBORS 64
#define WLC_RRM_BCNREQ_INTERVAL 180000 /* 3 minutes */
#define WLC_RRM_BCNREQ_INTERVAL_DUR 20
#define MAX_BEACON_NEIGHBORS 5
#define MAX_CHANNEL_REPORT_IES 8
#define WLC_RRM_BCNREQ_NUM_REQ_ELEM 8 /* number of requested elements for reporting detail 1 */
#define WLC_RRM_WILDCARD_SSID_IND   "*" /* indicates the wildcard SSID (SSID len of 0) */

#define RRM_NREQ_FLAGS_LCI_PRESENT		0x1
#define RRM_NREQ_FLAGS_CIVICLOC_PRESENT		0x2
#define RRM_NREQ_FLAGS_OWN_REPORT		0x4
#define RRM_NREQ_FLAGS_BSS_PREF			0x8
#define RRM_NREQ_FLAGS_COLOCATED_BSSID		0x10 /* add colocated BSSID subelement */

/* Radio Measurement states */
#define WLC_RRM_IDLE                     0 /* Idle */
#define WLC_RRM_ABORT                    1 /* Abort */
#define WLC_RRM_WAIT_START_SET           2 /* Wait Start set */
#define WLC_RRM_WAIT_STOP_TRAFFIC        3 /* Wait Stop traffic */
#define WLC_RRM_WAIT_TX_SUSPEND          4 /* Wait Tx Suspend */
#define WLC_RRM_WAIT_PS_ANNOUNCE         5 /* Wait PS Announcement */
#define WLC_RRM_WAIT_BEGIN_MEAS          6 /* Wait Begin Measurement */
#define WLC_RRM_WAIT_END_MEAS            7 /* Wait End Measurement */
#define WLC_RRM_WAIT_END_CCA             8 /* Wait End CCA */
#define WLC_RRM_WAIT_END_SCAN            9  /* Wait End Scan */
#define WLC_RRM_WAIT_END_FRAME           10 /* Wait End Frame Measurement */

#define WLC_RRM_NOISE_SUPPORTED(rrm_info) TRUE /* except obsolete bphy, all current phy support */

/* a radio measurement is in progress unless is it IDLE,
 * ABORT, or waiting to start or channel switch for a set
 */
#define WLC_RRM_IN_PROGRESS(wlc) (WL11K_ENAB(wlc->pub) && \
		!(((wlc)->rrm_info)->rrm_state->step == WLC_RRM_IDLE ||	\
		((wlc)->rrm_info)->rrm_state->step == WLC_RRM_ABORT ||	\
		((wlc)->rrm_info)->rrm_state->step == WLC_RRM_WAIT_STOP_TRAFFIC || \
		((wlc)->rrm_info)->rrm_state->step == WLC_RRM_WAIT_START_SET))

#define RRM_BSSCFG_CUBBY(rrm_info, cfg) ((rrm_bsscfg_cubby_t *)BSSCFG_CUBBY(cfg, (rrm_info)->cfgh))

#define RRM_NBR_REP_HEAD(rrm_cfg, addtype) \
	((addtype == NBR_ADD_DYNAMIC) ? rrm_cfg->nbr_rep_head_dyn : rrm_cfg->nbr_rep_head)
#define RRM_NBR_REP_HEADPTR(rrm_cfg, addtype) \
	((addtype == NBR_ADD_DYNAMIC) ? &(rrm_cfg->nbr_rep_head_dyn) : &(rrm_cfg->nbr_rep_head))

/* iovar table */
enum {
	IOV_RRM = 1,
	IOV_RRM_NBR_REQ = 2,
	IOV_RRM_BCN_REQ = 3,
	IOV_RRM_CHLOAD_REQ = 4,
	IOV_RRM_NOISE_REQ = 5,
	IOV_RRM_FRAME_REQ = 6,
	IOV_RRM_STAT_REQ = 7,
	IOV_RRM_STAT_RPT = 8,
	IOV_RRM_TXSTREAM_REQ = 9,
	IOV_RRM_LCI_REQ = 10,
	IOV_RRM_CIVIC_REQ = 11,
	IOV_RRM_LOCID_REQ = 12,
	IOV_RRM_LM_REQ = 13,
	IOV_RRM_NBR_LIST = 14,
	IOV_RRM_NBR_DEL_NBR = 15,
	IOV_RRM_NBR_ADD_NBR = 16,
	IOV_RRM_BCN_REQ_THRTL_WIN = 17,
	IOV_RRM_BCN_REQ_MAX_OFF_CHAN_TIME = 18,
	IOV_RRM_BCN_REQ_TRAFF_MEAS_PER = 19,
	IOV_RRM_CONFIG = 20,
	IOV_RRM_FRNG = 21,
	IOV_RRM_NBR_SCAN = 22,
	IOV_RRM_NBR_REPORT = 23,
	IOV_RRM_NBR = 24,
	IOV_RRM_LAST
};

static const bcm_iovar_t rrm_iovars[] = {
	{"rrm", IOV_RRM, IOVF_SET_DOWN, 0, IOVT_INT32, 0},
	{"rrm_nbr_req", IOV_RRM_NBR_REQ, (IOVF_SET_UP), 0, IOVT_BUFFER, 0},
	{"rrm_bcn_req", IOV_RRM_BCN_REQ, (IOVF_SET_UP), 0, IOVT_BUFFER, 0},
	{"rrm_lm_req", IOV_RRM_LM_REQ, (IOVF_SET_UP), 0, IOVT_BUFFER, 0},
	{"rrm_chload_req", IOV_RRM_CHLOAD_REQ, (IOVF_SET_UP), 0, IOVT_BUFFER, 0},
	{"rrm_noise_req", IOV_RRM_NOISE_REQ, (IOVF_SET_UP), 0, IOVT_BUFFER, 0},
	{"rrm_frame_req", IOV_RRM_FRAME_REQ, (IOVF_SET_UP), 0, IOVT_BUFFER, 0},
	{"rrm_stat_req", IOV_RRM_STAT_REQ, (IOVF_SET_UP), 0, IOVT_BUFFER, 0},
	{"rrm_stat_rpt", IOV_RRM_STAT_RPT, (IOVF_SET_UP), 0, IOVT_BUFFER, sizeof(statrpt_t)},
	{"rrm_txstrm_req", IOV_RRM_TXSTREAM_REQ, (IOVF_SET_UP), 0, IOVT_BUFFER, 0},
	{"rrm_lci_req", IOV_RRM_LCI_REQ, (IOVF_SET_UP), 0, IOVT_BUFFER, 0},
	{"rrm_civic_req", IOV_RRM_CIVIC_REQ, (IOVF_SET_UP), 0, IOVT_BUFFER, 0},
	{"rrm_locid_req", IOV_RRM_LOCID_REQ, (IOVF_SET_UP), 0, IOVT_BUFFER, 0},
	{"rrm_nbr_list", IOV_RRM_NBR_LIST, (IOVF_GET_UP), 0, IOVT_BUFFER, 0},
	{"rrm_nbr_del_nbr", IOV_RRM_NBR_DEL_NBR, 0, 0, IOVT_BUFFER, 0},
	{"rrm_nbr_add_nbr", IOV_RRM_NBR_ADD_NBR, 0, 0, IOVT_BUFFER, 0},
	{"rrm_bcn_req_thrtl_win", IOV_RRM_BCN_REQ_THRTL_WIN, (IOVF_RSDB_SET), 0, IOVT_UINT32, 0},
	{"rrm_bcn_req_max_off_chan_time", IOV_RRM_BCN_REQ_MAX_OFF_CHAN_TIME,
	(IOVF_RSDB_SET), 0, IOVT_UINT32, 0
	},
	{"rrm_bcn_req_traff_meas_per", IOV_RRM_BCN_REQ_TRAFF_MEAS_PER,
	(IOVF_SET_UP|IOVF_RSDB_SET), 0, IOVT_UINT32, 0
	},
	{"rrm_config", IOV_RRM_CONFIG, 0, 0, IOVT_BUFFER, 0},
#ifdef WL_FTM_11K
#if defined(WL_PROXDETECT) && defined(WL_FTM)
	{"rrm_frng", IOV_RRM_FRNG, (IOVF_SET_UP), 0, IOVT_BUFFER, 0},
	{"rrm_nbr", IOV_RRM_NBR, 0, 0, IOVT_BUFFER, 0},
#endif /* WL_PROXDETECT && WL_FTM */
#endif /* WL_FTM_11K */
	{"rrm_nbr_scan", IOV_RRM_NBR_SCAN, 0, 0, IOVT_UINT8, 0},
	{"rrm_nbr_report", IOV_RRM_NBR_REPORT, (IOVF_GET_UP), 0, IOVT_BUFFER, 0},
	{NULL, 0, 0, 0, 0, 0}
};

/* Neighbor Report element */
typedef struct rrm_nbr_rep {
	struct rrm_nbr_rep *next;
	nbr_rpt_elem_t nbr_elt;
	uint8 *opt_lci;
	uint8 *opt_civic;
	uint8 *opt_locid;
} rrm_nbr_rep_t;

/* AP Channel Report */
#define VALID_NBR_CHAN(_chan) (((_chan) > 0) && ((_chan) < MAXCHANNEL))
#define VALID_NBR_REP_CHAN(_nrep) VALID_NBR_CHAN((_nrep)->nbr_elt.channel)
typedef struct reg_nbr_count {
	struct reg_nbr_count *next;
	uint8 reg;
	uint16 nbr_count_by_channel[MAXCHANNEL];
} reg_nbr_count_t;

typedef struct {
	wlc_info_t *wlc;
	wlc_rrm_info_t *rrm_info;
	uint8 req_token;			/* token used in measure requests from us */
	uint8 dialog_token;			/* Dialog token received in measure req */
	struct ether_addr da;
	rrm_nbr_rep_t *nbr_rep_head;		/* Neighbor Report element */
	reg_nbr_count_t *nbr_cnt_head;		/* AP Channel Report */
	uint8 rrm_cap[DOT11_RRM_CAP_LEN];	/* RRM Enabled Capability */
	uint32 bcn_req_timer;			/* Auto Beacon request timer */
	bool rrm_timer_set;
	rrm_nbr_rep_t *nbr_rep_head_dyn;	/* AutoLearnt Neighbor Report */
	rrm_nbr_rep_t *nbr_rep_self;		/* Neighbor Report element for self */
#ifdef WL_FTM_11K
	uint8 lci_token;                        /* LCI token received in measure req */
	uint8 civic_token;                      /* CIVIC loc token received in measure req */
#endif /* WL_FTM_11K */
	struct wl_timer *pilot_timer;		/* Measurement Piolt timer */
	uint8 mp_period;
	uint8 nbr_scan;				/* Enable dynamic nbr report */
} rrm_bsscfg_cubby_t;

typedef struct rrm_scb_info {
	uint32 timestamp;	/* timestamp of the report */
	uint16 flag;		/* flag */
	uint16 len;			/* length of payload data */
	unsigned char data[WL_RRM_RPT_MAX_PAYLOAD];
	uint8 rrm_capabilities[DOT11_RRM_CAP_LEN];	/* rrm capabilities of station */
	uint32 bcnreq_time;
	rrm_nbr_rep_t *nbr_rep_head;
} rrm_scb_info_t;

/* new counter for 802.11K stats linked by scb pointer scb_rrm_stats */
typedef struct _scb_rrm_stats {
	rrm_stat_group_qos_t scb_qos_stats[NUMPRIO];
	rrm_stat_group_13_t scb_bw_stats;
} scb_rrm_stats_t;

typedef struct scb_tscm {
	rrm_tscm_t scb_qos_tscm[NUMPRIO];
} scb_tscm_t;

typedef struct rrm_scb_cubby {
	rrm_scb_info_t *scb_info;
	scb_rrm_stats_t *scb_rrm_stats;
	scb_tscm_t *scb_rrm_tscm;
} rrm_scb_cubby_t;
#define RRM_SCB_CUBBY(rrm_info, scb) (rrm_scb_cubby_t *)SCB_CUBBY(scb, (rrm_info->scb_handle))
#define RRM_SCB_INFO(rrm_info, scb) (RRM_SCB_CUBBY(rrm_info, scb))->scb_info

typedef int (*rrm_config_fn_t)(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	uint8 *opt, int len);

#define WLCCNTSTATGROUPDELTA(_fullsrc, _gid, _dst, delta)	\
	do {									\
		if (delta) {						\
			stat_req->sta_data.group##_gid._dst =	\
				_fullsrc - stat_req->sta_data.group##_gid._dst;	\
		} else {						\
			stat_req->sta_data.group##_gid._dst = _fullsrc;	\
		}								\
	} while (0)

#define WLCCNTSTATGROUPQOS(tid, _src, _dst, delta)	\
	do {									\
		if (delta) {						\
			stat_req->sta_data.group_qos._dst =	\
				rrm_scb_stats->scb_qos_stats[tid]._src -	\
				stat_req->sta_data.group_qos._dst;	\
		} else {						\
			stat_req->sta_data.group_qos._dst =	\
				rrm_scb_stats->scb_qos_stats[tid]._src;	\
		}								\
	} while (0)

#define WL_RRM_EVENT_BUF_MAX_SIZE  1016

static void wlc_rrm_parse_requests(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, dot11_rm_ie_t *ie,
	int len, wlc_rrm_req_t *req, int count);
static void wlc_rrm_free(wlc_rrm_info_t *rrm_info);

static void wlc_rrm_recv_rmreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, struct scb *scb,
	uint8 *body, int body_len);
static void wlc_rrm_recv_rmrep(wlc_rrm_info_t *rrm_info, struct scb *scb, uint8 *body,
	int body_len);

#ifdef WL11K_BCN_MEAS
static int wlc_rrm_bcnrep_add(wlc_rrm_info_t *rrm_info, wlc_bss_info_t *bi,
	uint8 *bufptr, uint buflen);
static bool wlc_rrm_request_current_regdomain(wlc_rrm_info_t *rrm_info,
	dot11_rmreq_bcn_t *rmreq_bcn, rrm_bcnreq_t *bcn_req, wlc_rrm_req_t *req);
static unsigned int wlc_rrm_ap_chreps_to_chanspec(bcm_tlv_t *tlvs, int tlvs_len, rrm_bcnreq_t
	*bcn_req);
static void wlc_rrm_recv_bcnreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	dot11_rmreq_bcn_t *rmreq_bcn, wlc_rrm_req_t *req);
static dot11_ap_chrep_t* wlc_rrm_get_ap_chrep(const wlc_bss_info_t *bi);
static uint8 wlc_rrm_get_ap_chrep_reg(const wlc_bss_info_t *bi);
static void wlc_rrm_bcnreq_scancb(void *arg, int status, wlc_bsscfg_t *cfg);
static int wlc_rrm_add_empty_bcnrep(const wlc_rrm_info_t *rrm_info, uint8 *bufptr, uint buflen);
static int32 wlc_rrm_send_bcnrep(wlc_rrm_info_t *rrm_info, wlc_bss_list_t *bsslist);
static void wlc_rrm_bcnreq_scancache(wlc_rrm_info_t *rrm_info, wlc_ssid_t *ssid,
	struct ether_addr *bssid);
static void wlc_rrm_send_bcnreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, bcn_req_t *bcnreq);
static void wlc_rrm_recv_bcnrep(wlc_rrm_info_t *rrm_info, struct scb *scb, dot11_rm_ie_t *ie);
static uint32 wlc_rrm_get_rm_hdr_ptr(wlc_rrm_info_t *rrm_info, uint8 *pbody, uint32 body_len,
	uint8 **buf);
#endif /* WL11K_BCN_MEAS */

static void wlc_rrm_framereq_measure(wlc_rrm_info_t *rrm_info);

#ifdef WL11K_ALL_MEAS
static void wlc_rrm_cca_start(wlc_rrm_info_t  *rrm_info, uint32 dur);
static void wlc_rrm_cca_finish(wlc_rrm_info_t  *rrm_info);
static int wlc_rrm_framerep_add(wlc_rrm_info_t *rrm_info, uint8 *bufptr, uint cnt);
static const int8 wlc_noise_ipi_bin_max[WL_RRM_IPI_BINS_NUM] =
	{ -92, -89, -86, -83, -80, -75, -70, -65, -60, -55, 0x7f };
static bool wlc_rrm_noise_ipi_sample(wlc_rrm_info_t *rrm_info, int8 noise);
static void wlc_rrm_noise_ipi_begin(wlc_rrm_info_t *rrm_info);
static void wlc_rrm_noise_ipi_timer(void *arg);

static void wlc_rrm_recv_framereq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	dot11_rmreq_frame_t *rmreq_frame, wlc_rrm_req_t *req);
static void wlc_rrm_recv_noisereq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	dot11_rmreq_noise_t *rmreq_noise, wlc_rrm_req_t *req);
static void wlc_rrm_recv_chloadreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	dot11_rmreq_chanload_t *rmreq_chload, wlc_rrm_req_t *req);
static void wlc_rrm_recv_statreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	dot11_rmreq_stat_t *rmreq_stat, wlc_rrm_req_t *req);
static void wlc_rrm_recv_txstrmreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
    dot11_rmreq_tx_stream_t *rmreq_txstrm, wlc_rrm_req_t *req);
static void wlc_rrm_send_chloadrep(wlc_rrm_info_t *rrm_info);
static void wlc_rrm_send_noiserep(wlc_rrm_info_t *rrm_info);
static void wlc_rrm_send_framerep(wlc_rrm_info_t *rrm_info);
static void wlc_rrm_send_statrep(wlc_rrm_info_t *rrm_info);
static void wlc_rrm_send_txstrmrep(wlc_rrm_info_t *rrm_info);
static void wlc_rrm_send_lcirep(wlc_rrm_info_t *rrm_info, uint8 token);
static void wlc_rrm_send_civiclocrep(wlc_rrm_info_t *rrm_info, uint8 token);
static void wlc_rrm_send_locidrep(wlc_rrm_info_t *rrm_info, uint8 token);
static void wlc_rrm_recv_lmreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, struct scb *scb,
	uint8 *body, int body_len, int8 rssi, ratespec_t rspec);
static void wlc_rrm_recv_lmrep(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, struct scb *scb,
	uint8 *body, int body_len);
static void wlc_rrm_send_lmreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	struct ether_addr *da);
static void wlc_rrm_send_noisereq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	rrmreq_t *noisereq);
static void wlc_rrm_send_chloadreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	rrmreq_t *chloadreq);
static void wlc_rrm_send_framereq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	framereq_t *framereq);
static void wlc_rrm_send_statreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	statreq_t *statreq);
static void wlc_rrm_send_txstrmreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	txstrmreq_t *txstrmreq);
#ifdef WL_PROXDETECT
static int wlc_rrm_send_lcireq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	lci_req_t *lcireq);
static int wlc_rrm_send_civicreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	civic_req_t *civicreq);
#else
static void wlc_rrm_send_lcireq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	lcireq_t *lcireq);
static void wlc_rrm_send_civicreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	civicreq_t *civicreq);
#endif // endif
static void wlc_rrm_send_locidreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	locidreq_t *locidreq);
static int rrm_config_get(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	void *p, uint plen, void *a, int alen);
static int rrm_config_set(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	void *p, uint plen, void *a, int alen);
static int wlc_rrm_get_self_data(dot11_meas_rep_t *rep,
	wl_rrm_config_ioc_t *rrm_cfg_cmd, int alen);
static int wlc_rrm_alloc_nbr_rep_self(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg);
static int wlc_rrm_add_loc(wlc_rrm_info_t *rrm_info, uint8 **loc,
	uint8 *new_loc, int len, uint8 type);
static int wlc_rrm_set_self_lci(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	uint8 *opt_lci,	int len);
static int wlc_rrm_get_self_lci(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	uint8 *a, int alen);
static int wlc_rrm_get_self_civic(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	uint8 *a, int alen);
static int wlc_rrm_set_self_civic(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	uint8 *opt_civic, int len);
static int wlc_rrm_get_self_locid(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	uint8 *a, int alen);
static int wlc_rrm_set_self_locid(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	uint8 *opt_locid, int len);
static void wlc_rrm_free_self_report(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *bsscfg);

static void wlc_rrm_recv_lcireq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	dot11_meas_req_loc_t *rmreq_lci, wlc_rrm_req_t *req);
static void wlc_rrm_recv_civicreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	dot11_meas_req_loc_t *rmreq_civic, wlc_rrm_req_t *req);
static void wlc_rrm_recv_locidreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	dot11_meas_req_loc_t *rmreq_locid, wlc_rrm_req_t *req);
static uint8 * wlc_rrm_create_colocatedbssid_ie(wlc_rrm_info_t *rrm_info,
	uint8 *data_ptr, int *buflen);

static rrm_config_fn_t rrmconfig_cmd_fn[] = {
	NULL, /* WL_RRM_CONFIG_NONE */
	wlc_rrm_get_self_lci, /* WL_RRM_CONFIG_GET_LCI */
	wlc_rrm_set_self_lci, /* WL_RRM_CONFIG_SET_LCI */
	wlc_rrm_get_self_civic, /* WL_RRM_CONFIG_GET_CIVIC */
	wlc_rrm_set_self_civic, /* WL_RRM_CONFIG_SET_CIVIC */
	wlc_rrm_get_self_locid, /* WL_RRM_CONFIG_GET_LOCID */
	wlc_rrm_set_self_locid, /* WL_RRM_CONFIG_SET_LOCID */
};
#endif /* WL11K_ALL_MEAS */

static int wlc_rrm_doiovar(void *hdl, uint32 actionid,
	void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif);
static void wlc_rrm_rep_err(wlc_rrm_info_t *rrm_info, uint8 type, uint8 token, uint8 reason);

static void wlc_rrm_state_upd(wlc_rrm_info_t  *rrm_info, uint state);
static void wlc_rrm_timer(void *arg);
#ifdef WL11K_NBR_MEAS
#ifdef STA
static void wlc_rrm_recv_nrrep(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, struct scb *scb,
	uint8 *body, int body_len);
static void wlc_rrm_regclass_neighbor_count(wlc_rrm_info_t *rrm_info, rrm_nbr_rep_t *nbr_rep,
	wlc_bsscfg_t *cfg);
static bool wlc_rrm_regclass_match(wlc_rrm_info_t *rrm_info, rrm_nbr_rep_t *nbr_rep,
	wlc_bsscfg_t *cfg);
static void wlc_rrm_send_nbrreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, wlc_ssid_t *ssid);
#ifdef WLASSOC_NBR_REQ
static void wlc_rrm_sta_assoc_state_upd(void *ctx, bss_assoc_state_data_t *assoc_state_info);
#endif /* WLASSOC_NBR_REQ */
#endif /* STA */
#ifdef WL11K_AP
static void wlc_rrm_recv_nrreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, struct scb *scb,
	uint8 *body, int body_len);
static void wlc_rrm_send_nbrrep(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, struct scb *scb,
	wlc_ssid_t *ssid, uint8 flags, int addtype);
static void wlc_rrm_get_neighbor_list(wlc_rrm_info_t *rrm_info, nbr_rpt_elem_t *nbr_elements,
	uint16 list_cnt, rrm_bsscfg_cubby_t *rrm_cfg);
static void wlc_rrm_del_neighbor(wlc_rrm_info_t *rrm_info, struct ether_addr *ea,
	struct wlc_if *wlcif, rrm_bsscfg_cubby_t *rrm_cfg);
static void wlc_rrm_remove_regclass_nbr_cnt(wlc_rrm_info_t *rrm_info,
	rrm_nbr_rep_t *nbr_rep, bool delete_empty_reg, bool *channel_removed,
	rrm_bsscfg_cubby_t *rrm_cfg);
static void wlc_rrm_add_neighbor(wlc_rrm_info_t *rrm_info, nbr_rpt_elem_t *nbr_elt,
	struct wlc_if *wlcif, rrm_bsscfg_cubby_t *rrm_cfg, int addtype);
static uint16 _wlc_rrm_get_neighbor_count(wlc_rrm_info_t *rrm_info,
	rrm_bsscfg_cubby_t *rrm_cfg, int addtype);
void wlc_rrm_add_neighbor_chanspec(nbr_rpt_elem_t *nbr_elt, rrm_bsscfg_cubby_t *rrm_cfg);
void wlc_rrm_add_neighbor_ssid(nbr_rpt_elem_t *nbr_elt, rrm_bsscfg_cubby_t *rrm_cfg);
int wlc_rrm_add_neighbor_lci(wlc_rrm_info_t *rrm_info, struct ether_addr *nbr_bss,
	uint8 *opt_lci, int len, rrm_bsscfg_cubby_t *rrm_cfg);
int wlc_rrm_add_neighbor_civic(wlc_rrm_info_t *rrm_info, struct ether_addr *nbr_bss,
	uint8 *opt_civic, int len, rrm_bsscfg_cubby_t *rrm_cfg);
static void wlc_rrm_add_regclass_neighbor_count(wlc_rrm_info_t *rrm_info, rrm_nbr_rep_t *nbr_rep,
	bool *channel_added, rrm_bsscfg_cubby_t *rrm_cfg);
static uint16 wlc_rrm_get_reg_count(wlc_rrm_info_t *rrm_info, rrm_bsscfg_cubby_t *rrm_cfg);
static void wlc_rrm_ap_scb_state_upd(void *ctx, scb_state_upd_data_t *data);
static void wlc_rrm_add_timer(wlc_info_t *wlc, struct scb *scb);
#ifdef WL11K_BCN_MEAS
static uint16 wlc_rrm_get_bcnnbr_count(wlc_rrm_info_t *rrm_info, rrm_scb_info_t *scb_info);
static bool wlc_rrm_security_match(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, uint16 cap,
	bcm_tlv_t *wpaie, bcm_tlv_t *wpa2ie);
static int wlc_rrm_add_bcnnbr(wlc_rrm_info_t *rrm_info, struct scb *scb,
	dot11_rmrep_bcn_t *rmrep_bcn, struct dot11_bcn_prb *bcn, bcm_tlv_t *htie,
	bcm_tlv_t *mdie, uint32 flags);
#endif /* WL11K_BCN_MEAS */
static void wlc_rrm_flush_bcnnbrs(wlc_rrm_info_t *rrm_info, struct scb *scb);
#endif /* WL11K_AP */
static void wlc_rrm_flush_neighbors(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *bsscfg,
	int addtype);
#ifdef BCMDBG
static void wlc_rrm_dump_neighbors(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *bsscfg, int addtype);
#endif /* BCMDBG */
#endif /* WL11K_NBR_MEAS */

static dot11_rm_ie_t *wlc_rrm_next_ie(dot11_rm_ie_t *ie, int *len, uint8 mea_id);
static void wlc_rrm_report_ioctl(wlc_rrm_info_t *rrm_info, wlc_rrm_req_t *req_block, int count);
static void wlc_rrm_next_set(wlc_rrm_info_t *rrm_info);
static chanspec_t wlc_rrm_chanspec(wlc_rrm_info_t *rrm_info);
static void wlc_rrm_update_beacon(wlc_info_t *wlc, wlc_bsscfg_t *cfg);

#if defined(WL11K_BCN_MEAS) || defined(WL11K_NBR_MEAS) || defined(WL11K_ALL_MEAS)
static void* wlc_rrm_prep_gen_report(wlc_rrm_info_t *rrm_info, unsigned int len, uint8 **pbody);
static void* wlc_rrm_prep_gen_request(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	struct ether_addr *da, unsigned int buflen, uint8 **pbody);
#endif // endif

static void wlc_rrm_process_rep(wlc_rrm_info_t *rrm_info, struct scb *scb,
	dot11_rm_action_t *rrm_rep, int rep_len);
static void wlc_rrm_parse_response(wlc_rrm_info_t *rrm_info, struct scb *scb, dot11_rm_ie_t* ie,
	int len);
#ifdef WL11K_ALL_MEAS
static void wlc_rrm_recv_statrep(wlc_rrm_info_t *rrm_info, struct scb *scb, dot11_rm_ie_t *ie);
#ifdef BCMDBG
static void wlc_rrm_recv_noiserep(wlc_rrm_info_t *rrm_info, struct scb *scb, dot11_rm_ie_t *ie);
static void wlc_rrm_recv_chloadrep(wlc_rrm_info_t *rrm_info, struct scb *scb, dot11_rm_ie_t *ie);
static void wlc_rrm_recv_framerep(wlc_rrm_info_t *rrm_info, struct scb *scb, dot11_rm_ie_t *ie);
static void wlc_rrm_recv_txstrmrep(wlc_rrm_info_t *rrm_info, struct scb *scb, dot11_rm_ie_t *ie);
static void wlc_rrm_recv_lcirep(wlc_rrm_info_t *rrm_info, struct scb *scb, dot11_rm_ie_t *ie);
static void wlc_rrm_recv_civicrep(wlc_rrm_info_t *rrm_info, struct scb *scb, dot11_rm_ie_t *ie);
static void wlc_rrm_recv_locidrep(wlc_rrm_info_t *rrm_info, struct scb *scb, dot11_rm_ie_t *ie);
#endif /* BCMDBG */
#endif /* WL11K_ALL_MEAS */

/* send event to user-space parser (eventd) */
static void wlc_rrm_send_event(wlc_rrm_info_t *rrm_info, struct scb *scb, char *ie, int len,
	int16 cat, int16 subevent);

static void wlc_rrm_meas_end(wlc_info_t* wlc);
static void wlc_rrm_start(wlc_info_t* wlc);
static int wlc_rrm_abort(wlc_info_t* wlc);

static int wlc_rrm_bsscfg_init(void *context, wlc_bsscfg_t *bsscfg);
static void wlc_rrm_bsscfg_deinit(void *context, wlc_bsscfg_t *bsscfg);
static void wlc_rrm_scb_deinit(void *ctx, struct scb *scb);
static int wlc_rrm_scb_init(void *ctx, struct scb *scb);
static uint wlc_rrm_scb_secsz(void *ctx, struct scb *scb);
#ifdef BCMDBG
static void wlc_rrm_bsscfg_dump(void *context, wlc_bsscfg_t *bsscfg, struct bcmstrbuf *b);
static void wlc_rrm_scb_dump(void *ctx, struct scb *scb, struct bcmstrbuf *b);
#else
#define wlc_rrm_bsscfg_dump NULL
#define wlc_rrm_scb_dump NULL
#endif /* BCMDBG */

static void wlc_rrm_watchdog(void *context);

/* IE mgmt */
#ifdef WL11K_ALL_MEAS
static uint wlc_rrm_calc_cap_ie_len(void *ctx, wlc_iem_calc_data_t *data);
static int wlc_rrm_write_cap_ie(void *ctx, wlc_iem_build_data_t *data);
#ifdef WL11K_AP
static uint wlc_rrm_ap_chrep_len(wlc_rrm_info_t *rrm, wlc_bsscfg_t *cfg);
static void wlc_rrm_add_ap_chrep(wlc_rrm_info_t *rrm, wlc_bsscfg_t *cfg, uint8 *bufstart);
static uint wlc_rrm_calc_ap_ch_rep_ie_len(void *ctx, wlc_iem_calc_data_t *data);
static int wlc_rrm_write_ap_ch_rep_ie(void *ctx, wlc_iem_build_data_t *data);
static uint wlc_rrm_calc_mptx_ie_len(void *ctx, wlc_iem_calc_data_t *data);
static int wlc_rrm_write_mptx_ie(void *ctx, wlc_iem_build_data_t *data);
static int wlc_assoc_parse_rrm_ie(void *ctx, wlc_iem_parse_data_t *data);
#endif /* WL11K_AP */

#ifdef STA
static uint wlc_rrm_calc_assoc_cap_ie_len(void *ctx, wlc_iem_calc_data_t *data);
static int wlc_rrm_write_assoc_cap_ie(void *ctx, wlc_iem_build_data_t *data);
static int wlc_rrm_parse_rrmcap_ie(void *ctx, wlc_iem_parse_data_t *data);
#endif /* STA */
#endif /* WL11K_ALL_MEAS */

#ifdef WL_FTM_11K
#if defined(WL_PROXDETECT) && defined(WL_FTM)
static uint8 * wlc_rrm_create_lci_meas_element(uint8 *ptr, rrm_bsscfg_cubby_t *rrm_cfg,
	rrm_nbr_rep_t *nbr_rep, int *buflen, uint8 flags);
static uint8 * wlc_rrm_create_civic_meas_element(uint8 *ptr, rrm_bsscfg_cubby_t *rrm_cfg,
	rrm_nbr_rep_t *nbr_rep, int *buflen, uint8 flags);
static int rrm_frng_set(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	void *p, uint plen, void *a, int alen);
static int wlc_rrm_recv_frngreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	dot11_rmreq_ftm_range_t *rmreq_ftmrange, wlc_rrm_req_t *req);
static int wlc_rrm_recv_frngrep(wlc_rrm_info_t *rrm_info, struct scb *scb,
	dot11_rmrep_ftm_range_t *rmrep_ftmrange, int len);
static int wlc_rrm_send_frngreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	uint8 *data, int len);
static int wlc_rrm_send_frngrep(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	uint8 *data, int len);
static int wlc_rrm_set_frngdir(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	uint8 *data, int len);
static int wlc_rrm_create_frngrep_element(uint8 **ptr, rrm_bsscfg_cubby_t *rrm_cfg,
	frngrep_t *frngrep);
static void wlc_rrm_frngreq_free(wlc_rrm_info_t *rrm_info);
typedef int (*rrm_frng_fn_t)(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	uint8 *data, int len);

static rrm_frng_fn_t rrmfrng_cmd_fn[] = {
	NULL, /* WL_RRM_FRNG_NONE */
	wlc_rrm_send_frngreq, /* WL_RRM_FRNG_SET_REQ send range request */
	wlc_rrm_send_frngrep, /* WL_RRM_FRNG_SET_REP app manually send range report */
	wlc_rrm_set_frngdir, /* WL_RRM_FRNG_SET_DIR whether driver or app handles range request */
	NULL /* WL_RRM_FRNG_MAX */
};
#endif /* WL_PROXDETECT && WL_FTM */
#endif /* WL_FTM_11K */

#if (defined(WL11K_AP) && defined(WL11k_NBR_MEAS)) || (defined(WL_FTM_11K) && \
	defined(WL_PROXDETECT) && defined(WL_FTM) && defined(WL11AC))
static void wlc_rrm_create_nbrrep_element(rrm_bsscfg_cubby_t *rrm_cfg, uint8 **ptr,
	rrm_nbr_rep_t *nbr_rep, uint8 flags, uint8 *bufend);
#endif /* (WL11K_AP && NBR_MEAS) || (WL_FTM_11K && WL_PROXDETECT && WL_FTM && WL11AC) */

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

wlc_rrm_info_t *
BCMATTACHFN(wlc_rrm_attach)(wlc_info_t *wlc)
{
	wlc_rrm_info_t *rrm_info = NULL;
	scb_cubby_params_t cubby_params;
#ifdef WL11K_ALL_MEAS
#ifdef WL11K_AP
	uint16 bcnfstbmp = FT2BMP(FC_BEACON) | FT2BMP(FC_PROBE_RESP);
	uint16 rrmcapfstbmp = FT2BMP(FC_BEACON) | FT2BMP(FC_PROBE_RESP) |
		FT2BMP(FC_ASSOC_RESP) | FT2BMP(FC_REASSOC_RESP);
	uint16 arqfstbmp = FT2BMP(FC_ASSOC_REQ) | FT2BMP(FC_REASSOC_REQ);
#endif /* WL11K_AP */
#ifdef STA
	uint16 assocrrmcapfstbmp = FT2BMP(FC_ASSOC_REQ) | FT2BMP(FC_REASSOC_REQ);
	uint16 arsfstbmp = FT2BMP(FC_ASSOC_RESP) | FT2BMP(FC_REASSOC_RESP);
#endif /* STA */
#endif /* WL11K_ALL_MEAS */

	if (!(rrm_info = (wlc_rrm_info_t *)MALLOCZ(wlc->osh, sizeof(wlc_rrm_info_t)))) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}
	rrm_info->wlc = wlc;

	if ((rrm_info->rrm_state = (wlc_rrm_req_state_t *)MALLOCZ(wlc->osh,
		sizeof(wlc_rrm_req_state_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}

	if (!(rrm_info->rrm_timer = wl_init_timer(wlc->wl, wlc_rrm_timer, rrm_info, "rrm"))) {
		WL_ERROR(("rrm_timer init failed\n"));
		goto fail;
	}

#ifdef WL11K_ALL_MEAS
	if (!(rrm_info->rrm_noise_ipi_timer = wl_init_timer(wlc->wl, wlc_rrm_noise_ipi_timer,
		rrm_info, "rrm_noise_ipi"))) {
		WL_ERROR(("rrm_noise_ipi_timer init failed\n"));
		goto fail;
	}
#endif // endif

	/* register module */
	if (wlc_module_register(wlc->pub, rrm_iovars, "rrm", rrm_info, wlc_rrm_doiovar,
	                        wlc_rrm_watchdog, NULL, NULL)) {
		WL_ERROR(("wl%d: %s wlc_module_register() failed\n", wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* reserve cubby in the bsscfg container for per-bsscfg private data */
	if ((rrm_info->cfgh = wlc_bsscfg_cubby_reserve(wlc, sizeof(rrm_bsscfg_cubby_t),
		wlc_rrm_bsscfg_init, wlc_rrm_bsscfg_deinit, wlc_rrm_bsscfg_dump,
		(void *)rrm_info)) < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* reserve scb cubby */
	bzero(&cubby_params, sizeof(cubby_params));
	cubby_params.context = rrm_info;
	cubby_params.fn_init = wlc_rrm_scb_init;
	cubby_params.fn_deinit = wlc_rrm_scb_deinit;
	cubby_params.fn_secsz = wlc_rrm_scb_secsz;
	cubby_params.fn_dump = wlc_rrm_scb_dump;
	rrm_info->scb_handle = wlc_scb_cubby_reserve_ext(wlc, sizeof(struct rrm_scb_cubby),
		&cubby_params);

	if (rrm_info->scb_handle < 0) {
		WL_ERROR(("wl%d: %s wlc_scb_cubby_reserve() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* register IE mgmt callbacks */
	/* calc/build */
#ifdef WL11K_ALL_MEAS
#ifdef WL11K_AP
	/* bcn/prbrsp */
	if (wlc_iem_add_build_fn_mft(wlc->iemi, bcnfstbmp, DOT11_MNG_AP_CHREP_ID,
	      wlc_rrm_calc_ap_ch_rep_ie_len, wlc_rrm_write_ap_ch_rep_ie, rrm_info) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_build_fn failed, chrep in bcn\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	if (wlc_iem_add_build_fn_mft(wlc->iemi, bcnfstbmp, DOT11_MNG_MEASUREMENT_PILOT_TX_ID,
	      wlc_rrm_calc_mptx_ie_len, wlc_rrm_write_mptx_ie, rrm_info) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_build_fn failed, mptx in bcn\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	/* bcn/prbrsp/assocresp/reassocresp */
	if (wlc_iem_add_build_fn_mft(wlc->iemi, rrmcapfstbmp, DOT11_MNG_RRM_CAP_ID,
	      wlc_rrm_calc_cap_ie_len, wlc_rrm_write_cap_ie, rrm_info) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_build_fn failed, cap ie\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* assocreq/reassocreq */
	if (wlc_iem_add_parse_fn_mft(wlc->iemi, arqfstbmp, DOT11_MNG_RRM_CAP_ID,
	                         wlc_assoc_parse_rrm_ie, wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_parse_fn_mft failed, RRM CAP ie\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#endif /* WL11K_AP */

#ifdef STA
	/* assocreq/reassocreq */
	if (wlc_iem_add_build_fn_mft(wlc->iemi, assocrrmcapfstbmp, DOT11_MNG_RRM_CAP_ID,
	      wlc_rrm_calc_assoc_cap_ie_len, wlc_rrm_write_assoc_cap_ie, rrm_info) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_build_fn failed, cap ie\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	if (wlc_iem_add_parse_fn_mft(wlc->iemi, arsfstbmp, DOT11_MNG_RRM_CAP_ID,
	                         wlc_rrm_parse_rrmcap_ie, rrm_info) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_parse_fn failed, rrm cap ie\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#endif /* STA */
#endif /* WL11K_ALL_MEAS */

#ifdef WL11K_NBR_MEAS
#ifdef WL11K_AP
	if (wlc_scb_state_upd_register(wlc,
		wlc_rrm_ap_scb_state_upd, (void*)rrm_info) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_scb_state_upd_register failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#endif /* WL11K_AP */

#ifdef STA
#ifdef WLASSOC_NBR_REQ
	if (wlc_bss_assoc_state_register(wlc,
		wlc_rrm_sta_assoc_state_upd, (void*)rrm_info) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_scb_state_upd_register failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#endif /* WLASSOC_NBR_REQ */
#endif /* STA */
#endif /* WL11K_NBR_MEAS */

	/* driver will handle ftm range request/report by default */
	rrm_info->direct_ranging_mode = TRUE;

	return rrm_info;

fail:
	MODULE_DETACH(rrm_info, wlc_rrm_detach);
	return NULL;
}

void
BCMATTACHFN(wlc_rrm_detach)(wlc_rrm_info_t *rrm_info)
{
	wlc_info_t *wlc = rrm_info->wlc;

	wlc_module_unregister(rrm_info->wlc->pub, "rrm", rrm_info);

#ifdef WL11K_NBR_MEAS
#ifdef STA
#ifdef WLASSOC_NBR_REQ
	wlc_bss_assoc_state_unregister(wlc, wlc_rrm_sta_assoc_state_upd, (void*)rrm_info);
#endif /* WLASSOC_NBR_REQ */
#endif /* STA */

#ifdef WL11K_AP
	wlc_scb_state_upd_unregister(wlc, wlc_rrm_ap_scb_state_upd, (void*)rrm_info);
#endif /* WL11K_AP */
#endif /* WL11K_NBR_MEAS */

	/* free radio measurement ioctl reports */
	if (rrm_info->rrm_timer) {
		wl_free_timer(wlc->wl, rrm_info->rrm_timer);
	}

#ifdef WL11K_ALL_MEAS
	if (rrm_info->rrm_noise_ipi_timer) {
		wl_free_timer(wlc->wl, rrm_info->rrm_noise_ipi_timer);
	}
#endif /* WL11K_ALL_MEAS */

	if (rrm_info->rrm_state) {
		MFREE(wlc->osh, rrm_info->rrm_state, sizeof(wlc_rrm_req_state_t));
	}
	MFREE(rrm_info->wlc->osh, rrm_info, sizeof(wlc_rrm_info_t));
}

void
wlc_frameaction_rrm(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, struct scb *scb,
	uint action_id, uint8 *body, int body_len, int8 rssi, ratespec_t rspec)
{
	wlc_info_t *wlc = rrm_info->wlc;

	(void)wlc;

	WL_RRM(("%s: wl%d: action_id %d, body_len %d, rssi=%d\n", __FUNCTION__,
		wlc->pub->unit, action_id, body_len, rssi));

	if (action_id == DOT11_RM_ACTION_RM_REQ)
		wlc_rrm_recv_rmreq(rrm_info, cfg, scb, body, body_len);
#ifdef WL11K_ALL_MEAS
	else if (action_id == DOT11_RM_ACTION_LM_REQ)
		wlc_rrm_recv_lmreq(rrm_info, cfg, scb, body, body_len, rssi, rspec);
	else if (action_id == DOT11_RM_ACTION_LM_REP)
		wlc_rrm_recv_lmrep(rrm_info, cfg, scb, body, body_len);
#endif /* WL11K_ALL_MEAS */
	else if (action_id == DOT11_RM_ACTION_RM_REP)
		wlc_rrm_recv_rmrep(rrm_info, scb, body, body_len);
#ifdef WL11K_NBR_MEAS
#ifdef WL11K_AP
	else if (action_id == DOT11_RM_ACTION_NR_REQ) {
		wlc_rrm_send_event(rrm_info, scb, (char *)body, body_len,
			DOT11_RM_ACTION_NR_REQ, 0);
		wlc_rrm_recv_nrreq(rrm_info, cfg, scb, body, body_len);
	}
#endif /* WL11K_AP */
#ifdef STA
	else if (action_id == DOT11_RM_ACTION_NR_REP)
		wlc_rrm_recv_nrrep(rrm_info, cfg, scb, body, body_len);
#endif /* STA */
#endif /* WL11K_NBR_MEAS */

	return;
}

static int
wlc_rrm_ie_count(dot11_rm_ie_t *ie, uint8 mea_id, uint8 mode_flag, int len)
{
	int count = 0;

	WL_RRM(("%s: <Enter> mea_id %d, mode_flag 0x%x, len %d\n",
		__FUNCTION__, mea_id, mode_flag, len));

	/* make sure this is a valid RRM IE */
	if (len < DOT11_RM_IE_LEN ||
	    len < TLV_HDR_LEN + ie->len) {
		ie = NULL;
		return count;
	}

	/* walk the req IEs counting valid RRM Requests,
	 * skipping unknown IEs or ones that just have autonomous report flags
	 */
	while (ie) {
		WL_RRM(("%s: <Enter> ie->id %d, ie->mode 0x%x\n", __FUNCTION__, ie->id, ie->mode));
		WL_RRM_HEX("ie:", (uchar *)ie, ie->len + TLV_HDR_LEN);

		if (ie->id == mea_id &&
		    0 == (ie->mode & mode_flag)) {
			/* found a measurement request */
			count++;
		}
		ie = wlc_rrm_next_ie(ie, &len, mea_id);
	}
	return count;
}

static dot11_rm_ie_t *
wlc_rrm_next_ie(dot11_rm_ie_t *ie, int *len, uint8 mea_id)
{
	int buflen = *len;

	while (ie) {
		/* advance to the next IE */
		buflen -= TLV_HDR_LEN + ie->len;
		ie = (dot11_rm_ie_t *)((int8 *)ie + TLV_HDR_LEN + ie->len);

		/* make sure there is room for a valid RRM IE */
		if (buflen < DOT11_RM_IE_LEN ||
		    buflen < TLV_HDR_LEN + ie->len) {
			buflen = 0;
			ie = NULL;
			break;
		}

		if (ie->id == mea_id) {
			/* found valid measurement request/response */
			break;
		}
	}

	*len = buflen;
	return ie;
}

static void
wlc_rrm_parse_requests(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, dot11_rm_ie_t *ie,
	int len, wlc_rrm_req_t *req, int count)
{
#ifdef WL11K_BCN_MEAS
	dot11_rmreq_bcn_t *rmreq_bcn = NULL;
	char eabuf[ETHER_ADDR_STR_LEN];
#endif // endif
#ifdef WL11K_ALL_MEAS
	dot11_rmreq_chanload_t *rmreq_chanload = NULL;
	dot11_rmreq_noise_t *rmreq_noise = NULL;
	dot11_rmreq_frame_t *rmreq_frame = NULL;
	dot11_rmreq_stat_t *rmreq_stat = NULL;
	dot11_rmreq_tx_stream_t *rmreq_txstrm = NULL;
	dot11_rmreq_pause_time_t *rmreq_pause = NULL;
#endif // endif

	int idx = 0;
	uint32 parallel_flag = 0;
	rrm_bsscfg_cubby_t *rrm_cfg;

	rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, rrm_info->cur_cfg);
	ASSERT(rrm_cfg != NULL);
	BCM_REFERENCE(rrm_cfg); /* in case BCMDBG off */

	WL_RRM(("%s: <Enter>, rrm_cfg=%p, count=%d\n", __FUNCTION__, rrm_cfg, count));

	/* convert each RRM IE into a wlc_rrm_req */
	for (; ie != NULL && idx < count;
		ie = wlc_rrm_next_ie(ie, &len, DOT11_MNG_MEASURE_REQUEST_ID)) {
		/* skip IE if we do not recongnized the request,
		 * or it was !enable (not a autonomous report req)
		 */
		if (ie->id != DOT11_MNG_MEASURE_REQUEST_ID ||
		    ie->len < (DOT11_RM_IE_LEN - TLV_HDR_LEN) ||
		    0 != (ie->mode & DOT11_RMREQ_MODE_ENABLE)) {
			WL_RRM(("## REQ-UNKNOWN ## %s: id=%d type=%d len=%d imode=%d \n",
				__FUNCTION__, ie->id, ie->type, ie->len, ie->mode));
			continue;
		}

		/* in case we skip measurement requests, keep track of the
		 * parallel bit flag separately. Clear here at the beginning of
		 * every set of measurements and set below after the first
		 * measurement req we actually send down.
		 */
		if (!(ie->mode & DOT11_RMREQ_MODE_PARALLEL)) {
			parallel_flag = 0;
		}

		switch (ie->type) {
#ifdef WL11K_BCN_MEAS
		case DOT11_MEASURE_TYPE_BEACON:

			if (ie->len < (DOT11_RMREQ_BCN_LEN - TLV_HDR_LEN)) {
				/*
				 * It is better to continue to next IE as this IE could
				 * be a suspected corrupted packet.
				 */
				continue;
			}

			rmreq_bcn = (dot11_rmreq_bcn_t *)ie;
			bcm_ether_ntoa(&rmreq_bcn->bssid, eabuf);
			WL_INFORM(("%s: TYPE = BEACON, subtype = %d, bssid: %s\n",
				__FUNCTION__, rmreq_bcn->bcn_mode, eabuf));
			wlc_rrm_recv_bcnreq(rrm_info, cfg, rmreq_bcn, &req[idx]);
			req[idx].dur = ltoh16_ua(&rmreq_bcn->duration);
			req[idx].intval = ltoh16_ua(&rmreq_bcn->interval);

			WL_INFORM(("%s: req.dur:%d, req_bcn->duration:%d\n",
				__FUNCTION__,
				req[idx].dur, rmreq_bcn->duration));
			break;
#endif /* WL11K_BCN_MEAS */
#ifdef WL11K_ALL_MEAS
		case DOT11_MEASURE_TYPE_NOISE:
			if (ie->len < (DOT11_RMREQ_NOISE_LEN - TLV_HDR_LEN)) {
				/*
				 * It is better to continue to next IE as this IE could
				 * be a suspected corrupted packet.
				 */
				continue;
			}

			rmreq_noise = (dot11_rmreq_noise_t *)ie;
			req[idx].chanspec = CH20MHZ_CHSPEC(rmreq_noise->channel);
			wlc_rrm_recv_noisereq(rrm_info, cfg, rmreq_noise, &req[idx]);
			req[idx].dur = ltoh16_ua(&rmreq_noise->duration);
			req[idx].intval = ltoh16_ua(&rmreq_noise->interval);
			break;
		case DOT11_MEASURE_TYPE_CHLOAD:
			if (ie->len < (DOT11_RMREQ_CHANLOAD_LEN - TLV_HDR_LEN)) {
				/*
				 * It is better to continue to next IE as this IE could
				 * be a suspected corrupted packet.
				 */
				continue;
			}

			rmreq_chanload = (dot11_rmreq_chanload_t *)ie;

			WL_RRM(("\nRRM-REQ-CHLOAD param: idx=%d id=%d type=%d len=%d channel=%d\n",
				idx, ie->id, ie->type, ie->len, rmreq_chanload->channel));

			req[idx].chanspec = CH20MHZ_CHSPEC(rmreq_chanload->channel);
			req[idx].dur = ltoh16_ua(&rmreq_chanload->duration);
			req[idx].intval = ltoh16_ua(&rmreq_chanload->interval);
			wlc_rrm_recv_chloadreq(rrm_info, cfg, rmreq_chanload, &req[idx]);
			break;
		case DOT11_MEASURE_TYPE_FRAME:
			if (ie->len < (DOT11_RMREQ_FRAME_LEN - TLV_HDR_LEN)) {
				/*
				 * It is better to continue to next IE as this IE could
				 * be a suspected corrupted packet.
				 */
				continue;
			}

			rmreq_frame = (dot11_rmreq_frame_t *)ie;
			req[idx].chanspec = CH20MHZ_CHSPEC(rmreq_frame->channel);
			wlc_rrm_recv_framereq(rrm_info, cfg, rmreq_frame, &req[idx]);
			req[idx].dur = ltoh16_ua(&rmreq_frame->duration);
			req[idx].intval = ltoh16_ua(&rmreq_frame->interval);
			break;
		case DOT11_MEASURE_TYPE_STAT:
			if (ie->len < sizeof(*rmreq_stat)) {
				/*
				 * It is better to continue to next IE as this IE could
				 * be a suspected corrupted packet.
				 */
				continue;
			}

			rmreq_stat = (dot11_rmreq_stat_t *)ie;
			wlc_rrm_recv_statreq(rrm_info, cfg, rmreq_stat, &req[idx]);
			req[idx].dur = ltoh16_ua(&rmreq_stat->duration);
			req[idx].intval = ltoh16_ua(&rmreq_stat->interval);
			break;
		case DOT11_MEASURE_TYPE_TXSTREAM:
			if (ie->len < sizeof(*rmreq_txstrm)) {
				/*
				 * It is better to continue to next IE as this IE could
				 * be a suspected corrupted packet.
				 */
				continue;
			}

			rmreq_txstrm = (dot11_rmreq_tx_stream_t *)ie;
			wlc_rrm_recv_txstrmreq(rrm_info, cfg, rmreq_txstrm, &req[idx]);
			req[idx].dur = ltoh16_ua(&rmreq_txstrm->duration);
			req[idx].intval = ltoh16_ua(&rmreq_txstrm->interval);
			break;
		case DOT11_MEASURE_TYPE_LCI:
			if (ie->len < (DOT11_RMREQ_LCI_LEN - TLV_HDR_LEN)) {
				/*
				 * It is better to continue to next IE as this IE could
				 * be a suspected corrupted packet.
				 */
				continue;
			}

			wlc_rrm_recv_lcireq(rrm_info, cfg, (dot11_meas_req_loc_t *)ie,
				&req[idx]);
			break;
		case DOT11_MEASURE_TYPE_CIVICLOC:
			if (ie->len < (DOT11_RMREQ_CIVIC_LEN - TLV_HDR_LEN)) {
				/*
				 * It is better to continue to next IE as this IE could
				 * be a suspected corrupted packet.
				 */
				continue;
			}

			wlc_rrm_recv_civicreq(rrm_info, cfg, (dot11_meas_req_loc_t *)ie,
				&req[idx]);
			break;
		case DOT11_MEASURE_TYPE_LOC_ID:
			if (ie->len < (DOT11_RMREQ_LOCID_LEN - TLV_HDR_LEN)) {
				/*
				 * It is better to continue to next IE as this IE could
				 *  be a suspected corrupted packet.
				 */
				continue;
			}
			wlc_rrm_recv_locidreq(rrm_info, cfg, (dot11_meas_req_loc_t *)ie,
				&req[idx]);
			break;
		case DOT11_MEASURE_TYPE_PAUSE:
			rmreq_pause = (dot11_rmreq_pause_time_t *)ie;
			req[idx].dur = rmreq_pause->pause_time;
			req[idx].intval = 0;
			break;
#endif /* WL11K_ALL_MEAS */
#ifdef WL_FTM_11K
#if defined(WL_PROXDETECT) && defined(WL_FTM)
		case DOT11_MEASURE_TYPE_FTMRANGE: {
			dot11_rmreq_ftm_range_t *rmreq_ftmrange = NULL;

			if (ie->len < sizeof(*rmreq_ftmrange)) {
				/*
				 * It is better to continue to next IE as this IE could
				 * be a suspected corrupted packet.
				 */
				continue;
			}

			if (!PROXD_ENAB(rrm_info->wlc->pub))
				break;
			if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_FTM_RANGE))
				break; /* respond even if incapable */
			rmreq_ftmrange = (dot11_rmreq_ftm_range_t *) ie;
			(void) wlc_rrm_recv_frngreq(rrm_info, cfg, rmreq_ftmrange, &req[idx]);
			req[idx].dur = 1; /* needs to be non-zero */
			req[idx].intval = ltoh16_ua(&rmreq_ftmrange->max_init_delay);
			break;
		}
#endif /* WL_PROXDETECT && WL_FTM */
#endif /* WL_FTM_11K */
		default:
			WL_ERROR(("%s: Unknown TYPE (%d), ignore it\n", __FUNCTION__, ie->type));

			/* unknown measurement type, do not reply */
			continue;
		}

#ifdef WL11K_BCN_MEAS
		if (ie->type == DOT11_MEASURE_TYPE_BEACON) {
			rmreq_bcn = (dot11_rmreq_bcn_t *)ie;
			req[idx].type = rmreq_bcn->bcn_mode;
		} else
#endif /* WL11K_BCN_MEAS */
			req[idx].type = ie->type;

		req[idx].flags |= parallel_flag;
		req[idx].token = ltoh16(ie->token);
		/* special handling for beacon table */
		if (ie->type == DOT11_RMREQ_BCN_TABLE) {
			req[idx].chanspec = 0;
			req[idx].dur = 0;
		}

		WL_RRM(("%s: req idx=%d, type=%d, flags=0x%x, token=%d, chanspec=%d, dur=%d\n",
			__FUNCTION__, idx, req[idx].type, req[idx].flags,
			req[idx].token,	req[idx].chanspec, req[idx].dur));

		/* set the parallel bit now that we output a request item so
		 * that other reqs in the same parallel set will have the bit
		 * set. The flag will be cleared above when a new parallel set
		 * starts.
		 */
		parallel_flag = DOT11_RMREQ_MODE_PARALLEL;
		idx++;
	}
}

static int
wlc_rrm_state_init(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, uint16 rx_time,
	dot11_rmreq_t *rm_req, int req_len)
{
	wlc_rrm_req_state_t *rrm_state = rrm_info->rrm_state;
	uint32 start_time_h;
	uint32 start_time_l;
	int rrm_req_size;
	int rrm_req_count;
	wlc_rrm_req_t *rrm_req;
	dot11_rm_ie_t *req_ie;
	uint8 *req_pkt;
	int body_len;
	wlc_info_t *wlc = rrm_info->wlc;

	WL_RRM(("%s: <Enter>, rrm_state->req=%p\n", __FUNCTION__, rrm_state->req));

	if (rrm_state->req) {
		WL_ERROR(("wl%d: %s: Already servicing another RRM request\n", wlc->pub->unit,
			__FUNCTION__));
		return BCME_BUSY;
	}
	/* Along with rrm_state->req, wlc_rrm_free() also frees rrm_info->bcnreq so assert that is
	 * clear if rrm_state->req is NULL.
	 */
	ASSERT(rrm_info->bcnreq == NULL);
	body_len = req_len;
	req_pkt = (uchar *)rm_req;
	req_pkt += DOT11_RMREQ_LEN;
	body_len -= DOT11_RMREQ_LEN;

	req_ie = (dot11_rm_ie_t *)req_pkt;
	rrm_req_count = wlc_rrm_ie_count(req_ie, DOT11_MNG_MEASURE_REQUEST_ID,
		DOT11_RMREQ_MODE_ENABLE, body_len);

	if (rrm_req_count == 0) {
		WL_ERROR(("%s: request count == 0\n", __FUNCTION__));
		return BCME_ERROR;
	}
	if (rm_req->reps < WLC_RRM_MAX_REP_NUM)
		rrm_state->reps = rm_req->reps;

	rrm_req_size = rrm_req_count * sizeof(wlc_rrm_req_t);
	rrm_req = (wlc_rrm_req_t *)MALLOCZ(wlc->osh, rrm_req_size);
	if (rrm_req == NULL) {
		WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__, rrm_req_size,
			MALLOCED(wlc->osh)));
		return BCME_NORESOURCE;
	}

	rrm_state->report_class = WLC_RRM_CLASS_11K;
	rrm_state->token = ltoh16(rm_req->token);
	rrm_info->dialog_token = rrm_state->token;
	rrm_state->req_count = rrm_req_count;
	rrm_state->req = rrm_req;

	/* Fill out the request blocks */
	wlc_rrm_parse_requests(rrm_info, cfg, req_ie, body_len, rrm_req, rrm_req_count);

	/* Compute the start time of the measurements,
	 * Req frame has a delay in TBTT times, and an offset in TUs
	 * from the delay time.
	 */
	start_time_l = 0;
	start_time_h = 0;

	/* The measurement request frame has one start time for the set of
	 * measurements, but the driver state allows for a start time for each
	 * measurement. Set the measurement request start time into the
	 * first of the driver measurement requests, and leave the following
	 * start times as zero to indicate they happen as soon as possible.
	 */
	rrm_req[0].tsf_h = start_time_h;
	rrm_req[0].tsf_l = start_time_l;

	return BCME_OK;
}

static void
wlc_rrm_recv_rmreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, struct scb *scb, uint8 *body,
	int body_len)
{
	dot11_rmreq_t *rmreq;
	uint16 rxtime = 0;

	rmreq = (dot11_rmreq_t *)body;

	if (body_len <= DOT11_RMREQ_LEN) {
		WL_ERROR(("wl%d: %s invalid body len %d in rmreq\n", rrm_info->wlc->pub->unit,
		          __FUNCTION__, body_len));
		WLCNTINCR(rrm_info->wlc->pub->_cnt->rxbadproto);
		return;
	}

	WL_RRM(("%s: abort current RM request due to the comming of new req\n", __FUNCTION__));

	wlc_rrm_abort(rrm_info->wlc);

	rrm_info->cur_cfg = cfg;
	bcopy(&scb->ea, &rrm_info->da, ETHER_ADDR_LEN);
	/* Parse measurement requests */
	if (wlc_rrm_state_init(rrm_info, cfg, rxtime, rmreq, body_len)) {
		WL_ERROR(("wl%s: failed to initialize radio "
			  "measurement state, dropping request\n", __FUNCTION__));
		return;
	}
	/* Start rrm state machine */
	wlc_rrm_start(rrm_info->wlc);
}

static void
wlc_rrm_parse_response
(wlc_rrm_info_t *rrm_info, struct scb *scb, dot11_rm_ie_t *ie,
int len)
{
	bool incap = FALSE;
	bool refused = FALSE;

	for (; ie != NULL;
		ie = wlc_rrm_next_ie(ie, &len, DOT11_MNG_MEASURE_REPORT_ID)) {

		if (ie->id != DOT11_MNG_MEASURE_REPORT_ID ||
		    ie->len < (DOT11_RM_IE_LEN - TLV_HDR_LEN)) {
			WL_ERROR(("%s: ie->id: %d not DOT11_MNG_MEASURE_REPORT_ID\n",
				__FUNCTION__, ie->id));
			continue;
		}
		wlc_rrm_send_event(rrm_info, scb, (char *)ie, ie->len + TLV_HDR_LEN,
			DOT11_RM_ACTION_RM_REP, ie->type);
		if (ie->mode & DOT11_RMREP_MODE_INCAPABLE)
			incap = TRUE;

		if (ie->mode & DOT11_RMREP_MODE_REFUSED)
			refused = TRUE;

		if (incap || refused) {
			WL_ERROR(("%s: type: %s\n",
				__FUNCTION__, incap ? "INCAPABLE" : "REFUSED"));
			continue;
		}
		switch (ie->type) {
#ifdef WL11K_BCN_MEAS
			case DOT11_MEASURE_TYPE_BEACON:
				wlc_rrm_recv_bcnrep(rrm_info, scb, ie);
				break;
#endif /* WL11K_BCN_MEAS */
#ifdef WL11K_ALL_MEAS
			case DOT11_MEASURE_TYPE_STAT:
				wlc_rrm_recv_statrep(rrm_info, scb, ie);
				break;

#ifdef BCMDBG
			case DOT11_MEASURE_TYPE_NOISE:
				wlc_rrm_recv_noiserep(rrm_info, scb, ie);
				break;

			case DOT11_MEASURE_TYPE_CHLOAD:
				wlc_rrm_recv_chloadrep(rrm_info, scb, ie);
				break;

			case DOT11_MEASURE_TYPE_FRAME:
				wlc_rrm_recv_framerep(rrm_info, scb, ie);
				break;

			case DOT11_MEASURE_TYPE_TXSTREAM:
				wlc_rrm_recv_txstrmrep(rrm_info, scb, ie);
				break;

			case DOT11_MEASURE_TYPE_LCI:
				wlc_rrm_recv_lcirep(rrm_info, scb, ie);
				break;

			case DOT11_MEASURE_TYPE_CIVICLOC:
				wlc_rrm_recv_civicrep(rrm_info, scb, ie);
				break;

			case DOT11_MEASURE_TYPE_LOC_ID:
				wlc_rrm_recv_locidrep(rrm_info, scb, ie);
				break;

#endif /* BCMDBG */
#endif /* WL11K_ALL_MEAS */
#ifdef WL_FTM_11K
#if defined(WL_PROXDETECT) && defined(WL_FTM)
			case DOT11_MEASURE_TYPE_FTMRANGE: {
				rrm_bsscfg_cubby_t *rrm_cfg;
				dot11_rmrep_ftm_range_t *rmrep_ftmrange;
				if (!PROXD_ENAB(rrm_info->wlc->pub))
					break;
				rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, scb->bsscfg);
				if (rrm_cfg != NULL && !isset(rrm_cfg->rrm_cap,
					DOT11_RRM_CAP_FTM_RANGE))
					continue;
				rmrep_ftmrange = (dot11_rmrep_ftm_range_t *) ie;
				(void) wlc_rrm_recv_frngrep(rrm_info, scb, rmrep_ftmrange, len);
				break;
			}
#endif /* WL_PROXDETECT && WL_FTM */
#endif /* WL_FTM_11K */

			default:
				WL_ERROR(("%s: TYPE = Unknown TYPE, igore it\n", __FUNCTION__));
				/* unknown type */
				break;
		}
	}
}

static void
wlc_rrm_process_rep(wlc_rrm_info_t *rrm_info, struct scb *scb, dot11_rm_action_t *rrm_rep,
int rep_len)
{
	uchar *rep_pkt;
	dot11_rm_ie_t *rep_ie;
	uint rrm_rep_count, body_len = rep_len;

	WL_RRM(("%s: <Enter> rep_len %d\n", __FUNCTION__, rep_len));
	WL_RRM_HEX("-- actframe:", (uchar *)rrm_rep, rep_len);

	rep_pkt = (uchar *)rrm_rep;
	rep_pkt += DOT11_RM_ACTION_LEN;
	body_len -= DOT11_RM_ACTION_LEN;

	rep_ie = (dot11_rm_ie_t *)rep_pkt;
	rrm_rep_count = wlc_rrm_ie_count(rep_ie, DOT11_MNG_MEASURE_REPORT_ID,
		0xff, body_len);

	if (rrm_rep_count == 0) {
		WL_ERROR(("%s: response count == 0\n", __FUNCTION__));
	}

	wlc_rrm_parse_response(rrm_info, scb, rep_ie, body_len);
}

static void
wlc_rrm_recv_rmrep(wlc_rrm_info_t *rrm_info, struct scb *scb, uint8 *body, int body_len)
{
	dot11_rm_action_t *rrm_rep;

	rrm_rep = (dot11_rm_action_t *)body;

	if (body_len <= DOT11_RM_ACTION_LEN) {
		WL_ERROR(("wl%d: %s invalid body len %d in rmrep\n", rrm_info->wlc->pub->unit,
		          __FUNCTION__, body_len));
		WLCNTINCR(rrm_info->wlc->pub->_cnt->rxbadproto);
		return;
	}

	wlc_rrm_process_rep(rrm_info, scb, rrm_rep, body_len);

	return;
}

#ifdef WL11K_BCN_MEAS
/* Converts all current reg domain channels into chanspec list for scanning. */
static bool
wlc_rrm_request_current_regdomain(wlc_rrm_info_t *rrm_info, dot11_rmreq_bcn_t *rmreq_bcn,
	rrm_bcnreq_t *bcn_req, wlc_rrm_req_t *req)
{
	wlc_info_t *wlc = rrm_info->wlc;
	wl_uint32_list_t *chanlist;
	uint32 req_len;
	unsigned int i;

	req_len = OFFSETOF(wl_uint32_list_t, element) + (sizeof(chanlist->element[0]) * MAXCHANNEL);
	if ((chanlist = (wl_uint32_list_t *)MALLOCZ(wlc->osh, req_len)) == NULL) {
		WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__, (int)req_len,
			MALLOCED(wlc->osh)));
		req->flags |= DOT11_RMREP_MODE_REFUSED;
		return FALSE;
	}

	chanlist->count = MAXCHANNEL;
	if ((bcmwifi_rclass_get_chanlist(wlc_channel_country_abbrev(wlc->cmi),
			rmreq_bcn->reg, chanlist)) == 0) {
		bcn_req->channel_num = (uint16)chanlist->count;
		for (i = 0; i < chanlist->count; i++)
			bcn_req->chanspec_list[i] = CH20MHZ_CHSPEC(chanlist->element[i]);
	}
	MFREE(wlc->osh, chanlist, req_len);

	return TRUE;
}

/* Searches TLVs for AP Channel Reports and adds channels to the beacon request chanspec. */
static unsigned int
wlc_rrm_ap_chreps_to_chanspec(bcm_tlv_t *tlvs, int tlvs_len, rrm_bcnreq_t *bcn_req)
{
	unsigned int channel_count = 0;
	dot11_ap_chrep_t *ap_chrep = (dot11_ap_chrep_t *)tlvs;
	uint8 chanlist_bitmask[CEIL(MAXCHANNEL, NBBY)];
	uint8 chanlist_chan;
	uint channel_num;
	uint i;

	if (!bcm_valid_tlv(tlvs, tlvs_len)) {
		return channel_count;
	}
	memset(chanlist_bitmask, 0, CEIL(MAXCHANNEL, NBBY));
	for (ap_chrep = (dot11_ap_chrep_t *)tlvs; ap_chrep != NULL;
		ap_chrep = (dot11_ap_chrep_t *)bcm_next_tlv((bcm_tlv_t*)ap_chrep, &tlvs_len)) {
		/* verify the ID and minimum length requirement */
		if (ap_chrep->id != DOT11_MNG_AP_CHREP_ID || ap_chrep->len < 1) {
			continue;
		}

		/* Subtract 1 byte for regclass. */
		channel_num = ap_chrep->len - 1;
		WL_NONE(("AP Chanrep found "));
		/* For each parsed TLV, derive the channel list and store it in an array to
		 * make sure that duplicates are handled and bcn_req->chanspec_list buffer
		 * is not overrun.
		 */
		for (i = 0; i < channel_num; i++) {
			chanlist_chan = ap_chrep->chanlist[i];
			/* Do we need to drop parsing of all TLV's altogether when one of
			 * the channels is invalid? Dropping the current TLV for now.
			 */
			if (!CH_NUM_VALID_RANGE(chanlist_chan)) {
				break;
			}
			setbit(chanlist_bitmask, (chanlist_chan - 1));
			WL_NONE(("%d ", ap_chrep->chanlist[i]));
		}
		WL_NONE(("\n"));
	}
	/* create the chanspec list from the bitmap of reported channels */
	for (i = 0; i < MAXCHANNEL; i++) {
		if (isset(chanlist_bitmask, i)) {
			bcn_req->chanspec_list[channel_count++] =
				CH20MHZ_CHSPEC(i + 1);
		}
	}

	return channel_count;
}

static void
wlc_rrm_recv_bcnreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, dot11_rmreq_bcn_t *rmreq_bcn,
	wlc_rrm_req_t *req)
{
	wlc_info_t *wlc = rrm_info->wlc;
	int tlv_len;
	int scan_type = -1;
	bcm_tlv_t *tlv = NULL;
	uint8 *tlvs;
	rrm_bcnreq_t *bcn_req;
	rrm_bsscfg_cubby_t *rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, rrm_info->cur_cfg);

	WL_RRM(("<802.11K Rx Req> %s: <Enter> rmreq_bcn->mode=%d, rmreq_bcn->bcn_mode=%d\n",
		__FUNCTION__, rmreq_bcn->mode, rmreq_bcn->bcn_mode));

	if (rmreq_bcn->mode & (DOT11_RMREQ_MODE_PARALLEL | DOT11_RMREQ_MODE_ENABLE)) {
		WL_ERROR(("wl%d: %s: Unsupported beacon request mode 0x%x\n",
			wlc->pub->unit, __FUNCTION__, rmreq_bcn->mode));
		req->flags |= DOT11_RMREP_MODE_INCAPABLE;
		return;
	}

	switch (rmreq_bcn->bcn_mode) {
		case DOT11_RMREQ_BCN_PASSIVE:
			if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_BCN_PASSIVE))
				break;
			scan_type = DOT11_SCANTYPE_PASSIVE;
			break;
		case DOT11_RMREQ_BCN_ACTIVE:
			if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_BCN_ACTIVE))
				break;
			scan_type = DOT11_SCANTYPE_ACTIVE;
			break;
		case DOT11_RMREQ_BCN_TABLE:
			if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_BCN_TABLE))
				break;
			scan_type = DOT11_RMREQ_BCN_TABLE;
			break;
	}

	if (scan_type < 0) {
			req->flags |= DOT11_RMREP_MODE_INCAPABLE;
		WL_ERROR(("wl%d: %s: Unsupported beacon mode 0x%x in rmreq\n",
			wlc->pub->unit, __FUNCTION__, rmreq_bcn->bcn_mode));
			return;
	}
	if ((rrm_info->bcnreq = (rrm_bcnreq_t *)MALLOCZ(wlc->osh, sizeof(rrm_bcnreq_t))) == NULL) {
		WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__, (int)sizeof(rrm_bcnreq_t),
			MALLOCED(wlc->osh)));
		req->flags |= DOT11_RMREP_MODE_REFUSED;
		return;
	}

	bcn_req = rrm_info->bcnreq;
	bcn_req->token = rmreq_bcn->token;
	bcn_req->reg = rmreq_bcn->reg;
	bcn_req->channel = rmreq_bcn->channel;
	bcn_req->rep_detail = DOT11_RMREQ_BCN_REPDET_ALL;
	bcn_req->scan_type = scan_type;

	bcn_req->duration_tu = ltoh16_ua(&rmreq_bcn->duration);
	/* 802.11k measurement duration: 11.10.3
	 * Convert 802.11 Time Unit (TU) to ms for use in local scan timers.
	 */
	bcn_req->duration = (ltoh16_ua(&rmreq_bcn->duration) * DOT11_TU_TO_US) / 1000;
	if (bcn_req->duration > WLC_SCAN_AWAY_LIMIT) {
		if (rmreq_bcn->mode & DOT11_RMREQ_MODE_DURMAND) {
			WL_INFORM(("wl%d: %s: mandatory duration %d longer than"
				" WLC_SCAN_AWAY_LIMIT\n",
				wlc->pub->unit, __FUNCTION__, bcn_req->duration));
		} else
			bcn_req->duration = WLC_SCAN_AWAY_LIMIT;
	}

	if (WLC_RRM_REPORT_ALL_CHANNELS(rmreq_bcn)) {
		wlc_rrm_request_current_regdomain(rrm_info, rmreq_bcn, bcn_req, req);
	}
	else if (WLC_RRM_CHANNEL_REPORT_MODE(rmreq_bcn) == FALSE) {
		bcn_req->channel_num = 1;
		bcn_req->chanspec_list[0] = CH20MHZ_CHSPEC(rmreq_bcn->channel);
	}
	/* else: AP Channel Report mode is handled in subelement parsing section.
	 *
	 * We'll first search for the AP Channel Report IE in this frame, if none
	 * is found we'll fallback to the AP Channel Report in the beacon.
	 */

	bcopy(&rmreq_bcn->bssid, &bcn_req->bssid, sizeof(struct ether_addr));
	bzero(&bcn_req->ssid, sizeof(wlc_ssid_t));

	/* parse (mandatory subset) optional subelements */
	tlv_len = rmreq_bcn->len - (DOT11_RMREQ_BCN_LEN - TLV_HDR_LEN);

	if (tlv_len > 0) {
		tlvs = (uint8 *)&rmreq_bcn[1];

		tlv = bcm_parse_tlvs(tlvs, tlv_len, DOT11_RMREQ_BCN_SSID_ID);
		if (tlv) {
			bcn_req->ssid.SSID_len = MIN(DOT11_MAX_SSID_LEN, tlv->len);
			bcopy(tlv->data, bcn_req->ssid.SSID, bcn_req->ssid.SSID_len);
		}

		tlv = bcm_parse_tlvs(tlvs, tlv_len, DOT11_RMREQ_BCN_REPDET_ID);
		if (tlv && tlv->len > 0)
			bcn_req->rep_detail = tlv->data[0];
		if (bcn_req->rep_detail == DOT11_RMREQ_BCN_REPDET_REQUEST) {
			tlv = bcm_parse_tlvs(tlvs, tlv_len, DOT11_RMREQ_BCN_REQUEST_ID);
			if (tlv) {
				bcn_req->req_eid_num = MIN(tlv->len, WLC_RRM_REQUEST_ID_MAX);
				WL_INFORM(("%s: Got DOT11_RMREQ_BCN_REQUEST_ID in request: "
					  "bcn_req->req_eid_num %d, tlv->len %d, "
					  "WLC_RRM_REQUEST_ID_MAX %d\n", __FUNCTION__,
					  bcn_req->req_eid_num, tlv->len, WLC_RRM_REQUEST_ID_MAX));
				bcopy(tlv->data, &bcn_req->req_eid[0], bcn_req->req_eid_num);
			} else
				WL_INFORM(("%s: No DOT11_RMREQ_BCN_REQUEST_ID in request\n",
					__FUNCTION__));
		} else
			WL_INFORM(("%s: bcn_req->rep_detail %d != DOT11_RMREQ_BCN_REPDET_REQUEST\n",
				__FUNCTION__, bcn_req->rep_detail));

		/* Only look for AP Channel Report IEs if we're scanning and in the correct mode. */
		if (WLC_RRM_CHANNEL_REPORT_MODE(rmreq_bcn)) {
			/* Convert multiple AP chan report IEs to chanspec. */
			bcn_req->channel_num = wlc_rrm_ap_chreps_to_chanspec((bcm_tlv_t*)tlvs,
				tlv_len, bcn_req);
		}
	} else if (tlv_len < 0) {
		req->flags |= DOT11_RMREP_MODE_REFUSED;
		return;
	}

	if (WLC_RRM_CHANNEL_REPORT_MODE(rmreq_bcn) && (bcn_req->channel_num == 0)) {
		/* Fallback to channels in AP Channel Report from Beacon Frame if no AP Channel
		 * Reports were included in this Beacon Request frame.
		 */
		/* Get beacon frame from current bss. */
		wlc_bss_info_t *bi = cfg->current_bss;
		dot11_ap_chrep_t *ap_chrep;

		ap_chrep = wlc_rrm_get_ap_chrep(bi);
		if (ap_chrep) {
			bcn_req->channel_num = wlc_rrm_ap_chreps_to_chanspec((bcm_tlv_t*)ap_chrep,
				tlv_len, bcn_req);
		}
		/* Fallback to all channels in the current regulatory domain as there were no AP
		 * Channel Reports in the Beacon Request or Beacon.
		 */
		if (bcn_req->channel_num == 0) {
			wlc_rrm_request_current_regdomain(rrm_info, rmreq_bcn, bcn_req, req);
		}
	}
	if (rmreq_bcn->bcn_mode < DOT11_RMREQ_BCN_TABLE &&
		rrm_info->bcn_req_thrtl_win) {
		bool refuse = FALSE;

		/* Fallback to 1 bcn req / N seconds mode if no off_chan_time allowed */
		if (!rrm_info->bcn_req_off_chan_time_allowed && rrm_info->bcn_req_win_scan_cnt) {
			refuse = TRUE;
		}

		/* If this req causes to exceed allowed time, refuse */
		if (rrm_info->bcn_req_off_chan_time_allowed && (rrm_info->bcn_req_off_chan_time +
			(bcn_req->duration * bcn_req->channel_num)) >
			rrm_info->bcn_req_off_chan_time_allowed) {
			refuse = TRUE;
		}
		if (refuse == TRUE) {
			WL_ERROR(("wl%d: %s: reject bcn req allowed:%d spent:%d scan_cnt:%d\n",
				wlc->pub->unit, __FUNCTION__,
				rrm_info->bcn_req_off_chan_time_allowed,
				rrm_info->bcn_req_off_chan_time,
				rrm_info->bcn_req_win_scan_cnt));
			req->flags |= DOT11_RMREP_MODE_REFUSED;

			/* rrm_info->bcn_req will be freed at the end of state machine */
			return;
		}
		rrm_info->bcn_req_scan_start_timestamp = OSL_SYSUPTIME();
	}
}
#endif /* WL11K_BCN_MEAS */

static void
wlc_rrm_framereq_measure(wlc_rrm_info_t *rrm_info)
{
	if (rrm_info == NULL || rrm_info->cur_cfg == NULL) {
		WL_ERROR(("%s: rrm_info=%p\n", __FUNCTION__, rrm_info));
		return;
	}

	/* simplified solution, just calculate the average value when state changed */
	if (rrm_info->framereq) {
		rrm_info->framereq->avg_rcpi =
			(rrm_info->framereq->avg_rcpi + rrm_info->cur_cfg->link->rssi)/2;
	}
}

#ifdef WL11K_ALL_MEAS
/* frame request handler */
static void
wlc_rrm_recv_framereq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, dot11_rmreq_frame_t *rmreq_frame,
	wlc_rrm_req_t *req)
{
	wlc_info_t *wlc = rrm_info->wlc;
	rrm_framereq_t *frame_req;
	char eabuf[ETHER_ADDR_STR_LEN];
	rrm_bsscfg_cubby_t *rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, rrm_info->cur_cfg);

	bcm_ether_ntoa(&rmreq_frame->ta, eabuf);

	WL_RRM(("<802.11K Rx Req> %s: <Enter> mode=%d, req_type=%d, flags=0x%x, MAC=%s\n",
		__FUNCTION__, rmreq_frame->mode, rmreq_frame->req_type, req->flags, eabuf));

	if (rmreq_frame->mode & (DOT11_RMREQ_MODE_PARALLEL | DOT11_RMREQ_MODE_ENABLE) ||
		!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_FM)) {
		WL_ERROR(("wl%d: %s: Unsupported frame request mode 0x%x\n",
			wlc->pub->unit, __FUNCTION__, rmreq_frame->mode));
		req->flags |= DOT11_RMREP_MODE_INCAPABLE;
		return;
	}

	if ((rrm_info->framereq =
		(rrm_framereq_t *)MALLOCZ(wlc->osh, sizeof(rrm_framereq_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		req->flags |= DOT11_RMREP_MODE_REFUSED;
		return;
	}

	frame_req = rrm_info->framereq;
	frame_req->token = rmreq_frame->token;
	frame_req->reg = rmreq_frame->reg;
	frame_req->channel = rmreq_frame->channel;
	frame_req->duration = ltoh16_ua(&rmreq_frame->duration);
	frame_req->frame_req_type = rmreq_frame->req_type;
	bcopy(&rmreq_frame->ta, &frame_req->mac, sizeof(struct ether_addr));

	/* save the initial value */
	frame_req->avg_rcpi = cfg->link->rssi; /* initial value */
	frame_req->rxframe_begin = wlc->pub->_cnt->rxframe;

	wlc_read_tsf(wlc, &frame_req->start_tsf_l, &frame_req->start_tsf_h);

	WL_RRM(("%s: cfg->link->rssi=%d, wlc->pub->_cnt->rxframe=%d, MAC=%s\n",
		__FUNCTION__, cfg->link->rssi, wlc->pub->_cnt->rxframe, eabuf));
}

/* channel load handler */
static void
wlc_rrm_recv_chloadreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	dot11_rmreq_chanload_t *rmreq_chload, wlc_rrm_req_t *req)
{
	wlc_info_t *wlc = rrm_info->wlc;
	rrm_chloadreq_t *chload_req;
	rrm_bsscfg_cubby_t *rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, rrm_info->cur_cfg);

	if (rmreq_chload->mode & (DOT11_RMREQ_MODE_PARALLEL | DOT11_RMREQ_MODE_ENABLE) ||
		!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_CLM)) {
		WL_ERROR(("wl%d: %s: Unsupported chload request mode 0x%x\n",
			wlc->pub->unit, __FUNCTION__, rmreq_chload->mode));
		req->flags |= DOT11_RMREP_MODE_INCAPABLE;
		return;
	}

	if ((rrm_info->chloadreq =
		(rrm_chloadreq_t *)MALLOCZ(wlc->osh, sizeof(rrm_chloadreq_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		req->flags |= DOT11_RMREP_MODE_REFUSED;
		return;
	}

	chload_req = rrm_info->chloadreq;
	chload_req->token = rmreq_chload->token;
	chload_req->reg = rmreq_chload->reg;
	chload_req->channel = rmreq_chload->channel;

	chload_req->duration = ltoh16_ua(&rmreq_chload->duration);
	chload_req->interval = ltoh16_ua(&rmreq_chload->interval);

	wlc_read_tsf(wlc, &chload_req->start_tsf_l, &chload_req->start_tsf_h);

	WL_RRM(("<802.11K Rx Req> %s (%zd bytes):token=%d reg=%d channel=0x%x dur=%d interval=%d\n",
		__FUNCTION__, sizeof(rrm_chloadreq_t),
		chload_req->token, chload_req->reg, chload_req->channel,
		chload_req->duration, chload_req->interval));
}

static void
wlc_rrm_noise_ipi_begin(wlc_rrm_info_t *rrm_info)
{
	rrm_noisereq_t *noise_req;

	ASSERT(rrm_info);

	noise_req = rrm_info->noisereq;

	if (noise_req) {
		wlc_read_tsf(rrm_info->wlc, &noise_req->start_tsf_l, &noise_req->start_tsf_h);
		noise_req->noise_sample = 0;
		noise_req->noise_sample_num = 0;
		noise_req->ipi_sample_num = 0;
		bzero(noise_req->ipi_dens, sizeof(noise_req->ipi_dens));
		noise_req->ipi_active = TRUE;
		noise_req->ipi_end = FALSE;

		/* start the measurement */
		wlc_rrm_noise_ipi_timer(rrm_info);
	}

	return;
}

static bool
wlc_rrm_noise_ipi_sample(wlc_rrm_info_t *rrm_info, int8 noise)
{
	rrm_noisereq_t *noise_req;
	int i;
	wlc_info_t *wlc;

	if (rrm_info == NULL) {
		WL_ERROR(("%s: rrm_info is NULL\n", __FUNCTION__));
		return FALSE;
	}

	if ((wlc = rrm_info->wlc) == NULL) {
		WL_ERROR(("%s: wlc is NULL\n", __FUNCTION__));
		return FALSE;
	}

	if ((noise_req = rrm_info->noisereq) == NULL)  {
		WL_ERROR(("wl%d: %s: noise_req is NULL\n", wlc->pub->unit, __FUNCTION__));
		return FALSE;
	}

	/* update the histogram */
	WL_RRM(("wl%d: %s: adding noise sample %d to histogram\n",
		wlc->pub->unit, __FUNCTION__, noise));

	for (i = 0; i < WL_RRM_IPI_BINS_NUM; i++) {
		if (noise <= wlc_noise_ipi_bin_max[i]) {
			noise_req->ipi_dens[i] += 1;
			break;
		}
	}
	noise_req->ipi_sample_num++;

	if (noise_req->ipi_end) {
		WL_ERROR(("wl%d: %s: ipi_end = TRUE\n", wlc->pub->unit, __FUNCTION__));
		wl_add_timer(wlc->wl, rrm_info->rrm_noise_ipi_timer, 0, 0);
	}

	return FALSE;
}

static void
wlc_rrm_noise_sample_request(wlc_info_t *wlc)
{
	int8 noise_dbm;

	wlc_lq_noise_sample_request(wlc, WLC_NOISE_REQUEST_SCAN, CHSPEC_CHANNEL(wlc->chanspec));
	noise_dbm = phy_noise_avg(WLC_PI(wlc));
	WL_RRM(("wl%d: %s: noise_dbm=%d\n", wlc->pub->unit, __FUNCTION__, noise_dbm));

	while (wlc_rrm_noise_ipi_sample(wlc->rrm_info, noise_dbm)) {
		noise_dbm = phy_noise_avg(WLC_PI(wlc));
	}
}

static void
wlc_rrm_noise_ipi_timer(void *arg)
{
	wlc_rrm_info_t *rrm_info = (wlc_rrm_info_t*)arg;
	rrm_noisereq_t *noise_req;
	uint32 dummy_tsf_h, tsf_l;
	uint32 diff;
	uint32 dur_us;
	wlc_info_t *wlc;
	int i;

	ASSERT(rrm_info);
	wlc = rrm_info->wlc;
	ASSERT(wlc);
	ASSERT(wlc->pub);

	if (!wlc->pub->up) {
		WL_ERROR(("wl%d: %s: !pub->up\n", wlc->pub->unit, __FUNCTION__));
		return;
	}

	if ((noise_req = rrm_info->noisereq) == NULL)  {
		WL_ERROR(("wl%d: %s: noise_req is NULL\n", wlc->pub->unit, __FUNCTION__));
		return;
	}

	if (!noise_req->ipi_active) {
		WL_ERROR(("wl%d: %s: !ipi_active\n", wlc->pub->unit, __FUNCTION__));
		return;
	}

	if (noise_req->ipi_end) {
		/* done with the measurement */
		noise_req->ipi_active = FALSE;

		/* normalize histogram to 0-255 */
		WL_ERROR(("wl%d: %s: Noise IPI measurement done\n", wlc->pub->unit, __FUNCTION__));

		for (i = 0; i < WL_RRM_IPI_BINS_NUM; i++) {
			WL_RRM(("Noise IPI: i=%d,  ipi_dens=%d, ipi_sample_num=%d\n",
				i, noise_req->ipi_dens[i], noise_req->ipi_sample_num));
			if (noise_req->ipi_dens[i] > 0) {
				noise_req->ipi_dens[i] =
					(255 * noise_req->ipi_dens[i]) / noise_req->ipi_sample_num;
			}

			WL_ERROR(("wl%d: %s: IPI bin[%d] = %3d/255\n",
				wlc->pub->unit, __FUNCTION__, i, noise_req->ipi_dens[i]));
		}
		return;
	}

	/* continue measuring */
	wlc_rrm_noise_sample_request(wlc);

	/* calc duration remaining */
	wlc_read_tsf(wlc, &tsf_l, &dummy_tsf_h);
	diff = tsf_l - rrm_info->rrm_state->actual_start_l;
	dur_us = noise_req->duration * DOT11_TU_TO_US;

	if (dur_us > diff &&
	    dur_us - diff > WLC_RRM_NOISE_IPI_INTERVAL * 1000) {
		WL_INFORM(("wl%d: %s: wl_add_timer, dur_us=%d, diff=%d\n",
			wlc->pub->unit, __FUNCTION__, dur_us, diff));
		wl_add_timer(wlc->wl, rrm_info->rrm_noise_ipi_timer, WLC_RRM_NOISE_IPI_INTERVAL, 0);
	} else {
		WL_INFORM(("wl%d: %s(ipi_end = TRUE): wl_add_timer, dur_us=%d, diff=%d\n",
			wlc->pub->unit, __FUNCTION__, dur_us, diff));
		/* end the measurement as we collect the next sample */
		noise_req->ipi_end = TRUE;
		wl_add_timer(wlc->wl, rrm_info->rrm_noise_ipi_timer, 0, 0);
	}
}

static void
wlc_rrm_recv_noisereq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, dot11_rmreq_noise_t *rmreq_noise,
	wlc_rrm_req_t *req)
{
	wlc_info_t *wlc = rrm_info->wlc;
	rrm_noisereq_t *noise_req;
	rrm_bsscfg_cubby_t *rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, rrm_info->cur_cfg);

	WL_RRM(("<802.11K Rx Req> %s: <Enter> rmreq_noise->mode=%d, req->flags=0x%x\n",
		__FUNCTION__, rmreq_noise->mode, req->flags));

	if (rmreq_noise->mode & (DOT11_RMREQ_MODE_PARALLEL | DOT11_RMREQ_MODE_ENABLE) ||
		!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_NHM)) {
		WL_ERROR(("wl%d: %s: Unsupported frame request mode 0x%x\n",
			wlc->pub->unit, __FUNCTION__, rmreq_noise->mode));
		req->flags |= DOT11_RMREP_MODE_INCAPABLE;
		return;
	}

	if ((rrm_info->noisereq =
		(rrm_noisereq_t *)MALLOCZ(wlc->osh, sizeof(rrm_noisereq_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		req->flags |= DOT11_RMREP_MODE_REFUSED;
		return;
	}

	noise_req = rrm_info->noisereq;
	noise_req->token = rmreq_noise->token;
	noise_req->reg = rmreq_noise->reg;
	noise_req->channel = rmreq_noise->channel;

	noise_req->duration = ltoh16_ua(&rmreq_noise->duration);

	WL_RRM(("%s: noise_req->duration=%d, rmreq_noise->duration=%d\n",
		__FUNCTION__, noise_req->duration, rmreq_noise->duration));
}

void wlc_rrm_stat_qos_counter(wlc_info_t *wlc, struct scb *scb, int tid, uint cnt_offset)
{
	rrm_stat_group_qos_t *ptr_qos;
	uint32 *counter;
	scb_rrm_stats_t *rrm_scb_stats;
	wlc_rrm_info_t *rrm_info;

	if (!scb)
		return;

	rrm_info = wlc->rrm_info;
	if (rrm_info == NULL)
		return;

	rrm_scb_stats = (RRM_SCB_CUBBY(rrm_info, scb))->scb_rrm_stats;
	if (rrm_scb_stats && (tid >= 0) && (tid < NUMPRIO)) {
		ptr_qos = &(rrm_scb_stats->scb_qos_stats[tid]);
		counter = (uint32*)((uint8 *)ptr_qos + cnt_offset);
		(*counter)++;
	}
}

/* tx/rx counter on different channel bandwidth */
void wlc_rrm_stat_bw_counter(wlc_info_t *wlc, struct scb *scb, bool tx)
{
	scb_rrm_stats_t *rrm_scb_stats;
	wlc_rrm_info_t *rrm_info;

	ASSERT(wlc);

	if (!wlc || !scb)
		return;

	rrm_info = wlc->rrm_info;
	if (rrm_info == NULL || rrm_info->cur_cfg == NULL)
		return;

	rrm_scb_stats = (RRM_SCB_CUBBY(rrm_info, scb))->scb_rrm_stats;
	if (!rrm_scb_stats) {
		WL_RRM(("%s: rrm_scb_stats is NULL\n", __FUNCTION__));
		return;
	}

	if (CHSPEC_IS40(rrm_info->cur_cfg->current_bss->chanspec)) {
		if (tx)
			WLCNTSCBINCR(rrm_scb_stats->scb_bw_stats.txframe40mhz);
		else
			WLCNTSCBINCR(rrm_scb_stats->scb_bw_stats.rxframe40mhz);
	}
	else {
		if (tx)
			WLCNTSCBINCR(rrm_scb_stats->scb_bw_stats.txframe20mhz);
		else
			WLCNTSCBINCR(rrm_scb_stats->scb_bw_stats.rxframe20mhz);
	}
}

/* for dot11ChannelWidthSwitchCount */
void wlc_rrm_stat_chanwidthsw_counter(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	struct scb *scb;
	struct scb_iter scbiter;
	scb_rrm_stats_t *rrm_scb_stats;
	wlc_rrm_info_t *rrm_info;

	rrm_info = wlc->rrm_info;
	if (rrm_info == NULL)
		return;

	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {
		if (SCB_ASSOCIATED(scb)) {
			rrm_scb_stats = (RRM_SCB_CUBBY(rrm_info, scb))->scb_rrm_stats;
			if (rrm_scb_stats) {
				WLCNTSCBINCR(rrm_scb_stats->scb_bw_stats.chanwidthsw);
			}
		}
	}
}

static int
wlc_rrm_stat_get_group_data(wlc_rrm_info_t *rrm_info, int delta)
{
	struct scb *scb;
	enum wlc_bandunit bandunit;
	wlc_info_t *wlc;
	wlc_bsscfg_t *bsscfg;
	rrm_statreq_t *stat_req;
	scb_rrm_stats_t *rrm_scb_stats;

	ASSERT(rrm_info != NULL);
	if (rrm_info == NULL)
		return (BCME_BADARG);

	wlc = rrm_info->wlc;
	bsscfg = rrm_info->cur_cfg;
	stat_req = rrm_info->statreq;

	if ((wlc == NULL) || (bsscfg == NULL) || (stat_req == NULL)) {
		WL_RRM(("%s: wlc (%p) or bsscfg (%p) or stat_req (%p) is NULL\n",
			__FUNCTION__, wlc, bsscfg, stat_req));
		return (BCME_BADARG);
	}

	if (bsscfg->up)
		bandunit = CHSPEC_BANDUNIT(bsscfg->current_bss->chanspec);
	else {
		WL_RRM(("%s: bsscfg not up\n", __FUNCTION__));
		return (BCME_BADARG);
	}

	if ((scb = wlc_scbfindband(wlc, bsscfg, &stat_req->peer, bandunit)) == NULL) {
#ifdef BCMDBG
		char eabuf[ETHER_ADDR_STR_LEN];
		WL_RRM(("%s: sta (%s) not found\n", __FUNCTION__,
			bcm_ether_ntoa(&stat_req->peer, eabuf)));
#endif /* BCMDBG */
		return (BCME_BADADDR);
	}

	rrm_scb_stats = (RRM_SCB_CUBBY(rrm_info, scb))->scb_rrm_stats;
	if (!rrm_scb_stats) {
		WL_RRM(("%s: rrm_scb_stats is NULL\n", __FUNCTION__));
		return (BCME_BADARG);
	}

	switch (stat_req->group_id) {
		case DOT11_RRM_STATS_GRP_ID_0:
#ifdef WLCNTSCB
			/* scb->scb_stats ==> stat_req->sta_data.group0 */
			WLCCNTSTATGROUPDELTA(wlc->pub->_cnt->txfrag, 0, txfrag, delta);
			WLCCNTSTATGROUPDELTA(wlc->pub->_cnt->txmulti, 0, txmulti, delta);
			WLCCNTSTATGROUPDELTA(wlc->pub->_cnt->txfail, 0, txfail, delta);
			WLCCNTSTATGROUPDELTA(wlc->pub->_cnt->rxfrag, 0, rxframe, delta);
			WLCCNTSTATGROUPDELTA(wlc->pub->_cnt->rxmulti, 0, rxmulti, delta);
			WLCCNTSTATGROUPDELTA(wlc->pub->_cnt->rxcrc, 0, rxbadfcs, delta);
			WLCCNTSTATGROUPDELTA(wlc->pub->_cnt->txfrmsnt, 0, txframe, delta);

			WL_RRM(("%s: delta=%d, group_id=%d, tx_pkts=%d, rx_ucast_pkts=%d\n",
				__FUNCTION__, delta, stat_req->group_id, scb->scb_stats.tx_pkts,
				scb->scb_stats.rx_ucast_pkts));
#endif /* WLCNTSCB */
			break;
		case DOT11_RRM_STATS_GRP_ID_1:
			WLCCNTSTATGROUPDELTA(wlc->pub->_cnt->txretry, 1, txretry, delta);
			WLCCNTSTATGROUPDELTA(wlc->pub->_cnt->txretrie, 1, txretries, delta);
			WLCCNTSTATGROUPDELTA(wlc->pub->_cnt->rxdup, 1, rxdup, delta);
			WLCCNTSTATGROUPDELTA(wlc->pub->_cnt->txrts, 1, txrts, delta);
			WLCCNTSTATGROUPDELTA(wlc->pub->_cnt->txnocts, 1, rtsfail, delta);
			WLCCNTSTATGROUPDELTA(wlc->pub->_cnt->txnoack, 1, ackfail, delta);

			WL_RRM(("%s: delta=%d, group_id=%d, txretry=%d, txrts=%d\n",
				__FUNCTION__, delta, stat_req->group_id,
				wlc->pub->_cnt->txretry, wlc->pub->_cnt->txrts));

			break;
		case DOT11_RRM_STATS_GRP_ID_2:
		case DOT11_RRM_STATS_GRP_ID_3:
		case DOT11_RRM_STATS_GRP_ID_4:
		case DOT11_RRM_STATS_GRP_ID_5:
		case DOT11_RRM_STATS_GRP_ID_6:
		case DOT11_RRM_STATS_GRP_ID_7:
		case DOT11_RRM_STATS_GRP_ID_8:
		case DOT11_RRM_STATS_GRP_ID_9:
			/* qos 0-7 use same data struct */
			WLCCNTSTATGROUPQOS(stat_req->group_id-2, txfrag, txfrag, delta);
			WLCCNTSTATGROUPQOS(stat_req->group_id-2, txfail, txfail, delta);
			WLCCNTSTATGROUPQOS(stat_req->group_id-2, txretry, txretry, delta);
			WLCCNTSTATGROUPQOS(stat_req->group_id-2, txretries, txretries, delta);
			WLCCNTSTATGROUPQOS(stat_req->group_id-2, rxdup, rxdup, delta);
			WLCCNTSTATGROUPQOS(stat_req->group_id-2, txrts, txrts, delta);
			WLCCNTSTATGROUPQOS(stat_req->group_id-2, rtsfail, rtsfail, delta);
			WLCCNTSTATGROUPQOS(stat_req->group_id-2, ackfail, ackfail, delta);
			WLCCNTSTATGROUPQOS(stat_req->group_id-2, rxfrag, rxfrag, delta);
			WLCCNTSTATGROUPQOS(stat_req->group_id-2, txframe, txframe, delta);
			WLCCNTSTATGROUPQOS(stat_req->group_id-2, txdrop, txdrop, delta);
			WLCCNTSTATGROUPQOS(stat_req->group_id-2, rxmpdu, rxmpdu, delta);
			WLCCNTSTATGROUPQOS(stat_req->group_id-2, rxretries, rxretries, delta);

			WL_RRM(("%s: delta=%d, group_id=%d, txframe=%d, rxmpdu=%d\n",
				__FUNCTION__, delta, stat_req->group_id,
				rrm_scb_stats->scb_qos_stats[stat_req->group_id-2].txframe,
				rrm_scb_stats->scb_qos_stats[stat_req->group_id-2].rxmpdu));

			break;
		case DOT11_RRM_STATS_GRP_ID_10:
			{
			/* avgdelay: not implemented */
			stat_req->sta_data.group10.avgdelaybe = 0;
			stat_req->sta_data.group10.avgdelaybg = 0;
			stat_req->sta_data.group10.avgdelayvi = 0;
			stat_req->sta_data.group10.avgdelayvo = 0;

			if (delta) {
				/* count stacount, chanutil at end of meas duration */
				stat_req->sta_data.group10.stacount =
					wlc_bss_assocscb_getcnt(wlc, bsscfg);
#ifdef WLCHANIM
				{
				chanim_stats_t cur_stats;
				if (wlc_lq_chanim_stats_get(wlc->chanim_info,
					wlc->chanspec, &cur_stats)) {
					stat_req->sta_data.group10.chanutil =
						100 - cur_stats.chan_idle;
				}
				else
					stat_req->sta_data.group10.chanutil = 0;
				}
#else
				stat_req->sta_data.group10.chanutil = 0;
#endif /* WLCHANIM */
			}

			/* scb->delay_stats[AC_BE] -> stat_req->sta_data.group10.avgdelaybe */
			WL_RRM(("%s: delta=%d, group_id=%d: be=%d, bg=%d, "
				"vi=%d, vo=%d, stacount=%d, chanutil=%d\n",
				__FUNCTION__, delta, stat_req->group_id,
				stat_req->sta_data.group10.avgdelaybe,
				stat_req->sta_data.group10.avgdelaybg,
				stat_req->sta_data.group10.avgdelayvi,
				stat_req->sta_data.group10.avgdelayvo,
				stat_req->sta_data.group10.stacount,
				stat_req->sta_data.group10.chanutil));
			}
			break;
		case DOT11_RRM_STATS_GRP_ID_11:
#ifdef WLAMSDU
			{
			rrm_stat_group_11_t tmp11;
			rrm_stat_group_11_t *grp11 = &stat_req->sta_data.group11;
			wlc_amsdu_get_stats(wlc, &tmp11);

			WLCCNTSTATGROUPDELTA(tmp11.txamsdu, 11, txamsdu, delta);
			WLCCNTSTATGROUPDELTA(tmp11.amsdufail, 11, amsdufail, delta);
			WLCCNTSTATGROUPDELTA(tmp11.amsduretry, 11, amsduretry, delta);
			WLCCNTSTATGROUPDELTA(tmp11.amsduretries, 11, amsduretries, delta);
			WLCCNTSTATGROUPDELTA(tmp11.txamsdubyte_h, 11, txamsdubyte_h, delta);

			/**
			 * Subtraction handles overflow. So, if txamsdubyte_l
			 * overflowed and less than the value
			 * at start of measurement, decrement txamsdubyte_h
			 */
			if (delta && grp11->txamsdubyte_h &&
				tmp11.txamsdubyte_l < grp11->txamsdubyte_l)
				grp11->txamsdubyte_h--;
			WLCCNTSTATGROUPDELTA(tmp11.txamsdubyte_l, 11, txamsdubyte_l, delta);
			WLCCNTSTATGROUPDELTA(tmp11.amsduackfail, 11, amsduackfail, delta);
			WLCCNTSTATGROUPDELTA(tmp11.rxamsdu, 11, rxamsdu, delta);
			WLCCNTSTATGROUPDELTA(tmp11.rxamsdubyte_h, 11, rxamsdubyte_h, delta);
			/* Subtraction handles overflow */
			if (delta && grp11->rxamsdubyte_h &&
				tmp11.rxamsdubyte_l < grp11->rxamsdubyte_l)
				grp11->rxamsdubyte_h--;
			WLCCNTSTATGROUPDELTA(tmp11.rxamsdubyte_l, 11, rxamsdubyte_l, delta);

			WL_RRM(("STATS-GRP11 %s: delta=%d, group_id=%d: "
				"txamsdu=%d amsdufail=%d amsduackfail=%d rxamsdu=%d\n",
				__FUNCTION__, delta, stat_req->group_id,
				stat_req->sta_data.group11.txamsdu,
				stat_req->sta_data.group11.amsdufail,
				stat_req->sta_data.group11.amsduackfail,
				stat_req->sta_data.group11.rxamsdu));
			}
#endif /* WLAMSDU */
			break;
		case DOT11_RRM_STATS_GRP_ID_12:
#ifdef WLAMPDU
			{
			rrm_stat_group_12_t tmp12;
			rrm_stat_group_12_t *grp12 = &stat_req->sta_data.group12;
			wlc_ampdu_get_stats(wlc, &tmp12);

			WLCCNTSTATGROUPDELTA(tmp12.txampdu, 12, txampdu, delta);
			WLCCNTSTATGROUPDELTA(tmp12.txmpdu, 12, txmpdu, delta);
			WLCCNTSTATGROUPDELTA(tmp12.txampdubyte_h, 12,
				txampdubyte_h, delta);
			/* Subtraction handles overflow */
			if (delta && grp12->txampdubyte_h &&
				tmp12.txampdubyte_l < grp12->txampdubyte_l)
				grp12->txampdubyte_h--;
			WLCCNTSTATGROUPDELTA(tmp12.txampdubyte_l, 12,
				txampdubyte_l, delta);
			WLCCNTSTATGROUPDELTA(tmp12.rxampdu, 12, rxampdu, delta);
			WLCCNTSTATGROUPDELTA(tmp12.rxmpdu, 12, rxmpdu, delta);
			WLCCNTSTATGROUPDELTA(tmp12.rxampdubyte_h, 12,
				rxampdubyte_h, delta);
			/* Subtraction handles overflow */
			if (delta && grp12->rxampdubyte_h &&
				tmp12.rxampdubyte_l < grp12->rxampdubyte_l)
				grp12->rxampdubyte_h--;
			WLCCNTSTATGROUPDELTA(tmp12.rxampdubyte_l, 12,
				rxampdubyte_l, delta);
			WLCCNTSTATGROUPDELTA(tmp12.ampducrcfail, 12,
				ampducrcfail, delta);

			WL_RRM(("STATS-GRP12 %s: delta=%d, group_id=%d: "
				"txampdu=%d txmpdu=%d rxampdu=%d rxmpdu=%d ampducrcfail=%d\n",
				__FUNCTION__, delta, stat_req->group_id,
				stat_req->sta_data.group12.txampdu,
				stat_req->sta_data.group12.txmpdu,
				stat_req->sta_data.group12.rxampdu,
				stat_req->sta_data.group12.rxmpdu,
				stat_req->sta_data.group12.ampducrcfail));
			}
#endif /* WLAMPDU */
			break;
		case DOT11_RRM_STATS_GRP_ID_13:
			/* use wlc->pub->_cnt->chanwidthsw */
			WLCCNTSTATGROUPDELTA(rrm_scb_stats->scb_bw_stats.chanwidthsw, 13,
				chanwidthsw, delta);
			/* use rrm_scb_stats->scb_bw_stats.txframe20mhz */
			WLCCNTSTATGROUPDELTA(rrm_scb_stats->scb_bw_stats.txframe20mhz, 13,
				txframe20mhz, delta);
			WLCCNTSTATGROUPDELTA(rrm_scb_stats->scb_bw_stats.txframe40mhz, 13,
				txframe40mhz, delta);
			WLCCNTSTATGROUPDELTA(rrm_scb_stats->scb_bw_stats.rxframe20mhz, 13,
				rxframe20mhz, delta);
			WLCCNTSTATGROUPDELTA(rrm_scb_stats->scb_bw_stats.rxframe40mhz, 13,
				rxframe40mhz, delta);

			WL_RRM(("%s: delta=%d, group_id=%d: chanwidthsw=%d, txframe20mhz=%d, "
				"txframe40mhz=%d, rxframe20mhz=%d, rxframe40mhz=%d\n",
				__FUNCTION__, delta, stat_req->group_id,
				rrm_scb_stats->scb_bw_stats.chanwidthsw,
				rrm_scb_stats->scb_bw_stats.txframe20mhz,
				rrm_scb_stats->scb_bw_stats.txframe40mhz,
				rrm_scb_stats->scb_bw_stats.rxframe20mhz,
				rrm_scb_stats->scb_bw_stats.rxframe40mhz));

			break;
		case DOT11_RRM_STATS_GRP_ID_14:
			/* TODO */
			break;
		case DOT11_RRM_STATS_GRP_ID_15:
			/* TODO */
			break;
		case DOT11_RRM_STATS_GRP_ID_16:
			WLCCNTSTATGROUPDELTA(wlc->pub->_cnt->ccmpreplay, 16,
				rsnarobustmgmtccmpreplay, delta);
			WLCCNTSTATGROUPDELTA(wlc->pub->_cnt->tkipicverr, 16,
				rsnatkipicverr, delta);
			WLCCNTSTATGROUPDELTA(wlc->pub->_cnt->tkipreplay, 16,
				rsnatkipicvreplay, delta);
			WLCCNTSTATGROUPDELTA(wlc->pub->_cnt->ccmpundec, 16,
				rsnaccmpdecrypterr, delta);
			WLCCNTSTATGROUPDELTA(wlc->pub->_cnt->ccmpreplay, 16,
				rsnaccmpreplay, delta);

			WL_RRM(("%s: delta=%d, group_id=%d: mgmtccmpreplay=%d, tkipicverr=%d, "
				"tkipicvreplay=%d, cmpdecrypterr=%d, ccmpreplay=%d\n",
				__FUNCTION__, delta, stat_req->group_id,
				stat_req->sta_data.group16.rsnarobustmgmtccmpreplay,
				stat_req->sta_data.group16.rsnatkipicverr,
				stat_req->sta_data.group16.rsnatkipicvreplay,
				stat_req->sta_data.group16.rsnaccmpdecrypterr,
				stat_req->sta_data.group16.rsnaccmpreplay));
			break;
		default:
			WL_ERROR(("%s: the group id %d is not suppoted\n",
				__FUNCTION__,  stat_req->group_id));
			break;
	};

	return 0;
}

static void
wlc_rrm_recv_statreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, dot11_rmreq_stat_t *rmreq_stat,
	wlc_rrm_req_t *req)
{
	wlc_info_t *wlc = rrm_info->wlc;
	rrm_statreq_t *stat_req;
	char eabuf[ETHER_ADDR_STR_LEN];
	rrm_bsscfg_cubby_t *rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, rrm_info->cur_cfg);

	bcm_ether_ntoa(&rmreq_stat->peer, eabuf);

	WL_RRM(("<802.11K Rx Req> %s: <Enter> mode=%d, group_id=%d, flags=0x%x, peer MAC=%s\n",
		__FUNCTION__, rmreq_stat->mode, rmreq_stat->group_id, req->flags, eabuf));

	if (rmreq_stat->mode & (DOT11_RMREQ_MODE_PARALLEL | DOT11_RMREQ_MODE_ENABLE) ||
		!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_SM)) {
		WL_ERROR(("wl%d: %s: Unsupported stat request mode 0x%x\n",
			wlc->pub->unit, __FUNCTION__, rmreq_stat->mode));
		req->flags |= DOT11_RMREP_MODE_INCAPABLE;
		return;
	}

	if ((rrm_info->statreq = (rrm_statreq_t *)MALLOCZ(wlc->osh, sizeof(rrm_statreq_t)))
		== NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		req->flags |= DOT11_RMREP_MODE_REFUSED;
		return;
	}

	stat_req = rrm_info->statreq;
	stat_req->token = rmreq_stat->token;

	/* save the initial value */
	bcopy(&rmreq_stat->peer, &stat_req->peer, sizeof(struct ether_addr));
	stat_req->duration = ltoh16_ua(&rmreq_stat->duration);
	stat_req->group_id = rmreq_stat->group_id;

	/* clear counter */
	WL_RRM(("%s: init stats data: %zd\n", __FUNCTION__, sizeof(stat_req->sta_data)));
	bzero((char *)&stat_req->sta_data, sizeof(stat_req->sta_data));
}

void wlc_rrm_tscm_upd(wlc_info_t *wlc, struct scb *scb, int tid, uint cnt_offset, uint cnt_val)
{
	rrm_tscm_t *tscm;
	uint32 *counter;
	scb_tscm_t *scb_tscm;
	wlc_rrm_info_t *rrm_info;

	if (!scb)
		return;

	rrm_info = wlc->rrm_info;
	if (rrm_info == NULL)
		return;

	scb_tscm = (RRM_SCB_CUBBY(rrm_info, scb))->scb_rrm_tscm;
	if (scb_tscm && (tid >= 0) && (tid < NUMPRIO)) {
		tscm = &(scb_tscm->scb_qos_tscm[tid]);
		counter = (uint32*)((uint8 *)tscm + cnt_offset);
		(*counter) += cnt_val;
	}
}

void wlc_rrm_delay_upd(wlc_info_t *wlc, struct scb *scb, uint8 tid, uint32 delay)
{
	uint32 bin0_range_us;
	scb_tscm_t *scb_tscm;
	wlc_rrm_info_t *rrm_info;

	if (!scb)
		return;

	rrm_info = wlc->rrm_info;
	if (rrm_info == NULL)
		return;

	scb_tscm = (RRM_SCB_CUBBY(rrm_info, scb))->scb_rrm_tscm;
	if (!scb_tscm)
		return;

	bin0_range_us = scb_tscm->scb_qos_tscm[tid].bin0_range_us;

	wlc_rrm_tscm_upd(wlc, scb, tid, OFFSETOF(rrm_tscm_t, tx_delay_sum), delay);
	wlc_rrm_tscm_upd(wlc, scb, tid, OFFSETOF(rrm_tscm_t, tx_delay_cnt), 1);
	if (delay < bin0_range_us)
		wlc_rrm_tscm_upd(wlc, scb, tid, OFFSETOF(rrm_tscm_t, bin0), 1);
	else if (delay < bin0_range_us * 2)
		wlc_rrm_tscm_upd(wlc, scb, tid, OFFSETOF(rrm_tscm_t, bin1), 1);
	else if (delay < bin0_range_us * 4)
		wlc_rrm_tscm_upd(wlc, scb, tid, OFFSETOF(rrm_tscm_t, bin2), 1);
	else if (delay < bin0_range_us * 8)
		wlc_rrm_tscm_upd(wlc, scb, tid, OFFSETOF(rrm_tscm_t, bin3), 1);
	else if (delay < bin0_range_us * 16)
		wlc_rrm_tscm_upd(wlc, scb, tid, OFFSETOF(rrm_tscm_t, bin4), 1);
	else
		wlc_rrm_tscm_upd(wlc, scb, tid, OFFSETOF(rrm_tscm_t, bin5), 1);

}
static int
wlc_rrm_txstrm_get_data(wlc_rrm_info_t *rrm_info, int delta)
{
	struct scb *scb;
	enum wlc_bandunit bandunit;
	wlc_info_t *wlc;
	wlc_bsscfg_t *bsscfg;
	rrm_txstrmreq_t *tscm_req;
	scb_tscm_t *scb_tscm;
	rrm_tscm_t *scb_qos_tscm;

	ASSERT(rrm_info != NULL);
	if (rrm_info == NULL)
		return (BCME_BADARG);

	wlc = rrm_info->wlc;
	bsscfg = rrm_info->cur_cfg;
	tscm_req = rrm_info->txstrmreq;

	if ((wlc == NULL) || (bsscfg == NULL) || (tscm_req == NULL)) {
		WL_RRM(("%s: wlc (%p) or bsscfg (%p) or tscm_req (%p) is NULL\n",
			__FUNCTION__, wlc, bsscfg, tscm_req));
		return (BCME_BADARG);
	}

	if (bsscfg->up)
		bandunit = CHSPEC_BANDUNIT(bsscfg->current_bss->chanspec);
	else {
		WL_RRM(("%s: bsscfg not up\n", __FUNCTION__));
		return (BCME_BADARG);
	}

	if ((scb = wlc_scbfindband(wlc, bsscfg, &tscm_req->peer, bandunit)) == NULL) {
#ifdef BCMDBG
		char eabuf[ETHER_ADDR_STR_LEN];
		WL_RRM(("%s: sta (%s) not found\n", __FUNCTION__,
			bcm_ether_ntoa(&tscm_req->peer, eabuf)));
#endif /* BCMDBG */
		return (BCME_BADADDR);
	}

	scb_tscm = (RRM_SCB_CUBBY(rrm_info, scb))->scb_rrm_tscm;
	if (!scb_tscm) {
		WL_RRM(("%s: scb_tscm is NULL\n", __FUNCTION__));
		return (BCME_BADARG);
	}
	scb_qos_tscm = &scb_tscm->scb_qos_tscm[tscm_req->tid];

	if (delta) {
		tscm_req->txmsdu_cnt = scb_qos_tscm->msdu_tx;
		tscm_req->msdufailed_cnt = scb_qos_tscm->msdu_fail;
		/* discarded count = lifetime expired + tx failed */
		tscm_req->msdu_discarded_cnt = scb_qos_tscm->msdu_exp + tscm_req->msdufailed_cnt;
		tscm_req->msduretry_cnt = scb_qos_tscm->msdu_retries;
		tscm_req->cfpolls_lost_cnt = scb_qos_tscm->cfpolls_lost;
		tscm_req->avrqueue_delay = scb_qos_tscm->queue_delay;
		if (scb_qos_tscm->tx_delay_cnt)
			tscm_req->avrtx_delay =
				scb_qos_tscm->tx_delay_sum / scb_qos_tscm->tx_delay_cnt;
		else
			tscm_req->avrtx_delay = 0;
		tscm_req->bin0 = scb_qos_tscm->bin0;
		tscm_req->bin1 = scb_qos_tscm->bin1;
		tscm_req->bin2 = scb_qos_tscm->bin2;
		tscm_req->bin3 = scb_qos_tscm->bin3;
		tscm_req->bin4 = scb_qos_tscm->bin4;
		tscm_req->bin5 = scb_qos_tscm->bin5;
	} else {
		memset(scb_qos_tscm, 0, sizeof(*scb_qos_tscm));
		scb_qos_tscm->bin0_range_us = tscm_req->bin0_range * 10 * DOT11_TU_TO_US;
	}
	WL_RRM(("%s: delta=%d, txmsdu=%d, discarded= %d, failed=%d, retry=%d, "
		"cfpoll lost=%d, queuedelay=%d, txdelay=%d \n"
		"bin: [0] = %d, [1] = %d, [2] = %d, [3] =%d, [4] = %d, [5] = %d\n",
		__FUNCTION__, delta, tscm_req->txmsdu_cnt, tscm_req->msdu_discarded_cnt,
		tscm_req->msdufailed_cnt, tscm_req->msduretry_cnt,
		tscm_req->cfpolls_lost_cnt, tscm_req->avrqueue_delay, tscm_req->avrtx_delay,
		tscm_req->bin0, tscm_req->bin1, tscm_req->bin2, tscm_req->bin3,
		tscm_req->bin4, tscm_req->bin5));

	return 0;
}

static void
wlc_rrm_recv_txstrmreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	dot11_rmreq_tx_stream_t *rmreq_txstrm, wlc_rrm_req_t *req)
{
	wlc_info_t *wlc = rrm_info->wlc;
	rrm_txstrmreq_t *tscm_req;
	char eabuf[ETHER_ADDR_STR_LEN];
	rrm_bsscfg_cubby_t *rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, rrm_info->cur_cfg);

	bcm_ether_ntoa(&rmreq_txstrm->peer, eabuf);

	WL_RRM(("<802.11K Rx Req> %s: <Enter> mode=%d, TID=%d, "
		"bin0 range:%d, flags=0x%x, peer=%s\n",
		__FUNCTION__, rmreq_txstrm->mode, rmreq_txstrm->traffic_id,
		rmreq_txstrm->bin0_range, req->flags, eabuf));

	if (rmreq_txstrm->mode & (DOT11_RMREQ_MODE_PARALLEL | DOT11_RMREQ_MODE_ENABLE) ||
		!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_TSCM)) {
		WL_ERROR(("wl%d: %s: Unsupported Transmit Stream/Category Measurement request "
			"mode 0x%x\n", wlc->pub->unit, __FUNCTION__, rmreq_txstrm->mode));
		req->flags |= DOT11_RMREP_MODE_INCAPABLE;
		return;
	}

	rrm_info->txstrmreq = (rrm_txstrmreq_t *)MALLOCZ(wlc->osh, sizeof(rrm_txstrmreq_t));
	if (rrm_info->txstrmreq == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		req->flags |= DOT11_RMREP_MODE_REFUSED;
		return;
	}

	tscm_req = rrm_info->txstrmreq;
	tscm_req->token = rmreq_txstrm->token;
	tscm_req->duration = ltoh16_ua(&rmreq_txstrm->duration);
	tscm_req->tid = rmreq_txstrm->traffic_id;
	tscm_req->bin0_range = rmreq_txstrm->bin0_range;
	bcopy(&rmreq_txstrm->peer, &tscm_req->peer, sizeof(struct ether_addr));

	wlc_read_tsf(wlc, &tscm_req->start_tsf_l, &tscm_req->start_tsf_h);
}
#endif /* WL11K_ALL_MEAS */

/* send event to user space daemon "eventd" to handle all reports */
static void
wlc_rrm_send_event(wlc_rrm_info_t *rrm_info, struct scb *scb, char *ie, int len,
	int16 cat, int16 subevent)
{
	char buf[WL_RRM_EVENT_BUF_MAX_SIZE] = {0};
	wl_rrm_event_t *evt = (wl_rrm_event_t *)buf;

	if (len > (WL_RRM_EVENT_BUF_MAX_SIZE - sizeof(wl_rrm_event_t))) {
		WL_RRM(("%s: event length is too big (%d)\n", __FUNCTION__, len));
		return;
	}

	WL_RRM(("%s: <0x%x> trigger event....\n", __FUNCTION__, subevent));

	evt->version = RRM_EVENT_VERSION;
	evt->len = len;
	evt->cat = cat;
	evt->subevent = subevent;
	memcpy(evt->payload, ie, len);
	wlc_bss_mac_event(rrm_info->wlc, rrm_info->cur_cfg, WLC_E_RRM,
		&scb->ea, 0, 0, 0, buf, sizeof(buf));
	WL_RRM(("%s: <0x%x> trigger event [DONE]\n", __FUNCTION__, subevent));
}

#ifdef WL11K_BCN_MEAS
static void
wlc_rrm_recv_bcnrep(wlc_rrm_info_t *rrm_info, struct scb *scb, dot11_rm_ie_t *ie)
{
	dot11_rmrep_bcn_t *rmrep_bcn = NULL;
	int len = ie->len + TLV_HDR_LEN;
#ifdef WL11K_AP
	bcm_tlv_t *tlv = NULL;
	int tlv_len = len - (DOT11_RM_IE_LEN + DOT11_RMREP_BCN_LEN);
	uint8 *tlvs = NULL;
	struct dot11_bcn_prb bcn;
	wlc_ssid_t report_ssid;
	bcm_tlv_t *wpaie = NULL;
	bcm_tlv_t *wpa2ie = NULL;
	bcm_tlv_t *htie = NULL;
	bcm_tlv_t *mdie = NULL;
	uint32 flags = 0;
	const bcm_tlv_t *wme_ie;
#endif /* WL11K_AP */

	if (len < DOT11_RM_IE_LEN + DOT11_RMREP_BCN_LEN) {
		WL_ERROR(("%s: len %d is less than %d \n", __FUNCTION__,
			len, (DOT11_RM_IE_LEN + DOT11_RMREP_BCN_LEN)));
		return;
	} else {
		rmrep_bcn = (dot11_rmrep_bcn_t *)&ie[1];
	}
#ifdef WL11K_AP
	tlvs = (uint8 *)&rmrep_bcn[1];
	bzero(&report_ssid, sizeof(wlc_ssid_t));
	bzero(&bcn, sizeof(bcn));

	/* look for sub elements */
	tlv = (bcm_tlv_t *)&rmrep_bcn[1];
	if (tlv->id == DOT11_RMREP_BCN_FRM_BODY && tlv_len >= (TLV_HDR_LEN + DOT11_BCN_PRB_LEN)) {
		tlvs += TLV_HDR_LEN;
		tlv_len -= TLV_HDR_LEN;
		/* copy fixed beacon params */
		bcopy(tlvs, &bcn, sizeof(bcn));
		bcopy(&bcn.timestamp[1], &rmrep_bcn->parent_tsf, sizeof(uint32));

		tlvs += DOT11_BCN_PRB_LEN;
		tlv_len -= DOT11_BCN_PRB_LEN;

		/* look for ies within bcn frame */
		tlv = bcm_parse_tlvs(tlvs, tlv_len, DOT11_MNG_SSID_ID);
		if (tlv) {
			report_ssid.SSID_len = MIN(DOT11_MAX_SSID_LEN, tlv->len);
			bcopy(tlv->data, report_ssid.SSID, report_ssid.SSID_len);
		}
		htie = bcm_parse_tlvs(tlvs, tlv_len, DOT11_MNG_HT_CAP);
		mdie = bcm_parse_tlvs(tlvs, tlv_len, DOT11_MNG_MDIE_ID);
		wpa2ie = bcm_parse_tlvs(tlvs, tlv_len, DOT11_MNG_RSN_ID);
		if (bcm_find_vendor_ie(tlvs, tlv_len, BRCM_OUI, NULL, 0) != NULL)
			flags |= WLC_BSS_BRCM;

		if ((wme_ie = wlc_find_wme_ie(tlvs, tlv_len)) != NULL) {
			flags |= WLC_BSS_WME;
		}
		wpaie = (bcm_tlv_t *)bcm_find_wpaie(tlvs, tlv_len);
	}
#endif /* WL11K_AP */
	WL_ERROR(("%s: channel :%d, duration: %d, frame info: %d, rcpi: %d,"
		"rsni: %d, bssid " MACF ", antenna id: %d, parent tsf: %d\n",
		__FUNCTION__, rmrep_bcn->channel, rmrep_bcn->duration, rmrep_bcn->frame_info,
		rmrep_bcn->rcpi, rmrep_bcn->rsni, ETHERP_TO_MACF(&rmrep_bcn->bssid),
		rmrep_bcn->antenna_id, rmrep_bcn->parent_tsf));
#if defined(WL11K_AP) && defined(WL11K_NBR_MEAS)
	/* check if SSID and security match with this BSSID */
	if ((report_ssid.SSID_len == scb->bsscfg->SSID_len) &&
		(memcmp(&report_ssid.SSID, &scb->bsscfg->SSID, report_ssid.SSID_len) == 0) &&
		(memcmp(&rmrep_bcn->bssid, &scb->bsscfg->BSSID, ETHER_ADDR_LEN) != 0)) {

		if (wlc_rrm_security_match(rrm_info, scb->bsscfg, bcn.capability,
			wpaie, wpa2ie)) {
			WL_NONE(("%s: matched BSSID:" MACF ", SSID: %s\n", __FUNCTION__,
				ETHERP_TO_MACF(&rmrep_bcn->bssid), report_ssid.SSID));
			wlc_rrm_add_bcnnbr(rrm_info, scb, rmrep_bcn, &bcn, htie, mdie, flags);
		}
	}
#endif /* WL11K_AP && WL11K_NBR_MEAS */
}
#endif /* WL11K_BCN_MEAS */

#ifdef WL11K_ALL_MEAS
static void
wlc_rrm_recv_statrep(wlc_rrm_info_t *rrm_info, struct scb *scb, dot11_rm_ie_t *ie)
{
	dot11_rmrep_stat_t *rmrep_stat = (dot11_rmrep_stat_t *)&ie[1];
	int len = ie->len + TLV_HDR_LEN;
	rrm_scb_info_t *scb_info;
	int header_len = sizeof(dot11_rmrep_stat_t) + DOT11_RM_IE_LEN;
	int stats_len;
	char *buf;
	bcm_tlv_t *vendor_ie;

	if (scb == NULL) {
		WL_ERROR(("wl%d: %s: scb==NULL\n",
			rrm_info->wlc->pub->unit, __FUNCTION__));
		return;
	}
	scb_info = RRM_SCB_INFO(rrm_info, scb);

	if (!scb_info) {
		WL_ERROR(("wl%d: %s: "MACF" scb_info is NULL\n",
			rrm_info->wlc->pub->unit, __FUNCTION__, ETHER_TO_MACF(scb->ea)));
		return;
	}

	scb_info->timestamp = OSL_SYSUPTIME();
	scb_info->flag = WL_RRM_RPT_FALG_ERR;
	scb_info->len = 0;

	switch (rmrep_stat->group_id) {
	case DOT11_MNG_PROPR_ID:
		WL_INFORM(("wl%d: ie len:%d: min:%d max:%d\n", rrm_info->wlc->pub->unit, len,
			WL_RRM_RPT_MIN_PAYLOAD + header_len,
			WL_RRM_RPT_MAX_PAYLOAD + header_len));

		if ((len < WL_RRM_RPT_MIN_PAYLOAD + header_len) ||
			(len >= WL_RRM_RPT_MAX_PAYLOAD + header_len))
		{
			WL_ERROR(("wl%d: ie len:%d min:%d max:%d\n", rrm_info->wlc->pub->unit, len,
			WL_RRM_RPT_MIN_PAYLOAD + header_len,
			WL_RRM_RPT_MAX_PAYLOAD + header_len));
			break;
		}

		scb_info->flag = WL_RRM_RPT_FALG_GRP_ID_PROPR;
		scb_info->len = len - header_len;
		memcpy(scb_info->data, (void *)&rmrep_stat[1], scb_info->len);
		break;

	case DOT11_RRM_STATS_GRP_ID_0:
		stats_len = header_len + DOT11_RRM_STATS_RPT_LEN_GRP_ID_0;
		if (len < stats_len) {
			WL_ERROR(("wl%d: len[%d] < stats_len[%d]\n",
				rrm_info->wlc->pub->unit, len, stats_len));
			break;
		} else if (len == stats_len) {
			WL_RRM(("wl%d: No optional subelements in grp 0 stats\n",
				rrm_info->wlc->pub->unit));
			break;
		}

		buf = (char *)rmrep_stat + stats_len;
		vendor_ie = (bcm_tlv_t *)buf;

		if (vendor_ie->id != DOT11_MNG_VS_ID) {
			WL_ERROR(("wl%d: %s: Not found Valid Vendor IE\n",
				rrm_info->wlc->pub->unit, __FUNCTION__));
		}
		else {
			scb_info->len = vendor_ie->len;
			memcpy(scb_info->data, (void *)vendor_ie->data, vendor_ie->len);
			scb_info->flag = WL_RRM_RPT_FALG_GRP_ID_0;
		}
		break;

	default:
		break;
	}
}

#ifdef BCMDBG
static void
wlc_rrm_recv_noiserep(wlc_rrm_info_t *rrm_info, struct scb *scb, dot11_rm_ie_t *ie)
{
	dot11_rmrep_noise_t *rmrep_noise = (dot11_rmrep_noise_t *)&ie[1];

	WL_RRM(("<802.11K Rx Rep> %s: regulatory: %d, channel: %d, "
		"duration: %d, antid: 0x%x, anpi: %d\n",
		__FUNCTION__, rmrep_noise->reg, rmrep_noise->channel, rmrep_noise->duration,
		rmrep_noise->antid, (int8)rmrep_noise->anpi));

	WL_RRM_HEX("recv_noiserep:", (uchar *)ie, ie->len+TLV_HDR_LEN);
}

static void
wlc_rrm_recv_chloadrep(wlc_rrm_info_t *rrm_info, struct scb *scb, dot11_rm_ie_t *ie)
{
	dot11_rmrep_chanload_t *rmrep_chload = (dot11_rmrep_chanload_t *)&ie[1];

	WL_RRM(("<802.11K Rx Rep> %s: regulatory: %d, channel: %d, "
		"actualStartTime 0x%08x %08x duration: %d, chload: %d\n",
		__FUNCTION__, rmrep_chload->reg, rmrep_chload->channel, rmrep_chload->starttime[0],
		rmrep_chload->starttime[1], rmrep_chload->duration, rmrep_chload->channel_load));
}

static void
wlc_rrm_recv_framerep(wlc_rrm_info_t *rrm_info, struct scb *scb, dot11_rm_ie_t *ie)
{
	dot11_rmrep_frame_t *rmrep_frame = (dot11_rmrep_frame_t *)&ie[1];
	dot11_rmrep_frmentry_t *frm_e;

	WL_RRM(("<802.11K Rx Rep> %s: regulatory: %d, channel :%d, duration: %d\n",
		__FUNCTION__, rmrep_frame->reg, rmrep_frame->channel, rmrep_frame->duration));

	frm_e = (dot11_rmrep_frmentry_t *)((uint8 *)&rmrep_frame[1] + 2);

	/* Display frame entry */
	WL_RRM(("%s: phy_type=%d, avg_rcpi=%d, last_rsni=%d, "
		"last_rcpi=%d, ant_id=%d, frame_cnt=%d\n",
		__FUNCTION__, frm_e->phy_type, (int8)frm_e->avg_rcpi, (int8)frm_e->last_rsni,
		(int8)frm_e->last_rcpi, frm_e->ant_id, frm_e->frame_cnt));
}

static void
wlc_rrm_recv_txstrmrep(wlc_rrm_info_t *rrm_info, struct scb *scb, dot11_rm_ie_t *ie)
{
	char eabuf[ETHER_ADDR_STR_LEN];

	dot11_rmrep_tx_stream_t *rmrep_txstrm = (dot11_rmrep_tx_stream_t *)&ie[1];
	bcm_ether_ntoa(&rmrep_txstrm->peer, eabuf);

	WL_RRM(("<802.11K Rx Rep> %s: actualStartTime 0x%08x %08x duration: %d, "
		"peer: %s, TID: %d\n", __FUNCTION__, rmrep_txstrm->starttime[0],
		rmrep_txstrm->starttime[1], rmrep_txstrm->duration, eabuf,
		rmrep_txstrm->traffic_id));
}

static void
wlc_rrm_recv_lcirep(wlc_rrm_info_t *rrm_info, struct scb *scb, dot11_rm_ie_t *ie)
{
	dot11_rmrep_ftm_lci_t *rmrep_lci = (dot11_rmrep_ftm_lci_t *)ie;
	char *buf = (char *)&rmrep_lci[1];
	int i;

	WL_RRM(("<802.11K Rx Rep> %s: id=%d, len=%d\n",
		__FUNCTION__, rmrep_lci->lci_sub_id, rmrep_lci->lci_sub_len));
	for (i = 0; i < rmrep_lci->lci_sub_len; i++) {
		if ((i&0xf) == 0)
			WL_RRM(("\n%04x: ", i));
		WL_RRM(("%02X ", buf[i]));
	}
	WL_RRM(("\n\n"));
}

static void
wlc_rrm_recv_civicrep(wlc_rrm_info_t *rrm_info, struct scb *scb, dot11_rm_ie_t *ie)
{
	dot11_rmrep_ftm_civic_t *rmrep_civic = (dot11_rmrep_ftm_civic_t *)ie;
	char *buf = (char *)&rmrep_civic[1];
	int i;

	WL_RRM(("<802.11K Rx Rep> %s: id=%d, len=%d type=%d\n", __FUNCTION__,
		rmrep_civic->civloc_sub_id, rmrep_civic->civloc_sub_len,
		rmrep_civic->civloc_type));
	for (i = 0; i < rmrep_civic->civloc_sub_len; i++) {
		if ((i&0xf) == 0)
			WL_RRM(("\n%04x: ", i));
		WL_RRM(("%02X ", buf[i]));
	}
	WL_RRM(("\n\n"));
}

static void
wlc_rrm_recv_locidrep(wlc_rrm_info_t *rrm_info, struct scb *scb, dot11_rm_ie_t *ie)
{
	dot11_rmrep_locid_t *rmrep_locid = (dot11_rmrep_locid_t *)ie;
	char *buf = (char *)&rmrep_locid[1];
	int i;
	WL_RRM(("<802.11K Rx Rep> %s: id=%d, len=%d\n",
		__FUNCTION__, rmrep_locid->locid_sub_id, rmrep_locid->locid_sub_len));
	for (i = 0; i < rmrep_locid->locid_sub_len; i++) {
		if ((i&0xf) == 0)
			WL_RRM(("\n%04x: ", i));
		WL_RRM(("%02X ", buf[i]));
	}
	WL_RRM(("\n\n"));
}
#endif /* BCMDBG */
#endif /* WL11K_ALL_MEAS */

#ifdef WL11K_BCN_MEAS
static void
wlc_rrm_begin_scan(wlc_rrm_info_t *rrm_info)
{
	rrm_bcnreq_t *bcn_req;
	uint32 ret;
	char eabuf[ETHER_ADDR_STR_LEN];
	wlc_info_t *wlc = rrm_info->wlc;
	wlc_rrm_req_state_t *rrm_state = rrm_info->rrm_state;

	bcn_req = rrm_info->bcnreq;
	ASSERT(bcn_req != NULL);
	wlc_bss_list_free(wlc, &bcn_req->scan_results);
	bzero(bcn_req->scan_results.ptrs, MAXBSS * sizeof(wlc_bss_info_t *));

	wlc_read_tsf(wlc, &bcn_req->start_tsf_l, &bcn_req->start_tsf_h);

	if (bcn_req->scan_type == DOT11_RMREQ_BCN_TABLE) {
		WL_ERROR(("%s: type = BCN_TABLE, defer it to WLC_RRM_WAIT_END_MEAS\n",
		__FUNCTION__));
	}
	else {
		bcm_ether_ntoa(&bcn_req->bssid, eabuf);
		/* clear stale scan_results */
		wlc_bss_list_free(wlc, wlc->scan_results);
		WL_ERROR(("%s: duration: %d, channel num: %d, bssid: %s\n", __FUNCTION__,
			bcn_req->duration, bcn_req->channel_num,
			eabuf));

		ret = wlc_scan_request(wlc, DOT11_BSSTYPE_ANY, &bcn_req->bssid,
			1, &bcn_req->ssid,
			bcn_req->scan_type, 1, bcn_req->duration, bcn_req->duration,
			-1, bcn_req->chanspec_list, bcn_req->channel_num,
			TRUE, wlc_rrm_bcnreq_scancb, wlc);

		if (ret != BCME_OK)
			return;
		rrm_state->scan_active = TRUE;

		if (rrm_info->bcn_req_thrtl_win) {
			/* Start a new win if no win is running */
			if (!rrm_info->bcn_req_thrtl_win_sec) {
				rrm_info->bcn_req_thrtl_win_sec = rrm_info->bcn_req_thrtl_win;
			}
		}

	}
}

/* Return a pointer to the AP Channel Report IE if it's in the beacon for the bss. */
static dot11_ap_chrep_t*
wlc_rrm_get_ap_chrep(const wlc_bss_info_t *bi)
{
	dot11_ap_chrep_t *ap_chrep = NULL;
	bcm_tlv_t *tlv;

	if (bi->bcn_prb && (bi->bcn_prb_len >= DOT11_BCN_PRB_LEN)) {
		tlv = (bcm_tlv_t *)((uint8 *)bi->bcn_prb + DOT11_BCN_PRB_FIXED_LEN);
		ap_chrep = (dot11_ap_chrep_t*)bcm_parse_tlvs((uint8 *)tlv, bi->bcn_prb_len -
			DOT11_BCN_PRB_FIXED_LEN, DOT11_MNG_AP_CHREP_ID);
	}

	return ap_chrep;
}

/* Return the Regulatory Class from the AP Channel Report for the BSS if present, or
 * DOT11_OP_CLASS_NONE if not.
 */
static uint8
wlc_rrm_get_ap_chrep_reg(const wlc_bss_info_t *bi)
{
	uint8 reg = DOT11_OP_CLASS_NONE;
	dot11_ap_chrep_t *ap_chrep = NULL;

	ap_chrep = (dot11_ap_chrep_t*)wlc_rrm_get_ap_chrep(bi);
	if (ap_chrep)
		reg = ap_chrep->reg;

	return reg;
}

/* Adds an empty beacon report if there's room. */
static int
wlc_rrm_add_empty_bcnrep(const wlc_rrm_info_t *rrm_info, uint8 *bufptr, uint buflen)
{
	dot11_rm_ie_t *rmrep_ie;

	if (buflen < DOT11_RM_IE_LEN) {
		return 0;
	}

	rmrep_ie = (dot11_rm_ie_t *)bufptr;
	rmrep_ie->len = DOT11_RM_IE_LEN - 2;
	rmrep_ie->id = DOT11_MNG_MEASURE_REPORT_ID;
	rmrep_ie->token = rrm_info->bcnreq->token;
	rmrep_ie->mode = 0;
	rmrep_ie->type = DOT11_MEASURE_TYPE_BEACON;

	return DOT11_RM_IE_LEN;
}

static void
wlc_rrm_bcnreq_scancb(void *arg, int status, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = (wlc_info_t*)arg;
	wlc_rrm_info_t *rrm_info = (wlc_rrm_info_t *)wlc->rrm_info;
	rrm_bcnreq_t *bcn_req;
	wlc_rrm_req_state_t *rrm_state = rrm_info->rrm_state;

	if (rrm_info->bcnreq == NULL || rrm_state == NULL) {
		WL_ERROR(("%s: bcn_req or rrm_state is NULL\n", __FUNCTION__));
		return;
	}

	WL_ERROR(("%s: state: %d, status: %d\n", __FUNCTION__, rrm_state->step, status));
	bcn_req = rrm_info->bcnreq;

	bcn_req->scan_status = status;
	rrm_state->scan_active = FALSE;

	if (status == WLC_E_STATUS_ABORT)
		return;

	/* copy scan results to rrm_state for reporting */
	wlc_bss_list_xfer(wlc->scan_results, &bcn_req->scan_results);

	wl_add_timer(wlc->wl, rrm_info->rrm_timer, 0, 0);
	WL_ERROR(("%s: scan count: %d\n", __FUNCTION__,
		bcn_req->scan_results.count));

	rrm_info->bcn_req_win_scan_cnt++;
	if (rrm_info->bcn_req_off_chan_time_allowed) {
		rrm_info->bcn_req_off_chan_time += OSL_SYSUPTIME() -
			rrm_info->bcn_req_scan_start_timestamp;
	}
}

static uint32
wlc_rrm_get_rm_hdr_ptr(wlc_rrm_info_t *rrm_info, uint8 *pbody, uint32 body_len, uint8 **buf)
{
	uint32 buflen;

	*buf = pbody + DOT11_RM_ACTION_LEN;
	buflen = body_len - DOT11_RM_ACTION_LEN;

	return (buflen);
}

static int32
wlc_rrm_send_bcnrep(wlc_rrm_info_t *rrm_info, wlc_bss_list_t *bsslist)
{
	wlc_info_t *wlc = rrm_info->wlc;
	wlc_bss_info_t *bi;
	void *p = NULL;
	uint8 *pbody, *buf = NULL;
	unsigned int i = 0, len = 0, buflen = 0;
	unsigned int bss_added = 0;
	struct scb *scb;
	uint32 j = 0, body_len = 0, availlen = 0;
	int32 ret = TRUE;

	WL_RRM(("%s: <Enter> bsslist->count %d\n", __FUNCTION__, bsslist->count));

	/* Use a do/while to ensure that a report packet is always allocated even if bsslist is
	 * empty.
	 */
	do {
		/* Allocate and initialise packet. */
		if (buflen <= 0) {
			/* calculate length */
			body_len = DOT11_RM_ACTION_LEN;
			availlen = ETHER_MAX_DATA - body_len;
			j = i;
			WL_INFORM(("%s: bss count %u j %u bodylen %u availlen %u\n",
				__FUNCTION__, bsslist->count, j, body_len, availlen));
			do {
				if (j < bsslist->count) {
					bi = bsslist->ptrs[j];
					len = wlc_rrm_bcnrep_add(rrm_info, bi, NULL, availlen);
					WL_INFORM(("%s: len %u bodylen %u availlen %u\n",
						__FUNCTION__, len, body_len, availlen));
					if (((body_len + len) < availlen)) {
						body_len += len;
						j++;
					} else {
						break;
					}
				}
			} while (j < bsslist->count);

			if ((p = wlc_rrm_prep_gen_report(rrm_info, body_len, &pbody)) == NULL) {
				WL_ERROR(("%s: failed to get mgmt frame\n", __FUNCTION__));
				return BCME_NOMEM;
			}

			buflen = wlc_rrm_get_rm_hdr_ptr(rrm_info, pbody, body_len, &buf);
		}

		/* Send, or add a BSS to the report. */
		if (i < bsslist->count) {
			bi = bsslist->ptrs[i];
			len = wlc_rrm_bcnrep_add(rrm_info, bi, buf, buflen);
			if (len > 0) {
				i++;
				buflen -= len;
				buf += len;
				bss_added += 1;
			} else {
				/* We allocated buffer and there is no space to write,
				 * something wrong.
				 */
				WL_ERROR(("%s: No space to write %u", __FUNCTION__, buflen));
				break;
			}
			if (buflen == 0) {
				/* buflen 0 means another beacon report won't fit
				 * in the buffer. Immediately send what we have so next loop
				 * iteration can allocate frame and add this beacon report.
				 */
				scb = wlc_scbfind(wlc, rrm_info->cur_cfg, &rrm_info->da);
				ret = wlc_sendmgmt(wlc, p, rrm_info->cur_cfg->wlcif->qi, scb);
				/* Force allocation of new buffer */
				buflen = 0;
				bss_added = 0;
			}
			WL_INFORM(("%s: buflen %u rpt len %u bss added %u\n",
				__FUNCTION__, buflen, len, bss_added));
		}
	} while (i < bsslist->count);

	if (buflen > 0) {
		/* We should not be here !!!
		 * Allocated buffer but not consumed, need to manually free packet.
		 * This is a malformed packet.
		 */
		ASSERT(0);
		PKTFREE(wlc->osh, p, TRUE);

		/* Make sure we send at least one beacon report even if it is empty. */
		body_len = DOT11_RM_ACTION_LEN + DOT11_RM_IE_LEN;
		if ((p = wlc_rrm_prep_gen_report(rrm_info, body_len, &pbody)) == NULL) {
			WL_ERROR(("%s: failed to get mgmt frame\n", __FUNCTION__));
			return BCME_NOMEM;
		}
		buflen = wlc_rrm_get_rm_hdr_ptr(rrm_info, pbody, body_len, &buf);
		len = wlc_rrm_add_empty_bcnrep(rrm_info, buf, buflen);
		buflen -= len;

		if (buflen == 0) {
			scb = wlc_scbfind(wlc, rrm_info->cur_cfg, &rrm_info->da);
			ret = wlc_sendmgmt(wlc, p, rrm_info->cur_cfg->wlcif->qi, scb);
		} else {
			/* We should not be here !!! */
			ASSERT(0);
			PKTFREE(wlc->osh, p, TRUE);
		}
	}
	if (rrm_info->bcnreq) {
		wlc_bss_list_free(rrm_info->wlc, &rrm_info->bcnreq->scan_results);
	}

	if (ret == FALSE) {
		return (BCME_ERROR);
	}

	return (BCME_OK);
}

/*
 * This API is used to get beaon report length as well as writing beacon
 * report into buffer.
 * bufptr = NULL; returns length without writing baecon report
 * bufptr = Non NULL; returns length as well as writes report
 */
static int
wlc_rrm_bcnrep_add(wlc_rrm_info_t *rrm_info, wlc_bss_info_t *bi, uint8 *bufptr, uint buflen)
{
	dot11_rm_ie_t *rmrep_ie = NULL;
	dot11_rmrep_bcn_t *rmrep_bcn = NULL;
	bcm_tlv_t *frm_body_tlv = NULL;
	unsigned int elem_len, i;
	uint8 in_bcn_req = 0;
	int tlv_len = 0;
	char eabuf[ETHER_ADDR_STR_LEN];
	uint16 frm_body_tlv_len = 0;
	uint32 dummy_tsf_h, tsf_l;

	ASSERT(rrm_info);

	elem_len = DOT11_RM_IE_LEN + DOT11_RMREP_BCN_LEN;

	if (bufptr) {
		if (buflen < elem_len) {
			return 0;
		}
		rmrep_ie = (dot11_rm_ie_t *)bufptr;
		rmrep_ie->id = DOT11_MNG_MEASURE_REPORT_ID;
		rmrep_ie->token = rrm_info->bcnreq->token;
		rmrep_ie->mode = 0;
		rmrep_ie->type = DOT11_MEASURE_TYPE_BEACON;

		rmrep_bcn = (dot11_rmrep_bcn_t *)&rmrep_ie[1];
		rmrep_bcn->reg = wlc_rrm_get_ap_chrep_reg(bi);
		if (rmrep_bcn->reg == DOT11_OP_CLASS_NONE) {
			rmrep_bcn->reg = rrm_info->bcnreq->reg;
		}
		rmrep_bcn->channel = wf_chspec_ctlchan(bi->chanspec);
		bcopy(&rrm_info->bcnreq->start_tsf_l, rmrep_bcn->starttime, (2*sizeof(uint32)));
		rmrep_bcn->duration = htol16(rrm_info->bcnreq->duration);
		rmrep_bcn->frame_info = 0;
		rmrep_bcn->rcpi = (uint8)bi->RSSI;
		rmrep_bcn->rsni = (uint8)bi->SNR;
		bcopy(&bi->BSSID, &rmrep_bcn->bssid, ETHER_ADDR_LEN);
		rmrep_bcn->antenna_id = 0;
		wlc_read_tsf(rrm_info->wlc, &tsf_l, &dummy_tsf_h);
		rmrep_bcn->parent_tsf = tsf_l;

		bcm_ether_ntoa(&rmrep_bcn->bssid, eabuf);
		WL_RRM(("%s: token: %d, channel :%d, duration: %d, frame info: %d, rcpi: %d,"
			"rsni: %d, bssid: %s, antenna id: %d, parent tsf: %d\n",
			__FUNCTION__, rmrep_ie->token, rmrep_bcn->channel, rmrep_bcn->duration,
			rmrep_bcn->frame_info, (int8)rmrep_bcn->rcpi, (int8)rmrep_bcn->rsni,
			eabuf, rmrep_bcn->antenna_id, rmrep_bcn->parent_tsf));
	}

	/* add bcn frame body subelement */
	if (rrm_info->bcnreq->rep_detail
			== DOT11_RMREQ_BCN_REPDET_ALL ||
			rrm_info->bcnreq->rep_detail
				== DOT11_RMREQ_BCN_REPDET_REQUEST) {
		bcm_tlv_t *tlv;
		int tlvs_len = 0;

		elem_len += TLV_HDR_LEN + DOT11_BCN_PRB_FIXED_LEN;
		if (bufptr) {
			if (buflen < elem_len) {
				return 0;
			}
			frm_body_tlv = (bcm_tlv_t *)&rmrep_bcn[1];
			frm_body_tlv->id = DOT11_RMREP_BCN_FRM_BODY;
			frm_body_tlv->len = DOT11_BCN_PRB_FIXED_LEN;
			bcopy(bi->bcn_prb, &frm_body_tlv->data[0], DOT11_BCN_PRB_FIXED_LEN);

			bufptr += elem_len;
		}
		frm_body_tlv_len = DOT11_BCN_PRB_FIXED_LEN;
		tlvs_len = bi->bcn_prb_len - DOT11_BCN_PRB_FIXED_LEN;
		if (tlvs_len) {
			tlv = (bcm_tlv_t *)((uint8 *)bi->bcn_prb + DOT11_BCN_PRB_FIXED_LEN);
			while (tlv) {
				tlv_len = TLV_HDR_LEN + tlv->len;
				if (frm_body_tlv_len + tlv_len >
						DOT11_RMREP_BCN_FRM_BODY_LEN_MAX) {
					WL_INFORM(("beacon frame body len exceeding limit"
						" So skipping further IEs, len = %d\n",
						frm_body_tlv_len + tlv_len));
					break;
				}

				if (rrm_info->bcnreq->rep_detail
						== DOT11_RMREQ_BCN_REPDET_REQUEST) {
					for (i = 0; i < rrm_info->bcnreq->req_eid_num; i++) {
						if (tlv->id == rrm_info->bcnreq->req_eid[i]) {
							in_bcn_req = 1;
							break;
						}
					}
				}
				if (rrm_info->bcnreq->rep_detail
						== DOT11_RMREQ_BCN_REPDET_ALL || in_bcn_req == 1) {
					elem_len += tlv_len;
					if (bufptr) {
						if (buflen < elem_len) {
							return 0;
						}
						bcopy(tlv, bufptr, tlv_len);
						bufptr += tlv_len;
						frm_body_tlv->len += (uint8)tlv_len;
					}
					in_bcn_req = 0;
				}
				tlv = bcm_next_tlv(tlv, (int *)&tlvs_len);
				frm_body_tlv_len += tlv_len;
			}
		}
	}
	if (bufptr && rmrep_ie) {
		rmrep_ie->len = (uint8)(elem_len - TLV_HDR_LEN);
	}

	WL_INFORM(("%s: elem len %u\n", __FUNCTION__, elem_len));
	return (elem_len);
}

static void
wlc_rrm_bcnreq_scancache(wlc_rrm_info_t *rrm_info, wlc_ssid_t *ssid, struct ether_addr *bssid)
{
	wlc_info_t *wlc = rrm_info->wlc;
	wlc_bss_list_t bss_list;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG */
	ASSERT(rrm_info->bcnreq->channel_num);

	WL_RRM(("%s: <Enter>\n", __FUNCTION__));

	if (!SCANCACHE_ENAB(wlc->scan) || SCAN_IN_PROGRESS(wlc->scan)) {
		WL_ERROR(("%s: scancache: %d, scan_in_progress: %d\n",
			__FUNCTION__, SCANCACHE_ENAB(wlc->scan),
			SCAN_IN_PROGRESS(wlc->scan)));
		/* send a null beacon report */
		wlc_rrm_rep_err(rrm_info, DOT11_MEASURE_TYPE_BEACON, rrm_info->bcnreq->token, 0);
		return;
	}

	if (ETHER_ISNULLADDR(bssid)) {
		bssid = NULL;
	}
	wlc_scan_get_cache(wlc->scan, bssid, 1, ssid, DOT11_BSSTYPE_ANY,
		rrm_info->bcnreq->chanspec_list,
		rrm_info->bcnreq->channel_num, &bss_list);

#ifdef BCMDBG
	if (bssid) {
		bcm_ether_ntoa(bssid, eabuf);
		WL_ERROR(("%s: ssid: %s, bssid: %s, bss_list.count: %d\n",
			__FUNCTION__, ssid->SSID, eabuf, bss_list.count));
	} else {
		WL_ERROR(("%s: ssid: %s, bssid: NULL, bss_list.count: %d\n",
			__FUNCTION__, ssid->SSID, bss_list.count));
	}
#endif /* BCMDBG */

	if (bss_list.count) {
		wlc_rrm_send_bcnrep(rrm_info, &bss_list);
		wlc_bss_list_free(wlc, &bss_list);
	} else {
		wlc_rrm_rep_err(rrm_info, DOT11_MEASURE_TYPE_BEACON,
			rrm_info->bcnreq->token, 0);
	}
}

static void
wlc_rrm_send_bcnreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, bcn_req_t *bcnreq)
{
	void *p;
	uint8 *pbody, repdet;
	int buflen;
	int dur;
	int channel;
	uint16 interval;
	uint16 repcond;
	dot11_rmreq_t *rmreq;
	dot11_rmreq_bcn_t *rmreq_bcn;
	struct ether_addr *da;
	uint8 bcnmode;
	int len = 0;
	int i = 0;
	struct scb *scb;

	wlc_info_t *wlc = rrm_info->wlc;
	da = &bcnreq->da;
	dur = bcnreq->dur;
	channel = bcnreq->channel;
	interval = bcnreq->random_int;
	bcnmode = bcnreq->bcn_mode;

	/* rm frame action header + bcn request header */
	buflen = DOT11_RMREQ_LEN + DOT11_RMREQ_BCN_LEN;
	if (bcnreq->ssid.SSID_len == 1 &&
		memcmp(bcnreq->ssid.SSID, WLC_RRM_WILDCARD_SSID_IND, 1) == 0) {
		buflen += TLV_HDR_LEN;
	}
	else if (bcnreq->ssid.SSID_len)
		buflen += bcnreq->ssid.SSID_len + TLV_HDR_LEN;

	/* AP Channel Report */
	len = bcnreq->chspec_list.num;
	if (len) {
		buflen += len + TLV_HDR_LEN + 1;
	}

	/* buflen is length of Reporting Detail
	   and optional beacon request id if present + 3 * TLV_HDR_LEN
	*/
	if (bcnreq->req_elements) {
		if (bcnreq->reps)
			buflen += (WLC_RRM_BCNREQ_NUM_REQ_ELEM + TLV_HDR_LEN) +
				(1 + TLV_HDR_LEN) + (DOT11_RMREQ_BCN_REPINFO_LEN +
				TLV_HDR_LEN);
		else
			buflen += (WLC_RRM_BCNREQ_NUM_REQ_ELEM + TLV_HDR_LEN) +
				(1 + TLV_HDR_LEN);
	}
	else {
		if (bcnreq->reps)
			buflen += (1 + TLV_HDR_LEN) + (DOT11_RMREQ_BCN_REPINFO_LEN +
				TLV_HDR_LEN);
		else
			buflen += (1 + TLV_HDR_LEN);
	}
	if ((p = wlc_rrm_prep_gen_request(rrm_info, cfg, da, buflen, &pbody)) == NULL)
		return;

	rmreq = (dot11_rmreq_t *)pbody;
	rmreq->reps = bcnreq->reps;
	pbody += DOT11_RMREQ_LEN;

	rmreq_bcn = (dot11_rmreq_bcn_t *)&rmreq->data[0];

	rmreq_bcn->id = DOT11_MNG_MEASURE_REQUEST_ID;

	rmreq_bcn->len = DOT11_RMREQ_BCN_LEN - TLV_HDR_LEN;

	WLC_RRM_UPDATE_TOKEN(rrm_info->req_elt_token);
	rmreq_bcn->token = rrm_info->req_elt_token;

	rmreq_bcn->mode = 0;

	rmreq_bcn->type = DOT11_MEASURE_TYPE_BEACON;
	rmreq_bcn->reg = wlc_get_regclass(wlc->cmi, cfg->current_bss->chanspec);
	rmreq_bcn->channel = channel;
	rmreq_bcn->interval = interval;
	rmreq_bcn->duration = dur;
	rmreq_bcn->bcn_mode = bcnmode;

	pbody += DOT11_RMREQ_BCN_LEN;
	/* sub-element SSID */
	if (bcnreq->ssid.SSID_len) {
		if (bcnreq->ssid.SSID_len == 1 &&
		memcmp(bcnreq->ssid.SSID, WLC_RRM_WILDCARD_SSID_IND, 1) == 0) {
			bcnreq->ssid.SSID_len = 0;
		}

		pbody = bcm_write_tlv(DOT11_MNG_SSID_ID, (uint8 *)bcnreq->ssid.SSID,
			bcnreq->ssid.SSID_len, pbody);
		rmreq_bcn->len += bcnreq->ssid.SSID_len + TLV_HDR_LEN;
	}

	if (rmreq->reps) {
		/* Beacon Reporting Information subelement may be included only for
		   repeated measurements, includes reporting condition
		*/
		repcond = DOT11_RMREQ_BCN_REPCOND_DEFAULT << 1;
		pbody = bcm_write_tlv(DOT11_RMREQ_BCN_REPINFO_ID, &repcond,
			DOT11_RMREQ_BCN_REPINFO_LEN, pbody);
		rmreq_bcn->len += DOT11_RMREQ_BCN_REPINFO_LEN + TLV_HDR_LEN;
	}

	/* request specific IEs since default will return only up to 224 bytes of beacon */
	if (bcnreq->req_elements)
		repdet = DOT11_RMREQ_BCN_REPDET_REQUEST;
	else
		repdet = DOT11_RMREQ_BCN_REPDET_ALL;

	pbody = bcm_write_tlv(DOT11_RMREQ_BCN_REPDET_ID, &repdet, 1, pbody);
	rmreq_bcn->len += 1 + TLV_HDR_LEN;

	/* add specific IEs to request */
	if (bcnreq->req_elements) {
		*pbody++ = DOT11_RMREQ_BCN_REQUEST_ID;
		*pbody++ = WLC_RRM_BCNREQ_NUM_REQ_ELEM;
		*pbody++ = DOT11_MNG_SSID_ID;
		*pbody++ = DOT11_MNG_HT_CAP;
		*pbody++ = DOT11_MNG_RSN_ID;
		*pbody++ = DOT11_MNG_AP_CHREP_ID;
		*pbody++ = DOT11_MNG_MDIE_ID;
		*pbody++ = DOT11_MNG_BSS_AVAL_ADMISSION_CAP_ID;
		*pbody++ = DOT11_MNG_RRM_CAP_ID;
		*pbody++ = DOT11_MNG_VS_ID;
		rmreq_bcn->len += WLC_RRM_BCNREQ_NUM_REQ_ELEM + TLV_HDR_LEN;
	}

	if (len) {
		/* AP Channel Report */
		*pbody++ = DOT11_RMREQ_BCN_APCHREP_ID;
		/* Length */
		*pbody++ = len + 1;

		/* only report channels for a single operating class, per spec */
		*pbody++ = wlc_get_regclass(wlc->cmi, bcnreq->chspec_list.list[0]);

		for (i = 0; i < len; i++)		{
			*pbody++ = CHSPEC_CHANNEL(bcnreq->chspec_list.list[i]);
		}
		rmreq_bcn->len += len + TLV_HDR_LEN + 1;
	}
	WL_ERROR(("%s: id: %d, len: %d, type: %d, channel: %d, duration: %d, bcn_mode: %d\n",
		__FUNCTION__, rmreq_bcn->id, rmreq_bcn->len, rmreq_bcn->type, rmreq_bcn->channel,
		rmreq_bcn->duration, rmreq_bcn->bcn_mode));

	memset(&rmreq_bcn->bssid.octet, 0xff, ETHER_ADDR_LEN);
	scb = wlc_scbfind(wlc, cfg, da);
	wlc_sendmgmt(wlc, p, cfg->wlcif->qi, scb);
}
#endif	/* WL11K_BCN_MEAS */

#ifdef WL11K_ALL_MEAS
#ifdef WL11K_AP
static void
wlc_rrm_add_ap_chrep(wlc_rrm_info_t *rrm, wlc_bsscfg_t *cfg, uint8 *bufstart)
{
	wlc_info_t *wlc = rrm->wlc;
	reg_nbr_count_t *nbr_cnt;
	uint8 ap_reg;
	uint8 chan_count = 0;
	int ch;
	rrm_bsscfg_cubby_t *rrm_cfg;

	rrm_cfg = RRM_BSSCFG_CUBBY(rrm, cfg);
	ASSERT(rrm_cfg != NULL);

	/* Get AP current regulatory class */
	ap_reg = wlc_get_regclass(wlc->cmi, cfg->current_bss->chanspec);

	nbr_cnt = rrm_cfg->nbr_cnt_head;
	while (nbr_cnt) {
		uint8 buf[TLV_HDR_LEN + 255];
		dot11_ap_chrep_t *ap_chrep = (dot11_ap_chrep_t *)buf;

		bzero(ap_chrep, sizeof(buf));

		/* check if the regulatory matches AP's regulatory class */
		if (nbr_cnt->reg == ap_reg) {
			/* Parse the channels found in this regulatory class */
			for (ch = 0; ch < MAXCHANNEL; ch++) {
				if (nbr_cnt->nbr_count_by_channel[ch] > 0) {
					ap_chrep->chanlist[chan_count] = ch;
					chan_count++;
				}
			}

			if (chan_count > 0) {
				ap_chrep->reg = nbr_cnt->reg;

				bcm_write_tlv(DOT11_MNG_AP_CHREP_ID, &ap_chrep->reg,
					chan_count + 1, bufstart);
			}
			break;
		}
		/* Move to the next regulatory class */
		nbr_cnt = nbr_cnt->next;
	}
}

/* calculate length of AP Channel Report element */
static uint
wlc_rrm_ap_chrep_len(wlc_rrm_info_t *rrm, wlc_bsscfg_t *cfg)
{
	reg_nbr_count_t *nbr_cnt;
	rrm_bsscfg_cubby_t *rrm_cfg;
	wlc_info_t *wlc = rrm->wlc;
	uint8 ap_reg;
	int ch;
	uint len = 0;

	rrm_cfg = RRM_BSSCFG_CUBBY(rrm, cfg);
	ASSERT(rrm_cfg != NULL);

	/* Get AP current regulatory class */
	ap_reg = wlc_get_regclass(wlc->cmi, cfg->current_bss->chanspec);

	nbr_cnt = rrm_cfg->nbr_cnt_head;
	while (nbr_cnt) {
		/* check if the regulatory matches AP's regulatory class */
		if (nbr_cnt->reg == ap_reg) {
			len += TLV_HDR_LEN + 1;
			for (ch = 0; ch < MAXCHANNEL; ch++) {
				if (nbr_cnt->nbr_count_by_channel[ch] > 0) {
					len++;
				}
			}
			break;
		}
		nbr_cnt = nbr_cnt->next;
	}
	return len;
}
#endif	/* WL11K_AP */
#endif	/* WL11K_ALL_MEAS */

/* FIXME: PR81518: need a wlc_bsscfg_t param to correctly determine addresses and
 * queue for wlc_sendmgmt()
 */
static void
wlc_rrm_rep_err(wlc_rrm_info_t *rrm_info, uint8 type, uint8 token, uint8 reason)
{
	wlc_info_t *wlc = rrm_info->wlc;
	void *p;
	uint8 *pbody;
	dot11_rm_action_t *rm_rep;
	dot11_rm_ie_t *rmrep_ie;
	struct scb *scb;

	if ((p = wlc_frame_get_action(wlc, &rrm_info->da,
		&rrm_info->cur_cfg->cur_etheraddr, &rrm_info->cur_cfg->BSSID,
		(DOT11_RM_ACTION_LEN + DOT11_RM_IE_LEN), &pbody, DOT11_ACTION_CAT_RRM)) != NULL) {

		rm_rep = (dot11_rm_action_t *)pbody;
		rm_rep->category = DOT11_ACTION_CAT_RRM;
		rm_rep->action = DOT11_RM_ACTION_RM_REP;
		rm_rep->token = rrm_info->dialog_token;
		rmrep_ie = (dot11_rm_ie_t *)&rm_rep->data[0];
		rmrep_ie->id = DOT11_MNG_MEASURE_REPORT_ID;
		rmrep_ie->len = DOT11_RM_IE_LEN - TLV_HDR_LEN;
		rmrep_ie->token = token;
		rmrep_ie->mode = reason;
		rmrep_ie->type = type;

		scb = wlc_scbfind(wlc, rrm_info->cur_cfg, &rrm_info->da);
		wlc_sendmgmt(wlc, p, rrm_info->cur_cfg->wlcif->qi, scb);
	}
}

#if defined(WL11K_BCN_MEAS) || defined(WL11K_NBR_MEAS) || defined(WL11K_ALL_MEAS)
static void*
wlc_rrm_prep_gen_request(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	struct ether_addr *da, unsigned int buflen, uint8 **pbody)
{
	void *p;
	wlc_info_t *wlc = rrm_info->wlc;
	dot11_rmreq_t *rmreq;

	if ((p = wlc_frame_get_action(wlc, da, &cfg->cur_etheraddr,
		&cfg->BSSID, buflen, pbody, DOT11_ACTION_CAT_RRM)) == NULL) {
		return NULL;
	}

	rmreq = (dot11_rmreq_t *)*pbody;
	rmreq->category = DOT11_ACTION_CAT_RRM;
	rmreq->action = DOT11_RM_ACTION_RM_REQ;
	WLC_RRM_UPDATE_TOKEN(rrm_info->req_token);
	rmreq->token = rrm_info->req_token;

	return p;
}

static void* wlc_rrm_prep_gen_report(wlc_rrm_info_t *rrm_info, unsigned int len, uint8 **pbody)
{
	wlc_info_t *wlc = rrm_info->wlc;
	dot11_rm_action_t *rm_rep;
	dot11_rm_ie_t *rmrep_ie;
	void *p = NULL;

	p = wlc_frame_get_action(wlc, &rrm_info->da, &rrm_info->cur_cfg->cur_etheraddr,
		&rrm_info->cur_cfg->BSSID, len, pbody, DOT11_ACTION_CAT_RRM);

	if (p == NULL) {
		WL_ERROR(("%s: failed to get mgmt frame\n", __FUNCTION__));
		return NULL;
	}
	rm_rep = (dot11_rm_action_t *)*pbody;
	rm_rep->category = DOT11_ACTION_CAT_RRM;
	rm_rep->action = DOT11_RM_ACTION_RM_REP;
	rm_rep->token = rrm_info->dialog_token;

	rmrep_ie = (dot11_rm_ie_t *)&rm_rep->data[0];
	rmrep_ie->id = DOT11_MNG_MEASURE_REPORT_ID;
	rmrep_ie->len = len - DOT11_RM_ACTION_LEN - TLV_HDR_LEN;

	return p;
}
#endif /* WL11K_BCN_MEAS || WL11K_NBR_MEAS || WL11K_ALL_MEAS */

#ifdef WL11K_ALL_MEAS
static void wlc_rrm_send_txstrmrep(wlc_rrm_info_t *rrm_info)
{
	wlc_info_t *wlc = rrm_info->wlc;
	dot11_rm_action_t *rm_rep;
	dot11_rm_ie_t *rmrep_ie;
	dot11_rmrep_tx_stream_t *rmrep_txstrm;
	void *p = NULL;
	uint8 *pbody;
	unsigned int len;
	char eabuf[ETHER_ADDR_STR_LEN];
	struct scb *scb;

	WL_RRM(("%s: <Enter>\n", __FUNCTION__));

	len = DOT11_RM_ACTION_LEN + DOT11_RM_IE_LEN + DOT11_RMREP_TXSTREAM_LEN;
	if ((p = wlc_rrm_prep_gen_report(rrm_info, len, &pbody)) == NULL)
		return;

	rm_rep = (dot11_rm_action_t *)pbody;
	rmrep_ie = (dot11_rm_ie_t *)&rm_rep->data[0];
	rmrep_ie->token = 0;
	rmrep_ie->mode = 0;
	rmrep_ie->type = DOT11_MEASURE_TYPE_TXSTREAM;

	rmrep_txstrm = (dot11_rmrep_tx_stream_t *)&rmrep_ie[1];
	bcopy(&rrm_info->txstrmreq->start_tsf_l, rmrep_txstrm->starttime, (2*sizeof(uint32)));
	rmrep_txstrm->duration = htol16(rrm_info->txstrmreq->duration);
	bcopy(&rrm_info->txstrmreq->peer, &rmrep_txstrm->peer, sizeof(struct ether_addr));
	rmrep_txstrm->traffic_id = rrm_info->txstrmreq->tid;
	rmrep_txstrm->reason = 0;

	wlc_rrm_txstrm_get_data(rrm_info, 1);

	rmrep_txstrm->txmsdu_cnt = htol32(rrm_info->txstrmreq->txmsdu_cnt);
	rmrep_txstrm->msdu_discarded_cnt = htol32(rrm_info->txstrmreq->msdu_discarded_cnt);
	rmrep_txstrm->msdufailed_cnt = htol32(rrm_info->txstrmreq->msdufailed_cnt);
	rmrep_txstrm->msduretry_cnt = htol32(rrm_info->txstrmreq->msduretry_cnt);
	rmrep_txstrm->cfpolls_lost_cnt = htol32(rrm_info->txstrmreq->cfpolls_lost_cnt);
	rmrep_txstrm->avrqueue_delay = htol32(rrm_info->txstrmreq->avrqueue_delay);
	rmrep_txstrm->avrtx_delay = htol32(rrm_info->txstrmreq->avrtx_delay);
	rmrep_txstrm->bin0_range = rrm_info->txstrmreq->bin0_range;
	rmrep_txstrm->bin0 = htol32(rrm_info->txstrmreq->bin0);
	rmrep_txstrm->bin1 = htol32(rrm_info->txstrmreq->bin1);
	rmrep_txstrm->bin2 = htol32(rrm_info->txstrmreq->bin2);
	rmrep_txstrm->bin3 = htol32(rrm_info->txstrmreq->bin3);
	rmrep_txstrm->bin4 = htol32(rrm_info->txstrmreq->bin4);
	rmrep_txstrm->bin5 = htol32(rrm_info->txstrmreq->bin5);

	bcm_ether_ntoa(&rmrep_txstrm->peer, eabuf);
	WL_RRM(("RRM-TXSTREAM_SENDREP %s: duration: %d, peer mac : %s, TID :%d, reason: %d\n",
		__FUNCTION__, rmrep_txstrm->duration, eabuf, rmrep_txstrm->traffic_id,
		rmrep_txstrm->reason));
	WL_RRM(("txmsdu: %d, msdu_discarded: %d, msdufailed: %d, msduretry: %d, "
		"cfpolls_lost: %d, avrqueue_delay: %d, avrtx_delay: %d \n",
		rmrep_txstrm->txmsdu_cnt, rmrep_txstrm->msdu_discarded_cnt,
		rmrep_txstrm->msdufailed_cnt, rmrep_txstrm->msduretry_cnt,
		rmrep_txstrm->cfpolls_lost_cnt, rmrep_txstrm->avrqueue_delay,
		rmrep_txstrm->avrtx_delay));
	WL_RRM(("bin0_range: %d, bins: [0] %d; [1] %d; [2] %d; [3] %d; [4] %d; [5] %d\n",
		rmrep_txstrm->bin0_range, rmrep_txstrm->bin0, rmrep_txstrm->bin1,
		rmrep_txstrm->bin2, rmrep_txstrm->bin3, rmrep_txstrm->bin4, rmrep_txstrm->bin5));

	scb = wlc_scbfind(wlc, rrm_info->cur_cfg, &rrm_info->da);
	wlc_sendmgmt(wlc, p, rrm_info->cur_cfg->wlcif->qi, scb);
}

static void wlc_rrm_send_statrep(wlc_rrm_info_t *rrm_info)
{
	wlc_info_t *wlc = rrm_info->wlc;
	dot11_rm_action_t *rm_rep;
	dot11_rm_ie_t *rmrep_ie;
	dot11_rmrep_stat_t *rmrep_stat;
	void *p = NULL;
	uint8 *pbody, *buf, *src;
	uint len;
	int group_data_len;
	struct scb *scb;

	WL_RRM(("%s: <Enter>\n", __FUNCTION__));

	if (rrm_info->statreq == NULL) {
		WL_ERROR(("%s: statreq is NULL\n", __FUNCTION__));
		return;
	}

	/* the len vary for diffferent group_id */
	if (rrm_info->statreq->group_id >= DOT11K_STA_STATS_MAX_GROUP) {
		WL_ERROR(("%s: wrong group_id %d\n",
			__FUNCTION__, rrm_info->statreq->group_id));
		return;
	}

	src = (uint8 *)&(rrm_info->statreq->sta_data);
	group_data_len = stat_group_length[rrm_info->statreq->group_id];

	len = DOT11_RM_ACTION_LEN + DOT11_RM_IE_LEN + sizeof(dot11_rmrep_stat_t) + group_data_len;
	if ((p = wlc_rrm_prep_gen_report(rrm_info, len, &pbody)) == NULL)
		return;

	rm_rep = (dot11_rm_action_t *)pbody;
	rmrep_ie = (dot11_rm_ie_t *)&rm_rep->data[0];
	rmrep_ie->token = rrm_info->statreq->token;
	rmrep_ie->mode = 0;
	rmrep_ie->type = DOT11_MEASURE_TYPE_STAT;

	rmrep_stat = (dot11_rmrep_stat_t *)&rmrep_ie[1];
	rmrep_stat->duration = htol16(rrm_info->statreq->duration);
	rmrep_stat->group_id = rrm_info->statreq->group_id;

	/* fill in content for different group */
	wlc_rrm_stat_get_group_data(rrm_info, 1);
	buf = (uint8 *)&rmrep_stat[1];
	memcpy(buf, src, group_data_len);

	WL_RRM(("%s: rmrep_ie=%p, rmrep_stat=%p, buf=%p, group_data_len=%d\n",
		__FUNCTION__, rmrep_ie, rmrep_stat, buf, group_data_len));

	WL_RRM_HEX("statrep pbody:", pbody, len);
	scb = wlc_scbfind(wlc, rrm_info->cur_cfg, &rrm_info->da);
	wlc_sendmgmt(wlc, p, rrm_info->cur_cfg->wlcif->qi, scb);
}

static void
wlc_rrm_send_framerep(wlc_rrm_info_t *rrm_info)
{
	wlc_info_t *wlc = rrm_info->wlc;
	dot11_rm_action_t *rm_rep;
	dot11_rm_ie_t *rmrep_ie;
	dot11_rmrep_frame_t *rmrep_frame;
	void *p = NULL;
	uint8 *pbody, *buf;
	uint len, buflen, frame_cnt = 1;
	struct scb *scb;

	WL_RRM(("%s: <Enter>\n", __FUNCTION__));

	if (rrm_info->framereq == NULL) {
		WL_ERROR(("%s: framereq is NULL\n", __FUNCTION__));
		return;
	}
	if ((p = wlc_rrm_prep_gen_report(rrm_info, ETHER_MAX_DATA, &pbody)) == NULL)
		return;

	rm_rep = (dot11_rm_action_t *)pbody;
	rmrep_ie = (dot11_rm_ie_t *)&rm_rep->data[0];
	rmrep_ie->len = DOT11_RM_IE_LEN - TLV_HDR_LEN; /* 5-2 */
	rmrep_ie->token = rrm_info->framereq->token;
	rmrep_ie->mode = 0;
	rmrep_ie->type = DOT11_MEASURE_TYPE_FRAME;

	rmrep_frame = (dot11_rmrep_frame_t *)&rmrep_ie[1];
	rmrep_frame->reg = rrm_info->framereq->reg;
	rmrep_frame->channel = rrm_info->framereq->channel;
	bcopy(&rrm_info->framereq->start_tsf_l, rmrep_frame->starttime, (2*sizeof(uint32)));
	rmrep_frame->duration = htol16(rrm_info->framereq->duration);

	buf = (uint8 *)&rmrep_frame[1];
	rmrep_ie->len += DOT11_RMREP_FRAME_LEN;
	buflen = ETHER_MAX_DATA -
		(DOT11_MGMT_HDR_LEN + DOT11_RM_ACTION_LEN + DOT11_RM_IE_LEN
			+ DOT11_RMREP_FRAME_LEN);

	len = wlc_rrm_framerep_add(rrm_info, buf, frame_cnt); /* 2 + n*19 */
	buflen -= len;
	rmrep_ie->len += len;

	if (p != NULL) {
		/* Fix up packet length */
		if (buflen > 0) {
			PKTSETLEN(wlc->osh, p, (ETHER_MAX_DATA - buflen));
		}
		scb = wlc_scbfind(wlc, rrm_info->cur_cfg, &rrm_info->da);
		wlc_sendmgmt(wlc, p, rrm_info->cur_cfg->wlcif->qi, scb);
	}
}

/*
 * ANPI: noise from phy_noise_avg()
 * IPI:
 * fomula: IPI Density Integer = (255 * DIPI) / ((1024 * DM) - TNAV - TTX - TRX)
 * Create a timer wlc_rrm_noise_ipi_timer every 20 ms to sample the noise by calling
 * wlc_rrm_noise_sample_request within the duration, then base on the formula to count
 * the noise density.
 */
static void
wlc_rrm_send_noiserep(wlc_rrm_info_t *rrm_info)
{
	wlc_info_t *wlc = rrm_info->wlc;
	dot11_rm_action_t *rm_rep;
	dot11_rm_ie_t *rmrep_ie;
	dot11_rmrep_noise_t *rmrep_noise;
	void *p = NULL;
	uint8 *pbody;
	unsigned int len;
	int i;
	uint8 *p_ini;
	struct scb *scb;

	WL_RRM(("%s: <Enter>\n", __FUNCTION__));

	if (rrm_info->noisereq == NULL) {
		WL_ERROR(("%s: noisereq is NULL\n", __FUNCTION__));
		return;
	}

	len = DOT11_RM_ACTION_LEN + DOT11_RM_IE_LEN + DOT11_RMREP_NOISE_LEN;
	if ((p = wlc_rrm_prep_gen_report(rrm_info, len, &pbody)) == NULL)
		return;

	rm_rep = (dot11_rm_action_t *)pbody;
	rmrep_ie = (dot11_rm_ie_t *)&rm_rep->data[0];
	rmrep_ie->token = rrm_info->noisereq->token;
	rmrep_ie->mode = 0;
	rmrep_ie->type = DOT11_MEASURE_TYPE_NOISE;

	rmrep_noise = (dot11_rmrep_noise_t *)&rmrep_ie[1];
	rmrep_noise->reg = rrm_info->noisereq->reg;
	rmrep_noise->channel = rrm_info->noisereq->channel;
	bcopy(&rrm_info->noisereq->start_tsf_l, rmrep_noise->starttime, (2*sizeof(uint32)));
	rmrep_noise->duration = htol16(rrm_info->noisereq->duration);

	wlc_stf_phy_txant_upd(wlc);
	rmrep_noise->antid = wlc->stf->phytxant;
	rmrep_noise->anpi = phy_noise_avg(WLC_PI(wlc));

	/* ipi0_dens ... ipi10_dens */
	p_ini = &(rmrep_noise->ipi0_dens);
	for (i = 0; i < WL_RRM_IPI_BINS_NUM; i++) {
		p_ini[i] = (uint8)rrm_info->noisereq->ipi_dens[i];
	}

	WL_RRM_HEX("noiserep pbody:", pbody, len);
	scb = wlc_scbfind(wlc, rrm_info->cur_cfg, &rrm_info->da);
	wlc_sendmgmt(wlc, p, rrm_info->cur_cfg->wlcif->qi, scb);
}

static void
wlc_rrm_send_chloadrep(wlc_rrm_info_t *rrm_info)
{
	wlc_info_t *wlc = rrm_info->wlc;
	dot11_rm_action_t *rm_rep;
	dot11_rm_ie_t *rmrep_ie;
	dot11_rmrep_chanload_t *rmrep_chload;
	void *p = NULL;
	uint8 *pbody;
	unsigned int len;
	wlc_rrm_req_state_t* rrm_state = rrm_info->rrm_state;
	struct scb *scb;

	WL_RRM(("%s: <Enter>\n", __FUNCTION__));

	len = DOT11_RM_ACTION_LEN + DOT11_RM_IE_LEN + DOT11_RMREP_CHANLOAD_LEN;
	if ((p = wlc_rrm_prep_gen_report(rrm_info, len, &pbody)) == NULL)
		return;

	rm_rep = (dot11_rm_action_t *)pbody;
	rmrep_ie = (dot11_rm_ie_t *)&rm_rep->data[0];
	rmrep_ie->token = 0;
	rmrep_ie->mode = 0;
	rmrep_ie->type = DOT11_MEASURE_TYPE_CHLOAD;

	rmrep_chload = (dot11_rmrep_chanload_t *)&rmrep_ie[1];

	rmrep_chload->reg = rrm_info->chloadreq->reg;
	rmrep_chload->channel = CHSPEC_CHANNEL(wlc->chanspec);
	rmrep_chload->channel_load = rrm_state->cca_busy;
	bcopy(&rrm_info->chloadreq->start_tsf_l, rmrep_chload->starttime, (2*sizeof(uint32)));
	rmrep_chload->duration = htol16(rrm_info->chloadreq->duration);

	WL_RRM(("RRM-CHLOAD_SENDREP %s: regulatory: %d, channel: %d, "
		"actualStartTime 0x%08x %08x duration: %d, chload: %d\n",
		__FUNCTION__, rmrep_chload->reg, rmrep_chload->channel, rmrep_chload->starttime[0],
		rmrep_chload->starttime[1], rmrep_chload->duration, rmrep_chload->channel_load));

	scb = wlc_scbfind(wlc, rrm_info->cur_cfg, &rrm_info->da);
	wlc_sendmgmt(wlc, p, rrm_info->cur_cfg->wlcif->qi, scb);
}

/* FIXME: PR81518: need a wlc_bsscfg_t param to correctly determine addresses and
 * queue for wlc_sendmgmt()
 */

static void
wlc_rrm_send_lcirep(wlc_rrm_info_t *rrm_info, uint8 token)
{
	wlc_info_t *wlc = rrm_info->wlc;
	dot11_rm_action_t *rm_rep;
	dot11_rm_ie_t *rmrep_ie;
	uint8 *rmrep_lci;
	void *p = NULL;
	uint8 *pbody;
	unsigned int len;
	uint8 lci_len = DOT11_FTM_LCI_UNKNOWN_LEN;
	uint8 *lci_data = NULL;
	rrm_bsscfg_cubby_t *rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, rrm_info->cur_cfg);
	dot11_meas_rep_t *lci_rep;
	struct scb *scb;

	if (rrm_cfg->nbr_rep_self &&
		rrm_cfg->nbr_rep_self->opt_lci) {
		lci_rep = (dot11_meas_rep_t *)
			rrm_cfg->nbr_rep_self->opt_lci;

		if (lci_rep->len > DOT11_MNG_IE_MREP_FIXED_LEN) {
			lci_len = lci_rep->len - DOT11_MNG_IE_MREP_FIXED_LEN;
			lci_data = &lci_rep->rep.data[0];
		}
	}

	len = DOT11_RM_ACTION_LEN + DOT11_RM_IE_LEN + lci_len;
	if ((p = wlc_rrm_prep_gen_report(rrm_info, len, &pbody)) == NULL)
		return;

	rm_rep = (dot11_rm_action_t *)pbody;
	rmrep_ie = (dot11_rm_ie_t *)&rm_rep->data[0];
	rmrep_ie->token = token;
	rmrep_ie->mode = 0;
	rmrep_ie->type = DOT11_MEASURE_TYPE_LCI;

	rmrep_lci = (uint8 *)&rmrep_ie[1];

	if (lci_data) {
		memcpy(rmrep_lci, lci_data, lci_len);
	} else {
		/* set output to unknown LCI */
		memset(rmrep_lci, 0, DOT11_FTM_LCI_UNKNOWN_LEN);
	}

	scb = wlc_scbfind(wlc, rrm_info->cur_cfg, &rrm_info->da);
	wlc_sendmgmt(wlc, p, rrm_info->cur_cfg->wlcif->qi, scb);
}

static void
wlc_rrm_send_civiclocrep(wlc_rrm_info_t *rrm_info, uint8 token)
{
	wlc_info_t *wlc = rrm_info->wlc;
	dot11_rm_action_t *rm_rep;
	dot11_rm_ie_t *rmrep_ie;
	uint8 *rmrep_civic;
	void *p = NULL;
	uint8 *pbody;
	unsigned int len;
	uint8 civic_len = DOT11_FTM_CIVIC_UNKNOWN_LEN;
	uint8 *civic_data = NULL;
	rrm_bsscfg_cubby_t *rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, rrm_info->cur_cfg);
	dot11_meas_rep_t *civic_rep;
	struct scb *scb;

	if (rrm_cfg->nbr_rep_self &&
		rrm_cfg->nbr_rep_self->opt_civic) {
		civic_rep = (dot11_meas_rep_t *)
			rrm_cfg->nbr_rep_self->opt_civic;

		if (civic_rep->len > DOT11_MNG_IE_MREP_FIXED_LEN) {
			civic_len = civic_rep->len - DOT11_MNG_IE_MREP_FIXED_LEN;
			civic_data = &civic_rep->rep.data[0];
		}
	}

	len = DOT11_RM_ACTION_LEN + DOT11_RM_IE_LEN + civic_len;
	if ((p = wlc_rrm_prep_gen_report(rrm_info, len, &pbody)) == NULL)
		return;

	rm_rep = (dot11_rm_action_t *)pbody;
	rmrep_ie = (dot11_rm_ie_t *)&rm_rep->data[0];
	rmrep_ie->token = token;
	rmrep_ie->mode = 0;
	rmrep_ie->type = DOT11_MEASURE_TYPE_CIVICLOC;

	rmrep_civic = (uint8 *)&rmrep_ie[1];

	if (civic_data) {
		memcpy(rmrep_civic, civic_data, civic_len);
	} else {
		/* set output to unknown civic */
		memset(rmrep_civic, 0, DOT11_FTM_CIVIC_UNKNOWN_LEN);
	}

	scb = wlc_scbfind(wlc, rrm_info->cur_cfg, &rrm_info->da);
	wlc_sendmgmt(wlc, p, rrm_info->cur_cfg->wlcif->qi, scb);
}
static void wlc_rrm_send_locidrep(wlc_rrm_info_t *rrm_info, uint8 token)
{
	wlc_info_t *wlc = rrm_info->wlc;
	dot11_rm_action_t *rm_rep;
	dot11_rm_ie_t *rmrep_ie;
	uint8 *rmrep_locid;
	void *p = NULL;
	uint8 *pbody;
	unsigned int len;
	uint8 locid_len = 0;
	uint8 *locid_data = NULL;
	rrm_bsscfg_cubby_t *rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, rrm_info->cur_cfg);
	dot11_meas_rep_t *locid_rep;
	struct scb *scb;

	if (rrm_cfg->nbr_rep_self &&
		rrm_cfg->nbr_rep_self->opt_locid) {
		locid_rep = (dot11_meas_rep_t *)
			rrm_cfg->nbr_rep_self->opt_locid;
		locid_data = &locid_rep->rep.data[0];
		locid_len = locid_rep->len - DOT11_MNG_IE_MREP_FIXED_LEN;
	} else {
		/* unknown locid */
		locid_len = DOT11_LOCID_UNKNOWN_LEN;
	}

	len = DOT11_RM_ACTION_LEN + DOT11_RM_IE_LEN + locid_len;
	if ((p = wlc_rrm_prep_gen_report(rrm_info, len, &pbody)) == NULL)
		return;

	rm_rep = (dot11_rm_action_t *)pbody;
	rmrep_ie = (dot11_rm_ie_t *)&rm_rep->data[0];
	rmrep_ie->token = token;
	rmrep_ie->mode = 0;
	rmrep_ie->type = DOT11_MEASURE_TYPE_LOC_ID;

	rmrep_locid = (uint8 *)&rmrep_ie[1];
	if (locid_data) {
		memcpy(rmrep_locid, locid_data, locid_len);
	} else {
		/* set output to unknown locid */
		memset(rmrep_locid, 0, DOT11_LOCID_UNKNOWN_LEN);
	}
	scb = wlc_scbfind(wlc, rrm_info->cur_cfg, &rrm_info->da);
	wlc_sendmgmt(wlc, p, rrm_info->cur_cfg->wlcif->qi, scb);
}

/*
 * RCPI: use RSSI, for average: use wlc_rrm_framereq_measure() when state changed
 * RSNI: (rssi - noise_avg)
 */
static int
wlc_rrm_framerep_add(wlc_rrm_info_t *rrm_info, uint8 *bufptr, uint cnt)
{
	uint i;
	bcm_tlv_t *frm_body_tlv;
	dot11_rmrep_frmentry_t *frm_e;
	wlc_info_t *wlc;
	uint32 frame_count;
	int32 noise_avg;
	wlc_bsscfg_t *bsscfg;

	if (rrm_info == NULL || bufptr == NULL) {
		WL_ERROR(("%s: rrm_info=%p, bufptr=%p\n", __FUNCTION__, rrm_info, bufptr));
		return 0;
	}

	bsscfg = rrm_info->cur_cfg;
	if (bsscfg == NULL) {
		WL_ERROR(("%s: bsscfg=NULL\n", __FUNCTION__));
		return 0;
	}

	wlc = rrm_info->wlc;

	frm_body_tlv = (bcm_tlv_t *)bufptr;
	frm_body_tlv->id = DOT11_RMREP_FRAME_COUNT_REPORT;
	frm_body_tlv->len = cnt * DOT11_RMREP_FRMENTRY_LEN;

	frm_e = (dot11_rmrep_frmentry_t *)(bufptr+2);

	wlc_stf_phy_txant_upd(wlc);
	noise_avg = phy_noise_avg(WLC_PI(wlc));

	WL_RRM(("%s: rssi=%d, snr=%d,noise_avg=%d, lq_noise=%d, "
		"phytxant=%d, txant=%d\n",
		__FUNCTION__, bsscfg->link->rssi, wlc->cfg->link->snr,
		noise_avg, wlc_lq_noise_ma_upd(wlc, noise_avg),
		wlc->stf->phytxant, wlc->stf->txant));

	for (i = 0; i < cnt; i++) {
		bcopy(&rrm_info->cur_cfg->cur_etheraddr, &frm_e->ta, sizeof(struct ether_addr));
		bcopy(&rrm_info->cur_cfg->BSSID, &frm_e->bssid, sizeof(struct ether_addr));
		frm_e->phy_type = wlc->band->phytype;
		frm_e->avg_rcpi = (uint8)(rrm_info->framereq->avg_rcpi);  /* use rssi 32To8 bit */
		frm_e->last_rsni = (uint8)(bsscfg->link->rssi - noise_avg); /* use snr */
		frm_e->last_rcpi = bsscfg->link->rssi;
		frm_e->ant_id = wlc->stf->phytxant;
		frame_count = wlc->pub->_cnt->rxframe - rrm_info->framereq->rxframe_begin;
		if (frame_count > 65535)
			frm_e->frame_cnt = 65535;
		else
			frm_e->frame_cnt = (uint16)frame_count;

		WL_RRM(("%s: phy_type=%d, avg_rcpi=%d, last_rsni=%d, last_rcpi=%d, "
			"ant_id=%d, frame_cnt=%d, rxframe=%d\n",
			__FUNCTION__, frm_e->phy_type, (int8)frm_e->avg_rcpi,
			(int8)frm_e->last_rsni, (int8)frm_e->last_rcpi, frm_e->ant_id,
			frm_e->frame_cnt, wlc->pub->_cnt->rxframe));

		frm_e++;
	}
	return frm_body_tlv->len + TLV_HDR_LEN;
}

static void
wlc_rrm_send_lmreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, struct ether_addr *da)
{
	void *p;
	uint8 *pbody;
	int buflen;
	dot11_lmreq_t *lmreq;
	struct scb *scb;

	wlc_info_t *wlc = rrm_info->wlc;

	/* lm request header */
	buflen = DOT11_LMREQ_LEN;

	if ((p = wlc_frame_get_action(wlc, da,
		&cfg->cur_etheraddr, &cfg->BSSID, buflen, &pbody, DOT11_ACTION_CAT_RRM)) == NULL) {
		return;
	}
	lmreq = (dot11_lmreq_t *)pbody;
	lmreq->category = DOT11_ACTION_CAT_RRM;
	lmreq->action = DOT11_RM_ACTION_LM_REQ;
	WLC_RRM_UPDATE_TOKEN(rrm_info->req_token);
	lmreq->token = rrm_info->req_token;
	lmreq->txpwr = 0;
	lmreq->maxtxpwr = 0;

	scb = wlc_scbfind(wlc, cfg, da);
	wlc_sendmgmt(wlc, p, cfg->wlcif->qi, scb);
}

static void
wlc_rrm_send_statreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, statreq_t *statreq)
{
	void *p;
	uint8 *pbody;
	int buflen;
	dot11_rmreq_t *rmreq;
	dot11_rmreq_stat_t *sreq;
	struct ether_addr *da;
	struct scb *scb;

	wlc_info_t *wlc = rrm_info->wlc;
	da = &statreq->da;

	/* rm frame action header + STAT request header */
	buflen = DOT11_RMREQ_LEN + DOT11_RMREQ_STAT_LEN;
	if ((p = wlc_rrm_prep_gen_request(rrm_info, cfg, da, buflen, &pbody)) == NULL)
		return;

	rmreq = (dot11_rmreq_t *)pbody;
	rmreq->reps = statreq->reps;

	sreq = (dot11_rmreq_stat_t *)&rmreq->data[0];
	sreq->id = DOT11_MNG_MEASURE_REQUEST_ID;
	sreq->len = DOT11_RMREQ_STAT_LEN - TLV_HDR_LEN;
	WLC_RRM_UPDATE_TOKEN(rrm_info->req_elt_token);
	sreq->token = rrm_info->req_elt_token;
	sreq->mode = 0;
	sreq->type = DOT11_MEASURE_TYPE_STAT;
	sreq->interval = statreq->random_int;
	sreq->duration = statreq->dur;
	sreq->group_id = statreq->group_id;
	bcopy(&statreq->peer, &sreq->peer, sizeof(struct ether_addr));

	scb = wlc_scbfind(wlc, cfg, da);
	wlc_sendmgmt(wlc, p, cfg->wlcif->qi, scb);
}

static void
wlc_rrm_send_framereq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, framereq_t *framereq)
{
	void *p;
	uint8 *pbody;
	int buflen;
	dot11_rmreq_t *rmreq;
	dot11_rmreq_frame_t *freq;
	struct ether_addr *da;
	struct scb *scb;

	wlc_info_t *wlc = rrm_info->wlc;
	da = &framereq->da;

	WL_RRM(("%s: <Enter>\n", __FUNCTION__));

	/* rm frame action header + frame request header */
	buflen = DOT11_RMREQ_LEN + DOT11_RMREQ_FRAME_LEN;
	if ((p = wlc_rrm_prep_gen_request(rrm_info, cfg, da, buflen, &pbody)) == NULL)
		return;

	rmreq = (dot11_rmreq_t *)pbody;
	rmreq->reps = framereq->reps;

	freq = (dot11_rmreq_frame_t *)&rmreq->data[0];
	freq->id = DOT11_MNG_MEASURE_REQUEST_ID;
	freq->len = DOT11_RMREQ_FRAME_LEN - TLV_HDR_LEN;
	freq->token = 0;
	freq->mode = 0;
	freq->type = DOT11_MEASURE_TYPE_FRAME;
	freq->reg = framereq->reg;
	freq->channel = framereq->chan;
	freq->interval = framereq->random_int;
	freq->duration = framereq->dur;
	/* Frame Count Report is requested */
	freq->req_type = 1;
	bcopy(&framereq->ta, &freq->ta, sizeof(struct ether_addr));

	scb = wlc_scbfind(wlc, cfg, da);
	wlc_sendmgmt(wlc, p, cfg->wlcif->qi, scb);
}

static void
wlc_rrm_send_noisereq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, rrmreq_t *noisereq)
{
	void *p;
	uint8 *pbody;
	int buflen;
	dot11_rmreq_t *rmreq;
	dot11_rmreq_noise_t *nreq;
	struct ether_addr *da;
	struct scb *scb;

	wlc_info_t *wlc = rrm_info->wlc;
	da = &noisereq->da;

	/* rm frame action header + noise request header */
	buflen = DOT11_RMREQ_LEN + DOT11_RMREQ_NOISE_LEN;
	if ((p = wlc_rrm_prep_gen_request(rrm_info, cfg, da, buflen, &pbody)) == NULL)
		return;

	rmreq = (dot11_rmreq_t *)pbody;
	rmreq->reps = noisereq->reps;

	nreq = (dot11_rmreq_noise_t *)&rmreq->data[0];
	nreq->id = DOT11_MNG_MEASURE_REQUEST_ID;
	nreq->len = DOT11_RMREQ_NOISE_LEN - TLV_HDR_LEN;
	WLC_RRM_UPDATE_TOKEN(rrm_info->req_elt_token);
	nreq->token = rrm_info->req_elt_token;
	nreq->mode = 0;
	nreq->type = DOT11_MEASURE_TYPE_NOISE;
	nreq->reg = noisereq->reg;
	nreq->channel = noisereq->chan;
	nreq->interval = noisereq->random_int;
	nreq->duration = noisereq->dur;

	scb = wlc_scbfind(wlc, cfg, da);
	wlc_sendmgmt(wlc, p, cfg->wlcif->qi, scb);
}

static void
wlc_rrm_send_chloadreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, rrmreq_t *chloadreq)
{
	void *p;
	uint8 *pbody;
	int buflen;
	dot11_rmreq_t *rmreq;
	dot11_rmreq_chanload_t *chreq;
	struct ether_addr *da;
	struct scb *scb;

	wlc_info_t *wlc = rrm_info->wlc;
	da = &chloadreq->da;

	/* rm frame action header + channel load request header */
	buflen = DOT11_RMREQ_LEN + DOT11_RMREQ_CHANLOAD_LEN;
	if ((p = wlc_rrm_prep_gen_request(rrm_info, cfg, da, buflen, &pbody)) == NULL)
		return;

	rmreq = (dot11_rmreq_t *)pbody;
	rmreq->reps = chloadreq->reps;

	chreq = (dot11_rmreq_chanload_t *)&rmreq->data[0];
	chreq->id = DOT11_MNG_MEASURE_REQUEST_ID;
	chreq->len = DOT11_RMREQ_CHANLOAD_LEN - TLV_HDR_LEN;
	WLC_RRM_UPDATE_TOKEN(rrm_info->req_elt_token);
	chreq->token = rrm_info->req_elt_token;
	chreq->mode = 0;
	chreq->type = DOT11_MEASURE_TYPE_CHLOAD;
	if (chloadreq->reg == 0) {
		chreq->reg = wlc_get_regclass(wlc->cmi, cfg->current_bss->chanspec);
	}
	else {
		chreq->reg = chloadreq->reg;
	}
	chreq->channel = chloadreq->chan;
	chreq->interval = chloadreq->random_int;
	chreq->duration = chloadreq->dur;

	scb = wlc_scbfind(wlc, cfg, da);
	wlc_sendmgmt(wlc, p, cfg->wlcif->qi, scb);
}

static void
wlc_rrm_send_txstrmreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, txstrmreq_t *txstrmreq)
{
	void *p;
	uint8 *pbody;
	int buflen;
	dot11_rmreq_t *rmreq;
	dot11_rmreq_tx_stream_t *rmreq_txstrm;
	struct ether_addr *da;
	struct scb *scb;

	wlc_info_t *wlc = rrm_info->wlc;
	da = &txstrmreq->da;

	/* rm frame action header + channel load request header */
	buflen = DOT11_RMREQ_LEN + DOT11_RMREQ_TXSTREAM_LEN;
	if ((p = wlc_rrm_prep_gen_request(rrm_info, cfg, da, buflen, &pbody)) == NULL)
		return;

	rmreq = (dot11_rmreq_t *)pbody;
	rmreq->reps = txstrmreq->reps;

	rmreq_txstrm = (dot11_rmreq_tx_stream_t *)&rmreq->data[0];
	rmreq_txstrm->id = DOT11_MNG_MEASURE_REQUEST_ID;
	rmreq_txstrm->len = DOT11_RMREQ_TXSTREAM_LEN - TLV_HDR_LEN;
	WLC_RRM_UPDATE_TOKEN(rrm_info->req_elt_token);
	rmreq_txstrm->token = rrm_info->req_elt_token;
	rmreq_txstrm->mode = 0;
	rmreq_txstrm->type = DOT11_MEASURE_TYPE_TXSTREAM;
	rmreq_txstrm->interval = txstrmreq->random_int;
	rmreq_txstrm->duration = txstrmreq->dur;
	bcopy(&txstrmreq->peer, &rmreq_txstrm->peer, sizeof(struct ether_addr));
	rmreq_txstrm->traffic_id = txstrmreq->tid;
	rmreq_txstrm->bin0_range = txstrmreq->bin0_range;

	scb = wlc_scbfind(wlc, cfg, da);
	wlc_sendmgmt(wlc, p, cfg->wlcif->qi, scb);
}

#ifdef WL_PROXDETECT
static int
wlc_rrm_send_lcireq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, lci_req_t *lcireq)
{
	int err = BCME_OK;
	void *p;
	uint8 *pbody;
	int buflen;
	dot11_rmreq_t *rmreq;
	dot11_meas_req_loc_t *locreq;
	struct ether_addr *da;
	wlc_info_t *wlc = rrm_info->wlc;
	rrm_bsscfg_cubby_t *rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, cfg);
	uint8 *ptr;
	dot11_ftm_range_subel_t *age_range_subel;
	struct scb *scb;

	da = &lcireq->da;

	if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_LCIM))
		return BCME_UNSUPPORTED;

	if (lcireq->subject == DOT11_FTM_LOCATION_SUBJ_THIRDPARTY)
		return BCME_UNSUPPORTED;

	/* rm frame action header + lci request header */
	buflen = DOT11_RMREQ_LEN + DOT11_MNG_IE_MREQ_LCI_FIXED_LEN + TLV_HDR_LEN;

	if (lcireq->max_age) {
		buflen += sizeof(dot11_ftm_range_subel_t);
	}

	if ((p = wlc_rrm_prep_gen_request(rrm_info, cfg, da, buflen, &pbody)) == NULL)
		return BCME_ERROR;

	rmreq = (dot11_rmreq_t *)pbody;
	rmreq->category = DOT11_ACTION_CAT_RRM;
	rmreq->action = DOT11_RM_ACTION_RM_REQ;
	WLC_RRM_UPDATE_TOKEN(rrm_info->req_token);
	rmreq->token = rrm_info->req_token;
	rmreq->reps = 0;

	locreq = (dot11_meas_req_loc_t *)&rmreq->data[0];
	locreq->id = DOT11_MNG_MEASURE_REQUEST_ID;
	locreq->len = DOT11_MNG_IE_MREQ_LCI_FIXED_LEN;
	WLC_RRM_UPDATE_TOKEN(rrm_info->req_elt_token);
	locreq->token = rrm_info->req_elt_token;
	locreq->mode = 0;
	locreq->type = DOT11_MEASURE_TYPE_LCI;
	locreq->req.lci.subject = lcireq->subject;

	if (lcireq->max_age) {
		ptr = (uint8 *)locreq;
		ptr += DOT11_MNG_IE_MREQ_LCI_FIXED_LEN + TLV_HDR_LEN;
		age_range_subel = (dot11_ftm_range_subel_t *) ptr;

		age_range_subel->id = DOT11_FTM_RANGE_SUBELEM_ID;
		age_range_subel->len = DOT11_FTM_RANGE_SUBELEM_LEN;
		store16_ua((uint8 *)&age_range_subel->max_age, lcireq->max_age);
		ptr += sizeof(*age_range_subel);
		locreq->len += DOT11_FTM_RANGE_SUBELEM_LEN + TLV_HDR_LEN;
	}

	scb = wlc_scbfind(wlc, cfg, &lcireq->da);
	wlc_sendmgmt(wlc, p, cfg->wlcif->qi, scb);

	return err;
}

static int
wlc_rrm_send_civicreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, civic_req_t *civicreq)
{
	int err = BCME_OK;
	void *p;
	uint8 *pbody;
	int buflen;
	dot11_rmreq_t *rmreq;
	dot11_meas_req_loc_t *locreq;
	struct ether_addr *da;
	struct scb *scb;
	wlc_info_t *wlc = rrm_info->wlc;
	rrm_bsscfg_cubby_t *rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, cfg);

	da = &civicreq->da;

	if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_CIVIC_LOC))
		return BCME_UNSUPPORTED;

	if (civicreq->subject == DOT11_FTM_LOCATION_SUBJ_THIRDPARTY)
		return BCME_UNSUPPORTED;

	/* rm frame action header + civic request header */
	buflen = DOT11_RMREQ_LEN + DOT11_MNG_IE_MREQ_CIVIC_FIXED_LEN + TLV_HDR_LEN;

	if ((p = wlc_rrm_prep_gen_request(rrm_info, cfg, da, buflen, &pbody)) == NULL)
		return BCME_ERROR;

	rmreq = (dot11_rmreq_t *)pbody;
	rmreq->category = DOT11_ACTION_CAT_RRM;
	rmreq->action = DOT11_RM_ACTION_RM_REQ;
	WLC_RRM_UPDATE_TOKEN(rrm_info->req_token);
	rmreq->token = rrm_info->req_token;
	rmreq->reps = 0;

	locreq = (dot11_meas_req_loc_t *)&rmreq->data[0];
	locreq->id = DOT11_MNG_MEASURE_REQUEST_ID;
	locreq->len = DOT11_MNG_IE_MREQ_CIVIC_FIXED_LEN;
	WLC_RRM_UPDATE_TOKEN(rrm_info->req_elt_token);
	locreq->token = rrm_info->req_elt_token;
	locreq->mode = 0;
	locreq->type = DOT11_MEASURE_TYPE_CIVICLOC;
	locreq->req.civic.subject = civicreq->subject;
	locreq->req.civic.type = civicreq->type;
	locreq->req.civic.siu = civicreq->si_units;
	locreq->req.civic.si = civicreq->service_interval;

	scb = wlc_scbfind(wlc, cfg, &civicreq->da);
	wlc_sendmgmt(wlc, p, cfg->wlcif->qi, scb);

	return err;
}

#else
static void
wlc_rrm_send_lcireq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, lcireq_t *lcireq)
{
	void *p;
	uint8 *pbody;
	int buflen;
	dot11_rmreq_t *rmreq;
	dot11_rmreq_ftm_lci_t *rmreq_lci;
	struct ether_addr *da;
	struct scb *scb;

	wlc_info_t *wlc = rrm_info->wlc;
	da = &lcireq->da;

	/* rm frame action header + channel load request header */
	buflen = DOT11_RMREQ_LEN + DOT11_RMREQ_LCI_LEN;
	if ((p = wlc_rrm_prep_gen_request(rrm_info, cfg, da, buflen, &pbody)) == NULL)
		return;

	rmreq = (dot11_rmreq_t *)pbody;
	rmreq->reps = lcireq->reps;

	rmreq_lci = (dot11_rmreq_ftm_lci_t *)&rmreq->data[0];
	rmreq_lci->id = DOT11_MNG_MEASURE_REQUEST_ID;
	rmreq_lci->len = DOT11_RMREQ_LCI_LEN - TLV_HDR_LEN;
	WLC_RRM_UPDATE_TOKEN(rrm_info->req_elt_token);
	rmreq_lci->token = rrm_info->req_elt_token;
	rmreq_lci->mode = 0;
	rmreq_lci->type = DOT11_MEASURE_TYPE_LCI;
	rmreq_lci->subj = lcireq->subj;
	rmreq_lci->lat_res = lcireq->lat_res;
	rmreq_lci->lon_res = lcireq->lon_res;
	rmreq_lci->alt_res = lcireq->alt_res;

	scb = wlc_scbfind(wlc, cfg, da);
	wlc_sendmgmt(wlc, p, cfg->wlcif->qi, scb);
}

static void
wlc_rrm_send_civicreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, civicreq_t *civicreq)
{
	void *p;
	uint8 *pbody;
	int buflen;
	dot11_rmreq_t *rmreq;
	dot11_rmreq_ftm_civic_t *rmreq_civic;
	char eabuf[ETHER_ADDR_STR_LEN];
	struct ether_addr *da;
	struct scb *scb;

	wlc_info_t *wlc = rrm_info->wlc;
	da = &civicreq->da;

	bcm_ether_ntoa(da, eabuf);
	WL_RRM(("%s: da: %s reps:%d subj:%d civloc_type:%d siu:%d si: %d\n",
		__FUNCTION__, eabuf, civicreq->reps, civicreq->subj,
		civicreq->civloc_type, civicreq->siu, civicreq->si));

	/* rm frame action header + channel load request header */
	buflen = DOT11_RMREQ_LEN + DOT11_RMREQ_CIVIC_LEN;
	if ((p = wlc_rrm_prep_gen_request(rrm_info, cfg, da, buflen, &pbody)) == NULL)
		return;

	rmreq = (dot11_rmreq_t *)pbody;
	rmreq->reps = civicreq->reps;

	rmreq_civic = (dot11_rmreq_ftm_civic_t *)&rmreq->data[0];
	rmreq_civic->id = DOT11_MNG_MEASURE_REQUEST_ID;
	rmreq_civic->len = DOT11_RMREQ_CIVIC_LEN - TLV_HDR_LEN;
	WLC_RRM_UPDATE_TOKEN(rrm_info->req_elt_token);
	rmreq_civic->token = rrm_info->req_elt_token;
	rmreq_civic->mode = 0;
	rmreq_civic->type = DOT11_MEASURE_TYPE_CIVICLOC;
	rmreq_civic->subj = civicreq->subj;
	rmreq_civic->civloc_type = civicreq->civloc_type;
	rmreq_civic->siu = civicreq->siu;
	rmreq_civic->si = civicreq->si;

	scb = wlc_scbfind(wlc, cfg, da);
	wlc_sendmgmt(wlc, p, cfg->wlcif->qi, scb);
}
#endif /* WL_PROXDETECT */

static void
wlc_rrm_send_locidreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, locidreq_t *locidreq)
{
	void *p;
	uint8 *pbody;
	int buflen;
	dot11_rmreq_t *rmreq;
	dot11_rmreq_locid_t *rmreq_locid;
	struct ether_addr *da;
	struct scb *scb;

	wlc_info_t *wlc = rrm_info->wlc;
	da = &locidreq->da;

	/* rm frame action header + channel load request header */
	buflen = DOT11_RMREQ_LEN + DOT11_RMREQ_LOCID_LEN;
	if ((p = wlc_rrm_prep_gen_request(rrm_info, cfg, da, buflen, &pbody)) == NULL)
		return;

	rmreq = (dot11_rmreq_t *)pbody;
	rmreq->reps = locidreq->reps;

	rmreq_locid = (dot11_rmreq_locid_t *)&rmreq->data[0];
	rmreq_locid->id = DOT11_MNG_MEASURE_REQUEST_ID;
	rmreq_locid->len = DOT11_RMREQ_LOCID_LEN - TLV_HDR_LEN;
	WLC_RRM_UPDATE_TOKEN(rrm_info->req_elt_token);
	rmreq_locid->token = rrm_info->req_elt_token;
	rmreq_locid->mode = 0;
	rmreq_locid->type = DOT11_MEASURE_TYPE_LOC_ID;
	rmreq_locid->subj = locidreq->subj;
	rmreq_locid->siu = locidreq->siu;
	rmreq_locid->si = locidreq->si;

	scb = wlc_scbfind(wlc, cfg, da);
	wlc_sendmgmt(wlc, p, cfg->wlcif->qi, scb);
}
#endif /* WL11K_ALL_MEAS */

#if (defined(WL11K_AP) && defined(WL11K_NBR_MEAS)) || (defined(WL_FTM_11K) && \
	defined(WL_PROXDETECT) && defined(WL_FTM) && defined(WL11AC))
static void
wlc_rrm_create_nbrrep_element(rrm_bsscfg_cubby_t *rrm_cfg, uint8 **ptr,
	rrm_nbr_rep_t *nbr_rep, uint8 flags, uint8 *bufend)
{
	dot11_neighbor_rep_ie_t *nbrrep;
	dot11_ngbr_bsstrans_pref_se_t *pref;
	int buflen = MIN(TLV_BODY_LEN_MAX, BUFLEN(*ptr, bufend));
	uint8 *nrrep_start = *ptr;

	nbrrep = (dot11_neighbor_rep_ie_t *) *ptr;

	nbrrep->id = nbr_rep->nbr_elt.id;
	nbrrep->len = DOT11_NEIGHBOR_REP_IE_FIXED_LEN;

	memcpy(&nbrrep->bssid, &nbr_rep->nbr_elt.bssid, ETHER_ADDR_LEN);

	store32_ua((uint8 *)&nbrrep->bssid_info, nbr_rep->nbr_elt.bssid_info);

	nbrrep->reg = nbr_rep->nbr_elt.reg;
	nbrrep->channel = nbr_rep->nbr_elt.channel;
	nbrrep->phytype = nbr_rep->nbr_elt.phytype;

	/* Check for optional elements */
	*ptr += TLV_HDR_LEN + DOT11_NEIGHBOR_REP_IE_FIXED_LEN;
	buflen -= TLV_HDR_LEN + DOT11_NEIGHBOR_REP_IE_FIXED_LEN;

	if (flags & RRM_NREQ_FLAGS_BSS_PREF && nbr_rep->nbr_elt.bss_trans_preference) {
		/* Add preference subelement */
		pref = (dot11_ngbr_bsstrans_pref_se_t*) *ptr;
		pref->sub_id = DOT11_NGBR_BSSTRANS_PREF_SE_ID;
		pref->len = DOT11_NGBR_BSSTRANS_PREF_SE_LEN;
		pref->preference = nbr_rep->nbr_elt.bss_trans_preference;
		*ptr += TLV_HDR_LEN + DOT11_NGBR_BSSTRANS_PREF_SE_LEN;
		buflen -= TLV_HDR_LEN + DOT11_NGBR_BSSTRANS_PREF_SE_LEN;
	}

	if (wf_chspec_valid(nbr_rep->nbr_elt.chanspec)) {
		uint8 *wide_bw_ch;
		buflen = MIN(buflen, BUFLEN(*ptr, bufend));
		wide_bw_ch = wlc_write_wide_bw_chan_ie(nbr_rep->nbr_elt.chanspec,
			*ptr, buflen);
		buflen -= (wide_bw_ch - *ptr);
		*ptr = wide_bw_ch;
	}
#ifdef WL_FTM_11K
	if (flags & RRM_NREQ_FLAGS_LCI_PRESENT) {
		buflen = MIN(buflen, BUFLEN(*ptr, bufend));
		*ptr = wlc_rrm_create_lci_meas_element(*ptr, rrm_cfg, nbr_rep,
				&buflen, flags);
	}

	if (flags & RRM_NREQ_FLAGS_CIVICLOC_PRESENT) {
		buflen = MIN(buflen, BUFLEN(*ptr, bufend));
		*ptr = wlc_rrm_create_civic_meas_element(*ptr, rrm_cfg, nbr_rep,
				&buflen, flags);
	}
#endif /* WL_FTM_11K */
	/* update neighbor report length */
	nbrrep->len = (*ptr - nrrep_start) - TLV_HDR_LEN;
}
#endif /* (WL11K_AP && WL11K_NBR_MEAS) || (WL_FTM_11K && WL_PROXDETECT && WL_FTM && WL11AC) */

#ifdef WL11K_NBR_MEAS
#ifdef STA
/* FIXME: PR81518: need a wlc_bsscfg_t param to correctly determine addresses and
 * queue for wlc_sendmgmt()
 */
static void
wlc_rrm_send_nbrreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, wlc_ssid_t *ssid)
{
	void *p;
	uint8 *pbody;
	int buflen;
	dot11_rm_action_t *rmreq;
	wlc_info_t *wlc = rrm_info->wlc;
	bcm_tlv_t *ssid_ie;
	rrm_bsscfg_cubby_t *rrm_cfg;
	uint8 *req_data_ptr;
	dot11_meas_req_loc_t *ie;
	struct scb *scb;

	rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, cfg);
	ASSERT(rrm_cfg != NULL);

	/* rm frame action header + optional ssid ie */
	buflen = DOT11_RM_ACTION_LEN;

	if (ssid)
		buflen += ssid->SSID_len + TLV_HDR_LEN;

	if (isset(cfg->ext_cap, DOT11_EXT_CAP_FTM_RESPONDER) ||
		isset(cfg->ext_cap, DOT11_EXT_CAP_FTM_INITIATOR)) {
		if (isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_LCIM)) {
			buflen += TLV_HDR_LEN + DOT11_MNG_IE_MREQ_LCI_FIXED_LEN;
		}
		if (isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_CIVIC_LOC)) {
			buflen += TLV_HDR_LEN + DOT11_MNG_IE_MREQ_CIVIC_FIXED_LEN;
		}
	} else {
		WL_ERROR(("wl%d: %s: FTM not enabled, not including location elements\n",
			wlc->pub->unit, __FUNCTION__));
	}

	if ((p = wlc_rrm_prep_gen_request(rrm_info, cfg, &cfg->BSSID, buflen, &pbody)) == NULL)
		return;

	rmreq = (dot11_rm_action_t *)pbody;
	rmreq->action = DOT11_RM_ACTION_NR_REQ;
	rmreq->token = rrm_cfg->req_token;
	req_data_ptr = &rmreq->data[0];

	if (ssid != NULL) {
		ssid_ie = (bcm_tlv_t *)&rmreq->data[0];
		ssid_ie->id = DOT11_MNG_SSID_ID;
		ssid_ie->len = MIN(DOT11_MAX_SSID_LEN, (uint8)ssid->SSID_len);
		bcopy(ssid->SSID, ssid_ie->data, ssid_ie->len);
		req_data_ptr += (TLV_HDR_LEN + ssid_ie->len);
	}

	if (isset(cfg->ext_cap, DOT11_EXT_CAP_FTM_RESPONDER) ||
		isset(cfg->ext_cap, DOT11_EXT_CAP_FTM_INITIATOR)) {

		WLC_RRM_UPDATE_TOKEN(rrm_info->req_elt_token);
		if (isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_LCIM)) {
			ie = (dot11_meas_req_loc_t *) req_data_ptr;
			ie->id = DOT11_MNG_MEASURE_REQUEST_ID;
			ie->len = DOT11_MNG_IE_MREQ_LCI_FIXED_LEN;
			ie->token = rrm_info->req_elt_token;
			ie->mode = 0; /* enable bit is 0 */
			ie->type = DOT11_MEASURE_TYPE_LCI;
			ie->req.lci.subject = DOT11_FTM_LOCATION_SUBJ_REMOTE;
			req_data_ptr += TLV_HDR_LEN + DOT11_MNG_IE_MREQ_LCI_FIXED_LEN;
		}
		if (isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_CIVIC_LOC)) {
			ie = (dot11_meas_req_loc_t *) req_data_ptr;
			ie->id = DOT11_MNG_MEASURE_REQUEST_ID;
			ie->len = DOT11_MNG_IE_MREQ_CIVIC_FIXED_LEN;
			ie->token = rrm_info->req_elt_token;
			ie->mode = 0; /* enable bit is 0 */
			ie->type = DOT11_MEASURE_TYPE_CIVICLOC;
			ie->req.civic.subject = DOT11_FTM_LOCATION_SUBJ_REMOTE;
			ie->req.civic.type = 0;
			ie->req.civic.siu = 0;
			ie->req.civic.si = 0;
			req_data_ptr += TLV_HDR_LEN + DOT11_MNG_IE_MREQ_CIVIC_FIXED_LEN;
		}
	}

	scb = wlc_scbfind(wlc, cfg, &cfg->BSSID);
	wlc_sendmgmt(wlc, p, cfg->wlcif->qi, scb);
}
#ifdef WLASSOC_NBR_REQ
/* called from sta assocreq_done function via scb state notif */
static void
wlc_rrm_sta_assoc_state_upd(void *ctx, bss_assoc_state_data_t *assoc_state_info)
{
	wlc_rrm_info_t *rrm_info;
	wlc_bsscfg_t *cfg;
	wlc_ssid_t ssid;
	int err;

	ASSERT(ctx && assoc_state_info && assoc_state_info->cfg);
	rrm_info = (wlc_rrm_info_t *)ctx;
	cfg = assoc_state_info->cfg;

	if (!WL11K_ENAB(rrm_info->wlc->pub) || !BSSCFG_STA(cfg)) {
		return;
	}

	/* Trigger a neighbor report to prepare hot channel list for roaming */
	if ((assoc_state_info->state == AS_JOIN_ADOPT) && cfg->BSS && cfg->target_bss &&
			(cfg->target_bss->capability & DOT11_CAP_RRM)) {
		ssid.SSID_len = cfg->SSID_len;
		memcpy(ssid.SSID, cfg->SSID, cfg->SSID_len);
		err = wlc_iovar_op(rrm_info->wlc, "rrm_nbr_req", NULL, 0,
				&ssid, sizeof(wlc_ssid_t), IOV_SET, cfg->wlcif);
		if (err) {
			WL_ERROR(("wl%d: ERROR %d wlc_iovar_op failed: \"rrm_nbr_req\"\n",
					rrm_info->wlc->pub->unit, err));
		}
	}
}
#endif /* WLASSOC_NBR_REQ */
#endif /* STA */

#ifdef WL11K_AP
static void
wlc_rrm_nbr_scancb(void *arg, int status, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = (wlc_info_t*)arg;
	wlc_rrm_info_t *rrm_info = (wlc_rrm_info_t *)wlc->rrm_info;
	wlc_bss_list_t *bsslist;
	int i = 0;
	wlc_bss_info_t *bi;
#ifdef BCMDBG
	wlc_rrm_req_state_t *rrm_state = rrm_info->rrm_state;
	char eabuf[ETHER_ADDR_STR_LEN];
#endif // endif
	rrm_bsscfg_cubby_t *rrm_cfg;
	struct scb *scb;
	nbr_rpt_elem_t nbr_elt;

	wlc_bss_info_t *ap_bi;

	struct dot11_bcn_prb *bcn;
	uint32 flags;
	uint32 apsd;
	uint32 nbr_bssid_info = 0;

	uint32 ap_bssid_info;
	uint16 ap_capability;

	int ncnt;

	WL_RRM(("%s: state: %d, status: %d, count=%d\n", __FUNCTION__,
		rrm_state->step, status, wlc->scan_results->count));

	if (status != WLC_E_STATUS_SUCCESS) {
		WL_ERROR(("RRM Neighbor scan failure %d\n", status));
		return;
	}

	rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, cfg);
	ASSERT(rrm_cfg != NULL);

	/* clear previous list */
	wlc_rrm_flush_neighbors(rrm_info, cfg, NBR_ADD_DYNAMIC);

	bsslist = wlc->scan_results;

	/* Similar to wlc_rrm_add_bcnnbr */
	ap_bi = cfg->current_bss;
	ap_capability = ap_bi->capability;

	/* bssid_info of ap */
	ap_bssid_info = DOT11_NGBR_BI_REACHABILTY | DOT11_NGBR_BI_SEC |
		((ap_capability & DOT11_CAP_SPECTRUM)? DOT11_NGBR_BI_CAP_SPEC_MGMT : 0) |
		((ap_capability & DOT11_CAP_QOS)? DOT11_NGBR_BI_CAP_QOS : 0) |
		((ap_capability & DOT11_CAP_APSD)? DOT11_NGBR_BI_CAP_APSD : 0) |
		((ap_capability & DOT11_CAP_RRM)? DOT11_NGBR_BI_CAP_RDIO_MSMT : 0) |
		((ap_capability & DOT11_CAP_DELAY_BA)? DOT11_NGBR_BI_CAP_DEL_BA : 0) |
		((ap_capability & DOT11_CAP_IMMEDIATE_BA)? DOT11_NGBR_BI_CAP_IMM_BA : 0);

	if (ap_bi->flags & WLC_BSS_HT) {
	        ap_bssid_info |= DOT11_NGBR_BI_HT;
	}

	/* Populate known values from scan_results into neighbor list */
	for (i = 0; i < bsslist->count; i++) {
		bi = bsslist->ptrs[i];
#ifdef BCMDBG
		WL_RRM(("%s: i=%d, SSID=%s BSSID=%s channel=%d\n", __FUNCTION__,
			i, bi->SSID, bcm_ether_ntoa(&bi->BSSID, eabuf),
			CHSPEC_CHANNEL(bi->chanspec)));
#endif // endif
		bzero(&(nbr_elt), sizeof(nbr_rpt_elem_t));

		/* BSSID */
		memcpy((void *)(&(nbr_elt.bssid)), (void *)(&bi->BSSID), ETHER_ADDR_LEN);

		bcn = bi->bcn_prb;
		flags = bi->flags;

		/* APSD enabled by default if wme is enabled */
		if ((flags & WLC_BSS_BRCM) && (flags & WLC_BSS_WME)) {
			apsd = DOT11_NGBR_BI_CAP_APSD;
		}
		else {
			apsd = ((bcn->capability & DOT11_CAP_APSD)? DOT11_NGBR_BI_CAP_APSD : 0);
		}

		/* bssid_info of neighbor */
		nbr_bssid_info = DOT11_NGBR_BI_REACHABILTY | DOT11_NGBR_BI_SEC |
			((bcn->capability & DOT11_CAP_SPECTRUM)? DOT11_NGBR_BI_CAP_SPEC_MGMT : 0) |
			((flags & WLC_BSS_WME)? DOT11_NGBR_BI_CAP_QOS : 0) |
			(apsd) |
			((bcn->capability & DOT11_CAP_RRM)? DOT11_NGBR_BI_CAP_RDIO_MSMT : 0) |
			((bcn->capability & DOT11_CAP_DELAY_BA)? DOT11_NGBR_BI_CAP_DEL_BA : 0) |
			((bcn->capability & DOT11_CAP_IMMEDIATE_BA)? DOT11_NGBR_BI_CAP_IMM_BA : 0);

		if (bi->flags & WLC_BSS_HT) {
			nbr_bssid_info |= DOT11_NGBR_BI_HT;
		}

		WL_RRM(("%s: ap_bssid_info=0x%x,cap=0x%x nbr_bssid_info=0x%x,cap=0x%x \n",
			__FUNCTION__, ap_bssid_info, ap_capability,
			nbr_bssid_info, bcn->capability));

		nbr_elt.bssid_info = nbr_bssid_info & ap_bssid_info;

		/* Regulatory class */
		nbr_elt.reg = wlc_get_regclass(wlc->cmi, bi->chanspec);

		/* channel */
		nbr_elt.channel = CHSPEC_CHANNEL(bi->chanspec);

		nbr_elt.id = DOT11_MNG_NEIGHBOR_REP_ID;
		nbr_elt.len = DOT11_NEIGHBOR_REP_IE_FIXED_LEN;

		nbr_elt.addtype = NBR_ADD_DYNAMIC;

#ifdef BCMDBG
		WL_RRM(("%s: i=%d, nbr_elt BSSID=%s ch=%d bssid_info=0x%x = nbr(0x%x) & ap(0x%x) ",
			__FUNCTION__, i, bcm_ether_ntoa(&bi->BSSID, eabuf), nbr_elt.channel,
			nbr_elt.bssid_info, nbr_bssid_info, ap_bssid_info));
#endif // endif
		/* Add to dynamic list - update neighbor count */
		wlc_rrm_add_neighbor(rrm_info, &nbr_elt, NULL, rrm_cfg, NBR_ADD_DYNAMIC);
	}

	/* Send auto learned Neighbor Report Response */
	ncnt = wlc_rrm_get_neighbor_count(wlc, cfg, NBR_ADD_DYNAMIC);
	BCM_REFERENCE(ncnt);  /* needed for internal builds */
	WL_RRM(("%s: auto learned Neighbor Count = %d - call send report \n ", __FUNCTION__, ncnt));
	scb = wlc_scbfind(wlc, cfg, &rrm_cfg->da);
	wlc_rrm_send_nbrrep(rrm_info, cfg, scb, NULL, 0, NBR_ADD_DYNAMIC);
}

static void
wlc_rrm_recv_nrreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, struct scb *scb, uint8 *body,
	int body_len)
{
	wlc_info_t *wlc;
	dot11_rm_action_t *rmreq;
	rrm_bsscfg_cubby_t *rrm_cfg = NULL;
	wlc_ssid_t ssid;
	uint8 flags = 0;
	int bcmerr = 0;
	int tlv_len;
	bcm_tlv_t *tlv = NULL;
	uint8 *tlvs;
	dot11_meas_req_loc_t *ie;
	int len;

	rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, cfg);
	ASSERT(rrm_cfg != NULL);

	wlc = rrm_info->wlc;
	ASSERT(wlc);

	BCM_REFERENCE(wlc);
	WL_RRM(("<802.11K Rx Req> %s: <Enter>\n", __FUNCTION__));
	WL_RRM_HEX("rrm_cfg->rrm_cap:", rrm_cfg->rrm_cap, DOT11_RRM_CAP_LEN);
	WL_RRM_HEX("body:", body, body_len);

	if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_NEIGHBOR_REPORT)) {
		WL_ERROR(("wl%d: %s: Unsupported\n", wlc->pub->unit,
			__FUNCTION__));
		return;
	}
	if (body_len < DOT11_RM_ACTION_LEN) {
		WL_ERROR(("wl%d: %s: invalid body len %d\n", wlc->pub->unit,
		          __FUNCTION__, body_len));
		return;
	}

	rmreq = (dot11_rm_action_t *)body;
	bcopy(&scb->ea, &rrm_cfg->da, ETHER_ADDR_LEN);
	rrm_cfg->dialog_token = rmreq->token;

	memset(&ssid, 0, sizeof(ssid));

	/* parse subelements */
	if (body_len > DOT11_RM_ACTION_LEN) {
		tlvs = (uint8 *)&rmreq->data[0];
		tlv_len = body_len - DOT11_RM_ACTION_LEN;

		if (tlv_len > 1) {
			tlv = bcm_parse_tlvs(tlvs, tlv_len, DOT11_RMREQ_BCN_SSID_ID);
			if (tlv) {
				ssid.SSID_len = MIN(DOT11_MAX_SSID_LEN, tlv->len);
				bcopy(tlv->data, ssid.SSID, ssid.SSID_len);
			}
		}
	}

	tlv = (bcm_tlv_t *)rmreq->data;
	len = body_len - DOT11_RM_ACTION_LEN;

	/* for each measurement request, calc the length of the report in the response */
	while (len > TLV_HDR_LEN) {
		tlv_len = TLV_HDR_LEN + tlv->len;
		if (tlv->id == DOT11_MNG_SSID_ID) {
			/*
			SSID element can be present in a neighbor request to
			request a neighbor list for a specifid SSID.
			*/
			ssid.SSID_len = MIN(DOT11_MAX_SSID_LEN, tlv->len);
				bcopy(tlv->data, ssid.SSID, ssid.SSID_len);
			WL_TMP(("wl%d: %s: Found SSID element, len %d, SSID \"%s\"\n",
				wlc->pub->unit, __FUNCTION__, ssid.SSID_len, ssid.SSID));
		} else if (tlv->id == DOT11_MNG_MEASURE_REQUEST_ID) {
			ie = (dot11_meas_req_loc_t *)tlv;
			WL_TMP(("wl%d: %s: Found Measure Request, len %d\n", wlc->pub->unit,
				__FUNCTION__, ie->len));
			if (ie->len < DOT11_MNG_IE_MREQ_MIN_LEN) {
				WL_ERROR(("wl%d: %s: Measure Request len %d too short\n",
					wlc->pub->unit, __FUNCTION__, ie->len));
				goto nrreq_next_tlv;
			}
			if ((ie->mode & DOT11_MEASURE_MODE_ENABLE) != 0) {
				WL_ERROR(("wl%d: %s: Measure mode 0x%x, ENABLE not 0\n",
					wlc->pub->unit, __FUNCTION__, ie->mode));
				goto nrreq_next_tlv;
			}
			if (ie->type != DOT11_MEASURE_TYPE_LCI &&
				ie->type != DOT11_MEASURE_TYPE_CIVICLOC) {
				WL_ERROR(("wl%d: %s: Measure type 0x%d, not LCI or CIVIC\n",
					wlc->pub->unit, __FUNCTION__, ie->type));
				goto nrreq_next_tlv;
			}
#ifdef WL_FTM_11K
			if (ie->type == DOT11_MEASURE_TYPE_LCI) {
				if (ie->req.lci.subject != DOT11_FTM_LOCATION_SUBJ_REMOTE) {
					WL_ERROR(("wl%d: %s: LCI Measurement subject 0x%d,"
						" not REMOTE\n", wlc->pub->unit, __FUNCTION__,
						ie->req.lci.subject));
					goto nrreq_next_tlv;
				}
				rrm_cfg->lci_token = ie->token;
				flags |= RRM_NREQ_FLAGS_LCI_PRESENT;
			}
			if (ie->type == DOT11_MEASURE_TYPE_CIVICLOC) {
				if (ie->req.civic.subject != DOT11_FTM_LOCATION_SUBJ_REMOTE) {
					WL_ERROR(("wl%d: %s: Civic Measurement subject 0x%d,"
						" not REMOTE\n", wlc->pub->unit, __FUNCTION__,
						ie->req.civic.subject));
					goto nrreq_next_tlv;
				}
				rrm_cfg->civic_token = ie->token;
				flags |= RRM_NREQ_FLAGS_CIVICLOC_PRESENT;
			}
#endif /* WL_FTM_11K */
		} else {
			WL_ERROR(("wl%d: %s: TLV ID 0x%d, not SSID or Measurement Request\n",
				wlc->pub->unit, __FUNCTION__, tlv->id));
			goto nrreq_next_tlv;
		}

nrreq_next_tlv:
		tlv = (bcm_tlv_t *)((int8*)tlv + tlv_len);
		len -= tlv_len;
	}

	if (isset(cfg->ext_cap, DOT11_EXT_CAP_FTM_RESPONDER)) {
		if (isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_LCIM) &&
			(flags & RRM_NREQ_FLAGS_LCI_PRESENT)) {
			flags |= RRM_NREQ_FLAGS_OWN_REPORT;
		}
		if (isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_CIVIC_LOC) &&
			(flags & RRM_NREQ_FLAGS_CIVICLOC_PRESENT)) {
			flags |= RRM_NREQ_FLAGS_OWN_REPORT;
		}
	}

	WL_RRM(("%s: ssid len=%d, SSID=%s\n", __FUNCTION__, ssid.SSID_len, ssid.SSID));

	if (rrm_cfg->nbr_scan) {
		bcmerr = wlc_scan_request_ex(wlc, DOT11_BSSTYPE_ANY, &ether_bcast, 1, &ssid,
				DOT11_SCANTYPE_ACTIVE, -1, 0, -1, -1, NULL, 0, 0, TRUE,
				wlc_rrm_nbr_scancb, wlc, WLC_ACTION_SCAN, FALSE, cfg, NULL, NULL);
		if (bcmerr != BCME_OK) {
			WL_RRM(("%s: wlc_scan_request returned %d \n", __FUNCTION__, bcmerr));
		}
	}

	/* Send static list here - the scancb will send the dynamic list */
	wlc_rrm_send_nbrrep(rrm_info, cfg, scb, &ssid, flags, NBR_ADD_STATIC);
}

/* Send Neighbor Report Response */
static void
wlc_rrm_send_nbrrep(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, struct scb *scb,
	wlc_ssid_t *ssid, uint8 flags, int addtype)
{
	wlc_info_t *wlc;
	uint rep_count = 0;
	rrm_nbr_rep_t *nbr_rep;
	dot11_rm_action_t *rmrep;
	int rrm_req_len, i;
	uint8 *pbody;
	void *p;
	uint8 *ptr, *bufend;
	uint16 list_cnt = 0;
	rrm_scb_info_t *scb_info = RRM_SCB_INFO(rrm_info, scb);
	rrm_bsscfg_cubby_t *rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, cfg);
	bool ssid_self_match = TRUE;
	rrm_nbr_rep_t *nbr_rep_self;
	int scb_sec_len = 0;

	ASSERT(rrm_cfg != NULL);

	WL_RRM(("%s: <Enter> type=%s \n", __FUNCTION__,
		(addtype == NBR_ADD_DYNAMIC) ? "Dynamic" : "Static"));

	wlc = rrm_info->wlc;

	nbr_rep = RRM_NBR_REP_HEAD(rrm_cfg, addtype);
	nbr_rep_self = rrm_cfg->nbr_rep_self;

#ifdef BCMDBG
	wlc_rrm_dump_neighbors(rrm_info, cfg, addtype);
#endif /* BCMDBG */

	while (nbr_rep) {
		rep_count++;
		nbr_rep = nbr_rep->next;
	}

	if (addtype == NBR_ADD_STATIC) {
		nbr_rep = scb_info->nbr_rep_head;
		while (nbr_rep) {
			nbr_rep = nbr_rep->next;
			list_cnt++;
		}
	}

	/* Add own BSSID to neighbor list if certain conditions are met */
	if (flags & RRM_NREQ_FLAGS_OWN_REPORT) {
		if (ssid != NULL && ssid->SSID_len > 0) {
			/* if ssid is specified and does not match entry, skip */
			/* SSID_len of 0 means wildcard */
			if ((ssid->SSID_len != cfg->SSID_len) ||
				(memcmp(cfg->SSID, &ssid->SSID,
				ssid->SSID_len) != 0)) {
				ssid_self_match = FALSE;
				WL_TMP(("wl%d: %s: SSID element present, and does not match self\n",
					wlc->pub->unit, __FUNCTION__));
			}
		}
	}
	if (rep_count == 0 && list_cnt == 0 &&
		!(ssid_self_match && (flags & RRM_NREQ_FLAGS_OWN_REPORT))) {
		/* must send a neighbor report response with zero neighbor reports */
		WL_ERROR(("wl%d: %s: Neighbor Report element is empty\n",
			wlc->pub->unit, __FUNCTION__));
	}

	/* too many optional elements to count up, fixup the length at the end */
	rrm_req_len = ETHER_MAX_DATA;

	p = wlc_frame_get_action(wlc, &rrm_cfg->da,
	                       &cfg->cur_etheraddr, &cfg->BSSID,
	                       rrm_req_len, &pbody, DOT11_ACTION_CAT_RRM);
	if (p == NULL) {
		WL_ERROR((WLC_BSS_MALLOC_ERR, WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg), __FUNCTION__,
			rrm_req_len, MALLOCED(wlc->osh)));
		return;
	}

	bufend = pbody + rrm_req_len;

	rmrep = (dot11_rm_action_t *)pbody;
	rmrep->category = DOT11_ACTION_CAT_RRM;
	rmrep->action = DOT11_RM_ACTION_NR_REP;
	rmrep->token = rrm_cfg->dialog_token;
	ptr = rmrep->data;

	flags |= RRM_NREQ_FLAGS_BSS_PREF;
	/* add neighbors obtained from beacon report */
	nbr_rep = scb_info->nbr_rep_head;

	for (i = 0; i < list_cnt; i++) {
		if (ssid != NULL && ssid->SSID_len > 0) {
			/* if ssid is specified and does not match entry, skip */
			if ((ssid->SSID_len != nbr_rep->nbr_elt.ssid.SSID_len) ||
				(memcmp(&nbr_rep->nbr_elt.ssid.SSID, &ssid->SSID,
				ssid->SSID_len) != 0)) {
				/* Move to the next one */
				nbr_rep = nbr_rep->next;
				continue;
			}
		}

		wlc_rrm_create_nbrrep_element(rrm_cfg, &ptr, nbr_rep, flags, bufend);
		/* Move to the next one */
		nbr_rep = nbr_rep->next;
	}

	nbr_rep = RRM_NBR_REP_HEAD(rrm_cfg, addtype);

	for (i = 0; i < rep_count; i++) {
		if (ssid != NULL && ssid->SSID_len > 0) {
			/* if ssid is specified and does not match entry, skip */
			if ((ssid->SSID_len != nbr_rep->nbr_elt.ssid.SSID_len) ||
				(memcmp(&nbr_rep->nbr_elt.ssid.SSID, &ssid->SSID,
				ssid->SSID_len) != 0)) {
				/* Move to the next one */
				nbr_rep = nbr_rep->next;
				continue;
			}
		}

		wlc_rrm_create_nbrrep_element(rrm_cfg, &ptr, nbr_rep, flags, bufend);
		/* Move to the next one */
		nbr_rep = nbr_rep->next;
	}

	/* Add own BSSID to neighbor list if certain conditions are met */
	if ((flags & RRM_NREQ_FLAGS_OWN_REPORT) && ssid_self_match) {
		rrm_nbr_rep_t nbr_rep_tmp;

		memset(&nbr_rep_tmp, 0, sizeof(nbr_rep_tmp));
		nbr_rep_tmp.nbr_elt.id = DOT11_MNG_NEIGHBOR_REP_ID;
		memcpy(&nbr_rep_tmp.nbr_elt.bssid, &cfg->BSSID, ETHER_ADDR_LEN);
		nbr_rep_tmp.nbr_elt.bssid_info |= (DOT11_NGBR_BI_CAP_RDIO_MSMT |
			DOT11_NGBR_BI_FTM | DOT11_NGBR_BI_SEC | DOT11_NGBR_BI_REACHABILTY);
		if (BSS_WL11H_ENAB(wlc, cfg))
			nbr_rep_tmp.nbr_elt.bssid_info |= DOT11_NGBR_BI_CAP_SPEC_MGMT;
		if (BSS_N_ENAB(wlc, cfg))
			nbr_rep_tmp.nbr_elt.bssid_info |= DOT11_NGBR_BI_HT;
		if (BSS_VHT_ENAB(wlc, cfg))
			nbr_rep_tmp.nbr_elt.bssid_info |= DOT11_NGBR_BI_VHT;

		nbr_rep_tmp.nbr_elt.reg = wlc_get_regclass(wlc->cmi, cfg->current_bss->chanspec);
		nbr_rep_tmp.nbr_elt.channel = CHSPEC_CHANNEL(wlc->chanspec);
		nbr_rep_tmp.nbr_elt.phytype = (uint8) wlc->band->phytype;
		memcpy(&nbr_rep_tmp.nbr_elt.chanspec, &wlc->chanspec, sizeof(chanspec_t));

		if (nbr_rep_self) {
			nbr_rep_tmp.opt_lci = nbr_rep_self->opt_lci;
			nbr_rep_tmp.opt_civic = nbr_rep_self->opt_civic;
			flags |= RRM_NREQ_FLAGS_COLOCATED_BSSID;
		}

		wlc_rrm_create_nbrrep_element(rrm_cfg, &ptr, &nbr_rep_tmp, flags, bufend);
	}

	if ((bufend - ptr) > 0) {
#ifdef MFP
		scb_sec_len = wlc_mfp_frame_get_sec_len(wlc->mfp, FC_ACTION,
			DOT11_ACTION_CAT_RRM, &scb->ea, cfg);
#endif // endif
		rrm_req_len += DOT11_MGMT_HDR_LEN;
		PKTSETLEN(wlc->osh, p, (rrm_req_len - (bufend - ptr)) + scb_sec_len);
	}
	wlc_rrm_send_event(rrm_info, scb, (char *)rmrep, rrm_req_len,
		DOT11_RM_ACTION_NR_REP, 0);
	wlc_sendmgmt(wlc, p, cfg->wlcif->qi, scb);
}

static void
wlc_rrm_get_neighbor_list(wlc_rrm_info_t *rrm_info, nbr_rpt_elem_t *nbr_elt, uint16 list_cnt,
	rrm_bsscfg_cubby_t *rrm_cfg)
{
	rrm_nbr_rep_t *nbr_rep;
	int i;

	uint8 *ptr = (uint8 *)nbr_elt;
	nbr_rep = rrm_cfg->nbr_rep_head;

	for (i = 0; i < list_cnt; i++) {
		nbr_elt->id = nbr_rep->nbr_elt.id;
		nbr_elt->len = nbr_rep->nbr_elt.len;

		memcpy(&nbr_elt->bssid, &nbr_rep->nbr_elt.bssid, ETHER_ADDR_LEN);

		store32_ua((uint8 *)&nbr_elt->bssid_info, nbr_rep->nbr_elt.bssid_info);

		nbr_elt->reg = nbr_rep->nbr_elt.reg;
		nbr_elt->channel = nbr_rep->nbr_elt.channel;
		nbr_elt->phytype = nbr_rep->nbr_elt.phytype;
		nbr_elt->ssid.SSID_len = nbr_rep->nbr_elt.ssid.SSID_len;
		memcpy(&(nbr_elt->ssid.SSID), &(nbr_rep->nbr_elt.ssid.SSID),
			DOT11_MAX_SSID_LEN);
		store16_ua((uint8 *)&nbr_elt->chanspec, nbr_rep->nbr_elt.chanspec);
		nbr_elt->bss_trans_preference = nbr_rep->nbr_elt.bss_trans_preference;

		ptr += sizeof(nbr_rpt_elem_t);
		if (nbr_elt->flags & WL_RRM_NBR_RPT_LCI_FLAG) {
			if (nbr_rep->opt_lci != NULL) {
				dot11_meas_rep_t *lci_rep = (dot11_meas_rep_t *) nbr_rep->opt_lci;
				memcpy(ptr, nbr_rep->opt_lci, lci_rep->len + TLV_HDR_LEN);
				ptr += (lci_rep->len + TLV_HDR_LEN);
			} else {
				nbr_elt->flags &= ~WL_RRM_NBR_RPT_LCI_FLAG;
			}
		}

		if (nbr_elt->flags & WL_RRM_NBR_RPT_CIVIC_FLAG) {
			if (nbr_rep->opt_civic != NULL) {
				dot11_meas_rep_t *civic_rep =
					(dot11_meas_rep_t *) nbr_rep->opt_civic;
				memcpy(ptr, nbr_rep->opt_civic, civic_rep->len + TLV_HDR_LEN);
				ptr += (civic_rep->len + TLV_HDR_LEN);
			} else {
				nbr_elt->flags &= ~WL_RRM_NBR_RPT_CIVIC_FLAG;
			}
		}
		nbr_rep = nbr_rep->next;
		nbr_elt++;
	}
}

static void
wlc_rrm_del_neighbor(wlc_rrm_info_t *rrm_info, struct ether_addr *ea, struct wlc_if *wlcif,
	rrm_bsscfg_cubby_t *rrm_cfg)
{
	rrm_nbr_rep_t *nbr_rep, *nbr_rep_pre = NULL;
	struct ether_addr *bssid;
	wlc_info_t *wlc;
	wlc_bsscfg_t *bsscfg;
	bool channel_removed = FALSE;

	wlc = rrm_info->wlc;
	nbr_rep = rrm_cfg->nbr_rep_head;
	while (nbr_rep) {
		bssid = &nbr_rep->nbr_elt.bssid;
		if (memcmp(ea, bssid, ETHER_ADDR_LEN) == 0)
			break;
		nbr_rep_pre = nbr_rep;
		nbr_rep = nbr_rep->next;

	}
	if (nbr_rep == NULL)
		return;

	wlc_rrm_remove_regclass_nbr_cnt(rrm_info, nbr_rep, 1, &channel_removed, rrm_cfg);

	/* remove node from neighbor report list */
	if (nbr_rep_pre == NULL)
		rrm_cfg->nbr_rep_head = nbr_rep->next;
	else
		nbr_rep_pre->next = nbr_rep->next;

	if (nbr_rep->opt_lci != NULL) {
		dot11_meas_rep_t *lci_rep = (dot11_meas_rep_t *) nbr_rep->opt_lci;
		MFREE(rrm_info->wlc->osh, nbr_rep->opt_lci, lci_rep->len + TLV_HDR_LEN);
	}
	if (nbr_rep->opt_civic != NULL) {
		dot11_meas_rep_t *civic_rep = (dot11_meas_rep_t *) nbr_rep->opt_civic;
		MFREE(rrm_info->wlc->osh, nbr_rep->opt_civic, civic_rep->len + TLV_HDR_LEN);
	}
	MFREE(rrm_info->wlc->osh, nbr_rep, sizeof(rrm_nbr_rep_t));

	/* Update beacons and probe responses */
	if (!channel_removed) {
		bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
		ASSERT(bsscfg);
		wlc_rrm_update_beacon(wlc, bsscfg);
	}
}

static void
wlc_rrm_remove_regclass_nbr_cnt(wlc_rrm_info_t *rrm_info, rrm_nbr_rep_t *nbr_rep,
	bool delete_empty_reg, bool *channel_removed, rrm_bsscfg_cubby_t *rrm_cfg)
{
	bool reg_became_empty = FALSE;
	reg_nbr_count_t *nbr_cnt, *nbr_cnt_pre = NULL;
	int i;

	*channel_removed = FALSE;
	if (!VALID_NBR_REP_CHAN(nbr_rep)) {
		return;
	}

	nbr_cnt = rrm_cfg->nbr_cnt_head;
	while (nbr_cnt) {
		if (nbr_rep->nbr_elt.reg == nbr_cnt->reg) {
			nbr_cnt->nbr_count_by_channel[nbr_rep->nbr_elt.channel] -= 1;
			if (nbr_cnt->nbr_count_by_channel[nbr_rep->nbr_elt.channel] == 0)
				*channel_removed = TRUE;

			reg_became_empty = TRUE;
			for (i = 0; i < MAXCHANNEL; i++) {
				if (nbr_cnt->nbr_count_by_channel[i] > 0) {
					reg_became_empty = FALSE;
					break;
				}
			}
			if (delete_empty_reg && reg_became_empty) {
				/* remove node */
				if (nbr_cnt_pre == NULL)
					rrm_cfg->nbr_cnt_head = nbr_cnt->next;
				else
					nbr_cnt_pre->next = nbr_cnt->next;
				MFREE(rrm_info->wlc->osh, nbr_cnt, sizeof(reg_nbr_count_t));
			}
			break;
		}
		nbr_cnt_pre = nbr_cnt;
		nbr_cnt = nbr_cnt->next;
	}
}

static void
wlc_rrm_add_neighbor(wlc_rrm_info_t *rrm_info, nbr_rpt_elem_t *nbr_elt, struct wlc_if *wlcif,
	rrm_bsscfg_cubby_t *rrm_cfg, int addtype)
{
	rrm_nbr_rep_t *nbr_rep;
	wlc_info_t *wlc;
	wlc_bsscfg_t *bsscfg;
	uint16 nbr_rep_cnt;
	bool channel_removed = FALSE;
	bool channel_added = FALSE;
	bool channels_same = FALSE;
	bool should_update_beacon = FALSE;
	bool del = FALSE;
	rrm_nbr_rep_t **rrm_nbr_rep_headp;

	wlc = rrm_info->wlc;
	rrm_nbr_rep_headp = (RRM_NBR_REP_HEADPTR(rrm_cfg, addtype));

	if (!VALID_NBR_CHAN(nbr_elt->channel)) {
		return;
	}

	/* Find Neighbor Report element from list */
	nbr_rep = *rrm_nbr_rep_headp;
	while (nbr_rep) {
		if (memcmp(&nbr_rep->nbr_elt.bssid, &nbr_elt->bssid, ETHER_ADDR_LEN) == 0)
			break;
		nbr_rep = nbr_rep->next;
	}

	if (nbr_rep) {
		if (nbr_rep->nbr_elt.reg == nbr_elt->reg) {
			if (nbr_rep->nbr_elt.channel == nbr_elt->channel)
				channels_same = TRUE;
		} else {
			del = TRUE;
		}
		wlc_rrm_remove_regclass_nbr_cnt(rrm_info, nbr_rep, del, &channel_removed, rrm_cfg);
		memcpy(&nbr_rep->nbr_elt, nbr_elt, sizeof(*nbr_elt));
		if (nbr_rep->opt_lci != NULL) {
			dot11_meas_rep_t *lci_rep = (dot11_meas_rep_t *) nbr_rep->opt_lci;
			MFREE(rrm_info->wlc->osh, nbr_rep->opt_lci, lci_rep->len + TLV_HDR_LEN);
			nbr_rep->opt_lci = NULL;
		}
		if (nbr_rep->opt_civic != NULL) {
			dot11_meas_rep_t *civic_rep = (dot11_meas_rep_t *) nbr_rep->opt_civic;
			MFREE(rrm_info->wlc->osh, nbr_rep->opt_civic, civic_rep->len + TLV_HDR_LEN);
			nbr_rep->opt_civic = NULL;
		}
	} else {
		bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
		nbr_rep_cnt = wlc_rrm_get_neighbor_count(wlc, bsscfg, addtype);
		if (nbr_rep_cnt >= MAX_NEIGHBORS) {
			WL_ERROR(("%s nbr_rep_cnt %d is over MAX_NEIGHBORS\n", __FUNCTION__,
				nbr_rep_cnt));
			return;
		}
		nbr_rep = (rrm_nbr_rep_t *)MALLOCZ(rrm_info->wlc->osh, sizeof(rrm_nbr_rep_t));
		if (nbr_rep == NULL) {
			WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(rrm_info->wlc), __FUNCTION__,
				(int)sizeof(rrm_nbr_rep_t), MALLOCED(wlc->osh)));
			return;
		}
		memcpy(&nbr_rep->nbr_elt, nbr_elt, sizeof(*nbr_elt));
		nbr_rep->next = *rrm_nbr_rep_headp;
		*rrm_nbr_rep_headp = nbr_rep;
	}

	if (addtype == NBR_ADD_STATIC) {
		wlc_rrm_add_regclass_neighbor_count(rrm_info, nbr_rep, &channel_added, rrm_cfg);

		should_update_beacon = !channels_same && (channel_added || channel_removed);

		/* Update beacons and probe responses  */
		if (should_update_beacon) {
			bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
			ASSERT(bsscfg);

			if (bsscfg->up &&
				(BSSCFG_AP(bsscfg) ||
					(!bsscfg->BSS && !BSS_TDLS_ENAB(wlc, bsscfg)))) {
				/* Update AP or IBSS beacons */
				wlc_bss_update_beacon(wlc, bsscfg);
				/* Update AP or IBSS probe responses */
				wlc_bss_update_probe_resp(wlc, bsscfg, TRUE);
			}
		}
	}
}

void
wlc_rrm_add_neighbor_chanspec(nbr_rpt_elem_t *nbr_elt, rrm_bsscfg_cubby_t *rrm_cfg)
{
	rrm_nbr_rep_t *nbr_rep;

	/* Find Neighbor Report element from list */
	nbr_rep = rrm_cfg->nbr_rep_head;
	while (nbr_rep) {
		if (memcmp(&nbr_rep->nbr_elt.bssid, &nbr_elt->bssid, ETHER_ADDR_LEN) == 0)
				break;
		nbr_rep = nbr_rep->next;
	}

	if (nbr_rep) {
		memcpy(&nbr_rep->nbr_elt.chanspec, &nbr_elt->chanspec, sizeof(chanspec_t));
	} else {
		WL_ERROR(("%s: neighbor not found\n", __FUNCTION__));
	}
}

void
wlc_rrm_add_neighbor_ssid(nbr_rpt_elem_t *nbr_elt, rrm_bsscfg_cubby_t *rrm_cfg)
{
	rrm_nbr_rep_t *nbr_rep;

	/* Find Neighbor Report element from list */
	nbr_rep = rrm_cfg->nbr_rep_head;
	while (nbr_rep) {
		if (memcmp(&nbr_rep->nbr_elt.bssid, &nbr_elt->bssid, ETHER_ADDR_LEN) == 0)
				break;
		nbr_rep = nbr_rep->next;
	}

	if (nbr_rep) {
		memcpy(&nbr_rep->nbr_elt.ssid, &nbr_elt->ssid, sizeof(wlc_ssid_t));
	} else {
		WL_ERROR(("%s: neighbor not found\n", __FUNCTION__));
	}
}

int
wlc_rrm_add_neighbor_lci(wlc_rrm_info_t *rrm_info, struct ether_addr *nbr_bss,
	uint8 *opt_lci, int len, rrm_bsscfg_cubby_t *rrm_cfg)
{
	rrm_nbr_rep_t *nbr_rep;

	/* Find Neighbor Report element from list */
	nbr_rep = rrm_cfg->nbr_rep_head;
	while (nbr_rep) {
		if (memcmp(&nbr_rep->nbr_elt.bssid, nbr_bss, ETHER_ADDR_LEN) == 0)
				break;
		nbr_rep = nbr_rep->next;
	}

	if (nbr_rep) {
		return wlc_rrm_add_loc(rrm_info, &nbr_rep->opt_lci,
			opt_lci, len, DOT11_MEASURE_TYPE_LCI);
	} else {
		WL_ERROR(("%s: neighbor not found\n", __FUNCTION__));
		return BCME_NOTFOUND;
	}
}

int
wlc_rrm_add_neighbor_civic(wlc_rrm_info_t *rrm_info, struct ether_addr *nbr_bss,
	uint8 *opt_civic, int len, rrm_bsscfg_cubby_t *rrm_cfg)
{
	rrm_nbr_rep_t *nbr_rep;

	/* Find Neighbor Report element from list */
	nbr_rep = rrm_cfg->nbr_rep_head;
	while (nbr_rep) {
		if (memcmp(&nbr_rep->nbr_elt.bssid, nbr_bss, ETHER_ADDR_LEN) == 0)
			break;
		nbr_rep = nbr_rep->next;
	}

	if (nbr_rep) {
		return wlc_rrm_add_loc(rrm_info, &nbr_rep->opt_civic,
			opt_civic, len, DOT11_MEASURE_TYPE_CIVICLOC);
	} else {
		WL_ERROR(("%s: neighbor not found\n", __FUNCTION__));
		return BCME_NOTFOUND;
	}
}
static void
wlc_rrm_add_regclass_neighbor_count(wlc_rrm_info_t *rrm_info, rrm_nbr_rep_t *nbr_rep,
	bool *channel_added, rrm_bsscfg_cubby_t *rrm_cfg)
{
	reg_nbr_count_t *nbr_cnt;
	wlc_info_t *wlc;
	uint16 list_cnt;
	bool nbr_cnt_find = FALSE;

	wlc = rrm_info->wlc;
	*channel_added = FALSE;

	if (!VALID_NBR_REP_CHAN(nbr_rep)) {
		return;
	}

	nbr_cnt = rrm_cfg->nbr_cnt_head;
	while (nbr_cnt) {
		if (nbr_rep->nbr_elt.reg == nbr_cnt->reg) {
			nbr_cnt->nbr_count_by_channel[nbr_rep->nbr_elt.channel] += 1;

			if (nbr_cnt->nbr_count_by_channel[nbr_rep->nbr_elt.channel] == 1)
				*channel_added = TRUE;

			nbr_cnt_find = TRUE;
			break;
		}
		nbr_cnt = nbr_cnt->next;
	}

	if (!nbr_cnt_find) {
		list_cnt = wlc_rrm_get_reg_count(rrm_info, rrm_cfg);
		if (list_cnt >= MAX_CHANNEL_REPORT_IES) {
			WL_ERROR(("%s over MAX_CHANNEL_REPORT_IES\n", __FUNCTION__));
			return;
		}
		nbr_cnt = (reg_nbr_count_t *)MALLOCZ(wlc->osh, sizeof(reg_nbr_count_t));
		if (nbr_cnt == NULL) {
			WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__,
				(int)sizeof(reg_nbr_count_t), MALLOCED(wlc->osh)));
			return;
		}
		nbr_cnt->reg = nbr_rep->nbr_elt.reg;
		nbr_cnt->nbr_count_by_channel[nbr_rep->nbr_elt.channel] += 1;
		nbr_cnt->next = rrm_cfg->nbr_cnt_head;
		rrm_cfg->nbr_cnt_head = nbr_cnt;

		*channel_added = TRUE;
	}
}

static uint16
_wlc_rrm_get_neighbor_count(wlc_rrm_info_t *rrm_info, rrm_bsscfg_cubby_t *rrm_cfg, int addtype)
{
	uint16 list_cnt = 0;
	rrm_nbr_rep_t *nbr_rep;

	nbr_rep = RRM_NBR_REP_HEAD(rrm_cfg, addtype);
	while (nbr_rep) {
		nbr_rep = nbr_rep->next;
		list_cnt++;
	}

	return list_cnt;
}

static uint16
wlc_rrm_get_reg_count(wlc_rrm_info_t *rrm_info, rrm_bsscfg_cubby_t *rrm_cfg)
{
	uint16 list_cnt = 0;
	reg_nbr_count_t *nbr_cnt;

	nbr_cnt = rrm_cfg->nbr_cnt_head;
	while (nbr_cnt) {
		nbr_cnt = nbr_cnt->next;
		list_cnt++;
	}
	return list_cnt;
}

#ifdef WL11K_BCN_MEAS
static uint16
wlc_rrm_get_bcnnbr_count(wlc_rrm_info_t *rrm_info, rrm_scb_info_t *scb_info)
{
	uint16 list_cnt = 0;
	rrm_nbr_rep_t *nbr_rep;

	nbr_rep = scb_info->nbr_rep_head;
	while (nbr_rep) {
		nbr_rep = nbr_rep->next;
		list_cnt++;
	}
	return list_cnt;
}

static bool
wlc_rrm_security_match(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, uint16 cap,
	bcm_tlv_t *wpaie, bcm_tlv_t *wpa2ie)
{
	bool match = FALSE;

	if ((wpa2ie != NULL) && bcmwpa_is_rsn_auth(cfg->WPA_auth) && WSEC_ENABLED(cfg->wsec)) {
		match = TRUE;
	}
	if ((wpaie != NULL) && bcmwpa_is_wpa_auth(cfg->WPA_auth) && WSEC_ENABLED(cfg->wsec)) {
		match = TRUE;
	}
	if ((wpa2ie == NULL) && (wpaie == NULL) &&
		WSEC_ENABLED(cfg->wsec) == (cap & DOT11_CAP_PRIVACY)) {
		match = TRUE;
	}

	return match;
}

static int
wlc_rrm_add_bcnnbr(wlc_rrm_info_t *rrm_info, struct scb *scb, dot11_rmrep_bcn_t *rmrep_bcn,
	struct dot11_bcn_prb *bcn, bcm_tlv_t *htie, bcm_tlv_t *mdie, uint32 flags)
{
	rrm_nbr_rep_t *nbr_rep;
	wlc_info_t *wlc;
	uint16 nbr_rep_cnt;
	rrm_scb_info_t *scb_info;
	uint32 apsd = 0;

	scb_info = RRM_SCB_INFO(rrm_info, scb);

	wlc = rrm_info->wlc;

	/* critical fix */
	if (!VALID_NBR_CHAN(rmrep_bcn->channel)) {
		return BCME_BADCHAN;
	}

	if ((flags & WLC_BSS_BRCM) && (flags & WLC_BSS_WME)) {
		apsd = DOT11_NGBR_BI_CAP_APSD; /* APSD is enabled by default if wme is enabled */
	}
	else {
		apsd = ((bcn->capability & DOT11_CAP_APSD)? DOT11_NGBR_BI_CAP_APSD : 0);
	}
	/* Find Neighbor Report element from list */
	nbr_rep = scb_info->nbr_rep_head;
	while (nbr_rep) {
		if (memcmp(&nbr_rep->nbr_elt.bssid, &rmrep_bcn->bssid, ETHER_ADDR_LEN) == 0)
			break;
		nbr_rep = nbr_rep->next;
	}

	if (nbr_rep) {
		nbr_rep->nbr_elt.id = DOT11_MNG_NEIGHBOR_REP_ID;
		nbr_rep->nbr_elt.len = DOT11_NEIGHBOR_REP_IE_FIXED_LEN +
			TLV_HDR_LEN + DOT11_NGBR_BSSTRANS_PREF_SE_LEN;
		memcpy(&nbr_rep->nbr_elt.bssid, &rmrep_bcn->bssid, ETHER_ADDR_LEN);

		nbr_rep->nbr_elt.ssid.SSID_len = scb->bsscfg->SSID_len;
		memcpy(&nbr_rep->nbr_elt.ssid.SSID, &scb->bsscfg->SSID,
			nbr_rep->nbr_elt.ssid.SSID_len);

		nbr_rep->nbr_elt.bssid_info = DOT11_NGBR_BI_REACHABILTY | DOT11_NGBR_BI_SEC |
			((bcn->capability & DOT11_CAP_SPECTRUM)? DOT11_NGBR_BI_CAP_SPEC_MGMT : 0) |
			((flags & WLC_BSS_WME)? DOT11_NGBR_BI_CAP_QOS : 0) |
			(apsd) |
			((bcn->capability & DOT11_CAP_RRM)? DOT11_NGBR_BI_CAP_RDIO_MSMT : 0) |
			((bcn->capability & DOT11_CAP_DELAY_BA)? DOT11_NGBR_BI_CAP_DEL_BA : 0) |
			((bcn->capability & DOT11_CAP_IMMEDIATE_BA)? DOT11_NGBR_BI_CAP_IMM_BA : 0);

		if (htie != NULL)
			nbr_rep->nbr_elt.bssid_info |= DOT11_NGBR_BI_HT;

		if (mdie != NULL)
			nbr_rep->nbr_elt.bssid_info |= DOT11_NGBR_BI_MOBILITY;

		nbr_rep->nbr_elt.reg = rmrep_bcn->reg;
		nbr_rep->nbr_elt.channel = rmrep_bcn->channel;
		nbr_rep->nbr_elt.phytype = (rmrep_bcn->frame_info >> 1);
		nbr_rep->nbr_elt.bss_trans_preference = rmrep_bcn->rsni; /* fix */
	}
	else {
		nbr_rep_cnt = wlc_rrm_get_bcnnbr_count(rrm_info, scb_info);
		if (nbr_rep_cnt >= MAX_BEACON_NEIGHBORS) {
			WL_ERROR(("%s nbr_rep_cnt: %d is over MAX_BEACON_NEIGHBORS\n",
				__FUNCTION__, nbr_rep_cnt));
			return BCME_ERROR;
		}
		nbr_rep = (rrm_nbr_rep_t *)MALLOCZ(wlc->osh, sizeof(rrm_nbr_rep_t));
		if (nbr_rep == NULL) {
			WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__,
				(int)sizeof(rrm_nbr_rep_t), MALLOCED(wlc->osh)));
			return BCME_NOMEM;
		}

		nbr_rep->nbr_elt.id = DOT11_MNG_NEIGHBOR_REP_ID;
		nbr_rep->nbr_elt.len = DOT11_NEIGHBOR_REP_IE_FIXED_LEN +
			TLV_HDR_LEN + DOT11_NGBR_BSSTRANS_PREF_SE_LEN;
		memcpy(&nbr_rep->nbr_elt.bssid, &rmrep_bcn->bssid, ETHER_ADDR_LEN);

		nbr_rep->nbr_elt.ssid.SSID_len = scb->bsscfg->SSID_len;
		memcpy(&nbr_rep->nbr_elt.ssid.SSID, &scb->bsscfg->SSID,
			nbr_rep->nbr_elt.ssid.SSID_len);

		nbr_rep->nbr_elt.bssid_info = DOT11_NGBR_BI_REACHABILTY | DOT11_NGBR_BI_SEC |
			((bcn->capability & DOT11_CAP_SPECTRUM)? DOT11_NGBR_BI_CAP_SPEC_MGMT : 0) |
			((flags & WLC_BSS_WME)? DOT11_NGBR_BI_CAP_QOS : 0) |
			(apsd) |
			((bcn->capability & DOT11_CAP_RRM)? DOT11_NGBR_BI_CAP_RDIO_MSMT : 0) |
			((bcn->capability & DOT11_CAP_DELAY_BA)? DOT11_NGBR_BI_CAP_DEL_BA : 0) |
			((bcn->capability & DOT11_CAP_IMMEDIATE_BA)? DOT11_NGBR_BI_CAP_IMM_BA : 0);

		if (htie != NULL)
			nbr_rep->nbr_elt.bssid_info |= DOT11_NGBR_BI_HT;

		if (mdie != NULL)
			nbr_rep->nbr_elt.bssid_info |= DOT11_NGBR_BI_MOBILITY;

		nbr_rep->nbr_elt.reg = rmrep_bcn->reg;
		nbr_rep->nbr_elt.channel = rmrep_bcn->channel;
		nbr_rep->nbr_elt.phytype = (rmrep_bcn->frame_info >> 1);
		nbr_rep->nbr_elt.bss_trans_preference = rmrep_bcn->rsni; /* fix */

		nbr_rep->next = scb_info->nbr_rep_head;
		scb_info->nbr_rep_head = nbr_rep;
	}

	return BCME_OK;
}
#endif /* WL11K_BCN_MEAS */

static void
wlc_rrm_flush_bcnnbrs(wlc_rrm_info_t *rrm_info, struct scb *scb)
{
	rrm_nbr_rep_t *nbr_rep, *nbr_rep_next;
	rrm_scb_info_t *scb_info = NULL;

	scb_info = RRM_SCB_INFO(rrm_info, scb);
	if (scb_info != NULL) {
		nbr_rep = scb_info->nbr_rep_head;
		while (nbr_rep) {
			nbr_rep_next = nbr_rep->next;
			MFREE(rrm_info->wlc->osh, nbr_rep, sizeof(rrm_nbr_rep_t));
			nbr_rep = nbr_rep_next;
		}
		scb_info->nbr_rep_head = NULL;
	}
}

void
wlc_rrm_add_timer(wlc_info_t *wlc, struct scb *scb)
{
	rrm_bsscfg_cubby_t *rrm_cfg;
	wlc_bsscfg_t *bsscfg = scb->bsscfg;

	rrm_cfg = RRM_BSSCFG_CUBBY(wlc->rrm_info, bsscfg);
	if (rrm_cfg->rrm_timer_set)
		return;
	wl_add_timer(wlc->wl, wlc->rrm_info->rrm_timer, rrm_cfg->bcn_req_timer, 0);
	rrm_cfg->rrm_timer_set = TRUE;
}

/* called from ap assocreq_done function via scb state notif */
static void
wlc_rrm_ap_scb_state_upd(void *ctx, scb_state_upd_data_t *data)
{
	wlc_rrm_info_t *rrm_info = (wlc_rrm_info_t *)ctx;
	struct scb *scb = data->scb;
	wlc_bsscfg_t *cfg = scb->bsscfg;
	BCM_REFERENCE(cfg);

	if (!WL11K_ENAB(rrm_info->wlc->pub) || !BSSCFG_AP(cfg)) {
		return;
	}

	/* hndl transition from unassoc to assoc */
	if (!(data->oldstate & ASSOCIATED) && SCB_ASSOCIATED(scb) &&
		SCB_RRM(scb)) {
		wlc_rrm_add_timer(rrm_info->wlc, scb);
	}
}
#endif /* WL11K_AP */
#endif /* WL11K_NBR_MEAS */

#ifdef WL_FTM_11K
#if defined(WL_PROXDETECT) && defined(WL_FTM)
static uint8 *
wlc_rrm_create_lci_meas_element(uint8 *ptr, rrm_bsscfg_cubby_t *rrm_cfg,
	rrm_nbr_rep_t *nbr_rep, int *buflen, uint8 flags)
{
	dot11_meas_rep_t *lci_rep = (dot11_meas_rep_t *) ptr;
	uint copy_len;

	if (*buflen < DOT11_RM_IE_LEN) {
		WL_ERROR(("%s: buf too small; buflen %d; leaving off LCI\n",
			__FUNCTION__, *buflen));
		goto done;
	}
	lci_rep->id = DOT11_MNG_MEASURE_REPORT_ID;
	lci_rep->type = DOT11_MEASURE_TYPE_LCI;
	lci_rep->mode = 0;
	lci_rep->token = rrm_cfg->lci_token;

	ptr += DOT11_RM_IE_LEN;
	*buflen -= DOT11_RM_IE_LEN;

	if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_LCIM)) {
		/* Send incapable */
		lci_rep->mode |= DOT11_MEASURE_MODE_INCAPABLE;
		lci_rep->len = DOT11_MNG_IE_MREP_FIXED_LEN;
		goto done;
	}

	if (nbr_rep->opt_lci != NULL) {
		dot11_meas_rep_t *opt_lci_rep = (dot11_meas_rep_t *) nbr_rep->opt_lci;
		copy_len = opt_lci_rep->len - (DOT11_RM_IE_LEN - TLV_HDR_LEN);

		if (*buflen < copy_len) {
			WL_ERROR(("%s: buf too small; buflen %d, need %d; leaving off LCI\n",
				__FUNCTION__, *buflen, copy_len));
			ptr -= DOT11_RM_IE_LEN;
			*buflen += DOT11_RM_IE_LEN;
			goto done;
		}

		memcpy(ptr, nbr_rep->opt_lci+DOT11_RM_IE_LEN, copy_len);
		lci_rep->len = opt_lci_rep->len;
		ptr += copy_len;
		*buflen -= copy_len;
	} else {
		if (*buflen < DOT11_MNG_IE_MREP_LCI_FIXED_LEN - DOT11_MNG_IE_MREP_FIXED_LEN) {
			WL_ERROR(("%s: buf too small; buflen %d, need %d; leaving off LCI\n",
				__FUNCTION__, *buflen,
				DOT11_MNG_IE_MREP_LCI_FIXED_LEN - DOT11_MNG_IE_MREP_FIXED_LEN));
			ptr -= DOT11_RM_IE_LEN;
			*buflen += DOT11_RM_IE_LEN;
			goto done;
		}
		/* Send unknown */
		lci_rep->rep.lci.subelement = DOT11_FTM_LCI_SUBELEM_ID;
		lci_rep->rep.lci.length = 0;
		lci_rep->len = DOT11_MNG_IE_MREP_LCI_FIXED_LEN;

		ptr += (DOT11_MNG_IE_MREP_LCI_FIXED_LEN - TLV_HDR_LEN);
		*buflen -= (DOT11_MNG_IE_MREP_LCI_FIXED_LEN - TLV_HDR_LEN);
	}

	/* if sending self LCI, need to add colocated bssid subelem */
	if (flags & RRM_NREQ_FLAGS_COLOCATED_BSSID &&
	    *buflen >= (DOT11_LCI_COLOCATED_BSSID_LIST_FIXED_LEN + (ETHER_ADDR_LEN * 2))) {
		uint8 *colocp = wlc_rrm_create_colocatedbssid_ie(rrm_cfg->rrm_info,
			ptr, buflen);
		lci_rep->len += (colocp - ptr);
		ptr = colocp;
	}

done:
	return ptr;
}

static uint8 *
wlc_rrm_create_civic_meas_element(uint8 *ptr, rrm_bsscfg_cubby_t *rrm_cfg,
	rrm_nbr_rep_t *nbr_rep, int *buflen, uint8 flags)
{
	dot11_meas_rep_t *civic_rep = (dot11_meas_rep_t *) ptr;
	uint copy_len;

	if (*buflen < DOT11_RM_IE_LEN) {
		WL_ERROR(("%s: buf too small; buflen %d; leaving off CIVIC\n",
			__FUNCTION__, *buflen));
		goto done;
	}
	civic_rep->id = DOT11_MNG_MEASURE_REPORT_ID;
	civic_rep->type = DOT11_MEASURE_TYPE_CIVICLOC;
	civic_rep->mode = 0;
	civic_rep->token = rrm_cfg->civic_token;

	ptr += DOT11_RM_IE_LEN;
	*buflen -= DOT11_RM_IE_LEN;

	if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_CIVIC_LOC)) {
		civic_rep->mode |= DOT11_MEASURE_MODE_INCAPABLE;
		civic_rep->len = DOT11_MNG_IE_MREP_FIXED_LEN;
		goto done;
	}

	if (nbr_rep->opt_civic != NULL) {
		dot11_meas_rep_t *opt_civic_rep = (dot11_meas_rep_t *) nbr_rep->opt_civic;
		copy_len = opt_civic_rep->len - (DOT11_RM_IE_LEN - TLV_HDR_LEN);

		if (*buflen < copy_len) {
			WL_ERROR(("%s: buf too small; buflen %d, need %d; leaving off CIVIC\n",
				__FUNCTION__, *buflen, copy_len));
			ptr -= DOT11_RM_IE_LEN;
			*buflen += DOT11_RM_IE_LEN;
			goto done;
		}

		memcpy(ptr, nbr_rep->opt_civic+DOT11_RM_IE_LEN, copy_len);
		civic_rep->len = opt_civic_rep->len;
		ptr += copy_len;
		*buflen -= copy_len;
	} else {
		if (*buflen < DOT11_MNG_IE_MREP_CIVIC_FIXED_LEN - DOT11_MNG_IE_MREP_FIXED_LEN) {
			WL_ERROR(("%s: buf too small; buflen %d, need %d; leaving off CIVIC\n",
				__FUNCTION__, *buflen,
				DOT11_MNG_IE_MREP_CIVIC_FIXED_LEN - DOT11_MNG_IE_MREP_FIXED_LEN));
			ptr -= DOT11_RM_IE_LEN;
			*buflen += DOT11_RM_IE_LEN;
			goto done;
		}
		/* Send unknown */
		civic_rep->rep.civic.type = DOT11_FTM_CIVIC_LOC_TYPE_RFC4776;
		civic_rep->rep.civic.subelement = DOT11_FTM_CIVIC_SUBELEM_ID;
		civic_rep->rep.civic.length = 0;
		civic_rep->len = DOT11_MNG_IE_MREP_CIVIC_FIXED_LEN;

		ptr += (DOT11_MNG_IE_MREP_CIVIC_FIXED_LEN - TLV_HDR_LEN);
		*buflen -= (DOT11_MNG_IE_MREP_CIVIC_FIXED_LEN - TLV_HDR_LEN);
	}
	/* if sending self Civic without LCI, need to add colocated bssid subelem */
	if (flags & RRM_NREQ_FLAGS_COLOCATED_BSSID &&
	    !(flags & RRM_NREQ_FLAGS_LCI_PRESENT) &&
	    *buflen >= (DOT11_LCI_COLOCATED_BSSID_LIST_FIXED_LEN + (ETHER_ADDR_LEN * 2))) {
		uint8 *colocp = wlc_rrm_create_colocatedbssid_ie(rrm_cfg->rrm_info,
			ptr, buflen);
		civic_rep->len += (colocp - ptr);
		ptr = colocp;
	}
done:
	return ptr;
}

/* rrm_frng SET iovar */
static int
rrm_frng_set(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	void *p, uint plen, void *a, int alen)
{
	int err = BCME_BADARG;
	rrm_frng_fn_t fn = NULL;
	wl_rrm_frng_ioc_t *rrm_cfg_cmd = (wl_rrm_frng_ioc_t *)a;

	if ((a == NULL) || (alen < WL_RRM_FRNG_MIN_LENGTH)) {
		return BCME_BUFTOOSHORT;
	}

	if (rrm_cfg_cmd->id <= WL_RRM_FRNG_NONE ||
		rrm_cfg_cmd->id >= (sizeof(rrmfrng_cmd_fn) / sizeof(rrm_frng_fn_t)))
		return BCME_BADARG;

	fn = rrmfrng_cmd_fn[rrm_cfg_cmd->id];
	if (fn) {
		err = fn(rrm_info, cfg, &rrm_cfg_cmd->data[0], (int)rrm_cfg_cmd->len);
	}

	return err;
}
#endif /* WL_PROXDETECT && WL_FTM */
#endif /* WL_FTM_11K */

#ifdef WL11K_ALL_MEAS
/* rrm_config GET iovar */
static int
rrm_config_get(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	void *p, uint plen, void *a, int alen)
{
	int err = BCME_BADARG;
	rrm_config_fn_t fn = NULL;
	wl_rrm_config_ioc_t *param = (wl_rrm_config_ioc_t *)p;

	if ((a == NULL) || (alen < WL_RRM_CONFIG_MIN_LENGTH)) {
		return BCME_BUFTOOSHORT;
	}
	if ((p == NULL) || (plen < WL_RRM_CONFIG_MIN_LENGTH)) {
		return BCME_BADARG;
	}

	if (param->id <= WL_RRM_CONFIG_NONE ||
		param->id >= (sizeof(rrmconfig_cmd_fn) / sizeof(rrm_config_fn_t)))
		return BCME_BADARG;

	fn = rrmconfig_cmd_fn[param->id];
	if (fn) {
		err = fn(rrm_info, cfg, a, alen);
	}

	return err;
}

/* rrm_config SET iovar */
static int
rrm_config_set(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	void *p, uint plen, void *a, int alen)
{
	int err = BCME_BADARG;
	rrm_config_fn_t fn = NULL;
	wl_rrm_config_ioc_t *rrm_cfg_cmd = (wl_rrm_config_ioc_t *)a;

	if ((a == NULL) || (alen < WL_RRM_CONFIG_MIN_LENGTH)) {
		return BCME_BUFTOOSHORT;
	}
	if (alen > (TLV_BODY_LEN_MAX + WL_RRM_CONFIG_MIN_LENGTH -
		DOT11_MNG_IE_MREP_FIXED_LEN)) {
		return BCME_BUFTOOLONG;
	}

	if (rrm_cfg_cmd->id <= WL_RRM_CONFIG_NONE ||
		rrm_cfg_cmd->id >= (sizeof(rrmconfig_cmd_fn) / sizeof(rrm_config_fn_t)))
		return BCME_BADARG;

	fn = rrmconfig_cmd_fn[rrm_cfg_cmd->id];
	if (fn) {
		err = fn(rrm_info, cfg, &rrm_cfg_cmd->data[0], (int)rrm_cfg_cmd->len);
	}

	return err;
}

static int
wlc_rrm_get_self_data(dot11_meas_rep_t *rep, wl_rrm_config_ioc_t *rrm_cfg_cmd, int alen)
{
	int err = BCME_OK;

	if (alen >= (rep->len + WL_RRM_CONFIG_MIN_LENGTH -
		DOT11_MNG_IE_MREP_FIXED_LEN)) {
		memcpy(&rrm_cfg_cmd->data[0], &rep->rep.data[0],
			(rep->len - DOT11_MNG_IE_MREP_FIXED_LEN));
		rrm_cfg_cmd->len = rep->len - DOT11_MNG_IE_MREP_FIXED_LEN;
	} else {
		err = BCME_BUFTOOSHORT;
	}
	return err;
}

static int
wlc_rrm_get_self_lci(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	uint8 *a, int alen)
{
	wl_rrm_config_ioc_t *rrm_cfg_cmd = (wl_rrm_config_ioc_t *)a; /* output */
	dot11_meas_rep_t *lci_rep;
	rrm_bsscfg_cubby_t *rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, cfg);
	int32 buflen = alen;
	uint8 *data_ptr;
	uint8 *colocp;

	if (rrm_cfg->nbr_rep_self && rrm_cfg->nbr_rep_self->opt_lci) {
		lci_rep = (dot11_meas_rep_t *)rrm_cfg->nbr_rep_self->opt_lci;
		if (wlc_rrm_get_self_data(lci_rep, rrm_cfg_cmd, alen) == BCME_OK) {
			/* if sending self LCI, need to add colocated bssid subelem */
			data_ptr = (uint8 *) &rrm_cfg_cmd->data[0] +
				(lci_rep->len - DOT11_MNG_IE_MREP_FIXED_LEN);
			buflen -= (lci_rep->len + WL_RRM_CONFIG_MIN_LENGTH -
				DOT11_MNG_IE_MREP_FIXED_LEN);
			colocp = wlc_rrm_create_colocatedbssid_ie(rrm_info, data_ptr, &buflen);
			lci_rep->len += (colocp - data_ptr);
			rrm_cfg_cmd->len += (colocp - data_ptr);
		}

	} else {
		/* set output to unknown LCI */
		rrm_cfg_cmd->len = DOT11_FTM_LCI_UNKNOWN_LEN;
		memset(&rrm_cfg_cmd->data[0], 0, DOT11_FTM_LCI_UNKNOWN_LEN);
	}

	return BCME_OK;
}

static int
wlc_rrm_get_self_civic(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	uint8 *a, int alen)
{
	wl_rrm_config_ioc_t *rrm_cfg_cmd = (wl_rrm_config_ioc_t *)a; /* output */
	dot11_meas_rep_t *civic_rep;
	rrm_bsscfg_cubby_t *rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, cfg);
	int32 buflen = alen;
	uint8 *data_ptr;
	uint8 *colocp;

	if (rrm_cfg->nbr_rep_self && rrm_cfg->nbr_rep_self->opt_civic) {
		civic_rep = (dot11_meas_rep_t *)rrm_cfg->nbr_rep_self->opt_civic;
		if (wlc_rrm_get_self_data(civic_rep, rrm_cfg_cmd, alen) == BCME_OK &&
		    rrm_cfg->nbr_rep_self->opt_lci == NULL) {
			/* if sending self Civic without LCI, need to add colocated bssid subelem */
			data_ptr = (uint8 *) &rrm_cfg_cmd->data[0] +
				(civic_rep->len - DOT11_MNG_IE_MREP_FIXED_LEN);
			buflen -= (civic_rep->len + WL_RRM_CONFIG_MIN_LENGTH -
				DOT11_MNG_IE_MREP_FIXED_LEN);
			colocp = wlc_rrm_create_colocatedbssid_ie(rrm_info, data_ptr, &buflen);
			civic_rep->len += (colocp - data_ptr);
			rrm_cfg_cmd->len += (colocp - data_ptr);
		}
	} else {
		/* set output to unknown civic */
		rrm_cfg_cmd->len = DOT11_FTM_CIVIC_UNKNOWN_LEN;
		memset(&rrm_cfg_cmd->data[0], 0, DOT11_FTM_CIVIC_UNKNOWN_LEN);
	}

	return BCME_OK;
}

static int
wlc_rrm_get_self_locid(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	uint8 *a, int alen)
{
	wl_rrm_config_ioc_t *rrm_cfg_cmd = (wl_rrm_config_ioc_t *)a; /* output */
	dot11_meas_rep_t *locid_rep;
	rrm_bsscfg_cubby_t *rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, cfg);

	if (rrm_cfg->nbr_rep_self && rrm_cfg->nbr_rep_self->opt_locid) {
		locid_rep = (dot11_meas_rep_t *)rrm_cfg->nbr_rep_self->opt_locid;
		return wlc_rrm_get_self_data(locid_rep, rrm_cfg_cmd, alen);
	} else {
		/* set output to unknown locid */
		rrm_cfg_cmd->len = DOT11_LOCID_UNKNOWN_LEN;
		memset(&rrm_cfg_cmd->data[0], 0, DOT11_LOCID_UNKNOWN_LEN);
	}

	return BCME_OK;
}

static uint8 *
wlc_rrm_create_colocatedbssid_ie(wlc_rrm_info_t *rrm_info,
	uint8 *data_ptr, int *buflen)
{
	uint32 colocated_bss_sublen = 0;
	uint16 i;
	wlc_bsscfg_t *bsscfg;
	dot11_colocated_bssid_list_se_t *colocated_bssid_se;

	uint32 bss_cnt = wlc_ap_bss_up_count(rrm_info->wlc);
	if (!MBSS_ENAB(rrm_info->wlc->pub) || bss_cnt <= 1) {
		goto done;
	}

	colocated_bss_sublen = DOT11_LCI_COLOCATED_BSSID_LIST_FIXED_LEN;
	colocated_bss_sublen += bss_cnt * ETHER_ADDR_LEN;

	if (*buflen < colocated_bss_sublen) {
		goto done;
	}

	colocated_bssid_se = (dot11_colocated_bssid_list_se_t *) data_ptr;

	colocated_bssid_se->sub_id = DOT11_LCI_COLOCATED_BSSID_SUBELEM_ID;
	colocated_bssid_se->length = colocated_bss_sublen - TLV_HDR_LEN;
	colocated_bssid_se->max_bssid_ind = 0;
	data_ptr = (uint8 *)colocated_bssid_se->bssid;
	*buflen -= DOT11_LCI_COLOCATED_BSSID_LIST_FIXED_LEN;

	FOREACH_UP_AP(rrm_info->wlc, i, bsscfg) {
		memcpy(data_ptr, &bsscfg->BSSID, ETHER_ADDR_LEN);
		data_ptr += ETHER_ADDR_LEN;
		*buflen -= ETHER_ADDR_LEN;
	}

done:
	return data_ptr;
}

/*
 * 'len' param only includes what comes after the "type" field
 * of the measurement report.
*/
static int
wlc_rrm_add_loc(wlc_rrm_info_t *rrm_info, uint8 **loc,
	uint8 *new_loc, int len, uint8 type)
{
	dot11_meas_rep_t *rep;

	if (*loc) {
		rep = (dot11_meas_rep_t *) *loc;
		MFREE(rrm_info->wlc->osh, rep, rep->len + TLV_HDR_LEN);
		*loc = NULL;
		rep = NULL;
	}

	/* add 3 bytes for token, mode and type */
	len += DOT11_MNG_IE_MREP_FIXED_LEN;
	/* malloc enough to store the total measurement report starting with "meas rep id" */
	*loc = MALLOCZ(rrm_info->wlc->osh, (TLV_HDR_LEN + len));

	if (*loc != NULL) {
		rep = (dot11_meas_rep_t *)(*loc);

		rep->id = DOT11_MNG_MEASURE_REPORT_ID;
		rep->len = len;
		rep->type = type;

		switch (type) {
		case DOT11_MEASURE_TYPE_LCI:
			if (len == DOT11_MNG_IE_MREP_LCI_FIXED_LEN) {
				/* Use unknown for empty config */
				rep->rep.lci.subelement = DOT11_FTM_LCI_SUBELEM_ID;
				rep->rep.lci.length = 0;
			} else {
				memcpy(&rep->rep.lci.subelement, new_loc,
					(len - DOT11_MNG_IE_MREP_FIXED_LEN));
			}
			break;
		case DOT11_MEASURE_TYPE_CIVICLOC:
			if (len == DOT11_MNG_IE_MREP_CIVIC_FIXED_LEN) {
				/* Use unknown for empty config */
				rep->rep.civic.type = DOT11_FTM_CIVIC_LOC_TYPE_RFC4776;
				rep->rep.civic.subelement = DOT11_FTM_CIVIC_SUBELEM_ID;
				rep->rep.civic.length = 0;
			} else {
				memcpy(&rep->rep.civic.type, new_loc,
					(len - DOT11_MNG_IE_MREP_FIXED_LEN));
			}
			break;
		case DOT11_MEASURE_TYPE_LOC_ID:
			if (len == DOT11_MNG_IE_MREP_LOCID_FIXED_LEN) {
				/* Use unknown for empty config */
				rep->rep.locid.subelement = DOT11_LOCID_SUBELEM_ID;
				rep->rep.locid.length = 0;
			} else {
				memcpy(rep->rep.locid.exp_tsf, new_loc,
					(len - DOT11_MNG_IE_MREP_FIXED_LEN));
			}
			break;
		default:
			WL_ERROR(("%s: Unknown Location type: %d\n", __FUNCTION__, type));
			return BCME_ERROR;
		}
		return BCME_OK;
	} else {
		WL_ERROR(("%s: out of memory, malloced %d bytes\n",
			__FUNCTION__, MALLOCED(rrm_info->wlc->osh)));
		return BCME_NOMEM;
	}
}

static int
wlc_rrm_alloc_nbr_rep_self(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg)
{
	rrm_bsscfg_cubby_t *rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, cfg);
	rrm_nbr_rep_t *nbr_rep = rrm_cfg->nbr_rep_self;

	if (nbr_rep == NULL) {
		nbr_rep = (rrm_nbr_rep_t *)MALLOCZ(rrm_info->wlc->osh, sizeof(rrm_nbr_rep_t));
		if (nbr_rep == NULL)
			return BCME_NOMEM;
		rrm_cfg->nbr_rep_self = nbr_rep;
	}

	return BCME_OK;
}

static int
wlc_rrm_set_self_lci(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	uint8 *opt_lci, int len)
{
	int ret = BCME_OK;
	rrm_bsscfg_cubby_t *rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, cfg);
	wlc_info_t *wlc = rrm_info->wlc;

	if (len < DOT11_FTM_LCI_UNKNOWN_LEN) {
		return BCME_BUFTOOSHORT;
	}

	ret = wlc_rrm_alloc_nbr_rep_self(rrm_info, cfg);

	if (ret == BCME_OK)
		ret = wlc_rrm_add_loc(rrm_info, &rrm_cfg->nbr_rep_self->opt_lci,
			opt_lci, len, DOT11_MEASURE_TYPE_LCI);

	if (ret != BCME_OK) {
		WL_ERROR(("%s: Failed to create self LCI of len %d, status %d\n",
			__FUNCTION__, len, ret));
	}
	else {
		wlc_bsscfg_set_ext_cap(cfg, DOT11_EXT_CAP_LCI, (len != DOT11_FTM_LCI_UNKNOWN_LEN));
		if (len != DOT11_FTM_LCI_UNKNOWN_LEN)	{
			setbit(rrm_cfg->rrm_cap, DOT11_RRM_CAP_LCIM);
		} else {
			clrbit(rrm_cfg->rrm_cap, DOT11_RRM_CAP_LCIM);
		}
		wlc_rrm_update_beacon(wlc, cfg);
	}

	return ret;
}

static int
wlc_rrm_set_self_civic(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	uint8 *opt_civic, int len)
{
	int ret = BCME_OK;
	rrm_bsscfg_cubby_t *rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, cfg);
	wlc_info_t *wlc = rrm_info->wlc;

	if (len < DOT11_FTM_CIVIC_UNKNOWN_LEN) {
		return BCME_BUFTOOSHORT;
	}

	ret = wlc_rrm_alloc_nbr_rep_self(rrm_info, cfg);

	if (ret == BCME_OK)
		ret = wlc_rrm_add_loc(rrm_info, &rrm_cfg->nbr_rep_self->opt_civic,
			opt_civic, len, DOT11_MEASURE_TYPE_CIVICLOC);

	if (ret != BCME_OK) {
		WL_ERROR(("%s: Failed to create self civic, len %d, status %d\n",
			__FUNCTION__, len, ret));
	}
	else {
		wlc_bsscfg_set_ext_cap(cfg, DOT11_EXT_CAP_CIVIC_LOC,
			(len != DOT11_FTM_CIVIC_UNKNOWN_LEN));
		if (len != DOT11_FTM_CIVIC_UNKNOWN_LEN) {
			setbit(rrm_cfg->rrm_cap, DOT11_RRM_CAP_CIVIC_LOC);
		} else {
			clrbit(rrm_cfg->rrm_cap, DOT11_RRM_CAP_CIVIC_LOC);
		}

		wlc_rrm_update_beacon(wlc, cfg);
	}

	return ret;
}

static int
wlc_rrm_set_self_locid(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	uint8 *opt_locid, int len)
{
	int ret = BCME_OK;
	rrm_bsscfg_cubby_t *rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, cfg);
	wlc_info_t *wlc = rrm_info->wlc;

	if (len < DOT11_LOCID_UNKNOWN_LEN) {
		return BCME_BUFTOOSHORT;
	}

	ret = wlc_rrm_alloc_nbr_rep_self(rrm_info, cfg);

	if (ret == BCME_OK)
		ret = wlc_rrm_add_loc(rrm_info, &rrm_cfg->nbr_rep_self->opt_locid,
			opt_locid, len, DOT11_MEASURE_TYPE_LOC_ID);

	if (ret != BCME_OK) {
		WL_ERROR(("%s: Failed to create self locid, len %d, status %d\n",
			__FUNCTION__, len, ret));
	}
	else {
		wlc_bsscfg_set_ext_cap(cfg, DOT11_EXT_CAP_IDENT_LOC,
			(len != DOT11_LOCID_UNKNOWN_LEN));
		if (len != DOT11_LOCID_UNKNOWN_LEN) {
			setbit(rrm_cfg->rrm_cap, DOT11_RRM_CAP_IDENT_LOC);
		} else {
			clrbit(rrm_cfg->rrm_cap, DOT11_RRM_CAP_IDENT_LOC);
		}

		wlc_rrm_update_beacon(wlc, cfg);
	}

	return ret;
}

static void
wlc_rrm_free_self_report(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *bsscfg)
{
	rrm_bsscfg_cubby_t *rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, bsscfg);
	rrm_nbr_rep_t *nbr_rep = rrm_cfg->nbr_rep_self;
	dot11_meas_rep_t *rep;

	if (nbr_rep) {
		if (nbr_rep->opt_lci) {
			rep = (dot11_meas_rep_t *) nbr_rep->opt_lci;
			MFREE(rrm_info->wlc->osh, rep,
				rep->len + TLV_HDR_LEN);
		}
		if (nbr_rep->opt_civic) {
			rep = (dot11_meas_rep_t *) nbr_rep->opt_civic;
			MFREE(rrm_info->wlc->osh, rep,
				rep->len + TLV_HDR_LEN);
		}
		if (nbr_rep->opt_locid) {
			rep = (dot11_meas_rep_t *) nbr_rep->opt_locid;
			MFREE(rrm_info->wlc->osh, rep,
				rep->len + TLV_HDR_LEN);
		}
		MFREE(rrm_info->wlc->osh, nbr_rep, sizeof(rrm_nbr_rep_t));
		rrm_cfg->nbr_rep_self = NULL;
	}

	return;
}

static void
wlc_rrm_recv_lcireq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	dot11_meas_req_loc_t *rmreq_lci, wlc_rrm_req_t *req)
{
	wlc_info_t *wlc = rrm_info->wlc;
	rrm_bsscfg_cubby_t *rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, rrm_info->cur_cfg);
	BCM_REFERENCE(wlc); /* in case BCMDBG off */

	if ((rmreq_lci->mode & (DOT11_RMREQ_MODE_PARALLEL | DOT11_RMREQ_MODE_ENABLE)) ||
		!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_LCIM)) {
		WL_ERROR(("wl%d: %s: Unsupported LCI request; mode 0x%x\n",
			wlc->pub->unit, __FUNCTION__, rmreq_lci->mode));
		req->flags |= DOT11_RMREP_MODE_INCAPABLE;
		return;
	}

	if (rmreq_lci->req.lci.subject != DOT11_FTM_LOCATION_SUBJ_REMOTE) {
		WL_ERROR(("wl%d: %s: Unsupported LCI request subject %d\n",
			wlc->pub->unit, __FUNCTION__, rmreq_lci->req.lci.subject));
		req->flags |= DOT11_RMREP_MODE_INCAPABLE;
		return;
	}

}

static void
wlc_rrm_recv_civicreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	dot11_meas_req_loc_t *rmreq_civic, wlc_rrm_req_t *req)
{
	wlc_info_t *wlc = rrm_info->wlc;
	BCM_REFERENCE(wlc); /* in case BCMDBG off */

	if ((rmreq_civic->mode & (DOT11_RMREQ_MODE_PARALLEL | DOT11_RMREQ_MODE_ENABLE)) ||
		!isset(cfg->ext_cap, DOT11_EXT_CAP_CIVIC_LOC)) {
		WL_ERROR(("wl%d: %s: Unsupported civic location request; mode 0x%x\n",
			wlc->pub->unit, __FUNCTION__, rmreq_civic->mode));
		req->flags |= DOT11_RMREP_MODE_INCAPABLE;
		return;
	}

	if (rmreq_civic->req.civic.subject != DOT11_FTM_LOCATION_SUBJ_REMOTE) {
		WL_ERROR(("wl%d: %s: Unsupported civic location request subject %d\n",
			wlc->pub->unit, __FUNCTION__, rmreq_civic->req.civic.subject));
		req->flags |= DOT11_RMREP_MODE_INCAPABLE;
		return;
	}

}

static void
wlc_rrm_recv_locidreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	dot11_meas_req_loc_t *rmreq_locid, wlc_rrm_req_t *req)
{
	wlc_info_t *wlc = rrm_info->wlc;
	BCM_REFERENCE(wlc); /* in case BCMDBG off */

	if ((rmreq_locid->mode & (DOT11_RMREQ_MODE_PARALLEL | DOT11_RMREQ_MODE_ENABLE)) ||
		!isset(cfg->ext_cap, DOT11_EXT_CAP_IDENT_LOC)) {
		WL_ERROR(("wl%d: %s: Unsupported location identifier request; mode 0x%x\n",
			wlc->pub->unit, __FUNCTION__, rmreq_locid->mode));
		req->flags |= DOT11_RMREP_MODE_INCAPABLE;
		return;
	}

	if (rmreq_locid->req.locid.subject != DOT11_FTM_LOCATION_SUBJ_REMOTE) {
		WL_ERROR(("wl%d: %s: Unsupported location identifier request subject %d\n",
			wlc->pub->unit, __FUNCTION__, rmreq_locid->req.locid.subject));
		req->flags |= DOT11_RMREP_MODE_INCAPABLE;
		return;
	}

}
#endif /* WL11K_ALL_MEAS */

static void
wlc_rrm_update_beacon(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	if (cfg->up && (BSSCFG_AP(cfg) ||
		(!cfg->BSS && !BSS_TDLS_ENAB(wlc, cfg)))) {
		/* Update AP or IBSS beacons */
		wlc_bss_update_beacon(wlc, cfg);
		/* Update AP or IBSS probe responses */
		wlc_bss_update_probe_resp(wlc, cfg, TRUE);
	}
}

#ifdef WL_FTM_11K
#if defined(WL_PROXDETECT) && defined(WL_FTM)
#if defined(WL11AC)
static uint
wide_channel_width_to_bw(uint8 channel_width)
{
	uint bw;

	if (channel_width == WIDE_BW_CHAN_WIDTH_40)
		bw = WL_CHANSPEC_BW_40;
	else if (channel_width == WIDE_BW_CHAN_WIDTH_80)
		bw = WL_CHANSPEC_BW_80;
	else if (channel_width == WIDE_BW_CHAN_WIDTH_160)
		bw = WL_CHANSPEC_BW_160;
	else if (channel_width == WIDE_BW_CHAN_WIDTH_80_80)
		bw = WL_CHANSPEC_BW_8080;
	else
		bw = WL_CHANSPEC_BW_20;

	return bw;
}
#endif /* WL11AC */

static uint
op_channel_width_to_bw(uint8 channel_width)
{
	uint bw;

	if (channel_width == VHT_OP_CHAN_WIDTH_80)
		bw = WL_CHANSPEC_BW_80;
	else if (channel_width == VHT_OP_CHAN_WIDTH_160)
		bw = WL_CHANSPEC_BW_160;
	else if (channel_width == VHT_OP_CHAN_WIDTH_80_80)
		bw = WL_CHANSPEC_BW_8080;
	else
		bw = WL_CHANSPEC_BW_40;

	return bw;
}

static void
wlc_rrm_parse_mr_tlvs(wlc_info_t *wlc, bcm_tlv_t *mr_tlvs, int *mr_tlv_len,
        rrm_nbr_rep_t *nbr_rep, wlc_bsscfg_t *cfg)
{
	while (mr_tlvs) {
		if (mr_tlvs->id == DOT11_MNG_MEASURE_REPORT_ID) {
			dot11_meas_rep_t *meas_rep = (dot11_meas_rep_t *) mr_tlvs;
			dot11_meas_rep_t *meas_elt;

			if (meas_rep->len < DOT11_MNG_IE_MREP_FIXED_LEN) {
				WL_ERROR(("wl%d: %s: malformed element id %d with length %d\n",
					wlc->pub->unit, __FUNCTION__, meas_rep->id, meas_rep->len));
				mr_tlvs = bcm_next_tlv(mr_tlvs, mr_tlv_len);
				continue;
			}

			if ((meas_rep->type == DOT11_MEASURE_TYPE_LCI ||
				meas_rep->type == DOT11_MEASURE_TYPE_CIVICLOC) &&
				!(meas_rep->mode & DOT11_MEASURE_MODE_INCAPABLE)) {

				meas_elt = (dot11_meas_rep_t *)MALLOCZ(wlc->osh,
					meas_rep->len + TLV_HDR_LEN);

				if (meas_elt == NULL) {
					WL_ERROR(("wl%d: %s: out of memory for"
						" measurement report, malloced %d bytes\n",
						wlc->pub->unit, __FUNCTION__,
						MALLOCED(wlc->osh)));
					return;
				} else {
					memcpy((uint8 *) meas_elt, (uint8 *) meas_rep,
						meas_rep->len + TLV_HDR_LEN);
				}
			} else {
				WL_ERROR(("wl%d: %s: Received UNEXPECTED or INCAPABLE"
					" Measurement report type %d\n", wlc->pub->unit,
					__FUNCTION__, meas_rep->type));
				mr_tlvs = bcm_next_tlv(mr_tlvs, mr_tlv_len);
				continue;
			}

			if (meas_rep->type == DOT11_MEASURE_TYPE_LCI) {
				nbr_rep->opt_lci = (uint8 *) meas_elt;
			} else if (meas_rep->type == DOT11_MEASURE_TYPE_CIVICLOC) {
				nbr_rep->opt_civic = (uint8 *) meas_elt;
			}
		}
		else if (mr_tlvs->id == DOT11_MNG_HT_ADD) {
			ht_add_ie_t	*ht_op_ie;
			chanspec_t chspec;

			ht_op_ie = wlc_read_ht_add_ie(wlc, (uint8 *)mr_tlvs, (uint8)*mr_tlv_len);

			if (ht_op_ie) {
				chspec = wlc_ht_chanspec(wlc, ht_op_ie->ctl_ch,
					ht_op_ie->byte1, cfg);
				if (chspec != INVCHANSPEC) {
					nbr_rep->nbr_elt.chanspec = chspec;
					/* also set channel in case there's a descrepancy */
					nbr_rep->nbr_elt.channel = ht_op_ie->ctl_ch;
				}
			}
			else {
				WL_ERROR(("wl%d: %s: Error parsing wlc_read_ht_add_ie id: %d, "
					"len %d\n", wlc->pub->unit, __FUNCTION__,
					mr_tlvs->id, mr_tlvs->len));
			}
		}
#ifdef WL11AC
		else if (mr_tlvs->id == DOT11_MNG_VHT_OPERATION_ID) {
			vht_op_ie_t *op_p;
			vht_op_ie_t op_ie;
			wlc_vht_info_t *vhti = wlc->vhti;

			if ((op_p = wlc_read_vht_op_ie(vhti, (uint8 *)mr_tlvs,
				(uint8)*mr_tlv_len, &op_ie)) != NULL) {
				/* determine the chanspec from VHT Operational IE */
				uint bw;
				uint8 primary_channel = nbr_rep->nbr_elt.channel;

				bw = op_channel_width_to_bw(op_p->chan_width);
				if (bw == WL_CHANSPEC_BW_8080) {
					nbr_rep->nbr_elt.chanspec =
						wf_chspec_get8080_chspec(primary_channel,
							op_p->chan1, op_p->chan2);
				}
				else {
					nbr_rep->nbr_elt.chanspec =
						wf_channel2chspec(primary_channel, bw);
				}
			}
			else {
				WL_ERROR(("wl%d: %s: Error parsing wlc_read_vht_op_ie id: %d, "
					"len %d\n", wlc->pub->unit, __FUNCTION__,
					mr_tlvs->id, mr_tlvs->len));
			}
		}
		/* check for optional neighbor report subelement - wide chan bw */
		else if (mr_tlvs->id == DOT11_NGBR_WIDE_BW_CHAN_SE_ID) {
			uint bw;
			uint8 chan1;
			uint8 chan2;
			uint8 primary_channel = nbr_rep->nbr_elt.channel;
			dot11_wide_bw_chan_ie_t *wide_bw_chan_ie =
				(dot11_wide_bw_chan_ie_t *)mr_tlvs;

			chan1 = wide_bw_chan_ie->center_frequency_segment_0;
			chan2 = wide_bw_chan_ie->center_frequency_segment_1;
			bw = wide_channel_width_to_bw(wide_bw_chan_ie->channel_width);
			if (bw == WL_CHANSPEC_BW_8080) {
				nbr_rep->nbr_elt.chanspec =
					wf_chspec_get8080_chspec(primary_channel, chan1, chan2);
			}
			else {
				nbr_rep->nbr_elt.chanspec = wf_channel2chspec(primary_channel, bw);
			}
		}
#endif /* WL11AC */
		else {
			WL_ERROR(("wl%d: %s: Received UNEXPECTED Element %d, "
				"len %d\n", wlc->pub->unit, __FUNCTION__,
				mr_tlvs->id, mr_tlvs->len));
		}

		mr_tlvs = bcm_next_tlv(mr_tlvs, mr_tlv_len);
	}
}

static void
rrm_ftm_get_meas_start(wlc_info_t *wlc, wlc_ftm_t *ftm, wlc_bsscfg_t *bsscfg,
	wl_proxd_ftm_session_info_t *si, uint32 *meas_start)
{
	uint64 tsf = 0, s_tsf;
	uint32 tsf_lo = 0, tsf_hi = 0;
	uint64 meas_tsf;
	int err = BCME_OK;

	/* convert session meas start to associated bsscfg reference */
#ifdef WLMCNX
	if (MCNX_ENAB(wlc->pub) && wlc_mcnx_tbtt_valid(wlc->mcnx, bsscfg)) {
		wlc_mcnx_read_tsf64(wlc->mcnx, bsscfg, &tsf_hi, &tsf_lo);
	} else
#else
	{
		if (wlc->clk)
			wlc_read_tsf(wlc, &tsf_lo, &tsf_hi);
		else
			err = BCME_NOCLK;
	}

	if (err != BCME_OK)
		goto done;
#endif /* WLMCNX */

	tsf = (uint64)tsf_hi << 32 | (uint64)tsf_lo; /* associated bss tsf */
	err = wlc_ftm_get_session_tsf(ftm, si->sid, &s_tsf);
	if (err != BCME_OK)
		goto done;
	meas_tsf = (uint64)si->meas_start_hi << 32 | (uint64)si->meas_start_lo;
	tsf -= (s_tsf - meas_tsf); /* convert by applying delta */

done:
	store32_ua(meas_start, (uint32)(tsf & 0xffffffffULL));
	WL_TMP(("wl%d.%d: %s: status %d meas start %d.%d\n",
		wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), __FUNCTION__,
		err, (uint32)(tsf >> 32), (uint32)tsf));
}

static void
wlc_rrm_ftm_ranging_cb(wlc_info_t *wlc, wlc_ftm_t *ftm,
	wl_proxd_status_t status, wl_proxd_event_type_t event,
	wl_proxd_session_id_t sid, void *cb_ctx)
{
	wl_proxd_ranging_info_t info;
	wl_proxd_rtt_result_t res;
	wl_proxd_ftm_session_info_t session_inf;
	frngrep_t *frng_buf = NULL;
	wlc_rrm_info_t *rrm_info = (wlc_rrm_info_t *)cb_ctx;
	wlc_bsscfg_t *bsscfg;
	int err = BCME_OK;
	int i;
	wlc_ftm_ranging_ctx_t *rctx;
	uint8 err_cnt = 0;
	uint8 success_cnt = 0;
	frngreq_t *frng_req = NULL;

	if (cb_ctx == NULL) {
		return;
	}
	bsscfg = rrm_info->cur_cfg;

	if (bsscfg == NULL) {
		return;
	}

	rctx = rrm_info->rrm_frng_req->rctx;

	if (rctx == NULL) {
		WL_ERROR(("%s: rctx not valid, err %d \n", __FUNCTION__, err));
		goto done;
	}

	err = wlc_ftm_ranging_get_info(rctx, &info);
	if (err != BCME_OK)	{
		WL_ERROR(("%s: wlc_ftm_ranging_get_info failed, err %d \n", __FUNCTION__, err));
		goto done;
	}

	if (info.state != WL_PROXD_RANGING_STATE_DONE) {
		WL_ERROR(("%s: ignoring ranging state %d \n", __FUNCTION__, info.state));
		return;
	}

	frng_req = rrm_info->rrm_frng_req->frng_req;
	if (frng_req == NULL) {
		WL_ERROR(("wl%d: %s: frng_req is NULL\n",
				wlc->pub->unit, __FUNCTION__));
		return;
	}

	rrm_info->rrm_frng_req->frng_rep = (frngrep_t *) MALLOCZ(wlc->osh, sizeof(*frng_buf));
	frng_buf = rrm_info->rrm_frng_req->frng_rep;
	if (frng_buf == NULL) {
		WL_ERROR(("%s: MALLOC failed \n", __FUNCTION__));
		goto done;
	}

	frng_buf->dialog_token = rrm_info->rrm_frng_req->token;
	memcpy(&frng_buf->da, &rrm_info->da, ETHER_ADDR_LEN);
	frng_buf->range_entry_count = 0;
	frng_buf->error_entry_count = 0;

	for (i = 0; i < frng_req->num_aps; i++) {
		memset(&res, 0, sizeof(res));
		memset(&session_inf, 0, sizeof(session_inf));

		err = wlc_ftm_ranging_get_result(rctx, frng_req->targets[i].sid, 0, &res);
		if (err != BCME_OK) {
			WL_ERROR(("%s: status %d getting result for session sid %d\n",
					__FUNCTION__, err, frng_req->targets[i].sid));
			res.status = err;
		}
		err = wlc_ftm_get_session_info(ftm, frng_req->targets[i].sid, &session_inf);
		if (err != BCME_OK) {
			WL_ERROR(("%s: status %d getting session info for session sid %d\n",
					__FUNCTION__, err, frng_req->targets[i].sid));
			res.status = err;
		}

		switch (res.status)
		{
			case WL_PROXD_E_INVALIDMEAS:
			case WL_PROXD_E_OK:
				memcpy(&frng_buf->range_entries[success_cnt].bssid,
					&frng_req->targets[i].bssid, ETHER_ADDR_LEN);
				if (res.status == WL_PROXD_E_INVALIDMEAS) {
					memset(&frng_buf->range_entries[success_cnt].range, 0xFF,
						sizeof(frng_buf->range_entries[success_cnt].range));
				}
				else {
					/* avg_dist is 1/256 meters, range is 1/4096 meters */
					frng_buf->range_entries[success_cnt].range =
						res.avg_dist * 16;
				}
				memset(&frng_buf->range_entries[success_cnt].max_err, 0,
					sizeof(frng_buf->range_entries[success_cnt].max_err));
				rrm_ftm_get_meas_start(wlc, ftm, bsscfg, &session_inf,
					&frng_buf->range_entries[success_cnt].start_tsf);

				frng_buf->range_entries[success_cnt].rsvd = 0;
				frng_buf->range_entry_count++;
				success_cnt++;
				break;

			case WL_PROXD_E_REMOTE_FAIL:
			case WL_PROXD_E_REMOTE_CANCEL:
				frng_buf->error_entries[err_cnt].code =
					DOT11_FTM_RANGE_ERROR_AP_FAILED;
				break;
			case WL_PROXD_E_REMOTE_INCAPABLE:
				frng_buf->error_entries[err_cnt].code =
					DOT11_FTM_RANGE_ERROR_AP_INCAPABLE;
				break;
			default:
				frng_buf->error_entries[err_cnt].code =
					DOT11_FTM_RANGE_ERROR_TX_FAILED;
				break;
		}

		if (res.status != WL_PROXD_E_OK && res.status != WL_PROXD_E_INVALIDMEAS) {
			frng_buf->error_entries[err_cnt].start_tsf = session_inf.meas_start_lo;
			rrm_ftm_get_meas_start(wlc, ftm, bsscfg, &session_inf,
				&frng_buf->error_entries[err_cnt].start_tsf);

			memcpy(&frng_buf->error_entries[err_cnt].bssid,
				&frng_req->targets[i].bssid, ETHER_ADDR_LEN);
			frng_buf->error_entry_count++;
			err_cnt++;
		}

		WL_TMP(("%s: Results: sid %d, dist %u, status %d, state %d, num_ftm %d \n",
			__FUNCTION__, res.sid, res.avg_dist, res.status, res.state, res.num_ftm));
	}

done:
	rrm_info->rrm_state->scan_active = FALSE;

	/* continue rrm state machine */
	wl_add_timer(wlc->wl, rrm_info->rrm_timer, 0, 0);
}

static int
wlc_rrm_recv_frngreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg,
	dot11_rmreq_ftm_range_t *rmreq_ftmrange, wlc_rrm_req_t *req)
{
	wlc_info_t *wlc = rrm_info->wlc;
	int tlv_len, i, req_len;
	bcm_tlv_t *tlvs = NULL;
	dot11_neighbor_rep_ie_t *nbr_rep_ie;
	dot11_ftm_range_subel_t *frng_subel;
	rrm_nbr_rep_t *nbr_rep, *nbr_rep_next;
	rrm_nbr_rep_t *nbr_rep_head = NULL; /* Neighbor Report element */
	frngreq_t *frng_req;
	char eabuf[ETHER_ADDR_STR_LEN];
	int err = BCME_OK;

	BCM_REFERENCE(wlc); /* in case BCMDBG off */

	if (rmreq_ftmrange->mode & (DOT11_RMREQ_MODE_PARALLEL | DOT11_RMREQ_MODE_ENABLE)) {
		WL_ERROR(("wl%d: %s: Unsupported FTM range request mode 0x%x\n",
			wlc->pub->unit, __FUNCTION__, rmreq_ftmrange->mode));
		req->flags |= DOT11_RMREP_MODE_INCAPABLE;
		return BCME_UNSUPPORTED;
	}

	ASSERT(rrm_info->rrm_frng_req == NULL);

	if ((rrm_info->rrm_frng_req = (rrm_ftmreq_t *)MALLOCZ(wlc->osh,
		sizeof(rrm_ftmreq_t))) == NULL) {
		WL_ERROR(("wl%d: %s: MALLOC(%d) failed, malloced %d bytes\n", WLCWLUNIT(wlc),
			__FUNCTION__, (int)sizeof(frngreq_t), MALLOCED(wlc->osh)));
		req->flags |= DOT11_RMREP_MODE_REFUSED;
		return BCME_NOMEM;
	}

	if ((rrm_info->rrm_frng_req->frng_req = (frngreq_t *)MALLOCZ(wlc->osh,
			(sizeof(frngreq_t) + ((DOT11_FTM_RANGE_ENTRY_MAX_COUNT-1) *
			sizeof(frngreq_target_t))))) == NULL) {
		WL_ERROR(("wl%d: %s: MALLOC(%d) failed, malloced %d bytes\n", WLCWLUNIT(wlc),
			__FUNCTION__, (int)sizeof(frngreq_t), MALLOCED(wlc->osh)));
		req->flags |= DOT11_RMREP_MODE_REFUSED;
		MFREE(wlc->osh, rrm_info->rrm_frng_req, sizeof(rrm_ftmreq_t));
		rrm_info->rrm_frng_req = NULL;
		return BCME_NOMEM;
	}

	frng_req = rrm_info->rrm_frng_req->frng_req;
	rrm_info->rrm_frng_req->token = rmreq_ftmrange->token;
	frng_req->event = WL_RRM_EVENT_FRNG_REQ;
	frng_req->max_init_delay = load16_ua(&rmreq_ftmrange->max_init_delay);
	frng_req->min_ap_count = rmreq_ftmrange->min_ap_count;

	tlvs = (bcm_tlv_t *)&rmreq_ftmrange->data[0];
	if (rmreq_ftmrange->len <= DOT11_MNG_IE_MREQ_FRNG_FIXED_LEN) {
		WL_ERROR(("wl%d: %s: ftmrange len is too less \n", WLCWLUNIT(wlc),
			__FUNCTION__));
		return BCME_BADARG;
	}
	tlv_len = rmreq_ftmrange->len - DOT11_MNG_IE_MREQ_FRNG_FIXED_LEN;

	while (tlvs) {
		if (tlvs->id == DOT11_MNG_NEIGHBOR_REP_ID) {
			nbr_rep_ie = (dot11_neighbor_rep_ie_t *)tlvs;

			if (nbr_rep_ie->len < DOT11_NEIGHBOR_REP_IE_FIXED_LEN) {
				WL_ERROR(("wl%d: %s: malformed Neighbor Report element with"
					" length %d\n", wlc->pub->unit, __FUNCTION__,
					nbr_rep_ie->len));

				tlvs = bcm_next_tlv(tlvs, &tlv_len);
				continue;
			}

			nbr_rep = (rrm_nbr_rep_t *)MALLOCZ(wlc->osh, sizeof(rrm_nbr_rep_t));
			if (nbr_rep == NULL) {
				WL_ERROR(("wl%d: %s: out of memory for neighbor report,"
					" malloced %d bytes\n", wlc->pub->unit, __FUNCTION__,
					MALLOCED(wlc->osh)));
					/* not enough resources to add more neighbors */
					err = BCME_NORESOURCE;
					break;
			}

			memcpy(&nbr_rep->nbr_elt.bssid, &nbr_rep_ie->bssid, ETHER_ADDR_LEN);
			nbr_rep->nbr_elt.bssid_info = load32_ua(&nbr_rep_ie->bssid_info);

			nbr_rep->nbr_elt.reg = nbr_rep_ie->reg;
			nbr_rep->nbr_elt.channel = nbr_rep_ie->channel;
			nbr_rep->nbr_elt.phytype = nbr_rep_ie->phytype;
			nbr_rep->nbr_elt.len = nbr_rep_ie->len;
			frng_req->num_aps++;

			/* Add the new nbr_rep to the head of the list */
			WLC_RRM_ELEMENT_LIST_ADD(nbr_rep_head, nbr_rep);

			bcm_ether_ntoa(&nbr_rep_ie->bssid, eabuf);
			WL_TMP((" bssid %s, bssid_info: 0x%08x, channel %d, reg %d, phytype %d\n",
				eabuf, ntoh32_ua(&nbr_rep_ie->bssid_info), nbr_rep_ie->channel,
				nbr_rep_ie->reg, nbr_rep_ie->phytype));

			if (nbr_rep_ie->len > DOT11_NEIGHBOR_REP_IE_FIXED_LEN) {
				bcm_tlv_t *mr_tlvs = (bcm_tlv_t *) &nbr_rep_ie->data[0];
				int mr_tlv_len = nbr_rep_ie->len - DOT11_NEIGHBOR_REP_IE_FIXED_LEN;

				if (mr_tlv_len < TLV_HDR_LEN) {
					WL_ERROR(("wl%d: %s: malformed variable data with"
						" total length %d\n", wlc->pub->unit, __FUNCTION__,
						mr_tlv_len));
					tlvs = bcm_next_tlv(tlvs, &tlv_len);
					continue;
				}

				wlc_rrm_parse_mr_tlvs(wlc, mr_tlvs, &mr_tlv_len, nbr_rep, cfg);
			}

		} else if (tlvs->id == DOT11_FTM_RANGE_SUBELEM_ID) {
			frng_subel = (dot11_ftm_range_subel_t *) tlvs;

			if (frng_subel->len < DOT11_FTM_RANGE_SUBELEM_LEN) {
				WL_ERROR(("wl%d: %s: malformed FTM Range Subelement with length"
					" %d\n", wlc->pub->unit,
					__FUNCTION__, frng_subel->len));

				tlvs = bcm_next_tlv(tlvs, &tlv_len);
				continue;
			}

			frng_req->max_age = load16_ua(&frng_subel->max_age);
		} else {
			WL_ERROR(("wl%d: %s: Received Unknown Element %d, len %d\n",
				wlc->pub->unit, __FUNCTION__, tlvs->id, tlvs->len));
		}

		/*
		Although the spec specifies a neighbor report optional subelement type,
		it's use is not specified, so will be ignored for now.
		*/

		tlvs = bcm_next_tlv(tlvs, &tlv_len);
	}

	if (frng_req->min_ap_count > frng_req->num_aps) {
		WL_ERROR(("wl%d: %s: Min AP count (%u) is > Num APs (%u) in request;"
			" adjusting min AP count to %u\n", wlc->pub->unit, __FUNCTION__,
			frng_req->min_ap_count, frng_req->num_aps, frng_req->num_aps));
		frng_req->min_ap_count = frng_req->num_aps;
	}

	/*
	* Have all the neighbors; now need to send to host app to request
	* ranges from the ranging code
	*/
	req_len = sizeof(*frng_req) + ((frng_req->num_aps-1) * sizeof(frngreq_target_t));
	nbr_rep = nbr_rep_head;
	i = 0;

	while (nbr_rep) {
		nbr_rep_next = nbr_rep->next;

		memcpy(&frng_req->targets[i].bssid, &nbr_rep->nbr_elt.bssid, ETHER_ADDR_LEN);
		frng_req->targets[i].bssid_info = nbr_rep->nbr_elt.bssid_info;
		frng_req->targets[i].reg = nbr_rep->nbr_elt.reg;
		frng_req->targets[i].channel = nbr_rep->nbr_elt.channel;
		frng_req->targets[i].chanspec = nbr_rep->nbr_elt.chanspec;
		frng_req->targets[i].phytype = nbr_rep->nbr_elt.phytype;

		if (nbr_rep->nbr_elt.chanspec == 0) {
			uint8 extch = wlc_rclass_extch_get(wlc->cmi, nbr_rep->nbr_elt.reg);
			frng_req->targets[i].chanspec = wlc_ht_chanspec(wlc,
				nbr_rep->nbr_elt.channel, extch, rrm_info->cur_cfg);
		}

		if (!wf_chspec_valid(frng_req->targets[i].chanspec)) {
			WL_ERROR(("wl%d:%s:invalid chanspec defaulting to 20MHz\n",
				wlc->pub->unit, __FUNCTION__));
			frng_req->targets[i].chanspec = CH20MHZ_CHSPEC(nbr_rep->nbr_elt.channel);
		}

		if (nbr_rep->opt_lci != NULL) {
			dot11_meas_rep_t *lci_rep = (dot11_meas_rep_t *) nbr_rep->opt_lci;
			MFREE(rrm_info->wlc->osh, nbr_rep->opt_lci, lci_rep->len + TLV_HDR_LEN);
		}

		if (nbr_rep->opt_civic != NULL) {
			dot11_meas_rep_t *civic_rep = (dot11_meas_rep_t *) nbr_rep->opt_civic;
			MFREE(rrm_info->wlc->osh, nbr_rep->opt_civic, civic_rep->len + TLV_HDR_LEN);
		}

		MFREE(rrm_info->wlc->osh, nbr_rep, sizeof(rrm_nbr_rep_t));
		nbr_rep = nbr_rep_next;
		i++;
	}

	wlc_bss_mac_event(wlc, cfg, WLC_E_RRM, &rrm_info->da,
		0, 0, 0, (void *)frng_req, req_len);

	return err;
}

static int
wlc_rrm_begin_ftm(wlc_rrm_info_t *rrm_info)
{
	int err = BCME_OK;
	int i;
	wlc_ftm_ranging_ctx_t *rctx = NULL;
	frngreq_t *frng_req = NULL;
	wlc_info_t *wlc = rrm_info->wlc;
	wlc_rrm_req_state_t *rrm_state = rrm_info->rrm_state;
	wl_proxd_tlv_t *req_tlvs = NULL;
	wl_proxd_tlv_t *req_tlvs_start = NULL;
	uint16 req_len_total, req_len;
	wl_proxd_ranging_info_t info;
	memset(&info, 0, sizeof(info));

	frng_req = rrm_info->rrm_frng_req->frng_req;
	if (frng_req == NULL) {
		WL_ERROR(("wl%d: %s: frng_req is NULL\n",
				wlc->pub->unit, __FUNCTION__));
		goto done;
	}

	if (rrm_info->cur_cfg == NULL) {
		WL_ERROR(("wl%d: %s: rrm_info->cur_cfg is NULL\n",
				wlc->pub->unit, __FUNCTION__));
		goto done;
	}

	/* set up ranging */
	/* create a ranging context */
	err = wlc_ftm_ranging_create(wlc_ftm_get_handle(wlc), wlc_rrm_ftm_ranging_cb,
		(void *) rrm_info, &rctx);
	if (err != BCME_OK || rctx == NULL) {
		WL_ERROR(("wl%d: %s: status %d, wlc_ftm_ranging_create failed\n",
			wlc->pub->unit, __FUNCTION__, err));
		goto done;
	}

	rrm_info->rrm_frng_req->rctx = rctx;

	err = wlc_ftm_ranging_set_flags(rctx,	WL_PROXD_RANGING_FLAG_DEL_SESSIONS_ON_STOP,
		WL_PROXD_RANGING_FLAG_DEL_SESSIONS_ON_STOP);
	if (err != BCME_OK) {
		WL_ERROR(("wl%d: %s: status %d, wlc_ftm_ranging_set_flags failed\n",
			wlc->pub->unit, __FUNCTION__, err));
		goto done;
	}

	err = wlc_ftm_ranging_set_sid_range(rctx, WL_PROXD_SID_RRM_START, WL_PROXD_SID_RRM_END);
	if (err != BCME_OK) {
		WL_ERROR(("wl%d: %s: status %d, wlc_ftm_ranging_set_sid_range failed\n",
			wlc->pub->unit, __FUNCTION__, err));
		goto done;
	}

	req_len = BCM_TLV_MAX_DATA_SIZE; /* fix */
	req_len_total = req_len;

	req_tlvs_start = MALLOCZ(wlc->osh, req_len);
	if (req_tlvs_start == NULL) {
		WL_ERROR(("wl%d: %s: MALLOC failed of %d bytes\n",
				wlc->pub->unit, __FUNCTION__, req_len));
		goto done;
	}

	for (i = 0; i < frng_req->num_aps; i++)
	{
		uint32 flags = 0;
		uint32 flags_mask = 0;
		uint32 chanspec;
		uint16 num_burst = 0;

		wl_proxd_session_id_t sid = WL_PROXD_SESSION_ID_GLOBAL;

		if (!(frng_req->targets[i].bssid_info & DOT11_NGBR_BI_FTM)) {
			/* skip if FTM not advertised by target */
			WL_ERROR(("wl%d: %s: skipping BSSID with FTM not set in BSSID info\n",
				wlc->pub->unit, __FUNCTION__));
			continue;
		}
		/* real sid will be returned */
		err = wlc_ftm_ranging_add_sid(rctx, rrm_info->cur_cfg, &sid);
		if (err != BCME_OK) {
			WL_ERROR(("wl%d: %s: status %d, wlc_ftm_ranging_add_sid failed\n",
				wlc->pub->unit, __FUNCTION__, err));
			goto done;
		}

		frng_req->targets[i].sid = sid;

		req_tlvs = req_tlvs_start;

		err = bcm_pack_xtlv_entry((uint8 **)&req_tlvs, &req_len,
			WL_PROXD_TLV_ID_PEER_MAC, sizeof(struct ether_addr),
			(uint8 *)&frng_req->targets[i].bssid, BCM_XTLV_OPTION_ALIGN32);
		if (err != BCME_OK) {
			WL_ERROR(("wl%d: %s: status %d, bcm_pack_xtlv_entry failed\n",
				wlc->pub->unit, __FUNCTION__, err));
			goto done;
		}

		/* set flags */
		flags_mask = WL_PROXD_SESSION_FLAG_INITIATOR | WL_PROXD_SESSION_FLAG_TARGET;
		flags_mask = htol32(flags_mask);
		err = bcm_pack_xtlv_entry((uint8 **)&req_tlvs, &req_len,
			WL_PROXD_TLV_ID_SESSION_FLAGS_MASK,
			sizeof(uint32), (uint8 *)&flags_mask, BCM_XTLV_OPTION_ALIGN32);
		if (err != BCME_OK) {
			WL_ERROR(("wl%d: %s: status %d, bcm_pack_xtlv_entry failed\n",
				wlc->pub->unit, __FUNCTION__, req_len));
			goto done;
		}

		flags = WL_PROXD_SESSION_FLAG_INITIATOR;
		flags = htol32(flags);
		err = bcm_pack_xtlv_entry((uint8 **)&req_tlvs, &req_len,
			WL_PROXD_TLV_ID_SESSION_FLAGS,
			sizeof(uint32), (uint8 *)&flags, BCM_XTLV_OPTION_ALIGN32);
		if (err != BCME_OK) {
			WL_ERROR(("wl%d: %s: status %d, bcm_pack_xtlv_entry failed\n",
				wlc->pub->unit, __FUNCTION__, req_len));
			goto done;
		}

		num_burst = frng_req->reps + 1;
		num_burst = htol16(num_burst);
		err = bcm_pack_xtlv_entry((uint8 **)&req_tlvs, &req_len, WL_PROXD_TLV_ID_NUM_BURST,
			sizeof(uint16), (uint8 *)&num_burst, BCM_XTLV_OPTION_ALIGN32);
		if (err != BCME_OK) {
			WL_ERROR(("wl%d: %s: status %d, bcm_pack_xtlv_entry failed\n",
				wlc->pub->unit, __FUNCTION__, req_len));
			goto done;
		}

		chanspec = htol32(frng_req->targets[i].chanspec);
		err = bcm_pack_xtlv_entry((uint8 **)&req_tlvs, &req_len, WL_PROXD_TLV_ID_CHANSPEC,
			sizeof(uint32), (uint8 *)&chanspec, BCM_XTLV_OPTION_ALIGN32);
		if (err != BCME_OK) {
			WL_ERROR(("wl%d: %s: status %d, bcm_pack_xtlv_entry failed\n",
				wlc->pub->unit, __FUNCTION__, req_len));
			goto done;
		}

		err = wlc_ftm_set_iov(wlc_ftm_get_handle(wlc), rrm_info->cur_cfg,
			WL_PROXD_CMD_CONFIG, sid, req_tlvs_start, req_len_total-req_len);
		if (err != BCME_OK) {
			WL_ERROR(("wl%d: %s: status %d, wlc_ftm_set_iov failed\n",
				wlc->pub->unit, __FUNCTION__, err));
			goto done;
		}

		memset(req_tlvs_start, 0, req_len_total);
	}

	/* FTM may do a scan, plus we need to wait for FTM to do stuff anyway */
	rrm_state->scan_active = TRUE;
#ifdef WL_PROXDETECT
	if (PROXD_ENAB(wlc->pub)) {
		err = wlc_ftm_ranging_start(rctx);
		if (err != BCME_OK) {
			WL_ERROR(("wl%d: %s: status %d, wlc_ftm_ranging_start failed\n",
				wlc->pub->unit, __FUNCTION__, err));
		}
		err = wlc_ftm_ranging_get_info(rctx, &info);
		if (err != BCME_OK) {
			WL_ERROR(("wl%d: %s: status %d, wlc_ftm_ranging_get_info failed\n",
				wlc->pub->unit, __FUNCTION__, err));
		}
	}
#endif // endif

done:
	if (req_tlvs_start) {
		MFREE(wlc->osh, req_tlvs_start, req_len_total);
	}

	if (info.state < WL_PROXD_RANGING_STATE_INPROGRESS && rctx != NULL)	{
		/* ranging did not start so cancel */
		rrm_state->scan_active = FALSE;
		wlc_ftm_ranging_cancel(rctx);
	}

	return err;
}

static int
wlc_rrm_recv_frngrep(wlc_rrm_info_t *rrm_info, struct scb *scb,
	dot11_rmrep_ftm_range_t *rmrep_ftmrange, int len)
{
	wlc_info_t *wlc = rrm_info->wlc;
	int tlv_len;
	bcm_tlv_t *tlvs = NULL;
	uint8 *ptr;
	int i;
	dot11_ftm_range_entry_t *range_entry;
	dot11_ftm_range_error_entry_t *error_entry;
	frngrep_t frng_rep;
	char eabuf[ETHER_ADDR_STR_LEN];

	BCM_REFERENCE(wlc); /* in case BCMDBG off */

	if (rmrep_ftmrange->mode & (DOT11_RMREQ_MODE_PARALLEL | DOT11_RMREQ_MODE_ENABLE)) {
		WL_ERROR(("wl%d: %s: Unsupported FTM range report mode 0x%x\n",
		wlc->pub->unit, __FUNCTION__, rmrep_ftmrange->mode));
		return BCME_UNSUPPORTED;
	}

	frng_rep.event = WL_RRM_EVENT_FRNG_REP;
	frng_rep.range_entry_count = rmrep_ftmrange->entry_count;
	frng_rep.dialog_token = rmrep_ftmrange->token;

	if (frng_rep.range_entry_count > DOT11_FTM_RANGE_ENTRY_CNT_MAX) {
		WL_ERROR(("wl%d: %s: FTM Range Report entry_count %d is too high,"
			" malformed frame\n", wlc->pub->unit, __FUNCTION__,
			frng_rep.range_entry_count));
		return BCME_ERROR;
	}

	ptr = &rmrep_ftmrange->data[0];

	if (len < DOT11_FTM_RANGE_REP_MIN_LEN +
		(frng_rep.range_entry_count * sizeof(dot11_ftm_range_entry_t)) + 1) {
		WL_ERROR(("wl%d: %s: FTM Range Report len %d is too short, malformed frame\n",
			wlc->pub->unit, __FUNCTION__, len));
		return BCME_ERROR;
	}

	/*
	 * Put all the FTM range report elements in a list
	 */
	for (i = 0; i < frng_rep.range_entry_count; i++) {
		range_entry = (dot11_ftm_range_entry_t *) ptr;

		frng_rep.range_entries[i].start_tsf = load32_ua(&range_entry->start_tsf);
		memcpy(&frng_rep.range_entries[i].bssid, &range_entry->bssid, ETHER_ADDR_LEN);

		frng_rep.range_entries[i].range = (((range_entry->range[0] & 0xff) << 16) |
			((range_entry->range[1] & 0xff) << 8) | (range_entry->range[2] & 0xff));
		frng_rep.range_entries[i].max_err = range_entry->max_err[0];

		frng_rep.range_entries[i].rsvd = range_entry->rsvd;
		bcm_ether_ntoa(&range_entry->bssid, eabuf);
		ptr += sizeof(dot11_ftm_range_entry_t);
	}

	frng_rep.error_entry_count = *ptr++; /* error count just a single byte */

	if (frng_rep.error_entry_count > DOT11_FTM_RANGE_ERROR_CNT_MAX) {
		WL_ERROR(("wl%d: %s: FTM Range Report error_count %d is too high,"
			" malformed frame\n", wlc->pub->unit, __FUNCTION__,
			frng_rep.error_entry_count));
		return BCME_ERROR;
	}

	/*
	 * Put all the FTM range error report elements in a list
	 */
	for (i = 0; i < frng_rep.error_entry_count; i++) {
		error_entry = (dot11_ftm_range_error_entry_t *) ptr;

		frng_rep.error_entries[i].start_tsf = load32_ua(&error_entry->start_tsf);
		memcpy(&frng_rep.error_entries[i].bssid, &error_entry->bssid, ETHER_ADDR_LEN);
		frng_rep.error_entries[i].code = error_entry->code;

		ptr += sizeof(dot11_ftm_range_error_entry_t);
	}

	wlc_bss_mac_event(wlc, scb->bsscfg, WLC_E_RRM, &rrm_info->da,
		0, 0, 0, (void *)&frng_rep, sizeof(frng_rep));

	/*
	 * tlvs will be used for optional subelements, but no non-vendor-specific
	 * optional subelements are currently defined
	 */
	BCM_REFERENCE(tlvs);
	BCM_REFERENCE(tlv_len);

	return BCME_OK;
}

static int
wlc_rrm_create_frngrep_element(uint8 **ptr, rrm_bsscfg_cubby_t *rrm_cfg, frngrep_t *frngrep)
{
	dot11_ftm_range_entry_t *ftm_range;
	dot11_ftm_range_error_entry_t *ftm_error;
	int copy_len = 0;
	int i = 0;
	dot11_meas_rep_t *frng_rep = (dot11_meas_rep_t *) *ptr;

	if (frngrep == NULL ||
		(!frngrep->range_entry_count && !frngrep->error_entry_count)) {
		WL_ERROR(("%s: no range entry data\n",
				__FUNCTION__));
		return 0;
	}

	frng_rep->id = DOT11_MNG_MEASURE_REPORT_ID;
	frng_rep->type = DOT11_MEASURE_TYPE_FTMRANGE;
	frng_rep->mode = 0;
	frng_rep->token = (uint8)frngrep->dialog_token;
	frng_rep->len = DOT11_MNG_IE_MREP_FRNG_FIXED_LEN;

	frng_rep->rep.ftm_range.entry_count = frngrep->range_entry_count;

	*ptr += TLV_HDR_LEN + DOT11_MNG_IE_MREP_FRNG_FIXED_LEN;

	/* Range Entries */
	for (i = 0; i < frngrep->range_entry_count; i++) {
		ftm_range = (dot11_ftm_range_entry_t *) *ptr;

		store32_ua((uint8 *)&ftm_range->start_tsf, frngrep->range_entries[i].start_tsf);
		memcpy(&ftm_range->bssid, &frngrep->range_entries[i].bssid, ETHER_ADDR_LEN);
		ftm_range->range[0] = (frngrep->range_entries[i].range >> 16) & 0xFF;
		ftm_range->range[1] = (frngrep->range_entries[i].range >> 8) & 0xFF;
		ftm_range->range[2] = frngrep->range_entries[i].range & 0xFF;

		ftm_range->max_err[0] = frngrep->range_entries[i].max_err;
		ftm_range->rsvd = 0;
		*ptr += sizeof(dot11_ftm_range_entry_t);
	}

	copy_len += (sizeof(*ftm_range) * frngrep->range_entry_count);
	**ptr = frngrep->error_entry_count;
	*ptr += sizeof(frngrep->error_entry_count);
	copy_len++;

	/* Error entries */
	for (i = 0; i < frngrep->error_entry_count; i++) {
		ftm_error = (dot11_ftm_range_error_entry_t *) *ptr;
		store32_ua((uint8 *)&ftm_error->start_tsf, frngrep->error_entries[i].start_tsf);
		memcpy(&ftm_error->bssid, &frngrep->error_entries[i].bssid, ETHER_ADDR_LEN);
		ftm_error->code = frngrep->error_entries[i].code;
		*ptr += sizeof(dot11_ftm_range_error_entry_t);
	}

	copy_len += (sizeof(*ftm_error) * frngrep->error_entry_count);

	/* Currently no optional subelements for FTM Range report */
	frng_rep->len += copy_len;
	return copy_len;
}

static int
wlc_rrm_send_frngrep(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, uint8 *data, int len)
{
	wlc_info_t *wlc = rrm_info->wlc;
	dot11_rm_action_t *rm_rep;
	void *p = NULL;
	uint8 *pbody;
	uint buflen;
	rrm_bsscfg_cubby_t *rrm_cfg = NULL;
	struct ether_addr *da;
	int err = BCME_OK;
	frngrep_t *frngrep = (frngrep_t *)data;
	struct scb *scb;

	if (len > sizeof(frngrep_t)) {
		return BCME_BUFTOOLONG;
	}
	if (len < (OFFSETOF(frngrep_t, range_entries) + sizeof(frngrep_error_t))) {
		return BCME_BUFTOOSHORT;
	}

	if (frngrep->range_entry_count > DOT11_FTM_RANGE_ENTRY_CNT_MAX) {
		WL_ERROR(("wl%d: %s IOV_RRM_FRNG: REP: range_entry_count %d"
			" invalid\n", rrm_info->wlc->pub->unit, __FUNCTION__,
			frngrep->range_entry_count));
		return BCME_BADARG;
	}

	if (frngrep->error_entry_count > DOT11_FTM_RANGE_ERROR_CNT_MAX) {
		WL_ERROR(("wl%d: %s IOV_RRM_FRNG_REP: error_entry_count %d"
			" invalid\n", rrm_info->wlc->pub->unit, __FUNCTION__,
			frngrep->error_entry_count));
		return BCME_BADARG;
	}

	if (!frngrep->range_entry_count && !frngrep->error_entry_count) {
		WL_ERROR(("wl%d: %s IOV_RRM_FRNG_REP: missing entries, range %d, error %d",
			rrm_info->wlc->pub->unit, __FUNCTION__,
			frngrep->range_entry_count,
			frngrep->error_entry_count));
		return BCME_BADARG;
	}

	rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, cfg);
	ASSERT(rrm_cfg != NULL);

	buflen = DOT11_RM_ACTION_LEN + DOT11_RM_IE_LEN + DOT11_FTM_RANGE_REP_FIXED_LEN;
	buflen += frngrep->range_entry_count * sizeof(dot11_ftm_range_entry_t);
	buflen += sizeof(frngrep->error_entry_count);
	buflen += frngrep->error_entry_count * sizeof(dot11_ftm_range_error_entry_t);

	da = &frngrep->da;
	p = wlc_frame_get_action(wlc, da, &cfg->cur_etheraddr, &cfg->BSSID,
		buflen, &pbody, DOT11_ACTION_CAT_RRM);

	if (p == NULL) {
		WL_ERROR(("%s: failed to get mgmt frame\n", __FUNCTION__));
		return BCME_NOMEM;
	}

	rm_rep = (dot11_rm_action_t *)pbody;
	rm_rep->category = DOT11_ACTION_CAT_RRM;
	rm_rep->action = DOT11_RM_ACTION_RM_REP;
	rm_rep->token = rrm_info->dialog_token;

	pbody = rm_rep->data;
	wlc_rrm_create_frngrep_element(&pbody, rrm_cfg, frngrep);

	scb = wlc_scbfind(wlc, cfg, da);
	wlc_sendmgmt(wlc, p, cfg->wlcif->qi, scb);

	return err;
}

static int
wlc_rrm_scbflag_check(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, struct ether_addr *da, int flag)
{
	wlc_info_t *wlc = rrm_info->wlc;
	struct scb *scb;

	if ((scb = wlc_scbfind(wlc, cfg, da)) == NULL) {
		return BCME_NOTASSOCIATED;
	}

	if (!(scb->flags3 & SCB3_RRM)) {
		return BCME_UNSUPPORTED;
	}
	else {
		rrm_scb_info_t *scb_info = RRM_SCB_INFO(rrm_info, scb);
		if (!isset(scb_info->rrm_capabilities, flag)) {
			return BCME_UNSUPPORTED;
		}
	}
	return BCME_OK;
}

static int
wlc_rrm_set_frngdir(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, uint8 *data, int len)
{
	int err = BCME_OK;
	uint8 mode = 0;

	if (len != sizeof(mode)) {
		return BCME_BADARG;
	}

	mode = data[0];
	rrm_info->direct_ranging_mode = (bool)mode;

	return err;
}

static int
wlc_rrm_send_frngreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, uint8 *data, int len)
{
	void *p;
	uint8 *pbody, *ptr;
	uint8 *bufend;
	int buflen;
	dot11_rmreq_t *rmreq;
	dot11_rmreq_ftm_range_t *freq;
	struct ether_addr *da;
	frngreq_target_t *targets;
	int i;
	uint16 nbr_rep_cnt;
	rrm_bsscfg_cubby_t *rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, cfg);
	int err = BCME_OK;
	frngreq_t *frngreq = (frngreq_t *)data;
	wlc_info_t *wlc = rrm_info->wlc;
	struct scb *scb;
	int scb_sec_len = 0;

	if (len > ((sizeof(frngreq_t) +
		((DOT11_FTM_RANGE_ENTRY_CNT_MAX-1) * sizeof(frngreq_target_t))))) {
		return BCME_BUFTOOLONG;
	}
	if (len < sizeof(frngreq_t)) {
		return BCME_BUFTOOSHORT;
	}

	if (frngreq->num_aps < frngreq->min_ap_count) {
		WL_ERROR(("wl%d: %s IOV_RRM_FRNG: REQ: num_aps %d <"
				" min_ap_count %d\n", rrm_info->wlc->pub->unit,
				__FUNCTION__, frngreq->num_aps, frngreq->min_ap_count));
		return BCME_BADARG;
	}

	da = &frngreq->da;

	/* rm frame action header + ftm range request header */
	buflen = DOT11_RMREQ_LEN + DOT11_RMREQ_FTM_RANGE_LEN;
	targets = (frngreq_target_t *) frngreq->targets;

	nbr_rep_cnt = wlc_rrm_get_neighbor_count(wlc, cfg, NBR_ADD_STATIC);
	if (ETHER_ISNULLADDR(&targets[0].bssid) &&
		(nbr_rep_cnt < frngreq->num_aps ||
		nbr_rep_cnt < frngreq->min_ap_count)) {
		WL_ERROR(("wl%d: %s IOV_RRM_FRNG: REQ: only %d APs in nbr list\n",
				rrm_info->wlc->pub->unit,
				__FUNCTION__, nbr_rep_cnt));
		return BCME_BADARG;
	}

	err = wlc_rrm_scbflag_check(rrm_info, cfg, da, DOT11_RRM_CAP_FTM_RANGE);
	if (err) {
		return err;
	}

	/* too many optional elements to count up, fixup the length at the end */
	buflen = ETHER_MAX_DATA;
	if ((p = wlc_frame_get_action(wlc, da,
		&cfg->cur_etheraddr, &cfg->BSSID, buflen, &pbody, DOT11_ACTION_CAT_RRM)) == NULL) {
		return BCME_NOMEM;
	}

	bufend = pbody + buflen;

	rmreq = (dot11_rmreq_t *)pbody;
	rmreq->category = DOT11_ACTION_CAT_RRM;
	rmreq->action = DOT11_RM_ACTION_RM_REQ;
	WLC_RRM_UPDATE_TOKEN(rrm_info->req_token);
	rmreq->token = rrm_info->req_token;
	rmreq->reps = frngreq->reps;

	freq = (dot11_rmreq_ftm_range_t *)&rmreq->data[0];
	freq->id = DOT11_MNG_MEASURE_REQUEST_ID;
	freq->len = buflen - DOT11_RMREQ_LEN - TLV_HDR_LEN;
	WLC_RRM_UPDATE_TOKEN(rrm_info->req_elt_token);
	freq->token = rrm_info->req_elt_token;
	freq->mode = 0;
	freq->type = DOT11_MEASURE_TYPE_FTMRANGE;

	store16_ua((uint8 *)&freq->max_init_delay, frngreq->max_init_delay);
	freq->min_ap_count = frngreq->min_ap_count;

	ptr = (uint8 *) freq->data;

	/* add neighbor reports -  at least 1 is required */
	if (ETHER_ISNULLADDR(&targets[0].bssid)) {
		/* add APs from current neighbor list if none given in IOVAR */
		rrm_nbr_rep_t *nbr_rep = rrm_cfg->nbr_rep_head;

		for (i = 0; i < frngreq->num_aps; i++) {
			wlc_rrm_create_nbrrep_element(rrm_cfg, &ptr, nbr_rep, 0, bufend);

			/* Move to the next one */
			nbr_rep = nbr_rep->next;
		}
	}
	else {
		/* add APs from list given in IOVAR */
		for (i = 0; i < frngreq->num_aps; i++) {
			rrm_nbr_rep_t nbr_rep;

			memset(&nbr_rep, 0, sizeof(nbr_rep));
			memcpy(&nbr_rep.nbr_elt.bssid, &targets[i].bssid,
			       sizeof(nbr_rep.nbr_elt.bssid));
			nbr_rep.nbr_elt.bssid_info = targets[i].bssid_info;
			nbr_rep.nbr_elt.channel = targets[i].channel;
			nbr_rep.nbr_elt.phytype = targets[i].phytype;
			nbr_rep.nbr_elt.reg = targets[i].reg;
			nbr_rep.nbr_elt.chanspec = targets[i].chanspec;
			nbr_rep.nbr_elt.id = DOT11_MNG_NEIGHBOR_REP_ID;

			wlc_rrm_create_nbrrep_element(rrm_cfg, &ptr, &nbr_rep, 0, bufend);
		}
	}

	/* add optional subelements in non-decreasing order */
	if (frngreq->max_age != 0) {
		dot11_ftm_range_subel_t *age_range_subel = (dot11_ftm_range_subel_t *) ptr;

		age_range_subel->id = DOT11_FTM_RANGE_SUBELEM_ID;
		age_range_subel->len = DOT11_FTM_RANGE_SUBELEM_LEN;
		store16_ua((uint8 *)&age_range_subel->max_age, frngreq->max_age);
		ptr += sizeof(*age_range_subel);
	}

	scb = wlc_scbfind(wlc, cfg, da);
	if ((bufend - ptr) > 0 && scb != NULL) {
#ifdef MFP
		scb_sec_len = wlc_mfp_frame_get_sec_len(wlc->mfp, FC_ACTION,
			DOT11_ACTION_CAT_RRM, &scb->ea, cfg);
#endif // endif
		buflen += DOT11_MGMT_HDR_LEN;
		freq->len -= (bufend - ptr);
		PKTSETLEN(wlc->osh, p, (buflen - (bufend - ptr)) + scb_sec_len);
	}

	wlc_sendmgmt(wlc, p, cfg->wlcif->qi, scb);

	return err;
}

static void wlc_rrm_frngreq_free(wlc_rrm_info_t *rrm_info)
{
	wlc_info_t *wlc = rrm_info->wlc;
	rrm_ftmreq_t *rrm_frng_req;

	rrm_frng_req = rrm_info->rrm_frng_req;
	if (rrm_frng_req) {
		if (rrm_frng_req->frng_rep) {
			MFREE(wlc->osh, rrm_frng_req->frng_rep, sizeof(frngrep_t));
			rrm_frng_req->frng_rep = NULL;
		}
		if (rrm_frng_req->frng_req) {
			MFREE(wlc->osh, rrm_frng_req->frng_req, sizeof(frngreq_t));
			rrm_frng_req->frng_req = NULL;
		}

		if (rrm_frng_req->rctx) {
			WL_TMP(("%s: Destroying range context\n", __FUNCTION__));
			wlc_ftm_ranging_cancel(rrm_frng_req->rctx);
			wlc_ftm_ranging_destroy(&rrm_frng_req->rctx);
		}

		MFREE(wlc->osh, rrm_frng_req, sizeof(rrm_ftmreq_t));
	}
	rrm_info->rrm_frng_req = NULL;
}
#endif /* WL_PROXDETECT && WL_FTM */
#endif /* WL_FTM_11K */

#ifdef WL11K_NBR_MEAS
static void
wlc_rrm_flush_neighbors(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *bsscfg, int addtype)
{
	rrm_nbr_rep_t *nbr_rep, *nbr_rep_next;
	reg_nbr_count_t *nbr_cnt, *nbr_cnt_next;
	rrm_bsscfg_cubby_t *rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, bsscfg);
	rrm_nbr_rep_t **rrm_nbr_rep_headp;

	if (rrm_cfg == NULL)
		return;

	rrm_nbr_rep_headp = (RRM_NBR_REP_HEADPTR(rrm_cfg, addtype));
	nbr_rep = RRM_NBR_REP_HEAD(rrm_cfg, addtype);
	while (nbr_rep) {
		nbr_rep_next = nbr_rep->next;

#ifdef WL_FTM_11K
		if (nbr_rep->opt_lci != NULL) {
			dot11_meas_rep_t *lci_rep = (dot11_meas_rep_t *) nbr_rep->opt_lci;
			MFREE(rrm_info->wlc->osh, nbr_rep->opt_lci, lci_rep->len + TLV_HDR_LEN);
		}
		if (nbr_rep->opt_civic != NULL) {
			dot11_meas_rep_t *civic_rep = (dot11_meas_rep_t *) nbr_rep->opt_civic;
			MFREE(rrm_info->wlc->osh, nbr_rep->opt_civic, civic_rep->len + TLV_HDR_LEN);
		}
#endif /* WL_FTM_11K */

		MFREE(rrm_info->wlc->osh, nbr_rep, sizeof(rrm_nbr_rep_t));
		nbr_rep = nbr_rep_next;
	}
	*rrm_nbr_rep_headp = NULL;

	if (addtype == NBR_ADD_STATIC) {
	  nbr_cnt = rrm_cfg->nbr_cnt_head;
	  while (nbr_cnt) {
		nbr_cnt_next = nbr_cnt->next;
		MFREE(rrm_info->wlc->osh, nbr_cnt, sizeof(reg_nbr_count_t));
		nbr_cnt = nbr_cnt_next;
	  }
	  rrm_cfg->nbr_cnt_head = NULL;
	}
}

#ifdef BCMDBG
static void
wlc_rrm_dump_neighbors(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *bsscfg, int addtype)
{
	rrm_nbr_rep_t *nbr_rep;
	reg_nbr_count_t *nbr_cnt;
	int count = 0;
	char eabuf[ETHER_ADDR_STR_LEN];
	int i;
	rrm_bsscfg_cubby_t *rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, bsscfg);

	ASSERT(rrm_cfg != NULL);

	WL_ERROR(("\nRRM Neighbor Report:\n"));
	nbr_rep = RRM_NBR_REP_HEAD(rrm_cfg, addtype);
	while (nbr_rep) {
		count++;

		WL_ERROR(("AP %2d: ", count));
		WL_ERROR(("bssid %s ", bcm_ether_ntoa(&nbr_rep->nbr_elt.bssid, eabuf)));
		WL_ERROR(("bssid_info %08x ", load32_ua(&nbr_rep->nbr_elt.bssid_info)));
		WL_ERROR(("reg %2d channel %3d phytype %d pref %3d\n", nbr_rep->nbr_elt.reg,
			nbr_rep->nbr_elt.channel, nbr_rep->nbr_elt.phytype,
			nbr_rep->nbr_elt.bss_trans_preference));
#ifdef WL_FTM_11K
		if (nbr_rep->opt_lci != NULL) {
			dot11_meas_rep_t *lci_rep = (dot11_meas_rep_t *) nbr_rep->opt_lci;
			WL_ERROR(("LCI meas report present, len %d, LCI subelement len %d\n",
				lci_rep->len, lci_rep->rep.lci.length));
		}
		if (nbr_rep->opt_civic != NULL) {
			dot11_meas_rep_t *civic_rep = (dot11_meas_rep_t *) nbr_rep->opt_civic;
			WL_ERROR(("Civic meas report present, len %d,"
				" Civic subelement type %d, len %d\n", civic_rep->len,
				civic_rep->rep.civic.type, civic_rep->rep.civic.length));
		}
#endif /* WL_FTM_11K */

		nbr_rep = nbr_rep->next;
	}

	if (addtype == NBR_ADD_STATIC) {
		WL_ERROR(("\nRRM AP Channel Report:\n"));
		nbr_cnt = rrm_cfg->nbr_cnt_head;
		while (nbr_cnt) {
			WL_ERROR(("regulatory %d channel ", nbr_cnt->reg));

			for (i = 0; i < MAXCHANNEL; i++) {
				if (nbr_cnt->nbr_count_by_channel[i] > 0)
					WL_ERROR(("%d ", i));
			}
			WL_ERROR(("\n"));

			nbr_cnt = nbr_cnt->next;
		}
	}
}
#endif /* BCMDBG */

#ifdef STA
static void
wlc_rrm_regclass_neighbor_count(wlc_rrm_info_t *rrm_info, rrm_nbr_rep_t *nbr_rep,
	wlc_bsscfg_t *cfg)
{
	bool found;
	wlc_info_t *wlc;
	reg_nbr_count_t *nbr_cnt;
	rrm_bsscfg_cubby_t *rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, cfg);

	ASSERT(rrm_cfg != NULL);

	if (!VALID_NBR_REP_CHAN(nbr_rep)) {
		return;
	}

	if ((found = wlc_rrm_regclass_match(rrm_info, nbr_rep, cfg)))
		return;

	wlc = rrm_info->wlc;

	/* There is no corresponding reg_nbr_count_t for this nbr_rep; create a new one */
	nbr_cnt = (reg_nbr_count_t *)MALLOCZ(wlc->osh, sizeof(reg_nbr_count_t));
	if (nbr_cnt == NULL) {
		WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__,
			(int)sizeof(reg_nbr_count_t), MALLOCED(wlc->osh)));
		return;
	}

	nbr_cnt->reg = nbr_rep->nbr_elt.reg;
	nbr_cnt->nbr_count_by_channel[nbr_rep->nbr_elt.channel] += 1;

	/* Add this reg_nbr_count_t to the list */
	WLC_RRM_ELEMENT_LIST_ADD(rrm_cfg->nbr_cnt_head, nbr_cnt);
}

static bool
wlc_rrm_regclass_match(wlc_rrm_info_t *rrm_info, rrm_nbr_rep_t *nbr_rep, wlc_bsscfg_t *cfg)
{
	reg_nbr_count_t *nbr_cnt;
	rrm_bsscfg_cubby_t *rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, cfg);

	ASSERT(rrm_cfg != NULL);

	if (!VALID_NBR_REP_CHAN(nbr_rep)) {
		return FALSE;
	}

	nbr_cnt = rrm_cfg->nbr_cnt_head;

	while (nbr_cnt) {
		if (nbr_cnt->reg == nbr_rep->nbr_elt.reg) {
			nbr_cnt->nbr_count_by_channel[nbr_rep->nbr_elt.channel] += 1;
			return TRUE;
		}
		nbr_cnt = nbr_cnt->next;
	}

	return FALSE;
}

/* Configure the roam channel cache manually, for instance if you have
 * .11k neighbor information.
 */
static int
wlc_set_roam_channel_cache(wlc_bsscfg_t *cfg, const chanspec_t *chanspecs, int num_chanspecs)
{
	int i, num_channel = 0;
	uint8 channel;
	wlc_roam_t *roam = cfg->roam;

	if (!roam) {
		return BCME_UNSUPPORTED;
	}
	/* The new information replaces previous contents. */
	memset(roam->roam_chn_cache.vec, 0, sizeof(chanvec_t));

	WL_SRSCAN(("wl%d: ROAM: Adding channels to cache:", WLCWLUNIT(cfg->wlc)));
	WL_ASSOC(("wl%d: ROAM: Adding channels to cache:\n", WLCWLUNIT(cfg->wlc)));

	/* Neighbor list from AP may contains more channels than necessay:
	 * Take the first wlc->roam_chn_cache_limit distinctive channels.
	 * (prioritize 5G over 2G channels)
	 */
	for (i = 0; (i < num_chanspecs) && (num_channel < roam->roam_chn_cache_limit); i++) {
		channel = wf_chspec_ctlchan(chanspecs[i]);

		if (CHSPEC_IS5G(chanspecs[i]) && isclr(roam->roam_chn_cache.vec, channel)) {
			WL_SRSCAN(("%u", channel));
			WL_ASSOC(("%u\n", channel));
			setbit(roam->roam_chn_cache.vec, channel);
			num_channel++;
		}
	}
	for (i = 0; (i < num_chanspecs) && (num_channel < roam->roam_chn_cache_limit); i++) {
		channel = wf_chspec_ctlchan(chanspecs[i]);

		if (CHSPEC_IS2G(chanspecs[i]) && isclr(roam->roam_chn_cache.vec, channel)) {
			WL_SRSCAN(("%u", channel));
			WL_ASSOC(("%u\n", channel));
			setbit(roam->roam_chn_cache.vec, channel);
			num_channel++;
		}
	}
	WL_ASSOC(("\n"));

	/* Don't let subsequent roam scans modify the cache. */
	roam->roam_chn_cache_locked = TRUE;

	return BCME_OK;
}

static void
wlc_rrm_recv_nrrep(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, struct scb *scb,
	uint8 *body, int body_len)
{
	wlc_info_t *wlc = rrm_info->wlc;
	dot11_rm_action_t *rm_rep;
	bcm_tlv_t *tlvs;
	int tlv_len;
	dot11_neighbor_rep_ie_t *nbr_rep_ie;
	rrm_nbr_rep_t *nbr_rep;
	dot11_ngbr_bsstrans_pref_se_t *pref;
	rrm_bsscfg_cubby_t *rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, cfg);
	chanspec_t ch;
	chanspec_t chanspec_list[MAXCHANNEL];
	uint channel_num = 0;

	ASSERT(rrm_cfg != NULL);
	if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_NEIGHBOR_REPORT)) {
		WL_ERROR(("%s: Unsupported\n", __FUNCTION__));
		return;
	}

	if (body_len < DOT11_RM_ACTION_LEN) {
		WL_ERROR(("wl%d: %s: Received Neighbor Report frame with incorrect length %d\n",
			wlc->pub->unit, __FUNCTION__, body_len));
		return;
	}

	rm_rep = (dot11_rm_action_t *)body;
	WL_SRSCAN(("received neighbor report (token = %d)",
		rm_rep->token));
	WL_ASSOC(("received neighbor report (token = %d)\n",
		rm_rep->token));

	wlc_rrm_flush_neighbors(rrm_info, cfg, NBR_ADD_STATIC);

	tlvs = (bcm_tlv_t *)&rm_rep->data[0];

	tlv_len = body_len - DOT11_RM_ACTION_LEN;

	while (tlvs && bcm_valid_tlv(tlvs, tlv_len) && tlvs->id == DOT11_MNG_NEIGHBOR_REP_ID) {
		nbr_rep_ie = (dot11_neighbor_rep_ie_t *)tlvs;

		if (nbr_rep_ie->len < DOT11_NEIGHBOR_REP_IE_FIXED_LEN) {
			WL_ERROR(("wl%d: %s: malformed Neighbor Report element with length %d\n",
				wlc->pub->unit, __FUNCTION__, nbr_rep_ie->len));

			tlvs = bcm_next_tlv(tlvs, &tlv_len);

			continue;
		}

		if (!VALID_NBR_CHAN(nbr_rep_ie->channel)) {
			WL_ERROR(("wl%d: Bad channel %d in Neighbor Report\n",
				wlc->pub->unit, nbr_rep_ie->channel));
			tlvs = bcm_next_tlv(tlvs, &tlv_len);
			continue;
		}

		nbr_rep = (rrm_nbr_rep_t *)MALLOCZ(wlc->osh, sizeof(rrm_nbr_rep_t));
		if (nbr_rep == NULL) {
			WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__,
				(int)sizeof(rrm_nbr_rep_t), MALLOCED(wlc->osh)));
			return;
		}

		memcpy(&nbr_rep->nbr_elt.bssid, &nbr_rep_ie->bssid, ETHER_ADDR_LEN);
		nbr_rep->nbr_elt.bssid_info = load32_ua(&nbr_rep_ie->bssid_info);
		nbr_rep->nbr_elt.reg = nbr_rep_ie->reg;
		nbr_rep->nbr_elt.channel = nbr_rep_ie->channel;
		nbr_rep->nbr_elt.phytype = nbr_rep_ie->phytype;

		pref = (dot11_ngbr_bsstrans_pref_se_t*) bcm_parse_tlvs(nbr_rep_ie->data,
			nbr_rep_ie->len - DOT11_NEIGHBOR_REP_IE_FIXED_LEN,
			DOT11_NGBR_BSSTRANS_PREF_SE_ID);
		if (pref) {
			nbr_rep->nbr_elt.bss_trans_preference = pref->preference;
		}
		/* Add the new nbr_rep to the head of the list
		 * - donot distinguish static or dynamic here
		 */
		WLC_RRM_ELEMENT_LIST_ADD(rrm_cfg->nbr_rep_head, nbr_rep);

		/* AP Channel Report */
		wlc_rrm_regclass_neighbor_count(rrm_info, nbr_rep, cfg);

		ch = CH20MHZ_CHSPEC(nbr_rep_ie->channel);
		WL_SRSCAN(("  bssid %02x:%02x:%02x",
			nbr_rep_ie->bssid.octet[0], nbr_rep_ie->bssid.octet[1],
			nbr_rep_ie->bssid.octet[2]));
		WL_SRSCAN(("        %02x:%02x:%02x",
			nbr_rep_ie->bssid.octet[3], nbr_rep_ie->bssid.octet[4],
			nbr_rep_ie->bssid.octet[5]));
		WL_SRSCAN((
			"    bssinfo: 0x%x: reg %d: channel %d: phytype %d",
			ntoh32_ua(&nbr_rep_ie->bssid_info), nbr_rep_ie->reg,
			nbr_rep_ie->channel, nbr_rep_ie->phytype));
		WL_ASSOC(("  bssid %02x:%02x:%02x\n",
			nbr_rep_ie->bssid.octet[0], nbr_rep_ie->bssid.octet[1],
			nbr_rep_ie->bssid.octet[2]));
		WL_ASSOC(("        %02x:%02x:%02x\n",
			nbr_rep_ie->bssid.octet[3], nbr_rep_ie->bssid.octet[4],
			nbr_rep_ie->bssid.octet[5]));
		WL_ASSOC((
			"    bssinfo: 0x%x: reg %d: channel %d: phytype %d\n",
			ntoh32_ua(&nbr_rep_ie->bssid_info), nbr_rep_ie->reg,
			nbr_rep_ie->channel, nbr_rep_ie->phytype));

		/* Prepare the hot channel list with 11k information */
		if ((channel_num < MAXCHANNEL) && wlc_valid_chanspec_db(wlc->cmi, ch))
			chanspec_list[channel_num++] = ch;

		tlvs = bcm_next_tlv(tlvs, &tlv_len);
	}

	/* Push the list for consumption */
	(void) wlc_set_roam_channel_cache(cfg, chanspec_list, channel_num);

#ifdef BCMDBG
	wlc_rrm_dump_neighbors(rrm_info, cfg, NBR_ADD_STATIC);
#endif /* BCMDBG */

	wlc_rrm_send_event(rrm_info, scb, (char *)body, body_len, DOT11_RM_ACTION_NR_REP, 0);
}
#endif /* STA */
#endif /* WL11K_NBR_MEAS */

static int
wlc_rrm_doiovar(void *hdl, uint32 actionid,
        void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif)
{
	wlc_rrm_info_t *rrm_info = hdl;
	wlc_info_t * wlc = rrm_info->wlc;
	rrm_bsscfg_cubby_t *rrm_cfg;
	wlc_bsscfg_t *cfg;
	int err = BCME_OK;

	cfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(cfg != NULL);

	rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, cfg);

	switch (actionid) {
	case IOV_SVAL(IOV_RRM):
	{
		wlc_bss_info_t *current_bss = cfg->current_bss;
		dot11_rrm_cap_ie_t *rrm_cap = (dot11_rrm_cap_ie_t *)a;
		dot11_rrm_cap_ie_t cap_avail;
		int i;
#ifdef WL11K_ALL_MEAS
		bool enabled;
#endif // endif
		cap_avail.cap[0] = DOT11_RRM_CAP_LINK_ENAB |
			DOT11_RRM_CAP_NEIGHBOR_REPORT_ENAB |
			DOT11_RRM_CAP_BCN_PASSIVE_ENAB |
			DOT11_RRM_CAP_BCN_ACTIVE_ENAB |
			0;
		cap_avail.cap[1] = DOT11_RRM_CAP_FM_ENAB |
			DOT11_RRM_CAP_CLM_ENAB |
			DOT11_RRM_CAP_NHM_ENAB |
			DOT11_RRM_CAP_SM_ENAB |
			DOT11_RRM_CAP_LCIM_ENAB |
			DOT11_RRM_CAP_TSCM_ENAB |
			0;
		cap_avail.cap[2] = 0;
		cap_avail.cap[3] = DOT11_RRM_CAP_MPC0_ENAB |
			DOT11_RRM_CAP_MPC1_ENAB |
			DOT11_RRM_CAP_MPC2_ENAB |
			DOT11_RRM_CAP_MPTI_ENAB |
			0;
		cap_avail.cap[4] = DOT11_RRM_CAP_CIVIC_LOC_ENAB |
			DOT11_RRM_CAP_IDENT_LOC_ENAB |
			DOT11_RRM_CAP_FTM_RANGE_ENAB |
			0;
		for (i = 0; i < DOT11_RRM_CAP_LEN; i++)
			rrm_cap->cap[i] &= cap_avail.cap[i];
#ifdef WL11K_ALL_MEAS
		/* Set/clear extended capabilities bits for LCI/Civic/Identifier
		 * location based on RRM capabilities
		 */
		enabled = isset(rrm_cap->cap, DOT11_RRM_CAP_LCIM);
		wlc_bsscfg_set_ext_cap(cfg, DOT11_EXT_CAP_LCI, enabled);

		enabled = isset(rrm_cap->cap, DOT11_RRM_CAP_CIVIC_LOC);
		wlc_bsscfg_set_ext_cap(cfg, DOT11_EXT_CAP_CIVIC_LOC, enabled);

		enabled = isset(rrm_cap->cap, DOT11_RRM_CAP_IDENT_LOC);
		wlc_bsscfg_set_ext_cap(cfg, DOT11_EXT_CAP_IDENT_LOC, enabled);
		clrbit(rrm_cap->cap, DOT11_RRM_CAP_IDENT_LOC);
#endif // endif

		bcopy(rrm_cap->cap, rrm_cfg->rrm_cap, DOT11_RRM_CAP_LEN);

		if (!wlc_rrm_enabled(rrm_info, cfg)) {
			wlc_bsscfg_t *bsscfg;
			int idx;
			bool _rrm = FALSE;

			current_bss->capability &= ~DOT11_CAP_RRM;

			FOREACH_BSS(wlc, idx, bsscfg) {
				if (bsscfg->current_bss->capability & DOT11_CAP_RRM) {
					_rrm = TRUE;
					break;
				}
			}
			wlc->pub->_rrm = _rrm;

#ifdef WL11K_NBR_MEAS
			wlc_rrm_flush_neighbors(rrm_info, cfg, NBR_ADD_STATIC);
#endif // endif
		} else {
			current_bss->capability |= DOT11_CAP_RRM;
			wlc->pub->_rrm = TRUE;
		}

		wlc_rrm_update_beacon(wlc, cfg);
		break;
	}
	case IOV_GVAL(IOV_RRM):
	{
		dot11_rrm_cap_ie_t *rrm_cap = (dot11_rrm_cap_ie_t *)a;

		bcopy(rrm_cfg->rrm_cap, rrm_cap->cap, DOT11_RRM_CAP_LEN);
#ifdef WL11K_ALL_MEAS
		if (isset(cfg->ext_cap, DOT11_EXT_CAP_CIVIC_LOC))
			setbit(rrm_cap->cap, DOT11_RRM_CAP_CIVIC_LOC);
		if (isset(cfg->ext_cap, DOT11_EXT_CAP_IDENT_LOC))
			setbit(rrm_cap->cap, DOT11_RRM_CAP_IDENT_LOC);
#endif /* WL11K_ALL_MEAS */

		break;
	}
#ifdef WL11K_NBR_MEAS
	/* IOVAR SET neighbor request */
	case IOV_SVAL(IOV_RRM_NBR_REQ):
	{
#ifdef STA
		wlc_ssid_t ssid;

		if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_NEIGHBOR_REPORT))
			return BCME_UNSUPPORTED;

		if (!cfg->associated)
			return BCME_NOTASSOCIATED;

		if (alen == sizeof(wlc_ssid_t)) {
			bcopy(a, &ssid, sizeof(wlc_ssid_t));
			wlc_rrm_send_nbrreq(rrm_info, cfg, &ssid);
		}
		else {
			wlc_rrm_send_nbrreq(rrm_info, cfg, NULL);
		}
#endif /* STA */
		break;
	}
#endif /* WL11K_NBR_MEAS */
#ifdef WL11K_BCN_MEAS
	/* IOVAR SET beacon request */
	case IOV_SVAL(IOV_RRM_BCN_REQ):
	{
		char eabuf[ETHER_ADDR_STR_LEN];
		bcn_req_t *bcnreq = NULL;
		bcnreq_t bcnreq_v0;

		if (!cfg->associated)
			return BCME_NOTASSOCIATED;

		if (alen >= sizeof(bcn_req_t)) {
			bcnreq = (bcn_req_t *)MALLOCZ(wlc->osh, alen);
			if (bcnreq == NULL) {
				WL_ERROR(("%s: out of memory, malloced %d bytes\n",
					__FUNCTION__, MALLOCED(wlc->osh)));
				err = BCME_ERROR;
				break;
			}
			memcpy(bcnreq, a, alen);
		}
		else if (alen == sizeof(bcnreq_t)) {
			bcnreq = (bcn_req_t *)MALLOCZ(wlc->osh, alen);
			if (bcnreq == NULL) {
				WL_ERROR(("%s: out of memory, malloced %d bytes\n",
					__FUNCTION__, MALLOCED(wlc->osh)));
				err = BCME_ERROR;
				break;
			}
			/* convert from old struct */
			memcpy(&bcnreq_v0, a, sizeof(bcnreq_t));
			bcnreq->bcn_mode = bcnreq_v0.bcn_mode;
			bcnreq->dur = bcnreq_v0.dur;
			bcnreq->channel = bcnreq_v0.channel;
			memcpy(&bcnreq->da, &bcnreq_v0.da, sizeof(struct ether_addr));
			memcpy(&bcnreq->ssid, &bcnreq_v0.ssid, sizeof(wlc_ssid_t));
			bcnreq->reps = bcnreq_v0.reps;
			bcnreq->version = WL_RRM_BCN_REQ_VER;
		}
		else {
			WL_ERROR(("wl%d: %s IOV_RRM_BCN_REQ: len of req: %d not match\n",
				rrm_info->wlc->pub->unit, __FUNCTION__, alen));
			err = BCME_ERROR;
			break;
		}
		if (bcnreq->version == WL_RRM_BCN_REQ_VER) {
			if (bcnreq->bcn_mode == DOT11_RMREQ_BCN_PASSIVE) {
				if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_BCN_PASSIVE))
					err =  BCME_UNSUPPORTED;
			} else if (bcnreq->bcn_mode == DOT11_RMREQ_BCN_ACTIVE) {
				if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_BCN_ACTIVE))
					err =  BCME_UNSUPPORTED;
			} else if (bcnreq->bcn_mode == DOT11_RMREQ_BCN_TABLE) {
				if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_BCN_TABLE))
					err =  BCME_UNSUPPORTED;
			} else {
				err = BCME_ERROR;
			}
			if (err == BCME_OK)
			{
				wlc_rrm_send_bcnreq(rrm_info, cfg, bcnreq);
				bcm_ether_ntoa(&bcnreq->da, eabuf);
				WL_ERROR(("IOV_RRM_BCN_REQ: da: %s, mode: %d, dur: %d, chan: %d\n",
				eabuf, bcnreq->bcn_mode, bcnreq->dur,
				bcnreq->channel));
			}
		}
		else {
			WL_ERROR(("wl%d: %s IOV_RRM_BCN_REQ: len of req: %d not match\n",
				rrm_info->wlc->pub->unit, __FUNCTION__, alen));
		}
		if (bcnreq)
			MFREE(wlc->osh, bcnreq, sizeof(*bcnreq));
		break;
	}
#endif /* WL11K_BCN_MEAS */

#ifdef WL11K_ALL_MEAS
	/* IOVAR SET channel load request */
	case IOV_SVAL(IOV_RRM_CHLOAD_REQ):
	{
		rrmreq_t chloadreq;

		if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_CLM))
			return BCME_UNSUPPORTED;

		bcopy(a, &chloadreq, sizeof(rrmreq_t));
		wlc_rrm_send_chloadreq(rrm_info, cfg, &chloadreq);
		break;
	}

	/* IOVAR SET noise request */
	case IOV_SVAL(IOV_RRM_NOISE_REQ):
	{
		rrmreq_t noisereq;

		if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_NHM))
			return BCME_UNSUPPORTED;

		bcopy(a, &noisereq, sizeof(rrmreq_t));
		wlc_rrm_send_noisereq(rrm_info, cfg, &noisereq);
		break;
	}

	/* IOVAR SET frame request */
	case IOV_SVAL(IOV_RRM_FRAME_REQ):
	{
		framereq_t framereq;

		if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_FM))
			return BCME_UNSUPPORTED;

		bcopy(a, &framereq, sizeof(framereq_t));
		wlc_rrm_send_framereq(rrm_info, cfg, &framereq);
		break;
	}

	/* IOVAR SET statistics request */
	case IOV_SVAL(IOV_RRM_STAT_REQ):
	{
		statreq_t statreq;

		if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_SM))
			return BCME_UNSUPPORTED;

		bcopy(a, &statreq, sizeof(statreq_t));
		wlc_rrm_send_statreq(rrm_info, cfg, &statreq);
		break;
	}

	/* IOVAR GET statistics report */
	case IOV_GVAL(IOV_RRM_STAT_RPT):
	{
		statrpt_t *param, *rpt;
		struct scb *scb;
		rrm_scb_info_t *scb_info;

		if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_SM))
			return BCME_UNSUPPORTED;

		if (alen < sizeof(statrpt_t)) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		rpt = (statrpt_t*)a;
		param = (statrpt_t*)p;

		if ((param == NULL) || ETHER_ISMULTI(&param->addr)) {
			err = BCME_BADARG;
			break;
		}

		if (param->ver != WL_RRM_RPT_VER) {
			WL_ERROR(("ver[%d] mismatch[%d]\n", param->ver, WL_RRM_RPT_VER));
			err = BCME_VERSION;
			break;
		}

		if ((scb = wlc_scbfind(wlc, cfg, &param->addr)) == NULL) {
			WL_ERROR(("wl%d: %s: non-associated station"MACF"\n",
				wlc->pub->unit, __FUNCTION__, ETHER_TO_MACF(param->addr)));
			err = BCME_NOTASSOCIATED;
			break;
		}
		scb_info = RRM_SCB_INFO(rrm_info, scb);

		if (!scb_info) {
			WL_ERROR(("wl%d: %s: "MACF" scb_info is NULL\n",
				wlc->pub->unit, __FUNCTION__, ETHER_TO_MACF(param->addr)));
			err = BCME_ERROR;
			break;
		}

		rpt->ver = WL_RRM_RPT_VER;
		rpt->flag = scb_info->flag;
		rpt->len = scb_info->len;
		rpt->timestamp = scb_info->timestamp;
		memcpy(rpt->data, scb_info->data, rpt->len);

		WL_INFORM(("ver:%d timestamp:%u flag:%d len:%d\n",
			rpt->ver, rpt->timestamp, rpt->flag, rpt->len));

		break;
	}

	case IOV_SVAL(IOV_RRM_TXSTREAM_REQ):
	{
		txstrmreq_t txstrmreq;

		if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_TSCM)) {
			WL_ERROR(("tx strm not supported\n"));
			return BCME_UNSUPPORTED;
		}

		bcopy(a, &txstrmreq, sizeof(txstrmreq_t));
		wlc_rrm_send_txstrmreq(rrm_info, cfg, &txstrmreq);
		break;
	}

	case IOV_SVAL(IOV_RRM_LCI_REQ):
	{
#ifdef WL_PROXDETECT
		lci_req_t lcireq;
#else
		lcireq_t lcireq;
#endif // endif

		if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_LCIM))
			return BCME_UNSUPPORTED;

		bcopy(a, &lcireq, sizeof(lcireq_t));
		wlc_rrm_send_lcireq(rrm_info, cfg, &lcireq);
		break;
	}

	case IOV_SVAL(IOV_RRM_CIVIC_REQ):
	{
#ifdef WL_PROXDETECT
		civic_req_t civicreq;
#else
		civicreq_t civicreq;
#endif // endif

		if (!isset(cfg->ext_cap, DOT11_EXT_CAP_CIVIC_LOC))
			return BCME_UNSUPPORTED;

		bcopy(a, &civicreq, sizeof(civicreq_t));
		wlc_rrm_send_civicreq(rrm_info, cfg, &civicreq);
		break;
	}

	case IOV_SVAL(IOV_RRM_LOCID_REQ):
	{
		locidreq_t locidreq;

		if (!isset(cfg->ext_cap, DOT11_EXT_CAP_IDENT_LOC))
			return BCME_UNSUPPORTED;

		bcopy(a, &locidreq, sizeof(locidreq_t));
		wlc_rrm_send_locidreq(rrm_info, cfg, &locidreq);
		break;
	}

	/* IOVAR SET link measurement request */
	case IOV_SVAL(IOV_RRM_LM_REQ):
	{
		struct ether_addr da;
		char eabuf[ETHER_ADDR_STR_LEN];

		if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_LINK))
			return BCME_UNSUPPORTED;

		if (alen == sizeof(struct ether_addr)) {
			bcopy(a, &da, sizeof(struct ether_addr));
			wlc_rrm_send_lmreq(rrm_info, cfg, &da);

			bcm_ether_ntoa(&da, eabuf);
		}
		else {
			WL_ERROR(("wl%d: %s IOV_RRM_LM_REQ: len of req: %d not match\n",
				rrm_info->wlc->pub->unit, __FUNCTION__, alen));
		}
		break;
	}
#endif /* WL11K_ALL_MEAS */

#ifdef WL11K_NBR_MEAS
	case IOV_GVAL(IOV_RRM_NBR_REPORT):
	{
#ifdef WL11K_AP
		uint16 list_cnt;
#endif /* WL11K_AP */
		if (BSSCFG_STA(cfg))
			return BCME_UNSUPPORTED;

#ifdef WL11K_AP
		if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_NEIGHBOR_REPORT))
			return BCME_UNSUPPORTED;

		if (plen == 0) {
			list_cnt = wlc_rrm_get_neighbor_count(rrm_info->wlc, cfg, NBR_ADD_STATIC);
			store16_ua((uint8 *)a, list_cnt);
			break;
		}

		list_cnt = load16_ua((uint8 *)p);

		wlc_rrm_get_nbr_report(rrm_info->wlc, cfg, list_cnt, a);
#endif /* WL11K_AP */
		break;
	}
	case IOV_GVAL(IOV_RRM_NBR_LIST):
	{
#ifdef WL11K_AP
		uint16 list_cnt;
		nbr_list_t *nbr_list = (nbr_list_t *)a;
		nbr_rpt_elem_t *nbr_elements;
#endif /* WL11K_AP */

		if (BSSCFG_STA(cfg))
			return BCME_UNSUPPORTED;

#ifdef WL11K_AP
		if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_NEIGHBOR_REPORT)) {
			return BCME_UNSUPPORTED;
		}

		list_cnt = wlc_rrm_get_neighbor_count(rrm_info->wlc, cfg, NBR_ADD_STATIC);

		nbr_list->version = WL_RRM_NBR_RPT_VER;
		nbr_list->count = list_cnt;
		nbr_list->fixed_length = sizeof(nbr_list_t);
		nbr_list->length = sizeof(nbr_list_t) +
			(list_cnt * sizeof(nbr_rpt_elem_t));
		if (list_cnt == 0) {
			break;
		}

		nbr_elements = (nbr_rpt_elem_t *)((uint8 *)nbr_list + nbr_list->fixed_length);
		wlc_rrm_get_neighbor_list(rrm_info, nbr_elements, list_cnt, rrm_cfg);
#endif /* WL11K_AP */
		break;
	}

#ifdef WL11K_AP
	case IOV_SVAL(IOV_RRM_NBR_DEL_NBR):
	{
		struct ether_addr ea;

		if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_NEIGHBOR_REPORT))
			return BCME_UNSUPPORTED;

		if (alen == sizeof(struct ether_addr)) {
			/* Flush all neighbors if input address is broadcast */
			if (ETHER_ISBCAST(a)) {
				wlc_rrm_flush_neighbors(rrm_info, cfg, NBR_ADD_STATIC);
				break;
			}
			bcopy(a, &ea, sizeof(struct ether_addr));
			wlc_rrm_del_neighbor(rrm_info, &ea, wlcif, rrm_cfg);
		}
		else {
			WL_ERROR(("wl%d: %s IOV_RRM_NBR_DEL_NBR: len of req: %d not match\n",
				rrm_info->wlc->pub->unit, __FUNCTION__, alen));
		}

		break;
	}

	case IOV_SVAL(IOV_RRM_NBR_ADD_NBR):
	{
		nbr_rpt_elem_t nbr_elt;
		memset(&nbr_elt, 0, sizeof(nbr_rpt_elem_t));

		if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_NEIGHBOR_REPORT))
			return BCME_UNSUPPORTED;

		if (alen == TLV_HDR_LEN + DOT11_NEIGHBOR_REP_IE_FIXED_LEN) {
			memcpy(((uint8 *)&nbr_elt + OFFSETOF(nbr_rpt_elem_t, id)), a,
				TLV_HDR_LEN + DOT11_NEIGHBOR_REP_IE_FIXED_LEN);
			nbr_elt.version = WL_RRM_NBR_RPT_VER;
		}
		else if ((alen > TLV_HDR_LEN + DOT11_NEIGHBOR_REP_IE_FIXED_LEN) &&
			(alen <= sizeof(nbr_elt))) {
			memcpy(&nbr_elt, a, alen);
		}
		if (nbr_elt.version == WL_RRM_NBR_RPT_VER) {
			wlc_rrm_add_neighbor(rrm_info, &nbr_elt, wlcif, rrm_cfg, NBR_ADD_STATIC);
		}
		else {
			WL_ERROR(("wl%d: %s IOV_RRM_NBR_ADD_NBR: len of req: %d not match\n",
				rrm_info->wlc->pub->unit, __FUNCTION__, alen));
		}
		break;
	}

	case IOV_SVAL(IOV_RRM_NBR_SCAN):
	{
		rrm_cfg->nbr_scan = *(uint8 *)a;
		break;
	}

	case IOV_GVAL(IOV_RRM_NBR_SCAN):
	{
		*(uint8 *)a = rrm_cfg->nbr_scan;
		break;
	}
	case IOV_SVAL(IOV_RRM_NBR):
	{
		uint8 *opt_civic = NULL;
		uint8 *opt_lci = NULL;
		nbr_rpt_elem_t nbr_elt;
		struct ether_addr *nbr_bss;
		char *cmdname = a;
		char *ab = (char *) a;

		if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_NEIGHBOR_REPORT)) {
			WL_ERROR(("wl%d: %s IOV_RRM_NBR: neighbor report"
				" capability disabled\n", rrm_info->wlc->pub->unit,
				__FUNCTION__));
			return BCME_UNSUPPORTED;
		}

		a = (void *) &ab[strlen(cmdname)+1];
		alen -= (strlen(cmdname)+1);

		if (strcmp(cmdname, "civic") == 0) {
			if (alen < (sizeof(struct ether_addr) + DOT11_FTM_CIVIC_UNKNOWN_LEN)) {
				WL_ERROR(("wl%d: %s IOV_RRM_NBR: CIVIC: alen %d too short\n",
					rrm_info->wlc->pub->unit, __FUNCTION__, alen));
				return BCME_BADARG;
			}

			nbr_bss = (struct ether_addr *) a;
			alen -= sizeof(struct ether_addr);

			if (alen > 0) {
				opt_civic = (uint8 *) a;
				opt_civic += sizeof(struct ether_addr);
			}

			return wlc_rrm_add_neighbor_civic(rrm_info, nbr_bss, opt_civic,
				alen, rrm_cfg);
		}
		else if (strcmp(cmdname, "lci") == 0) {
			if (alen < (sizeof(struct ether_addr) + DOT11_FTM_LCI_UNKNOWN_LEN)) {
				WL_ERROR(("wl%d: %s IOV_RRM_NBR: LCI: alen %d too short\n",
					rrm_info->wlc->pub->unit, __FUNCTION__, alen));
				return BCME_BADARG;
			}

			nbr_bss = (struct ether_addr *) a;
			alen -= sizeof(struct ether_addr);

			if (alen > 0) {
				opt_lci = (uint8 *) a;
				opt_lci += sizeof(struct ether_addr);
			}

			return wlc_rrm_add_neighbor_lci(rrm_info, nbr_bss, opt_lci,
				alen, rrm_cfg);
		} else if (strcmp(cmdname, "ssid") == 0) {
			/* ssid */
			memset(&nbr_elt, 0, sizeof(nbr_rpt_elem_t));

			if (alen != sizeof(nbr_elt)) {
				WL_ERROR(("wl%d: %s IOV_RRM_NBR: SSID: alen %d too short\n",
					rrm_info->wlc->pub->unit, __FUNCTION__, alen));
				return BCME_BADARG;
			}

			memcpy(&nbr_elt, a, alen);

			if (nbr_elt.version == WL_RRM_NBR_RPT_VER) {
				wlc_rrm_add_neighbor_ssid(&nbr_elt, rrm_cfg);
			}
			else {
				err = BCME_VERSION;
			}
		} else if (strcmp(cmdname, "chanspec") == 0) {
			/* wide bandwidth channel */
			memset(&nbr_elt, 0, sizeof(nbr_rpt_elem_t));

			if (alen != sizeof(nbr_elt)) {
				WL_ERROR(("wl%d: %s IOV_RRM_NBR: chanspec: alen %d too short\n",
					rrm_info->wlc->pub->unit, __FUNCTION__, alen));
				return BCME_BADARG;
			}

			memcpy(&nbr_elt, a, alen);

			if (nbr_elt.version == WL_RRM_NBR_RPT_VER) {
				wlc_rrm_add_neighbor_chanspec(&nbr_elt, rrm_cfg);
			}
			else {
				err = BCME_VERSION;
			}
		}

		break;
	}
#endif /* WL11K_AP */
#endif /* WL11K_NBR_MEAS */
#ifdef WL_FTM_11K
#if defined(WL_PROXDETECT) && defined(WL_FTM)
	case IOV_SVAL(IOV_RRM_FRNG):
	{
		if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_FTM_RANGE))
			return BCME_UNSUPPORTED;

		err = rrm_frng_set(rrm_info, cfg, p, plen, a, alen);
		break;
	}
#endif /* WL_PROXDETECT && WL_FTM */
#endif /* WL_FTM_11K */
#ifdef WL11K_BCN_MEAS
	case IOV_SVAL(IOV_RRM_BCN_REQ_THRTL_WIN):
	{
		if (p && (plen >= (int)sizeof(rrm_info->bcn_req_thrtl_win))) {
			rrm_info->bcn_req_thrtl_win = *(uint32 *)p;
		}

		/* Sanity check: throttle window must always be > off chan time */
		if (rrm_info->bcn_req_thrtl_win <
			rrm_info->bcn_req_off_chan_time_allowed/WLC_RRM_MS_IN_SEC) {
			rrm_info->bcn_req_thrtl_win = 0;
			rrm_info->bcn_req_off_chan_time_allowed = 0;
		}
		break;
	}
	case IOV_GVAL(IOV_RRM_BCN_REQ_THRTL_WIN):
	{
		if (a && (alen >= (int)sizeof(rrm_info->bcn_req_thrtl_win))) {
			*((uint32 *)a) = rrm_info->bcn_req_thrtl_win;
		}
		break;
	}
	case IOV_SVAL(IOV_RRM_BCN_REQ_MAX_OFF_CHAN_TIME):
	{
		if (p && (plen >= (int)sizeof(rrm_info->bcn_req_off_chan_time_allowed))) {
			rrm_info->bcn_req_off_chan_time_allowed = *(uint32 *)p * WLC_RRM_MS_IN_SEC;
		}

		/* Sanity check: throttle window must always be > off chan time */
		if (rrm_info->bcn_req_thrtl_win <
			rrm_info->bcn_req_off_chan_time_allowed/WLC_RRM_MS_IN_SEC) {
			rrm_info->bcn_req_thrtl_win = 0;
			rrm_info->bcn_req_off_chan_time_allowed = 0;
		}
		break;
	}
	case IOV_GVAL(IOV_RRM_BCN_REQ_MAX_OFF_CHAN_TIME):
	{
		if (a && (alen >= (int)sizeof(rrm_info->bcn_req_off_chan_time_allowed))) {
			*((uint32 *)a) = rrm_info->bcn_req_off_chan_time_allowed/WLC_RRM_MS_IN_SEC;
		}
		break;
	}
	case IOV_SVAL(IOV_RRM_BCN_REQ_TRAFF_MEAS_PER):
	{
		if (p && (plen >= (int)sizeof(rrm_info->bcn_req_traff_meas_prd))) {
			rrm_info->bcn_req_traff_meas_prd = *((uint32 *)p);
		}
		break;
	}
	case IOV_GVAL(IOV_RRM_BCN_REQ_TRAFF_MEAS_PER):
	{
		if (a && (alen >= (int)sizeof(rrm_info->bcn_req_traff_meas_prd))) {
			*((uint32 *)a) = rrm_info->bcn_req_traff_meas_prd;
		}
		break;
	}
#endif /* WL11K_BCN_MEAS */
#ifdef WL11K_ALL_MEAS
	case IOV_SVAL(IOV_RRM_CONFIG):
	{
		err = rrm_config_set(rrm_info, cfg, p, plen, a, alen);
		break;
	}

	case IOV_GVAL(IOV_RRM_CONFIG):
	{
		err = rrm_config_get(rrm_info, cfg, p, plen, a, alen);
		break;
	}
#endif /* WL11K_ALL_MEAS */
	default:
	{
		err = BCME_UNSUPPORTED;
		break;
	}
	}

	return (err);
}

#ifdef WL11K_ALL_MEAS
/* FIXME: PR81518: need a wlc_bsscfg_t param to correctly determine addresses and
 * queue for wlc_sendmgmt()
 */
static void
wlc_rrm_recv_lmreq(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, struct scb *scb, uint8 *body,
	int body_len, int8 rssi, ratespec_t rspec)
{
	wlc_info_t *wlc = rrm_info->wlc;
	dot11_lmreq_t *lmreq;
	dot11_lmrep_t *lmrep;
	uint8 *pbody;
	void *p;
	uint8 rcv_antidx, tx_antidx;

	rrm_bsscfg_cubby_t *rrm_cfg;

	rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, cfg);
	ASSERT(rrm_cfg != NULL);

	WL_RRM(("<802.11K Rx Req> %s: <Enter>\n", __FUNCTION__));
	WL_RRM_HEX("rrm_cfg->rrm_cap:", rrm_cfg->rrm_cap, DOT11_RRM_CAP_LEN);

	if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_LINK)) {
		WL_ERROR(("wl%d: %s: Unsupported\n", wlc->pub->unit, __FUNCTION__));
		return;
	}

	lmreq = (dot11_lmreq_t *)body;

	if (body_len < DOT11_LMREQ_LEN) {
		WL_ERROR(("wl%d: %s invalid body len %d in rmreq\n", rrm_info->wlc->pub->unit,
		          __FUNCTION__, body_len));
		return;
	}

	p = wlc_frame_get_action(wlc, &scb->ea,	&cfg->cur_etheraddr,
		&cfg->BSSID, DOT11_LMREP_LEN, &pbody, DOT11_ACTION_CAT_RRM);
	if (p == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return;
	}

	WL_RRM(("RRM-LM_RECVREQ %s: token=%d txpwr=%d maxtxpwr=%d \n",
		__FUNCTION__, lmreq->token, lmreq->txpwr, lmreq->maxtxpwr));

	phy_stf_chain_get(WLC_PI(wlc), &tx_antidx, &rcv_antidx);

	WL_RRM(("RRM-LM_RECVREQ %s: wlc_phy_stf_chain_get tx_antidx=%d rcv_antidx=%d ....\n",
		__FUNCTION__, tx_antidx, rcv_antidx));

	lmrep = (dot11_lmrep_t *)pbody;
	lmrep->category = DOT11_ACTION_CAT_RRM;
	lmrep->action = DOT11_RM_ACTION_LM_REP;
	lmrep->token = lmreq->token;
	lmrep->rxant = rcv_antidx;
	lmrep->txant = wlc->stf->txant;
	lmrep->rcpi = (uint8)rssi;

	/* tpc report element */
	wlc_tpc_rep_build(wlc, rssi, rspec, &lmrep->tpc);

	WL_RRM(("RRM-LM_SENDREP %s: body_len=%d token=%d rxant=%d txant=%d "
		"txpwr=%d linkMargin=%d rcpi=%d rsni=%d\n",
		__FUNCTION__, body_len, lmrep->token, lmrep->rxant, lmrep->txant,
		(int)(lmrep->tpc.tx_pwr), (int)(lmrep->tpc.margin),
		(int8)(lmrep->rcpi), (int8)(lmrep->rsni)));

	wlc_sendmgmt(wlc, p, cfg->wlcif->qi, scb);
}

static void
wlc_rrm_recv_lmrep(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, struct scb *scb,
	uint8 *body, int body_len)
{
	rrm_bsscfg_cubby_t *rrm_cfg;
#ifdef BCMDBG
	dot11_lmrep_t *lmrep = (dot11_lmrep_t *)&body[0];
#endif // endif

	rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, cfg);
	ASSERT(rrm_cfg != NULL);

	if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_LINK)) {
		WL_ERROR(("wl%d: %s: Unsupported\n", rrm_info->wlc->pub->unit,
		          __FUNCTION__));
		return;
	}

	if (body_len < DOT11_LMREP_LEN) {
		WL_ERROR(("wl%d: %s invalid body len %d in rmrep\n", rrm_info->wlc->pub->unit,
		          __FUNCTION__, body_len));
		return;
	}

#if defined(BCMDBG)
	WL_RRM_HEX("wlc_rrm_recv_lmrep:", body, body_len);

	WL_RRM(("<802.11K Rx Rep> %s: body_len=%d token=%d rxant=%d "
		"txant=%d txpwr=%d linkMargin=%d rcpi=%d rsni=%d\n",
		__FUNCTION__, body_len, lmrep->token, lmrep->rxant, lmrep->txant,
		(int)(lmrep->tpc.tx_pwr), (int)(lmrep->tpc.margin),
		(int8)(lmrep->rcpi), (int8)(lmrep->rsni)));
#endif // endif

	wlc_rrm_send_event(rrm_info, scb, (char *)body, body_len, DOT11_RM_ACTION_LM_REP, 0);

	return;
}
#endif /* WL11K_ALL_MEAS */

static chanspec_t
wlc_rrm_chanspec(wlc_rrm_info_t *rrm_info)
{
	wlc_rrm_req_state_t *rrm_state = rrm_info->rrm_state;
	wlc_rrm_req_t *req;
	int req_idx;
	chanspec_t chanspec = 0;

	for (req_idx = rrm_state->cur_req; req_idx < rrm_state->req_count; req_idx++) {
		req = &rrm_state->req[req_idx];

		/* check for end of parallel measurements */
		if (req_idx != rrm_state->cur_req &&
		    (req->flags & DOT11_RMREQ_MODE_PARALLEL) == 0) {
			break;
		}

		/* check for request that have already been marked to skip */
		if (req->flags & (DOT11_RMREP_MODE_INCAPABLE| DOT11_RMREP_MODE_REFUSED))
			continue;

		if (req->dur > 0) {
			/* found the first non-zero dur request that will not be skipped */
			chanspec = req->chanspec;
			break;
		}
	}

	return chanspec;
}

static void wlc_rrm_allreq_free(wlc_rrm_info_t *rrm_info)
{
	wlc_info_t *wlc = rrm_info->wlc;
	rrm_bcnreq_t *bcn_req;
	rrm_framereq_t *frame_req;
	rrm_noisereq_t *noise_req;
	rrm_chloadreq_t *chload_req;
	rrm_statreq_t *stat_req;
	rrm_txstrmreq_t *txstrm_req;

	bcn_req = rrm_info->bcnreq;
	if (bcn_req) {
		wlc_bss_list_free(wlc, &bcn_req->scan_results);
		MFREE(wlc->osh, bcn_req, sizeof(rrm_bcnreq_t));
	}
	rrm_info->bcnreq = NULL;

	/* add other pointers here */
	frame_req = rrm_info->framereq;
	if (frame_req) {
		MFREE(wlc->osh, frame_req, sizeof(rrm_framereq_t));
	}
	rrm_info->framereq = NULL;

	noise_req = rrm_info->noisereq;
	if (noise_req) {
		wl_del_timer(wlc->wl, rrm_info->rrm_noise_ipi_timer);
		MFREE(wlc->osh, noise_req, sizeof(rrm_noisereq_t));
	}
	rrm_info->noisereq = NULL;

	chload_req = rrm_info->chloadreq;
	if (chload_req) {
		MFREE(wlc->osh, chload_req, sizeof(rrm_chloadreq_t));
	}
	rrm_info->chloadreq = NULL;

	stat_req = rrm_info->statreq;
	if (stat_req) {
		MFREE(wlc->osh, stat_req, sizeof(rrm_statreq_t));
	}
	rrm_info->statreq = NULL;

	txstrm_req = rrm_info->txstrmreq;
	if (txstrm_req) {
		MFREE(wlc->osh, txstrm_req, sizeof(rrm_txstrmreq_t));
	}
	rrm_info->txstrmreq = NULL;
}

static void
wlc_rrm_free(wlc_rrm_info_t *rrm_info)
{
	wlc_rrm_req_state_t *rrm_state = rrm_info->rrm_state;
	wlc_info_t *wlc = rrm_info->wlc;

	WL_INFORM(("%s: free memory\n", __FUNCTION__));

	if (rrm_state->req)
		MFREE(wlc->osh, rrm_state->req, rrm_state->req_count * sizeof(wlc_rrm_req_t));

	wl_del_timer(wlc->wl, rrm_info->rrm_timer);
	wlc_rrm_state_upd(rrm_info, WLC_RRM_IDLE);
	WL_INFORM(("%s: state upd to %d\n", __FUNCTION__, rrm_state->step));

	rrm_state->scan_active = FALSE;
	wlc_rrm_allreq_free(rrm_info);
#ifdef WL_FTM_11K
#if defined(WL_PROXDETECT) && defined(WL_FTM)
	if (PROXD_ENAB(wlc->pub)) {
		wlc_rrm_frngreq_free(rrm_info);
	}
#endif /* WL_PROXDETECT && WL_FTM */
#endif /* WL_FTM_11K */
	/* update ps control */
	wlc_set_wake_ctrl(wlc);
	rrm_state->chanspec_return = 0;
	rrm_state->cur_req = 0;
	rrm_state->req_count = 0;
	rrm_state->req = NULL;
	rrm_state->reps = 0;
	rrm_info->dialog_token = 0;
	return;
}

void
wlc_rrm_terminate(wlc_rrm_info_t *rrm_info)
{
	wlc_info_t *wlc = rrm_info->wlc;
#ifdef STA
	int idx;
	wlc_bsscfg_t *cfg;
#endif // endif
	/* enable CFP & TSF update */
	wlc_mhf(wlc, MHF2, MHF2_SKIP_CFP_UPDATE, 0, WLC_BAND_ALL);
	wlc_skip_adjtsf(wlc, FALSE, NULL, WLC_SKIP_ADJTSF_RM, WLC_BAND_ALL);

#ifdef STA
	/* come out of PS mode if appropriate */
	FOREACH_BSS(wlc, idx, cfg) {
		if (!BSSCFG_STA(cfg))
			continue;
		/* un-block PSPoll operations and restore PS state */
		mboolclr(cfg->pm->PMblocked, WLC_PM_BLOCK_CHANSW);
		if (cfg->pm->PM != PM_MAX || cfg->pm->WME_PM_blocked) {
			WL_RTDC(wlc, "wlc_rrm_meas_end: exit PS", 0, 0);
			wlc_set_pmstate(cfg, FALSE);
			wlc_pm2_sleep_ret_timer_start(cfg, 0);
		}
	}
#endif /* STA */
	wlc_set_wake_ctrl(wlc);
}

static int
wlc_rrm_abort(wlc_info_t *wlc)
{
	wlc_rrm_info_t *rrm_info = wlc->rrm_info;
	wlc_rrm_req_state_t *rrm_state = rrm_info->rrm_state;
	bool canceled = FALSE;

	if (rrm_state->step == WLC_RRM_IDLE) {
		return TRUE;
	}
	if (rrm_state->step == WLC_RRM_ABORT) {
		/* timer has been canceled, but not fired yet */
		return FALSE;
	}

	if (rrm_state->scan_active == TRUE)
		wlc_scan_abort(wlc->scan, WLC_E_STATUS_ABORT);

	wlc_rrm_state_upd(rrm_info, WLC_RRM_ABORT);
	WL_ERROR(("%s: state upd to %d\n", __FUNCTION__, rrm_state->step));

	/* cancel any timers and clear state */
	if (wl_del_timer(wlc->wl, rrm_info->rrm_timer)) {
		wlc_rrm_state_upd(rrm_info, WLC_RRM_IDLE);
		WL_ERROR(("%s: state upd to %d\n", __FUNCTION__, rrm_state->step));
		canceled = TRUE;
	}

	/* Change the radio channel to the return channel */
	if ((rrm_state->chanspec_return != 0) &&
	(WLC_BAND_PI_RADIO_CHANSPEC != rrm_state->chanspec_return)) {
		wlc_suspend_mac_and_wait(wlc);
		WL_ERROR(("%s: return to the home channel\n", __FUNCTION__));
		wlc_set_chanspec(wlc, rrm_state->chanspec_return, CHANSW_REASON(CHANSW_IOVAR));
		wlc_enable_mac(wlc);
	}

	wlc_rrm_terminate(wlc->rrm_info);

	/* un-suspend the data fifos in case they were suspended
	 * for off channel measurements
	 */
	wlc_tx_resume(wlc);

	wlc_rrm_free(rrm_info);

	return canceled;
}

static void
wlc_rrm_report_11k(wlc_rrm_info_t *rrm_info, wlc_rrm_req_t *req_block, int count)
{
	int i;
	wlc_rrm_req_t *req;
#ifdef WL_FTM_11K
#if defined(WL_PROXDETECT) && defined(WL_FTM)
	frngrep_t *frng_rep;
#endif // endif
#endif /* WL_FTM_11K */
#ifdef WL11K_BCN_MEAS
	int status;
	rrm_bcnreq_t *bcn_req;
#endif // endif
	rrm_bsscfg_cubby_t *rrm_cfg;

	rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, rrm_info->cur_cfg);
	ASSERT(rrm_cfg != NULL);
	BCM_REFERENCE(rrm_cfg);

	WL_RRM(("%s: <Enter> count %d\n", __FUNCTION__, count));

	for (i = 0; i < count; i++) {
		req = req_block + i;

		if (req->flags & DOT11_RMREP_MODE_INCAPABLE) {
			WL_RRM(("%s: DOT11_RMREP_MODE_INCAPABLE\n", __FUNCTION__));
			wlc_rrm_rep_err(rrm_info, (req->type <= DOT11_RMREQ_BCN_TABLE) ?
				DOT11_MEASURE_TYPE_BEACON : req->type,
				req->token, DOT11_RMREP_MODE_INCAPABLE);
			continue;
		}
		if (req->flags & DOT11_RMREP_MODE_REFUSED) {
			WL_RRM(("%s: DOT11_RMREP_MODE_REFUSED\n", __FUNCTION__));
			wlc_rrm_rep_err(rrm_info, (req->type <= DOT11_RMREQ_BCN_TABLE) ?
				DOT11_MEASURE_TYPE_BEACON : req->type,
				req->token, DOT11_RMREP_MODE_REFUSED);
		}

		WL_RRM(("%s: type:0x%x, req->flags:0x%x\n",
			__FUNCTION__, req->type, req->flags));

		switch (req->type) {
#ifdef WL11K_BCN_MEAS
		case DOT11_RMREQ_BCN_ACTIVE:
			if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_BCN_ACTIVE)) {
				WL_RRM(("%s: not DOT11_RRM_CAP_BCN_ACTIVE\n", __FUNCTION__));
				break;
			}
		case DOT11_RMREQ_BCN_PASSIVE:
			if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_BCN_PASSIVE)) {
				WL_RRM(("%s: not DOT11_RRM_CAP_BCN_PASSIVE\n", __FUNCTION__));
				break;
			}
			bcn_req = rrm_info->bcnreq;
			if (bcn_req == NULL) {
				WL_RRM(("%s: bcn_req == NULL\n", __FUNCTION__));
				break;
			}
			status = bcn_req->scan_status;

			if (status == WLC_E_STATUS_ERROR) {
				WL_ERROR(("%s: scan status = error\n", __FUNCTION__));
				break;
			}

			if (status != WLC_E_STATUS_SUCCESS) {
				WL_ERROR(("%s: scan status != error\n", __FUNCTION__));
				wlc_rrm_rep_err(rrm_info, DOT11_MEASURE_TYPE_BEACON,
					req->token, DOT11_RMREP_MODE_REFUSED);
				break;
			}

			if (bcn_req->scan_results.count == 0) {
				WL_ERROR(("%s: scan results count = 0\n", __FUNCTION__));
				/* send a null beacon report */
				wlc_rrm_rep_err(rrm_info, DOT11_MEASURE_TYPE_BEACON,
					req->token, 0);
				break;
			}
			WL_ERROR(("%s: scan type: %d, scan results count = %d, sendout bcnrep\n",
				__FUNCTION__, req->type, bcn_req->scan_results.count));

			wlc_rrm_send_bcnrep(rrm_info, &bcn_req->scan_results);
			break;
		case DOT11_RMREQ_BCN_TABLE:
			if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_BCN_TABLE))
				break;
			if (req->flags & DOT11_RMREP_MODE_INCAPABLE) {
				wlc_rrm_rep_err(rrm_info, DOT11_MEASURE_TYPE_BEACON,
					req->token, DOT11_RMREP_MODE_INCAPABLE);
				break;
			}
			if (req->flags & DOT11_RMREP_MODE_REFUSED) {
				wlc_rrm_rep_err(rrm_info, DOT11_MEASURE_TYPE_BEACON,
					req->token, DOT11_RMREP_MODE_REFUSED);
				break;
			}
			bcn_req = rrm_info->bcnreq;
			if (bcn_req == NULL)
				break;
			WL_INFORM(("%s: BCN_TABLE\n", __FUNCTION__));

			wlc_rrm_bcnreq_scancache(rrm_info, &bcn_req->ssid, &bcn_req->bssid);
			break;
#endif /* WL11K_BCN_MEAS */
#ifdef WL11K_ALL_MEAS
		case DOT11_MEASURE_TYPE_CHLOAD:
			if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_CLM))
				break;
			wlc_rrm_cca_finish(rrm_info);
			wlc_rrm_send_chloadrep(rrm_info);
			break;
		case DOT11_MEASURE_TYPE_NOISE:
			if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_NHM))
				break;
			wlc_rrm_send_noiserep(rrm_info);
			break;
		case DOT11_MEASURE_TYPE_FRAME:
			if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_FM))
				break;
			wlc_rrm_send_framerep(rrm_info);
			break;
		case DOT11_MEASURE_TYPE_STAT:
			if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_SM))
				break;
			wlc_rrm_send_statrep(rrm_info);
			break;
		case DOT11_MEASURE_TYPE_LCI:
			wlc_rrm_send_lcirep(rrm_info, req->token);
			break;
		case DOT11_MEASURE_TYPE_TXSTREAM:
			wlc_rrm_send_txstrmrep(rrm_info);
			break;
		case DOT11_MEASURE_TYPE_CIVICLOC:
			wlc_rrm_send_civiclocrep(rrm_info, req->token);
			break;
		case DOT11_MEASURE_TYPE_LOC_ID:
			wlc_rrm_send_locidrep(rrm_info, req->token);
#endif /* WL11K_ALL_MEAS */
#ifdef WL_FTM_11K
#if defined(WL_PROXDETECT) && defined(WL_FTM)
		case DOT11_MEASURE_TYPE_FTMRANGE:
			if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_FTM_RANGE) ||
				req->flags & DOT11_RMREP_MODE_INCAPABLE) {
				wlc_rrm_rep_err(rrm_info, DOT11_MEASURE_TYPE_FTMRANGE,
					req->token, DOT11_RMREP_MODE_INCAPABLE);
				break;
			}
			frng_rep = rrm_info->rrm_frng_req->frng_rep;
			if (frng_rep == NULL)
				break;

			/* send ftm range report to requester */
			wlc_rrm_send_frngrep(rrm_info, rrm_info->cur_cfg,
				(uint8 *)frng_rep, sizeof(*frng_rep));
			if (rrm_info->rrm_frng_req->rctx &&
				PROXD_ENAB(rrm_info->wlc->pub)) {
				wlc_ftm_ranging_cancel(rrm_info->rrm_frng_req->rctx);
				wlc_ftm_ranging_destroy(&rrm_info->rrm_frng_req->rctx);
			}
			break;
#endif /* WL_PROXDETECT && WL_FTM */
#endif /* WL_FTM_11K */
		default:
			break;
		}
	}

}

static void
wlc_rrm_meas_end(wlc_info_t *wlc)
{
	wlc_rrm_info_t *rrm_info = wlc->rrm_info;
	wlc_rrm_req_state_t *rrm_state = rrm_info->rrm_state;
	wlc_rrm_req_t *req;
	int req_idx;
	int req_count;

	WL_RRM(("%s: <Enter> rrm_state->scan_active %d\n", __FUNCTION__, rrm_state->scan_active));

	if (rrm_state->scan_active) {
		WL_ERROR(("wl%d: wlc_rrm_meas_end: waiting for scan\n",
			wlc->pub->unit));
		return;
	}

	/* return to the home channel if needed */
	if (rrm_state->chanspec_return != 0) {
		wlc_suspend_mac_and_wait(wlc);
		WL_ERROR(("%s: return to the home channel\n", __FUNCTION__));
		wlc_set_chanspec(wlc, rrm_state->chanspec_return, CHANSW_REASON(CHANSW_IOVAR));
		wlc_enable_mac(wlc);
	}
	wlc_rrm_terminate(rrm_info);
	/* un-suspend the data fifos in case they were suspended
	 * * for off channel measurements
	 */
	if (!SCAN_IN_PROGRESS(wlc->scan) || ((wlc->scan->state & SCAN_STATE_WSUSPEND) == 0))
		wlc_tx_resume(wlc);
	/* count the requests for this set */
	for (req_idx = rrm_state->cur_req; req_idx < rrm_state->req_count; req_idx++) {
		req = &rrm_state->req[req_idx];
		/* check for end of parallel measurements */
		if (req_idx != rrm_state->cur_req &&
		    (req->flags & DOT11_RMREQ_MODE_PARALLEL) == 0) {
			break;
		}
	}
	req_count = req_idx - rrm_state->cur_req;
	req = &rrm_state->req[rrm_state->cur_req];

	WL_RRM(("%s: rrm_state->report_class %d\n", __FUNCTION__, rrm_state->report_class));

	switch (rrm_state->report_class) {
		case WLC_RRM_CLASS_IOCTL:
			wlc_rrm_report_ioctl(rrm_info, req, req_count);
			break;
		case WLC_RRM_CLASS_11K:
			wlc_rrm_report_11k(rrm_info, req, req_count);
			break;
		default:
			break;
	}
	/* done with the current set of measurements,
	 * advance the pointers and start the next set
	 */
	rrm_state->cur_req += req_count;
	wlc_rrm_next_set(rrm_info);
}

#ifdef WL11K_ALL_MEAS
static void
wlc_rrm_cca_measure(wlc_rrm_info_t *rrm_info, cca_ucode_counts_t *cp)
{
	int rc = wlc_bmac_cca_stats_read(rrm_info->wlc->hw, cp);
	if (rc) {
		WL_RRM(("##CCA-RRM wlc_bmac_cca_stats_read err=%d \n", rc));
	}
	WL_RRM(("##CCA-RRM## %s txdur=%d ibss=%d obss=%d noctg=%d nopkt=%d usecs=%d\n",
		__FUNCTION__, cp->txdur, cp->ibss, cp->obss, cp->noctg, cp->nopkt, cp->usecs));
}

static void
wlc_rrm_cca_start(wlc_rrm_info_t  *rrm_info, uint32 dur)
{
	wlc_rrm_req_state_t* rrm_state = rrm_info->rrm_state;

	ASSERT(rrm_info);
	ASSERT(rrm_info->wlc);

	/* Get Initial cca_stats */
	wlc_rrm_cca_measure(rrm_info, (&(rrm_state->cca_cnt_initial)));

	rrm_state->cca_idle = 0;
	rrm_state->cca_dur = dur;

	WL_RRM(("##CCA-RRM## %s: done\n", __FUNCTION__));

	return;
}

static void
wlc_rrm_cca_finish(wlc_rrm_info_t *rrm_info)
{
	wlc_rrm_req_state_t* rrm_state = rrm_info->rrm_state;
	cca_ucode_counts_t *cpSt = &(rrm_state->cca_cnt_initial);
	cca_ucode_counts_t *cpFin = &(rrm_state->cca_cnt_final);
	uint32 cca_dur_us;
	uint32 busy_us;
	uint8  frac;

	/* Get Final cca_stats */
	wlc_rrm_cca_measure(rrm_info, cpFin);

	busy_us  = cpFin->txdur - cpSt->txdur;
	busy_us += cpFin->ibss  - cpSt->ibss;
	busy_us += cpFin->obss  - cpSt->obss;
	busy_us += cpFin->noctg - cpSt->noctg;
	busy_us += cpFin->nopkt - cpSt->nopkt;

	cca_dur_us = rrm_state->cca_dur * DOT11_TU_TO_US;

	frac = (uint8)CEIL((255 * busy_us), cca_dur_us);

	rrm_state->cca_idle = cca_dur_us - busy_us;
	rrm_state->cca_busy = frac;

	WL_RRM(("##CCA-RRM## %s: cca_dur=%d cp_usecs=%d busy_us=%d frac=%d \n",
		__FUNCTION__, rrm_state->cca_dur, (cpFin->usecs - cpSt->usecs), busy_us, frac));

	return;
}
#endif /* WL11K_ALL_MEAS */

static void
wlc_rrm_report_ioctl(wlc_rrm_info_t *rrm_info, wlc_rrm_req_t *req_block, int count)
{
	WL_ERROR(("wl%d: %s dummy function\n", rrm_info->wlc->pub->unit,
	          __FUNCTION__));
}

static void
wlc_rrm_begin(wlc_rrm_info_t *rrm_info)
{
	int dur_max;
	chanspec_t rrm_chanspec;
	wlc_rrm_req_state_t *rrm_state = rrm_info->rrm_state;
	wlc_rrm_req_t *req = NULL;
	int req_idx;
	bool blocked = FALSE;
	wlc_info_t *wlc = rrm_info->wlc;
	uint32 dur_ms = 0;
	uint32 rrm_done_chanswitch = 0;

	rrm_chanspec = wlc_rrm_chanspec(rrm_info);

	WL_INFORM(("%s: rrm_chanspec: %d\n", __FUNCTION__, rrm_chanspec));

	if (rrm_chanspec != 0)
		blocked = (wlc_mac_request_entry(wlc, NULL, WLC_ACTION_RM, 0) != BCME_OK);

	WL_INFORM(("rrm_chanspec[%x] WLC_BAND_PI_RADIO_CHANSPEC[%x]",
		rrm_chanspec, WLC_BAND_PI_RADIO_CHANSPEC));

	WL_RRM(("%s: <Enter> rrm_chanspec %d\n", __FUNCTION__, rrm_chanspec));

	/* check for a channel change */
	if (!blocked &&
	    rrm_chanspec != 0 &&
	    wf_chspec_ctlchan(rrm_chanspec) != wf_chspec_ctlchan(WLC_BAND_PI_RADIO_CHANSPEC)) {
		/* has the PS announcement completed yet? */
		if (rrm_state->ps_pending) {
			/* wait for PS state to be communicated before switching channels */
			wlc_rrm_state_upd(rrm_info, WLC_RRM_WAIT_PS_ANNOUNCE);
			WL_ERROR(("%s, state upd to %d\n", __FUNCTION__, rrm_state->step));
			return;
		}
		/* has the suspend completed yet? */
		if (wlc->pub->associated && !wlc_tx_suspended(wlc)) {
			/* suspend tx data fifos for off channel measurements */
			wlc_tx_suspend(wlc);
			/* wait for the suspend before switching channels */
			wlc_rrm_state_upd(rrm_info, WLC_RRM_WAIT_TX_SUSPEND);
			WL_ERROR(("%s, state upd to %d\n", __FUNCTION__, rrm_state->step));
			return;
		}
		if (CHSPEC_CHANNEL(rrm_chanspec) != 0 &&
			CHSPEC_CHANNEL(rrm_chanspec) != 255) {
			rrm_state->chanspec_return = WLC_BAND_PI_RADIO_CHANSPEC;
			/* skip CFP & TSF update */
			wlc_mhf(wlc, MHF2, MHF2_SKIP_CFP_UPDATE, MHF2_SKIP_CFP_UPDATE,
				WLC_BAND_ALL);
			wlc_skip_adjtsf(wlc, TRUE, NULL, WLC_SKIP_ADJTSF_RM, WLC_BAND_ALL);
			wlc_suspend_mac_and_wait(wlc);
			WL_ERROR(("%s: calling wlc_set_chanspec\n", __FUNCTION__));
			wlc_set_chanspec(wlc, rrm_chanspec, CHANSW_REASON(CHANSW_IOVAR));
			wlc_enable_mac(wlc);
			rrm_done_chanswitch = 1;
		}
	} else {
		rrm_state->chanspec_return = 0;
		if (rrm_chanspec != 0 &&
		    (wf_chspec_ctlchan(rrm_chanspec) !=
		     wf_chspec_ctlchan(WLC_BAND_PI_RADIO_CHANSPEC)))
			WL_ERROR(("wl%d: %s: fail to set channel "
				"for rm due to rm request is blocked\n",
				wlc->pub->unit, __FUNCTION__));
	}

	/* record the actual start time of the measurements */
	wlc_read_tsf(wlc, &rrm_state->actual_start_l, &rrm_state->actual_start_h);

	dur_max = 0;

	for (req_idx = rrm_state->cur_req; req_idx < rrm_state->req_count; req_idx++) {
		req = &rrm_state->req[req_idx];

		/* check for end of parallel measurements */
		if (req_idx != rrm_state->cur_req &&
		    (req->flags & DOT11_RMREQ_MODE_PARALLEL) == 0) {
			WL_ERROR(("%s: req_idx: %d, NOT PARALLEL\n",
				__FUNCTION__, req_idx));
			break;
		}

		/* check for request that have already been marked to skip */
		if (req->flags & (DOT11_RMREP_MODE_INCAPABLE | DOT11_RMREP_MODE_REFUSED)) {
			WL_ERROR(("%s: req->flags: %d\n", __FUNCTION__, req->flags));
			continue;
		}

		/* mark all requests as refused if blocked from measurement */
		if (blocked) {
			req->flags |= DOT11_RMREP_MODE_REFUSED;
			WL_ERROR(("%s: blocked\n", __FUNCTION__));
			continue;
		}

		if (req->dur > dur_max)
			dur_max = req->dur;

		/* record the actual start time of the measurements */
		req->tsf_h = rrm_state->actual_start_h;
		req->tsf_l = rrm_state->actual_start_l;
		WL_INFORM(("type:%d\n", req->type));

		WL_RRM(("%s: req->type %d\n", __FUNCTION__, req->type));

		switch (req->type) {
#ifdef WL11K_BCN_MEAS
		case DOT11_RMREQ_BCN_ACTIVE:
		case DOT11_RMREQ_BCN_PASSIVE:
			wlc_rrm_begin_scan(rrm_info);
			break;
		case DOT11_RMREQ_BCN_TABLE:
			break;
#endif /* WL11K_BCN_MEAS */
#ifdef WL11K_ALL_MEAS
		case DOT11_MEASURE_TYPE_CHLOAD:
			WL_RRM(("--DOT11_MEASURE_TYPE_CHLOAD-- %s: req->type %d req->dur=%d \n",
				__FUNCTION__, req->type, req->dur));
			wlc_rrm_cca_start(rrm_info, req->dur);
			break;
		case DOT11_MEASURE_TYPE_NOISE:
			/* Adding a buffer time to dur_max to ensure that ipi
			 * timer expires before rrm timer
			 */
			if (dur_max < req->dur + WL_RRM_IPI_BUFF_TIME)
				dur_max = req->dur + WL_RRM_IPI_BUFF_TIME;
			wlc_rrm_noise_ipi_begin(rrm_info);
			break;
		case DOT11_MEASURE_TYPE_FRAME:
			break;
		case DOT11_MEASURE_TYPE_STAT:
			/* record the initial value */
			if (rrm_info->statreq->duration) {
				wlc_rrm_stat_get_group_data(rrm_info, 0);
			}
			break;
		case DOT11_MEASURE_TYPE_LCI:
			break;
		case DOT11_MEASURE_TYPE_CIVICLOC:
			break;
		case DOT11_MEASURE_TYPE_LOC_ID:
			break;
		case DOT11_MEASURE_TYPE_TXSTREAM:
			wlc_rrm_txstrm_get_data(rrm_info, 0);
			break;
		case DOT11_MEASURE_TYPE_PAUSE:
			dur_max = (int)req->dur * 10;
			break;
#endif /* WL11K_ALL_MEAS */
#ifdef WL_FTM_11K
#if defined(WL_PROXDETECT) && defined(WL_FTM)
		case DOT11_MEASURE_TYPE_FTMRANGE:
			if (rrm_info->direct_ranging_mode) {
				wlc_rrm_begin_ftm(rrm_info);
			}
			else {
				WL_INFORM(("wl%d: %s: direct ranging disabled "
					"waiting for app action; type %d in request token %d\n",
					wlc->pub->unit, __FUNCTION__, req->type, rrm_state->token));
			}
			break;
#endif /* WL_PROXDETECT && WL_FTM */
#endif /* WL_FTM_11K */
		default:
			WL_ERROR(("wl%d: %s: unknown measurement "
				"request type %d in request token %d\n",
				wlc->pub->unit, __FUNCTION__, req->type, rrm_state->token));
			break;
		}
		if ((req->flags & DOT11_RMREQ_MODE_PARALLEL) == 0)
			break;
	}

	/* Move to wait for end measurement */
	wlc_rrm_state_upd(rrm_info, WLC_RRM_WAIT_END_MEAS);
	WL_RRM(("%s in WLC_RRM_WAIT_END_MEAS  state upd to %d\n", __FUNCTION__, rrm_state->step));

	/* Set rrm_timer for measurement duration */
	if (dur_max) {
		/* For ZPL and video traffic - minimize pkt loss when switching channels */
		/* Keep measurement duration no more than 1sec if tx_suspend on operChannel */
		if ((rrm_done_chanswitch != 0) &&
		    (dur_max > WLC_RRM_MAX_MEAS_DUR)) {
			dur_max = WLC_RRM_MAX_MEAS_DUR;
		}
		/* Add to rrm_timer */
		dur_ms = dur_max * DOT11_TU_TO_US / 1000;
		WL_RRM(("%s: add to rrm_timer dur_ms=%dmsec dur_max=%dTU \n",
			__FUNCTION__, dur_ms, dur_max));
		wl_add_timer(wlc->wl, rrm_info->rrm_timer, dur_ms, 0);
	} else {
		WL_RRM(("%s: duration=0 call wlc_rrm_timer with rrm_info\n",
			__FUNCTION__));
		wlc_rrm_timer(rrm_info);
	}
	return;
}

void
wlc_rrm_pm_pending_complete(wlc_rrm_info_t *rrm_info)
{
	wlc_rrm_req_state_t *rrm_state = rrm_info->rrm_state;
	wlc_info_t *wlc = rrm_info->wlc;
	if (!rrm_state->ps_pending)
		return;

	rrm_state->ps_pending = FALSE;
	/* if the RM state machine is waiting for the PS announcement,
	 * then schedule the timer
	 */
	if (rrm_state->step == WLC_RRM_WAIT_PS_ANNOUNCE)
		wl_add_timer(wlc->wl, rrm_info->rrm_timer, 0, 0);
}

static void
wlc_rrm_timer(void *arg)
{
	wlc_rrm_info_t *rrm_info = (wlc_rrm_info_t *)arg;
	wlc_info_t *wlc = rrm_info->wlc;
	wlc_rrm_req_state_t *rrm_state = rrm_info->rrm_state;
	wlc_rrm_req_t *req;
	uint32 offset, intval;
#ifdef STA
	int idx;
	wlc_bsscfg_t *cfg;
#endif // endif
#if defined(WL11K_BCN_MEAS) && defined(WL11K_NBR_MEAS)
#ifdef WL11K_AP
	struct scb_iter scbiter;
	struct scb *scb;
	bool rrm_timer_set = FALSE;
#endif /* WL11K_AP */
#endif /* WL11K_BCN_MEAS && WL11K_NBR_MEAS */

	if (!wlc->pub->up)
		return;
	if (DEVICEREMOVED(wlc)) {
		WL_ERROR(("wl%d: %s: dead chip\n", wlc->pub->unit, __FUNCTION__));
		wl_down(wlc->wl);
		return;
	}
	WL_INFORM(("step:%d\n", rrm_state->step));

	switch (rrm_state->step) {
		case WLC_RRM_WAIT_STOP_TRAFFIC:
		/* announce PS mode to the AP if we are not already in PS mode */
#ifdef STA
		rrm_state->ps_pending = TRUE;
		FOREACH_AS_STA(wlc, idx, cfg) {
			/* block any PSPoll operations since we are just holding off AP traffic */
			mboolset(cfg->pm->PMblocked, WLC_PM_BLOCK_CHANSW);
			if (cfg->pm->PMenabled)
				continue;
			WL_ERROR(("wl%d.%d: wlc_rrm_timer: WAIT_STOP_TRAFFIC, "
				"entering PS mode for off channel measurement\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(cfg)));
			wlc_set_pmstate(cfg, TRUE);
		}
		/* We are supposed to wait for PM0->PM1 transition to finish but in case
		 * * we failed to send PM indications or failed to receive ACKs fake a PM0->PM1
		 * * transition so that anything depending on the transition to finish can move
		 * * forward i.e. scan engine can continue.
		*/
		wlc_pm_pending_complete(wlc);
		if (!wlc->PMpending)
			rrm_state->ps_pending = FALSE;
		wlc_set_wake_ctrl(wlc);

		/* suspend tx data fifos here for off channel measurements
		 * * if we are not announcing PS mode
		*/
		if (!rrm_state->ps_pending)
			wlc_tx_suspend(wlc);
#endif /* STA */
		/* calculate how long until this measurement should start */
		req = &rrm_state->req[rrm_state->cur_req];
		intval = req->intval * DOT11_TU_TO_US / 1000;
		if (intval)
			offset = R_REG(wlc->osh, D11_TSF_RANDOM(wlc)) % intval;
		else
			offset = 0;

		if (offset > 0) {
			wlc_rrm_state_upd(rrm_info, WLC_RRM_WAIT_BEGIN_MEAS);
			WL_ERROR(("%s: state upd to %d\n", __FUNCTION__, rrm_state->step));

			wl_add_timer(wlc->wl, rrm_info->rrm_timer, offset, 0);
			WL_INFORM(("wl%d: wlc_rrm_timer: WAIT_STOP_TRAFFIC, "
				"%d ms until measurement, waiting for measurement timer\n",
				wlc->pub->unit, offset));
		} else if (rrm_state->ps_pending) {
			wlc_rrm_state_upd(rrm_info, WLC_RRM_WAIT_PS_ANNOUNCE);
			WL_ERROR(("%s: state upd to %d\n", __FUNCTION__, rrm_state->step));
			/* wait for PS state to be communicated by
			 * waiting for the Null Data packet tx to
			 * complete, then start measurements
			 */
			WL_ERROR(("wl%d: wlc_rrm_timer: WAIT_STOP_TRAFFIC, "
				"%d ms until measurement, waiting for PS announcement\n",
				wlc->pub->unit, offset));
		} else {
			wlc_rrm_state_upd(rrm_info, WLC_RRM_WAIT_TX_SUSPEND);
			WL_ERROR(("%s: state upd to %d\n", __FUNCTION__, rrm_state->step));
			/* wait for the suspend interrupt to come back
			 * and start measurements
			 */
			WL_ERROR(("wl%d: wlc_rrm_timer: WAIT_STOP_TRAFFIC, "
				"%d ms until measurement, waiting for TX fifo suspend\n",
				wlc->pub->unit, offset));
		}
		break;
	case WLC_RRM_WAIT_PS_ANNOUNCE:
	case WLC_RRM_WAIT_TX_SUSPEND:
	case WLC_RRM_WAIT_BEGIN_MEAS:
		wlc_rrm_state_upd(rrm_info, WLC_RRM_WAIT_START_SET);
		/* Set step as WLC_RRM_WAIT_START_SET,
		 * fall through to case WLC_RRM_WAIT_START_SET.
		 */
	case WLC_RRM_WAIT_START_SET:
		WL_INFORM(("wl%d: wlc_rrm_timer: START_SET\n",
			wlc->pub->unit));
		wlc_rrm_begin(rrm_info);
		break;
	case WLC_RRM_WAIT_END_SCAN:
		WL_INFORM(("wl%d: wlc_rrm_timer: END_SCAN\n",
			wlc->pub->unit));
		break;
	case WLC_RRM_WAIT_END_FRAME:
		WL_INFORM(("wl%d: wlc_rrm_timer: END_FRAME\n",
			wlc->pub->unit));
		break;
	case WLC_RRM_WAIT_END_MEAS:
		WL_INFORM(("wl%d: wlc_rrm_timer: END_MEASUREMENTS\n",
			wlc->pub->unit));
		wlc_rrm_meas_end(wlc);
		break;
	case WLC_RRM_WAIT_END_CCA:
		WL_INFORM(("wl%d: wlc_rrm_timer: END_CCA\n",
			wlc->pub->unit));
		break;
	case WLC_RRM_ABORT:
		wlc_rrm_state_upd(rrm_info, WLC_RRM_IDLE);
		WL_INFORM(("%s, state upd to %d\n", __FUNCTION__, rrm_state->step));
		break;
	case WLC_RRM_IDLE:
#if defined(WL11K_BCN_MEAS) && defined(WL11K_NBR_MEAS)
#ifdef WL11K_AP
		/* use a timer to send periodic beacon requests 11k enabled STAs */
		FOREACH_UP_AP(wlc, idx, cfg)
		{
			rrm_bsscfg_cubby_t *rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, cfg);
			FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {
				if (SCB_ASSOCIATED(scb)) {
					rrm_scb_info_t *scb_info;
					bcn_req_t bcnreq;
					uint32 current_time;

					memset(&bcnreq, 0, sizeof(bcn_req_t));
					scb_info = RRM_SCB_INFO(rrm_info, scb);
					if (!scb_info) {
						continue;
					}

					memcpy(&bcnreq.da, &scb->ea, ETHER_ADDR_LEN);
					memcpy(&bcnreq.ssid.SSID, &scb->bsscfg->SSID,
						scb->bsscfg->SSID_len);
					bcnreq.ssid.SSID_len = scb->bsscfg->SSID_len;
					bcnreq.bcn_mode = DOT11_RMREQ_BCN_PASSIVE;
					bcnreq.channel = 0;	 /* all channels */
					bcnreq.dur = WLC_RRM_BCNREQ_INTERVAL_DUR;
					bcnreq.random_int = 0;
					bcnreq.reps = 1;
					bcnreq.req_elements = 1;
					current_time = OSL_SYSUPTIME();

					if (isset(scb_info->rrm_capabilities,
						DOT11_RRM_CAP_BCN_PASSIVE) &&
						rrm_cfg->bcn_req_timer != 0) {
						if (((current_time - scb_info->bcnreq_time >=
							rrm_cfg->bcn_req_timer) ||
							(scb_info->bcnreq_time == 0))) {
							scb_info->bcnreq_time = current_time;
							wlc_rrm_flush_bcnnbrs(rrm_info, scb);
							wlc_rrm_send_bcnreq(rrm_info, cfg, &bcnreq);
						}
						if (!rrm_timer_set) {
							rrm_timer_set = TRUE;
							rrm_cfg->rrm_timer_set = FALSE;
							wlc_rrm_add_timer(wlc, scb);
						}
					}
				}
			} /* for each bss scb */
			rrm_timer_set = FALSE;
		}
#endif /* WL11K_AP */
#endif /* WL11K_BCN_MEAS && WL11K_NBR_MEAS */
		WL_INFORM(("wl%d: %s: error, in timer with state WLC_RRM_IDLE\n",
			wlc->pub->unit, __FUNCTION__));
		break;
	default:
		WL_ERROR(("wl%d: %s: Invalid rrm state:%d\n", WLCWLUNIT(wlc), __FUNCTION__,
			rrm_state->step));
		break;
	}
	/* update PS control */
	wlc_set_wake_ctrl(wlc);
}

static void
wlc_rrm_validate(wlc_rrm_info_t *rrm_info)
{
	wlc_rrm_req_state_t *rrm_state = rrm_info->rrm_state;
	chanspec_t rrm_chanspec;
	wlc_rrm_req_t *req;
	int i;
	wlc_info_t *wlc = rrm_info->wlc;
	DBGONLY(char chanbuf[CHANSPEC_STR_LEN]; )

	rrm_chanspec = 0;

	for (i = 0; i < rrm_state->req_count; i++) {
		req = &rrm_state->req[i];
#ifdef WL11K_ALL_MEAS
		if (req->type == DOT11_MEASURE_TYPE_PAUSE) {
			if (rrm_state->req_count == 1) {
				req->flags |= DOT11_RMREP_MODE_REFUSED;
				WL_INFORM(("%s: DOT11_RMREP_MODE_REFUSED, "
					"pause cannot be the only request\n", __FUNCTION__));
				continue;
			}
			if ((i == rrm_state->req_count - 1) && (rrm_state->reps == 0)) {
				req->flags |= DOT11_RMREP_MODE_REFUSED;
				WL_INFORM(("%s: DOT11_RMREP_MODE_REFUSED, "
					"pause cannot be last request if no repetitions\n",
					__FUNCTION__));
				continue;
			}
			if (req->flags & DOT11_RMREQ_MODE_PARALLEL) {
				req->flags |= DOT11_RMREP_MODE_INCAPABLE;
				WL_INFORM(("%s: DOT11_RMREP_MODE_INCAPABLE, "
					"pause cannot be parallel\n", __FUNCTION__));
				continue;
			}
			if (i && rrm_state->req[i-1].flags & DOT11_RMREQ_MODE_PARALLEL) {
				rrm_state->req[i-1].flags |= DOT11_RMREP_MODE_INCAPABLE;
				WL_INFORM(("%s: DOT11_RMREP_MODE_INCAPABLE, "
					"pause cannot be parallel\n", __FUNCTION__));
				continue;
			}
		}
#endif /* WL11K_ALL_MEAS */

		if (!(req->flags & DOT11_RMREQ_MODE_PARALLEL)) {
			/* new set of parallel measurements */
			rrm_chanspec = 0;
		}

		/* check for an unsupported channel */
		if (CHSPEC_CHANNEL(req->chanspec) != 0 &&
			CHSPEC_CHANNEL(req->chanspec) != 255 &&
			!wlc_valid_chanspec_db(wlc->cmi, req->chanspec)) {
			if (req->type == DOT11_RMREQ_BCN_TABLE)
				WL_ERROR(("%s: DOT11_RMREP_MODE_INCAPABLE: invalid channel\n",
					__FUNCTION__));
			req->flags |= DOT11_RMREP_MODE_INCAPABLE;
			continue;
		}

		/* refuse zero dur measurements */
		if (req->type == DOT11_RMREQ_BCN_TABLE ||
			req->type == DOT11_MEASURE_TYPE_STAT ||
			req->type == DOT11_MEASURE_TYPE_LCI ||
			req->type == DOT11_MEASURE_TYPE_LOC_ID ||
			req->type == DOT11_MEASURE_TYPE_CIVICLOC) {
		}
		else {
			uint32 activity_delta;
			if (req->dur == 0) {
				req->flags |= DOT11_RMREP_MODE_REFUSED;
			WL_RRM(("%s: DOT11_RMREP_MODE_REFUSED, due to dur=0 type: %d\n",
				__FUNCTION__, req->type));
				continue;
			}
			/* If data activity in past bcn_req_traff_meas_prd ms, refuse */
			activity_delta = OSL_SYSUPTIME() - rrm_info->data_activity_ts;
			if (activity_delta < rrm_info->bcn_req_traff_meas_prd) {
				WL_ERROR(("%s: Req refused due to data activity in last %d ms\n",
					__FUNCTION__, activity_delta));
				req->flags |= DOT11_RMREP_MODE_REFUSED;
				continue;
			}
		}

		/* pick up the channel for this parallel set */
		if (rrm_chanspec == 0)
			rrm_chanspec = req->chanspec;

		/* check for parallel measurements on different channels */
		if (req->chanspec && req->chanspec != rrm_chanspec) {
			WL_INFORM(("wl%d: wlc_rrm_validate: refusing parallel "
				   "measurement with different channel than "
				   "others, RM type %d token %d chanspec %s\n",
				   wlc->pub->unit, req->type, req->token,
				   wf_chspec_ntoa_ex(req->chanspec, chanbuf)));
			req->flags |= DOT11_RMREP_MODE_REFUSED;
			continue;
		}
	}
}

static void
wlc_rrm_end(wlc_rrm_info_t *rrm_info)
{
	/* clean up state */
	wlc_rrm_free(rrm_info);
	if (rrm_info->rrm_state->cb) {
		cb_fn_t cb = (cb_fn_t)(rrm_info->rrm_state->cb);
		void *cb_arg = rrm_info->rrm_state->cb_arg;

		(cb)(cb_arg);
		rrm_info->rrm_state->cb = NULL;
		rrm_info->rrm_state->cb_arg = NULL;
	}
}

/* start the radio measurement state machine working on
 * the next set of parallel measurements.
 */
static void
wlc_rrm_next_set(wlc_rrm_info_t *rrm_info)
{
	uint32 offset = 0, intval = 0;
	chanspec_t chanspec;
	wlc_rrm_req_state_t *rrm_state = rrm_info->rrm_state;
	wlc_info_t *wlc = rrm_info->wlc;
	wlc_rrm_req_t *req = &rrm_state->req[rrm_state->cur_req];

	WL_RRM(("%s <Enter>: rrm_state->cur_req=%d, rrm_state->req_count=%d\n",
		__FUNCTION__, rrm_state->cur_req, rrm_state->req_count));

	if (rrm_state->cur_req >= rrm_state->req_count) {
		if (rrm_state->reps == 0) {
			/* signal that all requests are done */
			wlc_rrm_end(rrm_info);
			WL_INFORM(("%s: all req are done\n", __FUNCTION__));
			return;
		} else {
			rrm_state->reps--;
			rrm_state->cur_req = 0;
		}
	}

	/* Start a timer for off-channel prep if measurements are off-channel,
	 * or for the measurements if on-channel
	 */
	chanspec = wlc_rrm_chanspec(rrm_info);
	if (wlc->pub->associated &&
		(chanspec != 0) &&
		(wf_chspec_ctlchan(chanspec) != wf_chspec_ctlchan(wlc->home_chanspec))) {
		if (rrm_state->chanspec_return != 0)
			offset = WLC_RRM_HOME_TIME;
		/* off-channel measurements, set a timer to prepare for the channel switch */
		wlc_rrm_state_upd(rrm_info, WLC_RRM_WAIT_STOP_TRAFFIC);
		WL_INFORM(("%s: state upd to %d\n", __FUNCTION__, rrm_state->step));
		if (req->intval < WLC_RRM_MIN_MEAS_INTVL) {
			WL_INFORM((" %s changing meas-intvl from 0x%x to 0x%x TUs ",
				__FUNCTION__, req->intval, WLC_RRM_MIN_MEAS_INTVL));
			req->intval = WLC_RRM_MIN_MEAS_INTVL;
		}
	} else {
		if (req->intval > 0) {
			/* calculate a random delay(ms) distributed uniformly in the range 0
			 * to the random internval
			 */
			intval = req->intval * DOT11_TU_TO_US / 1000;
			offset = R_REG(wlc->osh, D11_TSF_RANDOM(wlc))
					% intval;
		}
		/* either unassociated, so no channel prep even if we do change channels,
		 * or channel-less measurement set (all suppressed or internal state report),
		 * or associated and measurements on home channel,
		 * set the timer for WLC_RRM_WAIT_START_SET
		 */

		wlc_rrm_state_upd(rrm_info, WLC_RRM_WAIT_START_SET);
		WL_INFORM(("%s: state upd to %d, offset: %d\n",
			__FUNCTION__, rrm_state->step, offset));
	}

	WL_RRM(("%s: offset=%dms intval=%dms\n", __FUNCTION__, offset, intval));

	if (offset > 0) {
		WL_RRM(("RRM timer %s: offset=%d call wl_add_timer rrm_info->rrm_timer ...\n",
			__FUNCTION__, offset));
		wl_add_timer(wlc->wl, rrm_info->rrm_timer, offset, 0);
	}
	else {
		WL_RRM(("RRM timer %s: offset=0 call wlc_rrm_timer with rrm_info ...\n",
			__FUNCTION__));
		wlc_rrm_timer(rrm_info);
	}
	return;
}

static void
wlc_rrm_start(wlc_info_t *wlc)
{
	wlc_rrm_info_t *rrm_info = wlc->rrm_info;
	wlc_rrm_req_state_t *rrm_state = rrm_info->rrm_state;
#ifdef BCMDBG
	char chanbuf[CHANSPEC_STR_LEN];
	wlc_rrm_req_t *req;
	const char *name;
	int i;
#endif /* BCMDBG */

	rrm_state->cur_req = 0;
	wlc_rrm_state_upd(rrm_info, WLC_RRM_IDLE);
	WL_INFORM(("%s: state upd to %d\n", __FUNCTION__, rrm_state->step));

#ifdef BCMDBG
	WL_INFORM(("wl%d: wlc_rrm_start(): %d RM Requests, token 0x%x (%d)\n",
		rrm_info->wlc->pub->unit, rrm_state->req_count,
		rrm_state->token, rrm_state->token));

	for (i = 0; i < rrm_state->req_count; i++) {
		req = &rrm_state->req[i];
		switch (req->type) {

		case DOT11_RMREQ_BCN_ACTIVE:
			name = "Active Scan";
			break;
		case DOT11_RMREQ_BCN_PASSIVE:
			name = "Passive Scan";
			break;
		case DOT11_RMREQ_BCN_TABLE:
			name = "Beacon Table";
			break;
		case DOT11_MEASURE_TYPE_CHLOAD:
			name = "Channel Load";
			break;
		case DOT11_MEASURE_TYPE_NOISE:
			name = "Noise";
			break;
		case DOT11_MEASURE_TYPE_BEACON:
			name = "Beacon";
			break;
		case DOT11_MEASURE_TYPE_FRAME:
			name = "Frame";
			break;
		case DOT11_MEASURE_TYPE_STAT:
			name = "STA statistics";
			break;
		case DOT11_MEASURE_TYPE_LCI:
			name = "Location";
			break;
		case DOT11_MEASURE_TYPE_CIVICLOC:
			name = "Civic location";
			break;
		case DOT11_MEASURE_TYPE_LOC_ID:
			name = "Location identifier";
			break;
		case DOT11_MEASURE_TYPE_TXSTREAM:
			name = "TX stream";
			break;
		case DOT11_MEASURE_TYPE_PAUSE:
			name = "Pause";
			break;
		case DOT11_MEASURE_TYPE_FTMRANGE:
			name = "FTM Range";
			break;
		default:
			name = "";
			WL_ERROR(("wl%d: %s: Unsupported RRM Req/Measure type:%d\n", WLCWLUNIT(wlc),
				__FUNCTION__, req->type));
			break;
		}

		WL_ERROR(("RM REQ token 0x%02x (%2d), type %2d: %s, chanspec %s, "
			"tsf 0x%x:%08x dur %4d TUs\n",
			req->token, req->token, req->type, name,
			wf_chspec_ntoa_ex(req->chanspec, chanbuf), req->tsf_h, req->tsf_l,
			req->dur));
	}
#endif /* BCMDBG */
	wlc_rrm_validate(rrm_info);

	wlc_rrm_next_set(rrm_info);
}

static void
wlc_rrm_state_upd(wlc_rrm_info_t *rrm_info, uint state)
{
	bool was_in_progress;
	wlc_rrm_req_state_t *rrm_state = rrm_info->rrm_state;
	wlc_info_t *wlc = rrm_info->wlc;

	WL_INFORM(("state:%d\n", state));

	WL_RRM(("%s <Enter>: rrm_state->step=%d, state=%d\n",
		__FUNCTION__, rrm_state->step, state));

	/* update counter */
	wlc_rrm_framereq_measure(rrm_info);

	if (rrm_state->step == state)
		return;

	WL_INFORM(("wlc_rrm_state_upd; change from %d to %d\n", rrm_state->step, state));
	was_in_progress = WLC_RRM_IN_PROGRESS(wlc);
	rrm_state->step = state;
	if (WLC_RRM_IN_PROGRESS(wlc) != was_in_progress)
		wlc_phy_hold_upd(WLC_PI(wlc), PHY_HOLD_FOR_RM, !was_in_progress);

	return;
}

#ifdef WL11K_ALL_MEAS
#ifdef WL11K_AP
/* AP Channel Report */
static uint
wlc_rrm_calc_ap_ch_rep_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	wlc_rrm_info_t *rrm = (wlc_rrm_info_t *)ctx;
	wlc_info_t *wlc = rrm->wlc;
	wlc_bsscfg_t *cfg = data->cfg;
	uint len = 0;

	if (WL11K_ENAB(wlc->pub) && BSSCFG_AP(cfg) && wlc_rrm_enabled(rrm, cfg))
		len = wlc_rrm_ap_chrep_len(rrm, cfg);

	return len;
}

static int
wlc_rrm_write_ap_ch_rep_ie(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_rrm_info_t *rrm = (wlc_rrm_info_t *)ctx;
	wlc_info_t *wlc = rrm->wlc;
	wlc_bsscfg_t *cfg = data->cfg;

	if (WL11K_ENAB(wlc->pub) && BSSCFG_AP(cfg) && wlc_rrm_enabled(rrm, cfg))
		wlc_rrm_add_ap_chrep(rrm, cfg, data->buf);

	return BCME_OK;
}

/* Measurement Pilot Transmission */
static uint
wlc_rrm_calc_mptx_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	wlc_rrm_info_t *rrm = (wlc_rrm_info_t *)ctx;
	wlc_info_t *wlc = rrm->wlc;
	wlc_bsscfg_t *cfg = data->cfg;
	rrm_bsscfg_cubby_t *rrm_cfg;

	rrm_cfg = RRM_BSSCFG_CUBBY(rrm, cfg);
	ASSERT(rrm_cfg != NULL);

	if (WL11K_ENAB(wlc->pub) && BSSCFG_AP(data->cfg) &&
		isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_MPTI))
		return TLV_HDR_LEN + 1;

	return 0;
}

static int
wlc_rrm_write_mptx_ie(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_rrm_info_t *rrm = (wlc_rrm_info_t *)ctx;
	wlc_info_t *wlc = rrm->wlc;
	wlc_bsscfg_t *cfg = data->cfg;
	rrm_bsscfg_cubby_t *rrm_cfg;

	rrm_cfg = RRM_BSSCFG_CUBBY(rrm, cfg);
	ASSERT(rrm_cfg != NULL);

	if (WL11K_ENAB(wlc->pub) && BSSCFG_AP(data->cfg) &&
		isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_MPTI)) {
		int8 pilot_tx[1];

		/* Measurement Pilot Transmission Information */
		pilot_tx[0] = rrm_cfg->mp_period; /* pilot tx interval */

		bcm_write_tlv(DOT11_MNG_MEASUREMENT_PILOT_TX_ID, pilot_tx, 1, data->buf);
	}

	return BCME_OK;
}

static int
wlc_assoc_parse_rrm_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;
	wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;
	wlc_bsscfg_t *cfg = data->cfg;
	wlc_rrm_info_t *rrm_info = wlc->rrm_info;
	bcm_tlv_t *rrmie = (bcm_tlv_t *)data->ie;
	struct scb *scb = ftpparm->assocreq.scb;
	rrm_scb_info_t *rrm_scb = RRM_SCB_INFO(rrm_info, scb);

	if (WL11K_ENAB(wlc->pub) && BSSCFG_AP(cfg) && wlc_rrm_enabled(rrm_info, cfg)) {

		if (rrmie == NULL)
			return BCME_OK;

		if ((rrmie->len == DOT11_RRM_CAP_LEN) &&
			(rrm_scb != NULL)) {
			bcopy(rrmie->data, rrm_scb->rrm_capabilities, DOT11_RRM_CAP_LEN);
			scb->flags3 &= ~SCB3_RRM_BCN_PASSIVE;
			if (isset(rrm_scb->rrm_capabilities, DOT11_RRM_CAP_BCN_PASSIVE))
				scb->flags3 |= SCB3_RRM_BCN_PASSIVE;
		}
	}

	return BCME_OK;
}
#endif /* WL11K_AP */
#endif /* WL11K_ALL_MEAS */

#ifdef WL11K_AP
static void
wlc_pilot_timer(void *arg)
{
#ifdef WL11K_ALL_MEAS
	wlc_bsscfg_t *cfg = (wlc_bsscfg_t *)arg;
	wlc_info_t *wlc = cfg->wlc;
	struct dot11_action_frmhdr *action_hdr;
	dot11_mprep_t *mp_rep;
	void *p = NULL;
	uint8 *pbody;
	unsigned int len;
	const char *country_str;
	rrm_bsscfg_cubby_t *rrm_cfg;

	if (wlc_quiet_chanspec(wlc->cmi, wlc->chanspec))
		return;

	rrm_cfg = RRM_BSSCFG_CUBBY(wlc->rrm_info, cfg);
	ASSERT(rrm_cfg != NULL);

	len = DOT11_ACTION_FRMHDR_LEN + DOT11_MPREP_LEN;
	p = wlc_frame_get_action(wlc, &ether_bcast, &cfg->cur_etheraddr,
		&cfg->BSSID, len, &pbody, DOT11_ACTION_CAT_PUBLIC);

	if (p == NULL) {
		WL_ERROR(("%s: failed to get mgmt frame\n", __FUNCTION__));
		return;
	}
	action_hdr = (struct dot11_action_frmhdr *)pbody;
	action_hdr->category = DOT11_ACTION_CAT_PUBLIC;
	action_hdr->action = DOT11_PUB_ACTION_MP;
	mp_rep = (dot11_mprep_t *)&action_hdr->data[0];
	memset(mp_rep, 0, DOT11_MPREP_LEN);

	if (BSS_WL11H_ENAB(wlc, cfg))
		mp_rep->cap_info |= DOT11_MP_CAP_SPECTRUM;
	if (wlc->band->gmode && wlc->shortslot)
		mp_rep->cap_info |= DOT11_MP_CAP_SHORTSLOT;

	country_str = wlc_get_country_string(wlc->cntry);
	mp_rep->country[0] = country_str[0];
	mp_rep->country[1] = country_str[1];

	mp_rep->opclass = wlc_get_regclass(wlc->cmi, cfg->current_bss->chanspec);
	mp_rep->channel = CHSPEC_CHANNEL(cfg->current_bss->chanspec);
	mp_rep->mp_interval = rrm_cfg->mp_period;

	if (p != NULL) {
		wlc_sendmgmt(wlc, p, cfg->wlcif->qi, NULL);
	}
#endif /* WL11K_ALL_MEAS */
}

int
wlc_rrm_init_pilot_timer(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg)
{
	rrm_bsscfg_cubby_t *rrm_cfg;
	wlc_info_t *wlc = rrm_info->wlc;

	rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, cfg);
	ASSERT(rrm_cfg != NULL);

	rrm_cfg->pilot_timer = wl_init_timer(wlc->wl, wlc_pilot_timer, cfg, "pilot");
	if (!rrm_cfg->pilot_timer) {
		WL_RRM(("pilot_timer init failed\n"));
		return BCME_NOMEM;
	}
	return BCME_OK;
}

void
wlc_rrm_add_pilot_timer(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
#ifdef WL11K_ALL_MEAS
	rrm_bsscfg_cubby_t *rrm_cfg;
	uint32 mp_ms = 0;
	uint16 beacon_period;
	uint32 mp_percent;
	uint8 mp_active;

	if (!WL11K_ENAB(wlc->pub))
		return;

	beacon_period = cfg->current_bss->beacon_period;

	rrm_cfg = RRM_BSSCFG_CUBBY(wlc->rrm_info, cfg);
	ASSERT(rrm_cfg != NULL);

	if (!isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_MPTI))
		return;
	mp_active = rrm_cfg->rrm_cap[3] & DOT11_RRM_CAP_MPA_MASK;
	switch (mp_active) {
		case 2:
			mp_percent = 4;
			break;
		case 3:
			mp_percent = 7;
			break;
		case 4:
			mp_percent = 12;
			break;
		case 5:
			mp_percent = 17;
			break;
		case 6:
			mp_percent = 22;
			break;
		case 7:
			mp_percent = 37;
			break;
		default:
			return;

	}
	rrm_cfg->mp_period = beacon_period * mp_percent / 100;
	mp_ms = rrm_cfg->mp_period * DOT11_TU_TO_US / 1000;

	WL_RRM(("%s: MP Interval = %dms\n", __FUNCTION__, mp_ms));
	wl_add_timer(wlc->wl, rrm_cfg->pilot_timer, mp_ms, 1);
#endif /* WL11K_ALL_MEAS */
}

void
wlc_rrm_del_pilot_timer(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	rrm_bsscfg_cubby_t *rrm_cfg;

	if (!WL11K_ENAB(wlc->pub))
		return;

	rrm_cfg = RRM_BSSCFG_CUBBY(wlc->rrm_info, cfg);
	ASSERT(rrm_cfg != NULL);

	if (rrm_cfg->pilot_timer) {
		wl_del_timer(wlc->wl, rrm_cfg->pilot_timer);
	}
}

void
wlc_rrm_free_pilot_timer(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg)
{
	rrm_bsscfg_cubby_t *rrm_cfg;
	wlc_info_t *wlc = rrm_info->wlc;

	rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, cfg);
	ASSERT(rrm_cfg != NULL);

	if (rrm_cfg->pilot_timer) {
		wl_free_timer(wlc->wl, rrm_cfg->pilot_timer);
	}
}

void
wlc_rrm_get_sta_cap(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct scb *scb, uint8 *rrm_cap)
{
	wlc_rrm_info_t *rrm_info = wlc->rrm_info;
	rrm_scb_info_t *rrm_scb = RRM_SCB_INFO(rrm_info, scb);

	if (WL11K_ENAB(wlc->pub) && BSSCFG_AP(cfg)) {
		memcpy(rrm_cap, rrm_scb->rrm_capabilities, DOT11_RRM_CAP_LEN);
	}
}
#endif /* WL11K_AP */

#ifdef WL11K_ALL_MEAS
/* RRM Cap */
static uint
wlc_rrm_calc_cap_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	wlc_rrm_info_t *rrm = (wlc_rrm_info_t *)ctx;
	wlc_info_t *wlc = rrm->wlc;
	wlc_bsscfg_t *cfg = data->cfg;

	if (WL11K_ENAB(wlc->pub) && wlc_rrm_enabled(rrm, cfg))
		return TLV_HDR_LEN + DOT11_RRM_CAP_LEN;

	return 0;
}

static int
wlc_rrm_write_cap_ie(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_rrm_info_t *rrm = (wlc_rrm_info_t *)ctx;
	wlc_info_t *wlc = rrm->wlc;
	wlc_bsscfg_t *cfg = data->cfg;
	rrm_bsscfg_cubby_t *rrm_cfg;

	rrm_cfg = RRM_BSSCFG_CUBBY(rrm, cfg);
	ASSERT(rrm_cfg != NULL);

	if (WL11K_ENAB(wlc->pub) && wlc_rrm_enabled(rrm, cfg)) {
		/* RM Enabled Capabilities */
		bcm_write_tlv(DOT11_MNG_RRM_CAP_ID, rrm_cfg->rrm_cap, DOT11_RRM_CAP_LEN,
			data->buf);
	}

	return BCME_OK;
}

#ifdef STA
/* return TRUE if wlc_bss_info_t contains RRM IE, else FALSE */
static bool
wlc_rrm_is_rrm_ie(wlc_bss_info_t *bi)
{
	uint bcn_parse_len = bi->bcn_prb_len - sizeof(struct dot11_bcn_prb);
	uint8 *bcn_parse = (uint8*)bi->bcn_prb + sizeof(struct dot11_bcn_prb);

	if (bcm_parse_tlvs(bcn_parse, bcn_parse_len, DOT11_MNG_RRM_CAP_ID))
		return TRUE;

	return FALSE;
}

/* RRM Cap */
static uint
wlc_rrm_calc_assoc_cap_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	/* include RRM cap only if target AP supports RRM cap */
	if (wlc_rrm_is_rrm_ie(wlc_iem_calc_get_assocreq_target(data)))
		return wlc_rrm_calc_cap_ie_len(ctx, data);

	return 0;
}

static int
wlc_rrm_write_assoc_cap_ie(void *ctx, wlc_iem_build_data_t *data)
{
	/* include RRM cap only if target AP supports RRM cap */
	if (wlc_rrm_is_rrm_ie(wlc_iem_build_get_assocreq_target(data)))
		return wlc_rrm_write_cap_ie(ctx, data);

	return BCME_OK;
}

static int
wlc_rrm_parse_rrmcap_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_rrm_info_t *rrm = (wlc_rrm_info_t *)ctx;
	wlc_info_t *wlc = rrm->wlc;
	wlc_bsscfg_t *cfg = data->cfg;
	wlc_assoc_t *as = cfg->assoc;
	wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;
	struct scb *scb = ftpparm->assocresp.scb;

	if (WL11K_ENAB(wlc->pub) && wlc_rrm_enabled(wlc->rrm_info, cfg)) {
		/* update the AP's scb for RRM if assoc response has RRM Cap bit set and
		 * RRM cap IE present.
		 */
		scb->flags3 &= ~SCB3_RRM;
		if ((ltoh16(as->resp->capability) & DOT11_CAP_RRM) &&
			(data->ie != NULL) && (data->ie_len > DOT11_RRM_CAP_LEN)) {
			scb->flags3 |= SCB3_RRM;
		}
	}
	return BCME_OK;
}

#endif /* STA */
#endif /* WL11K_ALL_MEAS */

void
wlc_rrm_stop(wlc_info_t *wlc)
{
	WL_ERROR(("%s: abort rrm processing\n", __FUNCTION__));
	/* just abort for now, eventually, send reports then stop */
	wlc_rrm_abort(wlc);
}

bool
wlc_rrm_inprog(wlc_info_t *wlc)
{
	return WLC_RRM_IN_PROGRESS(wlc);
}

#ifdef WL_FTM_11K
bool
wlc_rrm_ftm_inprog(wlc_info_t *wlc)
{
	return (WLC_RRM_IN_PROGRESS(wlc) &&
			wlc->rrm_info->rrm_state->scan_active &&
			wlc->rrm_info->rrm_frng_req->frng_req);
}
#endif /* WL_FTM_11K */

bool
wlc_rrm_wait_tx_suspend(wlc_info_t *wlc)
{
	return (wlc->rrm_info->rrm_state->step == WLC_RRM_WAIT_TX_SUSPEND);
}

void
wlc_rrm_start_timer(wlc_info_t *wlc)
{
	wl_add_timer(wlc->wl, wlc->rrm_info->rrm_timer, 0, 0);
}

bool
wlc_rrm_enabled(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg)
{
	rrm_bsscfg_cubby_t *rrm_cfg;
	int i;

	rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, cfg);
	ASSERT(rrm_cfg != NULL);

	for (i = 0; i < DOT11_RRM_CAP_LEN; i++) {
		if (rrm_cfg->rrm_cap[i] != 0)
			return TRUE;
	}
#ifdef WL11K_ALL_MEAS
	if (isset(cfg->ext_cap, DOT11_EXT_CAP_CIVIC_LOC) ||
		isset(cfg->ext_cap, DOT11_EXT_CAP_IDENT_LOC))
		return TRUE;
#endif // endif

	return FALSE;
}

bool
wlc_rrm_stats_enabled(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg)
{
	rrm_bsscfg_cubby_t *rrm_cfg;

	rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, cfg);
	if (!rrm_cfg)
		return FALSE;

	if (isset(rrm_cfg->rrm_cap, DOT11_RRM_CAP_SM))
		return TRUE;
	return FALSE;
}

static int
wlc_rrm_bsscfg_init(void *context, wlc_bsscfg_t *bsscfg)
{
	wlc_rrm_info_t *rrm_info = (wlc_rrm_info_t *)context;
	wlc_info_t *wlc = rrm_info->wlc;
	rrm_bsscfg_cubby_t *rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, bsscfg);
	int err = BCME_OK;

	rrm_cfg->wlc = wlc;
	rrm_cfg->rrm_info = rrm_info;
	rrm_cfg->rrm_timer_set = FALSE;

	/* Configure RRM enabled capability */
	bzero(rrm_cfg->rrm_cap, DOT11_RRM_CAP_LEN);

	/* disable all rrm_cap by default, can use wl rrm to enable, e.g. wl rrm 0x10f33 */

	wlc->pub->_rrm = TRUE;

#if defined(WL11K_AP)
	if ((err = wlc_rrm_init_pilot_timer(rrm_info, bsscfg)) != BCME_OK) {
		WL_ERROR(("pilot_timer init failed\n"));
	}
#endif // endif
	return err;
}

static void
wlc_rrm_bsscfg_deinit(void *context, wlc_bsscfg_t *bsscfg)
{
	wlc_rrm_info_t *rrm_info = (wlc_rrm_info_t *)context;
	rrm_bsscfg_cubby_t *rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, bsscfg);

	if (rrm_cfg == NULL)
		return;

#ifdef WL11K_NBR_MEAS
	/* Free nbr_rep_head and nbr_cnt_head */
	wlc_rrm_flush_neighbors(rrm_info, bsscfg, NBR_ADD_STATIC);
	wlc_rrm_flush_neighbors(rrm_info, bsscfg, NBR_ADD_DYNAMIC);
#endif // endif

#ifdef WL11K_ALL_MEAS
	wlc_rrm_free_self_report(rrm_info, bsscfg);
#endif // endif
#ifdef WL11K_AP
	wlc_rrm_free_pilot_timer(rrm_info, bsscfg);
#endif // endif

}

#ifdef WLSCANCACHE
void
wlc_rrm_update_cap(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *bsscfg)
{
	rrm_bsscfg_cubby_t *rrm_cfg;

	ASSERT(bsscfg != NULL);
	rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, bsscfg);

	if (SCANCACHE_ENAB(rrm_info->wlc->scan))
		setbit(rrm_cfg->rrm_cap, DOT11_RRM_CAP_BCN_TABLE);
	else
		clrbit(rrm_cfg->rrm_cap, DOT11_RRM_CAP_BCN_TABLE);
}
#endif /* WLSCANCACHE */

#ifdef BCMDBG
static void
wlc_rrm_bsscfg_dump(void *context, wlc_bsscfg_t *bsscfg, struct bcmstrbuf *b)
{
	wlc_rrm_info_t *rrm_info = (wlc_rrm_info_t *)context;

	WL_ERROR(("%s: rrm_info->cfgh: %d\n", __FUNCTION__, rrm_info->cfgh));
}
#endif // endif

bool wlc_rrm_in_progress(wlc_info_t *wlc)
{
	return WLC_RRM_IN_PROGRESS(wlc);
}

static uint
wlc_rrm_scb_secsz(void *ctx, struct scb *scb)
{
	wlc_rrm_info_t *rrm_info = (wlc_rrm_info_t *)ctx;
	uint size = 0;

	BCM_REFERENCE(rrm_info);
	if (scb && !SCB_INTERNAL(scb)) {
		size = ALIGN_SIZE(sizeof(rrm_scb_info_t), sizeof(uint32));
#ifdef WL11K_ALL_MEAS
#ifdef WL11K_AP
		/* don't allocate 802.11K stats extra memory to avoid memory consuming */
		if (isset(((rrm_bsscfg_cubby_t *)RRM_BSSCFG_CUBBY(rrm_info, scb->bsscfg))->rrm_cap,
			DOT11_RRM_CAP_SM)) {
			size += ALIGN_SIZE(sizeof(scb_rrm_stats_t), sizeof(uint32));
		}
#endif // endif
		if (isset(((rrm_bsscfg_cubby_t *)RRM_BSSCFG_CUBBY(rrm_info, scb->bsscfg))->rrm_cap,
			DOT11_RRM_CAP_TSCM)) {
			size += ALIGN_SIZE(sizeof(scb_tscm_t), sizeof(uint32));
		}
#endif /* WL11K_ALL_MEAS */
	}
	return size;
}

/* scb cubby_init function */
static int wlc_rrm_scb_init(void *ctx, struct scb *scb)
{
	wlc_rrm_info_t *rrm_info = (wlc_rrm_info_t *)ctx;
	wlc_info_t *wlc = rrm_info->wlc;

	rrm_scb_cubby_t *scb_cubby = RRM_SCB_CUBBY(rrm_info, scb);
	uint8 *secptr;
#ifdef WL11K_ALL_MEAS
	scb_rrm_stats_t *scb_stats = NULL;
	scb_tscm_t *scb_tscm = NULL;
#endif // endif

	secptr = wlc_scb_sec_cubby_alloc(wlc, scb, wlc_rrm_scb_secsz(ctx, scb));
	scb_cubby->scb_info = (rrm_scb_info_t*) secptr;

	if (secptr != NULL) {
		secptr += ALIGN_SIZE(sizeof(rrm_scb_info_t), sizeof(uint32));
#ifdef WL11K_ALL_MEAS
#ifdef WL11K_AP
		/* don't allocate 802.11K stats extra memory to avoid memory consuming */
		if (isset(((rrm_bsscfg_cubby_t *)RRM_BSSCFG_CUBBY(rrm_info, scb->bsscfg))->rrm_cap,
			DOT11_RRM_CAP_SM)) {
			scb_stats = (scb_rrm_stats_t*) secptr;
			secptr += ALIGN_SIZE(sizeof(scb_rrm_stats_t), sizeof(uint32));
		}
#endif // endif
		scb_cubby->scb_rrm_stats = scb_stats;

		if (isset(((rrm_bsscfg_cubby_t *)RRM_BSSCFG_CUBBY(rrm_info, scb->bsscfg))->rrm_cap,
			DOT11_RRM_CAP_TSCM)) {
			scb_tscm = (scb_tscm_t*)secptr;
		}
		scb_cubby->scb_rrm_tscm = scb_tscm;
#endif /* WL11K_ALL_MEAS */
	}

	return BCME_OK;
}

/* scb cubby_deinit fucntion */
static void wlc_rrm_scb_deinit(void *ctx, struct scb *scb)
{
	wlc_rrm_info_t *rrm_info = (wlc_rrm_info_t *)ctx;
	wlc_info_t *wlc = rrm_info->wlc;
	rrm_scb_cubby_t *scb_cubby = RRM_SCB_CUBBY(rrm_info, scb);
	rrm_scb_info_t *scb_info = RRM_SCB_INFO(rrm_info, scb);
#ifdef WL11K_NBR_MEAS
#ifdef WL11K_AP
	struct scb_iter scbiter;
	struct scb *tscb;
	bool delete_timer = TRUE;
	int idx;
	wlc_bsscfg_t *cfg;
	rrm_bsscfg_cubby_t *rrm_cfg = NULL;
	FOREACH_UP_AP(wlc, idx, cfg) {
		FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, tscb) {
			rrm_scb_info_t *tscb_info;
			rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, cfg);
			tscb_info = RRM_SCB_INFO(rrm_info, tscb);
			if (!scb_info) {
				continue;
			}

			if (isset(tscb_info->rrm_capabilities, DOT11_RMREQ_BCN_PASSIVE)) {
				delete_timer = FALSE;
				break;
			}
		}
	}
	if (delete_timer) {
		wl_del_timer(wlc->wl, rrm_info->rrm_timer);
		if (rrm_cfg)
			rrm_cfg->rrm_timer_set = FALSE;
	}
	wlc_rrm_flush_bcnnbrs(rrm_info, scb);
#endif /* WL11K_AP */
#endif /* WL11K_NBR_MEAS */

	if (scb_info)
		wlc_scb_sec_cubby_free(wlc, scb, scb_info);

	scb_cubby->scb_info = NULL;

#ifdef WL11K_ALL_MEAS
	scb_cubby->scb_rrm_stats = NULL;
	scb_cubby->scb_rrm_tscm = NULL;
#endif /* WL11K_ALL_MEAS */
}

#ifdef BCMDBG
/* scb cubby_dump fucntion */
static void wlc_rrm_scb_dump(void *ctx, struct scb *scb, struct bcmstrbuf *b)
{
	wlc_rrm_info_t *rrm_info = (wlc_rrm_info_t *)ctx;
	rrm_scb_info_t *scb_info = RRM_SCB_INFO(rrm_info, scb);
	rrm_nbr_rep_t *nbr_rep;
	int count = 0;
	char eabuf[ETHER_ADDR_STR_LEN];

	if (scb_info == NULL)
		return;

	bcm_bprintf(b, "     SCB: len:%d timestamp:%u\n",
		scb_info->len, scb_info->timestamp);

	bcm_bprintf(b, "RRM capability = %02x %02x %02x %02x %02x \n",
		scb_info->rrm_capabilities[0], scb_info->rrm_capabilities[1],
		scb_info->rrm_capabilities[2], scb_info->rrm_capabilities[3],
		scb_info->rrm_capabilities[4]);
	bcm_bprintf(b, "RRM Neighbor Report (from beacons):\n");
	nbr_rep = scb_info->nbr_rep_head;
	while (nbr_rep) {
		count++;
		bcm_bprintf(b, "AP %2d: ", count);
		bcm_bprintf(b, "bssid %s ", bcm_ether_ntoa(&nbr_rep->nbr_elt.bssid, eabuf));
		bcm_bprintf(b, "bssid_info %08x ", load32_ua(&nbr_rep->nbr_elt.bssid_info));
		bcm_bprintf(b, "reg %2d channel %3d phytype %d pref %3d\n", nbr_rep->nbr_elt.reg,
			nbr_rep->nbr_elt.channel, nbr_rep->nbr_elt.phytype,
			nbr_rep->nbr_elt.bss_trans_preference);
		nbr_rep = nbr_rep->next;
	}
}
#endif /* BCMDBG */

void
wlc_rrm_upd_data_activity_ts(wlc_rrm_info_t *ri)
{
	if (ri->bcn_req_traff_meas_prd)
		ri->data_activity_ts = OSL_SYSUPTIME();
}

static void
wlc_rrm_watchdog(void *context)
{
	wlc_rrm_info_t *ri = (wlc_rrm_info_t *) context;
	if (ri->bcn_req_thrtl_win_sec) {
		ri->bcn_req_thrtl_win_sec--;
		/* Reset params for next window (to be started at next bcn_req rx) */
		if (ri->bcn_req_thrtl_win_sec == 0) {
			ri->bcn_req_off_chan_time = 0;
			ri->bcn_req_win_scan_cnt = 0;
		}
	}
}

#if defined(WL11K_AP) && (defined(WL11K_NBR_MEAS) || (defined(WL_MBO) && \
	!defined(WL_MBO_DISABLED) && defined(MBO_AP)))
/* This routine prepares Neighbor report with BSS transition preference subelement
 * for every neighbor present in static list. In addition AP add it's own information
 */
void
wlc_rrm_get_nbr_report(wlc_info_t* wlc, wlc_bsscfg_t *bsscfg, uint16 cnt, void* buf)
{
	wlc_rrm_info_t *rrm_info = wlc->rrm_info;
	rrm_bsscfg_cubby_t *rrm_cfg;
	rrm_nbr_rep_t *nbr_rep;
	dot11_neighbor_rep_ie_t *nbrrep;
	uint8 *ptr;
	dot11_ngbr_bsstrans_pref_se_t *pref;
	int i;

	if (!rrm_info) {
		return;
	}

	rrm_cfg = RRM_BSSCFG_CUBBY(rrm_info, bsscfg);
	ASSERT(rrm_cfg != NULL);

	nbr_rep = rrm_cfg->nbr_rep_head;
	ptr = (uint8 *)buf;

	for (i = 0; i < cnt; i++) {
		nbrrep = (dot11_neighbor_rep_ie_t *)ptr;

		memcpy(nbrrep, &nbr_rep->nbr_elt, (TLV_HDR_LEN + DOT11_NEIGHBOR_REP_IE_FIXED_LEN));

		nbrrep->len = DOT11_NEIGHBOR_REP_IE_FIXED_LEN +
			DOT11_NGBR_BSSTRANS_PREF_SE_LEN + TLV_HDR_LEN;

		/* Add preference subelement */
		pref = (dot11_ngbr_bsstrans_pref_se_t*) nbrrep->data;
		pref->sub_id = DOT11_NGBR_BSSTRANS_PREF_SE_ID;
		pref->len = DOT11_NGBR_BSSTRANS_PREF_SE_LEN;
		/* Fill preference with once firwmare stores to
		 * nbr_rep->nbr_elt.bss_trans_preference
		 */
		pref->preference = nbr_rep->nbr_elt.bss_trans_preference;
		/* Move to the next one */
		nbr_rep = nbr_rep->next;

		ptr += TLV_HDR_LEN + DOT11_NEIGHBOR_REP_IE_FIXED_LEN +
			TLV_HDR_LEN + DOT11_NGBR_BSSTRANS_PREF_SE_LEN;
	}
#ifdef WLWNM_AP
	wlc_create_nbr_element_own_bss(wlc, bsscfg, &ptr);
#endif /* WLWNM_AP */
}
#endif /* WL11K_AP */

/* wrapper to call nbr cnt, as both rrm bsscfg cubby and RRM_BSSCFG_CUBBY
 * are defined in wlc_rrm.c
 */
int
wlc_rrm_get_neighbor_count(wlc_info_t* wlc, wlc_bsscfg_t* bsscfg, int addr_type)
{
#if defined(WL11K_AP) && defined(WL11K_NBR_MEAS)
	rrm_bsscfg_cubby_t *rrm_cfg;
#endif /* WL11K_AP &&  11K_NBR_MEAS */
	uint16 list_cnt = 0;
#if defined(WL11K_AP) && defined(WL11K_NBR_MEAS)
	rrm_cfg = RRM_BSSCFG_CUBBY(wlc->rrm_info, bsscfg);
	list_cnt = _wlc_rrm_get_neighbor_count(wlc->rrm_info, rrm_cfg, addr_type);
#endif /* WL11K_AP && 11K_NBR_MEAS */
	return list_cnt;
}
