/*
 * Association/Roam related routines
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
 * $Id: wlc_assoc.c 778094 2019-08-22 09:42:45Z $
 */

/**
 * XXX Related Twiki's:
 * [Dept3339SwBobsCorner#wlc_assoc_c] Sample function call sequences
 * [BSSTransition11vOlympic] Olympic BSS Transition feature: AP load balancing related
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
#include <sbchipc.h>
#include <bcmendian.h>
#include <802.11.h>
#include <802.11e.h>
#include <wpa.h>
#include <bcmwpa.h>
#include <bcmdevs.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_cca.h>
#include <wlc_keymgmt.h>
#include <wlc_bsscfg.h>
#include <wlc_channel.h>
#include <wlc.h>
#include <wlc_dbg.h>
#include <wlc_hw.h>
#include <wlc_bmac.h>
#include <wlc_scb.h>
#include <wlc_led.h>
#include <phy_chanmgr_api.h>
#include <phy_misc_api.h>
#ifdef WLTDLS
#include <wlc_tdls.h>
#endif // endif
#include <wlc_scb_ratesel.h>
#include <wl_export.h>
#if defined(BCMSUP_PSK)
#include <wlc_wpa.h>
#include <wlc_sup.h>
#endif // endif
#include <wlc_pmkid.h>
#include <wlc_rm.h>
#include <wlc_cac.h>
#include <wlc_ap.h>
#include <wlc_apps.h>
#include <wlc_scan.h>
#include <wlc_assoc.h>
#ifdef WLMCNX
#include <wlc_mcnx.h>
#endif // endif
#include <wlc_p2p.h>
#ifdef WLMCHAN
#include <wlc_mchan.h>
#endif // endif
#include <wlc_wnm.h>

#include <awd.h>
#include <wlioctl.h>
#include <wlc_11h.h>
#include <wlc_csa.h>
#include <wlc_quiet.h>
#include <wlc_11d.h>
#include <wlc_cntry.h>
#include <wlc_prot_g.h>
#include <wlc_prot_n.h>
#include <wlc_utils.h>
#include <wlc_pcb.h>
#include <wlc_he.h>
#include <wlc_vht.h>
#include <wlc_ht.h>
#include <wlc_txbf.h>
#include <wlc_macfltr.h>

#ifdef WLPFN
#include <wl_pfn.h>
#endif // endif
#define TK_CM_BLOCKED(_wlc, _bsscfg) (\
	wlc_keymgmt_tkip_cm_enabled((_wlc)->keymgmt, (_bsscfg)))

#ifdef WL11K
#include <wlc_rrm.h>
#endif /* WL11K */
#include <wlc_btcx.h>
#ifdef WLAMPDU
#include <wlc_ampdu_cmn.h>
#endif /* WLAMPDU */
#include <wlc_ie_mgmt.h>
#include <wlc_ie_mgmt_ft.h>
#include <wlc_ie_mgmt_vs.h>
#include <wlc_ie_mgmt_lib.h>
#include <wlc_ie_helper.h>
#ifdef WLFBT
#include <wlc_fbt.h>
#endif /* WLFBT */
#if defined(WL_ASSOC_MGR)
#include <wlc_assoc_mgr.h>
#endif /* defined(WL_ASSOC_MGR) */
#ifdef WLRCC
#include <wlc_okc.h>
#endif /* WLRCC */

#include <wlc_ht.h>
#include <wlc_obss.h>
#include <wlc_tx.h>

#ifdef WL_PWRSTATS
#include <wlc_pwrstats.h>
#endif // endif

#ifdef WLRSDB
#include <wlc_rsdb.h>
#endif /* WLRSDB */

#if defined(WLBSSLOAD_REPORT)
#include <wlc_bssload.h>
#endif // endif

#ifdef WL_MODESW
#include <wlc_modesw.h>
#endif // endif

#include <wlc_msch.h>
#include <wlc_chanctxt.h>
#include <wlc_sta.h>

#include <wlc_hs20.h>

#include <wlc_rx.h>

#ifdef WLTPC
#include <wlc_tpc.h>
#endif // endif
#include <wlc_lq.h>

#ifdef WLC_SW_DIVERSITY
#include <wlc_swdiv.h>
#endif /* WLC_SW_DIVERSITY */

#define SCAN_BLOCK_AT_ASSOC_COMPL	3	/* scan block count at the assoc complete */

#if defined(WL_PROXDETECT) && defined(WL_FTM)
#include <wlc_ftm.h>
#endif /* WL_PROXDETECT && WL_FTM */

#include <event_trace.h>
#include <wlc_qoscfg.h>
#include <wlc_event_utils.h>
#include <wlc_rspec.h>
#include <wlc_pm.h>
#include <wlc_scan_utils.h>
#include <wlc_stf.h>
#include <wlc_dump.h>
#include <wlc_iocv.h>
#ifdef WL_OCE
#include <wlc_oce.h>
#endif /* WL_OCE */
#ifdef WL_FILS
#include <wlc_fils.h>
#endif /* WL_FILS */
#ifdef BCMULP
#include <ulp.h>
#endif // endif
#ifdef WL11AX
#include <802.11ax.h>
#endif // endif
#ifdef BCMULP
#include <wlc_ulp.h>
#endif /* BCMULP */
#include <phy_noise_api.h>
#include <phy_calmgr_api.h>
#include <phy_radio_api.h>
#include <phy_radar_api.h>
#include <wlc_dfs.h>
#if defined(WL_MBO) && !defined(WL_MBO_DISABLED) && defined(MBO_AP)
#include <wlc_mbo.h>
#endif /* WL_MBO && !WL_MBO_DISABLED ** MBO_AP */
#include <wlc_mfp.h>

/* shared wlc module info */
typedef struct wlc_assoc_cmn {
	/* XXX mSTA: Serialize join/roam requests for now until simultaneous
	 * associations are supported.
	 * RSDB: There should be single Assoc request Q between wlc's.
	 * Due to the fact that JOIN has higher priority than ROAM this list
	 * will not have mixed JOIN requests and ROAM requests:
	 * - it is empty when there is no JOIN or ROAM requests being processed
	 * - it has only one ROAM request that is being processed, all new ROAM
	 *   requests are blocked
	 * - all requests in the list are JOIN requests, new JOIN request will be
	 *   appended to the list, the top most one is currently processed
	 * - when a new JOIN request comes in while the ROAM request is being
	 *   processed, ROAM will be aborted and deleted from the list, JOIN
	 *   request will be added in the list and processed right away
	 * - ASSOCIATION RECREATE is treated same as JOIN
	 *
	 * The storage will be allocated to hold all requests up to WLC_MAXBSSCFG
	 */
	wlc_bsscfg_t	*assoc_req[WLC_MAXBSSCFG]; /**< join/roam requests */

	/* association */
	uint		sta_retry_time;		/**< time to retry on initial assoc failure */
	wlc_bss_list_t	*join_targets;
	uint		join_targets_last;	/**< index of last target tried (next: --last) */
} wlc_assoc_cmn_t;

/* per wlc module info */
struct wlc_assoc_info {
	wlc_info_t *wlc;
	wlc_assoc_cmn_t *cmn;
	bcm_notif_h as_st_notif_hdl;	/**< assoc state notifier handle. */
	bcm_notif_h dis_st_notif_hdl;	/**< disassoc state notifier handle. */
	int cfgh;			/* bsscfg cubby offset */
	bcm_notif_h time_since_bcn_notif_hdl; /** time_since_bcn hnotifer */
};

#define JOIN_DATA_INFO_LEN 128
typedef struct {
	uint8 buf[JOIN_DATA_INFO_LEN];
	uint8 num_of_bssids;
	uint32 join_start_time;
	bool buf_inuse;
	bool stats_exist;
	bool attempt_done;
} wlc_join_attempt_info_t;

typedef struct assoc_cfg_cubby {
	uint32	timestamp_lastbcn; /* in miliseconds */
	uint32	mschreg_errcnt;
	uint32	mschunreg_errcnt;
	wlc_msch_req_handle_t *msch_homech_req_hdl;
	uint stflags;	/* state flags, see AS_ST_FLAG_XXXX */
	wlc_join_attempt_info_t join_attempt_info;
} assoc_cfg_cubby_t;

/* assoc cubby access macro */
#define BSSCFG_ASSOC_CUBBY(assoc_info, cfg) \
	(assoc_cfg_cubby_t *)BSSCFG_CUBBY((cfg), (assoc_info)->cfgh)

/* stflags */
#define AS_ST_FLAG_ABORT_PROC	(1<<0)	/* wlc_assoc_abort in progress */
#define AS_ST_FLAG_DONE_PROC	(1<<2)	/* wlc_join_done in progress */

/* debug */
#define WL_RECURSIVE(msg) WL_PRINT(msg)

#ifdef STA
/* join targets sorting preference */
#define MAXJOINPREFS		5	/**< max # user-supplied join prefs */
#define MAXWPACFGS		16	/**< max # wpa configs */
struct wlc_join_pref {
	struct {
		uint8 type;		/**< type */
		uint8 start;		/**< offset */
		uint8 bits;		/**< width */
		uint8 reserved;
	} field[MAXJOINPREFS];		/**< preference field, least significant first */
	uint fields;			/**< # preference fields */
	uint prfbmp;			/**< preference types bitmap */
	struct {
		uint8 akm[WPA_SUITE_LEN];	/**< akm oui */
		uint8 ucipher[WPA_SUITE_LEN];	/**< unicast cipher oui */
		uint8 mcipher[WPA_SUITE_LEN];	/**< multicast cipher oui */
	} wpa[MAXWPACFGS];		/**< wpa configs, least favorable first */
	uint wpas;			/**< # wpa configs */
	uint8 band;			/**< 802.11 band pref */
};

/* join pref width in bits */
#define WLC_JOIN_PREF_BITS_TRANS_PREF	8 /**< # of bits in weight for AP Transition Join Pref */
#define WLC_JOIN_PREF_BITS_RSSI		16 /**< # of bits in weight for RSSI Join Pref */
#define WLC_JOIN_PREF_BITS_WPA		4 /**< # of bits in weight for WPA Join Pref */
#define WLC_JOIN_PREF_BITS_BAND		1 /**< # of bits in weight for BAND Join Pref */
#define WLC_JOIN_PREF_BITS_RSSI_DELTA	0 /**< # of bits in weight for RSSI Delta Join Pref */

/* Fixed join pref start bits */
#define WLC_JOIN_PREF_START_TRANS_PREF	(32 - WLC_JOIN_PREF_BITS_TRANS_PREF)

/* join pref formats */
#define WLC_JOIN_PREF_OFF_COUNT		1 /**< Tuple cnt field offset in WPA Join Pref TLV value */
#define WLC_JOIN_PREF_OFF_BAND		1 /**< Band field offset in BAND Join Pref TLV value */
#define WLC_JOIN_PREF_OFF_DELTA_RSSI	0 /**< RSSI delta value offset */

/* handy macros */
#define WLCAUTOWPA(cfg)		((cfg)->join_pref != NULL && (cfg)->join_pref->wpas > 0)
#define WLCTYPEBMP(type)	(1 << (type))
#define WLCMAXCNT(type)		(1 << (type))
#define WLCMAXVAL(bits)		((1 << (bits)) - 1)
#define WLCBITMASK(bits)	((1 << (bits)) - 1)

#if MAXWPACFGS != WLCMAXCNT(WLC_JOIN_PREF_BITS_WPA)
#error "MAXWPACFGS != (1 << WLC_JOIN_PREF_BITS_WPA)"
#endif // endif

/* roaming trigger step value */
#define WLC_ROAM_TRIGGER_STEP		10	/**< roaming trigger step in dB */
#define WLC_CU_TRIGGER_STEP		15	/* cu trigger step in percentage */

#define WLC_BSSID_SCAN_NPROBES		4
#define WLC_BSSID_SCAN_ACTIVE_TIME	120

#define WLC_SCAN_DFS_CHSW_TIME		120	/* scan time on new home channel to reassoc */
#define WLC_SCAN_DFS_CHSW_NPROBES	4	/* max number of Probe Requests sent to reassoc */

#define WLC_SCAN_RECREATE_TIME		50	/**< default assoc recreate scan time */
#define WLC_NLO_SCAN_RECREATE_TIME	30	/**< nlo enabled assoc recreate scan time */

#ifdef BCMQT_CPU
#define WECA_ASSOC_TIMEOUT	1500	/**< qt is slow */
#define WECA_AUTH_TIMEOUT	1500	/**< qt is slow */
#else
#define WECA_ASSOC_TIMEOUT	300	/**< authentication or association timeout in ms */
#define WECA_AUTH_TIMEOUT	300	/**< authentication timeout in ms */
#endif // endif

#ifdef BCMDBG
#define WLC_EXT_AUTH_TIMEOUT	450	/* External supplicant takes time in debug mode */
#else
#define WLC_EXT_AUTH_TIMEOUT	350	/* external auth timeout */
#endif // endif

#define WLC_IE_WAIT_TIMEOUT	200	/**< assoc. ie waiting timeout in ms */
#define WLC_ASSOC_RETRY_TIMEOUT 1000	/**<time to wait after temporary assoc deny */

#ifdef WLABT
#define ABT_MIN_TIMEOUT		5
#define ABT_HIGH_RSSI		-65
#endif // endif

#define DELAY_10MS		10	/* delay time in ms */

#define WLC_ASSOC_TIME_US                   (5000 * 1000)
#define WLC_MSCH_CHSPEC_CHNG_START_TIME_US  (2000)

#define WLC_ASSOC_IS_PROHIBITED_CHANSPEC(wlc, chanspec) \
	((WLC_CNTRY_DEFAULT_ENAB(wlc) && !WLC_AUTOCOUNTRY_ENAB(wlc) && \
	(chanspec != INVCHANSPEC) && \
	!wlc_valid_chanspec_cntry(wlc->cmi, wlc_channel_country_abbrev(wlc->cmi), chanspec)))

/* local routine declarations */
#if defined(WL_ASSOC_MGR)
static int
wlc_assoc_continue_sent_auth1(wlc_bsscfg_t *cfg, struct ether_addr* addr);

typedef int (*wlc_assoc_continue_handler)(wlc_bsscfg_t *cfg, struct ether_addr* addr);

typedef struct {
	uint state;
	wlc_assoc_continue_handler handler;
} wlc_assoc_continue_tbl_t;

static wlc_assoc_continue_tbl_t wlc_assoc_continue_table[] =
	{
		{AS_SENT_AUTH_1, wlc_assoc_continue_sent_auth1},
		/* Last entry */
		{AS_VALUE_NOT_SET, NULL}
	};
#endif /* defined(WL_ASSOC_MGR) */

static void
wlc_assoc_continue_post_auth1(wlc_bsscfg_t *cfg, struct scb *scb);
#if defined(WL_CLIENT_SAE)
static int
wlc_start_assoc(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, const struct ether_addr *ea);
#endif /* WL_CLIENT_SAE */
static int wlc_setssid_disassociate_client(wlc_bsscfg_t *cfg);
static void wlc_setssid_disassoc_complete(wlc_info_t *wlc, uint txstatus, void *arg);
static int wlc_assoc_bsstype(wlc_bsscfg_t *cfg);
static int wlc_assoc_scan_prep(wlc_bsscfg_t *cfg, wl_join_scan_params_t *scan_params,
	const struct ether_addr *bssid, const chanspec_t* chanspec_list, int channel_num);
static int wlc_assoc_scan_start(wlc_bsscfg_t *cfg, wl_join_scan_params_t *scan_params,
	const struct ether_addr *bssid, const chanspec_t* chanspec_list, int channel_num);
#ifdef WLSCANCACHE
static void wlc_assoc_cache_eval(wlc_info_t *wlc,
	const struct ether_addr *BSSID, const wlc_ssid_t *SSID,
	int bss_type, const chanspec_t *chanspec_list, uint chanspec_num,
	wlc_bss_list_t *bss_list, chanspec_t **target_list, uint *target_num);
static void wlc_assoc_cache_fail(wlc_bsscfg_t *cfg);
#else
#define wlc_assoc_cache_eval(wlc, BSSID, SSID, bt, cl, cn, bl, tl, tn)	((void)tl, (void)tn)
#define wlc_assoc_cache_fail(cfg)	((void)cfg)
#endif // endif
static void wlc_assoc_scan_complete(void *arg, int status, wlc_bsscfg_t *cfg);
static void wlc_assoc_success(wlc_bsscfg_t *cfg, struct scb *scb);
#ifdef WL_ASSOC_RECREATE
static void wlc_speedy_recreate_fail(wlc_bsscfg_t *cfg);
#endif /* WL_ASSOC_RECREATE */
static void wlc_auth_complete(wlc_bsscfg_t *cfg, uint status, struct ether_addr *addr,
	uint auth_status, uint auth_type);
static void wlc_assoc_complete(wlc_bsscfg_t *cfg, uint status, struct ether_addr *addr,
	uint assoc_status, bool reassoc, uint bss_type);
static void wlc_auth_tx_complete(wlc_info_t *wlc, uint txstatus, void *arg);
static void wlc_assoc_recreate_timeout(wlc_bsscfg_t *cfg);

static void
wlc_assocreq_complete(wlc_info_t *wlc, uint txstatus, void *arg);

static void wlc_roamscan_complete(wlc_bsscfg_t *cfg);
static void wlc_roam_set_env(wlc_bsscfg_t *cfg, uint entries);
static void wlc_roam_release_flow_cntrl(wlc_bsscfg_t *cfg);

#ifdef AP
static bool wlc_join_check_ap_need_csa(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	chanspec_t chanspec, uint state);
static bool wlc_join_ap_do_csa(wlc_info_t *wlc, chanspec_t tgt_chanspec);
#endif /* AP */

static void wlc_join_BSS_limit_caps(wlc_bsscfg_t *cfg, wlc_bss_info_t *bi);
static void wlc_join_ibss(wlc_bsscfg_t *cfg);
static void wlc_join_BSS_sendauth(wlc_bsscfg_t *cfg, wlc_bss_info_t *bi,
	struct scb *scb, uint8 mcsallow);
static bool wlc_join_chanspec_filter(wlc_bsscfg_t *cfg, chanspec_t chanspec);
static void wlc_cook_join_targets(wlc_bsscfg_t *cfg, bool roam, int cur_rssi);
static void wlc_create_ibss(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
static bool wlc_join_target_verify_rates(wlc_info_t *wlc, wlc_bsscfg_t *cfg, wlc_bss_info_t *bi,
	wlc_rateset_t *rateset);
static bool wlc_join_verify_basic_rates(wlc_info_t *wlc, wlc_bsscfg_t *cfg, wlc_bss_info_t *bi,
	wlc_rateset_t *rateset);
static bool wlc_join_verify_filter_membership(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	wlc_bss_info_t *bi, wlc_rateset_t *rateset);
static void wlc_join_adopt_bss(wlc_bsscfg_t *cfg);
static int wlc_join_start(wlc_bsscfg_t *cfg, wl_join_scan_params_t *scan_params,
	wl_join_assoc_params_t *assoc_params);

static int _wlc_join_start_ibss(wlc_info_t *wlc, wlc_bsscfg_t *cfg, int bss_type);

static int wlc_join_pref_tlv_len(wlc_info_t *wlc, uint8 *pref, int len);
static int wlc_join_pref_parse(wlc_bsscfg_t *cfg, uint8 *pref, int len);
static int wlc_join_pref_build(wlc_bsscfg_t *cfg, uint8 *pref, int len);
static void wlc_join_pref_reset(wlc_bsscfg_t *cfg);
static void wlc_join_pref_band_upd(wlc_bsscfg_t *cfg);

static int wlc_join_assoc_params_parse(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	wl_join_assoc_params_t *assoc_params, const struct ether_addr **bssid,
	const chanspec_t **chanspec_list, int *chanspec_num);

static int wlc_bss_list_expand(wlc_bsscfg_t *cfg, wlc_bss_list_t *from, wlc_bss_list_t *to);

static int wlc_assoc_req_add_entry(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint type);
static int wlc_assoc_req_remove_entry(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
static void wlc_assoc_req_process_next(wlc_info_t *wlc);

static int wlc_set_ssid_complete(wlc_bsscfg_t *cfg, uint status, struct ether_addr *addr,
	uint bss_type);
static int wlc_roam_complete(wlc_bsscfg_t *cfg, uint status,
	struct ether_addr *addr, uint bss_type);

static int wlc_merge_bcn_prb(wlc_info_t *wlc, struct dot11_bcn_prb *p1, int p1_len,
	struct dot11_bcn_prb *p2, int p2_len, struct dot11_bcn_prb **merged, int *merged_len);
static int wlc_merged_ie_len(wlc_info_t *wlc, uint8 *tlvs1, int tlvs1_len, uint8 *tlvs2,
	int tlvs2_len);
static bool wlc_find_ie_match(bcm_tlv_t *ie, bcm_tlv_t *ies, int len);
static void wlc_merge_ies(uint8 *tlvs1, int tlvs1_len, uint8 *tlvs2, int tlvs2_len, uint8* merge);
static bool wlc_is_critical_roam(wlc_bsscfg_t *cfg);
#ifdef ROBUST_DISASSOC_TX
static void wlc_disassoc_timeout(void *arg);
#endif /* ROBUST_DISASSOC_TX */
static void wlc_join_BSS_post_ch_switch(wlc_bsscfg_t *cfg, wlc_bss_info_t *bi,
        bool ch_changed, chanspec_t chanspec);
#ifdef WLABT
static void wlc_check_adaptive_bcn_timeout(wlc_bsscfg_t *cfg);
#endif // endif

static void wlc_roam_period_update(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
static int wlc_assoc_chanspec_change_cb(void* handler_ctxt, wlc_msch_cb_info_t *cb_info);

#if defined(SLAVE_RADAR)
static bool wlc_assoc_do_dfs_reentry_cac_if_required(wlc_bsscfg_t *cfg, wlc_bss_info_t *bi);
#endif /* SLAVE_RADAR */

#if defined(BCMDBG) || defined(WLMSG_ASSOC)
static void wlc_print_roam_status(wlc_bsscfg_t *cfg, uint roam_reason, bool printcache);
#endif // endif

static void wlc_bss_assoc_state_notif(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	uint type, uint state);

static int16 wlc_roamprof_get_min_idx(wlc_info_t *wlc, int bandtype);
static bool wlc_roamprof_is_cu_enabled(wlc_info_t *wlc, int bandtype, int16 idx);
static void wlc_bss_time_since_bcn_notif(wlc_assoc_info_t *as, wlc_bsscfg_t *cfg, uint time);
#endif /* STA */

static void wlc_deauth_sendcomplete(wlc_info_t *wlc, uint txstatus, void *arg);
static void wlc_bss_disassoc_notif(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	uint type, uint state, const struct ether_addr *addr);

#ifdef STA
static void wlc_handle_ap_lost(wlc_info_t *wlc, wlc_bsscfg_t *cfg);

static void wlc_roam_prof_update(wlc_info_t *wlc, wlc_bsscfg_t *cfg, bool reset);
static void wlc_roam_prof_update_default(wlc_info_t *wlc, wlc_bsscfg_t *cfg);

static void wlc_join_done_int(wlc_bsscfg_t *cfg, int status);

static int wlc_assoc_homech_req_clbk(void* handler_ctxt, wlc_msch_cb_info_t *cb_info);
static int wlc_assoc_homech_req_register(wlc_bsscfg_t *bsscfg);
static void wlc_assoc_on_rxbcn(void *ctx, bss_rx_bcn_notif_data_t *data);

/* wlc_set_ssid_complete return code */
#define JOIN_CMPLT_RETRY	1
#define JOIN_CMPLT_DONE		2

/* wlc_roam_complete return code */
#define ROAM_CMPLT_DELAY	1
#define ROAM_CMPLT_DONE		2
#define ROAM_CMPLT_PARTIAL	3

static void wlc_join_attempt_info_init(wlc_bsscfg_t *cfg);
static void wlc_join_attempt_info_start(wlc_bsscfg_t *cfg, wlc_bss_info_t *bi);
static void wlc_join_attempt_info_end(wlc_bsscfg_t *cfg, int status);
static void wlc_join_attempt_info_update_tot_time(wlc_bsscfg_t *cfg);

static int32 wlc_rmc_rpt_version_get(bcm_xtlv_t *out_xtlv, uint out_len);
static int32 wlc_rmc_rpt_config_get(wlc_bsscfg_t *cfg, bcm_xtlv_t *out_xtlv, uint out_len);

static void wlc_assoc_process_retry_later(wlc_bsscfg_t *cfg, struct scb *scb, uint16 fk,
	uint8 *body, uint body_len);

static void wlc_assoc_disable_apcfg_on_repeater(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);

#if defined(BCMDBG) || defined(WLMSG_ASSOC)
static const char *join_pref_name[] = {"rsvd", "rssi", "wpa", "band", "band_rssi_delta"};
#define WLCJOINPREFN(type)	join_pref_name[type]
#endif	/* BCMDBG || WLMSG_ASSOC */

/* XXX mSTA: Phase 1 takes a simple approach - seriallize join/roam requests.
 * revisit here when simultaneous multiple join/roam requests are supported.
 */
#if defined(BCMDBG) || defined(WLMSG_ASSOC) || defined(WLMSG_INFORM) || \
	defined(WLMSG_ROAM)
static const char *as_type_name[] = {
	"NONE", "JOIN", "ROAM", "RECREATE", "VERIFY"
};
#define WLCASTYPEN(type)	as_type_name[type]
#endif /* BCMDBG || WLMSG_ASSOC || WLMSG_INFORM || WLMSG_ROAM */

#if defined(BCMDBG) || defined(WLMSG_ASSOC) || defined(WLMSG_ASSOC_LT)
/* When an AS_ state is added, add a string translation to the table below */
#if AS_LAST_STATE != 40 /* don't change this without adding to the table below!!! */
#error "You need to add an assoc state name string to as_st_names for the new assoc state"
#endif // endif

const char * as_st_names[] = {
	"IDLE",
	"JOIN_INIT",
	"SCAN",
	"WAIT_IE",
	"WAIT_IE_TIMEOUT",
	"JOIN_START",
	"SENT_FT_REQ",
	"RECV_FT_RES",
	"WAIT_TX_DRAIN",
	"SENT_AUTH_1",
	"SENT_AUTH_2",
	"SENT_AUTH_3",
	"SENT_ASSOC",
	"REASSOC_RETRY",
	"JOIN_ADOPT",
	"WAIT_RCV_BCN",
	"SYNC_RCV_BCN",
	"WAIT_DISASSOC",
	"WAIT_PASSHASH",
	"RECREATE_WAIT_RCV_BCN",
	"ASSOC_VERIFY",
	"LOSS_ASSOC",
	"JOIN_CACHE_DELAY",
	"WAIT_FOR_AP_CSA",
	"WAIT_FOR_AP_CSA_ROAM_FAIL",
	"AS_MODE_SWITCH_START",
	"AS_MODE_SWITCH_COMPLETE",
	"AS_MODE_SWITCH_FAILED",
	"AS_DISASSOC_TIMEOUT",
	"AS_IBSS_CREATE",
	"AS_DFS_CAC_START",
	"AS_DFS_CAC_FAIL",
	"AS_DFS_ISM_INIT",
	"AS_RETRY_ESS",
	"AS_NEXT_ESS",
	"AS_RETRY_BSS",
	"AS_NEXT_BSS",
	"AS_SCAN_ABORT",
	"AS_EXT_AUTH_START",
	"AS_SENT_AUTH_UP",
};

static const char *
BCMRAMFN(wlc_as_st_name)(uint state)
{
	const char * result;

	if (state >= ARRAYSIZE(as_st_names))
		result = "UNKNOWN";
	else {
		result = as_st_names[state];
	}

	return result;
}

#endif /* BCMDBG || WLMSG_ASSOC || WLMSG_ASSOC_LT */

static int
wlc_assoc_req_add_entry(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint type)
{
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char ssidbuf[SSID_FMT_BUF_LEN];
#endif // endif
	int i, j;
	wlc_bsscfg_t *bc;
	wlc_assoc_t *as;
	bool as_in_progress_already;

	/* check the current state of assoc_req array */
	/* mark to see whether it's null or not */
	as_in_progress_already = AS_IN_PROGRESS(wlc);

	if (type == AS_ROAM) {
		if (as_in_progress_already)
			return BCME_BUSY;
		goto find_entry;
	}
	/* else if (type == AS_ASSOCIATION || type == AS_RECREATE) { */
	/* XXX At this point the list is either empty, or has only one ROAM request,
	 * or has one or more JOIN request(s). Remove the ROAM request if any. Append
	 * the new JOIN/RECREATE request at the first available entry.
	 */
	/* remove all other low priority requests */
	for (i = 0; i < WLC_MAXBSSCFG; i ++) {
		if ((bc = wlc->as->cmn->assoc_req[i]) == NULL)
			break;
		as = bc->assoc;
		/* Do not pre-empt when in state AS_XX_WAIT_RCV_BCN. In this state
		 * association is already completed, if we abort in this state then all
		 * those tasks which are performed as part of wlc_join_complete will be
		 * skipped but still maintain the association.
		 * This leaves power save and 11h/d in wrong state
		 */
		if (as->type == AS_ASSOCIATION || as->type == AS_RECREATE ||
			(as->type == AS_ROAM && as->state == AS_WAIT_RCV_BCN)) {
			continue;
		}
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
		wlc_format_ssid(ssidbuf, bc->SSID, bc->SSID_len);
#endif // endif
		WL_ASSOC(("wl%d.%d: remove %s request in state %s for SSID '%s "
		          "from assoc_req list slot %d\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), WLCASTYPEN(as->type),
		          wlc_as_st_name(as->state), ssidbuf, i));
		/* other associations such as roam association may be preempted */
		wlc_assoc_abort(bc);
		ASSERT(wlc->as->cmn->assoc_req[i] != bc);
		/* Rewind loop to check this (modified) slot. */
		i--;
	}
	/* } */

find_entry:
	/* find the first empty entry */
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	wlc_format_ssid(ssidbuf, cfg->SSID, cfg->SSID_len);
#endif // endif

	for (i = 0; i < WLC_MAXBSSCFG; i ++) {
		if ((bc = wlc->as->cmn->assoc_req[i]) == NULL)
			goto ins_entry;
		if (bc == cfg) {
			WL_ASSOC(("wl%d.%d: %s request in state %s for SSID '%s' exists "
			          "in assoc_req list at slot %d\n", wlc->pub->unit,
			          WLC_BSSCFG_IDX(cfg), WLCASTYPEN(type),
			          wlc_as_st_name(cfg->assoc->state), ssidbuf, i));
			return i;
		}
	}

	return BCME_NORESOURCE;

ins_entry:
	/* insert the bsscfg in the list at slot i */
	WL_ASSOC(("wl%d.%d: insert %s request in state %s for SSID '%s' "
	          "in assoc_req list at slot %d\n", wlc->pub->unit, WLC_BSSCFG_IDX(cfg),
	          WLCASTYPEN(type), wlc_as_st_name(cfg->assoc->state), ssidbuf, i));

	ASSERT(i < WLC_MAXBSSCFG);

	j = i;
	bc = cfg;
	do {
		wlc_bsscfg_t *temp = wlc->as->cmn->assoc_req[i];
		wlc->as->cmn->assoc_req[i] = bc;
		bc = temp;
		if (bc == NULL || ++i >= WLC_MAXBSSCFG)
			break;
		as = bc->assoc;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
		wlc_format_ssid(ssidbuf, bc->SSID, bc->SSID_len);
#endif // endif
		WL_ASSOC(("wl%d.%d: move %s request in state %s for SSID '%s' "
		          "in assoc_req list to slot %d\n", wlc->pub->unit,
		          WLC_BSSCFG_IDX(cfg), WLCASTYPEN(as->type),
		          wlc_as_st_name(as->state), ssidbuf, i));
	}
	while (TRUE);
	ASSERT(i < WLC_MAXBSSCFG);

	/* as_in_progress now changed, update ps_ctrl */
	if (as_in_progress_already != AS_IN_PROGRESS(wlc)) {
		wlc_set_wake_ctrl(wlc);
	}

	return j;
} /* wlc_assoc_req_add_entry */

static void
wlc_assoc_req_process_next(wlc_info_t *wlc)
{
	wlc_bsscfg_t *cfg;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char ssidbuf[SSID_FMT_BUF_LEN];
#endif // endif
	wlc_assoc_t *as;

	if ((cfg = wlc->as->cmn->assoc_req[0]) == NULL) {
		WL_ASSOC(("wl%d: all assoc requests in assoc_req list have been processed\n",
		          wlc->pub->unit));
		wlc_set_wake_ctrl(wlc);
		return;
	}

#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	wlc_format_ssid(ssidbuf, cfg->SSID, cfg->SSID_len);
#endif // endif

	as = cfg->assoc;
	ASSERT(as != NULL);
	/* If the current bsscfg is in state AS_IDLE
	*	or if it is AS_WAIT_DISASSOC
	*	we should not trigger a start from state JOIN_INIT
	*/
	if (as->state != AS_IDLE || as->state != AS_WAIT_DISASSOC) {
		return;
	}
	WL_ASSOC(("wl%d.%d: type %d request in assoc_req list for SSID '%s'\n",
	          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), as->type, ssidbuf));

	switch (as->type) {
	case AS_ASSOCIATION:
		wlc_join_start(cfg, wlc_bsscfg_scan_params(cfg),
				wlc_bsscfg_assoc_params(cfg));
		break;

	case AS_RECREATE:
		if (ASSOC_RECREATE_ENAB(wlc->pub)) {
			wlc_join_recreate(wlc, cfg);
		}
		break;

	default:
		ASSERT(0);
		break;
	}
}

static int
wlc_assoc_req_remove_entry(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char ssidbuf[SSID_FMT_BUF_LEN];
#endif // endif
	int i;
	int err = BCME_ERROR;
	bool as_in_progress_already;

	/* check the current state of assoc_req array */
	/* mark to see whether it's null or not */
	as_in_progress_already = AS_IN_PROGRESS(wlc);

	for (i = 0; i < WLC_MAXBSSCFG; i ++) {
		if (wlc->as->cmn->assoc_req[i] != cfg)
			continue;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
		wlc_format_ssid(ssidbuf, cfg->SSID, cfg->SSID_len);
#endif // endif
		WL_ASSOC(("wl%d.%d: remove %s request in state %s in assoc_req list "
		          "for SSID %s at slot %d\n", wlc->pub->unit, WLC_BSSCFG_IDX(cfg),
		          WLCASTYPEN(cfg->assoc->type), wlc_as_st_name(cfg->assoc->state),
		          ssidbuf, i));
		/* move assoc requests up the list by 1 and stop at the first empty entry */
		for (; i < WLC_MAXBSSCFG - 1; i ++) {
			if ((wlc->as->cmn->assoc_req[i] = wlc->as->cmn->assoc_req[i + 1]) == NULL) {
				i = i + 1;
				break;
			}
		}
		wlc->as->cmn->assoc_req[i] = NULL;
		err = BCME_OK;
		break;
	}

	/* if we cleared the wlc->as->cmn->assoc_req[] list, update ps_ctrl */
	if (as_in_progress_already != AS_IN_PROGRESS(wlc)) {
		wlc_set_wake_ctrl(wlc);
	}
	return err;
}

/* Fixup bsscfg type based on the given bss_info. Only Infrastrucutre and Independent
 * STA switch is allowed if as_in_prog (association in progress) is TRUE.
 *
 * This only needs to work on primary bsscfg, as non-primary bsscfg can be created with
 * a fixed type & subtype upfront (i.e. primary bsscfg is created without the desired
 * type & subtype).
 */
static int
wlc_assoc_fixup_bsscfg_type(wlc_info_t *wlc, wlc_bsscfg_t *cfg, wlc_bss_info_t *bi,
	bool as_in_prog)
{
	wlc_bsscfg_type_t type;
	int err;

	/* bsscfg type fix up is by passed for all non-primary and non-generic bsscfgs */
	if ((cfg != wlc_bsscfg_primary(wlc)) && (cfg->type != BSSCFG_TYPE_GENERIC)) {
		WL_INFORM(("wl%d.%d: %s: ignore non-primary bsscfg\n",
		           wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
		return BCME_OK;
	}

	if ((err = wlc_bsscfg_type_bi2cfg(wlc, cfg, bi, &type)) != BCME_OK) {
		WL_ASSOC_ERROR(("wl%d.%d: %s: wlc_bsscfg_type_bi2cfg failed with err %d.\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__, err));
		return err;
	}
	if ((err = wlc_bsscfg_type_fixup(wlc, cfg, &type, as_in_prog)) != BCME_OK) {

		return err;
	}

	return BCME_OK;
}

/** start a join process by broadcast scanning all channels */
/**
 * prepare and start a join process including:
 * - abort any ongoing association or scan process if necessary
 * - enable the bsscfg (set the flags, ..., not much for STA)
 * - start the disassoc process if already associated to a different SSID, otherwise
 * - start the scan process (broadcast on all channels, or direct on specific channels)
 * - start the association process (assoc with a BSS, start an IBSS, or coalesce with an IBSS)
 * - mark the bsscfg to be up if the association succeeds otherwise try the next BSS
 *
 * bsscfg stores the desired SSID and/or bssid and/or chanspec list for later use in
 * a different execution context for example timer callback.
 */
int
wlc_join(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, const uint8 *SSID, int len,
	wl_join_scan_params_t *scan_params,
	wl_join_assoc_params_t *assoc_params, int assoc_params_len)
{
	wlc_assoc_t *as;
	int ret;
	bool queued = FALSE;

	ASSERT(bsscfg != NULL);
	ASSERT(BSSCFG_STA(bsscfg));

	/* add the join request in the assoc_req list
	 * if someone is already in the process of association
	 */
	if ((ret = wlc_mac_request_entry(wlc, bsscfg, WLC_ACTION_ASSOC, 0)) > 0) {
		WL_ASSOC(("wl%d.%d: JOIN request queued at slot %d\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), ret));
		/* still need to go through the following code except
		 * wlc_join_start() before return...
		 */
		queued = TRUE;
		ret = BCME_OK;
	} else if (ret < 0) {
		WL_ASSOC_ERROR(("wl%d.%d: JOIN request failed, err = %d\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), ret));
		return ret;
	}

	as = bsscfg->assoc;
	ASSERT(as != NULL);

	as->ess_retries = 0;
	as->bss_retries = 0;

	as->type = AS_ASSOCIATION;

	/* save SSID and assoc params for later use in a different context and retry */
	wlc_bsscfg_SSID_set(bsscfg, SSID, len);

	wlc_bsscfg_scan_params_set(wlc, bsscfg, scan_params);
	wlc_bsscfg_assoc_params_set(wlc, bsscfg, assoc_params, assoc_params_len);

	wlc_join_attempt_info_init(bsscfg);

	/* the request is queued so just return */
	if (!queued) {
		scan_params = wlc_bsscfg_scan_params(bsscfg);
		assoc_params = wlc_bsscfg_assoc_params(bsscfg);

		ret = wlc_join_start(bsscfg, scan_params, assoc_params);
	}
	return ret;
} /* wlc_join */

static void
wlc_assoc_timer_del(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	wlc_assoc_t *as = cfg->assoc;

	wl_del_timer(wlc->wl, as->timer);
	as->rt = FALSE;
}

void
wlc_assoc_timer_add(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	wlc_assoc_timer_del(wlc, cfg);
	wl_add_timer(wlc->wl, cfg->assoc->timer, WECA_ASSOC_TIMEOUT + 10, 0);
}

/**
 * prepare for join by using the default parameters in wlc->default_bss
 * except SSID which comes from the cfg->SSID
 */
int
wlc_join_start_prep(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	wlc_bss_info_t *target_bss = cfg->target_bss;
	int ret;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char *ssidbuf;
	const char *ssidstr;

	if (cfg->SSID_len == 0)
		WL_ASSOC(("wl%d: JOIN: wlc_join_start_prep, Setting SSID to NULL...\n",
		          WLCWLUNIT(wlc)));
	else {
		ssidbuf = (char *) MALLOC(wlc->osh, SSID_FMT_BUF_LEN);
		if (ssidbuf) {
			wlc_format_ssid(ssidbuf, cfg->SSID, cfg->SSID_len);
			ssidstr = ssidbuf;
		} else
			ssidstr = "???";
		WL_ASSOC(("wl%d: JOIN: wlc_join_start_prep, Setting SSID to \"%s\"...\n",
		          WLCWLUNIT(wlc), ssidstr));
		if (ssidbuf != NULL)
			 MFREE(wlc->osh, (void *)ssidbuf, SSID_FMT_BUF_LEN);
	}
#endif /* #if defined(BCMDBG) || defined(WLMSG_ASSOC) */

	/* adopt the default BSS params as the target's BSS params */
	bcopy(wlc->default_bss, target_bss, sizeof(wlc_bss_info_t));

	/* update the target_bss with the ssid */
	target_bss->SSID_len = cfg->SSID_len;
	if (cfg->SSID_len > 0)
		bcopy(cfg->SSID, target_bss->SSID, cfg->SSID_len);

	/* this is STA only, no aps_associated issues */
	ret = wlc_bsscfg_enable(wlc, cfg);
	if (ret) {
		WL_ASSOC_ERROR(("wl%d: %s: Can not enable bsscfg,err:%d\n", wlc->pub->unit,
			__FUNCTION__, ret));
		return ret;
	}
	if (cfg->assoc->state != AS_WAIT_DISASSOC) {
		wlc_assoc_change_state(cfg, AS_JOIN_INIT);
		wlc_bss_assoc_state_notif(wlc, cfg, cfg->assoc->type, AS_JOIN_INIT);
	}

	return BCME_OK;
} /* wlc_join_start_prep */

/* kick off
 * the association scan...
 */
static int
wlc_join_assoc_prep(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	wl_join_scan_params_t *scan_params, wl_join_assoc_params_t *assoc_params)
{
	const struct ether_addr *bssid = NULL;
	const chanspec_t *chanspec_list = NULL;
	int chanspec_num = 0;
	int err;

	/* TODO: insert bsscfg type fixup code here...
	 * it could be deferred further until the join target is chosen
	 * after the assoc scan but the assoc scan itself may need to know
	 * the bsscfg type as well...
	 */

	/* continue the join process by starting the association scan */
	if ((err = wlc_join_assoc_params_parse(wlc, cfg, assoc_params, &bssid,
			&chanspec_list, &chanspec_num)) != BCME_OK) {
		WL_ASSOC(("wl%d.%d: wlc_join_assoc_params_parse failed. err %d\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), err));
		goto fail;
	}
	return wlc_assoc_scan_prep(cfg, scan_params, bssid, chanspec_list, chanspec_num);

fail:
	return err;
}

#if defined(STA) && defined(AP)
void wlc_try_join_start(wlc_bsscfg_t *cfg, wl_join_scan_params_t *scan_params,
	wl_join_assoc_params_t *assoc_params)
{
	/* Handle only APSTA case */
	if (!BSSCFG_IS_PRIMARY(cfg) || !BSSCFG_STA(cfg) || !APSTA_ENAB(cfg->wlc->pub)) {
		return;
	}

	if (cfg->SSID_len == 0) {
		/* boot up time, let userspace app to come up with explicit WLC_SET_SSID
		 * or IOV_JOIN request
		 */
		WL_INFORM(("wl%d: Invalid SSID len[%d] to join, return \n", cfg->wlc->pub->unit,
			cfg->SSID_len));
		return;
	}
	if (cfg->roam && cfg->roam->off) {
		/* Run time case, user maintains client's association state machine */
		WL_INFORM(("wl%d: roaming disabled in firmware, skip try join start\n",
			cfg->wlc->pub->unit));
		return;
	}

	WL_INFORM(("wl%d: try join for ssid[%s] with cfg index[%d] \n", cfg->wlc->pub->unit,
		cfg->SSID, WLC_BSSCFG_IDX(cfg)));

	wlc_join_start(cfg, scan_params, assoc_params);
}
#endif /* STA & AP */
/** start a join process. SSID is saved in bsscfg. finish up the process
 * if there's any error.
 */
static int
wlc_join_start(wlc_bsscfg_t *cfg, wl_join_scan_params_t *scan_params,
	wl_join_assoc_params_t *assoc_params)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif // endif
	int ret = BCME_OK;

	as->ess_retries ++;

	ASSERT(as->state == AS_IDLE || as->state == AS_WAIT_DISASSOC);

	/* setting up SSID/default_bss/etc... */
	(void)wlc_join_start_prep(wlc, cfg);

	if (as->state == AS_WAIT_DISASSOC) {
		/* we are interrupting an earlier wlc_set_ssid call and wlc_assoc_scan_start is
		 * scheduled to run as the wlc_disassociate_client callback. The target_bss has
		 * been updated above so just allow the wlc_assoc_scan_start callback to pick up
		 * the set_ssid work with the new target.
		 */
	} else {
		wlc_bss_info_t *current_bss = cfg->current_bss;

		wlc_assoc_init(cfg, AS_ASSOCIATION);
		if (cfg->associated &&
			(cfg->SSID_len != current_bss->SSID_len ||
			memcmp(cfg->SSID, (char*)current_bss->SSID, cfg->SSID_len) ||
			(assoc_params &&
			memcmp(&current_bss->BSSID, &assoc_params->bssid,
			sizeof(assoc_params->bssid))))) {

			WL_ASSOC(("wl%d: JOIN: wlc_join_start, Disassociating from %s first\n",
				WLCWLUNIT(wlc), bcm_ether_ntoa(&current_bss->BSSID, eabuf)));
			wlc_assoc_change_state(cfg, AS_WAIT_DISASSOC);
			wlc_setssid_disassociate_client(cfg);
		} else {
			/* make sure the association timer is not pending */
			wlc_assoc_timer_del(wlc, cfg);

			/* continue the join process by starting the association scan */
			ret = wlc_join_assoc_prep(wlc, cfg, scan_params, assoc_params);
		}
	}

	return ret;
} /* wlc_join_start */

/* parse assoc parameters for chanspec list (and bssid list?) */
static int
wlc_join_assoc_params_parse(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	wl_join_assoc_params_t *assoc_params, const struct ether_addr **bssid,
	const chanspec_t **chanspec_list, int *chanspec_num)
{
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN];
	char chanbuf[CHANSPEC_STR_LEN];
#endif // endif

	/* use assoc params if any to limit the scan hence to speed up
	 * the join process.
	 */
	if (assoc_params != NULL) {
		int bccnt;

		*bssid = &assoc_params->bssid;
		*chanspec_list = assoc_params->chanspec_list;
		*chanspec_num = assoc_params->chanspec_num;
		bccnt = assoc_params->bssid_cnt;

		WL_ASSOC(("wl%d: JOIN: chanspec_num %d "
		        "bssid_cnt %d\n", WLCWLUNIT(wlc), *chanspec_num, bccnt));

		/* if BSSID/channel pairs specified, adjust accordingly */
		if (bccnt) {
			/* channel number field is index for channel and bssid */
			*bssid = (const struct ether_addr*)&(*chanspec_list)[bccnt];
			*chanspec_list += *chanspec_num;
			*bssid += *chanspec_num;

			/* check if we reached the end */
			if (*chanspec_num >= bccnt) {
				WL_ASSOC(("wl: JOIN: pairs done\n"));
				wlc_bsscfg_assoc_params_reset(wlc, cfg);
				return BCME_BADARG;
			}

			WL_ASSOC(("wl%d: JOIN: pair %d of %d,"
				"BSS %s, chanspec %s\n", WLCWLUNIT(wlc),
				*chanspec_num, bccnt,
				bcm_ether_ntoa(*bssid, eabuf),
				wf_chspec_ntoa_ex(**chanspec_list, chanbuf)));

			/* force count to one (only this channel) */
			*chanspec_num = 1;
		}
	}

	return BCME_OK;
} /* wlc_join_assoc_params_parse */

/** disassoc from the associated BSS first and then start a join process */
static void
wlc_setssid_disassoc_complete(wlc_info_t *wlc, uint txstatus, void *arg)
{
	wl_join_scan_params_t *scan_params;
	wl_join_assoc_params_t *assoc_params;
	wlc_bsscfg_t *cfg = wlc_bsscfg_find_by_ID(wlc, (uint16)(uintptr)arg);
	wlc_assoc_t *as;

	BCM_REFERENCE(txstatus);

	/* in case bsscfg is freed before this callback is invoked */
	if (cfg == NULL) {
		WL_NONE(("wl%d: %s: unable to find bsscfg by ID %p\n",
		          wlc->pub->unit, __FUNCTION__,
				OSL_OBFUSCATE_BUF(arg)));
		return;
	}

	as = cfg->assoc;

	/* Check for aborted assoc */
	if (as->type != AS_ASSOCIATION) {
		WL_ASSOC(("wl%d: wlc_setssid_disassoc_complete, as->type "
		          "was changed to %d\n", WLCWLUNIT(wlc), as->type));
		return;
	}

	if (as->state != AS_WAIT_DISASSOC) {
		WL_ASSOC(("wl%d: wlc_setssid_disassoc_complete, as->state "
		          "was changed to %d\n", WLCWLUNIT(wlc), as->state));
		return;
	}

	WL_ASSOC(("wl%d: JOIN: disassociation complete\n", WLCWLUNIT(wlc)));

	/* use assoc params if any to limit the scan hence to speed up
	 * the join process.
	 */
	scan_params = wlc_bsscfg_scan_params(cfg);
	assoc_params = wlc_bsscfg_assoc_params(cfg);

	/* continue the join process by starting the association scan */
	wlc_join_assoc_prep(wlc, cfg, scan_params, assoc_params);
} /* wlc_setssid_disassoc_complete */

/**
 * Roaming related, STA chooses the best prospect AP to associate with.
 * If bss-transition scoring is not enabled, rssi is used for scoring. Else, bss-transition score is
 * used along with score_delta and rssi is used with roam_delta.
 * input: bsscfg, bss_info, -ve rssi.
 * output: prssi (positive rssi) and score (return value)
 */
static uint32
wlc_bss_pref_score_rssi(wlc_bsscfg_t *cfg, wlc_bss_info_t *bi, int16 rssi, uint32 *prssi)
{
#ifdef WLWNM
	uint32 score;
#endif /* WLWNM */

	BCM_REFERENCE(bi);
	BCM_REFERENCE(cfg);

	/* convert negative value to positive */
	*prssi = WLCMAXVAL(WLC_JOIN_PREF_BITS_RSSI) + rssi;

#ifdef WLWNM
	if (WLWNM_ENAB(cfg->wlc->pub)) {
		if ((wlc_wnm_bss_pref_score_rssi(cfg, bi,
			MAX(WLC_RSSI_MINVAL_INT8, rssi), &score) == BCME_OK))
			return score;
	}
#endif /* WLWNM */
	return *prssi;
}

/** Roaming related, STA chooses the best prospect AP to associate with */
uint32
wlc_bss_pref_score(wlc_bsscfg_t *cfg, wlc_bss_info_t *bi, bool band_rssi_boost, uint32 *prssi)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_join_pref_t *join_pref = cfg->join_pref;
	int j;
	int16 rssi;
	uint32 weight, value;
	uint chband;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_ASSOC */
	(void)wlc;

	/* clamp RSSI to the range 0 > rssi >= WLC_RSSI_MINVAL
	 * (The 0 RSSI is invalid at this point, and is converted to lowest)
	 */
	rssi = (bi->RSSI != WLC_RSSI_INVALID) ? bi->RSSI : WLC_RSSI_MINVAL;
	rssi = MIN(0, rssi);
	rssi = MAX(rssi, WLC_RSSI_MINVAL);

	chband = CHSPEC2WLC_BAND(bi->chanspec);

	/* RSSI boost (positive value to the fixed band) by join preference:
	 * Give target a better RSSI based on delta preference as long as it is already
	 * has a strong enough signal.
	 */
	if (cfg->join_pref_rssi_delta.band == chband && band_rssi_boost &&
	    cfg->join_pref_rssi_delta.rssi != 0 &&
	    rssi >= WLC_JOIN_PREF_RSSI_BOOST_MIN) {

		WL_ASSOC(("wl%d: Boost RSSI for AP on band %d by %d db from %d db to %d db\n",
		          wlc->pub->unit,
		          cfg->join_pref_rssi_delta.band,
		          cfg->join_pref_rssi_delta.rssi,
		          rssi, MIN(0, rssi+cfg->join_pref_rssi_delta.rssi)));

		rssi += cfg->join_pref_rssi_delta.rssi;

		/* clamp RSSI again to the range 0 >= rssi >= WLC_RSSI_MINVAL
		 * (The 0 RSSI is considered valid at this point which is the max possible)
		 */
		rssi = MIN(0, rssi);

		WL_SRSCAN(("  pref: boost rssi: band=%d delta=%d rssi=%d",
			cfg->join_pref_rssi_delta.band,
			cfg->join_pref_rssi_delta.rssi, rssi));
		WL_ASSOC(("  pref: boost rssi: band=%d delta=%d rssi=%d\n",
			cfg->join_pref_rssi_delta.band,
			cfg->join_pref_rssi_delta.rssi, rssi));
	}

	/* RSSI boost (positive & negative value to the other band) by roam profile:
	 * Give target a better RSSI based on delta preference as long as it is already
	 * has a strong enough signal.
	 */
	if (band_rssi_boost &&
	    CHSPEC2WLC_BAND(cfg->current_bss->chanspec) != chband &&
	    cfg->roam->roam_rssi_boost_delta != 0 &&
	    rssi >= cfg->roam->roam_rssi_boost_thresh) {
		rssi += cfg->roam->roam_rssi_boost_delta;

		/* clamp RSSI again to the range 0 > rssi > WLC_RSSI_MINVAL */
		rssi = MIN(0, rssi);
		rssi = MAX(rssi, WLC_RSSI_MINVAL);

		WL_SRSCAN(("  prof: boost rssi: band=%d delta=%d rssi=%d",
			chband, cfg->roam->roam_rssi_boost_delta, rssi));
		WL_ASSOC(("  prof: boost rssi: band=%d delta=%d rssi=%d\n",
			chband, cfg->roam->roam_rssi_boost_delta, rssi));
	}

	for (j = 0, weight = 0; j < (int)join_pref->fields; j++) {
		switch (join_pref->field[j].type) {
		case WL_JOIN_PREF_RSSI:
			value = wlc_bss_pref_score_rssi(cfg, bi, rssi, prssi);
			break;
		case WL_JOIN_PREF_WPA:
			/* use index as preference weight */
			value = bi->wpacfg;
			break;
		case WL_JOIN_PREF_BAND:
			/* use 1 for preferred band */
			if (join_pref->band == WLC_BAND_AUTO) {
				value = 0;
				break;
			}
			value = (chband == join_pref->band) ? 1 : 0;
			break;

		default:
			/* quiet compiler, should not come to here! */
			value = 0;
			break;
		}
		value &= WLCBITMASK(join_pref->field[j].bits);
		WL_ASSOC(("wl%d: wlc_bss_pref_score: field %s entry %d value 0x%x offset %d\n",
			WLCWLUNIT(wlc), WLCJOINPREFN(join_pref->field[j].type),
			j, value, join_pref->field[j].start));
		weight += value << join_pref->field[j].start;

		WL_SRSCAN(("  pref: apply: type=%d start=%d bits=%d",
			join_pref->field[j].type, join_pref->field[j].start,
			join_pref->field[j].bits));
		WL_SRSCAN(("  pref: apply: delta=%d score=%d",
			value << join_pref->field[j].start, weight));
		WL_ASSOC(("  pref: apply: type=%d start=%d bits=%d\n",
			join_pref->field[j].type, join_pref->field[j].start,
			join_pref->field[j].bits));
		WL_ASSOC(("  pref: apply: delta=%d score=%d\n",
			value << join_pref->field[j].start, weight));
	}

	/* pref fields may not be set; use rssi only to get the positive number */
	if (join_pref->fields == 0) {
		weight = wlc_bss_pref_score_rssi(cfg, bi, rssi, prssi);
		WL_SRSCAN(("  pref: rssi only: delta=%d score=%d",
			weight, weight));
		WL_ASSOC(("  pref: rssi only: delta=%d score=%d\n",
			weight, weight));
	}

	WL_ASSOC(("wl%d: %s: RSSI is %d in BSS %s with preference score 0x%x (qbss_load_aac 0x%x "
	          "and qbss_load_chan_free 0x%x)\n", WLCWLUNIT(wlc), __FUNCTION__, bi->RSSI,
	          bcm_ether_ntoa(&bi->BSSID, eabuf), weight, bi->qbss_load_aac,
	          bi->qbss_load_chan_free));

#ifdef WLWNM
	if (WBTEXT_ACTIVE(wlc->pub) &&
		(band_rssi_boost || (bi == cfg->current_bss))) {

		/* XXX: Enabling debug print for "wbtext" svt verification.
		 * This will be disabled in production image later.
		 */
		wlc_wnm_bsstrans_print_score(wlc->wnm_info, cfg, bi, rssi, weight);
	}
#endif /* WLWNM */

	return weight;
} /* wlc_bss_pref_score */

/** Roaming related */
static void
wlc_zero_pref(join_pref_t *join_pref)
{
	join_pref->score = 0;
	join_pref->rssi = 0;
}

/** Rates the candidate APs available to a STA, to subsequently make a join/roam decision */
static void
wlc_populate_join_pref_score(wlc_bsscfg_t *cfg, bool for_roam, join_pref_t *join_pref)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_roam_t *roam = cfg->roam;
	wlc_bss_info_t **bip = wlc->as->cmn->join_targets->ptrs;
	int i, j;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_ASSOC */

	for (i = 0; i < (int)wlc->as->cmn->join_targets->count; i++) {
		if ((roam->reason == WLC_E_REASON_BSSTRANS_REQ) &&
			(WLC_IS_CURRENT_BSSID(cfg, &bip[i]->BSSID)) &&
#ifdef WLWNM
			(!WLWNM_ENAB(wlc->pub) ||
			wlc_wnm_bsstrans_zero_assoc_bss_score(wlc->wnm_info, cfg) ||
			FALSE) &&
#endif /* WLWNM */
			TRUE) {
				wlc_zero_pref(&join_pref[i]);
		} else {
			WL_SRSCAN(("  targ: bssid=%02x:%02x ch=%d rssi=%d",
				bip[i]->BSSID.octet[4], bip[i]->BSSID.octet[5],
				wf_chspec_ctlchan(bip[i]->chanspec), bip[i]->RSSI));
			WL_ASSOC(("  targ: bssid=%02x:%02x ch=%d rssi=%d\n",
				bip[i]->BSSID.octet[4], bip[i]->BSSID.octet[5],
				wf_chspec_ctlchan(bip[i]->chanspec), bip[i]->RSSI));
			WL_ROAM(("ROAM: %02d %02x:%02x:%02x ch=%d rssi=%d\n",
				i, bip[i]->BSSID.octet[0], bip[i]->BSSID.octet[4],
				bip[i]->BSSID.octet[5],	wf_chspec_ctlchan(bip[i]->chanspec),
				bip[i]->RSSI));

			join_pref[i].score =
				wlc_bss_pref_score(cfg, bip[i], TRUE, &join_pref[i].rssi);
		}

		/*
		 * For every bss entry found perform below operations:
		 * 1. update non neighbor best score if needed
		 * 2. While roaming, continue blacklist on previously blacklisted AP
		 *	  to prevent roaming back to same AP within disassociation interval
		 */
		if (WBTEXT_ACTIVE(wlc->pub)) {
			if (for_roam && (wlc_wnm_is_blacklisted_bss(wlc->wnm_info, cfg, bip[i]))) {
				wlc_zero_pref(&join_pref[i]);
			}
			wlc_wnm_update_nonnbr_bestscore(wlc->wnm_info,
				cfg, bip[i], join_pref[i].score);
		}

		/* Iterate through the roam cache and check that the target is valid */
		if (!for_roam || !roam->cache_valid || (roam->cache_numentries == 0))
			continue;

		for (j = 0; j < (int) roam->cache_numentries; j++) {
			struct ether_addr* bssid = &bip[i]->BSSID;
			if (!bcmp(bssid, &roam->cached_ap[j].BSSID, ETHER_ADDR_LEN) &&
			    (roam->cached_ap[j].chanspec ==
			     wf_chspec_ctlchspec(bip[i]->chanspec))) {
				if (j != 0) {
					roam->cached_ap[j].rssi_st.rssi = bip[i]->RSSI;
					roam->cached_ap[j].bss_load = bip[i]->qbss_load_aac;
					roam->cached_ap[j].time = OSL_SYSUPTIME();
				}
				if (roam->cached_ap[j].time_left_to_next_assoc > 0) {
					wlc_zero_pref(&join_pref[i]);
					WL_SRSCAN(("  excluded: bssid=%02x:%02x "
						"next_assoc=%ds",
						bssid->octet[4], bssid->octet[5],
						roam->cached_ap[j].time_left_to_next_assoc));
					WL_ASSOC(("  excluded: AP with BSSID %s marked "
					          "as an invalid roam target\n",
					          bcm_ether_ntoa(bssid, eabuf)));
				}
			}
		}
	}
} /* wlc_populate_join_pref_score */

/** Rates the candidate APs available to a STA, to subsequently make a join/roam decision */
static void
wlc_sort_on_join_pref_score(wlc_bsscfg_t *cfg, join_pref_t *join_pref)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_bss_info_t **bip = wlc->as->cmn->join_targets->ptrs;
	wlc_bss_info_t *tmp_bi;
	int j, k;
	join_pref_t tmp_join_pref;
	bool done;
	bool force_sort = FALSE;

	/* no need to sort if we only see one AP */
	if (wlc->as->cmn->join_targets->count > 1) {
		/* sort join_targets by join preference score in increasing order */
		for (k = (int)wlc->as->cmn->join_targets->count; --k >= 0;) {
			done = TRUE;
			for (j = 0; j < k; j++) {
				if (join_pref[j].score > join_pref[j+1].score) {
					force_sort = TRUE;
				}
#ifdef WNM_BSSTRANS_EXT
				else if (WBTEXT_ACTIVE(wlc->pub) &&
					(join_pref[j].score == join_pref[j+1].score)) {
					/* Give preference to band and then rssi and then cu */
					if ((CHSPEC2WLC_BAND(bip[j]->chanspec) == WLC_BAND_5G) &&
					(CHSPEC2WLC_BAND(bip[j+1]->chanspec) == WLC_BAND_2G)) {
						force_sort = TRUE;
					} else if (CHSPEC2WLC_BAND(bip[j]->chanspec) ==
						CHSPEC2WLC_BAND(bip[j+1]->chanspec)) {
						if (bip[j]->RSSI > bip[j+1]->RSSI) {
							force_sort = TRUE;
						}
						else if ((bip[j]->RSSI == bip[j+1]->RSSI) &&
							(bip[j]->qbss_load_chan_free >
								bip[j+1]->qbss_load_chan_free)) {
							force_sort = TRUE;
						}
					}
				}
#endif /* WNM_BSSTRANS_EXT */

				if (force_sort) {
					/* swap join_pref */
					tmp_join_pref = join_pref[j];
					join_pref[j] = join_pref[j+1];
					join_pref[j+1] = tmp_join_pref;
					/* swap bip */
					tmp_bi = bip[j];
					bip[j] = bip[j+1];
					bip[j+1] = tmp_bi;
					done = FALSE;
					force_sort = FALSE;
				}
			}
			if (done)
				break;
		}
	}
}

/* update pref score of BTM neighbor listed AP */
bool
wlc_update_pref_score(void *join_pref_list, uint8 index, uint32 threshold, uint32 score)
{
	join_pref_t *join_pref = (join_pref_t *)join_pref_list;
	bool ret = FALSE;

	if (join_pref[index].score > threshold) {
		join_pref[index].score += score;
		ret = TRUE;
	}

	return ret;
}

#ifdef WNM_BSSTRANS_EXT
static void
wlc_dump_join_pref(wlc_bsscfg_t *cfg)
{
	wlc_join_pref_t *join_pref = cfg->join_pref;
	uint32 delta = 0;
	uint32 band = 0;
	uint32 p = 0;
	int i;

	/* return if join_pref is not active */
	if (!join_pref || !join_pref->fields) {
		return;
	}

	for (i = 0; i < join_pref->fields; i ++) {
		p |= WLCTYPEBMP(join_pref->field[i].type);
	}

	if (cfg->join_pref_rssi_delta.rssi != 0 &&
		cfg->join_pref_rssi_delta.band != WLC_BAND_AUTO) {
		delta = cfg->join_pref_rssi_delta.rssi,
		band = cfg->join_pref_rssi_delta.band;
		p |= WLCTYPEBMP(WL_JOIN_PREF_RSSI_DELTA);
	}
	BCM_REFERENCE(delta);
	BCM_REFERENCE(band);

	WBTEXT_INFO(("WBTEXT DBG: JOINPREF: type:%02X, RSSI Delta:%d:%d\n",
		p, delta, band));
}
#endif /* WNM_BSSTRANS_EXT */

/** Rates the candidate APs available to a STA, to subsequently make a join/roam decision */
static void
wlc_cook_join_targets(wlc_bsscfg_t *cfg, bool for_roam, int cur_rssi)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_roam_t *roam = cfg->roam;
	int i, j;
	wlc_bss_info_t **bip, *tmp_bi;
	uint roam_delta = 0;
#if defined(BCMDBG) || defined(WLMSG_ASSOC) || defined(WLMSG_ROAM)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_ASSOC */
	join_pref_t *join_pref, tmp_join_pref;
	uint32 join_pref_score_size = wlc->as->cmn->join_targets->count * sizeof(*join_pref);
	uint32 cur_pref_score = 0, cur_pref_rssi = 0, score_delta = 0;
#ifdef OPPORTUNISTIC_ROAM
	int k, best_ap_idx;
#endif /* OPPORTUNISTIC_ROAM */
	uint bss_cnt = 0;

	WL_SRSCAN(("wl%d: RSSI is %d; %d roaming target[s]; Join preference fields "
		"are 0x%x", WLCWLUNIT(wlc), cur_rssi, wlc->as->cmn->join_targets->count,
		cfg->join_pref->fields));
	WL_ASSOC(("wl%d: RSSI is %d; %d roaming target[s]; Join preference fields "
		"are 0x%x\n", WLCWLUNIT(wlc), cur_rssi, wlc->as->cmn->join_targets->count,
		cfg->join_pref->fields));
	WL_ROAM(("ROAM: RSSI %d, %d roaming targets\n", cur_rssi,
		wlc->as->cmn->join_targets->count));

	join_pref = (join_pref_t *)MALLOCZ(wlc->osh, join_pref_score_size);
	if (!join_pref) {
		WL_ERROR(("wl%d: %s: MALLOC failure\n",
			WLCWLUNIT(wlc), __FUNCTION__));
		return;
	}

	bip = wlc->as->cmn->join_targets->ptrs;
	bss_cnt = wlc->as->cmn->join_targets->count;
	BCM_REFERENCE(bss_cnt);

#ifdef WLWNM
	if (WLWNM_ENAB(cfg->wlc->pub)) {
		wlc_wnm_process_join_trgts_bsstrans(wlc->wnm_info, cfg, bip,
			wlc->as->cmn->join_targets->count);
	}
#endif /* WLWNM */
#ifdef WNM_BSSTRANS_EXT
	if (WBTEXT_ACTIVE(wlc->pub)) {
		wlc_dump_join_pref(cfg);
	}
#endif /* WNM_BSSTRANS_EXT */

#ifdef WL_OCE
	if (OCE_ENAB(wlc->pub)) {
		oce_calc_join_pref(cfg, bip, bss_cnt, join_pref);
	} else
#endif /* WL_OCE */
	wlc_populate_join_pref_score(cfg, for_roam, join_pref);

	wlc_sort_on_join_pref_score(cfg, join_pref);

	if (for_roam &&
#ifdef OPPORTUNISTIC_ROAM
	    (roam->reason != WLC_E_REASON_BETTER_AP) &&
#endif /* OPPORTUNISTIC_ROAM */
	    TRUE) {
		wlc_bss_info_t *current_bss = cfg->current_bss;
		/*
		 * - We're here because our current AP's RSSI fell below some threshold
		 *   or we haven't heard from him for a while.
		 */

		if (wlc_bss_connected(cfg)) {
			WL_SRSCAN(("  curr: bssid=%02x:%02x ch=%d rssi=%d",
				current_bss->BSSID.octet[4], current_bss->BSSID.octet[5],
				wf_chspec_ctlchan(current_bss->chanspec), cur_rssi));
			WL_ASSOC(("  curr: bssid=%02x:%02x ch=%d rssi=%d\n",
				current_bss->BSSID.octet[4], current_bss->BSSID.octet[5],
				wf_chspec_ctlchan(current_bss->chanspec), cur_rssi));
			cur_pref_score =
				wlc_bss_pref_score(cfg, current_bss, FALSE, &cur_pref_rssi);
		}

		/*
		 * Use input cur_rssi if we didn't get a probe response because we were
		 * unlucky or we're out of contact; otherwise, use the RSSI of the probe
		 * response. The channel of the probe response/beacon is checked in case
		 * the AP switched channels. In that case we do not want to update cur_rssi
		 * and instead consider the AP as a roam target.
		 */
		if (wlc_bss_connected(cfg) && roam->reason == WLC_E_REASON_LOW_RSSI) {
			roam_delta = wlc->band->roam_delta;
		} else {
			cur_pref_score = cur_pref_rssi = 0;
			roam_delta = 0;
		}

		/* update associated AP score from scan results */
		for (i = (int)wlc->as->cmn->join_targets->count - 1; i >= 0; i--) {
			if (WLC_IS_CURRENT_BSSID(cfg, &bip[i]->BSSID) &&
				(wf_chspec_ctlchan(bip[i]->chanspec) ==
				wf_chspec_ctlchan(current_bss->chanspec))) {
				cur_pref_score = join_pref[i].score;
				cur_pref_rssi = join_pref[i].rssi;
				cur_rssi = bip[i]->RSSI;
				WL_SRSCAN(("  curr(scan): bssid=%02x:%02x ch=%d rssi=%d",
					current_bss->BSSID.octet[4], current_bss->BSSID.octet[5],
					wf_chspec_ctlchan(current_bss->chanspec), cur_rssi));
				WL_ASSOC(("  curr(scan): bssid=%02x:%02x ch=%d rssi=%d\n",
					current_bss->BSSID.octet[4], current_bss->BSSID.octet[5],
					wf_chspec_ctlchan(current_bss->chanspec), cur_rssi));
				break;
			}
		}

		WL_ASSOC(("wl%d: ROAM: wlc_cook_join_targets, roam_metric[%s] = 0x%x\n",
			WLCWLUNIT(wlc), bcm_ether_ntoa(&(cfg->BSSID), eabuf),
			cur_pref_score));

		/* Prune candidates not "significantly better" than our current AP.
		 * Notes:
		 * - The definition of "significantly better" may not be the same for
		 *   all candidates.  For example, Jason suggests using a different
		 *   threshold for our previous AP, if we roamed before.
		 * - The metric will undoubtedly need to change.  First pass, just use
		 *   the RSSI of the probe response.
		 */

		/* Try to make roam_delta adaptive. lower threshold when our AP is very weak,
		 * falling the edge of sensitivity. Hold threshold when our AP is relative strong.
		 * (hold a minimum 10dB delta to avoid ping-pong roaming)
		 *
		 *	roam_trigger+10	roam_trigger	roam_trigger-10	roam_trigger-20
		 *	|		|		|		|
		 *	| roam_delta    | roam_delta	| roam_delta-10 | roam_delta-10
		 */
		if (!wlc_roam_scan_islazy(wlc, cfg, FALSE) &&
		    (cur_rssi <= wlc->band->roam_trigger) && (roam_delta > 10)) {
			WL_SRSCAN(("  adapting roam delta"));
			WL_ASSOC(("  adapting roam delta\n"));
			roam_delta =
				MAX((int)(roam_delta - (wlc->band->roam_trigger - cur_rssi)), 10);
		}

		WL_SRSCAN(("  criteria: rssi=%d roam_delta=%d",
			cur_pref_rssi, roam_delta));
		WL_ASSOC(("  criteria: rssi=%d roam_delta=%d\n",
			cur_pref_rssi, roam_delta));

		/* TODO
		 * It may be advisable not to apply roam delta-based prune once
		 * preference score is above a certain threshold.
		 */

		WL_ASSOC(("wl%d: ROAM: wlc_cook_join_targets, roam_delta = %d\n",
		      WLCWLUNIT(wlc), roam_delta));
		WL_ROAM(("ROAM: cur score %d(%d), trigger/delta %d/%d\n", cur_pref_score,
		(int)cur_pref_score-255, wlc->band->roam_trigger, roam_delta));

#ifdef WLWNM
		if (WBTEXT_ACTIVE(wlc->pub)) {
			wlc_wnm_info_t *wnm = wlc->wnm_info;

			wlc_wnm_bsstrans_update_scoredelta(wnm, cfg, cur_pref_score);
			score_delta = wlc_wnm_bsstrans_get_scoredelta(wnm, cfg);

			/* Check if pref candidates needs to be bumped up
			 * to prioritize them over non pref candidates
			 */
			if (wlc_wnm_update_join_pref_score(wnm, cfg,
				wlc->as->cmn->join_targets, join_pref, cur_pref_score)) {
				/* Sort the list again as scores are updated */
				wlc_sort_on_join_pref_score(cfg, join_pref);
			}
		}
		else if (WLWNM_ENAB(wlc->pub) &&
			wlc_wnm_bsstrans_is_product_policy(wlc->wnm_info)) {
			/* Zero trgt score if:
			 * It does not meet roam_delta improvement OR
			 * The sta_cnt is maxed and hence trgt does not accept new connections.
			 */
			for (i = 0; i < (int)wlc->as->cmn->join_targets->count; i++) {
				WL_WNM_BSSTRANS_LOG("bssid %02x:%02x rssi: cur:%d trgt:%d\n",
					bip[i]->BSSID.octet[4], bip[i]->BSSID.octet[5],
					cur_pref_rssi, join_pref[i].rssi);
				if (roam->reason == WLC_E_REASON_BSSTRANS_REQ) {
					bool is_below_rssi_thresh = FALSE;
					if (wlc_wnm_btm_get_rssi_thresh(wlc->wnm_info, cfg) != 0) {
						/* convert negative rssi to positive */
						uint32 rssi_thresh =
						WLCMAXVAL(WLC_JOIN_PREF_BITS_RSSI)+
						wlc_wnm_btm_get_rssi_thresh(wlc->wnm_info, cfg);
						if (join_pref[i].rssi < rssi_thresh) {
							/* below rssi thresh */
							is_below_rssi_thresh = TRUE;
							WL_WNM_BSSTRANS_LOG("bss %02x:%02x "
								"rssi below thresh:%d trgt:%d\n",
								bip[i]->BSSID.octet[4],
								bip[i]->BSSID.octet[5],
								rssi_thresh,
								join_pref[i].rssi);
						}
					}
					/* For BTM-req initiated roam, ignore candidates worse
					   than current AP and also lower than rssi_thresh, if it
					   is configured.
					 */
					if (join_pref[i].rssi < cur_pref_rssi &&
						is_below_rssi_thresh) {
						WL_WNM_BSSTRANS_LOG("bss %02x:%02x "
							"rssi below current:%d trgt:%d\n",
							bip[i]->BSSID.octet[4],
							bip[i]->BSSID.octet[5],
							cur_pref_rssi,
							join_pref[i].rssi);
						wlc_zero_pref(&join_pref[i]);
					}
				} else if ((join_pref[i].rssi < (cur_pref_rssi + roam_delta))) {
					wlc_zero_pref(&join_pref[i]);
				}

				if (bip[i]->flags2 & WLC_BSS_MAX_STA_CNT) {
					WL_WNM_BSSTRANS_LOG("zero score; sta_cnt_maxed: %d\n",
						!!(bip[i]->flags2 & WLC_BSS_MAX_STA_CNT), 0, 0, 0);
					wlc_zero_pref(&join_pref[i]);
				}
			}
			/* Sort once more to pull zero score targets to beginning */
			wlc_sort_on_join_pref_score(cfg, join_pref);

			if (wlc_bss_connected(cfg)) {
				score_delta = wlc_wnm_bsstrans_get_scoredelta(wlc->wnm_info, cfg);
			}
			WL_WNM_BSSTRANS_LOG("score_delta %d roam_delta:%d\n", score_delta,
				roam_delta, 0, 0);
		} else
#endif /* WLWNM */
		{
			score_delta = roam_delta;
		}

		/* find cutoff point for pruning.
		 * already sorted in increasing order of join_pref_score
		 */
		for (i = 0; i < (int)wlc->as->cmn->join_targets->count; i++) {
			if (join_pref[i].score > (cur_pref_score + score_delta)) {
				break;
			}
#ifdef WLWNM
			else if (WLWNM_ENAB(wlc->pub) &&
				wlc_wnm_bsstrans_is_product_policy(wlc->wnm_info)) {
				WL_WNM_BSSTRANS_LOG("bss %02x:%02x score not met cur:%d trgt:%d\n",
					bip[i]->BSSID.octet[4], bip[i]->BSSID.octet[5],
					cur_pref_score, join_pref[i].score);

				if (WBTEXT_ACTIVE(wlc->pub)) {
					WBTEXT_INFO(("WBTEXT DBG: bssid:%02x:%02x:%02x:%02x, "
						"score not met. cur:%d, trgt:%d(%d:%d)\n",
						bip[i]->BSSID.octet[2],	bip[i]->BSSID.octet[3],
						bip[i]->BSSID.octet[4], bip[i]->BSSID.octet[5],
						join_pref[i].score, cur_pref_score + score_delta,
						cur_pref_score, score_delta));
				}
			}
#endif /* WLWNM */
		}

		/* Prune, finally.
		 * - move qualifying APs to the beginning of list
		 * - note the boundary of the qualifying AP list
		 * - ccx-based pruning is done in wlc_join_attempt()
		 */
		for (j = 0; i < (int)wlc->as->cmn->join_targets->count; i++) {
			/* one last check (in case WLWNM is not enabled):
			 * when roaming in the same band, the band RSSI boost should be disabled.
			 */
			if (wlc_bss_connected(cfg) &&
				(CHSPEC2WLC_BAND(bip[i]->chanspec) ==
				CHSPEC2WLC_BAND(current_bss->chanspec))) {
				uint32 rssi_no_boost = 0, score;

				score = wlc_bss_pref_score(cfg, bip[i], FALSE, &rssi_no_boost);
				if (WBTEXT_ACTIVE(wlc->pub)) {
					if (score <= (cur_pref_score + score_delta)) {
						continue;
					}
				}
				else if (rssi_no_boost <= (cur_pref_rssi + roam_delta)) {
					continue;
				}
			}

			/* swap join_pref[i] to join_pref[j] */
			memcpy(&tmp_join_pref, &join_pref[j], sizeof(*join_pref));
			memcpy(&join_pref[j], &join_pref[i], sizeof(*join_pref));
			memcpy(&join_pref[i], &tmp_join_pref, sizeof(*join_pref));

			/* swap bip[i] with bip[j]
			 * moving bip[i] to bip[j] alone without swapping causes memory leak.
			 * by swapping, the ones below threshold are still left in bip
			 * so that they can be freed at later time.
			 */
			tmp_bi = bip[j];
			bip[j] = bip[i];
			bip[i] = tmp_bi;
			WL_ASSOC(("wl%d: ROAM: cook_join_targets, after prune, roam_metric[%s] "
				"= 0x%x\n", WLCWLUNIT(wlc),
				bcm_ether_ntoa(&bip[j]->BSSID, eabuf), join_pref[j].score));
			WL_ROAM(("ROAM:%d %s %d(%d) Q%d\n", j,
				bcm_ether_ntoa(&bip[j]->BSSID, eabuf), join_pref[j].score,
				(int)join_pref[j].score - 255, bip[j]->qbss_load_chan_free));
			j++;
		}
		wlc->as->cmn->join_targets_last = j;
		WL_SRSCAN(("  result: %d target(s)", wlc->as->cmn->join_targets_last));
		WL_ASSOC(("  result: %d target(s)\n", wlc->as->cmn->join_targets_last));
		WL_ROAM(("ROAM: %d targets after prune\n", j));
	}

	MFREE(wlc->osh, join_pref, join_pref_score_size);

	/* Now sort pruned list using per-port criteria */
	if (wlc->pub->wlfeatureflag & WL_SWFL_WLBSSSORT)
		(void)wl_sort_bsslist(wlc->wl, bip, wlc->as->cmn->join_targets_last);

#ifdef OPPORTUNISTIC_ROAM
	if (!wlc->as->cmn->join_targets_last)
		return;
	best_ap_idx = (int)wlc->as->cmn->join_targets->count - 1;

	/* As per requirement, place the join_bssid (if present) at the best AP spot */
	if (memcmp(cfg->join_bssid.octet, BSSID_INVALID, sizeof(cfg->join_bssid.octet)) != 0) {
		for (i = best_ap_idx; i >= 0; i--) {
			if (memcmp(cfg->join_bssid.octet, bip[i]->BSSID.octet,
			           sizeof(bip[i]->BSSID.octet)) == 0) {
				tmp_bi = bip[i];
				for (k = i; k < best_ap_idx; k++)
				{
					bip[k] = bip[k+1];
				}
				bip[best_ap_idx] = tmp_bi;
				break;
			}
		}
		memcpy(cfg->join_bssid.octet, BSSID_INVALID, sizeof(cfg->join_bssid.octet));
	}

	/* At this point this is a sorted list of candidates that may include the curAP.
	* No pruning was done for WLC_E_REASON_BETTER_AP.
	* Driver should not reassociate to the same BSSID on the same channel it's already
	* connected to.  check if the best result is the same BSSID and same chanspec.
	* Nothing to be done if STA is not connected.
	*/
	if (for_roam && wlc_bss_connected(cfg) &&
		(roam->reason == WLC_E_REASON_BETTER_AP)) {
		for (i = best_ap_idx; i >= 0; i--) {
			/* Test to see if the candidate is the curAP */
			if (!memcmp(cfg->current_bss->BSSID.octet, bip[i]->BSSID.octet,
				ETHER_ADDR_LEN) && (wf_chspec_ctlchan(cfg->current_bss->chanspec) ==
				wf_chspec_ctlchan(bip[i]->chanspec))) {
				wlc->as->cmn->join_targets_last = 0;
				/* If the current AP is the best AP, end roam efforts */
				if (i == best_ap_idx) {
					WL_ASSOC(("wl%d.%d:%s(): Current BSSID is the best AP,"
						" nothing more to do\n",
						wlc->pub->unit, cfg->_idx, __FUNCTION__));
				} else {
					/* Cur AP is not the best AP, swap it out and
					 * the other worse off APs from the candidate list and
					 * update join_targets_last for wlc_join_attempt().
					 * e.g. AP1 AP2 AP3 C AP4 AP5 --> AP4 AP5 AP3 C AP1 AP2
					 * join_targets_last = 2, so wlc_join_attempt()
					 * will consider AP4 and AP5, where C = cur AP
					 */
					for (k = i+1; k <= best_ap_idx; k++) {
						/* Need to do a swap to avoid memory leaks
						 *  and double free
						 */
						tmp_bi = bip[k-i-1];
						bip[k-i-1] = bip[k];
						bip[k] = tmp_bi;
						wlc->as->cmn->join_targets_last++;
					}
				}
				break;
			}
		}
	}
#endif /* OPPORTUNISTIC_ROAM */
} /* wlc_cook_join_targets */

/** use to reassoc to a BSSID on a particular channel list */
int
wlc_reassoc(wlc_bsscfg_t *cfg, wl_reassoc_params_t *reassoc_params)
{
	wlc_info_t *wlc = cfg->wlc;
	chanspec_t* chanspec_list = NULL;
	int channel_num = reassoc_params->chanspec_num;
	struct ether_addr *bssid = &(reassoc_params->bssid);
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_ASSOC */
	wlc_assoc_t *as = cfg->assoc;
	int ret;

	if (!BSSCFG_STA(cfg) || !cfg->BSS) {
		WL_ASSOC_ERROR(("wl%d: %s: bad argument STA %d BSS %d\n",
		          wlc->pub->unit, __FUNCTION__, BSSCFG_STA(cfg), cfg->BSS));
		return BCME_BADARG;
	}

	/* add the reassoc request in the assoc_req list
	 * if someone is already in the process of association
	 */
	if ((ret = wlc_mac_request_entry(wlc, cfg, WLC_ACTION_REASSOC, 0)) > 0) {
		WL_ASSOC(("wl%d.%d: REASSOC request queued at slot %d\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), ret));
		return BCME_OK;
	} else if (ret < 0) {
		WL_ASSOC_ERROR(("wl%d.%d: REASSOC request failed\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg)));
		return ret;
	}

	WL_ASSOC(("wl%d : %s: Attempting association to %s\n",
	          wlc->pub->unit, __FUNCTION__, bcm_ether_ntoa(bssid, eabuf)));

	/* fix dangling pointers */
	if (channel_num)
		chanspec_list = reassoc_params->chanspec_list;

	wlc_assoc_init(cfg, AS_ROAM);
	cfg->roam->reason = WLC_E_REASON_INITIAL_ASSOC;

	if (!bcmp(cfg->BSSID.octet, bssid->octet, ETHER_ADDR_LEN)) {
		/* Clear the current BSSID info to allow a roam-to-self */
		wlc_bss_clear_bssid(cfg);
	}

	/* Since doing a directed roam, use the cache if available */
	if (ASSOC_CACHE_ASSIST_ENAB(wlc))
		as->flags |= AS_F_CACHED_ROAM;

	return wlc_assoc_scan_prep(cfg, NULL, bssid, chanspec_list, channel_num);
} /* wlc_reassoc */

static int
wlc_assoc_scan_prep(wlc_bsscfg_t *cfg, wl_join_scan_params_t *scan_params,
	const struct ether_addr *bssid, const chanspec_t *chanspec_list, int channel_num)
{
	if (cfg->assoc->type != AS_ASSOCIATION) {
		wlc_bss_mac_event(cfg->wlc, cfg, WLC_E_ROAM_START, NULL, 0,
		                  cfg->roam->reason, 0, 0, 0);
	}

	return wlc_assoc_scan_start(cfg, scan_params, bssid, chanspec_list, channel_num);
}

static int
wlc_assoc_bsstype(wlc_bsscfg_t *cfg)
{
	if (cfg->assoc->type == AS_ASSOCIATION)
		return cfg->target_bss->bss_type;
	else
		return DOT11_BSSTYPE_INFRASTRUCTURE;
}

#ifdef RSDB_APSCAN
/* update the value of roam->prune_type on roamscan state. state transitions handled are
 * ROAM_PRUNE_NONE-->ROAM_PRUNE_APBAND_CHANNELS. If no channels can be enumerated in state
 * ROAM_PRUNE_APBAND_CHANNELS, the move to ROAM_PRUNE_NON_APBAND_CHANNELS.
 */
static int
wlc_assoc_pruned_roamscan_prep(wlc_bsscfg_t *cfg, chanspec_t *chanspec_list, int channel_num)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_roam_t *roam = cfg->roam;
	int ret = channel_num;

	if (!WLRCC_ENAB(wlc->pub) || roam->roam_scan_started == FALSE ||
		chanspec_list == NULL || !channel_num) {
		return ret;
	}

	if (roam->prune_type == ROAM_PRUNE_NONE) {
		roam->prune_type = ROAM_PRUNE_APBAND_CHANNELS;

		ret = wlc_scan_filter_channels(wlc->scan, chanspec_list, channel_num);

		if (ret == channel_num) {
			/* Feature not active or not required in the current scenario.
			 * Set the prune state back to NONE.
			 */
			roam->prune_type = ROAM_PRUNE_NONE;

		} else {
			/* Do nothing */
		}
	} else if (roam->prune_type == ROAM_PRUNE_NON_APBAND_CHANNELS) {
		/* Handling of the second pass when the non AP channels need to be scanned */
		ret = wlc_scan_filter_channels(wlc->scan, chanspec_list, channel_num);
		ASSERT(ret != channel_num);
	}
	return ret;
}

/* Handles prune scan states. state transition handled are
 * ROAM_PRUNE_APBAND_CHANNELS --> ROAM_PRUNE_NON_APBAND_CHANNELS -> ROAM_PRUNE_NONE
 * The function checks for valid channels in that state and proceed if channels got enumerated.
 * return type:(int)
 * BCME_BUSY: When next scan is already scheduled.
 * BCME_OK: Done with prune scan or its not enalbed, Let the caller handle next full scan.
 */
static int
wlc_assoc_handle_prune_scanstate(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_roam_t *roam = cfg->roam;
	int err = BCME_OK;

	if (roam->prune_type == ROAM_PRUNE_APBAND_CHANNELS) {
		/* Check if there is any channel that was left out in the first pass,
		 * If so, then schedule the second pass of ROAM scan with the left out channels
		 */
		chanspec_t *chanspec_pruned = (chanspec_t *)MALLOCZ(wlc->pub->osh,
			sizeof(*chanspec_pruned) * roam->n_rcc_channels);
		int channel_num_pruned = roam->n_rcc_channels;
		if (chanspec_pruned == NULL) {
			WL_ERROR((WLC_BSS_MALLOC_ERR, WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg),
				__FUNCTION__, (int)roam->n_rcc_channels, MALLOCED(wlc->pub->osh)));
			err = BCME_NOMEM;
			goto fail;
		}
		memcpy(chanspec_pruned, roam->rcc_channels,
			roam->n_rcc_channels * sizeof(*chanspec_pruned));
		channel_num_pruned = wlc_scan_filter_channels(wlc->scan,
			chanspec_pruned, roam->n_rcc_channels);
		if (channel_num_pruned == 0) {
			/* all the channels are scanned in the previous pass itself */
			roam->prune_type = ROAM_PRUNE_NONE;
		} else {
			WL_ROAM(("wl%d.%d %s second roam scan pass for pruned scan."
				" ROAM_PRUNE_NON_APBAND_CHANNELS reason %d\n",
				wlc->pub->unit, cfg->_idx, __FUNCTION__,
				roam->reason));

			roam->scan_block = 0;
			roam->prune_type = ROAM_PRUNE_NON_APBAND_CHANNELS;
			wlc_join_done_int(cfg, WLC_E_STATUS_NO_NETWORKS);
			wlc_roamscan_start(cfg, roam->reason);
			err = BCME_BUSY;
		}
		MFREE(wlc->osh, chanspec_pruned, sizeof(*chanspec_pruned) * roam->n_rcc_channels);
	} else if (roam->prune_type == ROAM_PRUNE_NON_APBAND_CHANNELS) {
		roam->prune_type = ROAM_PRUNE_NONE;
	}

fail:
	return err;
}
#endif /* RSDB_APSCAN */

/* kick off assoc scan process. finish the join process if there's any error. */
/** chanspec_list being NULL/channel_num being 0 means all available chanspecs */
static int
wlc_assoc_scan_start(wlc_bsscfg_t *cfg, wl_join_scan_params_t *scan_params,
	const struct ether_addr *bssid, const chanspec_t *chanspec_list, int channel_num)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	wlc_roam_t *roam = cfg->roam;
	wlc_bss_info_t *target_bss = cfg->target_bss;
	int bss_type = wlc_assoc_bsstype(cfg), idx;
	int err = BCME_ERROR;
	bool assoc = (as->type == AS_ASSOCIATION);
	wlc_ssid_t ssid;
	chanspec_t* chanspecs = NULL;
	uint chanspecs_size = 0;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char *ssidbuf;
#endif /* BCMDBG || WLMSG_ASSOC */
	chanspec_t *target_list = NULL;
	uint target_num = 0;
	int active_time = -1;
	int nprobes = -1;
	uint chn_cache_count = bcm_bitcount(roam->roam_chn_cache.vec, sizeof(chanvec_t));
	uint scan_flag = 0;

	/* specific bssid is optional */
	if (bssid == NULL)
		bssid = &ether_bcast;
	else {
		wl_assoc_params_t *assoc_params = wlc_bsscfg_assoc_params(cfg);
		if (assoc_params && assoc_params->bssid_cnt) {
			/* When bssid list is specified, increase the default dwell times */
			nprobes = WLC_BSSID_SCAN_NPROBES;
			active_time = WLC_BSSID_SCAN_ACTIVE_TIME;
		}
	}

#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	ssidbuf = (char *)MALLOCZ(wlc->osh, SSID_FMT_BUF_LEN);
	if (ssidbuf == NULL) {
		WL_ERROR((WLC_BSS_MALLOC_ERR, WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg),
			__FUNCTION__, SSID_FMT_BUF_LEN, MALLOCED(wlc->osh)));
		err = BCME_NOMEM;
		goto done;
	}
#endif /* BCMDBG || WLMSG_ASSOC */

	if (assoc) {
		/* standard association */
		ssid.SSID_len = target_bss->SSID_len;
		bcopy(target_bss->SSID, ssid.SSID, ssid.SSID_len);

		WL_SRSCAN(("starting assoc scan"));
		WL_ASSOC(("starting assoc scan\n"));
	} else {
		wlc_bss_info_t *current_bss = cfg->current_bss;
		bool partial_scan_ok;

		/* roaming */
		ssid.SSID_len = current_bss->SSID_len;
		bcopy(current_bss->SSID, ssid.SSID, ssid.SSID_len);

		/* Assume standard full roam. */
		WL_SRSCAN(("roam scan: reason=%d rssi=%d", roam->reason, cfg->link->rssi));
		WL_ASSOC(("roam scan: reason=%d rssi=%d\n", roam->reason, cfg->link->rssi));
		roam->roam_type = ROAM_FULL;

		/* Force full scan for any other roam reason but these */
		partial_scan_ok = ((roam->reason == WLC_E_REASON_LOW_RSSI) ||
			(roam->reason == WLC_E_REASON_BCNS_LOST) ||
			(roam->reason == WLC_E_REASON_MINTXRATE) ||
			(roam->reason == WLC_E_REASON_TXFAIL));

		/* Roam scan only on selected channels */
#ifdef WLRCC
		if (WLRCC_ENAB(wlc->pub) && roam->rcc_valid) {
			ASSERT(roam->n_rcc_channels);
			/* Scan list will be overwritten later */
			WL_SRSCAN(("starting RCC directed scan"));
			WL_ASSOC(("starting RCC directed scan\n"));
		} else
#endif /* WLRCC */
		if (channel_num != 0) {
			ASSERT(chanspec_list);
			WL_SRSCAN(("starting user directed scan"));
			WL_ASSOC(("starting user directed scan\n"));
		} else if (partial_scan_ok && roam->cache_valid && roam->cache_numentries > 0) {
			uint i, chidx = 0;

			roam->roam_type = ROAM_PARTIAL;

			chanspecs_size = roam->cache_numentries * sizeof(chanspec_t);
			chanspecs = (chanspec_t *)MALLOCZ(wlc->pub->osh, chanspecs_size);
			if (chanspecs == NULL) {
				WL_ERROR((WLC_BSS_MALLOC_ERR, WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg),
				__FUNCTION__, (int)chanspecs_size, MALLOCED(wlc->pub->osh)));
				err = BCME_NOMEM;
				goto done;
			}

			WL_SRSCAN(("starting partial scan"));
			WL_ASSOC(("starting partial scan\n"));

			/* We have exactly one AP in our roam candidate list, so scan it
			 * whether or not we are blocked from associated to it due to a prior
			 * roam event
			 */
			if (roam->cache_numentries == 1) {
				WL_SRSCAN(("  channel %d",
					wf_chspec_ctlchan(roam->cached_ap[0].chanspec)));
				WL_ASSOC(("  channel %d\n",
					wf_chspec_ctlchan(roam->cached_ap[0].chanspec)));
				chanspecs[0] = roam->cached_ap[0].chanspec;
				chidx = 1;
			}
			/* can have multiple entries on one channel */
			else {
				for (idx = 0; idx < (int)roam->cache_numentries; idx++) {
					/* If not valid, don't add it to the chanspecs to scan */
					if (roam->cached_ap[idx].time_left_to_next_assoc)
						continue;

					/* trim multiple APs on the same channel */
					for (i = 0; chidx && (i < chidx); i++) {
						if (chanspecs[i] == roam->cached_ap[idx].chanspec)
							break;
					}

					if (i == chidx) {
						WL_SRSCAN(("  channel %d",
							wf_chspec_ctlchan(roam->
								cached_ap[idx].chanspec)));
						WL_ASSOC(("  channel %d\n",
							wf_chspec_ctlchan(roam->
								cached_ap[idx].chanspec)));
						chanspecs[chidx++] = roam->cached_ap[idx].chanspec;
					}
				}
			}
			chanspec_list = chanspecs;
			channel_num = chidx;

			WL_ASSOC(("wl%d: SCAN: using the cached scan results list (%d channels)\n",
			          WLCWLUNIT(wlc), channel_num));
		}
		/* Split roam scan.
		 * Bypass this algorithm for certain roam scan, ex. reassoc
		 * (using partial_scan_ok condition)
		 */
		else if (partial_scan_ok && roam->split_roam_scan) {
			uint8 chn_list;
			uint32 i, j, count, hot_count;

			roam->roam_type = ROAM_SPLIT_PHASE;

			/* Invalidate the roam cache so that we don't somehow transition
			 * to partial roam scans before all phases complete.
			 */
			roam->cache_valid = FALSE;

			/* Limit to max value */
			if (roam->roam_chn_cache_locked)
				chn_cache_count = MIN(roam->roam_chn_cache_limit, chn_cache_count);

			/* Allocate the chanspec list. */
			chanspecs_size = NBBY * sizeof(chanvec_t) * sizeof(chanspec_t);
			chanspecs = (chanspec_t *)MALLOCZ(wlc->pub->osh, chanspecs_size);
			if (chanspecs == NULL) {
				WL_ERROR((WLC_BSS_MALLOC_ERR, WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg),
				__FUNCTION__, (int)chanspecs_size, MALLOCED(wlc->pub->osh)));
				err = BCME_NOMEM;
				goto done;
			}

			WL_SRSCAN(("starting split scan: phase %d",
				roam->split_roam_phase + 1));
			WL_ASSOC(("starting split scan: phase %d\n",
				roam->split_roam_phase + 1));

			/* Always include home channel as the first to be scanned */
			count = 0;
			chanspecs[count++] = CH20MHZ_CHSPEC(
				wf_chspec_ctlchan(cfg->current_bss->chanspec));
			hot_count = 1;

			/* Fill in the chanspec list. */
			for (i = 0; i < sizeof(chanvec_t); i++) {
				chn_list = roam->roam_chn_cache.vec[i];
				idx = i << 3;
				for (j = 0; j < NBBY; j++, idx++, chn_list >>= 1) {
					if (!wlc_valid_chanspec_db(wlc->cmi,
						CH20MHZ_CHSPEC(idx)))
						continue;

					/* Home channel is already included */
					if (chanspecs[0] == CH20MHZ_CHSPEC(idx))
						continue;

					if ((chn_list & 0x1) &&
					    (hot_count <= chn_cache_count)) {
						hot_count++;

						/* Scan hot channels in phase 1 */
						if (roam->split_roam_phase == 0) {
							chanspecs[count++] =
								CH20MHZ_CHSPEC(idx);
							WL_SRSCAN(("  channel %d", idx));
							WL_ASSOC(("  channel %d\n", idx));
						}
					} else if (roam->split_roam_phase != 0) {
						/* Scan all other channels in phase 2 */
						chanspecs[count++] = CH20MHZ_CHSPEC(idx);
						WL_SRSCAN(("  channel %d", idx));
						WL_ASSOC(("  channel %d\n", idx));
					}
				}
			}
			chanspec_list = chanspecs;
			channel_num = count;

			/* Increment the split roam scan phase counter (cyclic). */
			roam->split_roam_phase =
				(roam->split_roam_phase == 0) ? 1 : 0;
		} else {
			WL_SRSCAN(("starting full scan"));
			WL_ASSOC(("starting full scan\n"));
		}
	}

#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	wlc_format_ssid(ssidbuf, ssid.SSID, ssid.SSID_len);
	WL_ASSOC(("wl%d: SCAN: wlc_assoc_scan_start, starting a %s%s scan for SSID %s\n",
		WLCWLUNIT(wlc), !ETHER_ISMULTI(bssid) ? "Directed " : "", assoc ? "JOIN" : "ROAM",
		ssidbuf));
#endif // endif

#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	/* DOT11_MAX_SSID_LEN check added so that we do not create ext logs for bogus
	 * joins of garbage ssid issued on XP
	 */
	if (assoc && (ssid.SSID_len != DOT11_MAX_SSID_LEN)) {
		wlc_format_ssid(ssidbuf, ssid.SSID, ssid.SSID_len);
	}
#endif /* BCMDBG || WLMSG_ASSOC */

	/* if driver is down due to mpc, turn it on, otherwise abort */
	wlc->mpc_scan = TRUE;
	wlc_radio_mpc_upd(wlc);

	/* Re-init channels and locale to Country default, host programmed
	 * country code
	 */
	if (WLC_CNTRY_DEFAULT_ENAB(wlc)) {
		WL_ASSOC(("wl%d:%s(): Initialize country setting to host country code\n",
		wlc->pub->unit, __FUNCTION__));
		wlc_cntry_use_default(wlc->cntry);
	}

	if (!wlc->pub->up) {
		WL_ASSOC(("wl%d: wlc_assoc_scan_start, can't scan while driver is down\n",
		          wlc->pub->unit));
		goto stop;
	}

#ifdef WLMCHAN
	/* XXX mChannel: Go wherever we are asked to if we are the only one
	 * associating/reassociating, otherwise limit it to the wlc->home_chanspec
	 * until multiple channel is supported.
	 */
	if (MCHAN_ENAB(wlc->pub)) {
		;	/* empty */
	} else
#endif /* WLMCHAN */
	if (wlc->stas_associated > 0 &&
	    (wlc->stas_associated > 1 || !cfg->associated)) {
		uint32 ctlchan, home_ctlchan;

		home_ctlchan = wf_chspec_ctlchan(wlc->home_chanspec);
		for (idx = 0; idx < channel_num; idx ++) {
			ctlchan = wf_chspec_ctlchan(chanspec_list[idx]);
			if (CH20MHZ_CHSPEC(ctlchan) ==
				CH20MHZ_CHSPEC(home_ctlchan))
				break;
		}
		if (channel_num == 0 || idx < channel_num) {
			WL_ASSOC(("wl%d: wlc_assoc_scan_start: use shared chanspec "
			          "wlc->home_chanspec 0x%x\n",
			          wlc->pub->unit, wlc->home_chanspec));
			chanspec_list = &wlc->home_chanspec;
			channel_num = 1;
		} else {
			WL_ASSOC(("wl%d: wlc_assoc_scan_start, no chanspec\n",
			          wlc->pub->unit));
			goto stop;
		}
	}

	/* clear scan_results in case there are some left over from a prev scan */
	wlc_bss_list_free(wlc, wlc->scan_results);

	wlc_set_mac(cfg);

#ifdef WLP2P
	/* init BSS block and ADDR_BMP entry to allow ucode to follow
	 * the necessary chains of states in the transmit direction
	 * prior to association.
	 */
	if (BSS_P2P_ENAB(wlc, cfg))
		wlc_p2p_prep_bss(wlc->p2p, cfg);
#endif // endif

	/* If association scancache use is enabled, check for a hit in the scancache.
	 * Do not check the cache if we are re-running the assoc scan after a cached
	 * assoc failure
	 */
	if (ASSOC_CACHE_ASSIST_ENAB(wlc) &&
	    (assoc || (as->flags & AS_F_CACHED_ROAM)) &&
	    !(as->flags & (AS_F_CACHED_CHANNELS | AS_F_CACHED_RESULTS))) {
		wlc_assoc_cache_eval(wlc, bssid, &ssid, bss_type,
		                     chanspec_list, channel_num,
		                     wlc->scan_results, &target_list, &target_num);
	}

	/* reset the assoc cache flags since this may be a retry of a cached attempt */
	as->flags &= ~(AS_F_CACHED_CHANNELS | AS_F_CACHED_RESULTS);

	/* narrow down the channel list if the cache eval came up with a short list */
	if (ASSOC_CACHE_ASSIST_ENAB(wlc) && target_num > 0) {
		WL_ASSOC(("wl%d: JOIN: using the cached scan results "
		          "to create a channel list len %u\n",
		          WLCWLUNIT(wlc), target_num));
		as->flags |= AS_F_CACHED_CHANNELS;
		chanspec_list = target_list;
		channel_num = target_num;
	}

	/* Use cached results if there was a hit instead of performing a scan */
	if (ASSOC_CACHE_ASSIST_ENAB(wlc) && wlc->scan_results->count > 0) {
		WL_ASSOC(("wl%d: JOIN: using the cached scan results for assoc (%d hits)\n",
		          WLCWLUNIT(wlc), wlc->scan_results->count));

		as->flags |= AS_F_CACHED_RESULTS;

		wlc_assoc_change_state(cfg, AS_SCAN);
		wlc_bss_assoc_state_notif(wlc, cfg, as->type, AS_SCAN);
		wlc_assoc_scan_complete(wlc, WLC_E_STATUS_SUCCESS, cfg);

		err = BCME_OK;
	} else {
		bool scan_suppress_ssid = FALSE;
		int passive_time = -1;
		int home_time = -1;
		int scan_type = 0;
		int away_ch_limit = 0;
		struct ether_addr *sa_override = NULL;
		chanspec_t chanspec_pruned[MAXCHANNEL];
		memset(chanspec_pruned, 0, sizeof(*chanspec_pruned) * MAXCHANNEL);

		/* override default scan params */
		if (scan_params != NULL) {
			scan_type = scan_params->scan_type;
			nprobes = scan_params->nprobes;
			active_time = scan_params->active_time;
			passive_time = scan_params->passive_time;
			home_time = scan_params->home_time;
		}

		if (WLRCC_ENAB(wlc->pub)) {
			if ((chanspec_list == NULL) && roam->rcc_valid && !roam->roam_fullscan) {
				chanspec_list = roam->rcc_channels;
				channel_num = roam->n_rcc_channels; /* considering to use 1 */
				roam->roam_scan_started = TRUE;
				WL_ROAM(("RCC: %d scan channels\n", channel_num));
			} else {
				WL_ROAM(("RCC: chanspec_list 0x%X, rcc valid %d, roam_fullscan %d, "
					"%d scan channels\n", (int)chanspec_list, roam->rcc_valid,
					roam->roam_fullscan, channel_num));
			}
			roam->roam_fullscan = FALSE;
		}

		if (channel_num) {
			WL_SRSCAN(("scan start: %d channel(s)", channel_num));
			WL_ASSOC(("scan start: %d channel(s)\n", channel_num));
		} else {
			WL_SRSCAN(("scan start: all channels"));
			WL_ASSOC(("scan start: all channels\n"));
		}

		/* active time for association recreation */
#ifdef NLO
		if (cfg->nlo && as->type == AS_RECREATE) {
			active_time	= WLC_NLO_SCAN_RECREATE_TIME;
		} else
#endif /* NLO */
		{
			if (as->flags & AS_F_SPEEDY_RECREATE)
				active_time = WLC_SCAN_RECREATE_TIME;
		}

		/* Extend home time for lazy roam scan
		 * Must be longer than pm2_sleep_ret_time and beacon period.
		 * Set it to 2*MAX(pm2_sleep_ret_time, 1.5*beaocn_period).
		 * Also, limit to 2 active scan per home_away_time
		 * (assume beacon period = 100, and scan_assoc_time=20).
		 */
		if ((roam->reason == WLC_E_REASON_LOW_RSSI) &&
			wlc_roam_scan_islazy(wlc, cfg, FALSE)) {
			home_time = 2 * MAX((int)cfg->pm->pm2_sleep_ret_time,
				(int)cfg->current_bss->beacon_period * 3 / 2);
			away_ch_limit = 2;
		}

		/* country_default feature: If this feature is enabled,
		* allow prohibited scanning.
		* This will allow the STA to be able to roam to those APs
		* that are operating on a channel that is prohibited
		* by the current country code.
		*/
		if (WLC_CNTRY_DEFAULT_ENAB(wlc)) {
			scan_flag |= WL_SCANFLAGS_PROHIBITED;
		}

		if (as->type == AS_ROAM) {
			/* override source MAC */
			sa_override = wlc_scanmac_get_mac(wlc->scan, WLC_ACTION_ROAM, cfg);
		}
#ifdef RSDB_APSCAN
		/* If pruned scan enabled only for roam scan */
		if (RSDB_APSCAN_ENAB(wlc->pub)) {
			/* Pruned scan is dependent on RCC in populating the channels for ROAM */
			if (WLRCC_ENAB(wlc->pub) && roam->roam_scan_started &&
				channel_num != 0 && chanspec_list != NULL) {
				int channel_num_pruned;
				memcpy(chanspec_pruned, chanspec_list,
					channel_num * sizeof(*chanspec_pruned));
				channel_num_pruned = wlc_assoc_pruned_roamscan_prep(cfg,
					chanspec_pruned, channel_num);
				if (channel_num_pruned != channel_num) {
					channel_num = channel_num_pruned;
					chanspec_list = chanspec_pruned;
				}
			}
		}
#endif /* RSDB_APSCAN */

#ifdef STA
		/* override default scan params
		 * Allow STA to scan longer on active channels to reassociate when switching
		 * home channel after AP detected radar.
		 * Multiple associated STAs may simultaneously try to reassociate
		 * at the csa count down competing for the medium.
		 */
		if (cfg->assoc->flags & AS_F_DO_CHANSWITCH) {
			cfg->assoc->flags &= ~AS_F_DO_CHANSWITCH;
			if ((chanspec_list[0] == wlc_get_home_chanspec(cfg)) &&
					(channel_num == 1)) {
				active_time = WLC_SCAN_DFS_CHSW_TIME;
				nprobes = WLC_SCAN_DFS_CHSW_NPROBES;
			} else {
				WL_ERROR(("wl%d: %s: AS_F_DO_CHANSWITCH is set unexpected\n",
					wlc->pub->unit, __FUNCTION__));
			}
		}
#endif /* STA */

		/* kick off a scan for the requested SSID, possibly broadcast SSID. */
		err = wlc_scan(wlc->scan, bss_type, bssid, 1, &ssid, scan_type, nprobes,
			active_time, passive_time, home_time, chanspec_list, channel_num, 0, TRUE,
			wlc_assoc_scan_complete, wlc, away_ch_limit, FALSE, scan_suppress_ssid,
			FALSE, scan_flag, cfg, SCAN_ENGINE_USAGE_NORM, NULL, NULL, sa_override);

		if (err == BCME_OK) {
			wlc_assoc_change_state(cfg, AS_SCAN);
			wlc_bss_assoc_state_notif(wlc, cfg, as->type, AS_SCAN);
		}

		/* when the scan is done, wlc_assoc_scan_complete() will be called to copy
		 * scan_results to join_targets and continue the association process
		 */
	}

	/* clean up short channel list if one was returned from wlc_assoc_cache_eval() */
	if (target_list != NULL)
		MFREE(wlc->osh, target_list, target_num * sizeof(chanspec_t));

stop:
	if (chanspecs != NULL)
		MFREE(wlc->pub->osh, chanspecs, chanspecs_size);

	wlc->mpc_scan = FALSE;
	wlc_radio_mpc_upd(wlc);

done:
	if (err != BCME_OK) {
		/* Reset the state to IDLE if down, no chanspec or unable to
		 * scan.
		 */
		wlc_assoc_change_state(cfg, AS_SCAN_ABORT);
		wlc_join_done_int(cfg, WLC_E_STATUS_FAIL);
	}
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	if (ssidbuf != NULL) {
		MFREE(wlc->osh, (void *)ssidbuf, SSID_FMT_BUF_LEN);
	}
#endif // endif
	return err;
} /* wlc_assoc_scan_start */

#ifdef WL_ASSOC_RECREATE
static void
wlc_speedy_recreate_fail(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;

	as->flags &= ~AS_F_SPEEDY_RECREATE;

	wlc_bss_list_free(wlc, wlc->as->cmn->join_targets);

	WL_ASSOC(("wl%d: %s: ROAM: Doing full scan since assoc-recreate failed\n",
	          WLCWLUNIT(wlc), __FUNCTION__));

#ifdef NLO
	/* event to get nlo assoc and scan params into bsscfg if nlo enabled */
	wlc_bss_mac_event(wlc, cfg, WLC_E_SPEEDY_RECREATE_FAIL, NULL, 0, 0, 0, 0, 0);
	if (cfg->nlo) {
		/* start assoc full scan using nlo parameters */
		wlc_assoc_scan_start(cfg, NULL, NULL,
			(cfg->assoc_params ? cfg->assoc_params->chanspec_list : NULL),
			(cfg->assoc_params ? cfg->assoc_params->chanspec_num : 0));
		wlc_bsscfg_assoc_params_reset(wlc, cfg);
	} else
#endif /* NLO */
	{
		wlc_assoc_scan_start(cfg, NULL, NULL, NULL, 0);
	}
}
#endif /* WL_ASSOC_RECREATE */

#ifdef WLSCANCACHE
/** returns TRUE if scan cache looks valid and recent, FALSE otherwise */
bool
wlc_assoc_cache_validate_timestamps(wlc_info_t *wlc, wlc_bss_list_t *bss_list)
{
	uint current_time = OSL_SYSUPTIME(), oldest_time, i;

	/* if there are no hits, just return with no cache assistance */
	if (bss_list->count == 0)
		return FALSE;

	/* if there are hits, check how old they are */
	oldest_time = current_time;
	for (i = 0; i < bss_list->count; i++)
		oldest_time = MIN(bss_list->ptrs[i]->timestamp, oldest_time);

	/* If the results are all recent enough, then use the cached bss_list
	 * for the association attempt.
	 */
	if (current_time - oldest_time < BCMWL_ASSOC_CACHE_TOLERANCE) {
		WL_ASSOC(("wl%d: %s: %d hits, oldest %d sec, using cache hits\n",
		          wlc->pub->unit, __FUNCTION__,
		          bss_list->count, (current_time - oldest_time)/1000));
		return TRUE;
	}

	return FALSE;
}

static void
wlc_assoc_cache_eval(wlc_info_t *wlc, const struct ether_addr *BSSID, const wlc_ssid_t *SSID,
                     int bss_type, const chanspec_t *chanspec_list, uint chanspec_num,
                     wlc_bss_list_t *bss_list, chanspec_t **target_list, uint *target_num)
{
	chanspec_t *target_chanspecs;
	uint target_chanspec_alloc_num;
	uint target_chanspec_num;
	uint i, j;
	osl_t *osh;

	osh = wlc->osh;

	*target_list = NULL;
	*target_num = 0;

	if (SCANCACHE_ENAB(wlc->scan))
		wlc_scan_get_cache(wlc->scan, BSSID, 1, SSID, bss_type,
			chanspec_list, chanspec_num, bss_list);

	BCM_REFERENCE(wlc);

	/* if there are no hits, just return with no cache assistance */
	if (bss_list->count == 0)
		return;

	if (wlc_assoc_cache_validate_timestamps(wlc, bss_list))
		return;

	WL_ASSOC(("wl%d: %s: %d hits, creating a channel list\n",
	          wlc->pub->unit, __FUNCTION__, bss_list->count));

	/* If the results are too old they might have stale information, so use a chanspec
	 * list instead to speed association.
	 */
	target_chanspec_num = 0;
	target_chanspec_alloc_num = bss_list->count;
	target_chanspecs = MALLOCZ(osh, sizeof(chanspec_t) * bss_list->count);
	if (target_chanspecs == NULL) {
		/* out of memory, skip cache assistance */
		WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__,
			(int)(sizeof(chanspec_t) * bss_list->count), MALLOCED(osh)));
		goto cleanup;
	}

	for (i = 0; i < bss_list->count; i++) {
		chanspec_t chanspec;
		uint8 ctl_ch;

		chanspec = bss_list->ptrs[i]->chanspec;

		/* convert a 40MHz or 80/160/8080Mhz chanspec to a 20MHz chanspec of the
		 * control channel since this is where we should scan for the BSS.
		 */
		if (CHSPEC_IS40(chanspec) || CHSPEC_IS80(chanspec) ||
			CHSPEC_IS160(chanspec) || CHSPEC_IS8080(chanspec)) {
			ctl_ch = wf_chspec_ctlchan(chanspec);
			chanspec = (chanspec_t)(ctl_ch | WL_CHANSPEC_BW_20 |
			                        CHSPEC_BAND(chanspec));
		}

		/* look for this bss's chanspec in the list we are building */
		for (j = 0; j < target_chanspec_num; j++)
			if (chanspec == target_chanspecs[j])
				break;

		/* if the chanspec is not already on the list, add it */
		if (j == target_chanspec_num)
			target_chanspecs[target_chanspec_num++] = chanspec;
	}

	/* Resize the chanspec list to exactly what it needed */
	if (target_chanspec_num != target_chanspec_alloc_num) {
		chanspec_t *new_list;

		new_list = MALLOCZ(osh, sizeof(chanspec_t) * target_chanspec_num);
		if (new_list != NULL) {
			memcpy(new_list, target_chanspecs,
			       sizeof(chanspec_t) * target_chanspec_num);
		} else {
			/* out of memory, skip cache assistance */
			WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__,
				(int)(sizeof(chanspec_t) * target_chanspec_num), MALLOCED(osh)));
			target_chanspec_num = 0;
		}

		MFREE(osh, target_chanspecs, sizeof(chanspec_t) * target_chanspec_alloc_num);
		target_chanspecs = new_list;
	}

	*target_list = target_chanspecs;
	*target_num = target_chanspec_num;

cleanup:
	/* clear stale scan_results */
	wlc_bss_list_free(wlc, wlc->scan_results);

	return;
} /* wlc_assoc_cache_eval */

/** If a cache assisted association attempt fails, retry with a regular assoc scan */
static void
wlc_assoc_cache_fail(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wl_join_scan_params_t *scan_params;
	const wl_join_assoc_params_t *assoc_params;
	const struct ether_addr *bssid = NULL;
	const chanspec_t *chanspec_list = NULL;
	int chanspec_num = 0;

	WL_ASSOC(("wl%d: %s: Association from cache failed, starting regular association scan\n",
	          WLCWLUNIT(wlc), __FUNCTION__));

	/* reset join_targets for new join attempt */
	wlc_bss_list_free(wlc, wlc->as->cmn->join_targets);

	scan_params = wlc_bsscfg_scan_params(cfg);
	if ((assoc_params = wlc_bsscfg_assoc_params(cfg)) != NULL) {
		bssid = &assoc_params->bssid;
		chanspec_list = assoc_params->chanspec_list;
		chanspec_num = assoc_params->chanspec_num;
	}

	wlc_assoc_scan_start(cfg, scan_params, bssid, chanspec_list, chanspec_num);
}

#endif /* WLSCANCACHE */

void
wlc_pmkid_build_cand_list(wlc_bsscfg_t *cfg, bool check_SSID)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_bss_info_t *target_bss = cfg->target_bss;
	uint i;
	wlc_bss_info_t *bi;

	WL_WSEC(("wl%d: building PMKID candidate list\n", wlc->pub->unit));

	/* Merge scan results and pmkid cand list */
	for (i = 0; i < wlc->scan_results->count; i++) {
#if defined(BCMDBG) || defined(WLMSG_WSEC)
		char eabuf[ETHER_ADDR_STR_LEN];
#endif	/* BCMDBG || WLMSG_WSEC */

		bi = wlc->scan_results->ptrs[i];

		/* right network? if not, move on */
		if (check_SSID &&
			((bi->SSID_len != target_bss->SSID_len) ||
			bcmp(bi->SSID, target_bss->SSID, bi->SSID_len)))
			continue;

		WL_WSEC(("wl%d: wlc_pmkid_build_cand_list(): processing %s...",
			wlc->pub->unit, bcm_ether_ntoa(&bi->BSSID, eabuf)));

		wlc_pmkid_prep_list(wlc->pmkid_info, cfg, &bi->BSSID, bi->wpa2.flags);
	}

	/* if port's open, request PMKID cache plumb */
	ASSERT(cfg->WPA_auth & WPA2_AUTH_UNSPECIFIED);
	if (cfg->wsec_portopen) {
		WL_WSEC(("wl%d: wlc_pmkid_build_cand_list(): requesting PMKID cache plumb...\n",
			wlc->pub->unit));
		wlc_pmkid_cache_req(wlc->pmkid_info, cfg);
	}
}

/* a simple wlc_join_done() routine specifically for slotted bss */
static void
wlc_join_done_slotted_bss(wlc_bsscfg_t *cfg, struct ether_addr *bssid, int bss_type, int status)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	uint type = as->type;

	wlc_set_ssid_complete(cfg, status, bssid, bss_type);

	/* Association state machine is halting, clear state and allow core to sleep */
	wlc_assoc_change_state(cfg, AS_IDLE);
	as->type = AS_NONE;
	wlc_bss_assoc_state_notif(wlc, cfg, type, AS_IDLE);
}

/**
 * Assoc/roam completion routine - must be called at the end of an assoc/roam process
 * no matter it finishes with success or failure or it doesn't finish due to abort.
 * Same as wlc_join_done() but internally used.
 */
static void
wlc_join_done_int(wlc_bsscfg_t *cfg, int status)
{
	wlc_join_done(cfg, &cfg->BSSID, cfg->target_bss->bss_type, status);
}

/**
 * Assoc/roam completion routine - must be called at the end of an assoc/roam process
 * no matter it finishes with success or failure or it doesn't finish due to abort.
 */
/* externally used - specify BSSID and bss_type */
void
wlc_join_done(wlc_bsscfg_t *cfg, struct ether_addr *bssid, int bss_type, int status)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_roam_t *roam = cfg->roam;
	wlc_assoc_t *as = cfg->assoc;
	uint type = as->type;
	assoc_cfg_cubby_t *as_cubby;
	int ret;

	/* Protection from second call from, e.g., wlc_assoc_abort() */
	if (as->state == AS_IDLE) {
		return;
	}

	/* special bsscfg type */
	if (BSSCFG_SLOTTED_BSS(cfg)) {
		wlc_join_done_slotted_bss(cfg, bssid, bss_type, status);
		return;
	}

	/* announcing we're finishing the join process */
	as_cubby = BSSCFG_ASSOC_CUBBY(wlc->as, cfg);
	if (as_cubby->stflags & AS_ST_FLAG_DONE_PROC) {
		/* bail out if we're recursively called */
		WL_RECURSIVE(("wl%d.%d: %s: already in progress...\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
		return;
	}
	as_cubby->stflags |= AS_ST_FLAG_DONE_PROC;

	/* Association state machine is halting, clear state */
	wlc_bss_list_free(wlc, wlc->as->cmn->join_targets);

	/* Stop any timer */
	wlc_assoc_timer_del(wlc, cfg);

#ifdef WLMCNX
	/* if (as->state == AS_WAIT_RCV_BCN) */
	/* resume TSF adjustment */
	if (MCNX_ENAB(wlc->pub) && cfg->BSS) {
		wlc_skip_adjtsf(wlc, FALSE, cfg, -1, WLC_BAND_ALL);
	}
#endif /* WLMCNX */

	if (type == AS_ASSOCIATION) {
		ret = wlc_set_ssid_complete(cfg, status, bssid, bss_type);
	} else {
		if ((roam->roam_type == ROAM_SPLIT_PHASE) &&
		    (status == WLC_E_STATUS_NO_NETWORKS)) {
			/* Start the next phase of the split roam scan.
			 * (roam scan phase is incremented upon scan start)
			 */
			if (roam->split_roam_phase) {
				WL_SRSCAN(("assoc done: status %d", status));
				WL_ASSOC(("assoc done: status %d\n", status));
				as_cubby->stflags &= ~AS_ST_FLAG_DONE_PROC;
				wlc_assoc_scan_prep(cfg, NULL, NULL, NULL, 0);
				return;
			}
		}
		ret = wlc_roam_complete(cfg, status, bssid, bss_type);
	}

	/* Why is it only resetting cached_ap[0]? */
	if (status == WLC_E_STATUS_SUCCESS) {
		cfg->roam->cached_ap[0].fullscan_cnt = 0;
		cfg->roam->cached_ap[0].rssi_st.roam_st.reason = 0;
		cfg->roam->cached_ap[0].rssi_st.roam_st.status = 0;
		cfg->roam->cached_ap[0].time = 0;
	}

	wlc_join_attempt_info_end(cfg, status);
	wlc_join_attempt_info_update_tot_time(cfg);

	/* Unregister assoc channel timeslot */
	if (as->req_msch_hdl) {
		/* TODO add new state to assoc state machine so assoc_done happens only
		 * after channel schduler both flex timeslot starts
		 */
		wlc_txqueue_end(wlc, cfg, NULL);
		wlc_msch_timeslot_unregister(wlc->msch_info, &as->req_msch_hdl);
		as->req_msch_hdl = NULL;
	}

	if (status == WLC_E_STATUS_TIMEOUT && !cfg->associated) {
		wlc_sta_timeslot_unregister(cfg);
	}

	/* assoc success, check if channel timeslot registered */
	if (status == WLC_E_STATUS_SUCCESS && !wlc_sta_timeslot_registed(cfg)) {
		WL_ASSOC_ERROR(("wl%d: %s: channel timeslot for connection not registered\n",
			WLCWLUNIT(wlc), __FUNCTION__));
		ASSERT(0);
	}

	/* announcing we've finished the join process */
	as_cubby->stflags &= ~AS_ST_FLAG_DONE_PROC;

	/* We're done join/roam so let's look at the system... */
	if (ret != JOIN_CMPLT_DONE && ret != ROAM_CMPLT_DONE) {
		WL_ASSOC(("wl%d: %s: %s early exit with status %d and return %d\n",
		          wlc->pub->unit, __FUNCTION__, WLCASTYPEN(as->type),
		          status, ret));
		return;
	}

	/* Association state machine is halting, clear state and allow core to sleep */
	wlc_assoc_change_state(cfg, AS_IDLE);
	as->type = AS_NONE;
	wlc_bss_assoc_state_notif(wlc, cfg, type, AS_IDLE);

#ifdef OPPORTUNISTIC_ROAM
	memcpy(cfg->join_bssid.octet, BSSID_INVALID, sizeof(cfg->join_bssid.octet));
#endif /* OPPORTUNISTIC_ROAM */

#ifdef WLPFN
	if (WLPFN_ENAB(wlc->pub)) {
		wl_notify_pfn(wlc);
	}
#endif /* WLPFN */

	/* APSTA: complete any deferred AP bringup */
	if (AP_ENAB(wlc->pub) && APSTA_ENAB(wlc->pub)) {
		wlc_restart_ap(wlc->ap);
	}

	/* allow AP to beacon and respond to probe requests */
	if (AP_ACTIVE(wlc)) {
		/* validate the phytxctl for the beacon before turning it on */
		wlc_validate_bcn_phytxctl(wlc, NULL);
	}

	wlc_ap_mute(wlc, FALSE, cfg, -1);
}

/* assoc scan completion callback */
static void
wlc_assoc_scan_complete(void *arg, int status, wlc_bsscfg_t *cfg)
{
	int idx;
	wlc_bsscfg_t *valid_cfg;
	bool cfg_ok = FALSE;
	wlc_info_t *wlc;
	wlc_assoc_t *as;
	wlc_roam_t *roam;
	wlc_bss_info_t *target_bss;
	bool for_roam;
	uint8 ap_24G = 0;
	uint8 ap_5G = 0;
	uint i;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char *ssidbuf = NULL;
	const char *ssidstr;
	const char *msg_name;
	const char *msg_pref;
#endif /* BCMDBG || WLMSG_ASSOC */
#ifdef WLRCC
	bool cache_valid = cfg->roam->cache_valid;
	bool roam_active = cfg->roam->active;
	bool force_rescan = FALSE;
#endif /* WLRCC */

	/* We have seen instances of where this function was called on a cfg that was freed.
	 * Verify cfg first before proceeding.
	 */
	wlc = (wlc_info_t *)arg;
	/* Must find a match in global bsscfg array before continuing */
	FOREACH_BSS(wlc, idx, valid_cfg) {
		if (valid_cfg == cfg) {
			cfg_ok = TRUE;
			break;
		}
	}
	if (!cfg_ok) {
		WL_ASSOC(("wl%d: %s: no valid bsscfg matches cfg %p, exit\n",
		          WLCWLUNIT(wlc), __FUNCTION__, OSL_OBFUSCATE_BUF(cfg)));
		goto exit;
	}

#ifdef STA
	/* reset AS_F_DO_CHANSWITCH it is not expected to be set beyond this point */
	cfg->assoc->flags &= ~AS_F_DO_CHANSWITCH;
#endif /* STA */

	/* cfg has been validated, continue with rest of function */
	as = cfg->assoc;
	roam = cfg->roam;
	target_bss = cfg->target_bss;
	for_roam = (as->type != AS_ASSOCIATION);
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	msg_name = for_roam ? "ROAM" : "JOIN";
	msg_pref = !(ETHER_ISMULTI(&wlc->scan->bssid)) ? "Directed " : "";
	ssidbuf = (char *)MALLOCZ(wlc->osh, SSID_FMT_BUF_LEN);
	if (ssidbuf) {
		wlc_format_ssid(ssidbuf, target_bss->SSID, target_bss->SSID_len);
		ssidstr = ssidbuf;
	} else {
		ssidstr = "???";
		WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__, (int)SSID_FMT_BUF_LEN,
			MALLOCED(wlc->osh)));
	}
#endif /* BCMDBG || WLMSG_ASSOC */

	WL_ASSOC(("wl%d: SCAN: wlc_assoc_scan_complete\n", WLCWLUNIT(wlc)));

#ifdef WL_EVENT_LOG_COMPILE
	{
	wl_event_log_tlv_hdr_t tlv_log = {{0, 0}};

	tlv_log.tag = TRACE_TAG_STATUS;
	tlv_log.length = sizeof(uint);

	WL_EVENT_LOG((EVENT_LOG_TAG_TRACE_WL_INFO,
	              for_roam ? TRACE_ROAM_SCAN_COMPLETE : WLC_E_SCAN_COMPLETE,
	              tlv_log.t, status));
	}
#endif /* WL_EVENT_LOG_COMPILE */

	/* Delay roam scan by scan_settle_time watchdog tick(s) to allow
	 * other events like calibrations to run
	 */
	if (WLFMC_ENAB(wlc->pub) && roam->active)
		roam->scan_block = MAX(roam->scan_block, roam->partialscan_period);
	else
		roam->scan_block = MAX(roam->scan_block, WLC_ROAM_SCAN_PERIOD);

	if (status == WLC_E_STATUS_SUCCESS && roam->reassoc_param != NULL) {
		MFREE(wlc->osh, roam->reassoc_param, roam->reassoc_param_len);
		roam->reassoc_param = NULL;
	}

	if (status != WLC_E_STATUS_SUCCESS) {

		/* If roam scan is aborted, reset split roam scans.
		 * Do not clear the scan block to avoid continuous scan.
		 */
		if (for_roam && status == WLC_E_STATUS_ABORT) {
			roam->split_roam_phase = 0;
		}

		if (target_bss->bss_type == DOT11_BSSTYPE_INFRASTRUCTURE ||
		    status != WLC_E_STATUS_SUPPRESS) {
			if (status != WLC_E_STATUS_ABORT) {
				wlc_join_done_int(cfg, status);
			}

			/* Exit association in progress, and let scan state machine restart the
			 * join process later.
			 */
			if ((status == WLC_E_STATUS_ABORT) && (as->type == AS_ASSOCIATION)) {
				WL_ASSOC_ERROR(("wl%d: Aborting in progress association, error in"
					"scan \n", wlc->pub->unit));

				wlc_assoc_abort(cfg);
			}

			goto exit;
		}
	}

	/* register scan results as possible PMKID candidates */
	if ((cfg->WPA_auth & WPA2_AUTH_UNSPECIFIED) &&
#ifdef WLFBT
	    (!BSSCFG_IS_FBT(cfg) || !(cfg->WPA_auth & WPA2_AUTH_FT)) &&
#endif /* WLFBT */
	    TRUE) {
		wlc_pmkid_build_cand_list(cfg, FALSE);
	}

	/* copy scan results to join_targets for the reassociation process */
	if (WLCAUTOWPA(cfg)) {
		if (wlc_bss_list_expand(cfg, wlc->scan_results, wlc->as->cmn->join_targets)) {
			WL_ASSOC_ERROR(("wl%d: wlc_bss_list_expand failed\n", WLCWLUNIT(wlc)));
			wlc_join_done_int(cfg, WLC_E_STATUS_FAIL);
			goto exit;
		}
	} else {
		wlc_bss_list_xfer(wlc->scan_results, wlc->as->cmn->join_targets);
	}

	wlc->as->cmn->join_targets_last = wlc->as->cmn->join_targets->count;

	if (wlc->as->cmn->join_targets->count > 0) {
		WL_ASSOC(("wl%d: SCAN for %s%s: SSID scan complete, %d results for \"%s\"\n",
			WLCWLUNIT(wlc), msg_pref, msg_name,
			wlc->as->cmn->join_targets->count, ssidstr));
	} else if (target_bss->SSID_len > 0) {
		WL_ASSOC(("wl%d: SCAN for %s%s: SSID scan complete, no matching SSIDs found "
			"for \"%s\"\n", WLCWLUNIT(wlc), msg_pref, msg_name, ssidstr));
	} else {
		WL_ASSOC(("wl%d: SCAN for %s: SSID scan complete, no SSIDs found for broadcast "
			"scan\n", WLCWLUNIT(wlc), msg_name));
	}

	/* sort join targets by signal strength */
	/* for roam, prune targets if they do not have signals stronger than our current AP */
	if (wlc->as->cmn->join_targets->count > 0) {
		wlc_bss_info_t **bip = wlc->as->cmn->join_targets->ptrs;

		if (!cfg->associated || !for_roam) {
			/* New Assoc, Clear cached channels */
			memset(roam->roam_chn_cache.vec, 0, sizeof(chanvec_t));
			roam->roam_chn_cache_locked = FALSE;

			/* wipe roam cache clean */
			roam->cache_valid = FALSE;
			roam->cache_numentries = 0;
		}

		/* Identify Multi AP Environment */
		for (i = 0; i < wlc->as->cmn->join_targets->count; i++) {
			if (CHSPEC_IS5G(bip[i]->chanspec)) {
				ap_5G++;
			} else {
				ap_24G++;
			}

			/* Build the split roam scan channel cache. */
			if (!roam->roam_chn_cache_locked) {
				setbit(roam->roam_chn_cache.vec,
					wf_chspec_ctlchan(bip[i]->chanspec));
			}
		}

		if (ap_24G > 1 || ap_5G > 1) {
			WL_ASSOC(("wl%d: ROAM: Multi AP Environment \r\n", WLCWLUNIT(wlc)));
			roam->multi_ap = TRUE;
		} else if (!cfg->associated) {
			/* Sticky multi_ap flag:
			 * During association, the multi_ap can only be set, not cleared.
			 */
			WL_ASSOC(("wl%d: ROAM: Single AP Environment \r\n", WLCWLUNIT(wlc)));
			roam->multi_ap = FALSE;
		} else {
			WL_ASSOC(("wl%d: ROAM: Keep %s AP Environment \r\n", WLCWLUNIT(wlc),
				(roam->multi_ap ? "multi" : "single")));
		}

		/* Update roam profile for possible multi_ap state change */
		wlc_roam_prof_update_default(wlc, cfg);

		/* No pruning to be done if this is a directed, cache assisted ROAM */
		if (for_roam && !(as->flags & AS_F_CACHED_ROAM)) {
			wlc_cook_join_targets(cfg, TRUE, cfg->link->rssi);
			WL_ASSOC(("wl%d: ROAM: %d roam target%s after pruning\n",
				WLCWLUNIT(wlc), wlc->as->cmn->join_targets_last,
				(wlc->as->cmn->join_targets_last == 1) ? "" : "s"));
		} else {
			wlc_cook_join_targets(cfg, FALSE, 0);
		}
		/* no results */
	} else if (for_roam && roam->cache_valid && roam->cache_numentries > 0) {
		/* We need to blow away our cache if we were roaming for entries that ended
		 * not existing. Maybe our AP changed channels?
		 */
		WL_ASSOC(("wl%d: %s: Forcing a new roam scan becasue we found no APs "
		          "from our partial scan results list\n",
		          wlc->pub->unit, __FUNCTION__));
		roam->cache_valid = FALSE;
		roam->active = FALSE;
		roam->scan_block = 0;
	}
#ifdef WLRCC
	if (WLRCC_ENAB(wlc->pub)) {
		if (roam->roam_scan_started) {
#ifdef RSDB_APSCAN
			if (RSDB_APSCAN_ENAB(wlc->pub) && wlc->as->cmn->join_targets_last == 0) {
				int ret = wlc_assoc_handle_prune_scanstate(cfg);
				if (ret == BCME_BUSY) {
					/* Waiting for second roam pass to complete. */
					goto exit;
				}
			}
#endif /* RSDB_APSCAN */
			roam->roam_scan_started = FALSE;
			if (wlc->as->cmn->join_targets_last == 0 &&
					(roam->rcc_mode != RCC_MODE_FORCE)) {
				if (roam->fullscan_count == roam->nfullscans ||
					wlc->as->cmn->join_targets->count == 0) {
					/* first roam scan failed to find a join target,
					 * then, retry the full roam scan immediately
					 */
					cfg->roam->scan_block = 0;
					roam->fullscan_count = 1;
					/* force a full scan */
					roam->time_since_upd = roam->fullscan_period;
					roam->cache_valid = FALSE;
					roam->rcc_valid = FALSE;
					force_rescan = TRUE;
				}
			}
		} else {
			if (for_roam && roam_active && (cache_valid == FALSE)) {
				roam->roam_fullscan = FALSE;
				if (roam->rcc_mode != RCC_MODE_FORCE) {
					/* full roaming scan done, update the roam channel cache */
					rcc_update_from_join_targets(roam,
					wlc->as->cmn->join_targets);
				}
			}
		}
	}
#endif /* WLRCC */

	/* Clear the flag */
	as->flags &= ~AS_F_CACHED_ROAM;

	if (wlc->as->cmn->join_targets_last > 0) {
		wlc_ap_mute(wlc, TRUE, cfg, -1);
		if (for_roam &&
			(!WLFMC_ENAB(wlc->pub) ||
			cfg->roam->reason != WLC_E_REASON_INITIAL_ASSOC))
			wlc_roam_set_env(cfg, wlc->as->cmn->join_targets->count);
		wlc_join_attempt(cfg);
	} else if (target_bss->bss_type != DOT11_BSSTYPE_INFRASTRUCTURE &&
		(!wlc->IBSS_join_only && target_bss->SSID_len > 0)) {
		/* no join targets */
		/* Create an IBSS if we are IBSS or AutoUnknown mode,
		 * and we had a non-Null SSID
		 */
		WL_ASSOC(("wl%d: JOIN: creating an IBSS with SSID \"%s\"\n",
			WLCWLUNIT(wlc), ssidstr));
		wlc_create_ibss(wlc, cfg);
	} else {
		/* no join targets */
		/* see if the target channel information could be cached, if caching is desired */
		if (for_roam && roam->active && roam->partialscan_period &&
#ifdef WLRCC
			!force_rescan &&
#endif /* WLRCC */
			TRUE) {
			wlc_roamscan_complete(cfg);
		}

		/* Retry the scan if we used the scan cache for the initial attempt,
		 * Otherwise, report the failure
		 */
		if (!for_roam && ASSOC_CACHE_ASSIST_ENAB(wlc) &&
		    (as->flags & AS_F_CACHED_CHANNELS))
			wlc_assoc_cache_fail(cfg);
#ifdef WL_ASSOC_RECREATE
		else if (as->flags & AS_F_SPEEDY_RECREATE)
			wlc_speedy_recreate_fail(cfg);
#endif /* WL_ASSOC_RECREATE */
		else {
			WL_ASSOC(("scan found no target\n"));

#ifdef WLP2P
			if (BSS_P2P_ENAB(wlc, cfg) &&
			    for_roam &&
			    (as->type == AS_ROAM)) {

				/* indicate DISASSOC due to unreachability */
				wlc_handle_ap_lost(wlc, cfg);

				WL_ASSOC(("wl%d %s: terminating roaming for p2p\n",
					WLCWLUNIT(wlc), __FUNCTION__));
				wlc_join_done_int(cfg, WLC_E_STATUS_NO_NETWORKS);
				wlc_bsscfg_disable(wlc, cfg);
				goto exit;
			}
#endif /* WLP2P */
			wlc_join_done_int(cfg, WLC_E_STATUS_NO_NETWORKS);
#ifdef WLRCC
			if (force_rescan) {
				WL_ROAM(("Force roamscan reason %d\n", roam->reason));
				wlc_roamscan_start(cfg, roam->reason);
			}
#endif // endif
		}
	}
exit:
#ifdef WLLED
	wlc_led_event(wlc->ledh);
#endif // endif

#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	if (ssidbuf != NULL)
		MFREE(wlc->osh, (void *)ssidbuf, SSID_FMT_BUF_LEN);
#endif // endif
	return;
} /* wlc_assoc_scan_complete */

/**
 * cases to roam
 * - wlc_watchdog() found beacon lost for too long
 * - wlc_recvdata found low RSSI in received frames
 * - AP DEAUTH/DISASSOC sta
 * it will end up in wlc_assoc_scan_complete and WLC_E_ROAM
 */
int
wlc_roam_scan(wlc_bsscfg_t *cfg, uint reason, chanspec_t *list, uint32 channum)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_roam_t *roam = cfg->roam;
	int ret = BCME_OK;
	uint cur_roam_reason = roam->reason;

	if (roam->off) {
		WL_INFORM(("wl%d: roam scan is disabled\n", wlc->pub->unit));
		ret = BCME_EPERM;
		goto fail;
	}

	if (!cfg->associated) {
		WL_ASSOC_ERROR(("wl%d: %s: AP not associated\n", WLCWLUNIT(wlc), __FUNCTION__));
		ret = BCME_NOTASSOCIATED;
		goto fail;
	}

	roam->reason = reason;
	if ((ret = wlc_mac_request_entry(wlc, cfg, WLC_ACTION_ROAM, 0)) != BCME_OK) {
		WL_ASSOC_ERROR(("wl%d.%d: ROAM request failed\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg)));
		roam->reason = cur_roam_reason;
		goto fail;
	}

	if (roam->original_reason == WLC_E_REASON_INITIAL_ASSOC) {
		WL_ASSOC(("wl%d %s: update original roam reason to %u\n", WLCWLUNIT(wlc),
			__FUNCTION__, reason));
		roam->original_reason = reason;
	}

	wlc_assoc_init(cfg, AS_ROAM);

	WL_ROAM(("ROAM: Start roamscan reason %d\n", reason));

#ifdef WL_EVENT_LOG_COMPILE
	{
	wl_event_log_tlv_hdr_t tlv_log = {{0, 0}};

	tlv_log.tag = TRACE_TAG_VENDOR_SPECIFIC;
	tlv_log.length = sizeof(uint);

	WL_EVENT_LOG((EVENT_LOG_TAG_TRACE_WL_INFO, TRACE_ROAM_SCAN_STARTED,
	              tlv_log.t, reason));
	}
#endif /* WL_EVENT_LOG_COMPILE */

	/* start the re-association process by starting the association scan */

	/* This relies on COPIES of the scan parameters being made
	 * somewhere below in the call chain.
	 * If not there can be trouble!
	 */
	if (cfg->roam_scan_params && cfg->roam_scan_params->active_time) {
		wl_join_scan_params_t jsc;

		bzero(&jsc, sizeof(jsc));

		jsc.scan_type = cfg->roam_scan_params->scan_type;
		jsc.nprobes = cfg->roam_scan_params->nprobes;
		jsc.active_time = cfg->roam_scan_params->active_time;
		jsc.passive_time = cfg->roam_scan_params->passive_time;
		jsc.home_time = cfg->roam_scan_params->home_time;

		wlc_assoc_scan_prep(cfg, &jsc, NULL,
			cfg->roam_scan_params->channel_list,
			cfg->roam_scan_params->channel_num);
	} else {
		/* original */
		wlc_assoc_scan_prep(cfg, NULL, NULL, list, channum);
	}

	return 0;

fail:
#ifdef OPPORTUNISTIC_ROAM
	memcpy(cfg->join_bssid.octet, BSSID_INVALID, sizeof(cfg->join_bssid.octet));
#endif /* OPPORTUNISTIC_ROAM */
	return ret;
}

static uint32
wlc_convert_rsn_to_wsec_bitmap(struct rsn_parms *rsn)
{
	uint index;
	uint32 ap_wsec = 0;

	for (index = 0; index < rsn->ucount; index++) {
		ap_wsec |= bcmwpa_wpaciphers2wsec(rsn->unicast[index]);
	}

	return ap_wsec;
}

/*
 * check encryption settings: return TRUE if security mismatch
 *
 * common_wsec = common ciphers between AP/IBSS and STA
 *
 * if (WSEC_AES_ENABLED(common_wsec)) {
 *    if (((AP ucast is None) && (AP mcast is AES)) ||
 *         (!(AP mcast is None) && (AP ucast includes AES))) {
 *         keep();
 *     }
 * } else if (WSEC_TKIP_ENABLED(common_wsec)) {
 *     if (((AP ucast is None) && (AP mcast is TKIP)) ||
 *         (!(AP mcast is None) && (AP ucast includes TKIP))) {
 *         keep();
 *     }
 * } else if (WSEC_WEP_ENABLED(common_wsec)) {
 *     if ((AP ucast is None) && (AP mcast is WEP)) {
 *         keep();
 *     }
 * }
 * prune();
 *
 * TKIP countermeasures:
 * - prune non-WPA
 * - prune WPA with encryption <= TKIP
 */
static bool
wlc_join_wsec_filter(wlc_info_t *wlc, wlc_bsscfg_t *cfg, wlc_bss_info_t *bi)
{
	struct rsn_parms *rsn;
	bool prune = TRUE;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif // endif
	bool akm_match = TRUE;
	uint32 ap_wsec; /* Ciphers supported by AP/IBSS in bitmap format */
	uint32 common_wsec; /* Common ciphers supported between AP/IBSS and STA */

	BCM_REFERENCE(common_wsec);

	if (WLOSEN_ENAB(wlc->pub) && wlc_hs20_is_osen(wlc->hs20, cfg))
		return FALSE;

#ifdef WLFBT
	/* Check AKM suite in target AP against the one STA is currently configured for */
	if (BSSCFG_IS_FBT(cfg)) {
		akm_match = wlc_fbt_akm_match(wlc->fbt, cfg, bi);
	}
#endif /* WLFBT */

	/* check authentication mode */
	if (bcmwpa_is_wpa_auth(cfg->WPA_auth) && (bi->wpa.flags & RSN_FLAGS_SUPPORTED) &&
		/* AKM count zero is not acceptable */
		(bi->wpa.acount != 0)) {
		rsn = &(bi->wpa);
	}
	/* Prune BSSID when STA is moving to a different security type */
	else if ((bcmwpa_is_rsn_auth(cfg->WPA_auth) && (bi->wpa2.flags & RSN_FLAGS_SUPPORTED) &&
		/* AKM count zero is not acceptable */
		(bi->wpa2.acount != 0)) && akm_match) {
		rsn = &(bi->wpa2);
	}
	else
	{
		WL_ASSOC(("wl%d: JOIN: BSSID %s pruned for security reasons\n",
			WLCWLUNIT(wlc), bcm_ether_ntoa(&bi->BSSID, eabuf)));
		wlc_bss_mac_event(wlc, cfg, WLC_E_PRUNE, &bi->BSSID, 0,
			WLC_E_RSN_MISMATCH, 0, 0, 0);
		return prune;
	}

#ifdef MFP
	/* do MFP checking before more complicated algorithm checks below */
	if ((BSSCFG_IS_MFP_REQUIRED(cfg)) && !(bi->wpa2.flags & RSN_FLAGS_MFPC)) {
		/* We require MFP , but peer is not MFP capable */
		WL_ASSOC(("wl%d: %s: BSSID %s pruned, MFP required but peer does"
			" not advertise MFP\n",
			WLCWLUNIT(wlc), __FUNCTION__, bcm_ether_ntoa(&bi->BSSID, eabuf)));
		return prune;
	}
	if ((bi->wpa2.flags & RSN_FLAGS_MFPR) && !(BSSCFG_IS_MFP_CAPABLE(cfg))) {
		/* Peer requires MFP , but we don't have MFP enabled */
		WL_ASSOC(("wl%d: %s: BSSID %s pruned, peer requires MFP but MFP not"
			" enabled locally\n",
			WLCWLUNIT(wlc), __FUNCTION__, bcm_ether_ntoa(&bi->BSSID, eabuf)));
		return prune;
	}
#endif /* MFP */

	/* Get the AP/IBSS RSN (ciphers) to bitmap wsec format */
	ap_wsec = wlc_convert_rsn_to_wsec_bitmap(rsn);

	/* Find the common ciphers between AP/IBSS and STA */
	common_wsec = ap_wsec & cfg->wsec;

	WL_ASSOC(("wl%d: %s: AP/IBSS wsec %04x, STA wsec %04x, Common wsec %04x \n",
		WLCWLUNIT(wlc), __FUNCTION__, ap_wsec, cfg->wsec, common_wsec));

	/* Clear TKIP Countermeasures Block Timer, if timestamped earlier
	 * than WPA_TKIP_CM_BLOCK (=60 seconds).
	 */

	if (WSEC_AES_ENABLED(common_wsec)) {

		if ((UCAST_NONE(rsn) && MCAST_AES(rsn)) || (MCAST_AES(rsn) && UCAST_AES(rsn)) ||
			(!TK_CM_BLOCKED(wlc, cfg) && (!MCAST_NONE(rsn) && UCAST_AES(rsn))))
			prune = FALSE;
		else {
			WL_ASSOC(("wl%d: JOIN: BSSID %s AES: no AES support ot TKIP cm bt \n",
				WLCWLUNIT(wlc),	bcm_ether_ntoa(&bi->BSSID, eabuf)));
			wlc_bss_mac_event(wlc, cfg, WLC_E_PRUNE, &bi->BSSID, 0,
				WLC_E_PRUNE_ENCR_MISMATCH,
				CHSPEC_CHANNEL(bi->chanspec), 0, 0);
		}

	} else if (WSEC_TKIP_ENABLED(common_wsec)) {

		if (!TK_CM_BLOCKED(wlc, cfg) && ((UCAST_NONE(rsn) && MCAST_TKIP(rsn)) ||
			(!MCAST_NONE(rsn) && UCAST_TKIP(rsn))))
			prune = FALSE;
		else {
			WL_ASSOC(("wl%d: JOIN: BSSID %s TKIP: no TKIP support or TKIP cm bt\n",
				WLCWLUNIT(wlc), bcm_ether_ntoa(&bi->BSSID, eabuf)));
			wlc_bss_mac_event(wlc, cfg, WLC_E_PRUNE, &bi->BSSID, 0,
				WLC_E_PRUNE_ENCR_MISMATCH,
				CHSPEC_CHANNEL(bi->chanspec), 0, 0);
		}

	} else
	{
		if (!TK_CM_BLOCKED(wlc, cfg) && (UCAST_NONE(rsn) && MCAST_WEP(rsn)))
			prune = FALSE;
		else {
			WL_ASSOC(("wl%d: JOIN: BSSID %s no WEP support or TKIP cm\n",
				WLCWLUNIT(wlc), bcm_ether_ntoa(&bi->BSSID, eabuf)));
			wlc_bss_mac_event(wlc, cfg, WLC_E_PRUNE, &bi->BSSID, 0,
				WLC_E_PRUNE_ENCR_MISMATCH,
				CHSPEC_CHANNEL(bi->chanspec), 0, 0);
		}
	}

	if (prune) {
		WL_ASSOC(("wl%d: JOIN: BSSID %s pruned for security reasons\n",
			WLCWLUNIT(wlc), bcm_ether_ntoa(&bi->BSSID, eabuf)));
	}

	return prune;
} /* wlc_join_wsec_filter */

/** check the channel against the channels in chanspecs list in assoc params */
static bool
wlc_join_chanspec_filter(wlc_bsscfg_t *bsscfg, chanspec_t chanspec)
{
	wl_join_assoc_params_t *assoc_params = wlc_bsscfg_assoc_params(bsscfg);
	int i, num;

	/* no restrictions */
	if (assoc_params == NULL)
		return TRUE;

	num = assoc_params->chanspec_num;

	/* if specified pairs, check only the current chanspec */
	if (assoc_params->bssid_cnt)
		return (chanspec == assoc_params->chanspec_list[num]);

	if (assoc_params != NULL && assoc_params->chanspec_num > 0) {
		for (i = 0; i < assoc_params->chanspec_num; i ++)
			if (chanspec == assoc_params->chanspec_list[i])
				return TRUE;
		return FALSE;
	}

	return TRUE;
}

#if defined(WLMSG_ROAM) && !defined(WLMSG_ASSOC)
#undef WL_ASSOC
#define WL_ASSOC(args) printf args
#endif /* WLMSG_ROAM && !WLMSG_ASSOC */

/**
 * scan finished with valid join_targets
 * loop through join targets and run all prune conditions
 *    if there is a one surviving, start join process.
 *    this function will be called if the join fails so next target(s) will be tried.
 */
void
wlc_join_attempt(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_join_pref_t *join_pref = cfg->join_pref;
	wlc_bss_info_t *bi;
	uint i;
	wlcband_t *target_band;
#if defined(BCMDBG) || defined(WLMSG_ASSOC) || defined(WLMSG_ROAM)
	char ssidbuf[SSID_FMT_BUF_LEN];
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_ASSOC */
	uint32 wsec, WPA_auth;
	int addr_match;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char chanbuf[CHANSPEC_STR_LEN];
#endif // endif
	bool ret;

	/* keep core awake until join finishes, as->state != AS_IDLE */
	ASSERT(STAY_AWAKE(wlc));
	wlc_set_wake_ctrl(wlc);

	WL_ASSOC(("wl%d.%d %s Join to \"%s\"\n", wlc->pub->unit, cfg->_idx, __FUNCTION__,
		cfg->target_bss->SSID));
	wlc_assoc_timer_del(wlc, cfg);

	/* validity check */
	if (wlc->as->cmn->join_targets == NULL) {
		WL_ASSOC(("wl%d: JOIN: join targets == NULL skipping,\n", WLCWLUNIT(wlc)));
		wlc_join_done_int(cfg, WLC_E_STATUS_FAIL);
		return;
	}

	/* walk the list until there is a valid join target */
	for (; (wlc->as->cmn->join_targets_last > 0 && wlc->as->cmn->join_targets_last < MAXBSS);
		wlc->as->cmn->join_targets_last--) {
		wlc_rateset_t rateset;
		chanspec_t chanspec;
		wlc_bsscfg_t *bc = NULL;

		bi = wlc_assoc_get_join_target(wlc, 0);
		ASSERT(bi != NULL);

		target_band = wlc->bandstate[CHSPEC_WLCBANDUNIT(bi->chanspec)];
		WL_ROAM(("JOIN: checking [%d] %s\n", wlc->as->cmn->join_targets_last - 1,
			bcm_ether_ntoa(&bi->BSSID, eabuf)));
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
		wlc_format_ssid(ssidbuf, bi->SSID, bi->SSID_len);
#endif // endif
#if defined(BCMQT)	/* bypass channel sanitize in Veloce test for 40/80 MHz BW */
		chanspec = bi->chanspec;
#else
		/* prune invalid chanspec based on STA capibility */
		if ((chanspec = wlc_max_valid_bw_chanspec(wlc, target_band,
				cfg, bi->chanspec)) == INVCHANSPEC) {
			WL_ASSOC(("wl%d: JOIN: Skipping invalid chanspec(0x%x):",
				WLCWLUNIT(wlc), bi->chanspec));
			continue;
		}
#endif /* BCMQT */

		/* validate BSS channel */
		if (!ISSIM_ENAB(wlc->pub->sih) && !wlc_join_chanspec_filter(cfg,
			CH20MHZ_CHSPEC(wf_chspec_ctlchan(bi->chanspec)))) {
			WL_ASSOC(("wl%d: JOIN: Skipping BSS %s, mismatch chanspec %x\n",
			          WLCWLUNIT(wlc), bcm_ether_ntoa(&bi->BSSID, eabuf),
			          bi->chanspec));
			continue;
		}

		/* Check if the chanspec still do not list in country setting after update to max
		 * valid chanspec
		 */
		if (WLC_ASSOC_IS_PROHIBITED_CHANSPEC(wlc, chanspec) == TRUE) {
			WL_ASSOC(("wl%d: JOIN: Found BSS %s,"
				"on restricted chanspec 0x%x in country %s\n",
				WLCWLUNIT(wlc), bcm_ether_ntoa(&bi->BSSID, eabuf), chanspec,
				wlc_channel_country_abbrev(wlc->cmi)));
		}

		/* mSTA: Check here to make sure we don't end up with multiple bsscfgs
		 * associated to the same BSS. Skip this when PSTA mode is enabled.
		 */
		if (!PSTA_ENAB(wlc->pub)) {
			FOREACH_AS_STA(wlc, i, bc) {
				if (bc != cfg &&
				    bcmp(&bi->BSSID, &bc->BSSID, ETHER_ADDR_LEN) == 0)
					break;
				bc = NULL;
			}
			if (bc != NULL) {
				WL_ASSOC(("wl%d.%d: JOIN: Skipping BSS %s, "
				          "associated by bsscfg %d\n",
				          WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg),
				          bcm_ether_ntoa(&bi->BSSID, eabuf),
				          WLC_BSSCFG_IDX(bc)));
				continue;
			}
		}

		/* prune roam candidate if security doesn't match */
		if (cfg->assoc->type == AS_ROAM) {
			if (!wlc_assoc_check_roam_candidate(cfg, bi)) {
				continue;
			}
		}

		/* derive WPA config if WPA auto config is on */
		if (WLCAUTOWPA(cfg)) {
			ASSERT(bi->wpacfg < join_pref->wpas);
			/* auth */
			cfg->auth = DOT11_OPEN_SYSTEM;
			/* WPA_auth */
			ret = bcmwpa_akm2WPAauth(join_pref->wpa[bi->wpacfg].akm, &WPA_auth);

			if (ret == FALSE) {
				WL_ASSOC_ERROR(("wl%d.%d: %s: Failed to set WPA Auth! WPA cfg:%d\n",
					WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg), __FUNCTION__,
					bi->wpacfg));
			}
			cfg->WPA_auth = WPA_auth;

			/* wsec - unicast */
			ret = bcmwpa_cipher2wsec(join_pref->wpa[bi->wpacfg].ucipher, &wsec);
			if (ret == FALSE) {
				WL_ASSOC_ERROR(("wl%d.%d: %s: Failed to set WSEC! WPA cfg:%d\n",
					WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg), __FUNCTION__,
					bi->wpacfg));
			}
			/*
			 * use multicast cipher only when unicast cipher is none, otherwise
			 * the next block (OID_802_11_ENCRYPTION_STATUS related) takes care of it
			 */
			if (!wsec) {
				uint8 mcs[WPA_SUITE_LEN];
				bcopy(join_pref->wpa[bi->wpacfg].mcipher, mcs, WPA_SUITE_LEN);
				if (!bcmp(mcs, WL_WPA_ACP_MCS_ANY, WPA_SUITE_LEN)) {
					mcs[DOT11_OUI_LEN] = bi->mcipher;
				}
				if (!bcmwpa_cipher2wsec(mcs, &wsec)) {
					WL_ASSOC(("JOIN: Skip BSS %s WPA cfg %d, cipher2wsec"
						" failed\n",
						bcm_ether_ntoa(&bi->BSSID, eabuf),
						bi->wpacfg));
					wlc_bss_mac_event(wlc, cfg, WLC_E_PRUNE, &bi->BSSID,
						0, WLC_E_PRUNE_CIPHER_NA,
						CHSPEC_CHANNEL(bi->chanspec), 0, 0);
					continue;
				}
			}
#ifdef BCMSUP_PSK
			if (SUP_ENAB(wlc->pub) &&
				BSS_SUP_ENAB_WPA(wlc->idsup, cfg)) {
				if (WSEC_AES_ENABLED(wsec))
					wsec |= TKIP_ENABLED;
				if (WSEC_TKIP_ENABLED(wsec))
					wsec |= WEP_ENABLED;
				/* merge rest flags */
				wsec |= cfg->wsec & ~(AES_ENABLED | TKIP_ENABLED |
				                      WEP_ENABLED);
			}
#endif /* BCMSUP_PSK */
			wlc_iovar_op(wlc, "wsec", NULL, 0, &wsec, sizeof(wsec),
				IOV_SET, cfg->wlcif);
			WL_ASSOC(("wl%d: JOIN: BSS %s wpa cfg %d WPA_auth 0x%x wsec 0x%x\n",
				WLCWLUNIT(wlc), bcm_ether_ntoa(&bi->BSSID, eabuf),
				bi->wpacfg, cfg->WPA_auth, cfg->wsec));
			wlc_bss_mac_event(wlc, cfg, WLC_E_AUTOAUTH, &bi->BSSID,
				WLC_E_STATUS_ATTEMPT, 0, bi->wpacfg, 0, 0);
		}

		/* check Privacy (encryption) in target BSS */
		/*
		 * WiFi: A STA with WEP off should never attempt to associate with
		 * an AP that has WEP on
		 */
		if ((bi->capability & DOT11_CAP_PRIVACY) && !WSEC_ENABLED(cfg->wsec)) {
			if (cfg->is_WPS_enrollee) {
				WL_ASSOC(("wl%d: JOIN: Assuming join to BSSID %s is for WPS "
				          " credentials, so allowing unencrypted EAPOL frames\n",
				          WLCWLUNIT(wlc), bcm_ether_ntoa(&bi->BSSID, eabuf)));
			} else {
				WL_ASSOC(("wl%d: JOIN: Skipping BSSID %s, encryption mandatory "
				          "in BSS, but encryption off for us.\n", WLCWLUNIT(wlc),
				          bcm_ether_ntoa(&bi->BSSID, eabuf)));
				wlc_bss_mac_event(wlc, cfg, WLC_E_PRUNE, &bi->BSSID, 0,
				                  WLC_E_PRUNE_ENCR_MISMATCH,
				                  CHSPEC_CHANNEL(bi->chanspec), 0, 0);
				continue;
			}
		}

		/* skip broadcast bssid */
		if (ETHER_ISBCAST(bi->BSSID.octet)) {
			WL_ASSOC(("wl%d: JOIN: Skipping BSS with broadcast BSSID\n",
				WLCWLUNIT(wlc)));
			wlc_bss_mac_event(wlc, cfg, WLC_E_PRUNE, &bi->BSSID, 0,
				WLC_E_PRUNE_BCAST_BSSID,
				CHSPEC_CHANNEL(bi->chanspec), 0, 0);
			continue;
		}
		/* prune join_targets based on allow/deny list */
		addr_match = wlc_macfltr_addr_match(wlc->macfltr, cfg, &bi->BSSID);
		if (addr_match == WLC_MACFLTR_ADDR_DENY) {
			WL_ASSOC(("wl%d: JOIN: pruning BSSID %s because it "
			          "was on the MAC Deny list\n", WLCWLUNIT(wlc),
			          bcm_ether_ntoa(&bi->BSSID, eabuf)));
			wlc_bss_mac_event(wlc, cfg, WLC_E_PRUNE, &bi->BSSID,
			                  0, WLC_E_PRUNE_MAC_DENY,
			                  CHSPEC_CHANNEL(bi->chanspec), 0, 0);
			continue;
		} else if (addr_match == WLC_MACFLTR_ADDR_NOT_ALLOW) {
			WL_ASSOC(("wl%d: JOIN: pruning BSSID %s because it "
			          "was not on the MAC Allow list\n", WLCWLUNIT(wlc),
			          bcm_ether_ntoa(&bi->BSSID, eabuf)));
			wlc_bss_mac_event(wlc, cfg, WLC_E_PRUNE, &bi->BSSID,
			                  0, WLC_E_PRUNE_MAC_NA,
			                  CHSPEC_CHANNEL(bi->chanspec), 0, 0);
			continue;
		}

		/* prune if we are in strict SpectrumManagement mode, the AP is not advertising
		 * support, and the locale requires 802.11h SpectrumManagement
		 */
		if (WL11H_ENAB(wlc) &&
		    (wlc_11h_get_spect(wlc->m11h) == SPECT_MNGMT_STRICT_11H) &&
		    bi->bss_type == DOT11_BSSTYPE_INFRASTRUCTURE &&
		    (bi->capability & DOT11_CAP_SPECTRUM) == 0 &&
		    (wlc_channel_locale_flags_in_band(wlc->cmi,
				target_band->bandunit) & WLC_DFS_TPC)) {
			WL_ASSOC(("wl%d: JOIN: pruning AP %s (SSID \"%s\", chanspec %s). "
			          "Current locale \"%s\" requires spectrum management "
			          "but AP does not have SpectrumManagement.\n",
			          WLCWLUNIT(wlc), bcm_ether_ntoa(&bi->BSSID, eabuf), ssidbuf,
			          wf_chspec_ntoa_ex(bi->chanspec, chanbuf),
			          wlc_channel_country_abbrev(wlc->cmi)));
			wlc_bss_mac_event(wlc, cfg, WLC_E_PRUNE, &bi->BSSID, 0,
				WLC_E_PRUNE_SPCT_MGMT,
				CHSPEC_CHANNEL(bi->chanspec), 0, 0);
			continue;
		}

		if (WL11H_ENAB(wlc) &&
		    bi->bss_type == DOT11_BSSTYPE_INFRASTRUCTURE &&
		    wlc_csa_quiet_mode(wlc->csa, (uint8 *)bi->bcn_prb, bi->bcn_prb_len)) {
			WL_ASSOC(("JOIN: pruning AP %s (SSID \"%s\", chanspec %s). "
				"AP is CSA quiet period.\n",
				  bcm_ether_ntoa(&bi->BSSID, eabuf), ssidbuf,
				  wf_chspec_ntoa_ex(bi->chanspec, chanbuf)));
			wlc_bss_mac_event(wlc, cfg, WLC_E_PRUNE, &bi->BSSID, 0,
				WLC_E_PRUNE_SPCT_MGMT,
				CHSPEC_CHANNEL(bi->chanspec), 0, 0);
			continue;
		}

		/* prune if in 802.11h SpectrumManagement mode and the IBSS is on radar channel */
		if ((WL11H_ENAB(wlc)) &&
		    (bi->bss_type == DOT11_BSSTYPE_INDEPENDENT) &&
		    wlc_radar_chanspec(wlc->cmi, bi->chanspec)) {
			WL_ASSOC(("wl%d: JOIN: pruning IBSS \"%s\" chanspec %s since "
			      "we are in 802.11h mode and IBSS is on a radar channel in "
			      "locale \"%s\"\n", WLCWLUNIT(wlc),
			      ssidbuf, wf_chspec_ntoa_ex(bi->chanspec, chanbuf),
			      wlc_channel_country_abbrev(wlc->cmi)));
			wlc_bss_mac_event(wlc, cfg, WLC_E_PRUNE, &bi->BSSID, 0,
				WLC_E_PRUNE_RADAR,
			        CHSPEC_CHANNEL(bi->chanspec), 0, 0);
			continue;
		}
		/* prune if the IBSS is on a restricted channel */
		if ((bi->bss_type == DOT11_BSSTYPE_INDEPENDENT) &&
		    wlc_restricted_chanspec(wlc->cmi, bi->chanspec)) {
			WL_ASSOC(("wl%d: JOIN: pruning IBSS \"%s\" chanspec %s since is is "
			          "on a restricted channel in locale \"%s\"\n",
			          WLCWLUNIT(wlc), ssidbuf,
			          wf_chspec_ntoa_ex(bi->chanspec, chanbuf),
			          wlc_channel_country_abbrev(wlc->cmi)));
			wlc_bss_mac_event(wlc, cfg, WLC_E_PRUNE, &bi->BSSID, 0,
				WLC_E_PRUNE_REG_PASSV,
				CHSPEC_CHANNEL(bi->chanspec), 0, 0);
			continue;
		}

		/* prune join targets based on security settings */
		if (cfg->WPA_auth != WPA_AUTH_DISABLED && WSEC_ENABLED(cfg->wsec)) {
			if (wlc_join_wsec_filter(wlc, cfg, bi))
				continue;
		}

		/* XXX
		 *
		 * Full text from the spec Std 802.11-2012:
		 * 10.1.4.6 Operation of Supported Rates and Extended Supported Rates elements:
		 * Supported Rate and Extended Supported Rate information in Beacon and
		 * Probe Response management frames is used by STAs in order to avoid associating
		 * with a BSS if they do not support all the data rates in the BSSBasicRateSet
		 * parameter or all of the BSS membership requirements in the
		 * BSSMembershipSelectorSet parameter.
		 *
		 * However the behavior has not been fully upto the spec except IBSS...but when
		 * we do want to please modify wlc_join_verify_basic_rates() to make it happen.
		 */

		/* work on a copy of the target's rateset because we modify it. */
		memcpy(&rateset, &bi->rateset, sizeof(bi->rateset));

		/* skip any BSS having BSS membership selectors we know of and do not support */
		if (!wlc_join_verify_filter_membership(wlc, cfg, bi, &rateset)) {
			WL_ASSOC(("wl%d: JOIN: BSSID %s pruned because we do not support all "
			          "BSS Membership Selectors\n", WLCWLUNIT(wlc),
			          bcm_ether_ntoa(&bi->BSSID, eabuf)));
			wlc_bss_mac_event(wlc, cfg, WLC_E_PRUNE, &bi->BSSID, 0,
			                  WLC_E_PRUNE_BASIC_RATES, 0, 0, 0);
			continue;
		}

		/* skip any IBSS having basic rates which we do not support */
		if (!wlc_join_verify_basic_rates(wlc, cfg, bi, &rateset)) {
			WL_ASSOC(("wl%d: JOIN: BSSID %s pruned because we do not support all "
			      "Basic Rates of the BSS\n", WLCWLUNIT(wlc),
			      bcm_ether_ntoa(&bi->BSSID, eabuf)));
			wlc_bss_mac_event(wlc, cfg, WLC_E_PRUNE, &bi->BSSID, 0,
				WLC_E_PRUNE_BASIC_RATES, 0, 0, 0);
			continue;
		}

		/* prune based on rateset, bail out if no common rates with the BSS/IBSS */
		if (!wlc_join_target_verify_rates(wlc, cfg, bi, &rateset)) {
			WL_ASSOC(("wl%d: JOIN: BSSID %s pruned because we don't have any rates "
			          "in common with the BSS\n", WLCWLUNIT(wlc),
			          bcm_ether_ntoa(&bi->BSSID, eabuf)));
			wlc_bss_mac_event(wlc, cfg, WLC_E_PRUNE, &bi->BSSID, 0,
			                  WLC_E_PRUNE_NO_COMMON_RATES, 0, 0, 0);
			continue;
		}

		if (APSTA_ENAB(wlc->pub)) {
			struct scb *scb;
			scb = wlc_scbfind(wlc, cfg, &bi->BSSID);
			if ((scb == NULL) && (NBANDS(wlc) > 1))
				scb = wlc_scbfindband(wlc, cfg, &bi->BSSID, OTHERBANDUNIT(wlc));
			if (scb && SCB_ASSOCIATED(scb) && !(scb->flags & SCB_MYAP)) {
				WL_ASSOC(("wl%d: JOIN: BSSID %s pruned, it's a known STA\n",
				            WLCWLUNIT(wlc), bcm_ether_ntoa(&bi->BSSID, eabuf)));
				wlc_bss_mac_event(wlc, cfg, WLC_E_PRUNE, &bi->BSSID, 0,
					WLC_E_PRUNE_KNOWN_STA, 0, 0, 0);
				continue;
			}
			if (scb && SCB_LEGACY_WDS(scb)) {
				WL_ASSOC(("wl%d: JOIN: BSSID %s pruned, it's a WDS peer\n",
				          WLCWLUNIT(wlc), bcm_ether_ntoa(&bi->BSSID, eabuf)));
				wlc_bss_mac_event(wlc, cfg, WLC_E_PRUNE, &bi->BSSID, 0,
				              WLC_E_PRUNE_WDS_PEER, 0, 0, 0);
				continue;
			}
		}

#ifdef WL_MBO
		if (BSS_MBO_ENAB(wlc, cfg)) {
			if (bi->bcnflags & WLC_BSS_MBO_ASSOC_DISALLOWED) {
				continue;
			}
		}
#endif /* WL_MBO */
#ifdef WL_OCE
		if (OCE_ENAB(wlc->pub) &&
			(bi->flags2 & WLC_BSS_OCE_ASSOC_REJD)) {
				continue;
		}
#endif // endif
		/* reach here means BSS passed all the pruning tests, so break the loop to join */
		break;
	}

	wlc_join_attempt_select(cfg);
}

void
wlc_join_attempt_select(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	wlc_bss_info_t *bi;

#if defined(BCMDBG) || defined(WLMSG_ASSOC) || defined(WLMSG_ROAM)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_ASSOC */

	if (wlc->as->cmn->join_targets_last > 0) {

		wlc_join_attempt_info_start(cfg, wlc_assoc_get_join_target(wlc, 0));

		as->bss_retries = 0;

		bi = wlc_assoc_get_join_target(wlc, 0);
		ASSERT(bi != NULL);

		WL_ASSOC(("wl%d: JOIN: attempting BSSID: %s\n", WLCWLUNIT(wlc),
		            bcm_ether_ntoa(&bi->BSSID, eabuf)));

		if (as->type != AS_ASSOCIATION)
			wlc_bss_mac_event(wlc, cfg, WLC_E_ROAM_PREP, &bi->BSSID, 0, 0, 0, NULL, 0);

		{
#ifdef BCMSUP_PSK
		/* XXX at this point we may not know the auth. type (WPA vs. PSK)
		 * until the BSS is chosen when we are doing auto WPA. So call
		 * wlc_sup_set_ssid() as long as we choose in-driver PSK supplicant
		 * and let wlc_sup_set_ssid() to decide if passhash algorithm is
		 * needed based on if the pass phrase is empty or not.
		 */
		if (SUP_ENAB(wlc->pub) &&
		    (BSS_SUP_TYPE(wlc->idsup, cfg) == SUP_WPAPSK)) {
			switch (wlc_sup_set_ssid(wlc->idsup, cfg, bi->SSID, bi->SSID_len)) {
			case 1:
				/* defer association till psk passhash is done */
				wlc_assoc_change_state(cfg, AS_WAIT_PASSHASH);
				return;
			case 0:
				/* psk supplicant is config'd so continue */
				break;
			case -1:
			default:
				/* XXX what do we do when wlc_sup_set_ssid() fails?
				 * should we continue with the association progress?
				 */
				WL_ASSOC(("wl%d: wlc_sup_set_ssid failed, stop assoc\n",
				          wlc->pub->unit));
				wlc_join_done_int(cfg, WLC_E_STATUS_FAIL);
				return;
			}
		}
#endif /* BCMSUP_PSK */

#ifdef WLTDLS
		/* cleanup the TDLS peers for roam */
		if (TDLS_ENAB(wlc->pub) && (as->type == AS_ROAM))
			wlc_tdls_cleanup(wlc->tdls, cfg, FALSE);
#endif /* WLTDLS */

#ifdef WLWNM
		if (WLWNM_ENAB(wlc->pub)) {
			bool delay_join = FALSE;

			/* Decided to join; send bss-trans-response here with accept */
			delay_join = wlc_wnm_bsstrans_roamscan_complete(wlc, cfg,
					DOT11_BSSTRANS_RESP_STATUS_ACCEPT, &bi->BSSID);

			if (delay_join == TRUE) {
				/* wlc_bsstrans_resp_tx_complete will do re-entry */
				return;
			}
		}
#endif /* WLWNM */

		wlc_join_bss_prep(cfg);
		}
		return;
	}

	if (!wlc->as->cmn->join_targets_last && cfg->associated) {
#if defined(WLRSDB)
		if (RSDB_ENAB(wlc->pub)) {
			/* Try to see if a mode switch is required */
			if (wlc_rsdb_assoc_mode_change(&cfg, cfg->current_bss) == BCME_NOTREADY) {
				return;
			}
		}
#endif // endif
	}

	WL_ASSOC(("wl%d.%d: JOIN: no more join targets available\n",
	          WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg)));

#ifdef WLP2P
	/* reset p2p assoc states only for non-roam cases */
	if (BSS_P2P_ENAB(wlc, cfg) && !cfg->associated)
		wlc_p2p_reset_bss(wlc->p2p, cfg);
#endif // endif

	/* handle association failure from a cache assisted assoc */
	if (!cfg->associated &&
	    ASSOC_CACHE_ASSIST_ENAB(wlc) &&
	    (as->flags & (AS_F_CACHED_CHANNELS | AS_F_CACHED_RESULTS))) {
		/* on a cached assoc failure, retry after a full scan */
		wlc_assoc_cache_fail(cfg);
		return;
	}

	/* handle reassociation failure */
	if (cfg->associated) {
#ifdef BCMSUP_PSK
		if (SUP_ENAB(wlc->pub)) {
			wlc_sup_set_ea(wlc->idsup, cfg, &cfg->current_bss->BSSID);
		}
#endif /* BCMSUP_PSK */
#ifdef WLFBT
		if (BSSCFG_IS_FBT(cfg) && (cfg->WPA_auth & WPA2_AUTH_FT)) {
			wlc_fbt_set_ea(wlc->fbt, cfg, &cfg->current_bss->BSSID);
		}
#endif /* WLFBT */
	}

	/* handle association failure */
	wlc_join_done(cfg, NULL, cfg->target_bss->bss_type, WLC_E_STATUS_FAIL);

#if defined(BCMSUP_PSK)
	if (SUP_ENAB(wlc->pub)) {
		wlc_wpa_send_sup_status(wlc->idsup, cfg, WLC_E_SUP_OTHER);
	}
#endif /* defined(BCMSUP_PSK) */

	wlc_block_datafifo(wlc, DATA_BLOCK_JOIN, 0);
	wlc_roam_release_flow_cntrl(cfg);
} /* wlc_join_attempt_select */

void
wlc_join_bss_prep(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_bss_info_t *bi;
#if defined(SLAVE_RADAR)
	bool dfs_cac_started = FALSE;
	bool from_radar = FALSE, to_radar = FALSE;
#endif /* SLAVE_RADAR */

	bi = wlc_assoc_get_join_target(wlc, 0);
	ASSERT(bi != NULL);

#if defined(SLAVE_RADAR)
	if (!BSSCFG_AP(cfg) && wlc_dfs_get_radar(wlc->dfs)) {
		dfs_cac_started = wlc_assoc_do_dfs_reentry_cac_if_required(cfg, bi);
		if (dfs_cac_started) {
			/* join after cac finish */
			return;
		}

	}
#endif /* SLAVE_RADAR */
	wlc_assoc_timer_del(wlc, cfg);

#ifdef SLAVE_RADAR
	from_radar = (wlc_radar_chanspec(wlc->cmi,
			cfg->current_bss->chanspec) == TRUE);
	to_radar = wlc_radar_chanspec(wlc->cmi, bi->chanspec);

	if ((!wlc_is_ap_interface_up(wlc)) && WL11H_STA_ENAB(wlc) && wlc_dfs_get_radar(wlc->dfs)) {
		if (from_radar && !to_radar) {
			cfg->pm->pm_modechangedisabled = FALSE;
			wlc_set_pm_mode(wlc, cfg->pm->pm_oldvalue, cfg);
			wlc->mpc = TRUE;
			wlc_radio_mpc_upd(wlc);
		}
		if (!from_radar && to_radar) {
			cfg->pm->pm_oldvalue = cfg->pm->PM;
			wlc_set_pm_mode(wlc, PM_OFF, cfg);
			cfg->pm->pm_modechangedisabled = TRUE;
			wlc->mpc = FALSE;
			wlc_radio_mpc_upd(wlc);
		}
		if (from_radar && to_radar && (cfg->pm->PM != PM_OFF)) {
			cfg->pm->pm_oldvalue = cfg->pm->PM;
			wlc_set_pm_mode(wlc, PM_OFF, cfg);
			cfg->pm->pm_modechangedisabled = TRUE;
			wlc->mpc = FALSE;
			wlc_radio_mpc_upd(wlc);
		}
	}
#endif /* SLAVE_RADAR */
	wlc_assoc_change_state(cfg, AS_JOIN_START);
	wlc_bss_assoc_state_notif(wlc, cfg, cfg->assoc->type, AS_JOIN_START);

	wlc_join_BSS_limit_caps(cfg, bi);
	cfg->auth_atmptd = cfg->auth;

#ifdef WLFBT
	/* FBT will control state machine if successful */
	if (!wlc_fbt_overds_attempt(cfg))
#endif /* WLFBT */
		wlc_join_bss_start(cfg);
}

void
wlc_join_bss_start(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	wlc_txq_info_t *qi = cfg->wlcif->qi;
	struct pktq *common_q = WLC_GET_CQ(qi);	/**< multi-priority packet queue */

	/* if roaming, make sure tx queue is drain out */
	if (as->type == AS_ROAM && wlc_bss_connected(cfg) &&
	    !wlc->block_datafifo &&
	    (!pktq_empty(common_q) || TXPKTPENDTOT(wlc) > 0)) {
		/* block tx path and roam to a new target AP only after all
		 * the pending tx packets sent out
		 */
		wlc_txflowcontrol_override(wlc, qi, ON, TXQ_STOP_FOR_PKT_DRAIN);
		if (as->state != AS_RECV_FTRES) {
			wlc_assoc_change_state(cfg, AS_WAIT_TX_DRAIN);
		}
		wl_add_timer(wlc->wl, as->timer, WLC_TXQ_DRAIN_TIMEOUT, 0);
		WL_ASSOC(("ROAM: waiting for %d tx packets drained"" out before roam\n",
			pktq_n_pkts_tot(common_q) + TXPKTPENDTOT(wlc)));
	} else {
		wlc_join_bss(cfg);
	}
}

#ifdef AP
/**
 * Determine if any SCB associated to ap cfg
 * cfg specifies a specific ap cfg to compare to.
 * If cfg is NULL, then compare to any ap cfg.
 */
static bool
wlc_scb_associated_to_ap(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	struct scb_iter scbiter;
	struct scb *scb;
	int idx;

	ASSERT((cfg == NULL) || BSSCFG_AP(cfg));

	if (cfg == NULL) {
		FOREACH_UP_AP(wlc, idx, cfg) {
			FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {
				if (SCB_ASSOCIATED(scb)) {
					return TRUE;
				}
			}
		}
	}
	else {
		FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {
			if (SCB_ASSOCIATED(scb)) {
				return TRUE;
			}
		}
	}

	return FALSE;
}

/**
 * Checks to see if we need to send out CSA for local ap cfgs before attempting to join a new AP.
 * Parameter description:
 * wlc - global wlc structure
 * cfg - the STA cfg that is about to perform a join a new AP
 * chanspec - the new chanspec the new AP is on
 * state - can be either AS_WAIT_FOR_AP_CSA or AS_WAIT_FOR_AP_CSA_ROAM_FAIL
 */
static bool
wlc_join_check_ap_need_csa(wlc_info_t *wlc, wlc_bsscfg_t *cfg, chanspec_t chanspec, uint state)
{
	wlc_assoc_t *as = cfg->assoc;
	bool need_csa = FALSE;
	chanspec_t curr_chanspec = WLC_BAND_PI_RADIO_CHANSPEC;

	/* If we're not in AS_WAIT_FOR_AP_CSA states, about to change channel
	 * and AP is active, allow AP to send CSA before performing the channel switch.
	 */
	if ((as->state != state) &&
	    (curr_chanspec != chanspec) &&
	    AP_ACTIVE(wlc)) {
		/* check if any stations associated to our APs */
		if (!wlc_scb_associated_to_ap(wlc, NULL)) {
			/* no stations associated to our APs, return false */
			WL_ASSOC(("wl%d.%d: %s: not doing ap CSA, no stas associated to ap(s)\n",
			          WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg), __FUNCTION__));
			return (FALSE);
		}
		if (wlc_join_ap_do_csa(wlc, chanspec)) {
			wlc_ap_mute(wlc, FALSE, cfg, -1);
			WL_ASSOC(("wl%d.%d: %s: doing ap CSA\n",
			          WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg), __FUNCTION__));
			wlc_assoc_change_state(cfg, state);
			need_csa = TRUE;
		}
	} else {
		WL_ASSOC(("wl%d.%d: %s: ap CSA not needed\n",
		          WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg), __FUNCTION__));
	}
	return need_csa;
}

static bool
wlc_join_ap_do_csa(wlc_info_t *wlc, chanspec_t tgt_chanspec)
{
	wlc_bsscfg_t *apcfg;
	int apidx;
	bool ap_do_csa = FALSE;

	if (WL11H_ENAB(wlc)) {
		FOREACH_UP_AP(wlc, apidx, apcfg) {
			/* find all ap's on current channel */
			if ((WLC_BAND_PI_RADIO_CHANSPEC == apcfg->current_bss->chanspec) &&
#ifdef WLMCHAN
				(!MCHAN_ENAB(wlc->pub) ||
					wlc_mchan_stago_is_disabled(wlc->mchan) ||
					!BSS_P2P_ENAB(wlc, apcfg)))
#else
				TRUE)
#endif // endif
			{
#if defined(WL_RESTRICTED_APSTA)
				if (RAPSTA_ENAB(wlc->pub) &&
					wlc_channel_apsta_restriction(wlc->cmi,
					apcfg->current_bss->chanspec, tgt_chanspec)) {
					tgt_chanspec = wlc_default_chanspec_by_band(wlc->cmi,
							BAND_2G_INDEX);
				}
#endif // endif
				wlc_csa_do_switch(wlc->csa, apcfg, tgt_chanspec);
				wlc_11h_set_spect_state(wlc->m11h, apcfg,
					NEED_TO_SWITCH_CHANNEL, NEED_TO_SWITCH_CHANNEL);
				ap_do_csa = TRUE;
				WL_ASSOC(("wl%d.%d: apply channel switch\n", wlc->pub->unit,
					apidx));
			}
		}
	}

	return ap_do_csa;
}
#endif /* AP */

static void
wlc_ibss_set_bssid_hw(wlc_bsscfg_t *cfg, struct ether_addr *bssid)
{
	struct ether_addr save;

	save = cfg->BSSID;
	cfg->BSSID = *bssid;
	wlc_set_bssid(cfg);
	cfg->BSSID = save;
}

static void
wlc_join_noscb(wlc_info_t *wlc, wlc_bsscfg_t *cfg, wlc_assoc_t *as)
{
	/* No SCB: move state to fake an auth anyway so FSM moves */
	WL_ASSOC(("wl%d.%d: can't create scb\n", WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg)));
	wlc_assoc_change_state(cfg, AS_SENT_AUTH_1);
	as->bss_retries = as->retry_max;
	wl_del_timer(wlc->wl, as->timer);
	wl_add_timer(wlc->wl, as->timer, WECA_AUTH_TIMEOUT + 10, 0);
}

static void *
wlc_join_assoc_start(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct scb *scb,
	wlc_bss_info_t *bi, bool associated)
{
	uint wsec_cfg;
	uint wsec_scb;

	bool allow_reassoc = TRUE;
	BCM_REFERENCE(wsec_cfg);
	ASSERT(cfg != NULL);
	ASSERT(scb != NULL);

	wsec_cfg = cfg->wsec;

	/* Update SCB crypto based on highest WSEC settings */
	if (WSEC_AES_ENABLED(wsec_cfg))
		wsec_scb = AES_ENABLED;
	else if (WSEC_TKIP_ENABLED(wsec_cfg))
		wsec_scb = TKIP_ENABLED;
	else if (WSEC_WEP_ENABLED(wsec_cfg))
		wsec_scb = WEP_ENABLED;
	else
		wsec_scb = 0;

	scb->wsec = wsec_scb;

	/* Based on wsec for the link, update AMPDU feature in the transmission path
	 * By spec, 11n device can send AMPDU only with Open or CCMP crypto
	 */
	if (N_ENAB(wlc->pub)) {
		if ((wsec_scb == WEP_ENABLED || wsec_scb == TKIP_ENABLED) &&
		    SCB_AMPDU(scb)) {
#ifdef WLAMPDU
			wlc_scb_ampdu_disable(wlc, scb);
#endif // endif
		}
#ifdef MFP
		else if (WLC_MFP_ENAB(wlc->pub) && SCB_MFP(scb)) {
#ifdef WLAMPDU
			wlc_scb_ampdu_disable(wlc, scb);
#endif // endif
		}
#endif /* MFP */
		else if (SCB_AMPDU(scb)) {
#ifdef WLAMPDU
			wlc_scb_ampdu_enable(wlc, scb);
#endif /* WLAMPDU */
		}
	}

#if defined(WLFBT)
	/* For FT cases, send out assoc request when
	* - doing initial FT association (roaming from a different security type)
	* - FT AKM suite for current and next association do not match
	* - there is link down or disassoc, as indicated by null current_bss->BSSID
	* - switching to a different FT mobility domain
	* Send out reassoc request
	* - when FT AKM suite for the current and next association matches
	* - for all non-FT associations
	*/

	if ((!(cfg->WPA_auth & WPA2_AUTH_FT)) && (!(bi->wpa2.flags & RSN_FLAGS_MFPR))) {
		allow_reassoc = TRUE;
	}
	else if (BSSCFG_IS_FBT(cfg) && !ETHER_ISNULLADDR(&cfg->current_bss->BSSID) &&
		wlc_fbt_is_fast_reassoc(wlc->fbt, cfg, bi)) {
		allow_reassoc = TRUE;
	} else
		allow_reassoc = FALSE;
#endif /* WLFBT */

	if (wlc->vhti && wlc_vht_is_omn_enabled(wlc->vhti, scb)) {
		/* disallow reassociation since oper mode changes must be redone */
		allow_reassoc = FALSE;
		wlc_vht_disable_scb_oper_mode(wlc->vhti, scb);
	}

	return wlc_sendassocreq(wlc, bi, scb, associated && allow_reassoc);
} /* wlc_join_assoc_start */

static void
wlc_join_BSS_limit_caps(wlc_bsscfg_t *cfg, wlc_bss_info_t *bi)
{
	wlc_info_t *wlc = cfg->wlc;
	bool ht_wsec = TRUE;

	BCM_REFERENCE(wlc);

	if (bi->flags & WLC_BSS_HT) {
		/* HT and TKIP(WEP) cannot be used for HT STA. When AP advertises HT and TKIP */
		/* (or WEP)  only,	downgrade to leagacy STA */

		if (WSEC_TKIP_ENABLED(cfg->wsec)) {
			if (!WSEC_AES_ENABLED(cfg->wsec) || (!UCAST_AES(&bi->wpa) &&
				!UCAST_AES(&bi->wpa2))) {
				if (wlc_ht_get_wsec_restriction(wlc->hti) &
						WLC_HT_TKIP_RESTRICT)
					ht_wsec = FALSE;
			}
		} else if (WSEC_WEP_ENABLED(cfg->wsec)) {
				/* In WPS Exchage, Win7/Win8 will set WEP as default cipher no
				* matter what ciphers AP used. In this case, downgrading to
				* leagacy STA will prevent our association to 11n only AP.
				*/
				if (!(bi->wpa.flags & RSN_FLAGS_SUPPORTED) &&
					!(bi->wpa2.flags & RSN_FLAGS_SUPPORTED))
				{
					if (wlc_ht_get_wsec_restriction(wlc->hti) &
						WLC_HT_WEP_RESTRICT)
						ht_wsec = FALSE;
				}
		}
		/* safe mode: if AP is HT capable, downgrade to legacy mode because */
		/* NDIS won't associate to HT AP */
		if (!ht_wsec) {
			bi->flags &= ~(WLC_BSS_HT | WLC_BSS_40INTOL | WLC_BSS_SGI_20 |
				WLC_BSS_SGI_40 | WLC_BSS_40MHZ);
			bi->chanspec = CH20MHZ_CHSPEC(
				wf_chspec_ctlchan(bi->chanspec));

			/* disable VHT and SGI80 also */
			bi->flags2 &= ~(WLC_BSS_VHT | WLC_BSS_SGI_80);
			bi->flags3 &= ~(WLC_BSS3_HE);
		}
	}
}

static void
wlc_join_ibss(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	wlc_bss_info_t *target_bss = cfg->target_bss;
	/* IBSS join */
	int beacon_wait_time;

#ifdef WLMCNX
	if (MCNX_ENAB(wlc->pub)) {
		cfg->current_bss->dtim_period = 1;
		wlc_mcnx_bss_upd(wlc->mcnx, cfg, TRUE);
	}
#endif // endif
	/*
	 * nothing more to do from a protocol point of view...
	 * join the BSS when the next beacon is received...
	 */
	WL_ASSOC(("wl%d: JOIN: IBSS case, wait for a beacon\n", WLCWLUNIT(wlc)));
	/* configure the BSSID addr for STA w/o promisc beacon */
	wlc_ibss_set_bssid_hw(cfg, &target_bss->BSSID);
	wlc_assoc_change_state(cfg, AS_WAIT_RCV_BCN);

	/* Start the timer for beacon receive timeout handling to ensure we move
	 * out of AS_WAIT_RCV_BCN state in case we never receive the first beacon
	 */
	wlc_assoc_timer_del(wlc, cfg);
	beacon_wait_time = as->recreate_bi_timeout *
		(target_bss->beacon_period * DOT11_TU_TO_US) / 1000;

	/* Validate beacon_wait_time, usually it will be 1024 with beacon interval 100TUs */
	beacon_wait_time = beacon_wait_time > 10000 ? 10000 :
		beacon_wait_time < 1000 ? 1024 : beacon_wait_time;

	wl_add_timer(wlc->wl, as->timer, beacon_wait_time, 0);

	wlc_bsscfg_up(wlc, cfg);

	/* notifying interested parties of the state... */
	wlc_bss_assoc_state_notif(wlc, cfg, as->type, AS_WAIT_RCV_BCN);
}
#ifdef WL_CLIENT_SAE
static int wlc_assoc_start_extauth(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
        struct ether_addr *bssid, int auth_alg)
{
	wl_ext_auth_evt_t ext_auth_event;
	wlc_assoc_t *as = cfg->assoc;

	if (BSSCFG_AP(cfg))
		return BCME_NOTSTA;

	if (auth_alg != WL_AUTH_SAE_KEY)
		return BCME_UNSUPPORTED;

	if (!bssid && !cfg->SSID)
		return BCME_ERROR;

	/* Send an event to upper layer to start External authentication */
	WL_ASSOC(("wl%d: %s: External Auth Initiated\n",
			wlc->pub->unit, __FUNCTION__));

	wlc_assoc_change_state(cfg, AS_START_EXT_AUTH);
	memcpy(ext_auth_event.ssid.SSID, cfg->SSID, cfg->SSID_len);
	ext_auth_event.ssid.SSID_len = cfg->SSID_len;
	memcpy(&ext_auth_event.bssid, bssid, ETHER_ADDR_LEN);

	wlc_bss_mac_event(wlc, cfg, WLC_E_START_AUTH, bssid,
			0, 0, 0,  &ext_auth_event, sizeof(ext_auth_event));
	wl_add_timer(wlc->wl, as->timer, WLC_EXT_AUTH_TIMEOUT, 0);

	return BCME_OK;
}
#endif /* WL_CLIENT_SAE */

/* send FC_AUTH frame, handle error situations, and feedback to the caller */
static bool
wlc_assoc_sendauth(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	struct ether_addr *ea, struct ether_addr *bssid, struct scb *scb,
	int auth_alg, int auth_seq, int auth_status,
	uint8 *challenge_text, bool short_preamble, uint state)
{
	void *pkt;
	wlc_assoc_t *as = cfg->assoc;

	wlc_assoc_change_state(cfg, state);

	pkt = wlc_sendauth(cfg, ea, bssid, scb, auth_alg, auth_seq, auth_status,
	                   challenge_text, short_preamble, NULL, NULL,
	                   wlc_auth_tx_complete, (void *)(uintptr)cfg->ID);
	if (pkt == NULL) {
		/* the tx completion callback must have been fired due to
		 * tx queueing failures which changed the FSM state...
		 * so let's ignore the rest.
		 */
		if (as->state != state) {
			WL_ASSOC(("wl%d: %s: Failed to send FC_AUTH frame.\n",
			          wlc->pub->unit, __FUNCTION__));
			return FALSE;
		}
		/* we may have temporary memory shortage so come back in a bit */
		wl_add_timer(wlc->wl, as->timer, WECA_AUTH_TIMEOUT + 10, 0);
	}

	return TRUE;
}

static void
wlc_join_BSS_sendauth(wlc_bsscfg_t *cfg, wlc_bss_info_t *bi,
	struct scb *scb, uint8 mcsallow)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	wlc_roam_t *roam = cfg->roam;
	int auth = cfg->auth_atmptd;
	BCM_REFERENCE(mcsallow);

	WL_ASSOC(("wl%d: JOIN: BSS case, sending AUTH REQ alg=%d ...(%d)\n",
	          WLCWLUNIT(wlc), auth, as->bss_retries));
	wlc_scb_setstatebit(wlc, scb, PENDING_AUTH | PENDING_ASSOC);

	/* if we were roaming, mark the old BSSID so we don't thrash back to it */
	if (as->type == AS_ROAM &&
	    (roam->reason == WLC_E_REASON_MINTXRATE ||
	     roam->reason == WLC_E_REASON_TXFAIL)) {
		uint idx;
		for (idx = 0; idx < roam->cache_numentries; idx++) {
			if (!bcmp(&roam->cached_ap[idx].BSSID,
			          &cfg->current_bss->BSSID,
			          ETHER_ADDR_LEN)) {
				roam->cached_ap[idx].time_left_to_next_assoc =
				        ROAM_REASSOC_TIMEOUT;
				WL_ASSOC(("wl%d: %s: Marking current AP as "
				          "unavailable for reassociation for %d "
				          "seconds due to roam reason %d\n",
				          wlc->pub->unit, __FUNCTION__,
				          ROAM_REASSOC_TIMEOUT, roam->reason));
				roam->thrash_block_active = TRUE;
			}
		}
	}

#ifdef WL_CLIENT_SAE
	if (BSSCFG_EXT_AUTH(cfg) && auth == DOT11_SAE) {
		if (wlc_assoc_start_extauth(wlc, cfg, &bi->BSSID, auth) != BCME_OK) {
			WL_ASSOC(("wl%d: JOIN: External auth Trigger Failed",
					WLCWLUNIT(wlc)));
		}
		return;
	}
#endif /* WL_CLIENT_SAE */
	(void)wlc_assoc_sendauth(wlc, cfg, &scb->ea, &bi->BSSID, scb,
	                   auth, 1, DOT11_SC_SUCCESS, NULL,
	                   ((bi->capability & DOT11_CAP_SHORT) != 0),
	                   AS_SENT_AUTH_1);
}

#ifdef WL11AX
typedef struct {
	he_cap_ie_t **cap_ie;
	he_op_ie_t **op_ie;
} wlc_parse_assoc_resp_t;

static int
_wlc_parse_he_cap_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	he_cap_ie_t **cap_ie = ctx;

	*cap_ie = (he_cap_ie_t *)data->ie;

	return BCME_OK;
}

static int
_wlc_parse_he_op_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	he_op_ie_t **op_ie = ctx;

	*op_ie = (he_op_ie_t *)data->ie;

	return BCME_OK;
}

static int
wlc_parse_assoc_resp(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint16 ft,
	uint8 *ies, uint ies_len, wlc_parse_assoc_resp_t *parsp)
{
	/* tags must be sorted in ascending order */
	wlc_iem_tag_t parse_tag[] = {
		DOT11_MNG_HE_CAP_ID,
		DOT11_MNG_HE_OP_ID,
	};
	wlc_iem_pe_t parse_cb[] = {
		{_wlc_parse_he_cap_ie, parsp->cap_ie},
		{_wlc_parse_he_op_ie, parsp->op_ie},
	};

	BCM_REFERENCE(wlc);

	ASSERT(ARRAYSIZE(parse_tag) == ARRAYSIZE(parse_cb));

	return wlc_ieml_parse_frame(cfg, ft, parse_tag, TRUE,
		parse_cb, ARRAYSIZE(parse_cb), NULL, NULL, ies, ies_len);
}
#endif /* WL11AX */

/* apply BSS configuration/capabilities to scb */

#define JOIN_BSS_FLAG_HT_CAP	(1<<0)
#define JOIN_BSS_FLAG_VHT_CAP	(1<<1)
#define JOIN_BSS_FLAG_HE_CAP	(1<<2)

static void
wlc_join_BSS_select(wlc_bsscfg_t *cfg,
	chanspec_t chanspec, uint8 mcsallow, uint flags)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	wlc_bss_info_t *target_bss = cfg->target_bss;
	struct scb *scb;

	/* BSS join */

#ifdef WLP2P
	/* P2P: configure the BSS TSF, NoA, CTWindow for early adoption */
	if (BSS_P2P_ENAB(wlc, cfg))
		wlc_p2p_adopt_bss(wlc->p2p, cfg, target_bss);
#endif // endif

	/* running out of scbs is fatal here */
	if (!(scb = wlc_scblookup(wlc, cfg, &target_bss->BSSID))) {
		WL_ERROR(("wl%d: %s: out of scbs\n", wlc->pub->unit, __FUNCTION__));
		wlc_join_noscb(wlc, cfg, as);
		return;
	}

	/* the cap and additional IE are only in the bcn/prb response pkts,
	 * when joining a bss parse the bcn_prb to check for these IE's.
	 * else check if SCB state needs to be cleared if AP might have changed its mode
	 */
	{
#ifdef WL11N
	ht_cap_ie_t *cap_ie = NULL;
	ht_add_ie_t *add_ie = NULL;

	if (flags & JOIN_BSS_FLAG_HT_CAP) {
		obss_params_t *obss_ie = NULL;
		uint len = target_bss->bcn_prb_len - sizeof(struct dot11_bcn_prb);
		uint8 *parse = (uint8*)target_bss->bcn_prb + sizeof(struct dot11_bcn_prb);

		/* extract ht cap and additional ie */
		cap_ie = wlc_read_ht_cap_ies(wlc, parse, len);
		add_ie = wlc_read_ht_add_ies(wlc, parse, len);
		if (COEX_ENAB(wlc))
			obss_ie = wlc_ht_read_obss_scanparams_ie(wlc, parse, len);
		wlc_ht_update_scbstate(wlc->hti, scb, cap_ie, add_ie, obss_ie);
	} else if (SCB_HT_CAP(scb) &&
	           ((target_bss->flags & WLC_BSS_HT) != WLC_BSS_HT)) {
		wlc_ht_update_scbstate(wlc->hti, scb, NULL, NULL, NULL);
	}
#endif /* WL11N */
#ifdef WL11AC
	if (flags & JOIN_BSS_FLAG_VHT_CAP) {
		vht_cap_ie_t *vht_cap_ie_p;
		vht_cap_ie_t vht_cap_ie;

		vht_op_ie_t *vht_op_ie_p;
		vht_op_ie_t vht_op_ie;

		uint8 *prop_tlv = NULL;
		int prop_tlv_len = 0;
		uint8 vht_ratemask = 0;
		int target_bss_band = CHSPEC2WLC_BAND(chanspec);

		uint len = target_bss->bcn_prb_len - sizeof(struct dot11_bcn_prb);
		uint8 *parse = (uint8*)target_bss->bcn_prb + sizeof(struct dot11_bcn_prb);

		/*
		 * Extract ht cap and additional ie
		 * Encapsulated Prop VHT IE appears if we are running VHT in 2.4G or
		 * the extended rates are enabled
		 */
		if (BAND_2G(target_bss_band) || WLC_VHT_FEATURES_RATES(wlc->pub)) {
			prop_tlv = wlc_read_vht_features_ie(wlc->vhti, parse, len,
					&vht_ratemask, &prop_tlv_len, target_bss);
		}

		if  (prop_tlv) {
			vht_cap_ie_p = wlc_read_vht_cap_ie(wlc->vhti, prop_tlv,
					prop_tlv_len, &vht_cap_ie);
			vht_op_ie_p = wlc_read_vht_op_ie(wlc->vhti, prop_tlv,
					prop_tlv_len, &vht_op_ie);
		} else {
			vht_cap_ie_p = wlc_read_vht_cap_ie(wlc->vhti,
					parse, len, &vht_cap_ie);
			vht_op_ie_p = wlc_read_vht_op_ie(wlc->vhti,
					parse, len, &vht_op_ie);
		}
		wlc_vht_update_scb_state(wlc->vhti, target_bss_band, scb, cap_ie,
				vht_cap_ie_p, vht_op_ie_p, vht_ratemask);
	} else if (SCB_VHT_CAP(scb) &&
	           ((target_bss->flags2 & WLC_BSS_VHT) != WLC_BSS_VHT)) {
		wlc_vht_update_scb_state(wlc->vhti,
				CHSPEC2WLC_BAND(chanspec), scb, NULL, NULL, NULL, 0);
	}
#endif /* WL11AC */
#ifdef WL11AX
	if (flags & JOIN_BSS_FLAG_HE_CAP) {
		he_cap_ie_t *he_cap_ie = NULL;
		he_op_ie_t *he_op_ie = NULL;
		uint len = target_bss->bcn_prb_len - sizeof(struct dot11_bcn_prb);
		uint8 *parse = (uint8*)target_bss->bcn_prb + sizeof(struct dot11_bcn_prb);
		wlc_parse_assoc_resp_t parsp = {&he_cap_ie, &he_op_ie};

		wlc_parse_assoc_resp(wlc, cfg,
		                     as->type == AS_ROAM ? FC_ASSOC_RESP : FC_REASSOC_RESP,
		                     parse, len, &parsp);

		wlc_he_update_scb_state(wlc->hei, CHSPEC2WLC_BAND(chanspec), scb,
		                        he_cap_ie, he_op_ie);
	} else if (SCB_HE_CAP(scb) &&
	           ((target_bss->flags3 & WLC_BSS3_HE) != WLC_BSS3_HE)) {
		wlc_he_update_scb_state(wlc->hei, CHSPEC2WLC_BAND(chanspec), scb,
		                        NULL, NULL);
	}
#endif /* WL11AX */
	}

	/* just created or assigned an SCB for the AP, flag as MYAP */
	ASSERT(!(SCB_ASSOCIATED(scb) && !(scb->flags & SCB_MYAP)));
	scb->flags |= SCB_MYAP;
	scb->flags &= ~SCB_SHORTPREAMBLE;
	if ((target_bss->capability & DOT11_CAP_SHORT) != 0)
		scb->flags |= SCB_SHORTPREAMBLE;

	/* replace any old scb rateset with new target rateset */
#ifdef WLP2P
	if (BSS_P2P_ENAB(wlc, cfg))
		wlc_rateset_filter(&target_bss->rateset /* src */, &scb->rateset /* dst */,
			FALSE, WLC_RATES_OFDM, RATE_MASK, mcsallow);
	else
#endif /* WLP2P */
		wlc_rateset_filter(&target_bss->rateset /* src */, &scb->rateset /* dst */, FALSE,
	                   WLC_RATES_CCK_OFDM, RATE_MASK, mcsallow);

	wlc_scb_ratesel_init(wlc, scb);
	wlc_assoc_timer_del(wlc, cfg);

#if defined(WLFBT)
	if (as->state == AS_RECV_FTRES) {
		wlc_scb_clearstatebit(wlc, scb, PENDING_AUTH | PENDING_ASSOC);
		wlc_scb_setstatebit(wlc, scb, AUTHENTICATED);
		wlc_assoc_continue_post_auth1(cfg, scb);
	} else
#endif /* WLFBT */
	if (!(scb->state & (PENDING_AUTH | PENDING_ASSOC))) {
	/* send authentication request */
		wlc_join_BSS_sendauth(cfg, target_bss, scb, mcsallow);
	}
}

void
wlc_join_bss(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	chanspec_t chanspec;
	wlcband_t *band;
	wlc_msch_req_param_t req;
	uint64 start_time;
	int err;
	wlc_bss_info_t *bi;
	bool force_chchange = FALSE;

	BCM_REFERENCE(force_chchange);

	bi = wlc_assoc_get_join_target(wlc, 0);
	ASSERT(bi != NULL);

#if defined(WLRSDB)
	if (RSDB_ENAB(wlc->pub)) {
		/* Try to see if a mode switch is required */
		if (wlc_rsdb_assoc_mode_change(&cfg, bi) == BCME_NOTREADY) {
			return;
		}
		/* Replace the current WLC pointer with the probably new wlc */
		wlc = cfg->wlc;
		as = cfg->assoc;
	}
#endif // endif

#ifdef WL_PWRSTATS
		if (PWRSTATS_ENAB(wlc->pub)) {
			/* Update connect time for primary infra sta only */
			if (cfg == wlc->cfg) {
				wlc_pwrstats_connect_start(wlc->pwrstats);
			}
		}
#endif /* WL_PWRSTATS */

	band = wlc->bandstate[CHSPEC_IS2G(bi->chanspec) ? BAND_2G_INDEX : BAND_5G_INDEX];

	/* prune invalid chanspec based on STA capibility */
	if ((chanspec = wlc_max_valid_bw_chanspec(wlc, band, cfg, bi->chanspec)) == INVCHANSPEC) {
		WL_ASSOC(("wl%d: JOIN: Skipping invalid chanspec(0x%x):",
			WLCWLUNIT(wlc), bi->chanspec));
		return;
	}

	/*
	 * Based on requirement that is, if target channel is found restricted to
	 * country setting, then update the internal country usage to auto_country_default.
	 */
	if (WLC_ASSOC_IS_PROHIBITED_CHANSPEC(wlc, chanspec) == TRUE) {
		char country_abbrev[WLC_CNTRY_BUF_SZ];
		memset(country_abbrev, 0, sizeof(country_abbrev));
		memcpy(country_abbrev, wlc_11d_get_autocountry_default(wlc->m11d),
			sizeof(country_abbrev));

		WL_ASSOC(("wl%d: setting regulatory information from built-in "
	           "country \"%s\" associating AP on restricted chanspec 0x%x\n",
	           wlc->pub->unit, country_abbrev, chanspec));
		wlc_set_countrycode(wlc->cmi, country_abbrev);
		/* Force channel change update to PHY. This handles to clear the
		 * quiet bit in the channel vector.
		 */
		force_chchange = TRUE;
	}
#ifdef AP
	if (wlc_join_check_ap_need_csa(wlc, cfg, bi->chanspec, AS_WAIT_FOR_AP_CSA)) {
		WL_ASSOC(("wl%d.%d: JOIN: %s delayed due to ap active, wait for ap CSA\n",
				WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg), __FUNCTION__));
		/* to prevent race condition in case of STA + SOFTAP single channel concurrency */
		wlc_sta_timeslot_unregister(cfg);
		return;
	}
#endif /* AP */

	wlc_block_datafifo(wlc, DATA_BLOCK_JOIN, DATA_BLOCK_JOIN);

	/* Change the radio channel to match the target_bss.
	 * FBToverDS fallback: change the channel while switching to FBT over-the-air if no response
	 * received for FT Request frame from the current AP or if current AP
	 * is not reachable.
	 */
	start_time = msch_future_time(wlc->msch_info, WLC_MSCH_CHSPEC_CHNG_START_TIME_US);
	req.req_type = MSCH_RT_START_FLEX;
	req.flags = 0;
	req.duration = WLC_ASSOC_TIME_US;
	req.flex.bf.min_dur = MSCH_MIN_FREE_SLOT;
	req.interval = 0;
	req.priority = MSCH_RP_ASSOC_SCAN;
	req.start_time_l = (uint32)(start_time);
	req.start_time_h = (uint32) (start_time >> 32);

	if (as->req_msch_hdl) {
		wlc_txqueue_end(wlc, cfg, NULL);
		wlc_msch_timeslot_unregister(wlc->msch_info, &as->req_msch_hdl);
	}

	if (force_chchange || wlc_quiet_chanspec(wlc->cmi, chanspec)) {
		/* clear the quiet bit on our channel and un-quiet */
		wlc_clr_quiet_chanspec(wlc->cmi, chanspec);
		wlc_mute(wlc, OFF, 0);
	}

	if ((err = wlc_msch_timeslot_register(wlc->msch_info, &chanspec, 1,
		wlc_assoc_chanspec_change_cb, cfg, &req, &as->req_msch_hdl))
		== BCME_OK) {
		wlc_msch_set_chansw_reason(wlc->msch_info, as->req_msch_hdl, CHANSW_ASSOC);
		WL_INFORM(("wl%d: Channel timeslot request success\n", wlc->pub->unit));
	} else {
		WL_INFORM(("wl%d: Channel timeslot request failed error %d\n",
			wlc->pub->unit, err));
		ASSERT(0);
	}
}

/** MSCH join register callback function for normal chanspec */
static int
wlc_assoc_chanspec_change_cb(void* handler_ctxt, wlc_msch_cb_info_t *cb_info)
{
	wlc_bsscfg_t *cfg =  (wlc_bsscfg_t *) handler_ctxt;
	chanspec_t chanspec = cb_info->chanspec;
	wlc_info_t *wlc = cfg->wlc;
	wlc_bss_info_t *bi;
	wlc_assoc_t *as = cfg->assoc;

	bi = wlc_assoc_get_join_target(wlc, 0);
	ASSERT(bi != NULL);

	WL_INFORM(("%s chspec 0x%x type %d type_specific %p\n", __FUNCTION__,
		cb_info->chanspec, cb_info->type, OSL_OBFUSCATE_BUF(cb_info->type_specific)));

	if (cb_info->type & MSCH_CT_ON_CHAN) {
		wlc_clr_quiet_chanspec(wlc->cmi, chanspec);
		wlc_mute(wlc, OFF, 0);
		wlc_txqueue_start(wlc, cfg, cb_info->chanspec, NULL);
		wlc_join_BSS_post_ch_switch(cfg, bi, TRUE, chanspec);
	}

	/* SLOT/REQ is complete, make the handle NULL */
	if ((cb_info->type & MSCH_CT_SLOT_END) ||
		(cb_info->type & MSCH_CT_REQ_END)) {
		wlc_txqueue_end(wlc, cfg, NULL);
		as->req_msch_hdl = NULL;
	}

	return BCME_OK;
}

void
wlc_assoc_update_dtim_count(wlc_bsscfg_t *cfg, uint16 dtim_count)
{
	cfg->assoc->dtim_count = dtim_count;
	return;
}

static void
wlc_join_BSS_post_ch_switch(wlc_bsscfg_t *cfg, wlc_bss_info_t *bi, bool ch_changed,
           chanspec_t chanspec)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	wlc_bss_info_t *target_bss = cfg->target_bss;
	bool cck_only;
	wlc_rateset_t *rs_hw;
	uint flags = 0;
	uint8 mcsallow = 0;
	wlcband_t *band;
	wlc_phy_t *pi = WLC_PI(wlc);
	int err;

	wlc_stf_arb_req_update(wlc, cfg, WLC_STF_ARBITRATOR_REQ_STATE_RXTX_ACTIVE);

	band = wlc->bandstate[CHSPEC_IS2G(bi->chanspec) ? BAND_2G_INDEX : BAND_5G_INDEX];

	/* Find HW default ratset for the band (according to target channel) */
	rs_hw = &band->hw_rateset;

	/* select target bss info */
	bcopy((char*)bi, (char*)target_bss, sizeof(wlc_bss_info_t));

	/* fixup bsscfg type based on chosen target_bss */
	if ((err = wlc_assoc_fixup_bsscfg_type(wlc, cfg, bi, TRUE)) != BCME_OK) {

		return;
	}

	/* update the target_bss->chanspec after possibe narrowing to 20MHz */
	target_bss->chanspec = chanspec;

	/* Keep only CCK if gmode == GMODE_LEGACY_B */
	if (BAND_2G(band->bandtype) && band->gmode == GMODE_LEGACY_B)
		cck_only = TRUE;
	else
		cck_only = FALSE;

	/* Make sure to verify AP also advertises WMM IE before updating HT IE
	 * Some AP's advertise HT IE in beacon without WMM IE, but AP's behaviour
	 * is unpredictable on using HT/AMPDU.
	 */
	if (target_bss->flags & WLC_BSS_WME) {
		flags |= ((target_bss->flags & WLC_BSS_HT) && BSS_N_ENAB(wlc, cfg)) ?
		        JOIN_BSS_FLAG_HT_CAP : 0;
		if (flags & JOIN_BSS_FLAG_HT_CAP)
			mcsallow |= WLC_MCS_ALLOW;
		flags |= ((target_bss->flags2 & WLC_BSS_VHT) && BSS_VHT_ENAB(wlc, cfg)) ?
		        JOIN_BSS_FLAG_VHT_CAP : 0;
		if (flags & JOIN_BSS_FLAG_VHT_CAP)
			mcsallow |= WLC_MCS_ALLOW_VHT;
#ifdef WL11AX
		flags |= ((target_bss->flags3 & WLC_BSS3_HE) &&
			BSS_HE_ENAB_BAND(wlc, wlc->band->bandtype, cfg)) ?
		        JOIN_BSS_FLAG_HE_CAP : 0;
		if (flags & JOIN_BSS_FLAG_HE_CAP) {
			mcsallow |= WLC_MCS_ALLOW_HE;
		}
#endif /* WL11AX */
	}

	if (WLPROPRIETARY_11N_RATES_ENAB(wlc->pub) &&
	    wlc->pub->ht_features != WLC_HT_FEATURES_PROPRATES_DISAB)
		mcsallow |= WLC_MCS_ALLOW_PROP_HT;

	if ((mcsallow & WLC_MCS_ALLOW_VHT) &&
		WLC_VHT_FEATURES_GET(wlc->pub, WL_VHT_FEATURES_1024QAM))
		mcsallow |= WLC_MCS_ALLOW_1024QAM;

	wlc_rateset_filter(&target_bss->rateset /* src */, &target_bss->rateset /* dst */,
	                   FALSE, cck_only ? WLC_RATES_CCK : WLC_RATES_CCK_OFDM,
	                   RATE_MASK_FULL, cck_only ? 0 : mcsallow);

	/* apply default rateset to invalid rateset */
	if (!wlc_rate_hwrs_filter_sort_validate(&target_bss->rateset /* [in+out] */,
		rs_hw /* [in] */, TRUE,
		wlc->stf->op_txstreams)) {
		WL_RATE(("wl%d: %s: invalid rateset in target_bss. bandunit 0x%x phy_type 0x%x "
			"gmode 0x%x\n", wlc->pub->unit, __FUNCTION__, band->bandunit,
			band->phytype, band->gmode));
#ifdef BCMDBG
		wlc_rateset_show(wlc, &target_bss->rateset, &target_bss->BSSID);
#endif // endif

		wlc_rateset_default(wlc, &target_bss->rateset, rs_hw, band->phytype,
			band->bandtype, cck_only, RATE_MASK_FULL,
			cck_only ? 0 : wlc_get_mcsallow(wlc, NULL),
			CHSPEC_WLC_BW(target_bss->chanspec), wlc->stf->op_rxstreams);
	}

#ifdef WLP2P
	if (BSS_P2P_ENAB(wlc, cfg)) {
		wlc_rateset_filter(&target_bss->rateset, &target_bss->rateset, FALSE,
		                   WLC_RATES_OFDM, RATE_MASK_FULL, wlc_get_mcsallow(wlc, cfg));
	}
#endif // endif

#ifdef BCMDBG
	wlc_rateset_show(wlc, &target_bss->rateset, &target_bss->BSSID);
#endif // endif

	wlc_rate_lookup_init(wlc, &target_bss->rateset);

	/* XXX Call Cals unconditionally on join BSS for ACPHY
	 * this is needed as the Calibration coeffs are zeroed out
	 * during a scan if there is a band switch. Dont do this during
	 * assoc recreation since full phy cal is already done
	 */
	/* Skip phy calibration here for FBT over-the DS. This is mainly for
	 * reducing transition time for over-the-DS case where channel switching
	 * happens only after receiving FT response.
	 */
	if (!(cfg->BSS && as->type == AS_RECREATE && (as->flags & AS_F_SPEEDY_RECREATE)) &&
	    /* skip cal for PSTAs; doing it on primary is good enough */
	    !BSSCFG_PSTA(cfg)) {
		WL_ASSOC(("wl%d: Call full phy cal from join_BSS\n",
		          WLCWLUNIT(wlc)));
		wlc_full_phy_cal(wlc, cfg, PHY_PERICAL_JOIN_BSS);
		wlc_phy_interference_set(pi, TRUE);
		wlc_phy_acimode_noisemode_reset(pi, chanspec, FALSE, TRUE, FALSE);
	}

	/* attempt to associate with the selected BSS */
	if (target_bss->bss_type == DOT11_BSSTYPE_INDEPENDENT) {
		wlc_join_ibss(cfg);
	} else {
		wlc_join_BSS_select(cfg, chanspec, mcsallow, flags);
	}

	as->bss_retries++;
} /* wlc_join_BSS_post_ch_switch */

void
wlc_assoc_unregister_pcb_on_timeout(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	wlc_assoc_t *as = cfg->assoc;

	if (as->state == AS_SENT_AUTH_1 ||
		as->state == AS_SENT_AUTH_3 ||
		as->state == AS_SENT_FTREQ) {
		/* unregister for auth_tx_complete */
		wlc_pcb_fn_find(wlc->pcb, wlc_auth_tx_complete, (void *)(uintptr)cfg->ID, TRUE);
	}

	/* Note: add any other callback to be unregistered here depending on as->state */
}

static void
wlc_assoc_timeout(void *arg)
{
	wlc_bsscfg_t *cfg = (wlc_bsscfg_t *)arg;
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	wlc_bss_info_t *target_bss = cfg->target_bss;
	bool recreate_success = FALSE;
	bool delete_assoc_timeslot = TRUE;

	WL_TRACE(("wl%d: wlc_timer", wlc->pub->unit));

	if (!wlc->pub->up)
		return;

	if (DEVICEREMOVED(wlc)) {
		WL_ERROR(("wl%d: %s: dead chip\n", wlc->pub->unit, __FUNCTION__));
		wl_down(wlc->wl);
		return;
	}

	/*
	* Timed out but no way to free up the Txd packet if waiting for
	* txstatus. Un-register pkt callback instead and let the packet
	* be silently freed without disturbing next course of assoc states
	*/
	wlc_assoc_unregister_pcb_on_timeout(wlc, cfg);

	/* We need to unblock the datatfifo which was blocked in
	 * wlc_join_recreate fucntion, even if there was not a successful
	 * association, so that we dont block the other interface from
	 * transmitting
	 */
	wlc_block_datafifo(wlc, DATA_BLOCK_JOIN, 0);

	if (ASSOC_RECREATE_ENAB(wlc->pub) &&
	    as->type == AS_RECREATE && as->state == AS_RECREATE_WAIT_RCV_BCN) {
		/* We reached the timeout waiting for a beacon from our former AP so take
		 * further action to reestablish our association.
		 */
		wlc_assoc_recreate_timeout(cfg);
	} else if (ASSOC_RECREATE_ENAB(wlc->pub) &&
	           as->type == AS_RECREATE && as->state == AS_ASSOC_VERIFY) {
		/* reset the association state machine */
		as->type = AS_NONE;
		wlc_assoc_change_state(cfg, AS_IDLE);
#ifdef WLPFN
		if (WLPFN_ENAB(wlc->pub))
			wl_notify_pfn(wlc);
#endif /* WLPFN */
		recreate_success = TRUE;
#ifdef NLO
		wlc_bss_mac_event(wlc, cfg, WLC_E_ASSOC_RECREATED, NULL,
			WLC_E_STATUS_SUCCESS, 0, 0, 0, 0);
#endif /* NLO */
#if defined(WLFBT)
	} else if (as->state == AS_SENT_FTREQ) {
		wlc_join_bss_start(cfg);
		delete_assoc_timeslot = FALSE;
	} else if (as->state == AS_RECV_FTRES) {
		wlc_join_bss(cfg);
		delete_assoc_timeslot = FALSE;
#endif /* WLFBT */
	} else if ((as->state == AS_JOIN_START) || (as->state == AS_WAIT_TX_DRAIN)) {
		wlc_join_bss(cfg);
		delete_assoc_timeslot = FALSE;
	} else if ((as->state == AS_SENT_AUTH_1) || (as->state == AS_SENT_AUTH_3)) {
		wlc_auth_complete(cfg, WLC_E_STATUS_TIMEOUT, &target_bss->BSSID, 0, cfg->auth);

		/* going to retry sending AUTH_REQ. don't delete the assoc timeslot */

		delete_assoc_timeslot = FALSE;
#if defined(WL_CLIENT_SAE)
	} else if ((as->state == AS_SENT_AUTH_UP) || (as->state == AS_START_EXT_AUTH)) {

		wlc_auth_complete(cfg, WLC_E_STATUS_FAIL, &target_bss->BSSID, 0, cfg->auth);
		/* going to retry sending AUTH_REQ. don't delete the assoc timeslot */

		delete_assoc_timeslot = FALSE;
#endif /* WL_CLIENT_SAE */
	} else if ((as->state == AS_SENT_ASSOC) || (as->state == AS_REASSOC_RETRY)) {
		wlc_assoc_complete(cfg, WLC_E_STATUS_TIMEOUT, &target_bss->BSSID, 0,
				as->type != AS_ASSOCIATION, target_bss->bss_type);
		if (as->bss_retries < as->retry_max) {
			/* going to retry sending ASSOC_REQ, don't delete the assoc timeslot */
			delete_assoc_timeslot = FALSE;
		}
	} else if (as->state == AS_WAIT_RCV_BCN) {
		wlc_join_done_int(cfg, WLC_E_STATUS_TIMEOUT);
	} else if (as->state == AS_RETRY_BSS) {
		struct scb *scb;
		void *pkt = NULL;
		scb = wlc_scbfind(wlc, cfg, &target_bss->BSSID);

		pkt = wlc_join_assoc_start(wlc, cfg, scb, target_bss, FALSE);
		wlc_assoc_change_state(cfg, AS_SENT_ASSOC);
		if (pkt != NULL) {
			if (wlc_pcb_fn_register(wlc->pcb, wlc_assocreq_complete,
				(void *)(uintptr)cfg->ID, pkt)) {
				WL_ERROR(("wl%d: %s out of pkt callbacks\n",
						wlc->pub->unit, __FUNCTION__));
				wlc_assoc_abort(cfg);
				return;
			}
		}

		as->bss_retries++;
	} else if (as->state == AS_IDLE && wlc->as->cmn->sta_retry_time &&
			BSSCFG_STA(cfg) && cfg->enable && !cfg->associated) {
		wlc_ssid_t ssid;
		/* STA retry: armed from wlc_set_ssid_complete() */
		WL_ASSOC(("wl%d: Retrying failed association\n", wlc->pub->unit));
		WL_APSTA_UPDN(("wl%d: wlc_assoc_timeout -> wlc_join()\n", wlc->pub->unit));
		/* XXX It was decided to retry STA join with broadcast scanning all channels
		 * because of its nature - upstream AP was unavailable during previous retries
		 * and we wouldn't know which channel it will be running when it comes up later.
		 */
		ssid.SSID_len = cfg->SSID_len;
		bcopy(cfg->SSID, ssid.SSID, ssid.SSID_len);
		wlc_join(wlc, cfg, ssid.SSID, ssid.SSID_len, NULL, NULL, 0);
		delete_assoc_timeslot = FALSE;
	}
#if defined(WLRSDB) && defined(WL_MODESW)
	else if (WLC_MODESW_ENAB(wlc->pub) && RSDB_ENAB(wlc->pub) &&
		(as->state == AS_MODE_SWITCH_START)) {
		WL_ASSOC_ERROR(("wl:%d.%d MODE SWITCH FAILED. Timedout\n",
			WLCWLUNIT(wlc), cfg->_idx));
		wlc_assoc_change_state(cfg, AS_MODE_SWITCH_FAILED);
	}
#endif // endif
#ifdef ROBUST_DISASSOC_TX
	else if (as->type == AS_NONE && as->state == AS_DISASSOC_TIMEOUT) {
		wlc_disassoc_timeout(cfg);
		as->type = AS_NONE;
		wlc_assoc_change_state(cfg, AS_IDLE);
	}
#endif /* ROBUST_DISASSOC_TX */

	if (delete_assoc_timeslot && as->req_msch_hdl) {
		wlc_txqueue_end(wlc, cfg, NULL);
		wlc_msch_timeslot_unregister(wlc->msch_info, &as->req_msch_hdl);
	}

	/* Check if channel timeslot exist */
	if (recreate_success && !wlc_sta_timeslot_registed(cfg)) {
		WL_ASSOC_ERROR(("wl%d: %s: channel timeslot for connection not registered\n",
			WLCWLUNIT(wlc), __FUNCTION__));
		ASSERT(0);
	}
	return;
} /* wlc_assoc_timeout */

static void
wlc_assoc_recreate_timeout(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_roam_t *roam = cfg->roam;

	WL_ASSOC(("wl%d: JOIN: RECREATE timeout waiting for former AP's beacon\n",
	          WLCWLUNIT(wlc)));

	/* Send assoc verify timeout notification */
	wlc_bss_assoc_state_notif(wlc, cfg, AS_VERIFY_TIMEOUT, cfg->assoc->state);

	/* Clear software BSSID information so that the current AP will be a valid roam target */
	wlc_bss_clear_bssid(cfg);

	wlc_update_bcn_info(cfg, FALSE);

	roam->reason = WLC_E_REASON_BCNS_LOST;

	if (WOWL_ENAB(wlc->pub) && cfg == wlc->cfg)
		roam->roam_on_wowl = TRUE;

	(void)wlc_mac_request_entry(wlc, cfg, WLC_ACTION_RECREATE_ROAM, 0);

	/* start the roam scan */
	wlc_assoc_scan_prep(cfg, NULL, NULL, NULL, 0);
}

void
wlc_join_recreate(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	struct scb *scb = NULL;
	wlc_assoc_t *as = bsscfg->assoc;
	wlc_bss_info_t *bi = bsscfg->current_bss;
	int beacon_wait_time;
	wlc_roam_t *roam = bsscfg->roam;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN];
	char ssidbuf[SSID_FMT_BUF_LEN];
	char chanbuf[CHANSPEC_STR_LEN];
#endif // endif

	/* validate that bsscfg and current_bss infrastructure/ibss mode flag match */
	ASSERT((bsscfg->BSS == (bi->bss_type == DOT11_BSSTYPE_INFRASTRUCTURE)));
	if (!(bsscfg->BSS == (bi->bss_type == DOT11_BSSTYPE_INFRASTRUCTURE))) {
		return;
	}

	/* these should already be set */
	if (!wlc->pub->associated || wlc->stas_associated == 0) {
		WL_ASSOC(("wl%d: %s: both should have been TRUE: stas_assoc %d associated %d\n",
		          wlc->pub->unit, __FUNCTION__,
		          wlc->stas_associated, wlc->pub->associated));
	}

	/* Declare that driver is preserving the assoc */
	as->preserved = TRUE;

	if (bsscfg->BSS) {
		scb = wlc_scbfindband(wlc, bsscfg, &bi->BSSID, CHSPEC_WLCBANDUNIT(bi->chanspec));

		if (scb == NULL) {
			/* we lost our association - the AP no longer exists */
			wlc_assoc_init(bsscfg, AS_RECREATE);
			wlc_assoc_change_state(bsscfg, AS_LOSS_ASSOC);
		}
	}

	/* recreating an association to an AP */
	if (bsscfg->BSS && scb != NULL) {
		WL_ASSOC(("wl%d: JOIN: %s: recreating BSS association to ch %s %s %s\n",
		          WLCWLUNIT(wlc), __FUNCTION__,
		          wf_chspec_ntoa(bi->chanspec, chanbuf),
		          bcm_ether_ntoa(&bi->BSSID, eabuf),
		          (wlc_format_ssid(ssidbuf, bi->SSID, bi->SSID_len), ssidbuf)));

		WL_ASSOC(("wl%d: %s: scb %s found\n", wlc->pub->unit, __FUNCTION__, eabuf));

		WL_ASSOC(("wl%d: JOIN: scb     State:%d Used:%d(%d)\n",
		          WLCWLUNIT(wlc),
		          scb->state, scb->used, (int)(scb->used - wlc->pub->now)));

		WL_ASSOC(("wl%d: JOIN: scb     Band:%s Flags:0x%x Cfg %p\n",
		          WLCWLUNIT(wlc),
		          ((scb->bandunit == BAND_2G_INDEX) ? BAND_2G_NAME : BAND_5G_NAME),
		          scb->flags, OSL_OBFUSCATE_BUF(scb->bsscfg)));

		WL_ASSOC(("wl%d: JOIN: scb     WPA_auth %d\n",
		          WLCWLUNIT(wlc), scb->WPA_auth));

		/* update scb state */
		/* 	scb->flags &= ~SCB_SHORTPREAMBLE; */
		/* 	if ((bi->capability & DOT11_CAP_SHORT) != 0) */
		/* 		scb->flags |= SCB_SHORTPREAMBLE; */
		/* 	scb->flags |= SCB_MYAP; */
		/* 	wlc_scb_setstatebit(wlc, scb, AUTHENTICATED | ASSOCIATED); */

		if (!(scb->flags & SCB_MYAP))
			WL_ASSOC(("wl%d: %s: SCB_MYAP 0x%x not set in flags 0x%x!\n",
			          WLCWLUNIT(wlc), __FUNCTION__, SCB_MYAP, scb->flags));
		if ((scb->state & (AUTHENTICATED | ASSOCIATED)) != (AUTHENTICATED | ASSOCIATED))
			WL_ASSOC(("wl%d: %s: (AUTHENTICATED | ASSOCIATED) 0x%x "
				"not set in scb->state 0x%x!\n",
				WLCWLUNIT(wlc), __FUNCTION__,
				(AUTHENTICATED | ASSOCIATED), scb->state));

		WL_ASSOC(("wl%d: JOIN: AID 0x%04x\n", WLCWLUNIT(wlc), bsscfg->AID));

		/*
		 * security setup
		 */
		if (scb->WPA_auth != bsscfg->WPA_auth)
			WL_ASSOC(("wl%d: %s: scb->WPA_auth 0x%x "
				  "does not match bsscfg->WPA_auth 0x%x!",
				  WLCWLUNIT(wlc), __FUNCTION__, scb->WPA_auth, bsscfg->WPA_auth));

		/* disable txq processing until we are fully resynced with AP */
		wlc_block_datafifo(wlc, DATA_BLOCK_JOIN, DATA_BLOCK_JOIN);
	} else if (!bsscfg->BSS) {
		WL_ASSOC(("wl%d: JOIN: %s: recreating IBSS association to ch %s %s %s\n",
		          WLCWLUNIT(wlc), __FUNCTION__,
		          wf_chspec_ntoa(bi->chanspec, chanbuf),
		          bcm_ether_ntoa(&bi->BSSID, eabuf),
		          (wlc_format_ssid(ssidbuf, bi->SSID, bi->SSID_len), ssidbuf)));

		/* keep IBSS link indication up during recreation by resetting
		 * time_since_bcn
		 */
		roam->time_since_bcn = 0;
		roam->bcns_lost = FALSE;
	}

	/* suspend the MAC and configure the BSS/IBSS */
	wlc_suspend_mac_and_wait(wlc);
	wlc_BSSinit(wlc, bi, bsscfg, bsscfg->BSS ? WLC_BSS_JOIN : WLC_BSS_START);
	wlc_enable_mac(wlc);

#ifdef WLMCNX
	/* init multi-connection assoc states */
	if (MCNX_ENAB(wlc->pub) && bsscfg->BSS) {
		wlc_mcnx_assoc_upd(wlc->mcnx, bsscfg, TRUE);
	}
#endif // endif

	wlc_scb_ratesel_init_bss(wlc, bsscfg);

	/* clean up assoc recreate preserve flag */
	bsscfg->flags &= ~WLC_BSSCFG_PRESERVE;

	/* force a PHY cal on the current BSS/IBSS channel (channel set in wlc_BSSinit() */
	wlc_full_phy_cal(wlc, bsscfg,
			(bsscfg->BSS ? PHY_PERICAL_JOIN_BSS : PHY_PERICAL_START_IBSS));

	wlc_bsscfg_up(wlc, bsscfg);

	/* if we are recreating an IBSS, we are done */
	if (!bsscfg->BSS) {
		/* should bump the first beacon TBTT out a few BIs to give the opportunity
		 * of a coalesce before our first beacon
		 */
		return;
	}

	if (as->type == AS_RECREATE && as->state == AS_LOSS_ASSOC) {
		/* Clear software BSSID information so that the current AP
		 * will be a valid roam target
		 */
		wlc_bss_clear_bssid(bsscfg);

		wlc_update_bcn_info(bsscfg, FALSE);

		roam->reason = WLC_E_REASON_INITIAL_ASSOC;

		(void)wlc_mac_request_entry(wlc, bsscfg, WLC_ACTION_RECREATE, 0);

		/* make sure the association retry timer is not pending */
		wlc_assoc_timer_del(wlc, bsscfg);

		/* start the roam scan */
		as->flags |= AS_F_SPEEDY_RECREATE;
		wlc_assoc_scan_prep(bsscfg, NULL, NULL, &bi->chanspec, 1);
	} else {
		/* if we are recreating a BSS, wait for the first beacon */
		wlc_assoc_init(bsscfg, AS_RECREATE);
		wlc_assoc_change_state(bsscfg, AS_RECREATE_WAIT_RCV_BCN);

		wlc_set_ps_ctrl(bsscfg);

		/* Set the timer to move the assoc recreate along
		 * if we do not see a beacon in our BSS/IBSS.
		 * Allow assoc_recreate_bi_timeout beacon intervals plus some slop to allow
		 * for medium access delay for the last beacon.
		 */
		beacon_wait_time = as->recreate_bi_timeout *
			(bi->beacon_period * DOT11_TU_TO_US) / 1000;
		beacon_wait_time += wlc->bcn_wait_prd; /* allow medium access delay */

		wlc_assoc_timer_del(wlc, bsscfg);
		wl_add_timer(wlc->wl, as->timer, beacon_wait_time, 0);
	}
} /* wlc_join_recreate */

int
wlc_assoc_abort(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	wlc_bss_info_t *target_bss = cfg->target_bss;
	struct scb *scb;
	int callbacks = 0;
	bool is_macos = FALSE;
	bool is_recreate;
	bool do_cmplt = FALSE;
	/* Save current band unit */
	int curr_bandunit = (int)wlc->band->bandunit;
	assoc_cfg_cubby_t *as_cubby;

	BCM_REFERENCE(is_macos);
	BCM_REFERENCE(is_recreate);

#ifdef MACOSX
	is_macos = TRUE;
#endif // endif

	/* announcing we're aborting the association process */
	as_cubby = BSSCFG_ASSOC_CUBBY(wlc->as, cfg);
	if (as_cubby->stflags & AS_ST_FLAG_ABORT_PROC) {
		/* bail out if we're recursively called */
		WL_RECURSIVE(("wl%d.%d: %s: already in progress...\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
		return 0;
	}
	as_cubby->stflags |= AS_ST_FLAG_ABORT_PROC;

	WL_ASSOC(("wlc_assoc_abort state=%d type=%d", as->state, as->type));

	/* WES FIXME: check that all assoc states are cleaned up. take care with
	 * recent additions to state machine: TX_DRAIN, PASSHASH, JOIN_START, DISASSOC
	 */
	if (as->state == AS_IDLE) {
		/* assoc req may be idle but still have req in assoc req list */
		/* clean it up! */
		goto cleanup_assoc_req;
	}

	WL_ASSOC(("wl%d: aborting %s in progress\n", WLCWLUNIT(wlc), WLCASTYPEN(as->type)));
	WL_ROAM(("wl%d: aborting %s in progress\n", WLCWLUNIT(wlc), WLCASTYPEN(as->type)));

	if (ANY_SCAN_IN_PROGRESS(wlc->scan)) {
		WL_ASSOC(("wl%d: wlc_assoc_abort: aborting association scan in process\n",
		            WLCWLUNIT(wlc)));
		wlc_scan_abort(wlc->scan, WLC_E_STATUS_ABORT);
		/* At this point as->state may change from AS_SCAN to AS_IDLE, as wlc_scan_abort()
		 * may call wlc_assoc_done()
		 */
	}

#if defined(BCMSUP_PSK) && defined(BCMINTSUP)
	if (SUP_ENAB(wlc->pub) && (as->state == AS_WAIT_PASSHASH))
		callbacks += wlc_sup_down(wlc->idsup, cfg);
	else
#endif /* BCMSUP_PSK && BCMINTSUP */
	if (!wl_del_timer(wlc->wl, as->timer)) {
		as->rt = FALSE;
		callbacks ++;
	}

#ifdef MACOSX
	if (is_macos) {
		is_recreate = (ASSOC_RECREATE_ENAB(wlc->pub) &&
		               as->type == AS_RECREATE &&
		               (as->state == AS_RECREATE_WAIT_RCV_BCN ||
		                as->state == AS_ASSOC_VERIFY));
		if (!is_recreate) {
			if (as->state > AS_SCAN && as->state < AS_WAIT_DISASSOC) {
				/* indicate association completion */
				/* N.B.: assoc_state check passed through assoc_status parameter */
				wlc_assoc_complete(cfg, WLC_E_STATUS_ABORT, &target_bss->BSSID,
					as->state > AS_WAIT_TX_DRAIN && as->state < AS_WAIT_RCV_BCN,
					as->type != AS_ASSOCIATION,
					target_bss->bss_type);
			}

			/* indicate connection or roam completion */
			wlc_join_done_int(cfg, WLC_E_STATUS_ABORT);
			do_cmplt = TRUE;
		}
	} else
#endif /* MACOSX */
	/* WES FIXME: why is this wrapped with == AS_ASSOCIATTION? Seems like the AUTH
	 * cases would be fine if roams usually send auth, and the final event has a
	 * check for AS state.  Maybe it is just the last check that should go. Or maybe
	 * we should send up roam events?
	 */
	if (as->type == AS_ASSOCIATION) {
		if (as->state == AS_SENT_AUTH_1 || as->state == AS_SENT_AUTH_3) {
			wlc_auth_complete(cfg, WLC_E_STATUS_ABORT, &target_bss->BSSID,
			                  0, cfg->auth);
		} else if (as->state == AS_SENT_ASSOC || as->state == AS_REASSOC_RETRY) {
			wlc_assoc_complete(cfg, WLC_E_STATUS_ABORT, &target_bss->BSSID,
			                   0, as->type != AS_ASSOCIATION, 0);
		}
		/* Indicate connection completion */
		wlc_join_done_int(cfg, WLC_E_STATUS_ABORT);
		do_cmplt = TRUE;
	}

	if (curr_bandunit != (int)wlc->band->bandunit) {
		WL_INFORM(("wl%d: wlc->band changed since entering %s\n",
		   WLCWLUNIT(wlc), __FUNCTION__));
		WL_INFORM(("wl%d: %s: curr_bandunit = %d, wlc->band->bandunit = %d\n",
		   WLCWLUNIT(wlc), __FUNCTION__, curr_bandunit, wlc->band->bandunit));
	}
	/* Use wlc_scbfindband here in case wlc->band has changed since entering this function */
	/*
	 * When roaming, function wlc_join_done() called above can change the wlc->band *
	 * if AP and APSTA are both enabled. *
	 * In such a case, we will not be able to locate the correct scb entry if we use *
	 * wlc_scbfind() instead of wlc_scbfindband(). *
	 */
	if ((scb = wlc_scbfindband(wlc, cfg, &target_bss->BSSID, curr_bandunit)))
		/* Clear pending bits */
		wlc_scb_clearstatebit(wlc, scb, PENDING_AUTH | PENDING_ASSOC);

	wlc_block_datafifo(wlc, DATA_BLOCK_JOIN, 0);
	wlc_roam_release_flow_cntrl(cfg);

	if (!do_cmplt) {
#ifdef WLRSDB
		wlc_info_t *other_wlc = RSDB_ENAB(wlc->pub) ? wlc_rsdb_get_other_wlc(wlc) : NULL;
		if (!(RSDB_ENAB(wlc->pub) &&(other_wlc != NULL) && AS_IN_PROGRESS_WLC(other_wlc)))
#endif // endif
		{
			wlc_bss_list_free(wlc, wlc->as->cmn->join_targets);
			wlc->as->cmn->join_targets_last = 0;
		}

		as->type = AS_NONE;
		wlc_assoc_change_state(cfg, AS_IDLE);

		/* APSTA: complete any deferred AP bringup */
		if (AP_ENAB(wlc->pub) && APSTA_ENAB(wlc->pub))
			wlc_restart_ap(wlc->ap);
	}

cleanup_assoc_req:
	/* ensure the assoc_req list no longer has ref to this bsscfg */
	/* sometimes next op is wlc_bsscfg_free */
	if (wlc_assoc_req_remove_entry(wlc, cfg) == BCME_OK) {
		WL_ASSOC(("wl%d.%d: %s: assoc req entry removed\n",
			WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg), __FUNCTION__));
	}
#ifdef WLWNM
	if (WLWNM_ENAB(wlc->pub)) {
		wlc_wnm_bsstrans_reset_pending_join(wlc, cfg);
	}
#endif /* WLWNM */

	/* we're done aborting the association process */
	as_cubby->stflags &= ~AS_ST_FLAG_ABORT_PROC;

	/* sanity check */
	ASSERT(!wlc_assoc_req_known_entry(wlc, cfg));

	return callbacks;
} /* wlc_assoc_abort */

void
wlc_assoc_init(wlc_bsscfg_t *cfg, uint type)
{
	wlc_assoc_t *as = cfg->assoc;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	wlc_info_t *wlc = cfg->wlc;

	ASSERT(type == AS_ROAM || type == AS_ASSOCIATION || type == AS_RECREATE);

	WL_ASSOC(("wl%d.%d: %s: assoc state machine init to assoc->type %d %s\n",
	          WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg), __FUNCTION__, type, WLCASTYPEN(type)));
#endif /* BCMDBG || WLMSG_ASSOC */

	as->type = type;
	as->flags &= ~AS_F_INIT_RESET;
}

void
wlc_assoc_change_state(wlc_bsscfg_t *cfg, uint newstate)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_phy_t *pi = WLC_PI(wlc);
	wlc_assoc_t *as = cfg->assoc;
	uint oldstate = as->state;

	ASSERT(newstate < AS_LAST_STATE);

	if (newstate == oldstate)
		return;

	as->state = newstate;
	WL_ASSOC_LT(("wl%d.%d: wlc_assoc_change_state: change assoc_state from %s to %s\n",
	          WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg),
	          wlc_as_st_name(oldstate), wlc_as_st_name(newstate)));

	wl_print_backtrace(__FUNCTION__, NULL, 0);

	if ((oldstate == AS_SCAN) && (newstate == AS_IDLE) && ANY_SCAN_IN_PROGRESS(wlc->scan)) {
		WL_ASSOC_LT(("wl%d.%d: %s: transition scan to idle, scan still in progress!\n",
			WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg), __FUNCTION__));
	}

#ifdef WL_PWRSTATS
	 if (PWRSTATS_ENAB(wlc->pub)) {
		/* Connection is over on port open and AS_IDLE OR assoc failed and AS_IDLE */
		if ((cfg == wlc->cfg) && cfg->BSS &&
			newstate == AS_IDLE && (wlc_portopen(cfg) || !cfg->associated))
			wlc_connect_time_upd(wlc);
	}
#endif /* WL_PWRSTATS */

	if (newstate == AS_JOIN_START) {
		wlc->ebd->assoc_start_time = OSL_SYSUPTIME();
		wlc->ebd->earliest_offset = 0;
		wlc->ebd->detect_done = FALSE;
	}

	if ((newstate == AS_IDLE) && as->req_msch_hdl) {
		wlc_txqueue_end(wlc, cfg, NULL);
		wlc_msch_timeslot_unregister(wlc->msch_info, &as->req_msch_hdl);
		as->req_msch_hdl = NULL;
	}

#ifdef SLAVE_RADAR
	if ((newstate == AS_DFS_CAC_START) || (newstate == AS_DFS_ISM_INIT)) {
		if ((!wlc_is_ap_interface_up(wlc)) && WL11H_STA_ENAB(wlc) &&
			wlc_dfs_get_radar(wlc->dfs)) {
			bool from_radar = FALSE;
			bool to_radar = FALSE;
			wlc_bss_info_t *bi;
			bi = wlc->as->cmn->join_targets->ptrs[wlc->as->cmn->join_targets_last];
			if (bi != NULL) {
				from_radar = (wlc_radar_chanspec(wlc->cmi,
					cfg->current_bss->chanspec) == TRUE);
				to_radar = wlc_radar_chanspec(wlc->cmi, bi->chanspec);
				if (wlc_dfs_get_radar(wlc->dfs) && (!from_radar && to_radar)) {
					cfg->pm->pm_oldvalue = cfg->pm->PM;
					wlc_set_pm_mode(wlc, PM_OFF, cfg);
					cfg->pm->pm_modechangedisabled = TRUE;
					wlc->mpc = FALSE;
					wlc_radio_mpc_upd(wlc);
				}
			}
		}
	}
	/* if radar detected during DFS re-entry logic, abort Association state
	 * machine and let wlc_try_join_start initiate join state machine from
	 * scan stage
	 */
	if (!BSSCFG_AP(cfg) && wlc_dfs_get_radar(wlc->dfs) && (newstate == AS_DFS_CAC_FAIL)) {
		wlc_assoc_abort(cfg);
	}
#endif /* SLAVE_RADAR */

	/* transition from IDLE */
	if (oldstate == AS_IDLE) {
		/* move out of IDLE */
		wlc_phy_hold_upd(pi, PHY_HOLD_FOR_ASSOC, TRUE);

		/* add the new request on top, or move existing request back on top
		 * in case the abort operation has removed it
		 */
		wlc_assoc_req_add_entry(wlc, cfg, as->type);
	}
	/* transition to IDLE */
	else if (newstate == AS_IDLE) {
		/* move into IDLE */
		wlc_phy_hold_upd(pi, PHY_HOLD_FOR_ASSOC, FALSE);

		if (wlc_assoc_req_remove_entry(wlc, cfg) == BCME_OK) {
			/* the current assoc process does no longer need these variables/states,
			 * reset them to prepare for the next one.
			 * - join targets
			 */
			if (!BSSCFG_SLOTTED_BSS(cfg)) {
				wlc_bss_list_free(wlc, wlc->as->cmn->join_targets);
			}

			/* Send bss assoc notification */
			if ((cfg->assoc->type == AS_NONE) && (cfg->assoc->state == AS_IDLE)) {
				wlc_bss_assoc_state_notif(wlc, cfg, cfg->assoc->type,
					cfg->assoc->state);
			}

			/* start the next request processing if any */
			wlc_assoc_req_process_next(wlc);
		} else {
			/* update wake control */
			wlc_set_wake_ctrl(wlc);
		}
	}
#if defined(WLRSDB) && defined(WL_MODESW)
	else if (WLC_MODESW_ENAB(wlc->pub) && RSDB_ENAB(wlc->pub) &&
		(newstate == AS_MODE_SWITCH_START)) {
		uint8 chip_mode;

		BCM_REFERENCE(chip_mode);

		as->state_rsdb = oldstate;

		chip_mode = WLC_RSDB_CURR_MODE(wlc);
		/* Upgrade the Chip mode from RSDB
		 * to either MIMO or 80p80 depending
		 * upon the current values of Nss and Bw.
		 */
		if (WLC_RSDB_DUAL_MAC_MODE(chip_mode)) {
			wlc_rsdb_upgrade_wlc(wlc);
		}
		/* Switch to RSDB if in 2X2 mode.
		 * The need for this switch is analysed
		 * by the caller. We just need to switch
		 * the current mode to RSDB
		 */
		else if (WLC_RSDB_SINGLE_MAC_MODE(chip_mode)) {
			wlc_rsdb_downgrade_wlc(wlc);
		}
	} else if (RSDB_ENAB(wlc->pub) && (newstate == AS_MODE_SWITCH_COMPLETE)) {
		WL_MODE_SWITCH(("wl:%d.%d MODE SWITCH SUCCESS..\n", WLCWLUNIT(wlc),
			cfg->_idx));
		wlc_assoc_change_state(cfg, as->state_rsdb);
		wlc_join_attempt(cfg);
	} else if (RSDB_ENAB(wlc->pub) && (newstate == AS_MODE_SWITCH_FAILED)) {
		wlc_bsscfg_t *icfg;
		int index;

		FOREACH_BSS(wlc, index, icfg) {
			wlc_modesw_clear_context(wlc->modesw, icfg);
		}
		wlc_join_attempt(cfg);
	}
#endif /* WLRSDB && WL_MODESW */
} /* wlc_assoc_change_state */

/** given the current chanspec, select a non-radar chansepc */
static chanspec_t
wlc_sradar_ap_select_channel(wlc_info_t *wlc, chanspec_t curr_chanspec)
{
	uint rand_idx;
	int listlen, noradar_listlen = 0, i;
	chanspec_t newchspec = 0;
	const char *abbrev = wlc_channel_country_abbrev(wlc->cmi);
	wl_uint32_list_t *list, *noradar_list = NULL;
	bool bw20 = FALSE, ch2g = FALSE;

	/* if curr_chanspec is non-radar, just return it and do nothing */
	if (!wlc_radar_chanspec(wlc->cmi, curr_chanspec)) {
		return (curr_chanspec);
	}

	/* use current chanspec to determine which valid */
	/* channels to look for */
	if (CHSPEC_IS2G(curr_chanspec) || (curr_chanspec == 0)) {
		ch2g = TRUE;
	}
	if (CHSPEC_IS5G(curr_chanspec) || (curr_chanspec == 0)) {
		ch2g = FALSE;
	}
	if (CHSPEC_IS20(curr_chanspec) || (curr_chanspec == 0)) {
		bw20 = TRUE;
	}
	if (CHSPEC_IS40(curr_chanspec) || (curr_chanspec == 0)) {
		bw20 = FALSE;
	}

	/* allocate memory for list */
	listlen =
		OFFSETOF(wl_uint32_list_t, element)
		+ sizeof(list->element[0]) * MAXCHANNEL;

	if ((list = MALLOC(wlc->osh, listlen)) == NULL) {
		WL_ERROR(("wl%d: %s: failed to allocate list\n",
		  wlc->pub->unit, __FUNCTION__));
	} else {
		/* get a list of valid channels */
		list->count = 0;
		wlc_get_valid_chanspecs(wlc->cmi, list,
		(bw20 ? WL_CHANSPEC_BW_20 : WL_CHANSPEC_BW_40), ch2g, abbrev);
	}

	/* This code builds a non-radar channel list out of the valid list */
	/* and picks one randomly (preferred option) */
	if (list && list->count) {
		/* build a noradar_list */
		noradar_listlen =
			OFFSETOF(wl_uint32_list_t, element) +
			sizeof(list->element[0]) * list->count;

		if ((noradar_list = MALLOC(wlc->osh, noradar_listlen)) == NULL) {
			WL_ERROR(("wl%d: %s: failed to allocate noradar_list\n",
			  wlc->pub->unit, __FUNCTION__));
		} else {
			noradar_list->count = 0;
			for (i = 0; i < (int)list->count; i++) {
				if (!wlc_radar_chanspec(wlc->cmi,
					(chanspec_t)list->element[i])) {
					/* add to noradar_list */
					noradar_list->element[noradar_list->count++]
						= list->element[i];
				}
			}
		}
		if (noradar_list && noradar_list->count) {
			/* randomly pick a channel in noradar_list */
			rand_idx = R_REG(wlc->osh, D11_TSF_RANDOM(wlc))
				% noradar_list->count;
			newchspec = (chanspec_t)noradar_list->element[rand_idx];
		}
	}
	if (list) {
		/* free list */
		MFREE(wlc->osh, list, listlen);
	}
	if (noradar_list) {
		/* free noradar_list */
		MFREE(wlc->osh, noradar_list, noradar_listlen);
	}

	return (newchspec);
} /* wlc_sradar_ap_select_channel */

/* checks for sradar enabled AP.
 * if one found, check current_bss chanspec
 * if chanspec on radar channel, randomly select non-radar channel
 * and move to it
 */
static void
wlc_sradar_ap_update(wlc_info_t *wlc)
{
	wlc_bsscfg_t *cfg;
	int idx;
	chanspec_t newchspec, chanspec = 0;
	bool move_ap = FALSE;

	/* See if we have a sradar ap on radar channel */
	FOREACH_UP_AP(wlc, idx, cfg) {
		if (BSSCFG_SRADAR_ENAB(cfg) &&
		    wlc_radar_chanspec(wlc->cmi, cfg->current_bss->chanspec)) {
			/* use current chanspec to determine which valid */
			/* channels to look for */
			chanspec = cfg->current_bss->chanspec;
			/* set flag to move ap */
			move_ap = TRUE;
			/* only need to do this for first matching up ap */
			break;
		}
	}

	if (move_ap == FALSE) {
		/* no sradar ap on radar channel, do nothing */
		return;
	}

	/* find a non-radar channel to move to */
	/* XXX: MCHAN (This is assuming multiple AP's all on same channel)
	 * Can't have multiple AP's on different channels.
	 * If want multiple AP's on different channels, need to rework.
	 */
	newchspec = wlc_sradar_ap_select_channel(wlc, chanspec);
	/* if no non-radar channel found, disable sradar ap on radar channel */
	FOREACH_UP_AP(wlc, idx, cfg) {
		if (BSSCFG_SRADAR_ENAB(cfg) &&
		    wlc_radar_chanspec(wlc->cmi, cfg->current_bss->chanspec)) {
			if (newchspec) {
				/* This code performs a channel switch immediately */
				/* w/o sending csa action frame and csa IE in beacon, prb_resp */
				wlc_do_chanswitch(cfg, newchspec);
			} else {
				/* can't find valid non-radar channel */
				/* shutdown ap */
				WL_INFORM(("%s: no radar channel found, disable ap\n",
				          __FUNCTION__));
				wlc_bsscfg_disable(wlc, cfg);
			}
		}
	}
} /* wlc_sradar_ap_update */

/** update STA association state */
void
wlc_sta_assoc_upd(wlc_bsscfg_t *cfg, bool state)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_phy_t  *pi	= WLC_PI(wlc);
	int idx;
	wlc_bsscfg_t *bc;

#if defined(BCMECICOEX)
	chanspec_t  chspec;
#endif // endif

	WL_ASSOC(("wl%d.%d: %s: assoc state change %d>%d\n",
	          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__, cfg->associated, state));

	/* STA is no longer associated, reset related states.
	 */
	if (!state) {
		wlc_reset_pmstate(cfg);
	}
	/* STA is assoicated, set related states that could have been
	 * missing before cfg->associated is set...
	 */
	else {
		wlc_set_pmawakebcn(cfg, wlc->PMawakebcn);
#if defined(WLBSSLOAD_REPORT)
		if (BSSLOAD_REPORT_ENAB(wlc->pub)) {
			wlc_bssload_reset_saved_data(cfg);
		}
#endif /* defined(WLBSSLOAD_REPORT) */
	}

	cfg->associated = state;

	wlc->stas_associated = 0;
	wlc->ibss_bsscfgs = 0;
	wlc->any_sta_in_160mhz = FALSE; /* count STA ifaces in 160MHz */
	FOREACH_AS_STA(wlc, idx, bc) {
	        wlc->stas_associated++;
		if (!bc->BSS)
			wlc->ibss_bsscfgs++;
		if (CHSPEC_IS8080(bc->current_bss->chanspec) ||
			CHSPEC_IS160(bc->current_bss->chanspec))
			wlc->any_sta_in_160mhz = TRUE;
	}
	if (wlc->any_sta_in_160mhz || wlc->num_160mhz_assocs > 0)
		phy_chanmgr_tdcs_enable_160m((phy_info_t*) wlc->pi, FALSE);
	else
		phy_chanmgr_tdcs_enable_160m((phy_info_t*) wlc->pi, TRUE);

	wlc->pub->associated = (wlc->stas_associated != 0 || wlc->aps_associated != 0);

	wlc_phy_hold_upd(pi, PHY_HOLD_FOR_NOT_ASSOC,
	                 wlc->pub->associated ? FALSE : TRUE);

	wlc->stas_connected = wlc_stas_connected(wlc);

#ifdef WLTPC
	/* Reset TPC on disconnect with AP to avoid using incorrect values when
	 * we connect to non 11d AP next time
	 */
	if ((wlc->stas_connected == 0) && (wlc->aps_associated == 0)) {
		wlc_tpc_reset_all(wlc->tpc);
	}
#endif // endif

	wlc_ap_sta_onradar_upd(cfg);

	/* Update maccontrol PM related bits */
	wlc_set_ps_ctrl(cfg);

	/* change the watchdog driver */
	wlc_watchdog_upd(cfg, WLC_WATCHDOG_TBTT(wlc));

	wlc_btc_set_ps_protection(wlc, cfg); /* enable if state = TRUE */

#if defined(BCMECICOEX)
	/* Inform BT about channel change on association and disassoc
	*  If disassociated, send channel zero to ensure it will be updated on AFH
	*  when BT and AWDL are active
	*/
	if (!state) {
		chspec = 0;
	} else {
		chspec = wlc->chanspec;
	}
		wlc_bmac_update_bt_chanspec(wlc->hw, chspec,
				SCAN_IN_PROGRESS(wlc->scan), WLC_RM_IN_PROGRESS(wlc));
#endif // endif

	/* if no station associated and we have ap up, check channel */
	/* if radar channel, move off to non-radar channel */
	if (WL11H_ENAB(wlc) && AP_ACTIVE(wlc) && wlc->stas_associated == 0) {
		wlc_sradar_ap_update(wlc);
	}

#ifdef WLRSDB
	if (RSDB_ENAB(wlc->pub))
		wlc_rsdb_update_active(wlc, NULL);
#endif /* WLRSDB */
}

static void
wlc_join_adopt_bss(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	wlc_roam_t *roam = cfg->roam;
	wlc_bss_info_t *target_bss = cfg->target_bss;
	uint reason = 0;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char chanbuf[CHANSPEC_STR_LEN];
	char ssidbuf[SSID_FMT_BUF_LEN];
	wlc_format_ssid(ssidbuf, target_bss->SSID, target_bss->SSID_len);
#endif /* BCMDBG || WLMSG_ASSOC */

	if (target_bss->bss_type == DOT11_BSSTYPE_INFRASTRUCTURE) {
		WL_ASSOC(("wl%d: JOIN: join BSS \"%s\" on chanspec %s\n", WLCWLUNIT(wlc),
			ssidbuf, wf_chspec_ntoa_ex(target_bss->chanspec, chanbuf)));
	} else {
		WL_ASSOC(("wl%d: JOIN: join IBSS \"%s\" on chanspec %s\n", WLCWLUNIT(wlc),
			ssidbuf, wf_chspec_ntoa_ex(target_bss->chanspec, chanbuf)));
	}

	wlc_block_datafifo(wlc, DATA_BLOCK_JOIN, 0);

	roam->RSSIref = 0; /* this will be reset on the first incoming frame */

	if (as->type == AS_ASSOCIATION) {
		roam->reason = WLC_E_REASON_INITIAL_ASSOC;
		WL_ASSOC(("wl%d: ROAM: roam_reason cleared to 0x%x\n",
			wlc->pub->unit, WLC_E_REASON_INITIAL_ASSOC));

		if (roam->reassoc_param != NULL) {
			MFREE(wlc->osh, roam->reassoc_param, roam->reassoc_param_len);
			roam->reassoc_param = NULL;
		}
	}

	roam->prev_rssi = target_bss->RSSI;
	WL_ASSOC(("wl%d: ROAM: initial rssi %d\n", WLCWLUNIT(wlc), roam->prev_rssi));
	WL_SRSCAN(("wl%d: ROAM: initial rssi %d\n", WLCWLUNIT(wlc), roam->prev_rssi));

	/* Reset split roam scan state so next scan will be on cached channels. */
	roam->split_roam_phase = 0;
	roam->roam_chn_cache_locked = FALSE;
#ifdef WLABT
	if (WLABT_ENAB(wlc->pub) && cfg->roam->prev_bcn_timeout != 0) {
		cfg->roam->bcn_timeout = cfg->roam->prev_bcn_timeout;
		cfg->roam->prev_bcn_timeout = 0;
		WL_ASSOC(("Reset bcn_timeout to %d\n", cfg->roam->bcn_timeout));
	}
#endif /* WLABT */

#ifdef WLMCNX
	/* stop TSF adjustment - do it before setting BSSID to prevent ucode
	 * from adjusting TSF before we program the TSF block
	 */
	if (MCNX_ENAB(wlc->pub) && cfg->BSS) {
		wlc_skip_adjtsf(wlc, TRUE, cfg, -1, WLC_BAND_ALL);
	}
#endif /* WLMCNX */

	/* suspend the MAC and join the BSS */
	wlc_suspend_mac_and_wait(wlc);
	wlc_BSSinit(wlc, target_bss, cfg, WLC_BSS_JOIN);
	wlc_enable_mac(wlc);

	wlc_sta_assoc_upd(cfg, TRUE);
	WL_RTDC(wlc, "wlc_join_adopt_bss: associated", 0, 0);

	/* Apply the STA AC params sent by AP */
	if (BSS_WME_AS(wlc, cfg)) {
		WL_ASSOC(("wl%d.%d: adopting WME AC params...\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg)));
		wlc_edcf_acp_apply(wlc, cfg, TRUE);
	}

#ifdef WLMCNX
	/* init multi-connection assoc states */
	if (MCNX_ENAB(wlc->pub) && cfg->BSS)
		wlc_mcnx_assoc_upd(wlc->mcnx, cfg, TRUE);
#endif // endif

	if (N_ENAB(wlc->pub) && COEX_ENAB(wlc))
		wlc_ht_obss_scan_reset(cfg);

	WL_APSTA_UPDN(("wl%d: Reporting link up on config 0 for STA joining %s\n",
	               WLCWLUNIT(wlc), cfg->BSS ? "a BSS" : "an IBSS"));

#if defined(WLFBT)
	if (BSSCFG_IS_FBT(cfg) && (bcmwpa_is_rsn_auth(cfg->WPA_auth)) &&
		(cfg->auth_atmptd == DOT11_FAST_BSS)) {
		reason = WLC_E_REASON_BSSTRANS_REQ;  /* any non-zero reason is okay here */
	}
#endif /* WLFBT */

	wlc_link(wlc, TRUE, &cfg->BSSID, cfg, reason);

	wlc_assoc_change_state(cfg, AS_JOIN_ADOPT);
	wlc_bss_assoc_state_notif(wlc, cfg, as->type, AS_JOIN_ADOPT);

	if (target_bss->bss_type == DOT11_BSSTYPE_INFRASTRUCTURE) {
		int beacon_wait_time;

		/* infrastructure join needs a BCN for TBTT coordination */
		WL_ASSOC(("wl%d: JOIN: BSS case...waiting for the next beacon...\n",
		            WLCWLUNIT(wlc)));
		wlc_assoc_change_state(cfg, AS_WAIT_RCV_BCN);
		wlc_bss_assoc_state_notif(wlc, cfg, as->type, AS_WAIT_RCV_BCN);

		/* Start the timer for beacon receive timeout handling to ensure we move
		 * out of AS_WAIT_RCV_BCN state in case we never receive the first beacon
		 */
		beacon_wait_time = as->recreate_bi_timeout *
			(target_bss->beacon_period * DOT11_TU_TO_US) / 1000;

		/* Validate beacon_wait_time, usually it will be 1024 with beacon interval 100TUs */
		beacon_wait_time = beacon_wait_time > 10000 ? 10000 :
			beacon_wait_time < 1000 ? 1024 : beacon_wait_time;

		wlc_assoc_timer_del(wlc, cfg);
		wl_add_timer(wlc->wl, as->timer, beacon_wait_time, 0);
	}

	wlc_roam_release_flow_cntrl(cfg);

	wlc_bsscfg_SSID_set(cfg, target_bss->SSID, target_bss->SSID_len);
	wlc_roam_set_env(cfg, wlc->as->cmn->join_targets->count);

	/* Reset roam profile for new association */
	wlc_roam_prof_update(wlc, cfg, TRUE);

#ifdef WLLED
	wlc_led_event(wlc->ledh);
#endif // endif
} /* wlc_join_adopt_bss */

/** return true if we support all of the target's basic rates in 'rateset' */
static bool
wlc_join_verify_basic_rates(wlc_info_t *wlc, wlc_bsscfg_t *cfg, wlc_bss_info_t *bi,
	wlc_rateset_t *rateset)
{
	uint i;
	uint8 rate;
	int band = CHSPEC2WLC_BAND(bi->chanspec);

	if (bi->bss_type != DOT11_BSSTYPE_INDEPENDENT) {
		return TRUE;
	}

	for (i = 0; i < rateset->count; i++) {
		if (rateset->rates[i] & WLC_RATE_FLAG) {
			rate = rateset->rates[i] & RATE_MASK;
			if (!wlc_valid_rate(wlc, rate, band, FALSE))
				return (FALSE);
		}
	}
	return (TRUE);
}

/* return true if we support one or more rates in the target's OperationRateset 'rateset'. */
static bool
wlc_join_target_verify_rates(wlc_info_t *wlc, wlc_bsscfg_t *cfg, wlc_bss_info_t *bi,
	wlc_rateset_t *rateset)
{
	wlcband_t *target_band;
	wlc_rateset_t dst;

	target_band = wlc->bandstate[CHSPEC_WLCBANDUNIT(bi->chanspec)];

	/* Copy rateset to preserve original (CCK only if LegacyB) */
	if (BAND_2G(target_band->bandtype) && (target_band->gmode == GMODE_LEGACY_B)) {
		wlc_rateset_filter(rateset /* src */, &dst /* dst */,
		                   FALSE, WLC_RATES_CCK, RATE_MASK_FULL, 0);
	} else {
		wlc_rateset_filter(rateset, &dst, FALSE, WLC_RATES_CCK_OFDM,
		                   RATE_MASK_FULL, wlc_get_mcsallow(wlc, cfg));
	}
	/* Adjust rateset for target channel based on the channel bandwidth
	 * filter-out unsupported rates.
	 */
	wlc_rateset_bw_mcs_filter(&dst,
	                          WL_BW_CAP_40MHZ(target_band->bw_cap) ?
	                          CHSPEC_WLC_BW(bi->chanspec) : 0);
	if (!wlc_rate_hwrs_filter_sort_validate(&dst /* [in+out] */,
	                                        &target_band->hw_rateset /* [in] */,
	                                        FALSE, wlc->stf->op_txstreams)) {
		return FALSE;
	}

	return TRUE;
}

/* return true if we support all of the target's BSS membership selectors
 * that we know of. return the rateset excluding the BSS membership selectors
 * we know of in 'rateset' (it may leak membership selector we don't know).
 */
static bool
wlc_join_verify_filter_membership(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	wlc_bss_info_t *bi, wlc_rateset_t *rateset)
{
	uint i;
	wlc_rateset_t dst;

	memcpy(&dst, rateset, sizeof(*rateset));
	bzero(dst.rates, sizeof(dst.rates));

	for (i = 0; i < rateset->count; i++) {
		switch (rateset->rates[i]) {
		case DOT11_BSS_MEMBERSHIP_HT:
#ifdef ASSOC_FAIL_SIM
			/* simulate STA ignores MembershipSelector filtering */
			if (wlc_assoc_fail_sim.test == WLC_ASSOC_FAIL_SIM_NO_MBSP_SEL_FLTR) {
				goto take;
			}
#endif // endif
			if (!BSS_N_ENAB(wlc, cfg)) {
				return FALSE;
			}
			break;
		case DOT11_BSS_MEMBERSHIP_VHT:
#ifdef ASSOC_FAIL_SIM
			/* simulate STA ignores MembershipSelector filtering */
			if (wlc_assoc_fail_sim.test == WLC_ASSOC_FAIL_SIM_NO_MBSP_SEL_FLTR) {
				goto take;
			}
#endif // endif
			if (!BSS_VHT_ENAB(wlc, cfg)) {
				return FALSE;
			}
			break;
		case DOT11_BSS_MEMBERSHIP_HE:
#ifdef ASSOC_FAIL_SIM
			/* simulate STA ignores MembershipSelector filtering */
			if (wlc_assoc_fail_sim.test == WLC_ASSOC_FAIL_SIM_NO_MBSP_SEL_FLTR) {
				goto take;
			}
#endif // endif
			if (!BSS_HE_ENAB(wlc, cfg)) {
				return FALSE;
			}
			break;
		default:
#ifdef ASSOC_FAIL_SIM
		take:
#endif // endif
			dst.rates[dst.count++] = rateset->rates[i];
			break;
		}
	}

	memcpy(rateset, &dst, sizeof(dst));
	return TRUE;
}

int
wlc_join_recreate_complete(wlc_bsscfg_t *cfg, wlc_d11rxhdr_t *wrxh, uint8 *plcp,
	struct dot11_bcn_prb *bcn, int bcn_len)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	wlc_bss_info_t *current_bss = cfg->current_bss;
	uint8* tlvs;
	int tlvs_len;
	bcm_tlv_t *tim;
	int err = BCME_OK;
	uint type = as->type;

	/* In a standard join attempt this is done in wlc_join_BSS() as the
	 * process begins. For a recreate, we wait until we have confirmation that
	 * we are in the presence of our former AP before unmuting a channel.
	 */
	if (wlc_quiet_chanspec(wlc->cmi, current_bss->chanspec)) {
		wlc_clr_quiet_chanspec(wlc->cmi, current_bss->chanspec);
		if (WLC_BAND_PI_RADIO_CHANSPEC == current_bss->chanspec) {
			wlc_mute(wlc, OFF, 0);
		}
	}

	/* In a standard join attempt this is done on the WLC_E_ASSOC/REASSOC event to
	 * allow traffic to flow to the newly associated AP. For a recreate, we wait until
	 * we have confirmation that we are in the presence of our former AP.
	 */
	wlc_block_datafifo(wlc, DATA_BLOCK_JOIN, 0);

	/* TSF adoption and PS transition is done just as in a standard association in
	 * wlc_join_complete()
	 */
	wlc_tsf_adopt_bcn(cfg, wrxh, plcp, bcn);

	tlvs = (uint8*)bcn + DOT11_BCN_PRB_LEN;
	tlvs_len = bcn_len - DOT11_BCN_PRB_LEN;

	tim = NULL;
	if (cfg->BSS) {
		/* adopt DTIM period for PS-mode support */
		tim = bcm_parse_tlvs(tlvs, tlvs_len, DOT11_MNG_TIM_ID);
		if (tim != NULL && tim->len >= DOT11_MNG_TIM_FIXED_LEN) {
			current_bss->dtim_period = tim->data[DOT11_MNG_TIM_DTIM_PERIOD];
#ifdef WLMCNX
			if (MCNX_ENAB(wlc->pub))
				; /* empty */
			else
#endif // endif
			if (cfg == wlc->cfg)
				wlc_write_shm(wlc, M_DOT11_DTIMPERIOD(wlc),
					current_bss->dtim_period);
			wlc_update_bcn_info(cfg, TRUE);
#ifdef WLMCNX
			if (MCNX_ENAB(wlc->pub))
				wlc_mcnx_dtim_upd(wlc->mcnx, cfg, TRUE);
#endif // endif
		}else { /*  (!tim) illed AP, prevent going to power-save mode */
			wlc_set_pmoverride(cfg, TRUE);
		}
	}

#ifdef BCMULP
	if (BCMULP_ENAB() && wlc_is_ulp_pending(wlc->ulp)) {
		/* extracting information from bcn_prb to populate wl_status info */
		current_bss->bcn_prb =
			(struct dot11_bcn_prb *)MALLOCZ(wlc->osh, bcn_len);
		if (current_bss->bcn_prb) {
			memcpy((char*)current_bss->bcn_prb, (char*)bcn, bcn_len);
			current_bss->bcn_prb_len = bcn_len;
		} else {
			WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			err = BCME_NOMEM;
			/* Note that no return here because malloc/copy is done here
			 * to print the status information ONLY, and below code
			 * SHOULD be executed to complete recreate.
			 */
		}
		current_bss->capability = ltoh16(bcn->capability);
		as->type = AS_NONE;
		wlc_assoc_change_state(cfg, AS_IDLE);
		/* note: AS_ASSOC_VERIFY is sent for completion indication
		* to esp. ulp
		*/
		wlc_bss_assoc_state_notif(wlc, cfg, type, AS_IDLE);
		return err;
	}
#endif /* BCMULP */

	if (cfg->BSS) {
		/* when recreating an association, send a null data to the AP to verify
		 * that we are still associated and wait a generous amount amount of time
		 * to allow the AP to send a disassoc/deauth if it does not consider us
		 * associated.
		 */
		if (!wlc_sendnulldata(wlc, cfg, &cfg->current_bss->BSSID, 0, 0,
			PRIO_8021D_BE, NULL, NULL))
			WL_ASSOC_ERROR(("wl%d: %s: wlc_sendnulldata() failed\n",
			          wlc->pub->unit, __FUNCTION__));

		/* kill the beacon timeout timer and reset for association verification */
		wlc_assoc_timer_del(wlc, cfg);
		wl_add_timer(wlc->wl, as->timer, as->verify_timeout, FALSE);

		wlc_assoc_change_state(cfg, AS_ASSOC_VERIFY);
	} else {
		/* for an IBSS recreate, we are done */
		as->type = AS_NONE;
		wlc_assoc_change_state(cfg, AS_IDLE);
	}

	/* Send bss assoc notification */
	wlc_bss_assoc_state_notif(wlc, cfg, type, as->state);
	return err;
} /* wlc_join_recreate_complete */

static int
wlc_merge_bcn_prb(wlc_info_t *wlc,
	struct dot11_bcn_prb *p1, int p1_len,
	struct dot11_bcn_prb *p2, int p2_len,
	struct dot11_bcn_prb **merged, int *merged_len)
{
	uint8 *tlvs1, *tlvs2;
	int tlvs1_len, tlvs2_len;
	int ret;

	*merged = NULL;
	*merged_len = 0;

	if (p1) {
		ASSERT(p1_len >= DOT11_BCN_PRB_LEN);
		/* fixup for non-assert builds */
		if (p1_len < DOT11_BCN_PRB_LEN)
			p1 = NULL;
	}
	if (p2) {
		ASSERT(p2_len >= DOT11_BCN_PRB_LEN);
		/* fixup for non-assert builds */
		if (p2_len < DOT11_BCN_PRB_LEN)
			p2 = NULL;
	}

	/* check for simple cases of one or the other of the source packets being null */
	if (p1 == NULL && p2 == NULL) {
		return BCME_ERROR;
	} else if (p2 == NULL) {
		*merged = (struct dot11_bcn_prb *) MALLOCZ(wlc->osh, p1_len);
		if (*merged != NULL) {
			*merged_len = p1_len;
			bcopy((char*)p1, (char*)*merged, p1_len);
			ret = BCME_OK;
		} else {
			WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__, p1_len,
				MALLOCED(wlc->osh)));
			ret = BCME_NOMEM;
		}
		return ret;
	} else if (p1 == NULL) {
		*merged = (struct dot11_bcn_prb *) MALLOCZ(wlc->osh, p2_len);
		if (*merged != NULL) {
			*merged_len = p2_len;
			bcopy((char*)p2, (char*)*merged, p2_len);
			ret = BCME_OK;
		} else {
			WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__, p2_len,
				MALLOCED(wlc->osh)));
			ret = BCME_NOMEM;
		}
		return ret;
	}

	/* both source packets are present, so do the merge work */
	tlvs1 = (uint8*)p1 + DOT11_BCN_PRB_LEN;
	tlvs1_len = p1_len - DOT11_BCN_PRB_LEN;

	tlvs2 = (uint8*)p2 + DOT11_BCN_PRB_LEN;
	tlvs2_len = p2_len - DOT11_BCN_PRB_LEN;

	/* allocate a buffer big enough for the merged ies */
	*merged_len = DOT11_BCN_PRB_LEN + wlc_merged_ie_len(wlc, tlvs1, tlvs1_len, tlvs2,
		tlvs2_len);
	*merged = (struct dot11_bcn_prb *) MALLOCZ(wlc->osh, *merged_len);
	if (*merged == NULL) {
		*merged_len = 0;
		WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__, *merged_len,
			MALLOCED(wlc->osh)));
		return BCME_NOMEM;
	}

	/* copy the fixed portion of the second packet so the latest TSF, cap, etc is kept */
	bcopy(p2, *merged, DOT11_BCN_PRB_LEN);

	/* merge the ies from both packets */
	wlc_merge_ies(tlvs1, tlvs1_len, tlvs2, tlvs2_len, (uint8*)*merged + DOT11_BCN_PRB_LEN);

	return BCME_OK;
} /* wlc_merge_bcn_prb */

static int
wlc_merged_ie_len(wlc_info_t *wlc, uint8 *tlvs1, int tlvs1_len, uint8 *tlvs2, int tlvs2_len)
{
	bcm_tlv_t *tlv1 = (bcm_tlv_t*)tlvs1;
	bcm_tlv_t *tlv2 = (bcm_tlv_t*)tlvs2;
	int total;
	int len;

	BCM_REFERENCE(wlc);

	/* treat an empty list or malformed list as empty */
	if (!bcm_valid_tlv(tlv1, tlvs1_len)) {
		tlv1 = NULL;
		tlvs1 = NULL;
	}
	if (!bcm_valid_tlv(tlv2, tlvs2_len)) {
		tlv2 = NULL;
		tlvs2 = NULL;
	}

	total = 0;

	len = tlvs2_len;
	while (tlv2) {
		total += TLV_HDR_LEN + tlv2->len;
		tlv2 = bcm_next_tlv(tlv2, &len);
	}

	len = tlvs1_len;
	while (tlv1) {
		if (!wlc_find_ie_match(tlv1, (bcm_tlv_t*)tlvs2, tlvs2_len))
			total += TLV_HDR_LEN + tlv1->len;

		tlv1 = bcm_next_tlv(tlv1, &len);
	}

	return total;
}

static bool
wlc_find_ie_match(bcm_tlv_t *ie, bcm_tlv_t *ies, int len)
{
	uint8 ie_len;

	ie_len = ie->len;

	while (ies) {
		if (ie_len == ies->len && !bcmp(ie, ies, (TLV_HDR_LEN + ie_len))) {
			return TRUE;
		}
		ies = bcm_next_tlv(ies, &len);
	}

	return FALSE;
}

static void
wlc_merge_ies(uint8 *tlvs1, int tlvs1_len, uint8 *tlvs2, int tlvs2_len, uint8* merge)
{
	bcm_tlv_t *tlv1 = (bcm_tlv_t*)tlvs1;
	bcm_tlv_t *tlv2 = (bcm_tlv_t*)tlvs2;
	int len;

	/* treat an empty list or malformed list as empty */
	if (!bcm_valid_tlv(tlv1, tlvs1_len)) {
		tlv1 = NULL;
		tlvs1 = NULL;
	}
	if (!bcm_valid_tlv(tlv2, tlvs2_len)) {
		tlv2 = NULL;
		tlvs2 = NULL;
	}

	/* copy in the ies from the second set */
	len = tlvs2_len;
	while (tlv2) {
		bcopy(tlv2, merge, TLV_HDR_LEN + tlv2->len);
		merge += TLV_HDR_LEN + tlv2->len;

		tlv2 = bcm_next_tlv(tlv2, &len);
	}

	/* merge in the ies from the first set */
	len = tlvs1_len;
	while (tlv1) {
		if (!wlc_find_ie_match(tlv1, (bcm_tlv_t*)tlvs2, tlvs2_len)) {
			bcopy(tlv1, merge, TLV_HDR_LEN + tlv1->len);
			merge += TLV_HDR_LEN + tlv1->len;
		}
		tlv1 = bcm_next_tlv(tlv1, &len);
	}
}

/** adopt IBSS parameters in cfg->target_bss */
void
wlc_join_adopt_ibss_params(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	/* adopt the IBSS parameters */
	wlc_join_adopt_bss(cfg);

	cfg->roam->time_since_bcn = 1;
	cfg->roam->bcns_lost = TRUE;

#ifdef WLMCNX
	/* use p2p ucode and support driver code... */
	if (MCNX_ENAB(wlc->pub)) {
		cfg->current_bss->dtim_period = 1;
		wlc_mcnx_dtim_upd(wlc->mcnx, cfg, TRUE);
	}
#endif // endif
}

void
wlc_adopt_dtim_period(wlc_bsscfg_t *cfg, uint8 dtim_period)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_bss_info_t *current_bss = cfg->current_bss;
	current_bss->dtim_period = dtim_period;
#ifdef WLMCNX
	if (MCNX_ENAB(wlc->pub))
		; /* empty */
	else
#endif // endif
	if (cfg == wlc->cfg)
		wlc_write_shm(wlc, M_DOT11_DTIMPERIOD(wlc), current_bss->dtim_period);
	wlc_update_bcn_info(cfg, TRUE);

#ifdef WLMCNX
	if (MCNX_ENAB(wlc->pub))
		wlc_mcnx_dtim_upd(wlc->mcnx, cfg, TRUE);
#endif // endif
}

void
wlc_join_complete(wlc_bsscfg_t *cfg, wlc_d11rxhdr_t *wrxh, uint8 *plcp,
	struct dot11_bcn_prb *bcn, int bcn_len)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	wlc_bss_info_t *target_bss = cfg->target_bss;
	uint8* tlvs;
	int tlvs_len;
	bcm_tlv_t *tim = NULL;
	struct dot11_bcn_prb *merged;
	int merged_len;
	wlc_bss_info_t *current_bss = cfg->current_bss;
	wlc_pm_st_t *pm = cfg->pm;
	uint type = as->type;

#ifdef DEBUG_TBTT
	wlc->bad_TBTT = FALSE;
#endif /* DEBUG_TBTT */

	wlc_tsf_adopt_bcn(cfg, wrxh, plcp, bcn);

	tlvs = (uint8*)bcn + DOT11_BCN_PRB_LEN;
	tlvs_len = bcn_len - DOT11_BCN_PRB_LEN;

	if (target_bss->bss_type == DOT11_BSSTYPE_INFRASTRUCTURE) {
		/* adopt DTIM period for PS-mode support */
		tim = bcm_parse_tlvs(tlvs, tlvs_len, DOT11_MNG_TIM_ID);
		if (tim != NULL && tim->len >= DOT11_MNG_TIM_FIXED_LEN) {
			wlc_adopt_dtim_period(cfg, tim->data[DOT11_MNG_TIM_DTIM_PERIOD]);
		}
		/* illed AP, prevent going to power-save mode */
		else {
			wlc_set_pmoverride(cfg, TRUE);
		}
	} else /* if (target_bss->bss_type == DOT11_BSSTYPE_INDEPENDENT) */ {
		if (BSS_WME_ENAB(wlc, cfg)) {
			const bcm_tlv_t *wme_ie = wlc_find_wme_ie(tlvs, tlvs_len);

			if (wme_ie) {
				cfg->flags |= WLC_BSSCFG_WME_ASSOC;
			}
		}
		wlc_join_adopt_ibss_params(wlc, cfg);
	}

	/* Merge the current saved probe response IEs with the beacon's in case the
	 * beacon is missing some info.
	 */
	wlc_merge_bcn_prb(wlc, current_bss->bcn_prb, current_bss->bcn_prb_len,
	                  bcn, bcn_len, &merged, &merged_len);

	/* save bcn's fixed and tagged parameters in current_bss */
	if (merged != NULL) {
		if (current_bss->bcn_prb)
			MFREE(wlc->osh, current_bss->bcn_prb, current_bss->bcn_prb_len);
		current_bss->bcn_prb = merged;
		current_bss->bcn_prb_len = (uint16)merged_len;
		current_bss->bcnlen = (uint16)bcn_len;
	}

	/* Grab and use the Country IE from the AP we are joining and override any Country IE
	 * we may have obtained from somewhere else.
	 */
	if ((BSS_WL11H_ENAB(wlc, cfg) || WLC_AUTOCOUNTRY_ENAB(wlc)) &&
	    (target_bss->bss_type == DOT11_BSSTYPE_INFRASTRUCTURE)) {
		wlc_cntry_adopt_country_ie(wlc->cntry, cfg, tlvs, tlvs_len);
	}

	/* If the PM2 Receive Throttle Duty Cycle feature is active, reset it */
	if (PM2_RCV_DUR_ENAB(cfg) && pm->PM == PM_FAST) {
		WL_RTDC(wlc, "wlc_join_complete: PMep=%02u AW=%02u",
			(pm->PMenabled ? 10 : 0) | pm->PMpending,
			(PS_ALLOWED(cfg) ? 10 : 0) | STAY_AWAKE(wlc));
		wlc_pm2_rcv_reset(cfg);
	}

	/* 802.11 PM can be entered now that we have synchronized with
	 * the BSS's TSF and DTIM count.
	 */
	if (PS_ALLOWED(cfg))
		wlc_set_pmstate(cfg, TRUE);
	/* sync/reset h/w */
	else {
		if (pm->PM == PM_FAST) {
			WL_RTDC(wlc, "wlc_join_complete: start srtmr, PMep=%02u AW=%02u",
			        (pm->PMenabled ? 10 : 0) | pm->PMpending,
			        (PS_ALLOWED(cfg) ? 10 : 0) | STAY_AWAKE(wlc));
			wlc_pm2_sleep_ret_timer_start(cfg, 0);
		}
		wlc_set_pmstate(cfg, FALSE);
	}

	/* If assoc_state is not AS_IDLE, then this join event is completing a roam or a
	 * join operation in an BSS; or finishing a join operatin in an IBSS.
	 * If assoc_state is AS_IDLE, then we are here due to an IBSS coalesce and there is
	 * no more follow on work.
	 */
	if (as->state != AS_IDLE) {
		/* send event to indicate final step in joining BSS */
		wlc_bss_mac_event(
			wlc, cfg, WLC_E_JOIN, &cfg->BSSID, WLC_E_STATUS_SUCCESS, 0,
			target_bss->bss_type, 0, 0);
		wlc_join_done_int(cfg, WLC_E_STATUS_SUCCESS);
	}

#ifdef SCAN_JOIN_TIMEOUT
	wlc->join_timeout = 0;
#endif // endif
	/* notifying interested parties of the state... */
	wlc_bss_assoc_state_notif(wlc, cfg, type, AS_IDLE);
} /* wlc_join_complete */

static int
wlc_join_pref_tlv_len(wlc_info_t *wlc, uint8 *pref, int len)
{
	BCM_REFERENCE(wlc);

	if (len < TLV_HDR_LEN)
		return 0;

	switch (pref[TLV_TAG_OFF]) {
	case WL_JOIN_PREF_RSSI:
	case WL_JOIN_PREF_BAND:
	case WL_JOIN_PREF_RSSI_DELTA:
		return TLV_HDR_LEN + pref[TLV_LEN_OFF];
	case WL_JOIN_PREF_WPA:
		if (len < TLV_HDR_LEN + WLC_JOIN_PREF_LEN_FIXED) {
			WL_ASSOC_ERROR(("wl%d: mulformed WPA cfg in join pref\n", WLCWLUNIT(wlc)));
			return -1;
		}
		return TLV_HDR_LEN + WLC_JOIN_PREF_LEN_FIXED +
		        pref[TLV_HDR_LEN + WLC_JOIN_PREF_OFF_COUNT] * 12;
	default:
		WL_ASSOC_ERROR(("wl%d: unknown join pref type\n", WLCWLUNIT(wlc)));
		return -1;
	}

	return 0;
}

static int
wlc_join_pref_parse(wlc_bsscfg_t *cfg, uint8 *pref, int len)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_join_pref_t *join_pref = cfg->join_pref;
	int tlv_pos, tlv_len;
	uint8 type;
	uint8 bits;
	uint8 start;
	uint i;
	uint8 f, c;
	uint p;
	uint8 band;

	WL_TRACE(("wl%d: wlc_join_pref_parse: pref len = %d\n", WLCWLUNIT(wlc), len));

	if (join_pref == NULL)
		return BCME_ERROR;

	band = join_pref->band;
	bzero(join_pref, sizeof(wlc_join_pref_t));
	join_pref->band = band;
	if (!len) {
		wlc_join_pref_band_upd(cfg);
		return 0;
	}

	/*
	* Each join target carries a 'weight', consisting of a number
	* of info such as akm and cipher. The weight is represented by
	* a N bit number. The bigger the number is the more likely the
	* target becomes the join candidate. Each info in the weight
	* is called a field, which is made of a type defined in wlioctl.h
	* and a bit offset assigned by parsing user-supplied "join_pref"
	* iovar. The fields are ordered from the most significant field to
	* the least significant field.
	*/
	/* count # tlvs # bits first */
	for (tlv_pos = 0, f = 0, start = 0, p = 0;
	     (tlv_len = wlc_join_pref_tlv_len(wlc, &pref[tlv_pos],
	                                      len - tlv_pos)) >= TLV_HDR_LEN &&
	             tlv_pos + tlv_len <= len;
	     tlv_pos += tlv_len) {
		type = pref[tlv_pos + TLV_TAG_OFF];
		if (p & WLCTYPEBMP(type)) {
			WL_ASSOC_ERROR(("wl%d: multiple join pref type %d\n",
					WLCWLUNIT(wlc), type));
			goto err;
		}
		switch (type) {
		case WL_JOIN_PREF_RSSI:
			bits = WLC_JOIN_PREF_BITS_RSSI;
			break;
		case WL_JOIN_PREF_WPA:
			bits = WLC_JOIN_PREF_BITS_WPA;
			break;
		case WL_JOIN_PREF_BAND:
			bits = WLC_JOIN_PREF_BITS_BAND;
			break;
		case WL_JOIN_PREF_RSSI_DELTA:
			bits = WLC_JOIN_PREF_BITS_RSSI_DELTA;
			break;
		default:
			WL_ASSOC_ERROR(("wl%d: invalid join pref type %d\n", WLCWLUNIT(wlc), type));
			goto err;
		}
		f++;
		start += bits;
		p |= WLCTYPEBMP(type);
	}
	/* rssi field is mandatory! */
	if (!(p & WLCTYPEBMP(WL_JOIN_PREF_RSSI))) {
		WL_ASSOC_ERROR(("wl%d: WL_JOIN_PREF_RSSI (type %d) is not present\n",
			WLCWLUNIT(wlc), WL_JOIN_PREF_RSSI));
		goto err;
	}

	/* RSSI Delta is not maintained in the join_pref fields */
	if (p & WLCTYPEBMP(WL_JOIN_PREF_RSSI_DELTA))
		f--;

	/* other sanity checks */
	if (start > sizeof(uint32) * 8) {
		WL_ASSOC_ERROR(("wl%d: too many bits %d max %u\n", WLCWLUNIT(wlc), start,
			(uint)sizeof(uint32) * 8));
		goto err;
	}
	if (f > MAXJOINPREFS) {
		WL_ASSOC_ERROR(("wl%d: too many tlvs/prefs %d\n", WLCWLUNIT(wlc), f));
		goto err;
	}
	WL_ASSOC(("wl%d: wlc_join_pref_parse: total %d fields %d bits\n", WLCWLUNIT(wlc), f,
		start));
	/* parse user-supplied join pref list */
	/* reverse the order so that most significant pref goes to the last entry */
	join_pref->fields = f;
	join_pref->prfbmp = p;
	for (tlv_pos = 0;
	     (tlv_len = wlc_join_pref_tlv_len(wlc, &pref[tlv_pos],
	                                      len - tlv_pos)) >= TLV_HDR_LEN &&
	             tlv_pos + tlv_len <= len;
	     tlv_pos += tlv_len) {
		bits = 0;
		switch ((type = pref[tlv_pos + TLV_TAG_OFF])) {
		case WL_JOIN_PREF_RSSI:
			bits = WLC_JOIN_PREF_BITS_RSSI;
			break;
		case WL_JOIN_PREF_WPA:
			c = pref[tlv_pos + TLV_HDR_LEN + WLC_JOIN_PREF_OFF_COUNT];
			bits = WLC_JOIN_PREF_BITS_WPA;
			/* sanity check */
			if (c > WLCMAXCNT(bits)) {
				WL_ASSOC_ERROR(("wl%d: two many wpa configs %d max %d\n",
					WLCWLUNIT(wlc), c, WLCMAXCNT(bits)));
				goto err;
			} else if (!c) {
				WL_ASSOC_ERROR(("wl%d: no wpa config specified\n", WLCWLUNIT(wlc)));
				goto err;
			}
			/* user-supplied list is from most favorable to least favorable */
			/* reverse the order so that the bigger the index the more favorable the
			 * config is
			 */
			for (i = 0; i < c; i ++)
				bcopy(&pref[tlv_pos + TLV_HDR_LEN + WLC_JOIN_PREF_LEN_FIXED +
				            i * sizeof(join_pref->wpa[0])],
				      &join_pref->wpa[c - 1 - i],
				      sizeof(join_pref->wpa[0]));
			join_pref->wpas = c;
			break;
		case WL_JOIN_PREF_BAND:
			bits = WLC_JOIN_PREF_BITS_BAND;
			/* honor use what WLC_SET_ASSOC_PREFER says first */
			if (pref[tlv_pos + TLV_HDR_LEN + WLC_JOIN_PREF_OFF_BAND] ==
				WLJP_BAND_ASSOC_PREF)
				break;
			/* overwrite with this setting */
			join_pref->band = pref[tlv_pos + TLV_HDR_LEN + WLC_JOIN_PREF_OFF_BAND];
			break;
		case WL_JOIN_PREF_RSSI_DELTA:
			bits = WLC_JOIN_PREF_BITS_RSSI_DELTA;
			cfg->join_pref_rssi_delta.rssi = pref[tlv_pos + TLV_HDR_LEN +
			                                 WLC_JOIN_PREF_OFF_DELTA_RSSI];

			cfg->join_pref_rssi_delta.band = pref[tlv_pos + TLV_HDR_LEN +
			                                 WLC_JOIN_PREF_OFF_BAND];
			break;
		}
		if (!bits)
			continue;

		f--;
		start -= bits;

		join_pref->field[f].type = type;
		join_pref->field[f].start = start;
		join_pref->field[f].bits = bits;
		WL_ASSOC(("wl%d: wlc_join_pref_parse: added field %s entry %d offset %d bits %d\n",
			WLCWLUNIT(wlc), WLCJOINPREFN(type), f, start, bits));
	}

	/* band preference can be from a different source */
	if (!(p & WLCTYPEBMP(WL_JOIN_PREF_BAND)))
		wlc_join_pref_band_upd(cfg);

	return 0;

	/* error handling */
err:
	band = join_pref->band;
	bzero(join_pref, sizeof(wlc_join_pref_t));
	join_pref->band = band;
	return BCME_ERROR;
} /* wlc_join_pref_parse */

static int
wlc_join_pref_build(wlc_bsscfg_t *cfg, uint8 *pref, int len)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_join_pref_t *join_pref = cfg->join_pref;
	int total, wpalen = 0;
	uint i, j;

	(void)wlc;

	WL_TRACE(("wl%d: wlc_join_pref_build: buffer len %d\n", WLCWLUNIT(wlc), len));

	if (join_pref == NULL)
		return BCME_ERROR;

	if (!ISALIGNED(pref, sizeof(total))) {
		WL_ASSOC_ERROR(("wl%d: %s: buffer not %d byte aligned\n",
			WLCWLUNIT(wlc), __FUNCTION__, (int)sizeof(total)));
		return BCME_BADARG;
	}

	/* calculate buffer length */
	total = 0;
	for (i = 0; i < join_pref->fields; i ++) {
		switch (join_pref->field[i].type) {
		case WL_JOIN_PREF_RSSI:
			total += TLV_HDR_LEN + WLC_JOIN_PREF_LEN_FIXED;
			break;
		case WL_JOIN_PREF_WPA:
			wpalen = join_pref->wpas * sizeof(join_pref->wpa[0]);
			total += TLV_HDR_LEN + WLC_JOIN_PREF_LEN_FIXED + wpalen;
			break;
		case WL_JOIN_PREF_BAND:
			total += TLV_HDR_LEN + WLC_JOIN_PREF_LEN_FIXED;
			break;
		}
	}

	/* Add separately maintained RSSI Delta entry */
	if (cfg->join_pref_rssi_delta.rssi != 0 && cfg->join_pref_rssi_delta.band != WLC_BAND_AUTO)
		total += TLV_HDR_LEN + WLC_JOIN_PREF_LEN_FIXED;

	if (len < total + (int)sizeof(total)) {
		WL_ASSOC_ERROR(("wl%d: %s: buffer too small need %d bytes\n",
			WLCWLUNIT(wlc), __FUNCTION__, (int)(total + sizeof(total))));
		return BCME_BUFTOOSHORT;
	}

	/* build join pref */
	bcopy(&total, pref, sizeof(total));
	pref += sizeof(total);
	/* reverse the order so that it is same as what user supplied */
	for (i = 0; i < join_pref->fields; i ++) {
		switch (join_pref->field[join_pref->fields - 1 - i].type) {
		case WL_JOIN_PREF_RSSI:
			*pref++ = WL_JOIN_PREF_RSSI;
			*pref++ = WLC_JOIN_PREF_LEN_FIXED;
			*pref++ = 0;
			*pref++ = 0;
			break;
		case WL_JOIN_PREF_WPA:
			*pref++ = WL_JOIN_PREF_WPA;
			*pref++ = WLC_JOIN_PREF_LEN_FIXED +
				(uint8)(sizeof(join_pref->wpa[0]) * join_pref->wpas);
			*pref++ = 0;
			*pref++ = (uint8)join_pref->wpas;
			/* reverse the order so that it is same as what user supplied */
			for (j = 0; j < join_pref->wpas; j ++) {
				bcopy(&join_pref->wpa[join_pref->wpas - 1 - j],
					pref, sizeof(join_pref->wpa[0]));
				pref += sizeof(join_pref->wpa[0]);
			}
			break;
		case WL_JOIN_PREF_BAND:
			*pref++ = WL_JOIN_PREF_BAND;
			*pref++ = WLC_JOIN_PREF_LEN_FIXED;
			*pref++ = 0;
			*pref++ = join_pref->band;
			break;
		}
	}

	/* Add the RSSI Delta information. Note that order is NOT important for this field
	 * as it's always applied
	 */
	if (cfg->join_pref_rssi_delta.rssi != 0 &&
	    cfg->join_pref_rssi_delta.band != WLC_BAND_AUTO) {
		*pref++ = WL_JOIN_PREF_RSSI_DELTA;
		*pref++ = WLC_JOIN_PREF_LEN_FIXED;
		*pref++ = cfg->join_pref_rssi_delta.rssi;
		*pref++ = cfg->join_pref_rssi_delta.band;
	}

	return 0;
} /* wlc_join_pref_build */

static void
wlc_join_pref_band_upd(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_join_pref_t *join_pref = cfg->join_pref;
	uint i;
	uint8 start = 0;
	uint p = 0;

	(void)wlc;

	WL_ASSOC(("wl%d: wlc_join_pref_band_upd: band pref is %d\n",
		WLCWLUNIT(wlc), join_pref->band));
	if (join_pref->band == WLC_BAND_AUTO)
		return;
	/* find band field first. rssi field should be set too if found */
	for (i = 0; i < join_pref->fields; i ++) {
		if (join_pref->field[i].type == WL_JOIN_PREF_BAND) {
			WL_ASSOC(("wl%d: found field %s entry %d\n",
				WLCWLUNIT(wlc), WLCJOINPREFN(WL_JOIN_PREF_BAND), i));
			return;
		}
		start += join_pref->field[i].bits;
		p |= WLCTYPEBMP(join_pref->field[i].type);
	}
	/* rssi field is mandatory. fields should be empty when rssi field is not set */
	if (!(p & WLCTYPEBMP(WL_JOIN_PREF_RSSI))) {
		ASSERT(join_pref->fields == 0);
		join_pref->field[0].type = WL_JOIN_PREF_RSSI;
		join_pref->field[0].start = 0;
		join_pref->field[0].bits = WLC_JOIN_PREF_BITS_RSSI;
		WL_ASSOC(("wl%d: wlc_join_pref_band_upd: added field %s entry 0 offset 0\n",
			WLCWLUNIT(wlc), WLCJOINPREFN(WL_JOIN_PREF_RSSI)));
		start = WLC_JOIN_PREF_BITS_RSSI;
		p |= WLCTYPEBMP(WL_JOIN_PREF_RSSI);
		i = 1;
	}
	/* add band field */
	join_pref->field[i].type = WL_JOIN_PREF_BAND;
	join_pref->field[i].start = start;
	join_pref->field[i].bits = WLC_JOIN_PREF_BITS_BAND;
	WL_ASSOC(("wl%d: wlc_join_pref_band_upd: added field %s entry %d offset %d\n",
		WLCWLUNIT(wlc), WLCJOINPREFN(WL_JOIN_PREF_BAND), i, start));
	p |= WLCTYPEBMP(WL_JOIN_PREF_BAND);
	join_pref->prfbmp = p;
	join_pref->fields = i + 1;
}

static void
wlc_join_pref_reset(wlc_bsscfg_t *cfg)
{
	uint8 band;
	wlc_join_pref_t *join_pref = cfg->join_pref;

	if (join_pref == NULL) {
		WL_ASSOC_ERROR(("wl%d: %s: join pref NULL Error\n", WLCWLUNIT(cfg->wlc),
			__FUNCTION__));
		return;
	}

	band = join_pref->band;
	bzero(join_pref, sizeof(wlc_join_pref_t));
	join_pref->band = band;
	wlc_join_pref_band_upd(cfg);
}

static void
wlc_auth_tx_complete(wlc_info_t *wlc, uint txstatus, void *arg)
{
	wlc_bsscfg_t *cfg = wlc_bsscfg_find_by_ID(wlc, (uint16)(uintptr)arg);
	wlc_assoc_t *as;
	wlc_bss_info_t *target_bss;
	/* do not proceed with auth complete, if reinit is active */
	if (wlc->cmn->reinit_active) {
		return;
	}

	/* in case bsscfg is freed before this callback is invoked */
	if (cfg == NULL) {
		WL_NONE(("wl%d: %s: unable to find bsscfg by ID %p\n",
		          wlc->pub->unit, __FUNCTION__, OSL_OBFUSCATE_BUF(arg)));
		return;
	}

	as = cfg->assoc;
	target_bss = cfg->target_bss;

	/* assoc aborted? */
	if (!(as->state == AS_SENT_AUTH_1 || as->state == AS_SENT_AUTH_3))
		return;

	/* no ack */
	if (!(txstatus & TX_STATUS_ACK_RCV)) {
		wlc_auth_complete(cfg, WLC_E_STATUS_NO_ACK, &target_bss->BSSID, 0, 0);
		return;
	}
	wlc_assoc_timer_del(wlc, cfg);
	wl_add_timer(wlc->wl, as->timer, WECA_AUTH_TIMEOUT + 10, 0);
}

static void
wlc_assocreq_complete(wlc_info_t *wlc, uint txstatus, void *arg)
{
	wlc_bsscfg_t *cfg = wlc_bsscfg_find_by_ID(wlc, (uint16)(uintptr)arg);
	wlc_assoc_t *as;
	wlc_bss_info_t *target_bss;

	/* in case bsscfg is freed before this callback is invoked */
	if (cfg == NULL) {
		WL_NONE(("wl%d: %s: unable to find bsscfg by ID %p\n",
		          wlc->pub->unit, __FUNCTION__, OSL_OBFUSCATE_BUF(arg)));
		return;
	}

	as = cfg->assoc;
	target_bss = cfg->target_bss;

	/* assoc aborted? */
	if (!(as->state == AS_SENT_ASSOC || as->state == AS_REASSOC_RETRY))
		return;

	/* no ack */
	if (!(txstatus & TX_STATUS_ACK_RCV)) {
		wlc_assoc_complete(cfg, WLC_E_STATUS_NO_ACK, &target_bss->BSSID, 0,
			as->type != AS_ASSOCIATION, 0);
		return;
	}
	wlc_assoc_timer_del(wlc, cfg);
	wl_add_timer(wlc->wl, as->timer, WECA_ASSOC_TIMEOUT + 10, 0);
}

static wlc_bss_info_t *
wlc_bss_info_dup(wlc_info_t *wlc, wlc_bss_info_t *bi)
{
	wlc_bss_info_t *bss = MALLOCZ(wlc->osh, sizeof(wlc_bss_info_t));
	if (!bss) {
		WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__,
			(int)sizeof(wlc_bss_info_t), MALLOCED(wlc->osh)));
		return NULL;
	}
	bcopy(bi, bss, sizeof(wlc_bss_info_t));
	if (bi->bcn_prb) {
		if (!(bss->bcn_prb = MALLOCZ(wlc->osh, bi->bcn_prb_len))) {
			WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__, bi->bcn_prb_len,
				MALLOCED(wlc->osh)));
			MFREE(wlc->osh, bss, sizeof(wlc_bss_info_t));
			return NULL;
		}
		bcopy(bi->bcn_prb, bss->bcn_prb, bi->bcn_prb_len);
	}
	return bss;
}

static int
wlc_bss_list_expand(wlc_bsscfg_t *cfg, wlc_bss_list_t *from, wlc_bss_list_t *to)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_join_pref_t *join_pref = cfg->join_pref;
	uint i, j, k, c;
	wlc_bss_info_t *bi;
	struct rsn_parms *rsn;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_ASSOC */
	wpa_suite_t *akm, *uc, *mc;

	WL_ASSOC(("wl%d: wlc_bss_list_expand: scan results %d\n", WLCWLUNIT(wlc), from->count));

	ASSERT(to->count == 0);
	if (WLCAUTOWPA(cfg)) {
		/* duplicate each bss to multiple copies, one for each wpa config */
		for (i = 0, c = 0; i < from->count && c < (uint) wlc->pub->tunables->maxbss; i ++) {
			/* ignore the bss if it does not support wpa */
			if (!(from->ptrs[i]->flags & (WLC_BSS_WPA | WLC_BSS_WPA2)))
			{
				WL_ASSOC(("wl%d: ignored BSS %s, it does not do WPA!\n",
					WLCWLUNIT(wlc),
					bcm_ether_ntoa(&from->ptrs[i]->BSSID, eabuf)));
				continue;
			}

			/*
			* walk thru all wpa configs, move/dup the bss to join targets list
			* if it supports the config.
			*/
			for (j = 0, rsn = NULL, bi = NULL;
			    j < join_pref->wpas && c < (uint) wlc->pub->tunables->maxbss; j ++) {
				WL_ASSOC(("wl%d: WPA cfg %d:"
				          " %02x%02x%02x%02x-%02x%02x%02x%02x-%02x%02x%02x%02x\n",
				          WLCWLUNIT(wlc), j,
				          join_pref->wpa[j].akm[0],
				          join_pref->wpa[j].akm[1],
				          join_pref->wpa[j].akm[2],
				          join_pref->wpa[j].akm[3],
				          join_pref->wpa[j].ucipher[0],
				          join_pref->wpa[j].ucipher[1],
				          join_pref->wpa[j].ucipher[2],
				          join_pref->wpa[j].ucipher[3],
				          join_pref->wpa[j].mcipher[0],
				          join_pref->wpa[j].mcipher[1],
				          join_pref->wpa[j].mcipher[2],
				          join_pref->wpa[j].mcipher[3]));
				/* check if the AP supports the wpa config */
				akm = (wpa_suite_t*)join_pref->wpa[j].akm;
				uc = (wpa_suite_t*)join_pref->wpa[j].ucipher;

				if (!bcmp(akm, WPA_OUI, DOT11_OUI_LEN))
					rsn = bi ? &bi->wpa : &from->ptrs[i]->wpa;
				else if (!bcmp(akm, WPA2_OUI, DOT11_OUI_LEN))
					rsn = bi ? &bi->wpa2 : &from->ptrs[i]->wpa2;
				else {
				/*
				* something has gone wrong, or need to add
				* new code to handle the new akm here!
				*/
					WL_ASSOC_ERROR(("wl%d: unknown akm suite %02x%02x%02x%02x"
							"in WPA	cfg\n",	WLCWLUNIT(wlc),	akm->oui[0],
							akm->oui[1], akm->oui[2], akm->type));
					continue;
				}
#ifdef BCMDBG
				if (WL_ASSOC_ON()) {
					prhex("rsn parms", (uint8 *)rsn, sizeof(*rsn));
				}
#endif /* BCMDBG */
				/* check if the AP offers the akm */
				for (k = 0; k < rsn->acount; k ++) {
					if (akm->type == rsn->auth[k])
						break;
				}
				/* the AP does not offer the akm! */
				if (k >= rsn->acount) {
					WL_ASSOC(("wl%d: skip WPA cfg %d: akm not match\n",
						WLCWLUNIT(wlc), j));
					continue;
				}
				/* check if the AP offers the unicast cipher */
				for (k = 0; k < rsn->ucount; k ++) {
					if (uc->type == rsn->unicast[k])
						break;
				}
				/* AP does not offer the cipher! */
				if (k >= rsn->ucount)
					continue;
				/* check if the AP offers the multicast cipher */
				mc = (wpa_suite_t*)join_pref->wpa[j].mcipher;
				if (bcmp(mc, WL_WPA_ACP_MCS_ANY, WPA_SUITE_LEN)) {
					if (mc->type != rsn->multicast) {
						WL_ASSOC(("wl%d: skip WPA cfg %d: mc not match\n",
							WLCWLUNIT(wlc), j));
						continue;
					}
				}
				/* move/duplicate the BSS */
				if (!bi) {
					to->ptrs[c] = bi = from->ptrs[i];
					from->ptrs[i] = NULL;
				} else if (!(to->ptrs[c] = wlc_bss_info_dup(wlc, bi))) {
					WL_ASSOC_ERROR(("wl%d: failed to duplicate bss info\n",
						WLCWLUNIT(wlc)));
					goto err;
				}
				WL_ASSOC(("wl%d: BSS %s and WPA cfg %d match\n", WLCWLUNIT(wlc),
					bcm_ether_ntoa(&bi->BSSID, eabuf), j));
				/* save multicast cipher for WPA config derivation */
				if (!bcmp(mc, WL_WPA_ACP_MCS_ANY, WPA_SUITE_LEN))
					to->ptrs[c]->mcipher = rsn->multicast;
				/* cache the config index as preference weight */
				to->ptrs[c]->wpacfg = (uint8)j;
				/* mask off WPA or WPA2 flag to match the selected entry */
				if (!bcmp(uc->oui, WPA2_OUI, DOT11_OUI_LEN))
					to->ptrs[c]->flags &= ~WLC_BSS_WPA;
				else
					to->ptrs[c]->flags &= ~WLC_BSS_WPA2;
				c ++;
			}
			/* the BSS does not support any of our wpa configs */
			if (!bi) {
				WL_ASSOC(("wl%d: ignored BSS %s, it does not offer expected WPA"
					" cfg!\n",
					WLCWLUNIT(wlc),
					bcm_ether_ntoa(&from->ptrs[i]->BSSID, eabuf)));
				continue;
			}
		}
	} else {
		c = 0;
		WL_ASSOC_ERROR(("wl%d: don't know how to expand the list\n", WLCWLUNIT(wlc)));
		goto err;
	}

	/* what if the join_target list is too big */
	if (c >= (uint) wlc->pub->tunables->maxbss) {
		WL_ASSOC_ERROR(("wl%d: two many entries, scan results may not be fully expanded\n",
			WLCWLUNIT(wlc)));
	}

	/* done */
	to->count = c;
	WL_ASSOC(("wl%d: wlc_bss_list_expand: join targets %d\n", WLCWLUNIT(wlc), c));

	/* clean up the source list */
	wlc_bss_list_free(wlc, from);
	return 0;

	/* error handling */
err:
	to->count = c;
	wlc_bss_list_free(wlc, to);
	wlc_bss_list_free(wlc, from);
	return BCME_ERROR;
} /* wlc_bss_list_expand */

static void wlc_assocresp_client_next(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	assoc_decision_t * assoc_decision, struct scb *scb);

void wlc_process_assocresp_decision(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, assoc_decision_t *dc)
{
	wlc_assocresp_client_next(wlc, bsscfg, dc, NULL);
}

static void wlc_assocresp_client_next(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	assoc_decision_t * dc, struct scb *scb)
{
	wlc_assoc_t *as = cfg->assoc;
	wlc_bss_info_t *target_bss = cfg->target_bss;
	uint16 status;
#if defined(BCMDBG) || defined(BCMDBG_ERR) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN];

	bcm_ether_ntoa(&dc->da, eabuf);
#endif // endif

	scb = wlc_scbfind(wlc, cfg, &dc->da);
	if (scb == NULL) {
		WL_ERROR(("wl%d.%d %s could not find scb\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(cfg), eabuf));
		return;
	}

	status = (dc->assoc_approved == TRUE) ? DOT11_SC_SUCCESS : dc->reject_reason;

	if (status != DOT11_SC_SUCCESS) {
		wlc_assoc_complete(cfg, WLC_E_STATUS_FAIL, &target_bss->BSSID,
			status, as->type != AS_ASSOCIATION,
			target_bss->bss_type);
		return;
	}

	wlc_assoc_success(cfg, scb);

	wlc_assoc_complete(cfg, WLC_E_STATUS_SUCCESS, &target_bss->BSSID,
		status, as->type != AS_ASSOCIATION,
		target_bss->bss_type);

	WL_ASSOC(("wl%d.%d: Checking if key needs to be inserted\n",
		WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg)));
	/* If Multi-SSID is enabled, and Legacy WEP is in use for this bsscfg,
	 * a "pairwise" key must be created by copying the default key from the bsscfg.
	 */

	if (cfg->WPA_auth == WPA_AUTH_DISABLED)
		WL_ASSOC(("wl%d: WPA disabled\n", WLCWLUNIT(wlc)));
	if (WSEC_WEP_ENABLED(cfg->wsec))
		WL_ASSOC(("wl%d: WEP enabled\n", WLCWLUNIT(wlc)));
	if (MBSS_ENAB(wlc->pub))
		WL_ASSOC(("wl%d: MBSS on\n", WLCWLUNIT(wlc)));
	if ((MBSS_ENAB(wlc->pub) || PSTA_ENAB(wlc->pub) || cfg != wlc->cfg) &&
		cfg->WPA_auth == WPA_AUTH_DISABLED && WSEC_WEP_ENABLED(cfg->wsec)) {

		wlc_key_t *key;
		wlc_key_info_t key_info;
		uint8 data[WEP128_KEY_SIZE];
		size_t data_len;
		wlc_key_algo_t algo;
		int err;

#ifdef PSTA
		key = wlc_keymgmt_get_bss_tx_key(wlc->keymgmt,
			(BSSCFG_PSTA(cfg) ? wlc_bsscfg_primary(wlc) : cfg), FALSE, &key_info);
#else /* PSTA */
		key = wlc_keymgmt_get_bss_tx_key(wlc->keymgmt, cfg, FALSE, &key_info);
#endif /* PSTA */
		BCM_REFERENCE(data_len);
		BCM_REFERENCE(data);
		BCM_REFERENCE(key);

		algo = key_info.algo;
		if (algo != CRYPTO_ALGO_OFF) {
			WL_ASSOC(("wl%d: Def key installed\n", WLCWLUNIT(wlc)));
			if (algo == CRYPTO_ALGO_WEP1 || algo == CRYPTO_ALGO_WEP128) {
				wlc_key_t *bss_key;
				BCM_REFERENCE(bss_key);
				WL_ASSOC(("wl%d: Inserting key for %s\n", wlc->pub->unit, eabuf));
				err = wlc_key_get_data(key, data, sizeof(data), &data_len);
				if (err == BCME_OK) {
					bss_key = wlc_keymgmt_get_key_by_addr(wlc->keymgmt,
						cfg, &target_bss->BSSID, 0, NULL);
					err = wlc_key_set_data(bss_key, algo, data, data_len);
				}
				if (err != BCME_OK)
					WL_ASSOC_ERROR(("wl%d.%d: Error %d inserting key for"
						"bss %s\n", WLCWLUNIT(wlc),
						WLC_BSSCFG_IDX(cfg), err, eabuf));
			}
		}
	}
}

void
wlc_assocresp_client(wlc_bsscfg_t *cfg, struct scb *scb,
	struct dot11_management_header *hdr, uint8 *body, uint body_len)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	wlc_bss_info_t *target_bss = cfg->target_bss;
	struct dot11_assoc_resp *assoc = (struct dot11_assoc_resp *) body;
	uint16 fk;
	uint16 status;
	wlc_iem_upp_t upp;
	wlc_iem_ft_pparm_t ftpparm;
	wlc_iem_pparm_t pparm;
	assoc_decision_t decision;
#if defined(BCMDBG) || defined(BCMDBG_ERR) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN];

	bcm_ether_ntoa(&hdr->sa, eabuf);
#endif /* BCMDBG || BCMDBG_ERR || WLMSG_ASSOC */

	ASSERT(BSSCFG_STA(cfg));

	fk = ltoh16(hdr->fc) & FC_KIND_MASK;
	status = ltoh16(assoc->status);

	if (!(as->state == AS_SENT_ASSOC || as->state == AS_REASSOC_RETRY) ||
		bcmp(hdr->bssid.octet, target_bss->BSSID.octet, ETHER_ADDR_LEN)) {
		WL_NONE(("wl%d.%d: unsolicited association response from %s\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), eabuf));
		wlc_assoc_complete(cfg, WLC_E_STATUS_UNSOLICITED, &hdr->sa, 0,
			fk != FC_ASSOC_RESP, 0);
		return;
	}

	/* capability */
#ifdef BCMDBG
	if (WL_ERROR_ON()) {
		if (!(ltoh16(assoc->capability) & DOT11_CAP_ESS)) {
			WL_ASSOC_ERROR(("wl%d: association response without ESS set from %s\n",
				wlc->pub->unit, eabuf));
		}
		if (ltoh16(assoc->capability) & DOT11_CAP_IBSS) {
			WL_ASSOC_ERROR(("wl%d: association response with IBSS set from %s\n",
				wlc->pub->unit, eabuf));
		}
	}
#endif /* BCMDBG */

	/* save last (re)association response */
	if (as->resp) {
		MFREE(wlc->osh, as->resp, as->resp_len);
		as->resp_len = 0;
	}
	if (!(as->resp = MALLOCZ(wlc->osh, body_len)))
		WL_ERROR((WLC_BSS_MALLOC_ERR, WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg), __FUNCTION__,
			(int)body_len, MALLOCED(wlc->osh)));
	else {
		as->resp_len = body_len;
		bcopy((char*)assoc, (char*)as->resp, body_len);
	}

	/* association denied */
	if (status == DOT11_SC_REASSOC_FAIL &&
	    as->type != AS_ASSOCIATION &&
	    as->state != AS_REASSOC_RETRY) {
		ASSERT(scb != NULL);

		wlc_join_assoc_start(wlc, cfg, scb, target_bss, FALSE);
		wlc_assoc_change_state(cfg, AS_REASSOC_RETRY);
		WL_ASSOC(("wl%d: Retrying with Assoc Req frame "
			"due to Reassoc Req failure (DOT11_SC_REASSOC_FAIL) from %s\n",
			wlc->pub->unit, eabuf));
		return;
	} else if (status == DOT11_SC_ASSOC_TRY_LATER) {
		if (as->bss_retries < as->retry_max) {
			/* retry sending assoc req after temporary association denial by AP */
			WL_ASSOC(("wl%d: Retrying assoc "
				"due to Assoc Req failure (DOT11_SC_ASSOC_TRY_LATER) from %s\n",
				wlc->pub->unit, eabuf));

			wlc_assoc_process_retry_later(cfg, scb, fk, body, body_len);
		}
		else {
			wlc_assoc_complete(cfg, WLC_E_STATUS_FAIL, &target_bss->BSSID,
				status, as->type != AS_ASSOCIATION,
				target_bss->bss_type);
		}
		return;
	} else if (status == DOT11_SC_INVALID_PMKID) {
			wlc_assoc_abort(cfg);
			return;
	} else if (status != DOT11_SC_SUCCESS) {
#ifdef WL_OCE
		if (OCE_ENAB(wlc->pub) &&
			status == OCE_ASSOC_REJECT_RC_INSUFFICIENT_RSSI) {
			wlc_oce_process_assoc_reject(cfg, scb, fk, body, body_len);
		}
#endif // endif
		wlc_assoc_complete(cfg, WLC_E_STATUS_FAIL, &target_bss->BSSID,
			status, as->type != AS_ASSOCIATION,
			target_bss->bss_type);
		return;
	}

	ASSERT(scb != NULL);

	/* Mark assoctime for use in roam calculations */
	scb->assoctime = wlc->pub->now;

	body += sizeof(struct dot11_assoc_resp);
	body_len -= sizeof(struct dot11_assoc_resp);

	wlc_bss_mac_event(wlc, cfg, WLC_E_ASSOC_RESP_IE, NULL, 0, 0, 0,
	                  body, (int)body_len);

	/* prepare IE mgmt calls */
	wlc_iem_parse_upp_init(wlc->iemi, &upp);
	bzero(&ftpparm, sizeof(ftpparm));
	ftpparm.assocresp.scb = scb;
	ftpparm.assocresp.status = status;
	bzero(&pparm, sizeof(pparm));
	pparm.ft = &ftpparm;

	/* parse IEs */
	if (wlc_iem_parse_frame(wlc->iemi, cfg, fk, &upp, &pparm,
	                        body, body_len) != BCME_OK) {
		return;
	}
	status = ftpparm.assocresp.status;

#ifdef WLP2P
	if (BSS_P2P_ENAB(wlc, cfg)) {
		wlc_p2p_process_assocresp(wlc->p2p, scb, body, body_len);
	}
#endif // endif

	/* Association success */
	cfg->AID = ltoh16(assoc->aid);

#if defined(SPLIT_ASSOC)
	if (SPLIT_ASSOC_RESP(cfg)) {
		/* send event */
		wlc_bss_mac_event(wlc, cfg, WLC_E_PRE_ASSOC_RSEP_IND,
			&hdr->sa, WLC_E_STATUS_SUCCESS, status, 0, body, body_len);
#if defined(BCMDBG) || defined(BCMDBG_ERR) || defined(WLMSG_ASSOC)
		WL_ASSOC(("wl%d: %s: recv assoc resp from (%s) systime(%u)\n",
			wlc->pub->unit, __FUNCTION__, eabuf, OSL_SYSUPTIME()));
#endif // endif
		return;
	} else
#endif /* SPLIT_ASSOC */
	{
		decision.assoc_approved = (status == DOT11_SC_SUCCESS) ? TRUE : FALSE;
		decision.reject_reason = (status == DOT11_SC_SUCCESS) ? 0 : status;
		bcopy(&scb->ea, &decision.da, ETHER_ADDR_LEN);
		wlc_assocresp_client_next(wlc, cfg, &decision, scb);
	}
} /* wlc_assocresp_client */

void
wlc_authresp_client(wlc_bsscfg_t *cfg, struct dot11_management_header *hdr,
	uint8 *body, uint body_len, bool short_preamble)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	wlc_bss_info_t *target_bss = cfg->target_bss;
#ifdef BCMDBG_ERR
	char eabuf[ETHER_ADDR_STR_LEN], *sa = bcm_ether_ntoa(&hdr->sa, eabuf);
#endif /* BCMDBG_ERR */
#ifdef WL_CLIENT_SAE
	uint8 *body_start = body;
	uint body_len_start = body_len;
#endif /* WL_CLIENT_SAE */
	struct dot11_auth *auth = (struct dot11_auth *) body;
	uint16 auth_alg, auth_seq, auth_status;
	int status = BCME_OK;
	uint cfg_auth_alg;
	bool unsolicited_pkt = FALSE;
	struct scb *scb;
	wlc_iem_upp_t upp;
	wlc_iem_ft_pparm_t ftpparm;
	wlc_iem_pparm_t pparm;

#ifdef WL_ASSOC_MGR
	uint original_body_length = body_len;
#endif /* WL_ASSOC_MGR */
	WL_ASSOC(("wl%d: wlc_authresp_client\n", wlc->pub->unit));
	auth_alg = ltoh16(auth->alg);
	auth_seq = ltoh16(auth->seq);
	auth_status = ltoh16(auth->status);

	/* ignore authentication frames from other stations */
	if (bcmp((char*)&hdr->sa, (char*)&target_bss->BSSID, ETHER_ADDR_LEN) ||
	    (scb = wlc_scbfind(wlc, cfg, (struct ether_addr *)&hdr->sa)) == NULL ||
	    (as->state != AS_SENT_AUTH_1 && as->state != AS_SENT_AUTH_3)) {
		WL_NONE(("wl%d.%d: unsolicited authentication response from %s\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), sa));
		wlc_auth_complete(cfg, WLC_E_STATUS_UNSOLICITED, &hdr->sa, 0, 0);
		return;
	}

	ASSERT(scb != NULL);

#if defined(AP) && defined(STA)
	if (SCB_ASSOCIATED(scb) || !(scb->flags & SCB_MYAP)) {
		WL_APSTA(("wl%d: got AUTH from %s, associated but not AP, forcing AP flag\n",
			wlc->pub->unit, sa));
	}
#endif /* APSTA */
	scb->flags |= SCB_MYAP;

	/* For SAE AUTH_1 and AUTH_2 will have seq. no. 1
	 * and AUTH_3, AUTH_4 will have seq. no. 2.
	 */
#ifdef WL_CLIENT_SAE
	if (auth_alg == DOT11_SAE) {
		if ((as->state == AS_SENT_AUTH_1 && auth_seq != 1) ||
		(as->state == AS_SENT_AUTH_3 && auth_seq != 2))
			unsolicited_pkt = TRUE;
	} else
#endif /* WL_CLIENT_SAE */
	{
		if ((as->state == AS_SENT_AUTH_1 && auth_seq != 2) ||
		(as->state == AS_SENT_AUTH_3 && auth_seq != 4))
			unsolicited_pkt = TRUE;
	}
	if (unsolicited_pkt == TRUE) {
		WL_ASSOC_ERROR(("wl%d: out-of-sequence authentication response from %s\n",
		          wlc->pub->unit, sa));
		wlc_auth_complete(cfg, WLC_E_STATUS_UNSOLICITED, &hdr->sa, 0, 0);
		return;
	}

	/* authentication error */
	if (auth_status != DOT11_SC_SUCCESS) {
#ifdef WLFBT
		if ((auth_status == DOT11_SC_ASSOC_R0KH_UNREACHABLE) ||
				(auth_status == DOT11_SC_INVALID_PMKID)) {
			wlc_fbt_clear_ies(wlc->fbt, cfg);
		}
		if (auth_status == DOT11_SC_ASSOC_R0KH_UNREACHABLE) {
			wlc_assoc_abort(cfg);
		}
#endif /* WLFBT */
		if (!(BSSCFG_EXT_AUTH(cfg) && auth_status == DOT11_SC_ANTICLOG_TOCKEN_REQUIRED)) {
			wlc_auth_complete(cfg, WLC_E_STATUS_FAIL, &hdr->sa, auth_status, auth_alg);
			return;
		}
	}
	/* invalid authentication algorithm number */
#ifdef WLFBT
	if (!BSSCFG_IS_FBT(cfg)) {
#endif // endif
		cfg_auth_alg = cfg->auth;

	if (auth_alg != cfg_auth_alg && !cfg->openshared) {
		WL_ASSOC_ERROR(("wl%d: invalid authentication algorithm number,"
				"got %d, expected %d\n", wlc->pub->unit, auth_alg, cfg_auth_alg));
		wlc_auth_complete(cfg, WLC_E_STATUS_FAIL, &hdr->sa, auth_status, auth_alg);
		return;
	}
#ifdef WLFBT
	}
#endif // endif

	body += sizeof(struct dot11_auth);
	body_len -= sizeof(struct dot11_auth);

	/* prepare IE mgmt calls */
	wlc_iem_parse_upp_init(wlc->iemi, &upp);
	bzero(&ftpparm, sizeof(ftpparm));
	ftpparm.auth.alg = auth_alg;
	ftpparm.auth.seq = auth_seq;
	ftpparm.auth.scb = scb;
	ftpparm.auth.status = (uint16)auth_status;
	bzero(&pparm, sizeof(pparm));
	pparm.ft = &ftpparm;

	/* parse IEs */
	if (wlc_iem_parse_frame(wlc->iemi, cfg, FC_AUTH, &upp, &pparm,
	                        body, body_len) != BCME_OK) {
		return;
	}
	auth_status = ftpparm.auth.status;
#if defined(WL_CLIENT_SAE)
	if (BSSCFG_EXT_AUTH(cfg)) {

		/* Don't process the Sae Auth packet , if the
		 * cfg->auth and auth_alg is != DOT11_SAE
		 */
		if (auth_alg != DOT11_SAE)
			return;

		if ((auth_status == DOT11_SC_SUCCESS) &&
			(auth_seq == WL_SAE_CONFIRM)) {
			wlc_auth_complete(cfg, status, &hdr->sa, auth_status, auth_alg);
		}

		WL_ASSOC(("wl%d: Sae External Auth: auth_seq = %d\n", WLCWLUNIT(wlc),
				auth_seq));
		wlc_assoc_change_state(cfg, AS_SENT_AUTH_UP);
		wlc_assoc_timer_del(wlc, cfg);
		wl_add_timer(wlc->wl, cfg->assoc->timer, WLC_EXT_AUTH_TIMEOUT, 0);

		wlc_bss_mac_event(wlc, cfg, WLC_E_AUTH, &hdr->sa, status,
				auth_status, auth_alg, body_start, body_len_start);
		return;
	}
#endif /* WL_CLIENT_SAE */

	if (auth_status == DOT11_SC_SUCCESS && auth_alg == DOT11_SHARED_KEY && auth_seq == 2) {
		uint8 *challenge;

		WL_ASSOC(("wl%d: JOIN: got authentication response seq 2 ...\n",
		            WLCWLUNIT(wlc)));

		challenge = ftpparm.auth.challenge;
		ASSERT(challenge != NULL);

		wlc_assoc_timer_del(wlc, cfg);

		(void)wlc_assoc_sendauth(wlc, cfg, &scb->ea, &target_bss->BSSID, scb,
			DOT11_SHARED_KEY, 3, DOT11_SC_SUCCESS, challenge, short_preamble,
			AS_SENT_AUTH_3);
	} else {

		/* For SNonce mismatch in fast transition, discard the frame,
		 * send unsolicited event to the host and wait for the next valid auth response.
		 */
		if ((auth_alg == DOT11_FAST_BSS) && (auth_status == DOT11_SC_INVALID_SNONCE)) {
			status = WLC_E_STATUS_UNSOLICITED;
		} else {
			status = (auth_status == DOT11_SC_SUCCESS) ?
				WLC_E_STATUS_SUCCESS : WLC_E_STATUS_FAIL;
		}

		ASSERT(auth_seq == 2 || (auth_alg == DOT11_SHARED_KEY && auth_seq == 4));
#ifdef WL_ASSOC_MGR
		if (WL_ASSOC_MGR_ENAB(wlc->pub) && status == WLC_E_STATUS_SUCCESS) {
			wlc_assoc_mgr_save_auth_resp(wlc->assoc_mgr, cfg, (uint8*)auth,
				original_body_length);
		}
#endif /* WL_ASSOC_MGR */

		/* authentication success or failure */
		wlc_auth_complete(cfg, status, &hdr->sa, auth_status, auth_alg);
	}
} /* wlc_authresp_client */

static void
wlc_assoc_process_retry_later(wlc_bsscfg_t *cfg, struct scb *scb, uint16 fk,
        uint8 *body, uint body_len)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_iem_upp_t upp;
	wlc_iem_ft_pparm_t ftpparm;
	wlc_iem_pparm_t pparm;
	wlc_assoc_t *as = cfg->assoc;
	uint32 timeout = 0;

	ASSERT(scb != NULL);

	body += sizeof(struct dot11_assoc_resp);
	body_len -= sizeof(struct dot11_assoc_resp);

	/* prepare IE mgmt calls */
	wlc_iem_parse_upp_init(wlc->iemi, &upp);
	bzero(&ftpparm, sizeof(ftpparm));
	ftpparm.assocresp.scb = scb;
	ftpparm.assocresp.status = DOT11_SC_ASSOC_TRY_LATER;
	bzero(&pparm, sizeof(pparm));
	pparm.ft = &ftpparm;

	/* parse IEs */
	wlc_iem_parse_frame(wlc->iemi, cfg, fk, &upp, &pparm,
		body, body_len);

#ifdef MFP
	timeout = wlc_mfp_get_assoc_comeback_time(wlc->mfp, scb);
#endif /* MFP */
	timeout = timeout ? timeout : WLC_ASSOC_RETRY_TIMEOUT;

	wlc_assoc_change_state(cfg, AS_RETRY_BSS);
	/* delete the assoc response timer */
	wl_del_timer(wlc->wl, as->timer);
	/* start the assoc retry wait timer */
	wl_add_timer(wlc->wl, as->timer, timeout, 0);
}

static void
wlc_clear_hw_association(wlc_bsscfg_t *cfg, bool mute_mode)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_rateset_t rs;

	wlc_set_pmoverride(cfg, FALSE);

	/* zero the BSSID so the core will not process TSF updates */
	wlc_bss_clear_bssid(cfg);

	/* Clear any possible Channel Switch Announcement */
	if (WL11H_ENAB(wlc))
		wlc_csa_reset_all(wlc->csa, cfg);

	cfg->assoc->rt = FALSE;

	/* reset quiet channels to the unassociated state */
	wlc_quiet_channels_reset(wlc->cmi);

	if (!DEVICEREMOVED(wlc)) {
		wlc_suspend_mac_and_wait(wlc);

		if (mute_mode == ON &&
		    wlc_quiet_chanspec(wlc->cmi, WLC_BAND_PI_RADIO_CHANSPEC))
			wlc_mute(wlc, ON, 0);

		/* clear BSSID in PSM */
		wlc_clear_bssid(cfg);

		/* write the beacon interval to the TSF block */
		W_REG(wlc->osh, D11_CFPRep(wlc), 0x80000000);

		/* gphy, aphy use the same CWMIN */
		wlc_bmac_set_cwmin(wlc->hw, APHY_CWMIN);
		wlc_bmac_set_cwmax(wlc->hw, PHY_CWMAX);

		if (BAND_2G(wlc->band->bandtype))
			wlc_bmac_set_shortslot(wlc->hw, wlc->shortslot);

		wlc_enable_mac(wlc);

		/* Reset the basic rate lookup table to phy mandatory rates */
		rs.count = 0;
		wlc_rate_lookup_init(wlc, &rs);
		wlc_set_ratetable(wlc);
	}
} /* wlc_clear_hw_association */

static void
wlc_create_ibss(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	/* zero out BSSID in order for _wlc_join_start_ibss() to assign a random one */
	bzero(&cfg->BSSID, ETHER_ADDR_LEN);

#ifdef WLRSDB
	if (RSDB_ENAB(wlc->pub)) {
		/* Will need clone/downgrade/upgrade. Deferring further processing */
		if (wlc_rsdb_ibss_bringup(wlc, &cfg) != BCME_OK) {
			return;
		}
	}
#endif /* WLRSDB */

	if (cfg->ibss_up_pending != TRUE) {
		(void)_wlc_join_start_ibss(wlc, cfg, cfg->target_bss->bss_type);
	}
	return;
}

/**
 * start an IBSS with all parameters in cfg->target_bss except SSID and BSSID.
 * SSID comes from cfg->SSID; BSSID comes from cfg->BSSID if it is not null,
 * generate a random BSSID otherwise.
 */
static int
_wlc_join_start_ibss(wlc_info_t *wlc, wlc_bsscfg_t *cfg, int bss_type)
{
	wlc_bss_info_t bi;
	wlcband_t *band;
	uint channel;
	wlc_assoc_t *as;
	int err;
	uint status;
	struct ether_addr *bssid;

	wlc_assoc_change_state(cfg, AS_IBSS_CREATE);
	wlc_bss_assoc_state_notif(wlc, cfg, AS_ASSOCIATION, AS_IBSS_CREATE);

	/* set up default BSS params */
	bcopy(cfg->target_bss, &bi, sizeof(bi));

	/* fixup network type */
	if (bss_type == DOT11_BSSTYPE_ANY) {
		WL_ASSOC(("wl%d: START: Setting bss_type to IBSS\n",
		          wlc->pub->unit));
		bss_type = DOT11_BSSTYPE_INDEPENDENT;
	}
	bi.bss_type = (int8)bss_type;

	bcopy(&cfg->BSSID, &bi.BSSID, ETHER_ADDR_LEN);
	if (!ETHER_ISNULLADDR(&wlc->default_bss->BSSID))
		bcopy(&wlc->default_bss->BSSID.octet, &bi.BSSID.octet, ETHER_ADDR_LEN);

	if (ETHER_ISNULLADDR(&bi.BSSID)) {
		/* create IBSS BSSID using a random number */
		wlc_getrand(wlc, &bi.BSSID.octet[0], ETHER_ADDR_LEN);

		/* Set the first 2 MAC addr bits to "locally administered" */
		ETHER_SET_LOCALADDR(&bi.BSSID.octet[0]);
		ETHER_SET_UNICAST(&bi.BSSID.octet[0]);
	}
	bssid = &bi.BSSID;

	/* adopt previously specified params */
	bzero(bi.SSID, sizeof(bi.SSID));
	bi.SSID_len = cfg->SSID_len;
	bcopy(cfg->SSID, bi.SSID, MIN(bi.SSID_len, sizeof(bi.SSID)));

	/* Check if 40MHz channel bandwidth is allowed in current locale and band and
	 * convert to 20MHz if not allowed
	 */
	band = wlc->bandstate[CHSPEC_IS2G(bi.chanspec) ? BAND_2G_INDEX : BAND_5G_INDEX];
	if (CHSPEC_IS40(bi.chanspec) &&
	    (!N_ENAB(wlc->pub) ||
	     (wlc_channel_locale_flags_in_band(wlc->cmi, band->bandunit) & WLC_NO_40MHZ) ||
	     !WL_BW_CAP_40MHZ(band->bw_cap))) {
		channel = wf_chspec_ctlchan(bi.chanspec);
		bi.chanspec = CH20MHZ_CHSPEC(channel);
	}

	/*
	   Validate or fixup default channel value.
	   Don't want to create ibss on quiet channel since it hasn't be
	   verified as radar-free.
	*/
	if (!wlc_valid_chanspec_db(wlc->cmi, bi.chanspec) ||
	    wlc_quiet_chanspec(wlc->cmi, bi.chanspec)) {
		chanspec_t chspec;
		/* Search for the non-quiet channel in both bands. */
		chspec = wlc_next_chanspec(wlc->cmi, bi.chanspec, CHAN_TYPE_CHATTY, TRUE);

		if (chspec == INVCHANSPEC) {
			status = WLC_E_STATUS_NO_NETWORKS;
			err = BCME_ERROR;
			goto exit;
		}
		bi.chanspec = chspec;
	}

#ifdef WL11N
	/* BSS rateset needs to be adjusted to account for channel bandwidth */
	wlc_rateset_bw_mcs_filter(&bi.rateset,
		WL_BW_CAP_40MHZ(wlc->band->bw_cap)?CHSPEC_WLC_BW(bi.chanspec):0);
#endif // endif

	if (WSEC_ENABLED(cfg->wsec) && (cfg->wsec_restrict)) {
		WL_WSEC(("%s(): set bi->capability DOT11_CAP_PRIVACY bit.\n", __FUNCTION__));
		bi.capability |= DOT11_CAP_PRIVACY;
	}

	if (bss_type == DOT11_BSSTYPE_INDEPENDENT)
		bi.capability |= DOT11_CAP_IBSS;

	/* fixup bsscfg type based on chosen target_bss */
	if ((err = wlc_assoc_fixup_bsscfg_type(wlc, cfg, &bi, TRUE)) != BCME_OK) {
		status = WLC_E_STATUS_FAIL;
		goto exit;
	}

	/* Set the ATIM window */
	bi.atim_window = 0;

	wlc_suspend_mac_and_wait(wlc);
	wlc_BSSinit(wlc, &bi, cfg, WLC_BSS_START);
	wlc_enable_mac(wlc);

	/* initialize link state tracking to the lost beacons condition */
	cfg->roam->time_since_bcn = 1;
	cfg->roam->bcns_lost = TRUE;

	wlc_sta_assoc_upd(cfg, TRUE);
	WL_RTDC(wlc, "wlc_join_start_ibss: associated", 0, 0);

	/* force a PHY cal on the current IBSS channel */
#ifdef PHYCAL_CACHING
	phy_chanmgr_create_ctx((phy_info_t *) WLC_PI(wlc), bi.chanspec);
#endif // endif
	wlc_full_phy_cal(wlc, cfg, PHY_PERICAL_START_IBSS);

#ifdef WLMCNX
	if (MCNX_ENAB(wlc->pub)) {
		cfg->current_bss->dtim_period = 1;
		wlc_mcnx_bss_upd(wlc->mcnx, cfg, TRUE);
	}
#endif /* WLMCNX */

	wlc_bsscfg_up(wlc, cfg);

	/* Apply the STA WME Parameters */
	if (BSS_WME_ENAB(wlc, cfg)) {
		/* To be consistent with IBSS Start and join set WLC_BSSCFG_WME_ASSOC */
		cfg->flags |= WLC_BSSCFG_WME_ASSOC;

		/* Starting IBSS so apply local EDCF parameters */
		wlc_edcf_acp_apply(wlc, cfg, TRUE);
	}

	as = cfg->assoc;
	ASSERT(as != NULL);

	/* notifying interested parties of the state... */
	wlc_bss_assoc_state_notif(wlc, cfg, as->type, as->state);

	WL_ASSOC(("wl%d: IBSS started\n", wlc->pub->unit));
	/* N.B.: bss_type passed through auth_type event field */
	wlc_bss_mac_event(wlc, cfg, WLC_E_START, bssid,
	            WLC_E_STATUS_SUCCESS, 0, bss_type,
	            NULL, 0);

	WL_APSTA_UPDN(("wl%d: Reporting link up on config %d for IBSS starting "
	               "a BSS\n", WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg)));
	wlc_link(wlc, TRUE, bssid, cfg, 0);

	wlc_if_event(wlc, WLC_E_IF_CHANGE, cfg->wlcif);

	status = WLC_E_STATUS_SUCCESS;
	err = BCME_OK;
	/* fall through */
exit:
	if (err != BCME_OK) {
		bssid = NULL;
	}
	wlc_join_done(cfg, bssid, bss_type, status);
	return err;
} /* _wlc_join_start_ibss */

/**
 * start an IBSS with all parameters in wlc->default_bss except:
 * - SSID
 * - BSSID
 * SSID comes from cfg->SSID and cfg->SSID_len.
 * BSSID comes from cfg->BSSID, a random BSSID is generated according to IBSS BSSID rule
 * if BSSID is null.
 */
/* Also bss_type is taken from the parameter list instead of the default_bss as well.
 * This routine is used by multiple users to Start a non-AP BSS so we should rename
 * the function to be wlc_join_start_bss().
 */
int
wlc_join_start_ibss(wlc_info_t *wlc, wlc_bsscfg_t *cfg, int8 bss_type)
{
	bool mpc_join = wlc->mpc_join;
	uint8 def_dtim = wlc->default_bss->dtim_period;
	int err;

	if (!wlc->pub->up) {
		WL_ASSOC_ERROR(("wl%d: %s: unable to start IBSS while driver is down\n",
		          wlc->pub->unit, __FUNCTION__));
		err = BCME_NOTUP;
		goto exit;
	}

	wlc->mpc_join = TRUE;
	wlc_radio_mpc_upd(wlc);

	/* reuse the default configuration to create an IBSS */
	wlc->default_bss->dtim_period = 1;

	(void)wlc_join_start_prep(wlc, cfg);

	err = _wlc_join_start_ibss(wlc, cfg, bss_type);

exit:
	/* restore default_bss before detecting error and bailing out */
	wlc->default_bss->dtim_period = def_dtim;

	wlc->mpc_join = mpc_join;
	wlc_radio_mpc_upd(wlc);

	return err;
} /* wlc_join_start_ibss */

static void
wlc_disassoc_done_quiet_chl(wlc_info_t *wlc, uint txstatus, void *arg)
{
	BCM_REFERENCE(txstatus);
	BCM_REFERENCE(arg);

	if (wlc->pub->up &&
	    wlc_quiet_chanspec(wlc->cmi, WLC_BAND_PI_RADIO_CHANSPEC)) {
		WL_ASSOC(("%s: muting the channel \n", __FUNCTION__));
		wlc_mute(wlc, ON, 0);
	}
}

#ifdef ROBUST_DISASSOC_TX
static void
wlc_disassoc_tx_complete(wlc_info_t *wlc, uint txstatus, void *arg)
{
	wlc_bsscfg_t *cfg = wlc_bsscfg_find_by_ID(wlc, (uint16)(uintptr)arg);
	uint interval = 0;

	/* in case bsscfg is freed before this callback is invoked */
	if (cfg == NULL) {
		WL_NONE(("wl%d: %s: unable to find bsscfg by ID %p\n",
		          wlc->pub->unit, __FUNCTION__, OSL_OBFUSCATE_BUF(arg)));
		return;
	}

	/* Make sure that this cfg is still associated */
	if (!cfg->associated) {
		WL_NONE(("wl%d: %s: bsscfg %d is down/disabled.\n",
		          wlc->pub->unit, __FUNCTION__, cfg->_idx));
		return;
	}
#ifdef WLP2P
	if (BSS_P2P_ENAB(wlc, cfg)) {
		/* If cfg is P2P/GC and operated with NoA/Ops,
		 * it should send disassoc with considering the absence of P2P/GO.
		 */
		if (wlc_p2p_get_noa_status(wlc, cfg)) {
			interval = 10;
		}
	}
#endif /* WLP2P */

	if (txstatus & TX_STATUS_ACK_RCV) {
		/* cleanup immediately on ACK_RCV */
		cfg->assoc->block_disassoc_tx = TRUE;
		wlc_disassoc_tx(cfg, FALSE);
	} else {
		/* no ack, arm timer to retry disassoc again */
		cfg->assoc->disassoc_txstatus = txstatus;
		cfg->assoc->type = AS_NONE;
		cfg->assoc->state = AS_DISASSOC_TIMEOUT;
		/* First delete the assoc timer before re-using it */
		wlc_assoc_timer_del(wlc, cfg);
		/* Add timer so that wlc_disassoc_tx happens in clean timer Call back context */
		wl_add_timer(wlc->wl, cfg->assoc->timer, interval, FALSE);
	}
	return;
}

static int
wlc_disassoc_tx_cb(wlc_info_t *wlc, wlc_bsscfg_t *cfg, void *pkt,
	void *arg, void *extra_arg)
{

	if (wlc_pcb_fn_register(wlc->pcb, wlc_disassoc_tx_complete,
		(void *)(uintptr)cfg->ID, pkt)) {
			WL_ERROR(("wl%d: %s out of pkt callbacks\n",
					wlc->pub->unit, __FUNCTION__));
			return BCME_ERROR;
		}
	return BCME_OK;
}

void
wlc_disassoc_tx(wlc_bsscfg_t *cfg, bool send_disassociate)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	struct scb *scb;
	struct ether_addr BSSID;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_ASSOC */
	wlc_bss_info_t *current_bss = cfg->current_bss;

	WL_TRACE(("wl%d: wlc_disasso_tx\n", wlc->pub->unit));

	if (DEVICEREMOVED(wlc) || !cfg->BSS)
		goto exit;

	/* abort any association state machine in process abort should be done
	 * only if the current cfg is undergoing assoc. Else this might go and
	 * clear some of the generic lists and states of the other bsscfg's
	 */
	if (wlc_assoc_get_as_cfg(wlc) == cfg) {
		wlc_assoc_abort(cfg);
	}

	if (!cfg->associated)
		goto exit;

	bcopy(&cfg->prev_BSSID, &BSSID, ETHER_ADDR_LEN);

#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	bcm_ether_ntoa(&BSSID, eabuf);
#endif // endif

	scb = wlc_scbfind(wlc, cfg, &BSSID);

	/* clear PM state, (value used for wake shouldn't matter) */
	wlc_update_bcn_info(cfg, FALSE);

	wlc_pspoll_timer_upd(cfg, FALSE);
	wlc_apsd_trigger_upd(cfg, FALSE);

#ifdef QUIET_DISASSOC
	if (send_disassociate) {
		send_disassociate = FALSE;
	}
#endif /* QUIET_DISASSOC */
#ifdef WLTDLS
	if (TDLS_ENAB(wlc->pub) && wlc_tdls_quiet_down(wlc->tdls)) {
		WL_ASSOC(("wl%d: JOIN: skipping DISASSOC to %s since we are "
				    "quite down.\n", WLCWLUNIT(wlc), eabuf));
		send_disassociate = FALSE;
	}
#endif // endif
	/* Send disassociate packet and (attempt to) schedule callback */
	if (send_disassociate) {
		if (ETHER_ISNULLADDR(cfg->BSSID.octet)) {
			/* a NULL BSSID indicates that we have given up on our AP connection
			 * to the point that we will reassociate to it if we ever see it
			 * again. In this case, we should not send a disassoc
			 */
			WL_ASSOC(("wl%d: JOIN: skipping DISASSOC to %s since we lost "
				    "contact.\n", WLCWLUNIT(wlc), eabuf));
		} else if (wlc_radar_chanspec(wlc->cmi, current_bss->chanspec) ||
		           wlc_restricted_chanspec(wlc->cmi, current_bss->chanspec)) {
			WL_ASSOC(("wl%d: JOIN: sending DISASSOC to %s on "
			          "radar/restricted channel \n",
			          WLCWLUNIT(wlc), eabuf));
		} else if (as->disassoc_tx_retry < 7) {
			WL_ASSOC(("wl%d: JOIN: sending DISASSOC to %s\n",
				WLCWLUNIT(wlc), eabuf));
			if (wlc_senddisassoc_ex(wlc, cfg, scb, &BSSID, &BSSID,
					&cfg->cur_etheraddr, DOT11_RC_DISASSOC_LEAVING,
					wlc_disassoc_tx_cb, NULL, NULL)) {

				WL_ASSOC(("wl%d: JOIN: error sending "
				          "DISASSOC\n", WLCWLUNIT(wlc)));
				goto exit;
			}
			as->disassoc_tx_retry++;
			return;
		}
	}

exit:
	as->disassoc_tx_retry = 0;
	wlc_bsscfg_disable(wlc, cfg);
	wlc_sta_assoc_upd(cfg, FALSE);
	cfg->assoc->block_disassoc_tx = FALSE;
	return;
} /* wlc_disassoc_tx */

/* This function calls wlc_disassoc_tx in this clean timer callback context.
 * Called from wlc_assoc_timeout.
 */
static void
wlc_disassoc_timeout(void *arg)
{
	wlc_bsscfg_t *cfg = (wlc_bsscfg_t *) arg;

	if (cfg && cfg->assoc) {
		if (cfg->assoc->disassoc_txstatus & TX_STATUS_ACK_RCV) {
			cfg->assoc->block_disassoc_tx = TRUE;
			wlc_disassoc_tx(cfg, FALSE);
		}
		else {
			wlc_disassoc_tx(cfg, TRUE);
		}
	}
}
#endif /* ROBUST_DISASSOC_TX */

static int
wlc_disassociate_client_cb_reschspec(wlc_info_t *wlc, wlc_bsscfg_t *cfg, void *pkt,
	void *arg, void *extra_arg)
{
	bool *mute_mode = (bool *)extra_arg;

	BCM_REFERENCE(arg);
	BCM_REFERENCE(cfg);

	if (wlc_pcb_fn_register(wlc->pcb, wlc_disassoc_done_quiet_chl, NULL, pkt)) {
		WL_ERROR(("wl%d: %s out of pkt callbacks\n", wlc->pub->unit, __FUNCTION__));
		return BCME_ERROR;
	} else {
		*mute_mode = OFF;
		return BCME_OK;
	}
}

static int
wlc_disassociate_client_cb_currchspec(wlc_info_t *wlc, wlc_bsscfg_t *cfg, void *pkt,
	void *arg, void *extra_arg)
{
	pkcb_fn_t *fn = (pkcb_fn_t *)extra_arg;
	if (*fn != NULL) {
		if (wlc_pcb_fn_register(wlc->pcb, *fn, arg, pkt)) {
			WL_ERROR(("wl%d: %s out of pkt callbacks\n",
			          wlc->pub->unit, __FUNCTION__));
			return BCME_ERROR;
		} else {
			WLPKTTAGBSSCFGSET(pkt, WLC_BSSCFG_IDX(cfg));
			/* the callback was registered, so clear fn local
			* so it will not be called at the end of this function
			*/
			*fn = NULL;
		}
	}
	return BCME_OK;
}

static int
_wlc_disassociate_client(wlc_bsscfg_t *cfg, bool send_disassociate, pkcb_fn_t fn, void *arg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	struct scb *scb;
	struct ether_addr BSSID;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_ASSOC */
	uint bsstype;
	wlc_bss_info_t *current_bss = cfg->current_bss;
	wlc_bss_info_t *target_bss = cfg->target_bss;
	bool mute_mode = ON;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char chanbuf[CHANSPEC_STR_LEN];
#endif // endif

	WL_TRACE(("wl%d: wlc_disassociate_client\n", wlc->pub->unit));

	if (wlc->pub->associated == FALSE)
		return (-1);
	if (!cfg->associated)
		return (-1);
	if (cfg->BSS) {
		bcopy(&cfg->prev_BSSID, &BSSID, ETHER_ADDR_LEN);
	} else {
		bcopy(&current_bss->BSSID, &BSSID, ETHER_ADDR_LEN);
	}
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	bcm_ether_ntoa(&BSSID, eabuf);
#endif /* BCMDBG || WLMSG_ASSOC */

#ifdef WLTDLS
	/* cleanup the TDLS peers which require an association */
	if (TDLS_ENAB(wlc->pub))
		wlc_tdls_cleanup(wlc->tdls, cfg, FALSE);
#endif /* WLTDLS */

#ifdef WLMCNX
	/* reset multi-connection assoc states */
	if (MCNX_ENAB(wlc->pub)) {
		if (cfg->BSS)
			wlc_mcnx_assoc_upd(wlc->mcnx, cfg, FALSE);
		else
			wlc_mcnx_bss_upd(wlc->mcnx, cfg, FALSE);
	}
#endif // endif

	if (DEVICEREMOVED(wlc)) {
		wlc_sta_assoc_upd(cfg, FALSE);
		wlc_clear_hw_association(cfg, mute_mode);
		wlc_disassoc_complete(cfg, WLC_E_STATUS_SUCCESS, &BSSID,
			DOT11_RC_DISASSOC_LEAVING, cfg->current_bss->bss_type);
		return (0);
	}

	/* BSS STA */
	if (cfg->BSS) {
		/* cache BSS type for disassoc indication */
		bsstype = DOT11_BSSTYPE_INFRASTRUCTURE;

		/* abort any association state machine in process */
		if ((as->state != AS_IDLE) && (as->state != AS_WAIT_DISASSOC))
			wlc_assoc_abort(cfg);

		scb = wlc_scbfind(wlc, cfg, &BSSID);

		/* clear PM state, (value used for wake shouldn't matter) */
		wlc_update_bcn_info(cfg, FALSE);

		wlc_pspoll_timer_upd(cfg, FALSE);
		wlc_apsd_trigger_upd(cfg, FALSE);

#ifdef QUIET_DISASSOC
		if (send_disassociate) {
			send_disassociate = FALSE;
		}
#endif /* QUIET_DISASSOC */
#ifdef WLTDLS
		if (TDLS_ENAB(wlc->pub) && wlc_tdls_quiet_down(wlc->tdls)) {
			WL_ASSOC(("wl%d: JOIN: skipping DISASSOC to %s since we are "
					    "quite down.\n", WLCWLUNIT(wlc), eabuf));
			send_disassociate = FALSE;
		}
#endif // endif
		/* Send disassociate packet and (attempt to) schedule callback */
		if (send_disassociate) {
			if (ETHER_ISNULLADDR(cfg->BSSID.octet)) {
				/* a NULL BSSID indicates that we have given up on our AP connection
				 * to the point that we will reassociate to it if we ever see it
				 * again. In this case, we should not send a disassoc
				 */
				WL_ASSOC(("wl%d: JOIN: skipping DISASSOC to %s since we lost "
					    "contact.\n", WLCWLUNIT(wlc), eabuf));
			} else if ((wlc_radar_chanspec(wlc->cmi, current_bss->chanspec) &&
				wlc_quiet_chanspec(wlc->cmi, current_bss->chanspec)) ||
			        wlc_restricted_chanspec(wlc->cmi, current_bss->chanspec)) {
				/* note that if the channel is a radar or restricted channel,
				 * Permit sending disassoc packet if no subsequent processing
				 * is waiting  (indicated by the presence of callbcak routine)
				 */
				if (fn != NULL) {

					WL_ASSOC(("wl%d: JOIN: skipping DISASSOC to %s"
						"since chanspec %s is quiet and call back\n",
						WLCWLUNIT(wlc), eabuf,
						wf_chspec_ntoa_ex(current_bss->chanspec, chanbuf)));

				} else if (!wlc_quiet_chanspec(wlc->cmi, current_bss->chanspec) &&
					(current_bss->chanspec == WLC_BAND_PI_RADIO_CHANSPEC)) {

					WL_ASSOC(("wl%d: JOIN: sending DISASSOC to %s on "
					          "radar/restricted channel \n",
					          WLCWLUNIT(wlc), eabuf));

					if (wlc_senddisassoc_ex(wlc, cfg, scb, &BSSID, &BSSID,
						&cfg->cur_etheraddr, DOT11_RC_DISASSOC_LEAVING,
						wlc_disassociate_client_cb_reschspec, NULL,
						&mute_mode)) {

						WL_ASSOC(("wl%d: JOIN: error sending "
						          "DISASSOC\n", WLCWLUNIT(wlc)));
					}
				} else {
					WL_ASSOC(("wl%d: JOIN: Skipping DISASSOC to %s since "
						"present channel not home channel \n",
						WLCWLUNIT(wlc), eabuf));
				}
			} else if (current_bss->chanspec == WLC_BAND_PI_RADIO_CHANSPEC) {
				WL_ASSOC(("wl%d: JOIN: sending DISASSOC to %s\n",
				    WLCWLUNIT(wlc), eabuf));

				if (wlc_senddisassoc_ex(wlc, cfg, scb, &BSSID, &BSSID,
						&cfg->cur_etheraddr, DOT11_RC_DISASSOC_LEAVING,
						wlc_disassociate_client_cb_currchspec, arg, &fn)) {
					WL_ASSOC(("wl%d: JOIN:error sending"
					          "DISASSOC\n", WLCWLUNIT(wlc)));
				}
			}
		}

		/* reset scb state */
		if (scb) {

			wlc_scb_clearstatebit(wlc, scb, ASSOCIATED | PENDING_AUTH | PENDING_ASSOC);

			wlc_scb_disassoc_cleanup(wlc, scb);
		}

#if NCONF
		/* XXX stop the temporary WAR to improve Tx throughput in a non-N mode association
		 * to share the medium with other B-only STA: disable EDCRS when disassoc
		 */
		if (WLCISNPHY(wlc->band)) {
			wlc_bmac_ifsctl_edcrs_set(wlc->hw, TRUE);
		}
#endif /* NCONF */
	}
	/* IBSS STA */
	else {
		/* cache BSS type for disassoc indication */
		bsstype = cfg->current_bss->bss_type;

		wlc_ibss_disable(cfg);
	}

	/* update association states */
	wlc_sta_assoc_upd(cfg, FALSE);

	if (!wlc->pub->associated) {
		/* if auto shortslot, switch back to 11b compatible long slot */
		if (wlc->shortslot_override == WLC_SHORTSLOT_AUTO)
			wlc->shortslot = FALSE;
	}

	/* init protection configuration */
	wlc_prot_g_cfg_init(wlc->prot_g, cfg);
#ifdef WL11N
	if (N_ENAB(wlc->pub))
		wlc_prot_n_cfg_init(wlc->prot_n, cfg);
#endif // endif

	if (!ETHER_ISNULLADDR(cfg->BSSID.octet) &&
		TRUE) {
		if (!(BSSCFG_IS_RSDB_CLONE(cfg))) {
			wlc_disassoc_complete(cfg, WLC_E_STATUS_SUCCESS, &BSSID,
				DOT11_RC_DISASSOC_LEAVING, bsstype);
		}
	}

	if (!AP_ACTIVE(wlc) && cfg == wlc->cfg) {
		WL_APSTA_UPDN(("wl%d: wlc_disassociate_client: wlc_clear_hw_association\n",
			wlc->pub->unit));
		wlc_clear_hw_association(cfg, mute_mode);
	} else {
		WL_APSTA_BSSID(("wl%d: wlc_disassociate_client -> wlc_clear_bssid\n",
			wlc->pub->unit));
		wlc_clear_bssid(cfg);
		wlc_bss_clear_bssid(cfg);
	}

	if (current_bss->bcn_prb) {
		MFREE(wlc->osh, current_bss->bcn_prb, current_bss->bcn_prb_len);
		current_bss->bcn_prb = NULL;
		current_bss->bcn_prb_len = 0;
	}

	WL_APSTA_UPDN(("wl%d: Reporting link down on config 0 (STA disassociating)\n",
	               WLCWLUNIT(wlc)));

	if (WLCISNPHY(wlc->band)) {
		wlc_phy_acimode_noisemode_reset(WLC_PI(wlc),
			current_bss->chanspec, TRUE, FALSE, TRUE);
	}

	if (!(BSSCFG_IS_RSDB_CLONE(cfg)) &&
		TRUE) {
		wlc_link(wlc, FALSE, &BSSID, cfg, WLC_E_LINK_DISASSOC);
	}

	/* reset rssi moving average */
	if (cfg->BSS) {
		wlc_lq_rssi_snr_noise_bss_sta_ma_reset(wlc, cfg,
			CHSPEC_BANDUNIT(cfg->current_bss->chanspec),
			WLC_RSSI_INVALID, WLC_SNR_INVALID, WLC_NOISE_INVALID);
		wlc_lq_rssi_bss_sta_event_upd(wlc, cfg);
	}

#ifdef WLLED
	wlc_led_event(wlc->ledh);
#endif // endif

	/* notifying interested parties of the event... */
	wlc_bss_disassoc_notif(wlc, cfg, DAN_TYPE_LOCAL_DISASSOC, DAN_ST_DISASSOC_CMPLT,
	                       &cfg->BSSID);

	/* disable radio due to end of association */
	WL_MPC(("wl%d: disassociation wlc->pub->associated==FALSE, update mpc\n", wlc->pub->unit));
	wlc_radio_mpc_upd(wlc);

	/* call the given callback fn if it has not been taken care of with
	 * a disassoc packet callback.
	 */
	if (fn)
		(*fn)(wlc, TX_STATUS_NO_ACK, arg);

	/* clean up... */
	bzero(target_bss->BSSID.octet, ETHER_ADDR_LEN);
	bzero(cfg->BSSID.octet, ETHER_ADDR_LEN);

	return (0);
} /* _wlc_disassociate_client */

static int
wlc_setssid_disassociate_client(wlc_bsscfg_t *cfg)
{
	return _wlc_disassociate_client(cfg, TRUE,
	        wlc_setssid_disassoc_complete, (void *)(uintptr)cfg->ID);
}

int
wlc_disassociate_client(wlc_bsscfg_t *cfg, bool send_disassociate)
{
	return _wlc_disassociate_client(cfg, send_disassociate, NULL, NULL);
}

static void
wlc_assoc_success(wlc_bsscfg_t *cfg, struct scb *scb)
{
	wlc_info_t *wlc = cfg->wlc;
	struct scb *prev_scb;

	ASSERT(scb != NULL);

	wlc_scb_clearstatebit(wlc, scb, PENDING_AUTH | PENDING_ASSOC);

	wlc_assoc_timer_del(wlc, cfg);

	/* clean up before leaving the BSS */
	if (cfg->BSS && cfg->associated) {
		prev_scb = wlc_scbfindband(wlc, cfg, &cfg->prev_BSSID,
			CHSPEC_WLCBANDUNIT(cfg->current_bss->chanspec));
		if (prev_scb) {
			uint8 bit_flag = (ASSOCIATED | AUTHORIZED);
			if (prev_scb != scb) {
				bit_flag |= AUTHENTICATED;
			}
			wlc_scb_clearstatebit(wlc, prev_scb, bit_flag);

			/* XXX APSTA: Should we clear SCB_MYAP here?  Still
			 * authenticated...
			 */

			/* delete old AP's pairwise key */
			wlc_scb_disassoc_cleanup(wlc, prev_scb);
		}
	}

#ifdef MFP
	/* MFP flags settings after reassoc when prev_scb and scb are same */
	/* we have a valid combination of MFP flags */
	scb->flags2 &= ~(SCB2_MFP | SCB2_SHA256);
	if ((BSSCFG_IS_MFP_CAPABLE(cfg)) &&
		(cfg->target_bss->wpa2.flags & RSN_FLAGS_MFPC))
		SCB_MFP_ENABLE(scb);
	if ((cfg->WPA_auth & (WPA2_AUTH_1X_SHA256 | WPA2_AUTH_PSK_SHA256)) &&
		(cfg->target_bss->wpa2.flags & RSN_FLAGS_SHA256))
		scb->flags2 |= SCB2_SHA256;
	WL_ASSOC(("wl%d: %s: turn MFP on %s\n", wlc->pub->unit, __FUNCTION__,
		((scb->flags2 & SCB2_SHA256) ? "with sha256" : "")));
#endif /* MFP */
	/* clear PM state */
	wlc_set_pmoverride(cfg, FALSE);
	wlc_update_bcn_info(cfg, FALSE);

	/* update scb state */
	wlc_scb_setstatebit(wlc, scb, ASSOCIATED);

	/* init per scb WPA_auth */
	scb->WPA_auth = cfg->WPA_auth;
	WL_WSEC(("wl%d: WPA_auth 0x%x\n", wlc->pub->unit, scb->WPA_auth));

	/* adopt the BSS parameters */
	wlc_join_adopt_bss(cfg);
	wlc_scb_ratesel_init(wlc, scb);

	/* 11g hybrid coex cause jerky mouse, disable for now. do not apply for ECI chip for now */
	if (!SCB_HT_CAP(scb) && !BCMECICOEX_ENAB(wlc))
		wlc_mhf(wlc, MHF3, MHF3_BTCX_SIM_RSP, 0, WLC_BAND_2G);

#if NCONF
	/* temporary WAR to improve Tx throughput in a non-N mode association */
	/* to share the medium with other B-only STA: Enable EDCRS when assoc */
	if (WLCISNPHY(wlc->band)) {
		wlc_bmac_ifsctl_edcrs_set(wlc->hw, SCB_HT_CAP(scb));
	}
#endif /* NCONF */

#ifdef WLWNM
	/* Check DMS req for primary infra STA */
	if (WLWNM_ENAB(wlc->pub)) {
		if ((!WSEC_ENABLED(scb->wsec) || WSEC_WEP_ENABLED(scb->wsec)) &&
			(cfg == wlc->cfg)) {
			wlc_wnm_check_dms_req(wlc, &scb->ea);
		}
		wlc_wnm_scb_assoc(wlc, scb);
	}
#endif /* WLWNM */
} /* wlc_assoc_success */

static void
wlc_assoc_continue_post_auth1(wlc_bsscfg_t *cfg, struct scb *scb)
{
	wlc_info_t *wlc = cfg->wlc;
	void *pkt = NULL;
	wlc_bss_info_t *target_bss = cfg->target_bss;
	wlc_assoc_t *as = cfg->assoc;

	wlc_assoc_change_state(cfg, AS_SENT_ASSOC);
	pkt = wlc_join_assoc_start(wlc, cfg, scb, target_bss, cfg->associated);

#if defined(WLP2P) && defined(BCMDBG)
	if (WL_P2P_ON()) {
		int bss = wlc_mcnx_BSS_idx(wlc->mcnx, cfg);
		uint16 state = wlc_mcnx_read_shm(wlc->mcnx, M_P2P_BSS_ST(wlc, bss));
		uint16 next_noa = wlc_mcnx_read_shm(wlc->mcnx, M_P2P_BSS_N_NOA(wlc, bss));
		uint16 hps = wlc_mcnx_read_shm(wlc->mcnx, M_P2P_HPS_OFFSET(wlc));

		WL_P2P(("wl%d: %s: queue ASSOC at tick 0x%x ST 0x%04X "
		        "N_NOA 0x%X HPS 0x%04X\n",
		        wlc->pub->unit, __FUNCTION__,
		        R_REG(wlc->osh, D11_TSFTimerLow(wlc)),
		        state, next_noa, hps));
	}
#endif /* WLP2P && BCMDBG */

	wlc_assoc_timer_del(wlc, cfg);

	if (pkt != NULL) {
		if (wlc_pcb_fn_register(wlc->pcb, wlc_assocreq_complete,
		                    (void *)(uintptr)cfg->ID, pkt)) {
			WL_ERROR(("wl%d: %s out of pkt callbacks\n",
					wlc->pub->unit, __FUNCTION__));
			wlc_assoc_abort(cfg);
			return;
		}
	}
	/* Fall back to timer method */
	wl_add_timer(wlc->wl, as->timer, WECA_ASSOC_TIMEOUT +10, 0);
}

static void
wlc_auth_complete(wlc_bsscfg_t *cfg, uint status, struct ether_addr* addr,
	uint auth_status, uint auth_type)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	wlc_bss_info_t *target_bss = cfg->target_bss;
	bool more_to_do_after_event = FALSE;
	struct scb *scb;
#if defined(BCMDBG) || defined(BCMDBG_ERR) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN];
	if (addr != NULL)
		bcm_ether_ntoa(addr, eabuf);
	else
		strncpy(eabuf, "<NULL>", sizeof(eabuf) - 1);
#endif /* BCMDBG || BCMDBG_ERR || WLMSG_ASSOC */

	/* If we receive an event not for us, preserve the timer and keep waiting */
	if (status != WLC_E_STATUS_UNSOLICITED) {
		wlc_assoc_timer_del(wlc, cfg);
	}
	/* SAE case upper layer auth requst timeout may happen */
	if ((as->state != AS_SENT_AUTH_UP) && (as->state != AS_START_EXT_AUTH)) {
		/* If we are not in state of waiting for authresp, it is unsolicited */
		if ((as->state != AS_SENT_AUTH_1) && (as->state != AS_SENT_AUTH_3)) {
			status = WLC_E_STATUS_UNSOLICITED;
		}
	}

	/* If unsolicited, do nothing */
	if (status == WLC_E_STATUS_UNSOLICITED)
		goto do_event;

	/* Clear pending bits */
	scb = addr ? wlc_scbfind(wlc, cfg, addr): NULL;
	if (scb)
		wlc_scb_clearstatebit(wlc, scb, PENDING_AUTH | PENDING_ASSOC);

	if (status == WLC_E_STATUS_SUCCESS) {
		WL_ASSOC(("wl%d: JOIN: authentication success\n",
		          WLCWLUNIT(wlc)));
		ASSERT(scb != NULL);
		wlc_scb_setstatebit(wlc, scb, AUTHENTICATED | PENDING_ASSOC);
		WL_APSTA(("wl%d: WLC_E_AUTH for %s, forcing MYAP flag.\n",
		          wlc->pub->unit, eabuf));

#if defined(WL_ASSOC_MGR)
		if (WL_ASSOC_MGR_ENAB(wlc->pub)) {
			/* see if we should pause here; if so, indicate auth event and exit */
			if (wlc_assoc_mgr_pause_on_auth_resp(wlc->assoc_mgr,
					cfg, addr, auth_status, auth_type)) {
				/* nothing more to do - paused for now */
				return;
			}
		}
#endif /* defined(WL_ASSOC_MGR) */

#ifdef WL_FILS
		if ((FILS_ENAB(wlc->pub)) &&
			auth_type == DOT11_FILS_SKEY) {
			wlc_assoc_change_state(cfg, AS_IDLE);
		}
		else
#endif /* WL_FILS */
		{
			if (BSSCFG_EXT_AUTH(cfg))
				return;
			wlc_assoc_continue_post_auth1(cfg, scb);
		}
		goto do_event;
	} else if (status == WLC_E_STATUS_TIMEOUT) {
		wlc_key_info_t key_info;
		BCM_REFERENCE(key_info);
#ifdef PSTA
		wlc_keymgmt_get_bss_tx_key(wlc->keymgmt,
			(BSSCFG_PSTA(cfg) ? wlc_bsscfg_primary(wlc) : cfg), FALSE, &key_info);
#else /* PSTA */
		(void)wlc_keymgmt_get_bss_tx_key(wlc->keymgmt, cfg, FALSE, &key_info);
#endif /* PSTA */
		WL_ASSOC(("wl%d: JOIN: timeout waiting for authentication "
			"response, assoc_state %d\n",
			WLCWLUNIT(wlc), as->state));
#ifdef WL_AUTH_SHARED_OPEN
		/* XXX WAR: Some APs do not support DOT11_SHARED_KEY.
		 * So there is 802.11 ACK but it does not send response and timeout occurs.
		 * Try to send auth frame with OPEN System.
		 */
		if ((AUTH_SHARED_OPEN_ENAB(wlc->pub)) && cfg->openshared &&
			cfg->auth_atmptd == DOT11_SHARED_KEY &&
			(target_bss->capability & DOT11_CAP_PRIVACY) &&
			WSEC_WEP_ENABLED(cfg->wsec) &&
			WLC_KEY_IS_DEFAULT_BSS(&key_info) &&
			(key_info.algo == CRYPTO_ALGO_WEP1 ||
			key_info.algo == CRYPTO_ALGO_WEP128)) {
			wlc_bss_info_t* bi = wlc_assoc_get_join_target(wlc, 0);
			ASSERT(bi != NULL);
			/* Try the current target BSS with DOT11_OPEN_SYSTEM */
			cfg->auth_atmptd = DOT11_OPEN_SYSTEM;
			(void)wlc_assoc_sendauth(wlc, cfg, &scb->ea, &bi->BSSID, scb,
				cfg->auth_atmptd, 1, DOT11_SC_SUCCESS,
				NULL, ((bi->capability & DOT11_CAP_SHORT) != 0),
				AS_SENT_AUTH_1);
			return;
		}
#endif /* WL_AUTH_SHARED_OPEN */
	} else if (status == WLC_E_STATUS_NO_ACK) {
		WL_ASSOC(("wl%d.%d: JOIN: authentication failure, no ack from %s\n",
		          WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg), eabuf));
	} else if (status == WLC_E_STATUS_FAIL) {
		wlc_key_info_t key_info;
		memset(&key_info, 0, sizeof(wlc_key_info_t));
		BCM_REFERENCE(key_info);
#ifdef PSTA
		wlc_keymgmt_get_bss_tx_key(wlc->keymgmt,
			(BSSCFG_PSTA(cfg) ? wlc_bsscfg_primary(wlc) : cfg), FALSE, &key_info);
#else /* PSTA */
		(void)wlc_keymgmt_get_bss_tx_key(wlc->keymgmt, cfg, FALSE, &key_info);
#endif /* PSTA */
		WL_ASSOC(("wl%d: JOIN: authentication failure status %d from %s\n",
		          WLCWLUNIT(wlc), (int)auth_status, eabuf));
		ASSERT(scb != NULL);
		wlc_scb_clearstatebit(wlc, scb, AUTHENTICATED);

		if (BSSCFG_EXT_AUTH(cfg) &&
			(auth_status == DOT11_SC_ANTICLOG_TOCKEN_REQUIRED))
			return;

		if (cfg->openshared &&
#ifdef WL_AUTH_SHARED_OPEN
			((AUTH_SHARED_OPEN_ENAB(wlc->pub) &&
			cfg->auth_atmptd == DOT11_SHARED_KEY) ||
			(!AUTH_SHARED_OPEN_ENAB(wlc->pub) &&
			cfg->auth_atmptd == DOT11_OPEN_SYSTEM)) &&
#else
			cfg->auth_atmptd == DOT11_OPEN_SYSTEM &&
#endif /* WL_AUTH_SHARED_OPEN */
			auth_status == DOT11_SC_AUTH_MISMATCH &&
			(target_bss->capability & DOT11_CAP_PRIVACY) &&
			WSEC_WEP_ENABLED(cfg->wsec) &&
			(key_info.algo == CRYPTO_ALGO_WEP1 ||
			key_info.algo == CRYPTO_ALGO_WEP128)) {
			wlc_bss_info_t* bi = wlc_assoc_get_join_target(wlc, 0);
			ASSERT(bi != NULL);
#ifdef WL_AUTH_SHARED_OPEN
			if (AUTH_SHARED_OPEN_ENAB(wlc->pub)) {
				/* Try the current target BSS with DOT11_OPEN_SYSTEM */
				cfg->auth_atmptd = DOT11_OPEN_SYSTEM;
			} else
#endif /* WL_AUTH_SHARED_OPEN */
			{
				/* Try the current target BSS with DOT11_SHARED_KEY */
				cfg->auth_atmptd = DOT11_SHARED_KEY;
			}
			(void)wlc_assoc_sendauth(wlc, cfg, &scb->ea, &bi->BSSID, scb,
				cfg->auth_atmptd, 1, DOT11_SC_SUCCESS,
				NULL, ((bi->capability & DOT11_CAP_SHORT) != 0),
				AS_SENT_AUTH_1);
			return;
		}
	} else if (status == WLC_E_STATUS_ABORT) {
		WL_ASSOC(("wl%d: JOIN: authentication aborted\n", WLCWLUNIT(wlc)));
		goto do_event;
	} else {
		WL_ASSOC_ERROR(("wl%d: %s, unexpected status %d\n",
		          WLCWLUNIT(wlc), __FUNCTION__, (int)status));
		goto do_event;
	}
	more_to_do_after_event = TRUE;

do_event:
	wlc_bss_mac_event(wlc, cfg, WLC_E_AUTH, addr, status, auth_status, auth_type, 0, 0);

	if (!more_to_do_after_event) {
#ifdef WL_FILS
		if ((FILS_ENAB(wlc->pub)) &&
			auth_type == DOT11_FILS_SKEY) {
			wlc_fils_free_ies(wlc, cfg);
		}
#endif /* WL_FILS */
		return;
	}

	/* This is when status != WLC_E_STATUS_SUCCESS... */

	/* Try current BSS again */
	if ((status == WLC_E_STATUS_NO_ACK || status == WLC_E_STATUS_TIMEOUT) &&
	    (as->bss_retries < as->retry_max)) {
		WL_ASSOC(("wl%d: Retrying authentication (%d)...\n",
		          WLCWLUNIT(wlc), as->bss_retries));
		wlc_join_bss(cfg);
	}
	else { /* Try next BSS */
		wlc_assoc_next_join_target(wlc);
		wlc_join_attempt(cfg);
	}
} /* wlc_auth_complete */

#if defined(WL_ASSOC_MGR)
int
wlc_assoc_continue(wlc_bsscfg_t *cfg, struct ether_addr* addr)
{
	int bcmerr = BCME_ERROR;
	uint table_index = 0;
	wlc_assoc_t *assoc = cfg->assoc;

	if (assoc->state != AS_VALUE_NOT_SET) {
		for (; wlc_assoc_continue_table[table_index].state != AS_VALUE_NOT_SET;
				table_index++) {

			if (wlc_assoc_continue_table[table_index].state == assoc->state) {

				bcmerr = wlc_assoc_continue_table[table_index].handler(cfg, addr);
				break;
			}
		}
	}
	return bcmerr;
}

static int
wlc_assoc_continue_sent_auth1(wlc_bsscfg_t *cfg, struct ether_addr* addr)
{
	int bcmerr = BCME_OK;
	wlc_info_t *wlc = cfg->wlc;
	wlc_bss_info_t *target_bss = cfg->target_bss;
	struct scb *scb;

	do {
		/* valid scb for accessing? */
		scb = addr ? wlc_scbfind(wlc, cfg, addr): NULL;

		if (scb == NULL || target_bss == NULL) {
			bcmerr = BCME_NOTREADY;
			WL_ASSOC_ERROR(("%s: couldn't continue assoc scb=%p target_bss=%p\n",
				__FUNCTION__, OSL_OBFUSCATE_BUF(scb),
				OSL_OBFUSCATE_BUF(target_bss)));
			break;
		}

		wlc_assoc_continue_post_auth1(cfg, scb);

	} while (0);

	return bcmerr;
}
#endif /* defined(WL_ASSOC_MGR) */

static void
wlc_assoc_complete(wlc_bsscfg_t *cfg, uint status, struct ether_addr* addr,
	uint assoc_status, bool reassoc, uint bss_type)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	bool more_to_do_after_event = FALSE;
	struct scb *scb;
#if defined(BCMDBG_ERR) || defined(WLMSG_ASSOC)
	const char* action = (reassoc)?"reassociation":"association";
#endif // endif
#if defined(BCMDBG) || defined(BCMDBG_ERR) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN];
	if (addr != NULL)
		bcm_ether_ntoa(addr, eabuf);
	else
		strncpy(eabuf, "<NULL>", sizeof(eabuf) - 1);
#endif /* BCMDBG || BCMDBG_ERR || WLMSG_ASSOC */

	if (status == WLC_E_STATUS_UNSOLICITED)
		goto do_event;

	if (status == WLC_E_STATUS_SUCCESS) {
		WL_ASSOC(("wl%d: JOIN: %s success ...\n", WLCWLUNIT(wlc), action));
		if (!reassoc) {
			wlc_bsscfg_up(wlc, cfg);
			if (addr) {
				bcopy(addr, &as->last_upd_bssid, ETHER_ADDR_LEN);
			}
		} else if (WLRCC_ENAB(wlc->pub)) {
			/* to avoid too long scan block (10 seconds),
			 * reduce the scan block.
			 */
			cfg->roam->scan_block = SCAN_BLOCK_AT_ASSOC_COMPL;
		}
#ifdef SLAVE_RADAR
		if ((!wlc_is_ap_interface_up(wlc)) && WL11H_STA_ENAB(wlc) &&
			wlc_dfs_get_radar(wlc->dfs) &&
			(cfg->roam->reason == WLC_E_REASON_RADAR_DETECTED)) {
			if (!wlc_radar_chanspec(wlc->cmi, WLC_BAND_PI_RADIO_CHANSPEC)) {
				cfg->pm->pm_modechangedisabled = FALSE;
				wlc_set_pm_mode(wlc, cfg->pm->pm_oldvalue, cfg);
				wlc->mpc = TRUE;
				wlc_radio_mpc_upd(wlc);
			}
		}
#endif  /* SLAVE_RADAR */
		if (WOWL_ENAB(wlc->pub) && cfg == wlc->cfg)
			cfg->roam->roam_on_wowl = FALSE;
		/* Restart the ap's in case of a band change */
		if (AP_ACTIVE(wlc)) {
			/* performing a channel change,
			 * all up ap scb's need to be cleaned up
			 */
			wlc_bsscfg_t *apcfg;
			int idx;
			bool mchan_stago_disab =
#ifdef WLMCHAN
				!MCHAN_ENAB(wlc->pub) ||
				wlc_mchan_stago_is_disabled(wlc->mchan);
#else
				TRUE;
#endif // endif

#ifdef AP
#ifdef WLMCHAN
			if (!MCHAN_ENAB(wlc->pub))
#endif // endif
				FOREACH_UP_AP(wlc, idx, apcfg) {
					/* Clean up scbs only when there is a chanspec change */
					if (WLC_BAND_PI_RADIO_CHANSPEC !=
						apcfg->current_bss->chanspec)
							wlc_ap_bsscfg_scb_cleanup(wlc, apcfg);
				}
#endif /* AP */

			FOREACH_UP_AP(wlc, idx, apcfg) {
				if (!(MCHAN_ENAB(wlc->pub)) || mchan_stago_disab) {
					wlc_txflowcontrol_override(wlc, apcfg->wlcif->qi, OFF,
						TXQ_STOP_FOR_PKT_DRAIN);
					wlc_scb_update_band_for_cfg(wlc, apcfg,
						WLC_BAND_PI_RADIO_CHANSPEC);
				}
			}
			wlc_restart_ap(wlc->ap);
		}
#if defined(BCMSUP_PSK) && defined(WLFBT)
		if (BSSCFG_IS_FBT(cfg) && SUP_ENAB(wlc->pub) && (cfg->WPA_auth & WPA2_AUTH_FT) &&
			reassoc)
			wlc_sup_clear_replay(wlc->idsup, cfg);
#endif /* BCMSUP_PSK && WLFBT */
		goto do_event;
	}

	if (!(as->state == AS_SENT_ASSOC || as->state == AS_REASSOC_RETRY))
		goto do_event;

	/* Clear pending bits */
	scb = addr ? wlc_scbfind(wlc, cfg, addr) : NULL;
	if (scb)
		wlc_scb_clearstatebit(wlc, scb, PENDING_AUTH | PENDING_ASSOC);

	if (status != WLC_E_STATUS_TIMEOUT) {
		wlc_assoc_timer_del(wlc, cfg);
	}
	if (status == WLC_E_STATUS_TIMEOUT) {
		WL_ASSOC(("wl%d: JOIN: timeout waiting for %s response\n",
		    WLCWLUNIT(wlc), action));
	} else if (status == WLC_E_STATUS_NO_ACK) {
		WL_ASSOC(("wl%d: JOIN: association failure, no ack from %s\n",
		    WLCWLUNIT(wlc), eabuf));
	} else if (status == WLC_E_STATUS_FAIL) {
		WL_ASSOC(("wl%d: JOIN: %s failure %d\n",
		    WLCWLUNIT(wlc), action, (int)assoc_status));
	} else if (status == WLC_E_STATUS_ABORT) {
		WL_ASSOC(("wl%d: JOIN: %s aborted\n", wlc->pub->unit, action));
		goto do_event;
	} else {
		WL_ASSOC_ERROR(("wl%d: %s: %s, unexpected status %d\n",
		    WLCWLUNIT(wlc), __FUNCTION__, action, (int)status));
		goto do_event;
	}
	more_to_do_after_event = TRUE;

do_event:
	wlc_bss_mac_event(wlc, cfg, reassoc ? WLC_E_REASSOC : WLC_E_ASSOC, addr,
		status, assoc_status, bss_type, as->resp, as->resp_len);

	if (!more_to_do_after_event)
		return;

	/* This is when status != WLC_E_STATUS_SUCCESS... */

	/* Try current BSS again */
	if ((status == WLC_E_STATUS_NO_ACK || status == WLC_E_STATUS_TIMEOUT) &&
	    (as->bss_retries < as->retry_max)) {
		WL_ASSOC(("wl%d: Retrying association (%d)...\n",
		          WLCWLUNIT(wlc), as->bss_retries));
		wlc_join_bss(cfg);
	}
	/* Try next BSS */
	else {
		wlc_assoc_next_join_target(wlc);
		wlc_join_attempt(cfg);
	}
} /* wlc_assoc_complete */

static int
wlc_set_ssid_complete(wlc_bsscfg_t *cfg, uint status, struct ether_addr *addr, uint bss_type)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	bool retry = FALSE;
	bool assoc_state = FALSE;

	/* flag to indicate connection completion when abort */
	if (status == WLC_E_STATUS_ABORT)
		assoc_state = ((bss_type == DOT11_BSSTYPE_INDEPENDENT) ?
		               TRUE :
		               (cfg->assoc->state > AS_IDLE &&
		                cfg->assoc->state < AS_WAIT_RCV_BCN));

	/* when going from scan to idle, due to failure  */
	if (as->state == AS_SCAN && status == WLC_E_STATUS_FAIL) {
		/* if here due to failure and we're still in scan state, then abort our assoc */
		if (SCAN_IN_PROGRESS(wlc->scan)) {
			WL_ASSOC(("wl%d:%s: abort association in process\n",
				WLCWLUNIT(wlc), __FUNCTION__));
			/* assoc was in progress when this condition occurred */
			/* if it's not, assert to catch the unexpected condition */
			ASSERT(AS_IN_PROGRESS(wlc));
			wlc_assoc_abort(cfg);
		}
	}

	/* Association state machine is halting, clear state and allow core to sleep */
	wlc_assoc_change_state(cfg, AS_IDLE);
	as->type = AS_NONE;

	if ((status != WLC_E_STATUS_SUCCESS) && (status != WLC_E_STATUS_FAIL) &&
	    (status != WLC_E_STATUS_NO_NETWORKS) && (status != WLC_E_STATUS_ABORT))
		WL_ASSOC_ERROR(("wl%d: %s: unexpected status %d\n",
		          WLCWLUNIT(wlc), __FUNCTION__, (int)status));

	if (status != WLC_E_STATUS_SUCCESS) {
		wl_assoc_params_t *assoc_params;
		assoc_params = wlc_bsscfg_assoc_params(cfg);

		WL_ASSOC(("wl%d: %s: failed status %u\n",
		          WLCWLUNIT(wlc), __FUNCTION__, status));

		/*
		 * If we are here because of a status abort don't check mpc,
		 * it is responsibility of the caller
		*/
		if (status != WLC_E_STATUS_ABORT)
			wlc_radio_mpc_upd(wlc);

		if ((status == WLC_E_STATUS_NO_NETWORKS ||
			(status == WLC_E_STATUS_FAIL)) &&
			cfg->enable) {
			/* retry if configured */

			/* XXX These two kinds of retries should not be enabled
			 * at the same time. Join (ESS) retry is to overcome
			 * the possible interference in an BTC environment, vs.
			 * assoc timer based (STA) retry is to connect the STA in APSTA
			 * mode to an AP if the AP is available later after the STA is started.
			 * In case they are indeed all configured let the Join
			 * retry takes place first.
			 */
			/* XXX Retry Join only when we haven't tried 802.11
			 * association procedures...
			 */
			if (as->ess_retries < as->retry_max) {
				WL_ASSOC(("wl%d: Retrying join (%d)...\n",
				          WLCWLUNIT(wlc), as->ess_retries));
				wlc_join_start(cfg, wlc_bsscfg_scan_params(cfg),
				               wlc_bsscfg_assoc_params(cfg));
				retry = TRUE;
			} else if ((assoc_params != NULL) && assoc_params->bssid_cnt) {
				int chidx = assoc_params->chanspec_num;
				int bccnt = assoc_params->bssid_cnt;

				WL_ASSOC(("wl%d: join failed, index %d of %d (%s)\n",
					WLCWLUNIT(wlc), chidx, bccnt,
					((chidx + 1) < bccnt) ? "continue" : "fail"));

				if (++chidx < bccnt) {
					assoc_params->chanspec_num = chidx;
					as->ess_retries = 0;
					wlc_join_start(cfg, wlc_bsscfg_scan_params(cfg),
					wlc_bsscfg_assoc_params(cfg));
					retry = TRUE;
				}
			} else if ((wlc->as->cmn->sta_retry_time > 0) &&
				(cfg->auth != WL_AUTH_SAE_KEY)) {
				/* Sae Auth Start is handled by external supplicant
				 * So auth retry have to be initiated by upper layer
				 */
				wl_del_timer(wlc->wl, as->timer);
				wl_add_timer(wlc->wl, as->timer,
					wlc->as->cmn->sta_retry_time*1000, 0);
				as->rt = TRUE;
				retry = TRUE;
			}
		} else if (status != WLC_E_STATUS_ABORT &&
			cfg->enable && (assoc_params != NULL) && assoc_params->bssid_cnt) {
			int chidx = assoc_params->chanspec_num;
			int bccnt = assoc_params->bssid_cnt;

			WL_ASSOC(("wl%d: join failed, index %d of %d (%s)\n",
				WLCWLUNIT(wlc), chidx, bccnt,
				((chidx + 1) < bccnt) ? "continue" : "fail"));

			if (++chidx < bccnt) {
				assoc_params->chanspec_num = chidx;
				as->ess_retries = 0;
				wlc_join_start(cfg, wlc_bsscfg_scan_params(cfg),
					wlc_bsscfg_assoc_params(cfg));
				retry = TRUE;
			}
		}
	}

	/* no more processing if we are going to retry */
	if (retry) {
		return JOIN_CMPLT_RETRY;
	}

	/* free join scan/assoc params */
	if (status == WLC_E_STATUS_SUCCESS) {
		wlc_bsscfg_scan_params_reset(wlc, cfg);
		wlc_bsscfg_assoc_params_reset(wlc, cfg);
	}

	/* N.B.: assoc_state check passed through status event field */
	/* N.B.: bss_type passed through auth_type event field */
	wlc_bss_mac_event(wlc, cfg, WLC_E_SET_SSID, addr, status, assoc_state, bss_type,
	                  cfg->SSID, cfg->SSID_len);
	return JOIN_CMPLT_DONE;
} /* wlc_set_ssid_complete */

static int
wlc_roam_complete(wlc_bsscfg_t *cfg, uint status, struct ether_addr *addr, uint bss_type)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	wlc_roam_t *roam = cfg->roam;
	bool assoc_recreate = FALSE;

#ifdef WLFBT
	uint8 ft_key[WPA_MIC_KEY_LEN + WPA_ENCR_KEY_LEN];
#endif // endif
	int ret;

	if (status != WLC_E_STATUS_SUCCESS) {
		roam->cached_ap[0].rssi_st.roam_st.status = (uint8)status;
	}

	if (status == WLC_E_STATUS_SUCCESS) {
		WL_ASSOC(("wl%d: JOIN: roam success\n", WLCWLUNIT(wlc)));
		wlc_bsscfg_reprate_init(cfg); /* clear txrate history on successful roam */
	} else if (status == WLC_E_STATUS_FAIL) {
		WL_ASSOC(("wl%d: JOIN: roam failure\n", WLCWLUNIT(wlc)));
#ifdef AP
		if (wlc_join_check_ap_need_csa(wlc, cfg, cfg->current_bss->chanspec,
		                               AS_WAIT_FOR_AP_CSA_ROAM_FAIL)) {
			WL_ASSOC(("wl%d.%d: ROAM FAIL: "
			          "%s delayed due to ap active, wait for ap CSA\n",
			          WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg), __FUNCTION__));
			return ROAM_CMPLT_DELAY;
		}
#endif /* AP */
	} else if (status == WLC_E_STATUS_NO_NETWORKS) {
		WL_ASSOC(("wl%d: JOIN: roam found no networks, disable apcfg "
			"if radio in repeater mode \n", WLCWLUNIT(wlc)));
		wlc_assoc_disable_apcfg_on_repeater(wlc, cfg);
	} else if (status == WLC_E_STATUS_ABORT) {
		WL_ASSOC(("wl%d: JOIN: roam aborted\n", WLCWLUNIT(wlc)));
	} else {
		WL_ASSOC_ERROR(("wl%d: %s: unexpected status %d\n",
		    WLCWLUNIT(wlc), __FUNCTION__, (int)status));
	}
	roam->roam_scan_started = FALSE;
#ifdef WLWNM
	if (WLWNM_ENAB(wlc->pub)) {
		/* If bsstrans_resp is pending here then we don't have a roam candidate.
		 * Send a bsstrans_reject.
		*/
		wlc_wnm_bsstrans_roamscan_complete(wlc, cfg,
			DOT11_BSSTRANS_RESP_STATUS_REJ_BSS_LIST_PROVIDED, NULL);
	}
#endif /* WLWNM */

	if (ASSOC_RECREATE_ENAB(wlc->pub))
		assoc_recreate = (as->type == AS_RECREATE);

	/* If a roam fails, restore state to that of our current association */
	if (status == WLC_E_STATUS_FAIL) {
		wlc_bss_info_t *current_bss = cfg->current_bss;

		/* restore old basic rates */
		wlc_rate_lookup_init(wlc, &current_bss->rateset);
	}

	ret = ROAM_CMPLT_DONE;

	/* N.B.: roam reason passed through status event field */
	/* N.B.: bss_type passed through auth_type event field */
	if (status == WLC_E_STATUS_SUCCESS) {
#ifdef WLFBT
		/* Send up ft_key in case of FT AUTH, and the supplicant will take the information
		 * only when it is valid at the FBT roaming.
		 */
		if (BSSCFG_IS_FBT(cfg) && (cfg->WPA_auth & WPA2_AUTH_FT)) {
			wlc_fbt_get_kck_kek(wlc->fbt, cfg, ft_key);
			wlc_bss_mac_event(wlc, cfg, WLC_E_BSSID, addr, status,
				roam->reason, bss_type, ft_key, sizeof(ft_key));
		} else
#endif /* WLFBT */
		wlc_bss_mac_event(wlc, cfg, WLC_E_BSSID, addr, 0, status, bss_type, 0, 0);
		bcopy(addr, &as->last_upd_bssid, ETHER_ADDR_LEN);
	}

	wlc_bss_mac_event(wlc, cfg, WLC_E_ROAM, addr, status, roam->reason, bss_type, 0, 0);
	/* XXX REVISIT WHERE THIS IS SUPPOSE TO GO: BEFORE OR AFTER WLC_E_ROAM EVENT.
	 * I believe I have left this the same as it was before removing processing
	 * from wlc_event_handle().
	 *
	 * Note there are two events, one in wlc_link and the explicit WLC_E_DISASOC for
	 * EXT_STA.  I think these events use to happen after the WLC_E_ROAM event because
	 * you were in the event handler when they got called and thus got queued and
	 * executed in the next iteration of the the do/while loop in wlc_process_event().
	 */

	/* if this was the roam scan for an association recreation, then
	 * declare link down immediately instead of letting bcn_timeout
	 * happen later.
	 */
	if (ASSOC_RECREATE_ENAB(wlc->pub) && assoc_recreate) {
		if (status != WLC_E_STATUS_SUCCESS) {
			WL_ASSOC(("wl%d: ROAM: RECREATE failed, link down\n", WLCWLUNIT(wlc)));
			wlc_link(wlc, FALSE, &cfg->prev_BSSID, cfg, WLC_E_LINK_ASSOC_REC);
			roam->bcns_lost = TRUE;
			roam->time_since_bcn = 1;
		}

#ifdef  NLO
		wlc_bss_mac_event(wlc, cfg, WLC_E_ASSOC_RECREATED, NULL, status, 0, 0, 0, 0);
#endif /* NLO */
	}

	/* Handle AP lost scenario when roam complete happens */
	if ((roam->time_since_bcn > roam->bcn_timeout) && !roam->bcns_lost &&
		(roam->reason == WLC_E_REASON_BCNS_LOST)) {
		wlc_handle_ap_lost(wlc, cfg);
	}

	return ret;
} /* wlc_roam_complete */

void
wlc_disassoc_complete(wlc_bsscfg_t *cfg, uint status, struct ether_addr *addr,
	uint disassoc_reason, uint bss_type)
{
	wlc_info_t *wlc = cfg->wlc;
#ifdef SLAVE_RADAR
	if ((!wlc_is_ap_interface_up(wlc)) && WL11H_STA_ENAB(wlc) && wlc_dfs_get_radar(wlc->dfs)) {
		cfg->pm->pm_modechangedisabled = FALSE;
		wlc_set_pm_mode(wlc, cfg->pm->pm_oldvalue, cfg);
	}
#endif /* SLAVE_RADAR */
#ifdef WLRM
	if (WLRM_ENAB(wlc->pub) && wlc_rminprog(wlc)) {
		WL_INFORM(("wl%d: abort RM due to disassoc\n", WLCWLUNIT(wlc)));
		wlc_rm_stop(wlc);
	}
#endif /* WLRM */

#ifdef WL11K
	if (wlc_rrm_inprog(wlc)) {
		WL_INFORM(("wl%d: abort RRM due to disassoc\n", WLCWLUNIT(wlc)));
		wlc_rrm_stop(wlc);
	}
#endif /* WL11K */

	wlc_bss_mac_event(wlc, cfg, WLC_E_DISASSOC, addr, status,
	                  disassoc_reason, bss_type, 0, 0);
} /* wlc_disassoc_complete */

int
wlc_roamscan_start(wlc_bsscfg_t *cfg, uint roam_reason)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	wlc_roam_t *roam = cfg->roam;
	bool roamscan_full = FALSE, roamscan_new;
	bool roamscan_stop;
	int err = BCME_OK;
#ifdef WLABT
	if (WLABT_ENAB(wlc->pub) && roam_reason == WLC_E_REASON_LOW_RSSI) {
		wlc_check_adaptive_bcn_timeout(cfg);
	}
#endif /* WLABT */
	if (roam_reason == WLC_E_REASON_DEAUTH || roam_reason == WLC_E_REASON_DISASSOC) {
		wlc_update_bcn_info(cfg, FALSE);

		/* Don't block this scan */
		roam->scan_block = 0;

		/* bzero(cfg->BSSID.octet, ETHER_ADDR_LEN); */
		wlc_bss_clear_bssid(cfg);
	}

	/* Turning off partials scans will restore original 'dumb' roaming algorithms */
	if (roam->partialscan_period) {
		uint idx, num_invalid_aps = 0;
		/* make sure that we don't have a valid cache full of APs we have just tried to
		 * associate with - this prevents thrashing
		 * If there's only one AP in the cache, we should consider it a valid target even
		 * if we are "blocked" from it
		 */
		if (roam->cache_numentries > 1 &&
		    (roam_reason == WLC_E_REASON_MINTXRATE || roam_reason == WLC_E_REASON_TXFAIL)) {
			for (idx = 0; idx < roam->cache_numentries; idx++)
				if (roam->cached_ap[idx].time_left_to_next_assoc > 0)
					num_invalid_aps++;

			if (roam->cache_numentries > 0 &&
			    num_invalid_aps == roam->cache_numentries) {
				WL_ASSOC(("wl%d: %s: Unable start roamscan because there are no "
				          "valid entries in the roam cache\n", wlc->pub->unit,
				          __FUNCTION__));
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
				wlc_print_roam_status(cfg, roam_reason, TRUE);
#endif // endif
				return BCME_ERROR;
			}
		}

		/* Check the cache for the RSSI and make sure that we didn't have a significant
		 * change. If we did, invalidate the cache and run a full scan, because we're
		 * walking somewhere.
		 */
		if (roam_reason == WLC_E_REASON_LOW_RSSI) {
			int rssi = cfg->link->rssi;

			/* Update roam profile at run-time */
			if (wlc->band->roam_prof)
				wlc_roam_prof_update(wlc, cfg, FALSE);

			if ((roam->prev_rssi - rssi > roam->ci_delta) ||
			    (roam->cache_valid && (rssi - roam->prev_rssi > roam->ci_delta))) {
				/* Ignore RSSI change if roaming is already running
				 * or previous roaming is waiting for beacon from roamed AP
				 */
				if (as->type == AS_ROAM && (SCAN_IN_PROGRESS(wlc->scan) ||
						(as->state == AS_WAIT_RCV_BCN))) {
					return BCME_BUSY;
				}

				WL_ASSOC(("wl%d: %s: Detecting a significant RSSI change, "
				          "forcing full scans, p %d, c %d\n",
				          wlc->pub->unit, __FUNCTION__, roam->prev_rssi, rssi));
				WL_SRSCAN(("wl%d: Detecting a significant RSSI change, "
				          "forcing full scans, p %d, c %d\n",
				          wlc->pub->unit, roam->prev_rssi, rssi));
				roam->cache_valid = FALSE;
				roam->scan_block = 0;
				roam->prev_rssi = rssi;
			}
		}

		/* FALSE = rssi(-70) >= roam_trigger(-60) */
		roamscan_stop = (cfg->link->rssi >= wlc->band->roam_trigger);
#ifdef WLWNM
		/* confirm roamscan is really required */
		if (WBTEXT_ACTIVE(wlc->pub) && !roamscan_stop) {
			int reason = BCME_OK;

			/* FALSE = rssi(-70) < roam_trigger(-60) &&
			 *         (CU not triggered || WBTEXT not enabled)
			 */
			if (!wlc_wnm_bsstrans_roam_required(wlc, cfg, &reason)) {
				/* CU is not triggered */
				if (reason != BCME_UNSUPPORTED) {
					roamscan_stop = TRUE;
				}
				/* WBTEXT not enabled. i.e.roam due to low rssi */
				else if (!roam->scan_block) {
					WBTEXT_INFO(("WBTEXT DBG: roam scan started due to "
						"reason %d\n", WLC_E_REASON_LOW_RSSI));
				}
			}
			/* TRUE = rssi(-70) < roam_trigger(-60) && cu(80%) > cu_trigger(70%) */
			/* Already roaming? */
			else if (!roam->scan_block) {
				WBTEXT_INFO(("WBTEXT DBG: roam scan started due to reason %d\n",
					WLC_E_REASON_LOW_RSSI_CU));
			}
		}
#endif /* WLWNM */
		/* We are above the threshold and should stop roaming now */
		if (roam_reason == WLC_E_REASON_LOW_RSSI && roamscan_stop) {
			/* Stop roam scans */
			if (roam->active && roam->reason == WLC_E_REASON_LOW_RSSI) {
				/* Ignore RSSI thrashing about the wnm_roam_trigger */
				if (WBTEXT_ACTIVE(wlc->pub) &&
					wlc_wnm_bsstrans_check_for_roamthrash(wlc, cfg)) {
					return BCME_ERROR;
				}
				else
				/* Ignore RSSI thrashing about the roam_trigger */
				if ((cfg->link->rssi - wlc->band->roam_trigger) <
				    wlc->roam_rssi_cancel_hysteresis) {
					return BCME_ERROR;
				}

				WL_ASSOC(("wl%d: %s: Finished with roaming\n",
				          wlc->pub->unit, __FUNCTION__));

				if (as->type == AS_ROAM && SCAN_IN_PROGRESS(wlc->scan)) {
					uint scan_block_save = roam->scan_block;

					WL_ASSOC(("wl%d: %s: Aborting the roam scan in progress "
					          "since we received a strong signal RSSI: %d\n",
					          wlc->pub->unit, __FUNCTION__, cfg->link->rssi));
					WL_ROAM(("ROAM: Aborting the roam scan by RSSI %d\n",
						cfg->link->rssi));

					wlc_assoc_abort(cfg);
					roam->scan_block = scan_block_save;

					/* clear reason */
					roam->reason = WLC_E_REASON_INITIAL_ASSOC;
				}

				/* Just stop scan
				 * Keep roam cache and scan_block to avoid thrashing
				 */
				roam->active = FALSE;

				/* Reset any roam profile */
				if (wlc->band->roam_prof)
					wlc_roam_prof_update(wlc, cfg, TRUE);
			}
			return BCME_ERROR;
		}

		if (roam->reason == WLC_E_REASON_MINTXRATE ||
		    roam->reason == WLC_E_REASON_TXFAIL) {
			if (roam->txpass_cnt > roam->txpass_thresh) {
				WL_ASSOC(("wl%d: %s: Canceling the roam action because we have "
				          "sent %d packets at a good rate\n",
				          wlc->pub->unit, __FUNCTION__, roam->txpass_cnt));

				if (as->type == AS_ROAM && SCAN_IN_PROGRESS(wlc->scan)) {
					WL_ASSOC(("wl%d: %s: Aborting the roam scan in progress\n",
					          wlc->pub->unit, __FUNCTION__));

					wlc_assoc_abort(cfg);
				}
				roam->cache_valid = FALSE;
				roam->active = FALSE;
				roam->scan_block = 0;
				roam->txpass_cnt = 0;
				roam->reason = WLC_E_REASON_INITIAL_ASSOC; /* clear reason */

				return BCME_ERROR;
			}
		}

#ifdef OPPORTUNISTIC_ROAM
		/* This is an explicit request from the HOST, so ignore scan_block */
		if (roam->reason == WLC_E_REASON_BETTER_AP) {
			roam->scan_block = 0;
		}
#endif /* OPPORTUNISTIC_ROAM */

		/* Already roaming, come back in another watchdog tick  */
		if (roam->scan_block) {
			return BCME_EPERM;
		}

		/* Should initiate the roam scan now */
		if (!WLRCC_ENAB(wlc->pub))
			roamscan_full = !roam->cache_valid;
		roamscan_new = !roam->active && !roam->cache_valid;

		if (roam->time_since_upd >= roam->fullscan_period) {
#ifdef WLRCC
			if (WLRCC_ENAB(wlc->pub)) {
				if (roam->rcc_mode != RCC_MODE_FORCE) {
					roamscan_full = TRUE;
					roam->roam_fullscan = TRUE;
				}
			} else
#endif /* WLRCC */
				roamscan_full = TRUE;

			/* wlc_roam_scan() uses this info to decide on a full scan or a partial
			 * scan.
			 */
			if (roamscan_full)
				roam->cache_valid = FALSE;
		}

		/*
		 * If a roam scan is currently in progress, do not start a new one just yet.
		 *
		 * The current roam scan/re-association may however be waiting for a beacon,
		 * which we do not get because the AP changed channel or the signal faded away.
		 * In that case, start the new roam scan nevertheless.
		 */
		if (as->type == AS_ROAM && as->state != AS_WAIT_RCV_BCN) {
			WL_ASSOC(("wl%d: %s: Not starting roam scan for reason %u because roaming "
				"already in progress for reason %u, as->state %d\n",
				wlc->pub->unit, __FUNCTION__, roam_reason, roam->reason,
				as->state));
#ifdef OPPORTUNISTIC_ROAM
			memcpy(cfg->join_bssid.octet, BSSID_INVALID, sizeof(cfg->join_bssid.octet));
#endif /* OPPORTUNISTIC_ROAM */
			return BCME_BUSY;
		}

		WL_ASSOC(("wl%d: %s: Start roam scan: Doing a %s scan with a scan period of %d "
		          "seconds for reason %u\n", wlc->pub->unit, __FUNCTION__,
		          roamscan_new ? "new" :
		          !(roam->cache_valid && roam->cache_numentries > 0) ? "full" : "partial",
		          roamscan_new || !(roam->cache_valid && roam->cache_numentries > 0) ?
		          roam->fullscan_period : roam->partialscan_period,
		          roam_reason));

		/* Kick off the roam scan */
		if (!(err = wlc_roam_scan(cfg, roam_reason, NULL, 0))) {
			roam->active = TRUE;
			roam->scan_block = roam->partialscan_period;

			if (roamscan_full || roamscan_new) {
				if (
#ifdef WLRCC
					(WLRCC_ENAB(wlc->pub) &&
					 (roam->rcc_mode != RCC_MODE_FORCE)) &&
#endif // endif
					roamscan_new && roam->multi_ap) {
					/* Do RSSI_ROAMSCAN_FULL_NTIMES number of scans before */
					/* kicking in partial scans */
					roam->fullscan_count = roam->nfullscans;
				} else {
					roam->fullscan_count = 1;
				}
				roam->time_since_upd = 0;
			}
		}
		else {
			if (err == BCME_EPERM) {
				WL_ASSOC(("wl%d: %s: Couldn't start the roam with error %d\n",
				          wlc->pub->unit, __FUNCTION__, err));
			}
			else {
				WL_ASSOC_ERROR(("wl%d: %s: Couldn't start the roam with error %d\n",
				          wlc->pub->unit,  __FUNCTION__, err));
			}
		}
		return err;
	}
	/* Original roaming */
	else {
		/* roam if metric is worse than "good enough" threshold, which is radio dependent
		 * XXX - Should we really do this every frame?  How do we keep from constantly
		 *       scanning if we're almost out of range of the only AP ?
		 */
		int roam_metric;
		if (roam_reason == WLC_E_REASON_LOW_RSSI)
			roam_metric = cfg->link->rssi;
		else
			roam_metric = WLC_RSSI_MINVAL;

		if (roam_metric < wlc->band->roam_trigger) {
			if (roam->scan_block || roam->off) {
				err = BCME_EPERM;
				WL_ASSOC(("ROAM: roam_metric=%d; block roam scan request(%u,%d)\n",
				          roam_metric, roam->scan_block, roam->off));
			} else {
				WL_ASSOC(("ROAM: RSSI = %d; request roam scan\n",
				          roam_metric));

				roam->scan_block = roam->fullscan_period;
				err = wlc_roam_scan(cfg, WLC_E_REASON_LOW_RSSI, NULL, 0);
			}
		}
	}
	return err;
} /* wlc_roamscan_start */

static void
wlc_roamscan_complete(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_roam_t *roam = cfg->roam;

	WL_TRACE(("wl%d: %s: enter\n", wlc->pub->unit, __FUNCTION__));

	/* Not going to cache channels for any other roam reason but these */
	if (!((roam->reason == WLC_E_REASON_LOW_RSSI) ||
	      (roam->reason == WLC_E_REASON_BCNS_LOST) ||
	      (roam->reason == WLC_E_REASON_MINTXRATE) ||
	      (roam->reason == WLC_E_REASON_TXFAIL)))
		return;

	if (roam->roam_type == ROAM_PARTIAL) {
		/* Update roam period with back off */
		wlc_roam_period_update(wlc, cfg);
		return;
	}

	if (roam->fullscan_count == 1) {
		WL_ASSOC(("wl%d.%d: %s: Building roam candidate cache from driver roam scans\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
		wlc_build_roam_cache(cfg, wlc->as->cmn->join_targets);
	}

	if ((roam->fullscan_count > 0) &&
		((roam->roam_type == ROAM_FULL) ||
		(roam->split_roam_phase == 0))) {
			roam->fullscan_count--;
	}
}

static uint32
wlc_assoc_get_unicast(rsn_parms_t *rsn)
{
	int i;
	uint32 value = 0;

	for (i = 0; i < rsn->ucount; i++) {
		value |= (1<<rsn->unicast[i]);
	}
	return value;
}

static bool
wlc_assoc_check_rsn(rsn_parms_t *current_rsn, rsn_parms_t *candidate_rsn)
{
	uint32 current_ucast = wlc_assoc_get_unicast(current_rsn);
	uint32 candidate_ucast = wlc_assoc_get_unicast(candidate_rsn);

	/* check multicast */
	if (current_rsn->multicast != candidate_rsn->multicast) {
		return FALSE;
	}

	/* check unicast */
	if (current_ucast && candidate_ucast && current_ucast !=  candidate_ucast) {
		return FALSE;
	}

	return TRUE;
}

bool
wlc_assoc_check_roam_candidate(wlc_bsscfg_t *cfg, wlc_bss_info_t *candidate_bi)
{
	wlc_bss_info_t *current_bi = cfg->current_bss;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char eabuf1[ETHER_ADDR_STR_LEN], eabuf2[ETHER_ADDR_STR_LEN];
#endif // endif

	WL_ASSOC(("current:%s (auth=0x%x), candidate:%s (auth=0x%x)\n",
		bcm_ether_ntoa(&current_bi->BSSID, eabuf1), cfg->WPA_auth,
		bcm_ether_ntoa(&candidate_bi->BSSID, eabuf2), candidate_bi->WPA_auth_support));

	if (cfg->WPA_auth == WPA_AUTH_DISABLED) {
		return (candidate_bi->WPA_auth_support == WPA_AUTH_DISABLED);
	}

	/* fisrt, check the candidate supports current auth type */
	if ((cfg->WPA_auth & candidate_bi->WPA_auth_support) != cfg->WPA_auth) {
		WL_ASSOC(("AKM doesn't support\n"));
		return FALSE;
	}

	/* check multicast and unicast cipher */
	if (bcmwpa_is_wpa_auth(cfg->WPA_auth)) {
		if (!wlc_assoc_check_rsn(&current_bi->wpa, &candidate_bi->wpa)) {
			WL_ASSOC(("WPA RSN doesn't match\n"));
			return FALSE;
		}
	}
	else if (bcmwpa_is_rsn_auth(cfg->WPA_auth)) {
		if (!wlc_assoc_check_rsn(&current_bi->wpa2, &candidate_bi->wpa2)) {
			WL_ASSOC(("WPA2 RSN doesn't match\n"));
			return FALSE;
		}
	}

	return TRUE;
}

void
wlc_build_roam_cache(wlc_bsscfg_t *cfg, wlc_bss_list_t *candidates)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_roam_t *roam = cfg->roam;
	int32 i;
	uint32 nentries, j;
	chanspec_t chanspec;
	struct ether_addr *bssid;

	(void)wlc;

	/* For legacy roam scan or phase 1 split roam scan, rebuild the cache.
	 * For phase 2 split roam scan, accumulate results.
	 */
	if ((roam->roam_type == ROAM_FULL) || (roam->split_roam_phase == 1)) {
		roam->cache_numentries = 0;

		/* If associated, add the current AP to the cache. */
		if (cfg->associated && !ETHER_ISNULLADDR(&cfg->BSSID)) {
			bssid = &cfg->current_bss->BSSID;
			chanspec = CH20MHZ_CHSPEC(
					wf_chspec_ctlchan(cfg->current_bss->chanspec));
			WL_SRSCAN(("cache add: idx %d: bssid %02x:%02x",
				roam->cache_numentries, bssid->octet[4], bssid->octet[5]));
			WL_ASSOC(("cache add: idx %d: bssid %02x:%02x\n",
				roam->cache_numentries, bssid->octet[4], bssid->octet[5]));
			bcopy(bssid, &roam->cached_ap[0].BSSID, ETHER_ADDR_LEN);
			roam->cached_ap[0].chanspec = chanspec;
			roam->cached_ap[0].time_left_to_next_assoc = 0;
			roam->cached_ap[0].fullscan_cnt++;
			roam->cached_ap[0].rssi_st.roam_st.reason = (uint8)roam->reason;
			roam->cached_ap[0].time = OSL_SYSUPTIME();
			roam->cache_numentries = 1;
		}
	}

	nentries = roam->cache_numentries;

	/* fill in the cache entries, avoid duplicates */
	for (i = candidates->count - 1; (i >= 0) &&
		(nentries < ROAM_CACHELIST_SIZE); i--) {

		bssid = &candidates->ptrs[i]->BSSID;
		chanspec = CH20MHZ_CHSPEC(
				wf_chspec_ctlchan(candidates->ptrs[i]->chanspec));

		for (j = 0; j < nentries; j++) {
			if (chanspec == roam->cached_ap[j].chanspec &&
			    !bcmp(bssid, &roam->cached_ap[j].BSSID, ETHER_ADDR_LEN)) {
				if (j != 0) {
					roam->cached_ap[j].rssi_st.rssi =
						candidates->ptrs[i]->RSSI;
					roam->cached_ap[j].bss_load =
						candidates->ptrs[i]->qbss_load_aac;
					roam->cached_ap[j].time = OSL_SYSUPTIME();
				}
				break;
			}
		}
		if (j == nentries) {
			WL_SRSCAN(("cache add: idx %d: bssid %02x:%02x",
				nentries, bssid->octet[4], bssid->octet[5]));
			WL_ASSOC(("cache add: idx %d: bssid %02x:%02x\n",
				nentries, bssid->octet[4], bssid->octet[5]));
			bcopy(bssid, &roam->cached_ap[nentries].BSSID, ETHER_ADDR_LEN);
			roam->cached_ap[nentries].chanspec = chanspec;
			roam->cached_ap[nentries].time_left_to_next_assoc = 0;
			roam->cached_ap[nentries].rssi_st.rssi = candidates->ptrs[i]->RSSI;
			roam->cached_ap[nentries].bss_load = candidates->ptrs[i]->qbss_load_aac;
			roam->cached_ap[nentries].time = OSL_SYSUPTIME();
			nentries++;
		}
	}

	roam->cache_numentries = nentries;

	/* Mark the cache valid. */
	if ((roam->cache_numentries) &&
		((roam->roam_type == ROAM_FULL) ||
		(roam->split_roam_phase == 0))) {
		bool cache_valid = TRUE;

		WL_SRSCAN(("wl%d: Full roam scans completed, starting partial scans with %d "
		          "entries and rssi %d", wlc->pub->unit, nentries, cfg->link->rssi));
		WL_ASSOC(("wl%d: Full roam scans completed, starting partial scans with %d "
		          "entries and rssi %d\n", wlc->pub->unit, nentries, cfg->link->rssi));
#ifdef WLRCC
		if (WLRCC_ENAB(wlc->pub)) {
			cache_valid = roam->rcc_mode != RCC_MODE_FORCE;
		}
#endif /* WLRCC */
		if (cache_valid) {
			roam->prev_rssi = cfg->link->rssi;
			roam->cache_valid = TRUE;
		}
		wlc_roam_set_env(cfg, nentries);
	}

	/* print status */
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	wlc_print_roam_status(cfg, roam->reason, TRUE);
#endif // endif
} /* wlc_build_roam_cache */

/** Set the AP environment */
static void
wlc_roam_set_env(wlc_bsscfg_t *cfg, uint nentries)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_roam_t *roam = cfg->roam;
	wlcband_t *this_band = wlc->band;
	wlcband_t *other_band = wlc->bandstate[OTHERBANDUNIT(wlc)];
#ifdef WNM_BSSTRANS_EXT
	int idx = roam->roam_prof_idx;
	wl_roam_prof_t *roam_prof = &wlc->band->roam_prof[idx];
#endif /* WNM_BSSTRANS_EXT */

	if (roam->ap_environment != AP_ENV_DETECT_NOT_USED) {
		if (nentries > 1 && roam->ap_environment != AP_ENV_DENSE) {
			roam->ap_environment = AP_ENV_DENSE;
			WL_ASSOC(("wl%d: %s: Auto-detecting dense AP environment with "
			          "%d targets\n", wlc->pub->unit, __FUNCTION__, nentries));
				/* if the roam trigger isn't set to the default, or to the value
				 * of the sparse environment setting, don't change it -- we don't
				 * want to overrride manually set triggers
				 */
			/* this means we are transitioning from a sparse environment */
			if (this_band->roam_trigger == WLC_AUTO_ROAM_TRIGGER -
			    WLC_ROAM_TRIGGER_STEP) {
				this_band->roam_trigger = WLC_AUTO_ROAM_TRIGGER;
				WL_ASSOC(("wl%d: %s: Setting roam trigger on bandunit %u "
				          "to %d\n", wlc->pub->unit, __FUNCTION__,
				          this_band->bandunit, WLC_AUTO_ROAM_TRIGGER));
#ifdef WLABT
				if (WLABT_ENAB(wlc->pub) && (NBANDS(wlc) > 1) &&
					(roam->prev_bcn_timeout != 0)) {
					roam->bcn_timeout = roam->prev_bcn_timeout;
					roam->prev_bcn_timeout = 0;
					WL_ASSOC(("wl%d: Restore bcn_timeout to %d\n",
						wlc->pub->unit, roam->bcn_timeout));
				}
#endif /* WLABT */
			} else if (this_band->roam_trigger != WLC_AUTO_ROAM_TRIGGER)
				WL_ASSOC(("wl%d: %s: Not modifying manually-set roam "
				          "trigger on bandunit %u from %d to %d\n",
				          wlc->pub->unit, __FUNCTION__, this_band->bandunit,
				          this_band->roam_trigger, WLC_AUTO_ROAM_TRIGGER));

			/* do the same for the other band */
			if ((NBANDS(wlc) > 1 && other_band->roam_trigger == WLC_AUTO_ROAM_TRIGGER -
			     WLC_ROAM_TRIGGER_STEP)) {
				other_band->roam_trigger = WLC_AUTO_ROAM_TRIGGER;
				WL_ASSOC(("wl%d: %s: Setting roam trigger on bandunit %u to %d\n",
				          wlc->pub->unit, __FUNCTION__, other_band->bandunit,
				          WLC_AUTO_ROAM_TRIGGER));
			} else if (NBANDS(wlc) > 1 && other_band->roam_trigger !=
			           WLC_AUTO_ROAM_TRIGGER)
				WL_ASSOC(("wl%d: %s: Not modifying manually-set band roam "
				          "trigger on bandunit %u from %d to %d\n",
				          wlc->pub->unit, __FUNCTION__, other_band->bandunit,
				          other_band->roam_trigger, WLC_AUTO_ROAM_TRIGGER));

#ifdef WNM_BSSTRANS_EXT
			if (WBTEXT_ACTIVE(wlc->pub)) {
				int cu_trigger;
				cu_trigger = wlc_wnm_get_cu_trigger_percent(wlc, cfg);
				if (cu_trigger &&
					(cu_trigger == roam_prof->channel_usage +
					WLC_CU_TRIGGER_STEP)) {
					/* office environment: update cu trigger */
					cu_trigger = roam_prof->channel_usage;
					wlc_wnm_set_cu_trigger_percent(wlc, cfg, cu_trigger);
					WL_ASSOC(("wl%d.%d: %s: Setting cu trigger to %d\n",
						WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg), __FUNCTION__,
						cu_trigger));
				}
			}
#endif /* WNM_BSSTRANS_EXT */

			/* this means we are transitioning into a sparse environment
			 * from either an INDETERMINATE or dense one
			 */
		} else if (nentries == 1 && roam->ap_environment == AP_ENV_INDETERMINATE) {
			WL_ASSOC(("wl%d: %s: Auto-detecting sparse AP environment with "
			          "one roam target\n", wlc->pub->unit, __FUNCTION__));
			roam->ap_environment = AP_ENV_SPARSE;

			if (this_band->roam_trigger == WLC_AUTO_ROAM_TRIGGER) {
				this_band->roam_trigger -= WLC_ROAM_TRIGGER_STEP;
				WL_ASSOC(("wl%d: %s: Setting roam trigger on bandunit %u "
				          "to %d\n", wlc->pub->unit, __FUNCTION__,
				          this_band->bandunit, this_band->roam_trigger));
#ifdef WLABT
				if (WLABT_ENAB(wlc->pub) && (NBANDS(wlc) > 1) && !P2P_ACTIVE(wlc)) {
					roam->prev_bcn_timeout = roam->bcn_timeout;
					roam->bcn_timeout = ABT_MIN_TIMEOUT;
					WL_ASSOC(("wl%d: Setting bcn_timeout to %d\n",
						wlc->pub->unit, roam->bcn_timeout));
				}
#endif /* WLABT */
			} else
				WL_ASSOC(("wl%d: %s: Not modifying manually-set roam "
				          "trigger on bandunit %u from %d to %d\n",
				          wlc->pub->unit, __FUNCTION__, this_band->bandunit,
				          this_band->roam_trigger,
				          this_band->roam_trigger - WLC_ROAM_TRIGGER_STEP));

			if (NBANDS(wlc) > 1 && other_band->roam_trigger == WLC_AUTO_ROAM_TRIGGER) {
				other_band->roam_trigger -= WLC_ROAM_TRIGGER_STEP;
				WL_ASSOC(("wl%d: %s: Setting roam trigger on bandunit %u "
				          "to %d\n", wlc->pub->unit, __FUNCTION__,
				          other_band->bandunit, other_band->roam_trigger));
			} else if (NBANDS(wlc) > 1)
				WL_ASSOC(("wl%d: %s: Not modifying manually-set band roam "
				          "trigger on bandunit %u from %d to %d\n",
				          wlc->pub->unit, __FUNCTION__, other_band->bandunit,
				          other_band->roam_trigger,
				          other_band->roam_trigger - WLC_ROAM_TRIGGER_STEP));

#ifdef WNM_BSSTRANS_EXT
			if (WBTEXT_ACTIVE(wlc->pub)) {
				int cu_trigger;
				cu_trigger = wlc_wnm_get_cu_trigger_percent(wlc, cfg);
				if (cu_trigger &&
					cu_trigger == roam_prof->channel_usage) {
					/* home environment: update cu trigger */
					cu_trigger += WLC_CU_TRIGGER_STEP;
					wlc_wnm_set_cu_trigger_percent(wlc, cfg, cu_trigger);
					WL_ASSOC(("wl%d.%d: %s: Setting cu trigger to %d\n",
						WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg), __FUNCTION__,
						cu_trigger));
				}
			}
#endif /* WNM_BSSTRANS_EXT */
		}
	}
} /* wlc_roam_set_env */

static void
wlc_roam_motion_detect(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_roam_t *roam = cfg->roam;

	/* Check for motion */
	if (!roam->motion &&
	    ABS(cfg->current_bss->RSSI - roam->RSSIref) >= roam->motion_rssi_delta) {
		roam->motion = TRUE;

		/* force a full scan on the next iteration */
		roam->cache_valid = FALSE;
		roam->scan_block = 0;

		wlc->band->roam_trigger += MOTION_DETECT_TRIG_MOD;
		wlc->band->roam_delta -= MOTION_DETECT_DELTA_MOD;

		if (NBANDS(wlc) > 1) {
			wlc->bandstate[OTHERBANDUNIT(wlc)]->roam_trigger += MOTION_DETECT_TRIG_MOD;
			wlc->bandstate[OTHERBANDUNIT(wlc)]->roam_delta -=
			        MOTION_DETECT_DELTA_MOD;
		}

		WL_ASSOC(("wl%d.%d: Motion detected, invalidating roaming cache and "
		          "moving roam_delta to %d and roam_trigger to %d\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), wlc->band->roam_delta,
		          wlc->band->roam_trigger));
	}

	if (roam->motion && ++roam->motion_dur > roam->motion_timeout) {
		roam->motion = FALSE;
		/* update RSSIref in next watchdog */
		roam->RSSIref = 0;
		roam->motion_dur = 0;

		if ((wlc->band->roam_trigger <= wlc->band->roam_trigger_def) ||
			(wlc->band->roam_delta >= wlc->band->roam_delta_def))
			return;

		wlc->band->roam_trigger -= MOTION_DETECT_TRIG_MOD;
		wlc->band->roam_delta += MOTION_DETECT_DELTA_MOD;

		if (NBANDS(wlc) > 1) {
			wlc->bandstate[OTHERBANDUNIT(wlc)]->roam_trigger -= MOTION_DETECT_TRIG_MOD;
			wlc->bandstate[OTHERBANDUNIT(wlc)]->roam_delta +=
			        MOTION_DETECT_DELTA_MOD;
		}

		WL_ASSOC(("wl%d.%d: Motion timeout, restoring default values of "
		          "roam_delta to %d and roam_trigger to %d, new RSSI ref is %d\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), wlc->band->roam_delta,
		          wlc->band->roam_trigger, roam->RSSIref));
	}
} /* wlc_roam_motion_detect */

static void
wlc_roam_release_flow_cntrl(wlc_bsscfg_t *cfg)
{
	wlc_txflowcontrol_override(cfg->wlc, cfg->wlcif->qi, OFF, TXQ_STOP_FOR_PKT_DRAIN);
}

static void
wlc_assoc_roam(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	wlc_join_pref_t *join_pref = cfg->join_pref;

	if (!cfg->roam->assocroam)
		return;

	/* if assocroam is enabled, need to consider roam to band pref band */
	WL_ASSOC(("wlc_assoc_roam, assocroam enabled, band pref = %d\n", join_pref->band));

	if (BSSCFG_STA(cfg) &&
	    (as->type != AS_ROAM && as->type != AS_RECREATE) &&
	    cfg->associated && (join_pref->band != WLC_BAND_AUTO)) {
		uint ssid_cnt = 0, i;
		wlc_bss_info_t **bip, *tmp;
		wlc_bss_info_t *current_bss = cfg->current_bss;

		if (join_pref->band ==
			CHSPEC2WLC_BAND(current_bss->chanspec))
			return;

		bip = wlc->scan_results->ptrs;

		/* sort current_bss->ssid to the front */
		for (i = 0; i < wlc->scan_results->count; i++) {
			if (WLC_IS_CURRENT_SSID(cfg, (char*)bip[i]->SSID, bip[i]->SSID_len) &&
			    join_pref->band == CHSPEC2WLC_BAND(bip[i]->chanspec)) {
				if (i > ssid_cnt) {	/* swap if not self */
					tmp = bip[ssid_cnt];
					bip[ssid_cnt] = bip[i];
					bip[i] = tmp;
				}
				ssid_cnt++;
			}
		}

		if (ssid_cnt > 0) {
			WL_ASSOC(("assoc_roam: found %d matching ssid with current bss\n",
				ssid_cnt));
			/* prune itself */
			for (i = 0; i < ssid_cnt; i++)
				if (WLC_IS_CURRENT_BSSID(cfg, &bip[i]->BSSID) &&
				    current_bss->chanspec == bip[i]->chanspec) {
					ssid_cnt--;

					tmp = bip[ssid_cnt];
					bip[ssid_cnt] = bip[i];
					bip[i] = tmp;
				}
		}

		if (ssid_cnt > 0) {
			/* hijack this scan to start roaming completion after free memory */
			uint indx;
			wlc_bss_info_t *bi;

			WL_ASSOC(("assoc_roam: consider asoc roam with %d targets\n", ssid_cnt));

			/* free other scan_results since scan_results->count will be reduced */
			for (indx = ssid_cnt; indx < wlc->scan_results->count; indx++) {
				bi = wlc->scan_results->ptrs[indx];
				if (bi) {
					if (bi->bcn_prb)
						MFREE(wlc->osh, bi->bcn_prb, bi->bcn_prb_len);

					MFREE(wlc->osh, bi, sizeof(wlc_bss_info_t));
					wlc->scan_results->ptrs[indx] = NULL;
				}
			}

			wlc->scan_results->count = ssid_cnt;

			wlc_assoc_init(cfg, AS_ROAM);
			wlc_assoc_change_state(cfg, AS_SCAN);
			wlc_assoc_scan_complete(wlc, WLC_E_STATUS_SUCCESS, cfg);
		}
	}
} /* wlc_assoc_roam */

void
wlc_roam_bcns_lost(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_roam_t *roam = cfg->roam;

	/* XXX If we've received no beacons at all since association, give up.
	 * For a roam this will simply continue roaming (it will attempt to go
	 * back to prev, but it's too late...) but it allows the LINK DOWN and
	 * another roam attempt.  For a first association, aborting here will
	 * simply start roaming, so instead explicitly fail to make sure the
	 * E_SET_SSID is sent and disable to stop further attempts.
	 */
	if (cfg->assoc->state == AS_WAIT_RCV_BCN) {
		if (cfg->assoc->type == AS_ASSOCIATION) {
			/* Fail, then force a disassoc */
			WL_ASSOC(("wl%d.%d: ASSOC: bcns_lost in WAIT_RCV_BCN\n",
			          WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg)));
			WL_ASSOC(("Using assoc_done()/bsscfg_disable() to fail/stop.\n"));
			wlc_join_done_int(cfg, WLC_E_STATUS_FAIL);
			wlc_bsscfg_disable(wlc, cfg);
			return;
		}
		if (cfg->assoc->type == AS_ROAM) {
#ifdef WLFBT
			if (BSSCFG_IS_FBT(cfg) && (cfg->WPA_auth & WPA2_AUTH_FT)) {
				int8 bss_type = cfg->target_bss->bss_type;

				/* When bcns lost in WAIT_RCV_BCN during FBT roaming,
				 * the host keep having previous BSSID information
				 * since WLC_E_LINK event is suppressed for FBT.
				 * Roaming event need to be updated before aborting association,
				 * otherwise disconnection event from roamed AP can be blocked
				 * from DHD.
				 */
				WL_ASSOC_ERROR(("wl%d.%d: ROAM: bcns_lost in WAIT_RCV_BCN"
					"during FBT, update BSSID\n", WLCWLUNIT(wlc),
					WLC_BSSCFG_IDX(cfg)));
				wlc_join_done(cfg, &cfg->BSSID, bss_type, WLC_E_STATUS_SUCCESS);
			}
#endif /* WLFBT */
			WL_ASSOC(("wl%d.%d: ROAM: bcns_lost in WAIT_RCV_BCN, abort assoc\n",
			          WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg)));
			wlc_assoc_abort(cfg);
			return;
		}
		WL_ASSOC(("wl%d.%d: ASSOC: bcns_lost in WAIT_RCV_BCN, type %d\n",
		          WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg), cfg->assoc->type));
	}

	if (!roam->off &&
	    cfg->assoc->state == AS_IDLE &&
	    !ANY_SCAN_IN_PROGRESS(wlc->scan)) {
		WL_ASSOC(("wl%d.%d: ROAM: time_since_bcn %d, request roam scan\n",
		          WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg), roam->time_since_bcn));

		/* No longer need to track PM states since we may have lost our AP */
		wlc_reset_pmstate(cfg);

		/* Allow consecutive roam scans without delay for the first
		 * ROAM_CONSEC_BCNS_LOST_THRESH beacon lost events after
		 * initially losing beacons
		 */
		if (roam->consec_roam_bcns_lost <= ROAM_CONSEC_BCNS_LOST_THRESH) {
			/* clear scan_block so that the roam scan will happen w/o delay */
			roam->scan_block = 0;
			roam->consec_roam_bcns_lost++;
			WL_ASSOC(("wl%d.%d: ROAM %s #%d w/o delay, setting scan_block to 0\n",
			          WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg), __FUNCTION__,
			          roam->consec_roam_bcns_lost));
		}

		wlc_roamscan_start(cfg, WLC_E_REASON_BCNS_LOST);
	}
} /* wlc_roam_bcns_lost */

static int
wlc_roam_trigger_logical_dbm(wlc_info_t *wlc, wlcband_t *band, int val)
{
	int trigger_dbm = WLC_NEVER_ROAM_TRIGGER;

	BCM_REFERENCE(wlc);

	if (val == WLC_ROAM_TRIGGER_DEFAULT)
		trigger_dbm = band->roam_trigger_init_def;
	else if (val == WLC_ROAM_TRIGGER_BANDWIDTH)
		trigger_dbm = band->roam_trigger_init_def + WLC_ROAM_TRIGGER_STEP;
	else if (val == WLC_ROAM_TRIGGER_DISTANCE)
		trigger_dbm = band->roam_trigger_init_def - WLC_ROAM_TRIGGER_STEP;
	else if (val == WLC_ROAM_TRIGGER_AUTO)
		trigger_dbm = WLC_AUTO_ROAM_TRIGGER;
	else {
		ASSERT(0);
	}

	return trigger_dbm;
}

/** Make decisions about roaming based upon feedback from the tx rate */
void
wlc_txrate_roam(wlc_info_t *wlc, struct scb *scb, tx_status_t *txs, bool pkt_sent,
	bool pkt_max_retries, uint8 ac)
{
	wlc_bsscfg_t *cfg;
	wlc_roam_t *roam;

	/* this code doesn't work if we have an override rate */
	if (wlc->band->rspec_override || wlc->band->mrspec_override)
		return;

	ASSERT(scb != NULL);

	cfg = SCB_BSSCFG(scb);
	if (cfg == NULL)
		return;

	roam = cfg->roam;

	/* prevent roaming for tx rate issues too frequently */
	if (roam->ratebased_roam_block > 0)
		return;

	if (pkt_sent && !wlc_scb_ratesel_minrate(wlc->wrsi, scb, txs, ac))
		roam->txpass_cnt++;
	else
		roam->txpass_cnt = 0;

	/* should we roam on too many packets at the min tx rate? */
	if (roam->minrate_txpass_thresh) {
		if (pkt_sent) {
			if (wlc_scb_ratesel_minrate(wlc->wrsi, scb, txs, ac))
				roam->minrate_txpass_cnt++;
			else
				roam->minrate_txpass_cnt = 0;

			if (roam->minrate_txpass_cnt > roam->minrate_txpass_thresh &&
			    !roam->active) {
				WL_ASSOC(("wl%d: %s: Starting roam scan due to %d "
				          "packets at the most basic rate\n",
				          WLCWLUNIT(wlc), __FUNCTION__,
				          roam->minrate_txpass_cnt));
#ifdef WLP2P
				if (!BSS_P2P_ENAB(wlc, cfg))
#endif // endif
					wlc_roamscan_start(cfg, WLC_E_REASON_MINTXRATE);
				roam->minrate_txpass_cnt = 0;
				roam->ratebased_roam_block = ROAM_REASSOC_TIMEOUT;
			}
		}
	}

	/* should we roam on too many tx failures at the min rate? */
	if (roam->minrate_txfail_thresh) {
		if (pkt_sent)
			roam->minrate_txfail_cnt = 0;
		else if (pkt_max_retries && wlc_scb_ratesel_minrate(wlc->wrsi, scb, txs, ac)) {
			if (++roam->minrate_txfail_cnt > roam->minrate_txfail_thresh &&
			    !roam->active) {
				WL_ASSOC(("wl%d: starting roamscan for txfail\n",
				          WLCWLUNIT(wlc)));
#ifdef WLP2P
				if (!BSS_P2P_ENAB(wlc, cfg))
#endif // endif
					wlc_roamscan_start(cfg, WLC_E_REASON_TXFAIL);
				roam->minrate_txfail_cnt = 0; /* throttle roaming */
				roam->ratebased_roam_block = ROAM_REASSOC_TIMEOUT;
			}
		}
	}
} /* wlc_txrate_roam */

#if defined(BCMDBG) || defined(WLMSG_ASSOC)
static void
wlc_print_roam_status(wlc_bsscfg_t *cfg, uint roam_reason, bool printcache)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_roam_t *roam = cfg->roam;
	uint idx;
	const char* event_name;
	char eabuf[ETHER_ADDR_STR_LEN];

	static struct {
		uint event;
		const char *event_name;
	} event_names[] = {
		{WLC_E_REASON_INITIAL_ASSOC, "INITIAL ASSOCIATION"},
		{WLC_E_REASON_LOW_RSSI, "LOW_RSSI"},
		{WLC_E_REASON_DEAUTH, "RECEIVED DEAUTHENTICATION"},
		{WLC_E_REASON_DISASSOC, "RECEIVED DISASSOCATION"},
		{WLC_E_REASON_BCNS_LOST, "BEACONS LOST"},
		{WLC_E_REASON_FAST_ROAM_FAILED, "FAST ROAM FAILED"},
		{WLC_E_REASON_DIRECTED_ROAM, "DIRECTED ROAM"},
		{WLC_E_REASON_TSPEC_REJECTED, "TSPEC REJECTED"},
		{WLC_E_REASON_BETTER_AP, "BETTER AP FOUND"},
		{WLC_E_REASON_MINTXRATE, "STUCK AT MIN TX RATE"},
		{WLC_E_REASON_BSSTRANS_REQ, "REQUESTED ROAM"},
		{WLC_E_REASON_TXFAIL, "TOO MANY TXFAILURES"}
	};

	event_name = "UNKNOWN";
	for (idx = 0; idx < ARRAYSIZE(event_names); idx++) {
		if (event_names[idx].event == roam_reason)
		    event_name = event_names[idx].event_name;
	}

	WL_ASSOC(("wl%d: Current roam reason is %s. The cache has %u entries.\n", wlc->pub->unit,
	          event_name, roam->cache_numentries));
	if (printcache) {
		for (idx = 0; idx < roam->cache_numentries; idx++) {
			WL_ASSOC(("\t Entry %u => chanspec 0x%x (BSSID: %s)\t", idx,
			          roam->cached_ap[idx].chanspec,
			          bcm_ether_ntoa(&roam->cached_ap[idx].BSSID, eabuf)));
			if (roam->cached_ap[idx].time_left_to_next_assoc)
				WL_ASSOC(("association blocked for %d more seconds\n",
				          roam->cached_ap[idx].time_left_to_next_assoc));
			else
				WL_ASSOC(("assocation not blocked\n"));
		}
	}
}
#endif /* defined(BCMDBG) || defined(WLMSG_ASSOC) */

static uint8 *
wlc_join_attempt_info_add_tlv(awd_data_t *awd_data, uint8 tagid)
{
	awd_tag_data_t *awd_tag_data;
	uint8 tlv_len, total_tlv_len;

	switch (tagid) {
	case AWD_DATA_TAG_JOIN_CLASSIFICATION_INFO:
		tlv_len = sizeof(join_classification_info_t);
		break;
	case AWD_DATA_TAG_JOIN_TARGET_CLASSIFICATION_INFO:
		tlv_len = sizeof(join_target_classification_info_t);
		break;
	case AWD_DATA_TAG_ASSOC_STATE:
		tlv_len = sizeof(join_assoc_state_t);
		break;
	case AWD_DATA_TAG_CHANNEL:
		tlv_len = sizeof(join_channel_t);
		break;
	case AWD_DATA_TAG_TOTAL_NUM_OF_JOIN_ATTEMPTS:
		tlv_len = sizeof(join_total_attempts_num_t);
		break;
	default:
		WL_ERROR(("%s: Join Attempt Info unknown TLV type %d \n",
			__FUNCTION__, tagid));
		ASSERT(0);
		return NULL;
	}

	/* check if if exist availeble place for target TLV */
	total_tlv_len = tlv_len + sizeof(awd_tag_data_t);
	if (awd_data->length + total_tlv_len > JOIN_DATA_INFO_LEN) {
		WL_ERROR(("%s: Join Attempt Info to long, max_len %d total_len %d \n",
			__FUNCTION__, JOIN_DATA_INFO_LEN, awd_data->length + total_tlv_len));
		return NULL;
	}

	/* update TLV Header */
	awd_tag_data = (awd_tag_data_t *)(awd_data->data + awd_data->length);
	awd_tag_data->tagid = tagid;
	awd_tag_data->length = tlv_len;

	/* update total length */
	awd_data->length += total_tlv_len;
	return awd_tag_data->data;
}

static void
wlc_join_attempt_info_init(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	assoc_cfg_cubby_t *assoc_cfg_cubby;
	wlc_join_attempt_info_t *join_info;
	awd_data_t *awd_data;
	join_classification_info_t *join_tlv;
	wl_join_assoc_params_t *assoc_params;

	assoc_cfg_cubby = BSSCFG_ASSOC_CUBBY(wlc->as, cfg);
	ASSERT(assoc_cfg_cubby);
	join_info = &(assoc_cfg_cubby->join_attempt_info);

	/* free awd_data from previous join */
	if (join_info->buf_inuse == TRUE) {
		memset(join_info->buf, 0, JOIN_DATA_INFO_LEN);
		join_info->buf_inuse = FALSE;
		join_info->stats_exist = FALSE;
	}

	join_info->buf_inuse = TRUE;
	join_info->num_of_bssids = 0;
	join_info->join_start_time = OSL_SYSUPTIME();

	awd_data = (awd_data_t *)join_info->buf;
	awd_data->version = AWD_DATA_VERSION;

	/* update JOIN_CLASSIFICATION_INFO TLV */
	join_tlv = (join_classification_info_t *)
		wlc_join_attempt_info_add_tlv(awd_data, AWD_DATA_TAG_JOIN_CLASSIFICATION_INFO);
	if (join_tlv == NULL)
		return;

	if ((assoc_params = wlc_bsscfg_assoc_params(cfg)) != NULL) {
		join_tlv->num_of_targets =
			MAX(assoc_params->chanspec_num, assoc_params->bssid_cnt);
	}

	join_tlv->wsec = cfg->wsec;
	join_tlv->wpa_auth = cfg->WPA_auth;
	join_tlv->total_attempts_num = 0;
}

static void
wlc_join_attempt_info_start(wlc_bsscfg_t *cfg, wlc_bss_info_t *bi)
{
	wlc_info_t *wlc = cfg->wlc;
	assoc_cfg_cubby_t *assoc_cfg_cubby;
	wlc_join_attempt_info_t *join_info;
	awd_data_t *awd_data;
	join_target_classification_info_t *trgt_join_tlv;

	assoc_cfg_cubby = BSSCFG_ASSOC_CUBBY(wlc->as, cfg);
	ASSERT(assoc_cfg_cubby);
	join_info = &(assoc_cfg_cubby->join_attempt_info);

	if (join_info->buf_inuse == FALSE)
		return;

	/* update number of join attempts */
	join_info->num_of_bssids++;

	/* update previous join attempt data */
	if (join_info->num_of_bssids > 1) {
		/* N.B.: use WLC_E_STATUS_INVALID to get the current as->type/as->state
		 * states recorded...
		 */
		wlc_join_attempt_info_end(cfg, WLC_E_STATUS_INVALID);
	}

	/* new join attempt */
	join_info->attempt_done = FALSE;
	awd_data = (awd_data_t *)join_info->buf;

	/* update JOIN_TARGET_CLASSIFICATION_INFO TLV */
	trgt_join_tlv = (join_target_classification_info_t *)
		wlc_join_attempt_info_add_tlv(awd_data,
		AWD_DATA_TAG_JOIN_TARGET_CLASSIFICATION_INFO);
	if (trgt_join_tlv == NULL)
		return;

	trgt_join_tlv->rssi = (int8)bi->RSSI;
	trgt_join_tlv->channel = CHSPEC_CHANNEL(bi->chanspec);
	memcpy(&trgt_join_tlv->oui, &bi->BSSID.octet, sizeof(trgt_join_tlv->oui));
	trgt_join_tlv->time_duration = OSL_SYSUPTIME();
}

static void
wlc_join_attempt_info_end(wlc_bsscfg_t *cfg, int status)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	assoc_cfg_cubby_t *assoc_cfg_cubby;
	wlc_join_attempt_info_t *join_info;
	awd_data_t *awd_data;
	join_classification_info_t *join_tlv;
	join_target_classification_info_t *trgt_join_tlv;
	uint32 start_time;
#if defined(CCA_STATS)
	wlc_congest_channel_req_t cca_results;
	int cca_status;
#endif /* CCA_STATS */

	assoc_cfg_cubby = BSSCFG_ASSOC_CUBBY(wlc->as, cfg);
	ASSERT(assoc_cfg_cubby);
	join_info = &(assoc_cfg_cubby->join_attempt_info);

	if (join_info->buf_inuse == FALSE)
		return;

	awd_data = (awd_data_t *)join_info->buf;

	/* update JOIN_CLASSIFICATION_INFO TLV */
	join_tlv = (join_classification_info_t *)(awd_data->data + sizeof(awd_tag_data_t));

	/* current join attempt already updated */
	if (join_info->attempt_done == TRUE)
		return;

	join_tlv->time_to_join = OSL_SYSUPTIME() - join_info->join_start_time;
	/* force to the end states if success */
	if (status == WLC_E_STATUS_SUCCESS) {
		join_tlv->assoc_type = AS_NONE;
		join_tlv->assoc_state = AS_IDLE;
	}
	/* record the current states if failure */
	else {
		join_tlv->assoc_type = (uint8)as->type;
		join_tlv->assoc_state = (uint8)as->state;
	}
	join_tlv->wsec_portopen = cfg->wsec_portopen;
	join_tlv->total_attempts_num += as->bss_retries;
	join_tlv->wpa_state = 0;
#ifdef BCMSUP_PSK
	if (SUP_ENAB(wlc->pub)) {
		join_tlv->wpa_state = wlc_sup_get_wpa_state(wlc->idsup, cfg);
	}
#endif /* BCMSUP_PSK */

	/* no hits for given ssid */
	if (join_info->num_of_bssids == 0)
		return;

	/* update JOIN_TARGET_CLASSIFICATION_INFO TLV */
	trgt_join_tlv  = (join_target_classification_info_t *)
		(awd_data->data + awd_data->length - sizeof(join_target_classification_info_t));

	start_time = trgt_join_tlv->time_duration;
	trgt_join_tlv->time_duration = OSL_SYSUPTIME() - start_time;
	trgt_join_tlv->num_of_attempts = as->bss_retries;
#if defined(CCA_STATS)
	cca_status = cca_query_stats(wlc, wf_chspec_ctlchspec(cfg->current_bss->chanspec),
		1, &cca_results, sizeof(cca_results));
	if (cca_status == BCME_OK && cca_results.secs[0].duration > 0) {
		wlc_congest_t wlc_congest = cca_results.secs[0];
		int cca_congest = wlc_congest.congest_ibss + wlc_congest.congest_obss +
			wlc_congest.interference;
		trgt_join_tlv->cca = (cca_congest * 100) / wlc_congest.duration;
	}
#endif /* CCA_STATS */

	join_info->attempt_done = TRUE;
}

static void
wlc_join_attempt_info_update_tot_time(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	assoc_cfg_cubby_t *assoc_cfg_cubby;
	wlc_join_attempt_info_t *join_info;
	awd_data_t *awd_data;
	join_classification_info_t *join_tlv;

	assoc_cfg_cubby = BSSCFG_ASSOC_CUBBY(wlc->as, cfg);
	ASSERT(assoc_cfg_cubby);
	join_info = &(assoc_cfg_cubby->join_attempt_info);

	if (join_info->buf_inuse == FALSE)
		return;

	awd_data = (awd_data_t *)join_info->buf;

	/* update JOIN_CLASSIFICATION_INFO TLV */
	join_tlv = (join_classification_info_t *)(awd_data->data + sizeof(awd_tag_data_t));

	/* join time already updated */
	if (join_tlv->time_to_join > 0)
		return;

	join_tlv->time_to_join = OSL_SYSUPTIME() - join_info->join_start_time;
}

int
wlc_join_attempt_info_get(wlc_bsscfg_t *cfg, uint8 *arg, int len)
{
	wlc_info_t *wlc = cfg->wlc;
	assoc_cfg_cubby_t *assoc_cfg_cubby;
	wlc_join_attempt_info_t *join_info;
	awd_data_t *awd_data;
	join_classification_info_t *join_tlv;
	join_target_classification_info_t *trgt_join_tlv = NULL;
	join_assoc_state_t *assoc_state_tlv;
	join_channel_t *channel_tlv;
	join_total_attempts_num_t *num_attemps_tlv;
	int total_len;

	assoc_cfg_cubby = BSSCFG_ASSOC_CUBBY(wlc->as, cfg);
	ASSERT(assoc_cfg_cubby);
	join_info = &(assoc_cfg_cubby->join_attempt_info);

	if (join_info->buf_inuse == FALSE) {
		return BCME_NOTREADY;
	}

	/* update last join attempt if needed */
	/* N.B.: use WLC_E_STATUS_INVALID to get the current as->type/as->state
	 * states recorded...
	 */
	wlc_join_attempt_info_end(cfg, WLC_E_STATUS_INVALID);
	wlc_join_attempt_info_update_tot_time(cfg);

	awd_data = (awd_data_t *)join_info->buf;

	if (join_info->stats_exist == FALSE) {
		/* get JOIN_CLASSIFICATION_INFO TLV */
		join_tlv = (join_classification_info_t *)(awd_data->data + sizeof(awd_tag_data_t));

		/* get JOIN_TARGET_CLASSIFICATION_INFO TLV */
		if (join_info->num_of_bssids > 0) {
			trgt_join_tlv  = (join_target_classification_info_t *)
				(awd_data->data + awd_data->length -
				sizeof(join_target_classification_info_t));
		}

		/* update TAG_ASSOC_STATE TLV */
		assoc_state_tlv = (join_assoc_state_t *)
			wlc_join_attempt_info_add_tlv(awd_data, AWD_DATA_TAG_ASSOC_STATE);
		if (assoc_state_tlv == NULL)
			return BCME_BUFTOOSHORT;
		assoc_state_tlv->assoc_state = join_tlv->assoc_state;

		/* update TAG_CHANNEL TLV */
		channel_tlv = (join_channel_t *)
			wlc_join_attempt_info_add_tlv(awd_data, AWD_DATA_TAG_CHANNEL);
		if (channel_tlv == NULL)
			return BCME_BUFTOOSHORT;
		channel_tlv->channel = (trgt_join_tlv) ? trgt_join_tlv->channel : 0;

		/* update TAG_TOTAL_NUM_OF_JOIN_ATTEMPTS TLV */
		num_attemps_tlv = (join_total_attempts_num_t *)
			wlc_join_attempt_info_add_tlv(awd_data,
			AWD_DATA_TAG_TOTAL_NUM_OF_JOIN_ATTEMPTS);
		if (num_attemps_tlv == NULL)
			return BCME_BUFTOOSHORT;
		num_attemps_tlv->total_attempts_num = join_tlv->total_attempts_num;

		join_info->stats_exist = TRUE;
	}

	total_len = awd_data->length + sizeof(awd_data_t);

	if (len < total_len) {
		return BCME_BUFTOOSHORT;
	}

	memcpy(arg, join_info->buf, total_len);
	return BCME_OK;
}

int
wlc_roam_cache_info_get(wlc_bsscfg_t *cfg, void *params, uint p_len, void *arg, uint out_len)
{
	const bcm_xtlv_opts_t no_pad = BCM_XTLV_OPTION_NONE;
	bcm_xtlv_t *bcn_rpt_tlv, *out_xtlv;
	uint16 bcn_rpt_cmd_id;
	int err;

	/* all commands start with an XTLV container */
	bcn_rpt_tlv = params;

	if (p_len < BCM_XTLV_HDR_SIZE || !bcm_valid_xtlv(bcn_rpt_tlv, p_len, no_pad)) {
		return BCME_BADARG;
	}

	/* collect the id/len values */
	bcn_rpt_cmd_id = bcn_rpt_tlv->id;

	out_xtlv = arg;

	switch (bcn_rpt_cmd_id) {
		case WL_RMC_RPT_CMD_VER:
			err = wlc_rmc_rpt_version_get(out_xtlv, out_len);
			break;
		case WL_RMC_RPT_CMD_DATA:
			err = wlc_rmc_rpt_config_get(cfg, out_xtlv, out_len);
			break;
		default:
			err = BCME_UNSUPPORTED;
			break;
		}

	return (err);
}

/* Function to get the roam cache version. */
static int32
wlc_rmc_rpt_version_get(bcm_xtlv_t *out_xtlv, uint out_len)
{
	bcm_xtlvbuf_t tbuf;
	int32 err;
	uint32 version;

	out_xtlv->id = WL_RMC_RPT_CMD_VER;
	/* bcm_xtlv_buf_init() takes length up to uint16 */
	out_len -= BCM_XTLV_HDR_SIZE;
	out_len = MIN(out_len, 0xFFFF);

	err = bcm_xtlv_buf_init(&tbuf, out_xtlv->data, (uint16)out_len, BCM_XTLV_OPTION_NONE);
	if (err) {
		return err;
	}

	version = WL_RMC_REPORT_CMD_VERSION;
	err = bcm_xtlv_put32(&tbuf, WL_RMC_RPT_XTLV_VER, &version, 1);

	/* Update payload len */
	out_xtlv->len = bcm_xtlv_buf_len(&tbuf);
	return err;
}

/* Function to get the roam cache data. */
static int32
wlc_rmc_rpt_config_get(wlc_bsscfg_t *cfg, bcm_xtlv_t *out_xtlv, uint out_len)
{
	uint16 req_id_list[] = {
		WL_RMC_RPT_XTLV_BSS_INFO,
		WL_RMC_RPT_XTLV_CANDIDATE_INFO
	};
	bcm_xtlvbuf_t tbuf;
	int32 i, req_id_count, err;
	uint j;
	rmc_bss_info_t rmc_bss_info;
	rmc_candidate_info_t rmc_candidate_info;

	if (!cfg->roam->cache_valid) {
		/* i item (current bss) in req buffer */
		req_id_count = 1;
	} else {
		/* size (in elements) of the req buffer */
		req_id_count = ARRAYSIZE(req_id_list);
	}

	out_xtlv->id = WL_RMC_RPT_CMD_DATA;
	/* bcm_xtlv_buf_init() takes length up to uint16 */
	out_len -= BCM_XTLV_HDR_SIZE;
	out_len = MIN(out_len, 0xFFFF);

	err = bcm_xtlv_buf_init(&tbuf, out_xtlv->data, (uint16)out_len, BCM_XTLV_OPTION_NONE);
	if (err) {
		return err;
	}

	for (i = 0; i < req_id_count; i++) {
		switch (req_id_list[i]) {
		case WL_RMC_RPT_XTLV_BSS_INFO:
			rmc_bss_info.rssi = cfg->current_bss->RSSI;
			rmc_bss_info.reason = cfg->roam->cached_ap[0].rssi_st.roam_st.reason;
			rmc_bss_info.status = cfg->roam->cached_ap[0].rssi_st.roam_st.status;
			rmc_bss_info.fullscan_count = cfg->roam->cached_ap[0].fullscan_cnt;
			if (cfg->roam->cached_ap[0].time) {
				rmc_bss_info.time_full_scan =
					OSL_SYSUPTIME() - cfg->roam->cached_ap[0].time;
			} else {
				rmc_bss_info.time_full_scan = 0;
			}
			err = bcm_xtlv_put_data(&tbuf, req_id_list[i],
				(uint8 *)&rmc_bss_info, sizeof(rmc_bss_info_t));
			break;

		case WL_RMC_RPT_XTLV_CANDIDATE_INFO:
			for (j = 1; j < cfg->roam->cache_numentries; j++) {
				rmc_candidate_info.rssi = cfg->roam->cached_ap[j].rssi_st.rssi;
				rmc_candidate_info.ctl_channel =
					CHSPEC_CHANNEL(cfg->roam->cached_ap[j].chanspec);
				rmc_candidate_info.time_last_seen =
					OSL_SYSUPTIME() - cfg->roam->cached_ap[j].time;
				rmc_candidate_info.bss_load =
					cfg->roam->cached_ap[j].bss_load;
				bcopy(&cfg->roam->cached_ap[j].BSSID, rmc_candidate_info.bssid,
					ETHER_ADDR_LEN);
				err = bcm_xtlv_put_data(&tbuf, req_id_list[i],
					(uint8 *)&rmc_candidate_info,
					sizeof(rmc_candidate_info_t));
			}
			break;

		default:
			err = BCME_UNSUPPORTED;
			break;
		}
	}

	/* Update payload len */
	out_xtlv->len = bcm_xtlv_buf_len(&tbuf);
	return (err);
}
#endif /* STA */

#if defined(STA) && defined(BCMSUP_PSK)
/* Helper fn: is any STA BSS is awaiting initial key? Return first such cfg */
#define SCAN_KEY_BLOCK_DEFAULT	6000
static wlc_bsscfg_t *
wlc_assoc_keying_in_progress(wlc_info_t *wlc)
{
	int idx;
	wlc_bsscfg_t *cfg;
	struct scb *scb;
	uint scbband;
	wlc_info_t *cwlc;

	FOREACH_ALL_WLC_BSS(wlc, idx, cfg) {
		cwlc = cfg->wlc;

		/* Only care about associated STA */
		if (!BSSCFG_STA(cfg) || !cfg->associated) {
			continue;
		}
		if (!ETHER_ISNULLADDR(&cfg->BSSID) && !wlc_portopen(cfg)) {
			scbband = CHSPEC_WLCBANDUNIT(cfg->current_bss->chanspec);
			if ((scb = wlc_scbfindband(cwlc, cfg, &cfg->BSSID, scbband)) != NULL) {
				uint32 time, tmo = 0;

				if (SUP_ENAB(cwlc->pub)) {
					tmo = wlc_sup_get_bsscfg_wpa_psk_tmo(cfg);
				}

				if (!tmo)
					tmo = SCAN_KEY_BLOCK_DEFAULT;
				time = cwlc->pub->now - scb->assoctime;
				time *= 1000;
				if (time <= tmo) {
					return cfg;
				}
			}
		}
	}

	return NULL;
}
#endif /* STA && BCMSUP_PSK */

#ifdef STA
/** association request on different bsscfgs will be queued up in wlc->assco_req[] */
/** association request has the highest priority over scan (af, scan, excursion),
 * roam, rm.
 */
static int
wlc_mac_request_assoc(wlc_info_t *wlc, wlc_bsscfg_t *cfg, int req)
{
	int err = BCME_OK;
	wlc_assoc_t *as;

	ASSERT(cfg != NULL);

	/* abort assoc/roam except disassoc in progress on this bsscfg */
	as = cfg->assoc;
	ASSERT(as != NULL);

	if (req == WLC_ACTION_ASSOC) {
		if (as->state != AS_IDLE && as->state != AS_WAIT_DISASSOC) {
			WL_INFORM(("wl%d: assoc request while association is in progress, "
					   "aborting association\n", wlc->pub->unit));
			wlc_assoc_abort(cfg);
		}
	}

	/* abort any roam in progress */
	if (AS_IN_PROGRESS(wlc)) {
		wlc_bsscfg_t *bc = wlc->as->cmn->assoc_req[0];

		ASSERT(bc != NULL);

		as = bc->assoc;
		ASSERT(as != NULL);

		/* Do not abort when in state AS_XX_WAIT_RCV_BCN. In this state
		 * association is already completed, if we abort in this state then all
		 * those tasks which are performed as part of wlc_join_complete will be
		 * skipped but still maintain the association.
		 * This leaves power save and 11h/d in wrong state
		 */
		if (as->state != AS_IDLE &&
			(as->type == AS_ROAM && as->state != AS_WAIT_RCV_BCN)) {
			WL_INFORM(("wl%d: assoc request while roam is in progress, "
			           "aborting roam\n", wlc->pub->unit));
			wlc_assoc_abort(bc);
		}
	}

	/* abort any scan engine usage */
	if (ANY_SCAN_IN_PROGRESS(wlc->scan) &&
		TRUE) {
		wlc_info_t *cur_wlc;
		int idx;
		WL_INFORM(("wl%d: assoc request while scan is in progress, "
		           "aborting scan\n", wlc->pub->unit));
		FOREACH_WLC(wlc->cmn, idx, cur_wlc) {
			wlc_scan_abort(cur_wlc->scan, WLC_E_STATUS_NEWASSOC);
		}
	}

#ifdef WLRM
	if (WLRM_ENAB(wlc->pub) && wlc_rminprog(wlc)) {
		WL_INFORM(("wl%d: association request while radio "
		           "measurement is in progress, aborting measurement\n",
		           wlc->pub->unit));
		wlc_rm_stop(wlc);
	}
#endif /* WLRM */

#ifdef WL11K
	if (wlc_rrm_inprog(wlc)) {
		WL_INFORM(("wl%d: association request while radio "
			   "measurement is in progress, aborting RRM\n",
			   wlc->pub->unit));
		wlc_rrm_stop(wlc);
	}
#endif /* WL11K */

	return err;
}

/**
 * roam request will be granted only when there is no outstanding association requests
 * and no other roam requests pending.
 *
 * roam has lower priority than association and scan (af, scan);
 * has higher priority than rm and excursion
 *
 * roam is also blocked during key exchange handshake
 */
static int
wlc_mac_request_roam(wlc_info_t *wlc, wlc_bsscfg_t *cfg, int req)
{
	int err = BCME_OK;
#ifdef BCMSUP_PSK
	wlc_bsscfg_t *keycfg = NULL;
#endif /* BCMSUP_PSK */
	BCM_REFERENCE(req);

	ASSERT(cfg != NULL);

	/* deny if any assoc/roam is in progress */
	if (AS_IN_PROGRESS(wlc)) {
		WL_INFORM(("wl%d: roam scan request blocked for association/roam in progress\n",
		           wlc->pub->unit));
		err = BCME_ERROR;
	}
#ifdef BCMSUP_PSK
	/* deny if keying in progress */
	else if ((keycfg = wlc_assoc_keying_in_progress(wlc))) {
		WL_INFORM(("wl%d: roam mac request (%d) cfg %d blocked for cfg %d "
		           "key exchange\n", wlc->pub->unit, req,
		           WLC_BSSCFG_IDX(cfg), WLC_BSSCFG_IDX(keycfg)));
		err = BCME_BUSY;
	}
#endif /* BCMSUP_PSK */
	/* deny if any scan engine is in use */
	else if (ANY_SCAN_IN_PROGRESS(wlc->scan)) {
#ifdef WLPFN
		if (WLPFN_ENAB(wlc->pub) &&
		          wl_pfn_scan_in_progress(wlc->pfn) &&
		          wlc_is_critical_roam(cfg)) {
			/* Critical roam must have higher priority over PFN scan */
			WL_INFORM(("wl%d: Aborting PFN scan in favor of critical roam\n",
			        wlc->pub->unit));
			wlc_scan_abort(wlc->scan, WLC_E_STATUS_ABORT);
		} else
#endif /* WLPFN */
		 {
			WL_INFORM(("wl%d: roam scan blocked for scan in progress\n",
			       wlc->pub->unit));
			err = BCME_ERROR;
		}
	} else if (cfg->pm->PSpoll) {
		WL_INFORM(("wl%d: roam scan blocked for outstanding PS-Poll\n",
		           wlc->pub->unit));
		err = BCME_ERROR;
	} else if (BSS_QUIET_STATE(wlc->quiet, cfg) & SILENCE) {
		WL_INFORM(("wl%d: roam scan blocked for 802.11h Quiet Period\n",
		           wlc->pub->unit));
		err = BCME_ERROR;
	}
#ifdef WLRM
	else if (WLRM_ENAB(wlc->pub) && wlc_rminprog(wlc)) {
		WL_INFORM(("wl%d: roam scan while radio measurement is in progress, "
		           "aborting measurement\n",
		           wlc->pub->unit));
		wlc_rm_stop(wlc);
	}
#endif /* WLRM */
#ifdef WL11K
	else if (wlc_rrm_inprog(wlc)) {
		WL_INFORM(("wl%d: roam scan while radio measurement is in progress,"
			" aborting RRM\n",
			wlc->pub->unit));
		wlc_rrm_stop(wlc);
	}
#endif /* WL11K */

	return err;
} /* wlc_mac_request_roam */
#endif /* STA */

/**
 * scan engine request will be granted only when there is no association in progress
 * among different priorities within the scan request:
 *     af > scan > iscan|escan > pno scan > excursion;
 *     scan request has higher priority than roam and rm;
 * scan is blocked during key exchange handshake
 */
static int
wlc_mac_request_scan(wlc_info_t *wlc, wlc_bsscfg_t *cfg, int req, uint scan_flags)
{
	int err = BCME_OK;
	wlc_bsscfg_t *other_cfg;
	int idx = 0;
	bool p2p_go_flag = FALSE;
#ifdef BCMSUP_PSK
	wlc_bsscfg_t *keycfg = NULL;
#endif /* BCMSUP_PSK */

	if (req == WLC_ACTION_SCAN || req == WLC_ACTION_ISCAN || req == WLC_ACTION_ESCAN) {
		FOREACH_BSS(wlc, idx, other_cfg) {
			if (P2P_GO(wlc, other_cfg)) {
				p2p_go_flag = TRUE;
			}
		}
		/* When 11h is enabled, prevent AP from leaving a DFS
		 * channel while in the in-service-monitor mode.
		 * Exception - ETSI/EDCRS_EU - where scan is allowed as returning to clear
		 * DFS radar channel won't cost a CAC again.
		 */
		if (wlc_radar_chanspec(wlc->cmi, wlc->home_chanspec) && (
#ifndef WL_SCAN_DFS_HOME
				!wlc->is_edcrs_eu ||
#endif /* WL_SCAN_DFS_HOME */
				wlc_quiet_chanspec(wlc->cmi, wlc->home_chanspec)) &&
				((!WLC_APSTA_ON_RADAR_CHANNEL(wlc) && AP_ACTIVE(wlc) &&
				  WL11H_AP_ENAB(wlc) && !p2p_go_flag) ||
#ifdef SLAVE_RADAR
				 (WL11H_STA_ENAB(wlc) && STA_ACTIVE(wlc)) ||
#endif /* SLAVE_RADAR */
				 FALSE)) {
			WL_INFORM(("wl%d: %s: On radar channel (not ETSI), WLC_SCAN ignored\n",
					wlc->pub->unit,	__FUNCTION__));
			err = BCME_SCANREJECT;
			return err;
		}
	}

	if (FALSE) {
		; /* empty */
	}
#ifdef STA
	/* is any assoc/roam in progress? */
	else if (AS_IN_PROGRESS(wlc)) {
		wlc_bsscfg_t *bc = wlc->as->cmn->assoc_req[0];
		wlc_assoc_t *as;

		ASSERT(bc != NULL);

		as = bc->assoc;
		ASSERT(as != NULL);

		/* is any assoc in progress? */
		if (as->state != AS_IDLE && as->type != AS_ROAM) {
			WL_ASSOC_ERROR(("wl%d: scan request blocked for association in progress\n",
				wlc->pub->unit));
			err = BCME_BUSY;
		}
		else if (req == WLC_ACTION_PNOSCAN && wlc_is_critical_roam(bc)) {
			/* don't abort critical roam for PNO scan */
			WL_INFORM(("wl%d: PNO scan request blocked for critical roam\n",
			     wlc->pub->unit));
			err = BCME_NOTREADY;
		} else if (scan_flags & WL_SCANFLAGS_LOW_PRIO) {
			WL_ASSOC_ERROR(("wl%d: low priority scan request blocked"
				" for roaming in progress\n", wlc->pub->unit));
			err = BCME_BUSY;
		} else {
			WL_INFORM(("wl%d: scan request while roam is in progress, aborting"
			           " roam\n", wlc->pub->unit));
			wlc_assoc_abort(bc);
		}
	}
#ifdef BCMSUP_PSK
	/* is any association in key exchange handshake phase */
	else if ((keycfg = wlc_assoc_keying_in_progress(wlc))) {
		WL_INFORM(("wl%d: scan mac request (%d) blocked for key exchange\n",
		           wlc->pub->unit, req));
		err = BCME_BUSY;
	}
#endif /* BCMSUP_PSK */
#ifdef WLRM
	else if (WLRM_ENAB(wlc->pub) && wlc_rminprog(wlc)) {
		WL_INFORM(("wl%d: scan request while radio measurement is in progress,"
		           " aborting measurement\n",
		           wlc->pub->unit));
		wlc_rm_stop(wlc);
	}
#endif /* WLRM */
#ifdef WL11K
#if defined(WL_PROXDETECT) && defined(WL_FTM_11K)
	else if (wlc_rrm_inprog(wlc) &&
		(!PROXD_ENAB(wlc->pub) || !wlc_rrm_ftm_inprog(wlc))) {
#else
	else if (wlc_rrm_inprog(wlc)) {
#endif // endif
		WL_INFORM(("wl%d: scan request while radio measurement is in progress,"
			" aborting RRM\n",
			wlc->pub->unit));
		wlc_rrm_stop(wlc);
	}
#endif /* WL11K */
#endif /* STA */
	else if (req == WLC_ACTION_ACTFRAME) {
		if (cfg != NULL && (BSS_QUIET_STATE(wlc->quiet, cfg) & SILENCE)) {
			WL_INFORM(("wl%d: AF request blocked for 802.11h Quiet Period\n",
			           wlc->pub->unit));
			err = BCME_EPERM;
		} else if (ANY_SCAN_IN_PROGRESS(wlc->scan)) {
			WL_INFORM(("aborting scan due to AF tx request\n"));
			wlc_scan_abort(wlc->scan, WLC_E_STATUS_NEWSCAN);
		}
	} else if (req == WLC_ACTION_ISCAN || req == WLC_ACTION_ESCAN) {
		if (ISCAN_IN_PROGRESS(wlc) || ESCAN_IN_PROGRESS(wlc->scan)) {
			/* i|e scans preempt in-progress i|e scans */
			WL_INFORM(("escan/iscan aborting escan/iscan\n"));
			wlc_scan_abort(wlc->scan, WLC_E_STATUS_NEWSCAN);
		}
#ifdef WLPFN
		else if (WLPFN_ENAB(wlc->pub) && !GSCAN_ENAB(wlc->pub) &&
		         wl_pfn_scan_in_progress(wlc->pfn)) {
			/* iscan/escan also preempts PNO scan */
			WL_INFORM(("escan/iscan request %d while PNO scan in progress, "
				   "aborting PNO scan\n", req));
			wlc_scan_abort(wlc->scan, WLC_E_STATUS_NEWSCAN);
		}
#endif /* WLPFN */
		else if (ANY_SCAN_IN_PROGRESS(wlc->scan)) {
			/* other scans have precedence over e/iscans */
			WL_INFORM(("escan/iscan blocked due to another kind of scan\n"));
			err = BCME_NOTREADY;
		}
	}
#ifdef WLPFN
	else if (WLPFN_ENAB(wlc->pub) && req == WLC_ACTION_PNOSCAN) {
		if (wl_pfn_scan_in_progress(wlc->pfn)) {
			WL_INFORM(("New PNO scan request while PNO scan in progress, "
				   "aborting PNO scan in progress\n"));
			wlc_scan_abort(wlc->scan, WLC_E_STATUS_NEWSCAN);
		} else if (ANY_SCAN_IN_PROGRESS(wlc->scan)) {
			/* other scans have precedence over PNO scan */
			WL_INFORM(("PNO scan blocked due to another kind of scan\n"));
			err = BCME_NOTREADY;
		}
	}
#endif /* WLPFN */
	else if (req == WLC_ACTION_SCAN) {
		if (ISCAN_IN_PROGRESS(wlc) || ESCAN_IN_PROGRESS(wlc->scan)) {
			WL_INFORM(("escan/iscan aborting due to another kind of scan request\n"));
			wlc_scan_abort(wlc->scan, WLC_E_STATUS_NEWSCAN);
		}
#ifdef WLPFN
		else if (WLPFN_ENAB(wlc->pub) &&
		    wl_pfn_scan_in_progress(wlc->pfn)) {
			WL_INFORM(("Aborting PNO scan due to scan request\n"));
			wlc_scan_abort(wlc->scan, WLC_E_STATUS_NEWSCAN);
		}
#endif // endif
		else if (ANY_SCAN_IN_PROGRESS(wlc->scan)) {
			WL_INFORM(("wl%d: scan request blocked for scan in progress\n",
			           wlc->pub->unit));
			err = BCME_NOTREADY;
		}
	}

	return err;
} /* wlc_mac_request_scan */

#ifdef STA
#ifdef WL11H
/** Quiet request */
/* XXX
 * is it BSS specific or channel specific? how should we handle multiple BSS
 * if it is channel specific?
 */
static int
wlc_mac_request_quiet(wlc_info_t *wlc, wlc_bsscfg_t *cfg, int req)
{
	wlc_assoc_t *as;
	int err = BCME_OK;
	BCM_REFERENCE(req);
	ASSERT(cfg != NULL);

	as = cfg->assoc;
	ASSERT(as != NULL);

	if (as->state != AS_IDLE) {
		if (as->type == AS_ASSOCIATION) {
			WL_INFORM(("wl%d: should not be attempting to enter Quiet Period "
			           "while an association is in progress, blocking Quiet\n",
			           wlc->pub->unit));
			err = BCME_ERROR;
			ASSERT(0);
		} else if (as->type == AS_ROAM) {
			WL_INFORM(("wl%d: Quiet Period starting while roam is in progress, "
			           "aborting roam\n", wlc->pub->unit));
			wlc_assoc_abort(cfg);
		}
	} else if (ANY_SCAN_IN_PROGRESS(wlc->scan)) {
		WL_INFORM(("wl%d: Quiet Period starting while scan is in progress, "
		           "aborting scan\n", wlc->pub->unit));
		wlc_scan_abort(wlc->scan, WLC_E_STATUS_11HQUIET);
	} else if (!cfg->associated) {
		WL_ASSOC_ERROR(("wl%d: should not be attempting to enter Quiet Period "
		          "if not associated, blocking Quiet\n",
		          wlc->pub->unit));
		err = BCME_ERROR;
	}

	return err;
} /* wlc_mac_request_quiet */
#endif /* WL11H */

/**
 * reassociation request
 * reassoc has lower priority than association, scan (af, scan);
 * has higher priority than roam, rm, and excursion
 */
static int
wlc_mac_request_reassoc(wlc_info_t *wlc, wlc_bsscfg_t *cfg, int req)
{
	wlc_assoc_t *as;
	int err = BCME_OK;
	BCM_REFERENCE(req);

	ASSERT(cfg != NULL);

	as = cfg->assoc;
	ASSERT(as != NULL);

	/* bail out if we are in quiet period */
	if ((BSS_QUIET_STATE(wlc->quiet, cfg) & SILENCE)) {
		WL_INFORM(("wl%d: reassoc request blocked for 802.11h Quiet Period\n",
		           wlc->pub->unit));
		err = BCME_EPERM;
	} else if (AS_IN_PROGRESS(wlc)) {
		wlc_bsscfg_t *bc = wlc->as->cmn->assoc_req[0];

		ASSERT(bc != NULL);

		as = bc->assoc;
		ASSERT(as != NULL);

		/* abort any roam in progress */
		if (as->state != AS_IDLE && as->type == AS_ROAM) {
			WL_INFORM(("wl%d: reassoc request while roam is in progress, "
			           "aborting roam\n", wlc->pub->unit));
			wlc_assoc_abort(bc);
		}
		/* bail out if someone is in the process of association */
		else {
			WL_INFORM(("wl%d: reassoc request while association is in progress",
			           wlc->pub->unit));
			err = BCME_BUSY;
		}
	} else if (ANY_SCAN_IN_PROGRESS(wlc->scan)) {
		WL_INFORM(("wl%d: reassoc request while scan is in progress",
		           wlc->pub->unit));
		err = BCME_BUSY;
	}
#ifdef WLRM
	else if (WLRM_ENAB(wlc->pub) && wlc_rminprog(wlc)) {
		WL_INFORM(("wl%d: reassoc request while radio measurement is in progress,"
		           " aborting measurement\n", wlc->pub->unit));
		wlc_rm_stop(wlc);
	}
#endif /* WLRM */

	return err;
} /* wlc_mac_request_reassoc */
#endif /* STA */

#if defined(WLRM) || defined(WL11K)
/** RM request has lower priority than association/roam/scan */
static int
wlc_mac_request_rm(wlc_info_t *wlc, wlc_bsscfg_t *cfg, int req)
{
	int err = BCME_OK;

#ifdef STA
	/* is any assoc/roam in progress? */
	if (AS_IN_PROGRESS(wlc)) {
		WL_INFORM(("wl%d: radio measure blocked for association in progress\n",
		           wlc->pub->unit));
		err = BCME_ERROR;
	}
	/* is scan engine busy? */
	else if (ANY_SCAN_IN_PROGRESS(wlc->scan)) {
		WL_INFORM(("wl%d: radio measure blocked for scan in progress\n",
		           wlc->pub->unit));
		err = BCME_ERROR;
	}
#endif /* STA */

	return err;
}
#endif /* WLRM || WL11K */

/*
 * Arbitrate off-home channel activities (assoc, roam, rm, af, excursion, scan, etc.)
 *
 * The priorities of different activities are listed below from high to low:
 *
 * - assoc, recreate
 * - af (action frame)
 * - scan
 * - iscan, escan
 * - reassoc
 * - roam
 * - excursion
 * - rm (radio measurement)
 *
 * Returns BCME_OK if the request can proceed, or a specific BCME_ code if the
 * request is blocked. This routine will make sure any lower priority MAC action
 * is aborted. Return > 0 if request is queued (for association).
 */
/* XXX mSTA: What should be done here to support simultaneous association and roam
 * requests? For now seriallize all requests of the same kind and follow the same
 * priorities as they have been (ASSOC > SCAN > ROAM > RM).
 *
 * Note: Only one roam request is allowed.
 */
int
wlc_mac_request_entry(wlc_info_t *wlc, wlc_bsscfg_t *cfg, int req, uint scan_flags)
{
	int err = BCME_OK;
#ifdef STA
	uint type;
#endif // endif

#if defined(WL_MODESW) && defined(WLRSDB)
	if (RSDB_ENAB(wlc->pub) && WLC_MODESW_ENAB(WLC_RSDB_GET_PRIMARY_WLC(wlc)->pub) &&
	    MODE_SWITCH_IN_PROGRESS(WLC_RSDB_GET_PRIMARY_WLC(wlc)->modesw)) {
		WL_ASSOC_ERROR(("wl%d: req %d blocked due to mode switch in progress\n",
		          wlc->pub->unit, req));
		return BCME_BUSY;
	}
#endif /* WL_MODESW && WLRSDB */

	switch (req) {
#ifdef STA
	case WLC_ACTION_RECREATE_ROAM:
		/* Roam as part of an assoc_recreate process.
		 * Happens when the former AP was not found and we need to look for
		 * another AP supporting the same SSID.
		 */
		ASSERT(cfg != NULL);
		ASSERT(cfg->assoc != NULL);
		ASSERT(cfg->assoc->type == AS_RECREATE);
		ASSERT(cfg->assoc->state == AS_RECREATE_WAIT_RCV_BCN);
		/* FALLSTHRU */
	case WLC_ACTION_ASSOC:
	case WLC_ACTION_RECREATE:
		err = wlc_mac_request_assoc(wlc, cfg, req);
		break;

	case WLC_ACTION_ROAM:
		err = wlc_mac_request_roam(wlc, cfg, req);
		break;

#if defined(WL11H)
	case WLC_ACTION_QUIET:
		err = wlc_mac_request_quiet(wlc, cfg, req);
		break;
#endif /* WL11H */

	case WLC_ACTION_REASSOC:
		err = wlc_mac_request_reassoc(wlc, cfg, req);
		break;
#endif /* STA */

#if defined(WLRM) || defined(WL11K)
	case WLC_ACTION_RM:
		err = wlc_mac_request_rm(wlc, cfg, req);
		break;
#endif /* WLRM || WL11K */

	case WLC_ACTION_SCAN:
	case WLC_ACTION_ISCAN:
	case WLC_ACTION_ESCAN:
	case WLC_ACTION_PNOSCAN:
	case WLC_ACTION_ACTFRAME:
		err = wlc_mac_request_scan(wlc, cfg, req, scan_flags);
		break;

	default:
		err = BCME_ERROR;
		ASSERT(0);
	}

#ifdef STA
	if (err != BCME_OK) {
		return err;
	}

	/* TODO: pass assoc type along with mac req so that we can eliminate the switch statement.
	 */

	/* Note: at this point for association/join, SSID is not configured... */

	/* we are granted MAC access! */
	switch (req) {
	case WLC_ACTION_ASSOC:
		type = AS_ASSOCIATION;
		goto request;
	case WLC_ACTION_RECREATE_ROAM:
	case WLC_ACTION_RECREATE:
		type = AS_RECREATE;
		goto request;
	case WLC_ACTION_ROAM:
	case WLC_ACTION_REASSOC:
		type = AS_ROAM;
		/* FALLSTHRU */
	request:
		ASSERT(type != AS_NONE);
		ASSERT(cfg != NULL);

		/* add the request into assoc req list */
		if ((err = wlc_assoc_req_add_entry(wlc, cfg, type)) < 0) {
#if defined(BCMDBG) || defined(WLMSG_INFORM)
			char ssidbuf[SSID_FMT_BUF_LEN];
			wlc_format_ssid(ssidbuf, cfg->SSID, cfg->SSID_len);
			WL_INFORM(("wl%d.%d: %s request not granted for SSID %s\n",
			           wlc->pub->unit, WLC_BSSCFG_IDX(cfg),
			           WLCASTYPEN(type), ssidbuf));
#endif // endif
		}
		/* inform the caller of the request has been queued at index 'err'
		 * when err > 0...
		 */
	}
#endif /* STA */

	return err;
} /* wlc_mac_request_entry */

#ifdef WLPFN
void wl_notify_pfn(wlc_info_t *wlc)
{
	if (AS_IN_PROGRESS(wlc)) {
		wlc_assoc_t *as;
		wlc_bsscfg_t *bc = wlc->as->cmn->assoc_req[0];

		ASSERT(bc != NULL);
		as = bc->assoc;
		ASSERT(as != NULL);
		if (as->state != AS_IDLE)
			return;
	}

	wl_pfn_inform_mac_availability(wlc);
} /* wl_notify_pfn */
#endif /* WLPFN */

/** get roam default parameters */
void
BCMATTACHFN(wlc_roam_defaults)(wlc_info_t *wlc, wlcband_t *band, int *roam_trigger,
	uint *roam_delta)
{
	/* set default roam parameters */
	if (band->radioid == NORADIO_ID) {
		*roam_trigger = WLC_NEVER_ROAM_TRIGGER;
		*roam_delta = WLC_NEVER_ROAM_DELTA;
	} else {
		*roam_trigger = BAND_5G(band->bandtype) ? WLC_5G_ROAM_TRIGGER :
		        WLC_2G_ROAM_TRIGGER;
		*roam_delta = BAND_5G(band->bandtype) ? WLC_5G_ROAM_DELTA :
		        WLC_2G_ROAM_DELTA;
		WL_INFORM(("wl%d: wlc_roam_defaults: USE GENERIC ROAM THRESHOLD "
		           "(%d %d) FOR RADIO %04x IN BAND %s\n", wlc->pub->unit,
		           *roam_trigger, *roam_delta, band->radioid,
		           BAND_5G(band->bandtype) ? "5G" : "2G"));
	}

	/* Fill up the default roam profile */
	if (band->roam_prof) {
		memset(band->roam_prof, 0, WL_MAX_ROAM_PROF_BRACKETS * sizeof(wl_roam_prof_t));
		band->roam_prof[0].roam_flags = WL_ROAM_PROF_DEFAULT;
		band->roam_prof[0].roam_trigger = (int8)(*roam_trigger);
		band->roam_prof[0].rssi_lower = WLC_RSSI_MINVAL_INT8;
		band->roam_prof[0].roam_delta = (int8)(*roam_delta);
		band->roam_prof[0].rssi_boost_thresh = WLC_JOIN_PREF_RSSI_BOOST_MIN;
		band->roam_prof[0].rssi_boost_delta = 0;
		band->roam_prof[0].nfscan = ROAM_FULLSCAN_NTIMES;
		band->roam_prof[0].fullscan_period = WLC_FULLROAM_PERIOD;
		band->roam_prof[0].init_scan_period = WLC_ROAM_SCAN_PERIOD;
		band->roam_prof[0].backoff_multiplier = 1;
		band->roam_prof[0].max_scan_period = WLC_ROAM_SCAN_PERIOD;
		band->roam_prof_max_idx = 1;

		if (WBTEXT_ENAB(wlc->pub)) {
			band->roam_prof[0].channel_usage = WL_CU_PERCENTAGE_DISABLE;
			band->roam_prof[0].cu_avg_calc_dur = WL_CU_CALC_DURATION_DEFAULT;

			if (WL_MAX_ROAM_PROF_BRACKETS > 1) {
				/* make roam_prof[0] - default wbtext roam profile */
				memcpy(&band->roam_prof[1], &band->roam_prof[0],
					sizeof(*band->roam_prof));

				/* update roam_prof[1], RSSI and CU
				 * ex:-
				 *  roam_prof[0]: RSSI[-60,-75] CU(trigger:70% duration:10s)
				 *  roam_prof[1]: RSSI[-75,-128] CU(trigger:0% duration:10s)
				 */
				band->roam_prof[0].roam_flags = WL_ROAM_PROF_NONE;
				band->roam_prof[1].roam_flags = WL_ROAM_PROF_NONE;

				/* fix wbtext roam params */
				band->roam_prof[0].roam_trigger = BAND_5G(band->bandtype) ?
					WL_CU_5G_ROAM_TRIGGER : WL_CU_2G_ROAM_TRIGGER;
				band->roam_prof[0].rssi_lower = (int8)(*roam_trigger);
				band->roam_prof[0].roam_delta = WL_CU_SCORE_DELTA_DEFAULT;
				band->roam_prof[0].channel_usage = WL_CU_PERCENTAGE_DEFAULT;
				band->roam_prof[0].cu_avg_calc_dur = WL_CU_CALC_DURATION_DEFAULT;
				band->roam_prof_max_idx++;
			}
		}
	}
} /* wlc_roam_defaults */

void
wlc_deauth_complete(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint status,
	const struct ether_addr *addr, uint deauth_reason, uint bss_type)
{
#if defined(STA) && defined(WLRM)
	if (WLRM_ENAB(wlc->pub) && (bsscfg && BSSCFG_STA(bsscfg)) && wlc_rminprog(wlc)) {
		WL_INFORM(("wl%d: abort RM due to deauth\n", WLCWLUNIT(wlc)));
		wlc_rm_stop(wlc);
	}
#endif /* STA && WLRM */

#if defined(STA) && defined(WL11K)
	if ((bsscfg && BSSCFG_STA(bsscfg)) && wlc_rrm_inprog(wlc)) {
		WL_INFORM(("wl%d: abort RRM due to deauth\n", WLCWLUNIT(wlc)));
		wlc_rrm_stop(wlc);
	}
#endif /* STA && WL11K */

	/* notifying interested parties of the event... */
	wlc_bss_disassoc_notif(wlc, bsscfg, DAN_TYPE_LOCAL_DEAUTH, DAN_ST_DEAUTH_CMPLT, addr);

	wlc_bss_mac_event(wlc, bsscfg, WLC_E_DEAUTH, addr, status, deauth_reason,
		bss_type, 0, 0);
}

void
wlc_send_deauth(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct scb *scb,
	struct ether_addr *da, struct ether_addr *bssid,
	struct ether_addr *sa, uint16 reason_code)
{
	void *pkt;
	ASSERT(scb);

	if (scb->sent_deauth) {
		WL_ERROR(("wl%d: %s: already sent deauth for scb "MACF"\n",
			wlc->pub->unit, __FUNCTION__, ETHERP_TO_MACF(&scb->ea)));
		return;
	}

	pkt = wlc_senddeauth(wlc, cfg, scb, &scb->ea, &cfg->BSSID, &cfg->cur_etheraddr,
		reason_code);
	if (pkt != NULL) {
		wlc_deauth_send_cbargs_t *args;

		args = MALLOCZ(wlc->osh, sizeof(wlc_deauth_send_cbargs_t));
		if (args == NULL) {
			WL_ERROR((WLC_BSS_MALLOC_ERR, WLCWLUNIT(wlc),
				WLC_BSSCFG_IDX(cfg), __FUNCTION__,
				(int)sizeof(wlc_deauth_send_cbargs_t),
				MALLOCED(wlc->osh)));
			return;
		}
		bcopy(&scb->ea, &args->ea, sizeof(struct ether_addr));
		args->_idx = WLC_BSSCFG_IDX(cfg);
		args->pkt = pkt;
		if (wlc_pcb_fn_register(wlc->pcb,
			wlc_deauth_sendcomplete, (void *)args, pkt)) {
			WL_ERROR(("wl%d: wlc_scb_set_auth: could not "
			          "register callback\n", wlc->pub->unit));
			MFREE(wlc->osh, args, sizeof(wlc_deauth_send_cbargs_t));
		}
		else {
			scb->sent_deauth = args;
		}
	}
}

static void
wlc_deauth_sendcomplete(wlc_info_t *wlc, uint txstatus, void *arg)
{
	struct scb *scb;
	wlc_bsscfg_t *bsscfg;
	wlc_deauth_send_cbargs_t *cbarg = arg;

	BCM_REFERENCE(txstatus);

	/* Is this scb still around */
	bsscfg = WLC_BSSCFG(wlc, cbarg->_idx);
	if ((bsscfg == NULL) || ((scb = wlc_scbfind(wlc, bsscfg, &cbarg->ea)) == NULL)) {
		/* do not free arg, let SCB cleanup do that */
		return;
	}

#ifdef AP
	/* Reset PS state if needed */
	if (SCB_PS(scb))
		wlc_apps_scb_ps_off(wlc, scb, TRUE);
#endif /* AP */

	if (scb->sent_deauth != NULL) {
		ASSERT(scb->sent_deauth == cbarg);
	} else {
		WL_ERROR(("wl%d: %s: deauth complete for scb "MACF", but struct freed (%p)\n",
			wlc->pub->unit, __FUNCTION__, ETHERP_TO_MACF(&scb->ea), cbarg));
		/* this shouldn't happen but will allow for TXQ cleanup @ SCB free */
		ASSERT(SCB_DEL_IN_PROGRESS(scb)); /* SCB being removed */
		return;
	}

	WL_ASSOC(("wl%d: %s: deauth complete\n", wlc->pub->unit, __FUNCTION__));
	wlc_bss_mac_event(wlc, SCB_BSSCFG(scb), WLC_E_DEAUTH, (struct ether_addr *)arg,
	              WLC_E_STATUS_SUCCESS, DOT11_RC_DEAUTH_LEAVING, 0, 0, 0);

	if (cbarg->pkt) {
		WLPKTTAGSCBSET(cbarg->pkt, NULL);
	}
	scb->sent_deauth = NULL;

	MFREE(wlc->osh, arg, sizeof(wlc_deauth_send_cbargs_t));

	if (BSSCFG_STA(bsscfg)) {
		/* Do this last: ea_arg points inside scb */
		wlc_scbfree(wlc, scb);
	} else {
		/* Clear states and mark the scb for deletion. SCB free will happen
		 * from the inactivity timeout context in wlc_ap_stastimeout()
		 * Mark the scb for deletion first as some scb state change notify callback
		 * functions need to be informed that the scb is about to be deleted.
		 * (For example wlc_cfp_scb_state_upd)
		 */
		SCB_MARK_DEL(scb);
		wlc_scb_clearstatebit(wlc, scb, AUTHENTICATED | ASSOCIATED | AUTHORIZED);

	}
} /* wlc_deauth_sendcomplete */

void
wlc_deauth_sendcomplete_clean(wlc_info_t *wlc, struct scb *scb)
{
	ASSERT(scb);
	if (scb->sent_deauth != NULL) {
		/* strip all callbacks */
		int matches = wlc_pcb_fn_find(wlc->pcb, wlc_deauth_sendcomplete, scb->sent_deauth,
			TRUE);
		ASSERT(matches <= 1);
		BCM_REFERENCE(matches);
		/* free memory */
		MFREE(wlc->osh, scb->sent_deauth, sizeof(wlc_deauth_send_cbargs_t));
		/* reset pointer */
		scb->sent_deauth = NULL;
	}
}

void
wlc_disassoc_ind_complete(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint status,
	struct ether_addr *addr, uint disassoc_reason, uint bss_type,
	uint8 *body, int body_len)
{
#if defined(STA) && defined(WLRM)
	if (WLRM_ENAB(wlc->pub) && wlc_rminprog(wlc)) {
		WL_INFORM(("wl%d: abort RM due to receiving disassoc request\n",
		           WLCWLUNIT(wlc)));
		wlc_rm_stop(wlc);
	}
#endif /* STA && WLRM */

#if defined(STA) && defined(WL11K)
	if (wlc_rrm_inprog(wlc)) {
		WL_INFORM(("wl%d: abort RRM due to receiving disassoc request\n",
		           WLCWLUNIT(wlc)));
		wlc_rrm_stop(wlc);
	}
#endif /* STA && WL11K */

#ifdef AP
	if (BSSCFG_AP(bsscfg)) {

		wlc_btc_set_ps_protection(wlc, bsscfg); /* disable */

#ifdef WLP2P
		/* reenable P2P in case a non-P2P STA leaves the BSS and
		 * all other associated STAs are P2P client
		 */
		if (BSS_P2P_ENAB(wlc, bsscfg))
			wlc_p2p_enab_upd(wlc->p2p, bsscfg);
#endif // endif

		if (DYNBCN_ENAB(bsscfg) && wlc_bss_assocscb_getcnt(wlc, bsscfg) == 0)
			wlc_bsscfg_bcn_disable(wlc, bsscfg);
	}
#endif /* AP */

	wlc_bss_mac_event(wlc, bsscfg, WLC_E_DISASSOC_IND, addr, status, disassoc_reason,
	                  bss_type, body, body_len);

} /* wlc_disassoc_ind_complete */

void
wlc_deauth_ind_complete(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint status,
	struct ether_addr *addr, uint deauth_reason, uint bss_type,
	uint8 *body, int body_len)
{
#if defined(STA) && defined(WLRM)
	if (WLRM_ENAB(wlc->pub) && wlc_rminprog(wlc)) {
		WL_INFORM(("wl%d: abort RM due to receiving deauth request\n",
		           WLCWLUNIT(wlc)));
		wlc_rm_stop(wlc);
	}
#endif /* STA && WLRM */

#if defined(STA) && defined(WL11K)
	if (wlc_rrm_inprog(wlc)) {
		WL_INFORM(("wl%d: abort RRM due to receiving deauth request\n",
		           WLCWLUNIT(wlc)));
		wlc_rrm_stop(wlc);
	}
#endif /* STA && WL11K */

#ifdef AP
	if (BSSCFG_AP(bsscfg)) {
		wlc_btc_set_ps_protection(wlc, bsscfg); /* disable */
#ifdef WLP2P
		/* reenable P2P in case a non-P2P STA leaves the BSS and
		 * all other associated STAs are P2P client
		 */
		if (BSS_P2P_ENAB(wlc, bsscfg))
			wlc_p2p_enab_upd(wlc->p2p, bsscfg);
#endif // endif
	}
#endif /* AP */

	wlc_bss_mac_event(wlc, bsscfg, WLC_E_DEAUTH_IND, addr, status, deauth_reason,
	                  bss_type, body, body_len);

} /* wlc_deauth_ind_complete */

void
wlc_assoc_bcn_mon_off(wlc_bsscfg_t *cfg, bool off, uint user)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_cxn_t *cxn = cfg->cxn;

	(void)wlc;

	WL_ASSOC(("wl%d.%d: %s: off %d user 0x%x\n",
	          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__, off, user));

	if (off)
		mboolset(cxn->ign_bcn_lost_det, user);
	else
		mboolclr(cxn->ign_bcn_lost_det, user);
}

#ifdef STA
int
wlc_assoc_iswpaenab(wlc_bsscfg_t *cfg, bool wpa)
{
	int ret = 0;

	if WLCAUTOWPA(cfg)
		ret = TRUE;
	else if (wpa) {
		if (cfg->WPA_auth != WPA_AUTH_DISABLED)
			ret = TRUE;
	} else if (cfg->WPA_auth & WPA2_AUTH_UNSPECIFIED)
		ret = TRUE;

	return ret;
}

static void
wlc_roam_timer_expiry(void *arg)
{
	wlc_bsscfg_t *cfg = (wlc_bsscfg_t *)arg;

	cfg->roam->timer_active = FALSE;
	/* A beacon was received since uatbtt */
	if (cfg->roam->time_since_bcn * 1000u < ((cfg->current_bss->beacon_period <<
		10) / 1000u) * UATBTT_TO_ROAM_BCN)
		return;

	if (cfg->pm->check_for_unaligned_tbtt) {
		WL_ASSOC(("wl%d: ROAM: done for unaligned TBTT, "
			"time_since_bcn %d Sec\n",
			WLCWLUNIT(cfg->wlc), cfg->roam->time_since_bcn));
		wlc_set_uatbtt(cfg, FALSE);
	}
	wlc_roam_bcns_lost(cfg);
}

#ifdef WLABT

static void
wlc_check_adaptive_bcn_timeout(wlc_bsscfg_t *cfg)
{
	wlc_roam_t *roam = cfg->roam;

	if (roam->prev_bcn_timeout != 0) {
		/* adaptive bcn_timeout is applied */
		bool is_high_rssi = cfg->link->rssi > ABT_HIGH_RSSI;
		if (is_high_rssi || P2P_ACTIVE(cfg->wlc)) {
			roam->bcn_timeout = roam->prev_bcn_timeout;
			roam->prev_bcn_timeout = 0;
			WL_ASSOC(("wl%d: Restore bcn_timeout to %d, RSSI %d, P2P %d\n",
				cfg->wlc->pub->unit, roam->bcn_timeout,
				cfg->link->rssi, P2P_ACTIVE(cfg->wlc)));
		}
	} else {
		/* adaptive bcn_timeout is NOT applied, so check the status */
		wlcband_t *band = cfg->wlc->band;
		bool is_low_trigger = band->roam_trigger ==
			(WLC_AUTO_ROAM_TRIGGER - WLC_ROAM_TRIGGER_STEP);
		bool is_low_rssi = cfg->link->rssi < WLC_AUTO_ROAM_TRIGGER;
		if (is_low_trigger && is_low_rssi && !P2P_ACTIVE(cfg->wlc) &&
			(roam->bcn_timeout > ABT_MIN_TIMEOUT)) {
			roam->prev_bcn_timeout = roam->bcn_timeout;
			roam->bcn_timeout = ABT_MIN_TIMEOUT;
			WL_ASSOC(("wl%d: Setting bcn_timeout to %d, RSSI %d, P2P %d\n",
				cfg->wlc->pub->unit, roam->bcn_timeout,
				cfg->link->rssi, P2P_ACTIVE(cfg->wlc)));
		}
	}
}
#endif /* WLABT */

static
void wlc_roam_period_update(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	if (wlc->band->roam_prof) {
		wlc_roam_t *roam = cfg->roam;
		int idx = roam->roam_prof_idx;
		uint prev_partialscan_period = roam->partialscan_period;
		wl_roam_prof_t *roam_prof = &wlc->band->roam_prof[idx];

		ASSERT((idx >= 0) && (idx < WL_MAX_ROAM_PROF_BRACKETS));

		if (roam_prof->max_scan_period && roam_prof->backoff_multiplier) {
			roam->partialscan_period *= roam_prof->backoff_multiplier;
			roam->partialscan_period =
				MIN(roam->partialscan_period, roam_prof->max_scan_period);
			roam->scan_block += roam->partialscan_period - prev_partialscan_period;
		}
	}
}

/** Return TRUE if current roam scan is background roam scan */
bool wlc_roam_scan_islazy(wlc_info_t *wlc, wlc_bsscfg_t *cfg, bool roam_scan_isactive)
{
	wlcband_t *band = wlc->band;

	if (NBANDS(wlc) > 1) {
		band = wlc->bandstate[CHSPEC_IS2G(cfg->current_bss->chanspec) ?
			BAND_2G_INDEX : BAND_5G_INDEX];
	}

	/* Check for active roam scan when requested */
	if (roam_scan_isactive) {
		if (!AS_IN_PROGRESS(wlc) || !SCAN_IN_PROGRESS(wlc->scan))
			return FALSE;

		if ((cfg->assoc->type != AS_ROAM) || (cfg->roam->reason != WLC_E_REASON_LOW_RSSI))
			return FALSE;
	}

	/* More checks */
	if ((cfg->pm->PM != PM_FAST) || !cfg->associated || !band->roam_prof)
		return FALSE;

	return (band->roam_prof[cfg->roam->roam_prof_idx].roam_flags & WL_ROAM_PROF_LAZY);
}

static bool wlc_is_critical_roam(wlc_bsscfg_t *cfg)
{
	wlc_roam_t *roam;

	ASSERT(cfg != NULL);
	roam = cfg->roam;
	return (roam->reason == WLC_E_REASON_DEAUTH ||
	        roam->reason == WLC_E_REASON_DISASSOC ||
	        roam->reason == WLC_E_REASON_BCNS_LOST ||
	        roam->reason == WLC_E_REASON_TXFAIL);
}

/** Assume already checked that this is background roam scan */
bool wlc_lazy_roam_scan_suspend(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	wlcband_t *band = wlc->band;

	if (NBANDS(wlc) > 1) {
		band = wlc->bandstate[CHSPEC_IS2G(cfg->current_bss->chanspec) ?
			BAND_2G_INDEX : BAND_5G_INDEX];
	}

	ASSERT(wlc_roam_scan_islazy(wlc, cfg, FALSE));
	return (band->roam_prof[cfg->roam->roam_prof_idx].roam_flags & WL_ROAM_PROF_SUSPEND);
}

/** Assume already checked that this is background roam scan */
bool wlc_lazy_roam_scan_sync_dtim(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	wlcband_t *band = wlc->band;

	if (NBANDS(wlc) > 1) {
		band = wlc->bandstate[CHSPEC_IS2G(cfg->current_bss->chanspec) ?
			BAND_2G_INDEX : BAND_5G_INDEX];
	}

	ASSERT(wlc_roam_scan_islazy(wlc, cfg, FALSE));
	return (band->roam_prof[cfg->roam->roam_prof_idx].roam_flags & WL_ROAM_PROF_SYNC_DTIM);
}

static void wlc_roam_prof_update(wlc_info_t *wlc, wlc_bsscfg_t *cfg, bool reset)
{
	wlc_roam_t *roam = cfg->roam;
	wlcband_t *band = wlc->band;

	if (wlc->band->roam_prof) {
		/* Full multiple roam profile support */
		int16 idx = roam->roam_prof_idx;
		int rssi = cfg->link->rssi;
		wl_roam_prof_t *roam_prof;
		wlcband_t *otherband = NULL;
		int new_scan_block;

		/* The current band/channel may not be the same as the current BSS */
		if (NBANDS(wlc) > 1) {
			band = wlc->bandstate[CHSPEC_IS2G(cfg->current_bss->chanspec) ?
				BAND_2G_INDEX : BAND_5G_INDEX];
			otherband = wlc->bandstate[CHSPEC_IS2G(cfg->current_bss->chanspec) ?
				BAND_5G_INDEX : BAND_2G_INDEX];
		}

		ASSERT((idx >= 0) && (idx < WL_MAX_ROAM_PROF_BRACKETS));
		ASSERT((rssi >= WLC_RSSI_MINVAL_INT8) && (rssi <= WLC_RSSI_INVALID));

		/* Check and update roam profile according to RSSI */
		if (reset) {
			idx = wlc_roamprof_get_min_idx(wlc, band->bandtype);
		} else {
			if (!WBTEXT_ACTIVE(wlc->pub) &&
				wlc_roamprof_is_cu_enabled(wlc, band->bandtype, idx)) {
				/* update roam profile idx if, cu enabled and wbtext not active */
				idx = wlc_roamprof_get_min_idx(wlc, band->bandtype);
			}
			else
			/* XXX: roam_prof[idx].roam_trigger and band->roam_trigger are not same -
			 *      when fw internally changes roam_trigger (ccx and ap_env)
			 * compare rssi with roam_prof[idx].roam_trigger for profile index change
			 */
			if ((rssi >= (band->roam_prof[idx].roam_trigger +
					wlc->roam_rssi_cancel_hysteresis)) &&
				(idx > wlc_roamprof_get_min_idx(wlc, band->bandtype)))
				idx--;
			else if ((band->roam_prof[idx].rssi_lower != WLC_RSSI_MINVAL_INT8) &&
			         (rssi < band->roam_prof[idx].rssi_lower) &&
			         (idx < (WL_MAX_ROAM_PROF_BRACKETS - 1)))
				idx++;
			else
				return;
		}

		roam->roam_prof_idx = idx;
		roam_prof = &band->roam_prof[idx];

		/* Update roam parameters when switching roam profile */
		if (roam_prof->roam_flags & WL_ROAM_PROF_NO_CI)
			roam->ci_delta |= ROAM_CACHEINVALIDATE_DELTA_MAX;
		else
			roam->ci_delta &= (ROAM_CACHEINVALIDATE_DELTA_MAX - 1);

		if (!WLFMC_ENAB(wlc->pub) || (roam->ap_environment != AP_ENV_SPARSE) ||
			(band->roam_trigger != (WLC_AUTO_ROAM_TRIGGER -
			WLC_ROAM_TRIGGER_STEP))) {
			band->roam_trigger = roam_prof->roam_trigger;
		}

		band->roam_delta = roam_prof->roam_delta;
		if (NBANDS(wlc) > 1) {
			roam->roam_rssi_boost_thresh = roam_prof->rssi_boost_thresh;
			roam->roam_rssi_boost_delta = roam_prof->rssi_boost_delta;
		}

		/* Adjusting scan_block taking into account of passed blocking time */
		new_scan_block = (int)roam_prof->init_scan_period -
			(int)(roam->partialscan_period - roam->scan_block);

		roam->nfullscans = (uint8)roam_prof->nfscan;
		roam->fullscan_period = roam_prof->fullscan_period;
		roam->partialscan_period = roam_prof->init_scan_period;

#ifdef WLWNM
		if (WBTEXT_ENAB(wlc->pub)) {
			uint8 channel_usage;
			channel_usage = roam_prof->channel_usage;
			if ((roam->ap_environment == AP_ENV_SPARSE) &&
				roam_prof->channel_usage) {
				/* home environment: update cu trigger */
				channel_usage += WLC_CU_TRIGGER_STEP;
			}

			wlc_wnm_set_cu_trigger_percent(wlc, cfg, channel_usage);
			wlc_wnm_set_cu_avg_calc_dur(wlc, cfg, roam_prof->cu_avg_calc_dur);

			if (roam_prof->channel_usage) {
				/* restore default, since the roam profile entry is cu enabled */
				band->roam_delta = band->roam_delta_def;
			}

			/* reset wbtext counters */
			wlc_wnm_bssload_calc_reset(cfg);
		}
#endif /* WLWNM */

		/* Minimum delay enforced upon new association & new profile configuration */
		roam->scan_block = (uint)((new_scan_block > 0) ? new_scan_block : 0);
		if (reset)
			roam->scan_block =
				MAX(roam->scan_block, wlc->pub->tunables->scan_settle_time);

		/* Basic update to other bands upon reset: This is not really useful.
		 * It just keeps legacy roam trigger/delta read command in-sync.
		 */
		if ((NBANDS(wlc) > 1) && reset) {
			if (otherband->roam_prof) {
				if (!WLFMC_ENAB(wlc->pub) ||
						(roam->ap_environment != AP_ENV_SPARSE) ||
						(otherband->roam_trigger !=
						(WLC_AUTO_ROAM_TRIGGER -
						WLC_ROAM_TRIGGER_STEP))) {
					otherband->roam_trigger =
						otherband->roam_prof[idx].roam_trigger;
				}
				otherband->roam_delta =
					otherband->roam_prof[idx].roam_delta;
			}
		}

		WL_SRSCAN(("ROAM prof[%d:%d]: trigger=%d delta=%d\n",
			band->bandtype, idx, band->roam_trigger, band->roam_delta));
		WL_ASSOC(("ROAM prof[%d:%d]: trigger=%d delta=%d\n",
			band->bandtype, idx, band->roam_trigger, band->roam_delta));
	}
} /* wlc_roam_prof_update */

/** This is to maintain the backward compatibility */
static void wlc_roam_prof_update_default(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	wlc_roam_t *roam = cfg->roam;
	wlcband_t *band;
	uint i;

	/* NULL check, roam is not required for AP, TDLS and p2p_disc */
	if (roam == NULL) {
		return;
	}

	if (WLFMC_ENAB(wlc->pub) && (roam->ap_environment == AP_ENV_SPARSE) &&
			cfg->associated) {
		/* Don't update to default when ap environment is
		 * sparse network & cfg is associated
		 */
		return;
	}

	for (i = 0; i < NBANDS(wlc); i++) {
		band = wlc->bandstate[i];

		/* Only for no roam profile support and single default profile */
		if (band->roam_prof && !(band->roam_prof->roam_flags & WL_ROAM_PROF_DEFAULT)) {
			WL_ASSOC_ERROR(("wl%d.%d: %s: Only for no roam support\n", WLCWLUNIT(wlc),
				WLC_BSSCFG_IDX(cfg), __FUNCTION__));
			return;
		}
	}

	for (i = 0; i < NBANDS(wlc); i++) {
		band = wlc->bandstate[i];

		/* Simple roam profile depending on multi_ap state:
		 * The multi_ap state is set at the end of association scan,
		 * so it is used here to set up roaming parameters.
		 */
		if (roam->multi_ap && roam->roam_delta_aggr && roam->roam_trigger_aggr) {
			band->roam_trigger = roam->roam_trigger_aggr;
			band->roam_delta = roam->roam_delta_aggr;
		} else {
			/* restore roam parameters to default */
			band->roam_trigger = band->roam_trigger_def;
			band->roam_delta = band->roam_delta_def;
		}

		/* Now update the single default profile for compatibility */
		if (band->roam_prof) {
			band->roam_prof->roam_trigger = (int8)band->roam_trigger;
			band->roam_prof->roam_delta = (int8)band->roam_delta;

			/* Update the following when BSS is known */
			band->roam_prof->nfscan = roam->nfullscans;
			band->roam_prof->fullscan_period = (uint16)roam->fullscan_period;
			band->roam_prof->init_scan_period = (uint16)roam->partialscan_period;
			band->roam_prof->backoff_multiplier = 1;
			band->roam_prof->max_scan_period = (uint16)roam->partialscan_period;
			if (WBTEXT_ENAB(wlc->pub)) {
				band->roam_prof->channel_usage = WL_CU_PERCENTAGE_DISABLE;
				band->roam_prof->cu_avg_calc_dur = WL_CU_CALC_DURATION_DEFAULT;
			}
		}
	}

	WL_SRSCAN(("ROAM prof[multi_ap=%d]: trigger=%d delta=%d\n",
		roam->multi_ap, wlc->band->roam_trigger, wlc->band->roam_delta));
	WL_ASSOC(("ROAM prof[multi_ap=%d]: trigger=%d delta=%d\n",
		roam->multi_ap, wlc->band->roam_trigger, wlc->band->roam_delta));
}

void
wlc_roam_handle_join(wlc_bsscfg_t *cfg)
{
	wlc_roam_t *roam = cfg->roam;

	/* Only clear when STA (re)joins a BSS */
	if (BSSCFG_STA(cfg)) {
		roam->bcn_interval_cnt = 0;
		roam->time_since_bcn = 0;
		roam->bcns_lost = FALSE;
		roam->tsf_l = roam->tsf_h = 0;
		roam->original_reason = WLC_E_REASON_INITIAL_ASSOC;
	}
}

void
wlc_roam_handle_missed_beacons(wlc_bsscfg_t *cfg, uint32 missed_beacons)
{
	wlc_roam_t *roam = cfg->roam;
	wlc_info_t *wlc = cfg->wlc;

#ifdef BCMDBG
	if (cfg->associated) {
		roam->tbtt_since_bcn++;
	}
#endif /* BCMDBG */

	/* if NOT in PS mode, increment beacon "interval" count */
	if (!cfg->pm->PMenabled && roam->bcn_thresh != 0) {

	        if (roam->bcn_interval_cnt < roam->bcn_thresh) {
			roam->bcn_interval_cnt = missed_beacons;
			roam->bcn_interval_cnt = MIN(roam->bcn_interval_cnt,
				roam->bcn_thresh);
		}

		/* issue beacon lost event if appropriate:
		* zero value for thresh means never
		*/
		if (roam->bcn_interval_cnt == roam->bcn_thresh) {

			WL_ASSOC_ERROR(("%s: bcn_interval_cnt 0x%x\n",
				__FUNCTION__, roam->bcn_interval_cnt));
			wlc_mac_event(wlc, WLC_E_BCNLOST_MSG,
				&cfg->prev_BSSID, WLC_E_STATUS_FAIL,
				0, 0, 0, 0);
			roam->bcn_interval_cnt++;
		}
	}
}

void
wlc_roam_handle_beacon_loss(wlc_bsscfg_t *cfg)
{
	wlc_roam_handle_missed_beacons(cfg, cfg->roam->bcn_interval_cnt + 1);
}

static void
wlc_handle_ap_lost(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	wlc_assoc_t *as = cfg->assoc;
	wlc_roam_t *roam = cfg->roam;
	wlc_bss_info_t *current_bss = cfg->current_bss;
	int reason;

	/* If we are in spectrum management mode and this channel is
	 * subject to radar detection rules, or this is a restricted
	 * channel, restore the channel's quiet bit until we
	 * reestablish contact with our AP
	 */
	if ((WL11H_ENAB(wlc) && wlc_radar_chanspec(wlc->cmi, current_bss->chanspec)) ||
	     wlc_restricted_chanspec(wlc->cmi, current_bss->chanspec))
		wlc_set_quiet_chanspec(wlc->cmi, current_bss->chanspec);

	WL_ASSOC(("wl%d: ROAM: time_since_bcn %d > bcn_timeout %d : link down\n",
		WLCWLUNIT(wlc), roam->time_since_bcn, roam->bcn_timeout));

	WL_ASSOC(("wl%d: original roam reason %d\n", WLCWLUNIT(wlc), roam->original_reason));
	switch (roam->original_reason) {
		case WLC_E_REASON_DEAUTH:
		case WLC_E_REASON_DISASSOC:
		case WLC_E_REASON_MINTXRATE:
		case WLC_E_REASON_DIRECTED_ROAM:
		case WLC_E_REASON_TSPEC_REJECTED:
			reason = WLC_E_LINK_DISASSOC;
			break;
		case WLC_E_REASON_BCNS_LOST:
		case WLC_E_REASON_BETTER_AP:
			reason = WLC_E_LINK_BCN_LOSS;
			break;
		default:
			reason = WLC_E_LINK_BCN_LOSS;
	}
	if (reason == WLC_E_LINK_BCN_LOSS) {
		WL_APSTA_UPDN(("wl%d: Reporting link down on config 0 (STA lost beacons)\n",
			WLCWLUNIT(wlc)));
	} else {
		WL_APSTA_UPDN(("wl%d: Reporting link down on config 0 (link down reason %d)\n",
			WLCWLUNIT(wlc), reason));
	}

	wlc_link(wlc, FALSE, &as->last_upd_bssid, cfg, reason);
	roam->bcns_lost = TRUE;
	wlc_sta_timeslot_unregister(cfg);

	/* reset rssi moving average */
	wlc_lq_rssi_snr_noise_bss_sta_ma_reset(wlc, cfg,
		CHSPEC_WLCBANDUNIT(cfg->current_bss->chanspec),
		WLC_RSSI_INVALID, WLC_SNR_INVALID, WLC_NOISE_INVALID);
	wlc_lq_rssi_bss_sta_event_upd(wlc, cfg);

	/* Delete the scb for the AP to free up any queued packets */
	if (BSSCFG_STA(cfg)) {
		uint bandunit;
		struct scb *scb;
		struct ether_addr *bssid = &cfg->BSSID;

		if (!ETHER_ISNULLADDR(bssid)) {
			bandunit = CHSPEC_WLCBANDUNIT(cfg->current_bss->chanspec);
			scb = wlc_scbfindband(wlc, cfg, bssid, bandunit);

			if (scb != NULL) {
				wlc_scbfree(wlc, scb);
			}
		}
	}

	/*
	 * Clear software BSSID information so that we do not
	 * process future beacons from this AP so that the roam
	 * code will continue attempting to join
	 */
	wlc_bss_clear_bssid(cfg);

} /* wlc_handle_ap_lost */

static bool
wlc_assoc_check_aplost_ok(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	wlc_roam_t *roam;

	BCM_REFERENCE(wlc);
	roam = cfg->roam;
	return (cfg->assoc->type == AS_IDLE) ||
		(cfg->bcn_to_dly && roam->consec_roam_bcns_lost > 1);
}

#define	MIN_LINK_MONITOR_INTERVAL	800	/* min link monitor interval, in unit of ms */

#define	SW_TIMER_LINK_REPORT	10

static void
wlc_ibss_monitor(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
		wlc_bss_info_t *bss, wlc_roam_t *roam)
{
	/* link state monitor */
	if (!cfg->BSS) {

		/* - if no beacon for too long time, indicate the link is down */
		if ((roam->time_since_bcn > roam->bcn_timeout) && !roam->bcns_lost) {
#ifdef WLAMPDU
			/* detect link up/down for IBSS so that ba sm can be cleaned up */
			scb_ampdu_cleanup_all(wlc, cfg);
#endif /* WLAMPDU */
			wlc_bss_mac_event(wlc, cfg, WLC_E_BEACON_RX, NULL,
				WLC_E_STATUS_FAIL, 0, 0, 0, 0);
			roam->bcns_lost = TRUE;
		}
	}

}

#if defined(DBG_BCN_LOSS)
static void
wlc_bcn_loss_log(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	wlc_bss_info_t *bss, wlc_roam_t *roam)

{
	struct scb *ap_scb = wlc_scbfindband(wlc, cfg, &cfg->BSSID,
		CHSPEC_WLCBANDUNIT(bss->chanspec));
	char eabuf[ETHER_ADDR_STR_LEN];
	char *phy_buf;
	struct bcmstrbuf b;

	if (ap_scb) {
		bcm_ether_ntoa(&cfg->BSSID, eabuf);
		WL_ASSOC_ERROR(("bcn_loss:\tLost beacon: %d AP:%s\n",
			roam->time_since_bcn, eabuf));
		WL_ASSOC_ERROR(("bcn_loss:\tNow: %d AP RSSI: %d BI: %d\n",
			wlc->pub->now, wlc->cfg->link->rssi,
			bss->beacon_period));

		WL_ASSOC_ERROR(("bcn_loss:\tTime of Assoc: %d"
			" Last pkt from AP: %d RSSI: %d BCN_RSSI: %d"
			" Last pkt to AP: %d\n",
			ap_scb->assoctime,
			ap_scb->dbg_bcn.last_rx,
			ap_scb->dbg_bcn.last_rx_rssi,
			ap_scb->dbg_bcn.last_bcn_rssi,
			ap_scb->dbg_bcn.last_tx));

		#define PHYCAL_DUMP_LEN 300
		phy_buf = MALLOC(wlc->osh, PHYCAL_DUMP_LEN);
		if (phy_buf != NULL) {
			bcm_binit(&b, phy_buf, PHYCAL_DUMP_LEN);
			(void)wlc_bmac_dump(wlc->hw, "phycalrxmin", &b);

			WL_ERROR(("bcn_loss:\tphydebug: 0x%x "
				"psmdebug: 0x%x psm_brc: 0x%x\n%s",
				R_REG(wlc->osh, D11_PHY_DEBUG(wlc)),
				R_REG(wlc->osh, D11_PSM_DEBUG(wlc)),
				R_REG(wlc->osh, D11_PSM_BRC_0(wlc)),
				phy_buf));
			MFREE(wlc->osh, phy_buf, PHYCAL_DUMP_LEN);
		} else {
			WL_ERROR(("wl%d:%s: out of memory for "
				"dbg bcn loss\n",
				wlc->pub->unit, __FUNCTION__));
		}
	}

}
#endif /* DBG_BCN_LOSS */

void
wlc_link_monitor_watchdog(wlc_info_t *wlc)
{
	int i;
	wlc_bsscfg_t *cfg;
	uint32 tdiff;
	assoc_cfg_cubby_t *assoc_cfg_cubby;
	int err;

	/* link monitor, roam, ... */
	FOREACH_AS_STA(wlc, i, cfg)	{
		wlc_roam_t *roam = cfg->roam;
		wlc_assoc_t *assoc = cfg->assoc;
		wlc_bss_info_t *bss = cfg->current_bss;
		wlc_cxn_t *cxn = cfg->cxn;

#ifdef PSTA
		/* No need to monitor for proxy stas */
		if (PSTA_ENAB(wlc->pub) && BSSCFG_PSTA(cfg))
			continue;
#endif /* PSTA */

		/* Monitor connection to a network */
		if (cfg->BSS) {
			struct scb *scb = wlc_scbfind(wlc, cfg, &cfg->BSSID);

			uint32 delta, cur = OSL_SYSUPTIME();
			bool check_roamscan = FALSE;
			delta = DELTA(cur, roam->link_monitor_last_update);

			if (delta >= MIN_LINK_MONITOR_INTERVAL) {
				check_roamscan = TRUE;
				roam->link_monitor_last_update = cur;
			}

			/* Increment the time since the last full scan, if caching is desired */
			if (roam->partialscan_period && roam->active && check_roamscan) {
				roam->time_since_upd++;
			}

			if (roam->motion_rssi_delta > 0 && scb != NULL &&
			    (wlc->pub->now - scb->assoctime) > WLC_ROAM_SCAN_PERIOD) {
				/* Don't activate motion detection code until we are at a
				 * "moderate" RSSI or lower
				 */
				if (!roam->RSSIref && bss->RSSI < MOTION_DETECT_RSSI) {
					WL_ASSOC(("wl%d: %s: Setting reference RSSI in this bss "
					          "to %d\n", wlc->pub->unit, __FUNCTION__,
					          bss->RSSI));
					roam->RSSIref = bss->RSSI;
				}
				if (roam->RSSIref)
					wlc_roam_motion_detect(cfg);
			}
			/* Handle Roaming if beacons are lost */
			if (roam->time_since_bcn > 0) {

				uint roam_time_thresh; /* millisec */
				wlc_pm_st_t *pm = cfg->pm;
				uint32 bp = bss->beacon_period;

#ifdef BCMDBG
				if (ETHER_ISNULLADDR(&cfg->BSSID)) {
					WL_ASSOC(("wl%d.%d: time_since_bcn %d\n", WLCWLUNIT(wlc),
						WLC_BSSCFG_IDX(cfg), roam->time_since_bcn));
				} else {
					WL_ASSOC_ERROR(("wl%d.%d: time_since_bcn %d\n",
							WLCWLUNIT(wlc),	WLC_BSSCFG_IDX(cfg),
							roam->time_since_bcn));
				}
#endif /* BCMDBG */

				/* convert from Kusec to millisec */
				bp = (bp << 10)/1000;

				/* bcn_timeout should be nonzero */
				ASSERT(roam->bcn_timeout != 0);

				if (cfg->bcn_to_dly)
					roam_time_thresh = MIN(roam->max_roam_time_thresh,
						((roam->bcn_timeout - 1)*1000));
				else
					roam_time_thresh = MIN(roam->max_roam_time_thresh,
						(roam->bcn_timeout*1000)/2);

				/* No beacon seen for a while, check unaligned beacon
				 * at UATBTT_TO_ROAM_BCN time before roam threshold time
				 */
				if (roam->time_since_bcn*1000u + bp * UATBTT_TO_ROAM_BCN >=
					roam_time_thresh) {

					if (pm->PMenabled && !pm->PM_override &&
						!ETHER_ISNULLADDR(&cfg->BSSID)) {
						WL_ASSOC(("wl%d: ROAM: check for unaligned TBTT, "
							"time_since_bcn %d Sec\n",
							WLCWLUNIT(wlc), roam->time_since_bcn));
						err = wlc_assoc_homech_req_register(cfg);
						if (err != BCME_OK) {
							WL_ERROR(("wl%d: homechreg failed err %d\n",
								WLCWLUNIT(wlc), err));
						}
					}
					if ((roam->timer_active == FALSE) &&
						!roam->roam_bcnloss_off) {
						wl_add_timer(wlc->wl, roam->timer,
							bp * UATBTT_TO_ROAM_BCN, FALSE);
						roam->timer_active = TRUE;
					}
				}

				/* no beacon seen for longer than roam_time_thresh and
				 * sta is still associated, not roaming,
				 * force going back to home channel to wait for beacon
				 * for 2 beacon periods
				 * tdiff in miliseconds
				 */
				assoc_cfg_cubby = BSSCFG_ASSOC_CUBBY(wlc->as, cfg);
				ASSERT(assoc_cfg_cubby != NULL);
				tdiff = OSL_SYSUPTIME() - assoc_cfg_cubby->timestamp_lastbcn;
				if (!ETHER_ISNULLADDR(&cfg->BSSID) &&
					cfg->assoc->type != AS_ROAM &&
					tdiff > roam_time_thresh) {
					err = wlc_assoc_homech_req_register(cfg);
					if (err != BCME_OK) {
						WL_ERROR(("wl%d: homechreg failed err=%d\n",
							WLCWLUNIT(wlc), err));
					}
				}
				if ((roam->time_since_bcn*1000u) < roam_time_thresh) {
					/* clear the consec_roam_bcns_lost counter */
					/* because we've seen at least 1 beacon since */
					/* we last called wlc_roam_bcns_lost() */
					roam->consec_roam_bcns_lost = 0;
				}
			}

			/* If the link was down and we got a beacon, mark it 'up' */
			/* bcns_lost indicates whether the link is marked 'down' or not */
			if (roam->time_since_bcn == 0 &&
			    roam->bcns_lost) {
				roam->bcns_lost = FALSE;
				bcopy(&cfg->prev_BSSID, &cfg->BSSID, ETHER_ADDR_LEN);
				bcopy(&cfg->prev_BSSID, &bss->BSSID,
				      ETHER_ADDR_LEN);
				WL_APSTA_UPDN(("wl%d: Reporting link up on config 0 (STA"
				               " recovered beacons)\n", WLCWLUNIT(wlc)));
				wlc_link(wlc, TRUE, &cfg->BSSID, cfg, 0);
				WL_ASSOC(("wl%d: ROAM: new beacon: called link_up() \n",
				            WLCWLUNIT(wlc)));
			}

#if defined(DBG_BCN_LOSS)

	if (roam->time_since_bcn > roam->bcn_timeout) {
			wlc_bcn_loss_log(wlc, cfg, bss, roam);
	}

#endif /* DBG_BCN_LOSS */

			/* No beacon for too long time. indicate the link is down */
			if (roam->time_since_bcn > roam->bcn_timeout &&
				!roam->bcns_lost && wlc_assoc_check_aplost_ok(wlc, cfg)) {
				wlc_handle_ap_lost(wlc, cfg);
			}

			if (roam->scan_block && check_roamscan)
				roam->scan_block--;

			if (roam->ratebased_roam_block)
				roam->ratebased_roam_block--;

			if (!AS_IN_PROGRESS(wlc) && !SCAN_IN_PROGRESS(wlc->scan) &&
				roam->reassoc_param) {

				if ((wlc_reassoc(cfg, roam->reassoc_param)) != 0) {
					WL_INFORM(("%s: Delayed reassoc attempt failed\r\n",
						__FUNCTION__));
				}
			}
		}

		wlc_ibss_monitor(wlc, cfg, bss, roam);

		/* Increment time since last bcn if we are associated (Infra or IBSS)
		 * Avoid wrapping by maxing count to bcn_timeout + 1
		 * Do not increment during an assoc recreate since we have not yet
		 * reestablished the connection to the AP.
		 */
		if (cxn->ign_bcn_lost_det == 0 &&
		    roam->time_since_bcn <= roam->bcn_timeout &&
		    !(ASSOC_RECREATE_ENAB(wlc->pub) && assoc->type == AS_RECREATE) &&
		    (FALSE ||
#ifdef BCMQT_CPU
		     TRUE ||
#endif // endif
		     !ISSIM_ENAB(wlc->pub->sih))) {
#ifdef BCMDBG
			if (roam->time_since_bcn > 0) {
#ifdef WLMCNX
				if (MCNX_ENAB(wlc->pub)) {
					WL_ASSOC(("wl%d.%d: ROAM: time_since_bcn %d\n",
					          wlc->pub->unit, WLC_BSSCFG_IDX(cfg),
					          roam->time_since_bcn));
				} else
#endif // endif
				{
					if (cfg == wlc->cfg) {
#ifdef WLCNT
						WL_ASSOC(("wl%d.%d: "
							"ROAM: time_since_bcn %d, tbtt %u\n",
							wlc->pub->unit, WLC_BSSCFG_IDX(cfg),
							roam->time_since_bcn,
							wlc->pub->_cnt->tbtt));
#endif /* WLCNT */
						WL_ASSOC(("wl%d.%d: "
							"TSF: 0x%08x 0x%08x CFPSTART: 0x%08x "
							"CFPREP: 0x%08x\n",
							wlc->pub->unit, WLC_BSSCFG_IDX(cfg),
							R_REG(wlc->osh, D11_TSFTimerHigh(wlc)),
							R_REG(wlc->osh, D11_TSFTimerLow(wlc)),
							R_REG(wlc->osh, D11_CFPStart(wlc)),
							R_REG(wlc->osh, D11_CFPRep(wlc))));
					}
				}
			}
#endif	/* BCMDBG */

			wlc_roam_update_time_since_bcn(cfg, roam->time_since_bcn + 1);
		}

		if (roam->thrash_block_active) {
			bool still_blocked = FALSE;
			int j;
			for (j = 0; j < (int) roam->cache_numentries; j++) {
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
				char eabuf[ETHER_ADDR_STR_LEN];
#endif // endif
				if (roam->cached_ap[j].time_left_to_next_assoc == 0)
					continue;
				roam->cached_ap[j].time_left_to_next_assoc--;
				if (roam->cached_ap[j].time_left_to_next_assoc == 0) {
					WL_ASSOC(("wl%d: %s: ROAM: AP with BSSID %s on "
						  "chanspec 0x%x available for "
						  "reassociation\n", wlc->pub->unit,
						  __FUNCTION__,
						  bcm_ether_ntoa(&roam->cached_ap[j].BSSID, eabuf),
						  roam->cached_ap[j].chanspec));
				} else {
					WL_ASSOC(("wl%d: %s: ROAM: AP with BSSID %s on "
						  "chanspec 0x%x blocked for %d seconds\n",
						  wlc->pub->unit, __FUNCTION__,
						  bcm_ether_ntoa(&roam->cached_ap[j].BSSID, eabuf),
						  roam->cached_ap[j].chanspec,
						  roam->cached_ap[j].time_left_to_next_assoc));
					still_blocked = TRUE;
					break;
				}
			}
			if (!still_blocked)
				roam->thrash_block_active = FALSE;
		}
	}
} /* wlc_link_monitor_watchdog */

/* Don't clear the WPA join preferences if they are already set */
void
wlc_join_pref_reset_cond(wlc_bsscfg_t *cfg)
{
	if (cfg->join_pref->wpas == 0)
		wlc_join_pref_reset(cfg);
}

/** validate and sanitize chanspecs passed with assoc params */
int
wlc_assoc_chanspec_sanitize(wlc_info_t *wlc, chanspec_list_t *list, int len, wlc_bsscfg_t *cfg)
{
	uint32 chanspec_num;
	chanspec_t *chanspec_list;
	int i;

	BCM_REFERENCE(cfg);
	if ((uint)len < sizeof(list->num))
		return BCME_BUFTOOSHORT;

	chanspec_num = load32_ua((uint8 *)&list->num);

	if ((uint)len < sizeof(list->num) + chanspec_num * sizeof(list->list[0]))
		return BCME_BUFTOOSHORT;

	chanspec_list = list->list;

	for (i = 0; i < (int)chanspec_num; i++) {
		chanspec_t chanspec = load16_ua((uint8 *)&chanspec_list[i]);
		if (wf_chspec_malformed(chanspec))
			return BCME_BADCHAN;
		/* get the control channel from the chanspec */
		chanspec = CH20MHZ_CHSPEC(wf_chspec_ctlchan(chanspec));
		htol16_ua_store(chanspec, (uint8 *)&chanspec_list[i]);
		if (!wlc_valid_chanspec_db(wlc->cmi, chanspec)) {
			/* Country Deafult : If channel is not supported by phy,
			* reject the channel
			*/
			if (WLC_CNTRY_DEFAULT_ENAB(wlc) && (wf_channel2mhz(CHSPEC_CHANNEL(chanspec),
				CHSPEC_IS5G(chanspec) ?
				WF_CHAN_FACTOR_5_G : WF_CHAN_FACTOR_2_4_G) != -1))
				continue;
			return BCME_BADCHAN;
		}
	}
	return BCME_OK;
}
/* API usage :
* Returns bsscfg from the target bss if association is in progress.
*/
wlc_bsscfg_t * BCMFASTPATH
wlc_find_assoc_in_progress(wlc_info_t *wlc, struct ether_addr *bssid)
{
	int idx;
	wlc_assoc_info_t *as;
	wlc_bsscfg_t *bc;
	wlc_bss_info_t *bi;

	as = wlc->as;

	if (as == NULL)
		return NULL;

	if (as->cmn->assoc_req[0] == NULL)
		return NULL;

	if (ETHER_ISNULLADDR(bssid) || ETHER_ISMULTI(bssid))
		return NULL;

	for (idx = 0; idx < WLC_MAXBSSCFG; idx++) {
		bc = as->cmn->assoc_req[idx];

		if (bc == NULL)
			break;

		bi = bc->target_bss;
		if (bi) {
			if (eacmp(bssid->octet, bi->BSSID.octet) == 0)  {
				return bc;
			}
		}
	}
	return NULL;
}

/* return min roam profile index */
static int16
wlc_roamprof_get_min_idx(wlc_info_t *wlc, int bandtype)
{
	wlcband_t *band;

	if (WBTEXT_ENAB(wlc->pub)) {
		/* if wbtext active, return 0 */
		if (WBTEXT_ACTIVE(wlc->pub)) {
			return 0;
		}

		/* if wbtext not active, return max_idx */
		band = wlc->bandstate[BAND_2G(bandtype) ? BAND_2G_INDEX : BAND_5G_INDEX];
		return band->roam_prof_max_idx - 1;
	} else {
		return 0;
	}
}

/* is roam_prof[idx] cu enabled? */
static bool
wlc_roamprof_is_cu_enabled(wlc_info_t *wlc, int bandtype, int16 idx)
{
	wlcband_t *band;

	ASSERT((idx >= 0) && (idx < WL_MAX_ROAM_PROF_BRACKETS));
	ASSERT((bandtype == WLC_BAND_2G) || (bandtype == WLC_BAND_5G));

	band = wlc->bandstate[BAND_2G(bandtype) ? BAND_2G_INDEX : BAND_5G_INDEX];

	return !!band->roam_prof[idx].channel_usage;
}

/* transition to AS_WAIT_RCV_BCN state */
void
wlc_restart_sta(wlc_info_t *wlc)
{
	int idx;
	wlc_bsscfg_t *cfg;
	bool update_tsf_cfprep = TRUE;

	WL_TRACE(("wl%d: %s\n", wlc->pub->unit, __FUNCTION__));
#ifdef WLMCNX
	/* handled by mcnx */
	if (MCNX_ENAB(wlc->pub))
		return;
#endif /* WLMCNX */

	FOREACH_AS_STA(wlc, idx, cfg) {
		if (cfg->up) {
			/* update tsf_cfprep if first up STA  */
			if (update_tsf_cfprep) {
				uint32 bi;
				/* get beacon period from primary bsscfg and convert to uS */
				bi = wlc->cfg->current_bss->beacon_period << 10;
				/* update the tsf_cfprep register */
				/* since init path would reset to default value */
				W_REG(wlc->osh, D11_CFPRep(wlc), (bi << CFPREP_CBI_SHIFT));
				update_tsf_cfprep = FALSE;
			}

			/* restore state upon receiving next beacon */
			wlc_assoc_change_state(cfg, AS_WAIT_RCV_BCN);

			/* Update maccontrol PM related bits */
			wlc_set_ps_ctrl(cfg);

			/* keep the chip awake until a beacon is received */
			if (cfg->pm->PMenabled) {
				wlc_set_pmawakebcn(cfg, TRUE);
				wlc_dtimnoslp_set(cfg);
			}

			wlc_pretbtt_set(cfg);
		}
	}
}

/* receive bcn in state AS_SYNC_RCV_BCN */
void
wlc_resync_recv_bcn(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	wlc_d11rxhdr_t *wrxh, uint8 *plcp, struct dot11_bcn_prb *bcn)
{
	WL_APSTA_UPDN(("wl%d: %s: got resync beacon, update TSF/TBTT\n",
	               wlc->pub->unit, __FUNCTION__));
	wlc_tsf_adopt_bcn(cfg, wrxh, plcp, bcn);
	wlc_assoc_change_state(cfg, AS_IDLE);
	/* APSTA: complete any deferred AP bringup */
	if (AP_ENAB(wlc->pub))
		wlc_restart_ap(wlc->ap);
	/* update PM state for STA */
	else
		wlc_set_pmstate(cfg, cfg->pm->PMenabled);
}

/* transition to AS_SYNC_RCV_BCN state */
void
wlc_resync_sta(wlc_info_t *wlc)
{
	if (!MCNX_ENAB(wlc->pub)) {
		int idx;
		wlc_bsscfg_t *bc;

		FOREACH_AS_STA(wlc, idx, bc) {
			if (bc != wlc->cfg)
				continue;
			if (!bc->BSS)
				continue;
			WL_APSTA_UPDN(("wl%d: last AP down,"
			               "sync STA: assoc_state %d type %d\n",
			               wlc->pub->unit, bc->assoc->state,
			               bc->assoc->type));
			/* We need to update tsf due to moving from APSTA -> STA.
			 * This is needed only for non-P2P ucode
			 * If not in idle mode then roam will update us.
			 * Otherwise unaligned-tbtt recovery will update tsf.
			 */
			if (bc->assoc->state == AS_IDLE) {
				ASSERT(bc->assoc->type == AS_NONE);
				wlc_assoc_change_state(bc, AS_SYNC_RCV_BCN);
			}
		}
	}
}
#endif /* STA */

/* ******** WORK IN PROGRESS ******** */

#ifdef STA
/* iovar table */
enum {
	IOV_STA_RETRY_TIME = 1,
	IOV_JOIN_PREF = 2,
	IOV_ASSOC_RETRY_MAX = 3,
	IOV_ASSOC_CACHE_ASSIST = 4,
	IOV_IBSS_JOIN_ONLY = 5,
	IOV_ASSOC_INFO = 6,
	IOV_ASSOC_REQ_IES = 7,
	IOV_ASSOC_RESP_IES = 8,
	IOV_BCN_TIMEOUT = 9,	/* Beacon timeout */
	IOV_BCN_THRESH = 10,	/* Beacon before send an event notifying of beacon loss */
	IOV_IBSS_COALESCE_ALLOWED = 11,
	IOV_ASSOC_LISTEN = 12,	/* Request this listen interval frm AP */
	IOV_ASSOC_RECREATE = 13,
	IOV_PRESERVE_ASSOC = 14,
	IOV_ASSOC_PRESERVED = 15,
	IOV_ASSOC_VERIFY_TIMEOUT = 16,
	IOV_RECREATE_BI_TIMEOUT = 17,
	IOV_ASSOCROAM = 18,
	IOV_ASSOCROAM_START = 19,
	IOV_ROAM_OFF = 20,	/* Disable/Enable roaming */
	IOV_FULLROAM_PERIOD = 21,
	IOV_ROAM_PERIOD = 22,
	IOV_ROAM_NFSCAN = 23,
	IOV_TXMIN_ROAMTHRESH = 24,	/* Roam threshold for too many pkts at min rate */
	IOV_AP_ENV_DETECT = 25,		/* Auto-detect the environment for optimal roaming */
	IOV_TXFAIL_ROAMTHRESH = 26,	/* Roam threshold for tx failures at min rate */
	IOV_MOTION_RSSI_DELTA = 27,	/* enable/disable motion detection and set threshold */
	IOV_SCAN_PIGGYBACK = 28,	/* Build roam cache from periodic upper-layer scans */
	IOV_ROAM_RSSI_CANCEL_HYSTERESIS = 29,
	IOV_ROAM_CONF_AGGRESSIVE = 30,
	IOV_ROAM_MULTI_AP_ENV = 31,
	IOV_ROAM_CI_DELTA = 32,
	IOV_SPLIT_ROAMSCAN = 33,
	IOV_ROAM_CHN_CACHE_LIMIT = 34,
	IOV_ROAM_CHANNELS_IN_CACHE = 35,
	IOV_ROAM_CHANNELS_IN_HOTLIST = 36,
	IOV_ROAMSCAN_PARMS = 37,
	IOV_LPAS = 38,
	IOV_OPPORTUNISTIC_ROAM_OFF = 39, /* Disable/Enable Opportunistic roam */
	IOV_ROAM_PROF = 40,
	IOV_ROAM_TIME_THRESH = 41,
	IOV_JOIN = 42,
	IOV_ROAM_BCNLOSS_OFF = 43,
	IOV_BCN_TO_DLY = 44,
	IOV_ROAM_CACHE = 45,
	IOV_SCB_ASSOC = 46,
	IOV_LAST
};

static const bcm_iovar_t assoc_iovars[] = {
	{"sta_retry_time", IOV_STA_RETRY_TIME, (IOVF_WHL|IOVF_OPEN_ALLOW), 0, IOVT_UINT32, 0},
	{"join_pref", IOV_JOIN_PREF, IOVF_OPEN_ALLOW, 0, IOVT_BUFFER, 0},
	{"assoc_retry_max", IOV_ASSOC_RETRY_MAX, IOVF_WHL, 0, IOVT_UINT8, 0},
#ifdef WLSCANCACHE
	{"assoc_cache_assist", IOV_ASSOC_CACHE_ASSIST, IOVF_RSDB_SET, 0, IOVT_BOOL, 0},
#endif // endif
	{"IBSS_join_only", IOV_IBSS_JOIN_ONLY, IOVF_OPEN_ALLOW|IOVF_RSDB_SET, 0, IOVT_BOOL, 0},
	{"assoc_info", IOV_ASSOC_INFO, IOVF_OPEN_ALLOW, 0, IOVT_BUFFER, (sizeof(wl_assoc_info_t))},
	{"assoc_req_ies", IOV_ASSOC_REQ_IES, IOVF_OPEN_ALLOW, 0, IOVT_BUFFER, 0},
	{"assoc_resp_ies", IOV_ASSOC_RESP_IES, IOVF_OPEN_ALLOW, 0, IOVT_BUFFER, 0},
	{"bcn_timeout", IOV_BCN_TIMEOUT, IOVF_NTRL, 0, IOVT_UINT32, 0},
	{"bcn_thresh", IOV_BCN_THRESH, IOVF_WHL | IOVF_BSSCFG_STA_ONLY, 0, IOVT_UINT32, 0},
	{"ibss_coalesce_allowed", IOV_IBSS_COALESCE_ALLOWED, (IOVF_OPEN_ALLOW|IOVF_RSDB_SET), 0,
	IOVT_BOOL, 0},
	{"assoc_listen", IOV_ASSOC_LISTEN, 0, 0, IOVT_UINT16, 0},
#ifdef WL_ASSOC_RECREATE
	{"assoc_recreate", IOV_ASSOC_RECREATE, IOVF_OPEN_ALLOW | IOVF_RSDB_SET, 0, IOVT_BOOL, 0},
	{"preserve_assoc", IOV_PRESERVE_ASSOC, IOVF_OPEN_ALLOW, 0, IOVT_BOOL, 0},
	{"assoc_preserved", IOV_ASSOC_PRESERVED, IOVF_OPEN_ALLOW, 0, IOVT_BOOL, 0},
	{"assoc_verify_timeout", IOV_ASSOC_VERIFY_TIMEOUT, IOVF_OPEN_ALLOW, 0, IOVT_UINT32, 0},
	{"recreate_bi_timeout", IOV_RECREATE_BI_TIMEOUT, IOVF_OPEN_ALLOW, 0, IOVT_UINT32, 0},
#endif // endif
	{"roam_off", IOV_ROAM_OFF, IOVF_OPEN_ALLOW, 0, IOVT_BOOL,   0},
	{"assocroam", IOV_ASSOCROAM, 0, 0, IOVT_BOOL, 0},
	{"assocroam_start", IOV_ASSOCROAM_START, 0, 0, IOVT_BOOL, 0},
	{"fullroamperiod", IOV_FULLROAM_PERIOD, IOVF_OPEN_ALLOW, 0, IOVT_UINT32, 0},
	{"roamperiod", IOV_ROAM_PERIOD, IOVF_OPEN_ALLOW, 0, IOVT_UINT32, 0},
	{"roam_nfscan", IOV_ROAM_NFSCAN, 0, 0, IOVT_UINT8, 0},
	{"txfail_roamthresh", IOV_TXFAIL_ROAMTHRESH, 0, 0, IOVT_UINT8, 0},
	{"txmin_roamthresh", IOV_TXMIN_ROAMTHRESH, 0, 0, IOVT_UINT8, 0},
	{"roam_env_detection", IOV_AP_ENV_DETECT, 0, 0, IOVT_BOOL, 0},
	{"roam_motion_detection", IOV_MOTION_RSSI_DELTA, 0, 0, IOVT_UINT8, 0},
	{"roam_scan_piggyback", IOV_SCAN_PIGGYBACK, 0, 0, IOVT_BOOL, 0},
	{"roam_rssi_cancel_hysteresis", IOV_ROAM_RSSI_CANCEL_HYSTERESIS, 0, 0, IOVT_UINT8, 0},
	{"roam_conf_aggressive", IOV_ROAM_CONF_AGGRESSIVE, 0, 0,  IOVT_INT32, 0},
	{"roam_multi_ap_env", IOV_ROAM_MULTI_AP_ENV, 0, 0,  IOVT_BOOL, 0},
	{"roam_ci_delta", IOV_ROAM_CI_DELTA, 0, 0, IOVT_UINT8, 0},
	{"split_roamscan", IOV_SPLIT_ROAMSCAN, 0, 0,  IOVT_BOOL, 0},
	{"roam_chn_cache_limit", IOV_ROAM_CHN_CACHE_LIMIT, 0, 0, IOVT_UINT8, 0},
	{"roam_channels_in_cache", IOV_ROAM_CHANNELS_IN_CACHE,
	(0), 0, IOVT_BUFFER, (sizeof(uint32)*(WL_NUMCHANSPECS+1))},
	{"roam_channels_in_hotlist", IOV_ROAM_CHANNELS_IN_HOTLIST,
	(0), 0, IOVT_BUFFER, (sizeof(uint32)*(WL_NUMCHANSPECS+1))},
	{"roamscan_parms", IOV_ROAMSCAN_PARMS, 0, 0, IOVT_BUFFER, 0},
#ifdef LPAS
	{"lpas", IOV_LPAS, IOVF_RSDB_SET, 0, IOVT_UINT32, 0},
#endif /* LPAS */
#ifdef OPPORTUNISTIC_ROAM
	{"oppr_roam_off", IOV_OPPORTUNISTIC_ROAM_OFF, 0, 0, IOVT_BOOL,   0},
#endif // endif
	{"roam_prof", IOV_ROAM_PROF, 0, 0, IOVT_BUFFER, 0},
	{"roam_time_thresh", IOV_ROAM_TIME_THRESH, 0, 0, IOVT_UINT32, 0},
	{"join", IOV_JOIN, IOVF_BSSCFG_STA_ONLY, 0, IOVT_BUFFER, WL_EXTJOIN_PARAMS_FIXED_SIZE},
	{"roam_bcnloss_off", IOV_ROAM_BCNLOSS_OFF, 0, 0, IOVT_BOOL, 0},
	{"bcn_to_dly", IOV_BCN_TO_DLY, 0, 0, IOVT_BOOL, 0},
	{"roam_cache", IOV_ROAM_CACHE, 0, 0, IOVT_BUFFER, BCM_XTLV_HDR_SIZE},
#ifdef  WL_CLIENT_SAE
	{"scb_assoc", IOV_SCB_ASSOC, IOVF_BSSCFG_STA_ONLY, 0,
	IOVT_BUFFER, (sizeof(wl_ext_auth_evt_t))},
#endif /* WL_CLIENT_SAE */
	{NULL, 0, 0, 0, 0, 0}
};

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */

#include <wlc_patch.h>

/* iovar dispatcher */
static int
wlc_assoc_doiovar(void *hdl, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif)
{
	wlc_assoc_info_t *asi = hdl;
	wlc_info_t *wlc = asi->wlc;
	int err = BCME_OK;
	bool bool_val;
	int32 int_val = 0;
	int32 *ret_int_ptr;
	wlc_bsscfg_t *bsscfg;
	wlc_assoc_t *as;
	wlc_roam_t *roam;

	/* update bsscfg pointer */
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	if (BSSCFG_AP(bsscfg) || BSSCFG_P2PDISC(bsscfg) || BSSCFG_IS_TDLS(bsscfg)) {
		return BCME_NOTSTA;
	}

	as = bsscfg->assoc;
	roam = bsscfg->roam;

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	/* bool conversion to avoid duplication below */
	bool_val = (int_val != 0);

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	/* Do the actual parameter implementation */
	switch (actionid) {

	case IOV_GVAL(IOV_STA_RETRY_TIME):
		*ret_int_ptr = (int32)asi->cmn->sta_retry_time;
		break;

	case IOV_SVAL(IOV_STA_RETRY_TIME):
		if (int_val < 0 || int_val > WLC_STA_RETRY_MAX_TIME) {
			err = BCME_RANGE;
			break;
		}
		asi->cmn->sta_retry_time = (uint32)int_val;
		break;

	case IOV_GVAL(IOV_JOIN_PREF):
		err = wlc_join_pref_build(bsscfg, arg, len);
		break;

	case IOV_SVAL(IOV_JOIN_PREF):
		err = wlc_join_pref_parse(bsscfg, arg, len);
		break;

	case IOV_GVAL(IOV_ASSOC_INFO): {
		wl_assoc_info_t *assoc_info = (wl_assoc_info_t *)arg;

		uint32 flags = (as->req_is_reassoc) ? WLC_ASSOC_REQ_IS_REASSOC : 0;
		bcopy(&flags, &assoc_info->flags, sizeof(uint32));

		bcopy(&as->req_len, &assoc_info->req_len, sizeof(uint32));
		bcopy(&as->resp_len, &assoc_info->resp_len, sizeof(uint32));

		if (as->req_len && as->req) {
			bcopy((char*)as->req, &assoc_info->req,
			      sizeof(struct dot11_assoc_req));
			if (as->req_is_reassoc)
				bcopy((char*)(as->req+1), &assoc_info->reassoc_bssid.octet[0],
					ETHER_ADDR_LEN);
		}
		if (as->resp_len && as->resp)
			bcopy((char*)as->resp, &assoc_info->resp,
			      sizeof(struct dot11_assoc_resp));
		break;
	}

	case IOV_SVAL(IOV_ASSOC_INFO): {
		wl_assoc_info_t *assoc_info = (wl_assoc_info_t *)arg;
		as->capability = assoc_info->req.capability;
		as->listen = assoc_info->req.listen;
		bcopy(assoc_info->reassoc_bssid.octet, as->bssid.octet,
			ETHER_ADDR_LEN);

		break;
	}

	case IOV_GVAL(IOV_ASSOC_REQ_IES): {
		/* if a reassoc then skip the bssid */
		uint ie_off = as->req_is_reassoc ?
			sizeof(struct dot11_reassoc_req) : sizeof(struct dot11_assoc_req);

		if (!as->req_len) {
			if (as->req) {
				err = BCME_ERROR;
			}
			break;
		}
		if (!as->req) {
			err = BCME_ERROR;
			break;
		}
		if ((as->req_len > ie_off) && (len >= (as->req_len - ie_off))) {
			memcpy(arg, (uint8 *)(as->req) + ie_off, as->req_len - ie_off);
		} else {
			return BCME_BUFTOOSHORT;
		}

		break;
	}

	case IOV_SVAL(IOV_ASSOC_REQ_IES): {
		if (len == 0)
			break;
		if (as->ie)
			MFREE(wlc->osh, as->ie, as->ie_len);
		as->ie_len = len;
		if (!(as->ie = MALLOC(wlc->osh, as->ie_len))) {
			WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			break;
		}
		bcopy((int8 *) arg, as->ie, as->ie_len);
		break;
	}

	case IOV_GVAL(IOV_ASSOC_RESP_IES):
		if (!as->resp || !as->resp_len ||
			(as->resp_len < sizeof(struct dot11_assoc_resp))) {
			err = BCME_NOTFOUND;
			break;
		}
		if (len < (as->resp_len - sizeof(struct dot11_assoc_resp)))
			return BCME_BUFTOOSHORT;
		bcopy((char*)(as->resp+1), (char*)arg, as->resp_len -
		      sizeof(struct dot11_assoc_resp));
		break;

	case IOV_GVAL(IOV_ASSOC_RETRY_MAX):
		*ret_int_ptr = (int32)as->retry_max;
		break;

	case IOV_SVAL(IOV_ASSOC_RETRY_MAX):
		as->retry_max = (uint8)int_val;
		break;

#ifdef WLSCANCACHE
	case IOV_GVAL(IOV_ASSOC_CACHE_ASSIST):
		*ret_int_ptr = (int32)wlc->_assoc_cache_assist;
		break;

	case IOV_SVAL(IOV_ASSOC_CACHE_ASSIST):
		wlc->_assoc_cache_assist = bool_val;
		break;
#endif /* WLSCANCACHE */

	case IOV_GVAL(IOV_IBSS_JOIN_ONLY):
		*ret_int_ptr = (int32)wlc->IBSS_join_only;
		break;
	case IOV_SVAL(IOV_IBSS_JOIN_ONLY):
		wlc->IBSS_join_only = bool_val;
		break;

	case IOV_GVAL(IOV_BCN_TIMEOUT):
		*ret_int_ptr = (int32)roam->bcn_timeout;
		break;

	case IOV_SVAL(IOV_BCN_TIMEOUT):
		roam->bcn_timeout = (uint)int_val;
		break;

	case IOV_GVAL(IOV_BCN_THRESH):
		*ret_int_ptr = (int32)roam->bcn_thresh;
		break;

	case IOV_SVAL(IOV_BCN_THRESH):
		roam->bcn_thresh = (uint)int_val;
		break;

	case IOV_SVAL(IOV_IBSS_COALESCE_ALLOWED):
	       wlc->ibss_coalesce_allowed = bool_val;
	       break;

	case IOV_GVAL(IOV_IBSS_COALESCE_ALLOWED):
	       *ret_int_ptr = (int32)wlc->ibss_coalesce_allowed;
	       break;

	case IOV_GVAL(IOV_ASSOC_LISTEN):
		*ret_int_ptr = as->listen;
		break;

	case IOV_SVAL(IOV_ASSOC_LISTEN):
		as->listen = (uint16)int_val;
		break;

#ifdef WL_ASSOC_RECREATE
	case IOV_SVAL(IOV_ASSOC_RECREATE): {
		int i;
		wlc_bsscfg_t *bc;

		if (wlc->pub->_assoc_recreate == bool_val)
			break;

		/* if we are turning the feature on, nothing more to do */
		if (bool_val) {
			wlc->pub->_assoc_recreate = bool_val;
			break;
		}

		/* if we are turning the feature off clean up assoc recreate state */

		/* if we are in the process of an association recreation, abort it */
		if (as->type == AS_RECREATE)
			wlc_assoc_abort(bsscfg);
		FOREACH_BSS(wlc, i, bc) {
			/* if we are down with a STA bsscfg enabled due to the preserve
			 * flag, disable the bsscfg. Without the preserve flag we would
			 * normally disable the bsscfg as we put the driver into the down
			 * state.
			 */
			if (!wlc->pub->up && BSSCFG_STA(bc) && (bc->flags & WLC_BSSCFG_PRESERVE))
				wlc_bsscfg_disable(wlc, bc);

			/* clean up assoc recreate preserve flag */
			bc->flags &= ~WLC_BSSCFG_PRESERVE;
		}

		wlc->pub->_assoc_recreate = bool_val;
		break;
	}

	case IOV_GVAL(IOV_ASSOC_RECREATE):
		*ret_int_ptr = wlc->pub->_assoc_recreate;
		break;

	case IOV_GVAL(IOV_ASSOC_PRESERVED):
	        *ret_int_ptr = as->preserved;
		break;

	case IOV_SVAL(IOV_PRESERVE_ASSOC):
		if (!ASSOC_RECREATE_ENAB(wlc->pub) && bool_val) {
			err = BCME_BADOPTION;
			break;
		}

		if (bool_val)
			bsscfg->flags |= WLC_BSSCFG_PRESERVE;
		else
			bsscfg->flags &= ~WLC_BSSCFG_PRESERVE;
		break;

	case IOV_GVAL(IOV_PRESERVE_ASSOC):
		*ret_int_ptr = ((bsscfg->flags & WLC_BSSCFG_PRESERVE) != 0);
		break;

	case IOV_SVAL(IOV_ASSOC_VERIFY_TIMEOUT):
		as->verify_timeout = (uint)int_val;
		break;

	case IOV_GVAL(IOV_ASSOC_VERIFY_TIMEOUT):
		*ret_int_ptr = (int)as->verify_timeout;
		break;

	case IOV_SVAL(IOV_RECREATE_BI_TIMEOUT):
		as->recreate_bi_timeout = (uint)int_val;
		break;

	case IOV_GVAL(IOV_RECREATE_BI_TIMEOUT):
		*ret_int_ptr = (int)as->recreate_bi_timeout;
		break;
#endif /* WL_ASSOC_RECREATE */

	case IOV_SVAL(IOV_ROAM_OFF):
		roam->off = bool_val;
		break;

	case IOV_GVAL(IOV_ROAM_OFF):
		*ret_int_ptr = (int32)roam->off;
		break;

	case IOV_GVAL(IOV_ASSOCROAM):
		*ret_int_ptr = (int32)roam->assocroam;
		break;

	case IOV_SVAL(IOV_ASSOCROAM):
		roam->assocroam = bool_val;
		break;

	case IOV_SVAL(IOV_ASSOCROAM_START):
		wlc_assoc_roam(bsscfg);
		break;

	case IOV_GVAL(IOV_FULLROAM_PERIOD):
		int_val = roam->fullscan_period;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_FULLROAM_PERIOD):
		roam->fullscan_period = int_val;
		wlc_roam_prof_update_default(wlc, bsscfg);
		break;

	case IOV_GVAL(IOV_ROAM_PERIOD):
		int_val = roam->partialscan_period;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_ROAM_PERIOD):
		roam->partialscan_period = int_val;
		wlc_roam_prof_update_default(wlc, bsscfg);
		break;

	case IOV_GVAL(IOV_ROAM_NFSCAN):
		*ret_int_ptr = (int32)roam->nfullscans;
		break;

	case IOV_SVAL(IOV_ROAM_NFSCAN):
		if (int_val > 0) {
			roam->nfullscans = (uint8)int_val;
			wlc_roam_prof_update_default(wlc, bsscfg);
		} else
			err = BCME_BADARG;
		break;

	case IOV_GVAL(IOV_TXFAIL_ROAMTHRESH):
		int_val = roam->minrate_txfail_thresh;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_TXFAIL_ROAMTHRESH):
		roam->minrate_txfail_thresh = int_val;
		break;

	/* Start roam scans if we are sending too often at the min TX rate */
	/* Setting to zero disables this feature */
	case IOV_GVAL(IOV_TXMIN_ROAMTHRESH):
		*ret_int_ptr = roam->minrate_txpass_thresh;
		break;

	case IOV_SVAL(IOV_TXMIN_ROAMTHRESH):
	        roam->minrate_txpass_thresh = (uint8)int_val;
	        /* this must be set to turn off the trigger */
	        roam->txpass_thresh = (uint8)int_val;
		break;

	/* Auto-detect the AP density within the environment. '0' disables all auto-detection.
	 * Setting to AP_ENV_INDETERMINATE turns on auto-detection
	 */
	case IOV_GVAL(IOV_AP_ENV_DETECT):
	        *ret_int_ptr = (int32)roam->ap_environment;
		break;

	case IOV_SVAL(IOV_AP_ENV_DETECT):
	        roam->ap_environment = (uint8)int_val;
		break;

	/* change roaming behavior if we detect that the STA is moving */
	case IOV_GVAL(IOV_MOTION_RSSI_DELTA):
	        *ret_int_ptr = roam->motion_rssi_delta;
	        break;

	case IOV_SVAL(IOV_MOTION_RSSI_DELTA):
	        roam->motion_rssi_delta = (uint8)int_val;
	        break;

	/* Piggyback roam scans on periodic upper-layer scans, e.g. the 63sec WinXP WZC scan */
	case IOV_GVAL(IOV_SCAN_PIGGYBACK):
	        *ret_int_ptr = (int32)roam->piggyback_enab;
		break;

	case IOV_SVAL(IOV_SCAN_PIGGYBACK):
	        roam->piggyback_enab = bool_val;
		break;

	case IOV_GVAL(IOV_ROAM_RSSI_CANCEL_HYSTERESIS):
		*ret_int_ptr = wlc->roam_rssi_cancel_hysteresis;
		break;

	case IOV_SVAL(IOV_ROAM_RSSI_CANCEL_HYSTERESIS):
		wlc->roam_rssi_cancel_hysteresis = (uint8)int_val;
		break;

	case IOV_GVAL(IOV_ROAM_CONF_AGGRESSIVE):
		if (BSSCFG_AP(bsscfg)) {
			err = BCME_NOTSTA;
			break;
		}

		*ret_int_ptr = ((-roam->roam_trigger_aggr) & 0xFFFF) << 16;
		*ret_int_ptr |= (roam->roam_delta_aggr & 0xFFFF);
		break;

	case IOV_SVAL(IOV_ROAM_CONF_AGGRESSIVE): {
		int16 roam_trigger;
		uint16 roam_delta;
		roam_trigger = -(int16)(int_val >> 16) & 0xFFFF;
		roam_delta = (uint16)(int_val & 0xFFFF);

		if (BSSCFG_AP(bsscfg)) {
			err = BCME_NOTSTA;
			break;
		}

		if ((roam_trigger > 0) || (roam_trigger < -100) || (roam_delta > 100)) {
			err = BCME_BADOPTION;
			break;
		}

		roam->roam_trigger_aggr = roam_trigger;
		roam->roam_delta_aggr =  roam_delta;
		wlc_roam_prof_update_default(wlc, bsscfg);
		break;
	}

	case IOV_GVAL(IOV_ROAM_MULTI_AP_ENV):
		if (BSSCFG_AP(bsscfg)) {
			err = BCME_NOTSTA;
			break;
		}
		*ret_int_ptr = (int32)roam->multi_ap;
		break;

	case IOV_SVAL(IOV_ROAM_MULTI_AP_ENV):
		if (BSSCFG_AP(bsscfg)) {
			err = BCME_NOTSTA;
			break;
		}

		/* Only Set option is allowed */
		if (!bool_val) {
			err = BCME_BADOPTION;
			break;
		}

		if (!roam->multi_ap) {
			roam->multi_ap = TRUE;
			wlc_roam_prof_update_default(wlc, bsscfg);
		}
		err = BCME_OK;
		break;

	case IOV_SVAL(IOV_ROAM_CI_DELTA):
		/* 8-bit ci_delta uses MSB for enable/disable */
		roam->ci_delta &= ROAM_CACHEINVALIDATE_DELTA_MAX;
		roam->ci_delta |= (uint8)int_val & (ROAM_CACHEINVALIDATE_DELTA_MAX - 1);
		break;

	case IOV_GVAL(IOV_ROAM_CI_DELTA):
		/* 8-bit ci_delta uses MSB for enable/disable */
		*ret_int_ptr = (int32)
			(roam->ci_delta & (ROAM_CACHEINVALIDATE_DELTA_MAX - 1));
		break;

	case IOV_GVAL(IOV_SPLIT_ROAMSCAN):
		*ret_int_ptr = (int32)roam->split_roam_scan;
		break;

	case IOV_SVAL(IOV_SPLIT_ROAMSCAN):
		if (BSSCFG_AP(bsscfg)) {
			err = BCME_NOTSTA;
			break;
		}

		roam->split_roam_scan = bool_val;

		if (!bool_val) {
			/* Reset split roam state */
			roam->split_roam_phase = 0;
		}
		break;

	case IOV_GVAL(IOV_ROAM_CHN_CACHE_LIMIT):
		*ret_int_ptr = (int32)roam->roam_chn_cache_limit;
		break;

	case IOV_SVAL(IOV_ROAM_CHN_CACHE_LIMIT):
		roam->roam_chn_cache_limit = (uint8)int_val;
		break;

	case IOV_GVAL(IOV_ROAM_CHANNELS_IN_CACHE): {
		wl_uint32_list_t *list = (wl_uint32_list_t *)arg;
		uint i;

		if (!BSSCFG_STA(bsscfg)) {
			err = BCME_NOTSTA;
			break;
		}

		if (len < (int)(sizeof(uint32) * (ROAM_CACHELIST_SIZE + 1))) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		list->count = 0;
		if (roam->cache_valid) {
			for (i = 0; i < roam->cache_numentries; i++) {
				list->element[list->count++] =
					roam->cached_ap[i].chanspec;
			}
		}
		break;
	}

	case IOV_GVAL(IOV_ROAM_CHANNELS_IN_HOTLIST): {
		wl_uint32_list_t *list = (wl_uint32_list_t *)arg;
		uint i;

		if (!BSSCFG_STA(bsscfg)) {
			err = BCME_NOTSTA;
			break;
		}

		if (len < (int)(sizeof(uint32) * (WL_NUMCHANSPECS + 1))) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		list->count = 0;
		for (i = 0; i < sizeof(chanvec_t); i++) {
			uint8 chn_list = roam->roam_chn_cache.vec[i];
			uint32 ch_idx = i << 3;
			while (chn_list) {
				if (chn_list & 1) {
					list->element[list->count++] =
						CH20MHZ_CHSPEC(ch_idx);
				}
				ch_idx++;
				chn_list >>= 1;
			}
		}
		break;
	}

	case IOV_GVAL(IOV_ROAMSCAN_PARMS):
	{
		wl_scan_params_t *psrc = bsscfg->roam_scan_params;
		wl_scan_params_t *pdst = arg;
		int reqlen = WL_MAX_ROAMSCAN_DATSZ;

		if (!psrc) {
			err = BCME_NOTREADY;
			break;
		}
		if ((int)len < reqlen) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		/* finally: grab the data we were asked for */
		bcopy(psrc, pdst, reqlen);

		break;
	}
	case IOV_SVAL(IOV_ROAMSCAN_PARMS):
	{
		wl_scan_params_t *psrc = (wl_scan_params_t *)params;
		uint reqlen = WL_MAX_ROAMSCAN_DATSZ;

		/* No check for bad parameters */
		if (p_len > reqlen) {
			err = BCME_BUFTOOLONG;
			break;
		}

		/* first time: allocate memory */
		/* XXX: size of memory allocated must correspond exactly
		 * to memory freed in wlc_bsscfg.c:wlc_bsscfg_mfree().
		 * MUST keep in sync.
		 */
		if (!bsscfg->roam_scan_params) {
			if ((bsscfg->roam_scan_params = (wl_scan_params_t *)
			MALLOCZ(wlc->osh, WL_MAX_ROAMSCAN_DATSZ)) == NULL) {
				err = BCME_NOMEM;
				break;
			}
		}

		/* Too easy: finally */
		bcopy(psrc, bsscfg->roam_scan_params, reqlen);

		break;

	}

#ifdef LPAS
	case IOV_SVAL(IOV_LPAS):
		wlc->lpas = (uint32)int_val;
		if (wlc->lpas) {
			/* for LPAS mode increase the threshold to 8 seconds */
			roam->max_roam_time_thresh = 8000;
			/* In LPAS mode sleep time is up to 2 seconds. Since
			 * wdog must be triggered every sec, it cannot be fed from
			 * tbtt in LPAS mode.
			 */
			wlc->pub->align_wd_tbtt = FALSE;
#ifdef WLC_SW_DIVERSITY
			if (WLSWDIV_ENAB(wlc)) {
				wlc_swdiv_enable_set(wlc->swdiv, FALSE);
			}
#endif /* WLC_SW_DIVERSITY */
			/* Disable scheduling noise measurements in the ucode */
			phy_noise_sched_set(WLC_PI(wlc), PHY_LPAS_MODE, TRUE);
		} else {
			roam->max_roam_time_thresh = wlc->pub->tunables->maxroamthresh;
			wlc->pub->align_wd_tbtt = TRUE;
#ifdef WLC_SW_DIVERSITY
			if (WLSWDIV_ENAB(wlc)) {
				wlc_swdiv_enable_set(wlc->swdiv, TRUE);
			}
#endif /* WLC_SW_DIVERSITY */
			/* Enable scheduling noise measurements in the ucode */
			phy_noise_sched_set(WLC_PI(wlc), PHY_LPAS_MODE, FALSE);
		}
		wlc_watchdog_upd(bsscfg, WLC_WATCHDOG_TBTT(wlc));

		break;
	case IOV_GVAL(IOV_LPAS):
		*ret_int_ptr = wlc->lpas;
		break;
#endif /* LPAS */

#ifdef OPPORTUNISTIC_ROAM
	case IOV_SVAL(IOV_OPPORTUNISTIC_ROAM_OFF):
		roam->oppr_roam_off = bool_val;
		break;

	case IOV_GVAL(IOV_OPPORTUNISTIC_ROAM_OFF):
		*ret_int_ptr = (int32)roam->oppr_roam_off;
		break;
#endif // endif

	case IOV_GVAL(IOV_ROAM_PROF):
	{
		uint32 band_id;
		wlcband_t *band;
		wl_roam_prof_band_t *rp = (wl_roam_prof_band_t *)arg;

		if (!BSSCFG_STA(bsscfg)) {
			err = BCME_NOTSTA;
			break;
		}

		if (!BAND_2G(int_val) && !BAND_5G(int_val)) {
			err = BCME_BADBAND;
			break;
		}
		band_id = BAND_2G(int_val) ? BAND_2G_INDEX : BAND_5G_INDEX;

		band = wlc->bandstate[band_id];
		if (!band || !band->roam_prof) {
			err = BCME_UNSUPPORTED;
			break;
		}

		rp->band = int_val;
		rp->ver = WL_MAX_ROAM_PROF_VER;
		rp->len = WL_MAX_ROAM_PROF_BRACKETS * sizeof(wl_roam_prof_t);
		memcpy(&rp->roam_prof[0], &band->roam_prof[0],
			WL_MAX_ROAM_PROF_BRACKETS * sizeof(wl_roam_prof_t));
	}	break;

	case IOV_SVAL(IOV_ROAM_PROF):
	{
		int i, np;
		uint32 band_id;
		wlcband_t *band;
		wl_roam_prof_band_t *rp = (wl_roam_prof_band_t *)arg;

		if (!BSSCFG_STA(bsscfg)) {
			err = BCME_NOTSTA;
			break;
		}

		if (!BAND_2G(int_val) && !BAND_5G(int_val)) {
			err = BCME_BADBAND;
			break;
		}
		band_id = BAND_2G(int_val) ? BAND_2G_INDEX : BAND_5G_INDEX;

		band = wlc->bandstate[band_id];
		if (!band || !band->roam_prof) {
			err = BCME_UNSUPPORTED;
			break;
		}

		/* Sanity check on size */
		if (rp->ver != WL_MAX_ROAM_PROF_VER) {
			err = BCME_VERSION;
			break;
		}

		if ((rp->len < sizeof(wl_roam_prof_t)) ||
		    (rp->len > WL_MAX_ROAM_PROF_BRACKETS * sizeof(wl_roam_prof_t)) ||
		    (rp->len % sizeof(wl_roam_prof_t))) {
			err = BCME_BADLEN;
			break;
		}

		np = rp->len / sizeof(wl_roam_prof_t);

		if (WBTEXT_ACTIVE(wlc->pub)) {
			/* WBTEXT profile must have non-zero CU */
			if (np != WBTEXT_ALLOWED_PROFS ||
				(rp->roam_prof[0].channel_usage == WL_CU_PERCENTAGE_DISABLE) ||
				(rp->roam_prof[0].roam_delta < WBTEXT_ROAMDELTA_MINVAL) ||
				(rp->roam_prof[0].roam_delta > WBTEXT_ROAMDELTA_MAXVAL)) {
				err = BCME_BADARG;
				break;
			}
		}

		for (i = 0; i < np; i++) {
			/* Sanity check */
			if ((rp->roam_prof[i].roam_trigger <=
				rp->roam_prof[i].rssi_lower) ||
			    (rp->roam_prof[i].nfscan == 0) ||
			    (rp->roam_prof[i].fullscan_period == 0) ||
			    (rp->roam_prof[i].init_scan_period &&
				(rp->roam_prof[i].backoff_multiplier == 0)) ||
			    (rp->roam_prof[i].max_scan_period <
				rp->roam_prof[i].init_scan_period) ||
				(rp->roam_prof[i].channel_usage > WL_CU_PERCENTAGE_MAX) ||
				(!rp->roam_prof[i].cu_avg_calc_dur) ||
				(rp->roam_prof[i].cu_avg_calc_dur >
				WL_CU_CALC_DURATION_MAX)) {
				err = BCME_BADARG;
				break;
			}

			if (i > 0) {
				if ((rp->roam_prof[i].roam_trigger >=
					rp->roam_prof[i-1].roam_trigger) ||
				    (rp->roam_prof[i].rssi_lower >=
					rp->roam_prof[i-1].rssi_lower) ||
				    (rp->roam_prof[i].roam_trigger <
					rp->roam_prof[i-1].rssi_lower)) {
					err = BCME_BADARG;
					break;
				}
			}

			/* Clear default profile flag, and switch to multiple profile mode */
			rp->roam_prof[i].roam_flags &= ~WL_ROAM_PROF_DEFAULT;

			if (i == (np - 1)) {
				/* Last profile can't be crazy */
				rp->roam_prof[i].roam_flags &= ~ WL_ROAM_PROF_LAZY;

				/* Last rssi_lower must be set to min. */
				rp->roam_prof[i].rssi_lower = WLC_RSSI_MINVAL_INT8;

				if (WBTEXT_ENAB(wlc->pub)) {
					/* Last channel_usage must be set to 0 */
					rp->roam_prof[i].channel_usage = WL_CU_PERCENTAGE_DISABLE;
				}
			}
		}

		/* Reset roam profile - it will be adjusted automatically later */
		if (err == BCME_OK) {
			memset(&(band->roam_prof[0]), 0,
				WL_MAX_ROAM_PROF_BRACKETS * sizeof(wl_roam_prof_t));
			memcpy(&band->roam_prof[0], &rp->roam_prof[0], rp->len);
			band->roam_prof_max_idx = np;
			if (BSSCFG_STA(bsscfg) && bsscfg->associated) {
				wlc_roam_prof_update(wlc, bsscfg, TRUE);
			}
		}
	}	break;

	case IOV_SVAL(IOV_ROAM_TIME_THRESH):
		if (int_val <= 0) {
			err = BCME_BADARG;
			break;
		}
		roam->max_roam_time_thresh = (uint32)int_val;
		break;
	case IOV_GVAL(IOV_ROAM_TIME_THRESH):
		*ret_int_ptr = roam->max_roam_time_thresh;
		break;

#ifdef WL_CLIENT_SAE
	case IOV_SVAL(IOV_SCB_ASSOC): {
		wl_ext_auth_evt_t *evt_data = (wl_ext_auth_evt_t *)arg;

		/* Once receive the Assoc Requst trigger
		 * From upper layer, delete the assoc timer
		 */
		wlc_assoc_timer_del(wlc, bsscfg);

		err = wlc_start_assoc(wlc, bsscfg, &evt_data->bssid);
		break;
		}
#endif /* WL_CLIENT_SAE */
	case IOV_SVAL(IOV_JOIN): {
		wlc_ssid_t *ssid = (wlc_ssid_t *)arg;
		wl_join_scan_params_t *scan_params = NULL;
		wl_join_assoc_params_t *assoc_params = NULL;
		int assoc_params_len = 0;
#if !defined(WL_SAE)
		if (bsscfg->auth == WL_AUTH_SAE_KEY) {
			err =  BCME_BADARG;
			break;
		}
#endif /* WL_SAE */

		if ((uint)len < OFFSETOF(wl_extjoin_params_t, assoc)) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		scan_params = &((wl_extjoin_params_t *)arg)->scan;

		if ((uint)len >= WL_EXTJOIN_PARAMS_FIXED_SIZE) {
			assoc_params = &((wl_extjoin_params_t *)arg)->assoc;
			assoc_params_len = len - OFFSETOF(wl_extjoin_params_t, assoc);
		}
		WL_APSTA_UPDN(("wl%d: \"join\" -> wlc_join()\n", wlc->pub->unit));
		err = wlc_join_cmd_proc(wlc, bsscfg, ssid,
		         scan_params,
		         assoc_params, assoc_params_len);
		break;
	}

	case IOV_SVAL(IOV_ROAM_BCNLOSS_OFF):
		roam->roam_bcnloss_off = bool_val;
		break;

	case IOV_GVAL(IOV_ROAM_BCNLOSS_OFF):
		*ret_int_ptr = (int32)roam->roam_bcnloss_off;
		break;

	case IOV_GVAL(IOV_BCN_TO_DLY):
		*ret_int_ptr = (int32)bsscfg->bcn_to_dly;
		break;

	case IOV_SVAL(IOV_BCN_TO_DLY):
		bsscfg->bcn_to_dly = bool_val;
		break;

	case IOV_GVAL(IOV_ROAM_CACHE):
		err = wlc_roam_cache_info_get(bsscfg, params, p_len, arg, len);
		break;

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
} /* wlc_assoc_doiovar */

/* handle join command (ioctl/iovar) */
/* XXX JQL It's not good to modify the input buffer...
 * need to figure out why assoc_params->chanspec_num is overloaded
 * with bssid_cnd and chanspec_num.
 */
int
wlc_join_cmd_proc(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	wlc_ssid_t *ssid, wl_join_scan_params_t *scan_params,
	wl_join_assoc_params_t *assoc_params, int assoc_params_len)
{
	int bcmerror = BCME_OK;

	if (assoc_params != NULL) {
		bool reset_chanspec_num = FALSE;

#ifdef WLRCC
		if (WLRCC_ENAB(wlc->pub) &&
		    (bsscfg->roam->rcc_mode != RCC_MODE_FORCE)) {
			rcc_update_channel_list(bsscfg->roam, ssid, assoc_params);
		}
#endif // endif
		/* When we intend to use bsslist, we set bssid_cnt to the count
		 * of bssids and chanspec (they are the same)
		 * and we set chanspec_num to zero.
		 * This is to make sure that we do not
		 * unintentionally use any uninitialized bssid_cnt
		 * as bssid_cnt is a new field .
		 */
		if ((assoc_params->bssid_cnt) &&
		    (assoc_params->chanspec_num == 0)) {
			reset_chanspec_num = TRUE;
			assoc_params->chanspec_num = assoc_params->bssid_cnt;
		}
#if !defined(BCMQT)	/* bypass channel sanitize in Veloce test for 40/80 MHz BW */
		bcmerror = wlc_assoc_chanspec_sanitize(wlc,
				(chanspec_list_t *)&assoc_params->chanspec_num,
				assoc_params_len - OFFSETOF(wl_assoc_params_t, chanspec_num),
				bsscfg);
#endif /* !BCMQT */
		if (reset_chanspec_num) {
			assoc_params->chanspec_num = 0;
		}
		if (bcmerror != BCME_OK) {
			return bcmerror;
		}
	}

	return wlc_join(wlc, bsscfg, ssid->SSID, ssid->SSID_len,
	                scan_params, assoc_params, assoc_params_len);
}

/* ioctl dispatcher */
static int
wlc_assoc_doioctl(void *ctx, uint32 cmd, void *arg, uint len, struct wlc_if *wlcif)
{
	wlc_assoc_info_t *asi = ctx;
	wlc_info_t *wlc = asi->wlc;
	wlc_bsscfg_t *bsscfg;
	int val = 0, *pval;
	int bcmerror = BCME_OK;
	wlc_assoc_t *as;
	wlc_roam_t *roam;
	uint band;
	struct scb *scb;

	/* update bsscfg pointer */
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	as = bsscfg->assoc;
	roam = bsscfg->roam;

	/* default argument is generic integer */
	pval = (int *) arg;

	/* This will prevent the misaligned access */
	if ((uint32)len >= sizeof(val))
		bcopy(pval, &val, sizeof(val));

	switch (cmd) {

	case WLC_SET_ASSOC_PREFER:
		if (!VALID_BAND(wlc, val)) {
			bcmerror = BCME_BADBAND; /* bad value */
			break;
		}
		bsscfg->join_pref->band = (uint8)val;
		wlc_join_pref_band_upd(bsscfg);
		break;

	case WLC_GET_ASSOC_PREFER:
		if (bsscfg->join_pref)
			*pval = bsscfg->join_pref->band;
		else
			*pval = 0;
		break;

	case WLC_DISASSOC:
		if (!wlc->clk) {
			if (!mboolisset(wlc->pub->radio_disabled, WL_RADIO_MPC_DISABLE)) {
				bcmerror = BCME_NOCLK;
			}
		} else if (BSSCFG_AP(bsscfg)) {
			/* XXX: Disassoc from AP will work only for apdef-stadef builds.
			 * Since this API is under ifdef STA
			*/
			if ((uint)len >= ETHER_ADDR_LEN) {
				struct ether_addr sta_mac;
				bcopy((void *)arg, (void *)&sta_mac, ETHER_ADDR_LEN);
				if (!(scb = wlc_scbfind(wlc, bsscfg, &sta_mac)) ||
					(!SCB_ASSOCIATED(scb))) {
					return BCME_NOTASSOCIATED;
				}
				wlc_senddisassoc(wlc, bsscfg, scb, &scb->ea, &bsscfg->BSSID,
					&bsscfg->cur_etheraddr, DOT11_RC_BUSY);
				if (!SCB_PS(scb)) {
					wlc_scb_resetstate(wlc, scb);
					wlc_scb_setstatebit(wlc, scb, AUTHENTICATED);

					wlc_bss_mac_event(wlc, bsscfg, WLC_E_DISASSOC_IND, &scb->ea,
						WLC_E_STATUS_SUCCESS, DOT11_RC_BUSY, 0, NULL, 0);
				} else {
					wlc_apps_set_change_scb_state(wlc, scb, TRUE);
				}

			} else {
				bcmerror = BCME_BADARG;
			}

		} else {
			/* translate the WLC_DISASSOC command to disabling the config
			 * disassoc packet will be sent if currently associated
			 */
			WL_APSTA_UPDN(("wl%d: WLC_DISASSOC -> wlc_bsscfg_disable()\n",
				wlc->pub->unit));
#ifdef ROBUST_DISASSOC_TX
			as->disassoc_tx_retry = 0;
			wlc_disassoc_tx(bsscfg, TRUE);
#else
			if (wlc_assoc_get_as_cfg(wlc) == bsscfg) {
				wlc_assoc_abort(bsscfg);
			}
			WL_INFORM(("wl%d: disable apcfg if radio in repeater mode \n",
				WLCWLUNIT(wlc)));
			wlc_assoc_disable_apcfg_on_repeater(wlc, bsscfg);
			wlc_bsscfg_disable(wlc, bsscfg);
#endif // endif
#ifdef WL_MIMOPS_CFG
			if (WLC_MIMOPS_ENAB(wlc->pub)) {
				wlc_stf_mimo_ps_clean_up(bsscfg, MIMOPS_CLEAN_UP_DISASSOC);
			}
#endif /* WL_MIMOPS_CFG */
		}
		break;

	case WLC_REASSOC: {
		wl_reassoc_params_t reassoc_params;
		wl_reassoc_params_t *params;
		memset(&reassoc_params, 0, sizeof(wl_reassoc_params_t));

		if (!wlc->pub->up) {
			bcmerror = BCME_NOTUP;
			break;
		}

		if ((uint)len < ETHER_ADDR_LEN) {
			bcmerror = BCME_BUFTOOSHORT;
			break;
		}
		if ((uint)len >= WL_REASSOC_PARAMS_FIXED_SIZE) {
			params = (wl_reassoc_params_t *)arg;
			bcmerror = wlc_assoc_chanspec_sanitize(wlc,
			        (chanspec_list_t *)&params->chanspec_num,
			        len - OFFSETOF(wl_reassoc_params_t, chanspec_num), bsscfg);
			if (bcmerror != BCME_OK)
				break;
		} else {
			bcopy((void *)arg, (void *)&reassoc_params.bssid, ETHER_ADDR_LEN);
			/* scan on all channels */
			reassoc_params.chanspec_num = 0;
			params = &reassoc_params;

			len = (int)WL_REASSOC_PARAMS_FIXED_SIZE;
		}

		if ((bcmerror = wlc_reassoc(bsscfg, params)) != 0) {
			WL_ASSOC_ERROR(("%s: wlc_reassoc fails (%d)\n", __FUNCTION__, bcmerror));
		}
		if ((bcmerror == BCME_BUSY) && (BSSCFG_STA(bsscfg)) && (bsscfg == wlc->cfg)) {
			/* If REASSOC is blocked by other activity,
			   it will retry again from wlc_watchdog
			   so the param info needs to be stored, and return success
			*/
			if (roam->reassoc_param != NULL) {
				MFREE(wlc->osh, roam->reassoc_param, roam->reassoc_param_len);
				roam->reassoc_param = NULL;
			}
			roam->reassoc_param_len = (uint8)len;
			roam->reassoc_param = MALLOC(wlc->osh, len);

			if (roam->reassoc_param != NULL) {
				bcopy((char*)params, (char*)roam->reassoc_param, len);
			}
			bcmerror = 0;
		}
		break;
	}

	case WLC_GET_ROAM_TRIGGER:
		/* bcmerror checking */
		if ((bcmerror = wlc_iocbandchk(wlc, (int*)arg, len, &band, FALSE)))
			break;

		if (band == WLC_BAND_ALL) {
			bcmerror = BCME_BADOPTION;
			break;
		}

		/* Return value for specified band: current or other */
		if ((int)band == wlc->band->bandtype)
			*pval = wlc->band->roam_trigger;
		else
			*pval = wlc->bandstate[OTHERBANDUNIT(wlc)]->roam_trigger;
		break;

	case WLC_SET_ROAM_TRIGGER: {
		int trigger_dbm;

		/* bcmerror checking */
		if ((bcmerror = wlc_iocbandchk(wlc, (int*)arg, len, &band, FALSE)))
			break;

		if ((val < -100) || (val > WLC_ROAM_TRIGGER_MAX_VALUE)) {
			bcmerror = BCME_RANGE;
			break;
		}

		if (WBTEXT_ENAB(wlc->pub)) {
			trigger_dbm = (val < 0) ? val :
				wlc_roam_trigger_logical_dbm(wlc, wlc->band, val);

			ASSERT(wlc->band->roam_prof_max_idx == 2);
			if (wlc->band->roam_prof &&
				(trigger_dbm >= wlc->band->roam_prof[0].roam_trigger)) {
				bcmerror = BCME_RANGE;
				break;
			}
		}

		/* set current band if specified explicitly or implicitly (via "all") */
		if ((band == WLC_BAND_ALL) || ((int)band == wlc->band->bandtype)) {
			/* roam_trigger is either specified as dBm (-1 to -99 dBm) or a
			 * logical value >= 0
			 */
			trigger_dbm = (val < 0) ? val :
			        wlc_roam_trigger_logical_dbm(wlc, wlc->band, val);
			wlc->band->roam_trigger_def = trigger_dbm;
			wlc->band->roam_trigger = trigger_dbm;
		}

		/* set other band if explicit or implicit (via "all") */
		if ((NBANDS(wlc) > 1) &&
		    ((band == WLC_BAND_ALL) || ((int)band != wlc->band->bandtype))) {
			trigger_dbm = (val < 0) ? val : wlc_roam_trigger_logical_dbm(wlc,
				wlc->bandstate[OTHERBANDUNIT(wlc)], val);
			wlc->bandstate[OTHERBANDUNIT(wlc)]->roam_trigger_def = trigger_dbm;
			wlc->bandstate[OTHERBANDUNIT(wlc)]->roam_trigger = trigger_dbm;
		}
		wlc_roam_prof_update_default(wlc, bsscfg);
		break;
	}

	case WLC_GET_ROAM_DELTA:
		/* bcmerror checking */
		if ((bcmerror = wlc_iocbandchk(wlc, (int*)arg, len, &band, FALSE)))
			break;

		if (band == WLC_BAND_ALL) {
			bcmerror = BCME_BADOPTION;
			break;
		}

		/* Return value for specified band: current or other */
		if ((int)band == wlc->band->bandtype)
			*pval = wlc->band->roam_delta;
		else
			*pval = wlc->bandstate[OTHERBANDUNIT(wlc)]->roam_delta;
		break;

	case WLC_SET_ROAM_DELTA:
		/* bcmerror checking */
		if ((bcmerror = wlc_iocbandchk(wlc, (int*)arg, len, &band, FALSE)))
			break;

		if ((val > 100) || (val < 0)) {
			bcmerror = BCME_BADOPTION;
			break;
		}

		/* set current band if specified explicitly or implicitly (via "all") */
		if ((band == WLC_BAND_ALL) || ((int)band == wlc->band->bandtype)) {
			wlc->band->roam_delta_def = val;
			wlc->band->roam_delta = val;
		}

		/* set other band if explicit or implicit (via "all") */
		if ((NBANDS(wlc) > 1) &&
		    ((band == WLC_BAND_ALL) || ((int)band != wlc->band->bandtype))) {
			wlc->bandstate[OTHERBANDUNIT(wlc)]->roam_delta_def = val;
			wlc->bandstate[OTHERBANDUNIT(wlc)]->roam_delta = val;
		}
		wlc_roam_prof_update_default(wlc, bsscfg);
		break;

	case WLC_GET_ROAM_SCAN_PERIOD:
		*pval = roam->partialscan_period;
		break;

	case WLC_SET_ROAM_SCAN_PERIOD:
		roam->partialscan_period = val;
		wlc_roam_prof_update_default(wlc, bsscfg);
		break;

	case WLC_GET_ASSOC_INFO:
		store32_ua((uint8 *)pval, as->req_len);
		pval ++;
		memcpy(pval, as->req, as->req_len);
		pval = (int *)((uint8 *)pval + as->req_len);
		store32_ua((uint8 *)pval, as->resp_len);
		pval ++;
		memcpy(pval, as->resp, as->resp_len);
		break;

	default:
		bcmerror = BCME_UNSUPPORTED;
		break;
	}

	return bcmerror;
} /* wlc_assoc_doioctl */
#endif /* STA */

/* bsscfg cubby */
static int
wlc_assoc_bss_init(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_assoc_info_t *asi = (wlc_assoc_info_t *)ctx;
	wlc_info_t *wlc = asi->wlc;
	wlc_roam_t *roam = NULL;
	wlc_assoc_t *as = NULL;
	int err;

	/* TODO: by design only Infra. STA uses pm states so
	 * need to check !BSSCFG_STA(cfg) before proceed.
	 * need to make sure nothing other than Infra. STA
	 * accesses pm structures though.
	 */

	if ((roam = (wlc_roam_t *)MALLOCZ(wlc->osh, sizeof(wlc_roam_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		err = BCME_NOMEM;
		goto fail;
	}
	cfg->roam = roam;

	if ((as = (wlc_assoc_t *)MALLOCZ(wlc->osh, sizeof(wlc_assoc_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		err = BCME_NOMEM;
		goto fail;
	}
	cfg->assoc = as;

#ifdef STA
	/* TODO: move the check all the way up to before the malloc */
	if (!BSSCFG_STA(cfg)) {
		err = BCME_OK;
		goto fail;
	}

	/* init beacon timeouts */
	roam->bcn_timeout = WLC_BCN_TIMEOUT;

	/* roam scan inits */
	roam->scan_block = 0;
	roam->partialscan_period = WLC_ROAM_SCAN_PERIOD;
	roam->fullscan_period = WLC_FULLROAM_PERIOD;
	roam->ap_environment = AP_ENV_DETECT_NOT_USED;
	roam->motion_timeout = ROAM_MOTION_TIMEOUT;
	roam->nfullscans = ROAM_FULLSCAN_NTIMES;
	roam->ci_delta = ROAM_CACHEINVALIDATE_DELTA;
	roam->roam_chn_cache_limit = WLC_SRS_DEF_ROAM_CHAN_LIMIT;
	roam->max_roam_time_thresh = (uint16) wlc->pub->tunables->maxroamthresh;
	roam->roam_rssi_boost_thresh = WLC_JOIN_PREF_RSSI_BOOST_MIN;

	if (WLRCC_ENAB(wlc->pub)) {
		if ((roam->rcc_channels = (chanspec_t *)
		     MALLOCZ(wlc->osh, sizeof(chanspec_t) * MAX_ROAM_CHANNEL)) == NULL) {
			WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			err = BCME_NOMEM;
			goto fail;
		}
	}

	/* create roam timer */
	if ((roam->timer =
	     wl_init_timer(wlc->wl, wlc_roam_timer_expiry, cfg, "roam")) == NULL) {
		WL_ASSOC_ERROR(("wl%d: wl_init_timer for bsscfg %d roam_timer failed\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg)));
		err = BCME_NORESOURCE;
		goto fail;
	}

	/* create association timer */
	if ((as->timer =
	     wl_init_timer(wlc->wl, wlc_assoc_timeout, cfg, "assoc")) == NULL) {
		WL_ERROR(("wl%d: wl_init_timer for bsscfg %d assoc_timer failed\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg)));
		err = BCME_NORESOURCE;
		goto fail;
	}

	as->recreate_bi_timeout = WLC_ASSOC_RECREATE_BI_TIMEOUT;
	as->listen = WLC_ADVERTISED_LISTEN;

	/* default AP disassoc/deauth timeout */
	as->verify_timeout = WLC_ASSOC_VERIFY_TIMEOUT;

	/* join preference */
	if ((cfg->join_pref = (wlc_join_pref_t *)
	     MALLOCZ(wlc->osh, sizeof(wlc_join_pref_t))) == NULL) {
		WL_ERROR(("wl%d.%d: %s: out of mem, allocated %d bytes\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
		          MALLOCED(wlc->osh)));
		err = BCME_NOMEM;
		goto fail;
	}

	/* init join pref */
	cfg->join_pref->band = WLC_BAND_AUTO;
#endif /* STA */

	return BCME_OK;

fail:
	return err;
}

static void
wlc_assoc_bss_deinit(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_assoc_info_t *asi = (wlc_assoc_info_t *)ctx;
	wlc_info_t *wlc = asi->wlc;

	/* free the association timer */
	if (cfg->assoc != NULL) {
		wlc_assoc_t *as = cfg->assoc;
#ifdef STA
		if (as->state != AS_IDLE) {
			wlc_assoc_abort(cfg);
		}
		if (as->timer != NULL) {
			wl_free_timer(wlc->wl, as->timer);
		}
		if (as->ie != NULL) {
			MFREE(wlc->osh, as->ie, as->ie_len);
		}
		if (as->req != NULL) {
			MFREE(wlc->osh, as->req, as->req_len);
		}
		if (as->resp != NULL) {
			MFREE(wlc->osh, as->resp, as->resp_len);
		}
#endif /* STA */
		MFREE(wlc->osh, as, sizeof(*as));
		cfg->assoc = NULL;
	}

	if (cfg->roam != NULL) {
		wlc_roam_t *roam = cfg->roam;
#ifdef STA
		if (roam->timer != NULL) {
			wl_free_timer(wlc->wl, roam->timer);
		}
		if (WLRCC_ENAB(wlc->pub) && (roam->rcc_channels != NULL)) {
			MFREE(wlc->osh, roam->rcc_channels,
				(sizeof(chanspec_t) * MAX_ROAM_CHANNEL));
		}
#endif /* STA */
		MFREE(wlc->osh, roam, sizeof(*roam));
		cfg->roam = NULL;
	}

#ifdef STA
	/* roam scan params */
	if (cfg->roam_scan_params != NULL) {
		MFREE(wlc->osh, cfg->roam_scan_params, WL_MAX_ROAMSCAN_DATSZ);
		cfg->roam_scan_params = NULL;
	}
	wlc_bsscfg_assoc_params_reset(wlc, cfg);

	if (cfg->join_pref != NULL) {
		MFREE(wlc->osh, cfg->join_pref, sizeof(wlc_join_pref_t));
		cfg->join_pref = NULL;
	}
#endif /* STA */
}

#ifdef WLRSDB
typedef struct {
	wlc_assoc_t *clone_assoc_params; /* pointer to assoc params */
	wlc_roam_t *clone_roam_params; /* pointer to roam params */
	wlc_join_pref_t	*clone_join_pref; /* pointer for join targets sorting preference */
	wlc_bss_info_t	*clone_current_bss; /* BSS parms in ASSOCIATED state needed for FBT */
	assoc_cfg_cubby_t *clone_cfg_cubby;
} wlc_assoc_copy_t;

#define ASSOC_COPY_SIZE	sizeof(wlc_assoc_copy_t)

static int
wlc_assoc_bss_get(void *ctx, wlc_bsscfg_t *cfg, uint8 *data, int *len)
{
#ifdef STA
	if ((cfg->assoc == NULL) && (cfg->roam == NULL) && (cfg->join_pref == NULL)) {
		*len = 0; /* Set not called */
		return BCME_OK;
	}

	/* Sync 'assoc' state & type */
	if (BSSCFG_STA(cfg)) {
		assoc_cfg_cubby_t *cfg_cubby = BSSCFG_ASSOC_CUBBY(cfg->wlc->as, cfg);
		wlc_assoc_copy_t *cp = (wlc_assoc_copy_t *)data;

		ASSERT(cfg_cubby);
		cp->clone_cfg_cubby = cfg_cubby;

		ASSERT(cfg->assoc);
		cp->clone_assoc_params = cfg->assoc;
		if (BSSCFG_IS_FBT(cfg) &&
				(cfg->assoc->type == AS_ROAM) && (cfg->WPA_auth & WPA2_AUTH_FT)) {
			cp->clone_current_bss = cfg->current_bss;
		} else {
			cp->clone_current_bss = NULL;
		}

		ASSERT(cfg->roam);
		cp->clone_roam_params = cfg->roam;

		ASSERT(cfg->join_pref);
		cp->clone_join_pref = cfg->join_pref;

	}
#endif /* STA */
	return BCME_OK;
}

static int
wlc_assoc_bss_set(void *ctx, wlc_bsscfg_t *cfg, const uint8 *data, int len)
{
#ifdef STA
	/* Sync 'assoc' state & type */
	if (BSSCFG_STA(cfg)) {
		const wlc_assoc_copy_t *cp = (const wlc_assoc_copy_t *)data;
		assoc_cfg_cubby_t *cfg_cubby = BSSCFG_ASSOC_CUBBY(cfg->wlc->as, cfg);
		ASSERT(cfg_cubby);

		cfg_cubby->join_attempt_info = cp->clone_cfg_cubby->join_attempt_info;

		if (cfg->assoc && cp->clone_assoc_params) {
			cfg->assoc->type = cp->clone_assoc_params->type;
			cfg->assoc->state = cp->clone_assoc_params->state;
			cfg->assoc->retry_max = cp->clone_assoc_params->retry_max;
			cfg->assoc->listen = cp->clone_assoc_params->listen;
			if (cfg->assoc->ie) {
				MFREE(cfg->wlc->osh, cfg->assoc->ie, cfg->assoc->ie_len);
			}
			cfg->assoc->ie = cp->clone_assoc_params->ie;
			cfg->assoc->ie_len = cp->clone_assoc_params->ie_len;
			cp->clone_assoc_params->ie = NULL;
			cp->clone_assoc_params->ie_len = 0;
			}
		/* We are not copying the entire Roam structure during clone */
		/* hence selected elements are copied */
		if (cfg->roam && cp->clone_roam_params) {
			cfg->roam->bcn_timeout = cp->clone_roam_params->bcn_timeout;
			cfg->roam->assocroam = cp->clone_roam_params->assocroam;
			cfg->roam->off = cp->clone_roam_params->off;
			cfg->roam->minrate_txfail_thresh =
				cp->clone_roam_params->minrate_txfail_thresh;
			cfg->roam->minrate_txpass_thresh =
				cp->clone_roam_params->minrate_txpass_thresh;
			cfg->roam->txpass_thresh = cp->clone_roam_params->txpass_thresh;
			cfg->roam->partialscan_period = cp->clone_roam_params->partialscan_period;
			cfg->roam->fullscan_period = cp->clone_roam_params->fullscan_period;
			cfg->roam->nfullscans = cp->clone_roam_params->nfullscans;
			cfg->roam->motion_rssi_delta = cp->clone_roam_params->motion_rssi_delta;
			cfg->roam->piggyback_enab = cp->clone_roam_params->piggyback_enab;
			cfg->roam->ap_environment = cp->clone_roam_params->ap_environment;
			cfg->roam->nfullscans = cp->clone_roam_params->nfullscans;
			cfg->roam->roam_trigger_aggr = cp->clone_roam_params->roam_trigger_aggr;
			cfg->roam->roam_delta_aggr = cp->clone_roam_params->roam_delta_aggr;
			cfg->roam->multi_ap =
				cfg->roam->multi_ap || cp->clone_roam_params->multi_ap;
			cfg->roam->split_roam_scan = cp->clone_roam_params->split_roam_scan;
			cfg->roam->ci_delta = cp->clone_roam_params->ci_delta;
		#ifdef WLRCC
			if (WLRCC_ENAB(cfg->wlc->pub)) {
				cfg->roam->n_rcc_channels = cp->clone_roam_params->n_rcc_channels;
				memcpy(cfg->roam->rcc_channels, cp->clone_roam_params->rcc_channels,
					sizeof(chanspec_t)*cp->clone_roam_params->n_rcc_channels);
				cfg->roam->rcc_valid = cp->clone_roam_params->rcc_valid;
				cfg->roam->rcc_mode = cp->clone_roam_params->rcc_mode;
			}
		#endif
		}
		ASSERT(cfg->join_pref && cp->clone_join_pref);
		if (cfg->join_pref && cp->clone_join_pref) {
			memcpy(cfg->join_pref, cp->clone_join_pref, sizeof(wlc_join_pref_t));
		}
		if (cp->clone_current_bss) {
			memcpy(cfg->current_bss, cp->clone_current_bss,
					sizeof(*cp->clone_current_bss));
			cp->clone_current_bss->bcn_prb = NULL;
			cp->clone_current_bss->bcn_prb_len = 0;
		}
	}
#endif /* STA */
	return BCME_OK;
}
#endif /* WLRSDB */

#if defined(BCMDBG)
#ifdef STA
static void
wlc_as_bss_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b)
{
	wlc_assoc_t *as;
	BCM_REFERENCE(ctx);

	ASSERT(cfg != NULL);

	as = cfg->assoc;
	if (as == NULL)
		return;

	bcm_bprintf(b, "============= assoc states =============\n");
	bcm_bprintf(b, "type %u state %u flags 0x%x\n", as->type, as->state, as->flags);
	bcm_bprintf(b, "preserved %d recreate_bi_to %u verify_to %u\n",
	            as->preserved, as->recreate_bi_timeout, as->verify_timeout);
	bcm_bprintf(b, "retry_max %u ess_retries %u bss_retries %u\n",
	            as->retry_max, as->ess_retries, as->bss_retries);
}

static void
wlc_roam_bss_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b)
{
	wlc_roam_t *roam;
	BCM_REFERENCE(ctx);

	ASSERT(cfg != NULL);

	roam = cfg->roam;
	if (roam == NULL)
		return;

	bcm_bprintf(b, "============= roam states =============\n");
	bcm_bprintf(b, "off %d\n", roam->off);
	bcm_bprintf(b, "reason %u\n", roam->reason);
	bcm_bprintf(b, "bcn_timeout %u time_since_bcn %u bcns_lost %d\n",
	            roam->bcn_timeout, roam->time_since_bcn, roam->bcns_lost);
	bcm_bprintf(b, "assocroam %d\n", roam->assocroam);
#ifdef BCMDBG
	bcm_bprintf(b, "tbtt_since_bcn %u\n", roam->tbtt_since_bcn);
#endif // endif
}

static void
wlc_join_pref_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b)
{
	uint i;

	BCM_REFERENCE(ctx);

	if ((ctx == NULL) || (cfg == NULL) || (cfg->join_pref == NULL)) {
		return;
	}

	bcm_bprintf(b, "============= join pref =============\n");
	bcm_bprintf(b, "band %d\n", cfg->join_pref->band);
	bcm_bprintf(b, "pref bitmap 0x%x\n", cfg->join_pref->prfbmp);
	for (i = 0; i < cfg->join_pref->fields; i ++) {
		bcm_bprintf(b, "field %d: type %d start %d bits %d\n",
		            i, cfg->join_pref->field[i].type,
		            cfg->join_pref->field[i].start,
		            cfg->join_pref->field[i].bits);
	}
	for (i = 0; i < cfg->join_pref->wpas; i ++) {
		bcm_bprintf(b, "wpa %d:", i);
		bcm_bprhex(b, " akm ", FALSE, cfg->join_pref->wpa[i].akm,
		           sizeof(cfg->join_pref->wpa[i].akm));
		bcm_bprhex(b, " ucipher ", FALSE, cfg->join_pref->wpa[i].ucipher,
		           sizeof(cfg->join_pref->wpa[i].ucipher));
		bcm_bprhex(b, " mcipher ", FALSE, cfg->join_pref->wpa[i].mcipher,
		           sizeof(cfg->join_pref->wpa[i].mcipher));
		bcm_bprintf(b, "\n");
	}
}
#endif /* STA */

static void
wlc_assoc_bss_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b)
{
#ifdef STA
	wlc_as_bss_dump(ctx, cfg, b);
	wlc_roam_bss_dump(ctx, cfg, b);
	wlc_join_pref_dump(ctx, cfg, b);
#endif // endif
}
#endif // endif

#ifdef STA
/* handle bsscfg state change */
static void
wlc_assoc_bss_state_upd(void *ctx, bsscfg_state_upd_data_t *evt)
{
	wlc_assoc_info_t *asi = (wlc_assoc_info_t *)ctx;
	wlc_info_t *wlc = asi->wlc;
	wlc_bsscfg_t *cfg = evt->cfg;

	/* cleanup when downing the bsscfg */
	if (evt->old_up && !cfg->up) {
		int callbacks = 0;

		if (BSSCFG_STA(cfg) && cfg->roam != NULL) {
			/* cancel any roam timer */
			if (cfg->roam->timer != NULL) {
				if (!wl_del_timer(wlc->wl, cfg->roam->timer)) {
					callbacks ++;
				}
			}
			cfg->roam->timer_active = FALSE;

			/* abort any association or roam in progress */
			callbacks += wlc_assoc_abort(cfg);
		}
		if (BSSCFG_STA(cfg) && cfg->assoc != NULL) {
			/* make sure we don't retry */
			if (cfg->assoc->timer != NULL) {
				if (!wl_del_timer(wlc->wl, cfg->assoc->timer)) {
					cfg->assoc->rt = FALSE;
					callbacks ++;
				}
			}
		}

		/* return to the caller */
		evt->callbacks_pending = callbacks;
	}
	/* cleanup when disabling the bsscfg */
	else if (evt->old_enable && !cfg->enable) {
		/* WES FIXME: we need to fix wlc_disassociate_client() to work when down
		 * For now do lame update of associated flags.
		 */
		if (BSSCFG_STA(cfg) && cfg->associated) {
			if (wlc->pub->up) {
				wlc_disassociate_client(cfg, !cfg->assoc->block_disassoc_tx);
			} else {
				wlc_sta_assoc_upd(cfg, FALSE);
			}
		}
	}

	if (BSSCFG_STA(cfg) && !cfg->enable) {
		/* Abort the assocation if cfg is not enabled. */
		wlc_assoc_abort(cfg);
	}
}

/* Challenge Text */
static uint
wlc_auth_calc_chlng_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	wlc_iem_ft_cbparm_t *ftcbparm = data->cbparm->ft;
	BCM_REFERENCE(ctx);

	if (ftcbparm->auth.alg == DOT11_SHARED_KEY) {
		if (ftcbparm->auth.seq == 3) {
			uint8 *chlng;

			chlng = ftcbparm->auth.challenge;
			ASSERT(chlng != NULL);

			return TLV_HDR_LEN + chlng[TLV_LEN_OFF];
		}
	}

	return 0;
}

static int
wlc_auth_write_chlng_ie(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_iem_ft_cbparm_t *ftcbparm = data->cbparm->ft;
	BCM_REFERENCE(ctx);

	if (ftcbparm->auth.alg == DOT11_SHARED_KEY) {
		uint8 *chlng;
		uint16 status;

		status = DOT11_SC_SUCCESS;

		if (ftcbparm->auth.seq == 3) {
			chlng = ftcbparm->auth.challenge;
			ASSERT(chlng != NULL);

			/* write to frame */
			bcopy(chlng, data->buf, 2 + chlng[1]);
#ifdef BCMDBG
			if (WL_ASSOC_ON()) {
				prhex("Auth challenge text #3", chlng, 2 + chlng[1]);
			}
#endif // endif
		}

		ftcbparm->auth.status = status;
	}

	return BCME_OK;
}

static int
wlc_auth_parse_chlng_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;

	if (ftpparm->auth.alg == DOT11_SHARED_KEY) {
		wlc_info_t *wlc = (wlc_info_t *)ctx;
		uint8 *chlng = data->ie;
		uint16 status;

		BCM_REFERENCE(wlc);

		status = DOT11_SC_SUCCESS;

		if (ftpparm->auth.seq == 2) {
			/* What else we need to do in addition to passing it out? */
			if (chlng == NULL) {
				WL_ASSOC(("wl%d: no WEP Auth Challenge\n", wlc->pub->unit));
				status = DOT11_SC_AUTH_CHALLENGE_FAIL;
				goto exit;
			}
			ftpparm->auth.challenge = chlng;
		exit:
			;
		}

		ftpparm->auth.status = status;
		if (status != DOT11_SC_SUCCESS) {
			WL_INFORM(("wl%d: %s: signal to stop parsing IEs, status %u\n",
			           wlc->pub->unit, __FUNCTION__, status));
			return BCME_ERROR;
		}
	}

	return BCME_OK;
}

/* register Auth IE mgmt handlers */
static int
BCMATTACHFN(wlc_auth_register_iem_fns)(wlc_info_t *wlc)
{
	wlc_iem_info_t *iemi = wlc->iemi;
	int err = BCME_OK;

	/* calc/build */
	if ((err = wlc_iem_add_build_fn(iemi, FC_AUTH, DOT11_MNG_CHALLENGE_ID,
	      wlc_auth_calc_chlng_ie_len, wlc_auth_write_chlng_ie, wlc)) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_add_build_fn failed, err %d, chlng in auth\n",
		          wlc->pub->unit, __FUNCTION__, err));
		goto fail;
	}

	/* parse */
	if ((err = wlc_iem_add_parse_fn(iemi, FC_AUTH, DOT11_MNG_CHALLENGE_ID,
	      wlc_auth_parse_chlng_ie, wlc)) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_add_parse_fn failed, err %d, chlng in auth\n",
		          wlc->pub->unit, __FUNCTION__, err));
		goto fail;
	}

	return BCME_OK;

fail:
	return err;
}

/* AssocReq: SSID IE */
static uint
wlc_assoc_calc_ssid_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	wlc_iem_ft_cbparm_t *ftcbparm = data->cbparm->ft;
	wlc_bss_info_t *bi = ftcbparm->assocreq.target;
	BCM_REFERENCE(ctx);
	return TLV_HDR_LEN + bi->SSID_len;
}

static int
wlc_assoc_write_ssid_ie(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_iem_ft_cbparm_t *ftcbparm = data->cbparm->ft;
	wlc_bss_info_t *bi = ftcbparm->assocreq.target;
	BCM_REFERENCE(ctx);
	bcm_write_tlv(DOT11_MNG_SSID_ID, bi->SSID, bi->SSID_len, data->buf);

	return BCME_OK;
}

/* AssocReq: Supported Rates IE */
static uint
wlc_assoc_calc_sup_rates_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	wlc_iem_ft_cbparm_t *ftcbparm = data->cbparm->ft;
	BCM_REFERENCE(ctx);
	if (ftcbparm->assocreq.sup->count == 0)
		return 0;

	return TLV_HDR_LEN + ftcbparm->assocreq.sup->count;
}

static int
wlc_assoc_write_sup_rates_ie(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_iem_ft_cbparm_t *ftcbparm = data->cbparm->ft;
	BCM_REFERENCE(ctx);

	if (ftcbparm->assocreq.sup->count == 0)
		return BCME_OK;

	bcm_write_tlv(DOT11_MNG_RATES_ID, ftcbparm->assocreq.sup->rates,
		ftcbparm->assocreq.sup->count, data->buf);

	return BCME_OK;
}

/* AssocReq: Extended Supported Rates IE */
static uint
wlc_assoc_calc_ext_rates_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	wlc_iem_ft_cbparm_t *ftcbparm = data->cbparm->ft;
	BCM_REFERENCE(ctx);
	if (ftcbparm->assocreq.ext->count == 0)
		return 0;

	return TLV_HDR_LEN + ftcbparm->assocreq.ext->count;
}

static int
wlc_assoc_write_ext_rates_ie(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_iem_ft_cbparm_t *ftcbparm = data->cbparm->ft;
	BCM_REFERENCE(ctx);

	if (ftcbparm->assocreq.ext->count == 0)
		return BCME_OK;

	bcm_write_tlv(DOT11_MNG_EXT_RATES_ID, ftcbparm->assocreq.ext->rates,
		ftcbparm->assocreq.ext->count, data->buf);

	return BCME_OK;
}

/* register AssocReq/ReassocReq IE mgmt handlers */
static int
BCMATTACHFN(wlc_assoc_register_iem_fns)(wlc_info_t *wlc)
{
	wlc_iem_info_t *iemi = wlc->iemi;
	int err = BCME_OK;
	uint16 fstbmp = FT2BMP(FC_ASSOC_REQ) | FT2BMP(FC_REASSOC_REQ);

	/* calc/build */
	/* assocreq/reassocreq */
	if ((err = wlc_iem_add_build_fn_mft(iemi, fstbmp, DOT11_MNG_SSID_ID,
	      wlc_assoc_calc_ssid_ie_len, wlc_assoc_write_ssid_ie, wlc)) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_add_build_fn failed, err %d, ssid ie\n",
		          wlc->pub->unit, __FUNCTION__, err));
		goto fail;
	}
	if ((err = wlc_iem_add_build_fn_mft(iemi, fstbmp, DOT11_MNG_RATES_ID,
	      wlc_assoc_calc_sup_rates_ie_len, wlc_assoc_write_sup_rates_ie, wlc)) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_add_build_fn failed, err %d, sup rates ie\n",
		          wlc->pub->unit, __FUNCTION__, err));
		goto fail;
	}
	if ((err = wlc_iem_add_build_fn_mft(iemi, fstbmp, DOT11_MNG_EXT_RATES_ID,
	    wlc_assoc_calc_ext_rates_ie_len, wlc_assoc_write_ext_rates_ie, wlc)) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_add_build_fn failed, err %d, ext rates ie\n",
		          wlc->pub->unit, __FUNCTION__, err));
		goto fail;
	}

	return BCME_OK;
fail:

	return err;
}

/**
 * These functions register/unregister/invoke the callback
 * when an association state has changed.
 */
int
BCMATTACHFN(wlc_bss_assoc_state_register)(wlc_info_t *wlc,
	bss_assoc_state_fn_t fn, void *arg)
{
	bcm_notif_h hdl = wlc->as->as_st_notif_hdl;

	return bcm_notif_add_interest(hdl, (bcm_notif_client_callback)fn, arg);
}

int
BCMATTACHFN(wlc_bss_assoc_state_unregister)(wlc_info_t *wlc,
	bss_assoc_state_fn_t fn, void *arg)
{
	bcm_notif_h hdl = wlc->as->as_st_notif_hdl;

	return bcm_notif_remove_interest(hdl, (bcm_notif_client_callback)fn, arg);
}

static void
wlc_bss_assoc_state_notif(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint type, uint state)
{
	bcm_notif_h hdl = wlc->as->as_st_notif_hdl;
	bss_assoc_state_data_t notif_data;

	WL_ASSOC(("wl%d.%d: %s: notify clients of assoc state change. type %s state %s.\n",
	          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
	          WLCASTYPEN(type), wlc_as_st_name(state)));

	notif_data.cfg = cfg;
	notif_data.type = type;
	notif_data.state = state;
	bcm_notif_signal(hdl, &notif_data);
}
#else
int
BCMATTACHFN(wlc_bss_assoc_state_register)(wlc_info_t *wlc,
	bss_assoc_state_fn_t fn, void *arg)
{
	return BCME_OK;
}
int
BCMATTACHFN(wlc_bss_assoc_state_unregister)(wlc_info_t *wlc,
	bss_assoc_state_fn_t fn, void *arg)
{
	return BCME_OK;
}
#endif /* STA */

#ifdef STA
/* These functions register/unregister/invoke the callback
 * back when an time_since_bcn is change
 */
int
BCMATTACHFN(wlc_bss_time_since_bcn_notif_register)(wlc_assoc_info_t *as,
	bss_time_since_bcn_notif_fn_t fn, void *arg)
{
	bcm_notif_h hdl = as->time_since_bcn_notif_hdl;

	return bcm_notif_add_interest(hdl, (bcm_notif_client_callback)fn, arg);
}

int
BCMATTACHFN(wlc_bss_time_since_bcn_notif_unregister)(wlc_assoc_info_t *as,
	bss_time_since_bcn_notif_fn_t fn, void *arg)
{
	bcm_notif_h hdl = as->time_since_bcn_notif_hdl;

	return bcm_notif_remove_interest(hdl, (bcm_notif_client_callback)fn, arg);

}

static void
wlc_bss_time_since_bcn_notif(wlc_assoc_info_t *as, wlc_bsscfg_t *cfg, uint time)
{
	bcm_notif_h hdl = as->time_since_bcn_notif_hdl;

	bss_time_since_bcn_notif_data_t notif_data;
	notif_data.cfg = cfg;
	notif_data.time_since_bcn = time;
	bcm_notif_signal(hdl, &notif_data);
}
#endif /* STA */

#if defined(BCMDBG)
static int
wlc_assoc_dump(void *ctx, struct bcmstrbuf *b)
{
	wlc_info_t *wlc = ctx;

#ifdef STA
	/* display bss assoc state change callbacks */
	bcm_bprintf(b, "-------- assoc state change notify list --------\n");
	bcm_notif_dump_list(wlc->as->as_st_notif_hdl, b);
#endif /* STA */

	/* display bss disassoc state change callbacks */
	bcm_bprintf(b, "-------- disassoc state change notify list --------\n");
	bcm_notif_dump_list(wlc->as->dis_st_notif_hdl, b);

	return BCME_OK;
}
#endif // endif

/**
 * These functions register/unregister/invoke the callback
 * when a disassociation/deauthentication has changed.
 */
int
BCMATTACHFN(wlc_bss_disassoc_notif_register)(wlc_info_t *wlc,
	bss_disassoc_notif_fn_t fn, void *arg)
{
	bcm_notif_h hdl = wlc->as->dis_st_notif_hdl;

	return bcm_notif_add_interest(hdl, (bcm_notif_client_callback)fn, arg);
}

int
BCMATTACHFN(wlc_bss_disassoc_notif_unregister)(wlc_info_t *wlc,
	bss_disassoc_notif_fn_t fn, void *arg)
{
	bcm_notif_h hdl = wlc->as->dis_st_notif_hdl;

	return bcm_notif_remove_interest(hdl, (bcm_notif_client_callback)fn, arg);
}

static void
wlc_bss_disassoc_notif(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	uint type, uint state, const struct ether_addr *addr)
{
	bcm_notif_h hdl = wlc->as->dis_st_notif_hdl;
	bss_disassoc_notif_data_t notif_data;

	WL_ASSOC(("wl%d.%d: %s: notify clients of disassoc state change. type %d state %d.\n",
	          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
	          type, state));

	notif_data.cfg = cfg;
	notif_data.type = type;
	notif_data.state = state;
	notif_data.addr = addr;
	bcm_notif_signal(hdl, &notif_data);
}

wlc_bss_info_t *
wlc_assoc_get_join_target(wlc_info_t *wlc, int offset)
{
	wlc_assoc_cmn_t *asc = wlc->as->cmn;
	if (asc->join_targets_last && ((int)asc->join_targets_last + (--offset) >= 0) &&
	    ((int)asc->join_targets_last + offset < (int)asc->join_targets->count)) {
		return asc->join_targets->ptrs[asc->join_targets_last + offset];
	}

	return NULL;
}

void
wlc_assoc_next_join_target(wlc_info_t *wlc)
{
	if (wlc->as->cmn->join_targets_last) {
		wlc->as->cmn->join_targets_last--;
	}
}

void
wlc_assoc_set_as_cfg(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	wlc_assoc_cmn_t *asc = wlc->as->cmn;

	asc->assoc_req[0] = cfg;
}

wlc_bsscfg_t *
wlc_assoc_get_as_cfg(wlc_info_t *wlc)
{
	wlc_assoc_cmn_t *asc = wlc->as->cmn;

	return asc->assoc_req[0];
}

/* module attach/detach interfaces */
wlc_assoc_info_t *
BCMATTACHFN(wlc_assoc_attach)(wlc_info_t *wlc)
{
	wlc_assoc_info_t *asi;
	bsscfg_cubby_params_t cubby_params;
#ifdef STA
	int ret;
#endif // endif
	/* allocate module info */
	if ((asi = MALLOCZ(wlc->osh, sizeof(*asi))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	asi->wlc = wlc;

	/* allocate shared info */
	if ((asi->cmn = obj_registry_get(wlc->objr, OBJR_ASSOCIATION_INFO)) == NULL) {
		if ((asi->cmn = MALLOCZ(wlc->osh, sizeof(*(asi->cmn)))) == NULL) {
			WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			goto fail;
		}
		obj_registry_set(wlc->objr, OBJR_ASSOCIATION_INFO, asi->cmn);
		(void)obj_registry_ref(wlc->objr, OBJR_ASSOCIATION_INFO);
#ifdef STA
		if ((asi->cmn->join_targets =
		     MALLOCZ(wlc->osh, sizeof(wlc_bss_list_t))) == NULL) {
			WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			goto fail;
		}
#endif // endif
	}
	else {
		(void)obj_registry_ref(wlc->objr, OBJR_ASSOCIATION_INFO);
	}

	/* reserve the bsscfg cubby for any bss specific private data */
	bzero(&cubby_params, sizeof(cubby_params));

	cubby_params.context = asi;
	cubby_params.fn_init = wlc_assoc_bss_init;
	cubby_params.fn_deinit = wlc_assoc_bss_deinit;
#if defined(BCMDBG)
	cubby_params.fn_dump = wlc_assoc_bss_dump;
#endif // endif
#ifdef WLRSDB
	cubby_params.fn_get = wlc_assoc_bss_get;
	cubby_params.fn_set = wlc_assoc_bss_set;
	cubby_params.config_size = ASSOC_COPY_SIZE;
#endif /* WLRSDB */

	asi->cfgh = wlc_bsscfg_cubby_reserve_ext(wlc, sizeof(assoc_cfg_cubby_t), &cubby_params);

	if (asi->cfgh < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve_ext failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

#ifdef STA
	if (wlc_bsscfg_state_upd_register(wlc, wlc_assoc_bss_state_upd, asi) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_state_upd_register failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	if (wlc_auth_register_iem_fns(wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_auth_register_iem_fns() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	if (wlc_assoc_register_iem_fns(wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_assoc_register_iem_fns() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* ioctl dispatcher requires an unique "hdl" */
	if (wlc_module_add_ioctl_fn(wlc->pub, asi, wlc_assoc_doioctl, 0, NULL) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_add_ioctl_fn() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* Create notification list for assoc state */
	if (bcm_notif_create_list(wlc->notif, &asi->as_st_notif_hdl) != BCME_OK) {
		WL_ERROR(("wl%d: %s: bcm_notif_create_list failed (asst)\n",
		         wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* Create notification list for time since bcn count */
	if (bcm_notif_create_list(wlc->notif, &asi->time_since_bcn_notif_hdl) != BCME_OK) {
		WL_ERROR(("wl%d: %s: bcm_notif_create_list failed (time since bcn)\n",
		         wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

#endif /* STA */
	/* Create notification list for disassoc state */
	if (bcm_notif_create_list(wlc->notif, &asi->dis_st_notif_hdl) != BCME_OK) {
		WL_ERROR(("wl%d: %s: bcm_notif_create_list failed (asst)\n",
		         wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#ifdef STA
	if (wlc_module_register(wlc->pub, assoc_iovars, "assoc", asi, wlc_assoc_doiovar,
			NULL, NULL, NULL)) {
		WL_ERROR(("wl%d: %s wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#endif /* STA */

#if defined(BCMDBG)
	(void)wlc_dump_register(wlc->pub, "assoc", wlc_assoc_dump, wlc);
#endif // endif

#ifdef STA
	wlc->ibss_coalesce_allowed = TRUE;

	/* register beacon rx notification callback */
	if ((ret = wlc_bss_rx_bcn_register(wlc, wlc_assoc_on_rxbcn, asi))) {
		WL_ERROR(("wl%d: %s wlc_bss_rx_bcn_register() failed %d\n",
			wlc->pub->unit, __FUNCTION__, ret));
		goto fail;
	}
#endif /* STA */

#if defined(RSDB_APSCAN) && !defined(RSDB_APSCAN_DISABLED)
	wlc->pub->cmn->_rsdb_scan |= SCAN_DOWNGRADED_CH_PRUNE_ROAM;
#endif /* RSDB_APSCAN && !RSDB_APSCAN_DISABLED */

	return asi;

fail:
	MODULE_DETACH(asi, wlc_assoc_detach);
	return NULL;
}

void
BCMATTACHFN(wlc_assoc_detach)(wlc_assoc_info_t *asi)
{
	wlc_info_t *wlc;

	if (asi == NULL)
		return;

	wlc = asi->wlc;

#ifdef STA
	/* un-register beacon rx notification callback */
	wlc_bss_rx_bcn_unregister(wlc, wlc_assoc_on_rxbcn, asi);

	(void)wlc_module_unregister(wlc->pub, "assoc", asi);
#endif // endif
	if (asi->dis_st_notif_hdl != NULL)
		bcm_notif_delete_list(&asi->dis_st_notif_hdl);
#ifdef STA
	if (asi->as_st_notif_hdl != NULL)
		bcm_notif_delete_list(&asi->as_st_notif_hdl);
	(void)wlc_module_remove_ioctl_fn(wlc->pub, asi);
	(void)wlc_bsscfg_state_upd_unregister(wlc, wlc_assoc_bss_state_upd, asi);
#endif /* STA */
	if (asi->time_since_bcn_notif_hdl != NULL)
		bcm_notif_delete_list(&asi->time_since_bcn_notif_hdl);

	if (obj_registry_unref(wlc->objr, OBJR_ASSOCIATION_INFO) == 0) {
		obj_registry_set(wlc->objr, OBJR_ASSOCIATION_INFO, NULL);
#ifdef STA
		if (asi->cmn->join_targets != NULL) {
			wlc_bss_list_free(wlc, asi->cmn->join_targets);
			MFREE(wlc->osh, asi->cmn->join_targets, sizeof(wlc_bss_list_t));
		}
#endif // endif
		MFREE(wlc->osh, asi->cmn, sizeof(*(asi->cmn)));
	}

	MFREE(wlc->osh, asi, sizeof(*asi));
}

#ifdef STA
static int
wlc_assoc_homech_req_register(wlc_bsscfg_t *bsscfg)
{
	wlc_info_t *wlc;
	assoc_cfg_cubby_t *assoc_cfg_cubby;
	wlc_msch_req_param_t req;
	int err;
	DBGONLY(char chanbuf[CHANSPEC_STR_LEN]; )

	ASSERT(bsscfg);
	wlc = bsscfg->wlc;

	if (BSSCFG_SPECIAL(wlc, bsscfg) || !bsscfg->associated) {
		return BCME_NOTREADY;
	}

	/* No need to register for unaligned TBTT handling in case of MCHAN scenario, as MSCH would
	 * never be able to schedule this request
	 */
	 if (MCHAN_ACTIVE(wlc->pub)) {
		return BCME_NORESOURCE;
	}

	WL_INFORM(("wl%d.%d: %s: on chanspec %s\n", wlc->pub->unit,
		WLC_BSSCFG_IDX(bsscfg), __FUNCTION__,
		wf_chspec_ntoa(bsscfg->current_bss->chanspec, chanbuf)));

	assoc_cfg_cubby = BSSCFG_ASSOC_CUBBY(wlc->as, bsscfg);
	ASSERT(assoc_cfg_cubby != NULL);

	/* if previous preempt request is still on, return */
	if (assoc_cfg_cubby->msch_homech_req_hdl)
		return BCME_EPERM;

	ASSERT(bsscfg->current_bss);

	memset(&req, 0, sizeof(wlc_msch_req_param_t));
	req.req_type = MSCH_RT_START_FLEX;
	req.flags = 0;
	/* 2 beacon period duration in us unit */
	req.duration = bsscfg->current_bss->beacon_period << 11;
	req.interval = 0;
	req.priority = MSCH_RP_SYNC_FRAME;

	/* will retry after 1 second, if register fails */
	err = wlc_msch_timeslot_register(wlc->msch_info, &bsscfg->current_bss->chanspec, 1,
	      wlc_assoc_homech_req_clbk, bsscfg, &req, &assoc_cfg_cubby->msch_homech_req_hdl);
	if (err != BCME_OK) {
		WL_ERROR(("%s homech req register failed. error %d\n", __FUNCTION__, err));
		assoc_cfg_cubby->mschreg_errcnt++;
	} else {
		wlc_msch_set_chansw_reason(wlc->msch_info, assoc_cfg_cubby->msch_homech_req_hdl,
			CHANSW_HOMECH_REQ);
	}

	return err;
}

void
wlc_assoc_homech_req_unregister(wlc_bsscfg_t *bsscfg)
{
	wlc_info_t *wlc;
	assoc_cfg_cubby_t *assoc_cfg_cubby;
	int err;

	ASSERT(bsscfg);
	wlc = bsscfg->wlc;

	if (BSSCFG_SPECIAL(wlc, bsscfg)) {
		return;
	}

	assoc_cfg_cubby = BSSCFG_ASSOC_CUBBY(wlc->as, bsscfg);
	ASSERT(assoc_cfg_cubby);

	if (assoc_cfg_cubby->msch_homech_req_hdl) {
		err = wlc_msch_timeslot_unregister(wlc->msch_info,
			&assoc_cfg_cubby->msch_homech_req_hdl);
		if (err != BCME_OK) {
			WL_ERROR(("%s homech req unregister failed. error %d\n",
			 __FUNCTION__, err));
			assoc_cfg_cubby->mschunreg_errcnt++;
		}
		assoc_cfg_cubby->msch_homech_req_hdl = NULL;
	}
}

static int
wlc_assoc_homech_req_clbk(void* handler_ctxt, wlc_msch_cb_info_t *cb_info)
{
	wlc_bsscfg_t *cfg = (wlc_bsscfg_t *)handler_ctxt;
	wlc_info_t *wlc;
	uint32 type = cb_info->type;
	assoc_cfg_cubby_t *assoc_cfg_cubby;

	DBGONLY(char chanbuf[CHANSPEC_STR_LEN]; )
	ASSERT(cfg);
	ASSERT(BSSCFG_STA(cfg));

	wlc = cfg->wlc;
	assoc_cfg_cubby = BSSCFG_ASSOC_CUBBY(wlc->as, cfg);
	ASSERT(assoc_cfg_cubby != NULL);
	WL_INFORM(("wl%d.%d: %s: chanspec %s, type 0x%04x\n",
		wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
		wf_chspec_ntoa(cb_info->chanspec, chanbuf), type));

	/* ASSERT START & END combination in same callback */
	ASSERT(((type & (MSCH_CT_ON_CHAN | MSCH_CT_SLOT_START)) == 0) ||
		((type & (MSCH_CT_OFF_CHAN | MSCH_CT_SLOT_END | MSCH_CT_OFF_CHAN_DONE))
		== 0));

	if (type & MSCH_CT_ON_CHAN) {
		if (cfg->up && cfg->associated) {
			wlc_set_uatbtt(cfg, TRUE);
			ASSERT(STAY_AWAKE(wlc));
		} else {
			wlc_assoc_homech_req_unregister(cfg);
		}
	}

	if (type & (MSCH_CT_REQ_END | MSCH_CT_SLOT_END)) {
		/* The msch hdl is not valid, set to NULL */
		assoc_cfg_cubby->msch_homech_req_hdl = NULL;
	}

	return BCME_OK;
}

void
wlc_assoc_homech_req_update(wlc_bsscfg_t *bsscfg)
{
	wlc_info_t *wlc = bsscfg->wlc;
	assoc_cfg_cubby_t *assoc_cfg_cubby;

	if (!BSSCFG_STA(bsscfg) || BSSCFG_SPECIAL(wlc, bsscfg))
		return;

	assoc_cfg_cubby = BSSCFG_ASSOC_CUBBY(wlc->as, bsscfg);
	ASSERT(assoc_cfg_cubby != NULL);

	assoc_cfg_cubby->timestamp_lastbcn = OSL_SYSUPTIME();
	if (assoc_cfg_cubby->msch_homech_req_hdl)
		wlc_assoc_homech_req_unregister(bsscfg);
}

static void
wlc_assoc_on_rxbcn(void *ctx, bss_rx_bcn_notif_data_t *data)
{
	wlc_bsscfg_t *cfg = data->cfg;
	wlc_assoc_homech_req_update(cfg);
}
#endif /* STA */

uint8
wlc_assoc_get_prune_type(wlc_info_t *wlc)
{
	if (wlc->as->cmn->assoc_req[0] &&
		wlc->as->cmn->assoc_req[0]->roam)
		return wlc->as->cmn->assoc_req[0]->roam->prune_type;
	else
		return ROAM_PRUNE_NONE;
}

#ifdef STA
void wlc_roam_update_time_since_bcn(wlc_bsscfg_t *bsscfg, uint8 new_time)
{
	if (bsscfg->roam == NULL) {
		ASSERT(!BSSCFG_INFRA_STA(bsscfg));
		return;
	}

	if (new_time != bsscfg->roam->time_since_bcn) {
		bsscfg->roam->time_since_bcn = new_time;
		wlc_bss_time_since_bcn_notif(bsscfg->wlc->as, bsscfg, new_time*1000);
	}
}
#endif /* STA */

bool
wlc_assoc_is_associating(wlc_bsscfg_t* bsscfg)
{
	return (bsscfg->assoc->state >= AS_SENT_AUTH_1) &&
		!ETHER_ISNULLADDR(&bsscfg->target_bss->BSSID);
}

int8
wlc_assoc_get_as_state(wlc_bsscfg_t *cfg)
{
	if (!BSSCFG_STA(cfg)) {
		return -1;
	} else {
		if (cfg->assoc == NULL) {
			return -1;
		}
		return cfg->assoc->state;
	}
}

#ifdef WL_CLIENT_SAE
int
wlc_ext_auth_tx_complete(wlc_info_t *wlc, wlc_bsscfg_t *cfg, void *pkt,
		void *arg)
{
	/* Once receive the Auth request 1 or 3 from upper layer,
	 * stop the timer and start a new timer once ACK received
	 */
	wlc_assoc_timer_del(wlc, cfg);

	if (wlc_pcb_fn_register(wlc->pcb, wlc_auth_tx_complete,
		arg, pkt)) {
		WL_ERROR(("wl%d: %s out of pkt callbacks\n",
				wlc->pub->unit, __FUNCTION__));
		return BCME_ERROR;
	}
	return BCME_OK;
}
/* Trigger the Assoc from upper layer */
static int
wlc_start_assoc(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, const struct ether_addr *ea)
{
	struct scb *scb = NULL;

	if ((ea == NULL) ||
			ETHER_ISMULTI(ea) || ETHER_ISNULLADDR(ea)) {
		WL_ASSOC(("wl%d: %s:  Wrong bssid, Couldn't start Assoc \n",
				wlc->pub->unit, __FUNCTION__));
		return (BCME_BADARG);
	}

	scb = wlc_scbfind(wlc, bsscfg, ea);

	if (!scb) {
		WL_ASSOC(("wl%d: %s:  Couldn't start Assoc \n",
				wlc->pub->unit, __FUNCTION__));
		wlc_assoc_abort(bsscfg);
		return BCME_NOTREADY;
	}
	wlc_assoc_continue_post_auth1(bsscfg, scb);
	return BCME_OK;
}
#endif /* WL_CLIENT_SAE */

#if defined(SLAVE_RADAR)
static bool
wlc_assoc_do_dfs_reentry_cac_if_required(wlc_bsscfg_t *cfg, wlc_bss_info_t *bi)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	bool dfs_cac_started = FALSE;

	if ((wlc_radar_chanspec(wlc->cmi, bi->chanspec)) &&
		!(wlc_cac_is_clr_chanspec(wlc->dfs, bi->chanspec))) {
		/* DFS cac is required on this channel, start CAC */
		wlc_suspend_mac_and_wait(wlc);
		wlc_set_chanspec(wlc, bi->chanspec, CHANSW_REASON(CHANSW_SCAN));
		wlc_enable_mac(wlc);
		phy_radar_detect_enable((phy_info_t *)WLC_PI(wlc), TRUE);
		wlc_assoc_timer_del(wlc, cfg);
		wl_add_timer(wlc->wl, as->timer,
			(wlc_dfs_get_cactime_ms(wlc->dfs) + DELAY_10MS), FALSE);
		wlc_assoc_change_state(cfg, AS_DFS_CAC_START);
		wlc_set_dfs_cacstate(wlc->dfs, ON, wlc->cfg);
		dfs_cac_started = TRUE;
	}
	return dfs_cac_started;
}
#endif /* SLAVE_RADAR */

#ifdef STA
static void
wlc_assoc_disable_apcfg_on_repeater(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	wlc_bsscfg_t *apcfg = NULL;
	uint8 i = 0;

	ASSERT(wlc != NULL);
	ASSERT(cfg != NULL);

	if ((!wlc->keep_ap_up) && ((AP_ENAB(wlc->pub) && APSTA_ENAB(wlc->pub) &&
		(DWDS_ENAB(cfg) || MAP_ENAB(cfg) ||
#if defined(WET) || defined(WET_DONGLE)
		(WET_ENAB(wlc) || WET_DONGLE_ENAB(wlc)) ||
#endif /* WET || WET_DONGLE */
		(cfg->_psta)) &&
		((cfg->disable_ap_up == FALSE))) ||
		(!cfg->associated))) {
		/* condition where STA interface of Radio started to roam
		 * but could not able to find target BSS's beacon, In this
		 * scenario down all AP interface as MBSS
		 */
		FOREACH_UP_AP(wlc, i, apcfg) {
			if ((apcfg->enable) && (apcfg->up)) {
				wlc_bsscfg_down(wlc, apcfg);
				cfg->disable_ap_up = TRUE;
			}
		}

		wlc_set_dfs_cacstate(wlc->dfs, OFF, wlc->cfg);
		wlc_disassociate_client(cfg, FALSE);
		if (!cfg->roam->off) {
			cfg->roam->active = FALSE;
		}
	}
}
#endif /* STA */
/* ******** WORK IN PROGRESS ******** */
