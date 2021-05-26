/*
 * Common (OS-independent) portion of
 * Broadcom 802.11abg Networking Device Driver
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
 * $Id: wlc_scan.c 791174 2020-09-18 07:37:03Z $
 */

/* XXX
 *
 * In wlc_scantimer function wlc_excursion_start/wlc_excursion_end may invoke
 * txstatus processing when switching a queue out of the DMA/FIFO, when which
 * happens some txcomplete routines such as wlc_nic_txcomplete fn may invoke
 * wlc_scan_terminate(), which in turn causes inconsistent scan state i.e.
 * scan->pass is in state WLC_STATE_ABORT but scan timer is not armed, which
 * in turn causes the scan state machine to stall when next time the scan is
 * requested and the scan API wlc_scan evaluates if it needs to start a timer
 * or not...
 *
 * In general we need to figure out a way to prevent re-enterancy between
 * APIs (public and private) such as wlc_scantimer() that could invoke user
 * callbacks from which other public APIs may be invoked recursively...
 */

/**
 * @file
 * @brief
 * XXX Twiki: [IncrementalScan] [ScanModularization] [WlExtendedScan]
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
#include <802.1d.h>
#include <802.11.h>
#include <802.11e.h>
#include <wpa.h>
#include <vlan.h>
#include <sbconfig.h>
#include <pcicfg.h>
#include <bcmsrom.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc_channel.h>
#include <wlc_scandb.h>
#include <wlc.h>
#include <wlc_dbg.h>
#include <wlc_hw.h>
#include <wlc_tx.h>
#include <wlc_phy_hal.h>
#include <phy_radio_api.h>
#include <phy_misc_api.h>
#include <wlc_event.h>
#include <wl_export.h>
#include <wlc_rm.h>
#include <wlc_ap.h>
#include <wlc_assoc.h>
#ifdef WLP2P
#include <wlc_p2p.h>
#endif // endif
#include <wlc_11h.h>
#include <wlc_11d.h>
#include <wlc_dfs.h>
#ifdef ANQPO
#include <wl_anqpo.h>
#endif // endif
#include <wlc_hw_priv.h>
#ifdef WL11K
#include <wlc_rrm.h>
#endif /* WL11K */
#include <wlc_obss.h>
#include <wlc_cca.h>
#ifdef WL_EXCESS_PMWAKE
#include <wlc_pm.h>
#endif /* WL_EXCESS_PMWAKE */
#ifdef WLPFN
#include <wl_pfn.h>
#endif // endif
#include <wlc_bmac.h>
#include <wlc_lq.h>
#ifdef WLSCAN_PS
#include <wlc_stf.h>
#endif // endif
#include <wlc_utils.h>
#include <wlc_msch.h>
#include <wlc_event_utils.h>
#include <wlc_scan_utils.h>
#include <wlc_dump.h>
#include <wlc_iocv.h>
#include <wlc_chanctxt.h>
#include <wlc_pm.h>
#ifdef WL_OCE
#include <wlc_oce.h>
#endif // endif
#include <wlc_btcx.h>
#include <wlc_scan_priv.h>
#include <wlc_scan.h>
#ifdef WLMCNX
#include <wlc_mcnx.h>
#endif /* WLMCNX */
#include <wlc_btcx.h>
#include <wlc_ops.h>
#include <hnd_ds.h>
#ifdef WL_AIR_IQ
#include <wlc_airiq.h>
#endif /* WL_AIR_IQ */
#ifdef WLOLPC
#include <wlc_olpc_engine.h>
#endif /* WLOLPC */
#if BAND6G
#include <fils.h>
#endif /* BAND6G */

#if defined(BCMDBG) || defined(WLMSG_INFORM)
#define	WL_INFORM_SCAN(args)                                                                    \
	do {                                                                            \
		if ((wl_msg_level & WL_INFORM_VAL) || (wl_msg_level2 & WL_SCAN_VAL))    \
		WL_PRINT(args);                                     \
	} while (0)
#undef WL_INFORM_ON
#define WL_INFORM_ON() ((wl_msg_level & WL_INFORM_VAL) || (wl_msg_level2 & WL_SCAN_VAL))
#else
#define        WL_INFORM_SCAN(args)
#endif /* BCMDBG */

/* scan times in milliseconds */
#define WLC_SCAN_MIN_PROBE_TIME		10	/* minimum useful time for an active probe */
#define WLC_SCAN_ASSOC_TIME		20	/* time to listen on a channel for probe resp while
						 * associated
						 */
#ifdef WL_SCAN_DFS_HOME
#define WLC_SCAN_DFS_AWAY_DURATION	20	/* On DFS home channel - max away time in ms */
#define WLC_SCAN_DFS_MIN_DWELL		10	/* On DFS home channel - min dwell time in sec */
#define WLC_SCAN_DFS_AUTO_REDUCE	0	/* Allow scan on DFS ch when active time/passive
						 * time is more than threshold by auto reducing scan
						 * time
						 */
#endif /* WL_SCAN_DFS_HOME */

#ifdef BCMQT_CPU
#define WLC_SCAN_UNASSOC_TIME		400	/* qt is slow */
#else
#define WLC_SCAN_UNASSOC_TIME		40	/* listen on a channel for prb rsp while
						 * unassociated
						*/
#endif // endif
#define WLC_SCAN_NPROBES		2	/* do this many probes on each channel for an
						 * active scan
						 */
#define SCAN_6G_PROBE_DEFER_TIME	25

/* Enables the iovars */
#define WLC_SCAN_IOVARS

#if defined(BRINGUP_BUILD)
#undef WLC_SCAN_IOVARS
#endif // endif

static void wlc_scan_watchdog(void *hdl);

static void wlc_scantimer(void *arg);

static int scanmac_get(scan_info_t *scan_info, void *params, uint p_len, void *arg, int len);
static int scanmac_set(scan_info_t *scan_info, void *arg, int len);
static void wlc_scan_set_params(
	wlc_scan_info_t *wlc_scan_info,
	wlc_bsscfg_t *cfg,
	int scan_type,
	int nprobes,
	int active_time,
	int passive_time,
	bool save_prb,
	bool include_cache,
	uint scan_flags,
	struct ether_addr *sa_override,
	scancb_fn_t fn,
	void* arg,
	actcb_fn_t act_cb,
	void *act_cb_arg);

static int wlc_scan_set_chanparams(scan_info_t *scan_info, const chanspec_t* chanspec_list,
	int n_channels);
static void wlc_scan_set_p2pparams(scan_info_t *scan_info, wlc_ssid_t *ssid, int nssid);
static void wlc_scan_alloc_ssidlist(scan_info_t *scan_info, int nssid, wlc_ssid_t *ssid);

static int
_wlc_scan(
	wlc_scan_info_t *wlc_scan_info,
	int bss_type,
	const struct ether_addr* bssid,
	int nssid,
	wlc_ssid_t *ssid,
	const chanspec_t* chanspec_list,
	int n_channels,
	chanspec_t chanspec_start,
	uint scan_flags,
	int bandinfo);

#ifdef WLC_SCAN_IOVARS
static int wlc_scan_doiovar(void *hdl, uint32 actionid,
	void *params, uint p_len, void *arg,
	uint len, uint val_size, struct wlc_if *wlcif);
#endif /* WLC_SCAN_IOVARS */

static int wlc_scan_ioctl(void *ctx, uint cmd, void *arg, uint len, struct wlc_if *wlcif);

static void wlc_scan_callback(scan_info_t *scan_info, uint status);
static int wlc_scan_apply_scanresults(scan_info_t *scan_info, int status);

static uint wlc_scan_prohibited_channels(scan_info_t *scan_info,
	chanspec_t *chanspec_list, int channel_max);
static void wlc_scan_do_pass(scan_info_t *scan_info, chanspec_t chanspec);
static void wlc_scan_sendprobe(wlc_info_t *wlc, void *arg, uint *dwell);
static void wlc_scan_ssidlist_free(scan_info_t *scan_info);
static int wlc_scan_chnsw_clbk(void* handler_ctxt, wlc_msch_cb_info_t *cb_info);
static int wlc_scan_schd_channels(wlc_scan_info_t *wlc_scan_info);
static int wlc_scan_add_chanspec(scan_info_t *scan_info, chanspec_t chanspec);
static uint32 wlc_scan_get_bandmask(scan_info_t *scan_info, uint bandtype);
static void wlc_scan_get_default_channels(scan_info_t *scan_info);

#if defined(BCMDBG) || defined(WLMSG_INFORM)
static void wlc_scan_print_ssids(wlc_ssid_t *ssid, int nssid);
#endif // endif
#if defined(BCMDBG)
static int wlc_scan_dump(scan_info_t *si, struct bcmstrbuf *b);
#endif // endif

static void wlc_ht_obss_scan_update(scan_info_t *scan_info, int status);

#ifdef WLSCANCACHE
static void wlc_scan_merge_cache(scan_info_t *scan_info, uint current_timestamp,
	const struct ether_addr *BSSID, int nssid, const wlc_ssid_t *SSID,
	int BSS_type, const chanspec_t *chanspec_list, uint chanspec_num,
	wlc_bss_list_t *bss_list);
static void wlc_scan_build_cache_list(void *arg1, void *arg2, uint timestamp,
	struct ether_addr *BSSID, wlc_ssid_t *SSID,
	int BSS_type, chanspec_t chanspec, void *data, uint datalen);
static void wlc_scan_cache_result(scan_info_t *scan_info);
#else
#define wlc_scan_cache_result(si)
#endif /* WLSCANCACHE */

#ifdef WLSCAN_PS
static int wlc_scan_ps_config_cores(scan_info_t *scan_info, bool flag);
#endif // endif

static void wlc_scan_bss_deinit(void *ctx, wlc_bsscfg_t *cfg);

static void wlc_scan_excursion_end(scan_info_t *scan_info);

#ifdef BCMDBG
static int wlc_scan_test(scan_info_t *scan_info, uint8 test_case);
#endif // endif

#if BAND6G
#define SCAN_6GCHAN_TO_INDEX(chn) ((chn - 1u)/4u)
#define WL_NUMCHANNELS_6G_PSC		15u
static void wlc_scan_determine_6g_params(scan_info_t *scan_info, uint n_channel, uint scan_flags,
	const struct ether_addr *bssid);
static void wlc_scan_6g_prune_chanlist(chanspec_t *chan_ptr, chanspec_t chanspec, bool psc);
static bool wlc_scan_find_6g_chan_start(scan_info_t *scan_info);
static void wlc_scan_6g_chan_handle(scan_info_t *scan_info);
static void wlc_scan_6g_process_non_rnr(wlc_scan_info_t *wlc_scan_info);
static void wlc_scan_6g_process_psc(wlc_scan_info_t *wlc_scan_info);
static void wlc_scan_handle_probereq_for_6g(wlc_scan_info_t *wlc_scan_info, wlc_bsscfg_t *cfg,
	wlc_ssid_t *ssid, const struct ether_addr *da);
static void wlc_scan_6g_handle_chan_states(scan_info_t *scan_info);
static bool
wlc_scan_6g_channel_is_rnr_reported(wlc_scan_info_t *wlc_scan_info, uint8 channel);
static void
wlc_scan_6g_process_rnr(wlc_scan_info_t *wlc_scan_info);
static bool wlc_scan_6g_check_chan_list_avail(scan_info_t *scan_info);
static int wlc_scan_rnr_list_set(wlc_info_t *wlc, rnr_list_req_t *rnr_list_req);
#endif /* BAND6G */

#define CHANNEL_PASSIVE_DWELLTIME(s) ((s)->passive_time)
#define CHANNEL_ACTIVE_DWELLTIME(s) ((s)->active_time)
#define SCAN_INVALID_DWELL_TIME		0

#define SCAN_PROBE_TXRATE(scan_info)    0
#if defined(HEALTH_CHECK)
static int wlc_hchk_scanmodule(uint8 *buffer, uint16 length,
	void *context,	int16 *bytes_written);
#endif // endif

#ifdef WLC_SCAN_IOVARS
/* IOVar table */

/* Parameter IDs, for use only internally to wlc -- in the wlc_iovars
 * table and by the wlc_doiovar() function.  No ordering is imposed:
 * the table is keyed by name, and the function uses a switch.
 */
enum {
	IOV_PASSIVE = 1,
	IOV_SCAN_ASSOC_TIME,
	IOV_SCAN_UNASSOC_TIME,
	IOV_SCAN_PASSIVE_TIME,
	IOV_SCAN_NPROBES,
	IOV_SCAN_EXTENDED,
	IOV_SCAN_NOPSACK,
	IOV_SCANCACHE,
	IOV_SCANCACHE_TIMEOUT,
	IOV_SCANCACHE_CLEAR,
	IOV_SCAN_ASSOC_TIME_DEFAULT,
	IOV_SCAN_UNASSOC_TIME_DEFAULT,
	IOV_SCAN_PASSIVE_TIME_DEFAULT,
	IOV_SCAN_DBG,
	IOV_SCAN_TEST,
	IOV_SCAN_RX_PWRSAVE,	/* reduce rx chains for pwr optimization */
	IOV_SCAN_TX_PWRSAVE,	/* reduce tx chains for pwr optimization */
	IOV_SCAN_PWRSAVE,	/* turn on/off bith tx and rx for single core scanning  */
	IOV_SCANMAC,
	IOV_SCAN_SUPPRESS,	/* scan suppress IOVAR */
	IOV_PASSIVE_ON_RESTRICTED_MODE,
#ifdef WL_SCAN_DFS_HOME
	IOV_SCAN_DFS_HOME_AWAY_DURATION,   /* On DFS home ch - max away time in millisec */
	IOV_SCAN_DFS_HOME_MIN_DWELL,        /* On DFS home ch - min dwell time in microsec */
	IOV_SCAN_DFS_HOME_AUTO_REDUCE,     /* Toggle based on iovar */
#endif /* WL_SCAN_DFS_HOME */
	IOV_SCAN_FORCE_TX,	/* can transmit on DFS scan channel */
	IOV_SCAN_RNR_LIST,      /* my rnr_list setting */
	IOV_SCAN_VER,		/* To get version of the scan */
	IOV_LAST		/* In case of a need to check max ID number */
};

/* AP IO Vars */
static const bcm_iovar_t wlc_scan_iovars[] = {
	{"passive", IOV_PASSIVE,
	(IOVF_NTRL|IOVF_OPEN_ALLOW), 0, IOVT_UINT16, 0
	},
#ifdef STA
	{"scan_assoc_time", IOV_SCAN_ASSOC_TIME,
	(IOVF_NTRL|IOVF_OPEN_ALLOW), 0, IOVT_UINT16, 0
	},
	{"scan_unassoc_time", IOV_SCAN_UNASSOC_TIME,
	(IOVF_NTRL|IOVF_OPEN_ALLOW), 0, IOVT_UINT16, 0
	},
#endif /* STA */
	{"scan_passive_time", IOV_SCAN_PASSIVE_TIME,
	(IOVF_WHL|IOVF_OPEN_ALLOW), 0, IOVT_UINT16, 0
	},
#ifdef STA
	{"scan_nprobes", IOV_SCAN_NPROBES,
	(IOVF_NTRL|IOVF_OPEN_ALLOW), 0, IOVT_INT8, 0
	},
#ifdef WLSCANCACHE
	{"scancache", IOV_SCANCACHE,
	(IOVF_OPEN_ALLOW), 0, IOVT_BOOL, 0
	},
	{"scancache_timeout", IOV_SCANCACHE_TIMEOUT,
	(IOVF_OPEN_ALLOW), 0, IOVT_INT32, 0
	},
	{"scancache_clear", IOV_SCANCACHE_CLEAR,
	(IOVF_OPEN_ALLOW), 0, IOVT_VOID, 0
	},
#endif /* WLSCANCACHE */
#endif /* STA */
#ifdef STA
	{"scan_assoc_time_default", IOV_SCAN_ASSOC_TIME_DEFAULT,
	(IOVF_NTRL|IOVF_OPEN_ALLOW), 0, IOVT_UINT16, 0
	},
	{"scan_unassoc_time_default", IOV_SCAN_UNASSOC_TIME_DEFAULT,
	(IOVF_NTRL|IOVF_OPEN_ALLOW), 0, IOVT_UINT16, 0
	},
#endif /* STA */
	{"scan_passive_time_default", IOV_SCAN_PASSIVE_TIME_DEFAULT,
	(IOVF_NTRL|IOVF_OPEN_ALLOW), 0, IOVT_UINT16, 0
	},
#ifdef BCMDBG
	{"scan_dbg", IOV_SCAN_DBG, 0, 0, IOVT_UINT8, 0},
	{"scan_test", IOV_SCAN_TEST, 0, 0, IOVT_UINT8, 0},
#endif // endif
#ifdef WLSCAN_PS
#if defined(BCMDBG) || defined(WLTEST)
	/* debug iovar to enable power optimization in rx */
	{"scan_rx_ps", IOV_SCAN_RX_PWRSAVE, (IOVF_WHL), 0, IOVT_UINT8, 0},
	/* debug iovar to enable power optimization in tx */
	{"scan_tx_ps", IOV_SCAN_TX_PWRSAVE, (IOVF_WHL), 0, IOVT_UINT8, 0},
#endif /* defined(BCMDBG) || defined(WLTEST) */
	/* single core scanning to reduce power consumption */
	{"scan_ps", IOV_SCAN_PWRSAVE, (IOVF_WHL), 0, IOVT_UINT8, 0},
#endif /* WLSCAN_PS */
	/* configurable scan MAC */
	{"scanmac", IOV_SCANMAC, 0, 0, IOVT_BUFFER, OFFSETOF(wl_scanmac_t, data)},
	{"scansuppress", IOV_SCAN_SUPPRESS, (0), 0, IOVT_BOOL, 0},
	{"passive_on_restricted_mode", IOV_PASSIVE_ON_RESTRICTED_MODE, (0), 0, IOVT_UINT32, 0},
#ifdef WL_SCAN_DFS_HOME
	{"scan_dfs_home_away_duration", IOV_SCAN_DFS_HOME_AWAY_DURATION, (IOVF_WHL), 0,
	(IOVT_UINT8), 0},
	{"scan_dfs_home_min_dwell", IOV_SCAN_DFS_HOME_MIN_DWELL, (IOVF_WHL), 0, IOVT_UINT32, 0},
	{"scan_dfs_home_auto_reduce", IOV_SCAN_DFS_HOME_AUTO_REDUCE, (IOVF_WHL), 0, IOVT_UINT8, 0},
#endif /* WL_SCAN_DFS_HOME */

	{ "rnr_list", IOV_SCAN_RNR_LIST, 0, 0, IOVT_BUFFER, sizeof(rnr_list_req_t)},
	{ "scan_ver", IOV_SCAN_VER, (0), 0, IOVT_BUFFER, sizeof(wl_scan_version_t)},

	{NULL, 0, 0, 0, 0, 0}
};
#endif /* WLC_SCAN_IOVARS */

#define SCAN_GET_TSF_TIMERLOW(wlc)		(R_REG(wlc->osh, D11_TSFTimerLow(wlc)))

/* debug timer used in scan module */
/* #define DEBUG_SCAN_TIMER */
#ifdef DEBUG_SCAN_TIMER
static void
wlc_scan_add_timer_dbg(scan_info_t *scan, uint to, bool prd, const char *fname, int line)
{
	WL_SCAN(("wl%d: %s(%d): wl_add_timer: timeout %u tsf %u\n",
		scan->unit, fname, line, to, SCAN_GET_TSF_TIMERLOW(scan->wlc)));
	wl_add_timer(scan->wlc->wl, scan->timer, to, prd);
}

static bool
wlc_scan_del_timer_dbg(scan_info_t *scan, const char *fname, int line)
{
	WL_SCAN(("wl%d: %s(%d): wl_del_timer: tsf %u\n",
		scan->unit, fname, line, SCAN_GET_TSF_TIMERLOW(scan->wlc)));
	return wl_del_timer(scan->wlc->wl, scan->timer);
}
#define WLC_SCAN_ADD_TIMER(scan, to, prd) \
	      wlc_scan_add_timer_dbg(scan, to, prd, __FUNCTION__, __LINE__)
#define WLC_SCAN_DEL_TIMER(scan) \
	      wlc_scan_del_timer_dbg(scan, __FUNCTION__, __LINE__)
#define WLC_SCAN_ADD_TEST_TIMER(scan, to, prd) \
	wl_add_timer((scan)->wlc->wl, (scan)->test_timer, (to), (prd))
#else /* DEBUG_SCAN_TIMER */
#define WLC_SCAN_ADD_TIMER(scan, to, prd) wl_add_timer((scan)->wlc->wl, (scan)->timer, (to), (prd))
#define WLC_SCAN_DEL_TIMER(scan) wl_del_timer((scan)->wlc->wl, (scan)->timer)
#define WLC_SCAN_ADD_TEST_TIMER(scan, to, prd) \
	wl_add_timer((scan)->wlc->wl, (scan)->test_timer, (to), (prd))
#endif /* DEBUG_SCAN_TIMER */
#define WLC_SCAN_FREE_TIMER(scan)	wl_free_timer((scan)->wlc->wl, (scan)->timer)

#define WLC_SCAN_LENGTH_ERR	"%s: Length verification failed len: %d sm->len: %d\n"

#ifdef BCMDBG
#define SCAN_DBG_ENT	0x1
#define WL_SCAN_ENT(scan, x)	do {					\
		if (WL_SCAN_ON() && ((scan)->debug & SCAN_DBG_ENT))	\
			printf x;					\
	} while (0)
#else /* !BCMDBG */
#define WL_SCAN_ENT(scan, x)
#endif /* !BCMDBG */

#ifdef BCMDBG
/* some test cases */
#define SCAN_TEST_NONE			0
#define SCAN_TEST_ABORT_PSPEND		1	/* abort after sending PM1 indication */
#define SCAN_TEST_ABORT_PSPEND_AND_SCAN	2	/* abort after sending PM1 indication and scan */
#define SCAN_TEST_ABORT_WSUSPEND	3	/* abort after tx suspend */
#define SCAN_TEST_ABORT_WSUSPEND_AND_SCAN 4	/* abort after suspend and scan */
#define SCAN_TEST_ABORT_ENTRY		5	/* abort right away after the scan request */
#endif // endif

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

wlc_scan_info_t*
BCMATTACHFN(wlc_scan_attach)(wlc_info_t *wlc, void *wl, osl_t *osh, uint unit)
{
	scan_info_t *scan_info;
	wlc_iov_disp_fn_t iovar_fn = NULL;
	const bcm_iovar_t *iovars = NULL;
	int	err = 0;
	uint	scan_info_size = (uint)sizeof(scan_info_t);
	uint	ssid_offs, chan_offs;

	ssid_offs = scan_info_size = ROUNDUP(scan_info_size, sizeof(uint32));
	scan_info_size += sizeof(wlc_ssid_t) * WLC_SCAN_NSSID_PREALLOC;
	chan_offs = scan_info_size = ROUNDUP(scan_info_size, sizeof(uint32));

	scan_info = (scan_info_t *)MALLOCZ(osh, scan_info_size);
	if (scan_info == NULL) {
		WL_ERROR((WLC_MALLOC_ERR, unit, __FUNCTION__, (int)scan_info_size,
			MALLOCED(osh)));
		return NULL;
	}

	scan_info->scan_pub = (struct wlc_scan_info *)MALLOCZ(osh, sizeof(struct wlc_scan_info));
	if (scan_info->scan_pub == NULL) {
		WL_ERROR((WLC_MALLOC_ERR, unit, __FUNCTION__,
			(int)sizeof(struct wlc_scan_info), MALLOCED(osh)));
		MFREE(osh, scan_info, scan_info_size);
		return NULL;
	}

	scan_info->scan_pub->scan_priv = (void *)scan_info;

#if defined(WLSCANCACHE) && !defined(WLSCANCACHE_DISABLED)
	scan_info->sdb = wlc_scandb_create(osh, unit, wlc->pub->tunables->max_scancache_results);
	if (scan_info->sdb == NULL) {
		WL_ERROR((WLC_MALLOC_ERR, unit, __FUNCTION__,
			(int)wlc->pub->tunables->max_scancache_results, MALLOCED(osh)));
		goto error;
	}
	wlc->pub->cmn->_scancache_support = TRUE;
	scan_info->scan_pub->_scancache = FALSE; /* disabled by default */
#endif /* WLSCANCACHE && !WLSCANCACHE_DISABLED */

	scan_info->memsize = scan_info_size;
	scan_info->wlc = wlc;
	scan_info->osh = osh;
	scan_info->unit = unit;
	scan_info->channel_idx = -1;
	scan_info->scan_pub->in_progress = FALSE;
	/* Initialise as all variables */
	scan_info->in_progress_duration = 0;
	scan_info->scanstall_threshold = DEFAULT_SCAN_STALL_THRESHOLD;

	scan_info->defaults.assoc_time = WLC_SCAN_ASSOC_TIME;
	scan_info->defaults.unassoc_time = WLC_SCAN_UNASSOC_TIME;
	scan_info->defaults.passive_time = WLC_SCAN_PASSIVE_TIME;
	scan_info->defaults.nprobes = WLC_SCAN_NPROBES;
	scan_info->defaults.passive = FALSE;
#ifdef WL_SCAN_DFS_HOME
	scan_info->scan_dfs_away_duration = WLC_SCAN_DFS_AWAY_DURATION;
	scan_info->scan_dfs_min_dwell = WLC_SCAN_DFS_MIN_DWELL;
	scan_info->scan_dfs_auto_reduce = WLC_SCAN_DFS_AUTO_REDUCE;
#endif /* WL_SCAN_DFS_HOME */

	scan_info->timer = wl_init_timer((struct wl_info *)wl,
	                                 wlc_scantimer, scan_info, "scantimer");
	if (scan_info->timer == NULL) {
		WL_ERROR(("wl%d: %s: wl_init_timer for scan timer failed\n", unit, __FUNCTION__));
		goto error;
	}

#ifdef WLC_SCAN_IOVARS
	iovar_fn = wlc_scan_doiovar;
	iovars = wlc_scan_iovars;
#endif /* WLC_SCAN_IOVARS */

	scan_info->ssid_prealloc = (wlc_ssid_t*)((uintptr)scan_info + ssid_offs);
	scan_info->nssid_prealloc = WLC_SCAN_NSSID_PREALLOC;
	scan_info->ssid_list = scan_info->ssid_prealloc;

	BCM_REFERENCE(chan_offs);

	if (PWRSTATS_ENAB(wlc->pub)) {
		scan_info->scan_stats =
			(wl_pwr_scan_stats_t *)MALLOCZ(osh, sizeof(*scan_info->scan_stats));
		if (scan_info->scan_stats == NULL) {
			WL_ERROR(("wl%d: %s: failure allocating power stats\n",
				unit, __FUNCTION__));
			goto error;
		}
	}
	err = wlc_module_register(wlc->pub, iovars, "scan", scan_info, iovar_fn,
		wlc_scan_watchdog, NULL, wlc_scan_down);
	if (err) {
		WL_ERROR(("wl%d: %s: wlc_module_register err=%d\n",
		          unit, __FUNCTION__, err));
		goto error;
	}

	err = wlc_module_add_ioctl_fn(wlc->pub, (void *)scan_info->scan_pub,
	                              wlc_scan_ioctl, 0, NULL);
	if (err) {
		WL_ERROR(("wl%d: %s: wlc_module_add_ioctl_fn err=%d\n",
		          unit, __FUNCTION__, err));
		goto error;
	}

	/* register bsscfg deinit callback through cubby registration */
	if ((err = wlc_bsscfg_cubby_reserve(wlc, 0, NULL, wlc_scan_bss_deinit, NULL,
			scan_info)) < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve err=%d\n",
		          unit, __FUNCTION__, err));
		goto error;
	}

#if defined(BCMDBG)
	wlc_dump_register(wlc->pub, "scan", (dump_fn_t)wlc_scan_dump, (void *)scan_info);
#ifdef WLSCANCACHE
	if (SCANCACHE_SUPPORT(wlc))
		wlc_dump_register(wlc->pub, "scancache", wlc_scandb_dump, scan_info->sdb);
#endif /* WLSCANCACHE */
#endif // endif

#ifdef BCMDBG
	/* scan_info->debug = SCAN_DBG_ENT; */
#endif // endif

#ifdef WLSCAN_PS
	scan_info->scan_ps_txchain = 0;
	scan_info->scan_ps_rxchain = 0;
	/* disable scan power optimization by default */
#if defined(BCMDBG) || defined(WLTEST)
	scan_info->scan_rx_pwrsave = FALSE;
	scan_info->scan_tx_pwrsave = FALSE;
#endif /* defined(BCMDBG) || defined(WLTEST) */
	scan_info->pwrsave_enable = FALSE;
	scan_info->mimo_override = FALSE;
#endif /* WLSCAN_PS */

#if defined(WLSCAN_PS) && defined(WL_STF_ARBITRATOR)
	if (WLSCAN_PS_ENAB(wlc->pub) && WLC_STF_ARB_ENAB(wlc->pub)) {
		/* Register with STF Aribitrator module for configuring chains */
		scan_info->stf_scan_req = wlc_stf_nss_request_register(wlc,
			WLC_STF_ARBITRATOR_REQ_ID_SCAN,
			WLC_STF_ARBITRATOR_REQ_PRIO_SCAN,
			NULL, NULL);
		if (!scan_info->stf_scan_req) {
			goto error;
		}
	}
#endif /* WLSCAN_PS && WL_STF_ARBITRATOR */

#if defined(HEALTH_CHECK)
	if (WL_HEALTH_CHECK_ENAB(wlc->pub->cmn)) {
		if (!wl_health_check_module_register(wlc->wl, "wl_scan_stall_check",
			wlc_hchk_scanmodule, wlc, WL_HC_DD_SCAN_STALL)) {
			goto error;
		}
	}
#endif /* HEALTH_CHECK */

	/* create notification list for scan start */
	if ((bcm_notif_create_list(wlc->notif, &scan_info->scan_start_h)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: bcm_notif_create_list() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto error;
	}

	return scan_info->scan_pub;

error:
	if (scan_info) {
		if (scan_info->timer != NULL)
			WLC_SCAN_FREE_TIMER(scan_info);
		if (scan_info->sdb) {
			wlc_scandb_free(scan_info->sdb);
			scan_info->sdb = NULL;
		}
		if (scan_info->scan_pub)
			MFREE(osh, scan_info->scan_pub, sizeof(struct wlc_scan_info));
		if (PWRSTATS_ENAB(wlc->pub) && scan_info->scan_stats) {
			MFREE(osh, scan_info->scan_stats, sizeof(wl_pwr_scan_stats_t));
		}
		MFREE(osh, scan_info, scan_info_size);
	}

	return NULL;
} /* wlc_scan_attach */

static void
wlc_scan_ssidlist_free(scan_info_t *scan_info)
{
	if (scan_info->ssid_list != scan_info->ssid_prealloc) {
		MFREE(scan_info->osh, scan_info->ssid_list,
		      scan_info->nssid * sizeof(wlc_ssid_t));
		scan_info->ssid_list = scan_info->ssid_prealloc;
		scan_info->nssid = scan_info->nssid_prealloc;
	}
}

/** Called when the WLC subsystem goes down */
int
BCMUNINITFN(wlc_scan_down)(void *hdl)
{
#ifdef WL_UCM
	wlc_bsscfg_t *ucm_bsscfg = NULL;
#endif /* WL_UCM */
#ifndef BCMNODOWN
	scan_info_t *scan_info = (scan_info_t *)hdl;
	int callbacks = 0;
	wlc_info_t *wlc = scan_info->wlc;
	int err;

	wlc_bss_list_free(wlc, wlc->scan_results);

	if (!WLC_SCAN_DEL_TIMER(scan_info))
		callbacks ++;

	/* Cancel the MSCH request */
	if (scan_info->msch_req_hdl) {
		if ((err = wlc_msch_timeslot_unregister(wlc->msch_info,
			&scan_info->msch_req_hdl)) != BCME_OK) {
			WL_SCAN_ERROR(("wl%d: %s: MSCH timeslot unregister failed, err %d\n",
				scan_info->unit, __FUNCTION__, err));
		}

		scan_info->msch_req_hdl = NULL;
	}

	scan_info->state = WLC_SCAN_STATE_START;
	scan_info->channel_idx = -1;
	scan_info->scan_pub->in_progress = FALSE;
#ifdef WL_SCAN_DFS_HOME
	scan_info->prev_scan = 0;
#endif /* WL_SCAN_DFS_HOME */

#ifdef HEALTH_CHECK
	if (WL_HEALTH_CHECK_ENAB(wlc->pub->cmn)) {
		/* Clear scan in progress counter */
		scan_info->in_progress_duration = 0;
	}
#endif /* HEALTH_CHECK */
#ifdef WL_UCM
	if (UCM_ENAB(wlc->pub)) {
		wlc_btc_get_matched_chanspec_bsscfg(wlc, wlc->home_chanspec, &ucm_bsscfg);
		if (wlc_btc_apply_profile(wlc, ucm_bsscfg) == BCME_ERROR) {
			WL_ERROR(("wl%d: %s: BTCOEX settings error: chspec %d!\n",
				wlc->pub->unit, __FUNCTION__,
				CHSPEC_CHANNEL(wlc->home_chanspec)));
			ASSERT(0);
		}
	}
#endif /* WL_UCM */

	wlc_phy_hold_upd(WLC_PI(wlc), PHY_HOLD_FOR_SCAN, FALSE);

	wlc_scan_ssidlist_free(scan_info);

	return callbacks;
#else
	return 0;
#endif /* BCMNODOWN */
} /* wlc_scan_down */

void
BCMATTACHFN(wlc_scan_detach)(wlc_scan_info_t *wlc_scan_info)
{
	scan_info_t *scan_info;
	osl_t *osh;
	wlc_info_t *wlc;

	if (!wlc_scan_info)
		return;

	scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
	ASSERT(scan_info);

	osh = scan_info->osh;
	wlc = scan_info->wlc;

	if (scan_info->scan_start_h) {
		bcm_notif_delete_list(&scan_info->scan_start_h);
	}

	if (scan_info->timer) {
		WLC_SCAN_FREE_TIMER(scan_info);
		scan_info->timer = NULL;
	}

	wlc_module_unregister(wlc->pub, "scan", scan_info);

	(void)wlc_module_remove_ioctl_fn(wlc->pub, (void *)scan_info->scan_pub);

	ASSERT(scan_info->ssid_list == scan_info->ssid_prealloc);
	if (scan_info->ssid_list != scan_info->ssid_prealloc) {
		WL_ERROR(("wl%d: %s: ssid_list not set to prealloc\n",
			scan_info->unit, __FUNCTION__));
	}

	if (scan_info->scan_stats) {
		MFREE(osh, scan_info->scan_stats, sizeof(*scan_info->scan_stats));
	}

#ifdef WLSCANCACHE
	if (SCANCACHE_SUPPORT(wlc)) {
		wlc_scandb_free(scan_info->sdb);
	}
#endif /* WLSCANCACHE */
#if defined(WLSCAN_PS) && defined(WL_STF_ARBITRATOR)
	if (WLSCAN_PS_ENAB(wlc->pub) && WLC_STF_ARB_ENAB(wlc->pub)) {
		if (scan_info->stf_scan_req &&
			wlc_stf_nss_request_unregister(wlc, scan_info->stf_scan_req))
		WL_STF_ARBITRATOR_ERROR(("ARBI: Error Unregister bsscfg's scan req\n"));
		scan_info->stf_scan_req = NULL;
	}
#endif /* WLSCAN_PS && WL_STF_ARBITRATOR */

	MFREE(osh, scan_info, scan_info->memsize);
	MFREE(osh, wlc_scan_info, sizeof(*wlc_scan_info));
} /* wlc_scan_detach */

#if defined(BCMDBG) || defined(WLMSG_INFORM)
/** printed when msglevel +info */
static void
wlc_scan_print_ssids(wlc_ssid_t *ssid, int nssid)
{
	char ssidbuf[SSID_FMT_BUF_LEN];
	int linelen = 0;
	int len;
	int i;

	for (i = 0; i < nssid; i++) {
		len = wlc_format_ssid(ssidbuf, ssid[i].SSID, ssid[i].SSID_len);
		/* keep the line length under 80 cols */
		if (linelen + (len + 2) > 80) {
			printf("\n");
			linelen = 0;
		}
		printf("\"%s\" ", ssidbuf);
		linelen += len + 3;
	}
	printf("\n");
}
#endif /* BCMDBG || WLMSG_INFORM */

/** returns TRUE if the caller specified chanspec is within the chanspec_list */
bool
wlc_scan_in_scan_chanspec_list(wlc_scan_info_t *wlc_scan_info, chanspec_t chanspec)
{
	scan_info_t *scan_info = (scan_info_t *) wlc_scan_info->scan_priv;
	int i;
	uint8 chan;

	/* scan chanspec list not setup, return no match */
	if (scan_info->channel_idx == -1) {
		WL_INFORM_SCAN(("%s: Scan chanspec list NOT setup, NO match\n", __FUNCTION__));
		return FALSE;
	}

	/* if strict channel match report is not needed, return match */
	if (wlc_scan_info->state & SCAN_STATE_OFFCHAN)
		return TRUE;

	chan = wf_chspec_ctlchan(chanspec);
	for (i = 0; i < scan_info->channel_num; i++) {
		if (wf_chspec_ctlchan(scan_info->chanspec_list[i]) == chan) {
			return TRUE;
		}
	}

	return FALSE;
}

#ifdef STA
static void
wlc_scan_start_notif(wlc_info_t *wlc, wlc_ssid_t *ssid)
{
	scan_info_t *scan_info = (scan_info_t *)wlc->scan->scan_priv;
	bcm_notif_h hdl = scan_info->scan_start_h;

	bcm_notif_signal(hdl, ssid);
}
#endif /* STA */

#ifdef WL_SCAN_DFS_HOME

/* In FCC allow scan on dfs channel when 11H is enabled.
 * On success return true, On failure return BCME_SCANREJECT
 */
static int
wlc_scan_on_dfs_chan(wlc_info_t *wlc, int chanspec_num, chanspec_t chanspec,
	int scan_type, int *active_time, int *passive_time)
{
	uint32 high, low;
	uint64 now = 0;
	scan_info_t *scan_info = (scan_info_t *) wlc->scan->scan_priv;
	wlc_bsscfg_t *other_cfg;
	int idx = 0;
	bool p2p_go = FALSE;
	uint scan_check_time = (WLC_DFS_RADAR_CHECK_INTERVAL - (scan_info->scan_dfs_away_duration));

	if (wlc_dfs_scan_in_progress(wlc->dfs)) {
		WL_ERROR(("wl%d: %s: WLC_SCAN rejected when BGDFS is in progress\n",
				wlc->pub->unit, __FUNCTION__));
		return BCME_SCANREJECT;
	}

	FOREACH_BSS(wlc, idx, other_cfg) {
		if (P2P_GO(wlc, other_cfg)) {
			p2p_go = TRUE;
			break;
		}
	}

	if (!wlc->clk) {
		return BCME_OK;
	}

	wlc_read_tsf(wlc, &low, &high);
	now = (((uint64)((uint32)high) << 32) | (uint32)low);

	/* do not reject in the following cases */
	if (!WL11H_ENAB(wlc) ||						/* non-11h */
			(AP_ENAB(wlc->pub) && p2p_go) ||		/* AP but P2P GO */
			BSSCFG_STA(wlc->primary_bsscfg) ||		/* STA */
			!wlc_radar_chanspec(wlc->cmi, wlc->home_chanspec) || /* non-radar ch */
			wlc->is_edcrs_eu ||				/* is EDCRS_EU */
			WLC_APSTA_ON_RADAR_CHANNEL(wlc) ||		/* repeater */
			!AP_ACTIVE(wlc)) {				/* AllowScanDuringStartUp */
		scan_info->prev_scan = now;
		return BCME_OK;
	}

	if ((now - wlc->scan->last_radar_poll) > (scan_check_time * 1000u)) {
		WL_REGULATORY(("wl%d: %s: WLC_SCAN rejected as it may interfere with radar \n",
				wlc->pub->unit, __FUNCTION__));
		return BCME_SCANREJECT;
	}

	if (wlc_dfs_monitor_mode(wlc->dfs)) {
		return BCME_SCANREJECT;
	}

	if (now - scan_info->prev_scan < (scan_info->scan_dfs_min_dwell * 1000000u)) {
		WL_ERROR(("wl%d: %s: WLC_SCAN ignored due to less dwell time %dsec\n",
				wlc->pub->unit, __FUNCTION__, scan_info->scan_dfs_min_dwell));
		return BCME_SCANREJECT;
	}
	if (chanspec_num != 1 || chanspec == 0) {
		WL_ERROR(("wl%d: %s: WLC_SCAN ignored because of multi chans num:%d, ch:0x%04x\n",
				wlc->pub->unit, __FUNCTION__, chanspec_num, chanspec));
		return BCME_SCANREJECT;
	}

	/*
	 * If scan is on a DFS channel AND
	 * transmissions on a scanned DFS channel is not allowed (AP only) AND
	 * channel is not the home channel then
	 * downgrade the scan to a passive scan.
	 */
	if (wlc_radar_chanspec(wlc->cmi, chanspec) &&
		(wf_chspec_ctlchan(chanspec) != wf_chspec_ctlchan(wlc->home_chanspec))) {
			scan_type = DOT11_SCANTYPE_PASSIVE;
			WL_SCAN(("wl%d: channel 0x%x scan downgraded to a passive scan\n",
					wlc->pub->unit, chanspec));
	}
	if (scan_type == DOT11_SCANTYPE_ACTIVE && *active_time < 0) {
		if (wlc->pub->associated) {
			*active_time = scan_info->defaults.assoc_time;
		} else {
			*active_time = scan_info->defaults.unassoc_time;
		}
		if (*active_time > scan_info->scan_dfs_away_duration &&
				scan_info->scan_dfs_auto_reduce) {
			*active_time = scan_info->scan_dfs_away_duration;
		}
	}
	if (scan_type == DOT11_SCANTYPE_PASSIVE && *passive_time < 0) {
		*passive_time = scan_info->defaults.passive_time;
		if (*passive_time > scan_info->scan_dfs_away_duration &&
				scan_info->scan_dfs_auto_reduce) {
			*passive_time = scan_info->scan_dfs_away_duration;
		}
	}
	if ((scan_type == DOT11_SCANTYPE_ACTIVE &&
			*active_time > scan_info->scan_dfs_away_duration) ||
			(scan_type == DOT11_SCANTYPE_PASSIVE &&
			*passive_time > scan_info->scan_dfs_away_duration)) {
		WL_ERROR(("wl%d: %s: scan ignored, active%d or passive time %d"
				"is more than the limit %d \n", wlc->pub->unit,
				__FUNCTION__, *active_time, *passive_time,
				scan_info->scan_dfs_away_duration));
		return BCME_SCANREJECT;
	}

	scan_info->prev_scan = now;

	return BCME_OK;
}
#endif /* WL_SCAN_DFS_HOME */

/**
 * Starts a scan, returns while the scan is in progress. Called by e.g. assoc module.
 * Caution: when passing a non-primary bsscfg to this function the caller must make sure to abort
 * the scan before freeing the bsscfg!
 *
 * @param[in] chanspec_list  List of channels to scan. NULL to scan default channels.
 * @param[in] n_channels     Number of items in chanspec_list. 0 to scan default channels.
 */
int
wlc_scan(
	wlc_scan_info_t *wlc_scan_info,
	int bss_type,
	const struct ether_addr* bssid,
	int nssid,
	wlc_ssid_t *ssid,
	int scan_type,
	int nprobes,
	int active_time,
	int passive_time,
	const chanspec_t* chanspec_list,
	int n_channels,
	chanspec_t chanspec_start,
	bool save_prb,
	scancb_fn_t fn, void* arg,
	bool include_cache,
	uint scan_flags,
	wlc_bsscfg_t *cfg,
	uint8 usage,
	actcb_fn_t act_cb,
	void *act_cb_arg,
	struct ether_addr *sa_override)
{
	scan_info_t *scan_info = (scan_info_t *) wlc_scan_info->scan_priv;
	uint ret_val;
	int bandtype = WLC_BAND_ALL;
#if defined(WL_SCAN_DFS_HOME) || defined(WL_AIR_IQ)
	int status;
#endif /* WL_SCAN_DFS_HOME || WL_AIR_IQ */

	WL_SCAN(("wl%d %s bss_type=%d nssid=%d nprobes=%d active_time=%d n_channels=%d"
		" save_prb=%d fn=%p scan_flags=0x%x usage=%d passive_time=%d\n",
		scan_info->unit, __FUNCTION__, bss_type, nssid, nprobes, active_time, n_channels,
		save_prb, fn, scan_flags, usage, passive_time));

#ifdef WL_SCAN_DFS_HOME
	ret_val = wlc_scan_on_dfs_chan(scan_info->wlc, n_channels,
		(chanspec_list ? chanspec_list[0] : 0), scan_type, &active_time, &passive_time);

	if (ret_val != BCME_OK) {
		WL_SCAN(("wl%d : %s failure due to err %d\n", scan_info->unit,
			__FUNCTION__, ret_val));
		status = WLC_E_STATUS_NOCHANS;
		goto fail_with_cb;
	}
#endif /* WL_SCAN_DFS_HOME */

	/* channel list validation */
	if (n_channels > SCAN_MAX_CHANSPECS) {
		WL_SCAN_ERROR(("wl%d: %s: wlc_scan bad param n_channels %d greater"
			" than max %d\n", scan_info->unit, __FUNCTION__,
			n_channels, SCAN_MAX_CHANSPECS));
		n_channels = 0;
	}

	if (n_channels > 0 && chanspec_list == NULL) {
		WL_SCAN_ERROR(("wl%d: %s: wlc_scan bad param channel_list was NULL"
			" with n_channels = %d\n",
			scan_info->unit, __FUNCTION__, n_channels));
		n_channels = 0;
	}
#ifdef WL_AIR_IQ
	if (wlc_airiq_scan_in_progress(scan_info->wlc)) {
		/* Abort the if airiq scan is already in progress. */
		WL_SCAN_ERROR(("wl%d %s AIRIQ scan in progress\n",
			scan_info->unit, __FUNCTION__));
		ret_val = BCME_SCANREJECT;
		status = WLC_E_STATUS_NEWSCAN;
		goto fail_with_cb;
	}
#endif /* WL_AIR_IQ */

#ifdef STA
	if (!AS_IN_PROGRESS(scan_info->wlc) && SCAN_IN_PROGRESS(wlc_scan_info)) {
		/* Abort the if any non-assoc scan is already in progress. */
		wlc_scan_abort(wlc_scan_info, WLC_E_STATUS_ABORT);
	}
#endif /* STA */

	/* usage is reset during abort .. so updating new reason here */
	wlc_scan_info->usage = (uint8)usage;

	/* When user requests an invalid channel explicitly, it means to scan all
	 * channels for the band requested.
	 * (reset n_channels to zero afterward to force full band scan)
	 */
	if ((n_channels == 1) && (chanspec_list != NULL) &&
	    (CHSPEC_CHANNEL(chanspec_list[0]) == 0)) {
		bandtype = CHSPEC_BANDTYPE(chanspec_list[0]);
		n_channels = 0;
	}

	WL_SCAN(("wl%d: %s: scan request\n", scan_info->unit, __FUNCTION__));
	WL_EAP_TRC_SCAN_DBG(("wl%d: %s: scan request type 0x%04x, at time 0x%08x, "
		"scanflags = 0x%x\n", scan_info->unit, __FUNCTION__, scan_type,
		SCAN_GET_TSF_TIMERLOW(scan_info->wlc), scan_flags));

	wlc_scan_set_params(wlc_scan_info, cfg, scan_type, nprobes, active_time, passive_time,
		save_prb, include_cache, scan_flags, sa_override, fn, arg, act_cb, act_cb_arg);

	ret_val = _wlc_scan(wlc_scan_info, bss_type, bssid, nssid, ssid,
		chanspec_list, n_channels, chanspec_start, scan_flags, bandtype);

#ifdef STA
	if (ret_val == BCME_OK) {
		wlc_scan_start_notif(scan_info->wlc, (nssid == 1) ? ssid : NULL);
	}
#endif /* STA */

	return ret_val;

#if defined(WL_SCAN_DFS_HOME) || defined(WL_AIR_IQ)
fail_with_cb:
	if (fn != NULL) {
		fn(arg, status, cfg);
	}
	return ret_val;
#endif /* WL_SCAN_DFS_HOME || WL_AIR_IQ */
} /* wlc_scan */

/** Store paramaters for scanning in struct. */
static void
wlc_scan_set_params(
	wlc_scan_info_t *wlc_scan_info,
	wlc_bsscfg_t *cfg,
	int scan_type,
	int nprobes,
	int active_time,
	int passive_time,
	bool save_prb,
	bool include_cache,
	uint scan_flags,
	struct ether_addr *sa_override,
	scancb_fn_t fn,
	void* arg,
	actcb_fn_t act_cb,
	void *act_cb_arg)
{
	scan_info_t *scan_info = (scan_info_t *) wlc_scan_info->scan_priv;
	wlc_info_t *wlc = scan_info->wlc;
	wlc_scan_info->state &= SCAN_STATE_SUPPRESS;

	scan_info->bsscfg = cfg ? cfg : wlc->primary_bsscfg;
	if (active_time > 0) {
		scan_info->active_time = (uint16)active_time;
	} else {
		if (wlc->pub->associated) {
			scan_info->active_time = scan_info->defaults.assoc_time;
		} else {
			scan_info->active_time = scan_info->defaults.unassoc_time;
		}
	}
	if (passive_time >= 0) {
		scan_info->passive_time = (uint16)passive_time;
	} else {
		scan_info->passive_time = scan_info->defaults.passive_time;
	}
	if (scan_info->defaults.passive)
		wlc_scan_info->state |= SCAN_STATE_PASSIVE;

	if (scan_type == DOT11_SCANTYPE_ACTIVE) {
		wlc_scan_info->state &= ~SCAN_STATE_PASSIVE;
	} else if (scan_type == DOT11_SCANTYPE_PASSIVE) {
		wlc_scan_info->state |= SCAN_STATE_PASSIVE;
	}

	if (save_prb)
		wlc_scan_info->state |= SCAN_STATE_SAVE_PRB;
	if (include_cache && SCANCACHE_ENAB(wlc_scan_info))
		wlc_scan_info->state |= SCAN_STATE_INCLUDE_CACHE;
	/* channel list validation */
#ifdef WL_PROXDETECT
	if (PROXD_ENAB(wlc->pub)) {
		wlc_scan_info->flag &= ~SCAN_FLAG_SWITCH_CHAN;
		if (scan_flags & WL_SCANFLAGS_SWTCHAN) {
			wlc_scan_info->flag |= SCAN_FLAG_SWITCH_CHAN;
		}
	}
#endif // endif
	if (scan_flags & WL_SCANFLAGS_OFFCHAN)
		scan_info->scan_pub->state |= SCAN_STATE_OFFCHAN;
	else
		scan_info->scan_pub->state &= ~SCAN_STATE_OFFCHAN;

	/* passive scan always has nprobes to 1 */
	if (wlc_scan_info->state & SCAN_STATE_PASSIVE) {
		scan_info->nprobes = 1;
	} else if (nprobes > 0) {
		scan_info->nprobes = (uint8)nprobes;
	} else {
		scan_info->nprobes = scan_info->defaults.nprobes;
	}

	if (ISSIM_ENAB(wlc->pub->sih)) {
		/* QT hack: abort scan since full scan may take forever */
		scan_info->channel_num = 1;
	}

	if (!sa_override || ETHER_ISNULLADDR(sa_override) ||
		IS_SCAN_TYPE_LISTEN_ON_CHANNEL(scan_flags)) {
		sa_override = &scan_info->bsscfg->cur_etheraddr;
	}
	memcpy(&scan_info->sa_override, sa_override, sizeof(scan_info->sa_override));

	scan_info->cb = fn;
	scan_info->cb_arg = arg;

	if (act_cb) {
		scan_info->act_cb = act_cb;
		scan_info->act_cb_arg = act_cb_arg;
	} else {
		/* Internal function to do scan with probe */
		scan_info->act_cb = wlc_scan_sendprobe;
		scan_info->act_cb_arg = NULL;
	}
} /* wlc_scan_set_params */

/** Configure scanning channel parameters */
static int
wlc_scan_set_chanparams(scan_info_t *scan_info, const chanspec_t* chanspec_list, int n_channels)
{
	wlc_info_t *wlc = scan_info->wlc;
	int i;
	int ret;

	for (i = 0; i < n_channels; i++) {
		if ((scan_info->scan_pub->state & SCAN_STATE_PROHIBIT) ||
			WLC_CNTRY_DEFAULT_ENAB(wlc)) {
			if (wf_chspec_valid(chanspec_list[i]) == FALSE) {
				return BCME_BADCHAN;
			}
		}
		else if (!wlc_valid_chanspec_db(wlc->cmi, chanspec_list[i])) {
			return BCME_BADCHAN;
		}
		/* CS5468559, cannot scan channels wider than 20Mhz if n mode is disabled */
		if (!N_ENAB(wlc->pub) && !CHSPEC_ISLE20(chanspec_list[i])) {
			WL_ERROR(("invalid scan channel 0x%04x: n mode disabled\n",
				chanspec_list[i]));
			return BCME_BADCHAN;
		}
	}

	scan_info->channel_num = 0;
	for (i = 0; i < n_channels; i++) {
#ifdef SLAVE_RADAR
		if (wlc_dfs_valid_ap_chanspec(wlc, chanspec_list[i]))
#endif // endif
		{
			ret = wlc_scan_add_chanspec(scan_info, chanspec_list[i]);
			if (ret) {
				return ret;
			}
		}
	}
	return BCME_OK;
}

/** Configure p2p specific parameters */
static void
wlc_scan_set_p2pparams(scan_info_t *scan_info, wlc_ssid_t *ssid, int nssid)
{
#ifdef WLP2P
	int i;
	if (P2P_ENAB(scan_info->wlc->pub)) {
		scan_info->ssid_wildcard_enabled = FALSE;
		for (i = 0; i < nssid; i++) {
			if (scan_info->ssid_list[i].SSID_len == 0)
				wlc_p2p_fixup_SSID(scan_info->wlc->p2p, scan_info->bsscfg,
					&scan_info->ssid_list[i]);
			if (scan_info->ssid_list[i].SSID_len == 0)
				scan_info->ssid_wildcard_enabled = TRUE;
		}
	}
#endif // endif
}

/** Allocated ssid_list for scanning */
static void
wlc_scan_alloc_ssidlist(scan_info_t *scan_info, int nssid, wlc_ssid_t *ssid)
{
	/* allocate memory for ssid list, using prealloc if sufficient */
	ASSERT(scan_info->ssid_list == scan_info->ssid_prealloc);
	if (scan_info->ssid_list != scan_info->ssid_prealloc) {
		WL_SCAN(("wl%d: %s: ssid_list not set to prealloc\n",
		          scan_info->unit, __FUNCTION__));
	}
	if (nssid > scan_info->nssid_prealloc) {
		scan_info->ssid_list = MALLOCZ(scan_info->osh,
		                              nssid * sizeof(wlc_ssid_t));
		/* failed, cap at prealloc (best effort) */
		if (scan_info->ssid_list == NULL) {
			WL_ERROR((WLC_MALLOC_ERR, scan_info->unit, __FUNCTION__,
				(int)(nssid * sizeof(wlc_ssid_t)),
				MALLOCED(scan_info->osh)));
			nssid = scan_info->nssid_prealloc;
			scan_info->ssid_list = scan_info->ssid_prealloc;
		}
	}
	/* Now ssid_list is the right size for [current] nssid count */

	bcopy(ssid, scan_info->ssid_list, (sizeof(wlc_ssid_t) * nssid));
	scan_info->nssid = nssid;
}

/**
 * Invoked by wlc_scan(). Starts a scan, returns while the scan is in progress.
 * @param[in] chanspec_list  List of channels to scan. NULL to scan default channels.
 * @param[in] n_channels     Number of items in chanspec_list. 0 to scan default channels.
 */
static int
_wlc_scan(
	wlc_scan_info_t *wlc_scan_info,
	int bss_type,
	const struct ether_addr* bssid,
	int nssid,
	wlc_ssid_t *ssid,
	const chanspec_t* chanspec_list,
	int n_channels,
	chanspec_t chanspec_start,
	uint scan_flags,
	int bandinfo)
{
	scan_info_t *scan_info = (scan_info_t *) wlc_scan_info->scan_priv;
	bool scan_in_progress;
	int i, num;
	wlc_info_t *wlc = scan_info->wlc;
#if defined(BCMDBG) || defined(WLMSG_INFORM)
	char *ssidbuf;
	char eabuf[ETHER_ADDR_STR_LEN];
	char chanbuf[CHANSPEC_STR_LEN];
#endif // endif
#ifdef WLMSG_ROAM
	char SSIDbuf[DOT11_MAX_SSID_LEN+1];
#endif // endif
	int ret = BCME_OK;
	int status;
#ifdef WLCHANIM
	int err;
#endif // endif

	ASSERT(nssid);
	ASSERT(ssid != NULL);
	ASSERT(bss_type == DOT11_BSSTYPE_INFRASTRUCTURE ||
	       bss_type == DOT11_BSSTYPE_INDEPENDENT ||
	       bss_type == DOT11_BSSTYPE_MESH ||
	       bss_type == DOT11_BSSTYPE_ANY);

#if defined(BCMDBG) || defined(WLMSG_INFORM)
	ssidbuf = (char *) MALLOCZ(scan_info->osh, SSID_FMT_BUF_LEN);
	if (ssidbuf == NULL) {
		WL_ERROR((WLC_MALLOC_ERR, scan_info->unit, __FUNCTION__, (int)SSID_FMT_BUF_LEN,
			MALLOCED(scan_info->osh)));
		status = WLC_E_STATUS_ERROR;
		ret = BCME_NOMEM;
		goto fail_with_cb;
	}

	if (nssid == 1) {
		wlc_format_ssid(ssidbuf, ssid->SSID, ssid->SSID_len);
		WL_INFORM_SCAN(("wl%d: %s: scanning for SSID \"%s\" (%d)\n", scan_info->unit,
			__FUNCTION__, ssidbuf, ssid->SSID_len));
	} else {
		WL_INFORM_SCAN(("wl%d: %s: scanning for SSIDs:\n", scan_info->unit, __FUNCTION__));
		if (WL_INFORM_ON())
			wlc_scan_print_ssids(ssid, nssid);
	}
	WL_INFORM_SCAN(("wl%d: %s: scanning for BSSID \"%s\"\n", scan_info->unit, __FUNCTION__,
	           (bcm_ether_ntoa(bssid, eabuf), eabuf)));
	MFREE(scan_info->osh, (void *)ssidbuf, SSID_FMT_BUF_LEN);
#endif /* BCMDBG || WLMSG_INFORM */

	scan_info->pass = 0;

	/* enforce valid argument */
	scan_info->ssid_wildcard_enabled = FALSE;
	for (i = 0; i < nssid; i++) {
		if (ssid[i].SSID_len > DOT11_MAX_SSID_LEN) {
			WL_SCAN(("wl%d: %s: invalid SSID len %d, capping\n",
			          scan_info->unit, __FUNCTION__, ssid[i].SSID_len));
			ssid[i].SSID_len = DOT11_MAX_SSID_LEN;
		}
		if (ssid[i].SSID_len == 0) {
			scan_info->ssid_wildcard_enabled = TRUE;
		}
	}

	scan_in_progress = SCAN_IN_PROGRESS(wlc_scan_info);

#ifdef ANQPO
	wlc_scan_info->is_hotspot_scan = FALSE;
	if (ANQPO_ENAB(wlc->pub) && (scan_flags & WL_SCANFLAGS_HOTSPOT)) {
		wl_anqpo_scan_start(wlc->anqpo);
		wlc_scan_info->is_hotspot_scan = TRUE;
	}
#endif // endif
#ifdef STA
#ifdef WL11D
	/* If we're doing autocountry, clear country info accumulation */
	if (WLC_AUTOCOUNTRY_ENAB(wlc))
		wlc_11d_scan_start(wlc->m11d);
#endif /* WL11D */
#endif /* STA */

	WL_SCAN(("wl%d: %s: wlc_scan params: nprobes %d dwell active/passive %dms/%dms"
		" flags %d\n",
		scan_info->unit, __FUNCTION__, scan_info->nprobes, scan_info->active_time,
		scan_info->passive_time, wlc_scan_info->state));

	WL_EAP_TRC_SCAN_DBG(("wl%d: %s: wlc_scan params: nprobes %d dwell active/passive %dms/%dms"
		" flags 0x%x\n",
		scan_info->unit, __FUNCTION__, scan_info->nprobes, scan_info->active_time,
		scan_info->passive_time, scan_flags));

	/* set required and optional params */
	/* If IBSS Lock Out feature is turned on, set the scan type to BSS only */
	wlc_scan_info->bss_type = (wlc->ibss_allowed == FALSE) ?
		DOT11_BSSTYPE_INFRASTRUCTURE : bss_type;
	bcopy((const char*)bssid, (char*)&wlc_scan_info->bssid, ETHER_ADDR_LEN);
	wlc_scan_alloc_ssidlist(scan_info, nssid, ssid);

#ifdef STA
	/* Update the mpc state to bring up the core */
	wlc->mpc_scan = TRUE;
	wlc_radio_mpc_upd(wlc);
#endif /* STA */

	if (!wlc->pub->up) {
		/* when the driver is going down, mpc can't pull up the core */
		WL_ERROR(("wl%d: %s, can not scan while driver is down\n",
			wlc->pub->unit, __FUNCTION__));
		status = WLC_E_STATUS_ERROR;
		ret = BCME_NOTUP;
		goto fail_with_cb;
	}

	wlc_scan_set_p2pparams(scan_info, ssid, nssid);

	/* Determine bandmask which will specify the bands to be scanned */
	scan_info->bandmask = wlc_scan_get_bandmask(scan_info, bandinfo);
	if (!scan_info->bandmask) {
		WL_ERROR(("wl%d: %s, can not determine band to scan (bandinfo=0x%x)\n",
			wlc->pub->unit, __FUNCTION__, bandinfo));
		status = WLC_E_STATUS_ERROR;
		ret = BCME_BADARG;
		goto fail_with_cb;
	}
#if BAND6G
	wlc_scan_determine_6g_params(scan_info, n_channels, scan_flags, bssid);
#endif /* BAND6G */

	scan_info->scan_pub->state &= ~SCAN_STATE_PROHIBIT;
	if (scan_flags & WL_SCANFLAGS_PROHIBITED) {
		scan_info->scan_pub->state |= SCAN_STATE_PROHIBIT;
	}
	if (n_channels) {
		ret = wlc_scan_set_chanparams(scan_info, chanspec_list, n_channels);
		/* DFS can block all channels */
		if ((!ret) && (scan_info->channel_num == 0)) {
			ret = BCME_BADCHAN;
		}
		if (ret) {
			status = WLC_E_STATUS_NOCHANS;
			goto fail_with_cb;
		}
	} else {
		if (!wlc_scan_info->iscan_cont) {
			wlc_scan_get_default_channels(scan_info);
		}

		if (scan_info->scan_pub->state & SCAN_STATE_PROHIBIT) {
			num = wlc_scan_prohibited_channels(scan_info,
				&scan_info->chanspec_list[scan_info->channel_num],
				(SCAN_MAX_CHANSPECS - scan_info->channel_num));
			scan_info->channel_num += num;
		}
	}

#ifdef WLMSG_ROAM
	bcopy(&ssid->SSID, SSIDbuf, ssid->SSID_len);
	SSIDbuf[ssid->SSID_len] = 0;
	WL_ROAM(("SCAN for '%s' %d SSID(s) %d channels\n", SSIDbuf, nssid, scan_info->channel_num));
#endif /* WLMSG_ROAM */

#ifdef BCMDBG
	if (WL_INFORM_ON()) {
		char chan_list_buf[128];
		struct bcmstrbuf b;

		bcm_binit(&b, chan_list_buf, sizeof(chan_list_buf));

		for (i = 0; i < scan_info->channel_num; i++) {
			bcm_bprintf(&b, " %s",
				wf_chspec_ntoa_ex(scan_info->chanspec_list[i], chanbuf));

			if ((i % 8) == 7 || (i + 1) == scan_info->channel_num) {
				WL_INFORM_SCAN(("wl%d: wlc_scan: scan channels %s\n",
					scan_info->unit, chan_list_buf));
				bcm_binit(&b, chan_list_buf, sizeof(chan_list_buf));
			}
		}
	}
#endif /* BCMDBG */

	if ((wlc_scan_info->state & SCAN_STATE_SUPPRESS) || (!scan_info->channel_num)) {
		WL_INFORM_SCAN(("wl%d: %s: scan->state %d scan->channel_num %d\n",
			scan_info->unit, __FUNCTION__,
			wlc_scan_info->state, scan_info->channel_num));

		if (wlc_scan_info->state & SCAN_STATE_SUPPRESS)
			status = WLC_E_STATUS_SUPPRESS;
		else
			status = WLC_E_STATUS_NOCHANS;

		if (scan_in_progress)
			wlc_scan_abort(wlc_scan_info, status);

		/* no scan now, but free any earlier leftovers */
		wlc_bss_list_free(wlc, wlc->scan_results);
		wlc_scan_ssidlist_free(scan_info);
		ret = BCME_EPERM;
		goto fail_with_cb;
	}

	/* extd scan for nssids one ssid per each pass..  */
	scan_info->npasses = scan_info->nprobes;
	scan_info->channel_idx = 0;
	if (chanspec_start != 0) {
		for (i = 0; i < scan_info->channel_num; i++) {
			if (scan_info->chanspec_list[i] == chanspec_start) {
				scan_info->channel_idx = i;
				WL_INFORM_SCAN(("starting new iscan on chanspec %s\n",
				           wf_chspec_ntoa_ex(chanspec_start, chanbuf)));
				break;
			}
		}
	}
#if defined(WLDFS)
	/* If we are switching away from radar home_chanspec
	 * because STA scans (normal/Join/Roam) with
	 * atleast one local 11H AP in radar channel,
	 * turn of radar_detect.
	 * NOTE: Implied that upstream AP assures this radar
	 * channel is clear.
	 */
	if (WLDFS_ENAB(wlc->pub) && WL11H_ENAB(wlc) && scan_info->bsscfg != NULL &&
		scan_info->bsscfg->up && wlc_radar_chanspec(wlc->cmi, wlc->home_chanspec) &&
		wlc->dfs != NULL) {
		WL_REGULATORY(("wl%d: %s Moving from home channel dfs OFF\n",
				wlc->pub->unit, __FUNCTION__));
		wlc_set_dfs_cacstate(wlc->dfs, OFF, scan_info->bsscfg);
	}
#endif /* defined(WLDFS) */

	wlc_scan_info->in_progress = TRUE;
#ifdef HEALTH_CHECK
	if (WL_HEALTH_CHECK_ENAB(wlc->pub->cmn)) {
		/* Re-initing the scan in progress counter */
		scan_info->in_progress_duration = 0;
	}
#endif /* HEALTH_CHECK */
#ifdef WLOLPC
	/* if on olpc chan notify - if needed, terminate active cal; go off channel */
	if (OLPC_ENAB(wlc)) {
		wlc_olpc_eng_hdl_chan_update(wlc->olpc_info);
	}
#endif /* WLOLPC */
	wlc_phy_hold_upd(WLC_PI(wlc), PHY_HOLD_FOR_SCAN, TRUE);

#if defined(STA)
	wlc_scan_info->scan_start_time = OSL_SYSUPTIME();
#endif /* STA */

	/* ...and free any leftover responses from before */
	wlc_bss_list_free(wlc, wlc->scan_results);

	/* keep core awake to receive solicited probe responses, SCAN_IN_PROGRESS is TRUE */
	ASSERT(STAY_AWAKE(wlc));
	wlc_set_wake_ctrl(wlc);

#ifdef WLCHANIM
	if (WLC_CHANIM_ENAB(wlc->pub) && !BSSCFG_STA(wlc->primary_bsscfg) &&
		!wlc->primary_bsscfg->up) {

		wlc_suspend_mac_and_wait(wlc);
		wlc_lq_chanim_acc_reset(wlc);
		if ((err = wlc_lq_chanim_update(wlc, wlc->chanspec, CHANIM_CHANSPEC))
			!= BCME_OK) {
			WL_ERROR(("wl%d: %s: CHANIM upd fail %d\n", wlc->pub->unit,
				__FUNCTION__, err));
		}
		wlc_enable_mac(wlc);
	}
#endif // endif

#ifdef WLSCAN_PS
	if (WLSCAN_PS_ENAB(wlc->pub)) {
		/* Support for MIMO force depends on underlying WLSCAN_PS code */
		if (scan_flags & WL_SCANFLAGS_MIMO)
			scan_info->mimo_override = TRUE;
		else
			scan_info->mimo_override = FALSE;
	}
#endif /* WLSCAN_PS */

	/* Only start timer if no beacon delay is requested.  Otherwise, the timer will
	 * be started from the interrupt indicating that all beacons have been sent.
	 */
	{
		if ((ret = wlc_scan_schd_channels(wlc_scan_info)) != BCME_OK) {
			/* call wlc_scantimer to get the scan state machine going */
			/* DUALBAND - Don't call wlc_scantimer() directly from DPC... */
			/* send out AF as soon as possible to aid reliability of GON */
			wlc_scan_abort(wlc_scan_info, WLC_SCAN_STATE_ABORT);
			goto fail;
		}
	}

#ifdef BCMECICOEX
	/* disable long duration BLE Scans before starting scans */
	wlc_btc_ble_grants_enable(wlc->btch, FALSE);
#endif // endif

	/* if a scan is in progress, allow the next callback to restart the scan state machine */
	ASSERT(ret == BCME_OK);
	return ret;

fail_with_cb:
	if (scan_info->cb != NULL) {
		(scan_info->cb)(scan_info->cb_arg, status, scan_info->bsscfg);
	}

fail:
#ifdef STA
	/* Update the mpc state  to bring the core down */
	wlc->mpc_scan = FALSE;
	wlc_radio_mpc_upd(wlc);
	wlc_radio_upd(wlc);
#endif /* STA */
	return ret;
} /* _wlc_scan */

/** Return 6G or OCE Probe Request Deferral time (WFA OCE spec v5 3.2) */
static uint8
wlc_scan_get_probe_defer_time(scan_info_t *scan_info, chanspec_t chanspec)
{
#ifdef WL_OCE
	wlc_info_t *wlc = scan_info->wlc;

	if (!CHSPEC_IS6G(chanspec)) {
		return wlc_oce_get_probe_defer_time(wlc->oce);
	}
#endif // endif
	return SCAN_6G_PROBE_DEFER_TIME;
}

/** Should current channel use OCE probe deferral */
static bool
wlc_scan_use_probe_defer(scan_info_t *scan_info, chanspec_t chanspec)
{
	if (CHSPEC_IS6G(chanspec)) {
		return FALSE;
	} else if (OCE_ENAB(scan_info->wlc->pub)) {
		return TRUE;
	}
	return FALSE;
}
/** Should current channel use passive or active scan */
static bool
wlc_scan_use_passive_scan(scan_info_t *scan_info, chanspec_t chanspec)
{
	wlc_info_t *wlc = scan_info->wlc;

	if (scan_info->scan_pub->state & SCAN_STATE_PASSIVE) {
		return TRUE;
	}

	if (!wlc_valid_chanspec_db(wlc->cmi, chanspec) ||
	     wlc_quiet_chanspec(wlc->cmi, chanspec)) {
		return TRUE;
	}

#if BAND6G
	if (CHSPEC_IS6G(chanspec) &&
		(scan_info->probing_6g == SCAN_6G_PASSIVE_PROBING)) {
		return TRUE;
	}
#endif /* BAND6G */
	return FALSE;
}

/** returns time in [ms] that it takes to scan the next channel */
static uint32
wlc_get_next_chan_scan_time(scan_info_t *scan_info)
{
	chanspec_t next_chanspec;
	uint32 channel_dwelltime;

	if (scan_info->channel_idx < scan_info->channel_num) {
		next_chanspec = scan_info->chanspec_list[scan_info->channel_idx];
	} else {
		return SCAN_INVALID_DWELL_TIME;
	}

	if (wlc_scan_use_passive_scan(scan_info, next_chanspec)) {
		return scan_info->passive_time;
	}
#if BAND6G
	if (CHSPEC_IS6G(next_chanspec)) {
		if (scan_info->probing_6g == SCAN_6G_ACTIVE_PROBING) {
			channel_dwelltime = WLC_SCAN_6G_ACTIVE_TIME;
			return channel_dwelltime;
		}
	}
#endif /* BAND6G */
	channel_dwelltime = scan_info->active_time;

	if (wlc_scan_use_probe_defer(scan_info, next_chanspec)) {
		channel_dwelltime += wlc_scan_get_probe_defer_time(scan_info, next_chanspec);
	}

	return channel_dwelltime;
}

static int
wlc_scan_schd_channels(wlc_scan_info_t *wlc_scan_info)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
	wlc_info_t *wlc = scan_info->wlc;
	uint32 ch_scan_time;
	uint32 chn_cnt;
	int chn_start;
	wlc_msch_req_param_t req_param;
	int err;

	/*
	* We need to account for invalide dwell times of 0, which could be returned if
	* we're doing an active scan and the caller has specified passive dwell time of 0
	* to avoid scanning quiet channels. In that case, we must reject the current channel
	* and see if we can find another channel that can be scanned.
	*/
	ch_scan_time = wlc_get_next_chan_scan_time(scan_info);
	if (ch_scan_time == SCAN_INVALID_DWELL_TIME) {
		while (++scan_info->channel_idx < scan_info->channel_num) {
			ch_scan_time = wlc_get_next_chan_scan_time(scan_info);
			if (ch_scan_time != SCAN_INVALID_DWELL_TIME) {
				break;
			}
		}

		if (scan_info->channel_idx >= scan_info->channel_num) {
			return BCME_SCANREJECT;
		}
	}

	chn_start = scan_info->channel_idx;

	/* We've got a channel and a target dwell time. So, get the other channels with the
	 * same dwell time.
	 */
	chn_cnt = 1;
	ch_scan_time = wlc_get_next_chan_scan_time(scan_info);
	while (++scan_info->channel_idx < scan_info->channel_num) {
		if ((wlc_get_next_chan_scan_time(scan_info) != ch_scan_time) ||
#if BAND6G
			(wlc_scan_find_6g_chan_start(scan_info)) ||
#endif /* BAND6G */
			FALSE) {
			break;
		}
		chn_cnt++;
	}
	scan_info->cur_scan_chanspec = 0;
	memset(&req_param, 0, sizeof(req_param));
	req_param.duration = MS_TO_USEC(ch_scan_time);
	req_param.req_type = MSCH_RT_START_FLEX;
	req_param.priority = MSCH_RP_CONNECTION;
	if ((err = wlc_msch_timeslot_register(wlc->msch_info,
		&scan_info->chanspec_list[chn_start],
		chn_cnt, wlc_scan_chnsw_clbk, wlc_scan_info, &req_param,
		&scan_info->msch_req_hdl)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: MSCH timeslot register failed, err %d\n",
			scan_info->unit, __FUNCTION__, err));
		return err;
	}
	return wlc_msch_set_chansw_reason(wlc->msch_info, scan_info->msch_req_hdl, CHANSW_SCAN);
} /* wlc_scan_schd_channels */

/** Called back by the Multichannel (msch) scheduler */
static int
wlc_scan_chnsw_clbk(void* handler_ctxt, wlc_msch_cb_info_t *cb_info)
{
	wlc_scan_info_t *wlc_scan_info = (wlc_scan_info_t *)handler_ctxt;
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
	wlc_info_t *wlc = scan_info->wlc;
	uint32 type = cb_info->type;
	uint32  dummy_tsf_h, start_tsf;
	char chanbuf[CHANSPEC_STR_LEN];

	BCM_REFERENCE(chanbuf);

	if (scan_info->msch_req_hdl == NULL) {
		/* The requeset is been cancelled, ignore the Clbk */
		return BCME_OK;
	}

	if (type & MSCH_CT_ON_CHAN) {
		/* At this point the FIFOs should be suspended */
		wlc_scan_info->state |= SCAN_STATE_READY;

		wlc_scan_info->flag |= SCAN_MSCH_ONCHAN_CALLBACK;

		wlc_txqueue_start(wlc, NULL, cb_info->chanspec);

		if (!(wlc_scan_info->flag & SCAN_MSCH_ONCHAN_CALLBACK)) {
			WL_SCAN(("wl%d: %s scan finished when in CB.\n",
				scan_info->unit, __FUNCTION__));
			wlc_txqueue_end(wlc, NULL);
			return BCME_OK;
		} else {
			wlc_scan_info->flag &= ~SCAN_MSCH_ONCHAN_CALLBACK;
		}
	}

	if (type & MSCH_CT_SLOT_START) {
		wlc_msch_onchan_info_t *onchan =
			(wlc_msch_onchan_info_t *)cb_info->type_specific;
		scan_info->cur_scan_chanspec = cb_info->chanspec;
#ifdef STA
		wlc_scan_utils_set_chanspec(wlc, scan_info->cur_scan_chanspec);
#endif // endif
		/* delete any pending timer */
		WLC_SCAN_DEL_TIMER(scan_info);

		/* disable CFP & TSF update */
		wlc_mhf(wlc, MHF2, MHF2_SKIP_CFP_UPDATE,
			MHF2_SKIP_CFP_UPDATE, WLC_BAND_ALL);
		wlc_skip_adjtsf(wlc, TRUE, NULL, WLC_SKIP_ADJTSF_SCAN,
			WLC_BAND_ALL);

		wlc_mac_bcn_promisc_update(wlc, BCNMISC_SCAN, TRUE);
		wlc_read_tsf(wlc, &start_tsf, &dummy_tsf_h);
		scan_info->start_tsf = start_tsf;

		scan_info->timeslot_id = onchan->timeslot_id;
		if (wlc_scan_use_passive_scan(scan_info, scan_info->cur_scan_chanspec)) {
			/* PASSIVE SCAN */
			scan_info->state = WLC_SCAN_STATE_LISTEN;

			WL_SCAN(("wl%d: passive dwell time %d ms, chanspec %s, tsf %u\n",
				scan_info->unit, scan_info->passive_time,
				wf_chspec_ntoa_ex(scan_info->cur_scan_chanspec, chanbuf),
				start_tsf));
			return BCME_OK;
		}
#if BAND6G
		if (CHSPEC_IS6G(scan_info->cur_scan_chanspec)) {
			wlc_scan_6g_chan_handle(scan_info);
			return BCME_OK;
		}
#endif /* BAND6G */

		/* Else active scan so fire off wlc_scantimer() to get it going */
		scan_info->state = WLC_SCAN_STATE_START;
		WLC_SCAN_ADD_TIMER(scan_info, 0, 0);
		goto exit;
	}

	if (type & MSCH_CT_SLOT_END) {
		if (scan_info->state != WLC_SCAN_STATE_SEND_PROBE &&
			scan_info->state != WLC_SCAN_STATE_LISTEN) {
			WL_SCAN(("wl%d: %s: wrong scan state %d \n",
				scan_info->unit, __FUNCTION__, scan_info->state));
		}

		scan_info->state = WLC_SCAN_STATE_CHN_SCAN_DONE;
		scan_info->timeslot_id = 0;
#if BAND6G
		scan_info->state_6g_chan = SCAN_6G_CHAN_NONE;
		scan_info->prb_idle_on_6g_chan = FALSE;
#endif /* BAND6G */

		/* scan passes complete for the current channel */
		WL_SCAN(("wl%d: %s: %sscanned channel %d, total responses %d, tsf %u\n",
			scan_info->unit, __FUNCTION__,
			((wlc_quiet_chanspec(wlc->cmi, cb_info->chanspec) &&
			!(wlc_scan_info->state & SCAN_STATE_RADAR_CLEAR)) ? "passively ":""),
			CHSPEC_CHANNEL(cb_info->chanspec),
			wlc->scan_results->count,
			SCAN_GET_TSF_TIMERLOW(wlc)));

		/* reset the radar clear flag since we will be leaving the channel */
		wlc_scan_info->state &= ~SCAN_STATE_RADAR_CLEAR;

		/* resume normal tx queue operation */
		wlc_scan_excursion_end(scan_info);

	}

	if (type & MSCH_CT_REQ_END) {
		/* delete any pending timer */
		WLC_SCAN_DEL_TIMER(scan_info);

		/* clear msch request handle */
		scan_info->msch_req_hdl = NULL;

		if (scan_info->channel_idx < scan_info->channel_num) {
#if BAND6G
			/* At the end of NON 6G scan, build the 6G chanspec list from the RNR
			 * channel profile built from 2g/5g scan
			 */
			uint8 channel_idx = scan_info->channel_idx;
			if (CHSPEC_IS6G(scan_info->chanspec_list[channel_idx])) {
				wlc_scan_6g_prepare_channel_list(wlc_scan_info);
			}
#endif // endif
			scan_info->state = WLC_SCAN_STATE_PARTIAL;
			WLC_SCAN_ADD_TIMER(scan_info, 0, 0);
		}
		else {
			scan_info->state = WLC_SCAN_STATE_COMPLETE;
			WLC_SCAN_ADD_TIMER(scan_info, 0, 0);
#ifdef CCA_STATS
			/* send CCA event for the last scanned channel */
			if (WL_CCA_STATS_ENAB(wlc->pub)) {
				cca_stats_upd(wlc, 1);
				cca_send_event(wlc, 1);
			}
#endif /* CCA_STATS */
			wlc_phy_hold_upd(WLC_PI(wlc), PHY_HOLD_FOR_SCAN, FALSE);
		}
	}

exit:
	return BCME_OK;
} /* wlc_scan_chnsw_clbk */

void
wlc_scan_timer_update(wlc_scan_info_t *wlc_scan_info, uint32 ms)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;

	WLC_SCAN_DEL_TIMER(scan_info);
	WLC_SCAN_ADD_TIMER(scan_info, ms, 0);
}

/** return number of channels in current scan */
int
wlc_scan_chnum(wlc_scan_info_t *wlc_scan_info)
{
	return ((scan_info_t *)wlc_scan_info->scan_priv)->channel_num;
}

#if defined(STA)
/** STA specific */
uint32
wlc_curr_roam_scan_time(wlc_info_t *wlc)
{
	uint32 curr_roam_scan_time = 0;
	/* Calculate awake time due to active roam scan */
	if (SCAN_IN_PROGRESS(wlc->scan) && (AS_IN_PROGRESS(wlc))) {
		wlc_bsscfg_t *cfg = AS_IN_PROGRESS_CFG(wlc);
		wlc_assoc_t *as = cfg->assoc;
		if (as->type == AS_ROAM)
			curr_roam_scan_time = wlc_get_curr_scan_time(wlc);
	}
	return curr_roam_scan_time;
}

/** STA specific */
uint32
wlc_get_curr_scan_time(wlc_info_t *wlc)
{
	wlc_scan_info_t *wlc_scan_info = wlc->scan;

	if (SCAN_IN_PROGRESS(wlc_scan_info)) {
		return (OSL_SYSUPTIME() - wlc_scan_info->scan_start_time);
	}

	return 0;
}

/** STA specific. Called by e.g. wlc_scantimer() */
static void
wlc_scan_time_upd(wlc_info_t *wlc, scan_info_t *scan_info)
{
	wlc_scan_info_t *scan_pub = scan_info->scan_pub;
	uint32 scan_dur;

	scan_dur = wlc_get_curr_scan_time(wlc);

	/* Record scan end */
	scan_pub->scan_stop_time = OSL_SYSUPTIME();

	/* For power stats, accumulate duration in the appropriate bucket */
	if (PWRSTATS_ENAB(wlc->pub)) {
		scan_data_t *scan_data = NULL;

		/* Accumlate this scan in the appropriate pwr_stats bucket */
		if (FALSE);
#ifdef WLPFN
#ifdef WL_EXCESS_PMWAKE
		else if (WLPFN_ENAB(wlc->pub) && wl_pfn_scan_in_progress(wlc->pfn)) {

			wlc_excess_pm_wake_info_t *epmwake = wlc->excess_pmwake;
			scan_data = &scan_info->scan_stats->pno_scans[0];

			epmwake->pfn_scan_ms += scan_dur;

			if (epmwake->pfn_alert_enable && ((epmwake->pfn_scan_ms -
				epmwake->pfn_alert_thresh_ts) >	epmwake->pfn_alert_thresh)) {
				wlc_generate_pm_alert_event(wlc, PFN_ALERT_THRESH_EXCEEDED,
					NULL, 0);
				/* Disable further events */
				epmwake->pfn_alert_enable = FALSE;
			}
		}
#endif /* WL_EXCESS_PMWAKE */
#endif /* WLPFN */
		else if (AS_IN_PROGRESS(wlc)) {
			wlc_bsscfg_t *cfg = AS_IN_PROGRESS_CFG(wlc);
			wlc_assoc_t *as = cfg->assoc;

			if (as->type == AS_ROAM) {
				scan_data = &scan_info->scan_stats->roam_scans;
#ifdef WL_EXCESS_PMWAKE
				wlc->excess_pmwake->roam_ms += scan_dur;
				wlc_check_roam_alert_thresh(wlc);
#endif /* WL_EXCESS_PMWAKE */
			}
			else
				scan_data = &scan_info->scan_stats->assoc_scans;
		}
		else
			scan_data = &scan_info->scan_stats->user_scans;

		if (scan_data) {
			scan_data->count++;
			/* roam scan in ms */
			if (scan_data == &scan_info->scan_stats->roam_scans)
				scan_data->dur += scan_dur;
			else
				scan_data->dur += (scan_dur * 1000); /* Scale to usec */
		}
	}

	return;
} /* wlc_scan_time_upd */

/** STA specific */
int
wlc_pwrstats_get_scan(wlc_scan_info_t *scan, uint8 *destptr, int destlen)
{
	scan_info_t *scani = (scan_info_t *)scan->scan_priv;
	wl_pwr_scan_stats_t *scan_stats = scani->scan_stats;
	uint16 taglen = sizeof(wl_pwr_scan_stats_t);

	/* Make sure there's room for this section */
	if (destlen < (int)ROUNDUP(sizeof(wl_pwr_scan_stats_t), sizeof(uint32)))
		return BCME_BUFTOOSHORT;

	/* Update common structure fields and copy into the destination */
	scan_stats->type = WL_PWRSTATS_TYPE_SCAN;
	scan_stats->len = taglen;
	memcpy(destptr, scan_stats, taglen);

	/* Report use of this segment (plus padding) */
	return (ROUNDUP(taglen, sizeof(uint32)));
}

#endif /* STA */

/** Abort the current scan, and return to home channel */
void
wlc_scan_abort(wlc_scan_info_t *wlc_scan_info, int status)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
	wlc_bsscfg_t *scan_cfg = scan_info->bsscfg;
	wlc_info_t *wlc = scan_info->wlc;

	BCM_REFERENCE(scan_cfg);

#if defined(WLDFS) && defined(BGDFS)
	if (BGDFS_ENAB(wlc->pub) && wlc_dfs_scan_in_progress(wlc->dfs)) {
		wlc_dfs_scan_abort(wlc->dfs);
	}
#endif /* WLDFS && BGDFS */

	if (!SCAN_IN_PROGRESS(wlc_scan_info)) {
		return;
	}

	WL_INFORM_SCAN(("wl%d: %s: aborting scan in progress\n", scan_info->unit, __FUNCTION__));
#ifdef WLRCC
	if ((WLRCC_ENAB(wlc->pub)) && scan_cfg &&
		scan_info->bsscfg->roam &&
		(scan_info->bsscfg->roam->n_rcc_channels > 0))
		scan_info->bsscfg->roam->rcc_valid = TRUE;
#endif // endif
	if (SCANCACHE_ENAB(wlc_scan_info) &&
#ifdef WLP2P
	    (scan_cfg && !BSS_P2P_DISC_ENAB(wlc, scan_cfg)) &&
#endif // endif
		TRUE) {
		wlc_scan_cache_result(scan_info);
	}

	wlc_bss_list_free(wlc, wlc->scan_results);
	wlc_scan_terminate(wlc_scan_info, status);

	wlc_ht_obss_scan_update(scan_info, WLC_E_STATUS_ABORT);
} /* wlc_scan_abort */

void
wlc_scan_abort_ex(wlc_scan_info_t *wlc_scan_info, wlc_bsscfg_t *cfg, int status)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
	if (scan_info->bsscfg == cfg)
		wlc_scan_abort(wlc_scan_info, status);
}

/**
 * wlc_scan_terminate is called when user (from intr/dpc or ioctl) requests to
 * terminate the scan. However it may also be invoked from its own call chain
 * from which tx status processing is executed. Use the flag TERMINATE to prevent
 * it from being re-entered.
 *
 * Driver's primeter lock will prevent wlc_scan_terminate from being invoked from
 * intr/dpc or ioctl when a timer callback is running. However it may be invoked
 * from the wlc_scantimer call chain in which tx status processing is executed.
 * So use the flag IN_TMR_CB to prevent wlc_scan_terminate being re-entered.
 */
void
wlc_scan_terminate(wlc_scan_info_t *wlc_scan_info, int status)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
	wlc_info_t *wlc = scan_info->wlc;
	int err;
#ifdef WL_UCM
	wlc_bsscfg_t *ucm_bsscfg = NULL;
#endif /* WL_UCM */

	if (!SCAN_IN_PROGRESS(wlc_scan_info))
		return;

	/* ignore if already in terminate/finish process */
	if (wlc_scan_info->state & SCAN_STATE_TERMINATE) {
		WL_SCAN(("wl%d: %s: ignore request\n", scan_info->unit, __FUNCTION__));
		return;
	}

	/* protect wlc_scan_terminate being recursively called from the
	 * callchain
	 */
	wlc_scan_info->state |= SCAN_STATE_TERMINATE;

	/* defer the termination if called from the timer callback */
	if (wlc_scan_info->state & SCAN_STATE_IN_TMR_CB) {
		WL_SCAN(("wl%d: %s: defer request\n",
		         scan_info->unit, __FUNCTION__));
		scan_info->status = status;
		return;
	}

	/* abort the current scan, and return to home channel */
	WL_INFORM_SCAN(("wl%d: %s: terminating scan in progress\n", scan_info->unit, __FUNCTION__));

#ifdef WLSCAN_PS
	/* scan is done. now reset the cores */
	if (WLSCAN_PS_ENAB(wlc->pub))
		wlc_scan_ps_config_cores(scan_info, FALSE);
#endif // endif

#if defined(WLDFS)
	/* If we are switching back to radar home_chanspec
	 * because:
	 * 1. STA scans (normal/Join/Roam) aborted with
	 * atleast one local 11H AP in radar channel,
	 * 2. Scan is not join/roam.
	 * turn radar_detect ON.
	 * NOTE: For Join/Roam radar_detect ON is done
	 * at later point in wlc_roam_complete() or
	 * wlc_set_ssid_complete(), when STA succesfully
	 * associates to upstream AP.
	 */
	if (WLDFS_ENAB(wlc->pub) && WL11H_AP_ENAB(wlc) && scan_info->bsscfg != NULL &&
			BSSCFG_AP(scan_info->bsscfg) && scan_info->bsscfg->up &&
			WLC_APSTA_ON_RADAR_CHANNEL(wlc) && wlc->dfs != NULL &&
#ifdef STA
			!AS_IN_PROGRESS(wlc) &&
#endif // endif
			wlc_radar_chanspec(wlc->cmi, wlc->home_chanspec)) {
		WL_REGULATORY(("wl%d: %s Join/scan aborted back"
			"to home channel dfs ON\n",
			wlc->pub->unit, __FUNCTION__));
		wlc_set_dfs_cacstate(wlc->dfs, ON, scan_info->bsscfg);
	}
#endif /* defined(WLDFS) */

	/* clear scan ready flag */
	wlc_scan_info->state &= ~SCAN_STATE_READY;

	/* Cancel the MSCH request */
	if (scan_info->msch_req_hdl) {
		if ((err = wlc_msch_timeslot_unregister(wlc->msch_info,
			&scan_info->msch_req_hdl)) != BCME_OK) {
			WL_SCAN_ERROR(("wl%d: %s: MSCH timeslot unregister failed, err %d\n",
			         scan_info->unit, __FUNCTION__, err));
		}

		/* resume normal tx queue operation */
		wlc_scan_excursion_end(scan_info);

		scan_info->msch_req_hdl = NULL;
	}

	scan_info->state = WLC_SCAN_STATE_ABORT;
	scan_info->channel_idx = -1;
#if defined(STA)
	wlc_scan_time_upd(wlc, scan_info);
#endif /* STA */
	wlc_scan_info->in_progress = FALSE;
#ifdef HEALTH_CHECK
	if (WL_HEALTH_CHECK_ENAB(wlc->pub->cmn)) {
		/* Resetting the scan in progress counter to 0 */
		scan_info->in_progress_duration = 0;
	}
#endif /* HEALTH_CHECK */
#ifdef WLBTCPROF
	wlc_btcx_set_btc_profile_param(wlc, wlc->home_chanspec, TRUE);
#endif // endif
#ifdef WL_UCM
	if (UCM_ENAB(wlc->pub)) {
		wlc_btc_get_matched_chanspec_bsscfg(wlc,
			wlc->home_chanspec, &ucm_bsscfg);
		if (wlc_btc_apply_profile(wlc, ucm_bsscfg) == BCME_ERROR) {
			WL_ERROR(("wl%d: %s: BTCOEX settings error: chspec %d!\n",
				wlc->pub->unit, __FUNCTION__,
				CHSPEC_CHANNEL(wlc->home_chanspec)));
			ASSERT(0);
		}
	}
#endif /* WL_UCM */

	wlc_phy_hold_upd(WLC_PI(wlc), PHY_HOLD_FOR_SCAN, FALSE);
#ifdef WLOLPC
	/* notify scan just terminated - if needed, kick off new cal */
	if (OLPC_ENAB(wlc)) {
		wlc_olpc_eng_hdl_chan_update(wlc->olpc_info);
	}
#endif /* WLOLPC */

	wlc_scan_ssidlist_free(scan_info);

#ifdef STA
	wlc_set_wake_ctrl(wlc);
	WL_MPC(("wl%d: %s: SCAN_IN_PROGRESS==FALSE, update mpc\n", scan_info->unit, __FUNCTION__));
	wlc->mpc_scan = FALSE;
	wlc_radio_mpc_upd(wlc);
#endif /* STA */

	/* abort PM indication process */
	wlc_scan_info->state &= ~SCAN_STATE_PSPEND;
	/* abort tx suspend delay process */
	wlc_scan_info->state &= ~SCAN_STATE_DLY_WSUSPEND;
	/* abort TX FIFO suspend process */
	wlc_scan_info->state &= ~SCAN_STATE_WSUSPEND;
	/* delete a future timer explictly */
	WLC_SCAN_DEL_TIMER(scan_info);
#ifdef STA
#ifdef WL11D
	/* If we are in 802.11D mode and we are still waiting to find a
	 * valid Country IE, take this opportunity to parse scan results,
	 * if necessary, and try to apply it.
	 */
	if (WLC_AUTOCOUNTRY_ENAB(wlc))
		wlc_11d_scan_complete(wlc->m11d, status);
#endif /* WL11D */
	wlc_scan_callback(scan_info, status);
#endif /* STA */

	scan_info->state = WLC_SCAN_STATE_START;

	wlc_scan_info->state &= ~SCAN_STATE_TERMINATE;
} /* wlc_scan_terminate */

/** Callback of scan->timer */
static void
wlc_scantimer(void *arg)
{
	scan_info_t *scan_info = (scan_info_t *)arg;
	wlc_scan_info_t	*wlc_scan_info = scan_info->scan_pub;
	wlc_info_t *wlc = scan_info->wlc;
	int status = WLC_E_STATUS_SUCCESS;
	/* char chanbuf[CHANSPEC_STR_LEN]; */
#ifdef WL_UCM
	wlc_bsscfg_t *ucm_bsscfg = NULL;
#endif /* WL_UCM */
	wlc_bsscfg_t *cfg;
	bool bss_up = FALSE;
	int i;

	WL_SCAN_ENT(scan_info, ("wl%d: %s: enter, state 0x%x tsf %u\n",
	                        scan_info->unit, __FUNCTION__,
	                        wlc_scan_info->state, SCAN_GET_TSF_TIMERLOW(wlc)));

	wlc_scan_info->state |= SCAN_STATE_IN_TMR_CB;

	if (DEVICEREMOVED(wlc)) {
		WL_ERROR(("wl%d: %s: dead chip\n", scan_info->unit, __FUNCTION__));
		if (SCAN_IN_PROGRESS(wlc_scan_info)) {
			wlc_bss_list_free(wlc, wlc->scan_results);
			wlc_scan_callback(scan_info, WLC_E_STATUS_ABORT);

		}
		wl_down(wlc->wl);
		goto exit;
	}

	if (scan_info->state == WLC_SCAN_STATE_PARTIAL) {
		if (wlc_scan_schd_channels(wlc_scan_info) == BCME_OK) {
			goto exit;
		}

		WL_SCAN_ERROR(("wl%d: %s: Error, failed to scan %d channels\n",
			scan_info->unit, __FUNCTION__,
			(scan_info->channel_num - scan_info->channel_idx)));

		scan_info->state = WLC_SCAN_STATE_COMPLETE;
	}

	if (scan_info->state == WLC_SCAN_STATE_ABORT) {
		WL_SCAN_ENT(scan_info, ("wl%d: %s: move state to START\n",
		                        scan_info->unit, __FUNCTION__));
		scan_info->state = WLC_SCAN_STATE_START;
		goto exit;
	}
#if BAND6G
	if (scan_info->state == WLC_SCAN_STATE_LISTEN &&
			CHSPEC_IS6G(scan_info->cur_scan_chanspec)) {
		wlc_scan_6g_handle_chan_states(scan_info);
	}
#endif /* BAND6G */

	if (scan_info->state == WLC_SCAN_STATE_START) {
		scan_info->state = WLC_SCAN_STATE_SEND_PROBE;
		scan_info->pass = scan_info->npasses;
		scan_info->defer_probe = FALSE;
		if (!wlc_scan_use_passive_scan(scan_info, scan_info->cur_scan_chanspec) &&
			wlc_scan_use_probe_defer(scan_info, scan_info->cur_scan_chanspec)) {

			if (scan_info->pass > 0) {
				scan_info->defer_probe = TRUE;
			}
		}
	}
	if (scan_info->state == WLC_SCAN_STATE_SEND_PROBE) {
		if (scan_info->pass > 0) {
			if (scan_info->defer_probe == TRUE) {
				scan_info->defer_probe = FALSE;
				wlc_scan_info->state |= SCAN_STATE_PRB_DEFERRED;
#ifdef WL_OCE
				if (!CHSPEC_IS6G(scan_info->cur_scan_chanspec)) {
					wlc_oce_flush_prb_suppress_bss_list(wlc->oce);
				}
#endif /* WL_OCE */
				WL_SCAN(("Enter probe deferral state\n"));
				WLC_SCAN_ADD_TIMER(scan_info,
					wlc_scan_get_probe_defer_time(scan_info,
						scan_info->cur_scan_chanspec), 0);
				goto exit;
			}
#ifdef WLSCAN_PS
			/* scan started, switch to one tx/rx core */
			if (WLSCAN_PS_ENAB(wlc->pub))
				wlc_scan_ps_config_cores(scan_info, TRUE);
#endif /* WLSCAN_PS */
			wlc_scan_do_pass(scan_info, scan_info->cur_scan_chanspec);
			scan_info->pass--;

			goto exit;
		} else {
			if (scan_info->timeslot_id) {
				if (BCME_OK != wlc_msch_timeslot_cancel(wlc->msch_info,
					scan_info->msch_req_hdl, scan_info->timeslot_id))
					goto exit;

				scan_info->timeslot_id = 0;

				/* reset radar clear flag since we are leaving this channel */
				wlc_scan_info->state &= ~SCAN_STATE_RADAR_CLEAR;

				/* resume normal tx queue operation */
				wlc_scan_excursion_end(scan_info);

				if (scan_info->chanspec_list[scan_info->channel_idx - 1] !=
					scan_info->cur_scan_chanspec) {
					/* scan complete for current channel */
					scan_info->state = WLC_SCAN_STATE_CHN_SCAN_DONE;
				} else {
					/* scan complete for current request */
					/* clear msch request handle */
					scan_info->msch_req_hdl = NULL;

					if (scan_info->channel_idx < scan_info->channel_num) {
						scan_info->state = WLC_SCAN_STATE_PARTIAL;
						WLC_SCAN_ADD_TIMER(scan_info, 0, 0);
					}
					else {
						scan_info->state = WLC_SCAN_STATE_COMPLETE;
						WLC_SCAN_ADD_TIMER(scan_info, 0, 0);
					}
					goto exit;
				}
			}
		}
	}

	if (scan_info->state != WLC_SCAN_STATE_COMPLETE) {
		goto exit;
	}

	/* If all BSSs are down, we completely unregister from MSCH
	 * So we need to return back to home channel here
	 */
	FOREACH_BSS(wlc, i, cfg) {
		if (cfg->up) {
			bss_up = TRUE;
			break;
		}
	}
	if (!bss_up) {
		WL_SCAN(("%s: All BSS's down. Return to home channel 0x%04x\n",
			__FUNCTION__, wlc->home_chanspec));
		wlc_set_home_chanspec(wlc, wlc->home_chanspec);
		wlc_suspend_mac_and_wait(wlc);
		wlc_set_chanspec(wlc, wlc->home_chanspec, CHANSW_REASON(CHANSW_SCAN));
		wlc_enable_mac(wlc);
	}

	scan_info->state = WLC_SCAN_STATE_START;
	wlc_scan_info->state |= SCAN_STATE_TERMINATE;

#if defined(BCMDBG) || defined(WLMSG_INFORM)
	if (scan_info->nssid == 1) {
		char ssidbuf[SSID_FMT_BUF_LEN];
		wlc_ssid_t *ssid = scan_info->ssid_list;

		if (WL_INFORM_ON())
			wlc_format_ssid(ssidbuf, ssid->SSID, ssid->SSID_len);
		WL_INFORM_SCAN(("wl%d: %s: %s scan done, %d total responses for SSID \"%s\"\n",
		           scan_info->unit, __FUNCTION__,
		           (wlc_scan_info->state & SCAN_STATE_PASSIVE) ? "Passive" : "Active",
		           wlc->scan_results->count, ssidbuf));
	} else {
		WL_INFORM_SCAN(("wl%d: %s: %s scan done, %d total responses for SSIDs:\n",
		           scan_info->unit, __FUNCTION__,
		           (wlc_scan_info->state & SCAN_STATE_PASSIVE) ? "Passive" : "Active",
		           wlc->scan_results->count));
		if (WL_INFORM_ON())
			wlc_scan_print_ssids(scan_info->ssid_list, scan_info->nssid);
	}
#endif /* BCMDBG || WLMSG_INFORM */

#ifdef WLSCAN_PS
	/* scan is done, revert core mask */
	if (WLSCAN_PS_ENAB(wlc->pub))
		wlc_scan_ps_config_cores(scan_info, FALSE);
#endif // endif

	wlc_scan_info->state &= ~SCAN_STATE_READY;
	scan_info->channel_idx = -1;
#if defined(STA)
	wlc_scan_time_upd(wlc, scan_info);
#endif /* STA */
	wlc_scan_info->in_progress = FALSE;
#ifdef WL_SCAN_DFS_HOME
	/* On scan completion, get into ISM state if home channel is DFS */
	if (WL11H_AP_ENAB(wlc) && wlc_radar_chanspec(wlc->cmi, wlc->home_chanspec)) {
		wlc_set_dfs_cacstate(wlc->dfs, ON, scan_info->bsscfg);
	}
#endif /* WL_SCAN_DFS_HOME */
#ifdef HEALTH_CHECK
	if (WL_HEALTH_CHECK_ENAB(wlc->pub->cmn)) {
		/* Resetting the scan in progress counter as scan finished */
		scan_info->in_progress_duration = 0;
	}
#endif /* HEALTH_CHECK */
#ifdef WLBTCPROF
	wlc_btcx_set_btc_profile_param(wlc, wlc->home_chanspec, TRUE);
#endif // endif
	wlc_phy_hold_upd(WLC_PI(wlc), PHY_HOLD_FOR_SCAN, FALSE);
#ifdef WLOLPC
	/* notify scan just terminated - if needed, kick off new cal */
	if (OLPC_ENAB(wlc)) {
		wlc_olpc_eng_hdl_chan_update(wlc->olpc_info);
	}
#endif /* WLOLPC */

	wlc_scan_ssidlist_free(scan_info);

	/* allow core to sleep again (no more solicited probe responses) */
	wlc_set_wake_ctrl(wlc);

#ifdef WLCQ
	/* resume any channel quality measurement */
	wlc_lq_channel_qa_sample_req(wlc);
#endif /* WLCQ */

	wlc_scan_callback(scan_info, status);

#ifdef STA
	/* disable radio for non-association scan.
	 * Association scan will continue with JOIN process and
	 * end up at MACEVENT: WLC_E_SET_SSID
	 */
	WL_MPC(("wl%d: scan done, SCAN_IN_PROGRESS==FALSE, update mpc\n", scan_info->unit));
	wlc->mpc_scan = FALSE;
	wlc_radio_mpc_upd(wlc);
	wlc_radio_upd(wlc);	/* Bring down the radio immediately */
#endif /* STA */

	wlc_scan_info->state &= ~SCAN_STATE_TERMINATE;

#ifdef WL_UCM
	if (UCM_ENAB(wlc->pub)) {
		wlc_btc_get_matched_chanspec_bsscfg(wlc, wlc->home_chanspec, &ucm_bsscfg);
		if (wlc_btc_apply_profile(wlc, ucm_bsscfg) == BCME_ERROR) {
			WL_ERROR(("wl%d: %s: BTCOEX settings error: chspec %d!\n",
				wlc->pub->unit, __FUNCTION__,
				CHSPEC_CHANNEL(wlc->home_chanspec)));
			ASSERT(0);
		}
	}
#endif /* WL_UCM */

exit:
	wlc_scan_info->state &= ~SCAN_STATE_IN_TMR_CB;

	/* You can't read hardware registers, tsf in this case, if you don't have
	 * clocks enabled. e.g. you are down. The exit path of this function will
	 * result in a down'ed driver if we are just completing a scan and MPC is
	 * enabled and meets the conditions to down the driver (typically no association).
	 * While typically MPC down is deferred for a time delay, the call of the
	 * wlc_scan_mpc_upd() in the scan completion path will force an immediate down
	 * before we get to the exit of this function.  For this reason, we condition
	 * the read of the tsf timer in the debugging output on the presence of
	 * hardware clocks.
	 */
	WL_SCAN_ENT(scan_info, ("wl%d: %s: exit, state 0x%x tsf %u\n",
	                        scan_info->unit, __FUNCTION__,
	                        wlc_scan_info->state,
	                        SCAN_GET_TSF_TIMERLOW(wlc)));
} /* wlc_scantimer */

/*** performs the next scan action */
static void
wlc_scan_act(scan_info_t *scan_info, uint dwell)
{
	wlc_info_t *wlc = scan_info->wlc;
#ifdef BCMDBG
	uint saved = dwell;
#endif // endif
	/* callback is an argument to wlc_scan(), eg wlc_af_send() */
	(scan_info->act_cb)(wlc, scan_info->act_cb_arg, &dwell);

#ifdef BCMDBG
	if (dwell != saved) {
		WL_SCAN(("wl%d: %s: adjusting dwell time from %u to %u ms\n",
			scan_info->unit, __FUNCTION__, saved, dwell));
	}
#endif // endif

	WLC_SCAN_ADD_TIMER(scan_info, dwell, 0);
}

/** called by other modules, e.g. on beacon reception */
void
wlc_scan_radar_clear(wlc_scan_info_t *wlc_scan_info)
{
	scan_info_t *scan_info = (scan_info_t *) wlc_scan_info->scan_priv;
	wlc_info_t *wlc = scan_info->wlc;
	chanspec_t chanspec = wlc->chanspec;
	uint32 tsf_l;
	uint32 elapsed_time, remaining_time, active_time;
	char chanbuf[CHANSPEC_STR_LEN];

	BCM_REFERENCE(chanbuf);

	/* if a passive scan was requested,
	 * or we already processed the radar clear signal,
	 * or it is not a prohibited channel,
	 * then do nothing
	 */
	if (!wlc_valid_chanspec_db(wlc->cmi, chanspec) ||
		wlc_channel_clm_restricted_chanspec(wlc->cmi, chanspec) ||
		(wlc_scan_info->state & (SCAN_STATE_PASSIVE | SCAN_STATE_RADAR_CLEAR))) {

		return;
	}

	/* if we are not in the channel scan portion of the scan, do nothing */
	if (scan_info->state != WLC_SCAN_STATE_LISTEN) {
		return;
	}

	wlc_read_tsf(wlc, &tsf_l, NULL);

	elapsed_time = (tsf_l - scan_info->start_tsf) / 1000;
	if (elapsed_time > scan_info->passive_time) {
		remaining_time = 0;
	} else {
		remaining_time = scan_info->passive_time - elapsed_time;
	}

	if (remaining_time < WLC_SCAN_MIN_PROBE_TIME) {
		return;
	}

	if (remaining_time >= scan_info->active_time) {
		/* Switch back to full active scan, once this active scan is complete the channel
		 * will be changed.
		 */
		active_time = scan_info->active_time / scan_info->npasses;
	} else {
		/* This will result in one probe, while remaining for the rest of the passive
		 * dwell time on the channel, avoid forced MSCH update, by clearing timeslot_id:
		 */
		active_time = remaining_time;
		scan_info->timeslot_id = 0;
	}

	scan_info->pass = scan_info->npasses;
	if (scan_info->pass > 0) {
		/* everything is ok to switch to an active scan */
		wlc_scan_info->state |= SCAN_STATE_RADAR_CLEAR;
		scan_info->state = WLC_SCAN_STATE_SEND_PROBE;

		WLC_SCAN_DEL_TIMER(scan_info);

		wlc_mute(wlc, OFF, 0);

		wlc_scan_act(scan_info, active_time);
		scan_info->pass--;

		WL_REGULATORY(("wl%d: %s: rcvd beacon on radar chanspec %s, converting"
			" to active scan, %d ms left\n", scan_info->unit, __FUNCTION__,
			wf_chspec_ntoa_ex(chanspec, chanbuf), active_time));
	}
} /* wlc_scan_radar_clear */

static int
wlc_scan_add_chanspec(scan_info_t *scan_info, chanspec_t chanspec)
{
	scan_info->chanspec_list[scan_info->channel_num] = chanspec;
	scan_info->channel_num++;
	ASSERT(scan_info->channel_num < SCAN_MAX_CHANSPECS);
	if (scan_info->channel_num >= SCAN_MAX_CHANSPECS) {
		return BCME_ERROR;
	}
	return BCME_OK;
}

#if BAND6G
/**
 * Determines how the 6G scan should be performed. It is required that bandmask has been
 * determined before this function is called
 * n_ssid, number of ssids to scan, can be 0
 * n_channel, number of chanspecs to be scanned, 0 means all.
 */

static void
wlc_scan_determine_6g_params(scan_info_t *scan_info, uint n_channel, uint scan_flags,
	const struct ether_addr *bssid)
{
	bool all_bands;

	/* Start with assuming worst case, then check if we can switch to better/faster mode */
	scan_info->probing_6g = SCAN_6G_PASSIVE_PROBING;
	scan_info->channels_6g = SCAN_6G_ALL_CHANNELS;

	all_bands = (scan_info->bandmask == (SCAN_BAND_2G | SCAN_BAND_5G | SCAN_BAND_6G));

	/* RNR non-wildcard case will be handled during scan start */
	if ((!scan_info->ssid_wildcard_enabled) || (eacmp(bssid, &ether_bcast))) {
		scan_info->probing_6g = SCAN_6G_ACTIVE_PROBING;
	}

	if (n_channel != 0) {
		goto done;
	}

	if (all_bands) {
		scan_info->channels_6g = SCAN_6G_PSC_CHANNELS | SCAN_6G_RNR_CHANNELS;
		goto done;
	}

	/* supporting iovar options based scan */
	if (scan_flags & WLC_SCANFLAGS_RNR) {
		scan_info->channels_6g = SCAN_6G_RNR_CHANNELS;
	}
	if (scan_flags & WLC_SCANFLAGS_PSC) {
		scan_info->channels_6g |= SCAN_6G_PSC_CHANNELS;
	}

done:
	if ((scan_info->channels_6g & SCAN_6G_RNR_CHANNELS) ||
		(scan_info->channels_6g & SCAN_6G_PSC_CHANNELS)) {
		scan_info->probing_6g = SCAN_6G_ACTIVE_PROBING;
	}
	WL_SCAN(("%s: 6G; scan type = %d, Probing active = %d\n", __FUNCTION__,
		scan_info->channels_6g,	scan_info->probing_6g == SCAN_6G_ACTIVE_PROBING));
}
#endif /* BAND6G */
/**
 * Returns bandmask as bitmask of the bands to be scanned (SCAN_BAND_..)
 * Input bandtype of type WLC_BAND...
 */
static uint32
wlc_scan_get_bandmask(scan_info_t *scan_info, uint bandtype)
{
	wlc_info_t *wlc = scan_info->wlc;
	uint32 bandmask;

	/* If bandlocked then overrule the configured bandtype with band locked */
	if (wlc->bandlocked) {
		bandtype = CHSPEC_BANDTYPE(wlc->home_chanspec);
	}

	bandmask = 0;

	/* Determine the bands the HW supports */
	if (wlc_get_band(wlc, BAND_2G_INDEX)) {
		bandmask |= SCAN_BAND_2G;
	}
	if (wlc_get_band(wlc, BAND_5G_INDEX)) {
		bandmask |= SCAN_BAND_5G;
	}
	if (wlc_get_band(wlc, BAND_6G_INDEX)) {
		bandmask |= SCAN_BAND_6G;
	}

	/* Now check if there is a limitation configured by bandtype */
	if (bandtype != WLC_BAND_ALL) {
		if (bandtype == WLC_BAND_2G) {
			bandmask &= SCAN_BAND_2G;
		} else if (bandtype == WLC_BAND_5G) {
			bandmask &= SCAN_BAND_5G;
		} else if (bandtype == WLC_BAND_6G) {
			bandmask &= SCAN_BAND_6G;
		}
	}

	return bandmask;
}

/**
 * Returns default channels for this locale in band 'band'. Each band to be scanned gets a band
 * specific order and its limitations will be applied.
 */
static void
wlc_scan_get_default_channels(scan_info_t *scan_info)
{
	wlc_info_t *wlc = scan_info->wlc;
	chanspec_t chanspec;
	struct wlcband *band;
	uint channel;

	/* At least one band should be set */
	ASSERT(scan_info->bandmask & (SCAN_BAND_2G | SCAN_BAND_5G | SCAN_BAND_6G));

	scan_info->channel_num = 0;

	/* First the 2.4G band is configured. In 2.4 first the PSC channels should be scanned if
	 * OCE enabled followed by non-PSC channels.
	 */
	if (!(scan_info->bandmask & SCAN_BAND_2G)) {
		goto setup_band_5g;
	}
#ifdef WL_OCE
	if (OCE_ENAB(wlc->pub)) {
		/* populate chanspec list with OCE channels first */
		scan_info->channel_num += wlc_oce_get_pref_channels(scan_info->chanspec_list);
	}
#endif /* WL_OCE */
	band = wlc_get_band(wlc, BAND_2G_INDEX);
	FOREACH_WLC_BAND_CHANNEL20(band, channel) {
		if (wlc_valid_channel20_in_band(wlc->cmi, BAND_2G_INDEX, channel)) {
			chanspec = CH20MHZ_CHSPEC2(channel, WL_CHANSPEC_BAND_2G);
#ifdef WL_OCE
			/* skip adding OCE channels to the list since its already done */
			if (!OCE_ENAB(wlc->pub) || !wlc_oce_is_pref_channel(chanspec))
#endif /* WL_OCE */
			{
				if (wlc_scan_add_chanspec(scan_info, chanspec))
					goto done;
			}
		}
	}

setup_band_5g:
	/* Now the 5G band. In 5g first the normal non-radar channels are to be scanned, followed
	 * by the radar channels
	 */
	if (!(scan_info->bandmask & SCAN_BAND_5G)) {
		goto setup_band_6g;
	}
	band = wlc_get_band(wlc, BAND_5G_INDEX);
	/* First the non radar 5G channels */
	FOREACH_WLC_BAND_CHANNEL20(band, channel) {
		chanspec = CH20MHZ_CHSPEC2(channel, WL_CHANSPEC_BAND_5G);
		if (wlc_valid_channel20_in_band(wlc->cmi, BAND_5G_INDEX, channel) &&
			!wlc_quiet_chanspec(wlc->cmi, chanspec) &&
			(wlc_dfs_valid_ap_chanspec(wlc, chanspec))) {

			if (wlc_scan_add_chanspec(scan_info, chanspec))
				goto done;
		}
	}

	/* Now for the radar 5G channels */
	FOREACH_WLC_BAND_CHANNEL20(band, channel) {
		chanspec = CH20MHZ_CHSPEC2(channel, WL_CHANSPEC_BAND_5G);
		if (wlc_valid_channel20_in_band(wlc->cmi, BAND_5G_INDEX, channel) &&
			wlc_quiet_chanspec(wlc->cmi, chanspec) &&
			(wlc_dfs_valid_ap_chanspec(wlc, chanspec))) {

			if (wlc_scan_add_chanspec(scan_info, chanspec))
				goto done;
		}
	}

setup_band_6g:
	if (!(scan_info->bandmask & SCAN_BAND_6G)) {
		goto done;
	}
#if BAND6G
	band = wlc_get_band(wlc, BAND_6G_INDEX);
	FOREACH_WLC_BAND_CHANNEL20(band, channel) {
		if (wlc_valid_channel20_in_band(wlc->cmi, BAND_6G_INDEX, channel)) {
			if ((scan_info->channels_6g == SCAN_6G_ALL_CHANNELS) ||
					(WF_IS_6G_PSC_CHAN(channel))) {
				chanspec = CH20MHZ_CHSPEC2(channel, WL_CHANSPEC_BAND_6G);
				if (wlc_scan_add_chanspec(scan_info, chanspec))
					goto done;
			}
		}
	}

	if (wlc->bandlocked && (scan_info->bandmask & SCAN_BAND_6G)) {
		scan_info->channel_idx = 0;
		wlc_scan_6g_prepare_channel_list(scan_info->scan_pub);
	}
#endif /* BAND6G */

done:
	WL_SCAN(("%s: found %d channels to scan, bandmask=%x\n", __FUNCTION__,
		scan_info->channel_num, scan_info->bandmask));
}

#if BAND6G
/*
*Checks if input channel is reported through RNR
* @param   wlc_scan_info_t , channel
* @return  TRUE : If channel is RNR reported
* False : IF channels is not RNR reported
*/
static bool
wlc_scan_6g_channel_is_rnr_reported(wlc_scan_info_t *wlc_scan_info, uint8 channel)
{
	uint8 index = 0;
	index = SCAN_6GCHAN_TO_INDEX(channel);
	ASSERT(index < WL_NUMCHANSPECS_6G_20);
	if (wlc_scan_info->scan_6g_prof[index] == NULL) {
		return 0; /* Default value of RNR sent */
	}
	return wlc_scan_info->scan_6g_prof[index]->rnr;
}

/*
* Handles updating the SCAN states for a RNR reported channels , so that scan timer
* is scheduled accordingly.
* @param   wlc_scan_info_t
* @return  void
*/
static void
wlc_scan_6g_process_rnr(wlc_scan_info_t *wlc_scan_info)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
	uint8 index, channel = 0;
	bool listen_6G_20TU_req = FALSE;
	uint32 time_ms = 0;
	channel = CHSPEC_CHANNEL(scan_info->cur_scan_chanspec);
	index = SCAN_6GCHAN_TO_INDEX(channel);
	ASSERT(index < WL_NUMCHANSPECS_6G_20);
	scan_info->state_6g_chan = SCAN_6G_CHAN_SEND_PROBE;
	if (wlc_scan_info->scan_6g_prof[index] == NULL) {
		return;
	}
	listen_6G_20TU_req = wlc_scan_info->scan_6g_prof[index]->prbres_20tu_set;
	scan_info->state = WLC_SCAN_STATE_START;

	/* RNR assisted full scan */
	if (scan_info->channels_6g & SCAN_6G_ALL_CHANNELS) {
		if (WF_IS_6G_PSC_CHAN(channel)) {
			scan_info->state_6g_chan = SCAN_6G_CHAN_IS_PSC_WAIT_FILS_DISC;
			time_ms = SCAN_6G_PROBE_DEFER_TIME;
		}
	/* RNR Scan */
	} else {
		if (listen_6G_20TU_req) {
			scan_info->state_6g_chan = SCAN_6G_CHAN_IN_RNR_20TU_SET;
			time_ms = SCAN_6G_PROBE_DEFER_TIME;
		}
	}

	if (time_ms) {
		scan_info->state = WLC_SCAN_STATE_LISTEN;
	}
	WLC_SCAN_ADD_TIMER(scan_info, time_ms, 0);
}

/* Handles processing of RNR , PSC and NON PSC channels */
static void
wlc_scan_6g_chan_handle(scan_info_t *scan_info)
{
	wlc_scan_info_t *wlc_scan_info = scan_info->scan_pub;
	uint8 channel = CHSPEC_CHANNEL(scan_info->cur_scan_chanspec);

	/* check if channel is in RNR database and 20TU is set   */
	if (wlc_scan_6g_channel_is_rnr_reported(wlc_scan_info, channel)) {
		wlc_scan_6g_process_rnr(wlc_scan_info);
	} else if (WF_IS_6G_PSC_CHAN(channel)) {
		wlc_scan_6g_process_psc(wlc_scan_info);
	} else {
		wlc_scan_6g_process_non_rnr(wlc_scan_info);
	}
}

/*
 * Handles updating the SCAN states for a PSC channel , so that scan timer
 * is scheduled accordingly.
 * @param   wlc_scan_info_t
 * @return  void
 */
static void
wlc_scan_6g_process_psc(wlc_scan_info_t *wlc_scan_info)
{
	 scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;

	 scan_info->state_6g_chan = SCAN_6G_CHAN_IS_PSC_WAIT_FILS_DISC;
	 scan_info->prb_idle_on_6g_chan = FALSE;

	 scan_info->state = WLC_SCAN_STATE_LISTEN;
	 WLC_SCAN_ADD_TIMER(scan_info, SCAN_6G_PROBE_DEFER_TIME, 0);
}

/**
 * Handles updating the SCAN states for a NON RNR channel , so that scan timer
 * is scheduled accordingly.
 * @param   wlc_scan_info_t
 * @return  void
 */

static void
wlc_scan_6g_process_non_rnr(wlc_scan_info_t *wlc_scan_info)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;

	scan_info->state_6g_chan = SCAN_6G_CHAN_SEND_PROBE;
	scan_info->state = WLC_SCAN_STATE_START;
	WLC_SCAN_ADD_TIMER(scan_info, 0, 0);
}

/*
 * Handles sending probereq on 6G channel for both RNR reported channels
 * PSC channels.
 * Traverses through the list by sending probe request untill list is empty
 * @param   wlc_scan_info_t , cfg , SSID, Destination Address
 * @return  void
 */
static void
wlc_scan_handle_probereq_for_6g(wlc_scan_info_t *wlc_scan_info, wlc_bsscfg_t *cfg,
	wlc_ssid_t *ssid, const struct ether_addr *da)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
	bool wildcard = scan_info->ssid_wildcard_enabled;
	chanspec_t cur_send_chanspec = scan_info->cur_scan_chanspec;
	uint8 channel = CHSPEC_CHANNEL(cur_send_chanspec);
	uint8 index = SCAN_6GCHAN_TO_INDEX(channel);
	wlc_scan_6g_chan_prof_t *chan_prof_6g, *temp = NULL;
	const struct ether_addr *bssid = &wlc_scan_info->bssid;
	uint32 *short_ssid = NULL;
	uint8 n_short_ssid = 0;

	ASSERT(index < WL_NUMCHANSPECS_6G_20);
	if ((wlc_scan_info->scan_6g_prof[index] != NULL) &&
			wlc_scan_info->scan_6g_prof[index]->profile != NULL) {
		chan_prof_6g = wlc_scan_info->scan_6g_prof[index]->profile;
		while (chan_prof_6g) {
			if (chan_prof_6g->bssid_valid) {
				bssid = &chan_prof_6g->bssid;
			} else {
				bssid = &ether_bcast;
			}
			if (chan_prof_6g->is_short_ssid) {
				short_ssid = (uint32 *)&chan_prof_6g->short_ssid;
				n_short_ssid = SHORT_SSID_LEN/sizeof(uint32);
			}

			/* For JOIN/ROAM scan  SSID will not be SHORT SSID,
			 * so parse through the RNR list,
			 * send the Probe Request for the remaining items
			 *  in the list
			 */
			if (!wildcard) {
				if (((!ETHER_ISNULLADDR(&wlc_scan_info->bssid)) &&
					(!ETHER_ISBCAST(&wlc_scan_info->bssid)))) {
					bssid = &wlc_scan_info->bssid;
				}
				wlc_sendprobe(scan_info->wlc, cfg, ssid->SSID, ssid->SSID_len,
						n_short_ssid, short_ssid,
						&scan_info->sa_override, da, bssid,
						SCAN_PROBE_TXRATE(scan_info), NULL, 0);
			} else {
				/* In Wild Card scan RNR list may contain Short SSID or
				 * regular SSID if same ssid bit in RNR is set
				 * while sending the probe request write to
				 * short SSID element if short ssid is valid or regular SSID
				 */
				if (chan_prof_6g->is_short_ssid) {
#ifdef WL_OCE
					wlc_sendprobe(scan_info->wlc, cfg, chan_prof_6g->SSID,
						chan_prof_6g->ssid_length,
						n_short_ssid, short_ssid,
						&scan_info->sa_override,
						da, bssid, SCAN_PROBE_TXRATE(scan_info),
						NULL, 0);
#endif /* WL_OCE */
				} else if (chan_prof_6g->ssid_length != 0 ||
					((!ETHER_ISNULLADDR(bssid)) &&
					(!ETHER_ISBCAST(bssid)))) {
					wlc_sendprobe(scan_info->wlc, cfg, chan_prof_6g->SSID,
						chan_prof_6g->ssid_length, 0, 0,
						&scan_info->sa_override, da, bssid,
						SCAN_PROBE_TXRATE(scan_info), NULL, 0);
				}
			}
			temp = chan_prof_6g;
			chan_prof_6g = (wlc_scan_6g_chan_prof_t *) temp->next_list;
		}
	} else if (scan_info->channels_6g & SCAN_6G_ALL_CHANNELS) {
		wlc_sendprobe(scan_info->wlc, cfg, ssid->SSID, ssid->SSID_len, 0, 0,
			&scan_info->sa_override, da, bssid,
			0, NULL, 0);
	}
}

/**
 * RNR channel list is updated by removing the entry form the database
 *
 * when matching BSSID is present for the given chanspec
 *
 * @param   wlc_scan_info_t , chanspec , BSSID
 *
 * @return  void
 */
void
wlc_scan_6g_chan_list_update_in_rnr(wlc_scan_info_t *wlc_scan_info, chanspec_t chanspec,
		struct ether_addr *bssid)
{
	uint8 channel = CHSPEC_CHANNEL(chanspec);
	uint8 index = SCAN_6GCHAN_TO_INDEX(channel);
	wlc_scan_6g_chan_prof_t *chan_prof_6g, *head, *prev = NULL;

	ASSERT(index < WL_NUMCHANSPECS_6G_20);
	if ((wlc_scan_info->scan_6g_prof[index] != NULL) &&
			wlc_scan_info->scan_6g_prof[index]->profile != NULL &&
			wlc_scan_info->scan_6g_prof[index]->chanspec == chanspec) {
		head = wlc_scan_info->scan_6g_prof[index]->profile;
		chan_prof_6g = head;
		do {
			if (chan_prof_6g->bssid_valid && (!eacmp(bssid, &chan_prof_6g->bssid))) {
				if (chan_prof_6g == head) {
					head = (wlc_scan_6g_chan_prof_t *) chan_prof_6g->next_list;
					wlc_scan_info->scan_6g_prof[index]->profile = head;
					break;
				} else {
					prev->next_list = chan_prof_6g->next_list;
					chan_prof_6g = (wlc_scan_6g_chan_prof_t *)prev->next_list;
				}
			} else {
				prev = chan_prof_6g;
				chan_prof_6g = (wlc_scan_6g_chan_prof_t *)chan_prof_6g->next_list;
			}
		} while (chan_prof_6g);
	}
}

/* Build scan list for 6G
 * Host channel present also in RNR database
 * PSC channel list Not part of RNR database
 * Non PSC host channel not part of RNR database
 */
void
wlc_scan_6g_prepare_channel_list(wlc_scan_info_t *wlc_scan_info)
{
	uint8 i = 0, chan_cnt = 0, idx = 0, non_psc_idx = 0, psc_idx = 0;
	scan_info_t *scan_info = (scan_info_t*)wlc_scan_info->scan_priv;
	chanspec_t nonpsc_chan_list[WL_NUMCHANSPECS_6G_20] = {INVCHANSPEC};
	chanvec_t list_6g;
	uint8 channel;
	chanspec_t psc_chan_list[WL_NUMCHANNELS_6G_PSC] = {INVCHANSPEC};
	chanspec_t chanspec = INVCHANSPEC;
#if defined(BCMDBG)
	uint8 rnr_channels = 0, nonrnr_channels = 0, psc_channels = 0;
#endif /* BCMDBG */
	idx = scan_info->channel_idx;

	bzero(&list_6g, sizeof(list_6g));
	/* Scan_info->chanspec list contains the channel provided from the HOST to scan
	 * Make a seperate list for Non PSC channel and PSC channel
	 */
	while (CHSPEC_IS6G(scan_info->chanspec_list[idx]) && idx < scan_info->channel_num) {
		channel = CHSPEC_CHANNEL(scan_info->chanspec_list[idx]);
		if (!WF_IS_6G_PSC_CHAN(channel)) {
			nonpsc_chan_list[non_psc_idx] = scan_info->chanspec_list[idx];
			non_psc_idx++;
		} else {
			psc_chan_list[psc_idx] = scan_info->chanspec_list[idx];
			psc_idx++;
		}
		setbit(list_6g.vec, channel);
		idx++;
	}
	idx = scan_info->channel_idx;
	/* Append the channels from RNR list to scan_info->chanspec_list if the channel matches
	 * with the host channels requested
	 */
	if (scan_info->channels_6g & SCAN_6G_RNR_CHANNELS) {
		for (i = 0; i < WL_NUMCHANSPECS_6G_20; i++) {
			if (wlc_scan_info->scan_6g_prof[i] == NULL) {
				continue;
			}
			chanspec = wlc_scan_info->scan_6g_prof[i]->chanspec;
			channel = CHSPEC_CHANNEL(chanspec);
			if (isset(list_6g.vec, channel)) {
				scan_info->chanspec_list[idx] =
					wlc_scan_info->scan_6g_prof[i]->chanspec;
				if (WF_IS_6G_PSC_CHAN(channel)) {
					wlc_scan_6g_prune_chanlist(psc_chan_list,
							scan_info->chanspec_list[idx], TRUE);
				} else {
					wlc_scan_6g_prune_chanlist(nonpsc_chan_list,
							scan_info->chanspec_list[idx], FALSE);
				}
				chan_cnt++;
				idx++;
			}
		}
	}

#if defined(BCMDBG)
	rnr_channels = chan_cnt;
	WL_SCAN(("Num of RNR reported 6G channels = %d\n", rnr_channels));
#endif /* BCMDBG */
	if (scan_info->channels_6g == SCAN_6G_RNR_CHANNELS) {
		goto done;
	}

	if (scan_info->channels_6g & SCAN_6G_PSC_CHANNELS ||
		scan_info->channels_6g & SCAN_6G_ALL_CHANNELS) {
		/* Insert the remaining PSC channels not found in RNR list */
		for (i = 0; i < psc_idx; i++) {
			if (psc_chan_list[i] != INVCHANSPEC) {
				scan_info->chanspec_list[idx] = psc_chan_list[i];
				chan_cnt++;
				idx++;
			}
		}
	}
#if defined(BCMDBG)
	psc_channels = chan_cnt - rnr_channels;
	WL_SCAN(("Num of NON RNR reported PSC 6G channels = %d\n", psc_channels));
#endif /* BCMDBG */

	if (scan_info->channels_6g & SCAN_6G_ALL_CHANNELS) {
		/* Insert the remaining host provided NONPSC channels after the RNR list */
		for (i = 0; i < non_psc_idx; i++) {
			if (nonpsc_chan_list[i] != INVCHANSPEC) {
				scan_info->chanspec_list[idx] = nonpsc_chan_list[i];
				chan_cnt++;
				idx++;
			}
		}
	}
#if defined(BCMDBG)
	nonrnr_channels = chan_cnt - (rnr_channels + psc_channels);
	WL_SCAN(("Num of NON  RNR reported 6G channels = %d\n", nonrnr_channels));
#endif /* BCMDBG */
done:
	scan_info->channel_num = chan_cnt;
}

/* Identifies whether the cur scan channel and prev scanned channel
 * is of same band or different band
 * @param : cur scan channel idx , scan_info_t
 * @return : TRUE if prev_chan and cur_chan are different band
 * else FALSE
 */
static bool
wlc_scan_find_6g_chan_start(scan_info_t *scan_info)
{
	chanspec_t prev_chanspec;
	uint8 prev_chan_idx;
	bool diff_band = FALSE;
	if (scan_info->channel_idx > 0 && scan_info->channel_idx < scan_info->channel_num) {
		prev_chan_idx = scan_info->channel_idx - 1;
		prev_chanspec = scan_info->chanspec_list[prev_chan_idx];
		if (((CHSPEC_IS5G(prev_chanspec) || (CHSPEC_IS2G(prev_chanspec))) &&
			CHSPEC_IS6G(scan_info->chanspec_list[scan_info->channel_idx]))) {
			diff_band = TRUE;
		}
	}
	return diff_band;
}

/* Updates the RNR and PSC chanspec list built from the host provided chanspec */
static void
wlc_scan_6g_prune_chanlist(chanspec_t *chan_ptr, chanspec_t chanspec, bool psc)
{
	uint8 i;
	if (psc) {
		for (i = 0; i < WL_NUMCHANNELS_6G_PSC; i++) {
			if (chan_ptr[i] == chanspec) {
				chan_ptr[i] = INVCHANSPEC;
				break;
			}
		}
	} else {
		for (i = 0; i < WL_NUMCHANSPECS_6G_20; i++) {
			if (chan_ptr[i] == chanspec) {
				chan_ptr[i] = INVCHANSPEC;
				break;
			}
		}
	}

}

/*
* this function checks to see if there are any entries populated from the
* fils discovery frame in case of PSC channel or 20Ms listen in case of
* Non RNR channel
* @param    scan_info_t
* @return TRUE : If Entries present
* FALSE : Entries are NULL
*/
static bool
wlc_scan_6g_check_chan_list_avail(scan_info_t *scan_info)
{
	wlc_scan_info_t *wlc_scan_info = scan_info->scan_pub;
	uint8 channel = CHSPEC_CHANNEL(scan_info->cur_scan_chanspec);
	uint8 index = SCAN_6GCHAN_TO_INDEX(channel);
	ASSERT(index < WL_NUMCHANSPECS_6G_20);
	if (wlc_scan_info->scan_6g_prof[index] == NULL ||
			wlc_scan_info->scan_6g_prof[index]->profile == NULL ||
			wlc_scan_info->scan_6g_prof[index]->chanspec == INVCHANSPEC) {
		return FALSE;
	}
	return TRUE;
}

/* scan state  will be in SCAN_STATE_LISTEN in below  conditions
 * case 1: RNR scan
 * 1) 20TU bit is set in RNR element,scan will be in Listen state for 20ms
 * to receive Probe responses, 6G scan state will be SCAN_6G_CHAN_IN_RNR_20TU_SET
 * and state transits to SCAN_6G_CHAN_20TU_TO_PRBREQ here
 * 2) Channel is PSC , scan is wildcard/non wildcard , to receive FILS DISCOVERY we continue to
 * listen for 20ms, 6G scan state will be in SCAN_6G_CHAN_PSC_WAIT_FILS_DISC to
 * SCAN_6G_CHAN_PSC_FILSDISC_TO_PRBREQ here
 * case 2: Full scan
 * 1) If scan is wildcard, to receive beacon we continue to listes for 110 ms
 * 2) If scan is non wild card, to receive FILS/probe responce scan will be in
 * listen state for 25ms, then 6g state is changed from SCAN_6G_CHAN_PSC_WAIT_FILS_DISC to
 * SCAN_6G_CHAN_PSC_FILSDISC_TO_PRBREQ here
 */
static void
wlc_scan_6g_handle_chan_states(scan_info_t *scan_info)
{
	scan_6g_chan_state_t scan_6g_state;
	scan_6g_state = scan_info->state_6g_chan;

	scan_info->state = WLC_SCAN_STATE_START;
	if (wlc_scan_6g_check_chan_list_avail(scan_info) ||
			(scan_info->probing_6g == SCAN_6G_ACTIVE_PROBING)) {
		if (scan_6g_state == SCAN_6G_CHAN_IN_RNR_20TU_SET) {
			scan_info->state_6g_chan = SCAN_6G_CHAN_20TU_TO_PRBREQ;
		} else if (scan_6g_state == SCAN_6G_CHAN_IS_PSC_WAIT_FILS_DISC) {
			scan_info->state_6g_chan = SCAN_6G_CHAN_PSC_FILSDISC_TO_PRBREQ;
		} else if (scan_6g_state == SCAN_6G_CHAN_NON_RNR_WAIT_DISC) {
			scan_info->state_6g_chan = SCAN_6G_CHAN_SEND_PROBE;
		}
		scan_info->state = WLC_SCAN_STATE_START;
	} else {
		/*
		 * In case of full scan with Wildcard ssid and bssid
		 */
		scan_info->state = WLC_SCAN_STATE_SEND_PROBE;
		scan_info->pass = 0;
	}
}
#endif /* BAND6G */

/** returns a list of 'forbidden' channels */
static uint
wlc_scan_prohibited_channels(scan_info_t *scan_info, chanspec_t *chanspec_list,
	int channel_max)
{
	wlc_info_t *wlc = scan_info->wlc;
	const char *acdef = wlc_11d_get_autocountry_default(wlc->m11d);
	wlcband_t *band;
	uint channel, maxchannel, j;
	chanspec_band_t chspec_band;
	chanvec_t sup_chanvec, chanvec;
	int num = 0;
	enum wlc_bandunit bandunit;

	FOREACH_WLC_BAND(wlc, bandunit) {
		band = wlc->bandstate[bandunit];
		bzero(&sup_chanvec, sizeof(sup_chanvec));
		/* Get the list of all the channels in autocountry_default and supported by phy */
		phy_radio_get_valid_chanvec(WLC_PI_BANDUNIT(wlc, bandunit),
			band->bandtype, &sup_chanvec);
		if (!wlc_channel_get_chanvec(wlc, acdef, band->bandtype, &chanvec)) {
			return 0;
		}

		for (j = 0; j < ARRAYSIZE(sup_chanvec.vec); j++) {
			sup_chanvec.vec[j] &= chanvec.vec[j];
		}

		maxchannel = BAND_2G(band->bandtype) ? (CH_MAX_2G_CHANNEL + 1) : MAXCHANNEL;
		chspec_band = wlc_bandunit2chspecband(bandunit);
		for (channel = 0; channel < maxchannel; channel++) {
			if (isset(sup_chanvec.vec, channel) &&
				!wlc_valid_channel20_in_band(wlc->cmi, bandunit, channel)) {
				chanspec_list[num++] = wf_create_20MHz_chspec(channel, chspec_band);

				if (num >= channel_max)
					return num;
			}
		}
	}

	return num;
}

/** Invoked by wlc_scan_callback() */
static int
wlc_scan_apply_scanresults(scan_info_t *scan_info, int status)
{
#ifdef STA
	int idx;
	wlc_bsscfg_t *cfg;
#endif /* STA */
	wlc_scan_info_t *wlc_scan_info;
	wlc_info_t *wlc = scan_info->wlc;

	BCM_REFERENCE(status);
	BCM_REFERENCE(wlc);
	BCM_REFERENCE(wlc_scan_info);

	wlc_scan_info = scan_info->scan_pub;

#ifdef STA
#ifdef WL11D
	/* If we are in 802.11D mode and we are still waiting to find a
	 * valid Country IE, then take this opportunity to parse these
	 * scan results for one.
	 */
	if (WLC_AUTOCOUNTRY_ENAB(wlc))
		wlc_11d_scan_complete(wlc->m11d, WLC_E_STATUS_SUCCESS);
#endif /* WL11D */
#endif /* STA */

	wlc_ht_obss_scan_update(scan_info, WLC_E_STATUS_SUCCESS);

	/* Don't fill the cache with results from a P2P discovery scan since these entries
	 * are short-lived. Also, a P2P association cannot use Scan cache
	 */
	if (SCANCACHE_ENAB(wlc_scan_info) &&
#ifdef WLP2P
	    !BSS_P2P_DISC_ENAB(wlc, scan_info->bsscfg) &&
#endif // endif
	    TRUE) {
		wlc_scan_cache_result(scan_info);
	}

#ifdef STA
	/* if this was a broadcast scan across all channels,
	 * update the roam cache, if possible
	 */
	if (ETHER_ISBCAST(&wlc_scan_info->bssid) &&
	    wlc_scan_info->bss_type == DOT11_BSSTYPE_ANY) {
		FOREACH_AS_STA(wlc, idx, cfg) {
			wlc_roam_t *roam = cfg->roam;
			if (roam && roam->roam_scan_piggyback &&
			    roam->active && !roam->fullscan_count) {
				WL_ASSOC(("wl%d: %s: Building roam cache with"
				          " scan results from broadcast scan\n",
				          scan_info->unit, __FUNCTION__));
				/* this counts as a full scan */
				roam->fullscan_count = 1;
				/* update the roam cache */
				wlc_build_roam_cache(cfg, wlc->scan_results);
			}
		}
	}
#endif /* STA */

	return BCME_OK;
} /* wlc_scan_apply_scanresults */

/** Invoked by e.g. the scan timer callback function, or when a scan is prematurely terminated */
static void
wlc_scan_callback(scan_info_t *scan_info, uint status)
{
	scancb_fn_t cb = scan_info->cb;
	void *cb_arg = scan_info->cb_arg;
	wlc_bsscfg_t *cfg = scan_info->bsscfg;
	int scan_completed;
	wlc_info_t *wlc = scan_info->wlc;
#ifdef WLPFN
	bool is_pfn_in_progress;
	/* Check the state before cb is called as the state
	 * changes after cb
	 */
	is_pfn_in_progress = WLPFN_ENAB(wlc->pub) &&
	                     wl_pfn_scan_in_progress(wlc->pfn);
#endif /* WLPFN */

	scan_completed = wlc_scan_apply_scanresults(scan_info, status);

	scan_info->bsscfg = NULL;
	scan_info->cb = NULL;
	scan_info->cb_arg = NULL;
	/* This scan is completed. If this callbac is from msch cb, cb should be deferred. */
	scan_info->scan_pub->flag &= ~SCAN_MSCH_ONCHAN_CALLBACK;

	/* Registered Scan callback function should take care of
	 * sending a BSS event to the interface attached to it.
	 */
	if (cb != NULL) {
		(cb)(cb_arg, status, cfg);
#ifdef WL_SCAN_DFS_HOME
		/* On successful scan completion, get into ISM state if home channel is DFS */
		if ((status != WLC_E_STATUS_ERROR) && (WL11H_AP_ENAB(wlc)) &&
			(wlc_radar_chanspec(wlc->cmi, wlc->home_chanspec))) {

			wlc_set_dfs_cacstate(wlc->dfs, ON, cfg);
		}
#endif /* WL_SCAN_DFS_HOME */
	} else if (scan_completed == BCME_OK) {
		/* Post a BSS event if an interface is attached to it */
		wlc_bss_mac_event(wlc, cfg, WLC_E_SCAN_COMPLETE, NULL, status, 0, 0, NULL, 0);
	}

	/* reset scan engine usage */
	if (scan_completed == BCME_OK) {
		scan_info->scan_pub->usage = SCAN_ENGINE_USAGE_NORM;
		/* Free the BSS's in the scan_results. Use the scan info where
		 * scan request is given.
		 */
		wlc_bss_list_free(wlc, wlc->scan_results);

#ifdef ANQPO
		if (ANQPO_ENAB(wlc->pub) && scan_info->scan_pub->is_hotspot_scan) {
			wl_anqpo_scan_stop(wlc->anqpo);
		}
#endif /* ANQPO */
	}
#ifdef WLPFN
	if (WLPFN_ENAB(wlc->pub) && !is_pfn_in_progress)
		wl_notify_pfn(wlc);
#endif /* WLPFN */

	/* allow scanmac to be updated */
	wlc_scanmac_update(scan_info->scan_pub);
} /* wlc_scan_callback */

chanspec_t
wlc_scan_get_current_chanspec(wlc_scan_info_t *wlc_scan_info)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
#ifdef WL_AIR_IQ
	if (wlc_airiq_scan_in_progress(scan_info->wlc) &&
			!wlc_airiq_phymode_3p1(scan_info->wlc)) {
		return wlc_airiq_get_current_scan_chanspec(scan_info->wlc);
	}
#endif /* WL_AIR_IQ */

	if (scan_info->channel_idx >= scan_info->channel_num)
		return scan_info->chanspec_list[scan_info->channel_num - 1];

	return scan_info->chanspec_list[scan_info->channel_idx];
}

int
wlc_scan_ioctl(void *ctx, uint cmd, void *arg, uint len, struct wlc_if *wlcif)
{
	wlc_scan_info_t *wlc_scan_info = ctx;
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
	wlc_info_t *wlc = scan_info->wlc;
	int bcmerror = 0;
	int val = 0, *pval;
	bool bool_val;

	/* default argument is generic integer */
	pval = (int *) arg;
	/* This will prevent the misaligned access */
	if (pval && (uint32)len >= sizeof(val))
		bcopy(pval, &val, sizeof(val));

	/* bool conversion to avoid duplication below */
	bool_val = (val != 0);
	BCM_REFERENCE(bool_val);

	switch (cmd) {
#ifdef STA
	case WLC_SET_PASSIVE_SCAN:
		scan_info->defaults.passive = (bool_val ? 1 : 0);
		break;

	case WLC_GET_PASSIVE_SCAN:
		ASSERT(pval != NULL);
		if (pval != NULL)
			*pval = scan_info->defaults.passive;
		else
			bcmerror = BCME_BADARG;
		break;

	case WLC_GET_SCANSUPPRESS:
		 ASSERT(arg != NULL);
		 bcmerror = wlc_iovar_op(wlc, "scansuppress", NULL, 0, arg, len, IOV_GET, wlcif);
		break;

	case WLC_SET_SCANSUPPRESS:
		ASSERT(arg != NULL);
		bcmerror = wlc_iovar_op(wlc, "scansuppress", NULL, 0, arg, len, IOV_SET, wlcif);
		break;

	case WLC_GET_SCAN_CHANNEL_TIME:
		ASSERT(arg != NULL);
		bcmerror = wlc_iovar_op(wlc, "scan_assoc_time", NULL, 0,
			arg, len, IOV_GET, wlcif);
		break;

	case WLC_SET_SCAN_CHANNEL_TIME:
		ASSERT(arg != NULL);
		bcmerror = wlc_iovar_op(wlc, "scan_assoc_time", NULL, 0,
			arg, len, IOV_SET, wlcif);
		break;

	case WLC_GET_SCAN_UNASSOC_TIME:
		ASSERT(arg != NULL);
		bcmerror = wlc_iovar_op(wlc, "scan_unassoc_time", NULL, 0,
			arg, len, IOV_GET, wlcif);
		break;

	case WLC_SET_SCAN_UNASSOC_TIME:
		ASSERT(arg != NULL);
		bcmerror = wlc_iovar_op(wlc, "scan_unassoc_time", NULL, 0,
			arg, len, IOV_SET, wlcif);
		break;
#endif /* STA */

	case WLC_GET_SCAN_PASSIVE_TIME:
		ASSERT(arg != NULL);
		bcmerror = wlc_iovar_op(wlc, "scan_passive_time", NULL, 0,
			arg, len, IOV_GET, wlcif);
		break;

	case WLC_SET_SCAN_PASSIVE_TIME:
		ASSERT(arg != NULL);
		bcmerror = wlc_iovar_op(wlc, "scan_passive_time", NULL, 0,
			arg, len, IOV_SET, wlcif);
		break;

	case WLC_GET_SCAN_NPROBES:
		ASSERT(arg != NULL);
		bcmerror = wlc_iovar_op(wlc, "scan_nprobes", NULL, 0,
			arg, len, IOV_GET, wlcif);
		break;

	case WLC_SET_SCAN_NPROBES:
		ASSERT(arg != NULL);
		bcmerror = wlc_iovar_op(wlc, "scan_nprobes", NULL, 0,
			arg, len, IOV_SET, wlcif);
		break;

	default:
		bcmerror = BCME_UNSUPPORTED;
		break;
	}
	return bcmerror;
} /* wlc_scan_ioctl */

#ifdef BCMDBG
/* test case support - requires wl UP (wl mpc 0; wl up) */
static void
wlc_scan_test_done(void *arg, int status, wlc_bsscfg_t *cfg)
{
	scan_info_t *scan_info = (scan_info_t *)arg;
	wlc_info_t *wlc = scan_info->wlc;

	BCM_REFERENCE(status);
	BCM_REFERENCE(cfg);

	scan_info->test = SCAN_TEST_NONE;

	if (scan_info->test_timer != NULL) {
		wl_del_timer(wlc->wl, scan_info->test_timer);
		wl_free_timer(wlc->wl, scan_info->test_timer);
		scan_info->test_timer = NULL;
	}
}

/** Invoked via iovar IOV_SCAN_TEST */
static void
wlc_scan_test_timer(void *arg)
{
	scan_info_t *scan_info = (scan_info_t *)arg;
	wlc_scan_info_t	*wlc_scan_info = scan_info->scan_pub;
	wlc_ssid_t ssid;
	int err;

	ssid.SSID_len = 0;

	switch (scan_info->test) {
	case SCAN_TEST_ABORT_ENTRY:
	case SCAN_TEST_ABORT_PSPEND:
	case SCAN_TEST_ABORT_WSUSPEND:
		wlc_scan_terminate(wlc_scan_info, WLC_E_STATUS_SUCCESS);
		break;
	case SCAN_TEST_ABORT_PSPEND_AND_SCAN:
	case SCAN_TEST_ABORT_WSUSPEND_AND_SCAN:
		wlc_scan_terminate(wlc_scan_info, WLC_E_STATUS_SUCCESS);
		err = wlc_scan(wlc_scan_info, DOT11_BSSTYPE_ANY, &ether_bcast, 1, &ssid,
		               -1, -1, -1, -1,
		               NULL, 0, 0, FALSE,
		               wlc_scan_test_done, scan_info,
		               SCANCACHE_ENAB(wlc_scan_info),
		               0, NULL, SCAN_ENGINE_USAGE_NORM, NULL, NULL, NULL);
		if (err != BCME_OK) {
			WL_SCAN_ERROR(("%s: wlc_scan failed, err %d\n", __FUNCTION__, err));
		}
		break;
	}
}

/** Invoked via iovar IOV_SCAN_TEST */
static int
wlc_scan_test(scan_info_t *scan_info, uint8 test_case)
{
	wlc_scan_info_t	*wlc_scan_info = scan_info->scan_pub;
	wlc_info_t *wlc = scan_info->wlc;
	wlc_ssid_t ssid;

	WL_PRINT(("%s: test case %d\n", __FUNCTION__, test_case));

	if (scan_info->test != SCAN_TEST_NONE)
		return BCME_BUSY;

	if (test_case == SCAN_TEST_NONE)
		return BCME_OK;

	ssid.SSID_len = 0;

	if (scan_info->test_timer == NULL)
		scan_info->test_timer =
		        wl_init_timer(wlc->wl, wlc_scan_test_timer, scan_info, "testtimer");
	if (scan_info->test_timer == NULL)
		return BCME_NORESOURCE;

	if ((scan_info->test = test_case) == SCAN_TEST_ABORT_ENTRY)
		/* do this out of order because the timer is served FIFO */
		wl_add_timer(wlc->wl, scan_info->test_timer, 0, 0);

	return wlc_scan(wlc_scan_info, DOT11_BSSTYPE_ANY, &ether_bcast, 1, &ssid,
	                -1, -1, -1, -1,
	                NULL, 0, 0, FALSE,
	                wlc_scan_test_done, scan_info,
	                SCANCACHE_ENAB(wlc_scan_info),
	                0, NULL, SCAN_ENGINE_USAGE_NORM, NULL, NULL, NULL);
}
#endif	/* BCMDBG */

#ifdef WLC_SCAN_IOVARS
/* get the version of scan that is supported */
	void
wlc_iov_get_scan_ver(wl_scan_version_t *ver)
{
	ASSERT(ver);

	ver->version = WL_SCAN_VERSION_T_VERSION;
	ver->length = sizeof(wl_scan_version_t);

	/* set SCAN interface version numbers */
	ver->scan_ver_major = WL_SCAN_VERSION_MAJOR;
}
static int
wlc_scan_doiovar(void *hdl, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif)
{
	scan_info_t *scan_info = (scan_info_t *)hdl;
	int err = 0;
	int32 int_val = 0;
	bool bool_val = FALSE;
	int32 *ret_int_ptr;
	wlc_info_t *wlc = scan_info->wlc;

	BCM_REFERENCE(wlc);
	BCM_REFERENCE(val_size);
	BCM_REFERENCE(wlcif);

	/* convenience int and bool vals for first 4 bytes of buffer */
	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));
	bool_val = (int_val != 0) ? TRUE : FALSE;
	BCM_REFERENCE(bool_val);

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	/* Do the actual parameter implementation */
	switch (actionid) {
	case IOV_GVAL(IOV_PASSIVE):
		*ret_int_ptr = (int32)scan_info->defaults.passive;
		break;

	case IOV_SVAL(IOV_PASSIVE):
		scan_info->defaults.passive = (int8)int_val;
		break;

#ifdef STA
	case IOV_GVAL(IOV_SCAN_ASSOC_TIME):
		*ret_int_ptr = (int32)scan_info->defaults.assoc_time;
		break;

	case IOV_SVAL(IOV_SCAN_ASSOC_TIME):
		scan_info->defaults.assoc_time = (uint16)int_val;
		break;

	case IOV_GVAL(IOV_SCAN_UNASSOC_TIME):
		*ret_int_ptr = (int32)scan_info->defaults.unassoc_time;
		break;

	case IOV_SVAL(IOV_SCAN_UNASSOC_TIME):
		scan_info->defaults.unassoc_time = (uint16)int_val;
		break;
#endif /* STA */

	case IOV_GVAL(IOV_SCAN_PASSIVE_TIME):
		*ret_int_ptr = (int32)scan_info->defaults.passive_time;
		break;

	case IOV_SVAL(IOV_SCAN_PASSIVE_TIME):
		scan_info->defaults.passive_time = (uint16)int_val;
		break;

#ifdef STA
	case IOV_GVAL(IOV_SCAN_NPROBES):
		*ret_int_ptr = (int32)scan_info->defaults.nprobes;
		break;

	case IOV_SVAL(IOV_SCAN_NPROBES):
		scan_info->defaults.nprobes = (int8)int_val;
		break;

#ifdef WLSCANCACHE
	case IOV_GVAL(IOV_SCANCACHE):
		*ret_int_ptr = scan_info->scan_pub->_scancache;
		break;

	case IOV_SVAL(IOV_SCANCACHE):
		if (SCANCACHE_SUPPORT(wlc)) {
			scan_info->scan_pub->_scancache = bool_val;
#ifdef WL11K
			/* Enable Table mode beacon report in RRM cap if scancache enabled */
			if (WL11K_ENAB(wlc->pub)) {
				wlc_bsscfg_t *cfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);

				ASSERT(cfg != NULL);

				wlc_rrm_update_cap(wlc->rrm_info, cfg);
			}
#endif /* WL11K */
		}
		else
			err = BCME_UNSUPPORTED;
		break;

	case IOV_GVAL(IOV_SCANCACHE_TIMEOUT):
		if (SCANCACHE_ENAB(scan_info->scan_pub))
			*ret_int_ptr = (int32)wlc_scandb_timeout_get(scan_info->sdb);
		else
			err = BCME_UNSUPPORTED;
		break;

	case IOV_SVAL(IOV_SCANCACHE_TIMEOUT):
		if (SCANCACHE_ENAB(scan_info->scan_pub))
			wlc_scandb_timeout_set(scan_info->sdb, (uint)int_val);
		else
			err = BCME_UNSUPPORTED;
		break;

	case IOV_SVAL(IOV_SCANCACHE_CLEAR):
		/* scancache might be disabled while clearing the cache.
		 * So check for scancache_support instead of scancache_enab.
		 */
		if (SCANCACHE_SUPPORT(wlc))
			wlc_scandb_clear(scan_info->sdb);
		else
			err = BCME_UNSUPPORTED;
		break;

#endif /* WLSCANCACHE */

	case IOV_GVAL(IOV_SCAN_ASSOC_TIME_DEFAULT):
		*ret_int_ptr = WLC_SCAN_ASSOC_TIME;
		break;

	case IOV_GVAL(IOV_SCAN_UNASSOC_TIME_DEFAULT):
		*ret_int_ptr = WLC_SCAN_UNASSOC_TIME;
		break;

#endif /* STA */

	case IOV_GVAL(IOV_SCAN_PASSIVE_TIME_DEFAULT):
		*ret_int_ptr = WLC_SCAN_PASSIVE_TIME;
		break;
#ifdef BCMDBG
	case IOV_SVAL(IOV_SCAN_DBG):
		scan_info->debug = (uint8)int_val;
		break;
	case IOV_SVAL(IOV_SCAN_TEST):
		err = wlc_scan_test(scan_info, (uint8)int_val);
		break;
#endif // endif

#ifdef WLSCAN_PS
#if defined(BCMDBG) || defined(WLTEST)
	case IOV_GVAL(IOV_SCAN_RX_PWRSAVE):
		if (WLSCAN_PS_ENAB(wlc->pub)) {
			*ret_int_ptr = (uint8)scan_info->scan_rx_pwrsave;
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;

	case IOV_SVAL(IOV_SCAN_RX_PWRSAVE):
		if (WLSCAN_PS_ENAB(wlc->pub)) {
			switch (int_val) {
				case 0:
				case 1:
					scan_info->scan_rx_pwrsave = (uint8)int_val;
					break;
				default:
					err = BCME_BADARG;
			}
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;

	case IOV_GVAL(IOV_SCAN_TX_PWRSAVE):
		if (WLSCAN_PS_ENAB(wlc->pub)) {
			*ret_int_ptr = (uint8)scan_info->scan_tx_pwrsave;
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;

	case IOV_SVAL(IOV_SCAN_TX_PWRSAVE):
		if (WLSCAN_PS_ENAB(wlc->pub)) {
			switch (int_val) {
				case 0:
				case 1:
					scan_info->scan_tx_pwrsave = (uint8)int_val;
					break;
				default:
					err = BCME_BADARG;
			}
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;
#endif /* defined(BCMDBG) || defined(WLTEST) */
	case IOV_GVAL(IOV_SCAN_PWRSAVE):
		*ret_int_ptr = (uint8)scan_info->pwrsave_enable;
		break;
	case IOV_SVAL(IOV_SCAN_PWRSAVE):
		if (int_val < 0 || int_val > 1)
			err = BCME_BADARG;
		else
			scan_info->pwrsave_enable = (uint8)int_val;
		break;
#endif /* WLSCAN_PS */
	case IOV_GVAL(IOV_SCANMAC):
		err = scanmac_get(scan_info, params, p_len, arg, len);
		break;
	case IOV_SVAL(IOV_SCANMAC):
		err = scanmac_set(scan_info, arg, len);
		break;

	case IOV_SVAL(IOV_SCAN_SUPPRESS):
		if (bool_val)
			scan_info->scan_pub->state |= SCAN_STATE_SUPPRESS;
		else
			scan_info->scan_pub->state &= ~SCAN_STATE_SUPPRESS;
		break;

	case IOV_GVAL(IOV_SCAN_SUPPRESS):
		*ret_int_ptr = scan_info->scan_pub->state & SCAN_STATE_SUPPRESS ? 1 : 0;
		break;

	case IOV_GVAL(IOV_PASSIVE_ON_RESTRICTED_MODE):
		*ret_int_ptr = (int32)scan_info->scan_pub->passive_on_restricted;
		break;

	case IOV_SVAL(IOV_PASSIVE_ON_RESTRICTED_MODE):
		scan_info->scan_pub->passive_on_restricted = (uint8) int_val;
		break;
#ifdef WL_SCAN_DFS_HOME
	case IOV_GVAL(IOV_SCAN_DFS_HOME_AWAY_DURATION):
		*ret_int_ptr = (uint8)scan_info->scan_dfs_away_duration;
		break;
	case IOV_SVAL(IOV_SCAN_DFS_HOME_AWAY_DURATION):
		scan_info->scan_dfs_away_duration = (uint8)int_val;
		break;
	case IOV_GVAL(IOV_SCAN_DFS_HOME_MIN_DWELL):
		*ret_int_ptr = (uint16)scan_info->scan_dfs_min_dwell;
		break;
	case IOV_SVAL(IOV_SCAN_DFS_HOME_MIN_DWELL):
		scan_info->scan_dfs_min_dwell = (uint16)int_val;
		break;
	case IOV_GVAL(IOV_SCAN_DFS_HOME_AUTO_REDUCE):
		*ret_int_ptr = (uint8)scan_info->scan_dfs_auto_reduce;
		break;
	case IOV_SVAL(IOV_SCAN_DFS_HOME_AUTO_REDUCE):
		scan_info->scan_dfs_auto_reduce = (uint8)int_val;
		break;
#endif /* WL_SCAN_DFS_HOME */

#if BAND6G
	case IOV_GVAL(IOV_SCAN_RNR_LIST):
		if ((p_len < sizeof(rnr_list_req_t)) ||
		    (len < sizeof(rnr_list_req_t))) {
			err = BCME_BUFTOOSHORT;
		} else {
			rnr_list_req_t *rnr_list_req = (rnr_list_req_t *) params;
			rnr_list_req_t *rnr_list_res = (rnr_list_req_t *) arg;
			rnr_list_res->ver = 0;
			rnr_list_res->len = 0;
			rnr_list_res->count = 0;
			rnr_list_res->cmd_type = rnr_list_req->cmd_type;
			wlc_scan_6g_get_prof_list(wlc->scan, rnr_list_res);
		}
		break;
	case IOV_SVAL(IOV_SCAN_RNR_LIST):
		if ((p_len < sizeof(rnr_list_req_t)) ||
		    (len < (int)sizeof(rnr_list_req_t))) {
			err = BCME_BUFTOOSHORT;
		} else {
			rnr_list_req_t *rnr_list_req = (rnr_list_req_t *) params;
			if (rnr_list_req->cmd_type == RNR_LIST_DEL) {
				wlc_scan_6g_delete_prof_list(wlc->scan);
				break;
			} else if (rnr_list_req->cmd_type != RNR_LIST_SET) {
				err = BCME_UNSUPPORTED;
				break;
			}
			err = wlc_scan_rnr_list_set(wlc, rnr_list_req);
		}
		break;
#endif /* BAND6G */
	case IOV_GVAL(IOV_SCAN_VER):
		/* set scan version */
		wlc_iov_get_scan_ver((wl_scan_version_t *)arg);
		break;
	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
} /* wlc_scan_doiovar */
#endif /* WLC_SCAN_IOVARS */

#if BAND6G
static int
wlc_scan_rnr_list_set(wlc_info_t *wlc, rnr_list_req_t *rnr_list_req)
{
	int i, j, err;
	wlc_scan_wlioctl_prof_list_t *prof_list =
		(wlc_scan_wlioctl_prof_list_t *)rnr_list_req->prof_list;
	for (i = 0; i < rnr_list_req->count; i++) {
		wlc_scan_wlioctl_prof_t *profile  =
			(wlc_scan_wlioctl_prof_t *)prof_list->profile;
		for (j = 0; j < prof_list->profile_count; j++) {
			wlc_scan_6g_chan_prof_t *cur_chan6g_prof =
				(wlc_scan_6g_chan_prof_t *)MALLOCZ(wlc->osh,
				sizeof(wlc_scan_6g_chan_prof_t));
			ASSERT(profile);
			if (cur_chan6g_prof != NULL) {
				uint32 calc_short_ssid = 0;
				if (profile->ssid_length) {
					memcpy(cur_chan6g_prof->SSID,
						profile->SSID, profile->ssid_length);
					cur_chan6g_prof->ssid_length = profile->ssid_length;
					//calculate short_ssid
					calc_short_ssid = wlc_calc_short_ssid(profile->SSID,
						profile->ssid_length);
					cur_chan6g_prof->is_short_ssid = TRUE;
					cur_chan6g_prof->short_ssid = calc_short_ssid;

					/* checking for ssid and short ssid match before
					 * creating or updating the element
					 */
					if (profile->is_short_ssid && (calc_short_ssid !=
						profile->short_ssid)) {
						WL_ERROR(("SSID and short_ssid donot match\n"));
						MFREE(wlc->osh, cur_chan6g_prof,
							sizeof(wlc_scan_6g_chan_prof_t));
						return BCME_ERROR;
					}
				}
				if (profile->is_short_ssid) {
					cur_chan6g_prof->is_short_ssid = profile->is_short_ssid;
					cur_chan6g_prof->short_ssid = profile->short_ssid;
				}
				if (profile->bssid_valid) {
					cur_chan6g_prof->bssid_valid = profile->bssid_valid;
					cur_chan6g_prof->bssid = profile->bssid;
				}
				cur_chan6g_prof->bss_params = profile->bss_params;
				cur_chan6g_prof->next_list = NULL;
				err = wlc_scan_6g_add_to_chan_prof_list(wlc, cur_chan6g_prof,
					prof_list->chanspec, prof_list->rnr);
				if (err) {
					return BCME_ERROR;
				}
			} else {
				return BCME_NOMEM;
			}
			profile++;
		}
		prof_list = (wlc_scan_wlioctl_prof_list_t *)profile;
	}
	return BCME_OK;
}
#endif /* BAND6G */

/** Active scanning requires the transmission of probe request. Invoked via the scan timer. */
static void
wlc_scan_sendprobe(wlc_info_t *wlc, void *arg, uint *dwell)
{
	wlc_scan_info_t *wlc_scan_info = wlc->scan;
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
	int i;
	wlc_ssid_t *ssid;
	wlc_bsscfg_t *cfg = scan_info->bsscfg;
	const struct ether_addr *da = &ether_bcast;
	const struct ether_addr *bssid = &ether_bcast;
#if BAND6G
	chanspec_t cur_send_chanspec = scan_info->cur_scan_chanspec;
#endif /* BAND6G */

#ifdef WL_OCE
	if (OCE_ENAB(wlc->pub)) {
		wlc_oce_set_max_channel_time(wlc->oce, *dwell);
	}
#endif /* WL_OCE */

	ASSERT(scan_info->pass >= 1);

	ssid = scan_info->ssid_list;
	for (i = 0; i < scan_info->nssid; i++) {
#ifdef WLFMC
		/* in case of roaming reassoc, use unicast scans */
		if (WLFMC_ENAB(wlc->pub) &&
			!ETHER_ISMULTI(&wlc_scan_info->bssid) &&
			(cfg->assoc->type == AS_ROAM) &&
			(cfg->roam->reason == WLC_E_REASON_INITIAL_ASSOC)) {
			da = &wlc_scan_info->bssid;
			bssid = &wlc_scan_info->bssid;
		}
#endif /* WLFMC */
		/* support to do unicast probe if bssid is specified */
		if (!ETHER_ISMULTI(&wlc_scan_info->bssid)) {
			da = &wlc_scan_info->bssid;
			bssid = &wlc_scan_info->bssid;
		}
#if BAND6G
		/* In 6G scan wildcard SSID/ wildcard BSSID scans are not allowed,
		* Need to send Directed probe request with either SSID / BSSID
		*/
		if (CHSPEC_IS6G(cur_send_chanspec)) {
			wlc_scan_handle_probereq_for_6g(scan_info->scan_pub, cfg, ssid, da);
		} else
#endif /* BAND6G */
		{
			wlc_sendprobe(wlc, cfg, ssid->SSID, ssid->SSID_len, 0, 0,
				&scan_info->sa_override, da, bssid, 0, NULL, 0);
		}
		ssid++;
	}
} /* wlc_scan_sendprobe */

/** Invoked by the scan timer */
static void
wlc_scan_do_pass(scan_info_t *scan_info, chanspec_t chanspec)
{
	uint32	channel_dwelltime = 0;
	uint32  start_tsf;
	char chanbuf[CHANSPEC_STR_LEN];
	wlc_info_t *wlc = scan_info->wlc;

	BCM_REFERENCE(chanbuf);
	BCM_REFERENCE(wlc);

	wlc_read_tsf(wlc, &start_tsf, NULL);
	scan_info->start_tsf = start_tsf;

	channel_dwelltime = scan_info->active_time/scan_info->npasses;
#if BAND6G
	if (CHSPEC_IS6G(scan_info->cur_scan_chanspec)) {
		channel_dwelltime = wlc_scan_get_probe_defer_time(scan_info,
			scan_info->cur_scan_chanspec);
	}
#endif /* BAND6G */
	WL_SCAN(("wl%d: active dwell time %d ms, chanspec %s, tsf %u\n",
		scan_info->unit, channel_dwelltime,
		wf_chspec_ntoa_ex(scan_info->cur_scan_chanspec, chanbuf),
		start_tsf));
	wlc_scan_act(scan_info, channel_dwelltime);

	/* record phy noise for the scan channel */
	/* XXX: remove noise sampling request for 4360 during active scanning
	 * as it is causing some issues in scanning with high BG_NOISE interrupts.
	 */
	if (D11REV_LT(wlc->pub->corerev, 40)) {
		wlc_lq_noise_sample_request(wlc, WLC_NOISE_REQUEST_SCAN,
			CHSPEC_CHANNEL(chanspec));
	}
}

/** Returns TRUE if the caller supplied SSID is in the list of scan results */
bool
wlc_scan_ssid_match(wlc_scan_info_t *scan_pub, uint8 *ssid_buf, uint8 ssid_len, bool filter)
{
	scan_info_t	*scan_info = (scan_info_t *)scan_pub->scan_priv;
	wlc_ssid_t	*ssid;
	int	i;
	char *c;

	if ((scan_info->nssid == 1) && ((scan_info->ssid_list[0]).SSID_len == 0)) {
		return TRUE;
	}

	if (scan_info->ssid_wildcard_enabled)
		return TRUE;

	if (!ssid_buf || ssid_len > DOT11_MAX_SSID_LEN) {
		return FALSE;
	}

	/* filter out beacons which have all spaces or nulls as ssid */
	if (filter) {
		if (ssid_len == 0)
			return FALSE;
		c = (char *)&ssid_buf[0];
		for (i = 0; i < ssid_len; i++) {
			if ((*c != 0) && (*c != ' '))
				break;
			c++;
		}
		if (i == ssid_len)
			return FALSE;
	}

	/* do not do ssid matching if we are sending out bcast SSIDs
	 * do the filtering before the scan_complete callback
	 */
	ssid = scan_info->ssid_list;
	for (i = 0; i < scan_info->nssid; i++) {
		if (WLC_IS_MATCH_SSID(ssid->SSID, ssid_buf, ssid->SSID_len, ssid_len))
			return TRUE;
#ifdef WLP2P
		if (P2P_ENAB(scan_info->wlc->pub) &&
		    wlc_p2p_ssid_match(scan_info->wlc->p2p, scan_info->bsscfg,
		                       ssid->SSID, ssid->SSID_len,
		                       ssid_buf, ssid_len))
			return TRUE;
#endif // endif
		ssid++;
	}

	return FALSE;
} /* wlc_scan_ssid_match */

/** Informs module wlc_obss.c */
static void
wlc_ht_obss_scan_update(scan_info_t *scan_info, int status)
{
	wlc_info_t *wlc = scan_info->wlc;
	wlc_bsscfg_t *cfg;
	uint8 chanvec[OBSS_CHANVEC_SIZE]; /* bitvec of channels in 2G */
	uint8 chan, i;
	uint8 n_channels = 0;

	WL_TRACE(("wl%d: wlc_ht_obss_scan_update\n", scan_info->unit));

	cfg = scan_info->bsscfg;

	/* checking  for valid fields */
	if (!wlc_obss_scan_fields_valid(wlc->obss, cfg)) {
		return;
	}

	if (!wlc_obss_is_scan_complete(wlc->obss, cfg, (status == WLC_E_STATUS_SUCCESS),
		scan_info->active_time, scan_info->passive_time)) {
		return;
	}

	bzero(chanvec, OBSS_CHANVEC_SIZE);
	for (i = 0; i < scan_info->channel_num; i++) {
		chanspec_t chspec = scan_info->chanspec_list[i];
		chan = wf_chspec_primary20_chan(chspec);
		if (CHSPEC_IS2G(chspec) && chan <= CH_MAX_2G_CHANNEL) {
			setbit(chanvec, chan);
			n_channels++;
		}
	}

	wlc_obss_scan_update_countdown(wlc->obss, cfg, chanvec, n_channels);

	/* XXX AP need to take the scan result and find a channel to
	 * operate
	 */
}

#ifdef WLSCANCACHE
int32
wlc_scan_add_bss_cache(wlc_scan_info_t *wlc_scan_info, wlc_bss_info_t *bi)
{
	scan_info_t *scan_info = (scan_info_t *) wlc_scan_info->scan_priv;

	return wlc_scandb_add(scan_info->sdb, bi);
} /* wlc_scan_add_bss_cache */

/** Add the current wlc_scan_info->scan_results to the scancache */
int32
wlc_scan_fill_cache(wlc_scan_info_t *wlc_scan_info, wlc_bss_list_t *scan_results, wlc_scandb_t *sdb,
	uint current_timestamp)
{
	uint index;
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
	wlc_bss_info_t *bi;
	int32 ret = BCME_OK;

	/* walk the list of scan results, adding each to the cache */
	for (index = 0; index < scan_results->count; index++) {
		bi = scan_results->ptrs[index];
		if (bi == NULL) continue;

		/* Add the BSS to the DB */
		ret = wlc_scandb_add(scan_info->sdb, bi);
	}

	return (ret);
}

/**
 * Return the contents of the scancache in the 'bss_list' param.
 *
 * Return only those scan results that match the criteria specified by the other params:
 *
 * BSSID:	match the provided BSSID exactly unless BSSID is a NULL pointer or FF:FF:FF:FF:FF:FF
 * nssid:	nssid number of ssids in the array pointed to by SSID
 * SSID:	match [one of] the provided SSIDs exactly unless SSID is a NULL pointer,
 *		SSID[0].SSID_len == 0 (broadcast SSID), or nssid = 0 (no SSIDs to match)
 * BSS_type:	match the 802.11 infrastructure type. Should be one of the values:
 *		{DOT11_BSSTYPE_INFRASTRUCTURE, DOT11_BSSTYPE_INDEPENDENT, DOT11_BSSTYPE_ANY}
 * chanspec_list, chanspec_num: if chanspec_num == 0, no channel filtering is done. Otherwise
 *		the chanspec list should contain 20MHz chanspecs. Only BSSs with a matching channel,
 *		or for a 40MHz BSS, with a matching control channel, will be returned.
 */
void
wlc_scan_get_cache(wlc_scan_info_t *scan_pub,
                   const struct ether_addr *BSSID, int nssid, const wlc_ssid_t *SSID,
                   int BSS_type, const chanspec_t *chanspec_list, uint chanspec_num,
                   wlc_bss_list_t *bss_list)
{
	scan_iter_params_t params;
	scan_info_t *scan_info = (scan_info_t *)scan_pub->scan_priv;

	params.merge = FALSE;
	params.bss_list = bss_list;
	params.current_ts = 0;

	memset(bss_list, 0, sizeof(wlc_bss_list_t));

	/* ageout any old entries */
	wlc_scandb_ageout(scan_info->sdb, OSL_SYSUPTIME());

	wlc_scandb_iterate(scan_info->sdb,
	                   BSSID, nssid, SSID, BSS_type, chanspec_list, chanspec_num,
	                   wlc_scan_build_cache_list, scan_info, &params);
}

/**
 * Merge the contents of the scancache with entries already in the 'bss_list' param.
 *
 * Return only those scan results that match the criteria specified by the other params:
 *
 * current_timestamp: timestamp matching the most recent additions to the cache. Entries with
 *		this timestamp will not be added to bss_list.
 * BSSID:	match the provided BSSID exactly unless BSSID is a NULL pointer or FF:FF:FF:FF:FF:FF
 * nssid:	nssid number of ssids in the array pointed to by SSID
 * SSID:	match [one of] the provided SSIDs exactly unless SSID is a NULL pointer,
 *		SSID[0].SSID_len == 0 (broadcast SSID), or nssid = 0 (no SSIDs to match)
 * BSS_type:	match the 802.11 infrastructure type. Should be one of the values:
 *		{DOT11_BSSTYPE_INFRASTRUCTURE, DOT11_BSSTYPE_INDEPENDENT, DOT11_BSSTYPE_ANY}
 * chanspec_list, chanspec_num: if chanspec_num == 0, no channel filtering is done. Otherwise
 *		the chanspec list should contain 20MHz chanspecs. Only BSSs with a matching channel,
 *		or for a 40MHz BSS, with a matching control channel, will be returned.
 */
static void
wlc_scan_merge_cache(scan_info_t *scan_info, uint current_timestamp,
                   const struct ether_addr *BSSID, int nssid, const wlc_ssid_t *SSID,
                   int BSS_type, const chanspec_t *chanspec_list, uint chanspec_num,
                   wlc_bss_list_t *bss_list)
{
	scan_iter_params_t params;

	params.merge = TRUE;
	params.bss_list = bss_list;
	params.current_ts = current_timestamp;

	wlc_scandb_iterate(scan_info->sdb,
	                   BSSID, nssid, SSID, BSS_type, chanspec_list, chanspec_num,
	                   wlc_scan_build_cache_list, scan_info, &params);
}

static void
wlc_scan_build_cache_list(void *arg1, void *arg2, uint timestamp,
                          struct ether_addr *BSSID, wlc_ssid_t *SSID,
                          int BSS_type, chanspec_t chanspec, void *data, uint datalen)
{
	scan_info_t *scan_info = (scan_info_t*)arg1;
	scan_iter_params_t *params = (scan_iter_params_t*)arg2;
	wlc_bss_list_t *bss_list = params->bss_list;
	wlc_bss_info_t *bi;
	wlc_bss_info_t *cache_bi;

	BCM_REFERENCE(chanspec);
	BCM_REFERENCE(BSSID);
	BCM_REFERENCE(SSID);
	BCM_REFERENCE(BSS_type);

	/* skip the most recent batch of results when merging the cache to a bss_list */
	if (params->merge &&
	    params->current_ts == timestamp)
		return;

	if (bss_list->count >= scan_info->wlc->pub->tunables->maxbss)
		return;

	bi = MALLOC(scan_info->osh, sizeof(*bi));
	if (!bi) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
		          scan_info->unit, __FUNCTION__, MALLOCED(scan_info->osh)));
		return;
	}

	ASSERT(data != NULL);
	ASSERT(datalen >= sizeof(wlc_bss_info_t));

	cache_bi = (wlc_bss_info_t*)data;

	memcpy(bi, cache_bi, sizeof(*bi));
	if (cache_bi->bcn_prb_len) {
		ASSERT(datalen >= sizeof(wlc_bss_info_t) + bi->bcn_prb_len);
		if (!(bi->bcn_prb = MALLOC(scan_info->osh, bi->bcn_prb_len))) {
			WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
				scan_info->unit, __FUNCTION__, MALLOCED(scan_info->osh)));
			MFREE(scan_info->osh, bi, sizeof(*bi));
			return;
		}
		/* Source is a flattened out structure but its bcn_prb pointer is not fixed
		 * when the entry was added to scancache db. So find out the new location.
		 */
		cache_bi->bcn_prb = (struct dot11_bcn_prb*)((uchar *) data +
		                                            sizeof(wlc_bss_info_t));

		memcpy(bi->bcn_prb, cache_bi->bcn_prb, bi->bcn_prb_len);
	}

	bss_list->ptrs[bss_list->count++] = bi;
} /* wlc_scan_build_cache_list */

static void
wlc_scan_cache_result(scan_info_t *scan_info)
{
	wlc_scan_info_t	*wlc_scan_info = scan_info->scan_pub;
	wlc_info_t *wlc = scan_info->wlc;
	uint timestamp = OSL_SYSUPTIME();

	/* if we have scan caching enabled, enter these results in the cache */
	wlc_scan_fill_cache(wlc_scan_info, wlc->scan_results, scan_info->sdb, timestamp);

	/* Provide the latest results plus cached results if they were requested. */
	if (wlc_scan_info->state & SCAN_STATE_INCLUDE_CACHE) {
		/* Merge cached results with current results */
		wlc_scan_merge_cache(scan_info, timestamp,
		                     &wlc_scan_info->bssid,
		                     scan_info->nssid, &scan_info->ssid_list[0],
		                     wlc_scan_info->bss_type,
		                     scan_info->chanspec_list, scan_info->channel_num,
		                     wlc->scan_results);

		WL_SCAN(("wl%d: %s: Merged scan results with cache, new total %d\n",
		         scan_info->unit, __FUNCTION__, wlc->scan_results->count));
	}
} /* wlc_scan_cache_result */

#endif /* WLSCANCACHE */

/** Called once per second. Don't confuse this with the scan timer. */
static void
wlc_scan_watchdog(void *hdl)
{
#ifdef WLSCANCACHE
	scan_info_t *scan = (scan_info_t *)hdl;

	/* ageout any old entries to free up memory */
	if (SCANCACHE_ENAB(scan->scan_pub))
		wlc_scandb_ageout(scan->sdb, OSL_SYSUPTIME());
#endif // endif
}

wlc_bsscfg_t *
wlc_scan_bsscfg(wlc_scan_info_t *wlc_scan_info)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
	return scan_info->bsscfg;
}

#ifdef WLSCAN_PS
#ifdef WL_STF_ARBITRATOR
/**
 * This function configures tx & rxcores to save power when STF arbitrator is enabled.
 * @param[in] flag   TRUE to set & FALSE to revert config
 */
static void
wlc_scan_ps_config_cores_arb(scan_info_t *scan_info, bool flag)
{
	wlc_info_t *wlc = scan_info->wlc;
	uint8 rxchains = 0, txchains = 0;

	/* Default to full chains */
	txchains = wlc->stf->hw_txchain;
	rxchains = wlc->stf->hw_rxchain;

	/* Scanning is started */
	/* On activate, adjust as specified by scan_ps iovar and mimo_override flag */
	if (!scan_info->mimo_override && scan_info->pwrsave_enable) {
		txchains = ONE_CHAIN_CORE0;
		rxchains = ONE_CHAIN_CORE0;
	}
	/* Pass the selected configuration to the STF arbitrator */
	wlc_stf_nss_request_update(wlc, scan_info->stf_scan_req,
		(flag ? WLC_STF_ARBITRATOR_REQ_STATE_RXTX_ACTIVE :
			WLC_STF_ARBITRATOR_REQ_STATE_RXTX_INACTIVE),
		txchains, WLC_BITSCNT(txchains), rxchains, WLC_BITSCNT(rxchains));
}
#endif /* WL_STF_ARBITRATOR */

/**
 * This function configures tx & rxcores to save power when STF arbitrator is disabled.
 * @param[in] flag   TRUE to set & FALSE to revert config
 */
static int
wlc_scan_ps_config_cores_non_arb(scan_info_t *scan_info, bool flag)
{
	int idx;
	wlc_bsscfg_t *cfg;
	wlc_info_t *wlc = scan_info->wlc;

	/* bail out if both scan tx & rx pwr opt. are disabled */
	if (!scan_info->scan_tx_pwrsave &&
	    !scan_info->scan_rx_pwrsave) {
		WL_SCAN(("wl%d: %s(%d): tx_ps %d rx_ps %d\n",
		                scan_info->unit, __FUNCTION__, __LINE__,
		                scan_info->scan_tx_pwrsave,
		                scan_info->scan_rx_pwrsave));
		return BCME_OK;
	}

	/* enable cores only when device is in PM = 1 or 2 mode */
	FOREACH_AS_STA(wlc, idx, cfg) {
		if (cfg->BSS && cfg->pm->PM == PM_OFF) {
			WL_SCAN(("wl%d: %s(%d): pm %d\n",
				scan_info->unit, __FUNCTION__, __LINE__, cfg->pm->PM));

			/* If PM becomes 0 after scan initiated,
			  * we need to reset the cores
			  */
			if (!scan_info->scan_ps_rxchain &&
			    !scan_info->scan_ps_txchain) {
				WL_SCAN(("wl%d: %s(%d): txchain %d rxchain %d\n",
				    scan_info->unit, __FUNCTION__, __LINE__,
				    scan_info->scan_ps_txchain, scan_info->scan_ps_rxchain));
				return BCME_ERROR;
			}
		}
	}

	wlc_suspend_mac_and_wait(wlc);

	if (flag) {
		/* Scanning is started */
		if ((
#if defined(BCMDBG) || defined(WLTEST)
			scan_info->scan_tx_pwrsave ||
#endif /* defined(BCMDBG) || defined(WLTEST) */
			scan_info->pwrsave_enable) && !scan_info->scan_ps_txchain) {
		/* if txchains doesn't match with hw defaults, don't modify chain mask
		  * and also ignore for 1x1. scan pwrsave iovar should be enabled otherwise ignore.
		  */
			if (wlc_stf_txchain_ishwdef(wlc) &&
				wlc->stf->hw_txchain >= 0x03) {
				wlc_suspend_mac_and_wait(wlc);
				/* back up chain configuration */
				scan_info->scan_ps_txchain = wlc->stf->txchain;
				wlc_stf_txchain_set(wlc, 0x1, FALSE, WLC_TXCHAIN_ID_USR);
				wlc_enable_mac(wlc);
			}
		}
		if ((
#if defined(BCMDBG) || defined(WLTEST)
			scan_info->scan_rx_pwrsave ||
#endif /* defined(BCMDBG) || defined(WLTEST) */
			scan_info->pwrsave_enable) && !scan_info->scan_ps_rxchain) {
		/* if rxchain doesn't match with hw defaults, don't modify chain mask
		  * and also ignore for 1x1. scan pwrsave iovar should be enabled otherwise ignore.
		  */
			if (wlc_stf_rxchain_ishwdef(wlc) &&
				wlc->stf->hw_rxchain >= 0x03) {
				wlc_suspend_mac_and_wait(wlc);
				/* back up chain configuration */
				scan_info->scan_ps_rxchain = wlc->stf->rxchain;
				wlc_stf_rxchain_set(wlc, 0x1, TRUE);
				wlc_enable_mac(wlc);
			}
		}
	} else {
		/* Scanning is ended */
		if (!scan_info->scan_ps_txchain && !scan_info->scan_ps_rxchain) {
			return BCME_OK;
		} else {
			/* when scan_ps_txchain is 0, it mean scan module has not modified chains */
			if (scan_info->scan_ps_txchain) {
				wlc_suspend_mac_and_wait(wlc);
				wlc_stf_txchain_set(wlc, scan_info->scan_ps_txchain,
				                    FALSE, WLC_TXCHAIN_ID_USR);
				scan_info->scan_ps_txchain = 0;
				wlc_enable_mac(wlc);
			}
			if (scan_info->scan_ps_rxchain) {
				wlc_suspend_mac_and_wait(wlc);
				wlc_stf_rxchain_set(wlc, scan_info->scan_ps_rxchain, TRUE);
				/* make the chain value to 0 */
				scan_info->scan_ps_rxchain = 0;
				wlc_enable_mac(wlc);
			}
		}
	}
	return BCME_OK;
} /* wlc_scan_ps_config_cores_non_arb */

/** reduces power consumption during scan */
static int
wlc_scan_ps_config_cores(scan_info_t *scan_info, bool flag)
{
	if (WL_OPS_ENAB(scan_info->wlc->pub)) {
		wlc_ops_update_scan_disable_reqs(scan_info->wlc->ops_info, flag);
	}
#ifdef WL_STF_ARBITRATOR
	if (WLC_STF_ARB_ENAB(scan_info->wlc->pub)) {
		/* XXX Currently arbitrator version is very different: respects only one config,
		 * not separate rx/tx; requests full chains when no pwrsave; needn't remember
		 * previous setting (just goes inactive), and needn't be wrapped with suspend.
		 * So actually no overlap (yet!) except the return value...
		 */
		wlc_scan_ps_config_cores_arb(scan_info, flag);
		return BCME_OK;
	} else
#endif /* WL_STF_ARBITRATOR */
	{
		return wlc_scan_ps_config_cores_non_arb(scan_info, flag);
	}
}

#endif /* WLSCAN_PS */

#if defined(BCMDBG)
static int
wlc_scan_dump(scan_info_t *si, struct bcmstrbuf *b)
{
	const bcm_bit_desc_t scan_flags[] = {
		{SCAN_STATE_SUPPRESS, "SUPPRESS"},
		{SCAN_STATE_SAVE_PRB, "SAVE_PRB"},
		{SCAN_STATE_PASSIVE, "PASSIVE"},
		{SCAN_STATE_WSUSPEND, "WSUSPEND"},
		{SCAN_STATE_RADAR_CLEAR, "RADAR_CLEAR"},
		{SCAN_STATE_PSPEND, "PSPEND"},
		{SCAN_STATE_DLY_WSUSPEND, "DLY_WSUSPEND"},
		{SCAN_STATE_READY, "READY"},
		{SCAN_STATE_INCLUDE_CACHE, "INC_CACHE"},
		{SCAN_STATE_PROHIBIT, "PROHIBIT"},
		{SCAN_STATE_IN_TMR_CB, "IN_TMR_CB"},
		{SCAN_STATE_OFFCHAN, "OFFCHAN"},
		{SCAN_STATE_TERMINATE, "TERMINATE"},
		{0, NULL}
	};
	const char *scan_usage[] = {
		"normal",
		"escan",
	};
	char state_str[64];
	char ssidbuf[SSID_FMT_BUF_LEN];
	char eabuf[ETHER_ADDR_STR_LEN];
	const char *bss_type_str;
	uint32 tsf_l, tsf_h;
	struct wlc_scan_info *scan_pub = si->scan_pub;

	wlc_format_ssid(ssidbuf, si->ssid_list[0].SSID, si->ssid_list[0].SSID_len);
	bcm_format_flags(scan_flags, scan_pub->state, state_str, 64);

	bss_type_str = wlc_bsstype_dot11name(scan_pub->bss_type);

	bcm_bprintf(b, "in_progress %d SSID \"%s\" type %s BSSID %s state 0x%x [%s] "
	            "usage %u [%s]\n",
	            scan_pub->in_progress, ssidbuf, bss_type_str,
	            bcm_ether_ntoa(&scan_pub->bssid, eabuf),
	            scan_pub->state, state_str,
	            scan_pub->usage,
	            scan_pub->usage < ARRAYSIZE(scan_usage) ?
	            scan_usage[scan_pub->usage] : "unknown");

	if (SCAN_IN_PROGRESS(scan_pub))
		bcm_bprintf(b, "wlc->home_chanspec: %x chanspec_current %x\n",
		            si->wlc->home_chanspec, si->chanspec_list[si->channel_idx]);

	if (si->wlc->pub->up) {
		wlc_read_tsf(si->wlc, &tsf_l, &tsf_h);
		bcm_bprintf(b, "start_tsf 0x%08x current tsf 0x%08x\n", si->start_tsf, tsf_l);
	} else {
		bcm_bprintf(b, "start_tsf 0x%08x current tsf <not up>\n", si->start_tsf);
	}

	return 0;
} /* wlc_scan_dump */
#endif // endif

/**
 * scanmac enable. Instead of using the HW MAC for scanning, allows scans to use a configured MAC
 * address (or randomized for privacy concerns).
 */
static int
scanmac_enable(scan_info_t *scan_info, int enable)
{
	int err = BCME_OK;
	wlc_info_t *wlc = scan_info->wlc;
	wlc_info_t *wlc_other = NULL;
	scan_info_t *scan_info_other = NULL;
	wlc_bsscfg_t *bsscfg = NULL;
	bool state = scan_info->scanmac_bsscfg != NULL;

	UNUSED_PARAMETER(wlc_other);
	UNUSED_PARAMETER(scan_info_other);

	if (enable) {
		int idx;
		uint32 flags = WLC_BSSCFG_NOIF | WLC_BSSCFG_NOBCMC;
		wlc_bsscfg_type_t type = {BSSCFG_TYPE_GENERIC, BSSCFG_GENERIC_STA};

		/* scanmac already enabled */
		if (state) {
			WL_SCAN(("wl%d: scanmac already enabled\n", wlc->pub->unit));
			return BCME_OK;
		}

		/* bsscfg with the MAC address exists */
		if (wlc_bsscfg_find_by_hwaddr(wlc, &scan_info->scanmac_config.mac) != NULL) {
			WL_SCAN_ERROR(("wl%d: MAC address is in use\n", wlc->pub->unit));
			return BCME_BUSY;
		}

		/* allocate bsscfg */
		if ((idx = wlc_bsscfg_get_free_idx(wlc)) == -1) {
			WL_ERROR(("wl%d: no free bsscfg\n", wlc->pub->unit));
			return BCME_NORESOURCE;
		}
		else if ((bsscfg = wlc_bsscfg_alloc(wlc, idx, &type, flags, 0,
		                &scan_info->scanmac_config.mac)) == NULL) {
			WL_ERROR(("wl%d: cannot create bsscfg\n", wlc->pub->unit));
			return BCME_NOMEM;
		}
		else if (wlc_bsscfg_init(wlc, bsscfg) != BCME_OK) {
			WL_SCAN_ERROR(("wl%d: cannot init bsscfg\n", wlc->pub->unit));
			err = BCME_ERROR;
			goto free;
		}
		memcpy(&bsscfg->BSSID, &bsscfg->cur_etheraddr, ETHER_ADDR_LEN);
		bsscfg->current_bss->bss_type = DOT11_BSSTYPE_INDEPENDENT;
		scan_info->scanmac_bsscfg = bsscfg;
	}
	else {
		wlc_scan_info_t *wlc_scan_info = wlc->scan;

		/* scanmac not enabled */
		if (!state) {
			WL_SCAN(("wl%d: scanmac is already disabled\n", wlc->pub->unit));
			return BCME_OK;
		}

		bsscfg = scan_info->scanmac_bsscfg;
		ASSERT(bsscfg != NULL);

		wlc_scan_abort(wlc_scan_info, WLC_E_STATUS_ABORT);

	free:
		scan_info->scanmac_bsscfg = NULL;
		memset(&scan_info->scanmac_config, 0, sizeof(scan_info->scanmac_config));

		/* free bsscfg + error handling */
		if (bsscfg != NULL)
			wlc_bsscfg_free(wlc, bsscfg);
	}

	return err;
} /* scanmac_enable */

/** For privacy concerns. Gets random mac address to be used in scanning. */
static void
scanmac_random_mac(scan_info_t *scan_info, struct ether_addr *mac)
{
	wlc_info_t *wlc = scan_info->wlc;

	/* HW random generator only available if core is up */
	wlc_getrand(wlc, (uint8 *)mac, ETHER_ADDR_LEN);
}

/** update configured or randomized MAC */
static void
scanmac_update_mac(scan_info_t *scan_info)
{
	wlc_info_t *wlc = scan_info->wlc;
	wlc_info_t *wlc_other = NULL;
	scan_info_t *scan_info_other = NULL;
	wlc_bsscfg_t *bsscfg = scan_info->scanmac_bsscfg;
	wl_scanmac_config_t *scanmac_config = &scan_info->scanmac_config;
	struct ether_addr *mac = &scanmac_config->mac;
	struct ether_addr *mask = &scanmac_config->random_mask;
	struct ether_addr random_mac;

	UNUSED_PARAMETER(wlc_other);
	UNUSED_PARAMETER(scan_info_other);

	/* generate random MAC if random mask is non-zero */
	if (!ETHER_ISNULLADDR(mask)) {
		struct ether_addr *prefix = &scanmac_config->mac;
		int i;
start_random_mac:
		scanmac_random_mac(scan_info, &random_mac);
		for (i = 0; i < ETHER_ADDR_LEN; i++) {
			/* AND the random mask bits */
			random_mac.octet[i] &= mask->octet[i];
			/* OR the MAC prefix */
			random_mac.octet[i] |= prefix->octet[i] & ~mask->octet[i];
		}

		/* randomize again if MAC is not unique accoss bsscfgs */
		if (wlc_bsscfg_find_by_hwaddr(wlc, &random_mac) != NULL) {
			WL_INFORM(("wl%d: scanmac_update_mac: regenerate random MAC\n",
				scan_info->unit));
			goto start_random_mac;
		}

		mac = &random_mac;
		scan_info->is_scanmac_config_updated = TRUE;
	}

	if (scan_info->is_scanmac_config_updated) {
		/* activate new MAC */
		memcpy(&bsscfg->BSSID, mac, ETHER_ADDR_LEN);
		wlc_validate_mac(wlc, bsscfg, mac);
		scan_info->is_scanmac_config_updated = FALSE;
	}
} /* scanmac_update_mac */

static int
scanmac_config(scan_info_t *scan_info, wl_scanmac_config_t *config)
{
	wlc_info_t *wlc = scan_info->wlc;
	wlc_bsscfg_t *bsscfg = scan_info->scanmac_bsscfg;

	if (scan_info->scanmac_bsscfg == NULL) {
		/* scanmac not enabled */
		return BCME_NOTREADY;
	}

	/* don't allow multicast MAC or random mask */
	if (ETHER_ISMULTI(&config->mac) || ETHER_ISMULTI(&config->random_mask)) {
		return BCME_BADADDR;
	}

	/* check if MAC exists */
	if (ETHER_ISNULLADDR(&config->random_mask)) {
		wlc_bsscfg_t *match = wlc_bsscfg_find_by_hwaddr(wlc, &config->mac);
		if (match != NULL && match != bsscfg) {
			WL_SCAN_ERROR(("wl%d: MAC address is in use\n", wlc->pub->unit));
			return BCME_BUSY;
		}
	}

	/* save config */
	memcpy(&scan_info->scanmac_config, config, sizeof(*config));
	scan_info->is_scanmac_config_updated = TRUE;

	/* update MAC if scan not in progress */
	if (!SCAN_IN_PROGRESS(scan_info->scan_pub)) {
		scanmac_update_mac(scan_info);
	}

	return BCME_OK;
} /* scanmac_config */

/** scanmac GET iovar */
static int
scanmac_get(scan_info_t *scan_info, void *params, uint p_len, void *arg, int len)
{
	int err = BCME_OK;
	wl_scanmac_t *sm = params;
	wl_scanmac_t *sm_out = arg;

	BCM_REFERENCE(len);

	/* verify length */
	if (p_len < OFFSETOF(wl_scanmac_t, data) ||
		sm->len > p_len - OFFSETOF(wl_scanmac_t, data)) {
		WL_SCAN_ERROR(("%s: Length verification failed len: %d sm->len: %d\n",
			__FUNCTION__, p_len, sm->len));
		return BCME_BUFTOOSHORT;
	}

	/* copy subcommand to output */
	sm_out->subcmd_id = sm->subcmd_id;

	/* process subcommand */
	switch (sm->subcmd_id) {
	case WL_SCANMAC_SUBCMD_ENABLE:
	{
		wl_scanmac_enable_t *sm_enable = (wl_scanmac_enable_t *)sm_out->data;
		sm_out->len = sizeof(*sm_enable);
		sm_enable->enable = scan_info->scanmac_bsscfg ? 1 : 0;
		break;
	}
	case WL_SCANMAC_SUBCMD_BSSCFG:
	{
		wl_scanmac_bsscfg_t *sm_bsscfg = (wl_scanmac_bsscfg_t *)sm_out->data;
		if (scan_info->scanmac_bsscfg == NULL) {
			return BCME_ERROR;
		} else {
			sm_out->len = sizeof(*sm_bsscfg);
			sm_bsscfg->bsscfg = WLC_BSSCFG_IDX(scan_info->scanmac_bsscfg);
		}
		break;
	}
	case WL_SCANMAC_SUBCMD_CONFIG:
	{
		wl_scanmac_config_t *sm_config = (wl_scanmac_config_t *)sm_out->data;
		sm_out->len = sizeof(*sm_config);
		memcpy(sm_config, &scan_info->scanmac_config, sizeof(*sm_config));
		break;
	}
	default:
		ASSERT(0);
		err = BCME_UNSUPPORTED;
		break;
	}
	return err;
} /* scanmac_get */

/** scanmac SET iovar */
static int
scanmac_set(scan_info_t *scan_info, void *arg, int len)
{
	int err = BCME_OK;
	wl_scanmac_t *sm = arg;

	/* verify length */
	if (len < (int)OFFSETOF(wl_scanmac_t, data) ||
		sm->len > len - OFFSETOF(wl_scanmac_t, data)) {
		return BCME_BUFTOOSHORT;
	}

	/* process subcommand */
	switch (sm->subcmd_id) {
	case WL_SCANMAC_SUBCMD_ENABLE:
	{
		wl_scanmac_enable_t *sm_enable = (wl_scanmac_enable_t *)sm->data;
		if (sm->len >= sizeof(*sm_enable)) {
			err = scanmac_enable(scan_info, sm_enable->enable);
		} else  {
			err = BCME_BADLEN;
		}
		break;
	}
	case WL_SCANMAC_SUBCMD_BSSCFG:
		err = BCME_BADARG;
		break;
	case WL_SCANMAC_SUBCMD_CONFIG:
	{
		wl_scanmac_config_t *sm_config = (wl_scanmac_config_t *)sm->data;
		if (sm->len >= sizeof(*sm_config)) {
			err = scanmac_config(scan_info, sm_config);
		} else  {
			err = BCME_BADLEN;
		}
		break;
	}
	default:
		ASSERT(0);
		err = BCME_UNSUPPORTED;
		break;
	}
	return err;
}

/** return scanmac bsscfg if macreq enabled, else return NULL */
wlc_bsscfg_t *
wlc_scanmac_get_bsscfg(wlc_scan_info_t *scan, int macreq, wlc_bsscfg_t *cfg)
{
	scan_info_t *scan_info = scan->scan_priv;
	wlc_bsscfg_t *bsscfg = cfg ? cfg : scan_info->wlc->primary_bsscfg;

	/* check if scanmac enabled */
	if (scan_info->scanmac_bsscfg != NULL) {
		uint16 sbmap = scan_info->scanmac_config.scan_bitmap;
		bool is_associated = bsscfg->associated;
		bool is_host_scan = (macreq == WLC_ACTION_SCAN || macreq == WLC_ACTION_ISCAN ||
			macreq == WLC_ACTION_ESCAN);

		/* return scanmac bsscfg if bitmap for macreq is enabled */
		if ((!is_associated && (is_host_scan || macreq == WLC_ACTION_PNOSCAN) &&
			(sbmap & WL_SCANMAC_SCAN_UNASSOC)) ||
			(is_associated &&
			(((macreq == WLC_ACTION_ROAM) && (sbmap & WL_SCANMAC_SCAN_ASSOC_ROAM)) ||
			((macreq == WLC_ACTION_PNOSCAN) && (sbmap & WL_SCANMAC_SCAN_ASSOC_PNO)) ||
			(is_host_scan && (sbmap & WL_SCANMAC_SCAN_ASSOC_HOST))))) {
			return scan_info->scanmac_bsscfg;
		}
	}
	return NULL;
}

/** return scanmac mac if macreq enabled, else return NULL */
struct ether_addr *
wlc_scanmac_get_mac(wlc_scan_info_t *scan, int macreq, wlc_bsscfg_t *bsscfg)
{
	wlc_bsscfg_t *scanmac_bsscfg = wlc_scanmac_get_bsscfg(scan, macreq, bsscfg);

	if (scanmac_bsscfg != NULL) {
		return &scanmac_bsscfg->cur_etheraddr;
	}
	return NULL;
}

/** invoked at scan or GAS complete to allow MAC to be updated */
int
wlc_scanmac_update(wlc_scan_info_t *scan)
{
	scan_info_t *scan_info = scan->scan_priv;

	if (scan_info->scanmac_bsscfg == NULL) {
		/* scanmac not enabled */
		return BCME_NOTREADY;
	}

	scanmac_update_mac(scan_info);

	return BCME_OK;
}

static void
wlc_scan_bss_deinit(void *ctx, wlc_bsscfg_t *cfg)
{
	scan_info_t *scan_info = (scan_info_t *)ctx;
	wlc_info_t *wlc = scan_info->wlc;
	wlc_scan_info_t	*wlc_scan_info = wlc->scan;

	/* Make sure that any active scan is not associated to this cfg */
	if (SCAN_IN_PROGRESS(wlc_scan_info) && (cfg == scan_info->bsscfg)) {
		WL_SCAN_ERROR(("wl%d.%d: %s: scan still active using cfg %p\n", WLCWLUNIT(wlc),
			WLC_BSSCFG_IDX(cfg), __FUNCTION__, OSL_OBFUSCATE_BUF(cfg)));
		wlc_scan_abort(wlc_scan_info, WLC_E_STATUS_ABORT);
	}
}

/** a wrapper to check & end scan excursion & resume primary txq operation */
static void
wlc_scan_excursion_end(scan_info_t *scan_info)
{
	/* enable CFP and TSF update */
	wlc_mhf(scan_info->wlc, MHF2, MHF2_SKIP_CFP_UPDATE, 0, WLC_BAND_ALL);
	wlc_skip_adjtsf(scan_info->wlc, FALSE, NULL, WLC_SKIP_ADJTSF_SCAN, WLC_BAND_ALL);

	/* Restore promisc behavior for beacons and probes */
	wlc_mac_bcn_promisc_update(scan_info->wlc, BCNMISC_SCAN, FALSE);

	/* check & resume normal tx queue operation */
	wlc_txqueue_end(scan_info->wlc, NULL);
}

#ifdef HEALTH_CHECK
static int
wlc_hchk_scanmodule(uint8 *buffer, uint16 length, void *context,
	int16 *bytes_written)
{
	int rc = 0;
	wlc_info_t *wlc = (wlc_info_t*)context;
	wlc_scan_info_t *wlc_scan_info = (wlc_scan_info_t *)wlc->scan;
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;

	if (SCAN_IN_PROGRESS(wlc_scan_info)) {
		if (HND_DS_HC_ENAB()) {
			if (scan_info->in_progress_duration == 0) {
				wl_health_check_notify(wlc->wl,
					HEALTH_CHECK_WL_SCAN_IN_PROGRESS, TRUE);
			}
		}
		scan_info->in_progress_duration++;
		/* scanstall_threshold value can also updated from the Health Check
		* IOVAR wl hc scan stall_threshold
		*/
		if (scan_info->in_progress_duration >= scan_info->scanstall_threshold)
		{
			WL_ERROR(("wl:%d: %s: Scan module stalled since %d Seconds, Scan: "
				"start time: %d mSec, Eng usage: %d, State: 0x%x, bss_type: %d\n",
				wlc->pub->unit, __FUNCTION__, scan_info->in_progress_duration,
				wlc_scan_info->scan_start_time,
				wlc_scan_info->usage, wlc_scan_info->state,
				wlc_scan_info->bss_type));
			/* Cause FATAL crash dump */
			wlc->hw->need_reinit = WL_REINIT_RC_SCAN_STALLED;
			WLC_FATAL_ERROR(wlc);
		}
	} else {
		/* Resetting back if scan not initiated */
		scan_info->in_progress_duration = 0;
		if (HND_DS_HC_ENAB()) {
			wl_health_check_notify(wlc->wl, HEALTH_CHECK_WL_SCAN_IN_PROGRESS, FALSE);
		}
	}
	return rc;
}
#endif /* HEALTH_CHECK */

#ifdef WL_OCE
void
wlc_scan_probe_suppress(wlc_scan_info_t *wlc_scan_info)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
	scan_info->pass = 0;
}
#endif /* WL_OCE */

int
wlc_scan_start_register(wlc_info_t *wlc, scan_start_fn_t fn, void *arg)
{
	scan_info_t *scan_info = (scan_info_t *)wlc->scan->scan_priv;
	bcm_notif_h hdl = scan_info->scan_start_h;

	return bcm_notif_add_interest(hdl, (bcm_notif_client_callback)fn, arg);
}

int
wlc_scan_start_unregister(wlc_info_t *wlc, scan_start_fn_t fn, void *arg)
{
	scan_info_t *scan_info = (scan_info_t *)wlc->scan->scan_priv;
	bcm_notif_h hdl = scan_info->scan_start_h;

	return bcm_notif_remove_interest(hdl, (bcm_notif_client_callback)fn, arg);
}

#ifdef WL_OCE

int
wlc_scan_update_current_slot_duration(wlc_info_t *wlc, uint32 duration)
{
	scan_info_t *scan_info = (scan_info_t *)wlc->scan->scan_priv;
	wlc_msch_req_param_t req;

	req.duration = duration;

	wlc_msch_timeslot_update(wlc->msch_info,
			scan_info->msch_req_hdl, &req, MSCH_UPDATE_END_TIME);

	return BCME_OK;
}

uint8
wlc_scan_get_num_passes_left(wlc_info_t *wlc)
{
	scan_info_t *scan_info = (scan_info_t *)wlc->scan->scan_priv;

	return scan_info->pass;
}

#endif /* WL_OCE */

#if BAND6G
/* RESETS the 6G channel profile during scan completion and scan abort */
void
wlc_scan_6g_get_prof_list(wlc_scan_info_t *scan, rnr_list_req_t *rnr_list)
{
	uint8 i = 0;
	int length = 0;
	wlc_scan_6g_chan_prof_t *chan_prof_6g, *next = NULL;
	wlc_scan_wlioctl_prof_list_t *prof_list = rnr_list->prof_list;
	wlc_scan_wlioctl_prof_t *profile;
	length = sizeof(rnr_list_req_t) -  sizeof(wlc_scan_wlioctl_prof_list_t);
	for (i = 0; i < WL_NUMCHANSPECS_6G_20; i++) {
		if (scan->scan_6g_prof[i] == NULL ||
			scan->scan_6g_prof[i]->chanspec == INVCHANSPEC ||
			scan->scan_6g_prof[i]->profile == NULL) {
			continue;
		}
		length += sizeof(wlc_scan_wlioctl_prof_list_t);
		length -= sizeof(wlc_scan_wlioctl_prof_t);
		rnr_list->count++;
		prof_list->chanspec = scan->scan_6g_prof[i]->chanspec;
		prof_list->prbres_20tu_set = scan->scan_6g_prof[i]->prbres_20tu_set;
		prof_list->rnr = scan->scan_6g_prof[i]->rnr;
		chan_prof_6g = scan->scan_6g_prof[i]->profile;
		profile = prof_list->profile;
		do {
			prof_list->profile_count++;
			next = chan_prof_6g->next_list;
			/*
			 * fill data for the profile
			 */
			memcpy(profile, (wlc_scan_wlioctl_prof_t *)chan_prof_6g,
				sizeof(wlc_scan_wlioctl_prof_t));
			profile++;
			length += sizeof(wlc_scan_wlioctl_prof_t);
			chan_prof_6g = next;
		} while (chan_prof_6g);
		prof_list = (wlc_scan_wlioctl_prof_list_t *)profile;
	}
	rnr_list->len = length;
}

void
wlc_scan_6g_delete_prof_list(wlc_scan_info_t *scan)
{
	uint8 i = 0;
	scan_info_t *scan_priv = (scan_info_t *)scan->scan_priv;
	wlc_scan_6g_chan_prof_t *chan_prof_6g, *next = NULL;
	for (i = 0; i < WL_NUMCHANSPECS_6G_20; i++) {
		if (scan->scan_6g_prof[i] == NULL ||
			scan->scan_6g_prof[i]->chanspec == INVCHANSPEC ||
			scan->scan_6g_prof[i]->profile == NULL) {
			continue;
		}
		/* Traverse through the list and free if any profile are present */
		chan_prof_6g = scan->scan_6g_prof[i]->profile;
		do {
			next = chan_prof_6g->next_list;
			MFREE(scan_priv->wlc->osh, chan_prof_6g,
				sizeof(wlc_scan_6g_chan_prof_t));
			chan_prof_6g = next;
		} while (chan_prof_6g);
		scan->scan_6g_prof[i]->chanspec = INVCHANSPEC;
		scan->scan_6g_prof[i]->prbres_20tu_set = FALSE;
		scan->scan_6g_prof[i]->rnr = FALSE;
		scan->scan_6g_prof[i]->profile = NULL;
		MFREE(scan_priv->wlc->osh, scan->scan_6g_prof[i],
			sizeof(wlc_scan_6g_prof_list_t));
	}
	scan_priv->state_6g_chan = SCAN_6G_CHAN_NONE;
	scan_priv->prb_idle_on_6g_chan = FALSE;
}

/**
* Builds the 6G channel Profile list from Info received from RNR and FILS discovery
* @param   wlc_info_t *wlc , wlc_scan_6g_chan_prof_t, chanspec , bool RNR
* @return  BCME_OK : If adding the profile to the list is succesfull, else
* appropriate Error codes are returned upon failure
*
*/
int
wlc_scan_6g_add_to_chan_prof_list(wlc_info_t *wlc, wlc_scan_6g_chan_prof_t *chan6g_prof,
	chanspec_t chanspec, bool rnr)
{
	uint8 index;
	uint8 ctl_ch;
	wlc_scan_info_t	*scan = wlc->scan;
	wlc_scan_6g_chan_prof_t *cur_chan6g_prof;
	int err = BCME_OK;

	/* Check for the valid chanspec check */
	if (!wf_chspec_valid(chanspec)) {
		err = BCME_BADCHAN;
		goto exit;
	}

	ASSERT(chan6g_prof);
	ASSERT(CHSPEC_IS6G(chanspec));

	/* Get the index to the channel profile list */
	ctl_ch = wf_chspec_ctlchan(chanspec);
	index = SCAN_6GCHAN_TO_INDEX(ctl_ch);
	ASSERT(index < WL_NUMCHANSPECS_6G_20);
	if (scan->scan_6g_prof[index] == NULL) {
		scan->scan_6g_prof[index] =
			(wlc_scan_6g_prof_list_t *)MALLOCZ(wlc->osh,
			sizeof(wlc_scan_6g_prof_list_t));
	}

	ASSERT(scan->scan_6g_prof[index] != NULL);
	scan->scan_6g_prof[index]->chanspec = (chanspec_t)(ctl_ch | WL_CHANSPEC_BW_20 |
		CHSPEC_BAND(chanspec));
	cur_chan6g_prof = chan6g_prof;

	if (scan->scan_6g_prof[index]->profile == NULL) {
		scan->scan_6g_prof[index]->profile = chan6g_prof;
	} else {
		struct wlc_scan_6g_chan_prof *next_pointer;
		cur_chan6g_prof = scan->scan_6g_prof[index]->profile;
		while (cur_chan6g_prof) {
			/* Check for Duplicate elements  */
			next_pointer = cur_chan6g_prof->next_list;
			if (!chan6g_prof->bssid_valid && !chan6g_prof->is_short_ssid &&
					!chan6g_prof->ssid_length) {
				/* must have any one of the parameter to create
				 * an element in the list
				 */
				WL_ERROR(("No unique parameter present to add to the list\n"));
				err = BCME_UNSUPPORTED;
				MFREE(wlc->osh, chan6g_prof, sizeof(wlc_scan_6g_chan_prof_t));
				break;
			}
			if (cur_chan6g_prof->bssid_valid) {
				if (chan6g_prof->bssid_valid &&
					!eacmp(&cur_chan6g_prof->bssid, &chan6g_prof->bssid)) {
					/* replace element on bssid match
					 */
					*cur_chan6g_prof = *chan6g_prof;
					cur_chan6g_prof->next_list = next_pointer;
					WL_INFORM(("wl%d:repeat element memory freed\n",
						wlc->pub->unit));
					MFREE(wlc->osh, chan6g_prof,
						sizeof(wlc_scan_6g_chan_prof_t));
					break;
				}
			} else if (cur_chan6g_prof->is_short_ssid && !chan6g_prof->bssid_valid) {
				if (cur_chan6g_prof->short_ssid == chan6g_prof->short_ssid) {
					/* TODO : more corner case error checks can be implemented
					 */
					if (!chan6g_prof->ssid_length &&
							cur_chan6g_prof->ssid_length) {
						/* save the ssid element from old rnr element
						 */
						memcpy(chan6g_prof->SSID, cur_chan6g_prof->SSID,
							cur_chan6g_prof->ssid_length);
						chan6g_prof->ssid_length =
							cur_chan6g_prof->ssid_length;
					}
					*cur_chan6g_prof = *chan6g_prof;
					cur_chan6g_prof->next_list = next_pointer;
					WL_INFORM(("wl%d:repeat element memory freed\n",
						wlc->pub->unit));
					MFREE(wlc->osh, chan6g_prof,
						sizeof(wlc_scan_6g_chan_prof_t));
					break;
				}
			}
			if (!cur_chan6g_prof->next_list) {
				cur_chan6g_prof->next_list =
					(struct wlc_scan_6g_chan_prof *)chan6g_prof;
				break;
			} else {
				cur_chan6g_prof =
					(wlc_scan_6g_chan_prof_t *)cur_chan6g_prof->next_list;
			}
		}
	}
	if (rnr) {
		scan->scan_6g_prof[index]->rnr = rnr;
	}

	if (rnr && RNR_BSS_20_TU_PRB_RSP_ACTIVE(chan6g_prof->bss_params)) {
		scan->scan_6g_prof[index]->prbres_20tu_set = TRUE;
	}
exit:
	if (err) {
		WL_SCAN_ERROR(("wlc_scan_6g_add_to_chan_prof_list err = %d\n", err));
	}
	return err;
}
#endif /* BAND6G */

#ifdef STA
#endif /* STA */
