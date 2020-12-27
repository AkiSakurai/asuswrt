/**
 * @File
 * @brief
 * Common (OS-independent) portion of Broadcom 802.11bang Networking Device Driver
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
 * $Id: wlc.c 778114 2019-08-22 19:38:37Z $
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmwifi_channels.h>
#include <siutils.h>
#include <pcie_core.h>
#include <nicpci.h>
#include <bcmendian.h>
#include <802.1d.h>
#include <802.11.h>
#include <802.11e.h>
#include <bcmip.h>
#include <wpa.h>
#include <vlan.h>
#include <hndsoc.h>
#include <sbchipc.h>
#include <pcicfg.h>
#include <bcmsrom.h>
#if defined(WLTEST)
#include <bcmnvram.h>
#endif // endif
#include <wlioctl.h>
#include <epivers.h>
#include <bcmwpa.h>
#include <sbhndpio.h>
#include <sbhnddma.h>
#include <hnddma.h>
#include <hndpmu.h>
#include <d11.h>
#include <hndd11.h>
#include <d11_cfg.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_cca.h>
#include <wlc_interfere.h>
#include <wlc_keymgmt.h>
#include <wlc_bsscfg.h>
#include <wlc_mbss.h>
#include <wlc_channel.h>
#include <wlc.h>
#include <wlc_hw.h>
#include <wlc_hw_priv.h>
#include <wlc_bmac.h>
#include <wlc_apps.h>
#include <wlc_scb.h>
#include <wlc_phy_hal.h>
#include <phy_utils_api.h>
#include <phy_calmgr_api.h>
#include <phy_api.h>
#include <phy_ana_api.h>
#include <phy_cache_api.h>
#include <phy_radio_api.h>
#include <phy_antdiv_api.h>
#include <phy_rssi_api.h>
#include <phy_misc_api.h>
#include <phy_noise_api.h>
#include <phy_high_api.h>
#include <phy_dbg_api.h>
#include <phy_rxspur_api.h>
#include <phy_tpc_api.h>
#include <phy_chanmgr_api.h>
#include <phy_prephy_api.h>
#include <phy_wd_api.h>
#if defined(BGDFS) || defined(WL_AIR_IQ)
#include <phy_ac_info.h>
#endif /* BGDFS  || WL_AIR_IQ */
#include <wlc_iocv_high.h>
#include <wlc_bmac_iocv.h>
#include <wlc_antsel.h>
#include <wlc_led.h>
#include <wlc_frmutil.h>
#include <wlc_stf.h>
#include <wlc_rsdb.h>
#if defined(WLRSDB_POLICY_MGR) && defined(WLRSDB)
#include <wlc_rsdb_policymgr.h>
#endif /* WLRSDB_POLICY_MGR && WLRSDB */
#ifdef WLNAR
#include <wlc_nar.h>
#endif // endif
#if defined(SCB_BS_DATA)
#include <wlc_bs_data.h>
#endif /* SCB_BS_DATA */
#ifdef PKTQ_LOG
#include <wlc_rx_report.h>
#endif // endif
#if defined(WLAMSDU) || defined(WLAMSDU_TX)
#include <wlc_amsdu.h>
#endif // endif
#ifdef WLAMPDU
#include <wlc_ampdu.h>
#include <wlc_ampdu_rx.h>
#include <wlc_ampdu_cmn.h>
#endif // endif
#ifdef WLTDLS
#include <wlc_tdls.h>
#endif // endif
#ifdef WLDLS
#include <wlc_dls.h>
#endif // endif
#ifdef WLTAF
#include <wlc_taf.h>
#endif // endif
#if defined(WLBSSLOAD) || defined(WLBSSLOAD_REPORT)
#include <wlc_bssload.h>
#endif // endif
#ifdef L2_FILTER
#include <wlc_l2_filter.h>
#endif // endif
#ifdef WLMCNX
#include <wlc_mcnx.h>
#include <wlc_tbtt.h>
#endif // endif
#include <wlc_p2p.h>
#ifdef BCMCOEXNOA
#include <wlc_cxnoa.h>
#endif // endif
#ifdef WLMCHAN
#include <wlc_mchan.h>
#endif // endif
#include <wlc_scb_ratesel.h>
#ifdef WL_LPC
#include <wlc_scb_powersel.h>
#endif // endif
#include <wlc_event.h>
#ifdef WOWL
#include <wlc_wowl.h>
#endif // endif
#ifdef WOWLPF
#include <wlc_wowlpf.h>
#endif // endif
#include <wlc_seq_cmds.h>
#ifdef WLOTA_EN
#include <wlc_ota_test.h>
#endif /* WLOTA_EN */
#ifdef WLDIAG
#include <wlc_diag.h>
#endif // endif
#include <wl_export.h>
#if defined(BCMSUP_PSK)
#include <wlc_sup.h>
#endif // endif
#include <wlc_pmkid.h>
#if defined(BCMAUTH_PSK)
#include <wlc_auth.h>
#endif // endif
#ifdef WET
#include <wlc_wet.h>
#endif // endif
#ifdef WET_TUNNEL
#include <wlc_wet_tunnel.h>
#endif // endif
#ifdef WMF
#include <wlc_wmf.h>
#endif // endif
#ifdef PSTA
#include <wlc_psta.h>
#endif /* PSTA */
#if defined(BCMNVRAMW) || defined(WLTEST)
#include <bcmotp.h>
#endif // endif
#ifdef BCMCCMP
#include <aes.h>
#endif // endif
#include <wlc_rm.h>
#include "wlc_cac.h"
#include <wlc_ap.h>
#ifdef AP
#include <wlc_apcs.h>
#endif // endif
#include <wlc_scan_utils.h>
#ifdef WL11K
#include <wlc_rrm.h>
#endif /* WL11K */
#ifdef WLWNM
#include <wlc_wnm.h>
#endif // endif
#include <wlc_alloc.h>
#include <wlc_assoc.h>
#if defined(RWL_WIFI) || defined(WIFI_REFLECTOR)
#include <wlc_rwl.h>
#endif // endif
#ifdef WLPFN
#include <wl_pfn.h>
#endif // endif
#ifdef STA
#include <wlc_wpa.h>
#endif /* STA */
#if defined(PROP_TXSTATUS)
#include <wlc_wlfc.h>
#endif // endif
#include <wlc_lq.h>
#include <wlc_11h.h>
#include <wlc_tpc.h>
#include <wlc_csa.h>
#include <wlc_quiet.h>
#include <wlc_dfs.h>
#include <wlc_11d.h>
#include <wlc_cntry.h>
#include <bcm_mpool_pub.h>
#include <wlc_utils.h>
#include <wlc_hrt.h>
#include <wlc_prot.h>
#include <wlc_prot_g.h>
#define _inc_wlc_prot_n_preamble_	/* include static INLINE uint8 wlc_prot_n_preamble() */
#include <wlc_prot_n.h>
#if defined(WL_PROT_OBSS) && !defined(WL_PROT_OBSS_DISABLED)
#include <wlc_prot_obss.h>
#endif // endif
#if defined(WL_OBSS_DYNBW) && !defined(WL_OBSS_DYNBW_DISABLED)
#include <wlc_obss_dynbw.h>
#endif /* defined(WL_OBSS_DYNBW) && !defined(WL_OBSS_DYNBW_DISABLED) */
#include <wlc_11u.h>
#include <wlc_probresp.h>
#ifdef WLTOEHW
#include <wlc_tso.h>
#endif /* WLTOEHW */
#include <wlc_vht.h>
#include <wlc_txbf.h>
#include <wlc_pcb.h>
#include <wlc_txc.h>
#ifdef EVENT_LOG_COMPILE
#include <event_log.h>
#endif // endif
#ifdef MFP
#include <wlc_mfp.h>
#endif // endif
#include <wlc_macfltr.h>
#include <wlc_addrmatch.h>
#ifdef WL_RELMCAST
#include "wlc_relmcast.h"
#endif // endif
#include <wlc_btcx.h>
#include <wlc_ie_mgmt.h>
#include <wlc_ie_mgmt_ft.h>
#include <wlc_ie_mgmt_vs.h>
#include <wlc_ie_helper.h>
#include <wlc_ie_reg.h>
#include <wlc_akm_ie.h>
#include <wlc_ht.h>
#include <wlc_obss.h>

#ifdef ANQPO
#include <wl_anqpo.h>
#endif /* ANQPO */
#include <wlc_hs20.h>
#include <wlc_pm.h>
#ifdef WLFBT
#include <wlc_fbt.h>
#endif /* WLFBT */
#if defined(WL_ASSOC_MGR)
#include <wlc_assoc_mgr.h>
#endif /* defined(WL_ASSOC_MGR) */
#if defined(WL_OKC) || defined(WLRCC)
#include <wlc_okc.h>
#endif /* defined(WL_OKC) || defined(WLRCC) */
#ifdef WLOLPC
#include <wlc_olpc_engine.h>
#endif /* OPEN LOOP POWER CAL */
#ifdef WL_STAPRIO
#include <wlc_staprio.h>
#endif /* WL_STAPRIO */
#include <monitor.h>
#include <wlc_monitor.h>
#include <wlc_stamon.h>
#include <wlc_ie_misc_hndlrs.h>
#ifdef WDS
#include <wlc_wds.h>
#endif // endif
#ifdef WLDURATION
#include <wlc_duration.h>
#endif // endif

#if defined(WLC_DEBUG_CRASH)
#include <wlc_debug_crash.h>
#endif /* WLC_DEBUG_CRASH */

#if defined(WL_STATS)
#include <wlc_stats.h>
#endif /* WL_STATS */
#include <wlc_objregistry.h>
#include <wlc_tx.h>
#ifdef WL_PROXDETECT
#include <wlc_pdsvc.h>
#endif /* PROXIMITY DETECTION */
#ifdef BCMLTECOEX
#include <wlc_ltecx.h>
#endif /* BCMLTECOEX */
#ifdef WLPROBRESP_MAC_FILTER
#include <wlc_probresp_mac_filter.h>
#endif // endif
#ifdef BCMSPLITRX
#include <wlc_ampdu_rx.h>
#include <wlc_pktfetch.h>
#endif /* BCMSPLITRX */
#ifdef WL_BWTE
#include <wlc_bwte.h>
#endif // endif
#ifdef WL_LTR
#include <wlc_ltr.h>
#endif /* Latency Tolerance Reporting */
#ifdef WL_TBOW
#include <wlc_tbow.h>
#endif // endif
#include <wlc_rx.h>
#include <wlc_dbg.h>
#include <wlc_macdbg.h>
#include <wlc_macreq.h>
#include <wlc_txcfg.h>
#include <wlc_fifo.h>
#ifdef WL_MODESW
#include <wlc_modesw.h>
#endif // endif
#include <wlc_msch.h>
#include <wlc_chanctxt.h>
#include <wlc_sta.h>
#ifdef WIFI_ACT_FRAME
#include <wlc_act_frame.h>
#endif // endif
#ifdef WL_PM_MUTE_TX
#include <wlc_pm_mute_tx.h>
#endif // endif
#include <wlc_bcntrim.h>
#if defined(WLVASIP)
#include <d11vasip_code.h>
#include <wlc_vasip.h>
#endif /* WLVASIP */
#include <wlc_misc.h>
#include <wlc_smfs.h>
#include <wlc_bsscfg_psq.h>
#include <wlc_txmod.h>
#include <wlc_bsscfg_viel.h>
#if defined(GTKOE)
#include <wl_gtkrefresh.h>
#endif // endif
#include <wlc_pktc.h>
#ifdef WL_RANDMAC
#include <wlc_randmac.h>
#endif /* WL_RANDMAC */
#include <event_trace.h>
#ifdef WL_ASDB
#include <wlc_asdb.h>
#endif // endif
#include <wlc_pspretend.h>
#ifdef BCMULP
#include <wlc_ulp.h>
#endif // endif
#include <wlc_frag.h>
#include <wlc_qoscfg.h>
#include <wlc_rspec.h>
#if defined(WL_PWRSTATS)
#include <wlc_pwrstats.h>
#endif // endif

#ifdef WL_MU_TX
#include <wlc_mutx.h>
#endif // endif

#ifdef WL_MU_RX
#include <wlc_murx.h>
#endif // endif
#if defined(WL_LINKSTAT)
#include <wlc_linkstats.h>
#endif // endif
#include <wlc_event_utils.h>
#include <wlc_txs.h>
#if defined(WL_HEALTH_CHECK)
#include "wlc_health_check.h"
#endif // endif
#include <wlc_perf_utils.h>
#include <wlc_test.h>
#include <wlc_srvsdb.h>
#include <wlc_dump.h>
#include <wlc_iocv.h>
#include <wlc_he.h>
#include <wlc_twt.h>
#include <wlc_heb.h>
#if defined(WLC_SW_DIVERSITY)
#include <wlc_swdiv.h>
#endif // endif
#ifdef WLC_TSYNC
#include <wlc_tsync.h>
#endif // endif

/* For dongle devices */
#ifdef HEALTH_CHECK
#include <hnd_hchk.h>
#endif // endif
#include <hnd_ds.h>

#include <wlc_log.h>

#if defined(WLATF) || defined(WLATF_PERC)
#include <wlc_airtime.h>
#endif /* WLATF || WLATF_PERC */

#ifdef ECOUNTERS
#include <ecounters.h>
#endif // endif
#ifdef BCMFRWDPOOLREORG
#include <hnd_poolreorg.h>
#include <wlc_poolreorg.h>
#endif // endif

#ifdef WL_MBO_OCE
#include <wlc_mbo_oce.h>
#endif /* WL_MBO_OCE */

#if defined(WL_MBO) && !defined(WL_MBO_DISABLED) && defined(MBO_AP)
#include <wlc_mbo.h>
#endif /* WL_MBO && !WL_MBO_DISABLED ** MBO_AP */

#ifdef WLCFP
#include <wlc_cfp.h>
#endif // endif
#ifdef WLSQS
#include <wlc_sqs.h>
#endif // endif
#ifdef WL_OCE
#include <wlc_oce.h>
#endif // endif

#ifdef WL_FILS
#include <wlc_fils.h>
#endif // endif

#ifdef WL_ESP
#include <wlc_esp.h>
#endif // endif

#ifdef BCMFCBS
#include <fcbs.h>
#endif // endif
#include <wlc_chctx.h>

#ifdef WLSLOTTED_BSS
#include <wlc_slotted_bss.h>
#endif // endif

#ifdef WL_LEAKY_AP_STATS
#include <wlc_leakyapstats.h>
#endif /* WL_LEAKY_AP_STATS */

#ifdef WLC_BAM
#include <wlc_bad_ap_manager.h>
#endif // endif

#ifdef WLC_ADPS
#include <wlc_adps.h>
#endif	/* WLC_ADPS */

#ifdef WL_FILTERIE
#include <wlc_filter_ie.h>
#endif /* WL_FILTERIE */

#include <wlc_tvpm.h>

#include <wlc_chsw_timecal.h>

#if defined(TDMTX)
#include <wlc_tdm_tx.h>
#endif // endif

#ifdef RADIONOA_PWRSAVE
#include <wlc_rpsnoa.h>
#endif // endif

#include <wlc_ops.h>
#include <wlc_ratelinkmem.h>

#if defined(PKTC_TBL)
#include <wl_pktc.h>
#endif // endif
#ifdef WL_MUSCHEDULER
#include <wlc_musched.h>
#endif /* WL_MUSCHEDULER */

#if defined(WL_AIR_IQ)
#include "wlc_airiq.h"
#endif /* WL_AIR_IQ */

#ifdef WL_GLOBAL_RCLASS
#include <bcmwifi_rclass.h>
#endif /* WL_GLOBAL_RCLASS */
#if defined(WLC_OFFLOADS_TXSTS) || defined(WLC_OFFLOADS_RXSTS)
#include <wlc_offld.h>
#endif /* WLC_OFFLOADS_TXSTS || WLC_OFFLOADS_RXSTS */

#include <wlc_pmq.h>

/*
 * buffer length needed for wlc_format_ssid
 * 32 SSID chars, max of 4 chars for each SSID char "\xFF", plus NULL.
 */
#define SSID_FMT_BUF_LEN	((4 * DOT11_MAX_SSID_LEN) + 1)

#define	TIMER_INTERVAL_WATCHDOG	1000	/* watchdog timer, in unit of ms */
#define	TIMER_INTERVAL_RADIOCHK	2000	/* radio monitor timer, in unit of ms */

/**
 * schedule watchdog after all bc/mc are received.
 * bcn bcmc bit set but did not recv bc/mc frames or timeout if more_data=0 frame is lost
 * Assuming (worst case)1500byte bcmc frame with 1mbps data rate: 12 (+1ms buffer).
 */
#define WLC_WD_BCMC_TIMEOUT		13	/* ms */

#ifndef AP_KEEP_ALIVE_INTERVAL
#define AP_KEEP_ALIVE_INTERVAL  10 /* seconds */
#endif /* AP_KEEP_ALIVE_INTERVAL */

/* softwaer queue precedence choice */
#define HIGH_PREC	TRUE
#define REGULAR_PREC	FALSE

#ifndef WLC_MPC_MAX_DELAYCNT
#define	WLC_MPC_MAX_DELAYCNT	10	/* Max MPC timeout, in unit of watchdog */
#endif // endif

#define	WLC_MPC_MIN_DELAYCNT	0	/* Min MPC timeout, in unit of watchdog */
#define	WLC_MPC_THRESHOLD	3	/* MPC count threshold level */

#define	BEACON_INTERVAL_DEFAULT	100	/* beacon interval, in unit of 1024TU */
#define	DTIM_INTERVAL_DEFAULT	3	/* DTIM interval, in unit of beacon interval */

/* Scale down delays to accommodate QT slow speed */
#define	BEACON_INTERVAL_DEF_QT	20	/* beacon interval, in unit of 1024TU */
#define	DTIM_INTERVAL_DEF_QT	1	/* DTIM interval, in unit of beacon interval */

/* QT PHY */
#define	PRETBTT_PHY_US_QT		10		/* phy pretbtt time in us on QT(no radio) */
/* real PHYs */
#define	PRETBTT_BPHY_US			250		/* b/g phy pretbtt time in us */
#define	PRETBTT_NPHY_US			512		/* n phy REV3 pretbtt time in us */
#define	PRETBTT_ACPHY_US		512		/* acphy pretbtt time in us */
#define PRETBTT_ACPHY_4360_US		1024		/* 4360 pre-btt */
#define	PRETBTT_LCN20PHY_US		384		/* lcn20 phy pretbtt time in us */
#define PRETBTT_ACPHY_4364_US		256             /* 4364 pre-btt */
/* ??? */
#define PRETBTT_ACPHY_AP_US		2		/* ac phy pretbtt time in us - for AP */

/*
 * driver maintains internal 'tick'(wlc->pub->now) which increments in 1s OS timer(soft
 * watchdog) it is not a wall clock and won't increment when driver is in "down" state
 * this low resolution driver tick can be used for maintenance tasks such as phy
 * calibration and scb update
 */
#define	SW_TIMER_IBSS_GMODE_RATEPROBE	60	/* periodic IBSS gmode rate probing */

/* debug/trace */
uint wl_msg_level =
#if defined(BCMDBG) || defined(BCMDBG_ERR)
	WL_ERROR_VAL;
#else
	0;
#endif /* BCMDBG */

uint wl_msg_level2 =
#if defined(BCMDBG)
	0;
#else
	0;
#endif /* BCMDBG */

#define RFDISABLE_DEFAULT	10000000 /* rfdisable delay timer 500 ms, runs of ALP clock */

#define MS_IN_SEC	1000

/* Get the bw, in required BW_XXMHZ encoding, from chanspec */
#define WLC_GET_BW_FROM_CHSPEC(cs) \
	((CHSPEC_IS8080(cs) || CHSPEC_IS160(cs)) ? BW_160MHZ : \
	CHSPEC_IS80(cs) ? BW_80MHZ : \
	CHSPEC_IS40(cs) ? BW_40MHZ : BW_20MHZ)

#ifdef BCMDBG
uint32 wl_apsta_dbg = WL_APSTA_UPDN_VAL;
/* pointer to most recently allocated wl/wlc */
static wlc_info_t *wlc_info_dbg = (wlc_info_t *)(NULL);
#if !defined(BCMDBG_EXCLUDE_HW_TIMESTAMP)
wlc_info_t *wlc_info_time_dbg = (wlc_info_t *)(NULL);
#endif /* !BCMDBG_EXCLUDE_HW_TIMESTAMP */
#endif /* BCMDBG */

#define U32_DUR(a, b) ((uint32)(b) - (uint32)(a))

/* IOVar table */
/* Parameter IDs, for use only internally to wlc -- in the wlc_iovars
 * table and by the wlc_doiovar() function.  No ordering is imposed:
 * the table is keyed by name, and the function uses a switch.
 */
enum wlc_iov {
	IOV_RTSTHRESH = 1,
	IOV_D11_AUTH = 2,
	IOV_VLAN_MODE = 3,
	IOV_WLFEATUREFLAG = 4,
	IOV_STA_INFO = 5,
	IOV_CAP = 6,
	IOV_MALLOCED = 7,
	IOV_WSEC_RESTRICT = 8,
	IOV_WET = 9,
	IOV_MAC_SPOOF = 10,
	IOV_EAP_RESTRICT = 11,
	IOV_FRAGTHRESH = 12,
	IOV_QTXPOWER = 13,
	IOV_WPA_MSGS = 14,
	IOV_WPA_AUTH = 15,
	IOV_EIRP = 16,
	IOV_CUR_ETHERADDR = 17,
	IOV_PERM_ETHERADDR = 18,
	IOV_RAND = 19,
	IOV_MPC = 20,
	IOV_WATCHDOG_DISABLE = 21,
	IOV_MPC_DUR = 22,
	IOV_BCMERRORSTR = 23,
	IOV_BCMERROR = 24,
	IOV_COUNTERS = 25,
	IOV_APSTA_DBG = 26,
	IOV_RFDISABLEDLY = 27,
	IOV_ANTSEL_TYPE = 28,
	IOV_COUNTRY_LIST_EXTENDED = 29,
	IOV_RESET_D11CNTS = 30,
	IOV_IBSS_ALLOWED = 31,
	IOV_MCAST_LIST = 32,
	IOV_BOARDFLAGS = 33,
	IOV_BOARDFLAGS2 = 34,
	IOV_ANTGAIN = 35,
	IOV_LIFETIME = 36,	/* Packet lifetime */
	IOV_TXMAXPKTS = 37,	/* max txpkts that can be pending in txfifo */
	IOV_CHANSPEC = 38,	/* return the current chanspec */
	IOV_CHANSPECS = 39,	/* return the available chanspecs (all or based on options) */
	IOV_TXFIFO_SZ = 40,	/* size of tx fifo */
	IOV_VER = 41,
	IOV_ANTENNAS = 42,	/* num of antennas to be used */
	IOV_NLO = 43,		/* NLO configuration */
	IOV_SENDUP_MGMT = 44,	/* Send up management packet per packet filter setting (unused) */
	IOV_MSGLEVEL2 = 45,
	IOV_INFRA_CONFIGURATION = 46,	/* Reads the configured infra mode */
	IOV_DOWN_OVERRIDE = 47,
	IOV_ALLMULTI = 48,
	IOV_LEGACY_PROBE = 49,
	IOV_BANDUNIT = 50,		/* Get the bandunit from dongle */
	IOV_DELTA_STATS_INTERVAL = 51,
	IOV_DELTA_STATS = 52,
	IOV_NAV_RESET_WAR_DISABLE = 53,	/* WAR to reset NAV on 0 dur ack reception */
	IOV_RFAWARE_LIFETIME = 54,
	IOV_ASSERTLOG = 55,
	IOV_ASSERT_TYPE = 56,
	IOV_PHYCAL_CACHE = 57,
	IOV_CACHE_CHANSWEEP = 58,
	IOV_SENDPRB = 59,
	IOV_POOL = 60,
	IOV_CURR_RATESET = 61,
	IOV_RATESET = 62,
	IOV_BRCM_DCS_REQ = 63,	/* BRCM DCS (RFAware feature) */
	IOV_AUTHOPS = 64,
	IOV_NOLINKUP = 65,
	IOV_DNGL_WD_KEEP_ALIVE = 66,
	IOV_DNGL_WD_FORCE_TRIGGER = 67,
	IOV_IS_WPS_ENROLLEE = 68,
	IOV_WAKE_EVENT = 69,	/* Start wake event timer */
	IOV_TSF = 70,
	IOV_TSF_ADJUST = 71,
	IOV_WLIF_BSSCFG_IDX = 72,
	IOV_SSID = 73,
	IOV_RPT_HITXRATE = 74,	/* report highest tx rate from tx rate history */
	IOV_RXOV = 75,
	IOV_RPMT = 76,		/* Rapid PM transition */
	IOV_BPRESET_ENAB = 77,
	IOV_BACKPLANE_RESET = 78,
	IOV_FCACHE = 79,
	IOV_MEMPOOL = 80,	/* Memory pool manager. */
	IOV_OVERLAY = 81,
	IOV_CLMVER = 82,
	IOV_FABID = 83,
	IOV_BRCM_IE = 84,	/* Advertise brcm ie (default is to advertise) */
	IOV_HEALTH_STATUS = 85,	/* Health status of the card */
	IOV_SAR_ENABLE = 86,
	IOV_SAR_LIMIT = 87,
	IOV_RESET_CNTS = 88,
	IOV_CHANSPECS_DEFSET = 89,	/* Chanspecs with current driver settings */
	IOV_ANTDIV_BCNLOSS = 90,
	IOV_EVENT_LOG_SET_INIT = 92,
	IOV_EVENT_LOG_SET_EXPAND = 93,
	IOV_EVENT_LOG_SET_SHRINK = 94,
	IOV_EVENT_LOG_TAG_CONTROL = 95,
	IOV_EVENT_LOG_GET = 96,
	IOV_PER_CHAN_INFO = 97,
	IOV_ATF = 98,
	IOV_NAS_NOTIFY_EVENT = 99,
	IOV_EARLY_BCN_THRESH = 100,
	IOV_BSS_PEER_INFO = 101,
	IOV_GPAIO = 102,
	IOV_IOC = 103,		/* IOCtl as IOVAR ioc\0<ioctl_cmd_id><params> */
	IOV_PM_BCNRX = 104,
	IOV_TCPACK_FAST_TX = 105, /* Faster AMPDU release and transmission in ucode for TCPACK */
	IOV_WD_ON_BCN = 106,	/* Defer watchdog on beacon */
	IOV_DYNBW = 107,
	IOV_LIFETIME_MG = 108,
	IOV_SUBCOUNTERS = 110,
	IOV_IF_COUNTERS = 111,
	IOV_RXFIFO_COUNTERS = 112,
	IOV_LIFETIME_CTRL = 113,
	IOV_DP_DUMP = 114,
	IOV_HC = 115,
	IOV_BEACON_INFO = 116,
	IOV_WLC_VER = 117,
	IOV_DYN160 = 118,	/* toggle dynamic 160MHz to 80MHz mode active */
	IOV_STA_REPORT = 119,
	IOV_ENT_AP_COUNTERS = 120,
	IOV_SPARE_121 = 121,
	IOV_EMSGLEVEL = 122,
	IOV_RATEOVERRIDE = 124,
	IOV_VAP_STA_L_TIMEOUT = 125,
	IOV_VAP_STA_S_TIMEOUT = 126,
	IOV_VAP_RTSTHRESH = 127,
	IOV_AP_80211_RAW_ENABLE = 128,   /* 802.11 Raw frame upper interface. Per-BSS */
	IOV_AP_80211_RAW_NOCRYPTO = 129, /* No encryption or decryption w/ raw interface. Per-BSS */
	IOV_DTIM = 130,
	IOV_DTIM_EN = 131,
	IOV_DTIM_STATS = 132,
	IOV_DTIM_MAX_MCAST_ENQ = 133,
	IOV_STA_SUPP_CHAN = 134,
	IOV_CHANNEL_UTIL_INTERVAL = 135,
	IOV_CHANNEL_UTIL_DURATION = 136,
	IOV_CCA_CHAN_UTIL = 137,
	IOV_CCA_RX_UTIL = 138, /* Utilization based on times in the ucode */
	IOV_CCA_TX_UTIL = 139,
	IOV_CCA_STATS_RAW = 140, /* RAW stats from ucode CCA */
	IOV_CCA_STATS_DELTA = 141, /* Delta stats from ucode CCA (RAW - previous RAW) */
	IOV_WET_DONGLE = 143,	/* wet enable/disable for dongle mode */
	IOV_EXTERN_EVENT_MSGS_TEST = 146,
	IOV_WL_EAP_STATS = 147,
	IOV_CAP_BEACON = 148,	/* WL_EAP_MONITOR */
	IOV_CAP_PROBE = 149,	/* WL_EAP_MONITOR */
	IOV_CAP_MAC = 150,	/* WL_EAP_MONITOR */
	IOV_ALLOW_MESH_FRM = 151, /* WL_EAP_ALLOW_MESH_FRM */
	IOV_NOBCNSSID = 152,	/* No SSID's in beacons */
	IOV_TXPWR_DEGRADE = 153,
	IOV_ALLOW_MGMT_FRM = 154, /* WL_EAP_ALLOW_MGMT_FRM */
	IOV_UCODE_EXT_CAPS = 155,
	IOV_RUNTIME_EN_EXT_CAPS = 156,
#ifdef SPLIT_ASSOC
	IOV_SPLIT_ASSOC_REQ = 157,	/* association split */
	IOV_SPLIT_ASSOC_RESP = 158,	/* association split */
#endif // endif
	IOV_ASSOC_DECISION = 159,	/* association decision */
	IOV_SKB_FREE_OFFLOAD = 160, /* Offload freeing of skbs to skb_free_task */
	IOV_MAXSCB = 161, /* max # of STAs allowed to join */
	IOV_ATM_STAPERC = 162,	/* atf percentage of fetch count in pcie layer */
	IOV_ATM_BSSPERC = 163,	/* atf percentage per BSS */
	IOV_ALLOW_SOUND_FB = 164,
	IOV_ACTIVE_BIDIR_THRESH = 165, /* rx active threshold */
	IOV_SEND_BAR_TO_IDLE_SCBS = 166,
	IOV_FIPS_LOOPBACK_TEST = 167,
	IOV_LAST		/* In case of a need to check max ID number */
};

static const bcm_iovar_t wlc_iovars[] = {
	{"rtsthresh", IOV_RTSTHRESH,
	(IOVF_WHL|IOVF_OPEN_ALLOW|IOVF_RSDB_SET), 0, IOVT_UINT16, 0
	},
	{"auth", IOV_D11_AUTH,
	IOVF_OPEN_ALLOW, IOVF2_RSDB_LINKED_CFG, IOVT_INT32, 0
	},
	{"nas_notify_event", IOV_NAS_NOTIFY_EVENT,
	IOVF_OPEN_ALLOW, 0, IOVT_BUFFER, sizeof(scb_val_t)
	},
	{"vlan_mode", IOV_VLAN_MODE,
	(IOVF_OPEN_ALLOW|IOVF_RSDB_SET), 0, IOVT_INT32, 0
	},
	{"wlfeatureflag", IOV_WLFEATUREFLAG,
	(IOVF_SET_DOWN|IOVF_RSDB_SET), 0, IOVT_INT32, 0
	},
	{"sta_info", IOV_STA_INFO,
	(IOVF_SET_UP|IOVF_OPEN_ALLOW), 0, IOVT_BUFFER, WL_OLD_STAINFO_SIZE
	},
	{"cap", IOV_CAP,
	(IOVF_OPEN_ALLOW), 0, IOVT_BUFFER, WLC_IOCTL_SMLEN
	},
	{"wpa_msgs", IOV_WPA_MSGS,
	(0), 0, IOVT_BOOL, 0
	},
	{"wpa_auth", IOV_WPA_AUTH,
	(IOVF_OPEN_ALLOW), IOVF2_RSDB_LINKED_CFG, IOVT_INT32, 0
	},
	{"malloced", IOV_MALLOCED,
	(IOVF_WHL), 0, IOVT_UINT32, 0
	},
	{"wsec_restrict", IOV_WSEC_RESTRICT,
	(IOVF_OPEN_ALLOW), IOVF2_RSDB_LINKED_CFG, IOVT_BOOL, 0
	},
#ifdef WET
	{"wet", IOV_WET,
	(IOVF_RSDB_SET), 0, IOVT_BOOL, 0
	},
#endif // endif
#ifdef WET_DONGLE
	{"wet_enab", IOV_WET_DONGLE,
	(IOVF_SET_DOWN), 0, IOVT_BOOL, 0
	},
#endif // endif
#ifdef MAC_SPOOF
	{"mac_spoof", IOV_MAC_SPOOF,
	(IOVF_RSDB_SET), 0, IOVT_BOOL, 0
	},
#endif /* MAC_SPOOF */
#if defined(BCMDBG) || defined(WLTEST)
	{"eirp", IOV_EIRP,
	(IOVF_MFG), 0, IOVT_UINT32, 0
	},
#endif /* defined(BCMDBG) || defined(WLTEST) */
	{"eap_restrict", IOV_EAP_RESTRICT,
	(0), IOVF2_RSDB_LINKED_CFG, IOVT_BOOL, 0
	},
	{"fragthresh", IOV_FRAGTHRESH,
	(IOVF_OPEN_ALLOW|IOVF_RSDB_SET), 0, IOVT_UINT16, 0
	},
	{"qtxpower", IOV_QTXPOWER,
	(IOVF_SET_UP|IOVF_WHL|IOVF_OPEN_ALLOW|IOVF_RSDB_SET), 0, IOVT_UINT32, 0
	},      /* constructed in wlu.c with txpwr or txpwr1 */
#if defined(WLP2P) || defined(WLPKTENG) || defined(WLTEST)
	{"cur_etheraddr", IOV_CUR_ETHERADDR,
	(0), 0, IOVT_BUFFER, ETHER_ADDR_LEN
	},
#else
	{"cur_etheraddr", IOV_CUR_ETHERADDR,
	(IOVF_SET_DOWN), 0, IOVT_BUFFER, ETHER_ADDR_LEN
	},
#endif // endif
	{"perm_etheraddr", IOV_PERM_ETHERADDR,
	(0), 0, IOVT_BUFFER, ETHER_ADDR_LEN
	},
#ifdef BCMDBG
	{"rand", IOV_RAND,
	(IOVF_GET_UP), 0, IOVT_UINT16, 0
	},
#endif // endif
	{"mpc", IOV_MPC,
	(IOVF_OPEN_ALLOW|IOVF_RSDB_SET), 0, IOVT_BOOL, 0
	},
#if defined(WLTEST)
	{"wd_disable", IOV_WATCHDOG_DISABLE,
	(IOVF_OPEN_ALLOW|IOVF_RSDB_SET|IOVF_MFG), 0, IOVT_BOOL, 0
	},
#endif // endif
#ifdef STA
	{"mpc_dur", IOV_MPC_DUR,
	(IOVF_RSDB_SET), IOVF2_RSDB_CORE_OVERRIDE, IOVT_UINT32, 0
	},
	{"apsta_dbg", IOV_APSTA_DBG,
	(0), 0, IOVT_UINT32, sizeof(uint32)
	},
#endif /* STA */
	{"bcmerrorstr", IOV_BCMERRORSTR,
	(0), 0, IOVT_BUFFER, BCME_STRLEN
	},
	{"bcmerror", IOV_BCMERROR,
	(0), 0, IOVT_INT8, 0
	},
	{"counters", IOV_COUNTERS,
	(IOVF_OPEN_ALLOW), IOVF2_RSDB_CORE_OVERRIDE, IOVT_BUFFER, sizeof(wl_cnt_info_t)
	},
	{"subcounters", IOV_SUBCOUNTERS,
	(IOVF_OPEN_ALLOW), IOVF2_RSDB_CORE_OVERRIDE, IOVT_BUFFER, OFFSETOF(wl_subcnt_info_t, data)
	},
	{"antsel_type", IOV_ANTSEL_TYPE,
	0, 0, IOVT_UINT8, 0
	},
	{"reset_d11cnts", IOV_RESET_D11CNTS,
	0, IOVF2_RSDB_CORE_OVERRIDE, IOVT_VOID, 0
	},
	{"ibss_allowed", IOV_IBSS_ALLOWED,
	(IOVF_OPEN_ALLOW), 0, IOVT_BOOL, 0
	},
	{"country_list_extended", IOV_COUNTRY_LIST_EXTENDED,
	(0), 0, IOVT_BOOL, 0
	},
	{"mcast_list", IOV_MCAST_LIST,
	(IOVF_OPEN_ALLOW), IOVF2_RSDB_LINKED_CFG, IOVT_BUFFER, sizeof(uint32)
	},
	{"chanspec", IOV_CHANSPEC,
	(IOVF_OPEN_ALLOW), IOVF2_RSDB_CORE_OVERRIDE, IOVT_UINT16, 0
	},
	{"chanspecs", IOV_CHANSPECS,
	(0), 0, IOVT_BUFFER, (sizeof(uint32))
	},
	/* Chanspecs with current driver settings */
	{"chanspecs_defset", IOV_CHANSPECS_DEFSET,
	(0), 0, IOVT_BUFFER, (sizeof(uint32)*(WL_NUMCHANSPECS+1))
	},
#ifdef WLATF
	{"atf", IOV_ATF, IOVF_RSDB_SET, 0, IOVT_UINT32, 0 },
#endif // endif
#if defined(WLTEST)
	{"boardflags", IOV_BOARDFLAGS,
	(IOVF_SET_DOWN | IOVF_MFG), 0, IOVT_UINT32, 0
	},
	{"boardflags2", IOV_BOARDFLAGS2,
	(IOVF_SET_DOWN | IOVF_MFG), 0, IOVT_UINT32, 0
	},
	{"antgain", IOV_ANTGAIN,
	(IOVF_SET_DOWN | IOVF_MFG), 0, IOVT_BUFFER, 0
	},
#endif // endif
	{"lifetime", IOV_LIFETIME,
	(0), 0, IOVT_BUFFER, sizeof(wl_lifetime_t)
	},
	{"lifetime_mg", IOV_LIFETIME_MG,
	(0), 0, IOVT_BUFFER, sizeof(wl_lifetime_mg_t)
	},
	{"ver", IOV_VER,
	(IOVF_OPEN_ALLOW), 0, IOVT_BUFFER, 0
	},
	{"antennas", IOV_ANTENNAS,
	(0), 0, IOVT_UINT8, 0
	},
#ifdef NLO
	{"nlo", IOV_NLO,
	(IOVF_BSSCFG_STA_ONLY), 0, IOVT_BOOL, 0
	},
#endif /* NLO */
	{"msglevel", IOV_MSGLEVEL2, 0, 0, IOVT_BUFFER, sizeof(struct wl_msglevel2)},
	{"infra_configuration", IOV_INFRA_CONFIGURATION, IOVF_OPEN_ALLOW, 0, IOVT_UINT32, 0},
	{"down_override", IOV_DOWN_OVERRIDE,
	(IOVF_OPEN_ALLOW), 0, IOVT_BOOL, 0
	},
	{"allmulti", IOV_ALLMULTI,
	(IOVF_OPEN_ALLOW), IOVF2_RSDB_LINKED_CFG, IOVT_BOOL, 0
	},
	{"legacy_probe", IOV_LEGACY_PROBE,
	(IOVF_OPEN_ALLOW), 0, IOVT_BOOL, 0
	},
	{"bandunit", IOV_BANDUNIT,
	(0), 0, IOVT_UINT32, 0
	},
#ifdef BCM_BLOG
	{"fcache", IOV_FCACHE,
	(0), 0, IOVT_UINT8, 0
	},
#endif /* BCM_BLOG */
#ifdef STA
	{"nolinkup", IOV_NOLINKUP,
	(0), 0, IOVT_BOOL, 0
	},
#endif /* STA */
#if defined(DELTASTATS)
	{"delta_stats_interval", IOV_DELTA_STATS_INTERVAL,
	(0), IOVF2_RSDB_CORE_OVERRIDE, IOVT_INT32, 0
	},
	{"delta_stats", IOV_DELTA_STATS,
	(0), IOVF2_RSDB_CORE_OVERRIDE, IOVT_BUFFER, sizeof(wl_delta_stats_t)
	},
#endif // endif
	{"nav_reset_war_disable", IOV_NAV_RESET_WAR_DISABLE,
	0, 0, IOVT_BOOL, 0
	},
	{"rfaware_lifetime", IOV_RFAWARE_LIFETIME,
	(IOVF_RSDB_SET|IOVF_RSDB_SET), 0, IOVT_UINT16, 0
	},
	{"assert_type", IOV_ASSERT_TYPE,
	(0), 0, IOVT_UINT32, 0,
	},
#ifdef PHYCAL_CACHING
	{"phycal_caching", IOV_PHYCAL_CACHE,
	0, 0, IOVT_BOOL, 0
	},
	{"cachedcal_scan", IOV_CACHE_CHANSWEEP,
	0, 0, IOVT_UINT16, 0
	},
#endif /* PHYCAL_CACHING */
#ifdef STA
	{"wake_event", IOV_WAKE_EVENT,
	(0), 0, IOVT_UINT16, 0
	},
#endif /* STA */
	{"pool", IOV_POOL,
	(0), 0, IOVT_UINT8, 0
	},
	{"cur_rateset", IOV_CURR_RATESET,
	(0), 0, IOVT_BUFFER, sizeof(wl_rateset_args_t)
	},
	{"rateset", IOV_RATESET,
	(IOVF_SET_DOWN), 0, IOVT_BUFFER, 0
	},
	{"dcs_req", IOV_BRCM_DCS_REQ,
	(IOVF_SET_UP), 0, IOVT_UINT16, 0
	},
	{"auth_ops", IOV_AUTHOPS,
	(IOVF_SET_UP), 0, IOVT_BUFFER, 0
	},
#ifdef DNGL_WD_KEEP_ALIVE
	{"dngl_wd", IOV_DNGL_WD_KEEP_ALIVE,
	(IOVF_SET_UP|IOVF_RSDB_SET), 0, IOVT_BOOL, 0
	},
	{"dngl_wdtrigger", IOV_DNGL_WD_FORCE_TRIGGER,
	(0), 0, IOVT_UINT32, 0
	},
#endif // endif
	{"is_WPS_enrollee", IOV_IS_WPS_ENROLLEE,
	(0), 0, IOVT_BOOL, 0
	},
#ifdef BCMDBG
	{"tsf", IOV_TSF,
	(IOVF_SET_UP | IOVF_GET_UP), 0, IOVT_UINT32, 0
	},
	{"tsf_adj", IOV_TSF_ADJUST,
	(IOVF_SET_UP), 0, IOVT_UINT32, 0
	},
#endif /* BCMDBG */
	{"bsscfg_idx", IOV_WLIF_BSSCFG_IDX,
	(0), 0, IOVT_UINT8, 0
	},
	{"ssid", IOV_SSID,
	(0), 0, IOVT_INT32, 0
	},
	{"rpt_hitxrate", IOV_RPT_HITXRATE,
	(0), 0, IOVT_INT32, 0
	},
#ifdef WLRXOV
	{"rxov", IOV_RXOV,
	(0), 0, IOVT_UINT32, 0
	},
#endif // endif
#ifdef BCMDBG
#ifdef STA
	{"rpmt", IOV_RPMT, IOVF_BSSCFG_STA_ONLY, 0, IOVT_BUFFER, 8},
#endif // endif
#endif /* BCMDBG */
#ifdef BPRESET
	{"bpreset_enab", IOV_BPRESET_ENAB,
	(0), 0, IOVT_UINT32, 0},
#ifdef BCMDBG
	{"bpreset", IOV_BACKPLANE_RESET,
	(0), 0, IOVT_UINT32, 0},
#endif /* BCMDBG */
#endif /* BPRESET */
	{"ucdload_status", IOV_BMAC_UCDLOAD_STATUS,
	(0), 0, IOVT_INT32, 0
	},
	{"ucdload_chunk_len", IOV_BMAC_UC_CHUNK_LEN,
	(0), 0, IOVT_INT32, 0
	},
	{"mempool", IOV_MEMPOOL,
	0, 0, IOVT_BUFFER, sizeof(wl_mempool_stats_t)
	},
#ifdef BCMOVLHW
	{"overlay", IOV_OVERLAY,
	(0), 0, IOVT_BUFFER, 0
	},
#endif // endif
	{"clmver", IOV_CLMVER,
	(0), 0, IOVT_BUFFER, MAX_CLMVER_STRLEN
	},
	{"fabid", IOV_FABID,
	(0), 0, IOVT_UINT16, 0
	},
	{"brcm_ie", IOV_BRCM_IE,
	(IOVF_RSDB_SET), 0, IOVT_BOOL, 0
	},

	{"sar_enable", IOV_SAR_ENABLE,
	IOVF_RSDB_SET, 0, IOVT_BOOL, 0
	},
#ifdef WL_SARLIMIT
	{"sar_limit", IOV_SAR_LIMIT,
	IOVF_RSDB_SET, 0, IOVT_BUFFER, (sizeof(sar_limit_t))
	},
#endif /* WL_SARLIMIT */
#ifdef WLCNT
	{"reset_cnts", IOV_RESET_CNTS,
	0, IOVF2_RSDB_CORE_OVERRIDE, IOVT_UINT32, 0
	},
#endif /* WLCNT */
	{"antdiv_bcnloss", IOV_ANTDIV_BCNLOSS,
	(0), 0, IOVT_UINT16, 0
	},
#ifdef EVENT_LOG_COMPILE
	{"event_log_set_init", IOV_EVENT_LOG_SET_INIT,
	(0), 0, IOVT_BUFFER, sizeof(wl_el_set_params_t)
	},
	{"event_log_set_expand", IOV_EVENT_LOG_SET_EXPAND,
	(0), 0, IOVT_BUFFER, sizeof(wl_el_set_params_t)
	},
	{"event_log_set_shrink", IOV_EVENT_LOG_SET_SHRINK,
	(0), 0, IOVT_BUFFER, sizeof(wl_el_set_params_t)
	},
	{"event_log_tag_control", IOV_EVENT_LOG_TAG_CONTROL,
	(0), 0, IOVT_BUFFER, sizeof(wl_el_tag_params_t)
	},
	{"event_log_get", IOV_EVENT_LOG_GET,
	(0), 0, IOVT_BUFFER, sizeof(wl_el_tag_params_t)
	},
#endif /* EVENT_LOG_COMPILE */
	/* it is required for regulatory testing */
	{"per_chan_info", IOV_PER_CHAN_INFO,
	(0), 0, IOVT_BUFFER, sizeof(wl_set_chan_info_t)
	},
#ifdef STA
	{"early_bcn_thresh", IOV_EARLY_BCN_THRESH,
	(IOVF_RSDB_SET), 0, IOVT_UINT32, 0
	},
#endif /* STA */
	{"bss_peer_info", IOV_BSS_PEER_INFO,
	(0), 0, IOVT_BUFFER,
	(BSS_PEER_LIST_INFO_FIXED_LEN + sizeof(bss_peer_info_t))
	},
#ifdef ATE_BUILD
	{"gpaio", IOV_GPAIO,
	IOVF_SET_UP | IOVF_MFG, 0, IOVT_UINT32, 0
	},
#endif // endif
#if defined(WLRSDB)
	{"ioc", IOV_IOC, 0, IOVF2_RSDB_CORE_OVERRIDE, IOVT_BUFFER, sizeof(int),
	},
#endif /* WLRSDB */
#ifdef WLPM_BCNRX
	{"pm_bcnrx", IOV_PM_BCNRX,
	(IOVF_SET_UP), 0, IOVT_BOOL, 0
	},
#endif // endif
	{"tcpack_fast_tx", IOV_TCPACK_FAST_TX, (0), 0, IOVT_BOOL, 0},
	{"wd_on_bcn", IOV_WD_ON_BCN,
	(0), 0, IOVT_BOOL, 0
	},
#if !defined(NDIS) && (!defined(WLC_HOSTOID) || defined(HOSTOIDS_DISABLED))
	{ "if_counters", IOV_IF_COUNTERS,
	(IOVF_OPEN_ALLOW), 0, IOVT_BUFFER, sizeof(wl_if_stats_t)
	},
#endif /* !NDIS && (!WLC_HOSTOID || HOSTOIDS_DISABLED) */
	{"rxfifo_counters", IOV_RXFIFO_COUNTERS,
	(IOVF_OPEN_ALLOW), 0, IOVT_BUFFER, sizeof(wl_rxfifo_cnt_t)},
	{"lifetime_ctrl", IOV_LIFETIME_CTRL, (0), 0, IOVT_UINT32, 0},
#if defined(WL_DATAPATH_LOG_DUMP) && defined(BCMDBG)
	{"dp_dump", IOV_DP_DUMP,
	(0), IOVF2_RSDB_CORE_OVERRIDE, IOVT_BUFFER, 0},
#endif // endif
#if defined(WL_DD_HANDLER)
	{"hc", IOV_HC, (IOVF_RSDB_SET), 0, IOVT_BUFFER, BCM_XTLV_HDR_SIZE},
#endif /* WL_DD_HANDLER */
	{"beacon_info", IOV_BEACON_INFO,
	(IOVF_OPEN_ALLOW), 0, IOVT_BUFFER, sizeof(uint32),
	},
	{ "wlc_ver", IOV_WLC_VER,
	(0), 0, IOVT_BUFFER, sizeof(wl_wlc_version_t)
	},
	{"dyn160", IOV_DYN160,
	(0), 0, IOVT_UINT32, sizeof(uint32)
	},
	{"sta_report", IOV_STA_REPORT,
	(0), 0, IOVT_BUFFER, sizeof(sta_report_t)
	},
#ifdef AP
	{"sta_supp_chan", IOV_STA_SUPP_CHAN,
	(0), 0, IOVT_BUFFER, sizeof(wlc_supp_chan_list_t)
	},
#endif /* AP */
	{"nobcnssid", IOV_NOBCNSSID,
	(0), 0, IOVT_BOOL, 0
	},
	{"txpwr_degrade", IOV_TXPWR_DEGRADE,
	(IOVF_OPEN_ALLOW), 0, IOVT_UINT8, 0
	},
#ifdef SPLIT_ASSOC
	{"split_assoc_req", IOV_SPLIT_ASSOC_REQ,
	(IOVF_SET_DOWN), 0, IOVT_UINT32, 0
	},
	{"split_assoc_resp", IOV_SPLIT_ASSOC_RESP,
	(IOVF_SET_DOWN), 0, IOVT_UINT32, 0
	},
#endif // endif
	{"assoc_decision", IOV_ASSOC_DECISION,
	(0), 0, IOVT_BUFFER, sizeof(assoc_decision_t)
	},
#ifdef BCM_SKB_FREE_OFFLOAD
	{"skb_free_offload", IOV_SKB_FREE_OFFLOAD,
	(0), 0, IOVT_BOOL, 0
	},
#endif // endif
	{"maxscb", IOV_MAXSCB,
	(0), 0, IOVT_UINT16, 0
	},
#if defined(WLATF) && defined(WLATF_PERC)
	{"atm_staperc", IOV_ATM_STAPERC,
	(0), 0, IOVT_BUFFER, sizeof(wl_atm_staperc_t)
	},
	{"atm_bssperc", IOV_ATM_BSSPERC,
	(0), 0, IOVT_UINT8, 0
	},
#endif /* WLATF && WLATF_PERC */
	{"active_bidir_thresh", IOV_ACTIVE_BIDIR_THRESH,
	(0), 0, IOVT_UINT32, 0
	},
	{NULL, 0, 0, 0, 0, 0}
};

/** Mapping from 802.1d priority to FIFO */
const uint8 prio2fifo[NUMPRIO] = {
	TX_AC_BE_FIFO,	/* 0	BE	AC_BE	Best Effort */
	TX_AC_BK_FIFO,	/* 1	BK	AC_BK	Background */
	TX_AC_BK_FIFO,	/* 2	--	AC_BK	Background */
	TX_AC_BE_FIFO,	/* 3	EE	AC_BE	Best Effort */
	TX_AC_VI_FIFO,	/* 4	CL	AC_VI	Video */
	TX_AC_VI_FIFO,	/* 5	VI	AC_VI	Video */
	TX_AC_VO_FIFO,	/* 6	VO	AC_VO	Voice */
	TX_AC_VO_FIFO	/* 7	NC	AC_VO	Voice */
};

/** Mapping from FIFO to 802.1d priority */
const uint8 fifo2prio[NFIFO_LEGACY] = {
	PRIO_8021D_BK,	/* 0	AC_BK	BK	Background */
	PRIO_8021D_BE,	/* 1	AC_BE	BE	Best Effort */
	PRIO_8021D_VI,	/* 2	AC_VI	VI	Video */
	PRIO_8021D_VO,	/* 3	AC_VO	VO	Voice */
	PRIO_8021D_BE,	/* 4	BCMC	BE	Best Effort */
	PRIO_8021D_BE	/* 5	ATIM	BE	Best Effort */
};

/** Mapping from 802.1d priority to AC */
const uint8 prio2ac[NUMPRIO] = {
	AC_BE,  /* 0    BE      AC_BE   Best Effort */
	AC_BK,  /* 1    BK      AC_BK   Background */
	AC_BK,  /* 2    --      AC_BK   Background */
	AC_BE,  /* 3    EE      AC_BE   Best Effort */
	AC_VI,  /* 4    CL      AC_VI   Video */
	AC_VI,  /* 5    VI      AC_VI   Video */
	AC_VO,  /* 6    VO      AC_VO   Voice */
	AC_VO	/* 7    NC      AC_VO   Voice */
};

/** 802.1D Priority to precedence queue mapping */
const uint8 wlc_prio2prec_map[] = {
	_WLC_PREC_BE,		/* 0 BE - Best-effort */
	_WLC_PREC_BK,		/* 1 BK - Background */
	_WLC_PREC_NONE,		/* 2 None = - */
	_WLC_PREC_EE,		/* 3 EE - Excellent-effort */
	_WLC_PREC_CL,		/* 4 CL - Controlled Load */
	_WLC_PREC_VI,		/* 5 Vi - Video */
	_WLC_PREC_VO,		/* 6 Vo - Voice */
	_WLC_PREC_NC,		/* 7 NC - Network Control */
};

/** 802.1D Precedence to priority queue mapping */
const uint8 wlc_prec2prio_map[] = {
	_WLC_PREC_BK/2,		/* 0 BK - Background */
	_WLC_PREC_BE/2,		/* 1 BE - Best effort */
	_WLC_PREC_NONE/2,	/* 2 None = - */
	_WLC_PREC_EE/2,		/* 3 EE - Excellent-effort */
	_WLC_PREC_CL/2,		/* 4 CL - Controlled Load */
	_WLC_PREC_VI/2,		/* 5 Vi - Video */
	_WLC_PREC_VO/2,		/* 6 Vo - Voice */
	_WLC_PREC_NC/2,		/* 7 NC - Network Control */
};

#define WLC_DEFAULT_MAX_DATA_PKT_LIFETIME	2000 /* 2 Seconds */
#define WLC_DEFAULT_MAX_MGMT_PKT_LIFETIME	1000 /* 1 Seconds */
#define WLC_DEFAULT_CTRL_PKT_LIFETIME		1000 /* 1 Seconds */
#define WLC_MGMT_PROBE_REQ_LIFETIME		350  /* 350ms considering p2p max discovery time */

/* conversion between auth values set externally and 802.11 auth type values */
#define DOT11AUTH2WLAUTH(bsscfg) (bsscfg->openshared ? WL_AUTH_OPEN_SHARED :\
	(bsscfg->auth == DOT11_OPEN_SYSTEM ? WL_AUTH_OPEN_SYSTEM : WL_AUTH_SHARED_KEY))
#define WLAUTH2DOT11AUTH(val) (val == WL_AUTH_OPEN_SYSTEM ? DOT11_OPEN_SYSTEM : DOT11_SHARED_KEY)

/* local prototypes */
static void wlc_iov_get_wlc_ver(wl_wlc_version_t *ver);

static void wlc_monitor_down(wlc_info_t *wlc);
static void wlc_bss_default_init(wlc_info_t *wlc);
static void wlc_ucode_mac_upd(wlc_info_t *wlc);
static int  wlc_xmtfifo_sz_get(wlc_info_t *wlc, uint fifo, uint *blocks);
static void wlc_xmtfifo_sz_upd_high(wlc_info_t *wlc, uint fifo, uint16 blocks);

static void wlc_tx_prec_map_init(wlc_info_t *wlc);
static char *wlc_cap(wlc_info_t *wlc, char *buf, uint bufsize);
static void wlc_cap_bcmstrbuf(wlc_info_t *wlc, struct bcmstrbuf *b);
static void wlc_pdu_push_txparams(wlc_info_t *wlc, void *p, uint32 flags, wlc_key_t *key,
	ratespec_t rate_override);

#ifndef ATE_BUILD
static void wlc_watchdog_timer(void *arg);
#ifdef STA
static void wlc_watchdog_indicate_maccore_state_timer(void *arg);
#endif /* STA */
#endif /* !ATE_BUILD */
static void wlc_refresh_wd_timer(wlc_info_t *wlc, uint32 timeout, bool periodic);
static int wlc_set_rateset(wlc_info_t *wlc, wlc_rateset_t *rs_arg);

static void wlc_mfbr_retrylimit_upd(wlc_info_t *wlc);

#if defined(BCMDBG)
static int wlc_dump_mempool(void *arg, struct bcmstrbuf *b);
#endif // endif

#ifdef BCMDBG
static void wlc_tsf_set(wlc_info_t *wlc, uint32 tsf_l, uint32 tsf_h);
#endif // endif

#if defined(BCMDBG) || defined(BCMDBG_PHYDUMP) || defined(WLTEST) || \
	defined(TDLS_TESTBED) || defined(BCMDBG_AMPDU) || defined(MCHAN_MINIDUMP) || \
	defined(BCM_DNGDMP) || defined(BCMDBG_PHYDUMP)
#ifdef WLTINYDUMP
static int wlc_tinydump(wlc_info_t *wlc, struct bcmstrbuf *b);
#endif /* WLTINYDUMP */
#endif // endif

#if defined(BCMDBG)
static int wlc_dump_bcmlog(wlc_info_t *wlc, struct bcmstrbuf *b);
static int wlc_bssinfo_dump(wlc_info_t *wlc, struct bcmstrbuf *b);
static int wlc_dump_wlc(wlc_info_t *wlc, struct bcmstrbuf *b);
static int wlc_dump_ratestuff(wlc_info_t *wlc, struct bcmstrbuf *b);
static int wlc_dump_bss_info(const char *name, wlc_bss_info_t *bi, struct bcmstrbuf *b);
#if defined(WLVASIP)
static int wlc_dump_vasip(wlc_info_t *wlc, struct bcmstrbuf *b);
#endif /* WLVASIP */
#endif // endif

#if defined(BCMDBG_TXSTUCK)
static void wlc_dump_txstuck(wlc_info_t *wlc, struct bcmstrbuf *b);
#endif /* defined(BCMDBG_TXSTUCK) */

#if defined(WL_EVENT_LOG_COMPILE)
static void wlc_log_event(wlc_info_t* wlc, wlc_event_t *e);
#endif // endif

/* send and receive */
static uint16 wlc_compute_airtime(wlc_info_t *wlc, ratespec_t rspec, uint length);
static void wlc_compute_cck_plcp(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	ratespec_t rate, uint length, uint16 fc, uint8 *plcp);
static void wlc_compute_ofdm_plcp(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	ratespec_t rate, uint length, uint16 fc, uint8 *plcp);
static void wlc_compute_mimo_plcp(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	ratespec_t rate, uint length, uint16 fc, uint8 *plcp);
static uint32 wlc_vht_siga1_get_partial_aid(void);
static vht_group_id_t wlc_compute_vht_groupid(uint16 fcs);

static void wlc_compute_vht_plcp(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	ratespec_t rate, uint length, uint16 fc, uint8 *plcp);
#ifdef WL11AX
static void wlc_compute_he_plcp(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	ratespec_t rspec, uint length, uint16 fc, uint8 *plcp);
#endif /* WL11AX */
static uint wlc_calc_ack_time(wlc_info_t *wlc, ratespec_t rate, uint8 preamble_type);

/* interrupt, up/down, band */
static void wlc_setband(wlc_info_t *wlc, uint bandunit);
static chanspec_t wlc_init_chanspec(wlc_info_t *wlc);

static void wlc_bsinit(wlc_info_t *wlc);

static void wlc_radio_hwdisable_upd(wlc_info_t* wlc);
static bool wlc_radio_monitor_start(wlc_info_t *wlc);
#ifndef ATE_BUILD
static void wlc_radio_timer(void *arg);
#endif /* !ATE_BUILD */
static bool wlc_radio_enable(wlc_info_t *wlc);

static int wlc_module_find(wlc_info_t *wlc, const char *name);

static uint8 wlc_get_antennas(wlc_info_t *wlc);

static void wlc_nav_reset_war(wlc_info_t *wlc, bool enable);

static uint16 wlc_bss_scb_getcnt(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	bool (*scb_test_cb)(struct scb *scb));

static void wlc_sta_tbtt(wlc_info_t *wlc);
static void wlc_ap_tbtt(wlc_info_t *wlc);

static void _wlc_bss_update_beacon(wlc_info_t *wlc, wlc_bsscfg_t *cfg);

/* ** STA-only routines ** */
#ifdef STA
static void wlc_sendpspoll_complete(wlc_info_t *wlc, void *pkt, uint txstatus);

static void wlc_radio_shutoff_dly_timer_upd(wlc_info_t *wlc);
#ifndef ATE_BUILD
static void wlc_pm2_radio_shutoff_dly_timer(void *arg);
#endif /* ATE_BUILD */
static void wlc_bss_pm_pending_upd(wlc_bsscfg_t *cfg, uint txstatus);

static bool wlc_sendpmnotif(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	struct ether_addr *da, ratespec_t rate_override, int prio, bool track);
#ifndef ATE_BUILD
static void wlc_wake_event_timer(void *arg);
#endif /* !ATE_BUILD */
/* PM2 tick timer functions.
 * Some are inline because they are used in the time-critical tx path
 */

static void _wlc_set_wake_ctrl(wlc_info_t *wlc);
static void wlc_rateprobe_complete(wlc_info_t *wlc, void *pkt, uint txs);
static void wlc_rateprobe_scan(wlc_bsscfg_t *cfg);
#ifdef WLWSEC
static void wlc_tkip_countermeasures(wlc_info_t *wlc, void *pkt, uint txs);
#else
#define wlc_tkip_countermeasures NULL
#endif /* WLWSEC */
static void wlc_pm_notif_complete(wlc_info_t *wlc, void *pkt, uint txs);

static void *wlc_frame_get_ps_ctl(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
                                  const struct ether_addr *bssid,
                                  const struct ether_addr *da);

static uint8 wlc_bss_wdsscb_getcnt(wlc_info_t *wlc, wlc_bsscfg_t *cfg);

#ifdef PHYCAL_CACHING
static int wlc_cache_cals(wlc_info_t *wlc);
static int wlc_cachedcal_sweep(wlc_info_t *wlc);
static int wlc_cachedcal_tune(wlc_info_t *wlc, uint16 chanspec);
#endif /* PHYCAL_CACHING */
#endif	/* STA */

#if defined(DELTASTATS)
static void wlc_delta_stats_update(wlc_info_t *wlc);
static int wlc_get_delta_stats(wlc_info_t *wlc, wl_delta_stats_t *stats);
#endif // endif

static bool wlc_attach_stf_ant_init(wlc_info_t *wlc);

static void wlc_down_led_upd(wlc_info_t *wlc);
static uint wlc_down_del_timer(wlc_info_t *wlc);

static int wlc_doioctl(void *ctx, uint cmd, void *arg, uint len, struct wlc_if *wlcif);
static int wlc_doiovar(void *hdl, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, wlc_if_t *wlcif);

#if defined(BCM_DCS) && defined(STA)
static int wlc_send_action_brcm_dcs(wlc_info_t *wlc, wl_bcmdcs_data_t *data, struct scb *scb);
#endif // endif

static int wlc_send_mgmt_auth(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
		dot11_management_header_t *mgmt, uint32 len);
static int wlc_send_mgmt_assoc(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
		dot11_management_header_t *mgmt, uint32 len);
#ifdef WL_CLIENT_SAE
static int wlc_sae_assoc_change_state(wlc_bsscfg_t *cfg, struct dot11_auth *auth_frm);
#endif /* WL_CLIENT_SAE */
static void wlc_read_rt_dirmap(wlc_info_t *wlc);

static void wlc_config_ucode_probe_resp(wlc_info_t *wlc);

#ifndef WLC_DISABLE_ACI
static void wlc_weakest_link_rssi_chan_stats_upd(wlc_info_t *wlc);
#endif // endif

static uint32 wlc_get_chan_info(wlc_info_t *wlc, uint16 old_chanspec);

static int8 wlc_antgain_calc(int8 ag);

static void wlc_copy_peer_info(wlc_bsscfg_t *bsscfg, struct scb *scb, bss_peer_info_t *peer_info);

#ifdef STA
static void wlc_nulldp_tpl_upd(wlc_bsscfg_t *cfg);
#endif /* STA */

static uint32 _wlc_get_accum_pmdur(wlc_info_t *wlc);
static int wlc_get_revision_info(wlc_info_t *wlc, void *buf, uint len);

static void wlc_chansw_notif(
	wlc_info_t *wlc, int reason_bitmap, chanspec_t old, chanspec_t new, uint32 tsf_l);
static void wlc_chansw_notif_signal(
	wlc_info_t *wlc, int reason, chanspec_t old, chanspec_t new, uint32 tsf_l);

/*
 * _EVENT_LOG macro expects the string and does not handle a pointer to the string.
 *  Hence MACRO is used to have identical string thereby compiler can optimize.
 */
#define WLC_BSS_INFO_LEN_ERR		"%s: to_bi_len (%d) too small i.e. < %d\n"

#if defined(EVENT_LOG_COMPILE) && defined(WLCNT)
static void wlc_ctl_mgt_frame_counter_report(void *arg, wlc_chansw_notif_data_t *data);
#endif // endif

static void wlc_lifetime_cache_upd(wlc_info_t *wlc);
static void wlc_do_down(wlc_info_t *wlc);
#if defined(ECOUNTERS)
#if defined(WLCNT)
static int wlc_ecounters_wl_counters(uint16 stats_type, void *context,
	const ecounters_stats_types_report_req_t *req,
	struct bcm_xtlvbuf *xtlvbuf, uint32 *cookie, const bcm_xtlv_t* tlv, uint16 *awl);
#endif /* WLCNT */
#endif /* ECOUNTERS */

static int wlc_do_up(wlc_info_t *wlc);

/* Health Check IOVAR support */
#if defined(WL_DD_HANDLER)

static int wlc_hc_iovar_handler(wlc_info_t *wlc, struct wlc_if *wlcif, bool isset,
	void *params, uint p_len, void *out, uint o_len);
#endif /* WL_DD_HANDLER */

#if defined(WL_RX_HANDLER)
static int wlc_hc_rx_set(void *ctx, const uint8 *buf, uint16 type, uint16 len);
static int wlc_hc_rx_get(wlc_info_t *wlc, wlc_if_t *wlcif,
        bcm_xtlv_t *params, void *out, uint o_len);
#endif /* WL_RX_HANDLER */

#ifdef BCMDBG
#ifdef STA
static void wlc_rpmt_timer_cb(void *arg);
#endif // endif
#endif /* BCMDBG */

#ifdef WLCNTSCB
static void
wlc_sta_info_upd_scb_stats(const wlc_info_t *wlc, const wlc_bsscfg_t *bsscfg,
	const wlc_scb_stats_t *scb_stats, sta_info_t *sta);
#endif /* WLCNTSCB */
static void wlc_update_current_bi_caps(wlc_info_t *wlc, wl_bss_info_t *bi);

#if defined(STA) && defined(AP)
static void wlc_apsta_restart_ap(wlc_info_t* wlc);
#endif /* STA && AP */

#if defined(WLATF) && defined(WLATF_PERC)
static void wlc_atm_update_perc(wlc_info_t *wlc);
static uint8 wlc_atm_cal_perc(wlc_info_t * wlc, uint8 perc_sum, uint8 num,
	uint8 auto_num, uint8 perc);
#endif /* WLATF && WLATF_PERC */

#ifdef BCMDBG_CTRACE
static int
wlc_pkt_ctrace_dump(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	PKT_CTRACE_DUMP(wlc->osh, b);

	return 0;
}
#endif /* BCMDBG_CTRACE */

#if defined(STA) && defined(AP)
static void
wlc_apsta_restart_ap(wlc_info_t *wlc)
{
	wlc_bsscfg_t *cfg;
	bool startap = FALSE;
	int i = 0;

	FOREACH_AP(wlc, i, cfg) {
		/* find the first ap that is enabled but not up */
		if (cfg->enable && !cfg->up) {
			startap = TRUE;
			break;
		}
	}

	if (startap) {
		WL_APSTA_UPDN(("wl%d: wlc_watchdog -> restart downed ap\n",
		       wlc->pub->unit));
		wlc_restart_ap(wlc->ap);
	}
}
#endif /* STA & AP */

/** bring the driver down, but don't reset hardware */
static void wlc_out(wlc_info_t *wlc);

/**
 * returns channel info (20 MHZ channels only)
 * This used to pass in a ptr to a uint16 which was very similar to the new chanspec except
 * it did not have b/w or sideband info. Given that this routine has already been exposed via an
 * iovar then ignore the new extra info in the chanspec passed in and recreate a chanspec from
 * just the channel number this routine is used to figure out the number of scan channels, so
 * only report the 20MHZ channels.
 */
static uint32
wlc_get_chan_info(wlc_info_t *wlc, uint16 old_chanspec)
{
	uint32 result;
	uint32 dfs_chan_info;
	uint channel = CHSPEC_CHANNEL(old_chanspec);
	chanspec_t chspec = CH20MHZ_CHSPEC(channel);

	result = 0;
	/* XXX WES: This loop assumes all devices are 2.4 GHz and only dual band
	 * devices support 5 GHz. That is, 5 GHz only devices not supported.
	 */
	if (channel && channel < MAXCHANNEL) {
		if ((channel <= CH_MAX_2G_CHANNEL) && isset(chanvec_all_2G.vec, channel))
			result |= WL_CHAN_VALID_HW;
		else if ((channel > CH_MAX_2G_CHANNEL) && isset(chanvec_all_5G.vec, channel))
			result |= WL_CHAN_VALID_HW | WL_CHAN_BAND_5G;
	}

	if (result & WL_CHAN_VALID_HW) {
		if (wlc_valid_chanspec_db(wlc->cmi, chspec))
			result |= WL_CHAN_VALID_SW;

		if (WLDFS_ENAB(wlc->pub) && (wlc->dfs != NULL)) {
			dfs_chan_info = wlc_dfs_get_chan_info(wlc->dfs, channel);
			result |= dfs_chan_info;
		}

		if (result & WL_CHAN_VALID_SW) {
			if (wlc_radar_chanspec(wlc->cmi, chspec) == TRUE)
				result |= WL_CHAN_RADAR;
			if (wlc_restricted_chanspec(wlc->cmi, chspec))
				result |= WL_CHAN_RESTRICTED;
			if (wlc_quiet_chanspec(wlc->cmi, chspec))
				result |= WL_CHAN_PASSIVE;
			if (wlc_channel_clm_restricted_chanspec(wlc->cmi, chspec))
				result |= WL_CHAN_CLM_RESTRICTED;
#ifdef RADAR
			if (wlc_is_european_weather_radar_channel(wlc, chspec) == TRUE) {
				result |= WL_CHAN_RADAR_EU_WEATHER;
			}
#endif /* RADAR */
		}
	}

	return (result);
}

#if defined(BCMDBG)
static int
wlc_dump_mempool(void *arg, struct bcmstrbuf *b)
{
	wlc_info_t *wlc = (wlc_info_t *) arg;
	int ret;

	ret = (bcm_mpm_dump(wlc->mem_pool_mgr, b));

#if defined(STS_FIFO_RXEN) || defined(WLC_OFFLOADS_RXSTS)
	bcm_bprintf(b, "\nPHYRXSTS  #Curr=%d #Fail=%d HiWtr=%d\n",
	            wlc->hw->sts_mempool.n_items, wlc->hw->sts_mempool.n_fail,
	            wlc->hw->sts_mempool.high_watermark);
#endif /* STS_FIFO_RXEN || WLC_OFFLOADS_RXSTS */

	return ret;
}
#endif // endif

#if defined(BCMDBG)
static int
wlc_bssinfo_dump(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	int idx;
	wlc_bsscfg_t *cfg;

	bcm_bprintf(b, "\n");
	wlc_dump_bss_info("default_bss", wlc->default_bss, b);
	bcm_bprintf(b, "\n");
	FOREACH_BSS(wlc, idx, cfg) {
		bcm_bprintf(b, "bsscfg %d (0x%p):\n", idx, OSL_OBFUSCATE_BUF(cfg));
		bcm_bprintf(b, "\n");
		wlc_dump_bss_info("target_bss", cfg->target_bss, b);
		bcm_bprintf(b, "\n");
		wlc_dump_bss_info("current_bss", cfg->current_bss, b);
	}
	return 0;
}

static int
wlc_dump_bcmlog(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	bcmdumplog(b->buf, b->size);
	return BCME_OK;
}
#endif // endif

bool
wlc_rminprog(wlc_info_t *wlc)
{
	BCM_REFERENCE(wlc);

	return WLC_RM_IN_PROGRESS(wlc);
}

#ifdef STA

uint32
wlc_get_mpc_dur(wlc_info_t *wlc)
{
	/* hw_up=1 when out of MPC & hw_up=0 when in MPC.  wlc->mpc_dur is updated when coming
	 * out of MPC and wlc->mpc_laston_ts is updated when going into MPC. In MPC mode, the sum of
	 * last mpc_dur and time since we have been in MPC currently gives the total mpc_dur.
	 */
	if ((wlc->mpc == 0) || wlc->pub->hw_up)
		return wlc->mpc_dur;
	return wlc->mpc_dur + (OSL_SYSUPTIME() - wlc->mpc_laston_ts);
}

#ifdef DBG_PRINT_WAKE_REASON
#define	DBG_PRINT_WAKE_RSN(rsn) do { if (rsn) {printf("Wake reason: " #rsn "\n");}} while (0);
#else
#define DBG_PRINT_WAKE_RSN(rsn)
#endif // endif

/** returns TRUE when the chip has to be kept awake, eg when a scan is in progress */
bool
wlc_stay_awake(wlc_info_t *wlc)
{
	int idx;
	wlc_bsscfg_t *cfg;
	bool ret = FALSE;

#ifdef BCM_BACKPLANE_TIMEOUT
	if (si_deviceremoved(wlc->pub->sih)) {
		return ret;
	}
#endif /* BCM_BACKPLANE_TIMEOUT */

	DBG_PRINT_WAKE_RSN(wlc->wake);
	DBG_PRINT_WAKE_RSN(wlc->PMawakebcn);
	DBG_PRINT_WAKE_RSN(SCAN_IN_PROGRESS(wlc->scan));
#ifdef WL_AIR_IQ
	DBG_PRINT_WAKE_RSN(wlc_airiq_scan_in_progress(wlc));
#endif /* WL_AIR_IQ */
	DBG_PRINT_WAKE_RSN(WLC_RM_IN_PROGRESS(wlc));
#ifdef WL11K
	DBG_PRINT_WAKE_RSN(wlc_rrm_inprog(wlc));
#endif /* WL11K */
	DBG_PRINT_WAKE_RSN(AS_IN_PROGRESS_WLC(wlc));
	DBG_PRINT_WAKE_RSN(wlc->PMpending);
	DBG_PRINT_WAKE_RSN(wlc->PSpoll);
	DBG_PRINT_WAKE_RSN(wlc->check_for_unaligned_tbtt);
	DBG_PRINT_WAKE_RSN(wlc->apsd_sta_usp);
	DBG_PRINT_WAKE_RSN(wlc->pm2_radio_shutoff_pending);
	DBG_PRINT_WAKE_RSN(wlc->gptimer_stay_awake_req);
	DBG_PRINT_WAKE_RSN(wlc->monitor != 0);
	DBG_PRINT_WAKE_RSN(wlc_hw_get_wake_override(wlc->hw) != 0);
	DBG_PRINT_WAKE_RSN(wlc->user_wake_req != 0);

	/* stay awake when one of these global conditions meets */
	if (wlc->wake ||
	    wlc->PMawakebcn ||
	    SCAN_IN_PROGRESS(wlc->scan) ||
#ifdef WL_AIR_IQ
	    wlc_airiq_scan_in_progress(wlc) ||
#endif // endif
	    WLC_RM_IN_PROGRESS(wlc) ||
#ifdef WL11K
	    wlc_rrm_inprog(wlc) ||
#endif /* WL11K */
	    AS_IN_PROGRESS_WLC(wlc) ||
	    wlc->PMpending ||
	    wlc->PSpoll ||
	    wlc->check_for_unaligned_tbtt ||
	    wlc->apsd_sta_usp ||
	    wlc->pm2_radio_shutoff_pending ||
	    wlc->gptimer_stay_awake_req ||
	    wlc->monitor != 0 ||
	    (wlc_hw_get_wake_override(wlc->hw) != 0) ||
	    wlc->user_wake_req != 0) {
		ret = TRUE;
		goto done;
	}

	/* stay awake as soon as we bring up a non-P2P AP bsscfg */
	FOREACH_UP_AP(wlc, idx, cfg) {
		if (
#ifdef WLP2P
			!BSS_P2P_ENAB(wlc, cfg) &&
#endif /* WLP2P */
#ifdef RADIONOA_PWRSAVE
			!(RADIONOA_PWRSAVE_ENAB(wlc->pub) &&
			wlc_radio_pwrsave_in_power_save(wlc->ap)) &&
#endif /* RADIONOA_PWRSAVE */
			TRUE)
		{
			ret = TRUE;
			goto done;
		}
	}

	/* stay awake as soon as we bring up a non-primary STA bsscfg */
	FOREACH_AS_STA(wlc, idx, cfg) {
		if (cfg != wlc->cfg &&
#ifdef WLMCNX
		    !MCNX_ENAB(wlc->pub) &&
#endif // endif
		    TRUE)
		{
			ret = TRUE;
			goto done;
		}
	}

#ifdef WLTDLS
	if (TDLS_ENAB(wlc->pub)) {
		if (wlc_tdls_stay_awake(wlc->tdls)) {
			ret = TRUE;
			goto done;
		}
	}
#endif // endif

	/* stay awake as long as an infra STA is associating */
	FOREACH_AS_STA(wlc, idx, cfg) {
		if (cfg->BSS) {
			if (cfg->assoc != NULL &&
			    !(cfg->assoc->state == AS_IDLE ||
			    cfg->assoc->state == AS_DISASSOC_TIMEOUT))
			{
				ret = TRUE;
				goto done;
			}
		}
	}

done:
#ifdef WL_PWRSTATS
	if (PWRSTATS_ENAB(wlc->pub))
		wlc_pwrstats_wake_reason_upd(wlc, ret);
#endif /* WL_PWRSTATS */
	return ret;
} /* wlc_stay_awake */

/** conditions under which the PM bit should be set in outgoing frames. */
bool
wlc_ps_allowed(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_pm_st_t *pm = cfg->pm;

	if (pm == NULL) {
		return FALSE;
	}

#ifdef PSTA
	if (PSTA_ENAB(wlc->pub))
		return FALSE;
#endif // endif

#ifdef WLTDLS
	if (BSS_TDLS_ENAB(wlc, cfg))
		return pm->PMenabled;
#endif // endif

#ifdef WL_PM_MUTE_TX
	if (wlc->pm_mute_tx->state != DISABLED_ST) {
		return pm->PMenabled;
	}
#endif // endif

	/* when ps request comes form the mchan module
	 * during switch, we should always set ps
	 */
	if (mboolisset(pm->PMenabledModuleId, WLC_BSSCFG_PS_REQ_CHANSW)) {
		ASSERT(pm->PMenabled);
		return TRUE;
	}

	/* disallow PS when one of the following bsscfg specific conditions meets */
	if (!cfg->associated ||
	    !pm->PMenabled ||
	    pm->PM_override ||
	    !cfg->dtim_programmed ||
#ifdef WLTDLS
	    !WLC_TDLS_PM_ALLOWED(wlc, cfg) ||
#endif // endif
	    !WLC_PORTOPEN(cfg))
		return FALSE;

	/* disallow PS when it is a non-primary STA bsscfg */
	if (cfg != wlc->cfg &&
#ifdef WLMCNX
	    !MCNX_ENAB(wlc->pub) &&
#endif // endif
	    TRUE)
		return FALSE;

	if (BSSCFG_IBSS(cfg)) {
		return FALSE;
	}

	return TRUE;
} /* wlc_ps_allowed */

bool
wlc_associnprog(wlc_info_t *wlc)
{
	return AS_IN_PROGRESS(wlc);
}
#endif /* STA */

#if defined(BCMDBG) || defined(BCMDBG_PHYDUMP) || defined(WLTEST) || \
	defined(TDLS_TESTBED) || defined(BCMDBG_AMPDU) || defined(MCHAN_MINIDUMP) || \
	defined(BCM_DNGDMP) || defined(BCMDBG_PHYDUMP)
#if defined(WLTINYDUMP)
/* APSTA: TODO: Add APSTA fields for tinydump */
/** Tiny dump that is available when full debug dump functionality is not compiled in */
static int
wlc_tinydump(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	char perm[32], cur[32];
	char ssidbuf[SSID_FMT_BUF_LEN];
	int i;
	wlc_bsscfg_t *bsscfg;

	/* XXX: Remove after 5/20/06 when wl-apps are upgraded to latest.
	 * There's a separate path (IOV_VER) for version.
	 */
	wl_dump_ver(wlc->wl, b);

	bcm_bprintf(b, "\n");

	bcm_bprintf(b, "resets %d\n", WLCNTVAL(wlc->pub->_cnt->reset));

	bcm_bprintf(b, "perm_etheraddr %s cur_etheraddr %s\n",
		bcm_ether_ntoa(&wlc->perm_etheraddr, perm),
		bcm_ether_ntoa(&wlc->pub->cur_etheraddr, cur));

	bcm_bprintf(b, "board 0x%x, board rev %s", wlc->pub->sih->boardtype,
	            bcm_brev_str(wlc->pub->boardrev, cur));
	if (wlc->pub->boardrev == BOARDREV_PROMOTED)
		bcm_bprintf(b, " (actually 0x%02x)", BOARDREV_PROMOTABLE);
	bcm_bprintf(b, "\n");

	bcm_bprintf(b, "rate_override: A %d, B %d\n",
		wlc->bandstate[BAND_5G_INDEX]->rspec_override,
		wlc->bandstate[BAND_2G_INDEX]->rspec_override);

	bcm_bprintf(b, "ant_rx_ovr %d txant %d\n", wlc->stf->ant_rx_ovr, wlc->stf->txant);

	FOREACH_BSS(wlc, i, bsscfg) {
		char ifname[32];
		wlc_key_info_t bss_key_info;

		(void)wlc_keymgmt_get_bss_tx_key(wlc->keymgmt, bsscfg, FALSE,
			&bss_key_info);
		bcm_bprintf(b, "\n");
		wlc_format_ssid(ssidbuf, bsscfg->SSID, bsscfg->SSID_len);
		strncpy(ifname, wl_ifname(wlc->wl, bsscfg->wlcif->wlif), sizeof(ifname));
		ifname[sizeof(ifname) - 1] = '\0';
		bcm_bprintf(b, "BSS Config %d: \"%s\"\n", i, ssidbuf);

		bcm_bprintf(b, "enable %d up %d wlif 0x%p \"%s\"\n",
		            bsscfg->enable,
		            bsscfg->up, OSL_OBFUSCATE_BUF(bsscfg->wlcif->wlif), ifname);
		bcm_bprintf(b, "wsec 0x%x auth %d wsec_index %d wep_algo %d\n",
		            bsscfg->wsec,
		            bsscfg->auth, bss_key_info.key_id, bss_key_info.algo);

		bcm_bprintf(b, "current_bss->BSSID %s\n",
		            bcm_ether_ntoa(&bsscfg->current_bss->BSSID, (char*)perm));

		wlc_format_ssid(ssidbuf, bsscfg->current_bss->SSID,
		                bsscfg->current_bss->SSID_len);
		bcm_bprintf(b, "current_bss->SSID \"%s\"\n", ssidbuf);

#ifdef STA
		/* STA ONLY */
		if (!BSSCFG_STA(bsscfg))
			continue;

		bcm_bprintf(b, "bsscfg %d assoc_state %d\n", WLC_BSSCFG_IDX(bsscfg),
		            bsscfg->assoc->state);
#endif /* STA */
	}
	bcm_bprintf(b, "\n");

#ifdef STA
	bcm_bprintf(b, "AS_IN_PROGRESS() %d stas_associated %d\n", AS_IN_PROGRESS(wlc),
	            wlc->stas_associated);
#endif /* STA */

	bcm_bprintf(b, "aps_associated %d\n", wlc->aps_associated);
	FOREACH_UP_AP(wlc, i, bsscfg)
	        bcm_bprintf(b, "BSSID %s\n", bcm_ether_ntoa(&bsscfg->BSSID, (char*)perm));

	return 0;
}
#endif /* WLTINYDUMP */
#endif // endif

#if defined(BCMTSTAMPEDLOGS)
static int
wlc_bcmdumptslog(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	BCM_REFERENCE(wlc);
	bcmdumptslog(b);
	return BCME_OK;
}
#endif // endif

/** flushes tx queues, resets d11 rx/tx dma, resets PHY */
void
BCMINITFN(wlc_reset)(wlc_info_t *wlc)
{
	WL_TRACE(("wl%d: wlc_reset\n", wlc->pub->unit));

	wlc->check_for_unaligned_tbtt = FALSE;

#ifdef STA
	/* Turn off PM mode for any stations that are up so we can utilize
	   unaligned tbtt to recover tsf related values.
	   With PM enabled before wlc_reset() is called, it seems that the
	   driver does not come out of PM mode properly afterwards and is
	   unable to receive beacons when wlc_phy_chanspec_set() doesn't get called.
	*/
	if (wlc->pub->associated) {
		int i;
		wlc_bsscfg_t *bsscfg;
		WL_EAP_TRC_ERROR(("wl%d: %s: !!!BIG-hammer!!!\n", wlc->pub->unit, __FUNCTION__));

		FOREACH_AS_STA(wlc, i, bsscfg) {
			if (bsscfg->up) {
				wlc_set_pmstate(bsscfg, FALSE);
				wlc_set_uatbtt(bsscfg, TRUE);
				WL_PS(("%s: turn off pm mode\n", __FUNCTION__));
				wlc->reset_triggered_pmoff = TRUE;
			}
		}
	}
#endif /* STA */

	/* slurp up hw mac counters before core reset */
	if (WLC_UPDATE_STATS(wlc)) {
		wlc_statsupd(wlc);
		/* reset our snapshot of macstat counters */
		bzero((char *)wlc->core->macstat_snapshot, sizeof(uint16) * MACSTAT_OFFSET_SZ);
#if defined(TDMTX)
		if (TDMTX_ENAB(wlc->pub)) {
			/* reset our snapshot of tdmtx counters */
			wlc_tdmtx_reset_cnts(wlc);
		}
#endif /* TDMTX */
	}

#ifdef WLAMSDU_TX
	/* clear amsdu agg so pkts don't come out during reset/before init */
	if (AMSDU_TX_ENAB(wlc->pub) && wlc->ami)
		wlc_amsdu_agg_flush(wlc->ami);
#endif /* WLAMSDU_TX */

	/* Check for any pending backplane access to complete
	 * In 4359b1 during big hammer sometimes driver crash was seen
	 * possibly from the ucode JIRA:SWWLAN-75844.
	 * Adding check based on the mac core revision
	 */
	if (D11REV_IS(wlc->pub->corerev, 56) && wlc->cmn->reinit_active) {
		wlc_suspend_mac_and_wait(wlc);
	}

	wlc_bmac_reset(wlc->hw);

	/* keep phypll off by default and let ucode take control of it.
	 * turn off phypll here that was turned on in wlc_reset.
	 */
	wlc_bmac_core_phypll_ctl(wlc->hw, FALSE);

	wlc_keymgmt_notify(wlc->keymgmt, WLC_KEYMGMT_NOTIF_WLC_DOWN, NULL, NULL, NULL, NULL);

	wlc->txretried = 0;

	/* Clear out all packets in the low_txq
	 *
	 * This call will flush any remaining packets in the low_txq software queue and
	 * reset the low_txq state.
	 */
	wlc_low_txq_flush(wlc->txqi, wlc->active_queue->low_txq);

} /* wlc_reset */

/** Called on eg BMAC FIFO errors */
void
wlc_fatal_error(wlc_info_t *wlc)
{
	uint32 gptime = 0;
#ifdef STA
	int idx;
	wlc_bsscfg_t *bsscfg;
	bool PMenabled[WLC_MAXBSSCFG] = {0};
#endif // endif

	if (wlc->down_override) {
		WL_ERROR(("wl%d: %s don't reinit if attempted to bring driver down\n",
			wlc->pub->unit, __FUNCTION__));
		return;
	}
	if ((wlc->hw->need_reinit !=  WL_REINIT_RC_USER_FORCED) &&
		(g_assert_type == 3)) {
		WL_ERROR(("wl%d: %s don't reinit if assert_type is 3 unless forced by user\n",
			wlc->pub->unit, __FUNCTION__));
		return;
	}

	WLCNTINCR(wlc->pub->_cnt->reinit);
	WL_ERROR(("wl%d: fatal error, reinitializing\n", wlc->pub->unit));
	WL_EAP_TRC_ERROR(("wl%d: fatal error, reinitializing\n", wlc->pub->unit));

	WL_PRINT(("wl%d: fatal error, reinitializing, total count of reinit's[%d]\n",
		wlc->pub->unit, wlc->pub->_cnt->reinit));

	if (wl_powercycle_inprogress(wlc->wl) != TRUE) {
		wlc->pub->reinitrsn->rsn[REINITRSNIDX(wlc->hw->need_reinit)]++;

		WL_ERROR(("wl%d: 802.11 reinit reason[%d], count[%d]\n",
			wlc->pub->unit, wlc->hw->need_reinit,
			wlc->pub->reinitrsn->rsn[REINITRSNIDX(wlc->hw->need_reinit)]));
	}
#if defined(DONGLEBUILD) && (defined(BCMDBG) || defined(WL_MACDBG))
	if ((wlc->hw->need_reinit !=  WL_REINIT_RC_NONE) &&
		(wlc->hw->need_reinit !=  WL_REINIT_RC_USER_FORCED))
	{
		/* this routine should never return to the caller for the dongle environment */
		OSL_SYS_HALT();
	}
#endif /* defined(DONGLEBUILD) && (defined(BCMDBG) || defined(WL_MACDBG)) */

	if (DEVICEREMOVED(wlc)) {
		/* Powercycle WiFi module in case of OSX */
		if (wlc->hw->need_reinit == WL_REINIT_RC_NONE) {
			wlc->hw->need_reinit = WL_REINIT_RC_DEVICE_REMOVED;
		}
	}

	wlc->state = WLC_STATE_GOING_UP;

	/* Call WL layer fatal error handler. It will return TRUE
	 * if it attemps to perform hard reset, FALSE otherwise
	 */
	if (wl_fatal_error(wlc->wl, wlc->hw->need_reinit)) {
		WL_ERROR(("Perform hard reset, skip core init\n"));
		return;
	}

	/* Clear reinit reason to continue with the coreinit */
	wlc->hw->need_reinit = WL_REINIT_RC_NONE;

	/* Set flag to indicate reinit is ON */
	if (D11REV_IS(wlc->pub->corerev, 56)) {
		wlc->cmn->reinit_active = TRUE;
	}

	/* cache gptime out count */
	gptime = wlc_hrt_gptimer_get(wlc);

#ifdef STA
	FOREACH_AS_STA(wlc, idx, bsscfg) {
		/* rtore last known PM state */
		PMenabled[idx] = bsscfg->pm->PMenabled;
	}
#endif // endif

#ifdef BCMHWA
	if (hwa_dev) {
		hwa_set_reinit(hwa_dev);
	}
#endif /* BCMHWA */

/* If RSDB chip and mode is RSDB then do RSDB reinitialization */
/* otherwise initialise the single interface for mimo img or mimo mode of rsdb */
#ifdef WLRSDB
	if (!wlc_rsdb_reinit(wlc))
#endif // endif
		wl_init(wlc->wl);

	/* restore gptime out count after init (gptimer is reset due to wlc_reset */
	if (gptime)
		wlc_hrt_gptimer_set(wlc, gptime);

	/* reset the flag */
	wlc->cmn->hwreinit = FALSE;

	/* Clear flag to indicate reinit is completed */
	if (D11REV_IS(wlc->pub->corerev, 56)) {
		wlc->cmn->reinit_active = FALSE;
	}

#ifdef STA
	FOREACH_AS_STA(wlc, idx, bsscfg) {
		/* restore last known PM state */
		bsscfg->pm->PMenabled = PMenabled[idx];
		wlc_set_pmenabled(bsscfg, bsscfg->pm->PMenabled);
	}
#endif // endif
}

#ifndef WLTEST
#ifdef WLRSDB
/* This function returns a chanspec which is on
* a band different than other wlc.
* This is used for wlc bringup
*/
static chanspec_t
wlc_rsdb_fixup_init_chanspec(wlc_info_t *wlc, chanspec_t chanspec)
{
	wlc_info_t *other_wlc;
	other_wlc = wlc_rsdb_get_other_wlc(wlc);
	/* if other_wlc is on same band
	* set chanspec to other band
	*/
	if (other_wlc &&
	other_wlc->pub->up &&
	CHSPEC_BAND(other_wlc->chanspec) == CHSPEC_BAND(chanspec)) {
		int ch_idx = CHSPEC_IS2G(other_wlc->chanspec) ? 15 : 0;
		chanspec =
		wlc_next_chanspec(wlc->cmi,
		CH20MHZ_CHSPEC(ch_idx), CHAN_TYPE_ANY, TRUE);
	}
	return chanspec;
}
#endif /* WLRSDB */
#endif /* WLTEST */

/**
 * Return the channel the driver should initialize during wlc_init.
 * the channel may have to be changed from the currently configured channel
 * if other configurations are in conflict (bandlocked, 11n mode disabled,
 * invalid channel for current country, etc.)
 */
static chanspec_t
BCMINITFN(wlc_init_chanspec)(wlc_info_t *wlc)
{
	chanspec_t chanspec;

	/* start with the default_bss channel since it store the last
	 * channel set by ioctl or iovar, or default to 2G
	 */
	chanspec = wlc->default_bss->chanspec;

#ifndef WLTEST
#ifdef WLRSDB
	if (RSDB_ENAB(wlc->pub))
	{
		wlc_info_t *other_wlc;
		wlc_info_t *wlc_2g, *wlc_5g;
		uint bunit = BAND_2G_INDEX;
		other_wlc = wlc_rsdb_get_other_wlc(wlc);

		if (other_wlc && other_wlc->pub->up &&
			CHSPEC_BAND(other_wlc->chanspec) == CHSPEC_BAND(chanspec)) {
			bunit = (CHSPEC_IS5G(other_wlc->chanspec)) ? BAND_2G_INDEX : BAND_5G_INDEX;
			chanspec = wlc_default_chanspec_by_band(wlc->cmi, bunit);
			WL_INFORM(("wl%d: %s returning chanspec %04x, other_wlc->chanspec %04x\n",
				wlc->pub->unit, __FUNCTION__, chanspec, other_wlc->chanspec));
		} else {
			wlc_rsdb_get_wlcs(wlc, &wlc_2g, &wlc_5g);
			if (CHSPEC_IS5G(chanspec) && (wlc != wlc_5g)) {
				chanspec = wlc_default_chanspec_by_band(wlc->cmi, BAND_2G_INDEX);
			} else if (CHSPEC_IS2G(chanspec) && (wlc != wlc_2g)) {
				chanspec = wlc_default_chanspec_by_band(wlc->cmi, BAND_5G_INDEX);
			}
		}
	}
	/* Since default_bss is shared between two wlc
	* it is possible that both wlc can be on same band.
	* To avoid this during wlc bring up we set a chanspec
	* which is on a band different than other wlc
	*/
	if (RSDB_ENAB(wlc->pub) &&
	    WLC_RSDB_IS_AUTO_MODE(wlc)) {
		chanspec = wlc_rsdb_fixup_init_chanspec(wlc, chanspec);
	}
#endif /* WLRSDB */
#endif /* WLTEST */

	/* Sanitize user setting for 80+80 MHz against current settings */
	if (CHSPEC_IS8080_UNCOND(chanspec) &&
		!VALID_8080CHANSPEC(wlc, chanspec)) {
		/* select the 80 0MHz primary channel in case 80 is allowed */
		chanspec = wf_chspec_primary80_chspec(chanspec);
	}

	/* Sanitize user setting for 160 MHz against current settings */
	if (CHSPEC_IS160_UNCOND(chanspec) &&
		!VALID_160CHANSPEC(wlc, chanspec)) {
		/* select the 80MHz primary channel in case 80 is allowed */
		chanspec = wf_chspec_primary80_chspec(chanspec);
	}

	/* Sanitize user setting for 80MHz against current settings */
	if (CHSPEC_IS80_UNCOND(chanspec) &&
		!VALID_80CHANSPEC(wlc, chanspec)) {
		/* select the 40MHz primary channel in case 40 is allowed */
		chanspec = wf_chspec_primary40_chspec(chanspec);
	}

	/* Sanitize user setting for 40MHz against current settings */
	if (CHSPEC_IS40_UNCOND(chanspec) &&
	    (!N_ENAB(wlc->pub) || !VALID_40CHANSPEC_IN_BAND(wlc, CHSPEC_WLCBANDUNIT(chanspec))))
		chanspec = CH20MHZ_CHSPEC(wf_chspec_ctlchan(chanspec));

	/* make sure the channel is on the supported band if we are band-restricted */
	if (wlc->bandlocked || NBANDS(wlc) == 1) {
		/* driver is configured for the current band only,
		 * pick another channel if the chanspec is not on the current band.
		 */
		if (CHSPEC_WLCBANDUNIT(chanspec) != wlc->band->bandunit)
			chanspec = wlc_default_chanspec(wlc->cmi, TRUE);
	}

	/* validate channel */
	if (!wlc_valid_chanspec_db(wlc->cmi, chanspec)) {
		wlcband_t *save_band = wlc->band;

		wlc_pi_band_update(wlc, CHSPEC_WLCBANDUNIT(chanspec));
		chanspec = wlc_default_chanspec(wlc->cmi, TRUE);
		wlc_pi_band_update(wlc, save_band->bandunit);
	}

	return chanspec;
} /* wlc_init_chanspec */

static void
BCMINITFN(wlc_set_ether_to_core)(wlc_info_t *wlc)
{
	int i;
	wlc_bsscfg_t *bsscfg;

	FOREACH_BSS(wlc, i, bsscfg) {
		wlc_set_mac(bsscfg);
		wlc_set_bssid(bsscfg);
	}
}

static void
BCMINITFN(wlc_enable_edcf)(wlc_info_t *wlc)
{
	if (WME_ENAB(wlc->pub)) {
		int i;
		wlc_bsscfg_t *bsscfg;

		OR_REG(wlc->osh, D11_IFS_CTL(wlc), IFS_USEEDCF);
		FOREACH_BSS(wlc, i, bsscfg) {
			if (BSS_WME_ENAB(wlc, bsscfg))
				wlc_edcf_acp_apply(wlc, bsscfg, FALSE);
		}
	}
}

static void
BCMINITFN(wlc_up_handlers)(wlc_info_t *wlc)
{
	int i;

	for (i = 0; i < wlc->pub->max_modules; i ++) {
		if (wlc->modulecb[i].up_fn == NULL)
			continue;
		if (wlc->modulecb[i].up_fn(wlc->modulecb_data[i].hdl) != BCME_OK) {
			WL_ERROR(("wl%d: %s UP handler error\n",
			          wlc->pub->unit, wlc->modulecb[i].name));
			/* continue??? */
		}
	}
}

static void
BCMINITFN(wlc_set_sar_and_mintxpower)(wlc_info_t *wlc)
{
	bool sar;

	if (wlc->pub->sih->boardvendor == VENDOR_APPLE) {
		int min_txpower;

		if (wlc->pub->sih->chip == BCM4360_CHIP_ID ||
			BCM43602_CHIP(wlc->pub->sih->chip) ||
			wlc->pub->sih->chip == BCM4350_CHIP_ID) {
			min_txpower = BCM94360_MINTXPOWER;
			sar = TRUE;
		} else if (wlc->pub->sih->chip == BCM43460_CHIP_ID ||
			wlc->pub->sih->chip == BCM43526_CHIP_ID ||
			wlc->pub->sih->chip == BCM4352_CHIP_ID) {
			min_txpower = BCM94360_MINTXPOWER;
			sar = FALSE;
		} else  {
			min_txpower = ARPT_MODULES_MINTXPOWER;
			sar = FALSE;
		}

		wlc_iovar_setint(wlc, "min_txpower", min_txpower);
	} else {
		/* IOVAR control get higher priority in generic usage */
		sar = wlc_channel_sarenable_get(wlc->cmi);
	}

	wlc_iovar_setint(wlc, "sar_enable", sar);
}

#ifdef STA
static void
BCMINITFN(wlc_assoc_abort_all_sta)(const wlc_info_t *wlc)
{
	int idx;
	wlc_bsscfg_t *cfg;

	if (wlc->pub->up) {
		FOREACH_STA(wlc, idx, cfg) {
			wlc_assoc_abort(cfg);
		}
	}
}
#endif // endif

static void
BCMINITFN(wlc_reinit)(wlc_info_t *wlc)
{
	if (wlc->pub->up) {
		/* Restart the AP if we are up and reiniting due to a big-hammer.
		 * If we are not yet up (and being called within wlc_up) do not
		 * call wlc_restart_ap() since it calls wlc_set_chanspec()
		 * which may attempt to switch to a band that has not yet been initialized.
		 * Clear aps_associated to force the first AP up to re-init core
		 * information, in particular the TSF (since we reset it above).
		 */
		if (AP_ENAB(wlc->pub)) {
			if (SCAN_IN_PROGRESS(wlc->scan)) {
		                wlc_scan_abort(wlc->scan, WLC_E_STATUS_ABORT);
			}
			wlc_restart_ap(wlc->ap);
		}
		/*
		 * If single AP, phy cal for AP was done at wlc_restart_ap();
		 * otherwise do phy cal on the current chanspec for everything else,
		 * e.g. multiple APs, single/multiple STAs
		 */
		if ((wlc->stas_associated > 0) || (wlc->aps_associated > 1)) {
			wlc_phy_cal_perical(WLC_PI(wlc), PHY_PERICAL_UP_BSS);
		}
#ifdef STA
		wlc_restart_sta(wlc);
#endif // endif
	}

#ifdef STA
	wlc_assoc_abort_all_sta(wlc);

#ifdef WLRSDB
	/* If parallel SCAN is in progress then it is possible
	* that other WLC is having an association in progress.
	* Assoc queue is shared in RSDB but the above assoc
	* abort is running on current WLC. So continue SCAN
	* to complete the association process if the init is not
	* due to a fatal error.
	*/
	if (RSDB_ENAB(wlc->pub) && !AS_IN_PROGRESS(wlc) &&
		SCAN_IN_PROGRESS(wlc->scan) && !wlc->cmn->reinit_active) {
		wlc_scan_abort(wlc->scan, WLC_E_STATUS_ABORT);
	}
#else
	/* Possibly resync scan state machine as channel and mac status would have changed */
	wlc_scan_abort(wlc->scan, WLC_E_STATUS_ABORT);
#endif /* WLRSDB */

#endif /* STA */
}

/**
 * (re)initializes MAC + PHY during 'wl up' or after a serious fault condition. Called by the WL
 * layer. Downloads ucode and takes the ucode out of suspend. This function enables WLAN interrupts
 * only if the hardware has been fully configured. Thus, if this function is called when
 * the WL subsystem is down or in the process of being up'ed, then it will not enable interrupts.
 */
void BCMINITFN(wlc_init)(wlc_info_t *wlc)
{
	chanspec_t chanspec;
	bool mute = FALSE;
	wlcband_t * band;
	wlc_phy_t * pi;
	uint16 feat_flag;

	BCM_REFERENCE(feat_flag);
	BCM_REFERENCE(mute);

	WL_TRACE(("wl%d: wlc_init\n", wlc->pub->unit));

	wl_intrsoff(wlc->wl);
	/* This will happen if a big-hammer was executed. In that case, we want to go back
	 * to the channel that we were on and not new channel
	 */
	if (wlc->pub->associated)
		chanspec = wlc->home_chanspec;
	else
		chanspec = wlc_init_chanspec(wlc);

	band = wlc->band;

	/* Choose the right bandunit in case it is not yet synchronized to the chanspe */
	if (NBANDS(wlc) > 1 && band->bandunit != CHSPEC_WLCBANDUNIT(chanspec))
		band = wlc->bandstate[OTHERBANDUNIT(wlc)];

	/* Update default_bss rates to the band specific default rate set */
	wlc_rateset_default(wlc, &wlc->default_bss->rateset, &band->defrateset, band->phytype,
		band->bandtype, FALSE, RATE_MASK_FULL, wlc_get_mcsallow(wlc, NULL),
		CHSPEC_WLC_BW(chanspec), wlc->stf->op_txstreams);

	/* Mute the phy when 11h is enabled and we are operating on radar
	 * sensitive channel.
	 */
	if (WL11H_AP_ENAB(wlc) && wlc_radar_chanspec(wlc->cmi, chanspec) &&
			wlc_quiet_chanspec(wlc->cmi, chanspec)) {
		mute = TRUE;
	}
#ifdef WL_SARLIMIT
	/* initialize SAR limit per SROM */
	wlc_channel_sar_init(wlc->cmi);
#endif /* WL_SARLIMIT */

	wlc_bmac_init(wlc->hw, chanspec); /* after this function, ucode is in suspended state */

#ifdef BCM_RECLAIM
	wl_reclaim_postattach();
#endif /* BCM_RECLAIM */
	if (D11REV_GE(wlc->pub->corerev, 40)) {
		wlc->pub->m_amt_info_blk = (wlc_read_shm(wlc, M_AMT_INFO_PTR(wlc)) << 1);
	}

	wlc_sr_fix_up(wlc);

	/* read rate table direct address maps from shm */
	wlc_read_rt_dirmap(wlc);

#ifdef STA
	/* update beacon listen interval */
	wlc_bcn_li_upd(wlc);

	/* maximum time to wait for beacon after wakeup from doze (at pretbtt).
	 * read the initial ucode config only...
	 * SHM init value is in 8us unit.
	 */
	if (wlc->bcn_wait_prd == 0) {
		wlc->bcn_wait_prd = wlc_bmac_read_shm(wlc->hw,
			M_POSTDTIM0_NOSLPTIME(wlc)) << 3 >> 10;
		ASSERT(wlc->bcn_wait_prd > 0);
	}
#endif // endif

	/* the world is new again, so is our reported rate */
	wlc_reprate_init(wlc);

	/* Call any registered UP handlers */
	wlc_up_handlers(wlc);

	/* write ethernet address to core */
	wlc_set_ether_to_core(wlc);

	wlc_bandinit_ordered(wlc, chanspec);

	pi = WLC_PI(wlc);

	/* init probe response timeout */
	wlc_write_shm(wlc, M_PRS_MAXTIME(wlc), wlc->prb_resp_timeout);

	/* limit frameburst txop by country */
	wlc_ht_frameburst_limit(wlc->hti);

	/* in case rifs was set when not up, need to run war here */
	phy_misc_tkip_rifs_war(pi, WLC_HT_GET_RIFS(wlc->hti));

	wlc_set_pwrthrottle_config(wlc);

	wlc_set_sar_and_mintxpower(wlc);

#ifdef FCC_PWR_LIMIT_2G
	wlc_phy_fcc_pwr_limit_set(pi, wlc_phy_fcc_pwr_limit_get(pi));
#endif /* FCC_PWR_LIMIT_2G */

#ifdef WLAMPDU
	/* Update some shared memory locations related to max AMPDU size allowed to received */
	wlc_ampdu_shm_upd(wlc->ampdu_rx);
#endif // endif

	/* Update txcore-mask and spatial-mapping in shared memory */
	if (N_ENAB(wlc->pub)) {
		wlc->stf->shmem_base = wlc->pub->m_coremask_blk;
		wlc_stf_txcore_shmem_write(wlc, TRUE);
	}

	/* band-specific inits */
	wlc_bsinit(wlc);

	/* Enable EDCF mode (while the MAC is suspended) */
	wlc_enable_edcf(wlc);

	/* Init precedence maps for empty FIFOs */
	wlc_tx_prec_map_init(wlc);

	/* read the ucode version if we have not yet done so */
	if (wlc->ucode_rev == 0) {
		wlc->ucode_rev = wlc_read_shm(wlc, M_BOM_REV_MAJOR(wlc)) << NBITS(uint16);
		wlc->ucode_rev |= wlc_read_shm(wlc, M_BOM_REV_MINOR(wlc));
		wlc->ucode_rev2 = 0;
	}

#if defined(WLAMPDU_UCODE)
	/* Enable ucode AMPDU aggregation */
	wlc_sidechannel_init(wlc);
#endif // endif

	/* ..now really unleash hell (allow the MAC out of suspend) */
	wlc_enable_mac(wlc);
#ifdef WL_PSMX
	if (PSMX_ENAB(wlc->pub)) {
		wlc_bmac_enable_macx(wlc->hw);
	}
#endif // endif

#ifndef BCMQT
	/* PSM(x) is enabled so ensure to suspend the fifos and mute the phy for preism cac time */
	if (mute) {
		wlc_mute(wlc, ON, PHY_MUTE_FOR_PREISM);
	}
#endif /* !BCMQT */

#ifdef STA
	_wlc_set_wake_ctrl(wlc);
#endif // endif

	/* Propagate rfaware_lifetime setting to ucode */
	wlc_rfaware_lifetime_set(wlc, wlc->rfaware_lifetime);

	/* clear tx flow control */
	/* XXX WES: why should wl_init() clear flow control? It is software state that should
	 * not change on a re-init. If a queue was flow controlled before a big-hammer init,
	 * it should still be after.
	 */
	wlc_txflowcontrol_reset(wlc);

	/* clear tx data fifo suspends */
	wlc->tx_suspended = FALSE;

	/* enable the RF Disable Delay timer */
	W_REG(wlc->osh, D11_rfdisabledly(wlc), RFDISABLE_DEFAULT);

	/*
	 * Initialize WME parameters; if they haven't been set by some other
	 * mechanism (IOVar, etc) then read them from the hardware.
	 */
	wlc_wme_shm_read(wlc);

	wlc_ht_update_coex_support(wlc, wlc->pub->_coex);

	wlc_ht_rts_minlen_set(wlc->hti);
#ifdef CCA_STATS
	if (WL_CCA_STATS_ENAB(wlc->pub)) {
		cca_stats_upd(wlc, 0);
	}
#endif // endif

#ifdef WLCHANIM
	if (WLC_CHANIM_ENAB(wlc->pub)) {
		int err;

		if ((err = wlc_lq_chanim_update(wlc, wlc->chanspec, CHANIM_CHANSPEC)) != BCME_OK) {
			WL_ERROR(("wl%d: %s: WLC_CHANIM upd fail err: %d\n", wlc->pub->unit,
				__FUNCTION__, err));
		}
	}
#endif /* WLCHANIM */

	/* Fixed the TX deadlock with the block_datafilo flag
	 * Only preserved flags for DFS.
	 * FIXME: should we move it after wl_scan_abort?
	 */

	/* XXX:TXQ What is the deadlock? Need to find out what is
	 * being unblocked and have the responsible module unblock itself.
	 */

	wlc->block_datafifo &= DATA_BLOCK_QUIET;

	/* Cleanup assoc in progress in case of reinit */
	wlc_reinit(wlc);

#ifdef WLPM_BCNRX
	if (PM_BCNRX_ENAB(wlc->pub))
		wlc_pm_bcnrx_init(wlc);
#endif /* WLPM_BCNRX */

	wlc->cached_enqtime = 0;

#ifdef WLSQS
	wlc_wlfc_sqs_stride_resume(wlc, NULL, 0);
#endif // endif
	/* XXX: Dynamic frameburst threshold
	 * 0: disabled
	 * ACTIVE_BIDIR_DFLT_THRESH: enabled
	 * ACTIVE_BIDIR_THRESH_AUTO: enabled after VW UDP packets are received.
	 */
	wlc->active_bidir_thresh = ACTIVE_BIDIR_THRESH_AUTO;
	wlc->active_bidir = FALSE;
	wlc->active_udpv6 = FALSE;
	wlc->active_udpv4 = FALSE;
	wlc->active_tcp = FALSE;

	if (wlc->hw->up == TRUE) {
		wl_intrson(wlc->wl);
	}
} /* wlc_init() */

void
wlc_mac_bcn_promisc_update(wlc_info_t *wlc, uint32 val,
	bool set_reset)
{
	if (set_reset == TRUE) {
		wlc->bcnmisc |= val;
	}
	else {
		wlc->bcnmisc &= ~val;
	}
	wlc_mac_bcn_promisc(wlc);
}

void
wlc_mac_bcn_promisc(wlc_info_t *wlc)
{
	if ((AP_ACTIVE(wlc) && (N_ENAB(wlc->pub) || wlc->band->gmode)) ||
	    wlc->bcnmisc) {
		wlc_mctrl(wlc, MCTL_BCNS_PROMISC, MCTL_BCNS_PROMISC);
	}
	else {
		wlc_mctrl(wlc, MCTL_BCNS_PROMISC, 0);
	}
}

/** set or clear maccontrol bits MCTL_PROMISC, MCTL_KEEPCONTROL and MCTL_KEEPBADFCS */
void
wlc_mac_promisc(wlc_info_t *wlc)
{
	uint32 promisc_bits = 0;
	uint32 mask = MCTL_PROMISC | MCTL_KEEPCONTROL | MCTL_KEEPBADFCS | MCTL_HETB;
#ifdef DWDS
	int idx;
	wlc_bsscfg_t *cfg;
#endif // endif

	/* promiscuous mode just sets MCTL_PROMISC
	 * Note: APs get all BSS traffic without the need to set the MCTL_PROMISC bit
	 * since all BSS data traffic is directed at the AP
	 */
	if (PROMISC_ENAB(wlc->pub) && !AP_ENAB(wlc->pub) &&
	    !WET_ENAB(wlc) && !PSTA_ENAB(wlc->pub) && !WET_DONGLE_ENAB(wlc))
		promisc_bits |= MCTL_PROMISC;

	promisc_bits |= wlc_monitor_get_mctl_promisc_bits(wlc->mon_info);
#ifdef DWDS
	/* disable promisc mode if any of the bsscfg is dwds */
	FOREACH_BSS(wlc, idx, cfg) {
		if (DWDS_ENAB(cfg) || MAP_ENAB(cfg)) {
			promisc_bits &= ~MCTL_PROMISC;
			break;
		}
	}
#endif // endif
#if defined(WL_EAP_AP1)
	if (wlc->pub->promisc) {
		promisc_bits |= MCTL_PROMISC | MCTL_KEEPCONTROL;
		mask = MCTL_PROMISC | MCTL_KEEPCONTROL;
	}
#endif /* WL_EAP_AP1 */

	wlc_mctrl(wlc, mask, promisc_bits);

	/* For promiscous mode, also enable probe requests */
	wlc_enable_probe_req(wlc, PROBE_REQ_PRMS_MASK,
		(promisc_bits & MCTL_PROMISC & mask) ? PROBE_REQ_PRMS_MASK:0);
}

#ifdef BCMASSERT_SUPPORT
/** check if hps and wake states of sw and hw are in sync */
bool
wlc_ps_check(wlc_info_t *wlc)
{
#ifdef STA
	bool hps, wake;
	bool wake_ok;
	volatile uint32 tmp;
	int idx;
	wlc_bsscfg_t *cfg;

	FOREACH_AS_STA(wlc, idx, cfg) {
		bool hw_hps = FALSE;

		if (!cfg->BSS)
			continue;

		if ((cfg == wlc->cfg) &&
#ifdef WLMCNX
		((MCNX_ENAB(wlc->pub) && !wlc->cmn->ps_multista) || !MCNX_ENAB(wlc->pub)) &&
#endif /* WLMCNX */
		TRUE) {
			tmp = R_REG(wlc->osh, D11_MACCONTROL(wlc));

			/* If deviceremoved is detected, then don't take any action
			 * as this can be called in any context. Assume that caller
			 * will take care of the condition. This is just to avoid assert
			 */
			if (tmp == 0xffffffff) {
				WL_ERROR(("wl%d: %s: dead chip\n", wlc->pub->unit, __FUNCTION__));
				WL_HEALTH_LOG(wlc, DEADCHIP_ERROR);
				return DEVICEREMOVED(wlc);
			}

			hw_hps = ((tmp & MCTL_HPS) != 0);
		}
#ifdef WLMCNX
		else if (MCNX_ENAB(wlc->pub)) {
			/* when HPS is forced i.e. during roaming ignore the check */
			if (TRUE &&
#ifdef WLP2P
			    BSS_P2P_ENAB(wlc, cfg) &&
#else
			    FALSE &&
#endif // endif
			    wlc_mcnx_hps_forced(wlc->mcnx, cfg)) {
					hw_hps = PS_ALLOWED(cfg);
			} else {
				tmp = wlc_mcnx_hps_get(wlc->mcnx, cfg);
				hw_hps = (tmp != 0);
			}
		}
#endif /* WLMCNX */

		hps = PS_ALLOWED(cfg);
		if (hps != hw_hps) {
			WL_ERROR(("wl%d.%d: hps not sync, sw %d, hw %d\n",
			          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), hps, hw_hps));
			WL_PS(("PM %d PMenabled %d PM_override %d "
			       "dtim_programmed %d PORTOPEN() %d\n",
			       cfg->pm->PM, cfg->pm->PMenabled, cfg->pm->PM_override,
			       cfg->dtim_programmed, WLC_PORTOPEN(cfg)));

			ASSERT(hps == hw_hps);
			return FALSE;
		}
	}

	tmp = R_REG(wlc->osh, D11_MACCONTROL(wlc));

	/* If deviceremoved is detected, then don't take any action
	 * as this can be called in any context. Assume that caller
	 * will take care of the condition. This is just to avoid assert
	 */
	if (tmp == 0xffffffff) {
		WL_ERROR(("wl%d: %s: dead chip\n", wlc->pub->unit, __FUNCTION__));
		WL_HEALTH_LOG(wlc, DEADCHIP_ERROR);
		return DEVICEREMOVED(wlc);
	}

	/* For a monolithic build the wake check can be exact since it looks at wake
	 * override bits. The MCTL_WAKE bit should match the 'wake' value.
	 */
	wake = STAY_AWAKE(wlc);
	wake_ok = (wake == ((tmp & MCTL_WAKE) != 0));
	if (!wake_ok) {
		WL_ERROR(("wl%d: wake not sync, sw %d maccontrol 0x%x\n",
		          wlc->pub->unit, wake, tmp));
		WL_PS(("wake %d PMawakebcn %d "
		       "SCAN_IN_PROGRESS() %d WLC_RM_IN_PROGRESS() %d AS_IN_PROGRESS() %d "
		       "PMpending %d PSpoll %d check_for_unaligned_tbtt %d apsd_sta_usp %d "
		       "pm2_radio_shutoff_pending %d monitor %d\n",
		       wlc->wake, wlc->PMawakebcn,
		       SCAN_IN_PROGRESS(wlc->scan), WLC_RM_IN_PROGRESS(wlc),
		       AS_IN_PROGRESS_WLC(wlc),
		       wlc->PMpending, wlc->PSpoll, wlc->check_for_unaligned_tbtt,
		       wlc->apsd_sta_usp,
		       wlc->pm2_radio_shutoff_pending, wlc->monitor));

		FOREACH_BSS(wlc, idx, cfg) {
			if (!cfg->BSS ||
			    !cfg->associated)
				continue;
			WL_PS(("bsscfg %d PMenabled %d "
			       "PMpending %d PSpoll %d apsd_sta_usp %d "
			       "check_for_unaligned_tbtt %d\n",
			       WLC_BSSCFG_IDX(cfg), cfg->pm->PMenabled,
			       cfg->pm->PMpending, cfg->pm->PSpoll, cfg->pm->apsd_sta_usp,
			       cfg->pm->check_for_unaligned_tbtt));
#ifdef WLP2P
			WL_PS(("P2P %d\n", BSS_P2P_ENAB(wlc, cfg)));
#endif // endif
		}
#ifdef WAR4360_UCODE
		WL_ERROR(("wl%d: %s: Wake State Out-of-Sync, Big Hammer\n",
			wlc->pub->unit, __FUNCTION__));
		wlc->hw->need_reinit = WL_REINIT_RC_PS_SYNC;
		return TRUE; /* return TRUE to avoid caller ASSERT */
#endif /* WAR4360_UCODE */

		ASSERT(wake_ok);
		return FALSE;
	}
#endif /* STA */
	return TRUE;
} /* wlc_ps_check */
#endif /* BCMASSERT_SUPPORT */

/** push sw wake state through hardware */
static void
__wlc_set_wake_ctrl(wlc_info_t *wlc, uint32 mc)
{
	uint32 new_mc;
	bool wake;
	bool awake_before;

	wake = STAY_AWAKE(wlc);

	WL_TRACE(("wl%d: __wlc_set_wake_ctrl: wake %d\n", wlc->pub->unit, wake));
	WL_RTDC2(wlc, wake ? "__wlc_set_wake_ctrl: wake=%02u PS---" :
	         "__wlc_set_wake_ctrl: wake=%02u PS+++", (wake ? 1 : 0), 0);

	new_mc = wake ? MCTL_WAKE : 0;

#if defined(BCMDBG) || defined(WLMSG_PS)
	if ((mc & MCTL_WAKE) && !wake)
		WL_PS(("wl%d: PS mode: clear WAKE (sleep if permitted) at tick %u\n",
		       wlc->pub->unit, R_REG(wlc->osh, D11_TSFTimerLow(wlc))));
	if (!(mc & MCTL_WAKE) && wake)
		WL_PS(("wl%d: PS mode: set WAKE (stay awake) at tick %u\n",
		       wlc->pub->unit, R_REG(wlc->osh, D11_TSFTimerLow(wlc))));
#endif	/* BCMDBG || WLMSG_PS */
	wlc_mctrl(wlc, MCTL_WAKE, new_mc);

	awake_before = (mc & MCTL_WAKE) != 0;

	if (wake && !awake_before)
		wlc_bmac_wait_for_wake(wlc->hw);

#ifdef WL_LTR
	if (LTR_ENAB(wlc->pub)) {
		/*
		 * Set latency tolerance based on wake and hps
		 * If stay awake, change to LTR active
		 * Else if hps bit is set, LTR sleep else LTR active
		 */
		if (wake) {
			wlc_ltr_hwset(wlc->hw, wlc->regs, LTR_ACTIVE);
		} else {
			wlc_ltr_hwset(wlc->hw, wlc->regs, (mc & MCTL_HPS)?LTR_SLEEP:LTR_ACTIVE);
		}
	}
#endif // endif

#ifdef WL_LEAKY_AP_STATS
	if (WL_LEAKYAPSTATS_ENAB(wlc->pub)) {
		if (!wake && awake_before) {
			wlc_leakyapstats_gt_event(wlc, wlc->cfg);
		}
	}
#endif /* WL_LEAKY_AP_STATS */
	if (HND_DS_HC_ENAB()) {
		wl_health_check_notify(wlc->wl, HEALTH_CHECK_WL_STAY_AWAKE, wake);
	}
} /* __wlc_set_wake_ctrl */

static void
_wlc_set_wake_ctrl(wlc_info_t *wlc)
{
	volatile uint32 mc;

	mc = R_REG(wlc->osh, D11_MACCONTROL(wlc));

	__wlc_set_wake_ctrl(wlc, mc);
}

/** push sw hps and wake state through hardware */
void
wlc_set_ps_ctrl(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	bool hps;
#if defined(BCMDBG) || defined(WLMSG_PS)
	volatile uint32 mc;
#endif /* BCMDBG || WLMSG_PS */

	if (!wlc->clk)
		return;

	if (DEVICEREMOVED(wlc)) {
		if (wlc->hw->need_reinit == WL_REINIT_RC_NONE) {
			wlc->hw->need_reinit = WL_REINIT_RC_DEVICE_REMOVED;
			WLC_FATAL_ERROR(wlc);
		}
		return;
	}

	hps = PS_ALLOWED(cfg);

#if defined(BCMDBG) || defined(WLMSG_PS)
	mc = R_REG(wlc->osh, D11_MACCONTROL(wlc));
#endif /* BCMDBG || WLMSG_PS */

	/* IBSS on primary interface uses shm hps bits for power save */
	if ((cfg == wlc->cfg) && !BSSCFG_IBSS(cfg) &&
#ifdef WLMCNX
		((MCNX_ENAB(wlc->pub) && !wlc->cmn->ps_multista) || !MCNX_ENAB(wlc->pub)) &&
#endif /* WLMCNX */
		TRUE) {
		uint32 new_mc;

		WL_TRACE(("wl%d.%d: wlc_set_ps_ctrl: hps %d\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), hps));

		new_mc = hps ? MCTL_HPS : 0;

#if defined(BCMDBG) || defined(WLMSG_PS)
		if ((mc & MCTL_HPS) && !hps) {
			WL_PS(("wl%d.%d: PM-MODE: clear HPS (no sleep and no PM)\n",
			       wlc->pub->unit, WLC_BSSCFG_IDX(cfg)));
#if !defined(DONGLEBUILD)
			/* Just now cleared HPS, clear awake_hps_0 to indicate SW does
			 * not know current wake status of uCode
			 */
			wlc->hw->awake_hps_0 = FALSE;
#endif /* !DONGLEBUILD */
		}

		if (!(mc & MCTL_HPS) && hps) {
			WL_PS(("wl%d.%d: PM-MODE: set HPS (permit sleep and enable PM)\n",
			       wlc->pub->unit, WLC_BSSCFG_IDX(cfg)));
#if !defined(DONGLEBUILD)
			/* HPS is enabled, clear awake_hps_0 to indicate SW does
			 * not know current wake status of uCode
			 */
			wlc->hw->awake_hps_0 = FALSE;
#endif /* !DONGLEBUILD */
		}
#endif /* BCMDBG || WLMSG_PS */

#ifdef WL_BCNTRIM
		if (WLC_BCNTRIM_ENAB(wlc->pub))
			wlc_bcntrim_update_bcntrim_enab(wlc->bcntrim, cfg, new_mc);
#endif /* WL_BCNTRIM */

		if (wlc->ebd->thresh && wlc->ebd->assoc_start_time) {
			/* if awake for EARLY_DETECT_PERIOD after assoc, early_detect is done */
			if ((OSL_SYSUPTIME() - wlc->ebd->assoc_start_time) > EARLY_DETECT_PERIOD)
				wlc->ebd->detect_done = TRUE;
			wlc->ebd->assoc_start_time = 0;
		}
		wlc_mctrl(wlc, MCTL_HPS, new_mc);
	}
#ifdef WLMCNX
	else if (MCNX_ENAB(wlc->pub)) {
#if defined(BCMDBG) || defined(WLMSG_PS)
		if (!hps)
			WL_PS(("wl%d.%d: PM-MODE: clear HPS (P2P no sleep and no PM)\n",
			       wlc->pub->unit, WLC_BSSCFG_IDX(cfg)));
		if (hps)
			WL_PS(("wl%d.%d: PM-MODE: set HPS (P2P permit sleep and enable PM)\n",
			       wlc->pub->unit, WLC_BSSCFG_IDX(cfg)));
#endif	/* BCMDBG || WLMSG_PS */

		wlc_mcnx_hps_set(wlc->mcnx, cfg, hps);
	}
#endif /* WLMCNX */
		if (HND_DS_HC_ENAB()) {
			wl_health_check_log(wlc->wl, HEALTH_CHECK_WL_PS_LOG,
				hps, 0);
		}

	_wlc_set_wake_ctrl(wlc);

	WL_PS(("wl%d.%d: wlc_set_ps_ctrl: mc 0x%x\n",
	        wlc->pub->unit, WLC_BSSCFG_IDX(cfg), R_REG(wlc->osh, D11_MACCONTROL(wlc))));

} /* wlc_set_ps_ctrl */

void
wlc_set_wake_ctrl(wlc_info_t *wlc)
{
	if (!wlc->pub->up)
		return;
	_wlc_set_wake_ctrl(wlc);
}

/**
 * Validate the MAC address for this bsscfg.
 * If valid, the MAC address will be set in the bsscfg's cur_etheraddr.
 */
int
wlc_validate_mac(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct ether_addr *addr)
{
	int err = 0;

	/* don't let a broadcast address be set */
	if (ETHER_ISMULTI(addr))
		return BCME_BADADDR;

	if (cfg == wlc->cfg) {
		/* XXX - JFH until we remove references to pub->cur_etheraddr,
		 * this must be here for the primary config
		 */
		bcopy(addr, &wlc->pub->cur_etheraddr, ETHER_ADDR_LEN);
		wlc_bmac_set_hw_etheraddr(wlc->hw, addr);

#ifdef MBSS
		if (MBSS_ENAB(wlc->pub)) {
			wlc_mbss_reset_mac(wlc, cfg);
		}
#endif // endif
	}
#ifdef MBSS
	else if (MBSS_ENAB(wlc->pub) && BSSCFG_AP(cfg) &&
	         (err = wlc_mbss_validate_mac(wlc, cfg, addr)) != BCME_OK) {
		return err;
	}
#endif /* MBSS */

	if (!err) {
		/* Accept the user's MAC address */
		bcopy(addr, &cfg->cur_etheraddr, ETHER_ADDR_LEN);

		/* Takes effect immediately. Note that it's still recommended that
		 * the BSS be down before doing this to avoid having frames in transit
		 * with this as TA, but that's a corner case.
		 */
		/* VSDB TODO: changes for rcmta -> AMT ? or is it taken care under wlc_set_mac() */
		if (wlc->clk &&
		    (R_REG(wlc->osh, D11_MACCONTROL(wlc)) & MCTL_EN_MAC)) {
			wlc_suspend_mac_and_wait(wlc);
			wlc_set_mac(cfg);
			wlc_enable_mac(wlc);
		}
	}

	return err;
} /* wlc_validate_mac */

void
wlc_clear_mac(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;

	WL_TRACE(("wl%d: %s: config %d\n", wlc->pub->unit, __FUNCTION__,
	          WLC_BSSCFG_IDX(cfg)));

	WL_APSTA_UPDN(("wl%d: wlc_set_mac for config %d\n", wlc->pub->unit,
	          WLC_BSSCFG_IDX(cfg)));

	if (cfg == wlc->cfg) {
		/* clear the MAC addr from the match registers */
		(void) wlc_clear_addrmatch(wlc, WLC_ADDRMATCH_IDX_MAC);
	}
#ifdef WLMCNX
	else if (MCNX_ENAB(wlc->pub)) {
		wlc_mcnx_ra_unset(wlc->mcnx, cfg);
	}
#endif /* WLMCNX */
}

/** Write this BSS config's MAC address to core. Updates RXE match engine. */
int
wlc_set_mac(wlc_bsscfg_t *cfg)
{
	int err = 0;
	wlc_info_t *wlc = cfg->wlc;
	const struct ether_addr *addr;

	addr = &cfg->cur_etheraddr;

	WL_APSTA_UPDN(("wl%d: wlc_set_mac for config %d\n", wlc->pub->unit,
	          WLC_BSSCFG_IDX(cfg)));

	if (cfg == wlc->cfg) {
		/* enter the MAC addr into the match registers */
		uint16 attr = AMT_ATTR_VALID | AMT_ATTR_A1;
		wlc_set_addrmatch(wlc, WLC_ADDRMATCH_IDX_MAC, addr, attr);
	}
#ifdef PSTA
	else if (BSSCFG_PSTA(cfg)) {
		;
	}
#endif /* PSTA */
#ifdef MBSS
	else if (MBSS_ENAB(wlc->pub) && BSSCFG_AP(cfg)) {
		wlc_mbss_set_mac(wlc, cfg);
	}
#endif /* MBSS */
#ifdef WLMCNX
	else if (MCNX_ENAB(wlc->pub)) {
		wlc_mcnx_ra_set(wlc->mcnx, cfg);
	}
#endif /* WLMCNX */

#ifdef WLAMPDU
	wlc_ampdu_macaddr_upd(wlc);
#endif // endif

	return err;
} /* wlc_set_mac */

void
wlc_clear_bssid(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;

	if (cfg == wlc->cfg)
		/* clear the BSSID from the match registers */
		(void) wlc_clear_addrmatch(wlc, WLC_ADDRMATCH_IDX_BSSID);
#if defined(WLMCNX)
		else if (
#ifdef WLMCNX
			MCNX_ENAB(wlc->pub) ||
#endif // endif
			FALSE)
		wlc_mcnx_bssid_unset(wlc->mcnx, cfg);
#endif /* WLMCNX */

	wlc_keymgmt_notify(wlc->keymgmt, WLC_KEYMGMT_NOTIF_BSSID_UPDATE, cfg,
		NULL, NULL, NULL);
}

/**
 * Write the BSS config's BSSID address to core (set_bssid in d11procs.tcl).
 * Updates RXE match engine.
 *
 * XXX The cfg==cfg->wlc part of this should just call wlc_set_bssid_hw() when
 * wlc_set_bssid_hw() is eventually made public.  Currently its static in wlc_mcnx.c.
 */
void
wlc_set_bssid(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;

	/* if primary config, we need to update BSSID in RXE match registers */
	if (cfg == wlc->cfg) {
		uint16 attr = 0;
		/* enter the MAC addr into the match registers */
		if (D11REV_GE(wlc->pub->corerev, 40)) {
			/* Null address gets zero attributes */
			if (!ETHER_ISNULLADDR(&cfg->BSSID)) {
				/* STA and AP configs need different match attributes
				 * for the BSSID postion, A1/A2/A3
				 */
				attr |= AMT_ATTR_VALID;
				if (BSSCFG_STA(cfg) && cfg->BSS) {
					/* For infra STA, BSSID can be in A2 (management)
					 * or A3 (data).
					 * Also in A1 for data with (FromDS, ToDS) = (0, 1).
					 * Set amtinfo to notify ucode that this A1 match is
					 * not RA but BSSID
					 */
					attr |= (AMT_ATTR_A1 | AMT_ATTR_A2 | AMT_ATTR_A3);
					if (ether_cmp(&cfg->BSSID, &cfg->cur_etheraddr)) {
						uint16 prev_amtinfo =
							wlc_read_amtinfo_by_idx(wlc, AMT_IDX_BSSID);
						wlc_write_amtinfo_by_idx(wlc, AMT_IDX_BSSID,
						  (prev_amtinfo | NBITVAL(C_ADDR_BSSID_NBIT)));
					}
				} else {
					/* For IBSS STA, BSSID is in A3 */
					/* For AP, BSSID might be in A3 for a probe request */
					attr |= AMT_ATTR_A3;
				}
			}
		}
		wlc_set_addrmatch(wlc, WLC_ADDRMATCH_IDX_BSSID, &cfg->BSSID, attr);
	}
#if defined(WLMCNX)
	else if (
#ifdef WLMCNX
		MCNX_ENAB(wlc->pub) ||
#endif // endif
		FALSE) {
		wlc_mcnx_bssid_set(wlc->mcnx, cfg);
	}
#endif /* defined(WLMCNX) */

#ifdef STA
	if (BSSCFG_STA(cfg) && cfg->BSS)
	{
		/*
		Set the BSSID, TA and RA in the NULL DP template for ucode generated
		Null data frames only for Mac Revs 40 or greater
		*/
		if (D11REV_GE(wlc->pub->corerev, 40)) {
			wlc_nulldp_tpl_upd(cfg);
		}
	}
#endif // endif

	wlc_keymgmt_notify(wlc->keymgmt, WLC_KEYMGMT_NOTIF_BSSID_UPDATE, cfg,
		NULL, NULL, NULL);

} /* wlc_set_bssid */

/**
 * Suspend the the MAC and update the slot timing for standard 11b/g (20us slots) or shortslot 11g
 * (9us slots).
 */
void
wlc_switch_shortslot(wlc_info_t *wlc, bool shortslot)
{
	int idx;
	wlc_bsscfg_t *cfg;

	ASSERT(wlc->band->gmode);

	/* use the override if it is set */
	if (wlc->shortslot_override != WLC_SHORTSLOT_AUTO)
		shortslot = (wlc->shortslot_override == WLC_SHORTSLOT_ON);

	if (wlc->shortslot == shortslot)
		return;

	wlc->shortslot = shortslot;

	/* update the capability based on current shortslot mode */
	FOREACH_BSS(wlc, idx, cfg) {
		if (!cfg->associated)
			continue;
		cfg->current_bss->capability &= ~DOT11_CAP_SHORTSLOT;
		if (wlc->shortslot)
			cfg->current_bss->capability |= DOT11_CAP_SHORTSLOT;
	}

	wlc_bmac_set_shortslot(wlc->hw, shortslot);
}

/**
 * Update aCWmin based on supported rates. 11g sec 9.2.12, 19.8.4, supported rates are all in the
 * set 1, 2, 5.5, and 11 use B PHY aCWmin == 31, The PSM needs to be suspended for this call.
 */
void
wlc_cwmin_gphy_update(wlc_info_t *wlc, wlc_rateset_t *rs, bool associated)
{
	uint16 cwmin;
	uint i;
	uint r;

	if (associated) {
		/* use the BSS supported rates to determine aCWmin */
		cwmin = BPHY_CWMIN;
		for (i = 0; i < rs->count; i++) {
			r = rs->rates[i] & RATE_MASK;
			/*
			 * Checking for RATE_ISOFDM(rate) is not sufficient.
			 * If the rateset includes PBCC rates 22 or 33Mbps,
			 * we should also use APHY_CWMIN,
			 * but the PBCC rates are not OFDM.
			 */
			if (!RATE_ISCCK(r)) {
				cwmin = APHY_CWMIN;
				break;
			}
		}
	} else {
		/* unassociated aCWmin A phy value (11g sec 9.2.12, 19.8.4) */
		cwmin = APHY_CWMIN;
	}

	WL_INFORM(("wl%d: wlc_cwmin_gphy_update(): setting aCWmin = %d\n", wlc->pub->unit, cwmin));

	wlc_set_cwmin(wlc, cwmin);
}

/** propagate home chanspec to all bsscfgs in case bsscfg->current_bss->chanspec is referenced */
void
wlc_set_home_chanspec(wlc_info_t *wlc, chanspec_t chanspec)
{
	if (wlc->home_chanspec != chanspec) {
		int idx;
		wlc_bsscfg_t *cfg;
#if defined(BGDFS) || defined(WL_AIR_IQ)
		phy_info_t *phyi = (phy_info_t *) WLC_PI(wlc);
		/* ensure that phymode is reset (scan core is disabled) before chspec change */
		if (PHYMODE(wlc) == PHYMODE_BGDFS) {
			phy_ac_chanmgr_set_val_phymode(PHY_AC_CHANMGR(phyi), 0);
		}
#endif /* BGDFS || WL_AIR_IQ */

		WL_INFORM(("wl%d: change shared chanspec wlc->home_chanspec from "
		           "0x%04x to 0x%04x\n", wlc->pub->unit, wlc->home_chanspec, chanspec));

		wlc->home_chanspec = chanspec;
		if (BSSCFG_STA(wlc->cfg)) {
			wlc->cfg->target_bss->chanspec = chanspec;
			wlc_bsscfg_set_current_bss_chan(wlc->cfg, chanspec);
		}

		FOREACH_UP_AP(wlc, idx, cfg) {

#ifdef WLMCHAN
			if (!MCHAN_ENAB(wlc->pub) ||
				(wlc_get_chanspec(wlc, cfg) == chanspec))
#endif // endif
			{
				cfg->target_bss->chanspec = chanspec;
				wlc_bsscfg_set_current_bss_chan(cfg, chanspec);
			}
		}

		if (WLCISNPHY(wlc->band)) {
			wlc_phy_t *pi = WLC_PI(wlc);
			wlc_phy_interference_set(pi, TRUE);

			wlc_phy_acimode_noisemode_reset(pi,
				chanspec, TRUE, TRUE, FALSE);
		}
	}
}

/** full phy cal when start or join a network */
void
wlc_full_phy_cal(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint8 reason)
{
	int idx;
	wlc_bsscfg_t *cfg;
	chanspec_t chanspec;

	chanspec = bsscfg->associated ?
		bsscfg->current_bss->chanspec : bsscfg->target_bss->chanspec;

	/* check if channel in bsscfg is a channel to share */
	FOREACH_BSS(wlc, idx, cfg) {
		if (!cfg->associated || cfg == bsscfg)
			continue;
		if (cfg->current_bss->chanspec == chanspec)
			/* skip full phy cal if already did by other interface */
			return;
	}

	wlc_phy_cal_perical(WLC_PI(wlc), reason);
}

static void
wlc_set_phy_chanspec(wlc_info_t *wlc, chanspec_t chanspec)
{
	wlc_phy_t *pi;
	BCM_REFERENCE(pi);

#ifdef FCC_PWR_LIMIT_2G
	if (FCC_PWR_LIMIT_2G_ENAB(wlc->pub)) {
		wlc_phy_prev_chanspec_set(WLC_PI(wlc), wlc->chanspec);
	}
#endif /* FCC_PWR_LIMIT_2G */
	/* Save our copy of the chanspec */
	wlc->chanspec = chanspec;
	if (wlc->default_bss) {
		wlc->default_bss->chanspec = chanspec;
	}

	WL_TSLOG(wlc, __FUNCTION__, TS_ENTER, 0);
	/* Set the chanspec and power limits for this locale.
	 * Any 11h local tx power constraints will be retrieved
	 * by the chanspec set function once the regulatory max
	 * has been established.
	 */
	wlc_channel_set_chanspec(wlc->cmi, chanspec);
#ifdef WL11AC
	wlc_stf_chanspec_upd(wlc);
#endif /* WL11AC */
#ifdef WL_MUSCHEDULER
	wlc_musched_chanspec_upd(wlc);
#endif /* WL_MUSCHEDULER */
#ifdef WL11N
#ifdef SRHWVSDB
	if (SRHWVSDB_ENAB(wlc->pub)) {
		wlc_srvsdb_stf_ss_algo_chan_get(wlc, chanspec);
	}
	else
#endif // endif
	if (wlc->stf->ss_algosel_auto) {
		/* following code brings ~3ms delay for split driver */
		wlc_stf_ss_algo_channel_get(wlc, &wlc->stf->ss_algo_channel, chanspec);
	}
	wlc_stf_ss_update(wlc, wlc->band);
	wlc_bmac_txbw_update(wlc->hw);
#endif /* WL11N */
#ifdef WL_BEAMFORMING
	wlc_txbf_impbf_upd(wlc->txbf);
#endif // endif
	WL_TSLOG(wlc, __FUNCTION__, TS_EXIT, 0);
} /* wlc_set_phy_chanspec */

void
wlc_set_chanspec(wlc_info_t *wlc, chanspec_t chanspec, int reason_bitmap)
{
	uint bandunit;
	uint32 tsf_l;
#if defined(BCMDBG) || defined(BCMDBG_ERR)
	char chanbuf1[CHANSPEC_STR_LEN];
#endif // endif
	DBGONLY(char chanbuf2[CHANSPEC_STR_LEN]; )
	chanspec_t old_chanspec = wlc->chanspec;
#if defined(CCA_STATS)
	bool skip_cca = FALSE;
#if !defined(DONGLEBUILD)
	int idx;
	wlc_bsscfg_t *cfg;
#endif /* !DONGLEBUILD */
#endif /* CCA_STATS */
#ifdef WLCHANIM
	int err;
#endif /* WLCHANIM */
#if defined(BGDFS) || defined(WL_AIR_IQ)
	phy_info_t *pi = (phy_info_t *) WLC_PI(wlc);
#endif /* BGDFS  || WL_AIR_IQ */
#ifdef WL_UCM
	wlc_bsscfg_t *ucm_bsscfg = NULL;
#endif /* WL_UCM */

	WL_TSLOG(wlc, "wlc_set_chanspec from => to", wlc->chanspec, chanspec);
	WL_TSLOG(wlc, __FUNCTION__, TS_ENTER, 0);
#if defined(BGDFS) || defined(WL_AIR_IQ)
	/* always ensure that phymode is reset (scan core is disabled) before chspec change */
	if ((PHYMODE(wlc) == PHYMODE_BGDFS)) {
		phy_ac_chanmgr_set_val_phymode(PHY_AC_CHANMGR(pi), 0);
	}
#endif /* BGDFS  || WL_AIR_IQ */

	tsf_l = R_REG(wlc->osh, D11_TSFTimerLow(wlc));
	if (WL_MCHAN_ON()) {
		WL_MCHAN(("wl%d: %s - changing chanspec from %s to %s\n",
			wlc->pub->unit, __FUNCTION__,
			wf_chspec_ntoa_ex(wlc->chanspec, chanbuf1),
			wf_chspec_ntoa_ex(chanspec, chanbuf2)));
	}
	if (!WLC_CNTRY_DEFAULT_ENAB(wlc) && !wlc_valid_chanspec_db(wlc->cmi, chanspec) &&
		!(wlc->scan->state & SCAN_STATE_PROHIBIT)) {
		WL_ERROR(("wl%d: %s: Bad chanspec %s\n",
			wlc->pub->unit, __FUNCTION__, wf_chspec_ntoa_ex(chanspec, chanbuf1)));
		goto set_chanspec_done;
	}

#ifdef CCA_STATS
#ifndef DONGLEBUILD
	/* to speed up roaming process, especially in dongle case, do not read
	* counters from shared memory for cca during roaming scan
	*/
	FOREACH_AS_STA(wlc, idx, cfg) {
		if (cfg->BSS && cfg->assoc->type == AS_ROAM) {
			skip_cca = TRUE;
			break;
		}
	}
#endif /* !DONGLEBUILD */
	if (wlc->hw->reinit)
		skip_cca = TRUE;

	/* About to leave this channel, so calculate cca stats from this channel */
	if (WL_CCA_STATS_ENAB(wlc->pub) && (!skip_cca)) {
		cca_stats_upd(wlc, 1);
		if (SCAN_IN_PROGRESS(wlc->scan))
			cca_send_event(wlc, 1);
	}
#endif /* CCA_STATS */
#ifdef WLCHANIM
	if (WLC_CHANIM_ENAB(wlc->pub)) {
		if ((err = wlc_lq_chanim_update(wlc, chanspec, CHANIM_CHANSPEC)) != BCME_OK) {
			WL_ERROR(("wl%d: %s: WLC_CHANIM upd fail err: %d\n", wlc->pub->unit,
				__FUNCTION__, err));
		}
	}
#endif /* WLCHANIM */

	if (old_chanspec == chanspec) {
		WL_TSLOG(wlc, __FUNCTION__, TS_EXIT, 0);
		return;
	}

#ifdef WLBTCPROF
	if (wlc_btcx_set_btc_profile_param(wlc, chanspec, TRUE))
	{
		WL_ERROR(("wl%d: %s: BTCOEX settings error: chspec %d!\n",
			wlc->pub->unit, __FUNCTION__, CHSPEC_CHANNEL(chanspec)));
	}
#endif /* WLBTCPROF */
#if defined(WL_PWRSTATS)
	if (PWRSTATS_ENAB(wlc->pub)) {
		if (wlc_mimo_siso_metrics_report(wlc, TRUE)) {
			WL_ERROR(("wl%d: %s: MIMO SISO metrics report error!\n",
				wlc->pub->unit, __FUNCTION__));
		}
	}
#endif /* WL_PWRSTATS */

	/* Switch bands if necessary */
	if (NBANDS(wlc) > 1) {
		bandunit = CHSPEC_WLCBANDUNIT(chanspec);
		if (wlc->band->bandunit != bandunit || wlc->bandinit_pending) {
			if (wlc->bandlocked) {
				WL_ERROR(("wl%d: %s: chspec %s band is locked!\n",
					wlc->pub->unit, __FUNCTION__,
					wf_chspec_ntoa_ex(chanspec, chanbuf1)));
				goto set_chanspec_done;
			}
#ifdef WL_BTCDYN
			/*  notify BTCOEX about chanspec change   */
			if (BTCDYN_ENAB(wlc->pub)) {
				wlc_btcx_chspec_change_notify(wlc, chanspec);
			}
#endif // endif
			/* BMAC_NOTE: should the setband call come after the wlc_bmac_chanspec() ?
			 * if the setband updates (wlc_bsinit) use low level calls to inspect and
			 * set state, the state inspected may be from the wrong band, or the
			 * following wlc_bmac_set_chanspec() may undo the work.
			 */
			wlc_setband(wlc, bandunit);
		}
	}
	ASSERT((VHT_ENAB_BAND(wlc->pub, (CHSPEC_IS2G(chanspec) ? WLC_BAND_2G : WLC_BAND_5G)) ||
		CHSPEC_ISLE20(chanspec) || CHSPEC_IS40(chanspec)));
	ASSERT(N_ENAB(wlc->pub) || CHSPEC_ISLE20(chanspec));
#if defined(WL_UCM)
	/* Update & Apply chainmask requested by BTCOEX before Phy calibrates */
	if (CHSPEC_IS2G(chanspec)) {
		wlc_btc_2g_chain_request_apply(wlc);
	} else {
		wlc_btc_chain_request_cancel(wlc);
	}
#endif  /* defined(WL_UCM) */
	/* sync up phy/radio chanspec */
	wlc_set_phy_chanspec(wlc, chanspec);

#ifdef WL11N
	/* update state that depends on channel bandwidth */
	if (CHSPEC_WLC_BW(old_chanspec) != CHSPEC_WLC_BW(chanspec)) {
		/* init antenna selection */
		if (WLANTSEL_ENAB(wlc))
			wlc_antsel_init(wlc->asi);

		/* Fix the hardware rateset based on bw.
		 * Mainly add MCS32 for 40Mhz, remove MCS 32 for 20Mhz
		 */
		wlc_rateset_bw_mcs_filter(&wlc->band->hw_rateset, CHSPEC_WLC_BW(chanspec));
	}
#endif /* WL11N */
	/* update some mac configuration since chanspec changed */
	wlc_ucode_mac_upd(wlc);
#ifdef WL_UCM
	if (UCM_ENAB(wlc->pub)) {
		wlc_btc_get_matched_chanspec_bsscfg(wlc, chanspec, &ucm_bsscfg);
		if (wlc_btc_apply_profile(wlc, ucm_bsscfg) == BCME_ERROR) {
			WL_ERROR(("wl%d: %s: BTCOEX settings error: chspec %d!\n",
				wlc->pub->unit, __FUNCTION__, CHSPEC_CHANNEL(chanspec)));
			ASSERT(0);
		}
	}
#endif /* WL_UCM */
#ifdef CCA_STATS
	/* update cca time */
	if (!skip_cca)
		cca_stats_upd(wlc, 0);
#endif // endif

	/* invalidate txcache as transmit b/w may have changed */
	if (N_ENAB(wlc->pub) &&
	    WLC_TXC_ENAB(wlc))
		wlc_txc_inv_all(wlc->txc);

	/* update ED/CRS settings */
	wlc_bmac_ifsctl_edcrs_set(wlc->hw, FALSE);

set_chanspec_done:
	wlc_chansw_notif(wlc, reason_bitmap, old_chanspec, chanspec, tsf_l);

#ifdef BCMLTECOEX
	/* Update LTECX states on Channel Switch */
	if (BCMLTECOEX_ENAB(wlc->pub)) {
		wlc_ltecx_update_all_states(wlc->ltecx);
	}
#endif /* BCMLTECOEX */
#ifdef WLOLPC
	if (OLPC_ENAB(wlc)) {
	/* we migrated to a new channel; check for phy cal need (and do it) */
	/* also, terminate cal if needed */
		wlc_olpc_eng_hdl_chan_update(wlc->olpc_info);
	}
#endif /* OPEN LOOP POWER CAL */
#ifdef WL_MIMOPS_CFG
	if (WLC_MIMOPS_ENAB(wlc->pub)) {
		wlc_update_mimops_cfg_transition(wlc);
	}
#endif /* WL_MMOPS_CFG */
	WL_TSLOG(wlc, __FUNCTION__, TS_EXIT, 0);
} /* wlc_set_chanspec */

#ifdef PHYCAL_CACHING
int
wlc_set_operchan(wlc_info_t *wlc, chanspec_t chanspec)
{
		phy_info_t *pi = (phy_info_t *) WLC_PI(wlc);
		return phy_chanmgr_set_oper(pi, chanspec);
}
#endif /* PHYCAL_CACHING */

int
wlc_get_last_txpwr(wlc_info_t *wlc, wlc_txchain_pwr_t *last_pwr)
{
	uint i;
	uint8 est_Pout[WLC_NUM_TXCHAIN_MAX];
	uint8 est_Pout_act[WLC_NUM_TXCHAIN_MAX];
	uint8 est_Pout_cck;
	int ret;

	if (!wlc->pub->up)
		return BCME_NOTUP;

	bzero(est_Pout, WLC_NUM_TXCHAIN_MAX);

	if ((ret = wlc_phy_get_est_pout(WLC_PI(wlc),
		est_Pout, est_Pout_act, &est_Pout_cck)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: PHY func fail: %d\n",
			wlc->pub->unit,
			__FUNCTION__,
			ret));
		return ret;
	}

	for (i = 0; i < WLC_NUM_TXCHAIN_MAX; i++) {
		last_pwr->chain[i] = est_Pout[i];
	}

	return BCME_OK;
}

#ifdef STA
#ifdef PHYCAL_CACHING
static int
wlc_cache_cals(wlc_info_t *wlc)
{
	chanspec_t chanspec = wlc_default_chanspec(wlc->cmi, TRUE);
	uint i;
	char abbrev[WLC_CNTRY_BUF_SZ];
	wl_uint32_list_t *list;

	if (mboolisset(wlc->pub->radio_disabled, WL_RADIO_HW_DISABLE) ||
	    mboolisset(wlc->pub->radio_disabled, WL_RADIO_SW_DISABLE))
		return BCME_RADIOOFF;

	list = (wl_uint32_list_t *)MALLOC(wlc->osh, (WL_NUMCHANSPECS+1) * sizeof(uint32));

	if (!list) {
		WL_ERROR(("wl%d: %s: out of mem, %d bytes malloced\n", wlc->pub->unit,
		          __FUNCTION__, MALLOCED(wlc->osh)));
		return BCME_NOMEM;
	}

	bzero(list, (WL_NUMCHANSPECS+1) * sizeof(uint32));
	bzero(abbrev, WLC_CNTRY_BUF_SZ);
	list->count = 0;
	wlc_get_valid_chanspecs(wlc->cmi, list, WL_CHANSPEC_BW_20, TRUE, abbrev);
	wlc_get_valid_chanspecs(wlc->cmi, list, WL_CHANSPEC_BW_20, FALSE, abbrev);

	/* 20MHz only for now
	 * wlc_get_valid_chanspecs(wlc->cmi, list, WL_CHANSPEC_BW_40, TRUE, abbrev);
	 * wlc_get_valid_chanspecs(wlc->cmi, list, WL_CHANSPEC_BW_40, FALSE, abbrev);
	 */

	/* need this to be set */
	wlc_iovar_setint(wlc, "phy_percal", PHY_PERICAL_SPHASE);

	/* we'll assert in PHY code if we are in MPC */
	wlc->mpc = FALSE;
	wlc_radio_mpc_upd(wlc);

	wl_up(wlc->wl);

	wlc_bmac_set_phycal_cache_flag(wlc->hw, TRUE);

	wlc_suspend_mac_and_wait(wlc);

	for (i = 0; i < list->count; i++) {
		wlc_phy_t *pi = WLC_PI(wlc);
		chanspec = (chanspec_t) list->element[i];
		WL_INFORM(("wl%d: %s: setting chanspec to 0x%04x and running a periodic cal\n",
		           wlc->pub->unit, __FUNCTION__, chanspec));
		phy_chanmgr_create_ctx((phy_info_t *) pi, chanspec);
		wlc_set_chanspec(wlc, chanspec, CHANSW_REASON(CHANSW_PHYCAL));
		wlc_phy_cal_perical(pi, PHY_PERICAL_JOIN_BSS);
	}

	wlc_enable_mac(wlc);

	wlc_iovar_setint(wlc, "phy_percal", PHY_PERICAL_MANUAL);

	if (list)
		MFREE(wlc->osh, list, (WL_NUMCHANSPECS+1) * sizeof(uint32));

	return BCME_OK;
} /* wlc_cache_cals */

static int
wlc_cachedcal_sweep(wlc_info_t *wlc)
{
	chanspec_t chanspec = wlc_default_chanspec(wlc->cmi, TRUE);
	uint i;
	char abbrev[WLC_CNTRY_BUF_SZ];
	wl_uint32_list_t *list = (wl_uint32_list_t *)
	        MALLOC(wlc->osh, (WL_NUMCHANSPECS+1) * sizeof(uint32));

	if (!list) {
		WL_ERROR(("wl%d: %s: out of mem, %d bytes malloced\n", wlc->pub->unit,
		          __FUNCTION__, MALLOCED(wlc->osh)));
		return BCME_NOMEM;
	}

	bzero(list, (WL_NUMCHANSPECS+1) * sizeof(uint32));
	bzero(abbrev, WLC_CNTRY_BUF_SZ);
	list->count = 0;

	wlc_get_valid_chanspecs(wlc->cmi, list, WL_CHANSPEC_BW_20, TRUE, abbrev);
	wlc_get_valid_chanspecs(wlc->cmi, list, WL_CHANSPEC_BW_20, FALSE, abbrev);

	/* 20MHz only for now
	 * wlc_get_valid_chanspecs(wlc->cmi, list, WL_CHANSPEC_BW_40, TRUE, abbrev);
	 * wlc_get_valid_chanspecs(wlc->cmi, list, WL_CHANSPEC_BW_40, FALSE, abbrev);
	 */

	/* we'll assert in PHY code if we are in MPC */
	wlc->mpc = FALSE;
	wlc_radio_mpc_upd(wlc);

	wlc_suspend_mac_and_wait(wlc);

	for (i = 0; i < list->count; i++) {
		wlc_phy_t *pi = WLC_PI(wlc);
		chanspec = (chanspec_t) list->element[i];
		WL_INFORM(("wl%d: %s: setting chanspec to 0x%04x and running a periodic cal\n",
		           wlc->pub->unit, __FUNCTION__, chanspec));
		phy_chanmgr_create_ctx((phy_info_t *) pi, chanspec);
		wlc_set_chanspec(wlc, chanspec, CHANSW_REASON(CHANSW_PHYCAL));
		wlc_phy_cal_perical(pi, PHY_PERICAL_JOIN_BSS);
	}

	wlc_enable_mac(wlc);

	if (list)
		MFREE(wlc->osh, list, (WL_NUMCHANSPECS+1) * sizeof(uint32));

	return BCME_OK;
} /* wlc_cachedcal_sweep */

static int
wlc_cachedcal_tune(wlc_info_t *wlc, uint16 channel)
{
	uint16 chanspec = (channel | WL_CHANSPEC_BW_20);
	channel <= CH_MAX_2G_CHANNEL ? (chanspec |= WL_CHANSPEC_BAND_2G) :
	        (chanspec |= WL_CHANSPEC_BAND_5G);

	if (wf_chspec_malformed(chanspec)) {
		WL_ERROR(("wl%d: %s: Malformed chanspec 0x%x\n", wlc->pub->unit, __FUNCTION__,
		          chanspec));
		return BCME_BADCHAN;
	}

	if (!VALID_CHANNEL20_IN_BAND(wlc, CHANNEL_BANDUNIT(wlc, channel), channel)) {
		WL_ERROR(("wl%d: %s: Bad channel %d, chanspec(0x%04x)\n", wlc->pub->unit,
			__FUNCTION__, channel, chanspec));
		return BCME_BADCHAN;
	}

	/* we'll assert in PHY code if we are in MPC */
	wlc->mpc = FALSE;
	wlc_radio_mpc_upd(wlc);

	wlc_suspend_mac_and_wait(wlc);
	WL_INFORM(("wl%d: %s: setting chanspec to 0x%04x and running a periodic cal\n",
	          wlc->pub->unit, __FUNCTION__, chanspec));
	wlc_set_chanspec(wlc, chanspec, CHANSW_REASON(CHANSW_PHYCAL));
	wlc_phy_cal_perical(WLC_PI(wlc), PHY_PERICAL_JOIN_BSS);
	wlc_enable_mac(wlc);

	return BCME_OK;
}
#endif /* PHYCAL_CACHING */
#endif	/* STA */

/** common AP/STA funnel function to perform operations when scb disassocs */
void
wlc_scb_disassoc_cleanup(wlc_info_t *wlc, struct scb *scb)
{
	wlc_bsscfg_t *bsscfg;
#if defined(BCMDBG) || defined(WLMSG_WSEC)
	char eabuf[ETHER_ADDR_STR_LEN];

	bcm_ether_ntoa(&scb->ea, eabuf);
#endif // endif

	if (SCB_MFP(scb)) {
		/* flush all outstanding MFP packets for this SCB before deleting keys */
		wlc_txq_scb_remove_mfp_frames(wlc, scb);
	}

	/* reset pairwise key on disassociate */
	wlc_keymgmt_reset(wlc->keymgmt, NULL, scb);

	bsscfg = SCB_BSSCFG(scb);
	/* delete *all* group keys on disassociate in STA mode with wpa */
	if (BSSCFG_STA(bsscfg) && (bsscfg->WPA_auth != WPA_AUTH_DISABLED) &&
		(bsscfg->WPA_auth != WPA_AUTH_NONE)) {
		wlc_keymgmt_reset(wlc->keymgmt, bsscfg, NULL);
	}

	wlc_set_ps_ctrl(bsscfg);

	scb->flags &= ~SCB_PENDING_PSPOLL;

	if (!SCB_LEGACY_WDS(scb)) {
#ifdef WLAMPDU
		/* cleanup ampdu at end of association */
		if (SCB_AMPDU(scb)) {
			WL_AMPDU(("wl%d: scb ampdu cleanup for %s\n", wlc->pub->unit, eabuf));
			scb_ampdu_cleanup(wlc, scb);
		}
#endif // endif
	}

	if (BSSCFG_AP(SCB_BSSCFG(scb))) {
		/* If the STA has been configured to be monitored before,
		 * association then continue sniffing that STA frames.
		 */
		if (STAMON_ENAB(wlc->pub) && STA_MONITORING(wlc, &scb->ea))
			wlc_stamon_sta_sniff_enab(wlc->stamon_info, &scb->ea, TRUE);

		/* Free any saved wpaie */
		wlc_scb_save_wpa_ie(wlc, scb, NULL);
	}

	/* Reset PS state if needed */
	if (SCB_PS(scb))
		wlc_apps_scb_ps_off(wlc, scb, TRUE);
#ifdef STA
	/* By now STA must have been disassociated, pcie link can be put in L1.2 */
	wlc_cfg_set_pmstate_upd(scb->bsscfg, TRUE);
#endif // endif
	scb->flags2 &= ~(SCB2_MFP|SCB2_SHA256);

#ifdef WLWNM
	if (WLWNM_ENAB(wlc->pub))
		wlc_wnm_scb_cleanup(wlc, scb);
#endif // endif

	/* cleanup the a4-data frames flag */
	if (!SCB_LEGACY_WDS(scb))
		SCB_A4_DATA_DISABLE(scb);

#ifdef DWDS
	if (SCB_DWDS(scb))
		SCB_DWDS_DEACTIVATE(scb);
#endif // endif

#if defined(WL_PWRSTATS)
	/* Update connect time for primary infra STA only */
	if (PWRSTATS_ENAB(wlc->pub) && (bsscfg == wlc->cfg) &&
		BSSCFG_STA(bsscfg) && bsscfg->BSS)
		wlc_connect_time_upd(wlc);
#endif /* WL_PWRSTATS */
} /* wlc_scb_disassoc_cleanup */

#ifdef STA
/** Configure the WMM U-APSD trigger frame timer */
int
wlc_apsd_trigger_upd(wlc_bsscfg_t *cfg, bool allow)
{
	wlc_info_t *wlc = cfg->wlc;
	struct scb *scb;
	wlc_pm_st_t *pm = cfg->pm;
	int callbacks = 0;

	WL_TRACE(("wl%d: %s : state %d, timeout %d\n",
		wlc->pub->unit, __FUNCTION__, allow, pm->apsd_trigger_timeout));

	if (pm->apsd_trigger_timer == NULL) {
		WL_ERROR(("wl%d.%d: %s: Trying to update NULL apsd trigger timer.\n",
		wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
		return 0;
	}

	if (!wl_del_timer(wlc->wl, pm->apsd_trigger_timer))
		callbacks ++;

	if (!allow || !cfg->associated || !pm->PMenabled ||
	    pm->apsd_trigger_timeout == 0)
		return callbacks;

	/*
	 * - check for WMM
	 * - check for APSD trigger- & delivery- enabled ACs
	 * - set timeout
	 * - enable timer as necessary
	 */
	scb = wlc_scbfindband(wlc, cfg, &cfg->BSSID,
	                      CHSPEC_WLCBANDUNIT(cfg->current_bss->chanspec));
	if (scb != NULL && SCB_WME(scb) && scb->apsd.ac_trig) {
		/* set timer for half the requested period so that we send frame
		 * only if there was no tx activity (in the APSD ac for the last
		 * 1/2 of the trigger timeout period.
		 */
		wl_add_timer(wlc->wl, pm->apsd_trigger_timer, pm->apsd_trigger_timeout / 2, TRUE);
	} else {
		WL_PS(("wl%d.%d: %s: apsd trigger timer not set\n",
		       wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
	}

	return 0;
} /* wlc_apsd_trigger_upd */

void
wlc_bss_clear_bssid(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	bool prev_tbtt = WLC_WATCHDOG_TBTT(wlc);
	bool cur_tbtt;

	bzero(&cfg->BSSID, ETHER_ADDR_LEN);
	bzero(&cfg->current_bss->BSSID, ETHER_ADDR_LEN);
	wlc_clear_bssid(cfg);

	wlc->stas_connected = wlc_stas_connected(wlc);

	/*
	* update the watchdog depending on current alignment state
	*/
	cur_tbtt = WLC_WATCHDOG_TBTT(wlc);
	if (cur_tbtt != prev_tbtt) {
		wlc_watchdog_upd(cfg, cur_tbtt);
	}

	wlc_btc_set_ps_protection(wlc, cfg); /* disable */

	if (cfg->pm->PMpending)
		wlc_update_pmstate(cfg, TX_STATUS_BE);

	/* Clear the quiet flag and transfer the packets */
	wlc_bsscfg_tx_start(wlc->psqi, cfg);
}

uint32
wlc_watchdog_backup_bi(wlc_info_t * wlc)
{
	return (2 * TIMER_INTERVAL_WATCHDOG);
}

/**
 * Change to run the watchdog either from a periodic timer or from tbtt handler.
 * Call watchdog from tbtt handler if tbtt is TRUE, watchdog timer otherwise.
 */
void
wlc_watchdog_upd(wlc_bsscfg_t *cfg, bool tbtt)
{
	wlc_info_t *wlc = cfg->wlc;
	uint32 wdtimer_delay = TIMER_INTERVAL_WATCHDOG;

	/* make sure changing watchdog driver is allowed */
	if (!wlc->pub->up)
		return;
	if (!wlc->pub->align_wd_tbtt) {
#ifdef LPAS
		/* NIC builds do not have align_wd_tbtt set to TRUE and may not
		 * expect wdog timer to be restarted everytime wlc_watchdog_upd is invoked.
		 */
		if (wlc->lpas == 1) {
			wlc_refresh_wd_timer(wlc, TIMER_INTERVAL_WATCHDOG, TRUE);
			WL_INFORM(("wl%d: wlc_watchdog_upd: align_wd disallowed\n",
				WLCWLUNIT(wlc)));
		}
#endif /* LPAS */
		return;
	}

	if (!tbtt) {
		/* arm watchdog timer and drive the watchdog there */
		wlc_watchdog_disable_ucode_sleep_intr(wlc);
		mboolclr(wlc->wd_state, WD_DEFERRED_PM0_BCNRX | DEFERRED_WLC_WD);
	}
	else {
		/* use tbtt interrupt to drive watchdog. schedule backup */
		wdtimer_delay = wlc_watchdog_backup_bi(wlc);
	}
	wlc_refresh_wd_timer(wlc, wdtimer_delay, TRUE);
} /* wlc_watchdog_upd */
#endif /* STA */

/** return true if the rateset contains an OFDM rate */
bool
wlc_rateset_isofdm(uint count, uint8 *rates)
{
	int i;
	for (i = count - 1; i >= 0; i--)
		if (RATE_ISOFDM(rates[i]))
			return (TRUE);
	return (FALSE);
}

void
wlc_BSSinit_rateset_filter(wlc_bsscfg_t *cfg)
{
	uint i, val;
	wlc_info_t *wlc = cfg->wlc;
	wlc_bss_info_t *current_bss = cfg->current_bss;

	/* Japan Channel 14 restrictions */
	if ((wf_chspec_ctlchan(WLC_BAND_PI_RADIO_CHANSPEC) == 14) && (wlc_japan(wlc) == TRUE)) {
		/* Japan does not allow OFDM or MIMO on channel 14 */
		if (wlc->band->gmode) {
			/* filter-out OFDM rates and MCS set */
			wlc_rateset_filter(&current_bss->rateset, &current_bss->rateset,
			                   FALSE, WLC_RATES_CCK, RATE_MASK_FULL, 0);
		}
	}

	/* filter-out unsupported rates and store in current_bss->rateset */
	wlc_rate_hwrs_filter_sort_validate(&current_bss->rateset,
		&wlc->band->hw_rateset, FALSE, wlc->stf->op_txstreams);

	if (BAND_2G(wlc->band->bandtype)) {
		if (wlc->band->gmode == GMODE_LEGACY_B) {
			/* Keep only CCK if gmode == GMODE_LEGACY_B */
			wlc_rateset_filter(&current_bss->rateset, &current_bss->rateset,
				FALSE, WLC_RATES_CCK, RATE_MASK_FULL, 0);
		} else if (wlc->band->gmode == GMODE_ONLY) {
			/* Keep CCK and OFDM if gmode == GMODE_ONLY */
			wlc_rateset_filter(&current_bss->rateset, &current_bss->rateset,
				FALSE, WLC_RATES_OFDM, RATE_MASK_FULL, 0);
		} else if (wlc->band->gmode == GMODE_AUTO) {
			/* Keep CCK, OFDM 11N MCS only if gmode == GMODE_ONLY
			 * This configuration will make sure VHT MCS is removed from rateset
			 */
			wlc_rateset_filter(&current_bss->rateset, &current_bss->rateset,
				FALSE, WLC_RATES_CCK_OFDM, RATE_MASK_FULL,
				wlc_get_mcsallow(wlc, cfg));
		}
	}

	/* if empty or no basic rates - use driver default rateset */
	/* ??? should fixup all WLC_RATE_FLAG reference to remove this requirement */
	val = 0;
	for (i = 0; i < current_bss->rateset.count; i++)
		if (current_bss->rateset.rates[i] & WLC_RATE_FLAG)
			val = 1;

	if (val == 0) {
		wlc_default_rateset(wlc, &current_bss->rateset);
		/* Keep only CCK if gmode == GMODE_LEGACY_B */
		if (BAND_2G(wlc->band->bandtype) && (wlc->band->gmode == GMODE_LEGACY_B))
			wlc_rateset_filter(&current_bss->rateset, &current_bss->rateset,
			                   FALSE, WLC_RATES_CCK, RATE_MASK_FULL, 0);
	}

	/* OFDM rates only */
	if (
#if defined(WLP2P)
	    BSS_P2P_ENAB(wlc, cfg) ||
#endif // endif
#if defined(AP)
	    (cfg->rateset == WLC_BSSCFG_RATESET_OFDM) ||
#endif // endif
	    FALSE) {
		wlc_rateset_filter(&current_bss->rateset, &current_bss->rateset, FALSE,
		                   WLC_RATES_OFDM, RATE_MASK_FULL, wlc_get_mcsallow(wlc, cfg));
	}

	wlc_rate_lookup_init(wlc, &current_bss->rateset);
} /* wlc_BSSinit_rateset_filter */

/**
 * Minimum pretbtt time required for PHY specific power up.  This time does not
 *   include the preceeding time necessary to power up the non phy related
 *   clocks, etc.  This time is SYNTHPU_* and is separately communicated to the
 *   ucode via shared memory.  Ucode adds SYNTHPU and PRETBTT to determine
 *   when to wake for a TBTT.  The pretbtt interrupt is given by ucode to the
 *   driver at PRETBTT time befoe TBTT, not when it first wakes up.
 */
/* XXX Note: Currently legacy/non-p2p ucode assumes SYNTHPU_* includes the pretbtt
 *   which effectively means that your can't lengthen the pretbtt time since SYNTHPU
 *   is written independent of PRETBTT and is a PHY specific constant.
 *   PHYs when used with P2P ucode will end up having an additional/duplicate
 *   PRETBTT delay because it is also built into each PHY specific SYNTHPU constant.
 *
 *   There is an effort to unify both sets of ucode so that SYNTHPU doesn't include the
 *   pretbtt value.  As of the time of this commit this unification is just in the Aardvark
 *   branch.
 *
 *  *** Remove this note when SYNTHPU unification is done on trunk ***
 */
static uint
wlc_pretbtt_min(wlc_info_t *wlc)
{
	wlcband_t *band = wlc->band;
	uint pretbtt;

	if (ISSIM_ENAB(wlc->pub->sih))
		pretbtt = PRETBTT_PHY_US_QT;
	else if (WLCISNPHY(band)) {
		pretbtt = PRETBTT_NPHY_US;
	} else if (WLCISACPHY(band)) {
		if (CHIPID(wlc->pub->sih->chip) == BCM4360_CHIP_ID) {
			pretbtt = PRETBTT_ACPHY_4360_US;
		} else {
			pretbtt = PRETBTT_ACPHY_US;
		}
	} else if (WLCISLCN20PHY(band)) {
		pretbtt = PRETBTT_LCN20PHY_US;
	} else
		pretbtt = PRETBTT_BPHY_US;

	return pretbtt;
}

/** Update HW PRETBTT */
static uint16
wlc_pretbtt_calc_hw(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	uint mbss_pretbtt = 0;
	uint bss_pretbtt;
	uint pretbtt;

	/* MBSS pretbtt needs */
	if (MBSS_ENAB(wlc->pub) && BSSCFG_AP(cfg))
		mbss_pretbtt = MBSS_PRE_TBTT_DEFAULT_us;

	/* Normal BSS (i.e. non-MBSS) phy specific pretbtt value.  See wlc_pretbtt_min() functions
	 *   comment for more details on exactly for a definition of this value and how it is
	 *   used by the ucode.
	 */
	bss_pretbtt = wlc_pretbtt_min(wlc);

	/* Note: Please add any chip specific setting to wlc_pretbtt_min() */
	if (WLCISACPHY(wlc->band)) {
		if (!MBSS_ENAB(wlc->pub) && BSSCFG_AP(cfg)) {
			bss_pretbtt = PRETBTT_ACPHY_AP_US;
		}
		/* Note: Please add any chip specific setting to wlc_pretbtt_min() */
	}
	/* Note: Please add any chip specific setting to wlc_pretbtt_min() */

	/* THIS IS THE MINIMUM PRETBTT REQUIREMENT */
	pretbtt = MAX(mbss_pretbtt, bss_pretbtt);
	if (pretbtt == 0)
		pretbtt = 2;

	return (uint16)pretbtt;
} /* wlc_pretbtt_calc_hw */

/* This function returns the maximum channel switch time taken for a particular chip.
 * This includes, mac suspend, set chanspec and mac enable.
 * OR,
 * on MSCH implementation, it is the maxinum time taken by _msch_chan_adopt()
 * to adopt to a new channel.
 */
uint16 wlc_proc_time_us(wlc_info_t *wlc)
{
	uint16 chan_sw_time = 0;

	switch (CHIPID(wlc->pub->sih->chip)) {
		default:
			/* Default value */
			chan_sw_time = 3000;
			break;
	};
	return chan_sw_time;
}

/** update PRETBTT: use the maximum value of all needs */
/* XXX Look at the given bsscfg's states as well as other bsscfgs' states to
 * decide what to do.
 */
uint16
wlc_pretbtt_calc(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	uint pretbtt;

	pretbtt = wlc_pretbtt_calc_hw(cfg);

#ifdef WLTDLS
	if (TDLS_ENAB(wlc->pub) && wlc_tdls_is_active(wlc)) {
		pretbtt += wlc_tdls_get_pretbtt_time(wlc->tdls);
	} else
#endif // endif
	{
#ifdef WLMCHAN
		if (MCHAN_ENAB(wlc->pub)) {
			pretbtt += wlc_mchan_get_pretbtt_time(wlc->mchan);
		}
#endif // endif
	}
	/* per BSS pretbtt, pick a maximum value amongst all registered clients */
	pretbtt = wlc_bss_pretbtt_query(wlc, cfg, pretbtt);
	WL_INFORM(("wl%d.%d: %s: pretbtt = %d\n",
		wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__, pretbtt));

	return (uint16)pretbtt;
}

/** Set pre Beacon Transmission Time. Mac is assumed to be suspended at this point */
void
wlc_pretbtt_set(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	uint16 phyctl;
	UNUSED_PARAMETER(phyctl);

#ifdef WLMCNX
	/* STA uses per BSS PreTBTT setting when p2p ucode is used */
	/* When WOWL is active, pretbtt needs to be set. */
	if (MCNX_ENAB(wlc->pub) && !BSSCFG_AP(cfg) && !WOWL_ACTIVE(wlc->pub))
		return;
#endif // endif
	phyctl = wlc_pretbtt_calc_hw(cfg);
	W_REG(wlc->osh, D11_TSF_CFP_PRE_TBTT(wlc), phyctl);	/* IHR */
	wlc_write_shm(wlc, M_PRETBTT(wlc), phyctl);		/* SHM */
}

void
wlc_dtimnoslp_set(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	uint16 noslp = wlc_pretbtt_calc(cfg);

	noslp += wlc->bcn_wait_prd << 10;

	wlc_write_shm(wlc, M_POSTDTIM0_NOSLPTIME(wlc), noslp >> 3);
}

/* infrastructure vs ad-hoc, STA vs AP, PMQ use, and ClosedNet */

/* XXX Have to check the given bsscfg's states separately because the place the function
 * is called which is prior to accounting it as a STA or AP or IBSS in wlc_sta_assoc_upd()
 * or wlc_ap_up_upd().
 */
/* XXX need to prevent multiple h/w IBSS bsscfgs from existing at the same time,
 * need to prevent h/w IBSS bsscfg from co-existing with h/w AP given the current
 * h/w and/or ucode limitations.
 */
/* XXX current h/w and ucode limitations:
 * - single TSF hence h/w IBSS and h/w AP can't coexist
 * - single TSF hence multiple h/w IBSSs can't coexist
 */

#define CLOSEDNET_ENAB(cfg) (HWPRB_ENAB(cfg) && (cfg)->closednet_nobcprbresp)

void
wlc_macctrl_init(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	/* MCTL_AP bit is taken care by wlc_ap_ctrl() */
	uint32 mask = (MCTL_INFRA | MCTL_DISCARD_PMQ | MCTL_CLOSED_NETWORK);
	uint32 val = 0;
	bool ap_active = (cfg && BSSCFG_AP(cfg)) || AP_ACTIVE(wlc);

	/* Set AP if this BSS is AP, or any AP BSS is up */
	if (ap_active) {
		wlc_validate_bcn_phytxctl(wlc, cfg); /* Can handle NULL cfg */
		wlc_ap_ctrl(wlc, TRUE, cfg, -1);     /* Can handle NULL cfg */
	}
	else {
		wlc_suspend_mac_and_wait(wlc);
		wlc_ap_ctrl(wlc, FALSE, cfg, -1);
		wlc_enable_mac(wlc);
	}

	/* As PMQ entries are currently used only by AP for detecting PM Mode transitions
	 * for STA, instruct ucode not to fill up entries in PMQ
	 * CAUTION: For STA, this bit will need to be cleared if we ever support IBSS using
	 * PMQ. Also, validate the phytxctl for the beacon
	 */
	if (!ap_active) {
		val |= MCTL_DISCARD_PMQ;
	}

	if (D11REV_IS(wlc->pub->corerev, 128)) {
		val |= MCTL_DISCARD_PMQ;
	}

	/* Set infrastructure mode all the time unless IBSS is up running
	 * (which will be taken care of by fn wlc_ibss_hw_enab).
	 * This will stop PSM from continued beaconing after tearing down
	 * the AP.
	 */
	val |= MCTL_INFRA;

	/* if this is the bsscfg for ucode beacon/probe responses,
	 * indicate whether this is a closed network
	 */
	if (!MBSS_ENAB(wlc->pub)) {
		if (cfg == NULL || !BSSCFG_AP(cfg) || !CLOSEDNET_ENAB(cfg)) {
			int idx;
			FOREACH_UP_AP(wlc, idx, cfg) {
				if (CLOSEDNET_ENAB(cfg)) {
					break;
				}
				cfg = NULL;
			}
		}
		if (cfg != NULL) {
#if defined(BCMDBG) || defined(WLMSG_INFORM)
			char eabuf[ETHER_ADDR_STR_LEN];
#endif // endif
			WL_INFORM(("wl%d: BSS %s is configured as a Closed Network, "
			           "enable MCTL bit in PSM\n", wlc->pub->unit,
			           bcm_ether_ntoa(&cfg->BSSID, eabuf)));
			val |= MCTL_CLOSED_NETWORK;
		}
	}

	wlc_mctrl(wlc, mask, val);
} /* wlc_macctrl_init */

/**
 * adjust h/w TSF counters as well SHM TSF offsets if applicable.
 * 'tgt' is the target h/w TSF time
 * 'ref' is the current h/w TSF time
 * 'nxttbtt' is the next TBTT time
 * 'bcnint' is the TBTT interval in us units
 * 'comp' indicates if 'tgt' needs to be compensated
 */
void
wlc_tsf_adj(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint32 tgt_h, uint32 tgt_l,
	uint32 ref_h, uint32 ref_l, uint32 nxttbtt, uint32 bcnint, bool comp)
{
	int32 off_h, off_l;
	uint cfpp = 0;
	osl_t *osh = wlc->osh;

	/* program the ATIM window */
	if (!cfg->BSS) {
		if (cfg->current_bss->atim_window != 0) {
			/* note that CFP is present */
			cfpp = CFPREP_CFPP;
		}
	}

#ifdef WLMCNX
	if (MCNX_ENAB(wlc->pub)) {
		/* make sure the ucode is awake and takes the adjustment... */
		wlc_mcnx_mac_suspend(wlc->mcnx);
	}
#endif // endif

	/* add the time has passed by so far from the ref time */
	if (comp) {
		uint32 now_h, now_l;

		wlc_read_tsf(wlc, &now_l, &now_h);
		off_h = now_h;
		off_l = now_l;
		wlc_uint64_sub((uint32 *)&off_h, (uint32 *)&off_l, ref_h, ref_l);
		wlc_uint64_add(&tgt_h, &tgt_l, off_h, off_l);
	}

	WL_ASSOC(("wl%d.%d: %s: tgt 0x%x%08x, ref 0x%x%08x, tbtt 0x%x, interval 0x%x, "
	          "compensate %d\n", wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
	          tgt_h, tgt_l, ref_h, ref_l, nxttbtt, bcnint, comp));

	/* Hold TBTTs */
	wlc_mctrl(wlc, MCTL_TBTT_HOLD, MCTL_TBTT_HOLD);

	/* low first, then high so the 64bit write is atomic */
	W_REG(osh, D11_TSFTimerLow(wlc), tgt_l);
	W_REG(osh, D11_TSFTimerHigh(wlc), tgt_h);

	/* write the beacon interval (to TSF) */
	W_REG(osh, D11_CFPRep(wlc), (bcnint << CFPREP_CBI_SHIFT) | cfpp);

	/* write the tbtt time */
	W_REG(osh, D11_CFPStart(wlc), nxttbtt);

#ifdef WLMCNX
	if (MCNX_ENAB(wlc->pub)) {
		/* adjust other connections' TSF offset */
		off_h = tgt_h;
		off_l = tgt_l;
		wlc_uint64_sub((uint32 *)&off_h, (uint32 *)&off_l, ref_h, ref_l);
		wlc_mcnx_tbtt_adj_all(wlc->mcnx, off_h, off_l);
	}
#endif // endif

	/* Release hold on TBTTs */
	wlc_mctrl(wlc, MCTL_TBTT_HOLD, 0);

#ifdef WLMCNX
	if (MCNX_ENAB(wlc->pub)) {
		/* restore ucode running state */
		wlc_mcnx_mac_resume(wlc->mcnx);
	}
#endif // endif
} /* wlc_tsf_adj */

void
wlc_BSSinit(wlc_info_t *wlc, wlc_bss_info_t *bi, wlc_bsscfg_t *cfg, int type)
{
	uint bcnint, bcnint_us;
	const bool restricted_ap = BSSCFG_AP(cfg) && (wlc->stas_associated > 0);
	osl_t *osh = wlc->osh;
	struct scb *scb;
#ifdef WL_MIMOPS_CFG
	wlc_hw_config_t *bsscfg_hw_cfg = NULL;
#endif // endif

#ifdef WLCHANIM
	int err;
#endif /* WLCHANIM */
	wlc_bss_info_t *current_bss = cfg->current_bss;
	wlc_bss_info_t *default_bss = wlc->default_bss;

	WL_TRACE(("wl%d: %s:\n", wlc->pub->unit, __FUNCTION__));

	WL_APSTA_UPDN(("wl%d: wlc_BSSinit(), %s, %s, stas/aps/associated %d/%d/%d\n",
		wlc->pub->unit, (BSSCFG_AP(cfg) ? "AP" : "STA"),
		(type == WLC_BSS_START ? "START" : "JOIN"),
		wlc->stas_associated, wlc->aps_associated, wlc->pub->associated));

	/* clear out bookkeeping */
#ifdef STA
	wlc_roam_handle_join(cfg);
#endif /* STA */

	if (wlc->aps_associated == 0 || !cfg->BSS) {
#ifdef STA
		/* Do not reset TPC if any connections are active */
		if (!wlc_stas_connected(wlc))
#endif /* STA */
		{
			wlc_tpc_reset_all(wlc->tpc);
		}
	}

	/* update current BSS information */
	if (bi != current_bss) {
		if (current_bss->bcn_prb)
			MFREE(osh, current_bss->bcn_prb, current_bss->bcn_prb_len);
		bcopy((char*)bi, (char*)current_bss, sizeof(wlc_bss_info_t));
		if (bi->bcn_prb) {
			/* need to copy bcn_prb, too */
			current_bss->bcn_prb =
			        (struct dot11_bcn_prb *)MALLOC(osh, bi->bcn_prb_len);
			if (current_bss->bcn_prb) {
				bcopy((char*)bi->bcn_prb, (char*)current_bss->bcn_prb,
					bi->bcn_prb_len);
				current_bss->bcn_prb_len = bi->bcn_prb_len;
			} else {
				WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
					wlc->pub->unit, __FUNCTION__, MALLOCED(osh)));
			}
		}
	}

#ifdef STA
	/* apply the capability and additional IE params stored in wlc_join_BSS() */
	if (N_ENAB(wlc->pub) && type == WLC_BSS_JOIN) {
	}
#endif // endif

	/* Validate chanspec in case the locale has changed */
	if (BSSCFG_AP(cfg) &&
	    !wlc_valid_chanspec_db(wlc->cmi, current_bss->chanspec)) {
		ASSERT(!cfg->associated);
		wlc_bsscfg_set_current_bss_chan(cfg, wlc_default_chanspec(wlc->cmi, FALSE));
	}

#ifdef WL_MIMOPS_CFG
	if (WLC_MIMOPS_ENAB(wlc->pub))
		bsscfg_hw_cfg = wlc_stf_bss_hw_cfg_get(cfg);
#endif // endif

	/*
	 * Assume here radio channel = current_bss->chanspec.
	 * Must do this before the rate calculations below.
	 * AP chanspec direct setting is prohibited. Should be synced with
	 * MSCH. IBSS will cover the same way later.
	 */
	if (WLC_BAND_PI_RADIO_CHANSPEC != current_bss->chanspec) {
		if (WLC_CHAN_COEXIST(WLC_BAND_PI_RADIO_CHANSPEC, current_bss->chanspec)) {
#ifdef WL_MIMOPS_CFG
			if (WLC_MIMOPS_ENAB(wlc->pub)) {
				bsscfg_hw_cfg->chanspec_override = FALSE;
			}
#endif /* WL_MIMOPS_CFG */
			wlc_update_bandwidth(wlc, cfg, current_bss->chanspec);
		} else if (!BSSCFG_AP(cfg)) {
			wlc_set_home_chanspec(wlc, current_bss->chanspec);
			wlc_set_chanspec(wlc, current_bss->chanspec,
					CHANSW_REASON(CHANSW_ASSOC));
		}
	} else if (!BSSCFG_AP(cfg)) {
		if (wlc->home_chanspec != WLC_BAND_PI_RADIO_CHANSPEC) {
			wlc->home_chanspec = WLC_BAND_PI_RADIO_CHANSPEC;
			/* sync up phy/radio chanspec */
			wlc_set_phy_chanspec(wlc, wlc->home_chanspec);
		}
	}

#ifdef WLCHANIM
	if (WLC_CHANIM_ENAB(wlc->pub)) {
		wlc_lq_chanim_acc_reset(wlc);
		if ((err = wlc_lq_chanim_update(wlc, current_bss->chanspec, CHANIM_CHANSPEC))
		    != BCME_OK) {
			WL_ERROR(("wl%d: %s: CHANIM upd fail %d\n", wlc->pub->unit,
				__FUNCTION__, err));
		}
	}
#endif /* WLCHANIM */

	wlc_BSSinit_rateset_filter(cfg);
#ifdef WL_MIMOPS_CFG
	if (WLC_MIMOPS_ENAB(wlc->pub)) {
		bsscfg_hw_cfg->original_chanspec_bw = current_bss->chanspec;
		bsscfg_hw_cfg->current_chanspec_bw = current_bss->chanspec;
	}
#endif /* WL_MIMOPS_CFG */

	/* Validate beacon interval, we should never create a 0 beacon period BSS/IBSS */
	if (type == WLC_BSS_START) {
		ASSERT(current_bss->beacon_period);
	}
	/* don't adopt a illegal beacon_period IBSS */
	if (current_bss->beacon_period == 0) {
		current_bss->beacon_period = ISSIM_ENAB(wlc->pub->sih) ?
			BEACON_INTERVAL_DEF_QT : BEACON_INTERVAL_DEFAULT;
	}

	/* initialize BSSID information */
	cfg->BSSID = current_bss->BSSID;
	cfg->prev_BSSID = current_bss->BSSID;

	/* write the BSSID (to RXE) */
	if (!BSSCFG_PSTA(cfg))
		wlc_set_bssid(cfg);

	/* Update shared memory with MAC */
	wlc_set_mac(cfg);

	/* write to SHM to adjust the ucode ACK/CTS rate tables... */
	wlc_set_ratetable(wlc);

	/* the world is new again, so is our reported rate */
	if (!wlc->pub->associated)
		wlc_bsscfg_reprate_init(cfg);

	if (wlc->band->gmode && !restricted_ap) {

		/* update shortslot state now before founding or joining a BSS */
		if (wlc->shortslot_override == WLC_SHORTSLOT_AUTO) {
			if (type == WLC_BSS_JOIN)
				/* STA JOIN: adopt the 11g shortslot mode of the network */
				wlc->shortslot =
				        (current_bss->capability & DOT11_CAP_SHORTSLOT) != 0;
			else if (BSSCFG_AP(cfg))	/* AP START: in short slot mode */
				wlc->shortslot = TRUE;
			else	/* IBSS START: depending on user setting in advanced tab. */
				wlc->shortslot =
				        (default_bss->capability & DOT11_CAP_SHORTSLOT) != 0;
		} else {
			/* Switch to long or short slot mode based on the override */
			wlc->shortslot = (wlc->shortslot_override == WLC_SHORTSLOT_ON);
		}
		wlc_bmac_set_shortslot(wlc->hw, wlc->shortslot);
	}

	/* Legacy B update */
	if (BAND_2G(wlc->band->bandtype) &&
	    (wlc->band->gmode == GMODE_LEGACY_B) && BSSCFG_AP(cfg)) {
		ASSERT(wlc->shortslot_override == WLC_SHORTSLOT_OFF);
		wlc->shortslot = (wlc->shortslot_override == WLC_SHORTSLOT_ON);
		wlc_bmac_set_shortslot(wlc->hw, wlc->shortslot);

		/* Update aCWmin based on basic rates. */
		wlc_cwmin_gphy_update(wlc, &current_bss->rateset, TRUE);
	}

	wlc_prot_g_init(wlc->prot_g, cfg);
#ifdef WL11N
	/* set default n-protection */
	if (N_ENAB(wlc->pub))
		wlc_prot_n_init(wlc->prot_n, cfg);
#endif /* WL11N */

	if (BSSCFG_AP(cfg) || !cfg->BSS) {
		/* update our capability */
		current_bss->capability &= ~DOT11_CAP_SHORT;
		if (BSSCFG_AP(cfg)) {
			if (cfg->PLCPHdr_override == WLC_PLCP_SHORT)
				current_bss->capability |= DOT11_CAP_SHORT;
		} else {
			/* use IBSSGmode setting for bcn and prb cap */
			if (default_bss->capability & DOT11_CAP_SHORT)
				current_bss->capability |= DOT11_CAP_SHORT;
		}

		/* update the capability based on current shortslot mode */
		current_bss->capability &= ~DOT11_CAP_SHORTSLOT;
		if (wlc->shortslot && wlc->band->gmode)
			current_bss->capability |= DOT11_CAP_SHORTSLOT;

		if (BSSCFG_AP(cfg)) {
			wlc_bss_update_dtim_period(wlc, cfg);
		}
		_wlc_bss_update_beacon(wlc, cfg);
		wlc_bss_update_probe_resp(wlc, cfg, FALSE);
	}

#ifdef STA
	if (BSSCFG_STA(cfg)) {
		if (!cfg->BSS) {
			/* set CFP duration */
			W_REG(osh, D11_CFPMaxDur(wlc), (uint32)current_bss->atim_window);
		}

		/* set initial RSSI value */
		if (cfg->BSS) {
			wlc_lq_rssi_snr_noise_bss_sta_ma_reset(wlc, cfg,
				CHSPEC_WLCBANDUNIT(cfg->current_bss->chanspec),
				WLC_RSSI_EXCELLENT, WLC_SNR_EXCELLENT, WLC_NOISE_EXCELLENT);
		}
		phy_bss_init(WLC_PI(wlc), FALSE, WLC_NOISE_EXCELLENT);
	}
#endif /* STA */

	/* First AP starting a BSS, or STA starting an IBSS */
	if (type == WLC_BSS_START && wlc->aps_associated == 0) {
#ifdef WLMCHAN
		/* If already have stations associated, need to move STA to per bss
		 * tbtt and reset tsf for AP to make AP tbtt land in the middle of
		 * STA's bcn period
		 */
		if (MCHAN_ENAB(wlc->pub) &&
		    BSSCFG_AP(cfg) && STA_ACTIVE(wlc) &&
		    wlc_mchan_ap_tbtt_setup(wlc, cfg)) {
			; /* empty */
		} else
#endif // endif
		/* AP or IBSS with h/w beacon enabled uses h/w TSF */
		if (BSSCFG_AP(cfg) || IBSS_HW_ENAB(cfg)) {
			uint32 old_l, old_h;

			/* Program the next TBTT and adjust TSF timer.
			 * If we are not starting the BSS, then the TSF timer adjust is done
			 * at beacon reception time.
			 */
			/* Using upstream beacon period will make APSTA 11H easier */
			bcnint = current_bss->beacon_period;
			bcnint_us = ((uint32)bcnint << 10);

			WL_APSTA_TSF(("wl%d: wlc_BSSinit(): starting TSF, bcnint %d\n",
			              wlc->pub->unit, bcnint));

			wlc_read_tsf(wlc, &old_l, &old_h);
			wlc_tsf_adj(wlc, cfg, 0, 0, old_h, old_l, bcnint_us, bcnint_us, FALSE);
		}
	}

	wlc_macctrl_init(wlc, cfg);

#ifdef WLMCNX
	/* update AP's or primary IBSS's channel */
	if (MCNX_ENAB(wlc->pub) &&
	    (BSSCFG_AP(cfg) ||
	     (BSSCFG_IBSS(cfg) && cfg == wlc->cfg))) {
		chanspec_t chanspec = current_bss->chanspec;
		uint16 go_chan;
		ASSERT(chanspec != INVCHANSPEC);
		if (D11REV_LT(wlc->pub->corerev, 40)) {
			go_chan = CHSPEC_CHANNEL(chanspec);
			if (CHSPEC_IS5G(chanspec))
				go_chan |= D11_CURCHANNEL_5G;
			if (CHSPEC_IS40(chanspec))
				go_chan |= D11_CURCHANNEL_40;
		} else
			/* store chanspec */
			go_chan = chanspec;
		wlc_mcnx_write_shm(wlc->mcnx, M_P2P_GO_CHANNEL_OFFSET(wlc), go_chan);
	}
#endif /* WLMCNX */

#ifdef STA
	/* Entering IBSS: make sure bcn promisc is on */
	if (!cfg->BSS) {
		wlc_ibss_enable(cfg);
	}
#endif /* STA */

	/* update bcast/mcast rateset */
	if (BSSCFG_HAS_BCMC_SCB(cfg)) {
		scb = WLC_BCMCSCB_GET(wlc, cfg);
		ASSERT(scb != NULL);
		wlc_rateset_filter(&current_bss->rateset /* src */, &scb->rateset /* dst */, FALSE,
		                   WLC_RATES_CCK_OFDM, RATE_MASK, wlc_get_mcsallow(wlc, cfg));
	}

	wlc_pretbtt_set(cfg);

#ifdef WLMCNX
	/* extend beacon timeout to cover possible long pretbtt */
	if (MCNX_ENAB(wlc->pub)) {
		if (BSSCFG_STA(cfg) && cfg->BSS)
			wlc_dtimnoslp_set(cfg);
	}
#endif // endif

	/* sync up TSF cache in case it's reset or adjusted when roamed or associated */
	wlc_lifetime_cache_upd(wlc);
} /* wlc_BSSinit */

/** ucode, hwmac update. Channel dependent updates for ucode and hw */
static void
wlc_ucode_mac_upd(wlc_info_t *wlc)
{
#ifdef STA
	/* enable or disable any active IBSSs depending on whether or not
	 * we are on the home channel
	 */
	if (wlc->home_chanspec == WLC_BAND_PI_RADIO_CHANSPEC) {
		wlc_ibss_enable_all(wlc);
		if (wlc->pub->associated) {
			/* BMAC_NOTE: This is something that should be fixed in ucode inits.
			 * I think that the ucode inits set up the bcn templates and shm values
			 * with a bogus beacon. This should not be done in the inits. If ucode needs
			 * to set up a beacon for testing, the test routines should write it down,
			 * not expect the inits to populate a bogus beacon.
			 */
			/* XXX need to reinit the beacon tsf offset
			 * after a band switch in a single phy device.
			 */
			if (WLC_PHY_11N_CAP(wlc->band)) {
				wlc_write_shm(wlc, M_BCN_TXTSF_OFFSET(wlc), wlc->band->bcntsfoff);
			}
		}
	} else {
		/* disable an active IBSS if we are not on the home channel */
		wlc_ibss_disable_all(wlc);
	}
#endif /* STA */

	/* update the various promisc bits */
	wlc_mac_bcn_promisc(wlc);
	wlc_mac_promisc(wlc);
}

void
wlc_bandinit_ordered(wlc_info_t *wlc, chanspec_t chanspec)
{
	wlc_rateset_t default_rateset;
	uint parkband;
	uint i, band_order[2];
#ifdef WLCHANIM
	int err;
#endif /* WLCHANIM */

	WL_TRACE(("wl%d: wlc_bandinit_ordered\n", wlc->pub->unit));
	/*
	 * We might have been bandlocked during down and the chip power-cycled (hibernate).
	 * figure out the right band to park on
	 */
	if (wlc->bandlocked || NBANDS(wlc) == 1) {
		ASSERT(CHSPEC_WLCBANDUNIT(chanspec) == wlc->band->bandunit);

		parkband = wlc->band->bandunit;	/* updated in wlc_bandlock() */
		band_order[0] = band_order[1] = parkband;
	} else {
		/* park on the band of the specified chanspec */
		parkband = CHSPEC_WLCBANDUNIT(chanspec);

		/* order so that parkband initialize last */
		band_order[0] = parkband ^ 1;
		band_order[1] = parkband;
	}

	/* make each band operational, software state init */
	for (i = 0; i < NBANDS(wlc); i++) {
		uint j = band_order[i];

#ifdef WLRSDB
		if (RSDB_ENAB(wlc->pub)) {
			if (!wlc_rsdb_band_match(wlc, wlc->bandstate[j])) {
				continue;
			}
		}
#endif // endif
		wlc_pi_band_update(wlc, j);

		ASSERT(wlc->bandstate[j]->hwrs_scb != NULL);

		wlc_default_rateset(wlc, &default_rateset);

		/* fill in hw_rate */
		wlc_rateset_filter(&default_rateset /* src */, &wlc->band->hw_rateset /* dst */,
			FALSE, WLC_RATES_CCK_OFDM, RATE_MASK, wlc_get_mcsallow(wlc, NULL));
#ifdef WL11N
		/* patch up MCS32 */
		wlc_rateset_bw_mcs_filter(&wlc->band->hw_rateset, CHSPEC_WLC_BW(chanspec));
#endif // endif
#ifdef WL11AX
		if (HE_ENAB(wlc->pub)) {
			wlc_he_update_mcs_cap(wlc->hei);
		}
#endif /* WL11AX */
		/* init basic rate lookup */
		wlc_rate_lookup_init(wlc, &default_rateset);
	}

	wlc_set_home_chanspec(wlc, chanspec);

#ifdef WLCHANIM
	if (WLC_CHANIM_ENAB(wlc->pub)) {
		wlc_lq_chanim_acc_reset(wlc);
	}
#endif // endif

#ifdef WLCHANIM
	if (WLC_CHANIM_ENAB(wlc->pub) && (wlc->pub->up)) {
		if ((err = wlc_lq_chanim_update(wlc, wlc->chanspec, CHANIM_CHANSPEC)) != BCME_OK) {
			WL_ERROR(("wl%d: %s: WLC_CHANIM upd fail %d\n", wlc->pub->unit,
				__FUNCTION__, err));
		}
	}
#endif /* WLCHANIM */
	/* sync up phy/radio chanspec */
	wlc_set_phy_chanspec(wlc, chanspec);
} /* wlc_bandinit_ordered */

/** band-specific init */
static void
WLBANDINITFN(wlc_bsinit)(wlc_info_t *wlc)
{
	WL_TRACE(("wl%d: wlc_bsinit: bandunit %d\n", wlc->pub->unit, wlc->band->bandunit));

	/* write ucode ACK/CTS rate table */
	wlc_set_ratetable(wlc);

	/* update some band specific mac configuration */
	wlc_ucode_mac_upd(wlc);

#ifdef WL11N
	/* init antenna selection */
	if (WLANTSEL_ENAB(wlc))
		wlc_antsel_init(wlc->asi);
#endif // endif
#if defined(WLC_SW_DIVERSITY)
	if (WLSWDIV_ENAB(wlc)) {
		/* During the band change or Init, If the SWDIV timer is already set,
		 * need to delete that as we are changing the band and coreband bitmap
		 * may not allow the SWDIV on the new band
		 */
		wlc_swdiv_antmap_init(wlc->swdiv);
		wlc_swdiv_scbcubby_stat_reset(wlc->swdiv);
	}
#endif /* WLC_SW_DIVERSITY */
}

/** switch to and initialize new band */
static void
WLBANDINITFN(wlc_setband)(wlc_info_t *wlc, uint bandunit)
{
#ifdef STA
	int idx;
	wlc_bsscfg_t *cfg;
#endif // endif

	ASSERT(NBANDS(wlc) > 1);
	ASSERT(!wlc->bandlocked);
	ASSERT(bandunit != wlc->band->bandunit || wlc->bandinit_pending);

	WL_DUAL(("DUALBAND: wlc_setband: bandunit %d\n", bandunit));

	wlc_pi_band_update(wlc, bandunit);

	if (!wlc->pub->up)
		return;

	WL_TSLOG(wlc, __FUNCTION__, TS_ENTER, 0);
#ifdef STA
	/* wait for at least one beacon before entering sleeping state */
	FOREACH_AS_STA(wlc, idx, cfg) {
	        wlc_set_pmawakebcn(cfg, TRUE);
	}
#endif // endif

	/* band-specific initializations */
	wlc_bsinit(wlc);
	WL_TSLOG(wlc, __FUNCTION__, TS_EXIT, 0);
}

/**
 * Check if this TLVS entry is a WME IE or params IE entry.
 * If not, move the tlvs pointer/length to indicate the next TLVS entry.
 */
const bcm_tlv_t *
wlc_find_wme_ie(const uint8 *tlvs, uint tlvs_len)
{
	const bcm_tlv_t *wme_ie;

	if ((wme_ie =
	     bcm_find_wmeie(tlvs, tlvs_len, WME_SUBTYPE_IE, WME_IE_LEN)) != NULL)
		return wme_ie;

	if ((wme_ie =
	     bcm_find_wmeie(tlvs, tlvs_len, WME_SUBTYPE_PARAM_IE, WME_PARAM_IE_LEN)) != NULL)
		return wme_ie;

	return NULL;
}

void
wlc_fragthresh_set(wlc_info_t *wlc, uint8 ac, uint16 thresh)
{
	wlc->fragthresh[ac] = thresh;
}

#ifndef ATE_BUILD
static bool
BCMATTACHFN(wlc_timers_init)(wlc_info_t *wlc, int unit)
{
	BCM_REFERENCE(unit);

	if (!(wlc->wdtimer = wl_init_timer(wlc->wl, wlc_watchdog_timer, wlc, "watchdog"))) {
		WL_ERROR(("wl%d: wdtimer failed\n", unit));
		goto fail;
	}

#ifdef STA
	if (!(wlc->wdtimer_maccore_state = wl_init_timer(wlc->wl,
		wlc_watchdog_indicate_maccore_state_timer, wlc, "wlc_watchdog_mac_core_state"))) {
		WL_ERROR(("wl%d: wdtimer_maccore_state failed\n", unit));
		goto fail;
	}
#endif /* STA */

	if (!(wlc->radio_timer = wl_init_timer(wlc->wl, wlc_radio_timer, wlc, "radio"))) {
		WL_ERROR(("wl%d: radio_timer failed\n", unit));
		goto fail;
	}

#ifdef STA
	if (!(wlc->pm2_radio_shutoff_dly_timer = wl_init_timer(wlc->wl,
		wlc_pm2_radio_shutoff_dly_timer, wlc, "radioshutoffdly"))) {
		WL_ERROR(("wl%d: radio_shutoff_dly_timer failed\n", unit));
		goto fail;
	}

	if (!(wlc->wake_event_timer = wl_init_timer(wlc->wl, wlc_wake_event_timer, wlc,
		"wakeeventtimer"))) {
		WL_ERROR(("wl%d: wl_init_timer for wake_event_timer failed\n", unit));
		goto fail;
	}
#endif	/* STA */
#ifdef WLRXOV
	if (WLRXOV_ENAB(wlc->pub)) {
		if (!(wlc->rxov_timer = wl_init_timer(wlc->wl, wlc_rxov_timer, wlc, "rxov"))) {
			WL_ERROR(("wl%d: rxov_timer failed\n", unit));
			goto fail;
		}
	}
#endif // endif
	return TRUE;

fail:
	return FALSE;
} /* wlc_timers_init */
#endif /* ATE_BUILD */

static bool
wlc_clkgating_cap(wlc_pub_t *pub)
{
	bool cap = FALSE;
	uint corerev = pub->corerev;

	if (getvar(pub->vars, "mac_clkgating") != NULL) {
		cap = (bool)getintvar(pub->vars, "mac_clkgating");
	} else {
		if (D11REV_IS(corerev, 61)) { /* 4347/6878 */
			cap = TRUE;
		}
	}

	return cap;
}

/** Initialize wlc_info default values. May get overrides later in this function. */
static void
BCMATTACHFN(wlc_info_init)(wlc_info_t *wlc, int unit)
{
	int i;

	BCM_REFERENCE(unit);

	/* Assume the device is there until proven otherwise */
	wlc->device_present = TRUE;

	/* set default power output percentage to 100 percent */
	wlc->txpwr_percent = 100;

	if ((wlc->phy_cap & PHY_PREATTACH_CAP_SUP_5G) &&
		!(wlc->phy_cap & PHY_PREATTACH_CAP_SUP_2G)) {
		/* Assuming cores are bandlocked/band specific */
		wlc->chanspec = CH20MHZ_CHSPEC(36);
	} else {
		wlc->chanspec = CH20MHZ_CHSPEC(1);
	}

	wlc->legacy_probe = TRUE;

	/* various 802.11g modes */
	wlc->shortslot = FALSE;
	wlc->shortslot_override = WLC_SHORTSLOT_AUTO;

	wlc->stf->ant_rx_ovr = ANT_RX_DIV_DEF;
	wlc->stf->txant = ANT_TX_DEF;

	wlc->prb_resp_timeout = WLC_PRB_RESP_TIMEOUT;

	wlc->usr_fragthresh = DOT11_DEFAULT_FRAG_LEN;
	for (i = 0; i < AC_COUNT; i++)
		wlc->fragthresh[i] = DOT11_DEFAULT_FRAG_LEN;
	wlc->RTSThresh = DOT11_DEFAULT_RTS_LEN;

#if defined(AP) && !defined(STA)
	wlc->pub->_ap = TRUE;
#endif // endif

	/* default rate fallback retry limits */
	wlc->SFBL = RETRY_SHORT_FB;
	wlc->LFBL = RETRY_LONG_FB;

	/* default mac retry limits */
	wlc->SRL = RETRY_SHORT_DEF;
	wlc->LRL = RETRY_LONG_DEF;

	/* init PM state */
	wlc->PMpending = FALSE;		/* Tracks whether STA indicated PM in the last attempt */

	wlc->excess_pm_period = 0; /* Disable the excess PM notify */
	wlc->excess_pm_percent = 100;

	/* Init wme queuing method */
	wlc->wme_prec_queuing = FALSE;

	/* Overrides for the core to stay awake under zillion conditions Look for STAY_AWAKE */
	wlc->wake = FALSE;
	/* Are we waiting for a response to PS-Poll that we sent */
	wlc->PSpoll = FALSE;

	/* APSD defaults */
#ifdef STA
	wlc->apsd_sta_usp = FALSE;
#endif // endif

	/* WME QoS mode is Auto by default */
#if defined(WME)
	wlc->pub->_wme = AUTO;
#endif // endif

#ifdef BCMSDIODEV_ENABLED
	wlc->pub->_priofc = TRUE;	/* enable priority flow control for sdio dongle */
#endif // endif

#ifdef WLAMPDU
	/* enable AMPDU by default */
	wlc->pub->_ampdu_tx = TRUE;
	wlc->pub->_ampdu_rx = TRUE;
#endif // endif
#ifdef WLAMPDU_HOSTREORDER
#ifdef WLAMPDU_HOSTREORDER_DISABLED
	wlc->pub->_ampdu_hostreorder = FALSE;
#else
	wlc->pub->_ampdu_hostreorder = TRUE;
#endif /* WLAMPDU_HOSTREORDER_DISABLED */
#endif /* WLAMPDU_HOSTREORDER */
#ifdef WLAMSDU
	/* AMSDU agg/deagg will be rechecked later */
	wlc->pub->_amsdu_tx = FALSE;
	wlc->_amsdu_rx = FALSE;
	wlc->_rx_amsdu_in_ampdu = FALSE;
#endif // endif

	/* AMSDU monitor packet queue */
	wlc->monitor_amsdu_pkts = NULL;

#if defined(WLTDLS) && !defined(WLTDLS_DISABLED)
	/*
	* Post deprecation of TDLS_SUPPORT(),
	* TDLS_ENAB() needs to be true before TDLS gets attached
	*/
	wlc->pub->cmn->_tdls = TRUE;
#endif /* WLTDLS */

#if defined(WLFBT) && !defined(WLFBT_DISABLED)
	wlc->pub->_fbt_cap = TRUE;
#endif // endif

#if defined(TINY_PKTJOIN) && !defined(TINY_PKTJOIN_DISABLED)
	wlc->pub->_tiny_pktjoin = TRUE;
#endif // endif

#if defined(WL_RXEARLYRC) && !defined(WL_RXEARLYRC_DISABLED)
	wlc->pub->_wl_rxearlyrc = TRUE;
#endif // endif

#if defined(WLRXOV) && !defined(WLRXOV_DISABLED)
	wlc->pub->_rxov = TRUE;
#endif // endif

#if !defined(WLNOEIND) && !defined(WLEIND_DISABLED)
	wlc->pub->_wleind = TRUE;
#endif // endif

#if defined(WL11K) && !defined(WL11K_DISABLED)
	wlc->pub->_rrm = TRUE;
#endif // endif

#if defined(WLOVERTHRUSTER) && !defined(WLOVERTHRUSTER_DISABLED)
	wlc->pub->_wloverthruster = TRUE;
#endif // endif
	wlc->pub->bcmerror = 0;

	wlc->ibss_allowed = TRUE;
	wlc->ibss_coalesce_allowed = TRUE;

	if (N_ENAB(wlc->pub))
		wlc->pub->_coex = ON;

#ifdef STA
	wlc->pm2_radio_shutoff_pending = FALSE;
	wlc->pm2_radio_shutoff_dly = 0; /* disable radio shutoff delay feature */

	/* initialize mpc delay (it should be non-zero value) */
	wlc->mpc_delay_off = wlc->mpc_dlycnt = WLC_MPC_THRESHOLD;
#endif // endif

	wlc->rfaware_lifetime = WLC_RFAWARE_LIFETIME_DEFAULT;

	wlc->pr80838_war = TRUE;
#ifdef STA
#if defined(AP_KEEP_ALIVE)
	wlc_ap_keep_alive_count_default(wlc);
#endif /* defined(AP_KEEP_ALIVE) */
#endif /* STA */
	wlc->roam_rssi_cancel_hysteresis = ROAM_RSSITHRASHDIFF;

	/* Init flag to prevent multiple executions thru wlc_send_q */
	wlc->in_send_q = FALSE;
#ifdef STA
	wlc->seq_reset = TRUE;          /* reset the sequence number register in SCR to 0 */
#endif // endif
#if defined(WLFMC) && !defined(WLFMC_DISABLED)
	wlc->pub->_fmc = TRUE;
#endif // endif
#if defined(WLRCC) && !defined(WLRCC_DISABLED)
	wlc->pub->_rcc = TRUE;
#endif // endif

#if defined(WLNON_CLEARING_RESTRICTED_CHANNEL_BEHAVIOR)
	wlc->pub->_clear_restricted_chan = FALSE;
#else
	wlc->pub->_clear_restricted_chan = TRUE;
#endif // endif

#if defined(WL_EVDATA_BSSINFO) && !defined(WL_EVDATA_BSSINFO_DISABLED)
	wlc->pub->_evdata_bssinfo = TRUE;
#endif // endif

#if defined(WLPM_BCNRX) && !defined(WLPM_BCNRX_DISABLED)
	wlc->pub->cmn->_pm_bcnrx = TRUE;
#endif // endif
#if defined(WLABT) && !defined(WLABT_DISABLED)
	wlc->pub->_abt = TRUE;
#endif // endif
#if defined(WLSCAN_PS) && !defined(WLSCAN_PS_DISABLED)
	wlc->pub->_scan_ps = TRUE;
#endif // endif
	/* It is enabled dynamically when PSM watchdog is hit */
	wlc->psm_watchdog_debug = FALSE;
	mboolset(wlc->wd_state, WATCHDOG_ON_BCN);
#ifdef BCMPCIEDEV
	if (BCMPCIEDEV_ENAB()) {
		wlc->pub->_wlfc_info_to_bus = TRUE;
		wlc->pub->_txmetadata_to_host = FALSE;
		wlc->pub->_rxmetadata_to_host = FALSE;
		wlc->pub->_wlfc_ctrl_to_host = FALSE;
	} else
#endif // endif
	{
		wlc->pub->_wlfc_info_to_bus = FALSE;
		wlc->pub->_txmetadata_to_host = TRUE;
		wlc->pub->_rxmetadata_to_host = TRUE;
		wlc->pub->_wlfc_ctrl_to_host = TRUE;
	}
#if defined(NDIS)
	/* in NIC enable scan result on WLC_E_SCAN_COMPLETE event  */
	wlc->pub->scanresults_on_evtcplt = TRUE;
#endif // endif
	/* init flag for u-code register dump */
	wlc->pub->_ucodedump = TRUE;

#if defined(FCC_PWR_LIMIT_2G) && !defined(FCC_PWR_LIMIT_2G_DISABLED)
	wlc->pub->_fccpwrlimit2g = TRUE;
#else
	wlc->pub->_fccpwrlimit2g = FALSE;
#endif /* FCC_PWR_LIMIT_2G && !FCC_PWR_LIMIT_2G_DISABLED */

	if (wlc_clkgating_cap(wlc->pub)) {
		wlc->pub->_mac_clkgating = TRUE;
	}

#if defined(WL_AUTH_SHARED_OPEN) && !defined(WL_AUTH_SHARED_OPEN_DISABLED)
	wlc->pub->_auth_shared_open = TRUE;
#endif // endif
#if defined(WL_PWRSTATS) && !defined(WL_PWRSTATS_DISABLED)
	wlc->pub->_pwrstats = TRUE;
#endif /* WL_PWRSTATS && !WL_PWRSTATS_DISABLED */

#if defined(WL_MONITOR) && !defined(WL_MONITOR_DISABLED)
	wlc->pub->_mon = TRUE;
#endif /* WL_MONITOR && !WL_MONITOR_DISABLED */

	{
		uint32 grp = (uint32) getintvar(wlc->pub->vars, "dis_ch_grp");
		if (IS_DIS_CH_GRP_VALID(grp)) {
			wlc->pub->_dis_ch_grp_conf = grp;
		} else {
			WL_ERROR(("wl%d: ignoring invalid dis_ch_grp from otp/srom/nvram 0x%x\n",
					wlc->pub->unit, grp));
		}
	}

#if defined(WL_DTS) && !defined(WL_DTS_DISABLED)
	wlc->pub->_dts = TRUE;
#endif // endif

	/* remains invalid till set through iovar; defaults are used till then */
	wlc->pub->_dyn160_active = WL_DEFAULT_DYN160_INVALID;
	wlc->txpwr_degrade = 0;

#ifdef PHYCAL_CACHING
	wlc->last_CSA_chanspec = 0;
#endif // endif
} /* wlc_info_init */

/* the err msgs help ensure valid uid is used */
#define MODULE_ATTACH(var, attach_func, num) \
	if (num <= prev_num) { \
		WL_ERROR(("%s: ID ERROR: prev_num=%d num=%d\n", \
		          __FUNCTION__, prev_num, num));	\
		err = num; \
		goto fail; \
	} \
	prev_num = num; \
	if ((wlc->var = attach_func(wlc)) == NULL) { \
		WL_ERROR(("%s: module %d attach failed\n", __FUNCTION__, num));  \
		err = num; \
		goto fail; \
	}

/** attach infra. modules that must do first before other modules */
/* NOTE: DO NOT ADD REGULAR MODULE ATTACH CALLS HERE, ADD THEM TO
 * wlc_attach_module() INSTEAD.
 */
static uint
BCMATTACHFN(wlc_attach_module_pre)(wlc_info_t *wlc)
{
	uint err = 0;
	uint prev_num = 0;
#ifdef BCMDBG_ERR
	uint unit = wlc->pub->unit;
#endif // endif

	/* Attach the notifier first, since other modules may create a notification
	 * server in their module-attach functions.
	 */
	if ((wlc->notif = bcm_notif_attach(wlc->osh, wlc->mem_pool_mgr,
	                                   wlc->pub->tunables->max_notif_servers,
	                                   wlc->pub->tunables->max_notif_clients)) == NULL) {
		WL_ERROR(("wl%d: %s: bcm_notif_attach failed\n", unit, __FUNCTION__));
		err = 1;
		goto fail;
	}
	MODULE_ATTACH(iocvi, wlc_iocv_attach, 10);
	MODULE_ATTACH(hrti, wlc_hrt_attach, 17);
	MODULE_ATTACH(pcb, wlc_pcb_attach, 30);
	MODULE_ATTACH(bcmh, wlc_bsscfg_attach, 40);
	MODULE_ATTACH(scbstate, wlc_scb_attach, 50); /* scbstate before wlc_ap_attach() */
	MODULE_ATTACH(iemi, wlc_iem_attach, 60);
	MODULE_ATTACH(ieri, wlc_ier_attach, 70);
	MODULE_ATTACH(txmodi, wlc_txmod_attach, 80);

	if (wlc_chansw_attach(wlc) != BCME_OK) {
		err = 90;
		goto fail;
	}

	MODULE_ATTACH(chctxi, wlc_chctx_pre_attach, 93);

	/* TODO: Move these IE mgmt callbacks registration to their own modules */

	/* Register IE mgmt callbacks. */
	if (wlc_register_iem_fns(wlc) != BCME_OK ||
	    wlc_bcn_register_iem_fns(wlc) != BCME_OK ||
	    wlc_prq_register_iem_fns(wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_register_iem_fns failed\n", unit, __FUNCTION__));
		err = 100;
		goto fail;
	}

	/* TODO: Move these IE mgmt registry creation to their own modules */

	/* Create IE mgmt callback registry */
#if defined(WLTDLS) && !defined(WLTDLS_DISABLED)
	/* TDLS Setup Request Action Frame */
	if ((wlc->ier_tdls_srq = wlc_ier_create_registry(wlc->ieri,
		IEM_TDLS_SRQ_BUILD_CB_MAX, IEM_TDLS_SRQ_PARSE_CB_MAX)) == NULL) {
		WL_ERROR(("wl%d: %s: wlc_ier_create_registry failed, tdls setup req\n",
			unit, __FUNCTION__));
		err = 101;
		goto fail;
	}
	/* TDLS Setup Response Action Frame */
	if ((wlc->ier_tdls_srs = wlc_ier_create_registry(wlc->ieri,
		IEM_TDLS_SRS_BUILD_CB_MAX, IEM_TDLS_SRS_PARSE_CB_MAX)) == NULL) {
		WL_ERROR(("wl%d: %s: wlc_ier_create_registry failed, tdls setup resp\n",
			unit, __FUNCTION__));
		err = 102;
		goto fail;
	}
	/* TDLS Setup Confirm Action Frame */
	if ((wlc->ier_tdls_scf = wlc_ier_create_registry(wlc->ieri,
		IEM_TDLS_SCF_BUILD_CB_MAX, IEM_TDLS_SCF_PARSE_CB_MAX)) == NULL) {
		WL_ERROR(("wl%d: %s: wlc_ier_create_registry failed, tdls setup confirm\n",
			unit, __FUNCTION__));
		err = 103;
		goto fail;
	}
	/* TDLS Discovery Response Action Frame */
	if ((wlc->ier_tdls_drs = wlc_ier_create_registry(wlc->ieri,
		IEM_TDLS_DRS_BUILD_CB_MAX, IEM_TDLS_DRS_PARSE_CB_MAX)) == NULL) {
		WL_ERROR(("wl%d: %s: wlc_ier_create_registry failed, tdls disc resp\n",
			unit, __FUNCTION__));
		err = 104;
		goto fail;
	}
#endif /* defined(WLTDLS) && !defined(WLTDLS_DISABLED) */
#ifdef WL11AC
	/* CS Wrapper IE */
	if ((wlc->ier_csw = wlc_ier_create_registry(wlc->ieri, 2, 2)) == NULL) {
		WL_ERROR(("wl%d: %s: wlc_ier_create_registry failed, cs wrapper ie\n",
		          unit, __FUNCTION__));
		err = 105;
		goto fail;
	}
#endif // endif
#if (defined(WLFBT) && !defined(WLFBT_DISABLED))
	/* FBT over the DS Action frame */
	if ((wlc->ier_fbt = wlc_ier_create_registry(wlc->ieri, 3, 3)) == NULL) {
		WL_ERROR(("wl%d: %s: wlc_ier_create_registry failed, fbt over the ds\n",
		          unit, __FUNCTION__));
		err = 106;
		goto fail;
	}
	/* FBT RIC request */
	if ((wlc->ier_ric = wlc_ier_create_registry(wlc->ieri, 2, 0)) == NULL) {
		WL_ERROR(("wl%d: %s: wlc_ier_create_registry failed, fbt ric request\n",
		          unit, __FUNCTION__));
		err = 107;
		goto fail;
	}
#endif /* WLFBT && !WLFBT_DISABLED */
	/* Create IE mgmt callback registry for NAN */

	return 0;

fail:
	WL_ERROR(("wl%d: %s: module %d failed to attach\n",
	          unit, __FUNCTION__, err));
	return err;
} /* wlc_attach_module_pre */

/** attach modules - add new modules to the end in general */
static uint
BCMATTACHFN(wlc_attach_module)(wlc_info_t *wlc)
{
	uint err = 0;
	uint prev_num = 0;
	uint unit = wlc->pub->unit;
	MODULE_ATTACH(eventq, wlc_eventq_attach, 1);
	MODULE_ATTACH(txqi, wlc_txq_attach, 2);

	/* construct scan data */
	wlc->scan = wlc_scan_attach(wlc, wlc->wl, wlc->osh, unit);
	if (!wlc->scan) {
		WL_ERROR(("wl%d: %s: failed to malloc scan struct\n", unit, __FUNCTION__));
		err = 12;
		goto fail;
	}
	MODULE_ATTACH(as, wlc_assoc_attach, 13);
	MODULE_ATTACH(qosi, wlc_qos_attach, 14);

	if (wlc_chctx_attach(wlc->chctxi) != BCME_OK) {
		err = 15;
		goto fail;
	}

#ifdef WLOLPC
	MODULE_ATTACH(olpc_info, wlc_olpc_eng_attach, 16 /* olpc */);
#endif // endif

#if defined(WLPROBRESP_SW) && !defined(WLPROBRESP_SW_DISABLED)
	MODULE_ATTACH(mprobresp, wlc_probresp_attach, 18);
#endif /* WLPROBRESP_SW && !WLPROBRESP_SW_DISABLED */

	MODULE_ATTACH(prot, wlc_prot_attach, 19);
	MODULE_ATTACH(prot_g, wlc_prot_g_attach, 20);
	MODULE_ATTACH(prot_n, wlc_prot_n_attach, 21);

	MODULE_ATTACH(ap, wlc_ap_attach, 22); /* ap_attach() depends on wlc->scbstate */
#ifdef WLLED
	MODULE_ATTACH(ledh, wlc_led_attach, 23);
#endif // endif
#ifdef WL11N
	MODULE_ATTACH(asi, wlc_antsel_attach, 24);
#endif // endif
#if defined(STA) && defined(WLRM) && !defined(WLRM_DISABLED)
	MODULE_ATTACH(rm_info, wlc_rm_attach, 25);
#endif // endif
#if defined(WLC_OFFLOADS_TXSTS) || defined(WLC_OFFLOADS_RXSTS)
	MODULE_ATTACH(offl, wlc_offload_attach, 26);
#endif /* WLC_OFFLOADS_TXSTS || WLC_OFFLOADS_RXSTS */
#ifdef WET
	MODULE_ATTACH(weth, wlc_wet_attach, 27);
#endif // endif
#ifdef WET_TUNNEL
	MODULE_ATTACH(wetth, wlc_wet_tunnel_attach, 28);
#endif // endif
#ifdef WLAMSDU
	/*
	 * assume core->xmtfifo_sz have been initialized and is not band specfic
	 *   otherwise need to call wlc_amsdu_agg_limit_upd() whenever that changes
	 */
	MODULE_ATTACH(ami, wlc_amsdu_attach, 29);
#endif // endif
#ifdef WLAMPDU
	MODULE_ATTACH(ampdu_tx, wlc_ampdu_tx_attach, 31);
	MODULE_ATTACH(ampdu_rx, wlc_ampdu_rx_attach, 32);
	if (wlc_ampdu_init(wlc)) {
		err = 32;
		goto fail;
	}
#endif // endif
#ifdef WMF
	MODULE_ATTACH(wmfi, wlc_wmf_attach, 34);
#endif // endif
	MODULE_ATTACH(wrsi, wlc_scb_ratesel_attach, 35);

#ifdef WLPLT
	if (WLPLT_ENAB(wlc->pub)) {
		MODULE_ATTACH(plt, wlc_plt_attach, 36);
	}
#endif // endif
#if defined(AP)
	MODULE_ATTACH(pmq, wlc_pmq_attach, 37);
#endif // endif
#if defined(WLCAC) && !defined(WLCAC_DISABLED)
	MODULE_ATTACH(cac, wlc_cac_attach, 41);
#endif // endif
	/* allocate the sequence commands info struct */
	if ((wlc->seq_cmds_info = wlc_seq_cmds_attach(wlc, wlc_ioctl)) == NULL) {
		WL_ERROR(("wl%d: %s: wlc_seq_cmds_attach failed\n", unit, __FUNCTION__));
		err = 42;
		goto fail;
	}

#if defined(WLMCNX) && !defined(WLMCNX_DISABLED)
	MODULE_ATTACH(mcnx, wlc_mcnx_attach, 43);
	MODULE_ATTACH(tbtt, wlc_tbtt_attach, 44);
#endif // endif
#if defined(WLP2P) && !defined(WLP2P_DISABLED)
	MODULE_ATTACH(p2p, wlc_p2p_attach, 45);
#endif // endif
#ifdef WOWL
	/* If hw is capable, then attempt to start wowl feature */
	if (wlc_wowl_cap(wlc)) {
		MODULE_ATTACH(wowl, wlc_wowl_attach, 46);
		wlc->pub->_wowl = TRUE;
	} else
		wlc->pub->_wowl = FALSE;
#endif // endif

#if defined(BCMAUTH_PSK) && !defined(BCMAUTH_PSK_DISABLED)
	MODULE_ATTACH(authi, wlc_auth_attach, 48);
	wlc->pub->_bcmauth_psk = TRUE;
#endif // endif

#if defined(RWL_WIFI) || defined(WIFI_REFLECTOR)
	MODULE_ATTACH(rwl, wlc_rwl_attach, 49);
#endif // endif

#if defined(WL11K) && !defined(WL11K_DISABLED)
	MODULE_ATTACH(rrm_info, wlc_rrm_attach, 52);
#endif /* WL11K && !WL11K_DISABLED */
#if defined(WLWNM) && !defined(WLWNM_DISABLED)
	MODULE_ATTACH(wnm_info, wlc_wnm_attach, 54);
#endif // endif
	MODULE_ATTACH(hti, wlc_ht_attach, 55);
#ifdef WL11AC
	MODULE_ATTACH(vhti, wlc_vht_attach, 56);
#endif // endif
	MODULE_ATTACH(lqi, wlc_lq_attach, 57);

#if defined(WL_MODESW) && !defined(WL_MODESW_DISABLED)
	MODULE_ATTACH(modesw, wlc_modesw_attach, 58);
#endif // endif

/* Attach the RSDB module before modules that register for
* Clone callback. Ex CSA and TDLS.
*/
#if defined(WLRSDB)
	if (RSDB_ENAB(wlc->pub)) {
	    MODULE_ATTACH(rsdbinfo, wlc_rsdb_attach, 59);
	}

#if defined(WLRSDB_POLICY_MGR) && !defined(WLRSDB_POLICY_MGR_DISABLED)
	MODULE_ATTACH(rsdb_policy_info, wlc_rsdb_policymgr_attach, 61);
#endif /* WLRSDB_POLICY_MGR && WLRSDB */
#endif /* WLRSDB */

#if defined(WLMCHAN) && !defined(WLMCHAN_DISABLED)
	MODULE_ATTACH(mchan, wlc_mchan_attach, 62);
#endif // endif
	if ((wlc_stf_attach(wlc) != 0)) {
		WL_ERROR(("wl%d: %s: wlc_stf_attach failed\n", unit, __FUNCTION__));
		err = 62;
		goto fail;
	}

#ifdef WL11H
	MODULE_ATTACH(m11h, wlc_11h_attach, 63);
#ifdef WLCSA
	MODULE_ATTACH(csa, wlc_csa_attach, 64);
#endif // endif
#ifdef WLQUIET
	MODULE_ATTACH(quiet, wlc_quiet_attach, 65);
#endif // endif
#if defined WLDFS && !defined(WLDFS_DISABLED)
	MODULE_ATTACH(dfs, wlc_dfs_attach, 66);
#endif // endif
#endif /* WL11H */

#ifdef WLTPC
	MODULE_ATTACH(tpc, wlc_tpc_attach, 67);
#endif // endif

#if defined(L2_FILTER) && !defined(L2_FILTER_DISABLED)
	MODULE_ATTACH(l2_filter, wlc_l2_filter_attach, 68);
	wlc->pub->_l2_filter = TRUE;
#endif /* L2_FILTER && !L2_FILTER_DISABLED */

#ifdef WL11D
	MODULE_ATTACH(m11d, wlc_11d_attach, 69);
#endif // endif
#ifdef WLCNTRY
	MODULE_ATTACH(cntry, wlc_cntry_attach, 71);
#endif // endif
#ifndef MEDIA_CFG /* TBD:Enable for MEDIA_CFG also */
#ifdef CCA_STATS
#ifndef CCA_STATS_DISABLED
	MODULE_ATTACH(cca_info, wlc_cca_attach, 72);
#endif /* CCA_STATS_DISABLED */
#endif /* MEDIA_CFG */
#ifdef ISID_STATS
	MODULE_ATTACH(itfr_info, wlc_itfr_attach, 73);
#endif // endif
#endif /* CCA_STATS */

#if defined(WL11U) && !defined(WL11U_DISABLED)
	MODULE_ATTACH(m11u, wlc_11u_attach, 75);
#endif // endif
#ifdef WLDLS
	MODULE_ATTACH(dls, wlc_dls_attach, 76);
#endif // endif
#if defined(WLBSSLOAD) || (defined(WLBSSLOAD_REPORT) && \
	!defined(WLBSSLOAD_REPORT_DISABLED))
	MODULE_ATTACH(mbssload, wlc_bssload_attach, 77);
#endif /* WLBSSLOAD */
#ifdef PSTA
	MODULE_ATTACH(psta, wlc_psta_attach, 78);
#endif /* PSTA */
	MODULE_ATTACH(txc, wlc_txc_attach, 79);
#if defined(WOWLPF)
	/* If hw is capable, then attempt to start wowl feature */
	if (WOWLPF_ENAB(wlc->pub)) {
		if (wlc_wowlpf_cap(wlc)) {
			MODULE_ATTACH(wowlpf, wlc_wowlpf_attach, 81);
			wlc->pub->_wowlpf = TRUE;
		} else
			wlc->pub->_wowlpf = FALSE;
	}
#endif // endif
#ifdef WLOTA_EN
	if ((wlc->ota_info = wlc_ota_test_attach(wlc)) == NULL) {
		WL_ERROR(("wl%d: wlc_attach: wlc_ota_test_attach failed\n", unit));
		err = 80;
		goto fail;
	}
#endif /* WLOTA_EN */

#ifdef WL_LPC
	/* attach power sel only if the rate sel is already up */
	MODULE_ATTACH(wlpci, wlc_scb_lpc_attach, 82);
#endif // endif

#if defined(MFP) && !defined(MFP_DISABLED)
	MODULE_ATTACH(mfp, wlc_mfp_attach, 83);
#endif // endif

	MODULE_ATTACH(macfltr, wlc_macfltr_attach, 84);

#ifdef WLNAR
	MODULE_ATTACH(nar_handle, wlc_nar_attach, 85);
#endif // endif

#if (defined(BCMSUP_PSK) && defined(BCMINTSUP) && !defined(BCMSUP_PSK_DISABLED))
	MODULE_ATTACH(idsup, wlc_sup_attach, 86);
	wlc->pub->_sup_enab = TRUE;
#endif /* (BCMSUP_PSK && BCMINTSUP && !BCMSUP_PSK_DISABLED) */

	MODULE_ATTACH(akmi, wlc_akm_attach, 87);

#ifdef STA
	MODULE_ATTACH(hs20, wlc_hs20_attach, 88);
#endif	/* STA */

#if (defined(WLFBT) && !defined(WLFBT_DISABLED))
	MODULE_ATTACH(fbt, wlc_fbt_attach, 89);
#endif // endif

	MODULE_ATTACH(pmkid_info, wlc_pmkid_attach, 91);

	MODULE_ATTACH(btch, wlc_btc_attach, 92);

#ifdef WL_STAPRIO
	MODULE_ATTACH(staprio, wlc_staprio_attach, 94);
#endif /* WL_STAPRIO */

#if defined(WL_MONITOR) && !defined(WL_MONITOR_DISABLED)
	/* Attach monitor module to wlc */
	MODULE_ATTACH(mon_info, wlc_monitor_attach, 95);
#endif /* WL_MONITOR && !WL_MONITOR_DISABLED */
#ifdef WL_STA_MONITOR
	MODULE_ATTACH(stamon_info, wlc_stamon_attach, 96);
#endif /* WL_STA_MONITOR */

#ifdef WDS
	MODULE_ATTACH(mwds, wlc_wds_attach, 97);
#endif /* WDS */

#if (defined(WL_OKC) && !defined(WL_OKC_DISABLED)) || (defined(WLRCC) && \
	!defined(WLRCC_DISABLED))
	MODULE_ATTACH(okc_info, wlc_okc_attach, 98 /* OKC */);
#endif /*  defined(WL_OKC) || defined(WLRCC) */

#if defined(WLWSEC) && !defined(WLWSEC_DISABLED)
	MODULE_ATTACH(keymgmt, wlc_keymgmt_attach, 100);
#endif /* WLWSEC && !WLWSEC_DISABLED */

#if defined(WL_RATELINKMEM) && !defined(WL_RATELINKMEM_DISABLED)
	MODULE_ATTACH(rlmi, wlc_ratelinkmem_attach, 101);
#endif /* WL_RATELINKMEM && !WL_RATELINKMEM_DISABLED */

#if defined(WL_BEAMFORMING) && !defined(WLTXBF_DISABLED)
	MODULE_ATTACH(txbf, wlc_txbf_attach, 102);
#endif /* WL_BEAMFORMING */

#if defined(WL_STATS) && !defined(WL_STATS_DISABLED)
	MODULE_ATTACH(stats_info, wlc_stats_attach, 103);
#endif /* WL_STATS && WL_STATS_DISABLED */

#if defined(SCB_BS_DATA)
	MODULE_ATTACH(bs_data_handle, wlc_bs_data_attach, 104);
#endif // endif

	MODULE_ATTACH(obss, wlc_obss_attach, 105);

#ifdef WLDURATION
	MODULE_ATTACH(dur_info, wlc_duration_attach, 106);
#endif /* WLDURATION */

#if defined(WL_PWRSTATS) && !defined(WL_PWRSTATS_DISABLED)
	MODULE_ATTACH(pwrstats, wlc_pwrstats_attach, 108);
#endif /* PWRSTATS */

#if defined(WLTDLS) && !defined(WLTDLS_DISABLED)
	MODULE_ATTACH(tdls, wlc_tdls_attach, 110);
#endif /* defined(WLTDLS) && !defined(WLTDLS_DISABLED) */
#if defined(WLSLOTTED_BSS)
	MODULE_ATTACH(sbi, wlc_slotted_bss_attach, 111);
#endif // endif

#if defined(RADIONOA_PWRSAVE) && !defined(RADIONOA_PWRSAVE_DISABLED)
	MODULE_ATTACH(rpsnoa, wlc_rpsnoa_attach, 127);
#endif /* RADIONOA_PWRSAVE && !RADIONOA_PWRSAVE_DISABLED */

#if defined(MBSS) && !defined(MBSS_DISABLED)
	MODULE_ATTACH(mbss, wlc_mbss_attach, 130);
#endif /* MBSS && !MBSS_DISABLED */

#if defined(BCMLTECOEX) && !defined(BCMLTECOEX_DISABLED)
	MODULE_ATTACH(ltecx, wlc_ltecx_attach, 131);
#endif /* defined(BCMLTECOEX) && !defined(BCMLTECOEX_DISABLED) */

#if defined(WLPROBRESP_MAC_FILTER) && !defined(WLPROBRESP_SW_DISABLED)
	MODULE_ATTACH(mprobresp_mac_filter, wlc_probresp_mac_filter_attach, 132);
#endif // endif

#if defined(WL_BWTE) && !defined(WL_BWTE_DISABLED)
	MODULE_ATTACH(bwte_info, wlc_bwte_attach, 133);
	wlc->pub->_bwte = TRUE;
#endif /* defined(WL_BWTE) && !defined(WL_BWTE_DISABLED) */

#if defined(APCS) && !defined(APCS_DISABLED)
	MODULE_ATTACH(cs, wlc_apcs_attach, 134);
#endif /* APCS && !APCS_DISABLED */

#ifdef WL_LTR
	MODULE_ATTACH(ltr_info, wlc_ltr_attach, 135);
#endif /* WL_LTR */

#if defined(WL_TBOW) && !defined(WL_TBOW_DISABLED)
	MODULE_ATTACH(tbow_info, wlc_tbow_attach, 136);
	wlc->pub->_tbow = TRUE;
#endif /* defined(WL_TBOW) && !defined(WL_TBOW_DISABLED) */

#if defined(WL_PROT_OBSS) && !defined(WL_PROT_OBSS_DISABLED)
	MODULE_ATTACH(prot_obss, wlc_prot_obss_attach, 139);
#endif // endif

#ifdef WL_PM_MUTE_TX
	MODULE_ATTACH(pm_mute_tx, wlc_pm_mute_tx_attach, 140);
#endif // endif

#if defined(WL_BCNTRIM) && !defined(WL_BCNTRIM_DISABLED)
	MODULE_ATTACH(bcntrim, wlc_bcntrim_attach, 141);
#endif // endif

#if defined(WL_RELMCAST) && !defined(WL_RELMCAST_DISABLED)
	MODULE_ATTACH(rmc, wlc_rmc_attach, 142 /* reliable multicast */);
#endif // endif

#if defined(WL_PROXDETECT) && !defined(WL_PROXDETECT_DISABLED)
	/* Attach proxmity detection service to wlc */
	if (wlc_is_proxd_supported(wlc))
	{
	MODULE_ATTACH(pdsvc_info, wlc_proxd_attach, 143);
	}
#endif // endif

	MODULE_ATTACH(misc, wlc_misc_attach, 144);
#if defined(SMF_STATS) && !defined(SMF_STATS_DISABLED)
	MODULE_ATTACH(smfs, wlc_smfs_attach, 145);
#endif // endif

	MODULE_ATTACH(macdbg, wlc_macdbg_attach, 146);

#ifdef WL_BSSCFG_TX_SUPR
	MODULE_ATTACH(psqi, wlc_bsscfg_psq_attach, 147);
#endif /* WL_BSSCFG_TX_SUPR */

#if defined(WLVASIP) && !defined(WLVASIP_DISABLED)
	MODULE_ATTACH(vasip, wlc_vasip_attach, 148);
#endif /* WLVASIP */

#if defined(WL_LINKSTAT) && !defined(WL_LINKSTAT_DISABLED)
	MODULE_ATTACH(linkstats_info, wlc_linkstats_attach, 149);
#endif /* WL_LINKSTAT */

	MODULE_ATTACH(vieli, wlc_bsscfg_viel_attach, 150);

#if defined(GTKOE) && !defined(GTKOE_DISABLED)
	MODULE_ATTACH(idsup, wlc_gtk_attach, 151);
	wlc->pub->_gtkoe = TRUE;
#endif /* defined(GTKOE) && !defined(GTKOE_DISABLED) */

#if defined(WL_MU_RX) && !defined(WL_MU_RX_DISABLED)
	MODULE_ATTACH(murx, wlc_murx_attach, 152);
#endif // endif

#if defined(PKTC) || defined(PKTC_DONGLE)
	MODULE_ATTACH(pktc_info, wlc_pktc_attach, 154);
#endif // endif

#if defined(WL_OBSS_DYNBW) && !defined(WL_OBSS_DYNBW_DISABLED)
	MODULE_ATTACH(obss_dynbw, wlc_obss_dynbw_attach, 155);
#endif /* defined(WL_OBSS_DYNBW) && !defined(WL_OBSS_DYNBW_DISABLED) */

#if defined(PROP_TXSTATUS) && defined(PROP_TXSTATUS_ENABLED)
	MODULE_ATTACH(wlfc, wlc_wlfc_attach, 156);
#endif // endif

#if defined(BCMULP) && !defined(BCMULP_DISABLED)
	MODULE_ATTACH(ulp, wlc_ulp_attach, 157);
#endif // endif

	MODULE_ATTACH(frag, wlc_frag_attach, 158);
	MODULE_ATTACH(pm, wlc_pm_attach, 160);
	MODULE_ATTACH(chanctxt_info, wlc_chanctxt_attach, 161);
#ifdef STA
	MODULE_ATTACH(sta_info, wlc_sta_attach, 162);
#endif // endif
	MODULE_ATTACH(msch_info, wlc_msch_attach, 163);
#if defined(BCMCOEXNOA) && !defined(BCMCOEXNOA_DISABLED)
	MODULE_ATTACH(cxnoa, wlc_cxnoa_attach, 164);
#endif // endif

#ifdef WIFI_ACT_FRAME
	MODULE_ATTACH(wlc_act_frame_info, wlc_act_frame_attach, 165);
#endif // endif
	MODULE_ATTACH(rspeci, wlc_rspec_attach, 166);

#if defined(WL_HEALTH_CHECK)
	MODULE_ATTACH(hc, wlc_health_check_attach, 167);
#endif // endif
	MODULE_ATTACH(sui, wlc_scan_utils_attach, 168);

	if (wlc_addrmatch_attach(wlc) != BCME_OK) {
		err = 169;
		goto fail;
	}
	MODULE_ATTACH(pui, wlc_perf_utils_attach, 170);
	MODULE_ATTACH(testi, wlc_test_attach, 171);

#if defined(SRHWVSDB) && !defined(SRHWVSDB_DISABLED)
	MODULE_ATTACH(srvsdb_info, wlc_srvsdb_attach, 172);
#endif // endif

#ifdef WL_RANDMAC
	MODULE_ATTACH(randmac_info, wlc_randmac_attach, 173);
#endif /* WL_RANDMAC */

#if defined(WL_ASSOC_MGR) && !defined(WL_ASSOC_MGR_DISABLED)
	MODULE_ATTACH(assoc_mgr, wlc_assoc_mgr_attach, 174);
#endif /* defined(WL_ASSOC_MGR) && !defined(WL_ASSOC_MGR_DISABLED) */

#if defined(WL_ASDB) && !defined(WLC_ASDB_DISABLED)
	MODULE_ATTACH(asdb, wlc_asdb_attach, 175);
#endif /* WL_ASDB */

#if defined(WLC_DEBUG_CRASH)
	MODULE_ATTACH(debug_crash_info, wlc_debug_crash_attach, 176);
#endif /* WLC_DEBUG_CRASH */

#if defined(WLC_SW_DIVERSITY) && !defined(WLC_SW_DIVERSITY_DISABLED)
		MODULE_ATTACH(swdiv, wlc_swdiv_attach, 177);
#endif /* WLC_SW_DIVERSITY && !WLC_SW_DIVERSITY_DISABLED */

	if (wlc_dump_attach(wlc->dumpi) != BCME_OK) {
		err = 178;
		goto fail;
	}

#if defined(WLC_TSYNC)
	MODULE_ATTACH(tsync, wlc_tsync_attach, 179);
#endif /* WLC_TSYNC */

#ifdef WLHEB
	MODULE_ATTACH(hebi, wlc_heb_attach, 180);
#endif // endif

#ifdef WL11AX
	MODULE_ATTACH(hei, wlc_he_attach, 181);
#endif // endif
#ifdef WLTWT
	MODULE_ATTACH(twti, wlc_twt_attach, 182);
#endif // endif
#ifdef WLC_TXCAL
	MODULE_ATTACH(cldi, wlc_calload_attach, 183);
#endif /* WLC_TXCAL */

	MODULE_ATTACH(txcfg, wlc_txcfg_attach, 185);

#if defined(WL_MBO_OCE) && !defined(WL_MBO_OCE_DISABLED)
	MODULE_ATTACH(mbo_oce, wlc_mbo_oce_attach, 186);
#endif /* WL_MBO_OCE && !WL_MBO_OCE_DISABLED */

#ifdef WL_RX_STALL
	/* Register RX health check module */
	MODULE_ATTACH(rx_hc, wlc_rx_hc_attach, 187);
#endif /* WL_RX_STALL */

#ifdef WL_TX_STALL
	/* Register TX stall health check module */
	MODULE_ATTACH(tx_stall, wlc_tx_stall_attach, 188);
#endif /* WL_TX_STALL */

#if defined(WL_ESP) && !defined(WL_ESP_DISABLED)
	/* Register ESP module */
	MODULE_ATTACH(esp, wlc_esp_attach, 189);
#endif /* WL_ESP && !WL_ESP_DISABLED */

#if defined(WL_MBO) && defined(MBO_AP)
	MODULE_ATTACH(mbo, wlc_mbo_ap_attach, 191);
#endif /* WL_MBO && MBO_AP */

#if defined(WL_FILS) && !defined(WL_FILS_DISABLED)
	/* Register FILS module */
	MODULE_ATTACH(fils, wlc_fils_attach, 193);
#endif /* WL_FILS && !WL_FILS_DISABLED */

#if defined(WL_LEAKY_AP_STATS) && !defined(WL_LEAKYAPSTATS_DISABLED)
	MODULE_ATTACH(leakyapstats_info, wlc_leakyapstats_attach, 194);
#endif /* WL_LEAKY_AP_STATS */

#if defined(WLC_ADPS) && !defined(WLC_ADPS_DISABLED)
	MODULE_ATTACH(adps_info, wlc_adps_attach, 195);
#endif /* WLC_ADPS && !WLC_ADPS_DISABLED */

#if defined(WLC_BAM) && !defined(WLC_BAM_DISABLED)
	MODULE_ATTACH(bam_info, wlc_bam_attach, 196);
#endif /* WLC_BAM && !WLC_BAM_DISABLED */

#if defined(WL_FILTERIE) && !defined(WL_FILTERIE_DISABLED)
	MODULE_ATTACH(fiei, wlc_filter_ie_attach, 197);
#endif /* WL_FILTERIE && WL_FILTERIE_DISABLED */

#if defined(WL_TVPM) && !defined(WL_TVPM_DISABLED)
	MODULE_ATTACH(tvpm, wlc_tvpm_attach, 198);
#endif /* WL_TVPM && !WL_TVPM_DISABLED */

#if defined(WL_OPS) && !defined(WL_OPS_DISABLED)
	MODULE_ATTACH(ops_info, wlc_ops_attach, 199);
#endif /* WL_OPS */

#ifdef WLCFP
	/* Register the CFP Module */
	MODULE_ATTACH(cfp, wlc_cfp_attach, 200);
#endif // endif
#if defined(TDMTX) && !defined(TDMTX_DISABLED)
	MODULE_ATTACH(tdmtx, wlc_tdm_tx_attach, 201);
#endif /* TDMTX && ! TDM_TX_DISABLED */
#if defined(WL_OCE) && !defined(WL_OCE_DISABLED)
	/* Register OCE module */
	MODULE_ATTACH(oce, wlc_oce_attach, 202);
#endif /* WL_OCE && !WL_OCE_DISABLED */

#if defined(WL_MU_TX) && !defined(WL_MU_TX_DISABLED)
	MODULE_ATTACH(mutx, wlc_mutx_attach, 203);
#endif /* WL_MU_TX && !WL_MU_TX_DISABLED */

#if defined(WL_AIR_IQ)
	if (AIRIQ_ENAB()) {
		MODULE_ATTACH(airiq, wlc_airiq_attach, 204);
	}
#endif /* WL_AIR_IQ */

#ifdef WLTAF
	MODULE_ATTACH(taf_handle, wlc_taf_attach, 206);
#endif // endif
	MODULE_ATTACH(macreq, wlc_macreq_attach, 207);
#ifdef WLSQS
	/* Register the SQS Module */
	MODULE_ATTACH(sqs, wlc_sqs_attach, 208);
#endif // endif
#ifdef PKTQ_LOG
	MODULE_ATTACH(rx_report_handle, wlc_rx_report_attach, 209);
#endif // endif
#ifndef ATE_BUILD
#if defined(WL_MUSCHEDULER) && !defined(WL_MUSCHEDULER_DISABLED)
	MODULE_ATTACH(musched, wlc_muscheduler_attach, 210);
#endif /* WL_MUSCHEDULER && !WL_MUSCHEDULER_DISABLED */
#endif /* !ATE_BUILD */
	MODULE_ATTACH(fifo, wlc_fifo_attach, 211);
	return BCME_OK;

fail:
	WL_ERROR(("wl%d: %s: module %d failed to attach\n",
	          unit, __FUNCTION__, err));
	return err;
} /* wlc_attach_module */

wlc_pub_t *
wlc_pub(void *wlc)
{
	return ((wlc_info_t *)wlc)->pub;
}

void wlc_get_override_vendor_dev_id(void *wlc, uint16 *vendorid, uint16 *devid)
{
	*vendorid = ((wlc_info_t *)wlc)->vendorid;
	*devid = ((wlc_info_t *)wlc)->deviceid;
}

/* allocate the hwrs scbs */
int
wlc_hwrsscbs_alloc(wlc_info_t *wlc)
{
	uint j;

	for (j = 0; j < NBANDS(wlc); j++) {
		/* Use band 1 for single band 11a */
		if (IS_SINGLEBAND_5G(wlc->deviceid, wlc->phy_cap))
			j = BAND_5G_INDEX;

		if (wlc->bandstate[j]->hwrs_scb == NULL) {
			wlc->bandstate[j]->hwrs_scb = wlc_hwrsscb_alloc(wlc, wlc->bandstate[j]);
		}
		if (!wlc->bandstate[j]->hwrs_scb) {
			WL_ERROR(("wl%d: %s: wlc_hwrsscb_alloc failed\n",
			          wlc->pub->unit, __FUNCTION__));
			return BCME_NOMEM;
		}
		bcopy(&wlc->bandstate[j]->hw_rateset, &wlc->bandstate[j]->hwrs_scb->rateset,
			sizeof(wlc_rateset_t));
	}

	return BCME_OK;
}

/* free the hwrs scbs */
void
wlc_hwrsscbs_free(wlc_info_t *wlc)
{
	uint i;

	for (i = 0; i < NBANDS(wlc); i++) {
		wlcband_t *bandstate = (NBANDS(wlc) == 1) ? wlc->band:wlc->bandstate[i];

		if (bandstate->hwrs_scb && obj_registry_islast(wlc->objr)) {
			wlc_hwrsscb_free(wlc, bandstate->hwrs_scb);
			bandstate->hwrs_scb = NULL;
		}
	}
}

#define CHIP_SUPPORTS_11AC(sih) TRUE

static const char BCMATTACHDATA(rstr_devid)[] = "devid";
static const char BCMATTACHDATA(rstr_11n_disable)[] = "11n_disable";
static const char BCMATTACHDATA(rstr_tx_duty_cycle_ofdm)[] = "tx_duty_cycle_ofdm";
static const char BCMATTACHDATA(rstr_tx_duty_cycle_cck)[] = "tx_duty_cycle_cck";
static const char BCMATTACHDATA(rstr_tx_duty_cycle_ofdm_40_5g)[] = "tx_duty_cycle_ofdm_40_5g";
static const char BCMATTACHDATA(rstr_tx_duty_cycle_thresh_40_5g)[] = "tx_duty_cycle_thresh_40_5g";
static const char BCMATTACHDATA(rstr_tx_duty_cycle_ofdm_80_5g)[] = "tx_duty_cycle_ofdm_80_5g";
static const char BCMATTACHDATA(rstr_tx_duty_cycle_thresh_80_5g)[] = "tx_duty_cycle_thresh_80_5g";
static const char BCMATTACHDATA(rstr_tx_duty_cycle_max)[] = "tx_duty_cycle_max";
static const char BCMATTACHDATA(rstr_NVRAMRev)[] = "NVRAMRev";
static const char BCMATTACHDATA(rstr_aa2g)[] = "aa2g";
static const char BCMATTACHDATA(rstr_aa5g)[] = "aa5g";
static const char BCMATTACHDATA(rstr_aga0)[] = "aga0";
static const char BCMATTACHDATA(rstr_agbg0)[] = "agbg0";
static const char BCMATTACHDATA(rstr_aga1)[] = "aga1";
static const char BCMATTACHDATA(rstr_agbg1)[] = "agbg1";
static const char BCMATTACHDATA(rstr_aga2)[] = "aga2";
static const char BCMATTACHDATA(rstr_agbg2)[] = "agbg2";
static const char BCMATTACHDATA(rstr_aa0)[] = "aa0";
static const char BCMATTACHDATA(rstr_aa1)[] = "aa1";
static const char BCMATTACHDATA(rstr_ag0)[] = "ag0";
static const char BCMATTACHDATA(rstr_ag1)[] = "ag1";
static const char BCMATTACHDATA(rstr_sar5g)[] = "sar5g";
static const char BCMATTACHDATA(rstr_sar2g)[] = "sar2g";

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

static int
BCMATTACHFN(wlc_attach_cb_init)(wlc_info_t *wlc)
{
	int code = 0;

	/* register a module (to handle iovars) */
	if (wlc_iocv_add_iov_fn(wlc->iocvi, wlc_iovars, wlc_doiovar, wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iocv_add_iov_fn() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		code = 1;
		goto fail;
	}

	/* register module IOCTL handlers */
	if (wlc_iocv_add_ioc_fn(wlc->iocvi, NULL, 0, wlc_doioctl, wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iocv_add_ioc_fn() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		code = 10;
		goto fail;
	}

#ifdef STA
	/* register packet class callback */
	if (wlc_pcb_fn_set(wlc->pcb, 0, WLF2_PCB1_PSP, wlc_sendpspoll_complete) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_pcb_fn_set(pspoll) failed\n",
		          wlc->pub->unit, __FUNCTION__));
		code = 12;
		goto fail;
	}
	if (wlc_pcb_fn_set(wlc->pcb, 0, WLF2_PCB1_TKIP_CM, wlc_tkip_countermeasures) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_pcb_fn_set(tkip) failed\n",
		          wlc->pub->unit, __FUNCTION__));
		code = 13;
		goto fail;
	}
	if (wlc_pcb_fn_set(wlc->pcb, 0, WLF2_PCB1_RATE_PRB, wlc_rateprobe_complete) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_pcb_fn_set(rate) failed\n",
		          wlc->pub->unit, __FUNCTION__));
		code = 14;
		goto fail;
	}
	if (wlc_pcb_fn_set(wlc->pcb, 0, WLF2_PCB1_PM_NOTIF, wlc_pm_notif_complete) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_pcb_fn_set(pmchg) failed\n",
		          wlc->pub->unit, __FUNCTION__));
		code = 15;
		goto fail;
	}
#endif /* STA */

	return BCME_OK;

fail:
	WL_ERROR(("wl%d: %s: code %d\n", wlc->pub->unit, __FUNCTION__, code));
	BCM_REFERENCE(code);
	return BCME_ERROR;
} /* wlc_attach_cb_init */

#ifdef BCMULP
/* In ULP mode, for warm boot, called by wlc_attach before ucode download.
 * This will contain to pre-attach of modules which need to retrieve and store data from wowl-ucode.
 */
static int
wlc_module_preattach(void)
{
	int err = BCME_OK;
	if (!si_is_warmboot()) {
		goto done;
	}
	err = wlc_ulp_preattach();
	if (err != BCME_OK)
		goto done;
	err = wlc_bmac_ulp_preattach();
	if (err != BCME_OK)
		goto done;
#ifdef BCMSUP_PSK
	err = wlc_sup_preattach();
	if (err != BCME_OK)
		goto done;
#endif // endif
#if defined(GTKOE) && defined(BCMULP)
	err = wlc_gtkoe_preattach();
	if (err != BCME_OK)
		goto done;
#endif // endif
	err = wlc_keymgmt_preattach();
	if (err != BCME_OK)
		goto done;
done:
	return err;
}
#endif /* defined(BCMULP) */

#if defined(WLATF) && defined(WLATF_PERC)
uint8
wlc_atm_cal_perc(wlc_info_t *wlc, uint8 perc_sum, uint8 num, uint8 auto_num, uint8 perc)
{
	uint8 ret = 0;

	if (perc_sum > 100) {
		ret = 100 / (num + auto_num);
		WL_ERROR(("wl%d: WARN: ATF percentage sum > 100"
			", use average(%2d%%) instead\n",
			wlc->pub->unit, ret));
	} else {
		if (!perc) {
			/* user not define perc, try to assign avg of remaining */
			ret = (100 - perc_sum) / auto_num;
		} else {
			if (!auto_num) {
				/* no one is auto,  assign perc + avg of remaining */
				ret = perc + (100 - perc_sum) / num;
			} else {
				/* someone is auto, just assign what we have */
				ret = perc;
			}
		}
	}

	ASSERT(ret <= 100);
	/* Allocate basic 1% if user make improper config */
	if (!ret)
		ret = 1;

	return ret;
}
#endif /* WLATF && WLATF_PERC */

#if defined(BCMDBG)
static void
wlc_dump_basic_rate(const char *name, uint8 *rates, uint count, struct bcmstrbuf *b)
{
	uint i;

	bcm_bprintf(b, "%s [ ", name ? name : "");
	for (i = 0; i < count; i ++) {
		bcm_bprintf(b, "%d ", rates[i]);
	}
	bcm_bprintf(b, "]");
}

static int
wlc_dump_ratesets(void *ctx, struct bcmstrbuf *b)
{
	wlc_info_t *wlc = ctx;
	char eabuf[ETHER_ADDR_STR_LEN];
	wlc_bsscfg_t *cfg;
	int idx;
	struct scb *scb;
	struct scb_iter scbiter;
	uint i;

	bcm_bprintf(b, ">>>> wlc %d up %d <<<<\n", wlc->pub->unit, wlc->pub->up);
	bcm_bprintf(b, "default_bss->rateset:\n");
	wlc_rateset_dump(&wlc->default_bss->rateset, b);
	for (i = 0; i < NBANDS(wlc); i ++) {
		/* Use band 1 for single band 11a */
		if (IS_SINGLEBAND_5G(wlc->deviceid, wlc->phy_cap)) {
			i = BAND_5G_INDEX;
		}
		bcm_bprintf(b, "bandstate[%d]->hw_rateset:\n", i);
		wlc_rateset_dump(&wlc->bandstate[i]->hw_rateset, b);
		bcm_bprintf(b, "bandstate[%d]->hwrs_scb->rateset:\n", i);
		wlc_rateset_dump(&wlc->bandstate[i]->hwrs_scb->rateset, b);
		bcm_bprintf(b, "bandstate[%d]->basic_rate:\n", i);
		wlc_dump_basic_rate("Basic", wlc->bandstate[i]->basic_rate,
		                    sizeof(wlc->bandstate[i]->basic_rate), b);
		bcm_bprintf(b, "\n");
	}

	FOREACH_BSS(wlc, idx, cfg) {
		bcm_bprintf(b, "==== cfg %d associated %d ====\n", idx, cfg->associated);
		bcm_bprintf(b, "target_bss->rateset:\n");
		wlc_rateset_dump(&cfg->target_bss->rateset, b);
		bcm_bprintf(b, "current_bss->rateset:\n");
		wlc_rateset_dump(&cfg->current_bss->rateset, b);
		if (BSSCFG_HAS_BCMC_SCB(cfg)) {
			bcm_bprintf(b, "bcmc_scb->rateset:\n");
			wlc_rateset_dump(&BSSCFG_BCMC_SCB(cfg)->rateset, b);
		}

		FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {
			bcm_bprintf(b, "scb[%s]->rateset:\n", bcm_ether_ntoa(&scb->ea, eabuf));
			wlc_rateset_dump(&scb->rateset, b);
		}
	}

	return BCME_OK;
}
#endif // endif

/**
 * The common driver entry routine. Error codes should be unique
 * @param [in] btparam - opaque pointer pointing to e.g. a Linux 'struct pci_dev'
 */
void *
BCMATTACHFN(wlc_attach)(void *wl, uint16 vendor, uint16 device, uint unit, uint iomode,
                      osl_t *osh, volatile void *regsva, uint bustype,
                      void *btparam, void *objr, uint *perr)
{
	wlc_info_t *wlc = NULL;
	uint err = 0;
	uint j;
	wlc_pub_t *pub;
	wlc_txq_info_t *qi;
	uint n_disabled;
	wlc_bmac_state_t state_bmac;
	char *nvramrev;
	si_t *sih = NULL;
	char *vars = NULL;
	uint vars_size = 0;
	int macunit = 0;
	wlc_hw_info_t *hw = NULL;
	uint32 phy_cap = 0;
#if defined(DONGLEBUILD) && !defined(PREATTACH_NORECLAIM) && !defined(BCMUCDOWNLOAD)
	uint8 ucode_reclaimed = 0;
#endif /* defined(DONGLEBUILD) && !defined(PREATTACH_NORECLAIM) && !defined(BCMUCDOWNLOAD) */

#ifdef BCMDBG
	/* wlc_info_t::last must be the last field of wlc_info_t */
	STATIC_ASSERT(sizeof(wlc_info_t) ==
	              OFFSETOF(wlc_info_t, last) + sizeof(((wlc_info_t *)0)->last));
	/* wlc_bsscfg_t::last must be the last field of wlc_bsscfg_t */
	STATIC_ASSERT(sizeof(wlc_bsscfg_t) ==
	              OFFSETOF(wlc_bsscfg_t, last) + sizeof(((wlc_info_t *)0)->last));
#endif // endif

#ifdef EVENT_LOG_COMPILE
	/* First thing to do.. initialize 'ATTACHLESS' component ERROR tag's attributes. */
	/* All errors are directed to log set error. */
	event_log_tag_start(EVENT_LOG_TAG_OBJR_ERROR, EVENT_LOG_SET_ERROR, EVENT_LOG_TAG_FLAG_LOG);
	event_log_tag_start(EVENT_LOG_TAG_MPOOL_ERROR, EVENT_LOG_SET_ERROR, EVENT_LOG_TAG_FLAG_LOG);
	event_log_tag_start(EVENT_LOG_TAG_PMU_ERROR, EVENT_LOG_SET_ERROR, EVENT_LOG_TAG_FLAG_LOG);
	event_log_tag_start(EVENT_LOG_TAG_BSROM_ERROR, EVENT_LOG_SET_ERROR, EVENT_LOG_TAG_FLAG_LOG);
	event_log_tag_start(EVENT_LOG_TAG_WL_ERROR, EVENT_LOG_SET_ERROR, EVENT_LOG_TAG_FLAG_LOG);
#endif // endif

	WL_TRACE(("wl%d: %s: vendor 0x%x device 0x%x\n", unit, __FUNCTION__, vendor, device));

	/* some code depends on packed structures */
	STATIC_ASSERT(sizeof(struct ether_addr) == ETHER_ADDR_LEN);
	STATIC_ASSERT(sizeof(struct ether_header) == ETHER_HDR_LEN);

	STATIC_ASSERT(sizeof(ofdm_phy_hdr_t) == D11_PHY_HDR_LEN);
	STATIC_ASSERT(sizeof(cck_phy_hdr_t) == D11_PHY_HDR_LEN);
	STATIC_ASSERT(sizeof(d11txh_pre40_t) == D11_TXH_LEN);
	STATIC_ASSERT(sizeof(d11actxh_t) == D11AC_TXH_LEN);
	STATIC_ASSERT(sizeof(d11rxhdr_lt80_t) == D11_RXHDR_LEN_REV_LT80);
	STATIC_ASSERT(sizeof(d11rxhdr_ge128_t) == D11_RXHDR_LEN_REV_GE128);
	STATIC_ASSERT(sizeof(d11rxhdr_ge129_t) == D11_RXHDR_LEN_REV_GE129);
	STATIC_ASSERT(sizeof(struct dot11_llc_snap_header) == DOT11_LLC_SNAP_HDR_LEN);
	STATIC_ASSERT(sizeof(struct dot11_header) == DOT11_A4_HDR_LEN);
	STATIC_ASSERT(sizeof(struct dot11_rts_frame) == DOT11_RTS_LEN);
	STATIC_ASSERT(sizeof(struct dot11_cts_frame) == DOT11_CTS_LEN);
	STATIC_ASSERT(sizeof(struct dot11_ack_frame) == DOT11_ACK_LEN);
	STATIC_ASSERT(sizeof(struct dot11_ps_poll_frame) == DOT11_PS_POLL_LEN);
	STATIC_ASSERT(sizeof(struct dot11_cf_end_frame) == DOT11_CS_END_LEN);
	STATIC_ASSERT(sizeof(struct dot11_management_header) == DOT11_MGMT_HDR_LEN);
	STATIC_ASSERT(sizeof(struct dot11_auth) == DOT11_AUTH_FIXED_LEN);
	STATIC_ASSERT(sizeof(struct dot11_bcn_prb) == DOT11_BCN_PRB_LEN);
	STATIC_ASSERT(sizeof(brcm_ie_t) == BRCM_IE_LEN);
	STATIC_ASSERT(sizeof(ht_add_ie_t) == HT_ADD_IE_LEN);
	STATIC_ASSERT(sizeof(ht_cap_ie_t) == HT_CAP_IE_LEN);
	STATIC_ASSERT(OFFSETOF(wl_scan_params_t, channel_list) == WL_SCAN_PARAMS_FIXED_SIZE);
	STATIC_ASSERT(TKIP_MIC_SIZE == (2 * sizeof(uint32)));	/* some tkip code expects this */
	STATIC_ASSERT(OSL_PKTTAG_SZ >= sizeof(wlc_pkttag_t));
	STATIC_ASSERT(BRCM_IE_LEN <= WLC_MAX_BRCM_ELT);

	STATIC_ASSERT(OFFSETOF(wlc_d11rxhdr_t, rxhdr) == WLC_RXHDR_LEN);

	STATIC_ASSERT(ISALIGNED(OFFSETOF(wlc_pub_t, cur_etheraddr), sizeof(uint16)));
	STATIC_ASSERT(ISALIGNED(OFFSETOF(struct scb, ea), sizeof(uint16)));
	STATIC_ASSERT(ISALIGNED(OFFSETOF(wlc_bsscfg_t, cur_etheraddr), sizeof(uint16)));
	STATIC_ASSERT(ISALIGNED(OFFSETOF(wlc_bsscfg_t, BSSID), sizeof(uint16)));

#ifdef STA
	ASSERT(OFFSETOF(wl_assoc_params_t, chanspec_list) -
	       OFFSETOF(wl_assoc_params_t, chanspec_num) ==
	       OFFSETOF(chanspec_list_t, list) - OFFSETOF(chanspec_list_t, num) &&
	       OFFSETOF(wl_assoc_params_t, chanspec_list[1]) -
	       OFFSETOF(wl_assoc_params_t, chanspec_list[0]) ==
	       OFFSETOF(chanspec_list_t, list[1]) - OFFSETOF(chanspec_list_t, list[0]));
#endif // endif
	MALLOC_SET_NOPERSIST(osh);

	sih = wlc_bmac_si_attach((uint)device, osh, regsva, bustype, btparam, &vars, &vars_size);

	if (sih == NULL) {
		WL_ERROR(("%s: si_attach failed\n", __FUNCTION__));
		err = 1;
		goto fail;
	}

	hw = wlc_bmac_phase1_attach(osh, sih, vars, vars_size, &err, unit);
	if (!hw) {
		WL_ERROR(("wl%d: %s: wlc_bmac_preattach failed\n", unit, __FUNCTION__));
		err = 11;
		goto fail;
	}
#ifndef PREATTACH_NORECLAIM
#ifdef DONGLEBUILD
	/* 3 stage reclaim
	 * download the ucode during attach just before allocating wlc_info
	 * & reclaim the ucode fw & its associated functions, so that
	 * attach phase malloc failure can be avoided.
	*/
#ifndef BCMUCDOWNLOAD
	wl_isucodereclaimed(&ucode_reclaimed);
#if defined(BCMULP)
	if (wlc_module_preattach()) {
		WL_ERROR(("wl%d: wlc_attach: wlc_module_preattach failed\n", unit));
		err = 2;
		goto fail;
	}
#endif /* defined(BCMULP) */
	if (!ucode_reclaimed) {
		if (wlc_bmac_process_ucode_sr(hw)) {
			WL_ERROR(("wl%d: wlc_attach: backplane attach failed\n", unit));
			err = 3;
			goto fail;
		}
		wl_reclaim();
	}
#endif /* BCMUCDOWNLOAD */
#endif /* DONGLEBUILD */
#endif /* PREATTACH_NORECLAIM */

	/* populate pre-attach-hardware-cap [now band specific stuff from phy] */
	(void)phy_prephy_phy_caps(hw->prepi, &phy_cap);
	wlc_bmac_phase1_detach(hw);
	MALLOC_CLEAR_NOPERSIST(osh);

	if (vars) {
		char *var;
		if ((var = getvar(vars, rstr_devid))) {
			uint16 devid = (uint16)bcm_strtoul(var, NULL, 0);

			WL_ERROR(("wl%d: %s: Overriding device id: 0x%x instead of 0x%x\n",
				unit, __FUNCTION__, devid, device));
			device = devid;
		}
	}

	/* allocate wlc_info_t state and its substructures */
	if ((wlc = (wlc_info_t*) wlc_attach_malloc(osh, unit, &err, device, objr)) == NULL) {
		WL_ERROR(("wl%d: %s: wlc_attach_malloc failed\n", unit, __FUNCTION__));
		goto fail;
	}
	wlc->osh = osh;
	wlc->state = WLC_STATE_IN_ATTACH;
	wlc->phy_cap = phy_cap;
	pub = wlc->pub;
	/* stash sih, vars and vars_size in pub now */
	pub->sih = sih;
	sih = NULL;
	pub->vars = vars;
	vars = NULL;
	pub->vars_size = vars_size;
	vars_size = 0;

#ifdef BPRESET
	/* enable bpreset by default */
	pub->_bpreset = TRUE;
#endif // endif

#if defined(BCMDBG)
	wlc_info_dbg = wlc;
#endif // endif

	wlc->stf->onechain = -1;
	wlc->core = wlc->corestate;
	wlc->wl = wl;
	pub->unit = unit;
	pub->osh = osh;
	pub->cmn->driverrev = 0;
	pub->cmn->drvrev_major	= EPI_MAJOR_VERSION;
	pub->cmn->drvrev_minor	= EPI_MINOR_VERSION;
	pub->cmn->drvrev_rc	= EPI_RC_NUMBER;
	pub->cmn->drvrev_rc_inc	= EPI_INCREMENTAL_NUMBER;
	wlc->btparam = btparam;
	wlc->brcm_ie = 1;
	wlc->mpc_mode = WLC_MPC_MODE_1; /* default to mpc 1, won't take effect until mpc was set */
#if defined(HEALTH_CHECK)
	pub->cmn->_health_check = wl_health_check_enabled(wl);
#endif /* HEALTH_CHECK */

	pub->_piomode = (iomode == IOMODE_TYPE_PIO);
	wlc->bandinit_pending = FALSE;

#if defined(BCM_DMA_CT) && !defined(BCM_DMA_CT_DISABLED)
		wlc->_dma_ct = (iomode == IOMODE_TYPE_CTDMA);
#endif // endif

	/* Attach dump registry early on */
	if ((wlc->dumpi = wlc_dump_pre_attach(wlc)) == NULL) {
		goto fail;
	}

	/* populate wlc_info_t with default values  */
	wlc_info_init(wlc, unit);

	/* Create the memory pool manager. */
	if (bcm_mpm_init(wlc->osh, wlc->pub->tunables->max_mempools,
	                 &wlc->mem_pool_mgr) != BCME_OK) {
		WL_ERROR(("wl%d: %s: failed to init memory pool manager\n", unit, __FUNCTION__));
		goto fail;
	}

	/* Register dump handler for memory pool manager. */
#if defined(BCMDBG)
	wlc_dump_register(pub, "mempool", wlc_dump_mempool, wlc);
#endif // endif

	/* update sta/ap related parameters */
	wlc_ap_upd(wlc, NULL);

#if defined(WL_OBJ_REGISTRY) && defined(BCMDBG)
	wlc_dump_register(pub, "objreg", (dump_fn_t)wlc_dump_objr, (void *)(wlc->objr));
#endif // endif

#if defined(BCMDBG)
	wlc_dump_register(pub, "bcmlog", (dump_fn_t)wlc_dump_bcmlog, (void *)wlc);
	wlc_dump_register(pub, "wlc", (dump_fn_t)wlc_dump_wlc, (void *)wlc);
	wlc_dump_register(pub, "ratestuff", (dump_fn_t)wlc_dump_ratestuff, (void *)wlc);
	wlc_dump_register(pub, "bssinfo", (dump_fn_t)wlc_bssinfo_dump, (void *)wlc);
#ifdef WL_RX_STALL
	wlc_dump_register(pub, "rx_activity",	(dump_fn_t)wlc_rx_activity_dump, (void*)wlc);
#endif // endif
#ifdef BCMDBG_CTRACE
	wlc_dump_register(pub, "ctrace", (dump_fn_t)wlc_pkt_ctrace_dump, (void *)wlc);
#endif // endif
#if defined(WLVASIP)
	wlc_dump_register(pub, "vasip", (dump_fn_t)wlc_dump_vasip, (void *)wlc);
#endif /* WLVASIP */
	wlc_dump_register(pub, "ratesets", wlc_dump_ratesets, wlc);
#endif // endif

#if defined(BCMDBG) || defined(BCMDBG_PHYDUMP) || defined(WLTEST) || \
	defined(TDLS_TESTBED) || defined(BCMDBG_AMPDU) || defined(MCHAN_MINIDUMP) || \
	defined(BCM_DNGDMP) || defined(BCMDBG_PHYDUMP)
#ifdef WLTINYDUMP
	wlc_dump_register(pub, "tiny", (dump_fn_t)wlc_tinydump, (void *)wlc);
#endif // endif
#endif // endif

#ifdef CHAN_SWITCH_HIST
	wlc_dump_register(pub, "chanswitch", (dump_fn_t)wlc_dump_chanswitch, (void *)wlc);
#endif // endif

#if defined(BCMTSTAMPEDLOGS)
	wlc_dump_register(pub, "timestamps", (dump_fn_t)wlc_bcmdumptslog, (void *)wlc);
#endif // endif

#if defined(BCMDBG_TXSTUCK)
	wlc_dump_register(pub, "txstuck", (dump_fn_t)wlc_dump_txstuck, (void *)wlc);
#endif /* defined(BCMDBG_TXSTUCK) */

#if defined(WL_NAP) && !defined(WL_NAP_DISABLED)
	wlc->pub->_nap = TRUE;
#else
	wlc->pub->_nap = FALSE;
#endif /* WL_NAP */

	macunit = wlc_get_wlcindex(wlc);
	ASSERT(macunit >= 0);

	/* low level attach steps(all hw accesses go inside, no more in rest of the attach) */
	err = wlc_bmac_attach(wlc, vendor, device, unit, (iomode == IOMODE_TYPE_PIO), osh,
			regsva, bustype, btparam, (uint)macunit);
	if (err != 0) {
		WL_ERROR(("wl%d: %s: wlc_bmac_attach failed\n", unit, __FUNCTION__));
		goto fail;
	}

	wlc->fabid = si_fabid(wlc->hw->sih);

	wlc_pi_band_update(wlc, IS_SINGLEBAND_5G(wlc->deviceid, wlc->phy_cap) ?
		BAND_5G_INDEX : BAND_2G_INDEX);

	/* PULL BMAC states
	 * for some states, due to different info pointer(e,g, wlc, wlc_hw) or master/slave split,
	 * HIGH driver(both monolithic and HIGH_ONLY) needs to sync states FROM BMAC portion driver
	 */
	if (wlc_bmac_state_get(wlc->hw, &state_bmac) != 0) {
		WL_ERROR(("wl%d: %s: wlc_bmac_state_get failed\n", unit, __FUNCTION__));
		err = 20;
		goto fail;
	}
	wlc->machwcap = state_bmac.machwcap;
	wlc->machwcap1 = state_bmac.machwcap1;

	for (j = 0; j < NFIFO_LEGACY; j++) {
		uint fifo_size;
		wlc_xmtfifo_sz_get(wlc, j, &fifo_size);
		wlc_xmtfifo_sz_upd_high(wlc, j, (uint16)fifo_size);
	}

	if (D11REV_GE(wlc->pub->corerev, 42))
		pub->m_coremask_blk_wowl = D11AC_M_COREMASK_BLK_WOWL;
	else
		pub->m_coremask_blk_wowl = M_COREMASK_BLK_WOWL;

	if (D11REV_GE(wlc->pub->corerev, 40)) {
		/* For 4369a0, Phy hdr length is same as AC */
		pub->bcn_tmpl_len = D11AC_BCN_TMPL_LEN;
		pub->d11tpl_phy_hdr_len = D11AC_PHY_HDR_LEN;
		pub->max_addrma_idx = AMT_SIZE(wlc->pub->corerev);
		wlc_mfbr_retrylimit_upd(wlc);
	} else {
		pub->bcn_tmpl_len = BCN_TMPL_LEN;
		pub->d11tpl_phy_hdr_len = D11_PHY_HDR_LEN;
		pub->max_addrma_idx = RCMTA_SIZE;
	}
	pub->m_coremask_blk = M_COREMASK_BLK(wlc);

	wlc->tcpack_fast_tx = FALSE;

	pub->phy_11ncapable = WLC_PHY_11N_CAP(wlc->band);
	pub->phy_bw40_capable = WLC_40MHZ_CAP_PHY(wlc);
	pub->phy_bw80_capable = WLC_80MHZ_CAP_PHY(wlc);
	pub->phy_bw8080_capable = WLC_8080MHZ_CAP_PHY(wlc);
	pub->phy_bw160_capable = WLC_160MHZ_CAP_PHY(wlc);

#ifndef DYN160_DISABLED
		pub->_dyn160 = WL_HAS_DYN160(wlc);
#endif /* DYN160_DISABLED */

	/* 11n_disable nvram */
	n_disabled = getintvar(pub->vars, rstr_11n_disable);

	if ((pub->sih->boardvendor == VENDOR_APPLE) &&
	           ((pub->sih->chip == BCM4352_CHIP_ID) ||
	            (pub->sih->chip == BCM4360_CHIP_ID) ||
	            (pub->sih->chip == BCM43602_CHIP_ID) ||
	            (pub->sih->chip == BCM4350_CHIP_ID))) {
		wlc->stf->tx_duty_cycle_thermal = WLC_DUTY_CYCLE_THERMAL_50;
		/* duty cycle throttling disabled for pwr throttling */
		wlc->stf->tx_duty_cycle_pwr = NO_DUTY_THROTTLE;
		wlc->stf->pwr_throttle = AUTO;
	}

	/* set maximum allowed duty cycle */
	wlc->stf->tx_duty_cycle_max = NO_DUTY_THROTTLE;
	wlc->stf->tx_duty_cycle_ofdm = (uint16)getintvar(pub->vars, rstr_tx_duty_cycle_ofdm);
	if (wlc->stf->tx_duty_cycle_ofdm == 0)
		wlc->stf->tx_duty_cycle_ofdm = NO_DUTY_THROTTLE;
	wlc->stf->tx_duty_cycle_cck = (uint16)getintvar(pub->vars, rstr_tx_duty_cycle_cck);
	if (wlc->stf->tx_duty_cycle_cck == 0)
		wlc->stf->tx_duty_cycle_cck = NO_DUTY_THROTTLE;
#ifdef WL11AC
	wlc->stf->tx_duty_cycle_ofdm_40_5g = (uint16)getintvar(pub->vars,
		rstr_tx_duty_cycle_ofdm_40_5g);
	wlc->stf->tx_duty_cycle_thresh_40_5g = (uint16)getintvar(pub->vars,
		rstr_tx_duty_cycle_thresh_40_5g);
	wlc->stf->tx_duty_cycle_ofdm_80_5g = (uint16)getintvar(pub->vars,
		rstr_tx_duty_cycle_ofdm_80_5g);
	wlc->stf->tx_duty_cycle_thresh_80_5g = (uint16)getintvar(pub->vars,
		rstr_tx_duty_cycle_thresh_80_5g);
	/* Make sure tx duty cycle is set to smaller value when tempsense throttle is active */
	if ((wlc->stf->tx_duty_cycle_ofdm_40_5g &&
		wlc->stf->tx_duty_cycle_pwr > wlc->stf->tx_duty_cycle_ofdm_40_5g) ||
		(wlc->stf->tx_duty_cycle_ofdm_80_5g &&
		wlc->stf->tx_duty_cycle_pwr > wlc->stf->tx_duty_cycle_ofdm_80_5g)) {
		WL_ERROR(("wl%d: %s: tx_duty_cycle_ofdm (40 %d, 80 %d) must be larger than"
			" tx_duty_cycle_pwr %d.\n", unit, __FUNCTION__,
			wlc->stf->tx_duty_cycle_ofdm_40_5g, wlc->stf->tx_duty_cycle_ofdm_80_5g,
			wlc->stf->tx_duty_cycle_pwr));
		err = 23;
		goto fail;
	}
#endif /* WL11AC */

	wlc_stf_phy_chain_calc(wlc);

	/* txchain 1: txant 0, txchain 2: txant 1 */
	if (WLCISNPHY(wlc->band) && (wlc->stf->op_txstreams == 1))
		wlc->stf->txant = wlc->stf->hw_txchain - 1;

	if (D11REV_GE(wlc->pub->corerev, 40)) {
		wlc->toe_capable = TRUE;
		wlc->toe_bypass = FALSE;
	}

	/* push to BMAC driver */
	wlc_stf_chain_init(wlc);

	/* pull up some info resulting from the low attach */
	wlc->core->txavail = wlc->hw->txavail;

	wlc->stf->ipaon = phy_tpc_ipa_ison(WLC_PI(wlc));

	wlc_bmac_hw_etheraddr(wlc->hw, &wlc->perm_etheraddr);

	bcopy((char *)&wlc->perm_etheraddr, (char *)&pub->cur_etheraddr, ETHER_ADDR_LEN);
#ifdef WLOLPC
	pub->_olpc = FALSE; /* try to enable in loop below */
#endif /* WLOLPC */

	for (j = 0; j < NBANDS(wlc); j++) {
		/* Use band 1 for single band 11a */
		if (IS_SINGLEBAND_5G(wlc->deviceid, wlc->phy_cap))
			j = BAND_5G_INDEX;

		wlc_pi_band_update(wlc, j);

		if (!wlc_attach_stf_ant_init(wlc)) {
			WL_ERROR(("wl%d: %s: wlc_attach_stf_ant_init failed\n",
			          unit, __FUNCTION__));
			err = 24;
			goto fail;
		}

		/* default contention windows size limits */
		wlc->band->CWmin = APHY_CWMIN;
		wlc->band->CWmax = PHY_CWMAX;

		/* set default roam parameters */
		wlc_roam_defaults(wlc, wlc->band, &wlc->band->roam_trigger_def,
			&wlc->band->roam_delta_def);
		wlc->band->roam_trigger_init_def = wlc->band->roam_trigger_def;
		wlc->band->roam_trigger = wlc->band->roam_trigger_def;
		wlc->band->roam_delta = wlc->band->roam_delta_def;

		/* init gmode value */
		if (BAND_2G(wlc->band->bandtype)) {
			wlc->band->gmode = GMODE_AUTO;
		}

		/* init _n_enab supported mode */
		if (WLC_PHY_11N_CAP(wlc->band)) {
			if (n_disabled & WLFEATURE_DISABLE_11N) {
				pub->_n_enab = OFF;
			} else {
				pub->_n_enab = SUPPORT_11N;
			}
		}

		if (WLC_PHY_VHT_CAP(wlc->band)) {
			if (CHIP_SUPPORTS_11AC(pub->sih)) {
				/* Setup default VHT feature set for device */
				WLC_VHT_FEATURES_DEFAULT(pub, wlc->bandstate, wlc->nbands);
#if defined(WL11AC)
/* for EAP builds do not add additional default VHT features beyond the defaults */
				if (wlc->phy_cap & PHY_PREATTACH_CAP_SUP_2G) {
					WLC_VHT_FEATURES_SET(pub, WL_VHT_FEATURES_2G);
				}
				if (WLC_VHT_PROP_RATES_CAP_PHY(wlc)) {
					/* (Default) enable all 5G VHT MCS rates 0-9 for all
					*   bandwidths, that makes bit1 of the "wl vht_features"
					*   to be 1
					*/
					WLC_VHT_FEATURES_SET(pub, WL_VHT_FEATURES_STD_RATES_5G);
					/* enable VHT MCS rates 8-9 for 20MHz on 2G core */
					WLC_VHT_FEATURES_SET(pub, WL_VHT_FEATURES_STD_RATES_2G);
				}

				if (WLC_1024QAM_CAP_PHY(wlc)) {
					WLC_VHT_FEATURES_SET(pub, WL_VHT_FEATURES_1024QAM);
					WLC_VHT_FEATURES_PROP_MCS_SET(pub, VHT_PROP_MCS_MAP_10_11);
				}
#endif // endif
				if (wlc->pub->_n_enab == OFF) {
					/* If _n_enab is off, _vht_enab should also be off */
					wlc->pub->_vht_enab = 0;
				} else {
					wlc->pub->_vht_enab = 1;
				}
			}
#ifdef WLOLPC
			/* open loop pwr cal only available on AC phys */
			pub->_olpc = TRUE;
#endif /* WLOLPC */
		}  else {
			wlc->pub->_vht_enab = 0;
#ifdef WL11AC
			wlc->pub->vht_features = 0;
#endif // endif
		};

		/* init per-band default rateset, depend on band->gmode */
		wlc_default_rateset(wlc, &wlc->band->defrateset);

		/* fill in hw_rateset (used early by WLC_SET_RATESET) */
		wlc_rateset_filter(&wlc->band->defrateset /* src */, &wlc->band->hw_rateset, FALSE,
		                   WLC_RATES_CCK_OFDM, RATE_MASK, wlc_get_mcsallow(wlc, NULL));
	}

#if defined(WL_STF_ARBITRATOR) && !defined(WL_STF_ARBITRATOR_DISABLED)
	wlc->pub->_stf_arb = TRUE;
#endif /* WL_STF_ARBITRATOR && NOT WL_STF_ARBITRATOR_DISABLED */

#if defined(WL_MIMOPS) && !defined(WL_MIMOPS_DISABLED)
	wlc->pub->_stf_mimops = TRUE;
#endif /* WL_MIMOPS && NOT WL_MIMOPS_DISABLED */

	/* update antenna config due to wlc->stf->txant/txchain/ant_rx_ovr change */
	wlc_stf_phy_txant_upd(wlc);

	/* attach infra. modules that must be done first */
	err = wlc_attach_module_pre(wlc);
	if (err != 0) {
		WL_ERROR(("wl%d: %s: wlc_attach_module_pre failed\n", unit, __FUNCTION__));
		goto fail;
	}

	/* attach regular modules */
	err = wlc_attach_module(wlc);
	if (err != 0) {
		WL_ERROR(("wl%d: %s: wlc_attach_module failed\n", unit, __FUNCTION__));
		goto fail;
	}

	wlc_prot_n_cfg_set(wlc->prot_n, WLC_PROT_N_PAM_OVR, (int8)state_bmac.preamble_ovr);
	/* TODO: VSDB/P2P/MCNX bmac update ? */

#ifndef ATE_BUILD
	/* Timers are not required for the ATE firmware */
	if (!wlc_timers_init(wlc, unit)) {
		WL_ERROR(("wl%d: %s: wlc_init_timer failed\n", unit, __FUNCTION__));
		err = 1000;
		goto fail;
	}
#endif /* !ATE_BUILD */

	/* depend on rateset, gmode */
	wlc->cmi = wlc_channel_mgr_attach(wlc);
	if (!wlc->cmi) {
		WL_ERROR(("wl%d: %s: wlc_channel_mgr_attach failed\n", unit, __FUNCTION__));
		err = 1001;
		goto fail;
	}

#if defined(OCL) && !defined(OCL_DISABLED)
	wlc->pub->_ocl = TRUE;
#endif /* OCL && NOT OCL_DISABLED */

	/* init default when all parameters are ready, i.e. ->rateset */
	wlc_bss_default_init(wlc);

	/*
	 * Complete the wlc default state initializations..
	 */

	/* allocate our initial queue */
	/* Ensure allocations are non-persist */
	MALLOC_SET_NOPERSIST(wlc->osh);
	/* this txq is never freed! */
	qi = wlc_txq_alloc(wlc, osh);
	if (qi == NULL) {
		WL_ERROR(("wl%d: %s: failed to malloc tx queue\n", unit, __FUNCTION__));
		err = 1003;
		goto fail;
	}
	wlc->active_queue = qi;
	wlc->def_primary_queue = qi;
	wlc->primary_queue = qi;

	/* allocate the excursion queue */
	qi = wlc_txq_alloc(wlc, osh);
	if (qi == NULL) {
		WL_ERROR(("wl%d: %s: failed to malloc excursion queue\n", unit, __FUNCTION__));
		err = 1004;
		goto fail;
	}
	wlc->excursion_queue = qi;

	/* Clear NOPERSIST for further allocations */
	MALLOC_CLEAR_NOPERSIST(wlc->osh);

	/* allocate/init the primary bsscfg */
	if (wlc_bsscfg_primary_init(wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s: init primary bsscfg failed\n", unit, __FUNCTION__));
		err = 1005;
		goto fail;
	}

	wlc_set_default_txmaxpkts(wlc);

	WLCNTSET(pub->_rxfifo_cnt->version, WL_RXFIFO_CNT_VERSION);
	WLCNTSET(pub->_rxfifo_cnt->length, sizeof(wl_rxfifo_cnt_t));

#ifdef WLAMSDU
	if (AMSDU_TX_SUPPORT(wlc->pub)) {
		/* set AMSDU agg based on core rev */
		if (D11REV_GE(wlc->pub->corerev, 40)) {
			/* enable AMSDU for core rev 40+ to allow large aggs */
			wlc->pub->_amsdu_tx = TRUE;
		}
#if defined(WLRSDB) && defined(AMSDU_OFF_IN_RSDB)
		/* Define AMSDU_OFF_IN_RSDB for chips which dont have enough buffers */
		/* Disable AMSDU in Dongle, since rxpost buffers are lesser for RSDB */
		if ((RSDB_ENAB(wlc->pub)) && (WLC_RSDB_CURR_MODE(wlc) & WLC_RSDB_MODE_RSDB) &&
			!WLC_DUALMAC_RSDB(wlc->cmn)) {
			wlc->pub->_amsdu_tx = FALSE;
		}
#endif /* WLRSDB && AMSDU_OFF_IN_RSDB */
	}

	/* check hw AMSDU deagg capability */
	if (wlc_amsdurx_cap(wlc->ami))
		wlc->_amsdu_rx = TRUE;
	/* enable A-MSDU in A-MPDU if we have the HW (corerev 40+)
	* and we have ample rx buffers and amsdu tx is ON.
	*/
	if (D11REV_GE(wlc->pub->corerev, 40) &&
		(wlc->pub->tunables->nrxbufpost >=
		wlc->pub->tunables->amsdu_rxpost_threshold) &&
		wlc->pub->_amsdu_tx) {
		wlc->_rx_amsdu_in_ampdu = TRUE;
	}
#endif	/* WLAMSDU */
	wlc_ht_init_defaults(wlc->hti);
	wlc_vht_init_defaults(wlc->vhti);
#ifdef WL11AX
	if (WLC_HE_CAP_PHY(wlc)) {
		if (wlc_he_init_defaults(wlc->hei) != BCME_OK) {
			goto fail;
		}
	}
#endif // endif
	wlc_ht_nvm_overrides(wlc->hti, n_disabled);

	/* allocate the perm scbs after everybody has reserved their cubby
	 *  can this be merged with early loop, where gmode/nmode has to before all module attach ?
	 */
	if (wlc_hwrsscbs_alloc(wlc) != BCME_OK) {
		err = 1100;
		goto fail;
	}

	if ((wlc->ebd = MALLOCZ(wlc->osh,
		sizeof(wlc_early_bcn_detect_t))) == NULL) {
		WL_ERROR(("wl%d: wlc_early_bcn_detect_t alloc failed\n", unit));
		err = 1101;
		goto fail;
	}
#ifdef WL_EXCESS_PMWAKE
	if ((wlc->excess_pmwake = MALLOCZ(wlc->osh,
		sizeof(wlc_excess_pm_wake_info_t))) == NULL) {
		WL_ERROR(("wl%d: wlc_excess_pm_wake_info_t alloc failed\n", unit));
		err = 1102;
		goto fail;
	}
#endif /* WL_EXCESS_PMWAKE */

#ifdef BCMUSBDEV_ENABLED
	/* USB does not have native per-AC flow control */
	pub->wlfeatureflag |= WL_SWFL_FLOWCONTROL;
#endif // endif

#ifdef DONGLEBUILD
	/* Disable HW radio monitor if feature is not supported */
	if (!(wlc->pub->boardflags & BFL_AIRLINEMODE)) {
		pub->wlfeatureflag |= WL_SWFL_NOHWRADIO;
	}
#endif /* DONGLEBUILD */

	/* fixup mpc */
	wlc->mpc = wlc_mpccap(wlc);
	if (wlc->mpc)
		wlc->mpc_mode = WLC_MPC_MODE_1;
	else
		wlc->mpc_mode = WLC_MPC_MODE_OFF;

#ifdef STA
	/* initialize radio_mpc_disable according to wlc->mpc */
	wlc_radio_mpc_upd(wlc);

	/* XXX Ubuntu 12.10 & 13.04 won't up wlan interface after reboot
	 * if RF switch key is disabled. In this case the wlc_radio_monitor_start()
	 * in wlc_up() won't be called. Therefore, we need to call wlc_radio_monitor_start()
	 * here to monitor RF switch key and track when it's turned on.
	 */
	wlc_radio_hwdisable_upd(wlc);
	if (wlc->pub->radio_disabled & WL_RADIO_HW_DISABLE) {
		wlc_radio_monitor_start(wlc);
	}
#endif /* STA */

	if (WLANTSEL_ENAB(wlc)) {
		wlc_bmac_antsel_set(wlc->hw, wlc->asi->antsel_avail);
	}

#ifdef WLATF
	/* Default for airtime fairness is in wlc_cfg.h */
	wlc->atf = WLC_ATF_ENABLE_DEFAULT;
#endif // endif

	if (wlc_attach_cb_init(wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_attach_cb_init failed\n", unit, __FUNCTION__));
		err = 2000;
		goto fail;
	}

#if defined(EVENT_LOG_COMPILE) && defined(WLCNT)
	/* In RSDB case register on both slices */
	if (wlc_chansw_notif_register(wlc, wlc_ctl_mgt_frame_counter_report, wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_chansw_notif_register failed\n", unit, __FUNCTION__));
		err = 2100;
		goto fail;
	}
#endif /* EVENT_LOG_COMPILE & WLCNT */

#if defined(DONGLEBUILD)
	for (j = 0; j < AC_COUNT; j++)
		wlc->lifetime[j] = WLC_DEFAULT_MAX_DATA_PKT_LIFETIME * 1000; /* msec -> us */

	wlc->cmn->lifetime_mg->lifetime = WLC_DEFAULT_MAX_MGMT_PKT_LIFETIME * 1000; /* ms ->us */
	wlc->cmn->lifetime_mg->mgmt_bitmap = MGMT_ALL;
	wlc->lifetime_ctrl = WLC_DEFAULT_CTRL_PKT_LIFETIME * 1000; /* ms ->us */
#endif /* DONGLEBUILD */

#ifdef WLRSDB
	/* set band locks in WLTEST */
	wlc_rsdb_do_band_lock(wlc, TRUE);
#endif /* WLRSDB */

	nvramrev = getvar(wlc->pub->vars, rstr_NVRAMRev);
	if (nvramrev)
	{
		while (*nvramrev && !bcm_isdigit(*nvramrev)) nvramrev++;
		wlc->nvramrev = bcm_strtoul(nvramrev, NULL, 10);
	}
	WL_MPC(("wl%d: ATTACHED\n", unit));

	wlc->cleanup_unused_scbs = TRUE;
	/* Begin with LTR_SLEEP state */
	wl_indicate_maccore_state(wlc->wl, LTR_SLEEP);
	wlc->cmn->hostmem_access_enabled = TRUE;

	if (perr)
		*perr = 0;

#ifdef AUXCORE_PM_WAR
	if (wlc->pub->unit == 1)
		wlc_user_wake_upd(wlc, WLC_USER_WAKE_REQ_NIC, TRUE);
#endif /* AUXCORE_PM_WAR */

	/* EVENT_LOG_COMPILE flag present below will be removed after
	 * MACOS completes porting of ecounters. Without this flag,
	 * precommit fails in NIC and NIC off load builds.
	 */
#if defined(WLCNT) && defined(ECOUNTERS) && defined(EVENT_LOG_COMPILE)
	/* only register BTCX econters for AUX core */
	if (ECOUNTERS_ENAB() &&
		((RSDB_ENAB(wlc->pub) && unit == MAC_CORE_UNIT_0) ||
		!RSDB_ENAB(wlc->pub))) {
		wl_ecounters_register_source(WL_IFSTATS_XTLV_WL_SLICE_V30_WLCNTRS,
			wlc_ecounters_wl_counters, (void*)wlc);
	}
#endif // endif

#if defined(HEALTH_CHECK) && defined(WL_RX_STALL) && !defined(HEALTH_CHECK_DISABLED)
	if (WL_HEALTH_CHECK_ENAB(wlc->pub->cmn)) {
		if (!wl_health_check_module_register(wlc->wl, "wl_rx_stall_check",
			wlc_rx_healthcheck, wlc, WL_HC_DD_RX_STALL)) {
			goto fail;
		}
	}
#endif // endif

#ifdef RX_DEBUG_ASSERTS
	/* RXS backup Storage */
	wlc->rxs_bkp = MALLOCZ(wlc->osh, MAX_RXS_BKP_ENTRIES * PER_RXS_SIZE);
	if (wlc->rxs_bkp == NULL) {
		WL_ERROR(("%s : RXS backup storage failed \n", __FUNCTION__));
		goto fail;
	}
#endif /* RX_DEBUG_ASSERTS */

	wlc->state = WLC_STATE_DOWN;

	return ((void*)wlc);

fail:
	WL_ERROR(("wl%d: %s: failed with err %d\n", unit, __FUNCTION__, err));
	if (sih) {
		wlc_bmac_si_detach(osh, sih);
		sih = NULL;
	}
	if (vars) {
		MFREE(osh, vars, vars_size);
		vars = NULL;
		vars_size = 0;
	}
	if (wlc) {
		MODULE_DETACH(wlc, wlc_detach);
	}

	if (perr)
		*perr = err;
	return (NULL);
} /* wlc_attach */

#ifdef WL_ANTGAIN

#define NUM_ANT      3
#define MAXNUM_ANT_ENTRY	(sizeof(ant_default)/sizeof(ant_default[0]))
#define WLC_TXMINGAIN 0

typedef struct {
	uint8	band2g [WLC_TXCORE_MAX];
	uint8	band5g [WLC_TXCORE_MAX];
} ant_gain_limit;

const struct {
	uint16	boardtype;
	ant_gain_limit antgain;
} ant_default [] = {
	{BCM94360X51P2,  {{0xc5, 0x83, 6, WLC_TXMINGAIN}, /* 2g ant gain 5.75 3.5 6 */
	{0xc7, 0xc7, 7, WLC_TXMINGAIN}}}, /* 5g ant gain 7.75 7.75 7 */
	{BCM94360X51P3,  {{6, 0x85, 0x83, WLC_TXMINGAIN}, /* 2g ant gain 6 5.5 3.5 */
	{0x83, 0x83, 5, WLC_TXMINGAIN}}}, /* 5g ant gain 3.5 3.5 5 */
	{BCM94360X29CP3,  {{0xc5, 0xc5, 0xc5, WLC_TXMINGAIN}, /* 2g ant gain 5.75 5.75 5.75 */
	{0xc6, 0xc6, 0xc6, WLC_TXMINGAIN}}}, /* 5g ant gain 6.75 6.75 6.75 */
	{BCM943602X87P3,  {{0xc5, 0xc5, 0xc5, WLC_TXMINGAIN}, /* 2g ant gain 5.75 5.75 5.75 */
	{0xc6, 0xc6, 0xc6, WLC_TXMINGAIN}}}, /* 5g ant gain 6.75 6.75 6.75 */
};

#endif /* WL_ANTGAIN */

static int8
wlc_antgain_calc(int8 ag)
{
	int8 gain, fract;
	/* Older sroms specified gain in whole dbm only.  In order
	 * be able to specify qdbm granularity and remain backward compatible
	 * the whole dbms are now encoded in only low 6 bits and remaining qdbms
	 * are encoded in the hi 2 bits. 6 bit signed number ranges from
	 * -32 - 31. Examples: 0x1 = 1 db,
	 * 0xc1 = 1.75 db (1 + 3 quarters),
	 * 0x3f = -1 (-1 + 0 quarters),
	 * 0x7f = -.75 (-1 in low 6 bits + 1 quarters in hi 2 bits) = -3 qdbm.
	 * 0xbf = -.50 (-1 in low 6 bits + 2 quarters in hi 2 bits) = -2 qdbm.
	 */
	gain = ag & 0x3f;
	gain <<= 2;	/* Sign extend */
	gain >>= 2;
	fract = (ag & 0xc0) >> 6;
	return (4 * gain + fract);
}

static void
BCMATTACHFN(wlc_attach_antgain_init)(wlc_info_t *wlc)
{
	int8 antgain = wlc->band->antgain;

	if ((antgain == -1) && (wlc->pub->sromrev == 1)) {
		/* default antenna gain for srom rev 1 is 2 dBm (8 qdbm) */
		antgain = 8;
	} else if (antgain == -1) {
		WL_ERROR(("wl%d: %s: Invalid antennas available in srom, using 2dB\n",
		          wlc->pub->unit, __FUNCTION__));
		antgain = 8;
	} else if (wlc->pub->sromrev < 11) {
		antgain = wlc_antgain_calc(antgain);
	} else if (wlc->pub->sromrev >= 11) {
		char *vars = wlc->pub->vars;
		int bandtype = wlc->band->bandtype;
		int8 ag[3], max_ag = 0;
		uint i;

		ag[0] = (int8)getintvar_slicespecific(wlc->hw,
				vars, (BAND_5G(bandtype) ? rstr_aga0 : rstr_agbg0));
		ag[1] = (int8)getintvar_slicespecific(wlc->hw,
				vars, (BAND_5G(bandtype) ? rstr_aga1 : rstr_agbg1));
		ag[2] = (int8)getintvar_slicespecific(wlc->hw, vars,
				(BAND_5G(bandtype) ? rstr_aga2 : rstr_agbg2));
#ifdef WL_ANTGAIN
		/* XXX check the board type with the pre defined table boardtype
		* same module with different Subsystem ID SROM Cannot be used
		*/
		for (i = 0; i < MAXNUM_ANT_ENTRY; i++) {
			if (ant_default[i].boardtype == wlc->pub->sih->boardtype) {
				uint j;
				for (j = 0; j < NUM_ANT; j++) {
				ag[j] = BAND_5G(bandtype) ?
					ant_default[i].antgain.band5g[j]:
					ant_default[i].antgain.band2g[j];
				}
			break;
			}
		}
#endif // endif
		for (i = 0; i < 3; i++) {
			ag[i] = wlc_antgain_calc(ag[i]);
			max_ag = MAX(ag[i], max_ag);
		}

		antgain = max_ag;
		WL_INFORM(("wl%d: %s: SROM %s antenna gain is %d\n", wlc->pub->unit, __FUNCTION__,
			BAND_5G(bandtype) ? "5G" : "2G", antgain));
	}
	wlc->band->antgain = antgain;
} /* wlc_attach_antgain_init */

static void
BCMATTACHFN(wlc_attach_sar_init)(wlc_info_t *wlc)
{
	wlc->band->sar = WLC_TXPWR_MAX;

	return;
} /* wlc_attach_sar_init */

static bool
BCMATTACHFN(wlc_attach_stf_ant_init)(wlc_info_t *wlc)
{
	int aa;
	char *vars;
	int bandtype;

	vars = wlc->pub->vars;
	bandtype = wlc->band->bandtype;

	/* get antennas available */
	aa = (int8)getintvar_slicespecific(wlc->hw,
			vars, (BAND_5G(bandtype) ? rstr_aa5g : rstr_aa2g));
	if (aa == 0)
		aa = (int8)getintvar_slicespecific(wlc->hw,
			vars, (BAND_5G(bandtype) ? rstr_aa1 : rstr_aa0));
	if ((aa < 1) || (aa > 15)) {
		WL_ERROR(("wl%d: %s: Invalid antennas available in srom (0x%x), using 3.\n",
			wlc->pub->unit, __FUNCTION__, aa));
		aa = 3;
	}

	/* reset the defaults if we have a single antenna */
	if (aa == 1) {
		WL_INFORM(("wl%d: %s: Single antenna available (aa = %d) forcing 0(main)\n",
			wlc->pub->unit, __FUNCTION__, aa));
		wlc->stf->ant_rx_ovr = ANT_RX_DIV_FORCE_0;
		wlc->stf->txant = ANT_TX_FORCE_0;
	} else if (aa == 2) {
		WL_INFORM(("wl%d: %s: Single antenna available (aa = %d) forcing 1(aux)\n",
			wlc->pub->unit, __FUNCTION__, aa));
		wlc->stf->ant_rx_ovr = ANT_RX_DIV_FORCE_1;
		wlc->stf->txant = ANT_TX_FORCE_1;
	}

	/* Compute Antenna Gain */
	wlc->band->antgain =
		(int8)getintvar_slicespecific(wlc->hw, vars,
		(BAND_5G(bandtype) ? rstr_ag1 : rstr_ag0));
	wlc_attach_antgain_init(wlc);

	wlc->band->sar =
			(int8)getintvar_slicespecific(wlc->hw, vars,
			(BAND_5G(bandtype) ? rstr_sar5g : rstr_sar2g));
	wlc_attach_sar_init(wlc);
	wlc->band->sar_cached = wlc->band->sar;

#define AA2DBI		(2 * WLC_TXPWR_DB_FACTOR)
	if (wlc->pub->sih->boardvendor == VENDOR_APPLE) {
		/* XXX This fixup board that have antenna gain
		 * programmed as 2dBi, fix it up.
		 */
		if (wlc->band->antgain > AA2DBI)
			goto exit;

		if (wlc->pub->sih->boardtype == BCM94322X9) {
			/* antenna gain for X9 is 6.75dBi (5G), 3.25dBi (2.4G) */
			wlc->band->antgain = BAND_5G(wlc->band->bandtype) ?
				((6 * WLC_TXPWR_DB_FACTOR) + 3) : ((3 * WLC_TXPWR_DB_FACTOR) + 1);
		} else if (wlc->pub->sih->boardtype == BCM94322M35e) {
			/* antenna gain for M35e is 4.75dBi (5G), 4.0dBi (2.4G) */
			wlc->band->antgain = BAND_5G(wlc->band->bandtype) ?
				((4 * WLC_TXPWR_DB_FACTOR) + 3) : (4 * WLC_TXPWR_DB_FACTOR);
		}
	}
exit:
	return TRUE;
} /* wlc_attach_stf_ant_init */

static void
BCMATTACHFN(wlc_timers_deinit)(wlc_info_t *wlc)
{
	/* free timer state */
	if (wlc->wdtimer) {
		wl_free_timer(wlc->wl, wlc->wdtimer);
		wlc->wdtimer = NULL;
	}

#ifdef STA
	if (wlc->wdtimer_maccore_state) {
		wl_free_timer(wlc->wl, wlc->wdtimer_maccore_state);
		wlc->wdtimer_maccore_state = NULL;
	}
#endif /* STA */

	if (wlc->radio_timer) {
		wl_free_timer(wlc->wl, wlc->radio_timer);
		wlc->radio_timer = NULL;
	}

#ifdef STA
	if (wlc->pm2_radio_shutoff_dly_timer) {
		wl_free_timer(wlc->wl, wlc->pm2_radio_shutoff_dly_timer);
		wlc->pm2_radio_shutoff_dly_timer = NULL;
	}

	if (wlc->wake_event_timer) {
		wl_free_timer(wlc->wl, wlc->wake_event_timer);
		wlc->wake_event_timer = NULL;
	}
#ifdef WLRXOV
	if (WLRXOV_ENAB(wlc->pub) && wlc->rxov_timer) {
		wl_free_timer(wlc->wl, wlc->rxov_timer);
		wlc->rxov_timer = NULL;
	}
#endif // endif
#endif /* STA */
} /* wlc_timers_deinit */

/** detach modules - add new modules at the beginning of this function */
static void
BCMATTACHFN(wlc_detach_module)(wlc_info_t *wlc)
{
	MODULE_DETACH(wlc->fifo, wlc_fifo_detach);
	MODULE_DETACH(wlc->txcfg, wlc_txcfg_detach);

	MODULE_DETACH(wlc->macreq, wlc_macreq_detach);
#ifdef WLOLPC
	MODULE_DETACH(wlc->olpc_info, wlc_olpc_eng_detach);
#endif /* OPEN LOOP POWER CAL */

#ifdef WLSQS
	/* detach SQS Module */
	MODULE_DETACH(wlc->sqs, wlc_sqs_detach);
#endif // endif

#if defined(WL_AIR_IQ)
	if (AIRIQ_ENAB()) {
		MODULE_DETACH(wlc->airiq, wlc_airiq_detach);
	}
#endif /* WL_AIR_IQ */

#ifndef ATE_BUILD
#if defined(WL_MUSCHEDULER) && !defined(WL_MUSCHEDULER_DISABLED)
	MODULE_DETACH(wlc->musched, wlc_muscheduler_detach);
#endif /* WL_MUSCHEDULER && !WL_MUSCHEDULER_DISABLED */
#endif /* !ATE_BUILD */

#if defined(WL_LEAKY_AP_STATS) && !defined(WL_LEAKYAPSTATS_DISABLED)
	MODULE_DETACH(leakyapstats_info, wlc_leakyapstats_detach);
#endif /* WL_LEAKY_AP_STATS && !WL_LEAKYAPSTATS_DISABLED */

#if defined(WL_OCE) && !defined(WL_OCE_DISABLED)
	/* Detach OCE module */
	MODULE_DETACH(wlc->oce, wlc_oce_detach);
#endif /* WL_OCE && !WL_OCE_DISABLED */

#if defined(WL_FILS) && !defined(WL_FILS_DISABLED)
	MODULE_DETACH(wlc->fils, wlc_fils_detach);
#endif /* WL_FILS && !WL_FILS_DISABLED */

#ifdef WL_RX_STALL
	/* Detach RX stall health check module */
	MODULE_DETACH(wlc->rx_hc, wlc_rx_hc_detach);
#endif // endif

#if defined(WL_MBO) && defined(MBO_AP)
	MODULE_DETACH(wlc->mbo, wlc_mbo_ap_detach);
#endif /* WL_MBO && MBO_AP */

#if defined(WL_MBO_OCE) && !defined(WL_MBO_OCE_DISABLED)
	MODULE_DETACH(wlc->mbo_oce, wlc_mbo_oce_detach);
#endif /* WL_MBO_OCE && !WL_MBO_OCE_DISABLED */

#ifdef WLC_TXCAL
	MODULE_DETACH(wlc->cldi, wlc_calload_detach);
#endif /* WLC_TXCAL */
#ifdef WLTWT
	MODULE_DETACH(wlc->twti, wlc_twt_detach);
#endif // endif
#ifdef WLHEB
	MODULE_DETACH(wlc->hebi, wlc_heb_detach);
#endif // endif
#if defined(WLC_SW_DIVERSITY) && !defined(WLC_SW_DIVERSITY_DISABLED)
	MODULE_DETACH(wlc->swdiv, wlc_swdiv_detach);
#endif // endif
#ifdef WL11AX
	MODULE_DETACH(wlc->hei, wlc_he_detach);
#endif // endif
#ifdef WL_RANDMAC
	MODULE_DETACH(wlc->randmac_info, wlc_randmac_detach);
#endif /* WL_RANDMAC */
	wlc_dump_detach(wlc->dumpi);

#if defined(WL_ASSOC_MGR) && !defined(WL_ASSOC_MGR_DISABLED)
	MODULE_DETACH(wlc->assoc_mgr, wlc_assoc_mgr_detach);
	wlc->pub->_assoc_mgr = FALSE;
#endif /* defined(WL_ASSOC_MGR) && !defined(WL_ASSOC_MGR_DISABLED) */
#if defined(SRHWVSDB) && !defined(SRHWVSDB_DISABLED)
	MODULE_DETACH(wlc->srvsdb_info, wlc_srvsdb_detach);
#endif // endif
	MODULE_DETACH(wlc->testi, wlc_test_detach);
	wlc_addrmatch_detach(wlc);

	MODULE_DETACH(wlc->sui, wlc_scan_utils_detach);
	MODULE_DETACH(wlc->rspeci, wlc_rspec_detach);
	MODULE_DETACH(wlc->qosi, wlc_qos_detach);
	MODULE_DETACH(wlc->pm, wlc_pm_detach);
	MODULE_DETACH(wlc->frag, wlc_frag_detach);
#if defined(PROP_TXSTATUS) && defined(PROP_TXSTATUS_ENABLED)
	MODULE_DETACH(wlc->wlfc, wlc_wlfc_detach);
#endif // endif
#if defined(PKTC) || defined(PKTC_DONGLE)
	MODULE_DETACH(wlc->pktc_info, wlc_pktc_detach);
#endif // endif
	MODULE_DETACH(wlc->vieli, wlc_bsscfg_viel_detach);
#ifdef WL_BSSCFG_TX_SUPR
	MODULE_DETACH(wlc->psqi, wlc_bsscfg_psq_detach);
#endif // endif

#if defined(SMF_STATS) && !defined(SMF_STATS_DISABLED)
	MODULE_DETACH(wlc->smfs, wlc_smfs_detach);
#endif // endif
	MODULE_DETACH(wlc->misc, wlc_misc_detach);

#if defined(AP)
	MODULE_DETACH(wlc->ap, wlc_ap_detach);
#endif // endif

#if defined(APCS) && !defined(APCS_DISABLED)
	MODULE_DETACH(wlc->cs, wlc_apcs_detach);
#endif /* APCS && !APCS_DISABLED */

#if defined(WLPROBRESP_MAC_FILTER) && !defined(WLPROBRESP_SW_DISABLED)
	MODULE_DETACH(wlc->mprobresp_mac_filter, wlc_probresp_mac_filter_detach);
#endif // endif
	MODULE_DETACH(wlc->macfltr, wlc_macfltr_detach);

#if defined(MBSS) && !defined(MBSS_DISABLED)
	MODULE_DETACH(wlc->mbss, wlc_mbss_detach);
#endif /* MBSS && !MBSS_DISABLED */
#if defined(MFP) && !defined(MFP_DISABLED)
	MODULE_DETACH(wlc->mfp, wlc_mfp_detach);
#endif // endif
	MODULE_DETACH(wlc->txc, wlc_txc_detach);
#if defined(WLBSSLOAD) || (defined(WLBSSLOAD_REPORT) && \
	!defined(WLBSSLOAD_REPORT_DISABLED))
	MODULE_DETACH(wlc->mbssload, wlc_bssload_detach);
#endif // endif
#ifdef WLDLS
	MODULE_DETACH(wlc->dls, wlc_dls_detach);
#endif // endif
#if defined(WL11U) && !defined(WL11U_DISABLED)
	MODULE_DETACH(wlc->m11u, wlc_11u_detach);
#endif // endif
#if defined(BCMAUTH_PSK) && !defined(BCMAUTH_PSK_DISABLED)
	MODULE_DETACH(wlc->authi, wlc_auth_detach);
#endif // endif
#ifdef WL11N
	MODULE_DETACH(wlc->asi, wlc_antsel_detach);
#endif // endif
#ifdef WET
	MODULE_DETACH(wlc->weth, wlc_wet_detach);
#endif // endif
#ifdef WET_TUNNEL
	MODULE_DETACH(wlc->wetth, wlc_wet_tunnel_detach);
#endif // endif
#ifdef WLAMSDU
	MODULE_DETACH(wlc->ami, wlc_amsdu_detach);
#endif // endif
#ifdef WLAMPDU
	MODULE_DETACH(wlc->ampdu_tx, wlc_ampdu_tx_detach);
	MODULE_DETACH(wlc->ampdu_rx, wlc_ampdu_rx_detach);
	wlc_ampdu_deinit(wlc);
#endif // endif
#ifdef WLNAR
	MODULE_DETACH(wlc->nar_handle, wlc_nar_detach);
#endif // endif
#ifdef WLTAF
	MODULE_DETACH(wlc->taf_handle, wlc_taf_detach);
#endif // endif

#if defined(WLMCHAN) && !defined(WLMCHAN_DISABLED)
	MODULE_DETACH(wlc->mchan, wlc_mchan_detach);
#endif // endif
#if defined(WLP2P) && !defined(WLP2P_DISABLED)
	MODULE_DETACH(wlc->p2p, wlc_p2p_detach);
#endif // endif
#if defined(BCMCOEXNOA) && !defined(BCMBTCOEX_DISABLED)
	MODULE_DETACH(wlc->cxnoa, wlc_cxnoa_detach);
#endif // endif
#if defined(RADIONOA_PWRSAVE) && !defined(RADIONOA_PWRSAVE_DISABLED)
	MODULE_DETACH(wlc->rpsnoa, wlc_rpsnoa_detach);
#endif // endif
	MODULE_DETACH(wlc->lqi, wlc_lq_detach);
#if defined(WLMCNX) && !defined(WLMCNX_DISABLED)
	MODULE_DETACH(wlc->tbtt, wlc_tbtt_detach);
	MODULE_DETACH(wlc->mcnx, wlc_mcnx_detach);
#endif // endif
	MODULE_DETACH(wlc->wrsi, wlc_scb_ratesel_detach);

#ifdef PSTA
	MODULE_DETACH(wlc->psta, wlc_psta_detach);
#endif // endif

#ifdef WLPLT
	if (WLPLT_ENAB(wlc->pub)) {
		MODULE_DETACH(wlc->plt, wlc_plt_detach);
	}
#endif // endif
#ifdef WOWL
	MODULE_DETACH(wlc->wowl, wlc_wowl_detach);
#endif // endif
#if defined(SEQ_CMDS)
	MODULE_DETACH(wlc->seq_cmds_info, wlc_seq_cmds_detach);
#endif /* SEQ_CMDS */
#if defined(AP)
	MODULE_DETACH(wlc->pmq, wlc_pmq_detach);
#endif // endif
#if defined(WLCAC) && !defined(WLCAC_DISABLED)
	MODULE_DETACH(wlc->cac, wlc_cac_detach);
#endif // endif
#ifdef WMF
	MODULE_DETACH(wlc->wmfi, wlc_wmf_detach);
#endif // endif
#if defined(WL11K) && !defined(WL11K_DISABLED)
	MODULE_DETACH(wlc->rrm_info, wlc_rrm_detach);
#endif /* WL11K && !WL11K_DISABLED */
#if defined(WLWNM) && !defined(WLWNM_DISABLED)
	MODULE_DETACH(wlc->wnm_info, wlc_wnm_detach);
#endif // endif
	wlc_stf_detach(wlc);
#if defined(STA) && defined(WLRM) && !defined(WLRM_DISABLED)
	MODULE_DETACH(wlc->rm_info, wlc_rm_detach);
#endif // endif
#if defined(WLWSEC) && !defined(WLWSEC_DISABLED)
	MODULE_DETACH(wlc->keymgmt, wlc_keymgmt_detach);
#endif /* WLWSEC && !WLWSEC_DISABLED */
#ifdef WL11H
	MODULE_DETACH(wlc->m11h, wlc_11h_detach);
#ifdef WLCSA
	MODULE_DETACH(wlc->csa, wlc_csa_detach);
#endif // endif
#ifdef WLQUIET
	MODULE_DETACH(wlc->quiet, wlc_quiet_detach);
#endif // endif
#ifdef WL_PM_MUTE_TX
	if (wlc->pm_mute_tx != NULL) {
		wlc_pm_mute_tx_detach(wlc->pm_mute_tx);
		wlc->pm_mute_tx = NULL;
	}
#endif // endif
#if defined WLDFS && !defined(WLDFS_DISABLED)
	MODULE_DETACH(wlc->dfs, wlc_dfs_detach);
#endif // endif
#endif /* WL11H */

#ifdef WLTPC
	MODULE_DETACH(wlc->tpc, wlc_tpc_detach);
#endif // endif
#if defined(WLTDLS) && !defined(WLTDLS_DISABLED)
	MODULE_DETACH(wlc->tdls, wlc_tdls_detach);
#endif /* defined(WLTDLS) && !defined(WLTDLS_DISABLED) */

#if defined(L2_FILTER) && !defined(L2_FILTER_DISABLED)
	MODULE_DETACH(wlc->l2_filter, wlc_l2_filter_detach);
#endif /* L2_FILTER && !L2_FILTER_DISABLED */

#ifdef WL11D
	MODULE_DETACH(wlc->m11d, wlc_11d_detach);
#endif // endif
#ifdef WLCNTRY
	MODULE_DETACH(wlc->cntry, wlc_cntry_detach);
#endif // endif
#ifdef CCA_STATS
#ifdef ISID_STATS
	MODULE_DETACH(wlc->itfr_info, wlc_itfr_detach);
#endif // endif
#ifndef CCA_STATS_DISABLED
	MODULE_DETACH(wlc->cca_info, wlc_cca_detach);
#endif /* CCA_STATS_DISABLED */

#endif /* CCA_STATS */
	MODULE_DETACH(wlc->prot, wlc_prot_detach);
	MODULE_DETACH(wlc->prot_g, wlc_prot_g_detach);
	MODULE_DETACH(wlc->prot_n, wlc_prot_n_detach);

#if defined(WLPROBRESP_SW) && !defined(WLPROBRESP_SW_DISABLED)
	MODULE_DETACH(wlc->mprobresp, wlc_probresp_detach);
#endif /* WLPROBRESP_SW && !WLPROBRESP_SW_DISABLED */
#ifdef WOWLPF
	MODULE_DETACH(wlc->wlc->wowlpf, wlc_wowlpf_detach);
#endif // endif

#ifdef WL_LPC
	MODULE_DETACH(wlc->wlpci, wlc_scb_lpc_detach);
#endif // endif

#ifdef WL11AC
	MODULE_DETACH(wlc->vhti, wlc_vht_detach);
#endif // endif
	MODULE_DETACH(wlc->obss, wlc_obss_detach);
	MODULE_DETACH(wlc->hti, wlc_ht_detach);

	MODULE_DETACH(wlc->akmi, wlc_akm_detach);
#ifdef STA
	MODULE_DETACH(wlc->hs20, wlc_hs20_detach);
#endif	/* STA */

#if (defined(WLFBT) && !defined(WLFBT_DISABLED))
	MODULE_DETACH(wlc->fbt, wlc_fbt_detach);
#endif // endif
#if (defined(BCMSUP_PSK) && defined(BCMINTSUP) && !defined(BCMSUP_PSK_DISABLED))
	MODULE_DETACH(wlc->idsup, wlc_sup_detach);
#endif // endif

	MODULE_DETACH(wlc->pmkid_info, wlc_pmkid_detach);

#ifdef WLOTA_EN
	MODULE_DETACH(wlc->ota_info, wlc_ota_test_detach);
#endif // endif

#if defined(WL_PROXDETECT) && !defined(WL_PROXDETECT_DISABLED)
	MODULE_DETACH(wlc->pdsvc_info, wlc_proxd_detach);
#endif // endif

#if defined(WL_RELMCAST) && !defined(WL_RELMCAST_DISABLED)
	MODULE_DETACH(wlc->rmc, wlc_rmc_detach);
#endif // endif

	MODULE_DETACH(wlc->btch, wlc_btc_detach);

#if defined(WLSLOTTED_BSS)
	MODULE_DETACH(wlc->sbi, wlc_slotted_bss_detach);
#endif // endif

#ifdef WL_STAPRIO
	MODULE_DETACH(wlc->staprio, wlc_staprio_detach);
#endif /* WL_STAPRIO */

#ifdef WL_STA_MONITOR
	MODULE_DETACH(wlc->stamon_info, wlc_stamon_detach);
#endif /* WL_STA_MONITOR */

#if defined(WL_MONITOR) && !defined(WL_MONITOR_DISABLED)
	/* Detach monitor module from wlc */
	MODULE_DETACH(wlc->mon_info, wlc_monitor_detach);
#endif /* WL_MONITOR && !WL_MONITOR_DISABLED */

#ifdef WDS
	MODULE_DETACH(wlc->mwds, wlc_wds_detach);
#endif /* WDS */

#if (defined(WL_OKC) && !defined(WL_OKC_DISABLED)) || (defined(WLRCC) && \
	!defined(WLRCC_DISABLED))
	MODULE_DETACH(wlc->okc_info, wlc_okc_detach);
#endif /* WL_OKC || WLRCC */

#if defined(WL_MU_RX) && !defined(WL_MU_RX_DISABLED)
	MODULE_DETACH(wlc->murx, wlc_murx_detach);
#endif // endif

#if defined(WL_BEAMFORMING) && !defined(WLTXBF_DISABLED)
	MODULE_DETACH(wlc->txbf, wlc_txbf_detach);
#endif /* WL_BEAMFORMING */
#if defined(WL_RATELINKMEM) && !defined(WL_RATELINKMEM_DISABLED)
	MODULE_DETACH(wlc->rlmi, wlc_ratelinkmem_detach);
#endif /* WL_RATELINKMEM && !WL_RATELINKMEM_DISABLED */

	MODULE_DETACH(wlc->pui, wlc_perf_utils_detach);
	if (wlc->ebd) {
		MFREE(wlc->osh, wlc->ebd, sizeof(wlc_early_bcn_detect_t));
		wlc->ebd = NULL;
	}

#if defined(WL_STATS) && !defined(WL_STATS_DISABLED)
	MODULE_DETACH(wlc->stats_info, wlc_stats_detach);
#endif /* WL_STATS && WL_STATS_DISABLED */

#ifdef WLDURATION
	MODULE_DETACH(wlc->dur_info, wlc_duration_detach);
#endif /* WLDURATION */

#if defined(SCB_BS_DATA)
	MODULE_DETACH(wlc->bs_data_handle, wlc_bs_data_detach);
#endif // endif

#if defined(WL_OBSS_DYNBW) && !defined(WL_OBSS_DYNBW_DISABLED)
	MODULE_DETACH(wlc->obss_dynbw, wlc_obss_dynbw_detach);
#endif /* defined(WL_OBSS_DYNBW) && !defined(WL_OBSS_DYNBW_DISABLED) */

#if defined(WL_PROT_OBSS) && !defined(WL_PROT_OBSS_DISABLED)
	MODULE_DETACH(wlc->prot_obss, wlc_prot_obss_detach);
#endif // endif

#if defined(WLRSDB)
#if defined(WLRSDB_POLICY_MGR) && !defined(WLRSDB_POLICY_MGR_DISABLED)
	MODULE_DETACH(rsdb_policy_info, wlc_rsdb_policymgr_detach);
#endif /* WLRSDB_POLICY_MGR && WLRSDB */
	if (RSDB_ENAB(wlc->pub))
		MODULE_DETACH(wlc->rsdbinfo, wlc_rsdb_detach);
#endif /* WLRSDB */

#if defined(WL_MODESW) && !defined(WL_MODESW_DISABLED)
	MODULE_DETACH(wlc->modesw, wlc_modesw_detach);
#endif // endif

#ifdef WL_LTR
	MODULE_DETACH(wlc->ltr_info, wlc_ltr_detach);
#endif // endif

#if defined(BCMLTECOEX) && !defined(BCMLTECOEX_DISABLED)
	MODULE_DETACH(wlc->ltecx, wlc_ltecx_detach);
#endif /* defined(BCMLTECOEX) && !defined(BCMLTECOEX_DISABLED) */
#if defined(WL_PWRSTATS) && !defined(WL_PWRSTATS_DISABLED)
	MODULE_DETACH(wlc->pwrstats, wlc_pwrstats_detach);
#endif /* PWRSTATS */

#ifdef WL_EXCESS_PMWAKE
	if (wlc->excess_pmwake) {
		MFREE(wlc->osh, wlc->excess_pmwake, sizeof(wlc_excess_pm_wake_info_t));
		wlc->excess_pmwake = NULL;
	}
#endif /* WL_EXCESS_PMWAKE */
#if defined(WL_TBOW) && !defined(WL_TBOW_DISABLED)
	MODULE_DETACH(wlc->tbow_info, wlc_tbow_detach);
#endif /* defined(WL_TBOW) && !defined(WL_TBOW_DISABLED) */

#if defined(WL_BWTE) && !defined(WL_BWTE_DISABLED)
	MODULE_DETACH(wlc->bwte_info, wlc_bwte_detach);
#endif /* defined(WL_BWTE) && !defined(WL_BWTE_DISABLED) */

#if defined(WL_BCNTRIM) && !defined(WL_BCNTRIM_DISABLED)
	MODULE_DETACH(wlc->bcntrim, wlc_bcntrim_detach);
#endif // endif

	MODULE_DETACH(wlc->txqi, wlc_txq_detach);
	MODULE_DETACH(wlc->macdbg, wlc_macdbg_detach);

#if defined(WLVASIP) && !defined(WLVASIP_DISABLED)
	MODULE_DETACH(wlc->vasip, wlc_vasip_detach);
#endif /* WLVASIP */

#if defined(GTKOE) && !defined(GTKOE_DISABLED)
	MODULE_DETACH(wlc->idsup, wlc_gtk_detach);
#endif /* defined(GTKOE) && !defined(GTKOE_DISABLED) */
	MODULE_DETACH(wlc->scan, wlc_scan_detach);

#if defined(BCMULP) && !defined(WLULP_DISABLED)
	MODULE_DETACH(wlc->ulp, wlc_ulp_detach);
#endif // endif
	MODULE_DETACH(wlc->chanctxt_info, wlc_chanctxt_detach);
#ifdef STA
	MODULE_DETACH(wlc->sta_info, wlc_sta_detach);
#endif // endif
	MODULE_DETACH(wlc->msch_info, wlc_msch_detach);

#if defined(WL_HEALTH_CHECK)
	MODULE_DETACH(wlc->hc, wlc_health_check_detach);
#endif // endif

#ifdef WIFI_ACT_FRAME
	MODULE_DETACH(wlc->wlc_act_frame_info, wlc_act_frame_detach);
#endif // endif

	wlc_chctx_detach(wlc->chctxi);

	MODULE_DETACH(wlc->as, wlc_assoc_detach);
#if defined(WL_ASDB) && !defined(WLC_ASDB_DISABLED)
	MODULE_DETACH(wlc->asdb, wlc_asdb_detach);
#endif /* WL_ASDB */

#if defined(WLC_DEBUG_CRASH)
	MODULE_DETACH(wlc->debug_crash_info, wlc_debug_crash_detach);
#endif /* WLC_DEBUG_CRASH */

#ifdef WL_TX_STALL
	/* Detach TX stall health check module */
	MODULE_DETACH(wlc->tx_stall, wlc_tx_stall_detach);
#endif // endif

#if defined(WLC_TSYNC)
	MODULE_DETACH(wlc->tsync, wlc_tsync_detach);
#endif /* WLC_TSYNC */

#if defined(WL_MU_TX)
	MODULE_DETACH(wlc->mutx, wlc_mutx_detach);
#endif // endif

	MODULE_DETACH(wlc->eventq, wlc_eventq_detach);

#if defined(WLC_ADPS) && !defined(WLC_ADPS_DISABLED)
	MODULE_DETACH(wlc->adps_info, wlc_adps_detach);
#endif /* WLC_ADPS && !WLC_ADPS_DISABLED */

#if defined(WLC_BAM) && !defined(WLC_BAM_DISABLED)
	MODULE_DETACH(wlc->bam_info, wlc_bam_detach);
#endif /* WLC_BAM && !WLC_BAM_DISABLED */

#if defined(WL_FILTERIE) && !defined(WL_FILTERIE_DISABLED)
	MODULE_DETACH(fiei, wlc_filter_ie_detach);
#endif /* WL_FILTERIE && WL_FILTERIE_DISABLED */
#if defined(TDMTX) && !defined(TDM_TX_DISABLED)
	MODULE_DETACH(wlc->tdmtx, wlc_tdm_tx_detach);
#endif /* TDMTX && ! TDM_TX_DISABLED */

#if defined(WL_OPS) && !defined(WL_OPS_DISABLED)
	MODULE_DETACH(wlc->ops_info, wlc_ops_detach);
#endif /* OPS */
#ifdef WLCFP
	/* detach CFP Module */
	MODULE_DETACH(wlc->cfp, wlc_cfp_detach);
#endif // endif
#if defined(WLC_OFFLOADS_TXSTS) || defined(WLC_OFFLOADS_RXSTS)
	MODULE_DETACH(wlc->offl, wlc_offload_detach);
#endif /* WLC_OFFLOADS_TXSTS || WLC_OFFLOADS_RXSTS */
#if defined(WL_ESP) && !defined(WL_ESP_DISABLED)
	/* Detach OCE module */
	MODULE_DETACH(wlc->esp, wlc_esp_detach);
#endif /* WL_ESP && !WL_ESP_DISABLED */
#ifdef PKTQ_LOG
	MODULE_DETACH(wlc->rx_report_handle, wlc_rx_report_detach);
#endif // endif

} /* wlc_detach_module */

/** detach infra. modules that must be done the last after all other modules */
/* NOTE: DO NOT ADD REGULAR MODULE DETACH CALLS HERE, ADD THEM TO
 * wlc_detach_module() INSTEAD.
 */
static void
BCMATTACHFN(wlc_detach_module_post)(wlc_info_t *wlc)
{
/* DON'T ADD ANY NEW MODULES HERE */
#ifdef WLTDLS
	if (wlc->ier_tdls_drs != NULL)
		wlc_ier_destroy_registry(wlc->ier_tdls_drs);
	if (wlc->ier_tdls_scf != NULL)
		wlc_ier_destroy_registry(wlc->ier_tdls_scf);
	if (wlc->ier_tdls_srs != NULL)
		wlc_ier_destroy_registry(wlc->ier_tdls_srs);
	if (wlc->ier_tdls_srq != NULL)
		wlc_ier_destroy_registry(wlc->ier_tdls_srq);
#endif // endif
#ifdef WL11AC
	if (wlc->ier_csw != NULL)
		wlc_ier_destroy_registry(wlc->ier_csw);
#endif // endif
#if (defined(WLFBT) && !defined(WLFBT_DISABLED))
	if (wlc->ier_fbt != NULL)
		wlc_ier_destroy_registry(wlc->ier_fbt);

	if (wlc->ier_ric != NULL)
		wlc_ier_destroy_registry(wlc->ier_ric);
#endif // endif

	MODULE_DETACH(wlc->chctxi, wlc_chctx_post_detach);

	wlc_chansw_detach(wlc);

	/* Keep these the last and in the this order since other modules
	 * may reference them.
	 */
	MODULE_DETACH(wlc->txmodi, wlc_txmod_detach);
	MODULE_DETACH(wlc->ieri, wlc_ier_detach);
	MODULE_DETACH(wlc->iemi, wlc_iem_detach);
	MODULE_DETACH(wlc->scbstate, wlc_scb_detach);
	MODULE_DETACH(wlc->bcmh, wlc_bsscfg_detach);
	MODULE_DETACH(wlc->pcb, wlc_pcb_detach);
	MODULE_DETACH(wlc->hrti, wlc_hrt_detach);
	MODULE_DETACH(wlc->iocvi, wlc_iocv_detach);
	MODULE_DETACH(wlc->notif, bcm_notif_detach);
/* DON'T ADD ANY NEW MODULES HERE */
} /* wlc_detach_module_post */

/**
 * Return a count of the number of driver callbacks still pending.
 *
 * General policy is that wlc_detach can only dealloc/free software states. It can NOT
 *  touch hardware registers since the d11core may be in reset and clock may not be available.
 *    One exception is sb register access, which is possible if crystal is turned on
 * After "down" state, driver should avoid software timer with the exception of radio_monitor.
 */
uint
BCMATTACHFN(wlc_detach)(wlc_info_t *wlc)
{
	uint i;
	uint callbacks = 0;
	wlc_bsscfg_t *bsscfg;

	if (wlc == NULL)
		return 0;

	WL_TRACE(("wl%d: %s\n", wlc->pub->unit, __FUNCTION__));

	ASSERT(!wlc->pub->up);

#ifdef RX_DEBUG_ASSERTS
	/* Remove RXS backup Storage */
	if (wlc->rxs_bkp) {
		MFREE(wlc->osh, wlc->rxs_bkp, MAX_RXS_BKP_ENTRIES * PER_RXS_SIZE);
	}
#endif /* RX_DEBUG_ASSERTS */

#if defined(EVENT_LOG_COMPILE) && defined(WLCNT)
	wlc_chansw_notif_unregister(wlc, wlc_ctl_mgt_frame_counter_report, wlc);
#endif // endif

#ifdef WLLED
	if (wlc->ledh) {
		wlc_led_deinit(wlc->ledh);
	}
#endif // endif

	/* delete software timers */
	if (!wlc_radio_monitor_stop(wlc))
		callbacks++;

#ifdef STA
	/* ...including any wake_event timer */
	if (wlc->wake_event_timer) {
		if (!wl_del_timer(wlc->wl, wlc->wake_event_timer))
			callbacks ++;
	}
#endif /* STA */

#ifdef WLLED
	if (wlc->ledh) {
		callbacks +=  wlc_led_detach(wlc->ledh);
		wlc->ledh = NULL;
	}
#endif // endif

	/* free the internal scbs */
	wlc_hwrsscbs_free(wlc);

	/* Free memory for bsscfg structs */
	FOREACH_BSS(wlc, i, bsscfg) {
		wlc_bsscfg_free(wlc, bsscfg);
	}

	MODULE_DETACH(wlc->cmi, wlc_channel_mgr_detach);

	wlc_timers_deinit(wlc);

	/* XXX: set detach queue to null to allow wlc_txq_free() to free
	* detach queue and avert looping forever issue
	*/
	wlc->txfifo_detach_transition_queue = NULL;

	while (wlc->tx_queues != NULL) {
		wlc_txq_free(wlc, wlc->osh, wlc->tx_queues);
	}

	wlc_detach_module(wlc);

	wlc_detach_module_post(wlc);

	/* XXX:do not change the order of the functions below, sih is
	 * still used inside wlc_bmac_detach()
	 */
	/* free all h/w states */
	callbacks += wlc_bmac_detach(wlc);

	/* free the sih now */
	if (wlc->pub->sih) {
		wlc_bmac_si_detach(wlc->osh, wlc->pub->sih);
		wlc->pub->sih = NULL;
	}

#ifdef BCMHWA
	if (hwa_dev) {
		hwa_detach(hwa_dev);
		hwa_dev = NULL;
	}
#endif /* BCMHWA */

	/* free other state */

	/* free vars now */
	if (wlc->pub->vars) {
		MFREE(wlc->osh, wlc->pub->vars, wlc->pub->vars_size);
		wlc->pub->vars = NULL;
	}

#if defined(RWL_WIFI) || defined(WIFI_REFLECTOR)
	if (wlc->rwl) {
		MODULE_DETACH(wlc->rwl, wlc_rwl_detach);
		wlc->rwl = NULL;
	}
#endif /* RWL_WIFI || WIFI_REFLECTOR */

	/*
	 * consistency check: wlc_module_register/wlc_module_unregister calls
	 * should match therefore nothing should be left here.
	 */
	for (i = 0; i < wlc->pub->max_modules; i ++) {
		if (wlc->modulecb[i].name[0] == 0)
			continue;
		if (obj_registry_islast(wlc->objr)) {
			WL_ERROR(("wl%d: module %s is still registered\n", wlc->pub->unit,
				wlc->modulecb[i].name));
			ASSERT(wlc->modulecb[i].name[0] == '\0');
		}
	}

	WL_MPC(("wl%d: DETACHED, callbacks %d\n", wlc->pub->unit, callbacks));

	/* Free the memory pool manager. */
	bcm_mpm_deinit(&wlc->mem_pool_mgr);

	MODULE_DETACH(wlc->dumpi, wlc_dump_post_detach);

	MODULE_DETACH_2(wlc, wlc->osh, wlc_detach_mfree);

	return (callbacks);
} /* wlc_detach */

/* channel switch notification */
int
BCMATTACHFN(wlc_chansw_notif_register)(wlc_info_t *wlc, wlc_chansw_notif_fn_t fn, void *arg)
{
	return bcm_notif_add_interest(wlc->chansw_hdl, (bcm_notif_client_callback)fn, arg);
}

int
BCMATTACHFN(wlc_chansw_notif_unregister)(wlc_info_t *wlc, wlc_chansw_notif_fn_t fn, void *arg)
{
	if (wlc->chansw_hdl == NULL)
		return BCME_NOTREADY;

	return bcm_notif_remove_interest(wlc->chansw_hdl, (bcm_notif_client_callback)fn, arg);
}

static void wlc_chansw_notif(wlc_info_t *wlc, int reason_bitmap, chanspec_t old, chanspec_t new,
	uint32 tsf_l)
{
	uint32 reason;

	for (reason = 0; reason_bitmap && reason < CHANSW_MAX_NUMBER; reason++,
		reason_bitmap >>= 1) {
		if (reason_bitmap & 0x0001) {
			wlc_chansw_notif_signal(wlc, reason, old, new, tsf_l);
		}
	}
	wlc->last_chansw_time = tsf_l;
}

static void
wlc_chansw_notif_signal(wlc_info_t *wlc, int reason, chanspec_t old, chanspec_t new, uint32 tsf_l)
{
	wlc_chansw_notif_data_t data;
	uint32 now_l = R_REG(wlc->osh, D11_TSFTimerLow(wlc));
	uint32 dwelltime = (tsf_l - wlc->last_chansw_time)/1000;

	BCM_REFERENCE(now_l);
	BCM_REFERENCE(dwelltime);

	if (WL_MCHAN_ON()) {
		WL_MCHAN(("wl%d: %s - completed in %d uS at 0x%x\n",
		          wlc->pub->unit, __FUNCTION__, (int32)(now_l - tsf_l), now_l));
	}

	chanswitch_history(wlc, old, new, tsf_l, now_l, reason, CHANSWITCH_SET_CHANSPEC);

#if defined(EVENT_LOG_COMPILE)
	EVENT_LOG(EVENT_LOG_TAG_TRACE_CHANSW,
		"time=%uus old=0x%04x new=0x%04x reason=%d dwelltime=%dms core=%d\n",
		tsf_l, old, new, reason, dwelltime, wlc->hw->macunit);
#endif /* EVENT_LOG_COMPILE */
#ifdef WLRSDB
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_RSDB)
	if (RSDB_ENAB(wlc->pub)) {
		wlc_rsdb_chan_switch_dump(wlc, old, dwelltime);
	}
#endif // endif
#endif /* WLRSDB */

	if (wlc->chansw_hdl) {
		data.old_chanspec = old;
		data.new_chanspec = new;
		data.tsf_l = tsf_l;
		data.tsf_h = 0;
		bcm_notif_signal(wlc->chansw_hdl, &data);
	} else {
		WL_ERROR(("Not ready, skip channel switch callback\n"));
	}
}

/** update state that depends on the current value of "ap" */
void
wlc_ap_upd(wlc_info_t* wlc, wlc_bsscfg_t *bsscfg)
{
	/* force AUTO where AP uses it. */
	if (bsscfg && BSSCFG_AP(bsscfg))
		/* AP: short not allowed, but not enforced */
		bsscfg->PLCPHdr_override = WLC_PLCP_AUTO;

	/* disable vlan_mode on AP since some legacy STAs cannot rx tagged pkts */
	wlc->vlan_mode = AP_ENAB(wlc->pub) ? OFF : AUTO;

	/* always disable oper_mode on STA/AP switch */
	if (bsscfg)
		bsscfg->oper_mode_enabled = FALSE;
}

/** read hwdisable state and propagate to wlc flag */
static void
wlc_radio_hwdisable_upd(wlc_info_t* wlc)
{
	if ((wlc->pub->wlfeatureflag & WL_SWFL_NOHWRADIO) ||
	    wlc->pub->hw_off)
		return;

	if (wlc_bmac_radio_read_hwdisabled(wlc->hw)) {
		mboolset(wlc->pub->radio_disabled, WL_RADIO_HW_DISABLE);
	} else {
		mboolclr(wlc->pub->radio_disabled, WL_RADIO_HW_DISABLE);
	}
}

bool
wlc_mpccap(wlc_info_t* wlc)
{
	bool mpc_capable = TRUE;

#ifdef PSTA
	if (PSTA_ENAB(wlc->pub))
		mpc_capable = FALSE;
#endif // endif
	/* MPC cannot be tested on QT builds. disable it by default */
	if (ISSIM_ENAB(wlc->pub->sih))
		mpc_capable = FALSE;

	return mpc_capable;
}

#ifdef STA
/** return TRUE if Minimum Power Consumption should be entered, FALSE otherwise */
static bool
wlc_is_non_delay_mpc(wlc_info_t* wlc)
{
	int idx;
	wlc_bsscfg_t *cfg;

	/* mpc feature must be enabled when this function is called! */
	ASSERT(wlc->mpc);

	/* force mpc off when any of these global conditions meet */
	if (wlc->pub->associated ||
		SCAN_IN_PROGRESS(wlc->scan) || MONITOR_ENAB(wlc) ||
		wlc->mpc_out || wlc->mpc_scan || wlc->mpc_join ||
		wlc->mpc_oidscan || wlc->mpc_oidjoin || wlc->mpc_oidnettype ||
		wlc->pub->delayed_down ||
		wlc->txfifo_detach_pending ||
		wlc->mpc_off_req) {

		return FALSE;
	}

	/* force mpc off when any of these per bsscfg conditions meet */
	FOREACH_BSS(wlc, idx, cfg) {
		if ((BSSCFG_STA(cfg) &&
			(cfg->assoc->state != AS_IDLE || cfg->assoc->rt ||
			wlc_keymgmt_tkip_cm_enabled(wlc->keymgmt, cfg))) ||
		    (BSSCFG_AP(cfg) && ((cfg->enable && !cfg->up) ||
		    (wlc_bss_wdsscb_getcnt(wlc, cfg) > 0))))
		    return FALSE;
	}

	return TRUE;
}

bool
wlc_ismpc(wlc_info_t* wlc)
{
	return ((wlc->mpc_delay_off == 0) && (wlc_is_non_delay_mpc(wlc)));
}

void
wlc_radio_mpc_upd(wlc_info_t* wlc)
{
	bool radio_state;

	/*
	* Clear the WL_RADIO_MPC_DISABLE bit when mpc feature is disabled
	* in case the WL_RADIO_MPC_DISABLE bit was set. Stop the radio
	* monitor also when WL_RADIO_MPC_DISABLE is the only reason that
	* the radio is going down.
	*/
	if (!wlc->mpc) {
		WL_MPC(("wl%d: radio_disabled %x radio_monitor %d\n", wlc->pub->unit,
			wlc->pub->radio_disabled, wlc->radio_monitor));
		if (!wlc->pub->radio_disabled)
			return;
		wlc->mpc_dur += OSL_SYSUPTIME() - wlc->mpc_laston_ts;
		wlc->mpc_lastoff_ts = OSL_SYSUPTIME();
	if (HND_DS_HC_ENAB()) {
		wl_health_check_notify(wlc->wl,
			HEALTH_CHECK_WL_RADIO_MPC_OFF, TRUE);
	}
		mboolclr(wlc->pub->radio_disabled, WL_RADIO_MPC_DISABLE);
		wlc_radio_upd(wlc);
		if (!wlc->pub->radio_disabled)
			wlc_radio_monitor_stop(wlc);
		return;
	}

	/*
	 * sync ismpc logic with WL_RADIO_MPC_DISABLE bit in wlc->pub->radio_disabled
	 * to go ON, always call radio_upd synchronously
	 * to go OFF, postpone radio_upd to later when context is safe(e.g. watchdog)
	 */
	radio_state = (mboolisset(wlc->pub->radio_disabled, WL_RADIO_MPC_DISABLE) ? OFF : ON);

	WL_MPC(("radio_state %d, non-del-mpc %d, delay_off %d, delay_cnt %d, off_time %u\n",
		radio_state, wlc_is_non_delay_mpc(wlc),
		wlc->mpc_delay_off, wlc->mpc_dlycnt,
		OSL_SYSUPTIME() - wlc->mpc_off_ts));

	if (wlc_ismpc(wlc) == TRUE) {
		/* Just change the state here. Let later context bring down the radio */
		mboolset(wlc->pub->radio_disabled, WL_RADIO_MPC_DISABLE);
		if (HND_DS_HC_ENAB()) {
			wl_health_check_notify(wlc->wl,
				HEALTH_CHECK_WL_RADIO_MPC_OFF, FALSE);
		}
	} else {
		if (radio_state == OFF) {
#ifdef WLRSDB
			/* For RSDB, Do not allow UP of Core1 in MIMO mode */
			if (RSDB_ENAB(wlc->pub) && !wlc_rsdb_up_allowed(wlc)) {
				return;
			}
#endif /* WLRSDB */
			/* Clear the state and bring up radio immediately */
			mboolclr(wlc->pub->radio_disabled, WL_RADIO_MPC_DISABLE);
			wlc_radio_upd(wlc);

			/* Calculate the mpc_dlycnt based on how long radio was off */
			if (OSL_SYSUPTIME() - wlc->mpc_off_ts < WLC_MPC_THRESHOLD * 1000) {
				wlc->mpc_dlycnt = WLC_MPC_MAX_DELAYCNT;
			} else	{
				wlc->mpc_dlycnt = WLC_MPC_MIN_DELAYCNT;
			}

			WL_MPC(("wl%d: mpc delay %d\n", wlc->pub->unit, wlc->mpc_dlycnt));
			wlc->mpc_dur += OSL_SYSUPTIME() - wlc->mpc_laston_ts;
			wlc->mpc_lastoff_ts = OSL_SYSUPTIME();
		}
		if (wlc_is_non_delay_mpc(wlc) == FALSE)
			wlc->mpc_delay_off = wlc->mpc_dlycnt;
	}
} /* wlc_radio_mpc_upd */

int wlc_mpc_off_req_set(wlc_info_t *wlc, mbool mask, mbool val)
{
	int ret = BCME_OK;

	/* set the MPC request mask and adjust the radio if necessary */
	wlc->mpc_off_req = (wlc->mpc_off_req & ~mask) | (val & mask);

	/* update radio MPC state from this setting */
	wlc_radio_mpc_upd(wlc);

	return ret;
}

static bool
wlc_scb_wds_cb(struct scb *scb)
{
	BCM_REFERENCE(scb);

	return SCB_WDS(scb) != NULL ? TRUE : FALSE;
}

static uint8
wlc_bss_wdsscb_getcnt(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	return wlc_bss_scb_getcnt(wlc, cfg, wlc_scb_wds_cb);
}

#endif /* STA */

static bool
wlc_scb_ps_cb(struct scb *scb)
{
	return (SCB_PS(scb) != 0) ? TRUE : FALSE;
}

uint8
wlc_bss_psscb_getcnt(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	return wlc_bss_scb_getcnt(wlc, cfg, wlc_scb_ps_cb);
}

static void
wlc_uslp_disable(wlc_info_t *wlc)
{
	wlc_bmac_mhf(wlc->hw, MHF1, MHF1_ULP, MHF1_ULP, WLC_BAND_ALL);
	/* cancel the watchdog timer */
	if (wlc->WDarmed) {
		wl_del_timer(wlc->wl, wlc->wdtimer);
		wlc->WDarmed = FALSE;
	}
}

static void
wlc_uslp_enable(wlc_info_t *wlc)
{
	wlc_bmac_mhf(wlc->hw, MHF1, MHF1_ULP, 0, WLC_BAND_ALL);
	/* start one second watchdog timer */
	if (!wlc->WDarmed) {
		wlc_refresh_wd_timer(wlc, TIMER_INTERVAL_WATCHDOG, TRUE);
	}
}

/**
 * centralized radio disable/enable function,
 * invoke radio enable/disable after updating hwradio status
 */
void
wlc_radio_upd(wlc_info_t* wlc)
{
	bool changed = FALSE;
	WL_MPC(("wl%d, wlc_radio_upd, radio_disabled 0x%x\n", wlc->pub->unit,
		wlc->pub->radio_disabled));

	if (wlc->pub->radio_disabled) {
		wlc->mpc_laston_ts = OSL_SYSUPTIME();
		wlc->total_on_time += (wlc->mpc_laston_ts - wlc->mpc_lastoff_ts);
		if (wlc->mpc_mode == WLC_MPC_MODE_2) {
			wlc_uslp_disable(wlc);
		} else {
			if (HND_DS_HC_ENAB()) {
				wl_health_check_notify(wlc->wl,
					HEALTH_CHECK_WL_RADIO_ENABLE, FALSE);
				wl_health_check_notify(wlc->wl,
					HEALTH_CHECK_WL_STAY_AWAKE, STAY_AWAKE(wlc));
				wl_health_check_notify(wlc->wl,
					HEALTH_CHECK_WL_SCAN_IN_PROGRESS,
					SCAN_IN_PROGRESS(wlc->scan));
			}
			changed = wlc_radio_disable(wlc);
		}
	} else {
		if (wlc->mpc_mode == WLC_MPC_MODE_2) {
			wlc_uslp_enable(wlc);
		} else {
			if (HND_DS_HC_ENAB() &&
				!wlc->mpc_mode) {
				wl_health_check_notify(wlc->wl, HEALTH_CHECK_WL_RADIO_ENABLE, TRUE);
			}
			changed = wlc_radio_enable(wlc);
		}
	}

	if (wlc->pub->last_radio_disabled != wlc->pub->radio_disabled) {
		changed = TRUE;
		wlc->pub->last_radio_disabled = wlc->pub->radio_disabled;
	}

	/* Send RADIO indications only if radio state actually changed */
	if (changed) {
#ifdef STA
		/* signal event to OS dependent layer */
		wlc_mac_event(wlc, WLC_E_RADIO, NULL, 0, 0, 0, 0, 0);
#endif // endif
	}
} /* wlc_radio_upd */

/** maintain LED behavior in down state */
static void
wlc_down_led_upd(wlc_info_t *wlc)
{
#ifdef STA
	int idx;
	wlc_bsscfg_t *cfg;
#endif // endif
	ASSERT(!wlc->pub->up);

#ifdef STA
	/* maintain LEDs while in down state, turn on sbclk if not available yet */
	FOREACH_BSS(wlc, idx, cfg) {
		if (BSSCFG_STA(cfg)) {
			/* turn on sbclk if necessary */
			wlc_pllreq(wlc, TRUE, WLC_PLLREQ_FLIP);

#ifdef WLLED
			wlc_led_event(wlc->ledh);
#endif // endif

			wlc_pllreq(wlc, FALSE, WLC_PLLREQ_FLIP);

			break;
		}
	}
#endif /* STA */
}

/** Returns TRUE if function actually brings down the nic or radio */
bool
wlc_radio_disable(wlc_info_t *wlc)
{
	if (!wlc->pub->up) {
		wlc_down_led_upd(wlc);
		return FALSE;
	}

	/* Check if are already going down */
	if (wlc->state == WLC_STATE_GOING_DOWN) {
		return FALSE;
	}

	/* XXX - make sure this is only called in safe context.
	 * i.e. it CANNOT be called in middle of dpc or followed by register access
	 */
	WL_MPC(("wl%d: wlc_radio_disable, radio off\n", wlc->pub->unit));
	wlc_radio_monitor_start(wlc);
	WL_APSTA_UPDN(("wl%d: wlc_radio_disable() -> wl_down()\n", wlc->pub->unit));
	wl_down(wlc->wl);
	return TRUE;
} /* wlc_radio_disable */

/** Returns True if function actually brings up the nic or radio */
static bool
wlc_radio_enable(wlc_info_t *wlc)
{
	if (wlc->pub->up)
		return FALSE;

	if (DEVICEREMOVED(wlc))
		return FALSE;

	if (!wlc->down_override) {	/* imposed by wl down/out ioctl */
		WL_MPC(("wl%d: wlc_radio_enable, radio on\n", wlc->pub->unit));
		WL_APSTA_UPDN(("wl%d: wlc_radio_enable() -> wl_up()\n", wlc->pub->unit));
		wl_up(wlc->wl);
	}
	return TRUE;
} /* wlc_radio_enable */

#ifndef ATE_BUILD
/** periodical query hw radio button while driver is "down" */
static void
wlc_radio_timer(void *arg)
{
	wlc_info_t *wlc = (wlc_info_t*)arg;

	if (DEVICEREMOVED(wlc)) {
		WL_ERROR(("wl%d: %s: dead chip\n", wlc->pub->unit, __FUNCTION__));
		wl_down(wlc->wl);
		return;
	}

	/* validate all the reasons driver could be down and running this radio_timer */
	ASSERT(wlc->pub->radio_disabled || wlc->down_override);
	wlc_radio_hwdisable_upd(wlc);
	wlc_radio_upd(wlc);
}
#endif /* ATE_BUILD */

static bool
wlc_radio_monitor_start(wlc_info_t *wlc)
{
	wlc->mpc_off_ts = OSL_SYSUPTIME();

	/* Don't start the timer if HWRADIO feature is disabled */
	if (wlc->radio_monitor || (wlc->pub->wlfeatureflag & WL_SWFL_NOHWRADIO))
		return TRUE;

	wlc->radio_monitor = TRUE;
	wlc_pllreq(wlc, TRUE, WLC_PLLREQ_RADIO_MON);
	wl_add_timer(wlc->wl, wlc->radio_timer, TIMER_INTERVAL_RADIOCHK, TRUE);
	return TRUE;
}

bool
wlc_radio_monitor_stop(wlc_info_t *wlc)
{
	if (!wlc->radio_monitor)
		return TRUE;

	ASSERT((wlc->pub->wlfeatureflag & WL_SWFL_NOHWRADIO) != WL_SWFL_NOHWRADIO);

	wlc->radio_monitor = FALSE;
	wlc_pllreq(wlc, FALSE, WLC_PLLREQ_RADIO_MON);
	return (wl_del_timer(wlc->wl, wlc->radio_timer));
}

bool
wlc_down_for_mpc(wlc_info_t *wlc)
{
	if (!wlc->pub->up && !wlc->down_override &&
	    mboolisset(wlc->pub->radio_disabled, WL_RADIO_MPC_DISABLE) &&
	    !mboolisset(wlc->pub->radio_disabled, ~WL_RADIO_MPC_DISABLE))
		return TRUE;

	return FALSE;
}

bool
wlc_check_assert_type(wlc_info_t *wlc, uint32 reinit_reason)
{
	bool is_out = FALSE;
	if (g_assert_type == 3) {
		WL_ERROR(("wl%d: %s call wlc_out(): reinit_reason %d down_override %d\n",
			wlc->pub->unit, __FUNCTION__, reinit_reason, wlc->down_override));
		/* bring the driver down without resetting hardware */
		wlc->down_override = TRUE;
		wlc->psm_watchdog_debug = TRUE;
		wlc_out(wlc);

		/* halt the PSM */
		wlc_bmac_mctrl(wlc->hw, MCTL_PSM_RUN, 0);
		wlc->psm_watchdog_debug = FALSE;
		is_out = TRUE;
	}
	else
	{
		WL_ERROR(("wl%d: %s HAMMERING: reinit_reason %d\n",
			wlc->pub->unit, __FUNCTION__, reinit_reason));
		/* big hammer */
		if (wlc->hw->need_reinit == WL_REINIT_RC_NONE) {
			wlc->hw->need_reinit = reinit_reason;
		}
		/* Force reinit of btcx shmem's when driver comes back up */
		wlc->hw->btc->btc_init_flag = FALSE;
		WLC_FATAL_ERROR(wlc);
	}
	return is_out;
}

/** bring the driver down, but don't reset hardware */
static void
wlc_out(wlc_info_t *wlc)
{
	int idx;
	wlc_cmn_info_t *wlc_cmn = wlc->cmn;
	wlc_info_t *wlc_iter;
	FOREACH_WLC(wlc_cmn, idx, wlc_iter) {
		wlc_bmac_set_noreset(wlc_iter->hw, TRUE);
		wlc_radio_upd(wlc_iter);
#ifdef WLRSDB
		if (RSDB_ENAB(wlc->pub) &&
			(WLC_RSDB_SINGLE_MAC_MODE(WLC_RSDB_CURR_MODE(wlc_iter)))&&
			(wlc_iter != wlc_iter->cmn->wlc[0])) {
				break;
		}
#endif /* WLRSDB */
#ifdef WL_DUALNIC_RSDB
		wlc_down(wlc_iter);
#else
		wl_down(wlc_iter->wl);
#endif // endif
		wlc_bmac_set_noreset(wlc_iter->hw, FALSE);

		/* core clk is TRUE in BMAC driver due to noreset, need to mirror it in HIGH */
		wlc_iter->clk = TRUE;

		/* This will make sure that when 'up' is done
		 * after 'out' it'll restore hardware (especially gpios)
		 */
		wlc_iter->pub->hw_up = FALSE;
	}

	if (wlc->psm_watchdog_debug) { /* only if actual PSM watchdog but wl out iovar */
#if defined(WLC_HOSTPMAC)
		/* Mac dump for full dongle driver */
		/* triggering DHD to dump d11core */
		WL_ERROR(("wl%d: %s: send WLC_E_MACDBG_DUMPALL event\n",
			wlc->pub->unit, __FUNCTION__));
		wlc_mac_event(wlc, WLC_E_MACDBG, NULL, WLC_E_STATUS_SUCCESS,
			WLC_E_MACDBG_DUMPALL, 0, NULL, 0);
#endif /* WLC_HOSTPMAC */
		/* schedule a task to dump macreg out of dpc context */
		wl_sched_macdbg_dump(wlc->wl);
	}
}

static void
wlc_refresh_wd_timer(wlc_info_t *wlc, uint32 timeout, bool periodic)
{
	/* delete old watchdog timer */
	if (wlc->WDarmed) {
		wl_del_timer(wlc->wl, wlc->wdtimer);
	}

	/* add new watchdog timer */
	if (wlc->pub->up) {
		wl_add_timer(wlc->wl, wlc->wdtimer, timeout, periodic);
		wlc->WDarmed = TRUE;
	}
	else {
		wlc->WDarmed = FALSE;
	}
}

#ifndef ATE_BUILD
static void
wlc_watchdog_timer(void *arg)
{
	wlc_info_t *wlc = (wlc_info_t *)arg;

	wlc_watchdog(arg);
	wlc->WDlast = OSL_SYSUPTIME();

#ifdef STA
	wlc_watchdog_disable_ucode_sleep_intr(wlc);
	mboolclr(wlc->wd_state, WD_DEFERRED_PM0_BCNRX | DEFERRED_WLC_WD);
#endif /* STA */

	wlc_refresh_wd_timer(wlc, TIMER_INTERVAL_WATCHDOG, TRUE);
}

#ifdef STA
static void
wlc_watchdog_indicate_maccore_state_timer(void *arg)
{
#ifdef BCMPCIEDEV
	if (BCMPCIEDEV_ENAB()) {
		wlc_info_t *wlc = (wlc_info_t *)arg;
		if (WLC_WATCHDOG_TBTT(wlc) && !wlc_maccore_wake_state(wlc)) {
			wl_indicate_maccore_state(wlc->wl, LTR_SLEEP);
		}
	}
#endif /* BCMPCIEDEV */
}
#endif /* STA */
#endif /* ATE_BUILD */

#ifdef STA
#if defined(AP_KEEP_ALIVE)
void
wlc_ap_keep_alive_count_default(wlc_info_t *wlc)
{
	wlc->keep_alive_time = AP_KEEP_ALIVE_INTERVAL;
	wlc->keep_alive_count = wlc->keep_alive_time;
}

void
wlc_ap_keep_alive_count_update(wlc_info_t *wlc, uint16 keep_alive_time)
{
	wlc->keep_alive_time = keep_alive_time;
	wlc->keep_alive_count = MIN(wlc->keep_alive_time, wlc->keep_alive_count);
}

static void
wlc_ap_keep_alive(wlc_info_t *wlc)
{
	wlc_bsscfg_t *cfg = wlc->cfg;

	if (wlc->keep_alive_count) {
		wlc->keep_alive_count--;
	}
	if (wlc->keep_alive_count == 0) {
		/* set priority '-1' : null data frame (without QoS) */
		if (!(wlc_sendnulldata(wlc, cfg, &cfg->BSSID, 0, 0, PKTPRIO_NON_QOS_DEFAULT,
			NULL, NULL)))
			WL_INFORM(("wl%d: wlc_ccx_keep_alive: keep-alive pkt failed\n",
				wlc->pub->unit));
		/* reload count */
		wlc->keep_alive_count = wlc->keep_alive_time;
	}
}
#endif /* defined(AP_KEEP_ALIVE) */

#if defined(WL_PWRSTATS)
void
wlc_connect_time_upd(wlc_info_t *wlc)
{
#ifdef WL_EXCESS_PMWAKE
	uint32 connect_time;
	connect_time = wlc_pwrstats_connect_time_upd(wlc->pwrstats);
	if (connect_time)
		wlc_epm_roam_time_upd(wlc, connect_time);
#endif /* WL_EXCESS_PMWAKE */
}
#endif /* WL_PWRSTATS */
#endif /* STA */

/** common watchdog code */
void
wlc_watchdog(void *arg)
{
	wlc_info_t *wlc = (wlc_info_t*)arg;
	int i;
	bool send_ltr_flag = FALSE;
#if defined(STA) || defined(DWDS)
	wlc_bsscfg_t *cfg;
#endif /* STA || DWDS */
#ifdef WLCHANIM
	int err;
#endif /* WLCHANIM */
#if defined(WLTEST)
	int16 txpktpendtot = 0;
#endif // endif

#ifdef WLRSDB
	/* This helps in avoiding the watchdog execution for wlc 1 with "mpc 0" in
	* single MAC mode. With MPC 1, we may not enter watchdog for wlc 1
	* after moving to MIMO/80p80 operation
	*/
	if (RSDB_ENAB(wlc->pub) && (wlc != wlc->cmn->wlc[0]) &&
		(WLC_RSDB_SINGLE_MAC_MODE(WLC_RSDB_CURR_MODE(wlc)))) {
		return;
	}
#endif /* WLRSDB */

	WL_TRACE(("wl%d: wlc_watchdog\n", wlc->pub->unit));

	if (!wlc->pub->up) {
		return;
	}
	if (wlc->wd_block_req != 0) {
		return;
	}
	if (DEVICEREMOVED(wlc)) {
		WL_ERROR(("wl%d: %s: dead chip\n", wlc->pub->unit, __FUNCTION__));
#ifdef NEED_HARD_RESET
		/* Perform hard reset and reinit */
		wlc_bmac_report_fatal_errors(wlc->hw, WL_REINIT_RC_DEVICE_REMOVED);
#else
		wl_down(wlc->wl);
#endif /* NEED_HARD_RESET */
		return;
	}

#if defined(WLTEST)
	for (i = 0; i < WLC_HW_NFIFO_TOTAL(wlc); i++) {
		if (i < TX_BCMC_FIFO ||
			(BCM_DMA_CT_ENAB(wlc) && i >= TX_FIFO_EXT_START)) {
			txpktpendtot += TXPKTPENDGET(wlc, i);
		}
	}
	/* Validate the pre-calculated txpktpendtot */
	ASSERT(txpktpendtot == TXPKTPENDTOT(wlc));

	if (wlc->watchdog_disable) {
		return;
	}
#endif // endif

	WLDURATION_ENTER(wlc, DUR_WATCHDOG);

#ifdef BCM_BACKPLANE_TIMEOUT
	/* Poll and clear any back plane timeouts */
	if (si_clear_backplane_to(wlc->pub->sih)) {
		/* Detected back plane timeout, initiate the fatal error recovery */
		wlc_bmac_report_fatal_errors(wlc->hw, WL_REINIT_RC_AXI_BUS_ERROR);
	}

#endif /* BCM_BACKPLANE_TIMEOUT */

#ifdef WAR4360_UCODE
	if (wlc->hw->need_reinit) {
		WLC_FATAL_ERROR(wlc);	/* big hammer */
	}

	if (R_REG(wlc->osh, D11_SubrStkStatus(wlc)) & 0x40) {
		WL_ERROR(("wl%d:%s: ucode stack error, Big Hammer\n",
			wlc->pub->unit, __FUNCTION__));
		WLC_FATAL_ERROR(wlc);	/* big hammer */
	}
#endif /* WAR4360_UCODE */
#ifndef WLC_DISABLE_ACI
	wlc_weakest_link_rssi_chan_stats_upd(wlc);
#endif // endif

	if (wlc->blocked_for_slowcal &&
	    !(R_REG(wlc->osh, D11_MACCOMMAND(wlc)) & MCMD_SLOWCAL)) {
		wlc_suspend_mac_and_wait(wlc);
		wlc->blocked_for_slowcal = FALSE;
		phy_rxspur_change_block_bbpll(WLC_PI(wlc), FALSE, FALSE);
		wlc_enable_mac(wlc);
	}

	/* increment second count */
	wlc->pub->now++;

#ifdef ROUTER_TINY
	if (pktq_full(WLC_GET_CQ(wlc->active_queue))) {
		WL_ERROR(("wl%d: %s: max txq full. HAMMERING!\n",
			wlc->pub->unit, __FUNCTION__));
		wlc_fatal_error(wlc);
		goto exit;
	}
#endif // endif
#ifdef DNGL_WD_KEEP_ALIVE
	if (wlc->dngl_wd_keep_alive) {
		wlc_bmac_dngl_wd_keep_alive(wlc->hw, TIMER_INTERVAL_DNGL_WATCHDOG);
	}
#endif // endif

#if defined(WLATF_DONGLE)
	if (ATFD_ENAB(wlc)) {
		struct scb *scb;
		struct scb_iter scbiter;
		FOREACHSCB(wlc->scbstate, &scbiter, scb) {
			if (!SCB_INTERNAL(scb) &&
				((SCB_ASSOCIATED(scb) && SCB_AUTHENTICATED(scb)) || SCB_WDS(scb))) {
				wlc_scb_upd_all_flr_weight(wlc, scb);
			}
		}
	}
#endif /* WLATF_DONGLE */

#if defined(STA)
#ifdef WL_EXCESS_PMWAKE
	if (!AP_ACTIVE(wlc) && (wlc->excess_pm_period || wlc->excess_pmwake->ca_thresh) &&
	    ((wlc->cfg->pm->PM && wlc->stas_associated) ||
	     0))
		wlc_check_excess_pm_awake(wlc);
#endif /* WL_EXCESS_PMWAKE */

#ifdef DEBUG_TBTT
	{
	uint32 tsf_l, tsf_h;

	/* read the tsf from our chip */
	wlc_read_tsf(wlc, &tsf_l, &tsf_h);

	WL_ASSOC(("wl%d: TBTT: wlc_watchdog: tsf time: %08x:%08x\n", WLCWLUNIT(wlc), tsf_h, tsf_l));
	}
#endif /* DEBUG_TBTT */
#endif /* STA */

#if defined(BCMECICOEX)
	/*
	* These BTCX parameters need to be read out from SHM before mac suspend
	* (which might be called in one or more of the watchdog routines). This is because
	* resuming after a mac suspend causes some of the SHM parameters to be reset
	*/
	wlc_btcx_read_btc_params(wlc);
#endif /* defined(BCMECICOEX) */
	wlc_bmac_watchdog(wlc);

#if defined(STA) && defined(AP)
	/* start AP if operation were pending on SCAN_IN_PROGRESS() or WLC_RM_IN_PROGRESS() */
	/* Find AP's that are enabled but not up to restart */
	if (AP_ENAB(wlc->pub) && APSTA_ENAB(wlc->pub) && wlc_apup_allowed(wlc)) {
		/* For DWDS and psta scb, defer the up operation of ap interface till
		 * scb is not up and associated.
		 */
		if (BSSCFG_STA(wlc->cfg) && BSSCFG_IS_PRIMARY(wlc->cfg) &&
			(MAP_ENAB(wlc->cfg) || DWDS_ENAB(wlc->cfg) ||
#if defined(WET) || defined(WET_DONGLE)
			(WET_ENAB(wlc) || WET_DONGLE_ENAB(wlc)) ||
#endif /* WET || WET_DONGLE */
			(wlc->cfg->_psta))) {

			if ((wlc->cfg->up) && (wlc->cfg->associated)) {
				wlc_apsta_restart_ap(wlc);
			} else {
				/* If no SCAN, ASSOC or RM in progress and primary sta interface
				 * is down or not associated, initiate join process
				 * wlc_apup_allowed routine checks about scan, assoc or RM process
				 * in progress or not
				 */
				/* Wrapper function to call static function wlc_join_start, to
				 * avoid rom abandon issue for 4366.
				 * For Merging to Trunk, direcly can call wlc_join_start
				 */
				if (wlc_assoc_get_as_state(wlc->cfg) == AS_DFS_CAC_START) {
					/* Wait. Sta is performing DFS re-entry CAC and if
					 * successful, will join upstream AP with assoc sta
					 * state machine switch to DFS_ISM_INIT. In case radar
					 * detected during DFS re-entry CAC, assoc sta state
					 * machine switch to AS_DFS_CAC_FAIL, followed with
					 * resume join logic again starting with SCAN state
					 * with wlc_try_join_start.
					 */
					;
				} else {
					wlc_try_join_start(wlc->cfg,
						wlc_bsscfg_scan_params(wlc->cfg),
						wlc_bsscfg_assoc_params(wlc->cfg));
				}
			}
		} else {
			wlc_apsta_restart_ap(wlc);
		}
	}
#endif /* STA && AP */

#if defined(DELTASTATS)
	/* check if delta stats enabled */
	if (DELTASTATS_ENAB(wlc->pub) && (wlc->delta_stats->interval != 0)) {
		/* update mac stats counters at every watchdog */
		wlc_statsupd(wlc);

		/* check if interval has elapsed */
		if (wlc->delta_stats->seconds % wlc->delta_stats->interval == 0) {
			wlc_delta_stats_update(wlc);
		}

		/* seconds is zeroed only when delta stats is enabled so
		 * that it can be used to determine when delta stats are valid
		 */
		wlc->delta_stats->seconds++;
	} else
#endif /* DELTASTATS */
	if ((WLC_UPDATE_STATS(wlc)) && (!(wlc->pub->now % SW_TIMER_MAC_STAT_UPD))) {
		/* occasionally sample mac stat counters to detect 16-bit counter wrap */
		wlc_statsupd(wlc);
	}
#ifdef WLCHANIM
	if (WLC_CHANIM_ENAB(wlc->pub)) {
		if ((err = wlc_lq_chanim_update(wlc, wlc->chanspec, CHANIM_WD)) != BCME_OK) {
			WL_TRACE(("wl%d: %s: WLC_CHANIM upd fail %d\n", wlc->pub->unit,
				__FUNCTION__, err));
		}
	}
#endif /* WLCHANIM */

#ifdef STA
	/* Ensure that the PM state is in sync. between the STA and AP */
	FOREACH_AS_STA(wlc, i, cfg) {
		if (cfg->pm->PMpending && PS_ALLOWED(cfg)) {
			if (!cfg->BSS ||
			    !cfg->pm->PMpending ||
			    !WLC_BSS_CONNECTED(cfg))
				continue;
			WL_RTDC(wlc, "wlc_watchdog: tx PMep=%02u AW=%02u",
				(cfg->pm->PMenabled ? 10 : 0) | cfg->pm->PMpending,
				(PS_ALLOWED(cfg) ? 10 : 0) | STAY_AWAKE(wlc));
			WL_RTDC(wlc, "            : PMb=0x%x", cfg->pm->PMblocked, 0);
			/* send another NULL data frame to communicate PM state */
			if (!(wlc_sendnulldata(wlc, cfg, &cfg->BSSID, 0, 0, PKTPRIO_NON_QOS_HIGH,
				NULL, NULL)))
				WL_ERROR(("wl%d: %s: PM state pkt failed\n",
				          wlc->pub->unit, __FUNCTION__));
		}
	}

	wlc_link_monitor_watchdog(wlc);

#ifdef SCAN_JOIN_TIMEOUT
	if (wlc->scan_timeout == 1) {

		wlc_bmac_report_fatal_errors(wlc->hw, WL_REINIT_RC_SCAN_TIMEOUT);
		if (wlc->scan_timeout != 0) {
			wlc->scan_timeout --;
		}
	}
	if (wlc->join_timeout == 1) {
		wlc_bmac_report_fatal_errors(wlc->hw, WL_REINIT_RC_SCAN_TIMEOUT);
		if (wlc->join_timeout != 0) {
			wlc->join_timeout --;
		}
	}
#endif // endif

	/* periodic IBSS gmode rate probing */
	if (wlc->band->gmode && (wlc->pub->now % SW_TIMER_IBSS_GMODE_RATEPROBE) == 0) {
		FOREACH_AS_STA(wlc, i, cfg) {
			if (cfg->BSS)
				continue;
			wlc_rateprobe_scan(cfg);
		}
	}

#if defined(AP_KEEP_ALIVE)
	cfg = wlc->cfg;
	if (BSSCFG_STA(cfg) && cfg->BSS) {
		if (cfg->associated && !ETHER_ISNULLADDR(&cfg->BSSID)) {
			wlc_ap_keep_alive(wlc);
		}
	}

#endif /* AP_KEEP_ALIVE */

	if (wlc->reset_triggered_pmoff) {
		FOREACH_AS_STA(wlc, i, cfg) {
			if (cfg->up) {
				/* resync pm mode */
				wlc_set_pm_mode(wlc, cfg->pm->PM, cfg);
				WL_PS(("wl%d.%d: resync PM mode to %d\n", wlc->pub->unit,
				       WLC_BSSCFG_IDX(cfg), cfg->pm->PM != PM_OFF));
			}
		}
	}
#endif /* STA */

#ifdef DWDS
	/* Clean up DWDS STA client list */
	FOREACH_AS_STA(wlc, i, cfg) {
		if (MAP_ENAB(cfg) && cfg->dwds_loopback_filter) {
			wlc_dwds_expire_sa(wlc, cfg);
		}
	}
#endif /* DWDS */
	/* push assoc state to phy. If no associations then set the flag so
	 * that phy can clear any desense it has done before.
	 */
	if (wlc_stas_active(wlc) || wlc_ap_stas_associated(wlc->ap) ||
#ifdef WDS
		wlc_wds_is_active(wlc) ||
#endif /* WDS */
		FALSE) {
		wlc_phy_hold_upd(WLC_PI(wlc), PHY_HOLD_FOR_NOT_ASSOC, FALSE);
	} else {
		wlc_phy_hold_upd(WLC_PI(wlc), PHY_HOLD_FOR_NOT_ASSOC, TRUE);
	}

	if (wlc->pm_dur_clear_timeout)
		wlc->pm_dur_clear_timeout--;
	else
		wlc_get_accum_pmdur(wlc);

	/* Call any registered watchdog handlers */
	for (i = 0; i < wlc->pub->max_modules; i ++) {
		if (wlc->modulecb[i].watchdog_fn == NULL)
			continue;
		WL_TRACE(("wl%d: %s: calling WATCHDOG callback for \"%s\"\n",
			wlc->pub->unit, __FUNCTION__, wlc->modulecb[i].name));
		wlc->modulecb[i].watchdog_fn(wlc->modulecb_data[i].hdl);
	}

#ifdef STA
	/* clear reset variable */
	wlc->reset_triggered_pmoff = FALSE;
#endif /* STA */

	ASSERT(wlc_bmac_taclear(wlc->hw, TRUE));

	/* Verify that txq stopped conditions and pkt counts are in sync to avoid lock ups */
	ASSERT(wlc_txq_fc_verify(wlc->txqi, wlc->active_queue->low_txq));

	ASSERT(wlc_ps_check(wlc));
#ifdef GPIO_TXINHIBIT
	/* If MAC up, update SHM tx_inhibit_tout from FW value */
	wlc_bmac_gpio_set_tx_inhibit_tout(wlc->hw);
#endif // endif

	/* XXX: with the PCIE full dongle model, dongle comes up in LTR_SLEEP mode.
	 * If the user doesn't change the PM mode, we will be stuck with that state.
	 * Currently LTR_ACTIVE is set in below scenarios
	 *  1.  Any AP is up  (wlc->aps_associated) or
	 *  2.  Any associated STA is in PM_OFF state.
	 */
	if (wlc->aps_associated) {
		send_ltr_flag = TRUE;
	}
#ifdef STA
	else  {
		FOREACH_AS_STA(wlc, i, cfg) {
			if (cfg->pm->PM == PM_OFF) {
				send_ltr_flag = TRUE;
				break;
			}
		}
	}
#endif /* STA */

	wlc_scb_cleanup_unused(wlc);
	if (send_ltr_flag)
		wl_indicate_maccore_state(wlc->wl, LTR_ACTIVE);

	/* cache R_REG(tsf) periodically to avoid the per packet latency in wlc_lifetime_set() */
	wlc_lifetime_cache_upd(wlc);

#if defined(WL_TX_STALL)
	/* Run TX stall health check */
	if (!WL_HEALTH_CHECK_ENAB(wlc->pub->cmn)) {
		wlc_tx_status_health_check(wlc);
	}
#endif // endif

#ifdef WL_TXQ_STALL
	wlc_txq_health_check(wlc);
#endif // endif

#ifdef STA
	/* delay radio disable */
	if (wlc->mpc && wlc_is_non_delay_mpc(wlc)) {
		if (wlc->mpc_delay_off)
			wlc->mpc_delay_off--;
	}

	WL_MPC(("wlc_watchdog: wlc->mpc_delay_off %d\n", wlc->mpc_delay_off));

	/* mpc sync */
	wlc_radio_mpc_upd(wlc);
#endif /* STA */

#if defined(WL_RX_STALL)
	if (!WL_HEALTH_CHECK_ENAB(wlc->pub->cmn)) {
		wlc_rx_healthcheck(NULL, 0, (void*)wlc, NULL);
	}
#endif // endif

	/* radio sync: sw/hw/mpc --> radio_disable/radio_enable */
	wlc_radio_hwdisable_upd(wlc);
	wlc_radio_upd(wlc);
#if defined(STA)
	/* if ismpc, driver should be in down state if up/down is allowed */
	if (wlc->mpc && wlc_ismpc(wlc))
		ASSERT(!wlc->pub->up);
#endif // endif
	if (HND_DS_HC_ENAB()) {
		wl_health_check_notify(wlc->wl, HEALTH_CHECK_WL_STAY_AWAKE, STAY_AWAKE(wlc));
		if (wlc->pub->up && wlc->clk) {
			wl_health_check_log(wlc->wl, HEALTH_CHECK_WL_CLK_LOG,
				R_REG(wlc->osh, D11_ClockCtlStatus(wlc)),
				R_REG(wlc->osh, D11_MACCONTROL(wlc)));
		}
	}

#if defined(WLATF) && defined(WLATF_PERC)
	if (wlc->atm_perc) {
		wlc_atm_update_perc(wlc);
	}
#endif // endif

	/* Please dont add anything here (always mpc stuff should be end) */

#ifdef ROUTER_TINY
exit:
#endif // endif
	WLDURATION_EXIT(wlc, DUR_WATCHDOG);
	return;
} /* wlc_watchdog */

#if defined(WLATF) && defined(WLATF_PERC)
void wlc_atm_update_perc(wlc_info_t *wlc)
{
	wlc_bsscfg_t *cfg;
	uint8 staperc_sum, sta_num, sta_auto;
	uint8 bssperc_sum, bss_num, bss_auto;
	struct scb *scb;
	struct scb_iter scbiter;
	int i;
	bssperc_sum = bss_num = bss_auto = 0;

	FOREACH_UP_AP(wlc, i, cfg) {
		bssperc_sum += cfg->bssperc;
		if (!cfg->bssperc)
			bss_auto ++;
		else
			bss_num ++;
	}

	FOREACH_UP_AP(wlc, i, cfg) {
		staperc_sum = sta_num = sta_auto = 0;
		FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {
			if (!SCB_INTERNAL(scb) &&
				SCB_ASSOCIATED(scb) &&
				SCB_AUTHENTICATED(scb)) {
				staperc_sum += scb->staperc;
				if (!scb->staperc)
					sta_auto ++;
				else
					sta_num ++;
			}
		}
		ASSERT(staperc_sum <= 100);

		FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {
			scb->sched_staperc = wlc_atm_cal_perc(wlc, staperc_sum, sta_num,
				sta_auto, (uint8)scb->staperc);
			scb->bsscfg->sched_bssperc = wlc_atm_cal_perc(wlc, bssperc_sum,
				bss_num, bss_auto, scb->bsscfg->bssperc);
			scb->sched_staperc = (scb->bsscfg->sched_bssperc * scb->sched_staperc)/100;
		}
	}
}
#endif /* WLATF && WLATF_PERC */

/** make interface operational */
int
BCMINITFN(wlc_up)(wlc_info_t *wlc)
{
	WL_TRACE(("wl%d: %s:\n", wlc->pub->unit, __FUNCTION__));

	/* HW is turned off so don't try to access it */
	if (wlc->pub->hw_off || DEVICEREMOVED(wlc))
		return BCME_RADIOOFF;

	if (SRPWR_ENAB()) {
		wlc_srpwr_request_on(wlc);
	}

	/* Reset reinit reason code */
	wlc->hw->need_reinit = WL_REINIT_RC_NONE;
	si_set_device_removed(wlc->pub->sih, FALSE);

#ifdef WLRSDB
	if (RSDB_ENAB(wlc->pub) &&
		(WLC_RSDB_SINGLE_MAC_MODE(WLC_RSDB_CURR_MODE(wlc))) &&
		(wlc != wlc->cmn->wlc[MAC_CORE_UNIT_0])) {
		return BCME_RADIOOFF;
	} else if (RSDB_ENAB(wlc->pub) &&
		(WLC_RSDB_SINGLE_MAC_MODE(WLC_RSDB_CURR_MODE(wlc)))) {
		/* Call down once for core 1 since after attach time */
		wlc_down(wlc->cmn->wlc[MAC_CORE_UNIT_1]);
	}
#endif /* WLRSDB */

	wlc->state = WLC_STATE_GOING_UP;

	if (!wlc->pub->hw_up) {
#ifdef WLLED
		/* Do basic GPIO initializations for LED. This just sets up LED mask */
		wlc_led_init(wlc->ledh);
#endif // endif
		wlc_bmac_hw_up(wlc->hw);
		wlc->pub->hw_up = TRUE;

		/* Initialize LTR and set latency to idle */
#ifdef WL_LTR
		if (LTR_ENAB(wlc->pub)) {
			wlc_ltr_up_prep(wlc, LTR_SLEEP);
		}
#endif /* WL_LTR */
	}

	{
	/* disable/enable RSSI power down feature */
	uint16 flags = wlc->stf->rssi_pwrdn_disable ? MHF5_HTPHY_RSSI_PWRDN : 0;
	wlc_mhf(wlc, MHF5, MHF5_HTPHY_RSSI_PWRDN, flags, WLC_BAND_ALL);
	}

	/*
	 * Need to read the hwradio status here to cover the case where the system
	 * is loaded with the hw radio disabled. We do not want to bring the driver up in this case.
	 * if radio is disabled, abort up, lower power, start radio timer and return 0(for NDIS)
	 * don't call radio_update to avoid looping wlc_up.
	 *
	 * wlc_bmac_up_prep() returns either 0 or BCME_RADIOOFF only
	 */
	if (!wlc->pub->radio_disabled) {
		int status = wlc_bmac_up_prep(wlc->hw);
		if (status == BCME_RADIOOFF) {
			if (!mboolisset(wlc->pub->radio_disabled, WL_RADIO_HW_DISABLE)) {
				int idx;
				wlc_bsscfg_t *bsscfg;

				mboolset(wlc->pub->radio_disabled, WL_RADIO_HW_DISABLE);

				FOREACH_BSS(wlc, idx, bsscfg) {
					/* all bsscfg including AP's for Win7 */
					if (!BSSCFG_STA(bsscfg) || !bsscfg->enable || !bsscfg->BSS)
						continue;
					WL_ERROR(("wl%d.%d: wlc_up: rfdisable -> "
						"wlc_bsscfg_disable()\n",
						wlc->pub->unit, idx));
					wlc_bsscfg_disable(wlc, bsscfg);
				}
			}
		} else
			ASSERT(!status);
	}

#if defined(SAVERESTORE) && defined(MACOSX)
	if (wlc_bmac_radio_read_hwdisabled(wlc->hw))
#else
	if (wlc->pub->radio_disabled)
#endif // endif
	{
		wlc_radio_monitor_start(wlc);
		wlc->state = WLC_STATE_DOWN;
		return 0;
	}

	if (wlc->hw->btswitch_ovrd_state != AUTO) {
		wlc_bmac_set_btswitch_ext(wlc->hw, wlc->hw->btswitch_ovrd_state);
	}

	/* wlc_bmac_up_prep has done wlc_corereset(). so clk is on, set it */
	wlc->clk = TRUE;

#ifdef WLCFP
	/* CFP has dependency on HW clock. Update the states now */
	wlc_cfp_state_update(wlc->cfp);
#endif /* WLCFP */

	wlc_radio_monitor_stop(wlc);

#if defined(BCMDBG) && !defined(BCMDBG_EXCLUDE_HW_TIMESTAMP)
	if (wlc_info_time_dbg == NULL) {
		wlc_info_time_dbg = wlc;
	}
#endif /* BCMDBG && !BCMDBG_EXCLUDE_HW_TIMESTAMP */

	wl_init(wlc->wl); /* Calls wl_reset and wlc_init. Loads ucode + takes it out of suspend. */
	wlc->pub->up = TRUE;

#ifndef ATE_BUILD
	/* watchdog must not be running at this point */
	ASSERT(!wlc->WDarmed);
#endif /* ATE_BUILD */

#if defined(WLRSDB) && defined(BCMECICOEX)
	if (WLC_DUALMAC_RSDB(wlc->cmn)) {
		/* 4364: Update COEX GCI mask on wlc_up path */
		wlc_btcx_update_gci_mask(wlc);
	}
#endif /* WLRSDB && BCMECICOEX */

	/* Do bmac up first before doing band set
	 * Because wlc_bmac_setband is blocked by wlc->pub->hwup
	 */
	wlc_bmac_up_finish(wlc->hw); /* sets the hw->up flag, (re)enables d11 interrupts */

	if (wlc->bandinit_pending) {
		wlc_suspend_mac_and_wait(wlc);
		wlc_set_chanspec(wlc, wlc->default_bss->chanspec, CHANSW_REASON(CHANSW_INIT));
		wlc->bandinit_pending = FALSE;
		wlc_enable_mac(wlc);
	}
#ifdef WL_SCAN_DFS_HOME
	wlc->scan->prev_scan = 0;
#endif /* WL_SCAN_DFS_HOME */
	/* Ser the default mode of probe req reception to the host */
	wlc_enable_probe_req(wlc, PROBE_REQ_EVT_MASK, wlc->prb_req_enable_mask);
	wlc_config_ucode_probe_resp(wlc);

	/* Propagate rfaware_lifetime setting to ucode */
	wlc_rfaware_lifetime_set(wlc, wlc->rfaware_lifetime);

	/* Enable BAck 20in80 for rssi -78dBm and below */
	wlc_mhf(wlc, MHF2, MHF2_RSPBW20, MHF2_RSPBW20, WLC_BAND_ALL);

	/* other software states up after ISR is running */
	/* start APs that were to be brought up but are not up  yet */
	if (AP_ENAB(wlc->pub))
		wlc_restart_ap(wlc->ap);

#ifdef STA
	if (ASSOC_RECREATE_ENAB(wlc->pub)) {
		int idx;
		wlc_bsscfg_t *cfg;
		FOREACH_BSS(wlc, idx, cfg) {
			if (BSSCFG_STA(cfg) && cfg->enable && (cfg->flags & WLC_BSSCFG_PRESERVE)) {
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
				char ssidbuf[SSID_FMT_BUF_LEN];
				wlc_format_ssid(ssidbuf, cfg->SSID, cfg->SSID_len);
#endif // endif
				WL_ASSOC(("wl%d: wlc_up: restarting STA bsscfg 0 \"%s\"\n",
				          wlc->pub->unit, ssidbuf));
				wlc_join_recreate(wlc, cfg);
			}
		}
	}
#endif /* STA */

	/* TODO: investigate why this call can't be moved to wlc_qos_wlc_up
	 * which was called earlier in the almost end of wlc_init fn...
	 */
	/* Program the TX wme params with the current settings */
	wlc_wme_retries_write(wlc);

	/* sanitize any existing scb rates */
	wlc_scblist_validaterates(wlc);

#ifndef ATE_BUILD
	if (!wlc->WDarmed) {
		/* start one second watchdog timer only if not already running */
		wlc_refresh_wd_timer(wlc, TIMER_INTERVAL_WATCHDOG, TRUE);
	}
#endif // endif

	/* ensure txc is up to date */
	if (WLC_TXC_ENAB(wlc))
		wlc_txc_upd(wlc->txc);

	/* ensure antenna config is up to date */
	wlc_stf_phy_txant_upd(wlc);

#ifdef WL11AC
	/* Updated duty cycle for band 40 and 80 */
	wlc_stf_chanspec_upd(wlc);
#endif /* WL11AC */
#ifdef WL_GLOBAL_RCLASS
	wlc_channel_set_cur_rclass(wlc, BCMWIFI_RCLASS_TYPE_GBL);
#endif /* WL_GLOBAL_RCLASS */
	/* ensure the brcm ie is up to date */
	wlc_update_brcm_ie(wlc);
	WL_APSTA_BCN(("wl%d: wlc_up() -> wlc_update_beacon()\n", wlc->pub->unit));
	wlc_update_beacon(wlc);
	wlc_update_probe_resp(wlc, TRUE);

#ifdef WLLED
	/* start led timer module */
	wlc_led_up(wlc);
	wlc_led_event(wlc->ledh);
#endif // endif

	/* toe enabled ? */
	wlc->toe_bypass = TRUE;
#ifdef WLTOEHW
	if (wlc->toe_capable) {
		uint32 toe_ctl;

		toe_ctl = R_REG(wlc->osh, D11_ToECTL(wlc));

#if defined(WLCSO) || defined(WLAMPDU)
		/* WLAMPDU needs TOE header for the Epoch bit. Header cache also need it (?) */
		/* TOE engine needs to be enabled for CSO, TSO, or AMPDU for d11 rev40 */
		if ((toe_ctl & 1) != 0) {
			toe_ctl &= 0xFFFE;
			W_REG(wlc->osh, D11_ToECTL(wlc), toe_ctl);
		}
#else
		/* TOE engine can be disabled if not needed */
		if ((toe_ctl & 1) == 0) {
			toe_ctl |= 1;
			W_REG(wlc->osh, D11_ToECTL(wlc), toe_ctl);
		}
#endif /* (WLCSO || WLAMPDU) */

		if (toe_ctl & 1)
			wlc->toe_bypass = TRUE;
		else
			wlc->toe_bypass = FALSE;
	}
#endif /* WLTOEHW */

	/* In case of rsdb chip it is required to have this operated
	* only in core-0 for once during wl up.
	*/
	if (((BCM4347_CHIP(wlc->pub->sih->chip)) ||
	     (BCM6878_CHIP(wlc->pub->sih->chip)) ||
	     (BCM4369_CHIP(wlc->pub->sih->chip)))) {
		/* Initiate the slow calibration */
#ifndef ATE_BUILD
		OR_REG(wlc->osh, D11_MACCOMMAND(wlc), MCMD_SLOWCAL);
#endif /* ATE_BUILD */
	}

	/* sync up btc mode between high and low driver */
	wlc_btc_mode_sync(wlc);

	/* wl_init() or other wlc_up() code may have set latency to active,
	 * set it back to idle
	 */
#ifdef WL_LTR
	if (LTR_ENAB(wlc->pub)) {
		wlc_ltr_hwset(wlc->hw, wlc->regs, LTR_SLEEP);
	}
#endif /* WL_LTR */
#ifdef BCMPCIEDEV
	if (BCMPCIEDEV_ENAB()) {
		wlc_bmac_enable_rx_hostmem_access(wlc->hw, wlc->cmn->hostmem_access_enabled);
	}
#endif /* BCMPCIEDEV */

	wlc_lifetime_cache_upd(wlc);

	WL_MPC(("wl%d: UP\n", wlc->pub->unit));

#ifdef BCMULP
	/* recreating the assoc after wlc up */
	if (BCMULP_ENAB()) {
		if (si_is_warmboot())
			if (wlc_ulp_assoc_recreate(wlc->ulp))
				return BCME_ERROR;
	}
#endif // endif

#ifdef BCM_CEVENT
	wlc_if_event(wlc, WLC_E_IF_BSSCFG_UP, wlc->cfg->wlcif);
#endif /* BCM_CEVENT */

	wlc->state = WLC_STATE_UP;

	return (0);
} /* wlc_up */

/** Initialize the base precedence map for dequeueing from txq based on WME settings */
static void
BCMINITFN(wlc_tx_prec_map_init)(wlc_info_t *wlc)
{
	bzero(wlc->fifo2prec_map, sizeof(wlc->fifo2prec_map));

	/* For non-WME, both fifos have overlapping MAXPRIO. So just disable all precedences
	 * if either is full.
	 */
	if (!WME_ENAB(wlc->pub)) {
		wlc->fifo2prec_map[TX_DATA_FIFO] = WLC_PREC_BMP_ALL;
		wlc->fifo2prec_map[TX_CTL_FIFO] = WLC_PREC_BMP_ALL;
	} else {
		wlc->fifo2prec_map[TX_AC_BK_FIFO] = WLC_PREC_BMP_AC_BK;
		wlc->fifo2prec_map[TX_AC_BE_FIFO] = WLC_PREC_BMP_AC_BE;
		wlc->fifo2prec_map[TX_AC_VI_FIFO] = WLC_PREC_BMP_AC_VI;
		wlc->fifo2prec_map[TX_AC_VO_FIFO] = WLC_PREC_BMP_AC_VO;
	}
}

static uint
BCMUNINITFN(wlc_down_del_timer)(wlc_info_t *wlc)
{
	uint callbacks = 0;

#ifdef STA
	/* cancel any radio_shutoff_dly timer */
	if (!wl_del_timer(wlc->wl, wlc->pm2_radio_shutoff_dly_timer))
		callbacks ++;

#endif /* STA */

	return callbacks;
}

/**
 * Mark the interface nonoperational, stop the software mechanisms,
 * disable the hardware, free any transient buffer state.
 * Return a count of the number of driver callbacks still pending.
 */
uint
BCMUNINITFN(wlc_down)(wlc_info_t *wlc)
{

	uint callbacks = 0;
	int i;
	bool dev_gone = FALSE;
	wlc_bsscfg_t *bsscfg;
	wlc_txq_info_t *qi;

	WL_TRACE(("wl%d: %s:\n", wlc->pub->unit, __FUNCTION__));

	/* check if we are already in the going down path */
	if (wlc->state == WLC_STATE_GOING_DOWN) {
		WL_ERROR(("wl%d: %s: Driver going down so return\n", wlc->pub->unit, __FUNCTION__));
		return 0;
	}
	if (wlc->cmn->reinit_active)
		return 0;
	if (!wlc->pub->up)
		return 0;

	/* Update the pm_dur value before going down */
	wlc_get_accum_pmdur(wlc);

#if defined(WLTDLS)
	if (TDLS_ENAB(wlc->pub)) {
		wlc_tdls_down(wlc->tdls);
		OSL_DELAY(1000);
	}
#endif /* defined(WLTDLS) */

	/* in between, mpc could try to bring down again.. */
	wlc->state = WLC_STATE_GOING_DOWN;

#ifdef DNGL_WD_KEEP_ALIVE
	/* disable dngl watchdog timer while going down */
	wlc_bmac_dngl_wd_keep_alive(wlc->hw, 0);
#endif // endif

#ifdef NEED_HARD_RESET
	/* NEED_HARD_RESET will power cycle and reconnect.
	 * Need to uninitialize all SW state during wlc_down().
	 */
	/* It looks like device gone validation is added to avoid touching HW
	 * registers, this is taken care by updating specific code accessing the HW
	 */
	if (wl_powercycle_inprogress(wlc->wl)) {
		dev_gone = FALSE;
	}
	else
#endif /* NEED_HARD_RESET */
	{
		dev_gone = DEVICEREMOVED(wlc);
	}
	if (!dev_gone) {
		/* abort any scan in progress */
		if (SCAN_IN_PROGRESS(wlc->scan)) {
			wlc_scan_abort(wlc->scan, WLC_E_STATUS_ABORT);
		}

		if (wlc->default_bss->chanspec != wlc->home_chanspec) {
			wlc_suspend_mac_and_wait(wlc);
			wlc_set_chanspec(wlc, wlc->home_chanspec, CHANSW_REASON(CHANSW_IOVAR));
			wlc_enable_mac(wlc);
		}

		FOREACH_BSS(wlc, i, bsscfg) {
			/* down any AP BSSs, but leave them enabled */
			if (BSSCFG_AP(bsscfg)) {
				if (bsscfg->up) {
					WL_APSTA_UPDN(("wl%d: wlc_down -> wlc_bsscfg_down %p "
						"(%d)\n", wlc->pub->unit,
						OSL_OBFUSCATE_BUF(bsscfg), i));
					callbacks += wlc_bsscfg_down(wlc, bsscfg);
				}
				continue;
			}

#ifdef STA
			/* Perform STA down operations if needed */

			/* abort any association in progress */
			callbacks += wlc_assoc_abort(bsscfg);

			/* if config is enabled, take care of deactivating */
			if (!bsscfg->enable)
				continue;

#ifdef DWDS
			/* Flush DWDS loopback sa list */
			if (MAP_ENAB(bsscfg) && bsscfg->dwds_loopback_filter) {
				wlc_dwds_flush_salist(wlc, bsscfg);
			}
#endif /* DWDS */
			/* For WOWL or Assoc Recreate, don't disassociate,
			 * just down the bsscfg.
			 * Otherwise, disable the config (STA requires active restart)
			 */
			/* WOWL_ACTIVE does not mean association needs to be recreated!
			 * ASSOC_RECREATE_ENAB is meant for that
			 */
			if (
#ifdef WOWL
			    WOWL_ACTIVE(wlc->pub) ||
#endif // endif
			    (ASSOC_RECREATE_ENAB(wlc->pub) &&
			     (bsscfg->flags & WLC_BSSCFG_PRESERVE))) {
				WL_APSTA_UPDN(("wl%d: %s: wlc_bsscfg_down(STA)\n",
				               wlc->pub->unit, __FUNCTION__));

				callbacks += wlc_bsscfg_down(wlc, bsscfg);

				/* reset the quiet channels vector to the union
				 * of the restricted and radar channel sets
				 */
				wlc_quiet_channels_reset(wlc->cmi);
			} else {
				WL_APSTA_UPDN(("wl%d: %s: wlc_bsscfg_disable(STA)\n",
				               wlc->pub->unit, __FUNCTION__));

				callbacks += wlc_bsscfg_disable(wlc, bsscfg);

				/* allow time for disassociation packet to
				 * notify associated AP of our departure
				 */
				OSL_DELAY(4 * 1000);
			}
#endif /* STA */
		}
	}

	callbacks += wlc_bmac_down_prep(wlc->hw);

	/* Call any registered down handlers */
	for (i = 0; i < wlc->pub->max_modules; i ++) {
		if (wlc->modulecb[i].down_fn == NULL)
			continue;
		WL_INFORM(("wl%d: %s: calling DOWN callback for \"%s\"\n",
		           wlc->pub->unit, __FUNCTION__, wlc->modulecb[i].name));
		callbacks += wlc->modulecb[i].down_fn(wlc->modulecb_data[i].hdl);
	}

	/* cancel the watchdog timer */
	if (wlc->WDarmed) {
		if (!wl_del_timer(wlc->wl, wlc->wdtimer))
			callbacks++;
		wlc->WDarmed = FALSE;
	}
	/* cancel all other timers */
	callbacks += wlc_down_del_timer(wlc);

	/* interrupt must have been blocked */
	ASSERT((wlc_hw_get_macintmask(wlc->hw) == 0) || !wlc->pub->up);

	wlc->pub->up = FALSE;

	wlc_phy_mute_upd(WLC_PI(wlc), FALSE, PHY_MUTE_ALL);

#ifdef WLRXOV
	if (WLRXOV_ENAB(wlc->pub) && wlc->rxov_timer) {
		if (!wl_del_timer(wlc->wl, wlc->rxov_timer))
			callbacks++;
	}
#endif // endif

	wlc_monitor_down(wlc);

#ifdef WLLED
	/* this has to be done after state change above for LEDs to turn off properly */
	callbacks += wlc_led_down(wlc);
#endif // endif

	/* clear txq flow control */
	wlc_txflowcontrol_reset(wlc);

	/* flush tx queues */
	for (qi = wlc->tx_queues; qi != NULL; qi = qi->next) {
		cpktq_flush(wlc, &qi->cpktq);
		ASSERT(pktq_empty(WLC_GET_CQ(qi)));
		pktq_deinit(WLC_GET_CQ(qi));
	}

#if defined(BCMDBG) && !defined(BCMDBG_EXCLUDE_HW_TIMESTAMP)
	if (wlc == wlc_info_time_dbg) {
		wlc_info_time_dbg = NULL;
	}
#endif /* BCMDBG && !BCMDBG_EXCLUDE_HW_TIMESTAMP */

	/* Increase latency tolerance */
#ifdef WL_LTR
	if (LTR_ENAB(wlc->pub)) {
		wlc_ltr_hwset(wlc->hw, wlc->regs, LTR_SLEEP);
	}
#endif /* WL_LTR */

#ifdef STA
	/* Remove keys after all pkts are flushed from txq and hardware */
	FOREACH_BSS(wlc, i, bsscfg) {
		if (BSSCFG_STA(bsscfg)) {
			if (
#ifdef WOWL
			    !WOWL_ACTIVE(wlc->pub) &&
#endif // endif
			    !(ASSOC_RECREATE_ENAB(wlc->pub) &&
			     (bsscfg->flags & WLC_BSSCFG_PRESERVE))) {
				/* delete all keys in bsscfg if it is still there */
				wlc_keymgmt_reset(wlc->keymgmt, bsscfg, NULL);
			}
		}
	}
#endif /* STA */

	callbacks += wlc_bmac_down_finish(wlc->hw);

	/* wlc_bmac_down_finish has done wlc_coredisable(). so clk is off */
	wlc->clk = FALSE;

#ifdef WLCFP
	/* CFP has dependency on HW clock. Update the states now */
	wlc_cfp_state_update(wlc->cfp);
#endif /* WLCFP */

	FOREACH_BSS(wlc, i, bsscfg) {
		/* BCMC SCBs needs to be de-allocated, if there is any */
		wlc_bsscfg_bcmcscbfree(wlc, bsscfg);
	}

	/* Verify all packets are flushed from the driver
	 * skip this if wlc_down is called from PSM WD context
	 */
	if (!wlc->psm_watchdog_debug && PKTALLOCED(wlc->osh) > 0) {
		WL_ERROR(("wl%d: %d packets not freed at wlc_down. MBSS=%d\n",
		          wlc->pub->unit, PKTALLOCED(wlc->osh), MBSS_ENAB(wlc->pub)));
		PKTLIST_DUMP(wlc->osh, NULL);

#if defined(MBSS) && defined(BCMDBG)
		if (MBSS_ENAB(wlc->pub)) {
			FOREACH_BSS(wlc, i, bsscfg) {
				wlc_mbss_dump_spt_pkt_state(wlc, bsscfg, i);
			}
		}
#endif /* defined(MBSS) && defined (BCMDBG) */

#if !(defined(CONFIG_BCM_BPM) || defined(CONFIG_BCM_BPM_MODULE))
		/* pkt allocation count is not maintained for BPM buffers */
		ASSERT(PKTALLOCED(wlc->osh) == 0);
#endif // endif
	}

	WL_MPC(("wl%d: DOWN  callbacks %d\n", wlc->pub->unit, callbacks));
	wlc->state = WLC_STATE_DOWN;

#if defined(WLRSDB)
	if (RSDB_ENAB(wlc->pub)) {
		int sar;
		sar = wlc_channel_sarenable_get(wlc->cmi);
		wlc_iovar_setint(wlc, "sar_enable", sar);
	}
#endif /* WLRSDB */

#if defined(WLRSDB) && defined(BCMECICOEX)
	if (WLC_DUALMAC_RSDB(wlc->cmn)) {
		/* For "wl out" case, don't update GCI mask -
		 * this casues ATE failures because of wrong GCI mask
		 */
		if (!wlc_bmac_get_noreset(wlc->hw)) {
			/* Update COEX GCI mask on wlc_down path - for core mpc cases in 4364 */
			wlc_btcx_update_gci_mask(wlc);
		}
	}
#endif /* WLRSDB && BCMECICOEX */

#ifdef BCM_CEVENT
	wlc_if_event(wlc, WLC_E_IF_BSSCFG_DOWN, wlc->cfg->wlcif);
#endif /* BCM_CEVENT */

	return (callbacks);
} /* wlc_down */

/** Set the current gmode configuration */
int
wlc_set_gmode(wlc_info_t *wlc, uint8 gmode, bool config)
{
	int ret = 0;
	uint i;
	wlc_rateset_t rs;
	/* Default to 54g Auto */
	int8 shortslot = WLC_SHORTSLOT_AUTO; /* Advertise and use shortslot (-1/0/1 Auto/Off/On) */
	bool shortslot_restrict = FALSE; /* Restrict association to stations that support shortslot
					  */
	bool ignore_bcns = TRUE;		/* Ignore legacy beacons on the same channel */
	bool ofdm_basic = FALSE;		/* Make 6, 12, and 24 basic rates */
	int preamble = WLC_PLCP_LONG; /* Advertise and use short preambles (-1/0/1 Auto/Off/On) */
	wlcband_t* band;

	/* if N-support is enabled, allow Gmode set as long as requested
	 * Gmode is not GMODE_LEGACY_B
	 */
	if (N_ENAB(wlc->pub) && gmode == GMODE_LEGACY_B)
		return BCME_UNSUPPORTED;

#ifdef WLP2P
	/* if P2P is enabled no GMODE_LEGACY_B is allowed */
	if (P2P_ACTIVE(wlc) && gmode == GMODE_LEGACY_B)
		return BCME_UNSUPPORTED;
#endif // endif

	/* verify that we are dealing with 2G band and grab the band pointer */
	if (wlc->band->bandtype == WLC_BAND_2G)
		band = wlc->band;
	else if ((NBANDS(wlc) > 1) &&
	         (wlc->bandstate[OTHERBANDUNIT(wlc)]->bandtype == WLC_BAND_2G))
		band = wlc->bandstate[OTHERBANDUNIT(wlc)];
	else
		return BCME_BADBAND;

	/* update configuration value */
	if (config == TRUE)
		wlc_prot_g_cfg_set(wlc->prot_g, WLC_PROT_G_USER, gmode);

	/* Clear supported rates filter */
	bzero(&wlc->sup_rates_override, sizeof(wlc_rateset_t));

	/* Clear rateset override */
	bzero(&rs, sizeof(wlc_rateset_t));

	switch (gmode) {
	case GMODE_LEGACY_B:
		shortslot = WLC_SHORTSLOT_OFF;
		wlc_rateset_copy(&gphy_legacy_rates, &rs);
		break;

	case GMODE_LRS:
		if (AP_ENAB(wlc->pub))
			wlc_rateset_copy(&cck_rates, &wlc->sup_rates_override);
		break;

	case GMODE_AUTO:
		/* Accept defaults */
		break;

	case GMODE_ONLY:
		ofdm_basic = TRUE;
		preamble = WLC_PLCP_SHORT;
		break;

	case GMODE_PERFORMANCE:
		if (AP_ENAB(wlc->pub))	/* Put all rates into the Supported Rates element */
			wlc_rateset_copy(&cck_ofdm_rates, &wlc->sup_rates_override);

		shortslot = WLC_SHORTSLOT_ON;
		shortslot_restrict = TRUE;
		ofdm_basic = TRUE;
		preamble = WLC_PLCP_SHORT;
		break;

	default:
		/* Error */
		WL_ERROR(("wl%d: %s: invalid gmode %d\n", wlc->pub->unit, __FUNCTION__, gmode));
		return BCME_UNSUPPORTED;
	}

	WL_INFORM(("wl%d: %s: 0x%x 0x%x\n", wlc->pub->unit, __FUNCTION__, gmode, config));

	/*
	 * If we are switching to gmode == GMODE_LEGACY_B,
	 * clean up rate info that may refer to OFDM rates.
	 */
	if ((gmode == GMODE_LEGACY_B) && (band->gmode != GMODE_LEGACY_B)) {
		band->gmode = gmode;
		wlc_scb_reinit(wlc);
		if (band->rspec_override && !RSPEC_ISCCK(band->rspec_override)) {
			band->rspec_override = 0;
			wlc_reprate_init(wlc);
		}
		if (band->mrspec_override && !RSPEC_ISCCK(band->mrspec_override)) {
			band->mrspec_override = 0;
		}
	}

	band->gmode = gmode;

	wlc->ignore_bcns = ignore_bcns;

	wlc->shortslot_override = shortslot;
	if (AP_ENAB(wlc->pub))
		wlc->ap->shortslot_restrict = shortslot_restrict;

	if (AP_ENAB(wlc->pub)) {
		wlc_bsscfg_t *bsscfg;
		FOREACH_BSS(wlc, i, bsscfg) {
			bsscfg->PLCPHdr_override =
				(preamble != WLC_PLCP_LONG) ? WLC_PLCP_SHORT : WLC_PLCP_AUTO;
		}
	}

	if ((AP_ENAB(wlc->pub) && preamble != WLC_PLCP_LONG) || preamble == WLC_PLCP_SHORT)
		wlc->default_bss->capability |= DOT11_CAP_SHORT;
	else
		wlc->default_bss->capability &= ~DOT11_CAP_SHORT;

	/* Update shortslot capability bit for AP and IBSS */
	if ((AP_ENAB(wlc->pub) && shortslot == WLC_SHORTSLOT_AUTO) ||
		shortslot == WLC_SHORTSLOT_ON)
		wlc->default_bss->capability |= DOT11_CAP_SHORTSLOT;
	else
		wlc->default_bss->capability &= ~DOT11_CAP_SHORTSLOT;

	/* Use the default 11g rateset */
	if (!rs.count)
		wlc_rateset_copy(&cck_ofdm_rates, &rs);

	if (ofdm_basic) {
		for (i = 0; i < rs.count; i++) {
			if (rs.rates[i] == WLC_RATE_6M || rs.rates[i] == WLC_RATE_12M ||
			    rs.rates[i] == WLC_RATE_24M)
				rs.rates[i] |= WLC_RATE_FLAG;
		}
	}

	/* Set default bss rateset */
	wlc->default_bss->rateset.count = rs.count;
	bcopy((char*)rs.rates, (char*)wlc->default_bss->rateset.rates,
		sizeof(wlc->default_bss->rateset.rates));
	band->defrateset.count = rs.count;
	bcopy((char*)rs.rates, (char*)band->defrateset.rates,
	      sizeof(band->defrateset.rates));

	wlc_update_brcm_ie(wlc);

	return ret;
} /* wlc_set_gmode */

static int
wlc_set_rateset(wlc_info_t *wlc, wlc_rateset_t *rs_arg)
{
	wlc_rateset_t rs, new;
	uint bandunit;

	bcopy((char*)rs_arg, (char*)&rs, sizeof(wlc_rateset_t));

	/* check for bad count value */
	if ((rs.count == 0) || (rs.count > WLC_NUMRATES))
		return BCME_BADRATESET;

	/* try the current band */
	bandunit = wlc->band->bandunit;
	bcopy((char*)&rs, (char*)&new, sizeof(wlc_rateset_t));
	if (wlc_rate_hwrs_filter_sort_validate(&new /* [in+out] */,
		&wlc->bandstate[bandunit]->hw_rateset /* [in] */, TRUE, wlc->stf->op_txstreams))
		goto good;

	/* try the other band */
	if (IS_MBAND_UNLOCKED(wlc)) {
		bandunit = OTHERBANDUNIT(wlc);
		bcopy((char*)&rs, (char*)&new, sizeof(wlc_rateset_t));
		if (wlc_rate_hwrs_filter_sort_validate(&new /* [in+out] */,
			&wlc->bandstate[bandunit]->hw_rateset /* [in] */, TRUE,
			wlc->stf->op_txstreams))
			goto good;
	}

	return BCME_ERROR;

good:
	/* apply new rateset */
	bcopy((char*)&new, (char*)&wlc->default_bss->rateset, sizeof(wlc_rateset_t));
	bcopy((char*)&new, (char*)&wlc->bandstate[bandunit]->defrateset, sizeof(wlc_rateset_t));

	if (VHT_ENAB_BAND(wlc->pub, wlc->band->bandtype)) {
		/* update wlc->vht_cap. mcs map (802.11 IE format) */
		wlc_vht_update_mcs_cap(wlc->vhti);
	}
#if defined(WL11AX)
	if (HE_ENAB_BAND(wlc->pub, wlc->band->bandtype)) {
		wlc_he_update_mcs_cap(wlc->hei);
	}
#endif /* WL11AC */

	return BCME_OK;
} /* wlc_set_rateset */

#ifdef BCMDBG
void
wlc_rateset_show(wlc_info_t *wlc, wlc_rateset_t *rs, struct ether_addr *ea)
{
	uint idx;
	uint r;
	bool b;

	if (WL_RATE_ON()) {
		if (ea != NULL) {
			char eabuf[ETHER_ADDR_STR_LEN];
			WL_RATE(("wl%d: %s: %s: ", wlc->pub->unit, __FUNCTION__,
			         bcm_ether_ntoa(ea, eabuf)));
		}
		WL_RATE(("[ "));
		for (idx = 0; idx < rs->count; idx++) {
			r = rs->rates[idx] & RATE_MASK;
			b = rs->rates[idx] & (~RATE_MASK);
			if (r == 0)
				break;
			WL_RATE(("%d%s%s ", (r / 2), (r % 2)?".5":"", b?"(b)":""));
		}
		WL_RATE(("]\n"));
		WL_RATE(("VHT mcsmap (rateset format): %04x\n", rs->vht_mcsmap));
		WL_RATE(("VHT mcsmap_prop (rateset format): %04x\n", rs->vht_mcsmap_prop));
	}
}
#endif /* BCMDBG */

/** bandlock ioctl */
int
wlc_bandlock(wlc_info_t *wlc, int val)
{
	bool move;
	uint bandunit, j;

	WL_TRACE(("wl%d: wlc_bandlock: wlc %p val %d\n",
		wlc->pub->unit, wlc, OSL_OBFUSCATE_BUF(val)));

	/* sanity check arg */
	if ((val < WLC_BAND_AUTO) || (val > WLC_BAND_2G))
		return BCME_RANGE;

	/* single band is easy */
	if (NBANDS(wlc) == 1) {
		if ((val != WLC_BAND_AUTO) && (val != wlc->band->bandtype))
			return BCME_BADBAND;

		wlc->bandlocked = (val == WLC_BAND_AUTO)? FALSE : TRUE;
#ifdef WLLED
		if (wlc->pub->up)
			wlc_led_event(wlc->ledh);
#endif // endif
		return 0;
	}

	switch (val) {
	case WLC_BAND_AUTO:
		wlc->bandlocked = FALSE;
#ifdef WLRSDB
		/* set band locks in WLTEST */
		wlc_rsdb_do_band_lock(wlc, FALSE);
#endif /* WLRSDB */
		break;

	case WLC_BAND_5G:
	case WLC_BAND_2G: {
		bool valid_channels = FALSE;
		/* multiband */
		move = (wlc->band->bandtype != val);
		bandunit = (val == WLC_BAND_5G) ? 1 : 0;
#ifdef WLRSDB
		if (RSDB_ENAB(wlc->pub)) {
			wlc_info_t *other_wlc = wlc_rsdb_get_other_wlc(wlc);
			if (wlc != wlc_rsdb_find_wlc_for_band(wlc->rsdbinfo, bandunit, other_wlc)) {
				WL_ERROR(("wl%d: Band is not allowed \n", wlc->pub->unit));
				return BCME_BADBAND;
			}
		}
#endif /* WLRSDB */

		/* make sure new band has at least one hw supported channel */
		/* XXX REVISIT johnvb  The above comment says a hw supported channel
		 * but it really checks for a hw supported channel for the
		 * current locale.  Is this what is intended or does it really
		 * want to test for just hw/phy support?  I.e. what if the current
		 * country precludes using the band/channel but a different
		 * locale would be fine.
		 */
		for (j = 0; j < MAXCHANNEL; j++) {
			if (VALID_CHANNEL20_IN_BAND(wlc, bandunit, j)) {
				valid_channels = TRUE;
				break;
			}
		}
		if (valid_channels == FALSE) {
			WL_ERROR(("wl%d: can't change band since no valid channels in new band\n",
				wlc->pub->unit));
			return BCME_BADBAND;
		}

		/* prepare to set one band, allow core switching */
		wlc->bandlocked = FALSE;

		if (wlc->pub->up) {
			wlc_scan_abort(wlc->scan, WLC_E_STATUS_ABORT);
		}

		if (move) {
#ifdef STA
			if (wlc->pub->up) {
				int idx;
				wlc_bsscfg_t *cfg;
				FOREACH_BSS(wlc, idx, cfg) {
					if (BSSCFG_STA(cfg) && cfg->enable) {
						WL_APSTA_UPDN(("wl%d: wlc_bandlock() ->"
						               " wlc_bsscfg_disable()\n",
						               wlc->pub->unit));
						wlc_disassociate_client(cfg, FALSE);
						wlc_bsscfg_disable(wlc, cfg);
					}
				}
			}
#endif /* STA */
#ifdef AP
			if (wlc->pub->up && AP_ENAB(wlc->pub) && wlc->pub->associated) {
				/* do not switch band if the interface is associated in AP mode.
				 * We need do wlc_BSSinit() to sanitize the rateset
				 * otherwise we will fail the wlc_valid_rate checking
				 */
				WL_ERROR(("wl%d: can't change band when associated\n",
					wlc->pub->unit));
				return BCME_ASSOCIATED;
			}
#endif // endif
#ifdef WDS
			/* On changing the band, delete the WDS nodes by freeing the scb
			 * and deleting the wds interface (done by wlc_scbfree). This is
			 * required because the scb is looked up off of the hash table
			 * associated with band and wds scbs will not be found on a band
			 * change in wlc_send.
			 */
			if (AP_ENAB(wlc->pub))
				wlc_scb_wds_free(wlc);
#endif // endif
			/* switch to first channel in the new band */
			wlc_channels_init_ext(wlc->cmi);
			wlc_change_band(wlc, bandunit);
			/* XXX APSTA JAS Where does AP update beacon and probe response?
			 * Enter a PR?
			 */
		}

		/* If the default bss chanspec is now invalid then pick a valid one */
		if (!wlc_valid_chanspec(wlc->cmi, wlc->default_bss->chanspec))
			wlc->default_bss->chanspec = wlc_default_chanspec(wlc->cmi, TRUE);

		/* Update bss rates to the band specific default rate set */
		wlc_default_rateset(wlc, &wlc->default_bss->rateset);

		wlc->bandlocked = TRUE;
		break;
	}
	default:
		ASSERT(0);
		break;
	}

#ifdef WLLED
	/* fixup LEDs */
	if (wlc->pub->up)
		wlc_led_event(wlc->ledh);
#endif // endif

	return 0;
} /* wlc_bandlock */

int
wlc_change_band(wlc_info_t *wlc, int band)
{
	chanspec_t chspec;

	/* sanity check arg */
	if (((band != BAND_2G_INDEX) && (band != BAND_5G_INDEX))) {
		return BCME_RANGE;
	}

	/* switch to first channel in the new band */
	wlc_pi_band_update(wlc, band);
	chspec = wlc_default_chanspec(wlc->cmi, FALSE);
	ASSERT(chspec != INVCHANSPEC);
	wlc_pi_band_update(wlc, OTHERBANDUNIT(wlc));
	wlc->home_chanspec = chspec;

	if (wlc->pub->up) {
		wlc_suspend_mac_and_wait(wlc);
		wlc_set_chanspec(wlc, chspec, CHANSW_REASON(CHANSW_UPDBW));
		wlc_enable_mac(wlc);
	} else {
		/* In down state, only update the software chanspec. Don't call
		 * wlc_set_chanspec(), which touches the hardware. In high driver,
		 * there's no concept of band switch, which is encapsulated inside
		 * the chanspec change.
		 */
		wlc_pi_band_update(wlc, band);
		/* sync up phy/radio chanspec */
		wlc_set_phy_chanspec(wlc, chspec);
	}

	return BCME_OK;
}

#ifdef STA
int
wlc_set_pm_mode(wlc_info_t *wlc, int val, wlc_bsscfg_t *bsscfg)
{
	wlc_pm_st_t *pm = bsscfg->pm;

#ifdef SLAVE_RADAR
	/* For Example: When STA on radar channel */
	if (wlc_dfs_get_radar(wlc->dfs) &&(pm->pm_modechangedisabled)) {
		WL_PS(("wl%d: PM mode changing blocked \n", WLCWLUNIT(wlc)));
		return BCME_UNSUPPORTED;
	}
#endif /* SLAVE_RADAR */
	if ((val < PM_OFF) || (val > PM_FAST)) {
		return BCME_ERROR;
	}

	WL_INFORM(("wl%d.%d: setting PM from %d to %d\n",
		WLCWLUNIT(wlc), WLC_BSSCFG_IDX(bsscfg), pm->PM, val));

	if (pm->PM == (uint8)val)
		return BCME_OK;

	pm->PM = (uint8)val;
	if (pm->PM == PM_MAX) {
		struct scb *scb;
		struct ether_addr *bssid = &bsscfg->BSSID;
		if ((scb = wlc_scbfind(wlc, bsscfg, bssid))) {
			/* SCB_PENDING_PSPOLL flag should not be set
			 * before enabling PM_MAX
			*/
			if (scb->flags & SCB_PENDING_PSPOLL) {
				WL_ERROR(("%s: SCB_FLAGS:%x, pm->PSpoll:%x, "
					"wlc->PSpoll:%x\n",
					__FUNCTION__,
					scb->flags,
					bsscfg->pm->PSpoll, wlc->PSpoll));
					WIFICC_CAPTURE("PM_MAX: SCB FLAGS");

				/* Clear SCB flags PSPoll PS state */
				scb->flags &= ~SCB_PENDING_PSPOLL;
				wlc_set_pspoll(bsscfg, FALSE);
			}
		}
	}

	/* if entering or exiting Fast PM mode, reset the PM2 features,
	 * stop any timers, and restore the TBTT wakeup interval.
	 */
	if (wlc->pub->up) {
		wlc_pm2_sleep_ret_timer_stop(bsscfg);
		if (PM2_RCV_DUR_ENAB(bsscfg)) {
			wlc_pm2_rcv_reset(bsscfg);
		}
		WL_RTDC(wlc, "WLC_SET_PM: %d\n", pm->PM, 0);
	}

	/* Set pmstate only if different from current state and */
	/* coming out of PM mode or PM is allowed */
	if ((!pm->WME_PM_blocked || val == PM_OFF)) {
		if (val == PM_FAST && BSSCFG_PM_ALLOWED(bsscfg))
			(void)wlc_pm2_start_ps(bsscfg);
		else
			wlc_set_pmstate(bsscfg, val != PM_OFF);
	}

	/* Change watchdog driver to align watchdog with tbtt if possible */
	wlc_watchdog_upd(bsscfg, WLC_WATCHDOG_TBTT(wlc));
	wlc_ampdu_upd_pm(wlc, pm->PM);

	return 0;
}

#endif /* STA */

static void
wlc_do_down(wlc_info_t *wlc)
{
	int idx;
	wlc_cmn_info_t *wlc_cmn = wlc->cmn;
	wlc_info_t *wlc_iter;
	FOREACH_WLC(wlc_cmn, idx, wlc_iter) {

		WL_APSTA_UPDN(("wl%d: wlc_ioctl(WLC_DOWN) -> wl_down()\n",
			wlc_iter->pub->unit));
#ifdef WLRSDB
		if (RSDB_ENAB(wlc->pub) &&
			(WLC_RSDB_SINGLE_MAC_MODE(WLC_RSDB_CURR_MODE(wlc_iter)))&&
			(wlc_iter != wlc_iter->cmn->wlc[0])) {
			break;
		}
#endif /* WLRSDB */
		/* Don't access the hardware if hw is off */
		if (wlc_iter->pub->hw_off)
			continue;

		wlc_iter->down_override = TRUE;
#ifdef WL_DUALNIC_RSDB
		wlc_down(wlc_iter);
#else
		wl_down(wlc_iter->wl);
#endif // endif
#ifdef WLLED
		wlc_led_event(wlc->ledh);
#endif /* WLLED */
		wlc_iter->pub->hw_up = FALSE;
	}
}

static int
wlc_do_up(wlc_info_t *wlc)
{
	int bcmerror = 0;
	int idx;
	wlc_info_t *wlc_iter;

	FOREACH_WLC_UP(wlc, idx, wlc_iter) {
#ifndef DONGLEBUILD
#ifdef WLRSDB
		/* If the current mode is not RSDB, we don't have to bring up the second wlc.
		 *  So, break out for NIC build if it is not the first wlc
		 */
		if ((WLC_RSDB_CURR_MODE(wlc_iter) != WLC_RSDB_MODE_RSDB) &&
				(wlc_iter != wlc_iter->cmn->wlc[0])) {
			break;
		}
#endif /* WLRSDB */
#endif /* !DONGLEBUILD */

	/* Don't access the hardware if hw is off */
	if (wlc_iter->pub->hw_off)
		return BCME_RADIOOFF;

	wlc_iter->down_override = FALSE;
	wlc_iter->mpc_out = FALSE;
#ifdef STA
	wlc_radio_mpc_upd(wlc_iter);
#endif /* STA */

	wlc_radio_hwdisable_upd(wlc_iter);
	wlc_radio_upd(wlc_iter);

	if (!wlc_iter->pub->up) {
		if (mboolisset(wlc_iter->pub->radio_disabled, WL_RADIO_HW_DISABLE) ||
			mboolisset(wlc_iter->pub->radio_disabled, WL_RADIO_SW_DISABLE) ||
#ifdef WLRSDB
			(RSDB_ENAB(wlc_iter->pub) &&
			(WLC_RSDB_SINGLE_MAC_MODE(WLC_RSDB_CURR_MODE(wlc_iter))) &&
			(wlc_iter != wlc_iter->cmn->wlc[0])) ||
#endif // endif
			FALSE) {
						bcmerror = BCME_RADIOOFF;
		} else if (mboolisset(wlc_iter->pub->radio_disabled,
				WL_RADIO_MPC_DISABLE)) {
			WL_INFORM(("wl%d: up failed:mpc radio is disabled\n", wlc_iter->pub->unit));
		} else if (mboolisset(wlc_iter->pub->radio_disabled,
				WL_RADIO_COUNTRY_DISABLE)) {
			WL_INFORM(("wl%d: up failed: radio is disabled for country %s\n",
				wlc_iter->pub->unit, wlc_channel_country_abbrev(wlc_iter->cmi)));
		} else if (mboolisset(wlc->pub->radio_disabled,
			WL_RADIO_PERCORE_DISABLE)) {
			WL_INFORM(("wl%d: up failed: radio is disabled for percore\n",
				wlc->pub->unit));
		}
#ifdef WLRSDB
		else if (RSDB_ENAB(wlc_iter->pub) &&
				(WLC_RSDB_SINGLE_MAC_MODE(WLC_RSDB_CURR_MODE(wlc_iter))) &&
				(wlc_iter != wlc_iter->cmn->wlc[0])) {
					return BCME_RADIOOFF;
		}
#endif /* WLRSDB */
		else {
								/* something is wrong */
			bcmerror = BCME_ERROR;
			ASSERT(0);
		}
	}
	}
	return bcmerror;
}

#define WLC_PHYTYPE(_x) (_x) /* macro to perform WLC PHY -> D11 PHY TYPE, currently 1:1 */

/** wlc ioctl handler. return: 0=ok, <0=error */
static int
wlc_doioctl(void *ctx, uint cmd, void *arg, uint len, struct wlc_if *wlcif)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;
	int val, *pval;
	bool bool_val;
	int bcmerror;
	uint i;
	struct scb *scb = NULL;
	uint band;
	wlc_bsscfg_t *bsscfg;
	struct scb_iter scbiter;
	osl_t *osh;
	wlc_bss_info_t *current_bss;
	wlc_phy_t *pi = WLC_PI(wlc);

	/* update bsscfg pointer */
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	current_bss = bsscfg->current_bss;

	/* update wlcif pointer */
	if (wlcif == NULL)
		wlcif = bsscfg->wlcif;
	ASSERT(wlcif != NULL);

	/* initialize the following to get rid of compiler warning */
	band = 0;
	BCM_REFERENCE(band);

	/* default argument is generic integer */
	pval = (int *) arg;

	/* This will prevent the misaligned access */
	if ((uint32)len >= sizeof(val))
		bcopy(pval, &val, sizeof(val));
	else
		val = 0;

	/* bool conversion to avoid duplication below */
	bool_val = (val != 0);

	bcmerror = 0;
	osh = wlc->osh;

	switch (cmd) {

	case WLC_GET_MAGIC:
		*pval = WLC_IOCTL_MAGIC;
		break;

	case WLC_GET_VERSION:
		*pval = WLC_IOCTL_VERSION;
		break;

	case WLC_UP:
		bcmerror = wlc_do_up(wlc);
		break;

	case WLC_OUT:

		/* Don't access the hardware if hw is off */
		if (wlc->pub->hw_off)
			return BCME_RADIOOFF;

		/* make MPC transparent to "wl out", bring driver up if MPC made it down */
		wlc->mpc_out = TRUE;
#ifdef STA
		wlc_radio_mpc_upd(wlc);	/* this may cause interrupts and queued dpc */
		if (!wlc->pub->up)
			WL_MPC(("wl%d: WLC_OUT, driver was down, but not due to MPC\n",
			        wlc->pub->unit));
#endif /* STA */

		wlc->down_override = TRUE;
		wlc_out(wlc);
		break;

	case WLC_DOWN:
	case WLC_REBOOT:
		wlc_do_down(wlc);
		break;

	case WLC_GET_UP:
		*pval = wlc_isup(wlc);
		break;

#if !defined(WLWSEC) || defined(WLWSEC_DISABLED)
	/* when -nosec- is used, we might need to fake some iovars/ioctls to keep host happy. */
	case WLC_SET_KEY:
		break;
#endif // endif

	case WLC_GET_CLK:
		*pval = wlc->clk;
		break;

	case WLC_SET_CLK:
		if (wlc->pub->up) {
			bcmerror = BCME_NOTDOWN;
			break;
		}

		/* Don't access the hardware if hw is off */
		if (wlc->pub->hw_off) {
			bcmerror = BCME_RADIOOFF;
			break;
		}

#ifdef BCM_RECLAIM_INIT_FN_DATA
		if (bcm_reclaimed) {
			bcmerror = val ? 0 : BCME_EPERM;
			break;
		}
#endif /* BCM_RECLAIM_INIT_FN_DATA */

		wlc_bmac_set_clk(wlc->hw, bool_val);
		wlc->clk = bool_val;

		break;

#if defined(BCMDBG) || defined(WLTEST)
	case WLC_GET_MSGLEVEL:
		*pval = wl_msg_level;
		break;

	case WLC_SET_MSGLEVEL:
		wl_msg_level = val;
		break;
#endif /* BCMDBG || WLTEST */

#if defined(WL_PROMISC) || defined(BCMSPACE)
	case WLC_GET_PROMISC:
		*pval = (int)wlc->pub->promisc;
		break;

	case WLC_SET_PROMISC:
		wlc->pub->promisc = (val != 0);

		if (!wlc->clk) {
			bcmerror = 0;
			break;
		}

		wlc_mac_promisc(wlc);
		break;
#endif /* WL_PROMISC || BCMSPACE */

#if defined(WL_MONITOR)
	case WLC_GET_MONITOR:
		if (!wlc->pub->_mon)
			return BCME_UNSUPPORTED;

		*pval = (int)wlc->monitor;
		break;

	case WLC_SET_MONITOR:
		if (!wlc->pub->_mon)
			return BCME_UNSUPPORTED;

		if (wlc->pub->associated || SCAN_IN_PROGRESS(wlc->scan)) {
			WL_ERROR(("wl%d: Monitor Mode in Associated(%d)/Scan(%d) state"
				" is not permitted.\n",
				wlc->pub->unit, wlc->pub->associated, SCAN_IN_PROGRESS(wlc->scan)));
			return BCME_EPERM;
		}

		wlc->monitor = val;
#ifdef STA
		/* disable mpc if monitor is on, enable mpc if monitor is off */
		wlc_radio_mpc_upd(wlc);
#endif // endif
		wl_set_monitor(wlc->wl, val);
		wlc_monitor_set_promisc_bit(wlc->mon_info, (wlc->monitor != 0));
		if (!wlc->clk) {
			bcmerror = 0;
			break;
		}

		wlc_monitor_promisc_enable(wlc->mon_info, (val != 0));
#if defined(DONGLEBUILD) && defined(WL_MU_MONITOR)
		if (val) {
			/* Indicate ucode to receive maximum upto monrxbufsz packet size */
			wlc_write_shm(wlc, M_MAXRXFRM_LEN(wlc),
					(uint16)wlc->pub->tunables->monrxbufsz);
			if (D11REV_GE(wlc->pub->corerev, 65)) {
				W_REG(wlc->osh, D11_DAGG_LEN_THR(wlc),
					(uint16)wlc->pub->tunables->monrxbufsz);
			}
			/* Update fifo0 dma rxbufsz to monrxbufsz */
			dma_param_set(wlc->hw->di[RX_FIFO], HNDDMA_NRXBUFSZ,
					(uint16)wlc->pub->tunables->monrxbufsz);
			/* Disable packet classification when monitor mode is enabled */
			wlc_mhf(wlc, MHF3, MHF3_SELECT_RXF1, 0, WLC_BAND_ALL);
		}
		else {
			wlc_write_shm(wlc, M_MAXRXFRM_LEN(wlc),
					(uint16)wlc->pub->tunables->rxbufsz);
			if (D11REV_GE(wlc->pub->corerev, 65)) {
				W_REG(wlc->osh, D11_DAGG_LEN_THR(wlc),
					(uint16)wlc->pub->tunables->rxbufsz);
			}
			dma_param_set(wlc->hw->di[RX_FIFO], HNDDMA_NRXBUFSZ,
					(uint16)wlc->pub->tunables->rxbufsz);
			wlc_mhf(wlc, MHF3, MHF3_SELECT_RXF1, MHF3_SELECT_RXF1, WLC_BAND_ALL);

		}
#endif /* defined(DONGLEBUILD) && defined(WL_MU_MONITOR) */

#ifdef STA
		/* Update PM state based on Monitor mode */
		wlc_set_wake_ctrl(wlc);
#endif /* STA */
		break;
#endif /* WL_MONITOR */

	case WLC_GET_RATE: {
		/* return raw datarate in units of 500 Kbit/s, other parameters are translated */
		ratespec_t rspec = wlc_get_rspec_history(bsscfg);
		*pval = RSPEC2KBPS(rspec)/500;
		break;
	}

	case WLC_GET_MAX_RATE: {
		/* return max raw datarate in units of 500 Kbit/s */
		ratespec_t rspec = wlc_get_current_highest_rate(bsscfg);
		*pval = RSPEC2KBPS(rspec)/500;
		break;
	}

	case WLC_GET_INSTANCE:
		*pval = wlc->pub->unit;
		break;

	case WLC_GET_INFRA:
		*pval = bsscfg->BSS;
		break;

	case WLC_SET_INFRA:
		bcmerror = wlc_iovar_op(wlc, "infra_configuration", NULL, 0,
		                        &val, sizeof(val), IOV_SET, wlcif);
		break;

	case WLC_GET_AUTH:
		*pval = DOT11AUTH2WLAUTH(bsscfg);
		break;

	case WLC_SET_AUTH:
		if (val == WL_AUTH_OPEN_SYSTEM || val == WL_AUTH_SHARED_KEY) {
			bsscfg->auth = WLAUTH2DOT11AUTH(val);
			bsscfg->openshared = 0;
		} else {
			/* xxx: Some branch use different define for WL_AUTH_OPEN_SHARED
			 * for example, PHOENIX2 Branch defined WL_AUTH_OPEN_SHARED as 3
			 * But other branch defined WL_AUTH_OPEN_SHARED as 2
			 * if it is mismatch, WEP association can be failed.
			 * therefore for othercase(!WL_AUTH_OPEN_SYSTEM && !WL_AUTH_SHARED_KEY)
			 * we assume it is WL_AUTH_OPEN_SHARED
			 * More information - RB:5320
			 */
#ifdef WL_AUTH_SHARED_OPEN
			if (AUTH_SHARED_OPEN_ENAB(wlc->pub)) {
				bsscfg->auth = DOT11_SHARED_KEY;
			} else
#endif /* WL_AUTH_SHARED_OPEN */
			{
				bsscfg->auth = DOT11_OPEN_SYSTEM;
			}
			bsscfg->openshared = 1;
		}
		break;

	case WLC_GET_BSSID:

		/* Report on primary config */
		if ((BSSCFG_STA(bsscfg) && !bsscfg->associated)) {
			bcmerror = BCME_NOTASSOCIATED;
			break;
		}
		if (len < ETHER_ADDR_LEN) {
			bcmerror = BCME_BUFTOOSHORT;
			break;
		}
		bcopy(bsscfg->BSSID.octet, arg, ETHER_ADDR_LEN);
		break;

	case WLC_SET_BSSID:
		if (bsscfg->up) {
			bcmerror = BCME_NOTDOWN;
			break;
		}

		if (len < ETHER_ADDR_LEN) {
			bcmerror = BCME_BUFTOOSHORT;
			break;
		}
		bcopy(arg, bsscfg->BSSID.octet, ETHER_ADDR_LEN);
		if (!wlc->clk) {
			bcmerror = 0;
			break;
		}
		wlc_set_bssid(bsscfg);
		break;

	case WLC_GET_SSID: {
		wlc_ssid_t *ssid = (wlc_ssid_t *) arg;
		if (len < (int)sizeof(wlc_ssid_t)) {
			bcmerror = BCME_BUFTOOSHORT;
			break;
		}
		bzero((char*)ssid, sizeof(wlc_ssid_t));
		if (BSSCFG_AP(bsscfg)) {
			ssid->SSID_len = bsscfg->SSID_len;
			bcopy(bsscfg->SSID, ssid->SSID, ssid->SSID_len);
		} else if (bsscfg->associated) {
			ssid->SSID_len = current_bss->SSID_len;
			bcopy(current_bss->SSID, ssid->SSID, current_bss->SSID_len);
		} else {
			ssid->SSID_len = wlc->default_bss->SSID_len;
			bcopy(wlc->default_bss->SSID, ssid->SSID, wlc->default_bss->SSID_len);
		}

		break;
	}

	case WLC_SET_SSID: {
		wlc_ssid_t *ssid = (wlc_ssid_t *)arg;

		if (len < sizeof(ssid->SSID_len)) {
			bcmerror = BCME_BUFTOOSHORT;
			break;
		}
		if (len < sizeof(ssid->SSID_len) + ssid->SSID_len) {
			bcmerror = BCME_BUFTOOSHORT;
			break;
		}
		if (ssid->SSID_len > DOT11_MAX_SSID_LEN) {
			bcmerror = BCME_BADSSIDLEN;
			break;
		}

#ifdef WL_MONITOR
		if (MONITOR_ENAB(wlc)) {
			WL_ERROR(("wl%d: SET_SSID in Monitor Mode state is not permitted.\n",
					wlc->pub->unit));
			return BCME_EPERM;
		}
#endif /* WL_MONITOR */

		/* keep old behavior of WLC_SET_SSID by restarting the AP's BSS
		 * if the SSID is non-null
		 */
		if (BSSCFG_AP(bsscfg)) {

			wlc_bsscfg_SSID_set(bsscfg, ssid->SSID, ssid->SSID_len);

			WL_APSTA_UPDN(("wl%d: SET_SSID -> wlc_bsscfg_disable()\n",
			               wlc->pub->unit));
			wlc_bsscfg_disable(wlc, bsscfg);

			/* only bring up the BSS if SSID is non-null */
			if (bsscfg->SSID_len != 0) {
				WL_APSTA_UPDN(("wl%d: SET_SSID -> wlc_bsscfg_enable()\n",
					wlc->pub->unit));
				bcmerror = wlc_bsscfg_enable(wlc, bsscfg);

				if (bcmerror)
					break;
#if defined(RADAR)
				if (RADAR_ENAB(wlc->pub) && WL11H_AP_ENAB(wlc)) {
					/* no radar detection if sta is associated since
					 * AP shares the same channel which is selected
					 * by an external AP sta associates with
					 */
					if (!(BSSCFG_SRADAR_ENAB(bsscfg) ||
					      BSSCFG_AP_NORADAR_CHAN_ENAB(bsscfg)))
						wlc_set_dfs_cacstate(wlc->dfs, ON, bsscfg);
				}
#endif /* defined(RADAR) */
			}
		}

#ifdef STA
		/* Behavior of WLC_SET_SSID for STA is different from that for AP */
		if (BSSCFG_STA(bsscfg)) {
			wl_join_assoc_params_t *assoc_params = NULL;
			int assoc_params_len = 0;

			if ((uint)len >= WL_JOIN_PARAMS_FIXED_SIZE) {
				assoc_params = &((wl_join_params_t *)arg)->params;
				assoc_params_len = len - OFFSETOF(wl_join_params_t, params);
			}

			WL_APSTA_UPDN(("wl%d: SET_SSID(STA) -> wlc_join()\n",
			               wlc->pub->unit));
			bcmerror = wlc_join_cmd_proc(wlc, bsscfg, ssid,
			         NULL,
			         assoc_params, assoc_params_len);
		}
#endif /* STA */

		break;
	}

	case WLC_GET_CHANNEL: {
		channel_info_t *ci = (channel_info_t*)arg;
		int chspec;

		ASSERT(len > (int)sizeof(ci));

		ci->hw_channel = wf_chspec_ctlchan(WLC_BAND_PI_RADIO_CHANSPEC);

		bcmerror = wlc_iovar_op(wlc, "chanspec",
		                        NULL, 0, &chspec, sizeof(int), IOV_GET, wlcif);
		if (bcmerror != BCME_OK)
			break;
		ci->target_channel = wf_chspec_ctlchan((chanspec_t)chspec);

		ci->scan_channel = 0;

		if (SCAN_IN_PROGRESS(wlc->scan)) {
			ci->scan_channel =
				wf_chspec_ctlchan(wlc_scan_get_current_chanspec(wlc->scan));
		}
		break;
	}

	case WLC_SET_CHANNEL: {
		int chspec = CH20MHZ_CHSPEC(val);

		if (val < 0 || val > MAXCHANNEL) {
			bcmerror = BCME_OUTOFRANGECHAN;
			break;
		}
		bcmerror = wlc_iovar_op(wlc, "chanspec",
		                        NULL, 0, &chspec, sizeof(int), IOV_SET, wlcif);
		break;
	}

	case WLC_GET_PWROUT_PERCENTAGE:
		*pval = wlc->txpwr_percent;
		break;

	case WLC_SET_PWROUT_PERCENTAGE:
		if ((uint)val > 100) {
			bcmerror = BCME_RANGE;
			break;
		}

		wlc->txpwr_percent = (uint8)val;
		wlc_bmac_set_txpwr_percent(wlc->hw, (uint8)val);
		if (wlc->pub->up) {
			uint8 constraint;

			if (SCAN_IN_PROGRESS(wlc->scan)) {
				WL_INFORM(("wl%d: Scan in progress, skipping txpower control\n",
					wlc->pub->unit));
				break;
			}

			wlc_suspend_mac_and_wait(wlc);

			/* Set the power limits for this locale after computing
			 * any 11h local tx power constraints.
			 */
			constraint = wlc_tpc_get_local_constraint_qdbm(wlc->tpc);
			wlc_channel_set_txpower_limit(wlc->cmi, constraint);

			wlc_enable_mac(wlc);
		}
		break;

	case WLC_GET_TXANT:
		if (WLCISACPHY(wlc->band) || WLCISLCN20PHY(wlc->band)) {
			bcmerror = BCME_UNSUPPORTED;
			break;
		}
		*pval = wlc->stf->txant;
		break;

	case WLC_SET_TXANT:
		if (WLCISACPHY(wlc->band) || WLCISLCN20PHY(wlc->band)) {
			bcmerror = BCME_UNSUPPORTED;
			break;
		}
		/* if down, we are done */
		if (!wlc->pub->up)
			break;
		{
			bcmerror = wlc_stf_ant_txant_validate(wlc, (int8)val);
			if (bcmerror < 0)
				break;

			wlc->stf->txant = (int8)val;

			/* if down, we are done */
			if (!wlc->pub->up)
				break;

			wlc_suspend_mac_and_wait(wlc);

			wlc_stf_phy_txant_upd(wlc);
			wlc_beacon_phytxctl_txant_upd(wlc, wlc->bcn_rspec);

			wlc_enable_mac(wlc);
		}
		break;

	case WLC_GET_ANTDIV: {
		uint8 phy_antdiv;

		if (WLCISACPHY(wlc->band)) {
			bcmerror = BCME_UNSUPPORTED;
			break;
		}

		/* return configured value if core is down */
		if (!wlc->pub->up) {
			*pval = wlc->stf->ant_rx_ovr;
		} else {
			phy_antdiv_get_rx((phy_info_t *)pi, &phy_antdiv);
			*pval = (int)phy_antdiv;
		}
		break;
	}
	case WLC_SET_ANTDIV:
		if (WLCISLCN20PHY(wlc->band) || WLCISACPHY(wlc->band)) {
			bcmerror = BCME_UNSUPPORTED;
			break;
		}

		/* values are -1=driver default, 0=force0, 1=force1, 2=start1, 3=start0 */
		if ((val < -1) || (val > 3)) {
			bcmerror = BCME_RANGE;
			break;
		}
		if (val == -1)
			val = ANT_RX_DIV_DEF;

		wlc->stf->ant_rx_ovr = (uint8)val;
		bcmerror = phy_antdiv_set_rx((phy_info_t *)pi, (uint8)val);
		break;

	case WLC_GET_RX_ANT: {	/* get latest used rx antenna */
		uint16 rxstatus;

		if (!wlc->pub->up) {
			bcmerror = BCME_NOTUP;
			break;
		}

		rxstatus = R_REG(osh, D11_PHY_REG_4(wlc));
		if (rxstatus == 0xdead || rxstatus == (uint16)-1) {
			bcmerror = BCME_ERROR;
			break;
		}
		*pval = (rxstatus & PRXS0_RXANT_UPSUBBAND) ? 1 : 0;
		break;
	}

#if defined(BCMDBG) || defined(WLTEST)
	case WLC_GET_UCANTDIV:
		if (!wlc->clk) {
			bcmerror = BCME_NOCLK;
			break;
		}

		if (WLCISACPHY(wlc->band)) {
			bcmerror = BCME_UNSUPPORTED;
			break;
		}

		*pval = (wlc_bmac_mhf_get(wlc->hw, MHF1, WLC_BAND_AUTO) & MHF1_ANTDIV);
		break;

	case WLC_SET_UCANTDIV: {
		if (!wlc->pub->up) {
			bcmerror = BCME_NOTUP;
			break;
		}

		/* if multiband, band must be locked */
		if (IS_MBAND_UNLOCKED(wlc)) {
			bcmerror = BCME_NOTBANDLOCKED;
			break;
		}

		if (WLCISACPHY(wlc->band)) {
			bcmerror = BCME_UNSUPPORTED;
			break;
		}

		wlc_mhf(wlc, MHF1, MHF1_ANTDIV, (val? MHF1_ANTDIV : 0), WLC_BAND_AUTO);
		break;
	}
#endif /* BCMDBG || WLTEST */

	case WLC_GET_SRL:
		*pval = wlc->SRL;
		break;

	case WLC_SET_SRL:
		if (val >= 1 && val <= RETRY_SHORT_MAX) {
			int ac;
			wlc->SRL = (uint16)val;

			wlc_bmac_retrylimit_upd(wlc->hw, wlc->SRL, wlc->LRL);

			for (ac = 0; ac < AC_COUNT; ac++) {
				WLC_WME_RETRY_SHORT_SET(wlc, ac, wlc->SRL);
			}
			wlc_wme_retries_write(wlc);
		} else
			bcmerror = BCME_RANGE;
		break;

	case WLC_GET_LRL:
		*pval = wlc->LRL;
		break;

	case WLC_SET_LRL:
		if (val >= 1 && val <= 255) {
			int ac;
			wlc->LRL = (uint16)val;

			wlc_bmac_retrylimit_upd(wlc->hw, wlc->SRL, wlc->LRL);

			for (ac = 0; ac < AC_COUNT; ac++) {
				WLC_WME_RETRY_LONG_SET(wlc, ac, wlc->LRL);
			}
			wlc_wme_retries_write(wlc);
		} else
			bcmerror = BCME_RANGE;
		break;

	case WLC_GET_CWMIN:
		*pval = wlc->band->CWmin;
		break;

	case WLC_SET_CWMIN:
		if (!wlc->clk) {
			bcmerror = BCME_NOCLK;
			break;
		}

		if (val >= 1 && val <= 255) {
			wlc_set_cwmin(wlc, (uint16)val);
		} else
			bcmerror = BCME_RANGE;
		break;

	case WLC_GET_CWMAX:
		*pval = wlc->band->CWmax;
		break;

	case WLC_SET_CWMAX:
		if (!wlc->clk) {
			bcmerror = BCME_NOCLK;
			break;
		}

		if (val >= 255 && val <= 2047) {
			wlc_set_cwmax(wlc, (uint16)val);
		} else
			bcmerror = BCME_RANGE;
		break;

	case WLC_GET_PLCPHDR:
		*pval = bsscfg->PLCPHdr_override;
		break;

	case WLC_SET_PLCPHDR:
		/*
		 * Regardless of the value here, the MAC will always return ctrl/mgmt
		 * response frames with the same type preamble as received.
		 * So will the driver unless forced long (1) here (testing only).
		 * (-1) is to disable short preamble capability advertisement and
		 *      never to initiate a short preamble frame exchange sequence.
		 * (0) BSS STA: enable short preamble capability advertisement but
		 *              the driver will only send short preambles if the
		 *              AP also advertises the capability;
		 *     AP:      enable short preamble capability advertisement and will
		 *              deny association to STAs that do not advertise the capability.
		 *     IBSS STA:questionable but will generally work.
		 */
		if ((val == WLC_PLCP_AUTO) || (val == WLC_PLCP_SHORT) || (val == WLC_PLCP_LONG))
			bsscfg->PLCPHdr_override = (int8) val;
		else
			bcmerror = BCME_RANGE;
		break;

	case WLC_GET_RADIO:	/* use mask if don't want to expose some internal bits */
		*pval = wlc->pub->radio_disabled;
		break;

	case WLC_SET_RADIO: { /* 32 bits input, higher 16 bits are mask, lower 16 bits are value to
			       * set
			       */
		uint16 radiomask, radioval;
		uint validbits = WL_RADIO_SW_DISABLE | WL_RADIO_HW_DISABLE;
		mbool new = 0;

		radiomask = (val & 0xffff0000) >> 16;
		radioval = val & 0x0000ffff;

		if ((radiomask == 0) || (radiomask & ~validbits) || (radioval & ~validbits) ||
		    ((radioval & ~radiomask) != 0)) {
			WL_ERROR(("SET_RADIO with wrong bits 0x%x\n", val));
			bcmerror = BCME_RANGE;
			break;
		}

		new = (wlc->pub->radio_disabled & ~radiomask) | radioval;

		wlc->pub->radio_disabled = new;
		WL_MPC(("wl%d: SET_RADIO, radio_disable vector 0x%x\n", wlc->pub->unit,
			wlc->pub->radio_disabled));

		wlc_radio_hwdisable_upd(wlc);
		wlc_radio_upd(wlc);
		break;
	}

	case WLC_GET_PHYTYPE:
		*pval = WLC_PHYTYPE(wlc->band->phytype);
		break;

#if defined(BCMDBG)
	case WLC_GET_FIXRATE:
		if (!(scb = wlc_scbfind(wlc, bsscfg, (struct ether_addr *)arg))) {
			bcmerror = BCME_NOTFOUND;
		} else {
			struct bcmstrbuf b;
			bcm_binit(&b, (char*)arg, len);
			wlc_scb_ratesel_get_fixrate(wlc->wrsi, scb, &b);
		}
		break;

	case WLC_SET_FIXRATE: {
		link_val_t *link_val = (link_val_t*)arg;
		if (!(scb = wlc_scbfind(wlc, bsscfg, (struct ether_addr *)(&link_val->ea)))) {
			bcmerror = BCME_NOTFOUND;
		} else {
			if (wlc_scb_ratesel_set_fixrate(wlc->wrsi, scb, link_val->ac,
				link_val->val) < 0)
				bcmerror = BCME_BADARG;
		}
	}
		break;

	case WLC_DUMP_RATE:
		if (!(scb = wlc_scbfind(wlc, bsscfg, (struct ether_addr *)arg))) {
			bcmerror = BCME_NOTFOUND;
		} else {
			struct bcmstrbuf b;

			bcm_binit(&b, (char*)arg, len);
			wlc_scb_ratesel_scbdump(wlc->wrsi, scb, &b);
		}
		break;
#endif // endif

#ifdef BCMDBG
	/* deprecated, use wl ratesel_xxx iovar */
	case WLC_SET_RATE_PARAMS:
		bcmerror = BCME_NOTFOUND;
		break;
#endif /* BCMDBG */

	case WLC_GET_WEP_RESTRICT:
		*pval = bsscfg->wsec_restrict;
		break;

	case WLC_SET_WEP_RESTRICT:
		bsscfg->wsec_restrict = bool_val;
		break;

#ifdef WLLED
	case WLC_SET_LED:
		if (len < (int)sizeof(wl_led_info_t)) {
			bcmerror = BCME_BUFTOOSHORT;
			break;
		}
		wlc_led_set(wlc->ledh, (wl_led_info_t *)arg);
		break;
#endif // endif

	case WLC_GET_CLOSED:
		*pval = (bsscfg->closednet_nobcnssid & bsscfg->closednet_nobcprbresp);
		break;

	case WLC_SET_CLOSED:
		bcmerror = wlc_iovar_op(wlc, "closednet", NULL, 0, arg, len, IOV_SET, wlcif);
		break;

	case WLC_GET_CURR_RATESET: {
		wl_rateset_t *ret_rs = (wl_rateset_t *)arg;
		wlc_rateset_t *rs;

		if (bsscfg->associated)
			rs = &current_bss->rateset;
		else
			rs = &wlc->default_bss->rateset;

		if (len < (int)(rs->count + sizeof(rs->count))) {
			bcmerror = BCME_BUFTOOSHORT;
			break;
		}

		/* Copy only legacy rateset section */
		ret_rs->count = rs->count;
		bcopy(&rs->rates, &ret_rs->rates, rs->count);
		break;
	}

	case WLC_GET_RATESET: {
		wlc_rateset_t rs;
		wl_rateset_t *ret_rs = (wl_rateset_t *)arg;

		bzero(&rs, sizeof(wlc_rateset_t));
		wlc_default_rateset(wlc, (wlc_rateset_t*)&rs);

		if (len < (int)(rs.count + sizeof(rs.count))) {
			bcmerror = BCME_BUFTOOSHORT;
			break;
		}

		/* Copy only legacy rateset section */
		ret_rs->count = rs.count;
		bcopy(&rs.rates, &ret_rs->rates, rs.count);
		break;
	}

	case WLC_SET_RATESET: {
		wlc_rateset_t rs;
		wl_rateset_t *in_rs = (wl_rateset_t *)arg;

		if (len < (int)(in_rs->count + sizeof(in_rs->count))) {
			bcmerror = BCME_BUFTOOSHORT;
			break;
		}

		if (in_rs->count > WLC_NUMRATES) {
			bcmerror = BCME_BUFTOOLONG;
			break;
		}

		bzero(&rs, sizeof(wlc_rateset_t));

		/* Copy only legacy rateset section */
		rs.count = in_rs->count;
		bcopy(&in_rs->rates, &rs.rates, rs.count);

		/* merge rateset coming in with the current mcsset */
		if (N_ENAB(wlc->pub)) {
			if (bsscfg->associated)
				bcopy(&current_bss->rateset.mcs[0], rs.mcs, MCSSET_LEN);
			else
				bcopy(&wlc->default_bss->rateset.mcs[0], rs.mcs, MCSSET_LEN);
		}

#if defined(WL11AC)
		if (VHT_ENAB_BAND(wlc->pub, wlc->band->bandtype)) {
				rs.vht_mcsmap = (bsscfg->associated)
				? current_bss->rateset.vht_mcsmap
				: wlc->default_bss->rateset.vht_mcsmap;

				rs.vht_mcsmap_prop = (bsscfg->associated)
				? current_bss->rateset.vht_mcsmap_prop
				: wlc->default_bss->rateset.vht_mcsmap_prop;
		}
#endif /* WL11AC */

		bcmerror = wlc_set_rateset(wlc, &rs);

		break;
	}

	case WLC_GET_DESIRED_BSSID:
		bcopy((char*)wlc->default_bss->BSSID.octet, (char*)arg, ETHER_ADDR_LEN);
		break;

	case WLC_SET_DESIRED_BSSID:
		bcopy((char*)arg, (char*)wlc->default_bss->BSSID.octet, ETHER_ADDR_LEN);
		break;

	case WLC_GET_BCNPRD:
#ifdef STA
		if (BSSCFG_STA(bsscfg) && bsscfg->BSS && bsscfg->associated)
			*pval = current_bss->beacon_period;
		else
#endif // endif
			*pval = wlc->default_bss->beacon_period;
		break;

	case WLC_SET_BCNPRD:
		/* range [1, 0xffff] */
		if (val >= DOT11_MIN_BEACON_PERIOD && val <= DOT11_MAX_BEACON_PERIOD) {
			wlc->default_bss->beacon_period = (uint16) val;
		} else
			bcmerror = BCME_RANGE;
		break;

	case WLC_GET_DTIMPRD:
#ifdef STA
		if (BSSCFG_STA(bsscfg) && bsscfg->BSS && bsscfg->associated)
			*pval = current_bss->dtim_period;
		else
#endif // endif
			*pval = wlc->default_bss->dtim_period;
		break;

	case WLC_SET_DTIMPRD:
		/* range [1, 0xff] */
		if (val >= DOT11_MIN_DTIM_PERIOD && val <= DOT11_MAX_DTIM_PERIOD) {
			wlc->default_bss->dtim_period = (uint8)val;
		} else
			bcmerror = BCME_RANGE;
		break;

	case WLC_GET_REVINFO:
		bcmerror = wlc_get_revision_info(wlc, arg, (uint)len);
		break;

	case WLC_SET_AP: {
	        bool wasup = wlc->pub->up;
		wlc_bsscfg_type_t type = {BSSCFG_TYPE_GENERIC, BSSCFG_SUBTYPE_NONE};

#ifndef AP
		if (val)
			bcmerror = BCME_NOTAP;
#endif /* AP */
#ifndef STA
		if (!val)
			bcmerror = BCME_NOTSTA;
#endif /* STA */
		/* APSTA mode takes precedence */
		if (APSTA_ENAB(wlc->pub) && !val)
			bcmerror = BCME_EPERM;

		if (bcmerror || (AP_ENAB(wlc->pub) == bool_val))
			break;

		if (wasup) {
			WL_APSTA_UPDN(("wl%d: WLC_SET_AP -> wl_down()\n", wlc->pub->unit));
			wl_down(wlc->wl);
		}

		wlc->pub->_ap = bool_val;
		type.subtype = bool_val ? BSSCFG_GENERIC_AP : BSSCFG_GENERIC_STA;
		bcmerror = wlc_bsscfg_reinit(wlc, bsscfg, &type, 0);
		if (bcmerror)
			break;

	        wlc_ap_upd(wlc, bsscfg);

	        /* always turn off WET when switching mode */
	        wlc->wet = FALSE;
	        wlc->wet_dongle = FALSE;

	        /* always turn off MAC_SPOOF when switching mode */
	        wlc->mac_spoof = FALSE;

	        if (wasup) {
	                WL_APSTA_UPDN(("wl%d: WLC_SET_AP -> wl_up()\n", wlc->pub->unit));
	                bcmerror = wl_up(wlc->wl);
	        }
#ifdef STA
	        wlc_radio_mpc_upd(wlc);
#endif /* STA */
	        break;
	}

	case WLC_GET_AP:
	        *pval = (int)AP_ENAB(wlc->pub);
	        break;

	case WLC_GET_EAP_RESTRICT:
		bcmerror = wlc_iovar_op(wlc, "eap_restrict", NULL, 0, arg, len, IOV_GET, wlcif);
		break;

	case WLC_SET_EAP_RESTRICT:
		bcmerror = wlc_iovar_op(wlc, "eap_restrict", NULL, 0, arg, len, IOV_SET, wlcif);
		break;

	/* ioctl to handle and transmit authentication frame */
	case WLC_SCB_AUTHENTICATE: {
		dot11_management_header_t *mgmt = (dot11_management_header_t *)arg;
		bcmerror = wlc_send_mgmt_auth(wlc, bsscfg, mgmt, len);
		break;
	}

	/* ioctl to handle and transmit association frame */
	case WLC_SCB_ASSOCIATE: {
		dot11_management_header_t *mgmt = (dot11_management_header_t *)arg;
		bcmerror = wlc_send_mgmt_assoc(wlc, bsscfg, mgmt, len);
		break;
	}
	case WLC_SCB_DEAUTHENTICATE:
		/* Supply a reason in val */
		val = DOT11_RC_INACTIVITY;
		/* Fall through */

	case WLC_SCB_DEAUTHENTICATE_FOR_REASON: {
		void *eaptr = arg;

		if (cmd == WLC_SCB_DEAUTHENTICATE_FOR_REASON) {
			/* point arg at MAC addr */
			if (len < (int)sizeof(scb_val_t)) {
				bcmerror = BCME_BUFTOOSHORT;
				break;
			}
			eaptr = &((scb_val_t *)arg)->ea;
			/* reason stays in `val' */
			val = ((scb_val_t*)arg)->val ? ((scb_val_t*)arg)->val : val;
		}
#ifdef STA
		if (BSSCFG_STA(bsscfg)) {
			struct ether_addr *apmac = (struct ether_addr *)eaptr;
			if (ETHER_ISMULTI(apmac)) {
				WL_ERROR(("wl%d: bc/mc deauth%s on STA BSS?\n", wlc->pub->unit,
					(cmd == WLC_SCB_DEAUTHENTICATE) ? "" : "_reason"));
				apmac = &bsscfg->BSSID;
			} else if ((scb = wlc_scbfind(wlc, bsscfg, (struct ether_addr *)eaptr)) &&
			           (scb->flags & SCB_MYAP))
				wlc_scb_resetstate(wlc, scb);
			if (bsscfg->BSS) {
				wlc_disassociate_client(bsscfg, FALSE);
				if (scb)
					(void)wlc_senddeauth(wlc, bsscfg, scb, apmac, apmac,
					                     &bsscfg->cur_etheraddr, (uint16)val);
				WL_APSTA_UPDN(("wl%d: SCB DEAUTH: wlc_bsscfg_disable()\n",
				            wlc->pub->unit));
				wlc_bsscfg_disable(wlc, bsscfg);
			}
			break;
		}
#endif /* STA */
		BCM_REFERENCE(eaptr);
	}
		/* fall thru */
	case WLC_SCB_AUTHORIZE:
	case WLC_SCB_DEAUTHORIZE: {
		uint32 flag;
		int rc;
		bool enable = (cmd == WLC_SCB_AUTHORIZE);

		WL_ASSOC_LT(("%s", enable ? "iov:SCB_AUTH\n" : "iov:SCB_DEAUTH\n"));
		if (cmd == WLC_SCB_DEAUTHENTICATE_FOR_REASON) {
			/* point arg at MAC addr */
			if (len < (int)sizeof(scb_val_t)) {
				bcmerror = BCME_BUFTOOSHORT;
				break;
			}
			arg = &((scb_val_t*)arg)->ea;
		}

		rc = val;
		/* ethernet address must be specified - otherwise we assert in dbg code  */
		if (!arg) {
			bcmerror = BCME_BADARG;
			break;
		}

		if (cmd == WLC_SCB_AUTHORIZE || cmd == WLC_SCB_DEAUTHORIZE)
			flag = AUTHORIZED;
		else
			flag = AUTHENTICATED;

		/* (de)authorize/authenticate all stations in this BSS */
		if (ETHER_ISBCAST(arg)) {

			FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
				/* skip if the scb is not valid or is WDS */
				if (SCB_LEGACY_WDS(scb))
					continue;

				/* don't authorize an unassociated scb */
				if (flag == AUTHORIZED && !SCB_ASSOCIATED(scb))
					continue;

				/* don't deauthenticate an unauthenticated scb */
				if (flag == AUTHENTICATED && !SCB_AUTHENTICATED(scb))
					continue;

				wlc_scb_set_auth(wlc, bsscfg, scb, enable, flag, rc);
			}
		}
		/* (de)authorize/authenticate a single station */
		else if ((scb = wlc_scbfind(wlc, bsscfg, (struct ether_addr *) arg))) {
			ASSERT(SCB_BSSCFG(scb) == bsscfg);
			wlc_scb_set_auth(wlc, bsscfg, scb, enable, flag, rc);
#ifdef DWDS
			/* Create dwds interface for this authorized scb if security
			 * is enabled and security is using AES or TKIP ciphers.
			 */
			if (enable && ((DWDS_ENAB(bsscfg) && SCB_DWDS_CAP(scb)) ||
			    (MAP_ENAB(bsscfg) && SCB_MAP_CAP(scb))) && SCB_ASSOCIATED(scb) &&
			    BSSCFG_AP(bsscfg) && (cmd == WLC_SCB_AUTHORIZE) &&
			    (WSEC_TKIP_ENABLED(bsscfg->wsec) || WSEC_AES_ENABLED(bsscfg->wsec))) {
				wlc_wds_create(wlc, scb, WDS_DYNAMIC);
			}
#endif /* DWDS */
		}
	}
		break;

#ifdef STA
	case WLC_GET_ATIM:
		if (bsscfg->associated)
			*pval = (int) current_bss->atim_window;
		else
			*pval = (int) wlc->default_bss->atim_window;
		break;

	case WLC_SET_ATIM:
		wlc->default_bss->atim_window = (uint32) val;
		break;
#endif /* STA */

	case WLC_GET_PKTCNTS: {
		get_pktcnt_t *pktcnt = (get_pktcnt_t*)pval;
		if (WLC_UPDATE_STATS(wlc))
			wlc_statsupd(wlc);
		pktcnt->rx_good_pkt = WLCNTVAL(wlc->pub->_cnt->rxframe);
		pktcnt->rx_bad_pkt = WLCNTVAL(wlc->pub->_cnt->rxerror);
		pktcnt->tx_good_pkt = WLCNTVAL(wlc->pub->_cnt->txfrmsnt);
		pktcnt->tx_bad_pkt = WLCNTVAL(wlc->pub->_cnt->txerror) +
			WLCNTVAL(wlc->pub->_cnt->txfail);
		if (len >= (int)sizeof(get_pktcnt_t)) {
			/* Be backward compatible - only if buffer is large enough  */
			pktcnt->rx_ocast_good_pkt =
				WLCNTVAL(MCSTVAR(wlc->pub, rxmgocast));
		}
		break;
	}

	case WLC_GET_WSEC:
		bcmerror = wlc_iovar_op(wlc, "wsec", NULL, 0, arg, len, IOV_GET, wlcif);
		break;

	case WLC_SET_WSEC:
		bcmerror = wlc_iovar_op(wlc, "wsec", NULL, 0, arg, len, IOV_SET, wlcif);
		break;

	case WLC_GET_WPA_AUTH:
		*pval = (int) bsscfg->WPA_auth;
		break;

	case WLC_SET_WPA_AUTH:
		bcmerror = wlc_iovar_op(wlc, "wpa_auth", NULL, 0, arg, len, IOV_SET, wlcif);
		break;

	case WLC_GET_BSS_INFO: {
		wl_bss_info_t *wl_bi;
		int datalen = sizeof(uint32) + sizeof(wl_bss_info_t);
		struct scb *local_scb;

		if (val > (int)len)
			val = (int)len;
		if (val < datalen) {
			bcmerror = BCME_BUFTOOSHORT;
			break;
		}

		if (!wlc->pub->hw_off) {
			/* Update the noise value of the current_bss structure */
			current_bss->phy_noise = wlc_lq_noise_ma_upd(wlc,
				phy_noise_avg(pi));
		}

		/* convert the wlc_bss_info_t, writing into buffer */
		wl_bi = (wl_bss_info_t*)((char*)arg + sizeof(uint32));
		wlc_bss2wl_bss(wlc, current_bss, wl_bi, val - sizeof(uint32), FALSE);
		wlc_update_current_bi_caps(wlc, wl_bi);

		if (BSSCFG_STA(bsscfg)) {
			local_scb = wlc_scbfindband(wlc, bsscfg, &current_bss->BSSID,
				CHSPEC_WLCBANDUNIT(current_bss->chanspec));
			if (local_scb != NULL) {
				wl_bi->vht_mcsmap = local_scb->rateset.vht_mcsmap;
				wl_bi->vht_mcsmap_prop = local_scb->rateset.vht_mcsmap_prop;
			}
		}

		/* For associated infra STAs, report some interworking IE information */
		if (BSSCFG_INFRA_STA(bsscfg) &&
		    bsscfg->associated && (bsscfg->assoc->state == AS_IDLE))
		{
			wl_bi->bcnflags |= WL_BSS_BCNFLAGS_INTERWORK_PRESENT_VALID;
			if (current_bss->bcnflags & WLC_BSS_INTERWORK_PRESENT) {
				wl_bi->bcnflags |= WL_BSS_BCNFLAGS_INTERWORK_PRESENT;
				wl_bi->accessnet = current_bss->accessnet;
			}
		}

		/* If the requested BSS config is an AP, get the SSID and BSSID
		 * from that configuration
		 */
		*pval = (int)datalen + wl_bi->ie_length; /* byte count for return */

		if (BSSCFG_AP(bsscfg)) {
			bzero(wl_bi->SSID, sizeof(wl_bi->SSID));
			wl_bi->SSID_len	= bsscfg->SSID_len;
			bcopy(bsscfg->SSID, wl_bi->SSID, wl_bi->SSID_len);
			bcopy(&bsscfg->BSSID, &wl_bi->BSSID, sizeof(wl_bi->BSSID));
		}
		break;
	}
	case WLC_GET_BANDLIST:
		/* count of number of bands, followed by each band type */
		*pval++ = NBANDS(wlc);
		*pval++ = wlc->band->bandtype;
		if (NBANDS(wlc) > 1) {
			*pval++ = wlc->bandstate[OTHERBANDUNIT(wlc)]->bandtype;
		}
		break;

	case WLC_GET_BAND:
		*pval = wlc->bandlocked ? wlc->band->bandtype : WLC_BAND_AUTO;
		break;

	case WLC_SET_BAND:
		bcmerror = wlc_bandlock(wlc, (uint)val);
#ifdef WLRSDB
		if (RSDB_ENAB(wlc->pub) && (!bcmerror)) {
			wlc_rsdb_config_auto_mode_switch(wlc, WLC_RSDB_AUTO_OVERRIDE_BANDLOCK, val);
		}
#endif /* WLRSDB */
		break;

	case WLC_GET_PHYLIST:
	{
		uchar *cp = arg;
		uint phy_type;

		if (len < 3) {
			bcmerror = BCME_BUFTOOSHORT;
			break;
		}

		if (NBANDS(wlc) == 1) {
			phy_type = wlc->band->phytype;
			*cp++ = (phy_type == PHY_TYPE_N) ? 'n' :
			        (phy_type == PHY_TYPE_AC) ? 'v' :
			        (phy_type == PHY_TYPE_LCN) ? 'c' :
				'?';
		} else if (WLCISACPHY(wlc->band)) {
			*cp++ = 'v';
		} else if (WLCISNPHY(wlc->band)) {
			*cp++ = 'n';
		} else {
			*cp++ = 'g';
			*cp++ = 'a';
		}
		*cp = '\0';
		break;
	}

	case WLC_GET_SHORTSLOT:
		*pval = wlc->shortslot;
		break;

	case WLC_GET_SHORTSLOT_OVERRIDE:
		*pval = wlc->shortslot_override;
		break;

	case WLC_SET_SHORTSLOT_OVERRIDE:
		if ((val != WLC_SHORTSLOT_AUTO) &&
		    (val != WLC_SHORTSLOT_OFF) &&
		    (val != WLC_SHORTSLOT_ON)) {
			bcmerror = BCME_RANGE;
			break;
		}

		wlc->shortslot_override = (int8)val;

		/* shortslot is an 11g feature, so no more work if we are
		 * currently on the 5G band
		 */
		if (BAND_5G(wlc->band->bandtype))
			break;

		if (wlc->pub->up && wlc->pub->associated) {
			/* let watchdog or beacon processing update shortslot */
		} else if (wlc->pub->up) {
			/* unassociated shortslot is off */
			wlc_switch_shortslot(wlc, FALSE);
		} else {
			/* driver is down, so just update the wlc_info value */
			if (wlc->shortslot_override == WLC_SHORTSLOT_AUTO) {
				wlc->shortslot = FALSE;
			} else {
				wlc->shortslot = (wlc->shortslot_override == WLC_SHORTSLOT_ON);
			}
		}
		break;

	case WLC_GET_GMODE:
		if (wlc->band->bandtype == WLC_BAND_2G)
			*pval = wlc->band->gmode;
		else if (NBANDS(wlc) > 1)
			*pval = wlc->bandstate[OTHERBANDUNIT(wlc)]->gmode;
		break;

	case WLC_SET_GMODE:
		if (!wlc->pub->associated) {
			bcmerror = wlc_set_gmode(wlc, (uint8)val, TRUE);
		} else {
			bcmerror = BCME_ASSOCIATED;
			break;
		}
		break;

	case WLC_SET_SUP_RATESET_OVERRIDE: {
		wlc_rateset_t rs, new;

		/* copyin */
		if (len < (int)sizeof(wl_rateset_t)) {
			bcmerror = BCME_BUFTOOSHORT;
			break;
		}
		STATIC_ASSERT(sizeof(wl_rateset_t) <= sizeof(wlc_rateset_t));

		bzero(&rs, sizeof(wlc_rateset_t));
		bcopy((char*)arg, (char*)&rs, sizeof(wl_rateset_t));

		/* check for bad count value */
		if (rs.count > WLC_NUMRATES) {
			bcmerror = BCME_BADRATESET;	/* invalid rateset */
			break;
		}

		/* this command is only appropriate for gmode operation */
		if (!(wlc->band->gmode ||
		      ((NBANDS(wlc) > 1) && wlc->bandstate[OTHERBANDUNIT(wlc)]->gmode))) {
			bcmerror = BCME_BADBAND;	/* gmode only command when not in gmode */
			break;
		}

		/* check for an empty rateset to clear the override */
		if (rs.count == 0) {
			bzero(&wlc->sup_rates_override, sizeof(wlc_rateset_t));
			break;
		}

		bzero(&new, sizeof(wlc_rateset_t));
		/* validate rateset by comparing pre and post sorted against 11g hw rates */
		wlc_rateset_filter(&rs /* src */, &new /* dst */, FALSE, WLC_RATES_CCK_OFDM,
		                   RATE_MASK, wlc_get_mcsallow(wlc, NULL));
		wlc_rate_hwrs_filter_sort_validate(&new /* [in+out] */, &cck_ofdm_rates /* [in] */,
			FALSE, wlc->stf->op_txstreams);
		if (rs.count != new.count) {
			bcmerror = BCME_BADRATESET;	/* invalid rateset */
			break;
		}

		/* apply new rateset to the override */
		bcopy((char*)&new, (char*)&wlc->sup_rates_override, sizeof(wlc_rateset_t));

		/* update bcn and probe resp if needed */
		if (wlc->pub->up && AP_ENAB(wlc->pub) && wlc->pub->associated) {
			WL_APSTA_BCN(("wl%d: Calling update from SET_SUP_RATESET_OVERRIDE\n",
			            WLCWLUNIT(wlc)));
			WL_APSTA_BCN(("wl%d: SET_SUP_RATESET_OVERRIDE -> wlc_update_beacon()\n",
			            WLCWLUNIT(wlc)));
			wlc_update_beacon(wlc);
			wlc_update_probe_resp(wlc, TRUE);
		}
		break;
	}

	case WLC_GET_SUP_RATESET_OVERRIDE:
		/* this command is only appropriate for gmode operation */
		if (!(wlc->band->gmode ||
		      ((NBANDS(wlc) > 1) && wlc->bandstate[OTHERBANDUNIT(wlc)]->gmode))) {
			bcmerror = BCME_BADBAND;	/* gmode only command when not in gmode */
			break;
		}
		if (len < (int)sizeof(wl_rateset_t)) {
			bcmerror = BCME_BUFTOOSHORT;
			break;
		}
		bcopy((char*)&wlc->sup_rates_override, (char*)arg, sizeof(wl_rateset_t));

		break;

	case WLC_GET_PRB_RESP_TIMEOUT:
		*pval = wlc->prb_resp_timeout;
		break;

	case WLC_SET_PRB_RESP_TIMEOUT:
		if (wlc->pub->up) {
			bcmerror = BCME_NOTDOWN;
			break;
		}
		if (val < 0 || val >= 0xFFFF) {
			bcmerror = BCME_RANGE; /* bad value */
			break;
		}
		wlc->prb_resp_timeout = (uint16)val;
		break;

#ifndef OPENSRC_IOV_IOCTL
	case WLC_SET_COUNTRY:
		bcmerror = wlc_iovar_op(wlc, "country", NULL, 0, arg, len, IOV_SET, wlcif);
		break;
#endif /* OPENSRC_IOV_IOCTL */

	case WLC_GET_COUNTRY:
		if (len < WLC_CNTRY_BUF_SZ) {
			bcmerror = BCME_BUFTOOSHORT;
			break;
		}
		bcopy(wlc_channel_country_abbrev(wlc->cmi), (char*)arg, WLC_CNTRY_BUF_SZ);
		break;

	/* NOTE that this only returns valid 20MHZ channels */
	case WLC_GET_VALID_CHANNELS: {
		wl_uint32_list_t *list = (wl_uint32_list_t *)arg;
		uint8 channels[MAXCHANNEL];
		uint count;

		/* make sure the io buffer has at least room for a uint32 count */
		if (len < (int)sizeof(uint32)) {
			bcmerror = BCME_BUFTOOSHORT;
			break;
		}
		/* trim count to the buffer size in case it is bogus */
		if (list->count > (len - sizeof(uint32))/sizeof(uint32))
			list->count = (len - sizeof(uint32))/sizeof(uint32);

		/* find all valid channels */
		count = 0;
		for (i = 0; i < MAXCHANNEL; i++) {
			if (wlc_valid_chanspec_db(wlc->cmi, CH20MHZ_CHSPEC(i)))
				channels[count++] = (uint8)i;
		}

		/* check for buffer size */
		if (list->count < count) {
			/* too short, need this much */
			list->count = count;
			bcmerror = BCME_BUFTOOSHORT;
			break;
		}

		/* return the valid channels */
		for (i = 0; i < count; i++) {
			list->element[i] = channels[i];
		}
		list->count = count;
		break;
	}

	case WLC_GET_CHANNELS_IN_COUNTRY:
		bcmerror = wlc_get_channels_in_country(wlc, arg);
		break;

	case WLC_GET_COUNTRY_LIST:
		bcmerror = wlc_get_country_list(wlc, arg);
		break;

	case WLC_SET_BAD_FRAME_PREEMPT:
		wlc->pub->bf_preempt_4306 = bool_val;
		break;

	case WLC_GET_BAD_FRAME_PREEMPT:
		*pval = wlc->pub->bf_preempt_4306 ? 1 : 0;
		break;
#ifdef WET
	case WLC_GET_WET:
		bcmerror = wlc_iovar_op(wlc, "wet", NULL, 0, arg, len, IOV_GET, wlcif);
		break;

	case WLC_SET_WET:
		bcmerror = AP_ONLY(wlc->pub) ?
		        BCME_BADARG :
		        wlc_iovar_op(wlc, "wet", NULL, 0, arg, len, IOV_SET, wlcif);
		break;
#endif	/* WET */

#ifndef BCM_RECLAIM_INIT_FN_DATA
	case WLC_INIT:
		if (!wlc->pub->up) {
			bcmerror = BCME_NOTUP;
			break;
		}

		if (wlc->hw->need_reinit == WL_REINIT_RC_NONE) {
			wlc->hw->need_reinit = WL_REINIT_RC_USER_FORCED;
			wlc_fatal_error(wlc);
		} else {
			WL_ERROR(("wl%d:%s: need_reinit set and inprogress %d\n",
				wlc->pub->unit, __FUNCTION__, wlc->hw->need_reinit));
			bcmerror = BCME_NOTREADY;
		}
		break;
#endif /* !BCM_RECLAIM_INIT_FN_DATA */

	case WLC_SET_WSEC_PMK:
#if defined(BCMSUP_PSK) || defined(WLFBT)
		/* Use the internal supplicant if it is enabled else use the FBT one */
#ifdef BCMSUP_PSK
		if (SUP_ENAB(wlc->pub) && BSSCFG_STA(bsscfg) &&
			BSS_SUP_ENAB_WPA(wlc->idsup, bsscfg)) {
			bcmerror = wlc_sup_set_pmk(wlc->idsup, bsscfg, (wsec_pmk_t *)pval,
				bsscfg->associated);
			break;
		} else
#endif /* BCMSUP_PSK */
#ifdef WLFBT
		if (BSSCFG_IS_FBT(bsscfg) && BSSCFG_STA(bsscfg)) {
			bcmerror = wlc_fbt_set_pmk(wlc->fbt, bsscfg, (wsec_pmk_t *)pval,
					bsscfg->associated);
			break;
		}
#endif /* WLFBT */
#endif /* BCMSUP_PSK || WLFBT */
#ifdef BCMAUTH_PSK
		if (BCMAUTH_PSK_ENAB(wlc->pub) && BSSCFG_AP(bsscfg)) {
			bcmerror = wlc_auth_set_pmk(wlc->authi, bsscfg, (wsec_pmk_t *)pval);
			break;
		}
#endif /* BCMAUTH_PSK */
		bcmerror = BCME_UNSUPPORTED;
		break;

	case WLC_GET_BANDSTATES: {
		wl_band_t *wl_band = (wl_band_t*) pval;
		for (i = 0; i < MAXBANDS; i++, wl_band++) {
			wl_band->bandtype = (uint16) wlc->bandstate[i]->bandtype;
			wl_band->bandunit = (uint16) wlc->bandstate[i]->bandunit;
			wl_band->phytype = (uint16) wlc->bandstate[i]->phytype;
			wl_band->phyrev = (uint16) wlc->bandstate[i]->phyrev;
		}
		break;
	}
	case WLC_GET_WLC_BSS_INFO:
		memcpy(pval, current_bss, sizeof(wlc_bss_info_t));
		break;

	case WLC_GET_BSS_BCN_TS:
		if (wlc->pub->associated && current_bss && current_bss->bcn_prb) {
			memcpy(pval, current_bss->bcn_prb->timestamp,
				sizeof(current_bss->bcn_prb->timestamp));
		} else {
			bcmerror = BCME_NOTASSOCIATED;
		}
		break;
	case WLC_GET_BSS_WPA_RSN:
		memcpy(pval, &current_bss->wpa, sizeof(current_bss->wpa));
		break;
	case WLC_GET_BSS_WPA2_RSN:
		memcpy(pval, &current_bss->wpa2, sizeof(current_bss->wpa2));
		break;

#if defined(WL_EXPORT_CURPOWER)
	case WLC_CURRENT_PWR:
		bcmerror = wlc_iovar_op(wlc, "curpower", NULL, 0, arg, len, IOV_GET, wlcif);
		break;
	case WLC_CURRENT_TXCTRL:
		bcmerror = wlc_iovar_op(wlc, "curtxctrl", NULL, 0, arg, len, IOV_GET, wlcif);
		break;
#endif // endif

	case WLC_TERMINATED:
		si_watchdog_ms(wlc->pub->sih, 10);
		while (1);
		break;

	case WLC_LAST:
	default:
		/* returns BCME_UNSUPPORTED */
		bcmerror = BCME_UNSUPPORTED;
		break;
	}

	return (bcmerror);
} /* wlc_doioctl */

/**
 * register iovar table, watchdog and down handlers.
 * calling function must keep 'iovars' until wlc_module_unregister is called.
 * 'iovar' must have the last entry's name field being NULL as terminator.
 */
#if defined(WLC_PATCH_IOCTL)
int
BCMATTACHFN(wlc_module_register_ex)(wlc_pub_t *pub, const bcm_iovar_t *iovars,
	const char *name, void *hdl, wlc_iov_disp_fn_t i_fn,
	watchdog_fn_t w_fn, up_fn_t u_fn, down_fn_t d_fn,
	const bcm_iovar_t *patch_iovt, wlc_iov_disp_fn_t patch_disp_fn)
#else
int
BCMATTACHFN(wlc_module_register)(wlc_pub_t *pub, const bcm_iovar_t *iovars,
	const char *name, void *hdl, wlc_iov_disp_fn_t i_fn,
	watchdog_fn_t w_fn, up_fn_t u_fn, down_fn_t d_fn)
#endif // endif
{
	wlc_info_t *wlc = (wlc_info_t *)pub->wlc;
	int i, found;
	int err;

	/* separate iovar table/dispatcher registration */
	if (i_fn != NULL && iovars != NULL &&
	    (err = wlc_iocv_high_register_iovt(wlc->iocvi, iovars, i_fn,
#ifdef WLC_PATCH_IOCTL
			patch_iovt, patch_disp_fn,
#endif // endif
			hdl)) != BCME_OK) {
		return err;
	}

	ASSERT(name != NULL);

	/* ignore all NULL pointers */
	if (w_fn == NULL && u_fn == NULL && d_fn == NULL) {
		return BCME_OK;
	}

	/* find if module already registered */
	found = wlc_module_find(wlc, name);
	if (found >= 0) {
		/* MUST be a call to register new instance */
		wlc->modulecb_data[found].hdl = hdl;
		wlc->modulecb[found].ref_cnt++;
		return BCME_OK;
	}
	/* find an empty entry and just add, no duplication check! */
	for (i = 0; i < wlc->pub->max_modules; i ++) {
		if (wlc->modulecb[i].name[0] == '\0') {
			strncpy(wlc->modulecb[i].name, name, sizeof(wlc->modulecb[i].name) - 1);
			wlc->modulecb[i].watchdog_fn = w_fn;
			wlc->modulecb[i].up_fn = u_fn;
			wlc->modulecb[i].down_fn = d_fn;
			wlc->modulecb[i].ref_cnt++;
			wlc->modulecb_data[i].hdl = hdl;
			return BCME_OK;
		}
	}

	WL_ERROR(("wl%d: %s : WLC_MAXMODULES exceeded!", wlc->pub->unit, __FUNCTION__));

	/* it is time to increase the capacity */
	ASSERT(i < wlc->pub->max_modules);
	return BCME_NORESOURCE;
} /* WLC_MODULE_REGISTER */

/** unregister module callbacks */
int
BCMATTACHFN(wlc_module_unregister)(wlc_pub_t *pub, const char *name, void *hdl)
{
	wlc_info_t *wlc;
	int i;

	if (pub == NULL)
		return BCME_NOTFOUND;

	wlc = (wlc_info_t *)pub->wlc;
	if (wlc == NULL)
		return BCME_NOTFOUND;

	ASSERT(name != NULL);

	for (i = 0; i < wlc->pub->max_modules; i ++) {
		if (!strcmp(wlc->modulecb[i].name, name) &&
		    (wlc->modulecb_data[i].hdl == hdl)) {
			wlc->modulecb[i].ref_cnt--;
			if (wlc->modulecb[i].ref_cnt == 0) {
				bzero(&wlc->modulecb[i], sizeof(modulecb_t));
				bzero(&wlc->modulecb_data[i], sizeof(modulecb_data_t));
			}
			return 0;
		}
	}

	/* table not found! */
	return BCME_NOTFOUND;
}

static int
BCMNMIATTACHFN(wlc_module_find)(wlc_info_t *wlc, const char *name)
{
	int i;
	for (i = 0; i < wlc->pub->max_modules; i ++) {
		if (!strcmp(wlc->modulecb[i].name, name)) {
			return i;
		}
	}

	/* not registered yet */
	return BCME_NOTFOUND;
}

/** Return transmit packets held by the driver per BSS
 *  which is expected to take current driver TX bandwidth.
 */
uint
wlc_txpktcnt_bss(struct wlc_info *wlc, wlc_bsscfg_t *bsscfg)
{
	uint pktcnt = 0;
	wlc_txq_info_t *qi = wlc->active_queue;

#ifdef WLAMSDU_TX
	pktcnt += wlc_amsdu_bss_txpktcnt(wlc->ami, bsscfg);
#endif /* WLAMSDU_TX */

#ifdef WLAMPDU
	pktcnt += wlc_ampdu_bss_txpktcnt(wlc->ampdu_tx, bsscfg);
#endif /* WLAMPDU */

	pktcnt += (uint)pktq_n_pkts_tot(WLC_GET_CQ(qi));

	/* Hardware FIFO */
	pktcnt += TXPKTPENDTOT(wlc);

	return pktcnt;

}

/** Return total transmit packets held by the driver */
uint
wlc_txpktcnt(struct wlc_info *wlc)
{
	uint pktcnt;
	int i;
	wlc_bsscfg_t *cfg;

	/* Software txmods */
	pktcnt = wlc_txmod_txpktcnt(wlc->txmodi);

	/* Hardware FIFO */
	pktcnt += TXPKTPENDTOT(wlc);

	/* For all BSSCFG */
	FOREACH_BSS(wlc, i, cfg) {
		pktcnt += wlc_bsscfg_tx_pktcnt(wlc->psqi, cfg);
	}

	return pktcnt;
}

#ifdef BCMDBG
#ifdef STA
static void
wlc_rpmt_timer_cb(void *arg)
{
	wlc_bsscfg_t *cfg = (wlc_bsscfg_t *)arg;
	uint32 to = 0;

	if (cfg->rpmt_n_st == 1) {
		wlc_set_pmstate(cfg, TRUE);
		to = cfg->rpmt_1_prd;
		cfg->rpmt_n_st = 0;
	} else if (cfg->rpmt_n_st == 0) {
		wlc_set_pmstate(cfg, FALSE);
		to = cfg->rpmt_0_prd;
		cfg->rpmt_n_st = 1;
	}
	if (to == 0)
		return;
	wlc_hrt_add_timeout(cfg->rpmt_timer, to, wlc_rpmt_timer_cb, cfg);
}
#endif /* STA */
#endif /* BCMDBG */

/**
 * handler for iovar table wlc_iovars.  IMPLEMENTATION NOTE: In order to avoid checking for get/set
 * in each iovar case, the switch statement maps the iovar id into separate get and set values. If
 * you add a new iovar to the switch you MUST use IOV_GVAL and/or IOV_SVAL in the case labels to
 * avoid conflict with another case.
 * Please use params for additional qualifying parameters.
 */
static int
wlc_doiovar(void *hdl, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif)
{
	wlc_info_t *wlc = hdl;
	wlc_phy_t  *pi	= WLC_PI(wlc);
	wlc_bsscfg_t *bsscfg;
	int err = 0;
	struct maclist *maclist;
	int32 int_val = 0;
	int32 int_val2 = 0;
	uint16 feat_flag;
	int32 *ret_int_ptr;
	bool bool_val;
	struct bcmstrbuf b;
#ifdef STA
	wlc_pm_st_t *pm;
#endif /* STA */
	wlc_bss_info_t *current_bss;
#ifdef DNGL_WD_KEEP_ALIVE
	uint32 delay;
#endif // endif

	BCM_REFERENCE(val_size);
	BCM_REFERENCE(feat_flag);

	/* update bsscfg w/provided interface context */
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

#ifdef STA
	pm = bsscfg->pm;
#endif // endif
	current_bss = bsscfg->current_bss;

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));
	if (p_len >= (int)sizeof(int_val) * 2)
		bcopy((void*)((uintptr)params + sizeof(int_val)), &int_val2, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	bool_val = (int_val != 0) ? TRUE : FALSE;

	/* Do the actual parameter implementation */
	switch (actionid) {

	case IOV_GVAL(IOV_QTXPOWER): {
		int8 qdbm;
		bool override;

		if ((err = wlc_phy_txpower_get(pi, &qdbm, &override)) != BCME_OK)
			return err;

		/* Return qdbm units */
		*ret_int_ptr = (uint8)qdbm | (override ? WL_TXPWR_OVERRIDE : 0);
		break;
	}

	/* As long as override is false, this only sets the *user* targets.
	   User can twiddle this all he wants with no harm.
	   wlc_phy_txpower_set() explicitly sets override to false if
	   not internal or test.
	*/
	case IOV_SVAL(IOV_QTXPOWER): {
		int8 qdbm;
		bool override;
		ppr_t *txpwr;
		ppr_ru_t *ru_txpwr = NULL;

		/* Remove override bit and clip to max power value */
		qdbm = (int8)(int_val & 0xff);
		/* Extract override setting */
		override = (int_val & WL_TXPWR_OVERRIDE) ? TRUE : FALSE;

#if defined WLTXPWR_CACHE
		wlc_phy_txpwr_cache_invalidate(phy_tpc_get_txpwr_cache(WLC_PI(wlc)));
#endif	/* WLTXPWR_CACHE */
		if ((txpwr = ppr_create(wlc->osh, PPR_CHSPEC_BW(wlc->chanspec))) == NULL) {
			break;
		}
		if ((ru_txpwr = ppr_ru_create(wlc->osh)) == NULL) {
			ppr_delete(wlc->osh, txpwr);
			break;
		}

		wlc_channel_reg_limits(wlc->cmi, wlc->chanspec, txpwr, ru_txpwr);
#ifdef WL11AX
		wlc_phy_set_ru_power_limits(WLC_PI(wlc), ru_txpwr);
#endif /* WL11AX */
		ppr_apply_max(txpwr, WLC_TXPWR_MAX);
		err = wlc_phy_txpower_set(pi, qdbm, override, txpwr);
		ppr_delete(wlc->osh, txpwr);
		ppr_ru_delete(wlc->osh, ru_txpwr);
		break;
	}

#ifdef WL11N
	case IOV_GVAL(IOV_ANTSEL_TYPE):
		*ret_int_ptr = (int32) wlc_antsel_antseltype_get(wlc->asi);
		break;
#endif /* WL11N */

	case IOV_GVAL(IOV_STA_INFO):
	{
		struct ether_addr *ea = (struct ether_addr *)params;
		sta_info_all_t *sta_info_all;
		sta_info_t *sta_info;
		int remaining_len = 0;
		struct scb_iter scbiter;
		struct scb *scb;

		if (ETHER_ISBCAST(ea)) {
			sta_info_all = (sta_info_all_t *)arg;

			sta_info_all->version = WL_STA_INFO_ALL_VER_1;
			sta_info_all->sta_info_ver = WL_STA_VER;
			sta_info_all->data_offset = sizeof(*sta_info_all);
			sta_info_all->count = 0;

			remaining_len  = (int)len - sizeof(*sta_info_all);
			if (remaining_len < sizeof(*sta_info)) {
				err = BCME_BUFTOOSHORT;
				break;
			}

			sta_info = (sta_info_t *)&(sta_info_all->data[0]);

			FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
				if ((err != BCME_OK) || remaining_len < sizeof(*sta_info)) {
					break;
				}
				err = wlc_sta_info(wlc, bsscfg, &scb->ea, sta_info, remaining_len);
				sta_info_all->count++;
				sta_info++;
				remaining_len -=  sizeof(*sta_info);
			}
			sta_info_all->length = (uint16)sizeof(*sta_info_all) +
				(sta_info_all->count * (uint16)sizeof(*sta_info));
		} else {
			err = wlc_sta_info(wlc, bsscfg, (struct ether_addr *)params, arg, len);
		}
		break;
	}
	case IOV_GVAL(IOV_STA_REPORT):
		err = wlc_sta_report(wlc, bsscfg, (struct ether_addr *)params, arg, len);
		break;

#ifdef AP
	case IOV_GVAL(IOV_STA_SUPP_CHAN):
		err = wlc_sta_supp_chan(wlc, bsscfg, (struct ether_addr *)params, arg, len);
		break;
#endif /* AP */

	case IOV_GVAL(IOV_CAP):
		if (wlc_cap(wlc, (char*)arg, len) == NULL)
			err = BCME_BUFTOOSHORT;
		break;

	case IOV_GVAL(IOV_WPA_AUTH):
		*((uint*)arg) = bsscfg->WPA_auth;
		break;

	case IOV_SVAL(IOV_WPA_AUTH):
#ifdef STA
		/* change of WPA_Auth modifies the PS_ALLOWED state */
		if (BSSCFG_STA(bsscfg)) {
			bool prev_psallowed = PS_ALLOWED(bsscfg);

			bsscfg->WPA_auth = int_val;
#ifdef WLFBT
			/* Reset the current state so as to do initial FT association
			 * on a wl join command
			 */
			wlc_fbt_reset_current_akm(wlc->fbt, bsscfg);
#endif /* WLFBT */
			if (prev_psallowed != PS_ALLOWED(bsscfg))
				wlc_set_pmstate(bsscfg, pm->PMenabled);

			/* Don't clear the WPA join preferences if they are already set */
			wlc_join_pref_reset_cond(bsscfg);
		} else
#endif	/* STA */
#ifdef AP
		if (BSSCFG_AP(bsscfg)) {
			if (bsscfg->up)
				err = BCME_UNSUPPORTED;
			else
				bsscfg->WPA_auth = int_val;
		} else
#endif	/* AP */
		bsscfg->WPA_auth = int_val;
		break;

	case IOV_GVAL(IOV_MALLOCED):
		*((uint*)arg) = MALLOCED(wlc->osh);
		break;

	case IOV_GVAL(IOV_PER_CHAN_INFO): {
		*ret_int_ptr = wlc_get_chan_info(wlc, (uint16)int_val);
		break;
	}

	case IOV_SVAL(IOV_PER_CHAN_INFO): {
		int ret;
		wl_set_chan_info_t *chan_info = (wl_set_chan_info_t *)params;
		if (p_len < sizeof(*chan_info)) {
			err = BCME_BUFTOOSHORT;
			break;
		}
		if ((ret = wlc_set_dfs_chan_info(wlc, chan_info)) != BCME_OK) {
			err = ret;
		}
		break;
	}

	case IOV_GVAL(IOV_WSEC_RESTRICT):
		*((uint*)arg) = bsscfg->wsec_restrict;
		break;

	case IOV_SVAL(IOV_WSEC_RESTRICT):
		bsscfg->wsec_restrict = bool_val;
		break;

	case IOV_GVAL(IOV_FRAGTHRESH):
		*((uint*)arg) = wlc->usr_fragthresh;
		break;

	case IOV_SVAL(IOV_FRAGTHRESH): {
		uint8 i;
		uint16 frag = (uint16)int_val;
#ifdef WLAMSDU_TX
		amsdu_info_t *ami = wlc->ami;
#endif /* WLAMSDU_TX */
		if (frag < DOT11_MIN_FRAG_LEN || frag > DOT11_MAX_FRAG_LEN) {
			err = BCME_RANGE;
			break;
		}

		wlc->usr_fragthresh = frag;
		for (i = 0; i < AC_COUNT; i++)
			wlc_fragthresh_set(wlc, i, frag);

#ifdef WLAMSDU_TX
		wlc_amsdu_agglimit_frag_upd(ami);
#endif /* WLAMSDU_TX */
		break;
	}

	/* Former offset-based items */
	case IOV_GVAL(IOV_RTSTHRESH):
		*ret_int_ptr = (int32)wlc->RTSThresh;
		break;

	case IOV_SVAL(IOV_RTSTHRESH):
		wlc->RTSThresh = (uint16)int_val;
		if (D11REV_GE(wlc->pub->corerev, 128)) {
			if (!wlc->clk) {
				err = BCME_NOCLK;
				break;
			}
			wlc_write_shm(wlc, M_RTS_MINLEN_L(wlc), (uint16)int_val);
		}
		break;

	case IOV_GVAL(IOV_D11_AUTH):
		*ret_int_ptr = DOT11AUTH2WLAUTH(bsscfg);
		break;

	case IOV_SVAL(IOV_D11_AUTH):
#if !defined(WL_SAE)
		if (int_val == WL_AUTH_SAE_KEY)
			return BCME_BADARG;
#endif /* WL_SAE */
		if (int_val == WL_AUTH_OPEN_SYSTEM ||
			int_val == WL_AUTH_SHARED_KEY) {
			bsscfg->auth = WLAUTH2DOT11AUTH(int_val);
			bsscfg->openshared = 0;
		} else if (int_val == WL_AUTH_SAE_KEY) {
			bsscfg->auth = (uint16)int_val;
		} else {
#ifdef WL_AUTH_SHARED_OPEN
			if (AUTH_SHARED_OPEN_ENAB(wlc->pub)) {
				bsscfg->auth = DOT11_SHARED_KEY;
			} else
#endif /* WL_AUTH_SHARED_OPEN */
			{
				bsscfg->auth = DOT11_OPEN_SYSTEM;
			}
			bsscfg->openshared = 1;
		}
		break;

	case IOV_SVAL(IOV_VLAN_MODE):
		if (int_val >= AUTO && int_val <= ON)
			wlc->vlan_mode = int_val;
		break;

	case IOV_GVAL(IOV_VLAN_MODE):
		*ret_int_ptr = wlc->vlan_mode;
		break;

	case IOV_GVAL(IOV_WLFEATUREFLAG):
		*ret_int_ptr = (int32)wlc->pub->wlfeatureflag;
		break;

	case IOV_SVAL(IOV_WLFEATUREFLAG):
		wlc->pub->wlfeatureflag = int_val;
		break;

#ifdef WET
	case IOV_GVAL(IOV_WET):
		*ret_int_ptr = (int32)wlc->wet;
		break;

	case IOV_SVAL(IOV_WET):
		if (!wlc->mac_spoof)
			wlc->wet = bool_val;
		else
			err = BCME_UNSUPPORTED;

#ifdef DPSTA
		if (WET_ENAB(wlc) && BSSCFG_STA(bsscfg)) {
			wl_dpsta_wet_register(wlc->wl, bsscfg);
		}
#endif /* DPSTA */
		break;
#endif /* WET */

#ifdef WET_DONGLE
	case IOV_GVAL(IOV_WET_DONGLE):
		*ret_int_ptr = (int32)wlc->wet_dongle;
		break;

	case IOV_SVAL(IOV_WET_DONGLE):
		if (!wlc->mac_spoof) {
			wlc->wet_dongle = bool_val;
		}
		else
			err = BCME_UNSUPPORTED;
		break;
#endif	/* WET_DONGLE */

#ifdef MAC_SPOOF
	case IOV_GVAL(IOV_MAC_SPOOF):
		*ret_int_ptr = (int32)wlc->mac_spoof;
		break;

	case IOV_SVAL(IOV_MAC_SPOOF):
		if (!WET_ENAB(wlc) && BSSCFG_STA(bsscfg) && !APSTA_ENAB(wlc->pub))
			wlc->mac_spoof = bool_val;
		else
			err = BCME_UNSUPPORTED;
		break;
#endif /* MAC_SPOOF */

	case IOV_GVAL(IOV_EAP_RESTRICT):
		*ret_int_ptr = (int32)bsscfg->eap_restrict;
		break;

	case IOV_SVAL(IOV_EAP_RESTRICT):
		bsscfg->eap_restrict = bool_val;
		break;
#if defined(BCMDBG) || defined(WLTEST)
	case IOV_GVAL(IOV_EIRP):
		*ret_int_ptr = wlc_channel_locale_flags(wlc->cmi) & WLC_EIRP;
		break;
#endif /* defined(BCMDBG) || defined(WLTEST) */

	case IOV_GVAL(IOV_CUR_ETHERADDR):
#if defined(WLSLOTTED_BSS)
		if (BSSCFG_SLOTTED_BSS(bsscfg)) {
			wlc_slotted_bss_cur_etheraddr(wlc->sbi, wlcif, arg);
		} else
#endif /* WLSLOTTED_BSS */
		bcopy(&bsscfg->cur_etheraddr, arg, ETHER_ADDR_LEN);
		break;

	case IOV_SVAL(IOV_CUR_ETHERADDR):
		/* XXX - We *could* allow this while up if:
		 * As a STA, we're not associated
		 * OR
		 * As an AP, no one is associated to this specific BSSconfig
		 * and if no one is associated, we'll need to update beacon
		 * and probe response templates(maybe some other things?)
		 * ALSO need to check for WDS - don't allow change if WDS
		 * link is active!!!
		 *
		 * For now, just call wlc_set_mac() and when we "UP" the
		 * bss config, the new MAC address will be used.
		 * If we want to allow changes when we're "not associated",
		 * how is that any better for the Application/User than
		 * being "down"?  It's definitely more work/complexity for us.
		 *
		 * It seems to me that a MAC address change is rare and a
		 * serious config change that warrants the requirement for us
		 * to be "down"
		 */
#if defined(WLSLOTTED_BSS)
		if (BSSCFG_SLOTTED_BSS(bsscfg)) {
			err = wlc_slotted_bss_update_etheraddr(wlc->sbi, wlcif, arg);
		} else
#endif /* WLSLOTTED_BSS */
#if defined(WLPKTENG) || defined(WLTEST)
		if (bsscfg->up) {
			/* allow change during e.g. pkteng mode, but not while BSS is up */
			err = BCME_NOTDOWN;
		} else
#endif /* WLPKTENG) || WLTEST */
		{
			err = wlc_validate_mac(wlc, bsscfg, arg);
		}
#ifdef WLRSDB
		/* MAC address being overrided by application. This should be applied
		 * to wlc-1's primary config also if we are applying to wlc0's primary.
		 * This is to ensure that both uses same primary cfg address after
		 * bsscfg clone.
		*/
		if (RSDB_ENAB(wlc->pub)) {
			wlc_info_t *other_wlc = wlc_rsdb_get_other_wlc(wlc);
			/* Only if we are updating primary cfg's address of current wlc,
			 * update primary of other wlc also.
			 */
			if (bsscfg == wlc->cfg) {
				err = wlc_validate_mac(other_wlc, other_wlc->cfg, arg);
			}
		}
#endif /* WLRSDB */

		break;

	case IOV_GVAL(IOV_PERM_ETHERADDR):
		bcopy(&wlc->perm_etheraddr, arg, ETHER_ADDR_LEN);
		break;
#ifdef BCMDBG
	case IOV_GVAL(IOV_RAND):
		(void)wlc_getrand(wlc, arg, sizeof(uint16));
		break;
#endif /* BCMDBG */
	case IOV_GVAL(IOV_MPC):
		*ret_int_ptr = (int32)wlc->mpc_mode;
		break;

	case IOV_SVAL(IOV_MPC):
		if (bool_val && !wlc_mpccap(wlc)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		if ((int_val < WLC_MPC_MODE_OFF) ||
#ifdef STA
			(int_val > WLC_MPC_MODE_2) ||
#else
			(int_val > WLC_MPC_MODE_1) ||
#endif /* STA */
			0)
				int_val = WLC_MPC_MODE_1;

		if (wlc->mpc_mode == int_val)
			break;
#ifdef STA
		if (wlc->mpc_mode) {
			wlc->mpc = FALSE;
			wlc_radio_mpc_upd(wlc);
		}
#endif /* STA */
		wlc->mpc_mode = (uint8)int_val;
		wlc->mpc = bool_val;
#ifdef STA
		wlc_radio_mpc_upd(wlc);

#endif /* STA */
		break;

#if defined(WLTEST)
	case IOV_GVAL(IOV_WATCHDOG_DISABLE):
		*ret_int_ptr = (int32)wlc->watchdog_disable;
		break;

	case IOV_SVAL(IOV_WATCHDOG_DISABLE):
		wlc->watchdog_disable = bool_val;
		break;
#endif // endif
#ifdef STA
	case IOV_GVAL(IOV_MPC_DUR):
		*ret_int_ptr = wlc_get_mpc_dur(wlc);
		break;

	case IOV_SVAL(IOV_MPC_DUR):
		wlc->mpc_dur = 0;
		wlc->mpc_laston_ts = OSL_SYSUPTIME(); /* need to do this when mpc radio is off */
		break;
#endif /* STA */

	case IOV_GVAL(IOV_BCMERRORSTR):
#ifdef BCMSPACE
		strncpy((char *)arg, bcmerrorstr(wlc->pub->bcmerror), BCME_STRLEN);
#else
		(void)snprintf((char *)arg, BCME_STRLEN, "error %d", wlc->pub->bcmerror);
#endif // endif
		break;

	case IOV_GVAL(IOV_CLMVER):
		bcm_binit(&b, arg, len);
		wlc_dump_clmver(wlc->cmi, &b);
		break;

	case IOV_GVAL(IOV_BCMERROR):
		*ret_int_ptr = (int32) wlc->pub->bcmerror;
		break;

#ifdef WLCNT
	case IOV_GVAL(IOV_COUNTERS):
		if (WLC_UPDATE_STATS(wlc)) {
			wlc_statsupd(wlc);
		}
		err = wlc_get_all_cnts(wlc, arg, len);
		break;

#endif /* WLCNT */

#if defined(DELTASTATS)
	case IOV_GVAL(IOV_DELTA_STATS_INTERVAL):
		if (DELTASTATS_ENAB(wlc->pub)) {
			*ret_int_ptr = wlc->delta_stats->interval;
		} else {
			err = BCME_NOTENABLED;
		}
		break;

	case IOV_SVAL(IOV_DELTA_STATS_INTERVAL):
		if (DELTASTATS_ENAB(wlc->pub)) {
			wlc->delta_stats->interval = int_val;
			if (wlc->delta_stats->interval > 0) {
				/* delta stats enabled */
				wlc->delta_stats->seconds = 0;
				wlc->delta_stats->current_index = 0;
			}
		} else {
			err = BCME_NOTENABLED;
		}
		break;

	case IOV_GVAL(IOV_DELTA_STATS):
		if (DELTASTATS_ENAB(wlc->pub)) {
			err = wlc_get_delta_stats(wlc, arg);
		} else {
			err = BCME_NOTENABLED;
		}
		break;
#endif /* DELTASTATS */

#if defined(MACOSX)
	case IOV_GVAL(IOV_TXFIFO_SZ): {
		wl_txfifo_sz_t *ts = (wl_txfifo_sz_t *)params;
		uint block_size = 0;

		if ((uint16)int_val != WL_TXFIFO_SZ_MAGIC) {
			WL_ERROR(("txfifo_sz: wl version don't match the driver\n"));
			err = BCME_VERSION;
			break;
		}

		err = wlc_bmac_xmtfifo_sz_get(wlc->hw, ts->fifo, &block_size);
		if (err)
			break;

		ts->size = (uint16)block_size;

		bcopy(ts, arg, sizeof(*ts));
		break;
	}

	case IOV_SVAL(IOV_TXFIFO_SZ): {
		wl_txfifo_sz_t *ts = (wl_txfifo_sz_t *)params;

		if ((uint16)int_val != WL_TXFIFO_SZ_MAGIC) {
			WL_ERROR(("txfifo_sz: wl version don't match the driver\n"));
			err = BCME_VERSION;
			break;
		}

		err = wlc_xmtfifo_sz_set(wlc, ts->fifo, ts->size);
		break;
	}
#endif // endif
	case IOV_SVAL(IOV_COUNTRY_LIST_EXTENDED):
		wlc->country_list_extended = bool_val;
		break;

	case IOV_GVAL(IOV_COUNTRY_LIST_EXTENDED):
		*ret_int_ptr = (int32)wlc->country_list_extended;
		break;

#ifdef WLCNT
	case IOV_SVAL(IOV_RESET_D11CNTS): {
		if (WLC_UPDATE_STATS(wlc)) {
			wl_cnt_wlc_t *cnt = wlc->pub->_cnt;
			wlc_statsupd(wlc);
			cnt->d11cnt_txrts_off += cnt->txrts;
			cnt->d11cnt_rxcrc_off += cnt->rxcrc;
			cnt->d11cnt_txnocts_off += cnt->txnocts;

			cnt->txframe = cnt->txfrag = cnt->txmulti = cnt->txfail =
			cnt->txerror = cnt->txserr = cnt->txretry = cnt->txretrie = cnt->rxdup =
			cnt->rxrtry = cnt->txrts = cnt->txnocts =
			cnt->txnoack = cnt->rxframe = cnt->rxerror = cnt->rxfrag = cnt->rxmulti =
			cnt->rxcrc = cnt->txfrmsnt = cnt->rxundec =
			cnt->rxundec_mcst = 0;

			WLCNTMCSTSET(wlc->pub, txbcnfrm, 0);
			WLCNTMCSTSET(wlc->pub, rxbeaconmbss, 0);

			if (wlcif) {
				bzero(wlcif->_cnt, sizeof(wl_if_stats_t));
				WLCNTSET(wlcif->_cnt->version, WL_IF_STATS_T_VERSION);
				WLCNTSET(wlcif->_cnt->length, sizeof(wl_if_stats_t));
			} else if (bsscfg->wlcif) {
				bzero(bsscfg->wlcif->_cnt, sizeof(wl_if_stats_t));
				WLCNTSET(bsscfg->wlcif->_cnt->version, WL_IF_STATS_T_VERSION);
				WLCNTSET(bsscfg->wlcif->_cnt->length, sizeof(wl_if_stats_t));
			}
		}
		break;
	}
	case IOV_GVAL(IOV_RESET_CNTS):
		if (WLC_UPDATE_STATS(wlc)) {
			if (wlc->clk)
				wlc_statsupd(wlc);

			/* Reset all counters and restore version/size */
			memset((char *)wlc->pub->_cnt, 0, sizeof(*wlc->pub->_cnt));
			memset(wlc->pub->_mcst_cnt, 0, WL_CNT_MCST_STRUCT_SZ);
#if defined(WL_PSMX)
			if (PSMX_ENAB(wlc->pub)) {
				memset(wlc->pub->_mcxst_cnt, 0, WL_CNT_MCXST_STRUCT_SZ);
			}
#endif /* WL_PSMX */

			if (wlcif) {
				bzero(wlcif->_cnt, sizeof(wl_if_stats_t));
				WLCNTSET(wlcif->_cnt->version, WL_IF_STATS_T_VERSION);
				WLCNTSET(wlcif->_cnt->length, sizeof(wl_if_stats_t));
			} else if (bsscfg->wlcif) {
				bzero(&(bsscfg->wlcif->_cnt), sizeof(wl_if_stats_t));
				WLCNTSET(bsscfg->wlcif->_cnt->version, WL_IF_STATS_T_VERSION);
				WLCNTSET(bsscfg->wlcif->_cnt->length, sizeof(wl_if_stats_t));
			}
			*ret_int_ptr = 0;
		}
		break;

	case IOV_GVAL(IOV_SUBCOUNTERS): {
		uint8 *cntr_base = (uint8 *)wlc->pub->_cnt;
		uint8 *mcst_cntr_base = (uint8 *)wlc->pub->_mcst_cnt;
		wl_subcnt_info_t *subcnt_request_info = (wl_subcnt_info_t *)params;
		wl_subcnt_info_t *subcnt_response_info = (wl_subcnt_info_t *)arg;
		uint16 num_counters = subcnt_request_info->num_subcounters;
		uint16 iov_version = subcnt_request_info->version;
		uint16 app_cntr_ver = subcnt_request_info->counters_version;
		uint16 length = subcnt_request_info->length;
		int32 index = 0;
		uint8 * reinitrsn_base = (uint8*)wlc->pub->reinitrsn;

		subcnt_response_info->version = WL_SUBCNTR_IOV_VER;
		subcnt_response_info->num_subcounters = num_counters;
		subcnt_response_info->counters_version = WL_CNT_T_VERSION;

		/* subcnt_response_info->length is uint16.  Check if num_counters
		* would cause subcnt_response_info->length overflow
		*/
		if (num_counters > ((0xFFFFu - OFFSETOF(wl_subcnt_info_t, data))/
			sizeof(subcnt_response_info->data[0]))) {
			err = BCME_BADARG;
			break;
		}

		subcnt_response_info->length = OFFSETOF(wl_subcnt_info_t, data) +
			num_counters * sizeof(subcnt_response_info->data[0]);

		if (iov_version != WL_SUBCNTR_IOV_VER) {
			err = BCME_VERSION;
			break;
		}

		if (app_cntr_ver != WL_CNT_T_VERSION) {
			/* If there is a version mismatch then return SUCCESS
			 * with num_subcounters as 0 to inform host that the
			 * request is not completed because of counter version mismatch.
			 */
			subcnt_response_info->num_subcounters = 0;
			break;
		}

		/* If param length is less than the expected IOVAR length or
		 * request and response length are not same or length (len) of
		 * arg output is less than response length then its an error condition.
		 */
		if ((p_len < subcnt_response_info->length) ||
				(length != subcnt_response_info->length) ||
				(len < subcnt_response_info->length))  {
			err = BCME_BADOPTION;
			break;
		}

		wlc_statsupd(wlc);

		for (index = 0; index < num_counters; index++) {
			/* As all the counters are 32 bit, the
			 * offset is always expected to be 4 byte aligned
			 * Check if offset is within bound or not.
			 */
			uint32 offset = subcnt_request_info->data[index];
			uint32 offset_upbd = sizeof(wl_cnt_wlc_t) +
			WL_CNT_MCST_STRUCT_SZ + sizeof(reinit_rsns_t);
			uint32 offset_lwbd = 0;
			if ((! ISALIGNED(offset, sizeof(uint32))) ||
					((offset > (offset_upbd - sizeof(uint32))) ||
					(offset < offset_lwbd))) {
				err = BCME_BADARG;
				break;
			} else {
				/* Check to validate is offset is for
				 * reinitrsn counters, mcst counters or wlc counters
				 */
				uint32 mcst_wlc_cntsz = sizeof(wl_cnt_wlc_t) +
					WL_CNT_MCST_STRUCT_SZ;
				if (offset >= mcst_wlc_cntsz) {
						subcnt_response_info->data[index] =
						*((uint32 *)(reinitrsn_base +
						(offset - mcst_wlc_cntsz)));
				}
				else if (offset >= sizeof(wl_cnt_wlc_t)) {
					subcnt_response_info->data[index] =
						*((uint32 *)(mcst_cntr_base +
						(offset - sizeof(wl_cnt_wlc_t))));
				} else {
					subcnt_response_info->data[index] =
						*((uint32 *)(cntr_base + offset));
				}
			}
		}
	}
		break;
#endif /* WLCNT */

	case IOV_GVAL(IOV_IBSS_ALLOWED):
		*ret_int_ptr = (int32)wlc->ibss_allowed;
		break;

	case IOV_SVAL(IOV_IBSS_ALLOWED):
		wlc->ibss_allowed = bool_val;
		break;

	case IOV_GVAL(IOV_MCAST_LIST):
		/* Check input buffer length */
		maclist = (struct maclist *)arg;
		if (len < (int)(sizeof(uint32) + bsscfg->nmulticast * ETHER_ADDR_LEN)) {
			err = BCME_BUFTOOSHORT;
			break;
		}
		*ret_int_ptr = bsscfg->nmulticast;

		/* Copy each address */
		bcopy((void *)(bsscfg->multicast), (void *)(maclist->ea),
			(bsscfg->nmulticast * ETHER_ADDR_LEN));

		break;

	case IOV_SVAL(IOV_MCAST_LIST):
		/* Store number of addresses in list */
		maclist = (struct maclist *)arg;
		bcopy(&maclist->count, &int_val, sizeof(uint32));
		if (int_val > MAXMULTILIST) {
			err = BCME_RANGE;
			break;
		}
		if (len < (int)(sizeof(uint32) + int_val * ETHER_ADDR_LEN)) {
			err = BCME_BUFTOOSHORT;
			break;
		}
		bsscfg->nmulticast = int_val;

		/* Copy each address */
		bcopy((void *)(maclist->ea), (void *)(bsscfg->multicast),
			(bsscfg->nmulticast * ETHER_ADDR_LEN));
		break;
#ifdef WLATF
	case IOV_GVAL(IOV_ATF):
#if defined(WLATF_DONGLE)
		if (ATFD_ENAB(wlc)) {
			uint32 status = 0;
			err = wlfc_get_fair_fetch_scheduling(wlc->wl, &status);
			if ((err == BCME_OK) && (status == 0)) {
				wlc->atf = WLC_AIRTIME_DISABLED;
			}
		}
#endif /* WLATF_DONGLE */
		*ret_int_ptr = wlc->atf;
		break;

	case IOV_SVAL(IOV_ATF):
		err = BCME_OK;
		/* Disable ATF_PMODE release for non AQM devices
		 * as the work to fix software tracking of the sequence numbers is not done
		 */
		if ((int_val == WLC_AIRTIME_PMODE) && !AMPDU_AQM_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}
#if defined(WLATF_DONGLE)
		  else {
			if (BCMPCIEDEV_ENAB()) {
				uint32 status = 0;

				/* Get status of Fair fetch scheduler */
				err = wlfc_get_fair_fetch_scheduling(wlc->wl, &status);
				if (err == BCME_OK) {
					/* Enable ATF if disabled */
					if (int_val == WLC_AIRTIME_DONGLE) {
						if (status == 0) {
							wlfc_enab_fair_fetch_scheduling(wlc->wl, 1);
						}
					/* Disable ATF if it is enabled and
					 * dongle mode is not specified
					 */
					} else {
						if (status == 1) {
							wlfc_enab_fair_fetch_scheduling(wlc->wl, 0);
						}
					}
				}
			}
#endif /* WLATF_DONGLE */
#if defined(WLATF) && defined(WLATF_PERC)
		if (int_val == WLC_AIRTIME_PERC) {
			wlc_ampdu_atf_set_default_mode(wlc->ampdu_tx, wlc->scbstate,
				TRUE);
			wlc->perc_max_last = 0;
		} else
#endif /* WLATF && WLATF_PERC */
		{
			wlc_ampdu_atf_set_default_mode(wlc->ampdu_tx, wlc->scbstate,
				(uint32)int_val);
		}
		wlc->atf = int_val;
#if defined(WLATF) && defined(WLATF_PERC)
		wlc->atm_perc = (int_val == WLC_AIRTIME_PERC) ? TRUE : FALSE;
#endif /* WLATF && WLATF_PERC */
		break;

#if defined(WLATF) && defined(WLATF_PERC)
	case IOV_GVAL(IOV_ATM_STAPERC):
	{
		wl_atm_staperc_t *staperc;
		struct scb *scb;

		if (len < sizeof(wl_atm_staperc_t))
			return BCME_BUFTOOSHORT;

		staperc = (wl_atm_staperc_t *)arg;

		memcpy(staperc, params, sizeof(wl_atm_staperc_t));

		if (&staperc->ea == NULL)
			return BCME_BADARG;

		scb = wlc_scbfind(wlc, bsscfg, &staperc->ea);
		if (!scb) {
			WL_ERROR(("wl%d %s: ERROR: NULL scb arg\n",
			wlc->pub->unit, __FUNCTION__));
			return BCME_BADARG;
		}
		staperc->perc = (uint8)scb->staperc;

		break;
	}

	case IOV_SVAL(IOV_ATM_STAPERC):
	{
		wl_atm_staperc_t *staperc;
		struct scb *scb;
		struct scb_iter scbiter;
		uint8 perc_sum = 0;

		staperc = (wl_atm_staperc_t *)params;

		if (&staperc->ea == NULL)
			return BCME_BADARG;

		FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
			perc_sum += scb->staperc;
		}

		scb = wlc_scbfind(wlc, bsscfg, &staperc->ea);
		if (!scb) {
			WL_ERROR(("wl%d: ERROR: NULL scb arg\n",
			wlc->pub->unit));
			return BCME_BADARG;
		}

		/* Check if new allocation to cause over 100 */
		if ((perc_sum - scb->staperc + staperc->perc) > 100) {
			WL_ERROR(("wl%d : ERROR: percentage of sum(%2d%%)"
				" + new STA perc(%2d%%) > 100!\n",
				wlc->pub->unit, perc_sum, staperc->perc));
			return BCME_BADARG;
		}

		scb->staperc = (uint32)staperc->perc;
		wlc_atm_update_perc(wlc);

		break;
	}

	case IOV_GVAL(IOV_ATM_BSSPERC):
	{
		*ret_int_ptr = (int32)bsscfg->bssperc;
		break;
	}

	case IOV_SVAL(IOV_ATM_BSSPERC):
	{
		wlc_bsscfg_t *cfg;
		int i;
		uint8 perc_sum = 0;

		FOREACH_UP_AP(wlc, i, cfg) {
			perc_sum += cfg->bssperc;
		}

		if ((perc_sum - bsscfg->bssperc + int_val) > 100) {
			WL_ERROR(("wl%d.%d : ERROR: percentage of sum(%2d%%)"
				" + new BSS perc(%2d%%) > 100!\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg),
				perc_sum, int_val));
			return BCME_BADARG;
		}

		bsscfg->bssperc = (uint8)int_val;
		wlc_atm_update_perc(wlc);
		break;
	}
#endif /* WLATF && WLATF_PERC */
#endif /* WLATF */

	/* Chanspecs with current driver settings */
	case IOV_GVAL(IOV_CHANSPECS_DEFSET): {
		char abbrev[1] = ""; /* No country abbrev should be take as input. */
		wl_uint32_list_t *list = (wl_uint32_list_t *)arg;

		list->count = 0;

		switch (wlc_ht_get_mimo_band_bwcap(wlc->hti)) {
			case WLC_N_BW_20ALL:
				/* 2G 20MHz */
				wlc_get_valid_chanspecs(wlc->cmi, list,
					WL_CHANSPEC_BW_20, TRUE, abbrev);

				/* 5G 20MHz */
				wlc_get_valid_chanspecs(wlc->cmi, list,
					WL_CHANSPEC_BW_20, FALSE, abbrev);
				break;
			case WLC_N_BW_40ALL:
				/* 2G 20MHz */
				wlc_get_valid_chanspecs(wlc->cmi, list,
					WL_CHANSPEC_BW_20, TRUE, abbrev);
				/* 2G 40MHz */
				wlc_get_valid_chanspecs(wlc->cmi, list,
					WL_CHANSPEC_BW_40, TRUE, abbrev);

				/* 5G 20MHz */
				wlc_get_valid_chanspecs(wlc->cmi, list,
					WL_CHANSPEC_BW_20, FALSE, abbrev);
				/* 5G 40MHz */
				wlc_get_valid_chanspecs(wlc->cmi, list,
					WL_CHANSPEC_BW_40, FALSE, abbrev);
				break;
			case WLC_N_BW_20IN2G_40IN5G:
				/* 2G 20MHz */
				wlc_get_valid_chanspecs(wlc->cmi, list,
					WL_CHANSPEC_BW_20, TRUE, abbrev);

				/* 5G 20MHz */
				wlc_get_valid_chanspecs(wlc->cmi, list,
					WL_CHANSPEC_BW_20, FALSE, abbrev);
				/* 5G 40MHz */
				wlc_get_valid_chanspecs(wlc->cmi, list,
					WL_CHANSPEC_BW_40, FALSE, abbrev);
				break;
			default:
				WL_ERROR(("mimo_bw_cap has invalid value set.\n"));
				break;
		}
		ASSERT(list->count < WL_NUMCHANSPECS);
	}
	break;

	case IOV_GVAL(IOV_CHANSPECS): {
		chanspec_t chanspec = *((chanspec_t *)params);
		char abbrev[WLC_CNTRY_BUF_SZ];
		wl_uint32_list_t *list_arg = (wl_uint32_list_t *)arg;
		wl_uint32_list_t *list = NULL;
		uint16 max_num_chanspecs = 0, alloc_len = 0;
		if (chanspec == 0) {
			max_num_chanspecs = (WL_NUMCHANSPECS);
		} else {
			if (CHSPEC_IS2G(chanspec)) {
				if (CHSPEC_IS20(chanspec))
					max_num_chanspecs += WL_NUMCHANSPECS_2G_20;
				if (CHSPEC_IS40(chanspec))
					max_num_chanspecs += WL_NUMCHANSPECS_2G_40;
			}
			if (CHSPEC_IS5G(chanspec)) {
				if (CHSPEC_IS20(chanspec))
					max_num_chanspecs += WL_NUMCHANSPECS_5G_20;
				if (CHSPEC_IS40(chanspec))
					max_num_chanspecs += WL_NUMCHANSPECS_5G_40;
				if (CHSPEC_IS80(chanspec))
					max_num_chanspecs += WL_NUMCHANSPECS_5G_80;
				if (CHSPEC_IS8080(chanspec))
					max_num_chanspecs += WL_NUMCHANSPECS_5G_8080;
				if (CHSPEC_IS160(chanspec))
					max_num_chanspecs += WL_NUMCHANSPECS_5G_160;
			}

		}

		alloc_len = ((1 + max_num_chanspecs) * sizeof(uint32));
		/* if caller's buf may not be long enough, malloc a temp buf */
		if (len < alloc_len) {
			list = (wl_uint32_list_t *)MALLOC(wlc->osh, alloc_len);
			if (list == NULL) {
				err = BCME_NOMEM;
				break;
			}
		} else {
			list = list_arg;
		}
		bzero(abbrev, WLC_CNTRY_BUF_SZ);
		strncpy(abbrev, ((char*)params + sizeof(chanspec_t)), WLC_CNTRY_BUF_SZ - 1);

		if (!bcmp(wlc_channel_ccode(wlc->cmi), abbrev, WLC_CNTRY_BUF_SZ -1) &&
			(wlc->band->bandunit == CHSPEC_WLCBANDUNIT(chanspec))) {
			/* If requested abrev is the current one, within the same band,
			 * avoid country lookup and return chanspecs of current country/regrev
			 */
			bzero(abbrev, WLC_CNTRY_BUF_SZ);
		}

		list->count = 0;
		if (chanspec == 0) {
			wlc_get_valid_chanspecs(wlc->cmi, list,
			                        WL_CHANSPEC_BW_20, TRUE, abbrev);
			wlc_get_valid_chanspecs(wlc->cmi, list,
			                        WL_CHANSPEC_BW_40, TRUE, abbrev);
			wlc_get_valid_chanspecs(wlc->cmi, list,
			                        WL_CHANSPEC_BW_20, FALSE, abbrev);
			wlc_get_valid_chanspecs(wlc->cmi, list,
			                        WL_CHANSPEC_BW_40, FALSE, abbrev);
			wlc_get_valid_chanspecs(wlc->cmi, list,
			                        WL_CHANSPEC_BW_80, FALSE, abbrev);
			wlc_get_valid_chanspecs(wlc->cmi, list,
			                        WL_CHANSPEC_BW_8080, FALSE, abbrev);
			wlc_get_valid_chanspecs(wlc->cmi, list,
			                        WL_CHANSPEC_BW_160, FALSE, abbrev);
		} else {
			wlc_get_valid_chanspecs(wlc->cmi, list,
			                        CHSPEC_BW(chanspec), CHSPEC_IS2G(chanspec),
			                        abbrev);
		}
		ASSERT(list->count <= max_num_chanspecs);
		/* if not reusing the caller's buffer */
		if (list != list_arg) {
			/* now copy if we can fr internal buf to caller's or return error */
			if (list->count <= (len - sizeof(uint32))/sizeof(uint32)) {
				bcopy(list, list_arg, (list->count + 1) * sizeof(uint32));
			} else {
				/* caller buf too short - free mem and return error */
				err = BCME_BUFTOOSHORT;
			}
			MFREE(wlc->osh, list, alloc_len);
		}
		break;
	}

	case IOV_GVAL(IOV_CHANSPEC):
		if (bsscfg->associated) {
			*ret_int_ptr = (int32)current_bss->chanspec;
		} else {
			*ret_int_ptr = (int32)wlc->default_bss->chanspec;
		}
		break;

	case IOV_SVAL(IOV_CHANSPEC): {
		chanspec_t chspec = (chanspec_t)int_val;
		wlc_info_t *to_wlc = wlc;
		if (wf_chspec_malformed(chspec)) {
			WL_ERROR(("wl%d: malformed chanspec 0x%04x\n",
			          to_wlc->pub->unit, chspec));
			err = BCME_BADCHAN;
			break;
		}

		if (!wlc_valid_chanspec_db(to_wlc->cmi, chspec)) {
			WL_ERROR(("wl%d: invalid chanspec 0x%04x\n",
			          to_wlc->pub->unit, chspec));
			err = BCME_BADCHAN;
			break;
		}
		to_wlc = wlc_find_wlc_for_chanspec(wlc, chspec);
		if (!to_wlc->pub->up && IS_MBAND_UNLOCKED(to_wlc)) {
			if (to_wlc->band->bandunit != CHSPEC_WLCBANDUNIT(chspec)) {
				to_wlc->bandinit_pending = TRUE;
			} else {
				to_wlc->bandinit_pending = FALSE;
			}
		}
		/* want to migrate to use bsscfg->chanspec as the configured chanspec
		 * and wlc->chanspec as the current chanspec.
		 * wlc->default_bss->chanspec would have no meaning
		 */
		 /* Note that default_bss is shared between all WLCs instance */
		to_wlc->default_bss->chanspec = chspec;

		if (AP_ENAB(to_wlc->pub)) {
			wlc_ap_set_chanspec(to_wlc->ap, chspec);
		}
#ifdef WLMCHAN
		/* check for wlcif pointer, if valid it means we refer correct bsscfg */
		if (MCHAN_ENAB(to_wlc->pub) && BSS_P2P_ENAB(to_wlc, bsscfg))
			wlc_mchan_config_go_chanspec(to_wlc->mchan, bsscfg, chspec);
#endif /* WLMCHAN */
		/* wlc_BSSinit() will sanitize the rateset before using it.. */
		/* Block setting of home_chanspec only if core is idle */
		if (to_wlc->pub->up && !to_wlc->pub->associated &&
			!AS_IN_PROGRESS(to_wlc) &&
			!ANY_SCAN_IN_PROGRESS(to_wlc->scan) &&
		    (WLC_BAND_PI_RADIO_CHANSPEC != chspec)) {
			wlc_set_home_chanspec(to_wlc, chspec);
			wlc_suspend_mac_and_wait(to_wlc);
			wlc_set_chanspec(to_wlc, chspec, CHANSW_REASON(CHANSW_IOVAR));
			wlc_enable_mac(to_wlc);
#ifdef WL_MONITOR
			if (to_wlc->monitor) {
				/* Channel is changed in monitor mode, calibrate the new channel */
				wlc_monitor_phy_cal(to_wlc->mon_info, (to_wlc->monitor != 0));
			}
#endif // endif
		}
		break;
	}

#if defined(WLTEST)
	case IOV_GVAL(IOV_BOARDFLAGS): {
		*ret_int_ptr = wlc->pub->boardflags;
		break;
	}

	case IOV_SVAL(IOV_BOARDFLAGS): {
		wlc->pub->boardflags = int_val;

		/* This iovar needs to be done at both host and dongle */
		err = wlc_iovar_op(wlc, "bmac_bf", arg, len, arg, len, IOV_SET, wlcif);
		break;
	}

	case IOV_GVAL(IOV_BOARDFLAGS2): {
		*ret_int_ptr = wlc->pub->boardflags2;
		break;
	}

	case IOV_SVAL(IOV_BOARDFLAGS2): {
		wlc->pub->boardflags2 = int_val;

		/* This iovar needs to be done at both host and dongle */
		err = wlc_iovar_op(wlc, "bmac_bf2", arg, len, arg, len, IOV_SET, wlcif);
		break;
	}

	case IOV_GVAL(IOV_ANTGAIN): {
		uint8	*ag = (uint8*)arg;
		uint j;

		ag[0] = 0;
		ag[1] = 0;

		for (j = 0; j < NBANDS(wlc); j++) {
			/* Use band 1 for single band 11a */
			if (IS_SINGLEBAND_5G(wlc->deviceid, wlc->phy_cap))
				j = BAND_5G_INDEX;

			/* Compute Antenna Gain */
			ag[j] = wlc->bandstate[j]->antgain >> 2;
		}
		break;
	}

	case IOV_SVAL(IOV_ANTGAIN): {
		uint8	*ag = (uint8*)params;
		uint j;

		for (j = 0; j < NBANDS(wlc); j++) {
			/* Use band 1 for single band 11a */
			if (IS_SINGLEBAND_5G(wlc->deviceid, wlc->phy_cap))
				j = BAND_5G_INDEX;

			wlc_pi_band_update(wlc, j);

			/* Compute Antenna Gain */
			wlc->band->antgain = BAND_5G(wlc->band->bandtype) ? ag[1] : ag[0];
			wlc->band->antgain = wlc_antgain_calc(wlc->band->antgain);
		}
		break;
	}
#endif // endif

	case IOV_GVAL(IOV_LIFETIME):
	{
		wl_lifetime_t *lifetime = (wl_lifetime_t *)arg;

		if (int_val < AC_COUNT) {
			lifetime->ac = int_val;
			lifetime->lifetime = wlc->lifetime[int_val] / 1000; /* us -> ms */
		} else {
			err = BCME_BADARG;
		}
		break;
	}

	case IOV_SVAL(IOV_LIFETIME):
	{
		wl_lifetime_t lifetime;

		bcopy(arg, &lifetime, sizeof(wl_lifetime_t));
		if (lifetime.ac < AC_COUNT && lifetime.lifetime <= WL_LIFETIME_MAX) {
			wlc->lifetime[lifetime.ac] = lifetime.lifetime * 1000; /* ms ->us */

			wlc_rfaware_lifetime_set(wlc, wlc->rfaware_lifetime);
		} else
			err = BCME_BADARG;

		break;
	}

	case IOV_GVAL(IOV_LIFETIME_MG):
	{
		wlc_cmn_info_t *cmn = wlc->cmn;
		wl_lifetime_mg_t *lt_mg = (wl_lifetime_mg_t *)arg;
		if (int_val & cmn->lifetime_mg->mgmt_bitmap) {
			lt_mg->mgmt_bitmap = cmn->lifetime_mg->mgmt_bitmap;
			lt_mg->lifetime = cmn->lifetime_mg->lifetime / 1000; /* us->ms */
		} else {
			err = BCME_BADARG;
		}
		break;
	}

	case IOV_SVAL(IOV_LIFETIME_MG):
	{
		wlc_cmn_info_t *cmn = wlc->cmn;
		if (((wl_lifetime_mg_t*)arg)->lifetime <= WL_LIFETIME_MAX) {
			bcopy(arg, cmn->lifetime_mg, sizeof(wl_lifetime_mg_t));
			/* ms ->us */
			cmn->lifetime_mg->lifetime = cmn->lifetime_mg->lifetime * 1000;
		} else {
			err = BCME_RANGE;
		}
		break;
	}

#ifdef STA
#ifdef BCMDBG
	case IOV_GVAL(IOV_APSTA_DBG):
		*ret_int_ptr = wl_apsta_dbg;
		break;

	case IOV_SVAL(IOV_APSTA_DBG):
		wl_apsta_dbg = int_val;
		break;
#endif /* BCMDBG */
#endif /* STA */
	case IOV_GVAL(IOV_NOBCNSSID):
		*ret_int_ptr = bsscfg->closednet_nobcnssid;
		break;

	case IOV_SVAL(IOV_NOBCNSSID):
		/* "nobcnssid" control only one functionality: hide ssid in bcns.
		 * No change in response to broadcast probe requests
		 */
		bsscfg->closednet_nobcnssid = bool_val;

		if (BSSCFG_AP(bsscfg) && bsscfg->up) {
			wlc_bss_update_beacon(wlc, bsscfg);
		}
		break;
	case IOV_GVAL(IOV_VER):
		bcm_binit(&b, arg, len);
		wl_dump_ver(wlc->wl, &b);
		break;

	case IOV_GVAL(IOV_ANTENNAS):
		*ret_int_ptr = wlc_get_antennas(wlc);
		break;

#ifdef NLO
	case IOV_SVAL(IOV_NLO):
		bsscfg->nlo = bool_val;
		break;
#endif /* NLO */

	case IOV_SVAL(IOV_MSGLEVEL2): {
		struct wl_msglevel2 *msglevel = (struct wl_msglevel2 *)arg;
#ifdef WL_CFG80211
		wl_cfg80211_trace_set((msglevel->high & WL_CFG80211_VAL) != 0);
#endif /* WL_CFG80211 */
		bcopy(&msglevel->low, &wl_msg_level, sizeof(uint32));
		bcopy(&msglevel->high, &wl_msg_level2, sizeof(uint32));
		break;
	}

	case IOV_GVAL(IOV_MSGLEVEL2): {
		struct wl_msglevel2 *msglevel = (struct wl_msglevel2 *)arg;
		msglevel->low = wl_msg_level;
		msglevel->high = wl_msg_level2;
		break;
	}

	case IOV_GVAL(IOV_INFRA_CONFIGURATION):
		*ret_int_ptr = (int)wlc_bsstype_dot112wl(wlc->default_bss->bss_type);
		break;

	case IOV_SVAL(IOV_INFRA_CONFIGURATION):
		wlc->default_bss->bss_type = (int8)wlc_bsstype_wl2dot11((uint)int_val);
		break;

	case IOV_GVAL(IOV_DOWN_OVERRIDE):
		*ret_int_ptr = (int32)wlc->down_override;
		break;

	case IOV_SVAL(IOV_DOWN_OVERRIDE):
		wlc->down_override = bool_val;
		break;

	case IOV_GVAL(IOV_ALLMULTI):
		*ret_int_ptr = (int32)bsscfg->allmulti;
		break;

	case IOV_SVAL(IOV_ALLMULTI):
		bsscfg->allmulti = bool_val;
		break;

	case IOV_GVAL(IOV_LEGACY_PROBE):
		*ret_int_ptr = (int32)wlc->legacy_probe;
		break;

	case IOV_SVAL(IOV_LEGACY_PROBE):
		wlc->legacy_probe = bool_val;
		break;

#ifdef STA
	case IOV_GVAL(IOV_NOLINKUP):
		*ret_int_ptr = wlc->cmn->nolinkup;
		break;

	case IOV_SVAL(IOV_NOLINKUP):
		wlc->cmn->nolinkup = bool_val;
		break;
#endif /* STA */

	case IOV_GVAL(IOV_BANDUNIT):
		*ret_int_ptr = wlc->band->bandunit;
		break;

#ifdef BCM_BLOG
	case IOV_SVAL(IOV_FCACHE):
		wlc->pub->fcache = bool_val;
		break;
	case IOV_GVAL(IOV_FCACHE):
		*ret_int_ptr = (int32)wlc->pub->fcache;
		break;
#endif /* BCM_BLOG */

#ifdef STA
	case IOV_GVAL(IOV_WAKE_EVENT):
		*ret_int_ptr = wlc->wake_event_timeout ? 1 : 0;
		break;

	case IOV_SVAL(IOV_WAKE_EVENT): {
		uint16 timeout = (uint16) int_val;

		if (timeout < 1 || timeout > 120) {
			err = BCME_BADARG;
			break;
		}
		wlc->wake_event_timeout = timeout;
		wl_del_timer(wlc->wl, wlc->wake_event_timer);
		wl_add_timer(wlc->wl, wlc->wake_event_timer, timeout * 1000, FALSE);
		break;
	}
#endif /* STA */

	case IOV_SVAL(IOV_NAV_RESET_WAR_DISABLE):
		wlc_nav_reset_war(wlc, bool_val);
		break;

	case IOV_GVAL(IOV_NAV_RESET_WAR_DISABLE):
		*ret_int_ptr = wlc->nav_reset_war_disable;
		break;

	case IOV_GVAL(IOV_RFAWARE_LIFETIME):
		*ret_int_ptr = (uint32)wlc->rfaware_lifetime;
		break;

	case IOV_SVAL(IOV_RFAWARE_LIFETIME):
		err = wlc_rfaware_lifetime_set(wlc, (uint16)int_val);
		break;

	case IOV_GVAL(IOV_ASSERT_TYPE):
		*ret_int_ptr = g_assert_type;
		break;

	case IOV_SVAL(IOV_ASSERT_TYPE):
		g_assert_type = (uint32)int_val;
		break;
#ifdef STA
#ifdef PHYCAL_CACHING
	case IOV_GVAL(IOV_PHYCAL_CACHE):
		*ret_int_ptr = (int32)wlc_bmac_get_phycal_cache_flag(wlc->hw);
		break;

	case IOV_SVAL(IOV_PHYCAL_CACHE):
		if (!bool_val) {
			if (wlc_bmac_get_phycal_cache_flag(wlc->hw)) {
				wlc_phy_cal_cache_deinit(pi);
				wlc_bmac_set_phycal_cache_flag(wlc->hw, FALSE);
				wlc_iovar_setint(wlc, "phy_percal", PHY_PERICAL_MPHASE);
			}
			break;
		} else {
			/*
			 * enable caching, break association, compute TX/RX IQ Cal and RSSI cal
			 * values
			 */
			if (wlc_bmac_get_phycal_cache_flag(wlc->hw))
				break;

			if (!WLCISNPHY(wlc->band)) {
				WL_ERROR(("wl%d: %s: Caching only supported on nphy\n",
					wlc->pub->unit, __FUNCTION__));
				err = BCME_UNSUPPORTED;
			} else if (wlc_phy_cal_cache_init(pi) == BCME_OK) {
				err = wlc_cache_cals(wlc);
			} else {
				err = BCME_NOMEM;
			}
			break;
		}

	case IOV_GVAL(IOV_CACHE_CHANSWEEP):
		if (wlc_bmac_get_phycal_cache_flag(wlc->hw)) {
			*ret_int_ptr = wlc_cachedcal_sweep(wlc);
		} else {
			*ret_int_ptr = BCME_UNSUPPORTED;
			WL_ERROR(("wl%d: %s: PhyCal caching not enabled, "
			          "can't scan\n", wlc->pub->unit, __FUNCTION__));
		}
		break;

	case IOV_SVAL(IOV_CACHE_CHANSWEEP):
	        if (wlc_bmac_get_phycal_cache_flag(wlc->hw)) {
			if (int_val > 0 && int_val < MAXCHANNEL)
				err = wlc_cachedcal_tune(wlc, (uint16) int_val);
			else if (!wf_chspec_malformed((uint16)int_val))
				err = wlc_cachedcal_tune(wlc,
				                         (uint16) int_val & WL_CHANSPEC_CHAN_MASK);
			else
				err = BCME_BADCHAN;
		} else {
			err = BCME_UNSUPPORTED;
			WL_ERROR(("wl%d: %s: PhyCal caching not enabled, "
			          "can't scan\n", wlc->pub->unit, __FUNCTION__));
		}
		break;

#endif /* PHYCAL_CACHING */
#endif /* STA */

#ifdef BCMPKTPOOL
	case IOV_GVAL(IOV_POOL):
		if (POOL_ENAB(wlc->pub->pktpool))
			*ret_int_ptr = pktpool_tot_pkts(wlc->pub->pktpool);
		else
			*ret_int_ptr = 0;
		break;
	case IOV_SVAL(IOV_POOL):
		if (!POOL_ENAB(wlc->pub->pktpool)) {
			err = BCME_UNSUPPORTED;
			break;
		}

		if (pktpool_setmaxlen(wlc->pub->pktpool, (uint16)int_val) != int_val) {
			err = BCME_BADLEN;
			break;
		}
		break;
#endif /* BCMPKTPOOL */

	case IOV_GVAL(IOV_CURR_RATESET): {
		wl_rateset_args_t *ret_rs = (wl_rateset_args_t *)arg;
		wlc_rateset_t *rs;
#ifdef WL11AC
		uint8 bw;
#endif /* WL11AC */
		wl_rateset_args_t *in_rs = (wl_rateset_args_t *)params;
		ret_rs->version = RATESET_ARGS_VERSION;

		/* if in_rs->version is 0, and length input is 4 bytes, it is a get operation
		 * to read the iovar version.
		 */
		if ((in_rs->version == 0) && (p_len == sizeof(int32))) {
			break;
		}

		if (wlc->pub->associated) {
			rs = &bsscfg->current_bss->rateset;
#ifdef WL11AC
			bw = WLC_GET_BW_FROM_CHSPEC(bsscfg->current_bss->chanspec);
#endif /* WL11AC */
		} else {
			rs = &wlc->default_bss->rateset;
#ifdef WL11AC
			bw = BW_20MHZ;
#endif /* WL11AC */
		}

		if (len < (int)(rs->count + sizeof(rs->count))) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		/* Copy legacy rateset and mcsset */
		ret_rs->count = rs->count;
		bcopy(&rs->rates, &ret_rs->rates, rs->count);

		if (N_ENAB(wlc->pub))
			bcopy(rs->mcs, ret_rs->mcs, MCSSET_LEN);
#ifdef WL11AC
		if (VHT_ENAB_BAND(wlc->pub, wlc->band->bandtype)) {
			uint8 nss, mcs_code, prop_mcs_code;
			for (nss = 1; nss <= wlc->stf->op_txstreams; nss++) {
				mcs_code = VHT_MCS_MAP_GET_MCS_PER_SS(nss, rs->vht_mcsmap);
				prop_mcs_code =
					VHT_MCS_MAP_GET_MCS_PER_SS(nss, rs->vht_mcsmap_prop);
				if (wlc->pub->associated) {
					uint8 vht_ratemask = 0;
					if AP_ENAB(wlc->pub) {
						vht_ratemask = BAND_5G(wlc->band->bandtype) ?
							WLC_VHT_FEATURES_RATES_5G(wlc->pub) :
							WLC_VHT_FEATURES_RATES_2G(wlc->pub);
					} else {
						/* Look for scb corresponding to the bss */
						struct scb *scb = wlc_scbfind(wlc,
							bsscfg, &bsscfg->current_bss->BSSID);
						if (scb) {
							vht_ratemask =
								wlc_vht_get_scb_ratemask(
									wlc->vhti, scb);
							vht_ratemask &=
								BAND_5G(wlc->band->bandtype) ?
								~(WL_VHT_FEATURES_STD_RATES_2G) :
								~(WL_VHT_FEATURES_STD_RATES_5G);
						} else {
							vht_ratemask =
								BAND_5G(wlc->band->bandtype) ?
								WLC_VHT_FEATURES_RATES_5G(wlc->pub)
								:
								WLC_VHT_FEATURES_RATES_2G(wlc->pub);
						}
					}

					ret_rs->vht_mcs[nss - 1] =
						wlc_get_valid_vht_mcsmap(mcs_code, prop_mcs_code,
						bw,
						((wlc->stf->ldpc_tx == AUTO)||
						(wlc->stf->ldpc_tx == ON)),
						nss, vht_ratemask);
				} else {
					ret_rs->vht_mcs[nss - 1] =
						VHT_MCS_CODE_TO_MCS_MAP(mcs_code) |
						VHT_PROP_MCS_CODE_TO_PROP_MCS_MAP(prop_mcs_code);
				}
			}
		}
		if (HE_ENAB_BAND(wlc->pub, wlc->band->bandtype)) {
			uint8 nss, mcs_code;

			if (wlc->pub->associated) {
				rs = &bsscfg->current_bss->rateset;
			} else {
				rs = &wlc->band->defrateset;
			}

			for (nss = 1; nss <= wlc->stf->txstreams; nss++) {
				mcs_code = HE_CAP_MAX_MCS_NSS_GET_MCS(nss, rs->he_bw80_tx_mcs_nss);
				ret_rs->he_mcs[nss - 1] = HE_MAX_MCS_TO_MCS_MAP(mcs_code);
			}
		}
#endif /* WL11AC */
		break;
	}

	case IOV_GVAL(IOV_RATESET): {
		wlc_rateset_t rs;
		wl_rateset_args_t *ret_rs = (wl_rateset_args_t *)arg;

		wl_rateset_args_t *in_rs = (wl_rateset_args_t *)params;
		ret_rs->version = RATESET_ARGS_VERSION;

		/* if in_rs->version is 0, and length input is 4 bytes, it is a get operation
		 * to read the iovar version.
		 */
		if ((in_rs->version == 0) && (p_len == sizeof(int32))) {
			break;
		}

		bzero(&rs, sizeof(wlc_rateset_t));
		wlc_default_rateset(wlc, (wlc_rateset_t*)&rs);

		if (len < (int)(rs.count + sizeof(rs.count))) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		/* Copy legacy rateset */
		ret_rs->count = rs.count;
		bcopy(&rs.rates, &ret_rs->rates, rs.count);

		/* Copy mcs rateset */
		if (N_ENAB(wlc->pub))
			bcopy(rs.mcs, ret_rs->mcs, MCSSET_LEN);
		/* copy vht mcs set */
		if (VHT_ENAB_BAND(wlc->pub, wlc->band->bandtype)) {
			uint8 nss, mcs_code, prop_mcs_code;
			for (nss = 1; nss <= wlc->stf->op_txstreams; nss++) {
				mcs_code = VHT_MCS_MAP_GET_MCS_PER_SS(nss, rs.vht_mcsmap);
				prop_mcs_code = VHT_MCS_MAP_GET_MCS_PER_SS(nss, rs.vht_mcsmap_prop);
				ret_rs->vht_mcs[nss - 1] = VHT_MCS_CODE_TO_MCS_MAP(mcs_code) |
					VHT_PROP_MCS_CODE_TO_PROP_MCS_MAP(prop_mcs_code);
			}
		}
		/* copy he mcs set */
		if (HE_ENAB_BAND(wlc->pub, wlc->band->bandtype)) {
			uint8 nss, mcs_code;
			for (nss = 1; nss <= wlc->stf->op_txstreams; nss++) {
				mcs_code = HE_CAP_MAX_MCS_NSS_GET_MCS(nss, rs.he_bw80_tx_mcs_nss);
				ret_rs->he_mcs[nss - 1] = HE_MAX_MCS_TO_MCS_MAP(mcs_code);
			}
		}

		break;
	}

	case IOV_SVAL(IOV_RATESET): {
		wlc_rateset_t rs;
		wl_rateset_args_t *in_rs = (wl_rateset_args_t *)arg;

		if (len < (int)(in_rs->count + sizeof(in_rs->count))) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		bzero(&rs, sizeof(wlc_rateset_t));

		/* Copy legacy rateset */
		rs.count = in_rs->count;
		bcopy(&in_rs->rates, &rs.rates, rs.count);

		/* Copy mcs rateset */
		if (N_ENAB(wlc->pub))
			bcopy(in_rs->mcs, rs.mcs, MCSSET_LEN);
#if defined(WL11AC)
		if (VHT_ENAB_BAND(wlc->pub, wlc->band->bandtype)) {
			/* Convert user level vht_mcs rate masks to driver level rateset map. */
			int nss;
			uint16 mcsmap;

			for (nss = 1; nss <= VHT_CAP_MCS_MAP_NSS_MAX; ++nss) {
				mcsmap = in_rs->vht_mcs[nss-1];

				VHT_MCS_MAP_SET_MCS_PER_SS(nss,
					VHT_MCS_MAP_TO_MCS_CODE((mcsmap &
					VHT_CAP_MCS_FULL_RATEMAP)), rs.vht_mcsmap);
				VHT_MCS_MAP_SET_MCS_PER_SS(nss,
					VHT_PROP_MCS_MAP_TO_PROP_MCS_CODE((mcsmap &
					VHT_PROP_MCS_FULL_RATEMAP)), rs.vht_mcsmap_prop);
			}
		}
#endif /* WL11AC */
#if defined(WL11AX)
		/* copy he mcs set */
		if (HE_ENAB_BAND(wlc->pub, wlc->band->bandtype)) {
			/* Convert user level he_mcs rate masks to driver level rateset map. */
			int nss;
			uint16 mcsmap;

			for (nss = 1; nss <= HE_CAP_MCS_MAP_NSS_MAX; nss++) {
				mcsmap = in_rs->he_mcs[nss-1];
				HE_MCS_MAP_SET_MCS_PER_SS(nss, HE_MCS_MAP_TO_MAX_MCS(mcsmap),
					rs.he_bw80_tx_mcs_nss);
				rs.he_bw80_rx_mcs_nss = rs.he_bw80_tx_mcs_nss;
				rs.he_bw160_tx_mcs_nss = rs.he_bw80_tx_mcs_nss;
				rs.he_bw160_rx_mcs_nss = rs.he_bw80_tx_mcs_nss;
				rs.he_bw80p80_tx_mcs_nss = rs.he_bw80_tx_mcs_nss;
				rs.he_bw80p80_rx_mcs_nss = rs.he_bw80_tx_mcs_nss;
			}
			wlc_he_set_rateset_filter(wlc->hei, &rs);
		}
#endif /* WL11AX */
		err = wlc_set_rateset(wlc, &rs);

		break;
	}

#if defined(BCM_DCS) && defined(STA)
	case IOV_SVAL(IOV_BRCM_DCS_REQ): {

		chanspec_t chspec = (chanspec_t) int_val;

		if (SCAN_IN_PROGRESS(wlc->scan)) {
			wlc_scan_abort(wlc->scan, WLC_E_STATUS_ABORT);
			err = BCME_BUSY;
			break;
		}

		if (wf_chspec_malformed(chspec) || !wlc_valid_chanspec(wlc->cmi, chspec)) {
			err = BCME_BADCHAN;
			break;
		}

		if (BSSCFG_STA(bsscfg) && bsscfg->associated) {
			wl_bcmdcs_data_t bcmdcs_data;
			bcmdcs_data.reason = BCM_DCS_IOVAR;
			bcmdcs_data.chspec = chspec;
			err = wlc_send_action_brcm_dcs(wlc, &bcmdcs_data,
				wlc_scbfind(wlc, bsscfg, &bsscfg->BSSID));
		}

		break;
	}
#endif /* BCM_DCS && STA */

	/* iovar versions of WLC_SCB_AUTHORIZE et al */
	case IOV_SVAL(IOV_AUTHOPS): {
		authops_t *pp = params;

		switch (pp->code) {
			/* these only get an ea pointer */
			case WLC_SCB_AUTHORIZE:
			case WLC_SCB_DEAUTHORIZE:
				err = wlc_ioctl(wlc, pp->code, &pp->ioctl_args.ea,
					sizeof(pp->ioctl_args.ea), wlcif);
				break;
			default:
			/* the rest get an scb_val_t pointer */
				err = wlc_ioctl(wlc, pp->code, &pp->ioctl_args,
					sizeof(pp->ioctl_args), wlcif);
		}
		break;
	}
#ifdef DNGL_WD_KEEP_ALIVE
	case IOV_GVAL(IOV_DNGL_WD_KEEP_ALIVE):
		*ret_int_ptr = (int)wlc->dngl_wd_keep_alive;
		break;
	case IOV_SVAL(IOV_DNGL_WD_KEEP_ALIVE):
			wlc->dngl_wd_keep_alive = bool_val;
			delay = bool_val ? TIMER_INTERVAL_DNGL_WATCHDOG : 0;
			wlc_bmac_dngl_wd_keep_alive(wlc->hw, delay);
		break;
	case IOV_SVAL(IOV_DNGL_WD_FORCE_TRIGGER):
			wlc_bmac_dngl_wd_keep_alive(wlc->hw, int_val);
		break;
#endif // endif
	case IOV_SVAL(IOV_IS_WPS_ENROLLEE):
		if (BSSCFG_STA(bsscfg))
			bsscfg->is_WPS_enrollee = bool_val;
		else
			err = BCME_NOTSTA;
		break;

	case IOV_GVAL(IOV_IS_WPS_ENROLLEE):
		if (BSSCFG_STA(bsscfg))
			*ret_int_ptr = (int32)bsscfg->is_WPS_enrollee;
		else
			err = BCME_NOTSTA;
		break;

#ifdef BCMDBG
	case IOV_SVAL(IOV_TSF):
		wlc_tsf_set(wlc, int_val, int_val2);
		break;

	case IOV_GVAL(IOV_TSF): {
		uint32 tsf_l, tsf_h;
		wlc_read_tsf(wlc, &tsf_l, &tsf_h);
		((uint32*)arg)[0] = tsf_l;
		((uint32*)arg)[1] = tsf_h;
		break;
	}

	case IOV_SVAL(IOV_TSF_ADJUST):
		wlc_tsf_adjust(wlc, int_val);
		break;
#endif /* BCMDBG */

	case IOV_GVAL(IOV_WLIF_BSSCFG_IDX):
		*ret_int_ptr = WLC_BSSCFG_IDX(bsscfg);
		break;

	case IOV_GVAL(IOV_SSID): {
		wlc_ssid_t ssid;
		int ssid_copy_len;

		/* copy the bsscfg's SSID to the on-stack return structure */
		ssid.SSID_len = bsscfg->SSID_len;
		if (ssid.SSID_len)
			bcopy(bsscfg->SSID, ssid.SSID, ssid.SSID_len);

		ssid_copy_len = (int)(sizeof(ssid.SSID_len) + ssid.SSID_len);
		if ((int)len < ssid_copy_len) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		bcopy(&ssid, arg, ssid_copy_len);
		break;
	}

	case IOV_SVAL(IOV_SSID): {
		int32 ssidlen;
		uchar* ssid_ptr;
#if defined(BCMDBG) || defined(WLMSG_OID)
		char ssidbuf[SSID_FMT_BUF_LEN];
#endif // endif
		/* input buffer is:
		 * uint32 ssid_len      (local int_val)
		 * char   ssid[]	(local arg + 4)
		 */

		/* get the SSID length from the wlc_ssid_t struct starting just past the
		 * bsscfg index
		 */
		ssidlen = int_val;

		if (ssidlen < 0 || ssidlen > DOT11_MAX_SSID_LEN) {
			err = BCME_BADSSIDLEN;
			break;
		}

		if (len < (int)(sizeof(int32) + ssidlen)) {
			err = BCME_BUFTOOSHORT;
			break;
		}
		ssid_ptr = (uchar*)arg + sizeof(int32);

		WL_OID(("wl: wlc_iovar_op: setting SSID of bsscfg %d to \"%s\"\n",
		 WLC_BSSCFG_IDX(bsscfg),
			(wlc_format_ssid(ssidbuf, ssid_ptr, ssidlen), ssidbuf)));

		/* can only change SSID if the config is down */
		if (bsscfg->up) {
			err = BCME_NOTDOWN;
			break;
		}
		wlc_bsscfg_SSID_set(bsscfg, ssid_ptr, ssidlen);

		break;
	}

	case IOV_GVAL(IOV_RPT_HITXRATE):
		*ret_int_ptr = wlc->rpt_hitxrate;
		break;

	case IOV_SVAL(IOV_RPT_HITXRATE):
		wlc->rpt_hitxrate = bool_val;
		break;

#ifdef WLRXOV
	case IOV_SVAL(IOV_RXOV):
		wlc->pub->_rxov = bool_val;
		if (WLRXOV_ENAB(wlc->pub) && (wlc->rxov_timer == NULL)) {
			wlc->rxov_timer = wl_init_timer(wlc->wl, wlc_rxov_timer, wlc, "rxov");
			if (wlc->rxov_timer == NULL) {
				err = BCME_NORESOURCE;
				break;
			}
		}

		if (WLRXOV_ENAB(wlc->pub) && wlc->rxov_timer) {
			/* wlc_write_shm(wlc, M_RXOV_HIWAT, RXOV_MPDU_HIWAT); */
			wlc_bmac_set_defmacintmask(wlc->hw, MI_RXOV, MI_RXOV);
		} else
			wlc_bmac_set_defmacintmask(wlc->hw, MI_RXOV, ~MI_RXOV);
		break;
	case IOV_GVAL(IOV_RXOV):
		*ret_int_ptr = wlc->pub->_rxov;
		/* XXX: looks like this implementation needs revisiting. M_RXOV_HIWAT is not
		 * generated and used like the above comment. So commenting.
		 *	WL_TRACE(("rxov hiwat: %d\n", wlc_read_shm(wlc, M_RXOV_HIWAT)));
		 */
		break;
#endif /* WLRXOV */

#ifdef BCMDBG
#ifdef STA
	case IOV_SVAL(IOV_RPMT): {
		uint32 rpmt_1_prd = load32_ua((uint8 *)arg);
		uint32 rpmt_0_prd = load32_ua((uint8 *)arg + sizeof(uint32));
		bool enab = rpmt_1_prd != 0 && rpmt_0_prd != 0;

		if (enab) {
			if (bsscfg->rpmt_timer != NULL) {
				err = BCME_BUSY;
				break;
			}
			if ((bsscfg->rpmt_timer = wlc_hrt_alloc_timeout(wlc->hrti)) == NULL) {
				WL_ERROR(("wl%d.%d: failed to alloc Rapid PM Transition timeout\n",
				          wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg)));
				err = BCME_NORESOURCE;
				break;
			}
			bsscfg->rpmt_1_prd = rpmt_1_prd;
			bsscfg->rpmt_0_prd = rpmt_0_prd;
			bsscfg->rpmt_n_st = 1;
			wlc_rpmt_timer_cb(bsscfg);
		} else {
			if (bsscfg->rpmt_timer == NULL) {
				err = BCME_ERROR;
				break;
			}
			wlc_hrt_free_timeout(bsscfg->rpmt_timer);
			bsscfg->rpmt_timer = NULL;
		}
		break;
	}
#endif /* STA */
#endif /* BCMDBG */

#ifdef BPRESET
	case IOV_SVAL(IOV_BPRESET_ENAB):
	        /* configure value for _bpreset */
	        wlc->pub->_bpreset = (int_val != 0);
	        break;
	case IOV_GVAL(IOV_BPRESET_ENAB):
	        *ret_int_ptr = (int)wlc->pub->_bpreset;
	        break;
#ifdef BCMDBG
	case IOV_SVAL(IOV_BACKPLANE_RESET):
		wlc_full_reset(wlc->hw, (uint32)int_val);
		break;
#endif /* BCMDBG */
#endif /* BPRESET */

	case IOV_GVAL(IOV_MEMPOOL):  {
		wl_mempool_stats_t *stats;

		stats = (wl_mempool_stats_t *)arg;

		/* Calculate the max # of statistics that can fit in buffer. */
		stats->num = (len - OFFSETOF(wl_mempool_stats_t, s)) / sizeof(bcm_mp_stats_t);

		err = bcm_mpm_stats(wlc->mem_pool_mgr, stats->s, &stats->num);
		break;
	}

#ifdef BCMOVLHW
	case IOV_GVAL(IOV_OVERLAY): {
		if (si_arm_ovl_vaildaddr(wlc->pub->sih, (void *)int_val)) {
			if (len > 512) len = 512;
			memcpy(arg, (void *)int_val, len);
		} else {
			err = BCME_ERROR;
		}
		break;
	}
	case IOV_SVAL(IOV_OVERLAY): {
		uint32 p[3];
		memcpy(p, arg, 3*sizeof(uint32));
		err = si_arm_ovl_remap(wlc->pub->sih, (void *)p[0], (void *)p[1], p[2]);
		break;
	}
#endif /* BCMOVLHW */

	case IOV_GVAL(IOV_FABID):
		*ret_int_ptr = wlc->fabid;
		break;

	case IOV_GVAL(IOV_BRCM_IE):
		*ret_int_ptr = (int32)wlc->brcm_ie;
		break;

	case IOV_SVAL(IOV_BRCM_IE):
		wlc->brcm_ie = bool_val;
		break;
#ifdef MACOSX
	case IOV_GVAL(IOV_HEALTH_STATUS):
		*ret_int_ptr = (int32)wlc->pub->health;
		break;

	case IOV_SVAL(IOV_HEALTH_STATUS):
		wlc->pub->health = 0;
#ifdef BCMDBG
		/* allow override for testing */
		wlc->pub->health = (uint32)int_val;
#endif /* BCMDBG */
		break;
#endif /* MACOSX */
	case IOV_GVAL(IOV_SAR_ENABLE):
		*ret_int_ptr = (int)wlc_channel_sarenable_get(wlc->cmi);
		break;

	case IOV_SVAL(IOV_SAR_ENABLE):
		wlc_channel_sarenable_set(wlc->cmi, bool_val);
#ifdef WL_SAR_SIMPLE_CONTROL
		wlc_phy_dynamic_sarctrl_set(WLC_PI(wlc), bool_val);
#endif /* WL_SAR_SIMPLE_CONTROL */
		break;
#ifdef WL_SARLIMIT
	case IOV_GVAL(IOV_SAR_LIMIT):
	{
		sar_limit_t *sar = (sar_limit_t *)arg;
		if (len < (int)sizeof(sar_limit_t))
			return BCME_BUFTOOSHORT;

		err = wlc_channel_sarlimit_get(wlc->cmi, sar);
		break;
	}

	case IOV_SVAL(IOV_SAR_LIMIT):
	{
		sar_limit_t *sar = (sar_limit_t *)arg;
		if (len < (int)sizeof(sar_limit_t))
			return BCME_BUFTOOSHORT;

		err = wlc_channel_sarlimit_set(wlc->cmi, sar);
		break;
	}
#endif /* WL_SARLIMIT */

	case IOV_GVAL(IOV_ANTDIV_BCNLOSS):
		err = BCME_UNSUPPORTED;
		break;
	case IOV_SVAL(IOV_ANTDIV_BCNLOSS):
		err = BCME_UNSUPPORTED;
		break;

#ifdef EVENT_LOG_COMPILE
	case IOV_SVAL(IOV_EVENT_LOG_SET_INIT):
	{
		uint8 set = ((wl_el_set_params_t *) arg)->set;
		int size = ((wl_el_set_params_t *) arg)->size;
		err = event_log_set_init(wlc->osh, set, size);
		break;
	}
	case IOV_SVAL(IOV_EVENT_LOG_SET_EXPAND):
	{
		uint8 set = ((wl_el_set_params_t *) arg)->set;
		int size = ((wl_el_set_params_t *) arg)->size;
		err = event_log_set_expand(wlc->osh, set, size);
		break;
	}
	case IOV_SVAL(IOV_EVENT_LOG_SET_SHRINK):
	{
		uint8 set = ((wl_el_set_params_t *) arg)->set;
		int size = ((wl_el_set_params_t *) arg)->size;
		err = event_log_set_shrink(wlc->osh, set, size);
		break;
	}

	case IOV_SVAL(IOV_EVENT_LOG_TAG_CONTROL):
	{
		uint16 tag = ((wl_el_tag_params_t *) arg)->tag;
		uint8 set = ((wl_el_tag_params_t *) arg)->set;
		uint8 flags = ((wl_el_tag_params_t *) arg)->flags;
		err = event_log_tag_start(tag, set, flags);
		break;
	}
#ifdef LOGTRACE
	case IOV_GVAL(IOV_EVENT_LOG_TAG_CONTROL):
	{
		uint8 set = ((wl_el_set_params_t *) params)->set;
		err = event_log_get(set, len, arg);
		break;
	}

	case IOV_SVAL(IOV_EVENT_LOG_GET):
	{
		/* flush the buffer by trigerring logtrace */
		uint8 set = ((wl_el_set_params_t *) params)->set;
		err = event_log_flush_log_buffer(set);
		break;
	}

	case IOV_GVAL(IOV_EVENT_LOG_GET):
	{
		/* Fill in the buffer that came in with IOVAR processing. */
		uint8 set = ((wl_el_set_params_t *) params)->set;
		err = event_log_get(set, len, arg);
		break;
	}

#endif /* LOGTRACE */
#endif /* EVENT_LOG_COMPILE */

	case IOV_SVAL(IOV_NAS_NOTIFY_EVENT): {
		scb_val_t *val_tmp = (scb_val_t *)arg;
		if (len < (int)sizeof(scb_val_t)) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		if (val_tmp->val == DOT11_RC_MIC_FAILURE)
			wlc_bss_mac_event(wlc, bsscfg, WLC_E_PRUNE, &val_tmp->ea, 0,
				WLC_E_PRUNE_ENCR_MISMATCH, 0, 0, 0);
		break;
	}
#ifdef STA
	case IOV_GVAL(IOV_EARLY_BCN_THRESH):
		*ret_int_ptr = wlc->ebd->thresh;
		break;

	case IOV_SVAL(IOV_EARLY_BCN_THRESH):
		wlc->ebd->thresh = int_val;
		if (int_val == 0)
			memset(wlc->ebd, 0, sizeof(wlc_early_bcn_detect_t));
		break;
#endif /* STA */

#ifdef WLPM_BCNRX
	case IOV_SVAL(IOV_PM_BCNRX): {
		if (!BSSCFG_STA(bsscfg)) {
			err = BCME_EPERM;
			break;
		}

#if defined(AP) && (defined(RADIO_PWRSAVE) || defined(RXCHAIN_PWRSAVE))
		if (RADIO_PWRSAVE_ENAB(wlc->ap) || RXCHAIN_PWRSAVE_ENAB(wlc->ap)) {
			WL_ERROR(("wl%d: Not allowed, AP PS active\n",
			          wlc->pub->unit));

			/* modes are enabled */
			if (PM_BCNRX_ENAB(wlc->pub))
				wlc_pm_bcnrx_disable(wlc);
			err = BCME_EPERM;
			break;
		}
#endif /* AP && (RADIO_PWRSAVE || RXCHAIN_PWRSAVE) */

#if defined(WL_PWRSTATS)
		if (PWRSTATS_ENAB(wlc->pub)) {
			/* Counters depend on flag, so sync before applying */
			wlc_mimo_siso_metrics_snapshot(wlc, FALSE,
			    WL_MIMOPS_METRICS_SNAPSHOT_PMBCNRX);
		}
#endif /* WL_PWRSTATS */

		if (bool_val && !wlc_pm_bcnrx_allowed(wlc)) {
			/* Only trigger ucode if rxchain has not changed */
			err = wlc_pm_bcnrx_set(wlc, FALSE);
		} else
			err = wlc_pm_bcnrx_set(wlc, bool_val);

		if (!err)
			wlc->pub->cmn->_pm_bcnrx = bool_val;
		}
		break;

	case IOV_GVAL(IOV_PM_BCNRX):
		if (!BSSCFG_STA(bsscfg)) {
			err = BCME_EPERM;
			break;
		}

		*ret_int_ptr = (int32)(PM_BCNRX_ENAB(wlc->pub) ? TRUE : FALSE);
		break;
#endif /* WLPM_BCNRX */

	case IOV_GVAL(IOV_BSS_PEER_INFO): {
		bss_peer_list_info_t *peer_list_info = (bss_peer_list_info_t  *)arg;
		struct scb *scb;
		struct scb_iter scbiter;
		bss_peer_info_param_t	*in_param = (bss_peer_info_param_t	*)params;
		struct ether_addr ea;
		uint32 cnt = 0;

		if ((load16_ua(&in_param->version)) != BSS_PEER_INFO_PARAM_CUR_VER) {
			err = BCME_VERSION;
			break;
		}

		bcopy(&in_param->ea, &ea, sizeof(struct ether_addr));
		store16_ua((uint8 *)&peer_list_info->version, BSS_PEER_LIST_INFO_CUR_VER);
		store16_ua((uint8 *)&peer_list_info->bss_peer_info_len, sizeof(bss_peer_info_t));

		/* If it is STA, return the current AP's info alone */
		if (BSSCFG_STA(bsscfg) && bsscfg->BSS && ETHER_ISNULLDEST(&ea)) {
			bcopy(&bsscfg->BSSID, &ea, ETHER_ADDR_LEN);
		}

		/* Get only the peer info specified by the mac address */
		if (!ETHER_ISNULLDEST(&ea)) {
			if ((scb = wlc_scbfind(wlc, bsscfg, &ea)) == NULL) {
				err = BCME_BADADDR;
				break;
			}

			if (BSSCFG_AP(bsscfg) && !SCB_ASSOCIATED(scb)) {
				err = BCME_BADADDR;
				break;
			}

			store32_ua((uint8 *)&peer_list_info->count, 1);
			wlc_copy_peer_info(bsscfg, scb, peer_list_info->peer_info);
			break;
		}

		len -= BSS_PEER_LIST_INFO_FIXED_LEN;
		FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
			if (BSSCFG_AP(bsscfg) && !SCB_ASSOCIATED(scb)) {
				continue;
			}

			cnt++;
			if (len < (int)sizeof(bss_peer_info_t)) {
				/* return the actual no of peers with error */
				err = BCME_BUFTOOSHORT;
				continue;
			}

			wlc_copy_peer_info(bsscfg, scb, &peer_list_info->peer_info[cnt - 1]);
			len -= sizeof(bss_peer_info_t);
		}
		store32_ua((uint8 *)&peer_list_info->count, cnt);
		break;
	}
#ifdef ATE_BUILD
	case IOV_SVAL(IOV_GPAIO): {
		wlc_phy_gpaio(wlc->hw->band->pi, int_val, int_val2);
		break;
	}
#endif // endif
#if defined(WLRSDB)
	case IOV_GVAL(IOV_IOC):

		switch (int_val) {
			case WLC_UP:
			case WLC_DOWN:
				return BCME_UNSUPPORTED;
		}

		err = wlc_ioctl(wlc, int_val, (void*)((int8*)params + sizeof(int32)),
			p_len - sizeof(int32), wlcif);
		memmove(arg, (void*)((int8*)params + sizeof(int32)), p_len - sizeof(int32));
		break;
	case IOV_SVAL(IOV_IOC):

		switch (int_val) {
			case WLC_UP:
			case WLC_DOWN:
				return BCME_UNSUPPORTED;
		}

		err = wlc_ioctl(wlc, int_val, (void*)((int8*)params + sizeof(int32)),
			p_len - sizeof(int32), wlcif);
		break;
#endif /* WLRSDB */

	case IOV_GVAL(IOV_TCPACK_FAST_TX):
		*ret_int_ptr = (int32)wlc->tcpack_fast_tx;
		break;

	case IOV_SVAL(IOV_TCPACK_FAST_TX):
		wlc->tcpack_fast_tx = bool_val;
		break;

	case IOV_GVAL(IOV_WD_ON_BCN):
		*ret_int_ptr = mboolisset(wlc->wd_state, WATCHDOG_ON_BCN);
		break;

	case IOV_SVAL(IOV_WD_ON_BCN):
		if (bool_val)
			 mboolset(wlc->wd_state, WATCHDOG_ON_BCN);
		else
			 mboolclr(wlc->wd_state, WATCHDOG_ON_BCN);
		break;

#if !defined(NDIS) && (!defined(WLC_HOSTOID) || defined(HOSTOIDS_DISABLED))
	case IOV_GVAL(IOV_IF_COUNTERS):
	{
		wl_if_stats_t	*pub_stats;
		pub_stats = (wl_if_stats_t *)arg;
		wlc_wlcif_stats_get(wlc, wlcif, pub_stats);
		break;
	}
#endif /* !NDIS && (!WLC_HOSTOID || HOSTOIDS_DISABLED) */
	case IOV_GVAL(IOV_RXFIFO_COUNTERS): {
		uint reqlen;
		reqlen = sizeof(wl_rxfifo_cnt_t);
		if (len < reqlen)
			return BCME_BUFTOOSHORT;

		memcpy(arg, wlc->pub->_rxfifo_cnt, reqlen);
		break;
		}

	case IOV_GVAL(IOV_LIFETIME_CTRL):
		*ret_int_ptr = wlc->lifetime_ctrl / 1000;
		break;
	case IOV_SVAL(IOV_LIFETIME_CTRL):
		if (int_val <= WL_LIFETIME_MAX)
			wlc->lifetime_ctrl = int_val * 1000;
		else
			err = BCME_BADARG;
		break;

#if defined(WL_DATAPATH_LOG_DUMP) && defined(BCMDBG)
	case IOV_GVAL(IOV_DP_DUMP):
		wlc_datapath_log_dump(wlc, EVENT_LOG_TAG_WL_ERROR);
		*ret_int_ptr = 0;
		break;
	case IOV_SVAL(IOV_DP_DUMP):
		wlc_datapath_log_dump(wlc, int_val);
		break;
#endif /* WL_DATAPATH_LOG_DUMP && BCMDBG */
#if defined(WL_DD_HANDLER)
	case IOV_GVAL(IOV_HC):
	case IOV_SVAL(IOV_HC):
		err = wlc_hc_iovar_handler(wlc, wlcif, IOV_ISSET(actionid),
		                           params, p_len, arg, (uint)len);
		break;
#endif /* WL_DD_HANDLER */

	case IOV_GVAL(IOV_BEACON_INFO): {
#ifdef AP
		if (BSSCFG_AP(bsscfg) && bsscfg->up) {
			uint tmp_len = len - sizeof(int_val);

			err = wlc_ap_get_bcnprb(wlc, bsscfg, TRUE, ((int32*)arg + 1), &tmp_len);
			if (err == BCME_OK) {
				*ret_int_ptr = (int32)tmp_len;
			}
		} else
#endif /* AP */
#ifdef STA
		if (BSSCFG_INFRA_STA(bsscfg)) {
			uint tmp_len = len - sizeof(int_val);

			err = wlc_sta_get_infra_bcn(wlc, bsscfg, ((int32*)arg + 1), &tmp_len);
			if (err == BCME_OK) {
				*ret_int_ptr = (int32)tmp_len;
			}
		} else
#endif /* STA */
		*ret_int_ptr = 0;

		break;
	}

	case IOV_GVAL(IOV_WLC_VER):
		wlc_iov_get_wlc_ver((wl_wlc_version_t *)arg);
		break;

	case IOV_GVAL(IOV_DYN160):
#if defined(DYN160) && !defined(DYN160_DISABLED)
		*ret_int_ptr = DYN160_ACTIVE(wlc->pub);
#else
		err = BCME_UNSUPPORTED;
#endif /* DYN160 && DYN160_DISABLED */
		break;

	case IOV_SVAL(IOV_DYN160):
#if defined(DYN160) && !defined(DYN160_DISABLED)
		DYN160_ACTIVE_SET(wlc->pub, (uint32) int_val);
#else
		err = BCME_UNSUPPORTED;
#endif /* DYN160 && DYN160_DISABLED */
		break;

	case IOV_GVAL(IOV_TXPWR_DEGRADE):
		*ret_int_ptr = wlc->txpwr_degrade;
		break;
	case IOV_SVAL(IOV_TXPWR_DEGRADE):
		wlc->txpwr_degrade = (uint8)int_val;
		wlc_bmac_set_txpwr_degrade(wlc->hw, (uint8)int_val);
		if (wlc->pub->up) {
			uint8 constraint;

			if (SCAN_IN_PROGRESS(wlc->scan)) {
				WL_INFORM(("wl%d: Scan in progress, skipping txpower control\n",
					wlc->pub->unit));
				break;
			}

			wlc_suspend_mac_and_wait(wlc);

			/* Set the power limits for this locale after computing
			 * any 11h local tx power constraints.
			 */
			constraint = wlc_tpc_get_local_constraint_qdbm(wlc->tpc);
			wlc_channel_set_txpower_limit(wlc->cmi, constraint);

			wlc_enable_mac(wlc);
		}
		break;

	case IOV_SVAL(IOV_ASSOC_DECISION): {
		assoc_decision_t *dc_bufp;

		if (len < (int)sizeof(assoc_decision_t) - 1) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		if (!(dc_bufp = MALLOC(wlc->osh, len))) {
			WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			err = BCME_NOMEM;
			break;
		}
		bcopy((uint8*)arg, dc_bufp, len);

#if defined(AP) && defined(SPLIT_ASSOC)
		if (BSSCFG_AP(bsscfg)) {
			wlc_ap_process_assocreq_decision(wlc, bsscfg, dc_bufp);
		}
#endif /* defined(AP) && defined(SPLIT_ASSOC) */
#ifdef STA
		if (BSSCFG_STA(bsscfg)) {
			wlc_process_assocresp_decision(wlc, bsscfg, dc_bufp);
		}
#endif // endif
		MFREE(wlc->osh, dc_bufp, len);
		break;
	}
#ifdef SPLIT_ASSOC
	case IOV_GVAL(IOV_SPLIT_ASSOC_REQ):
		*ret_int_ptr = ((int32)(bsscfg->flags2 & WLC_BSSCFG_FL2_SPLIT_ASSOC_REQ) == 0 ?
			FALSE : TRUE);
		break;
	case IOV_SVAL(IOV_SPLIT_ASSOC_REQ):
		if (bool_val) {
			bsscfg->flags2 |= WLC_BSSCFG_FL2_SPLIT_ASSOC_REQ;
		} else {
			bsscfg->flags2 &= ~WLC_BSSCFG_FL2_SPLIT_ASSOC_REQ;
		}
		break;

	case IOV_GVAL(IOV_SPLIT_ASSOC_RESP):
		*ret_int_ptr = ((int32)(bsscfg->flags2 &
			WLC_BSSCFG_FL2_SPLIT_ASSOC_RESP) == 0 ? FALSE : TRUE);
		break;

	case IOV_SVAL(IOV_SPLIT_ASSOC_RESP):
		if (bool_val) {
			bsscfg->flags2 |= WLC_BSSCFG_FL2_SPLIT_ASSOC_RESP;
		} else {
			bsscfg->flags2 &= ~WLC_BSSCFG_FL2_SPLIT_ASSOC_RESP;
		}
		break;
#endif /* SPLIT_ASSOC */

#ifdef BCM_SKB_FREE_OFFLOAD
	case IOV_SVAL(IOV_SKB_FREE_OFFLOAD):
		((osl_pubinfo_t*)(wlc->osh))->skb_free_offload = bool_val;
		break;
	case IOV_GVAL(IOV_SKB_FREE_OFFLOAD):
		*ret_int_ptr = ((osl_pubinfo_t*)(wlc->osh))->skb_free_offload;
		break;
#endif /* BCM_SKB_FREE_OFFLOAD */

	case IOV_GVAL(IOV_MAXSCB):
		*ret_int_ptr = (int32)wlc->pub->tunables->maxscb;
		break;

	case IOV_SVAL(IOV_MAXSCB):
		/* can only change if the config is down */
		if (bsscfg->up) {
			err = BCME_NOTDOWN;
			break;
		}

		if (int_val > MAXSCB) {
			WL_ERROR(("Maximum %d SCBs supported\n", MAXSCB));
			err = BCME_RANGE;
		} else if (int_val < wlc_ap_get_maxassoc(wlc->ap)) {
			WL_ERROR(("maxsb (%d) cannot be less than maxassoc (%d)\n",
				int_val, wlc_ap_get_maxassoc(wlc->ap)));
			err = BCME_RANGE;
		} else if (int_val < bsscfg->maxassoc) {
			WL_ERROR(("maxsb (%d) cannot be less than bss_maxassoc (%d)\n",
				int_val, bsscfg->maxassoc));
			err = BCME_RANGE;
		} else {
			/* set max # of STAs allowed to join */
			wlc->pub->tunables->maxscb = int_val;
		}

		break;

	case IOV_GVAL(IOV_ACTIVE_BIDIR_THRESH):
		*ret_int_ptr = wlc->active_bidir_thresh;
		break;

	case IOV_SVAL(IOV_ACTIVE_BIDIR_THRESH):
		if (int_val > 0)
			WL_ERROR(("Enabling dynamic frameburst\n"));
		wlc->active_bidir_thresh = int_val;
		wlc->active_bidir = FALSE;
		wlc->active_udpv6 = FALSE;
		wlc->active_udpv4 = FALSE;
		wlc->active_tcp = FALSE;
		break;

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	goto exit;	/* avoid unused label warning */

exit:
	return err;
} /* wlc_doiovar */

#if defined(WL_DATAPATH_LOG_DUMP)
/**
 * Use EVENT_LOG to dump a summary of the datapath state
 * @param wlc   pointer to wlc_info state structure
 * @param tag   EVENT_LOG tag for output
 */
void
wlc_datapath_log_dump(wlc_info_t *wlc, int tag)
{
	int32 idx;
	wlc_bsscfg_t *cfg;

	/* dump the association of cubby name IDs to strings */
	wlc_scb_cubby_name_dump(wlc->scbstate, tag);

	/* dump the main TxQ contexts */
	wlc_tx_datapath_log_dump(wlc, tag);

	/* Dump datapath info for each BSSCFG and associated SCBs */
	FOREACH_BSS(wlc, idx, cfg) {
		struct scb *scb;
		struct scb_iter scbiter;

		wlc_bsscfg_datapath_log_dump(cfg, tag);

		FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {
			wlc_scb_datapath_log_dump(wlc, scb, tag);
		}
	}

	/* Dump the TxStatus history */
	wlc_txs_hist_log_dump(wlc->hw, tag);
}
#endif /* WL_DATAPATH_LOG_DUMP */

static void
BCMATTACHFN(wlc_mfbr_retrylimit_upd)(wlc_info_t *wlc)
{
	int ac;

	wlc->LRL = RETRY_LONG_DEF_AQM;

	wlc_bmac_retrylimit_upd(wlc->hw, wlc->SRL, wlc->LRL);

	for (ac = 0; ac < AC_COUNT; ac++)
		WLC_WME_RETRY_LONG_SET(wlc, ac, wlc->LRL);

	wlc_wme_retries_write(wlc);
}

/**
 * Convert a wlc_bss_info_t into an external wl_bss_info_t. Destination pointer does not need to be
 * aligned.
 */
/* XXX Currently, this is only used for the ioctl GET_BSS_INFO and for getting
 * scan results.  If the IE elements of the wl_bss_info_t structure are ever
 * referenced for a non-primary BSS in AP mode, they will probably be wrong.
 * One solution to this is to pass a BSS config structure (wlc_bsscfg_t) to
 * this routine and fill out BSS cfg specific info from that if it is non-NULL.
 */
int
wlc_bss2wl_bss(wlc_info_t *wlc, wlc_bss_info_t *bi, wl_bss_info_t *to_bi, int to_bi_len,
	bool need_ies)
{
	wl_bss_info_t wl_bi;
	uint bi_length;
	int ie_length;
	uint16 ie_offset;
	uint8 *ie;

	BCM_REFERENCE(wlc);

	/* user buffer should at least hold fixed portion of bss info */
	if (to_bi_len < (int) sizeof(wl_bss_info_t)) {
		WL_ERROR((WLC_BSS_INFO_LEN_ERR, __FUNCTION__, to_bi_len,
			(int)sizeof(wl_bss_info_t)));
		return BCME_BUFTOOSHORT;
	}

	/* init ie length, point to IEs */
	if (bi->bcn_prb) {
		ie_length = bi->bcn_prb_len - DOT11_BCN_PRB_LEN;
		ie_offset = sizeof(wl_bss_info_t);
		if (ie_length > 0) {
			ie = (uint8 *)bi->bcn_prb + DOT11_BCN_PRB_LEN;
		} else {
			ie_length = 0;
			ie_offset = 0;
			ie = NULL;
		}
	} else {
		/* no IEs */
		ie_length = 0;
		ie_offset = 0;
		ie = NULL;
	}

	/* check for user buffer again to see if it can take IEs */
	if (ie_length) {
		/* adjust bss info length to hold variable length IEs as well */
		bi_length = sizeof(wl_bss_info_t) + ROUNDUP(ie_length, 4);
		/* check again for user buffer length */
		if (to_bi_len < (int) bi_length) {
			if (need_ies) {
				WL_ERROR((WLC_BSS_INFO_LEN_ERR, __FUNCTION__, to_bi_len,
					(int)bi_length));
				return BCME_BUFTOOSHORT;
			}
			bi_length = sizeof(wl_bss_info_t); /* just the fixed bss info */
			ie_length = 0;
			ie_offset = 0;
			ie = NULL;
		}
	} else {
		bi_length = sizeof(wl_bss_info_t);
	}

	bzero(&wl_bi, sizeof(wl_bss_info_t));

	/* constant values for wl_bss_info */
	wl_bi.version		= WL_BSS_INFO_VERSION;
	wl_bi.length		= bi_length;

	/* simple copies from wlc_bss_info */
	wl_bi.beacon_period	= bi->beacon_period;
	wl_bi.capability	= bi->capability;
	wl_bi.chanspec		= bi->chanspec;
	wl_bi.atim_window	= bi->atim_window;
	wl_bi.dtim_period	= bi->dtim_period;
	wl_bi.RSSI		= bi->RSSI;
	wl_bi.SNR		= bi->SNR;
	wl_bi.phy_noise		= bi->phy_noise;

	bcopy(bi->BSSID.octet, wl_bi.BSSID.octet, ETHER_ADDR_LEN);

	wl_bi.SSID_len	= bi->SSID_len;
	bcopy(bi->SSID, wl_bi.SSID, wl_bi.SSID_len);

	if (bi->flags & WLC_BSS_BEACON)
		wl_bi.flags |= WL_BSS_FLAGS_FROM_BEACON;
	if (bi->flags & WLC_BSS_CACHE)
		wl_bi.flags |= WL_BSS_FLAGS_FROM_CACHE;
	if (bi->flags & WLC_BSS_RSSI_ON_CHANNEL)
		wl_bi.flags |= WL_BSS_FLAGS_RSSI_ONCHANNEL;
	if (bi->flags2 & WLC_BSS_RSSI_INVALID)
		wl_bi.flags |= WL_BSS_FLAGS_RSSI_INVALID;
	if (bi->flags2 & WLC_BSS_RSSI_INACCURATE)
		wl_bi.flags |= WL_BSS_FLAGS_RSSI_INACCURATE;
	if (bi->flags2 & WLC_BSS_HS20)
		wl_bi.flags |= WL_BSS_FLAGS_HS20;

	/* limit rates to destination rate array size */
	wl_bi.rateset.count = MIN(bi->rateset.count, sizeof(wl_bi.rateset.rates));
	bcopy(bi->rateset.rates, wl_bi.rateset.rates, wl_bi.rateset.count);

	/* Some 802.11N related capabilities */
	if (bi->flags & WLC_BSS_HT) {
		wl_bi.n_cap = TRUE;
		if (bi->flags & WLC_BSS_40MHZ)
			wl_bi.nbss_cap |= HT_CAP_40MHZ;
		if (bi->flags & WLC_BSS_SGI_20)
			wl_bi.nbss_cap |= HT_CAP_SHORT_GI_20;
		if (bi->flags & WLC_BSS_SGI_40)
			wl_bi.nbss_cap |= HT_CAP_SHORT_GI_40;
		/* Copy Basic MCS set */
		bcopy(&bi->rateset.mcs[0], &wl_bi.basic_mcs[0], MCSSET_LEN);
		wl_bi.ctl_ch = wf_chspec_ctlchan(bi->chanspec);
	}

#ifdef WL11AC
	if (bi->flags2 & WLC_BSS_VHT) {
		wl_bi.vht_cap = TRUE;
		if ((bi->flags2 & WLC_BSS_SGI_80)) {
			wl_bi.nbss_cap |= VHT_BI_SGI_80MHZ;
		}
		wl_bi.vht_rxmcsmap = bi->vht_rxmcsmap;
		wl_bi.vht_txmcsmap = bi->vht_txmcsmap;
		wl_bi.vht_txmcsmap_prop = bi->vht_txmcsmap_prop;
	}
#endif /* WL11AC */
#ifdef WL11AX
	if (bi->flags3 & WLC_BSS3_HE) {
		wl_bi.he_cap = TRUE;
		wl_bi.he_sup_bw80_tx_mcs = bi->he_sup_bw80_tx_mcs;
		wl_bi.he_sup_bw80_rx_mcs = bi->he_sup_bw80_rx_mcs;
		wl_bi.he_sup_bw160_tx_mcs = bi->he_sup_bw160_tx_mcs;
		wl_bi.he_sup_bw160_rx_mcs = bi->he_sup_bw160_rx_mcs;
		wl_bi.he_sup_bw80p80_tx_mcs = bi->he_sup_bw80p80_tx_mcs;
		wl_bi.he_sup_bw80p80_rx_mcs = bi->he_sup_bw80p80_rx_mcs;
		wl_bi.he_neg_bw80_tx_mcs = bi->he_neg_bw80_tx_mcs;
		wl_bi.he_neg_bw80_rx_mcs = bi->he_neg_bw80_rx_mcs;
		wl_bi.he_neg_bw160_tx_mcs = bi->he_neg_bw160_tx_mcs;
		wl_bi.he_neg_bw160_rx_mcs = bi->he_neg_bw160_rx_mcs;
		wl_bi.he_neg_bw80p80_tx_mcs = bi->he_neg_bw80p80_tx_mcs;
		wl_bi.he_neg_bw80p80_rx_mcs = bi->he_neg_bw80p80_rx_mcs;
	}
#endif /* WL11AX */
	/* ie length */
	wl_bi.ie_length = ie_length;
	wl_bi.ie_offset = ie_offset;

	/* copy fixed portion of the bss info structure to user buffer */
	bcopy(&wl_bi, to_bi, sizeof(wl_bss_info_t));

	/* append beacon/probe response IEs */
	if (ie_length)
		bcopy(ie, (uint8 *)to_bi + sizeof(wl_bss_info_t), ie_length);
	return 0;
} /* wlc_bss2wl_bss */

#if defined(BCMDBG)
static int
wlc_dump_ratestuff(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	uint i;
	uint r;

#if defined(WL11AX)
	wlc_dump_he_rateset("\nhw_hemap ", &wlc->band->hw_rateset, b);
#endif // endif

#if defined(WL11AC)
	wlc_dump_vht_mcsmap("\nhw_vhtmap ", wlc->band->hw_rateset.vht_mcsmap, b);
	wlc_dump_vht_mcsmap_prop("\nhw_vhtmap_prop ", wlc->band->hw_rateset.vht_mcsmap_prop, b);
#endif /* WL11AC */

#ifdef WL11N
	wlc_dump_mcsset("\nhw_mcsset ", &wlc->band->hw_rateset.mcs[0], b);
#endif // endif
	bcm_bprintf(b, "\n");
	bcm_bprintf(b, "basic_rate: ");
	for (i = 0; i < sizeof(wlc->band->basic_rate); i++)
		if ((r = wlc->band->basic_rate[i]))
			bcm_bprintf(b, "%d%s->%d%s ",
				(i / 2), (i % 2)?".5":"",
				(r / 2), (r % 2)?".5":"");
	bcm_bprintf(b, "\n");

	return 0;
}
#endif // endif

#if defined(BCMDBG)
static int
wlc_dump_wlc(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	wlcband_t *band;
	uint fifo_size[NFIFO_LEGACY];
	char perm[32], cur[32];
	char chn[8];
	uint16 maj_rev, min_rev, features, dbgst;
	uint32 repro_rev; /* repro rev */
	uint i;
	wlc_bsscfg_t *cfg;
	wlc_txq_info_t *qi;
	wlc_if_t *wlcif;
	wlc_tunables_t *tunables;
	char chanbuf[CHANSPEC_STR_LEN];

	/* read ucode revision info */
	maj_rev = wlc->ucode_rev >> NBITS(uint16);
	min_rev = wlc->ucode_rev & 0xffff;

	/* skip accessing registers if the clock is off */
	if (!wlc->clk) {
		repro_rev = 0;
		features = dbgst = 0;
	} else {
		repro_rev = wlc_read_shm(wlc, M_REV_L(wlc)) |
			(wlc_read_shm(wlc, M_REV_H(wlc)) << 16);
		features = wlc_read_shm(wlc, M_UCODE_FEATURES(wlc));
		/* read ucode debug status */
		dbgst = wlc_read_shm(wlc, M_UCODE_DBGST(wlc));
	}

	bcm_bprintf(b, "\n");

	bcm_bprintf(b, "wl%d: wlc %p wl %p msglevel 0x%x clk %d up %d hw_off %d now %d\n",
	            wlc->pub->unit, OSL_OBFUSCATE_BUF(wlc), OSL_OBFUSCATE_BUF(wlc->wl),
			wl_msg_level, wlc->clk, wlc->pub->up, wlc->pub->hw_off, wlc->pub->now);

	bcm_bprintf(b, "ucode %d.%d repro_rev %d features 0x%04x dbgst 0x%x d11war_flags 0x%x\n",
		maj_rev, min_rev, repro_rev, features, dbgst,
		wlc_bmac_get_d11war_flags(wlc->hw));
	bcm_bprintf(b, "capabilities: ");
	wlc_cap_bcmstrbuf(wlc, b);
	bcm_bprintf(b, "\n");

	bcm_bprintf(b, "promisc %d monitor %d piomode %d gmode %d\n",
	            wlc->pub->promisc, wlc->monitor, PIO_ENAB(wlc->pub), wlc->band->gmode);

	bcm_bprintf(b, "ap %d apsta %d wet %d wet_dongle %d wme %d mac_spoof %d"
	            " per-ac maxrate %d\n",
	            AP_ENAB(wlc->pub), APSTA_ENAB(wlc->pub), wlc->wet, wlc->wet_dongle,
	            wlc->pub->_wme, wlc->mac_spoof, WME_PER_AC_MAXRATE_ENAB(wlc->pub));
	bcm_bprintf(b, "vendor 0x%x device 0x%x nbands %d regs %p\n",
	            wlc->vendorid, wlc->deviceid, NBANDS(wlc), OSL_OBFUSCATE_BUF(wlc->regs));

	bcm_bprintf(b, "chip %s chiprev %d corerev %d maccap 0x%x\n",
	            bcm_chipname(SI_CHIPID(wlc->pub->sih), chn, 8), wlc->pub->sih->chiprev,
	            wlc->pub->corerev, wlc->machwcap);

	bcm_bprintf(b, "boardvendor 0x%x boardtype 0x%x boardrev %s "
	            "boardflags 0x%x boardflags2 0x%x sromrev %d\n",
	            wlc->pub->sih->boardvendor, wlc->pub->sih->boardtype,
	            bcm_brev_str(wlc->pub->boardrev, cur), wlc->pub->boardflags,
	            wlc->pub->boardflags2, wlc->pub->sromrev);
	if (wlc->pub->boardrev == BOARDREV_PROMOTED)
		bcm_bprintf(b, " (actually 0x%02x)", BOARDREV_PROMOTABLE);
	bcm_bprintf(b, "\n");

	bcm_bprintf(b, "perm_etheraddr %s cur_etheraddr %s\n",
	            bcm_ether_ntoa(&wlc->perm_etheraddr, (char*)perm),
	            bcm_ether_ntoa(&wlc->pub->cur_etheraddr, (char*)cur));

	bcm_bprintf(b, "swdecrypt %d\n",
	            wlc->pub->swdecrypt);
	bcm_bprintf(b, "\nuserfragthresh %d, fragthresh %d/%d/%d/%d RTSThresh %d \n",
	            wlc->usr_fragthresh, wlc->fragthresh[0], wlc->fragthresh[1],
	            wlc->fragthresh[2], wlc->fragthresh[3], wlc->RTSThresh);

	bcm_bprintf(b, "\nSRL %d LRL %d SFBL %d LFBL %d\n",
	            wlc->SRL, wlc->LRL, wlc->SFBL, wlc->LFBL);

	bcm_bprintf(b, "shortslot %d shortslot_ovrrd %d ignore_bcns %d\n",
	            wlc->shortslot, wlc->shortslot_override, wlc->ignore_bcns);

	bcm_bprintf(b, "\nblock_datafifo 0x%x tx_suspended %d\n",
	            wlc->block_datafifo, wlc->tx_suspended);

	bcm_bprintf(b, "bandunit %d bandlocked %d \n", wlc->band->bandunit, wlc->bandlocked);
	bcm_bprintf(b, "radio_disabled 0x%x down_override %d\n", wlc->pub->radio_disabled,
	            wlc->down_override);
#ifdef WL11AC
	bcm_bprintf(b, "vht_features  0x%x\n", wlc->pub->vht_features);
#endif // endif

#ifdef STA
	bcm_bprintf(b, "mpc %d, mpc_scan %d mpc_join %d mpc_oidscan %d mpc_oidjoin %d"
	            " mpc_oidnettype %d mpc_out %d\n",
	            wlc->mpc, wlc->mpc_scan, wlc->mpc_join, wlc->mpc_oidscan, wlc->mpc_oidjoin,
	            wlc->mpc_oidnettype, wlc->mpc_out);
#endif // endif

	bcm_bprintf(b, "5G band: ratespec_override 0x%x mratespec_override 0x%x\n",
	            wlc->bandstate[BAND_5G_INDEX]->rspec_override,
	            wlc->bandstate[BAND_5G_INDEX]->mrspec_override);

	bcm_bprintf(b, "2G band: ratespec_override 0x%x mratespec_override 0x%x\n",
	            wlc->bandstate[BAND_2G_INDEX]->rspec_override,
	            wlc->bandstate[BAND_2G_INDEX]->mrspec_override);
	bcm_bprintf(b, "\n");

	FOREACH_BSS(wlc, i, cfg) {
		bcm_bprintf(b, "PLCPHdr_ovrrd %d\n",
			cfg->PLCPHdr_override);
	}

	bcm_bprintf(b, "CCK_power_boost %d \n", (wlc->pub->boardflags & BFL_CCKHIPWR) ? 1 : 0);

	bcm_bprintf(b, "mhf 0x%02x mhf2 0x%02x mhf3 0x%02x mhf4 0x%02x mhf5 0x%02x\n",
		wlc_bmac_mhf_get(wlc->hw, MHF1, WLC_BAND_AUTO),
		wlc_bmac_mhf_get(wlc->hw, MHF2, WLC_BAND_AUTO),
		wlc_bmac_mhf_get(wlc->hw, MHF3, WLC_BAND_AUTO),
		wlc_bmac_mhf_get(wlc->hw, MHF4, WLC_BAND_AUTO),
		wlc_bmac_mhf_get(wlc->hw, MHF5, WLC_BAND_AUTO));

	bcm_bprintf(b, "swdecrypt %d\n", wlc->pub->swdecrypt);

#ifdef STA
	bcm_bprintf(b, "\n");
	bcm_bprintf(b, "STAY_AWAKE() %d wake %d PMpending %d\n",
	            STAY_AWAKE(wlc), wlc->wake, wlc->PMpending);
	FOREACH_AS_STA(wlc, i, cfg) {
		bcm_bprintf(b, "wsec_portopen %d WLC_PORTOPEN %d\n",
		            cfg->wsec_portopen, WLC_PORTOPEN(cfg));

		bcm_bprintf(b, "PS_ALLOWED() %d\n", PS_ALLOWED(cfg));
		bcm_bprintf(b, "dtim_programmed %d\n", cfg->dtim_programmed);

		bcm_bprintf(b, "BSSID %s BSS %d\n",
		            bcm_ether_ntoa(&cfg->BSSID, (char*)cur),
		            cfg->BSS);
		bcm_bprintf(b, "reprate %dkbps\n",
		            RSPEC2KBPS(wlc_get_rspec_history(cfg)));

		bcm_bprintf(b, "\n");
	}
#endif /* STA */

	bcm_bprintf(b, "associated %d stas_associated %d aps_associated %d\n",
	            wlc->pub->associated, wlc->stas_associated, wlc->aps_associated);

	FOREACH_BSS(wlc, i, cfg) {
		bcm_bprintf(b, "BSSID %s\n", bcm_ether_ntoa(&cfg->BSSID, (char*)perm));
	}

	if (wlc->pub->up)
		bcm_bprintf(b, "chanspec %s\n",
			wf_chspec_ntoa_ex(WLC_BAND_PI_RADIO_CHANSPEC, chanbuf));
	else
		bcm_bprintf(b, "chan N/A ");

	bcm_bprintf(b, "country \"%s\"\n", wlc_channel_country_abbrev(wlc->cmi));

	bcm_bprintf(b, "counter %d\n", wlc->counter & 0xfff);

	bcm_bprintf(b, "\n");

	for (qi = wlc->tx_queues; qi != NULL; qi = qi->next) {
		bcm_bprintf(b, "txqinfo %p len %d stopped 0x%x\n",
		            OSL_OBFUSCATE_BUF(qi), pktq_n_pkts_tot(WLC_GET_CQ(qi)), qi->stopped);
		bcm_bprintf(b, "associated wlcifs:");

		for (wlcif = wlc->wlcif_list;  wlcif != NULL; wlcif = wlcif->next) {
			char ifname[32];

			if (wlcif->qi != qi)
				continue;

			strncpy(ifname, wl_ifname(wlc->wl, wlcif->wlif), sizeof(ifname));
			ifname[sizeof(ifname) - 1] = '\0';

			bcm_bprintf(b, " \"%s\" 0x%p", ifname, OSL_OBFUSCATE_BUF(wlcif));
		}
		bcm_bprintf(b, "\n");
	}

	bcm_bprintf(b, "\n");

	bcm_bprintf(b, "malloc_failed %d\n", MALLOC_FAILED(wlc->osh));

	bcm_bprintf(b, "txpktpend AC_BK %d AC_BE %d AC_VI %d AC_VO %d BCMC %d fifo5 %d "
	            "total_tx_pkt %d\n",
	            TXPKTPENDGET(wlc, TX_AC_BK_FIFO), TXPKTPENDGET(wlc, TX_AC_BE_FIFO),
	            TXPKTPENDGET(wlc, TX_AC_VI_FIFO), TXPKTPENDGET(wlc, TX_AC_VO_FIFO),
	            TXPKTPENDGET(wlc, TX_BCMC_FIFO), TXPKTPENDGET(wlc, TX_ATIM_FIFO),
	            wlc_txpktcnt(wlc));

	bcm_bprintf(b, "pkt_callback_reg_fail %d tx_stopped_map %x\n",
		WLCNTVAL(wlc->pub->_cnt->pkt_callback_reg_fail),
		txq_stopped_map(wlc->active_queue->low_txq));

	for (i = 0; i < NFIFO_LEGACY; i++) {
		if (wlc_bmac_xmtfifo_sz_get(wlc->hw, i, &fifo_size[i]))
			fifo_size[i] = 0;
	}
	bcm_bprintf(b, "xmtfifo_sz(in unit of 256B)");
	bcm_bprintf(b, "AC_BK %d AC_BE %d AC_VI %d AC_VO %d 5th %d 6th %d\n",
	            fifo_size[TX_AC_BK_FIFO],
	            fifo_size[TX_AC_BE_FIFO],
	            fifo_size[TX_AC_VI_FIFO],
	            fifo_size[TX_AC_VO_FIFO],
	            fifo_size[4],
	            fifo_size[5]);

	band = wlc->bandstate[IS_SINGLEBAND_5G(wlc->deviceid, wlc->phy_cap) ?
		BAND_5G_INDEX : BAND_2G_INDEX];
	bcm_bprintf(b, "(ROAMThreshold, ROAMDelta) (2.4G) default: %d, %d::current: %d, %d\n",
	            band->roam_trigger_def, band->roam_delta_def,
	            band->roam_trigger, band->roam_delta);

	if (NBANDS(wlc) > 1) {
		band = wlc->bandstate[BAND_5G_INDEX];
		bcm_bprintf(b, "(ROAMThreshold, ROAMDelta) (5G) default: %d, %d::current: %d, %d\n",
		            band->roam_trigger_def, band->roam_delta_def,
		            band->roam_trigger, band->roam_delta);
	}

	if (wlc->stf->throttle_state) {
		bcm_bprintf(b, "state:%d duty cycle:%d rxchain: %x txchain: %x\n",
		            wlc->stf->throttle_state,
			TVPM_ENAB(wlc->pub) ? wlc_tvpm_get_target_txdc(wlc->tvpm) :
			wlc->stf->tx_duty_cycle_pwr,
			wlc->stf->rxchain,
			wlc->stf->txchain);
	}

	tunables = wlc->pub->tunables;
	bcm_bprintf(b, "tunables:\n");
	bcm_bprintf(b, "\tntxd = %d, nrxd = %d, rxbufsz = %d, nrxbufpost = %d, maxscb = %d\n",
	            tunables->ntxd, tunables->nrxd, tunables->rxbufsz,
	            tunables->nrxbufpost, tunables->maxscb);
	bcm_bprintf(b, "\tampdunummpdu2streams = %d, ampdunummpdu3streams = %d\n",
	            tunables->ampdunummpdu2streams, tunables->ampdunummpdu3streams);
	bcm_bprintf(b, "\tmaxpktcb = %d,  maxucodebss = %d, maxucodebss4 = %d\n",
	            tunables->maxpktcb,
	            tunables->maxucodebss, tunables->maxucodebss4);
	bcm_bprintf(b, "\tmaxbss = %d, datahiwat = %d, ampdudatahiwat = %d, maxubss = %d\n",
	            tunables->maxbss, tunables->datahiwat, tunables->ampdudatahiwat,
	            tunables->maxubss);
	bcm_bprintf(b, "\trxbnd = %d, txsbnd = %d\n", tunables->rxbnd, tunables->txsbnd);
	bcm_bprintf(b, "\tampdu_pktq_size = %d, ampdu_pktq_fav_size = %d\n",
		tunables->ampdu_pktq_size, tunables->ampdu_pktq_fav_size);

#ifdef PROP_TXSTATUS
	bcm_bprintf(b, "\twlfcfifocreditac0 = %d, wlfcfifocreditac1 = %d, wlfcfifocreditac2 = %d, "
	            "wlfcfifocreditac3 = %d\n",
	            tunables->wlfcfifocreditac0, tunables->wlfcfifocreditac1,
	            tunables->wlfcfifocreditac2, tunables->wlfcfifocreditac3);
	bcm_bprintf(b, "\twlfcfifocreditbcmc = %d, wlfcfifocreditother = %d\n",
	            tunables->wlfcfifocreditbcmc, tunables->wlfcfifocreditother);
#endif // endif

	bcm_bprintf(b, "-------- hwrs scbs --------\n");
	for (i = 0; i < MAXBANDS; i ++) {
		if (wlc->bandstate[i] != NULL &&
		    wlc->bandstate[i]->hwrs_scb != NULL)
			wlc_scb_dump_scb(wlc, NULL, wlc->bandstate[i]->hwrs_scb, b, -1);
	}

#if defined(BCM_DMA_CT) && !defined(BCM_DMA_CT_DISABLED)
	bcm_bprintf(b, "dma_ct %d mu_tx %d\n", wlc->_dma_ct, wlc->_mu_tx);
#endif // endif

	return 0;
} /* wlc_dump_wlc */
#endif // endif

#if defined(BCMDBG_TXSTUCK)
static void
wlc_dump_txstuck(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	bcm_bprintf(b, "ampdu:\n");
	wlc_ampdu_print_txstuck(wlc, b);
	bcm_bprintf(b, "wlc:\n");
	bcm_bprintf(b, "wlc active q: %d, PM pending %d\n",
		pktq_n_pkts_tot(WLC_GET_CQ(wlc->active_queue)),
		wlc->PMpending);
	bcm_bprintf(b, "total tx: %d, hw fifo tx pending: %d\n",
		wlc_txpktcnt(wlc),
		TXPKTPENDTOT(wlc));
	bcm_bprintf(b, "block 0x%x\n", wlc->block_datafifo);
	if (wlc->txfifo_detach_pending) {
		bcm_bprintf(b, "wlc->txfifo_detach_pending %d\n",
			wlc->txfifo_detach_pending);
	}
	if (wlc->primary_queue == wlc->active_queue) {
		bcm_bprintf(b, "primary_queue == active_queue\n");
	}
	bcm_bprintf(b, "excursion active: %d\n", wlc->excursion_active);
	bcm_bprintf(b, "wlc_bsscfg:\n");
	wlc_bsscfg_print_txstuck(wlc, b);
	bcm_bprintf(b, "wlc_apps:\n");
	wlc_apps_print_scb_psinfo_txstuck(wlc, b);
	bcm_bprintf(b, "wlc_bmac:\n");
	wlc_bmac_print_muted(wlc, b);
}
#endif /* defined(BCMDBG_TXSTUCK) */

#if defined(BCMDBG)
int
wlc_dump_bss_info(const char *name, wlc_bss_info_t *bi, struct bcmstrbuf *b)
{
	char bssid[DOT11_MAX_SSID_LEN];
	char ssidbuf[SSID_FMT_BUF_LEN];
	char chanbuf[CHANSPEC_STR_LEN];

	wlc_format_ssid(ssidbuf, bi->SSID, bi->SSID_len);

	bcm_bprintf(b, "%s: %s BSSID %s\n",
	            name, wlc_bsstype_dot11name(bi->bss_type),
	            bcm_ether_ntoa(&bi->BSSID, bssid));
	bcm_bprintf(b, "SSID \"%s\" len %d ISBRCM %d ISHT %d ISVHT %d ISHE %d\n",
	            ssidbuf, bi->SSID_len, ((bi->flags & WLC_BSS_BRCM) != 0),
		((bi->flags & WLC_BSS_HT) != 0), ((bi->flags2 & WLC_BSS_VHT) != 0),
		((bi->flags3 & WLC_BSS3_HE) != 0));
	bcm_bprintf(b, "flags 0x%x flags2 0x%x flags3 0x%x\n",
	            bi->flags, bi->flags2, bi->flags3);
	bcm_bprintf(b, "channel %d chanspec %s beacon %d dtim %d atim %d capability 0x%04x"
		" flags 0x%02x RSSI %d\n", wf_chspec_ctlchan(bi->chanspec),
		wf_chspec_ntoa_ex(bi->chanspec, chanbuf), bi->beacon_period, bi->dtim_period,
		bi->atim_window, bi->capability, bi->flags, bi->RSSI);
	wlc_dump_rateset("rateset", &bi->rateset, b);
	if (bi->flags & WLC_BSS_HT) {
		wlc_dump_mcsset("\nmcs", &bi->rateset.mcs[0], b);
	}

#if defined(WL11AC)
	if (bi->flags2 & WLC_BSS_VHT) {
		wlc_dump_vht_mcsmap("\nvht", bi->rateset.vht_mcsmap, b);
		bcm_bprintf(b, "(rx %04x tx %04x)", bi->vht_rxmcsmap, bi->vht_txmcsmap);
		wlc_dump_vht_mcsmap_prop("\nprop vht", bi->rateset.vht_mcsmap_prop, b);
	}
#endif /* WL11AC */

#if defined(WL11AX)
	if (bi->flags3 & WLC_BSS3_HE) {
		wlc_dump_he_rateset("\nhe", &bi->rateset, b);
	}
#endif /* WL11AX */

	bcm_bprintf(b, "\n");

	return 0;
}
#endif // endif

#if defined(BCMDBG)
#if defined(WLVASIP)
static int
wlc_dump_vasip(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	wlc_vasip_code_info(wlc, b);
	return 0;
}
#endif /* WLVASIP */
#endif // endif

#if defined(WL_PSMX)
static void
wlc_macxstatupd(wlc_info_t *wlc)
{
	int i;
	uint16 macxstat_readbuf[MACXSTAT_OFFSET_SZ];
	uint16 *macxstat_snapshot = wlc->core->macxstat_snapshot;
	uint32 *macxstat_cnt = wlc->pub->_mcxst_cnt;

	if (!PSMX_ENAB(wlc->pub))
		return;

	/* Read macx stats from contiguous shared memory. MX_UCODEX_MACSTAT is a byte offset. */
	wlc_bmac_copyfrom_shmx(wlc->hw, MX_UCODEX_MACSTAT, macxstat_readbuf,
		sizeof(uint16) * MACXSTAT_OFFSET_SZ);

	for (i = 0; i < MACXSTAT_OFFSET_SZ; i++) {
		uint16 v, v_delta;

		v = ltoh16(macxstat_readbuf[i]);
		v_delta = (v - macxstat_snapshot[i]);
		if (v_delta != 0) {
			/* Update to wlc->pub->_mcxstat_cnt */
			macxstat_cnt[i] += v_delta;
			macxstat_snapshot[i] = v;
		}
	}
}
#else
#define wlc_macxstatupd(wlc) do {} while (0)
#endif /* WL_PSMX */

void
wlc_statsupd(wlc_info_t *wlc)
{
	int i;
	wlc_pub_t *pub = wlc->pub;
	wl_cnt_wlc_t *cnt = pub->_cnt;
#ifdef BCMDBG
	uint16 delta = 0;
	uint16 delta1 = 0;
	uint16 rxf0ovfl;
	uint16 rxhlovfl = 0;
	uint16 txphyerr;
	uint16 txfunfl[NFIFO_LEGACY];
#endif /* BCMDBG */
#ifdef WLCNT
	uint16 macstat_readbuf[MACSTAT_OFFSET_SZ];
	uint16 *macstat_snapshot = wlc->core->macstat_snapshot;
	uint32 *macstat_cnt = pub->_mcst_cnt;
#endif /* WLCNT */
#if defined(RTS_PER_ITF) && defined(WLMCHAN)
	wlc_bsscfg_t *cfg = NULL;
	uint32 old_txrts = cnt->txrts;
	uint32 old_txnocts = cnt->txnocts;
#endif /* RTS_PER_ITF &&  WLMCHAN */

	/* if driver or core are down, makes no sense to update stats */
	if (!pub->up || !wlc->clk) {
		return;
	}

	if (DEVICEREMOVED(wlc)) {
		WL_ERROR(("%s: DEVICE_REMOVED\n", __FUNCTION__));
		return;
	}

#ifdef BCMDBG
	/* save last rx fifo 0 overflow count */
	rxf0ovfl = macstat_snapshot[MCSTOFF_RXF0OVFL];
	if (D11REV_GE(pub->corerev, 40)) { /* RXHLOVFL is not implemented in DCF (<rev40) ucode */
		rxhlovfl = macstat_snapshot[MCSTOFF_RXHLOVFL];
	}

	/* save last tx fifo underflow count */
	for (i = 0; i < NFIFO_LEGACY; i++) {
		txfunfl[i] = macstat_snapshot[MCSTOFF_TXFUNFL + i];
	}
	/* save last tx phy error count */
	txphyerr = macstat_snapshot[MCSTOFF_TXPHYERR];
#endif /* BCMDBG */

#ifdef WLCNT
	/* Read mac stats from contiguous shared memory. M_PSM2HOST_STATS() is a byte offset. */
	wlc_bmac_copyfrom_shm(wlc->hw, M_PSM2HOST_STATS(wlc), macstat_readbuf,
		sizeof(uint16) * MACSTAT_OFFSET_SZ);

	for (i = 0; i < MACSTAT_OFFSET_SZ; i++) {
		uint16 v;
		uint16 v_delta;

		v = ltoh16(macstat_readbuf[i]);
		v_delta = (v - macstat_snapshot[i]);

		if (v_delta != 0) {
			/* Update to pub->_macstat_cnt */
			macstat_cnt[i] += v_delta;
			macstat_snapshot[i] = v;
		}
	}

	wlc_macxstatupd(wlc);
	wlc_macdbg_upd_d11cnts(wlc);
#endif /* WLCNT */

#ifdef BCMDBG
	/* check for rx fifo 0 overflow */
	delta = macstat_snapshot[MCSTOFF_RXF0OVFL] - rxf0ovfl;
	if (D11REV_GE(pub->corerev, 40)) {
		delta1 = macstat_snapshot[MCSTOFF_RXHLOVFL] - rxhlovfl;
	}
	if (delta && delta1) {
		WL_ERROR(("wl%d: rx fifo 0 and hdfifo overflows: %u %u!\n",
			pub->unit, delta, delta1));
	} else if (delta) {
		WL_ERROR(("wl%d: %u rx fifo 0 overflows!\n", pub->unit, delta));
	} else if (delta1) {
		WL_ERROR(("wl%d: %u rx hlfifo overflows!\n", pub->unit, delta1));
	}

	/* check for tx phy errors */
	delta = macstat_snapshot[MCSTOFF_TXPHYERR] - txphyerr;
	if (delta) {
		WL_ERROR(("wl%d: %u tx phy errors!\n", pub->unit, delta));
		WL_PRINT(("wl%d: PHYTX error\n", pub->unit));
		wlc_dump_phytxerr(wlc, 0);
	}

	/* check for tx fifo underflows */
	for (i = 0; i < NFIFO_LEGACY; i++) {
		delta = macstat_snapshot[MCSTOFF_TXFUNFL + i] - txfunfl[i];
		if (delta) {
			WL_ERROR(("wl%d: %u tx fifo %d underflows!\n", pub->unit, delta, i));
		}
	}
#endif /* BCMDBG */

	/* dot11 counter update */
	WLCNTSET(cnt->txrts,
	         (MCSTVAR(wlc->pub, rxctsucast) - cnt->d11cnt_txrts_off));
	WLCNTSET(cnt->rxcrc,
	         (MCSTVAR(wlc->pub, rxbadfcs) - cnt->d11cnt_rxcrc_off));
	WLCNTSET(cnt->txnocts,
	         ((MCSTVAR(wlc->pub, txrtsfrm) - MCSTVAR(wlc->pub, rxctsucast)) -
	          cnt->d11cnt_txnocts_off));

#if defined(RTS_PER_ITF) && defined(WLMCHAN)
	/* Update the RTS/CTS information of the per-interface stats
	 * of the current primary queue.
	 */
	cfg = wlc_mchan_get_cfg_frm_q(wlc, wlc->primary_queue);
	if (cfg && cfg->wlcif) {
		cfg->wlcif->_cnt->txrts += (cnt->txrts - old_txrts);
		cfg->wlcif->_cnt->txnocts += (cnt->txnocts - old_txnocts);
	}
#endif /* RTS_PER_ITF && WLMCHAN */

	if (!PIO_ENAB(wlc->pub)) {
		/* merge counters from dma module */
		for (i = 0; i < WLC_HW_NFIFO_INUSE(wlc); i++) {
			hnddma_t *di = WLC_HW_DI(wlc, i);
			if (di != NULL) {
				WLCNTADD(cnt->txnobuf, di->txnobuf);
				WLCNTADD(cnt->rxnobuf, di->rxnobuf);
				WLCNTADD(cnt->rxgiant, di->rxgiants);
				dma_counterreset(di);
			}
		}
	}

	/*
	 * Aggregate transmit and receive errors that probably resulted
	 * in the loss of a frame are computed on the fly.
	 */
	WLCNTSET(cnt->txerror,  cnt->txnobuf + cnt->txnoassoc +
	       cnt->txuflo + cnt->txrunt + cnt->dmade +
	       cnt->dmada + cnt->dmape);
	WLCNTSET(cnt->rxerror, cnt->rxoflo + cnt->rxnobuf +
	       cnt->rxfragerr + cnt->rxrunt + cnt->rxgiant +
	       cnt->rxnoscb + cnt->rxbadsrcmac);
	for (i = 0; i < NFIFO_LEGACY; i++) {
		WLCNTADD(cnt->rxerror, cnt->rxuflo[i]);
	}
} /* wlc_statsupd */

#ifdef WLCNT
int
wlc_get_all_cnts(wlc_info_t *wlc, void *buf, int buflen)
{
	int i;
	wl_cnt_info_t *cntinfo = buf;
	uint8 *xtlvbuf_p = cntinfo->data;
	uint16 xtlvbuflen = (uint16)buflen;

	xtlv_desc_t xtlv_desc[] = {
		{WL_CNT_XTLV_SLICE_IDX, sizeof(uint8), &wlc->pub->unit},
		{WL_CNT_XTLV_WLC_RINIT_RSN, sizeof(reinit_rsns_t),
		wlc->pub->reinitrsn},
		{WL_CNT_XTLV_WLC, sizeof(wl_cnt_wlc_t), wlc->pub->_cnt},
		{WL_CNT_XTLV_GE40_UCODE_V1, WL_CNT_MCST_STRUCT_SZ,
		wlc->pub->_mcst_cnt},
#if defined(WL_PSMX)
		{0, 0, NULL},
#endif /* WL_PSMX */
		{0, 0, NULL}
	};

	if (D11REV_LT(wlc->pub->corerev, 40)) {
		/* If revid < 40, change the type appropriately. */
		xtlv_desc[3].type = WL_CNT_XTLV_LT40_UCODE_V1;
	}
#if defined(WL_PSMX)
	if (PSMX_ENAB(wlc->pub)) {
		int psmx_idx = 4;
		xtlv_desc[psmx_idx].type = WL_CNT_XTLV_GE64_UCODEX_V1;
		xtlv_desc[psmx_idx].len = WL_CNT_MCXST_STRUCT_SZ;
		xtlv_desc[psmx_idx].ptr = wlc->pub->_mcxst_cnt;
	}
#endif	/* WL_PSMX */

	for (i = 0; i < sizeof(wlc->pub->_cnt->reinitreason)/sizeof(uint32); i++) {
		wlc->pub->_cnt->reinitreason[i] = INVALID_CNT_VAL;
	}

	/* Report as much as we can in the buffer provided. */
	bcm_pack_xtlv_buf_from_mem(&xtlvbuf_p, &xtlvbuflen,
		xtlv_desc, BCM_XTLV_OPTION_ALIGN32);

	cntinfo->version = WL_CNT_T_VERSION;
	cntinfo->datalen = (buflen - xtlvbuflen);

	return BCME_OK;
} /* wlc_get_all_cnts */

#endif /* WLCNT */

#if defined(DELTASTATS)
static void
wlc_delta_stats_update(wlc_info_t *wlc)
{
	wl_delta_stats_t *stats;
#ifdef WLCNT
	wl_cnt_wlc_t *cnt = wlc->pub->_cnt;
#endif // endif

	/* current stats becomes previous stats */
	wlc->delta_stats->current_index =
		(wlc->delta_stats->current_index + 1) % DELTA_STATS_NUM_INTERVAL;

	/* update current stats */
	stats = &wlc->delta_stats->stats[wlc->delta_stats->current_index];

	/* NOTE: wlc_statsupd() must be invoked to update the stats if there are
	 * any stats which depend on macstat_t
	 */

	/* stats not dependent on macstat_t */
	WLCNTSET(stats->txframe,   WLCNTVAL(cnt->txframe));
	WLCNTSET(stats->txbyte,    WLCNTVAL(cnt->txbyte));
	WLCNTSET(stats->txretrans, WLCNTVAL(cnt->txretrans));
	WLCNTSET(stats->txfail,    WLCNTVAL(cnt->txfail));
	WLCNTSET(stats->rxframe,   WLCNTVAL(cnt->rxframe));
	WLCNTSET(stats->rxbyte,    WLCNTVAL(cnt->rxbyte));
	WLCNTSET(stats->rx1mbps,   WLCNTVAL(cnt->rx1mbps));
	WLCNTSET(stats->rx2mbps,   WLCNTVAL(cnt->rx2mbps));
	WLCNTSET(stats->rx5mbps5,  WLCNTVAL(cnt->rx5mbps5));
	WLCNTSET(stats->rx6mbps,   WLCNTVAL(cnt->rx6mbps));
	WLCNTSET(stats->rx9mbps,   WLCNTVAL(cnt->rx9mbps));
	WLCNTSET(stats->rx11mbps,  WLCNTVAL(cnt->rx11mbps));
	WLCNTSET(stats->rx12mbps,  WLCNTVAL(cnt->rx12mbps));
	WLCNTSET(stats->rx18mbps,  WLCNTVAL(cnt->rx18mbps));
	WLCNTSET(stats->rx24mbps,  WLCNTVAL(cnt->rx24mbps));
	WLCNTSET(stats->rx36mbps,  WLCNTVAL(cnt->rx36mbps));
	WLCNTSET(stats->rx48mbps,  WLCNTVAL(cnt->rx48mbps));
	WLCNTSET(stats->rx54mbps,  WLCNTVAL(cnt->rx54mbps));

	WLCNTSET(stats->rxbadplcp, MCSTVAR(wlc->pub, rxbadplcp));
	WLCNTSET(stats->rxcrsglitch, MCSTVAR(wlc->pub, rxcrsglitch));
	WLCNTSET(stats->bphy_rxcrsglitch, MCSTVAR(wlc->pub, bphy_rxcrsglitch));
	WLCNTSET(stats->bphy_badplcp, MCSTVAR(wlc->pub, bphy_badplcp));
}

static int
wlc_get_delta_stats(wlc_info_t *wlc, wl_delta_stats_t *stats)
{
	int err = BCME_OK;

	/* return zeros if disabled or interval has not yet elapsed */
	if (wlc->delta_stats->interval == 0 ||
		wlc->delta_stats->seconds <= wlc->delta_stats->interval) {
		/* zero the entire structure */
		bzero(stats, sizeof(wl_delta_stats_t));
		err = BCME_ERROR;
	} else {
		/* indexes to current and previous stats */
#ifdef WLCNT
		uint32 curr_index = wlc->delta_stats->current_index;
		uint32 prev_index = (curr_index - 1) % DELTA_STATS_NUM_INTERVAL;

		/* pointers to current and previous stats */
		wl_delta_stats_t *curr = &wlc->delta_stats->stats[curr_index];
		wl_delta_stats_t *prev = &wlc->delta_stats->stats[prev_index];
#endif // endif

		/* calculate delta statistics */
		WLCNTSET(stats->txframe,   curr->txframe   - prev->txframe);
		WLCNTSET(stats->txbyte,    curr->txbyte    - prev->txbyte);
		WLCNTSET(stats->txretrans, curr->txretrans - prev->txretrans);
		WLCNTSET(stats->txfail,    curr->txfail    - prev->txfail);
		WLCNTSET(stats->rxframe,   curr->rxframe   - prev->rxframe);
		WLCNTSET(stats->rxbyte,    curr->rxbyte    - prev->rxbyte);
		WLCNTSET(stats->rx1mbps,   curr->rx1mbps   - prev->rx1mbps);
		WLCNTSET(stats->rx2mbps,   curr->rx2mbps   - prev->rx2mbps);
		WLCNTSET(stats->rx5mbps5,  curr->rx5mbps5  - prev->rx5mbps5);
		WLCNTSET(stats->rx6mbps,   curr->rx6mbps   - prev->rx6mbps);
		WLCNTSET(stats->rx9mbps,   curr->rx9mbps   - prev->rx9mbps);
		WLCNTSET(stats->rx11mbps,  curr->rx11mbps  - prev->rx11mbps);
		WLCNTSET(stats->rx12mbps,  curr->rx12mbps  - prev->rx12mbps);
		WLCNTSET(stats->rx18mbps,  curr->rx18mbps  - prev->rx18mbps);
		WLCNTSET(stats->rx24mbps,  curr->rx24mbps  - prev->rx24mbps);
		WLCNTSET(stats->rx36mbps,  curr->rx36mbps  - prev->rx36mbps);
		WLCNTSET(stats->rx48mbps,  curr->rx48mbps  - prev->rx48mbps);
		WLCNTSET(stats->rx54mbps,  curr->rx54mbps  - prev->rx54mbps);

		WLCNTSET(stats->rxbadplcp,	curr->rxbadplcp  - prev->rxbadplcp);
		WLCNTSET(stats->rxcrsglitch,	curr->rxcrsglitch  - prev->rxcrsglitch);
		WLCNTSET(stats->bphy_rxcrsglitch, curr->bphy_rxcrsglitch  - prev->bphy_rxcrsglitch);
		WLCNTSET(stats->bphy_badplcp,	curr->bphy_badplcp  - prev->bphy_badplcp);
	}

	/* Set the slice index for which the stats are reported */
	WLCNTSET(stats->slice_index, wlc->pub->unit);

	/* set version and length of structure */
	WLCNTSET(stats->version, WL_DELTA_STATS_T_VERSION);
	WLCNTSET(stats->length, sizeof(wl_delta_stats_t));

	return err;
} /* wlc_get_delta_stats */
#endif /* DELTASTATS */

uint
wlc_ctrupd(wlc_info_t *wlc, uint macstat_offset)
{
#ifdef WLCNT
	uint16 v, delta;
	uint32 *macstat_cnt = wlc->pub->_mcst_cnt;
	uint16 *macstat_core_cnt = wlc->core->macstat_snapshot;

	v = wlc_bmac_read_shm(wlc->hw, MACSTAT_ADDR(wlc, macstat_offset));
	delta = (v - macstat_core_cnt[macstat_offset]);

	if (delta != 0) {
		macstat_cnt[macstat_offset] += delta;
		macstat_core_cnt[macstat_offset] = v;
	}
	return (delta);
#else
	return (0);
#endif /* WLCNT */
}

static const uint16 BCMATTACHDATA(wlc_devid)[] = {
	BCM6362_D11N_ID, BCM6362_D11N2G_ID, BCM6362_D11N5G_ID,
	BCM43217_D11N2G_ID, BCM43227_D11N2G_ID,
	BCM4360_D11AC_ID, BCM4360_D11AC2G_ID, BCM4360_D11AC5G_ID,
	BCM43602_D11AC_ID, BCM43602_D11AC2G_ID, BCM43602_D11AC5G_ID,
	BCM4352_D11AC_ID, BCM4352_D11AC2G_ID, BCM4352_D11AC5G_ID,
	BCM4365_D11AC_ID, BCM4365_D11AC2G_ID, BCM4365_D11AC5G_ID,
	BCM4366_D11AC_ID, BCM4366_D11AC2G_ID, BCM4366_D11AC5G_ID,
	BCM53573_D11AC_ID, BCM53573_D11AC2G_ID, BCM53573_D11AC5G_ID,
	BCM47189_D11AC_ID, BCM47189_D11AC2G_ID, BCM47189_D11AC5G_ID,
	BCM43684_D11AX_ID, BCM43684_D11AX2G_ID, BCM43684_D11AX5G_ID,
	EMBEDDED_2x2AX_ID, EMBEDDED_2x2AX_DEV2G_ID, EMBEDDED_2x2AX_DEV5G_ID,
	BCM4347_D11AC_ID, BCM4347_D11AC2G_ID, BCM4347_D11AC5G_ID,
	BCM6710_D11AX_ID, BCM6710_D11AX2G_ID, BCM6710_D11AX5G_ID,
	BCM6878_D11AC_ID, BCM6878_D11AC2G_ID, BCM6878_D11AC5G_ID,
	BCM43569_D11AC_ID, BCM43569_D11AC2G_ID, BCM43569_D11AC5G_ID,
	/* Chips with blank-OTP */
	BCM4347_CHIP_ID,
	BCM43217_CHIP_ID, BCM43227_CHIP_ID,
	BCM43602_CHIP_ID, BCM43462_CHIP_ID, BCM43522_CHIP_ID,
	BCM43684_CHIP_ID,
	BCM6710_CHIP_ID,
	BCM43569_CHIP_ID, BCM43570_CHIP_ID,
	BCM4352_CHIP_ID, BCM4360_CHIP_ID,
};

bool
BCMATTACHFN(wlc_chipmatch)(uint16 vendor, uint16 device)
{
	uint i;
	if (vendor != VENDOR_BROADCOM) {
		WL_TRACE(("%s: unsupported vendor %04x device %04x\n", __FUNCTION__,
			vendor, device));
		return (FALSE);
	}

	for (i = 0; i < sizeof(wlc_devid)/sizeof(wlc_devid[0]); i++) {
		if (device == wlc_devid[i])
			return TRUE;
	}

	WL_ERROR(("%s: unsupported vendor %04x device %04x\n", __FUNCTION__, vendor, device));
	return (FALSE);
}

uint16
wlc_rate_shm_offset(wlc_info_t *wlc, uint8 rate)
{
	return (wlc_bmac_rate_shm_offset(wlc->hw, rate));
}

/** Callback for device removed */

/**
 * Get the current timestamp for packet lifetime feature.
 *
 * return Timestamp in us.
 */
inline uint32
wlc_lifetime_now(wlc_info_t *wlc)
{
#if defined(DONGLEBUILD)
	/* For dongle, R_REG(tsf) has less latency than OSL_SYSUPTIME() */
	return R_REG(wlc->osh, D11_TSFTimerLow(wlc));
#else
	/* now(us) = TSF cache(us) + elapsed system time(ms * 1000) since TSF cache */
	return wlc->lifetime_tsftimer_cache +
		(OSL_SYSUPTIME() - wlc->lifetime_osltime_cache) * 1000;
#endif /* DONGLEBUILD */
}

/**
 * If a lifetime is configured, calculate the expiry time for the packet and update the pkttag.
 */
void
wlc_lifetime_set(wlc_info_t *wlc, void *sdu, uint32 lifetime)
{
	WLPKTTAG(sdu)->flags |= WLF_EXPTIME;
	WLPKTTAG(sdu)->u.exptime = wlc_lifetime_now(wlc) + lifetime;
}

/**
* Cache TSF timer for wlc_lifetime_set() to avoid per packet R_REG(tsf) latency
*/
static void
wlc_lifetime_cache_upd(wlc_info_t *wlc)
{
#if !defined(DONGLEBUILD)
	if (wlc->pub->up) {
		wlc->lifetime_tsftimer_cache = R_REG(wlc->osh, D11_TSFTimerLow(wlc));
		wlc->lifetime_osltime_cache = OSL_SYSUPTIME();
	}
#endif /* !DONGLEBUILD */
}

/**
 * Attempts to queue a packet onto a multiple-precedence queue, if necessary evicting a lower
 * precedence packet from the queue.
 *
 * @param prec Precedence number that has already been mapped from the packet priority
 * @param q    Multi-priority packet queue
 *
 * Returns TRUE if packet consumed (queued), FALSE if not.
 */
bool BCMFASTPATH
wlc_prec_enq(wlc_info_t *wlc, struct pktq *q, void *pkt, int prec)
{
	return wlc_prec_enq_head(wlc, q, pkt, prec, FALSE);
}

/** @param q   Multi-priority packet queue */
bool BCMFASTPATH
wlc_prec_enq_head(wlc_info_t *wlc, struct pktq *q, void *pkt, int prec, bool head)
{
	void *p;
	void *dqp = NULL;
	int eprec = -1;		/* precedence to evict from */
	struct scb *scb;
#ifdef PKTQ_LOG
	pktq_counters_t* prec_cnt = 0;

	if (q->pktqlog) {
		prec_cnt = q->pktqlog->_prec_cnt[prec];

		/* check for auto enabled logging */
		if (prec_cnt == NULL && (q->pktqlog->_prec_log & PKTQ_LOG_AUTO)) {
			prec_cnt = wlc_txq_prec_log_enable(wlc, q, (uint32)prec);
		}
	}
	WLCNTCONDINCR(prec_cnt, prec_cnt->requested);
#endif // endif

	/* Determine precedence from which to evict packet, if any */
	if (pktqprec_full(q, prec)) {
		eprec = prec;
	}
	else if (pktq_full(q)) {
		p = pktq_peek_tail(q, &eprec);
		if (p == NULL)
			WL_ERROR(("%s: null ptr", __FUNCTION__));
		ASSERT(p != NULL);
		BCM_REFERENCE(p);
		if (eprec > prec) {
#ifdef PKTQ_LOG
			WLCNTCONDINCR(prec_cnt, prec_cnt->full_dropped);
#endif // endif
			wlc_tx_status_update_counters(wlc, pkt, NULL, NULL,
				WLC_TX_STS_TX_Q_FULL, 1);
			return FALSE;
		}
	}

	/* Evict if needed */
	if (eprec >= 0) {
		bool discard_oldest;

		/* Detect queueing to unconfigured precedence */
		ASSERT(!pktqprec_empty(q, eprec));

		discard_oldest = AC_BITMAP_TST(wlc->wme_dp_precmap, eprec);

		/* Refuse newer packet unless configured to discard oldest */
		if (eprec == prec && !discard_oldest) {
#ifdef PKTQ_LOG
			WLCNTCONDINCR(prec_cnt, prec_cnt->dropped);
#endif // endif
			wlc_tx_status_update_counters(wlc, pkt, NULL, NULL,
				WLC_TX_STS_TX_Q_FULL, 1);
			return FALSE;
		}
		/* Evict packet according to discard policy */
		dqp = discard_oldest ? pktq_pdeq(q, eprec) : pktq_pdeq_tail(q, eprec);
		ASSERT(dqp != NULL);

		/* Increment wme stats */
		if (WME_ENAB(wlc->pub)) {
			WLCNTINCR(wlc->pub->_wme_cnt->tx_failed[WME_PRIO2AC(PKTPRIO(dqp))].packets);
			WLCNTADD(wlc->pub->_wme_cnt->tx_failed[WME_PRIO2AC(PKTPRIO(dqp))].bytes,
			         pkttotlen(wlc->osh, dqp));
		}

		/* Update tx stall counters */
		wlc_tx_status_update_counters(wlc, dqp, NULL, NULL, WLC_TX_STS_TX_Q_FULL, 1);

#if defined(WLCNT) || defined(WLCNTSCB)
		scb = WLPKTTAGSCBGET(dqp);
#endif /* WLCNT || WLCNTSCB */
#ifdef PKTQ_LOG
		if (eprec == prec) {
			WLCNTCONDINCR(prec_cnt, prec_cnt->selfsaved);
		} else {
			WLCNTCONDINCR(prec_cnt, prec_cnt->saved);
			WLCNTCONDINCR(prec_cnt, prec_cnt->sacrificed);
		}
#endif /* PKTQ_LOG */
		WLCNTINCR(wlc->pub->_cnt->txnobuf);
		WLCIFCNTINCR(scb, txnobuf);
		WLCNTSCB_COND_INCR(scb, scb->scb_stats.tx_failures);
#ifdef WL11K
		wlc_rrm_stat_qos_counter(wlc, scb, PKTPRIO(dqp),
			OFFSETOF(rrm_stat_group_qos_t, txfail));
#endif // endif
	}

	/* Enqueue */
#ifdef PKTQ_LOG
	WLCNTCONDINCR(prec_cnt, prec_cnt->stored);

	if (prec_cnt)
	{
		uint32 qlen = pktqprec_avail_pkts(q, prec);
		uint32* max_avail = &prec_cnt->max_avail;
		uint32* max_used = &prec_cnt->max_used;

		if (qlen < *max_avail) {
			*max_avail = qlen;
		}
		qlen = pktqprec_n_pkts(q, prec);
		if (qlen > *max_used) {
			*max_used = qlen;
		}
	}
#endif /* PKTQ_LOG */

	if (head)
		p = pktq_penq_head(q, prec, pkt);
	else
		p = pktq_penq(q, prec, pkt);
	if (p == NULL)
		WL_ERROR(("%s: null ptr2", __FUNCTION__));
	ASSERT(p != NULL);

	PKTDBG_TRACE(wlc->osh, p, PKTLIST_PRECQ);

	BCM_REFERENCE(scb);

	/* Free dequeued packet */
	if (dqp != NULL) {
		/* Adjust per-scb accounting for evicted packet */
		cpktq_dec(q, dqp, 1, eprec);
		PKTFREE(wlc->osh, dqp, TRUE);
	}

	return TRUE;
} /* wlc_prec_enq_head */

int
wlc_prio2prec(wlc_info_t *wlc, int prio)
{
	ASSERT(prio >= 0 && (uint)prio < ARRAYSIZE(wlc_prio2prec_map));
	if (WME_ENAB(wlc->pub)) {
		return wlc_prio2prec_map[prio];
	} else {
		return _WLC_PREC_BE; /* default to BE */
	}
}

/** Sends an 80211 packet with radio parameters specified */
bool
wlc_send80211_specified(wlc_info_t *wlc, void *sdu, ratespec_t rspec, struct wlc_if *wlcif)
{
	bool short_preamble = FALSE;
	wlc_bsscfg_t *bsscfg;
	struct scb *scb;
	chanspec_t chanspec;

	/* figure out the bsscfg for this packet */
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	if (wlcif == NULL)
		wlcif = bsscfg->wlcif;
	ASSERT(wlcif != NULL);

	ASSERT(wlcif->qi != NULL);

	chanspec = wlc_get_home_chanspec(bsscfg);

	/* grab the hwrs_scb and validate the rate override
	 * based on the home channel band for this wlcif
	 */
	if (CHSPEC_IS2G(chanspec)) {
		scb = wlc->bandstate[BAND_2G_INDEX]->hwrs_scb;
	} else {
		scb = wlc->bandstate[BAND_5G_INDEX]->hwrs_scb;
		/* make sure the rate override does not specify a DSSS/CCK rate */
		if (RSPEC_ISCCK(rspec)) {
			rspec = 0; /* clear the invalid rate override */
			WL_ERROR(("wl%d: %s: rate override with DSSS rate "
			          "incompatible with 5GHz band\n",
			          wlc->pub->unit, __FUNCTION__));
		}
	}

	if (rspec & WL_RSPEC_OVERRIDE_MODE) {
		/* validate the rspec for the correct BW */
		if (CHSPEC_IS20(wlc->chanspec) && !RSPEC_IS20MHZ(rspec)) {
			rspec = 0; /* clear the invalid rate override */
			WL_ERROR(("wl%d: %s: rate override with bw > 20MHz "
			          "but only 20MHz channel\n",
			          wlc->pub->unit, __FUNCTION__));
		} else if (CHSPEC_IS40(wlc->chanspec) &&
		           !(RSPEC_IS20MHZ(rspec) || RSPEC_IS40MHZ(rspec))) {
			rspec = 0; /* clear the invalid rate override */
			WL_ERROR(("wl%d: %s: rate override with bw > 40MHz "
			          "but only 40MHz channel\n",
			          wlc->pub->unit, __FUNCTION__));
		}

		/* if the rspec is for 2,5.5,11 rate, and specifies a mode override,
		 * check for short preamble
		 */
		if (RSPEC_ISLEGACY(rspec) &&
		    ((rspec & RATE_MASK) == WLC_RATE_2M ||
		     (rspec & RATE_MASK) == WLC_RATE_5M5 ||
		     (rspec & RATE_MASK) == WLC_RATE_11M))
			short_preamble = ((rspec & WL_RSPEC_SHORT_PREAMBLE) != 0);
	}

	return wlc_queue_80211_frag(wlc, sdu, wlc->active_queue, scb,
		(scb ? scb->bsscfg : NULL), short_preamble, NULL, rspec);
} /* wlc_send80211_specified */

/* Move flags from one packet to another */
#define WLPKTTAG_FLAGS_COPY_COMMON(pkt_from, pkt_to) \
	do { \
		WLPKTTAG(pkt_to)->flags = WLPKTTAG(pkt_from)->flags & \
			(WLF_APSD | WLF_PSDONTQ | WLF_AMSDU | WLF_AMPDU_MPDU | \
			 WLF_BSS_DOWN | \
			 WLF_USERTS | WLF_RATE_AUTO | WLF_8021X | WLF_EXPTIME | \
		         WLF_DATA | WLF_TXHDR | WLF_PROPTX_PROCESSED | \
			 WLF_RX_KM | WLF_MESH_RETX); \
		WLPKTTAG(pkt_to)->flags2 = WLPKTTAG(pkt_from)->flags2; \
		WLPKTTAG(pkt_to)->flags3 = WLPKTTAG(pkt_from)->flags3 & \
			(WLF3_NO_PMCHANGE | WLF3_DATA_WOWL_PKT | WLF3_FAVORED | \
			 WLF3_BYPASS_AMPDU); \
	} while (0)

#define WLPKTTAG_FLAGS_COPY(pkt_from, pkt_to) \
	do { \
		WLPKTTAG_FLAGS_COPY_COMMON(pkt_from, pkt_to); \
	} while (0)

#define WLPKTTAG_FLAGS_MOVE(pkt_from, pkt_to) \
	do { \
		WLPKTTAG_FLAGS_COPY(pkt_from, pkt_to); \
	} while (0)

/**
 * Moves callback functions from a packet to another.
 * CAUTION: This is destructive operation for pkt_from
 */
void
wlc_pkttag_info_move(wlc_info_t *wlc, void *pkt_from, void *pkt_to)
{
	/* Make sure not moving to same packet! */
	ASSERT(pkt_from != pkt_to);
	WLPKTTAG(pkt_to)->callbackidx = WLPKTTAG(pkt_from)->callbackidx;
	WLPKTTAG(pkt_from)->callbackidx = 0;
	WLPKTTAG(pkt_to)->_bsscfgidx = WLPKTTAG(pkt_from)->_bsscfgidx;
	WLPKTTAG(pkt_to)->_scb = WLPKTTAG(pkt_from)->_scb;
	WLPKTTAG(pkt_to)->rspec = WLPKTTAG(pkt_from)->rspec;
	WLPKTTAG(pkt_to)->seq = WLPKTTAG(pkt_from)->seq;
	WLPKTTAG(pkt_to)->u.exptime = WLPKTTAG(pkt_from)->u.exptime;
	WLFC_PKTAG_INFO_MOVE(pkt_from, pkt_to);
	WLPKTTAG_FLAGS_MOVE(pkt_from, pkt_to);
	WLPKTTAG(pkt_to)->shared.packetid = WLPKTTAG(pkt_from)->shared.packetid;
	wlc_pcb_cb_move(wlc->pcb, pkt_from, pkt_to);
#ifdef WLTAF
	WLPKTTAG(pkt_to)->pktinfo.taf = WLPKTTAG(pkt_from)->pktinfo.taf;
#endif // endif
#ifdef WLATF
	WLPKTTAG(pkt_to)->pktinfo.atf = WLPKTTAG(pkt_from)->pktinfo.atf;
#endif /* WLATF */
}

/**
 * queue packet suppressed due to scheduled absence periods to the end of the BSS's suppression
 * queue or the STA's PS queue.
 */
bool
wlc_pkt_abs_supr_enq(wlc_info_t *wlc, struct scb *scb, void *pkt)
{
	bool free_pkt = FALSE;
	wlc_bsscfg_t *cfg;
	wlc_pkttag_t *pkttag;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif // endif

	ASSERT(scb != NULL);
	ASSERT(pkt != NULL);

	cfg = SCB_BSSCFG(scb);
	ASSERT(cfg != NULL);

	if (!cfg->enable) {
		WL_ERROR(("%s: called with cfg %p disabled\n",
			__FUNCTION__, OSL_OBFUSCATE_BUF(cfg)));
		return TRUE; /* free packets */
	}

	ASSERT(BSS_TX_SUPR(cfg));

#ifdef WLP2P
	/* Basically do not put packets in abs_q (PSQ) if NOA or OpPS */
	/* is already cancelled */
	if (BSS_P2P_ENAB(wlc, SCB_BSSCFG(scb)) &&
	    !wlc_p2p_noa_valid(wlc->p2p, cfg) &&
	    !wlc_p2p_ops_valid(wlc->p2p, cfg)) {
		WL_INFORM(("wl%d: free supr'd frame outside absence period "
			"due to rx processing lag???\n", wlc->pub->unit));
		return TRUE;
	}
#endif /* WLP2P */

	pkttag = WLPKTTAG(pkt);

	if (pkttag->flags3 & WLF3_TXQ_SHORT_LIFETIME) {
		WL_INFORM(("%s: drop short-lived pkt %p...\n",
			__FUNCTION__, OSL_OBFUSCATE_BUF(pkt)));
		free_pkt = TRUE;
	} else if (BSSCFG_AP(cfg) &&
	         SCB_PS(scb) && !SCB_ISMULTI(scb) &&
	         SCB_ASSOCIATED(scb) && SCB_P2P(scb)) {
		if (!(pkttag->flags & WLF_PSDONTQ)) {
			free_pkt = wlc_apps_scb_supr_enq(wlc, scb, pkt);
			WL_INFORM(("wl%d.%d: %s: requeue packet %p for %s %s\n",
			        wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
				OSL_OBFUSCATE_BUF(pkt),
			        bcm_ether_ntoa(&scb->ea, eabuf),
			        free_pkt?"unsuccessfully":"successfully"));
		} else {
			/* toss driver generated NULL frame */
			WL_INFORM(("wl%d.%d: %s: drop NULL packet %p for %s\n",
			        wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
				OSL_OBFUSCATE_BUF(pkt),
			        bcm_ether_ntoa(&scb->ea, eabuf)));
			free_pkt = TRUE;
		}
	} else if (BSSCFG_STA(cfg)) {
		struct dot11_management_header *h =
			(struct dot11_management_header *)((uint8 *)PKTDATA(wlc->osh, pkt) +
				((pkttag->flags & WLF_TXHDR) != 0 ? D11_TXH_LEN_EX(wlc) : 0));
		uint16 fc = ltoh16(h->fc);
		bool psp = (fc & FC_KIND_MASK) == FC_PS_POLL;

		if (!psp) {
			free_pkt = wlc_bsscfg_tx_supr_enq(wlc->psqi, cfg, pkt);
			WL_INFORM(("wl%d.%d: %s: requeue packet %p %s\n",
			        wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
				OSL_OBFUSCATE_BUF(pkt), free_pkt?"unsuccessfully":"successfully"));
		} else {
			/* toss driver generated PSPoll frame */
			WL_INFORM(("wl%d.%d: %s: drop PSPoll packet %p for %s\n",
			        wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
				OSL_OBFUSCATE_BUF(pkt),
			        bcm_ether_ntoa(&scb->ea, eabuf)));
			free_pkt = TRUE;
		}
	} else {
		free_pkt = wlc_bsscfg_tx_supr_enq(wlc->psqi, cfg, pkt);
		WL_INFORM(("wl%d.%d: %s: requeue packet %p %s\n",
		        wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
			OSL_OBFUSCATE_BUF(pkt),
		        free_pkt?"unsuccessfully":"successfully"));
	}

	return free_pkt;
} /* wlc_pkt_abs_supr_enq */

/** get original etype for converted ether frame or 8023 frame */
uint16
wlc_sdu_etype(wlc_info_t *wlc, void *sdu)
{
	uint16 etype;
	struct ether_header *eh;

	eh = (struct ether_header*) PKTDATA(wlc->osh, sdu);
	if (WLPKTTAG(sdu)->flags & WLF_NON8023) {
		/* ether and llc/snap are in one continuous buffer */
		etype = *(uint16 *)((uint8*)eh + ETHER_HDR_LEN + DOT11_LLC_SNAP_HDR_LEN - 2);
		ASSERT(ntoh16(etype) > ETHER_MAX_DATA);
	} else {
		etype = eh->ether_type;
	}

	return etype;
}

/** get data/body pointer from 8023 frame(original or converted) */
uint8 *
wlc_sdu_data(wlc_info_t *wlc, void *sdu)
{
	uint body_offset;
	uint hdrlen;
	osl_t *osh = wlc->osh;

	BCM_REFERENCE(osh);

	body_offset = ETHER_HDR_LEN + DOT11_LLC_SNAP_HDR_LEN;

	/* handle chained hdr buffer and data buffer */
	hdrlen = PKTLEN(osh, sdu);
	if (body_offset >= hdrlen)
		return ((uint8*)PKTDATA(osh, PKTNEXT(osh, sdu)) + (body_offset - hdrlen));
	else
		return ((uint8*)PKTDATA(osh, sdu) + body_offset);
}

/** Convert Ethernet to 802.3 per 802.1H (use bridge-tunnel if type in SST) */
void BCMFASTPATH
wlc_ether_8023hdr(wlc_info_t *wlc, osl_t *osh, struct ether_header *eh, void *p)
{
	struct ether_header *neh;
	struct dot11_llc_snap_header *lsh;
	uint16 plen, ether_type;
	BCM_REFERENCE(wlc);

	ether_type = ntoh16(eh->ether_type);
	neh = (struct ether_header *)PKTPUSH(osh, p, DOT11_LLC_SNAP_HDR_LEN);

	/* 802.3 MAC header */
	eacopy((char*)eh->ether_dhost, (char*)neh->ether_dhost);
	eacopy((char*)eh->ether_shost, (char*)neh->ether_shost);
	plen = (uint16)pkttotlen(osh, p) - ETHER_HDR_LEN;
	neh->ether_type = hton16(plen);

	/* 802.2 LLC header */
	lsh = (struct dot11_llc_snap_header *)&neh[1];
	lsh->dsap = 0xaa;
	lsh->ssap = 0xaa;
	lsh->ctl = 0x03;

	/* 802.2 SNAP header Use RFC1042 or bridge-tunnel if type in SST per 802.1H */
	lsh->oui[0] = 0x00;
	lsh->oui[1] = 0x00;
	if (SSTLOOKUP(ether_type))
		lsh->oui[2] = 0xf8;
	else
		lsh->oui[2] = 0x00;
	lsh->type = hton16(ether_type);
}

static uint16
wlc_compute_airtime(wlc_info_t *wlc, ratespec_t rspec, uint length)
{
	uint16 usec = 0;
	uint mac_rate;
	uint nsyms;

	BCM_REFERENCE(wlc);

	if (RSPEC_ISCCK(rspec)) {
		mac_rate = RSPEC2RATE(rspec);
		switch (mac_rate) {
		case WLC_RATE_1M:
			usec = length << 3;
			break;
		case WLC_RATE_2M:
			usec = length << 2;
			break;
		case WLC_RATE_5M5:
			usec = (length << 4)/11;
			break;
		case WLC_RATE_11M:
			usec = (length << 3)/11;
			break;
		default:
			WL_ERROR(("wl%d: %s: unsupported rspec 0x%x\n",
			          wlc->pub->unit, __FUNCTION__, rspec));
			ASSERT(!"Bad phy_rate");
			break;
		}
	} else if (RSPEC_ISOFDM(rspec)) {
		mac_rate = RSPEC2RATE(rspec);
		/* nsyms = Ceiling(Nbits / (Nbits/sym))
		 *
		 * Nbits = length * 8
		 * Nbits/sym = Mbps * 4 = mac_rate * 2
		 */
		nsyms = CEIL((length * 8), (mac_rate * 2));

		/* usec = symbols * usec/symbol */
		usec = (uint16) (nsyms * APHY_SYMBOL_TIME);
		return (usec);
	} else if (RSPEC_ISHT(rspec) || RSPEC_ISVHT(rspec) || RSPEC_ISHE(rspec)) {
		/* not supported yet */
		ASSERT(0);
	}

	return (usec);
} /* wlc_compute_airtime */

typedef void (*wlc_compute_plcp_t) (wlc_info_t *wlc, wlc_bsscfg_t *cfg, ratespec_t rspec,
	uint length, uint16 fc, uint8 *plcp);

/* d11hdrs pclp computation functions */
static wlc_compute_plcp_t wlc_compute_plcp_fn[] = {
	wlc_compute_cck_plcp,		/* RSPEC_ISCCK */
	wlc_compute_ofdm_plcp,		/* RSPEC_ISOFDM */
	wlc_compute_mimo_plcp,		/* RSPEC_ISHT */
	wlc_compute_vht_plcp,		/* RSPEC_ISVHT */
#ifdef WL11AX
	wlc_compute_he_plcp		/* RSPEC_ISHE */
#endif /* WL11AX */
};

void
BCMRAMFN(wlc_compute_plcp)(wlc_info_t *wlc, wlc_bsscfg_t *cfg, ratespec_t rspec, uint length,
	uint16 fc, uint8 *plcp)
{
	uint ft = WLC_RSPEC_ENC2FT(rspec);

	ASSERT(ft < ARRAYSIZE(wlc_compute_plcp_fn));

	WL_TRACE(("wl%d: wlc_compute_plcp: rate 0x%x, length %d\n", wlc->pub->unit, rspec, length));

	wlc_compute_plcp_fn[ft](wlc, cfg, rspec, length, fc, plcp);

	return;
}

#ifdef WL11AX
/**
 * Fn to compute HE PLCP (Rate: 802.11 rate code, length: PSDU length in octets)
 *
 * Based on TXctrl_PLCP v3.10 :
 * (https://docs.google.com/spreadsheets/d/1eP6ZCRrtnF924ds1R-XmbcH0IdQ0WNJpS1-FHmWeb9g/edit)
 *
 * TBD : Currently only handling SU cases, others like trigger based PPDU & MU to be addressed
 */
static void
wlc_compute_he_plcp(wlc_info_t *wlc, wlc_bsscfg_t *cfg, ratespec_t rspec, uint length,
	uint16 fc, uint8 *plcp)
{
	/* compute HE plcp - 6 B */
	uint32 plcp0 = 0;
	uint16 plcp1 = 0;

	BCM_REFERENCE(length);

	/**
	 * HE Sig A 6 Bytes
	 *
	 * [b0] : Format
	 *
	 * Set 1 for all HE packets (i.e mark as SU),
	 * Ucode shall overwrite this field as required.
	 */
	plcp0 |= HE_SIGA_FORMAT_HE_SU;

	/**
	 * HE Sig A 6 Bytes
	 *
	 * [b1] : Beam Change = 1
	 *
	 */
	plcp0 |= HE_SIGA_BEAM_CHANGE_PLCP0;

	/**
	 * [b2] : UL_DL
	 * - UL = 0, DL = 1
	 *
	 * HE INFRA STA shall always set this field to 1 (for UL traffic), 0 otherwise
	 *
	 * Note : Expected value & handling for TDLS cases has not yet been finalized.
	 */
	plcp0 |= (BSSCFG_INFRA_STA(cfg) ? HE_SIGA_UL_DL_PLCP0 : 0);
#ifdef WLTDLS
	if (BSS_TDLS_ENAB(wlc, cfg)) {
		/* TBD : Revisit when TDLS handling is finalized */
	}
#endif /* WLTDLS */

	/* [b3-b6] : MCS (For SU: MCS index) */
	plcp0 |= ((rspec & WL_RSPEC_HE_MCS_MASK) << HESU_SIGA_MCS_SHIFT);

	/* [b7] : DCM */
	plcp0 |= ((rspec & WL_RSPEC_DCM) ? (1 << HESU_SIGA_DCM_SHIFT) : 0);

	/* [b8-b13] : bsscolor */
	plcp0 &= ~HE_SIGA_BSS_COLOR_MASK_(HE_FORMAT_SU);
	plcp0 |= wlc_get_hebsscolor(wlc, cfg) << HE_SIGA_BSS_COLOR_SHIFT_(HE_FORMAT_SU);

	/**
	 * [b19 - b20] indicate bandwidth :
	 * Set to 0 for 20 MHz, 1 for 40 MHz,
	 * 2 for 80 MHz, 3 for 160 MHz and 80+80 MHz
	 */
	switch (RSPEC_BW(rspec)) {
		case WL_RSPEC_BW_20MHZ:
			plcp0 |= HE_SIGA_20MHZ_VAL;
			break;
		case WL_RSPEC_BW_40MHZ:
			plcp0 |= HE_SIGA_40MHZ_VAL;
			break;
		case WL_RSPEC_BW_80MHZ:
			plcp0 |= HE_SIGA_80MHZ_VAL;
			break;
		case WL_RSPEC_BW_160MHZ:
			/* 160 / 80+80 Mhz */
			plcp0 |= HE_SIGA_160MHZ_VAL;
			break;
		default:
			ASSERT(0);
			break;
	}

	/* [b21-b22] CP_LTF_SIZE */
	switch (RSPEC_HE_LTF_GI(rspec)) {
		case WL_RSPEC_HE_1x_LTF_GI_0_8us:
		/* RSPEC_HE_1x_LTF_GI_0_8us currently not supported on 4369a0
		 * Thereby defaulting to RSPEC_HE_2x_LTF_GI_0_8us in its place.
		 */
		/* Fall through */
		case WL_RSPEC_HE_2x_LTF_GI_0_8us:
			plcp0 |= HE_SIGA_2x_LTF_GI_0_8us_VAL;
			break;
		case WL_RSPEC_HE_2x_LTF_GI_1_6us:
			plcp0 |= HE_SIGA_2x_LTF_GI_1_6us_VAL;
			break;
		case WL_RSPEC_HE_4x_LTF_GI_3_2us:
			plcp0 |= HE_SIGA_4x_LTF_GI_3_2us_VAL;
			break;
	}

	/* [b23 -b25] NSTS (num STS -1) */
	plcp0 |= (((wlc_ratespec_nsts(rspec) - 1) << HE_SIGA_NSTS_SHIFT)
		& HE_SIGA_NSTS_MASK);

	/**
	 * [b26-b32] TxOP
	 * Setting all to 1's for now.
	 */
	plcp0 |= HE_SIGA_TXOP_PLCP0;
	plcp1 |= HE_SIGA_TXOP_PLCP1;

	/**
	 * [b33] Coding
	 * For SU, b33 is set to 0 for BCC, 1 for LDPC
	 */
	if (RSPEC_ISLDPC(rspec))
		plcp1 |= HE_SIGA_CODING_LDPC;

	/**
	 * [b35] :  STBC
	 */
	if (RSPEC_ISSTBC(rspec))
		plcp1 |= HE_SIGA_STBC;

	/* [b36] : TXBF */
	if (RSPEC_ISTXBF(rspec) && RSPEC_ISHE(rspec))
		plcp0 |= HE_SIGA_BEAMFORM_ENABLE;

	/**
	 * [b14, b40] : Reserved fields
	 *
	 * Fill reserved fields as 1
	 */
	plcp0 |= HE_SIGA_RESERVED_PLCP0;
	plcp1 |= HE_SIGA_RESERVED_PLCP1;

	/* Fill in computed plcp */
	plcp[0] = (uint8)(plcp0);
	plcp[1] = (uint8)(plcp0 >> 8);
	plcp[2] = (uint8)(plcp0 >> 16);
	plcp[3] = (uint8)(plcp0 >> 24);
	plcp[4] = (uint8)(plcp1);
	plcp[5] = (uint8)(plcp1 >> 8);
} /* wlc_compute_he_plcp */
#endif /* WL11AX */

static uint32
wlc_vht_siga1_get_partial_aid(void)
{
	/* 9.17a Partial AID in VHT PPDUs
	The TXVECTOR parameter PARTIAL_AID(#1509) is set as follows:
	In a VHT PPDU that carries group addressed MPDUs, the TXVECTOR parameter PARTIAL_AID(#1509)
	is set to 0. In a VHT PPDU sent by an AP(#1512) that carries MPDUs addressed to a single
	non-AP STA, the TXVECTOR parameter PARTIAL_AID(#1509) is set to:
	PARTIAL_AID[0:8] = (AID(0:8) + ((BSSID[0:3] . BSSID[4:7]) << 5)) mod 29 (9-1)
	Where A[b:c] indicates the bits in positions from b to c of the binary representation of A;
	. is a bitwise exclusive OR operation; << 5 indicates a 5 positions bit shift operation
	towards MSB; mod X indicates the Xmodulo operation; AID is the AID of the recipient STA.
	BSSID is the BSSID the STA is associated with. In DLS or TDLS transmission, the AID for the
	peer STA is obtained from DLS Setup Request and Response frame or TDLS Setup Request and
	Response frame. In a VHT PPDU that carries MPDUs addressed to an AP(#1278), the TXVECTOR
	parameter PARTIAL_AID(#1509) is set to the lower 9 bits of the BSSID. In a VHT PPDU
	addressed to an IBSS peer STA, the TXVECTOR parameter PARTIAL_AID(#1509) is set to 0.
	TODO PW Need to follow above in comments to generate partial AID -- ok for quickturn tho
	*/
	return 0;
}
static vht_group_id_t
wlc_compute_vht_groupid(uint16 fc)
{
	/* In a SU VHT PPDU, if the PPDU carries MPDU(s)
	addressed to an AP the Group ID(Ed) field is set to 0, otherwise
	it is set to 63.
	For a MU-MIMO PPDU the Group ID is set as in 22.3.12.3
	(Group ID)(#972)
	*/
	bool toAP = ((fc & (FC_TODS | FC_FROMDS)) == FC_TODS);
	vht_group_id_t gid = VHT_SIGA1_GID_MAX_GID;

	if (toAP) {
		gid = VHT_SIGA1_GID_TO_AP;
	} else {
		gid = VHT_SIGA1_GID_NOT_TO_AP;
	}
	ASSERT(gid <= VHT_SIGA1_GID_MAX_GID);
	ASSERT(gid >= 0);
	return gid;
}

/** Rate: 802.11 rate code, length: PSDU length in octets */
static void
wlc_compute_vht_plcp(wlc_info_t *wlc, wlc_bsscfg_t *cfg, ratespec_t rspec, uint length,
	uint16 fc, uint8 *plcp)
{
	/* compute VHT plcp - 6 B */
	uint32 plcp0 = 0, plcp1 = 0;
	vht_group_id_t gid = wlc_compute_vht_groupid(fc);
	BCM_REFERENCE(length);

	/* set up const fields -- bit 23 and bit 2 */
	plcp0 = VHT_SIGA1_CONST_MASK;

	/* L-STF L-LTF LSIG VHT-SIG-A VHTSTF VHTLTF VHTLTF      VHTSIG-B Data */
	/* VHT Sig A1 3 Bytes */
	/* bit 0..1 bandwidth */
	/* Set to 0 for 20 MHz,
	        1 for 40 MHz, 2 for 80 MHz, 3 for
	        160 MHz and 80+80 MHz
	*/
	switch (RSPEC_BW(rspec)) {
		case WL_RSPEC_BW_20MHZ:
		/* set to zero eg no-op */
			plcp0 |= VHT_SIGA1_20MHZ_VAL;
			break;
		case WL_RSPEC_BW_40MHZ:
			plcp0 |= VHT_SIGA1_40MHZ_VAL;
			break;
		case WL_RSPEC_BW_80MHZ:
			plcp0 |= VHT_SIGA1_80MHZ_VAL;
			break;
		case WL_RSPEC_BW_160MHZ:
			plcp0 |= VHT_SIGA1_160MHZ_VAL;
			break;
		default:
			ASSERT(0);
	}

	/* bit 3 stb coding */
	if (RSPEC_ISSTBC(rspec))
		plcp0 |= VHT_SIGA1_STBC;

	/* (in SU) bit4-9 if pkt addressed to AP = 0, else 63 */
	plcp0 |= (gid << VHT_SIGA1_GID_SHIFT);

	/* b10-b13 NSTS */
	/* for SU b10-b12 set to num STS-1 (fr 0-7) */
	plcp0 |= ((wlc_ratespec_nsts(rspec)-1) << VHT_SIGA1_NSTS_SHIFT);

	/* b13-b21 partial AID: Set to value of TXVECTOR param PARTIAL_AID */
	plcp0 |= (wlc_vht_siga1_get_partial_aid() << VHT_SIGA1_PARTIAL_AID_SHIFT);

	/* b22 TXOP 1 if txop PS permitted, 0 otherwise */
	/* Indicates whether or not AP
		supports VHT TXOP PS Mode
		for STAs in BSS when included
		in Beacon/Probe Response
		frames.
		Indicates whether or not STA is
		in VHT TXOP PS mode when
		included in(#1513) Association/
		Reassociation Requests(#1501)
	*/

	/* put plcp0 in plcp */
	plcp[0] = (plcp0 & 0xff);
	plcp[1] = ((plcp0 >> 8) & 0xff);
	plcp[2] = ((plcp0 >> 16) & 0xff);

	/* VHT Sig A2 3 Bytes */

	/* B0-B1 Short GI:
	 * B0 is Short GI
	 * B2 disambiguates length, but HW sets this bit
	 */
	if (RSPEC_ISSGI(rspec)) {
		plcp1 |= VHT_SIGA2_GI_SHORT;
	}

	/* B2-B3 Coding 2 B3:(#1304)
	For SU, B2 is set to 0 for BCC, 1 for LDPC
	For MU, if the NSTS field for user 1 is non-zero, then B2
	indicates the coding used for user 1; set to 0 for BCC and
	1 for LDPC. If the NSTS field for user 1 is set to 0, then
	this field is reserved and set to 1.
	B3:
	Set to 1 if LDPC PPDU encoding process (or at least one
	LPDC user's PPDU encoding process) results in an extra
	OFDM symbol (or symbols) as described in 22.3.11.5.2
	(LDPC coding)(#853) and 22.3.11.5.3 (Encoding process
	for MU transmissions)(#1304). Set to 0 otherwise.
	*/
	if (RSPEC_ISLDPC(rspec))
		plcp1 |= VHT_SIGA2_CODING_LDPC;

	/* B4-B7 MCS 4 For SU:
	MCS index
	For MU:
	If the NSTS field for user 2 is non-zero, then B4 indicates
	coding for user 2: set to 0 for BCC, 1 for LDPC. If NSTS
	for user 2 is set to 0, then B4 is reserved and set to 1.
	If the NSTS field for user 3 is non-zero, then B5 indicates
	coding for user 3: set to 0 for BCC, 1 for LDPC. If NSTS
	for user 3 is set to 0, then B5 is reserved and set to 1.
	If the NSTS field for user 4 is non-zero, then B6 indicates
	coding for user 4: set to 0 for BCC, 1 for LDPC. If NSTS
	for user 4 is set to 0, then B6(#612) is reserved and set to
	1. B7 is reserved and set to 1
	*/
	plcp1 |= ((rspec & WL_RSPEC_VHT_MCS_MASK) << VHT_SIGA2_MCS_SHIFT);

	/* B8 Beamformed 1 For SU:
	Set to 1 if a Beamforming steering matrix is applied to the
	waveform in an SU transmission as described in
	19.3.11.11.2 (Spatial mapping)(#1631), set to 0 otherwise.
	For MU:
	Reserved and set to 1
	*/
	if (RSPEC_ISTXBF(rspec) && RSPEC_ISVHT(rspec))
		plcp1 |= VHT_SIGA2_BEAMFORM_ENABLE;

	/* B9 Reserved 1 Reserved and set to 1 */
	plcp1 |= VHT_SIGA2_B9_RESERVED;

	plcp[3] = (plcp1 & 0xff);
	plcp[4] = ((plcp1 >> 8) & 0xff);
	plcp[5] = ((plcp1 >> 16) & 0xff);
} /* wlc_compute_vht_plcp */

/** Transmit related. Length: PSDU length in octets */
static void
wlc_compute_mimo_plcp(wlc_info_t *wlc, wlc_bsscfg_t *cfg, ratespec_t rspec, uint length,
	uint16 fc, uint8 *plcp)
{
	uint8 plcp3;
	uint8 mcs = (uint8)(rspec & WL_RSPEC_HT_MCS_MASK);

	ASSERT(RSPEC_ISHT(rspec));

	BCM_REFERENCE(fc);

	plcp[0] = mcs;
	if (RSPEC_IS40MHZ(rspec) || (mcs == 32))
		plcp[0] |= MIMO_PLCP_40MHZ;
	WLC_SET_MIMO_PLCP_LEN(plcp, length);

	plcp3 = 0x7;	/* set smoothing, not sounding ppdu & reserved */

	/* support STBC expansion of Nss = 1 (MCS 0-7) to Nsts = 2 */
	if (RSPEC_ISSTBC(rspec)) {
		/* STC = Nsts - Nss */
		plcp3 |= 1 << PLCP3_STC_SHIFT;
	}

	if (RSPEC_ISLDPC(rspec)) {
		plcp3 |= PLCP3_LDPC;
	}

	if (RSPEC_ISSGI(rspec)) {
		plcp3 |= PLCP3_SGI;
	}

	plcp[3] = plcp3;

	plcp[4] = 0;	/* number of extension spatial streams bit 0 & 1 */
	plcp[5] = 0;
}

/** Only called with a 'basic' or 'a/g' phy rate spec. length: PSDU length in octets */
static void
wlc_compute_ofdm_plcp(wlc_info_t *wlc, wlc_bsscfg_t *cfg, ratespec_t rspec, uint32 length,
	uint16 fc, uint8 *plcp)
{
	uint8 rate_signal;
	uint32 tmp = 0;
	int rate;

	ASSERT(RSPEC_ISOFDM(rspec));
	BCM_REFERENCE(fc);

	/* extract the 500Kbps rate for rate_info lookup */
	rate = RSPEC2RATE(rspec);

	/* encode rate per 802.11a-1999 sec 17.3.4.1, with lsb transmitted first */
	rate_signal = rate_info[rate] & RATE_INFO_RATE_MASK;
	ASSERT(rate_signal != 0);

	bzero(plcp, D11_PHY_HDR_LEN);
	D11A_PHY_HDR_SRATE((ofdm_phy_hdr_t *)plcp, rate_signal);

	tmp = (length & 0xfff) << 5;
	plcp[2] |= (tmp >> 16) & 0xff;
	plcp[1] |= (tmp >> 8) & 0xff;
	plcp[0] |= tmp & 0xff;
}

/**
 * Compute PLCP, but only requires actual rate and length of pkt. Rate is given in the driver
 * standard multiple of 500 kbps. le is set for 11 Mbps rate if necessary. Broken out for PRQ.
 */
void
wlc_cck_plcp_set(int rate_500, uint length, uint8 *plcp)
{
	uint16 usec = 0;
	uint8 le = 0;

	switch (rate_500) {
	case 2:
		usec = length << 3;
		break;
	case 4:
		usec = length << 2;
		break;
	case 11:
		usec = (length << 4)/11;
		if ((length << 4) - (usec * 11) > 0)
			usec++;
		break;
	case 22:
		usec = (length << 3)/11;
		if ((length << 3) - (usec * 11) > 0) {
			usec++;
			if ((usec * 11) - (length << 3) >= 8)
				le = D11B_PLCP_SIGNAL_LE;
		}
		break;

	default:
		WL_ERROR(("%s: unsupported rate %d\n", __FUNCTION__, rate_500));
		ASSERT(!"invalid rate");
		break;
	}
	/* PLCP signal byte */
	plcp[0] = rate_500 * 5; /* r (500kbps) * 5 == r (100kbps) */
	/* PLCP service byte */
	plcp[1] = (uint8)(le | D11B_PLCP_SIGNAL_LOCKED);
	/* PLCP length uint16, little endian */
	plcp[2] = usec & 0xff;
	plcp[3] = (usec >> 8) & 0xff;
	/* PLCP CRC16 */
	plcp[4] = 0;
	plcp[5] = 0;
} /* wlc_cck_plcp_set */

/** Rate: 802.11 rate code, length: PSDU length in octets */
static void
wlc_compute_cck_plcp(wlc_info_t *wlc, wlc_bsscfg_t *cfg, ratespec_t rspec, uint length,
	uint16 fc, uint8 *plcp)
{
	int rate;

	ASSERT(RSPEC_ISCCK(rspec));

	BCM_REFERENCE(fc);

	/* extract the 500Kbps rate for rate_info lookup */
	rate = RSPEC2RATE(rspec);

	wlc_cck_plcp_set(rate, length, plcp);
}

/**
 * Calculate the 802.11 MAC header DUR field for MPDU
 * DUR for a single frame = 1 SIFS + 1 ACK
 * DUR for a frame with following frags = 3 SIFS + 2 ACK + next frag time
 *
 * rate			MPDU rate in unit of 500kbps
 * next_frag_len	next MPDU length in bytes
 * preamble_type	use short/GF or long/MM PLCP header
 */
uint16
wlc_compute_frame_dur(wlc_info_t *wlc, uint8 bandunit, ratespec_t rate,
	uint8 preamble_type, uint next_frag_len)
{
	uint16 dur, sifs;

	sifs = SIFS(bandunit);

	dur = sifs;
	dur += (uint16)wlc_calc_ack_time(wlc, rate, preamble_type);

	if (next_frag_len) {
		/* Double the current DUR to get 2 SIFS + 2 ACKs */
		dur *= 2;
		/* add another SIFS and the frag time */
		dur += sifs;
		dur += (uint16)wlc_calc_frame_time(wlc, rate, preamble_type, next_frag_len);
	}
	return (dur);
}

/**
 * Calculate the 802.11 MAC header DUR field for an RTS or CTS frame
 * DUR for normal RTS/CTS w/ frame = 3 SIFS + 1 CTS + next frame time + 1 ACK
 * DUR for CTS-TO-SELF w/ frame    = 2 SIFS         + next frame time + 1 ACK
 *
 * cts			cts-to-self or rts/cts
 * rts_rate		rts or cts rate in unit of 500kbps
 * rate			next MPDU rate in unit of 500kbps
 * frame_len		next MPDU frame length in bytes
 */
uint16
wlc_compute_rtscts_dur(wlc_info_t *wlc, uint8 bandunit, bool cts_only,
	ratespec_t rts_rate, ratespec_t frame_rate, uint8 rts_preamble_type,
	uint8 frame_preamble_type, uint frame_len, bool ba)
{
	uint16 dur, sifs;

	sifs = SIFS(bandunit);

	if (!cts_only) {	/* RTS/CTS */
		dur = 3 * sifs;
		dur += (uint16)wlc_calc_cts_time(wlc, rts_rate, rts_preamble_type);
	} else {	/* CTS-TO-SELF */
		dur = 2 * sifs;
	}

	dur += (uint16)wlc_calc_frame_time(wlc, frame_rate, frame_preamble_type, frame_len);
#ifdef WL11N
	if (ba)
		dur += (uint16)wlc_calc_ba_time(wlc->hti, frame_rate, WLC_SHORT_PREAMBLE);
	else
#endif // endif
		dur += (uint16)wlc_calc_ack_time(wlc, frame_rate, frame_preamble_type);
	return (dur);
}

uint16
wlc_mgmt_ctl_d11hdrs(wlc_info_t *wlc, void *pkt, struct scb *scb,
	uint queue, ratespec_t rspec_override)
{
	return wlc_d11hdrs(wlc, pkt, scb, 0, 0, 0, queue, 0, NULL, NULL, rspec_override);
}

void
wlc_req_wd_block(wlc_info_t *wlc, uint set_clear, uint req)
{
	if (set_clear == WLC_BIT_SET) {
		mboolset(wlc->wd_block_req, req);
	} else {
		mboolclr(wlc->wd_block_req, req);
	}
}
void
wlc_tx_suspend(wlc_info_t *wlc)
{
	wlc->tx_suspended = TRUE;

	wlc_bmac_tx_fifo_suspend(wlc->hw, TX_DATA_FIFO);
}

bool
wlc_tx_suspended(wlc_info_t *wlc)
{
	return wlc_bmac_tx_fifo_suspended(wlc->hw, TX_DATA_FIFO);
}

void
wlc_tx_resume(wlc_info_t *wlc)
{
	bool silence = FALSE;
	int idx;
	wlc_bsscfg_t *cfg;

	wlc->tx_suspended = FALSE;

	/* XXX JQL: we should resume h/w tx fifo here and let the driver to
	 * handle any further blocking in case of multiple STA environment
	 * where each STA must have a different MAC address (otherwise wlc_clear_mac()
	 * call in wlc_start_quiet2() will disable ucode ACKs for all STAs
	 * that share that same MAC address).
	 */
	FOREACH_AS_STA(wlc, idx, cfg) {
		if (BSS_QUIET_STATE(wlc->quiet, cfg) & SILENCE) {
			/* do not unsuspend when in quiet period */
			silence = TRUE;
			break;
		}
	}
	if (!silence && !wlc_quiet_chanspec(wlc->cmi, WLC_BAND_PI_RADIO_CHANSPEC)) {
		wlc_bmac_tx_fifo_resume(wlc->hw, TX_DATA_FIFO);
	}
}

#ifdef STA
void
wlc_watchdog_run_wd_now(wlc_info_t *wlc)
{
	/* wlc_watchdog specific */
	if (mboolisset(wlc->wd_state, DEFERRED_WLC_WD)) {
		mboolclr(wlc->wd_state, DEFERRED_WLC_WD);
		wlc_watchdog((void *)wlc);
		wlc->WDlast = OSL_SYSUPTIME();

		/* schedule backup timer */
		wlc_refresh_wd_timer(wlc, wlc_watchdog_backup_bi(wlc), TRUE);
	}

	/* phy watchdog specific */
#ifdef LPAS
	if (wlc->lpas && mboolisset(wlc->wd_state, DEFERRED_PHY_WD)) {
		mboolclr(wlc->wd_state, DEFERRED_PHY_WD);
		phy_watchdog((phy_info_t *) wlc->hw->band->pi);
		wlc->last_lpas_phy_wd_ms = OSL_SYSUPTIME();

#if defined(WL11N)
			if (TVPM_ENAB(wlc->pub)) {
				wlc_tvpm_update(wlc->tvpm);
			} else {
				wlc_stf_tempsense_upd(wlc);
			}
#endif /* WL11N */
	}
#endif /* LPAS */
}

void
wlc_watchdog_process_ucode_sleep_intr(wlc_info_t *wlc)
{
	if (mboolisset(wlc->user_wake_req, WLC_USER_WAKE_REQ_WDWAITBCN)) {
		wlc_watchdog_run_wd_now(wlc);
#ifdef BCMPCIEDEV
		if (BCMPCIEDEV_ENAB()) {
			wl_add_timer(wlc->wl, wlc->wdtimer_maccore_state, 0, FALSE);
		}
#endif /* BCMPCIEDEV */
	}
	wlc_user_wake_upd(wlc, WLC_USER_WAKE_REQ_WDWAITBCN, FALSE);
}

void
wlc_watchdog_disable_ucode_sleep_intr(wlc_info_t *wlc)
{
	if (mboolisset(wlc->user_wake_req, WLC_USER_WAKE_REQ_WDWAITBCN)) {
		wlc_write_shm(wlc, M_SLP_RDY_INT(wlc), 0);
		wlc_user_wake_upd(wlc, WLC_USER_WAKE_REQ_WDWAITBCN, FALSE);
	}
#ifdef BCMPCIEDEV
	if (BCMPCIEDEV_ENAB()) {
		wl_del_timer(wlc->wl, wlc->wdtimer_maccore_state);
	}
#endif /* BCMPCIEDEV */
}

static void
wlc_run_watchdog(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	uint32 cur, delta;
	uint32 wdtimer_delay = wlc_watchdog_backup_bi(wlc);

	if (!wlc_bsscfg_is_associated(cfg)) {
		/* link down may have occurred, let watchdog run from timer */
		return;
	}

	if (wlc->WDarmed) {
		wl_del_timer(wlc->wl, wlc->wdtimer);
		wlc->WDarmed = FALSE;
	}

	cur = OSL_SYSUPTIME();
	delta = cur > wlc->WDlast ? cur - wlc->WDlast :
	        (uint32)~0 - wlc->WDlast + cur + 1;

	if (delta >= TIMER_INTERVAL_WATCHDOG) {
		if (mboolisset(wlc->wd_state, WATCHDOG_ON_BCN)) {
			/*
			* schedule backup watchdog:
			* pretbtt + WLC_WD_BCMC_TIMEOUT
			*/
			wdtimer_delay = wlc_pretbtt_calc(cfg)/1000 + WLC_WD_BCMC_TIMEOUT;

			if (WLC_WATCHDOG_TBTT(wlc)) {
				/* case where watchdog beacon_alignement will work */
				if (!mboolisset(wlc->user_wake_req, WLC_USER_WAKE_REQ_WDWAITBCN)) {
					/*
					* Keep ucode stay awake when phy calibration is running.
					* via wlc->WDwaitforBcn
					*/
					wlc_write_shm(wlc, M_SLP_RDY_INT(wlc), (MI_TTTT>>16));
					mboolset(wlc->wd_state, DEFERRED_WLC_WD);
					mboolclr(wlc->wd_state, WD_DEFERRED_PM0_BCNRX);
					wlc_user_wake_upd(wlc, WLC_USER_WAKE_REQ_WDWAITBCN, TRUE);
				} else {
					/* some other cfg has armed watchdog already, return */
					return;
				}
			} else {
				/*
				* reached here because device is not in sleep state
				* push watchdog post beacon RX for cfg not in PM enabled state
				*/
				if (!cfg->pm->PMenabled &&
					!mboolisset(wlc->wd_state, WD_DEFERRED_PM0_BCNRX)) {
					mboolset(wlc->wd_state, WD_DEFERRED_PM0_BCNRX |
						DEFERRED_WLC_WD);
					wlc_watchdog_disable_ucode_sleep_intr(wlc);
				}
				else {
					/* this cfg not allowed to do anything */
					return;
				}
			}
		} else {
			/* if watchdog_on_bcn is disabled via IOVAR, run watchdog now. */
			wlc_watchdog((void *)wlc);
			wlc_watchdog_disable_ucode_sleep_intr(wlc);
			mboolclr(wlc->wd_state, WD_DEFERRED_PM0_BCNRX | DEFERRED_WLC_WD);
			/* update last run Watchdog time */
			do {
				wlc->WDlast += TIMER_INTERVAL_WATCHDOG;
				delta -= TIMER_INTERVAL_WATCHDOG;
			} while (delta > TIMER_INTERVAL_WATCHDOG);
		}
	}

	/* activate backup watchdog */
	wlc_refresh_wd_timer(wlc, wdtimer_delay, TRUE);
} /* wlc_run_watchdog */

/* Check if watchdog needs to run on tbtt */
bool
wlc_watchdog_on_tbtt(wlc_info_t *wlc)
{
	bool wd_on_tbtt = FALSE;
	wlc_bsscfg_t *cfg;
	int idx;

	wd_on_tbtt = wlc->pub->align_wd_tbtt &&
		wlc->stas_connected && !wlc->ibss_bsscfgs &&
		!wlc->aps_associated;

	if (wd_on_tbtt) {
		/* hence forth all infra BSS only */
		FOREACH_AS_STA(wlc, idx, cfg) {
			if (!cfg->pm->PMenabled) {
				wd_on_tbtt = FALSE;
				break;
			}
		}
	}
	return wd_on_tbtt;
}
#endif /* STA */

void
wlc_bss_tbtt(wlc_bsscfg_t *cfg)
{
#ifdef LPAS
	uint32	cur_time;
#endif /* LPAS */
	wlc_info_t *wlc = cfg->wlc;
	(void)wlc;
#ifdef LPAS
	(void)cur_time;
#endif /* LPAS */

	WL_APSTA_BCN(("wl%d: wlc_tbtt: wlc_bss_tbtt()\n", wlc->pub->unit));

#ifdef STA
	if (BSSCFG_STA(cfg) && cfg->BSS) {
		/*
		* wlc_run_watchdog internally checks if watchdog_bcn_alignment
		* is possible. This will be TRUE if all STA cfgs are in PS state. If not,
		* we atleast try to run watchdog after beacon RX for one STA cfg.
		* If multiple STA cfgs are associated, beacon loss in PM0 may occur
		* if beacon RX collides with watchdog execution
		*/
		if (!wlc->aps_associated) {
			/* no Infra AP is up, proceed */
			wlc_run_watchdog(wlc, cfg);
		}
#ifdef WLMCNX
		/* p2p ucode has per BSS pretbtt.
		 * for some reason such as scan, if tbtt comes when the radio
		 * is on a off channel, p2p_tbtt_miss will count such instances
		 */
		if (MCNX_ENAB(wlc->pub) &&
			(WLC_BAND_PI_RADIO_CHANSPEC != cfg->current_bss->chanspec)) {
			WLCNTINCR(wlc->pub->_cnt->p2p_tbtt_miss);
		}
#endif /* WLMCNX */

#ifdef LPAS
		cur_time = OSL_SYSUPTIME();
		if (wlc->lpas && (cur_time - wlc->last_lpas_phy_wd_ms > MS_IN_SEC)) {
			if (!mboolisset(wlc->wd_state, WATCHDOG_ON_BCN) ||
				!WLC_WATCHDOG_TBTT(wlc)) {
				wlc_watchdog_disable_ucode_sleep_intr(wlc);
				if (!mboolisset(wlc->wd_state, WD_DEFERRED_PM0_BCNRX)) {
					mboolclr(wlc->wd_state, DEFERRED_PHY_WD);
					/* time is right to run watchdog immediately */
					phy_watchdog((phy_info_t *)wlc->hw->band->pi);
					wlc->last_lpas_phy_wd_ms = cur_time;

#if defined(WL11N)
			if (TVPM_ENAB(wlc->pub)) {
				wlc_tvpm_update(wlc->tvpm);
			} else {
				wlc_stf_tempsense_upd(wlc);
			}
#endif /* WL11NM */

				}
				else {
					/* postpone till beacon rx */
					mboolset(wlc->wd_state, DEFERRED_PHY_WD);
				}
			} else {
				/* postpone till MI_TTTT interrupt handler */
				mboolset(wlc->wd_state, DEFERRED_PHY_WD);
				if (mboolisset(wlc->user_wake_req, WLC_USER_WAKE_REQ_WDWAITBCN)) {
					/* nothing more to do */
				}
				else {
					wlc_write_shm(wlc, M_SLP_RDY_INT(wlc),
						(MI_TTTT>>16));
					wlc_user_wake_upd(wlc, WLC_USER_WAKE_REQ_WDWAITBCN, TRUE);
				}
			}
		}
#endif /* LPAS */
		wlc_roam_handle_beacon_loss(cfg);
	}
#endif /* STA */

#ifdef AP
	if (BSSCFG_AP(cfg)) {
#ifdef WL_MODESW
		if (WLC_MODESW_ENAB(wlc->pub))
			wlc_modesw_bss_tbtt(wlc->modesw, cfg);
#endif // endif
		if (BSS_WL11H_ENAB(wlc, cfg) ||
		    BSSCFG_IS_CSA_IN_PROGRESS(cfg) ||
		    FALSE) {
			wlc_11h_tbtt(wlc->m11h, cfg);
		}
#ifdef WLWNM_AP
		if (WLWNM_ENAB(wlc->pub))
			wlc_wnm_tbtt(wlc, cfg);
#endif /* WLWNM_AP */
	}
#endif /* AP */

	/* AP and STA TBTT handlers */
#ifdef WLTWT
	if (TWT_ENAB(wlc->pub)) {
		wlc_twt_tbtt(wlc, cfg);
	}
#endif /* WLTWT */

} /* wlc_bss_tbtt */

/**
 * MI_TBTT and MI_DTIM_TBTT interrupt handler. The interrupt is generated when tsf_cfpstart register
 * is programmed and tsf_timerlow = tsf_cfpstart + pre_tbtt.
 *
 * - In APSTA mode when the AP is brought up the tsf timers including tsf_cfpstart register are
 *   allocated to the AP otherwise when there is no AP running they are allocated to the primary STA
 *   (primary bsscfg).
 * - When P2P is enabled SHM TSFs are allocated for each STA BSS including the primary STA (primary
 *   bsscfg).
 * NOTE: this is called for non-p2p ucode only; see wlc_bss_tbtt for p2p ucode equivalent
 */
void
wlc_tbtt(wlc_info_t *wlc, d11regs_t *regs)
{
	BCM_REFERENCE(regs);
	WLCNTINCR(wlc->pub->_cnt->tbtt);

	WL_TRACE(("wl%d: wlc_tbtt: TBTT indication\n", wlc->pub->unit));

#ifdef DEBUG_TBTT
	if (WL_INFORM_ON()) {
		wlc->prev_TBTT = R_REG(wlc->osh, D11_TSFTimerLow(wlc_hw));
	}
#endif /* DEBUG_TBTT */

	if (AP_ACTIVE(wlc)) {
		wlc_ap_tbtt(wlc);
		return;
	}
	wlc_sta_tbtt(wlc);
}

static void
wlc_sta_tbtt(wlc_info_t *wlc)
{
	wlc_bsscfg_t *cfg = wlc->cfg;

	if (!BSSCFG_STA(cfg))
		return;

	WL_APSTA_BCN(("wl%d: wlc_tbtt: wlc_sta_tbtt()\n", wlc->pub->unit));
	WL_RTDC2(wlc, "wlc_tbtt: PMe=%u", cfg->pm->PMenabled, 0);

	if (!cfg->BSS) {
		/* DirFrmQ is now valid...defer setting until end of ATIM window */
		wlc->qvalid |= MCMD_DIRFRMQVAL;
	}

#ifdef WLMCNX
	/* p2p ucode has per BSS pretbtt */
	if (MCNX_ENAB(wlc->pub) && cfg->BSS)
		return;
#endif // endif
	wlc_bss_tbtt(cfg);
} /* wlc_sta_tbtt */

static void
wlc_ap_tbtt(wlc_info_t *wlc)
{
	int idx;
	wlc_bsscfg_t *cfg;

	ASSERT(AP_ACTIVE(wlc));

	WL_APSTA_BCN(("wl%d: wlc_tbtt: wlc_ap_tbtt()\n", wlc->pub->unit));

	/* increment scb tbtt counts, used for PS aging */
	wlc_apps_tbtt_update(wlc);

#ifdef WLMCNX
	/* p2p ucode has per BSS pretbtt */
	if (MCNX_ENAB(wlc->pub))
		return;
#endif // endif

#if defined(WL_OCE) && defined(WL_OCE_AP) && !defined(WLMCNX)
		if (OCE_ENAB(wlc->pub)) {
			wlc_oce_pretbtt_fd_callback(wlc->oce);
		}
#endif /* WL_OCE && WL_OCE_AP && !WLMCNX */

	FOREACH_UP_AP(wlc, idx, cfg) {
		wlc_bss_tbtt(cfg);
	}

	wlc_he_ap_tbtt(wlc->hei);
}

/**
 * This function needs to be called if the user of gptimer requires the device to stay awake while
 * gptimer is running. An exception to this rule would be if the user of gptimer knows that the
 * device will always be awake when it is using gptimer.
 */
void
wlc_gptimer_wake_upd(wlc_info_t *wlc, mbool requester, bool set)
{
	mbool old_req = wlc->gptimer_stay_awake_req;

	if (set) {
		mboolset(wlc->gptimer_stay_awake_req, requester);
		/* old request was cleared, we just set a new request.
		 * need to update wake ctrl
		 */
		if (old_req == 0) {
			wlc_set_wake_ctrl(wlc);
		}
	} else {
		mboolclr(wlc->gptimer_stay_awake_req, requester);
		/* if old request was set and now it is not set,
		 * need to update wake ctrl
		 */
		if (old_req && (wlc->gptimer_stay_awake_req == 0)) {
			wlc_set_wake_ctrl(wlc);
		}
	}
}

/** This function needs to be called if the user requires the device to stay awake */
void
wlc_user_wake_upd(wlc_info_t *wlc, mbool requester, bool set)
{
	mbool old_req = wlc->user_wake_req;

	if (set) {
		mboolset(wlc->user_wake_req, requester);
		/* old request was cleared, we just set a new request.
		 * need to update wake ctrl
		 */
		if (old_req == 0) {
			wlc_set_wake_ctrl(wlc);
		}
	} else {
		mboolclr(wlc->user_wake_req, requester);
		/* if old request was set and now it is not set,
		 * need to update wake ctrl
		 */
		if (old_req && (wlc->user_wake_req == 0)) {
			wlc_set_wake_ctrl(wlc);
		}
	}
}

static void
wlc_nav_reset_war(wlc_info_t *wlc, bool disable)
{
	if (disable) {
		wlc->nav_reset_war_disable = TRUE;
		wlc_mhf(wlc, MHF2, MHF2_BTCANTMODE, MHF2_BTCANTMODE, WLC_BAND_ALL);
	} else {
		wlc->nav_reset_war_disable = FALSE;
		wlc_mhf(wlc, MHF2, MHF2_BTCANTMODE, 0, WLC_BAND_ALL);
	}
}

/* XXX: Following two capability related functions are forced into RAM to avoid abandons
 * when capabilities change post-tapeout
 */
/** Return driver capability string where the "capability" means *both* the sw and hw exist */
static char*
BCMRAMFN(wlc_cap)(wlc_info_t *wlc, char *buf, uint bufsize)
{
	struct bcmstrbuf b;

	bcm_binit(&b, buf, bufsize);

	wlc_cap_bcmstrbuf(wlc, &b);

	/* this is either full or overflow. return error */
	if (b.size <= 1)
		return NULL;

	return (buf);
}

static void
BCMRAMFN(wlc_cap_bcmstrbuf)(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	/* pure software-only components */

/* Need AP defined here to print out capability only if enabled */
#ifdef AP
	bcm_bprintf(b, "ap ");
#endif // endif

#ifdef STA
	bcm_bprintf(b, "sta ");
#endif /* STA */
#if defined(WET) || defined(WET_DONGLE)
	bcm_bprintf(b, "wet ");
#endif /* WET || WET_DONGLE */
#ifdef WET_TUNNEL
	bcm_bprintf(b, "wet_tunnel ");
#endif /* WET_TUNNEL */
#ifdef MAC_SPOOF
	bcm_bprintf(b, "mac_spoof ");
#endif /* MAC_SPOOF */
#if defined(TOE) && !defined(TOE_DISABLED)
	bcm_bprintf(b, "toe ");
#endif /* TOE */
#ifdef WLLED
	bcm_bprintf(b, "led ");
#endif /* WLLED */
#ifdef WME
	bcm_bprintf(b, "wme ");
#endif /* WME */
#ifdef WLPIO
	bcm_bprintf(b, "pio ");
#endif /* PIO */
#ifdef WL11D
	bcm_bprintf(b, "802.11d ");
#endif /* WL11D */
#ifdef WL11H
	bcm_bprintf(b, "802.11h ");
#endif /* WL11H */
#if defined(WLRM) && !defined(WLRM_DISABLED)
	bcm_bprintf(b, "rm ");
#endif /* WLRM */
#ifdef WLCQ
	bcm_bprintf(b, "cqa ");
#endif /* WLCQ */
#ifdef BCM_RECLAIM_INIT_FN_DATA
	bcm_bprintf(b, "reclm ");
#endif /* BCM_RECLAIM_INIT_FN_DATA */
#if defined(WLCAC) && !defined(WLCAC_DISABLED)
	bcm_bprintf(b, "cac ");
#endif /* WLCAC */
#ifdef MBSS
	if (MBSS_SUPPORT(wlc->pub)) {
		if (wlc_bmac_ucodembss_hwcap(wlc->hw))
			bcm_bprintf(b, "mbss%d ", wlc->pub->tunables->maxucodebss);
	}
#endif /* MBSS */

	/* combo software+hardware components */

	if (NBANDS(wlc) > 1)
		bcm_bprintf(b, "dualband ");
#ifdef WLAMPDU
	bcm_bprintf(b, "ampdu ampdu_tx ampdu_rx ");
#endif // endif
#ifdef WLAMSDU
	if (wlc_amsdurx_cap(wlc->ami))
		bcm_bprintf(b, "amsdurx ");

	if (wlc_amsdutx_cap(wlc->ami))
		bcm_bprintf(b, "amsdutx ");
#endif // endif
#if defined(WLTDLS)
	if (TDLS_ENAB(wlc->pub) && wlc_tdls_cap(wlc->tdls))
		bcm_bprintf(b, "tdls ");
#endif /* defined(WLTDLS) */
#ifdef WOWL
	if (wlc_wowl_cap(wlc))
		bcm_bprintf(b, "wowl ");
#endif // endif
#ifdef WMF
	bcm_bprintf(b, "wmf ");
#endif // endif
#ifdef RXCHAIN_PWRSAVE
	bcm_bprintf(b, "rxchain_pwrsave ");
#endif // endif
#ifdef RADIO_PWRSAVE
	bcm_bprintf(b, "radio_pwrsave ");
#endif // endif
#if defined(WLP2P) && !defined(WLP2P_DISABLED)
	if (wlc_p2p_cap(wlc->p2p))
		bcm_bprintf(b, "p2p ");
#endif // endif
#ifdef BCM_DCS
	bcm_bprintf(b, "bcm_dcs ");
#endif // endif
#if defined(SRHWVSDB)
	if (SRHWVSDB_ENAB(wlc->pub))
		bcm_bprintf(b, "srvsdb ");
#endif // endif
#if defined(PROP_TXSTATUS)
	if (PROP_TXSTATUS_ENAB(wlc->pub))
		bcm_bprintf(b, "proptxstatus ");
#endif // endif
#if defined(WLMCHAN)
	if (MCHAN_ENAB(wlc->pub))
		bcm_bprintf(b, "mchan ");
#endif	/* WLMCHAN */
#ifdef PSTA
	bcm_bprintf(b, "psta psr ");
#endif /* PSTA */
#ifdef WDS
	bcm_bprintf(b, "wds ");
#endif // endif
#ifdef DWDS
	bcm_bprintf(b, "dwds ");
#endif // endif
#ifdef BULK_PKTLIST
	bcm_bprintf(b, "dma-bulk-pktlist ");
#endif // endif
#ifdef WLCSO
	if (wlc->toe_capable)
		bcm_bprintf(b, "cso ");
#endif // endif
#ifdef WLTSO
	if (wlc->toe_capable)
		bcm_bprintf(b, "tso ");
#endif // endif
#if defined(P2PO) && !defined(P2PO_DISABLED)
	bcm_bprintf(b, "p2po ");
#endif // endif
#if defined(ANQPO) && !defined(ANQPO_DISABLED)
	bcm_bprintf(b, "anqpo ");
#endif // endif
#ifdef WL11AC
	if (WLC_VHT_PROP_RATES_CAP_PHY(wlc))
		bcm_bprintf(b, "vht-prop-rates ");

	if (WLC_MU_BFR_CAP_PHY(wlc))
		bcm_bprintf(b, "multi-user-beamformer ");
	if (WLC_SU_BFR_CAP_PHY(wlc))
		bcm_bprintf(b, "single-user-beamformer ");
	if (WLC_MU_BFE_CAP_PHY(wlc))
		bcm_bprintf(b, "multi-user-beamformee ");
	if (WLC_SU_BFE_CAP_PHY(wlc))
		bcm_bprintf(b, "single-user-beamformee ");
#ifdef WL11AC_160
	if (WLC_160MHZ_CAP_PHY(wlc))
		bcm_bprintf(b, "160 ");
#endif /* WL11AC_160 */
#ifdef WL11AC_80P80
	if (WLC_8080MHZ_CAP_PHY(wlc))
		bcm_bprintf(b, "80+80 ");
#endif /* WL11AC_80P80 */
#endif /* WL11AC */
#ifdef STA
	bcm_bprintf(b, "dfrts ");
#ifdef LPAS
	bcm_bprintf(b, "lpas ");
#endif /* LPAS */
#endif /* STA */
#ifdef WLTXPWR_CACHE
	bcm_bprintf(b, "txpwrcache ");
#endif // endif
#if defined(WL11N) || defined(WL11AC)
	if (WLC_STBC_CAP_PHY(wlc)) {
		bcm_bprintf(b, "stbc-tx ");
		bcm_bprintf(b, "stbc-rx-1ss ");
	}
#endif // endif
#ifdef PSPRETEND
	if (PSPRETEND_ENAB(wlc->pub)) {
		bcm_bprintf(b, "pspretend ");
	}
#endif /* PSPRETEND */
#ifdef WLRSDB
	if (RSDB_ENAB(wlc->pub)) {
#ifdef WLP2P
		if (P2P_ENAB(wlc->pub)) {
		bcm_bprintf(b, "mp2p ");
		}
#endif	/* WLP2P */

		bcm_bprintf(b, "rsdb ");
	}
#endif	/* WLRSDB */
#ifdef WLPROBRESP_MAC_FILTER
	bcm_bprintf(b, "probresp_mac_filter ");
#endif // endif
#if defined(MFP) && !defined(MFP_DISABLED)
	bcm_bprintf(b, "mfp ");
#endif /* MFP */
#if defined(WLNDOE) && !defined(WLNDOE_DISABLED)
	bcm_bprintf(b, "ndoe ");
#endif // endif
#if defined(RSSI_MONITOR) && !defined(RSSI_MONITOR_DISABLED)
	bcm_bprintf(b, "rssi_mon ");
#endif /* RSSI_MONITOR */
#if defined(WLWNM) && !defined(WLWNM_DISABLED)
	bcm_bprintf(b, "wnm ");
	bcm_bprintf(b, "bsstrans ");
#endif // endif
#ifdef WLC_TXPWRCAP
	bcm_bprintf(b, "txcap ");
#endif /* WLC_TXPWRCAP */
#if defined(WLPFN) && !defined(WLPFN_DISABLED)
	bcm_bprintf(b, "epno ");
#endif /* WLPFN && !WLPFN_DISABLED */
#if defined(WL_MPF) && !defined(WL_MPF_DISABLED)
	if (MPF_ENAB(wlc->pub))
		bcm_bprintf(b, "mpf ");
#endif /* WL_MPF && !WL_MPF_DISABLED */
	bcm_bprintf(b, "scanmac ");
#if defined(BDO) && !defined(BDO_DISABLED)
	bcm_bprintf(b, "bdo ");
#endif // endif
#ifdef WL_EXPORT_CURPOWER
	bcm_bprintf(b, "cptlv-%d ", ppr_get_tlv_ver());
#endif /* WL_EXPORT_CURPOWER */
#if defined(PACKET_FILTER2) && !defined(PKT_FLT2_DISABLED)
	bcm_bprintf(b, "pktfltr2 ");
#if defined(PACKET_FILTER6) && !defined(PKT_FLT6_DISABLED)
	bcm_bprintf(b, "pf6 ");
#endif /* PACKET_FILTER6 && !PKT_FLT6_DISABLED */
#endif /* PACKET_FILTER2 && !PKT_FLT2_DISABLED */
#if defined(WLFBT) && !defined(WLFBT_DISABLED) && defined(WL11K) && \
	!defined(WL11K_DISABLED) && defined(WME)
	bcm_bprintf(b, "ve ");
#endif /* VE */
#if defined(WLFBT) && !defined(WLFBT_DISABLED)
	bcm_bprintf(b, "fbtoverds ");
	bcm_bprintf(b, "fbt_adpt ");
#endif /* fbtoverds */
#if defined(ECOUNTERS) && !defined(ECOUNTERS_DISABLED)
	bcm_bprintf(b, "ecounters ");
#endif // endif
#ifdef EVENT_LOG_COMPILE
	bcm_bprintf(b, "event_log ");
#endif // endif
#ifdef CNTRY_DEFAULT
	bcm_bprintf(b, "cdef ");
#endif /* CNTRY_DEFAULT */
#ifdef WLC_TXPWRCAP
	bcm_bprintf(b, "txcap ");
#endif /* WLC_TXPWRCAP */
#if defined(WL_MIMOPS) && !defined(WL_MIMOPS_DISABLED)
	bcm_bprintf(b, "mimo_ps ");
#endif /* WL_MIMOPS */
#if defined(WL_STF_ARBITRATOR) && !defined(WL_STF_ARBITRATOR_DISABLED)
	bcm_bprintf(b, "arb ");
#endif /* WL_STF_ARBITRATOR */
#if defined(OCL) && !defined(OCL_DISABLED)
	bcm_bprintf(b, "ocl ");
#endif /* OCL */
#if (defined(HEALTH_CHECK) && !defined(HEALTH_CHECK_DISABLED))
	bcm_bprintf(b, "hchk ");
#endif /* HEALTH_CHECK */
#if (defined(LOGTRACE) && !defined(LOGTRACE_DISABLED))
	bcm_bprintf(b, "logtrace ");
#endif /* LOGTRACE */
#if defined(WLSCANCACHE) && !defined(WLSCANCACHE_DISABLED)
	bcm_bprintf(b, "scancache ");
#endif /* WLSCANCACHE && !WLSCANCACHE_DISABLED */
#if defined(APF) && !defined(APF_DISABLED)
		bcm_bprintf(b, "apf");
#endif /* APF */
#if defined(ICMP) && !defined(ICMP_DISABLED)
	bcm_bprintf(b, "icmp ");
#endif // endif
#ifdef WLC_API_VERSION_MAJOR
	bcm_bprintf(b, "ifver ");
#endif /* WLC_API_VERSION_MAJOR */
#if defined(BGDFS) || defined(WL_AIR_IQ)
	if (BGDFS_ENAB(wlc->pub)) {
		bcm_bprintf(b, "bgdfs ");
		if (WLC_PHY_160_FULL_NSS(wlc)) {
			bcm_bprintf(b, "bgdfs160 ");
		}
	}
#endif /* BGDFS  || WL_AIR_IQ */
#if defined(BCMAUTH_PSK) && !defined(BCMAUTH_PSK_DISABLED)
	bcm_bprintf(b, "idauth ");
#endif /* BCMAUTH_PSK | !BCMAUTH_PSK_DISABLED */
#if defined(WL_NAP) && !defined(WL_NAP_DISABLED)
	bcm_bprintf(b, "nap ");
#endif /* WL_NAP */
#if defined(WL_UCM)
	if (UCM_ENAB(wlc->pub)) {
		bcm_bprintf(b, "ucm ");
	}
#endif /* WL_UCM */
#if defined(WL_FILTERIE) && !defined(WL_FILTERIE_DISABLED)
	bcm_bprintf(b, "fie ");
#endif /* WL_FILTERIE */
#ifdef WL_TVPM
	if (TVPM_ENAB(wlc->pub)) {
		bcm_bprintf(b, "tvpm ");
	}
#endif /* WL_TVPM */

#if defined(WLC_TSYNC)
	if (TSYNC_ENAB(wlc->pub)) {
		bcm_bprintf(b, "tsync ");
	}
#endif // endif
#if defined(WL_BCNTRIM) && !defined(WL_BCNTRIM_DISABLED)
	bcm_bprintf(b, "bcntrim ");
#endif /* WL_BCNTRIM */
#if defined(TDMTX) && !defined(TDM_TX_DISABLED)
	if (TDMTX_ENAB(wlc->pub)) {
		bcm_bprintf(b, "tdmtx ");
	}
#endif // endif
	bcm_bprintf(b, "lpr_scan ");
	bcm_bprintf(b, "bkoff_evt ");
	bcm_bprintf(b, "awd_data_info ");
#if defined(WL_OPS) && !defined(WL_OPS_DISABLED)
	bcm_bprintf(b, "ops ");
#endif /* WL_OPS */
#ifdef DYN160
	if (DYN160_ENAB(wlc->pub)) {
		bcm_bprintf(b, "dyn160 ");
	}
#endif /* DYN160 */
#ifdef WL_MONITOR
	bcm_bprintf(b, "monitor ");
#endif /* WL_MONITOR */
#ifdef WL_RADIOTAP
	bcm_bprintf(b, "rtap ");
#endif // endif
#if defined(BCM_CEVENT) && !defined(BCM_CEVENT_DISABLED)
	bcm_bprintf(b, "cevent ");
#endif /* BCM_CEVENT && ! BCM_CEVENT_DISABLED */
#ifdef WL_TRAFFIC_THRESH
	bcm_bprintf(b, "traffic_thresh ");
#endif /* WL_TRAFFIC_THRESH */
#ifndef BCM_HE_DISABLE
	if (WLC_HE_CAP_PHY(wlc)) {
		bcm_bprintf(b, "11ax ");
	}
#endif /* BCM_HE_DISABLE */
#ifdef WL_STA_MONITOR
	bcm_bprintf(b, "stamon ");
#endif /* WL_STA_MONITOR */
#ifdef WLCFP
	if (CFP_ENAB(wlc->pub) == TRUE) {
		bcm_bprintf(b, "cfp ");
	}
#endif /* WLCFP */
#ifdef BCMHWA
	hwa_caps(hwa_dev, b);
#endif // endif
#ifdef WLSQS
	bcm_bprintf(b, "sqs ");
#endif // endif
#if defined(WL_AIR_IQ)
	if (wlc->airiq != NULL) {
		bcm_bprintf(b, "airiq_active ");
	} else {
		bcm_bprintf(b, "airiq ");
	}
#endif /* WL_AIR_IQ */

#if defined(WL_SAE)
	bcm_bprintf(b, "sae ");
#endif // endif
} /* wlc_cap_bcmstrbuf */

/* return pkt pointer and offset from beginning of pkt */
int BCMFASTPATH
wlc_pkt_get_txh_hdr(wlc_info_t* wlc, void* p, d11txhdr_t **hdrPtrPtr)
{
	uint8 *pktHdr = (uint8*)PKTDATA(wlc->osh, p);
	int tsoHdrSize = 0;

	ASSERT(pktHdr != NULL);
	ASSERT(p != NULL);
	ASSERT(hdrPtrPtr != NULL);
	ASSERT(D11REV_GE(wlc->pub->corerev, 40));

#ifdef WLTOEHW
	tsoHdrSize = WLC_TSO_HDR_LEN(wlc, (d11ac_tso_t*)pktHdr);
#endif // endif
	ASSERT((uint)PKTLEN(wlc->osh, p) >= D11_TXH_SHORT_LEN(wlc) + tsoHdrSize);
	*hdrPtrPtr = (d11txhdr_t*)(pktHdr + tsoHdrSize);
	return tsoHdrSize;
}

struct dot11_header*
wlc_pkt_get_d11_hdr(wlc_info_t* wlc, void *p)
{
	struct dot11_header* h = NULL;
	ASSERT(wlc);
	ASSERT(p);
	if (D11REV_LT(wlc->pub->corerev, 40)) {
		d11txh_pre40_t *nonVHTHdr = (d11txh_pre40_t *)PKTDATA(wlc->osh, p);
		ASSERT(nonVHTHdr != NULL);
		h = (struct dot11_header*)((uint8*)(nonVHTHdr + 1) + D11_PHY_HDR_LEN);
	} else {
		int offset = 0;
		uint8 *pktHdr = (uint8*)PKTDATA(wlc->osh, p);
		d11txhdr_t	*HdrPtr = NULL;
		/* Increment D11_TXH_LEN only if already added to the packet.
		 * For frames that are tossed before queuing, wlc_prep_pdu does not get the
		 * opportunity to push the d11 header in.
		 */
		if ((WLPKTTAG(p)->flags & WLF_TXHDR) != 0) {
#ifdef WLTOEHW
			offset = WLC_TSO_HDR_LEN(wlc, (d11ac_tso_t*)pktHdr);
#endif // endif
			HdrPtr = (d11txhdr_t*)(pktHdr + offset);
			if (*D11_TXH_GET_MACLOW_PTR(wlc, HdrPtr) &
				htol16(D11AC_TXC_HDR_FMT_SHORT)) {
				offset += D11_TXH_SHORT_LEN(wlc);
			} else {
				offset += D11_TXH_LEN_EX(wlc);
			}
		}
		ASSERT((uint)PKTLEN(wlc->osh, p) >=
			(offset + sizeof(struct dot11_header)));
		h = (struct dot11_header*)(pktHdr + offset);
	}
	return h;
}

void
wlc_pkt_set_ack(wlc_info_t* wlc, void* p, bool want_ack)
{
	if (D11REV_GE(wlc->pub->corerev, 40)) {
		uint16 *maclow;
		uint16 *machigh;
		d11txhdr_t* txh = NULL;
		wlc_pkt_get_txh_hdr(wlc, p, &txh);

		maclow = D11_TXH_GET_MACLOW_PTR(wlc, txh);
		machigh = D11_TXH_GET_MACHIGH_PTR(wlc, txh);

		if (!want_ack) {
			*maclow &= ~D11AC_TXC_IACK;
			/* if no immediate ACK, clear PS-pretend as well to prevent flush */
			*machigh &= ~D11AC_TXC_PPS;
		} else {
			*maclow |= D11AC_TXC_IACK;
		}
	} else {
		d11txh_pre40_t* nonVHTHdr = (d11txh_pre40_t *)PKTDATA(wlc->osh, p);
		if (!want_ack) {
			nonVHTHdr->MacTxControlLow &= ~TXC_IMMEDACK;
		} else {
			nonVHTHdr->MacTxControlLow |= TXC_IMMEDACK;
		}
	}
}

void
wlc_pkt_set_core(wlc_info_t* wlc, void* p, uint8 core)
{
	if (D11REV_LT(wlc->pub->corerev, 40) || D11REV_GE(wlc->pub->corerev, 128)) {
		/* not implemented */
		ASSERT(0);
	} else { /* rev 40 - 79 */
		d11txhdr_t* txh = NULL;
		d11actxh_rate_t* local_rate_info;

		wlc_pkt_get_txh_hdr(wlc, p, &txh);
		local_rate_info = WLC_TXD_RATE_INFO_GET(&txh->rev40, wlc->pub->corerev);

		/* send on core #ant antenna #ant */
		local_rate_info[0].PhyTxControlWord_0 &= ~D11AC_PHY_TXC_CORE_MASK;
		local_rate_info[0].PhyTxControlWord_0 |=
			(1 << (core + D11AC_PHY_TXC_CORE_SHIFT));
	}
}

void
wlc_pkt_set_txpwr_offset(wlc_info_t* wlc, void* p, uint8 pwr_offset)
{
	 if (D11REV_LT(wlc->pub->corerev, 40) || D11REV_GE(wlc->pub->corerev, 128)) {
		/* not implemented */
		ASSERT(0);
	} else { /* rev40 - 79 */
		d11txhdr_t* txh = NULL;
		d11actxh_rate_t* local_rate_info;
		wlc_pkt_get_txh_hdr(wlc, p, &txh);
		local_rate_info = WLC_TXD_RATE_INFO_GET(&txh->rev40, wlc->pub->corerev);
		if (D11REV_LT(wlc->pub->corerev, 64)) {
			local_rate_info[0].PhyTxControlWord_1 &= ~D11AC_PHY_TXC_TXPWR_OFFSET_MASK;
			local_rate_info[0].PhyTxControlWord_1 |=
				(pwr_offset << D11AC_PHY_TXC_TXPWR_OFFSET_SHIFT);
		} else {
			local_rate_info[0].PhyTxControlWord_1 &= ~D11AC2_PHY_TXC_TXPWR_OFFSET_MASK;
			local_rate_info[0].PhyTxControlWord_1 |=
				(pwr_offset << D11AC2_PHY_TXC_TXPWR_OFFSET_SHIFT);
		}
	}
}

#ifdef STA
/** update cfg->PMpending and wlc->PMpending */
void
wlc_set_pmpending(wlc_bsscfg_t *cfg, bool pm0to1)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_pm_st_t *pm = cfg->pm;
	bool oldstate = pm->PMpending;

	WL_PS(("wl%d.%d: %s: old %d new %d\n",
	       wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__, oldstate, pm0to1));

	pm->PMpending = pm0to1;
	wlc_sta_set_pmpending(wlc, cfg, pm0to1);

	if (oldstate == pm0to1)
		return;

	if (pm0to1) {
		pm->pm2_ps0_allowed = FALSE;
		wlc->PMpending = TRUE;
	}
	/* leave setting wlc->PMpending to FALSE to wlc_pm_pending_complete()
	 * else {
	 * 	int idx;
	 *	wlc_bsscfg_t *bc;
	 *	FOREACH_AS_STA(wlc, idx, bc) {
	 *		if (bc->pm->PMpending)
	 *			return;
	 *	}
	 *	wlc->PMpending = FALSE;
	 * }
	 */

	wlc_set_wake_ctrl(wlc);
} /* wlc_set_pmpending */

/** update cfg->PMawakebcn and wlc->PMawakebcn */
void
wlc_set_pmawakebcn(wlc_bsscfg_t *cfg, bool state)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_pm_st_t *pm = cfg->pm;
	bool oldstate = pm->PMawakebcn;
	wlc_cxn_t *cxn = cfg->cxn;

	WL_PS(("wl%d.%d: %s: old %d new %d\n",
	       wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__, oldstate, state));

	if (state == oldstate)
		return;

	if (state) {
		if (cxn->ign_bcn_lost_det != 0)
			return;
		pm->PMawakebcn = TRUE;
		wlc->PMawakebcn = TRUE;
	} else {
		int idx;
		wlc_bsscfg_t *bc;

		pm->PMawakebcn = FALSE;
		FOREACH_AS_STA(wlc, idx, bc) {
			if (bc->pm->PMawakebcn)
				return;
		}
		wlc->PMawakebcn = FALSE;
	}

	wlc_set_wake_ctrl(wlc);
} /* wlc_set_pmawakebcn */

/** update cfg->PSpoll and wlc->PSpoll */
void
wlc_set_pspoll(wlc_bsscfg_t *cfg, bool state)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_pm_st_t *pm = cfg->pm;
	bool oldstate = pm->PSpoll;

	WL_PS(("wl%d.%d: %s: old %d new %d\n",
	       wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__, oldstate, state));

	pm->PSpoll = state;
	if (oldstate == state)
		return;

	if (state) {
		wlc->PSpoll = TRUE;
	} else {
		int idx;
		wlc_bsscfg_t *bc;

		FOREACH_AS_STA(wlc, idx, bc) {
			if (bc->pm->PSpoll)
				return;
		}
		wlc->PSpoll = FALSE;
	}
	wlc_set_wake_ctrl(wlc);
}

/** update cfg->apsd_sta_usp and wlc->apsd_sta_usp */
void
wlc_set_apsd_stausp(wlc_bsscfg_t *cfg, bool state)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_pm_st_t *pm = cfg->pm;
	bool oldstate = pm->apsd_sta_usp;

	WL_PS(("wl%d.%d: %s: old %d new %d\n",
	       wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__, oldstate, state));

	pm->apsd_sta_usp = state;
	if (state == oldstate)
		return;

	if (state) {
		wlc->apsd_sta_usp = TRUE;
	} else {
		int idx;
		wlc_bsscfg_t *bc;
		FOREACH_BSS(wlc, idx, bc) {
			if (BSSCFG_STA(bc) && (bc->associated || BSS_TDLS_ENAB(wlc, bc)))
				if (bc->pm->apsd_sta_usp)
					return;
		}
		wlc->apsd_sta_usp = FALSE;
	}
	wlc_set_wake_ctrl(wlc);
}

/** update cfg->check_for_unaligned_tbtt and wlc->check_for_unaligned_tbtt */
void
wlc_set_uatbtt(wlc_bsscfg_t *cfg, bool state)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_pm_st_t *pm = cfg->pm;
	bool oldstate = pm->check_for_unaligned_tbtt;

	WL_PS(("wl%d.%d: %s: old %d new %d\n",
	       wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__, oldstate, state));

	pm->check_for_unaligned_tbtt = state;
	if (state == oldstate)
		return;

	if (state) {
		wlc->check_for_unaligned_tbtt = TRUE;
		wlc->ebd->uatbtt_start_time = OSL_SYSUPTIME();
#ifdef WLMCNX
		if (MCNX_ENAB(wlc->pub))
			wlc_mcnx_tbtt_inv(wlc->mcnx, cfg);
#endif // endif
	} else {
		int idx;
		wlc_bsscfg_t *bc;
		FOREACH_AS_STA(wlc, idx, bc) {
			if (bc->pm->check_for_unaligned_tbtt)
				return;
		}
		wlc->check_for_unaligned_tbtt = FALSE;
	}
	wlc_set_wake_ctrl(wlc);
}

/** update cfg->PMenabled */
void
wlc_set_pmenabled(wlc_bsscfg_t *cfg, bool state)
{
	wlc_pm_st_t *pm = cfg->pm;
#if defined(BCMDBG) || defined(WLMSG_PS)
	bool oldstate = pm->PMenabled;
#endif // endif
	WL_PS(("wl%d.%d: %s: old %d new %d\n",
	       cfg->wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__, oldstate, state));

	pm->PMenabled = state;

	/* Set the PM state for the decision to do noise measurement */
	if ((WLC_BSSCFG_IDX(cfg) == 0) && (cfg->BSS))
		phy_noise_pmstate_set((wlc_phy_t *)cfg->wlc->pi, state);

	wlc_set_ps_ctrl(cfg);
}

/** update cfg->PM_override */
void
wlc_set_pmoverride(wlc_bsscfg_t *cfg, bool state)
{
	wlc_pm_st_t *pm = cfg->pm;
	bool oldstate = pm->PM_override;

	WL_PS(("wl%d.%d: %s: old %d new %d\n",
	       cfg->wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__, oldstate, state));

	cfg->pm->PM_override = state;
	if (oldstate == state)
		return;

	wlc_set_pmstate(cfg, cfg->pm->PMenabled);
}

/** update cfg->dtim_programmed */
void
wlc_set_dtim_programmed(wlc_bsscfg_t *cfg, bool state)
{
	wlc_info_t *wlc = cfg->wlc;

	(void)wlc;

	WL_PS(("wl%d.%d: %s: old %d new %d\n",
		wlc->pub->unit,    WLC_BSSCFG_IDX(cfg), __FUNCTION__, cfg->dtim_programmed, state));

	if (cfg->dtim_programmed == state)
		return;

	cfg->dtim_programmed = state;
}

/** set dtim period on STA */
int
wlc_set_dtim_period(wlc_bsscfg_t *bsscfg, uint16 dtim)
{
	wlc_info_t *wlc = bsscfg->wlc;

	if (!BSSCFG_STA(bsscfg))
		return BCME_UNSUPPORTED;

#ifdef WLMCNX
	if (MCNX_ENAB(wlc->pub)) {
		wlc_mcnx_write_dtim_prd(wlc->mcnx, bsscfg, dtim);
	}
#else
	wlc_write_shm(wlc, M_DOT11_DTIMPERIOD(wlc), dtim);
#endif /* WLMCNX */

	return BCME_OK;
}

/** get STA dtim period */
int
wlc_get_dtim_period(wlc_bsscfg_t *bsscfg, uint16* dtim)
{
	wlc_info_t *wlc = bsscfg->wlc;

	if (!BSSCFG_STA(bsscfg))
		return BCME_UNSUPPORTED;

#ifdef WLMCNX
	if (MCNX_ENAB(wlc->pub)) {
		*dtim = wlc_mcnx_read_dtim_prd(wlc->mcnx, bsscfg);
	}
#else
	*dtim = wlc_read_shm(wlc, M_DOT11_DTIMPERIOD(wlc));
#endif /* WLMCNX */

	return BCME_OK;
}

void
wlc_update_bcn_info(wlc_bsscfg_t *cfg, bool state)
{
	wlc_set_dtim_programmed(cfg, state);

	wlc_set_ps_ctrl(cfg);
}

/* Set the module id that triggered PS mode and call wlc_set_pmstate */
void wlc_module_set_pmstate(wlc_bsscfg_t *cfg, bool state, mbool moduleId)
{
	mboolset(cfg->pm->PMenabledModuleId, moduleId);
	wlc_set_pmstate(cfg, state);
}

/* Convolute wake and associate condition to support RSDB chip
*/
bool wlc_maccore_wake_state(wlc_info_t *wlc)
{
	int idx, bss_idx;
	wlc_info_t *wlc_iter;
	bool mac_wake_state = FALSE;
	wlc_bsscfg_t	*cfg;

	FOREACH_WLC(wlc->cmn, idx, wlc_iter) {
		FOREACH_AS_BSS(wlc_iter, bss_idx, cfg) {
			if (wlc->cmn->core_wake & (1 << WLC_BSSCFG_IDX(cfg))) {
				mac_wake_state = TRUE;
			}
		}
	}

	return (mac_wake_state);
}

/* For PCIe FD, LTR state does not exactly sync with d11 core doze state.
 * We want to change LTR based on data across PCIe bus (mainly data path).
 * Currently LTR_ACTIVE is set in below scenarios
 *	1.	Any AP is up  (wlc->aps_associated) or
 *	2.	Any associated STA is in PM_OFF state.
 * This can be further optimized for some AP power saving features.
 */

void
wlc_cfg_set_pmstate_upd(wlc_bsscfg_t *cfg, bool pmenabled)
{
	wlc_info_t *wlc = cfg->wlc;
	bool wake = !pmenabled;

	if (wake) {
		wlc->cmn->core_wake |= (1 << WLC_BSSCFG_IDX(cfg));
	}
	else {
		wlc->cmn->core_wake &= ~(1 << WLC_BSSCFG_IDX(cfg));
	}
	if (wlc_maccore_wake_state(wlc))
		wl_indicate_maccore_state(wlc->wl, LTR_ACTIVE);
	else
		wl_indicate_maccore_state(wlc->wl, LTR_SLEEP);
}

/** Set PS mode and communicate new state to AP by sending null data frame */
void
wlc_set_pmstate(wlc_bsscfg_t *cfg, bool state)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_pm_st_t *pm = cfg->pm;
	uint txstatus = TX_STATUS_ACK_RCV;

	if (!BSSCFG_STA(cfg) || !cfg->BSS) {
		return;
	}

	wlc_cfg_set_pmstate_upd(cfg, state);
#ifdef PWRSTATS
	if (PWRSTATS_ENAB(wlc->pub)) {
		wlc_update_pm_history(state, CALL_SITE);
	}
#endif // endif
	WL_PS(("wl%d.%d: PM-MODE: wlc_set_pmstate: set PMenabled to %d\n",
	       wlc->pub->unit, WLC_BSSCFG_IDX(cfg), state));

	/* Disable U-APSD when STA in active mode */
	if (state == FALSE && pm->apsd_sta_usp)
		wlc_set_apsd_stausp(cfg, FALSE);

	if (state == FALSE) {
		pm->PMenabledModuleId = 0;
		pm->pm_immed_retries = 0;
		if (wlc->pub->up && BSSCFG_STA(cfg) && cfg->BSS && cfg->associated)
			wlc_pm2_ret_upd_last_wake_time(cfg, NULL);
	}

	wlc_set_pmenabled(cfg, state);

	wlc_set_pmpending(cfg, PS_ALLOWED(cfg));

	if (!wlc->pub->up)
		return;

	if (!cfg->associated ||
#if defined(BCMULP)
	    (BCMULP_ENAB() && wlc_wowl_pm2_to_pm1(wlc->wowl)) ||
#endif /* BCMULP */
	    (!wlc_portopen(cfg) &&
#ifdef WLMCHAN
	     !MCHAN_ACTIVE(wlc->pub) &&
#endif // endif
	     TRUE)) {
		WL_PS(("wl%d.%d: skip the PM null frame\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(cfg)));
		return;
	}

	WL_RTDC(wlc, "wlc_set_pmstate: tx PMep=%02u AW=%02u",
	        (pm->PMenabled ? 10 : 0) | pm->PMpending,
	        (PS_ALLOWED(cfg) ? 10 : 0) | STAY_AWAKE(wlc));

	/* If wlc->PMPending, fake a status of TX_STATUS_PMINDCTD | TX_STATUS_ACK_RCV
	 * else, fake TX_STATUS_ACK_RCV, in case the upper layers need to know of the
	 * conditions.
	 */
	if (pm->PMpending)
		txstatus |= TX_STATUS_PMINDCTD;

	/* announce PM change */
	/* send NULL data frame to communicate PM state to each associated APs */
	/* don't bother sending a null data frame if we lost our AP connection */
	if (!WLC_BSS_CONNECTED(cfg) ||
#ifdef WLMCNX
	    (!MCNX_ENAB(wlc->pub) && cfg != wlc->cfg) ||
#endif // endif
	    FALSE) {
		WL_PS(("wl%d.%d: skip the PM null frame, fake a PM0->PM1 transition\n",
		       wlc->pub->unit, WLC_BSSCFG_IDX(cfg)));
		wlc_bss_pm_pending_upd(cfg, txstatus);
		/* update the global PMpending state */
		wlc_pm_pending_complete(wlc);
	}

	/* XXX JQL: optimization opportunity:
	 * check if there is pending tx packets before sending a null packet!
	 */
	/* if excursion is already active, no point in sending null pkt out.
	 * null pkts are queued to bsscfg specific queues so they will not
	 * go out until excursion is complete.
	 */

	/*
	* NULL DATA packets are sent at normal priority by default (through TX_DATA_FIFO)
	* During PM transitions in heavy traffic scenario with scan, the NULL packets for PM
	* indication to AP sometimes gets missed or are not ACK'd before STA goes to off
	* channel and results in rate drops and/or stalls. To improve performance, changes
	* are made in the code to increase priority of NULL DATA packets during PM
	* transition by enqueuing them at max priority (in TX_CTL_FIFO).
	* With these changes, we also avoid sending QoS NULL in place of NULL data,
	* which is a problem if we attempt to increase packet priority
	*/

	else if (wlc->excursion_active ||
	         !wlc_sendpmnotif(wlc, cfg, &cfg->BSSID, 0, PKTPRIO_NON_QOS_HIGH,
	                          SCAN_IN_PROGRESS(wlc->scan) && state)) {
		WL_ERROR(("wl%d.%d: failed to send PM null frame, "
			"fake a PM0->PM1 transition, excursion_active %d\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(cfg), wlc->excursion_active));
		wlc_bss_pm_pending_upd(cfg, txstatus);
		/* update the global PMpending state */
		wlc_pm_pending_complete(wlc);
	}

	/* start periodic pspoll timer if we are indeed in PS mode */
	wlc_pspoll_timer_upd(cfg, state);
	wlc_apsd_trigger_upd(cfg, state);
} /* wlc_set_pmstate */

void
wlc_mimops_pmbcnrx(wlc_info_t *wlc)
{
	if (!si_iscoreup(wlc->pub->sih))
		return;
	/* ucode wakes radio cores depending on the value loaded in M_RXCORE_STATE
	 * Driver need to say the required coremask to ucode by updating this shm
	 * so that required core can be woke up even though PM_BCNRX is disabled
	*/
	wlc_write_shm(wlc, M_RXCORE_STATE(wlc), wlc->stf->rxchain);
	return;
}

#ifdef WLPM_BCNRX
static bool
wlc_pm_bcnrx_cap(wlc_info_t *wlc)
{
	if (D11REV_GE(wlc->pub->corerev, 48))
		return TRUE;

	return FALSE; /* 43217/4360b0 */
}

bool
wlc_pm_bcnrx_allowed(wlc_info_t *wlc)
{
	 bool allowed = TRUE;    /* Start assuming it's ok */

	/* Check blockers: chain, reduced chain, mrc override. */
	if (!(wlc->stf->hw_rxchain & (wlc->stf->hw_rxchain - 1)))
		allowed = FALSE;
	else if (!wlc_stf_rxchain_ishwdef(wlc))
		allowed = FALSE;
#ifdef WL_MIMOPS_CFG
	else if (WLC_MIMOPS_ENAB(wlc->pub))
		allowed = wlc_stf_mimops_check_mrc_overrride(wlc);
#endif /* WL_MIMOPS_CFG */

	return allowed;
}

int
wlc_pm_bcnrx_set(wlc_info_t *wlc, bool enable)
{
	bool val;

	if (!wlc_pm_bcnrx_cap(wlc))
		return BCME_UNSUPPORTED;

	if (!si_iscoreup(wlc->pub->sih))
		return BCME_NOTUP;

	val = (wlc_mhf_get(wlc, MHF3, WLC_BAND_AUTO) & MHF3_PM_BCNRX) ? TRUE : FALSE;
	if (val == enable)
		return 0;

	wlc_suspend_mac_and_wait(wlc);
	wlc_mhf(wlc, MHF3, MHF3_PM_BCNRX, enable ? MHF3_PM_BCNRX : 0, WLC_BAND_ALL);
	wlc_enable_mac(wlc);

	return 0;
}

void
wlc_pm_bcnrx_disable(wlc_info_t *wlc)
{
	int err;

	err = wlc_pm_bcnrx_set(wlc, FALSE);

	/* If it fails because core was down,
	 * don't clear flag so we get another
	 * chance in wlc_init
	 */
	if (!err) {
		WL_ERROR(("Forcing PM bcnrx disable\n"));
		wlc->pub->cmn->_pm_bcnrx = FALSE;
	}
}

void
wlc_pm_bcnrx_init(wlc_info_t *wlc)
{
#if defined(AP) && (defined(RADIO_PWRSAVE) || defined(RXCHAIN_PWRSAVE))
	if (RADIO_PWRSAVE_ENAB(wlc->ap) || RXCHAIN_PWRSAVE_ENAB(wlc->ap)) {
		wlc_pm_bcnrx_disable(wlc);
	} else
#endif /* AP && (RADIO_PWRSAVE || RXCHAIN_PWRSAVE) */
	if (wlc_pm_bcnrx_allowed(wlc))
		(void) wlc_pm_bcnrx_set(wlc, TRUE);
	else
		(void) wlc_pm_bcnrx_set(wlc, FALSE);
}
#endif /* WLPM_BCNRX */

static void
wlc_radio_shutoff_dly_timer_upd(wlc_info_t *wlc)
{
	int idx;
	wlc_bsscfg_t *cfg = NULL;

	wl_del_timer(wlc->wl, wlc->pm2_radio_shutoff_dly_timer);

	if (wlc->pm2_radio_shutoff_dly) {
		/* we are done if no one is in FAST PS mode */
		FOREACH_AS_STA(wlc, idx, cfg) {
			if (cfg->pm->PMenabled && cfg->pm->PM == PM_FAST)
				break;
		}
	}

	if (cfg) {
		WL_PS(("wl%d: wlc_radio_shutoff_dly_timer_upd: arm periodic(%u) radio shutoff "
				"delay timer\n", wlc->pub->unit, wlc->pm2_radio_shutoff_dly));
		wlc->pm2_radio_shutoff_pending = TRUE;
		wl_add_timer(wlc->wl, wlc->pm2_radio_shutoff_dly_timer,
				wlc->pm2_radio_shutoff_dly, FALSE);
	} else {
		if (wlc->pm2_radio_shutoff_pending) {
			/* Cleanup */
			wlc->pm2_radio_shutoff_pending = FALSE;
		}
		wlc_set_wake_ctrl(wlc);
	}

	return;
}

int
wlc_pspoll_timer_upd(wlc_bsscfg_t *cfg, bool allow)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_pm_st_t *pm = cfg->pm;
	int callbacks = 0;

	if (pm->pspoll_timer == NULL) {
		WL_ERROR(("wl%d.%d: %s: Trying to update NULL pspoll timer.\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
		return 0;
	}

	if (!wl_del_timer(wlc->wl, pm->pspoll_timer))
		callbacks ++;

	if (!allow || !cfg->associated || !pm->PMenabled || pm->pspoll_prd == 0)
		return callbacks;

	WL_PS(("wl%d.%d: %s: arm periodic(%u) pspoll timer\n",
	       wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__, pm->pspoll_prd));

	wlc_pm_update_pspoll_state(cfg, TRUE);
	wl_add_timer(wlc->wl, pm->pspoll_timer, pm->pspoll_prd, TRUE);
	return 0;
}

/**
 * Update local PM state(s) and change local PS mode based on tx status. It will also resume any
 * pending work ie. scan that depend on the PM transition, or allow local to doze if there is
 * nothing pending.
 */
void
wlc_update_pmstate(wlc_bsscfg_t *cfg, uint txstatus)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_pm_st_t *pm = cfg->pm;
	bool pmindctd;

	/* we only work on associated BSS STA... */
	if (!BSSCFG_STA(cfg) || ((!cfg->BSS || !cfg->associated) && !BSS_TDLS_ENAB(wlc, cfg)))
		return;

	/* If we get a suppressed frame due to lifetime expiration when waiting for PM indication,
	 * consider it as a PM indication since it indicates a strong interference. Since ucode
	 * doesn't give us PM indication bit in this case, we need to fake it.
	 */

	if (pm->PMpending) {
		WL_PS(("wl%d.%d: PM-MODE: wlc_update_pmstate: txstatus 0x%x\n",
		       wlc->pub->unit, WLC_BSSCFG_IDX(cfg), txstatus));

		if (((txstatus & TX_STATUS_SUPR_MASK) >> TX_STATUS_SUPR_SHIFT) ==
			TX_STATUS_SUPR_EXPTIME) {
			WL_PS(("wl%d.%d: PM-MODE: frame lifetime expired\n",
			       wlc->pub->unit, WLC_BSSCFG_IDX(cfg)));
			txstatus |= TX_STATUS_PMINDCTD;
		}
		if ((txstatus & TX_STATUS_MASK) == TX_STATUS_NO_ACK &&
			SCAN_IN_PROGRESS(wlc->scan)) {
			WL_PS(("wl%d.%d: PM-MODE: frame recv'd no ACK\n",
			       wlc->pub->unit, WLC_BSSCFG_IDX(cfg)));
			txstatus |= TX_STATUS_ACK_RCV|TX_STATUS_PMINDCTD;
		}
	}

	pmindctd = (txstatus & TX_STATUS_PMINDCTD) == TX_STATUS_PMINDCTD;

#ifdef WLTDLS
	if (BSS_TDLS_ENAB(wlc, cfg) && pm->PMpending)
		wlc_tdls_update_pm(wlc->tdls, cfg, txstatus);
#endif // endif

	/* PM change */
	if (pm->priorPMstate != pmindctd) {
		wlc_bss_pm_pending_upd(cfg, txstatus);
		pm->priorPMstate = pmindctd;
	}

	/* if an Ack was received for a PM1 Null tx, stop any retransmits of
	 * PM1 Null at each received infra bss beacon.
	 */
	if (pmindctd && (txstatus & TX_STATUS_ACK_RCV)) {
		pm->pm_immed_retries = 0;
	}

	/* If this evaluates to true, the PM1->PM0 transition was lost and we didn't catch the
	 * transition. So, fake the transition (for upper layers that need to be aware of
	 * PM1->PM0 transitions, and then send the real PM0->PM1 transition
	 */
	if (pm->PMpending && pmindctd) {
		if (SCAN_IN_PROGRESS(wlc->scan) && (!(txstatus & TX_STATUS_ACK_RCV))) {
			WL_PS(("wl%d.%d: PM-MODE: fake a lost PM1->PM0 transition\n",
			       wlc->pub->unit, WLC_BSSCFG_IDX(cfg)));
			wlc_bss_pm_pending_upd(cfg, (txstatus | TX_STATUS_ACK_RCV));
			pm->pm_immed_retries = 0;
		} else if (txstatus & TX_STATUS_ACK_RCV) {
			wlc_bss_pm_pending_upd(cfg, txstatus);
		}
		/* reaching here means our PM1 tx was not acked by the AP */
		else if (pm->ps_resend_mode == PS_RESEND_MODE_BCN_NO_SLEEP ||
			pm->ps_resend_mode == PS_RESEND_MODE_BCN_SLEEP) {
			/* If we have not reached the max immediate PM1 retries,
			 *   Increment the immediate PM1 retry count
			 *   Send a new PM1 Null frame with a new seq#.
			 */
			if (pm->pm_immed_retries < PM_IMMED_RETRIES_MAX) {
				++pm->pm_immed_retries;
				wlc_sendnulldata(wlc, cfg, &cfg->BSSID, 0, 0, PKTPRIO_NON_QOS_HIGH,
					NULL, NULL);
			/* Else we have reached the max# of immediate PM1 resends.
			 * Leave pm_immed_retries at the max retries value to
			 * indicate to the the beacon handler to resend PM1 Null
			 * frames at every DTIM beacon until an Ack is received.
			 */
			} else {
				/* If configured, go to sleep while waiting for the
				 * next beacon.
				 */
				if (pm->ps_resend_mode == PS_RESEND_MODE_BCN_SLEEP) {
					wlc_bss_pm_pending_upd(cfg,
						(txstatus | TX_STATUS_ACK_RCV));
				}
			}
		}
	}

	/* update the PMpending state and move things forward when possible */
	wlc_pm_pending_complete(wlc);
#ifdef WL_MODESW
	if (WLC_MODESW_ENAB(wlc->pub))
		wlc_modesw_pm_pending_complete(wlc->modesw, cfg);
#endif /* WL_MODESW */
} /* wlc_update_pmstate */

static void
wlc_bss_pm_pending_reset(wlc_bsscfg_t *cfg, uint txs)
{
	WL_PS(("wl%d.%d: PM-MODE: fake a lost PM0->PM1 transition\n",
	       cfg->wlc->pub->unit, WLC_BSSCFG_IDX(cfg)));

	wlc_bss_pm_pending_upd(cfg, txs | TX_STATUS_PMINDCTD | TX_STATUS_ACK_RCV);
	wlc_pm_pending_complete(cfg->wlc);
}

/** we are only interested in PM0->PM1 transition! */
void
wlc_pm_pending_complete(wlc_info_t *wlc)
{
	/* Do NOT call wlc_set_wake_ctrl() anywhere inside this function
	 * as wlc_radio_shutoff_dly_timer_upd() at the end will call it once for all.
	 * leakyapstats relies on a clean transitional edge to work properly.
	 */
	int idx;
	wlc_bsscfg_t *cfg;

	if (!wlc->PMpending)
		return;

	WL_PS(("wl%d: PM-MODE: wlc_pm_pending_complete: wlc->PMpending %d\n",
	       wlc->pub->unit, wlc->PMpending));

	FOREACH_STA(wlc, idx, cfg) {
		if (cfg->associated) {
			/* Skip the ones that are not on curr channel */
			if (!wlc_shared_current_chanctxt(wlc, cfg)) {
				continue;
			}
			if (cfg->pm->PMpending)
				return;
		}
#ifdef WLTDLS
		else if (BSS_TDLS_ENAB(wlc, cfg) && (cfg->pm->PMpending))
			return;
#endif // endif
	}

	WL_PS(("wl%d: PM-MODE: PM indications are all finished, "
	       "set wlc->PMpending to FALSE\n", wlc->pub->unit));

	wlc->PMpending = FALSE;

	wlc_sta_pm_pending_complete(wlc);

#ifdef WLRM
	if (WLRM_ENAB(wlc->pub))
		wlc_rm_pm_pending_complete(wlc->rm_info);
#endif // endif

#ifdef WL11K
	if (WL11K_ENAB(wlc->pub))
		wlc_rrm_pm_pending_complete(wlc->rrm_info);
#endif /* WL11K */

	/* non-p2p APs are out there don't shutdown the radio */
	FOREACH_UP_AP(wlc, idx, cfg) {
#ifdef WLP2P
		if (!BSS_P2P_ENAB(wlc, cfg))
#endif // endif
		{
			return;
		}
	}
	wlc_radio_shutoff_dly_timer_upd(wlc);
} /* wlc_pm_pending_complete */

/** we are only interested in PM0->PM1 transition! */
static void
wlc_bss_pm_pending_upd(wlc_bsscfg_t *cfg, uint txstatus)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_pm_st_t *pm = cfg->pm;
	bool pmindctd = (txstatus & TX_STATUS_PMINDCTD) == TX_STATUS_PMINDCTD;

	/* Get rid of warning */
	(void)wlc;

	WL_PS(("wl%d.%d: PM-MODE: wlc_bss_pm_pending_upd: PMpending %d pmindctd %d\n",
	       wlc->pub->unit, WLC_BSSCFG_IDX(cfg), pm->PMpending, pmindctd));

	if (!pm->PMpending)
		return;

	if (pm->PMblocked || (txstatus & TX_STATUS_ACK_RCV))
		wlc_set_pmpending(cfg, !pmindctd);

	if (pm->PMpending)
		return;

	/* when done waiting PM0->PM1 transition, let core go to sleep */
	WL_RTDC(wlc, "wlc_bss_pm_pending_upd: PMep=%02u PMaw=%02u",
	        (pm->PMenabled ? 10 : 0) | pm->PMpending,
	        (PS_ALLOWED(cfg) ? 10 : 0) | STAY_AWAKE(wlc));
	WL_PS(("wl%d.%d: PM-MODE: PS_ALLOWED() %d, %s\n",
	       wlc->pub->unit, WLC_BSSCFG_IDX(cfg), PS_ALLOWED(cfg),
	       STAY_AWAKE(wlc)?"but remaining awake":"going to sleep"));

	if (PM2_RCV_DUR_ENAB(cfg) && pm->PM == PM_FAST) {
		if (pm->pm2_rcv_state == PM2RD_WAIT_RTS_ACK) {
			pm->pm2_rcv_state = PM2RD_WAIT_BCN;
			WL_RTDC(wlc, "pm2_rcv_state=PM2RD_WAIT_BCN", 0, 0);
		}
	}
} /* wlc_bss_pm_pending_upd */

void
wlc_reset_pmstate(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_pm_st_t *pm = cfg->pm;

	(void)wlc;

	WL_PS(("wl%d.%d: PM-MODE: wlc_reset_pmstate:\n",
		wlc->pub->unit, WLC_BSSCFG_IDX(cfg)));

	wlc_set_pmawakebcn(cfg, FALSE);

	if (pm->PMpending)
		wlc_update_pmstate(cfg, TX_STATUS_BE);

	wlc_set_pspoll(cfg, FALSE);
	wlc_set_apsd_stausp(cfg, FALSE);
	wlc_set_uatbtt(cfg, FALSE);
	pm->WME_PM_blocked = FALSE;

#ifdef STA
	/* If there are no STAs for watchdog on tbtt, disable interrupts */
	if (!WLC_WATCHDOG_TBTT(wlc)) {
		wlc_watchdog_disable_ucode_sleep_intr(wlc);
	}
#endif /* STA */

	/* clear all PMblocked bits */
	pm->PMblocked = 0;
	pm->PMenabled = (pm->PM != PM_OFF);
	pm->PMenabledModuleId = 0;
	wlc_set_ps_ctrl(cfg);
#ifdef BCMPCIEDEV
	/* Set latency tolerance state if no bsscfg up, set LTR sleep */
	if (BCMPCIEDEV_ENAB())
		wlc_cfg_set_pmstate_upd(cfg, TRUE);
#endif // endif
}

bool
wlc_bssid_is_current(wlc_bsscfg_t *cfg, struct ether_addr *bssid)
{
	return WLC_IS_CURRENT_BSSID(cfg, bssid);
}

bool BCMFASTPATH
wlc_bss_connected(wlc_bsscfg_t *cfg)
{
	return WLC_BSS_CONNECTED(cfg);
}

uint8
wlc_stas_connected(wlc_info_t *wlc)
{
	int idx;
	wlc_bsscfg_t *cfg;
	uint8 stas_connected = 0;

	/* count STAs currently still talking with their associated APs */
	FOREACH_AS_STA(wlc, idx, cfg) {
		if (WLC_BSS_CONNECTED(cfg))
			stas_connected ++;
	}

	return stas_connected;
}

bool
wlc_ibss_active(wlc_info_t *wlc)
{
	int i;
	wlc_bsscfg_t *cfg;

	/* IBSS link monitor ... */
	FOREACH_AS_STA(wlc, i, cfg) {
		if (BSSCFG_IBSS(cfg)) {
			if (BSSCFG_SLOTTED_BSS(cfg)) {
				return TRUE;
			} else {
				wlc_roam_t *roam = cfg->roam;
				if (!roam->bcns_lost) {
					return TRUE;
				}
			}
		}
	}
	return FALSE;
}

bool
wlc_non_slotted_ibss_active(wlc_info_t *wlc)
{
	int i;
	wlc_bsscfg_t *cfg;

	/* IBSS link monitor ... */
	FOREACH_AS_STA(wlc, i, cfg) {
		if (BSSCFG_IBSS(cfg)) {
			if (BSSCFG_SLOTTED_BSS(cfg)) {
				return FALSE;
			} else {
				wlc_roam_t *roam = cfg->roam;
				if (!roam->bcns_lost) {
					return TRUE;
				}
			}
		}
	}
	return FALSE;
}
bool
wlc_stas_active(wlc_info_t *wlc)
{
	/* check for IBSS state first */
	if (wlc->ibss_bsscfgs)
		return wlc_ibss_active(wlc);
	else
		return wlc->stas_connected;
}

bool
wlc_portopen(wlc_bsscfg_t *cfg)
{
	return WLC_PORTOPEN(cfg);
}

#endif /* STA */

/**
 * Can be called in either STA or AP context. In STA mode, we received a Channel Switch
 * announcement. In AP, we detected radar and need to move off channel.
 */
void
wlc_do_chanswitch(wlc_bsscfg_t *cfg, chanspec_t newchspec)
{
	wlc_info_t *wlc = cfg->wlc;
	bool band_chg = FALSE;
	bool bw_chg = FALSE;
	bool from_radar = FALSE;
	bool to_radar = FALSE;
#if defined(BCMDBG) || defined(WLMSG_INFORM)
	char chanbuf[CHANSPEC_STR_LEN];

	WL_INFORM(("wl%d: %s: new chanspec %s\n", wlc->pub->unit, __FUNCTION__,
		wf_chspec_ntoa_ex(newchspec, chanbuf)));
#endif /* BCMDBG || WLMSG_INFORM */

	if (!wlc_valid_chanspec_db(wlc->cmi, newchspec)) {
		WL_REGULATORY(("%s: Received invalid channel - ignoring\n", __FUNCTION__));
		/* something needs to be done here ?? */
		return;
	}
	/* With 11H AP active:
	 * NOTE: Non-Radar (NR), Radar (R).
	 * 1. oldchanspec = R, newchanspec = R
	 * Action:
	 * 1. Turn radar_detect ON affter channel switch.
	 *
	 * 2. oldchanspec = R, newchanspec = NR
	 * Action:
	 *	1. Turn radar OFF after channel switch.
	 *
	 * 3. oldchanspec = NR, newchanspec = R
	 * Action:
	 *	1. Turn radar ON After switch to new channel.
	 *
	 * Turning on radar in case 1 and 3 are take care in wlc_restart_ap().
	 * Turning off radar in case of 2 is done after wlc_restart_ap().
	 */
	if (WL11H_ENAB(wlc) && (AP_ACTIVE(wlc) ||
#ifdef SLAVE_RADAR
		STA_ACTIVE(wlc) ||
#endif // endif
		FALSE)) {
		from_radar = (wlc_radar_chanspec(wlc->cmi, cfg->current_bss->chanspec) == TRUE);
		to_radar = (wlc_radar_chanspec(wlc->cmi, newchspec));
	}

	/* evaluate band or bandwidth change here before wlc->home_chanspec gets changed */
	if (CHSPEC_WLCBANDUNIT(wlc->home_chanspec) != CHSPEC_WLCBANDUNIT(newchspec))
		band_chg = TRUE;
	if (CHSPEC_BW(wlc->home_chanspec) != CHSPEC_BW(newchspec))
		bw_chg = TRUE;

	/* mark as quiet only when opreating on primary interface in MBSS/VAP */
	if (WL11H_ENAB(wlc) && BSSCFG_IS_PRIMARY(cfg)) {
		/* restore the current channel's quiet bit unless in EU */
		if ((wlc_radar_chanspec(wlc->cmi, wlc->home_chanspec) ||
				wlc_restricted_chanspec(wlc->cmi, wlc->home_chanspec)) &&
				!wlc->is_edcrs_eu) {
			wlc_set_quiet_chanspec_exclude(wlc->cmi, wlc->home_chanspec, newchspec);
		}
	}

#ifdef WLMCHAN
	if (MCHAN_ENAB(wlc->pub) &&
	    !BSS_TDLS_ENAB(wlc, cfg)) {
		WL_MCHAN(("wl%d.%d: %s: chansw command issued, change chanctx to 0x%x\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__, newchspec));

		/* Make sure cfg->current_bss and cfg->target_bss chanspec changed also */
		cfg->target_bss->chanspec = newchspec;
		wlc_bsscfg_set_current_bss_chan(cfg, newchspec);
	}
#endif // endif
	WL_REGULATORY(("wl%d.%d: %s: switching to chanspec %s\n",
		wlc->pub->unit, cfg->_idx, __FUNCTION__, wf_chspec_ntoa_ex(newchspec, chanbuf)));

	if (BSSCFG_SLOTTED_BSS(cfg) ||
		BSS_TDLS_ENAB(wlc, cfg) || BSS_PROXD_ENAB(wlc, cfg)) {
		WL_ERROR(("wl%d.%d: %s: Not supported yet\n",
			wlc->pub->unit, cfg->_idx, __FUNCTION__));
		return;
	}

	/* Skip for proxySTA bsscfg */
	if (!BSSCFG_PSTA(wlc->cfg)) { /* checking primary cfg */
		wlc_set_home_chanspec(wlc, newchspec);
#ifdef STA
		/* change in chanspec, re-register with the scheduler
		 * with new chanspec
		 */
		if (BSSCFG_STA(wlc->cfg)) { /* checking primary cfg */
			wlc_sta_timeslot_register(wlc->cfg); /* checking primary cfg */
			/* before doing bsscfg up operation, enable sta cfg
			 * if down
			 */
			if (!wlc->cfg->enable) {
				wlc_bsscfg_enable(wlc, wlc->cfg);
			}
		}
#endif /* STA */
		/* Make AP and MCHAN to use new chanspec */
		wlc_bsscfg_up(wlc, cfg);
	}

	if (BSSCFG_AP(cfg)) {
		wlc_suspend_mac_and_wait(wlc);
		wlc_bss_update_beacon(wlc, cfg);
		wlc_bss_update_probe_resp(wlc, cfg, FALSE);
		wlc_enable_mac(wlc);
	}

	WL_REGULATORY(("wl%d.%d: %s: switched to chanspec %s\n",
		wlc->pub->unit, cfg->_idx, __FUNCTION__, wf_chspec_ntoa_ex(newchspec, chanbuf)));

	if (band_chg) {
		wlc_scb_update_band_for_cfg(wlc, cfg, newchspec);
	}

	if (AP_ACTIVE(wlc)) {
		/* set newchspec as default_bss chanspec
		* This will allow AP to be started on this chanspec
		*/
		wlc->default_bss->chanspec = newchspec;
	}

	if (band_chg || to_radar) {
		/* in case of band switch, restart ap */
		if (AP_ACTIVE(wlc))
			wlc_restart_ap(wlc->ap);
	}

	if (WLDFS_ENAB(wlc->pub) && (from_radar) && (!to_radar)) {
#if defined(SLAVE_RADAR)
		if ((!wlc_is_ap_interface_up(wlc)) && WL11H_STA_ENAB(wlc) &&
			wlc_dfs_get_radar(wlc->dfs)) {
			cfg->pm->pm_modechangedisabled = FALSE;
			wlc_set_pm_mode(wlc, cfg->pm->pm_oldvalue, cfg);
		}
#endif /* STA && SLAVE_RADAR */
		/* If we are moving out of a radar channel, then stop CAC statemachine */
		wlc_set_dfs_cacstate(wlc->dfs, OFF, cfg);
	}

	if (bw_chg)
		/* force a rate selection init. */
		wlc_scb_ratesel_init_bss(wlc, cfg);
	else
		/* reset the rate sel state to init state */
		wlc_scb_ratesel_rfbr_bss(wlc, cfg);
#ifdef SLAVE_RADAR
	if (WLDFS_ENAB(wlc->pub) && (!from_radar) && (to_radar)) {
		if ((!wlc_is_ap_interface_up(wlc)) && WL11H_STA_ENAB(wlc) &&
			wlc_dfs_get_radar(wlc->dfs)) {
			if (wlc_cac_is_clr_chanspec(wlc->dfs, newchspec)) {
				if (!(band_chg || bw_chg || ETHER_ISNULLADDR(&cfg->BSSID))) {
					wlc_set_dfs_cacstate(wlc->dfs, ON, cfg);
				}
			}
			cfg->pm->pm_oldvalue = cfg->pm->PM;
			wlc_set_pm_mode(wlc, PM_OFF, cfg);
			cfg->pm->pm_modechangedisabled = TRUE;
			wlc->mpc = FALSE;
			wlc_radio_mpc_upd(wlc);
		}
	}
#endif /* SLAVE_RADAR */
	/* perform a reassoc procedure to sync up everything with the AP.
	 * 1. If band or BW change.
	 * 2. If for some reason (bcn_lost etc) STA disconnected
	 * with AP (cfg->BSSID is cleared when this happens).
	*/
	if (band_chg || bw_chg || ETHER_ISNULLADDR(&cfg->BSSID)) {
#ifdef STA
		if (BSSCFG_STA(cfg) && cfg->BSS) {
			if (!BSSCFG_PSTA(cfg)) {
				/* call wlc_reassoc at end of the lengthy STA CLBK func to prevent
				 * reducing the SCAN dwell time duration.
				 */
				cfg->assoc->flags |= AS_F_DO_CHANSWITCH;
			} else {
				wl_reassoc_params_t assoc_params;

				memset(&assoc_params, 0, sizeof(wl_reassoc_params_t));

				/* If we got disconnected by now try
				 * non-directeed scan.
				 */
				assoc_params.bssid = (ETHER_ISNULLADDR(&cfg->BSSID)) ?
					ether_bcast:cfg->BSSID;
				assoc_params.chanspec_num = 1;
				assoc_params.chanspec_list[0] = newchspec;
				wlc_reassoc(cfg, &assoc_params);
			}
		}
#endif /* STA */
	}
#ifdef WL_MIMOPS_CFG
	if (WLC_MIMOPS_ENAB(wlc->pub)) {
		if (BSSCFG_STA(cfg) && cfg->BSS) {
			wlc_stf_mimops_handle_csa_chanspec(cfg);
		}
	}
#endif /* WL_MIMOPS_CFG */

	if (WLC_BCNTRIM_ENAB(wlc->pub)) {
		if (BSSCFG_STA(cfg) && cfg->BSS) {
			wlc_stf_bcntrim_handle_csa_chanspec(wlc->bcntrim, cfg);
		}
	}

	return;
} /* wlc_do_chanswitch */

#ifdef STA
/** Update beacon listen interval in shared memory */
void
wlc_bcn_li_upd(wlc_info_t *wlc)
{
	/* In the shared memory lower 8-bit value instructs ucode to listen every Nth beacon
	 * and upper 8-bit value instructs ucode to listen every Nth dtim beacon. When both are
	 * non-zero upper 8-bit value takes precedence as listen interval. When both are 0
	 * ucode has its default behavior, which is to listen every dtim beacon.
	 *
	 * XXX Do we need to stop the MAC before changing these values to avoid race?
	 */
	/* wake up every DTIM is the default */
	if (wlc->bcn_li_dtim == 1)
		wlc_write_shm(wlc, M_BCN_LI(wlc), 0);
	else
		wlc_write_shm(wlc, M_BCN_LI(wlc), (wlc->bcn_li_dtim << 8) | wlc->bcn_li_bcn);

	/* BSS state word of p2p block is introduced to identify the station running
	 * multi-DTIM feature.
	 * At present support is enabled to run only for primary cfg.
	 * Used in rsdb case when primary cfg of wlcs are sta i.e dual sta case.
	 */
#ifdef WLMCNX
	if (MCNX_ENAB(wlc->pub)) {
		if (wlc->bcn_li_dtim == 1) {
			wlc_mcnx_multidtim_upd(wlc->mcnx, wlc->cfg, FALSE);
		} else {
			wlc_mcnx_multidtim_upd(wlc->mcnx, wlc->cfg, TRUE);
		}
	}
#endif /* WLMCNX */
}
#endif /* STA */

void
wlc_enable_probe_req(wlc_info_t *wlc, uint32 mask, uint32 val)
{

	wlc->prb_req_enable_mask = (wlc->prb_req_enable_mask & ~mask) | (val & mask);

	if (!wlc->pub->up) {
#ifndef ATE_BUILD
		WL_ERROR(("wl%d: %s: state down, deferring setting of host flags\n",
			wlc->pub->unit, __FUNCTION__));
#endif // endif
		return;
	}

	if (wlc->prb_req_enable_mask)
		wlc_mhf(wlc, MHF5, MHF5_SUPPRESS_PRB_REQ, 0, WLC_BAND_ALL);
	else
		wlc_mhf(wlc, MHF5, MHF5_SUPPRESS_PRB_REQ, MHF5_SUPPRESS_PRB_REQ, WLC_BAND_ALL);
}

static void
wlc_config_ucode_probe_resp(wlc_info_t *wlc)
{
	if (wlc->prb_rsp_disable_mask) {
		/* disable ucode ProbeResp */
		wlc_mhf(wlc, MHF2, MHF2_DISABLE_PRB_RESP,
			MHF2_DISABLE_PRB_RESP, WLC_BAND_ALL);
	} else {
		/* enable ucode ProbeResp */
		wlc_mhf(wlc, MHF2, MHF2_DISABLE_PRB_RESP,
			0, WLC_BAND_ALL);
	}
}

void
wlc_disable_probe_resp(wlc_info_t *wlc, uint32 mask, uint32 val)
{
	bool disable = (wlc->prb_rsp_disable_mask != 0);
	wlc->prb_rsp_disable_mask = (wlc->prb_rsp_disable_mask & ~mask) | (val & mask);

	/* deferring setting in wlc_up */
	if (!wlc->pub->up) {
		return;
	}

	if (disable != (wlc->prb_rsp_disable_mask != 0))
		wlc_config_ucode_probe_resp(wlc);
}

chanspec_t
wlc_get_cur_wider_chanspec(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	wlc_bsscfg_t	*cfg;
	int idx;
	chanspec_t	chspec = bsscfg->current_bss->chanspec;

	FOREACH_BSS(wlc, idx, cfg) {
		if (cfg->associated || (BSSCFG_AP(cfg) && cfg->up) ||
#ifdef WLTDLS
		    (BSS_TDLS_ENAB(wlc, cfg) && wlc_tdls_scb_associated(wlc->tdls, cfg)) ||
#endif /* WLTDLS */
			FALSE) {

			if (wf_chspec_ctlchan(chspec) !=
				wf_chspec_ctlchan(cfg->current_bss->chanspec)) {
				continue;
			}

			/* check for highest operating bandwidth */
			if (CHSPEC_BW_GT(cfg->current_bss->chanspec, CHSPEC_BW(chspec))) {
				chspec = cfg->current_bss->chanspec;
			}
		}
	}
	return chspec;
}

bool
wlc_is_shared_chanspec(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	int idx;
	wlc_bsscfg_t *cfg;
	FOREACH_BSS(wlc, idx, cfg) {
		if (cfg == bsscfg) {
			continue;
		}
		if (wlc_shared_chanctxt_on_chan(wlc, cfg, bsscfg->current_bss->chanspec)) {
			return TRUE;
		}
	}
	return FALSE;
}

void
wlc_update_bandwidth(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	chanspec_t new_chspec)
{
	chanspec_t old_chanspec;
#if defined(PHYCAL_CACHING)
	wlc_phy_t * bpi = wlc->hw->band->pi;
#endif // endif

	if (!new_chspec || new_chspec == INVCHANSPEC) {
		return;
	}
#ifdef WL_MIMOPS_CFG
	if (WLC_MIMOPS_ENAB(wlc->pub)) {
		if (wlc_stf_mimops_handle_bw_upd(wlc, bsscfg, new_chspec))
			return;
	}
#endif /* WL_MIMOPS_CFG */

	old_chanspec = bsscfg->current_bss->chanspec;

	wlc_bsscfg_set_current_bss_chan(bsscfg, new_chspec);

	if (!wlc_has_chanctxt(wlc, bsscfg)) {
		WL_ERROR(("wl%d.%d: %s: bsscfg is without chan context \r\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), __FUNCTION__));
		return;
	}
#if defined(PHYCAL_CACHING)
	/* Delete existing channel calibration context if required */
	if (!wlc_is_shared_chanspec(wlc, bsscfg)) {
		phy_chanmgr_destroy_ctx((phy_info_t *) bpi, bsscfg->current_bss->chanspec);
	}
#endif // endif

	new_chspec = wlc_get_cur_wider_chanspec(wlc, bsscfg);
#if defined(PHYCAL_CACHING)
	/* Add channel calibration context */
	/* Create and set last cal time so that cal will be triggered right away */
	if (phy_chanmgr_create_ctx((phy_info_t *) bpi, new_chspec) == BCME_OK) {
		wlc_phy_update_chctx_glacial_time(bpi, new_chspec);
	}
#endif // endif

	/* if current phy chanspec is not the higest oper. bw
	 * update the phy chanspec
	 */
	if (wlc->chanspec != new_chspec) {
		wlc->home_chanspec = new_chspec;

		if (BSSCFG_AP(bsscfg)) {
			wlc_ap_update_bw(wlc, bsscfg, old_chanspec, new_chspec);
		} else if (BSSCFG_STA(bsscfg)) {
			if (BSSCFG_SLOTTED_BSS(bsscfg)) {
				return;
			}
			wlc_sta_update_bw(wlc, bsscfg, old_chanspec, new_chspec);
		} else {
			ASSERT(0);
		}
#ifndef WLMCHAN
#ifdef WL_MODESW
		if (WLC_MODESW_ENAB(wlc->pub)) {
			if (BSSCFG_IS_MODESW_BWSW(bsscfg))
			{
				phy_chanmgr_create_ctx((phy_info_t *) wlc->pi, new_chspec);
			}
		}
#endif /* WL_MODESW */
#endif /* !WLMCHAN */

		/* Updated the PHY chanspec, calibrate the channel */
		wlc_full_phy_cal(wlc, bsscfg, PHY_PERICAL_UP_BSS);
#ifdef WL11K
		wlc_rrm_stat_chanwidthsw_counter(wlc, bsscfg);
#endif /* WL11K */
	}

	wlc_scb_ratesel_init_bss(wlc, bsscfg);
} /* wlc_update_bandwidth */

/* Check if a bandwidth is valid for use with current cfg
 * wlc - pointer to wlc_info_t
 * cfg - pointer to wlc_bsscfg_t
 * bandunit - either BAND_5G_INDEX or BAND_2G_INDEX
 * Return true if a given bw for bandunit is valid (as per cfg, capability, mode, locale, ...)
 */
bool
wlc_is_valid_bw(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint8 bandunit, uint16 bw)
{
	uint16 locale_flags;
	uint16 bandtype = 0;
	wlcband_t * band;

	ASSERT(wlc != NULL);
	ASSERT(cfg != NULL);

	if (bandunit == BAND_2G_INDEX) {
		bandtype = WLC_BAND_2G;
	} else if (bandunit == BAND_5G_INDEX) {
		bandtype = WLC_BAND_5G;
	} else {
		ASSERT(0);
	}

	band = wlc->bandstate[bandunit];

	WL_INFORM(("wl%d: %s: given bandunit: %d, bw: 0x%2x", wlc->pub->unit, __FUNCTION__,
			bandunit, bw));

	locale_flags = wlc_channel_locale_flags_in_band(wlc->cmi, bandunit);

	switch (bw) {
#ifdef WL11AC_80P80
		case WL_CHANSPEC_BW_8080:
			return (bandunit == BAND_5G_INDEX &&
					VHT_ENAB_BAND(wlc->pub, bandtype) &&
					!(locale_flags & WLC_NO_160MHZ) &&
					WL_BW_CAP_160MHZ(band->bw_cap));
#endif /* WL11AC_80P80 */
		case WL_CHANSPEC_BW_160:
			return (bandunit == BAND_5G_INDEX &&
					VHT_ENAB_BAND(wlc->pub, bandtype) &&
					!(locale_flags & WLC_NO_160MHZ) &&
					WL_BW_CAP_160MHZ(band->bw_cap));
		case WL_CHANSPEC_BW_80:
			return (bandunit == BAND_5G_INDEX &&
					VHT_ENAB_BAND(wlc->pub, bandtype) &&
					!(locale_flags & WLC_NO_80MHZ) &&
					WL_BW_CAP_80MHZ(band->bw_cap));
		case WL_CHANSPEC_BW_40:
			return (N_ENAB(wlc->pub) &&
					!(locale_flags & WLC_NO_40MHZ) &&
					WL_BW_CAP_40MHZ(band->bw_cap) &&
					/* if on 2G, ensure 40 MHz tolerance */
					!(BAND_2G(bandtype) && WLC_INTOL40_DET(wlc, cfg)));
		case WL_CHANSPEC_BW_20:
			/* no 20MHz bw checks yet; always supported on all bands and configs */
			return TRUE;
	}

	return FALSE;
}
/**
 * Find the maximum bandwidth chanspec that we can use from given chanspec.
 * If chanspec is not valid, downgrade the bandwidth until valid chanspec is found.
 * Return the valid chsanspec if found. Else return INVCHANSPEC.
 * XXX: reuse this function in wlc_prot_n.c/wlc_csa.c
 */
chanspec_t
wlc_max_valid_bw_chanspec(wlc_info_t *wlc, wlcband_t *band, wlc_bsscfg_t *cfg, chanspec_t chanspec)
{
	WL_INFORM(("wl%d: %s: given chanspec: 0x%x", wlc->pub->unit, __FUNCTION__, chanspec));

	/* Sanitize user setting for 80MHz against current settings
	 * Reduce an 80MHz chanspec to 40MHz if needed.
	 */
	if (CHSPEC_IS8080_UNCOND(chanspec) &&
		!VALID_8080CHANSPEC(wlc, chanspec)) {
		/* select the 80MHz primary channel in case80 is allowed */
		chanspec = wf_chspec_primary80_chspec(chanspec);
	}
	if (CHSPEC_IS160_UNCOND(chanspec) &&
		!VALID_160CHANSPEC(wlc, chanspec)) {
		/* select the 80MHz primary channel in case80 is allowed */
		chanspec = wf_chspec_primary80_chspec(chanspec);
	}
	if (CHSPEC_IS80_UNCOND(chanspec) &&
	    !VALID_80CHANSPEC(wlc, chanspec)) {
		/* select the 40MHz primary channel in case 40 is allowed */
		chanspec = wf_chspec_primary40_chspec(chanspec);
	}

	/*
	 * Sanitize chanspec for 40MHz against current settings
	 * (not in NMODE or the locale does not allow 40MHz
	 * or the band is not configured for 40MHz operation).
	 * Note that the unconditional version of the CHSPEC_IS40 is used so that
	 * code compiled without MIMO support will still recognize and convert
	 * a 40MHz chanspec.
	 */
	if (CHSPEC_IS40_UNCOND(chanspec) &&
	    (!N_ENAB(wlc->pub) ||
	     (wlc_channel_locale_flags_in_band(wlc->cmi, band->bandunit) &
	      WLC_NO_40MHZ) || !WL_BW_CAP_40MHZ(band->bw_cap) ||
	     (BAND_2G(band->bandtype) && WLC_INTOL40_DET(wlc, cfg)) ||
	     !wlc_valid_chanspec_db(wlc->cmi, chanspec)))
	{
		chanspec = wf_chspec_ctlchspec(chanspec);
	}

	if (!WLC_CNTRY_DEFAULT_ENAB(wlc) && !wlc_valid_chanspec_db(wlc->cmi, chanspec)) {
		WL_ERROR(("wl%d: %s: chanspec 0x%x is invalid",
			wlc->pub->unit, __FUNCTION__, chanspec));
		return INVCHANSPEC;
	}

	WL_INFORM(("wl%d: %s: returning chanspec 0x%x", wlc->pub->unit, __FUNCTION__, chanspec));
	return chanspec;
} /* wlc_max_valid_bw_chanspec */

#ifdef WL_CLIENT_SAE
static int wlc_sae_assoc_change_state(wlc_bsscfg_t *cfg, struct dot11_auth *auth_frm)
{
		int auth_alg = ltoh16(auth_frm->alg);
		uint auth_seq = ltoh16(auth_frm->seq);
		int assoc_status = 0;

		/* Sae External Auth requesting should not be
		* in Idle State.Instead it should be in
		* AS_SENT_AUTH_UP/AS_START_EXT_AUTH/other Auth states
		*/
		if (cfg->assoc->state == AS_IDLE)
			return BCME_ERROR;

		if (auth_alg != DOT11_SAE)
			return BCME_ERROR;

		if (auth_seq > WL_SAE_CONFIRM) {
			WL_ERROR(("%s: Wrong Sae auth Sequence %d\n",
					__FUNCTION__, auth_seq));
			return BCME_ERROR;
		}
		assoc_status = (auth_seq == WL_SAE_COMMIT) ?
			AS_SENT_AUTH_1:AS_SENT_AUTH_3;

		wlc_assoc_change_state(cfg, assoc_status);

		return BCME_OK;
}
#endif /* WL_CLIENT_SAE */

static int wlc_auth_get_bssid(wlc_bsscfg_t *cfg, struct ether_addr *bssid,
		struct ether_addr *target_mac)
{
	if (BSSCFG_AP(cfg) && (!(ETHER_ISNULLADDR(&cfg->BSSID)))) {
		memcpy((void *)bssid, &cfg->BSSID, ETHER_ADDR_LEN);
	} else if (BSSCFG_STA(cfg) && (!(ETHER_ISNULLADDR(target_mac)))) {
		memcpy((void *)bssid, target_mac, ETHER_ADDR_LEN);
	} else
		return BCME_ERROR;

	return BCME_OK;
}

static int wlc_send_mgmt_assoc(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	dot11_management_header_t *mgmt, uint32 len)
{
	void *pkt;
	int ret = 0;
	struct ether_addr target_mac, bssid;
	struct scb *scb = NULL;
	uint8 *assoc_frm = NULL;
	struct dot11_assoc_resp *assoc_resp;
#if defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* WLMSG_ASSOC */
	wlc_iem_ft_cbparm_t ftcbparm;
	wlc_iem_cbparm_t cbparm;
	wlc_bss_info_t *current_bss;
	wlc_rateset_t sup_rates, ext_rates;
	uint rsp_len = 0;
	uint16 type;

	if (!mgmt)
		return BCME_ERROR;

	type = (ltoh16(mgmt->fc) & FC_KIND_MASK);
	current_bss = bsscfg->current_bss;
	memcpy(&target_mac, &mgmt->da, ETHER_ADDR_LEN);

#if defined(WLMSG_ASSOC)
	WL_ASSOC(("wl%d: %s: assoc resp(%d) frame for MAC %s\n",
			wlc->pub->unit, __FUNCTION__, type,
			bcm_ether_ntoa((struct ether_addr *)&mgmt->da,
			eabuf)));
#endif /* WLMSG_ASSOC */

	if (!BSSCFG_AP(bsscfg)) {
		WL_ERROR(("wl%d: %s: Not AP, Drop assoc resp frame\n",
				wlc->pub->unit, __FUNCTION__));
		return BCME_NOTAP;
	}

	if (wlc_auth_get_bssid(bsscfg, &bssid, &target_mac) != BCME_OK) {
		WL_ERROR(("wl%d: %s: NULL bssid, Drop assoc resp frame\n",
				wlc->pub->unit, __FUNCTION__));
		return BCME_BADADDR;
	}

	if (len < (DOT11_MGMT_HDR_LEN + DOT11_ASSOC_RESP_FIXED_LEN)) {
		WL_ERROR(("wl%d: %s: assoc resp frame is too small\n",
				wlc->pub->unit, __FUNCTION__));
		return BCME_BUFTOOSHORT;
	}

	scb = wlc_scbfind(wlc, bsscfg, &target_mac);
	if (scb == NULL) {
#if defined(WLMSG_ASSOC)
		WL_ASSOC(("wl%d: %s: assoc resp frame for unknown MAC %s\n",
				wlc->pub->unit, __FUNCTION__,
				bcm_ether_ntoa((struct ether_addr *)&mgmt->da,
				eabuf)));
#endif /* WLMSG_ASSOC */
		return BCME_BADADDR;
	}

	/* create the supported rates and extended supported rates elts */
	bzero(&sup_rates, sizeof(wlc_rateset_t));
	bzero(&ext_rates, sizeof(wlc_rateset_t));
	/* check for a supported rates override */
	if (wlc->sup_rates_override.count > 0)
		bcopy(&wlc->sup_rates_override, &sup_rates, sizeof(wlc_rateset_t));
	wlc_rateset_elts(wlc, bsscfg, &current_bss->rateset, &sup_rates, &ext_rates);

	rsp_len = DOT11_ASSOC_RESP_FIXED_LEN;
	/* prepare IE mgmt calls */
	bzero(&ftcbparm, sizeof(ftcbparm));
	ftcbparm.assocresp.mcs = scb->rateset.mcs;
	ftcbparm.assocresp.scb = scb;
	ftcbparm.assocresp.sup = &sup_rates;
	ftcbparm.assocresp.ext = &ext_rates;
	bzero(&cbparm, sizeof(cbparm));
	cbparm.ft = &ftcbparm;
	cbparm.bandunit = CHSPEC_WLCBANDUNIT(current_bss->chanspec);
	cbparm.ht = SCB_HT_CAP(scb);
#ifdef WL11AC
	cbparm.vht = SCB_VHT_CAP(scb);
#endif // endif
#ifdef WL11AX
	cbparm.he = SCB_HE_CAP(scb);
#endif // endif

	/* calculate IEs' length */
	rsp_len += wlc_iem_calc_len(wlc->iemi, bsscfg, type, NULL, &cbparm);
	/* alloc a packet */
	if ((pkt = wlc_frame_get_mgmt(wlc, type, &target_mac, &bsscfg->cur_etheraddr,
			&bssid, rsp_len,
			&assoc_frm)) == NULL) {
		WL_ERROR(("wl%d: %s:  Sending assoc resp frame failed \n",
				wlc->pub->unit, __FUNCTION__));
		return BCME_ERROR;
	}
	memcpy(assoc_frm, (uint8 *)mgmt + DOT11_MGMT_HDR_LEN, len - DOT11_MGMT_HDR_LEN);
	assoc_resp = (struct dot11_assoc_resp *) assoc_frm;
	assoc_frm += DOT11_ASSOC_RESP_FIXED_LEN;
	rsp_len -= DOT11_ASSOC_RESP_FIXED_LEN;

	/* write IEs in the frame */
	if (wlc_iem_build_frame(wlc->iemi, bsscfg, type, NULL, &cbparm,
		assoc_frm, rsp_len) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_build_frame failed\n",
				wlc->pub->unit, __FUNCTION__));
		return BCME_ERROR;
	}

	ret = wlc_sendmgmt(wlc, pkt, scb->bsscfg->wlcif->qi, scb);

	if (assoc_resp->status == DOT11_SC_SUCCESS) {
		if (bsscfg->WPA_auth & WPA3_AUTH_SAE_PSK) {
			/* Set SCB to associated */
			WL_ASSOC(("wl%d: %s: Set scb to ASSOCIATED\n", wlc->pub->unit,
				__FUNCTION__));
			wlc_scb_clearstatebit(wlc, scb, PENDING_ASSOC);
			scb->aid = assoc_resp->aid;
			scb->assoctime = wlc->pub->now;
			wlc_scb_setstatebit(wlc, scb, ASSOCIATED);

			/* send WLC_E_REASSOC_IND/WLC_E_ASSOC_IND to add STA in DHD.
			 * Cfg80211 layer should ignore this event
			 */
			wlc_bss_mac_event(wlc, bsscfg, type == FC_REASSOC_REQ ? WLC_E_REASSOC_IND :
					WLC_E_ASSOC_IND, &target_mac, 0, 0, scb->auth_alg,
					NULL, 0);
		}
	}
	else {
		/* Reset security parameters for scb */
		scb->WPA_auth = WPA_AUTH_DISABLED;
		scb->wsec = 0;
#ifdef MFP
		if (WLC_MFP_ENAB(wlc->pub) && SCB_MFP(scb)) {
			SCB_MFP_DISABLE(scb);
		}
#endif /* MFP */
	}
	return ret;
}

static int wlc_send_mgmt_auth(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	dot11_management_header_t *mgmt, uint32 len)
{
		void *pkt;
		int ret = 0;
		struct ether_addr target_mac, bssid;
		struct scb *scb = NULL;
		uint8 *auth_frm = NULL;
#ifdef WL_SAE
		struct dot11_auth *auth;
#endif /* WL_SAE */
#if defined(WLMSG_ASSOC)
		char eabuf[ETHER_ADDR_STR_LEN];
#endif /* WLMSG_ASSOC */

		if (!mgmt)
			return BCME_ERROR;

		memcpy(&target_mac, &mgmt->da, ETHER_ADDR_LEN);

#if defined(WLMSG_ASSOC)
		WL_ASSOC(("wl%d: %s: auth frame for MAC %s\n",
				wlc->pub->unit, __FUNCTION__,
				bcm_ether_ntoa((struct ether_addr *)&mgmt->da,
				eabuf)));
#endif /* WLMSG_ASSOC */

		if (wlc_auth_get_bssid(bsscfg, &bssid, &target_mac) != BCME_OK) {
			WL_ERROR(("wl%d: %s: NULL bssid, Drop Auth frame\n",
					wlc->pub->unit, __FUNCTION__));
			return BCME_BADADDR;
		}

		if (len < (DOT11_MGMT_HDR_LEN + DOT11_AUTH_FIXED_LEN)) {
			WL_ERROR(("wl%d: %s: auth frame is too small\n",
					wlc->pub->unit, __FUNCTION__));
			return BCME_BUFTOOSHORT;
		}

		scb = wlc_scbfind(wlc, bsscfg, &target_mac);
		if (scb == NULL) {
#if defined(WLMSG_ASSOC)
			WL_ASSOC(("wl%d: %s: auth frame for unknown MAC %s\n",
					wlc->pub->unit, __FUNCTION__,
					bcm_ether_ntoa((struct ether_addr *)&mgmt->da,
					eabuf)));
#endif /* WLMSG_ASSOC */
			return BCME_BADADDR;
		}

		if ((pkt = wlc_frame_get_mgmt(wlc, FC_AUTH, &target_mac, &bsscfg->cur_etheraddr,
				&bssid, len - DOT11_MGMT_HDR_LEN,
				&auth_frm)) == NULL) {
			WL_ERROR(("wl%d: %s:  Sending auth frame failed \n",
					wlc->pub->unit, __FUNCTION__));
			return BCME_ERROR;
		}
		memcpy(auth_frm, (uint8 *)mgmt + DOT11_MGMT_HDR_LEN, len - DOT11_MGMT_HDR_LEN);

		/* Register a call back function to notify the tx completion */
#ifdef WL_CLIENT_SAE
		if (BSSCFG_EXT_AUTH(bsscfg)) {
			if ((ret = wlc_sae_assoc_change_state(bsscfg,
				(struct dot11_auth *)auth_frm)) != BCME_OK) {
				WL_ERROR(("wl%d: %s:  Wrong Sae Auth seq number \n",
					wlc->pub->unit, __FUNCTION__));
			} else if ((ret = wlc_ext_auth_tx_complete(wlc, bsscfg,
				pkt, (void *)(uintptr)bsscfg->ID)) != BCME_OK) {
				WL_ERROR(("wl%d: %s:  Couldn't register the tx complete cb \n",
						wlc->pub->unit, __FUNCTION__));
			}
			if (ret != BCME_OK) {
				PKTFREE(wlc->osh, pkt, TRUE);
				wlc_assoc_abort(bsscfg);
				return ret;
			}
		}
#endif /* WL_CLIENT_SAE */

		ret = wlc_sendmgmt(wlc, pkt, scb->bsscfg->wlcif->qi, scb);
#ifdef WL_SAE
		auth = (struct dot11_auth *) auth_frm;
		if (BSSCFG_AP(bsscfg)) {
			if ((bsscfg->WPA_auth & WPA3_AUTH_SAE_PSK) &&
					(ltoh16(auth->alg) == DOT11_SAE) &&
					(ltoh16(auth->seq) == WL_SAE_CONFIRM)) {
				/* Set SCB to authenticated */
				WL_ASSOC(("wl%d: %s: Set scb to AUTHENTICATED\n", wlc->pub->unit,
						__FUNCTION__));
				wlc_scb_clearstatebit(wlc, scb, PENDING_AUTH);
				wlc_scb_setstatebit(wlc, scb, AUTHENTICATED);
			}
		}
#endif /* WL_SAE */
		return ret;
}

#if defined(BCM_DCS) && defined(STA)
static int
wlc_send_action_brcm_dcs(wlc_info_t *wlc, wl_bcmdcs_data_t *data, struct scb *scb)
{
	void *p;
	uint8 *pbody;
	uint body_len;
	dot11_action_vs_frmhdr_t *action_vs_hdr;
	uint32 reason;
	chanspec_t channel_spec;

	if (!scb) {
		WL_ERROR(("wl%d: %s: scb = NULL\n", wlc->pub->unit, __FUNCTION__));
		return BCME_ERROR;
	}

	body_len = DOT11_ACTION_VS_HDR_LEN + sizeof(wl_bcmdcs_data_t);

	if ((p = wlc_frame_get_action(wlc, &scb->ea, &scb->bsscfg->cur_etheraddr,
		&scb->bsscfg->BSSID, body_len, &pbody, DOT11_ACTION_CAT_VS)) == NULL)
		return BCME_NOMEM;

	action_vs_hdr = (dot11_action_vs_frmhdr_t *)pbody;

	action_vs_hdr->category  = DOT11_ACTION_CAT_VS;
	action_vs_hdr->OUI[0]    = BCM_ACTION_OUI_BYTE0;
	action_vs_hdr->OUI[1]    = BCM_ACTION_OUI_BYTE1;
	action_vs_hdr->OUI[2]    = BCM_ACTION_OUI_BYTE2;
	action_vs_hdr->type      = BCM_ACTION_RFAWARE;
	action_vs_hdr->subtype   = BCM_ACTION_RFAWARE_DCS;

	reason = htol32(data->reason);
	bcopy(&reason, &action_vs_hdr->data[0], sizeof(uint32));
	channel_spec = htol16(data->chspec);
	bcopy(&channel_spec, &action_vs_hdr->data[4], sizeof(chanspec_t));

#ifdef BCMDBG
	{
	char da[ETHER_ADDR_STR_LEN];
	WL_INFORM(("wl%d: %s: send action frame to %s\n", wlc->pub->unit, __FUNCTION__,
		bcm_ether_ntoa(&scb->ea, da)));
	}
#endif // endif
	if (wlc_sendmgmt(wlc, p, SCB_WLCIFP(scb)->qi, scb) == FALSE) {
		return BCME_ERROR;
	}

	return 0;
} /* wlc_send_action_brcm_dcs */
#endif /* BCM_DCS && STA */

int
wlc_send_action_err(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	struct dot11_management_header *hdr, uint8 *body, int body_len)
{
	void *p;
	uint8* pbody;
	uint action_category;
#if defined(BCMDBG) || defined(WLMSG_INFORM)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_INFORM */

	if (!bsscfg) {
		WL_ERROR(("wl%d: %s: suppressing error reply due to missing bsscfg\n",
			wlc->pub->unit, __FUNCTION__));
		return BCME_ERROR;
	}

	if (ETHER_ISMULTI(&hdr->da)) {
		WL_INFORM(("wl%d: %s: suppressing error reply to %s",
			wlc->pub->unit, __FUNCTION__, bcm_ether_ntoa(&hdr->sa, eabuf)));
		WL_INFORM((" due to multicast address %s\n",
			bcm_ether_ntoa(&hdr->da, eabuf)));
		return BCME_ERROR;
	}

	action_category = (uint)body[0];

	if (body_len < 2) {
		WL_INFORM(("wl%d: %s: sending error reply to %s for Category %d "
		           " with missing Action field\n", wlc->pub->unit, __FUNCTION__,
		           bcm_ether_ntoa(&hdr->sa, eabuf), action_category));
	} else {
		WL_INFORM(("wl%d: %s: sending error reply to %s for Category/Action %d/%d\n",
		           wlc->pub->unit, __FUNCTION__,
		           bcm_ether_ntoa(&hdr->sa, eabuf), action_category, (int)body[1]));
	}

	action_category |= DOT11_ACTION_CAT_ERR_MASK;

	p = wlc_frame_get_action(wlc, &hdr->sa, &bsscfg->cur_etheraddr, &bsscfg->BSSID,
	                       body_len, &pbody, DOT11_ACTION_CAT_ERR_MASK);
	if (p == NULL) {
		WL_ERROR(("wl%d: %s: no memory for Action Error\n", WLCWLUNIT(wlc), __FUNCTION__));
		return BCME_NOMEM;
	}

	bcopy(body, pbody, body_len);
	pbody[0] = (action_category | DOT11_ACTION_CAT_ERR_MASK);

	if (wlc_sendmgmt(wlc, p, bsscfg->wlcif->qi, NULL) == FALSE) {
		return BCME_ERROR;
	}
	return BCME_OK;
} /* wlc_send_action_err */

void
wlc_bss_list_free(wlc_info_t *wlc, wlc_bss_list_t *bss_list)
{
	uint indx;
	wlc_bss_info_t *bi;

	/* inspect all BSS descriptor */
	for (indx = 0; indx < bss_list->count; indx++) {
		bi = bss_list->ptrs[indx];
		if (bi) {
			if (bi->bcn_prb) {
				MFREE(wlc->osh, bi->bcn_prb, bi->bcn_prb_len);
			}
			MFREE(wlc->osh, bi, sizeof(wlc_bss_info_t));
			bss_list->ptrs[indx] = NULL;
		}
	}
	bss_list->count = 0;
}

void
wlc_bss_list_xfer(wlc_bss_list_t *from, wlc_bss_list_t *to)
{
	uint size;

	ASSERT(to->count == 0);
	size = from->count * sizeof(wlc_bss_info_t*);
	bcopy((char*)from->ptrs, (char*)to->ptrs, size);
	bzero((char*)from->ptrs, size);

	to->count = from->count;
	from->count = 0;
}

#if defined(STS_FIFO_RXEN) || defined(WLC_OFFLOADS_RXSTS)
void BCMFASTPATH
wlc_stsbuff_free(wlc_info_t *wlc, void *pkt)
{
	if (STS_RX_ENAB(wlc->pub) || STS_RX_OFFLOAD_ENAB(wlc->pub)) {
		wlc_bmac_stsbuff_free(wlc->hw, pkt);
	}
}
#endif /* STS_FIFO_RXEN || WLC_OFFLOADS_RXSTS */

/* XXX 4360: wlc_calc_lsig_len() only needed for HT capable phys before rev40.
 * could ifdef around both this fn and wlc_d11n_hdrs() fn. Or will linker
 * optimization eliminate unused fns?
 */
/**
 * calculate frame duration for Mixed-mode L-SIG spoofing, return number of bytes goes in the length
 * field. Formula given by HT PHY Spec v 1.13
 *   len = 3(nsyms + nstream + 3) - 3
 */
uint16
wlc_calc_lsig_len(wlc_info_t *wlc, ratespec_t ratespec, uint mac_len)
{
	uint nsyms, len = 0, kNdps;

	WL_TRACE(("wl%d: %s: rate 0x%x, len %d\n", wlc->pub->unit, __FUNCTION__,
		ratespec, mac_len));

	ASSERT(!RSPEC_ISVHT(ratespec) && !RSPEC_ISHE(ratespec));

	if (RSPEC_ISHT(ratespec)) {
		uint mcs = ratespec & WL_RSPEC_HT_MCS_MASK;
		int tot_streams = wlc_ratespec_nsts(ratespec);

		ASSERT(WLC_PHY_11N_CAP(wlc->band));
		ASSERT(VALID_MCS(mcs));

		if (!VALID_MCS(mcs)) {
			mcs = 0;
		}

		/* the payload duration calculation matches that of regular ofdm */
		/* 1000Ndbps = kbps * 4 */
		kNdps = MCS_RATE(mcs, RSPEC_IS40MHZ(ratespec), RSPEC_ISSGI(ratespec)) * 4;

		if (RSPEC_ISSTBC(ratespec) == 0)
			/* NSyms = CEILING((SERVICE + 8*NBytes + TAIL) / Ndbps) */
			nsyms = CEIL((APHY_SERVICE_NBITS + 8 * mac_len + APHY_TAIL_NBITS)*1000,
			             kNdps);
		else
			/* STBC needs to have even number of symbols */
			nsyms = 2 * CEIL((APHY_SERVICE_NBITS + 8 * mac_len + APHY_TAIL_NBITS)*1000,
			                 2 * kNdps);

		nsyms += (tot_streams + 3);	/* (+3) account for HT-SIG(2) and HT-STF(1) */
		/* 3 bytes/symbol @ legacy 6Mbps rate */
		len = (3 * nsyms) - 3;	/* (-3) excluding service bits and tail bits */
	}

	return (uint16)len;
} /* wlc_calc_lsig_len */

/** calculate frame duration of a given rate and length, return time in usec unit */
uint
wlc_calc_frame_time(wlc_info_t *wlc, ratespec_t ratespec, uint8 preamble_type, uint mac_len)
{
	uint nsyms, dur = 0, Ndps;
	uint rate;

	WL_TRACE(("wl%d: wlc_calc_frame_time: rspec 0x%x, preamble_type %d, len%d\n",
		wlc->pub->unit, ratespec, preamble_type, mac_len));

	if (RSPEC_ISVHT(ratespec) || RSPEC_ISHE(ratespec)) {
		rate = wf_rspec_to_rate(ratespec);
		if (rate == 0) {
			ASSERT(0);
			WL_ERROR(("wl%d: WAR: using rate of 1 mbps\n", wlc->pub->unit));
			rate = 1000;
		}
		/* this is very approximate, ucode controls retries; not acct for he/vht overhead */
		dur = (mac_len * 8000 + rate - 1) / rate;
	} else if (RSPEC_ISHT(ratespec)) {
		dur = wlc_ht_calc_frame_time(wlc->hti, ratespec, preamble_type, mac_len);
	} else if (RSPEC_ISOFDM(ratespec)) {
		rate = RSPEC2RATE(ratespec);
		dur = APHY_PREAMBLE_TIME;
		dur += APHY_SIGNAL_TIME;
		/* Ndbps = Mbps * 4 = rate(500Kbps) * 2 */
		Ndps = rate*2;
		/* NSyms = CEILING((SERVICE + 8*NBytes + TAIL) / Ndbps) */
		nsyms = CEIL((APHY_SERVICE_NBITS + 8 * mac_len + APHY_TAIL_NBITS), Ndps);
		dur += APHY_SYMBOL_TIME * nsyms;
		if (BAND_2G(wlc->band->bandtype))
			dur += DOT11_OFDM_SIGNAL_EXTENSION;
	} else {
		ASSERT(RSPEC_ISCCK(ratespec));
		rate = RSPEC2RATE(ratespec);
		/* calc # bits * 2 so factor of 2 in rate (1/2 mbps) will divide out */
		mac_len = mac_len * 8 * 2;
		/* calc ceiling of bits/rate = microseconds of air time */
		dur = (mac_len + rate - 1) / rate;
		if (preamble_type & WLC_SHORT_PREAMBLE)
			dur += BPHY_PLCP_SHORT_TIME;
		else
			dur += BPHY_PLCP_TIME;
	}
	return dur;
}

static uint
wlc_calc_ack_time(wlc_info_t *wlc, ratespec_t rspec, uint8 preamble_type)
{
	uint dur = 0;

	WL_TRACE(("wl%d: wlc_calc_ack_time: rspec 0x%x, preamble_type %d\n", wlc->pub->unit, rspec,
		preamble_type));
	/* Spec 9.6: ack rate is the highest rate in BSSBasicRateSet that is less than
	 * or equal to the rate of the immediately previous frame in the FES
	 */
	rspec = WLC_BASIC_RATE(wlc, rspec);
	ASSERT(VALID_RATE_DBG(wlc, rspec));

	/* ACK frame len == 14 == 2(fc) + 2(dur) + 6(ra) + 4(fcs) */
	dur = wlc_calc_frame_time(wlc, rspec, preamble_type, (DOT11_ACK_LEN + DOT11_FCS_LEN));
	return dur;
}

uint
wlc_calc_cts_time(wlc_info_t *wlc, ratespec_t rspec, uint8 preamble_type)
{
	WL_TRACE(("wl%d: wlc_calc_cts_time: ratespec 0x%x, preamble_type %d\n", wlc->pub->unit,
	        rspec, preamble_type));
	return wlc_calc_ack_time(wlc, rspec, preamble_type);
}

/** derive wlc->band->basic_rate[] table from 'rateset' */
void
wlc_rate_lookup_init(wlc_info_t *wlc, wlc_rateset_t *rateset)
{
	uint8 rate;
	uint8 mandatory;
	uint8 cck_basic = 0;
	uint8 ofdm_basic = 0;
	uint8 *br = &wlc->band->basic_rate[0];
	uint i;

	/* incoming rates are in 500kbps units as in 802.11 Supported Rates */
	bzero(br, WLC_MAXRATE + 1);

	/* For each basic rate in the rates list, make an entry in the
	 * best basic lookup.
	 */
	for (i = 0; i < rateset->count; i++) {
		/* only make an entry for a basic rate */
		if (!(rateset->rates[i] & WLC_RATE_FLAG))
			continue;

		/* mask off basic bit */
		rate = (rateset->rates[i] & RATE_MASK);

		if (rate > WLC_MAXRATE) {
			WL_ERROR(("%s: invalid rate 0x%X in rate set\n",
				__FUNCTION__, rateset->rates[i]));
			continue;
		}

		br[rate] = rate;
	}

	/* The rate lookup table now has non-zero entries for each
	 * basic rate, equal to the basic rate: br[basicN] = basicN
	 *
	 * To look up the best basic rate corresponding to any
	 * particular rate, code can use the basic_rate table
	 * like this
	 *
	 * basic_rate = wlc->band->basic_rate[tx_rate]
	 *
	 * Make sure there is a best basic rate entry for
	 * every rate by walking up the table from low rates
	 * to high, filling in holes in the lookup table
	 */

	for (i = 0; i < wlc->band->hw_rateset.count; i++) {
		int is_ofdm;

		rate = wlc->band->hw_rateset.rates[i];
		ASSERT(rate <= WLC_MAXRATE);

		is_ofdm = RATE_ISOFDM(rate);

		if (br[rate] != 0) {
			/* This rate is a basic rate.
			 * Keep track of the best basic rate so far by
			 * modulation type.
			 */
			if (is_ofdm)
				ofdm_basic = rate;
			else
				cck_basic = rate;

			continue;
		}

		/* This rate is not a basic rate so figure out the
		 * best basic rate less than this rate and fill in
		 * the hole in the table
		 */

		br[rate] = is_ofdm ? ofdm_basic : cck_basic;

		if (br[rate] != 0)
			continue;

		/* This is a weird setup where the lowest basic rate
		 * of a modulation type is non-existent or higher than
		 * some supported rates in the same modulation
		 */
		WL_RATE(("wl%d: no basic rate with the same modulation less than or equal to rate"
			" %d%s\n",
			 wlc->pub->unit, rate/2, (rate % 2)?".5":""));

		if (is_ofdm) {
			/* In 11g and 11a, the OFDM mandatory rates are 6, 12, and 24 Mbps */
			if (rate >= WLC_RATE_24M)
				mandatory = WLC_RATE_24M;
			else if (rate >= WLC_RATE_12M)
				mandatory = WLC_RATE_12M;
			else
				mandatory = WLC_RATE_6M;
		} else {
			/* In 11b, all the CCK rates are mandatory 1 - 11 Mbps */
			mandatory = rate;
		}

		br[rate] = mandatory;
	}
} /* wlc_rate_lookup_init */

/** read rate table direct address map from shm */
static void
wlc_read_rt_dirmap(wlc_info_t *wlc)
{
	int i;
	for (i = 0; i < D11_RT_DIRMAP_SIZE; i ++)
		wlc->rt_dirmap_a[i] = wlc_read_shm(wlc, (M_RT_DIRMAP_A(wlc) + i * 2));
	for (i = 0; i < D11_RT_DIRMAP_SIZE; i ++)
		wlc->rt_dirmap_b[i] = wlc_read_shm(wlc, (M_RT_DIRMAP_B(wlc) + i * 2));
}

static void
wlc_write_rate_shm(wlc_info_t *wlc, uint8 rate, uint8 basic_rate)
{
	uint8 indx;
	uint8 basic_index;
	uint16 basic_table;
	uint16 *rt_dirmap;

	/* Find the direct address map table we are reading */
	rt_dirmap = RATE_ISOFDM(basic_rate) ? wlc->rt_dirmap_a : wlc->rt_dirmap_b;

	/* Shared memory address for the table we are writing */
	basic_table = RATE_ISOFDM(rate) ? M_RT_BBRSMAP_A(wlc) : M_RT_BBRSMAP_B(wlc);

	/*
	 * for a given rate, the LS-nibble of the PLCP SIGNAL field is
	 * the index into the rate table.
	 */
	indx = rate_info[rate] & RATE_INFO_M_RATE_MASK;
	basic_index = rate_info[basic_rate] & RATE_INFO_M_RATE_MASK;

	/* Update the SHM BSS-basic-rate-set mapping table with the pointer
	 * to the correct basic rate for the given incoming rate
	 */
	wlc_write_shm(wlc, (basic_table + indx * 2), rt_dirmap[basic_index]);
}

static const wlc_rateset_t *
wlc_rateset_get_hwrs(wlc_info_t *wlc)
{
	const wlc_rateset_t *rs_dflt;

	if (WLC_PHY_11N_CAP(wlc->band)) {
		if (BAND_5G(wlc->band->bandtype))
			rs_dflt = &ofdm_mimo_rates;
		else
			rs_dflt = &cck_ofdm_mimo_rates;
	} else if (wlc->band->gmode)
		rs_dflt = &cck_ofdm_rates;
	else
		rs_dflt = &cck_rates;

	return rs_dflt;
}

void
wlc_set_ratetable(wlc_info_t *wlc)
{
	const wlc_rateset_t *rs_dflt;
	wlc_rateset_t rs;
	uint8 rate, basic_rate;
	uint i;

	rs_dflt = wlc_rateset_get_hwrs(wlc);
	ASSERT(rs_dflt != NULL);

	wlc_rateset_copy(rs_dflt, &rs);

	wlc_rateset_mcs_upd(&rs, wlc->stf->op_txstreams);
	wlc_rateset_filter(&rs, &rs, FALSE, WLC_RATES_CCK_OFDM, RATE_MASK_FULL,
	                   wlc_get_mcsallow(wlc, wlc->cfg));

	/* walk the phy rate table and update SHM basic rate lookup table */
	for (i = 0; i < rs.count; i++) {
		rate = rs.rates[i] & RATE_MASK;

		/* for a given rate WLC_BASIC_RATE returns the rate at
		 * which a response ACK/CTS should be sent.
		 */
		basic_rate = WLC_BASIC_RATE(wlc, rate);
		if (basic_rate == 0) {
			/* This should only happen if we are using a
			 * restricted rateset.
			 */
			WL_RATE(("wl%d: set_ratetable: Adding rate %d to ratetable\n",
				wlc->pub->unit, rate));
			basic_rate = rs.rates[0] & RATE_MASK;
		}

		wlc_write_rate_shm(wlc, rate, basic_rate);
	}
}

/**
 * Return true if the specified rate is supported by the specified band.
 * WLC_BAND_AUTO indicates the current band.
 */
bool
wlc_valid_rate(wlc_info_t *wlc, ratespec_t rspec, int band, bool verbose)
{
	wlc_rateset_t *hw_rateset;
	uint i;

	if ((band == WLC_BAND_AUTO) || (band == wlc->band->bandtype)) {
		hw_rateset = &wlc->band->hw_rateset;
	} else if (NBANDS(wlc) > 1) {
		hw_rateset = &wlc->bandstate[OTHERBANDUNIT(wlc)]->hw_rateset;
	} else {
		/* other band specified and we are a single band device */
		return (FALSE);
	}

	if (RSPEC_ISHE(rspec)) {
		/* check for a HE (11ax) rate that is supported by the hardware */

		uint mcs = (rspec & WL_RSPEC_HE_MCS_MASK);
		uint nss = ((rspec & WL_RSPEC_HE_NSS_MASK) >> WL_RSPEC_HE_NSS_SHIFT);

		/* Need a check for valid HE rspec and hw support for HE mcs/Nss
		 * The following check for mcs>11 is an 11ax limit, but nss > 4 is a HW limit---11ax
		 * allows up to nss==8.
		 * Maybe an overal ratespec_valid check for well formed, and ratespec_hw_support
		 * check for current hw limits.
		 */
		if (mcs > WLC_MAX_HE_MCS || nss > 4) {
			goto error;
		} else {
			return TRUE;
		}
	} else if (RSPEC_ISVHT(rspec)) {
		/* check for a VHT (11ac) rate that is supported by the hardware */

		uint mcs = (rspec & WL_RSPEC_VHT_MCS_MASK);
		uint nss = ((rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT);

		/* XXX 4360:WES: Need a check for valid VHT rspec and hw support for VHT mcs/Nss
		 * The following check for mcs>9 is an 11ac limit, but nss>3 is a HW limit---11ac
		 * allows up to nss==8.
		 * Maybe an overal ratespec_valid check for well formed, and ratespec_hw_support
		 * check for current hw limits.
		 */
		if (mcs > WLC_MAX_VHT_MCS || nss > 4) {
			goto error;
		} else {
			return TRUE;
		}
	} else if (RSPEC_ISHT(rspec)) {
		/* check for an HT (11n) rate that is supported by the hardware */

		uint mcs = (rspec & WL_RSPEC_HT_MCS_MASK);

		if (!VALID_MCS(mcs))
			goto error;

		return isset(hw_rateset->mcs, mcs);
	}

	for (i = 0; i < hw_rateset->count; i++)
		if ((hw_rateset->rates[i] & RATE_MASK) == RSPEC2RATE(rspec))
			return (TRUE);
error:
	if (verbose) {
		WL_ERROR(("wl%d: %s: rate spec 0x%x not in hw_rateset\n",
		          wlc->pub->unit, __FUNCTION__, rspec));
#ifdef BCMDBG
		wlc_rateset_show(wlc, hw_rateset, NULL);
#endif // endif
	}

	return (FALSE);
} /* wlc_valid_rate */

/**
 * Writes PLCP headers and durations for probe response frames at all rates.
 */
static void
wlc_mod_prb_rsp_rate_table(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint frame_len)
{
	const wlc_rateset_t *rs_dflt;
	wlc_rateset_t rs;
	uint8 rate, bandunit;
	uint16 entry_ptr;
	uint8 plcp[D11_PHY_HDR_LEN];
	uint16 dur, sifs;
	uint i;

	bandunit = CHSPEC2WLC_BAND(cfg->current_bss->chanspec);

	sifs = SIFS(bandunit);

	rs_dflt = wlc_rateset_get_hwrs(wlc);
	ASSERT(rs_dflt != NULL);

	wlc_rateset_copy(rs_dflt, &rs);

	wlc_rateset_mcs_upd(&rs, wlc->stf->op_txstreams);
	wlc_rateset_filter(&rs, &rs, FALSE, WLC_RATES_CCK_OFDM, RATE_MASK_FULL,
	                   wlc_get_mcsallow(wlc, wlc->cfg));

	/* walk the phy rate table and update MAC core SHM basic rate table entries */
	for (i = 0; i < rs.count; i++) {  // for every rate supported by the PHY
		rate = rs.rates[i] & RATE_MASK;

		entry_ptr = wlc_rate_shm_offset(wlc, rate);

		/* Calculate the Probe Response PLCP for the given rate and addr */
		/* FROMDS -- not necessarily but a good first approximation for "to STA" */
		wlc_compute_plcp(wlc, cfg, rate, frame_len, FC_FROMDS, plcp);

		/* Calculate the duration of the Probe Response frame plus SIFS for the MAC */
		dur = (uint16)wlc_calc_frame_time(wlc, rate, WLC_LONG_PREAMBLE, frame_len);
		dur += sifs;

		/* Update the SHM Rate Table entry Probe Response values */
		/* different location for 802.11ac chips, per ucode */
		if (D11REV_GE(wlc->pub->corerev, 40)) {
			wlc_write_shm(wlc, entry_ptr + D11AC_M_RT_PRS_PLCP_POS,
				(uint16)(plcp[0] + (plcp[1] << 8)));
			wlc_write_shm(wlc, entry_ptr + D11AC_M_RT_PRS_PLCP_POS + 2,
				(uint16)(plcp[2] + (plcp[3] << 8)));
			wlc_write_shm(wlc, entry_ptr + D11AC_M_RT_PRS_DUR_POS, dur);
		} else {
			wlc_write_shm(wlc, entry_ptr + M_RT_PRS_PLCP_POS(wlc),
				(uint16)(plcp[0] + (plcp[1] << 8)));
			wlc_write_shm(wlc, entry_ptr + M_RT_PRS_PLCP_POS(wlc) + 2,
				(uint16)(plcp[2] + (plcp[3] << 8)));
			wlc_write_shm(wlc, entry_ptr + M_RT_PRS_DUR_POS(wlc), dur);
		}
	}
} /* wlc_mod_prb_rsp_rate_table */

/**
 * Compute the offset in usec from the first symbol in the payload (PHY-RXSTART), to the first
 * symbol of the TSF.
 */
uint16
wlc_compute_bcn_payloadtsfoff(wlc_info_t *wlc, ratespec_t rspec)
{
	uint bcntsfoff = 0;

	if (!RSPEC_ISLEGACY(rspec)) {
		WL_ERROR(("wl%d: recd beacon with mcs rate; rspec 0x%x\n",
			wlc->pub->unit, rspec));
	} else if (RSPEC_ISOFDM(rspec)) {
		/* PLCP SERVICE + MAC header time (SERVICE + FC + DUR + A1 + A2 + A3 + SEQ == 26
		 * bytes at beacon rate)
		 */
		bcntsfoff += wlc_compute_airtime(wlc, rspec, APHY_SERVICE_NBITS/8 +
			DOT11_MAC_HDR_LEN);
	} else {
		/* MAC header time (FC + DUR + A1 + A2 + A3 + SEQ == 24 bytes at beacon rate) */
		bcntsfoff += wlc_compute_airtime(wlc, rspec, DOT11_MAC_HDR_LEN);
	}
	return (uint16)(bcntsfoff);
}

/** Compute the offset in usec from the beginning of the preamble, to the first symbol of the TSF */
uint16
wlc_compute_bcntsfoff(wlc_info_t *wlc, ratespec_t rspec, bool short_preamble, bool phydelay)
{
	uint bcntsfoff = 0;

	/* compute the air time for the preamble */
	if (!RSPEC_ISLEGACY(rspec)) {
		WL_ERROR(("wl%d: recd beacon with mcs rate; rspec 0x%x\n",
			wlc->pub->unit, rspec));
	} else if (RSPEC_ISOFDM(rspec)) {
		/* tx delay from MAC through phy to air (2.1 usec) +
		 * phy header time (preamble + PLCP SIGNAL == 20 usec) +
		 * PLCP SERVICE + MAC header time (SERVICE + FC + DUR + A1 + A2 + A3 + SEQ == 26
		 * bytes at beacon rate)
		 */
		bcntsfoff += phydelay ? D11A_PHY_TX_DELAY : 0;
		bcntsfoff += APHY_PREAMBLE_TIME + APHY_SIGNAL_TIME;
	} else {
		/* tx delay from MAC through phy to air (3.4 usec) +
		 * phy header time (long preamble + PLCP == 192 usec) +
		 * MAC header time (FC + DUR + A1 + A2 + A3 + SEQ == 24 bytes at beacon rate)
		 */
		bcntsfoff += phydelay ? D11B_PHY_TX_DELAY : 0;
		bcntsfoff += short_preamble ? D11B_PHY_SPREHDR_TIME: D11B_PHY_LPREHDR_TIME;
	}

	/* add the time from the end of the preamble/beginning of payload to the tsf field */
	bcntsfoff += wlc_compute_bcn_payloadtsfoff(wlc, rspec);

	return (uint16)(bcntsfoff);
}

bool
wlc_erp_find(wlc_info_t *wlc, void *body, uint body_len, uint8 **erp, int *len)
{
	bcm_tlv_t *ie;
	uint8 *tlvs = (uint8 *)body + DOT11_BCN_PRB_LEN;
	int tlvs_len = body_len - DOT11_BCN_PRB_LEN;
	BCM_REFERENCE(wlc);

	ie = bcm_parse_tlvs(tlvs, tlvs_len, DOT11_MNG_ERP_ID);

	if (ie == NULL)
		return FALSE;

	*len = ie->len;
	*erp = ie->data;

	return TRUE;
}

/**
 *	Max buffering needed for beacon template/prb resp template is 142 bytes.
 *
 *	PLCP header is 6 bytes.
 *	802.11 A3 header is 24 bytes.
 *	Max beacon frame body template length is 112 bytes.
 *	Max probe resp frame body template length is 110 bytes.
 *
 *      *len on input contains the max length of the packet available.
 *
 *	The *len value is set to the number of bytes in buf used, and starts with the PLCP
 *	and included up to, but not including, the 4 byte FCS.
 */
void
wlc_bcn_prb_template(wlc_info_t *wlc, uint type, ratespec_t bcn_rspec, wlc_bsscfg_t *cfg,
                     uint16 *buf, int *len)
{
	uint8 *body;
	struct dot11_management_header *h;
	int hdr_len, body_len;

	ASSERT(*len >= 142);
	ASSERT(type == FC_BEACON || type == FC_PROBE_RESP);

	if ((MBSS_BCN_ENAB(wlc, cfg) && type == FC_BEACON) ||
	    (D11REV_GE(wlc->pub->corerev, 40) && type == FC_PROBE_RESP)) {
		h = (struct dot11_management_header *)buf;
		hdr_len = DOT11_MAC_HDR_LEN;
	} else {
		int plcp_hdr_len = wlc->pub->d11tpl_phy_hdr_len;
		h = (struct dot11_management_header *)((uint8 *)buf + plcp_hdr_len);
		hdr_len = plcp_hdr_len + DOT11_MAC_HDR_LEN;
	}
	bzero(buf, hdr_len);

	body_len = *len - hdr_len; /* calc buffer size provided for frame body */
	body = (uint8 *)buf + hdr_len;

	wlc_bcn_prb_body(wlc, type, cfg, body, &body_len, FALSE);

	*len = hdr_len + body_len; /* return actual size */

	/* PLCP for Probe Response frames are filled in from core's rate table */
	if (!MBSS_BCN_ENAB(wlc, cfg) && type == FC_BEACON) {
		int mf_len = DOT11_MAC_HDR_LEN + body_len;
		uint8 *plcp;
		/* fill in PLCP */
		plcp = (uint8 *)buf + wlc_template_plcp_offset(wlc, bcn_rspec);
		wlc_compute_plcp(wlc, cfg, bcn_rspec, mf_len + DOT11_FCS_LEN, FC_FROMDS, plcp);
	}

	/* fill in 802.11 header */
	h->fc = htol16((uint16)type);

	/* DUR is 0 for multicast bcn, or filled in by MAC for prb resp */
	/* A1 filled in by MAC for prb resp, broadcast for bcn */
	if (type == FC_BEACON)
		bcopy((const char*)&ether_bcast, (char*)&h->da, ETHER_ADDR_LEN);
	bcopy((char*)&cfg->cur_etheraddr, (char*)&h->sa, ETHER_ADDR_LEN);
	bcopy((char*)&cfg->BSSID, (char*)&h->bssid, ETHER_ADDR_LEN);

	/* SEQ filled in by MAC */
} /* wlc_bcn_prb_template */

int
wlc_write_hw_bcnparams(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	uint16 *bcn, int len, ratespec_t bcn_rspec, bool suspend)
{
	BCM_REFERENCE(suspend);
	if (HWBCN_ENAB(cfg))
		wlc_beacon_phytxctl_txant_upd(wlc, bcn_rspec);

	if (!cfg->associated || BSSCFG_IBSS(cfg)) {
		uint16 bcn_offset;

		if (HWBCN_ENAB(cfg)) {
#ifdef WLMCHAN
			wlc_beacon_phytxctl(wlc, bcn_rspec, cfg->current_bss->chanspec);
#else
			wlc_beacon_phytxctl(wlc, bcn_rspec, wlc->chanspec);
#endif /* WLMCHAN */
			wlc_beacon_upddur(wlc, bcn_rspec, len);
		}
		/* VSDB TODO: wlc_beacon_phytxctl_percfg  */

		/* beacon timestamp symbol tsf offset */
		bcn_offset = wlc_compute_bcntsfoff(wlc, bcn_rspec, FALSE, TRUE);
		ASSERT(bcn_offset != 0);
		wlc->band->bcntsfoff = bcn_offset;
		wlc_write_shm(wlc, M_BCN_TXTSF_OFFSET(wlc), bcn_offset);
	}

	if (BSSCFG_AP(cfg) && !MBSS_BCN_ENAB(wlc, cfg)) {
		uint8 *cp;
		uint16 tim_offset;

		/* find the TIM elt offset in the bcn template */
		cp = (uint8 *)bcn + wlc->pub->d11tpl_phy_hdr_len +
		        DOT11_MAC_HDR_LEN + DOT11_BCN_PRB_LEN;
		len -= (int)(cp - (uint8 *)bcn);
		if (len < 0) {
			WL_ERROR(("wl%d.%d: %s: beacon length is wrong\n",
			          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
			return BCME_BADARG;
		}
		cp = (uint8 *)bcm_parse_tlvs(cp, len, DOT11_MNG_TIM_ID);
		if (cp == NULL) {
			WL_ERROR(("wl%d.%d: %s: unable to find TIM in beacon frame\n",
			          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
			return BCME_ERROR;
		}
		tim_offset = (uint16)(cp - (uint8 *)bcn);
		wlc_write_shm(wlc, M_TIMBPOS_INBEACON(wlc), tim_offset);
	}

	return BCME_OK;
} /* wlc_write_hw_bcnparams */

void
wlc_bss_update_dtim_period(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	uint16 dtim_period;
	uint32 val = 0;

	/* set DTIM count to 0 in SCR */
	wlc_bmac_copyto_objmem(wlc->hw, S_DOT11_DTIMCOUNT << 2, &val, sizeof(val), OBJADDR_SCR_SEL);

	dtim_period = cfg->associated ? cfg->current_bss->dtim_period :
		wlc->default_bss->dtim_period;

	wlc_write_shm(wlc, M_DOT11_DTIMPERIOD(wlc), dtim_period);
}

/** save bcn/prbresp in current_bss for IBSS */
static void
wlc_ibss_bcn_prb_save(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint8 *bcn, int len)
{
	wlc_bss_info_t *current_bss = cfg->current_bss;

	if (current_bss->bcn_prb != NULL)
		return;

	ASSERT(current_bss->bcn_prb_len == 0);

	len -= wlc->pub->d11tpl_phy_hdr_len + DOT11_MAC_HDR_LEN;
	if (len <= 0)
		return;

	current_bss->bcn_prb = (struct dot11_bcn_prb *)MALLOC(wlc->osh, len);
	if (current_bss->bcn_prb == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return;
	}

	bcopy(bcn + wlc->pub->d11tpl_phy_hdr_len + DOT11_MAC_HDR_LEN,
		(uint8 *)current_bss->bcn_prb, len);
	current_bss->bcn_prb_len = (uint16)len;
}

static void
wlc_bss_set_bcn_rspec(wlc_info_t *wlc, wlc_bsscfg_t *cfg, wlc_bss_info_t *bss)
{
	ratespec_t rspec = 0;

	/* AP */
	if (BSSCFG_AP(cfg)) {
		rspec = wlc_force_bcn_rspec(wlc);
	}

	if (rspec == 0) {
		/* STA or AP */
		rspec = wlc_get_compat_basic_rspec(wlc, &bss->rateset, WLC_RATE_1M);
	}

	ASSERT(wlc_valid_rate(wlc, rspec,
		CHSPEC_IS2G(bss->chanspec) ? WLC_BAND_2G : WLC_BAND_5G, TRUE));

	wlc->bcn_rspec = rspec;
}

/**
 * Update a beacon for a particular BSS. For MBSS, this updates the software template and sets
 * "latest" to the index of the template updated. Otherwise, it updates the hardware template.
 */
static void
_wlc_bss_update_beacon(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	/* Clear the soft intmask */
	wlc_bmac_set_defmacintmask(wlc->hw, MI_BCNTPL, ~MI_BCNTPL);

#ifdef MBSS
	if (MBSS_BCN_ENAB(wlc, cfg)) {
		wlc_mbss_update_beacon(wlc, cfg);
	} else
#endif /* MBSS */
	if (HWBCN_ENAB(cfg)) { /* Hardware beaconing for this config */
		uint16 *bcn;
		uint32	both_valid = MCMD_BCN0VLD | MCMD_BCN1VLD;
		osl_t *osh = wlc->osh;
		wlc_bss_info_t *current_bss = cfg->current_bss;
		int len, bcn_tmpl_len;

		bcn_tmpl_len = wlc->pub->bcn_tmpl_len;
		len = bcn_tmpl_len;

		/* Check if both templates are in use, if so sched. an interrupt
		 *	that will call back into this routine
		 */
		if ((R_REG(osh, D11_MACCOMMAND(wlc)) & both_valid) == both_valid) {
			/* clear any previous status */
			W_REG(osh, D11_MACINTSTATUS(wlc), MI_BCNTPL);
		}
		/* Check that after scheduling the interrupt both of the
		 *	templates are still busy. if not clear the int. & remask
		 */
		if ((R_REG(osh, D11_MACCOMMAND(wlc)) & both_valid) == both_valid) {
			wlc_bmac_set_defmacintmask(wlc->hw, MI_BCNTPL, MI_BCNTPL);
			return;
		}

		if ((bcn = (uint16 *)MALLOC(osh, bcn_tmpl_len)) == NULL) {
			WL_ERROR(("wl%d: %s: out of mem, %d bytes malloced\n",
			          wlc->pub->unit, __FUNCTION__, MALLOCED(osh)));
			return;
		}

		wlc_bss_set_bcn_rspec(wlc, cfg, current_bss);

		/* update the template and ucode shm */
		wlc_bcn_prb_template(wlc, FC_BEACON, wlc->bcn_rspec, cfg, bcn, &len);
		wlc_write_hw_bcnparams(wlc, cfg, bcn, len, wlc->bcn_rspec, FALSE);
		wlc_write_hw_bcntemplates(wlc, bcn, len, FALSE);

		/* set a default bcn_prb for IBSS */
		if (BSSCFG_STA(cfg) && !cfg->BSS)
			wlc_ibss_bcn_prb_save(wlc, cfg, (uint8 *)bcn, len);

		MFREE(osh, (void *)bcn, bcn_tmpl_len);
	} else {
		wlc_bss_tplt_upd_notif(wlc, cfg, FC_BEACON);
	}
} /* _wlc_bss_update_beacon */

void
wlc_bss_update_beacon(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	if (!cfg->up)
		return;
	_wlc_bss_update_beacon(wlc, cfg);
}

/** Update all beacons for the system */
void
wlc_update_beacon(wlc_info_t *wlc)
{
	int idx;
	wlc_bsscfg_t *bsscfg;

	/* update AP or IBSS beacons */
	FOREACH_BSS(wlc, idx, bsscfg) {
		if (bsscfg->up &&
		    (BSSCFG_AP(bsscfg) || !bsscfg->BSS))
			wlc_bss_update_beacon(wlc, bsscfg);
	}
}

/** Write ssid into shared memory */
void
wlc_shm_ssid_upd(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	uint8 *ssidptr = cfg->SSID;
	uint16 base = M_SSID(wlc);
	uint8 ssidbuf[DOT11_MAX_SSID_LEN];

#ifdef MBSS
	if (MBSS_ENAB(wlc->pub)) {
		wlc_mbss_shm_ssid_upd(wlc, cfg, &base);
		return;
	}
#endif /* MBSS */

	/* padding the ssid with zero and copy it into shm */
	bzero(ssidbuf, DOT11_MAX_SSID_LEN);
	bcopy(ssidptr, ssidbuf, cfg->SSID_len);

	wlc_copyto_shm(wlc, base, ssidbuf, DOT11_MAX_SSID_LEN);

	if (!MBSS_BCN_ENAB(wlc, cfg))
		wlc_write_shm(wlc, M_SSID_BYTESZ(wlc), (uint16)cfg->SSID_len);
} /* wlc_shm_ssid_upd */

void
wlc_update_probe_resp(wlc_info_t *wlc, bool suspend)
{
	int idx;
	wlc_bsscfg_t *bsscfg;

	/* update AP or IBSS probe responses */
	FOREACH_BSS(wlc, idx, bsscfg) {
		if (bsscfg->up &&
		    (BSSCFG_AP(bsscfg) || !bsscfg->BSS))
			wlc_bss_update_probe_resp(wlc, bsscfg, suspend);
	}
}

void
wlc_bss_update_probe_resp(wlc_info_t *wlc, wlc_bsscfg_t *cfg, bool suspend)
{
	int len, bcn_tmpl_len;

	bcn_tmpl_len = wlc->pub->bcn_tmpl_len;
	len = bcn_tmpl_len;

#ifdef WLPROBRESP_SW
	if (WLPROBRESP_SW_ENAB(wlc))
		return;
#endif /* WLPROBRESP_SW */

	/* write the probe response to hardware, or save in the config structure */
#if defined(MBSS)
	if (MBSS_PRB_ENAB(wlc, cfg)) {
		/* Generating probe resp in sw; update local template */
		wlc_mbss_update_probe_resp(wlc, cfg, suspend);
	} else
#endif /* MBSS */
	if (HWPRB_ENAB(cfg)) { /* Hardware probe resp for this config */
		uint16 *prb_resp;
		int offset;

		if ((prb_resp = (uint16 *)MALLOC(wlc->osh, bcn_tmpl_len)) == NULL) {
			WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			return;
		}

		/* create the probe response template */
		wlc_bcn_prb_template(wlc, FC_PROBE_RESP, 0, cfg, prb_resp, &len);

		if (suspend)
			wlc_suspend_mac_and_wait(wlc);

		if (D11REV_GE(wlc->pub->corerev, 40))
			offset = D11AC_T_PRS_TPL_BASE;
		else
			offset = D11_T_PRS_TPL_BASE;

		/* write the probe response into the template region */
		wlc_bmac_write_template_ram(wlc->hw, offset, (len + 3) & ~3, prb_resp);

		/* write the length of the probe response frame (+PLCP/-FCS) */
		wlc_write_shm(wlc, M_PRS_FRM_BYTESZ(wlc), (uint16)len);

		/* write the SSID and SSID length */
		wlc_shm_ssid_upd(wlc, cfg);

		/*
		 * Write PLCP headers and durations for probe response frames at all rates.
		 * Use the actual frame length covered by the PLCP header for the call to
		 * wlc_mod_prb_rsp_rate_table() by adding the FCS.
		 */
		len += DOT11_FCS_LEN;

		/* pre-rev40 macs include space for the PLCP in the template area
		 * so subtract that from the frame len
		 */

		if (D11REV_LT(wlc->pub->corerev, 40))
			len -= D11_PHY_HDR_LEN;

		wlc_mod_prb_rsp_rate_table(wlc, cfg, (uint16)len);

		if (suspend)
			wlc_enable_mac(wlc);

		MFREE(wlc->osh, (void *)prb_resp, bcn_tmpl_len);
	} else {
		wlc_bss_tplt_upd_notif(wlc, cfg, FC_PROBE_RESP);
	}
} /* wlc_bss_update_probe_resp */

/* Beacon and Probe Response frame body
 *  max sz  order
 *  8       1 Timestamp
 *  2       2 Beacon interval
 *  2       3 Capability information
 *  34      4 SSID
 *  10      5 Supported rates
 *  (0)     6 FH Params             (not supported)
 *  3       7 DS Parameter Set
 *  (0)     8 CF Params             (not supported)
 *  4       9 IBSS Parameter Set    (IBSS only)
 *  6(256)  10 TIM                  (BSS-Beacon only)
 *  8(113)  11 Country Information  (11d)
 *  3       15 Channel Switch Announcement (11h)
 *  6       16 Quiet period	    (11h)
 *  3       19 ERP Information      (11g)
 *  10(257) 20 Ext. Sup. Rates      (11g)
 *  7          BRCM proprietary
 *  27         WME params
 *
 *  The TIM can be 256 bytes long in general. For a beacon template,
 *  a NULL TIM is used, which is 6 bytes long.
 *
 *  Country element is minimum of 8 which includes a base of 5 plus
 *  possibly 3 bytes per supported channel (less if the channel numbers
 *  are monotonic and can be compressed.
 *
 *  Max template beacon frame body length is 112 bytes. (items 1-7 and 10, 19, 20, 221)
 *  Max template probe response frame body length is 110 bytes. (items 1-7 and 9, 19, 20, 221)
 */

void
wlc_bcn_prb_body(wlc_info_t *wlc, uint type, wlc_bsscfg_t *bsscfg, uint8 *bcn, int *len,
	bool legacy_tpl)
{
	struct dot11_bcn_prb *fixed_info;
	wlc_rateset_t sup_rates, ext_rates;
	uint8 *pbody;
	uint body_len;
	wlc_bss_info_t *current_bss;
	wlc_iem_ft_cbparm_t ftcbparm;
	wlc_iem_cbparm_t cbparm;
	uint8 ht_wsec_restrict;
	bool ht_cap = TRUE;

	ASSERT(bsscfg != NULL);
	ASSERT(len != NULL);

	current_bss = bsscfg->current_bss;

	ASSERT(*len >= 112);
	ASSERT(ISALIGNED(bcn, sizeof(uint16)));
	ASSERT(type == FC_BEACON || type == FC_PROBE_RESP);

	bzero(&sup_rates, sizeof(wlc_rateset_t));
	bzero(&ext_rates, sizeof(wlc_rateset_t));

	/* check for a supported rates override, add back iBSS Gmode setting */
	if (wlc->sup_rates_override.count > 0)
		wlc_rateset_copy(&wlc->sup_rates_override, &sup_rates);
	else if ((!AP_ENAB(wlc->pub)) && (wlc->band->gmode == GMODE_LRS))
		wlc_rateset_copy(&cck_rates, &sup_rates);
	else if ((!AP_ENAB(wlc->pub)) && (wlc->band->gmode == GMODE_PERFORMANCE))
		wlc_default_rateset(wlc, &sup_rates);

	wlc_rateset_elts(wlc, bsscfg, &current_bss->rateset, &sup_rates, &ext_rates);

	body_len = DOT11_BCN_PRB_LEN;

	ht_wsec_restrict = wlc_ht_get_wsec_restriction(wlc->hti);
	if (((ht_wsec_restrict & WLC_HT_WEP_RESTRICT) &&
		WSEC_WEP_ENABLED(bsscfg->wsec)) ||
		((ht_wsec_restrict & WLC_HT_TKIP_RESTRICT) &&
		WSEC_TKIP_ENABLED(bsscfg->wsec) && !(WSEC_AES_ENABLED(bsscfg->wsec)))) {
		ht_cap = FALSE;
	}

	/* prepare IE mgmt calls */
	bzero(&ftcbparm, sizeof(ftcbparm));
	ftcbparm.bcn.sup = &sup_rates;
	ftcbparm.bcn.ext = &ext_rates;
	bzero(&cbparm, sizeof(cbparm));
	cbparm.ft = &ftcbparm;
	cbparm.bandunit = CHSPEC_WLCBANDUNIT(current_bss->chanspec);
	cbparm.ht = (BSS_N_ENAB(wlc, bsscfg) && !legacy_tpl && ht_cap) ? TRUE : FALSE;
#ifdef WL11AC
	cbparm.vht = (BSS_VHT_ENAB(wlc, bsscfg) && !legacy_tpl && ht_cap) ? TRUE : FALSE;
#endif // endif
#ifdef WL11AX
	cbparm.he = (BSS_HE_ENAB_BAND(wlc, wlc->bandstate[cbparm.bandunit]->bandtype, bsscfg) &&
	             !legacy_tpl && ht_cap) ? TRUE : FALSE;
#endif // endif

	/* calculate the IEs' length */
	body_len += wlc_iem_calc_len(wlc->iemi, bsscfg, (uint16)type, NULL, &cbparm);
	if (body_len > (uint)*len) {
		WL_ERROR(("wl%d: %s: buffer too short, buf %d, needs %u\n",
		          wlc->pub->unit, __FUNCTION__, *len, body_len));
		return;
	}

	fixed_info = (struct dot11_bcn_prb *)bcn;
	pbody = bcn;

	/* fill in fixed_info info, Timestamp, Beacon interval, and Capability */
	bzero((char*)fixed_info, sizeof(*fixed_info));
	/* Timestamp is zero in bcn template, filled in by MAC */
	/* [N.B.: fixed_info->timestamp is misaligned but unreferenced, so "safe".] */

	fixed_info->beacon_interval = htol16((uint16)current_bss->beacon_period);
	if (BSSCFG_AP(bsscfg)) {
		fixed_info->capability = DOT11_CAP_ESS;
#ifdef WL11K
		if (WL11K_ENAB(wlc->pub) && wlc_rrm_enabled(wlc->rrm_info, bsscfg))
			fixed_info->capability |= DOT11_CAP_RRM;
#endif // endif
	}
#ifdef WLP2P
	else if (BSS_P2P_DISC_ENAB(wlc, bsscfg))
		fixed_info->capability = 0;
#endif // endif
	else
		fixed_info->capability = DOT11_CAP_IBSS;
	if (WSEC_ENABLED(bsscfg->wsec) && bsscfg->wsec_restrict) {
		fixed_info->capability |= DOT11_CAP_PRIVACY;
	}
	/* Advertise short preamble capability if we have not been forced long AND we
	 * are not an APSTA
	 */
	if (!APSTA_ENAB(wlc->pub) && BAND_2G(wlc->band->bandtype) &&
	    (current_bss->capability & DOT11_CAP_SHORT))
		fixed_info->capability |= DOT11_CAP_SHORT;
	if (wlc->band->gmode && wlc->shortslot)
		fixed_info->capability |= DOT11_CAP_SHORTSLOT;
	if (BSS_WL11H_ENAB(wlc, bsscfg) && BSSCFG_AP(bsscfg))
		fixed_info->capability |= DOT11_CAP_SPECTRUM;

	fixed_info->capability = htol16(fixed_info->capability);

	pbody += DOT11_BCN_PRB_LEN;
	body_len -= DOT11_BCN_PRB_LEN;

	/* fill in Info elts, SSID, Sup Rates, DS params, and IBSS params */
	if (wlc_iem_build_frame(wlc->iemi, bsscfg, (uint16)type,
	                        NULL, &cbparm, pbody, body_len) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_build_frame failed\n",
		          wlc->pub->unit, __FUNCTION__));
	}

	pbody -= DOT11_BCN_PRB_LEN;
	body_len += DOT11_BCN_PRB_LEN;

	*len = body_len;

	return;
} /* wlc_bcn_prb_body */

uint16
wlc_assocscb_getcnt(wlc_info_t *wlc)
{
	struct scb *scb;
	uint16 assoc = 0;
	struct scb_iter scbiter;
	int32 idx;
	wlc_bsscfg_t *bsscfg;

	FOREACH_BSS(wlc, idx, bsscfg) {
		FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
			if (SCB_ASSOCIATED(scb))
				assoc++;
		}
	}
	return assoc;
}

static uint16
wlc_bss_scb_getcnt(wlc_info_t *wlc, wlc_bsscfg_t *cfg, bool (*scb_test_cb)(struct scb *scb))
{
	struct scb *scb;
	uint16 assoc = 0;
	struct scb_iter scbiter;
	int32 idx;
	wlc_bsscfg_t *bsscfg;

	FOREACH_BSS(wlc, idx, bsscfg) {
		FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
			if (scb->bsscfg == cfg && scb_test_cb(scb))
				assoc++;
		}
	}
	return assoc;
}

static bool
wlc_scb_associated_cb(struct scb *scb)
{
	return SCB_ASSOCIATED(scb);
}

uint16
wlc_bss_assocscb_getcnt(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	return wlc_bss_scb_getcnt(wlc, cfg, wlc_scb_associated_cb);
}

/** return TRUE if cfg->brcm_ie[] changed, FALSE if not */
bool
wlc_update_brcm_ie(wlc_info_t *wlc)
{
	int idx;
	wlc_bsscfg_t *cfg;
	bool update = FALSE;

	FOREACH_BSS(wlc, idx, cfg) {
		update |= wlc_bss_update_brcm_ie(wlc, cfg);
	}

	return update;
}

bool
wlc_bss_update_brcm_ie(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	uint8 assoc;
	brcm_ie_t *brcm_ie;
	volatile uint8 flags = 0, flags1 = 0;

	/*
	 * PR 30446
	 * Odd compiler behavior observed
	 * Be cautious about changing the following lines.
	 *
	 */

	brcm_ie = (brcm_ie_t *)&cfg->brcm_ie[0];

	flags = 0;

#ifdef AP
	if (BSSCFG_AP(cfg)) {
#ifdef WDS
		if (wlc_wds_lazywds_is_enable(wlc->mwds) && cfg == wlc->cfg)
			flags |= BRF_LZWDS;
#endif // endif
#ifdef BCM_DCS
		/* enable BCM_DCS for BCM AP */
		if (wlc->ap->dcs_enabled)
			flags1 |= BRF1_RFAWARE_DCS;
#endif // endif
		/* As long as WME is enabled, AP does the right thing
		 * when a WME STA goes to PS mode as opposed to some versions of BRCM AP
		 * that would not buffer traffic going to a WME STA in PS mode
		 */
		if (WME_ENAB(wlc->pub))
			flags1 |= BRF1_WMEPS;
#ifdef SOFTAP
		flags1 |= BRF1_SOFTAP;
#endif // endif
		flags1 |= BRF1_PSOFIX;
	}
#endif /* AP */

	/* enable capability of receiving large aggregate if STA is
	 * ACPHY and AMPDU_ENAB
	 */
	if (WLCISACPHY(wlc->band) && AMPDU_ENAB(wlc->pub)) {
		flags1 |= BRF1_RX_LARGE_AGG;
	}

	if (WLPROPRIETARY_11N_RATES_ENAB(wlc->pub)) {
		if (wlc->pub->ht_features != WLC_HT_FEATURES_PROPRATES_DISAB)
			flags |= BRF_PROP_11N_MCS;
	}

#ifdef DWDS
	if (DWDS_ENAB(cfg)) {
		flags1 |= BRF1_DWDS;
	}
#endif /* DWDS */

	/* count the total number of associated stas */
	assoc = wlc_bss_assocscb_getcnt(wlc, cfg);

#if defined(WLWNM) && defined(WLWNM_BRCM)
	if (WLWNM_ENAB(wlc->pub) && WLWNM_BRCM_ENAB(wlc->pub)) {
		flags1 |= BRF1_WNM;
	}
#endif /* WLWNM && WLWNM_BRCM */

	/* no change? */
	if ((brcm_ie->assoc == assoc) && (brcm_ie->flags == flags) && (brcm_ie->flags1 == flags1))
		return (FALSE);

	brcm_ie->assoc = assoc;
	brcm_ie->flags = flags;
	brcm_ie->flags1 = flags1;

	ASSERT((cfg->brcm_ie[TLV_LEN_OFF]+TLV_HDR_LEN) < WLC_MAX_BRCM_ELT);

	return (TRUE);
} /* wlc_bss_update_brcm_ie */

/** process received BRCM info element */
void
wlc_process_brcm_ie(wlc_info_t *wlc, struct scb *scb, brcm_ie_t *brcm_ie)
{

	if (!brcm_ie || brcm_ie->len < 4) {
#ifdef BRCMAPIVTW
		if (!AMPDU_HOST_REORDER_ENAB(wlc->pub))
			wlc_keymgmt_ivtw_enable(wlc->keymgmt, scb, FALSE);
#endif // endif
		return;
	}

	ASSERT(scb != NULL);

	/* remote station is BRCM AP/STA */
	scb->flags |= SCB_BRCM;

	/* legacy AES implementations */
	if (brcm_ie->ver <= BRCM_IE_LEGACY_AES_VER)
		scb->flags |= SCB_LEGACY_AES;
	else
		scb->flags &= ~SCB_LEGACY_AES;

	/* early brcm_ie is only 7 bytes in length */
	if (brcm_ie->len <= (OFFSETOF(brcm_ie_t, flags) - TLV_HDR_LEN)) {
		/* XXX This can abort below STA BRF1_WMEPS processing.
		 * Is this correct? johnvb
		 */
		return;
	}

	/* process flags field */

	/* newer brcm_ie has flags1 field */
	if (brcm_ie->len <= (OFFSETOF(brcm_ie_t, flags1) - TLV_HDR_LEN)) {
#ifdef BRCMAPIVTW
		if (!AMPDU_HOST_REORDER_ENAB(wlc->pub))
			wlc_keymgmt_ivtw_enable(wlc->keymgmt, scb, TRUE);
#endif // endif
		return;
	}

	/* process flags1 field */

	if (brcm_ie->flags1 & BRF1_WMEPS)
		scb->flags |= SCB_WMEPS;
	else
		scb->flags &= ~SCB_WMEPS;

#ifdef BCM_DCS
	if (brcm_ie->flags1 & BRF1_RFAWARE_DCS)
		scb->flags2 |= SCB2_BCMDCS;
	else
		scb->flags2 &= ~SCB2_BCMDCS;
#endif /* BCM_DCS */

#ifdef BRCMAPIVTW
	if (!AMPDU_HOST_REORDER_ENAB(wlc->pub)) {
		wlc_keymgmt_ivtw_enable(wlc->keymgmt,  scb,
			((brcm_ie->flags1 & BRF1_PSOFIX) ? FALSE : TRUE));
	}
#endif // endif

	if (brcm_ie->flags1 & BRF1_RX_LARGE_AGG) {
		scb->flags2 |= SCB2_RX_LARGE_AGG;
	}
	else {
		scb->flags2 &= ~SCB2_RX_LARGE_AGG;
	}

	if (WLPROPRIETARY_11N_RATES_ENAB(wlc->pub)) {
		if (wlc->pub->ht_features != WLC_HT_FEATURES_PROPRATES_DISAB &&
		    GET_BRF_PROP_11N_MCS(brcm_ie)) {
			SCB_HT_PROP_RATES_CAP_SET(scb);
		}
	}
} /* wlc_process_brcm_ie */

/* recover 32bit TSF value from the 16bit TSF value */
/* assumption is time in rxh is within 65ms of the current tsf */
/* local TSF inserted in the rxh is at RxStart which is before 802.11 header */
uint32
wlc_recover_tsf32(wlc_info_t *wlc, wlc_d11rxhdr_t *wrxh)
{
	uint16	rxh_tsf;
	uint32	ts_tsf;
	uint16 rfdly;
	BCM_REFERENCE(wlc);

	rxh_tsf = D11RXHDR_ACCESS_VAL(&wrxh->rxhdr, wlc->pub->corerev, RxTSFTime);

	ts_tsf = wrxh->tsf_l;

	/* adjust rx dly added in RxTSFTime */
	/* XXX comment in d11.h:
	 * BWL_PRE_PACKED_STRUCT struct d11rxhdr {
	 *	...
	 *	uint16 RxTSFTime;	RxTSFTime time of first MAC symbol + M_PHY_PLCPRX_DLY
	 *	...
	 * }
	 */

	/* TODO: add PHY type specific value here... */
	rfdly = M_BPHY_PLCPRX_DLY;

	rxh_tsf -= rfdly;

	return (((ts_tsf - rxh_tsf) & 0xFFFF0000) | rxh_tsf);
}

/* recover 64bit TSF value from the 16bit TSF value */
/* assumption is time in rxh is within 65ms of the current tsf */
/* 'tsf' carries in the current TSF time */
/* local TSF inserted in the rxh is at RxStart which is before 802.11 header */
void
wlc_recover_tsf64(wlc_info_t *wlc, wlc_d11rxhdr_t *wrxh, uint32 *tsf_h, uint32 *tsf_l)
{
	uint32 rxh_tsf;

	rxh_tsf = wlc_recover_tsf32(wlc, wrxh);

	/* a lesser TSF time indicates the low 32 bits of
	 * TSF wrapped, so decrement the high 32 bits.
	 */
	if (*tsf_l < rxh_tsf) {
		*tsf_h -= 1;
		WL_NONE(("TSF has wrapped (rxh %x tsf %x), adjust to %x%08x\n",
		         rxh_tsf, *tsf_l, *tsf_h, rxh_tsf));
	}
	*tsf_l = rxh_tsf;
}

#ifdef STA
static void *
wlc_frame_get_ps_ctl(wlc_info_t *wlc, wlc_bsscfg_t *cfg, const struct ether_addr *bssid,
	const struct ether_addr *sa)
{
	void *p;
	struct dot11_ps_poll_frame *hdr;

	if ((p = wlc_frame_get_ctl(wlc, DOT11_PS_POLL_LEN)) == NULL) {
		return (NULL);
	}

	/* construct a PS-Poll frame */
	hdr = (struct dot11_ps_poll_frame *)PKTDATA(wlc->osh, p);
	hdr->fc = htol16(FC_PS_POLL);
	hdr->durid = htol16(cfg->AID);
	bcopy((const char*)bssid, (char*)&hdr->bssid, ETHER_ADDR_LEN);
	bcopy((const char*)sa, (char*)&hdr->ta, ETHER_ADDR_LEN);

	return (p);
}
#endif /* STA */

void*
wlc_frame_get_ctl(wlc_info_t *wlc, uint len)
{
	void *p;
	osl_t *osh;

	ASSERT(len != 0);

	osh = wlc->osh;
	if ((p = PKTGET(osh, (TXOFF + len), TRUE)) == NULL) {
		WL_ERROR(("wl%d: %s: pktget error for len %d\n",
			wlc->pub->unit, __FUNCTION__, ((int)TXOFF + len)));
		WLCNTINCR(wlc->pub->_cnt->txnobuf);
		return (NULL);
	}
	ASSERT(ISALIGNED(PKTDATA(osh, p), sizeof(uint32)));

	/* reserve TXOFF bytes of headroom */
	PKTPULL(osh, p, TXOFF);
	PKTSETLEN(osh, p, len);

	PKTSETPRIO(p, 0);

	return (p);
}

void
wlc_fill_mgmt_hdr(struct dot11_management_header *hdr, uint16 fc,
	const struct ether_addr *da, const struct ether_addr *sa, const struct ether_addr *bssid)
{
	/* construct a management frame */
	hdr->fc = htol16(fc);
	hdr->durid = 0;
	bcopy((const char*)da, (char*)&hdr->da, ETHER_ADDR_LEN);
	bcopy((const char*)sa, (char*)&hdr->sa, ETHER_ADDR_LEN);
	bcopy((const char*)bssid, (char*)&hdr->bssid, ETHER_ADDR_LEN);
	hdr->seq = 0;
}

static void*
wlc_frame_get_mgmt_int(wlc_info_t *wlc, uint16 fc, const struct ether_addr *da,
	const struct ether_addr *sa, const struct ether_addr *bssid, uint body_len,
	uint8 **pbody, uint8 cat)
{
	void *p;
#ifdef MFP
	if (WLC_MFP_ENAB(wlc->pub)) {
		p = wlc_mfp_frame_get_mgmt(wlc->mfp, fc, cat, da, sa, bssid, body_len, pbody);
	} else {
		p = wlc_frame_get_mgmt_ex(wlc, fc, da, sa, bssid, body_len, pbody, 0, 0);
	}
#else
	p = wlc_frame_get_mgmt_ex(wlc, fc, da, sa, bssid, body_len, pbody, 0, 0);
#endif /* !MFP */

	/* Management frame statistics counters:
	 * These counters are incremented rihgt after frame generation,
	 * and they may be dropped, or failed in TX.
	 */
	if (p) {
		switch (fc & FC_KIND_MASK) {
		case FC_ASSOC_REQ:
			WLCNTINCR(wlc->pub->_cnt->txassocreq);
			break;
		case FC_ASSOC_RESP:
			WLCNTINCR(wlc->pub->_cnt->txassocrsp);
			break;
		case FC_REASSOC_REQ:
			WLCNTINCR(wlc->pub->_cnt->txreassocreq);
			break;
		case FC_REASSOC_RESP:
			WLCNTINCR(wlc->pub->_cnt->txreassocrsp);
			break;
		case FC_DISASSOC:
			WLCNTINCR(wlc->pub->_cnt->txdisassoc);
			break;
		case FC_AUTH:
			WLCNTINCR(wlc->pub->_cnt->txauth);
			break;
		case FC_DEAUTH:
			WLCNTINCR(wlc->pub->_cnt->txdeauth);
			break;
		case FC_ACTION:
			WLCNTINCR(wlc->pub->_cnt->txaction);
			break;
		case FC_PROBE_REQ:
			WLCNTINCR(wlc->pub->_cnt->txprobereq);
			break;
		case FC_PROBE_RESP:
			WLCNTINCR(wlc->pub->_cnt->txprobersp);
			break;
		default:
			break;
		}
	}
	return p;
}

void*
wlc_frame_get_mgmt_ex(wlc_info_t *wlc, uint16 fc, const struct ether_addr *da,
    const struct ether_addr *sa, const struct ether_addr *bssid, uint body_len,
    uint8 **pbody, uint iv_len, uint tail_len)
{
	uint len;
	void *p = NULL;
	osl_t *osh;
	struct dot11_management_header *hdr;

	osh = wlc->osh;
	len = DOT11_MGMT_HDR_LEN + iv_len + body_len + tail_len;

	if ((p = PKTGET(osh, (TXOFF + len), TRUE)) == NULL) {
#if defined(BCMPKTPOOL) && (!defined(BCMHWA) || !defined(HWA_TXFIFO_BUILD))
		uint32 req_len = TXOFF + len;

		if (POOL_ENAB(wlc->pub->pktpool) &&
		    (req_len <= pktpool_max_pkt_bytes(wlc->pub->pktpool)))
			p = pktpool_get(wlc->pub->pktpool);
#endif /* BCMPKTPOOL */

		if (p == NULL) {
			WL_ERROR(("wl%d: wlc_frame_get_mgmt: pktget error for len %d fc %x\n",
				wlc->pub->unit, ((int)TXOFF + len), fc));
			WLCNTINCR(wlc->pub->_cnt->txnobuf);
			return (NULL);
		}
	}

	ASSERT(ISALIGNED((uintptr)PKTDATA(osh, p), sizeof(uint32)));

	/* reserve TXOFF bytes of headroom */
	PKTPULL(osh, p, TXOFF);
	PKTSETLEN(osh, p, len - tail_len);

	/* construct a management frame */
	hdr = (struct dot11_management_header *)PKTDATA(osh, p);
	wlc_fill_mgmt_hdr(hdr, fc, da, sa, bssid);

	*pbody = (uint8*)&hdr[1] + iv_len;

	/* Set MAX Prio for MGMT packets */
	PKTSETPRIO(p, MAXPRIO);

#if defined(WLP2P) && defined(BCMDBG)
	if (fc == FC_ACTION || fc == FC_PROBE_REQ || fc == FC_PROBE_RESP) {
		char eabuf[ETHER_ADDR_STR_LEN];
		char *frmtype_str = NULL;
		switch (fc) {
			case FC_ACTION:
				frmtype_str = "ACTION";
				break;
			case FC_PROBE_REQ:
				frmtype_str = "PROBE_REQ";
				break;
			case FC_PROBE_RESP:
				frmtype_str = "PROBE_RESP";
				break;
		}
		WL_P2P(("wl%d: prep %s frame tx to %s, pkt %p\n",
		        wlc->pub->unit, frmtype_str,
		        bcm_ether_ntoa(&hdr->da, eabuf), OSL_OBFUSCATE_BUF(p)));
	}
	WL_WSEC(("wl%d: %s: allocated %u bytes; iv %u and tail %u bytes\n",
	         wlc->pub->unit, __FUNCTION__, (uint)(TXOFF + len), iv_len, tail_len));
#endif /* WLP2P && BCMDBG */

	return (p);
} /* wlc_frame_get_mgmt_ex */

void*
wlc_frame_get_mgmt(wlc_info_t *wlc, uint16 fc, const struct ether_addr *da,
	const struct ether_addr *sa, const struct ether_addr *bssid, uint body_len,
	uint8 **pbody)
{
	/* All action frame must use wlc_frame_get_action instead of wlc_frame_get_mgmt */
	ASSERT(fc != FC_ACTION);
	return wlc_frame_get_mgmt_int(wlc, fc, da, sa, bssid, body_len, pbody, 0);
}

void*
wlc_frame_get_action(wlc_info_t *wlc, const struct ether_addr *da,
	const struct ether_addr *sa, const struct ether_addr *bssid, uint body_len,
	uint8 **pbody, uint8 cat)
{
	return wlc_frame_get_mgmt_int(wlc, FC_ACTION, da, sa, bssid, body_len, pbody, cat);
}

/*
 * check the packet header and see if these are AMPDU session packets
 * and return whether they should go on default software high prec queue or
 * regular prec as data packets
*/
static uint8
wlc_ctrlmgmt_pkt_prec(uchar *pbody)
{
	struct dot11_header *hdr = (struct dot11_header *)pbody;
	uint16 fk;

	fk = (ltoh16(hdr->fc) & FC_KIND_MASK);

	if (fk == FC_ACTION) {
		uchar *ba_data;
		struct dot11_management_header *m_hdr = (struct dot11_management_header *)pbody;
		ba_data = ((uint8 *)&m_hdr[1]);

		WL_INFORM(("action frame: pbody %p, m_hdr[1] %p, category %d, action %d\n",
			pbody, &m_hdr[1], ba_data[0], ba_data[1]));
		if ((ba_data[0] == DOT11_ACTION_CAT_BLOCKACK) &&
			((ba_data[1] == DOT11_BA_ACTION_ADDBA_REQ) ||
			(ba_data[1] == DOT11_BA_ACTION_DELBA)))
		{
			return REGULAR_PREC;
		}
	}
	else if (fk == FC_BLOCKACK_REQ)
		return REGULAR_PREC;

	return HIGH_PREC;
}

bool
wlc_queue_80211_frag(wlc_info_t *wlc, void *p, wlc_txq_info_t *qi, struct scb *scb,
	wlc_bsscfg_t *bsscfg, bool short_preamble, wlc_key_t *key, ratespec_t rate_override)
{
	int prio;
	uint txparam_flag = 0;
	uint16 fk;
	struct dot11_management_header *hdr;
	wlc_bsscfg_t *cfg;

#ifdef WL_TX_STALL
	wlc_tx_status_t toss_reason = WLC_TX_STS_TOSS_UNKNOWN;
	bool update_tx_status_counter = TRUE;
#endif /* WL_TX_STALL */

	ASSERT(wlc->pub->up);

	hdr = (struct dot11_management_header*)PKTDATA(wlc->osh, p);
	if (scb == NULL) {
		if (!ETHER_ISMULTI(&hdr->da)) {
			/* XXX:SCB is NULL:
			 * Unicast frames such as "probe response" could be destined to an
			 * already associated client. Need to check if a scb matching "DA"
			 * exists before assigning a hwrs_scb.
			 */
			scb = wlc_scbapfind(wlc, (struct ether_addr *)(&hdr->da), &cfg);
		}
		/* use hw rateset scb if NULL */
		if (!scb) {
			scb = wlc->band->hwrs_scb;
		}
		if (!bsscfg) {
			bsscfg = scb->bsscfg;
		}
	}

	/* Update TX queued count */
	wlc_tx_status_update_counters(wlc, p, scb, bsscfg, WLC_TX_STS_QUEUED, 1);

	prio = PKTPRIO(p);
	ASSERT(prio <= MAXPRIO);

	if (!bsscfg) {
		WL_TX_STS_UPDATE(toss_reason, WLC_TX_STS_TOSS_BSSCFG_DOWN);
		goto toss;
	}
	if (SCB_DEL_IN_PROGRESS(scb)|| SCB_MARKED_DEL(scb)) {
		WL_TX_STS_UPDATE(toss_reason, WLC_TX_STS_TOSS_SCB_DELETED)

		WLPKTTAGSCBSET(p, scb);
		WLPKTTAGBSSCFGSET(p, WLC_BSSCFG_IDX(bsscfg));
		goto toss;
	}

	fk = (ltoh16(hdr->fc) & FC_KIND_MASK);

	WLPKTTAGSCBSET(p, scb);

#ifdef MFP
	/* setup key for MFP */
	if (WLC_MFP_ENAB(wlc->pub) && (WLPKTTAG(p)->flags & WLF_MFP) && key == NULL) {
		if (ETHER_ISMULTI(&hdr->da) && BSSCFG_AP(bsscfg))
			key = wlc_keymgmt_get_bss_tx_key(wlc->keymgmt, bsscfg, TRUE, NULL);
		else if (!ETHER_ISMULTI(&hdr->da) && SCB_MFP(scb))
			key = wlc_keymgmt_get_scb_key(wlc->keymgmt, scb, WLC_KEY_ID_PAIRWISE,
				WLC_KEY_FLAG_NONE, NULL);
	}
#endif /* MFP */

	WLPKTTAGBSSCFGSET(p, WLC_BSSCFG_IDX(bsscfg));

	/* set WLF_PSDONTQ for all non-bufferable mgmt frames */
	if (FC_TYPE(ltoh16(hdr->fc)) == FC_TYPE_MNG &&
	    !(fk == FC_ACTION || fk == FC_DISASSOC || fk == FC_DEAUTH ||
	      (fk == FC_PROBE_RESP && BSSCFG_IBSS(bsscfg)))) {
		WLPKTTAG(p)->flags |= WLF_PSDONTQ;
	}

	if (short_preamble)
		txparam_flag |= WLC_TX_PARAMS_SHORTPRE;
	/* save the tx options for wlc_prep_pdu */
	wlc_pdu_push_txparams(wlc, p, txparam_flag, key, rate_override);

	/* Set the lifetime, if its not set && we have bitmask set && lifetime is set */
	/* This gives flexibility for the caller to set its own lifetime as required */
	if (!(WLPKTTAG(p)->flags & WLF_EXPTIME) &&
		(FC_TYPE(hdr->fc) == FC_TYPE_MNG) && (wlc->cmn->lifetime_mg->lifetime)) {
		if (wlc->cmn->lifetime_mg->mgmt_bitmap & (1 << FC_SUBTYPE(hdr->fc)))
			wlc_lifetime_set(wlc, p, wlc->cmn->lifetime_mg->lifetime);
	}

	if (BSSCFG_AP(bsscfg) && SCB_PS(scb) && !(WLPKTTAG(p)->flags & WLF_PSDONTQ)) {
		if (wlc_apps_psq(wlc, p, WLC_PRIO_TO_HI_PREC(prio))) {
			return TRUE;
		} else {
			WL_TX_STS_UPDATE(toss_reason, WLC_TX_STS_TOSS_NOT_PSQ1);
			goto toss;
		}
	}

	if (cpktq_prec_enq(wlc, &qi->cpktq, p,
		(wlc_ctrlmgmt_pkt_prec((uchar *)hdr) ? WLC_PRIO_TO_HI_PREC(prio) :
			WLC_PRIO_TO_PREC(prio))))
	{
		wlc_send_q(wlc, qi);
		return TRUE;
	}
#ifdef WL_TX_STALL
	/* XXX: need to ensure wlc_txq_immediate_enqueue() updates
	 * error counters on queue failure
	 */
	/* On failure wlc_prec_enq already incremented the the failure count. */
	update_tx_status_counter = FALSE;
#endif /* WL_TX_STALL */

toss:
#ifdef WL_TX_STALL
	WL_INFORM(("wl%d: %s, toss_reason: %d ;", wlc->pub->unit, __FUNCTION__, toss_reason));
#else
	WL_INFORM(("wl%d: %s, toss ;", wlc->pub->unit, __FUNCTION__));
#endif /* WL_TX_STALL */
	if (WL_INFORM_ON())
		wlc_log_unexpected_tx_frame_log_80211hdr(wlc, (const struct dot11_header *)hdr);

#ifdef WL_TX_STALL
	if (update_tx_status_counter) {
		/* There should be a valid reason to toss a frame */
		ASSERT(toss_reason != WLC_TX_STS_TOSS_UNKNOWN);

		/* Update failure reason count */
		wlc_tx_status_update_counters(wlc, p, scb, bsscfg, toss_reason, 1);
	}
#endif /* WL_TX_STALL */

	PKTFREE(wlc->osh, p, TRUE);
	WLCNTINCR(wlc->pub->_cnt->txnobuf);
	WLCIFCNTINCR(scb, txnobuf);
	WLCNTSCB_COND_INCR(scb, scb->scb_stats.tx_failures);
#ifdef WL11K
	wlc_rrm_stat_qos_counter(wlc, scb, prio, OFFSETOF(rrm_stat_group_qos_t, txfail));
#endif // endif
	return FALSE;
} /* wlc_queue_80211_frag */

bool
wlc_sendmgmt(wlc_info_t *wlc, void *p, wlc_txq_info_t *qi, struct scb *scb)
{

	return wlc_queue_80211_frag(wlc, p, qi, scb,
		(scb ? scb->bsscfg : NULL), FALSE, NULL,
		(scb ? WLC_LOWEST_SCB_RSPEC(scb) : WLC_LOWEST_BAND_RSPEC(wlc->band)));
}

/**
 * Handles send of both control packets and nulldata packet.
 */
bool
wlc_sendctl(wlc_info_t *wlc, void *p, wlc_txq_info_t *qi, struct scb *scb,
	ratespec_t rate_override, bool enq_only)
{
	int prio;
#ifdef WL_TX_STALL
	wlc_tx_status_t toss_reason = WLC_TX_STS_TOSS_UNKNOWN;
	bool update_tx_status_counter = TRUE;
#endif /* WL_TX_STALL */
	struct dot11_header *hdr = (struct dot11_header *)PKTDATA(wlc->osh, p);

	BCM_REFERENCE(hdr);

	ASSERT(wlc->pub->up);

	/* SCB must not be NULL */
	ASSERT(scb != NULL);

	wlc_tx_status_update_counters(wlc, p, scb, scb->bsscfg,
		WLC_TX_STS_QUEUED, 1);

	if (!scb->bsscfg) {
		WL_TX_STS_UPDATE(toss_reason, WLC_TX_STS_TOSS_BSSCFG_DOWN);
		goto toss;
	}
	if (SCB_DEL_IN_PROGRESS(scb)|| SCB_MARKED_DEL(scb)) {
		WL_TX_STS_UPDATE(toss_reason, WLC_TX_STS_TOSS_SCB_DELETED);
		goto toss;
	}

	prio = PKTPRIO(p);
	ASSERT(prio <= MAXPRIO);

	WLPKTTAGSCBSET(p, scb);
	if (scb->bsscfg)
		WLPKTTAGBSSCFGSET(p, WLC_BSSCFG_IDX(scb->bsscfg));

	/* save the tx options for wlc_prep_pdu */
	wlc_pdu_push_txparams(wlc, p, 0, NULL, rate_override);

	if ((wlc->lifetime_ctrl) && !(WLPKTTAG(p)->flags & WLF_EXPTIME))
		wlc_lifetime_set(wlc, p, wlc->lifetime_ctrl);

	if (BSSCFG_AP(SCB_BSSCFG(scb)) && SCB_PS(scb) && !(WLPKTTAG(p)->flags & WLF_PSDONTQ)) {
		if (wlc_apps_psq(wlc, p, WLC_PRIO_TO_HI_PREC(prio))) {
			return TRUE;
		} else {
			WL_TX_STS_UPDATE(toss_reason, WLC_TX_STS_TOSS_NOT_PSQ2);
			goto toss;
		}
	}

	if (cpktq_prec_enq(wlc, &qi->cpktq, p,
		(wlc_ctrlmgmt_pkt_prec((uchar *)hdr) ? WLC_PRIO_TO_HI_PREC(prio) :
			WLC_PRIO_TO_PREC(prio))))
	{
		if (!enq_only)
			wlc_send_q(wlc, qi);
		return TRUE;
	}
#ifdef WL_TX_STALL
	else {
		/* On failure wlc_prec_enq already incremented the the failure count. */
		update_tx_status_counter = FALSE;
	}
#endif /* WL_TX_STALL */

toss:

#ifdef WL_TX_STALL
	if (update_tx_status_counter) {
		/* There should be a valid reason to toss a frame */
		ASSERT(toss_reason != WLC_TX_STS_TOSS_UNKNOWN);

		/* Update failure reason count */
		wlc_tx_status_update_counters(wlc, p, scb, scb->bsscfg, toss_reason, 1);
	}
#endif /* WL_TX_STALL */

	PKTFREE(wlc->osh, p, TRUE);
	WLCNTINCR(wlc->pub->_cnt->txnobuf);
	return FALSE;
} /* wlc_sendctl */

static const uint8 ac2prio[] = {
	PRIO_8021D_BE,	/* AC_BE index */
	PRIO_8021D_BK, 	/* AC_BK index */
	PRIO_8021D_VI, 	/* AC_VI index */
	PRIO_8021D_VO	/* AC_VO index */
};

/** prep raw pkt via header additions etc. */
uint16
wlc_prep80211_raw(wlc_info_t *wlc, wlc_if_t *wlcif, uint ac,
	void *p, ratespec_t rspec, uint *outfifo)
{
	struct scb *scb;
	uint fifo;
	int prio;
	uint16 fid;

	ASSERT(ac < ARRAYSIZE(ac2prio));
	prio = ac2prio[ac];
	/* Currently, only the OLPC engine uses this function.  If another module needs
	 * to use this, then it may need to be modified to pass in a pointer to the scb.
	 */
	scb = wlc->band->hwrs_scb;
	BCM_REFERENCE(wlcif);

	/* WME: send frames based on priority setting */
	if (WME_ENAB(wlc->pub)) {
		fifo = prio2fifo[prio];
	} else {
		fifo = TX_CTL_FIFO;
	}
	if (outfifo) {
		*outfifo = fifo;
	}

	PKTSETPRIO(p, prio);
	WL_TRACE(("%s: fifo=%d prio=%d\n", __FUNCTION__, fifo, prio));
	/* add headers */
	fid = wlc_d11hdrs(wlc, p, scb, 0, 0, 1, fifo, 0, NULL, NULL, rspec);

	WLPKTTAGSCBSET(p, scb);
	if (scb->bsscfg)
		WLPKTTAGBSSCFGSET(p, WLC_BSSCFG_IDX(scb->bsscfg));
	return fid;
}

/**
 * Save the tx options for wlc_prep_pdu. Push a tx_params structure on the head of a packet when
 * creating an MPDU packet without a d11 hardware txhdr.
 */
static void
wlc_pdu_push_txparams(wlc_info_t *wlc, void *p, uint32 flags, wlc_key_t *key,
	ratespec_t rate_override)
{
	wlc_pdu_tx_params_t tx_params;

	tx_params.flags = flags;
	tx_params.key = key;
	tx_params.rspec_override = rate_override;

	PKTPUSH(wlc->osh, p, sizeof(tx_params));
	memcpy(PKTDATA(wlc->osh, p), &tx_params, sizeof(tx_params));

	/* Mark the pkt as an CTLMGMT (tx_params and 802.11 header) but no ucode txhdr */
	WLPKTTAG(p)->flags |= WLF_CTLMGMT;
}

static void
wlc_get_sup_ext_rates(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, wlc_rateset_t *sup_rates,
	wlc_rateset_t *ext_rates)
{
	const wlc_rateset_t *rs;

	bzero(sup_rates, sizeof(wlc_rateset_t));
	bzero(ext_rates, sizeof(wlc_rateset_t));
	if ((BAND_2G(wlc->band->bandtype)) && (wlc->band->gmode == GMODE_LEGACY_B))
		rs = &cck_rates;	/* only advertise 11b rates */
	else
		rs = &wlc->band->hw_rateset;

	/* For 2.4Ghz band probe requests, CCK-only in sup_rates, OFDM in ext_rates */
	if ((BAND_2G(wlc->band->bandtype)) && wlc->legacy_probe)
		wlc_rateset_copy(&cck_rates, sup_rates);

	wlc_rateset_elts(wlc, bsscfg, rs, sup_rates, ext_rates);
}

/* XXX we are generating probe request based on the configuration in this particular bsscfg.
 * This may not be the correct behavior because we are passing the wlc->cfg when scanning
 */
void
wlc_sendprobe(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	const uint8 ssid[], int ssid_len,
	const struct ether_addr *sa, const struct ether_addr *da, const struct ether_addr *bssid,
	ratespec_t ratespec_override, uint8 *extra_ie, int extra_ie_len)
{
	void *pkt;
	wlc_rateset_t sup_rates, ext_rates;
	uint8 *pbody;
	uint body_len;
	wlc_iem_ft_cbparm_t ftcbparm;
	wlc_iem_cbparm_t cbparm;
	BCM_REFERENCE(ratespec_override);
	BCM_REFERENCE(extra_ie);
	BCM_REFERENCE(extra_ie_len);

	if (!bsscfg||!bsscfg->brcm_ie||!sa)
		return;
#ifdef WL_MBO
	/* FMC feature sends unicast probe request in case of roam reassoc.
	 * BRCM internal/debug build enables to send unicast probe request.
	 * Check if unicast probe request is allowed to associated AP to be safe.
	 */
	if ((BSS_MBO_ENAB(wlc, bsscfg)) && (bsscfg->associated) &&
		(!memcmp(da, &bsscfg->BSSID, ETHER_ADDR_LEN)) &&
		(bsscfg->current_bss->bcnflags & WLC_BSS_MBO_ASSOC_DISALLOWED)) {
		return;
	}
#endif /* WL_MBO */

	wlc_get_sup_ext_rates(wlc, bsscfg, &sup_rates, &ext_rates);

	/* prepare the IE mgmt calls */
	bzero(&ftcbparm, sizeof(ftcbparm));
	ftcbparm.prbreq.mcs = wlc->band->hw_rateset.mcs;
	ftcbparm.prbreq.ssid = (const uint8 *)ssid;
	ftcbparm.prbreq.ssid_len = (uint8)ssid_len;
	ftcbparm.prbreq.sup = &sup_rates;
	ftcbparm.prbreq.ext = &ext_rates;
	ftcbparm.prbreq.da = da;
	bzero(&cbparm, sizeof(cbparm));
	cbparm.ft = &ftcbparm;
	/* Can we assume we are already on the right band at this point? */
	cbparm.bandunit = (uint8)wlc->band->bandunit;
	cbparm.ht = BSS_N_ENAB(wlc, bsscfg);
#ifdef WL11AC
	cbparm.vht = BSS_VHT_ENAB(wlc, bsscfg);
#endif // endif
#ifdef WL11AX
	cbparm.he = BSS_HE_ENAB_BAND(wlc, wlc->bandstate[cbparm.bandunit]->bandtype, bsscfg) ?
	        TRUE : FALSE;
#endif // endif

	/* calculate IEs' length */
	body_len = wlc_iem_calc_len(wlc->iemi, bsscfg, FC_PROBE_REQ, NULL, &cbparm);

	/* allocate a packet */
	if ((pkt = wlc_frame_get_mgmt(wlc, FC_PROBE_REQ, da,
		sa, bssid, body_len, &pbody)) == NULL) {
		WL_ERROR(("wl%d: %s: wlc_frame_get_mgmt failed\n",
		          wlc->pub->unit, __FUNCTION__));
		return;
	}

	/* populte IEs */
	if (wlc_iem_build_frame(wlc->iemi, bsscfg, FC_PROBE_REQ, NULL, &cbparm,
	                        pbody, body_len) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_build_frame failed\n",
		          wlc->pub->unit, __FUNCTION__));
		PKTFREE(wlc->osh, pkt, TRUE);
		return;
	}

	wlc_lifetime_set(wlc, pkt, WLC_MGMT_PROBE_REQ_LIFETIME*1000);
	/* Ensure that pkt is not re-enqueued to FIFO after suppress */
	WLPKTTAG(pkt)->flags3 |= WLF3_TXQ_SHORT_LIFETIME;

#ifdef WLP2P
	if (BSS_P2P_ENAB(wlc, bsscfg))
		wlc_p2p_sendprobe(wlc->p2p, bsscfg, pkt);
	else
#endif // endif
#ifdef WL_OCE
		if (OCE_ENAB(wlc->pub))
			wlc_oce_send_probe(wlc->oce, pkt);
		else
#endif // endif
			wlc_sendmgmt(wlc, pkt, wlc->active_queue, NULL);
} /* wlc_sendprobe */

void *
wlc_sendauth(
	wlc_bsscfg_t *cfg,
	struct ether_addr *ea,
	struct ether_addr *bssid,
	struct scb *scb,
	int auth_alg,
	int auth_seq,
	int auth_status,
	uint8 *challenge_text,
	bool short_preamble,
	int (*pre_send_fn)(wlc_info_t *wlc, wlc_bsscfg_t *cfg, void *data, void *pkt),
	void *data,
	void (*tx_cplt_fn)(wlc_info_t *wlc, uint txstatus, void *tx_cplt_ctx),
	void *tx_cplt_ctx)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_txq_info_t *qi;
	void *pkt = NULL;
	uint8 *pbody;
	uint body_len;
	struct dot11_auth *auth;
	wlc_key_t *key = NULL;
	wlc_key_info_t key_info;

	wlc_iem_ft_cbparm_t ftcbparm;
	wlc_iem_cbparm_t cbparm;
	uint16 fc;

	ASSERT(cfg != NULL);
	memset(&key_info, 0, sizeof(wlc_key_info_t));

	/* assert:  (status == success) -> (scb is not NULL) */
	ASSERT((auth_status != DOT11_SC_SUCCESS) || (scb != NULL));

	/* All Authentication frames have a common 6 byte contents:
	 * 2 bytes Authentication Alg Number
	 * 2 bytes Authentication Transaction Number
	 * 2 bytes Status Code
	 */
	body_len = sizeof(struct dot11_auth);

	wlc_key_get_info(NULL, &key_info); /* init key info, algo=off */

	/* Authentications frames 2 and 3 for Shared Key auth have a 128 byte
	 * Challenge Text Info element, with frame 3 WEP encrypted
	 */
#ifdef WLWSEC
	if ((auth_status == DOT11_SC_SUCCESS) &&
		(auth_alg == DOT11_SHARED_KEY) && (auth_seq == 3)) {
		key = wlc_keymgmt_get_scb_key(wlc->keymgmt, scb, WLC_KEY_ID_PAIRWISE,
			WLC_KEY_FLAG_NONE, &key_info);
		if (key_info.algo == CRYPTO_ALGO_OFF) {
			wlc_bsscfg_t *tx_cfg;
			tx_cfg = (BSSCFG_PSTA(cfg)) ? wlc_bsscfg_primary(wlc) : cfg;
			BCM_REFERENCE(tx_cfg);
			key = wlc_keymgmt_get_bss_tx_key(wlc->keymgmt, tx_cfg, FALSE, &key_info);
		}

		if (key_info.algo == CRYPTO_ALGO_OFF) {
			if (tx_cplt_fn != NULL) {
				(tx_cplt_fn)(wlc, TX_STATUS_NO_ACK, tx_cplt_ctx);
			}
			WL_ERROR(("wl%d: transmit key absent\n", wlc->pub->unit));
			return NULL;
		}
	}
#endif /* WLWSEC */

	/* prepare the IE mgmt calls, and calculate IE's length */
	bzero(&ftcbparm, sizeof(ftcbparm));
	ftcbparm.auth.alg = auth_alg;
	ftcbparm.auth.seq = auth_seq;
	ftcbparm.auth.scb = scb;
	ftcbparm.auth.challenge = challenge_text;
	bzero(&cbparm, sizeof(cbparm));
	cbparm.ft = &ftcbparm;

	body_len += wlc_iem_calc_len(wlc->iemi, cfg, FC_AUTH, NULL, &cbparm);
	fc = FC_AUTH;
	if (key_info.algo != CRYPTO_ALGO_OFF && auth_alg == DOT11_SHARED_KEY)
		fc |= FC_WEP;

	/* allocate a packet */
	pkt = wlc_frame_get_mgmt_ex(wlc, fc, ea, &cfg->cur_etheraddr,
		bssid, body_len, &pbody, key_info.iv_len, key_info.icv_len);
	if (pkt == NULL) {
		WL_ERROR(("wl%d: %s: pkt alloc failed\n", wlc->pub->unit, __FUNCTION__));
		/* XXX PR84212: pkt can only be NULL if packet alloc failed, give
		 * up now rather calling wlc_auth_tx_complete() to simulate frame
		 * if (tx_cplt_fn != NULL) {
		 *	(tx_cplt_fn)(wlc, TX_STATUS_NO_ACK, tx_cplt_ctx);
		 * }
		 */
		return NULL;
	}

	/* Authentication request frame statistics counters:
	 * Auth request frame allocated by directly calling to wlc_frame_get_mgmt_ex(), which
	 * is not in counter maintaining path. Add maintain code here to fix incorrect counter.
	 */
	WLCNTINCR(wlc->pub->_cnt->txauth);

	if (tx_cplt_fn != NULL &&
	    wlc_pcb_fn_register(wlc->pcb, tx_cplt_fn, tx_cplt_ctx, pkt) != BCME_OK) {
		PKTFREE(wlc->osh, pkt, TRUE);
		return NULL;
	}

	/* init fixed portion */
	auth = (struct dot11_auth *)pbody;
	auth->alg = htol16((uint16)auth_alg);
	auth->seq = htol16((uint16)auth_seq);
	auth->status = htol16((uint16)auth_status);

	pbody += sizeof(struct dot11_auth);
	body_len -= sizeof(struct dot11_auth);

	/* generate IEs */
	if (wlc_iem_build_frame(wlc->iemi, cfg, FC_AUTH, NULL, &cbparm,
	                        pbody, body_len) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_build_frame failed\n",
		          wlc->pub->unit, __FUNCTION__));
/* XXX should bail out here if there is any error when building the frame...
 * but to keep the behavior as before the IE mgmt. module let's keep going
		PKTFREE(wlc->osh, pkt, TRUE);
		return NULL;
 */
	}
	if (auth_status == DOT11_SC_SUCCESS &&
	    ftcbparm.auth.status != DOT11_SC_SUCCESS) {
		WL_INFORM(("wl%d: %s: wlc_iem_build_frame status %u\n",
		           wlc->pub->unit, __FUNCTION__, ftcbparm.auth.status));
		auth->status = htol16(ftcbparm.auth.status);
	}

#ifdef SIM_QUE_FAIL
	/* simulate queueing failure */
	PKTFREE(wlc->osh, pkt, TRUE);
	return NULL;
#endif // endif

#if defined(BCMDBG) || defined(WLMSG_PRPKT)
	WL_ASSOC(("wl%d: JOIN: sending Auth %d ...\n", WLCWLUNIT(wlc), auth_seq));

	if (WL_ASSOC_ON()) {
		struct dot11_management_header *h =
		        (struct dot11_management_header *)PKTDATA(wlc->osh, pkt);
		uint l = PKTLEN(wlc->osh, pkt);
		wlc_print_auth(wlc, h, l);
	}
#endif // endif
	/* As an AP, send using the bsscfg queue so that the auth response will go out on the bsscfg
	 * channel.
	 *
	 * As a STA, send using the active_queue instead of the bsscfg queue. When roaming, the
	 * bsscfg queue will be associated with the current associated channel, not the roam target
	 * channel.
	 */
	if (BSSCFG_AP(cfg))
		qi = cfg->wlcif->qi;
	else
		qi = wlc->active_queue;
	ASSERT(qi != NULL);

	if (pre_send_fn) {
		if (pre_send_fn(wlc, cfg, data, pkt)) {
			PKTFREE(wlc->osh, pkt, TRUE);
			return NULL;
		}
	 }
	if (!wlc_queue_80211_frag(wlc, pkt, qi, scb, cfg, short_preamble, key,
		(scb ? WLC_LOWEST_SCB_RSPEC(scb) : WLC_LOWEST_BAND_RSPEC(wlc->band)))) {
		WL_ERROR(("wl%d: %s: wlc_queue_80211_frag failed\n",
		          wlc->pub->unit, __FUNCTION__));
		return NULL;
	}

	WL_EVENT_LOG((EVENT_LOG_TAG_TRACE_WL_INFO,
		(cfg->assoc->type == AS_ROAM) ?
		TRACE_ROAM_AUTH_STARTED : TRACE_FW_AUTH_STARTED));

#ifdef BCM_CEVENT
	if (CEVENT_STATE(wlc->pub)) {
		wlc_send_cevent(wlc, cfg, SCB_EA(scb), (uint16)auth_status, 0, 0, NULL, 0,
				CEVENT_D2C_ST_AUTH_TX,
				CEVENT_D2C_FLAG_QUEUED | CEVENT_FRAME_DIR_TX);
	}
#endif /* BCM_CEVENT */

	return pkt;
} /* wlc_sendauth */

int
wlc_senddisassoc(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct scb *scb,
	const struct ether_addr *da, const struct ether_addr *bssid,
	const struct ether_addr *sa, uint16 reason_code)
{
	int ret = BCME_OK;
	ret = wlc_senddisassoc_ex(wlc, cfg, scb, da, bssid,
		sa, reason_code, NULL, NULL, NULL);
	return ret;
}

int
wlc_senddisassoc_ex(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct scb *scb,
	const struct ether_addr *da, const struct ether_addr *bssid,
	const struct ether_addr *sa, uint16 reason_code,
	int (*pre_send_fn)(wlc_info_t *wlc, wlc_bsscfg_t *cfg, void *pkt,
	void* arg, void *extra_arg),
	void *arg, void *extra_arg)
{
	void *pkt = NULL;
	uint8 *pbody;
	uint body_len;
	uint16 *reason;

	ASSERT(cfg != NULL);

	/*
	 * get a packet - disassoc pkt has the following contents:
	 * 2 bytes Reason Code
	 */
	body_len = sizeof(uint16);

	/* calculate IEs' length */
	body_len += wlc_iem_calc_len(wlc->iemi, cfg, FC_DISASSOC, NULL, NULL);

	if ((pkt = wlc_frame_get_mgmt(wlc, FC_DISASSOC, da, sa, bssid,
	                              body_len, &pbody)) == NULL) {
		WL_ERROR(("wl%d: %s: wlc_frame_get_mgmt failed\n",
		          wlc->pub->unit, __FUNCTION__));
		return BCME_ERROR;
	}
	WLPKTTAG(pkt)->flags3 |= WLF3_NO_PMCHANGE;

	/* fill out the disassociation reason code */
	reason = (uint16 *) pbody;
	reason[0] = htol16(reason_code);

	pbody += sizeof(uint16);
	body_len -= sizeof(uint16);

	/* generate IEs */
	if (wlc_iem_build_frame(wlc->iemi, cfg, FC_DISASSOC, NULL, NULL,
	                        pbody, body_len) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_build_frame failed\n",
		          wlc->pub->unit, __FUNCTION__));
		PKTFREE(wlc->osh, pkt, TRUE);
		return BCME_ERROR;
	}

	if (pre_send_fn) {
		if (pre_send_fn(wlc, cfg, pkt, arg, extra_arg)) {
			PKTFREE(wlc->osh, pkt, TRUE);
			return BCME_ERROR;
		}
	}

#ifdef BCM_CEVENT
	if (CEVENT_STATE(wlc->pub)) {
		wlc_send_cevent(wlc, cfg, SCB_EA(scb), 0,
				reason_code, 0, NULL, 0, CEVENT_D2C_ST_DISASSOC_TX,
				CEVENT_D2C_FLAG_QUEUED | CEVENT_FRAME_DIR_TX);
	}
#endif /* BCM_CEVENT */

	if (wlc_sendmgmt(wlc, pkt, cfg->wlcif->qi, scb))
		return BCME_OK;

	if (BSS_SMFS_ENAB(wlc, cfg)) {
		(void)wlc_smfs_update(wlc->smfs, cfg, SMFS_TYPE_DISASSOC_TX, reason_code);
	}
	return BCME_ERROR;

} /* wlc_senddisassoc */

/**
 * For non-WMM association: sends a Null Data frame.
 *
 * For WMM association: if prio is -1, sends a Null Data frame;
 * otherwise sends a QoS Null frame with priority prio.
 */
void *
wlc_alloc_nulldata(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb,
                 uint32 pktflags, int prio)
{
	osl_t *osh = wlc->osh;
	void *p;
	uint16 *pqos;
	uint16 fc;
	bool qos;
	int len;
	struct dot11_header *hdr;
	uint32 htc_code;

	ASSERT(bsscfg != NULL);
	ASSERT(scb != NULL);

	/* allocate the packet */
	len = ROUNDUP(TXOFF, 4);
	if ((p = PKTGET(osh, len, TRUE)) == NULL) {
		WL_ERROR(("wl%d: wlc_alloc_nulldata: pktget error for len %d \n",
		          wlc->pub->unit, len));
		WLCNTINCR(wlc->pub->_cnt->txnobuf);
		return (NULL);
	}
	ASSERT(ISALIGNED((uintptr)PKTDATA(osh, p), sizeof(uint32)));

	/* assuming non-QOS */
	if (prio == PKTPRIO_NON_QOS_HIGH) {
		prio = PRIO_8021D_VO;
		qos = FALSE;
	}
	else if (prio == PKTPRIO_NON_QOS_DEFAULT) {
		prio = PRIO_8021D_BE;
		qos = FALSE;
	}
	else {
		/* valid qos prio */
		qos = SCB_QOS(scb);
	}

	/* Null Data or QoS Null Data are either 3 address or 4 address format,
	 * start with 3 address first...
	 */
	len = DOT11_A3_HDR_LEN;

	htc_code = 0;
	if (qos) {
		fc = FC_QOS_NULL;
		len += DOT11_QOS_LEN;
		if (wlc_he_htc_tx(wlc, scb, p, &htc_code)) {
			fc |= FC_ORDER;
			len += DOT11_HTC_LEN;
		}
	} else {
		fc = FC_NULL_DATA;
	}

	if (SCB_A4_NULLDATA(scb)) {
		fc |= FC_FROMDS | FC_TODS;
		len += ETHER_ADDR_LEN;
	} else if (BSSCFG_INFRA_STA(bsscfg)) {
		fc |= FC_TODS;
	} else if (BSSCFG_AP(bsscfg)) {
		fc |= FC_FROMDS;
	}

	/* reserve TXOFF - len bytes of headroom */
	len = TXOFF - len;
	PKTPULL(osh, p, len);

	/* init dot11 header fields */
	hdr = (struct dot11_header *)PKTDATA(osh, p);
	hdr->fc = htol16(fc);
	hdr->durid = 0;
	memcpy(&hdr->a1, &scb->ea, ETHER_ADDR_LEN);
	memcpy(&hdr->a2, &bsscfg->cur_etheraddr, ETHER_ADDR_LEN);
	if (SCB_A4_NULLDATA(scb)) {
		memcpy(&hdr->a3, &scb->ea, ETHER_ADDR_LEN);
		memcpy(&hdr->a4, &bsscfg->cur_etheraddr, ETHER_ADDR_LEN);
		pqos = (uint16 *)((uint8 *)hdr + DOT11_A4_HDR_LEN);
	} else {
		memcpy(&hdr->a3, &bsscfg->BSSID, ETHER_ADDR_LEN);
		pqos = (uint16 *)((uint8 *)hdr + DOT11_A3_HDR_LEN);
	}
	hdr->seq = 0;

	WLPKTTAG(p)->flags |= pktflags;

	PKTSETPRIO(p, prio);

	if (qos) {
		uint16 vqos;
		wlc_wme_t *wme = bsscfg->wme;

		/* Initialize the QoS Control field that comes after all addresses. */
		if (wme->wme_noack == QOS_ACK_NO_ACK)
			WLPKTTAG(p)->flags |= WLF_WME_NOACK;
		vqos = ((prio << QOS_PRIO_SHIFT) & QOS_PRIO_MASK) |
		        ((wme->wme_noack << QOS_ACK_SHIFT) & QOS_ACK_MASK);
#ifdef WLTDLS
		if (BSS_TDLS_ENAB(wlc, bsscfg) &&
#ifdef AP
		    SCB_PS(scb) &&
		    !wlc_apps_scb_apsd_cnt(wlc, scb) &&
#endif // endif
		    TRUE) {
			vqos |= QOS_EOSP_MASK;
		}
#endif /* WLTDLS */
		*pqos = htol16(vqos);
		/* Insert 11ax HTC+ field if needed */
		if (htc_code) {
			uint32 *phtc;

			phtc = (uint32 *)((uint8 *)pqos + DOT11_QOS_LEN);
			*phtc = htol32(htc_code);
		}
	}

	return p;
} /* wlc_alloc_nulldata */

bool
wlc_sendnulldata(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct ether_addr *da,
	ratespec_t rate_override, uint32 pktflags, int prio,
	int (*pre_send_fn)(wlc_info_t*, wlc_bsscfg_t*, void*, void *), void *data)
{
	void *p;
	struct scb *scb;
#if defined(BCMDBG) || defined(WLMSG_PS)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif // endif
	int err;
	bool result;

	WL_TRACE(("wl%d: %s: DA %s rspec 0x%08X pktflags 0x%08X prio %d bsscfg %p\n",
		wlc->pub->unit, __FUNCTION__,
		bcm_ether_ntoa(da, eabuf), rate_override,
		pktflags, prio, OSL_OBFUSCATE_BUF(bsscfg)));

	ASSERT(bsscfg != NULL);
	if ((scb = wlc_scbfind(wlc, bsscfg, da)) == NULL)
		return FALSE;

#ifdef STA
	/* If we're in the middle of a join, don't send any null data frames.
	 * It could end up being sent between the time we sent the auth req to
	 * the time the AP received our assoc req.
	 * If that happened, the AP will disassoc this cfg.
	 */
	if ((wlc->block_datafifo & DATA_BLOCK_JOIN) && (AS_IN_PROGRESS_CFG(wlc) == bsscfg)) {
		WL_ASSOC(("wl%d.%d: %s trying to send null data during JOIN, discard\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), __FUNCTION__));
		return FALSE;
	}
#endif /* STA */
	if ((p = wlc_alloc_nulldata(wlc, bsscfg, scb, pktflags, prio)) == NULL)
		return FALSE;

	/* If caller registered function to process pkt buffer before send, call it */
	if (pre_send_fn)
		if ((err = pre_send_fn(wlc, bsscfg, p, data))) {
			WL_ERROR(("%s: callback failed with error %d\n", __FUNCTION__, err));
			return FALSE;
		}

	WL_PS(("wl%d.%d: sending null frame to %s\n", wlc->pub->unit,
		WLC_BSSCFG_IDX(bsscfg), bcm_ether_ntoa(da, eabuf)));

	if ((wlc->lifetime[WME_PRIO2AC(PKTPRIO(p))]) && !(WLPKTTAG(p)->flags & WLF_EXPTIME))
		wlc_lifetime_set(wlc, p, (wlc->lifetime[WME_PRIO2AC(PKTPRIO(p))]));

	result = wlc_sendctl(wlc, p, bsscfg->wlcif->qi, scb, rate_override, FALSE);

	return result;
} /* wlc_sendnulldata */

#ifdef STA
static int wlc_sendpmnotif_cb(wlc_info_t *wlc, wlc_bsscfg_t *cfg, void *pkt, void *data)
{
	bool track = (bool)(uintptr) data;
	BCM_REFERENCE(wlc);
	BCM_REFERENCE(cfg);
	/* Sometimes this Null data packet is not sent to the air and read back
	 * from txfifo in wlc_tx_fifo_sync_complete(), because other packets
	 * already in txfifo have been transmitted and called wlc_update_pmstate()
	 * in wlc_dotxstatus().
	 * In case it initiates excursion start, then we don't have to re-enqueue
	 * this null data packet as PM indication is already done.
	 */
	WLPKTTAG(pkt)->flags3 |= WLF3_TXQ_SHORT_LIFETIME;

	/* register the completion callback */
	if (track)
		WLF2_PCB1_REG(pkt, WLF2_PCB1_PM_NOTIF);
	return BCME_OK;
}

static bool
wlc_sendpmnotif(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct ether_addr *da,
	ratespec_t rate_override, int prio, bool track)
{
#if defined(BCMDBG)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif // endif
	WL_TRACE(("wl%d: %s: DA %s rspec 0x%08X prio %d cfg %p\n", wlc->pub->unit, __FUNCTION__,
	          bcm_ether_ntoa(da, eabuf), rate_override, prio, OSL_OBFUSCATE_BUF(cfg)));

	return wlc_sendnulldata(wlc, cfg, da, rate_override, 0, prio,
		wlc_sendpmnotif_cb, (void *)(uintptr)track);
}
#endif /* STA */

void *
wlc_nulldata_template(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct ether_addr *da)
{
	void *pkt;
	bool shortpreamble;
	struct scb *scb;
	ratespec_t rspec;

	ASSERT(bsscfg != NULL);

	if ((scb = wlc_scbfind(wlc, bsscfg, da)) == NULL)
		return NULL;

	pkt = wlc_alloc_nulldata(wlc, bsscfg, scb, 0, -1);
	if (pkt == NULL)
		return NULL;

	if (wlc->cfg->PLCPHdr_override == WLC_PLCP_LONG)
		shortpreamble = FALSE;
	else
		shortpreamble = TRUE;

	rspec = wlc_lowest_basic_rspec(wlc, &wlc->cfg->current_bss->rateset);
	ASSERT(wlc_valid_rate(wlc, rspec,
	                      CHSPEC_IS2G(wlc->cfg->current_bss->chanspec) ?
	                      WLC_BAND_2G : WLC_BAND_5G, TRUE));

	/* add headers */
	wlc_d11hdrs(wlc, pkt, scb, shortpreamble, 0, 1,
	            TX_DATA_FIFO, 0, NULL, NULL, rspec);
	return pkt;
}

#ifdef STA
static int wlc_rateprobe_cb(wlc_info_t *wlc, wlc_bsscfg_t *cfg, void *pkt, void *data)
{
	BCM_REFERENCE(cfg);
	BCM_REFERENCE(data);
	BCM_REFERENCE(wlc);
	WLF2_PCB1_REG(pkt, WLF2_PCB1_RATE_PRB);
	return BCME_OK;
}

void
wlc_rateprobe(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct ether_addr *ea, ratespec_t rate_override)
{
	wlc_sendnulldata(wlc, cfg, ea, rate_override, 0, PRIO_8021D_BE, wlc_rateprobe_cb, NULL);
}

/** Called after a tx completion of a 'rate probe' (a NULL data frame) */
static void
wlc_rateprobe_complete(wlc_info_t *wlc, void *pkt, uint txs)
{
	struct scb *scb;
	wlc_bsscfg_t *bsscfg;
	uint8 mcsallow = 0;

	/* no ack */
	if (!(txs & TX_STATUS_ACK_RCV))
		return;

	/* make sure the scb is still around */
	if ((scb = WLPKTTAGSCBGET(pkt)) == NULL)
		return;

	bsscfg = SCB_BSSCFG(scb);
	ASSERT(bsscfg != NULL);

	/* upgrade the scb rateset to all supported network rates */
	if (SCB_HT_CAP(scb))
		mcsallow |= WLC_MCS_ALLOW;
	if (SCB_VHT_CAP(scb))
		mcsallow |= WLC_MCS_ALLOW_VHT;

	if (WLPROPRIETARY_11N_RATES_ENAB(wlc->pub) && SCB_HT_PROP_RATES_CAP(scb))
		mcsallow |= WLC_MCS_ALLOW_PROP_HT;

	if ((mcsallow & WLC_MCS_ALLOW_VHT) &&
		WLC_VHT_FEATURES_GET(wlc->pub, WL_VHT_FEATURES_1024QAM) &&
		SCB_1024QAM_CAP(scb))
		mcsallow |= WLC_MCS_ALLOW_1024QAM;

	if (SCB_HE_CAP(scb)) {
		mcsallow |= WLC_MCS_ALLOW_HE;
	}

	wlc_rateset_filter((bsscfg->associated ? &bsscfg->current_bss->rateset :
		&bsscfg->target_bss->rateset) /* src */, &scb->rateset /* dst */,
		FALSE, WLC_RATES_CCK_OFDM, RATE_MASK, mcsallow);
	wlc_scb_ratesel_init(wlc, scb);
}

/** Called after a tx completion of a 'rate probe' (a NULL data frame) */
static void
wlc_rateprobe_scan(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	struct scb *scb;
	struct scb_iter scbiter;

	/*
	 * Send ofdm rate probe to any station
	 * which does not yet have an ofdm rate in its scb rateset
	 * and has been active within the past 60 seconds.
	 */

	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {
		if (!RATE_ISOFDM(scb->rateset.rates[scb->rateset.count-1]) &&
		    ((wlc->pub->now - scb->used) < 60))
			wlc_rateprobe(wlc, cfg, &scb->ea, WLC_RATEPROBE_RSPEC);
	}
}

bool
wlc_sendpspoll(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	void *p;
	struct scb *scb;
	struct ether_addr *bssid = &bsscfg->BSSID;
#if defined(BCMDBG) || defined(BCMDBG_ERR)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif // endif

	if (!(scb = wlc_scbfind(wlc, bsscfg, bssid))) {
		WL_ERROR(("wl%d: %s: wlc_scbfind failed, BSSID %s bandunit %d\n",
		          wlc->pub->unit, __FUNCTION__, bcm_ether_ntoa(bssid, eabuf),
		          wlc->band->bandunit));
		return FALSE;
	}

	/* If a pspoll is still pending, don't send another one */
	if (scb->flags & SCB_PENDING_PSPOLL)
		return TRUE;

	if ((p = wlc_frame_get_ps_ctl(wlc, bsscfg, bssid, &bsscfg->cur_etheraddr)) == NULL) {
		WL_ERROR(("wl%d: %s: wlc_frame_get_ps_ctl failed\n", wlc->pub->unit, __FUNCTION__));
		return FALSE;
	}

	/* Force d11 to wake */
	wlc_set_pspoll(bsscfg, TRUE);

	/* register the completion callback */
	WLF2_PCB1_REG(p, WLF2_PCB1_PSP);

	/* wlc_sendctl -> wlc_send_q may drop the packet without notifying the caller.
	 * To take care of this set the SCB_PENDING_PSPOLL before wlc_sendctl() and
	 * verify the flag after return to confirm packet is not dropped
	*/
	scb->flags |= SCB_PENDING_PSPOLL;

	if (!wlc_sendctl(wlc, p, bsscfg->wlcif->qi, scb, 0, FALSE)) {
		WL_ERROR(("wl%d: %s: wlc_sendctl failed\n", wlc->pub->unit, __FUNCTION__));

		/* Failed, allow d11 to sleep */
		goto fail;
	}

	if ((scb->flags & SCB_PENDING_PSPOLL) !=
		SCB_PENDING_PSPOLL) {
		WL_ERROR(("Ps-Poll Tx Failed\n"));
		goto fail;
	}
	WLCNTINCR(wlc->pub->_cnt->txpspoll);

#ifdef WLP2P
	if (BSS_P2P_ENAB(wlc, bsscfg))
		wlc_p2p_pspoll_resend_upd(wlc->p2p, bsscfg, TRUE);
#endif // endif
	return TRUE;
fail:
	/* Ps-Poll TX failed, restore the flags */
	scb->flags &= ~SCB_PENDING_PSPOLL;

	/* Failed, allow d11 to sleep */
	wlc_set_pspoll(bsscfg, FALSE);
	return FALSE;
} /* wlc_sendpspoll */

static void
wlc_sendpspoll_complete(wlc_info_t *wlc, void *pkt, uint txstatus)
{
	struct scb *scb;
#if defined(BCMDBG) || defined(BCMDBG_ERR)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || BCMDBG_ERR */
	BCM_REFERENCE(wlc);

	/* Is this scb still around */
	if ((scb = WLPKTTAGSCBGET(pkt)) == NULL)
		return;

	if ((txstatus & TX_STATUS_MASK) == TX_STATUS_NO_ACK) {
		wlc_bsscfg_t *cfg;

		WL_ERROR(("wl%d: %s: no ACK from %s for PS-Poll\n",
		          wlc->pub->unit, __FUNCTION__, bcm_ether_ntoa(&scb->ea, eabuf)));

		cfg = SCB_BSSCFG(scb);
		ASSERT(cfg != NULL);

		wlc_set_pspoll(cfg, FALSE);
	}

	scb->flags &= ~SCB_PENDING_PSPOLL;
}

#ifndef ATE_BUILD
static void
wlc_pm2_radio_shutoff_dly_timer(void *arg)
{
	wlc_info_t *wlc = (wlc_info_t *)arg;

	wlc->pm2_radio_shutoff_pending = FALSE;
	wlc_set_wake_ctrl(wlc);
	return;
}

static void
wlc_wake_event_timer(void *arg)
{
	wlc_info_t *wlc = (wlc_info_t *)arg;

	wlc->wake_event_timeout = 0;
	wlc_mac_event(wlc, WLC_E_WAKE_EVENT, NULL, WLC_E_STATUS_SUCCESS, 0, 0, 0, 0);
	return;
}
#endif /* ATE_BUILD */
#endif /* STA */

void *
wlc_senddeauth(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct scb *scb,
	struct ether_addr *da, struct ether_addr *bssid,
	struct ether_addr *sa, uint16 reason_code)
{
	void *pkt;
	uint8 *pbody;
	uint body_len;
	uint16 *reason;

	ASSERT(cfg != NULL);

	/* get a packet - deauth pkt has the following contents: 2 bytes Reason Code */
	body_len = sizeof(uint16);

	/* calculate IEs' length */
	body_len += wlc_iem_calc_len(wlc->iemi, cfg, FC_DEAUTH, NULL, NULL);

	/* allocate a packet */
	if ((pkt = wlc_frame_get_mgmt(wlc, FC_DEAUTH, da, sa, bssid,
	                              body_len, &pbody)) == NULL) {
		WL_ERROR(("wl%d: %s: wlc_frame_get_mgmt failed\n",
		          wlc->pub->unit, __FUNCTION__));
		return NULL;
	}

	/* fill out the deauthentication reason code */
	reason = (uint16 *)pbody;
	reason[0] = htol16(reason_code);

	pbody += sizeof(uint16);
	body_len -= sizeof(uint16);

	/* generate IEs */
	if (wlc_iem_build_frame(wlc->iemi, cfg, FC_DEAUTH, NULL, NULL,
	                        pbody, body_len) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_build_frame failed\n",
		          wlc->pub->unit, __FUNCTION__));
		PKTFREE(wlc->osh, pkt, TRUE);
		return NULL;
	}

#ifdef BCM_CEVENT
	if (CEVENT_STATE(wlc->pub) && !SCB_INTERNAL(scb)) {
		wlc_send_cevent(wlc, cfg, SCB_EA(scb), 0,
				reason_code, 0, NULL, 0, CEVENT_D2C_ST_DEAUTH_TX,
				CEVENT_D2C_FLAG_QUEUED | CEVENT_FRAME_DIR_TX);
	}
#endif /* BCM_CEVENT */

	if (wlc_sendmgmt(wlc, pkt, cfg->wlcif->qi, scb))
		return pkt;

	if (BSS_SMFS_ENAB(wlc, cfg)) {
		(void)wlc_smfs_update(wlc->smfs, cfg, SMFS_TYPE_DEAUTH_TX, reason_code);
	}

	return NULL;
} /* wlc_senddeauth */

#ifdef STA
uint16
wlc_assoc_capability(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, wlc_bss_info_t *bi)
{
	wlc_assoc_t *as;
	uint16 capability;

	ASSERT(bsscfg != NULL);

	as = bsscfg->assoc;

	/* Capability information */
	if (as->capability) {
		return htol16(as->capability);
	} else {
		capability = DOT11_CAP_ESS;
		if (WSEC_ENABLED(bsscfg->wsec))
			capability |= DOT11_CAP_PRIVACY;
		if (BAND_2G(wlc->band->bandtype) &&
		    ((bsscfg->PLCPHdr_override == WLC_PLCP_SHORT) ||
		     (bsscfg->PLCPHdr_override == WLC_PLCP_AUTO)))
			capability |= DOT11_CAP_SHORT;
		/* Nokia A032 AP (11b-only AP) rejects association req if
		 * capability - [short slot time, spectrum management] bit is set.
		 * The solution for interoperability
		 * with such AP is to
		 * (1) set DOT11_CAP_SHORTSLOT only for gmode-ofdm case
		 * (2) set DOT11_CAP_SPECTRUM only if beacon has it set.
		 */
		if (wlc->band->gmode &&
		    wlc_rateset_isofdm(bi->rateset.count, bi->rateset.rates) &&
		    (wlc->shortslot_override != WLC_SHORTSLOT_OFF))
			capability |= DOT11_CAP_SHORTSLOT;
		if (BSS_WL11H_ENAB(wlc, bsscfg) &&
		    (bi->capability & DOT11_CAP_SPECTRUM))
			capability |= DOT11_CAP_SPECTRUM;
#ifdef WL11K
		if (WL11K_ENAB(wlc->pub) &&
			wlc_rrm_enabled(wlc->rrm_info, bsscfg) &&
			(bi->capability & DOT11_CAP_RRM))
			capability |= DOT11_CAP_RRM;
#endif // endif
		capability = htol16(capability);
	}

	return capability;
} /* wlc_assoc_capability */

static bool
wlc_assoc_req_ins_cb(void *ctx, wlc_iem_ins_data_t *data)
{
	/* insert all non Vendor Specific IEs that haven't been
	 * proclaimed so far before Vendor Specific IEs
	 */
	BCM_REFERENCE(ctx);
	if (data->tag == DOT11_MNG_VS_ID)
		return TRUE;
	return FALSE;
}

static bool
wlc_assoc_req_mod_cb(void *ctx, wlc_iem_mod_data_t *data)
{
	/* modify Supported Rates, Extended Supported Rates and RSN IEs */
	BCM_REFERENCE(ctx);
	if (data->ie[TLV_TAG_OFF] == DOT11_MNG_RATES_ID ||
	    data->ie[TLV_TAG_OFF] == DOT11_MNG_EXT_RATES_ID ||
		data->ie[TLV_TAG_OFF] == DOT11_MNG_RSN_ID)
		return TRUE;
	/* insert other IEs as is */
	return FALSE;
}

void *
wlc_sendassocreq(wlc_info_t *wlc, wlc_bss_info_t *bi, struct scb *scb, bool reassoc)
{
	void *pkt = NULL;
	uint8 *pbody;
	uint body_len;
	struct dot11_assoc_req *assoc;
	uint16 type;
	wlc_bsscfg_t *bsscfg;
	wlc_assoc_t *as;
	wlc_rateset_t sup_rates, ext_rates;
	wlc_pre_parse_frame_t ppf;
	wlc_iem_uiel_t uiel, *puiel;
	wlc_iem_ft_cbparm_t ftcbparm;
	wlc_iem_cbparm_t cbparm;

	ASSERT(scb != NULL);

	bsscfg = SCB_BSSCFG(scb);
	ASSERT(bsscfg != NULL);

	as = bsscfg->assoc;

	body_len = DOT11_ASSOC_REQ_FIXED_LEN;
	/* Current AP address */
	if (reassoc)
		body_len += ETHER_ADDR_LEN;

	type = (uint16)(reassoc ? FC_REASSOC_REQ : FC_ASSOC_REQ);

	/* special handling: Rates Overrides */
	bzero(&sup_rates, sizeof(sup_rates));
	bzero(&ext_rates, sizeof(ext_rates));
	bzero(&ppf, sizeof(ppf));
	ppf.sup = &sup_rates;
	ppf.ext = &ext_rates;

	if (wlc_arq_pre_parse_frame(wlc, bsscfg, type, as->ie, as->ie_len, &ppf) != BCME_OK) {
		WL_ERROR(("%s: wlc_arq_pre_parse_frame failed\n", __FUNCTION__));
		return NULL;
	}

	if ((bi->flags & WLC_BSS_54G) &&
	    (wlc->band->gmode != GMODE_LEGACY_B)) {
		/* Legacy 54g AP, so ignore rates overrides and put
		 * all rates in the supported rates elt
		 */
		sup_rates.count = wlc->band->hw_rateset.count;
		bcopy(wlc->band->hw_rateset.rates, sup_rates.rates, wlc->band->hw_rateset.count);
#ifdef WLP2P
		if (BSS_P2P_ENAB(wlc, bsscfg))
			wlc_p2p_rateset_filter(wlc->p2p, bsscfg, &sup_rates);
#endif // endif
		ext_rates.count = 0;
	} else {
		/* create the ratesets with overrides taken into account
		 * target rateset has been filtered against our hw_rateset
		 */
		wlc_rateset_elts(wlc, bsscfg, &bi->rateset, &sup_rates, &ext_rates);
	}

	/* prepare IE mgmt calls */
	puiel = NULL;
	if (as->ie != NULL) {
		wlc_iem_build_uiel_init(wlc->iemi, &uiel);
		/* overrides */
		uiel.ies = as->ie;
		uiel.ies_len = as->ie_len;
		/* XXX 'ctx' isn't used in wlc_assoc_req_ins_cb or wlc_assoc_req_mod_cb
		 * so it's ok for now to make this partial override. We should fix it
		 * with a full override including vsie_fn and idext_fn and ctx otherwise.
		 */
		uiel.ins_fn = wlc_assoc_req_ins_cb;
		uiel.mod_fn = wlc_assoc_req_mod_cb;
		puiel = &uiel;
	}
	bzero(&ftcbparm, sizeof(ftcbparm));
	ftcbparm.assocreq.scb = scb;
	ftcbparm.assocreq.target = bi;
	ftcbparm.assocreq.sup = &sup_rates;
	ftcbparm.assocreq.ext = &ext_rates;
	bzero(&cbparm, sizeof(cbparm));
	cbparm.ft = &ftcbparm;
	/* Can we assume we are already on the right band at this point? */
	cbparm.bandunit = (uint8)wlc->band->bandunit;
	cbparm.ht = N_ENAB(wlc->pub) && SCB_HT_CAP(scb);
#ifdef WL11AC
	cbparm.vht = VHT_ENAB(wlc->pub) && SCB_VHT_CAP(scb);
#endif // endif
#ifdef WL11AX
	cbparm.he = HE_ENAB(wlc->pub) && SCB_HE_CAP(scb);
#endif // endif

#ifdef WL11AC
	if (cbparm.vht) {
		ftcbparm.assocreq.narrow_bw = CHSPEC_IS40(wlc->chanspec) ? NARROW_BW_40 :
			(CHSPEC_IS20(wlc->chanspec)?
				NARROW_BW_20 : NARROW_BW_NONE);
	}
#endif // endif

	/* calculate IEs length */
	body_len += wlc_iem_calc_len(wlc->iemi, bsscfg, type, puiel, &cbparm);

	/* get a control packet */
	if ((pkt = wlc_frame_get_mgmt(wlc, type, &bi->BSSID, &bsscfg->cur_etheraddr, &bi->BSSID,
	                              body_len, &pbody)) == NULL) {
		WL_ERROR(("wl%d: %s: wlc_frame_get_mgmt failed\n",
		          wlc->pub->unit, __FUNCTION__));
		return NULL;
	}

	/* fill out the association request body */
	assoc = (struct dot11_assoc_req *)pbody;

	/* Capability information */
	assoc->capability = wlc_assoc_capability(wlc, bsscfg, bi);

	/* Listen interval */
	assoc->listen = htol16(as->listen);

	pbody += DOT11_ASSOC_REQ_FIXED_LEN;
	body_len -= DOT11_ASSOC_REQ_FIXED_LEN;

	/* Current AP address */
	if (reassoc) {
		if (!ETHER_ISNULLADDR(&as->bssid))
			bcopy(&as->bssid, pbody, ETHER_ADDR_LEN);
		else
			bcopy(&bsscfg->prev_BSSID, pbody, ETHER_ADDR_LEN);
		pbody += ETHER_ADDR_LEN;
		body_len -= ETHER_ADDR_LEN;
	}

	/* generate IEs */
	if (wlc_iem_build_frame(wlc->iemi, bsscfg, type, puiel, &cbparm,
	                        pbody, body_len) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_build_frame failed\n",
		          wlc->pub->unit, __FUNCTION__));
		PKTFREE(wlc->osh, pkt, TRUE);
		return NULL;
	}

	wlc_bss_mac_event(wlc, bsscfg, WLC_E_ASSOC_REQ_IE, NULL, 0, 0, 0, pbody, body_len);

	/* Save a copy of the last association request */
	if (as->req != NULL) {
		MFREE(wlc->osh, as->req, as->req_len);
		as->req = NULL;
	}

	if (reassoc) {
		pbody -= ETHER_ADDR_LEN;
		body_len += ETHER_ADDR_LEN;
			}
	pbody -= DOT11_ASSOC_REQ_FIXED_LEN;
	body_len += DOT11_ASSOC_REQ_FIXED_LEN;
	as->req_is_reassoc = reassoc;
	if ((as->req = MALLOC(wlc->osh, body_len)) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		as->req_len = 0;
	} else {
		bcopy(pbody, as->req, body_len);
		as->req_len = body_len;
	}

#ifdef WL_EVENT_LOG_COMPILE
	{
		wl_event_log_tlv_hdr_t tlv_log = {{0, 0}};

		tlv_log.tag = TRACE_TAG_BSSID;
		tlv_log.length = 6;

		WL_EVENT_LOG((EVENT_LOG_TAG_TRACE_WL_INFO,
			reassoc ? TRACE_FW_RE_ASSOC_STARTED : TRACE_FW_ASSOC_STARTED, tlv_log.t,
			ETHER_ADDR_PACK_LOW(&bi->BSSID), ETHER_ADDR_PACK_HI(&bi->BSSID)));
	}
#endif // endif
#if defined(BCMDBG) || defined(WLMSG_PRPKT)
	WL_ASSOC(("wl%d: JOIN: sending %s REQ ...\n",
	            WLCWLUNIT(wlc), (reassoc ? "REASSOC" : "ASSOC")));

	if (WL_ASSOC_ON()) {
		struct dot11_management_header *h =
		        (struct dot11_management_header *)PKTDATA(wlc->osh, pkt);
		uint l = PKTLEN(wlc->osh, pkt);
		wlc_print_assoc_req(wlc, h, l);
	}
#endif // endif
	/* Send using the active_queue instead of the bsscfg queue. When roaming, the bsscfg queue
	 * will be associated with the current associated channel, not the roam target channel.
	 */
	if (!wlc_queue_80211_frag(wlc, pkt, wlc->active_queue, scb, bsscfg,
	                ((bi->capability & DOT11_CAP_SHORT) != 0),
	                NULL, WLC_LOWEST_SCB_RSPEC(scb))) {
		WL_ERROR(("wl%d: %s: wlc_queue_80211_frag failed\n",
		          wlc->pub->unit, __FUNCTION__));
		return NULL;
	}

#ifdef BCM_CEVENT
	if (CEVENT_STATE(wlc->pub)) {
		wlc_send_cevent(wlc, bsscfg, SCB_EA(scb), 0, 0, 0, NULL, 0,
				CEVENT_D2C_ST_ASSOC_TX,
				CEVENT_D2C_FLAG_QUEUED | CEVENT_FRAME_DIR_TX);
	}
#endif /* BCM_CEVENT */

	return pkt;
} /* wlc_sendassocreq */

/** Enable/Disable IBSS operations in hardware */
static void
wlc_ibss_hw_enab(wlc_info_t *wlc, wlc_bsscfg_t *cfg, bool enab)
{
	uint32 infra;
	uint16 mhf;

	WL_TRACE(("wl%d.%d: wlc_ibss_hw_enab %d\n",
	          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), enab));

	ASSERT(!cfg->BSS);
	ASSERT(IBSS_HW_ENAB(cfg));

	if (enab) {
		/* enable bcn promisc mode */
		wlc_mac_bcn_promisc_update(wlc, BCNMISC_IBSS, TRUE);

		/* Switch back to IBSS STA mode */
		infra = 0;
		/* enable IBSS security mode */
		mhf = MHF4_IBSS_SEC;

		wlc_validate_bcn_phytxctl(wlc, cfg);
	} else {
		/* disable bcn promisc mode */
		wlc_mac_bcn_promisc_update(wlc, BCNMISC_IBSS, FALSE);
		/* Switch to BSS STA mode so that MAC will not send IBSS Beacons
		 * or respond to probe requests
		 */
		infra = MCTL_INFRA;
		/* disable IBSS security mode */
		mhf = 0;
	}

	wlc_mctrl(wlc, MCTL_INFRA, infra);
	if (IBSS_PEER_GROUP_KEY_ENAB(wlc->pub))
		wlc_mhf(wlc, MHF4, MHF4_IBSS_SEC, mhf, WLC_BAND_ALL);
} /* wlc_ibss_hw_enab */

void
wlc_ibss_enable_all(wlc_info_t *wlc)
{
	int idx;
	wlc_bsscfg_t *cfg;

#ifdef WL_PM_MUTE_TX
	wlc_pm_mute_tx_pm_pending_complete(wlc->pm_mute_tx);
#endif // endif

	FOREACH_AS_STA(wlc, idx, cfg) {
		if (cfg->BSS)
			continue;
		wlc_ibss_enable(cfg);
	}
}

void
wlc_ibss_disable_all(wlc_info_t *wlc)
{
	int idx;
	wlc_bsscfg_t *cfg;

	FOREACH_AS_STA(wlc, idx, cfg) {
		if (cfg->BSS)
			continue;
		wlc_ibss_disable(cfg);
	}
}

void
wlc_ibss_disable(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;

	ASSERT(!cfg->BSS);

	if (IBSS_HW_ENAB(cfg))
		wlc_ibss_hw_enab(wlc, cfg, FALSE);
	else
		wlc_ibss_mute_upd_notif(wlc, cfg, ON);

#ifdef WLMCNX
	if (MCNX_ENAB(wlc->pub)) {
		/* enable P2P mode host flag in BSS mode only */
		wlc_mhf(wlc, MHF5, MHF5_P2P_MODE, MHF5_P2P_MODE, WLC_BAND_ALL);
	}
#endif /* WLMCNX */
}

void
wlc_ibss_enable(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;

	ASSERT(!cfg->BSS);

	/* MHF5_P2P_MODE shouldn't be disabled for NAN because
	 *  - local TSF updated on RX beacon should be skipped
	 *  - and required to manage per-BSS TSF offset
	*/
	if (BSSCFG_NAN_MGMT(cfg) ||
	    BSSCFG_NAN_DATA(cfg)) {
		return;
	}

	if (IBSS_HW_ENAB(cfg))
		wlc_ibss_hw_enab(wlc, cfg, TRUE);
	else
		wlc_ibss_mute_upd_notif(wlc, cfg, OFF);

#ifdef WLMCNX
	if (MCNX_ENAB(wlc->pub)) {
		/* disable P2P mode host flag in IBSS mode to allow beacon coalescing */
		wlc_mhf(wlc, MHF5, MHF5_P2P_MODE, 0, WLC_BAND_ALL);
	}
#endif /* WLMCNX */
}

#ifdef WLWSEC
static void
wlc_tkip_countermeasures(wlc_info_t *wlc, void *pkt, uint txs)
{
	struct scb *scb;
	wlc_bsscfg_t *bsscfg;

	WL_WSEC(("wl%d: %s: TKIP countermeasures: tx status for 2nd"
		" MIC failure indication...\n", WLCWLUNIT(wlc), __FUNCTION__));

	/* make sure the scb still exists */
	if ((scb = WLPKTTAGSCBGET(pkt)) == NULL) {
		WL_ERROR(("wl%d: %s: unable to find scb from the pkt %p\n",
		          wlc->pub->unit, __FUNCTION__, OSL_OBFUSCATE_BUF(pkt)));
		return;
	}

	bsscfg = SCB_BSSCFG(scb);
	ASSERT(bsscfg != NULL);

	if ((txs & TX_STATUS_MASK) == TX_STATUS_NO_ACK) {
		WL_ERROR(("wl%d.%d: %s: No ack for TKIP countermeasures MIC report.\n",
			WLCWLUNIT(wlc), WLC_BSSCFG_IDX(bsscfg), __FUNCTION__));
	}

	/* notify keymgmt that we sent mic error report. It is up to the
	 * event handler to send deauth if necessary. This should be done
	 * only for STA BSSCFG
	 */
	wlc_keymgmt_notify(wlc->keymgmt, WLC_KEYMGMT_NOTIF_TKIP_CM_REPORTED,
		bsscfg, scb, NULL, pkt);
}
#endif /* WLWSEC */
/* Use this packet tx completion callback as the last resort to advance
 * the scan engine state machine.
 */
/* XXX TX_STATUS_PMINDCTD is processed in wlc_dotxstatus() with calling
 * wlc_update_pmstate() as an indication that PM state has been communicated
 * to the associated AP. However it is observed that sometimes with an
 * so far unknown reason the PMINDCTD bit isn't always set even when
 * the PM null data frame is successfully transmitted and ack'd, which
 * causes cfg->pm->PMpending hence wlc->PMpending to remain set, which
 * causes the scan engine to stuck at waiting for PM transition to finish.
 */
static void
wlc_pm_notif_complete(wlc_info_t *wlc, void *pkt, uint txs)
{
	if (SCAN_IN_PROGRESS(wlc->scan)) {
		wlc_bsscfg_t *cfg = WLC_BSSCFG(wlc, WLPKTTAGBSSCFGGET(pkt));

		if (cfg != NULL && cfg->pm->PMpending)
			wlc_bss_pm_pending_reset(cfg, txs);
	}
}

#ifdef BCMSUP_PSK
/** called from per-port mic_error handlers because they can't see sup ptr */
bool
wlc_sup_mic_error(wlc_bsscfg_t *cfg, bool group)
{
	return wlc_sup_send_micfailure(cfg->wlc->idsup, cfg, group);
}
#endif /* BCMSUP_PSK */
#endif /* STA */

static uint32
random_hash(uint32 rand_swinput, uint32 rand_hwinput)
{
	uint32 rand_sw;
	uint32 rand_hw;

	rand_sw = ((int)rand_swinput) >= 0 ? (2 * rand_swinput) : (-2 * rand_swinput - 1);
	rand_hw = ((int)rand_hwinput) >= 0 ? (2 * rand_hwinput) : (-2 * rand_hwinput - 1);

	return (rand_sw >= rand_hw ? (rand_sw * rand_sw + rand_sw + rand_hw) :
		(rand_sw + rand_hw * rand_hw));
}

/* It has been found that the random number generated by reading the hardware
 * register(tsf_random) starts getting repeated within 1 hour.
 * To make it more random and avoid any predictability in the random output,
 * we are first generating the seed value of 4 byte by combining the 2 byte
 * from the output of OSL_RAND and 2 byte output of reading the tsf_random
 * register.The seed value generated is then passed to hash function to generate
 * the random number.
 */
int
wlc_getrand(wlc_info_t *wlc, uint8 *buf, int buflen)
{
	uint8 i = 5, j = 24;
	uint16 temp_reg;
	uint32 rnd;

	if (buf != NULL) {
		while (buflen > 1) {
			if (!wlc->clk) {
				*buf++ = (OSL_RAND() >> i) & 0xFFu;
				*buf++ = (OSL_RAND() >> j) & 0xFFu;
			} else {
				temp_reg = R_REG(wlc->osh, D11_TSF_RANDOM(wlc));
				rnd = ((temp_reg & 0xFFFFu) << 16u) | ((temp_reg + i) & 0xFFFFu);
				*buf++ = (random_hash(OSL_RAND(), rnd) >> i) & 0xFFu;
				temp_reg = R_REG(wlc->osh, D11_TSF_RANDOM(wlc));
				rnd = ((temp_reg & 0xFFFFu) << 16u) | ((temp_reg - j) & 0xFFFFu);
				*buf++ = (random_hash(OSL_RAND(), rnd) >> j) & 0xFFu;
			}
			buflen -= 2;
			i = i + 5;
			j = j - 3;
			if ((i > 32) || (j == 3)) {
				i = i % 32;
				j = 24;
			}
		}
		/* Was buflen odd? */
		if (buflen == 1) {
			if (!wlc->clk) {
				*buf = (OSL_RAND() >> 16u) & 0xFFu;
			} else {
				temp_reg = R_REG(wlc->osh, D11_TSF_RANDOM(wlc));
				rnd = ((temp_reg & 0xFFFFu) << 16u) | ((temp_reg + 1) & 0xFFFFu);
				*buf = (random_hash(OSL_RAND(), rnd) >> 16u) & 0xFFu;
			}
		}
	}

	return BCME_OK;
}

static uint8
wlc_get_antennas(wlc_info_t *wlc)
{
	if (WLCISACPHY(wlc->band) && (ACREV_IS(wlc->band->phyrev, 32) ||
		ACREV_IS(wlc->band->phyrev, 33))) {
		/* 4365b1/4365c0 */
		return ANTENNA_NUM_4;
	}
	else if (WLCISACPHY(wlc->band))
		return ANTENNA_NUM_3;
	else if (WLCISNPHY(wlc->band))
		return ANTENNA_NUM_2;
	else if ((wlc->stf->ant_rx_ovr == ANT_RX_DIV_FORCE_0) ||
	    (wlc->stf->ant_rx_ovr == ANT_RX_DIV_FORCE_1))
		return ANTENNA_NUM_1;
	else
		return ANTENNA_NUM_2;
}

/** init tx reported rate mechanism */
void
wlc_reprate_init(wlc_info_t *wlc)
{
	int i;
	wlc_bsscfg_t *bsscfg;

	FOREACH_BSS(wlc, i, bsscfg) {
		wlc_bsscfg_reprate_init(bsscfg);
	}
}

/** Retrieve a consolidated set of revision information, typically for the WLC_GET_REVINFO ioctl */
static int
wlc_get_revision_info(wlc_info_t *wlc, void *buf, uint len)
{
	wlc_rev_info_t *rinfo = (wlc_rev_info_t *)buf;

	if (len < WL_REV_INFO_LEGACY_LENGTH)
		return BCME_BUFTOOSHORT;

	rinfo->vendorid = wlc->vendorid;
	rinfo->deviceid = wlc->deviceid;
	if (WLCISACPHY(wlc->band))
		rinfo->radiorev = (wlc->band->radiorev << IDCODE_ACPHY_REV_SHIFT) |
		                  (wlc->band->radioid << IDCODE_ACPHY_ID_SHIFT);
	else
		rinfo->radiorev = (wlc->band->radiorev << IDCODE_REV_SHIFT) |
		                  (wlc->band->radioid << IDCODE_ID_SHIFT);
	rinfo->chiprev = wlc->pub->sih->chiprev;
	rinfo->corerev = wlc->pub->corerev;
	rinfo->boardid = wlc->pub->sih->boardtype;
	rinfo->boardvendor = wlc->pub->sih->boardvendor;
	rinfo->boardrev = wlc->pub->boardrev;
	rinfo->ucoderev = wlc->ucode_rev;
	rinfo->driverrev = wlc->pub->cmn->driverrev;
	rinfo->drvrev_major	= wlc->pub->cmn->drvrev_major;
	rinfo->drvrev_minor	= wlc->pub->cmn->drvrev_minor;
	rinfo->drvrev_rc	= wlc->pub->cmn->drvrev_rc;
	rinfo->drvrev_rc_inc	= wlc->pub->cmn->drvrev_rc_inc;
	rinfo->bus = wlc->pub->sih->bustype;
	rinfo->chipnum = SI_CHIPID(wlc->pub->sih);

	if (len >= (OFFSETOF(wlc_rev_info_t, chippkg))) {
		rinfo->phytype = wlc->band->phytype;
		rinfo->phyrev = wlc->band->phyrev;
		rinfo->anarev = 0;	/* obsolete stuff, suppress */
	}

	if (len >= (OFFSETOF(wlc_rev_info_t, nvramrev))) {
		rinfo->chippkg = wlc->pub->sih->chippkg;
	}

	if (len >= (OFFSETOF(wlc_rev_info_t, phyminorrev))) {
		rinfo->nvramrev = wlc->nvramrev;
	}
	if (len >= (OFFSETOF(wlc_rev_info_t, coreminorrev))) {
		rinfo->phyminorrev = wlc->band->phy_minor_rev;
	}
	if (len >= sizeof(*rinfo)) {
		rinfo->coreminorrev = wlc->pub->corerev_minor;
		rinfo->otpflag = wlc->pub->sih->otpflag;
		rinfo->ucoderev2 = wlc->ucode_rev2;
	}

	return BCME_OK;
} /* wlc_get_revision_info */

/**
 * Several rate related functions have an 'mcsallow' parameter. That parameter is a bit mask that
 * indicates if 11n, 11n proprietary and/or 11ac MCS rates are allowed.
 *
 * Return value: the mcsallow flags based on status in pub and bsscfg.
 */
uint8
wlc_get_mcsallow(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	uint8 mcsallow = 0;

	if (wlc) {
		if (cfg) {
			if (BSS_N_ENAB(wlc, cfg))
				mcsallow |= WLC_MCS_ALLOW;
			if (BSS_VHT_ENAB(wlc, cfg))
				mcsallow |= WLC_MCS_ALLOW_VHT;
			if (BSS_HE_ENAB(wlc, cfg))
				mcsallow |= WLC_MCS_ALLOW_HE;
		} else {
			if (N_ENAB(wlc->pub))
				mcsallow |= WLC_MCS_ALLOW;
			if (VHT_ENAB_BAND(wlc->pub, wlc->band->bandtype))
				mcsallow |= WLC_MCS_ALLOW_VHT;
			if (HE_ENAB(wlc->pub))
				mcsallow |= WLC_MCS_ALLOW_HE;
		}
		if (WLPROPRIETARY_11N_RATES_ENAB(wlc->pub) &&
			wlc->pub->ht_features != WLC_HT_FEATURES_PROPRATES_DISAB)
			mcsallow |= WLC_MCS_ALLOW_PROP_HT;

		if ((mcsallow & WLC_MCS_ALLOW_VHT) &&
			WLC_VHT_FEATURES_GET(wlc->pub, WL_VHT_FEATURES_1024QAM))
			mcsallow |= WLC_MCS_ALLOW_1024QAM;
	}

	return mcsallow;
}

/** Get the default rateset */
void
wlc_default_rateset(wlc_info_t *wlc, wlc_rateset_t *rs)
{
	wlc_rateset_default(wlc, rs, NULL, wlc->band->phytype, wlc->band->bandtype, FALSE,
	                    RATE_MASK_FULL, wlc_get_mcsallow(wlc, NULL),
	                    CHSPEC_WLC_BW(wlc->default_bss->chanspec),
	                    wlc->stf->op_rxstreams);

	WL_RATE(("wl%d: %s: bandunit 0x%x phy_type 0x%x gmode 0x%x\n", wlc->pub->unit,
	        __FUNCTION__, wlc->band->bandunit, wlc->band->phytype, wlc->band->gmode));
#ifdef BCMDBG
	wlc_rateset_show(wlc, rs, &wlc->default_bss->BSSID);
#endif // endif
}

static void
BCMATTACHFN(wlc_bss_default_init)(wlc_info_t *wlc)
{
	chanspec_t chanspec;
	wlcband_t * band;
	wlc_bss_info_t * bi = wlc->default_bss;

	/* init default and target BSS with some sane initial values */
	bzero((char*)(bi), sizeof(wlc_bss_info_t));
	bi->beacon_period = ISSIM_ENAB(wlc->pub->sih) ? BEACON_INTERVAL_DEF_QT :
		BEACON_INTERVAL_DEFAULT;
	bi->dtim_period = ISSIM_ENAB(wlc->pub->sih) ? DTIM_INTERVAL_DEF_QT :
	        DTIM_INTERVAL_DEFAULT;

#ifdef WLRSDB
	/* For RSDB, avoid Same-InBand (SIB) with both on 2G
	 * Default Main to 5G and Aux to 2G
	 * XXX: TBD: boardflag if 2G or 5G FEMs are shared between Main/Aux
	 */
	if (RSDB_ENAB(wlc->pub) && (wlc_get_wlcindex(wlc) == 0)) {
		chanspec = wlc_next_chanspec(wlc->cmi,
			CH20MHZ_CHSPEC((CH_MAX_2G_CHANNEL+1)), CHAN_TYPE_ANY, TRUE);
	} else
#endif // endif
	{
		/* fill the default channel as the first valid channel
		 * starting from the 2G channels
		 */
		chanspec = wlc_next_chanspec(wlc->cmi, CH20MHZ_CHSPEC(0), CHAN_TYPE_ANY, TRUE);
	}

	/* if there are no valid channels, use the first phy supported channel
	 * for the current band
	 */
	if (chanspec == INVCHANSPEC)
		chanspec = phy_utils_chanspec_band_firstch((phy_info_t *)WLC_PI(wlc),
			wlc->band->bandtype);

	wlc->home_chanspec = bi->chanspec = chanspec;

	/* find the band of our default channel */
	band = wlc->band;
	if (NBANDS(wlc) > 1 && band->bandunit != CHSPEC_WLCBANDUNIT(chanspec))
		band = wlc->bandstate[OTHERBANDUNIT(wlc)];

	/* init bss rates to the band specific default rate set */
	wlc_rateset_default(wlc, &bi->rateset, NULL, band->phytype, band->bandtype, FALSE,
		RATE_MASK_FULL, wlc_get_mcsallow(wlc, NULL),
		CHSPEC_WLC_BW(chanspec), wlc->stf->op_rxstreams);

	if (WME_ENAB(wlc->pub)) {
		bi->flags |= WLC_BSS_WME;
	}

	if (N_ENAB(wlc->pub))
		bi->flags |= WLC_BSS_HT;

	if (VHT_ENAB_BAND(wlc->pub, band->bandtype))
		bi->flags2 |= WLC_BSS_VHT;

	if (HE_ENAB_BAND(wlc->pub, band->bandtype))
		bi->flags3 |= WLC_BSS3_HE;
} /* wlc_bss_default_init */

/**
 * Combine Supported Rates and Extended Supported Rates in to a rateset.
 * Return BCME_OK on success, BCME_XXXX on error.
 */
int
wlc_combine_rateset(wlc_info_t *wlc, wlc_rateset_t *sup, wlc_rateset_t *ext, wlc_rateset_t *rates)
{
	int ext_count;

	ASSERT(sup != NULL);
	ASSERT(ext != NULL);
	ASSERT(rates != NULL);

	bzero(rates, sizeof(*rates));

	rates->count = MIN(sup->count, WLC_NUMRATES);
	bcopy(sup->rates, rates->rates, rates->count);

	ext_count = MIN(ext->count, WLC_NUMRATES - rates->count);
	bcopy(ext->rates, rates->rates + rates->count, ext_count);
	rates->count += ext_count;

	return BCME_OK;
}

/* internal WLC_MBSP_SEL_XXX to spec DOT11_BSS_MEMBERSHIP_XXX mapping */
static const dot11_mbsp_sel_t mbsp_sel_tbl[] = {
	DOT11_BSS_MEMBERSHIP_HT,
	DOT11_BSS_MEMBERSHIP_VHT,
	DOT11_BSS_MEMBERSHIP_HE
};

/* Query user configured BSS Membership Selector(s).
 * The return value is a bitmap of WLC_MBSP_SEL_XXXX.
 */
wlc_mbsp_sel_t
wlc_bss_membership_get(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	wlc_mbsp_sel_t mbsp_sel = 0;

	/* for now get HT PHY membership only */
	if (wlc_ht_get_phy_membership(wlc->hti)) {
		mbsp_sel |= WLC_MBSP_SEL_HT;
	}

	return mbsp_sel;
}

/* Add Membership Selector(s) into Rates IE based on user configuration */
static int
wlc_bss_membership_add(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	wlc_rateset_t *sup_rates, wlc_rateset_t *ext_rates)
{
	uint i;
	wlc_mbsp_sel_t mbsp_sel;

	ASSERT(sup_rates != NULL);
	ASSERT(ext_rates != NULL);

	mbsp_sel = wlc_bss_membership_get(wlc, cfg);

	for (i = 0; i < ARRAYSIZE(mbsp_sel_tbl); i ++) {
		dot11_mbsp_sel_t mbsp;

		if ((mbsp_sel & (1 << i)) == 0 ||
		    (mbsp = mbsp_sel_tbl[i]) == 0) {
			continue;
		}

		if (sup_rates->count < 8) {
			sup_rates->rates[sup_rates->count++] = mbsp;
		} else if (ext_rates->count < ARRAYSIZE(ext_rates->rates)) {
			ext_rates->rates[ext_rates->count++] = mbsp;
		} else {
			return BCME_BUFTOOSHORT;
		}
	}

	return BCME_OK;
}

void
wlc_rateset_elts(wlc_info_t *wlc, wlc_bsscfg_t *cfg, const wlc_rateset_t *rates,
	wlc_rateset_t *sup_rates, wlc_rateset_t *ext_rates)
{
	uint8 sup_bin[WLC_MAXRATE+1];
	uint8 ext_bin[WLC_MAXRATE+1];
	uint8 sup_pref_bin[WLC_MAXRATE+1];
	const wlc_rateset_t *sup_pref_rates;
	const wlc_rateset_t *hw_rs = &wlc->band->hw_rateset;
	uint8 r;
	uint sup_count, ext_count;
	uint i;
	int j;
	BCM_REFERENCE(wlc);

	ASSERT(rates->count <= WLC_NUMRATES);
	ASSERT(sup_rates != NULL);

	bzero(sup_bin, sizeof(sup_bin));
	bzero(ext_bin, sizeof(ext_bin));
	bzero(sup_pref_bin, sizeof(sup_pref_bin));

	if (!wlc->band->gmode ||
#ifdef WLP2P
	    BSS_P2P_ENAB(wlc, cfg) ||
#endif // endif
	    FALSE) {
#ifdef WLP2P
		wlc_rateset_t rs;

		if (BSS_P2P_ENAB(wlc, cfg)) {
			wlc_rateset_copy(rates, &rs);
			wlc_p2p_rateset_filter(wlc->p2p, cfg, &rs);
			rates = &rs;
		}
#endif // endif
		/* All rates end up in the Supported Rates elt */
		sup_rates->count = rates->count;
		bcopy(rates->rates, sup_rates->rates, rates->count);
		/* setting BSS membership selectors */
		(void)wlc_bss_membership_add(wlc, cfg, sup_rates, ext_rates);
		return;
	}

	/* check for a supported rates override */
	if (sup_rates->count > 0) {
		/* split the rateset into supported and extended using the provided sup_rates
		 * as the guide
		 */
		sup_pref_rates = sup_rates;
	} else {
		/* use the default rate split of LRS rates (wlc_lrs_rates -> 1 2 5.5 11 18 24 36 54)
		 * in the supported rates, and others in extended rates
		 */
		sup_pref_rates = &wlc_lrs_rates;
	}

	/* init the supported rates preferred lookup */
	for (i = 0; i < sup_pref_rates->count; i++) {
		r = sup_pref_rates->rates[i] & RATE_MASK;
		sup_pref_bin[r] = r;
	}

	sup_count = 0;
	ext_count = 0;

	/* split the rates into a supported and ext supported using the preferred split. */
	for (i = 0; i < rates->count; i++) {
		uint8 rate = rates->rates[i];
		r = rate & RATE_MASK;
		if (sup_pref_bin[r]) {
			sup_bin[r] = rate;
			sup_count++;
		} else {
			ext_bin[r] = rate;
			ext_count++;
		}
	}

	if (sup_rates->count == 0 && ext_rates->count == 0) {
		/* fill up the supported rates to the full 8 rates unless there was an override */
		j = (int)hw_rs->count;
		for (i = sup_count; i < 8 && ext_count > 0; i++) {
			for (; j >= 0; j--) {
				r = hw_rs->rates[j] & RATE_MASK;
				if (ext_bin[r] != 0) {
					sup_bin[r] = ext_bin[r];
					sup_count++;
					ext_bin[r] = 0;
					ext_count--;
					break;
				}
			}
		}
	}

	/* fill out the sup_rates and ext_rates in sorted order */
	bzero(sup_rates, sizeof(wlc_rateset_t));
	bzero(ext_rates, sizeof(wlc_rateset_t));
	for (i = 0; i < hw_rs->count; i++) {
		r = hw_rs->rates[i] & RATE_MASK;
		if (sup_bin[r] != 0)
			sup_rates->rates[sup_rates->count++] = sup_bin[r];
		if (ext_bin[r] != 0)
			ext_rates->rates[ext_rates->count++] = ext_bin[r];
	}

	/* setting BSS membership selectors */
	(void)wlc_bss_membership_add(wlc, cfg, sup_rates, ext_rates);
} /* wlc_rateset_elts */

/**
 * The struct ether_addr arg is (when non-NULL) the mac addr of the AP to which we're associating.
 * It's used by nas supplicant in the ap-sta build variant.
 */
void
wlc_link(wlc_info_t *wlc, bool isup, struct ether_addr *addr, wlc_bsscfg_t *bsscfg, uint reason)
{
	wlc_event_t *e;
	wlc_bss_info_t *current_bss;
#ifdef STA
	uint8 *edata, *secdata = NULL;
	uint edatlen, secdatlen = 0;
	brcm_prop_ie_t *event_ie;
#endif /* STA */

	ASSERT(bsscfg != NULL);

#ifdef WLRSDB
	if (RSDB_ENAB(wlc->pub) && bsscfg && BSSCFG_IS_RSDB_IF(bsscfg)) {
		WL_INFORM(("wl%d: %s: ignore i/f event with reason %u\n",
		          wlc->pub->unit, __FUNCTION__, reason));
		return;
	}
#endif /* WLRSDB */

	current_bss = bsscfg->current_bss;
	BCM_REFERENCE(current_bss);

#ifdef STA
	/* When the link goes down, driver is no longer preserving the association */
	if (!isup) {
		if (bsscfg->assoc) {
			bsscfg->assoc->preserved = FALSE;
		}
#ifdef WL_MIMOPS_CFG
		if (WLC_MIMOPS_ENAB(wlc->pub)) {
			wlc_stf_mimo_ps_clean_up(bsscfg, MIMOPS_CLEAN_UP_LINKDOWN);
		}
#endif /* WL_MIMOPS_CFG */

	}
#endif /* STA */

	/* Allow for link-up suppression */
	if (isup && wlc->cmn->nolinkup)
		return;

	e = wlc_event_alloc(wlc->eventq, WLC_E_LINK);
	if (e == NULL) {
		WL_ERROR(("wl%d: %s wlc_event_alloc failed\n", wlc->pub->unit, __FUNCTION__));
		return;
	}

	e->event.event_type = WLC_E_LINK;
	e->event.flags = isup ? WLC_EVENT_MSG_LINK : 0;
	e->event.reason = reason;

#ifdef STA

	/* send WPA/WPA2 IEs in the probe resp as event data */
	if (BSSCFG_STA(bsscfg) && bsscfg->associated &&
	    WSEC_ENABLED(bsscfg->wsec) &&
	    current_bss->bcn_prb != NULL &&
	    current_bss->bcn_prb_len > DOT11_BCN_PRB_LEN) {
		uint8 *parse;
		uint len;
		bcm_tlv_t *ie;

		parse = (uint8 *)current_bss->bcn_prb + sizeof(struct dot11_bcn_prb);
		len = current_bss->bcn_prb_len - sizeof(struct dot11_bcn_prb);

		if ((bcmwpa_is_wpa_auth(bsscfg->WPA_auth) &&
		     (ie = (bcm_tlv_t *)bcm_find_wpaie(parse, len)) != NULL) ||
		    (bcmwpa_is_rsn_auth(bsscfg->WPA_auth) &&
		     (ie = bcm_parse_tlvs(parse, len, DOT11_MNG_RSN_ID)) != NULL) ||
		    FALSE) {

#if defined(WLFBT)
			/* Don't report the association event if it's a fast transition */
			if (BSSCFG_IS_FBT(bsscfg) && (bcmwpa_is_rsn_auth(wlc->cfg->WPA_auth)) &&
				isup && reason) {
				wlc_event_free(wlc->eventq, e);
				return;
			}
#endif /* WLFBT */

			if (EVDATA_BSSINFO_ENAB(wlc->pub)) {
				secdata = (uint8*)ie;
				secdatlen = ie->len + TLV_HDR_LEN;
			} else {
				e->data = wlc_event_data_alloc(wlc->eventq,
				                               ie->len + TLV_HDR_LEN, WLC_E_LINK);
				if (e->data != NULL) {
					e->event.datalen = ie->len + TLV_HDR_LEN;
					bcopy(ie, e->data, e->event.datalen);
				}
			}
		}
	}

	if (EVDATA_BSSINFO_ENAB(wlc->pub)) {
		/* Malloc a block for event data: wl_bss_info_t plus any security IE */
		edatlen = secdatlen;
		edatlen += BRCM_PROP_IE_LEN + sizeof(wl_bss_info_t);
		edata = wlc_event_data_alloc(wlc->eventq, edatlen, WLC_E_LINK);

		/* If we got enough for both, do bss_info first, move later if need room for IE */
		if (edata != NULL) {
			event_ie = (brcm_prop_ie_t*)edata;
			event_ie->id = DOT11_MNG_PROPR_ID;
			event_ie->len = (BRCM_PROP_IE_LEN - TLV_HDR_LEN) + sizeof(wl_bss_info_t);
			bcopy(BRCM_PROP_OUI, event_ie->oui, DOT11_OUI_LEN);
			event_ie->type = BRCM_EVT_WL_BSS_INFO;

			if (wlc_bss2wl_bss(wlc, current_bss,
				(wl_bss_info_t*)((uint8 *)event_ie + BRCM_PROP_IE_LEN),
				sizeof(wl_bss_info_t), FALSE)) {
				/* Really shouldn't happen, but if so skip it */
				ASSERT(0);
				wlc_event_data_free(wlc->eventq, edata, edatlen);
				edata = NULL;
			}
		}

		/* Now deal with security IE: slide bss_info over, or malloc new if needed */
		if (secdatlen) {
			if (edata) {
				memmove(edata + secdatlen, edata,
					BRCM_PROP_IE_LEN + sizeof(wl_bss_info_t));
			} else {
				/* Malloc a block for event data without wl_bss_info_t */
				edatlen = secdatlen;
				edata = wlc_event_data_alloc(wlc->eventq,
				                             edatlen, WLC_E_LINK);
			}

			if (edata) {
				bcopy(secdata, edata, secdatlen);
			}
		}

		/* Either way, if we have data then link it to the event */
		if (edata) {
			e->data = edata;
			e->event.datalen = edatlen;
		} else {
			e->data = NULL;
			e->event.datalen = 0;
		}
	}
#endif /* STA */

	wlc_event_if(wlc, bsscfg, e, addr);

	wlc_process_event(wlc, e);
} /* wlc_link */

/** Clean up monitor resources if driver goes down */
static void
wlc_monitor_down(wlc_info_t *wlc)
{
	if (wlc->monitor_amsdu_pkts)
		PKTFREE(wlc->osh, wlc->monitor_amsdu_pkts, FALSE);
	wlc->monitor_amsdu_pkts = NULL;
}

#ifdef WLCNTSCB

static void
wlc_sta_info_upd_scb_stats(const wlc_info_t *wlc, const wlc_bsscfg_t *bsscfg,
	const wlc_scb_stats_t *scb_stats, sta_info_t *sta)
{
	int tx_rate;

	sta->flags |= WL_STA_SCBSTATS;
	sta->tx_failures = scb_stats->tx_failures;
	sta->rx_ucast_pkts = scb_stats->rx_ucast_pkts;
	sta->rx_mcast_pkts = scb_stats->rx_mcast_pkts;
	tx_rate = RSPEC2KBPS(scb_stats->tx_rate);
	tx_rate = (tx_rate >= 0)? tx_rate : 0;
	sta->tx_rate = tx_rate;
	sta->tx_rate_fallback = RSPEC2KBPS(scb_stats->tx_rate_fallback);
	sta->rx_rate = RSPEC2KBPS(scb_stats->rx_rate);
	sta->rx_decrypt_succeeds = scb_stats->rx_decrypt_succeeds;
	sta->rx_decrypt_failures = scb_stats->rx_decrypt_failures;

	if (BSSCFG_AP(bsscfg)) {
		struct scb *bcmc_scb = WLC_BCMCSCB_GET(wlc, bsscfg);

		/* scb_pkts hold total unicast for AP SCB */
		sta->tx_pkts = scb_stats->tx_pkts;	/* unicast TX */
		sta->tx_mcast_pkts = bcmc_scb ? bcmc_scb->scb_stats.tx_pkts : 0;
		sta->tx_mcast_bytes = bcmc_scb ? bcmc_scb->scb_stats.tx_ucast_bytes : 0;
	} else {
		/* scb_pkts hold total # for non-AP SCB */
		sta->tx_pkts = scb_stats->tx_pkts - scb_stats->tx_mcast_pkts;
		sta->tx_mcast_pkts = scb_stats->tx_mcast_pkts;
		sta->tx_mcast_bytes = scb_stats->tx_mcast_bytes;
	}
	sta->tx_ucast_bytes = scb_stats->tx_ucast_bytes;
	sta->tx_tot_pkts = sta->tx_pkts + sta->tx_mcast_pkts;
	sta->tx_tot_bytes = sta->tx_ucast_bytes + sta->tx_mcast_bytes;
	sta->rx_ucast_bytes = scb_stats->rx_ucast_bytes;
	sta->rx_mcast_bytes = scb_stats->rx_mcast_bytes;
	sta->rx_tot_pkts = sta->rx_ucast_pkts + sta->rx_mcast_pkts;
	sta->rx_tot_bytes = sta->rx_ucast_bytes + sta->rx_mcast_bytes;
	sta->tx_pkts_retried = scb_stats->tx_pkts_retried;
	sta->rx_pkts_retried = scb_stats->rx_pkts_retried;

	/* WLAN TX statistics for link monitor */
	sta->tx_pkts_total = scb_stats->tx_pkts_total;
	sta->tx_pkts_retries = scb_stats->tx_pkts_retries;
	sta->tx_pkts_retry_exhausted = scb_stats->tx_pkts_retry_exhausted;
	sta->tx_pkts_fw_total = scb_stats->tx_pkts_fw_total;
	sta->tx_pkts_fw_retries = scb_stats->tx_pkts_fw_retries;
	sta->tx_pkts_fw_retry_exhausted = scb_stats->tx_pkts_fw_retry_exhausted;

	sta->tx_rspec = scb_stats->tx_rate;
	sta->rx_rspec = scb_stats->rx_rate;
}

#endif /* WLCNTSCB */

int
wlc_sta_report(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, const struct ether_addr *ea,
	void *buf, int len)
{
	sta_report_t sta_report;
	struct scb *scb;
	int bandunit;

	ASSERT(ea != NULL);
	if (ea == NULL) {
		return (BCME_BADARG);
	}

	ASSERT(bsscfg != NULL);
	if (bsscfg->up) {
		bandunit = CHSPEC_WLCBANDUNIT(bsscfg->current_bss->chanspec);
	}
	else {
		bandunit = CHSPEC_WLCBANDUNIT(wlc->home_chanspec);
	}

	if ((scb = wlc_scbfindband(wlc, bsscfg, ea, bandunit)) == NULL) {
		return (BCME_BADADDR);
	}

	if (len < sizeof(sta_report)) {
		return BCME_BUFTOOSHORT;
	}

	/* fill in the sta_report struct */
	bzero(&sta_report, sizeof(sta_report));
	sta_report.ver = WL_STA_REPORT_VER;
	bcopy(ea, &sta_report.ea, ETHER_ADDR_LEN);

#ifdef WLCNTSCB
	sta_report.tx_rspec = scb->scb_stats.tx_rate;
	sta_report.rx_rspec = scb->scb_stats.rx_rate;
#endif /* WLCNTSCB */

	sta_report.len = sizeof(sta_report);
	/* bcopy to avoid alignment issues */
	bcopy(&sta_report, buf, sizeof(sta_report));

	return BCME_OK;
}

int
wlc_sta_info(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, const struct ether_addr *ea,
	void *buf, int len)
{
	sta_info_t sta;
	struct scb *scb;
	int bandunit, i;
	int copy_len;
	uint8 nss, mcs_code, prop_mcs_code, bw;

	ASSERT(ea != NULL);
	if ((ea == NULL) || ETHER_ISMULTI(ea)) {
		return (BCME_BADARG);
	}

	ASSERT(bsscfg != NULL);
	if (bsscfg->up)
		bandunit = CHSPEC_WLCBANDUNIT(bsscfg->current_bss->chanspec);
	else
		bandunit = CHSPEC_WLCBANDUNIT(wlc->home_chanspec);

	if ((scb = wlc_scbfindband(wlc, bsscfg, ea, bandunit)) == NULL)
		return (BCME_BADADDR);

	if (len < WL_OLD_STAINFO_SIZE) {
		return BCME_BUFTOOSHORT;
	}
	/* fill in the sta_info struct */
	bzero(&sta, sizeof(sta_info_t));
	sta.ver = WL_STA_VER;
	bw = wlc_scb_ratesel_get_link_bw(wlc, scb);
	sta.chanspec = wf_channel2chspec(wf_chspec_ctlchan(bsscfg->current_bss->chanspec),
			(bw == BW_20MHZ ? WL_CHANSPEC_BW_20 :
			(bw == BW_40MHZ ? WL_CHANSPEC_BW_40 :
			(bw == BW_80MHZ ? WL_CHANSPEC_BW_80 :
			(bw == BW_160MHZ ? WL_CHANSPEC_BW_160 :
			WL_CHANSPEC_BW_20)))));
	bcopy(ea, &sta.ea, ETHER_ADDR_LEN);
	bcopy(&scb->rateset, &sta.rateset, sizeof(wl_rateset_t));
	sta.idle = wlc->pub->now - scb->used;
	sta.flags = 0;
	sta.in = 0;

	if (scb->flags & SCB_BRCM)
		sta.flags |= WL_STA_BRCM;
	if (SCB_WME(scb))
		sta.flags |= WL_STA_WME;
	if (SCB_AUTHENTICATED(scb))
		sta.flags |= WL_STA_AUTHE;
	if (SCB_ASSOCIATED(scb)) {
		sta.flags |= WL_STA_ASSOC;
		sta.in = wlc->pub->now - scb->assoctime;
		if (scb->bsscfg) {
			wlc_key_info_t key_info;
			key_info.algo = CRYPTO_ALGO_OFF;
			(void)wlc_keymgmt_get_bss_tx_key(wlc->keymgmt, scb->bsscfg, FALSE,
					&key_info);
			sta.algo = key_info.algo;
		} else {
			sta.algo = CRYPTO_ALGO_OFF;
		}
		sta.wpauth = scb->WPA_auth;
	}
	if (SCB_AUTHORIZED(scb))
		sta.flags |= WL_STA_AUTHO;
	if (AC_BITMAP_TST(scb->apsd.ac_defl, AC_BE))
		sta.flags |= WL_STA_APSD_BE;
	if (AC_BITMAP_TST(scb->apsd.ac_defl, AC_BK))
		sta.flags |= WL_STA_APSD_BK;
	if (AC_BITMAP_TST(scb->apsd.ac_defl, AC_VI))
		sta.flags |= WL_STA_APSD_VI;
	if (AC_BITMAP_TST(scb->apsd.ac_defl, AC_VO))
		sta.flags |= WL_STA_APSD_VO;
	sta.listen_interval_inms = 0;

	if (SCB_LEGACY_WDS(scb)) {
		sta.flags |= WL_STA_WDS;
		if (scb->flags & SCB_WDS_LINKUP)
			sta.flags |= WL_STA_WDS_LINKUP;
	}
	if (BSSCFG_AP(bsscfg)) {
		if (SCB_PS(scb))
			sta.flags |= WL_STA_PS;
		sta.aid = scb->aid;
		sta.cap = scb->cap;
	}
	else if (BSSCFG_STA(scb->bsscfg)) {
		sta.aid = bsscfg->AID;
	}
	sta.listen_interval_inms = wlc_apps_get_listen_prd(wlc, scb) *
	        bsscfg->current_bss->beacon_period;
	wlc_ht_fill_sta_fields(wlc->hti, scb, &sta);

	if (scb->flags & SCB_WPSCAP)
		sta.flags |= WL_STA_WPS;

	if (scb->flags & SCB_NONERP)
		sta.flags |= WL_STA_NONERP;
	if (SCB_AMPDU(scb))
		sta.flags |= WL_STA_AMPDU_CAP;
	if (SCB_AMSDU(scb))
		sta.flags |= WL_STA_AMSDU_CAP;
#ifdef WL11N
	if (WLC_HT_GET_SCB_MIMOPS_ENAB(wlc->hti, scb))
		sta.flags |= WL_STA_MIMO_PS;
	if (WLC_HT_GET_SCB_MIMOPS_RTS_ENAB(wlc->hti, scb))
		sta.flags |= WL_STA_MIMO_RTS;
#endif /* WL11N */
	if (scb->flags & SCB_RIFSCAP)
		sta.flags |= WL_STA_RIFS_CAP;
	if (SCB_VHT_CAP(scb)) {
		sta.flags |= WL_STA_VHT_CAP;
#ifdef WL11AC
		sta.vht_flags = wlc_vht_get_scb_flags(wlc->vhti, scb);
#endif // endif
	}

#ifdef DWDS
	if (SCB_DWDS_CAP(scb)) {
		sta.flags |= WL_STA_DWDS_CAP;
	}
	if (SCB_DWDS(scb)) {
		sta.flags |= WL_STA_DWDS;
	}
#endif // endif

#ifdef WL11AX
	if (SCB_HE_CAP(scb)) {
		sta.flags |= WL_STA_HE_CAP;

		STATIC_ASSERT(SCB_HE_LDPCCAP == WL_STA_HE_LDPCCAP);
		STATIC_ASSERT(SCB_HE_TX_STBCCAP == WL_STA_HE_TX_STBCCAP);
		STATIC_ASSERT(SCB_HE_RX_STBCCAP == WL_STA_HE_RX_STBCCAP);
		STATIC_ASSERT(SCB_HE_HTC_CAP == WL_STA_HE_HTC_CAP);
		STATIC_ASSERT(SCB_HE_SU_BFR == WL_STA_HE_SU_BEAMFORMER);
		STATIC_ASSERT(SCB_HE_SU_MU_BFE == WL_STA_HE_SU_MU_BEAMFORMEE);
		STATIC_ASSERT(SCB_HE_MU_BFR == WL_STA_HE_MU_BEAMFORMER);

		sta.he_flags = wlc_he_get_scb_flags(wlc->hei, scb);
	}
#endif /* WL11AX */

#ifdef WL_GLOBAL_RCLASS
	if (wlc_ap_scb_support_global_rclass(wlc, scb)) {
		sta.flags |= WL_STA_GBL_RCLASS;
	}
#endif /* WL_GLOBAL_RCLASS */
	/* update per antenna rssi and noise floor */
	/* to be done */

	if (len < (int)sizeof(sta_info_t)) {
		copy_len = WL_OLD_STAINFO_SIZE;
	} else {
#ifdef WLCNTSCB
		wlc_sta_info_upd_scb_stats(wlc, bsscfg, &scb->scb_stats, &sta);
#endif /* WLCNTSCB */
		copy_len = sizeof(sta_info_t);
		for (i = WL_ANT_IDX_1; i < WL_RSSI_ANT_MAX; i++) {
			sta.rssi[i] = wlc_lq_ant_rssi_get(wlc, SCB_BSSCFG(scb), scb, i);
		}

		for (i = WL_ANT_IDX_1; i < WL_RSSI_ANT_MAX; i++)
			sta.rx_lastpkt_rssi[i] =
			        wlc_lq_ant_rssi_last_get(wlc, SCB_BSSCFG(scb), scb, i);

		/* per core noise floor */
		for (i = 0; i < WL_RSSI_ANT_MAX; i++)
			sta.nf[i] = phy_noise_avg_per_antenna(WLC_PI(wlc), i);

		sta.link_bw = bw;
		sta.srssi = wlc_lq_rssi_ma(wlc, bsscfg, scb);
	}

	/* copy rateset into the sta rateset adv */
	sta.rateset_adv.count = scb->rateset.count;
	memcpy(&sta.rateset_adv.rates, &scb->rateset.rates, sizeof(sta.rateset_adv.rates));

	/* Copy mcs rateset */
	memcpy(&sta.rateset_adv.mcs, &scb->rateset.mcs, sizeof(sta.rateset_adv.mcs));

	/* copy vht mcs set */
	for (nss = 1; nss <= wlc->stf->op_txstreams; nss++) {
		mcs_code = VHT_MCS_MAP_GET_MCS_PER_SS(nss, scb->rateset.vht_mcsmap);
		prop_mcs_code = VHT_MCS_MAP_GET_MCS_PER_SS(nss, scb->rateset.vht_mcsmap_prop);
		sta.rateset_adv.vht_mcs[nss - 1] = VHT_MCS_CODE_TO_MCS_MAP(mcs_code) |
			VHT_PROP_MCS_CODE_TO_PROP_MCS_MAP(prop_mcs_code);
	}
#ifdef WL11AX
	/* copy he bw80 tx/rx, bw160 tx/rx, bw80p80 tx/rx mcs info */
	{
		sta.rateset_adv.he_mcs[0] = scb->rateset.he_bw80_tx_mcs_nss;
		sta.rateset_adv.he_mcs[1] = scb->rateset.he_bw80_rx_mcs_nss;
		sta.rateset_adv.he_mcs[2] = scb->rateset.he_bw160_tx_mcs_nss;
		sta.rateset_adv.he_mcs[3] = scb->rateset.he_bw160_rx_mcs_nss;
		sta.rateset_adv.he_mcs[4] = scb->rateset.he_bw80p80_tx_mcs_nss;
		sta.rateset_adv.he_mcs[5] = scb->rateset.he_bw80p80_rx_mcs_nss;
	}
#endif /* WL11AX */
#ifdef WLWNM_AP
	sta.wnm_cap = wlc_wnm_get_scbcap(wlc, scb);
#endif /* WLWNM_AP */
#ifdef AP
	wlc_ap_get_sta_info_vendor_oui(wlc, scb, &sta.sta_vendor_oui);
#endif /* AP */
#ifdef WL11K_AP
	wlc_rrm_get_sta_cap(wlc, bsscfg, scb, sta.rrm_capabilities);
#endif /* WL11K_AP */
	sta.len = (uint16)copy_len;
	/* bcopy to avoid alignment issues */
	bcopy(&sta, buf, copy_len);

	return (0);
} /* wlc_sta_info */

#ifdef BCMDBG
/** reset the TSF register to the given value */
static void
wlc_tsf_set(wlc_info_t *wlc, uint32 tsf_l, uint32 tsf_h)
{
	osl_t *osh;
	osh = wlc->osh;

	W_REG(osh, D11_TSFTimerLow(wlc), tsf_l);
	W_REG(osh, D11_TSFTimerHigh(wlc), tsf_h);
}
#endif /* BCMDBG */

/** adjust the TSF register by a 32 bit delta */
void
wlc_tsf_adjust(wlc_info_t *wlc, int delta)
{
	wlc_bmac_tsf_adjust(wlc->hw, delta);
} /* wlc_tsf_adjust */

/**
 * This routine creates a chanspec with the current band and b/w using the channel
 * number passed in. For 40MHZ chanspecs it always chooses the lower ctrl SB.
 */
chanspec_t
wlc_create_chspec(wlc_info_t *wlc, uint8 channel)
{
	chanspec_t chspec = 0;

	if (BAND_2G(wlc->band->bandtype))
		chspec |= WL_CHANSPEC_BAND_2G;
	else
		chspec |= WL_CHANSPEC_BAND_5G;
	if (N_ENAB(wlc->pub) &&
	    wlc_valid_chanspec_db(wlc->cmi, wlc->chanspec) &&
	    CHSPEC_IS40(wlc->chanspec) &&
	    (wlc_channel_locale_flags(wlc->cmi) & WLC_NO_40MHZ) == 0 &&
	    WL_BW_CAP_40MHZ(wlc->band->bw_cap)) {
		chspec |= WL_CHANSPEC_BW_40;
		chspec |= WL_CHANSPEC_CTL_SB_LOWER;
	} else {
		chspec |= WL_CHANSPEC_BW_20;
	}
	chspec |= channel;

	return chspec;
}

#ifdef STA
/** Change PCIE War override for some platforms */
void
wlc_pcie_war_ovr_update(wlc_info_t *wlc, uint8 aspm)
{
	 wlc_bmac_pcie_war_ovr_update(wlc->hw, aspm);
}

void
wlc_pcie_power_save_enable(wlc_info_t *wlc, bool enable)
{
	wlc_bmac_pcie_power_save_enable(wlc->hw, enable);
}
#endif /* STA */

#if defined(BCMTSTAMPEDLOGS)
/** Asynchronously output a timestamped log message */
void
wlc_log(wlc_info_t *wlc, const char* str, uint32 p1, uint32 p2)
{
	uint32 tstamp;

	/* Read a timestamp from the TSF timer register */
	tstamp = R_REG(wlc->osh, D11_TSFTimerLow(wlc));

	/* Store the timestamp and the log message in the log buffer */
	bcmtslog(tstamp, str, p1, p2);
}
#endif /* defined(BCMTSTAMPEDLOGS) */

#ifdef	WME
/** Function downgrades the edcf parameters */
int BCMFASTPATH
wlc_wme_downgrade_fifo(wlc_info_t *wlc, uint* pfifo, struct scb *scb)
{
	wlc_bsscfg_t *cfg;
	wlc_wme_t *wme;
	uint ac;

	ASSERT(scb != NULL);

	ASSERT(*pfifo < NFIFO_LEGACY);
	ac = wme_fifo2ac[*pfifo];

	/* Downgrade the fifo if admission is not yet gained */
	if (CAC_ENAB(wlc->pub) && wlc_cac_is_traffic_admitted(wlc->cac, ac, scb)) {
		return BCME_OK;
	}

	cfg = SCB_BSSCFG(scb);
	ASSERT(cfg != NULL);

	wme = cfg->wme;

	if (AC_BITMAP_TST(wme->wme_admctl, ac)) {
		do {
			if (*pfifo == 0) {
				WL_ERROR(("No AC available; tossing pkt\n"));
				WLCNTINCR(wlc->pub->_cnt->txnobuf);
				WLCIFCNTINCR(scb, txnobuf);
				WLCNTSCB_COND_INCR(scb, scb->scb_stats.tx_failures);
				return BCME_ERROR;
			}

			(*pfifo)--;
			ac = wme_fifo2ac[*pfifo];
		} while (AC_BITMAP_TST(wme->wme_admctl, ac));

		/*
		 * Note: prio in packet is NOT updated.
		 * Original prio is also used below to determine APSD trigger AC.
		 */
	}

	return BCME_OK;
} /* wlc_wme_downgrade_fifo */

uint16
wlc_wme_get_frame_medium_time(wlc_info_t *wlc, uint8 bandunit,
	ratespec_t rate, uint8 preamble_type, uint mac_len)
{
	uint16 duration;

	duration = (uint16)SIFS(bandunit);
	duration += (uint16)wlc_calc_ack_time(wlc, rate, preamble_type);
	duration += (uint16)wlc_calc_frame_time(wlc, rate, preamble_type, mac_len);
	/* duration in us */
	return duration;
}
#endif /* WME */

#if defined(WLPLT)
int
wlc_minimal_down(wlc_info_t *wlc)
{
	uint callbacks;
	wlc_phy_t *pi = WLC_PI(wlc);
#ifdef STA
	int i;
	wlc_bsscfg_t *bsscfg;
#endif // endif

	callbacks = 0;
	WL_TRACE(("wl%d: %s: Enter\n", wlc->pub->unit, __FUNCTION__));
	if (!wlc->pub->up)
		return BCME_NOTUP;

	if (wlc->pub->radio_active == OFF)
		return BCME_ERROR;

	/* Do not access Phy registers if core is not up */
	if (wlc_bmac_si_iscoreup(wlc->hw) == FALSE)
		return BCME_ERROR;

	/* abort any scan in progress */
	wlc_scan_abort(wlc->scan, WLC_E_STATUS_ABORT);

#ifdef STA
#ifdef WLRM
	/* abort any radio measures in progress */
	if (WLRM_ENAB(wlc->pub) && !wlc_rm_abort(wlc))
		callbacks++;
#endif /* WLRM */

	FOREACH_BSS(wlc, i, bsscfg) {
		/* Perform STA down operations if needed */
		if (!BSSCFG_STA(bsscfg))
			continue;
		/* abort any association in progress */
		wlc_assoc_abort(bsscfg);

		/* if config is enabled, take care of deactivating */
		if (bsscfg->enable) {
			/* For WOWL or Assoc Recreate, don't disassociate,
			 * just down the bsscfg.
			 * Otherwise,
			 * disable the config (STA requires active restart)
			 */
			if (ASSOC_RECREATE_ENAB(wlc->pub) &&
			    (bsscfg->flags & WLC_BSSCFG_PRESERVE)) {
				WL_ASSOC(("wl%d: %s: Preserving existing association....\n",
				          wlc->pub->unit, __FUNCTION__));
				wlc_bsscfg_down(wlc, bsscfg);
			} else {
				WL_ASSOC(("wl%d: %s: Disabling bsscfg on down\n",
				          wlc->pub->unit, __FUNCTION__));
				wlc_bsscfg_disable(wlc, bsscfg);
				/* allow time for disassociation packet to
				 * notify associated AP of our departure
				 */
				OSL_DELAY(4 * 1000);
			}
		}
	}
#endif /* STA */

#ifdef WLLED
	wlc_led_down(wlc);
#endif // endif

#ifdef STA
	/* stop pspoll timer to prevent the CPU from running too often */
	FOREACH_AS_STA(wlc, i, bsscfg) {
		callbacks += wlc_pspoll_timer_upd(bsscfg, FALSE);
		callbacks += wlc_apsd_trigger_upd(bsscfg, FALSE);
	}
#endif // endif

	/* suspend d11 core */
	wlc_suspend_mac_and_wait(wlc);

	phy_radio_switch((phy_info_t *)pi, OFF);
	phy_ana_switch((phy_info_t *)pi, OFF);
	wlc->pub->radio_active = OFF;

	wlc_bmac_minimal_radio_hw(wlc->hw, FALSE);

	wlc->pub->up = FALSE;
	return callbacks;
} /* wlc_minimal_down */

int
wlc_minimal_up(wlc_info_t * wlc)
{
	wlc_phy_t *pi = WLC_PI(wlc);
	WL_TRACE(("wl%d: %s: Enter\n", wlc->pub->unit, __FUNCTION__));

	if (wlc->pub->radio_active == ON)
		return BCME_ERROR;

	/* Do not access Phy registers if core is not up */
	if (wlc_bmac_si_iscoreup(wlc->hw) == FALSE)
		return BCME_ERROR;

	wlc_bmac_minimal_radio_hw(wlc->hw, TRUE);

	phy_ana_switch((phy_info_t *)pi, ON);
	phy_radio_switch((phy_info_t *)pi, ON);
	wlc->pub->radio_active = ON;

	/* resume d11 core */
	wlc_enable_mac(wlc);

#ifdef WLLED
	wlc_led_up(wlc);
#endif // endif
	wlc->pub->up = TRUE;

#ifdef STA
	if (ASSOC_RECREATE_ENAB(wlc->pub)) {
		int idx;
		wlc_bsscfg_t *cfg;
		FOREACH_BSS(wlc, idx, cfg) {
			if (BSSCFG_STA(cfg) && cfg->enable) {
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
				char ssidbuf[SSID_FMT_BUF_LEN];
				wlc_format_ssid(ssidbuf, cfg->SSID, cfg->SSID_len);
#endif // endif

				WL_ASSOC(("wl%d: wlc_up: restarting STA bsscfg 0 \"%s\"\n",
				          wlc->pub->unit, ssidbuf));
				wlc_join_recreate(wlc, cfg);
			}
		}
	}
#endif /* STA */

	return BCME_OK;
} /* wlc_minimal_up */
#endif // endif

/** wrapper BMAC functions to for HIGH driver access */

/** Read a single uint16 from shared memory. SHM 'offset' needs to be an even address. */
uint16
wlc_read_shm(wlc_info_t *wlc, uint offset)
{
	return wlc_bmac_read_shm(wlc->hw, offset);
}

/** Write a single uint16 to shared memory. SHM 'offset' needs to be an even address */
void
wlc_write_shm(wlc_info_t *wlc, uint offset, uint16 v)
{
	wlc_bmac_write_shm(wlc->hw, offset, v);
}

void
wlc_update_shm(wlc_info_t *wlc, uint offset, uint16 v, uint16 mask)
{
	wlc_bmac_update_shm(wlc->hw, offset, v, mask);
}

/**
 * Set a range of shared memory to a value. SHM 'offset' needs to be an even address and Range
 * length 'len' must be an even number of bytes
 */
void
wlc_set_shm(wlc_info_t *wlc, uint offset, uint16 v, int len)
{
	/* offset and len need to be even */
	ASSERT((offset & 1) == 0);
	ASSERT((len & 1) == 0);

	if (len <= 0)
		return;

	wlc_bmac_set_shm(wlc->hw, offset, v, len);
}

/**
 * Copy a buffer to shared memory. SHM 'offset' needs to be an even address and
 * Buffer length 'len' must be an even number of bytes
 */
void
wlc_copyto_shm(wlc_info_t *wlc, uint offset, const void* buf, int len)
{
	/* offset and len need to be even */
	ASSERT((offset & 1) == 0);
	ASSERT((len & 1) == 0);

	if (len <= 0)
		return;
	wlc_bmac_copyto_objmem(wlc->hw, offset, buf, len, OBJADDR_SHM_SEL);

}

/**
 * Copy from shared memory to a buffer. SHM 'offset' needs to be an even address and
 * Buffer length 'len' must be an even number of bytes
 */
void
wlc_copyfrom_shm(wlc_info_t *wlc, uint offset, void* buf, int len)
{
	/* offset and len need to be even */
	ASSERT((offset & 1) == 0);
	ASSERT((len & 1) == 0);

	if (len <= 0)
		return;

	wlc_bmac_copyfrom_objmem(wlc->hw, offset, buf, len, OBJADDR_SHM_SEL);
}

#if defined(WL_PSMX)
/* Read a single uint16 from PSMX shared memory.
 * SHM 'offset' needs to be an even address
 */
uint16
wlc_read_shmx(wlc_info_t *wlc, uint offset)
{
	uint16 val;

	wlc_bmac_copyfrom_objmem(wlc->hw, offset, &val, sizeof(val), OBJADDR_SHMX_SEL);
	return val;
}

/* Write a single uint16 to PSMX shared memory.
 * SHM 'offset' needs to be an even address
 */
void
wlc_write_shmx(wlc_info_t *wlc, uint offset, uint16 v)
{
	wlc_bmac_copyto_objmem(wlc->hw, offset, &v, sizeof(v), OBJADDR_SHMX_SEL);
}

/* Read a single uint16 from PSMX ihr.
 * IHRX 'offset' should be the host space address. (x + 0x200) *2.
 */
uint16
wlc_read_macregx(wlc_info_t *wlc, uint offset)
{
	uint16 val;
	ASSERT(offset >= D11REG_IHR_BASE);
	ASSERT(PSMX_ENAB(wlc->pub));

	offset = (offset - D11REG_IHR_BASE) << 1;
	wlc_bmac_copyfrom_objmem(wlc->hw, offset, &val, sizeof(val), OBJADDR_IHRX_SEL);
	return val;
}

/* Write a single uint16 to PSMX ihr.
 * IHRX 'offset' should be the host space address. (x + 0x200) *2.
 */
void
wlc_write_macregx(wlc_info_t *wlc, uint offset, uint16 v)
{
	ASSERT(offset >= D11REG_IHR_BASE);
	ASSERT(PSMX_ENAB(wlc->pub));

	offset = (offset - D11REG_IHR_BASE) << 1;
	wlc_bmac_copyto_objmem(wlc->hw, offset, &v, sizeof(v), OBJADDR_IHRX_SEL);
}
#endif /* WL_PSMX */

#if defined(WL_PSMR1)
/* Read a single uint16 from PSMR1 shared memory.
 * SHM 'offset' needs to be an even address
 */
uint16
wlc_read_shm1(wlc_info_t *wlc, uint offset)
{
	return wlc_bmac_read_shm1(wlc->hw, offset);
}

/* Write a single uint16 to PSMR1 shared memory.
 * SHM 'offset' needs to be an even address
 */
void
wlc_write_shm1(wlc_info_t *wlc, uint offset, uint16 v)
{
	wlc_bmac_write_shm1(wlc->hw, offset, v);
}

/* Read a single uint16 from PSMR1 ihr.
 * IHR1 'offset' should be the host space address. (x + 0x200) *2.
 */
uint16
wlc_read_macreg1(wlc_info_t *wlc, uint offset)
{
	uint16 val;
	ASSERT(offset >= D11REG_IHR_BASE);
	ASSERT(PSMR1_ENAB(wlc->hw));

	offset = (offset - D11REG_IHR_BASE) << 1;
	wlc_bmac_copyfrom_objmem(wlc->hw, offset, &val, sizeof(val), OBJADDR_IHR1_SEL);
	return val;
}

/* Write a single uint16 to PSMR1 ihr.
 * IHR1 'offset' should be the host space address. (x + 0x200) *2.
 */
void
wlc_write_macreg1(wlc_info_t *wlc, uint offset, uint16 v)
{
	ASSERT(offset >= D11REG_IHR_BASE);
	ASSERT(PSMR1_ENAB(wlc->hw));

	offset = (offset - D11REG_IHR_BASE) << 1;
	wlc_bmac_copyto_objmem(wlc->hw, offset, &v, sizeof(v), OBJADDR_IHR1_SEL);
}
#endif /* WL_PSMR1 */

#ifdef WL_HWKTAB
/**
 * Copy a buffer to HW key table. 'offset' needs to be multiple of 4 bytes and
 * Buffer length 'len' must be an multiple of 4 bytes
 */
void
wlc_copyto_keytbl(wlc_info_t *wlc, uint offset, const uint8* buf, int len)
{
	wlc_bmac_copyto_objmem32(wlc->hw, offset, buf, len, OBJADDR_KEYTBL_SEL);
}

/**
 * Copy from HW key table to a buffer. 'offset' needs to be multiple of 4 bytes and
 * Buffer length 'len' must be an multiple of 4 bytes
 */
void
wlc_copyfrom_keytbl(wlc_info_t *wlc, uint offset, uint8* buf, int len)
{
	wlc_bmac_copyfrom_objmem32(wlc->hw, offset, buf, len, OBJADDR_KEYTBL_SEL);
}

/**
 * Set a range of HW key table to a value. 'offset' needs to be multiple of 4 bytes and
 * Buffer length 'len' must be an multiple of 4 bytes
 */
void
wlc_set_keytbl(wlc_info_t *wlc, uint offset, uint32 v, int len)
{
	wlc_bmac_set_objmem32(wlc->hw, offset, v, len, OBJADDR_KEYTBL_SEL);
}
#endif	/* WL_HWKTAB */

void
wlc_mctrl(wlc_info_t *wlc, uint32 mask, uint32 val)
{
	wlc_bmac_mctrl(wlc->hw, mask, val);
}

void
wlc_mctrlx(wlc_info_t *wlc, uint32 mask, uint32 val)
{
	wlc_bmac_mctrlx(wlc->hw, mask, val);
}

void
wlc_corereset(wlc_info_t *wlc, uint32 flags)
{
	wlc_bmac_corereset(wlc->hw, flags);
}

void
wlc_suspend_mac_and_wait(wlc_info_t *wlc)
{
	wlc_bmac_suspend_mac_and_wait(wlc->hw);
}

void
wlc_enable_mac(wlc_info_t *wlc)
{
	wlc_bmac_enable_mac(wlc->hw);
}

void
wlc_mhf(wlc_info_t *wlc, uint8 idx, uint16 mask, uint16 val, int bands)
{
	wlc_bmac_mhf(wlc->hw, idx, mask, val, bands);
}

uint16
wlc_mhf_get(wlc_info_t *wlc, uint8 idx, int bands)
{
	return wlc_bmac_mhf_get(wlc->hw, idx, bands);
}

static int
wlc_xmtfifo_sz_get(wlc_info_t *wlc, uint fifo, uint *blocks)
{
	return wlc_bmac_xmtfifo_sz_get(wlc->hw, fifo, blocks);
}

#if defined(MACOSX)
static int
wlc_xmtfifo_sz_set(wlc_info_t *wlc, uint fifo, uint16 blocks)
{
	if (fifo >= NFIFO_LEGACY || blocks > 299)
		return BCME_RANGE;

	wlc_xmtfifo_sz_upd_high(wlc, fifo, blocks);

	wlc_bmac_xmtfifo_sz_set(wlc->hw, fifo, blocks);

	return BCME_OK;
}
#endif // endif

static void
wlc_xmtfifo_sz_upd_high(wlc_info_t *wlc, uint fifo, uint16 blocks)
{
	ASSERT(fifo < ARRAYSIZE(wlc->xmtfifo_szh));
	wlc->xmtfifo_szh[fifo] = blocks;
}

void
wlc_write_template_ram(wlc_info_t *wlc, int offset, int len, void *buf)
{
	wlc_bmac_write_template_ram(wlc->hw, offset, len, buf);
}

void
wlc_write_hw_bcntemplates(wlc_info_t *wlc, void *bcn, int len, bool both)
{
	wlc_bmac_write_hw_bcntemplates(wlc->hw, bcn, len, both);
}

/** Halts transmit operation when 'on' is TRUE */
void
wlc_mute(wlc_info_t *wlc, bool on, mbool flags)
{
	wlc_bmac_mute(wlc->hw, on, flags);
}

void
wlc_read_tsf(wlc_info_t* wlc, uint32* tsf_l_ptr, uint32* tsf_h_ptr)
{
	wlc_bmac_read_tsf(wlc->hw, tsf_l_ptr, tsf_h_ptr);
}

#ifdef WLC_TSYNC
void
wlc_start_tsync(wlc_info_t* wlc)
{
	wlc_bmac_start_tsync(wlc->hw);
}
#endif // endif

uint32
wlc_read_usec_timer(wlc_info_t* wlc)
{
	return wlc_bmac_read_usec_timer(wlc->hw);
}

void
wlc_set_cwmin(wlc_info_t *wlc, uint16 newmin)
{
	wlc->band->CWmin = newmin;
	wlc_bmac_set_cwmin(wlc->hw, newmin);
}

void
wlc_set_cwmax(wlc_info_t *wlc, uint16 newmax)
{
	wlc->band->CWmax = newmax;
	wlc_bmac_set_cwmax(wlc->hw, newmax);
}

void
wlc_fifoerrors(wlc_info_t *wlc)
{
	wlc_bmac_fifoerrors(wlc->hw);
}

void
wlc_pllreq(wlc_info_t *wlc, bool set, mbool req_bit)
{
	wlc_bmac_pllreq(wlc->hw, set, req_bit);
}

void
wlc_update_phy_mode(wlc_info_t *wlc, uint32 phy_mode)
{
#ifdef WL11N
	/* Notify the rate selection module of the ACI state change */
	wlc_scb_ratesel_aci_change(wlc->wrsi, phy_mode & PHY_MODE_ACI);
#endif // endif
}

void
wlc_reset_bmac_done(wlc_info_t *wlc)
{
	BCM_REFERENCE(wlc);
}

/* Only for dying-gasp use to quickly turn off wl core */
void
wlc_shutdown_handler(wlc_info_t* wlc)
{
	wl_intrsoff(wlc->wl);
	wlc_led_radioset(wlc->ledh, OFF);

	wlc->hw->up = FALSE; /* WAR: to avoid ASSERT calling wlc_coredisable() */
	wlc_bmac_set_clk(wlc->hw, FALSE);
}

/**
 * wlc_exptime_start() and wlc_exptime_stop() are just for RF awareness external log
 * filtering so we don't send an extlog for each suppressed frame
 */
void
wlc_exptime_start(wlc_info_t *wlc)
{
	uint32 curr_time = OSL_SYSUPTIME();

	WL_INFORM(("wl%d: %s: packet lifetime (%d ms) expired: frame suppressed\n",
		wlc->pub->unit, __FUNCTION__, wlc->rfaware_lifetime/4));
	wlc->last_exptime = curr_time;
	wlc->exptime_cnt++;
}

/** Enter this only when we're in exptime state */
void
wlc_exptime_check_end(wlc_info_t *wlc)
{
	uint32 curr_time = OSL_SYSUPTIME();

	ASSERT(wlc->exptime_cnt);

	/* No lifetime expiration for WLC_EXPTIME_END_TIME means no interference.
	 * Leave the exptime state.
	 */
	if (curr_time - wlc->last_exptime > WLC_EXPTIME_END_TIME) {
		wlc->exptime_cnt = 0;
		wlc->last_exptime = 0;
	}
}

int
wlc_rfaware_lifetime_set(wlc_info_t *wlc, uint16 lifetime)
{
	int i, ret = BCME_OK;
	bool wme_lifetime = FALSE;

	/* XXX Go through lifetime array only at init. If they're nonzero, disable RF awareness.
	 * Setting WME lifetime to nonzero doesn't automatically turn RF awareness back on.
	 */
	for (i = 0; i < AC_COUNT; i++)
		if (wlc->lifetime[i])
			wme_lifetime = TRUE;

	/* Enable RF awareness only when wme lifetime is not used because they share same EXPTIME
	 * tx status
	 */
	if (wlc->txmsdulifetime || wme_lifetime) {
		wlc->rfaware_lifetime = 0;
		if ((wlc->rfaware_lifetime == 0) && lifetime) {
			WL_INFORM(("%s: can't turn on RFAWARE if WME is on\n", __FUNCTION__));
			ret = BCME_EPERM;
		}
	} else {
		wlc->rfaware_lifetime = lifetime;
	}

	if (wlc->pub->up) {
		wlc_suspend_mac_and_wait(wlc);
		wlc_write_shm(wlc, M_AGING_THRSH(wlc), wlc->rfaware_lifetime);
		wlc_mhf(wlc, MHF4, MHF4_AGING, (wlc->rfaware_lifetime ? MHF4_AGING : 0),
			WLC_BAND_ALL);
		wlc_enable_mac(wlc);
	}

	return ret;
} /* wlc_rcvfifo_limit_get */

wlc_if_t*
wlc_wlcif_alloc(wlc_info_t *wlc, osl_t *osh, uint8 type, wlc_txq_info_t *qi)
{
	wlc_if_t *wlcif;

	wlcif = (wlc_if_t*)MALLOCZ(osh, sizeof(wlc_if_t));
	if (wlcif != NULL) {
		wlcif->type = type;
		wlcif->qi = qi;
		wlcif->_cnt = (wl_if_stats_t *)MALLOCZ(osh, sizeof(wl_if_stats_t));
		if (wlcif->_cnt == NULL) {
			WL_ERROR(("Memory allocation failed for wlcif->_cnt\n"));
			MFREE(osh, wlcif, sizeof(wlc_if_t));
			return NULL;
		}
		/* initiaze version fields in per-port counters structure */
		WLCNTSET(wlcif->_cnt->version, WL_IF_STATS_T_VERSION);
		WLCNTSET(wlcif->_cnt->length, sizeof(wl_if_stats_t));

		/* add this interface to the global list */
		wlcif->next = wlc->wlcif_list;
		wlc->wlcif_list = wlcif;
	}

	return wlcif;
}

void
wlc_wlcif_free(wlc_info_t *wlc, osl_t *osh, wlc_if_t *wlcif)
{
	wlc_if_t *p;

	if (wlcif == NULL)
		return;

	/* remove the interface from the interface linked list */
	p = wlc->wlcif_list;
	if (p == wlcif) {
		wlc->wlcif_list = p->next;
	} else {
		while (p != NULL && p->next != wlcif)
			p = p->next;
		if (p == NULL)
			WL_ERROR(("%s: null ptr2", __FUNCTION__));

		/* assert that we found wlcif before getting to the end of the list */
		ASSERT(p != NULL);

		if (p != NULL) {
			p->next = p->next->next;
		}
	}
	MFREE(osh, wlcif->_cnt, sizeof(wl_if_stats_t));
	MFREE(osh, wlcif, sizeof(wlc_if_t));
}

/** Return the interface pointer for the BSS indexed by idx */
wlc_if_t *
wlc_wlcif_get_by_index(wlc_info_t *wlc, uint idx)
{
	wlc_bsscfg_t	*bsscfg;
	wlc_if_t	*wlcif = NULL;

	if (wlc == NULL)
		return NULL;

	if (idx >= WLC_MAXBSSCFG)
		return NULL;

	bsscfg = wlc->bsscfg[idx];
	if (bsscfg) {
		/* RSDB: WLCIF incorrect index can cause wlc mixup */
		ASSERT(wlc == bsscfg->wlc);
		if (wlc != bsscfg->wlc) {
			WL_ERROR(("RSDB: WLCIF doesnot belong to WLC[%d] for bsscfg[%d]\n",
				wlc->pub->unit, idx));
		}
		wlcif = bsscfg->wlcif;
	}

	return wlcif;
}

void
wlc_wlcif_stats_get(wlc_info_t *wlc, wlc_if_t *wlcif, wl_if_stats_t *wl_if_stats)
{
	BCM_REFERENCE(wlc);
	if ((wlcif == NULL) || (wl_if_stats == NULL))
		return;

	/*
	 * Aggregate errors from other errors
	 * These other errors are only updated when it makes sense
	 * that the error should be charged to a logical interface
	 */
	wlcif->_cnt->txerror = wlcif->_cnt->txnobuf + wlcif->_cnt->txrunt + wlcif->_cnt->txfail;
	wlcif->_cnt->rxerror = wlcif->_cnt->rxnobuf + wlcif->_cnt->rxrunt + wlcif->_cnt->rxfragerr;

	memcpy(wl_if_stats, wlcif->_cnt, sizeof(wl_if_stats_t));
#ifdef RTS_PER_ITF
	if (!MCHAN_ACTIVE(wlc->pub)) {
		/* Local, per-interface stats about RTS only exist
		 * when mchan is active. Fall-back on common numbers otherwise.
		 * Otherwise being single interface or common channel.
		 */
		wlc_statsupd(wlc);
		wl_if_stats->txrts	= wlc->pub->_cnt->txrts;
		wl_if_stats->txnocts	= wlc->pub->_cnt->txnocts;
	}
#endif /* RTS_PER_ITF */
}

/** This function will read the PM duration accumulator maintained by ucode */
static uint32
_wlc_get_accum_pmdur(wlc_info_t *wlc)
{
	uint32 res;
	uint16 prev_st = 0, new_st = 0;
	uint pt, ct;
#define UCODE_STATE_CHANGE_MAX_WAIT_TIME 200

	if (wlc->hw->need_reinit) {
		return wlc->wlc_pm_dur_cntr;
	}

	ct = pt = OSL_SYSUPTIME();

	/* Read the PM dur related values in a stable PSM state */
	do {
		prev_st = wlc_read_shm(wlc, M_UCODE_DBGST(wlc));
		res = wlc_bmac_cca_read_counter(wlc->hw, M_MAC_SLPDUR_L_OFFSET(wlc),
			M_MAC_SLPDUR_H_OFFSET(wlc));
		new_st = wlc_read_shm(wlc, M_UCODE_DBGST(wlc));

	} while ((prev_st != new_st) &&
		(((ct = OSL_SYSUPTIME()) - pt) < UCODE_STATE_CHANGE_MAX_WAIT_TIME));

	/* Waited too long, trigger fatal error */
	if ((ct - pt) >=
		UCODE_STATE_CHANGE_MAX_WAIT_TIME) {
		WL_ERROR(("%s: prev_st:%u, new_st:%u",
			__FUNCTION__, prev_st, new_st));

		if (wlc->hw->need_reinit == WL_REINIT_RC_NONE) {
			wlc->hw->need_reinit = WL_REINIT_RC_MAC_SPIN_WAIT;
		}
		WLC_FATAL_ERROR(wlc);

		/* Return last known value */
		return wlc->wlc_pm_dur_cntr;
	}

	/* wlc_pm_dur_cntr is in ms. */
	res = (res - wlc->wlc_pm_dur_last_sample) / 1000;
	wlc->wlc_pm_dur_cntr += res;

	wlc->pm_dur_clear_timeout = TIMEOUT_TO_READ_PM_DUR;
	wlc->wlc_pm_dur_last_sample += (res * 1000);

	return wlc->wlc_pm_dur_cntr;
}

/** This API can be invoked irrespctive of clk available */
uint32
wlc_get_accum_pmdur(wlc_info_t *wlc)
{
	if (wlc->pub->hw_up)
		return _wlc_get_accum_pmdur(wlc);
	else
		return wlc->wlc_pm_dur_cntr;
}

void
wlc_txstop_intr(wlc_info_t *wlc)
{
#ifdef STA
#ifdef WLRM
	/* handle a radio measurement request for tx suspend */
	if (WLRM_ENAB(wlc->pub) && wlc->rm_info->rm_state->step == WLC_RM_WAIT_TX_SUSPEND)
		wl_add_timer(wlc->wl, wlc->rm_info->rm_timer, 0, 0);
#endif // endif

#ifdef WL11K
	/* handle a radio measurement request for tx suspend */
	if (WL11K_ENAB(wlc->pub) && wlc_rrm_wait_tx_suspend(wlc))
		wlc_rrm_start_timer(wlc);
#endif /* WL11K */

	/* move to next quiet state if both DATA and CTL fifos are suspended */
	if (WL11H_ENAB(wlc) && wlc_bmac_tx_fifo_suspended(wlc->hw, TX_CTL_FIFO))
		wlc_quiet_txfifo_suspend_complete(wlc->quiet);
#endif /* STA */
}

void
wlc_rfd_intr(wlc_info_t *wlc)
{
	uint32 rfd = R_REG(wlc->osh, D11_PHY_DEBUG(wlc)) & PDBG_RFD;

	WL_ERROR(("wl%d: MAC Detected a change on the RF Disable Input 0x%x\n",
		wlc->pub->unit, rfd));

	WLCNTINCR(wlc->pub->_cnt->rfdisable);

	/* delay the cleanup to wl_down in IBSS case */
	if (rfd) {
		int idx;
		wlc_bsscfg_t *bsscfg;
		FOREACH_BSS(wlc, idx, bsscfg) {
			if ((TRUE &&
			     !BSSCFG_STA(bsscfg)) ||
			    !bsscfg->BSS ||
			    !bsscfg->enable)
				continue;
			WL_APSTA_UPDN(("wl%d: wlc_dpc: rfdisable -> wlc_bsscfg_disable()\n",
				wlc->pub->unit));
			wlc_bsscfg_disable(wlc, bsscfg);
		}
	}
}

static void
wlc_iov_get_wlc_ver(wl_wlc_version_t *ver)
{
	ASSERT(ver);

	ver->version = (uint16)WL_WLC_VERSION_T_VERSION;
	ver->length = (uint16)sizeof(wl_wlc_version_t);

	/* set epi version numbers */
	ver->epi_ver_major = (uint16)EPI_MAJOR_VERSION;
	ver->epi_ver_minor = (uint16)EPI_MINOR_VERSION;
	ver->epi_rc_num = (uint16)EPI_RC_NUMBER;
	ver->epi_incr_num = (uint16)EPI_INCREMENTAL_NUMBER;

	/* set WLC interface version numbers */
	ver->wlc_ver_major = (uint16)WLC_API_VERSION_MAJOR;
	ver->wlc_ver_minor = (uint16)WLC_API_VERSION_MINOR;

	/* set version numbers of pprpbw_t and pprpbw_ru_t */
	ver->ppr_version = PPR_VERSION;
	ver->ppr_ru_version = PPR_RU_VERSION;
}

uint8
wlc_template_plcp_offset(wlc_info_t *wlc, ratespec_t rspec)
{
	bool ac_phy = FALSE;
	uint8	offset = 0;

	if (D11REV_GE(wlc->pub->corerev, 40))
		ac_phy = TRUE;

	if (!ac_phy)
		return 0;

	if (D11REV_GE(wlc->pub->corerev, 65))
		return D11AC_PHY_BEACON_PLCP_OFFSET;

	if (RSPEC_ISVHT(rspec)) {
		offset = D11AC_PHY_VHT_PLCP_OFFSET;
	} else if (RSPEC_ISHT(rspec)) {
		offset = D11AC_PHY_HTGF_PLCP_OFFSET;
	} else if (RSPEC_ISOFDM(rspec)) {
		offset = D11AC_PHY_OFDM_PLCP_OFFSET;
	} else {
		offset = D11AC_PHY_CCK_PLCP_OFFSET;
	}
	return offset;
}

uint16
wlc_read_amtinfo_by_idx(wlc_info_t *wlc, uint index)
{
	uint16 ret_val;

	if (D11REV_LT(wlc->pub->corerev, 40))
		return 0;
	ASSERT(index < wlc->pub->max_addrma_idx);
	ASSERT(wlc->pub->m_amt_info_blk > 0);
	ret_val =  wlc_read_shm(wlc, wlc->pub->m_amt_info_blk + (index * 2));
	return ret_val;
}

void
wlc_write_amtinfo_by_idx(wlc_info_t *wlc, uint index, uint16 val)
{
	if (D11REV_LT(wlc->pub->corerev, 40))
		return;

	ASSERT(index < wlc->pub->max_addrma_idx);
	ASSERT(wlc->pub->m_amt_info_blk > 0);
	wlc_write_shm(wlc, wlc->pub->m_amt_info_blk + (index * 2), val);
}

#ifndef WLC_DISABLE_ACI
void
wlc_weakest_link_rssi_chan_stats_upd(wlc_info_t *wlc)
{
	int idx;
	struct scb *scb;
	struct scb_iter scbiter;
	wlc_bsscfg_t *bsscfg;
	int8 rssi = 0;
	wlc_phy_t *pi;

	/* Example chanArray and rssiArray
	  * chanArray		6		11		36		104
	  * rssiArray		-45 	-67 	-87 	-25
	  */
	chanspec_t chanArray[MAX_MCHAN_CHANNELS];
	int8 rssiArray[MAX_MCHAN_CHANNELS];
	int chanLen = 0;
	int j;

#if defined(ISID_STATS) && defined(CCA_STATS)
	int sample_time = 1;
	wlc_congest_channel_req_t *result;
	uint result_len;
	int status;
	chanspec_t chanspec;
#else
	bool valid;
	chanim_stats_t stats;
	chanspec_t chanspec;
#endif /* CCA_STATS  ISID_STATS */

	bzero(chanArray, MAX_MCHAN_CHANNELS * sizeof(chanspec_t));
	bzero(rssiArray, MAX_MCHAN_CHANNELS * sizeof(int8));

	FOREACH_BSS(wlc, idx, bsscfg) {
		/* Should not save channel/rssi in array if not UP. */
		if (!bsscfg->up)
			continue;
		for (j = 0; j < MAX_MCHAN_CHANNELS; j++) {
			if (chanArray[j] == 0) {
				chanArray[j] = bsscfg->current_bss->chanspec;
				/* Use this 'j' to compare RSSI and update the least RSSI */
				chanLen++;
				break;
			} else if (chanArray[j] == bsscfg->current_bss->chanspec) {
				/* Use this 'j' to compare RSSI and update the least RSSI */
				break;
			}
		}
		/* if (chanLen == MAX_MCHAN_CHANNELS)
		  * Number of different channels > MAX_MCHAN_CHANNELS
		  */
		ASSERT(chanLen <= MAX_MCHAN_CHANNELS);
		/* if ('j' == MAX_MCHAN_CHANNELS)
		  * Number of different channels > MAX_MCHAN_CHANNELS
		  */
		ASSERT(j < MAX_MCHAN_CHANNELS);

		if (j == MAX_MCHAN_CHANNELS)
			continue;

		FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
				if (SCB_ASSOCIATED(scb) ||
					(!bsscfg->BSS && !ETHER_ISBCAST(&scb->ea))) {
					scb->rssi = rssi = wlc_lq_rssi_get(wlc, bsscfg, scb);
				if (rssiArray[j] > rssi) {
					rssiArray[j] = rssi;
				}
			}
		}
	}

#if defined(ISID_STATS) && defined(CCA_STATS)
	result_len = sizeof(wlc_congest_channel_req_t) +
		((sample_time - 1) * sizeof(wlc_congest_t));
	result = (wlc_congest_channel_req_t*)MALLOC(wlc->osh, result_len);
#endif /* CCA_STATS  ISID_STATS */
	pi = WLC_PI(wlc);
	for (j = 0; j < chanLen; j++) {
		wlc_phy_interf_rssi_update(pi, chanArray[j], rssiArray[j]);

#if defined(ISID_STATS) && defined(CCA_STATS)
		chanspec = wf_chspec_ctlchspec(chanArray[j]);  /* ctrl chan based */
		status = cca_query_stats(wlc, chanspec, sample_time, result, result_len);
		if (status == 0)
			phy_noise_interf_chan_stats_upd(pi, chanArray[j],
			   result->secs[0].crsglitch, result->secs[0].bphy_crsglitch,
			   result->secs[0].badplcp, result->secs[0].bphy_badplcp,
			                                   0, result->secs[0].duration);
#else /* CCA_STATS  ISID_STATS */
		chanspec = chanArray[j];  /* center chan based */
		memset(&stats, 0, sizeof(stats));
		valid = FALSE;
#ifdef WLCHANIM
		if (WLC_CHANIM_ENAB(wlc->pub)) {
			valid = wlc_lq_chanim_stats_get(wlc->chanim_info, chanspec, &stats);
		}
#endif // endif
		WL_CHANINT(("Is valid flag is %d for channel %X \n", valid, chanspec));
		if (valid) {
			phy_noise_interf_chan_stats_upd(pi, chanArray[j],
				stats.glitchcnt, stats.bphy_glitchcnt,
				stats.badplcp, stats.bphy_badplcp,
				stats.chan_idle, 1000);
		}
		BCM_REFERENCE(chanspec);
#endif /* CCA_STATS  ISID_STATS */
	}

#if defined(ISID_STATS) && defined(CCA_STATS)
	MFREE(wlc->osh, result, result_len);
#endif /* CCA_STATS  ISID_STATS */
} /* wlc_weakest_link_rssi_chan_stats_upd */
#endif /* WLC_DISABLE_ACI */

bool
wlc_valid_pkt_field(osl_t *osh, void *pkt, void *ptr, int len)
{
	uint8 *pb = (uint8 *)PKTDATA(osh, pkt);
	uint8 *pe = pb + PKTLEN(osh, pkt);
	uint8 *fb = (uint8 *)ptr;
	uint8 *fe = (uint8 *)fb + len;
	return (len == 0) ||
		((pb <= fb && fb <= pe) && (pb <= fe && fe <= pe));
}

int wlc_is_singleband_5g(unsigned int device, unsigned int corecap)
{
	return (_IS_SINGLEBAND_5G(device) ||
		((corecap & PHY_PREATTACH_CAP_SUP_5G) && !(corecap & PHY_PREATTACH_CAP_SUP_2G)));
}

static void
wlc_copy_peer_info(wlc_bsscfg_t *bsscfg, struct scb *scb, bss_peer_info_t *peer_info)
{
	wlc_info_t *wlc = bsscfg->wlc;
#ifdef WLCNTSCB
	uint32 tx_rate = RSPEC2KBPS(scb->scb_stats.tx_rate);
	uint32 rx_rate = RSPEC2KBPS(scb->scb_stats.rx_rate);
	ratespec_t rspec = wlc_get_rspec_history(bsscfg);
	uint32 rate = RSPEC2KBPS(rspec);
	uint32 rate_max = -1;
#endif /* WLCNTSCB */

	store16_ua((uint8 *)&peer_info->version, BSS_PEER_INFO_CUR_VER);
	bcopy((void*)&scb->ea, (void*)&peer_info->ea, ETHER_ADDR_LEN);
	bcopy(&scb->rateset, &peer_info->rateset, sizeof(wl_rateset_t));
	store32_ua((uint8 *)&peer_info->rssi, wlc_lq_rssi_get(wlc, bsscfg, scb));
#ifdef WLCNTSCB
	store32_ua((uint8 *)&peer_info->tx_rate, tx_rate == rate_max ? rate:tx_rate);
	store32_ua((uint8 *)&peer_info->rx_rate, rx_rate == rate_max ? rate:rx_rate);
#endif /* WLCNTSCB */
	store32_ua((uint8 *)&peer_info->age, (wlc->pub->now - scb->used));
}

#ifdef STA
/**
 * Update the DCF/AC NULLDP Template with all 3 mac addresses.
 * The order is: BSSID, TA and RA.  Each BSS has its own null frame template.
 */
void
wlc_nulldp_tpl_upd(wlc_bsscfg_t *cfg)
{
	char template[T_P2P_NULL_TPL_SIZE];
	struct dot11_header *mac_header;
	wlc_info_t *wlc = cfg->wlc;
	int bss_idx = 0;
	uint32 base;
	uint8 size;

#ifdef WLMCNX
	if (MCNX_ENAB(wlc->pub))
		bss_idx = wlc_mcnx_BSS_idx(wlc->mcnx, cfg);
#endif /* WLMCNX */

	bzero(template, sizeof(template));

	if (D11REV_LT(wlc->pub->corerev, 40)) {
		/* Non-ac chips - Update PLCP header and mac addresses */
		wlc_compute_plcp(wlc, cfg, WLC_RATE_1M, (DOT11_MAC_HDR_LEN + DOT11_FCS_LEN),
			FC_TODS, (uint8*)template);
		mac_header = (struct dot11_header *) ((char *)template + 6);
		mac_header->fc = FC_NULL_DATA;
		mac_header->durid = 0;
		base = (T_P2P_NULL_TPL_BASE + (bss_idx * T_P2P_NULL_TPL_SIZE));
		size = T_P2P_NULL_TPL_SIZE;
	} else {
		/* ac chips  - update mac addresses */
		mac_header = (struct dot11_header *) template;
		base = (D11AC_T_NULL_TPL_BASE + (bss_idx * D11AC_T_NULL_TPL_SIZE_BYTES));
		size = D11AC_T_NULL_TPL_SIZE_BYTES;
	}
	bcopy((char*)&cfg->BSSID, (char *)&mac_header->a1, ETHER_ADDR_LEN);
	bcopy((char*)&cfg->cur_etheraddr, (char *)&mac_header->a2, ETHER_ADDR_LEN);
	bcopy((char*)&cfg->BSSID, (char *)&mac_header->a3, ETHER_ADDR_LEN);

	wlc_write_template_ram(wlc, base, size, template);
} /* wlc_nulldp_tpl_upd */
#endif /* STA */

void wlc_check_txq_fc(wlc_info_t *wlc, wlc_txq_info_t *qi)
{
	if (!WME_ENAB(wlc->pub) || (wlc->pub->wlfeatureflag & WL_SWFL_FLOWCONTROL)) {
		if (wlc_txflowcontrol_prio_isset(wlc, qi, ALLPRIO) &&
		    (pktq_n_pkts_tot(WLC_GET_CQ(qi)) < wlc->pub->tunables->datahiwat / 2)) {
			wlc_txflowcontrol(wlc, qi, OFF, ALLPRIO);
		}
	} else if (wlc->pub->_priofc) {
		int prio;
		/* XXX WES: consider keeping a bitmask of prios that were sent above
		 * so that the code below would only check the those prios.
		 * A common case would be only 1 prio (best effort) being sent for
		 * the majority of calls to this fn.
		 */
		for (prio = MAXPRIO; prio >= 0; prio--) {
			if (wlc_txflowcontrol_prio_isset(wlc, qi, prio) &&
			    pktqprec_n_pkts(WLC_GET_CQ(qi), wlc_prio2prec_map[prio]) <
			    wlc->pub->tunables->datahiwat/2) {
				wlc_txflowcontrol(wlc, qi, OFF, prio);
			}
		}
	}
}

/** call this function to update wlc->band and wlc->pi pointer with active band_index */
void wlc_pi_band_update(wlc_info_t *wlc, uint band_index)
{
	wlc->band = wlc->bandstate[band_index];
	wlc->pi = WLC_PI_BANDUNIT((wlc), band_index);
}

void
wlc_devpwrstchg_change(wlc_info_t *wlc, bool hostmem_access_enabled)
{
	wlc_cmn_info_t *wlc_cmn = wlc->cmn;
#ifdef BCMPCIEDEV
	wlc_info_t *wlc_iter;
	int idx;
	bool rx_hostmem_access_enabled;
#endif /* BCMPCIEDEV */
	wlc_cmn->hostmem_access_enabled = hostmem_access_enabled;

#ifdef BCMPCIEDEV
	if (BCMPCIEDEV_ENAB()) {
#ifdef BCMFRWDPOOLREORG
		if (BCMFRWDPOOLREORG_ENAB()) {
			rx_hostmem_access_enabled = wlc_poolreorg_devpwrstchg(wlc,
					hostmem_access_enabled);
		} else
#endif /* BCMFRWDPOOLREORG */
		{
			rx_hostmem_access_enabled = hostmem_access_enabled;
		}

		FOREACH_WLC(wlc_cmn, idx, wlc_iter) {
		/* Handle power state change handler for each wlc here to handle RSDB. */
			wlc_bmac_enable_rx_hostmem_access(wlc_iter->hw,
				rx_hostmem_access_enabled);
			wlc_bmac_enable_tx_hostmem_access(wlc_iter->hw,
				wlc_iter->cmn->hostmem_access_enabled);
		}
	}
#endif /* BCMPCIEDEV */

#ifdef WLC_TSYNC
	if (TSYNC_ENAB(wlc->pub)) {
		wlc_tsync_process_pmevt(wlc->tsync, hostmem_access_enabled);
	}
#endif // endif
}

/* This function is to get chains's up status
* return FALSE
*	all chains are down
* return TRUE
*	Atleast one chain is up
*/
int wlc_isup(wlc_info_t *wlc)
{
	int idx;
	wlc_info_t *wlc_iter;
	int rval = FALSE;

	FOREACH_WLC(wlc->cmn, idx, wlc_iter) {
		if (wlc_iter->pub->up) {
			rval = TRUE;
		}
	}
	return (rval);
}

/* XXX: called by the per port code at this point while handling the trap notification from OS
 * halt the hardware and keep in sanitized state so that DMA's won't affect the state
*/
uint32 wlc_halt_device(wlc_info_t *wlc)
{
	if (wlc->pub->up) {
		if (wlc->clk) {
#ifdef DONGLEBUILD
			bool suspend_mac = FALSE;
			/* lets try to gather reg dump, if not already done */
			if (UCODEDUMP_ENAB(wlc->pub)) {
				wlc_handle_fatal_error_dump(wlc);
			}
			if ((R_REG(wlc->osh, D11_MACCONTROL(wlc)) & MCTL_EN_MAC) &&
				(wlc->pub->_cnt->reinit == 0)) {
					wlc->mac_suspended_for_halt = TRUE;
					suspend_mac = TRUE;
			}
			wlc_bmac_handle_device_halt(wlc->hw, TRUE, suspend_mac, TRUE);
#endif /* DONGLEBUILD */
			return (1 << wlc->core->coreidx);
		}
	}
	return 0;
}

INLINE bool
rfc894chk(wlc_info_t *wlc, struct dot11_llc_snap_header *lsh)
{
	BCM_REFERENCE(wlc);
	return lsh->dsap == 0xaa && lsh->ssap == 0xaa && lsh->ctl == 0x03 &&
	        lsh->oui[0] == 0 && lsh->oui[1] == 0 &&
	        ((lsh->oui[2] == 0x00 && !SSTLOOKUP(ntoh16(lsh->type))) ||
	         (lsh->oui[2] == 0xf8 && SSTLOOKUP(ntoh16(lsh->type))));
}

/** Convert 802.3 back to ethernet per 802.1H (use bridge-tunnel if type in SST) */
void BCMFASTPATH
wlc_8023_etherhdr(wlc_info_t *wlc, osl_t *osh, void *p)
{
	struct ether_header *neh, *eh;
	struct dot11_llc_snap_header *lsh;

	if (PKTLEN(osh, p) < DOT11_LLC_SNAP_HDR_LEN)
		return;

	eh = (struct ether_header *) PKTDATA(osh, p);

	/*
	 * check conversion is necessary
	 * - if ([AA AA 03 00 00 00] and protocol is not in SST) or
	 *   if ([AA AA 03 00 00 F8] and protocol is in SST), convert to RFC894
	 * - otherwise,
	 *	 preserve 802.3 (including RFC1042 with protocol in SST)
	 */
	lsh = (struct dot11_llc_snap_header *)((uint8 *)eh + ETHER_HDR_LEN);
	if (!rfc894chk(wlc, lsh))
		return;

	/* 802.3 MAC header */
	neh = (struct ether_header *) PKTPULL(osh, p, DOT11_LLC_SNAP_HDR_LEN);
	eacopy((char*)eh->ether_shost, (char*)neh->ether_shost);
	eacopy((char*)eh->ether_dhost, (char*)neh->ether_dhost);

	/* no change to the ether_type field */

	return;
} /* wlc_8023_etherhdr */

void
wlc_generate_pme_to_host(wlc_info_t *wlc, bool pme_state)
{
#ifdef BCMPCIEDEV
	if (BCMPCIEDEV_ENAB()) {
#ifdef WLRSDB
		if (RSDB_ENAB(wlc->pub) &&
			wlc_isup(wlc) && wlc_bmac_rsdb_cap(wlc->hw)) {
				WL_INFORM(("%s: driver state is up, do PME\n", __FUNCTION__));
		} else
#endif // endif
		if (!wlc->pub->up) {
			bool mpc = wlc->mpc;
			WL_INFORM(("%s: driver state down...bring it up\n", __FUNCTION__));
			wlc->mpc = 0;
			wlc_radio_mpc_upd(wlc);
			wl_up(wlc->wl);
			wlc->mpc = mpc;
			if (!wlc->pub->up) {
				wl_down(wlc->wl);
				WL_ERROR(("%s: driver still not up,"
					"no PME to host\n", __FUNCTION__));
				return;
			}
		}
	/* Assert PME by write to PSM mac intstatus, but clear through write to mac intstatus */
		if (pme_state) {
			si_wrapperreg(wlc->pub->sih, AI_IOCTRL, AI_IOCTRL_ENABLE_D11_PME,
				AI_IOCTRL_ENABLE_D11_PME);
			OR_REG(wlc->osh, D11_PSM_MAC_INTSTAT_L(wlc), MI_PME);
		} else {
			AND_REG(wlc->osh, D11_MACINTSTATUS(wlc), MI_PME);
		}
	}
#else
	BCM_REFERENCE(wlc);
	BCM_REFERENCE(pme_state);
#endif /* BCMPCIEDEV */
}

/**
 * (de)authorize/(de)authenticate single station
 * 'enable' TRUE means authorize, FALSE means deauthorize/deauthenticate
 * 'flag' is AUTHORIZED or AUTHENTICATED for the type of operation
 * 'rc' is the reason code for a deauthenticate packet
 */
int
wlc_scb_set_auth(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb, bool enable, uint32 flag,
                 int rc)
{
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_ASSOC */
	int ret = BCME_OK;

	if (SCB_LEGACY_WDS(scb)) {
		WL_ERROR(("wl%d.%d %s: WDS=" MACF " enable=%d flag=%x\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg),
			__FUNCTION__, ETHERP_TO_MACF(&scb->ea), enable, flag));
	}

	if (enable) {
		if (flag == AUTHORIZED) {
			wlc_scb_setstatebit(wlc, scb, AUTHORIZED);
			scb->flags &= ~SCB_DEAUTH;

			if (BSSCFG_AP(bsscfg) &&
			    wlc_eventq_test_ind(wlc->eventq, WLC_E_AUTHORIZED)) {
				wlc_bss_mac_event(wlc, bsscfg, WLC_E_AUTHORIZED,
					(struct ether_addr *)&scb->ea,
					WLC_E_AUTHORIZED, 0, 0, 0, 0);
			}
#if defined(WL11N) && defined(WLAMPDU)
			if (SCB_MFP(scb) &&
			    N_ENAB(wlc->pub) && SCB_AMPDU(scb) &&
			    (scb->wsec == AES_ENABLED)) {
				wlc_scb_ampdu_enable(wlc, scb);
			}
#endif /* WL11N */
		} else {
			wlc_scb_setstatebit(wlc, scb, AUTHENTICATED);
		}
	} else {
		if (flag == AUTHORIZED) {
			wlc_scb_clearstatebit(wlc, scb, AUTHORIZED);
		} else {
			if (wlc->pub->up && (SCB_AUTHENTICATED(scb) || SCB_LEGACY_WDS(scb)) &&
				!scb->sent_deauth) {
				/* It was observed that if AP is running in DWDS AP mode along with
				 * another DWDS repeater and both have the same ssid, one of them
				 * can have the stale entry of client if client was earlier
				 * connected to either of two and later join to another with wl
				 * join command.
				 *
				 * This stale entry persist at AP/DWDS repeater end. The reason for
				 * this behaviour is due to ucode level ACK is coming for the Qos
				 * NUll frame being sent from the AP/DWDS repeater if client's
				 * idle timeout is over.
				 *
				 * Proposed solution:-> Remove client's entry via IOVAR requested
				 * by Application. This is the task of Application to figure out
				 * the stale entry of client among the AP/Repeaters.
				 *
				 * Mark SCB for Deletion, in case STA already associated/roamed to
				 * another DWDS interface(or Repeater). No need to send explicit
				 * deauth for this case.
				 */
				if (rc == DOT11_RC_DEAUTH_LEAVING) {
					/* Clear states and mark the scb for deletion. SCB free
					 * will happen from the inactivity timeout context in
					 * wlc_ap_stastimeout()
					 * Mark the scb for deletion first as some scb state change
					 * notify callback functions need to be informed that the
					 * scb is about to be deleted.
					 * (For example wlc_cfp_scb_state_upd)
					 */
					wlc_bss_mac_event(wlc, SCB_BSSCFG(scb), WLC_E_DEAUTH,
						(struct ether_addr *)&scb->ea,
						WLC_E_STATUS_SUCCESS, rc, 0, 0, 0);

					SCB_MARK_DEL(scb);
					wlc_scb_clearstatebit(wlc, scb, AUTHENTICATED | ASSOCIATED
							| AUTHORIZED);
				} else {
					wlc_send_deauth(wlc, bsscfg, scb, &scb->ea, &bsscfg->BSSID,
							&bsscfg->cur_etheraddr, (uint16)rc);
				}
			}
		}
	}
	WL_ASSOC(("wl%d: %s: %s %s%s\n", wlc->pub->unit, __FUNCTION__,
		bcm_ether_ntoa(&scb->ea, eabuf),
		(enable ? "" : "de"),
		((flag == AUTHORIZED) ? "authorized" : "authenticated")));

	/* update link entry */
	if (RATELINKMEM_ENAB(wlc->pub) == TRUE) {
		wlc_ratelinkmem_update_link_entry(wlc, scb);
	}

	return (ret);
} /* wlc_scb_set_auth */

/** write into shared memory to send CTS to no where */
void
wlc_cts_to_nowhere(wlc_info_t *wlc, uint16 duration)
{
	int mac_depth = 0;
	int orig_state;

	/* Make sure that we have a duration value greater than zero.
	 * If not it doesn't make sense to send CTS to nowhere with zero
	 * duration in it
	 */
	if (duration == 0 || duration > MAX_DCF_DURATION) {
		WL_ERROR(("%s: Invalid duration sent = %d\n", __FUNCTION__,
			duration));
		return;
	}

	/* Save the state of the MAC */
	orig_state = R_REG(wlc->osh, D11_MACCONTROL(wlc)) & MCTL_EN_MAC;

	/* Make sure that MAC is enabled before writing into shared memory
	 * so that CTS to nowhere is sent as soon as we write into the
	 * shared memeory
	 */
	while ((mac_depth < MAX_MAC_ENABLE_RETRIES) &&
		!(R_REG(wlc->osh, D11_MACCONTROL(wlc)) & MCTL_EN_MAC)) {
		/* Unsuspend mac */
		wlc_enable_mac(wlc);
		mac_depth++;
	}

	/* Check if MAC is enabled or not */
	ASSERT((R_REG(wlc->osh, D11_MACCONTROL(wlc)) & MCTL_EN_MAC) != 0);

	wlc_write_shm(wlc, M_CTS_DURATION(wlc), duration);

	/* Once we are done send back the MAC to the original state */
	while (mac_depth) {
		/* Leave the mac in its original state */
		wlc_suspend_mac_and_wait(wlc);
		mac_depth--;
	}

	/* Enable MAC if it was previously enabled when we entered this function */
	if (orig_state)
		wlc_enable_mac(wlc);
} /* wlc_cts_to_nowhere */

void
wlc_bsscfg_set_current_bss_chan(wlc_bsscfg_t *bsscfg, chanspec_t cspec)
{
	wlc_bss_info_t *current_bss = bsscfg->current_bss;

	current_bss->chanspec = cspec;
	wlc_ht_update_txburst_limit(bsscfg->wlc->hti, bsscfg);
#if defined(AP)
	wlc_ap_channel_switch(bsscfg->wlc->ap, bsscfg);
#endif // endif
}

/* The PMU time may change as we are reading it. It is updated by hw every tick.
 * It needs to be read twice and if the values are different, read it one one more time
 */
uint32 wlc_current_pmu_time(wlc_info_t *wlc)
{
	return si_pmu_get_pmutimer(wlc->pub->sih);
}

#if defined(WL_EVENT_LOG_COMPILE)
void
wlc_log_event(wlc_info_t *wlc, wlc_event_t *e)
{
	uint msg = e->event.event_type;
	struct ether_addr *addr = e->addr;
	uint result = e->event.status;
	wl_event_log_tlv_hdr_t tlv_log = {{0, 0}};
	uint32 tlv_log_tmp = 0;

	switch (msg) {
	case WLC_E_DISASSOC:
	case WLC_E_EAPOL_MSG:
		WL_EVENT_LOG((EVENT_LOG_TAG_TRACE_WL_INFO, msg));
		break;

	case WLC_E_BEACON_RX:
		WL_EVENT_LOG((EVENT_LOG_TAG_BEACON_LOG, msg));
		break;

	case WLC_E_ASSOC:
	case WLC_E_REASSOC:
	case WLC_E_AUTH:

		tlv_log.tag = TRACE_TAG_STATUS;
		tlv_log.length = sizeof(uint);
		tlv_log_tmp = tlv_log.t;

		tlv_log.tag = TRACE_TAG_BSSID;
		tlv_log.length = 6;

		WL_EVENT_LOG((EVENT_LOG_TAG_TRACE_WL_INFO, msg,
			tlv_log_tmp, result,
			tlv_log.t, ETHER_ADDR_PACK_LOW(addr), ETHER_ADDR_PACK_HI(addr)));
		break;

	default:
		break;
	}
}
#endif /* WL_EVENT_LOG_COMPILE */

#ifndef DONGLEBUILD
int
wlc_halt_hw(wlc_info_t * wlc)
{
	if (wlc->hw == NULL) {
		WL_ERROR(("%s, wlc->hw == NULL\n", __FUNCTION__));
		return BCME_NODEVICE;
	}

	if (!wlc->hw->clk) {
		return BCME_NOCLK;
	}

	/* Halt ucode */
	wlc_bmac_mctrl(wlc->hw, MCTL_PSM_RUN, 0);
	wlc_bmac_write_ihr(wlc->hw, TSF_GPT_0_STAT, 0);

	return BCME_OK;
}
#endif /* !DONGLEBUILD */

uint32
wlc_need_reinit(wlc_info_t * wlc)
{
#ifdef WAR4360_UCODE
	return wlc_bmac_need_reinit(wlc->hw);
#else
	return 0;
#endif /* WAR4360_UCODE */
}

/* force the d11/ucode to run at HT clock and force the ucode/d11 to stay awake */
bool
wlc_force_ht(wlc_info_t *wlc, bool force, bool *prev)
{
	osl_t *osh;
	volatile uint32 *ccs;

	if (prev != NULL)
		*prev = wlc->wake;

	wlc->wake = force;
	wlc_set_wake_ctrl(wlc);

	if (force) {
		osh = wlc->osh;
		ccs = D11_ClockCtlStatus(wlc);

		/* make sure the HT is indeed running... */
		SPINWAIT((R_REG(osh, ccs) & CCS_HTAVAIL) == 0, PMU_MAX_TRANSITION_DLY);
		ASSERT(R_REG(osh, ccs) & CCS_HTAVAIL);

		if (!(R_REG(osh, ccs) & CCS_HTAVAIL)) {
			return FALSE;
		}
	}
	return TRUE;
}

void
wlc_srpwr_request_on(wlc_info_t *wlc)
{
	uint domain = wlc_bmac_coreunit(wlc) == DUALMAC_MAIN ?
		SRPWR_DMN3_MACMAIN_MASK : SRPWR_DMN2_MACAUX_MASK;

	si_srpwr_request(wlc->pub->sih, domain, domain);
}

void
wlc_srpwr_request_off(wlc_info_t *wlc)
{
	uint domain = wlc_bmac_coreunit(wlc) == DUALMAC_MAIN ?
		SRPWR_DMN3_MACMAIN_MASK : SRPWR_DMN2_MACAUX_MASK;

	si_srpwr_request(wlc->pub->sih, domain, 0);
}

void
wlc_srpwr_request_on_all(wlc_info_t *wlc, bool force_pwr)
{
	wlc_cmn_info_t *wlc_cmn = wlc->cmn;
	wlc_info_t *wlc_iter;
	int idx;

	FOREACH_WLC(wlc_cmn, idx, wlc_iter) {
		if (force_pwr|| wlc_iter->clk) {
			wlc_srpwr_request_on(wlc_iter);
		}
	}
}

void
wlc_srpwr_spinwait(wlc_info_t *wlc)
{
	uint st, domain = wlc_bmac_coreunit(wlc) == DUALMAC_MAIN ?
		SRPWR_DMN3_MACMAIN_MASK : SRPWR_DMN2_MACAUX_MASK;

	st = si_srpwr_stat_spinwait(wlc->pub->sih, domain, domain);
	BCM_REFERENCE(st);
	ASSERT(st & domain);
}

/* EVENT_LOG_COMPILE flag present below will be removed after
 * MACOS completes porting of ecounters. Without this flag,
 * precommit fails in NIC and NIC off load builds.
 */
#if defined(WLCNT) && defined(ECOUNTERS) && defined(EVENT_LOG_COMPILE)

/* For WL counters, we will return everything just like what is returned
 * when wl counters iovar is issued.
 * The const bcm_xtlv_t* tlv parameter is ignored for now.
 */

static
int wlc_ecounters_wl_counters(uint16 stats_type, void *context,
	const ecounters_stats_types_report_req_t *req,
	struct bcm_xtlvbuf *xtlvbuf, uint32 *cookie, const bcm_xtlv_t* tlv,
	uint16 *attempted_write_len)

{
	int rc = BCME_OK;
	wlc_info_t *wlc = (wlc_info_t *)context;
	uint16 slice_index = 0, index, write_len = 0;
	struct bcm_xtlvbuf local_xtlvbuf;

	xtlv_desc_t *tlv_desc, xtlv_desc[] = {
		{WL_CNT_XTLV_WLC, sizeof(wl_cnt_wlc_t), NULL},
		{WL_CNT_XTLV_WLC_RINIT_RSN, sizeof(reinit_rsns_t),
		NULL},
		{WL_CNT_XTLV_GE40_UCODE_V1, WL_CNT_MCST_STRUCT_SZ,
		NULL}
	};

	/* Should not happen */
	if (stats_type != WL_IFSTATS_XTLV_WL_SLICE_V30_WLCNTRS) {
		return BCME_ERROR;
	}

	/* slice index to report Only two slices to choose from */
	slice_index = (req->slice_mask &
			ECOUNTERS_STATS_TYPES_SLICE_MASK_SLICE0) ? 0 : 1;

	/* Get the right WLC */
	wlc = RSDB_ENAB(wlc->pub) ? wlc->cmn->wlc[slice_index] : wlc;

	/* Fill in right location to grab stats */
	xtlv_desc[0].ptr = wlc->pub->_cnt;
	xtlv_desc[1].ptr = wlc->pub->reinitrsn;
	xtlv_desc[2].ptr = wlc->pub->_mcst_cnt;

	/* First time here */
	if (*cookie == ECOUNTERS_CLIENT_PARAM_COOKIE_INVALID) {
		index = 0;
		/* Report stats from a single snapshot */
		if (WLC_UPDATE_STATS(wlc)) {
			wlc_statsupd(wlc);
		}
	} else {
		index = *cookie;

		if (index >= ARRAYSIZE(xtlv_desc)) {
			/* Illegal configuration. We are done */
			*cookie = ECOUNTERS_CLIENT_PARAM_COOKIE_INVALID;
			rc = BCME_ERROR;
			goto fail;
		}
	}

	/* This one needs a container. */
	/* So create a local xtlv buffer context to fill in data */
	bcm_xtlv_buf_init(&local_xtlvbuf,
		(uint8 *) (bcm_xtlv_buf(xtlvbuf) + BCM_XTLV_HDR_SIZE),
		(bcm_xtlv_buf_rlen(xtlvbuf) - BCM_XTLV_HDR_SIZE),
		BCM_XTLV_OPTION_ALIGN32);

	/* These many bytes were "taken" in buffer preparation.
	 * i.e. the XTLV header size is consumed for container
	 * type and length mentioned above
	 */
	write_len = BCM_XTLV_HDR_SIZE;

	while (index < ARRAYSIZE(xtlv_desc)) {
		tlv_desc = &xtlv_desc[index];
		/* Write data in the allocated buffer */
		rc = bcm_xtlv_put_data(&local_xtlvbuf,
			tlv_desc->type,
			(const uint8 *) tlv_desc->ptr, tlv_desc->len);

		if (rc == BCME_NOMEM || rc == BCME_BUFTOOSHORT) {
			uint16 local_len;

			/* We could not fit in data but did we write anything
			 * meaningful i.e. at least one complete XTLV?
			 */
			local_len = bcm_xtlv_buf_len(&local_xtlvbuf);

			/* Yes we wrote at leat one XTLV. Then complete the container
			 * and tell how much data could not fit in the buffer provided.
			 */
			if (local_len) {
				/* Complete the outer container with type and length
				 * only.
				 */
				bcm_xtlv_put_data(xtlvbuf,
					stats_type,
					NULL, local_len);
			}

			/* buf too short happened when this much length was about
			 * to be written to buffer passed in xtlv context.
			 */
			/* Failed to write complete TLV so note it */
			/* Ask some more space as we also need to fill the container
			 * type and length.
			 */
			write_len += tlv_desc->len + BCM_XTLV_HDR_SIZE;
			*attempted_write_len = write_len;

			/* We stopped at this index. When called back, we will resume
			 * from this value.
			 */
			*cookie = index;
			rc = BCME_BUFTOOSHORT;
			break;
		}
		index++;
	}

	/* Done finally. Fill up the container */
	if (rc == BCME_OK) {
		/* Complete the outer container with type and length
		 * only.
		 */
		bcm_xtlv_put_data(xtlvbuf,
			stats_type,
			NULL, bcm_xtlv_buf_len(&local_xtlvbuf));
		/* Clean up cookie usage after our work. */
		*cookie = ECOUNTERS_CLIENT_PARAM_COOKIE_INVALID;
	}

fail:
	return rc;
}
#endif /* defined(WLCNT) && defined(ECOUNTERS) */

#ifdef ENABLE_CORECAPTURE
int wlc_dump_mem(wlc_info_t *wlc, int type)
{

	if (si_deviceremoved(wlc->pub->sih)) {
		return BCME_NODEVICE;
	}

	if (type == WL_DUMP_MEM_UCM) {
		return wlc_ucode_dump_ucm(wlc->hw);
	}

	return BCME_BADARG;
}
#endif /* ENABLE_CORECAPTURE */

bool
wlc_srpwr_stat(wlc_info_t *wlc)
{
	uint st, domain = wlc_bmac_coreunit(wlc) == DUALMAC_MAIN ?
		SRPWR_DMN3_MACMAIN_MASK : SRPWR_DMN2_MACAUX_MASK;

	st = si_srpwr_stat(wlc->pub->sih);

	return (st & domain) ? TRUE : FALSE;
}

int
wlc_get_weakest_link_chain_rssi(wlc_info_t *wlc)
{
	int8 weakest_rssi = 0;
	uint8 as_count = 0;
	int idx;

	int8 i, this_rssi;
	wlc_bsscfg_t* cfg;
	struct scb *scb;
	struct scb_iter scbiter;
	as_count = wlc->aps_associated + wlc->stas_connected;

	if (!as_count) {
		return INVALID_RSSI;
	}

	FOREACH_AS_BSS(wlc, idx, cfg) {
		if (wlc->chanspec == cfg->current_bss->chanspec) {
			FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {
				for (i = 0; i < (wlc->stf->rxchain); i++) {
					if (((wlc->stf->rxchain >> i) & 1) == 0) {
						this_rssi = 0;
					} else {
						this_rssi = wlc_lq_ant_rssi_get(wlc,
							cfg, scb, i);
					}

					if (weakest_rssi > this_rssi) {
						weakest_rssi = this_rssi;
					}
				}
			}
		}
	}

	if (weakest_rssi == 0) {
		weakest_rssi = INVALID_RSSI;
	}

	return weakest_rssi;
}

#if defined(WL_DD_HANDLER)
/**
 * Handle the Health Check 'hc' iovar
 */
static int
wlc_hc_iovar_handler(wlc_info_t *wlc, struct wlc_if *wlcif, bool isset,
                     void *params, uint p_len, void *out, uint o_len)
{
	const bcm_xtlv_opts_t no_pad = BCM_XTLV_OPTION_NONE;
	const bcm_xtlv_opts_t align = BCM_XTLV_OPTION_ALIGN32;
	bcm_xtlv_t *hc_tlv;
	uint16 len, hc_category;
	int err;

	/* dbg prints if needed */
	WL_NONE(("HCIOVar: params %p plen %u out %p olen %u\n", params, p_len, out, o_len));
	/* prhex("HCIOVar", (uchar *) params, p_len); */

	/* all commands start with an XTLV container */
	hc_tlv = (bcm_xtlv_t*)params;

	if (p_len < (uint)BCM_XTLV_HDR_SIZE || !bcm_valid_xtlv(hc_tlv, p_len, no_pad)) {
		return BCME_BADARG;
	}

	/* collect the LE id/len values */
	hc_category = ltoh16(hc_tlv->id);
	len = ltoh16(hc_tlv->len);

	/* contianer must have room for at least one xtlv header */
	if (len < BCM_XTLV_HDR_SIZE) {
		return BCME_BADARG;
	}

	if (isset) {
		/* SET handling */

		/* init context struct for xtlv unpack callback fns */
		struct wlc_hc_ctx hc_ctx = {wlc, wlcif};

		/* on SET, container payload is sequence of XTLVs attributes */
		switch (hc_category) {
#if defined(WL_TX_STALL)
		case WL_HC_XTLV_ID_CAT_DATAPATH_TX:
			err = bcm_unpack_xtlv_buf(&hc_ctx, hc_tlv->data, len, align, wlc_hc_tx_set);
			break;
#endif // endif
#if defined(WL_RX_HANDLER)
		case WL_HC_XTLV_ID_CAT_DATAPATH_RX: {
			err = bcm_unpack_xtlv_buf(&hc_ctx, hc_tlv->data, len, align, wlc_hc_rx_set);
			break;
		}
#endif // endif
#if defined(WL_SCAN_STALL_CHECK)
		case WL_HC_XTLV_ID_CAT_SCAN:
			err = bcm_unpack_xtlv_buf(&hc_ctx, hc_tlv->data, len, align,
				wlc_hc_scan_set);
			break;
#endif // endif

		default:
			err = BCME_UNSUPPORTED;
			break;
		}
	} else {
		/* GET handling */
		bcm_xtlv_t *val_xtlv = (bcm_xtlv_t*)hc_tlv->data;

		/* on GET, container payload one XTLV with a list of attribute IDs to GET */

		/* make sure ID List is formatted well */
		if (!bcm_valid_xtlv(val_xtlv, p_len, align)) {
			return BCME_BADARG;
		}
		switch (hc_category) {
#if defined(WL_TX_STALL)
		case WL_HC_XTLV_ID_CAT_DATAPATH_TX:
			err = wlc_hc_tx_get(wlc, wlcif, val_xtlv, out, o_len);
			break;
#endif // endif
#if defined(WL_RX_HANDLER)
		case WL_HC_XTLV_ID_CAT_DATAPATH_RX:
			err = wlc_hc_rx_get(wlc, wlcif, val_xtlv, out, o_len);
			break;
#endif // endif
#if defined(WL_SCAN_STALL_CHECK)
		case WL_HC_XTLV_ID_CAT_SCAN:
			err = wlc_hc_scan_get(wlc, wlcif, val_xtlv, out, o_len);
			break;
#endif // endif
		default:
			err = BCME_UNSUPPORTED;
			break;
		}
	}
	return err;
}

/** Copy data from XTLV ID list to an ID buffer */
int
wlc_hc_unpack_idlist(bcm_xtlv_t *id_list, uint16 *ids, uint *count)
{
	uint src_count;
	uint16 *src_ids;
	uint i;

	src_count = ltoh16(id_list->len) / sizeof(ids[0]);
	src_ids = (uint16*)id_list->data;

	/* make sure the id list fits */
	if (src_count > *count) {
		return BCME_BUFTOOLONG;
	}
	*count = src_count;

	/* copy the list of request XTLV IDs to our local req_id_list */
	for (i = 0; i < src_count; i++) {
		ids[i] = ltoh16(src_ids[i]);
	}
	return BCME_OK;
}
#endif /* WL_DD_HANDLER */

#ifdef WL_RX_HANDLER

static int
wlc_hc_rx_set(void *ctx, const uint8 *buf, uint16 type, uint16 len)
{
	struct wlc_hc_ctx *hc_ctx = ctx;
	wlc_info_t *wlc = hc_ctx->wlc;
	uint32 val;
	int err = BCME_OK;
	uint16 expect_len;

	/* values are uint32, check the iobuffer length */
	expect_len = sizeof(uint32);

	if (len < expect_len) {
		return BCME_BUFTOOSHORT;
	} else if (len > expect_len) {
		return BCME_BUFTOOLONG;
	}

	val = ((const uint32*)buf)[0];
	val = ltoh32(val);

	switch (type) {
#if defined(WL_RX_STALL)
	case WL_HC_RX_XTLV_ID_VAL_STALL_THRESHOLD:
		if (val > 100) {
			err = BCME_RANGE;
			break;
		}
		wlc->rx_hc->rx_hc_alert_th = val;
		break;
	case WL_HC_RX_XTLV_ID_VAL_STALL_SAMPLE_SIZE:
		if (val < 1) {
			err = BCME_RANGE;
			break;
		}
		wlc->rx_hc->rx_hc_cnt = val;
		break;
	case WL_HC_RX_XTLV_ID_VAL_STALL_FORCE:
		wlc_rx_healthcheck_force_fail(wlc);
		break;
#endif /* WL_RX_STALL */
#if defined(WL_RX_DMA_STALL_CHECK)
	case WL_HC_RX_XTLV_ID_VAL_DMA_STALL_TIMEOUT:
		err = wlc_bmac_rx_dma_stall_timeout_set(wlc->hw, val);
		break;
	case WL_HC_RX_XTLV_ID_VAL_DMA_STALL_FORCE:
		err = wlc_bmac_rx_dma_stall_force(wlc->hw, val);
		break;
#endif /* WL_RX_DMA_STALL_CHECK */

	default:
		err = BCME_BADOPTION;
		break;
	}

	return err;
}

static int
wlc_hc_rx_get(wlc_info_t *wlc, wlc_if_t *wlcif, bcm_xtlv_t *params, void *out, uint o_len)
{
	bcm_xtlv_t *hc_rx;
	bcm_xtlvbuf_t tbuf;
	uint32 val;
	int err = BCME_OK;
	/* local list for params copy, or for request of all attributes */
	uint16 req_id_list[] = {
		WL_HC_RX_XTLV_ID_VAL_STALL_THRESHOLD,
		WL_HC_RX_XTLV_ID_VAL_STALL_SAMPLE_SIZE,
		WL_HC_RX_XTLV_ID_VAL_DMA_STALL_TIMEOUT,
	};
	uint req_id_count, i;
	uint16 val_id;

	BCM_REFERENCE(wlc);
	BCM_REFERENCE(wlcif);

	/* The input params are expected to be in the same memory as the
	 * output buffer, so save the parameter list.
	 */

	/* size (in elements) of the req buffer */
	req_id_count = ARRAYSIZE(req_id_list);

	err = wlc_hc_unpack_idlist(params, req_id_list, &req_id_count);
	if (err) {
		return err;
	}

	/* start formatting the output buffer */

	/* HC container XTLV comes first */
	if (o_len < (uint)BCM_XTLV_HDR_SIZE) {
		return BCME_BUFTOOSHORT;
	}

	hc_rx = out;
	hc_rx->id = htol16(WL_HC_XTLV_ID_CAT_DATAPATH_RX);

	/* adjust len for the hc_rx header */
	o_len -= BCM_XTLV_HDR_SIZE;

	/* bcm_xtlv_buf_init() takes length up to uint16 */
	o_len = MIN(o_len, 0xFFFF);

	err = bcm_xtlv_buf_init(&tbuf, hc_rx->data, (uint16)o_len, BCM_XTLV_OPTION_ALIGN32);
	if (err) {
		return err;
	}

	/* walk the requests and write the value to the 'out' buffer */
	for (i = 0; !err && i < req_id_count; i++) {
		val_id = req_id_list[i];

		switch (val_id) {
#if defined(WL_RX_STALL)
		case WL_HC_RX_XTLV_ID_VAL_STALL_THRESHOLD:
			val = wlc->rx_hc->rx_hc_alert_th;
			break;
		case WL_HC_RX_XTLV_ID_VAL_STALL_SAMPLE_SIZE:
			val = wlc->rx_hc->rx_hc_cnt;
			break;
#endif /* WL_RX_STALL */
#if defined(WL_RX_DMA_STALL_CHECK)
		case WL_HC_RX_XTLV_ID_VAL_DMA_STALL_TIMEOUT:
			val = wlc_bmac_rx_dma_stall_timeout_get(wlc->hw);
			break;
#endif /* WL_RX_DMA_STALL_CHECK */

		default: /* unknown attribute ID */
			return BCME_BADOPTION;
		}

		/* pack an XTLV with the single value */
		err = bcm_xtlv_put32(&tbuf, val_id, &val, 1);
	}
	/* now we can write the container payload len */
	hc_rx->len = htol16(bcm_xtlv_buf_len(&tbuf));
	return err;
}
#endif /* WL_RX_HANDLER */

uint8
wlc_get_cmn_bwcap(wlc_info_t *wlc, int bandtype)
{
	uint8 bwcap = 0;
	int bandindex;

	if (bandtype == WLC_BAND_5G) {
		bandindex = BAND_5G_INDEX;
	} else if (bandtype == WLC_BAND_2G) {
		bandindex = BAND_2G_INDEX;
	} else {
		WL_ERROR(("wl%d: %s: In valid bandtype %d\n", wlc->pub->unit, __FUNCTION__,
			bandtype));
		ASSERT(0);
		goto done;
	}

#ifdef RSDB_CMN_BANDSTATE
	/* If bandstate is shared across WLCs then we return common BW cap */
	if (RSDB_CMN_BANDSTATE_ENAB(wlc->pub)) {
		bwcap = wlc_rsdb_get_cmn_bwcap(wlc, bandtype);
	} else
#endif /* RSDB_CMN_BANDSTATE */
	{
		bwcap = wlc->bandstate[bandindex]->bw_cap;
	}

done:
	return bwcap;
}

#if defined(EVENT_LOG_COMPILE) && defined(WLCNT)
static void wlc_ctl_mgt_frame_counter_report(void *arg, wlc_chansw_notif_data_t *data)
{
	wlc_info_t *wlc_iter;
	wl_cnt_wlc_t *cnt;
	uint32 idx;
	uint32 *op1, *op2;
	wlc_info_t *wlc = (wlc_info_t *)arg;
	wl_ctl_mgt_cnt_t *frm_cnt, *local_frm_cnt;
	wlc_cmn_info_t *wlc_cmn = wlc->cmn;

	if (!EVENT_LOG_IS_ON(EVENT_LOG_TAG_CTL_MGT_CNT)) {
		return;
	}

	/* Just malloc. Don't need to zero out as the
	 * data will be copied to individual elements of the array
	 * anyways.
	 */
	frm_cnt = (wl_ctl_mgt_cnt_t *) MALLOC(wlc->osh,
		MAX_RSDB_MAC_NUM * sizeof(wl_ctl_mgt_cnt_t));

	if (frm_cnt == NULL) {
		return;
	}

	/* We are going to send only one report */

	/* Cycle over both WLCs */
	/* If we try to do addition in this loop, code size increases by ~500 bytes.
	 * So copy data first and then add.
	 */
	FOREACH_WLC(wlc_cmn, idx, wlc_iter) {
		cnt = wlc_iter->pub->_cnt;
		local_frm_cnt = &frm_cnt[idx];

		/* Null data frame counter reports */
		local_frm_cnt->txnull = cnt->txnull;
		local_frm_cnt->rxnull = cnt->rxnull;
		local_frm_cnt->txqosnull = cnt->txqosnull;
		local_frm_cnt->rxqosnull = cnt->rxqosnull;

		/* Management frame counter reports */
		local_frm_cnt->txassocreq = cnt->txassocreq;
		local_frm_cnt->rxassocreq = cnt->rxassocreq;
		local_frm_cnt->txassocrsp = cnt->txassocrsp;
		local_frm_cnt->rxassocrsp = cnt->rxassocrsp;
		local_frm_cnt->txreassocreq = cnt->txreassocreq;
		local_frm_cnt->rxreassocreq = cnt->rxreassocreq;
		local_frm_cnt->txreassocrsp = cnt->txreassocrsp;
		local_frm_cnt->rxreassocrsp = cnt->rxreassocrsp;
		local_frm_cnt->txdisassoc = cnt->txdisassoc;
		local_frm_cnt->rxdisassoc = cnt->rxdisassoc;
		local_frm_cnt->txauth = cnt->txauth;
		local_frm_cnt->rxauth = cnt->rxauth;
		local_frm_cnt->txdeauth = cnt->txdeauth;
		local_frm_cnt->rxdeauth = cnt->rxdeauth;
		local_frm_cnt->txprobereq = cnt->txprobereq;
		local_frm_cnt->rxprobereq = cnt->rxprobereq;
		local_frm_cnt->txprobersp = cnt->txprobersp;
		local_frm_cnt->rxprobersp = cnt->rxprobersp;
		local_frm_cnt->txaction = cnt->txaction;
		local_frm_cnt->rxaction = cnt->rxaction;

		/* Control frame counter reports */
		local_frm_cnt->txrts = MCSTVAR(wlc_iter->pub, txrtsfrm);
		local_frm_cnt->rxrts = MCSTVAR(wlc_iter->pub, rxrtsucast);
		local_frm_cnt->txcts = MCSTVAR(wlc_iter->pub, txctsfrm);
		local_frm_cnt->rxcts = MCSTVAR(wlc_iter->pub, rxctsucast);
		local_frm_cnt->txack = MCSTVAR(wlc_iter->pub, txackfrm);
		local_frm_cnt->rxack = MCSTVAR(wlc_iter->pub, rxackucast);
#ifdef WLAMPDU
		local_frm_cnt->txbar = cnt->txbar;
		local_frm_cnt->rxbar = cnt->rxbar;
		local_frm_cnt->txback = cnt->txback;
		local_frm_cnt->rxback = cnt->rxback;
#else
		local_frm_cnt->txbar = 0;
		local_frm_cnt->rxbar = 0;
		local_frm_cnt->txback = 0;
		local_frm_cnt->rxback = 0;
#endif /* WLAMPDU */
		local_frm_cnt->txpspoll = cnt->txpspoll;
		local_frm_cnt->rxpspoll = cnt->rxpspoll;
	}

	if (RSDB_ENAB(wlc->pub)) {
		/* cycle over both slices */
		op1 = (uint32 *) &frm_cnt[MAC_CORE_UNIT_0];
		op2 = (uint32 *) &frm_cnt[MAC_CORE_UNIT_1];
		for (idx = 0; idx < (sizeof(wl_ctl_mgt_cnt_t) / sizeof(uint32)); idx++) {
			*op1 += *op2;
			op1++;
			op2++;
		}
	}

	frm_cnt[MAC_CORE_UNIT_0].type = WL_CNT_CTL_MGT_FRAMES;
	frm_cnt[MAC_CORE_UNIT_0].len = sizeof(wl_ctl_mgt_cnt_t) -
		sizeof(local_frm_cnt->type) - sizeof(local_frm_cnt->len);

	EVENT_LOG_BUFFER(EVENT_LOG_TAG_CTL_MGT_CNT,
		(void *)(&frm_cnt[MAC_CORE_UNIT_0]), sizeof(wl_ctl_mgt_cnt_t));

	MFREE(wlc->osh, frm_cnt, MAX_RSDB_MAC_NUM * sizeof(wl_ctl_mgt_cnt_t));
	return;
}
#endif /* EVENT_LOG_COMPILE & WLCNT */

wlc_info_t*
wlc_find_wlc_for_chanspec(wlc_info_t *wlc, chanspec_t chanspec)
{
	wlc_info_t * iwlc = wlc;
#ifdef WLRSDB
	if (RSDB_ENAB(wlc->pub)) {
		iwlc = wlc_rsdb_find_wlc_for_chanspec(wlc, NULL, chanspec, NULL, 0);
	}
#endif // endif
	return iwlc;
}
#ifdef RX_DEBUG_ASSERTS
/* Debug code to print 100 previous RXS values on RX corruptions */
void
wlc_print_prev_rxs(wlc_info_t *wlc)
{
	uint16 cur_idx = wlc->rxs_bkp_idx;
	uint16 j, i = 0;
	uint16 iter = 0;

	printf("Previous %d RXstatus \n", MAX_RXS_BKP_ENTRIES);
	/* Dump previous 100 RX status */
	for (i = 0, j = cur_idx; i < MAX_RXS_BKP_ENTRIES; i++) {
		uint8* data = (uint8*)((uint8*)wlc->rxs_bkp + (j * PER_RXS_SIZE));
		printf("idx1 %d  : ", i);
		for (iter = 0; iter < PER_RXS_SIZE; iter++) {
			printf("%02x ", data[iter]);
		}
		printf("\n");

		/* Jump to next RXS entry */
		j = (j + 1) % MAX_RXS_BKP_ENTRIES;
	}
}
#endif /* RX_DEBUG_ASSERTS */

#ifdef MULTIAP
/* Process received  multiap ie. */
void
wlc_process_multiap_ie(wlc_info_t *wlc, struct scb *scb, multiap_ie_t *multiap_ie)
{
	uint len = MAP_IE_MAX_LEN - TLV_HDR_LEN;
	multiap_ext_attr_t *map_attr = NULL;

	ASSERT(scb != NULL);

	if (!multiap_ie || multiap_ie->len > len) {
		return;
	}

	map_attr = (multiap_ext_attr_t *)multiap_ie->attr;

	if (map_attr && (map_attr->attr_val != 0)) {
		scb->flags3 |= SCB3_MAP_CAP;
	} else {
		scb->flags3 &= ~SCB3_MAP_CAP;
	}
}

/* Update multiap ie returns true on sucess */
bool
wlc_update_multiap_ie(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	multiap_ext_attr_t *map_attr;
	multiap_ie_t *map_ie = (multiap_ie_t *)&bsscfg->multiap_ie[0];

	if (!map_ie) {
		return FALSE;
	}

	map_attr = (multiap_ext_attr_t *)map_ie->attr;
	map_attr->attr = MAP_EXT_ATTR;
	map_attr->attr_len = MAP_EXT_ATTR_LEN;
	map_attr->attr_val = bsscfg->map_attr;
	map_ie->len = MAP_IE_MAX_LEN - TLV_HDR_LEN;

	return TRUE;
}
#endif	/* MULTIAP */

/* Update current bss ht and vht flags. */
static void
wlc_update_current_bi_caps(wlc_info_t *wlc, wl_bss_info_t *bi)
{
	uint32 flags = 0;

#if defined(WL11N)
	if (bi->n_cap) {
		flags = wlc_ht_get_cap(wlc->hti);
		if (flags & HT_CAP_40MHZ) {
			bi->nbss_cap |= HT_CAP_40MHZ;
		}
		if (flags & HT_CAP_SHORT_GI_20) {
			bi->nbss_cap |= HT_CAP_SHORT_GI_20;
		}
		if (flags & HT_CAP_SHORT_GI_40) {
			bi->nbss_cap |= HT_CAP_SHORT_GI_40;
		}
	}
#endif	/* WL11N */

#if defined(WL11AC)
	if (bi->vht_cap) {
		flags = wlc_vht_get_cap_info(wlc->vhti);
		if (flags & VHT_CAP_CHAN_WIDTH_SUPPORT_160) {
			bi->nbss_cap |= VHT_BI_160MHZ;
		}
		if (flags & VHT_CAP_CHAN_WIDTH_SUPPORT_160_8080) {
			bi->nbss_cap |= VHT_BI_8080MHZ;
		}
		if (flags & VHT_CAP_INFO_SGI_160MHZ) {
			bi->nbss_cap |= VHT_BI_SGI_160MHZ;
		}
		if (flags & VHT_CAP_INFO_SGI_80MHZ) {
			bi->nbss_cap |= VHT_BI_SGI_80MHZ;
		}
		if (flags & VHT_CAP_INFO_SU_BEAMFMR) {
			bi->nbss_cap |= VHT_BI_CAP_SU_BEAMFMR;
		}
		if (flags & VHT_CAP_INFO_MU_BEAMFMR) {
			bi->nbss_cap |= VHT_BI_CAP_MU_BEAMFMR;
		}
	}
#endif	/* WL11AC */
}

void
wlc_set_psm_watchdog_debug(wlc_info_t *wlc, bool debug)
{
	wlc->psm_watchdog_debug = debug;
}
