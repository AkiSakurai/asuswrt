/*
 * SCAN Module Public Interface
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
 * $Id: wlc_scan.h 777088 2019-07-18 17:39:11Z $
 */

#ifndef _WLC_SCAN_H_
#define _WLC_SCAN_H_

#include <wlc_scandb.h>

/* scan state bits */
#define SCAN_STATE_SUPPRESS	(1 << 0)
#define SCAN_STATE_SAVE_PRB	(1 << 1)
#define SCAN_STATE_PASSIVE	(1 << 2)
#define SCAN_STATE_WSUSPEND	(1 << 3)
#define SCAN_STATE_RADAR_CLEAR	(1 << 4)
#define SCAN_STATE_PSPEND	(1 << 5)
#define SCAN_STATE_READY	(1 << 7)
#define SCAN_STATE_INCLUDE_CACHE	(1 << 8)
#define SCAN_STATE_PROHIBIT	(1 << 9)
#define SCAN_STATE_DLY_WSUSPEND	(1 << 10)
#define SCAN_STATE_IN_TMR_CB	(1 << 11)
#define SCAN_STATE_OFFCHAN	(1 << 12)
#define SCAN_STATE_TERMINATE	(1 << 13)
#define SCAN_STATE_HOME_TIME_SPENT	(1 << 15)
#define SCAN_STATE_CHANNEL_CHANGE (1 << 16)
#define SCAN_STATE_OFF_CHAN     (1 << 18)
#define SCAN_STATE_PRB_DEFERRED	(1 << 19)

#define SCAN_FLAG_SWITCH_CHAN					(1 << 0)
#define SCAN_FLAG_PASSIVE_PSCAN_BLOCK			(1 << 1)
#define SCAN_FLAG_PASSIVE_PSCAN_WAKE_PENDING	(1 << 2)
#define SCAN_FLAG_PASSIVE_PSCAN_REQ_ENDED		(1 << 3)
#define SCAN_MSCH_ONCHAN_CALLBACK			(1 << 7)

#define WLC_SCAN_PASSIVE_TIME	110	/* ms to listen on a channel for beacons for passivescan */
#define WLC_SCAN_AWAY_LIMIT	100	/* max time to be away from home channel before return */
#define WLC_SCAN_PS_PREP_TIME	10	/* average time to announce PS mode */
#define WLC_SCAN_PS_OVERHEAD	50	/* overhead time to allow for one long PS announce delay */

#define WLC_SCAN_NSSID_PREALLOC	5	/* Number of preallocated SSID slots */

struct wlc_scan_info {
	void		*scan_priv;		/* pointer to scan private struct */
	struct wlc_scan_cmn_info *wlc_scan_cmn;
	uint32		state;			/* scan state bits */
	bool		in_progress;		/* scan in progress */
	struct ether_addr bssid;
	uint32		flag;			/* scan flag supplement of state bits */
	bool		iscan_cont;		/* true if iscan continuing pass */
	int8		nscantx;		/* WL_EAP_SCAN_TX - number of frames */
	int8		txpwr_max_offset;	/* offset in qdbm to get to max tx power */
	uint64		prev_scan;		/* timestamp of previous scan */
	uint64		last_radar_poll;	/* timestamp of the last radar poll */
};

struct wlc_scan_cmn_info {
	/* Add Scan Function Call Interface */
	uint8		usage;			/* scan engine usage */
	int		bss_type;		/* Scan for Infra, IBSS, or Any */
	bool		is_hotspot_scan;	/* hotspot scan */

	/* WLSCANCACHE */
	bool		_scancache;		/* scan cache enable */

	/* STA */
	uint32		scan_start_time;	/* for scan time accumulation... */
	uint32		scan_stop_time;		/* ...here so callbacks can see it */
	uint8 passive_on_restricted;	/**< controls scan on restricted channels */
};

/* scan start notification callback */
typedef void (*scan_start_fn_t)(void *ctx, wlc_ssid_t *ssid);

int wlc_scan_start_register(wlc_info_t *wlc, scan_start_fn_t fn, void *arg);
int wlc_scan_start_unregister(wlc_info_t *wlc, scan_start_fn_t fn, void *arg);

#define SCAN_IN_PROGRESS(scan_info)	((scan_info) && (scan_info)->in_progress)

#ifdef WLRSDB
#define ANY_SCAN_IN_PROGRESS(scaninfo) wlc_scan_anyscan_in_progress(scaninfo)
extern int wlc_scan_anyscan_in_progress(wlc_scan_info_t *scan);
#else
#define ANY_SCAN_IN_PROGRESS(scaninfo) SCAN_IN_PROGRESS(scaninfo)
#endif // endif

/* scan engine usage */
#define SCAN_ENGINE_USAGE_NORM	0	/* scan, assoc, roam, etc. */
#define SCAN_ENGINE_USAGE_ESCAN	1	/* escan */
#define SCAN_ENGINE_USAGE_RM	3	/* RM */

#define NORM_IN_PROGRESS(scan)		((scan)->wlc_scan_cmn->usage == SCAN_ENGINE_USAGE_NORM)
#define ESCAN_IN_PROGRESS(scan)		((scan)->wlc_scan_cmn->usage == SCAN_ENGINE_USAGE_ESCAN)

#ifdef WLSCANCACHE
	#if defined(ROM_ENAB_RUNTIME_CHECK) || !defined(DONGLEBUILD)
		#define SCANCACHE_ENAB(scan_info)	((scan_info)->wlc_scan_cmn->_scancache)
	#elif defined(WLSCANCACHE_DISABLED)
		#define SCANCACHE_ENAB(scan_info)	(0)
	#else
		#define SCANCACHE_ENAB(scan_info)	((scan_info)->wlc_scan_cmn->_scancache)
	#endif
#else
	#define SCANCACHE_ENAB(scan_info)	(0)
#endif /* WLSCANCACHE */

extern wlc_scan_info_t *wlc_scan_attach(wlc_info_t *wlc, void *wl, osl_t *osh, uint);
extern void wlc_scan_detach(wlc_scan_info_t *scan_ptr);
extern int wlc_scan_down(void *hdl);

extern bool wlc_scan_in_scan_chanspec_list(wlc_scan_info_t *wlc_scan_info, chanspec_t chanspec);

/* scan completion callback */
typedef void (*scancb_fn_t)(void *wlc, int status, wlc_bsscfg_t *cfg);
/* scan action callback */
typedef void (*actcb_fn_t)(wlc_info_t *wlc, void *arg, uint *dwell);

extern int wlc_scan(
	wlc_scan_info_t *scan_ptr,
	int bss_type,
	const struct ether_addr* bssid,
	int nssid,
	wlc_ssid_t *ssid,
	int scan_type,
	int nprobes,
	int active_time,
	int passive_time,
	int home_time,
	const chanspec_t* chanspec_list,
	int channel_num,
	chanspec_t chanspec_start,
	bool save_prb,
	scancb_fn_t fn,
	void* arg,
	int away_limit_override,
	bool extd_scan,
	bool suppress_ssid,
	bool include_cache,
	uint scan_flags,
	wlc_bsscfg_t *cfg,
	uint8 usage,
	actcb_fn_t act_cb,
	void *act_arg,
	struct ether_addr *sa_override
);

#ifdef WL_SCAN_DFS_HOME
extern int wlc_scan_on_dfs_chan(wlc_info_t *wlc, int chanspec_num, chanspec_t chanspec,
	int scan_type, int *active_time, int *passive_time);
#endif /* WL_SCAN_DFS_HOME */
extern void wlc_scan_abort(wlc_scan_info_t *wlc_scan_info, int status);
extern void wlc_scan_abort_ex(wlc_scan_info_t *wlc_scan_info, wlc_bsscfg_t *cfg, int status);
extern void wlc_scan_terminate(wlc_scan_info_t *wlc_scan_info, int status);
extern void wlc_scan_radar_clear(wlc_scan_info_t *wlc_scan_info);
extern void wlc_scan_default_channels(wlc_scan_info_t *wlc_scan_info, chanspec_t chanspec_start,
	int band, chanspec_t *chanspec_list, int *channel_count);

extern bool wlc_scan_inprog(wlc_info_t *wlc_info);
extern void wlc_scan_fifo_suspend_complete(wlc_scan_info_t *wlc_scan_info);
extern void wlc_scan_pm_pending_complete(wlc_scan_info_t *wlc_scan_info);
extern chanspec_t wlc_scan_get_current_chanspec(wlc_scan_info_t *wlc_scan_info);
extern int wlc_scan_ioctl(void *ctx, uint cmd, void *arg, uint len, struct wlc_if *wlcif);
extern bool wlc_scan_ssid_match(wlc_scan_info_t *wlc_scan_info,
	uint8 *ssid_buf, uint8 ssid_len, bool filter);
extern int wlc_scan_chnum(wlc_scan_info_t *wlc_scan_info);

#ifdef WLSCANCACHE
extern void wlc_scan_get_cache(wlc_scan_info_t *scan_info,
                   const struct ether_addr *BSSID, int nssid, const wlc_ssid_t *SSID,
                   int BSS_type, const chanspec_t *chanspec_list, uint chanspec_num,
                   wlc_bss_list_t *bss_list);
extern int32 wlc_scan_add_bss_cache(wlc_scan_info_t *scan_info, wlc_bss_info_t *bi);

#else
#define wlc_scan_get_cache(si, BSSID, nssid, SSID, BSS_type, c_list, c_num, bss_list)	\
	(void)((bss_list)->count = 0)
#endif // endif

#ifdef CNTRY_DEFAULT
extern int wlc_scan_prohibited_channels_get(wlc_scan_info_t *scan, chanspec_t *chanspec_list,
	int channel_max);
#else
#define wlc_scan_prohibited_channels_get(scan_info, chanspec_list, channel_max) (0)
#endif /* CNTRY_DEFAULT */

extern wlc_bsscfg_t *wlc_scan_bsscfg(wlc_scan_info_t *scan_info);
extern void wlc_scan_timer_update(wlc_scan_info_t *wlc_scan_info, uint32 ms);

#if defined(STA)
/* pwrstats retrieval function */
extern int wlc_pwrstats_get_scan(wlc_scan_info_t *scan, uint8 *destptr, int destlen);
extern uint32 wlc_get_curr_scan_time(wlc_info_t *wlc);
extern uint32 wlc_curr_roam_scan_time(wlc_info_t *wlc);
#endif /* STA */

extern wlc_bsscfg_t *wlc_scanmac_get_bsscfg(wlc_scan_info_t *scan, int macreq, wlc_bsscfg_t *cfg);
struct ether_addr *wlc_scanmac_get_mac(wlc_scan_info_t *scan, int macreq, wlc_bsscfg_t *bsscfg);
extern int wlc_scanmac_update(wlc_scan_info_t *scan);
extern int wlc_scan_filter_channels(wlc_scan_info_t *scan_pub,
	chanspec_t *chspec_list, int num);
#if defined(WL_SCAN_STALL_CHECK)
extern int wlc_hc_scan_set(void *ctx, const uint8 *buf, uint16 type, uint16 len);
extern int wlc_hc_scan_get(wlc_info_t *wlc, wlc_if_t *wlcif,
        bcm_xtlv_t *params, void *out, uint o_len);
#endif /* WL_SCAN_STALL_CHECK */
extern int32 wlc_scan_fill_cache(wlc_scan_info_t *wlc_scan_info, wlc_bss_list_t *scan_results,
		wlc_scandb_t *sdb, uint current_timestamp);

#ifdef WLSCAN_SUMEVT
typedef enum wlc_scan_summary_opts {
	SCAN_SUMM_OPS_SEND,
	SCAN_SUMM_OPS_STATS_UPDATE
} wlc_scan_summary_opts_t;
void wlc_scan_summary_chan_stats_op(wlc_scan_info_t *wlc_scan_info, bool ishome,
	wlc_scan_summary_opts_t op);
#endif /* WLSCAN_SUMEVT */

#ifdef WL_OCE
extern void wlc_scan_probe_suppress(wlc_scan_info_t *scan_info);
extern int wlc_scan_update_current_slot_duration(wlc_info_t *wlc, uint32 duration);
extern uint8 wlc_scan_get_num_passes_left(wlc_info_t *wlc);
#endif /* WL_OCE */

#endif /* _WLC_SCAN_H_ */
