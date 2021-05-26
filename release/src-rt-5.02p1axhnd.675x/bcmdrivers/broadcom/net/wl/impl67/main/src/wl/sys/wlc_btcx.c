/*
 * BT Coex module
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
 * $Id: wlc_btcx.c 790656 2020-08-30 17:35:19Z $
 */

/**
 * @file
 * @brief
 * XXX Twiki: [BTCoexistenceHardware] [SoftwareApplicationNotes] [UcodeBTCoExistence]
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcm_math.h>
#include <bcmutils.h>
#include <siutils.h>
#include <sbchipc.h>
#include <sbgci.h>
#include <bcmendian.h>
#include <802.11.h>
#include <wlioctl.h>
#include <bcmwpa.h>
#include <bcmwifi_channels.h>
#include <bcmdevs.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_channel.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wlc_btcx.h>
#include <wlc_scan.h>
#include <wlc_assoc.h>
#include <wlc_bmac.h>
#include <wlc_ap.h>
#include <wlc_stf.h>
#include <wlc_ampdu.h>
#include <wlc_ampdu_rx.h>
#include <wlc_ampdu_cmn.h>
#ifdef WLMCNX
#include <wlc_mcnx.h>
#endif // endif
#include <wlc_hw_priv.h>

#ifdef BCMLTECOEX
#include <wlc_ltecx.h>
#endif /* BCMLTECOEX */
#include <wlc_ie_mgmt.h>
#include <wlc_ie_mgmt_ft.h>
#include <wlc_ie_mgmt_vs.h>
#include <wlc_rspec.h>
#include <wlc_lq.h>
#include <wlc_dump.h>
#include <wlc_iocv.h>

#include <wlc_event_utils.h>
#include <phy_btcx_api.h>
#include <phy_utils_api.h>
#ifdef WL11AX
#include <wlc_he.h>
#endif // endif

/* Defines */
#define BTC_BTRSSI_THRESH	-70 /* btrssi thresh for implibit mode switch */
/* actual btrssi = -1 * (btrssi_from_ucode * btrssi_step + btrssi_offset) */
#define BTC_BTRSSI_STEP		5
#define BTC_BTRSSI_OFFSET	10
#define BTC_BTRSSI_INVALID	0   /* invalid btrssi */
#define BTC_BTRSSI_MAX_SAMP		16  /* max number of btrssi samples for moving avg. */
#define BTC_BTRSSI_MIN_SAMP	4   /* min number of btrssi samples for moving avg. */

#define BTC_SISOACK_CORES_MASK	0x00FF
#define BTC_SISOACK_TXPWR_MASK	0xFF00

#ifndef MSECS_PER_SEC
#define MSECS_PER_SEC 1000
#endif /* MSECS_PER_SEC */

#define BLE_DEBOUNCE_ON_SECS	3
#define BLE_DEBOUNCE_OFF_SECS	20
#define A2DP_DEBOUNCE_ON_SECS	1
#define A2DP_DEBOUNCE_OFF_SECS	10

#define BT_AMPDU_THRESH		21000	/* if BT period < this threshold, turn off ampdu */
/* RX aggregation size (high/low) is decided based on this threshold */
#define BT_AMPDU_RESIZE_THRESH	7500

#define BTC_BLE_GRANTS_DISABLE	0

/* ENUMs */
enum {
	IOV_BTC_MODE =			0,	/* BT Coexistence mode */
	IOV_BTC_WIRE =			1,	/* BTC number of wires */
	IOV_BTC_STUCK_WAR =		2,	/* BTC stuck WAR */
	IOV_BTC_FLAGS =			3,	/* BT Coex ucode flags */
	IOV_BTC_PARAMS =		4,	/* BT Coex shared memory parameters */
	IOV_COEX_DEBUG_GPIO_ENABLE =	5,
	IOV_BTC_SISO_ACK =		7,	/* Specify SISO ACK antenna, disabled when 0 */
	IOV_BTC_RXGAIN_THRESH =		8,	/* Specify restage rxgain thresholds */
	IOV_BTC_RXGAIN_FORCE =		9,	/* Force for BTC restage rxgain */
	IOV_BTC_RXGAIN_LEVEL =		10,	/* Set the BTC restage rxgain level */
	IOV_BTC_DYNCTL =		11,	/* get/set coex dynctl profile */
	IOV_BTC_DYNCTL_STATUS =		12,	/* get dynctl status: dsns, btpwr,
						   wlrssi, btc_mode, etc
						*/
	IOV_BTC_DYNCTL_SIM =		13,	/* en/dis & config dynctl algo simulation mode */
	IOV_BTC_STATUS =		14,	/* bitmap of current bt-coex state */
	IOV_BTC_SELECT_PROFILE =	15,
	IOV_BTC_PROFILE =		16,	/* BTC profile for UCM */
	IOV_BTC_DYAGG =			17,	/* force on/off dynamic tx aggregation */
	IOV_BTC_BTRSSI_AVG =		18,	/* average btrssi (clear btrssi history when 0) */
	IOV_BTC_BLE_GRANTS =		19,	/* en/dis long duration BLE Scan grants */
	IOV_BTC_PROFILE_ACTIVE =	20,
	IOV_BTC_NSS =			21,	/* max nss capability of the associated AP */
	IOV_BTC_2G_CHAIN_DISABLE =	22,	/* # of chains allowed in 2.4G for WiFi */
	IOV_BTC_TASK =			23,	/* BT Coexistence task: eSCO, BLE Scan, etc. */
	IOV_BTC_BT_GPIO =		26,	/* 3-wire BT Coex BT request GPIO num */
	IOV_BTC_STAT_GPIO =		27,	/* 3-wire BT Coex BT status GPIO num */
	IOV_BTC_WLAN_GPIO =		28,	/* 3-wire BT Coex WLAN active/TX_CONF GPIO num */
	IOV_LAST
};

enum {
	BTCX_STATUS_BTC_ENABLE_SHIFT =	0,	/* btc is enabled */
	BTCX_STATUS_ACT_PROT_SHIFT =	1,	/* active protection */
	BTCX_STATUS_SLNA_SHIFT =	2,	/* sim rx with shared fem */
	BTCX_STATUS_SISO_ACK_SHIFT =	3,	/* siso ack */
	BTCX_STATUS_AVAILABLE_1 =	4,
	BTCX_STATUS_PS_PROT_SHIFT =	5,	/* PS mode is allowed for protection */
	BTCX_STATUS_AVAILABLE_2 =	6,
	BTCX_STATUS_DYN_AGG_SHIFT =	7,	/* dynamic aggregation is enabled */
	BTCX_STATUS_ANT_WLAN_SHIFT =	8,	/* prisel always to WL */
	BTCX_STATUS_AVAILABLE3 =	9,
	BTCX_STATUS_DEFANT_SHIFT =	10,	/* default shared antenna position */
	BTCX_STATUS_DESENSE_STATE =	11, /* Current state of Desense */
	BTCX_STATUS_CHAIN_PWR_STATE =	12	/* TX power chain state */
};

/*
 * Various Aggregation states that BTCOEX can request,
 * saved as a variable for improved visibility.
 */
enum {
	BTC_AGGOFF = 0,
	BTC_CLAMPTXAGG_RXAGGOFFEXCEPTAWDL = 1,
	BTC_DYAGG = 2,
	BTC_AGGON = 3,
	BTC_LIMAGG = 4
};

#ifdef WL_UCM
#define WLC_UCM_NO_TRANSMISSION		-128
#define WLC_UCM_MAX_POWER		127
#define WLC_UCM_PROFILE_UN_INIT		255
#define WLC_UCM_WEAK_TX_POWER		0
#define WLC_UCM_STRONG_TX_POWER		1
#define WLC_UCM_DESENSE_OFF		0
#define WLC_UCM_DESENSE_ON		1
#define WLC_UCM_MODE_STRONG_WL_BT	1
#define WLC_UCM_MODE_WEAK_WL		2
#define WLC_UCM_MODE_WEAK_BT		3
#define WLC_UCM_MODE_WEAK_WL_BT		4
#define WLC_UCM_BT_RSSI_INVALID		0
#define WLC_UCM_INVALID			-1
#define GCI_WL2BT_NOTIFY_HYBRID_MODE	(1 << 24)

#ifndef WL_UCM_DISABLED
static int wlc_btc_ucm_attach(wlc_btc_info_t *btc);
static void wlc_btc_ucm_detach(wlc_btc_info_t *btc);
#endif /* WL_UCM_DISABLED */
static void wlc_btcx_notify_hybrid_mode_to_bt(wlc_info_t * wlc);
static int wlc_btc_prof_get(wlc_info_t *wlc, int32 idx, void *resbuf, uint len);
static int wlc_btc_prof_set(wlc_info_t *wlc, void *parambuf, uint len);
static uint8 wlc_btc_nss_get(wlc_btc_info_t *btc);
static int wlc_btc_prof_active_set(wlc_info_t *wlc, wlc_btc_info_t *btc, int value);
static uint8 wlc_btc_ucm_elect_desense(wlc_info_t *wlc, wlc_btcx_profile_t *prof, int band);
static uint8  wlc_btc_ucm_elect_chain_pwr(wlc_info_t *wlc, wlc_btcx_profile_t *prof);
static int wlc_btc_ucm_elect_sisoack_core(wlc_info_t *wlc, wlc_btcx_profile_t *prof, int8 mode);
static int wlc_btc_ucm_elect_mode(wlc_info_t *wlc, wlc_btcx_profile_t *prof);
static void wlc_btc_ucm_set_desense(wlc_btc_info_t *btc, wlc_btcx_profile_t *prof, int band);
static void wlc_btc_ucm_desense(wlc_info_t *wlc, wlc_btcx_profile_t *prof, int band);
static int8 wlc_btc_get_wlrssi(wlc_bsscfg_t *cfg);
static void wlc_btc_clear_profile_info(wlc_btc_info_t *btc);
#endif /* WL_UCM */

/* Constants */
const bcm_iovar_t btc_iovars[] = {
	{"btc_mode", IOV_BTC_MODE, (0), 0, IOVT_UINT32, 0},
	{"btc_stuck_war", IOV_BTC_STUCK_WAR, 0, 0, IOVT_BOOL, 0 },
	{"btc_flags", IOV_BTC_FLAGS,
	(IOVF_SET_UP | IOVF_GET_UP | 0), 0, IOVT_BUFFER, 0
	},
	{"btc_params", IOV_BTC_PARAMS, (0), 0, IOVT_BUFFER, 0 },
	{"btc_siso_ack", IOV_BTC_SISO_ACK, 0, 0, IOVT_INT16, 0
	},
	{"btc_rxgain_thresh", IOV_BTC_RXGAIN_THRESH, 0, 0, IOVT_UINT32, 0
	},
	{"btc_rxgain_force", IOV_BTC_RXGAIN_FORCE, 0, 0, IOVT_UINT32, 0
	},
	{"btc_rxgain_level", IOV_BTC_RXGAIN_LEVEL, 0, 0, IOVT_UINT32, 0
	},
#ifdef WL_BTCDYN
	/* set dynctl profile, get status , etc */
	{"btc_dynctl", IOV_BTC_DYNCTL, 0, 0, IOVT_BUFFER, 0
	},
	/* set dynctl status */
	{"btc_dynctl_status", IOV_BTC_DYNCTL_STATUS, 0, 0, IOVT_BUFFER, 0
	},
	/* enable & configure dynctl simulation mode (aka dryrun) */
	{"btc_dynctl_sim", IOV_BTC_DYNCTL_SIM, 0, 0, IOVT_BUFFER, 0
	},
#endif // endif
	{"btc_status", IOV_BTC_STATUS, (IOVF_GET_UP), 0, IOVT_UINT32, 0},
#ifdef STA
#ifdef WLBTCPROF
	{"btc_select_profile", IOV_BTC_SELECT_PROFILE, 0, 0, IOVT_BUFFER, 0},
#endif // endif
#endif /* STA */
#ifdef WL_UCM
	{"btc_profile", IOV_BTC_PROFILE, 0, 0, IOVT_BUFFER, sizeof(wlc_btcx_profile_t)
	},
	{"btc_profile_active", IOV_BTC_PROFILE_ACTIVE, (0), 0, IOVT_INT32, 0
	},
	{"btc_nss", IOV_BTC_NSS, 0, 0, IOVT_UINT8, 0},
#endif /* WL_UCM */
	{"btc_dyagg", IOV_BTC_DYAGG, (0), 0, IOVT_INT8, 0
	},
	{"btc_btrssi_avg", IOV_BTC_BTRSSI_AVG, (0), 0, IOVT_INT8, 0
	},
	{"btc_ble_grants", IOV_BTC_BLE_GRANTS, (0), 0, IOVT_BOOL, 0
	},
	{"btc_2g_chain_disable", IOV_BTC_2G_CHAIN_DISABLE, 0, 0, IOVT_UINT8, 0
	},
	{"btc_task", IOV_BTC_TASK, 0, 0, IOVT_UINT16, 0},
	{"btc_bt_gpnum", IOV_BTC_BT_GPIO, (IOVF_SET_DOWN), 0, IOVT_UINT8, 0},
	{"btc_stat_gpnum", IOV_BTC_STAT_GPIO, (IOVF_SET_DOWN), 0, IOVT_UINT8, 0},
	{"btc_wlan_gpnum", IOV_BTC_WLAN_GPIO, (IOVF_SET_DOWN), 0, IOVT_UINT8, 0},
	{NULL, 0, 0, 0, 0, 0}
};

#ifdef WL_BTCDYN
/*  btcdyn nvram variables to initialize the profile  */
static const char BCMATTACHDATA(rstr_btcdyn_flags)[] = "btcdyn_flags";
static const char BCMATTACHDATA(rstr_btcdyn_dflt_dsns_level)[] = "btcdyn_dflt_dsns_level";
static const char BCMATTACHDATA(rstr_btcdyn_low_dsns_level)[] = "btcdyn_low_dsns_level";
static const char BCMATTACHDATA(rstr_btcdyn_mid_dsns_level)[] = "btcdyn_mid_dsns_level";
static const char BCMATTACHDATA(rstr_btcdyn_high_dsns_level)[] = "btcdyn_high_dsns_level";
static const char BCMATTACHDATA(rstr_btcdyn_default_btc_mode)[] = "btcdyn_default_btc_mode";
static const char BCMATTACHDATA(rstr_btcdyn_msw_rows)[] = "btcdyn_msw_rows";
static const char BCMATTACHDATA(rstr_btcdyn_dsns_rows)[] = "btcdyn_dsns_rows";
static const char BCMATTACHDATA(rstr_btcdyn_msw_row0)[] = "btcdyn_msw_row0";
static const char BCMATTACHDATA(rstr_btcdyn_msw_row1)[] = "btcdyn_msw_row1";
static const char BCMATTACHDATA(rstr_btcdyn_msw_row2)[] = "btcdyn_msw_row2";
static const char BCMATTACHDATA(rstr_btcdyn_msw_row3)[] = "btcdyn_msw_row3";
static const char BCMATTACHDATA(rstr_btcdyn_dsns_row0)[] = "btcdyn_dsns_row0";
static const char BCMATTACHDATA(rstr_btcdyn_dsns_row1)[] = "btcdyn_dsns_row1";
static const char BCMATTACHDATA(rstr_btcdyn_dsns_row2)[] = "btcdyn_dsns_row2";
static const char BCMATTACHDATA(rstr_btcdyn_dsns_row3)[] = "btcdyn_dsns_row3";
static const char BCMATTACHDATA(rstr_btcdyn_btrssi_hyster)[] = "btcdyn_btrssi_hyster";
static const char BCMATTACHDATA(rstr_btcdyn_wlpwr_thresh)[] = "btcdyn_wlpwr_thresh";
static const char BCMATTACHDATA(rstr_btcdyn_wlpwr_val)[] = "btcdyn_wlpwr_val";
#endif /* WL_BTCDYN */

/* BT RSSI threshold for implict mode switching */
static const char BCMATTACHDATA(rstr_prot_btrssi_thresh)[] = "prot_btrssi_thresh";
/* siso ack setting for hybrid mode */
static const char BCMATTACHDATA(rstr_btc_siso_ack)[] = "btc_siso_ack";

/* structure definitions */
struct wlc_btc_debounce_info {
	uint8 on_timedelta;		/*
					 * #secs of continuous inputs
					 * of 1 to result in setting
					 * debounce to 1.
					 */
	uint8 off_timedelta;		/* #secs of continuous inputs
					 * of 0 to result in clearing
					 * debounce to 0.
					 */
	uint16 status_inprogress;	/* pre-debounced value */
	uint16 status_debounced;		/* Final debounced value */
	int status_timestamp;		/* Time since last consecutive
					 * value seen in current_status
					 */
};
typedef struct wlc_btc_debounce_info wlc_btc_debounce_info_t;

/* BTCDYN info */
struct wlc_btcdyn_info {
	/* BTCOEX extension: adds dynamic desense & modes witching feature */
	uint16	bt_pwr_shm;	/* last raw/per task bt_pwr read from ucode */
	int8	bt_pwr;		/* current bt power  */
	int8	wl_rssi;	/* last wl rssi */
	uint8	cur_dsns;	/* current desense level */
	dctl_prof_t *dprof;	/* current board dynctl profile  */
	btcx_dynctl_calc_t desense_fn;  /* calculate desense level  */
	btcx_dynctl_calc_t mswitch_fn;  /* calculate mode switch */
	/*  stimuli for dynctl dry runs(fake BT & WL activity) */
	int8	sim_btpwr;
	int8	sim_wlrssi;
	int8	sim_btrssi;
	/* mode switching hysteresis */
	int8 msw_btrssi_hyster;	/* from bt rssi */
	bool	dynctl_sim_on;  /* enable/disable simulation mode */
	uint32	prev_btpwr_ts;	/* timestamp of last call to btc_dynctl() */
	int8	prev_btpwr;		/* prev btpwr reading to filter out false invalids */
	/* dynamic tx power adjustment based on RSSI for hybrid coex */
	uint8   wlpwr_steps;
	uint8   padding[3]; /* can be reused */
	int8    *wlpwr_thresh;
	int8    *wlpwr_val;
};
typedef struct wlc_btcdyn_info wlc_btcdyn_info_t;

struct wlc_btcx_curr_profile_info {
	uint8   chain_2g_req;   /* chain mask requested by Host's Unified Coex Manager */
	uint8   chain_2g_ack;   /* chain mask acknowledged by STF arbitor */
	uint8   chain_2g_shared;
	uint8   simrx;  /* simultaneous rx with shared fem/lna */
	uint8   curr_profile_idx;   /* Id of the Profile which is currently applied */
	uint8   mode_state; /* Current state of UCM */
	int8    chain_pwr_state;    /* Current state of Tx chain power */
	int8    desense_state;  /* Current state of Desense */
	int8    wlrssi; /* WL Rssi of the current bsscfg */
	chanspec_t  curr_chanspec;  /* Chanspec from current bsscfg */
	int8    *chain_pwr; /* Current chain power for all cores */
	int8    *ack_pwr;   /* Current siso ack power for all cores */
	int8    *lim_pwr;   /* Power used to determine cap value based on SAR limits */
	uint8   *ucm_restage_rxgain_level;  /* De-sense level for each cores */
	void    *stf_req_2g;
};
typedef struct wlc_btcx_curr_profile_info wlc_btcx_curr_profile_info_t;

/*  used by BTCDYN BT pwr debug code */
typedef struct pwrs {
	int8 pwr;
	uint32 ts;
} pwrs_t;

/* BTC stuff */
struct wlc_btc_info {
	wlc_info_t *wlc;
	uint16  bth_period;             /* bt coex period. read from shm. */
	bool    bth_active;             /* bt active session */
	uint8   ampdutx_rssi_thresh;    /* rssi threshold to turn off ampdutx */
	uint8   ampdurx_rssi_thresh;    /* rssi threshold to turn off ampdurx */
	uint8   host_requested_pm;      /* saved pm state specified by host */
	uint8   mode_overridden;        /* override btc_mode for long range */
	/* cached value for btc in high driver to avoid frequent RPC calls */
	int     mode;
	int     wire;
	int16   siso_ack;               /* txcoremask for siso ack (e.g., 1: use core 1 for ack) */
	int     restage_rxgain_level;
	int     restage_rxgain_force;
	int     restage_rxgain_active;
	uint8   restage_rxgain_on_rssi_thresh;  /* rssi threshold to turn on rxgain restaging */
	uint8   restage_rxgain_off_rssi_thresh; /* rssi threshold to turn off rxgain restaging */
	uint16	agg_off_bm;
	bool    siso_ack_ovr;           /* siso_ack set 0: automatically 1: by iovar */
	wlc_btc_prev_connect_t *btc_prev_connect; /* btc previous connection info */
	wlc_btc_select_profile_t *btc_profile; /* User selected profile for 2G and 5G params */
	wlc_btcdyn_info_t *btcdyn; /* btcdyn info */
	int8	*btrssi_sample;        /* array of recent BT RSSI values */
	uint8   btrssi_maxsamp;          /* maximum number of btrssi samples to average */
	uint8   btrssi_minsamp;          /* minimum number of btrssi samples to average */
	int16	btrssi_sum;	/* bt rssi MA sum */
	uint8   btrssi_cnt;     /* number of btrssi samples */
	uint8	btrssi_idx;	/* index to bt_rssi sample array */
	int8	bt_rssi;	/* averaged bt rssi */
	uint16	bt_shm_addr;
	uint8	run_cnt;
	int8	prot_btrssi_thresh; /* used by implicit mode switching */
	int8    dyagg;                  /* dynamic tx agg (1: on, 0: off, -1: auto) */
	uint8	btrssi_hyst;		/* btrssi hysteresis */
	uint8	wlrssi_hyst;		/* wlrssi hysteresis */
	uint16	acl_last_ts;
	uint16	a2dp_last_ts;
	uint8	agg_state_req;	/* Keeps state of the requested Aggregation state
				 * requested by BTC
				 */
	wlc_btc_debounce_info_t *ble_debounce; /* Used to debounce shmem for ble activity. */
	wlc_btc_debounce_info_t *a2dp_debounce; /* Used to debounce shmem for last_a2dp activity. */
	int8	sco_threshold_set;
	uint8	rxagg_resized;			/* Flag to indicate rx agg size was resized */
	uint8	rxagg_size;				/* New Rx agg size when SCO is on */
	uint16  profile_active_2g;  /* UCM profile active in 2G band */
	uint16  profile_active_5g;  /* UCM profile active in 5G band */
	wlc_btcx_curr_profile_info_t *profile_info;
	bool	ble_grants;		/* flag to indicate large BLE grants disabling */
	uint16	ble_hi_thres;		/* HI interval BLE grant threshold */
	wlc_btcx_profile_t	**profile_array;	/* UCM BTCX profile array */
	int16	siso_ack_nvram;		/* siso_ack setting from nvram */
	bool	chain_2g_shared;
	uint8   chain_2g_req;		/* chain mask requested by Host's Unified Coex Manager */
	uint8   chain_2g_ack;		/* chain mask acknowledged by STF arbitor */
	uint8	btc_bt_gpio;		/* 3-wire coex, bt active GPIO num */
	uint8	btc_stat_gpio;		/* 3-wire coex, bt status GPIO num */
	uint8	btc_wlan_gpio;		/* 3-wire coex, wlan active/TX_CONF GPIO num */
	void    *stf_req_2g;
	int     task;			/* BT task communicated to ucode: eSCO, BLE scan, etc. */
	uint8	coex_mode;		/* one of COEX_MODE_Xxx defs */
	bool	ulmu_disable;		/* UL OFDMA has been disabled */
};

/* Function prototypes */
static int wlc_btc_mode_set(wlc_info_t *wlc, int int_val);
static int wlc_btc_task_get(wlc_info_t *wlc);
static int wlc_btc_wire_set(wlc_info_t *wlc, int int_val);
static int wlc_btc_flags_idx_set(wlc_info_t *wlc, int int_val, int int_val2);
static int wlc_btc_flags_idx_get(wlc_info_t *wlc, int int_val);
static void wlc_btc_stuck_war50943(wlc_info_t *wlc, bool enable);
static int16 wlc_btc_siso_ack_get(wlc_info_t *wlc);
static int wlc_btc_wlc_up(void *ctx);
static int wlc_btc_wlc_down(void *ctx);
int wlc_btcx_desense(wlc_btc_info_t *btc, int band);
#ifdef WL_BTCDYN
static int wlc_btc_dynctl_profile_set(wlc_info_t *wlc, void *parambuf);
static int wlc_btc_dynctl_profile_get(wlc_info_t *wlc, void *resbuf);
static int wlc_btc_dynctl_status_get(wlc_info_t *wlc, void *resbuf);
static int wlc_btc_dynctl_sim_get(wlc_info_t *wlc, void *resbuf);
static int wlc_btc_dynctl_sim_set(wlc_info_t *wlc, void *parambuf);
#if !defined(WL_BTCDYN_DISABLED)
static int wlc_btcdyn_attach(wlc_btc_info_t *btc);
#endif // endif
static void wlc_btcx_dynctl(wlc_btc_info_t *btc);
#endif /* WL_BTCDYN */
static void wlc_btc_update_btrssi(wlc_btc_info_t *btc);
static void wlc_btc_reset_btrssi(wlc_btc_info_t *btc);

#if defined(STA) && defined(BTCX_PM0_IDLE_WAR)
static void wlc_btc_pm_adjust(wlc_info_t *wlc,  bool bt_active);
#endif // endif
static int wlc_btc_doiovar(void *hdl, uint32 actionid,
        void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif);
static void wlc_btcx_watchdog(void *arg);
#if defined(BCMDBG)
static int wlc_dump_btcx(void *ctx, struct bcmstrbuf *b);
static int wlc_clr_btcxdump(void *ctx);
#endif // endif
static uint32 wlc_btcx_get_btc_status(wlc_info_t *wlc);

#ifdef STA
static bool wlc_btcx_check_port_open(wlc_info_t *wlc);
#endif // endif

static wlc_btc_debounce_info_t *btc_debounce_init(wlc_info_t *wlc, uint8 on_seconds,
	uint8 off_seconds);
#ifdef WLAMPDU
static int wlc_btcx_rssi_for_ble_agg_decision(wlc_info_t *wlc);
static void wlc_btcx_evaluate_agg_state(wlc_btc_info_t *btc);
static uint16 wlc_btcx_debounce(wlc_btc_debounce_info_t *btc_debounce, uint16 curr);
static bool wlc_btcx_is_hybrid_in_TDM(wlc_info_t *wlc);
#endif // endif
static void wlc_btc_update_sco_params(wlc_info_t *wlc);
#if defined(WL_UCM)
static int wlc_btc_2g_chain_set(wlc_info_t *wlc, bool bool_val);
static bool wlc_btc_2g_chain_get(wlc_info_t *wlc);
#endif /* WL_UCM */

#ifdef WLAMPDU
static void wlc_btcx_rx_ba_window_modify(wlc_info_t *wlc, bool *rx_agg);
static void wlc_btcx_rx_ba_window_reset(wlc_info_t *wlc);
#endif /* WLAMPDU */
static void wlc_btc_sisoack_shm(wlc_info_t *wlc);

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

wlc_btc_info_t *
BCMATTACHFN(wlc_btc_attach)(wlc_info_t *wlc)
{
	wlc_btc_info_t *btc;
#ifdef WLBTCPROF
	wlc_btc_profile_t *select_profile;
#endif /* WLBTCPROF */
	wlc_hw_info_t *wlc_hw = wlc->hw;
#ifdef WL_UCM
	uint8 chain_value;
#endif /* WL_UCM */
	uint16 var;
	uint8 gpio_num;

	if ((btc = (wlc_btc_info_t*)
		MALLOCZ(wlc->osh, sizeof(wlc_btc_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	btc->wlc = wlc;

	if (BTCX_ENAB(wlc_hw)) {
		if (SWBTCX_ENAB(wlc_hw)) {
			if ((wlc_hw->boardflags2 & BFL2_BTC3WIREONLY)) {
				btc->coex_mode = COEX_MODE_GCI;
			} else {
				btc->coex_mode = COEX_MODE_GPIO;
			}
		} else {
			btc->coex_mode = COEX_MODE_ECI;
		}
	}

	/* register module */
	if (wlc_module_register(wlc->pub, btc_iovars, "btc", btc, wlc_btc_doiovar,
		wlc_btcx_watchdog, wlc_btc_wlc_up, wlc_btc_wlc_down) != BCME_OK) {
		WL_ERROR(("wl%d: %s: btc register err\n", wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

#if defined(BCMDBG)
	/* register dump stats for btcx */
	wlc_dump_add_fns(wlc->pub, "btcx", wlc_dump_btcx, wlc_clr_btcxdump, wlc);
#endif // endif

#if defined(BCMCOEXNOA) && !defined(BCMCOEXNOA_DISABLED)
	wlc->pub->_cxnoa = TRUE;
#endif /* defined(BCMCOEXNOA) && !defined(BCMCOEXNOA_DISABLED) */

#ifdef WLBTCPROF
	if ((btc->btc_prev_connect = (wlc_btc_prev_connect_t *)
		MALLOCZ(wlc->osh, sizeof(wlc_btc_prev_connect_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}

	if ((btc->btc_profile = (wlc_btc_select_profile_t *)
		MALLOCZ(wlc->osh, sizeof(wlc_btc_select_profile_t) * BTC_SUPPORT_BANDS)) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}

	bzero(btc->btc_prev_connect, sizeof(wlc_btc_prev_connect_t));
	bzero(btc->btc_profile, sizeof(wlc_btc_profile_t) * BTC_SUPPORT_BANDS);

	memset(&btc->btc_prev_connect->prev_2G_profile, 0, sizeof(struct wlc_btc_profile));
	memset(&btc->btc_prev_connect->prev_5G_profile, 0, sizeof(struct wlc_btc_profile));

	btc->btc_prev_connect->prev_band = WLC_BAND_ALL;

	select_profile = &btc->btc_profile[BTC_PROFILE_2G].select_profile;
	select_profile->btc_wlrssi_thresh = BTC_WL_RSSI_DEFAULT;
	select_profile->btc_btrssi_thresh = BTC_BT_RSSI_DEFAULT;
	if (CHIPID(wlc->pub->sih->chip) == BCM4350_CHIP_ID) {
		select_profile->btc_num_desense_levels = MAX_BT_DESENSE_LEVELS_4350;
		select_profile->btc_wlrssi_hyst = BTC_WL_RSSI_HYST_DEFAULT_4350;
		select_profile->btc_btrssi_hyst = BTC_BT_RSSI_HYST_DEFAULT_4350;
		select_profile->btc_max_siso_resp_power[0] =
			BTC_WL_MAX_SISO_RESP_POWER_TDD_DEFAULT_4350;
		select_profile->btc_max_siso_resp_power[1] =
			BTC_WL_MAX_SISO_RESP_POWER_HYBRID_DEFAULT_4350;
	}
#endif /* WLBTCPROF */

	btc->siso_ack_ovr = FALSE;
	btc->siso_ack_nvram = (uint16)getintvar(wlc_hw->vars, rstr_btc_siso_ack);
	btc->siso_ack = btc->siso_ack_nvram;

#if defined(WL_BTCDYN) && !defined(WL_BTCDYN_DISABLED)
	/* btcdyn attach */
	if (wlc_btcdyn_attach(btc) != BCME_OK) {
		goto fail;
	}
	wlc->pub->_btcdyn = TRUE;
#endif // endif

	if ((btc->btrssi_sample =
		(int8*)MALLOCZ(wlc->osh, sizeof(int8) * BTC_BTRSSI_MAX_SAMP)) == NULL) {
		WL_ERROR(("wl%d: %s: MALLOC for btrssi_sample failed, %d bytes\n",
		    wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	btc->btrssi_maxsamp = BTC_BTRSSI_MAX_SAMP;
	btc->btrssi_minsamp = BTC_BTRSSI_MIN_SAMP;
	wlc_btc_reset_btrssi(btc);

	if (getvar(wlc_hw->vars, rstr_prot_btrssi_thresh) != NULL) {
		btc->prot_btrssi_thresh =
			(int8)getintvar(wlc_hw->vars, rstr_prot_btrssi_thresh);
	} else {
		btc->prot_btrssi_thresh = BTC_BTRSSI_THRESH;
	}

	btc->dyagg = AUTO;

	if ((btc->ble_debounce = btc_debounce_init(wlc, BLE_DEBOUNCE_ON_SECS,
			BLE_DEBOUNCE_OFF_SECS)) == NULL) {
		goto fail;
	}
	if ((btc->a2dp_debounce = btc_debounce_init(wlc, A2DP_DEBOUNCE_ON_SECS,
			A2DP_DEBOUNCE_OFF_SECS)) == NULL) {
		goto fail;
	}
	// Default BTCX agg state is ON.
	btc->agg_state_req = BTC_AGGON;

	/* Initialize long duration BLE grants to enabled */
	btc->ble_grants = 1;

#if defined(WL_UCM) && !defined(WL_UCM_DISABLED)
	/* all the UCM related attach activities go here */
	if (wlc_btc_ucm_attach(btc) != BCME_OK) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	wlc->pub->_ucm = TRUE;
#endif /* WL_UCM && !WL_UCM_DISABLED */
#ifdef WL_UCM
#ifdef WL_STF_ARBITRATOR
	/* Register with STF Aribitrator module for configuring
	*	chains for 2.4G band - high priority
	*/
	if (!btc->stf_req_2g) {
		btc->stf_req_2g = wlc_stf_nss_request_register(wlc,
			WLC_STF_ARBITRATOR_REQ_ID_BTC_2G, WLC_STF_ARBITRATOR_REQ_PRIO_BTC_2G,
				wlc_stf_arb_btc_2g_call_back, (void*)btc);
		if (! btc->stf_req_2g) {
			WL_ERROR(("wl%d: %s: Failed to register stf_nss:btc_req_2g\n",
				wlc->pub->unit, __FUNCTION__));
			goto fail;
		}
	}
#endif /* WL_STF_ARBITRATOR */
	/* btc_2gchain must be defined in nvram file */
	chain_value = (uint8)getintvar(wlc->hw->vars, "btc_2gchain");
	if (chain_value != 0) {
		btc->chain_2g_req = chain_value;
		btc->chain_2g_ack = chain_value;
	} else {
		btc->chain_2g_req = wlc->stf->hw_txchain;
		btc->chain_2g_ack = wlc->stf->hw_txchain;
	}
	btc->chain_2g_shared = FALSE;
#endif /* WL_UCM */

	var = getintvar(wlc_hw->vars, "coex_gpioctrl_0");
	gpio_num = (uint8)(var & 0xff);
	if (gpio_num && gpio_num <= BOARD_GPIO_SW_MAX_VAL) {
		btc->btc_bt_gpio = gpio_num;
	} else {
		btc->btc_bt_gpio = BOARD_GPIO_SW_BTC_BT;
	}

	gpio_num = (uint8)((var >> 8) & 0xff);
	if (gpio_num && gpio_num <= BOARD_GPIO_SW_MAX_VAL) {
		btc->btc_stat_gpio = gpio_num;
	} else {
		btc->btc_stat_gpio = BOARD_GPIO_SW_BTC_STAT;
	}

	var = getintvar(wlc_hw->vars, "coex_gpioctrl_1");
	gpio_num = (uint8)(var & 0xff);
	if (gpio_num && gpio_num <= BOARD_GPIO_SW_MAX_VAL) {
		btc->btc_wlan_gpio = gpio_num;
	} else {
		btc->btc_wlan_gpio = BOARD_GPIO_SW_BTC_WLAN;
	}

	return btc;

	/* error handling */
fail:
	MODULE_DETACH(btc, wlc_btc_detach);
	return NULL;
}

void
BCMINITFN(wlc_btc_init)(wlc_info_t *wlc)
{
	wlc_btc_info_t *btch = wlc->btch;

	/* Deny BT initially to protect cals */
	if (btch->coex_mode == COEX_MODE_GPIO || btch->coex_mode == COEX_MODE_GCI) {
		si_gpioouten(wlc->hw->sih, (1 << (btch->btc_wlan_gpio - 1)),
			(1 << (btch->btc_wlan_gpio - 1)), GPIO_DRV_PRIORITY);
		si_gpioout(wlc->hw->sih, (1 << (btch->btc_wlan_gpio - 1)),
			(1 << (btch->btc_wlan_gpio - 1)), GPIO_DRV_PRIORITY);
	}
}

static wlc_btc_debounce_info_t *
btc_debounce_init(wlc_info_t *wlc, uint8 on_seconds, uint8 off_seconds)
{
	/* Initialize debounce variables */
	wlc_btc_debounce_info_t *btc_debounce;
	if ((btc_debounce = (wlc_btc_debounce_info_t *)
		MALLOCZ(wlc->osh, sizeof(wlc_btc_debounce_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
		wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}
	btc_debounce->on_timedelta = on_seconds;
	btc_debounce->off_timedelta = off_seconds;
	btc_debounce->status_inprogress = 0;
	btc_debounce->status_debounced = 0;
	btc_debounce->status_timestamp = 0;
	return btc_debounce;
}

void
BCMATTACHFN(wlc_btc_detach)(wlc_btc_info_t *btc)
{
	wlc_info_t *wlc;

	if (btc == NULL)
		return;

	wlc = btc->wlc;
	wlc_module_unregister(wlc->pub, "btc", btc);

#ifdef WL_BTCDYN
	if (BTCDYN_ENAB(wlc->pub)) {
		if (btc->btcdyn != NULL) {
			if (btc->btcdyn->dprof) {
				MFREE(wlc->osh, btc->btcdyn->dprof, sizeof(dctl_prof_t));
				btc->btcdyn->dprof = NULL;
			}
			if (btc->btcdyn->wlpwr_val) {
				MFREE(wlc->osh, btc->btcdyn->wlpwr_val,
					sizeof(int8)*DYN_PWR_MAX_STEPS);
				btc->btcdyn->wlpwr_val = NULL;
			}
				if (btc->btcdyn->wlpwr_thresh) {
				MFREE(wlc->osh, btc->btcdyn->wlpwr_thresh,
					sizeof(int8)*(DYN_PWR_MAX_STEPS-1));
				btc->btcdyn->wlpwr_thresh = NULL;
			}
			MFREE(wlc->osh, btc->btcdyn, sizeof(wlc_btcdyn_info_t));
		}
	}
#endif /* WL_BTCDYN */

#if defined(BCMCOEXNOA) && !defined(BCMCOEXNOA_DISABLED)
	wlc->pub->_cxnoa = FALSE;
#endif /* defined(BCMCOEXNOA) && !defined(BCMCOEXNOA_DISABLED) */

#ifdef WLBTCPROF
	if (btc->btc_prev_connect != NULL)
		MFREE(wlc->osh, btc->btc_prev_connect, sizeof(wlc_btc_prev_connect_t));
	if (btc->btc_profile != NULL)
		MFREE(wlc->osh, btc->btc_profile, sizeof(wlc_btc_select_profile_t));
#endif /* WLBTCPROF */
	if (btc->btrssi_sample != NULL)
		MFREE(wlc->osh, btc->btrssi_sample, sizeof(int8) * BTC_BTRSSI_MAX_SAMP);

	if (btc->a2dp_debounce != NULL)
		MFREE(wlc->osh, btc->a2dp_debounce, sizeof(wlc_btc_debounce_info_t));
	if (btc->ble_debounce != NULL)
		MFREE(wlc->osh, btc->ble_debounce, sizeof(wlc_btc_debounce_info_t));

#if defined(WL_UCM) && !defined(WL_UCM_DISABLED)
#ifdef WL_STF_ARBITRATOR
	/* UnRegister with STF Aribitrator module for configuring  chains */
	if (btc->stf_req_2g &&
			wlc_stf_nss_request_unregister(wlc, btc->stf_req_2g))
			WL_STF_ARBITRATOR_ERROR(("ARBI: Error at Unregister BTC req\n"));
	btc->stf_req_2g = NULL;
#endif /* WL_STF_ARBITRATOR */

	/* All the UCM related detach activities go here */
	wlc_btc_ucm_detach(btc);
#endif /* defined(WL_UCM) && !defined(WL_UCM_DISABLED) */
	MFREE(wlc->osh, btc, sizeof(wlc_btc_info_t));
}

#if defined(WL_UCM) && !defined(WL_UCM_DISABLED)
/* All UCM related attach activities go here */
static int
BCMATTACHFN(wlc_btc_ucm_attach)(wlc_btc_info_t *btc)
{
	wlc_info_t *wlc = btc->wlc;
	int idx, idx2;
	size_t ucm_profile_sz;
	size_t ucm_array_sz = MAX_UCM_PROFILES*sizeof(*btc->profile_array);
	uint8 chain_attr_count = WLC_BITSCNT(wlc->stf->hw_txchain);
	wlc_btcx_chain_attr_t *chain_attr;

	/* first create an array of pointers to btc profile */
	btc->profile_array = (wlc_btcx_profile_t **)MALLOCZ(wlc->osh, ucm_array_sz);
	if (btc->profile_array == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return BCME_NOMEM;
	}

	/* malloc individual profiles and insert their address in the btc profile array */
	for (idx = 0; idx < MAX_UCM_PROFILES; idx++) {
		/* the profile size is determined by the number of tx chains */
		ucm_profile_sz = sizeof(*btc->profile_array[idx])
			+ chain_attr_count*sizeof(btc->profile_array[idx]->chain_attr[0]);
		btc->profile_array[idx] =
			(wlc_btcx_profile_t *)MALLOCZ(wlc->osh, ucm_profile_sz);
		if (btc->profile_array[idx] == NULL) {
			WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			return BCME_NOMEM;
		}
		btc->profile_array[idx]->profile_index = idx;
		/* last element of the profile is a struct with explicit padding */
		btc->profile_array[idx]->length = ucm_profile_sz;
		/* whenever changing the profile struct, update the last fixed entry here */
		btc->profile_array[idx]->fixed_length =
			STRUCT_SIZE_THROUGH(btc->profile_array[idx],
			tx_pwr_wl_lo_hi_rssi_thresh);
		btc->profile_array[idx]->version = UCM_PROFILE_VERSION;
		btc->profile_array[idx]->chain_attr_count = chain_attr_count;
		/* whenever changing the attributes struct, update last fixed entry here */
		for (idx2 = 0; idx2 < chain_attr_count; idx2++) {
			chain_attr = &(btc->profile_array[idx]->chain_attr[idx2]);
			chain_attr->length =
				STRUCT_SIZE_THROUGH(chain_attr, tx_pwr_weak_rssi);
		}
	}

	if ((btc->profile_info = (wlc_btcx_curr_profile_info_t *)
		MALLOCZ(wlc->osh, sizeof(wlc_btcx_curr_profile_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return BCME_NOMEM;
	}

	bzero(btc->profile_info, sizeof(wlc_btcx_curr_profile_info_t));
	btc->profile_info->chain_pwr = MALLOCZ(wlc->osh, sizeof(int8) * chain_attr_count);
	btc->profile_info->ack_pwr = MALLOCZ(wlc->osh, sizeof(int8) * chain_attr_count);
	btc->profile_info->lim_pwr = MALLOCZ(wlc->osh, sizeof(int8) * chain_attr_count);
	btc->profile_info->ucm_restage_rxgain_level =
		MALLOCZ(wlc->osh, sizeof(uint8) * chain_attr_count);

	if (!btc->profile_info->chain_pwr || !btc->profile_info->ack_pwr ||
		!btc->profile_info->lim_pwr || !btc->profile_info->ucm_restage_rxgain_level) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return BCME_NOMEM;
	}

	btc->profile_info->mode_state       = WLC_UCM_MODE_WEAK_WL_BT;
	btc->profile_info->desense_state    = WLC_UCM_DESENSE_OFF;
	btc->profile_info->chain_pwr_state  = WLC_UCM_INVALID;
	btc->profile_info->curr_profile_idx = WLC_UCM_INVALID;
	btc->profile_active_2g              = WLC_UCM_INVALID;
	btc->profile_active_5g              = WLC_UCM_INVALID;
	return BCME_OK;
}

/* All UCM related detach activities go here */
static void
BCMATTACHFN(wlc_btc_ucm_detach)(wlc_btc_info_t *btc)
{
	int idx;
	wlc_info_t *wlc = btc->wlc;
	wlc_btcx_profile_t *profile;
	size_t ucm_profile_sz;
	size_t ucm_array_sz = MAX_UCM_PROFILES*sizeof(*btc->profile_array);
	uint8 chain_attr_count = WLC_BITSCNT(wlc->stf->hw_txchain);
	/* if profile_array was not allocated, then nothing to do */
	if (btc->profile_array) {
		for (idx = 0; idx < MAX_UCM_PROFILES; idx++) {
			profile = btc->profile_array[idx];
			if (profile) {
				ucm_profile_sz = sizeof(*profile)
					+(profile->chain_attr_count)
					*sizeof(profile->chain_attr[0]);
				MFREE(wlc->osh, profile, ucm_profile_sz);
			}
		}
		MFREE(wlc->osh, btc->profile_array, ucm_array_sz);
	}
	if (btc->profile_info) {
		if (btc->profile_info->chain_pwr) {
			MFREE(wlc->osh, btc->profile_info->chain_pwr,
				sizeof(btc->profile_info->chain_pwr[0]) * chain_attr_count);
		}
		if (btc->profile_info->ack_pwr) {
			MFREE(wlc->osh, btc->profile_info->ack_pwr,
				sizeof(btc->profile_info->ack_pwr[0]) * chain_attr_count);
		}
		if (btc->profile_info->lim_pwr) {
			MFREE(wlc->osh, btc->profile_info->lim_pwr,
				sizeof(btc->profile_info->lim_pwr[0]) * chain_attr_count);
		}
		if (btc->profile_info->ucm_restage_rxgain_level) {
			MFREE(wlc->osh, btc->profile_info->ucm_restage_rxgain_level,
				sizeof(btc->profile_info->ucm_restage_rxgain_level[0]) *
				chain_attr_count);
		}
		MFREE(wlc->osh, btc->profile_info, sizeof(wlc_btcx_curr_profile_info_t));
	}
}
#endif /* defined(WL_UCM) && !defined(WL_UCM_DISABLED) */

/* BTCX Wl up callback */
static int
wlc_btc_wlc_up(void *ctx)
{
	wlc_btc_info_t *btc = (wlc_btc_info_t *)ctx;
	wlc_info_t *wlc = btc->wlc;
	wlc_hw_info_t *wlc_hw = wlc->hw;
	wlc_phy_btc_config_t phy_btc_config;
	si_t *sih = wlc->pub->sih;

	if (!btc->bt_shm_addr)
		btc->bt_shm_addr = 2 * wlc_bmac_read_shm(wlc->hw, M_BTCX_BLK_PTR(wlc));

	/* Apply any pending changes in BLE grants */
	wlc_btc_ble_grants_enable(btc, btc->ble_grants);

	if (btc->coex_mode == COEX_MODE_ECI) {
		// SECI_OUT on GPIO 3; SECI_IN on GPIO 4
		wlc_hw->btc->gpio_mask = 0x0018;
		wlc_hw->btc->gpio_out  = 0x0008;
	} else if ((btc->btc_bt_gpio && btc->btc_stat_gpio && btc->btc_wlan_gpio)) {
		uint32 mask = 0;
		uint32 bitcount = 0;

		mask = (1 << (btc->btc_bt_gpio - 1)) |
			(1 << (btc->btc_stat_gpio - 1)) | (1 << (btc->btc_wlan_gpio - 1));
		/* make sure no GPIO num is the same (count # of 1s in bitmask) */
		bitcount = bcm_bitcount((uint8 *)&mask, sizeof(mask));
		if (bitcount != BOARD_GPIO_NUM_GPIOS) {
			WL_ERROR(("wl%d: %s: the %d Coex GPIO bit assignments (0x%x) are not "
				  "unique. Applying default bits 0x%x.\n",
				  wlc->pub->unit, __FUNCTION__, BOARD_GPIO_NUM_GPIOS, mask,
				  BOARD_GPIO_SW_BTC_DEFMASK));
			btc->btc_bt_gpio = BOARD_GPIO_SW_BTC_BT;
			btc->btc_stat_gpio = BOARD_GPIO_SW_BTC_STAT;
			btc->btc_wlan_gpio = BOARD_GPIO_SW_BTC_WLAN;
			mask = BOARD_GPIO_SW_BTC_DEFMASK;
		}
		/* program coex gpio mask bit scratch registers */
		wlc_bmac_write_scr(wlc_hw, S_BTCX_BT_GPIO,
				(uint16)(1 << (btc->btc_bt_gpio - 1)));
		wlc_bmac_write_scr(wlc_hw, S_BTCX_STAT_GPIO,
				(uint16)(1 << (btc->btc_stat_gpio - 1)));
		wlc_bmac_write_scr(wlc_hw, S_BTCX_WLAN_GPIO,
				(uint16)(1 << (btc->btc_wlan_gpio - 1)));
		wlc_hw->btc->gpio_mask = mask;
		wlc_hw->btc->gpio_out	|= (1 << (btc->btc_wlan_gpio - 1));

		if (btc->coex_mode == COEX_MODE_GCI) {
			si_gci_3wire_init(sih, btc->btc_wlan_gpio, btc->btc_bt_gpio,
					btc->btc_stat_gpio);
		}
	}

	wlc_bmac_btc_gpio_enable(wlc_hw);

	/* sync over high level settings needed by phy_btcx */
	phy_btc_config.mode = btc->mode;
	phy_btc_config.wire = btc->wire;
	phy_btc_config.task = btc->task;
	phy_btc_config.gpio_mask = wlc_hw->btc->gpio_mask;
	phy_btc_config.gpio_out = wlc_hw->btc->gpio_out;
	phy_btc_config.btc_wlan_gpio = btc->btc_wlan_gpio;
	phy_btc_config.coex_mode = btc->coex_mode;
	wlc_iovar_op(wlc, "phy_btc_config", NULL, 0, &phy_btc_config,
		sizeof(wlc_phy_btc_config_t), IOV_SET, NULL);

	return BCME_OK;
}

/* BTCX Wl down callback */
static int
wlc_btc_wlc_down(void *ctx)
{
	wlc_btc_info_t *btc = (wlc_btc_info_t *)ctx;
	wlc_info_t *wlc = btc->wlc;
#if defined(BCMECICOEX)
	si_t *sih = btc->wlc->pub->sih;
#endif /* BCMECICOEX && WLC_SW_DIVERSITY */

	BCM_REFERENCE(wlc);
	btc->bt_shm_addr = 0;
#if defined(BCMECICOEX)
	if (BCMSECICOEX_ENAB_BMAC(wlc->hw)) {
		if (BAND_2G(btc->wlc->band->bandtype)) {
			si_gci_direct(sih, GCI_OFFSETOF(sih, gci_control_1),
				GCI_WL_2G_UP_INFO, GCI_WL_2G_UP_INFO);
			si_gci_direct(sih, GCI_OFFSETOF(sih, gci_output[1]),
				GCI_WL_2G_UP_INFO, GCI_WL_RESET_2G_UP_INFO);
		}
		if (BAND_5G(btc->wlc->band->bandtype)) {
			si_gci_direct(sih, GCI_OFFSETOF(sih, gci_control_1),
				GCI_WL_5G_UP_INFO, GCI_WL_5G_UP_INFO);
			si_gci_direct(sih, GCI_OFFSETOF(sih, gci_output[1]),
				GCI_WL_5G_UP_INFO, GCI_WL_RESET_5G_UP_INFO);
		}

#ifdef WL_UCM
		if (UCM_ENAB(wlc->pub)) {
			wlc_btc_clear_profile_info(btc);
		}
#endif /* WL_UCM */

#if defined(WLC_SW_DIVERSITY)
		/* Set back SWDIV to 0 when WLAN is down */
		si_gci_direct(sih, GCI_OFFSETOF(sih, gci_control_1),
			GCI_WL_SWDIV_ANT_VALID_BIT_MASK,
			GCI_WL_SWDIV_ANT_VALID_BIT_MASK);
		si_gci_direct(sih, GCI_OFFSETOF(sih, gci_output[1]),
			GCI_WL_SWDIV_ANT_VALID_BIT_MASK,
			GCI_SWDIV_ANT_VALID_DISABLE);
#endif /* WLC_SW_DIVERSITY */
	}
#endif  /* BCMECICOEX */

	return BCME_OK;
}

static int
wlc_btc_mode_set(wlc_info_t *wlc, int int_val)
{
	int err = wlc_bmac_btc_mode_set(wlc->hw, int_val);
	wlc->btch->mode = wlc_bmac_btc_mode_get(wlc->hw);
	return err;
}

int
wlc_btc_mode_get(wlc_info_t *wlc)
{
	return wlc->btch->mode;
}

int
wlc_btc_task_set(wlc_info_t *wlc, int int_val)
{
	int err = wlc_bmac_btc_task_set(wlc->hw, int_val);
	wlc->btch->task = wlc_bmac_btc_task_get(wlc->hw);
	return err;
}

static int
wlc_btc_task_get(wlc_info_t *wlc)
{
	wlc->btch->task = wlc_bmac_btc_task_get(wlc->hw);
	return wlc->btch->task;
}

int
wlc_btcx_desense(wlc_btc_info_t *btc, int band)
{
	int i;
	wlc_info_t *wlc = (wlc_info_t *)btc->wlc;
	int btc_mode = wlc_btc_mode_get(wlc);

	/* Dynamic restaging of rxgain for BTCoex */
	if (!SCAN_IN_PROGRESS(wlc->scan) &&
	    btc->bth_active &&
	    (wlc->primary_bsscfg->link->rssi != WLC_RSSI_INVALID)) {
		if (!btc->restage_rxgain_active &&
		    ((BAND_5G(band) &&
		      ((btc->restage_rxgain_force &
			BTC_RXGAIN_FORCE_5G_MASK) == BTC_RXGAIN_FORCE_5G_ON)) ||
		     (BAND_2G(band) &&
		      ((btc->restage_rxgain_force &
			BTC_RXGAIN_FORCE_2G_MASK) == BTC_RXGAIN_FORCE_2G_ON) &&
		      (!btc->restage_rxgain_on_rssi_thresh ||
		       (btc_mode == WL_BTC_DISABLE) ||
		       (btc->restage_rxgain_on_rssi_thresh &&
		       ((btc_mode == WL_BTC_HYBRID) || (btc_mode == WL_BTC_FULLTDM)) &&
			(-wlc->primary_bsscfg->link->rssi <
				btc->restage_rxgain_on_rssi_thresh)))))) {
			if ((i = wlc_iovar_setint(wlc, "phy_btc_restage_rxgain",
				btc->restage_rxgain_level)) == BCME_OK) {
				btc->restage_rxgain_active = 1;
			}
			WL_BTCPROF(("wl%d: BTC restage rxgain (%x) ON: RSSI %d "
				"Thresh -%d, bt %d, (err %d)\n",
				wlc->pub->unit, wlc->stf->rxchain, wlc->primary_bsscfg->link->rssi,
				btc->restage_rxgain_on_rssi_thresh,
				btc->bth_active, i));
		}
		else if (btc->restage_rxgain_active &&
			((BAND_5G(band) &&
			((btc->restage_rxgain_force &
			BTC_RXGAIN_FORCE_5G_MASK) == BTC_RXGAIN_FORCE_OFF)) ||
			(BAND_2G(band) &&
			(((btc->restage_rxgain_force &
			BTC_RXGAIN_FORCE_2G_MASK) == BTC_RXGAIN_FORCE_OFF) ||
			(btc->restage_rxgain_off_rssi_thresh &&
			((btc_mode == WL_BTC_HYBRID) || (btc_mode == WL_BTC_FULLTDM)) &&
			(-wlc->primary_bsscfg->link->rssi >
				btc->restage_rxgain_off_rssi_thresh)))))) {
			  if ((i = wlc_iovar_setint(wlc, "phy_btc_restage_rxgain", 0)) == BCME_OK) {
				btc->restage_rxgain_active = 0;
			  }
			  WL_BTCPROF(("wl%d: BTC restage rxgain (%x) OFF: RSSI %d "
				"Thresh -%d, bt %d, (err %d)\n",
				wlc->pub->unit, wlc->stf->rxchain, wlc->primary_bsscfg->link->rssi,
				btc->restage_rxgain_off_rssi_thresh,
				btc->bth_active, i));
		}
	} else if (btc->restage_rxgain_active) {
		if ((i = wlc_iovar_setint(wlc, "phy_btc_restage_rxgain", 0)) == BCME_OK) {
			btc->restage_rxgain_active = 0;
		}
		WL_BTCPROF(("wl%d: BTC restage rxgain (%x) OFF: RSSI %d bt %d (err %d)\n",
			wlc->pub->unit, wlc->stf->rxchain, wlc->primary_bsscfg->link->rssi,
			btc->bth_active, i));
	}

	return BCME_OK;
}

#ifdef WL_UCM
static void
wlc_btcx_notify_hybrid_mode_to_bt(wlc_info_t * wlc)
{
	si_t *sih = wlc->pub->sih;

	/* check if we are using shared LNA settings */
	/* if in hybrid mode, lower BT TX power */
	if (BCMSECICOEX_ENAB_BMAC(wlc->hw)) {
		/* GCI bit 56: used to indicate Hybrid Mode operation to BT */
		si_gci_direct(sih, GCI_OFFSETOF(sih, gci_control_1),
				GCI_WL2BT_NOTIFY_HYBRID_MODE, GCI_WL2BT_NOTIFY_HYBRID_MODE);

		if ((wlc->btch->mode == WL_BTC_HYBRID) && (wlc->btch->profile_info->simrx)) {
			si_gci_direct(sih, GCI_OFFSETOF(sih, gci_output[1]),
					GCI_WL2BT_NOTIFY_HYBRID_MODE, GCI_WL2BT_NOTIFY_HYBRID_MODE);
		} else {
			si_gci_direct(sih, GCI_OFFSETOF(sih, gci_output[1]),
					GCI_WL2BT_NOTIFY_HYBRID_MODE, 0);
		}
	}
}

/* Wrapper fn to get the bsscfg for the corresponding chanspec */
void
wlc_btc_get_matched_chanspec_bsscfg(wlc_info_t *wlc, chanspec_t chanspec, wlc_bsscfg_t **cfg)
{
	int index = 0;
	/* cfg is assigned to wlc->bsscfg[index] inside the MACRO */
	FOREACH_AS_BSS(wlc, index, (*cfg)) {
		if ((*cfg)->current_bss->chanspec == chanspec) {
			return;
		}
	}
	/* If none matches, make cfg to NULL */
	(*cfg) = NULL;
}

/*  wrapper for btcx code portability */
static int8
wlc_btc_get_wlrssi(wlc_bsscfg_t *cfg)
{
	if (cfg != NULL) {
		return cfg->link->rssi;
	} else {
		return WLC_RSSI_INVALID;
	}
}

/* Enable/Disable the desense mode based on WL RSSI
 * This is a common function for both 2G and 5G.
 * The function sets the de-sense level from the current btc_profile
 */
static uint8
wlc_btc_ucm_elect_desense(wlc_info_t *wlc, wlc_btcx_profile_t *prof, int band)
{
	wlc_btc_info_t *btch = wlc->btch;
	int count = 0;
	int wlrssi = btch->profile_info->wlrssi;
	int8 desense_state = WLC_UCM_INVALID, desense = FALSE;
	/*  Desense is chosen based on the threshold levels and is OFF in rssi invalid case */
	if (wlrssi < prof->desense_wl_hi_lo_rssi_thresh) {
		desense_state = WLC_UCM_DESENSE_OFF;
	} else if ((wlrssi != WLC_RSSI_INVALID) && (wlrssi > prof->desense_wl_lo_hi_rssi_thresh)) {
		desense_state = WLC_UCM_DESENSE_ON;
	} else {
		return ((btch->profile_info->desense_state == WLC_UCM_INVALID) ?
				WLC_UCM_DESENSE_OFF : btch->profile_info->desense_state);
	}

	/* Set/Reset the desense levels */
	if ((desense_state == WLC_UCM_DESENSE_ON) && (btch->bth_active)) {
		for (count = 0; count < prof->chain_attr_count; count++) {
			if (prof->chain_attr[count].desense_level != WLC_UCM_DESENSE_OFF) {
				/* if any of the cores have a valid desense value,
				 * the state is reported as Desense ON
				 */
				desense = TRUE;
			}
			wlc->btch->profile_info->ucm_restage_rxgain_level[count] =
				prof->chain_attr[count].desense_level;
		}
	} else {
		for (count = 0; count < prof->chain_attr_count; count++) {
			wlc->btch->profile_info->ucm_restage_rxgain_level[count] =
				WLC_UCM_DESENSE_OFF;
		}
	}
	if (desense == FALSE) {
		desense_state = WLC_UCM_DESENSE_OFF;
	}
	return desense_state;
}
/* Sets the desense level for each core
 * The de-sense array is directly passed to the Phy function
 */
static void
wlc_btc_ucm_set_desense(wlc_btc_info_t *btc, wlc_btcx_profile_t *prof, int band)
{
	wlc_info_t *wlc = (wlc_info_t *)btc->wlc;
	int err;
	err = phy_btcx_ucm_set_desense_rxgain(WLC_PI(wlc), band,
			prof->chain_attr_count, btc->profile_info->ucm_restage_rxgain_level);
	if (err) {
		WL_ERROR(("wl%d: BTC restage rxgain (%x) OFF: RSSI %d bt %d (err %d)\n",
			wlc->pub->unit, wlc->stf->rxchain, wlc->primary_bsscfg->link->rssi,
			btc->bth_active, err));
	}
}

/* API for electing and setting de-sense mode */
static void
wlc_btc_ucm_desense(wlc_info_t *wlc, wlc_btcx_profile_t *prof, int band)
{
	int8 desense_state;
	/* Elect desense settings */
	desense_state = wlc_btc_ucm_elect_desense(wlc, prof, band);
	/* apply desense settings */
	if (desense_state != wlc->btch->profile_info->desense_state) {
		wlc_btc_ucm_set_desense(wlc->btch, prof, band);
		wlc->btch->profile_info->desense_state = desense_state;
	}
}

/* Elects the btcx mode based on the WL/BT RSSI
 * The current mode and the state of the btcx profile is
 * stored in the local structure, based on which thresholds are chosen
 */
static int
wlc_btc_ucm_elect_mode(wlc_info_t *wlc, wlc_btcx_profile_t *prof)
{
	int mode = 0;
	int mode_state = wlc->btch->profile_info->mode_state;
	int wlrssi = wlc->btch->profile_info->wlrssi;
	int btrssi = wlc->btch->bt_rssi;

	UCM_TRACE(("%s: wl rssi - %d, bt rssi - %d, mode_state - %d\n", __FUNCTION__,
		wlrssi, btrssi, mode_state));

	/* If all the modes are set to same, no need to compare RSSI */
	if ((prof->mode_strong_wl_bt ==  prof->mode_weak_bt) &&
		(prof->mode_weak_bt == prof->mode_weak_wl) &&
		(prof->mode_weak_wl ==  prof->mode_weak_wl_bt)) {
		mode = prof->mode_strong_wl_bt;
		wlc->btch->profile_info->mode_state = WLC_UCM_MODE_STRONG_WL_BT;
		UCM_TRACE(("%s: All modes are same. Setting the mode as %d\n", __FUNCTION__, mode));
		return mode;
	}

	/* If WL and BT RSSI are invalid, weak_wl_bt mode is selected */
	if ((wlrssi == WLC_RSSI_INVALID) && (btrssi == WLC_UCM_BT_RSSI_INVALID)) {
		mode = prof->mode_weak_wl_bt;
		wlc->btch->profile_info->mode_state = WLC_UCM_MODE_WEAK_WL_BT;
		UCM_TRACE(("%s: WL and BT RSSI are invalid. setting mode to %d\n",
			__FUNCTION__, mode));
		return mode;
	}

	/* WL and BT RSSI Invalid check is not done here
	 * If wlrssi is 0, the mode will switch between strong_wl_bt and weak_bt based on BT RSSI.
	 * If btrssi is 0, the mode will switch between strong_wl_bt and weak_wl based on WL RSSI.
	 */

	switch (mode_state) {
		case WLC_UCM_MODE_STRONG_WL_BT:
			mode = prof->mode_strong_wl_bt;
			if ((wlrssi < prof->mode_wl_hi_lo_rssi_thresh) &&
					(btrssi < prof->mode_bt_hi_lo_rssi_thresh)) {
				mode = prof->mode_weak_wl_bt;
				wlc->btch->profile_info->mode_state = WLC_UCM_MODE_WEAK_WL_BT;
			} else if (wlrssi < prof->mode_wl_hi_lo_rssi_thresh) {
				mode = prof->mode_weak_wl;
				wlc->btch->profile_info->mode_state = WLC_UCM_MODE_WEAK_WL;
			} else if (btrssi < prof->mode_bt_hi_lo_rssi_thresh) {
				mode = prof->mode_weak_bt;
				wlc->btch->profile_info->mode_state = WLC_UCM_MODE_WEAK_BT;
			}
			break;
		case WLC_UCM_MODE_WEAK_WL:
			mode = prof->mode_weak_wl;
			if ((wlrssi > prof->mode_wl_lo_hi_rssi_thresh) &&
					(btrssi > prof->mode_bt_lo_hi_rssi_thresh)) {
				mode = prof->mode_strong_wl_bt;
				wlc->btch->profile_info->mode_state = WLC_UCM_MODE_STRONG_WL_BT;
			} else if ((wlrssi < prof->mode_wl_hi_lo_rssi_thresh) &&
					(btrssi < prof->mode_bt_hi_lo_rssi_thresh)) {
				mode = prof->mode_weak_wl_bt;
				wlc->btch->profile_info->mode_state = WLC_UCM_MODE_WEAK_WL_BT;
			} else if (btrssi < prof->mode_bt_hi_lo_rssi_thresh) {
				mode = prof->mode_weak_bt;
				wlc->btch->profile_info->mode_state = WLC_UCM_MODE_WEAK_BT;
			}
			break;
		case WLC_UCM_MODE_WEAK_BT:
			mode = prof->mode_weak_bt;
			if ((wlrssi > prof->mode_wl_lo_hi_rssi_thresh) &&
					(btrssi > prof->mode_bt_lo_hi_rssi_thresh)) {
				mode = prof->mode_strong_wl_bt;
				wlc->btch->profile_info->mode_state = WLC_UCM_MODE_STRONG_WL_BT;
			} else if ((wlrssi < prof->mode_wl_hi_lo_rssi_thresh) &&
					(btrssi < prof->mode_bt_hi_lo_rssi_thresh)) {
				mode = prof->mode_weak_wl_bt;
				wlc->btch->profile_info->mode_state = WLC_UCM_MODE_WEAK_WL_BT;
			} else if (wlrssi < prof->mode_wl_hi_lo_rssi_thresh) {
				mode = prof->mode_weak_wl;
				wlc->btch->profile_info->mode_state = WLC_UCM_MODE_WEAK_WL;
			}
			break;
		case WLC_UCM_MODE_WEAK_WL_BT:
			mode = prof->mode_weak_wl_bt;
			if ((wlrssi > prof->mode_wl_lo_hi_rssi_thresh) &&
					(btrssi > prof->mode_bt_lo_hi_rssi_thresh)) {
				mode = prof->mode_strong_wl_bt;
				wlc->btch->profile_info->mode_state = WLC_UCM_MODE_STRONG_WL_BT;
			} else if (wlrssi < prof->mode_wl_hi_lo_rssi_thresh) {
				mode = prof->mode_weak_wl;
				wlc->btch->profile_info->mode_state = WLC_UCM_MODE_WEAK_WL;
			} else if (btrssi < prof->mode_bt_hi_lo_rssi_thresh) {
				mode = prof->mode_weak_bt;
				wlc->btch->profile_info->mode_state = WLC_UCM_MODE_WEAK_BT;
			}
			break;
		default:
			ASSERT(0);
	}
	return mode;
}

/* Function will return the core mask to be used to send ACK
* The core mask and Ack power are chosen based on the WL RSSI
* The core with -128 dBm Ack power is skipped.
* The Ack power to be used is stored in the local structure
* which is sent to the Phy function.
*/
static int
wlc_btc_ucm_elect_sisoack_core(wlc_info_t *wlc, wlc_btcx_profile_t *prof, int8 mode)
{
	wlc_btc_info_t *btch = wlc->btch;
	int core_mask = 0, count = 0;
	int wlrssi = btch->profile_info->wlrssi;

	/* During init, if the wlrssi falls between the hi and low thresholds
	 * we push the rssi to strong threshold level and respective core\ack power is used
	 */
	if ((btch->siso_ack == 0) &&
		(wlrssi > prof->ack_pwr_wl_hi_lo_rssi_thresh) &&
		(wlrssi < prof->ack_pwr_wl_lo_hi_rssi_thresh)) {
		wlrssi = prof->ack_pwr_wl_lo_hi_rssi_thresh;
	}

	if (wlrssi <= prof->ack_pwr_wl_hi_lo_rssi_thresh) {
		for (count = 0; count < prof->chain_attr_count; count++) {
			if (prof->chain_attr[count].ack_pwr_weak_rssi != WLC_UCM_NO_TRANSMISSION)
				core_mask |= 1 << count;
			/* PHY expects in half dBm */
			btch->profile_info->ack_pwr[count] =
				(prof->chain_attr[count].ack_pwr_weak_rssi/2);
		}
	} else if ((wlrssi >= prof->ack_pwr_wl_lo_hi_rssi_thresh) && (wlrssi != WLC_RSSI_INVALID)) {
		for (count = 0; count < prof->chain_attr_count; count++) {
			if (prof->chain_attr[count].ack_pwr_strong_rssi != WLC_UCM_NO_TRANSMISSION)
				core_mask |= 1 << count;
			/* PHY expects in half dBm */
			btch->profile_info->ack_pwr[count] =
				(prof->chain_attr[count].ack_pwr_strong_rssi/2);
		}
	} else {
		core_mask = btch->siso_ack;
	}
	return core_mask;
}

/* Updates the per chain Tx power to be used for each btc modes
 * For parallel mode, the power from the profile structure is used based on WL RSSI
 * For other modes, the power is set to WLC_UCM_MAX_POWER
 */
static uint8
wlc_btc_ucm_elect_chain_pwr(wlc_info_t *wlc, wlc_btcx_profile_t *prof)
{
	wlc_btc_info_t *btch = wlc->btch;
	int count = 0, chain_pwr_state = 0;
	int wlrssi = btch->profile_info->wlrssi;

	/* If wlrssi falls in between the thresholds
	 * we push the rssi to strong and the respective txpower is used
	 */
	if ((btch->profile_info->chain_pwr_state == WLC_UCM_INVALID) &&
		(wlrssi > prof->tx_pwr_wl_hi_lo_rssi_thresh) &&
		(wlrssi < prof->tx_pwr_wl_lo_hi_rssi_thresh)) {
		wlrssi = prof->tx_pwr_wl_lo_hi_rssi_thresh;
	}

	if (wlrssi <= prof->tx_pwr_wl_hi_lo_rssi_thresh) {
		for (count = 0; count < prof->chain_attr_count; count++) {
			btch->profile_info->chain_pwr[count] =
				prof->chain_attr[count].tx_pwr_weak_rssi;
		}
		chain_pwr_state = WLC_UCM_WEAK_TX_POWER;
	} else if (wlrssi >= prof->tx_pwr_wl_lo_hi_rssi_thresh &&
			(wlrssi != WLC_RSSI_INVALID)) {
		for (count = 0; count < prof->chain_attr_count; count++) {
			btch->profile_info->chain_pwr[count] =
				prof->chain_attr[count].tx_pwr_strong_rssi;
		}
		chain_pwr_state = WLC_UCM_STRONG_TX_POWER;
	} else {
		chain_pwr_state = btch->profile_info->chain_pwr_state;
	}
	return chain_pwr_state;
}

static void
wlc_btc_clear_profile_info(wlc_btc_info_t *btc)
{
	btc->profile_info->mode_state       = WLC_UCM_MODE_WEAK_WL_BT;
	btc->profile_info->desense_state    = WLC_UCM_INVALID;
	btc->profile_info->chain_pwr_state  = WLC_UCM_INVALID;
	btc->profile_info->simrx            = FALSE;
}
/* Main function of UCM, where btcx mode is choosen based on WL/BT RSSI and
 * respective settings are applied.
 * Called in btcx_watchdog and all WL scan related areas.
 */
int
wlc_btc_apply_profile(struct wlc_info *wlc, wlc_bsscfg_t *bsscfg)
{
	int err = BCME_OK;
	wlc_btcx_profile_t *btcx_cur_profile, **profile_array = wlc->btch->profile_array;
	wlc_btc_info_t *btch = wlc->btch;
	chanspec_t chanspec;
	int new_band = 0, mode = 0;
	uint8 new_profile_idx = 0, chain_pwr_state = 0;

	/* delay apply profile until MAC is up */
	if (!wlc->clk) {
		err = BCME_NOCLK;
		WL_ERROR(("%s wlc down, err - %d\n", __FUNCTION__, err));
		goto finish;
	}

	if (!btch) {
		err = BCME_ERROR;
		WL_ERROR(("%s invalid btch, err - %d\n", __FUNCTION__, err));
		goto finish;
	}

	/* get chanspec from current bsscfg, if not take primary chanspec */
	chanspec = (bsscfg ? bsscfg->current_bss->chanspec : wlc->chanspec);
	/* get band from the chanspec */
	new_band = CHSPEC_BANDTYPE(chanspec);
	/* choose the profile to be applied based on Band */
	new_profile_idx = (new_band == WLC_BAND_2G) ?
		btch->profile_active_2g : btch->profile_active_5g;

	/* During Init, index is set as Invalid
	 * So unless the mode is set explicitly, btcx profile will not be applied
	 */
	if (new_profile_idx == WLC_UCM_PROFILE_UN_INIT) {
#ifndef WL_TEST
		if (btch->bth_active) {
			WL_PRINT(("%s : UCM Profile not selected/activated\n", __FUNCTION__));
		}
#endif // endif
		err = BCME_OK;
		goto finish;
	}
	/* clear the states when there is a change in the profile
	 * or when there is a change in chanspec
	 */
	if ((new_profile_idx != btch->profile_info->curr_profile_idx) ||
		(chanspec != btch->profile_info->curr_chanspec)) {
		wlc_btc_clear_profile_info(btch);
		btch->profile_info->curr_profile_idx = new_profile_idx;
		btch->profile_info->curr_chanspec = chanspec;
	}

	btcx_cur_profile = profile_array[new_profile_idx];

	/* update WL RSSI */
	btch->profile_info->wlrssi = wlc_btc_get_wlrssi(bsscfg);
	/* choose BTCOEX mode */
	mode = wlc_btc_ucm_elect_mode(wlc, btcx_cur_profile);
	/* Set only if there is a change in the mode */
	if (mode != wlc_btc_mode_get(wlc)) {
		err = wlc_btc_mode_set(wlc, mode);
		if (err) {
			WL_ERROR(("%s Mode set failed, err - %d\n", __FUNCTION__, err));
			goto finish;
		}
		btch->profile_info->simrx = (mode == WL_BTC_HYBRID) ? TRUE : FALSE;
		/* Notify hybrid mode operation to BT */
		wlc_btcx_notify_hybrid_mode_to_bt(wlc);
	}

	UCM_TRACE(("%s: chanspec - %x, new profile id - %d,"
		" wl rssi - %d, bt rssi - %d, mode - %d\n", __FUNCTION__,
		chanspec, new_profile_idx, btch->profile_info->wlrssi, btch->bt_rssi, mode));

	if (wlc->clk) {
		wlc_btc_set_ps_protection(wlc, wlc->primary_bsscfg);
	}

	/* Elects and sets the de-sense level based on the WL RSSI */
	wlc_btc_ucm_desense(wlc, btcx_cur_profile, new_band);

	/* Siso Ack related configurations */
	if (mode == WL_BTC_HYBRID) {
		/* setChain_Ack */
		err = wlc_btc_siso_ack_set(
			wlc, wlc_btc_ucm_elect_sisoack_core(wlc, btcx_cur_profile, mode), TRUE);
		if (err) {
			WL_ERROR(("%s: Siso Ack(%x) set to ucode failed, err - %d\n",
				__FUNCTION__, btch->siso_ack, err));
			goto finish;
		}
		if (btch->siso_ack && wlc->clk) {
			err = phy_btcx_ucm_update_siso_resp_offset(WLC_PI(wlc),
				btch->profile_info->ack_pwr, btcx_cur_profile->chain_attr_count);
			if (err) {
				WL_ERROR(("%s: Siso Ack(%x) set to PHY failed, err - %d\n",
					__FUNCTION__, btch->siso_ack, err));
				goto finish;
			}
		}
	} else {
		/* Clear the siso ack core mask */
		err = wlc_btc_siso_ack_set(wlc, 0, TRUE);
		if (err) {
			WL_ERROR(("%s: Siso Ack(%x) set to ucode failed, err - %d\n",
				__FUNCTION__, btch->siso_ack, err));
			goto finish;
		}
	}

	/* Tx Power related configurations
	 * Sets the Tx power only if there is a change in the Tx power
	 */
	chain_pwr_state = wlc_btc_ucm_elect_chain_pwr(wlc, btcx_cur_profile);
	if (chain_pwr_state != btch->profile_info->chain_pwr_state) {
		/* Update the current state */
		btch->profile_info->chain_pwr_state = chain_pwr_state;
		/* Pass on btcoex profile txpwrcap values to PHY */
		err = phy_btcx_ucm_txpwrcaplmt(WLC_PI(wlc),
				&btch->profile_info->chain_pwr[0], TRUE);
		if (err) {
			WL_ERROR(("%s: Setting TXPWRCAP failed, err - %d\n", __FUNCTION__, err));
			goto finish;
		}
	}
finish:
	return err;
}
#endif /* WL_UCM */

#ifdef WLBTCPROF
#ifdef WLBTCPROF_EXT
int
wlc_btcx_set_ext_profile_param(wlc_info_t *wlc)
{
	wlc_btc_info_t *btch = wlc->btch;
	int err = BCME_ERROR;
	wlc_btc_profile_t *select_profile;

	select_profile = &btch->btc_profile[BTC_PROFILE_2G].select_profile;
	/* program bt and wlan threshold and hysteresis data */
	btch->prot_btrssi_thresh = -1 * select_profile->btc_btrssi_thresh;
	btch->btrssi_hyst = select_profile->btc_btrssi_hyst;
	btch->wlrssi_hyst = select_profile->btc_wlrssi_hyst;
	if (wlc->clk) {
		wlc_btc_set_ps_protection(wlc, wlc->primary_bsscfg); /* enable */

		if (select_profile->btc_num_desense_levels == MAX_BT_DESENSE_LEVELS_4350) {
			err = wlc_phy_btc_set_max_siso_resp_pwr(WLC_PPI(wlc),
				&select_profile->btc_max_siso_resp_power[0],
				MAX_BT_DESENSE_LEVELS_4350);
		}
	}

	return err;
}
#endif /* WLBTCPROF_EXT */
int
wlc_btcx_set_btc_profile_param(struct wlc_info *wlc, chanspec_t chanspec, bool force)
{
	int err = BCME_OK;
	wlc_btc_profile_t *btc_cur_profile, *btc_prev_profile;
	wlc_btc_info_t *btch = wlc->btch;
	int btc_inactive_offset[WL_NUM_TXCHAIN_MAX] = {0, 0, 0, 0};

	int band = CHSPEC_BANDTYPE(chanspec);

	/* "Disable Everything" profile for scanning */
	static wlc_btc_profile_t btc_scan_profile;
	int btc_scan_profile_init_done = 0;
	if (btc_scan_profile_init_done == 0) {
		btc_scan_profile_init_done = 1;
		bzero(&btc_scan_profile, sizeof(btc_scan_profile));
		btc_scan_profile.mode = WL_BTC_HYBRID;
		btc_scan_profile.desense_level = 0;
		btc_scan_profile.chain_power_offset[0] = btc_inactive_offset;
		btc_scan_profile.chain_ack[0] = wlc->stf->hw_txchain;
		btc_scan_profile.num_chains = WLC_BITSCNT(wlc->stf->hw_txchain);
	}

	if (!btch)
	{
		WL_INFORM(("%s invalid btch\n", __FUNCTION__));
		err = BCME_ERROR;
		goto finish;
	}

	if (!btch->btc_prev_connect)
	{
		WL_INFORM(("%s invalid btc_prev_connect\n", __FUNCTION__));
		err = BCME_ERROR;
		goto finish;
	}

	if (!btch->btc_profile)
	{
		WL_INFORM(("%s invalid btc_profile\n", __FUNCTION__));
		err = BCME_ERROR;
		goto finish;
	}

	if (band == WLC_BAND_2G)
	{
		if (!force) {
			if (btch->btc_profile[BTC_PROFILE_2G].enable == BTC_PROFILE_OFF)
				goto finish;

			if (btch->btc_prev_connect->prev_band == WLC_BAND_2G)
				goto finish;

			if ((btch->btc_prev_connect->prev_2G_mode == BTC_PROFILE_DISABLE) &&
			(btch->btc_profile[BTC_PROFILE_2G].enable == BTC_PROFILE_DISABLE))
				goto finish;
		}

		btc_cur_profile = &btch->btc_profile[BTC_PROFILE_2G].select_profile;
		btc_prev_profile = &btch->btc_prev_connect->prev_2G_profile;
		if (btch->btc_profile[BTC_PROFILE_2G].enable == BTC_PROFILE_DISABLE)
		{
			btch->btc_prev_connect->prev_2G_mode = BTC_PROFILE_DISABLE;
			memset(btc_cur_profile, 0, sizeof(wlc_btc_profile_t));
			btc_cur_profile->btc_wlrssi_thresh = BTC_WL_RSSI_DEFAULT;
			btc_cur_profile->btc_btrssi_thresh = BTC_BT_RSSI_DEFAULT;
			if (CHIPID(wlc->pub->sih->chip) == BCM4350_CHIP_ID) {
				btc_cur_profile->btc_wlrssi_hyst = BTC_WL_RSSI_HYST_DEFAULT_4350;
				btc_cur_profile->btc_btrssi_hyst = BTC_BT_RSSI_HYST_DEFAULT_4350;
				btc_cur_profile->btc_max_siso_resp_power[0] =
					BTC_WL_MAX_SISO_RESP_POWER_TDD_DEFAULT_4350;
				btc_cur_profile->btc_max_siso_resp_power[1] =
					BTC_WL_MAX_SISO_RESP_POWER_HYBRID_DEFAULT_4350;
			}
		}
		else
		{
			btch->btc_prev_connect->prev_2G_mode = BTC_PROFILE_ENABLE;
		}

		btch->btc_prev_connect->prev_band = WLC_BAND_2G;
	}
	else
	{
		if (!force) {
			if (btch->btc_profile[BTC_PROFILE_5G].enable == BTC_PROFILE_OFF)
				goto finish;

			if (btch->btc_prev_connect->prev_band == WLC_BAND_5G)
				goto finish;

			if ((btch->btc_prev_connect->prev_5G_mode == BTC_PROFILE_DISABLE) &&
			(btch->btc_profile[BTC_PROFILE_5G].enable == BTC_PROFILE_DISABLE))
				goto finish;
		}

		btc_cur_profile = &btch->btc_profile[BTC_PROFILE_5G].select_profile;
		btc_prev_profile = &btch->btc_prev_connect->prev_5G_profile;

		if (btch->btc_profile[BTC_PROFILE_5G].enable == BTC_PROFILE_DISABLE)
		{
			btch->btc_prev_connect->prev_5G_mode = BTC_PROFILE_DISABLE;
			memset(&btch->btc_profile[BTC_PROFILE_5G].select_profile,
				0, sizeof(wlc_btc_profile_t));
		}
		else
		{
			btch->btc_prev_connect->prev_5G_mode = BTC_PROFILE_ENABLE;
		}

		btch->btc_prev_connect->prev_band = WLC_BAND_5G;
		btc_cur_profile = &btch->btc_profile[BTC_PROFILE_5G].select_profile;
		btc_prev_profile = &btch->btc_prev_connect->prev_5G_profile;
	}

	/* New request to disable btc profiles during scan */
	if (SCAN_IN_PROGRESS(wlc->scan)) {
		btc_cur_profile = &btc_scan_profile;
		/* During Scans set btcmode to hybrid
		*  but zero all other coex params
		*/
		btc_cur_profile->mode = WL_BTC_HYBRID;
	}

	/* New request to disable btc profiles during scan */
	if (SCAN_IN_PROGRESS(wlc->scan) && (band == WLC_BAND_2G)) {
		btc_cur_profile = &btc_scan_profile;
	}

	WL_BTCPROF(("%s chanspec 0x%x\n", __FUNCTION__, chanspec));

	/* setBTCOEX_MODE */
	err = wlc_btc_mode_set(wlc, btc_cur_profile->mode);
	WL_BTCPROF(("btc mode %d\n", btc_cur_profile->mode));
	if (err)
	{
		err = BCME_ERROR;
		goto finish;
	}

	/* setDESENSE_LEVEL */
	btch->restage_rxgain_level = btc_cur_profile->desense_level;
	WL_BTCPROF(("btc desense level %d\n", btc_cur_profile->desense_level));

	/* setDESENSE */
	btch->restage_rxgain_force =
		(btch->btc_profile[BTC_PROFILE_2G].select_profile.desense)?
		BTC_RXGAIN_FORCE_2G_ON : 0;
	btch->restage_rxgain_force |=
		(btch->btc_profile[BTC_PROFILE_5G].select_profile.desense)?
		BTC_RXGAIN_FORCE_5G_ON : 0;
	WL_BTCPROF(("btc rxgain force 0x%x\n", btch->restage_rxgain_force));

	/* setting 2G thresholds, 5G thresholds are not used */
	btch->restage_rxgain_on_rssi_thresh =
		(uint8)((btch->btc_profile[BTC_PROFILE_2G].select_profile.desense_thresh_high *
		-1) & 0xFF);
	btch->restage_rxgain_off_rssi_thresh =
		(uint8)((btch->btc_profile[BTC_PROFILE_2G].select_profile.desense_thresh_low *
		-1) & 0xFF);
	WL_BTCPROF(("btc rxgain on 0x%x rxgain off 0x%x\n",
		btch->restage_rxgain_on_rssi_thresh,
		btch->restage_rxgain_off_rssi_thresh));

	/* check the state of bt_active signal */
		if (BT3P_HW_COEX(wlc) && wlc->clk) {
			wlc_bmac_btc_period_get(wlc->hw, &btch->bth_period,
				&btch->bth_active, &btch->agg_off_bm, &wlc->btch->acl_last_ts,
				&wlc->btch->a2dp_last_ts);
		}

	/* apply desense settings */
	wlc_btcx_desense(btch, band);

	/* setChain_Ack */
	wlc_btc_siso_ack_set(wlc, (int16)btc_cur_profile->chain_ack[0], TRUE);
	WL_BTCPROF(("btc chain ack 0x%x num chains %d\n",
		btc_cur_profile->chain_ack[0],
		btc_cur_profile->num_chains));

	/* setTX_CHAIN_POWER */
	if (btch->bth_active) {
	wlc_channel_set_tx_power(wlc, band, btc_cur_profile->num_chains,
		&btc_cur_profile->chain_power_offset[0],
		&btc_prev_profile->chain_power_offset[0]);
	*btc_prev_profile = *btc_cur_profile;
	} else {
		wlc_channel_set_tx_power(wlc, band, btc_cur_profile->num_chains,
			&btc_inactive_offset[0], &btc_prev_profile->chain_power_offset[0]);
	}

#ifdef WLBTCPROF_EXT
	/* set hybrid params */
	if ((band == WLC_BAND_2G) && (CHIPID(wlc->pub->sih->chip) == BCM4350_CHIP_ID)) {
		if ((err = wlc_btcx_set_ext_profile_param(wlc)) == BCME_ERROR) {
			WL_INFORM(("%s Unable to program siso ack powers\n", __FUNCTION__));
		}
	} else {
		for (i = 0; i < MAX_BT_DESENSE_LEVELS; i++) {
			btc_cur_profile->btc_max_siso_resp_power[i] =
				BTC_WL_MAX_SISO_RESP_POWER_TDD_DEFAULT;
		}

		if ((btc_cur_profile->btc_num_desense_levels == MAX_BT_DESENSE_LEVELS_4350) &&
			wlc->clk) {
			err = wlc_phy_btc_set_max_siso_resp_pwr(WLC_PPI(wlc),
				&btc_cur_profile->btc_max_siso_resp_power[0],
				MAX_BT_DESENSE_LEVELS_4350);
		}
	}
#endif /* WLBTCPROF_EXT */

finish:
	return err;
}

int
wlc_btcx_select_profile_set(wlc_info_t *wlc, uint8 *pref, int len)
{
	wlc_btc_info_t *btch;

	btch = wlc->btch;
	if (!btch)
	{
		WL_INFORM(("%s invalid btch\n", __FUNCTION__));
		return BCME_ERROR;
	}

	if (pref)
	{
		if (!bcmp(pref, btch->btc_profile, len))
			return BCME_OK;

		bcopy(pref, btch->btc_profile, len);

		if (wlc_btcx_set_btc_profile_param(wlc, wlc->chanspec, TRUE))
		{
			WL_ERROR(("wl%d: %s: setting btc profile first time error: chspec %d!\n",
				wlc->pub->unit, __FUNCTION__, CHSPEC_CHANNEL(wlc->chanspec)));
		}

		return BCME_OK;
	}

	return BCME_ERROR;
}

int
wlc_btcx_select_profile_get(wlc_info_t *wlc, uint8 *pref, int len)
{
	wlc_btc_info_t *btch = wlc->btch;

	if (!btch)
	{
		WL_INFORM(("%s invalid btch\n", __FUNCTION__));
		return BCME_ERROR;
	}

	if (pref)
	{
		bcopy(btch->btc_profile, pref, len);
		return BCME_OK;
	}

	return BCME_ERROR;
}
#endif /* WLBTCPROF */

int
wlc_btc_siso_ack_set(wlc_info_t *wlc, int16 siso_ack, bool force)
{
	wlc_btc_info_t *btch = wlc->btch;

	if (!btch)
		return BCME_ERROR;

	if (force) {
		if (siso_ack == AUTO)
			btch->siso_ack_ovr = FALSE;
		else {
			/* sanity check forced value */
			if (!(siso_ack & TXCOREMASK))
				return BCME_BADARG;
			btch->siso_ack = siso_ack;
			btch->siso_ack_ovr = TRUE;
		}
	}

	if (!btch->siso_ack_ovr) {
		/* no override, set siso_ack according to btc_mode/chipids/boardflag etc. */
		if (btch->siso_ack_nvram) {
			/* if siso ack is set in nvram file, just use it */
			siso_ack = btch->siso_ack_nvram;
		} else if (siso_ack == AUTO) {
			siso_ack = 0;
			if (wlc->pub->boardflags & BFL_FEM_BT) {
				/* check boardflag: antenna shared w BT */
				/* futher check srom, nvram */
				if (wlc->hw->btc->btcx_aa == 0x3) { /* two antenna */
					if (wlc->pub->boardflags2 &
					    BFL2_BT_SHARE_ANT0) { /* core 0 shared */
						siso_ack = 0x2;
					} else {
						siso_ack = 0x1;
					}
				} else if (wlc->hw->btc->btcx_aa == 0x7) { /* three antenna */
					; /* not supported yet */
				}
			}
		}
		btch->siso_ack = siso_ack;
	}

	wlc_btc_sisoack_shm(wlc);

	return BCME_OK;
}

int16
wlc_btc_siso_ack_get(wlc_info_t *wlc)
{
	return wlc->btch->siso_ack;
}

void
wlc_btc_sisoack_shm(wlc_info_t *wlc)
{
	uint16 sisoack;

	if (wlc->clk) {
		sisoack = wlc_bmac_read_shm(wlc->hw, M_COREMASK_BTRESP(wlc));

		sisoack = sisoack & BTC_SISOACK_TXPWR_MASK; /* txpwr offset set by phy */
		sisoack = sisoack | (wlc->btch->siso_ack & BTC_SISOACK_CORES_MASK);

		wlc_bmac_write_shm(wlc->hw, M_COREMASK_BTRESP(wlc), sisoack);
	}
}

static int
wlc_btc_wire_set(wlc_info_t *wlc, int int_val)
{
	int err;
	err = wlc_bmac_btc_wire_set(wlc->hw, int_val);
	wlc->btch->wire = wlc_bmac_btc_wire_get(wlc->hw);
	return err;
}

int
wlc_btc_wire_get(wlc_info_t *wlc)
{
	return wlc->btch->wire;
}

void wlc_btc_mode_sync(wlc_info_t *wlc)
{
	wlc->btch->mode = wlc_bmac_btc_mode_get(wlc->hw);
	wlc->btch->task = wlc_bmac_btc_task_get(wlc->hw);
	wlc->btch->wire = wlc_bmac_btc_wire_get(wlc->hw);
}

uint8 wlc_btc_save_host_requested_pm(wlc_info_t *wlc, uint8 val)
{
	return (wlc->btch->host_requested_pm = val);
}

bool wlc_btc_get_bth_active(wlc_info_t *wlc)
{
	return wlc->btch->bth_active;
}

uint16 wlc_btc_get_bth_period(wlc_info_t *wlc)
{
	return wlc->btch->bth_period;
}

static int
wlc_btc_flags_idx_set(wlc_info_t *wlc, int int_val, int int_val2)
{
	return wlc_bmac_btc_flags_idx_set(wlc->hw, int_val, int_val2);
}

static int
wlc_btc_flags_idx_get(wlc_info_t *wlc, int int_val)
{
	return wlc_bmac_btc_flags_idx_get(wlc->hw, int_val);
}

int
wlc_btc_params_set(wlc_info_t *wlc, int int_val, int int_val2)
{
	return wlc_bmac_btc_params_set(wlc->hw, int_val, int_val2);
}

int
wlc_btc_params_get(wlc_info_t *wlc, int int_val)
{
	return wlc_bmac_btc_params_get(wlc->hw, int_val);
}

#ifdef WLBTCPROF_EXT
int wlc_btc_profile_set(wlc_info_t *wlc, int int_val, int iovar_id)
{
	wlc_btc_info_t *btch = wlc->btch;
	wlc_btc_profile_t *select_profile;

	select_profile = &btch->btc_profile[BTC_PROFILE_2G].select_profile;
	switch (iovar_id)
	{
	case IOV_BTC_WLRSSI_THRESH:
		select_profile->btc_wlrssi_thresh = (int8) int_val;
		break;
	case IOV_BTC_WLRSSI_HYST:
		select_profile->btc_wlrssi_hyst = (uint8) int_val;
		break;
	case IOV_BTC_BTRSSI_THRESH:
		select_profile->btc_btrssi_thresh = (int8) int_val;
		break;
	case IOV_BTC_BTRSSI_HYST:
		select_profile->btc_btrssi_hyst = (uint8) int_val;
		break;
	default:
		break;
	}
	return 1;
}

int wlc_btc_profile_get(wlc_info_t *wlc, int iovar_id)
{
	wlc_btc_info_t *btch = wlc->btch;
	wlc_btc_profile_t *select_profile;

	select_profile = &btch->btc_profile[BTC_PROFILE_2G].select_profile;
	switch (iovar_id)
	{
	case IOV_BTC_WLRSSI_THRESH:
		return select_profile->btc_wlrssi_thresh;
		break;
	case IOV_BTC_WLRSSI_HYST:
		return select_profile->btc_wlrssi_hyst;
		break;
	case IOV_BTC_BTRSSI_THRESH:
		return select_profile->btc_btrssi_thresh;
		break;
	case IOV_BTC_BTRSSI_HYST:
		return select_profile->btc_btrssi_hyst;
		break;
	default:
		break;
	}
	return 1;
}
#endif /* WLBTCPROF_EXT */

static void
wlc_btc_stuck_war50943(wlc_info_t *wlc, bool enable)
{
	wlc_bmac_btc_stuck_war50943(wlc->hw, enable);
}

uint
wlc_btc_frag_threshold(wlc_info_t *wlc, struct scb *scb)
{
	ratespec_t rspec;
	uint rate, thresh;
	wlc_bsscfg_t *cfg;

	/* Make sure period is known */
	if (wlc->btch->bth_period == 0)
		return 0;

	ASSERT(scb != NULL);

	cfg = SCB_BSSCFG(scb);
	ASSERT(cfg != NULL);

	/* if BT SCO is ongoing, packet length should not exceed 1/2 of SCO period */
	rspec = wlc_get_rspec_history(cfg);
	rate = RSPEC2KBPS(rspec);

	/*  use one half of the duration as threshold.  convert from usec to bytes */
	/* thresh = (bt_period * rate) / 1000 / 8 / 2  */
	thresh = (wlc->btch->bth_period * rate) >> 14;

	if (thresh < DOT11_MIN_FRAG_LEN)
		thresh = DOT11_MIN_FRAG_LEN;
	return thresh;
}

#if defined(STA) && defined(BTCX_PM0_IDLE_WAR)
static void
wlc_btc_pm_adjust(wlc_info_t *wlc,  bool bt_active)
{
	wlc_bsscfg_t *cfg = wlc->primary_bsscfg;
	/* only bt is not active, set PM to host requested mode */
	if (wlc->btch->host_requested_pm != PM_FORCE_OFF) {
		if (bt_active) {
				if (PM_OFF == wlc->btch->host_requested_pm &&
				cfg->pm->PM != PM_FAST)
				wlc_set_pm_mode(wlc, PM_FAST, cfg);
		} else {
			if (wlc->btch->host_requested_pm != cfg->pm->PM)
				wlc_set_pm_mode(wlc, wlc->btch->host_requested_pm, cfg);
		}
	}
}
#endif /* STA */

#ifdef STA
static bool
wlc_btcx_check_port_open(wlc_info_t *wlc)
{
	uint idx;
	wlc_bsscfg_t *cfg;

	FOREACH_AS_STA(wlc, idx, cfg) {
		if (BSSCFG_IBSS(cfg))
			continue;
		if (!WLC_PORTOPEN(cfg))
			return (FALSE);
	}
	return (TRUE);
}
#endif  /* STA */

/* XXX FIXME
 * BTCX settings are now global but we may later on need to change it for multiple BSS
 * hence pass the bsscfg as a parm.
 */
void
wlc_btc_set_ps_protection(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	BCM_REFERENCE(bsscfg);
	if (MCHAN_ACTIVE(wlc->pub))
		return;

	if ((wlc_btc_wire_get(wlc) >= WL_BTC_3WIRE)) {
		wlc_bsscfg_t *pc;
		int btc_flags = wlc_bmac_btc_flags_get(wlc->hw);
		uint16 protections;
		uint16 active = 0;
		uint16 ps;

		pc = wlc->primary_bsscfg;
		BCM_REFERENCE(pc);

		/* if radio is disable, driver may be down, quit here */
		if (wlc->pub->radio_disabled || !wlc->pub->up)
			return;

#if defined(STA) && !defined(BCMNODOWN)
		/* ??? if ismpc, driver should be in down state if up/down is allowed */
		if (wlc->mpc && wlc_ismpc(wlc))
			return;
#endif // endif
		active = (btc_flags & WL_BTC_FLAG_ACTIVE_PROT) ? MHF3_BTCX_ACTIVE_PROT : 0;
		ps = (btc_flags & WL_BTC_FLAG_PS_PROTECT) ? MHF3_BTCX_PS_PROTECT : 0;
		BCM_REFERENCE(ps);

#ifdef STA
		/* Enable PS protection when there is only one STA/GC connection */
		if ((wlc->stas_connected == 1) && (wlc->aps_associated == 0) &&
			(wlc_ap_stas_associated(wlc->ap) == 0) &&
			wlc->ibss_bsscfgs == 0) {
				/* when WPA/PSK security is enabled
				  * wait until WLC_PORTOPEN() is TRUE
				  */
				if (wlc_btcx_check_port_open(wlc)) {
					protections = active | ps;
				} else {
					/* XXX temporarily disable protections in between
					 * association is done and key is plumbed???
					 */
					protections = 0;
				}
		}
		else if (wlc->stas_connected > 0)
			protections = active;
		else
#endif /* STA */
#ifdef AP
		if (wlc->aps_associated > 0 && wlc_ap_stas_associated(wlc->ap) > 0)
			protections = active;
		/* No protection */
		else
#endif /* AP */
			protections = 0;

		wlc_mhf(wlc, MHF3, MHF3_BTCX_ACTIVE_PROT | MHF3_BTCX_PS_PROTECT,
		        protections, WLC_BAND_2G);
#ifdef WLMCNX
		/*
		For non-VSDB the only time we turn on PS protection is when there is only
		one STA associated - primary or GC. In this case, set the BSS index in
		designated SHM location as well.
		*/
		if ((MCNX_ENAB(wlc->pub)) && (protections & ps)) {
			uint idx;
			wlc_bsscfg_t *cfg;
			int bss_idx;

			FOREACH_AS_STA(wlc, idx, cfg) {
				if (BSSCFG_IBSS(cfg))
					continue;
				bss_idx = wlc_mcnx_BSS_idx(wlc->mcnx, cfg);
				wlc_mcnx_shm_bss_idx_set(wlc->mcnx, bss_idx);
				break;
			}
		}
#endif /* WLMCNX */
	}
}

#if defined(BCMDBG)
static int wlc_dump_btcx(void *ctx, struct bcmstrbuf *b)
{
	wlc_info_t *wlc = ctx;
	uint8 idx, offset;
	uint16 hi, lo;
	uint32 buff[C_BTCX_DBGBLK_EXT_SZ/2];
	uint16 base = M_BTCX_RFACT_CTR_L(wlc);
	uint32 numwords = (D11REV_GE(wlc->hw->corerev, 128) ?
			C_BTCX_DBGBLK_EXT_SZ : C_BTCX_DBGBLK_SZ);

	if (!wlc->clk) {
		return BCME_NOCLK;
	}

	for (idx = 0; idx < numwords; idx += 2) {
		offset = idx*2;
		lo = wlc_bmac_read_shm(wlc->hw, base+offset);
		hi = wlc_bmac_read_shm(wlc->hw, base+offset+2);
		buff[idx>>1] = (hi<<16) | lo;
	}

	if (D11REV_LT(wlc->hw->corerev, 128)) {
		bcm_bprintf(b, "nrfact: %u, ntxconf: %u (%u%%), txconf_durn(us): %u\n",
				buff[0], buff[1], buff[0] ? (buff[1]*100)/buff[0]: 0,
				buff[2]);
	} else {
		bcm_bprintf(b, "nrfact: %u, nrfpri: %u, nprot: %u (%u%%), "
			    "ntxconf: %u (%u%%), txconf_durn(us): %u\n",
			    buff[0], buff[3], buff[4], buff[1] ? (buff[4]*100)/buff[1]: 0,
			    buff[1], buff[0] ? (buff[1]*100)/buff[0]: 0, buff[2]);
	}
	return 0;
}

static int wlc_clr_btcxdump(void *ctx)
{
	wlc_info_t *wlc = ctx;
	uint32 numwords = (D11REV_GE(wlc->hw->corerev, 128) ?
			C_BTCX_DBGBLK_EXT_SZ : C_BTCX_DBGBLK_SZ);

	if (!wlc->clk) {
		return BCME_NOCLK;
	}

	wlc_bmac_set_shm(wlc->hw, M_BTCX_RFACT_CTR_L(wlc), 0, numwords*2);
	return BCME_OK;
}
#endif // endif

/* Read relevant BTC params to determine if aggregation has to be enabled/disabled */
void
wlc_btcx_read_btc_params(wlc_info_t *wlc)
{
	wlc_btc_info_t *btch = wlc->btch;

	if (BT3P_HW_COEX(wlc) && wlc->clk) {
		wlc_bmac_btc_period_get(wlc->hw, &btch->bth_period,
			&btch->bth_active, &btch->agg_off_bm, &btch->acl_last_ts,
			&btch->a2dp_last_ts);
		if (!btch->bth_active && btch->btrssi_cnt) {
			wlc_btc_reset_btrssi(btch);
		}
	}
}

#ifdef WLRSDB
void
wlc_btcx_update_coex_iomask(wlc_info_t *wlc)
{
#if defined(BCMECICOEX)

	wlc_hw_info_t *wlc_hw = wlc->hw;

	/* Should come here only for RSDB capable devices */
	ASSERT(wlc_bmac_rsdb_cap(wlc_hw));

	/* In the MIMO/80p80 mode set coex_io_mask to 0x3 on core 1
	 * (i.e) mask both Txconf and Prisel on Core 1.
	 * Leave coex_io_mask on core 0 to its default value (0x0)
	 */
	if (!si_coreunit(wlc_hw->sih)) {
		d11regs_t *sregs;

		/* Unmask coex_io_mask on core0 to 0 */
		AND_REG(wlc_hw->osh, D11_COEX_IO_MASK(wlc_hw),
			((uint16)(~((1 << COEX_IOMASK_PRISEL_POS) |
				(1 << COEX_IOMASK_TXCONF_POS)))));

		/* Set: core 1 */
		sregs = si_d11_switch_addrbase(wlc_hw->sih, 1);
		/* Enable MAC control IHR access on core 1 */
		OR_REG(wlc_hw->osh, D11_MACCONTROL_ALTBASE(sregs, wlc_hw->regoffsets),
			MCTL_IHR_EN);

		/* Mask Txconf and Prisel on core 1 */
		OR_REG(wlc_hw->osh, D11_COEX_IO_MASK_ALTBASE(sregs, wlc_hw->regoffsets),
		       ((uint16)(((1 << COEX_IOMASK_PRISEL_POS) |
				(1 << COEX_IOMASK_TXCONF_POS)))));

		/* Disable MAC control IHR access on core 1 */
		/* OR_REG(wlc_hw->osh, D11_MACCONTROL_ALTBASE(sregs, wlc_hw->regoffsets),
		 *  ~MCTL_IHR_EN);
		 */

		/* Restore: core 0 */
		si_d11_switch_addrbase(wlc_hw->sih, 0);
	}
#endif /* BCMECICOEX */
}

#endif /* WLRSDB */

static void
wlc_btc_update_sco_params(wlc_info_t *wlc)
{
	uint16 holdsco_limit, grant_ratio;
	bool low_threshold = TRUE;

	if (BT3P_HW_COEX(wlc) && BAND_2G(wlc->band->bandtype)) {
		if ((wlc->btch->bth_period) && (!wlc->btch->sco_threshold_set)) {
			uint16 sco_thresh = (uint16)wlc_btc_params_get(wlc,
				BTC_FW_HOLDSCO_HI_THRESH);
			uint16 btcx_config = (uint16)wlc_btc_params_get(wlc,
				M_BTCX_CONFIG_OFFSET(wlc) >> 1);

			// Use High thresholds if BT period is higher than configured
			// threshold or if Wlan RSSI is low
			if ((sco_thresh && (wlc->btch->bth_period > sco_thresh)) ||
				(btcx_config & C_BTCX_CONFIG_LOW_RSSI)) {
				low_threshold = FALSE;
			}
			wlc->btch->sco_threshold_set = TRUE;
		} else if ((!wlc->btch->bth_period) && (wlc->btch->sco_threshold_set)) {
			wlc->btch->sco_threshold_set = FALSE;
		} else {
			return;
		}

		if (low_threshold) {
			holdsco_limit = (uint16)wlc_btc_params_get(wlc, BTC_FW_HOLDSCO_LIMIT);
			grant_ratio = (uint16)wlc_btc_params_get(wlc,
				BTC_FW_SCO_GRANT_HOLD_RATIO);
		} else {
			holdsco_limit = (uint16)wlc_btc_params_get(wlc, BTC_FW_HOLDSCO_LIMIT_HI);
			grant_ratio = (uint16)wlc_btc_params_get(wlc,
				BTC_FW_SCO_GRANT_HOLD_RATIO_HI);
		}
		wlc_btc_params_set(wlc, M_BTCX_HOLDSCO_LIMIT_OFFSET(wlc) >> 1, holdsco_limit);
		wlc_btc_params_set(wlc, M_BTCX_SCO_GRANT_HOLD_RATIO_OFFSET(wlc) >> 1, grant_ratio);
	}
}

#ifdef WLAMPDU
/*
 * Function: wlc_btcx_debounce
 * Description: COEX status bits from shmem may be inconsistent when sampled once a second
 * and may require debouncing.  An example is BLE bit in agg_off_bm, which is '1' when a
 * BLE transaction occurred within 20ms of the last transaction, and '0' when the BLE
 * transactions were more than 20ms apart.  Rather than put debounce logic in ucode, it
 * is more efficient to put in in FW.

 * This routine requires a structure to be passed in with the following arguments:
 * inputs: current, on_timedelta, off_timedelta.
 * outputs: status_debounced, status_cntr, timeStamp.
 *
 * If current is consistently set to '1' for 5 seconds, then status_debounced will
 * bet set.  Once status_debounced is '1', then current will need to be '0' consistently
 * for 5 seconds in order for status_debounced to be cleared to '0'.  Note that the '5 second'
 * is configurable via the timedelta arguments passed in.
 */
static uint16
wlc_btcx_debounce(wlc_btc_debounce_info_t *btc_debounce, uint16 curr)
{
	uint8 on_timedelta = btc_debounce->on_timedelta;
	uint8 off_timedelta = btc_debounce->off_timedelta;
	uint32 curr_time = OSL_SYSUPTIME() / MSECS_PER_SEC;

	switch ((btc_debounce->status_debounced << 2) |
		(btc_debounce->status_inprogress << 1) | (curr & 1)) {
		case 0x0:
		case 0x7:
			btc_debounce->status_timestamp = curr_time;
			break;
		case 0x1:
		case 0x5:
		case 0x2:
		case 0x6:
			btc_debounce->status_inprogress = curr;
			btc_debounce->status_timestamp = curr_time;
			break;
		case 0x3:
			if ((curr_time - btc_debounce->status_timestamp) > on_timedelta) {
				btc_debounce->status_debounced = 1;
				btc_debounce->status_timestamp = curr_time;
			}
			break;
		case 0x4:
			if ((curr_time - btc_debounce->status_timestamp) > off_timedelta) {
				btc_debounce->status_debounced = 0;
				btc_debounce->status_timestamp = curr_time;
			}
			break;
		default:
			break;
	}
	return btc_debounce->status_debounced;
}

/* Function: wlc_btcx_evaluate_agg_state
 * Description: Determine proper aggregation settings based on the
 * BT/LTE COEX Scenario. There are 2 possible scenarios related to ucode.
 * 1. The corresponding change is present in ucode. In this case the SHM
 * parameters M_BTCX_HOLDSCO_LIMIT & M_BTCX_SCO_GRANT_HOLD_RATIO are initialized
 * to the low thresholds and will get reset based on BT Period once this function
 * runs.
 * 2. The corresponding change is not present in ucode. In this case, ucode uses
 * either low or high threshold parameters as needed. This function will set
 * the low threshold parameters to low or high values as needed. When it sets
 * the low threshold parameters to high values, it has no side-effect since ucode
 * will use the high threshold parameters.
 */
static void
wlc_btcx_evaluate_agg_state(wlc_btc_info_t *btc)
{
	wlc_info_t *wlc = (wlc_info_t *)btc->wlc;
	int btc_mode = wlc_btc_mode_get(wlc);
	uint16 ble_debounced = 0, a2dp_debounced = 0, sco_active = 0, siso_mode = 0;
	uint8 hybrid_mode = 0; // btc_mode 5
	uint8 explicit_tdm_mode = 0; // btc_mode 1
	uint8 implicit_tdm_mode = 0; // btc_mode 5 operating in tdd mode
	uint8 implicit_hybrid_mode = 0; // btc_mode 5 with simrx and limited tx

	if (BT3P_HW_COEX(wlc) && BAND_2G(wlc->band->bandtype)) {
		uint8 agg_state_req;

		if (btc_mode && btc_mode != WL_BTC_PARALLEL) {

			/* Make sure STA is on the home channel to avoid changing AMPDU
			 * state during scanning
			 */
			if (AMPDU_ENAB(wlc->pub) && !SCAN_IN_PROGRESS(wlc->scan) &&
				wlc->pub->associated) {
				/* Debounce ble status, which flaps frequently depending on
				 * whether the previous BLE request was within 20ms
				 */
				ble_debounced = wlc_btcx_debounce(btc->ble_debounce,
					(wlc->btch->agg_off_bm & C_BTCX_AGGOFF_BLE));

				a2dp_debounced = wlc_btcx_debounce(btc->a2dp_debounce,
					((wlc->btch->a2dp_last_ts) ? 1 : 0));
				sco_active = (wlc->btch->bth_period &&
					(wlc->btch->bth_period < BT_AMPDU_THRESH)) ? 1 : 0;
				/* op_txstreams==1 indicates chip operating in SISO mode.  */
				siso_mode = (wlc->stf->op_txstreams == 1) ? 1 : 0;
				hybrid_mode = (btc_mode == WL_BTC_HYBRID) ? 1 : 0;
				explicit_tdm_mode = IS_BTCX_FULLTDM(btc_mode);
				implicit_tdm_mode = wlc_btcx_is_hybrid_in_TDM(wlc);
				implicit_hybrid_mode = (btc_mode == WL_BTC_HYBRID) &&
					!implicit_tdm_mode;

				/* process all bt related disabling/enabling here */
				if (sco_active || a2dp_debounced || ble_debounced) {
					if (sco_active && (explicit_tdm_mode ||
						implicit_tdm_mode)) {
						/* If configured, dynamically adjust Rx
						* aggregation window sizes
						*/
						if (wlc_bmac_btc_params_get(wlc->hw,
							BTC_FW_MOD_RXAGG_PKT_SZ_FOR_SCO)) {
							agg_state_req = BTC_LIMAGG;
						} else {
						/* Turn off Agg for TX & RX for SCO/LEA COEX */
							agg_state_req = BTC_AGGOFF;
						}
					} else if (sco_active && implicit_hybrid_mode) {
						agg_state_req = BTC_DYAGG;

					} else if (((ble_debounced || a2dp_debounced) &&
						((explicit_tdm_mode && siso_mode) ||
						(!siso_mode && explicit_tdm_mode &&
						(wlc_btcx_rssi_for_ble_agg_decision(wlc) < 0)))) ||
						((ble_debounced || a2dp_debounced) &&
						implicit_tdm_mode)) {
						/* There are 3 conditions where we disable AMPDU-RX
						 * except for AWDL:
						 * 1) SISO w/btc_mode 1 and A2DP/BLE are active
						 * 2) MIMO w/btc_mode 1 and A2DP/BLE are active
						 * and WL RSSI is weak
						 * 3) Hybrid when coex mode has switched to TDM
						 */
						if (wlc_bmac_btc_params_get(wlc->hw,
							BTC_FW_MOD_RXAGG_PKT_SZ_FOR_A2DP)) {
							agg_state_req = BTC_LIMAGG;
						} else {
							agg_state_req =
								BTC_CLAMPTXAGG_RXAGGOFFEXCEPTAWDL;
						}
					} else if (((ble_debounced || a2dp_debounced) &&
						!siso_mode) &&
						(hybrid_mode || (explicit_tdm_mode &&
						wlc_btcx_rssi_for_ble_agg_decision(wlc) > 0))) {
						/*
						* There are 2 conditions where we enable aggregation
						* 1) MIMO chip in Hybrid mode and A2DP/BLE are
						* active
						* 2) MIMO chip /w btc_mode 1 and A2DP/BLE are active
						* and WL RSSI is strong
						*/
						agg_state_req = BTC_AGGON;
					} else {
						agg_state_req = wlc->btch->agg_state_req;
					}
				} else {
					agg_state_req = BTC_AGGON;
				}
#if defined(WL11AX) && defined(STA)
				/* Toggle disable of UL OFDMA if BT coex active state changes */
				if ((ble_debounced || a2dp_debounced || sco_active) !=
				    wlc->btch->ulmu_disable) {
					wlc_bsscfg_t *bsscfg;
					int idx;

					wlc->btch->ulmu_disable = !wlc->btch->ulmu_disable;
					/* STA: make sure immediate OMI is triggered if needed */
					FOREACH_AS_STA(wlc, idx, bsscfg) {
						ASSERT(bsscfg != NULL);
						wlc_he_omi_ulmu_disable(wlc->hei, bsscfg,
							wlc->btch->ulmu_disable);
					}
				}
#endif /* WL11AX && STA */
			} else {
				agg_state_req = BTC_AGGON;
			}
		} else {
			/* Dynamic BTC mode requires this */
			agg_state_req = BTC_AGGON;
		}
		/* Apply Aggregation modes found above */
		wlc->btch->agg_state_req = agg_state_req;
	}
}

/* Function: wlc_btcx_ltecx_apply_agg_state
 * Description: This function also applies the AMPDU settings,
 * as determined by wlc_btcx_evaluate_agg_state() & LTECX states
 */
void
wlc_btcx_ltecx_apply_agg_state(wlc_info_t *wlc)
{
	wlc_btc_info_t *btc = wlc->btch;
	uint16 btc_reagg_en;
	bool btcx_ampdu_dur = FALSE;
	bool dyagg = FALSE;
	bool rx_agg = TRUE, tx_agg = TRUE;

	if (AMPDU_ENAB(wlc->pub) && !SCAN_IN_PROGRESS(wlc->scan) &&
		wlc->pub->associated) {
		if (BT3P_HW_COEX(wlc) && BAND_2G(wlc->band->bandtype)) {
			switch (btc->agg_state_req) {
				case BTC_AGGOFF:
					tx_agg = FALSE;
					rx_agg = FALSE;
				break;
				case BTC_CLAMPTXAGG_RXAGGOFFEXCEPTAWDL:
					btcx_ampdu_dur = TRUE;
					rx_agg = FALSE;
				break;
				case BTC_DYAGG:
					/* IOVAR can override dynamic settting */
					dyagg = TRUE;
				break;
				case BTC_AGGON:
					/* XXX Can't resume rx aggregation per PR71913,
					 * unless allowed via iovar.
					 */
					btc_reagg_en = (uint16)wlc_btc_params_get(wlc,
						BTC_FW_RX_REAGG_AFTER_SCO);
					if (!btc_reagg_en) {
						rx_agg = FALSE;
					}
				break;
				case BTC_LIMAGG:
					wlc_btcx_rx_ba_window_modify(wlc, &rx_agg);
					tx_agg = TRUE;
					dyagg = TRUE;
				break;
				default:
				break;
			}
			wlc_ampdu_btcx_tx_dur(wlc, btcx_ampdu_dur);
			if (wlc->btch->dyagg != AUTO) {
				dyagg = wlc->btch->dyagg; /* IOVAR override */
			}
			wlc_btc_hflg(wlc, dyagg, BTCX_HFLG_DYAGG);
			wlc_btc_params_set(wlc, BTC_FW_AGG_STATE_REQ, wlc->btch->agg_state_req);

			if ((btc->agg_state_req != BTC_LIMAGG) && (wlc->btch->rxagg_resized)) {
				wlc_ampdu_agg_state_update_rx_all(wlc, OFF);
				wlc_btcx_rx_ba_window_reset(wlc);
				WL_INFORM(("BTCX: Revert Rx agg\n"));
			}
		}
		if ((rx_agg) &&
#ifdef BCMLTECOEX
		   (!wlc_ltecx_rx_agg_off(wlc)) &&
#endif // endif
		   (TRUE)) {
			wlc_ampdu_agg_state_update_rx_all(wlc, ON);
		} else {
			wlc_ampdu_agg_state_update_rx_all(wlc, OFF);
		}
		if ((tx_agg) &&
#ifdef BCMLTECOEX
		   (!wlc_ltecx_tx_agg_off(wlc)) &&
#endif // endif
		   (TRUE)) {
			wlc_ampdu_agg_state_update_tx_all(wlc, ON);
		} else
			wlc_ampdu_agg_state_update_tx_all(wlc, OFF);
	}
}
static bool
wlc_btcx_is_hybrid_in_TDM(wlc_info_t *wlc)
{
	/* This function is created for 4350 chip
	 * where implicit TDM mode is set in Hybrid mode operation.
	 * Returning false for all other chips
	 */
	return FALSE;
}

/*
 * Function helps determine when to disable aggregation for BLE COEX scenario on MIMO chips
 * Returns negative value when RSSI is weaker than low-threshold (i.e. rssi -80, and
 * low_thresh=-70, return -10)
 * Returns positive value when RSSI is stronger than high-threshold (i.e. rssi -50 and
 * high_thresh=-65, return 15)
 * Returns 0 when rssi is inbetween thresholds.	 (i.e. rssi -68, return 0)
 * If SoftAP, then this funtion always returns 0.
 * if thresh = -70, then a new variables are created in this routine called high_thresh, at -65.
 */
static int
wlc_btcx_rssi_for_ble_agg_decision(wlc_info_t *wlc)
{
	int retval;
	int rssi = wlc->primary_bsscfg->link->rssi;
	uint16 btc_rssi_low_thresh, btc_rssi_high_thresh;

	btc_rssi_low_thresh = (uint16)wlc_btc_params_get(wlc,
		BTC_FW_RSSI_THRESH_BLE);
	if (btc_rssi_low_thresh) {
		/* If low is 70, high will be 65, since thesholds stored
		 * as positive numbers
		 */
		btc_rssi_high_thresh = btc_rssi_low_thresh - 5;
		retval = rssi + btc_rssi_low_thresh;
		if (retval < 0) {
			return retval;
		}
		retval = rssi + btc_rssi_high_thresh;
		if (retval > 0) {
			return retval;
		}
	}
	return 0;
}

/*
This function is called by AMPDU Rx to get modified Rx aggregation size.
*/
uint8 wlc_btcx_get_ba_rx_wsize(wlc_info_t *wlc)
{
	if (BAND_2G(wlc->band->bandtype)) {
		if (wlc->btch->rxagg_resized && wlc->btch->rxagg_size) {
			return wlc->btch->rxagg_size;
		}
	}
	return (0);
}
#endif /* WLAMPDU */

/* Function to update GCI bits for WLAN in 2G and 5G band */
void
wlc_btcx_upd_gci_chanspec_indications(wlc_info_t *wlc)
{
#ifdef BCMECICOEX
	si_t *sih = wlc->pub->sih;
	int gci_val =  GCI_WL_RESET_2G_UP_INFO | GCI_WL_RESET_5G_UP_INFO;

	if (wlc->pub->up) {
		if (CHSPEC_IS2G(wlc->hw->chanspec)) {
			gci_val |=  GCI_WL_2G_UP_INFO;
		} else if (CHSPEC_IS5G(wlc->hw->chanspec)) {
			gci_val |=  GCI_WL_5G_UP_INFO;
		}
	}

	/* Update 5G & 2G chanspec GCI bits (bit 34 and 35 respectively) */
	si_gci_direct(sih, GCI_OFFSETOF(sih, gci_control_1),
		GCI_WL_2G_UP_INFO | GCI_WL_5G_UP_INFO,
		GCI_WL_2G_UP_INFO | GCI_WL_5G_UP_INFO);

	si_gci_direct(sih, GCI_OFFSETOF(sih, gci_output[1]),
		GCI_WL_2G_UP_INFO | GCI_WL_5G_UP_INFO,
		gci_val);
#endif /* BCMECICOEX */
}

/* Function to indicate WLAN active in 5G channel.
 * Whether associated in 5G or 5G part of AWDL sequence.
 */
void
wlc_btcx_active_in_5g(wlc_info_t *wlc)
{
#ifdef BCMECICOEX
	si_t *sih = wlc->pub->sih;
	bool set_gci_ind = FALSE;

	/* Indicate 5G active to BT by writing GCI (bit 57) as 1 */
	si_gci_direct(sih, GCI_OFFSETOF(sih, gci_control_1),
		GCI_WL_5G_ACTIVE_INFO, GCI_WL_5G_ACTIVE_INFO);

	if (set_gci_ind == FALSE) {
		int i;
		wlc_bsscfg_t *cfg;
		FOR_ALL_UP_BSS(wlc, i, cfg) {
			if (CHSPEC_IS5G(cfg->current_bss->chanspec)) {
				if ((BSSCFG_AP(cfg) && wlc_bss_assocscb_getcnt(cfg->wlc, cfg)) ||
					BSSCFG_INFRA_STA(cfg)) {
					set_gci_ind = TRUE;
					break;
				}
			}
		}
	}

	if (set_gci_ind) {
		si_gci_direct(sih, GCI_OFFSETOF(sih, gci_output[1]),
			GCI_WL_5G_ACTIVE_INFO, GCI_WL_5G_ACTIVE_INFO);
	} else {
		si_gci_direct(sih, GCI_OFFSETOF(sih, gci_output[1]),
			GCI_WL_5G_ACTIVE_INFO, GCI_WL_RESET_5G_ACTIVE_INFO);
	}
#endif /* BCMECICOEX */
}

bool
wlc_btc_mode_not_parallel(int btc_mode)
{
	return (btc_mode && (btc_mode != WL_BTC_PARALLEL));
}

#ifdef WLAMPDU
static void
wlc_btcx_rx_ba_window_modify(wlc_info_t *wlc, bool *rx_agg)
{
	uint8 window_sz;
	if (!wlc->btch->rxagg_resized) {

		/* For periodic tasks */
		if (wlc->btch->bth_period) {
			if (wlc->btch->bth_period >= BT_AMPDU_RESIZE_THRESH) {
				window_sz = (uint8) wlc_bmac_btc_params_get(wlc->hw,
						BTC_FW_AGG_SIZE_HIGH);
			} else {
				window_sz = (uint8) wlc_bmac_btc_params_get(wlc->hw,
						BTC_FW_AGG_SIZE_LOW);
			}
		} else { /* For Non-periodic tasks */
			window_sz = (uint8) wlc_bmac_btc_params_get(wlc->hw,
					BTC_FW_AGG_SIZE_HIGH);
		}
		if (!window_sz) {
			*rx_agg = FALSE;
		}
		else {
			/* To modify Rx agg size, turn off agg
			 * which will result in the other side
			 * sending BA Request. The new size will
			 * be indicated at that time
			 */
			wlc_ampdu_agg_state_update_rx_all(wlc, OFF);

			/* New size will be set when BA request
			 * is received
			 */
			wlc->btch->rxagg_resized = TRUE;
			wlc->btch->rxagg_size = window_sz;
			*rx_agg = TRUE;
			WL_INFORM(("BTCX: Resize Rx agg: %d\n",
				window_sz));
		}
	}
}

static void
wlc_btcx_rx_ba_window_reset(wlc_info_t *wlc)
{
	wlc->btch->rxagg_resized = FALSE;
	wlc->btch->rxagg_size = 0;
}
#endif /* WLAMPDU */

static void
wlc_btcx_watchdog(void *arg)
{
	wlc_btc_info_t *btc = (wlc_btc_info_t *)arg;
	wlc_info_t *wlc = (wlc_info_t *)btc->wlc;
	int btc_mode = wlc_btc_mode_get(wlc);
#ifdef WL_UCM
	wlc_bsscfg_t *ucm_bsscfg = NULL;
#endif // endif

	wlc_btc_update_btrssi(btc);
#ifdef WL_UCM
	if (UCM_ENAB(wlc->pub)) {
		wlc_btc_get_matched_chanspec_bsscfg(wlc, phy_utils_get_chanspec(wlc->pi),
			&ucm_bsscfg);
		if (wlc_btc_apply_profile(wlc, ucm_bsscfg) == BCME_ERROR) {
			WL_ERROR(("wl%d: %s: BTCOEX: Apply UCM profile returned error\n",
				wlc->pub->unit, __FUNCTION__));
			ASSERT(0);
		}
	} else
#endif /* WL_UCM */
	{
		/* update critical BT state, only for 2G band */
		if (btc_mode && BAND_2G(wlc->band->bandtype)) {
#if defined(STA) && defined(BTCX_PM0_IDLE_WAR)
			wlc_btc_pm_adjust(wlc, wlc->btch->bth_active);
#endif /* STA */

#ifdef WL_BTCDYN
			if (BTCDYN_ENAB(wlc->pub) && IS_DYNCTL_ON(btc->btcdyn->dprof)) {
				/* new dynamic btcoex algo */
				wlc_btcx_dynctl(btc);
			} else
#endif // endif
			{
				/* enable protection in ucode */
				wlc_btc_set_ps_protection(wlc, wlc->primary_bsscfg); /* enable */
			}
		}
	}

	wlc_btc_update_sco_params(wlc);

#ifdef WLAMPDU
	/* Adjust WL AMPDU settings based on BT/LTE COEX Profile */
	wlc_btcx_evaluate_agg_state(btc);
	wlc_btcx_ltecx_apply_agg_state(wlc);
#endif /* WLAMPDU */

#ifdef WL_UCM
	if (!UCM_ENAB(wlc->pub))
#endif /* WL_UCM */
	{
#ifdef WL_BTCDYN
		if (BTCDYN_ENAB(wlc->pub) && (!IS_DYNCTL_ON(btc->btcdyn->dprof)))
#endif // endif
			/* legacy desense coex  */
		{
			/* Dynamic restaging of rxgain for BTCoex */
			wlc_btcx_desense(btc, wlc->band->bandtype);
		}
	}

	if (wlc->clk && (wlc->pub->sih->boardvendor == VENDOR_APPLE) &&
	    (CHIPID(wlc->pub->sih->chip) == BCM4360_CHIP_ID)) {
		wlc_write_shm(wlc, M_COREMASK_BTRESP(wlc), (uint16)btc->siso_ack);
	}
	/* Enable long duration BLE Scan only if WLAN critical activity
	* is not ongoing. SCAN, Association, M4 handshake, P2P negotiation
	* roaming scan cases are covered.
	*/
	if (!(AS_IN_PROGRESS(wlc) || SCAN_IN_PROGRESS(wlc->scan)))
		wlc_btc_ble_grants_re_enable(wlc);
}

/* Check for association state of all the SCBs belonging to the BSS
* and enable the grants only if none of the SCB's are in ASSOCIATED
* but not AUTHORIZED state
*/
void
wlc_btc_ble_grants_re_enable(wlc_info_t *wlc)
{
	wlc_bsscfg_t *bsscfg;
	bool wait_for_authorized = FALSE;
	struct scb *scb;
	struct scb_iter scbiter;
	int idx;
	/* Loop through all the associated SCB's */
	FOREACH_AS_BSS(wlc, idx, bsscfg) {
		FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
			if (SCB_ASSOCIATED(scb) && (bsscfg->wsec)&& !SCB_AUTHORIZED(scb)) {
				wait_for_authorized = TRUE;
				break;
			}
		}
	}
	if (wait_for_authorized == FALSE) {
		wlc_btc_ble_grants_enable(wlc->btch, TRUE);
	}
	/* check if VSDB is active */
	if (MCHAN_ACTIVE(wlc->pub)) {
		/* set flag to deny BLE grants */
		wlc_btc_hflg(wlc, TRUE, 1<<C_BTCX_HFLG_BLE_GRANT_NBIT);
	} else {
		/* VSDB is not active, reset the flag to grant BLE scans */
		wlc_btc_hflg(wlc, FALSE, 1<<C_BTCX_HFLG_BLE_GRANT_NBIT);
	}
}

/* handle BTC related iovars */

static int
wlc_btc_doiovar(void *ctx, uint32 actionid,
        void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif)
{
	wlc_btc_info_t *btc = (wlc_btc_info_t *)ctx;
	wlc_info_t *wlc = (wlc_info_t *)btc->wlc;
	int32 int_val = 0;
	int32 int_val2 = 0;
	int32 *ret_int_ptr;
	bool bool_val;
	int err = 0;

	BCM_REFERENCE(len);
	BCM_REFERENCE(val_size);
	BCM_REFERENCE(wlcif);

	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	if (p_len >= (int)sizeof(int_val) * 2)
		bcopy((void*)((uintptr)params + sizeof(int_val)), &int_val2, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	bool_val = (int_val != 0) ? TRUE : FALSE;

	switch (actionid) {

	case IOV_SVAL(IOV_BTC_FLAGS):
		err = wlc_btc_flags_idx_set(wlc, int_val, int_val2);
		break;

	case IOV_GVAL(IOV_BTC_FLAGS): {
		*ret_int_ptr = wlc_btc_flags_idx_get(wlc, int_val);
		break;
		}

	case IOV_SVAL(IOV_BTC_PARAMS):
		err = wlc_btc_params_set(wlc, int_val, int_val2);
		break;

	case IOV_GVAL(IOV_BTC_PARAMS):
		*ret_int_ptr = wlc_btc_params_get(wlc, int_val);
		break;

	case IOV_SVAL(IOV_BTC_MODE):
		err = wlc_btc_mode_set(wlc, int_val);
		break;

	case IOV_GVAL(IOV_BTC_MODE):
		*ret_int_ptr = wlc_btc_mode_get(wlc);
		break;

	case IOV_SVAL(IOV_BTC_WIRE):
		err = wlc_btc_wire_set(wlc, int_val);
		break;

	case IOV_GVAL(IOV_BTC_WIRE):
		*ret_int_ptr = wlc_btc_wire_get(wlc);
		break;

	case IOV_SVAL(IOV_BTC_STUCK_WAR):
		wlc_btc_stuck_war50943(wlc, bool_val);
		break;

	case IOV_GVAL(IOV_BTC_SISO_ACK):
		*ret_int_ptr = wlc_btc_siso_ack_get(wlc);
		break;

	case IOV_SVAL(IOV_BTC_SISO_ACK):
		wlc_btc_siso_ack_set(wlc, (int16)int_val, TRUE);
		break;

	case IOV_GVAL(IOV_BTC_RXGAIN_THRESH):
		*ret_int_ptr = ((uint32)btc->restage_rxgain_on_rssi_thresh |
			((uint32)btc->restage_rxgain_off_rssi_thresh << 8));
		break;

	case IOV_SVAL(IOV_BTC_RXGAIN_THRESH):
		if (int_val == 0) {
			err = wlc_iovar_setint(wlc, "phy_btc_restage_rxgain", 0);
			if (err == BCME_OK) {
				btc->restage_rxgain_on_rssi_thresh = 0;
				btc->restage_rxgain_off_rssi_thresh = 0;
				btc->restage_rxgain_active = 0;
				WL_BTCPROF(("wl%d: BTC restage rxgain disabled\n", wlc->pub->unit));
			} else {
				err = BCME_NOTREADY;
			}
		} else {
			btc->restage_rxgain_on_rssi_thresh = (uint8)(int_val & 0xFF);
			btc->restage_rxgain_off_rssi_thresh = (uint8)((int_val >> 8) & 0xFF);
			WL_BTCPROF(("wl%d: BTC restage rxgain enabled\n", wlc->pub->unit));
		}
		WL_BTCPROF(("wl%d: BTC restage rxgain thresh ON: -%d, OFF -%d\n",
			wlc->pub->unit,
			btc->restage_rxgain_on_rssi_thresh,
			btc->restage_rxgain_off_rssi_thresh));
		break;

	case IOV_GVAL(IOV_BTC_RXGAIN_FORCE):
		*ret_int_ptr = btc->restage_rxgain_force;
		break;

	case IOV_SVAL(IOV_BTC_RXGAIN_FORCE):
		btc->restage_rxgain_force = int_val;
		break;

	case IOV_GVAL(IOV_BTC_RXGAIN_LEVEL):
		*ret_int_ptr = btc->restage_rxgain_level;
		break;

	case IOV_SVAL(IOV_BTC_RXGAIN_LEVEL):
		btc->restage_rxgain_level = int_val;
		if (btc->restage_rxgain_active) {
			if ((err = wlc_iovar_setint(wlc, "phy_btc_restage_rxgain",
				btc->restage_rxgain_level)) != BCME_OK) {
				/* Need to apply new level on next update */
				btc->restage_rxgain_active = 0;
				err = BCME_NOTREADY;
			}
			WL_BTCPROF(("wl%d: set BTC rxgain level %d (active %d)\n",
				wlc->pub->unit,
				btc->restage_rxgain_level,
				btc->restage_rxgain_active));
		}
		break;

#ifdef	WL_BTCDYN
	/* get/set profile for bcoex dyn.ctl (desense, mode, etc) */
	case IOV_SVAL(IOV_BTC_DYNCTL):
		if (BTCDYN_ENAB(wlc->pub)) {
			err = wlc_btc_dynctl_profile_set(wlc, params);
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;

	case IOV_GVAL(IOV_BTC_DYNCTL):
		if (BTCDYN_ENAB(wlc->pub)) {
			err = wlc_btc_dynctl_profile_get(wlc, arg);
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;

	case IOV_GVAL(IOV_BTC_DYNCTL_STATUS):
		if (BTCDYN_ENAB(wlc->pub)) {
			err = wlc_btc_dynctl_status_get(wlc, arg);
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;

	case IOV_GVAL(IOV_BTC_DYNCTL_SIM):
		if (BTCDYN_ENAB(wlc->pub)) {
			err = wlc_btc_dynctl_sim_get(wlc, arg);
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;

	case IOV_SVAL(IOV_BTC_DYNCTL_SIM):
		if (BTCDYN_ENAB(wlc->pub)) {
			err = wlc_btc_dynctl_sim_set(wlc, arg);
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;
#endif /* WL_BTCDYN */

#ifdef WLBTCPROF_EXT
	case IOV_SVAL(IOV_BTC_BTRSSI_THRESH):
#ifdef WLBTCPROF
		wlc_btc_profile_set(wlc, int_val, IOV_BTC_BTRSSI_THRESH);
#endif /* WLBTCPROF */
		btc->prot_btrssi_thresh = (uint8)int_val;
		break;

	case IOV_SVAL(IOV_BTC_BTRSSI_HYST):
#ifdef WLBTCPROF
		wlc_btc_profile_set(wlc, int_val, IOV_BTC_BTRSSI_HYST);
#endif /* WLBTCPROF */
		btc->btrssi_hyst = (uint8)int_val;
		break;

	case IOV_SVAL(IOV_BTC_WLRSSI_HYST):
#ifdef WLBTCPROF
		wlc_btc_profile_set(wlc, int_val, IOV_BTC_WLRSSI_HYST);
#endif /* WLBTCPROF */
		btc->wlrssi_hyst = (uint8)int_val;
		break;

	case IOV_SVAL(IOV_BTC_WLRSSI_THRESH):
#ifdef WLBTCPROF
		wlc_btc_profile_set(wlc, int_val, IOV_BTC_WLRSSI_THRESH);
#endif /* WLBTCPROF */
		err = wlc_btc_params_set(wlc, (M_BTCX_PROT_RSSI_THRESH_OFFSET(wlc) >> 1), int_val);
		break;
#endif /* WLBTCPROF_EXT */

	case IOV_GVAL(IOV_BTC_STATUS):
		*ret_int_ptr = wlc_btcx_get_btc_status(wlc);
		break;
	case IOV_GVAL(IOV_BTC_BLE_GRANTS):
		*ret_int_ptr = btc->ble_grants;
		break;
	case IOV_SVAL(IOV_BTC_BLE_GRANTS):
		btc->ble_grants = (bool)int_val;
		break;
#ifdef STA
#ifdef WLBTCPROF
	case IOV_GVAL(IOV_BTC_SELECT_PROFILE):
		err = wlc_btcx_select_profile_get(wlc, arg, len);
		break;

	case IOV_SVAL(IOV_BTC_SELECT_PROFILE):
		err = wlc_btcx_select_profile_set(wlc, arg, len);
		break;
#endif /* WLBTCPROF */
#endif /* STA */

#ifdef WL_UCM
	case IOV_GVAL(IOV_BTC_PROFILE):
		if (UCM_ENAB(wlc->pub))
			err = wlc_btc_prof_get(wlc, int_val, arg, len);
		else
			err = BCME_UNSUPPORTED;
	break;

	case IOV_SVAL(IOV_BTC_PROFILE):
		if (UCM_ENAB(wlc->pub))
			err = wlc_btc_prof_set(wlc, arg, len);
		else
			err = BCME_UNSUPPORTED;
	break;

	case IOV_GVAL(IOV_BTC_PROFILE_ACTIVE):
		if (UCM_ENAB(wlc->pub)) {
			*ret_int_ptr = (uint32)(btc->profile_active_2g)
				| (uint32)(btc->profile_active_5g << 16);
		} else {
			err = BCME_UNSUPPORTED;
		}
	break;

	case IOV_SVAL(IOV_BTC_PROFILE_ACTIVE):
		if (UCM_ENAB(wlc->pub)) {
			err = wlc_btc_prof_active_set(wlc, btc, int_val);
		} else {
			err = BCME_UNSUPPORTED;
		}
	break;
	case IOV_GVAL(IOV_BTC_NSS):
		*ret_int_ptr = wlc_btc_nss_get(btc);
		break;
#endif /* WL_UCM */

	case IOV_GVAL(IOV_BTC_DYAGG):
		*ret_int_ptr = btc->dyagg;
		break;

	case IOV_SVAL(IOV_BTC_DYAGG):
		btc->dyagg = (int8)int_val;
		break;

	case IOV_GVAL(IOV_BTC_BTRSSI_AVG):
		*ret_int_ptr = btc->bt_rssi;
		break;

	case IOV_SVAL(IOV_BTC_BTRSSI_AVG):
		if (int_val == 0) {
			wlc_btc_reset_btrssi(wlc->btch);
		}
		break;
#if defined(WL_UCM)
	case IOV_GVAL(IOV_BTC_2G_CHAIN_DISABLE):
		/* Return actual txchain selected by STF Arbitor */
		*ret_int_ptr = wlc_btc_2g_chain_get(wlc);
		break;
	case IOV_SVAL(IOV_BTC_2G_CHAIN_DISABLE):
		err = wlc_btc_2g_chain_set(wlc, bool_val);
		break;
#endif /* WL_UCM */

	case IOV_SVAL(IOV_BTC_TASK):
		err = wlc_btc_task_set(wlc, int_val);
		break;
	case IOV_GVAL(IOV_BTC_TASK):
		*ret_int_ptr = wlc_btc_task_get(wlc);
		break;

	case IOV_SVAL(IOV_BTC_BT_GPIO):
		if (int_val && int_val <= BOARD_GPIO_SW_MAX_VAL) {
			btc->btc_bt_gpio = (uint8)int_val;
		} else {
			err = BCME_RANGE;
		}
		break;
	case IOV_GVAL(IOV_BTC_BT_GPIO):
		*ret_int_ptr = btc->btc_bt_gpio;
		break;

	case IOV_SVAL(IOV_BTC_STAT_GPIO):
		if (int_val && int_val <= BOARD_GPIO_SW_MAX_VAL) {
			btc->btc_stat_gpio = (uint8)int_val;
		} else {
			err = BCME_RANGE;
		}
		break;
	case IOV_GVAL(IOV_BTC_STAT_GPIO):
		*ret_int_ptr = btc->btc_stat_gpio;
		break;

	case IOV_SVAL(IOV_BTC_WLAN_GPIO):
		if (int_val && int_val <= BOARD_GPIO_SW_MAX_VAL) {
			btc->btc_wlan_gpio = (uint8)int_val;
		} else {
			err = BCME_RANGE;
		}
		break;
	case IOV_GVAL(IOV_BTC_WLAN_GPIO):
		*ret_int_ptr = btc->btc_wlan_gpio;
		break;

	default:
		err = BCME_UNSUPPORTED;
	}
	return err;
}

/* E.g., To set BTCX_HFLG_SKIPLMP, wlc_btc_hflg(wlc, 1, BTCX_HFLG_SKIPLMP) */
void
wlc_btc_hflg(wlc_info_t *wlc, bool set, uint16 val)
{
	uint16 btc_hflg;

	if (!wlc->clk)
		return;

	btc_hflg = wlc_bmac_read_shm(wlc->hw, M_BTCX_HOST_FLAGS(wlc));

	if (set)
		btc_hflg |= val;
	else
		btc_hflg &= ~val;

	wlc_bmac_write_shm(wlc->hw, M_BTCX_HOST_FLAGS(wlc), btc_hflg);
}

uint32
wlc_btcx_get_btc_status(wlc_info_t *wlc)
{
	uint32 status = 0;
	uint16 mhf1 = wlc_mhf_get(wlc, MHF1, WLC_BAND_AUTO);
	uint16 mhf3 = wlc_mhf_get(wlc, MHF3, WLC_BAND_AUTO);
	uint16 mhf5 = wlc_mhf_get(wlc, MHF5, WLC_BAND_AUTO);
	uint16 btc_hf = (uint16)wlc_btc_params_get(wlc, M_BTCX_HOST_FLAGS_OFFSET(wlc)/2);
	wlc_btc_info_t *btch = wlc->btch;

	/* if driver or core are down, makes no sense to update stats */
	if (!wlc->pub->up || !wlc->clk) {
	/* d11core asleep, so return 0, which is appropriate value for core-asleep */
		return FALSE;
	}

	status = (((mhf1 & MHF1_BTCOEXIST) ? 1 << BTCX_STATUS_BTC_ENABLE_SHIFT : 0) |
			((mhf3 & MHF3_BTCX_ACTIVE_PROT) ? 1 << BTCX_STATUS_ACT_PROT_SHIFT : 0) |
			((btch->siso_ack) ? 1 << BTCX_STATUS_SISO_ACK_SHIFT : 0) |
			((mhf3 & MHF3_BTCX_PS_PROTECT) ? 1 << BTCX_STATUS_PS_PROT_SHIFT : 0) |
			((btc_hf & BTCX_HFLG_DYAGG) ? 1 << BTCX_STATUS_DYN_AGG_SHIFT : 0) |
			((btc_hf & BTCX_HFLG_ANT2WL) ? 1 << BTCX_STATUS_ANT_WLAN_SHIFT : 0) |
			((mhf5 & MHF5_BTCX_DEFANT) ? 1 << BTCX_STATUS_DEFANT_SHIFT : 0));

#ifdef WL_UCM
	if (UCM_ENAB(wlc->pub)) {
		status = (status |
			((btch->profile_info->simrx) ? 1 << BTCX_STATUS_SLNA_SHIFT : 0) |
			((btch->profile_info->desense_state == WLC_UCM_DESENSE_ON) ?
				1 << BTCX_STATUS_DESENSE_STATE : 0) |
			((btch->profile_info->chain_pwr_state == WLC_UCM_STRONG_TX_POWER) ?
				1 << BTCX_STATUS_CHAIN_PWR_STATE : 0));
	}
#endif /* WL_UCM */
	return status;
}

#ifdef WL_UCM
static int
wlc_btc_prof_get(wlc_info_t *wlc, int32 idx, void *resbuf, uint len)
{
	wlc_btcx_profile_t *profile, **profile_array = wlc->btch->profile_array;
	if (idx >= MAX_UCM_PROFILES || idx < 0) {
		return BCME_RANGE;
	}
	profile = profile_array[idx];
	/* check if the IO buffer has provided enough space to send the profile */
	if (profile->length > len) {
		return BCME_BUFTOOSHORT;
	}
	/* Although wlu does not know in advance, we copy over the profile with right sized array */
	memcpy(resbuf, profile, sizeof(*profile)
		+(profile->chain_attr_count)*sizeof(profile->chain_attr[0]));
	return BCME_OK;
}

static int
wlc_btc_prof_set(wlc_info_t *wlc, void *parambuf, uint len)
{
	uint8 idx, prof_attr;
	uint16 prof_len, prof_ver;
	wlc_btcx_profile_t *profile, **profile_array = wlc->btch->profile_array;
	size_t ucm_prof_sz;

	/* extract the profile index, so we know which one needs to be updated */
	idx = ((wlc_btcx_profile_t *)parambuf)->profile_index;
	if (idx >= MAX_UCM_PROFILES) {
		return BCME_RANGE;
	}
	profile = profile_array[idx];
	ucm_prof_sz = sizeof(*profile)
		+(profile->chain_attr_count)*sizeof(profile->chain_attr[0]);
	/* check if the io buffer has enough data */
	prof_len = ((wlc_btcx_profile_t *)parambuf)->length;
	if (prof_len > len) {
		return BCME_BUFTOOSHORT;
	}
	/* check if the version returned by the utility is correct */
	prof_ver = ((wlc_btcx_profile_t *)parambuf)->version;
	if (prof_ver != UCM_PROFILE_VERSION) {
		return BCME_VERSION;
	}
	/* check if the profile length is consistent */
	if (prof_len != profile->length) {
		return BCME_BADLEN;
	}
	prof_attr = ((wlc_btcx_profile_t *)parambuf)->chain_attr_count;
	/* check if the number of tx chains is consistent */
	if (prof_attr != profile->chain_attr_count) {
		return BCME_BADLEN;
	}

	/* wlc will copy over the var array sized to number of attributes */
	memcpy(profile, parambuf, ucm_prof_sz);

	/* clear the current states when there is a update in the current profile */
	if (idx == wlc->btch->profile_info->curr_profile_idx) {
		wlc_btc_clear_profile_info(wlc->btch);
	}

	/* indicate that this profile or a part of this profile has been initialized */
	profile->init = 1;
	return BCME_OK;
}

/*
 * Allows the Host's COEX Policy manager to determine whether BT/WiFi shared core
 * will be shared by both BT and WiFi, or dedicated to BT.  This is accomplished
 * by sending down the chain mask that will be used by WiFi.
 * Note that the host will be responsible for the reassoc, or disassoc/join commands
 * after receiving "WLC_E_DMA_TXFLUSH_COMPLETE" event from fw and confirming
 * that the chains have been updated.
 * Note that the host should stop posting data packets to the TxQueue before
 * calling this function so that the deletion of a chain won't cause Tx errors.
 * (Where the packet was queued up assuming 3 chains, and then sent out incorrectly
 * over 2 chains).
 * When the band switches from 2.4G to 5G, this request will be cancelled.
 * When the band switches back to 2.4G, it needs to be re-register/reapplied.
 * This function will be called on every band change.
 */
int wlc_btc_2g_chain_request_apply(wlc_info_t *wlc)
{
	int err = BCME_OK;
	wlc_btc_info_t *btc = wlc->btch;
#ifdef WL_STF_ARBITRATOR
	if (WLC_STF_ARB_ENAB(wlc->pub)) {
		if (BAND_2G(wlc->band->bandtype) && btc->chain_2g_shared) {
			/* Apply the request for 2 chains only if currently in 2G band.
			* Note that this function also gets called on band changes, so
			* it will be applied when band changes to 2G.
			*/
			err = wlc_stf_nss_request_update(wlc, btc->stf_req_2g,
				WLC_STF_ARBITRATOR_REQ_STATE_RXTX_ACTIVE,
				btc->chain_2g_req,
				WLC_BITSCNT(btc->chain_2g_req),
				btc->chain_2g_req,
				WLC_BITSCNT(btc->chain_2g_req));
		}
	} else
#endif /* WL_STF_ARBITRATOR */
	{
		if (BAND_2G(wlc->band->bandtype) && btc->chain_2g_shared) {
			wlc_stf_txchain_set(wlc, btc->chain_2g_req,
				FALSE, WLC_TXCHAIN_ID_BTCOEX);
			if ((wlc->stf->txchain_pending == 0) &&
				((wlc->stf->txchain & wlc->stf->rxchain) != wlc->stf->txchain)) {
				err = BCME_UNSUPPORTED;
				return err;
			}
			if (wlc->stf->txchain_pending == 0) {
				wlc_stf_rxchain_set(wlc, btc->chain_2g_req, 0);
				wlc_bss_mac_event(wlc, wlc->primary_bsscfg,
					WLC_E_DMA_TXFLUSH_COMPLETE, NULL, 0, 0, 0, NULL, 0);
			} else {
				wlc->stf->bt_rxchain_pending = TRUE;
			}
		}
	}
	return err;
}
void wlc_btc_rxchain_update(wlc_info_t *wlc)
{
	wlc_btc_info_t *btc = wlc->btch;
	wlc_stf_rxchain_set(wlc, btc->chain_2g_req, 0);
	wlc->stf->bt_rxchain_pending = FALSE;
	wlc_bss_mac_event(wlc, wlc->primary_bsscfg, WLC_E_DMA_TXFLUSH_COMPLETE, NULL, 0, 0,
		0, NULL, 0);
}
static int wlc_btc_2g_chain_set(wlc_info_t *wlc, bool bool_val)
{
	int err = BCME_OK;
	wlc_btc_info_t *btc = wlc->btch;
	btc->chain_2g_shared = bool_val;
	if (btc->chain_2g_shared) {
		err = wlc_btc_2g_chain_request_apply(wlc);
	} else {
		err = wlc_btc_chain_request_cancel(wlc);
	}
	return err;
}
/*
 * When the band switches to 5G, BTC changes bands back to 3x3 as a low
 * priority request.  We don't want to force 3x3
 * operation if another module wants to fewer chains.
 * This function is called on every band change to 5G.
 */
int wlc_btc_chain_request_cancel(wlc_info_t *wlc)
{

	int err = BCME_OK;
#ifdef WL_STF_ARBITRATOR
	wlc_btc_info_t *btc = wlc->btch;
	if (WLC_STF_ARB_ENAB(wlc->pub)) {
		/* Request a re-evaluation of the chains */
		err = wlc_stf_nss_request_update(wlc, btc->stf_req_2g,
			WLC_STF_ARBITRATOR_REQ_STATE_RXTX_ACTIVE,
			wlc->stf->hw_txchain, WLC_BITSCNT(wlc->stf->hw_txchain),
			wlc->stf->hw_rxchain, WLC_BITSCNT(wlc->stf->hw_rxchain));
	} else
#endif /* WL_STF_ARBITRATOR */
	{
		wlc->stf->bt_rxchain_pending = FALSE;
		wlc_stf_rxchain_set(wlc, wlc->stf->hw_rxchain, 0);
		wlc_stf_txchain_set(wlc, wlc->stf->hw_txchain,
			FALSE, WLC_TXCHAIN_ID_BTCOEX);
		if (!wlc->stf->txchain_pending)
			wlc_bss_mac_event(wlc, wlc->primary_bsscfg, WLC_E_DMA_TXFLUSH_COMPLETE,
				NULL, 0, 0, 0, NULL, 0);
	}
	return err;
}
/*
 * This function will return the chainmask in use in 2.4g band
 * regardless of current band
 */
static bool wlc_btc_2g_chain_get(wlc_info_t *wlc)
{
	return wlc->btch->chain_2g_shared;
}
#ifdef WL_STF_ARBITRATOR
/*
 * The STF module that arbitrates chain_mask requests will call this function
 * once the chain request has been evaluated.
 * Open Issue: There is no need to register this callback if we are going to
 * instead use txchain in wlc_btc_2g_chain_get()
 */
void wlc_stf_arb_btc_2g_call_back(void *ctx, uint8 txc_active, uint8 rxc_active)
{
	wlc_btc_info_t *btc = (wlc_btc_info_t*)ctx;
	if (txc_active != rxc_active) {
		WL_ERROR(("%s BTC_2G Requested chainmask of 0x%x, but Arbitor set "
			"txchain:0x%x, rxchain:0x%x\n", __FUNCTION__,
			btc->chain_2g_req, txc_active, rxc_active));
	} else {
		btc->chain_2g_ack = txc_active;
	}
}
#endif /* WL_STF_ARBITRATOR */

static int
wlc_btc_prof_active_set(wlc_info_t *wlc, wlc_btc_info_t *btc, int int_val)
{
	uint16 profile_idx_2g, profile_idx_5g;
	wlc_btcx_profile_t **profile_array = wlc->btch->profile_array;

	/* If profile_active is set to -1, UCM profile will not be applied */
	if (int_val == WLC_UCM_INVALID) {
		wlc_btc_clear_profile_info(btc);
		btc->profile_active_2g = WLC_UCM_INVALID;
		btc->profile_active_5g = WLC_UCM_INVALID;
	} else {
		if ((int_val & 0xFFFF) >= MAX_UCM_PROFILES ||
				((int_val >> 16) & 0xFFFF) >= MAX_UCM_PROFILES) {
			return BCME_RANGE;
		} else {
			profile_idx_2g = int_val & 0xFFFF;
			profile_idx_5g = (int_val >> 16) & 0xFFFF;
		}

		if ((profile_array[profile_idx_2g]->init != 1) ||
				(profile_array[profile_idx_5g]->init != 1)) {
			return BCME_NOTREADY;
		} else {
			btc->profile_active_2g = profile_idx_2g;
			btc->profile_active_5g = profile_idx_5g;
		}
	}
	return BCME_OK;
}

/*
 * Returns the number of streams supported by Access Point that the STA is
 * currently associated to (and has valid BSSID).  Returns 0 if unassociated.
 */
static uint8 wlc_btc_nss_get(wlc_btc_info_t *btc)
{
	wlc_info_t *wlc = (wlc_info_t *)btc->wlc;
	wlc_bsscfg_t *cfg = wlc->primary_bsscfg;

	if (cfg && cfg->up && (wlc->stas_connected == 1)) {
		return cfg->nss_sup;
	}

	return 0;
}

#endif /* WL_UCM */

/* This function enables/disables long duration BLE grants */
void
wlc_btc_ble_grants_enable(wlc_btc_info_t *btc, bool on)
{
	wlc_info_t *wlc = btc->wlc;
	uint16 grant_thresh;

	/* Return if d11rev < 40 */
	if (!IS_D11_ACREV(wlc)) {
		return;
	}

	/* read current large duration BLE grant threshold */
	grant_thresh = wlc_bmac_read_shm(wlc->hw, M_BTCX_BLE_SCAN_GRANT_THRESH(wlc));

	/* enable BLE grants only if not disabled using IOVAR */
	if (!btc->ble_grants && on)
		on = FALSE;

	if (on && (grant_thresh == BTC_BLE_GRANTS_DISABLE)) {
		/* Restore threshold and enable long duration BLE grants */
		wlc_bmac_write_shm(wlc->hw, M_BTCX_BLE_SCAN_GRANT_THRESH(wlc),
			btc->ble_hi_thres);
	} else if (!on && (grant_thresh != BTC_BLE_GRANTS_DISABLE)) {
		/* Save long duration BLE grant threshold */
		btc->ble_hi_thres = grant_thresh;
		/* Override threshold and disable long duration BLE grants */
		wlc_bmac_write_shm(wlc->hw, M_BTCX_BLE_SCAN_GRANT_THRESH(wlc),
			BTC_BLE_GRANTS_DISABLE);
	}
}

#ifdef WL_BTCDYN
/* dynamic BTCOEX wl densense & coex mode switching */

void wlc_btcx_chspec_change_notify(wlc_info_t *wlc, chanspec_t chanspec)
{
	BTCDBG(("%s:wl%d: old chspec:0x%x new chspec:0x%x\n",
		__FUNCTION__, wlc->pub->unit,
		old_chanspec, chanspec));
	/* chip specific WAR code */
}

/* extract bf, apply mask check if invalid */
static void btcx_extract_pwr(int16 btpwr, uint8 shft, int8 *pwr8)
{
	int8 tmp;
	*pwr8 = BT_INVALID_TX_PWR;

	if ((tmp = (btpwr >> shft) & SHM_BTC_MASK_TXPWR)) {
		*pwr8 = (tmp * BT_TX_PWR_STEP) + BT_TX_PWR_OFFSET;
	}
}

/*
	checks for BT power of each active task, converts to dBm and
	returns the highest power level if there is > 1 task detected.
*/
static int8 wlc_btcx_get_btpwr(wlc_btc_info_t *btc)
{
	wlc_info_t *wlc = btc->wlc;

	int8 pwr_sco, pwr_a2dp, pwr_sniff, pwr_acl;
	int8 result_pwr = BT_INVALID_TX_PWR;
	uint16 txpwr_shm;
	int8 pwr_tmp;

	/* read btpwr  */
	txpwr_shm = wlc_read_shm(wlc, M_BTCX_BT_TXPWR(wlc));
	btc->btcdyn->bt_pwr_shm = txpwr_shm; /* keep a copy for dbg & status */

	/* clear the shm after read, ucode will refresh with a new value  */
	wlc_write_shm(wlc, M_BTCX_BT_TXPWR(wlc), 0);

	btcx_extract_pwr(txpwr_shm, SHM_BTC_SHFT_TXPWR_SCO, &pwr_sco);
	btcx_extract_pwr(txpwr_shm, SHM_BTC_SHFT_TXPWR_A2DP, &pwr_a2dp);
	btcx_extract_pwr(txpwr_shm, SHM_BTC_SHFT_TXPWR_SNIFF, &pwr_sniff);
	btcx_extract_pwr(txpwr_shm, SHM_BTC_SHFT_TXPWR_ACL, &pwr_acl);

	/*
	  although rare, both a2dp & sco may be active,
	  pick the highest one. if both are invalid, check sniff
	*/
	if (pwr_sco != BT_INVALID_TX_PWR ||
		pwr_a2dp != BT_INVALID_TX_PWR) {

		BTCDBG(("%s: shmem_val:%x, BT tasks pwr: SCO:%d, A2DP:%d, SNIFF:%d\n",
			__FUNCTION__, txpwr_shm, pwr_sco, pwr_a2dp, pwr_sniff));

		result_pwr = pwr_sco;
		if (pwr_a2dp > pwr_sco)
			result_pwr = pwr_a2dp;

	} else if (pwr_acl != BT_INVALID_TX_PWR) {
		result_pwr = pwr_acl;
	} else if (pwr_sniff != BT_INVALID_TX_PWR) {
		result_pwr = pwr_sniff;
	}

#ifdef DBG_BTPWR_HOLES
	btcdyn_detect_btpwrhole(btc, result_pwr);
#endif // endif

	/* protect from single invalid pwr reading ("pwr hole") */
	if (result_pwr == BT_INVALID_TX_PWR) {
		BTCDBG(("cur btpwr invalid, use prev value:%d\n",
			btc->prev_btpwr));
		pwr_tmp = btc->btcdyn->prev_btpwr;
	} else {
		pwr_tmp = result_pwr;
	}

	btc->btcdyn->prev_btpwr = result_pwr;
	result_pwr = pwr_tmp;
	return result_pwr;
}

#if !defined(WL_BTCDYN_DISABLED)
/*
	At a given BT TX PWR level (typically > 7dbm)
	there is a certain WL RSSI level range at which WL performance in Hybrid
	COEX mode is actually lower than in Full TDM.
	The algorithm below selects the right mode based on tabulated data points.
	The profile table is specific for each board and needs to be calibrated
	for every new board + BT+WIFI antenna design.
*/
static uint8 btcx_dflt_mode_switch(wlc_info_t *wlc, int8 wl_rssi, int8 bt_pwr, int8 bt_rssi)
{
	wlc_btc_info_t *btc = wlc->btch;
	dctl_prof_t *profile = btc->btcdyn->dprof;
	uint8 row, new_mode;

	new_mode = profile->default_btc_mode;

#ifdef BCMLTECOEX
	/* No mode switch is required if ltecx is ON and Not in desense mode */
	if (BCMLTECOEX_ENAB(wlc->pub) && wlc_ltecx_get_lte_status(wlc->ltecx) &&
		!wlc->ltecx->mws_elna_bypass) {
		return WL_BTC_FULLTDM;
	}
#endif // endif

	/* no active BT task when bt_pwr is invalid */
	if	(bt_pwr == BT_INVALID_TX_PWR) {
		return new_mode;
	}

	/* keep current coex mode  if: */
	if ((btc->bt_rssi == BTC_BTRSSI_INVALID) ||
		(btc->btcdyn->wl_rssi == WLC_RSSI_INVALID)) {
		return btc->mode;
	}

	for (row = 0; row < profile->msw_rows; row++) {
		if ((bt_pwr >= profile->msw_data[row].bt_pwr) &&
			(bt_rssi < profile->msw_data[row].bt_rssi +
			btc->btcdyn->msw_btrssi_hyster) &&
			(wl_rssi > profile->msw_data[row].wl_rssi_low) &&
			(wl_rssi < profile->msw_data[row].wl_rssi_high)) {
			/* new1: fallback mode is now per {btpwr + bt_rssi + wl_rssi range} */
				new_mode = profile->msw_data[row].mode;
			break;
		}
	}

	if (new_mode != profile->default_btc_mode) {
		/* the new mode is a downgrade from the default one,
		 for wl & bt signal conditions have deteriorated.
		 Apply hysteresis to stay in this mode until
		 the conditions get better by >= hyster values
		*/
		/*  positive for bt rssi  */
		btc->btcdyn->msw_btrssi_hyster = profile->msw_btrssi_hyster;
	} else {
		/* in or has switched to default, turn off h offsets */
		btc->btcdyn->msw_btrssi_hyster = 0;
	}

	return new_mode;
}

/*
* calculates new desense level using
* current btcmode, bt_pwr, wl_rssi,* and board profile data points
*/
static uint8 btcx_dflt_get_desense_level(wlc_info_t *wlc, int8 wl_rssi, int8 bt_pwr, int8 bt_rssi)
{
	wlc_btc_info_t *btc = wlc->btch;
	dctl_prof_t *profile = btc->btcdyn->dprof;
	uint8 row, new_level;

	new_level = profile->dflt_dsns_level;

	/* BT "No tasks" -> use default desense */
	if	(bt_pwr == BT_INVALID_TX_PWR) {
		return new_level;
	}

	/*  keep current desense level if: */
	if ((btc->bt_rssi == BTC_BTRSSI_INVALID) ||
		(btc->btcdyn->wl_rssi == WLC_RSSI_INVALID)) {
		return btc->btcdyn->cur_dsns;
	}

	for (row = 0; row < profile->dsns_rows; row++) {
		if (btc->mode == profile->dsns_data[row].mode &&
			bt_pwr >= profile->dsns_data[row].bt_pwr) {

			if (wl_rssi > profile->dsns_data[row].wl_rssi_high) {
				new_level = profile->high_dsns_level;
			}
			else if (wl_rssi > profile->dsns_data[row].wl_rssi_low) {

				new_level = profile->mid_dsns_level;
			} else {
				new_level = profile->low_dsns_level;
			}
			break;
		}
	}
	return  new_level;
}
#endif /* !WL_BTCDYN_DISABLED */

/* set external desense handler */
int btcx_set_ext_desense_calc(wlc_info_t *wlc, btcx_dynctl_calc_t fn)
{
	wlc_btcdyn_info_t *btcdyn = wlc->btch->btcdyn;

	ASSERT(fn);
	btcdyn->desense_fn = fn;
	return BCME_OK;
}

/* set external mode switch handler */
int btcx_set_ext_mswitch_calc(wlc_info_t *wlc, btcx_dynctl_calc_t fn)
{
	wlc_btcdyn_info_t *btcdyn = wlc->btch->btcdyn;

	ASSERT(fn);
	btcdyn->mswitch_fn = fn;
	return BCME_OK;
}

/*  wrapper for btcx code portability */
static int8 btcx_get_wl_rssi(wlc_btc_info_t *btc)
{
	int idx;
	wlc_bsscfg_t *cfg;
	int rssi = WLC_RSSI_INVALID;

	FOREACH_AS_BSS(btc->wlc, idx, cfg) {
		if (BSSCFG_STA(cfg) && CHSPEC_IS2G(cfg->current_bss->chanspec)) {
			if ((rssi == WLC_RSSI_INVALID) || (rssi > cfg->link->rssi)) {
				rssi = cfg->link->rssi;
			}
		}
	}
	return rssi;
}

/*  Dynamic Tx power control */
static int
wlc_btcx_dyn_txpwr_ctrl(wlc_info_t *wlc)
{
	wlc_btc_info_t *btc = wlc->btch;
	int cur_txpwr, new_txpwr;
	uint8 idx;
	int err = 0;

	/*  keep current tx power if: */
	if (btc->btcdyn->wl_rssi == WLC_RSSI_INVALID ||
		btc->btcdyn->wlpwr_steps == 0) {
		return BCME_BADOPTION;
	}

	/* get current tx power */
	(void)wlc_iovar_getint(wlc, "qtxpower", &cur_txpwr);

	/* restore default pwr if BT is not active or not in hybrid or parallel coex */
	if (!btc->bth_active ||
		!(btc->mode == WL_BTC_HYBRID || btc->mode == WL_BTC_PARALLEL)) {
		if (cur_txpwr != DYN_PWR_DFLT_QDBM) {
			return wlc_iovar_setint(wlc, "qtxpower", DYN_PWR_DFLT_QDBM);
		}
		return BCME_OK;
	}

	/* find new target pwr based on RSSI */
	for (idx = 0; idx < btc->btcdyn->wlpwr_steps - 1; idx++) {
		if (btc->btcdyn->wl_rssi > btc->btcdyn->wlpwr_thresh[idx])
		break;
	}

	/* idx value can be out-of-bound for wlpwr_thresh and should not be referenced here */

	new_txpwr = btc->btcdyn->wlpwr_val[idx] * 4; /* convert to quarter dBm */

	if (btc->mode == WL_BTC_PARALLEL) {
		/* apply new Tx power for all packets */
		if (new_txpwr != cur_txpwr) {
			if ((err = wlc_iovar_setint(wlc, "qtxpower", new_txpwr))
				!= BCME_OK) {
				return err;
			}
		}
	} else if (btc->mode == WL_BTC_HYBRID) {
		if (btc->siso_ack & BTC_SISOACK_CORES_MASK) {
			/* TO DO : Update SISO response power */
			/* wlc_phy_btc_update_siso_resp_offset(wlc->hw->band->pi, new_txpwr/2); */
		} else {
			/* siso ack not used; set pwr for all pkts */
			if (new_txpwr != cur_txpwr) {
				if ((err = wlc_iovar_setint(wlc, "qtxpower", new_txpwr))
					!= BCME_OK) {
					return err;
				}
			}
		}
	}
	return BCME_OK;
}

/*
	dynamic COEX CTL (desense & switching) called from btc_wtacdog()
*/
static void wlc_btcx_dynctl(wlc_btc_info_t *btc)
{
	wlc_info_t *wlc = btc->wlc;
	uint8 btc_mode = wlc->btch->mode;
	dctl_prof_t *ctl_prof = btc->btcdyn->dprof;
	wlc_btcdyn_info_t *btcdyn = btc->btcdyn;
	uint16 btcx_blk_ptr = wlc->hw->btc->bt_shm_addr;
	uint32 cur_ts = OSL_SYSUPTIME();

	/* protection against too frequent calls from btc watchdog context */
	if ((cur_ts - btcdyn->prev_btpwr_ts) < DYNCTL_MIN_PERIOD) {
		btcdyn->prev_btpwr_ts = cur_ts;
		return;
	}
	btcdyn->prev_btpwr_ts = cur_ts;

	btcdyn->bt_pwr = wlc_btcx_get_btpwr(btc);
	btcdyn->wl_rssi = btcx_get_wl_rssi(btc);

	if (btcdyn->dynctl_sim_on) {
	/* simulation mode is on  */
		btcdyn->bt_pwr = btcdyn->sim_btpwr;
		btcdyn->wl_rssi = btcdyn->sim_wlrssi;
		btc->bt_rssi = btcdyn->sim_btrssi;
	}

#ifdef BCMLTECOEX
	/* No mode switch is required if ltecx is ON */
	if (IS_MSWITCH_ON(ctl_prof) && !wlc_ltecx_get_lte_status(wlc->ltecx)) {
	/* 1st check if we need to switch the btc_mode */
#else
	if (IS_MSWITCH_ON(ctl_prof)) {
#endif /* BCMLTECOEX */
		uint8 new_mode;

		ASSERT(btcdyn->mswitch_fn);
		new_mode = btcdyn->mswitch_fn(wlc, btcdyn->wl_rssi,
			btcdyn->bt_pwr, btc->bt_rssi);

		if (new_mode != btc_mode) {
			wlc_btc_mode_set(wlc, new_mode);

			BTCDBG(("%s mswitch mode:%d -> mode:%d,"
				" bt_pwr:%d, wl_rssi:%d, cur_dsns:%d, BT hstr[rssi:%d]\n",
				__FUNCTION__, btc_mode, new_mode, btcdyn->bt_pwr,
				btcdyn->wl_rssi, btcdyn->cur_dsns,
				ctl_prof->msw_btrssi_hyster));

			if ((wlc->hw->boardflags & BFL_FEM_BT) == 0 && /* dLNA chip */
				btcx_blk_ptr != 0) {
				/* update btcx_host_flags  based on btc_mode */
				if (new_mode == WL_BTC_FULLTDM) {
					wlc_bmac_update_shm(wlc->hw, M_BTCX_HOST_FLAGS(wlc),
						BTCX_HFLG_DLNA_TDM_VAL, BTCX_HFLG_DLNA_MASK);
				} else {
					/* mainly for hybrid and parallel */
					wlc_bmac_update_shm(wlc->hw, M_BTCX_HOST_FLAGS(wlc),
						BTCX_HFLG_DLNA_DFLT_VAL, BTCX_HFLG_DLNA_MASK);
				}
			}
		}
	}

	/* enable protection after mode switching */
	wlc_btc_set_ps_protection(wlc, wlc->primary_bsscfg); /* enable */

	/* disable siso ack in tdm (IOVAR can override) */
	if (wlc->btch->mode == WL_BTC_HYBRID) {
		wlc_btc_siso_ack_set(wlc, AUTO, FALSE);
	} else {
		wlc_btc_siso_ack_set(wlc, 0, FALSE);
	}

	/* check if we need to switch the desense level */
	if (IS_DESENSE_ON(ctl_prof)) {
		uint8 new_level;

		ASSERT(btcdyn->desense_fn);
		new_level = btcdyn->desense_fn(wlc, btcdyn->wl_rssi,
			btcdyn->bt_pwr, btc->bt_rssi);

		if (new_level != btcdyn->cur_dsns) {
			/* apply new desense level */
			if ((wlc_iovar_setint(wlc, "phy_btc_restage_rxgain",
				new_level)) == BCME_OK) {

				BTCDBG(("%s: set new desense:%d, prev was:%d btcmode:%d,"
					" bt_pwr:%d, wl_rssi:%d\n",
					__FUNCTION__, new_level, btcdyn->cur_dsns, btc->mode,
					btcdyn->bt_pwr, btcdyn->wl_rssi));

				btcdyn->cur_dsns = new_level;
			} else
				WL_ERROR(("%s desense apply error\n",
					__FUNCTION__));
		}
	}

	/* dynamic tx power control */
	if (IS_PWRCTRL_ON(ctl_prof)) {
		wlc_btcx_dyn_txpwr_ctrl(wlc);
	}
}

static int wlc_btc_dynctl_profile_set(wlc_info_t *wlc, void *parambuf)
{
	wlc_btcdyn_info_t *btcdyn = wlc->btch->btcdyn;
	bcopy(parambuf, btcdyn->dprof, sizeof(dctl_prof_t));
	return BCME_OK;
}

static int wlc_btc_dynctl_profile_get(wlc_info_t *wlc, void *resbuf)
{
	wlc_btcdyn_info_t *btcdyn = wlc->btch->btcdyn;
	bcopy(btcdyn->dprof, resbuf, sizeof(dctl_prof_t));
	return BCME_OK;
}

/* get dynctl status iovar handler */
static int wlc_btc_dynctl_status_get(wlc_info_t *wlc, void *resbuf)
{
	wlc_btc_info_t *btc = wlc->btch;
	dynctl_status_t dynctl_status;

	/* agg. stats into local stats var */
	dynctl_status.sim_on = btc->btcdyn->dynctl_sim_on;
	dynctl_status.bt_pwr_shm  = btc->btcdyn->bt_pwr_shm;
	dynctl_status.bt_pwr = btc->btcdyn->bt_pwr;
	dynctl_status.bt_rssi = btc->bt_rssi;
	dynctl_status.wl_rssi = btc->btcdyn->wl_rssi;
	dynctl_status.dsns_level = btc->btcdyn->cur_dsns;
	dynctl_status.btc_mode = btc->mode;

	/* return it */
	bcopy(&dynctl_status, resbuf, sizeof(dynctl_status_t));
	return BCME_OK;
}

/*   get dynctl sim parameters */
static int wlc_btc_dynctl_sim_get(wlc_info_t *wlc, void *resbuf)
{
	dynctl_sim_t sim;
	wlc_btcdyn_info_t *btcdyn = wlc->btch->btcdyn;

	sim.sim_on = btcdyn->dynctl_sim_on;
	sim.btpwr = btcdyn->sim_btpwr;
	sim.btrssi = btcdyn->sim_btrssi;
	sim.wlrssi = btcdyn->sim_wlrssi;

	bcopy(&sim, resbuf, sizeof(dynctl_sim_t));
	return BCME_OK;
}

/*   set dynctl sim parameters */
static int wlc_btc_dynctl_sim_set(wlc_info_t *wlc, void *parambuf)
{
	dynctl_sim_t sim;

	wlc_btcdyn_info_t *btcdyn = wlc->btch->btcdyn;
	bcopy(parambuf, &sim, sizeof(dynctl_sim_t));

	btcdyn->dynctl_sim_on = sim.sim_on;
	btcdyn->sim_btpwr = sim.btpwr;
	btcdyn->sim_btrssi = sim.btrssi;
	btcdyn->sim_wlrssi = sim.wlrssi;

	return BCME_OK;
}

#if !defined(WL_BTCDYN_DISABLED)
/*
* initialize one row btc_thr_data_t * from a named nvram var
*/
static int
BCMATTACHFN(wlc_btc_dynctl_init_trow)(wlc_btc_info_t *btc,
	btc_thr_data_t *trow, const char *varname, uint16 xpected_sz)
{
	wlc_info_t *wlc = btc->wlc;
	wlc_hw_info_t *wlc_hw = wlc->hw;
	uint16 j = 0;

	/* read mode switching table */
	int array_sz = getintvararraysize(wlc_hw->vars, varname);
	if (!array_sz)
		return 0; /* var is not present */

	/* mk sure num of items in the var is OK */
	if (array_sz != xpected_sz)
		return -1;

	trow->mode = getintvararray(wlc_hw->vars, varname, j++);
	trow->bt_pwr = getintvararray(wlc_hw->vars, varname, j++);
	trow->bt_rssi = getintvararray(wlc_hw->vars, varname, j++);
	trow->wl_rssi_high = getintvararray(wlc_hw->vars, varname, j++);
	trow->wl_rssi_low = getintvararray(wlc_hw->vars, varname, j++);
	return 1;
}

static int
BCMATTACHFN(wlc_btcdyn_attach)(wlc_btc_info_t *btc)
{
	wlc_info_t *wlc = btc->wlc;
	wlc_hw_info_t *wlc_hw = wlc->hw;
	wlc_btcdyn_info_t *btcdyn;
	dctl_prof_t *dprof;

	if ((btcdyn = (wlc_btcdyn_info_t *)
			MALLOCZ(wlc->osh, sizeof(wlc_btcdyn_info_t))) == NULL) {
			WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			return BCME_NOMEM;
	}

	btc->btcdyn = btcdyn;

	if ((dprof = MALLOCZ(wlc->osh, sizeof(dctl_prof_t))) != NULL) {

		/*  default desnense & mode switching tables (2 rows each) */
		btc_thr_data_t dflt_msw_data[2] = {
			{1, 12, -73, -30, -90},
			{1, 8, -73, -30, -60},
		};
		/*  default desense datatable  */
		btc_thr_data_t dflt_dsns_data[2] = {
			{5, 4, 0, -55, -65},
			{5, -16, 0, -50, -65}
		};

		/* defult tx power control table for hybrid coex */
		int8 dflt_wlpwr_thresh[DYN_PWR_DFLT_STEPS-1] = {-30, -40, -50, -60, -70};
		int8 dflt_wlpwr_val[DYN_PWR_DFLT_STEPS] = {3, 4, 6, 8, 10, 12};
		/* allocate memory for tx power control */
		btc->btcdyn->wlpwr_val = MALLOCZ(wlc->osh, sizeof(int8)*DYN_PWR_MAX_STEPS);
		btc->btcdyn->wlpwr_thresh = MALLOCZ(wlc->osh, sizeof(int8)*(DYN_PWR_MAX_STEPS-1));
		if (btc->btcdyn->wlpwr_val == NULL || btc->btcdyn->wlpwr_thresh == NULL) {
			if (btc->btcdyn->wlpwr_val) {
				MFREE(wlc->osh, btc->btcdyn->wlpwr_val,
					sizeof(btc->btcdyn->wlpwr_val));
				btc->btcdyn->wlpwr_val = NULL;
			}
			if (btc->btcdyn->wlpwr_thresh) {
				MFREE(wlc->osh, btc->btcdyn->wlpwr_thresh,
					sizeof(btc->btcdyn->wlpwr_thresh));
				btc->btcdyn->wlpwr_thresh = NULL;
			}

			MFREE(wlc->osh, dprof, sizeof(dctl_prof_t));

			WL_ERROR(("wl%d: %s: MALLOC for wlpwr failed, %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		   return BCME_NOMEM;
		}

		btcdyn->dprof = dprof;
		/* dynctl profile struct ver */
		dprof->version = DCTL_PROFILE_VER;
		/* default WL desense & mode switch handlers  */
		btcdyn->desense_fn = btcx_dflt_get_desense_level;
		btcdyn->mswitch_fn = btcx_dflt_mode_switch;
		btc->bt_rssi = BTC_BTRSSI_INVALID;
		dprof->msw_btrssi_hyster = BTCDYN_DFLT_BTRSSI_HYSTER;

		/*
		 * try loading btcdyn profile from nvram,
		 * use "btcdyn_flags" var as a presense indication
		 */
		if (getvar(wlc_hw->vars, rstr_btcdyn_flags) != NULL) {

			uint16 i;

			/* read int params 1st */
			dprof->flags =	getintvar(wlc_hw->vars, rstr_btcdyn_flags);
			dprof->dflt_dsns_level =
				getintvar(wlc_hw->vars, rstr_btcdyn_dflt_dsns_level);
			dprof->low_dsns_level =
				getintvar(wlc_hw->vars, rstr_btcdyn_low_dsns_level);
			dprof->mid_dsns_level =
				getintvar(wlc_hw->vars, rstr_btcdyn_mid_dsns_level);
			dprof->high_dsns_level =
				getintvar(wlc_hw->vars, rstr_btcdyn_high_dsns_level);
			dprof->default_btc_mode =
				getintvar(wlc_hw->vars, rstr_btcdyn_default_btc_mode);
			dprof->msw_btrssi_hyster =
				getintvar(wlc_hw->vars, rstr_btcdyn_btrssi_hyster);

			/* params for dynamic tx power control */
			btc->btcdyn->wlpwr_steps =
			getintvararraysize(wlc_hw->vars, rstr_btcdyn_wlpwr_val);
			if (btc->btcdyn->wlpwr_steps > DYN_PWR_MAX_STEPS) {
				btc->btcdyn->wlpwr_steps = 0;
			}
			if (btc->btcdyn->wlpwr_steps > 0 && ((btc->btcdyn->wlpwr_steps - 1) !=
				getintvararraysize(wlc_hw->vars, rstr_btcdyn_wlpwr_thresh))) {
				btc->btcdyn->wlpwr_steps = 0;
			}
			if (btc->btcdyn->wlpwr_steps == 0) {
				/* use default tx power table for hybrid coex */
				btc->btcdyn->wlpwr_steps = DYN_PWR_DFLT_STEPS;
				memcpy(btc->btcdyn->wlpwr_thresh, dflt_wlpwr_thresh,
				    sizeof(dflt_wlpwr_thresh));
				memcpy(btc->btcdyn->wlpwr_val, dflt_wlpwr_val,
				    sizeof(dflt_wlpwr_val));
			} else {
				/* use nvram settings */
				int pwr_step_idx;
				btc->btcdyn->wlpwr_val[0] =
				    getintvararray(wlc_hw->vars, rstr_btcdyn_wlpwr_val, 0);
				for (pwr_step_idx = 1; pwr_step_idx < btc->btcdyn->wlpwr_steps;
					pwr_step_idx++) {
					btc->btcdyn->wlpwr_val[pwr_step_idx] =
						getintvararray(wlc_hw->vars,
						rstr_btcdyn_wlpwr_val, pwr_step_idx);
					btc->btcdyn->wlpwr_thresh[pwr_step_idx-1] =
						getintvararray(wlc_hw->vars,
						rstr_btcdyn_wlpwr_thresh, pwr_step_idx-1);
				}
			}

			/* these two are used for data array sz check */
			dprof->msw_rows =
				getintvar(wlc_hw->vars, rstr_btcdyn_msw_rows);
			dprof->dsns_rows =
				getintvar(wlc_hw->vars, rstr_btcdyn_dsns_rows);

			/* sanity check on btcdyn nvram table sz */
			if ((dprof->msw_rows > DCTL_TROWS_MAX) ||
				(((dprof->flags & DCTL_FLAGS_MSWITCH) == 0) !=
				(dprof->msw_rows == 0))) {
				BTCDBG(("btcdyn invalid mode switch config\n"));
				goto rst2_dflt;
			}
			if ((dprof->dsns_rows > DCTL_TROWS_MAX) ||
				(((dprof->flags & DCTL_FLAGS_DESENSE) == 0) !=
				(dprof->dsns_rows == 0))) {
				BTCDBG(("btcdyn invalid dynamic desense config\n"));
				goto rst2_dflt;
			}

			/*  initialize up to 4 rows in msw table */
			i = wlc_btc_dynctl_init_trow(btc, &dprof->msw_data[0],
				rstr_btcdyn_msw_row0, sizeof(btc_thr_data_t));
			i += wlc_btc_dynctl_init_trow(btc, &dprof->msw_data[1],
				rstr_btcdyn_msw_row1, sizeof(btc_thr_data_t));
			i += wlc_btc_dynctl_init_trow(btc, &dprof->msw_data[2],
				rstr_btcdyn_msw_row2, sizeof(btc_thr_data_t));
			i += wlc_btc_dynctl_init_trow(btc, &dprof->msw_data[3],
				rstr_btcdyn_msw_row3, sizeof(btc_thr_data_t));

			/* number of initialized table rows must match to specified in nvram */
			if (i != dprof->msw_rows) {
				BTCDBG(("btcdyn incorrect nr of mode switch rows (%d)\n", i));
				goto rst2_dflt;
			}

			/*  initialize up to 4 rows in desense sw table */
			i = wlc_btc_dynctl_init_trow(btc, &dprof->dsns_data[0],
				rstr_btcdyn_dsns_row0, sizeof(btc_thr_data_t));
			i += wlc_btc_dynctl_init_trow(btc, &dprof->dsns_data[1],
				rstr_btcdyn_dsns_row1, sizeof(btc_thr_data_t));
			i += wlc_btc_dynctl_init_trow(btc, &dprof->dsns_data[2],
				rstr_btcdyn_dsns_row2, sizeof(btc_thr_data_t));
			i += wlc_btc_dynctl_init_trow(btc, &dprof->dsns_data[3],
				rstr_btcdyn_dsns_row3, sizeof(btc_thr_data_t));

			/* number of initialized table rows must match to specified in nvram */
			if (i != dprof->dsns_rows) {
				BTCDBG(("btcdyn incorrect nr of dynamic desense rows (%d)\n", i));
				goto rst2_dflt;
			}

			BTCDBG(("btcdyn profile has been loaded from nvram - Ok\n"));
		} else {
			rst2_dflt:
			WL_ERROR(("wl%d: %s: nvram.txt: missing or bad btcdyn profile vars."
				" do init from default\n", wlc->pub->unit, __FUNCTION__));

			/* all dynctl features are disabled until changed by iovars */
			dprof->flags = DCTL_FLAGS_DISABLED;

			/* initialize default profile */
			dprof->dflt_dsns_level = DESENSE_OFF;
			dprof->low_dsns_level = DESENSE_OFF;
			dprof->mid_dsns_level = DFLT_DESENSE_MID;
			dprof->high_dsns_level = DFLT_DESENSE_HIGH;
			dprof->default_btc_mode = WL_BTC_HYBRID;
			dprof->msw_rows = DCTL_TROWS;
			dprof->dsns_rows = DCTL_TROWS;
			btcdyn->sim_btpwr = BT_INVALID_TX_PWR;
			btcdyn->sim_wlrssi = WLC_RSSI_INVALID;

			/*  sanity check for the table sizes */
			ASSERT(sizeof(dflt_msw_data) <=
				(DCTL_TROWS_MAX * sizeof(btc_thr_data_t)));
			bcopy(dflt_msw_data, dprof->msw_data, sizeof(dflt_msw_data));
			ASSERT(sizeof(dflt_dsns_data) <=
				(DCTL_TROWS_MAX * sizeof(btc_thr_data_t)));
			bcopy(dflt_dsns_data,
				dprof->dsns_data, sizeof(dflt_dsns_data));

			/* default tx power table for hybrid coex */
			btc->btcdyn->wlpwr_steps = DYN_PWR_DFLT_STEPS;
			memcpy(btc->btcdyn->wlpwr_thresh,
				dflt_wlpwr_thresh, sizeof(dflt_wlpwr_thresh));
			memcpy(btc->btcdyn->wlpwr_val,
				dflt_wlpwr_val, sizeof(dflt_wlpwr_val));

		}
		/* set btc_mode to default value */
		wlc_hw->btc->mode = dprof->default_btc_mode;
	} else {
		WL_ERROR(("wl%d: %s: MALLOC for dprof failed, %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			MFREE(wlc->osh, btcdyn, sizeof(wlc_btcdyn_info_t));
			return BCME_NOMEM;
	}
	return BCME_OK;
}
#endif /* !WL_BTCDYN_DISABLED */

#ifdef DBG_BTPWR_HOLES
static void btcdyn_detect_btpwrhole(wlc_btc_info_t *btc, int8 cur_pwr)
{
	static pwrs_t pwr_smpl[4] = {{-127, 0}, {-127, 0}, {-127, 0}, {-127, 0}};
	static uint8 pwr_idx = 0;
	int32 cur_ts;

	cur_ts =  OSL_SYSUPTIME();
	pwr_smpl[pwr_idx].pwr = cur_pwr;
	pwr_smpl[pwr_idx].ts = cur_ts;

	/* detect a hole (an abnormality) in PWR sampling sequence */
	if ((pwr_smpl[pwr_idx].pwr != BT_INVALID_TX_PWR) &&
		(pwr_smpl[(pwr_idx-1) & 0x3].pwr == BT_INVALID_TX_PWR) &&
		(pwr_smpl[(pwr_idx-2) & 0x3].pwr != BT_INVALID_TX_PWR)) {

		DYNCTL_ERROR(("BTPWR hole at T-1:%d, delta from T-2:%d\n"
			" btpwr:[t, t-1, t-2]:%d,%d,%d\n", pwr_smpl[(pwr_idx-1) & 0x3].ts,
			(pwr_smpl[(pwr_idx-1) & 0x3].ts - pwr_smpl[(pwr_idx-2) & 0x3].ts),
			pwr_smpl[pwr_idx].pwr, pwr_smpl[(pwr_idx-1) & 0x3].pwr,
			pwr_smpl[(pwr_idx-2) & 0x3].pwr));

	}
	pwr_idx = (pwr_idx + 1) & 0x3;
}
#endif /* DBG_BTPWR_HOLES */
#endif /* WL_BTCDYN */

/* Read bt rssi from shm and do moving average */
static void
wlc_btc_update_btrssi(wlc_btc_info_t *btc)
{
	wlc_info_t *wlc = btc->wlc;
	uint16 btrssi_shm;
	int16 btrssi_avg = 0;
	int8 cur_rssi = 0, old_rssi;

	if (!btc->bth_active) {
		wlc_btc_reset_btrssi(btc);
		return;
	}

	/* read btrssi idx from shm */
	btrssi_shm = wlc_bmac_read_shm(wlc->hw, M_BTCX_RSSI(wlc));

	if (btrssi_shm) {
		/* clear shm because ucode keeps max btrssi idx */
		wlc_bmac_write_shm(wlc->hw, M_BTCX_RSSI(wlc), 0);

		/* actual btrssi = -1 x (btrssi x BT_RSSI_STEP + BT_RSSI_OFFSET) */
		cur_rssi = (-1) * (int8)(btrssi_shm * BTC_BTRSSI_STEP + BTC_BTRSSI_OFFSET);

		/* # of samples max out at btrssi_maxsamp */
		if (btc->btrssi_cnt < btc->btrssi_maxsamp)
			btc->btrssi_cnt++;

		/* accumulate & calc moving average */
		old_rssi = btc->btrssi_sample[btc->btrssi_idx];
		btc->btrssi_sample[btc->btrssi_idx] = cur_rssi;
		/* sum = -old one, +new  */
		btc->btrssi_sum = btc->btrssi_sum - old_rssi + cur_rssi;
		ASSERT(btc->btrssi_cnt);
		btrssi_avg = btc->btrssi_sum / btc->btrssi_cnt;

		btc->btrssi_idx = MODINC_POW2(btc->btrssi_idx, btc->btrssi_maxsamp);

		if (btc->btrssi_cnt < btc->btrssi_minsamp) {
			btc->bt_rssi = BTC_BTRSSI_INVALID;
			return;
		}

		btc->bt_rssi = (int8)btrssi_avg;
	}
}

static void
wlc_btc_reset_btrssi(wlc_btc_info_t *btc)
{
	memset(btc->btrssi_sample, 0, sizeof(int8)*BTC_BTRSSI_MAX_SAMP);
	btc->btrssi_cnt = 0;
	btc->btrssi_idx = 0;
	btc->btrssi_sum = 0;
	btc->bt_rssi = BTC_BTRSSI_INVALID;
}
