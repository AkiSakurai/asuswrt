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
 * $Id: wlc_scan.c 773698 2019-03-28 10:57:34Z $
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
#include <proto/802.1d.h>
#include <proto/802.11.h>
#include <proto/802.11e.h>
#ifdef	BCMCCX
#include <proto/802.11_ccx.h>
#endif	/* BCMCCX */
#include <proto/wpa.h>
#include <proto/vlan.h>
#include <sbconfig.h>
#include <pcicfg.h>
#include <bcmsrom.h>
#include <wlioctl.h>
#include <epivers.h>
#ifdef BCMCCX
#include <bcmcrypto/ccx.h>
#endif /* BCMCCX */
#include <d11.h>
#ifdef EVENT_LOG_COMPILE
#include <event_log.h>
#endif // endif
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc_channel.h>
#include <wlc_scandb.h>
#include <wlc.h>
#include <wlc_hw.h>
#ifdef AP
#include <wlc_apps.h>
#endif // endif
#include <wlc_scb.h>
#include <wlc_phy_hal.h>
#ifdef WLLED
#include <wlc_led.h>
#endif // endif
#include <wlc_frmutil.h>
#ifdef WLDPT
#include <wlc_dpt.h>
#endif // endif
#include <wlc_security.h>
#ifdef WLAMSDU
#include <wlc_amsdu.h>
#endif // endif
#include <wlc_event.h>
#ifdef WLDIAG
#include <wlc_diag.h>
#endif /* WLDIAG */
#include <wl_export.h>
#if defined(BCMSUP_PSK) || defined(BCMCCX)
#include <proto/eapol.h>
#endif /* defined(BCMSUP_PSK) || defined(BCMCCX) */
#ifdef BCMSDIO
#include <bcmsdh.h>
#endif /* BCMSDIO */
#ifdef WET
#include <wlc_wet.h>
#endif /* WET */
#if defined(BCMNVRAMW) || defined(WLTEST)
#include <sbchipc.h>
#include <bcmotp.h>
#endif // endif
#ifdef BCMCCMP
#include <bcmcrypto/aes.h>
#endif /* BCMCCMP */
#include <wlc_rm.h>
#ifdef BCMCCX
#include <wlc_ccx.h>
#include <wlc_cac.h>
#endif	/* BCMCCX */
#ifdef BCM_WL_EMULATOR
#include <wl_bcm57emu.h>
#endif // endif
#include <wlc_ap.h>
#ifdef AP
#include <wlc_apcs.h>
#endif // endif
#include <wlc_assoc.h>
#include <wlc_scan_priv.h>
#include <wlc_scan.h>
#ifdef WLP2P
#include <wlc_p2p.h>
#endif // endif
#ifdef WLMCHAN
#include <wlc_mchan.h>
#endif // endif
#include <wlc_11h.h>
#include <wlc_11d.h>
#include <wlc_dfs.h>
#ifdef WLTDLS
#include <wlc_tdls.h>
#endif // endif
#ifdef ANQPO
#include <wl_anqpo.h>
#endif // endif
#ifdef WL_BCN_COALESCING
#include <wlc_bcn_clsg.h>
#endif /* WL_BCN_COALESCING */
#ifdef WLOFFLD
#include <wlc_offloads.h>
#endif // endif
#include <wlc_hw_priv.h>
#ifdef WLAWDL
#include <wlc_awdl.h>
#endif // endif
#ifdef SCANOL
#include <bcm_ol_msg.h>
#include <wlc_scanol.h>
#endif /* SCANOL */
#ifdef WL11K
#include <wlc_rrm.h>
#endif /* WL11K */

#include <wlc_bmac.h>

#ifdef WLSCAN_PS
#include <wlc_stf.h>
#endif // endif

#ifdef WLAIBSS
#include <wlc_aibss.h>
#endif // endif

#ifdef BCMDBG
/* redefine WL_INFORM to print if either INFORM or SCAN level messaging is on */
#undef WL_INFORM
#define	WL_INFORM(args)									\
	do {										\
		if ((wl_msg_level & WL_INFORM_VAL) || (wl_msg_level2 & WL_SCAN_VAL))	\
		    WL_PRINT(args);					\
	} while (0)
#undef WL_INFORM_ON
#define WL_INFORM_ON()	((wl_msg_level & WL_INFORM_VAL) || (wl_msg_level2 & WL_SCAN_VAL))
#endif /* BCMDBG */

/* scan times in milliseconds */
#define WLC_SCAN_MIN_PROBE_TIME	10	/* minimum useful time for an active probe */
#define WLC_SCAN_HOME_TIME	45	/* time for home channel processing */
#define WLC_SCAN_ASSOC_TIME 20 /* time to listen on a channel for probe resp while associated */

#ifdef BCMQT_CPU
#define WLC_SCAN_UNASSOC_TIME 400	/* qt is slow */
#else
#define WLC_SCAN_UNASSOC_TIME 40	/* listen on a channel for prb rsp while unassociated */
#endif // endif
#define WLC_SCAN_NPROBES	2	/* do this many probes on each channel for an active scan */

/* scan_pass state values */
#define WLC_SCAN_SUCCESS	0 /* scan success */
#define WLC_SCAN_ABORT		-2 /* Abort the scan */
#define WLC_SCAN_START		-1 /* Start the scan */
#define WLC_SCAN_CHANNEL_PREP	0  /* Prepare the channel for the scan */

/* Enables the iovars */
#define WLC_SCAN_IOVARS

#if defined(BRINGUP_BUILD)
#undef WLC_SCAN_IOVARS
#endif // endif
#ifdef WLOLPC
#include <wlc_olpc_engine.h>
#endif /* WLOLPC */

static int wlc_scan_watchdog(void *hdl);

static void wlc_scan_channels(scan_info_t *scan_info, chanspec_t *chanspec_list, int *pchannel_num,
	int channel_max, chanspec_t chanspec_start, int channel_type);

static void wlc_scantimer(void *arg);

#ifdef WLC_SCAN_IOVARS
static int wlc_scan_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid,
	const char *name, void *params, uint p_len, void *arg,
	int len, int val_size, struct wlc_if *wlcif);
#endif /* WLC_SCAN_IOVARS */

static void wlc_scan_return_home_channel(scan_info_t *scan_info);
static void wlc_scan_callback(scan_info_t *scan_info, uint status);

static uint wlc_scan_prohibited_channels(scan_info_t *scan_info,
	chanspec_t *chanspec_list, int channel_max);
static void wlc_scan_do_pass(scan_info_t *scan_info, chanspec_t chanspec);
static void wlc_scan_sendprobe(scan_info_t *scan_info);
static void wlc_scan_ssidlist_free(scan_info_t *scan_info);

#if defined(BCMDBG) || defined(WLMSG_INFORM)
static void wlc_scan_print_ssids(wlc_ssid_t *ssid, int nssid);
#endif // endif
#if (defined(BCMDBG) || defined(BCMDBG_DUMP)) && !defined(SCANOL)
static int wlc_scan_dump(scan_info_t *si, struct bcmstrbuf *b);
#endif // endif
#ifdef WLPFN
#include <wl_pfn.h>
#endif /* WLPFN */
#ifdef WL11N
static void wlc_ht_obss_scan_update(scan_info_t *scan_info, int status);
#else /* WL11N */
#define wlc_ht_obss_scan_update(a, b) do {} while (0)
#endif /* WL11N */

#ifdef WLSCANCACHE
static void wlc_scan_merge_cache(scan_info_t *scan_info, uint current_timestamp,
                                 const struct ether_addr *BSSID, int nssid, const wlc_ssid_t *SSID,
                                 int BSS_type, const chanspec_t *chanspec_list, uint chanspec_num,
                                 wlc_bss_list_t *bss_list);
static void wlc_scan_fill_cache(scan_info_t *scan_info, uint current_timestamp);
static void wlc_scan_build_cache_list(void *arg1, void *arg2, uint timestamp,
                                      struct ether_addr *BSSID, wlc_ssid_t *SSID,
                                      int BSS_type, chanspec_t chanspec, void *data, uint datalen);
static void wlc_scan_cache_result(scan_info_t *scan_info);
#else
#define wlc_scan_fill_cache(si, ts)	do {} while (0)
#define wlc_scan_merge_cache(si, ts, BSSID, nssid, SSID, BSS_type, c_list, c_num, bss_list)
#define wlc_scan_cache_result(si)
#endif // endif

#ifdef WLSCAN_PS
static int wlc_scan_ps_config_cores(scan_info_t *scan_info, bool flag);
#endif // endif

#ifdef EXTENDED_SCAN
#define SCAN_TYPE_FOREGROUND(scan_info) 	\
	(((scan_info)->extdscan && 	\
	  ((scan_info)->scan_type == EXTDSCAN_FOREGROUND_SCAN)) ? TRUE : FALSE)
#define SCAN_TYPE_FBACKGROUND(scan_info) 	\
	(((!(scan_info)->extdscan) || (scan_info->extdscan && 	\
	((scan_info)->scan_type == EXTDSCAN_FORCEDBACKGROUND_SCAN))) ? TRUE : FALSE)
#define SCAN_TYPE_BACKGROUND(scan_info) 	\
	(((scan_info)->extdscan && 	\
	((scan_info)->scan_type == EXTDSCAN_BACKGROUND_SCAN)) ? TRUE : FALSE)

#define CHANNEL_PASSIVE_DWELLTIME(s)	\
	((s)->extdscan && (s)->chan_list &&	\
	  (s)->chan_list[(s)->channel_idx].channel_maxtime) ?	\
	((s)->chan_list[(s)->channel_idx].channel_maxtime) : (s)->passive_time

#define CHANNEL_ACTIVE_DWELLTIME(s)	\
	((s)->extdscan && (s)->chan_list &&	\
	  (s)->chan_list[(s)->channel_idx].channel_maxtime) ?	\
	((s)->chan_list[(s)->channel_idx].channel_maxtime) : (s)->active_time

#define SCAN_PROBE_TXRATE(scan_info)	\
	(((scan_info)->extdscan) ? (scan_info)->max_txrate : 0)
#else /* EXTENDED_SCAN */
#define SCAN_TYPE_FOREGROUND(scan_info) 	FALSE
#define SCAN_TYPE_BACKGROUND(scan_info) 	FALSE
#define SCAN_TYPE_FBACKGROUND(scan_info) 	TRUE

#define CHANNEL_PASSIVE_DWELLTIME(s) ((s)->passive_time)
#define CHANNEL_ACTIVE_DWELLTIME(s) ((s)->active_time)
#define SCAN_PROBE_TXRATE(scan_info)	0
#endif /* EXTENDED_SCAN */

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
	IOV_SCAN_HOME_TIME,
	IOV_SCAN_NPROBES,
	IOV_SCAN_EXTENDED,
	IOV_SCAN_NOPSACK,
	IOV_SCANCACHE,
	IOV_SCANCACHE_TIMEOUT,
	IOV_SCANCACHE_CLEAR,
	IOV_SCAN_FORCE_ACTIVE,	/* force passive to active conversion in radar/restricted channel */
	IOV_SCAN_ASSOC_TIME_DEFAULT,
	IOV_SCAN_UNASSOC_TIME_DEFAULT,
	IOV_SCAN_PASSIVE_TIME_DEFAULT,
	IOV_SCAN_HOME_TIME_DEFAULT,
	IOV_SCAN_DBG,
	IOV_SCAN_TEST,
	IOV_SCAN_HOME_AWAY_TIME,
	IOV_SCAN_RX_PWRSAVE,	/* reduce rx chains for pwr optimization */
	IOV_SCAN_TX_PWRSAVE,	/* reduce tx chains for pwr optimization */
	IOV_SCAN_PWRSAVE,	/* turn on/off bith tx and rx for single core scanning  */
	IOV_LAST		/* In case of a need to check max ID number */
};

/* AP IO Vars */
static const bcm_iovar_t wlc_scan_iovars[] = {
	{"passive", IOV_PASSIVE,
	(IOVF_NTRL|IOVF_OPEN_ALLOW), IOVT_UINT16, 0
	},
#ifdef STA
	{"scan_assoc_time", IOV_SCAN_ASSOC_TIME,
	(IOVF_NTRL|IOVF_OPEN_ALLOW), IOVT_UINT16, 0
	},
	{"scan_unassoc_time", IOV_SCAN_UNASSOC_TIME,
	(IOVF_NTRL|IOVF_OPEN_ALLOW), IOVT_UINT16, 0
	},
#endif /* STA */
	{"scan_passive_time", IOV_SCAN_PASSIVE_TIME,
	(IOVF_WHL|IOVF_OPEN_ALLOW), IOVT_UINT16, 0
	},
	/* unlike the other scan times, home_time can be zero */
	{"scan_home_time", IOV_SCAN_HOME_TIME,
	(IOVF_WHL|IOVF_OPEN_ALLOW), IOVT_UINT16, 0
	},
#ifdef STA
	{"scan_nprobes", IOV_SCAN_NPROBES,
	(IOVF_NTRL|IOVF_OPEN_ALLOW), IOVT_INT8, 0
	},
	{"scan_force_active", IOV_SCAN_FORCE_ACTIVE,
	0, IOVT_BOOL, 0
	},
#ifdef EXTENDED_SCAN
	{"extdscan", IOV_SCAN_EXTENDED,
	(IOVF_NTRL), IOVT_BUFFER, 0
	},
#ifdef BCMDBG
	{"scan_nopsack", IOV_SCAN_NOPSACK,
	(IOVF_NTRL), IOVT_UINT8, 0
	},
#endif /* BCMDBG */
#endif /* EXTENDED_SCAN */
#ifdef WLSCANCACHE
	{"scancache", IOV_SCANCACHE,
	(IOVF_OPEN_ALLOW), IOVT_BOOL, 0
	},
	{"scancache_timeout", IOV_SCANCACHE_TIMEOUT,
	(IOVF_OPEN_ALLOW), IOVT_INT32, 0
	},
	{"scancache_clear", IOV_SCANCACHE_CLEAR,
	(IOVF_OPEN_ALLOW), IOVT_VOID, 0
	},
#endif /* WLSCANCACHE */
#endif /* STA */
#ifdef STA
	{"scan_assoc_time_default", IOV_SCAN_ASSOC_TIME_DEFAULT,
	(IOVF_NTRL|IOVF_OPEN_ALLOW), IOVT_UINT16, 0
	},
	{"scan_unassoc_time_default", IOV_SCAN_UNASSOC_TIME_DEFAULT,
	(IOVF_NTRL|IOVF_OPEN_ALLOW), IOVT_UINT16, 0
	},
#endif /* STA */
	{"scan_passive_time_default", IOV_SCAN_PASSIVE_TIME_DEFAULT,
	(IOVF_NTRL|IOVF_OPEN_ALLOW), IOVT_UINT16, 0
	},
	/* unlike the other scan times, home_time can be zero */
	{"scan_home_time_default", IOV_SCAN_HOME_TIME_DEFAULT,
	(IOVF_WHL|IOVF_OPEN_ALLOW), IOVT_UINT16, 0
	},
#ifdef BCMDBG
	{"scan_dbg", IOV_SCAN_DBG, 0, IOVT_UINT8, 0},
	{"scan_test", IOV_SCAN_TEST, 0, IOVT_UINT8, 0},
#endif // endif
#ifdef STA
	{"scan_home_away_time", IOV_SCAN_HOME_AWAY_TIME, (IOVF_WHL), IOVT_UINT16, 0},
#endif /* STA */
#ifdef WLSCAN_PS
#if defined(BCMDBG) || defined(WLTEST)
	/* debug iovar to enable power optimization in rx */
	{"scan_rx_ps", IOV_SCAN_RX_PWRSAVE, (IOVF_WHL), IOVT_UINT8, 0},
	/* debug iovar to enable power optimization in tx */
	{"scan_tx_ps", IOV_SCAN_TX_PWRSAVE, (IOVF_WHL), IOVT_UINT8, 0},
#endif /* defined(BCMDBG) || defined(WLTEST) */
	/* single core scanning to reduce power consumption */
	{"scan_ps", IOV_SCAN_PWRSAVE, (IOVF_WHL), IOVT_UINT8, 0},
#endif /* WLSCAN_PS */
	{NULL, 0, 0, 0, 0 }
};
#endif /* WLC_SCAN_IOVARS */

/* debug timer used in scan module */
/* #define DEBUG_SCAN_TIMER */
#ifdef DEBUG_SCAN_TIMER
static void
wlc_scan_add_timer_dbg(scan_info_t *scan, uint to, bool prd, const char *fname, int line)
{
	WL_SCAN(("wl%d: %s(%d): wl_add_timer: timeout %u tsf %u\n",
		scan->unit, fname, line, to, SCAN_GET_TSF_TIMERLOW(scan)));
	wl_add_timer(scan->wlc->wl, scan->timer, to, prd);
}

static bool
wlc_scan_del_timer_dbg(scan_info_t *scan, const char *fname, int line)
{
	WL_SCAN(("wl%d: %s(%d): wl_del_timer: tsf %u\n",
		scan->unit, func, line, SCAN_GET_TSF_TIMERLOW(scan)));
	return wl_del_timer(scan->wlc->wl, scan->timer);
}
#define WLC_SCAN_ADD_TIMER(scan, to, prd) \
	      wlc_scan_add_timer_dbg(scan, to, prd, __FUNCTION__, __LINE__)
#define WLC_SCAN_DEL_TIMER(wlc, scan) \
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
#define SCAN_TEST_NONE	0
#define SCAN_TEST_ABORT_PSPEND	1	/* abort after sending PM1 indication */
#define SCAN_TEST_ABORT_PSPEND_AND_SCAN	2	/* abort after sending PM1 indication and scan */
#define SCAN_TEST_ABORT_WSUSPEND	3	/* abort after tx suspend */
#define SCAN_TEST_ABORT_WSUSPEND_AND_SCAN	4	/* abort after suspend and scan */
#define SCAN_TEST_ABORT_ENTRY	5	/* abort right away after the scan request */
#endif // endif

/* guard time */
#define WLC_SCAN_PSPEND_GUARD_TIME	5
#define WLC_SCAN_WSUSPEND_GUARD_TIME	5

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

wlc_scan_info_t*
BCMATTACHFN(wlc_scan_attach)(wlc_info_t *wlc, void *wl, osl_t *osh, uint unit)
{
	scan_info_t *scan_info;
	watchdog_fn_t watchdog_fn = NULL;
	iovar_fn_t iovar_fn = NULL;
	const bcm_iovar_t *iovars = NULL;
	int	err = 0;

	uint	scan_info_size = (uint)sizeof(scan_info_t);
	uint	ssid_offs, chan_offs;
	BCM_REFERENCE(iovars);
	BCM_REFERENCE(iovar_fn);

	ssid_offs = scan_info_size = ROUNDUP(scan_info_size, sizeof(uint32));
	scan_info_size += sizeof(wlc_ssid_t) * WLC_SCAN_NSSID_PREALLOC;
	chan_offs = scan_info_size = ROUNDUP(scan_info_size, sizeof(uint32));
	scan_info_size += sizeof(chan_scandata_t) * WLC_SCAN_NCHAN_PREALLOC;

	scan_info = (scan_info_t *)MALLOC(osh, scan_info_size);
	if (scan_info == NULL)
		return NULL;

	bzero((char*)scan_info, scan_info_size);

	scan_info->scan_pub = (struct wlc_scan_info *)MALLOC(osh, sizeof(struct wlc_scan_info));
	if (scan_info->scan_pub == NULL) {
		MFREE(osh, scan_info, scan_info_size);
		return NULL;
	}
	bzero((char*)scan_info->scan_pub, sizeof(struct wlc_scan_info));
	scan_info->scan_pub->scan_priv = (void *)scan_info;

	scan_info->memsize = scan_info_size;
	scan_info->wlc = wlc;
	scan_info->osh = osh;
	scan_info->unit = unit;
#ifdef SCANOL
	if (wlc_scanol_init(scan_info, wlc->hw, osh) != BCME_OK) {
		MFREE(osh, scan_info->scan_pub, sizeof(struct wlc_scan_info));
		if (PWRSTATS_ENAB(wlc->pub) && scan_info->scan_stats)
			MFREE(SCAN_OSH(scan_info), scan_info->scan_stats,
				sizeof(wl_pwr_scan_stats_t));
		MFREE(osh, scan_info, scan_info_size);
		return NULL;
	}
#endif // endif
	scan_info->channel_idx = -1;
	scan_info->scan_pub->in_progress = FALSE;

	scan_info->defaults.assoc_time = WLC_SCAN_ASSOC_TIME;
	scan_info->defaults.unassoc_time = WLC_SCAN_UNASSOC_TIME;
	scan_info->defaults.home_time = WLC_SCAN_HOME_TIME;
	scan_info->defaults.passive_time = WLC_SCAN_PASSIVE_TIME;
	scan_info->defaults.nprobes = WLC_SCAN_NPROBES;
	scan_info->defaults.passive = FALSE;
	scan_info->home_away_time = WLC_SCAN_AWAY_LIMIT;

	scan_info->timer = wl_init_timer((struct wl_info *)wl,
	                                 wlc_scantimer, scan_info, "scantimer");
	if (scan_info->timer == NULL) {
		WL_ERROR(("wl%d: %s: wl_init_timer for scan timer failed\n", unit, __FUNCTION__));
		goto error;
	}

#if defined(WLSCANCACHE) && !defined(WLSCANCACHE_DISABLED)
	scan_info->sdb = wlc_scandb_create(osh, unit);
	if (scan_info->sdb == NULL) {
		WL_ERROR(("wl%d: %s: wlc_create_scandb failed\n", unit, __FUNCTION__));
		goto error;
	}
	wlc->pub->_scancache_support = TRUE;
	scan_info->scan_pub->_scancache = TRUE;	/* enabled by default */
#endif /* WLSCANCACHE || WLSCANCACHE_DISABLED */
	watchdog_fn = wlc_scan_watchdog;
	SCAN_SET_WATCHDOG_FN(watchdog_fn);

#ifdef WLC_SCAN_IOVARS
	iovar_fn = wlc_scan_doiovar;
	iovars = wlc_scan_iovars;
#endif /* WLC_SCAN_IOVARS */

	scan_info->ssid_prealloc = (wlc_ssid_t*)((uintptr)scan_info + ssid_offs);
	scan_info->nssid_prealloc = WLC_SCAN_NSSID_PREALLOC;
	scan_info->ssid_list = scan_info->ssid_prealloc;

#ifdef EXTENDED_SCAN
	scan_info->chan_prealloc = (chan_scandata_t*)((uintptr)scan_info + chan_offs);
	scan_info->nchan_prealloc = WLC_SCAN_NCHAN_PREALLOC;
	scan_info->chan_list = scan_info->chan_prealloc;
#else
	BCM_REFERENCE(chan_offs);
#endif /* EXTENDED_SCAN */

	if (PWRSTATS_ENAB(wlc->pub)) {
		scan_info->scan_stats =
		        (wl_pwr_scan_stats_t*)MALLOC(osh, sizeof(wl_pwr_scan_stats_t));
		if (scan_info->scan_stats == NULL) {
			WL_ERROR(("wl%d: %s: failure allocating power stats\n",
			          unit, __FUNCTION__));
			goto error;
		}
		bzero((char*)scan_info->scan_stats, sizeof(wl_pwr_scan_stats_t));
	}
	err = wlc_module_register(wlc->pub, iovars, "scan", scan_info, iovar_fn,
	                          watchdog_fn, NULL, wlc_scan_down);
	if (err) {
		WL_ERROR(("wl%d: %s: wlc_module_register err=%d\n",
		          unit, __FUNCTION__, err));
		goto error;
	}

	err = wlc_module_add_ioctl_fn(wlc->pub, (void *)scan_info->scan_pub,
	                              (wlc_ioctl_fn_t)wlc_scan_ioctl, 0, NULL);
	if (err) {
		WL_ERROR(("wl%d: %s: wlc_module_add_ioctl_fn err=%d\n",
		          unit, __FUNCTION__, err));
		goto error;
	}

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
	wlc_dump_register(wlc->pub, "scan", (dump_fn_t)wlc_scan_dump, (void *)scan_info);
#ifdef WLSCANCACHE
	if (SCANCACHE_ENAB(scan_info->scan_pub))
		wlc_dump_register(wlc->pub, "scancache", wlc_scandb_dump, scan_info->sdb);
#endif /* WLSCANCACHE */
#endif /* BCMDBG || BCMDBG_DUMP */

#ifdef BCMDBG
	/* scan_info->debug = SCAN_DBG_ENT; */
#endif // endif
	scan_info->pspend_guard_time = WLC_SCAN_PSPEND_GUARD_TIME;
	scan_info->wsuspend_guard_time = WLC_SCAN_WSUSPEND_GUARD_TIME;

#ifdef WLSCAN_PS
	scan_info->scan_ps_txchain = 0;
	scan_info->scan_ps_rxchain = 0;
	/* disable scan power optimization by default */
#if defined(BCMDBG) || defined(WLTEST)
	scan_info->scan_rx_pwrsave = FALSE;
	scan_info->scan_tx_pwrsave = FALSE;
#endif /* defined(BCMDBG) || defined(WLTEST) */
	scan_info->scan_pwrsave_enable = FALSE;
#endif /* WLSCAN_PS */

	return scan_info->scan_pub;

error:
	if (scan_info) {
		if (scan_info->timer != NULL)
			WLC_SCAN_FREE_TIMER(scan_info);
		if (scan_info->sdb)
			wlc_scandb_free(scan_info->sdb);
#ifdef SCANOL
		wlc_scanol_cleanup(scan_info, osh);
#endif // endif
		if (scan_info->scan_pub)
			MFREE(osh, scan_info->scan_pub, sizeof(struct wlc_scan_info));
		if (PWRSTATS_ENAB(wlc->pub) && scan_info->scan_stats) {
			MFREE(osh, scan_info->scan_stats, sizeof(wl_pwr_scan_stats_t));
		}
		MFREE(osh, scan_info, scan_info_size);
	}

	return NULL;
}

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

int
BCMUNINITFN(wlc_scan_down)(void *hdl)
{
#ifndef BCMNODOWN
	scan_info_t *scan_info = (scan_info_t *)hdl;
	int callbacks = 0;

	if (!WLC_SCAN_DEL_TIMER(scan_info))
		callbacks ++;

	scan_info->pass = WLC_SCAN_START;
	scan_info->channel_idx = -1;
	scan_info->scan_pub->in_progress = FALSE;
	wlc_phy_hold_upd(SCAN_GET_PI_PTR(scan_info), PHY_HOLD_FOR_SCAN, FALSE);

	wlc_scan_ssidlist_free(scan_info);

	return callbacks;
#else
	return 0;
#endif /* BCMNODOWN */
}

void
BCMATTACHFN(wlc_scan_detach)(wlc_scan_info_t *wlc_scan_info)
{
	scan_info_t *scan_info;

	if (!wlc_scan_info)
		return;

	scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
	if (scan_info) {
		if (scan_info->timer) {
			WLC_SCAN_FREE_TIMER(scan_info);
			scan_info->timer = NULL;
		}
#ifdef WLSCANCACHE
		if (SCANCACHE_SUPPORT(scan_info->wlc->pub))
			wlc_scandb_free(scan_info->sdb);
#endif /* WLSCANCACHE */

		wlc_module_unregister(scan_info->wlc->pub, "scan", scan_info);

		wlc_module_remove_ioctl_fn(scan_info->wlc->pub, (void *)scan_info->scan_pub);

		ASSERT(scan_info->ssid_list == scan_info->ssid_prealloc);
		if (scan_info->ssid_list != scan_info->ssid_prealloc) {
			WL_ERROR(("wl%d: %s: ssid_list not set to prealloc\n",
				scan_info->unit, __FUNCTION__));
		}
		MFREE(scan_info->osh, wlc_scan_info, sizeof(struct wlc_scan_info));
		MFREE(scan_info->osh, scan_info, scan_info->memsize);
	}
}

#if defined(BCMDBG) || defined(WLMSG_INFORM)
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

bool
wlc_scan_in_scan_chanspec_list(wlc_scan_info_t *wlc_scan_info, chanspec_t chanspec)
{
	scan_info_t *scan_info = (scan_info_t *) wlc_scan_info->scan_priv;
	int i;
	uint8 chan;

	/* scan chanspec list not setup, return no match */
	if (scan_info->channel_idx == -1) {
		WL_INFORM(("%s: Scan chanspec list NOT setup, NO match\n", __FUNCTION__));
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

/* Caution: when passing a non-primary bsscfg to this function the caller
 * must make sure to abort the scan before freeing the bsscfg!
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
	int home_time,
	const chanspec_t* chanspec_list,
	int channel_num,
	chanspec_t chanspec_start,
	bool save_prb,
	scancb_fn_t fn, void* arg,
	int away_channels_limit,
	bool extdscan,
	bool suppress_ssid,
	bool include_cache,
	uint scan_flags,
	wlc_bsscfg_t *cfg,
	uint8 usage,
	actcb_fn_t act_cb, void *act_cb_arg)
{
	bool scan_in_progress;
	int i, num;

	scan_info_t *scan_info = (scan_info_t *) wlc_scan_info->scan_priv;
	wlc_info_t *wlc = scan_info->wlc;
#if defined(BCMDBG) || defined(WLMSG_INFORM)
	char *ssidbuf;
	char eabuf[ETHER_ADDR_STR_LEN];
#endif // endif
#ifdef WLMSG_ROAM
	char SSIDbuf[DOT11_MAX_SSID_LEN+1];
#endif // endif
	BCM_REFERENCE(wlc);
	ASSERT(nssid);
	ASSERT(ssid != NULL);
	ASSERT(bss_type == DOT11_BSSTYPE_INFRASTRUCTURE ||
	       bss_type == DOT11_BSSTYPE_INDEPENDENT ||
	       bss_type == DOT11_BSSTYPE_ANY);

	WL_SCAN(("wl%d: %s: scan request at %u\n", scan_info->unit, __FUNCTION__,
	         SCAN_GET_TSF_TIMERLOW(scan_info)));
#if defined(BCMDBG) || defined(WLMSG_INFORM)
	ssidbuf = (char *) MALLOC(scan_info->osh, SSID_FMT_BUF_LEN);

	if (nssid == 1) {
			WL_INFORM(("wl%d: %s: scanning for SSID \"%s\"\n", scan_info->unit,
				__FUNCTION__, ssidbuf ? (wlc_format_ssid(ssidbuf, ssid->SSID,
				ssid->SSID_len), ssidbuf) : "???"));
	} else {
		WL_INFORM(("wl%d: %s: scanning for SSIDs:\n", scan_info->unit, __FUNCTION__));
		if (WL_INFORM_ON())
			wlc_scan_print_ssids(ssid, nssid);
	}
	WL_INFORM(("wl%d: %s: scanning for BSSID \"%s\"\n", scan_info->unit, __FUNCTION__,
	           (bcm_ether_ntoa(bssid, eabuf), eabuf)));
#endif /* BCMDBG || WLMSG_INFORM */

	/* enforce valid argument */
	scan_info->ssid_wildcard_enabled = 0;
	for (i = 0; i < nssid; i++) {
		if (ssid[i].SSID_len > DOT11_MAX_SSID_LEN) {
			WL_ERROR(("wl%d: %s: invalid SSID len %d, capping\n",
			          scan_info->unit, __FUNCTION__, ssid[i].SSID_len));
			ssid[i].SSID_len = DOT11_MAX_SSID_LEN;
		}
		if (ssid[i].SSID_len == 0)
			scan_info->ssid_wildcard_enabled = 1;
	}

	scan_in_progress = SCAN_IN_PROGRESS(wlc_scan_info);
	/* clear or set optional params to default */
	/* keep persistent scan suppress flag */
	wlc_scan_info->state &= SCAN_STATE_SUPPRESS;
	scan_info->nprobes = scan_info->defaults.nprobes;
	if (IS_ASSOCIATED(scan_info)) {
		scan_info->active_time = scan_info->defaults.assoc_time;
		scan_info->home_time = scan_info->defaults.home_time;
	} else {
		scan_info->active_time = scan_info->defaults.unassoc_time;
		scan_info->home_time = 0;
	}
	scan_info->passive_time = scan_info->defaults.passive_time;
	if (scan_info->defaults.passive)
		wlc_scan_info->state |= SCAN_STATE_PASSIVE;

	if (scan_type == DOT11_SCANTYPE_ACTIVE) {
		wlc_scan_info->state &= ~SCAN_STATE_PASSIVE;
	} else if (scan_type == DOT11_SCANTYPE_PASSIVE) {
		wlc_scan_info->state |= SCAN_STATE_PASSIVE;
	}
	/* passive scan always has nprobes to 1 */
	if (wlc_scan_info->state & SCAN_STATE_PASSIVE) {
		scan_info->nprobes = 1;
	}
	if (active_time > 0)
		scan_info->active_time = (uint16)active_time;

	if (passive_time >= 0)
		scan_info->passive_time = (uint16)passive_time;

	if (home_time >= 0 && IS_ASSOCIATED(scan_info))
		scan_info->home_time = (uint16)home_time;
	if (nprobes > 0 && ((wlc_scan_info->state & SCAN_STATE_PASSIVE) == 0))
		scan_info->nprobes = (uint8)nprobes;
	if (save_prb)
		wlc_scan_info->state |= SCAN_STATE_SAVE_PRB;
	if (include_cache && SCANCACHE_ENAB(wlc_scan_info))
		wlc_scan_info->state |= SCAN_STATE_INCLUDE_CACHE;

	if (scan_flags & WL_SCANFLAGS_HOTSPOT) {
#ifdef ANQPO
		if (SCAN_ANQPO_ENAB(scan_info)) {
			wl_scan_anqpo_scan_start(scan_info);
			wlc_scan_info->is_hotspot_scan = TRUE;
		}
#endif // endif
	} else {
		wlc_scan_info->is_hotspot_scan = FALSE;
	}

#ifdef STA
#ifdef WL11D
	/* If we're doing autocountry, clear country info accumulation */
	if (IS_AUTOCOUNTRY_ENAB(scan_info))
		wlc_scan_11d_scan_start(scan_info);
#endif /* WL11D */
#endif /* STA */

#ifdef WL_PROXDETECT
	if (PROXD_ENAB(wlc->pub)) {
		wlc_scan_info->flag &= ~SCAN_FLAG_SWITCH_CHAN;
		if (scan_flags & WL_SCANFLAGS_SWTCHAN) {
			wlc_scan_info->flag |= SCAN_FLAG_SWITCH_CHAN;
		}
	}
#endif // endif

	WL_SCAN(("wl%d: %s: wlc_scan params: nprobes %d dwell active/passive %dms/%dms home %dms"
		" flags %d\n",
		scan_info->unit, __FUNCTION__, scan_info->nprobes, scan_info->active_time,
		scan_info->passive_time, scan_info->home_time, wlc_scan_info->state));

	wlc_scan_default_channels(wlc_scan_info, scan_info->chanspec_list,
	                          &scan_info->channel_num);

	if (scan_flags & WL_SCANFLAGS_PROHIBITED) {
		scan_info->scan_pub->state |= SCAN_STATE_PROHIBIT;
		num = wlc_scan_prohibited_channels(scan_info,
			&scan_info->chanspec_list[scan_info->channel_num],
			(MAXCHANNEL - scan_info->channel_num));
		scan_info->channel_num += num;
	} else
		scan_info->scan_pub->state &= ~SCAN_STATE_PROHIBIT;

	if (scan_flags & WL_SCANFLAGS_OFFCHAN)
		scan_info->scan_pub->state |= SCAN_STATE_OFFCHAN;
	else
		scan_info->scan_pub->state &= ~SCAN_STATE_OFFCHAN;

	if (IS_SIM_ENAB(scan_info)) {
		/* QT hack: abort scan since full scan may take forever */
		scan_info->channel_num = 1;
	}

	/* set required and optional params */
	/* If IBSS Lock Out feature is turned on, set the scan type to BSS only */
	wlc_scan_info->bss_type =
		(IS_IBSS_ALLOWED(scan_info) == FALSE)?DOT11_BSSTYPE_INFRASTRUCTURE:bss_type;
	bcopy((const char*)bssid, (char*)&wlc_scan_info->bssid, ETHER_ADDR_LEN);

	/* allocate memory for ssid list, using prealloc if sufficient */
	ASSERT(scan_info->ssid_list == scan_info->ssid_prealloc);
	if (scan_info->ssid_list != scan_info->ssid_prealloc) {
		WL_ERROR(("wl%d: %s: ssid_list not set to prealloc\n",
		          scan_info->unit, __FUNCTION__));
	}
	if (nssid > scan_info->nssid_prealloc) {
		scan_info->ssid_list = MALLOC(scan_info->osh,
		                              nssid * sizeof(wlc_ssid_t));
		/* failed, cap at prealloc (best effort) */
		if (scan_info->ssid_list == NULL) {
			nssid = scan_info->nssid_prealloc;
			scan_info->ssid_list = scan_info->ssid_prealloc;
		}
	}
	/* Now ssid_list is the right size for [current] nssid count */

	bcopy(ssid, scan_info->ssid_list, (sizeof(wlc_ssid_t) * nssid));
	scan_info->nssid = nssid;

#ifdef WLP2P
	if (IS_P2P_ENAB(scan_info)) {
		scan_info->ssid_wildcard_enabled = FALSE;
		for (i = 0; i < nssid; i ++) {
			if (scan_info->ssid_list[i].SSID_len == 0)
				wlc_p2p_fixup_SSID(wlc->p2p, cfg,
					&scan_info->ssid_list[i]);
			if (scan_info->ssid_list[i].SSID_len == 0)
				scan_info->ssid_wildcard_enabled = TRUE;
		}
	}
#endif // endif

	/* channel list validation */
	if (channel_num > MAXCHANNEL) {
		WL_ERROR(("wl%d: %s: wlc_scan bad param channel_num %d greater than max %d\n",
			scan_info->unit, __FUNCTION__, channel_num, MAXCHANNEL));
		channel_num = 0;
	}
	if (channel_num > 0 && chanspec_list == NULL) {
		WL_ERROR(("wl%d: %s: wlc_scan bad param channel_list was NULL with channel_num ="
			" %d\n",
			scan_info->unit, __FUNCTION__, channel_num));
		channel_num = 0;
	}
	for (i = 0; i < channel_num; i++) {
		if ((scan_flags & WL_SCANFLAGS_PROHIBITED) ||
			WLC_CNTRY_DEFAULT_ENAB(wlc)) {
			if (wf_channel2mhz(CHSPEC_CHANNEL(chanspec_list[i]),
				CHSPEC_IS5G(chanspec_list[i]) ?
				WF_CHAN_FACTOR_5_G : WF_CHAN_FACTOR_2_4_G) == -1)
				channel_num = 0;
		}
		else if (!wlc_scan_valid_chanspec_db(scan_info, chanspec_list[i])) {
			channel_num = 0;
		}
	}

	if (channel_num > 0) {
		for (i = 0; i < channel_num; i++)
			scan_info->chanspec_list[i] = chanspec_list[i];
		scan_info->channel_num = channel_num;
	}
#ifdef BCMCCX
	else if (IS_CCX_ENAB(scan_info))
		/* no fast roam if the roam channel list is invalid */
		wlc->ccx->fast_roam = FALSE;
#endif // endif
#ifdef WLMSG_ROAM
	bcopy(&ssid->SSID, SSIDbuf, ssid->SSID_len);
	SSIDbuf[ssid->SSID_len] = 0;
	WL_ROAM(("SCAN for '%s' %d SSID(s) %d channels\n", SSIDbuf, nssid, channel_num));
#endif /* WLMSG_ROAM */
#ifdef EXTENDED_SCAN
	/* no dynamic allocation for extended-scan chan_list yet, validate against prealloc */
	if (extdscan && (scan_info->channel_num > scan_info->nchan_prealloc)) {
		WL_ERROR(("wl%d: wlc_scan channel_num %d exceeds prealloc %d (arg %d)\n",
		          scan_info->unit, scan_info->channel_num,
		          scan_info->nchan_prealloc, channel_num));
		scan_info->channel_num = scan_info->nchan_prealloc;
	}
#endif /* EXTENDED_SCAN */

#ifdef BCMDBG
	if (WL_INFORM_ON()) {
		char chan_list_buf[128];
		struct bcmstrbuf b;

		bcm_binit(&b, chan_list_buf, sizeof(chan_list_buf));

		for (i = 0; i < scan_info->channel_num; i++) {
			bcm_bprintf(&b, " %d", CHSPEC_CHANNEL(scan_info->chanspec_list[i]));

			if ((i % 8) == 7 || (i + 1) == scan_info->channel_num) {
				WL_INFORM(("wl%d: wlc_scan: scan channels %s\n", scan_info->unit,
					chan_list_buf));
				bcm_binit(&b, chan_list_buf, sizeof(chan_list_buf));
			}
		}
	}
#endif /* BCMDBG */

	if ((wlc_scan_info->state & SCAN_STATE_SUPPRESS) || (!scan_info->channel_num)) {
		int status;

		WL_INFORM(("wl%d: %s: scan->state %d scan->channel_num %d\n",
			scan_info->unit, __FUNCTION__,
			wlc_scan_info->state, scan_info->channel_num));

		if (wlc_scan_info->state & SCAN_STATE_SUPPRESS)
			status = WLC_E_STATUS_SUPPRESS;
		else
			status = WLC_E_STATUS_NOCHANS;

		if (scan_in_progress)
			wlc_scan_abort(wlc_scan_info, status);

		/* no scan now, but free any earlier leftovers */
		wlc_scan_bss_list_free(scan_info);

		if (fn != NULL)
			(fn)(arg, status, SCAN_USER(scan_info, cfg));

		wlc_scan_ssidlist_free(scan_info);

#if defined(BCMDBG) || defined(WLMSG_INFORM)
		if (ssidbuf != NULL)
			 MFREE(scan_info->osh, (void *)ssidbuf, SSID_FMT_BUF_LEN);
#endif // endif
		return BCME_EPERM;
	}

#ifdef STA
	if (!IS_EXTSTA_ENAB(scan_info))
		if (scan_in_progress && !IS_AS_IN_PROGRESS(scan_info))
			wlc_scan_callback(scan_info, WLC_E_STATUS_ABORT);
#endif /* STA */

	scan_info->bsscfg = SCAN_USER(scan_info, cfg);

	scan_info->cb = fn;
	scan_info->cb_arg = arg;

	scan_info->act_cb = act_cb;
	scan_info->act_cb_arg = act_cb_arg;

	wlc_scan_info->usage = (uint8)usage;

	/* start the scan with the results cleared */
	scan_info->away_channels_cnt = 0;
	if (!away_channels_limit)
		away_channels_limit = MAX(1, scan_info->home_away_time / scan_info->active_time);

	scan_info->away_channels_limit = away_channels_limit;
	scan_info->extdscan = extdscan;

#ifdef EXT_STA
	/* Save flag: suppress ssid in probes during connection attempt
	 * following hibernation
	 */
	if (IS_EXTSTA_ENAB(scan_info))
		scan_info->suppress_ssid = suppress_ssid;
#endif /* EXT_STA */

	/* extd scan for nssids one ssid per each pass..  */
	scan_info->npasses = (scan_info->extdscan && nssid) ? nssid : scan_info->nprobes;
	scan_info->channel_idx = 0;
	if (chanspec_start != 0) {
		for (i = 0; i < scan_info->channel_num; i++) {
			if (scan_info->chanspec_list[i] == chanspec_start) {
				scan_info->channel_idx = i;
				WL_INFORM(("starting new iscan on channel %d\n",
				           CHSPEC_CHANNEL(chanspec_start)));
				break;
			}
		}
	}

#if !defined(WLOFFLD) && !defined(SCANOL)
	/* If we are switching away from radar home_chanspec
	 * because STA scans (normal/Join/Roam) with
	 * atleast one local 11H AP in radar channel,
	 * turn of radar_detect.
	 * NOTE: Implied that upstream AP assures this radar
	 * channel is clear.
	 */
	if (WL11H_AP_ENAB(wlc) && WLC_APSTA_ON_RADAR_CHANNEL(wlc) &&
		wlc_radar_chanspec(wlc->cmi, wlc->home_chanspec)) {
		WL_REGULATORY(("wl%d: %s Moving from home channel dfs OFF\n",
			wlc->pub->unit, __FUNCTION__));
		wlc_set_dfs_cacstate(wlc->dfs, OFF);
	}
#endif /* !defined(WLOFFLD) && !defined(SCANOL)) */
	wlc_scan_info->in_progress = TRUE;
#ifdef WLOLPC
	/* if on olpc chan notify - if needed, terminate active cal; go off channel */
	if (OLPC_ENAB(wlc)) {
		wlc_olpc_eng_hdl_chan_update(wlc->olpc_info);
	}
#endif /* WLOLPC */
	wlc_phy_hold_upd(SCAN_GET_PI_PTR(scan_info), PHY_HOLD_FOR_SCAN, TRUE);

#if defined(STA) && !defined(SCANOL)
	wlc_scan_info->scan_start_time = OSL_SYSUPTIME();
#endif /* STA && !SCANOL */

#ifdef WL_BCN_COALESCING
	wlc_bcn_clsg_disable(wlc->bc, BCN_CLSG_SCAN_MASK,
		wlc_scan_info->in_progress ? BCN_CLSG_SCAN_MASK : 0);
#endif /* WL_BCN_COALESCING */

#ifdef WLOFFLD
	if (WLOFFLD_CAP(wlc)) {
		wlc_ol_rx_deferral(wlc->ol, OL_SCAN_MASK, OL_SCAN_MASK);
	}
#endif // endif
	scan_info->pass = WLC_SCAN_START;
	/* ...and free any leftover responses from before */
	wlc_scan_bss_list_free(scan_info);

	/* keep core awake to receive solicited probe responses, SCAN_IN_PROGRESS is TRUE */
	ASSERT(SCAN_STAY_AWAKE(scan_info));
	wlc_scan_set_wake_ctrl(scan_info);

	{
		/* send out AF as soon as possible to aid reliability of GON */
		WLC_SCAN_DEL_TIMER(scan_info);
		WLC_SCAN_ADD_TIMER(scan_info, 0, 0);
	}

#if defined(BCMDBG) || defined(WLMSG_INFORM)
	if (ssidbuf != NULL)
		 MFREE(scan_info->osh, (void *)ssidbuf, SSID_FMT_BUF_LEN);
#endif // endif
#ifdef WLAWDL
	if (AWDL_SUPPORT(wlc->pub))
		wlc_send_awdl_scan_evt(wlc, WLC_E_AWDL_SCAN_START);
#endif // endif
	/* if a scan is in progress, allow the next callback to restart the scan state machine */
	return BCME_OK;
}

void
wlc_scan_timer_update(wlc_scan_info_t *wlc_scan_info, uint32 ms)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;

	WLC_SCAN_DEL_TIMER(scan_info);
	WLC_SCAN_ADD_TIMER(scan_info, ms, 0);
}

/* return number of channels in current scan */
int
wlc_scan_chnum(wlc_scan_info_t *wlc_scan_info)
{
	return ((scan_info_t *)wlc_scan_info->scan_priv)->channel_num;
}

#if defined(STA) && !defined(SCANOL)
uint32
wlc_curr_roam_scan_time(wlc_info_t *wlc)
{
	uint32 curr_roam_scan_time = 0;
	/* Calculate awake time due to active roam scan */
	if (wlc_scan_inprog(wlc) && (AS_IN_PROGRESS(wlc))) {
		wlc_assoc_t *as = wlc->assoc_req[0]->assoc;
		if (as->type == AS_ROAM)
			curr_roam_scan_time = wlc_get_curr_scan_time(wlc);
	}
	return curr_roam_scan_time;
}

uint32
wlc_get_curr_scan_time(wlc_info_t *wlc)
{
	scan_info_t *scan_info = wlc->scan->scan_priv;
	wlc_scan_info_t *scan_pub = scan_info->scan_pub;
	if (scan_pub->in_progress)
		return (OSL_SYSUPTIME() - scan_pub->scan_start_time);
	return 0;
}

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
			wlc_assoc_t *as = wlc->assoc_req[0]->assoc;

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
		else if (scan_pub->usage == SCAN_ENGINE_USAGE_NORM ||
			scan_pub->usage == SCAN_ENGINE_USAGE_ESCAN)
			scan_data = &scan_info->scan_stats->user_scans;
		else
			scan_data = &scan_info->scan_stats->other_scans;

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
}

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

#endif /* STA && !SCANOL */

/* abort the current scan, and return to home channel */
void
wlc_scan_abort(wlc_scan_info_t *wlc_scan_info, int status)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
#if defined(EXT_STA) || defined(WLP2P)
	wlc_bsscfg_t *scan_cfg = scan_info->bsscfg;
#endif /* EXT_STA || WLP2P */

	if (!SCAN_IN_PROGRESS(wlc_scan_info))
		return;

	WL_INFORM(("wl%d: %s: aborting scan in progress\n", scan_info->unit, __FUNCTION__));
#ifdef WLRCC
	if ((WLRCC_ENAB(scan_info->wlc->pub)) && (scan_info->bsscfg->roam->n_rcc_channels > 0))
		scan_info->bsscfg->roam->rcc_valid = TRUE;
#endif // endif
	if (SCANCACHE_ENAB(wlc_scan_info) &&
#ifdef WLP2P
	    !BSS_P2P_DISC_ENAB(scan_info->wlc, scan_cfg) &&
#endif // endif
		TRUE) {
		wlc_scan_cache_result(scan_info);
	}

	wlc_scan_bss_list_free(scan_info);
	wlc_scan_terminate(wlc_scan_info, status);

	if (IS_N_ENAB(scan_info) && IS_COEX_ENAB(scan_info))
		wlc_ht_obss_scan_update(scan_info, WLC_SCAN_ABORT);

#ifdef EXT_STA
	/* Required for Vista */
	if (IS_EXTSTA_ENAB(scan_info))
		wlc_scan_bss_mac_event(scan_info, scan_cfg, WLC_E_SCAN_COMPLETE, NULL,
			WLC_E_STATUS_ABORT, 0, 0, NULL, 0);
#endif // endif
}

void
wlc_scan_abort_ex(wlc_scan_info_t *wlc_scan_info, wlc_bsscfg_t *cfg, int status)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
	if (scan_info->bsscfg == cfg)
		wlc_scan_abort(wlc_scan_info, status);
}

/* wlc_scan_terminate is called when user (from intr/dpc or ioctl) requests to
 * terminate the scan. However it may also be invoked from its own call chain
 * from which tx status processing is executed. Use the flag TERMINATE to prevent
 * it from being re-entered.
 */
/* Driver's primeter lock will prevent wlc_scan_terminate from being invoked from
 * intr/dpc or ioctl when a timer callback is running. However it may be invoked
 * from the wlc_scantimer call chain in which tx status processing is executed.
 * So use the flag IN_TMR_CB to prevent wlc_scan_terminate being re-entered.
 */
void
wlc_scan_terminate(wlc_scan_info_t *wlc_scan_info, int status)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
#if defined(WLOFFLD) || defined(WL_BCN_COALESCING)
	wlc_info_t *wlc = scan_info->wlc;
#endif // endif
#ifdef STA
	int idx;
	wlc_bsscfg_t *cfg;
#endif // endif

	if (!SCAN_IN_PROGRESS(wlc_scan_info))
		return;

	/* ignore if already in terminate/finish process */
	if (wlc_scan_info->state & SCAN_STATE_TERMINATE) {
		WL_SCAN(("wl%d: %s: ignore wlc_scan_terminate request\n",
		         scan_info->unit, __FUNCTION__));
		return;
	}

	/* protect wlc_scan_terminate being recursively called from the callchain
	 * ...->wlc_scan_terminate->wlc_scan_return_home_channel->txstatus_proc->
	 * ...->wlc_scan_terminate
	 */

	wlc_scan_info->state |= SCAN_STATE_TERMINATE;

	/* defer the termination if called from the timer callback */
	if (wlc_scan_info->state & SCAN_STATE_IN_TMR_CB) {
		WL_SCAN(("wl%d: %s: defer wlc_scan_terminate request\n",
		         scan_info->unit, __FUNCTION__));
		scan_info->status = status;
		return;
	}

	/* abort the current scan, and return to home channel */
	WL_INFORM(("wl%d: %s: terminating scan in progress\n", scan_info->unit, __FUNCTION__));

#ifdef WLSCAN_PS
	/* scan is done. now reset the cores */
	if (WLSCAN_PS_ENAB(scan_info->wlc->pub))
		wlc_scan_ps_config_cores(scan_info, FALSE);
#endif // endif

	/* return to home channel */
	wlc_scan_return_home_channel(scan_info);

#ifdef STA
	/* When ending scan, PM2 timer was likely off: if we're configured
	 * for PM2 and eligible (BSS associated) force timer restart.
	 */
	{
		SCAN_FOREACH_AS_STA(scan_info, idx, cfg) {
			if (cfg->BSS && cfg->pm->PM == PM_FAST)
				wlc_scan_pm2_sleep_ret_timer_start(cfg);
		}
	}
#endif /* STA */

#if !defined(WLOFFLD) && !defined(SCANOL)
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
	if (WL11H_AP_ENAB(scan_info->wlc) &&
		WLC_APSTA_ON_RADAR_CHANNEL(scan_info->wlc) &&
#ifdef STA
		!AS_IN_PROGRESS(scan_info->wlc) &&
#endif // endif
		wlc_radar_chanspec(scan_info->wlc->cmi, scan_info->wlc->home_chanspec)) {
		WL_REGULATORY(("wl%d: %s Join/scan aborted back"
			"to home channel dfs ON\n",
			scan_info->wlc->pub->unit, __FUNCTION__));
		wlc_set_dfs_cacstate(scan_info->wlc->dfs, ON);
	}
#endif /* !defined(WLOFFLD) && !defined(SCANOL)) */

	/* clear scan ready flag */
	wlc_scan_info->state &= ~SCAN_STATE_READY;

	scan_info->pass = WLC_SCAN_ABORT;
	scan_info->channel_idx = -1;
#if defined(STA) && !defined(SCANOL)
	wlc_scan_time_upd(scan_info->wlc, scan_info);
#endif /* STA && ! SCANOL */
	wlc_scan_info->in_progress = FALSE;

	wlc_phy_hold_upd(SCAN_GET_PI_PTR(scan_info), PHY_HOLD_FOR_SCAN, FALSE);
#ifdef WLOLPC
	/* notify scan just terminated - if needed, kick off new cal */
	if (OLPC_ENAB(scan_info->wlc)) {
		wlc_olpc_eng_hdl_chan_update(scan_info->wlc->olpc_info);
	}
#endif /* WLOLPC */

#ifdef WL_BCN_COALESCING
	wlc_bcn_clsg_disable(wlc->bc, BCN_CLSG_SCAN_MASK,
		wlc_scan_info->in_progress ? BCN_CLSG_SCAN_MASK : 0);
#endif /* WL_BCN_COALESCING */
#ifdef WLOFFLD
	if (WLOFFLD_CAP(wlc)) {
		wlc_ol_rx_deferral(wlc->ol, OL_SCAN_MASK, 0);
	}
#endif // endif
	wlc_scan_ssidlist_free(scan_info);

#ifdef STA
	wlc_scan_set_wake_ctrl(scan_info);
	WL_MPC(("wl%d: %s: SCAN_IN_PROGRESS==FALSE, update mpc\n", scan_info->unit, __FUNCTION__));
	wlc_scan_radio_mpc_upd(scan_info);
#endif /* STA */

	/* abort PM indication process */
	wlc_scan_info->state &= ~SCAN_STATE_PSPEND;
	/* abort tx suspend delay process */
	wlc_scan_info->state &= ~SCAN_STATE_DLY_WSUSPEND;
	/* abort TX FIFO suspend process */
	wlc_scan_info->state &= ~SCAN_STATE_WSUSPEND;
	/* delete a future timer explictly */
	WLC_SCAN_DEL_TIMER(scan_info);
	scan_info->pass = WLC_SCAN_START;
#ifdef STA
#ifdef WL11D
	/* If we are in 802.11D mode and we are still waiting to find a
	 * valid Country IE, take this opportunity to parse scan results,
	 * if necessary, and try to apply it.
	 */
	if (IS_AUTOCOUNTRY_ENAB(scan_info))
		wlc_scan_11d_scan_complete(scan_info, status);
#endif /* WL11D */
	wlc_scan_callback(scan_info, status);
#endif /* STA */

	wlc_scan_info->state &= ~SCAN_STATE_TERMINATE;
}

/* return TRUE if completion is pending; FALSE otherwise. */
static bool
wlc_scan_tx_suspend(scan_info_t *scan_info)
{
	wlc_scan_info_t *wlc_scan_info = scan_info->scan_pub;

	if (!IS_ASSOCIATED(scan_info)) {
		WL_SCAN_ENT(scan_info, ("wl%d: %s: not associated, suspend not needed\n",
		                        scan_info->unit, __FUNCTION__));
		return FALSE;
	}

	WL_SCAN_ENT(scan_info, ("wl%d: %s: suspending tx tsf %u...\n",
	                        scan_info->unit, __FUNCTION__,
	                        SCAN_GET_TSF_TIMERLOW(scan_info)));

	/* block any tx traffic */
	SCAN_BLOCK_DATAFIFO_SET(scan_info, DATA_BLOCK_SCAN);
	if (wlc_scan_tx_suspended(scan_info)) {
		WL_SCAN_ENT(scan_info, ("wl%d: %s: tx already suspended...\n",
		                        scan_info->unit, __FUNCTION__));
		return FALSE;
	}

	/* set our TX suspend callback flag first so that even if wlc_tx_suspend()
	 * ever changes to complete synchronously...
	 */
	wlc_scan_info->state |= SCAN_STATE_WSUSPEND;
	_wlc_scan_tx_suspend(scan_info);

	/* WSUSPEND completed immediately */
	if (!(wlc_scan_info->state & SCAN_STATE_WSUSPEND)) {
		WL_SCAN_ENT(scan_info, ("wl%d: %s: tx suspend completed...\n",
		                        scan_info->unit, __FUNCTION__));
		return FALSE;
	}

	/* add a guard timer to move the state machine forward
	 * in case the fifo suspend complete event is lost or
	 * if the event comes too late...
	 */
	WLC_SCAN_ADD_TIMER(scan_info, scan_info->wsuspend_guard_time, 0);

#ifdef BCMDBG
	/* scan terminate test case - requires to be associated with an AP and pssing traffic? */
	if (scan_info->test == SCAN_TEST_ABORT_WSUSPEND ||
	    scan_info->test == SCAN_TEST_ABORT_WSUSPEND_AND_SCAN) {
		WLC_SCAN_ADD_TEST_TIMER(scan_info, 0, 0);
	}
#endif // endif

	return TRUE;
}

/* prepare to leave home channel */
static void
wlc_scan_prepare_off_channel(scan_info_t *scan_info)
{
	WL_SCAN(("wl%d: %s\n", scan_info->unit, __FUNCTION__));
#ifdef STA
	wlc_scan_ibss_disable_all(scan_info);
	wlc_scan_mhf(scan_info, MHF2, MHF2_SKIP_CFP_UPDATE, MHF2_SKIP_CFP_UPDATE, WLC_BAND_ALL);
	wlc_scan_skip_adjtsf(scan_info, TRUE, NULL, WLC_SKIP_ADJTSF_SCAN, WLC_BAND_ALL);
#endif /* STA */

	/* Must disable AP beacons and probe responses first before going away from home channel */
	wlc_scan_ap_mute(scan_info, TRUE, NULL, WLC_AP_MUTE_SCAN);
}

#ifdef STA
/* Indicate PM 1 to associated APs.
 * return TRUE if completion is pending; FALSE otherwise.
 */
static bool
wlc_scan_prepare_pm_mode(scan_info_t *scan_info)
{
	int idx;
	wlc_bsscfg_t *cfg;
	wlc_scan_info_t *wlc_scan_info = scan_info->scan_pub;
#ifdef WLDPT
	bool dpt_pmpending;
#endif // endif
#ifdef WLTDLS
	bool tdls_pmpending;
#endif // endif
	bool quiet_channel = FALSE;

	if (IS_11H_ENAB(scan_info) &&
		wlc_scan_quiet_chanspec(scan_info, SCAN_BAND_PI_RADIO_CHANSPEC(scan_info)))

		quiet_channel = TRUE;

	if (IS_EXPTIME_CNT_ZERO(scan_info) &&
	    !SCAN_TYPE_FOREGROUND(scan_info) && !quiet_channel) {
		/* set our PS callback flag so that wlc_scantimer is called
		 * when the PM State Null Data packet completes
		 */
		wlc_scan_info->state |= SCAN_STATE_PSPEND;
		/* announce PS mode to the AP if we are not already in PS mode */
		SCAN_FOREACH_STA(scan_info, idx, cfg) {
			/* block any PSPoll operations for holding off AP traffic */
			mboolset(cfg->pm->PMblocked, WLC_PM_BLOCK_SCAN);
			if (cfg->associated) {
				if (cfg->pm->PMenabled)
					continue;
#ifdef WLMCHAN
				/* For mchan operation, only enable PS for STA on our home */
				/* channel */
				if (IS_MCHAN_ENAB(scan_info) && cfg->chan_context &&
				    (cfg->chan_context->chanspec != SCAN_HOME_CHANNEL(scan_info))) {
					WL_MCHAN(("wl%d.%d: %s: skip pmpend for cfg on other"
						"channel\n", scan_info->unit,
						WLC_BSSCFG_IDX(cfg), __FUNCTION__));
					continue;
				}
#endif // endif
				WL_SCAN_ENT(scan_info, ("wl%d.%d: wlc_scan_prepare_pm_mode: "
				                        "entering PM mode tsf %u...\n",
				                        scan_info->unit, WLC_BSSCFG_IDX(cfg),
				                        SCAN_GET_TSF_TIMERLOW(scan_info)));

				wlc_scan_set_pmstate(cfg, TRUE);

				/* set forcePMenabled if it was changed from FALSE to TRUE.
				 * this could still remain at FALSE if it is IBSS bsscfg.
				 */
				if (cfg->pm->PMenabled)
					cfg->pm->forcePMenabled = WLC_PM_FORCE_PMENABLED_SCAN;
			}
#ifdef WLTDLS
			else if (BSS_TDLS_ENAB(scan_info->wlc, cfg) && !cfg->pm->PMenabled) {
				wlc_tdls_notify_pm_state(scan_info->wlc->tdls, cfg, TRUE);

				/* set forcePMenabled if it was changed from FALSE to TRUE.
				 * this could still remain at FALSE depending on scb flags
				 */
				if (cfg->pm->PMenabled)
					cfg->pm->forcePMenabled = WLC_PM_FORCE_PMENABLED_SCAN;
			}
#endif // endif
		}
		/* We are supposed to wait for PM0->PM1 transition to finish but in case
		 * we failed to send PM indications or failed to receive ACKs fake a PM0->PM1
		 * transition so that anything depending on the transition to finish can move
		 * forward i.e. scan engine can continue.
		 * N.B.: to get wlc->PMpending updated in case all BSSs have done above
		 * fake PM0->PM1 transitions.
		 */
		_wlc_scan_pm_pending_complete(scan_info);
#ifdef WLDPT
		dpt_pmpending = FALSE;
		SCAN_FOREACH_AS_STA(scan_info, idx, cfg) {
			if (WLC_DPT_PM_PENDING(scan_info->wlc, cfg)) {
				dpt_pmpending = TRUE;
				break;
			}
		}
#endif // endif

#ifdef WLTDLS
		tdls_pmpending = FALSE;
		SCAN_FOREACH_STA(scan_info, idx, cfg) {
			if (BSS_TDLS_ENAB(scan_info->wlc, cfg) && cfg->pm->PMpending) {
				tdls_pmpending = TRUE;
				break;
			}
		}
#endif // endif

		/* clear our callback flag if PMpending is false, indicating
		 * that no PM State Null Data packet was sent by
		 * wlc_scan_set_pmstate()
		 */
		if (!IS_PM_PENDING(scan_info) &&
#ifdef WLDPT
		    !dpt_pmpending &&
#endif // endif
#ifdef WLTDLS
			!tdls_pmpending &&
#endif // endif
		    TRUE)
			wlc_scan_info->state &= ~SCAN_STATE_PSPEND;
	}
	wlc_scan_set_wake_ctrl(scan_info);

	/* PSPEND completed immediately */
	if (!(wlc_scan_info->state & SCAN_STATE_PSPEND)) {
		WL_SCAN_ENT(scan_info, ("wl%d: %s: PM completed.\n",
		                        scan_info->unit, __FUNCTION__));
		return FALSE;
	}

	/* add a guard timer to move the state machine forward
	 * if the PM state machine has any problems or if the
	 * PM indication comes too late...
	 */
	WLC_SCAN_ADD_TIMER(scan_info, scan_info->pspend_guard_time, 0);

#ifdef BCMDBG
	/* scan terminate test case - requires to be associated with an AP... */
	if (scan_info->test == SCAN_TEST_ABORT_PSPEND ||
	    scan_info->test == SCAN_TEST_ABORT_PSPEND_AND_SCAN) {
		WLC_SCAN_ADD_TEST_TIMER(scan_info, 0, 0);
	}
#endif // endif

	return TRUE;
}
#endif /* STA */

#ifndef SCANOL

/* Start the delay period after announcing PM mode if configured.
 * return TRUE if delay timer is armed, FALSE otherwise.
 */
static bool
wlc_scan_tx_suspend_dly(wlc_scan_info_t *wlc_scan_info)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
	wlc_info_t *wlc = scan_info->wlc;

	if (wlc->pm2_radio_shutoff_dly == 0)
		return FALSE;

	wlc_scan_info->state |= SCAN_STATE_DLY_WSUSPEND;
	WLC_SCAN_ADD_TIMER(scan_info, wlc->pm2_radio_shutoff_dly, 0);

	return TRUE;
}
#endif /* SCANOL */

#ifdef WLAWDL
/* SCAN and Roam synchronization with AWDL
 * Following cases  need to be taken care:
 * 1. Scan starts, AWDL state machine in AWindow - Action: Postpone Scan till AWindow and
 *      extensions, resume SCAN later.
 * 2. Scanning in progress, AWDL state machine switches to AWindow, Action: Suspend Scan till
 *     AWindow and extensions, resume SCAN.
 *     - Before Scan Channel is switched AWDL checks if its going to have AW in few msecs?
 *       If so then we suspend the scan and resume after AW end.
 *
 * 3. Roam Starts, Roam event sent to host, host to change presencemode so that Roam scan can
 *     take precedence.
 */
uint32	wlc_get_next_chan_scan_time(wlc_scan_info_t *wlc_scan_info)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
	wlc_info_t *wlc = scan_info->wlc;
	chanspec_t next_chanspec;
	bool passive_scan = FALSE;

	if (scan_info->channel_idx < scan_info->channel_num - 1)
		next_chanspec = scan_info->chanspec_list[scan_info->channel_idx];
	else
		return 0;

	if (scan_info->scan_pub->state & SCAN_STATE_PASSIVE)
		passive_scan = TRUE;

	if (passive_scan || wlc_quiet_chanspec(wlc->cmi, next_chanspec) ||
		!wlc_valid_chanspec_db(wlc->cmi, next_chanspec)) {
		return CHANNEL_PASSIVE_DWELLTIME(scan_info);
	}
	else {
		return CHANNEL_ACTIVE_DWELLTIME(scan_info);
	}
}

void wlc_suspend_scan(wlc_scan_info_t *wlc_scan_info)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
	wlc_info_t *wlc = scan_info->wlc;

	if (!SCAN_IN_PROGRESS(wlc_scan_info))
		return;
	if (wlc_awdl_pscan_in_progress(wlc))
		return;
	if (wlc_scan_info->state & SCAN_STATE_AWDL_AW)
		return;
	wlc_scan_return_home_channel(scan_info);
	wlc_scan_info->state |= SCAN_STATE_AWDL_AW;
	wlc_scan_info->state &= ~SCAN_STATE_READY;
	WL_SCAN(("AW Scan Suspend %x\n", wlc_scan_info->state));
}

void wlc_resume_scan(wlc_scan_info_t *wlc_scan_info)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
	wlc_info_t *wlc = scan_info->wlc;

	if ((wlc_scan_info->state & SCAN_STATE_AWDL_AW) && SCAN_IN_PROGRESS(wlc_scan_info) &&
		!wlc_awdl_pscan_in_progress(wlc)) {
		uint timeout = TXPKTPENDTOT(wlc);
		WLC_SCAN_DEL_TIMER(scan_info);
		if (timeout > 4)
			timeout = 4;
		WLC_SCAN_ADD_TIMER(scan_info, timeout, 0);
		if (!(wlc_scan_info->state & SCAN_STATE_HOME_TIME_SPENT))
			scan_info->pass = WLC_SCAN_START;
	}

	wlc_scan_info->state &= ~SCAN_STATE_AWDL_AW;
}

/*
*****************************************************************************
* Function:   wlc_do_awdl_pscan_pass
*
* Purpose:    Function to do start scan pass send probes if we have to
*
* Parameters: wlc_info_t
*
* Returns:    None.
*****************************************************************************
*/
static void wlc_do_awdl_pscan_pass(wlc_info_t *wlc)
{
	scan_info_t *scan_info = (scan_info_t *)wlc->scan->scan_priv;
	uint dwell = 0;

	if (!(scan_info->scan_pub->state & SCAN_STATE_PASSIVE) &&
		!wlc_quiet_chanspec(wlc->cmi, WLC_BAND_PI_RADIO_CHANSPEC) &&
		wlc_valid_chanspec_db(wlc->cmi, WLC_BAND_PI_RADIO_CHANSPEC)) {
		if (scan_info->act_cb)
			(scan_info->act_cb)(wlc, scan_info->act_cb_arg, &dwell);
		else
			wlc_scan_sendprobe(scan_info);
		if (scan_info->pass < scan_info->npasses)
			WLC_SCAN_ADD_TIMER(scan_info, wlc_awdl_pscan_dwell_time(wlc), 0);
		WL_SCAN(("%s(): pass = %d\n", __FUNCTION__, scan_info->pass));
	}
	/* record phy noise for the scan channel */
	wlc_lq_noise_sample_request(wlc, WLC_NOISE_REQUEST_SCAN, CHSPEC_CHANNEL(wlc->chanspec));
}

/*
*****************************************************************************
* Function:   wlc_awdl_piggyback_scan
*
* Purpose:    Function to handle start/nextpass/end of  scan dwell.
*
* Parameters: wlc_info_t , status
*
* Returns:    None.
*****************************************************************************
*/
void wlc_awdl_piggyback_scan(wlc_info_t *wlc, uint32 status)
{
	scan_info_t *scan_info = (scan_info_t *)wlc->scan->scan_priv;
	uint16 pscan_state;

	if (!wlc_awdl_pscan_in_progress(wlc))
		return;
	WLC_SCAN_DEL_TIMER(scan_info);
	pscan_state = wlc_awdl_get_pscan_state(wlc);
	switch (status) {
		case AWDL_SCAN_DWELL_START :
			wlc_awdl_set_pscan_state(wlc, pscan_state | SCAN_STATE_AWDL_PSCAN_START);
			wlc->bcnmisc_scan = TRUE;
			wlc_mac_bcn_promisc(wlc);
			scan_info->pass = 1;
			/* Fall through */
		case AWDL_SCAN_NEXT_PASS :
			wlc_do_awdl_pscan_pass(wlc);
			break;

		case AWDL_SCAN_DWELL_END :
			wlc->bcnmisc_scan = FALSE;
			wlc_mac_bcn_promisc(wlc);
			break;

		case AWDL_SCAN_END :
			wlc_awdl_set_pscan_state(wlc, pscan_state | SCAN_STATE_AWDL_PSCAN_DONE);
			/* wraps up scan process. */
			wlc_scantimer(scan_info);
			/* Fall through */
		case AWDL_SCAN_DONE :
			wlc->bcnmisc_scan = FALSE;
			wlc_mac_bcn_promisc(wlc);
			wlc_awdl_set_pscan_state(wlc, pscan_state &
				(~(SCAN_STATE_AWDL_PSCAN_DONE | SCAN_STATE_AWDL_PSCAN_START)));
			wlc_awdl_reset_pscan_in_progress(wlc);
			break;

		default :
			break;
	}
}
#endif /* WLAWDL */

static void
wlc_scantimer(void *arg)
{
	scan_info_t *scan_info = (scan_info_t *)arg;
	wlc_scan_info_t	*wlc_scan_info = scan_info->scan_pub;
#ifndef SCANOL
	wlc_info_t *wlc = scan_info->wlc;
#endif // endif
#ifdef STA
	int idx;
	wlc_bsscfg_t *cfg;
#endif /* STA */
	wlc_bsscfg_t *scan_cfg;
	chanspec_t chanspec;
	chanspec_t next_chanspec;
	int8 passes;
	int status = WLC_E_STATUS_SUCCESS;

#ifndef SCANOL
	UNUSED_PARAMETER(wlc);
#endif // endif

	WL_SCAN_ENT(scan_info, ("wl%d: %s: enter, state 0x%x tsf %u\n",
	                        scan_info->unit, __FUNCTION__,
	                        wlc_scan_info->state,
	                        SCAN_GET_TSF_TIMERLOW(scan_info)));

	wlc_scan_info->state |= SCAN_STATE_IN_TMR_CB;

	/* Store for later use */
	scan_cfg = scan_info->bsscfg;

	if (SCAN_DEVICEREMOVED(scan_info)) {
		WL_ERROR(("wl%d: %s: dead chip\n", scan_info->unit, __FUNCTION__));
		if (SCAN_IN_PROGRESS(wlc_scan_info)) {
			wlc_scan_bss_list_free(scan_info);
			wlc_scan_callback(scan_info, WLC_E_STATUS_ABORT);

			/* Post a BSS event if an interface is attached to it */
			wlc_scan_bss_mac_event(scan_info, scan_cfg, WLC_E_SCAN_COMPLETE, NULL,
			                  WLC_E_STATUS_ABORT, 0, 0, NULL, 0);
		}
		SCAN_WL_DOWN(scan_info);
		goto exit;
	}
#ifdef WLAWDL
	if (AWDL_ENAB(wlc->pub) && wlc_awdl_pscan_in_progress(wlc)) {
		if (wlc_awdl_get_pscan_state(wlc) & SCAN_STATE_AWDL_PSCAN_DONE)
			goto done;
		if (wlc_awdl_get_pscan_state(wlc) & SCAN_STATE_AWDL_PSCAN_START) {
			if (scan_info->pass < scan_info->nprobes) {
				scan_info->pass++;
				wlc_awdl_piggyback_scan(wlc, AWDL_SCAN_NEXT_PASS);
			}
		}
		goto exit;
	}
#endif /* WLAWDL */

	if (scan_info->pass == WLC_SCAN_ABORT) {
		WL_SCAN_ENT(scan_info, ("wl%d: %s: move state to START\n",
		                        scan_info->unit, __FUNCTION__));
		scan_info->pass = WLC_SCAN_START;
		goto exit;
	}

	if (!SCAN_ISUP(scan_info))
		goto exit;

#ifdef WLAWDL
	if (AWDL_SUPPORT(wlc->pub) && !wlc_awdl_scan_interleave_allowed(wlc)) {
		wlc_suspend_scan(wlc_scan_info);
		goto exit;
	}
#endif // endif

	/* PM indication guard timer or off channel delay timer fired */
	if (wlc_scan_info->state & (SCAN_STATE_PSPEND | SCAN_STATE_DLY_WSUSPEND)) {
		if (wlc_scan_info->state & SCAN_STATE_PSPEND) {
			ASSERT(!(wlc_scan_info->state & SCAN_STATE_DLY_WSUSPEND));
			WL_SCAN_ENT(scan_info, ("wl%d: %s: PM indicaton timeout\n",
			                        scan_info->unit, __FUNCTION__));
		}
		if (wlc_scan_info->state & SCAN_STATE_DLY_WSUSPEND) {
			ASSERT(!(wlc_scan_info->state & SCAN_STATE_PSPEND));
			WL_SCAN_ENT(scan_info, ("wl%d: %s: tx suspend delay timer expired\n",
			                        scan_info->unit, __FUNCTION__));
		}
		wlc_scan_info->state &= ~(SCAN_STATE_PSPEND | SCAN_STATE_DLY_WSUSPEND);

		/* N.B.: return value TRUE means task pending and guard timer armed */
		if (wlc_scan_tx_suspend(scan_info)) {
			WL_SCAN_ENT(scan_info, ("wl%d: %s: waiting for tx suspend to "
			                        "complete\n",
			                        scan_info->unit, __FUNCTION__));
			goto exit;
		}
	}

	/* tx suspend guard timer fired */
	if (wlc_scan_info->state & SCAN_STATE_WSUSPEND) {
		ASSERT(!(wlc_scan_info->state & SCAN_STATE_PSPEND));
		ASSERT(!(wlc_scan_info->state & SCAN_STATE_DLY_WSUSPEND));
		WL_SCAN(("wl%d: %s: tx suspend timeout\n", scan_info->unit, __FUNCTION__));
		wlc_scan_info->state &= ~SCAN_STATE_WSUSPEND;

		/* N.B.: move forward the state machine even when we have failed tx suspend */
	}

	scan_info->pass++;
	chanspec = scan_info->chanspec_list[scan_info->channel_idx];
#ifdef STA
	SCAN_ISCAN_CHANSPEC_LAST(scan_info) = chanspec;
#endif // endif
	passes = (wlc_scan_quiet_chanspec(scan_info, chanspec) ||
	          !wlc_scan_valid_chanspec_db(scan_info, chanspec)) ? 1 : scan_info->npasses;

	if (scan_info->pass > passes) {
		if (scan_info->pass == passes + 1) {
			/* scan passes complete for the current channel */
			WL_SCAN(("wl%d: %s: %sscanned channel %d, total responses %d, tsf %u\n",
			         scan_info->unit, __FUNCTION__,
			         ((wlc_scan_quiet_chanspec(scan_info, chanspec) &&
			           !(wlc_scan_info->state & SCAN_STATE_RADAR_CLEAR)) ?
			          "passively ":""),
			         CHSPEC_CHANNEL(chanspec), SCAN_RESULT_MEB(scan_info, count),
			         SCAN_GET_TSF_TIMERLOW(scan_info)));

			/* reset the radar clear flag since we will be leaving the channel */
			wlc_scan_info->state &= ~SCAN_STATE_RADAR_CLEAR;
		}

		if (scan_info->channel_idx < scan_info->channel_num - 1)
			next_chanspec = scan_info->chanspec_list[scan_info->channel_idx + 1];
		else
			next_chanspec = INVCHANSPEC;

		/* keep track of the number of channels scanned since the last
		 * time we returned to the home channel
		 */
		if (wlc_scan_info->state & SCAN_STATE_READY)
			scan_info->away_channels_cnt++;
		else
			scan_info->away_channels_cnt = 0;

		/* If the home_time is non-zero,
		 * and there are more channels to scan,
		 * and we reached the away channel limit or the channel
		 *	we just scanned or are about to scan is a passive scan channel,
		 * return to the home channel before scanning the next channel
		 */
		if (scan_info->pass == (passes + 1) &&
		    scan_info->home_time > 0 &&
		    next_chanspec != INVCHANSPEC &&
		    (scan_info->away_channels_cnt >= scan_info->away_channels_limit ||
		     wlc_scan_quiet_chanspec(scan_info, chanspec) ||
		     wlc_scan_quiet_chanspec(scan_info, next_chanspec))) {
			/* return to home channel */
			wlc_scan_return_home_channel(scan_info);

			/* deferred termination */
			if (wlc_scan_info->state & SCAN_STATE_TERMINATE) {
				WL_SCAN(("wl%d: %s: process deferred wlc_scan_terminate request\n",
				         scan_info->unit, __FUNCTION__));
				status = scan_info->status;
				goto done;
			}

			/* clear scan ready flag */
			wlc_scan_info->state &= ~SCAN_STATE_READY;

			/* Allow normal traffic before next channel scan */
			WL_SCAN_ENT(scan_info, ("wl%d: %s: stay home for %u ms\n",
			                        scan_info->unit, __FUNCTION__,
			                        scan_info->home_time));
			WLC_SCAN_ADD_TIMER(scan_info, scan_info->home_time, 0);
			goto exit;
		}

		/* if there are more channels to scan ... */
		if (next_chanspec != INVCHANSPEC) {
			/* ... continue scanning */
			scan_info->channel_idx++;
			chanspec = next_chanspec;

			/* Update the last iscan chanspec */
#ifdef STA
			SCAN_ISCAN_CHANSPEC_LAST(scan_info) = chanspec;
#endif // endif
			scan_info->pass = WLC_SCAN_CHANNEL_PREP;
#ifdef WLAWDL
			if (AWDL_SUPPORT(wlc->pub) && wlc_awdl_scan_is_near_aw(wlc)) {
				wlc_suspend_scan(wlc_scan_info);
				goto exit;
			}
#endif // endif
		}
		/* ... otherwise the scan is done,
		 * scan.pass > passes and we fall through to the end of this function
		 */
	}

	if (scan_info->pass == WLC_SCAN_CHANNEL_PREP) {
		/* do off-home-channel setup if we are on the home channel
		 * and we are going away.
		 */
		if (!(wlc_scan_info->state & SCAN_STATE_READY) &&
		    (SCAN_BAND_PI_RADIO_CHANSPEC(scan_info) == SCAN_HOME_CHANNEL(scan_info)) &&
		    (wf_chspec_ctlchspec(SCAN_BAND_PI_RADIO_CHANSPEC(scan_info)) != chanspec)) {
#ifdef WLAWDL
			if (AWDL_SUPPORT(wlc->pub) && wlc_awdl_scan_is_near_aw(wlc)) {
				wlc_suspend_scan(wlc_scan_info);
				goto exit;
			}
#endif // endif
			/* Must leave IBSS/AP first before going away from home channel
			 * This is needed as driver could have come back to home channel and
			 * renabled IBSS/AP
			 */
			wlc_scan_prepare_off_channel(scan_info);
#ifdef STA
			/* N.B.: return value indicates if any task is pending...
			 * TRUE means YES so we stop.
			 */
			if (wlc_scan_prepare_pm_mode(scan_info)) {
				WL_SCAN_ENT(scan_info, ("wl%d: %s: waiting for PM indication to "
				                        "complete\n",
				                        scan_info->unit, __FUNCTION__));
				goto exit;
			}
#endif /* STA */
			/* N.B.: return value indicates if any task is pending...
			 * TRUE means YES so we stop.
			 */
			if (wlc_scan_tx_suspend(scan_info)) {
				WL_SCAN_ENT(scan_info, ("wl%d: %s: waiting for tx suspend to "
				                        "complete\n",
				                        scan_info->unit, __FUNCTION__));
				goto exit;
			}
		}
		scan_info->pass = 1;
	}

	/* scan the channel */
	if (scan_info->pass >= 1 && scan_info->pass <= passes) {
#ifdef WLAWDL

		if (AWDL_SUPPORT(wlc->pub) && wlc_awdl_scan_is_near_aw(wlc)) {
			wlc_suspend_scan(wlc_scan_info);
			goto exit;
		}
#endif // endif
		if ((wlc_scan_info->flag & SCAN_FLAG_SWITCH_CHAN) ||
			wf_chspec_ctlchan(chanspec) !=
			wf_chspec_ctlchan(SCAN_BAND_PI_RADIO_CHANSPEC(scan_info))) {
			wlc_scan_suspend_mac_and_wait(scan_info);

			/* Must leave IBSS/AP first before going to away channel
			 * This is needed as driver could have come back to home channel and
			 * renabled IBSS/AP
			 */
			wlc_scan_prepare_off_channel(scan_info);

			/* suspend normal tx queue operation for channel excursion */
			wlc_scan_excursion_start(scan_info);

			wlc_scan_set_chanspec(scan_info, chanspec);
			wlc_scan_enable_mac(scan_info);

			WL_SCAN(("wl%d: %s: switched to channel %d tsf %u\n",
			         scan_info->unit, __FUNCTION__, CHSPEC_CHANNEL(chanspec),
			         SCAN_GET_TSF_TIMERLOW(scan_info)));
		}

		WL_SCAN_ENT(scan_info, ("wl%d: %s: do pass %u\n",
		                        scan_info->unit, __FUNCTION__,
		                        scan_info->pass));
#ifdef WLSCAN_PS
		/* scan started, switch to one tx/rx core */
		if (WLSCAN_PS_ENAB(scan_info->wlc->pub))
			wlc_scan_ps_config_cores(scan_info, TRUE);
#endif // endif
		wlc_scan_do_pass(scan_info, chanspec);
		goto exit;
	}

	/*
	 * wraps up scan process.
	 */

	wlc_scan_info->state |= SCAN_STATE_TERMINATE;

#if defined(BCMDBG) || defined(WLMSG_INFORM)
	if (scan_info->nssid == 1) {
		char ssidbuf[SSID_FMT_BUF_LEN];
		wlc_ssid_t *ssid = scan_info->ssid_list;

		if (WL_INFORM_ON())
			wlc_format_ssid(ssidbuf, ssid->SSID, ssid->SSID_len);
		WL_INFORM(("wl%d: %s: %s scan done, %d total responses for SSID \"%s\"\n",
		           scan_info->unit, __FUNCTION__,
		           (wlc_scan_info->state & SCAN_STATE_PASSIVE) ? "Passive" : "Active",
		           SCAN_RESULT_MEB(scan_info, count), ssidbuf));
	} else {
		WL_INFORM(("wl%d: %s: %s scan done, %d total responses for SSIDs:\n",
		           scan_info->unit, __FUNCTION__,
		           (wlc_scan_info->state & SCAN_STATE_PASSIVE) ? "Passive" : "Active",
		           SCAN_RESULT_MEB(scan_info, count)));
		if (WL_INFORM_ON())
			wlc_scan_print_ssids(scan_info->ssid_list, scan_info->nssid);
	}
#endif /* BCMDBG || WLMSG_INFORM */

#ifdef WLSCAN_PS
	/* scan is done, revert core mask */
	if (WLSCAN_PS_ENAB(scan_info->wlc->pub))
		wlc_scan_ps_config_cores(scan_info, FALSE);
#endif // endif

	/* return to home channel */
	wlc_scan_return_home_channel(scan_info);

	WL_SRSCAN(("  to channel %04x (returned home)",
		SCAN_HOME_CHANNEL(scan_info)));
	WL_ASSOC(("  to channel %04x (returned home)\n",
		SCAN_HOME_CHANNEL(scan_info)));
done:
	wlc_scan_info->state &= ~SCAN_STATE_READY;
	scan_info->channel_idx = -1;
#if defined(STA) && !defined(SCANOL)
	wlc_scan_time_upd(wlc, scan_info);
#endif /* STA && !SCANOL */
	wlc_scan_info->in_progress = FALSE;

#if !defined(WLOFFLD) && !defined(SCANOL)
	/* If we are switching back to radar home_chanspec
	 * because:
	 * 1. STA scans with atleast one local 11H AP
	 * in radar channel,
	 * 2. Scan is not join/roam.
	 * turn radar_detect ON.
	 * NOTE: For Join/Roam radar_detect ON is done
	 * at later point in wlc_roam_complete() or
	 * wlc_set_ssid_complete(), when STA succesfully
	 * associates to upstream AP.
	 */
	if (WL11H_AP_ENAB(wlc) &&
		WLC_APSTA_ON_RADAR_CHANNEL(wlc) &&
#ifdef STA
		!AS_IN_PROGRESS(wlc) &&
#endif // endif
		wlc_radar_chanspec(wlc->cmi, wlc->home_chanspec)) {
		WL_REGULATORY(("wl%d: %s scan completed, back"
			"to home channel dfs ON\n",
			wlc->pub->unit, __FUNCTION__));
		wlc_set_dfs_cacstate(wlc->dfs, ON);
	}
#endif /* !defined(WLOFFLD) && !defined(SCANOL)) */
	wlc_phy_hold_upd(SCAN_GET_PI_PTR(scan_info), PHY_HOLD_FOR_SCAN, FALSE);
#ifdef WLOLPC
	/* notify scan just terminated - if needed, kick off new cal */
	if (OLPC_ENAB(scan_info->wlc)) {
		wlc_olpc_eng_hdl_chan_update(scan_info->wlc->olpc_info);
	}
#endif /* WLOLPC */

#ifdef WL_BCN_COALESCING
	wlc_bcn_clsg_disable(wlc->bc, BCN_CLSG_SCAN_MASK,
		wlc_scan_info->in_progress ? BCN_CLSG_SCAN_MASK : 0);
#endif /* WL_BCN_COALESCING */
#ifdef WLOFFLD
	if (WLOFFLD_CAP(wlc)) {
		wlc_ol_rx_deferral(wlc->ol, OL_SCAN_MASK, 0);
	}
#endif // endif
	wlc_scan_ssidlist_free(scan_info);

	/* allow core to sleep again (no more solicited probe responses) */
	wlc_scan_set_wake_ctrl(scan_info);

#ifdef WLCQ
	/* resume any channel quality measurement */
	if (wlc->channel_qa_active)
		wlc_lq_channel_qa_sample_req(wlc);
#endif /* WLCQ */

#ifdef STA
	/* disable radio for non-association scan.
	 * Association scan will continue with JOIN process and
	 * end up at MACEVENT: WLC_E_SET_SSID
	 */
	WL_MPC(("wl%d: scan done, SCAN_IN_PROGRESS==FALSE, update mpc\n", scan_info->unit));
	wlc_scan_radio_mpc_upd(scan_info);
	wlc_scan_radio_upd(scan_info);	/* Bring down the radio immediately */
#ifdef WL11D
	/* If we are in 802.11D mode and we are still waiting to find a
	 * valid Country IE, take this opportunity to parse scan results,
	 * if necessary, and try to apply it.
	 */
	if (IS_AUTOCOUNTRY_ENAB(scan_info))
		wlc_scan_11d_scan_complete(scan_info, WLC_E_STATUS_SUCCESS);
#endif /* WL11D */
#endif /* STA */

	if (IS_N_ENAB(scan_info) && IS_COEX_ENAB(scan_info))
		wlc_ht_obss_scan_update(scan_info, WLC_SCAN_SUCCESS);

	/* Don't fill the cache with results from a P2P discovery scan since these entries
	 * are short-lived. Also, a P2P association cannot use Scan cache
	 */
	if (SCANCACHE_ENAB(wlc_scan_info) &&
#ifdef WLP2P
	    !BSS_P2P_DISC_ENAB(wlc, scan_cfg) &&
#endif // endif
	    TRUE) {
		wlc_scan_cache_result(scan_info);
	}

	wlc_scan_callback(scan_info, status);

#ifdef STA
	/* if this was a broadcast scan across all channels, update the roam cache, if possible */
	if (ETHER_ISBCAST(&wlc_scan_info->bssid) &&
	    wlc_scan_info->bss_type == DOT11_BSSTYPE_ANY) {
		SCAN_FOREACH_AS_STA(scan_info, idx, cfg) {
			wlc_roam_t *roam = cfg->roam;
			if (roam &&
			    roam->roam_scan_piggyback && roam->active && !roam->fullscan_count) {
				WL_ASSOC(("wl%d: %s: Building roam cache with scan results from "
				          "broadcast scan\n", scan_info->unit, __FUNCTION__));
				/* this counts as a full scan */
				roam->fullscan_count = 1;
				/* update the roam cache */
				wlc_build_roam_cache(cfg, SCAN_RESULT_PTR(scan_info));
			}
		}
	}
#endif /* STA */

	wlc_scan_bss_list_free(scan_info);

#if defined(MACOSX)
	/* Post a BSS event if an interface is attached to it */
		wlc_scan_bss_mac_event(scan_info, scan_cfg, WLC_E_SCAN_COMPLETE, NULL,
		WLC_E_STATUS_SUCCESS, 0, 0, NULL, 0);
#endif /* MACOSX */

#ifdef STA
	/* If in PM2 and associated and not in PS mode, start the return to sleep
	 * timer to make sure we eventually go back to power save mode.
	 */
	SCAN_FOREACH_AS_STA(scan_info, idx, cfg) {
		if (cfg->BSS && cfg->pm->PM == PM_FAST && !cfg->pm->PMenabled) {
			WL_RTDC(scan_info->wlc, "wlc_scantimer end: start FRTS timer", 0, 0);
			wlc_scan_pm2_sleep_ret_timer_start(cfg);
		}
	}
#endif /* STA */

	wlc_scan_info->state &= ~SCAN_STATE_TERMINATE;

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
	                        SCAN_CLK(scan_info) ? SCAN_GET_TSF_TIMERLOW(scan_info) : 0));
}

static void
wlc_scan_return_home_channel(scan_info_t *scan_info)
{
#ifdef STA
	int idx;
	wlc_bsscfg_t *cfg;
#endif // endif
#if defined(BCMDBG) || defined(WLMSG_SCAN) || defined(SCANOL)
	char chanbuf[CHANSPEC_STR_LEN];
	chanbuf[0] = '\0';
#endif // endif

	wlc_scan_suspend_mac_and_wait(scan_info);

	/* resume normal tx queue operation */
	wlc_scan_excursion_end(scan_info);
	WL_SCAN_ENT(scan_info, ("wl%d: %s: excursion ended\n",
	                        scan_info->unit, __FUNCTION__));
#ifdef WLAWDL
	if (!(wlc_awdl_in_aw(scan_info->wlc) || wlc_awdl_in_preaw(scan_info->wlc)))
#endif // endif
		wlc_scan_set_chanspec(scan_info, SCAN_HOME_CHANNEL(scan_info));
	WL_SCAN(("wl%d: %s: switched to home_chanspec %s tsf %u\n",
		scan_info->unit,
		__FUNCTION__,
		wf_chspec_ntoa(SCAN_HOME_CHANNEL(scan_info), chanbuf),
		SCAN_GET_TSF_TIMERLOW(scan_info)));

#ifdef STA
	/* Return to IBSS and re-enable AP */
	SCAN_FOREACH_AS_STA(scan_info, idx, cfg) {
		if (!cfg->BSS)
			wlc_scan_ibss_enable(cfg);
	}
	if (IS_AP_ACTIVE(scan_info)) {
		/* validate the phytxctl for the beacon before turning it on */
		wlc_scan_validate_bcn_phytxctl(scan_info, NULL);
	}
#endif /* STA */
	wlc_scan_ap_mute(scan_info, FALSE, NULL, WLC_AP_MUTE_SCAN);
#ifdef WLAIBSS
	if (AIBSS_ENAB(scan_info->wlc->pub))
		wlc_aibss_back2homechan_notify(scan_info->wlc);
#endif // endif
#ifdef STA
	/* enable CFP and TSF update */
	wlc_scan_mhf(scan_info, MHF2, MHF2_SKIP_CFP_UPDATE, 0, WLC_BAND_ALL);
	wlc_scan_skip_adjtsf(scan_info, FALSE, NULL, WLC_SKIP_ADJTSF_SCAN, WLC_BAND_ALL);
#endif /* STA */
	/* Restore promisc behavior for beacons and probes */
	SCAN_BCN_PROMISC(scan_info) = FALSE;
	wlc_scan_mac_bcn_promisc(scan_info);

	wlc_scan_enable_mac(scan_info);
	if (IS_SCAN_BLOCK_DATAFIFO(scan_info, DATA_BLOCK_SCAN)) {
		/* Clear the tssi values */
		wlc_phy_clear_tssi(SCAN_GET_PI_PTR(scan_info));

			/* un-suspend the DATA fifo now that we are back on the home channel */
		WL_SCAN_ENT(scan_info, ("wl%d: %s: tx resume...\n", scan_info->unit, __FUNCTION__));
		wlc_scan_tx_resume(scan_info);

			/* re-enable txq processing now that we are back on the home channel */
		SCAN_BLOCK_DATAFIFO_CLR(scan_info, DATA_BLOCK_SCAN);
	}
#ifdef STA
	/* un-block PSPoll operations and restore PS state */
	SCAN_FOREACH_STA(scan_info, idx, cfg) {
		mboolclr(cfg->pm->PMblocked, WLC_PM_BLOCK_SCAN);

		/* check forcePMenabled only for excursion off-channel operation for now.
		 * Other scan usage cases can be added here if the total off-channel duration
		 * is relatively short.
		 */
		/* XXX: Restrict to NAN_ENAB() and LPRIO_EXCRX_IN_PROGRESS() case for now
		 *       to ensure no-risk to the existing codes.
		 *      To be removed after multiple cycles of SVT testing
		 */
		if (NAN_ENAB(scan_info->wlc->pub) &&
			LPRIO_EXCRX_IN_PROGRESS(scan_info->scan_pub)) {
			/* 1) if it was in PMenabled state before off-channel operation,
			 *    we just want to return back to the previous PMenabled state
			 *    rather than waking up STA. This is to reduce power consumption
			 * 2) if forcePMenabled was set by other than scan, just return.
			 *    this is for handling nested use cases.
			 */
			if (cfg->pm->forcePMenabled == 0 ||
				cfg->pm->forcePMenabled != WLC_PM_FORCE_PMENABLED_SCAN) {
				continue;
			}
		}

		/* clear forcePMenabled if it was set by scan */
		if (cfg->pm->forcePMenabled == WLC_PM_FORCE_PMENABLED_SCAN)
			cfg->pm->forcePMenabled = 0;

		/* come out of PS mode if appropriate */
		if (cfg->associated) {
			if ((cfg->pm->PM != PM_MAX || cfg->pm->WME_PM_blocked) &&
				cfg->pm->PMenabled) {
#ifdef WLMCHAN
				/* For mchan operation, only disable PS for STA on our home */
				/* channel */
				if (IS_MCHAN_ENAB(scan_info) && cfg->chan_context &&
				    (cfg->chan_context->chanspec != SCAN_HOME_CHANNEL(scan_info))) {
					WL_MCHAN(("wl%d.%d: %s: skip pmclr for cfg on other "
						"channel\n", scan_info->unit,
						WLC_BSSCFG_IDX(cfg), __FUNCTION__));
					continue;
				}
#endif // endif
#ifdef WLAWDL
				if (AWDL_ENAB(scan_info->wlc->pub) &&
					((wlc_awdl_in_aw(scan_info->wlc) ||
					wlc_awdl_in_preaw(scan_info->wlc)) ||
					mboolisset(cfg->pm->PMblocked, WLC_PM_BLOCK_AWDL)))
					continue;
#endif // endif
#ifndef SCANOL
				if (wlc_roam_scan_islazy(scan_info->wlc, cfg, TRUE)) {
					/* For background roam scan, the home_time is set to
					 * 2*MAX(pm2_sleep_ret_time, 1.5*beacon_period).
					 * Now, set sleep return time to be half of it.
					 *
					 * Background scan schedyule allows mor ebeacon reception
					 * oppotunities, so no need to exit PS.
					 */
					cfg->pm->pm2_sleep_ret_time_left = scan_info->home_time/2;
					wlc_scan_pm2_sleep_ret_timer_start(cfg);
					continue;
				}
#endif /* !SCANOL */
				WL_RTDC(scan_info->wlc, "wlc_scan_return_home_channel: exit PS",
					0, 0);
				wlc_scan_set_pmstate(cfg, FALSE);
			}
		}
#ifdef WLTDLS
		else if (BSS_TDLS_ENAB(scan_info->wlc, cfg) && cfg->pm->PMenabled)
			wlc_tdls_notify_pm_state(scan_info->wlc->tdls, cfg, FALSE);
#endif // endif
	}
	wlc_scan_set_wake_ctrl(scan_info);
#endif /* STA */

	/* run txq if not empty */
	wlc_scan_send_q(scan_info);
}

#ifndef SCANOL
/* change the active queue and resume tx without terminating the scan
 * i.e. without transitioning to PM 0 and switching back to "home channel".
 */
void
wlc_scan_tx_resume_txqi(wlc_scan_info_t *scan, wlc_txq_info_t *txqi)

{
	scan_info_t *priv = (scan_info_t *)scan->scan_priv;
	wlc_info_t *wlc = priv->wlc;

	/* connect the given txqi as the active txq */
	wlc_suspend_mac_and_wait(wlc);
	wlc_active_queue_set(wlc, txqi);
	wlc_enable_mac(wlc);

	/* resume tx h/w */
	wlc_phy_clear_tssi(wlc->band->pi);
	WL_SCAN_ENT(priv, ("wl%d: %s: tx resume...\n", wlc->pub->unit, __FUNCTION__));
	wlc_tx_resume(wlc);
	wlc->block_datafifo &= ~DATA_BLOCK_SCAN;

	/* run txq if not empty */
	if (!pktq_empty(&wlc->active_queue->q))
		wlc_send_q(wlc, wlc->active_queue);
}

#endif /* !SCANOL */

static void
wlc_scan_act(scan_info_t *si, uint dwell, bool active)
{
	wlc_info_t *wlc = si->wlc;
#ifdef BCMDBG
	uint saved = dwell;
#endif // endif

	/* real scan request */
	if (si->act_cb == NULL) {
		if (active)
			wlc_scan_sendprobe(si);
		goto set_timer;
	}

	/* other requests using the scan engine */
	(si->act_cb)(wlc, si->act_cb_arg, &dwell);

#ifdef BCMDBG
	if (dwell != saved) {
		WL_SCAN(("wl%d: %s: adjusting dwell time from %u to %u ms\n",
		         si->unit, __FUNCTION__, saved, dwell));
	}
#endif // endif

set_timer:
	WLC_SCAN_ADD_TIMER(si, dwell, 0);
}

void
wlc_scan_radar_clear(wlc_scan_info_t *wlc_scan_info)
{
	scan_info_t	*scan_info = (scan_info_t *) wlc_scan_info->scan_priv;
	uint32		channel_dwelltime;

	uint32 cur_l, cur_h;
	uint32 elapsed_time, remaining_time, active_time;

	/* if we are not on a radar quiet channel,
	 * or a passive scan was requested,
	 * or we already processed the radar clear signal,
	 * or it is not a prohibited channel,
	 * then do nothing
	 */
	if ((wlc_scan_valid_chanspec_db(scan_info, SCAN_BAND_PI_RADIO_CHANSPEC(scan_info)) &&
	     !wlc_scan_quiet_chanspec(scan_info, SCAN_BAND_PI_RADIO_CHANSPEC(scan_info))) ||
	    (wlc_scan_info->state & (SCAN_STATE_PASSIVE | SCAN_STATE_RADAR_CLEAR)))
		return;

	/* if we are not in the channel scan portion of the scan, do nothing */
	if (scan_info->pass != 1)
		return;

	/* if there is not enough time remaining for a probe,
	 * do nothing unless explicitly enabled
	 */
	if (scan_info->force_active == FALSE) {
		SCAN_READ_TSF(scan_info, &cur_l, &cur_h);

		elapsed_time = (cur_l - scan_info->start_tsf) / 1000;

		channel_dwelltime = CHANNEL_PASSIVE_DWELLTIME(scan_info);

		if (elapsed_time > channel_dwelltime)
			remaining_time = 0;
		else
			remaining_time = channel_dwelltime - elapsed_time;

		if (remaining_time < WLC_SCAN_MIN_PROBE_TIME)
			return;

		active_time = MIN(remaining_time, CHANNEL_ACTIVE_DWELLTIME(scan_info));
	}
	else
		active_time = CHANNEL_ACTIVE_DWELLTIME(scan_info);

	/* everything is ok to switch to an active scan */
	wlc_scan_info->state |= SCAN_STATE_RADAR_CLEAR;

	WLC_SCAN_DEL_TIMER(scan_info);

	SCAN_BLOCK_DATAFIFO_CLR(scan_info, DATA_BLOCK_SCAN);
	SCAN_TO_MUTE(scan_info, OFF, 0);

	wlc_scan_act(scan_info, active_time, TRUE);

	WL_REGULATORY(("wl%d: wlc_scan_radar_clear: rcvd beacon on radar channel %d,"
		" converting to active scan, %d ms left\n",
		scan_info->unit, CHSPEC_CHANNEL(SCAN_BAND_PI_RADIO_CHANSPEC(scan_info)),
		active_time));
}

#ifdef BCMDBG
static void
print_valid_channel_error(scan_info_t *scan_info, chanspec_t chspec)
{
#ifndef SCANOL
	uint8 channel = CHSPEC_CHANNEL(chspec);
	uint bandunit = CHSPEC_WLCBANDUNIT(chspec);
	wlc_info_t *wlc = scan_info->wlc;

	WL_ERROR(("CHSPEC_CHANNEL(chspec)=%x\n", CHSPEC_CHANNEL(chspec)));

	if (CHANNEL_BANDUNIT(wlc, CHSPEC_CHANNEL(chspec)) != bandunit) {
			WL_ERROR(("CHANNEL_BANDUNIT(wlc, CHSPEC_CHANNEL(chspec))=%x\n",
			CHANNEL_BANDUNIT(wlc, CHSPEC_CHANNEL(chspec))));
		return;
	}

	/* Check a 20Mhz channel -- always assumed to be dual-band */
	if (CHSPEC_IS20(chspec)) {
		if (!VALID_CHANNEL20_DB(wlc, chspec)) {
			WL_ERROR(("VALID_CHANNEL20_DB = %d\n",
			VALID_CHANNEL20_DB(wlc, chspec)));
		} else {
			WL_ERROR(("%s: no error found\n", __FUNCTION__));
		}
		return;
	} else if (CHSPEC_IS40(chspec)) {
		/* Check a 40Mhz channel */
		if (!wlc->pub->phy_bw40_capable) {
			WL_ERROR(("phy not bw40 capable\n"));
			return;
		}

		if (!VALID_40CHANSPEC_IN_BAND(wlc, CHSPEC_WLCBANDUNIT(chspec))) {
			WL_ERROR(("!VALID_40CHANSPEC_IN_BAND(%p, %d)\n", wlc, chspec));
			return;
		}
		if (!VALID_CHANNEL20_DB(wlc, LOWER_20_SB(channel)) ||
			!VALID_CHANNEL20_DB(wlc, UPPER_20_SB(channel))) {
			WL_ERROR(("dual bands not both valid = [%x, %x]\n",
				LOWER_20_SB(channel), UPPER_20_SB(channel)));
			return;
		}

		/* check that the lower sideband allows an upper sideband */
			WL_ERROR(("%s: lower sideband not allow upper one OR error not found\n",
				__FUNCTION__));

	} else if (CHSPEC_IS80(chspec)) {
		/* Check a 80MHz channel - only 5G band supports 80MHz */

		chanspec_t chspec40;

		/* Only 5G supports 80MHz
		 * Check the chanspec band with BAND_5G() instead of the more straightforward
		 * CHSPEC_IS5G() since BAND_5G() is conditionally compiled on BAND5G support. This
		 * check will turn into a constant check when compiling without BAND5G support.
		 */
		if (!BAND_5G(CHSPEC2WLC_BAND(chspec))) {
			WL_ERROR(("band not 5g for 80MHz\n"));
			return;
		}

		/* Make sure that the phy is 80MHz capable and that
		 * we are configured for 80MHz on the band
		 */
		if (!wlc->pub->phy_bw80_capable ||
		    !WL_BW_CAP_80MHZ(SCAN_GET_BANDSTATE(scan_info, BAND_5G_INDEX)->bw_cap)) {
			WL_ERROR(("!phy_bw80_capable (%x) || !mimo_cap_80 (%x)\n",
				!wlc->pub->phy_bw80_capable,
				!WL_BW_CAP_80MHZ(SCAN_GET_BANDSTATE(scan_info,
				BAND_5G_INDEX)->bw_cap)));
			return;
		}
		/* XXX 4360: do we need a regulatory WLC_NO_80MHZ flag like
		 * the one checked in VALID_40CHANSPEC_IN_BAND()?
		 */
		/* Check that the 80MHz center channel is a defined channel */
		/* Make sure both 40 MHz side channels are valid
		 * Create a chanspec for each 40MHz side side band and check
		 */
		chspec40 = (chanspec_t)((channel - CH_20MHZ_APART) |
			WL_CHANSPEC_CTL_SB_L |
			WL_CHANSPEC_BW_40 |
			WL_CHANSPEC_BAND_5G);

		if (!wlc_scan_valid_chanspec_db(scan_info, chspec40)) {
			WL_ERROR(("wl%d: %s: 80MHz: chanspec %0X -> chspec40 %0X "
					"failed valid check\n",
					scan_info->unit, __FUNCTION__, chspec, chspec40));

			return;
		}
		chspec40 = (chanspec_t)((channel + CH_20MHZ_APART) |
			WL_CHANSPEC_CTL_SB_L |
			WL_CHANSPEC_BW_40 |
			WL_CHANSPEC_BAND_5G);

		if (!wlc_scan_valid_chanspec_db(scan_info, chspec40)) {
			WL_ERROR(("wl%d: %s: 80MHz: chanspec %0X -> chspec40 %0X "
					"failed valid check\n",
					scan_info->unit, __FUNCTION__, chspec, chspec40));
			return;
		}
		WL_ERROR(("%s: err not found or 80MHz has no channel %d\n", __FUNCTION__, channel));
		return;
	}
#endif /* !SCANOL */
}
#endif /* BCMDBG */

void
wlc_scan_default_channels(wlc_scan_info_t *wlc_scan_info, chanspec_t *chanspec_list,
	int *channel_count)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
	chanspec_t chanspec_start;
	int num;

	/* start the scan on the current home channel */
	chanspec_start = wf_chspec_ctlchspec(SCAN_HOME_CHANNEL(scan_info));
#ifdef BCMDBG
	if (!wlc_scan_valid_chanspec_db(scan_info, chanspec_start)) {
		WL_ERROR(("wlc_scan_valid_chanspec_db(%p, %x)==FALSE\n",
			SCAN_CMIPTR(scan_info), chanspec_start));
		print_valid_channel_error(scan_info, chanspec_start);
	}
#endif /* BCMDBG */
	ASSERT(wlc_scan_valid_chanspec_db(scan_info, chanspec_start));

	/* enumerate all the active (non-quiet) channels first */
	wlc_scan_channels(scan_info, chanspec_list, &num, MAXCHANNEL,
		chanspec_start, CHAN_TYPE_CHATTY);
	*channel_count = num;

	/* if scan_info->passive_time = 0, skip the passive channels */
	if (!scan_info->passive_time)
		return;

	/* enumerate all the passive (quiet) channels second */
	wlc_scan_channels(scan_info, &chanspec_list[num], &num,
		(MAXCHANNEL - *channel_count), chanspec_start, CHAN_TYPE_QUIET);
	*channel_count += num;
}

/*
 * Scan channels are always 20MHZ, so return the valid set of 20MHZ channels for this locale.
 */
static void
wlc_scan_channels(scan_info_t *scan_info, chanspec_t *chanspec_list,
	int *pchannel_num, int channel_max, chanspec_t chanspec_start, int channel_type)
{
	uint bandunit;
	uint channel;
	chanspec_t chanspec;
	int num = 0;
	uint i;

	/* chanspec start should be for a 20MHZ channel */
	ASSERT(CHSPEC_IS20(chanspec_start));
	bandunit = CHSPEC_WLCBANDUNIT(chanspec_start);
	for (i = 0; i < SCAN_NBANDS(scan_info); i++) {
		channel = CHSPEC_CHANNEL(chanspec_start);
		chanspec = CH20MHZ_CHSPEC(channel);
		while (num < channel_max) {
			if (SCAN_VALID_CHANNEL20_IN_BAND(scan_info, bandunit, channel) &&
			    ((channel_type == CHAN_TYPE_CHATTY &&
				!wlc_scan_quiet_chanspec(scan_info, chanspec)) ||
			     (channel_type == CHAN_TYPE_QUIET &&
				wlc_scan_quiet_chanspec(scan_info, chanspec))))
					chanspec_list[num++] = chanspec;
			channel = (channel + 1) % MAXCHANNEL;
			chanspec = CH20MHZ_CHSPEC(channel);
			if (chanspec == chanspec_start)
				break;
		}

		/* only find channels for one band */
		if (!SCAN_IS_MBAND_UNLOCKED(scan_info))
			break;

		/* prepare to find the other band's channels */
		bandunit = ((bandunit == 1) ? 0 : 1);
		chanspec_start = CH20MHZ_CHSPEC(0);
	}

	*pchannel_num = num;
}

static uint
wlc_scan_prohibited_channels(scan_info_t *scan_info, chanspec_t *chanspec_list,
	int channel_max)
{
	WLC_BAND_T *band;
	uint channel, maxchannel, i, j;
	chanvec_t sup_chanvec, chanvec;
	int num = 0;

	if (!IS_AUTOCOUNTRY_ENAB(scan_info))
		return 0;

	band = SCAN_GET_CUR_BAND(scan_info);
	for (i = 0; i < SCAN_NBANDS(scan_info); i++) {
		const char *acdef = wlc_scan_11d_get_autocountry_default(scan_info);

		bzero(&sup_chanvec, sizeof(chanvec_t));
		/* Get the list of all the channels in autocountry_default
		 * and supported by phy
		 */
		wlc_phy_chanspec_band_validch(band->pi, band->bandtype, &sup_chanvec);
		if (!wlc_scan_get_chanvec(scan_info, acdef, band->bandtype, &chanvec))
			return 0;

		for (j = 0; j < sizeof(chanvec_t); j++)
			sup_chanvec.vec[j] &= chanvec.vec[j];

		maxchannel = BAND_2G(band->bandtype) ? (CH_MAX_2G_CHANNEL + 1) : MAXCHANNEL;
		for (channel = 0; channel < maxchannel; channel++) {
			if (isset(sup_chanvec.vec, channel) &&
			    !SCAN_VALID_CHANNEL20_IN_BAND(scan_info, band->bandunit, channel)) {
				chanspec_list[num++] = CH20MHZ_CHSPEC(channel);
				if (num >= channel_max)
					return num;
			}
		}
		band = SCAN_GET_BANDSTATE(scan_info, SCAN_OTHERBANDUNIT(scan_info));
	}

	return num;
}

#ifndef SCANOL
bool
wlc_scan_inprog(wlc_info_t *wlc_info)
{
	return SCAN_IN_PROGRESS(wlc_info->scan);
}
#endif /* !SCANOL */

void
wlc_scan_fifo_suspend_complete(wlc_scan_info_t *wlc_scan_info)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;

	if (!SCAN_IN_PROGRESS(wlc_scan_info))
		return;

	WL_SCAN_ENT(scan_info, ("wl%d: %s: state 0x%x tsf %u\n",
		scan_info->unit, __FUNCTION__, wlc_scan_info->state,
		SCAN_GET_TSF_TIMERLOW(scan_info)));

	if (!(wlc_scan_info->state & SCAN_STATE_WSUSPEND))
		return;

	wlc_scan_info->state &= ~SCAN_STATE_WSUSPEND;
	WLC_SCAN_DEL_TIMER(scan_info);

	/* no pending tasks (neither guard timers) so start a timer
	 * to move the state machine forward...
	 */
	wlc_scan_info->state |= SCAN_STATE_READY;
	WLC_SCAN_ADD_TIMER(scan_info, 0, 0);
}

#ifndef SCANOL
void
wlc_scan_pm_pending_complete(wlc_scan_info_t *wlc_scan_info)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;

	if (!SCAN_IN_PROGRESS(wlc_scan_info))
		return;

	WL_SCAN_ENT(scan_info, ("wl%d: %s: state 0x%x tsf %u\n",
	                        SCAN_UNIT(scan_info), __FUNCTION__,
	                        wlc_scan_info->state,
	                        SCAN_GET_TSF_TIMERLOW(scan_info)));

	if (wlc_scan_info->state & SCAN_STATE_IN_TMR_CB)
		return;

	if (!(wlc_scan_info->state & SCAN_STATE_PSPEND))
		return;

	if (IS_PM_PENDING(scan_info) ||
#if defined(EXTENDED_SCAN) && defined(BCMDBG)
	    scan_info->test_nopsack ||
#endif // endif
	    FALSE) {
		WL_INFORM(("wl%d: No ACK for PS null frame\n", scan_info->unit));
		/* abort scan if background scan and PS didn't make it to AP */
		if ((SCAN_TYPE_BACKGROUND(scan_info))) {
			WL_ERROR(("Aborting the scan\n"));
			wlc_scan_abort(wlc_scan_info, WLC_E_STATUS_ABORT);
			return;
		}
	}
	wlc_scan_info->state &= ~SCAN_STATE_PSPEND;
	WLC_SCAN_DEL_TIMER(scan_info);

	/* delay moving to off channel by pm2_radio_shutoff_dly if configured */
	/* N.B.: we have pending task (delay timer) to move the state machine forward... */
	if (wlc_scan_tx_suspend_dly(wlc_scan_info)) {
		WL_SCAN_ENT(scan_info, ("wl%d: %s: delaying tx suspend\n",
		                        scan_info->unit, __FUNCTION__));
		return;
	}

	/* suspend tx */
	/* N.B.: we have pending task (fifo suspend complete) and
	 * also a guard timer to move the state machine forward...
	 */
	if (wlc_scan_tx_suspend(scan_info)) {
		WL_SCAN_ENT(scan_info, ("wl%d: %s: waiting for tx suspend to "
		                        "complete\n",
		                        scan_info->unit, __FUNCTION__));
		return;
	}

	/* no pending tasks (neither guard timers) so start a timer
	 * to move the state machine forward...
	 */
	WLC_SCAN_ADD_TIMER(scan_info, 0, 0);
}

#endif /* !SCANOL */

static void
wlc_scan_callback(scan_info_t *scan_info, uint status)
{
	scancb_fn_t cb = scan_info->cb;
	void *cb_arg = scan_info->cb_arg;
	wlc_bsscfg_t *cfg = scan_info->bsscfg;
#ifdef WLAWDL
	wlc_info_t *wlc = scan_info->wlc;
#endif // endif

	scan_info->bsscfg = NULL;

	scan_info->cb = NULL;
	scan_info->cb_arg = NULL;
#ifdef WLAWDL
	if (AWDL_SUPPORT(wlc->pub))
		wlc_send_awdl_scan_evt(wlc, WLC_E_AWDL_SCAN_DONE);
#endif // endif

	if (cb != NULL)
		(cb)(cb_arg, status, cfg);

	SCAN_RESTORE_BSSCFG(scan_info, cfg);

	/* reset scan engine usage */
	scan_info->scan_pub->usage = SCAN_ENGINE_USAGE_NORM;
#ifdef ANQPO
	if (SCAN_ANQPO_ENAB(scan_info) && scan_info->scan_pub->is_hotspot_scan) {
		wl_scan_anqpo_scan_stop(scan_info);
	}
#endif // endif
}

chanspec_t
wlc_scan_get_current_chanspec(wlc_scan_info_t *wlc_scan_info)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;

	return scan_info->chanspec_list[scan_info->channel_idx];
}

int
wlc_scan_ioctl(wlc_scan_info_t *wlc_scan_info,
	int cmd, void *arg, int len, struct wlc_if *wlcif)
{
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
		ASSERT(pval != NULL);
		if (pval != NULL)
			*pval = wlc_scan_info->state & SCAN_STATE_SUPPRESS ? 1 : 0;
		else
			bcmerror = BCME_BADARG;
		break;

	case WLC_SET_SCANSUPPRESS:
		if (val)
			wlc_scan_info->state |= SCAN_STATE_SUPPRESS;
		else
			wlc_scan_info->state &= ~SCAN_STATE_SUPPRESS;
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

	case WLC_GET_SCAN_HOME_TIME:
		ASSERT(arg != NULL);
		bcmerror = wlc_iovar_op(wlc, "scan_home_time", NULL, 0,
			arg, len, IOV_GET, wlcif);
		break;

	case WLC_SET_SCAN_HOME_TIME:
		ASSERT(arg != NULL);
		bcmerror = wlc_iovar_op(wlc, "scan_home_time", NULL, 0,
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
}

#ifdef BCMDBG
/* test case support - requires wl UP (wl mpc 0; wl up) */
static void
wlc_scan_test_done(void *arg, int status, wlc_bsscfg_t *cfg)
{
	scan_info_t *scan_info = (scan_info_t *)arg;
	wlc_info_t *wlc = scan_info->wlc;

	scan_info->test = SCAN_TEST_NONE;

	if (scan_info->test_timer != NULL) {
		wl_del_timer(wlc->wl, scan_info->test_timer);
		wl_free_timer(wlc->wl, scan_info->test_timer);
		scan_info->test_timer = NULL;
	}
}

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
		               -1, -1, -1, -1, -1,
		               NULL, 0, 0, FALSE,
		               wlc_scan_test_done, scan_info, 0, FALSE, FALSE,
		               SCANCACHE_ENAB(wlc_scan_info),
		               0, NULL, SCAN_ENGINE_USAGE_NORM, NULL, NULL);
		if (err != BCME_OK) {
			WL_ERROR(("%s: wlc_scan failed, err %d\n", __FUNCTION__, err));
		}
		break;
	}
}

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
	                -1, -1, -1, -1, -1,
	                NULL, 0, 0, FALSE,
	                wlc_scan_test_done, scan_info, 0, FALSE, FALSE,
	                SCANCACHE_ENAB(wlc_scan_info),
	                0, NULL, SCAN_ENGINE_USAGE_NORM, NULL, NULL);
}
#endif	/* BCMDBG */

#ifdef WLC_SCAN_IOVARS
static int
wlc_scan_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif)
{
	scan_info_t *scan_info = (scan_info_t *)hdl;
	int err = 0;
	int32 int_val = 0;
	bool bool_val = FALSE;
	int32 *ret_int_ptr;
	wlc_info_t *wlc = scan_info->wlc;

	BCM_REFERENCE(wlc);

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
	case IOV_GVAL(IOV_SCAN_HOME_TIME):
		*ret_int_ptr = (int32)scan_info->defaults.home_time;
		break;

	case IOV_SVAL(IOV_SCAN_HOME_TIME):
		scan_info->defaults.home_time = (uint16)int_val;
		break;

	case IOV_GVAL(IOV_SCAN_NPROBES):
		*ret_int_ptr = (int32)scan_info->defaults.nprobes;
		break;

	case IOV_SVAL(IOV_SCAN_NPROBES):
		scan_info->defaults.nprobes = (int8)int_val;
		break;

	case IOV_GVAL(IOV_SCAN_FORCE_ACTIVE):
		*ret_int_ptr = (int32)scan_info->force_active;
		break;

	case IOV_SVAL(IOV_SCAN_FORCE_ACTIVE):
		scan_info->force_active = (int_val != 0);
		break;

#ifdef EXTENDED_SCAN
	case IOV_SVAL(IOV_SCAN_EXTENDED):
		err = wlc_extdscan_request(scan_info->scan_pub, arg, len, NULL, NULL);
		break;
#ifdef BCMDBG
	case IOV_SVAL(IOV_SCAN_NOPSACK):
		scan_info->test_nopsack = (int8)int_val;
		break;
	case IOV_GVAL(IOV_SCAN_NOPSACK):
		*ret_int_ptr = (int32)scan_info->test_nopsack;
		break;
#endif /* BCMDBG */

#endif /* EXTENDED_SCAN */

#ifdef WLSCANCACHE
	case IOV_GVAL(IOV_SCANCACHE):
		*ret_int_ptr = scan_info->scan_pub->_scancache;
		break;

	case IOV_SVAL(IOV_SCANCACHE):
		if (SCANCACHE_SUPPORT(wlc->pub)) {
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
		if (SCANCACHE_SUPPORT(wlc->pub))
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

	case IOV_GVAL(IOV_SCAN_HOME_TIME_DEFAULT):
		*ret_int_ptr = WLC_SCAN_HOME_TIME;
		break;

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
#ifdef STA
	case IOV_GVAL(IOV_SCAN_HOME_AWAY_TIME):
		*ret_int_ptr = (int32)scan_info->home_away_time;
		break;
	case IOV_SVAL(IOV_SCAN_HOME_AWAY_TIME):
		if (int_val <= 0)
			err = BCME_BADARG;
		else
			scan_info->home_away_time = (uint16)int_val;
		break;
#endif /* STA */

#ifdef WLSCAN_PS
#if defined(BCMDBG) || defined(WLTEST)
	case IOV_GVAL(IOV_SCAN_RX_PWRSAVE):
		if (WLSCAN_PS_ENAB(scan_info->wlc->pub)) {
			*ret_int_ptr = (uint8)scan_info->scan_rx_pwrsave;
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;

	case IOV_SVAL(IOV_SCAN_RX_PWRSAVE):
		if (WLSCAN_PS_ENAB(scan_info->wlc->pub)) {
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
		if (WLSCAN_PS_ENAB(scan_info->wlc->pub)) {
			*ret_int_ptr = (uint8)scan_info->scan_tx_pwrsave;
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;
	case IOV_SVAL(IOV_SCAN_TX_PWRSAVE):
		if (WLSCAN_PS_ENAB(scan_info->wlc->pub)) {
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
		*ret_int_ptr = (uint8)scan_info->scan_pwrsave_enable;
		break;
	case IOV_SVAL(IOV_SCAN_PWRSAVE):
		if (int_val < 0 || int_val > 1)
			err = BCME_BADARG;
		else
			scan_info->scan_pwrsave_enable = (uint8)int_val;
		break;
#endif /* WLSCAN_PS */

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}
#endif /* WLC_SCAN_IOVARS */

static void
wlc_scan_sendprobe(scan_info_t *scan_info)
{
	int i;
	wlc_ssid_t *ssid;
	int n;
	wlc_bsscfg_t *cfg = scan_info->bsscfg;
	const struct ether_addr *da = &ether_bcast;
	const struct ether_addr *bssid = &ether_bcast;

	ASSERT(scan_info->pass >= 1);

	if (scan_info->extdscan) {
		ssid = &scan_info->ssid_list[scan_info->pass - 1];
		n = scan_info->nprobes;
	}
	else {
		ssid = scan_info->ssid_list;
		n = scan_info->nssid;
	}

#ifdef EXT_STA
	if (IS_EXTSTA_ENAB(scan_info) && scan_info->suppress_ssid) {
		_wlc_scan_sendprobe(scan_info, cfg, (const uchar *)"", 0,
		              da, bssid, SCAN_PROBE_TXRATE(scan_info),
		              NULL, 0);
		return;
	}
#endif /* EXT_STA */

	for (i = 0; i < n; i++) {
#ifdef WLFMC
		wlc_scan_info_t *wlc_scan_info = scan_info->scan_pub;
		/* in case of roaming reassoc, use unicast scans */
		if (WLFMC_ENAB(scan_info->wlc->pub) &&
			!ETHER_ISMULTI(&wlc_scan_info->bssid) &&
			(cfg->assoc->type == AS_ROAM) &&
			(cfg->roam->reason == WLC_E_REASON_INITIAL_ASSOC)) {
			da = &wlc_scan_info->bssid;
			bssid = &wlc_scan_info->bssid;
		}
#endif /* WLFMC */
		/*
		 * XXX: when the Txheader is changed to pass along the txpower
		 * along with each packet, needs to find the chan_idx to set the
		 * proper txpower
		 */
		_wlc_scan_sendprobe(scan_info, cfg, ssid->SSID, ssid->SSID_len,
		              da, bssid, SCAN_PROBE_TXRATE(scan_info), NULL, 0);
		if (!scan_info->extdscan)
			ssid++;
	}
}

static bool
wlc_scan_usage_scan(wlc_scan_info_t *scan)
{
	return (NORM_IN_PROGRESS(scan) || ESCAN_IN_PROGRESS(scan) ||
		LPRIO_EXCRX_IN_PROGRESS(scan));
}

static void
wlc_scan_do_pass(scan_info_t *scan_info, chanspec_t chanspec)
{
	bool 	passive_scan = FALSE;
	uint32	channel_dwelltime = 0;
	uint32  dummy_tsf_h, start_tsf;
	wlc_info_t *wlc = scan_info->wlc;
	bool quiet, actframe;

	SCAN_BCN_PROMISC(scan_info) = wlc_scan_usage_scan(scan_info->scan_pub);
	wlc_scan_mac_bcn_promisc(scan_info);

	SCAN_READ_TSF(scan_info, &start_tsf, &dummy_tsf_h);
	scan_info->start_tsf = start_tsf;

	quiet = wlc_scan_quiet_chanspec(scan_info, chanspec);
	actframe = scan_info->scan_pub->usage == SCAN_ENGINE_USAGE_AF;

	if ((scan_info->scan_pub->state & SCAN_STATE_PASSIVE) ||
	     (quiet && !actframe) ||
	    !wlc_scan_valid_chanspec_db(scan_info, chanspec)) {
		channel_dwelltime = CHANNEL_PASSIVE_DWELLTIME(scan_info);

		WL_SCAN(("wl%d: passive dwell time %d ms, channel %d, tsf %u\n",
			scan_info->unit, channel_dwelltime, CHSPEC_CHANNEL(chanspec),
		        start_tsf));
		WL_ROAM(("PASSIVE SCAN CH%03d %03dms\n", CHSPEC_CHANNEL(chanspec),
			channel_dwelltime));

		passive_scan = TRUE;
	}
	else {
		channel_dwelltime = CHANNEL_ACTIVE_DWELLTIME(scan_info)/scan_info->npasses;

		WL_SCAN(("wl%d: active dwell time %d ms, channel %d, tsf %u\n",
			scan_info->unit, channel_dwelltime, CHSPEC_CHANNEL(chanspec),
		        start_tsf));
		if (quiet) {
			wlc_bmac_mute(wlc->hw, OFF, 0);
		}
#ifdef WLMSG_ROAM
		if (scan_info->pass == 1) {
			WL_ROAM(("ACTIVE SCAN  CH%03d %03dms\n", CHSPEC_CHANNEL(chanspec),
				scan_info->npasses * channel_dwelltime));
		}
#endif // endif
	}

	wlc_scan_act(scan_info, channel_dwelltime, !passive_scan);

	/* record phy noise for the scan channel */
#ifndef SCANOL
	/* XXX: remove noise sampling request for 4360 during scanning
	 * as it is causing some issues in scanning with high BG_NOISE interrupts.
	 */
	if (D11REV_LT(IS_COREREV(scan_info), 40))
		wlc_lq_noise_sample_request(scan_info->wlc, WLC_NOISE_REQUEST_SCAN,
			CHSPEC_CHANNEL(chanspec));
#endif /* !SCANOL */
}

bool
wlc_scan_ssid_match(wlc_scan_info_t *scan_pub, bcm_tlv_t *ssid_ie, bool filter)
{
	scan_info_t 	*scan_info = (scan_info_t *)scan_pub->scan_priv;
	wlc_ssid_t 	*ssid;
	int 		i;
	char		*c;

	if ((scan_pub->usage == SCAN_ENGINE_USAGE_RM) ||
	    ((scan_info->nssid == 1) && ((scan_info->ssid_list[0]).SSID_len == 0))) {
		return TRUE;
	}

	if (scan_info->ssid_wildcard_enabled)
		return TRUE;

	if (!ssid_ie || ssid_ie->len > DOT11_MAX_SSID_LEN) {
		return FALSE;
	}

	/* filter out beacons which have all spaces or nulls as ssid */
	if (filter) {
		if (ssid_ie->len == 0)
			return FALSE;
		c = (char *)&ssid_ie->data[0];
		for (i = 0; i < ssid_ie->len; i++) {
			if ((*c != 0) && (*c != ' '))
				break;
			c++;
		}
		if (i == ssid_ie->len)
			return FALSE;
	}

	/* do not do ssid matching if we are sending out bcast SSIDs
	 * do the filtering before the scan_complete callback
	 */
#ifdef EXT_STA
	if (IS_EXTSTA_ENAB(scan_info)) {
		if (scan_info->suppress_ssid)
			return TRUE;
	}
#endif /* EXT_STA */

	ssid = scan_info->ssid_list;
	for (i = 0; i < scan_info->nssid; i++) {
		if (SCAN_IS_MATCH_SSID(scan_info, ssid->SSID, ssid_ie->data,
		                      ssid->SSID_len, ssid_ie->len))
			return TRUE;
#ifdef WLP2P
		if (IS_P2P_ENAB(scan_info) &&
		    wlc_p2p_ssid_match(scan_info->wlc->p2p, scan_info->bsscfg,
		                       ssid->SSID, ssid->SSID_len,
		                       ssid_ie->data, ssid_ie->len))
			return TRUE;
#endif // endif
		ssid++;
	}
	return FALSE;
}

#ifdef WL11N
static void
wlc_ht_obss_scan_update(scan_info_t *scan_info, int status)
{
#ifndef SCANOL
	wlc_info_t *wlc = scan_info->wlc;
	wlc_bsscfg_t *cfg;
	wlc_obss_info_t *obss;
	obss_params_t *params;
	uint8 chanvec[OBSS_CHANVEC_SIZE]; /* bitvec of channels in 2G */
	uint8 chan, i;
	uint8 num_chan = 0;

	(void)wlc;

	WL_TRACE(("wl%d: wlc_ht_obss_scan_update\n", scan_info->unit));

	cfg = scan_info->bsscfg;

	/* checking  for valid fields */
	if (cfg == NULL || cfg->obss == NULL)
	{
		WL_COEX(("%s: %s is null; aborting obss scan update",
			__FUNCTION__, (cfg == NULL) ? "bsscfg" : "cfg->obss"));
		return;
	}
	/* abort if STA is not OBSS feature enabled */
	if (!IS_COEX_ACTIVE(cfg))
		return;

	obss = cfg->obss;

	params = &obss->params;

	/* need to reset obss scan timer if expired and scan abort */
	if (status != WLC_SCAN_SUCCESS && !obss->scan_countdown) {
		obss->scan_countdown = 5;
		return;
	}
	if (scan_info->active_time < (int)params->active_dwell) {
		WL_COEX(("wl%d: %s: active time %d < %d\n", scan_info->unit, __FUNCTION__,
			scan_info->active_time, params->active_dwell));
		return;
	}
	if (scan_info->passive_time < (int)params->passive_dwell) {
		WL_COEX(("wl%d: %s: passive time %d < %d\n", scan_info->unit, __FUNCTION__,
			scan_info->passive_time, params->passive_dwell));
		return;
	}

	bzero(chanvec, OBSS_CHANVEC_SIZE);
	for (i = 0; i < scan_info->channel_num; i++) {
		chan = CHSPEC_CHANNEL(scan_info->chanspec_list[i]);
		if (chan <= CH_MAX_2G_CHANNEL) {
			setbit(chanvec, chan);
			num_chan++;
		}
	}
	if (num_chan < wlc->obss->num_chan) {
		WL_COEX(("wl%d: %s: scanned channel [%d] < OBSS Scan Params [%d]\n",
			scan_info->unit, __FUNCTION__, num_chan, wlc->obss->num_chan));
		return;
	}
	if (bcmp(chanvec, wlc->obss->chanvec, OBSS_CHANVEC_SIZE)) {
		WL_COEX(("wl%d: %s: scanned channel list don't match to OBSS Scan requirement\n",
			scan_info->unit, __FUNCTION__));
		return;
	}

	/* XXX if OBSS scan period is < default scan, it will cause
	 * extra scan occure when default scan is scheduled
	 */
	if (cfg->associated && IS_COEX_ACTIVE(cfg)) {
		/* valid scan, analysis the result */
		wlc_ht_coex_filter_scanresult(cfg);
		obss->scan_countdown = params->bss_widthscan_interval;
		WL_COEX(("wl%d: %s: Arm scan for %dsec\n", scan_info->unit, __FUNCTION__,
			obss->scan_countdown));
	}
	/* XXX AP need to take the scan result and find a channel to
	 * operate
	 */
#endif /* !SCANOL */
}
#endif /* WL11N */

#ifdef WLSCANCACHE

/* Add the current wlc_scan_info->scan_results to the scancache */
static void
wlc_scan_fill_cache(scan_info_t *scan_info, uint current_timestamp)
{
	uint index;
	wlc_bss_list_t *scan_results;
	wlc_bss_info_t *bi;
	wlc_ssid_t SSID;
	uint8 bsstype;
	size_t datalen = 0;
	uint8* data = NULL;
	size_t bi_len;
	wlc_bss_info_t *new_bi;

	scan_results = SCAN_RESULT_PTR(scan_info);
	ASSERT(scan_results);

	wlc_scandb_ageout(scan_info->sdb, current_timestamp);

	/* walk the list of scan resutls, adding each to the cache */
	for (index = 0; index < scan_results->count; index++) {
		bi = scan_results->ptrs[index];
		if (bi == NULL) continue;

		bi_len = sizeof(wlc_bss_info_t);
		if (bi->bcn_prb)
			bi_len += bi->bcn_prb_len;

		/* allocate a new buffer if the current one is not big enough */
		if (data == NULL || bi_len > datalen) {
			if (data != NULL)
				MFREE(scan_info->osh, data, datalen);

			datalen = ROUNDUP(bi_len, 64);

			data = MALLOC(scan_info->osh, datalen);
			if (data == NULL)
				continue;
		}

		new_bi = (wlc_bss_info_t*)data;

		memcpy(new_bi, bi, sizeof(wlc_bss_info_t));
		if (bi->bcn_prb) {
			new_bi->bcn_prb = (struct dot11_bcn_prb*)(data + sizeof(wlc_bss_info_t));
			memcpy(new_bi->bcn_prb, bi->bcn_prb, bi->bcn_prb_len);
		}
		new_bi->flags |= WLC_BSS_CACHE;

		bsstype = bi->infra ? DOT11_BSSTYPE_INFRASTRUCTURE : DOT11_BSSTYPE_INDEPENDENT;
		SSID.SSID_len = bi->SSID_len;
		memcpy(SSID.SSID, bi->SSID, DOT11_MAX_SSID_LEN);

		wlc_scandb_add(scan_info->sdb,
		               &bi->BSSID, &SSID, bsstype, bi->chanspec, current_timestamp,
		               new_bi, bi_len);
	}

	if (data != NULL)
		MFREE(scan_info->osh, data, datalen);
}

/* Return the contents of the scancache in the 'bss_list' param.
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

/* Merge the contents of the scancache with entries already in the 'bss_list' param.
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

	/* skip the most recent batch of results when merging the cache to a bss_list */
	if (params->merge &&
	    params->current_ts == timestamp)
		return;

	if (bss_list->count >= (uint)SCAN_MAXBSS(scan_info))
		return;

	bi = MALLOC(scan_info->osh, sizeof(wlc_bss_info_t));
	if (!bi) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
		          scan_info->unit, __FUNCTION__, MALLOCED(scan_info->osh)));
		return;
	}

	ASSERT(data != NULL);
	ASSERT(datalen >= sizeof(wlc_bss_info_t));

	cache_bi = (wlc_bss_info_t*)data;

	memcpy(bi, cache_bi, sizeof(wlc_bss_info_t));
	if (cache_bi->bcn_prb_len) {
		ASSERT(datalen >= sizeof(wlc_bss_info_t) + bi->bcn_prb_len);
		if (!(bi->bcn_prb = MALLOC(scan_info->osh, bi->bcn_prb_len))) {
			WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
				scan_info->unit, __FUNCTION__, MALLOCED(scan_info->osh)));
			MFREE(scan_info->osh, bi, sizeof(wlc_bss_info_t));
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

}

static void
wlc_scan_cache_result(scan_info_t *scan_info)
{
	wlc_scan_info_t	*wlc_scan_info = scan_info->scan_pub;
	uint timestamp = OSL_SYSUPTIME();

	/* if we have scan caching enabled, enter these results in the cache */
	wlc_scan_fill_cache(scan_info, timestamp);

	/* filter to just the desired SSID if we did a bcast scan for suppress */
#ifdef EXT_STA
	if (IS_EXTSTA_ENAB(scan_info)) {
		if (scan_info->suppress_ssid) {
			uint i, dst;
			wlc_bss_info_t **list = SCAN_RESULT_MEB(scan_info, ptrs);
			wlc_bss_info_t *bi;
			wlc_ssid_t *ssid = &scan_info->ssid_list[0];
			uint count = SCAN_RESULT_MEB(scan_info, count);

			for (i = 0, dst = 0; i < count; i++) {
				bi = list[i];

				if ((bi->SSID_len == ssid->SSID_len) &&
				    !bcmp(bi->SSID, ssid->SSID, ssid->SSID_len)) {
					list[dst] = list[i];
					dst++;
				} else {
					if (bi->bcn_prb) {
						MFREE(scan_info->osh, bi->bcn_prb, bi->bcn_prb_len);
					}
					MFREE(scan_info->osh, bi, sizeof(wlc_bss_info_t));
					list[i] = NULL;
				}
			}
			SCAN_RESULT_MEB(scan_info, count) = dst;
		}
	}
#endif /* EXT_STA */

	/* Provide the latest results plus cached results if they were requested. */
	if (wlc_scan_info->state & SCAN_STATE_INCLUDE_CACHE) {
		/* Merge cached results with current results */
		wlc_scan_merge_cache(scan_info, timestamp,
		                     &wlc_scan_info->bssid,
		                     scan_info->nssid, &scan_info->ssid_list[0],
		                     wlc_scan_info->bss_type,
		                     scan_info->chanspec_list, scan_info->channel_num,
		                     SCAN_RESULT_PTR(scan_info));

		WL_SCAN(("wl%d: %s: Merged scan results with cache, new total %d\n",
		         scan_info->unit, __FUNCTION__, SCAN_RESULT_MEB(scan_info, count)));
	}
}
#endif /* WLSCANCACHE */

static int
wlc_scan_watchdog(void *hdl)
{
#ifdef WLSCANCACHE
	scan_info_t *scan = (scan_info_t *)hdl;

	/* ageout any old entries to free up memory */
	if (SCANCACHE_ENAB(scan->scan_pub))
		wlc_scandb_ageout(scan->sdb, OSL_SYSUPTIME());
#endif // endif
	return 0;
}

#ifdef EXTENDED_SCAN
void
wlc_extdscan(wlc_scan_info_t *wlc_scan_info, int max_txrate,
	int nchan, chan_scandata_t *channel_list, wl_scan_type_t scan_type,
	int nprobes, bool split_scan, int nssid, wlc_ssid_t *ssid, scancb_fn_t fn, void *arg)
{
	chanspec_t	chanspec_list_arg[MAXCHANNEL];
	uint32 	scan_mode;
	int	i;
	chan_scandata_t	*cur_chandata;
	scan_info_t	*scan_info = (scan_info_t *) wlc_scan_info->scan_priv;
	int	away_channels_limit = 0;

	if (nchan == 0)
		return;

	/* split scan only one away channel to scan before coming back to home channel */
	if (split_scan)
		away_channels_limit = 1;

	/* passive/active scan */
	if (nprobes == 0)
		scan_mode = DOT11_SCANTYPE_PASSIVE;
	else
		scan_mode = DOT11_SCANTYPE_ACTIVE;

	scan_info->scan_type = scan_type;
	scan_info->max_txrate = max_txrate;

	/* channel list -- for now assume prealloc store in use */
	if (nchan > scan_info->nchan_prealloc)
		nchan = scan_info->nchan_prealloc;

	bzero(scan_info->chan_list, sizeof(chan_scandata_t) * scan_info->nchan_prealloc);
	bzero(chanspec_list_arg, sizeof(chanspec_list_arg));
	for (i = 0; i < nchan; i++) {
		cur_chandata = &channel_list[i];
		bcopy(cur_chandata, &scan_info->chan_list[i], sizeof(chan_scandata_t));
		chanspec_list_arg[i] = CH20MHZ_CHSPEC(cur_chandata->channel);
		WL_SCAN(("%d: Channel 0x%x, max_time %d, min_time %d\n", i, chanspec_list_arg[i],
			cur_chandata->channel_maxtime, cur_chandata->channel_mintime));
	}
	/* call the wlc_scan with the common API */
	wlc_scan(wlc_scan_info, DOT11_BSSTYPE_ANY, &ether_bcast, nssid, ssid,
		scan_mode, nprobes, 0, 0, -1,
		chanspec_list_arg, nchan, -1, FALSE, fn,
		arg, away_channels_limit, TRUE, FALSE, SCANCACHE_ENAB(wlc_scan_info),
		FALSE, wlc->cfg, SCAN_ENGINE_USAGE_NORM, NULL, NULL);
}

int
wlc_extdscan_request(wlc_scan_info_t *wlc_scan_info, void *param, int len,
	scancb_fn_t fn, void* arg)
{
	scan_info_t		*scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
	chanspec_t		chanspec_list[MAXCHANNEL];
	wl_extdscan_params_t 	*extdscan_params = NULL; /* to avoid alignment issues */
	int 			i, nchan;
	int 			ssid_count = 0, chandata_list_size = 0;
	chan_scandata_t		*chandata_list = NULL, *chanptr, *chanarg;
	wlc_ssid_t		 *ssid;
	int 			bcmerror = 0;

	/* validate the user passed arguments */
	if (len < WL_EXTDSCAN_PARAMS_FIXED_SIZE) {
		WL_ERROR(("%s: len is %d\n", __FUNCTION__, len));
		bcmerror = BCME_BADARG;
		goto done;
	}

	bzero(chanspec_list, sizeof(chanspec_list));
	nchan = 0;
	extdscan_params = (wl_extdscan_params_t *)MALLOC(scan_info->osh, len);
	if (extdscan_params == NULL) {
		WL_ERROR(("%s: NULL extdscan_params!\n", __FUNCTION__));
		bcmerror = BCME_NORESOURCE;
		goto done;
	}
	bcopy(param, (void *)extdscan_params, len);

#if defined(BCMDBG) || defined(WLMSG_INFORM)
	{
		bool n_ssid = FALSE;

		WL_INFORM(("Scan Params are \n"));
		WL_INFORM(("txrate is %d\n", extdscan_params->tx_rate));
		WL_INFORM(("nprobes is %d\n", extdscan_params->nprobes));
		WL_INFORM(("scan_type is %d\n", extdscan_params->scan_type));
		WL_INFORM(("split_scan is %d\n", extdscan_params->split_scan));
		WL_INFORM(("band is %d\n", extdscan_params->band));
		WL_INFORM(("channel_num is %d\n", extdscan_params->channel_num));
		chanarg = &extdscan_params->channel_list[0];
		for (i = 0; i < extdscan_params->channel_num; i++) {
			WL_INFORM(("Channel %d, txpower %d, chanmaxtime %d, chanmintime %d\n",
				chanarg->channel, chanarg->txpower,
				chanarg->channel_maxtime, chanarg->channel_mintime));
			chanarg++;
		}
		for (i = 0; i < WLC_EXTDSCAN_MAX_SSID; i++) {
			char ssidbuf[128];
			ssid = &extdscan_params->ssid[i];
			if (ssid->SSID_len) {
				if (!n_ssid) {
					n_ssid = TRUE;
					WL_INFORM(("ssids are \n"));
				}
				wlc_format_ssid(ssidbuf, ssid->SSID, ssid->SSID_len);
				WL_INFORM(("%d: \"%s\"\n", i, ssidbuf));
			}
		}
		if (!n_ssid) {
			WL_INFORM(("SSID List Empty: broadcast Scan\n"));
		}
	}
#endif /* BCMDBG || WLMSG_INFORM */

	if (extdscan_params->channel_num == 0)
		chandata_list_size = MAXCHANNEL * sizeof(chan_scandata_t);
	else
		chandata_list_size = extdscan_params->channel_num * sizeof(chan_scandata_t);

	chandata_list = (chan_scandata_t*)MALLOC(scan_info->osh, chandata_list_size);
	if (chandata_list == NULL) {
		WL_ERROR(("%s: NULL chandata_list!\n", __FUNCTION__));
		bcmerror = BCME_NORESOURCE;
		goto done;
	}
	chanptr = chandata_list;

	/* one band yet a time for now */
	if (extdscan_params->channel_num == 0) {
		uint32 			band, bd;
		int 			channel_count;
		chanspec_t		chanspec;

		wlc_scan_default_channels(scan_info->scan_pub, chanspec_list,
		                          &channel_count);
		band = extdscan_params->band;
		for (i = 0; i < channel_count; i++) {
			chanspec = chanspec_list[i];
			bd = chanspec & WL_CHANSPEC_BAND_MASK;
			if ((band == WLC_BAND_ALL) ||
			    ((band == WLC_BAND_2G) && (bd == WL_CHANSPEC_BAND_2G)) ||
			    ((band == WLC_BAND_5G) && (bd == WL_CHANSPEC_BAND_5G))) {
				chanptr->channel = CHSPEC_CHANNEL(chanspec);
				chanptr->channel_mintime = WLC_SCAN_ASSOC_TIME;
				chanptr->channel_maxtime = WLC_SCAN_ASSOC_TIME;
				chanptr++;
				nchan++;
			}
		}
	}
	else {
		chanarg = &extdscan_params->channel_list[0];
		nchan = extdscan_params->channel_num;
		bcopy((void *)chanarg, (void *)chanptr, ((sizeof(chan_scandata_t)) * nchan));
	}

	/* for now, limited to prealloced channel area, bail if too many */
	if (nchan > scan_info->nchan_prealloc) {
		WL_ERROR(("Extd scan: %s chan count %d exceeds prealloc %d\n",
		          (extdscan_params->channel_num ? "requested" : "default"),
		          extdscan_params->channel_num, scan_info->nchan_prealloc));
		bcmerror = extdscan_params->channel_num ? BCME_BADARG : BCME_RANGE;
		goto done;
	}

	/* validate the user passed arguments */
	for (i = 0; i < WLC_EXTDSCAN_MAX_SSID; i++) {
		ssid = &(extdscan_params->ssid[i]);
		if (ssid->SSID_len > 0)
			ssid_count++;
	}
	/* Broadcast Scan */
	if (!ssid_count) {
		WL_INFORM(("Broadcast Scan\n"));
		ssid = &(extdscan_params->ssid[0]);
		ssid->SSID_len = 0;
		ssid->SSID[0] = 0x00;
		ssid_count = 1;
	}

	/* for now, limited to prealloced ssid area, bail if too many */
	if (ssid_count > scan_info->nssid_prealloc) {
		WL_ERROR(("Extd scan: %d ssids exceeds prealloc of %d\n",
		          ssid_count, scan_info->nssid_prealloc));
		bcmerror = BCME_BADARG;
		goto done;
	}

	/* make the extd scan call */
	if (!fn) {
		fn = wlc_custom_scan_complete;
		arg = (void *)scan_info->wlc;
	}
	wlc_extdscan(wlc_scan_info, extdscan_params->tx_rate, nchan, chandata_list,
		extdscan_params->scan_type, extdscan_params->nprobes,
		extdscan_params->split_scan, ssid_count, extdscan_params->ssid, fn, arg);

done:
	if (bcmerror && fn)
		(fn)(arg, WLC_E_STATUS_FAIL, NULL);

	if (extdscan_params)
		MFREE(scan_info->osh, extdscan_params, len);
	if (chandata_list)
		MFREE(scan_info->osh, chandata_list, chandata_list_size);
	return bcmerror;
}
#endif /* EXTENDED_SCAN */

wlc_bsscfg_t *
wlc_scan_bsscfg(wlc_scan_info_t *wlc_scan_info)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
	return scan_info->bsscfg;
}

#ifdef WLSCAN_PS
/* This function configures tx & rxcores to save power.
 *  flag: TRUE to set & FALSE to revert config
 */
static int
wlc_scan_ps_config_cores(scan_info_t *scan_info, bool flag)
{
	if (flag) {
		/* Scanning is started */
		if ((
#if defined(BCMDBG) || defined(WLTEST)
			scan_info->scan_tx_pwrsave ||
#endif /* defined(BCMDBG) || defined(WLTEST) */
			scan_info->scan_pwrsave_enable) && !scan_info->scan_ps_txchain) {
			if (wlc_stf_txchain_ishwdef(scan_info->wlc) &&
				scan_info->wlc->stf->hw_txchain >= 0x03) {
				wlc_suspend_mac_and_wait(scan_info->wlc);
				/* back up chain configuration */
				scan_info->scan_ps_txchain = scan_info->wlc->stf->txchain;
				wlc_stf_txchain_set(scan_info->wlc, 0x1, FALSE, WLC_TXCHAIN_ID_USR);
				wlc_enable_mac(scan_info->wlc);
			}
		}

		if ((
#if defined(BCMDBG) || defined(WLTEST)
			scan_info->scan_rx_pwrsave ||
#endif /* defined(BCMDBG) || defined(WLTEST) */
			scan_info->scan_pwrsave_enable) && !scan_info->scan_ps_rxchain) {
			if (wlc_stf_rxchain_ishwdef(scan_info->wlc) &&
				scan_info->wlc->stf->hw_rxchain >= 0x03) {
				wlc_suspend_mac_and_wait(scan_info->wlc);
				/* back up chain configuration */
				scan_info->scan_ps_rxchain = scan_info->wlc->stf->rxchain;
				wlc_stf_rxchain_set(scan_info->wlc, 0x1, TRUE);
				wlc_enable_mac(scan_info->wlc);
			}
		}
	} else {
		/* Scanning is ended */
		if (!scan_info->scan_ps_txchain && !scan_info->scan_ps_rxchain) {
			return BCME_OK;
		} else {
			/* when scan_ps_txchain is 0, it mean scan module has not modified chains */
			if (scan_info->scan_ps_txchain) {
				wlc_suspend_mac_and_wait(scan_info->wlc);
				wlc_stf_txchain_set(scan_info->wlc, scan_info->scan_ps_txchain,
				                    FALSE, WLC_TXCHAIN_ID_USR);
				scan_info->scan_ps_txchain = 0;
				wlc_enable_mac(scan_info->wlc);
			}
			if (scan_info->scan_ps_rxchain) {
				wlc_suspend_mac_and_wait(scan_info->wlc);
				wlc_stf_rxchain_set(scan_info->wlc,
				                    scan_info->scan_ps_rxchain, TRUE);
				/* make the chain value to 0 */
				scan_info->scan_ps_rxchain = 0;
				wlc_enable_mac(scan_info->wlc);
			}
		}
	}

	return BCME_OK;
}
#endif /* WLSCAN_PS */

#if (defined(BCMDBG) || defined(BCMDBG_DUMP)) && !defined(SCANOL)
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
		"af",
		"rm",
		"excursion"
	};
	char state_str[64];
	char ssidbuf[SSID_FMT_BUF_LEN];
	char eabuf[ETHER_ADDR_STR_LEN];
	const char *bss_type_str;
	uint32 tsf_l, tsf_h;
	struct wlc_scan_info *scan_pub = si->scan_pub;

	wlc_format_ssid(ssidbuf, si->ssid_list[0].SSID, si->ssid_list[0].SSID_len);
	bcm_format_flags(scan_flags, scan_pub->state, state_str, 64);

	if (scan_pub->bss_type == DOT11_BSSTYPE_INFRASTRUCTURE)
		bss_type_str = "Infra";
	else if (scan_pub->bss_type == DOT11_BSSTYPE_INDEPENDENT)
		bss_type_str = "IBSS";
	else
		bss_type_str = "any";

	bcm_bprintf(b, "in_progress %d SSID \"%s\" type %s BSSID %s state 0x%x [%s] "
	            "usage %u [%s]\n",
	            scan_pub->in_progress, ssidbuf, bss_type_str,
	            bcm_ether_ntoa(&scan_pub->bssid, eabuf),
	            scan_pub->state, state_str,
	            scan_pub->usage, scan_pub->usage < ARRAYSIZE(scan_usage) ?
	                             scan_usage[scan_pub->usage] : "unknown");

	bcm_bprintf(b, "extdscan %d\n", si->extdscan);

	bcm_bprintf(b, "away_channels_cnt %d pass %d scan_results %d\n",
	            si->away_channels_cnt, si->pass, SCAN_RESULT_MEB(si, count));

	if (SCAN_IN_PROGRESS(scan_pub))
		bcm_bprintf(b, "wlc->home_chanspec: %x chanspec_current %x\n",
		            SCAN_HOME_CHANNEL(si), si->chanspec_list[si->channel_idx]);

#ifdef EXT_STA
	bcm_bprintf(b, "suppress_ssid %d\n", si->suppress_ssid);
#endif /* EXT_STA */

	if (SCAN_ISUP(si)) {
		SCAN_READ_TSF(si, &tsf_l, &tsf_h);
		bcm_bprintf(b, "start_tsf 0x%08x current tsf 0x%08x\n", si->start_tsf, tsf_l);
	} else {
		bcm_bprintf(b, "start_tsf 0x%08x current tsf <not up>\n", si->start_tsf);
	}

	return 0;
}
#endif /* (BCMDBG || BCMDBG_DUMP) && !SCANOL */
