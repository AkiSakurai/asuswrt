/*
 * Private SCAN info of
 * Broadcom 802.11bang Networking Device Driver
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
 * $Id: wlc_scan_priv.h 787107 2020-05-19 06:09:35Z $
 */

#ifndef _WLC_SCAN_PRIV_H_
#define _WLC_SCAN_PRIV_H_

#include <wlc_scan.h>

/* forward declarations */
typedef struct scan_info scan_info_t;

typedef enum {
	WLC_SCAN_STATE_START = 0,
	WLC_SCAN_STATE_SEND_PROBE = 1,
	WLC_SCAN_STATE_LISTEN = 2,
	WLC_SCAN_STATE_CHN_SCAN_DONE = 3,
	WLC_SCAN_STATE_PARTIAL = 4,
	WLC_SCAN_STATE_COMPLETE = 5,
	WLC_SCAN_STATE_ABORT = 6
}	scan_state_t;

typedef enum scan_6g_chan_state {
	SCAN_6G_CHAN_NONE = 0,
	SCAN_6G_CHAN_IN_RNR_LIST = 1,
	SCAN_6G_CHAN_IN_RNR_20TU_SET = 2,
	SCAN_6G_CHAN_20TU_TO_PRBREQ = 3,
	SCAN_6G_CHAN_IS_PSC_WAIT_PRB_DELAY = 4,
	SCAN_6G_CHAN_PSC_WAIT_PRBDLY_TO_PRBREQ = 5,
	SCAN_6G_CHAN_IS_PSC_WAIT_FILS_DISC = 6,
	SCAN_6G_CHAN_PSC_FILSDISC_TO_PRBREQ = 7,
	SCAN_6G_CHAN_NON_RNR_WAIT_DISC = 8,
	SCAN_6G_CHAN_SEND_PROBE = 9
} scan_6g_chan_state_t;

#define DEFAULT_SCAN_STALL_THRESHOLD 30 /* Default scan stall threshold value */

/* Defines of bitmap type, which are used to identify the bands to be scanned: */
#define SCAN_BAND_2G		(1 << 0)
#define SCAN_BAND_5G		(1 << 1)
#define SCAN_BAND_6G		(1 << 2)

/* definition to support long listen dwell time under VSDB. unit is ms. */
#define SCAN_LONG_LISTEN_MAX_TIME_LIMIT                 5000u
#define SCAN_LONG_LISTEN_DWELL_TIME_THRESHOLD           512u
#define SCAN_LONG_LISTEN_BG_SCAN_HOME_TIME              110u
#define SCAN_LONG_LISTEN_BG_SCAN_PASSIVE_TIME           130u
#define SCAN_MARGIN_FROM_ONESHOT_TO_BG_PASSIVE          10u

/* For 6g two types of probing exist: */
typedef enum {
	SCAN_6G_ACTIVE_PROBING = 0,
	SCAN_6G_PASSIVE_PROBING = 1
} scan_6g_probing_t;

/* For 6g two types of channels lists exist: */
typedef enum {
	SCAN_6G_RNR_CHANNELS		= 1,
	SCAN_6G_PSC_CHANNELS		= 2,
	SCAN_6G_ALL_CHANNELS		= 4
} scan_6g_channels_t;

#define SHORT_SSID_LEN			4U

struct scan_info {
	struct wlc_scan_info	*scan_pub;
	uint		unit;
	wlc_info_t	*wlc;
	osl_t		*osh;
	wlc_scandb_t	*sdb;
	uint		memsize;	/* allocated size of this structure (for freeing) */

	/* scan defaults */
	struct {
		uint16	unassoc_time;	/* dwell time per channel when unassociated */
		uint16	assoc_time;	/* dwell time per channel when associated */
		uint16	passive_time;	/* dwell time per channel for passive scanning */
		int8	nprobes;	/* number of probes per channel */
		int8	passive;	/* scan type: 1 -> passive, 0 -> active */
	} defaults;

	uint8		pwrsave_enable;	/* turn on/off single core scanning */
	uint8		mimo_override;	/* force MIMO scan on this scan */
	/* Counter to track the duration for which scan is in progress: */
	uint32		in_progress_duration;
	/* Threshold Value upto which in_progress_count need to be checked: */
	uint32		scanstall_threshold;
	int		channel_idx;	/* index in chanspec_list of channel being scanned */
	/* scan times are in milliseconds */
	int8		pass;		/* current scan pass or scan state */
	int8		nprobes;	/* number of probes per channel */
	int8		npasses;	/* number of passes per channel */
	uint16		active_time;	/* dwell time per channel for active scanning */
	uint16		passive_time;	/* dwell time per channel for passive scanning */
	uint32		start_tsf;	/* TSF read from chip at start of channel scan */
	struct wl_timer *timer;		/* timer for BSS scan operations */
	scancb_fn_t	cb;				/* function to call when scan is done */
	void		*cb_arg;		/* arg to cb fn */

	int		nssid;		/* number off ssids in the ssid list */
	wlc_ssid_t	*ssid_list;	/* ssids to look for in scan (could be dynamic) */
	wlc_ssid_t	*ssid_prealloc;	/* pointer to preallocated (non-dynamic) store */
	int		nssid_prealloc;	/* number of preallocated entries */

	bool		ssid_wildcard_enabled;
	wlc_bsscfg_t	*bsscfg;
	actcb_fn_t	act_cb;		/* function to call when scan is done */
	void		*act_cb_arg;	/* arg to cb fn */
	uint		wd_count;
	int		status;		/* deferred status */
	wl_pwr_scan_stats_t *scan_stats;	/* power stats for scan */

	uint8		scan_rx_pwrsave;	/* reduce rxchain to save power in scan rx window */
	uint8		scan_tx_pwrsave;	/* reduce txchain to save power in scan tx */
	uint8		scan_ps_txchain;	/* track txchain and restore after scan complete */
	uint8		scan_ps_rxchain;	/* track rxchain and restore after scan complete */

	wlc_bsscfg_t *scanmac_bsscfg;		/* scanmac bsscfg */
	wl_scanmac_config_t scanmac_config;	/* scanmac config */
	bool is_scanmac_config_updated;		/* config updated flag */

	struct ether_addr sa_override;		/* override source MAC */
	wlc_msch_req_handle_t	*msch_req_hdl;	/* hdl to msch request */
	chanspec_t	cur_scan_chanspec;	/* current scan channel spec */
	scan_state_t	state;			/* Channel scheduler state */

	uint32		timeslot_id;
	void		*stf_scan_req;		/* STF Arbitrator request */
	bool		defer_probe;		/* OCE probe request deferral */
	bcm_notif_h	scan_start_h;		/* scan_start notifier handle. */

	uint8		scan_dfs_away_duration; /* On DFS home channel - max away time in ms */
	uint8		scan_dfs_auto_reduce;   /* Allow scan on DFS ch when active time/passive
						 * time is more than threshold by reducing time
						 */
	uint16		scan_dfs_min_dwell;     /* On DFS home ch - min dwell time in sec */
	uint64		prev_scan;		/* timestamp of previous scan */

	uint32		bandmask;		/* bitmask of bands to scan, SCAN_BAND... */
	int		channel_num;		/* length of chanspec_list */
	chanspec_t	chanspec_list[SCAN_MAX_CHANSPECS];	/* list of channels to scan */
	/* keep all these debugging related fields at the end */
#ifdef BCMDBG
	uint8		debug;
	uint8		test;
	struct wl_timer	*test_timer;		/* timer for various tests */
#endif // endif
	bool		prb_idle_on_6g_chan;	/* Probe IDLE time on 6g channel */
	scan_6g_chan_state_t state_6g_chan;
	scan_6g_probing_t probing_6g;		/* Method of probing to use in 6G */
	scan_6g_channels_t channels_6g;		/* Channels to scan in 6G */
};

typedef struct scan_iter_params {
	wlc_bss_list_t *bss_list;	/* list on which cached items will be added */
	int merge;			/* if TRUE, merge cached entries with different timestamp
					 * to existing entries on bss_list
					 */
	uint current_ts;		/* timestamp of most recent cache additions */
} scan_iter_params_t;

#endif /* _WLC_SCAN_PRIV_H_ */
