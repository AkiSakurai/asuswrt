/**
 * @file
 * @brief
 * Code that assesses the link quality
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
 * $Id: wlc_lq.c 781404 2019-11-20 08:58:36Z $
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <wlc_types.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_channel.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scan.h>
#ifdef WLMCNX
#include <wlc_mcnx.h>
#endif // endif
#ifdef APCS
#include <wlc_apcs.h>
#endif // endif
#include <wlc_rm.h>
#include <wlc_ap.h>
#include <wlc_scb.h>
#include <wlc_frmutil.h>
#include <wl_export.h>
#include <wlc_tbtt.h>
#include <wlc_bmac.h>
#include <wlc_lq.h>
#include <wl_dbg.h>
#include <wlc_event_utils.h>
#include <wlc_cca.h>
#include <wlc_dump.h>
#include <wlc_stf.h>
#include <wlc_iocv.h>
#include <wlc_assoc.h>
#ifdef WLRSDB
#include <wlc_rsdb.h>
#endif /* WLRSDB */
#ifdef ECOUNTERS
#include <ecounters.h>
#endif // endif

#include <phy_noise_api.h>

/* iovar table */
enum {
	IOV_SNR,
	IOV_NOISE_LTE,
	IOV_NOISE_LTE_RESET,
	IOV_RSSI_EVENT,
	IOV_RSSI_WINDOW_SZ,
	IOV_SNR_WINDOW_SZ,
	IOV_RSSI_DELTA,
	IOV_SNR_DELTA,
	IOV_LQ_MAX_BCN_LOSS,
	IOV_LINKQUAL_ONOFF,
	IOV_GET_LINKQUAL_STATS,
	IOV_CHANIM_ENAB,
	IOV_CHANIM_STATE, /* chan interference detect */
	IOV_CHANIM_MODE,
	IOV_CHANIM_STATS, /* the chanim stats */
	IOV_CCASTATS_THRES,
	IOV_CRSGLITCH_THRES, /* chan interference threshold */
	IOV_BGNOISE_THRES,  /* background noise threshold */
	IOV_SAMPLE_PERIOD,
	IOV_THRESHOLD_TIME,
	IOV_MAX_ACS,
	IOV_LOCKOUT_PERIOD,
	IOV_ACS_RECORD,
	IOV_RSSI_ANT,
	IOV_RSSI_MONITOR,
	IOV_LQ_LAST
};

static const bcm_iovar_t wlc_lq_iovars[] = {
	{"rssi_event", IOV_RSSI_EVENT,
	(0), 0, IOVT_BUFFER, sizeof(wl_rssi_event_t)
	},
#ifdef STA
	{"rssi_win", IOV_RSSI_WINDOW_SZ,
	(0), 0, IOVT_UINT16, 0
	},
	{"snr_win", IOV_SNR_WINDOW_SZ,
	(0), 0, IOVT_UINT16, 0
	},
	{"lq_snr_delta", IOV_SNR_DELTA,
	(IOVF_WHL), 0, IOVT_UINT8, 0
	},
	{"lq_rssi_delta", IOV_RSSI_DELTA,
	(IOVF_WHL), 0, IOVT_UINT8, 0
	},
	{"lq_max_bcn_thresh", IOV_LQ_MAX_BCN_LOSS,
	(IOVF_WHL), 0, IOVT_UINT32, 0
	},
#endif /* STA */

	{"snr", IOV_SNR,
	(0), 0, IOVT_INT32, 0
	},
	{"noise_lte", IOV_NOISE_LTE,
	(0), 0, IOVT_INT32, 0
	},
	{"noise_lte_reset", IOV_NOISE_LTE_RESET,
	(0), 0, IOVT_INT32, 0
	},
	{"chanim_enab", IOV_CHANIM_ENAB,
	(0), 0, IOVT_UINT32, 0
	},
#ifdef WLCHANIM
	{"chanim_state", IOV_CHANIM_STATE,
	(0), 0, IOVT_BOOL, 0
	},
	{"chanim_mode", IOV_CHANIM_MODE,
	(0), 0, IOVT_UINT8, 0
	},
	{"chanim_ccathres", IOV_CCASTATS_THRES,
	(0), 0, IOVT_UINT8, 0
	},
	{"chanim_glitchthres", IOV_CRSGLITCH_THRES,
	(0), 0, IOVT_UINT32, 0
	},
	{"chanim_bgnoisethres", IOV_BGNOISE_THRES,
	(0), 0, IOVT_INT8, 0
	},
	{"chanim_sample_period", IOV_SAMPLE_PERIOD,
	(0), 0, IOVT_UINT8, 0
	},
	{"chanim_threshold_time", IOV_THRESHOLD_TIME,
	(0), 0, IOVT_UINT8, 0
	},
	{"chanim_max_acs", IOV_MAX_ACS,
	(0), 0, IOVT_UINT8, 0
	},
	{"chanim_lockout_period", IOV_LOCKOUT_PERIOD,
	(0), 0, IOVT_UINT32, 0
	},
	{"chanim_acs_record", IOV_ACS_RECORD,
	(0), 0, IOVT_BUFFER, sizeof(wl_acs_record_t),
	},
	{"chanim_stats", IOV_CHANIM_STATS,
	(0), 0, IOVT_BUFFER, sizeof(wl_chanim_stats_t),
	},
#endif /* WLCHANIM */
	{"phy_rssi_ant", IOV_RSSI_ANT, (0), 0, IOVT_BUFFER, sizeof(wl_rssi_ant_t)},
	{"rssi_monitor", IOV_RSSI_MONITOR,
	(0), 0, IOVT_BUFFER, sizeof(wl_rssi_monitor_cfg_t)
	},
	{NULL, 0, 0, 0, 0, 0}
};

/* ioctl table */
static const wlc_ioctl_cmd_t wlc_lq_ioctls[] = {
	{WLC_GET_RSSI, 0, sizeof(int32)},
	{WLC_GET_PHY_NOISE, 0, sizeof(int32)}
};

/* special rssi sample window in monitor mode */
typedef struct {
	/* raw per antenna rssi - valid when # ants > 1 */
	uint16	rssi_chain_window_sz;
	uint8	rssi_chain_index;
	int8	*rssi_chain_window;	/* int8 [WL_RSSI_ANT_MAX][MA_WINDOW_SZ] */
} monitor_rssi_win_t;

/* allocation size */
#define MONITOR_RSSI_WIN_SZ(lqi) (sizeof(monitor_rssi_win_t) + \
				  MONITOR_MA_WIN_SZ * WL_RSSI_ANT_MAX)

/* size of channel_qa_sample array */
#define WLC_CHANNEL_QA_NSAMP	2
/* size of noise_lte_values array */
#define WLC_NOISE_LTE_SIZE 9

/* module specific data */
struct wlc_lq_info {
	/* back pointers */
	wlc_info_t *wlc;	/* pointer to main wlc structure */
	osl_t *osh;

	/* cubby handles */
	int cfgh;
	int scbh;

	/* noise levels */
	int8 noise;
	int8 noise_lte;
	int noise_lte_values[WLC_NOISE_LTE_SIZE];
	uint8 noise_lte_val_idx;

	/* rssi & snr sample windows allocation sizes */
	uint8 sta_ma_window_sz;
	uint8 def_ma_window_sz;

	/* misc */
	uint8 ants;		/* # antennas */

	/* channel quality measure */
	bool channel_qa_active;		/**< true if chan qual measurement in progress */
	int channel_quality;		/* quality metric(0-3) of last measured channel, or
					 * -1 if in progress
					 */
	uint8 channel_qa_channel;	/**< channel number of channel being evaluated */
	int8 channel_qa_sample[WLC_CHANNEL_QA_NSAMP];	/**< rssi samples of background
							 * noise
							 */
	uint channel_qa_sample_num;	/**< count of samples in channel_qa_sample array */

	/* phy noise sample requests */
	uint8 noise_req;

	/* bcn rssi sample window */
	uint8 sta_bcn_window_sz;

	/* monitor mode rssi window */
	monitor_rssi_win_t *monitor;
};

/* moving average window size */
#define STA_MA_WINDOW_SZ	16	/* for STA */
#define DEF_MA_WINDOW_SZ	8
#define STA_BCN_WINDOW_SIZE	16
#define MONITOR_MA_WIN_SZ	16

/* # antennas */
#define RX_ANT_NUM(lqi)		(lqi)->ants

/* bss specific data */
typedef struct {
	/* rssi & snr sample windows sampling sizes */
	uint16 rssi_window_sz;		/* rssi window size. apply to all scbs in the bsscfg. */
	uint16 snr_window_sz;		/* SNR window size. apply to all scbs in the bsscfg. */

/* **** the following fields are all for infra STA only. **** */
/* For IBSS these are allocated but not used. The reason is we change the cfg->BSS
 * type in the middle of a connection creation process hence we can't key their alloc
 * off cfg->BSS in the beginning of a bsscfg creation and their free at the end of
 * the bsscfg deletion.
 */
	/* inline rssi average states */
	uint8 *rssi_qdb_window;		/* window for rssi fraction in qdb units */
	int rssi_tot;			/**< rssi samples total, in qdb units */
	uint8 rssi_count;		/* # of valid samples in the window */
	bool last_rssi_is_ucast;	/**< last RSSI sample is from unicast frame */

	/* inline snr average states */
	uint snr_tot;			/* snr samples total */
	uint8 snr_count;		/**< number of valid values in the window */
	bool last_snr_is_ucast;		/**< last SNR sample is from unicast frame */

	/* RSSI event notification */
	wl_rssi_event_t *rssi_event;	/**< RSSI event notification configuration. */
	struct wl_timer *rssi_event_timer;	/**< timer to limit event notifications */
	bool rssi_event_timer_active;	/**< flag to indicate timer active */
	uint8 rssi_level;		/**< current rssi based on notification configuration */

	uint16 rssi_bcn_window_sz;	/* rssi window size. */

	/* RSSI/SNR auto-deduction upon consecutive beacon loss */
	uint8 rssi_delta;	/* Reduce RSSI by this much upon consecutive beacon loss */
	uint8 snr_delta;	/* Reduce SNR by this much upon consecutive beacon loss */
	int32 max_lq_bcn_loss;	/* beacon loss threshold in ms */
	uint32 lq_last_bcn_time; /* local time for last beacon received */
} bss_lq_info_t;

/* handy macros to access bsscfg cubby & data */
#define BSS_LQ_INFO_LOC(lqi, cfg)	(bss_lq_info_t **)BSSCFG_CUBBY(cfg, (lqi)->cfgh)
#define BSS_LQ_INFO(lqi, cfg)		*BSS_LQ_INFO_LOC(lqi, cfg)

/* rssi_window_sz & snr_window_sz are encoded, use these macros to decode */
#define MA_ADMIT_ALL_FRAME(x)	(((x) & 0x0100) != 0)
#define MA_ADMIT_MCAST_ONLY(x)	(((x) & 0x0200) != 0)
#define MA_WIN_DEBUG(x)		(((x) & 0x8000) != 0)
#define MA_WIN_SZ(x)		(uint8)(x)

/* moving average window allocation size based on bsscfg role */
#define BSS_MA_WINDOW_SZ(lqi, cfg)	(((void)cfg, BSSCFG_STA(cfg)) /* && (cfg)->BSS */ ? \
					 (lqi)->sta_ma_window_sz : (lqi)->def_ma_window_sz)

/* scb specific data */
typedef struct {
	/* RSSI moving average */
	int8	*rssi_window;		/* rssi samples buffer - int8 [MA_WINDOW_SZ] */
	uint8	rssi_index;

	/* raw per antenna rssi - valid when # ants > 1 */
	int8	*rssi_chain_window;	/* int8 [WL_RSSI_ANT_MAX][MA_WINDOW_SZ] */
	uint8	rssi_chain_index;

/* **** the following fields are all for infra STA only. **** */
/* For IBSS these are allocated but not used. The reason is we change the cfg->BSS
 * type in the middle of a connection creation process hence we can't key their alloc
 * off cfg->BSS in the beginning of a bsscfg creation and their free at the end of
 * the bsscfg deletion.
 */
	/* SNR moving average */
	uint8	*snr_window;		/**< SNR moving average window - uint8 [MA_WINDOW_SZ] */
	uint8	snr_index;		/**< SNR moving average window index */

	/* raw per antenna bcn-rssi valid when #ants >1 */
	int8	*rssi_chain_bcn_window;
	uint8	rssi_chain_bcn_index;
} scb_lq_info_t;

/* handy macros to access scb cubby & data */
#define SCB_LQ_INFO_LOC(lqi, scb)	(scb_lq_info_t **)SCB_CUBBY(scb, (lqi)->scbh)
#define SCB_LQ_INFO(lqi, scb)		*SCB_LQ_INFO_LOC(lqi, scb)

/* **** Use these MACROs to decide if an scb has certain data and behavior **** */
/* per-scb rssi sampling */
#define PER_SCB_RSSI_SAMP(cfg)		TRUE
#define PER_SCB_RSSI_QDB_SAMP(cfg)	(BSSCFG_STA(cfg) /* && cfg->BSS */)
/* per-scb per-ant rssi sampling */
#define PER_ANT_RSSI_SAMP(lqi, cfg)	(PER_SCB_RSSI_SAMP(cfg) && (RX_ANT_NUM(lqi) > 1))
/* per-scb per-ant rssi sampling */
#define PER_ANT_BCN_RSSI_SAMP(lqi, cfg)	(PER_SCB_RSSI_SAMP(cfg) && (RX_ANT_NUM(lqi) >= 1))
/* inline per-scb rssi moving average calculation */
#define PER_SCB_RSSI_MOVE_AVG(cfg)	(BSSCFG_STA(cfg) && cfg->BSS)
/* per-scb snr sampling */
#define PER_SCB_SNR_SAMP(cfg)		(BSSCFG_STA(cfg) /* && cfg->BSS */)
/* inline per-scb snr moving average calculation */
#define PER_SCB_SNR_MOVE_AVG(cfg)	(BSSCFG_STA(cfg) && cfg->BSS)

/* local function declarataions */

static int wlc_lq_doiovar(void *hdl, uint32 actionid,
	void *p, uint plen, void *arg, uint alen, uint val_size, struct wlc_if *wlcif);
static int wlc_lq_doioctl(void *ctx, uint32 cmd, void *arg, uint len, struct wlc_if *wlcif);
static void wlc_lq_watchdog(void *ctx);
#if defined(BCMDBG) || defined(WLTEST)
static int wlc_lq_dump(wlc_info_t *wlc, struct bcmstrbuf *b);
#endif // endif

static int wlc_lq_bss_init(void *ctx, wlc_bsscfg_t *cfg);
static void wlc_lq_bss_deinit(void *ctx, wlc_bsscfg_t *cfg);
#ifdef WLRSDB
static int wlc_lq_bss_get(void *ctx, wlc_bsscfg_t *cfg, uint8 *data, int *len);
static int wlc_lq_bss_set(void *ctx, wlc_bsscfg_t *cfg, const uint8 *data, int len);
#endif /* WLRSDB */
#if defined(BCMDBG) || defined(WLTEST)
static void wlc_lq_bss_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b);
#endif // endif

static int wlc_lq_scb_init(void *ctx, struct scb *scb);
static void wlc_lq_scb_deinit(void *ctx, struct scb *scb);
static uint wlc_lq_scb_secsz(void *ctx, struct scb *scb);
#if defined(BCMDBG) || defined(WLTEST)
static void wlc_lq_scb_dump(void *ctx, struct scb *scb, struct bcmstrbuf *b);
#endif // endif

static void wlc_lq_rssi_event_timeout(void *arg);
int8 wlc_lq_rssi_ma(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct scb *scb);
#ifdef STA
static void wlc_lq_ant_bcn_rssi(void *ctx, bss_rx_bcn_notif_data_t *notif_data);
#ifdef WLMCNX
static void wlc_lq_update_tbtt(void *ctx, wlc_mcnx_intr_data_t *notif_data);
#endif // endif
#endif /* STA */

#if defined(WL_MONITOR) && defined(WLTEST)
/* local functions used in monitor mode */
static int8 wlc_lq_ant_rssi_monitor_get(wlc_info_t *wlc, int chain);
#if defined(BCMDBG) || defined(WLTEST)
static void wlc_lq_monitor_dump(wlc_info_t *wlc, struct bcmstrbuf *b);
#endif // endif
#endif /* WL_MONITOR && WLTEST */

#ifdef WLCHANIM
/* **** chanim **** */

/* TODO: move chanim code out to a different file */

/** accumulative  count structure */
typedef struct {
	uint32 glitch_cnt;
	uint32 badplcp_cnt;
	uint32 ccastats_us[CCASTATS_MAX]; /**< in microsecond */
	chanspec_t chanspec;
	uint stats_ms;			/**< accumulative time, in millisecond */
	uint32 bphy_glitch_cnt;
	uint32 bphy_badplcp_cnt;
} chanim_accum_t;

typedef struct {
	uint64 busy_tm;
	uint64 ccastats_us[CCASTATS_MAX]; /* in microsecond */
	uint64 rxcrs_pri20;             /* rx crs primary 20 */
	uint64 rxcrs_sec20;             /* rx crs secondary 20 */
	uint64 rxcrs_sec40;             /* rx crs secondary 40 */
	uint64 rxcrs_sec80;             /* rx crs secondary 80 */
	chanspec_t chanspec;
} chanim_acc_us_t;

/** basic count struct */
typedef struct {
	uint16 glitch_cnt;
	uint16 badplcp_cnt;
	uint32 ccastats_cnt[CCASTATS_MAX];
	uint timestamp;
	uint32 bphy_glitch_cnt;
	uint32 bphy_badplcp_cnt;
} chanim_cnt_t;
/** configurable parameters */
typedef struct {
	uint8 mode;		/**< configurable chanim mode */
	uint8 sample_period;	/**< in seconds, time to do a sampling measurement */
	uint8 threshold_time;	/**< number of sample period to trigger an action */
	uint8 max_acs;		/**< the maximum acs scans for one lockout period */
	uint32 lockout_period;	/**< in seconds, time for one lockout period */
	uint32 crsglitch_thres;
	uint32 ccastats_thres;
	int8 bgnoise_thres; /**< background noise threshold */
	uint scb_max_probe; /**< when triggered by intf, how many times to probe */
} chanim_config_t;

/** transient counters/stamps */
typedef struct {
	bool state;
	uint32 detecttime;
	uint32 flags;
	uint8 record_idx;   /**< where the next acs record should locate */
	uint scb_max_probe; /**< original number of scb probe to conduct */
	uint scb_timeout;   /**< the storage for the original scb timeout that is swapped */
} chanim_mark_t;

typedef struct wlc_chanim_stats {
	struct wlc_chanim_stats *next;
	chanim_stats_t chanim_stats;
	bool is_valid;
} wlc_chanim_stats_t;

typedef struct wlc_chanim_stats_us {
	struct wlc_chanim_stats_us *next;
	chanim_stats_us_v2_t chanim_stats_us;
} wlc_chanim_stats_us_t;

/* chanim module info struct */
typedef struct chanim_interface_info {
	struct chanim_interface_info *next;
	chanim_accum_t accum;		/* accumulative counts */
	wlc_chanim_stats_t *stats;	/* respective chspec stats wlc->chanim_info->stats */
	chanspec_t chanspec;		/* chanspec of the interface */
	chanim_acc_us_t acc_us;
	chanim_acc_us_t last_acc_us;
	wlc_chanim_stats_us_t *stats_us;
	chanim_stats_us_v2_t chanim_stats_us;
} chanim_interface_info_t;

struct chanim_info {
	wlc_chanim_stats_t *stats; /**< normalized stats obtained during scan */
	chanim_cnt_t last_cnt;     /**< count from last read */
	chanim_mark_t mark;
	chanim_config_t config;
	chanim_acs_record_t record[CHANIM_ACS_RECORD];
	wlc_info_t *wlc;
	chanim_interface_info_t *ifaces;
	chanim_interface_info_t *free_list;
	wlc_chanim_stats_us_t *stats_us;
	chanim_cnt_us_t last_cnt_us; /* count from last read */
	wlc_chanim_stats_t cur_stats; /* normalized stats obtained in home channel */
	struct wl_timer *chanim_timer;     /* time to switch channel after last beacon */
};

#define chanim_mark(c_info)	(c_info)->mark
#define chanim_config(c_info)	(c_info)->config
#define chanim_act_delay(c_info) (chanim_config(c_info).sample_period * \
				  chanim_config(c_info).threshold_time)

#define CHANIM_SCB_MAX_PROBE 20

#define CRSGLITCH_THRESHOLD_DEFAULT	4000 /* normalized per second count */
#define CCASTATS_THRESHOLD_DEFAULT	40 /* normalized percent stats 0 - 255 */
#define BGNOISE_THRESHOLD_DEFAULT	-55 /* in dBm */
#define SAMPLE_PERIOD_DEFAULT		1 /* in second */
#define THRESHOLD_TIME_DEFAULT		2 /* number of sample periods */
#define LOCKOUT_PERIOD_DEFAULT		120 /* in second */
#define MAX_ACS_DEFAULT			5 /* number of ACS in a lockout period */

#define SAMPLE_PERIOD_MIN		1
#define THRESHOLD_TIME_MIN		1

/* chanim flag defines */
#define CHANIM_DETECTED		0x1	 /* interference detected */
#define CHANIM_ACTON		0x2	 /* ACT triggered */

static bool wlc_lq_chanim_any_if_info_setup(chanim_interface_info_t *ifaces);

#ifndef WLCHANIM_DISABLED
static int wlc_lq_chanim_attach(wlc_info_t *wlc);
static void wlc_lq_chanim_detach(chanim_info_t *c_info);
static void wlc_chanim_mfree(osl_t *osh, chanim_info_t *c_info);

#endif /* WLCHANIM_DISABLED */
#ifdef WLCHANIM_US
static wlc_chanim_stats_us_t *wlc_lq_chanim_create_stats_us(wlc_info_t *wlc, chanspec_t chanspec);
static int wlc_lq_chanim_get_stats_us(chanim_info_t *c_info, wl_chanim_stats_us_v2_t* iob,
	int *len, int count, uint32 time);
static wlc_chanim_stats_us_t *wlc_lq_chanim_find_stats_us(wlc_info_t *wlc, chanspec_t chanspec);
static wlc_chanim_stats_us_t *wlc_lq_chanim_chanspec_to_stats_us(chanim_info_t *c_info,
	chanspec_t chanspec);
static void wlc_lq_chanim_insert_stats_us(wlc_chanim_stats_us_t **rootp,
	wlc_chanim_stats_us_t *new);
static void wlc_lq_chanim_us_accum(chanim_info_t* c_info, chanim_cnt_us_t *cur_cnt,
	chanim_acc_us_t *acc);
#ifndef WLCHANIM_DISABLED
static void wlc_chanim_msec_timeout(void *arg);
#endif /* WLCCHANIM_DISABLED */
#endif /* WLCHANIM_US */
static wlc_chanim_stats_t *wlc_lq_chanim_create_stats(wlc_info_t *wlc, chanspec_t chanspec);
static int wlc_lq_chanim_get_stats(chanim_info_t *c_info, wl_chanim_stats_t* iob, int *len, int);
static wlc_chanim_stats_t *wlc_lq_chanim_find_stats(wlc_info_t *wlc, chanspec_t chanspec);
static int wlc_lq_chanim_get_acs_record(chanim_info_t *c_info, int buf_len, void *output);
static void wlc_lq_chanim_insert_stats(wlc_chanim_stats_t **rootp, wlc_chanim_stats_t *new);
static void wlc_lq_chanim_meas(wlc_info_t *wlc, chanim_cnt_t *chanim_cnt,
	chanim_cnt_us_t *chanim_cnt_us);
static void wlc_lq_chanim_glitch_accum(chanim_info_t* c_info, chanim_cnt_t *cur_cnt,
	chanim_accum_t *acc);
static void wlc_lq_chanim_badplcp_accum(chanim_info_t* c_info, chanim_cnt_t *cur_cnt,
	chanim_accum_t *acc);
static void wlc_lq_chanim_ccastats_accum(chanim_info_t* c_info, chanim_cnt_t *cur_cnt,
	chanim_accum_t *acc);
static void wlc_lq_chanim_accum(wlc_info_t* wlc, chanspec_t chanspec, chanim_accum_t *acc,
	chanim_acc_us_t *acc_us);
static void wlc_lq_chanim_clear_acc(wlc_info_t* wlc, chanim_accum_t* acc,
	chanim_acc_us_t *acc_us, bool chan_switch);
static int8 wlc_lq_chanim_phy_noise(wlc_info_t *wlc);
static void wlc_lq_chanim_close(wlc_info_t* wlc, chanspec_t chanspec, chanim_accum_t* acc,
	wlc_chanim_stats_t *cur_stats, chanim_acc_us_t *acc_us,
	wlc_chanim_stats_us_t *cur_stats_us, bool chan_switch);
static bool wlc_lq_chanim_interfered_glitch(wlc_chanim_stats_t *stats, uint32 thres);
static bool wlc_lq_chanim_interfered_cca(wlc_chanim_stats_t *stats, uint32 thres);
static bool wlc_lq_chanim_interfered_noise(wlc_chanim_stats_t *stats, int8 thres);
static chanim_interface_info_t *wlc_lq_chanim_if_info_find(chanim_interface_info_t *ifaces,
	chanspec_t chanspec);
static wlc_chanim_stats_t *wlc_lq_chanim_chanspec_to_stats(chanim_info_t *c_info,
	chanspec_t chanspec, bool scan_param);
#ifdef BCMDBG
static int wlc_lq_chanim_display(wlc_info_t *wlc, chanspec_t chanspec,
	wlc_chanim_stats_t *cur_stats);
#endif // endif
#endif /* WLCHANIM */

#ifdef WLCQ
static int wlc_lq_channel_qa_start(wlc_info_t *wlc);
static int wlc_lq_channel_qa_eval(wlc_info_t *wlc);
static void wlc_lq_channel_qa_sample_cb(wlc_info_t *wlc, uint8 channel, int8 noise_dbm);
#endif /* WLCQ */

#ifdef ECOUNTERS
static int wlc_ecounters_rx_lqm(uint16 stats_type, void *context,
	const ecounters_stats_types_report_req_t *req,
	struct bcm_xtlvbuf *xtlvbuf, uint32 *cookie, const bcm_xtlv_t* tlv, uint16 *awl);
#endif // endif

#ifdef WLRSDB
typedef struct {
	uint16 rssi_window_sz;		/* rssi window size. apply to all scbs in the bsscfg. */
	uint16 snr_window_sz;		/* SNR window size. apply to all scbs in the bsscfg. */
	wl_rssi_event_t rssi_event;	/**< RSSI event notification configuration. */
	uint8 rssi_delta;		/* Reduce RSSI by this much upon consecutive beacon loss */
	uint8 snr_delta;		/* Reduce SNR by this much upon consecutive beacon loss */
	int32 max_lq_bcn_loss;		/* beacon loss threshold in ms */
} wlc_lq_copy_t;

#define LQ_COPY_SIZE	sizeof(wlc_lq_copy_t)

static int
wlc_lq_bss_get(void *ctx, wlc_bsscfg_t *cfg, uint8 *data, int *len)
{
	wlc_lq_info_t *lqi = (wlc_lq_info_t *)ctx;
	bss_lq_info_t *blqi = BSS_LQ_INFO(lqi, cfg);
	wlc_lq_copy_t *cp = (wlc_lq_copy_t *)data;

	ASSERT(cfg);
	ASSERT(data);
	if (*len < LQ_COPY_SIZE) {
		*len = LQ_COPY_SIZE;
		return BCME_BUFTOOSHORT;
	}

	/* retrieve the PM mode between bsscfg's */
	if (BSSCFG_STA(cfg)) {
		cp->rssi_window_sz = blqi->rssi_window_sz;
		cp->snr_window_sz = blqi->snr_window_sz;
		memcpy(&cp->rssi_event, blqi->rssi_event, sizeof(cp->rssi_event));
		cp->rssi_delta = blqi->rssi_delta;
		cp->snr_delta = blqi->snr_delta;
		cp->max_lq_bcn_loss = blqi->max_lq_bcn_loss;
		*len = sizeof(*cp);
	}
	return BCME_OK;
}

static int
wlc_lq_bss_set(void *ctx, wlc_bsscfg_t *cfg, const uint8 *data, int len)
{
	wlc_lq_info_t *lqi = (wlc_lq_info_t *)ctx;
	bss_lq_info_t *blqi = BSS_LQ_INFO(lqi, cfg);
	const wlc_lq_copy_t *cp = (const wlc_lq_copy_t *)data;

	ASSERT(cfg);
	ASSERT(data);
	if (len < LQ_COPY_SIZE) {
		return BCME_BUFTOOSHORT;
	}

	/* copy the PM mode between bsscfg's */
	if (BSSCFG_STA(cfg)) {
		blqi->rssi_window_sz = cp->rssi_window_sz;
		blqi->snr_window_sz = cp->snr_window_sz;
		memcpy(blqi->rssi_event, &cp->rssi_event, sizeof(*blqi->rssi_event));
		blqi->rssi_delta = cp->rssi_delta;
		blqi->snr_delta = cp->snr_delta;
		blqi->max_lq_bcn_loss = cp->max_lq_bcn_loss;
	}
	return BCME_OK;
}
#endif /* WLRSDB */

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

/* attach/detach entries */
wlc_lq_info_t *
BCMATTACHFN(wlc_lq_attach)(wlc_info_t *wlc)
{
	wlc_lq_info_t *lqi;
	bsscfg_cubby_params_t bss_cubby_params;
	scb_cubby_params_t scb_cubby_params;

	STATIC_ASSERT(ISPOWEROF2(STA_MA_WINDOW_SZ));
	STATIC_ASSERT(ISPOWEROF2(DEF_MA_WINDOW_SZ));

	/* allocate module private state structure */
	if ((lqi = MALLOCZ(wlc->osh, sizeof(wlc_lq_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: mem alloc failed, allocated %d ytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	lqi->wlc = wlc;
	lqi->osh = wlc->osh;

	lqi->sta_ma_window_sz = STA_MA_WINDOW_SZ;
	lqi->def_ma_window_sz = DEF_MA_WINDOW_SZ;
	lqi->sta_bcn_window_sz = STA_BCN_WINDOW_SIZE;

	lqi->ants = WLC_BITSCNT(wlc->stf->rxchain);
	lqi->ants = MIN(lqi->ants, WL_RSSI_ANT_MAX);

#if defined(WL_MONITOR) && defined(WLTEST)
	/* a special rssi window to be used in monitor mode */
	if ((lqi->monitor = MALLOCZ(wlc->osh, MONITOR_RSSI_WIN_SZ(lqi))) == NULL) {
		WL_ERROR(("wl%d: %s: out of memory, allocated %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	lqi->monitor->rssi_chain_window_sz = MONITOR_MA_WIN_SZ;
	lqi->monitor->rssi_chain_window = (int8 *)&lqi->monitor[1];
#endif // endif

	/* reserve cubby in the bsscfg container for per-bsscfg private data */
	bzero(&bss_cubby_params, sizeof(bss_cubby_params));
	bss_cubby_params.context = lqi;
	bss_cubby_params.fn_init = wlc_lq_bss_init;
	bss_cubby_params.fn_deinit = wlc_lq_bss_deinit;
#if defined(BCMDBG) || defined(WLTEST)
	bss_cubby_params.fn_dump = wlc_lq_bss_dump;
#endif // endif
#ifdef WLRSDB
	bss_cubby_params.fn_get = wlc_lq_bss_get;
	bss_cubby_params.fn_set = wlc_lq_bss_set;
	bss_cubby_params.config_size = LQ_COPY_SIZE;
#endif /* WLRSDB */
	lqi->cfgh = wlc_bsscfg_cubby_reserve_ext(wlc, sizeof(bss_lq_info_t *), &bss_cubby_params);
	if (lqi->cfgh < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* reserve cubby in the scb container for per-scb private data */
	bzero(&scb_cubby_params, sizeof(scb_cubby_params));
	scb_cubby_params.context = lqi;
	scb_cubby_params.fn_init = wlc_lq_scb_init;
	scb_cubby_params.fn_deinit = wlc_lq_scb_deinit;
	scb_cubby_params.fn_secsz = wlc_lq_scb_secsz;
#if defined(BCMDBG) || defined(WLTEST)
	scb_cubby_params.fn_dump = wlc_lq_scb_dump;
#endif // endif
	if ((lqi->scbh = wlc_scb_cubby_reserve_ext(wlc,
			sizeof(scb_lq_info_t *), &scb_cubby_params)) < 0) {
		WL_ERROR(("wl%d: %s: wlc_scb_cubby_reserve_ext() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

#ifdef STA
	if (wlc_bss_rx_bcn_register(wlc, wlc_lq_ant_bcn_rssi, lqi) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_lq_ant_bcn_rssi() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#ifdef WLMCNX
	if (MCNX_ENAB(wlc->pub)) {
		/* register preTBTT callback */
		if ((wlc_mcnx_intr_register(wlc->mcnx, wlc_lq_update_tbtt, lqi)) != BCME_OK) {
			WL_ERROR(("wl%d: wlc_mcnx_intr_register failed (lq_tbtt)\n",
				wlc->pub->unit));
			goto fail;
		}
	}
#endif /* WLMCNX */
#endif /* STA */

	/* register module */
	if (wlc_module_register(wlc->pub, wlc_lq_iovars, "lq", wlc, wlc_lq_doiovar,
			wlc_lq_watchdog, NULL, NULL)) {
		WL_ERROR(("wl%d: %s: wlc_module_register failed\n", wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	if (wlc_module_add_ioctl_fn(wlc->pub, lqi, wlc_lq_doioctl,
			ARRAYSIZE(wlc_lq_ioctls), wlc_lq_ioctls) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_add_ioctl_fn() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

#if defined(BCMDBG) || defined(WLTEST)

	/* keep the existing "wl dump rssi" command, and also use it to dump all
	 * "link quality" samples and results.
	 */
	wlc_dump_register(wlc->pub, "rssi", (dump_fn_t)wlc_lq_dump, (void *)wlc);
#endif // endif

#if defined(RSSI_MONITOR) && !defined(RSSI_MONITOR_DISABLED)
	wlc->pub->cmn->_rmon = TRUE;
#else
	wlc->pub->cmn->_rmon = FALSE;
#endif /* RSSI_MONITOR && !RSSI_MONITOR_DISABLED */

#if defined(WLCHANIM) && !defined(WLCHANIM_DISABLED)
	if (wlc_lq_chanim_attach(wlc)) {
		WL_ERROR(("wl%d: %s: chanim attach failed\n", wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#endif /* WLCHANIM && !defined(WLCHANIM_DISABLED */
#if defined(ECOUNTERS) && defined(EVENT_LOG_COMPILE)
	/* Relocating LQM stats from wlc.c to wlc_lq.c */
	if (ECOUNTERS_ENAB() && (!RSDB_ENAB(wlc->pub) ||
		(RSDB_ENAB(wlc->pub) && wlc->pub->unit == MAC_CORE_UNIT_0))) {
		wl_ecounters_register_source(WL_IFSTATS_XTLV_IF_LQM,
			wlc_ecounters_rx_lqm, (void*)wlc);
	}
#endif // endif
	return lqi;
fail:
	MODULE_DETACH(lqi, wlc_lq_detach);
	return NULL;
}

void
BCMATTACHFN(wlc_lq_detach)(wlc_lq_info_t *lqi)
{
	wlc_info_t *wlc;

	if (lqi == NULL)
		return;

	wlc = lqi->wlc;

#if defined(WLCHANIM) && !defined(WLCHANIM_DISABLED)
	MODULE_DETACH(wlc->chanim_info, wlc_lq_chanim_detach);
#endif /* WLCHANIM && !defined(WLCHANIM_DISABLED) */

	(void)wlc_module_remove_ioctl_fn(wlc->pub, lqi);
	wlc_module_unregister(wlc->pub, "lq", wlc);

#ifdef STA
	wlc_bss_rx_bcn_unregister(wlc, wlc_lq_ant_bcn_rssi, lqi);
#ifdef WLMCNX
	if (MCNX_ENAB(wlc->pub)) {
		wlc_mcnx_intr_unregister(wlc->mcnx, wlc_lq_update_tbtt, lqi);
	}
#endif // endif
#endif /* STA */

#if defined(WL_MONITOR) && defined(WLTEST)
	if (lqi->monitor != NULL) {
		MFREE(wlc->osh, lqi->monitor, MONITOR_RSSI_WIN_SZ(lqi));
	}
#endif // endif

	MFREE(wlc->osh, lqi, sizeof(wlc_lq_info_t));
}

/* iovar/ioctl entries */
static int
wlc_lq_doiovar(void *hdl, uint32 actionid,
	void *p, uint plen, void *a, uint alen, uint val_size, struct wlc_if *wlcif)
{
	wlc_info_t *wlc = (wlc_info_t *)hdl;
	wlc_bsscfg_t *bsscfg;
	int32 int_val = 0;
	int32 *ret_int_ptr;
	int err = 0;
	wlc_lq_info_t *lqi;
	bss_lq_info_t *blqi;

	BCM_REFERENCE(alen);
	BCM_REFERENCE(val_size);

	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	lqi = wlc->lqi;
	blqi = BSS_LQ_INFO(lqi, bsscfg);

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)a;

	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	switch (actionid) {
	case IOV_GVAL(IOV_RSSI_ANT): {
		wl_rssi_ant_t *ant_rssi = (wl_rssi_ant_t *)a;
		struct ether_addr *ea;
		struct scb *scb;
		int i;

#if defined(WL_MONITOR) && defined(WLTEST)
		/* mfgtest special rssi sampling mode */
		if (!wlc->pub->associated && MONITOR_ENAB(wlc)) {
			/* ant_rssi->mask = wlc->stf->rxchain; */
			memset(ant_rssi, 0, sizeof(*ant_rssi));
			for (i = WL_ANT_IDX_1; i < WL_RSSI_ANT_MAX; i++) {
				if (wlc->stf->rxchain & (1 << i)) {
					ant_rssi->rssi_ant[i] =
					        wlc_lq_ant_rssi_monitor_get(wlc, i);
					ant_rssi->count++;
				}
			}
			break;
		}
#endif // endif

		/* Infra STA */
		if (BSSCFG_STA(bsscfg) && bsscfg->BSS) {
			ea = &bsscfg->BSSID;
		}
		/* AP or IBSS, user has given a possible MAC address */
		else if (plen >= ETHER_ADDR_LEN) {
			ea = (struct ether_addr *)p;
		}
		/* A MAC address must present for AP or IBSS */
		else {
			err = BCME_BADARG;
			break;
		}

		/* validate the MAC address */
		if (ETHER_ISMULTI(ea) || ETHER_ISNULLADDR(ea)) {
			/* XXX Should have been BCME_BADARG but
			 * use BCME_UNSUPPORTED instead in order to
			 * not fail users like UTF.
			 */
			err = BCME_UNSUPPORTED;
			break;
		}

		/* find the scb and do the work */
		if ((scb = wlc_scbfind(wlc, bsscfg, ea)) == NULL) {
			err = BCME_NOTFOUND;
			break;
		}

		/* enable per-scb collection on first request */
		wlc_lq_sample_req_enab(scb, RX_LQ_SAMP_REQ_WLC, TRUE);
		/* ant_rssi->mask = wlc->stf->rxchain; */
		memset(ant_rssi, 0, sizeof(*ant_rssi));
		for (i = WL_ANT_IDX_1; i < WL_RSSI_ANT_MAX; i++) {
			if (wlc->stf->rxchain & (1 << i)) {
				ant_rssi->rssi_ant[i] =
					wlc_lq_ant_rssi_get(wlc, bsscfg, scb, i);
				ant_rssi->count++;
			}
		}
		break;
	}

	case IOV_GVAL(IOV_RSSI_EVENT):
		memcpy(a, blqi->rssi_event, sizeof(wl_rssi_event_t));
		break;

	case IOV_SVAL(IOV_RSSI_EVENT):
		if (blqi->rssi_event_timer) {
			wl_del_timer(wlc->wl, blqi->rssi_event_timer);
			wl_free_timer(wlc->wl, blqi->rssi_event_timer);
			blqi->rssi_event_timer = NULL;
		}
		blqi->rssi_event_timer_active = FALSE;
		blqi->rssi_level = 0; 	/* reset current rssi level */
		memcpy(blqi->rssi_event, a, sizeof(wl_rssi_event_t));
		if (blqi->rssi_event->rate_limit_msec) {
			blqi->rssi_event_timer = wl_init_timer(wlc->wl,
				wlc_lq_rssi_event_timeout, bsscfg, "rssi_event");
		}
		break;

#ifdef RSSI_MONITOR
	case IOV_GVAL(IOV_RSSI_MONITOR):
		if (RMON_ENAB(wlc->pub)) {
			wlc_link_qual_t *link = bsscfg->link;
			wl_rssi_monitor_cfg_t *cfg = (wl_rssi_monitor_cfg_t *)a;

			cfg->version = RSSI_MONITOR_VERSION;
			cfg->max_rssi = link->rssi_monitor.max_rssi;
			cfg->min_rssi = link->rssi_monitor.min_rssi;
			cfg->flags = link->rssi_monitor.flag;
			break;
		} else {
			err = BCME_UNSUPPORTED;
			break;
		}

	case IOV_SVAL(IOV_RSSI_MONITOR):
		if (RMON_ENAB(wlc->pub)) {
			wlc_link_qual_t *link = bsscfg->link;
			wl_rssi_monitor_cfg_t *cfg = (wl_rssi_monitor_cfg_t *)a;

			if (cfg->version != RSSI_MONITOR_VERSION) {
				err = BCME_VERSION;
				break;
			}
			if (cfg->flags & RSSI_MONITOR_STOP) {
				link->rssi_monitor.flag &= ~RSSI_MONITOR_ENABLED;
				break;
			}
			link->rssi_monitor.min_rssi = cfg->min_rssi;
			link->rssi_monitor.max_rssi = cfg->max_rssi;
			link->rssi_monitor.flag &= ~RSSI_MONITOR_EVT_SENT;
			link->rssi_monitor.flag |= RSSI_MONITOR_ENABLED;
			wlc_lq_rssi_monitor_event(bsscfg);
			break;
		} else {
			err = BCME_UNSUPPORTED;
			break;
		}
#endif /* RSSI_MONITOR */

#ifdef STA
	case IOV_SVAL(IOV_RSSI_WINDOW_SZ):
		if (MA_WIN_SZ(int_val) == 0 ||
		    MA_WIN_SZ(int_val) > BSS_MA_WINDOW_SZ(lqi, bsscfg)) {
			err = BCME_RANGE;
			break;
		}
		if ((MA_WIN_SZ(int_val) & MA_WIN_SZ(int_val - 1)) != 0) {
			/* Value passed is not power of 2 */
			err = BCME_BADARG;
			break;
		}
		if (!BSSCFG_STA(bsscfg) || !bsscfg->BSS) {
			err = BCME_NOTSTA;
			break;
		}
		blqi->rssi_window_sz = (uint16)int_val;
		wlc_lq_rssi_snr_noise_bss_sta_ma_reset(wlc, bsscfg,
				CHSPEC_BANDUNIT(bsscfg->current_bss->chanspec),
				bsscfg->link->rssi, bsscfg->link->snr, lqi->noise);
		break;

	case IOV_GVAL(IOV_RSSI_WINDOW_SZ):
		*ret_int_ptr = blqi->rssi_window_sz;
		break;

	case IOV_SVAL(IOV_SNR_WINDOW_SZ):
		if (MA_WIN_SZ(int_val) == 0 ||
		    MA_WIN_SZ(int_val) > BSS_MA_WINDOW_SZ(lqi, bsscfg)) {
			err = BCME_RANGE;
			break;
		}

		if ((MA_WIN_SZ(int_val) & MA_WIN_SZ(int_val - 1)) != 0) {
			/* Value passed is not power of 2 */
			err = BCME_BADARG;
			break;
		}
		if (!BSSCFG_STA(bsscfg) || !bsscfg->BSS) {
			err = BCME_NOTSTA;
			break;
		}
		blqi->snr_window_sz = (uint16)int_val;
		wlc_lq_rssi_snr_noise_bss_sta_ma_reset(wlc, bsscfg,
				CHSPEC_BANDUNIT(bsscfg->current_bss->chanspec),
				bsscfg->link->rssi, bsscfg->link->snr, lqi->noise);
		break;

	case IOV_GVAL(IOV_SNR_WINDOW_SZ):
		*ret_int_ptr = blqi->snr_window_sz;
		break;

	case IOV_SVAL(IOV_RSSI_DELTA):
		blqi->rssi_delta = (uint8)int_val;
		break;

	case IOV_GVAL(IOV_RSSI_DELTA):
		*ret_int_ptr = (int32)blqi->rssi_delta;
		break;

	case IOV_SVAL(IOV_SNR_DELTA):
		blqi->snr_delta = (uint8)int_val;
		break;

	case IOV_GVAL(IOV_SNR_DELTA):
		*ret_int_ptr = (int32)blqi->snr_delta;
		break;

	case IOV_SVAL(IOV_LQ_MAX_BCN_LOSS):
		blqi->max_lq_bcn_loss = int_val;	/* in ms */
		break;

	case IOV_GVAL(IOV_LQ_MAX_BCN_LOSS):
		*ret_int_ptr = blqi->max_lq_bcn_loss;	/* in ms */
		break;
#endif /* STA */

	case IOV_GVAL(IOV_SNR):
		*ret_int_ptr = bsscfg->link->snr;
		break;

	case IOV_GVAL(IOV_NOISE_LTE): {
		*ret_int_ptr = lqi->noise_lte;
		break;
	}

	case IOV_GVAL(IOV_NOISE_LTE_RESET): {
		uint8 i;
		lqi->noise_lte = WLC_NOISE_INVALID;
		for (i = 0; i < WLC_NOISE_LTE_SIZE; i++)
			lqi->noise_lte_values[i] = WLC_NOISE_INVALID;
		lqi->noise_lte_val_idx = 0;
		*ret_int_ptr = 0;
		break;
	}

	case IOV_GVAL(IOV_CHANIM_ENAB):
		*ret_int_ptr = (int32)WLC_CHANIM_ENAB(wlc->pub);
		break;

#ifdef WLCHANIM
	case IOV_GVAL(IOV_CHANIM_STATE): {
		chanspec_t chspec;
		wlc_chanim_stats_t *stats;

		if (!WLC_CHANIM_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}

		if (plen < (int)sizeof(int)) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		chspec = (chanspec_t) int_val;

		if (wf_chspec_malformed(chspec)) {
			err = BCME_BADCHAN;
			break;
		}

		stats = wlc_lq_chanim_chanspec_to_stats(wlc->chanim_info, chspec, FALSE);

		if (!stats) {
			err = BCME_RANGE;
			break;
		}

		if (WLC_CHANIM_MODE_EXT(wlc->chanim_info))
			*ret_int_ptr = (int32) chanim_mark(wlc->chanim_info).state;
		else
			*ret_int_ptr = (int32) wlc_lq_chanim_interfered(wlc, chspec);

		break;
	}

	case IOV_SVAL(IOV_CHANIM_STATE):
		if (!WLC_CHANIM_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		if (WLC_CHANIM_MODE_EXT(wlc->chanim_info))
			chanim_mark(wlc->chanim_info).state = (bool)int_val;
		break;

	case IOV_GVAL(IOV_CHANIM_MODE):
		if (!WLC_CHANIM_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		*ret_int_ptr = (int32)chanim_config(wlc->chanim_info).mode;
		break;

	case IOV_GVAL(IOV_SAMPLE_PERIOD):
		if (!WLC_CHANIM_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		*ret_int_ptr = (int32)chanim_config(wlc->chanim_info).sample_period;
		break;

	case IOV_SVAL(IOV_SAMPLE_PERIOD):
		if (!WLC_CHANIM_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		if (int_val < SAMPLE_PERIOD_MIN)
			err = BCME_RANGE;
		chanim_config(wlc->chanim_info).sample_period = (uint8)int_val;
		break;

	case IOV_SVAL(IOV_CHANIM_MODE):
		if (!WLC_CHANIM_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		if (int_val >= CHANIM_MODE_MAX) {
			err = BCME_RANGE;
			break;
		}

		chanim_config(wlc->chanim_info).mode = (uint8)int_val;
		break;

	case IOV_GVAL(IOV_CCASTATS_THRES):
		if (!WLC_CHANIM_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		*ret_int_ptr = (int32)chanim_config(wlc->chanim_info).ccastats_thres;
		break;

	case IOV_SVAL(IOV_CCASTATS_THRES):
		if (!WLC_CHANIM_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		chanim_config(wlc->chanim_info).ccastats_thres = (uint8)int_val;
		break;

	case IOV_GVAL(IOV_CRSGLITCH_THRES):
		if (!WLC_CHANIM_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		*ret_int_ptr = chanim_config(wlc->chanim_info).crsglitch_thres;
		break;

	case IOV_SVAL(IOV_CRSGLITCH_THRES):
		if (!WLC_CHANIM_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		chanim_config(wlc->chanim_info).crsglitch_thres = int_val;
		break;

	case IOV_GVAL(IOV_BGNOISE_THRES):
		if (!WLC_CHANIM_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		*ret_int_ptr = (int32)chanim_config(wlc->chanim_info).bgnoise_thres;
		break;

	case IOV_SVAL(IOV_BGNOISE_THRES):
		if (!WLC_CHANIM_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		chanim_config(wlc->chanim_info).bgnoise_thres = (int8)int_val;
		break;

	case IOV_GVAL(IOV_THRESHOLD_TIME):
		if (!WLC_CHANIM_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		*ret_int_ptr = (int32)chanim_config(wlc->chanim_info).threshold_time;
		break;

	case IOV_SVAL(IOV_THRESHOLD_TIME):
		if (!WLC_CHANIM_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		if (int_val < THRESHOLD_TIME_MIN)
			err = BCME_RANGE;
		chanim_config(wlc->chanim_info).threshold_time = (uint8)int_val;
		break;

	case IOV_GVAL(IOV_MAX_ACS):
		if (!WLC_CHANIM_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		*ret_int_ptr = (int32)chanim_config(wlc->chanim_info).max_acs;
		break;

	case IOV_SVAL(IOV_MAX_ACS):
		if (!WLC_CHANIM_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		if (int_val > CHANIM_ACS_RECORD)
			err = BCME_RANGE;
		chanim_config(wlc->chanim_info).max_acs = (uint8)int_val;
		break;

	case IOV_GVAL(IOV_LOCKOUT_PERIOD):
		if (!WLC_CHANIM_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		*ret_int_ptr = chanim_config(wlc->chanim_info).lockout_period;
		break;

	case IOV_SVAL(IOV_LOCKOUT_PERIOD):
		if (!WLC_CHANIM_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		chanim_config(wlc->chanim_info).lockout_period = int_val;
		break;

	case IOV_GVAL(IOV_ACS_RECORD):
		if (!WLC_CHANIM_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		if (alen < (int)sizeof(wl_acs_record_t))
			err = BCME_BUFTOOSHORT;
		else
			err = wlc_lq_chanim_get_acs_record(wlc->chanim_info, alen, a);
		break;

	case IOV_GVAL(IOV_CHANIM_STATS): {
		wl_chanim_stats_t input = *((wl_chanim_stats_t *)p);
		int buflen = (int)input.buflen;

		if (!WLC_CHANIM_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		if (input.count == WL_CHANIM_COUNT_ONE || input.count == WL_CHANIM_COUNT_ALL ||
			input.count == WL_CHANIM_READ_VERSION) {
			wl_chanim_stats_t *iob = (wl_chanim_stats_t*) a;

			if (input.count == WL_CHANIM_READ_VERSION) {
				iob->version = WL_CHANIM_STATS_VERSION;
				iob->count = 0;
				return err;
			}
			if ((uint)alen < WL_CHANIM_STATS_FIXED_LEN) {
				err = BCME_BUFTOOSHORT;
				break;
			}

			if ((uint)buflen < WL_CHANIM_STATS_FIXED_LEN) {
				err = BCME_BUFTOOSHORT;
				break;
			}

			err = wlc_lq_chanim_get_stats(wlc->chanim_info, iob, &buflen, input.count);
		}
#ifdef WLCHANIM_US
		else if (input.count == WL_CHANIM_COUNT_US_ONE ||
				input.count == WL_CHANIM_COUNT_US_ALL ||
				input.count == WL_CHANIM_COUNT_US_RESET ||
				input.count == WL_CHANIM_US_DUR ||
				input.count == WL_CHANIM_US_DUR_GET) {
			wl_chanim_stats_us_v2_t *iob = (wl_chanim_stats_us_v2_t*) a;
			wl_chanim_stats_us_v2_t input_us = *((wl_chanim_stats_us_v2_t *)p);
			if (input.count != WL_CHANIM_COUNT_US_RESET) {
				if ((uint)alen < WL_CHANIM_STATS_US_FIXED_LEN) {
					err = BCME_BUFTOOSHORT;
					break;
				}

				if ((uint)buflen < WL_CHANIM_STATS_US_FIXED_LEN) {
					err = BCME_BUFTOOSHORT;
					break;
				}
			}
			err = wlc_lq_chanim_get_stats_us(wlc->chanim_info, iob,
					&buflen, input.count, input_us.dur);
		}
#endif /* WLCHANIM_US */
		else {
			err = BCME_UNSUPPORTED;
		}
		break;
	}
#endif /* WLCHANIM */

	default:
		err = BCME_UNSUPPORTED;
	}

	return err;
}

static int
wlc_lq_doioctl(void *ctx, uint32 cmd, void *arg, uint len, struct wlc_if *wlcif)
{
	wlc_lq_info_t *lqi = (wlc_lq_info_t *)ctx;
	wlc_info_t *wlc = lqi->wlc;
	int *pval;
	int err = BCME_OK;
	wlc_bsscfg_t *cfg;

	/* update bsscfg pointer */
	cfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(cfg != NULL);

	/* default argument is generic integer */
	pval = (int *)arg;

	switch (cmd) {
	case WLC_GET_RSSI:
		if (len == sizeof(scb_val_t)) {
			scb_val_t *scb_val = (scb_val_t*)arg;
			struct scb *scb;

			/* Infra STA */
			if (BSSCFG_STA(cfg) && cfg->BSS) {
				if (cfg->associated) {
					scb_val->val = cfg->link->rssi;
				} else {
					scb_val->val = 0;
				}
				break;
			}

			/* User must provide a MAC address to use for AP and IBSS */
			if (ETHER_ISMULTI(&scb_val->ea) || ETHER_ISNULLADDR(&scb_val->ea)) {
				err = BCME_BADARG;
				break;
			}

			if ((scb = wlc_scbfind(wlc, cfg, &scb_val->ea)) == NULL) {
				err = BCME_NOTFOUND;
				break;
			}

			/* enable per-scb collection on first request */
			wlc_lq_sample_req_enab(scb, RX_LQ_SAMP_REQ_WLC, TRUE);
			scb_val->val = wlc_lq_rssi_ma(wlc, cfg, scb);
			break;
		}
		else if (len == sizeof(int32) &&
			BSSCFG_STA(cfg) && cfg->BSS) {
			/* User may have asked for cfg->link->rssi with an int sized buff */
			/* Infra STA */
			if (cfg->associated) {
					*pval = cfg->link->rssi;
				} else {
					*pval = 0;
				}
			break;
		}
		break;

	case WLC_GET_PHY_NOISE:
		*pval = wlc_lq_noise_ma_upd(wlc, phy_noise_avg(WLC_PI(wlc)));
		break;

#ifdef WLCQ
	case WLC_GET_CHANNEL_QA:
		*pval = lqi->channel_quality;
		break;

	case WLC_START_CHANNEL_QA:
		if (!wlc->pub->up) {
			err = BCME_NOTUP;
			break;
		}

		if (!WLCISNPHY(wlc->band)) {
			err = BCME_BADBAND;
			break;
		}

		err = wlc_lq_channel_qa_start(wlc);
		break;
#endif /* WLCQ */

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

static void
wlc_lq_watchdog(void *ctx)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;

	/* Get Noise Est from Phy */
	wlc_lq_noise_ma_upd(wlc, phy_noise_avg(WLC_PI(wlc)));
	wlc_lq_noise_lte_ma_upd(wlc, phy_noise_lte_avg(WLC_PI(wlc)));
}

/* init per-ant rssi sample windows */
static void
wlc_lq_ant_rssi_init(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct scb *scb, int8 rssi,
	wlc_rx_pkt_type_t type)
{
	wlc_lq_info_t *lqi = wlc->lqi;
	bss_lq_info_t *blqi = BSS_LQ_INFO(lqi, cfg);
	scb_lq_info_t *slqi = SCB_LQ_INFO(lqi, scb);
	uint k, window_sz;
	int8 *row;

	if (!PER_ANT_RSSI_SAMP(lqi, cfg)) {
		return;
	}

	for (k = WL_ANT_IDX_1; k < WL_RSSI_ANT_MAX; k ++) {
		if ((wlc->stf->rxchain & (1 << k)) == 0)
			continue;
		if (type == PKT_TYPE_DATA) {
			row = slqi->rssi_chain_window + k * MA_WIN_SZ(blqi->rssi_window_sz);
			window_sz = MA_WIN_SZ(blqi->rssi_window_sz);
			memset(row, rssi, window_sz);
		}
#ifdef STA
		else {
			row = slqi->rssi_chain_bcn_window + k * MA_WIN_SZ(blqi->rssi_bcn_window_sz);
			window_sz = MA_WIN_SZ(blqi->rssi_bcn_window_sz);
			memset(row, rssi, window_sz);
		}
#endif /* STA */
	}
	if (type == PKT_TYPE_DATA) {
		slqi->rssi_chain_index = 0;
	}
#ifdef STA
	else {
		slqi->rssi_chain_bcn_index = 0;
	}
#endif /* STA */
}

/* Reset samples windows */
void
wlc_lq_rssi_snr_noise_ma_reset(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct scb *scb,
	int8 rssi, uint8 snr, int8 noise)
{
	wlc_lq_info_t *lqi = wlc->lqi;
	bss_lq_info_t *blqi = BSS_LQ_INFO(lqi, cfg);
	scb_lq_info_t *slqi = SCB_LQ_INFO(lqi, scb);
	int i;
	wlc_link_qual_t *link = cfg->link;

	ASSERT(BSSCFG_STA(cfg) && cfg->BSS);

	/* rssi window */
	wlc_lq_rssi_ma_reset(wlc, cfg, scb, rssi);

	/* snr window */
	blqi->snr_tot = 0;
	blqi->snr_count = 0;

	for (i = 0; i < MA_WIN_SZ(blqi->snr_window_sz); i++) {
		slqi->snr_window[i] = snr;
	}
	slqi->snr_index = 0;

	link->snr = snr;

	/* noise */
	lqi->noise = noise;
	lqi->noise_lte = noise;
}

void
wlc_lq_rssi_snr_noise_bss_sta_ma_reset(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	int band, int8 rssi, uint8 snr, int8 noise)
{
	struct scb *scb;

	ASSERT(BSSCFG_STA(cfg) && cfg->BSS);

	if ((scb = wlc_scbfindband(wlc, cfg, &cfg->BSSID, band)) != NULL) {
		wlc_lq_rssi_snr_noise_ma_reset(wlc, cfg, scb, rssi, snr, noise);
	}
}

/* Reset RSSI sample window */
void
wlc_lq_rssi_ma_reset(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct scb *scb, int8 rssi)
{
	wlc_lq_info_t *lqi = wlc->lqi;
	bss_lq_info_t *blqi = BSS_LQ_INFO(lqi, cfg);
	scb_lq_info_t *slqi = SCB_LQ_INFO(lqi, scb);
	int i;
	wlc_link_qual_t *link = cfg->link;

	blqi->rssi_tot = 0;
	blqi->rssi_count = 0;

	/* rssi window - whole numbers */
	for (i = 0; i < MA_WIN_SZ(blqi->rssi_window_sz); i++) {
		slqi->rssi_window[i] = rssi;
	}
	/* rssi window - qdb numbers */
	if (PER_SCB_RSSI_QDB_SAMP(cfg)) {
		for (i = 0; i < MA_WIN_SZ(blqi->rssi_window_sz); i++) {
			blqi->rssi_qdb_window[i] = 0;
		}
	}
	slqi->rssi_index = 0;

	link->rssi = rssi;

	/* per-ant rssi window */
	wlc_lq_ant_rssi_init(wlc, cfg, scb, rssi, PKT_TYPE_DATA);
#ifdef STA
	/* per-ant bcn rssi window */
	wlc_lq_ant_rssi_init(wlc, cfg, scb, rssi, PKT_TYPE_BEACON);
#endif /* STA */
}

void
wlc_lq_rssi_bss_sta_ma_reset(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	int band, int8 rssi)
{
	struct scb *scb;

	ASSERT(BSSCFG_STA(cfg) && cfg->BSS);

	if ((scb = wlc_scbfindband(wlc, cfg, &cfg->BSSID, band)) != NULL) {
		wlc_lq_rssi_ma_reset(wlc, cfg, scb, rssi);
	}
}

static int8 BCMFASTPATH
wlc_lq_rssi_ma_upd(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct scb *scb,
	int8 rssi, uint8 qdb, bool ucast)
{
	wlc_lq_info_t *lqi = wlc->lqi;
	bss_lq_info_t *blqi = BSS_LQ_INFO(lqi, cfg);
	scb_lq_info_t *slqi = SCB_LQ_INFO(lqi, scb);
	wlc_link_qual_t *link = cfg->link;
	int rssi_qdb;

	ASSERT(BSSCFG_STA(cfg) && cfg->BSS);

	if (rssi != WLC_RSSI_INVALID) {
		bool admit_mcast_only = MA_ADMIT_MCAST_ONLY(blqi->rssi_window_sz);
		uint16 rssi_window_sz = MA_WIN_SZ(blqi->rssi_window_sz);

		/* rssi filtering with unicast when possible
		 * (count all before rssi window reaching full and no unicast for a while)
		 */
		if (MA_ADMIT_ALL_FRAME(blqi->rssi_window_sz) ||
		    (admit_mcast_only && !ucast) ||
		    (!admit_mcast_only && (ucast || !blqi->last_rssi_is_ucast))) {
			/* evict old value */
			if (blqi->rssi_count >= rssi_window_sz) {
				rssi_qdb = (int)slqi->rssi_window[slqi->rssi_index] << 2;
				if (blqi->rssi_qdb_window != NULL) {
					rssi_qdb += blqi->rssi_qdb_window[slqi->rssi_index];
				}
				blqi->rssi_tot -= rssi_qdb;
			}

			/* admit new value - combine rssi(in rssi) and rssi_qdb (in qdb) */
			slqi->rssi_window[slqi->rssi_index] = rssi;
			if (blqi->rssi_qdb_window != NULL) {
				blqi->rssi_qdb_window[slqi->rssi_index] = qdb;
			}
			slqi->rssi_index = MODINC_POW2(slqi->rssi_index, rssi_window_sz);

			/* average */
			rssi_qdb = (rssi << 2) + qdb;
			blqi->rssi_tot += rssi_qdb;
			if (blqi->rssi_count < rssi_window_sz) {
				blqi->rssi_count++;
			}
			rssi_qdb = blqi->rssi_tot / blqi->rssi_count;
			link->rssi_qdb = (uint8)(rssi_qdb & 3);
			link->rssi = (int8)(rssi_qdb >> 2);

			if (MA_WIN_DEBUG(blqi->rssi_window_sz)) {
				WL_PRINT(("rssi_flt(0x%04x:%d): new=%d(%c) -> rssi=%d\n",
					blqi->rssi_window_sz, blqi->rssi_count, rssi,
					(ucast ? 'U' : 'M'), link->rssi));
			}
		}

		blqi->last_rssi_is_ucast = ucast;
	}
	else if (blqi->rssi_count == 0) {
		link->rssi = WLC_RSSI_INVALID;
	}

	return link->rssi;
}

int8 BCMFASTPATH
wlc_lq_rssi_bss_sta_ma_upd_bcntrim(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	int band, int8 rssi, uint8 qdb)
{
	struct scb *scb;
	wlc_lq_info_t *lqi = wlc->lqi;
	bss_lq_info_t *blqi;

	ASSERT(BSSCFG_STA(cfg) && cfg->BSS);
	blqi = BSS_LQ_INFO(lqi, cfg);
	blqi->lq_last_bcn_time = OSL_SYSUPTIME();

	if ((scb = wlc_scbfindband(wlc, cfg, &cfg->BSSID, band)) != NULL) {
		return wlc_lq_rssi_ma_upd(wlc, cfg, scb, rssi, qdb, FALSE);
	}

	return WLC_RSSI_INVALID;
}

/* request to check rssi level and raise an host indication if needed */
void BCMFASTPATH
wlc_lq_rssi_bss_sta_event_upd(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	wlc_lq_info_t *lqi = wlc->lqi;
	bss_lq_info_t *blqi = BSS_LQ_INFO(lqi, cfg);
	int level;
	wlc_link_qual_t *link = cfg->link;

	ASSERT(BSSCFG_STA(cfg) && cfg->BSS);

	/* no update if timer active */
	if (blqi->rssi_event_timer_active)
		return;

	/* find rssi level */
	for (level = 0; level < blqi->rssi_event->num_rssi_levels; level++) {
		if (link->rssi <= blqi->rssi_event->rssi_levels[level])
			break;
	}

	if (level != blqi->rssi_level) {
		/* rssi level changed - post rssi event */
		wl_event_data_rssi_t value;
		value.rssi  = hton32(link->rssi);
		value.snr   = hton32(link->snr);
		value.noise = hton32(lqi->noise);
		blqi->rssi_level = (uint8)level;
		wlc_bss_mac_event(wlc, cfg, WLC_E_RSSI, NULL, 0, 0, 0, &value, sizeof(value));
		if (blqi->rssi_event_timer && blqi->rssi_event->rate_limit_msec) {
			/* rate limit rssi events */
			wl_del_timer(wlc->wl, blqi->rssi_event_timer);
			wl_add_timer(wlc->wl, blqi->rssi_event_timer,
				blqi->rssi_event->rate_limit_msec, FALSE);
		}
	}
}

static void
wlc_lq_rssi_event_timeout(void *arg)
{
	wlc_bsscfg_t *cfg = (wlc_bsscfg_t*)arg;
	wlc_info_t *wlc = cfg->wlc;
	wlc_lq_info_t *lqi = wlc->lqi;
	bss_lq_info_t *blqi = BSS_LQ_INFO(lqi, cfg);

	blqi->rssi_event_timer_active = FALSE;
}

#ifdef RSSI_MONITOR
void
wlc_lq_rssi_monitor_event(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_link_qual_t *link = cfg->link;
	wlc_rssi_monitor_t *rssi_monitor = &(link->rssi_monitor);

	if (rssi_monitor->flag & RSSI_MONITOR_EVT_SENT ||
	   !(rssi_monitor->flag & RSSI_MONITOR_ENABLED) ||
	   !link->rssi) {
		return;
	}

	if (link->rssi < rssi_monitor->min_rssi ||
	   link->rssi > rssi_monitor->max_rssi) {
		wl_rssi_monitor_evt_t event_data;
		event_data.version = RSSI_MONITOR_VERSION;
		event_data.cur_rssi = link->rssi;
		event_data.pad = 0;
		wlc_bss_mac_event(wlc, cfg, WLC_E_RSSI_LQM, &cfg->BSSID, 0, 0,
			0, &event_data, sizeof(event_data));
		rssi_monitor->flag |= RSSI_MONITOR_EVT_SENT;
	}
	return;
}
#endif /* RSSI_MONITOR */

/* Smooth SNR observation with an 8-point moving average
 * XXX - Ignore boundary conditions and sample age
 */
static uint8 BCMFASTPATH
wlc_lq_snr_ma_upd(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct scb *scb, uint8 snr, bool ucast)
{
	wlc_lq_info_t *lqi = wlc->lqi;
	bss_lq_info_t *blqi = BSS_LQ_INFO(lqi, cfg);
	scb_lq_info_t *slqi = SCB_LQ_INFO(lqi, scb);
	wlc_link_qual_t *link = cfg->link;

	ASSERT(BSSCFG_STA(cfg) && cfg->BSS);

	if (snr != WLC_SNR_INVALID) {
		bool admit_mcast_only = MA_ADMIT_MCAST_ONLY(blqi->snr_window_sz);
		uint16 snr_window_sz = MA_WIN_SZ(blqi->snr_window_sz);

		/* snr filtering with unicast when possible
		 * (count all before snr window reaching full and no unicast for a while)
		 */
		if (MA_ADMIT_ALL_FRAME(blqi->snr_window_sz) ||
		    (admit_mcast_only && !ucast) ||
		    (!admit_mcast_only && (ucast || !blqi->last_snr_is_ucast))) {
			/* evict old value */
			if (blqi->snr_count >= snr_window_sz) {
				blqi->snr_tot -= slqi->snr_window[slqi->snr_index];
			}

			/* admit new value */
			slqi->snr_window[slqi->snr_index] = snr;
			slqi->snr_index = MODINC_POW2(slqi->snr_index, snr_window_sz);

			blqi->snr_tot += snr;
			if (blqi->snr_count < snr_window_sz) {
				blqi->snr_count++;
			}
			link->snr = blqi->snr_tot / blqi->snr_count;

			if (MA_WIN_DEBUG(blqi->snr_window_sz)) {
				WL_PRINT(("snr_flt(0x%04x:%d): new=%d(%c) -> snr=%d\n",
					blqi->snr_window_sz, blqi->snr_count, snr,
					(ucast ? 'U' : 'M'), link->snr));
			}
		}

		blqi->last_snr_is_ucast = ucast;
	}
	else if (blqi->snr_count == 0) {
		link->snr = WLC_SNR_INVALID;
	}

	return link->snr;
}

#ifdef WLMCNX
static void wlc_lq_update_tbtt(void *ctx, wlc_mcnx_intr_data_t *notif_data)
{
	wlc_info_t *wlc = notif_data->cfg->wlc;
	wlc_bsscfg_t *cfg = notif_data->cfg;
	wlc_lq_info_t *lqi = wlc->lqi;
	bss_lq_info_t *blqi = BSS_LQ_INFO(lqi, cfg);
	wlc_link_qual_t *link = cfg->link;
	int rssi = WLC_RSSI_INVALID, snr = WLC_SNR_INVALID;
	uint32 now = OSL_SYSUPTIME();
	int band;
	struct scb *scb;

	/* Here, only infrastructure STA is recognized */
	if (!(BSSCFG_STA(cfg) && cfg->BSS))
		return;

	/* Here, only TBTT interrupt is recognized */
	if (notif_data->intr != M_P2P_I_PRE_TBTT)
		return;

	/* Skip beacon loss checking if away from home channel */
	if (WLC_BAND_PI_RADIO_CHANSPEC != cfg->current_bss->chanspec) {
#ifdef EVENT_LOG_COMPILE
		EVENT_LOG(EVENT_LOG_TAG_BEACON_LOG, "Beacon(tbtt): away (SCAN=%d)",
			SCAN_IN_PROGRESS(wlc->scan));
#endif // endif
		/* When away from home, this beacon period is deducted from
		 * the beacon loss time by adjusting lq_last_bcn_time forward.
		 * (approximately adjust time in ms with beacon period in TU)
		 */
		blqi->lq_last_bcn_time += cfg->current_bss->beacon_period;
		if ((int)(blqi->lq_last_bcn_time - now) > 0)
			blqi->lq_last_bcn_time = now;
		return;
	}

	/* Check for consecutive beacon loss */
#ifdef EVENT_LOG_COMPILE
	EVENT_LOG(EVENT_LOG_TAG_BEACON_LOG, "Beacon(tbtt): home");
#endif // endif
	if ((blqi->max_lq_bcn_loss == 0) ||
	    ((int)(now - blqi->lq_last_bcn_time) < blqi->max_lq_bcn_loss))
		return;

	band = CHSPEC_BANDUNIT(cfg->current_bss->chanspec);
	if ((scb = wlc_scbfindband(wlc, cfg, &cfg->BSSID, band)) == NULL) {
		return;
	}

	/* By now, we have missed enough beacons. Need to discount RSSI/SNR */
	if ((blqi->rssi_delta != 0) && (link->rssi != WLC_RSSI_INVALID)) {
		rssi = link->rssi - blqi->rssi_delta;

		/* Do not reduce it beyond WLC_RSSI_MINVAL or noise floor */
		if (rssi < WLC_RSSI_MINVAL)
			rssi = WLC_RSSI_MINVAL;
		if ((lqi->noise != WLC_NOISE_INVALID) && (rssi < lqi->noise))
			rssi = lqi->noise;

#ifdef EVENT_LOG_COMPILE
		EVENT_LOG(EVENT_LOG_TAG_BEACON_LOG,
			"Apply rssi_delta: last beacon = %08x now = %08x\n",
			blqi->lq_last_bcn_time, now);
#endif // endif
		wlc_lq_rssi_ma_upd(wlc, cfg, scb, (int8)rssi, 0, FALSE);
		cfg->current_bss->RSSI = (int16)link->rssi;
	}

	if ((blqi->snr_delta != 0) && (link->snr != WLC_SNR_INVALID)) {
		snr = link->snr - blqi->snr_delta;
		if (snr < WLC_SNR_MINVAL)
			snr = WLC_SNR_MINVAL;

#ifdef EVENT_LOG_COMPILE
		EVENT_LOG(EVENT_LOG_TAG_BEACON_LOG,
			"Apply snr_delta: last beacon = %08x now = %08x\n",
			blqi->lq_last_bcn_time, now);
#endif // endif
		wlc_lq_snr_ma_upd(wlc, cfg, scb, (uint8)snr, FALSE);
		cfg->current_bss->SNR = (int16)link->snr;
	}
#ifdef EVENT_LOG_COMPILE
	EVENT_LOG(EVENT_LOG_TAG_BEACON_LOG, "Beacon(miss): RSSI=%d(avg:%d) SNR=%d(avg:%d)",
		rssi, link->rssi, snr, link->snr);
#endif // endif
}
#endif /* WLMCNX */

int8
wlc_lq_noise_ma_upd(wlc_info_t *wlc, int8 noise)
{
	wlc_lq_info_t *lqi = wlc->lqi;

	/* Asymmetric noise floor filter:
	 *	Going up slowly by only +1
	 *	Coming down faster by diff/2
	 */
	if (noise != WLC_NOISE_INVALID) {
		if (lqi->noise == WLC_NOISE_INVALID)
			lqi->noise = noise;
		else if (noise > lqi->noise)
			lqi->noise ++;
		else if (noise < lqi->noise)
			lqi->noise += (noise - lqi->noise - 1) / 2;

#ifdef CCA_STATS
		if (WL_CCA_STATS_ENAB(wlc->pub) &&
		    wlc_cca_chan_qual_event_update(wlc, WL_CHAN_QUAL_NF, lqi->noise)) {
			cca_chan_qual_event_t event_output;

			event_output.status = 0;
			event_output.id = WL_CHAN_QUAL_NF;
			event_output.chanspec = wlc->chanspec;
			event_output.len = sizeof(event_output.noise);
			event_output.noise = lqi->noise;

			wlc_bss_mac_event(wlc, NULL, WLC_E_CCA_CHAN_QUAL, NULL,
				0, 0, 0, &event_output, sizeof(event_output));
		}
#endif /* CCA_STATS */
	}

	return lqi->noise;
}

static
int wlc_lq_noise_lte_median(int* values)
{
	int noise_val[WLC_NOISE_LTE_SIZE], temp;
	uint8 i, j;
	uint8 len = WLC_NOISE_LTE_SIZE;
	uint8 median_idx = len/2;

	for (i = 0; i < len; i++)
		noise_val[i] = values[i];

	for (i = 0; i < len-1; i++) {
		if (i > median_idx)
			break;
		for (j = i+1; j < len; j++) {
			temp = MAX(noise_val[i], noise_val[j]);
			noise_val[j] = MIN(noise_val[i], noise_val[j]);
			noise_val[i] = temp;
		}
	}
	return noise_val[median_idx];
}

int8
wlc_lq_noise_lte_ma_upd(wlc_info_t *wlc, int8 noise)
{
	wlc_lq_info_t *lqi = wlc->lqi;

	if (noise != WLC_NOISE_INVALID) {
		lqi->noise_lte_values[lqi->noise_lte_val_idx] = noise;
		lqi->noise_lte_val_idx++;
		if (lqi->noise_lte_val_idx >= WLC_NOISE_LTE_SIZE)
			lqi->noise_lte_val_idx = 0;
		lqi->noise_lte = (int8)wlc_lq_noise_lte_median(lqi->noise_lte_values);
		if (lqi->noise_lte == WLC_NOISE_INVALID)
			lqi->noise_lte = noise;
		if (lqi->noise_lte < PHY_NOISE_FIXED_VAL)
			lqi->noise_lte = PHY_NOISE_FIXED_VAL;

#ifdef CCA_STATS
		if (WL_CCA_STATS_ENAB(wlc->pub) &&
		    wlc_cca_chan_qual_event_update(wlc, WL_CHAN_QUAL_NF_LTE, lqi->noise_lte)) {
			cca_chan_qual_event_t event_output;

			event_output.status = 0;
			event_output.id = WL_CHAN_QUAL_NF_LTE;
			event_output.chanspec = wlc->chanspec;
			event_output.len = sizeof(event_output.noise);
			event_output.noise = lqi->noise_lte;

			wlc_bss_mac_event(wlc, NULL, WLC_E_CCA_CHAN_QUAL, NULL,
				0, 0, 0, &event_output, sizeof(event_output));
		}
#endif /* CCA_STATS */
	}

	return lqi->noise_lte;
}

/*
This function returns SNR during the recently received frame.
SNR is computed by PHY during frame reception and keeps it in the
D11 frame header. This function reads that value from the D11 frame header
and returns it.
For CCK frame SNR in the D11 frame is in dB in Q.2 format.
For OFDM frames SNR in the D11 frame is 9.30139866 * SNR dB in Q.0 format.
This function returns the SNR for both the frames in dB in Q.0 format.
Brief documentation is available about signalQuality parameter in D11 frame is
available at http://hwnbu-twiki.broadcom.com/bin/view/Mwgroup/MAC-PhyInterface.
*/
uint8 BCMFASTPATH
wlc_lq_recv_snr_compute(wlc_info_t *wlc, int8 rssi, int8 noise)
{
	uint8 snr = WLC_SNR_INVALID;

	if (rssi == WLC_RSSI_INVALID) {
		return snr;
	}

	if (WLCISACPHY(wlc->band) || WLCISLCN20PHY(wlc->band)) {
		if (!noise)
			noise = WLC_NOISE_EXCELLENT;
		snr = rssi - noise;

		/* Backup just in case Noise Est is incorrect */
		if ((int8)snr < 0)
			snr = 3;

		if (snr >= WLC_SNR_EXCELLENT_11AC) {
			snr = WLC_SNR_EXCELLENT_11AC;
		}

		WL_INFORM(("snr=%d, rssi=%d, noise=%d\n", snr, rssi, noise));
	}

	/* return SNR */
	return snr;
}

#if defined(BCMDBG) || defined(WLTEST)

static int
wlc_lq_dump(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	int idx;
	wlc_bsscfg_t *cfg;
	wlc_lq_info_t *lqi = wlc->lqi;

#if defined(WL_MONITOR) && defined(WLTEST)
	wlc_lq_monitor_dump(wlc, b);
#endif // endif

	FOREACH_BSS(wlc, idx, cfg) {
		bcm_bprintf(b, "Link Quality - bsscfg: %d\n", WLC_BSSCFG_IDX(cfg));
		wlc_lq_bss_dump(lqi, cfg, b);
		bcm_bprintf(b, "\n");
	}

	return BCME_OK;
}
#endif // endif

/* Use simple averaging algo for now until there is need to optimize the perfermance,
 * then we can go back to the STA's way doing it.
 */
int8
wlc_lq_rssi_ma(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct scb *scb)
{
	wlc_lq_info_t *lqi = wlc->lqi;
	bss_lq_info_t *blqi = BSS_LQ_INFO(lqi, cfg);
	scb_lq_info_t *slqi = SCB_LQ_INFO(lqi, scb);
	int rssi = WLC_RSSI_INVALID, cnt;
	uint i;

	for (i = 0, cnt = 0; i < MA_WIN_SZ(blqi->rssi_window_sz); i++) {
		if (slqi->rssi_window[i] != WLC_RSSI_INVALID) {
			rssi += slqi->rssi_window[i];
			cnt++;
		}
	}
	if (cnt > 1) {
		rssi /= cnt;
	}

	return (int8)rssi;
}

/* query rssi moving average */
int8
wlc_lq_rssi_get(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct scb *scb)
{
	if (PER_SCB_RSSI_MOVE_AVG(cfg)) {
		return cfg->link->rssi;
	}
	else {
		return wlc_lq_rssi_ma(wlc, cfg, scb);
	}
}

/* query the last received packet's rssi */
int8
wlc_lq_rssi_last_get(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct scb *scb)
{
	wlc_lq_info_t *lqi = wlc->lqi;
	bss_lq_info_t *blqi = BSS_LQ_INFO(lqi, cfg);
	scb_lq_info_t *slqi = SCB_LQ_INFO(lqi, scb);
	uint i;

	i = MODDEC_POW2(slqi->rssi_index, MA_WIN_SZ(blqi->rssi_window_sz));

	return slqi->rssi_window[i];
}

/* query the mac received packet's rssi (withinthe current movign window) */
int8
wlc_lq_rssi_max_get(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct scb *scb)
{
	wlc_lq_info_t *lqi = wlc->lqi;
	bss_lq_info_t *blqi = BSS_LQ_INFO(lqi, cfg);
	scb_lq_info_t *slqi = SCB_LQ_INFO(lqi, scb);
	int8 rssi = WLC_RSSI_INVALID;
	uint i;

	for (i = 0; i < MA_WIN_SZ(blqi->rssi_window_sz); i++) {
		if (rssi == WLC_RSSI_INVALID) {
			rssi = slqi->rssi_window[i];
		} else if (slqi->rssi_window[i] != WLC_RSSI_INVALID) {
			rssi = (rssi >= slqi->rssi_window[i]) ? rssi : slqi->rssi_window[i];
		}
	}

	return rssi;
}

/* query per-ant rssi moving average */
int8
wlc_lq_ant_rssi_get(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct scb *scb, int chain)
{
	wlc_lq_info_t *lqi = wlc->lqi;
	bss_lq_info_t *blqi = BSS_LQ_INFO(lqi, cfg);
	scb_lq_info_t *slqi = SCB_LQ_INFO(lqi, scb);
	int rssi = WLC_RSSI_INVALID;
	int cnt;
	uint i;
	int8 *row;

	if (!PER_ANT_RSSI_SAMP(lqi, cfg)) {
		return wlc_lq_rssi_get(wlc, cfg, scb);
	}

	ASSERT((chain >= WL_ANT_IDX_1) && (chain < WL_RSSI_ANT_MAX));

	if ((wlc->stf->rxchain & (1 << chain))) {
		row = slqi->rssi_chain_window + chain * MA_WIN_SZ(blqi->rssi_window_sz);
		for (i = 0, cnt = 0; i < MA_WIN_SZ(blqi->rssi_window_sz); i++) {
			if (*(row + i) != WLC_RSSI_INVALID) {
				rssi += *(row + i);
				cnt++;
			}
		}
		if (cnt > 1) {
			rssi /= cnt;
		}
	}

	return (int8)rssi;
}

/* return the rssi of last received packet per scb and
 * per antenna chain.
 */
int8
wlc_lq_ant_rssi_last_get(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct scb *scb, int chain)
{
	wlc_lq_info_t *lqi = wlc->lqi;
	bss_lq_info_t *blqi = BSS_LQ_INFO(lqi, cfg);
	scb_lq_info_t *slqi = SCB_LQ_INFO(lqi, scb);
	int8 rssi = WLC_RSSI_INVALID;
	uint i;
	int8 *row;

	if (!PER_ANT_RSSI_SAMP(lqi, cfg)) {
		return wlc_lq_rssi_last_get(wlc, cfg, scb);
	}

	i = MODDEC_POW2(slqi->rssi_index, MA_WIN_SZ(blqi->rssi_window_sz));

	ASSERT((chain >= WL_ANT_IDX_1) && (chain < WL_RSSI_ANT_MAX));

	if ((wlc->stf->rxchain & (1 << chain))) {
		row = slqi->rssi_chain_window + chain * MA_WIN_SZ(blqi->rssi_window_sz);
		if (*(row + i) != WLC_RSSI_INVALID) {
			rssi = *(row + i);
		}
	}

	return rssi;
}

/* return the maximum rssi of past received packet (within the moving window)
 * per scb and per antenna chain.
 */
int8
wlc_lq_ant_rssi_max_get(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct scb *scb, int chain)
{
	wlc_lq_info_t *lqi = wlc->lqi;
	bss_lq_info_t *blqi = BSS_LQ_INFO(lqi, cfg);
	scb_lq_info_t *slqi = SCB_LQ_INFO(lqi, scb);
	int8 rssi = WLC_RSSI_INVALID;
	uint i;
	int8 *row;

	if (!PER_ANT_RSSI_SAMP(lqi, cfg)) {
		return wlc_lq_rssi_max_get(wlc, cfg, scb);
	}

	ASSERT((chain >= WL_ANT_IDX_1) && (chain < WL_RSSI_ANT_MAX));

	if ((wlc->stf->rxchain & (1 << chain))) {
		row = slqi->rssi_chain_window + chain * MA_WIN_SZ(blqi->rssi_window_sz);
		for (i = 0; i < MA_WIN_SZ(blqi->rssi_bcn_window_sz); i++) {
			if (rssi == WLC_RSSI_INVALID) {
				rssi = *(row + i);
			} else if (*(row + i) != WLC_RSSI_INVALID) {
				rssi = (rssi >= *(row + i)) ? rssi : *(row + i);
			}
		}
	}

	return rssi;
}

int8
wlc_lq_ant_bcn_rssi_max_get(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct scb *scb, int chain)
{
	wlc_lq_info_t *lqi = wlc->lqi;
	bss_lq_info_t *blqi = BSS_LQ_INFO(lqi, cfg);
	scb_lq_info_t *slqi = SCB_LQ_INFO(lqi, scb);
	int8 rssi = WLC_RSSI_INVALID;
	uint i;
	int8 *row;

	if (!PER_ANT_BCN_RSSI_SAMP(lqi, cfg)) {
		return WLC_RSSI_INVALID;
	}

	ASSERT((chain >= WL_ANT_IDX_1) && (chain < WL_RSSI_ANT_MAX));

	if ((wlc->stf->rxchain & (1 << chain)) == 0) {
		row = slqi->rssi_chain_bcn_window + chain * MA_WIN_SZ(blqi->rssi_bcn_window_sz);
		for (i = 0; i < MA_WIN_SZ(blqi->rssi_bcn_window_sz); i++) {
			if (rssi == WLC_RSSI_INVALID) {
				rssi = *(row + i);
			} else if (*(row + i) != WLC_RSSI_INVALID) {
				rssi = (rssi >= *(row + i)) ? rssi : *(row + i);
			}
		}
	}

	return (int8)rssi;
}

/** Enable or disable RSSI update for a particular requestor module */
void
wlc_lq_sample_req_enab(struct scb *scb, rx_lq_samp_req_t rid, bool enab)
{
	if (enab) {
		scb->rx_lq_samp_req |= (1<<rid);
	} else {
		scb->rx_lq_samp_req &= ~(1<<rid);
	}
}

/* add rssi sample into window */
static void BCMFASTPATH
wlc_lq_rssi_sample(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct scb *scb, int8 rssi)
{
	wlc_lq_info_t *lqi = wlc->lqi;
	bss_lq_info_t *blqi = BSS_LQ_INFO(lqi, cfg);
	scb_lq_info_t *slqi = SCB_LQ_INFO(lqi, scb);

	slqi->rssi_window[slqi->rssi_index] = rssi;

	slqi->rssi_index = MODINC_POW2(slqi->rssi_index, MA_WIN_SZ(blqi->rssi_window_sz));
}

/* add per-ant rssi sample into window */
static void BCMFASTPATH
wlc_lq_ant_rssi_sample(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct scb *scb, wlc_d11rxhdr_t *wrxh)
{
	wlc_lq_info_t *lqi = wlc->lqi;
	bss_lq_info_t *blqi = BSS_LQ_INFO(lqi, cfg);
	scb_lq_info_t *slqi = SCB_LQ_INFO(lqi, scb);
	uint i, k;
	int8 *row;

	i = slqi->rssi_chain_index;

	for (k = WL_ANT_IDX_1; k < WL_RSSI_ANT_MAX; k ++) {
		row = slqi->rssi_chain_window + k * MA_WIN_SZ(blqi->rssi_window_sz);
		if ((wlc->stf->rxchain & (1 << k)) == 0) {
			*(row + i) = WLC_RSSI_INVALID;
		} else {
			*(row + i) = wrxh->rxpwr[k];
		}
	}

	slqi->rssi_chain_index =
		MODINC_POW2(slqi->rssi_chain_index, MA_WIN_SZ(blqi->rssi_window_sz));
}

#ifdef STA
/* add per-ant bcn rssi sample into window */
/* Elnabypass uses bcn rssi values added into separate windows */
static void
wlc_lq_ant_bcn_rssi_sample(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct scb *scb,
	wlc_d11rxhdr_t *wrxh)
{
	wlc_lq_info_t *lqi = wlc->lqi;
	bss_lq_info_t *blqi = BSS_LQ_INFO(lqi, cfg);
	scb_lq_info_t *slqi = SCB_LQ_INFO(lqi, scb);
	uint i, k;
	int8 *row;

	i = slqi->rssi_chain_bcn_index;

	for (k = WL_ANT_IDX_1; k < WL_RSSI_ANT_MAX; k ++) {
		if ((wlc->stf->rxchain & (1 << k)) == 0)
			continue;
		row = slqi->rssi_chain_bcn_window + k * MA_WIN_SZ(blqi->rssi_bcn_window_sz);
		*(row + i) = wrxh->rxpwr[k];
	}

	slqi->rssi_chain_bcn_index =
	        MODINC_POW2(slqi->rssi_chain_bcn_index, MA_WIN_SZ(blqi->rssi_bcn_window_sz));
}
#endif /* STA */

/* add snr sample into window */
static void BCMFASTPATH
wlc_lq_snr_sample(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct scb *scb, uint8 snr)
{
	wlc_lq_info_t *lqi = wlc->lqi;
	bss_lq_info_t *blqi = BSS_LQ_INFO(lqi, cfg);
	scb_lq_info_t *slqi = SCB_LQ_INFO(lqi, scb);

	slqi->snr_window[slqi->snr_index] = (uint8)snr;

	slqi->snr_index = MODINC_POW2(slqi->snr_index, MA_WIN_SZ(blqi->snr_window_sz));
}

/* add rssi/snr/per-ant rssi/etc. into windows */
void BCMFASTPATH
wlc_lq_sample(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct scb *scb,
	wlc_d11rxhdr_t *wrxh, bool ucast, rx_lq_samp_t *samp)
{
	wlc_lq_info_t *lqi = wlc->lqi;
	int8 rssi = wrxh->rssi, rssi_avg = WLC_RSSI_INVALID;
	uint8 snr, snr_avg = WLC_SNR_INVALID;

	/* rssi */
	if (PER_SCB_RSSI_SAMP(cfg) && (rssi != WLC_RSSI_INVALID)) {
		/* collect the sample and calculate per-scb rssi moving average */
		if (PER_SCB_RSSI_MOVE_AVG(cfg) && SCB_ASSOCIATED(scb)) {
			rssi_avg = wlc_lq_rssi_ma_upd(wlc, cfg, scb, rssi, 0, ucast);
		}
		/* collect per-scb rssi samples */
		else {
			wlc_lq_rssi_sample(wlc, cfg, scb, rssi);
		}
		/* collect per-scb per-ant rssi samples */
		if (PER_ANT_RSSI_SAMP(lqi, cfg)) {
			wlc_lq_ant_rssi_sample(wlc, cfg, scb, wrxh);
		}
#if defined(STA) && defined(DBG_BCN_LOSS)
		scb->dbg_bcn.last_rx_rssi = rssi;
#endif // endif
	}

	/* snr */
	snr = wlc_lq_recv_snr_compute(wlc, rssi, lqi->noise);
	if (PER_SCB_SNR_SAMP(cfg) && (snr != WLC_SNR_INVALID)) {
		/* collect the sample & calculate per-scb snr moving average */
		if (PER_SCB_SNR_MOVE_AVG(cfg) && SCB_ASSOCIATED(scb)) {
			snr_avg = wlc_lq_snr_ma_upd(wlc, cfg, scb, snr, ucast);
		}
		/* collect per-scb snr samples */
		else {
			wlc_lq_snr_sample(wlc, cfg, scb, snr);
		}
	}

	samp->rssi = rssi;
	samp->rssi_avg = rssi_avg;
	samp->snr = snr;
	samp->snr_avg = snr_avg;
}

#if defined(WL_MONITOR) && defined(WLTEST)
/* add rssi sample into window - special for mfgtest */
void BCMFASTPATH
wlc_lq_monitor_sample(wlc_info_t *wlc, wlc_d11rxhdr_t *wrxh,
	rx_lq_samp_t *samp)
{
	if (wrxh->rssi != WLC_RSSI_INVALID) {
		wlc_lq_info_t *lqi = wlc->lqi;
		monitor_rssi_win_t *win = lqi->monitor;
		uint k;
		int8 *row;

		for (k = WL_ANT_IDX_1; k < WL_RSSI_ANT_MAX; k ++) {
			row = win->rssi_chain_window + k * MA_WIN_SZ(win->rssi_chain_window_sz);
			if ((wlc->stf->rxchain & (1 << k)) == 0) {
				*(row + win->rssi_chain_index) = WLC_RSSI_INVALID;
			} else {
				*(row + win->rssi_chain_index) = wrxh->rxpwr[k];
			}
		}

		win->rssi_chain_index =
		        MODINC_POW2(win->rssi_chain_index, MA_WIN_SZ(win->rssi_chain_window_sz));
	}

	samp->rssi = wrxh->rssi;
	samp->rssi_avg = WLC_RSSI_INVALID;
	samp->snr = WLC_SNR_INVALID;
	samp->snr_avg = WLC_SNR_INVALID;
}

static int8
wlc_lq_ant_rssi_monitor_get(wlc_info_t *wlc, int chain)
{
	wlc_lq_info_t *lqi = wlc->lqi;
	monitor_rssi_win_t *monitor = lqi->monitor;
	int rssi = WLC_RSSI_INVALID;
	int cnt;
	uint i;
	int8 *row;

	ASSERT((chain >= WL_ANT_IDX_1) && (chain < WL_RSSI_ANT_MAX));

	if ((wlc->stf->rxchain & (1 << chain))) {
		row = monitor->rssi_chain_window + chain * MA_WIN_SZ(monitor->rssi_chain_window_sz);
		for (i = 0, cnt = 0; i < MA_WIN_SZ(monitor->rssi_chain_window_sz); i++) {
			if (*(row + i) != WLC_RSSI_INVALID) {
				rssi += *(row + i);
				cnt++;
			}
		}
		if (cnt > 1) {
			rssi /= cnt;
		}
	}

	return (int8)rssi;
}

#if defined(BCMDBG) || defined(WLTEST)
static void
wlc_lq_monitor_dump(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	monitor_rssi_win_t *monitor = wlc->lqi->monitor;
	uint8 i, k;
	uint idx;
	int8 *row;
	uint rssi_window_sz = monitor->rssi_chain_window_sz;

	for (k = WL_ANT_IDX_1; k < WL_RSSI_ANT_MAX; k++) {
		if ((wlc->stf->rxchain & (1 << k)) == 0)
			continue;
		row = monitor->rssi_chain_window + k * rssi_window_sz;
		bcm_bprintf(b, "     ANT%d: [ ", k);
		idx = monitor->rssi_chain_index;
		for (i = 0; i < rssi_window_sz; i ++) {
			bcm_bprintf(b, "%3d ", *(row + idx));
			idx = MODINC_POW2(idx, rssi_window_sz);
		}
		/* XXX new format but can't do without breaking the UTF.
		 * bcm_bprintf(b, "] AVG: %3d\n",
		 *	wlc_lq_ant_rssi_get(wlc, cfg, scb, k));
		 */
		bcm_bprintf(b, "] AVG [%d]\n",
		            wlc_lq_ant_rssi_monitor_get(wlc, k));
	}
}
#endif // endif
#endif /* WL_MONITOR && WLTEST */

/* bsscfg cubby */
static int
wlc_lq_bss_init(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_lq_info_t *lqi = (wlc_lq_info_t *)ctx;
	wlc_info_t *wlc = lqi->wlc;
	bss_lq_info_t **pblqi = BSS_LQ_INFO_LOC(lqi, cfg);
	bss_lq_info_t *blqi = BSS_LQ_INFO(lqi, cfg);
	uint16 rssi_window_sz, rssi_bcn_window_sz;
	uint extra;

	/* sanity check */
	ASSERT(blqi == NULL);

	rssi_window_sz = BSS_MA_WINDOW_SZ(lqi, cfg);
	rssi_bcn_window_sz = lqi->sta_bcn_window_sz;

	/* allocate rssi qdb sample window, wl_rssi_event_t, and wlc_link_qual_t
	 * after bss_lq_info_t.
	 */
	extra = 0;
	if (PER_SCB_RSSI_QDB_SAMP(cfg)) {
		extra += rssi_window_sz;	/* rssi_qdb_window */
	}
	extra += sizeof(wl_rssi_event_t) + sizeof(wlc_link_qual_t);

	if ((blqi = MALLOCZ(lqi->osh, sizeof(*blqi) + extra)) == NULL) {
		WL_ERROR(("wl%d: %s: mem alloc failed, allocated %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(lqi->osh)));
		return BCME_NOMEM;
	}
	extra = 0;
	if (PER_SCB_RSSI_QDB_SAMP(cfg)) {
		blqi->rssi_qdb_window = (uint8 *)&blqi[1];
		extra += rssi_window_sz;	/* rssi_qdb_window */
	}
	blqi->rssi_event = (wl_rssi_event_t *)((uint8 *)&blqi[1] + extra);
	extra += sizeof(wl_rssi_event_t);
	cfg->link = (wlc_link_qual_t *)((uint8 *)&blqi[1] + extra);
	extra += sizeof(wlc_link_qual_t);

	/* rssi & snr sampling sizes */
	blqi->rssi_window_sz = rssi_window_sz;
	blqi->snr_window_sz = rssi_window_sz;
	blqi->rssi_bcn_window_sz = rssi_bcn_window_sz;

	*pblqi = blqi;

	BCM_REFERENCE(wlc);
	return BCME_OK;
}

static void
wlc_lq_bss_deinit(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_lq_info_t *lqi = (wlc_lq_info_t *)ctx;
	wlc_info_t *wlc = lqi->wlc;
	bss_lq_info_t **pblqi = BSS_LQ_INFO_LOC(lqi, cfg);
	bss_lq_info_t *blqi = BSS_LQ_INFO(lqi, cfg);
	uint16 rssi_window_sz;
	uint extra;

	if (blqi == NULL) {
		return;
	}

	if (blqi->rssi_event_timer) {
		wl_del_timer(wlc->wl, blqi->rssi_event_timer);
		wl_free_timer(wlc->wl, blqi->rssi_event_timer);
		blqi->rssi_event_timer_active = FALSE;
	}

	rssi_window_sz = BSS_MA_WINDOW_SZ(lqi, cfg);

	extra = 0;
	if (PER_SCB_RSSI_QDB_SAMP(cfg)) {
		extra += rssi_window_sz;	/* rssi_qdb_window */
	}
	extra += sizeof(wl_rssi_event_t) + sizeof(wlc_link_qual_t);

	MFREE(lqi->osh, blqi, sizeof(*blqi) + extra);
	cfg->link = NULL;

	*pblqi = NULL;
}

#if defined(BCMDBG) || defined(WLTEST)
static void
wlc_lq_bss_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b)
{
	wlc_lq_info_t *lqi = (wlc_lq_info_t *)ctx;
	wlc_info_t *wlc = lqi->wlc;
	bss_lq_info_t *blqi;
	scb_lq_info_t *slqi;
	struct scb *scb;
	struct scb_iter scbiter;
	uint i;
	uint idx;
	char eabuf[ETHER_ADDR_STR_LEN];

	/* per-scb data */
	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {
		bcm_bprintf(b, "  SCB: %s BAND: %d\n",
			bcm_ether_ntoa(&scb->ea, eabuf), scb->bandunit);
		wlc_lq_scb_dump(wlc->lqi, scb, b);
	}

	/* dump the folllowing info only for infra STA */
	if (!BSSCFG_STA(cfg) || !cfg->BSS || !cfg->associated)
		return;
	if ((scb = wlc_scbfindband(wlc, cfg, &cfg->BSSID,
			CHSPEC_BANDUNIT(cfg->current_bss->chanspec))) == NULL)
		return;

	/* average */
	bcm_bprintf(b, "  RSSI: %d SNR: %d BAND: %d\n",
		cfg->link->rssi, cfg->link->snr,
		CHSPEC_BANDUNIT(cfg->current_bss->chanspec));

	blqi = BSS_LQ_INFO(lqi, cfg);
	slqi = SCB_LQ_INFO(lqi, scb);

	/* RSSI QDB */
	if (PER_SCB_RSSI_QDB_SAMP(cfg)) {
		bcm_bprintf(b, "     QDB: [ ");
		for (i = 0, idx = slqi->rssi_index;
		     i < MA_WIN_SZ(blqi->rssi_window_sz);
		     i++, idx = MODINC_POW2(idx, MA_WIN_SZ(blqi->rssi_window_sz))) {
			bcm_bprintf(b, "%3d ", blqi->rssi_qdb_window[idx]);
		}
		bcm_bprintf(b, "]\n");
	}
}
#endif // endif

/* scb cubby */
static uint
wlc_lq_scb_secsz(void *ctx, struct scb *scb)
{
	wlc_lq_info_t *lqi = (wlc_lq_info_t *)ctx;
	wlc_bsscfg_t *cfg = SCB_BSSCFG(scb);
	uint16 rssi_window_sz, snr_window_sz;
	uint extra;
#ifdef STA
	uint16 rssi_bcn_window_sz;
#endif /* STA */

	if (scb && SCB_INTERNAL(scb)) {
		return 0;
	}

	snr_window_sz = rssi_window_sz = BSS_MA_WINDOW_SZ(lqi, cfg);
#ifdef STA
	rssi_bcn_window_sz = lqi->sta_bcn_window_sz;
#endif /* STA */

	/* allocate rssi moving average window, per-ant rssi sampling window,
	 * and snr moving average window after the scb_lq_info_t.
	 */
	extra = rssi_window_sz;					/* PHY reported rssi samples */
	if (PER_ANT_RSSI_SAMP(lqi, cfg)) {
		extra += rssi_window_sz * WL_RSSI_ANT_MAX;	/* per-ant raw rssi samples */
	}
#ifdef STA
	/* per-ant raw bcn rssi samples */
	if (PER_ANT_BCN_RSSI_SAMP(lqi, cfg)) {
		extra += rssi_bcn_window_sz * WL_RSSI_ANT_MAX;
	}
#endif /* STA */
	if (PER_SCB_SNR_SAMP(cfg)) {
		extra += snr_window_sz;				/* SNR samples */
	}

	return sizeof(scb_lq_info_t) + extra;
}

static int
wlc_lq_scb_init(void *ctx, struct scb *scb)
{
	wlc_lq_info_t *lqi = (wlc_lq_info_t *)ctx;
	wlc_info_t *wlc = lqi->wlc;
	scb_lq_info_t **pslqi = SCB_LQ_INFO_LOC(lqi, scb);
	scb_lq_info_t *slqi = SCB_LQ_INFO(lqi, scb);
	wlc_bsscfg_t *cfg = SCB_BSSCFG(scb);
	uint16 rssi_window_sz, snr_window_sz;
	uint extra;
	uint secsz;
#ifdef STA
	uint16 rssi_bcn_window_sz;
#endif /* STA */

	/* sanity check */
	ASSERT(slqi == NULL);

	/* allocate rssi moving average window, per-ant rssi sampling window,
	 * and snr moving average window after the scb_lq_info_t.
	 */
	secsz = wlc_lq_scb_secsz(lqi, scb);
	if ((slqi = wlc_scb_sec_cubby_alloc(wlc, scb, secsz)) == NULL) {
		/* can happen for internal cubbies */
		return BCME_OK;
	}
	*pslqi = slqi;

	snr_window_sz = rssi_window_sz = BSS_MA_WINDOW_SZ(lqi, cfg);
#ifdef STA
	rssi_bcn_window_sz = lqi->sta_bcn_window_sz;
#endif /* STA */

	slqi->rssi_window = (int8 *)&slqi[1];
	extra = rssi_window_sz;
	if (PER_ANT_RSSI_SAMP(lqi, cfg)) {
		slqi->rssi_chain_window = slqi->rssi_window + extra;
		extra += rssi_window_sz * WL_RSSI_ANT_MAX;
	}
#ifdef STA
	if (PER_ANT_BCN_RSSI_SAMP(lqi, cfg)) {
		slqi->rssi_chain_bcn_window = slqi->rssi_window + extra;
		extra += rssi_bcn_window_sz * WL_RSSI_ANT_MAX;
	}
#endif /* STA */
	if (PER_SCB_SNR_SAMP(cfg)) {
		slqi->snr_window = (uint8 *)slqi->rssi_window + extra;
		extra += snr_window_sz;
	}
	ASSERT(sizeof(*slqi) + extra == secsz);

	/* move to wlc_assoc.c once modularized */
	if (cfg->BSS) {
		wlc_lq_sample_req_enab(scb, RX_LQ_SAMP_REQ_BSS_STA, TRUE);
	}

	if (BSSCFG_AP(cfg) || BSSCFG_IBSS(cfg)) {
		wlc_lq_rssi_ma_reset(wlc, cfg, scb, WLC_RSSI_INVALID);
	}

	return BCME_OK;
}

static void
wlc_lq_scb_deinit(void *ctx, struct scb *scb)
{
	wlc_lq_info_t *lqi = (wlc_lq_info_t *)ctx;
	wlc_info_t *wlc = lqi->wlc;
	scb_lq_info_t **pslqi = SCB_LQ_INFO_LOC(lqi, scb);
	scb_lq_info_t *slqi = SCB_LQ_INFO(lqi, scb);

	if (slqi != NULL) {
		wlc_scb_sec_cubby_free(wlc, scb, slqi);
	}
	*pslqi = NULL;
}

#if defined(BCMDBG) || defined(WLTEST)
static void
wlc_lq_scb_dump(void *ctx, struct scb *scb, struct bcmstrbuf *b)
{
	wlc_lq_info_t *lqi = (wlc_lq_info_t *)ctx;
	wlc_info_t *wlc = lqi->wlc;
	wlc_bsscfg_t *cfg = SCB_BSSCFG(scb);
	bss_lq_info_t *blqi = BSS_LQ_INFO(lqi, cfg);
	scb_lq_info_t *slqi = SCB_LQ_INFO(lqi, scb);
	uint i, k;
	uint idx;
	int8 *row;
	uint rssi_window_sz;
	uint snr_window_sz;

	if (slqi == NULL)
		return;

	rssi_window_sz = MA_WIN_SZ(blqi->rssi_window_sz);
	snr_window_sz = MA_WIN_SZ(blqi->snr_window_sz);

	/* RSSI */
	if (PER_SCB_RSSI_SAMP(cfg)) {
		bcm_bprintf(b, "     RSSI: [ ");
		idx = slqi->rssi_index;
		for (i = 0; i < rssi_window_sz; i ++) {
			bcm_bprintf(b, "%3d ", slqi->rssi_window[idx]);
			idx = MODINC_POW2(idx, rssi_window_sz);
		}
		bcm_bprintf(b, "] AVG: %d\n", wlc_lq_rssi_get(wlc, cfg, scb));
		if (PER_ANT_RSSI_SAMP(lqi, cfg)) {
			for (k = WL_ANT_IDX_1; k < WL_RSSI_ANT_MAX; k ++) {
				if ((wlc->stf->rxchain & (1 << k)) == 0)
					continue;
				row = slqi->rssi_chain_window + k * rssi_window_sz;
				bcm_bprintf(b, "     ANT%d: [ ", k);
				idx = slqi->rssi_chain_index;
				for (i = 0; i < rssi_window_sz; i ++) {
					bcm_bprintf(b, "%3d ", *(row + idx));
					idx = MODINC_POW2(idx, rssi_window_sz);
				}
				/* XXX new format but can't do without breaking the UTF.
				 * bcm_bprintf(b, "] AVG: %3d\n",
				 *	wlc_lq_ant_rssi_get(wlc, cfg, scb, k));
				 */
				bcm_bprintf(b, "] AVG [%d]\n",
					wlc_lq_ant_rssi_get(wlc, cfg, scb, k));
			}
		}
	}

	/* SNR */
	if (PER_SCB_SNR_SAMP(cfg)) {
		bcm_bprintf(b, "     SNR: [ ");
		idx = slqi->snr_index;
		for (i = 0; i < snr_window_sz; i ++) {
			bcm_bprintf(b, "%3d ", slqi->snr_window[idx]);
			idx = MODINC_POW2(idx, snr_window_sz);
		}
		bcm_bprintf(b, "] AVG: %u\n", cfg->link->snr);
	}
}
#endif // endif

#ifdef STA
/* callback for beacon reception */
static void
wlc_lq_ant_bcn_rssi(void *ctx, bss_rx_bcn_notif_data_t *bcn)
{
	wlc_lq_info_t *lqi = (wlc_lq_info_t *)ctx;
	wlc_info_t *wlc = lqi->wlc;
	wlc_bsscfg_t *cfg = bcn->cfg;
	bss_lq_info_t *blqi = BSS_LQ_INFO(lqi, cfg);
	rx_lq_samp_t lq_sample;

	wlc_lq_sample(wlc, cfg, bcn->scb, bcn->wrxh, FALSE, &lq_sample);
	/* per ant bcn rssi */
	wlc_lq_ant_bcn_rssi_sample(wlc, cfg, bcn->scb, bcn->wrxh);

	if (cfg->BSS) {
		wlc_lq_rssi_bss_sta_event_upd(wlc, cfg);

#ifdef EVENT_LOG_COMPILE
		{
		struct dot11_bcn_prb *bcnprb = (struct dot11_bcn_prb *)bcn->body;
		EVENT_LOG(EVENT_LOG_TAG_BEACON_LOG,
			"Beacon(recv): RSSI=%d SNR=%d TSF=%08x%08x",
			lq_sample.rssi_avg, lq_sample.snr_avg,
			bcnprb->timestamp[1], bcnprb->timestamp[0]);
		}
#endif /* EVENT_LOG_COMPILE */
		blqi->lq_last_bcn_time = OSL_SYSUPTIME();
		cfg->current_bss->RSSI = lq_sample.rssi_avg;
		cfg->current_bss->SNR = lq_sample.snr_avg;

#ifdef RSSI_MONITOR
		if (RMON_ENAB(wlc->pub)) {
			wlc_lq_rssi_monitor_event(cfg);
		}
#endif /* RSSI_MONITOR */
	}
}
#endif /* STA */

#ifdef WLCHANIM
static wlc_chanim_stats_t *
wlc_lq_chanim_create_stats(wlc_info_t *wlc, chanspec_t chanspec)
{
	wlc_chanim_stats_t *new_stats = NULL;
	chanspec_t ctl_chanspec;

	/* if the chanspec passed is malformed or Zero avoid allocation of memory */
	if (chanspec == 0 || wf_chspec_malformed(chanspec)) {
		return NULL;
	}

	new_stats = (wlc_chanim_stats_t *) MALLOCZ(wlc->osh, sizeof(wlc_chanim_stats_t));

	if (!new_stats) {
		WL_ERROR(("wl%d: %s: out of memory %d bytes\n",
			wlc->pub->unit, __FUNCTION__, (uint)sizeof(wlc_chanim_stats_t)));
	}
	else {
		memset(new_stats, 0, sizeof(*new_stats));
		ctl_chanspec = wf_chspec_ctlchspec(chanspec);
		new_stats->chanim_stats.chanspec = ctl_chanspec;
		new_stats->next = NULL;
	}
	return new_stats;
}

static void
wlc_lq_chanim_insert_stats(wlc_chanim_stats_t **rootp, wlc_chanim_stats_t *new)
{
	wlc_chanim_stats_t *curptr;
	wlc_chanim_stats_t *previous;

	curptr = *rootp;
	previous = NULL;

	while (curptr &&
		(curptr->chanim_stats.chanspec < new->chanim_stats.chanspec)) {
		previous = curptr;
		curptr = curptr->next;
	}
	new->next = curptr;

	if (previous == NULL)
		*rootp = new;
	else
		previous->next = new;
}

chanim_stats_t *
wlc_lq_chanspec_to_chanim_stats(chanim_info_t *c_info, chanspec_t chanspec)
{
	wlc_chanim_stats_t *stats;

	if ((stats = wlc_lq_chanim_chanspec_to_stats(c_info, chanspec, FALSE)) != NULL) {
		return &(stats->chanim_stats);
	}

	return NULL;
}

#ifdef WLCHANIM_US
static wlc_chanim_stats_us_t *
wlc_lq_chanim_create_stats_us(wlc_info_t *wlc, chanspec_t chanspec)
{
	wlc_chanim_stats_us_t *new_stats = NULL;
	chanspec_t ctl_chanspec;

	/* if the chanspec passed is malformed or Zero avoid allocation of memory */
	if (chanspec == 0 || wf_chspec_malformed(chanspec)) {
		return NULL;
	}

	new_stats = (wlc_chanim_stats_us_t *) MALLOCZ(wlc->osh, sizeof(wlc_chanim_stats_us_t));

	if (!new_stats) {
		WL_ERROR(("wl%d: %s: out of memory %d bytes\n",
			wlc->pub->unit, __FUNCTION__, (uint)sizeof(wlc_chanim_stats_us_t)));
	}
	else {
		memset(new_stats, 0, sizeof(*new_stats));
		ctl_chanspec = wf_chspec_ctlchspec(chanspec);
		new_stats->chanim_stats_us.chanspec = ctl_chanspec;
		new_stats->next = NULL;
	}
	return new_stats;
}

static void
wlc_lq_chanim_insert_stats_us(wlc_chanim_stats_us_t **rootp, wlc_chanim_stats_us_t *new)
{
	wlc_chanim_stats_us_t *curptr;
	wlc_chanim_stats_us_t *previous;

	curptr = *rootp;
	previous = NULL;

	while (curptr &&
		(curptr->chanim_stats_us.chanspec < new->chanim_stats_us.chanspec)) {
		previous = curptr;
		curptr = curptr->next;
	}
	new->next = curptr;

	if (previous == NULL) {
		*rootp = new;
	} else {
		previous->next = new;
	}
}

static wlc_chanim_stats_us_t *
wlc_lq_chanim_chanspec_to_stats_us(chanim_info_t *c_info, chanspec_t chanspec)
{
	chanspec_t ctl_chanspec;
	wlc_chanim_stats_us_t *cur_stats_us = c_info->stats_us;
	chanim_interface_info_t *if_info = NULL;

	/* For quicker access, look in cache first. Otherwise, walk the list. */
	if ((if_info = wlc_lq_chanim_if_info_find(c_info->ifaces, chanspec)) != NULL) {
		if (if_info->stats_us != NULL &&
			if_info->stats_us->chanim_stats_us.chanspec == chanspec) {
			return if_info->stats_us;
		}
	}

	ctl_chanspec = wf_chspec_ctlchspec(chanspec);
	while (cur_stats_us) {
		if (cur_stats_us->chanim_stats_us.chanspec == ctl_chanspec) {
			return cur_stats_us;
		}
		cur_stats_us = cur_stats_us->next;
	}
	return cur_stats_us;
}

static wlc_chanim_stats_us_t *
wlc_lq_chanim_find_stats_us(wlc_info_t *wlc, chanspec_t chanspec)
{
	wlc_chanim_stats_us_t *stats_us = NULL;
	chanim_info_t *c_info = wlc->chanim_info;
	stats_us = wlc_lq_chanim_chanspec_to_stats_us(c_info, chanspec);

	if (!stats_us) {
		stats_us = wlc_lq_chanim_create_stats_us(wlc, chanspec);
		if (stats_us) {
			wlc_lq_chanim_insert_stats_us(&c_info->stats_us, stats_us);
		}
	}

	return stats_us;
}

static void
wlc_lq_chanim_us_accum(chanim_info_t* c_info, chanim_cnt_us_t *cur_cnt_us, chanim_acc_us_t *acc_us)
{
	int i;
	uint32 ccastats_us_delta = 0;
	chanim_cnt_us_t *last_cnt_us;

	last_cnt_us = &c_info->last_cnt_us;

	for (i = 0; i < CCASTATS_MAX; i++) {
		if (last_cnt_us->ccastats_cnt[i] || acc_us->chanspec) {
			ccastats_us_delta = cur_cnt_us->ccastats_cnt[i] -
				last_cnt_us->ccastats_cnt[i];
			acc_us->ccastats_us[i] += ccastats_us_delta;
		}
		last_cnt_us->ccastats_cnt[i] = cur_cnt_us->ccastats_cnt[i];
	}
	ccastats_us_delta = cur_cnt_us->busy_tm - last_cnt_us->busy_tm;
	acc_us->busy_tm += ccastats_us_delta;
	last_cnt_us->busy_tm =  cur_cnt_us->busy_tm;

	ccastats_us_delta = cur_cnt_us->rxcrs_pri20 - last_cnt_us->rxcrs_pri20;
	acc_us->rxcrs_pri20 += ccastats_us_delta;
	last_cnt_us->rxcrs_pri20 =  cur_cnt_us->rxcrs_pri20;

	ccastats_us_delta = cur_cnt_us->rxcrs_sec20 - last_cnt_us->rxcrs_sec20;
	acc_us->rxcrs_sec20 += ccastats_us_delta;
	last_cnt_us->rxcrs_sec20 =  cur_cnt_us->rxcrs_sec20;

	ccastats_us_delta = cur_cnt_us->rxcrs_sec40 - last_cnt_us->rxcrs_sec40;
	acc_us->rxcrs_sec40 += ccastats_us_delta;
	last_cnt_us->rxcrs_sec40 =  cur_cnt_us->rxcrs_sec40;
}
#endif /* WLCHANIM_US */

static wlc_chanim_stats_t *
wlc_lq_chanim_chanspec_to_stats(chanim_info_t *c_info, chanspec_t chanspec, bool scan_param)
{
	chanspec_t ctl_chanspec;
	wlc_chanim_stats_t *cur_stats = c_info->stats;
	chanim_interface_info_t *if_info = NULL;

	/* For quicker access, look in cache first. Otherwise, walk the list. */
	if (!scan_param) {
		/* For quicker access, look in cache first. Otherwise, walk the list. */
		if ((if_info = wlc_lq_chanim_if_info_find(c_info->ifaces, chanspec)) != NULL) {

			return if_info->stats;
		}
	}

	ctl_chanspec = wf_chspec_ctlchspec(chanspec);
	while (cur_stats) {
		if (cur_stats->chanim_stats.chanspec == ctl_chanspec) {
			return cur_stats;
		}
		cur_stats = cur_stats->next;
	}
	return cur_stats;
}

bool
wlc_lq_chanim_stats_get(chanim_info_t *c_info, chanspec_t chanspec, chanim_stats_t *stats)
{
	wlc_chanim_stats_t *chanim_stats = wlc_lq_chanim_chanspec_to_stats(c_info, chanspec, FALSE);
	if (chanim_stats != NULL) {
		memcpy(stats, &chanim_stats->chanim_stats, sizeof(chanim_stats_t));
		return chanim_stats->is_valid;
	}
	return FALSE;
}

static wlc_chanim_stats_t *
wlc_lq_chanim_find_stats(wlc_info_t *wlc, chanspec_t chanspec)
{
	wlc_chanim_stats_t *stats = NULL;
	chanim_info_t *c_info = wlc->chanim_info;

	if (SCAN_IN_PROGRESS(wlc->scan)) {
		stats = wlc_lq_chanim_chanspec_to_stats(c_info, chanspec, TRUE);
		if (!stats) {
			stats = wlc_lq_chanim_create_stats(wlc, chanspec);
			if (stats)
				wlc_lq_chanim_insert_stats(&c_info->stats, stats);
		}
	} else {
		stats = &c_info->cur_stats;
		stats->chanim_stats.chanspec = chanspec;
		stats->next = NULL;
	}

	return stats;
}

static void
wlc_lq_chanim_meas(wlc_info_t *wlc, chanim_cnt_t *chanim_cnt, chanim_cnt_us_t *chanim_cnt_us)
{
	uint16 rxcrsglitch = 0;
	uint16 rxbadplcp = 0;
	uint32 ccastats_cnt[CCASTATS_MAX];
	int i;
	uint32 cca_wifi = 0, cca_edcrs = 0;

	/* Read rxcrsglitch count from shared memory */
	rxcrsglitch = wlc_bmac_read_shm(wlc->hw,
		MACSTAT_ADDR(wlc, MCSTOFF_RXCRSGLITCH));
	chanim_cnt->glitch_cnt = rxcrsglitch;

	chanim_cnt->bphy_glitch_cnt = wlc_bmac_read_shm(wlc->hw,
		MACSTAT_ADDR(wlc, MCSTOFF_BPHYGLITCH));

	rxbadplcp = wlc_bmac_read_shm(wlc->hw,
		MACSTAT_ADDR(wlc, MCSTOFF_RXBADPLCP));
	chanim_cnt->badplcp_cnt = rxbadplcp;

	chanim_cnt->bphy_badplcp_cnt = wlc_bmac_read_shm(wlc->hw,
		MACSTAT_ADDR(wlc, MCSTOFF_BPHY_BADPLCP));

#ifdef WLCHANIM_US
	wlc_bmaq_lq_stats_read(wlc->hw, chanim_cnt_us);
#endif /* WLCHANIM_US */
	chanim_cnt_us->busy_tm = 0;
	for (i = 0; i < CCASTATS_MAX; i++) {
		ccastats_cnt[i] =
			wlc_bmac_cca_read_counter(wlc->hw, 4 * i, (4 * i + 2));
#ifdef WLCHANIM_US
		chanim_cnt_us->ccastats_cnt[i] =
			wlc_bmac_cca_read_counter(wlc->hw, 4 * i, (4 * i + 2));
		if (i > 0 && i <= 4) {
			chanim_cnt_us->busy_tm += chanim_cnt_us->ccastats_cnt[i];
		}
#endif /* WLCHANIM_US */
		chanim_cnt->ccastats_cnt[i] = ccastats_cnt[i];
	}

	if (D11REV_GE(wlc->pub->corerev, 40)) {
			cca_wifi = wlc_bmac_cca_read_counter(wlc->hw,
				M_CCA_WIFI_L_OFFSET(wlc), M_CCA_WIFI_H_OFFSET(wlc));
			cca_edcrs = wlc_bmac_cca_read_counter(wlc->hw,
				M_CCA_EDCRSDUR_L_OFFSET(wlc), M_CCA_EDCRSDUR_H_OFFSET(wlc));
			chanim_cnt->ccastats_cnt[CCASTATS_NOCTG] += cca_wifi;
			chanim_cnt->ccastats_cnt[CCASTATS_NOPKT] += cca_edcrs;
#ifdef WLCHANIM_US
			chanim_cnt_us->ccastats_cnt[CCASTATS_NOCTG] += cca_wifi;
			chanim_cnt_us->ccastats_cnt[CCASTATS_NOPKT] += cca_edcrs;
			chanim_cnt_us->busy_tm += cca_wifi;
			chanim_cnt_us->busy_tm += cca_edcrs;
#endif // endif

	}

	chanim_cnt->timestamp = OSL_SYSUPTIME();
}

static void
wlc_lq_chanim_glitch_accum(chanim_info_t* c_info, chanim_cnt_t *cur_cnt, chanim_accum_t *acc)
{
	uint16 glitch_delta = 0;
	uint16 bphy_glitch_delta = 0;
	chanim_cnt_t *last_cnt;

	last_cnt = &c_info->last_cnt;

	/* The statistics glitch_delta are computed when there is a non zero value of
	   last_cnt->glitch_cnt. Bphy statistics are also being updated here because,
	   last_cnt->glitch_cnt is the sum of both OFDM and BPHY glitch counts.
	   So, if there is a non zero value of total glitch count, it is a good idea
	   to update both OFDM and BPHY glitch counts.
	 */
	if (last_cnt->glitch_cnt || acc->chanspec) {
		glitch_delta = cur_cnt->glitch_cnt - last_cnt->glitch_cnt;
		bphy_glitch_delta = cur_cnt->bphy_glitch_cnt - last_cnt->bphy_glitch_cnt;
		acc->glitch_cnt += glitch_delta;
		acc->bphy_glitch_cnt += bphy_glitch_delta;
	}
	last_cnt->glitch_cnt = cur_cnt->glitch_cnt;
	last_cnt->bphy_glitch_cnt = cur_cnt->bphy_glitch_cnt;
}

static void
wlc_lq_chanim_badplcp_accum(chanim_info_t* c_info, chanim_cnt_t *cur_cnt, chanim_accum_t *acc)
{
	uint16 badplcp_delta = 0;
	uint16 bphy_badplcp_delta = 0;
	chanim_cnt_t *last_cnt;

	last_cnt = &c_info->last_cnt;

	/* The statistics badplcp_delta are computed when there is a non zero value of
	   last_cnt->badplcp_cnt. Bphy statistics are also being updated here because,
	   last_cnt->badplcp_cnt is the sum of both OFDM and BPHY badplcp counts.
	   So, if there is a non zero value of total badplcp count, it is a good idea
	   to update both OFDM and BPHY badplcp counts.
	 */
	if (last_cnt->badplcp_cnt) {
		badplcp_delta = cur_cnt->badplcp_cnt - last_cnt->badplcp_cnt;
		bphy_badplcp_delta = cur_cnt->bphy_badplcp_cnt - last_cnt->bphy_badplcp_cnt;
		acc->badplcp_cnt += badplcp_delta;
		acc->bphy_badplcp_cnt += bphy_badplcp_delta;
	}
	last_cnt->badplcp_cnt = cur_cnt->badplcp_cnt;
	last_cnt->bphy_badplcp_cnt = cur_cnt->bphy_badplcp_cnt;
}

static void
wlc_lq_chanim_ccastats_accum(chanim_info_t* c_info, chanim_cnt_t *cur_cnt, chanim_accum_t *acc)
{
	int i;
	uint32 ccastats_delta = 0;
	chanim_cnt_t *last_cnt;
	bool overflow = FALSE;

	last_cnt = &c_info->last_cnt;

	for (i = 0; i < CCASTATS_MAX; i++) {
		/* dont accumulate any stat if last_cnt was reset or rolled over */
		if (cur_cnt->ccastats_cnt[i] < last_cnt->ccastats_cnt[i]) {
			overflow = TRUE;
			break;
		}
	}

	for (i = 0; i < CCASTATS_MAX; i++) {
		if ((last_cnt->ccastats_cnt[i] || acc->chanspec) && (!overflow)) {
			ccastats_delta = cur_cnt->ccastats_cnt[i] - last_cnt->ccastats_cnt[i];
			acc->ccastats_us[i] += ccastats_delta;
		}
		last_cnt->ccastats_cnt[i] = cur_cnt->ccastats_cnt[i];
	}
}

/*
 * based on current read, accumulate the count
 * also, update the last
 */
static void
wlc_lq_chanim_accum(wlc_info_t* wlc, chanspec_t chanspec, chanim_accum_t *acc,
	chanim_acc_us_t *acc_us)
{
	chanim_info_t* c_info = wlc->chanim_info;
	chanim_cnt_t cur_cnt, *last_cnt;
	chanim_cnt_us_t cur_cnt_us;
	uint cur_time;
	uint interval = 0;

	/* read the current measurement counters */
	wlc_lq_chanim_meas(wlc, &cur_cnt, &cur_cnt_us);

	last_cnt = &c_info->last_cnt;
	cur_time = OSL_SYSUPTIME();
	if (last_cnt->timestamp) {
		interval = cur_time - last_cnt->timestamp;
	}

	/* update the accumulator with current deltas */
	wlc_lq_chanim_glitch_accum(c_info, &cur_cnt, acc);
	wlc_lq_chanim_badplcp_accum(c_info, &cur_cnt, acc);
	wlc_lq_chanim_ccastats_accum(c_info, &cur_cnt, acc);

#ifdef WLCHANIM_US
	if (wlc_isup(wlc)) {
		wlc_lq_chanim_us_accum(c_info, &cur_cnt_us, acc_us);
		acc_us->chanspec = chanspec;
	}
#endif /* WLCHANIM_US */

	last_cnt->timestamp = cur_time;
	acc->stats_ms += interval;
	acc->chanspec = chanspec;
}

static void
wlc_lq_chanim_clear_acc(wlc_info_t* wlc, chanim_accum_t* acc, chanim_acc_us_t* acc_us,
	bool chan_switch)
{
	int i;

	if (acc) {
		acc->glitch_cnt = 0;
		acc->badplcp_cnt = 0;
		acc->bphy_glitch_cnt = 0;
		acc->bphy_badplcp_cnt = 0;

		for (i = 0; i < CCASTATS_MAX; i++)
			acc->ccastats_us[i] = 0;

		acc->stats_ms = 0;
	}
#ifdef WLCHANIM_US
	if (acc_us && (chan_switch == TRUE)) {
		acc_us->busy_tm = 0;
		acc_us->rxcrs_pri20 = 0;
		acc_us->rxcrs_sec20 = 0;
		acc_us->rxcrs_sec40 = 0;

			for (i = 0; i < CCASTATS_MAX; i++) {
				acc_us->ccastats_us[i] = 0;
		}
	}
#endif /* WLCHANIM_US */
}

static int8
wlc_lq_chanim_phy_noise(wlc_info_t *wlc)
{

	int32 rxiq = 0;
	int8 result = 0;
	int err = 0;
	wl_iqest_params_t params;
	int16 rxiq_core[WL_STA_ANT_MAX] = {0};

	if (SCAN_IN_PROGRESS(wlc->scan)) {
		int cnt = 10, valid_cnt = 0;
		int i;
		int sum = 0;

		memset(&params, 0, sizeof(params));
		params.niter = 1;
		params.delay = PHY_RXIQEST_AVERAGING_DELAY;
		params.rxiq = 10 << 8 | 3; /* default: samples = 1024 (2^10) and antenna = 3 */

		/* iovar set */
		if ((err = wlc_iovar_op(wlc, "phy_rxiqest", NULL, 0, &params, sizeof(params),
			IOV_SET, NULL)) < 0) {
			WL_ERROR(("failed to set phy_rxiqest\n"));
			return err;
		}
		for (i =  0; i < cnt; i++) {
			memset(rxiq_core, 0, sizeof(rxiq_core));
			rxiq = 0;
			if ((err = wlc_iovar_op(wlc, "phy_rxiqest", NULL, 0,
				(void *)&rxiq_core, sizeof(rxiq_core), IOV_GET, NULL)) < 0) {
				WL_ERROR(("failed to get phy_rxiqest\n"));
				return err;
			}
			/* use the last byte to compute the bgnoise estimation
			* phy_rxiqest returns values in dBm (negative number).
			* We require only some portion of the values as determined
			* by the last byte.
			*/
			rxiq_core[1] &= 0xff;
			rxiq_core[0] &= 0xff;
			rxiq = (int32)(rxiq_core[1]<<16) + (int32)rxiq_core[0];

			if (rxiq >> 8)
				result = (int8)MAX((rxiq >> 8) & 0xff, (rxiq & 0xff));
			else
				result = (int8)(rxiq & 0xff);
			if (result) {
				sum += result;
				valid_cnt++;
			}
		}
		if (valid_cnt)
			result = sum/valid_cnt;
	}

	if (!SCAN_IN_PROGRESS(wlc->scan))
		result = phy_noise_avg(WLC_PI(wlc));

	WL_CHANINT(("bgnoise: %d dBm\n", result));

	return result;
}

/*
 * convert the stats from the accumulative counters to the final stats
 * also clear the accumulative counter.
 */
static void
wlc_lq_chanim_close(wlc_info_t* wlc, chanspec_t chanspec, chanim_accum_t* acc,
	wlc_chanim_stats_t *cur_stats, chanim_acc_us_t *acc_us,
	wlc_chanim_stats_us_t *cur_stats_us, bool chan_switch)
{
	int i;
	uint8 stats_frac = 0;
	int32 aci_chan_vld_dur;
	uint txop_us = 0;
	uint slottime = APHY_SLOT_TIME;
	uint txop = 0, txop_nom = 0;
	uint8 txop_percent = 0;
#ifdef WLCHANIM_US
	chanim_info_t* c_info = wlc->chanim_info;
	chanim_cnt_us_t *last_cnt_us;
	chanim_interface_info_t *if_info, *ifaces = c_info->ifaces;
#endif /* WLCHANIM_US */

	if (cur_stats == NULL || acc == NULL)
		return;

	// Don't include valid TX & RX in idle time, as that bloats the glitches un-necesarily
	// raising desense. Only use doze(sleep time) and stats_ms(in case its doing p2p)
	aci_chan_vld_dur = (int32) ((acc->stats_ms * 1000) - acc->ccastats_us[CCASTATS_DOZE] + 500)
		 / 1000;

	/* normalized to per second count */
	if ((acc->stats_ms) && (aci_chan_vld_dur > 0)) {

		cur_stats->chanim_stats.glitchcnt = acc->glitch_cnt * 1000 / aci_chan_vld_dur;

		cur_stats->chanim_stats.bphy_glitchcnt = acc->bphy_glitch_cnt *
			1000 / aci_chan_vld_dur;

		cur_stats->chanim_stats.badplcp = acc->badplcp_cnt * 1000 / aci_chan_vld_dur;

		cur_stats->chanim_stats.bphy_badplcp = acc->bphy_badplcp_cnt *
			1000 / aci_chan_vld_dur;

		cur_stats->is_valid = TRUE;

	} else {
		cur_stats->chanim_stats.glitchcnt = 0;

		cur_stats->chanim_stats.bphy_glitchcnt = 0;

		cur_stats->chanim_stats.badplcp = 0;

		cur_stats->chanim_stats.bphy_badplcp = 0;

		cur_stats->is_valid = FALSE;
	}

	if (wlc->band->gmode && !wlc->shortslot)
		slottime = BPHY_SLOT_TIME;

	for (i = 0; i < CCASTATS_MAX; i++) {
		/* normalize to be 0-100 */

		if (acc->stats_ms) {
			if (i == CCASTATS_TXOP)
				stats_frac = (uint8)CEIL(acc->ccastats_us[i] * slottime,
				  acc->stats_ms * 10);
			else
				stats_frac = (uint8)CEIL(100 * acc->ccastats_us[i],
				  acc->stats_ms * 1000);
		}

		if (stats_frac > 100) {
			WL_INFORM(("stats(%d) > 100: ccastats_us: %d, acc->statss_ms: %d\n",
				stats_frac, acc->ccastats_us[i], acc->stats_ms));
			stats_frac = 100;
		}
		cur_stats->chanim_stats.ccastats[i] = stats_frac;
	}

	/* calc chan_idle */
	txop_us = (acc->stats_ms * 1000) - acc->ccastats_us[CCASTATS_DOZE];
	txop_nom = txop_us / slottime;
	txop = acc->ccastats_us[CCASTATS_TXOP] +
		(acc->ccastats_us[CCASTATS_TXDUR] -
		acc->ccastats_us[CCASTATS_BDTXDUR]) / slottime;
	if (txop_nom) {
		 txop_percent = (uint8)CEIL(100 * txop, txop_nom);
		 txop_percent = MIN(100, txop_percent);
	}
	cur_stats->chanim_stats.chan_idle = txop_percent;

	cur_stats->chanim_stats.bgnoise = wlc_lq_chanim_phy_noise(wlc);

	cur_stats->chanim_stats.timestamp = OSL_SYSUPTIME();
#ifdef WLCHANIM_US
	if (wlc_isup(wlc)) {
		last_cnt_us = &c_info->last_cnt_us;
		cur_stats_us->chanim_stats_us.total_tm = ((OSL_SYSUPTIME() -
			last_cnt_us->start_tm)*1000) +
			last_cnt_us->total_tm;
		cur_stats_us->chanim_stats_us.busy_tm = acc_us->busy_tm;
		for (i = 0; i < CCASTATS_MAX; i++) {
			cur_stats_us->chanim_stats_us.ccastats_us[i] = acc_us->ccastats_us[i];
		}

		cur_stats_us->chanim_stats_us.rxcrs_pri20 = acc_us->rxcrs_pri20;
		cur_stats_us->chanim_stats_us.rxcrs_sec20 = acc_us->rxcrs_sec20;
		cur_stats_us->chanim_stats_us.rxcrs_sec40 = acc_us->rxcrs_sec40;
		if (chan_switch == TRUE) {
			last_cnt_us->start_tm = 0;
		}
		if (SCAN_IN_PROGRESS(wlc->scan) && (chanspec == wlc->home_chanspec))
		{
			if_info = wlc_lq_chanim_if_info_find(ifaces, chanspec);
			if (if_info) {
				memcpy(&if_info->acc_us, acc_us, sizeof(chanim_acc_us_t));
			}
		}
	}
#endif /* WLCHANIM_US */
	wlc_lq_chanim_clear_acc(wlc, acc, acc_us, chan_switch);
}

#ifdef BCMDBG
static int
wlc_lq_chanim_display(wlc_info_t *wlc, chanspec_t chanspec, wlc_chanim_stats_t *cur_stats)
{
	chanim_info_t *c_info = wlc->chanim_info;

	if (!cur_stats)
		return BCME_ERROR;

	BCM_REFERENCE(c_info);

	WL_CHANINT(("**intf: %d glitch cnt: %d badplcp: %d noise: %d chanspec: 0x%x \n",
		chanim_mark(c_info).state, cur_stats->chanim_stats.glitchcnt,
		cur_stats->chanim_stats.badplcp, cur_stats->chanim_stats.bgnoise, chanspec));

	WL_CHANINT(("***cca stats: txdur: %d, inbss: %d, obss: %d,"
	  "nocat: %d, nopkt: %d, doze: %d\n",
	  cur_stats->chanim_stats.ccastats[CCASTATS_TXDUR],
	  cur_stats->chanim_stats.ccastats[CCASTATS_INBSS],
	  cur_stats->chanim_stats.ccastats[CCASTATS_OBSS],
	  cur_stats->chanim_stats.ccastats[CCASTATS_NOCTG],
	  cur_stats->chanim_stats.ccastats[CCASTATS_NOPKT],
	  cur_stats->chanim_stats.ccastats[CCASTATS_DOZE]));

	return BCME_OK;
}
#endif /* BCMDBG */

/*
 * Given a chanspec, find the matching interface info. If there isn't a match, then
 * find an empty slot. A reference count is incremented if this is called from a
 * BSSCFG callback.
 */
static chanim_interface_info_t *
wlc_lq_chanim_if_info_setup(chanim_info_t *c_info, chanspec_t chanspec)
{
	wlc_info_t *wlc = c_info->wlc;
	chanim_interface_info_t *if_info, *ifaces = c_info->ifaces;
#ifdef WLCHANIM_US
	int i = 0;
	chanim_cnt_us_t *last_cnt_us;

	last_cnt_us = &c_info->last_cnt_us;
#endif /* WLCHANIM_US */
	/* Find an existing entry with a matching chanspec */
	if_info = wlc_lq_chanim_if_info_find(ifaces, chanspec);

	if (if_info == NULL) {
		/* Not found. Create an new slot */
		if (c_info->free_list) {
			if_info = c_info->free_list;
			c_info->free_list = if_info->next;
		} else {
			if_info = (chanim_interface_info_t *) MALLOCZ(wlc->osh,
				sizeof(chanim_interface_info_t));

			if (!if_info) {
				WL_ERROR(("wl%d: %s: out of memory %d bytes\n",
					wlc->pub->unit, __FUNCTION__,
					(uint)sizeof(chanim_interface_info_t)));
				ASSERT(FALSE);
				return NULL;
			}
		}

		/* Put in interface info list head */
		if_info->next = ifaces->next;
		ifaces->next = if_info;

		/* Get stats for channel (cached) */
		if_info->stats = wlc_lq_chanim_find_stats(wlc, chanspec);
		if_info->chanspec = chanspec;
		ASSERT(if_info->stats != NULL);
#ifdef WLCHANIM_US
		memset(&if_info->acc_us, 0, sizeof(chanim_acc_us_t));
		memset(last_cnt_us, 0, sizeof(chanim_cnt_us_t));
		last_cnt_us->start_tm = OSL_SYSUPTIME();
		if_info->stats_us = wlc_lq_chanim_find_stats_us(wlc, chanspec);
		ASSERT(if_info->stats_us != NULL);
		last_cnt_us->total_tm = if_info->stats_us->chanim_stats_us.total_tm;
		if_info->acc_us.busy_tm =
			if_info->stats_us->chanim_stats_us.busy_tm;
		if_info->acc_us.rxcrs_pri20 =
			if_info->stats_us->chanim_stats_us.rxcrs_pri20;
		if_info->acc_us.rxcrs_sec20 =
			if_info->stats_us->chanim_stats_us.rxcrs_sec20;
		if_info->acc_us.rxcrs_sec40 =
			if_info->stats_us->chanim_stats_us.rxcrs_sec40;
		for (i = 0; i < CCASTATS_MAX; i++) {
			if_info->acc_us.ccastats_us[i] =
				if_info->stats_us->chanim_stats_us.ccastats_us[i];
		}
		if_info->stats_us->chanim_stats_us.total_tm = 0;
#endif /* WLCHANIM_US */
	}

	return if_info;
}

/*
 * Given a chanspec, release the interface. NOTE: it will only free if reference
 * count is zero. A reference count is decremented if this is called from a BSSCFG
 * callback.
 */
static void
wlc_lq_chanim_if_info_release(chanim_info_t *c_info, chanspec_t chanspec)
{
	chanim_interface_info_t *ifaces = c_info->ifaces;
	chanim_interface_info_t *if_info = NULL;

	/* Find an existing entry with a matching chanspec */
	if_info = wlc_lq_chanim_if_info_find(ifaces, chanspec);

	if (if_info) {
		chanim_interface_info_t *curptr, *previous = ifaces;

		/* remove it from the interface info list */
		while ((curptr = previous->next)) {
			if (curptr == if_info) {
				previous->next = curptr->next;
				break;
			}
			previous = curptr;
		}

		/* Add it to the free list */
		memset(if_info, 0, sizeof(chanim_interface_info_t));
		if_info->next = c_info->free_list;
		c_info->free_list = if_info;
	}
}

static chanim_interface_info_t *
wlc_lq_chanim_if_info_find(chanim_interface_info_t *ifaces, chanspec_t chanspec)
{
	chanim_interface_info_t *if_info = ifaces;

	while ((if_info = if_info->next)) {
		if (if_info->chanspec == chanspec) {
			break;
		}
	}
	return if_info;
}

/*
 * When trying to find a replacement slot in multichannel case, the adopt
 * hook may use a chanspec that is a different bandwidth. In this case, calling
 * the above lookup will not find an exact match. Search based on control
 * channel instead.
 */
static chanim_interface_info_t *
wlc_lq_chanim_if_info_find_ctl(chanim_interface_info_t *ifaces, chanspec_t chanspec)
{
	chanim_interface_info_t *if_info = ifaces;
	chanspec_t ctl_chanspec = wf_chspec_ctlchan(chanspec);

	while ((if_info = if_info->next)) {
		if (wf_chspec_ctlchan(if_info->chanspec) == ctl_chanspec) {
			break;
		}
	}
	return if_info;
}

/*
 * This is called when an i/f is changing a channel. In this case, it accumulate/close
 * the old channel and re-purpose the accumulator for the new channel.
 */
static void
wlc_lq_chanim_if_switch_channels(wlc_info_t *wlc,
	chanim_interface_info_t *if_info,
	chanspec_t chanspec,
	chanspec_t prev_chanspec)
{
#ifdef WLCHANIM_US
	chanim_info_t* c_info = wlc->chanim_info;
	chanim_cnt_us_t *last_cnt_us;
	int i;
#endif /* WLCHANIM_US */
	ASSERT(if_info != NULL);
#ifdef WLCHANIM_US
		last_cnt_us = &c_info->last_cnt_us;
#endif /* WLCHANIM_US */
	/*
	 * NOTE: the new chanspec is passed in because it will overwrite the chanspec field with
	 * this value.
	 */
	wlc_lq_chanim_accum(wlc, chanspec, &if_info->accum, &if_info->acc_us);

	/* Close the previous channel. */
	wlc_lq_chanim_close(wlc, prev_chanspec, &if_info->accum, if_info->stats,
		&if_info->acc_us, if_info->stats_us, TRUE);

#ifdef BCMDBG
	if (wlc_lq_chanim_display(wlc, prev_chanspec, if_info->stats) != BCME_OK) {
		WL_ERROR(("wl%d: %s: if_info->stats is NULL\n", wlc->pub->unit, __FUNCTION__));
	}
#endif // endif

	/* during scan, if the recently closed prev_chanspec stats are in c_info->cur_stats,
	 * copy to c_info->stats linked list entry as well
	 */
	if (if_info->stats == &wlc->chanim_info->cur_stats && SCAN_IN_PROGRESS(wlc->scan) &&
			CHSPEC_IS20(prev_chanspec)) {
		wlc_chanim_stats_t *stats = wlc_lq_chanim_find_stats(wlc, prev_chanspec);
		if (stats != NULL &&
			stats->chanim_stats.chanspec == if_info->stats->chanim_stats.chanspec) {
			if (stats->chanim_stats.timestamp <
					if_info->stats->chanim_stats.timestamp) {
				stats->chanim_stats = if_info->stats->chanim_stats;
				stats->is_valid = if_info->stats->is_valid;
			}
		}
	}
	/*
	 * Since it is switching to a new channel, update the cached pointer to point to the new
	 * counters.
	 */
	if_info->stats = wlc_lq_chanim_find_stats(wlc, chanspec);
	ASSERT(if_info->stats != NULL);
#ifdef WLCHANIM_US
	if (wlc_isup(wlc)) {
		if_info->stats_us = wlc_lq_chanim_find_stats_us(wlc, chanspec);
		ASSERT(if_info->stats_us != NULL);

		/*
		 * Since we are switching to new channel update  accumulated counter with new
		 * channel stats
		 */
		last_cnt_us->start_tm = OSL_SYSUPTIME();
		last_cnt_us->total_tm = if_info->stats_us->chanim_stats_us.total_tm;
		if_info->acc_us.busy_tm = if_info->stats_us->chanim_stats_us.busy_tm;
		if_info->acc_us.rxcrs_pri20 = if_info->stats_us->chanim_stats_us.rxcrs_pri20;
		if_info->acc_us.rxcrs_sec20 = if_info->stats_us->chanim_stats_us.rxcrs_sec20;
		if_info->acc_us.rxcrs_sec40 = if_info->stats_us->chanim_stats_us.rxcrs_sec40;
		for (i = 0; i < CCASTATS_MAX; i++) {
			if_info->acc_us.ccastats_us[i] =
				if_info->stats_us->chanim_stats_us.ccastats_us[i];
		}
	}
#endif /* WLCHANIM_US */
	/* Finally, update chanspec to reflect the new one. */
	if_info->chanspec = chanspec;
}

/*
 * For multi-channel, it requires special hooks called during bss channel
 * creation and deletion. In addition, a hook is called during channel
 * adopt.
 */
void
wlc_lq_chanim_create_bss_chan_context(wlc_info_t *wlc, chanspec_t chanspec,
	chanspec_t prev_chanspec)
{
	chanim_interface_info_t *if_info = NULL;

	if (!WLC_CHANIM_ENAB(wlc->pub)) {
		return;
	}

	if ((prev_chanspec != 0) && (prev_chanspec != chanspec)) {
		/* An interface is switching channels. */
		if_info = wlc_lq_chanim_if_info_find_ctl(wlc->chanim_info->ifaces, prev_chanspec);
	}

	if (if_info != NULL) {
		wlc_lq_chanim_if_switch_channels(wlc, if_info, chanspec, prev_chanspec);
	} else {
		/* Set up a new i/f */
		if_info = wlc_lq_chanim_if_info_setup(wlc->chanim_info, chanspec);
	}

	ASSERT(if_info != NULL);
}

void
wlc_lq_chanim_delete_bss_chan_context(wlc_info_t *wlc, chanspec_t chanspec)
{
	if (!WLC_CHANIM_ENAB(wlc->pub)) {
		return;
	}

	wlc_lq_chanim_if_info_release(wlc->chanim_info, chanspec);
}

/*
 * This is called from mchan's when it is about to adopt a new channel context. This routine
 * will check if we have an entry for the new channel. If not, then that means we're
 * transitioning to a new channel (a real channel change). Otherwise, we already have an
 * entry. No action is required.
 */
int
wlc_lq_chanim_adopt_bss_chan_context(wlc_info_t *wlc, chanspec_t chanspec, chanspec_t prev_chanspec)
{
	chanim_info_t *c_info;
	chanim_interface_info_t *ifaces;
	chanim_interface_info_t *if_info;

	if (!WLC_CHANIM_ENAB(wlc->pub)) {
		return BCME_ERROR;
	}

	c_info = wlc->chanim_info;
	ifaces = c_info->ifaces;

	ASSERT(chanspec != prev_chanspec);

	/* Is there an entry for the new channel? */
	if_info = wlc_lq_chanim_if_info_find(ifaces, chanspec);

	if (if_info) {
		/* Yes, no action required */
		return BCME_OK;
	}

	/*
	 * The above search did not find a match. This can happen when a station chanspec changed
	 * bandwidth in adopt. Instead, search using control channel and switch to the new channel.
	 */
	if ((if_info = wlc_lq_chanim_if_info_find_ctl(ifaces, chanspec))) {
		wlc_lq_chanim_if_switch_channels(wlc, if_info, chanspec, if_info->chanspec);
		return BCME_OK;
	}

	if (SCAN_IN_PROGRESS(wlc->scan)) {
		return BCME_BUSY;
	}
	return BCME_OK;
}

#ifndef WLCHANIM_DISABLED
static int
BCMATTACHFN(wlc_lq_chanim_attach)(wlc_info_t *wlc)
{
	chanim_info_t *c_info;
	chanim_interface_info_t *if_info;
	int ret = BCME_OK;

	if ((wlc->chanim_info = MALLOCZ(wlc->osh, sizeof(chanim_info_t))) == NULL) {
		ret = BCME_NOMEM;
		goto err;
	}

	c_info = wlc->chanim_info;
	ASSERT(wlc->chanim_info != NULL);

	c_info->config.crsglitch_thres = CRSGLITCH_THRESHOLD_DEFAULT;
	c_info->config.ccastats_thres = CCASTATS_THRESHOLD_DEFAULT;
	c_info->config.bgnoise_thres = BGNOISE_THRESHOLD_DEFAULT;
	c_info->config.mode = CHANIM_DETECT;
	c_info->config.sample_period = SAMPLE_PERIOD_DEFAULT;
	c_info->config.threshold_time = THRESHOLD_TIME_DEFAULT;
	c_info->config.lockout_period = LOCKOUT_PERIOD_DEFAULT;
	c_info->config.max_acs = MAX_ACS_DEFAULT;
	c_info->config.scb_max_probe = CHANIM_SCB_MAX_PROBE;

	c_info->stats = NULL;
#ifdef WLCHANIM_US
	c_info->stats_us = NULL;
#endif /* WLCHANIM_US */
	c_info->wlc = wlc;

	if_info = MALLOCZ(wlc->osh, sizeof(chanim_interface_info_t));

	if (if_info == NULL) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		ret = BCME_NOMEM;
		goto err;
	}

	c_info->ifaces = if_info;
	c_info->free_list = NULL;

	/*
	 * Initialize if_info dedicated for scan (this saves an extra check for chanspec valid in
	 * chanim_update).
	 */
	if_info->chanspec = 0x1001;
	if_info->stats = wlc_lq_chanim_find_stats(wlc, if_info->chanspec);
#ifdef WLCHANIM_US
	if_info->stats_us = wlc_lq_chanim_find_stats_us(wlc, if_info->chanspec);
	c_info->last_cnt_us.start_tm = OSL_SYSUPTIME();
	c_info->chanim_timer = wl_init_timer(wlc->wl, wlc_chanim_msec_timeout,
			(void *)wlc, "chanim");
	if (!c_info->chanim_timer) {
		WL_ERROR(("wl%d: %s: wl_init_timer failed\n",
			wlc->pub->unit, __FUNCTION__));
		return BCME_NORESOURCE;
	}
#endif /* WLCHANIM_US */
	wlc->pub->_chanim = TRUE;
	return BCME_OK;
err:
#ifdef WLCHANIM
	MODULE_DETACH(wlc->chanim_info, wlc_lq_chanim_detach);
#endif /* WLCHANIM */

	return ret;
}

static void
BCMATTACHFN(wlc_chanim_mfree)(osl_t *osh, chanim_info_t *c_info)
{
	wlc_chanim_stats_t *headptr = c_info->stats;
	wlc_chanim_stats_t *curptr;
#ifdef WLCHANIM_US
		wlc_chanim_stats_us_t *head_us = c_info->stats_us;
			wlc_chanim_stats_us_t *cur_us;
#endif /* WLCHANIM_US */
	while (headptr) {
		curptr = headptr;
		headptr = headptr->next;
		MFREE(osh, curptr, sizeof(wlc_chanim_stats_t));
	}
#ifdef WLCHANIM_US
	while (head_us) {
		cur_us = head_us;
		head_us = head_us->next;
		MFREE(osh, cur_us, sizeof(*cur_us));
	}
#endif /* WLCHANIM_US */

	c_info->stats = NULL;
#ifdef WLCHANIM_US
	c_info->stats_us = NULL;
#endif /* WLCHANIM_US */
	MFREE(osh, c_info, sizeof(chanim_info_t));
}

static void
BCMATTACHFN(wlc_lq_chanim_detach)(chanim_info_t *c_info)
{
	wlc_info_t *wlc;
	chanim_interface_info_t *ifaces, *headptr;

	if (c_info == NULL)
	return;

	wlc = c_info->wlc;

	headptr = c_info->free_list;
	while ((ifaces = headptr)) {
		headptr = headptr->next;
		MFREE(wlc->osh, ifaces, sizeof(chanim_interface_info_t));
	}

	headptr = c_info->ifaces;
	while ((ifaces = headptr)) {
		headptr = headptr->next;
		MFREE(wlc->osh, ifaces, sizeof(chanim_interface_info_t));
	}

	wlc_chanim_mfree(wlc->osh, c_info);
}

#endif /* WLCHANIM_DISABLED */

static bool
wlc_lq_chanim_any_if_info_setup(chanim_interface_info_t *ifaces)
{
	return (ifaces->next != NULL);
}

/*
 * the main function for chanim information update
 * it could occur 1) on watchdog 2) on channel switch
 * based on the flag.
 */
int
wlc_lq_chanim_update(wlc_info_t *wlc, chanspec_t chanspec, uint32 flags)
{
	chanim_info_t* c_info = wlc->chanim_info;
	chanim_interface_info_t *if_info, *ifaces = c_info->ifaces;

	if (!WLC_CHANIM_ENAB(wlc->pub)) {
		WL_ERROR(("wl%d: %s: WLC_CHANIM not enabled \n", wlc->pub->unit, __FUNCTION__));
		return BCME_ERROR;
	}
	/* on watchdog trigger */
	if (flags & CHANIM_WD) {
		if (!WLC_CHANIM_MODE_DETECT(c_info) || SCAN_IN_PROGRESS(wlc->scan)) {
			WL_SCAN(("wl%d: %s: WLC_CHANIM upd blocked scan/detect\n", wlc->pub->unit,
				__FUNCTION__));
			return BCME_NOTREADY;
		}

		/*
		 * Cycle through list of interfaces, accumulate matching chanspec and close,
		 * if required.
		 */
		if_info = ifaces;
		while ((if_info = if_info->next)) {
			if (!wf_chspec_malformed(if_info->chanspec)) {
				if (chanspec == if_info->chanspec) {
					wlc_lq_chanim_accum(wlc, if_info->chanspec, &if_info->accum,
						&if_info->acc_us);
				}

				if ((wlc->pub->now % chanim_config(c_info).sample_period) == 0) {
					wlc_lq_chanim_close(wlc, if_info->chanspec,
						&if_info->accum, if_info->stats, &if_info->acc_us,
						if_info->stats_us, FALSE);

#ifdef WLCHANIM_US
					memcpy(&ifaces->acc_us, &if_info->acc_us,
						sizeof(chanim_acc_us_t));
					ifaces->stats_us = if_info->stats_us;
					ifaces->chanspec = if_info->chanspec;
#endif /* WLCHANIM_US */
#ifdef BCMDBG
					if (wlc_lq_chanim_display(wlc, if_info->chanspec,
						if_info->stats) != BCME_OK)
					{
						WL_ERROR(("wl%d: %s: if_info->stats is NULL\n",
							wlc->pub->unit, __FUNCTION__));
					}
#endif // endif
#ifdef WLCHANIM_US
					break;
#endif /* WLCHANIM_US */
				}
			}
		}
	}

	/* on channel switch */
	WL_TSLOG(wlc, __FUNCTION__, TS_ENTER, 0);
	if (flags & CHANIM_CHANSPEC) {
		/* Switching to a new channel */
		if (SCAN_IN_PROGRESS(wlc->scan) || !wlc_lq_chanim_any_if_info_setup(ifaces)) {
			if_info = ifaces;
			wlc_lq_chanim_if_switch_channels(wlc, if_info, chanspec, if_info->chanspec);
		} else {
			if_info = wlc_lq_chanim_if_info_find(ifaces, chanspec);
			if (if_info != NULL) {
				wlc_lq_chanim_accum(wlc, chanspec, &if_info->accum,
					&if_info->acc_us);
			} else {
				if_info = ifaces->next;
				wlc_lq_chanim_if_switch_channels(wlc, if_info, chanspec,
						if_info->chanspec);
				if (wlc->home_chanspec != if_info->chanspec) {
					if_info->chanspec = wlc->home_chanspec;
				}
			}
		}
	}
	WL_TSLOG(wlc, __FUNCTION__, TS_EXIT, 0);
	return BCME_OK;
}

void
wlc_lq_chanim_acc_reset(wlc_info_t *wlc)
{
	chanim_info_t* c_info = wlc->chanim_info;
	chanim_interface_info_t *if_info;

	if_info = wlc_lq_chanim_if_info_find(c_info->ifaces, wlc->chanspec);

	if (if_info != NULL) {
		wlc_lq_chanim_clear_acc(wlc, &if_info->accum, &if_info->acc_us, FALSE);
		/* chanspec is cleared and updated during accumulation again */
		if_info->accum.chanspec = (chanspec_t)INVCHANSPEC;
	}
	bzero((char*)&c_info->last_cnt, sizeof(chanim_cnt_t));
}

static bool
wlc_lq_chanim_interfered_glitch(wlc_chanim_stats_t *stats, uint32 thres)
{
	bool interfered = FALSE;

	interfered = stats->chanim_stats.glitchcnt > thres;
	return interfered;
}

static bool
wlc_lq_chanim_interfered_cca(wlc_chanim_stats_t *stats, uint32 thres)
{
	bool interfered = FALSE;
	uint8 stats_sum;

	stats_sum = stats->chanim_stats.ccastats[CCASTATS_NOPKT];
	interfered = stats_sum > (uint8)thres;

	return interfered;
}

static bool
wlc_lq_chanim_interfered_noise(wlc_chanim_stats_t *stats, int8 thres)
{
	bool interfered = FALSE;
	int8 bgnoise;

	bgnoise = stats->chanim_stats.bgnoise;
	interfered = bgnoise > (uint8)thres;

	return interfered;
}

bool
wlc_lq_chanim_interfered(wlc_info_t *wlc, chanspec_t chanspec)
{
	bool interfered = FALSE;
	wlc_chanim_stats_t *cur_stats;
	chanim_info_t *c_info = wlc->chanim_info;

	if (!WLC_CHANIM_ENAB(wlc->pub)) {
		return FALSE;
	}

	cur_stats = wlc_lq_chanim_chanspec_to_stats(wlc->chanim_info, chanspec, FALSE);

	if (!cur_stats)  {
		WL_INFORM(("%s: no stats allocated for chanspec 0x%x\n",
			__FUNCTION__, chanspec));
		return interfered;
	}

	if (wlc_lq_chanim_interfered_glitch(cur_stats, chanim_config(c_info).crsglitch_thres) ||
		wlc_lq_chanim_interfered_cca(cur_stats, chanim_config(c_info).ccastats_thres) ||
		wlc_lq_chanim_interfered_noise(cur_stats, chanim_config(c_info).bgnoise_thres))
		interfered = TRUE;

	if (chanspec == wlc->home_chanspec)
		chanim_mark(c_info).state = interfered;

	return interfered;
}

#ifdef AP
static void
wlc_lq_chanim_scb_probe(wlc_info_t *wlc, bool activate)
{
	chanim_info_t * c_info = wlc->chanim_info;
	int err;

	ASSERT(AP_ENAB(wlc->pub));

	if (activate) {
		/* store the original values, and replace with the chanim values */
		if ((err = wlc_get(wlc, WLC_GET_SCB_TIMEOUT,
				(int *)&chanim_mark(c_info).scb_timeout)) != BCME_OK ||
		    (err = wlc_iovar_getint(wlc, "scb_probe",
				(int *)&chanim_mark(c_info).scb_max_probe)) != BCME_OK ||
		    (err = wlc_set(wlc, WLC_SET_SCB_TIMEOUT,
				chanim_config(c_info).sample_period)) != BCME_OK ||
		    (err = wlc_iovar_setint(wlc, "scb_probe",
				chanim_config(c_info).scb_max_probe)) != BCME_OK) {
			WL_ERROR(("wl%d: %s: iovar/ioctl failure, activate %d, err %d\n",
				wlc->pub->unit, __FUNCTION__, activate, err));
			/* error handling */
		}
	}
	else {
		/* swap back on exit */
		if ((err = wlc_set(wlc, WLC_SET_SCB_TIMEOUT,
				chanim_mark(c_info).scb_timeout)) != BCME_OK ||
		    (err = wlc_iovar_setint(wlc, "scb_probe",
				chanim_mark(c_info).scb_max_probe)) != BCME_OK) {
			WL_ERROR(("wl%d: %s: iovar/ioctl failure, activate %d, err %d\n",
				wlc->pub->unit, __FUNCTION__, activate, err));
			/* error handling */
		}
	}
}
#endif /* AP */

void
wlc_lq_chanim_upd_act(wlc_info_t *wlc)
{
	chanim_info_t * c_info = wlc->chanim_info;

	if (!WLC_CHANIM_ENAB(wlc->pub)) {
		return;
	}

	if (wlc_lq_chanim_interfered(wlc, wlc->home_chanspec) &&
		(wlc->chanspec == wlc->home_chanspec)) {
		if (chanim_mark(c_info).detecttime && !WLC_CHANIM_ACT(c_info)) {
			if ((wlc->pub->now - chanim_mark(c_info).detecttime) >
				(uint)chanim_act_delay(c_info)) {
			    chanim_mark(c_info).flags |= CHANIM_ACTON;
				WL_CHANINT(("***chanim action set\n"));
			}
		}
		else if (!WLC_CHANIM_ACT(c_info)) {
			chanim_mark(c_info).detecttime = wlc->pub->now;
#ifdef AP
			/* start to probe */
			wlc_lq_chanim_scb_probe(wlc, TRUE);
#endif /* AP */
		}
	}
	else {
#ifdef AP
		if (chanim_mark(c_info).detecttime)
			wlc_lq_chanim_scb_probe(wlc, FALSE);
#endif /* AP */
		chanim_mark(c_info).detecttime = 0;
		chanim_mark(c_info).flags &= ~CHANIM_ACTON;
	}
}

void
wlc_lq_chanim_upd_acs_record(chanim_info_t *c_info, chanspec_t home_chspc,
	chanspec_t selected, uint8 trigger)
{
	chanim_acs_record_t* cur_record = &c_info->record[chanim_mark(c_info).record_idx];
	wlc_chanim_stats_t *cur_stats = wlc_lq_chanim_chanspec_to_stats(c_info, home_chspc, FALSE);

	if (WLC_CHANIM_MODE_EXT(c_info))
		return;

	bzero(cur_record, sizeof(chanim_acs_record_t));

	cur_record->trigger = trigger;
	cur_record->timestamp = OSL_SYSUPTIME();
	cur_record->selected_chspc = selected;
	cur_record->valid = TRUE;

	if (cur_stats) {
		cur_record->glitch_cnt = cur_stats->chanim_stats.glitchcnt;
		cur_record->ccastats = cur_stats->chanim_stats.ccastats[CCASTATS_NOPKT];
	}

	chanim_mark(c_info).record_idx ++;
	if (chanim_mark(c_info).record_idx == CHANIM_ACS_RECORD)
		chanim_mark(c_info).record_idx = 0;
}

static int
wlc_lq_chanim_get_acs_record(chanim_info_t *c_info, int buf_len, void *output)
{
	wl_acs_record_t *record = (wl_acs_record_t *)output;
	uint8 idx = chanim_mark(c_info).record_idx;
	int i, count = 0;

	if (WLC_CHANIM_MODE_EXT(c_info))
		return BCME_OK;

	for (i = 0; i < CHANIM_ACS_RECORD; i++) {
		if (c_info->record[idx].valid) {
			bcopy(&c_info->record[idx], &record->acs_record[i],
				sizeof(chanim_acs_record_t));
			count++;
		}
		idx = (idx + 1) % CHANIM_ACS_RECORD;
	}

	record->count = (uint8)count;
	record->timestamp = OSL_SYSUPTIME();
	return BCME_OK;
}

#ifdef WLCHANIM_US
static int
wlc_lq_chanim_get_stats_us(chanim_info_t *c_info, wl_chanim_stats_us_v2_t* iob,
	int *len, int cnt, uint32 dur)
{
	uint32 count = 0;
	uint32 datalen;
	wlc_chanim_stats_us_t* stats_us = NULL;
	int bcmerror = BCME_OK;
	int buflen = *len;
	wlc_chanim_stats_us_t cur_stats_us;
	datalen = WL_CHANIM_STATS_US_FIXED_LEN;

	if (cnt == WL_CHANIM_COUNT_US_ALL) {
		stats_us = c_info->stats_us;
	} else if (cnt == WL_CHANIM_COUNT_US_ONE) {
		chanim_interface_info_t *if_info = c_info->ifaces;
		/*
		 * There are multiple i/f. To preserve original behavior, find the i/f that matches
		 * home chanspec.
		 */
		while ((if_info = if_info->next)) {
			if (c_info->wlc->home_chanspec == if_info->chanspec) {
				cur_stats_us = *if_info->stats_us;
				cur_stats_us.next = NULL;
				stats_us = &cur_stats_us;
				break;
			}
		}
	} else if (cnt == WL_CHANIM_COUNT_US_RESET) {
		int i;
		chanim_cnt_us_t *last_cnt_us;
		last_cnt_us = &c_info->last_cnt_us;
		stats_us = c_info->stats_us;
		while (stats_us) {
			chanspec_t  chanspec_temp;
			chanim_interface_info_t *if_info = c_info->ifaces;
			while ((if_info = if_info->next)) {
				chanim_acc_us_t *acc_us = &if_info->acc_us;
				if (c_info->wlc->home_chanspec == if_info->chanspec) {
					if (acc_us)  {
						acc_us->busy_tm = 0;
						acc_us->rxcrs_pri20 = 0;
						acc_us->rxcrs_sec20 = 0;
						acc_us->rxcrs_sec40 = 0;

						for (i = 0; i < CCASTATS_MAX; i++) {
							acc_us->ccastats_us[i] = 0;
						}
					}
					break;
				}
			}
			chanspec_temp = stats_us->chanim_stats_us.chanspec;
			bzero(&stats_us->chanim_stats_us, sizeof(chanim_stats_us_v2_t));
			stats_us->chanim_stats_us.chanspec = chanspec_temp;
			last_cnt_us->start_tm = OSL_SYSUPTIME();
			last_cnt_us->total_tm = 0;
			stats_us = stats_us->next;
		}
		iob->count = WL_CHANIM_COUNT_US_RESET;
		iob->version = WL_CHANIM_STATS_US_VERSION;
		return bcmerror;
	} else if (cnt == WL_CHANIM_US_DUR) {
		chanim_interface_info_t *if_info = c_info->ifaces;
		while ((if_info = if_info->next)) {
			if (c_info->wlc->home_chanspec == if_info->chanspec) {
				bzero(&if_info->chanim_stats_us, sizeof(chanim_stats_us_v2_t));
				wlc_lq_chanim_accum(c_info->wlc, if_info->chanspec, &if_info->accum,
					&if_info->acc_us);
				if_info->chanim_stats_us.total_tm = dur * 1000;
				if_info->chanim_stats_us.chanspec = if_info->chanspec;
				break;
			}
		}
		wl_del_timer(c_info->wlc->wl, c_info->chanim_timer);
		wl_add_timer(c_info->wlc->wl, c_info->chanim_timer, dur, 0);
		iob->version = WL_CHANIM_STATS_US_VERSION;
		return bcmerror;
	} else if (cnt == WL_CHANIM_US_DUR_GET) {
		chanim_interface_info_t *if_info = c_info->ifaces;
		while ((if_info = if_info->next)) {
			if (c_info->wlc->home_chanspec == if_info->chanspec) {
				bcopy(&if_info->chanim_stats_us, &iob->stats_us_v2[count],
						sizeof(chanim_stats_us_v2_t));
				count++;
				datalen += sizeof(chanim_stats_us_v2_t);
				buflen -= sizeof(chanim_stats_us_v2_t);
				iob->count = count;
				iob->buflen = datalen;
				iob->version = WL_CHANIM_STATS_US_VERSION;
				return bcmerror;
			}
		}

	}
	if (stats_us == NULL) {
		memset(&iob->stats_us_v2[0], 0, sizeof(chanim_stats_us_v2_t));
		count = 0;
	} else {
		while (stats_us) {
			if (buflen < (int)sizeof(chanim_stats_us_v2_t)) {
				bcmerror = BCME_BUFTOOSHORT;
				break;
			}
			bcopy(&stats_us->chanim_stats_us, &iob->stats_us_v2[count],
					sizeof(chanim_stats_us_v2_t));
			count++;
			datalen += sizeof(chanim_stats_us_v2_t);
			buflen -= sizeof(chanim_stats_us_v2_t);

			stats_us = stats_us->next;
		}
	}

	iob->count = count;
	iob->buflen = datalen;
	iob->version = WL_CHANIM_STATS_US_VERSION;

	return bcmerror;
}

#ifndef WLCHANIM_DISABLED
static void wlc_chanim_msec_timeout(void *arg)
{
	wlc_info_t *wlc = (wlc_info_t *)arg;
	chanim_info_t* c_info = wlc->chanim_info;
	chanim_interface_info_t *if_info = c_info->ifaces;
	while ((if_info = if_info->next)) {
		if (c_info->wlc->home_chanspec == if_info->chanspec) {
			int i;
			bcopy(&if_info->acc_us, &if_info->last_acc_us,  sizeof(chanim_acc_us_t));
			wlc_lq_chanim_accum(c_info->wlc, if_info->chanspec, &if_info->accum,
					&if_info->acc_us);
			if_info->chanim_stats_us.chanspec = if_info->chanspec;
			if_info->chanim_stats_us.busy_tm =
					if_info->acc_us.busy_tm - if_info->last_acc_us.busy_tm;
			for (i = 0; i < CCASTATS_MAX; i++) {
				if_info->chanim_stats_us.ccastats_us[i]	=
					if_info->acc_us.ccastats_us[i]-
					if_info->last_acc_us.ccastats_us[i];
			}
			if_info->chanim_stats_us.rxcrs_pri20 = if_info->acc_us.rxcrs_pri20 -
				if_info->last_acc_us.rxcrs_pri20;
			if_info->chanim_stats_us.rxcrs_sec20 = if_info->acc_us.rxcrs_sec20 -
				if_info->last_acc_us.rxcrs_sec20;
			if_info->chanim_stats_us.rxcrs_sec40 = if_info->acc_us.rxcrs_sec40 -
				if_info->last_acc_us.rxcrs_sec40;
			break;
		}
	}
}
#endif /* WLCHANIM_DISABLED */
#endif /* WLCHANIM_US */

static int
wlc_lq_chanim_get_stats(chanim_info_t *c_info, wl_chanim_stats_t* iob, int *len, int cnt)
{
	uint32 count = 0;
	uint32 datalen;
	wlc_chanim_stats_t* stats = NULL;
	int bcmerror = BCME_OK;
	int buflen = *len;
	wlc_chanim_stats_t cur_stats;

	iob->version = WL_CHANIM_STATS_VERSION;
	datalen = WL_CHANIM_STATS_FIXED_LEN;

	if (cnt == WL_CHANIM_COUNT_ALL)
		stats = c_info->stats;
	else {
		chanim_interface_info_t *if_info = c_info->ifaces;

		/*
		 * There are multiple i/f. To preserve original behavior, find the i/f that matches
		 * home chanspec.
		 */
		while ((if_info)) {
			if (c_info->wlc->home_chanspec == if_info->chanspec) {
				cur_stats = *if_info->stats;
				cur_stats.next = NULL;
				stats = &cur_stats;
			}
			if_info = if_info->next;
		}
	}

	if (stats == NULL) {
		memset(&iob->stats[0], 0, sizeof(chanim_stats_t));
		count = 0;
	} else {
		while (stats) {
			if (buflen < (int)sizeof(chanim_stats_t)) {
				bcmerror = BCME_BUFTOOSHORT;
				break;
			}
			bcopy(&stats->chanim_stats, &iob->stats[count],
					sizeof(chanim_stats_t));

			count++;
			stats = stats->next;
			datalen += sizeof(chanim_stats_t);
			buflen -= sizeof(chanim_stats_t);
		}
	}

	iob->count = count;
	iob->buflen = datalen;

	return bcmerror;
}

#ifdef APCS
static bool
chanim_chk_lockout(chanim_info_t *c_info)
{
	uint8 cur_idx = chanim_mark(c_info).record_idx;
	uint8 start_idx;
	chanim_acs_record_t *start_record;
	uint32 cur_time;

	if (!chanim_config(c_info).max_acs)
		return TRUE;

	start_idx = MODSUB(cur_idx, chanim_config(c_info).max_acs, CHANIM_ACS_RECORD);
	start_record = &c_info->record[start_idx];
	cur_time = OSL_SYSUPTIME();

	if (start_record->valid && ((cur_time - start_record->timestamp) <
			chanim_config(c_info).lockout_period * 1000)) {
		WL_CHANINT(("***chanim lockout true\n"));
		return TRUE;
	}

	return FALSE;
}

/* function for chanim mitigation (action) */
void
wlc_lq_chanim_action(wlc_info_t *wlc)
{
	chanim_info_t *c_info = wlc->chanim_info;
	struct scb *scb;
	struct scb_iter scbiter;

	if (!WLC_CHANIM_ENAB(wlc->pub)) {
		return;
	}

	/* clear the action flag and reset detecttime */
	chanim_mark(c_info).flags &= ~CHANIM_ACTON;
	chanim_mark(c_info).detecttime = 0;
#ifdef AP
	wlc_lq_chanim_scb_probe(wlc, FALSE);
#endif /* AP */

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (SCB_ASSOCIATED(scb) &&
		    (wlc->pub->now - scb->used < (uint)chanim_act_delay(c_info)))
			break;
	}

	if (!scb) {
		wl_uint32_list_t request;

		if (chanim_chk_lockout(c_info)) {
			WL_CHANINT(("***chanim scan is not allowed due to lockout\n"));
			return;
		}

		request.count = 0;

		if (APCS_ENAB(wlc->pub)) {
			(void)wlc_cs_scan_start(wlc->cfg, &request, TRUE, FALSE, TRUE,
				wlc->band->bandtype, APCS_CHANIM, NULL, NULL);
		}
	}
	return;
}
#endif /* APCS */

bool
wlc_lq_chanim_act(chanim_info_t *c_info)
{
	return (chanim_mark(c_info).flags & CHANIM_ACTON) != 0;
}

bool
wlc_lq_chanim_mode_act(chanim_info_t *c_info)
{
	return (chanim_config(c_info).mode >= CHANIM_ACT);
}

bool
wlc_lq_chanim_mode_ext(chanim_info_t *c_info)
{
	return (chanim_config(c_info).mode == CHANIM_EXT);
}

bool
wlc_lq_chanim_mode_detect(chanim_info_t *c_info)
{
	return (chanim_config(c_info).mode >= CHANIM_DETECT);
}
#endif /* WLCHANIM */

typedef struct wlc_lq_stats_notif {
	wlc_bmac_obss_counts_t *init_stats;	/* stats at the time when notif reg is called. */
	wlc_info_t *wlc;
	uint32 req_time_ms;
	stats_cb cb;
	uint16 connID;
	void *arg;
	struct wl_timer *notif_timer;
} wlc_lq_stats_notif_t;

static void wlc_lq_register_obss_stats_cleanup(wlc_info_t *wlc, wlc_lq_stats_notif_t *notif_ctx);
static void wlc_lq_notif_timer(void *arg);

static void
wlc_lq_register_obss_stats_cleanup(wlc_info_t *wlc, wlc_lq_stats_notif_t *notif_ctx)
{
	if (notif_ctx) {
		if (notif_ctx->notif_timer) {
			wl_del_timer(wlc->wl, notif_ctx->notif_timer);
			wl_free_timer(wlc->wl, notif_ctx->notif_timer);
		}
		if (notif_ctx->init_stats)
			MFREE(wlc->osh, notif_ctx->init_stats,
				sizeof(wlc_bmac_obss_counts_t));
		MFREE(wlc->osh, notif_ctx, sizeof(wlc_lq_stats_notif_t));
	}
}

/* This function checks to see if the interface is up.
* It computes the bsscfg from the connID and checks if the
* interface is up.
*/
static bool
wlc_bsscfg_is_intf_up(wlc_info_t *wlc, uint16 connID)
{
	wlc_bsscfg_t *cfg;

	if (wlc->pub->up == FALSE)
		return FALSE;

	cfg = wlc_bsscfg_find_by_ID(wlc, connID);

	if (cfg == NULL || cfg->up == FALSE)
		return FALSE;

	return TRUE;
}

static void
wlc_lq_notif_timer(void *arg)
{
	wlc_lq_stats_notif_t *notif_ctx = (wlc_lq_stats_notif_t *)arg;
	wlc_bmac_obss_counts_t curr;
	wlc_bmac_obss_counts_t *prev = notif_ctx->init_stats;
	wlc_bmac_obss_counts_t delta;

	if (wlc_bsscfg_is_intf_up(notif_ctx->wlc, notif_ctx->connID) ==
		FALSE) {
		(notif_ctx->cb)(notif_ctx->wlc, notif_ctx->arg,
			notif_ctx->req_time_ms, NULL);
		goto fail;
	}
	(void)memset(&curr, 0, sizeof(curr));
	/* get cur_statsent stats in  notif_ctx->init_stats */
	wlc_bmac_obss_stats_read(notif_ctx->wlc->hw, &curr);

	/* calc delta */
	delta.usecs = curr.usecs - prev->usecs;
	delta.txdur = curr.txdur - prev->txdur;
	delta.ibss = curr.ibss - prev->ibss;
	delta.obss = curr.obss - prev->obss;
	delta.noctg = curr.noctg - prev->noctg;
	delta.nopkt = curr.nopkt - prev->nopkt;
	delta.PM = curr.PM - prev->PM;
	delta.txopp = curr.txopp - prev->txopp;
	delta.slot_time_txop = curr.slot_time_txop;
#if defined(BCMDBG)
	delta.gdtxdur = curr.gdtxdur - prev->gdtxdur;
	delta.bdtxdur = curr.bdtxdur - prev->bdtxdur;
#endif // endif

#ifdef WL_OBSS_DYNBW
	if (WLC_OBSS_DYNBW_ENAB(notif_ctx->wlc->pub)) {
		if (D11REV_GE(notif_ctx->wlc->pub->corerev, 40)) {
			delta.rxdrop20s = curr.rxdrop20s - prev->rxdrop20s;
			delta.rx20s = curr.rx20s - prev->rx20s;

			delta.rxcrs_pri = curr.rxcrs_pri - prev->rxcrs_pri;
			delta.rxcrs_sec20 = curr.rxcrs_sec20 - prev->rxcrs_sec20;
			delta.rxcrs_sec40 = curr.rxcrs_sec40 - prev->rxcrs_sec40;

			delta.sec_rssi_hist_hi = curr.sec_rssi_hist_hi - prev->sec_rssi_hist_hi;
			delta.sec_rssi_hist_med = curr.sec_rssi_hist_med - prev->sec_rssi_hist_med;
			delta.sec_rssi_hist_low = curr.sec_rssi_hist_low - prev->sec_rssi_hist_low;
		}
	}
#endif /* WL_OBSS_DYNBW */
	/* call back using cb with the delta stats */
	(notif_ctx->cb)(notif_ctx->wlc, notif_ctx->arg, notif_ctx->req_time_ms, &delta);

fail:
	/* free the notif_ctx */
	wlc_lq_register_obss_stats_cleanup(notif_ctx->wlc, notif_ctx);
}

int
wlc_lq_register_dynbw_stats_cb(wlc_info_t *wlc, uint32 req_time_ms, stats_cb cb,
	uint16 connID, void *arg)
{
	wlc_lq_stats_notif_t *notif_ctx = NULL;
	int err = BCME_OK;

	/* if the radio/intf is down, clear the context and signal failure
	*/
	if (wlc_bsscfg_is_intf_up(wlc, connID) ==
		FALSE) {
		(cb)(wlc, arg, req_time_ms, NULL);
		goto cb_fail;
	}

	notif_ctx = (wlc_lq_stats_notif_t *) MALLOCZ(wlc->osh, sizeof(wlc_lq_stats_notif_t));

	if (notif_ctx == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced bytes\n",
			wlc->pub->unit,
			__FUNCTION__));
		err = BCME_NOMEM;
		goto cb_fail;
	}
	notif_ctx->cb = cb;
	notif_ctx->arg = arg;
	notif_ctx->req_time_ms = req_time_ms;
	notif_ctx->wlc = wlc;
	notif_ctx->connID = connID;
	notif_ctx->init_stats = (wlc_bmac_obss_counts_t *) MALLOCZ(wlc->osh,
		sizeof(wlc_bmac_obss_counts_t));

	if (notif_ctx->init_stats == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced bytes\n",
			wlc->pub->unit,
			__FUNCTION__));
		err = BCME_NOMEM;
		goto cb_fail;
	}

	notif_ctx->notif_timer = wl_init_timer(wlc->wl, wlc_lq_notif_timer, notif_ctx, "lq_notif");

	if (!notif_ctx->notif_timer) {
		err = BCME_NOMEM;
		goto cb_fail;
	}

	/* store current stats in  notif_ctx->init_stats */
	wlc_bmac_obss_stats_read(wlc->hw, notif_ctx->init_stats);

	/* start timer */
	wl_add_timer(wlc->wl, notif_ctx->notif_timer, req_time_ms, 0);

	return BCME_OK;

cb_fail:
	wlc_lq_register_obss_stats_cleanup(wlc, notif_ctx);
	return err;
}

#ifdef WLCQ
static int
wlc_lq_channel_qa_eval(wlc_info_t *wlc)
{
	wlc_lq_info_t *lqi = wlc->lqi;
	int k;
	int sample_count;
	int rssi_avg;
	int noise_est;
	int quality_metric;

	sample_count = (int)lqi->channel_qa_sample_num;
	rssi_avg = 0;
	for (k = 0; k < sample_count; k++)
		rssi_avg += lqi->channel_qa_sample[k];
	rssi_avg = (rssi_avg + sample_count/2) / sample_count;

	noise_est = rssi_avg;

	if (noise_est < -85)
		quality_metric = 3;
	else if (noise_est < -75)
		quality_metric = 2;
	else if (noise_est < -65)
		quality_metric = 1;
	else
		quality_metric = 0;

	WL_INFORM(("wl%d: %s: samples rssi {%d %d} avg %d qa %d\n",
		wlc->pub->unit, __FUNCTION__,
		lqi->channel_qa_sample[0], lqi->channel_qa_sample[1],
		rssi_avg, quality_metric));

	return (quality_metric);
}

/* this callback chain must defer calling phy_noise_sample_request */
static void
wlc_lq_channel_qa_sample_cb(wlc_info_t *wlc, uint8 channel, int8 noise_dbm)
{
	wlc_lq_info_t *lqi = wlc->lqi;
	bool moretest = FALSE;

	if (!lqi->channel_qa_active)
		return;

	if (channel != lqi->channel_qa_channel) {
		/* bad channel, try again */
		WL_INFORM(("wl%d: %s: retry, samples from channel %d instead of channel %d\n",
			wlc->pub->unit, __FUNCTION__, channel, lqi->channel_qa_channel));
		moretest = TRUE;
	} else {
		/* save the sample */
		lqi->channel_qa_sample[lqi->channel_qa_sample_num++] = (int8)noise_dbm;
		if (lqi->channel_qa_sample_num < WLC_CHANNEL_QA_NSAMP) {
			/* still need more samples */
			moretest = TRUE;
		} else {
			/* done with the channel quality measurement */
			lqi->channel_qa_active = FALSE;

			/* evaluate the samples to a quality metric */
			lqi->channel_quality = wlc_lq_channel_qa_eval(wlc);
		}
	}

	if (moretest)
		wlc_lq_channel_qa_sample_req(wlc);

}

static int
wlc_lq_channel_qa_start(wlc_info_t *wlc)
{
	wlc_lq_info_t *lqi = wlc->lqi;

	/* do nothing if there is already a request for a measurement */
	if (lqi->channel_qa_active)
		return 0;

	WL_INFORM(("wl%d: %s: starting qa measure\n", wlc->pub->unit, __FUNCTION__));

	lqi->channel_qa_active = TRUE;

	lqi->channel_quality = -1;	/* clear to invalid value */
	lqi->channel_qa_sample_num = 0;	/* clear the sample array */

	wlc_lq_channel_qa_sample_req(wlc);

	return 0;
}

void
wlc_lq_channel_qa_sample_req(wlc_info_t *wlc)
{
	wlc_lq_info_t *lqi = wlc->lqi;

	if (!lqi->channel_qa_active)
		return;

	/* wait until after a scan if one is in progress */
	if (SCAN_IN_PROGRESS(wlc->scan)) {
		WL_NONE(("wl%d: %s: deferring sample request until after scan\n", wlc->pub->unit,
			__FUNCTION__));
		return;
	}

	lqi->channel_qa_channel = CHSPEC_CHANNEL(WLC_BAND_PI_RADIO_CHANSPEC);

	WL_NONE(("wl%d: %s(): requesting samples for channel %d\n", wlc->pub->unit,
		__FUNCTION__, lqi->channel_qa_channel));

	WL_INFORM(("wlc_noise_cb(): WLC_NOISE_REQUEST_CQ.\n"));

	wlc_lq_noise_sample_request(wlc, WLC_NOISE_REQUEST_CQ, lqi->channel_qa_channel);

}
#endif /* defined(WLCQ)  */

void
wlc_lq_noise_cb(wlc_info_t *wlc, uint8 channel, int8 noise_dbm)
{
	wlc_lq_info_t *lqi = wlc->lqi;

	if (lqi->noise_req & WLC_NOISE_REQUEST_SCAN) {
		/* TODO - probe responses may have been constructed, fixup those dummy values
		 *  if being blocked by CQRM sampling at different channels, make another request
		 *  if we are still in the requested scan channel and scan hasn't finished yet
		 */

		lqi->noise_req &= ~WLC_NOISE_REQUEST_SCAN;
	}

#ifdef WLCQ
	if (lqi->noise_req & WLC_NOISE_REQUEST_CQ) {
		lqi->noise_req &= ~WLC_NOISE_REQUEST_CQ;
		WL_INFORM(("wlc_noise_cb(): WLC_NOISE_REQUEST_CQ.\n"));
		wlc_lq_channel_qa_sample_cb(wlc, channel, noise_dbm);
	}
#else
	BCM_REFERENCE(channel);
#endif // endif

#if defined(STA) && defined(WLRM)
	if (lqi->noise_req & WLC_NOISE_REQUEST_RM) {
		lqi->noise_req &= ~WLC_NOISE_REQUEST_RM;
		WL_INFORM(("wlc_noise_cb(): WLC_NOISE_REQUEST_RM.\n"));
		if (WLRM_ENAB(wlc->pub) && wlc->rm_info->rm_state->rpi_active) {
			if (wlc_rm_rpi_sample(wlc->rm_info, noise_dbm))
				wlc_lq_noise_sample_request(wlc, WLC_NOISE_REQUEST_RM,
						CHSPEC_CHANNEL(WLC_BAND_PI_RADIO_CHANSPEC));
		}
	}
#else
	BCM_REFERENCE(noise_dbm);
#endif // endif

	return;

}

void
wlc_lq_noise_sample_request(wlc_info_t *wlc, uint8 request, uint8 channel)
{
	wlc_lq_info_t *lqi = wlc->lqi;
	bool sampling_in_progress = (lqi->noise_req != 0);

	WL_TRACE(("%s(): request=%d, channel=%d\n", __FUNCTION__, request, channel));
	WL_EAP_TRC_SCAN(("%s(): request=%d, channel=%d\n", __FUNCTION__, request, channel));

	switch (request) {
	case WLC_NOISE_REQUEST_SCAN:
		lqi->noise_req |= WLC_NOISE_REQUEST_SCAN;
		break;

	case WLC_NOISE_REQUEST_CQ:

		lqi->noise_req |= WLC_NOISE_REQUEST_CQ;
		break;

	case WLC_NOISE_REQUEST_RM:

		lqi->noise_req |= WLC_NOISE_REQUEST_RM;
		break;

	default:
		ASSERT(0);
		break;
	}

	if (sampling_in_progress) {
		return;
	}

	phy_noise_sample_request_external(WLC_PI(wlc));

	return;
}

int8
wlc_lq_read_noise_lte(wlc_info_t *wlc)
{
	wlc_lq_info_t *lqi = wlc->lqi;
	int8 nval = lqi->noise_lte;
	return nval;
}

#ifdef WLCHANIM
/* Get tx + rx load */
/* USED by RSDB PM module to decide on the modeswitch */
uint32
wlc_rsdb_get_lq_load(wlc_info_t *wlc)
{
	wlc_chanim_stats_t *stats = NULL;
	uint32 good_tx = 0;
	uint32 inbss_rx = 0;
	uint32 load = 0;

	/* Update the stats */
	stats = wlc_lq_chanim_chanspec_to_stats(wlc->chanim_info, wlc->chanspec, FALSE);

	/* check for valid stats */
	if (stats == NULL) {
		WL_ERROR(("%s: LQ stats not initialized \n", __FUNCTION__));
		ASSERT(0);
		return load;
	}

	good_tx = stats->chanim_stats.ccastats[CCASTATS_GDTXDUR];
	inbss_rx = stats->chanim_stats.ccastats[CCASTATS_INBSS];

	/* gross Load */
	load = good_tx + inbss_rx;
	WL_TRACE(("%s : Chanspec %x TX dur : %d   RX inbss %d:  load %d \n",
		__FUNCTION__, wlc->chanspec, good_tx, inbss_rx, load));

	return load;
}
#endif /* WLCHANIM */

#if defined(ECOUNTERS) && defined(EVENT_LOG_COMPILE)

/* LQM is on an interface specified by the configuration */
static int wlc_ecounters_rx_lqm(uint16 stats_type, void *context,
	const ecounters_stats_types_report_req_t *req,
	struct bcm_xtlvbuf *xtlvbuf, uint32 *cookie, const bcm_xtlv_t *tlv, uint16 *awl)
{
	wlc_info_t *wlc = (wlc_info_t*)context;
	wlc_if_t *wlcif;
	wlc_bsscfg_t *bsscfg;
	wl_rx_signal_metric_t *sm;
	wl_lqm_t lqm;

	/* stats type must be LQM inorder to proceed */
	if (stats_type != WL_IFSTATS_XTLV_IF_LQM) {
		return BCME_BADARG;
	}

	/* In this case we exactly know how much data to send.
	 * Before we start collecting data, check if there is adequate
	 * space available in the buffer
	 */
	 if (bcm_xtlv_buf_rlen(xtlvbuf) < (sizeof(lqm) + BCM_XTLV_HDR_SIZE)) {
		/* We need this much space */
		*awl = sizeof(lqm) + BCM_XTLV_HDR_SIZE;
		return BCME_BUFTOOSHORT;
	 }

	/* Get the wlcif for the associated interface */
	/* Note that LQM values are provided for STA BSSCFG only */
	wlcif = wlcif_get_by_ifindex(wlc, req->if_index);

	if (wlcif == NULL) {
		return BCME_UNSUPPORTED;
	}

	/* Get the bsscfg associated with the wlcif */
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	lqm.version = WL_LQM_VERSION_1;
	lqm.noise_level = bsscfg->wlc->lqi->noise;
	lqm.flags = 0;

	if (BSSCFG_STA(bsscfg) && !BSSCFG_IBSS(bsscfg)) {
		if (wlc_bsscfg_is_associated(bsscfg))
		{
			lqm.flags |= WL_LQM_CURRENT_BSS_VALID;
			sm = &lqm.current_bss;

			sm->rssi = bsscfg->link->rssi;
			sm->snr = bsscfg->link->snr;
			sm->chanspec = bsscfg->current_bss->chanspec;
			memcpy(&sm->BSSID, &bsscfg->current_bss->BSSID,
				ETHER_ADDR_LEN);
		}

		if (wlc_assoc_is_associating(bsscfg))
		{
			lqm.flags |= WL_LQM_TARGET_BSS_VALID;
			sm = &lqm.target_bss;
			sm->rssi = bsscfg->target_bss->RSSI;
			sm->snr = bsscfg->target_bss->SNR;
			sm->chanspec = bsscfg->target_bss->chanspec;
			memcpy(&sm->BSSID, &bsscfg->target_bss->BSSID,
				ETHER_ADDR_LEN);
		}
	}

	return bcm_xtlv_put_data(xtlvbuf, stats_type,
		(const uint8 *)&lqm, sizeof(lqm));
}
#endif /* ifdef ECOUNTERS */

void
wlc_lq_rssi_ant_get_api(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, int8 *rssi)
{
	int i;
	struct ether_addr *ea;
	struct scb *scb;
	for (i = WL_ANT_IDX_1; i < WL_RSSI_ANT_MAX; i++) {
		rssi[i] = WLC_RSSI_INVALID;
	}
	if (BSSCFG_STA(bsscfg) && bsscfg->BSS) {
		ea = &bsscfg->BSSID;
		if ((scb = wlc_scbfind(wlc, bsscfg, ea)) != NULL) {
			for (i = WL_ANT_IDX_1; i < WL_RSSI_ANT_MAX; i++) {
				if (wlc->stf->rxchain & (1 << i)) {
					rssi[i] = wlc_lq_ant_rssi_get(wlc, bsscfg, scb, i);
				}
			}
		}
	}
}
