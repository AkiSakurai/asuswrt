/*
 * AP Module
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
 * $Id: wlc_ap.c 786140 2020-04-17 18:42:52Z $
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>

#ifndef AP
#error "AP must be defined to include this module"
#endif // endif

#include <typedefs.h>
#include <bcmdefs.h>

#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <proto/802.1d.h>
#include <proto/802.11.h>
#include <proto/802.11e.h>
#include <sbconfig.h>
#include <wlioctl.h>
#include <proto/eapol.h>
#include <bcmwpa.h>
#include <bcmcrypto/wep.h>
#include <sbhndpio.h>
#include <sbhnddma.h>
#include <hnddma.h>
#include <bcmdevs.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_ie_misc_hndlrs.h>
#include <wlc_hw.h>
#include <wlc_apps.h>
#include <wlc_scb.h>
#include <wlc_scb_ratesel.h>
#include <wlc_phy_hal.h>
#include <wlc_led.h>
#include <wlc_security.h>
#include <wlc_event.h>
#include <wl_export.h>
#include <wlc_apcs.h>
#include <wlc_stf.h>
#include <wlc_dpt.h>
#include <wlc_ap.h>
#include <wlc_scan.h>
#include <wlc_ampdu_cmn.h>
#include <wlc_amsdu.h>
#ifdef	WLCAC
#include <wlc_cac.h>
#endif // endif
#ifdef WLBTAMP
#include <wlc_bta.h>
#endif // endif
#include <wlc_btcx.h>
#include <wlc_bmac.h>
#ifdef BCMAUTH_PSK
#include <wlc_auth.h>
#endif // endif
#ifdef APCS
#include <wlc_apcs.h>
#endif // endif
#include <wlc_assoc.h>
#ifdef WLMCNX
#include <wlc_mcnx.h>
#endif // endif
#ifdef WLP2P
#include <wlc_p2p.h>
#endif // endif
#ifdef WLMCHAN
#include <wlc_mchan.h>
#endif // endif
#include <wlc_lq.h>
#if defined(PROP_TXSTATUS)
#include <wlfc_proto.h>
#include <wl_wlfc.h>
#endif // endif
#include <wlc_11h.h>
#include <wlc_tpc.h>
#include <wlc_csa.h>
#include <wlc_quiet.h>
#include <wlc_dfs.h>
#include <wlc_prot_g.h>
#include <wlc_prot_n.h>
#include <wlc_11u.h>
#if defined(BCMWAPI_WPI) || defined(BCMWAPI_WAI)
#include <wlc_wapi.h>
#endif // endif
#include <wlc_pcb.h>
#ifdef PSTA
#include <wlc_psta.h>
#endif // endif
#ifdef WLPROPRIETARY_11N_RATES
#include <wlc_ht.h>
#endif // endif
#ifdef WL11AC
#include <wlc_vht.h>
#include <wlc_txbf.h>
#endif // endif
#ifdef TRAFFIC_MGMT
#include <wlc_traffic_mgmt.h>
#endif // endif
#ifdef WL11K_AP
#include <wlc_rrm.h>
#endif /* WL11K_AP */
#ifdef EXT_STA
#define EXTRA_SCAN_HOME_TIME_FOR_AP	20
#endif /* EXT_STA */
#ifdef MFP
#include <wlc_mfp.h>
#endif // endif
#ifdef WLTOEHW
#include <wlc_tso.h>
#endif /* WLTOEHW */
#ifdef WL_RELMCAST
#include "wlc_relmcast.h"
#endif // endif
#include <wlc_ie_mgmt.h>
#include <wlc_ie_mgmt_ft.h>
#include <wlc_ie_mgmt_vs.h>
#ifdef WLWNM
#include <wlc_wnm.h>
#endif // endif
#include <wlc_stamon.h>
#ifdef WDS
#include <wlc_wds.h>
#endif // endif
#include <wlc_macfltr.h>
#ifdef WLAWDL
#include <wlc_awdl.h>
#endif // endif
#include "wlc_txc.h"
#include "wlc_ht.h" /* WLC_HT_FEATURES_PROPRATES_DISAB definition */

#if defined(RXCHAIN_PWRSAVE) || defined(RADIO_PWRSAVE)

#define PWRSAVE_RXCHAIN 1
#define PWRSAVE_RADIO 2

/* Bitmap definitions for rxchain power save */
/* rxchain power save enable */
#define PWRSAVE_ENB		0x01
/* enter power save when no STA associated to AP,
 * this flag is valid under the condition
 * PWRSAVE_ENB is on
 */
#define NONASSOC_PWRSAVE_ENB	0x02

#endif /* RXCHAIN_PWRSAVE or RADIO_PWRSAVE */

/*
 * This is a generic structure for power save implementations
 * Defines parameters for packets per second threshold based power save
 */
typedef struct wlc_pwrsave {
	bool	in_power_save;		/* whether we are in power save mode or not */
	uint8	power_save_check;	/* Whether power save mode check need to be done */
	uint8   stas_assoc_check;	/* check for associated STAs before going to power save */
	uint	in_power_save_counter;	/* how many times in the power save mode */
	uint	in_power_save_secs;	/* how many seconds in the power save mode */
	uint	quiet_time_counter;	/* quiet time before we enter the  power save mode */
	uint	prev_pktcount;		/* total pkt count from the previous second */
	uint	quiet_time;		/* quiet time in the network before we go to power save */
	uint	pps_threshold;		/* pps threshold for power save */
} wlc_pwrsave_t;

typedef struct wlc_rxchain_pwrsave {
#ifdef WL11N
	/* need to save rx_stbc HT capability before enter rxchain_pwrsave mode */
	uint8	ht_cap_rx_stbc;		/* configured rx_stbc HT capability */
#endif // endif
	uint	rxchain;		/* configured rxchains */
	wlc_pwrsave_t pwrsave;
} wlc_rxchain_pwrsave_t;

typedef struct wlc_radio_pwrsave {
	uint8  level;			/* Low, Medium or High Power Savings */
	uint16 on_time;			/* number of  TUs radio is 'on' */
	uint16 off_time;		/* number of  TUs radio is 'off' */
	int radio_disabled;		/* Whether the radio needs to be disabled now */
	uint	pwrsave_state;		/* radio pwr save state */
	int32	tbtt_skip;		/* num of tbtt to skip */
	bool	cncl_bcn;		/* whether to stop bcn or not in level 1 & 2. */
	struct wl_timer *timer;		/* timer to keep track of duty cycle */
	wlc_pwrsave_t pwrsave;
} wlc_radio_pwrsave_t;

#ifdef RXCHAIN_PWRSAVE

#define RXCHAIN_PWRSAVE_ENAB_BRCM_NONHT	1
#define RXCHAIN_PWRSAVE_ENAB_ALL	2

#endif // endif

#ifdef RADIO_PWRSAVE
#define RADIO_PWRSAVE_LOW		0
#define RADIO_PWRSAVE_MEDIUM		1
#define RADIO_PWRSAVE_HIGH		2

#define RADIO_PWRSAVE_TIMER_LATENCY	15
#endif // endif

#ifndef BCM_STA_SUPP_CHAN_ROM_COMPAT
typedef struct wlc_supp_chan_set {
	uint8 first_channel;		/* first channel supported */
	uint8 num_channels;		/* number of channels suuported */
} wlc_supp_chan_set_t;

struct wlc_supp_channels {
	uint8 count;			/* count of supported channels */
	uint8 chanset[MAXCHANNEL/8];	/* bitmap of Supported channel set */
};

#define SCB_SUPP_CHAN_CUBBY(appvt, scb) \
	(wlc_supp_channels_t*)SCB_CUBBY((scb), (appvt)->scb_supp_chan_handle)
#endif /* BCM_STA_SUPP_CHAN_ROM_COMPAT */

/*
* The operational mode capabilities for STAs required to associate
* to the BSS.
* NOTE: The order of values is important. They must be kept in
* increasing order of capabilities, because the enums are compared
* numerically.
* That is: OMC_VHT > OMC_HT > OMC_ERP > OMC_NONE.
*/
typedef enum _opmode_cap_t {

	OMC_NONE = 0, /* no requirements for STA to associate to BSS */

	OMC_ERP = 1, /* STA must advertise ERP (11g) capabilities
				  * to be allowed to associate to 2G band BSS.
				  */
	OMC_HT = 2,	 /* STA must advertise HT (11n) capabilities to
				  * be allowed to associate to the BSS.
				  */
	OMC_VHT = 3, /* Devices must advertise VHT (11ac) capabilities
				  * to be allowed to associate to the BSS.
				  */

	OMC_MAX
} opmode_cap_t;

/* bsscfg cubby */
typedef struct ap_bsscfg_cubby {
	/*
	 * operational mode capabilities
	 * required for STA association
	 * acceptance with the BSS.
	 */
	opmode_cap_t opmode_cap_reqd;
	bool authresp_macfltr;	/* enable/disable suppressing auth resp by MAC filter */
} ap_bsscfg_cubby_t;

#define AP_BSSCFG_CUBBY(__apinfo, __cfg) \
((ap_bsscfg_cubby_t *)BSSCFG_CUBBY((__cfg), (__apinfo)->cfgh))

/* Private AP data structure */
typedef struct
{
	struct wlc_ap_info	appub;		/* Public AP interface: MUST BE FIRST */
	wlc_info_t		*wlc;
	wlc_pub_t		*pub;
	bool			goto_longslot;	/* Goto long slot on next beacon */
	uint			cs_scan_timer;	/* periodic auto channel scan timer (seconds) */
	uint32			maxassoc;	/* Max # associations to allow */
	wlc_radio_pwrsave_t radio_pwrsave;   /* radio duty cycle power save structure */
	uint16			txbcn_inactivity;	/* txbcn inactivity counter */
	uint16			txbcn_snapshot;	/* snapshot of txbcnfrm register */
	int		        cfgh;			/* ap bsscfg cubby handle */
	bool			ap_sta_onradar; /* local AP and STA are on overlapping
						 * radar channel?
						 */
#if defined(EXT_STA) || defined(SPLIT_ASSOC)
	int     scb_handle;     /* scb cubby handle to retrieve data from scb */
#endif /* EXT_STA || SPLIT_ASSOC */
	uint32          pre_tbtt_min_thresh_us; /* Minimum threshold time before TBTT */
	wlc_rxchain_pwrsave_t  rxchain_pwrsave;	/* rxchain reduction power save structure */
	ratespec_t	force_bcn_rspec; /* force setup beacon ratespec (in unit of 500kbps) */
	bool		wlancoex;	/* flags to save WLAN dual 2G radios coex status */
#ifndef BCM_STA_SUPP_CHAN_ROM_COMPAT
	int		scb_supp_chan_handle;	/* To get list of sta supported channels */
#endif /* BCM_STA_SUPP_CHAN_ROM_COMPAT */
} wlc_ap_info_pvt_t;

typedef struct wlc_assoc_req
{
	wlc_ap_info_t *ap;
	wlc_bsscfg_t *bsscfg;
	struct dot11_management_header *hdr;
	uint8 *body;
	uint body_len;
	struct scb *scb;
	bool short_preamble;
	uint len;
	uint16 aid;
	bool akm_ie_included;
	wpa_ie_fixed_t *wpaie;
	bcm_tlv_t *ssid;
	uint8 *pbody;
	void *e_data;
	int e_datalen;
	wlc_rateset_t req_rates;
#if defined(BCMWAPI_WAI) || defined(BCMWAPI_WAI_COMPAT)
	bool wapi_assocreq;
#endif /* BCMWAPI_WAI */
	uint buf_len;
	uint16 status;
} wlc_assoc_req_t;

/* IOVar table */

/* Parameter IDs, for use only internally to wlc -- in the wlc_ap_iovars
 * table and by the wlc_ap_doiovar() function.  No ordering is imposed:
 * the table is keyed by name, and the function uses a switch.
 */
enum {
	IOV_AP_ISOLATE = 1,
	IOV_SCB_ACTIVITY_TIME,
	IOV_AUTHE_STA_LIST,
	IOV_AUTHO_STA_LIST,
	IOV_WME_STA_LIST,
	IOV_BSS,
	IOV_MBSS,
	IOV_APCSCHSPEC,
	IOV_MAXASSOC,
	IOV_BSS_MAXASSOC,
	IOV_CLOSEDNET,
	IOV_AP,
	IOV_APSTA,		/* enable simultaneously active AP/STA */
	IOV_TXBURST_LIM_OVERRIDE, /* set the advertised tx burst limit */
	IOV_PREF_CHANSPEC,      /* User supplied chanspec, for when we're not using AutoChannel */
	IOV_BCN_ROTATE,	/* enable/disable beacon rotation */
	IOV_AP_ASSERT,		/* User forced crash */
#ifdef RXCHAIN_PWRSAVE
	IOV_RXCHAIN_PWRSAVE_ENABLE,		/* Power Save with single rxchain enable */
	IOV_RXCHAIN_PWRSAVE_QUIET_TIME,		/* Power Save with single rxchain quiet time */
	IOV_RXCHAIN_PWRSAVE_PPS,		/* single rxchain packets per second */
	IOV_RXCHAIN_PWRSAVE,		/* Current power save mode */
	IOV_RXCHAIN_PWRSAVE_STAS_ASSOC_CHECK,	/* Whether to check for associated stas */
#endif // endif
#ifdef RADIO_PWRSAVE
	IOV_RADIO_PWRSAVE_ENABLE,		/* Radio duty cycle Power Save enable */
	IOV_RADIO_PWRSAVE_QUIET_TIME,		/* Radio duty cycle Power Save */
	IOV_RADIO_PWRSAVE_PPS,		/* Radio power save packets per second */
	IOV_RADIO_PWRSAVE,		/* Whether currently in power save or not */
	IOV_RADIO_PWRSAVE_LEVEL,		/* Radio power save duty cycle on time */
	IOV_RADIO_PWRSAVE_STAS_ASSOC_CHECK,	/* Whether to check for associated stas */
#endif // endif
	IOV_AP_RESET,	/* User forced reset */
	IOV_BCMDCS, 	/* dynamic channel switch (management) */
	IOV_DYNBCN,	/* Dynamic beaconing */
	IOV_SCB_LASTUSED,	/* time (s) elapsed since any of the associated scb is used */
	IOV_SCB_PROBE,		/* get/set scb probe parameters */
	IOV_SCB_ASSOCED,	/* if it has associated SCBs at phy if level */
	IOV_ACS_UPDATE,		/* update after acs_scan and chanspec selection */
	IOV_BEACON_INFO,	/* get beacon management packet */
	IOV_PROBE_RESP_INFO,	/* get probe response management packet */
	IOV_MODE_REQD,		/*
				 * operational mode capabilities required for STA
				 * association acceptance with the BSS
				 */
	IOV_PSPRETEND_THRESHOLD,
	IOV_PSPRETEND_RETRY_LIMIT,
	IOV_BSS_RATESET,	/* Set rateset per BSS */
	IOV_FORCE_BCN_RSPEC,	/* Setup Beacon rate from lowest basic to specific basic rate */
	IOV_WLANCOEX,		/* Setup dual radio usbap coex function on/off */
	IOV_SCB_MAX_PROBE,
#ifdef WLAUTHRESP_MAC_FILTER
	IOV_AUTHRESP_MACFLTR,	/* enable/disable suppressing auth resp by MAC filter */
#endif /* WLAUTHRESP_MAC_FILTER */
	IOV_PROXY_ARP_ADVERTISE,    /* Update beacon, probe response frames for proxy arp bit */
#ifdef	MULTIAP
	IOV_MAP,		/* Set/Unset multiap. */
#endif	/* MULTIAP */
	IOV_LAST		/* In case of a need to check max ID number */

};

/* AP IO Vars */
static const bcm_iovar_t wlc_ap_iovars[] = {
	{"ap", IOV_AP,
	0, IOVT_INT32, 0
	},
	{"ap_isolate", IOV_AP_ISOLATE,
	(0), IOVT_BOOL, 0
	},
	{"scb_activity_time", IOV_SCB_ACTIVITY_TIME,
	(IOVF_NTRL), IOVT_UINT32, 0
	},
	{"authe_sta_list", IOV_AUTHE_STA_LIST,
	(IOVF_SET_UP), IOVT_BUFFER, sizeof(uint32)
	},
	{"autho_sta_list", IOV_AUTHO_STA_LIST,
	(IOVF_SET_UP), IOVT_BUFFER, sizeof(uint32)
	},
	{"wme_sta_list", IOV_WME_STA_LIST,
	(0), IOVT_BUFFER, sizeof(uint32)
	},
	{"maxassoc", IOV_MAXASSOC,
	(IOVF_WHL), IOVT_UINT32, 0
	},
	{"bss_maxassoc", IOV_BSS_MAXASSOC,
	(IOVF_NTRL), IOVT_UINT32, 0
	},
	{"bss", IOV_BSS,
	(0), IOVT_INT32, 0
	},
	{"closednet", IOV_CLOSEDNET,
	(0), IOVT_BOOL, 0
	},
	{"mbss", IOV_MBSS,
	(IOVF_SET_DOWN), IOVT_BOOL, 0,
	},
	{"apcschspec", IOV_APCSCHSPEC,
	(0), IOVT_UINT16, 0
	},
#ifdef RXCHAIN_PWRSAVE
	{"rxchain_pwrsave_enable", IOV_RXCHAIN_PWRSAVE_ENABLE,
	(0), IOVT_UINT8, 0
	},
	{"rxchain_pwrsave_quiet_time", IOV_RXCHAIN_PWRSAVE_QUIET_TIME,
	(0), IOVT_UINT32, 0
	},
	{"rxchain_pwrsave_pps", IOV_RXCHAIN_PWRSAVE_PPS,
	(0), IOVT_UINT32, 0
	},
	{"rxchain_pwrsave", IOV_RXCHAIN_PWRSAVE,
	(0), IOVT_UINT8, 0
	},
	{"rxchain_pwrsave_stas_assoc_check", IOV_RXCHAIN_PWRSAVE_STAS_ASSOC_CHECK,
	(0), IOVT_UINT8, 0
	},
#endif /* RXCHAIN_PWRSAVE */
#ifdef RADIO_PWRSAVE
	{"radio_pwrsave_enable", IOV_RADIO_PWRSAVE_ENABLE,
	(0), IOVT_UINT8, 0
	},
	{"radio_pwrsave_quiet_time", IOV_RADIO_PWRSAVE_QUIET_TIME,
	(0), IOVT_UINT32, 0
	},
	{"radio_pwrsave_pps", IOV_RADIO_PWRSAVE_PPS,
	(0), IOVT_UINT32, 0
	},
	{"radio_pwrsave_level", IOV_RADIO_PWRSAVE_LEVEL,
	(0), IOVT_UINT8, 0
	},
	{"radio_pwrsave", IOV_RADIO_PWRSAVE,
	(0), IOVT_UINT8, 0
	},
	{"radio_pwrsave_stas_assoc_check", IOV_RADIO_PWRSAVE_STAS_ASSOC_CHECK,
	(0), IOVT_UINT8, 0
	},
#endif /* RADIO_PWRSAVE */
#if defined(STA) /* APSTA */
	{"apsta", IOV_APSTA,
	(IOVF_SET_DOWN), IOVT_BOOL, 0,
	},
#endif /* APSTA */
	{"pref_chanspec", IOV_PREF_CHANSPEC,
	(0), IOVT_UINT16, 0
	},
	{"bcn_rotate", IOV_BCN_ROTATE,
	(0), IOVT_BOOL, 0
	},
#ifdef BCM_DCS
	{"bcm_dcs", IOV_BCMDCS,
	(0), IOVT_BOOL, 0
	},
#endif /* BCM_DCS */
	{"dynbcn", IOV_DYNBCN,
	(0), IOVT_BOOL, 0
	},
	{"scb_lastused", IOV_SCB_LASTUSED, (0), IOVT_UINT32, 0},
	{"scb_probe", IOV_SCB_PROBE,
	(IOVF_SET_UP), IOVT_BUFFER, sizeof(wl_scb_probe_t)
	},
	{"scb_assoced", IOV_SCB_ASSOCED, (0), IOVT_BOOL, 0},
	{"acs_update", IOV_ACS_UPDATE,
	(IOVF_SET_UP), IOVT_BOOL, 0
	},
	{"beacon_info", IOV_BEACON_INFO,
	(IOVF_OPEN_ALLOW), IOVT_BUFFER, sizeof(uint32),
	},
	{"probe_resp_info", IOV_PROBE_RESP_INFO,
	(IOVF_OPEN_ALLOW), IOVT_BUFFER, sizeof(uint32),
	},
	{"mode_reqd", IOV_MODE_REQD,
	(IOVF_OPEN_ALLOW), IOVT_UINT8, 0
	},
#ifdef PSPRETEND
	{"pspretend_threshold", IOV_PSPRETEND_THRESHOLD,
	(0), IOVT_UINT8, 0
	},
	{"pspretend_retry_limit", IOV_PSPRETEND_RETRY_LIMIT,
	(0), IOVT_UINT8, 0
	},
#endif // endif
	{"bss_rateset", IOV_BSS_RATESET,
	(0), IOVT_INT32, 0
	},
	{"force_bcn_rspec", IOV_FORCE_BCN_RSPEC,
	(0), IOVT_UINT8, 0
	},
#ifdef USBAP
	{"wlancoex", IOV_WLANCOEX,
	(0), IOVT_BOOL, 0
	},
#endif // endif
	{"scb_max_probe", IOV_SCB_MAX_PROBE,
	(IOVF_NTRL), IOVT_UINT32, 0
	},
#ifdef WLAUTHRESP_MAC_FILTER
	{"authresp_mac_filter", IOV_AUTHRESP_MACFLTR,
	(0), IOVT_BOOL, 0
	},
#endif /* WLAUTHRESP_MAC_FILTER */
	{"proxy_arp_advertise", IOV_PROXY_ARP_ADVERTISE,
	(0), IOVT_BOOL, 0
	},
#ifdef MULTIAP
	{"map", IOV_MAP, (IOVF_SET_DOWN), IOVT_UINT8, 0},
#endif	/* MULTIAP */
	{NULL, 0, 0, 0, 0 }
};

/* Local Prototypes */
static int wlc_ap_watchdog(void *arg);
static int wlc_ap_wlc_up(void *ctx);
static void wlc_assoc_notify(wlc_ap_info_t *ap, struct ether_addr *sta, struct wlc_if *wlif);
static void wlc_reassoc_notify(wlc_ap_info_t *ap, struct ether_addr *sta, struct wlc_if *wlif);
static void wlc_ap_sta_probe(wlc_ap_info_t *ap, struct scb *scb);
static void wlc_ap_sta_probe_complete(wlc_info_t *wlc, uint txstatus, struct scb *scb, void *pkt);
static void wlc_ap_stas_timeout(wlc_ap_info_t *ap);
static void wlc_ap_wsec_stas_timeout(wlc_ap_info_t *ap);
static int wlc_authenticated_sta_check_cb(struct scb *scb);
static int wlc_authorized_sta_check_cb(struct scb *scb);
static int wlc_wme_sta_check_cb(struct scb *scb);
static int wlc_sta_list_get(wlc_ap_info_t *ap, wlc_bsscfg_t *cfg, uint8 *buf,
                            int len, int (*sta_check)(struct scb *scb));
#ifdef BCMDBG
static int wlc_dump_ap(wlc_ap_info_t *ap, struct bcmstrbuf *b);
#ifdef RXCHAIN_PWRSAVE
static int wlc_dump_rxchain_pwrsave(wlc_ap_info_t *ap, struct bcmstrbuf *b);
#endif // endif
#ifdef RADIO_PWRSAVE
static int wlc_dump_radio_pwrsave(wlc_ap_info_t *ap, struct bcmstrbuf *b);
#endif // endif
#if defined(RXCHAIN_PWRSAVE) || defined(RADIO_PWRSAVE)
static void wlc_dump_pwrsave(wlc_pwrsave_t *pwrsave, struct bcmstrbuf *b);
#endif // endif
#endif /* BCMDBG */

static int wlc_ap_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif);
static int wlc_ap_ioctl(void *hdl, int cmd, void *arg, int len, struct wlc_if *wlcif);

static void wlc_ap_up_upd(wlc_ap_info_t *ap, wlc_bsscfg_t *bsscfg, bool state);

static void wlc_ap_acs_update(wlc_info_t *wlc);
static int wlc_ap_set_opmode_cap_reqd(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	opmode_cap_t opmode);
static opmode_cap_t wlc_ap_get_opmode_cap_reqd(wlc_info_t* wlc, wlc_bsscfg_t *bsscfg);
static int wlc_ap_bsscfg_init(void *context, wlc_bsscfg_t *cfg);

#if defined(RXCHAIN_PWRSAVE) || defined(RADIO_PWRSAVE)
#ifdef RXCHAIN_PWRSAVE
static void wlc_disable_pwrsave(wlc_ap_info_t *ap, int type);
#ifdef WDS
static bool wlc_rxchain_wds_detection(wlc_info_t *wlc);
#endif /* WDS */
#endif /* RXCHAIN_PWRSAVE */
static void wlc_reset_pwrsave_mode(wlc_ap_info_t *ap, int type);
static void wlc_pwrsave_mode_check(wlc_ap_info_t *ap, int type);
#endif /* RXCHAIN_PWRSAVE || RADIO_PWRSAVE */
#ifdef RADIO_PWRSAVE
static void wlc_radio_pwrsave_update_quiet_ie(wlc_ap_info_t *ap, uint8 count);
static void wlc_reset_radio_pwrsave_mode(wlc_ap_info_t *ap);
static void wlc_radio_pwrsave_off_time_start(wlc_ap_info_t *ap);
static void wlc_radio_pwrsave_timer(void *arg);
#endif /* RADIO_PWRSAVE */

#ifdef MBSS
/* MBSS16 */
static void wlc_ap_mbss16_write_beacon(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
static void wlc_mbss16_setup(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
static void wlc_mbss16_upd_closednet(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
#endif /* MBSS */

#if defined(EXT_STA) || defined(SPLIT_ASSOC)
#define SCB_ASSOC_CUBBY(appvt, scb) (wlc_assoc_req_t*)SCB_CUBBY((scb), (appvt)->scb_handle)
static void wlc_ap_scb_free_notify(void *context, struct scb *scb);
#endif /* EXT_STA || SPLIT_ASSOC */

static void wlc_ap_probe_complete(wlc_info_t *wlc, void *pkt, uint txs);

/* IE mgmt */
static uint wlc_assoc_calc_sup_rates_ie_len(void *ctx, wlc_iem_calc_data_t *data);
static int wlc_assoc_write_sup_rates_ie(void *ctx, wlc_iem_build_data_t *data);
static uint wlc_assoc_calc_ext_rates_ie_len(void *ctx, wlc_iem_calc_data_t *data);
static int wlc_assoc_write_ext_rates_ie(void *ctx, wlc_iem_build_data_t *data);
static int wlc_assoc_parse_ssid_ie(void *ctx, wlc_iem_parse_data_t *data);
static int wlc_assoc_parse_sup_rates_ie(void *ctx, wlc_iem_parse_data_t *data);
static int wlc_assoc_parse_ext_rates_ie(void *ctx, wlc_iem_parse_data_t *data);
#ifndef BCM_STA_SUPP_CHAN_ROM_COMPAT
static int wlc_assoc_parse_supp_chan_ie(void *ctx, wlc_iem_parse_data_t *data);
#endif /* BCM_STA_SUPP_CHAN_ROM_COMPAT */
static int wlc_assoc_parse_wps_ie(void *ctx, wlc_iem_parse_data_t *data);
#ifdef WL11N
static int wlc_ht_ap_coex_scan_parse_cap_ie(void *ctx, wlc_iem_parse_data_t *data);
static int wlc_ht_ap_coex_bcn_parse_cap_ie(void *ctx, wlc_iem_parse_data_t *data);
#endif // endif
static bool wlc_ap_on_radarchan(wlc_ap_info_t *ap);
static int wlc_force_bcn_rspec_upd(wlc_info_t *wlc, wlc_bsscfg_t *cfg, wlc_ap_info_t *ap,
	ratespec_t rspec);
#ifdef USBAP
static int wlc_wlancoex_upd(wlc_info_t *wlc, wlc_ap_info_t *ap, bool coex_en);
#endif /* USBAP */
#ifdef WL_SAE
static uint wlc_sae_parse_auth(wlc_bsscfg_t *cfg, struct dot11_management_header *hdr,
	uint8 *body, uint body_len);
#endif /* WL_SAE */
#ifdef WL_GLOBAL_RCLASS
static int wlc_assoc_parse_regclass_ie(void *ctx, wlc_iem_parse_data_t *data);
static void wlc_ap_scb_state_upd_cb(void *ctx, scb_state_upd_data_t *notif_data);
#endif /* WL_GLOBAL_RCLASS */
/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

wlc_ap_info_t*
BCMATTACHFN(wlc_ap_attach)(wlc_info_t *wlc)
{
	int err = 0;
	wlc_ap_info_pvt_t *appvt;
	wlc_ap_info_t* ap;
	wlc_pub_t *pub = wlc->pub;
	uint16 arsfstbmp = FT2BMP(FC_ASSOC_RESP) | FT2BMP(FC_REASSOC_RESP);
	uint16 arqfstbmp = FT2BMP(FC_ASSOC_REQ) | FT2BMP(FC_REASSOC_REQ);
#ifdef WL11N
	uint16 scanfstbmp = FT2BMP(WLC_IEM_FC_SCAN_BCN) | FT2BMP(WLC_IEM_FC_SCAN_PRBRSP);
#endif // endif
#ifdef RXCHAIN_PWRSAVE
	char *var;
#endif /* RXCHAIN_PWRSAVE */

	wlc->scb_timeout = SCB_TIMEOUT;
	wlc->scb_activity_time = SCB_ACTIVITY_TIME;
	wlc->scb_max_probe = SCB_GRACE_ATTEMPTS;

	appvt = (wlc_ap_info_pvt_t*)MALLOC(pub->osh, sizeof(wlc_ap_info_pvt_t));
	if (appvt == NULL) {
		WL_ERROR(("%s: MALLOC wlc_ap_info_pvt_t failed\n", __FUNCTION__));
		goto fail;
	}

	bzero(appvt, sizeof(wlc_ap_info_pvt_t));

	ap = &appvt->appub;
	ap->shortslot_restrict = FALSE;

	appvt->wlc = wlc;
	appvt->pub = pub;
	appvt->maxassoc = wlc_ap_get_maxassoc_limit(ap);

	/* reserve cubby in the bsscfg container for per-bsscfg private data */
	if ((appvt->cfgh = wlc_bsscfg_cubby_reserve(wlc, sizeof(ap_bsscfg_cubby_t),
		wlc_ap_bsscfg_init, NULL, NULL,	(void *)appvt)) < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	if (wlc_apps_attach(wlc)) {
		WL_ERROR(("%s: wlc_apps_attach failed\n", __FUNCTION__));
		goto fail;
	}

#ifdef RXCHAIN_PWRSAVE
#ifdef BCMDBG
	wlc_dump_register(pub, "rxchain_pwrsave", (dump_fn_t)wlc_dump_rxchain_pwrsave, (void *)ap);
#endif // endif
	var = getvar(NULL, "wl_nonassoc_rxchain_pwrsave_enable");
	if (var) {
		if (!bcm_strtoul(var, NULL, 0))
			appvt->rxchain_pwrsave.pwrsave.power_save_check &= ~NONASSOC_PWRSAVE_ENB;
		else
			appvt->rxchain_pwrsave.pwrsave.power_save_check |= NONASSOC_PWRSAVE_ENB;
	}
#endif /* RXCHAIN_PWRSAVE */

#ifdef RADIO_PWRSAVE
	if (!(appvt->radio_pwrsave.timer =
		wl_init_timer(wlc->wl, wlc_radio_pwrsave_timer, ap, "radio_pwrsave"))) {
		WL_ERROR(("%s: wl_init_timer for radio powersave timer failed\n", __FUNCTION__));
		goto fail;
	}
#ifdef BCMDBG
	wlc_dump_register(pub, "radio_pwrsave", (dump_fn_t)wlc_dump_radio_pwrsave, (void *)ap);
#endif // endif
#endif /* RADIO_PWRSAVE */

#ifdef BCMDBG
	wlc_dump_register(pub, "ap", (dump_fn_t)wlc_dump_ap, (void *)ap);
#endif // endif
#ifdef WL_GLOBAL_RCLASS
	/* Add client callback to the scb state notification list */
	if (wlc_scb_state_upd_register(wlc,
			(bcm_notif_client_callback)wlc_ap_scb_state_upd_cb, wlc) != BCME_OK) {
		WL_ERROR(("wl%d:%s: unable to register callback %p\n",
		        wlc->pub->unit, __FUNCTION__, wlc_ap_scb_state_upd_cb));
		goto fail;
	}
#endif // endif

	err = wlc_module_register(pub, wlc_ap_iovars, "ap", appvt, wlc_ap_doiovar,
	                          wlc_ap_watchdog, wlc_ap_wlc_up, NULL);
	if (err) {
		WL_ERROR(("%s: wlc_module_register failed\n", __FUNCTION__));
		goto fail;
	}

	err = wlc_module_add_ioctl_fn(wlc->pub, (void *)ap, wlc_ap_ioctl, 0, NULL);
	if (err) {
		WL_ERROR(("%s: wlc_module_add_ioctl_fn err=%d\n",
		          __FUNCTION__, err));
		goto fail;
	}

#if defined(EXT_STA) || defined(SPLIT_ASSOC)
	appvt->scb_handle = wlc_scb_cubby_reserve(wlc, sizeof(wlc_assoc_req_t),
#if defined(BCM_HOST_MEM_RESTORE) && defined(BCM_HOST_MEM_SCB)
		NULL, wlc_ap_scb_free_notify, NULL, appvt, 0);
#else
		NULL, wlc_ap_scb_free_notify, NULL, appvt);
#endif /* (BCM_HOST_MEM_RESTORE) && (BCM_HOST_MEM_SCB) */

	if (appvt->scb_handle < 0) {
		WL_ERROR(("%s: wlc_scb_cubby_reserve failed\n", __FUNCTION__));
		goto fail;
	}
#endif /* (EXT_STA) || defined (SPLIT_ASSOC) */

#ifndef BCM_STA_SUPP_CHAN_ROM_COMPAT
	appvt->scb_supp_chan_handle = wlc_scb_cubby_reserve(wlc,
		sizeof(wlc_supp_channels_t), NULL, NULL, NULL, appvt);

	if (appvt->scb_supp_chan_handle < 0) {
		WL_ERROR(("%s: wlc_scb_cubby_reserve failed\n", __FUNCTION__));
		goto fail;
	}
#endif /* BCM_STA_SUPP_CHAN_ROM_COMPAT */

	/* register packet class callback */
	err = wlc_pcb_fn_set(wlc->pcb, 0, WLF2_PCB1_STA_PRB, wlc_ap_probe_complete);
	if (err != BCME_OK) {
		WL_ERROR(("%s: wlc_pcb_fn_set err=%d\n", __FUNCTION__, err));
		goto fail;
	}
#ifdef WL_GLOBAL_RCLASS
	if (wlc_iem_add_parse_fn_mft(wlc->iemi, arqfstbmp, DOT11_MNG_REGCLASS_ID,
		wlc_assoc_parse_regclass_ie, wlc) != BCME_OK) {

		WL_ERROR(("wl%d: %s wlc_iem_vs_add_parse_fn failed, regclass in assocreq\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#endif /* WL_GLOBAL_RCLASS */

	/* register IE mgmt callback */
	/* calc/build */
	/* assocresp/reassocresp */
	if (wlc_iem_add_build_fn_mft(wlc->iemi, arsfstbmp, DOT11_MNG_RATES_ID,
	      wlc_assoc_calc_sup_rates_ie_len, wlc_assoc_write_sup_rates_ie, wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_add_build_fn failed, sup rates in assocresp\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	if (wlc_iem_add_build_fn_mft(wlc->iemi, arsfstbmp, DOT11_MNG_EXT_RATES_ID,
	      wlc_assoc_calc_ext_rates_ie_len, wlc_assoc_write_ext_rates_ie, wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_add_build_fn failed, ext rates in assocresp\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	/* parse */
	/* assocreq/reassocreq */
	if (wlc_iem_add_parse_fn_mft(wlc->iemi, arqfstbmp, DOT11_MNG_SSID_ID,
	                             wlc_assoc_parse_ssid_ie, wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_add_parse_fn failed, ssid in assocreq\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	if (wlc_iem_add_parse_fn_mft(wlc->iemi, arqfstbmp, DOT11_MNG_RATES_ID,
	                             wlc_assoc_parse_sup_rates_ie, wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_add_parse_fn failed, sup rates in assocreq\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	if (wlc_iem_add_parse_fn_mft(wlc->iemi, arqfstbmp, DOT11_MNG_EXT_RATES_ID,
	                             wlc_assoc_parse_ext_rates_ie, wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_add_parse_fn failed, ext rates in assocreq\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#ifndef BCM_STA_SUPP_CHAN_ROM_COMPAT
	if (wlc_iem_add_parse_fn_mft(wlc->iemi, arqfstbmp, DOT11_MNG_SUPP_CHANNELS_ID,
	                             wlc_assoc_parse_supp_chan_ie, wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_add_parse_fn failed, ext rates in assocreq\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#endif /* BCM_STA_SUPP_CHAN_ROM_COMPAT */
	if (wlc_iem_vs_add_parse_fn_mft(wlc->iemi, arqfstbmp, WLC_IEM_VS_IE_PRIO_WPS,
	                                wlc_assoc_parse_wps_ie, wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_vs_add_parse_fn failed, wps in assocreq\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#ifdef WL11N
	/* bcn/prbrsp */
	if (wlc_iem_add_parse_fn_mft(wlc->iemi, scanfstbmp, DOT11_MNG_HT_CAP,
	                             wlc_ht_ap_coex_scan_parse_cap_ie, wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_add_parse_fn failed, ht cap in scan\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	/* bcn */
	if (wlc_iem_add_parse_fn(wlc->iemi, WLC_IEM_FC_AP_BCN, DOT11_MNG_HT_CAP,
	                         wlc_ht_ap_coex_bcn_parse_cap_ie, wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_add_parse_fn failed, ht cap in bcn\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#endif /* WL11N */

	ap->txbcn_timeout = 0;

#if defined(MBSS) && !defined(MBSS_DISABLED)
	pub->_mbss_support = TRUE;
#endif // endif

	return (wlc_ap_info_t*)appvt;

fail:
	wlc_ap_detach((wlc_ap_info_t*)appvt);
	return NULL;
}

void
BCMATTACHFN(wlc_ap_detach)(wlc_ap_info_t *ap)
{
	wlc_ap_info_pvt_t* appvt;
	wlc_info_t *wlc;
	wlc_pub_t *pub;

	if (ap == NULL) {
		return;
	}
	appvt = (wlc_ap_info_pvt_t*) ap;
	wlc = appvt->wlc;
	pub = appvt->pub;

	wlc_apps_detach(wlc);
#ifdef WL_GLOBAL_RCLASS
	wlc_scb_state_upd_unregister(wlc,
			(bcm_notif_client_callback)wlc_ap_scb_state_upd_cb, wlc);
#endif /* WL_GLOBAL_RCLASS */

#ifdef RADIO_PWRSAVE
	if (appvt->radio_pwrsave.timer) {
		wl_del_timer(wlc->wl, appvt->radio_pwrsave.timer);
		wl_free_timer(wlc->wl, appvt->radio_pwrsave.timer);
		appvt->radio_pwrsave.timer = NULL;
	}
#endif // endif

	wlc_module_unregister(pub, "ap", appvt);

	wlc_module_remove_ioctl_fn(wlc->pub, (void *)ap);

	MFREE(pub->osh, appvt, sizeof(wlc_ap_info_pvt_t));
}

static int
wlc_ap_wlc_up(void *ctx)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *)ctx;
	wlc_ap_info_t *ap = &appvt->appub;

	(void)ap;

#ifdef RXCHAIN_PWRSAVE
	wlc_reset_rxchain_pwrsave_mode(ap);
#endif // endif
#ifdef RADIO_PWRSAVE
	wlc_reset_radio_pwrsave_mode(ap);
#endif // endif

	return BCME_OK;
}

int
wlc_ap_up(wlc_ap_info_t *ap, wlc_bsscfg_t *bsscfg)
{
	wlc_ap_info_pvt_t* appvt = (wlc_ap_info_pvt_t*)ap;
	wlc_info_t *wlc = appvt->wlc;
	uint channel;
	wlcband_t *band;
	wlc_bss_info_t *target_bss = bsscfg->target_bss;
#if defined(BCMDBG) || defined(WLMSG_INFORM)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif // endif
#ifdef EXT_STA
	int i;
	wlc_bsscfg_t *cfg;
#endif /* EXT_STA */

	/* update radio parameters from default_bss */
	/* adopt the default BSS parameters as our BSS */
	/* adopt the default BSS params as the target's BSS params */
	bcopy(wlc->default_bss, target_bss, sizeof(wlc_bss_info_t));

	/* set some values to be appropriate for AP operation */
	target_bss->infra = 1;
	target_bss->atim_window = 0;
	target_bss->capability = DOT11_CAP_ESS;
#ifdef WL11K_AP
	if (WL11K_ENAB(wlc->pub)) {
		if (wlc_rrm_enabled(wlc->rrm_info, bsscfg))
			target_bss->capability |= DOT11_CAP_RRM;
	}
#endif /* WL11K_AP */

	bcopy((char*)&bsscfg->cur_etheraddr, (char*)&target_bss->BSSID, ETHER_ADDR_LEN);

#ifdef EXT_STA
	/* find the first bsscfg that is a STA and set home time to a longer period */
	/* Only do this if ap was not already up */
	/* if ap already up, then down was not called and we should leave scan_home_time alone */
	/* if wlc_ap_down() called or this is first time calling wlc_ap_up(), then */
	/* wlc->ap->sta_scan_home_time will be 0 */
	if (WLEXTSTA_ENAB(wlc->pub)) {
		if (!wlc->ap->sta_scan_home_time) {
			FOREACH_BSS(wlc, i, cfg) {
				if (BSSCFG_STA(cfg)) {
					/* save current scan home time */
					wlc_iovar_getint(wlc, "scan_home_time",
					                 (int *)&wlc->ap->sta_scan_home_time);

					if (((uint)wlc->default_bss->beacon_period +
					     EXTRA_SCAN_HOME_TIME_FOR_AP) >
					    wlc->ap->sta_scan_home_time)
						/* set scan home channel long enough to cover AP
						 *  beacon transmit.
						 * XXX - scan time could be too long if beacon
						 * period is longer then 100 ms
						 */
						wlc_iovar_setint(wlc, "scan_home_time",
						                 wlc->default_bss->beacon_period +
						                 EXTRA_SCAN_HOME_TIME_FOR_AP);
					break;
				}
			}
		}
	}
#endif /* EXT_STA */

#ifdef WLMCHAN
	/* allow P2P GO to run a possible different channel */
	if (MCHAN_ENAB(wlc->pub) &&
	    !(BSSCFG_AP_MCHAN_DISABLED(wlc, bsscfg) ||
	      wlc_mchan_stago_is_disabled(wlc->mchan)) &&
#ifdef WLP2P
	    BSS_P2P_ENAB(wlc, bsscfg) &&
#endif // endif
	    bsscfg->chanspec != 0) {
		WL_INFORM(("wl%d: use cfg->chanspec 0x%04x in BSS %s\n", wlc->pub->unit,
		          bsscfg->chanspec, bcm_ether_ntoa(&target_bss->BSSID, eabuf)));
		target_bss->chanspec = bsscfg->chanspec;
		/* continue to validate the chanspec (channel and channel width)...
		 * they may become invalid due to other user configurations happened
		 * between GO bsscfg creation and now...
		 */
	}
	else
#endif /* WLMCHAN */
	/* use the current operating channel if any */
	if (wlc->pub->associated) {
		WL_INFORM(("wl%d: share wlc->home_chanspec 0x%04x in BSS %s\n", wlc->pub->unit,
		          wlc->home_chanspec, bcm_ether_ntoa(&target_bss->BSSID, eabuf)));
		target_bss->chanspec = wlc->home_chanspec;
		return BCME_OK;
	}

#ifdef RADAR
	/* use dfs selected channel */
	/* XXX should we ask the dfs to select a channel only when
	 * the target_bss->chanspec is configured on a DFS channel?
	 * also should we let the dfs to validate both channel and
	 * channel width?
	 */
	if (WL11H_AP_ENAB(wlc) && CHSPEC_IS5G(target_bss->chanspec) &&
	    !(BSSCFG_SRADAR_ENAB(bsscfg) || BSSCFG_AP_NORADAR_CHAN_ENAB(bsscfg) ||
		BSSCFG_AP_USR_RADAR_CHAN_SELECT(bsscfg))) {
		/* if chanspec_next is uninitialized, e.g. softAP with
		 * DFS_TPC country, pick a random one like wlc_restart_ap()
		 */
		if (wlc->chanspec_selected)
			target_bss->chanspec = wlc->chanspec_selected;
		else
			target_bss->chanspec = wlc_dfs_sel_chspec(wlc->dfs, FALSE);
		WL_INFORM(("wl%d: use dfs chanspec 0x%04x in BSS %s\n",
		           wlc->pub->unit, target_bss->chanspec,
		           bcm_ether_ntoa(&target_bss->BSSID, eabuf)));
	}
#endif /* RADAR */

	/* validate or fixup default channel value */
	/* also, fixup for GO if radar channel */
	if (!wlc_valid_chanspec_db(wlc->cmi, target_bss->chanspec) ||
	    wlc_restricted_chanspec(wlc->cmi, target_bss->chanspec) ||
#ifdef WLP2P
	   (BSS_P2P_ENAB(wlc, bsscfg) &&
	    BSS_11H_AP_NORADAR_CHAN_ENAB(wlc, bsscfg) &&
	    wlc_radar_chanspec(wlc->cmi, target_bss->chanspec)) ||
#endif // endif
	   FALSE) {
		chanspec_t chspec = wlc_default_chanspec(wlc->cmi, FALSE);
		if ((chspec == INVCHANSPEC) ||
		    wlc_restricted_chanspec(wlc->cmi, chspec)) {
			WL_ERROR(("wl%d: cannot create BSS on channel %u\n",
			          wlc->pub->unit,
			          CHSPEC_CHANNEL(target_bss->chanspec)));
			bsscfg->up = FALSE;
			return BCME_BADCHAN;
		}
		target_bss->chanspec = chspec;
		WL_INFORM(("wl%d: use default chanspec 0x%04x in BSS %s\n",
		           wlc->pub->unit, chspec, bcm_ether_ntoa(&target_bss->BSSID, eabuf)));
	}

	/* Validate the channel bandwidth */
	band = wlc->bandstate[CHSPEC_IS2G(target_bss->chanspec) ? BAND_2G_INDEX : BAND_5G_INDEX];
	if (CHSPEC_IS40(target_bss->chanspec) &&
	    (!N_ENAB(wlc->pub) ||
	     (wlc_channel_locale_flags_in_band(wlc->cmi, band->bandunit) & WLC_NO_40MHZ) ||
	     !WL_BW_CAP_40MHZ(band->bw_cap))) {
		channel = wf_chspec_ctlchan(target_bss->chanspec);
		target_bss->chanspec = CH20MHZ_CHSPEC(channel);
		WL_INFORM(("wl%d: use 20Mhz channel width in BSS %s\n",
		           wlc->pub->unit, bcm_ether_ntoa(&target_bss->BSSID, eabuf)));
	}

	return BCME_OK;
}

int
wlc_ap_down(wlc_ap_info_t *ap, wlc_bsscfg_t *bsscfg)
{
	wlc_ap_info_pvt_t* appvt = (wlc_ap_info_pvt_t*) ap;
	wlc_info_t *wlc = appvt->wlc;
	int callback = 0; /* Need to fix this timer callback propagation; error prone right now */
	struct scb_iter scbiter;
	struct scb *scb;
	int assoc_scb_count = 0;
#ifdef EXT_STA
	int i;
	wlc_bsscfg_t *cfg;
	int ap_bsscfg_up_exist = 0;
#endif /* EXT_STA */

#ifdef WLMCNX
	if (MCNX_ENAB(wlc->pub)) {
		wlc_mcnx_bss_upd(wlc->mcnx, bsscfg, FALSE);
	}
#endif // endif

	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
		if (SCB_ASSOCIATED(scb)) {
			if (WLEXTSTA_ENAB(wlc->pub)) {
				/* indicate sta leaving */
				wlc_bss_mac_event(wlc, bsscfg, WLC_E_DISASSOC_IND, &scb->ea,
					WLC_E_STATUS_SUCCESS, DOT11_RC_DISASSOC_LEAVING, 0, 0, 0);
			}
			wlc_scbfree(wlc, scb);
			assoc_scb_count++;
		}
	}

	if (assoc_scb_count) {
		if (WLEXTSTA_ENAB(wlc->pub) && wlc->pub->up) {
			/* send broadcast disassoc request to all stas */
			wlc_senddisassoc(wlc, bsscfg, WLC_BCMCSCB_GET(wlc, bsscfg),
			                 &ether_bcast, &bsscfg->cur_etheraddr, &bsscfg->BSSID,
			                 DOT11_RC_DISASSOC_LEAVING);
		}
		else if (!WLEXTSTA_ENAB(wlc->pub)) {
			/* send up a broadcast deauth mac event if there were any
			 * associated STAs
			 */
			wlc_deauth_complete(wlc, bsscfg, WLC_E_STATUS_SUCCESS, &ether_bcast,
			                    DOT11_RC_DEAUTH_LEAVING, 0);
		}
	}

	/* adjust associated state(s) accordingly */
	wlc_ap_up_upd(ap, bsscfg, FALSE);

	/* Fix up mac control */
	wlc_macctrl_init(wlc, NULL);

	WL_APSTA_UPDN(("Reporting link down on config %d (AP disabled)\n",
		WLC_BSSCFG_IDX(bsscfg)));

	wlc_link(wlc, FALSE, &bsscfg->cur_etheraddr, bsscfg, WLC_E_LINK_BSSCFG_DIS);

#ifdef EXT_STA
	if (WLEXTSTA_ENAB(wlc->pub)) {
		/* Make sure there are no other bsscfg that is ap and up */
		FOREACH_UP_AP(wlc, i, cfg)
		{
			ap_bsscfg_up_exist = 1;
			break;
		}
		if (!ap_bsscfg_up_exist) {
			/* if we find the first bsscfg that is a STA, restore home time */
			FOREACH_BSS(wlc, i, cfg) {
				if (BSSCFG_STA(cfg)) {
					/* restore scan home time */
					if (wlc->ap->sta_scan_home_time) {
						wlc_iovar_setint(wlc, "scan_home_time",
						 wlc->ap->sta_scan_home_time);
						/* zero out to signal we restored it */
						wlc->ap->sta_scan_home_time = 0;
					}
					break;
				}
			}
		}
	}
#endif /* EXT_STA */

#ifdef RADIO_PWRSAVE
	wlc_reset_radio_pwrsave_mode(ap);
#endif // endif

	/* reset per bss link margin values */
	wlc_ap_bss_tpc_setup(wlc->tpc, bsscfg);

	/* Clear the BSSID */
	bzero(bsscfg->BSSID.octet, ETHER_ADDR_LEN);

	/* WES XXX: on every bsscfg down, need to clear SSID in ucode to stop probe
	 * responses if this is the bsscfg_prb_idx
	 */

	/* WES XXX: if we take down the bsscfg_bcn_idx cfg, what beacon should be in
	 * the ucode template? Should we just leave it as is, or update to another so
	 * that an active SSID and security params show up in the beacon?
	 */

	/* WES XXX: on last bsscfg down, need to stop beaconing.
	 * Maybe just clear maccontrol AP bit
	 */

	return callback;
}

#ifdef WME
/* Initialize a WME Parameter Info Element with default AP parameters from WMM Spec, Table 14 */
void
wlc_wme_initparams_ap(wlc_ap_info_t *ap, wme_param_ie_t *pe)
{
	static const wme_param_ie_t apdef = {
		WME_OUI,
		WME_OUI_TYPE,
		WME_SUBTYPE_PARAM_IE,
		WME_VER,
		0,
		0,
		{
			{ EDCF_AC_BE_ACI_AP, EDCF_AC_BE_ECW_AP, HTOL16(EDCF_AC_BE_TXOP_AP) },
			{ EDCF_AC_BK_ACI_AP, EDCF_AC_BK_ECW_AP, HTOL16(EDCF_AC_BK_TXOP_AP) },
			{ EDCF_AC_VI_ACI_AP, EDCF_AC_VI_ECW_AP, HTOL16(EDCF_AC_VI_TXOP_AP) },
			{ EDCF_AC_VO_ACI_AP, EDCF_AC_VO_ECW_AP, HTOL16(EDCF_AC_VO_TXOP_AP) }
		}
	};

	STATIC_ASSERT(sizeof(*pe) == WME_PARAM_IE_LEN);
	memcpy(pe, &apdef, sizeof(*pe));
}
#endif /* WME */

static int
wlc_authenticated_sta_check_cb(struct scb *scb)
{
	return SCB_AUTHENTICATED(scb);
}

static int
wlc_authorized_sta_check_cb(struct scb *scb)
{
	return SCB_AUTHORIZED(scb);
}

static int
wlc_wme_sta_check_cb(struct scb *scb)
{
	return SCB_WME(scb);
}

/* Returns a maclist of all scbs that pass the provided check function.
 * The buf is formatted as a struct maclist on return, and may be unaligned.
 * buf must be at least 4 bytes long to hold the maclist->count value.
 * If the maclist is too long for the supplied buffer, BCME_BUFTOOSHORT is returned
 * and the maclist->count val is set to the number of MAC addrs that would
 * have been returned. This allows the caller to allocate the needed space and
 * call again.
 */
static int
wlc_sta_list_get(wlc_ap_info_t *ap, wlc_bsscfg_t *cfg, uint8 *buf,
                 int len, int (*sta_check_cb)(struct scb *scb))
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;
	wlc_info_t *wlc = appvt->wlc;

	int err = 0;
	uint c = 0;
	uint8 *dst;
	struct scb *scb;
	struct scb_iter scbiter;
	ASSERT(len >= (int)sizeof(uint));

	/* make room for the maclist count */
	dst = buf + sizeof(uint);
	len -= sizeof(uint);
	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {
		if (sta_check_cb(scb)) {
			c++;
			if (len >= ETHER_ADDR_LEN) {
				bcopy(scb->ea.octet, dst, ETHER_ADDR_LEN);
				dst += sizeof(struct ether_addr);
				len -= sizeof(struct ether_addr);
			} else {
				err = BCME_BUFTOOSHORT;
			}
		}
	}

	/* copy the actual count even if the buffer is too short */
	bcopy(&c, buf, sizeof(uint));

	return err;
}

/* AP processes WME Setup Request Management frame */
/* Algorithm for accept/reject is still in flux. For now, reject all requests */
void
wlc_wme_setup_req(wlc_ap_info_t *ap, struct dot11_management_header *hdr,
	uint8 *body, int body_len)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;
	wlc_info_t *wlc = appvt->wlc;
	void *outp;
	uint8* pbody;
	struct dot11_management_notification* mgmt_hdr;
#if defined(BCMDBG) || defined(WLMSG_INFORM)
	char eabuf[ETHER_ADDR_STR_LEN], *sa = bcm_ether_ntoa(&hdr->sa, eabuf);
#endif // endif
	wlc_bsscfg_t *cfg = wlc->cfg;

	if ((outp = wlc_frame_get_mgmt(wlc, FC_ACTION, &hdr->sa, &cfg->cur_etheraddr,
	                               &cfg->BSSID, body_len, &pbody)) == NULL) {
		WL_ERROR(("wl%d: failed to get WME response packet \n",
			wlc->pub->unit));
		return;
	}

	/*
	 *  duplicate incoming packet and change
	 *  the few necessary fields
	 */
	bcopy((char *)body, (char *)pbody, body_len);
	mgmt_hdr = (struct dot11_management_notification *)pbody;
	mgmt_hdr->action = WME_ADDTS_RESPONSE;

	/* for now, reject all setup requests */
	mgmt_hdr->status = WME_ADMISSION_REFUSED;
	WL_INFORM(("Sending WME_SETUP_RESPONSE, refusal to %s\n", sa));
	wlc_sendmgmt(wlc, outp, cfg->wlcif->qi, NULL);
}

/* age out STA associated but waiting on AS for authorization */
static void
wlc_ap_wsec_stas_timeout(wlc_ap_info_t *ap)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;
	wlc_info_t *wlc = appvt->wlc;
	struct scb *scb;
	struct scb_iter scbiter;
	char eabuf[ETHER_ADDR_STR_LEN], *ea;

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		wlc_bsscfg_t *cfg = SCB_BSSCFG(scb);

		/* Filter out the permanent SCB, AP SCB */
		if (scb->permanent ||
		    cfg == NULL || !BSSCFG_AP(cfg))
			continue;

		/* Filter out scbs not associated or marked for deletion */
		if (!SCB_ASSOCIATED(scb) || SCB_MARKED_FOR_DELETION(scb))
			continue;

		if (WSEC_ENABLED(scb->bsscfg->wsec) && !SCB_AUTHORIZED(scb) &&
			!(scb->wsec == WEP_ENABLED)) {
			scb->wsec_auth_timeout++;
			if (scb->wsec_auth_timeout > SCB_AUTH_TIMEOUT) {
				scb->wsec_auth_timeout = 0;
				ea  = bcm_ether_ntoa(&scb->ea, eabuf);
				BCM_REFERENCE(ea);
				WL_ERROR(("wl%d: authentication delay, send deauth to sta %s\n",
						wlc->pub->unit, ea));
				wlc_scb_set_auth(wlc, scb->bsscfg, scb, FALSE, AUTHENTICATED,
					DOT11_RC_UNSPECIFIED);
			}
		}
	}
}

static void
wlc_ap_stas_timeout(wlc_ap_info_t *ap)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;
	wlc_info_t *wlc = appvt->wlc;
	struct scb *scb;
	struct scb_iter scbiter;

	WL_INFORM(("%s: run at time = %d\n", __FUNCTION__, wlc->pub->now));
	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		wlc_bsscfg_t *cfg = SCB_BSSCFG(scb);

		/* Don't age out the permanent SCB and AP SCB */
		if (scb->permanent ||
		    cfg == NULL || !BSSCFG_AP(cfg))
			continue;

		/* kill any other band scb */
		if (TRUE &&
#ifdef WLMCHAN
		    !MCHAN_ENAB(wlc->pub) &&
#endif // endif
		    wlc_scbband(scb) != wlc->band) {
			wlc_scbfree(wlc, scb);
			continue;
		}

		if (SCB_MARKED_FOR_DELETION(scb)) {
			/* If the SCB has been marked for deletion, let's free the SCB here */
			wlc_scbfree(wlc, scb);
			continue;
		}

		/* probe associated stas if idle for scb_activity_time or reprobe them */
		if (SCB_ASSOCIATED(scb) &&
		    ((wlc->scb_activity_time && ((wlc->pub->now - scb->used) >=
		      wlc->scb_activity_time)) ||
		     (wlc->reprobe_scb && scb->grace_attempts))) {
			wlc_ap_sta_probe(ap, scb);
		}

		/* Authenticated but idle for long time free it now */
		if ((scb->state == AUTHENTICATED) &&
		    ((wlc->pub->now - scb->used) >= SCB_LONG_TIMEOUT)) {
			wlc_scbfree(wlc, scb);
			continue;
		}
	}
}

uint
wlc_ap_stas_associated(wlc_ap_info_t *ap)
{
	wlc_info_t *wlc = ((wlc_ap_info_pvt_t *)ap)->wlc;
	int i;
	wlc_bsscfg_t *bsscfg;
	uint count = 0;

	FOREACH_UP_AP(wlc, i, bsscfg)
		count += wlc_bss_assocscb_getcnt(wlc, bsscfg);

	return count;
}

#ifdef WL_SAE
/*
 * Called at the top of wlc_authresp_client.
 * Parse the auth frame, identify Commit and Confirms
 */
static uint
wlc_sae_parse_auth(wlc_bsscfg_t *cfg, struct dot11_management_header *hdr,
	uint8 *body, uint body_len)
{
	wlc_info_t *wlc = cfg->wlc;
	struct scb *scb;
	struct dot11_auth *auth = (struct dot11_auth *) body;
	uint16 auth_alg, auth_seq;
	int status = DOT11_SC_SUCCESS;

	auth_alg = ltoh16(auth->alg);
	auth_seq = ltoh16(auth->seq);

	if (body_len < sizeof(struct dot11_auth)) {
		WL_ERROR(("wl%d: %s: Bad argument\n", wlc->pub->unit, __FUNCTION__));
		status = BCME_BADARG;
		goto done;
	}

	WL_ASSOC(("wl%d: %s: alg : %d seq %d len %d\n", wlc->pub->unit, __FUNCTION__,
		auth_alg, auth_seq, body_len));
	if ((auth_alg != DOT11_SAE) || ((auth_seq != WL_SAE_COMMIT) &&
		(auth_seq != WL_SAE_CONFIRM))) {
		/* not an SAE auth. Fail. */
		status = DOT11_SC_FAILURE;
		goto done;
	}

	if ((scb = wlc_scbfind(wlc, cfg, (struct ether_addr *)&hdr->sa)) == NULL) {
		status = DOT11_SC_FAILURE;
		WL_ERROR(("wl%d: %s: NO SCB\n", wlc->pub->unit, __FUNCTION__));
		goto done;
	}

done:
	return status;
}
#endif /* WL_SAE */
#if defined(MBSS) && defined(WLC_HIGH) && defined(WLC_LOW)

/* ******* Probe Request Fifo Handling: Generate Software Probe Responses ******* */

/*
 * Convert raw PRQ entry to info structure
 * Returns error if bsscfg not found in wlc structure
 */

static int
prq_entry_convert(wlc_info_t *wlc, shm_mbss_prq_entry_t *in, wlc_prq_info_t *out)
{
	int uc_idx, sw_idx;

	bzero(out, sizeof(*out));
	bcopy(in, &out->source, sizeof(shm_mbss_prq_entry_t));
	if (ETHER_ISNULLADDR(&out->source.ta)) {
		WL_ERROR(("prq_entry_convert: PRQ Entry for Transmitter Address is NULL\n"));
		return -1;
	}

	out->directed_ssid = SHM_MBSS_PRQ_ENT_DIR_SSID(in);
	out->directed_bssid = SHM_MBSS_PRQ_ENT_DIR_BSSID(in);
	out->is_directed = (out->directed_ssid || out->directed_bssid);
	out->frame_type = SHM_MBSS_PRQ_ENT_FRAMETYPE(in);
	out->up_band = SHM_MBSS_PRQ_ENT_UPBAND(in);
	out->plcp0 = SHM_MBSS_PRQ_ENT_PLCP0(in);
#ifdef WLLPRS
	out->is_htsta = SHM_MBSS_PRQ_ENT_HTSTA(in);
#endif /* WLLPRS */

	if (out->is_directed) {
		uc_idx = SHM_MBSS_PRQ_ENT_UC_BSS_IDX(in);
		sw_idx = WLC_BSSCFG_HW2SW_IDX(wlc, uc_idx);
		if (sw_idx < 0) {
			return -1;
		}
		out->bsscfg = wlc->bsscfg[sw_idx];
		ASSERT(out->bsscfg != NULL);
	}

	return 0;
}

#if defined(BCMDBG)
static void
prq_entry_dump(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint16 rd_ptr, shm_mbss_prq_entry_t *entry)
{
	uint8 *ptr;
	char ssidbuf[SSID_FMT_BUF_LEN];

	ptr = (uint8 *)entry;
	if (rd_ptr != 0) {
		WL_MBSS(("Dump of raw PRQ entry from offset 0x%x (word offset 0x%x)\n",
			rd_ptr * 2, rd_ptr));
	} else {
		WL_MBSS(("    Dump of raw PRQ entry\n"));
	}
	WL_MBSS(("    %02x%02x %02x%02x %02x%02x %02x%02x %04x\n",
		ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5],
		entry->prq_info[0], entry->prq_info[1], entry->time_stamp));
	WL_MBSS(("    %sdirected SSID. %sdirected BSSID. uc_idx: %d. type %d. upband %d.\n",
		SHM_MBSS_PRQ_ENT_DIR_SSID(entry) ? "" : "not ",
		SHM_MBSS_PRQ_ENT_DIR_BSSID(entry) ? "" : "not ",
		SHM_MBSS_PRQ_ENT_UC_BSS_IDX(entry),
		SHM_MBSS_PRQ_ENT_FRAMETYPE(entry),
		SHM_MBSS_PRQ_ENT_UPBAND(entry)));
	if (bsscfg != NULL) {
		wlc_format_ssid(ssidbuf, bsscfg->SSID, bsscfg->SSID_len);
		WL_MBSS(("    Entry mapped to bss %d, ssid %s\n", WLC_BSSCFG_IDX(bsscfg), ssidbuf));
	}
}
static void
prq_info_dump(wlc_info_t *wlc, wlc_prq_info_t *info)
{
	WL_MBSS(("Dump of PRQ info: dir %d. dir ssid %d. dir bss %d. bss cfg idx %d\n",
		info->is_directed, info->directed_ssid, info->directed_bssid,
		WLC_BSSCFG_IDX(info->bsscfg)));
	WL_MBSS(("    frame type %d, up_band %d, plcp0 0x%x\n",
		info->frame_type, info->up_band, info->plcp0));
	prq_entry_dump(wlc, info->bsscfg, 0, &info->source);
}
#else
#define prq_entry_dump(a, b, c, d)
#define prq_info_dump(a, b)
#endif /* BCMDBG */

/*
 * PRQ FIFO Processing
 */

static void
prb_pkt_final_setup(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, wlc_prq_info_t *info,
	uint8 *pkt_start, int len)
{
	d11txh_t *txh;
	uint8 *plcp_hdr;
	uint8 *d11_hdr;
	uint8 *d11_da;
	int plcp_len;
	uint32 exptime;
	uint16 exptime_low;
	uint16 mcl;

	txh = (d11txh_t *)pkt_start;
	plcp_hdr = &pkt_start[D11_TXH_LEN];
	d11_hdr = &pkt_start[D11_TXH_LEN + D11_PHY_HDR_LEN];
	d11_da = &d11_hdr[2 * sizeof(uint16)]; /* skip fc and duration/id */

	/* Set up the PHY header */
	plcp_len = len - D11_TXH_LEN - D11_PHY_HDR_LEN + DOT11_FCS_LEN;
	wlc_prb_resp_plcp_hdrs(wlc, bsscfg, info, plcp_len, plcp_hdr, txh, d11_hdr);

	if (bsscfg->prb_ttl_us > 0) { /* Set the packet expiry time */
		exptime = R_REG(wlc->osh, &wlc->regs->tsf_timerlow);
		exptime_low = (uint16)exptime;
		exptime = (exptime & 0xffff0000) | info->source.time_stamp;
		if (exptime_low < info->source.time_stamp) { /* Rollover occurred */
			exptime -= 0x10000; /* Decrement upper 16 bits. */
		}
		exptime += bsscfg->prb_ttl_us;

		txh->TstampLow = htol16(exptime & 0xffff);
		txh->TstampHigh = htol16((exptime >> 16) & 0xffff);
		mcl = ltoh16(txh->MacTxControlLow);
		mcl |= TXC_LIFETIME;
		txh->MacTxControlLow = htol16(mcl);
	}

	/* Set up the dest addr */
	bcopy(&info->source.ta, d11_da, sizeof(struct ether_addr));
}

#if defined(MBSS) && defined(WLLPRS)
/* This function does a selective copy of the PRQ template to the actual probe resp
 * packet. The idea is to intelligently form the "best" probe resp based on the
 * prq_info passed in by uCODE, based on parsing of the probe request. This is a change
 * to the existing "universal" probe response model.
 */
static void
wlc_prq_resp_form(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, wlc_prq_info_t *info,
                  uint8 *pkt_start, int *len)
{
	wlc_pkt_t template;

	template = bsscfg->probe_template;

	if (template == NULL)
		return;

	/* We need to change from the template */
	if (!info->is_htsta && PRB_HTIE(bsscfg).present) {
		uint8 *src_ptr = PKTDATA(wlc->osh, template);
		uint8 *dst_ptr = pkt_start;
		int copy_len = D11_PHY_HDR_LEN + D11_TXH_LEN + DOT11_MGMT_HDR_LEN +
		               PRB_HTIE(bsscfg).offset;

		/* Exclude ANA HT IEs while copying */
		bcopy(src_ptr, dst_ptr, copy_len);

		*len -= PRB_HTIE(bsscfg).length;
		src_ptr = PKTDATA(wlc->osh, template) + copy_len +
		          PRB_HTIE(bsscfg).length;
		dst_ptr = pkt_start + copy_len;
		copy_len = *len - copy_len;

		bcopy(src_ptr, dst_ptr, copy_len);
	} else
		bcopy(PKTDATA(wlc->osh, template), pkt_start, *len);

	return;
}
#endif /* MBSS  && WLLPRS */

/* Respond to the given PRQ entry on the given bss cfg */
static void
wlc_prq_directed(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, wlc_prq_info_t *info)
{
	wlc_pkt_t pkt, template;
	uint8 *pkt_start;
	int len, bcn_tmpl_len;

	bcn_tmpl_len = wlc->pub->bcn_tmpl_len;
	len = bcn_tmpl_len;

	template = bsscfg->probe_template;
	if (template == NULL) {
		return;
	}
	len = PKTLEN(wlc->osh, template);

	/* Allocate a new pkt for DMA; copy from template; set up hdrs */
	pkt = PKTGET(wlc->osh, bcn_tmpl_len, TRUE);
	if (pkt == NULL) {
		WLCNTINCR(bsscfg->cnt->prb_resp_alloc_fail);
		return;
	}
	/* Template includes TX header, PLCP header and D11 header */
	pkt_start = PKTDATA(wlc->osh, pkt);
#if defined(MBSS) && defined(WLLPRS)
	if (N_ENAB(wlc->pub))
		wlc_prq_resp_form(wlc, bsscfg, info, pkt_start, &len);
#else /* MBSS && WLLPRS */
	bcopy(PKTDATA(wlc->osh, template), pkt_start, len);
#endif /* MBSS && WLLPRS */
	PKTSETLEN(wlc->osh, pkt, len);
	WLPKTTAGBSSCFGSET(pkt, WLC_BSSCFG_IDX(bsscfg));

	prb_pkt_final_setup(wlc, bsscfg, info, pkt_start, len);

	if (dma_txfast(WLC_HW_DI(wlc, TX_ATIM_FIFO), pkt, TRUE) < 0) {
		WL_MBSS(("Failed to transmit probe resp for bss %d\n", WLC_BSSCFG_IDX(bsscfg)));
		WLCNTINCR(bsscfg->cnt->prb_resp_tx_fail);
	}
}

/* Process a PRQ entry, whether broadcast or directed, generating probe response(s) */

/* Is the given config up and responding to probe responses in SW? */
#define CFG_SOFT_PRB_RESP(cfg) \
	(((cfg) != NULL) && ((cfg)->up) && ((cfg)->enable) && SOFTPRB_ENAB(cfg))

static void
wlc_prq_response(wlc_info_t *wlc, wlc_prq_info_t *info)
{
	int idx;
	wlc_bsscfg_t *cfg;

#ifdef WLPROBRESP_SW
	if (WLPROBRESP_SW_ENAB(wlc))
		return;
#endif /* WLPROBRESP_SW */

	if (info->is_directed) {
		ASSERT(info->bsscfg != NULL);
		wlc_prq_directed(wlc, info->bsscfg, info);
	} else { /* Broadcast probe response */
		for (idx = 0; idx < WLC_MAXBSSCFG; idx++) {
			cfg = wlc->bsscfg[(idx + wlc->bcast_next_start) % WLC_MAXBSSCFG];
			if (CFG_SOFT_PRB_RESP(cfg) && !cfg->closednet_nobcprbresp) {
				wlc_prq_directed(wlc, cfg, info);
			}
		}
		/* Move "next start" up to next BSS skipping inactive BSSes */

		for (idx = 0; idx < WLC_MAXBSSCFG; idx++) {
			if (++wlc->bcast_next_start == WLC_MAXBSSCFG) {
				wlc->bcast_next_start = 0;
			}
			if (CFG_SOFT_PRB_RESP(wlc->bsscfg[wlc->bcast_next_start])) {
				break;
			}
		}
	}
}

/*
 * Process the PRQ Fifo.
 * Note that read and write pointers are (uint16 *) in the ucode
 * Return TRUE if more entries to process.
 */

/*
 * After some investigation, it looks like there's never more than 1 PRQ
 * entry to be serviced at a time.  So the bound here is probably inconsequential.
 */
#define PRQBND 5

bool
wlc_prq_process(wlc_info_t *wlc, bool bounded)
{
	uint16 rd_ptr, wr_ptr, prq_base, prq_top;
	shm_mbss_prq_entry_t entry;
	wlc_prq_info_t info;
	int count = 0;
	bool rv = FALSE;  /* Default, no more to be done */
	bool set_rd_ptr = FALSE;

	if (!MBSS_ENAB(wlc->pub)) {
		return FALSE;
	}

	prq_base = wlc->prq_base;
	prq_top = prq_base + (SHM_MBSS_PRQ_TOT_BYTES / 2);

	rd_ptr = wlc->prq_rd_ptr;
	wr_ptr = wlc_read_shm(wlc, SHM_MBSS_PRQ_WRITE_PTR);

#if defined(BCMDBG)
	/* Debug checks for rd and wr ptrs */
	if (wr_ptr < prq_base || wr_ptr >= prq_top) {
		WL_ERROR(("Error: PRQ fifo write pointer 0x%x out of bounds (%d, %d)\n",
			wr_ptr, prq_base, prq_top));
		return FALSE;
	}
	if (rd_ptr < prq_base || rd_ptr >= prq_top) {
		WL_ERROR(("Error: PRQ read pointer 0x%x out of bounds; clearing fifo\n", rd_ptr));
		/* Reset read pointer to write pointer, emptying the fifo */
		rd_ptr = wr_ptr;
		set_rd_ptr = TRUE;
	}
#endif /* BCMDBG */

	while (rd_ptr != wr_ptr) {
		WLCNTINCR(wlc->pub->_cnt->prq_entries_handled);
		set_rd_ptr = TRUE;

		/* Copy entry from PRQ; convert and respond; update rd ptr */
		wlc_copyfrom_shm(wlc, rd_ptr * 2, &entry, sizeof(entry));
		if (prq_entry_convert(wlc, &entry, &info) < 0) {
			WL_ERROR(("Error reading prq fifo at offset 0x%x\n", rd_ptr));
			prq_entry_dump(wlc, NULL, rd_ptr, &entry);
			WLCNTINCR(wlc->pub->_cnt->prq_bad_entries);
		} else if (info.is_directed && !(info.bsscfg->up)) { /* Ignore rqst */
			WL_MBSS(("MBSS: Received PRQ entry on down BSS (%d)\n",
				WLC_BSSCFG_IDX(info.bsscfg)));
		} else {
#if defined(BCMDBG)
			if (info.is_directed && info.bsscfg != NULL) {
				if (0) { /* Extra dump for directed requests */
					prq_info_dump(wlc, &info);
				}
				WLCNTINCR(info.bsscfg->cnt->prq_directed_entries);
			} else {
				WLCNTINCR(wlc->pub->_cnt->prq_undirected_entries);
			}
#endif // endif
			wlc_prq_response(wlc, &info);
		}

		/* Update the read pointer */
		rd_ptr += sizeof(entry) / 2;
		if (rd_ptr >= prq_top) {
			rd_ptr = prq_base;
		}

		if (bounded && (count++ >= PRQBND)) {
			rv = TRUE; /* Possibly more to do */
			break;
		}
	}

	if (set_rd_ptr) { /* Write the value back when done processing */
		wlc_write_shm(wlc, SHM_MBSS_PRQ_READ_PTR, rd_ptr);
		wlc->prq_rd_ptr = rd_ptr;
	}

	return rv;
}
#endif /* MBSS && WLC_HIGH && WLC_LOW */

static void
wlc_auth_nhdlr_cb(void *ctx, wlc_iem_nhdlr_data_t *data)
{
	if (WL_INFORM_ON()) {
		printf("%s: no parser\n", __FUNCTION__);
		prhex("IE", data->ie, data->ie_len);
	}
}

static uint8
wlc_auth_vsie_cb(void *ctx, wlc_iem_pvsie_data_t *data)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;

	return wlc_iem_vs_get_id(wlc->iemi, data->ie);
}

void
wlc_ap_authresp(wlc_ap_info_t *ap, wlc_bsscfg_t *bsscfg, struct dot11_management_header *hdr,
	uint8 *body, uint body_len, bool short_preamble, bool dec_attempt)
{
#if defined(BCMDBG) || defined(BCMDBG_ERR) || defined(WLMSG_ASSOC) || \
	defined(WLMSG_BTA)
	char eabuf[ETHER_ADDR_STR_LEN], *sa = bcm_ether_ntoa(&hdr->sa, eabuf);
#endif // endif
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;
	wlc_info_t *wlc = appvt->wlc;
	struct dot11_auth *auth;
	struct scb *scb = NULL;
	uint16 auth_alg, auth_seq;
	uint16 status = DOT11_SC_SUCCESS;
	int i;
	int key_index;
	uint16 fc;
	wsec_key_t *key;
	wlc_iem_upp_t upp;
	wlc_iem_ft_pparm_t ftpparm;
	wlc_iem_pparm_t pparm;
	bool mac_mode_deny_client = FALSE;
#ifdef WL_SAE
	uint8 *body_start = body;
	uint body_len_start = body_len;
#endif // endif

	WL_TRACE(("wl%d: wlc_authresp_ap\n", wlc->pub->unit));

	/* PR38742 WAR: do not process auth frames while AP scan is in
	 * progress, it may cause frame going out on a different
	 * band/channel
	 */
	if (SCAN_IN_PROGRESS(wlc->scan)) {
		WL_ASSOC(("wl%d: AP Scan in progress, abort auth\n",
			wlc->pub->unit));
		status = SMFS_CODE_IGNORED;
		goto smf_stats;
	}

#ifdef BCMAUTH_PSK
	if (AP_ENAB(wlc->pub) && WSEC_TKIP_ENABLED(bsscfg->wsec) && bsscfg->tkip_countermeasures) {
		uint32 cur_tick = OSL_SYSUPTIME();
		if (wlc_clear_tkip_cm_bt(cur_tick, bsscfg->tk_cm_bt_tmstmp)) {
			bsscfg->tk_cm_dt = 0;
			bsscfg->tk_cm_bt = 0;
			bsscfg->tkip_countermeasures = FALSE;
		} else {
			return;
		}
	}
#endif /* BCMAUTH_PSK */

	/* Only handle Auth requests when the bsscfg is operational */
	if (BSSCFG_AP(bsscfg) && !bsscfg->up) {
		WL_ASSOC(("wl%d: BSS is not operational, abort auth\n", wlc->pub->unit));
		status = SMFS_CODE_IGNORED;
		goto smf_stats;
	}

#ifdef PSTA
	/* Don't allow more than psta max assoc limit */
	if (PSTA_ENAB(wlc->pub) && (wlc_ap_stas_associated(ap) >= PSTA_MAX_ASSOC(wlc))) {
	        WL_ERROR(("wl%d: %s denied association due to max psta association limit\n",
	                  wlc->pub->unit, sa));
		status = SMFS_CODE_IGNORED;
		goto smf_stats;
	}
#endif /* PSTA */

#ifdef BAND5G
	/* Reject auth & assoc while  performing a channel switch */
	if (wlc->block_datafifo & DATA_BLOCK_QUIET) {
		WL_REGULATORY(("wl%d: %s: Authentication denied while in radar"
		               " avoidance mode\n", wlc->pub->unit, __FUNCTION__));
		status = SMFS_CODE_IGNORED;
		goto smf_stats;
	}
#endif /* BAND5G */

#ifdef WLAUTHRESP_MAC_FILTER
	/* Suppress auth resp by MAC filter if authresp_macfltr is enabled */
	if (BSSCFG_AP(bsscfg) && AP_BSSCFG_CUBBY(appvt, bsscfg)->authresp_macfltr) {
		int addr_match = wlc_macfltr_addr_match(wlc->macfltr, bsscfg, &hdr->sa);
		if ((addr_match == WLC_MACFLTR_ADDR_DENY) ||
			(addr_match == WLC_MACFLTR_ADDR_NOT_ALLOW)) {
			WL_ASSOC(("wl%d: auth resp to %s suppressed by MAC filter\n",
			wlc->pub->unit, sa));
			wlc_bss_mac_event(wlc, bsscfg, WLC_E_PRUNE,
				&hdr->sa, 0, WLC_E_PRUNE_AUTH_RESP_MAC, 0, 0, 0);
			status = SMFS_CODE_IGNORED;
			goto smf_stats;
		}
	}
#endif /* WLAUTHRESP_MAC_FILTER */

	ASSERT(BSSCFG_AP(bsscfg));

	fc = ltoh16(hdr->fc);

	if (fc & FC_WEP) {
		/* Check if it is logical to have an encrypted packet here */
		if ((scb = wlc_scbfind(wlc, bsscfg, (struct ether_addr *) &hdr->sa)) == NULL) {
			WL_ERROR(("wl%d: %s: auth resp frame encrypted"
				  "from unknown STA %s\n", wlc->pub->unit, __FUNCTION__, sa));
			status = SMFS_CODE_MALFORMED;
			goto smf_stats;
		}
		if (scb->challenge == NULL) {
			WL_ERROR(("wl%d: %s: auth resp frame encrypted "
				"with no challenge recorded from %s\n",
				wlc->pub->unit, __FUNCTION__, sa));
			status = SMFS_CODE_MALFORMED;
			goto smf_stats;
		}

		/* Processing a WEP encrypted AUTH frame:
		 * BSS config0 is allowed to use the HW default keys, all other BSS configs require
		 * software decryption of AUTH frames.  For simpler code:
		 *
		 * If the frame has been decrypted(a default HW key is present at the right index),
		 * always re-encrypt the frame with the key used by HW and then use the BSS config
		 * specific WEP key to decrypt. This means that all WEP encrypted AUTH frames will
		 * be decrypted in software.
		 */

		WL_ASSOC(("wl%d: %s: received wep from %sassociated scb\n",
		          wlc->pub->unit, __FUNCTION__, SCB_ASSOCIATED(scb) ? "" : "non-"));

		status = DOT11_SC_AUTH_CHALLENGE_FAIL;
		key_index = body[3] >> DOT11_KEY_INDEX_SHIFT;

		if (dec_attempt) {
			uint8 key_data[16];
			rc4_ks_t ks;

			WL_ASSOC(("wl%d: %s: received wep decrypted frame\n",
			          wlc->pub->unit, __FUNCTION__));

			/* Use the default key in hardware to re-encrypt! */
			key = WSEC_KEY(wlc, key_index);
			/* RXS_DECATMPT flag will be set regardless of whether
			 * the key is valid.  So, if we don't really have a key
			 * there or the key's algo is "0" then the frame
			 * wasn't really decrypted by HW.
			 */
			if (key && (key->algo != 0)) {
				bcopy(body, key_data, 3);
				bcopy(key->data, &key_data[3], key->len);

				/* prepare key */
				prepare_key(key_data, key->len + 3, &ks);
				rc4(body+4, body_len - 4, &ks);
			}
		}

		/* Fixup auth alg and seq for encrypted frames assuming this is a
		 * Shared Key challenge response
		 */
		auth_alg = DOT11_SHARED_KEY;
		auth_seq = 3;

		/* At this point the frame is encrypted. Attempt decrypting
		 * using the bsscfg key.  Then compare the decrypted challenge
		 * text
		 */
		key = WLC_BSSCFG_DEFKEY(bsscfg, key_index);
		if (key) {
			if (wep_decrypt(body_len, body, key->len, key->data)) {
				WL_ASSOC(("wl%d: %s: ICV pass : %s: BSSCFG = %d\n",
				          wlc->pub->unit, __FUNCTION__, sa,
				          WLC_BSSCFG_IDX(bsscfg)));
			} else {
				WL_ASSOC(("wl%d: %s: ICV fail : %s: BSSCFG = %d\n",
				          wlc->pub->unit, __FUNCTION__, sa,
				          WLC_BSSCFG_IDX(bsscfg)));
				wlc_bss_mac_event(wlc, bsscfg, WLC_E_PRUNE, &hdr->sa,
					0, WLC_E_PRUNE_ENCR_MISMATCH, 0, 0, 0);
				goto clean_and_send;
			}
		}
		else {
			WL_ASSOC(("wl%d: %s: no key for WEP Auth Challenge from %s\n",
			          wlc->pub->unit, __FUNCTION__, sa));
		clean_and_send:
			/* free the challenge text */
			MFREE(wlc->osh, scb->challenge, 2 + scb->challenge[1]);
			scb->challenge = NULL;
			goto send_result;
		}

		/* Skip IV */
		body += 4;
		body_len -= 4;

		/* Remove ICV */
		body_len -= 4;
	}

	auth = (struct dot11_auth *)body;
	auth_alg = ltoh16(auth->alg);
	auth_seq = ltoh16(auth->seq);

	/* if sequence number would be out of range, do nothing */
	if (auth_seq >= 4) {
		status = SMFS_CODE_MALFORMED;
		goto smf_stats;
	}

	body += sizeof(struct dot11_auth);
	body_len -= sizeof(struct dot11_auth);

	if (fc & FC_WEP)
		goto parse_ies;
	if ((auth_seq == 1) ||
#ifdef WL_SAE
			(auth_seq == WL_SAE_CONFIRM) ||
#endif /* WL_SAE */
			FALSE) {
		/* free all scbs for this sta
		 * PR38890:Check if scb for the STA already exists, if so then
		 * free it to resurrect scb from the scratch.
		 */
		for (i = 0; i < (int)NBANDS(wlc); i++) {
			/* Use band 1 for single band 11a */
			if (IS_SINGLEBAND_5G(wlc->deviceid))
				i = BAND_5G_INDEX;

			scb = wlc_scbfindband(wlc, bsscfg, (struct ether_addr *)&hdr->sa, i);
			if (scb && (auth_seq == 1)) {
				WL_ASSOC(("wl%d: %s: scb for the STA-%s already exists\n",
					wlc->pub->unit, __FUNCTION__, sa));
#ifdef WLBTAMP
				if ((scb->bsscfg == bsscfg) && BSS_BTA_ENAB(wlc, bsscfg)) {
					WL_BTA(("wl%d: %s: preserving pre-existing "
								"BT-AMP peer %s\n",
								wlc->pub->unit, __FUNCTION__, sa));
					goto btamp_sanity;
				}
#endif /* WLBTAMP */
				wlc_scbfree(wlc, scb);
			}
#ifdef WL_SAE
			/* In case of SAE confirm, once we find the corresponding
			 * scb, we should process it.
			 */
			if (scb && (auth_seq == WL_SAE_CONFIRM)) {
				WL_ASSOC(("wl%d: %s: scb for the STA-%s exists"
					"\n", wlc->pub->unit, __FUNCTION__, sa));
				break;
			}
#endif /* WL_SAE */
#ifdef WL_SAE
			if (scb && (auth_seq == WL_SAE_CONFIRM)) {
				break;
			}
#endif /* WL_SAE */
		}
	}
	switch (auth_seq) {
		/* case 1 covers both SAE and other WPA2 auth including FBT */
		case 1:
				/* allocate an scb */
		if (!(scb = wlc_scblookup(wlc, bsscfg, (struct ether_addr *) &hdr->sa))) {
			/* To address new requirement i.e. Send Auth and Assoc response
			 * with reject status for client if client is in Mac deny list
			 * of AP.
			 * Send auth response with status "insufficient bandwidth"
			 */
			int addr_match = wlc_macfltr_addr_match(wlc->macfltr, bsscfg,
				(struct ether_addr*)&hdr->sa);

			if ((addr_match == WLC_MACFLTR_ADDR_DENY) ||
				(addr_match == WLC_MACFLTR_ADDR_NOT_ALLOW)) {

				mac_mode_deny_client = TRUE;
				break;
			} else {
				WL_ERROR(("wl%d: %s: out of scbs for %s\n",  wlc->pub->unit,
					__FUNCTION__, sa));
				status = DOT11_SC_FAILURE;
				break;
			}
		}
		if (scb->flags & SCB_MYAP) {
			if (APSTA_ENAB(wlc->pub)) {
				WL_APSTA(("wl%d: Reject AUTH request from AP %s\n",
				          wlc->pub->unit, sa));
				status = DOT11_SC_FAILURE;
				break;
			}
			scb->flags &= ~SCB_MYAP;
		}

		wlc_scb_disassoc_cleanup(wlc, scb);

#ifdef WLBTAMP
	btamp_sanity:
#endif /* WLBTAMP */

		/* auth_alg is coming from the STA, not us */
		scb->auth_alg = (uint8)auth_alg;
		switch (auth_alg) {
		case DOT11_OPEN_SYSTEM:

			if ((WLC_BSSCFG_AUTH(bsscfg) == DOT11_OPEN_SYSTEM) && (!status)) {
				wlc_scb_setstatebit_bsscfg(scb, AUTHENTICATED,
				                           WLC_BSSCFG_IDX(bsscfg));
			}
			else {
				wlc_scb_clearstatebit_bsscfg(scb, AUTHENTICATED,
				                             WLC_BSSCFG_IDX(bsscfg));
			}

			/* At this point, we should have at least one valid authentication in open
			 * system
			 */
			if (!(SCB_AUTHENTICATED(scb))) {
				WL_ERROR(("wl%d: %s: Open System auth attempted "
					  "from %s but only Shared Key supported\n",
					  wlc->pub->unit, __FUNCTION__, sa));
				status = DOT11_SC_AUTH_MISMATCH;
				wlc_bss_mac_event(wlc, bsscfg, WLC_E_PRUNE, &hdr->sa, 0,
					WLC_E_PRUNE_ENCR_MISMATCH, 0, 0, 0);
			}
			break;
		case DOT11_SHARED_KEY:
			break;
		case DOT11_FAST_BSS:
			break;
#ifdef WL_SAE
		case DOT11_SAE:
			break;
#endif /* WL_SAE */
		default:
			WL_ERROR(("wl%d: %s: unhandled algorithm %d from %s\n",
				wlc->pub->unit, __FUNCTION__, auth_alg, sa));
			status = DOT11_SC_AUTH_MISMATCH;
			wlc_bss_mac_event(wlc, bsscfg, WLC_E_PRUNE, &hdr->sa, 0,
				WLC_E_PRUNE_ENCR_MISMATCH, 0, 0, 0);
			break;
		}
		break;

#ifdef WL_SAE
		case WL_SAE_CONFIRM:
		if (scb == NULL) {
			WL_ERROR(("wl%d: %s: scb is NULL for %s\n",
				wlc->pub->unit, __FUNCTION__, sa));
			status = DOT11_SC_FAILURE;
			break;
		} else {
			scb->auth_alg = (uint8)auth_alg;
			switch (auth_alg) {
				case DOT11_SAE:
					break;
				default:
					WL_ERROR(("wl%d: %s: unhandled algorithm %d \n",
						wlc->pub->unit, __FUNCTION__, auth_alg));
					status = DOT11_SC_AUTH_MISMATCH;
					wlc_bss_mac_event(wlc, bsscfg, WLC_E_PRUNE, &hdr->sa, 0,
							WLC_E_PRUNE_ENCR_MISMATCH, 0, 0, 0);
					break;
			}
		}
		break;
#endif /* WL_SAE */
	default:

		WL_ERROR(("wl%d: %s: unexpected authentication sequence %d from %s\n",
			wlc->pub->unit, __FUNCTION__, auth_seq, sa));
		status = DOT11_SC_AUTH_SEQ;
		break;
	}
#ifdef WL_SAE
	/* If running SAE algorithm, by-pass tests that might not apply */
	if (scb && (bsscfg->WPA_auth & WPA3_AUTH_SAE_PSK) && auth_alg == DOT11_SAE) {
		/* sae_parse_auth will send auth frame to authenticator. */
		status = wlc_sae_parse_auth(bsscfg, hdr, body_start, body_len_start);
		if (status == DOT11_SC_SUCCESS) {
			/* set scb state to PENDING_AUTH */
			wlc_scb_setstatebit(scb, PENDING_AUTH);
			wlc_bss_mac_event(wlc, bsscfg, WLC_E_AUTH, &scb->ea, WLC_E_STATUS_SUCCESS,
				DOT11_SC_SUCCESS, auth_alg, body_start, body_len_start);
		}
		goto smf_stats;
	}
#endif /* WL_SAE */

	if (status != DOT11_SC_SUCCESS) {
		WL_INFORM(("wl%d: %s: skip IE parse, status %u\n",
		           wlc->pub->unit, __FUNCTION__, status));
		goto send_result;
	}

parse_ies:
	/* prepare IE mgmt calls */
	bzero(&upp, sizeof(upp));
	upp.notif_fn = wlc_auth_nhdlr_cb;
	upp.vsie_fn = wlc_auth_vsie_cb;
	upp.ctx = wlc;
	bzero(&ftpparm, sizeof(ftpparm));
	ftpparm.auth.alg = auth_alg;
	ftpparm.auth.seq = auth_seq;
	ftpparm.auth.scb = scb;
	ftpparm.auth.status = status;
	bzero(&pparm, sizeof(pparm));
	pparm.ft = &ftpparm;

	/* parse IEs */
	if (wlc_iem_parse_frame(wlc->iemi, bsscfg, FC_AUTH, &upp, &pparm,
	                        body, body_len) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_parse_frame failed\n",
		          wlc->pub->unit, __FUNCTION__));
		/* Don't bail out, send response... */
	}
	status = ftpparm.auth.status;

send_result:

	/* send authentication response */
	if (status == DOT11_SC_SUCCESS && scb && (ftpparm.auth.alg == DOT11_FAST_BSS) &&
			SCB_AUTHENTICATING(scb)) {
#ifdef RXCHAIN_PWRSAVE
	/* fast switch back from rxchain_pwrsave state upon authentication */
		wlc_reset_rxchain_pwrsave_mode(ap);
#endif /* RXCHAIN_PWRSAVE */
		goto smf_stats;
	}

#ifdef RXCHAIN_PWRSAVE
	if (status == DOT11_SC_SUCCESS && scb && SCB_AUTHENTICATED(scb))
	{
		wlc_reset_rxchain_pwrsave_mode(ap);
	}
#endif /* RXCHAIN_PWRSAVE */
	if (scb != NULL) {
		wlc_ap_sendauth(wlc->ap, bsscfg, scb, auth_alg, auth_seq + 1, status, NULL,
			short_preamble, TRUE);
	} else {
		if (mac_mode_deny_client && !scb) {
			scb = wlc->band->hwrs_scb;
			status = DOT11_SC_INSUFFICIENT_BANDWIDTH;
			wlc_sendauth(bsscfg, &hdr->sa, &bsscfg->BSSID, scb, auth_alg, auth_seq +1,
				status, NULL, NULL, short_preamble);
		}
	}

smf_stats:
	WL_ASSOC_LT(("wlc_ap_authresp: status %d\n", status));
	wlc_smfstats_update(wlc, bsscfg, SMFS_TYPE_AUTH, status);
}

static void
wlc_assoc_nhdlr_cb(void *ctx, wlc_iem_nhdlr_data_t *data)
{
	if (WL_INFORM_ON()) {
		printf("%s: no parser\n", __FUNCTION__);
		prhex("IE", data->ie, data->ie_len);
	}
}

static uint8
wlc_assoc_vsie_cb(void *ctx, wlc_iem_pvsie_data_t *data)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;

	return wlc_iem_vs_get_id(wlc->iemi, data->ie);
}

static void wlc_ap_process_assocreq_next(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	assoc_decision_t * assoc_decision, struct scb *scb,
	wlc_assoc_req_t * param);
static void wlc_ap_process_assocreq_done(wlc_ap_info_t *ap, wlc_bsscfg_t *bsscfg,
	assoc_decision_t * dc, struct dot11_management_header *hdr, uint8 *body, uint body_len,
	struct scb *scb, wlc_assoc_req_t * param);
static void wlc_ap_process_assocreq_exit(wlc_ap_info_t *ap, wlc_bsscfg_t *bsscfg,
	assoc_decision_t * dc, struct dot11_management_header *hdr, uint8 *body, uint body_len,
	void *rsp, uint rsp_len, struct scb *scb, wlc_assoc_req_t * param);
static void wl_smf_stats(wlc_ap_info_t *ap, wlc_bsscfg_t *bsscfg,
	struct dot11_management_header *hdr, wlc_assoc_req_t * param);

static void wl_smf_stats(wlc_ap_info_t *ap, wlc_bsscfg_t *bsscfg,
	struct dot11_management_header *hdr, wlc_assoc_req_t * param)
{
#ifdef SMF_STATS
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;
	wlc_info_t *wlc = appvt->wlc;
	bool reassoc;
	uint8 type;

	reassoc = ((ltoh16(hdr->fc) & FC_KIND_MASK) == FC_REASSOC_REQ);
	type = (reassoc ? SMFS_TYPE_REASSOC : SMFS_TYPE_ASSOC);
	wlc_smfstats_update(wlc, bsscfg, type, param->status);
#endif // endif
}

/* respond to association and reassociation requests */
void
wlc_ap_process_assocreq(wlc_ap_info_t *ap, wlc_bsscfg_t *bsscfg,
	struct dot11_management_header *hdr, uint8 *body, uint body_len,
	struct scb *scb, bool short_preamble)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;
	wlc_info_t *wlc = appvt->wlc;
	ap_bsscfg_cubby_t *ap_cfg = AP_BSSCFG_CUBBY(appvt, bsscfg);
#if defined(BCMDBG_ERR) || defined(WLMSG_ASSOC) || defined(WLMSG_WSEC)
	char eabuf[ETHER_ADDR_STR_LEN], *sa = bcm_ether_ntoa(&hdr->sa, eabuf);
#endif // endif
	struct dot11_assoc_req *req = (struct dot11_assoc_req *) body;
	wlc_rateset_t req_rates;
	wlc_rateset_t sup_rates, ext_rates;
	uint8 req_rates_lookup[WLC_MAXRATE+1];
	bool reassoc;
	bool erp_sta;
	uint16 capability;
	uint16 status = DOT11_SC_SUCCESS;
	uint i;
	uint8 r;
	int idx;
	assoc_decision_t decision;
	wlc_assoc_req_t param_buf;
	wlc_assoc_req_t * param;
	/* Used to encapsulate data to a generate event */
	bool akm_ie_included = FALSE;
	wlc_bss_info_t *current_bss;
	uint16 type;
	opmode_cap_t opmode_cap_curr = OMC_NONE;
	opmode_cap_t opmode_cap_reqd = ap_cfg->opmode_cap_reqd;
	wlc_iem_upp_t upp;
	wlc_iem_pparm_t pparm;
	wlc_iem_ft_pparm_t ftpparm;
	uint8 *tlvs;
	uint tlvs_len;
#ifndef BCM_STA_SUPP_CHAN_ROM_COMPAT
	wlc_supp_channels_t supp_chan;
	wlc_supp_channels_t *supp_chan_cubby = NULL;
#endif /* BCM_STA_SUPP_CHAN_ROM_COMPAT */

	WL_TRACE(("wl%d: %s\n", wlc->pub->unit, __FUNCTION__));

	ASSERT(bsscfg != NULL);
	ASSERT(bsscfg->up);
	ASSERT(BSSCFG_AP(bsscfg));

	param = &param_buf;
#if defined(EXT_STA) || defined(SPLIT_ASSOC)
	if (WLEXTSTA_ENAB(wlc->pub) || SPLIT_ASSOC_REQ(bsscfg)) {
		param = SCB_ASSOC_CUBBY(appvt, scb);
		if (param == NULL)
			return;

		/*
		* ignore the req if there is one pending, based on the assumption
		* that param was zeroed on allocation (as part of scbpub in wlc_userscb_alloc)
		*
		* free this memory in case host misses this event, or never respond
		*/
		if (param->body != NULL) {
			wl_smf_stats(ap, bsscfg, hdr, param);
			return;
		}
	}
#endif /* EXT_STA || SPLIT_ASSOC */
	bzero(param, sizeof(wlc_assoc_req_t));
	param->hdr = hdr;
	param->body = body;
#if defined(EXT_STA) || defined(SPLIT_ASSOC)
	if (WLEXTSTA_ENAB(wlc->pub) || SPLIT_ASSOC_REQ(bsscfg)) {
		param->buf_len = body_len + sizeof(struct dot11_management_header);
		param->body = MALLOC(wlc->osh, param->buf_len);
		if (param->body == NULL) {
			WL_ERROR(("wl%d: wlc_process_assocreq - no memory for len %d\n",
				wlc->pub->unit, body_len));
			goto done;
		}
		bcopy(body, param->body, body_len);
		param->hdr = (struct dot11_management_header *)((char *)param->body + body_len);
		bcopy(hdr, param->hdr, sizeof(struct dot11_management_header));

		/* start using copied header and body */
		hdr = param->hdr;
		body = param->body;
	}
#endif /* defined(EXT_STA) */
	param->ap = ap;
	param->bsscfg = bsscfg;
	param->body_len = body_len;
	param->scb = scb;
	param->short_preamble = short_preamble;

	/* Get the AID, assign a new one if needed */
	if (!scb->aid)
		scb->aid = wlc_bsscfg_newaid(scb->bsscfg);

	param->aid = scb->aid;

#ifdef RXCHAIN_PWRSAVE
	/* fast switch back from rxchain_pwrsave state upon association */
	wlc_reset_rxchain_pwrsave_mode(ap);
#endif /* RXCHAIN_PWRSAVE */

	current_bss = bsscfg->current_bss;

	bzero(&req_rates, sizeof(wlc_rateset_t));

	reassoc = ((ltoh16(hdr->fc) & FC_KIND_MASK) == FC_REASSOC_REQ);
	capability = ltoh16(req->capability);

	type = reassoc ? FC_REASSOC_REQ : FC_ASSOC_REQ;

	if (SCB_AUTHENTICATED(scb) && SCB_ASSOCIATED(scb) && SCB_MFP(scb)) {
		status = DOT11_SC_ASSOC_TRY_LATER;
		goto done;
	}

	/* PR38742 WAR: do not process auth frames while AP scan is in
	 * progress, it may cause frame going out on a different
	 * band/channel
	 */
	if (SCAN_IN_PROGRESS(wlc->scan)) {
		WL_ASSOC(("wl%d: AP Scan in progress, abort association\n", wlc->pub->unit));
		status = DOT11_SC_ASSOC_BUSY_FAIL;
		goto exit;
	}

#ifdef BCMAUTH_PSK
	if (AP_ENAB(wlc->pub) && WSEC_TKIP_ENABLED(bsscfg->wsec) && bsscfg->tkip_countermeasures) {
		uint32 cur_tick = OSL_SYSUPTIME();
		if (wlc_clear_tkip_cm_bt(cur_tick, bsscfg->tk_cm_bt_tmstmp)) {
			bsscfg->tk_cm_dt = 0;
			bsscfg->tk_cm_bt = 0;
			bsscfg->tkip_countermeasures = FALSE;
		} else {
			return;
		}
	}
#endif /* BCMAUTH_PSK */

	if ((reassoc && body_len < DOT11_REASSOC_REQ_FIXED_LEN) ||
	    (!reassoc && body_len < DOT11_ASSOC_REQ_FIXED_LEN)) {
		status = DOT11_SC_FAILURE;
		goto exit;
	}

	/* set up some locals to hold info from the (re)assoc packet */
	if (!reassoc) {
		tlvs = body + DOT11_ASSOC_REQ_FIXED_LEN;
		tlvs_len = body_len - DOT11_ASSOC_REQ_FIXED_LEN;
	} else {
		tlvs = body + DOT11_REASSOC_REQ_FIXED_LEN;
		tlvs_len = body_len - DOT11_REASSOC_REQ_FIXED_LEN;
	}

	/* Init per scb WPA_auth and wsec.
	 * They will be reinitialized when parsing the AKM IEs.
	 */
	scb->WPA_auth = WPA_AUTH_DISABLED;
	scb->wsec = 0;

	/* send up all IEs in the WLC_E_ASSOC_IND/WLC_E_REASSOC_IND event */
	param->e_data = tlvs;
	param->e_datalen = tlvs_len;

	/* SSID is first tlv */
	param->ssid = (bcm_tlv_t*)tlvs;
	if (!bcm_valid_tlv((bcm_tlv_t *)tlvs, tlvs_len)) {
		status = DOT11_SC_FAILURE;
		goto exit;
	}

	/* prepare IE mgmt parse calls */
	bzero(&upp, sizeof(upp));
	upp.notif_fn = wlc_assoc_nhdlr_cb;
	upp.vsie_fn = wlc_assoc_vsie_cb;
	upp.ctx = wlc;
	bzero(&ftpparm, sizeof(ftpparm));
	ftpparm.assocreq.sup = &sup_rates;
	ftpparm.assocreq.ext = &ext_rates;
	ftpparm.assocreq.scb = scb;
	ftpparm.assocreq.status = status;
#ifndef BCM_STA_SUPP_CHAN_ROM_COMPAT
	ftpparm.assocreq.supp_chan = &supp_chan;
#endif /* BCM_STA_SUPP_CHAN_ROM_COMPAT */
	bzero(&pparm, sizeof(pparm));
	pparm.ft = &ftpparm;
	pparm.ht = N_ENAB(wlc->pub);
#ifdef WL11AC
	pparm.vht = VHT_ENAB(wlc->pub);
#endif // endif

	/* parse IEs */
	if (wlc_iem_parse_frame(wlc->iemi, bsscfg, type, &upp, &pparm,
	                        tlvs, tlvs_len) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_parse_frame failed\n",
		          wlc->pub->unit, __FUNCTION__));
		status = ftpparm.assocreq.status;
		ASSERT(status != DOT11_SC_SUCCESS);
#ifdef WLFBT
		if ((status == DOT11_SC_INVALID_FTIE) || (status == DOT11_SC_INVALID_AKMP) ||
			(status == DOT11_SC_INVALID_PMKID) || (status == DOT11_SC_INVALID_MDID) ||
			(status == DOT11_SC_INVALID_RSNIE) ||
			(status == DOT11_SC_INVALID_RSNIE_CAP)) {
			goto done;
		}
#endif /* WLFBT */
		goto exit;
	}
	status = ftpparm.assocreq.status;

	ASSERT(bsscfg == scb->bsscfg);

	/* store the advertised capability field */
	scb->cap = capability;

#ifndef BCM_STA_SUPP_CHAN_ROM_COMPAT
	/* store the advertised supported channels list */
	supp_chan_cubby = SCB_SUPP_CHAN_CUBBY(appvt, scb);

	if (supp_chan_cubby) {
		bzero(supp_chan_cubby, sizeof(wlc_supp_channels_t));
		if (supp_chan.count && (supp_chan.count <= WL_NUMCHANNELS)) {
			supp_chan_cubby->count = supp_chan.count;
			bcopy(supp_chan.chanset, supp_chan_cubby->chanset,
				sizeof(supp_chan.chanset));
		}
	}
#endif /* BCM_STA_SUPP_CHAN_ROM_COMPAT */

	/* qualify WPS IE */
	if (scb->wpaie && !bcm_find_wpsie(scb->wpaie, scb->wpaie[TLV_LEN_OFF] + TLV_HDR_LEN))
		akm_ie_included = TRUE;

	param->akm_ie_included = akm_ie_included;

	/* ===== Assoc Request Validations Start Here ===== */

	/* rates validation */
	if (wlc_combine_rateset(wlc, &sup_rates, &ext_rates, &req_rates) != BCME_OK) {
		WL_ERROR(("wl%d: could not parse the rateset from (Re)Assoc Request "
		          "packet from %s\n", wlc->pub->unit, sa));
		status = DOT11_SC_ASSOC_RATE_MISMATCH;
		goto done;
	}
	if (wlc->htphy_membership != req_rates.htphy_membership) {
		WL_ASSOC(("wl%d: mismatch Membership from (Re)Assoc Request packet from"
			  " %s\n", wlc->pub->unit, sa));
		status = DOT11_SC_ASSOC_RATE_MISMATCH;
		goto done;
	}

	/* catch the case of an already assoc. STA */
	if (SCB_ASSOCIATED(scb)) {
		/* If we are in PS mode then return to non PS mode as there is a state mismatch
		 * between the STA and the AP
		 */
		if (SCB_PS(scb))
			wlc_apps_scb_ps_off(wlc, scb, TRUE);

#ifdef WLFBT
		if (scb->auth_alg != DOT11_FAST_BSS)
#endif /* WLFBT */
		{
			/* return STA to auth state and check the (re)assoc pkt */
			wlc_scb_clearstatebit(scb, ~AUTHENTICATED);
		}
		wlc_scb_disassoc_cleanup(wlc, scb);
	}

	/* check if we are authenticated w/ this particular bsscfg */
	/*
	  get the index for that bsscfg. Would be nicer to find a way to represent
	  the authentication status directly.
	*/
	idx = WLC_BSSCFG_IDX(bsscfg);
	if (idx < 0) {
		WL_ERROR(("wl%d: %s: association request for non existent bsscfg\n",
		        wlc->pub->unit, sa));
		status = DOT11_SC_ASSOC_FAIL;
		goto done;
	}
	if (!SCB_AUTHENTICATED_BSSCFG(scb, idx)) {
		WL_ASSOC(("wl%d: %s: association request for non-authenticated station\n",
		        wlc->pub->unit, sa));
		status = DOT11_SC_ASSOC_FAIL;
		goto done;
	}

	/* validate 11h */
	if (WL11H_ENAB(wlc) &&
	    (wlc_11h_get_spect(wlc->m11h) == SPECT_MNGMT_STRICT_11H) &&
	    ((capability & DOT11_CAP_SPECTRUM) == 0)) {

		/* send a failure association response to this STA */
		WL_REGULATORY(("wl%d: %s: association denied as spectrum management is required\n",
		               wlc->pub->unit, sa));
		status = DOT11_SC_ASSOC_SPECTRUM_REQUIRED;
		goto done;
	}

#ifdef BAND5G
	/* Previously detected radar and are in the
	 * process of switching to a new channel
	 */
	if (wlc->block_datafifo & DATA_BLOCK_QUIET) {
		/* send a failure association response to this node */
		WL_REGULATORY(("wl%d: %s: association denied while in radar avoidance mode\n",
		        wlc->pub->unit, sa));
		status = DOT11_SC_ASSOC_FAIL;
		goto done;
	}
#endif /* BAND5G */

	/* get the requester's rates into a lookup table and record ERP capability */
	erp_sta = FALSE;
	bzero(req_rates_lookup, sizeof(req_rates_lookup));
	for (i = 0; i < req_rates.count; i++) {
		r = req_rates.rates[i] & RATE_MASK;
		if ((r > WLC_MAXRATE) || (rate_info[r] == 0)) {
			WL_INFORM(("%s: bad rate in rate set 0x%x\n", __FUNCTION__, r));
			continue;
		}
		req_rates_lookup[r] = r;
		if (IS_OFDM(r))
			erp_sta = TRUE;
	}

	if (erp_sta)
		opmode_cap_curr = OMC_ERP;

	/* update the scb's capability flags */
	scb->flags &= ~(SCB_NONERP | SCB_LONGSLOT | SCB_SHORTPREAMBLE);
	if (wlc->band->gmode && !erp_sta)
		scb->flags |= SCB_NONERP;
	if (wlc->band->gmode && (!(capability & DOT11_CAP_SHORTSLOT)))
		scb->flags |= SCB_LONGSLOT;
	if (capability & DOT11_CAP_SHORT)
		scb->flags |= SCB_SHORTPREAMBLE;

#ifdef WL11K_AP
	if (WL11K_ENAB(wlc->pub) && wlc_rrm_enabled(wlc->rrm_info, bsscfg)) {
		scb->flags3 &= ~SCB3_RRM;
		if (capability & DOT11_CAP_RRM)
			scb->flags3 |= SCB3_RRM;
	}
#endif /* WL11K_AP */

	/* check the required rates */
	for (i = 0; i < current_bss->rateset.count; i++) {
		/* check if the rate is required */
		r = current_bss->rateset.rates[i];
		if (r & WLC_RATE_FLAG) {
			if (req_rates_lookup[r & RATE_MASK] == 0) {
				/* a required rate was not available */
				WL_ERROR(("wl%d: %s does not support required rate %d\n",
				        wlc->pub->unit, sa, r & RATE_MASK));
				status = DOT11_SC_ASSOC_RATE_MISMATCH;
				goto done;
			}
		}
	}

	/* verify mcs basic rate settings */
	/* XXX need to rearchitect wlc_ht_update_scbstate() and wlc_vht_update_scbstate()
	 * to take relevant IEs independently in order to move these validations/updates
	 * into each IEs' handlers.
	 */
	if (N_ENAB(wlc->pub)) {
		ht_cap_ie_t *ht_cap_p = NULL;

		/* find the HT cap IE, if found copy the mcs set into the requested rates */
		if (ftpparm.assocreq.ht_cap_ie != NULL) {
			ht_cap_p = wlc_read_ht_cap_ie(wlc, ftpparm.assocreq.ht_cap_ie,
			        TLV_HDR_LEN + ftpparm.assocreq.ht_cap_ie[TLV_LEN_OFF]);

			wlc_ht_update_scbstate(wlc, scb, ht_cap_p, NULL, NULL);
			if (ht_cap_p != NULL) {
				bcopy(ht_cap_p->supp_mcs, req_rates.mcs, MCSSET_LEN);

				/* verify mcs basic rate settings */
				for (i = 0; i < MCSSET_LEN; i++) {
					if ((wlc->ht_add.basic_mcs[i] & req_rates.mcs[i]) !=
					    wlc->ht_add.basic_mcs[i]) {
						/* a required rate was not available */
						WL_ERROR(("wl%d: %s does not support required mcs "
						          "idx %d\n", wlc->pub->unit, sa, i));
						status = DOT11_SC_ASSOC_RATE_MISMATCH;
						goto done;
					}
				}

				opmode_cap_curr = OMC_HT;
			}
		}
#ifdef WL11AC
		if (VHT_ENAB_BAND(wlc->pub, wlc->band->bandtype)) {
			vht_cap_ie_t *vht_cap_p = NULL;
			vht_cap_ie_t vht_cap;
			vht_op_ie_t *vht_op_p = NULL;
			vht_op_ie_t vht_op;

			if (ftpparm.assocreq.vht_cap_ie != NULL)
				vht_cap_p = wlc_read_vht_cap_ie(wlc->vhti,
					ftpparm.assocreq.vht_cap_ie,
				        TLV_HDR_LEN + ftpparm.assocreq.vht_cap_ie[TLV_LEN_OFF],
				        &vht_cap);
			if (ftpparm.assocreq.vht_op_ie != NULL)
				vht_op_p =  wlc_read_vht_op_ie(wlc->vhti,
					ftpparm.assocreq.vht_op_ie,
				        TLV_HDR_LEN + ftpparm.assocreq.vht_op_ie[TLV_LEN_OFF],
				        &vht_op);
			wlc_vht_update_scb_state(wlc->vhti, wlc->band->bandtype, scb, ht_cap_p,
			                vht_cap_p, vht_op_p, ftpparm.assocreq.vht_ratemask);

			if (vht_cap_p != NULL)
				opmode_cap_curr = OMC_VHT;
		}
#endif /* WL11AC */
	}

	/*
	 * Verify whether the operation capabilities of current STA
	 * acceptable for this BSS based on required setting.
	 */
	if (opmode_cap_curr < opmode_cap_reqd) {
		WL_ASSOC(("wl%d: %s: Assoc is rejected due to oper capability "
			"mode mismatch. Reqd mode=%d, client's mode=%d\n",
			wlc->pub->unit, sa, opmode_cap_reqd, opmode_cap_curr));
		/*
		 * XXX: the standard doesn't define a status codes for this case, so
		 * returning this one.
		 */
		status = DOT11_SC_ASSOC_RATE_MISMATCH;
		goto done;
	}

	/*
	 * If in an auth mode that wants a WPA info element, look for one.
	 * If found, check to make sure what it requests is supported.
	 */
	if ((bsscfg->WPA_auth != WPA_AUTH_DISABLED && WSEC_ENABLED(bsscfg->wsec)) ||
	    WSEC_SES_OW_ENABLED(bsscfg->wsec)) {
		/* WPA/RSN IE is parsed in the registered IE mgmt callbacks and
		 * the scb->WPA_auth and scb->wsec should have been setup by now,
		 * otherwise it signals an abnormality in the STA assoc req frame.
		 */
		if (scb->WPA_auth == WPA_AUTH_DISABLED || !WSEC_ENABLED(scb->wsec)) {
			/* check for transition mode */
			if (!WSEC_WEP_ENABLED(bsscfg->wsec) && !WSEC_SES_OW_ENABLED(bsscfg->wsec)) {
				WL_ERROR(("wl%d: %s: "
				          "deny transition mode assoc req from %s... "
				          "transition mode not enabled on AP\n",
				          wlc->pub->unit, __FUNCTION__, sa));
				status = DOT11_SC_ASSOC_FAIL;
				goto done;
			}
		}
		WL_WSEC(("wl%d: %s: %s WPA_auth 0x%x\n", wlc->pub->unit, __FUNCTION__, sa,
		         scb->WPA_auth));
	}

	/* check the capabilities.
	 * In case OW_ENABLED, allow privacy bit to be set even if !WSEC_ENABLED if
	 * there is no wpa or rsn IE in request.
	 * This covers Microsoft doing WPS association with sec bit set even when we are
	 * in open mode..
	 */
	/* by far scb->WPA_auth should be setup if the STA has sent us appropriate request */
	if ((capability & DOT11_CAP_PRIVACY) && !WSEC_ENABLED(bsscfg->wsec) &&
	    (!WSEC_SES_OW_ENABLED(bsscfg->wsec) || akm_ie_included)) {
		WL_ERROR(("wl%d: %s is requesting privacy but encryption is not enabled on the"
		        " AP. SES OW %d WPS IE %d\n", wlc->pub->unit, sa,
		        WSEC_SES_OW_ENABLED(bsscfg->wsec), akm_ie_included));
		status = DOT11_SC_ASSOC_FAIL;
		wlc_bss_mac_event(wlc, bsscfg, WLC_E_PRUNE, &hdr->sa,
			0, WLC_E_PRUNE_ENCR_MISMATCH, 0, 0, 0);
		goto done;
	}

	/* When WEP is enabled along with the WPA we'll deny STA request,
	 * if STA attempts with WPA IE and shared key 802.11 authentication,
	 * by deauthenticating the STA...
	 */
	if ((bsscfg->WPA_auth != WPA_AUTH_DISABLED) && WSEC_ENABLED(bsscfg->wsec) &&
	    (WSEC_WEP_ENABLED(bsscfg->wsec))) {
		/*
		 * WPA with 802.11 open authentication is required, or 802.11 shared
		 * key authentication without WPA authentication attempt (no WPA IE).
		 */
		/* attempt WPA auth with 802.11 shared key authentication */
		if (scb->auth_alg == DOT11_SHARED_KEY && scb->WPA_auth != WPA_AUTH_DISABLED) {
			WL_ERROR(("wl%d: WPA auth attempt with 802.11 shared key auth from %s, "
			          "deauthenticating...\n", wlc->pub->unit, sa));
			(void)wlc_senddeauth(wlc, bsscfg, scb, &scb->ea, &bsscfg->BSSID,
			                     &bsscfg->cur_etheraddr, DOT11_RC_AUTH_INVAL);
			status = DOT11_RC_AUTH_INVAL;
			goto done;
		}
	}

	/* If not APSTA, deny association to stations unable to support 802.11b short preamble
	 * if short network.  In APSTA, we don't enforce short preamble on the AP side since it
	 * might have been set by the STA side.  We need an extra flag for enforcing short
	 * preamble independently.
	 */
	if (!APSTA_ENAB(wlc->pub) && BAND_2G(wlc->band->bandtype) &&
	    bsscfg->PLCPHdr_override == WLC_PLCP_SHORT && !(capability & DOT11_CAP_SHORT)) {
		WL_ERROR(("wl%d: %s does not support short preambles\n", wlc->pub->unit, sa));
		status = DOT11_SC_ASSOC_SHORT_REQUIRED;
		goto done;
	}
	/* deny association to stations unable to support 802.11g short slot timing
	 * if shortslot exclusive network
	 */
	if ((BAND_2G(wlc->band->bandtype)) &&
	    ap->shortslot_restrict && !(capability & DOT11_CAP_SHORTSLOT)) {
		WL_ERROR(("wl%d: %s does not support ShortSlot\n", wlc->pub->unit, sa));
		status = DOT11_SC_ASSOC_SHORTSLOT_REQUIRED;
		goto done;
	}
	/* check the max association limit */
	if (wlc_assocscb_getcnt(wlc) >= appvt->maxassoc) {
		WL_ERROR(("wl%d: %s denied association due to max association limit\n",
		          wlc->pub->unit, sa));
		status = DOT11_SC_ASSOC_BUSY_FAIL;
		goto done;
	}

#if defined(MBSS) || defined(WLP2P)
	if (wlc_bss_assocscb_getcnt(wlc, bsscfg) >= bsscfg->maxassoc) {
		WL_ERROR(("wl%d.%d %s denied association due to max BSS association "
		          "limit\n", wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), sa));
		status = DOT11_SC_ASSOC_BUSY_FAIL;
		goto done;
	}
#endif /* MBSS || WLP2P */

#ifdef WLP2P
	if (BSS_P2P_ENAB(wlc, bsscfg)) {
		if (wlc_p2p_process_assocreq(wlc->p2p, scb, tlvs, tlvs_len) != BCME_OK) {
			status = DOT11_SC_ASSOC_FAIL;
			goto done;
		}
	}
#endif // endif
	param->status = status;

	bcopy(&req_rates, &param->req_rates, sizeof(wlc_rateset_t));
#ifdef WL_SAE
	/* Copy supported mcs index bit map */
	bcopy(req_rates.mcs, scb->rateset.mcs, MCSSET_LEN);
#endif /* WL_SAE */
#if defined(EXT_STA) || defined(SPLIT_ASSOC)
	if ((WLEXTSTA_ENAB(wlc->pub) && BSSCFG_HAS_NATIVEIF(bsscfg)) ||
		SPLIT_ASSOC_REQ(bsscfg)) {
		/* indicate pre-(re)association event to OS and get association
		* decision from OS from the same thread
		* vif doesn't use this mac event and needs auto-reply of assoc req
		*/
		wlc_bss_mac_event(wlc, bsscfg,
		reassoc ? WLC_E_PRE_REASSOC_IND : WLC_E_PRE_ASSOC_IND,
		&hdr->sa, WLC_E_STATUS_SUCCESS, status, 0, body, body_len);
		WL_ASSOC(("wl%d: %s: sa(%s) reassoc(%d) systime(%u)\n",
			wlc->pub->unit, __FUNCTION__, sa, reassoc, OSL_SYSUPTIME()));
	} else
#endif /* defined(EXT_STA) || defined(SPLIT_ASSOC) */
	{
		decision.assoc_approved = TRUE;
		decision.reject_reason = 0;
		bcopy(&scb->ea, &decision.da, ETHER_ADDR_LEN);
		wlc_ap_process_assocreq_next(wlc, bsscfg, &decision, scb, param);
	}

#ifdef PSTA
	/* Send WLC_E_PSTA_PRIMARY_INTF_IND event to indicate psta primary mac addr */
	if (scb->psta_prim) {
		wl_psta_primary_intf_event_t psta_prim_evet;

		WL_ASSOC(("wl%d.%d scb:"MACF", psta_prim:"MACF"\n",
			wlc->pub->unit,	WLC_BSSCFG_IDX(bsscfg),
			ETHER_TO_MACF(scb->ea), ETHER_TO_MACF(scb->psta_prim->ea)));

		memcpy(&psta_prim_evet.prim_ea, &scb->psta_prim->ea, sizeof(struct ether_addr));
		wlc_bss_mac_event(wlc, bsscfg, WLC_E_PSTA_PRIMARY_INTF_IND,
			&scb->ea, WLC_E_STATUS_SUCCESS, 0, 0,
			&psta_prim_evet, sizeof(wl_psta_primary_intf_event_t));
	}
#endif /* PSTA */

	return;

	/* ===== Assoc Request Validations End ===== */

done:
	param->status = status;
	wlc_ap_process_assocreq_done(ap, bsscfg, &decision, hdr, body, body_len, scb, param);
	goto body_free;

exit:
	if (WLEXTSTA_ENAB(wlc->pub)) {
		wlc_ap_process_assocreq_exit(ap, bsscfg, &decision, hdr, body, body_len, NULL, 0,
			scb, param);
	} else {
		wl_smf_stats(ap, bsscfg, hdr, param);
		/* fall through */
	}
body_free:
#if defined(EXT_STA) || defined(SPLIT_ASSOC)
	if ((WLEXTSTA_ENAB(wlc->pub) || SPLIT_ASSOC_REQ(bsscfg)) && param->body) {
		MFREE(wlc->osh, param->body, param->buf_len);
		bzero(param, sizeof(wlc_assoc_req_t));
	}
#endif /* defined(EXT_STA) || defined(SPLIT_ASSOC) */
	WL_ASSOC_LT(("wlc_ap_process_assocreq status %d\n", status));
	return;
}
	/*
	 * here if association request parsing is successful...
	 */

void wlc_ap_process_assocreq_decision(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, assoc_decision_t *dc)
{
#if defined(EXT_STA)|| defined(SPLIT_ASSOC)
	wlc_ap_process_assocreq_next(wlc, bsscfg, dc, NULL, NULL);
#endif /* defined(EXT_STA) || defined(SPLIT_ASSOC) */
}

static void wlc_ap_process_assocreq_next(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	assoc_decision_t * dc, struct scb *scb, wlc_assoc_req_t * param)
{
	wlc_ap_info_t *ap;
	struct dot11_management_header *hdr;
	uint8 *body;
	uint body_len;
#if defined(EXT_STA)|| defined(SPLIT_ASSOC)
	wlc_ap_info_pvt_t *appvt;
#endif // endif
	struct dot11_assoc_req *req;
	wlc_bss_info_t *current_bss;
	uint16 capability;
#if defined(BCMDBG_ERR) || defined(WLMSG_ASSOC)
	char dabuf[ETHER_ADDR_STR_LEN], *da = bcm_ether_ntoa(&dc->da, dabuf);
#endif // endif
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char sabuf[ETHER_ADDR_STR_LEN], *sa;
#endif // endif

	WL_TRACE(("wl%d: wlc_process_assocreq_next\n", wlc->pub->unit));

	ASSERT(bsscfg != NULL);
	ASSERT(bsscfg->up);
	ASSERT(BSSCFG_AP(bsscfg));

	ap = wlc->ap;

#if defined(EXT_STA)|| defined(SPLIT_ASSOC)
	appvt = (wlc_ap_info_pvt_t *) ap;
	if (WLEXTSTA_ENAB(wlc->pub) || SPLIT_ASSOC_REQ(bsscfg)) {

		scb = wlc_scbfind(wlc, bsscfg, &dc->da);
		if (scb == NULL) {
			WL_ERROR(("wl%d.%d %s could not find scb\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), da));
			return;
		}

		param = SCB_ASSOC_CUBBY(appvt, scb);
		if (!param || !param->body) {
			WL_ERROR(("wl%d.%d assocreq NULL CUBBY body\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg)));
			return;
		}

		if (scb != param->scb) {
			WL_ERROR(("wl%d.%d %s wrong scb found\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), da));
			return;
		}
	}
#endif /* EXT_STA || SPLIT_ASSOC */

#if  defined(BCMDBG) || defined(WLMSG_ASSOC)
	sa = bcm_ether_ntoa(&param->hdr->sa, sabuf);
#endif // endif

	if (scb == NULL) {
		WL_ERROR(("wl%d.%d %s could not find scb\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), da));
		return;
	}
	hdr = param->hdr;
	body = param->body;
	body_len = param->body_len;
	req = (struct dot11_assoc_req *) body;

	if (!dc->assoc_approved) {
		param->status = dc->reject_reason;
		WL_ERROR(("wl%d.%d %s denied association status(%d)\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), da, dc->reject_reason));
		goto done;
	}

#if defined(SPLIT_ASSOC)
	if (SPLIT_ASSOC_REQ(bsscfg)) {
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
		WL_ASSOC(("wl%d: %s: da(%s) systime(%u)\n",
			wlc->pub->unit, __FUNCTION__, da, OSL_SYSUPTIME()));
#endif // endif
		wlc_assoc_dc_dispatch(wlc, bsscfg, dc, scb);
	}
#endif /* SPLIT_ASSOC */

	capability = ltoh16(req->capability);
	current_bss = bsscfg->current_bss;

	/* WEP encryption */
	if (scb->WPA_auth == WPA_AUTH_DISABLED &&
	    WSEC_WEP_ENABLED(bsscfg->wsec))
		scb->wsec = WEP_ENABLED;

	/* XXX JQL
	 * we should have a callback mechanism to call registered scb cubby owners
	 * to reset any relevant data for the (re)association.
	 */
	wlc_ap_tpc_assoc_reset(wlc->tpc, scb);

	/* When a HT or VHT STA using TKIP or WEP only unicast cipher suite tries
	 * to associate, exclude HT and VHT IEs from assoc response to force
	 * the STA to operate in legacy mode.
	 */
	if (WSEC_ENABLED(bsscfg->wsec)) {
		bool keep_ht = TRUE;

		ASSERT(param->req_rates.count > 0);
		if (scb->wsec == TKIP_ENABLED &&
		    (wlc->ht_wsec_restriction & WLC_HT_TKIP_RESTRICT)) {
			keep_ht = FALSE;
		} else if (scb->wsec == WEP_ENABLED &&
		           (wlc->ht_wsec_restriction & WLC_HT_WEP_RESTRICT)) {
			keep_ht = FALSE;
		}

		if (!keep_ht) {
			if (N_ENAB(wlc->pub) && SCB_HT_CAP(scb))
				wlc_ht_update_scbstate(wlc, scb, NULL, NULL, NULL);
#ifdef WL11AC
			if (VHT_ENAB_BAND(wlc->pub, wlc->band->bandtype) && SCB_VHT_CAP(scb))
				wlc_vht_update_scb_state(wlc->vhti,
					wlc->band->bandtype, scb, NULL, NULL, NULL, 0);
#endif /* WL11AC */
		}
	}

	wlc_prot_g_cond_upd(wlc->prot_g, scb);
	wlc_prot_n_cond_upd(wlc->prot_n, scb);

#ifdef RADIO_PWRSAVE
	if (RADIO_PWRSAVE_ENAB(wlc->ap) && wlc_radio_pwrsave_in_power_save(wlc->ap)) {
		wlc_radio_pwrsave_exit_mode(wlc->ap);
		WL_INFORM(("We have an assoc request, going right out of the power save mode!\n"));
	}
#endif // endif
#ifdef WL11N
	/* Check for association of a 40 intolerant STA */
	if ((scb->flags & SCB_HT40INTOLERANT)) {
		if (N_ENAB(wlc->pub) && COEX_ENAB(wlc->pub)) {
			wlc_ht_upd_coex_state(bsscfg, DOT11_OBSS_COEX_40MHZ_INTOLERANT);

			if (CHSPEC_IS40(current_bss->chanspec)) {
				wlc_ht_coex_update_fid_time(bsscfg);
				wlc_ht_coex_switch_bw(bsscfg, TRUE, COEX_DGRADE_40INTOL);
			}
		}
	}
#endif /* WL11N */

	if (wlc->band->gmode)
		wlc_prot_g_mode_upd(wlc->prot_g, bsscfg);

	if (N_ENAB(wlc->pub))
		wlc_prot_n_mode_upd(wlc->prot_n, bsscfg);

	WL_ASSOC(("AP: Checking if WEP key needs to be inserted\n"));
	/* If Multi-SSID is enabled, and Legacy WEP is in use for this bsscfg, a "pairwise" key must
	   be created by copying the default key from the bsscfg.
	*/
	if (bsscfg->WPA_auth == WPA_AUTH_DISABLED)
		WL_ASSOC(("WPA disabled \n"));

	/* Since STA is declaring privacy w/o WPA IEs => WEP */
	if ((scb->wsec == 0) && (capability & DOT11_CAP_PRIVACY) &&
	    WSEC_WEP_ENABLED(bsscfg->wsec))
		scb->wsec = WEP_ENABLED;

	if (WSEC_WEP_ENABLED(bsscfg->wsec))
		WL_ASSOC(("WEP enabled \n"));
	if (MBSS_ENAB(wlc->pub))
		WL_ASSOC(("MBSS on \n"));
	if ((MBSS_ENAB(wlc->pub) || PSTA_ENAB(wlc->pub) || bsscfg != wlc->cfg) &&
	    bsscfg->WPA_auth == WPA_AUTH_DISABLED && WSEC_WEP_ENABLED(bsscfg->wsec)) {
		wsec_key_t *defkey = WSEC_BSS_DEFAULT_KEY(bsscfg);
		if (defkey)
			WL_ASSOC(("Def key installed \n"));
		if (defkey &&
		    (defkey->algo == CRYPTO_ALGO_WEP1 ||
		     defkey->algo == CRYPTO_ALGO_WEP128)) {
			int bcmerror;
			WL_ASSOC(("wl%d: %s Inserting key \n",
			        wlc->pub->unit, sa));
			bcmerror = wlc_key_insert(wlc, bsscfg, defkey->len, defkey->id,
			        defkey->flags,
			        defkey->algo, defkey->data, &scb->ea, NULL, NULL);
			ASSERT(!bcmerror);
			BCM_REFERENCE(bcmerror);
		}
	}

	/* Based on wsec for STA, update AMPDU feature
	 * By spec, 11n device can send AMPDU only with Open or CCMP crypto
	 */
	if (N_ENAB(wlc->pub)) {
		/* scb->wsec is the specific unicast algo being used.
		 * It should be a subset of the whole bsscfg wsec
		 */
		ASSERT((bsscfg->wsec & scb->wsec) == scb->wsec);
		if (SCB_AMPDU(scb)) {
			if ((scb->wsec == WEP_ENABLED) ||
			    (scb->wsec == TKIP_ENABLED) ||
			    SCB_MFP(scb)) {
				wlc_scb_ampdu_disable(wlc, scb);
			}
			else {
				wlc_scb_ampdu_enable(wlc, scb);
			}
		}
	}

#ifdef TRAFFIC_MGMT
	if (!scb->wsec) {
		wlc_scb_trf_mgmt(wlc, bsscfg, scb);
	}
#endif // endif
done:
#ifndef WL_SAE_ASSOC_OFFLOAD
#ifdef WL_SAE
	/* STA has chosen SAE AKM */
	if ((scb->WPA_auth == WPA3_AUTH_SAE_PSK) &&
			scb->pmkid_included) {
		/* set scb state to PENDING_ASSOC */
		wlc_scb_setstatebit(scb, PENDING_ASSOC);
		/* Send WLC_E_ASSOC event to cfg80211 layer */
		wlc_bss_mac_event(wlc, bsscfg, WLC_E_ASSOC, &hdr->sa, 0, 0, scb->auth_alg,
				param->e_data, param->e_datalen);
		return;
	}
	else
#endif /* WL_SAE */
#endif /* WL_SAE_ASSOC_OFFLOAD */
	{
		wlc_ap_process_assocreq_done(ap, bsscfg, dc, hdr, body, body_len, scb, param);
	}
#if defined(EXT_STA)|| defined(SPLIT_ASSOC)
	if (WLEXTSTA_ENAB(wlc->pub) || SPLIT_ASSOC_REQ(bsscfg)) {
		MFREE(wlc->osh, param->body, param->buf_len);
		bzero(param, sizeof(wlc_assoc_req_t));
	}
#endif /* defined (EXT_STA) || defined(SPLIT_ASSOC) */
	return;
}

	/* ===== Prepare Assoc Response ===== */
static void wlc_ap_process_assocreq_done(wlc_ap_info_t *ap, wlc_bsscfg_t *bsscfg,
	assoc_decision_t * dc, struct dot11_management_header *hdr,
	uint8 *body, uint body_len, struct scb *scb, wlc_assoc_req_t * param)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;
	wlc_info_t *wlc = appvt->wlc;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN], *sa = bcm_ether_ntoa(&hdr->sa, eabuf);
#endif // endif
	struct dot11_assoc_req *req = (struct dot11_assoc_req *) body;

	wlc_rateset_t sup_rates, ext_rates;
	struct ether_addr *reassoc_ap = NULL;
	wlc_bss_info_t *current_bss;
	struct dot11_assoc_resp *resp;
	uint8 *rsp = NULL;
	uint rsp_len = 0;
	uint8 rates;
	bool reassoc;
	uint16 listen;
	uint8 mcsallow = 0;
	wlc_iem_ft_cbparm_t ftcbparm;
	wlc_iem_cbparm_t cbparm;
	uint16 type;
	void *pkt;
	wlc_rateset_t *req_rates = &param->req_rates;

	listen = ltoh16(req->listen);
	current_bss = bsscfg->current_bss;
	reassoc = ((ltoh16(hdr->fc) & FC_KIND_MASK) == FC_REASSOC_REQ);
	type = reassoc ? FC_REASSOC_RESP : FC_ASSOC_RESP;

	if (reassoc) {
		struct dot11_reassoc_req *reassoc_req = (struct dot11_reassoc_req *) body;
		reassoc_ap = &reassoc_req->ap;
	}

	/* create the supported rates and extended supported rates elts */
	bzero(&sup_rates, sizeof(wlc_rateset_t));
	bzero(&ext_rates, sizeof(wlc_rateset_t));
	/* check for a supported rates override */
	if (wlc->sup_rates_override.count > 0)
		bcopy(&wlc->sup_rates_override, &sup_rates, sizeof(wlc_rateset_t));
	wlc_rateset_elts(wlc, bsscfg, &current_bss->rateset, &sup_rates, &ext_rates);

	/* filter rateset for the BSS supported rates */
	wlc_rate_hwrs_filter_sort_validate(req_rates, &current_bss->rateset, FALSE,
		wlc->stf->txstreams);

	rsp_len = DOT11_ASSOC_RESP_FIXED_LEN;

	/* prepare IE mgmt calls */
	bzero(&ftcbparm, sizeof(ftcbparm));
	ftcbparm.assocresp.mcs = req_rates->mcs;
	ftcbparm.assocresp.scb = scb;
	ftcbparm.assocresp.status = param->status;
	ftcbparm.assocresp.sup = &sup_rates;
	ftcbparm.assocresp.ext = &ext_rates;
	bzero(&cbparm, sizeof(cbparm));
	cbparm.ft = &ftcbparm;
	cbparm.ht = SCB_HT_CAP(scb);
#ifdef WL11AC
	cbparm.vht = SCB_VHT_CAP(scb);
#endif // endif

	/* calculate IEs' length */
	rsp_len += wlc_iem_calc_len(wlc->iemi, bsscfg, type, NULL, &cbparm);

	/* alloc a packet */
	if ((pkt = wlc_frame_get_mgmt(wlc, type, &hdr->sa, &bsscfg->cur_etheraddr,
	                              &bsscfg->BSSID, rsp_len, &rsp)) == NULL) {
		param->status = DOT11_SC_ASSOC_BUSY_FAIL;
		WL_ASSOC(("wl%d.%d %s %sassociattion failed rsp_len %d\n", wlc->pub->unit,
			WLC_BSSCFG_IDX(bsscfg), sa, reassoc ? "re" : "", rsp_len));
		rsp_len = 0;
		if (WLEXTSTA_ENAB(wlc->pub))
			goto exit;
		else
			goto smf_stats;
	}
	ASSERT(rsp && ISALIGNED(rsp, sizeof(uint16)));
	resp = (struct dot11_assoc_resp *) rsp;

	/* fill out the association response body */
	resp->capability = DOT11_CAP_ESS;
	if (BAND_2G(wlc->band->bandtype) &&
	    bsscfg->PLCPHdr_override == WLC_PLCP_SHORT)
		resp->capability |= DOT11_CAP_SHORT;
	if (WSEC_ENABLED(bsscfg->wsec) && bsscfg->wsec_restrict)
		resp->capability |= DOT11_CAP_PRIVACY;
	if (wlc->shortslot && wlc->band->gmode)
		resp->capability |= DOT11_CAP_SHORTSLOT;

#ifdef WL11K_AP
	if (WL11K_ENAB(wlc->pub) && wlc_rrm_enabled(wlc->rrm_info, bsscfg))
		resp->capability |= DOT11_CAP_RRM;
#endif /* WL11K_AP */

	resp->capability = htol16(resp->capability);
	resp->status = htol16(param->status);
	resp->aid = htol16(param->aid);

	rsp += DOT11_ASSOC_RESP_FIXED_LEN;
	rsp_len -= DOT11_ASSOC_RESP_FIXED_LEN;

	/* write IEs in the frame */
	if (wlc_iem_build_frame(wlc->iemi, bsscfg, type, NULL, &cbparm,
	                        rsp, rsp_len) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_build_frame failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto exit;
	}

	/* send the association response */
	wlc_queue_80211_frag(wlc, pkt, bsscfg->wlcif->qi, scb, bsscfg,
		param->short_preamble, NULL, 0);

	/* ===== Post Processing ===== */

	/* XXX why we are doing these after initiating the Response TX???
	 * and what if the queuing of Response frame fail???
	 */

	rsp -= DOT11_ASSOC_RESP_FIXED_LEN;
	rsp_len += DOT11_ASSOC_RESP_FIXED_LEN;

#ifdef MFP
	if (WLC_MFP_ENAB(wlc->pub) && SCB_MFP(scb) &&
	    SCB_AUTHENTICATED(scb) && SCB_ASSOCIATED(scb)) {
		wlc_mfp_start_sa_query(wlc->mfp, bsscfg, scb);
		goto smf_stats;
	}
#endif // endif

	/* if error, we're done */
	if (param->status != DOT11_SC_SUCCESS) {
		if (WLEXTSTA_ENAB(wlc->pub))
			goto exit;
		else
			goto smf_stats;
	}

	/*
	 * FIXME: We should really set a callback and only update association state if
	 * the assoc resp packet tx is successful..
	 */

	/* update scb state */

	WL_ASSOC(("wl%d.%d %s %sassociated\n", wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), sa,
		reassoc ? "re" : ""));

	scb->aid = param->aid;

#ifdef PROP_TXSTATUS
	if (wlc_scb_wlfc_entry_add(wlc, scb) == BCME_OK) {
		WLFC_DBGMESG(("AP: MAC-ADD for [%02x:%02x:%02x:%02x:%02x:%02x], "
			"handle: [%d], if:%d, t_idx:%d\n",
			scb->ea.octet[0],
			scb->ea.octet[1],
			scb->ea.octet[2],
			scb->ea.octet[3],
			scb->ea.octet[4],
			scb->ea.octet[5],
			scb->mac_address_handle,
			((scb->bsscfg == NULL) ? 0 : scb->bsscfg->wlcif->index),
			WLFC_MAC_DESC_GET_LOOKUP_INDEX(scb->mac_address_handle)));
	}
#endif /* PROP_TXSTATUS */

	/*
	 * scb->listen is used by the AP for timing out PS pkts,
	 * ensure pkts are held for at least one dtim period
	 */
	scb->listen = MAX(current_bss->dtim_period, listen);
	wlc_scb_setstatebit(scb, ASSOCIATED);

	/* Start beaconing if this is first STA */
	if (DYNBCN_ENAB(bsscfg) && wlc_bss_assocscb_getcnt(wlc, bsscfg) == 1) {
		wlc_bsscfg_bcn_enable(wlc, bsscfg);
	}

	scb->assoctime = wlc->pub->now;

	/* notify other APs on the DS that this station has roamed */
	if (reassoc && bcmp((char*)&bsscfg->BSSID, reassoc_ap->octet, ETHER_ADDR_LEN))
		wlc_reassoc_notify(ap, &hdr->sa, bsscfg->wlcif);

	/* 802.11f assoc. announce pkt */
	wlc_assoc_notify(ap, &hdr->sa, bsscfg->wlcif);

	/* copy sanitized set to scb */
#ifdef WLP2P
	if (BSS_P2P_ENAB(wlc, bsscfg))
		rates = WLC_RATES_OFDM;
	else
#endif // endif
	rates = WLC_RATES_CCK_OFDM;
	if (BSS_N_ENAB(wlc, bsscfg) && SCB_HT_CAP(scb))
		mcsallow |= WLC_MCS_ALLOW;
	if (BSS_VHT_ENAB(wlc, bsscfg) && SCB_VHT_CAP(scb))
		mcsallow |= WLC_MCS_ALLOW_VHT;
	if (WLPROPRIETARY_11N_RATES_ENAB(wlc->pub)) {
		if (wlc->pub->ht_features != WLC_HT_FEATURES_PROPRATES_DISAB &&
			SCB_HT_PROP_RATES_CAP(scb))
			mcsallow |= WLC_MCS_ALLOW_PROP_HT;
	}

	wlc_rateset_filter(req_rates, &scb->rateset, FALSE, rates, RATE_MASK, mcsallow);

	/* re-initialize rate info
	 * Note that this wipes out any previous rate stats on the STA. Since this
	 * being called at Association time, it does not seem like a big loss.
	 */
	wlc_scb_ratesel_init(wlc, scb);

#ifdef WLBTAMP
	if (BSS_BTA_ENAB(wlc, bsscfg) && !reassoc)
		wlc_bta_join_complete(wlc->bta, scb, 0);
#endif /* WLBTAMP */

#ifdef DWDS
	/* create dwds i/f for this scb:
	 *  a) if security is disabled;
	 *  b) if security is enabled and security type is WEP;
	 * For all other security types will create the interface
	 * once scb is authorized.
	 */
	if (((DWDS_ENAB(bsscfg) && SCB_DWDS_CAP(scb)) ||
		(MAP_ENAB(bsscfg) && SCB_MAP_CAP(scb)))&&
		(!WSEC_ENABLED(bsscfg->wsec) || WSEC_WEP_ENABLED(bsscfg->wsec)))
		wlc_wds_create(wlc, scb, WDS_DYNAMIC);
#endif /* DWDS */

#if defined(BCMAUTH_PSK)
	/* kick off 4 way handshaking */
	if (BCMAUTH_PSK_ENAB(wlc->pub) && (bsscfg->authenticator != NULL))
		wlc_auth_join_complete(bsscfg->authenticator, &scb->ea, TRUE);
#endif /* BCMAUTH_PSK */

#ifdef WLP2P
	if (BSS_P2P_ENAB(wlc, bsscfg))
		wlc_p2p_enab_upd(wlc->p2p, bsscfg);
#endif // endif

	/* Enable BTCX PS protection */
	wlc_enable_btc_ps_protection(wlc, bsscfg, TRUE);

exit:
	wlc_ap_process_assocreq_exit(ap, bsscfg, dc, hdr, body, body_len, rsp, rsp_len, scb, param);
	return;

smf_stats:
	wl_smf_stats(ap, bsscfg, hdr, param);
	return;

}

static void wlc_ap_process_assocreq_exit(wlc_ap_info_t *ap, wlc_bsscfg_t *bsscfg,
	assoc_decision_t * dc, struct dot11_management_header *hdr, uint8 *body, uint body_len,
	void *rsp, uint rsp_len, struct scb *scb, wlc_assoc_req_t * param)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;
	wlc_info_t *wlc = appvt->wlc;
	uint8 *ext_assoc_pkt;
	uint ext_assoc_pkt_len;
	bool reassoc;

	WL_ASSOC_LT(("proc assocreq exit\n"));

	reassoc = ((ltoh16(hdr->fc) & FC_KIND_MASK) == FC_REASSOC_REQ);
	/* send WLC_E_REASSOC_IND/WLC_E_ASSOC_IND to interested App and/or non-WIN7 OS */
	wlc_bss_mac_event(wlc, bsscfg, reassoc ? WLC_E_REASSOC_IND : WLC_E_ASSOC_IND,
		&hdr->sa, WLC_E_STATUS_SUCCESS, param->status,
#if defined(WLFBT) && !defined(WLFBT_DISABLED)  /* ROM friendly */
		scb->auth_alg,
#else
		0,
#endif /* WLFBT && !WLFBT_DISABLED */
		param->e_data, param->e_datalen);
	/* Send WLC_E_ASSOC_REASSOC_IND_EXT to interested App */
	ext_assoc_pkt_len = sizeof(*hdr) + body_len;
	if ((ext_assoc_pkt = (uint8*)MALLOC(bsscfg->wlc->osh, ext_assoc_pkt_len)) == NULL) {
		WL_ERROR(("wl%d.%d: %s: MALLOC(%d) failed, malloced %d bytes\n",
			WLCWLUNIT(bsscfg->wlc), WLC_BSSCFG_IDX(bsscfg), __FUNCTION__,
			ext_assoc_pkt_len, MALLOCED(bsscfg->wlc->osh)));
	} else {
		memcpy(ext_assoc_pkt, hdr, sizeof(*hdr));
		memcpy(ext_assoc_pkt + sizeof(*hdr), body, body_len);

		wlc_bss_mac_event(wlc, bsscfg, WLC_E_ASSOC_REASSOC_IND_EXT,
			&hdr->sa, WLC_E_STATUS_SUCCESS, param->status,
#if defined(WLFBT) && !defined(WLFBT_DISABLED)  /* ROM friendly */
			scb->auth_alg,
#else
			0,
#endif /* WLFBT && !WLFBT_DISABLED */
			ext_assoc_pkt, ext_assoc_pkt_len);
		MFREE(bsscfg->wlc->osh, ext_assoc_pkt, ext_assoc_pkt_len);
	}
	/* Suspend STA sniffing if it is associated to AP  */
	if (STAMON_ENAB(wlc->pub) && STA_MONITORING(wlc, &hdr->sa))
		wlc_stamon_sta_sniff_enab(wlc->stamon_info, &hdr->sa, FALSE);
#ifdef WLWNM_AP
	if (WLWNM_ENAB(wlc->pub) && WNM_MAXIDLE_ENABLED(wlc_wnm_get_cap(wlc, bsscfg)))
			wlc_wnm_rx_tstamp_update(wlc, scb);
#endif /* WLWNM_AP */
#if defined(EXT_STA)
	if (WLEXTSTA_ENAB(wlc->pub)) {
		assoc_info_t *assoc_info = NULL;
		int bcn_tmpl_len = wlc->pub->bcn_tmpl_len;
		int ind_status;
		uint8 *ptr;

		uint32 total = sizeof(assoc_info_t) + ROUNDUP(body_len, sizeof(uint16))
			+ ROUNDUP(rsp_len, sizeof(uint16)) + ROUNDUP(bcn_tmpl_len, sizeof(uint16))
			+ scb->wpaie_len;

		if (dc && !dc->assoc_approved) {
			ind_status = WLC_E_STATUS_ABORT;
		} else if (param->status == DOT11_SC_SUCCESS &&
			(assoc_info = (assoc_info_t *)MALLOC(wlc->osh, total)) != NULL) {
			bzero(assoc_info, total);
			assoc_info->assoc_req = sizeof(assoc_info_t);
			assoc_info->assoc_req_len = body_len;
			assoc_info->assoc_rsp = ((int32)assoc_info->assoc_req +
				ROUNDUP(body_len, sizeof(uint16)));
			assoc_info->assoc_rsp_len = rsp_len;
			assoc_info->bcn = ((int32)assoc_info->assoc_rsp +
				ROUNDUP(rsp_len, sizeof(uint16)));
			assoc_info->bcn_len = bcn_tmpl_len;
			assoc_info->wpaie = ((int32)assoc_info->bcn +
				ROUNDUP(bcn_tmpl_len, sizeof(uint16)));
			assoc_info->auth_alg = scb->auth_alg;
			assoc_info->WPA_auth = param->akm_ie_included ?
				(uint8)scb->WPA_auth : WPA_AUTH_DISABLED;
			assoc_info->wsec =
				((scb->cap & DOT11_CAP_PRIVACY) && !WSEC_ENABLED(scb->wsec) &&
				WSEC_SES_OW_ENABLED(bsscfg->wsec) && !param->akm_ie_included) ?
				WEP_ENABLED : scb->wsec;
			assoc_info->ewc_cap = (N_ENAB(wlc->pub) &&
				(scb->flags & SCB_HTCAP)) ? TRUE : FALSE;
			assoc_info->ofdm = wlc_rateset_isofdm(scb->rateset.count,
				scb->rateset.rates);

			ptr = (uint8*)assoc_info + sizeof(assoc_info_t);
			if (body_len) {
				bcopy(body, ptr, body_len);
				ptr += ROUNDUP(body_len, sizeof(uint16));
			}
			if (rsp_len) {
				bcopy(rsp, ptr, rsp_len);
				ptr += ROUNDUP(rsp_len, sizeof(uint16));
			}
			wlc_bcn_prb_body(wlc, FC_BEACON, bsscfg,
				ptr, (int*)&assoc_info->bcn_len, FALSE);
			ptr += ROUNDUP(bcn_tmpl_len, sizeof(uint16));
			if (scb->wpaie_len) {
				bcopy(scb->wpaie, ptr, scb->wpaie_len);
			}

			ind_status = WLC_E_STATUS_SUCCESS;
		} else {
			ind_status = WLC_E_STATUS_FAIL;
			WL_ASSOC(("wl%d.%d %sassociated but no memory for event\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg),
				reassoc ? "re" : ""));
		}

		/* send WLC_E_REASSOC_IND_NDIS/WLC_E_ASSOC_IND_NDIS to NDIS */
		wlc_bss_mac_event(wlc, bsscfg, reassoc ? WLC_E_REASSOC_IND_NDIS:
			WLC_E_ASSOC_IND_NDIS, &hdr->sa, ind_status, param->status, 0,
			ind_status == WLC_E_STATUS_SUCCESS ? assoc_info : 0,
			ind_status == WLC_E_STATUS_SUCCESS ? total : 0);

		if (ind_status == WLC_E_STATUS_SUCCESS)
			MFREE(wlc->osh, assoc_info, total);
	} else /* WLEXTSTA_ENAB(wlc->pub) */
#endif /* EXT_STA */
#ifdef PLC_WET
	/* Add node so that we can route the frames appropriately */
	if (PLC_ENAB(wlc->pub))
		wlc_plc_node_add(wlc, NODE_TYPE_WIFI_ONLY, scb);
#endif /* PLC_WET */

	wl_smf_stats(ap, bsscfg, hdr, param);
}

#ifndef BCM_STA_SUPP_CHAN_ROM_COMPAT
int
wlc_sta_supp_chan(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, const struct ether_addr *ea,
	void *buf, int len)
{
	wlc_supp_channels_t* supp_chan = NULL;
	wlc_supp_chan_list_t supp_chan_list;
	struct scb *scb;
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *)(wlc->ap);

	ASSERT(ea != NULL);
	if (ea == NULL) {
		return (BCME_BADARG);
	}

	ASSERT(bsscfg != NULL);

	if ((scb = wlc_scbfind(wlc, bsscfg, ea)) == NULL) {
		return (BCME_BADADDR);
	}

	if (len < sizeof(supp_chan_list)) {
		return BCME_BUFTOOSHORT;
	}

	bzero(&supp_chan_list, sizeof(supp_chan_list));

	supp_chan_list.version = WL_STA_SUPP_CHAN_VER;

	supp_chan = SCB_SUPP_CHAN_CUBBY(appvt, scb);
	if (supp_chan) {
		supp_chan_list.count = supp_chan->count;
		bcopy(supp_chan->chanset, &(supp_chan_list.chanset),
			sizeof(supp_chan->chanset));
	}

	supp_chan_list.length =	sizeof(supp_chan_list);
	bcopy(&supp_chan_list, buf, sizeof(supp_chan_list));

	return BCME_OK;
}
#endif /* BCM_STA_SUPP_CHAN_ROM_COMPAT */

#ifdef RXCHAIN_PWRSAVE
/*
 * Reset the rxchain power save related counters and modes
 */
void
wlc_reset_rxchain_pwrsave_mode(wlc_ap_info_t *ap)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;
	wlc_info_t *wlc = appvt->wlc;

	WL_INFORM(("Resetting the rxchain power save counters\n"));
	/* Come out of the power save mode if we are in it */
	if (appvt->rxchain_pwrsave.pwrsave.in_power_save) {
		wlc_stf_rxchain_set(wlc, appvt->rxchain_pwrsave.rxchain, TRUE);
#ifdef WL11N
		/* need to restore rx_stbc HT capability after exit rxchain_pwrsave mode */
		wlc_stf_exit_rxchain_pwrsave(wlc, appvt->rxchain_pwrsave.ht_cap_rx_stbc);
#endif /* WL11N */
	}
	wlc_reset_pwrsave_mode(ap, PWRSAVE_RXCHAIN);
}

void
wlc_disable_rxchain_pwrsave(wlc_ap_info_t *ap)
{
	wlc_disable_pwrsave(ap, PWRSAVE_RXCHAIN);

	return;
}

#ifdef WL11N
/*
 * get rx_stbc HT capability, if in rxchain_pwrsave mode, return saved rx_stbc value,
 * because rx_stbc capability may be changed when enter rxchain_pwrsave mode
 */
uint8
wlc_rxchain_pwrsave_stbc_rx_get(wlc_info_t *wlc)
{
	uint8 ht_cap_rx_stbc = wlc_stf_stbc_rx_get(wlc);
	wlc_ap_info_t *ap = wlc->ap;
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;
	if ((appvt != NULL) && appvt->rxchain_pwrsave.pwrsave.in_power_save) {
		ht_cap_rx_stbc = appvt->rxchain_pwrsave.ht_cap_rx_stbc;
	}
	return ht_cap_rx_stbc;
}
#endif /* WL11N */
#endif /* RXCHAIN_PWRSAVE */

#ifdef RADIO_PWRSAVE
/*
 * Reset the radio power save related counters and modes
 */
static void
wlc_reset_radio_pwrsave_mode(wlc_ap_info_t *ap)
{
	uint8 dtim_period;
	uint16 beacon_period;
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;
	wlc_info_t *wlc = appvt->wlc;
	wlc_bsscfg_t *bsscfg = wlc->cfg;

	if (bsscfg->associated) {
		dtim_period = bsscfg->current_bss->dtim_period;
		beacon_period = bsscfg->current_bss->beacon_period;
	} else {
		dtim_period = wlc->default_bss->dtim_period;
		beacon_period = wlc->default_bss->beacon_period;
	}

	wl_del_timer(wlc->wl, appvt->radio_pwrsave.timer);
	wlc_reset_pwrsave_mode(ap, PWRSAVE_RADIO);
	appvt->radio_pwrsave.pwrsave_state = 0;
	appvt->radio_pwrsave.radio_disabled = FALSE;
	appvt->radio_pwrsave.cncl_bcn = FALSE;
	switch (appvt->radio_pwrsave.level) {
	case RADIO_PWRSAVE_LOW:
		appvt->radio_pwrsave.on_time = 2*beacon_period*dtim_period/3;
		appvt->radio_pwrsave.off_time = beacon_period*dtim_period/3;
		break;
	case RADIO_PWRSAVE_MEDIUM:
		appvt->radio_pwrsave.on_time = beacon_period*dtim_period/2;
		appvt->radio_pwrsave.off_time = beacon_period*dtim_period/2;
		break;
	case RADIO_PWRSAVE_HIGH:
		appvt->radio_pwrsave.on_time = beacon_period*dtim_period/3;
		appvt->radio_pwrsave.off_time = 2*beacon_period*dtim_period/3;
		break;
	default:
		ASSERT(0);
		break;
	}
}
#endif /* RADIO_PWRSAVE */

#if defined(RXCHAIN_PWRSAVE) || defined(RADIO_PWRSAVE)
/*
 * Reset power save related counters and modes
 */
void
wlc_reset_pwrsave_mode(wlc_ap_info_t *ap, int type)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;
	wlc_pwrsave_t *pwrsave = NULL;
	int enable = 0;

	switch (type) {
#ifdef RXCHAIN_PWRSAVE
		case PWRSAVE_RXCHAIN:
			enable = ap->rxchain_pwrsave_enable;
			pwrsave = &appvt->rxchain_pwrsave.pwrsave;
			if (pwrsave) {
				if (!enable)
					pwrsave->power_save_check &= ~PWRSAVE_ENB;
				else
					pwrsave->power_save_check |= PWRSAVE_ENB;
			}
			break;
#endif // endif
#ifdef RADIO_PWRSAVE
		case PWRSAVE_RADIO:
			enable = ap->radio_pwrsave_enable;
			pwrsave = &appvt->radio_pwrsave.pwrsave;
			appvt->radio_pwrsave.pwrsave_state = 0;
			if (pwrsave)
				pwrsave->power_save_check = enable;
			break;
#endif // endif
		default:
			break;
	}

	WL_INFORM(("Resetting the rxchain power save counters\n"));
	if (pwrsave) {
		pwrsave->quiet_time_counter = 0;
		pwrsave->in_power_save = FALSE;
	}
}

#ifdef RXCHAIN_PWRSAVE
static void
wlc_disable_pwrsave(wlc_ap_info_t *ap, int type)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;
	wlc_pwrsave_t *pwrsave = NULL;

	switch (type) {
#ifdef RXCHAIN_PWRSAVE
		case PWRSAVE_RXCHAIN:
			pwrsave = &appvt->rxchain_pwrsave.pwrsave;
			if (pwrsave)
				pwrsave->power_save_check &= ~PWRSAVE_ENB;
			break;
#endif // endif
#ifdef RADIO_PWRSAVE
		case PWRSAVE_RADIO:
			pwrsave = &appvt->radio_pwrsave.pwrsave;
			appvt->radio_pwrsave.pwrsave_state = 0;
			if (pwrsave)
				pwrsave->power_save_check = FALSE;
			break;
#endif // endif
		default:
			break;
	}

	WL_INFORM(("Disabling power save mode\n"));
	if (pwrsave)
		pwrsave->in_power_save = FALSE;
}

#ifdef WDS
/* Detect if WDS or DWDS is configured */
static bool
wlc_rxchain_wds_detection(wlc_info_t *wlc)
{
	struct scb_iter scbiter;
	struct scb *scb;
	wlc_bsscfg_t *bsscfg;
	int32 idx;

	FOREACH_BSS(wlc, idx, bsscfg) {
		FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
			if ((SCB_WDS(scb) != NULL) || (SCB_DWDS(scb)))
				return TRUE;
		}
	}
	return FALSE;
}
#endif /* WDS */
#endif /* RXCHAIN_PWRSAVE */

#endif /* RXCHAIN_PWRSAVE || RADIO_PWRSAVE */

bool
wlc_apsta_on_radar_channel(wlc_ap_info_t *ap)
{
	ASSERT(ap);
	return ((wlc_ap_info_pvt_t *)ap)->ap_sta_onradar;
}

static bool
wlc_ap_on_radarchan(wlc_ap_info_t *ap)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;
	wlc_info_t *wlc = appvt->wlc;
	int ap_idx;
	wlc_bsscfg_t *ap_cfg;
	bool ap_onradar = FALSE;

	if (AP_ACTIVE(wlc)) {
		FOREACH_UP_AP(wlc, ap_idx, ap_cfg) {
			wlc_bss_info_t *current_bss = ap_cfg->current_bss;

			if (wlc_radar_chanspec(wlc->cmi, current_bss->chanspec) == TRUE) {
				ap_onradar = TRUE;
				break;
			}
		}
	}

	return ap_onradar;
}

#ifdef STA
/* Check if any local AP/STA chanspec overlap with STA/local AP
 * radar chanspec.
 * NOTE: To take future possiblility of MCHAN in APSTA, have to use
 * FOREACH_UP_AP()
 *	FOREACH_AS_STA().
 */
void
wlc_ap_sta_onradar_upd(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_bss_info_t *current_bss = cfg->current_bss;
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) wlc->ap;
	bool dfs_slave_present = FALSE;

	/* Only when on radar channel we look for overlapping AP + STA.
	 * The moment we switch to non-radar we turn of dfs_slave_present.
	 */
	if (wlc_radar_chanspec(wlc->cmi, current_bss->chanspec) == TRUE)
	{
		int ap_idx;
		wlc_bsscfg_t *ap_cfg;

		FOREACH_UP_AP(wlc, ap_idx, ap_cfg) {
			int sta_idx;
			wlc_bsscfg_t *sta_cfg;
			wlc_bss_info_t *ap_bss = ap_cfg->current_bss;
			uint8 ap_ctlchan = wf_chspec_ctlchan(ap_bss->chanspec);

			FOREACH_AS_STA(wlc, sta_idx, sta_cfg) {
				wlc_bss_info_t *sta_bss = sta_cfg->current_bss;
				uint8 sta_ctlchan = wf_chspec_ctlchan(sta_bss->chanspec);

				if (ap_ctlchan == sta_ctlchan) {
					dfs_slave_present = TRUE;
					break;
				}
			}

			if (dfs_slave_present) break;
		}
	}

	appvt->ap_sta_onradar = dfs_slave_present;
}
#endif /* STA */

void
wlc_restart_ap(wlc_ap_info_t *ap)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;
	wlc_info_t *wlc = appvt->wlc;
	int i;
	wlc_bsscfg_t *bsscfg;
#ifdef RADAR
	uint bss_radar_flags = 0;
	bool radar_ap = FALSE;
#endif /* RADAR */

	WL_TRACE(("wl%d: %s:\n", wlc->pub->unit, __FUNCTION__));

#ifdef RADAR
	if (WL11H_AP_ENAB(wlc)) {
		FOREACH_AP(wlc, i, bsscfg) {
			bss_radar_flags |= (bsscfg)->flags & (WLC_BSSCFG_SRADAR_ENAB |
				WLC_BSSCFG_AP_NORADAR_CHAN |
				WLC_BSSCFG_USR_RADAR_CHAN_SELECT);

			/* Make sure these flags are are never combined together
			 * either within single ap bss or across all ap-bss.
			 */
			ASSERT(!(bss_radar_flags & (bss_radar_flags - 1)));
		}

		/* No random channel selection in any of bss_radar_flags modes */
		if (!bss_radar_flags &&
		    (wlc_channel_locale_flags_in_band(wlc->cmi, BAND_5G_INDEX) & WLC_DFS_TPC)) {
			wlc_dfs_sel_chspec(wlc->dfs, TRUE);
		}
	}
#endif /* RADAR */

	if (MBSS_ENAB(wlc->pub)) {
		ap->pre_tbtt_us = MBSS_PRE_TBTT_DEFAULT_us;
		ap->pre_tbtt_max_lat_us = MBSS_PRE_TBTT_MAX_LATENCY_us;
		appvt->pre_tbtt_min_thresh_us = MBSS_PRE_TBTT_MIN_THRESH_us;
	} else {
		ap->pre_tbtt_us = PRE_TBTT_DEFAULT_us;
	}

	/* Bring up any enabled AP configs which aren't up yet */
	FOREACH_AP(wlc, i, bsscfg) {
		if (bsscfg->enable) {
			uint wasup = bsscfg->up;
#ifdef RADAR
			wlc_bss_info_t *current_bss = bsscfg->current_bss;
#endif /* RADAR */

			WL_APSTA_UPDN(("wl%d: wlc_restart_ap -> wlc_bsscfg_up on bsscfg %d%s\n",
			               appvt->pub->unit, i, (bsscfg->up ? "(already up)" : "")));
			/* Clearing association state to let the beacon phyctl0
			 * and phyctl1 parameters be updated in shared memory.
			 * The phyctl0 and phyctl1 would be cleared from shared
			 * memory after the big hammer is executed.
			 * The wlc_bsscfg_up function below will update the
			 * associated state accordingly.
			 */
			bsscfg->associated = 0;
			if ((BCME_OK != wlc_bsscfg_up(wlc, bsscfg)) && wasup) {
				wlc_bsscfg_down(wlc, bsscfg);
			}
#ifdef RADAR
			if (bsscfg->up &&
				(wlc_radar_chanspec(wlc->cmi, current_bss->chanspec) == TRUE)) {
				radar_ap = TRUE;
			}
#endif /* RADAR */
#ifdef EXT_STA
			if (WLEXTSTA_ENAB(wlc->pub))
			{
				chanspec_t chanspec = wlc_get_home_chanspec(bsscfg);
				/* indicate AP starting with channel spec info */
				wlc_bss_mac_event(wlc, bsscfg, WLC_E_AP_STARTED, NULL,
					WLC_E_STATUS_SUCCESS, 0, 0, &chanspec, sizeof(chanspec));
			}
#endif /* #ifdef EXT_STA */
		}
	}

#ifdef RADAR
	if (WL11H_AP_ENAB(wlc) && AP_ACTIVE(wlc)) {
		/* Check If radar_detect explicitly disabled
		 * OR
		 * non of the AP's on radar chanspec
		 */
		bool dfs_on = ((bss_radar_flags
				& (WLC_BSSCFG_SRADAR_ENAB | WLC_BSSCFG_AP_NORADAR_CHAN)) ||
				!radar_ap) ? OFF:ON;

		wlc_set_dfs_cacstate(wlc->dfs, dfs_on);
	}
#endif /* RADAR */

#ifdef RXCHAIN_PWRSAVE
	wlc_reset_rxchain_pwrsave_mode(ap);
#endif // endif
#ifdef RADIO_PWRSAVE
	wlc_reset_radio_pwrsave_mode(ap);
#endif // endif
}

void
wlc_bss_up(wlc_ap_info_t *ap, wlc_bsscfg_t *bsscfg)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;
	wlc_info_t *wlc = appvt->wlc;
	wlc_bss_info_t *target_bss = bsscfg->target_bss;

	WL_TRACE(("wl%d: %s:\n", wlc->pub->unit, __FUNCTION__));

#ifdef WL11N
	/* Adjust target bss rateset according to target channel bandwidth */
	wlc_rateset_bw_mcs_filter(&target_bss->rateset,
		WL_BW_CAP_40MHZ(wlc->band->bw_cap)?CHSPEC_WLC_BW(target_bss->chanspec):0);
#endif /* WL11N */

	wlc_suspend_mac_and_wait(wlc);
	wlc_BSSinit(wlc, target_bss, bsscfg, WLC_BSS_START);
	wlc_enable_mac(wlc);

	/* update the AP association count */
	wlc_ap_up_upd(ap, bsscfg, TRUE);

#ifdef WLMCNX
	if (MCNX_ENAB(wlc->pub)) {
		wlc_mcnx_bss_upd(wlc->mcnx, bsscfg, TRUE);
	}
#endif // endif

	if (BSS_WME_ENAB(wlc, bsscfg))
		wlc_edcf_acp_apply(wlc, bsscfg, TRUE);

	wlc_led_event(wlc->ledh);

	if (WLCISNPHY(wlc->band) || WLCISHTPHY(wlc->band) || WLCISACPHY(wlc->band))
		wlc_full_phy_cal(wlc, bsscfg, PHY_PERICAL_UP_BSS);

	/* Indicate AP is now up */
	WL_APSTA_UPDN(("Reporting link up on config %d (AP enabled)\n",
		WLC_BSSCFG_IDX(bsscfg)));
	wlc_link(wlc, TRUE, &bsscfg->cur_etheraddr, bsscfg, 0);
}

static void
wlc_ap_up_upd(wlc_ap_info_t *ap, wlc_bsscfg_t *bsscfg, bool state)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;
	wlc_info_t *wlc = appvt->wlc;
	uint8 constraint;

	bsscfg->associated = state;

#ifdef WLMCHAN
	if (MCHAN_ENAB(wlc->pub)) {
		if (state == TRUE) {
			wlc_mchan_create_bss_chan_context(wlc, bsscfg,
			                                  bsscfg->current_bss->chanspec);
		}
		else {
			wlc_mchan_delete_bss_chan_context(wlc, bsscfg);
		}
	}
#endif /* WLMCHAN */

	wlc->aps_associated = (uint8)AP_BSS_UP_COUNT(wlc);

	wlc_ap_sta_onradar_upd(bsscfg);

	wlc_mac_bcn_promisc(wlc);

	if ((wlc->aps_associated == 0) || ((wlc->aps_associated == 1))) /* No need beyond 1 */
		wlc_bmac_enable_tbtt(wlc->hw, TBTT_AP_MASK, wlc->aps_associated ? TBTT_AP_MASK : 0);

	wlc->pub->associated = wlc->aps_associated > 0 || wlc->stas_associated > 0;

	/* Win7: Enable AP if it wasn't enabled */
	if (WLEXTSTA_ENAB(wlc->pub) && wlc->aps_associated && !AP_ENAB(wlc->pub))
		wlc->pub->_ap = 1;

	wlc->pub->associated = wlc->aps_associated > 0 || wlc->stas_associated > 0;
	/* Set the power limits for this locale after computing
	 * any 11h local tx power constraints.
	 */
	constraint = wlc_tpc_get_local_constraint_qdbm(wlc->tpc);

	wlc_channel_set_txpower_limit(wlc->cmi, constraint);
}

/* known reassociation magic packets */

struct lu_reassoc_pkt {
	struct ether_header eth;
	struct dot11_llc_snap_header snap;
	uint8 unknown_field[2];
	uint8 data[36];
};

static
const struct lu_reassoc_pkt lu_reassoc_template = {
	{ { 0x01, 0x60, 0x1d, 0x00, 0x01, 0x00 },
	{ 0 },
	HTON16(sizeof(struct lu_reassoc_pkt) - sizeof(struct ether_header)) },
	{ 0xaa, 0xaa, 0x03, { 0x00, 0x60, 0x1d }, HTON16(0x0001) },
	{ 0x00, 0x04 },
	"Lucent Technologies Station Announce"
};

struct csco_reassoc_pkt {
	struct ether_header eth;
	struct dot11_llc_snap_header snap;
	uint8 unknown_field[4];
	struct ether_addr ether_dhost, ether_shost, a1, a2, a3;
	uint8 pad[4];
};
/* WES - I think the pad[4] at the end of the struct above should be
 * dropped, it appears to just be the ethernet padding to 64
 * bytes. This would fix the length calculation below, (no more -4).
 * It matches with the 0x22 field in the 'unknown_field' which appears
 * to be the length of the encapsulated packet starting after the snap
 * header.
 */

static
const struct csco_reassoc_pkt csco_reassoc_template = {
	{ { 0x01, 0x40, 0x96, 0xff, 0xff, 0x00 },
	{ 0 },
	HTON16(sizeof(struct csco_reassoc_pkt) - sizeof(struct ether_header) - 4) },
	{ 0xaa, 0xaa, 0x03, { 0x00, 0x40, 0x96 }, HTON16(0x0000) },
	{ 0x00, 0x22, 0x02, 0x02 },
	{ { 0x01, 0x40, 0x96, 0xff, 0xff, 0x00 } },
	{ { 0 } },
	{ { 0 } },
	{ { 0 } },
	{ { 0 } },
	{ 0x00, 0x00, 0x00, 0x00 }
};

bool
wlc_roam_check(wlc_ap_info_t *ap, wlc_bsscfg_t *bsscfg, struct ether_header *eh, uint len)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;
	wlc_info_t *wlc = appvt->wlc;
#ifdef BCMDBG_ERR
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG_ERR */
	struct lu_reassoc_pkt *lu = (struct lu_reassoc_pkt *) eh;
	struct csco_reassoc_pkt *csco = (struct csco_reassoc_pkt *) eh;
	struct ether_addr *sta = NULL;
	struct scb *scb;

	/* check for Lucent station announce packet */
	if (!bcmp(eh->ether_dhost, (const char*)lu_reassoc_template.eth.ether_dhost,
	          ETHER_ADDR_LEN) &&
	    len >= sizeof(struct lu_reassoc_pkt) &&
	    !bcmp((const char*)&lu->snap, (const char*)&lu_reassoc_template.snap,
	              DOT11_LLC_SNAP_HDR_LEN))
		sta = (struct ether_addr *) lu->eth.ether_shost;

	/* check for Cisco station announce packet */
	else if (!bcmp(eh->ether_dhost, (const char*)csco_reassoc_template.eth.ether_dhost,
	               ETHER_ADDR_LEN) &&
	         len >= sizeof(struct csco_reassoc_pkt) &&
	         !bcmp((const char*)&csco->snap, (const char*)&csco_reassoc_template.snap,
	               DOT11_LLC_SNAP_HDR_LEN))
		sta = &csco->a1;

	/* not a magic packet */
	else
		return (FALSE);

	/* disassociate station */
	if ((scb = wlc_scbfind(wlc, bsscfg, sta)) && SCB_ASSOCIATED(scb)) {
		WL_ERROR(("wl%d: %s roamed\n", wlc->pub->unit, bcm_ether_ntoa(sta, eabuf)));
		if (APSTA_ENAB(wlc->pub) && (scb->flags & SCB_MYAP)) {
			WL_APSTA(("wl%d: Ignoring roam report from my AP.\n", wlc->pub->unit));
			return (FALSE);
		}
		/* FIXME: (WES) Why are we sending a dissassoc here?
		 * If it just for wpa_msg, fix by moving wpa to WLC_E_DISASSOC* case
		 */

		wlc_senddisassoc(wlc, bsscfg, scb, sta, &bsscfg->BSSID,
		                 &bsscfg->cur_etheraddr, DOT11_RC_NOT_AUTH);
		wlc_scb_resetstate(scb);
		wlc_scb_setstatebit(scb, AUTHENTICATED);

		wlc_bss_mac_event(wlc, bsscfg, WLC_E_DISASSOC_IND, sta, WLC_E_STATUS_SUCCESS,
			DOT11_RC_DISASSOC_LEAVING, 0, 0, 0);
		/* Resume sniffing of this STA frames if the STA has been
	     * configured to be monitored before association.
	     */
		if (STAMON_ENAB(wlc->pub) && STA_MONITORING(wlc, sta))
			wlc_stamon_sta_sniff_enab(wlc->stamon_info, sta, TRUE);
	}

	return (TRUE);
}

static void
wlc_assoc_notify(wlc_ap_info_t *ap, struct ether_addr *sta, struct wlc_if *wlcif)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;
	wlc_info_t *wlc = appvt->wlc;

	const uchar pkt80211f[] = {0x00, 0x01, 0xAF, 0x81, 0x01, 0x00};
	struct ether_header *eh;
	uint16 len = sizeof(pkt80211f);
	void *p;

	/* prepare 802.11f IAPP announce packet. This should look like a wl rx packet since it sent
	   along the same path. Some work should be done to evaluate the real need for
	   extra headroom (how much), but the alignement enforced by wlc->hwrxoff_pktget must be
	   preserved.
	*/
	if ((p = PKTGET(wlc->osh, sizeof(pkt80211f) + BCMEXTRAHDROOM + wlc->hwrxoff_pktget +
		ETHER_HDR_LEN, FALSE)) == NULL)
		goto err;
	ASSERT(ISALIGNED(PKTDATA(wlc->osh, p), sizeof(uint32)));
	PKTPULL(wlc->osh, p, BCMEXTRAHDROOM + wlc->hwrxoff_pktget);

	eh = (struct ether_header *) PKTDATA(wlc->osh, p);
	bcopy(&ether_bcast, eh->ether_dhost, ETHER_ADDR_LEN);
	bcopy(sta, eh->ether_shost, ETHER_ADDR_LEN);
	eh->ether_type = hton16(len);
	bcopy(pkt80211f, &eh[1], len);
	WL_PRPKT("802.11f assoc", PKTDATA(wlc->osh, p), PKTLEN(wlc->osh, p));

		wl_sendup(wlc->wl, wlcif->wlif, p, 1);

	return;

err:
	WL_ERROR(("wl%d: %s: pktget error\n", wlc->pub->unit, __FUNCTION__));
	WLCNTINCR(wlc->pub->_cnt->rxnobuf);
	WLCNTINCR(wlcif->_cnt.rxnobuf);
	return;
}

static void
wlc_reassoc_notify(wlc_ap_info_t *ap, struct ether_addr *sta, struct wlc_if *wlcif)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;
	wlc_info_t *wlc = appvt->wlc;
	void *p;
	osl_t *osh;
	struct lu_reassoc_pkt *lu;
	struct csco_reassoc_pkt *csco;
	int len;
	wlc_bsscfg_t *cfg;

	osh = wlc->osh;

	cfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(cfg != NULL);

	/* prepare Lucent station announce packet */
	len = sizeof(struct lu_reassoc_pkt);
	if ((p = PKTGET(osh, len + BCMEXTRAHDROOM + wlc->hwrxoff_pktget +
		ETHER_HDR_LEN, FALSE)) == NULL)
		goto err;
	ASSERT(ISALIGNED(PKTDATA(osh, p), sizeof(uint32)));
	PKTPULL(osh, p, BCMEXTRAHDROOM + wlc->hwrxoff_pktget);

	lu = (struct lu_reassoc_pkt*) PKTDATA(osh, p);
	bcopy((const char*)&lu_reassoc_template, (char*) lu, sizeof(struct lu_reassoc_pkt));
	bcopy((const char*)sta, (char*)&lu->eth.ether_shost, ETHER_ADDR_LEN);
	WL_PRPKT("lu", PKTDATA(osh, p), PKTLEN(osh, p));

		wl_sendup(wlc->wl, wlcif->wlif, p, 1);

	/* prepare Cisco station announce packets */
	len = sizeof(struct csco_reassoc_pkt);
	if ((p = PKTGET(osh, len + BCMEXTRAHDROOM + wlc->hwrxoff_pktget +
		ETHER_HDR_LEN, FALSE)) == NULL)
		goto err;
	ASSERT(ISALIGNED(PKTDATA(osh, p), sizeof(uint32)));
	PKTPULL(osh, p, BCMEXTRAHDROOM + wlc->hwrxoff_pktget);

	csco = (struct csco_reassoc_pkt *) PKTDATA(osh, p);
	bcopy((const char*)&csco_reassoc_template, (char*)csco, len);
	bcopy((char*)sta, (char*)&csco->eth.ether_shost, ETHER_ADDR_LEN);
	bcopy((char*)sta, (char*)&csco->ether_shost, ETHER_ADDR_LEN);
	bcopy((char*)sta, (char*)&csco->a1, ETHER_ADDR_LEN);
	bcopy((char*)&cfg->BSSID, (char*)&csco->a2, ETHER_ADDR_LEN);
	bcopy((char*)&cfg->BSSID, (char*)&csco->a3, ETHER_ADDR_LEN);
	WL_PRPKT("csco1", PKTDATA(osh, p), PKTLEN(osh, p));

		wl_sendup(wlc->wl, wlcif->wlif, p, 1);

	len = sizeof(struct csco_reassoc_pkt);
	if ((p = PKTGET(osh, len + BCMEXTRAHDROOM + wlc->hwrxoff_pktget +
		ETHER_HDR_LEN, FALSE)) == NULL)
		goto err;
	ASSERT(ISALIGNED(PKTDATA(osh, p), sizeof(uint32)));
	PKTPULL(osh, p, BCMEXTRAHDROOM + wlc->hwrxoff_pktget);

	csco = (struct csco_reassoc_pkt *) PKTDATA(osh, p);
	bcopy((const char*)&csco_reassoc_template, (char*)csco, len);
	bcopy((char*)&cfg->BSSID, (char*)&csco->eth.ether_shost, ETHER_ADDR_LEN);
	bcopy((char*)&cfg->BSSID, (char*)&csco->ether_shost, ETHER_ADDR_LEN);
	bcopy((char*)sta, (char*)&csco->a1, ETHER_ADDR_LEN);
	bcopy((char*)&cfg->BSSID, (char*)&csco->a2, ETHER_ADDR_LEN);
	bcopy((char*)&cfg->BSSID, (char*)&csco->a3, ETHER_ADDR_LEN);
	WL_PRPKT("csco2", PKTDATA(osh, p), PKTLEN(osh, p));

		wl_sendup(wlc->wl, wlcif->wlif, p, 1);

	return;

err:
	WL_ERROR(("wl%d: %s: pktget error\n", wlc->pub->unit, __FUNCTION__));
	WLCNTINCR(wlc->pub->_cnt->rxnobuf);
	WLCNTINCR(wlcif->_cnt.rxnobuf);
	return;
}

uint8
wlc_lowest_basicrate_get(wlc_bsscfg_t *cfg)
{
	uint8 i, rate = 0;
	wlc_bss_info_t *current_bss = cfg->current_bss;

	for (i = 0; i < current_bss->rateset.count; i++) {
		if (current_bss->rateset.rates[i] & WLC_RATE_FLAG) {
			rate = current_bss->rateset.rates[i] & RATE_MASK;
			break;
		}
	}

	return rate;
}

static void
wlc_ap_probe_complete(wlc_info_t *wlc, void *pkt, uint txs)
{
	struct scb *scb;
	struct scb *newscb;
	struct scb_iter scbiter;
	bool match = FALSE;

	if ((scb = WLPKTTAGSCBGET(pkt)) == NULL)
		return;

	/* Before using the scb, need to check if this is stale first */
	FOREACHSCB(wlc->scbstate, &scbiter, newscb) {
		if (newscb == scb) {
			match = TRUE;
			break;
		}
	}
	if (!match)
		return;

#ifdef WDS
	if (SCB_LEGACY_WDS(scb) && WLWDS_CAP(wlc))
		wlc_ap_wds_probe_complete(wlc, txs, scb);
	else
#endif // endif
	wlc_ap_sta_probe_complete(wlc, txs, scb, pkt);
}

#ifndef DONGLEBUILD
static int
wlc_ap_sendnulldata_cb(wlc_info_t *wlc, wlc_bsscfg_t *cfg, void *pkt, void *data)
{
	/* register packet callback */
	WLF2_PCB1_REG(pkt, WLF2_PCB1_STA_PRB);
	return BCME_OK;
}
#endif /* !DONGLEBUILD */

static void
wlc_ap_sta_probe(wlc_ap_info_t *ap, struct scb *scb)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;
	wlc_info_t *wlc = appvt->wlc;
#ifdef DONGLEBUILD
	void *pkt;
#endif /* DONGLEBUILD */
	uint8 rate_override;
#ifdef PSPRETEND
	bool  ps_pretend = SCB_PS_PRETEND(scb);
#else
	const bool ps_pretend = FALSE;
#endif // endif

	/* If a probe is still pending, don't send another one */
	if (scb->flags & SCB_PENDING_PROBE)
		return;

	/* use the lowest basic rate */
	rate_override = wlc_lowest_basicrate_get(scb->bsscfg);

	ASSERT(VALID_RATE(wlc, rate_override));
#ifdef DONGLEBUILD
	pkt = wlc_sendnulldata(wlc, scb->bsscfg, &scb->ea, rate_override,
	                       ps_pretend ? WLF_PSDONTQ : 0,
	                       ps_pretend ? PRIO_8021D_VO : PRIO_8021D_BE);

	if (pkt == NULL) {
#else
	if (!wlc_sendnulldata_ex(wlc, scb->bsscfg, &scb->ea, rate_override,
		ps_pretend ? WLF_PSDONTQ : 0,
		ps_pretend ? PRIO_8021D_VO : PRIO_8021D_BE,
		wlc_ap_sendnulldata_cb, NULL))
	{
#endif /* DONGLEBUILD */
		WL_ERROR(("wl%d: wlc_ap_sta_probe: wlc_sendnulldata failed\n",
		          wlc->pub->unit));
		return;
	}

#ifdef DONGLEBUILD
	/* register packet callback */
	WLF2_PCB1_REG(pkt, WLF2_PCB1_STA_PRB);
#endif /* DONGLEBUILD */
	scb->flags |= SCB_PENDING_PROBE;
}

static void
wlc_ap_sta_probe_complete(wlc_info_t *wlc, uint txstatus, struct scb *scb, void *pkt)
{
#if defined(BCMDBG) || defined(WLMSG_INFORM)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif // endif

	ASSERT(scb != NULL);

	scb->flags &= ~SCB_PENDING_PROBE;

#ifdef PSPRETEND
	if (SCB_PS_PRETEND_PROBING(scb)) {
		if ((txstatus & TX_STATUS_MASK) == TX_STATUS_ACK_RCV) {
			/* probe response OK - exit PS Pretend state */
			WL_PS(("wl%d.%d: received ACK to ps pretend probe "MACF" (count %d)\n",
			        wlc->pub->unit, WLC_BSSCFG_IDX(SCB_BSSCFG(scb)),
			        ETHER_TO_MACF(scb->ea), scb->grace_attempts));

			/* Assert check that the fifo was cleared before exiting ps mode */
			if (SCB_PS_PRETEND_BLOCKED(scb)) {
				WL_ERROR(("wl%d.%d: %s: SCB_PS_PRETEND_BLOCKED, "
				          "expected to see PMQ PPS entry\n", wlc->pub->unit,
				          WLC_BSSCFG_IDX(SCB_BSSCFG(scb)), __FUNCTION__));
			}
			if (!(wlc->block_datafifo & DATA_BLOCK_PS)) {
				wlc_apps_scb_ps_off(wlc, scb, FALSE);
			}
			else {
				WL_ERROR(("wl%d.%d: %s: annother PS flush in progress, "
				          "unable to resume\n", wlc->pub->unit,
				          WLC_BSSCFG_IDX(SCB_BSSCFG(scb)), __FUNCTION__));
			}
		}
		else {
			++scb->grace_attempts;
			WL_PS(("wl%d.%d: no response to ps pretend probe "MACF" (count %d)\n",
			        wlc->pub->unit, WLC_BSSCFG_IDX(SCB_BSSCFG(scb)),
			        ETHER_TO_MACF(scb->ea), scb->grace_attempts));
		}
		/* we re-probe using ps pretend probe timer if not stalled,
		* so return from here
		*/
		return;
	}
#endif /* PSPRETEND */
	/* ack indicates the sta should not be removed or we might have missed the ACK but if there
	 * was some activity after sending the probe then it indicates there is life out there in
	 * scb.
	 */
	if (((txstatus & TX_STATUS_MASK) != TX_STATUS_NO_ACK) ||
	    (wlc->pub->now - scb->used <= wlc->scb_activity_time) ||
	    wlc->scb_activity_time == 0) {
		scb->grace_attempts = 0;
		/* update the primary PSTA also */
		if (scb->psta_prim)
			scb->psta_prim->used = wlc->pub->now;
		return;
	}

	/* If still more grace_attempts are left, then probe the STA again */
	if (++scb->grace_attempts < wlc->scb_max_probe) {
		wlc->reprobe_scb = TRUE;
		return;
	}

	WL_INFORM(("wl%d: wlc_ap_sta_probe_complete: no ACK from %s for Null Data\n",
		wlc->pub->unit, bcm_ether_ntoa(&scb->ea, eabuf)));

	if (SCB_AUTHENTICATED(scb)) {
		wlc_deauth_complete(wlc, SCB_BSSCFG(scb), WLC_E_STATUS_SUCCESS, &scb->ea,
			DOT11_RC_INACTIVITY, 0);
	}

	/* free the scb */
	wlc_scbfree(wlc, scb);
	WLPKTTAGSCBCLR(pkt);
}

#ifdef BCMDBG
static int
wlc_dump_ap(wlc_ap_info_t *ap, struct bcmstrbuf *b)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;
	wlc_info_t *wlc = appvt->wlc;

	bcm_bprintf(b, "\n");

	bcm_bprintf(b, " shortslot_restrict %d scb_timeout %d\n",
		ap->shortslot_restrict, wlc->scb_timeout);

	bcm_bprintf(b, "tbtt %d pre-tbtt-us %u. max latency %u. "
		"min threshold %u. block datafifo %d "
		"\n",
		WLCNTVAL(wlc->pub->_cnt->tbtt),
		ap->pre_tbtt_us, ap->pre_tbtt_max_lat_us,
		appvt->pre_tbtt_min_thresh_us, wlc->block_datafifo);

	return 0;
}

#ifdef RXCHAIN_PWRSAVE
static int
wlc_dump_rxchain_pwrsave(wlc_ap_info_t *ap, struct bcmstrbuf *b)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;

	wlc_dump_pwrsave(&appvt->rxchain_pwrsave.pwrsave, b);

	return 0;
}
#endif /* RXCHAIN_PWRSAVE */

#ifdef RADIO_PWRSAVE
static int
wlc_dump_radio_pwrsave(wlc_ap_info_t *ap, struct bcmstrbuf *b)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;

	wlc_dump_pwrsave(&appvt->radio_pwrsave.pwrsave, b);

	return 0;
}
#endif /* RADIO_PWRSAVE */

#if defined(RXCHAIN_PWRSAVE) || defined(RADIO_PWRSAVE)
static void
wlc_dump_pwrsave(wlc_pwrsave_t *pwrsave, struct bcmstrbuf *b)
{
	bcm_bprintf(b, "\n");

	bcm_bprintf(b, " in_power_save %d\n",
		pwrsave->in_power_save);

	bcm_bprintf(b, " no: of times in power save mode %d\n",
		pwrsave->in_power_save_counter);

	bcm_bprintf(b, " power save time (in secs) %d\n",
		pwrsave->in_power_save_secs);

	return;
}
#endif /* RXCHAIN_PWRSAVE or RADIO_PWRSAVE */
#endif /* BCMDBG */

static int
wlc_ap_watchdog(void *arg)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) arg;
	wlc_ap_info_t *ap = &appvt->appub;
	wlc_info_t *wlc = appvt->wlc;
#ifdef RXCHAIN_PWRSAVE
	bool done = 0;
#endif // endif
	int idx;
	wlc_bsscfg_t *cfg;

	/* part 1 */
	if (AP_ENAB(wlc->pub)) {
		struct scb *scb;
		struct scb_iter scbiter;

		/* before checking for stuck tx beacons make sure atleast one
		 * ap bss is up phy is not muted(for whatever reason) and beaconing.
		 */
		if (!wlc_phy_ismuted(wlc->band->pi) && !MBSS_ENAB4(wlc->pub) &&
		    (ap->txbcn_timeout > 0) && (AP_BSS_UP_COUNT(wlc) > 0)) {
			uint16 txbcn_snapshot;

			/* see if any beacons are transmitted since last
			 * watchdog timeout.
			 */
			txbcn_snapshot = wlc_read_shm(wlc, M_UCODE_MACSTAT +
			                              OFFSETOF(macstat_t, txbcnfrm));

			/* if no beacons are transmitted for txbcn timeout period
			 * then some thing has gone bad, do the big-hammer.
			 */
			if (txbcn_snapshot == appvt->txbcn_snapshot) {
				if (++appvt->txbcn_inactivity >= ap->txbcn_timeout) {
					WL_ERROR(("wl%d: bcn inactivity detected\n",
					          wlc->pub->unit));
					WL_ERROR(("wl%d: txbcnfrm %d prev txbcnfrm %d "
					          "txbcn inactivity %d timeout %d\n",
					          wlc->pub->unit, txbcn_snapshot,
					          appvt->txbcn_snapshot,
					          appvt->txbcn_inactivity,
					          ap->txbcn_timeout));
					appvt->txbcn_inactivity = 0;
					appvt->txbcn_snapshot = 0;
					wlc_fatal_error(wlc);
					return BCME_ERROR;
				}
			} else
				appvt->txbcn_inactivity = 0;

			/* save the txbcn counter */
			appvt->txbcn_snapshot = txbcn_snapshot;
		}

		/* deauth rate limiting - enable sending one deauth every second */
		FOREACHSCB(wlc->scbstate, &scbiter, scb) {
#ifdef RXCHAIN_PWRSAVE
			if ((ap->rxchain_pwrsave_enable == RXCHAIN_PWRSAVE_ENAB_BRCM_NONHT) &&
			    !done) {
				if (SCB_ASSOCIATED(scb) && !(scb->flags & SCB_BRCM) &&
				    (scb->flags & SCB_HTCAP)) {
					if (appvt->rxchain_pwrsave.pwrsave.in_power_save) {
						wlc_stf_rxchain_set(wlc,
							appvt->rxchain_pwrsave.rxchain, TRUE);
#ifdef WL11N
						/* need to restore rx_stbc HT capability
						 * after exit rxchain_pwrsave mode
						 */
						wlc_stf_exit_rxchain_pwrsave(wlc,
							appvt->rxchain_pwrsave.ht_cap_rx_stbc);
#endif /* WL11N */
						appvt->rxchain_pwrsave.pwrsave.in_power_save =
							FALSE;
					}
					appvt->rxchain_pwrsave.pwrsave.power_save_check &=
						~PWRSAVE_ENB;
					done = 1;
				}
			}
#endif /* RXCHAIN_PWRSAVE */
			/* clear scb deauth. sent flag so new deauth allows to be sent */
			scb->flags &= ~SCB_DEAUTH;
		}
#ifdef RXCHAIN_PWRSAVE
		if (!done) {
			if (!ap->rxchain_pwrsave_enable)
				appvt->rxchain_pwrsave.pwrsave.power_save_check &= ~PWRSAVE_ENB;
			else
				appvt->rxchain_pwrsave.pwrsave.power_save_check |= PWRSAVE_ENB;
		}
#endif // endif
	}

#if defined(WLPKTDLYSTAT) && defined(WLPKTDLYSTAT_IND)
	if (wlc->pub->txdelay_params.period > 0)
		wlc_delay_stats_watchdog(wlc);
#endif /* defined(WLPKTDLYSTAT) && defined(WLPKTDLYSTAT_IND) */

	/* part 2 */
	FOREACH_UP_AP(wlc, idx, cfg) {
#ifdef WL11N
		/* OBSS COEX check for possible upgrade
		 * only do it when it is currently 20MHz
		 */
		if (COEX_ACTIVE(cfg) && CHSPEC_IS20(cfg->current_bss->chanspec)) {
			/* if any 40 intolerant still associated, don't upgrade */
			if (wlc_prot_n_ht40int(wlc->prot_n, cfg))
				wlc_ht_coex_update_fid_time(cfg);
			else {
				wlc_obss_info_t *obss = cfg->obss;
				uint32 tr_delay = obss->params.bss_widthscan_interval *
					obss->params.chanwidth_transition_dly;
				if ((wlc->pub->now - obss->fid_time) > tr_delay) {
					/* The AP udates the fid_time everytime a new trigger
					 * happens. If we get here, we are good to upgrade.
					 */
					wlc_ht_coex_update_permit(cfg, TRUE);
					wlc_ht_coex_switch_bw(cfg, FALSE, COEX_UPGRADE_TIMER);
				}
			}
		}
#endif /* WL11N */
	}

	/* part 3 */
	if (AP_ENAB(wlc->pub)) {

		/* process age-outs only when not in scan progress */
		if (!SCAN_IN_PROGRESS(wlc->scan)) {
			/* age out ps queue packets */
			wlc_apps_psq_ageing(wlc);
			if (wlc->pub->now < wlc->pub->pending_now)
				wlc->pub->pending_now = 0;

			/* age out stas */
			if ((wlc->scb_timeout &&
			(((wlc->pub->now - wlc->pub->pending_now)
			  % wlc->scb_timeout) == 0)) ||
#ifdef WLP2P
			    (wlc->p2p && wlc_p2p_go_scb_timeout(wlc->p2p)) ||
#endif /* WLP2P */
			    wlc->reprobe_scb) {
				wlc_ap_stas_timeout(ap);
				wlc->reprobe_scb = FALSE;
			}
			/* age out secure STAs not in authorized state */
			wlc_ap_wsec_stas_timeout(ap);
		} else {
			wlc->pub->pending_now++;
		}

		if (WLC_CHANIM_ENAB(wlc) && WLC_CHANIM_MODE_ACT(wlc->chanim_info))
			wlc_lq_chanim_upd_act(wlc);

#ifdef APCS
		/* periodic auto channel selection timer */
		if (!WL11H_ENAB(wlc) &&
		    /* this channel selection disabled for 11h-enabled AP */
		    AP_ENAB(wlc->pub) &&
		    appvt->cs_scan_timer &&
		    !(wlc->block_datafifo & DATA_BLOCK_QUIET) &&
		    (((wlc->pub->now % appvt->cs_scan_timer) == 0) ||
		     wlc->cs_scan_ini || WLC_CHANIM_ACT(wlc->chanim_info))) {
			wlc_cs_scan_timer(wlc->cfg);
		}
#endif /* APCS */

#ifdef RXCHAIN_PWRSAVE
		/* Do the wl power save checks */
		if (appvt->rxchain_pwrsave.pwrsave.power_save_check & PWRSAVE_ENB)
			wlc_pwrsave_mode_check(ap, PWRSAVE_RXCHAIN);

		if (appvt->rxchain_pwrsave.pwrsave.in_power_save)
			appvt->rxchain_pwrsave.pwrsave.in_power_save_secs++;
#endif // endif

#ifdef RADIO_PWRSAVE
		if (appvt->radio_pwrsave.pwrsave.power_save_check)
			wlc_pwrsave_mode_check(ap, PWRSAVE_RADIO);

		if (appvt->radio_pwrsave.pwrsave.in_power_save)
			appvt->radio_pwrsave.pwrsave.in_power_save_secs++;
#endif // endif
	}

	/* Part 4 */
	FOREACH_UP_AP(wlc, idx, cfg) {
		/* update brcm_ie and our beacon */
		if (wlc_bss_update_brcm_ie(wlc, cfg)) {
			WL_APSTA_BCN(("wl%d.%d: wlc_watchdog() calls wlc_update_beacon()\n",
			              wlc->pub->unit, WLC_BSSCFG_IDX(cfg)));
			wlc_bss_update_beacon(wlc, cfg);
			wlc_bss_update_probe_resp(wlc, cfg, TRUE);
		}
	}

	return BCME_OK;
}

#if defined(MBSS)
/* Assumes SW beaconing active */
#define BSS_BEACONING(cfg) ((cfg) && BSSCFG_AP(cfg) && (cfg)->up)

/* MBSS4 MI_TBTT and MI_DTIM_TBTT handler */
int
wlc_ap_mbss4_tbtt(wlc_info_t *wlc, uint32 macintstatus)
{
	int i, idx;
	wlc_bsscfg_t *cfg;
	uint16 beacon_count = 0;
	wlc_pkt_t pkt;
	bool dtim;
	uint32 tbtt_delay;
#ifdef RADIO_PWRSAVE
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) wlc->ap;
#endif /* RADIO_PWRSAVE */

	dtim = ((macintstatus & MI_DTIM_TBTT) != 0);
	/* Update our dtim count and determine which BSS config will beacon first */
	if (dtim) {
		wlc->cur_dtim_count = 0x00;
		/* disabled beacon rotate when beacon_bssidx < 0  */
		if (wlc->beacon_bssidx >= 0) {
			/* Bump the "starting" bss index up for bss rotation */
			for (i = 0; i < WLC_MAXBSSCFG; i++) {
				if (++wlc->beacon_bssidx >= WLC_MAXBSSCFG) {
					wlc->beacon_bssidx = 0;
				}
				if (BSS_BEACONING(wlc->bsscfg[wlc->beacon_bssidx])) {
					/* Found the next beaconing BSS index; break */
					break;
				}
			}
		}
	}
	else  {
		if (wlc->cur_dtim_count)
			wlc->cur_dtim_count--;
		else
			wlc->cur_dtim_count = wlc->default_bss->dtim_period - 1;
	}

	/* If we've taken too long to get beacons ready, don't bother queuing them */
	tbtt_delay = (R_REG(wlc->osh, &wlc->regs->tsf_timerlow) -
	         wlc->last_tbtt_us);
	if (tbtt_delay > wlc->ap->pre_tbtt_max_lat_us) {
		WLCNTINCR(wlc->pub->_cnt->late_tbtt_dpc);
		WL_MBSS(("wl%d: ERROR: TBTT latency: %u; skipping bcn\n",
			wlc->pub->unit, tbtt_delay));
		return 0;
	}

#ifdef RADIO_PWRSAVE
	/* just out of pwr save with a need to skip bcn in level 1/2. */
	if (!dtim && appvt->radio_pwrsave.cncl_bcn) {
		WL_INFORM(("wl%d: radio pwrsave skipping bcn.\n", wlc->pub->unit));
		return 0;
	}
#endif /* RADIO_PWRSAVE */

	if (SCAN_IN_PROGRESS(wlc->scan)) {
		WL_MBSS(("wl%d: WARNING: MBSS Not beaconing due to scan in progress.\n",
		         wlc->pub->unit));
		return 0;
	}

	for (i = 0; i < WLC_MAXBSSCFG; i++) {
		idx = i;
		if (wlc->beacon_bssidx >= 0)
			idx = (i + wlc->beacon_bssidx) % WLC_MAXBSSCFG;
		cfg = wlc->bsscfg[idx];
		if (!BSS_BEACONING(cfg)) {
			continue; /* Skip cfgs not present or not AP or not up */
		}
		ASSERT(cfg->bcn_template->latest_idx >= 0);
		ASSERT(cfg->bcn_template->latest_idx < WLC_SPT_COUNT_MAX);
		if (cfg->bcn_template->in_use_bitmap != 0) {
			WLCNTINCR(wlc->pub->_cnt->bcn_template_not_ready);
#if defined(BCMDBG_MBSS_PROFILE)
			if (cfg->bcn_tx_done) {
				WLCNTINCR(wlc->pub->_cnt->bcn_template_not_ready_done);
			}
#endif // endif
			continue;
		}

		pkt = SPT_LATEST_PKT(cfg->bcn_template);
		ASSERT(pkt != NULL);

		/* Update DTIM count */
		BCN_TIM_DTIM_COUNT_SET(cfg->bcn_template->tim_ie, wlc->cur_dtim_count);

		/*
		 * Update BCMC flag in the beacon.
		 * At this point, the driver has not yet written the last FID SHM locations;
		 * so either bcmc_fid or bcmc_fid_shm may be indicate pkts in the BC/MC fifo
		 */
		if (BCMC_PKTS_QUEUED(cfg)) {
			BCN_TIM_BCMC_FLAG_SET(cfg->bcn_template->tim_ie);
		} else {
			BCN_TIM_BCMC_FLAG_RESET(cfg->bcn_template->tim_ie);
		}

		if (dma_txfast(WLC_HW_DI(wlc, TX_ATIM_FIFO), pkt, TRUE) < 0) {
			WLCNTINCR(cfg->cnt->bcn_tx_failed);
		} else {
			++beacon_count;
			cfg->bcn_template->in_use_bitmap |= (1 << cfg->bcn_template->latest_idx);
#if defined(BCMDBG_MBSS_PROFILE)
			cfg->bcn_tx_done = FALSE;
#endif /* BCMDBG_MBSS_PROFILE */
#if defined(WLC_SPT_DEBUG)
			cfg->bcn_template->tx_count++;
#endif /* WLC_SPT_DEBUG */
		}
	}
	wlc_write_shm(wlc, SHM_MBSS_BCN_COUNT, beacon_count);

	if (dtim) {
		wlc_apps_update_bss_bcmc_fid(wlc);
	}

	return 0;
}

/* MBSS16 MI_TBTT and MI_DTIM_TBTT handler */
int
wlc_ap_mbss16_tbtt(wlc_info_t *wlc, uint32 macintstatus)
{
	bool dtim;
	int cfgidx;
	int ucidx;
	wlc_bsscfg_t *cfg = NULL;
	uint16 beacon_count = 0;
	uint16 dtim_map = 0;
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) wlc->ap;

	uint32 time_left_for_next_tbtt_us; /* time left for next TBTT */
	uint32 tsf_timerlow_us; /* current tfs */
	uint32 beacon_period_us; /* beacon period in us */
	bool ucode_bcn_suspend = FALSE; /* Indicates whether driver have to suspend ucode
									 * for beaconing.
									 */
	bool ucode_bcn_suspended = FALSE; /* Indicates whether ucode is in suspended
									   * state for beaconing.
									   */

	dtim = ((macintstatus & MI_DTIM_TBTT) != 0);

#ifdef RADIO_PWRSAVE
	if (!dtim && appvt->radio_pwrsave.cncl_bcn) {
		wlc_write_shm(wlc, SHM_MBSS_BCN_COUNT, 0);
		WL_INFORM(("wl%d: radio pwrsave skipping bcn.\n", wlc->pub->unit));
		return 0;
	}
#endif /* RADIO_PWRSAVE */

	/* Calculating the time left for next TBTT */
	beacon_period_us = wlc->default_bss->beacon_period * DOT11_TU_TO_US;
	tsf_timerlow_us = R_REG(wlc->osh, &wlc->regs->tsf_timerlow);
	time_left_for_next_tbtt_us = beacon_period_us -
		(tsf_timerlow_us % beacon_period_us);

	if (time_left_for_next_tbtt_us <= appvt->pre_tbtt_min_thresh_us)
		ucode_bcn_suspend = TRUE;

	/* Traverse the bsscfg's
	 * create a count of "active" bss's
	 *
	 * if we're at a DTIM:
	 * create a DTIM map,  push "last" bc/mc fid's to shm
	 *
	 * if a beacon has been modified push to shm
	 */
	for (cfgidx = 0; cfgidx < WLC_MAXBSSCFG; cfgidx++) {
		cfg = wlc->bsscfg[cfgidx];
		if (!BSS_BEACONING(cfg))
			continue;

		ASSERT(cfg->bcn_template->latest_idx >= 0);
		ASSERT(cfg->bcn_template->latest_idx < WLC_SPT_COUNT_MAX);

		++beacon_count;

		ucidx = WLC_BSSCFG_UCIDX(cfg);
		ASSERT(ucidx != WLC_BSSCFG_IDX_INVALID);
		/* Update BCMC flag in the beacon. */
		if (dtim && (cfg->bcmc_fid != INVALIDFID)) {
			uint fid_addr;

			dtim_map |= NBITVAL(ucidx);
			fid_addr = SHM_MBSS_BC_FID_ADDR16(ucidx);
			if (ucode_bcn_suspend && !ucode_bcn_suspended) {
				ucode_bcn_suspended = TRUE;
				/* Suspending ucode from beaconing for next TBTT
				* due to we are updating bcmc_fid in shared memory
				* very close to next TBTT
				*/
				wlc_write_shm(wlc, SHM_MBSS_BCN_COUNT, 0xFFFF);
			}
			wlc_write_shm((wlc), fid_addr, cfg->bcmc_fid);
			BCMC_FID_SHM_COMMIT(cfg);
		}

		if ((cfg->bcn_template->bcn_modified == TRUE) &&
			ucode_bcn_suspend && !ucode_bcn_suspended) {
			ucode_bcn_suspended = TRUE;
			/* Suspending ucode from beaconing for next TBTT
			* due to the beacon template is going to be updated
			* in shared memory very close to next TBTT
			*/
			wlc_write_shm(wlc, SHM_MBSS_BCN_COUNT, 0xFFFF);
		}

		/* Update the HW beacon template */
		wlc_ap_mbss16_write_beacon(wlc, cfg);

	} /* cfgidx loop */

	wlc_write_shm(wlc, SHM_MBSS_BCN_COUNT, beacon_count);
	wlc_write_shm(wlc, SHM_MBSS_BSSID_NUM, beacon_count);

	if (dtim)
		wlc_write_shm(wlc, SHM_MBSS_BC_BITMAP, dtim_map);

	return 0;
}

/* Write the BSSCFG's beacon template into HW */
static void
wlc_ap_mbss16_write_beacon(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	wlc_pkt_t pkt;
	uint32 ucidx;
	int start;
	uint16 len;
	uint8 * pt;
	d11actxh_t *txh = NULL;
	int ac_hdr = 0;
	uint8 phy_hdr[D11AC_PHY_HDR_LEN];
	int  bcn_tmpl_len;

	bcn_tmpl_len = wlc->pub->bcn_tmpl_len;

	bzero(phy_hdr, sizeof(phy_hdr));

	ucidx = WLC_BSSCFG_UCIDX(cfg);
	ASSERT(ucidx != (uint32)WLC_BSSCFG_IDX_INVALID);

	ASSERT(cfg->bcn_template->latest_idx >= 0);
	ASSERT(cfg->bcn_template->latest_idx < WLC_SPT_COUNT_MAX);

	pkt = SPT_LATEST_PKT(cfg->bcn_template);
	ASSERT(pkt != NULL);

	/* beacon */
	if (cfg->bcn_template->bcn_modified == TRUE) {

		WL_MBSS(("wl%d: %s: bcn_modified on bsscfg %d\n",
		         wlc->pub->unit, __FUNCTION__, WLC_BSSCFG_IDX(cfg)));

		start = wlc->shm_bcn0_tpl_base + (ucidx * bcn_tmpl_len);
		if (D11REV_GE(wlc->pub->corerev, 40)) {
			uint8 offset = wlc_template_plcp_offset_ext(wlc, wlc->bcn_rspec);

			/* Get pointer TX header and build the phy header */
			pt = (uint8 *) PKTDATA(wlc->osh, pkt);
			len = (uint16) PKTLEN(wlc->osh, pkt);
#ifdef WLTOEHW
			{
				uint16 tso_hdr_size;
				d11ac_tso_t *tso_hdr;

				tso_hdr = (d11ac_tso_t *) pt;

				/* Skip overhead */
				tso_hdr_size = (uint16) (wlc->toe_bypass ? 0 :
					wlc_tso_hdr_length(tso_hdr));
				pt += tso_hdr_size;
				len -= tso_hdr_size;
			}
#endif /* WLTOEHW */
			txh = (d11actxh_t *) pt;

			bcopy(txh->RateInfo[0].plcp,
			      &phy_hdr[offset], D11_PHY_HDR_LEN);

			/* Now get the MAC frame */
			pt += D11AC_TXH_LEN;
			len -= D11AC_TXH_LEN;
			ac_hdr = 1;
		}
		else {
			pt = ((uint8 *)(PKTDATA(wlc->osh, pkt)) + D11_TXH_LEN);
			len = PKTLEN(wlc->osh, pkt) - D11_TXH_LEN;
		}

		ASSERT(len <= bcn_tmpl_len);

		if (ac_hdr) {
			/* Write the phy header (PLCP) first */
			wlc_write_template_ram(wlc, start, D11AC_PHY_HDR_LEN, phy_hdr);

			/* Now, write the remaining beacon frame */
			wlc_write_template_ram(wlc, start + D11AC_PHY_HDR_LEN,
			                       (len + 3) & (~3), pt);

			/* bcn len */
			wlc_write_shm(wlc, SHM_MBSS_BCNLEN0 + (2 * ucidx), len+D11AC_PHY_HDR_LEN);
		} else {
			wlc_write_template_ram(wlc, start, (len + 3) & (~3), pt);

			/* bcn len */
			wlc_write_shm(wlc, SHM_MBSS_BCNLEN0 + (2 * ucidx), len);
		}

		wlc_mbss16_setup(wlc, cfg);
		cfg->bcn_template->bcn_modified = FALSE;
	}
}

/* Write the BSSCFG's probe response template into HW, suspend MAC if requested */
void
wlc_ap_mbss16_write_prbrsp(wlc_info_t *wlc, wlc_bsscfg_t *cfg, bool suspend)
{
	wlc_pkt_t pkt;
	uint32 ucidx;
	int start;
	uint16 len;
	uint8 * pt;
	int  bcn_tmpl_len;

	bcn_tmpl_len = wlc->pub->bcn_tmpl_len;

	ucidx = WLC_BSSCFG_UCIDX(cfg);
	ASSERT(ucidx != (uint32)WLC_BSSCFG_IDX_INVALID);

	pkt = cfg->probe_template;
	ASSERT(pkt != NULL);

	WL_MBSS(("%s: wl%d.%d %smodified %d\n", __FUNCTION__, wlc->pub->unit, WLC_BSSCFG_IDX(cfg),
	         suspend ? "w/suspend " : "", cfg->prb_modified));

	/* probe response */
	if (cfg->prb_modified == TRUE) {
		if (suspend)
			wlc_suspend_mac_and_wait(wlc);

		start = MBSS_PRS_BLKS_START(wlc->max_ap_bss) + (ucidx * bcn_tmpl_len);

		if (D11REV_GE(wlc->pub->corerev, 40)) {
#ifdef WLTOEHW
			uint16 tso_hdr_size;
			d11ac_tso_t *tso_hdr;

			tso_hdr = (d11ac_tso_t *)PKTDATA(wlc->osh, pkt);
			tso_hdr_size = (uint16) (wlc->toe_bypass ? 0 :
			                         wlc_tso_hdr_length(tso_hdr));
			PKTPULL(wlc->osh, pkt, tso_hdr_size);
#endif /* WLTOEHW */
			pt = ((uint8 *)(PKTDATA(wlc->osh, pkt)) + (D11AC_TXH_LEN));
			len = PKTLEN(wlc->osh, pkt) - (D11AC_TXH_LEN);
		}
		else {
			pt = ((uint8 *)(PKTDATA(wlc->osh, pkt)) + D11_TXH_LEN);
			len = PKTLEN(wlc->osh, pkt) - D11_TXH_LEN;
		}

		ASSERT(len <= bcn_tmpl_len);

		wlc_write_template_ram(wlc, start, (len + 3) & (~3), pt);
		/* probe response len */
		wlc_write_shm(wlc, SHM_MBSS_PRSLEN0 + (2 * ucidx), len);

#ifdef WLLPRS
		if (N_ENAB(wlc->pub)) {
			wlc_pkt_t prspkt;
			uint16 lgcyprs_len_ptr;

			prspkt = cfg->lprs_template;
			ASSERT(prspkt != NULL);

			/* 11g probe resp, which follows the ht probe resp */
			start = MBSS_PRS_BLKS_START(wlc->max_ap_bss) +
				(wlc->max_ap_bss * bcn_tmpl_len) + (ucidx * LPRS_TMPL_LEN);

			if (D11REV_GE(wlc->pub->corerev, 40)) {
				pt = ((uint8 *)(PKTDATA(wlc->osh, prspkt)) + D11AC_TXH_LEN);
				len = PKTLEN(wlc->osh, prspkt) - D11AC_TXH_LEN;
			} else {
				pt = ((uint8 *)(PKTDATA(wlc->osh, prspkt)) + D11_TXH_LEN);
				len = PKTLEN(wlc->osh, prspkt) - D11_TXH_LEN;
			}

			ASSERT(len <= LPRS_TMPL_LEN);

			wlc_write_template_ram(wlc, start, (len + 3) & (~3), pt);

			lgcyprs_len_ptr = wlc_read_shm(wlc, SHM_MBSS_BC_FID3);

			wlc_write_shm(wlc, ((lgcyprs_len_ptr + ucidx) * 2), len);
		}
#endif /* WLLPRS */

		cfg->prb_modified = FALSE;

		if (suspend)
			wlc_enable_mac(wlc);
	}
}

void
wlc_mbss16_updssid(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	uint32 start;
	uint16 val;
	uint32 ssidlen = cfg->SSID_len;
	uint32 swplen;
	int8 ucidx = WLC_BSSCFG_UCIDX(cfg);
	uint8 ssidbuf[DOT11_MAX_SSID_LEN];

	ASSERT((ucidx >= 0) && (ucidx <= wlc->mbss_ucidx_mask));

	/* push ssid, ssidlen out to ucode Search Engine */
	start = SHM_MBSS_SSIDSE_BASE_ADDR + (ucidx * SHM_MBSS_SSIDSE_BLKSZ);
	/* search mem length field is always little endian */
	swplen = htol32(ssidlen);
	/* invent new function like wlc_write_shm using OBJADDR_SRCHM_SEL */
	wlc_bmac_copyto_objmem(wlc->hw, start, &swplen, SHM_MBSS_SSIDLEN_BLKSZ, OBJADDR_SRCHM_SEL);

	bzero(ssidbuf, DOT11_MAX_SSID_LEN);
	bcopy(cfg->SSID, ssidbuf, cfg->SSID_len);

	start += SHM_MBSS_SSIDLEN_BLKSZ;
	wlc_bmac_copyto_objmem(wlc->hw, start, ssidbuf, SHM_MBSS_SSID_BLKSZ, OBJADDR_SRCHM_SEL);

	/* push ssidlen out to shm
	 * XXX: again? Already sent to search engine ...
	 * XXX: awkward rmw sequence
	 */

	start = SHM_MBSS_SSIDLEN0 + (ucidx & 0xFE);
	val = wlc_read_shm(wlc, start);
	/* set bit indicating closed net if appropriate */
	if (cfg->closednet_nobcnssid)
		ssidlen |= SHM_MBSS_CLOSED_NET;

	if (ucidx & 0x01) {
		val &= 0xff;
		val |= ((uint8)ssidlen << 8);

	} else {
		val &= 0xff00;
		val |= (uint8)ssidlen;
	}
	wlc_write_shm(wlc, start, val);
}

/* Updates SHM for closed net */
static void
wlc_mbss16_upd_closednet(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	uint32 start;
	int8 ucidx = WLC_BSSCFG_UCIDX(cfg);
	uint16 val;
	uint8 shift = 0;

	ASSERT((ucidx >= 0) && (ucidx <= wlc->mbss_ucidx_mask));
	if (ucidx & 0x01)
		shift = 8;
	start = SHM_MBSS_SSIDLEN0 + (ucidx & 0xFE);
	val = wlc_read_shm(wlc, start);
	val = val & ~(SHM_MBSS_CLOSED_NET << shift);
	if (cfg->closednet_nobcnssid)
		val = val | (SHM_MBSS_CLOSED_NET << shift);
	wlc_write_shm(wlc, start, val);
}

static void
wlc_mbss16_setup(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	uint8 *bcn;
	void *pkt;
	uint16 tim_offset;

	/* find the TIM elt offset in the bcn template, push to shm */
	pkt = SPT_LATEST_PKT(cfg->bcn_template);
	bcn = (uint8 *)(PKTDATA(wlc->osh, pkt));
	tim_offset = (uint16)(cfg->bcn_template->tim_ie - bcn);
	/* we want it less the actual ssid length */
	tim_offset -= cfg->SSID_len;
	if (D11REV_GE(wlc->pub->corerev, 40)) {
		/* not sure what do to for 4360... */
		tim_offset -= D11_TXH_LEN_EX(wlc);
		tim_offset += 8;
	}
	else {
		/* and less the D11_TXH_LEN too */
		tim_offset -= D11_TXH_LEN;
	}

	wlc_write_shm(wlc, M_TIMBPOS_INBEACON, tim_offset);
}

#undef BSS_BEACONING

#endif /* MBSS */

int
wlc_ap_get_maxassoc(wlc_ap_info_t *ap)
{
	wlc_ap_info_pvt_t* appvt = (wlc_ap_info_pvt_t*) ap;

	return appvt->maxassoc;
}

void
wlc_ap_set_maxassoc(wlc_ap_info_t *ap, int val)
{
	wlc_ap_info_pvt_t* appvt = (wlc_ap_info_pvt_t*) ap;

	appvt->maxassoc = val;
}

int
wlc_ap_get_maxassoc_limit(wlc_ap_info_t *ap)
{
	wlc_info_t *wlc = ((wlc_ap_info_pvt_t *)ap)->wlc;

#if defined(MAXASSOC_LIMIT)
	if (MAXASSOC_LIMIT <= wlc->pub->tunables->maxscb)
		return MAXASSOC_LIMIT;
	else
#endif /* MAXASSOC_LIMIT */
		return wlc->pub->tunables->maxscb;
}

#ifdef WL11N
void
wlc_ht_coex_switch_bw(wlc_bsscfg_t *cfg, bool downgrade, uint8 reason_code)
{
	chanspec_t chanspec, newchanspec;
	wlc_info_t *wlc = cfg->wlc;
#ifdef WLCSA
	wl_chan_switch_t csa;
#endif // endif

	if (!cfg->associated)
		return;

	chanspec = cfg->current_bss->chanspec;

	ASSERT(cfg->BSS);
	ASSERT(cfg->obss->coex_enab);

	if ((!CHSPEC_IS40(chanspec) && downgrade) ||
	    (CHSPEC_IS40(chanspec) && !downgrade)) {
		WL_COEX(("wl%d: %s: cannot %s already at BW %s\n", wlc->pub->unit, __FUNCTION__,
			downgrade ? "downgrade":"upgrade",
			(CHSPEC_IS40(chanspec)? "40MHz":"20MHz")));
		return;
	}

	/* return if  a channel switch is already scheduled */
	if (BSS_WL11H_ENAB(wlc, cfg) &&
	    wlc_11h_get_spect_state(wlc->m11h, cfg) & NEED_TO_SWITCH_CHANNEL)
		return;

	/* upgrade AP only if previous has been downgraded */
	if (!downgrade && !CHSPEC_IS40(wlc->default_bss->chanspec))
		return;

	if (downgrade) {
		newchanspec = CH20MHZ_CHSPEC(wf_chspec_ctlchan(chanspec));
		WL_COEX(("wl%d: wlc_ht_coex_downgrad_bw\n", wlc->pub->unit));
	}
	else {
		/* If the default bss chanspec is now invalid then pick a valid one */
		if (!wlc_valid_chanspec(wlc->cmi, wlc->default_bss->chanspec))
			wlc->default_bss->chanspec = wlc_default_chanspec(wlc->cmi, TRUE);
		newchanspec = wlc->default_bss->chanspec;
	}

	if (newchanspec == chanspec)
		return;

	WL_COEX(("wl%d: %s: switching chanspec 0x%x to 0x%x reason %d\n",
		wlc->pub->unit, __FUNCTION__, chanspec, newchanspec, reason_code));

	/* XXX need to send legacy CSA and new 11n Ext-CSA if
	 * is n-enabled.  This can conflict with 11h
	 * statemachine.
	 * This code currently is used for testing!!!!
	 */
#ifdef WLCSA
	csa.mode = DOT11_CSA_MODE_ADVISORY;
	/* downgrade/upgrade with same control channel, do it immediately */
	csa.count =
		(wf_chspec_ctlchan(chanspec) == wf_chspec_ctlchan(newchanspec)) ? 0 : 10;
	csa.chspec = newchanspec;
	csa.reg = wlc_get_regclass(wlc->cmi, newchanspec);
	csa.frame_type = CSA_BROADCAST_ACTION_FRAME;
	wlc_csa_do_csa(wlc->csa, cfg, &csa, csa.count == 0);
#endif // endif
#if NCONF
	/* Update edcrs on bw change */
	wlc_bmac_ifsctl_edcrs_set(wlc->hw, WLCISHTPHY(wlc->band));
#endif /* NCONF */
}

void
wlc_ht_coex_update_permit(wlc_bsscfg_t *cfg, bool permit)
{
	ASSERT(COEX_ACTIVE(cfg));
	cfg->obss->coex_permit = permit;
}

void
wlc_ht_coex_update_fid_time(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;

	ASSERT(COEX_ACTIVE(cfg));
	cfg->obss->fid_time = wlc->pub->now;
}

/* check for coex trigger event A (TE-A)
 * a non-ht beacon
 */
bool
wlc_ht_ap_coex_tea_chk(wlc_bsscfg_t *cfg, ht_cap_ie_t *cap_ie)
{
	chanspec_t chspec = cfg->current_bss->chanspec;

	if (!BSSCFG_AP(cfg) || !cfg->associated)
		return FALSE;

	if (!CHSPEC_IS2G(chspec))
		return FALSE;

	/* get a non-ht beacon when the bss is up */
	if (!cap_ie &&
	    !(cfg->obss->coex_te_mask & COEX_MASK_TEA))
		return TRUE;

	return FALSE;
}

/* Trigger event B (TE-B)
 * For HT APs with COEX, check for Trigger Event B (TE-B) :
 * an HT beacon with HT_CAP_40MHZ_INTOLERANT bit set in HT Cap IE
 */
/* XXX Do we need to handle the BRCM IE that encapsulate the HT CAP IE
 * as Trigger event B?
 */
/* XXX Do we need to process a regular beacon as 20/40 BSS coex TE-B?
 */
static int
wlc_ht_ap_coex_scan_parse_cap_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;
	int idx;
	wlc_bsscfg_t *cfg;
	ht_cap_ie_t *cap;
	uint16 ht_cap;
	chanspec_t rxchspec;

	if (!N_ENAB(wlc->pub))
		return BCME_OK;

	if (data->ie == NULL)
		return BCME_OK;

	if ((cap = wlc_read_ht_cap_ie(wlc, data->ie, data->ie_len)) == NULL)
		return BCME_OK;

	if (((ht_cap = ltoh16_ua(&cap->cap)) & HT_CAP_40MHZ_INTOLERANT) == 0)
		return BCME_OK;

	rxchspec = CH20MHZ_CHSPEC(data->pparm->ft->bcn.chan);

	/* for AP, this is trigger event B (TE-B) */
	FOREACH_UP_AP(wlc, idx, cfg) {
		chanspec_t chspec = cfg->current_bss->chanspec;

		if (!COEX_ACTIVE(cfg))
			continue;
		if (!CHSPEC_CTLOVLP(chspec, rxchspec, CH_20MHZ_APART))
			continue;

		wlc_ht_ap_coex_tebc_proc(cfg);
	}

	return BCME_OK;
}

/*
 * process trigger event B/C (TE-B, TE-C)
 */
void
wlc_ht_ap_coex_tebc_proc(wlc_bsscfg_t *cfg)
{
	chanspec_t chspec = cfg->current_bss->chanspec;

	if (!BSSCFG_AP(cfg) || !cfg->associated)
		return;

	if (!CHSPEC_IS2G(chspec))
		return;

	WL_COEX(("wl%d: %s: obss_coex trigger event B/C detected\n",
		cfg->wlc->pub->unit, __FUNCTION__));

	wlc_ht_coex_update_fid_time(cfg);

	if (!CHSPEC_IS40(chspec))
		return;

	wlc_ht_coex_switch_bw(cfg, TRUE, COEX_DGRADE_TEBC);
}

/* Trigger event A (TE-A)
 * For HT APs with COEX, check for Trigger Event A (TE-A) :
 * a non-HT beacon is detected
 */
static int
wlc_ht_ap_coex_bcn_parse_cap_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;
	int idx;
	wlc_bsscfg_t *cfg;
	chanspec_t rxchspec;

	if (!N_ENAB(wlc->pub) || SCAN_IN_PROGRESS(wlc->scan))
		return BCME_OK;

	/* An optimization - bail out if HT Cap IE is present */
	if (data->ie != NULL)
		return BCME_OK;

	rxchspec = CH20MHZ_CHSPEC(data->pparm->ft->bcn.chan);

	FOREACH_UP_AP(wlc, idx, cfg) {
		chanspec_t chspec = cfg->current_bss->chanspec;

		if (!COEX_ACTIVE(cfg))
			continue;
		if (!CHSPEC_CTLOVLP(chspec, rxchspec, CH_20MHZ_APART))
			continue;

		if (!wlc_ht_ap_coex_tea_chk(cfg, (ht_cap_ie_t *)data->ie))
			continue;
		wlc_ht_coex_update_permit(cfg, FALSE);
		wlc_ht_coex_update_fid_time(cfg);
		if (CHSPEC_IS20(chspec))
			continue;
		wlc_ht_coex_switch_bw(cfg, TRUE, COEX_DGRADE_TEA);
	}

	return BCME_OK;
}

/* check for coex trigger event D (TE-D)
 * a 20/40 BSS coex frame
 */
bool
wlc_ht_ap_coex_ted_chk(wlc_bsscfg_t *cfg, bcm_tlv_t *bss_chanlist_tlv, uint8 coex_bits)
{
	wlc_info_t *wlc = cfg->wlc;
	dot11_obss_chanlist_t *chanlist_ie = (dot11_obss_chanlist_t *)bss_chanlist_tlv;
	uint8 chlist_len, chan, i;
	uint8 ch_min, ch_max, ch_ctl;

	WL_TRACE(("wl%d: wlc_ht_upd_coex_map\n", wlc->pub->unit));

	if (!CHSPEC_IS2G(wlc->home_chanspec))
		return FALSE;

	if (!chanlist_ie) {
		WL_ERROR(("wl%d: %s: 20/40 BSS Intolerant Channel Report IE NOT found\n",
			wlc->pub->unit, __FUNCTION__));
		return FALSE;
	}

	/* minimum len is 3 bytes (IE Hdr + regulatory class) */
	if (bss_chanlist_tlv->len < DOT11_OBSS_CHANLIST_FIXED_LEN) {
		WL_ERROR(("wl%d: %s: Invalid 20/40 BSS Intolerant Channel Report IE len %d\n",
			wlc->pub->unit, __FUNCTION__, bss_chanlist_tlv->len));
		return FALSE;
	}
	/* this is a trigger irrelevant of the channel in use  */
	if (coex_bits & ~WL_COEX_INFO_REQ) {
		return TRUE;
	}
	/* exclude regulatory class */
	chlist_len = chanlist_ie->len - DOT11_OBSS_CHANLIST_FIXED_LEN;

	WL_COEX(("wl%d: wlc_ht_upd_coex_map : %d 20MHZ channels\n", wlc->pub->unit, chlist_len));

	/* retrieve exclusion range and ctl channel */
	wlc_ht_coex_exclusion_range(wlc, &ch_min, &ch_max, &ch_ctl);
	/* If we do not do 40 mhz */
	if (!ch_min)
		return FALSE;

	for (i = 0; i < chlist_len; i++) {
		chan = chanlist_ie->chanlist[i];
		if (chan <= CH_MAX_2G_CHANNEL) {
			/*  Here we should store the date for each channel
			    It could be used in the future for auto channel
			    purposes.
			*/

			WL_COEX(("wl%d: %s: chan %d flag 0x%x\n", wlc->pub->unit, __FUNCTION__,
			         chan, WL_COEX_WIDTH20));
			if (chan >= ch_min && chan <= ch_max && chan != ch_ctl) {
				/* trigger */
				return TRUE;
			}
		}
	}
	return FALSE;
}
#endif /* WL11N */

static int
wlc_ap_ioctl(void *hdl, int cmd, void *arg, int len, struct wlc_if *wlcif)
{
	wlc_ap_info_t *ap = (wlc_ap_info_t *) hdl;
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;
	wlc_info_t *wlc = appvt->wlc;
	int val = 0, *pval;
	bool bool_val;
	int bcmerror = 0;
	struct maclist *maclist;
	wlc_bsscfg_t *bsscfg;
	struct scb_iter scbiter;
	struct scb *scb = NULL;

	/* update bsscfg pointer */
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	/* update wlcif pointer */
	if (wlcif == NULL)
		wlcif = bsscfg->wlcif;
	ASSERT(wlcif != NULL);

	/* default argument is generic integer */
	pval = (int *) arg;
	/* This will prevent the misaligned access */
	if (pval && (uint32)len >= sizeof(val))
		bcopy(pval, &val, sizeof(val));
	/* bool conversion to avoid duplication below */
	bool_val = (val != 0);

	switch (cmd) {

	case WLC_GET_SHORTSLOT_RESTRICT:
		if (AP_ENAB(wlc->pub)) {
			ASSERT(pval != NULL);
			*pval = ap->shortslot_restrict;
		}
		else
			bcmerror = BCME_NOTAP;
		break;

	case WLC_SET_SHORTSLOT_RESTRICT:
		if (AP_ENAB(wlc->pub))
			ap->shortslot_restrict = bool_val;
		else
			bcmerror = BCME_NOTAP;
		break;

#ifdef BCMDBG
	case WLC_GET_IGNORE_BCNS:
		if (AP_ENAB(wlc->pub)) {
			ASSERT(pval != NULL);
			*pval = wlc->ignore_bcns;
		}
		else
			bcmerror = BCME_NOTAP;
		break;

	case WLC_SET_IGNORE_BCNS:
		if (AP_ENAB(wlc->pub))
			wlc->ignore_bcns = bool_val;
		else
			bcmerror = BCME_NOTAP;
		break;
#endif /* BCMDBG */

	case WLC_GET_SCB_TIMEOUT:
		if (AP_ENAB(wlc->pub)) {
			ASSERT(pval != NULL);
			*pval = wlc->scb_timeout;
		}
		else
			bcmerror = BCME_NOTAP;
		break;

	case WLC_SET_SCB_TIMEOUT:
		if (AP_ENAB(wlc->pub))
			wlc->scb_timeout = val;
		else
			bcmerror = BCME_NOTAP;
		break;

	case WLC_GET_ASSOCLIST:
		ASSERT(arg != NULL);
		maclist = (struct maclist *) arg;
		ASSERT(maclist);

		/* returns a list of STAs associated with a specific bsscfg */
		if (len < (int)(sizeof(maclist->count) + (MAXSCB * ETHER_ADDR_LEN))) {
			bcmerror = BCME_BUFTOOSHORT;
			break;
		}
		val = 0;
		FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
			if (SCB_ASSOCIATED(scb)) {
				bcopy((void*)&scb->ea, (void*)&maclist->ea[val],
					ETHER_ADDR_LEN);
				val++;
			}
		}
		maclist->count = val;
		break;

	case WLC_TKIP_COUNTERMEASURES:
		if (BSSCFG_AP(bsscfg) && WSEC_TKIP_ENABLED(bsscfg->wsec))
			bsscfg->tkip_countermeasures = (val != 0);
		else
			bcmerror = BCME_BADARG;
		break;

#ifdef APCS
	case WLC_GET_CHANNEL_SEL:
		if (!AP_ACTIVE(wlc) && !SCAN_IN_PROGRESS(wlc->scan)) {
			ASSERT(pval != NULL);
			*pval = wlc->chanspec_selected;
		} else {
			bcmerror = BCME_BADARG;
		}
		break;

	case WLC_START_CHANNEL_SEL:
		/* irrelevant to 802.11h-enabled AP */
		if (WL11H_AP_ENAB(wlc)) {
			bcmerror = BCME_UNSUPPORTED;
			break;
		}

		if (AP_ACTIVE(wlc) || SCAN_IN_PROGRESS(wlc->scan)) {
			bcmerror = BCME_BUSY;
			break;
		}

		ASSERT(arg != NULL);
		bcmerror = wlc_cs_scan_start(wlc->cfg, arg, TRUE, FALSE, wlc->band->bandtype,
		                             APCS_IOCTL, NULL, NULL);

		break;

	case WLC_SET_CS_SCAN_TIMER:
		if (AP_ENAB(wlc->pub)) {
			/* for 802.11h-enabled AP, cs_scan is disallowed */
			appvt->cs_scan_timer = WL11H_ENAB(wlc) ? 0 : val * 60;
		}
		else
			bcmerror = BCME_NOTAP;
		break;

	case WLC_GET_CS_SCAN_TIMER:
		ASSERT(pval != NULL);
		*pval = appvt->cs_scan_timer / 60;
		break;
#endif /* APCS */

#ifdef RADAR
	case WLC_SET_RADAR:
		bcmerror = wlc_dfs_set_radar(wlc->dfs, (int)val);
		break;

	case WLC_GET_RADAR:
		ASSERT(pval != NULL);
		*pval = (int32)wlc_dfs_get_radar(wlc->dfs);
		break;
#endif /* RADAR */

	default:
		bcmerror = BCME_UNSUPPORTED;
		break;
	}

	return (bcmerror);
}

static int
wlc_ap_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif)
{
	wlc_ap_info_pvt_t* appvt = (wlc_ap_info_pvt_t*) hdl;
	wlc_ap_info_t* ap = &appvt->appub;
	wlc_info_t *wlc = appvt->wlc;
	wlc_bsscfg_t *bsscfg;
	int err = 0;
	int32 int_val = 0;
	int32 int_val2 = 0;
	int32 *ret_int_ptr;
	bool bool_val;
	bool bool_val2;

	/* update bsscfg w/provided interface context */
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	if (p_len >= (int)sizeof(int_val) * 2)
		bcopy((void*)((uintptr)params + sizeof(int_val)), &int_val2, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	bool_val = (int_val != 0) ? TRUE : FALSE;
	bool_val2 = (int_val2 != 0) ? TRUE : FALSE;
	BCM_REFERENCE(bool_val2);

	/* update wlcif pointer */
	if (wlcif == NULL)
		wlcif = bsscfg->wlcif;
	ASSERT(wlcif != NULL);

	/* Do the actual parameter implementation */
	switch (actionid) {

	case IOV_GVAL(IOV_AUTHE_STA_LIST):
		/* return buf is a maclist */
		err = wlc_sta_list_get(ap, bsscfg, (uint8*)arg, len,
		                       wlc_authenticated_sta_check_cb);
		break;

	case IOV_GVAL(IOV_AUTHO_STA_LIST):
		/* return buf is a maclist */
		err = wlc_sta_list_get(ap, bsscfg, (uint8*)arg, len,
		                       wlc_authorized_sta_check_cb);
		break;

	case IOV_GVAL(IOV_WME_STA_LIST):	/* Deprecated; use IOV_STA_INFO */
		/* return buf is a maclist */
		err = wlc_sta_list_get(ap, bsscfg, (uint8*)arg, len,
		                       wlc_wme_sta_check_cb);
		break;

	case IOV_GVAL(IOV_MAXASSOC):
		*(uint32*)arg = wlc_ap_get_maxassoc(wlc->ap);
		break;

	case IOV_SVAL(IOV_MAXASSOC):
		if (int_val > wlc_ap_get_maxassoc_limit(wlc->ap)) {
			err = BCME_RANGE;
			goto exit;
		}
		wlc_ap_set_maxassoc(wlc->ap, int_val);
		break;

#if defined(MBSS) || defined(WLP2P)
	case IOV_GVAL(IOV_BSS_MAXASSOC):
		*(uint32*)arg = bsscfg->maxassoc;
		break;

	case IOV_SVAL(IOV_BSS_MAXASSOC):
		if (int_val > wlc->pub->tunables->maxscb) {
			err = BCME_RANGE;
			goto exit;
		}
		bsscfg->maxassoc = int_val;
		break;
#endif /* MBSS || WLP2P */
#if defined(MBSS)
	case IOV_GVAL(IOV_BCN_ROTATE):
		if (wlc->beacon_bssidx < 0)
			*ret_int_ptr = 0;
		else
			*ret_int_ptr = 1;
		break;

	case IOV_SVAL(IOV_BCN_ROTATE):
		if (bool_val)
			wlc->beacon_bssidx = 0;
		else
			wlc->beacon_bssidx = -1;
		break;
#endif /* MBSS */

	case IOV_GVAL(IOV_AP_ISOLATE):
		*ret_int_ptr = (int32)bsscfg->ap_isolate;
		break;

	case IOV_GVAL(IOV_AP):
		*((uint*)arg) = BSSCFG_AP(bsscfg);
		break;

#if defined(STA) && defined(AP)
	case IOV_SVAL(IOV_AP):
		if (!APSTA_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		wlc_bsscfg_reinit(wlc, bsscfg, bool_val);
		break;
#endif /* defined(STA) && defined(AP) */

	case IOV_SVAL(IOV_AP_ISOLATE):
#ifndef BCMPCIEDEV_ENABLED
		bsscfg->ap_isolate = (uint8)int_val;
#endif // endif
		break;
	case IOV_GVAL(IOV_SCB_ACTIVITY_TIME):
		*ret_int_ptr = (int32)wlc->scb_activity_time;
		break;

	case IOV_SVAL(IOV_SCB_ACTIVITY_TIME):
		wlc->scb_activity_time = (uint32)int_val;
		break;

	case IOV_GVAL(IOV_SCB_MAX_PROBE):
		*ret_int_ptr = (int32)wlc->scb_max_probe;
		break;

	case IOV_SVAL(IOV_SCB_MAX_PROBE):
		wlc->scb_max_probe = (uint32)int_val;
		break;

	case IOV_GVAL(IOV_CLOSEDNET):
		*ret_int_ptr = (bsscfg->closednet_nobcnssid & bsscfg->closednet_nobcprbresp);
		break;

	case IOV_SVAL(IOV_CLOSEDNET):
		/* "closednet" control two functionalities: hide ssid in bcns
		 * and don't respond to broadcast probe requests
		 */
		bsscfg->closednet_nobcnssid = bool_val;
		bsscfg->closednet_nobcprbresp = bool_val;
		if (BSSCFG_AP(bsscfg) && bsscfg->up) {
			wlc_bss_update_beacon(wlc, bsscfg);
#ifdef MBSS
			if (MBSS_ENAB(wlc->pub))
				wlc_mbss16_upd_closednet(wlc, bsscfg);
			else
#endif // endif
				wlc_mctrl(wlc, MCTL_CLOSED_NETWORK,
				(bool_val ? MCTL_CLOSED_NETWORK : 0));
		}
		break;

	case IOV_GVAL(IOV_MBSS):
		*ret_int_ptr = wlc->pub->_mbss_mode ? TRUE : FALSE;
		break;

	case IOV_SVAL(IOV_MBSS): {
#ifdef MBSS
		bool curstat = (wlc->pub->_mbss_mode != 0);
		bool rev16 = D11REV_ISMBSS16(wlc->pub->corerev) ? TRUE : FALSE;
		bool rev4 = D11REV_ISMBSS4(wlc->pub->corerev) ? TRUE : FALSE;

		/* No change requested */
		if (curstat == bool_val)
			break;

		if (!MBSS_SUPPORT(wlc->pub) || (!rev4 && !rev16)) {
			err = BCME_UNSUPPORTED;
			break;
		}

		/* Reject if insufficient template memory */
		if (rev16 && (wlc_bmac_ucodembss_hwcap(wlc->hw) == FALSE)) {
			err = BCME_NORESOURCE;
			break;
		}

		if (curstat) {
			/* if turning off mbss, disable extra bss configs */
			wlc_bsscfg_disablemulti(wlc);
			wlc_bmac_set_defmacintmask(wlc->hw, MI_PRQ, ~MI_PRQ);
			wlc_bmac_set_defmacintmask(wlc->hw, MI_DTIM_TBTT, ~MI_DTIM_TBTT);
			wlc->pub->_mbss_mode = 0;
		}
		else {
			if (!rev16)
				wlc_bmac_set_defmacintmask(wlc->hw, MI_PRQ, MI_PRQ);
			wlc_bmac_set_defmacintmask(wlc->hw, MI_DTIM_TBTT, MI_DTIM_TBTT);
			wlc->pub->_mbss_mode = rev4 ? MBSS4_ENABLED : MBSS16_ENABLED;
		}

		if (wlc->pub->_mbss_mode && rev4) {
			wlc->max_ap_bss = 4;
			wlc->mbss_ucidx_mask = wlc->max_ap_bss - 1;
		}
#ifdef WLLPRS
		/* Enable/disable legacy prs support in ucode based on mbss
		 * state.
		 */
		wlc_mhf(wlc, MHF5, MHF5_LEGACY_PRS, (wlc->pub->_mbss_mode ? MHF5_LEGACY_PRS : 0),
			WLC_BAND_ALL);
#endif /* WLLPRS */
#else
		err = BCME_UNSUPPORTED;
#endif	/* MBSS */
		break;
	}

	case IOV_GVAL(IOV_BSS):
		if (p_len < (int)sizeof(int)) {
			err = BCME_BUFTOOSHORT;
			break;
		}
		if (int_val >= 0) {
			bsscfg = wlc_bsscfg_find(wlc, int_val, &err);
		} /* otherwise, use the value from the wlif object */

		if (bsscfg)
			*ret_int_ptr = bsscfg->up;
		else if (err == BCME_NOTFOUND)
			*ret_int_ptr = 0;
		else
			break;
		break;

	case IOV_SVAL(IOV_BSS): {
		bool sta = FALSE;

		if (len < (int)(2 * sizeof(int))) {
			err = BCME_BUFTOOSHORT;
			break;
		}
#ifdef STA
		if (int_val2 == WLC_AP_IOV_OP_MANUAL_AP_BSSCFG_CREATE) {
			if (STA_ONLY(wlc->pub)) {
				err = BCME_BADARG;
				break;
			}
			sta = FALSE;
		} else if (int_val2 == WLC_AP_IOV_OP_MANUAL_STA_BSSCFG_CREATE) {
			if (AP_ONLY(wlc->pub)) {
				err = BCME_BADARG;
				break;
			}
			sta = TRUE;
		}
#endif // endif
		/* On an 'up', use wlc_bsscfg_alloc() to create a bsscfg if it does not exist,
		 * but on a 'down', just find the bsscfg if it already exists
		 */
		if (int_val >= 0) {
			bsscfg = wlc_bsscfg_find(wlc, int_val, &err);
			if (int_val2 > WLC_AP_IOV_OP_DISABLE) {
				if (bsscfg == NULL && err == BCME_NOTFOUND) {
					bsscfg = wlc_bsscfg_alloc(wlc, int_val, 0, NULL, !sta);
					if (bsscfg == NULL)
						err = BCME_NOMEM;
					else if ((err = wlc_bsscfg_init(wlc, bsscfg))) {
						WL_ERROR(("wl%d: wlc_bsscfg_init %s failed (%d)\n",
						          wlc->pub->unit, sta ? "STA" : "AP", err));
						wlc_bsscfg_free(wlc, bsscfg);
						break;
					}
				}
			}
			/* delete bsscfg */
			if (int_val2 == WLC_AP_IOV_OP_DELETE) {
				if (bsscfg != NULL) {
					if (bsscfg->enable) {
#ifdef WLAWDL
						if (BSSCFG_AWDL(wlc, bsscfg) && wlc->awdl_info)
							wlc_awdl_delete(wlc->awdl_info);
#endif // endif
						wlc_bsscfg_disable(wlc, bsscfg);
					}
					wlc_bsscfg_free(wlc, bsscfg);
				}
				break;
			}
		}

#ifdef STA
		if (int_val2 == WLC_AP_IOV_OP_MANUAL_STA_BSSCFG_CREATE)
			break;
		else if (int_val2 == WLC_AP_IOV_OP_MANUAL_AP_BSSCFG_CREATE)
			break;
#endif // endif
		if (bsscfg == NULL) { /* the condition is logically equivalent to if (err) .. */
			/* do not error on a 'down' of a nonexistent bsscfg */
			if (err == BCME_NOTFOUND && int_val2 == WLC_AP_IOV_OP_DISABLE)
				err = 0;
			break;
		}

		if (int_val2 > WLC_AP_IOV_OP_DISABLE) {
			if (bsscfg->up) {
				WL_APSTA_UPDN(("wl%d: Ignoring UP, bsscfg %d already UP\n",
					wlc->pub->unit, int_val));
				break;
			}
			if (mboolisset(wlc->pub->radio_disabled, WL_RADIO_HW_DISABLE) ||
				mboolisset(wlc->pub->radio_disabled, WL_RADIO_SW_DISABLE)) {
				WL_APSTA_UPDN(("wl%d: Ignoring UP, bsscfg %d; radio off\n",
					wlc->pub->unit, int_val));
				err = BCME_RADIOOFF;
				break;
			}

			WL_APSTA_UPDN(("wl%d: BSS up cfg %d (%s) -> wlc_bsscfg_enable()\n",
				wlc->pub->unit, int_val, (BSSCFG_AP(bsscfg) ? "AP" : "STA")));
			if (BSSCFG_AP(bsscfg))
				err = wlc_bsscfg_enable(wlc, bsscfg);
#ifdef WLP2P
			else if (BSS_P2P_ENAB(wlc, bsscfg))
				err = BCME_ERROR;
#endif // endif
#ifdef STA
			else if (BSSCFG_STA(bsscfg))
				wlc_join(wlc, bsscfg, bsscfg->SSID, bsscfg->SSID_len,
				         NULL, NULL, 0);
#endif // endif
			if (err)
				break;
#ifdef RADAR
			if (WL11H_AP_ENAB(wlc)) {
				if (BSSCFG_AP(bsscfg) && !(BSSCFG_SRADAR_ENAB(bsscfg) ||
				      BSSCFG_AP_NORADAR_CHAN_ENAB(bsscfg)))
					wlc_set_dfs_cacstate(wlc->dfs, ON);
			}
#endif /* RADAR */
		} else {
			if (!bsscfg->enable) {
				WL_APSTA_UPDN(("wl%d: Ignoring DOWN, bsscfg %d already DISABLED\n",
					wlc->pub->unit, int_val));
				break;
			}
			WL_APSTA_UPDN(("wl%d: BSS down on %d (%s) -> wlc_bsscfg_disable()\n",
				wlc->pub->unit, int_val, (BSSCFG_AP(bsscfg) ? "AP" : "STA")));
			wlc_bsscfg_disable(wlc, bsscfg);

			/* Turn of radar_detect if none of AP's are on radar chanspec */
			if (WL11H_AP_ENAB(wlc)) {
				if (!wlc_ap_on_radarchan(wlc->ap) && !(BSSCFG_SRADAR_ENAB(bsscfg) ||
					BSSCFG_AP_NORADAR_CHAN_ENAB(bsscfg)))
					wlc_set_dfs_cacstate(wlc->dfs, OFF);
			}
		}
		break;
	}

	case IOV_GVAL(IOV_APCSCHSPEC):
		if (wlc->pub->up && AP_ENAB(wlc->pub) &&
		    !SCAN_IN_PROGRESS(wlc->scan)) {
			*ret_int_ptr = (int32)wlc->chanspec_selected;
		}
		else {
			err = BCME_BADARG;
		}
		break;

#if defined(STA) /* APSTA */
	case IOV_GVAL(IOV_APSTA):
		*ret_int_ptr = APSTA_ENAB(wlc->pub);
		break;

	case IOV_SVAL(IOV_APSTA):
		/* Flagged for no set while up */
		if (bool_val == APSTA_ENAB(wlc->pub)) {
			WL_APSTA(("wl%d: No change to APSTA mode\n", wlc->pub->unit));
			break;
		}

		bsscfg = wlc_bsscfg_primary(wlc);
		if (bool_val) {
			/* Turning on APSTA, force various other items:
			 *   Global AP, cfg (wlc->cfg) STA, not IBSS.
			 *   Make beacon/probe AP config bsscfg[1].
			 *   Force off 11D.
			 */
			WL_APSTA(("wl%d: Enabling APSTA mode\n", wlc->pub->unit));
			if (bsscfg->enable)
				wlc_bsscfg_disable(wlc, bsscfg);
		}
		else {
			/* Turn off APSTA: make global AP and cfg[0] same */
			WL_APSTA(("wl%d: Disabling APSTA mode\n", wlc->pub->unit));
		}
		err = wlc_bsscfg_reinit(wlc, bsscfg, bool_val == 0);
		if (err)
			break;
		wlc->pub->_ap = TRUE;
		if (bool_val) {
			wlc->default_bss->infra = 1;
		}
		wlc->pub->_apsta = bool_val;

		/* Act similarly to WLC_SET_AP */
		wlc_ap_upd(wlc, bsscfg);
		wlc->wet = FALSE;
		wlc_radio_mpc_upd(wlc);
		break;

#endif /* APSTA */

	case IOV_GVAL(IOV_PREF_CHANSPEC):
		*ret_int_ptr = (int32)wlc->ap->pref_chanspec;
		break;

	case IOV_SVAL(IOV_PREF_CHANSPEC):
		/* Special case to clear user preferred setting.
		 * 0 - pref_chanspec means we're using Auto Channel.
		 */
		if (int_val == 0) {
			wlc->ap->pref_chanspec = 0;
			break;
		}

		if (!wlc_valid_chanspec_db(wlc->cmi, (chanspec_t)int_val)) {
			err = BCME_BADCHAN;
			break;
		}

		wlc->ap->pref_chanspec = (chanspec_t)int_val;
		break;

#ifdef RXCHAIN_PWRSAVE
	case IOV_GVAL(IOV_RXCHAIN_PWRSAVE_ENABLE):
		*ret_int_ptr = ap->rxchain_pwrsave_enable;
		break;

	case IOV_SVAL(IOV_RXCHAIN_PWRSAVE_ENABLE):
		if (!int_val)
			appvt->rxchain_pwrsave.pwrsave.power_save_check &= ~PWRSAVE_ENB;
		else
			appvt->rxchain_pwrsave.pwrsave.power_save_check |= PWRSAVE_ENB;

		ap->rxchain_pwrsave_enable = int_val;
		if (!int_val)
			wlc_reset_rxchain_pwrsave_mode(ap);

#ifdef WLPM_BCNRX
		if (PM_BCNRX_ENAB(wlc->pub) && bool_val) {
			/* Avoid ucode interference if AP enables this power-save mode */
			wlc_pm_bcnrx_disable(wlc);
		}
#endif // endif
		break;
	case IOV_GVAL(IOV_RXCHAIN_PWRSAVE_QUIET_TIME):
	        *ret_int_ptr = appvt->rxchain_pwrsave.pwrsave.quiet_time;
		break;

	case IOV_SVAL(IOV_RXCHAIN_PWRSAVE_QUIET_TIME):
		appvt->rxchain_pwrsave.pwrsave.quiet_time = int_val;
		break;
	case IOV_GVAL(IOV_RXCHAIN_PWRSAVE_PPS):
	        *ret_int_ptr = appvt->rxchain_pwrsave.pwrsave.pps_threshold;
		break;

	case IOV_SVAL(IOV_RXCHAIN_PWRSAVE_PPS):
		appvt->rxchain_pwrsave.pwrsave.pps_threshold = int_val;
		break;
	case IOV_GVAL(IOV_RXCHAIN_PWRSAVE):
	        *ret_int_ptr = appvt->rxchain_pwrsave.pwrsave.in_power_save;
		break;
	case IOV_GVAL(IOV_RXCHAIN_PWRSAVE_STAS_ASSOC_CHECK):
	        *ret_int_ptr = appvt->rxchain_pwrsave.pwrsave.stas_assoc_check;
		break;
	case IOV_SVAL(IOV_RXCHAIN_PWRSAVE_STAS_ASSOC_CHECK):
		appvt->rxchain_pwrsave.pwrsave.stas_assoc_check = int_val;
		if (int_val && ap->rxchain_pwrsave_enable &&
		    appvt->rxchain_pwrsave.pwrsave.in_power_save &&
		    wlc_ap_stas_associated(wlc->ap)) {
			wlc_reset_rxchain_pwrsave_mode(ap);
		}
		break;
#endif /* RXCHAIN_PWRSAVE */
#ifdef RADIO_PWRSAVE
	case IOV_GVAL(IOV_RADIO_PWRSAVE_ENABLE):
		*ret_int_ptr = ap->radio_pwrsave_enable;
		break;

	case IOV_SVAL(IOV_RADIO_PWRSAVE_ENABLE):
		if (!MBSS_ENAB(wlc->pub)) {
			err = BCME_EPERM;
			WL_ERROR(("wl%d: Radio pwrsave not supported in non-mbss case yet.\n",
				wlc->pub->unit));
			break;
		}
		ap->radio_pwrsave_enable = appvt->radio_pwrsave.pwrsave.power_save_check = int_val;
		wlc_reset_radio_pwrsave_mode(ap);

#ifdef WLPM_BCNRX
		if (PM_BCNRX_ENAB(wlc->pub) && bool_val) {
			/* Avoid ucode interference if AP enables this power-save mode */
			wlc_pm_bcnrx_disable(wlc);
		}
#endif // endif
		break;
	case IOV_GVAL(IOV_RADIO_PWRSAVE_QUIET_TIME):
		*ret_int_ptr = appvt->radio_pwrsave.pwrsave.quiet_time;
		break;

	case IOV_SVAL(IOV_RADIO_PWRSAVE_QUIET_TIME):
		appvt->radio_pwrsave.pwrsave.quiet_time = int_val;
		break;
	case IOV_GVAL(IOV_RADIO_PWRSAVE_PPS):
		*ret_int_ptr = appvt->radio_pwrsave.pwrsave.pps_threshold;
		break;

	case IOV_SVAL(IOV_RADIO_PWRSAVE_PPS):
		appvt->radio_pwrsave.pwrsave.pps_threshold = int_val;
		break;
	case IOV_GVAL(IOV_RADIO_PWRSAVE):
		*ret_int_ptr = appvt->radio_pwrsave.pwrsave.in_power_save;
		break;
	case IOV_SVAL(IOV_RADIO_PWRSAVE_LEVEL):{
		uint8 dtim_period;
		uint16 beacon_period;

		bsscfg = wlc->cfg;

		if (bsscfg->associated) {
			dtim_period = bsscfg->current_bss->dtim_period;
			beacon_period = bsscfg->current_bss->beacon_period;
		} else {
			dtim_period = wlc->default_bss->dtim_period;
			beacon_period = wlc->default_bss->beacon_period;
		}

		if (int_val > RADIO_PWRSAVE_HIGH) {
			err = BCME_RANGE;
			goto exit;
		}

		if (dtim_period == 1) {
			err = BCME_ERROR;
			goto exit;
		}

		appvt->radio_pwrsave.level = int_val;
		switch (appvt->radio_pwrsave.level) {
		case RADIO_PWRSAVE_LOW:
			appvt->radio_pwrsave.on_time = 2*beacon_period*dtim_period/3;
			appvt->radio_pwrsave.off_time = beacon_period*dtim_period/3;
			break;
		case RADIO_PWRSAVE_MEDIUM:
			appvt->radio_pwrsave.on_time = beacon_period*dtim_period/2;
			appvt->radio_pwrsave.off_time = beacon_period*dtim_period/2;
			break;
		case RADIO_PWRSAVE_HIGH:
			appvt->radio_pwrsave.on_time = beacon_period*dtim_period/3;
			appvt->radio_pwrsave.off_time = 2*beacon_period*dtim_period/3;
			break;
		}
		break;
	}
	case IOV_GVAL(IOV_RADIO_PWRSAVE_LEVEL):
		*ret_int_ptr = appvt->radio_pwrsave.level;
		break;
	case IOV_SVAL(IOV_RADIO_PWRSAVE_STAS_ASSOC_CHECK):
		appvt->radio_pwrsave.pwrsave.stas_assoc_check = int_val;
		if (int_val && RADIO_PWRSAVE_ENAB(wlc->ap) &&
		    wlc_radio_pwrsave_in_power_save(wlc->ap) &&
		    wlc_ap_stas_associated(wlc->ap)) {
			wlc_radio_pwrsave_exit_mode(wlc->ap);
			WL_INFORM(("Going out of power save as there are associated STASs!\n"));
		}
		break;
	case IOV_GVAL(IOV_RADIO_PWRSAVE_STAS_ASSOC_CHECK):
	        *ret_int_ptr = appvt->radio_pwrsave.pwrsave.stas_assoc_check;
		break;
#endif /* RADIO_PWRSAVE */
#ifdef BCM_DCS
	case IOV_GVAL(IOV_BCMDCS):
		*ret_int_ptr = ap->dcs_enabled ? TRUE : FALSE;
		break;

	case IOV_SVAL(IOV_BCMDCS):
		ap->dcs_enabled = bool_val;
		break;

#endif /* BCM_DCS */
	case IOV_GVAL(IOV_DYNBCN):
		*ret_int_ptr = (int32)((bsscfg->flags & WLC_BSSCFG_DYNBCN) == WLC_BSSCFG_DYNBCN);
		break;
	case IOV_SVAL(IOV_DYNBCN):
		if (!BSSCFG_AP(bsscfg)) {
			err = BCME_UNSUPPORTED;
			break;
		}

		if (int_val && !DYNBCN_ENAB(bsscfg)) {
			bsscfg->flags |= WLC_BSSCFG_DYNBCN;

			/* Disable beacons if no sta is associated */
			if (wlc_bss_assocscb_getcnt(wlc, bsscfg) == 0)
				wlc_bsscfg_bcn_disable(wlc, bsscfg);
		} else if (!int_val && DYNBCN_ENAB(bsscfg)) {
			bsscfg->flags &= ~WLC_BSSCFG_DYNBCN;
			wlc_bsscfg_bcn_enable(wlc, bsscfg);
		}
		break;

	case IOV_GVAL(IOV_SCB_LASTUSED): {
		uint elapsed = 0;
		uint min_val = (uint)-1;
		struct scb *scb;
		struct scb_iter scbiter;

		FOREACHSCB(wlc->scbstate, &scbiter, scb) {
			if (SCB_ASSOCIATED(scb)) {
				elapsed = wlc->pub->now - scb->used;
				if (elapsed < min_val)
					min_val = elapsed;
			}
		}
		*ret_int_ptr = min_val;
		break;
	}

	case IOV_GVAL(IOV_SCB_PROBE): {
		wl_scb_probe_t scb_probe;

		scb_probe.scb_timeout = wlc->scb_timeout;
		scb_probe.scb_activity_time = wlc->scb_activity_time;
		scb_probe.scb_max_probe = wlc->scb_max_probe;

		bcopy((char *)&scb_probe, (char *)arg, sizeof(wl_scb_probe_t));
		break;
	}

	case IOV_SVAL(IOV_SCB_PROBE): {
		wl_scb_probe_t *scb_probe = (wl_scb_probe_t *)arg;

		if (!scb_probe->scb_timeout || (!scb_probe->scb_max_probe)) {
			err = BCME_BADARG;
			break;
		}

		wlc->scb_timeout = scb_probe->scb_timeout;
		wlc->scb_activity_time = scb_probe->scb_activity_time;
		wlc->scb_max_probe = scb_probe->scb_max_probe;
		break;
	}

	case IOV_GVAL(IOV_SCB_ASSOCED): {

		bool assoced = TRUE;
		struct scb *scb;
		struct scb_iter scbiter;

		FOREACHSCB(wlc->scbstate, &scbiter, scb) {
			if (SCB_ASSOCIATED(scb))
				break;
		}

		if (!scb)
			assoced = FALSE;

		*ret_int_ptr = (uint32)assoced;
		break;
	}

	case IOV_SVAL(IOV_ACS_UPDATE):

		if (SCAN_IN_PROGRESS(wlc->scan)) {
			err = BCME_BUSY;
			break;
		}

		if (wlc->pub->up && (wlc->chanspec_selected != 0) &&
		    (WLC_BAND_PI_RADIO_CHANSPEC != wlc->chanspec_selected))
			wlc_ap_acs_update(wlc);

		if (WL11H_AP_ENAB(wlc) && AP_ACTIVE(wlc)) {
			if (wlc_radar_chanspec(wlc->cmi, wlc->chanspec_selected))
				wlc_set_dfs_cacstate(wlc->dfs, ON);
			else
				wlc_set_dfs_cacstate(wlc->dfs, OFF);
		}
		break;

	case IOV_GVAL(IOV_BEACON_INFO):
	case IOV_GVAL(IOV_PROBE_RESP_INFO): {
		/* Retrieve the 802.11 beacon management packet */
		int hdr_skip = 0;
		int pkt_len = 0;
		uint8 *pkt_data = NULL;
		uint16 *template = NULL;

		if (BSSCFG_AP(bsscfg) && bsscfg->up) {
			if (MBSS_BCN_ENAB(wlc, bsscfg)) {
#if defined(MBSS)
				wlc_pkt_t pkt;

				if (IOV_ID(actionid) == IOV_BEACON_INFO) {
					hdr_skip = D11_TXH_LEN_EX(wlc);
					pkt = SPT_LATEST_PKT(bsscfg->bcn_template);
#ifdef WLTOEHW
					{
						uint16 tso_hdr_size;
						d11ac_tso_t *tso_hdr;
						/* Pull off space so that d11hdrs below works */
						tso_hdr = (d11ac_tso_t *) PKTDATA(wlc->osh, pkt);
						tso_hdr_size = (uint16) (wlc->toe_bypass ? 0 :
							wlc_tso_hdr_length(tso_hdr));
						hdr_skip += tso_hdr_size;
					}
#endif /* WLTOEHW */
					pkt_len = PKTLEN(wlc->osh, pkt);
					pkt_data = (uint8 *)PKTDATA(wlc->osh, pkt);
				} else {
					/* IOV_PROBE_RESP_INFO */
					if ((pkt = bsscfg->probe_template)) {
						hdr_skip = D11_TXH_LEN_EX(wlc);
						pkt_len = PKTLEN(wlc->osh, pkt);
						pkt_data = (uint8 *)PKTDATA(wlc->osh, pkt);
					}
				}
#endif /* MBSS */
			} else if (HWBCN_ENAB(bsscfg)) {
				uint type;
				ratespec_t rspec;

				hdr_skip = wlc->pub->d11tpl_phy_hdr_len;
				pkt_len = wlc->pub->bcn_tmpl_len;
				template = (uint16 *)MALLOC(wlc->pub->osh, pkt_len);
				if (template == NULL) {
					WL_ERROR(("wl%d: %s: MALLOC template failed,"
						" malloced %d bytes\n", wlc->pub->unit,
						__FUNCTION__, MALLOCED(wlc->osh)));
					err = BCME_NOMEM;
					break;
				}
				pkt_data = (uint8 *) template;

				if (IOV_ID(actionid) == IOV_BEACON_INFO) {
					type = FC_BEACON;
					rspec = wlc_lowest_basic_rspec(wlc,
						&bsscfg->current_bss->rateset);
				} else {
					/* IOV_PROBE_RESP_INFO */
					type = FC_PROBE_RESP;
					rspec = (ratespec_t) 0;
				}

				wlc_bcn_prb_template(wlc, type, rspec,
				                     bsscfg, template, &pkt_len);
			}
		}

		/* Only interested in the management frame */
		pkt_data += hdr_skip;
		pkt_len -= hdr_skip;

		if (len >= (pkt_len + (int) sizeof(int_val))) {
			*ret_int_ptr = (int32) pkt_len;
			if (pkt_len > 0) {
				arg = (char *)arg + sizeof(int_val);
				bcopy(pkt_data, arg, pkt_len);
			}
		} else {
			err = BCME_BUFTOOSHORT;
		}

		if (template) {
			MFREE(wlc->pub->osh, template, pkt_len);
		}

	        break;
	}
	case IOV_GVAL(IOV_MODE_REQD):
		*ret_int_ptr = (int32)wlc_ap_get_opmode_cap_reqd(wlc, bsscfg);
		break;
	case IOV_SVAL(IOV_MODE_REQD):
		err = wlc_ap_set_opmode_cap_reqd(wlc, bsscfg, int_val);
		break;

#ifdef PSPRETEND
	case IOV_GVAL(IOV_PSPRETEND_THRESHOLD):
	{
		*ret_int_ptr = (int32)bsscfg->ps_pretend_threshold;
	}
	break;

	case IOV_SVAL(IOV_PSPRETEND_THRESHOLD):
	{
		bsscfg->ps_pretend_threshold = (uint8)int_val;
		if (WLC_TXC_ENAB(wlc)) {
			wlc_txc_inv_all(wlc->txc);
		}
	}
	break;

	case IOV_GVAL(IOV_PSPRETEND_RETRY_LIMIT):
	{
		*ret_int_ptr = (int32)bsscfg->ps_pretend_retry_limit;
	}
	break;

	case IOV_SVAL(IOV_PSPRETEND_RETRY_LIMIT):
	{
		bsscfg->ps_pretend_retry_limit = (uint8)int_val;
		if (WLC_TXC_ENAB(wlc)) {
			wlc_txc_inv_all(wlc->txc);
		}
	}
	break;
#endif /* PSPRETEND */

	case IOV_GVAL(IOV_BSS_RATESET):
		if (bsscfg)
			*ret_int_ptr = (int32)bsscfg->rateset;
		else {
			*ret_int_ptr = 0;
			err = BCME_BADARG;
		}
		break;

	case IOV_SVAL(IOV_BSS_RATESET):
		if (!bsscfg || int_val < WLC_BSSCFG_RATESET_DEFAULT ||
		            int_val > WLC_BSSCFG_RATESET_MAX)
			err = BCME_BADARG;
		else if (bsscfg->up)
			/* do not change rateset while this bss is up */
			err = BCME_NOTDOWN;
		else
			bsscfg->rateset = (uint8)int_val;
		break;

	case IOV_GVAL(IOV_FORCE_BCN_RSPEC):
		*ret_int_ptr = (int32)(appvt->force_bcn_rspec & RATE_MASK);

		break;

	case IOV_SVAL(IOV_FORCE_BCN_RSPEC): {
		ratespec_t rspec = (ratespec_t)(int_val & RATE_MASK);

		/* escape if new value identical to current value */
		if ((rspec & RATE_MASK) == (appvt->force_bcn_rspec & RATE_MASK))
			break;

		err = wlc_force_bcn_rspec_upd(wlc, bsscfg, ap, rspec);
		break;

	}
	break;

#ifdef USBAP
	case IOV_GVAL(IOV_WLANCOEX):
		*ret_int_ptr = (int32)appvt->wlancoex;
		break;

	case IOV_SVAL(IOV_WLANCOEX): {
		if (appvt->wlancoex == bool_val)
			break;

		err = wlc_wlancoex_upd(wlc, ap, bool_val);
		break;
	}
#endif /* USBAP */

#ifdef WLAUTHRESP_MAC_FILTER
	case IOV_GVAL(IOV_AUTHRESP_MACFLTR):
		if (BSSCFG_AP(bsscfg))
			*ret_int_ptr = (int32)(AP_BSSCFG_CUBBY(appvt, bsscfg)->authresp_macfltr);
		else
			err = BCME_NOTAP;
		break;

	case IOV_SVAL(IOV_AUTHRESP_MACFLTR):
		if (BSSCFG_AP(bsscfg))
			AP_BSSCFG_CUBBY(appvt, bsscfg)->authresp_macfltr = bool_val;
		else
			err = BCME_NOTAP;
		break;
#endif /* WLAUTHRESP_MAC_FILTER */

	case IOV_GVAL(IOV_PROXY_ARP_ADVERTISE):
		if (BSSCFG_AP(bsscfg)) {
			*ret_int_ptr = isset(bsscfg->ext_cap, DOT11_EXT_CAP_PROXY_ARP) ? 1 : 0;
		} else {
			err = BCME_NOTAP;
		}
		break;

	case IOV_SVAL(IOV_PROXY_ARP_ADVERTISE):
		if (BSSCFG_AP(bsscfg)) {
			/* update extend capabilities */
			wlc_bsscfg_set_ext_cap(bsscfg, DOT11_EXT_CAP_PROXY_ARP, bool_val);
			if (bsscfg->up) {
				/* update proxy arp service bit in probe response and beacons */
				wlc_bss_update_beacon(wlc, bsscfg);
				wlc_bss_update_probe_resp(wlc, bsscfg, TRUE);
			}
		} else {
			err = BCME_NOTAP;
		}
		break;
#ifdef MULTIAP
	case IOV_GVAL(IOV_MAP):
		*ret_int_ptr = 0;
		if (bsscfg->map_attr & MAP_EXT_ATTR_FRNTHAUL_BSS) {
			setbit(ret_int_ptr, 0);
		}
		if (bsscfg->map_attr & MAP_EXT_ATTR_BACKHAUL_BSS) {
			setbit(ret_int_ptr, 1);
		}
		if (bsscfg->map_attr & MAP_EXT_ATTR_BACKHAUL_STA) {
			setbit(ret_int_ptr, 2);
		}
		break;
	case IOV_SVAL(IOV_MAP): {
		uint8 flags = 0;

		if ((isset(&int_val, 0) || isset(&int_val, 1)) && !BSSCFG_AP(bsscfg)) {
			WL_ERROR(("Error. Trying to set Multi-AP Fronthaul or Backhaul AP"
				" on a non AP interface\n"));
			err = BCME_UNSUPPORTED;
			break;
		}

		if (isset(&int_val, 2) && !BSSCFG_STA(bsscfg)) {
			WL_ERROR(("Error. Trying to set Multi-AP Backhaul STA"
				" on a non STA interface\n"));
			err = BCME_UNSUPPORTED;
			break;
		}

		if (isset(&int_val, 0)) {
			flags |= MAP_EXT_ATTR_FRNTHAUL_BSS;
		}
		if (isset(&int_val, 1)) {
			flags |= MAP_EXT_ATTR_BACKHAUL_BSS;
		}
		if (isset(&int_val, 2)) {
			flags |= MAP_EXT_ATTR_BACKHAUL_STA;
		}
		bsscfg->_map = flags ? TRUE : FALSE;
		bsscfg->map_attr = flags;
		(void)wlc_bss_update_multiap_ie(wlc, bsscfg);
	}
	break;
#endif	/* MULTIAP */

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

exit:
	return err;
}

static void
wlc_ap_acs_update(wlc_info_t *wlc)
{

	WL_INFORM(("wl%d: %s: changing chanspec to %d\n",
		wlc->pub->unit, __FUNCTION__, wlc->chanspec_selected));
	wlc_set_home_chanspec(wlc, wlc->chanspec_selected);
	wlc_suspend_mac_and_wait(wlc);

	wlc_set_chanspec(wlc, wlc->chanspec_selected);
	if (AP_ENAB(wlc->pub)) {
		wlc->bcn_rspec = wlc_lowest_basic_rspec(wlc,
			&wlc->cfg->current_bss->rateset);
		ASSERT(wlc_valid_rate(wlc, wlc->bcn_rspec,
			CHSPEC_IS2G(wlc->cfg->current_bss->chanspec) ?
			WLC_BAND_2G : WLC_BAND_5G, TRUE));
		wlc_beacon_phytxctl(wlc, wlc->bcn_rspec, wlc->chanspec);
	}

	if (wlc->pub->associated) {
		wlc_update_beacon(wlc);
		wlc_update_probe_resp(wlc, FALSE);
	}
	wlc_enable_mac(wlc);
}

/*
 * Set the operational capabilities for STAs required to associate to the BSS.
 */
static int
wlc_ap_set_opmode_cap_reqd(wlc_info_t* wlc, wlc_bsscfg_t *bsscfg,
	opmode_cap_t opmode)
{
	int err = 0;

	wlc_ap_info_pvt_t *appvt;
	ap_bsscfg_cubby_t *ap_cfg;

	ASSERT(wlc != NULL);
	ASSERT(bsscfg != NULL);

	appvt = (wlc_ap_info_pvt_t *) wlc->ap;
	ASSERT(appvt != NULL);
	ap_cfg = AP_BSSCFG_CUBBY(appvt, bsscfg);
	ASSERT(ap_cfg != NULL);

	if (opmode >= OMC_MAX) {
		err = BCME_RANGE;
		goto exit;
	}

	/* can only change setting if the BSS is down */
	if (bsscfg->up) {
		err = BCME_ASSOCIATED;
		goto exit;
	}

	/* apply setting */
	ap_cfg->opmode_cap_reqd = opmode;

exit:
	return err;
}

/*
 * Get the operational capabilities for STAs required to associate to the BSS.
 */
static opmode_cap_t
wlc_ap_get_opmode_cap_reqd(wlc_info_t* wlc, wlc_bsscfg_t *bsscfg)
{
	wlc_ap_info_pvt_t *appvt;
	ap_bsscfg_cubby_t *ap_cfg;

	ASSERT(wlc != NULL);
	ASSERT(bsscfg != NULL);

	appvt = (wlc_ap_info_pvt_t *) wlc->ap;
	ASSERT(appvt != NULL);
	ap_cfg = AP_BSSCFG_CUBBY(appvt, bsscfg);
	ASSERT(ap_cfg != NULL);

	/* return the setting */
	return ap_cfg->opmode_cap_reqd;
}

static int wlc_ap_bsscfg_init(void *context, wlc_bsscfg_t *cfg)
{
	wlc_ap_info_pvt_t *appvt;
	ap_bsscfg_cubby_t *ap_cfg;

	ASSERT(context != NULL);
	ASSERT(cfg != NULL);

	appvt = (wlc_ap_info_pvt_t *)context;
	ap_cfg = AP_BSSCFG_CUBBY(appvt, cfg);
	ASSERT(ap_cfg != NULL);

	/* The operational mode capabilities init */
	ap_cfg->opmode_cap_reqd = OMC_NONE;

	return 0;
}

#if defined(EXT_STA)|| defined(SPLIT_ASSOC)
static void
wlc_ap_scb_free_notify(void *context, struct scb *scb)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t*)context;
	wlc_info_t *wlc = appvt->wlc;
	wlc_bsscfg_t *bsscfg = SCB_BSSCFG(scb);
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN], *ea = bcm_ether_ntoa(&scb->ea, eabuf);
#endif /* BCMDBG */
	wlc_assoc_req_t * param = SCB_ASSOC_CUBBY(appvt, scb);
	/* free param body if not freed during assoc stage */
	if (WLEXTSTA_ENAB(wlc->pub) || SPLIT_ASSOC_REQ(bsscfg)) {
		if (param != NULL && param->body != NULL) {
			MFREE(wlc->osh, param->body, param->buf_len);
			bzero(param, sizeof(wlc_assoc_req_t));
		}
	}

	if (bsscfg && BSSCFG_AP(bsscfg) && SCB_ASSOCIATED(scb) && wlc->eventq) {
		WL_ASSOC(("wl%d: AP: scb free: indicate disassoc for the STA-%s\n",
			wlc->pub->unit, ea));
		wlc_bss_mac_event(wlc, scb->bsscfg, WLC_E_DISASSOC_IND, &scb->ea,
			WLC_E_STATUS_SUCCESS, DOT11_RC_DISASSOC_LEAVING, 0, 0, 0);
	}
}
#endif /* defined(EXT_STA) || defined(SPLIT_ASSOC) */

void
wlc_ap_bsscfg_scb_cleanup(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	struct scb_iter scbiter;
	struct scb *scb;

	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
		if (SCB_ASSOCIATED(scb)) {
			if (!scb->permanent) {
				wlc_scbfree(wlc, scb);
			}
		}
	}
}

void
wlc_ap_scb_cleanup(wlc_info_t *wlc)
{
	wlc_bsscfg_t *apcfg;
	int idx;

	FOREACH_UP_AP(wlc, idx, apcfg) {
		wlc_txflowcontrol_override(wlc, apcfg->wlcif->qi, OFF, TXQ_STOP_FOR_PKT_DRAIN);
		wlc_scb_update_band_for_cfg(wlc, apcfg, WLC_BAND_PI_RADIO_CHANSPEC);
	}
}

#if defined(RXCHAIN_PWRSAVE) || defined(RADIO_PWRSAVE)
/*
 * Returns true if check for associated STAs is enabled
 * and there are STAs associated
 */
static bool
wlc_pwrsave_stas_associated_check(wlc_ap_info_t *ap, int type)
{
	wlc_ap_info_pvt_t* appvt = (wlc_ap_info_pvt_t*) ap;
	bool check_assoc_stas = FALSE;

	switch (type) {
#ifdef RXCHAIN_PWRSAVE
		case PWRSAVE_RXCHAIN:
			check_assoc_stas = appvt->rxchain_pwrsave.pwrsave.stas_assoc_check;
			break;
#endif // endif
#ifdef RADIO_PWRSAVE
		case PWRSAVE_RADIO:
			check_assoc_stas = appvt->radio_pwrsave.pwrsave.stas_assoc_check;
			break;
#endif // endif
		default:
			break;
	}
	return (check_assoc_stas && (wlc_ap_stas_associated(ap) > 0));
}

/*
 * At every watchdog tick we update the power save
 * data structures and see if we can go into a power
 * save mode
 */
static void
wlc_pwrsave_mode_check(wlc_ap_info_t *ap, int type)
{
	wlc_ap_info_pvt_t* appvt = (wlc_ap_info_pvt_t*) ap;
	wlc_info_t *wlc = appvt->wlc;
	wlc_pwrsave_t *pwrsave = NULL;
	uint pkts_per_second, total_pktcount;

	switch (type) {
#ifdef RXCHAIN_PWRSAVE
		case PWRSAVE_RXCHAIN:
			pwrsave = &appvt->rxchain_pwrsave.pwrsave;
			/* Exit power save mode if channel is quiet */
			if (wlc_quiet_chanspec(wlc->cmi, WLC_BAND_PI_RADIO_CHANSPEC)) {
				wlc_reset_rxchain_pwrsave_mode(ap);
				return;
			}
			break;
#endif // endif
#ifdef RADIO_PWRSAVE
		case PWRSAVE_RADIO:
			pwrsave = &appvt->radio_pwrsave.pwrsave;
			break;
#endif // endif
		default:
			break;
	}

#ifdef RXCHAIN_PWRSAVE
	/* Enter rxchain_pwrsave mode when there is no STA associated to AP
	 * and BSS is already up
	 */
	if ((appvt->rxchain_pwrsave.pwrsave.power_save_check & NONASSOC_PWRSAVE_ENB) &&
#ifdef WDS
		!wlc_rxchain_wds_detection(wlc) &&
#endif /* WDS */
		(type == PWRSAVE_RXCHAIN) && (wlc_ap_stas_associated(ap) == 0) &&
		!pwrsave->in_power_save && wlc->cfg->up) {
		appvt->rxchain_pwrsave.rxchain = wlc->stf->rxchain;
#ifdef WL11N
		/* need to save and disable rx_stbc HT capability
		 * before enter rxchain_pwrsave mode
		 */
		appvt->rxchain_pwrsave.ht_cap_rx_stbc = wlc_stf_enter_rxchain_pwrsave(wlc);
#endif /* WL11N */
		wlc_stf_rxchain_set(wlc, 0x1, FALSE);
		pwrsave->in_power_save = TRUE;
		pwrsave->in_power_save_counter++;
		return;
	}
#endif /* RXCHAIN_PWRSAVE */

	/* Total pkt count - forwarded packets + packets the os has given + sendup packets */
	total_pktcount =  WLPWRSAVERXFVAL(wlc) + WLPWRSAVETXFVAL(wlc);

	/* Calculate the packets per second */
	pkts_per_second = total_pktcount - pwrsave->prev_pktcount;

	/* Save the current packet count for next second */
	pwrsave->prev_pktcount = total_pktcount;

	if (pkts_per_second < pwrsave->pps_threshold) {
		/* When the packets are below the threshold we just
		 * increment our timeout counter
		 */
		if (!pwrsave->in_power_save) {
			if ((pwrsave->quiet_time_counter >= pwrsave->quiet_time) &&
			    (! wlc_pwrsave_stas_associated_check(ap, type))) {
				WL_INFORM(("Entering power save mode pps is %d\n",
					pkts_per_second));
#ifdef RXCHAIN_PWRSAVE
				if (type == PWRSAVE_RXCHAIN) {
					/* Save current configured rxchains */
					appvt->rxchain_pwrsave.rxchain = wlc->stf->rxchain;
#ifdef WL11N
					/* need to save and disable rx_stbc HT capability
					 * before enter rxchain_pwrsave mode
					 */
					appvt->rxchain_pwrsave.ht_cap_rx_stbc =
						wlc_stf_enter_rxchain_pwrsave(wlc);
#endif /* WL11N */
					wlc_stf_rxchain_set(wlc, 0x1, TRUE);
				}
#endif /* RXCHAIN_PWRSAVE */
				pwrsave->in_power_save = TRUE;
				pwrsave->in_power_save_counter++;
				return;
			}
		}
		pwrsave->quiet_time_counter++;
	} else {
		/* If we are already in the wait mode counting
		 * up then just reset the counter since
		 * packets have gone above threshold
		 */
		pwrsave->quiet_time_counter = 0;
		WL_INFORM(("Resetting quiet time\n"));
		if (pwrsave->in_power_save) {
			if (type == PWRSAVE_RXCHAIN) {
#ifdef RXCHAIN_PWRSAVE
				wlc_stf_rxchain_set(wlc, appvt->rxchain_pwrsave.rxchain, TRUE);
#ifdef WL11N
				/* need to restore rx_stbc HT capability
				 * after exit rxchain_pwrsave mode
				 */
				wlc_stf_exit_rxchain_pwrsave(wlc,
					appvt->rxchain_pwrsave.ht_cap_rx_stbc);
#endif /* WL11N */
#endif /* RXCHAIN_PWRSAVE */
			} else if (type == PWRSAVE_RADIO) {
#ifdef RADIO_PWRSAVE
				wl_del_timer(wlc->wl, appvt->radio_pwrsave.timer);
				if (appvt->radio_pwrsave.radio_disabled) {
					wlc_bmac_radio_hw(wlc->hw, TRUE,
						(CHIPID(wlc->pub->sih->chip) == BCM5356_CHIP_ID) ?
						TRUE : FALSE);
					appvt->radio_pwrsave.radio_disabled = FALSE;
				}
				appvt->radio_pwrsave.pwrsave_state = 0;
				appvt->radio_pwrsave.cncl_bcn = FALSE;
#endif // endif
			}
			WL_INFORM(("Exiting power save mode pps is %d\n", pkts_per_second));
			pwrsave->in_power_save = FALSE;
		}
	}
}
#endif /* RXCHAIN_PWRSAVE || RADIO_PWRSAVE */

#ifdef RADIO_PWRSAVE

/*
 * Routine that enables/disables the radio for the duty cycle
 */
static void
wlc_radio_pwrsave_timer(void *arg)
{
	wlc_ap_info_t *ap = (wlc_ap_info_t*)arg;
	wlc_ap_info_pvt_t* appvt = (wlc_ap_info_pvt_t*) ap;
	wlc_info_t *wlc = appvt->wlc;

	if (appvt->radio_pwrsave.radio_disabled) {
		WL_INFORM(("wl power timer OFF period end. enabling radio\n"));

		if (appvt->radio_pwrsave.level) {
			appvt->radio_pwrsave.cncl_bcn = TRUE;
		}

		wlc_bmac_radio_hw(wlc->hw, TRUE,
			(CHIPID(wlc->pub->sih->chip) == BCM5356_CHIP_ID) ?  TRUE : FALSE);
		appvt->radio_pwrsave.radio_disabled = FALSE;
		/* Re-enter power save state */
		appvt->radio_pwrsave.pwrsave_state = 2;
	} else {
		WL_INFORM(("wl power timer OFF period starts. disabling radio\n"));
		wlc_radio_pwrsave_off_time_start(ap);
		wlc_bmac_radio_hw(wlc->hw, FALSE,
			(CHIPID(wlc->pub->sih->chip) == BCM5356_CHIP_ID) ?  TRUE : FALSE);
		appvt->radio_pwrsave.radio_disabled = TRUE;
	}
}

/*
 * Start the on time of the beacon interval
 */
void
wlc_radio_pwrsave_on_time_start(wlc_ap_info_t *ap, bool dtim)
{
	wlc_ap_info_pvt_t* appvt = (wlc_ap_info_pvt_t*) ap;
	wlc_info_t *wlc = appvt->wlc;
	uint32 on_time, state;

	/* Update quite ie only the first time we enter power save mode.
	 * If we are entering power save for the first time set the count
	 * such that quiet time starts after 3 bcn intervals.
	 *
	 * state 0 - initial state / just exited pwr save
	 * state 1 - entering pwr save - sent quiet ie with count n
	 * state 2 - re-entering pwr save - sent quiet ie with count 1
	 * state 3 - in pwr save - radio on/off
	 *
	 * ---> 0 ----> 1 ------> 3
	 *      ^       |    ^    |
	 *      |       |    |    |
	 *      |       v    |    v
	 *      +<---------- 2 <--+
	 *      |                 |
	 *      |                 |
	 *      |                 |
	 *      +<----------------+
	 */
	state = appvt->radio_pwrsave.pwrsave_state;
	if (state == 0) {
		wlc_radio_pwrsave_update_quiet_ie(ap, 3);
		/* Enter power save state */
		appvt->radio_pwrsave.tbtt_skip = 3;
		appvt->radio_pwrsave.pwrsave_state = 1;
	}

	/* We are going to start radio on/off only after counting
	 * down tbtt_skip bcn intervals.
	 */
	if (appvt->radio_pwrsave.tbtt_skip-- > 0) {
		WL_INFORM(("wl%d: tbtt skip %d\n", wlc->pub->unit,
		           appvt->radio_pwrsave.tbtt_skip));
		return;
	}

	if (!dtim)
		return;

	if (appvt->radio_pwrsave.level) {
		appvt->radio_pwrsave.cncl_bcn = FALSE;
	}

	/* Schedule the timer to turn off the radio after on_time msec */
	on_time = appvt->radio_pwrsave.on_time * DOT11_TU_TO_US;
	on_time += ap->pre_tbtt_us;

	appvt->radio_pwrsave.tbtt_skip = ((on_time / DOT11_TU_TO_US) /
	                                          wlc->cfg->current_bss->beacon_period);
	on_time /= 1000;
	/* acc for extra phy and mac suspend delays, it seems to be huge. Need
	 * to extract out the exact delays.
	 */
	on_time += RADIO_PWRSAVE_TIMER_LATENCY;

	WL_INFORM(("wl%d: adding timer to disable phy after %d ms state %d, skip %d\n",
	           wlc->pub->unit, on_time, appvt->radio_pwrsave.pwrsave_state,
	           appvt->radio_pwrsave.tbtt_skip));

	/* In case pre-tbtt intr arrived before the timer that disables the radio */
	wl_del_timer(wlc->wl, appvt->radio_pwrsave.timer);

	wl_add_timer(wlc->wl, appvt->radio_pwrsave.timer, on_time, FALSE);

	/* Update bcn and probe resp to send quiet ie starting from next
	 * tbtt intr.
	 */
	wlc_radio_pwrsave_update_quiet_ie(ap, appvt->radio_pwrsave.tbtt_skip);
}

/*
 * Start the off time of the beacon interval
 */
static void
wlc_radio_pwrsave_off_time_start(wlc_ap_info_t *ap)
{
	wlc_ap_info_pvt_t* appvt = (wlc_ap_info_pvt_t*) ap;
	wlc_info_t *wlc = appvt->wlc;
	uint32 off_time;

	/* Calcuate the delay after which to schedule timer to turn on
	 * the radio. Also take in to account the phy enabling latency.
	 * We have to make sure the phy is enabled by the next pre-tbtt
	 * interrupt time.
	 */
	off_time = appvt->radio_pwrsave.off_time;

	off_time *= DOT11_TU_TO_US;
	off_time -= (ap->pre_tbtt_us + PHY_DISABLE_MAX_LATENCY_us);

	/* In power save state */
	appvt->radio_pwrsave.pwrsave_state = 3;
	off_time /= 1000;

	/* acc for extra phy and mac suspend delays, it seems to be huge. Need
	 * to extract out the exact delays.
	 */
	off_time -= RADIO_PWRSAVE_TIMER_LATENCY;

	WL_INFORM(("wl%d: add timer to enable phy after %d msec state %d\n",
	           wlc->pub->unit, off_time, appvt->radio_pwrsave.pwrsave_state));

	/* Schedule the timer to turn on the radio after off_time msec */
	wl_add_timer(wlc->wl, appvt->radio_pwrsave.timer, off_time, FALSE);
}

/*
 * Check whether we are in radio power save mode
 */
int
wlc_radio_pwrsave_in_power_save(wlc_ap_info_t *ap)
{
	wlc_ap_info_pvt_t* appvt = (wlc_ap_info_pvt_t*) ap;

	return (appvt->radio_pwrsave.pwrsave.in_power_save);
}

/*
 * Enter radio power save
 */
void
wlc_radio_pwrsave_enter_mode(wlc_info_t *wlc, bool dtim)
{
	/* If AP is in radio power save mode, we need to start the duty
	 * cycle with TBTT
	 */
	if (AP_ENAB(wlc->pub) && RADIO_PWRSAVE_ENAB(wlc->ap) &&
	    wlc_radio_pwrsave_in_power_save(wlc->ap)) {
		wlc_radio_pwrsave_on_time_start(wlc->ap, dtim);
	}
}

/*
 * Exit out of the radio power save if we are in it
 */
void
wlc_radio_pwrsave_exit_mode(wlc_ap_info_t *ap)
{
	wlc_ap_info_pvt_t* appvt = (wlc_ap_info_pvt_t*) ap;
	wlc_info_t *wlc = appvt->wlc;

	appvt->radio_pwrsave.pwrsave.quiet_time_counter = 0;
	appvt->radio_pwrsave.pwrsave.in_power_save = FALSE;
	wl_del_timer(wlc->wl, appvt->radio_pwrsave.timer);
	if (appvt->radio_pwrsave.radio_disabled) {
		wlc_bmac_radio_hw(wlc->hw, TRUE,
			(CHIPID(wlc->pub->sih->chip) == BCM5356_CHIP_ID) ?  TRUE : FALSE);
		appvt->radio_pwrsave.radio_disabled = FALSE;
	}
	appvt->radio_pwrsave.pwrsave_state = 0;
	appvt->radio_pwrsave.cncl_bcn = FALSE;
}

/*
 * Update the beacon with quiet IE
 */
static void
wlc_radio_pwrsave_update_quiet_ie(wlc_ap_info_t *ap, uint8 count)
{
#ifdef WL11H
	wlc_ap_info_pvt_t* appvt = (wlc_ap_info_pvt_t*) ap;
	wlc_info_t *wlc = appvt->wlc;
	uint32 duration;
	wlc_bsscfg_t *cfg = wlc->cfg;
	dot11_quiet_t quiet_cmd;

	if (WL11H_ENAB(wlc))
		return;

	duration = appvt->radio_pwrsave.off_time;
	duration *= DOT11_TU_TO_US;
	duration -= (ap->pre_tbtt_us + PHY_ENABLE_MAX_LATENCY_us +
	             PHY_DISABLE_MAX_LATENCY_us);
	duration /= DOT11_TU_TO_US;

	/* Setup the quiet command */
	quiet_cmd.period = 0;
	quiet_cmd.count = count;
	quiet_cmd.duration = (uint16)duration;
	quiet_cmd.offset = appvt->radio_pwrsave.on_time % cfg->current_bss->beacon_period;
	WL_INFORM(("wl%d: quiet cmd: count %d, dur %d, offset %d\n",
	            wlc->pub->unit, quiet_cmd.count,
	            quiet_cmd.duration, quiet_cmd.offset));

	wlc_quiet_do_quiet(wlc->quiet, cfg, &quiet_cmd);
#endif /* WL11H */
}
#endif /* RADIO_PWRSAVE */

/* ======================= IE mgmt routines ===================== */
/* ============================================================== */
/* Supported Rates IE */
static uint
wlc_assoc_calc_sup_rates_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	wlc_iem_ft_cbparm_t *ftcbparm = data->cbparm->ft;

	return TLV_HDR_LEN + ftcbparm->assocresp.sup->count;
}

static int
wlc_assoc_write_sup_rates_ie(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_iem_ft_cbparm_t *ftcbparm = data->cbparm->ft;

	bcm_write_tlv(DOT11_MNG_RATES_ID, ftcbparm->assocresp.sup->rates,
		ftcbparm->assocresp.sup->count, data->buf);

	return BCME_OK;
}

/* Extended Supported Rates IE */
static uint
wlc_assoc_calc_ext_rates_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	wlc_iem_ft_cbparm_t *ftcbparm = data->cbparm->ft;

	if (ftcbparm->assocresp.ext->count == 0) {
		return 0;
	}

	return TLV_HDR_LEN + ftcbparm->assocresp.ext->count;
}

static int
wlc_assoc_write_ext_rates_ie(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_iem_ft_cbparm_t *ftcbparm = data->cbparm->ft;

	bcm_write_tlv(DOT11_MNG_EXT_RATES_ID, ftcbparm->assocresp.ext->rates,
		ftcbparm->assocresp.ext->count, data->buf);

	return BCME_OK;
}

/* SSID */
static int
wlc_assoc_parse_ssid_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;
	wlc_bsscfg_t *cfg = data->cfg;
	wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
		char eabuf[ETHER_ADDR_STR_LEN];
#endif // endif

	/* special case handling where some IoT devices send reassoc req with empty ssid. */
	if (data->ie_len == TLV_BODY_OFF) {
		WL_ASSOC(("wl%d: %s association with empty SSID\n",
		          wlc->pub->unit, bcm_ether_ntoa(&ftpparm->assocreq.scb->ea, eabuf)));
		ftpparm->assocreq.status = DOT11_SC_SUCCESS;
		return BCME_OK;
	}

	if (data->ie == NULL || data->ie_len < TLV_BODY_OFF) {
		WL_ASSOC(("wl%d: %s attempted association with no SSID\n",
		          wlc->pub->unit, bcm_ether_ntoa(&ftpparm->assocreq.scb->ea, eabuf)));
		ftpparm->assocreq.status = DOT11_SC_FAILURE;
		return BCME_ERROR;
	}

	/* failure if the SSID does not match any active AP config */
	if (!WLC_IS_MATCH_SSID(wlc, cfg->SSID, &data->ie[TLV_BODY_OFF],
	                       cfg->SSID_len, data->ie[TLV_LEN_OFF])) {
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
		char ssidbuf[SSID_FMT_BUF_LEN];

		wlc_format_ssid(ssidbuf, &data->ie[TLV_BODY_OFF], data->ie[TLV_LEN_OFF]);
		WL_ASSOC(("wl%d: %s attempted association with incorrect SSID ID\"%s\"\n",
		          wlc->pub->unit, bcm_ether_ntoa(&ftpparm->assocreq.scb->ea, eabuf),
		          ssidbuf));
#endif // endif
		ftpparm->assocreq.status = DOT11_SC_ASSOC_FAIL;
		return BCME_ERROR;
	}

	ftpparm->assocreq.status = DOT11_SC_SUCCESS;
	return BCME_OK;
}

static int
wlc_assoc_parse_sup_rates_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;
	wlc_rateset_t *sup = ftpparm->assocreq.sup;

	bzero(sup, sizeof(*sup));

	if (data->ie == NULL || data->ie_len <= TLV_BODY_OFF) {
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
		char eabuf[ETHER_ADDR_STR_LEN];
		struct scb *scb = ftpparm->assocreq.scb;
		wlc_info_t *wlc = (wlc_info_t *)ctx;

		WL_ASSOC(("wl%d: %s attempted association with no Supported Rates IE\n",
		          wlc->pub->unit, bcm_ether_ntoa(&scb->ea, eabuf)));
#endif // endif
		ftpparm->assocreq.status = DOT11_SC_ASSOC_RATE_MISMATCH;
		return BCME_ERROR;
	}

#ifdef BCMDBG
	if (data->ie[TLV_LEN_OFF] > WLC_NUMRATES) {
		wlc_info_t *wlc = (wlc_info_t *)ctx;

		WL_ERROR(("wl%d: %s: IE contains too many rates, truncate\n",
		          wlc->pub->unit, __FUNCTION__));
	}
#endif // endif

	sup->count = MIN(data->ie[TLV_LEN_OFF], WLC_NUMRATES);
	bcopy(&data->ie[TLV_BODY_OFF], sup->rates, sup->count);

	ftpparm->assocreq.status = DOT11_SC_SUCCESS;
	return BCME_OK;
}

static int
wlc_assoc_parse_ext_rates_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;
	wlc_rateset_t *ext = ftpparm->assocreq.ext;

	bzero(ext, sizeof(*ext));

	if (data->ie == NULL || data->ie_len <= TLV_BODY_OFF) {
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
		char eabuf[ETHER_ADDR_STR_LEN];
		struct scb *scb = ftpparm->assocreq.scb;
		wlc_info_t *wlc = (wlc_info_t *)ctx;

		WL_ASSOC(("wl%d: %s attempted association with no Extended Supported Rates IE\n",
		          wlc->pub->unit, bcm_ether_ntoa(&scb->ea, eabuf)));
#endif // endif
		return BCME_OK;
	}

#ifdef BCMDBG
	if (data->ie[TLV_LEN_OFF] > WLC_NUMRATES) {
		wlc_info_t *wlc = (wlc_info_t *)ctx;

		WL_ERROR(("wl%d: %s: IE contains too many rates, truncate\n",
		          wlc->pub->unit, __FUNCTION__));
	}
#endif // endif

	ext->count = MIN(data->ie[TLV_LEN_OFF], WLC_NUMRATES);
	bcopy(&data->ie[TLV_BODY_OFF], ext->rates, ext->count);

	ftpparm->assocreq.status = DOT11_SC_SUCCESS;
	return BCME_OK;
}

#ifndef BCM_STA_SUPP_CHAN_ROM_COMPAT
static int
wlc_assoc_parse_supp_chan_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;
	wlc_supp_channels_t *supp_chan = ftpparm->assocreq.supp_chan;
	wlc_supp_chan_set_t *chanset = NULL;
	uint8 *iedata = NULL;
	uint8 datalen;
	int count;
	int i = 0, j = 0;
	uint8 channel, chan_sep;

	bzero(supp_chan, sizeof(*supp_chan));

	if (data->ie == NULL || data->ie_len <= TLV_BODY_OFF) {
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
		char eabuf[ETHER_ADDR_STR_LEN];
		struct scb *scb = ftpparm->assocreq.scb;
		wlc_info_t *wlc = (wlc_info_t *)ctx;

		WL_ASSOC(("wl%d: %s attempted association with no Supported Channels IE\n",
		          wlc->pub->unit, bcm_ether_ntoa(&scb->ea, eabuf)));
#endif /* BCMDBG || WLMSG_ASSOC */
		return BCME_OK;
	}

#ifdef BCMDBG
	if (data->ie[TLV_LEN_OFF] > (WL_NUMCHANNELS * sizeof(wlc_supp_chan_set_t))) {
		wlc_info_t *wlc = (wlc_info_t *)ctx;

		WL_ERROR(("wl%d: %s: IE contains too many channels, truncate\n",
		          wlc->pub->unit, __FUNCTION__));
	}
#endif /* BCMDBG */

	datalen = MIN(data->ie[TLV_LEN_OFF], (WL_NUMCHANNELS * sizeof(wlc_supp_chan_set_t)));
	count = datalen / sizeof(wlc_supp_chan_set_t);
	iedata = &data->ie[TLV_BODY_OFF];

	for (i = 0; i < count; i++) {
		chanset = (wlc_supp_chan_set_t *)iedata;
		supp_chan->count += chanset->num_channels;

		channel = chanset->first_channel;
		chan_sep = (channel > CH_MAX_2G_CHANNEL) ?
			CH_20MHZ_APART : CH_5MHZ_APART;

		for (j = 0; j < chanset->num_channels; j++) {
			if (CH_NUM_VALID_RANGE(channel)) {
				setbit((void *)supp_chan->chanset, channel);
			} else {
				break;
			}
			channel += chan_sep;
		}
		iedata += sizeof(wlc_supp_chan_set_t);
	}

	ftpparm->assocreq.status = DOT11_SC_SUCCESS;
	return BCME_OK;
}
#endif /* BCM_STA_SUPP_CHAN_ROM_COMPAT */

static int
wlc_assoc_parse_wps_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;
	wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;
	wlc_bsscfg_t *cfg = data->cfg;

	if (WSEC_SES_OW_ENABLED(cfg->wsec)) {
		bcm_tlv_t *wpsie = (bcm_tlv_t *)data->ie;
		struct scb *scb = ftpparm->assocreq.scb;

		if (wpsie == NULL)
			return BCME_OK;

		if (wpsie->len < 5) {
#if defined(BCMDBG) || defined(BCMDBG_ERR)
			char eabuf[ETHER_ADDR_STR_LEN];
#endif // endif
			WL_ERROR(("wl%d: wlc_assocresp: unsupported request in WPS IE from %s\n",
			          wlc->pub->unit, bcm_ether_ntoa(&scb->ea, eabuf)));
			ftpparm->assocreq.status = DOT11_SC_ASSOC_FAIL;
			return BCME_ERROR;
		}

		ftpparm->assocreq.wps_ie = (uint8 *)wpsie;

		return wlc_scb_save_wpa_ie(wlc, scb, wpsie);
	}

	return BCME_OK;
}

/* ============================================================== */
/* ======================= IE mgmt routines ===================== */

#ifdef PSPRETEND
bool
wlc_ap_do_pspretend_probe(wlc_info_t *wlc, struct scb *scb, uint32 elapsed_time)
{
#ifdef DONGLEBUILD
	void *pkt;
#endif /* DONGLEBUILD */
	uint8 rate_override;
	uint32 listen_in_ms = scb->listen * wlc->default_bss->beacon_period;

	/* this increments each time we are called from the pretend probe timer,
	 * approximately once per 10ms
	 */
	scb->ps_pretend_probe++;

	/* If a probe is still pending, don't send another one */
	if (scb->flags & SCB_PENDING_PROBE) {
		return FALSE;
	}

	/* As time goes by, we probe less often - this function is called
	 * once per timer interval (10ms) per SCB in pps state, so
	 * we return early if we decide to skip
	 */
	if ((elapsed_time >= listen_in_ms) &&
#ifdef BCMDBG
	        !(wlc->block_datafifo & DATA_BLOCK_PS)) {
#else
	        TRUE) {
#endif // endif
		/* there's no more point to sending probes, the
		 * destination has probably died. Note that the TIM
		 * is set indicating ps state, so after pretend probing
		 * has completed, there is still the beacon to awaken the STA
		 */
		scb->ps_pretend &= ~PS_PRETEND_PROBING;
		if (scb->grace_attempts) {
			/* reset grace_attempts but indicate we have done some probing */
			scb->grace_attempts = 1;
		}
		return FALSE;
	}
	else if (wlc_scb_restrict_do_probe(scb)) {
		/* probing requested  */
	}
	else if (elapsed_time > 600) {
		/* after 0.6s, only probe every 5th timer, approx 50 ms */
		if ((scb->ps_pretend_probe % 5) != 0) {
			return FALSE;
		}
	}
	else if (elapsed_time > 300) {
		/* after 0.3s, only probe every 3rd timer, approx 30ms */
		if ((scb->ps_pretend_probe % 3) != 0) {
			return FALSE;
		}
	}
	else if (elapsed_time > 150) {
		/* after 0.15s, only probe every 2nd timer, approx 20ms */
		if ((scb->ps_pretend_probe % 2) != 0) {
			return FALSE;
		}
	}
	/* else we probe each time (10ms) */

	/* If there is fifo drain in process, we do not send the probe. Partly because
	 * sending a probe might interfere with the fifo drain, but mainly because
	 * even if the data probe was successful, we cannot exit ps pretend state
	 * whilst the fifo drain was still happening. So, waiting for the fifo drain
	 * to finish first is clean, predictable and consistent.
	 * Note that when the DATA_BLOCK_PS condition clears, a probe is sent regardless.
	 */
	if (wlc->block_datafifo & DATA_BLOCK_PS) {
		WL_PS(("wl%d.%d: %s: "MACF" DATA_BLOCK_PS pending %d pkts "
			   "time since pps %dms\n",
			   wlc->pub->unit, WLC_BSSCFG_IDX(SCB_BSSCFG(scb)), __FUNCTION__,
			   ETHER_TO_MACF(scb->ea), TXPKTPENDTOT(wlc), elapsed_time));
		return FALSE;
	}

	/* use the lowest basic rate */
	rate_override = wlc_lowest_basicrate_get(scb->bsscfg);
	ASSERT(VALID_RATE(wlc, rate_override));

	WL_PS(("wl%d.%d: pps probe to "MACF" time since pps is %dms\n", wlc->pub->unit,
	        WLC_BSSCFG_IDX(SCB_BSSCFG(scb)), ETHER_TO_MACF(scb->ea), elapsed_time));

#ifdef DONGLEBUILD
	pkt = wlc_sendnulldata(wlc, scb->bsscfg, &scb->ea, rate_override,
	                       WLF_PSDONTQ, PRIO_8021D_VO);

	if (pkt == NULL) {
#else
	if (!wlc_sendnulldata_ex(wlc, scb->bsscfg, &scb->ea, rate_override,
		WLF_PSDONTQ, PRIO_8021D_VO, wlc_ap_sendnulldata_cb, NULL))
	{
#endif /* DONGLEBUILD */
		WL_ERROR(("wl%d.%d: %s: wlc_sendnulldata failed\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(SCB_BSSCFG(scb)), __FUNCTION__));
		return FALSE;
	}

#ifdef DONGLEBUILD
	/* register packet callback */
	WLF2_PCB1_REG(pkt, WLF2_PCB1_STA_PRB);
#endif /* DONGLEBUILD */
	scb->flags |= SCB_PENDING_PROBE;

	return TRUE;
}

void
wlc_ap_pspretend_probe(void *arg)
{
	wlc_info_t *wlc = (wlc_info_t*)arg;
	wlc_pps_info_t* pps_info;

	ASSERT(wlc);

	pps_info = wlc->pps_info;
	pps_info->is_ps_pretend_probe_timer_running = FALSE;

	/* double check in case driver went down since timer was scheduled */
	if (wlc->pub->up && !wlc->going_down) {
		struct scb_iter scbiter;
		struct scb *scb;
		bool        ps_pretend_probe_continue = FALSE;
		uint32      tsf_low = R_REG(wlc->osh, &wlc->regs->tsf_timerlow);

		FOREACHSCB(wlc->scbstate, &scbiter, scb) {
			if (SCB_PS_PRETEND_PROBING(scb)) {
				uint32 pretend_elapsed = (tsf_low - scb->ps_pretend_start)/1000;
				ps_pretend_probe_continue = TRUE;
				wlc_ap_do_pspretend_probe(wlc, scb, pretend_elapsed);
			}
			else {
				scb->ps_pretend_probe = 0;
			}
		}

		/* retstart timer as long as at least one scb is in pretend state */
		if (ps_pretend_probe_continue) {
			/* 10 milliseconds */
			wl_add_timer(wlc->wl, pps_info->ps_pretend_probe_timer,
			             10, FALSE);
			pps_info->is_ps_pretend_probe_timer_running = TRUE;
		}
	}
}
#endif /* PSPRETEND */

ratespec_t
wlc_force_bcn_rspec(wlc_info_t *wlc)
{
	wlc_ap_info_pvt_t* appvt = (wlc_ap_info_pvt_t*)wlc->ap;
	return appvt->force_bcn_rspec;
}

static int
wlc_force_bcn_rspec_upd(wlc_info_t *wlc, wlc_bsscfg_t *cfg, wlc_ap_info_t *ap, ratespec_t rspec)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;

	/* new bcn rspec requested */
	if (rspec) {
		uint i;
		/* check rspec is valid */
		if (!wlc_valid_rate(wlc, rspec, CHSPEC_IS2G(cfg->current_bss->chanspec) ?
		    WLC_BAND_2G : WLC_BAND_5G, TRUE))
			return BCME_BADRATESET;

		/* check rspec is basic rate */
		for (i = 0; i < cfg->current_bss->rateset.count; i++) {
			if ((cfg->current_bss->rateset.rates[i] & WLC_RATE_FLAG) &&
			    (cfg->current_bss->rateset.rates[i] & RATE_MASK) == rspec) {
#ifdef WL11N
				if (IS_OFDM(rspec)) {
					if (WLCISNPHY(wlc->band) &&
					    wlc->stf->ss_opmode == PHY_TXC1_MODE_CDD) {
						rspec |= (1 << RSPEC_TXEXP_SHIFT);
					}
					if (WLCISHTPHY(wlc->band) ||
					    WLCISACPHY(wlc->band)) {
						uint ntx = wlc_stf_txchain_get(wlc, rspec);
						uint nss = wlc_ratespec_nss(rspec);
						rspec |= ((ntx - nss) << RSPEC_TXEXP_SHIFT);
					}
				}
#endif /* WL11N */
				appvt->force_bcn_rspec = rspec;
				break;
			}
		}
		if (i == cfg->current_bss->rateset.count)
			return BCME_UNSUPPORTED;
	}
	else
		appvt->force_bcn_rspec = (ratespec_t) 0;

	/* update beacon rate */
	wlc_update_beacon(wlc);

	return BCME_OK;
}

#ifdef USBAP
bool
wlc_wlancoex_on(wlc_info_t *wlc)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) wlc->ap;
	bool wlancoex_on = FALSE;

	if (appvt) {
		wlancoex_on = appvt->wlancoex;
	}

	return wlancoex_on;
}

static int
wlc_wlancoex_upd(wlc_info_t *wlc, wlc_ap_info_t *ap, bool coex_en)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) wlc->ap;
	char *var;
	int i, err = BCME_OK;
	uint16 coexcfg = 0;
	uint16 coexgpio = 0;
	wlc_bsscfg_t *cfg;

#ifdef WLC_HIGH_ONLY
	/* wlancoex slave shmem init */
	if ((var = getvar(wlc->pub->vars, "wlancoex_s")) != NULL) {
		if (coex_en) {
			coexcfg = M_WLCX_CONFIG_EN;
			coexgpio = (1 << bcm_strtoul(var, NULL, 0));
		}
	}
#else
	/* wlancoex master shmem init */
	if ((var = getvar(wlc->pub->vars, "wlancoex_m")) != NULL) {
		if (coex_en) {
			coexcfg = M_WLCX_CONFIG_EN | M_WLCX_CONFIG_MASTER;
			coexgpio = (1 << bcm_strtoul(var, NULL, 0));
		}
	}
#endif /* WLC_HIGH_ONLY */
	else {
		WL_ERROR(("wl%d: wlancoex param error. wlancoex_m:%s wlancoex_s:%s\n",
			wlc->pub->unit,
			getvar(wlc->pub->vars, "wlancoex_m"),
			getvar(wlc->pub->vars, "wlancoex_s")));
		return BCME_BADARG;

	}

	/* setup wlancoex driver flag */
	appvt->wlancoex = coex_en;
	/* setup wlancoex ucode mode flag */
	wlc_write_shm(wlc, M_WLCX_CONFIG, coexcfg);
	/* setup wlancoex ucode gpio value */
	wlc_write_shm(wlc, M_WLCX_GPIO_CONFIG, coexgpio);

	FOREACH_BSS(wlc, i, cfg) {
		if (BSSCFG_STA(cfg)) {
			uint pm = coex_en? PM_MAX: PM_OFF;
			err = wlc_ioctl(wlc, WLC_SET_PM, (void *)&pm, sizeof(pm), cfg->wlcif);

			if (err) {
				WL_ERROR(("wl%d: wlancoex set PM error %s\n",
					wlc->pub->unit,
					wl_ifname(wlc->wl, cfg->wlcif->wlif)));
				break;
			}
		}
	}

	return err;
}
#endif /* USBAP */
int
wlc_ap_sendauth(wlc_ap_info_t *ap, wlc_bsscfg_t *bsscfg,
	struct scb *scb, int auth_alg,
	int auth_seq, int status, uint8 *challenge_text,
	bool short_preamble, bool send_auth)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;
	wlc_info_t *wlc = appvt->wlc;
#if defined(BCMDBG) || defined(BCMDBG_ERR) || defined(WLMSG_ASSOC) || \
	defined(WLMSG_BTA)

	char eabuf[ETHER_ADDR_STR_LEN], *sa;
	sa = bcm_ether_ntoa(&scb->ea, eabuf);
	/* send authentication response */
	WL_ASSOC(("wl%d.%d %s: %s authenticated\n", wlc->pub->unit,
		WLC_BSSCFG_IDX(bsscfg), __FUNCTION__, sa));
	printf("wl%d.%d %s: %s authenticated\n", wlc->pub->unit,
		WLC_BSSCFG_IDX(bsscfg), __FUNCTION__, sa);
#endif // endif
	if ((status == DOT11_SC_SUCCESS) && scb && SCB_AUTHENTICATED(scb)) {
		wlc_bss_mac_event(wlc, bsscfg, WLC_E_AUTH_IND, &scb->ea,
			WLC_E_STATUS_SUCCESS, DOT11_SC_SUCCESS, auth_alg, 0, 0);

#ifdef RXCHAIN_PWRSAVE
		/* fast switch back from rxchain_pwrsave state upon authentication */
		wlc_reset_rxchain_pwrsave_mode(ap);
#endif /* RXCHAIN_PWRSAVE */
	}

	if (send_auth)
		wlc_sendauth(bsscfg, &scb->ea, &bsscfg->BSSID, scb,
			auth_alg, auth_seq, status, NULL, NULL, short_preamble);

	return BCME_OK;
}

#ifdef WL_GLOBAL_RCLASS
bool
wlc_ap_scb_support_global_rclass(wlc_info_t *wlc, struct scb *scb)
{
	if (!scb) {
		return FALSE;
	}
	return SCB_SUPPORTS_GLOBAL_RCLASS(scb);
}
static int
wlc_assoc_parse_regclass_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_info_t *wlc = (wlc_info_t*)ctx;
	wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;
	struct scb *scb =  ftpparm->assocreq.scb;
	uint8 *cp = NULL;

	cp = data->ie;

	switch (data->ft) {
		case FC_ASSOC_REQ:
		case FC_REASSOC_REQ:
			{
				bcm_tlv_t* ie;
				uint8 i;
				uint8 *setBitBuff = NULL;
				uint8 scb_cap_flag[1] = {0};

				ie = bcm_parse_tlvs(cp, data->ie_len, DOT11_MNG_REGCLASS_ID);
				if ((!ie) ||(ie->len == 0) || (ie->len > MAXRCLISTSIZE)) {
					return BCME_OK;
				}
#ifdef BCMDBG
				prhex(" supported class data:\n", ie->data, ie->len);
#endif /* BCMDBG */
				/* Max possible opearting class is 255, means
				 * 32 index uint8 array buffer to set/reset/check
				 * bit case
				 */
				setBitBuff = MALLOCZ(wlc->osh, 32);

				if (!setBitBuff) {
					/* return with BCME_NOMEM gives assert in
					 * routine wlc_ap_process_assocreq.
					 * ASSERT(status != DOT11_SC_SUCCESS) if
					 * ret is not BCME_OK.
					 * Reasonably not hit assert due to malloc
					 * failure.
					 */
					return BCME_NOMEM;
				}
				cp = ie->data;
				cp++;

				for (i = 0; i < ie->len; i++) {
					setbit(setBitBuff, *cp);
					cp++;
				}
				/* Check global support from list of operating class values,
				 * these list of supported operating class holds operating band
				 * capability. Update scb band capability.
				 */
				if (wlc_sta_supports_global_rclass(wlc, setBitBuff,
					&scb_cap_flag[0])) {
					scb->flags3 |= SCB3_GLOBAL_RCLASS_CAP;
				}
				MFREE(wlc->osh, setBitBuff, 32);
			}
		break;
	}

	return BCME_OK;
}

static void
wlc_ap_scb_state_upd_cb(void *ctx, scb_state_upd_data_t *notif_data)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;
	struct scb *scb;
	uint8 oldstate;
	uint8 assoc_state_change = 0x00;

	ASSERT(notif_data != NULL);

	scb = notif_data->scb;
	ASSERT(scb != NULL);

	if (!BSSCFG_AP(scb->bsscfg) || SCB_SUPPORTS_GLOBAL_RCLASS(scb)) {
		WL_INFORM(("wl%d: scb["MACF"] support gbl rclass [%d] no need to process,"
			"total sta count not support global rclass[%d] return\n",
			wlc->pub->unit, ETHERP_TO_MACF(&scb->ea),
			SCB_SUPPORTS_GLOBAL_RCLASS(scb),
			scb->bsscfg->scb_without_gbl_rclass));
		return;
	}

	oldstate = notif_data->oldstate;

	WL_INFORM(("wl%d: scb["MACF"] state[%x] oldstate[%x] support_gbl_rclass[%d],"
		"bsscfg->scb_without_gbl_rclass[%d]\n", wlc->pub->unit,
		ETHERP_TO_MACF(&scb->ea), scb->state,
		oldstate, SCB_SUPPORTS_GLOBAL_RCLASS(scb),
		scb->bsscfg->scb_without_gbl_rclass));

	assoc_state_change = ((oldstate & ASSOCIATED) ^ SCB_ASSOCIATED(scb));

	if (!assoc_state_change) {
		WL_INFORM(("wl%d: No change in assoc state of scb["MACF"], return\n",
			wlc->pub->unit, ETHERP_TO_MACF(&scb->ea)));

		return;
	}

	if (SCB_ASSOCIATED(scb)) {
		scb->bsscfg->scb_without_gbl_rclass++;
		WL_INFORM((" wl%d: scb["MACF"] assoc, bsscfg->scb_without_gbl_rclass[%d]\n",
			wlc->pub->unit, ETHERP_TO_MACF(&scb->ea),
			scb->bsscfg->scb_without_gbl_rclass));
	} else {
		if (scb->bsscfg->scb_without_gbl_rclass) {
			scb->bsscfg->scb_without_gbl_rclass--;
		} else {
			WL_INFORM(("Unlikely situation:\n"
				"scb["MACF"] not support_gbl_rclass[%d], but "
				" bsscfg->scb_without_gbl_rclass[%d] is Zero\n",
				ETHERP_TO_MACF(&scb->ea),
				SCB_SUPPORTS_GLOBAL_RCLASS(scb),
				scb->bsscfg->scb_without_gbl_rclass));
			ASSERT(0);
		}
		WL_INFORM(("wl%d: scb["MACF"] disassoc ,bsscfg->scb_without_gbl_rclass[%d]\n",
			wlc->pub->unit, ETHERP_TO_MACF(&scb->ea),
			scb->bsscfg->scb_without_gbl_rclass));
	}

	wlc_bsscfg_update_rclass(wlc, scb->bsscfg);
}
#endif /* WL_GLOBAL_RCLASS */
