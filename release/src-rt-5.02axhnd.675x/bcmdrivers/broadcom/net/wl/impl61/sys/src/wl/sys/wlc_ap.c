/*
 * AP Module
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
 * $Id: wlc_ap.c 778114 2019-08-22 19:38:37Z $
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
#include <802.1d.h>
#include <802.11.h>
#include <802.11e.h>
#include <sbconfig.h>
#include <wlioctl.h>
#include <eapol.h>
#include <bcmwpa.h>
#include <wep.h>
#include <sbhndpio.h>
#include <sbhnddma.h>
#include <hnddma.h>
#include <bcmdevs.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_keymgmt.h>
#include <wlc_bsscfg.h>
#include <wlc_mbss.h>
#include <wlc.h>
#include <wlc_dbg.h>
#include <wlc_hw.h>
#include <wlc_apps.h>
#include <wlc_scb.h>
#include <wlc_scb_ratesel.h>
#include <wlc_phy_hal.h>
#include <phy_utils_api.h>
#include <wlc_led.h>
#include <wlc_event.h>
#include <wl_export.h>
#include <wlc_apcs.h>
#include <wlc_stf.h>
#include <wlc_ap.h>
#include <wlc_scan.h>
#include <wlc_ampdu_cmn.h>
#include <wlc_ampdu.h>
#include <wlc_amsdu.h>
#ifdef	WLCAC
#include <wlc_cac.h>
#endif // endif
#include <wlc_btcx.h>
#include <wlc_bmac.h>
#ifdef BCMAUTH_PSK
#include <wlc_auth.h>
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
#include <wlc_11h.h>
#include <wlc_tpc.h>
#include <wlc_csa.h>
#include <wlc_quiet.h>
#include <wlc_dfs.h>
#include <wlc_prot_g.h>
#include <wlc_prot_n.h>
#include <wlc_pcb.h>
#ifdef PSTA
#include <wlc_psta.h>
#endif // endif
#ifdef WL11AC
#include <wlc_vht.h>
#include <wlc_txbf.h>
#endif // endif
#include <wlc_he.h>
#ifdef WL11K
#include <wlc_rrm.h>
#endif /* WL11K */
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
#include <wlc_ie_helper.h>
#ifdef WLWNM
#include <wlc_wnm.h>
#endif // endif
#include <wlc_stamon.h>
#ifdef WDS
#include <wlc_wds.h>
#endif // endif
#include <wlc_ht.h>
#include <wlc_obss.h>
#include "wlc_txc.h"
#include <wlc_tx.h>
#ifdef WLRSDB
#include <wlc_rsdb.h>
#endif // endif
#include <wlc_macfltr.h>
#ifdef PROP_TXSTATUS
#include <wlc_ampdu.h>
#include <wlc_apps.h>
#include <wlc_wlfc.h>
#endif /* PROP_TXSTATUS */
#include <wlc_smfs.h>
#include <wlc_pspretend.h>
#include <wlc_msch.h>
#include <wlc_qoscfg.h>
#include <wlc_chanctxt.h>
#ifdef WL_MODESW
#include <wlc_modesw.h>
#endif // endif
#ifdef WL_AIR_IQ
#include <wlc_airiq.h>
#endif /* WL_AIR_IQ */
#if defined(STA)
#include <wlc_rm.h>
#endif // endif
#ifdef WL_OCE_AP
#include <wlc_oce.h>
#endif // endif
#include <wlc_rspec.h>
#include <wlc_event_utils.h>
#include <wlc_rx.h>
#include <wlc_dump.h>
#include <wlc_iocv.h>
#include <phy_chanmgr_api.h>
#include <phy_calmgr_api.h>
#include <wlc_dbg.h>
#include <wlc_rpsnoa.h>
#if defined(WL_MBO) && defined(MBO_AP)
#include <wlc_mbo.h>
#endif /* WL_MBO && MBO_AP */

/* Default pre tbtt time for non mbss case */
#define	PRE_TBTT_DEFAULT_us		2

#if defined(RXCHAIN_PWRSAVE) || defined(RADIO_PWRSAVE)

#define PWRSAVE_RXCHAIN 1
#define PWRSAVE_RADIO 2

/* Bitmap definitions for rxchain power save */
/* rxchain power save enable */
#define PWRSAVE_ENB		0x01

/**
 * Enter power save when no STA associated to AP, this flag is valid under the condition
 * PWRSAVE_ENB is on
 */
#define NONASSOC_PWRSAVE_ENB	0x02

#endif /* RXCHAIN_PWRSAVE or RADIO_PWRSAVE */

#define DFS_DEFAULT_SUBBANDS    0x00FFu
#define SCB_LONG_TIMEOUT	3600	/**< # seconds of idle time after which we proactively
					 * free an authenticated SCB
					 */
#define SCB_MARK_DEL_TIMEOUT	5	/**< # delay before freeing  marked for delete scb */
#define SCB_GRACE_ATTEMPTS	   10	/**< # attempts to probe sta beyond scb_activity_time */
#define SCB_SUPPORT_GLOBAL_RCLASS	0X01
#define WLC_BW_80_160_DELAY	10	/* Delay in seconds for 160-80 BW switch check */

#define SCB_SUPPORTS_GLOBAL_RCLASS(cubby)  (cubby->flags & SCB_SUPPORT_GLOBAL_RCLASS)
/**
 * This is a generic structure for power save implementations
 * Defines parameters for packets per second threshold based power save
 */
typedef struct wlc_pwrsave {
	bool	in_power_save;		/**< whether we are in power save mode or not */
	uint8	power_save_check;	/**< Whether power save mode check need to be done */
	uint8   stas_assoc_check;	/**< check for associated STAs before going to power save */
	uint	in_power_save_counter;	/**< how many times in the power save mode */
	uint	in_power_save_secs;	/**< how many seconds in the power save mode */
	uint	quiet_time_counter;	/**< quiet time before we enter the  power save mode */
	uint	prev_pktcount;		/**< total pkt count from the previous second */
	uint	quiet_time;		/**< quiet time in the network before we go to power save */
	uint	pps_threshold;		/**< pps threshold for power save */
} wlc_pwrsave_t;

typedef struct wlc_rxchain_pwrsave {
	/* need to save rx_stbc HT capability before enter rxchain_pwrsave mode */
	uint8	ht_cap_rx_stbc;		/**< configured rx_stbc HT capability */
	uint	rxchain;		/**< configured rxchains */
	wlc_pwrsave_t pwrsave;
} wlc_rxchain_pwrsave_t;

typedef struct wlc_radio_pwrsave {
	uint8  level;			/**< Low, Medium or High Power Savings */
	uint16 on_time;			/**< number of  TUs radio is 'on' */
	uint16 off_time;		/**< number of  TUs radio is 'off' */
	int radio_disabled;		/**< Whether the radio needs to be disabled now */
	uint	pwrsave_state;		/**< radio pwr save state */
	int32	tbtt_skip;		/**< num of tbtt to skip */
	bool	cncl_bcn;		/**< whether to stop bcn or not in level 1 & 2. */
	struct wl_timer *timer;		/**< timer to keep track of duty cycle */
	wlc_pwrsave_t pwrsave;
} wlc_radio_pwrsave_t;

#ifndef AIDMAPSZ
#define AIDMAPSZ	(ROUNDUP(MAXSCB, NBBY)/NBBY)	/**< aid bitmap size in bytes */
#endif // endif

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

typedef struct wlc_supp_chan_set {
	uint8 first_channel;		/* first channel supported */
	uint8 num_channels;		/* number of channels suuported */
} wlc_supp_chan_set_t;

struct wlc_supp_channels {
	uint8 count;			/* count of supported channels */
	uint8 chanset[MAXCHANNEL/8];	/* bitmap of Supported channel set */
};

/*
* The operational mode capabilities for STAs required to associate
* to the BSS.
* NOTE: The order of values is important. They must be kept in
* increasing order of capabilities, because the enums are compared
* numerically.
* That is: OMC_HE > OMC_VHT > OMC_HT > OMC_ERP > OMC_NONE.
*/
typedef enum _opmode_cap_t {

	OMC_NONE = 0, /**< no requirements for STA to associate to BSS */

	OMC_ERP = 1, /**< STA must advertise ERP (11g) capabilities
				  * to be allowed to associate to 2G band BSS.
				  */
	OMC_HT = 2,	 /**< STA must advertise HT (11n) capabilities to
				  * be allowed to associate to the BSS.
				  */
	OMC_VHT = 3, /**< Devices must advertise VHT (11ac) capabilities
				  * to be allowed to associate to the BSS.
				  */
	OMC_HE = 4, /**< Devices must advertise HE (11ax) capabilities
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
	bool ap_up_pending;
	bool authresp_macfltr;	/* enable/disable suppressing auth resp by MAC filter */
	uint8 *aidmap;		/**< aid map */
	wlc_msch_req_handle_t *msch_req_hdl;
	wlc_msch_req_param_t req;
	uint32 msch_state;
} ap_bsscfg_cubby_t;

typedef struct ap_bsscfg_cubby_copy {
	opmode_cap_t opmode_cap_reqd;
	bool authresp_macfltr;	/* enable/disable suppressing auth resp by MAC filter */
	uint8 *aidmap;		/**< aid map */
} ap_bsscfg_cubby_copy_t;

#define AP_BSSCFG_CUBBY_LEN	(sizeof(ap_bsscfg_cubby_copy_t) + AIDMAPSZ)

#define AP_BSSCFG_CUBBY(__apinfo, __cfg) \
	((ap_bsscfg_cubby_t *)BSSCFG_CUBBY((__cfg), (__apinfo)->cfgh))

/* AP Channel Ctx states (msch_state) */
typedef enum {
	AP_SCHED_UNREG =     0x1,
	AP_SCHED_PRE_INIT =  0x2,
	AP_SCHED_ON_CHAN =   0x3,
	AP_SCHED_OFF_CHAN =  0x4
} ap_sched_state_t;

static wlc_bsscfg_t* wlc_ap_iface_create(void *if_module_ctx, wl_interface_create_t *if_buf,
	wl_interface_info_t *wl_info, int32 *err);
static int32 wlc_ap_iface_remove(wlc_info_t *wlc, wlc_if_t *wlcif);

typedef struct
{
	uint32	rxframe;
	uint32	txframe;
	uint32	rxbytes;
	uint32	txbytes;
} data_snapshot_t;

/** Private AP data structure */
typedef struct
{
	struct wlc_ap_info	appub;		/**< Public AP interface: MUST BE FIRST */
	wlc_info_t	*wlc;
	wlc_pub_t	*pub;
	uint32		maxassoc;		/**< Max # associations to allow */
	wlc_radio_pwrsave_t radio_pwrsave;	/**< radio duty cycle power save structure */
	uint16		txbcn_inactivity;	/**< txbcn inactivity counter */
	uint16		txbcn_snapshot;		/**< snapshot of txbcnfrm register */
	uint16		txbcn_timeout;		/**< txbcn inactivity timeout */
	uint16		pre_tbtt_us;		/**< Current pre-TBTT us value */
	int		cfgh;			/**< ap bsscfg cubby handle */
	bool		ap_sta_onradar;		/**< local AP and STA are on overlapping
						 * radar channel?
						 */
	int		as_scb_handle;	/**< scb cubby handle to retrieve data from scb */
	int		ap_scb_handle;	/* scb cubby handle to access ap_scb_info_t from scb */

	wlc_rxchain_pwrsave_t  rxchain_pwrsave;	/**< rxchain reduction power save structure */
	ratespec_t	force_bcn_rspec; /**< force setup beacon ratespec (in unit of 500kbps) */
	uint		scb_timeout;            /**< inactivity timeout for associated STAs */
	uint8		scb_mark_del_timeout;   /**< delay before freeing marked for delete scb */
	uint		scb_activity_time;      /**< skip probe if activity during this time */
	uint		scb_max_probe;          /**< max number of probes to be conducted */
	bool		reprobe_scb;            /**< to let watchdog know there are scbs to probe */

	chanspec_t	chanspec_selected;      /**< chan# selected by WLC_START_CHANNEL_SEL */
	uint8 basic_mcs[MCSSET_LEN]; /**< Bit position means mcs rate */

	int		scb_supp_chan_handle;	/* To get list of sta supported channels */
	int		scb_assoc_oui_handle;	/* To get oui list of the station */
	uint32		recheck_160_80;		/* relative sec to pub.now to recheck 160/80 mode */
	data_snapshot_t	snapshot;               /**< snapshot for interface traffic */
} wlc_ap_info_pvt_t;

/* assoc scb cubby */
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
	uint buf_len;
	uint16 status;
} wlc_assoc_req_t;

#define AS_SCB_CUBBY_LOC(appvt, scb) (wlc_assoc_req_t **)SCB_CUBBY(scb, (appvt)->as_scb_handle)
#define AS_SCB_CUBBY(appvt, scb) *AS_SCB_CUBBY_LOC(appvt, scb)

/* general scb cubby */
typedef struct {
	uint8	*wpaie;		/**< WPA IE */
	uint16	wpaie_len;	/**< Length of wpaie */
	uint16	grace_attempts;	/**< Additional attempts made beyond scb_timeout
				 * before scb is removed
				 */
	uint8	*challenge;	/**< pointer to shared key challenge info element */
	struct scb *psta_prim;	/**< pointer to primary proxy sta */
	uint8	flags; /* bitwise properties e.g. can communicate in global reg class */
} ap_scb_cubby_t;

#define AP_SCB_CUBBY_LOC(appvt, scb) (ap_scb_cubby_t **)SCB_CUBBY(scb, (appvt)->ap_scb_handle)
#define AP_SCB_CUBBY(appvt, scb) *AP_SCB_CUBBY_LOC(appvt, scb)

#define SCB_SUPP_CHAN_CUBBY(appvt, scb) \
	(wlc_supp_channels_t*)SCB_CUBBY((scb), (appvt)->scb_supp_chan_handle)

#define SCB_ASSOC_OUI_CUBBY(appvt, scb) \
		(wlc_assoc_oui_t*)SCB_CUBBY((scb), (appvt)->scb_assoc_oui_handle)

/* IOVar table */

/* Parameter IDs, for use only internally to wlc -- in the wlc_ap_iovars
 * table and by the wlc_ap_doiovar() function.  No ordering is imposed:
 * the table is keyed by name, and the function uses a switch.
 */
enum wlc_ap_iov {
	IOV_AP_ISOLATE = 1,
	IOV_SCB_ACTIVITY_TIME = 2,
	IOV_AUTHE_STA_LIST = 3,
	IOV_AUTHO_STA_LIST = 4,
	IOV_WME_STA_LIST = 5,
	IOV_BSS = 6,
	IOV_MAXASSOC = 7,
	IOV_BSS_MAXASSOC = 8,
	IOV_CLOSEDNET = 9,
	IOV_AP = 10,
	IOV_APSTA = 11,				/**< enable simultaneously active AP/STA */
	IOV_AP_ASSERT = 12,			/**< User forced crash */
	IOV_RXCHAIN_PWRSAVE_ENABLE = 13,	/**< Power Save with single rxchain enable */
	IOV_RXCHAIN_PWRSAVE_QUIET_TIME = 14,	/**< Power Save with single rxchain quiet time */
	IOV_RXCHAIN_PWRSAVE_PPS = 15,		/**< single rxchain packets per second */
	IOV_RXCHAIN_PWRSAVE = 16,		/**< Current power save mode */
	IOV_RXCHAIN_PWRSAVE_STAS_ASSOC_CHECK = 17,	/**< Whether to check for associated stas */
	IOV_RADIO_PWRSAVE_ENABLE = 18,		/**< Radio duty cycle Power Save enable */
	IOV_RADIO_PWRSAVE_QUIET_TIME = 19,	/**< Radio duty cycle Power Save */
	IOV_RADIO_PWRSAVE_PPS = 20,		/**< Radio power save packets per second */
	IOV_RADIO_PWRSAVE = 21,			/**< Whether currently in power save or not */
	IOV_RADIO_PWRSAVE_LEVEL = 22,		/**< Radio power save duty cycle on time */
	IOV_RADIO_PWRSAVE_STAS_ASSOC_CHECK = 23,	/**< Whether to check for associated stas */
	IOV_AP_RESET = 24,	/**< User forced reset */
	IOV_BCMDCS = 25,	/**< dynamic channel switch (management) */
	IOV_DYNBCN = 26,	/**< Dynamic beaconing */
	IOV_SCB_LASTUSED = 27,	/**< time (s) elapsed since any of the associated scb is used */
	IOV_SCB_PROBE = 28,	/**< get/set scb probe parameters */
	IOV_SCB_ASSOCED = 29,	/**< if it has associated SCBs at phy if level */
	IOV_ACS_UPDATE = 30,		/**< update after acs_scan and chanspec selection */
	IOV_PROBE_RESP_INFO = 31,	/**< get probe response management packet */
	IOV_MODE_REQD = 32,	/*
				 * operational mode capabilities required for STA
				 * association acceptance with the BSS
				 */
	IOV_BSS_RATESET = 33,		/**< Set rateset per BSS */
	IOV_FORCE_BCN_RSPEC = 34, /**< Setup Beacon rate from lowest basic to specific basic rate */
	IOV_AUTHRESP_MACFLTR = 36,	/**< enable/disable suppressing auth resp by MAC filter */
	IOV_PROXY_ARP_ADVERTISE = 37, /**< Update beacon, probe response frames for proxy arp bit */
	IOV_WOWL_PKT = 38,		/**< Generate a wakeup packet */
	IOV_SET_RADAR = 39,		/* Set radar. Convert IOCTL to IOVAR for RSDB. */
	IOV_MCAST_FILTER_NOSTA = 40,	/**< drop mcast frames on BSS if no STAs associated */
	IOV_CEVENT = 41,		/**< enable/disable cevent levels */
	IOV_STA_SCB_TIMEOUT = 42,	/**< SCB timeout per STA */
	IOV_ODAP_ACK_TIMEOUT = 43,	/**< outdoor AP ack timeout */
	IOV_ODAP_SLOT_TIME = 44,	/**< outdoor AP slot time */
	IOV_TRF_THOLD = 45,		/**< set thresholds for traffic failure */
	IOV_MAP = 46,			/**< Set/Unset multiap. */
	IOV_FAR_STA_RSSI = 47,		/**< Setting far_sta rssi. */
	IOV_KEEP_AP_UP = 48,	/**< Set/Unset AP can up if bsscfg->disable_ap_up is TRUE */
	IOV_BW_SWITCH_160 = 49,		/* 160-80 BW Switch. 0-Disabled, 1-TDCS WAR, 2-BW Switch */
	IOV_SCB_MARK_DEL_TIMEOUT = 50,	/**< delay before freeing marked for delete  scb */
	IOV_BCNPRS_TXPWR_OFFSET = 51,	/**< Specify additional txpwr backoff for bcn/prs in dB. */
	IOV_TXBCN_TIMEOUT = 52, /**< interval to trigger tx beacon loss */
	IOV_SCB_IDLE_POLL_THRESH = 53,  /**< Send BlockAckRequest to idle SCBs */
	IOV_LAST,		/**< In case of a need to check max ID number */
};

/** AP IO Vars */
static const bcm_iovar_t wlc_ap_iovars[] = {
	{"ap", IOV_AP,
	0, 0, IOVT_INT32, 0
	},
	{"ap_isolate", IOV_AP_ISOLATE,
	(0), 0, IOVT_BOOL, 0
	},
	{"scb_activity_time", IOV_SCB_ACTIVITY_TIME,
	(IOVF_NTRL|IOVF_RSDB_SET), 0, IOVT_UINT32, 0
	},
	{"authe_sta_list", IOV_AUTHE_STA_LIST,
	(IOVF_SET_UP), 0, IOVT_BUFFER, sizeof(uint32)
	},
	{"autho_sta_list", IOV_AUTHO_STA_LIST,
	(IOVF_SET_UP), 0, IOVT_BUFFER, sizeof(uint32)
	},
	{"wme_sta_list", IOV_WME_STA_LIST,
	(0), 0, IOVT_BUFFER, sizeof(uint32)
	},
	{"maxassoc", IOV_MAXASSOC,
	(IOVF_WHL|IOVF_RSDB_SET), 0, IOVT_UINT32, 0
	},
	{"bss_maxassoc", IOV_BSS_MAXASSOC,
	(IOVF_NTRL), 0, IOVT_UINT32, 0
	},
	{"bss", IOV_BSS,
	(0), 0, IOVT_INT32, 0
	},
	{"closednet", IOV_CLOSEDNET,
	(0), 0, IOVT_BOOL, 0
	},
#ifdef RXCHAIN_PWRSAVE
	{"rxchain_pwrsave_enable", IOV_RXCHAIN_PWRSAVE_ENABLE,
	(0), 0, IOVT_UINT8, 0
	},
	{"rxchain_pwrsave_quiet_time", IOV_RXCHAIN_PWRSAVE_QUIET_TIME,
	(0), 0, IOVT_UINT32, 0
	},
	{"rxchain_pwrsave_pps", IOV_RXCHAIN_PWRSAVE_PPS,
	(0), 0, IOVT_UINT32, 0
	},
	{"rxchain_pwrsave", IOV_RXCHAIN_PWRSAVE,
	(0), 0, IOVT_UINT8, 0
	},
	{"rxchain_pwrsave_stas_assoc_check", IOV_RXCHAIN_PWRSAVE_STAS_ASSOC_CHECK,
	(0), 0, IOVT_UINT8, 0
	},
#endif /* RXCHAIN_PWRSAVE */
#ifdef RADIO_PWRSAVE
	{"radio_pwrsave_enable", IOV_RADIO_PWRSAVE_ENABLE,
	(0), 0, IOVT_UINT8, 0
	},
	{"radio_pwrsave_quiet_time", IOV_RADIO_PWRSAVE_QUIET_TIME,
	(0), 0, IOVT_UINT32, 0
	},
	{"radio_pwrsave_pps", IOV_RADIO_PWRSAVE_PPS,
	(0), 0, IOVT_UINT32, 0
	},
	{"radio_pwrsave_level", IOV_RADIO_PWRSAVE_LEVEL,
	(0), 0, IOVT_UINT8, 0
	},
	{"radio_pwrsave", IOV_RADIO_PWRSAVE,
	(0), 0, IOVT_UINT8, 0
	},
	{"radio_pwrsave_stas_assoc_check", IOV_RADIO_PWRSAVE_STAS_ASSOC_CHECK,
	(0), 0, IOVT_UINT8, 0
	},
#endif /* RADIO_PWRSAVE */
#if defined(STA) /* APSTA */
	{"apsta", IOV_APSTA,
	(IOVF_SET_DOWN|IOVF_RSDB_SET), 0, IOVT_BOOL, 0,
	},
#endif /* APSTA */
#ifdef BCM_DCS
	{"bcm_dcs", IOV_BCMDCS,
	(0), 0, IOVT_BOOL, 0
	},
#endif /* BCM_DCS */
	{"dynbcn", IOV_DYNBCN,
	(0), 0, IOVT_BOOL, 0
	},
	{"scb_lastused", IOV_SCB_LASTUSED, (0), 0, IOVT_UINT32, 0},
	{"scb_probe", IOV_SCB_PROBE,
	(IOVF_SET_UP|IOVF_RSDB_SET), 0, IOVT_BUFFER, sizeof(wl_scb_probe_t)
	},
	{"scb_assoced", IOV_SCB_ASSOCED, (0), 0, IOVT_BOOL, 0},
	{"acs_update", IOV_ACS_UPDATE,
	(IOVF_SET_UP), 0, IOVT_BOOL, 0
	},
	{"probe_resp_info", IOV_PROBE_RESP_INFO,
	(IOVF_OPEN_ALLOW), 0, IOVT_BUFFER, sizeof(uint32),
	},
	{"mode_reqd", IOV_MODE_REQD,
	(IOVF_OPEN_ALLOW), 0, IOVT_UINT8, 0
	},
	{"bss_rateset", IOV_BSS_RATESET,
	(0), 0, IOVT_INT32, 0
	},
	{"force_bcn_rspec", IOV_FORCE_BCN_RSPEC,
	(0), 0, IOVT_UINT8, 0
	},
#ifdef WLAUTHRESP_MAC_FILTER
	{"authresp_mac_filter", IOV_AUTHRESP_MACFLTR,
	(0), 0, IOVT_BOOL, 0
	},
#endif /* WLAUTHRESP_MAC_FILTER */
	{"proxy_arp_advertise", IOV_PROXY_ARP_ADVERTISE,
	(0), 0, IOVT_BOOL, 0
	},
	{"wowl_pkt", IOV_WOWL_PKT, 0, 0, IOVT_BUFFER, 0},
	{"radar", IOV_SET_RADAR,
	(IOVF_RSDB_SET), 0, IOVT_BOOL, 0
	},
#ifdef WL_MCAST_FILTER_NOSTA
	{"mcast_filter_nosta", IOV_MCAST_FILTER_NOSTA,
	(0), 0, IOVT_BOOL, 0
	},
#endif // endif
#ifdef BCM_CEVENT
	{"cevent", IOV_CEVENT, 0, 0,  IOVT_UINT32, 0},
#endif /* BCM_CEVENT */
#ifdef WL_TRAFFIC_THRESH
	{"traffic_thresh", IOV_TRF_THOLD, 0, 0, IOVT_BUFFER, sizeof(wl_traffic_thresh_t)},
#endif /* WL_TRAFFIC_THRESH */
#ifdef MULTIAP
	{"map", IOV_MAP, (IOVF_SET_DOWN), 0, IOVT_UINT8, 0},
#endif	/* MULTIAP */
	{"far_sta_rssi", IOV_FAR_STA_RSSI, 0, 0, IOVT_INT8, 0},
	{"keep_ap_up", IOV_KEEP_AP_UP, 0, 0, IOVT_UINT8, 0},
	{"bw_switch_160", IOV_BW_SWITCH_160, (IOVF_SET_DOWN), 0, IOVT_UINT8, 0},
	{"scb_markdel_time", IOV_SCB_MARK_DEL_TIMEOUT, 0, 0,  IOVT_UINT8, 0},
	{"bcnprs_txpwr_offset", IOV_BCNPRS_TXPWR_OFFSET, 0, 0, IOVT_UINT8, 0},
	{"txbcn_timeout", IOV_TXBCN_TIMEOUT, 0, 0, IOVT_UINT32, 0},
	{NULL, 0, 0, 0, 0, 0 }
};

/* Local Prototypes */
static void wlc_ap_watchdog(void *arg);
static int wlc_ap_wlc_up(void *ctx);
static void wlc_assoc_notify(wlc_ap_info_t *ap, struct ether_addr *sta, wlc_bsscfg_t *cfg);
static void wlc_reassoc_notify(wlc_ap_info_t *ap, struct ether_addr *sta, wlc_bsscfg_t *cfg);
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

static int wlc_ap_doiovar(void *hdl, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif);
static int wlc_ap_ioctl(void *hdl, uint cmd, void *arg, uint len, struct wlc_if *wlcif);

static void wlc_ap_up_upd(wlc_ap_info_t *ap, wlc_bsscfg_t *bsscfg, bool state);

static void wlc_ap_acs_update(wlc_info_t *wlc);
static int wlc_ap_set_opmode_cap_reqd(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	opmode_cap_t opmode);
static opmode_cap_t wlc_ap_get_opmode_cap_reqd(wlc_info_t* wlc, wlc_bsscfg_t *bsscfg);
static int wlc_ap_bsscfg_init(void *context, wlc_bsscfg_t *cfg);
static void wlc_ap_bsscfg_deinit(void *context, wlc_bsscfg_t *cfg);
#if defined(BCMDBG)
static void wlc_ap_bsscfg_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b);
#else
#define wlc_ap_bsscfg_dump NULL
#endif // endif
#ifdef WLRSDB
static int wlc_ap_bss_get(void *ctx, wlc_bsscfg_t *from_cfg, uint8 *data, int *len);
static int wlc_ap_bss_set(void *ctx, wlc_bsscfg_t *to_cfg, const uint8 *data, int len);
#endif /* WLRSDB */
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

#ifdef SPLIT_ASSOC
static int wlc_as_scb_init(void *context, struct scb *scb);
static void wlc_as_scb_deinit(void *context, struct scb *scb);
static uint wlc_as_scb_secsz(void *context, struct scb *scb);
#ifdef WLRSDB
static int wlc_as_scb_update(void *context, struct scb *scb, wlc_bsscfg_t* new_cfg);
#endif /* WLRSDB */
#endif /* SPLIT_ASSOC */

static void wlc_ap_probe_complete(wlc_info_t *wlc, void *pkt, uint txs);

static int wlc_ap_scb_init(void *context, struct scb *scb);
static void wlc_ap_scb_deinit(void *context, struct scb *scb);
static uint wlc_ap_scb_secsz(void *context, struct scb *scb);

/* IE mgmt */
static uint wlc_auth_calc_chlng_ie_len(void *ctx, wlc_iem_calc_data_t *data);
static int wlc_auth_write_chlng_ie(void *ctx, wlc_iem_build_data_t *data);
static uint wlc_assoc_calc_sup_rates_ie_len(void *ctx, wlc_iem_calc_data_t *data);
static int wlc_assoc_write_sup_rates_ie(void *ctx, wlc_iem_build_data_t *data);
static uint wlc_assoc_calc_ext_rates_ie_len(void *ctx, wlc_iem_calc_data_t *data);
static int wlc_assoc_write_ext_rates_ie(void *ctx, wlc_iem_build_data_t *data);
static int wlc_auth_parse_chlng_ie(void *ctx, wlc_iem_parse_data_t *data);
static int wlc_assoc_parse_ssid_ie(void *ctx, wlc_iem_parse_data_t *data);
static int wlc_assoc_parse_sup_rates_ie(void *ctx, wlc_iem_parse_data_t *data);
static int wlc_assoc_parse_ext_rates_ie(void *ctx, wlc_iem_parse_data_t *data);
static int wlc_assoc_parse_supp_chan_ie(void *ctx, wlc_iem_parse_data_t *data);
static int wlc_assoc_parse_wps_ie(void *ctx, wlc_iem_parse_data_t *data);
static int wlc_assoc_parse_psta_ie(void *ctx, wlc_iem_parse_data_t *data);

#ifdef WLDFS
static bool wlc_ap_on_radarchan(wlc_ap_info_t *ap);
#endif // endif
static int wlc_force_bcn_rspec_upd(wlc_info_t *wlc, wlc_bsscfg_t *cfg, wlc_ap_info_t *ap,
	ratespec_t rspec);

static int wlc_ap_pre_sendauth_cb(wlc_info_t *wlc, wlc_bsscfg_t *cfg, void *scb, void *pkt);
static void wlc_ap_auth_tx_complete(wlc_info_t *wlc, uint txstatus, void *arg);

static bool wlc_ap_assreq_verify_rates(wlc_assoc_req_t *param, wlc_iem_ft_pparm_t *ftpparm,
	opmode_cap_t opmode_cap_reqd, uint16 capability, wlc_rateset_t *req_rates);
static bool wlc_ap_assreq_verify_authmode(wlc_info_t *wlc, struct scb *scb,
	wlc_bsscfg_t *bsscfg, uint16 capability, bool akm_ie_included,
	struct dot11_management_header *hdr);

static int wlc_ap_msch_clbk(void* handler_ctxt, wlc_msch_cb_info_t *cb_info);
static void wlc_ap_return_home_channel(wlc_bsscfg_t *cfg);
static void wlc_ap_prepare_off_channel(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
static void wlc_ap_off_channel_done(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
static int32 wlc_ap_verify_basic_mcs(wlc_ap_info_t *ap, const uint8 *rates, uint32 len);
static void wlc_ap_upd_onchan(wlc_ap_info_t *ap, wlc_bsscfg_t *bsscfg);

static void wlc_ap_authresp_deauth(wlc_info_t *wlc, struct scb *scb);
#ifdef WL_SAE
static uint wlc_sae_parse_auth(wlc_bsscfg_t *cfg, struct dot11_management_header *hdr,
		uint8 *body, uint body_len);
#endif /* WL_SAE */

#ifdef WL_GLOBAL_RCLASS
static int wlc_assoc_parse_regclass_ie(void *ctx, wlc_iem_parse_data_t *data);
static void wlc_ap_scb_no_gbl_rclass_upd(wlc_info_t *wlc, struct scb *scb,
	uint8 scb_assoced, uint8 oldstate);
#endif /* WL_GLOBAL_RCLASS */
static void wlc_ap_scb_state_upd_cb(void *ctx, scb_state_upd_data_t *notif_data);

#ifdef WL_MODESW
static int wlc_ap_160_80_bw_switch(wlc_ap_info_t *ap);
#endif /* WL_MODESW */

#ifdef WL_TRAFFIC_THRESH
int wlc_traffic_thresh_set_val(wlc_info_t* wlc, wlc_bsscfg_t* bsscfg,
	wl_traffic_thresh_t *req);
int wlc_traffic_thresh_get_val(wlc_info_t* wlc, wlc_bsscfg_t* bsscfg,
	wl_traffic_thresh_t *req, struct get_traffic_thresh *res);
int
wlc_traffic_thresh_set_val(wlc_info_t* wlc, wlc_bsscfg_t* bsscfg,
	wl_traffic_thresh_t *req)
{
	int i;
	int err = BCME_OK;
	struct scb *scb;
	struct scb_iter scbiter;
	wlc_bsscfg_t* cfg;
	wlc_trf_data_t *ptr;
	wlc_intfer_params_t *val;
	switch (req->mode) {
	case WL_TRF_MODE_CFG_FEATURE:
	{
		if (req->enable) {
			if (!bsscfg->traffic_thresh_enab) {
				if (wlc->pub->_traffic_thresh) {
					err = wlc_bsscfg_traffic_thresh_init(wlc, bsscfg);
					if (err != BCME_OK) {
						wlc_bsscfg_traffic_thresh_deinit(wlc, bsscfg);
						return err;
					}
				}
				bsscfg->traffic_thresh_enab = 1;
			}
		} else {
			if (bsscfg->traffic_thresh_enab) {
				wlc_bsscfg_traffic_thresh_deinit(wlc, bsscfg);
				bsscfg->traffic_thresh_enab = 0;
			}
		}
	}
	break;
	case WL_TRF_MODE_WL_FEATURE:
	{
		if (req->enable) {
			if (!wlc->pub->_traffic_thresh) {
				FOREACH_BSS(wlc, i, cfg) {
					if (cfg->traffic_thresh_enab) {
						err = wlc_bsscfg_traffic_thresh_init(wlc, cfg);
						if (err != BCME_OK) {
							wlc_bsscfg_traffic_thresh_deinit(wlc,
								cfg);
							return err;
						}
					}
				}
				wlc->pub->_traffic_thresh = 1;
			}
		} else {
			if (wlc->pub->_traffic_thresh) {
				FOREACH_BSS(wlc, i, cfg) {
					if (cfg->traffic_thresh_enab) {
						wlc_bsscfg_traffic_thresh_deinit(wlc, cfg);
					}
				}
				wlc->pub->_traffic_thresh = 0;
			}
		}
	}
	break;
	case WL_TRF_MODE_AP:
	{
		if (req->type & WL_TRF_AE) {
			req->type &= 0x0f;
			if (req->enable) {
				bsscfg->trf_cfg_enable |= (1 << req->type);
			} else {
				bsscfg->trf_cfg_enable &= ~(1 << req->type);
				if (wlc->pub->_traffic_thresh && bsscfg->traffic_thresh_enab) {
					ptr = &bsscfg->trf_cfg_data[req->type];
					val = &bsscfg->trf_cfg_params[req->type];
					/* reset the data as feature is disabled */
					ptr->cur = 0;
					ptr->count = 0;
					for (i = 0; i < val->num_secs; i++) {
						bsscfg->trf_cfg_data[req->type].num_data[i] = 0;
					}
				}
			}
			break;
		}
		/* delete old num_data */
		ptr = &bsscfg->trf_cfg_data[req->type];
		val = &bsscfg->trf_cfg_params[req->type];
		if (val->num_secs == req->num_secs) {
			if ((val->thresh != req->thresh) && (req->thresh)) {
				/* reset the data as thresh is changed */
				ptr->cur = 0;
				ptr->count = 0;
				for (i = 0; i < val->num_secs; i++) {
					bsscfg->trf_cfg_data[req->type].num_data[i] = 0;
				}
				val->thresh = req->thresh;
			} else {
				err = BCME_USAGE_ERROR;
			}
			break;
		}
		/* now the memory needs to reallocated */
		if (ptr->num_data != NULL) {
			MFREE(wlc->osh, ptr->num_data,
					sizeof(uint16) * val->num_secs);
		}
		if (req->num_secs) {
			if (wlc->pub->_traffic_thresh && bsscfg->traffic_thresh_enab) {
				if ((ptr->num_data = MALLOCZ(wlc->osh,
						sizeof(uint16) * req->num_secs))
						== NULL) {
					WL_ERROR(("wl%d: not enough memory\n",
						wlc->pub->unit));
					err = BCME_NORESOURCE;
					break;
				}
			}
			val->thresh = req->thresh;
		} else {
			val->thresh = 0;
		}
		ptr->cur = 0;
		ptr->count = 0;
		val->num_secs = req->num_secs;
	}
	break;
	case WL_TRF_MODE_STA:
	{
		if (req->type & WL_TRF_AE) {
			req->type &= 0x0f;
			if (req->enable) {
				if (!(bsscfg->trf_scb_enable & (1 << req->type))) {
					FOREACH_BSS_SCB(wlc->scbstate,
							&scbiter, bsscfg, scb) {
						scb->trf_enable_flag |= (1 << req->type);
					}
					bsscfg->trf_scb_enable |= (1 << req->type);
				}
			} else {
				if (bsscfg->trf_scb_enable & (1 << req->type)) {
					FOREACH_BSS_SCB(wlc->scbstate,
							&scbiter, bsscfg, scb) {
						scb->trf_enable_flag &= ~(1 << req->type);
						scb->scb_trf_data[req->type].timestamp = 0;
						scb->scb_trf_data[req->type].idx = 0;
						scb->scb_trf_data[req->type].histo[0] = 0;
					}
					bsscfg->trf_scb_enable &= ~(1 << req->type);
				}
			}
			break;
		}
		/* delete old num_data */
		val = &bsscfg->trf_scb_params[req->type];
		if (val->num_secs == req->num_secs) {
			if (val->thresh == req->thresh) {
				/* nothing to change */
				err = BCME_OK;
				break;
			} else if (req->thresh) {
				/* reset the data as thresh is changed */
				val->thresh = req->thresh;
				FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
					scb->scb_trf_data[req->type].timestamp = 0;
					scb->scb_trf_data[req->type].idx = 0;
					scb->scb_trf_data[req->type].histo[0] = 0;
				}
			} else {
				err = BCME_USAGE_ERROR;
			}
			break;
		}
		if (req->num_secs) {
			val->thresh = req->thresh;
			/* reset the data as params are changed */
			FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
				scb->scb_trf_data[req->type].timestamp = 0;
				scb->scb_trf_data[req->type].idx = 0;
				scb->scb_trf_data[req->type].histo[0] = 0;
			}
		} else {
			val->thresh = 0;
		}
		if (req->num_secs >= WL_TRF_MAX_SECS) {
			val->num_secs = WL_TRF_MAX_SECS;
		} else {
			val->num_secs = req->num_secs;
		}
	}
	break;
	case WL_TRF_MODE_MAC:
	{
		FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
			if (!ether_cmp(&scb->ea, &req->mac)) {
				req->type &= 0x0f;
				if (req->enable) {
					if (!(scb->trf_enable_flag & (1 << req->type))) {
						scb->trf_enable_flag |= (1 << req->type);
					}
				} else {
					if (scb->trf_enable_flag & (1 << req->type)) {
						/* reset the data and disable flag */
						scb->scb_trf_data[req->type].timestamp = 0;
						scb->scb_trf_data[req->type].idx = 0;
						scb->scb_trf_data[req->type].histo[0] = 0;
						scb->trf_enable_flag &= ~(1 << req->type);
					}
				}
				err = BCME_OK;
				break;
			} else {
				err = BCME_NORESOURCE;
			}
		}
	}
		break;
	default:
	err = BCME_USAGE_ERROR;
	break;
}
return err;
}
int
wlc_traffic_thresh_get_val(wlc_info_t* wlc, wlc_bsscfg_t* bsscfg,
	wl_traffic_thresh_t *req, struct get_traffic_thresh *res)
{
	int i, err = BCME_OK;
	wl_traffic_thresh_t *data = res->data;
	wlc_intfer_params_t *ptr = NULL;
	res->intf_enab = wlc->pub->_traffic_thresh;
	res->bss_enab = bsscfg->traffic_thresh_enab;
	switch (req->mode) {
	case WL_TRF_MODE_AP:
	{
		ptr = bsscfg->trf_cfg_params;
		res->auto_enable = bsscfg->trf_cfg_enable;
		res->mode = WL_TRF_MODE_AP;
		for (i = 0; i < WL_TRF_MAX_QUEUE; i++) {
			data[i].thresh = ptr[i].thresh;
			data[i].num_secs = ptr[i].num_secs;
		}
		err = BCME_OK;
	}
	break;
	case WL_TRF_MODE_STA:
	{
		ptr = bsscfg->trf_scb_params;
		res->auto_enable = bsscfg->trf_scb_enable;
		res->mode = WL_TRF_MODE_STA;
		for (i = 0; i < WL_TRF_MAX_QUEUE; i++) {
			data[i].thresh = ptr[i].thresh;
			data[i].num_secs = ptr[i].num_secs;
		}
		err = BCME_OK;
	}
	break;
	case  WL_TRF_MODE_MAC:
	{
		struct scb *scb;
		struct scb_iter scbiter;
		res->mode = WL_TRF_MODE_MAC;
		FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
			if (!ether_cmp(&scb->ea, &req->mac)) {
				res->auto_enable = scb->trf_enable_flag;
				err =  BCME_OK;
				break;
			}
			err = BCME_NORESOURCE;
		}
	}
	break;
	default:
	{
		err = BCME_USAGE_ERROR;
		break;
	}

	}
	return err;
}
#endif /* WL_TRAFFIC_THRESH */

#ifdef WL_MODESW
static int
wlc_ap_160_80_bw_switch(wlc_ap_info_t *ap)
{
	wlc_ap_info_pvt_t* appvt = (wlc_ap_info_pvt_t*)ap;
	wlc_info_t *wlc = appvt->wlc;
	wlc_bsscfg_t *bsscfg = wlc->cfg;
	uint8 new_oper_mode = 0, curr_oper_mode, bw = 0, nss;
	uint16 num_160mhz_assocs = wlc->num_160mhz_assocs;
	bool upgrd_160 = FALSE;
	chanspec_t chspec_160;
	int idx;

	appvt->recheck_160_80 = 0;

	/* When primary interface is STA, return */
	if (BSSCFG_STA(bsscfg)) {
		return BCME_UNSUPPORTED;
	}

	if (BAND_2G(wlc->band->bandtype)) {
		return BCME_BADBAND;
	}

	if (!WLC_BW_SWITCH_ENABLED(wlc)) {
		return BCME_UNSUPPORTED;
	}

	WL_MODE_SWITCH(("wl%d %s chanspec 0x%x num_160mhz_assocs %d\n", wlc->pub->unit,
			__FUNCTION__, wlc->chanspec, num_160mhz_assocs));

	if (num_160mhz_assocs && CHSPEC_IS80(wlc->chanspec)) {
		uint16 center_ch = CHSPEC_CHANNEL(wlc->chanspec);
		if (center_ch < 36 || center_ch > 128) {
			WL_MODE_SWITCH(("wl%d Bad chspec 0x%x center_ch %d\n",
					wlc->pub->unit, wlc->chanspec, center_ch));
			return BCME_OK;
		}
		chspec_160 = wf_channel2chspec(wf_chspec_ctlchan(wlc->chanspec),
				WL_CHANSPEC_BW_160);
		WL_MODE_SWITCH(("wl%d chspec_160 0x%x center_ch %d\n",
				wlc->pub->unit, chspec_160, center_ch));
		if (!WL_BW_CAP_160MHZ(wlc->band->bw_cap) ||
				!CHSPEC_IS160(chspec_160) ||
				!wlc_valid_chanspec_db(wlc->cmi, chspec_160)) {
			return BCME_OK;
		}

		if (wlc_quiet_chanspec(wlc->cmi, chspec_160)) {
			wl_event_req_bw_upgd_t evt_data;

			memset(&evt_data, 0, sizeof(evt_data));
			evt_data.version = WL_EVENT_REQ_BW_UPGD_VER_1;
			evt_data.length = WL_EVENT_REQ_BW_UPGD_LEN;
			evt_data.upgrd_chspec = chspec_160;
			/* send an event so that ACSD app can trigger BGDFS */
			wlc_bss_mac_event(wlc, bsscfg, WLC_E_REQ_BW_CHANGE, NULL, 0,
				0, 0, (void *)&evt_data, sizeof(evt_data));
			WL_MODE_SWITCH(("wl%d 160Mhz upgrade by WLC_E_REQ_BW_CHANGE\n",
				wlc->pub->unit));
			return BCME_OK;
		}
		upgrd_160 = TRUE;
	} else if (!num_160mhz_assocs && CHSPEC_IS160(wlc->chanspec)) {
		upgrd_160 = FALSE;
	} else {
		return BCME_OK;
	}

	curr_oper_mode = wlc_modesw_derive_opermode(wlc->modesw,
			bsscfg->current_bss->chanspec, bsscfg,
			wlc->stf->op_rxstreams);
	nss = DOT11_OPER_MODE_RXNSS(curr_oper_mode);
	bw = DOT11_OPER_MODE_CHANNEL_WIDTH(curr_oper_mode);
	new_oper_mode = DOT11_D8_OPER_MODE(0, nss, 0, upgrd_160, bw);
	WL_MODE_SWITCH(("wl%d Attempting %sMhz by opmode 0x%x\n",
			WLCWLUNIT(wlc), (upgrd_160) ? "160":"80",
			new_oper_mode));
	FOREACH_UP_AP(wlc, idx, bsscfg) {
		WL_MODE_SWITCH(("cfg:%d %s modesw_entry\n",
			bsscfg->_idx, __FUNCTION__));
		wlc_modesw_handle_oper_mode_notif_request(wlc->modesw,
			bsscfg, new_oper_mode, TRUE,
			MODESW_CTRL_OPMODE_IE_REQD_OVERRIDE);
	}

	return BCME_OK;
}
#endif /* WL_MODESW */

static void
wlc_ap_160mhz_upd_bw_check(wlc_info_t *wlc, struct scb *scb, uint8 scb_assoced)
{
	uint prev_160mhz_assocs;

	if (scb_assoced) {
		/* count 160 capable STAs associated to our AP (any BSS) */
		prev_160mhz_assocs = wlc->num_160mhz_assocs;
		if (scb->flags3 & (SCB3_IS_80_80 | SCB3_IS_160)) {
			wlc->num_160mhz_assocs++;
		} else {
			wlc->num_non160mhz_assocs++;
		}
	} else {
		/* count 160 capable STAs associated to our AP (any BSS) */
		prev_160mhz_assocs = wlc->num_160mhz_assocs;
		if (scb->flags3 & (SCB3_IS_80_80 | SCB3_IS_160)) {
			ASSERT(prev_160mhz_assocs > 0);
			wlc->num_160mhz_assocs--;
		} else {
			ASSERT(wlc->num_non160mhz_assocs > 0);
			wlc->num_non160mhz_assocs--;
		}
	}

#ifdef WL_MODESW
	WL_MODE_SWITCH(("wl%d %s prev_160mhz_assocs %d 160mhz_assocs %d non160mhz_assocs %d"
			" wlc->chanspec 0x%x\n",
			wlc->pub->unit, __FUNCTION__, prev_160mhz_assocs,
			wlc->num_160mhz_assocs, wlc->num_non160mhz_assocs, wlc->chanspec));
	if (WLC_BW_SWITCH_ENABLED(wlc) && BAND_5G(wlc->band->bandtype)) {
		/* Schedule a timer for a first 160Mhz client 160/80Mhz BW switch attempt.
		 * When there are no 160Mhz clients and atleast 1 80Mhz client,
		 * switch to 80Mhz chanspec.
		 * When chanspec is 80Mhz, and client is 160Mhz capable attempt switch to 160Mhz
		 */
		if ((!prev_160mhz_assocs && wlc->num_160mhz_assocs > 0 &&
				CHSPEC_IS80(wlc->chanspec)) ||
			(!wlc->num_160mhz_assocs && wlc->num_non160mhz_assocs &&
				CHSPEC_IS160(wlc->chanspec))) {
			wlc_ap_info_pvt_t* appvt = (wlc_ap_info_pvt_t*) wlc->ap;
			WL_MODE_SWITCH(("wl%d Schedule a BW switch during %s after a delay\n",
					WLCWLUNIT(wlc), scb_assoced ? "Assoc" : "Disassoc"));
			appvt->recheck_160_80 = wlc->pub->now + WLC_BW_80_160_DELAY;
		}
		if (CHSPEC_IS160(wlc->chanspec)) {
			if ((prev_160mhz_assocs > 0) && (wlc->num_160mhz_assocs == 0)) {
				phy_chanmgr_tdcs_enable_160m((phy_info_t*) wlc->pi, TRUE);
			} else if ((prev_160mhz_assocs == 0) && (wlc->num_160mhz_assocs > 0)) {
				phy_chanmgr_tdcs_enable_160m((phy_info_t*) wlc->pi, FALSE);
			}
		}
	} else
#endif /* WL_MODESW */
	if (WLC_BW_SWITCH_TDCS_EN(wlc)) {
		if ((prev_160mhz_assocs > 0) && (wlc->num_160mhz_assocs == 0) &&
				!wlc->any_sta_in_160mhz) {
			phy_chanmgr_tdcs_enable_160m((phy_info_t*) wlc->pi, TRUE);
		} else if ((prev_160mhz_assocs == 0) && (wlc->num_160mhz_assocs > 0)) {
			/* first 160 MHZ assoc */
			phy_chanmgr_tdcs_enable_160m((phy_info_t*) wlc->pi, FALSE);
		}
	}
}

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
	scb_cubby_params_t cubby_params;
	bsscfg_cubby_params_t bss_cubby_params;
	uint16 arsfstbmp = FT2BMP(FC_ASSOC_RESP) | FT2BMP(FC_REASSOC_RESP);
	uint16 arqfstbmp = FT2BMP(FC_ASSOC_REQ) | FT2BMP(FC_REASSOC_REQ);

#ifdef RXCHAIN_PWRSAVE
	char *var;
#endif /* RXCHAIN_PWRSAVE */

	appvt = (wlc_ap_info_pvt_t*)MALLOCZ(pub->osh, sizeof(wlc_ap_info_pvt_t));
	if (appvt == NULL) {
		WL_ERROR(("%s: MALLOC wlc_ap_info_pvt_t failed\n", __FUNCTION__));
		goto fail;
	}

	ap = &appvt->appub;
	ap->shortslot_restrict = FALSE;

	appvt->scb_timeout = SCB_TIMEOUT;
	appvt->scb_mark_del_timeout = SCB_MARK_DEL_TIMEOUT;
	appvt->scb_activity_time = SCB_ACTIVITY_TIME;
	appvt->scb_max_probe = SCB_GRACE_ATTEMPTS;

	appvt->wlc = wlc;
	appvt->pub = pub;
	appvt->maxassoc = wlc_ap_get_maxassoc_limit(ap);

	/* reserve cubby in the bsscfg container for per-bsscfg private data */
	bzero(&bss_cubby_params, sizeof(bss_cubby_params));
	bss_cubby_params.context = appvt;
	bss_cubby_params.fn_init = wlc_ap_bsscfg_init;
	bss_cubby_params.fn_deinit = wlc_ap_bsscfg_deinit;
	bss_cubby_params.fn_dump = wlc_ap_bsscfg_dump;
#ifdef WLRSDB
	bss_cubby_params.fn_get = wlc_ap_bss_get;
	bss_cubby_params.fn_set = wlc_ap_bss_set;
	bss_cubby_params.config_size = AP_BSSCFG_CUBBY_LEN;
#endif /* WLRSDB */

	/* reserve cubby in the bsscfg container for per-bsscfg private data */
	appvt->cfgh = wlc_bsscfg_cubby_reserve_ext(wlc, sizeof(ap_bsscfg_cubby_t),
		&bss_cubby_params);

	if (appvt->cfgh < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve() failed\n",
			wlc->pub->unit, __FUNCTION__));
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

	/* Add client callback to the scb state notification list */
	if (wlc_scb_state_upd_register(wlc, wlc_ap_scb_state_upd_cb, wlc) != BCME_OK) {
		WL_ERROR(("wl%d:%s: unable to register callback %p\n",
		          wlc->pub->unit, __FUNCTION__,
			OSL_OBFUSCATE_BUF(wlc_ap_scb_state_upd_cb)));
		goto fail;
	}

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

	pub->_bw_switch_160 = WLC_PHY_160_FULL_NSS(wlc) ? WLC_BW_SWITCH_TDCS :
			WLC_BW_SWITCH_DISABLE;

#ifdef SPLIT_ASSOC
	BCM_REFERENCE(wlc_as_scb_init);
	BCM_REFERENCE(wlc_as_scb_deinit);
	BCM_REFERENCE(wlc_as_scb_secsz);
#ifdef WLRSDB
	BCM_REFERENCE(wlc_as_scb_update);
#endif /* WLRSDB */

	/* reserve cubby in the scb container for per-scb private data */
	bzero(&cubby_params, sizeof(cubby_params));

	cubby_params.context = appvt;
	cubby_params.fn_init = wlc_as_scb_init;
	cubby_params.fn_deinit = wlc_as_scb_deinit;
	cubby_params.fn_secsz = wlc_as_scb_secsz;
#ifdef WLRSDB
	cubby_params.fn_update = wlc_as_scb_update;
#endif /* WLRSDB */

	appvt->as_scb_handle = wlc_scb_cubby_reserve_ext(wlc,
	                                                 sizeof(wlc_assoc_req_t *),
	                                                 &cubby_params);

	if (appvt->as_scb_handle < 0) {
		WL_ERROR(("%s: wlc_scb_cubby_reserve for as req failed\n", __FUNCTION__));
		goto fail;
	}
#endif /* defined(SPLIT_ASSOC) */

	/* reserve cubby in the scb container for per-scb private data */
	bzero(&cubby_params, sizeof(cubby_params));

	cubby_params.context = appvt;
	cubby_params.fn_init = wlc_ap_scb_init;
	cubby_params.fn_deinit = wlc_ap_scb_deinit;
	cubby_params.fn_secsz = wlc_ap_scb_secsz;

	appvt->ap_scb_handle = wlc_scb_cubby_reserve_ext(wlc,
	                                                 sizeof(ap_scb_cubby_t *),
	                                                 &cubby_params);

	if (appvt->ap_scb_handle < 0) {
		WL_ERROR(("%s: wlc_scb_cubby_reserve for ap failed\n", __FUNCTION__));
		goto fail;
	}

	appvt->scb_supp_chan_handle = wlc_scb_cubby_reserve(wlc,
		sizeof(wlc_supp_channels_t), NULL, NULL, NULL, appvt);

	if (appvt->scb_supp_chan_handle < 0) {
		WL_ERROR(("%s: wlc_scb_cubby_reserve failed\n", __FUNCTION__));
		goto fail;
	}
	appvt->scb_assoc_oui_handle = wlc_scb_cubby_reserve(wlc,
			sizeof(wlc_assoc_oui_t), NULL, NULL, NULL, appvt);
	if (appvt->scb_assoc_oui_handle < 0) {
		WL_ERROR(("%s: wlc_scb_cubby_reserve failed\n", __FUNCTION__));
		goto fail;
	}

	/* register packet class callback */
	err = wlc_pcb_fn_set(wlc->pcb, 0, WLF2_PCB1_STA_PRB, wlc_ap_probe_complete);
	if (err != BCME_OK) {
		WL_ERROR(("%s: wlc_pcb_fn_set err=%d\n", __FUNCTION__, err));
		goto fail;
	}

	/* Attach APPS module */
	if (wlc_apps_attach(wlc)) {
		WL_ERROR(("%s: wlc_apps_attach failed\n", __FUNCTION__));
		goto fail;
	}

	/* register IE mgmt callback */
	/* calc/build */
	if (wlc_iem_add_build_fn(wlc->iemi, FC_AUTH, DOT11_MNG_CHALLENGE_ID,
	      wlc_auth_calc_chlng_ie_len, wlc_auth_write_chlng_ie, wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_add_build_fn failed, chlng in auth\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

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
	/* auth */
	if (wlc_iem_add_parse_fn(wlc->iemi, FC_AUTH, DOT11_MNG_CHALLENGE_ID,
	                         wlc_auth_parse_chlng_ie, wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_add_parse_fn failed, chlng in auth\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

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
	if (wlc_iem_add_parse_fn_mft(wlc->iemi, arqfstbmp, DOT11_MNG_SUPP_CHANNELS_ID,
	                             wlc_assoc_parse_supp_chan_ie, wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_add_parse_fn failed, ext rates in assocreq\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	if (wlc_iem_vs_add_parse_fn_mft(wlc->iemi, arqfstbmp, WLC_IEM_VS_IE_PRIO_WPS,
	                                wlc_assoc_parse_wps_ie, wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_vs_add_parse_fn failed, wps in assocreq\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	if (wlc_iem_vs_add_parse_fn_mft(wlc->iemi, arqfstbmp, WLC_IEM_VS_IE_PRIO_BRCM_PSTA,
	                                wlc_assoc_parse_psta_ie, wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_vs_add_parse_fn failed, psta in assocreq\n",
		          wlc->pub->unit, __FUNCTION__));
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
	appvt->txbcn_timeout = WLC_AP_TXBCN_TIMEOUT;

	/* Add callback function for AP interface creation */
	if (wlc_bsscfg_iface_register(wlc, WL_INTERFACE_TYPE_AP, wlc_ap_iface_create,
		wlc_ap_iface_remove, appvt) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_iface_register() failed\n",
			WLCWLUNIT(wlc), __FUNCTION__));
		goto fail;
	}

	return (wlc_ap_info_t*)appvt;

fail:
	MODULE_DETACH_TYPECASTED((wlc_ap_info_t*)appvt, wlc_ap_detach);
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
	wlc_scb_state_upd_unregister(wlc, wlc_ap_scb_state_upd_cb, wlc);

#ifdef RADIO_PWRSAVE
	if (appvt->radio_pwrsave.timer) {
		wl_del_timer(wlc->wl, appvt->radio_pwrsave.timer);
		wl_free_timer(wlc->wl, appvt->radio_pwrsave.timer);
		appvt->radio_pwrsave.timer = NULL;
	}
#endif // endif

	/* Unregister the AP interface during detach */
	if (wlc_bsscfg_iface_unregister(wlc, WL_INTERFACE_TYPE_AP) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_iface_unregister() failed\n",
			WLCWLUNIT(wlc), __FUNCTION__));
	}

	wlc_module_unregister(pub, "ap", appvt);

	(void)wlc_module_remove_ioctl_fn(wlc->pub, (void *)ap);

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

#if defined(WLRSDB) && defined(WL_MODESW)
void
wlc_set_ap_up_pending(wlc_info_t* wlc, wlc_bsscfg_t* bsscfg, bool val)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) wlc->ap;
	ap_bsscfg_cubby_t *ap_cfg = AP_BSSCFG_CUBBY(appvt, bsscfg);
	ap_cfg->ap_up_pending = val;
}

bool
wlc_get_ap_up_pending(wlc_info_t* wlc, wlc_bsscfg_t* bsscfg)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) wlc->ap;
	ap_bsscfg_cubby_t *ap_cfg = AP_BSSCFG_CUBBY(appvt, bsscfg);
	return ap_cfg->ap_up_pending;
}
#endif /* WLRSDB && WL_MODESW */

int
wlc_ap_up(wlc_ap_info_t *ap, wlc_bsscfg_t *bsscfg)
{
	wlc_ap_info_pvt_t* appvt = (wlc_ap_info_pvt_t*)ap;
	wlc_info_t *wlc = appvt->wlc;
	wlcband_t *band;
	wlc_bss_info_t *target_bss = bsscfg->target_bss;
#if defined(BCMDBG) || defined(WLMSG_INFORM)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif // endif
#ifdef WLMCHAN
	chanspec_t chspec;
#endif // endif

#if defined(BCMDBG) || defined(BCMDBG_ERR) || defined(WLMSG_INFORM)
	char chanbuf[CHANSPEC_STR_LEN];
#endif /* BCMDBG || BCMDBG_ERR || WLMSG_INFORM */
	int ret = BCME_OK;
	chanspec_t radar_chanspec;

	/* update radio parameters from default_bss */
	/* adopt the default BSS parameters as our BSS */
	/* adopt the default BSS params as the target's BSS params */
	bcopy(wlc->default_bss, target_bss, sizeof(wlc_bss_info_t));

	/* set some values to be appropriate for AP operation */
	target_bss->bss_type = DOT11_BSSTYPE_INFRASTRUCTURE;
	target_bss->atim_window = 0;
	target_bss->capability = DOT11_CAP_ESS;
#ifdef WL11K_AP
	if (WL11K_ENAB(wlc->pub) &&
	    wlc_rrm_enabled(wlc->rrm_info, bsscfg)) {
		target_bss->capability |= DOT11_CAP_RRM;
	}
#endif /* WL11K_AP */

	bcopy((char*)&bsscfg->cur_etheraddr, (char*)&target_bss->BSSID, ETHER_ADDR_LEN);

#ifdef WLMCHAN
	/* allow P2P GO or soft-AP to run on a possible different channel */
	if (MCHAN_ENAB(wlc->pub) &&
	    !wlc_mchan_stago_is_disabled(wlc->mchan) &&
#ifdef WLP2P
	    BSS_P2P_ENAB(wlc, bsscfg) &&
#endif // endif
	    (chspec = wlc_mchan_configd_go_chanspec(wlc->mchan, bsscfg)) != 0) {
		WL_INFORM(("wl%d: use cfg->chanspec 0x%04x in BSS %s\n", wlc->pub->unit,
		          chspec, bcm_ether_ntoa(&target_bss->BSSID, eabuf)));
		target_bss->chanspec = chspec;
		/* continue to validate the chanspec (channel and channel width)...
		 * they may become invalid due to other user configurations happened
		 * between GO bsscfg creation and now...
		 */
	} else
#endif /* WLMCHAN */
	/* use the current operating channel if any */
	if (wlc->pub->associated &&
#ifdef WLRSDB
		(((RSDB_ENAB(wlc->pub)) &&
		(wlc->band->bandtype == CHSPEC2WLC_BAND(target_bss->chanspec))) ||
		(!(RSDB_ENAB(wlc->pub)))) &&
#endif // endif
	    TRUE) {
#if defined(WL_RESTRICTED_APSTA)
		/* Do not try SCC in 5G if channel is not 149 */
		if (RAPSTA_ENAB(wlc->pub) && wlc->stas_associated &&
			wlc_channel_apsta_restriction(wlc->cmi, wlc->home_chanspec,
				target_bss->chanspec)) {
			WL_INFORM(("wl%d: AP in 5G channel %d STA in channel %d.."
				" Push AP in 2G\n", WLCWLUNIT(wlc),
				CHSPEC_CHANNEL(target_bss->chanspec),
				CHSPEC_CHANNEL(wlc->home_chanspec)));
			/* Force 2G operation for the AP */
			target_bss->chanspec = wlc_default_chanspec_by_band(wlc->cmi,
				BAND_2G_INDEX);
			wlc->default_bss->chanspec = target_bss->chanspec;
			if (target_bss->chanspec == INVCHANSPEC) {
				return BCME_ERROR;
			}
		} else if (RAPSTA_ENAB(wlc->pub) && !wlc->stas_associated &&
				wlc_get_ap_up_pending(wlc, bsscfg)) {
			/* The chanspec for this AP is already available */
			return BCME_OK;
		} else
#endif /* WL_RESTRICTED_APSTA */
		{
			/* AP is being brought up in same channel/band as STA */
			WL_INFORM(("wl%d: share wlc->home_chanspec 0x%04x in BSS %s\n",
			           wlc->pub->unit, wlc->home_chanspec,
			           bcm_ether_ntoa(&target_bss->BSSID, eabuf)));
			target_bss->chanspec = wlc->home_chanspec;
			goto sradar_check;
		}
	}
#if defined(WL_RESTRICTED_APSTA)
	else if (RAPSTA_ENAB(wlc->pub) &&
		wlc_get_ap_up_pending(wlc, bsscfg)) {
		/* The AP is pending to be brought up.
		* So dont derive new chanspec for it.
		* It will already have a valid chanspec.
		*/
		return BCME_OK;
	}
#endif /* WL_RESTRICTED_APSTA */

#ifdef RADAR
	/* use dfs selected channel */
	/* XXX should we ask the dfs to select a channel only when
	 * the target_bss->chanspec is configured on a DFS channel?
	 * also should we let the dfs to validate both channel and
	 * channel width?
	 */
	if (RADAR_ENAB(wlc->pub) && WL11H_AP_ENAB(wlc) &&
			CHSPEC_IS5G(target_bss->chanspec) &&
	    !(BSSCFG_SRADAR_ENAB(bsscfg) ||
	      BSSCFG_AP_NORADAR_CHAN_ENAB(bsscfg) ||
	      BSSCFG_AP_USR_RADAR_CHAN_SELECT(bsscfg))) {
		/* if chanspec_next is uninitialized, e.g. softAP with
		 * DFS_TPC country, pick a random one like wlc_restart_ap()
		 */
		if (appvt->chanspec_selected)
			target_bss->chanspec = appvt->chanspec_selected;
		else
			target_bss->chanspec = wlc_dfs_sel_chspec(wlc->dfs, FALSE, bsscfg);
		WL_INFORM(("wl%d: use dfs chanspec 0x%04x in BSS %s\n",
		           wlc->pub->unit, target_bss->chanspec,
		           bcm_ether_ntoa(&target_bss->BSSID, eabuf)));
	}
#endif /* RADAR */

	radar_chanspec = wlc_radar_chanspec(wlc->cmi, target_bss->chanspec);

	/* validate or fixup default channel value */
	/* also, fixup for GO if radar channel */
	if (!wlc_valid_chanspec_db(wlc->cmi, target_bss->chanspec) ||
	    wlc_restricted_chanspec(wlc->cmi, target_bss->chanspec) ||
#ifdef WLP2P
	   (BSS_P2P_ENAB(wlc, bsscfg) &&
	    BSS_11H_AP_NORADAR_CHAN_ENAB(wlc, bsscfg) &&
	    radar_chanspec != 0) ||
#endif // endif
	    FALSE) {
		chanspec_t chspec_local = wlc_default_chanspec(wlc->cmi, FALSE);
		if ((chspec_local == INVCHANSPEC) ||
		    wlc_restricted_chanspec(wlc->cmi, chspec_local)) {
			WL_ERROR(("wl%d: cannot create BSS on chanspec %s\n",
				wlc->pub->unit,
				wf_chspec_ntoa_ex(target_bss->chanspec, chanbuf)));
			ret = BCME_BADCHAN;
			goto exit;
		}
		target_bss->chanspec = chspec_local;
		WL_INFORM(("wl%d: use default chanspec %s in BSS %s\n",
			wlc->pub->unit, wf_chspec_ntoa_ex(chspec_local, chanbuf),
			bcm_ether_ntoa(&target_bss->BSSID, eabuf)));
	}

	/* Validate the channel bandwidth */
	band = wlc->bandstate[CHSPEC_IS2G(target_bss->chanspec) ? BAND_2G_INDEX : BAND_5G_INDEX];
	if (CHSPEC_IS40(target_bss->chanspec) &&
	    (!N_ENAB(wlc->pub) ||
	     (wlc_channel_locale_flags_in_band(wlc->cmi, band->bandunit) & WLC_NO_40MHZ) ||
	     !WL_BW_CAP_40MHZ(band->bw_cap))) {
		uint channel = wf_chspec_ctlchan(target_bss->chanspec);
		target_bss->chanspec = CH20MHZ_CHSPEC(channel);
		WL_INFORM(("wl%d: use 20Mhz channel width in BSS %s\n",
		           wlc->pub->unit, bcm_ether_ntoa(&target_bss->BSSID, eabuf)));
	}

sradar_check:
	radar_chanspec = wlc_radar_chanspec(wlc->cmi, target_bss->chanspec);

	/* for softap and extap, following special radar rules */
	/* return bad channel error if radar channel */
	/* when no station associated */
	/* won't allow soft/ext ap to be started on radar channel */
	if (BSS_11H_SRADAR_ENAB(wlc, bsscfg) &&
	    radar_chanspec != 0 &&
#ifdef STA
	    !wlc->stas_associated &&
#endif // endif
	    TRUE) {
		WL_ERROR(("no assoc STA and starting soft or ext AP on radar chanspec %s\n",
		          wf_chspec_ntoa_ex(target_bss->chanspec, chanbuf)));
		ret = BCME_BADCHAN;
		goto exit;
	}

	/* for softap and extap with AP_NORADAR_CHAN flag set, don't allow
	 * bss to start if on a radar channel.
	 */
	if (BSS_11H_AP_NORADAR_CHAN_ENAB(wlc, bsscfg) &&
	    radar_chanspec != 0) {
		WL_ERROR(("AP_NORADAR_CHAN flag set, disallow ap on radar chanspec %s\n",
		          wf_chspec_ntoa_ex(target_bss->chanspec, chanbuf)));
		ret = BCME_BADCHAN;
		goto exit;
	}

exit:

	return ret;
}

int
wlc_ap_down(wlc_ap_info_t *ap, wlc_bsscfg_t *bsscfg)
{
	wlc_ap_info_pvt_t* appvt = (wlc_ap_info_pvt_t*) ap;
	wlc_info_t *wlc = appvt->wlc;
#ifdef WDS
	wlc_if_t *wlcif;
#endif // endif
	int callback = 0; /* Need to fix this timer callback propagation; error prone right now */
	struct scb_iter scbiter;
	struct scb *scb;
	int assoc_scb_count = 0;

#if defined(WL11K_AP)
		wlc_rrm_del_pilot_timer(wlc, bsscfg);
#endif // endif
#ifdef WLMCNX
	if (MCNX_ENAB(wlc->pub)) {
		wlc_mcnx_bss_upd(wlc->mcnx, bsscfg, FALSE);
	}
#endif // endif

	wlc_ap_timeslot_unregister(bsscfg);

	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
		if (!SCB_LEGACY_WDS(scb)) {
			if (SCB_ASSOCIATED(scb)) {
				assoc_scb_count++;
			}
			wlc_scbfree(wlc, scb);
		}
#ifdef WDS
		else if ((wlcif = SCB_WDS(scb)) != NULL) {
			/* send WLC_E_LINK event with status DOWN to WDS interface */
			wlc_wds_create_link_event(wlc, wlcif->u.scb, FALSE);
		}
#endif // endif
	}

	if (assoc_scb_count) {
		/* send up a broadcast deauth mac event if there were any
		 * associated STAs
		 */
		wlc_deauth_complete(wlc, bsscfg, WLC_E_STATUS_SUCCESS, &ether_bcast,
		                    DOT11_RC_DEAUTH_LEAVING, 0);
	}
	/* adjust associated state(s) accordingly */
	wlc_suspend_mac_and_wait(wlc);
	wlc_ap_up_upd(ap, bsscfg, FALSE);
	/* Fix up mac control */
	wlc_macctrl_init(wlc, NULL);
	wlc_enable_mac(wlc);
	wlc_txqueue_end(wlc, bsscfg, NULL);

	WL_APSTA_UPDN(("Reporting link down on config %d (AP disabled)\n",
		WLC_BSSCFG_IDX(bsscfg)));

	if (!BSSCFG_IS_RSDB_CLONE(bsscfg)) {
		wlc_link(wlc, FALSE, &bsscfg->cur_etheraddr, bsscfg, WLC_E_LINK_BSSCFG_DIS);
	}

#ifdef RADIO_PWRSAVE
	wlc_reset_radio_pwrsave_mode(ap);
#endif // endif

#ifdef WLTPC
	/* reset per bss link margin values */
	wlc_ap_bss_tpc_setup(wlc->tpc, bsscfg);
#endif // endif

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

#ifdef STA
	if (wlc->pub->up && !AP_ACTIVE(wlc)) {
		wlc_resync_sta(wlc);
	}
#endif // endif

	return callback;
}

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
		if (!SCB_ASSOCIATED(scb) || SCB_MARKED_DEL(scb))
			continue;

		if (WSEC_ENABLED(cfg->wsec) && !SCB_AUTHORIZED(scb) &&
			!(scb->wsec == WEP_ENABLED)) {
			scb->wsec_auth_timeout++;
			if (scb->wsec_auth_timeout > SCB_AUTH_TIMEOUT) {
				scb->wsec_auth_timeout = 0;
				ea  = bcm_ether_ntoa(&scb->ea, eabuf);
				BCM_REFERENCE(ea);
				WL_ERROR(("wl%d: authentication delay, send deauth to sta %s\n",
						wlc->pub->unit, ea));
				wlc_scb_set_auth(wlc, cfg, scb, FALSE, AUTHENTICATED,
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
		ap_scb_cubby_t *ap_scb = AP_SCB_CUBBY(appvt, scb);

		/* Don't age out the permanent SCB and AP SCB */
		if (scb->permanent ||
		    cfg == NULL || !BSSCFG_AP(cfg))
			continue;

		/* kill any other band scb */
		if (TRUE &&
#ifdef WLMCHAN
		    !MCHAN_ENAB(wlc->pub) &&
#endif // endif
		    wlc_scbband(wlc, scb) != wlc->band) {
			wlc_scbfree(wlc, scb);
			continue;
		}

		if ((SCB_MARKED_DEL(scb)) &&
			((wlc->pub->now - scb->used) >= appvt->scb_mark_del_timeout)) {
			wlc_scbfree(wlc, scb);
			continue;
		 }

		/* probe associated stas if idle for scb_activity_time or reprobe them */
		if (SCB_ASSOCIATED(scb) &&
			((appvt->scb_activity_time &&
				((wlc->pub->now - scb->used) >= appvt->scb_activity_time)) ||
				(appvt->reprobe_scb && ap_scb->grace_attempts))) {
			wlc_ap_sta_probe(ap, scb);
		}

		/* Authenticated but idle for long time free it now */
		if ((scb->state == AUTHENTICATED) &&
		    ((wlc->pub->now - scb->used) >= SCB_LONG_TIMEOUT)) {
			/* Notify HOST with a Deauth, this will ensure
			 * clearing any obsolete STA entry created
			 * during previous association of the SCB
			 */
			wlc_deauth_complete(wlc, cfg, WLC_E_STATUS_SUCCESS,
					&scb->ea, DOT11_RC_INACTIVITY, 0);
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

void
wlc_ap_authresp(wlc_ap_info_t *ap, wlc_bsscfg_t *bsscfg,
	struct dot11_management_header *hdr,  uint8 *body, uint body_len,
	void *p, bool short_preamble, d11rxhdr_t *rxh)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;
	wlc_info_t *wlc = appvt->wlc;
	struct dot11_auth *auth;
	struct scb *scb = NULL;
	uint16 auth_alg, auth_seq;
	uint16 status = DOT11_SC_SUCCESS;
	int i;
	wlc_key_id_t key_id;
	uint16 fc;
	int err;
	wlc_iem_upp_t upp;
	wlc_iem_ft_pparm_t ftpparm;
	wlc_iem_pparm_t pparm;
	wlc_key_t *key;
	bool mac_mode_deny_client = FALSE;
#ifdef WL_SAE
	uint8 *body_start = body;
	uint body_len_start = body_len;
#endif // endif

#if defined(BCMDBG) || defined(BCMDBG_ERR) || defined(WLMSG_ASSOC) || \
	defined(WLMSG_BTA)
	char eabuf[ETHER_ADDR_STR_LEN], *sa;
#endif // endif

	WL_TRACE(("wl%d: wlc_authresp_ap\n", WLCWLUNIT(wlc)));

#if defined(BCMDBG) || defined(BCMDBG_ERR) || defined(WLMSG_ASSOC) || \
	defined(WLMSG_BTA)
	sa = bcm_ether_ntoa(&hdr->sa, eabuf);
#endif // endif

	auth = (struct dot11_auth *) body;

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
	/* check for tkip countermesures */
	if (BSSCFG_AP(bsscfg) && WSEC_TKIP_ENABLED(bsscfg->wsec) &&
		wlc_keymgmt_tkip_cm_enabled(wlc->keymgmt, bsscfg))
		return;
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
		ap_scb_cubby_t *ap_scb;

		/* frame is protected, assume shared key */
		auth_alg = DOT11_SHARED_KEY;
		auth_seq = 3;

		/* Check if it is logical to have an encrypted packet here */
		if ((scb = wlc_scbfind(wlc, bsscfg, (struct ether_addr *) &hdr->sa)) == NULL) {
			WL_ERROR(("wl%d: %s: auth resp frame encrypted"
				  "from unknown STA %s\n", wlc->pub->unit, __FUNCTION__, sa));
			status = SMFS_CODE_MALFORMED;
			goto smf_stats;
		}

		ap_scb = AP_SCB_CUBBY(appvt, scb);
		ASSERT(ap_scb != NULL);

		if (ap_scb->challenge == NULL) {
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

		WLPKTTAGSCBSET(p, scb);

		status = DOT11_SC_AUTH_CHALLENGE_FAIL;
		key_id = body[3] >> DOT11_KEY_INDEX_SHIFT;
		key = wlc_keymgmt_get_bss_key(wlc->keymgmt, bsscfg, key_id, NULL);

		/* if the frame was incorrectly decrypted with default bss by h/w, rx
		 * processing will encrypt it back and find the right key and
		 * decrypt it.
		 */
		err = wlc_key_rx_mpdu(key, p, rxh);
		if (err != BCME_OK) {
			WL_ASSOC(("wl%d.%d: %s: rx from %s failed with error %d\n",
				WLCWLUNIT(wlc),  WLC_BSSCFG_IDX(bsscfg),
				__FUNCTION__, sa, err));

			/* free the challenge text */
			MFREE(wlc->osh, ap_scb->challenge, 2 + ap_scb->challenge[1]);
			ap_scb->challenge = NULL;
			goto send_result;
		}

		WL_ASSOC(("wl%d: %s: ICV pass : %s: BSSCFG = %d\n",
			WLCWLUNIT(wlc), __FUNCTION__, sa, WLC_BSSCFG_IDX(bsscfg)));

		/* Skip IV */
		body += DOT11_IV_LEN;
		body_len -= DOT11_IV_LEN;

		/* Remove ICV */
		body_len -= DOT11_ICV_LEN;
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
			int idx;
			wlc_bsscfg_t *cfg;

			/* Use band 1 for single band 11a */
			if (IS_SINGLEBAND_5G(wlc->deviceid, wlc->phy_cap))
				i = BAND_5G_INDEX;

			FOREACH_BSS(wlc, idx, cfg) {
				scb = wlc_scbfindband(wlc, cfg, (struct ether_addr *)&hdr->sa, i);
				if (scb && (auth_seq == 1)) {
					WL_ASSOC(("wl%d: %s: scb for the STA-%s already exists"
						"\n", wlc->pub->unit, __FUNCTION__, sa));
					/* Notify HOST with a Deauth, this will ensure
					 * clearing any flow-rings (PCIe case) created
					 * during previous association of the SCB
					 */
					wlc_ap_authresp_deauth(wlc, scb);
					/* call early to cleanup any pkts in common q
					 * for scb before wlc_scbfree; then record
					 * remaining pktpend cnt.
					 */
					WLPKTTAGSCBSET(p, NULL);
					wlc_scbfree(wlc, scb);
					scb = NULL;
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
			}
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
				WL_ERROR(("wl%d: %s: Reject auth request from %s suppressed"
					" by MAC filter\n", wlc->pub->unit, __FUNCTION__, sa));
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

		/* auth_alg is coming from the STA, not us */
		scb->auth_alg = (uint8)auth_alg;
		switch (auth_alg) {
		case DOT11_OPEN_SYSTEM:

			if ((WLC_BSSCFG_AUTH(bsscfg) == DOT11_OPEN_SYSTEM) && (!status)) {
				wlc_scb_setstatebit(wlc, scb, AUTHENTICATED);
			} else {
				wlc_scb_clearstatebit(wlc, scb, AUTHENTICATED);
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
	if (scb && bsscfg->WPA_auth & WPA3_AUTH_SAE_PSK && auth_alg == DOT11_SAE) {
		/* sae_parse_auth will send auth frame to authenticator. */
		status = wlc_sae_parse_auth(bsscfg, hdr, body_start, body_len_start);
		if (status == DOT11_SC_SUCCESS) {
			/* set scb state to PENDING_AUTH */
			wlc_scb_setstatebit(wlc, scb, PENDING_AUTH);
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
	wlc_iem_parse_upp_init(wlc->iemi, &upp);
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

#ifdef AP
	if (scb && (status == DOT11_SC_SUCCESS) && (ftpparm.auth.alg == DOT11_FAST_BSS) &&
		SCB_AUTHENTICATING(scb)) {
#ifdef RXCHAIN_PWRSAVE
		wlc_reset_rxchain_pwrsave_mode(ap);
#endif /* RXCHAIN_PWRSAVE */
		goto smf_stats;
	}
#endif /* AP */
	if (scb != NULL) {
		wlc_ap_sendauth(wlc->ap, bsscfg, scb, auth_alg, auth_seq + 1, status, NULL,
			short_preamble, TRUE);
	} else {
		if (mac_mode_deny_client && !scb) {
			scb = wlc->band->hwrs_scb;
			status = DOT11_SC_INSUFFICIENT_BANDWIDTH;
			wlc_sendauth(bsscfg, &hdr->sa, &bsscfg->BSSID, scb, auth_alg, auth_seq +1,
				status, NULL, short_preamble, wlc_ap_pre_sendauth_cb, scb, NULL,
				NULL);
		}
	}

smf_stats:
	WL_ASSOC_LT(("wlc_ap_authresp: status %d\n", status));
	if (BSS_SMFS_ENAB(wlc, bsscfg)) {
		(void)wlc_smfs_update(wlc->smfs, bsscfg, SMFS_TYPE_AUTH, status);
	}

#if defined(BCMHWA) && defined(HWA_RXFILL_BUILD)
	/* Update auth Rx timestamp.
	 * Ctrl/Mgmt packets are from fifo-2 and wl dpc handle it but
	 * Data packets are handled by hwa dpc.  So SW may process a
	 * NULL data packet between auth and assoc which cause SW
	 * to send disassoc to the station.
	 * WAR: We drop the stall data packets between scb auth and assoc state.
	 */
	if (scb != NULL) {
		scb->rx_auth_tsf = (uint16)D11RXHDR_GE128_ACCESS_VAL(rxh, RxTsfTimeH);
		scb->rx_auth_tsf = scb->rx_auth_tsf << 16;
		scb->rx_auth_tsf |= (uint16)D11RXHDR_GE128_ACCESS_VAL(rxh, RxTSFTime);
	}
#endif // endif
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
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;
	wlc_info_t *wlc = appvt->wlc;

	BCM_REFERENCE(wlc);

	if (BSS_SMFS_ENAB(wlc, bsscfg)) {
		bool reassoc;
		uint8 type;

		reassoc = ((ltoh16(hdr->fc) & FC_KIND_MASK) == FC_REASSOC_REQ);
		type = (reassoc ? SMFS_TYPE_REASSOC : SMFS_TYPE_ASSOC);
		(void)wlc_smfs_update(wlc->smfs, bsscfg, type, param->status);
	}
}

/* verify if the STA capabilities meet the BSS requirements */
static bool
wlc_ap_bss_membership_verify(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct scb *scb)
{
	wlc_mbsp_sel_t mbsp_req;

	if ((mbsp_req = wlc_bss_membership_get(wlc, cfg)) == 0) {
		return TRUE;
	}

	if ((mbsp_req & WLC_MBSP_SEL_HT) && !SCB_HT_CAP(scb)) {
		return FALSE;
	}
	if ((mbsp_req & WLC_MBSP_SEL_VHT) && !SCB_VHT_CAP(scb)) {
		return FALSE;
	}
	if ((mbsp_req & WLC_MBSP_SEL_HE) && !SCB_HE_CAP(scb)) {
		return FALSE;
	}

	return TRUE;
}

static bool
wlc_ap_assreq_verify_rates(wlc_assoc_req_t *param, wlc_iem_ft_pparm_t *ftpparm,
	opmode_cap_t opmode_cap_reqd, uint16 capability, wlc_rateset_t *req_rates)
{
	struct scb *scb = param->scb;
	wlc_bsscfg_t *bsscfg = param->bsscfg;
	wlc_info_t *wlc = bsscfg->wlc;
	wlc_bss_info_t *current_bss = bsscfg->current_bss;
	opmode_cap_t opmode_cap_curr = OMC_NONE;
	bool erp_sta;
	uint8 req_rates_lookup[WLC_MAXRATE+1];
	uint i;
	uint8 r;
#if defined(BCMDBG_ERR) || defined(WLMSG_ASSOC)
	struct dot11_management_header *hdr = param->hdr;
	char eabuf[ETHER_ADDR_STR_LEN], *sa = bcm_ether_ntoa(&hdr->sa, eabuf);
#endif // endif

	/* get the requester's rates into a lookup table and record ERP capability */
	erp_sta = FALSE;
	bzero(req_rates_lookup, sizeof(req_rates_lookup));
	for (i = 0; i < req_rates->count; i++) {
		r = req_rates->rates[i] & RATE_MASK;
		if ((r > WLC_MAXRATE) || (rate_info[r] == 0)) {
			continue;
		}
		req_rates_lookup[r] = r;
		if (RATE_ISOFDM(r))
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

	/* check the required rates */
	for (i = 0; i < current_bss->rateset.count; i++) {
		/* check if the rate is required */
		r = current_bss->rateset.rates[i];
		if (r & WLC_RATE_FLAG) {
			if (req_rates_lookup[r & RATE_MASK] == 0) {
				/* a required rate was not available */
				WL_ERROR(("wl%d: %s does not support required rate %d\n",
				        wlc->pub->unit, sa, r & RATE_MASK));
				return FALSE;
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
		if (ftpparm->assocreq.ht_cap_ie != NULL) {
			ht_cap_p = wlc_read_ht_cap_ie(wlc, ftpparm->assocreq.ht_cap_ie,
			        TLV_HDR_LEN + ftpparm->assocreq.ht_cap_ie[TLV_LEN_OFF]);

			wlc_ht_update_scbstate(wlc->hti, scb, ht_cap_p, NULL, NULL);
			if (ht_cap_p != NULL) {
				bcopy(ht_cap_p->supp_mcs, req_rates->mcs, MCSSET_LEN);

				/* verify mcs basic rate settings */
				if (wlc_ap_verify_basic_mcs(wlc->ap,
					req_rates->mcs, MCSSET_LEN) != BCME_OK) {
						/* a required rate was not available */
						WL_ERROR(("wl%d: %s not support required mcs\n",
							wlc->pub->unit, sa));
						return FALSE;
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

			if (ftpparm->assocreq.vht_cap_ie != NULL)
				vht_cap_p = wlc_read_vht_cap_ie(wlc->vhti,
					ftpparm->assocreq.vht_cap_ie,
				        TLV_HDR_LEN + ftpparm->assocreq.vht_cap_ie[TLV_LEN_OFF],
				        &vht_cap);
			if (ftpparm->assocreq.vht_op_ie != NULL)
				vht_op_p =  wlc_read_vht_op_ie(wlc->vhti,
					ftpparm->assocreq.vht_op_ie,
				        TLV_HDR_LEN + ftpparm->assocreq.vht_op_ie[TLV_LEN_OFF],
				        &vht_op);

			wlc_vht_update_scb_state(wlc->vhti, wlc->band->bandtype, scb, ht_cap_p,
			                vht_cap_p, vht_op_p, ftpparm->assocreq.vht_ratemask);

			if (vht_cap_p != NULL)
				opmode_cap_curr = OMC_VHT;
		}
#endif /* WL11AC */
#ifdef WL11AX
		if (HE_ENAB_BAND(wlc->pub, wlc->band->bandtype)) {
			wlc_he_update_scb_state(wlc->hei, wlc->band->bandtype, scb,
				(he_cap_ie_t *)	ftpparm->assocreq.he_cap_ie, NULL);
		}
			if (SCB_HE_CAP(scb))
				opmode_cap_curr = OMC_HE;
#endif /* WL11AX */
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
		return FALSE;
	}

	/* verify if the STA meets bss membership selector requirements */
	if (!wlc_ap_bss_membership_verify(wlc, bsscfg, scb)) {
		WL_ASSOC(("wl%d: mismatch Membership from (Re)Assoc Request packet from %s\n",
		          wlc->pub->unit, sa));
		return FALSE;
	}

	return TRUE;
}

static bool
wlc_ap_assreq_verify_authmode(wlc_info_t *wlc, struct scb *scb, wlc_bsscfg_t *bsscfg,
	uint16 capability, bool akm_ie_included, struct dot11_management_header *hdr)
{

#if defined(BCMDBG_ERR)
	char eabuf[ETHER_ADDR_STR_LEN], *sa = bcm_ether_ntoa(&hdr->sa, eabuf);
#endif // endif
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
				return FALSE;
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
		wlc_bss_mac_event(wlc, bsscfg, WLC_E_PRUNE, &hdr->sa,
			0, WLC_E_PRUNE_ENCR_MISMATCH, 0, 0, 0);
		return FALSE;
	}
	return TRUE;
}

/* respond to association and reassociation requests */
void
wlc_ap_process_assocreq(wlc_ap_info_t *ap, wlc_bsscfg_t *bsscfg,
	wlc_rx_data_desc_t *rx_data_desc, struct scb *scb, bool short_preamble)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;
	wlc_info_t *wlc = appvt->wlc;
	ap_bsscfg_cubby_t *ap_cfg;
	struct dot11_management_header *hdr = rx_data_desc->hdr;
#ifdef WL_OCE_AP
	wlc_d11rxhdr_t *wrxh = rx_data_desc->wrxh;
#endif // endif
	uint8 *body = rx_data_desc->body;
	uint body_len = rx_data_desc->body_len;
#if defined(BCMDBG_ERR) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN], *sa = bcm_ether_ntoa(&hdr->sa, eabuf);
#endif // endif
	struct dot11_assoc_req *req = (struct dot11_assoc_req *) body;
	wlc_rateset_t req_rates;
	wlc_rateset_t sup_rates, ext_rates;
	bool reassoc;
	uint16 capability;
	uint16 status = DOT11_SC_SUCCESS;
	int idx;
	assoc_decision_t decision;
	wlc_assoc_req_t param_buf;
	wlc_assoc_req_t * param;
	/* Used to encapsulate data to a generate event */
	bool akm_ie_included = FALSE;
	uint16 type;
	opmode_cap_t opmode_cap_reqd;
	wlc_iem_upp_t upp;
	wlc_iem_pparm_t pparm;
	wlc_iem_ft_pparm_t ftpparm;
	uint8 *tlvs;
	uint tlvs_len;
	ap_scb_cubby_t *ap_scb;
	wlc_supp_channels_t supp_chan;
	wlc_supp_channels_t *supp_chan_cubby = NULL;
	bool free_scb = FALSE;
	wlc_assoc_oui_t     *assoc_oui_cubby = NULL;
	wlc_assoc_oui_t     assoc_oui;

	if (scb == NULL)
		return;

	ap_scb = AP_SCB_CUBBY(appvt, scb);

	WL_TRACE(("wl%d: %s\n", wlc->pub->unit, __FUNCTION__));

	ASSERT(bsscfg != NULL);
	ASSERT(bsscfg->up);
	ASSERT(BSSCFG_AP(bsscfg));
	ASSERT(ap_scb != NULL);

	ap_cfg = AP_BSSCFG_CUBBY(appvt, bsscfg);
	opmode_cap_reqd = ap_cfg->opmode_cap_reqd;

	bzero(&decision, sizeof(assoc_decision_t));
	param = &param_buf;
#ifdef SPLIT_ASSOC
	if (SPLIT_ASSOC_REQ(bsscfg)) {
		param = AS_SCB_CUBBY(appvt, scb);

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
#endif /* SPLIT_ASSOC */
	bzero(param, sizeof(wlc_assoc_req_t));
	param->hdr = hdr;
	param->body = body;
#ifdef SPLIT_ASSOC
	if (SPLIT_ASSOC_REQ(bsscfg)) {
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
#endif /* SPLIT_ASSOC */
	param->ap = ap;
	param->bsscfg = bsscfg;
	param->body_len = body_len;
	param->scb = scb;
	param->short_preamble = short_preamble;
	param->aid = scb->aid;

#ifdef RXCHAIN_PWRSAVE
	/* fast switch back from rxchain_pwrsave state upon association */
	wlc_reset_rxchain_pwrsave_mode(ap);
#endif /* RXCHAIN_PWRSAVE */

	bzero(&req_rates, sizeof(req_rates));

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
	if (AP_ENAB(wlc->pub) && WSEC_TKIP_ENABLED(bsscfg->wsec) &&
		wlc_keymgmt_tkip_cm_enabled(wlc->keymgmt, bsscfg))
		return;
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

	bzero(&assoc_oui, sizeof(wlc_assoc_oui_t));
	/* prepare IE mgmt parse calls */
	wlc_iem_parse_upp_init(wlc->iemi, &upp);
	bzero(&ftpparm, sizeof(ftpparm));
	ftpparm.assocreq.sup = &sup_rates;
	ftpparm.assocreq.ext = &ext_rates;
	ftpparm.assocreq.scb = scb;
	ftpparm.assocreq.status = status;
	ftpparm.assocreq.supp_chan = &supp_chan;
	ftpparm.assocreq.assoc_oui = &assoc_oui;
	bzero(&pparm, sizeof(pparm));
	pparm.ft = &ftpparm;
	pparm.ht = N_ENAB(wlc->pub);
#ifdef WL11AC
	pparm.vht = VHT_ENAB(wlc->pub);
#endif // endif
#ifdef WL11AX
	pparm.he = HE_ENAB(wlc->pub);
#endif // endif

	/* parse IEs */
	if (wlc_iem_parse_frame(wlc->iemi, bsscfg, type, &upp, &pparm,
	                        tlvs, tlvs_len) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_parse_frame failed\n",
		          wlc->pub->unit, __FUNCTION__));
		status = ftpparm.assocreq.status;

		if (status != DOT11_SC_SUCCESS) {
			/* Send an association response */
			goto done;
		}
	}
	status = ftpparm.assocreq.status;

	ASSERT(bsscfg == scb->bsscfg);

	/* store the advertised capability field */
	scb->cap = capability;

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
	assoc_oui_cubby = SCB_ASSOC_OUI_CUBBY(appvt, scb);
	if (assoc_oui_cubby) {
		bzero(assoc_oui_cubby, sizeof(wlc_assoc_oui_t));
		bcopy(&assoc_oui, assoc_oui_cubby,
				sizeof(wlc_assoc_oui_t));
	}

	/* qualify WPS IE */
	if (ap_scb->wpaie != NULL && !(scb->flags & SCB_WPSCAP))
		akm_ie_included = TRUE;

	param->akm_ie_included = akm_ie_included;

	/* ===== Assoc Request Validations Start Here ===== */

#ifdef WL_OCE_AP
	if (OCE_ENAB(wlc->pub) &&
		(wlc_oce_if_valid_assoc(wlc->oce, &wrxh->rssi, &decision.reject_data) ==
			OCE_ASSOC_REJECT_RC_INSUFFICIENT_RSSI)) {

		WL_ASSOC(("wl%d: OCE_AP: *** Reject Association due to insufficient RSSI (%d) "
			"by delta(%d)\n", wlc->pub->unit,
			wrxh->rssi, decision.reject_data));
		status = OCE_ASSOC_REJECT_RC_INSUFFICIENT_RSSI;
		decision.assoc_approved = FALSE;
		decision.reject_reason = status;
		bcopy(&scb->ea, &decision.da, ETHER_ADDR_LEN);
		goto done;
	}
#endif /* WL_OCE_AP */

	/* rates validation */
	if (wlc_combine_rateset(wlc, &sup_rates, &ext_rates, &req_rates) != BCME_OK ||
	    /* filter rateset by hwrs rates - also sort and sanitize them */
	    !wlc_rate_hwrs_filter_sort_validate(&req_rates /* [in+out] */,
			&wlc->band->hw_rateset /* [in] */, FALSE, wlc->stf->op_txstreams)) {
		WL_ASSOC(("wl%d: invalid rateset in (Re)Assoc Request "
		          "packet from %s\n", wlc->pub->unit, sa));
		status = DOT11_SC_ASSOC_RATE_MISMATCH;
		goto done;
	}
#ifdef WL11AX
	/* HE has its intersected rates stored in scb. Copy in req_rates so they dont get lost */
	wlc_rateset_he_cp(&req_rates, &scb->rateset);
#endif /* WL11AX */

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
			wlc_scb_clearstatebit(wlc, scb, ~AUTHENTICATED);
		}
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
		free_scb = TRUE;
		goto done;
	}

	if (!SCB_AUTHENTICATED(scb)) {
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
#ifdef WL11K_AP
	if (WL11K_ENAB(wlc->pub) && wlc_rrm_enabled(wlc->rrm_info, bsscfg)) {
		scb->flags3 &= ~SCB3_RRM;
		if (capability & DOT11_CAP_RRM)
			scb->flags3 |= SCB3_RRM;
	}
#endif /* WL11K_AP */

	/* get the requester's rates into a lookup table and record ERP capability */
	if (!wlc_ap_assreq_verify_rates(param, &ftpparm, opmode_cap_reqd,
		capability, &req_rates)) {
		status = DOT11_SC_ASSOC_RATE_MISMATCH;
		goto done;
	}
	/*
	 * If in an auth mode that wants a WPA info element, look for one.
	 * If found, check to make sure what it requests is supported.
	 */

	if (!wlc_ap_assreq_verify_authmode(wlc, scb, bsscfg, capability, akm_ie_included, hdr)) {
		status = DOT11_SC_ASSOC_FAIL;
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
	if (wlc_ap_stas_associated(wlc->ap) >= appvt->maxassoc) {
		WL_ERROR(("wl%d: %s denied association due to max association limit\n",
		          wlc->pub->unit, sa));
		status = DOT11_SC_ASSOC_BUSY_FAIL;
		free_scb = TRUE;
		goto done;
	}

	if ((SCB_MAP_CAP(scb) || SCB_DWDS_CAP(scb)) && wl_max_if_reached(wlc->wl)) {
		WL_ERROR(("wl%d: %s denied DWDS association due to max interface limit\n",
		          wlc->pub->unit, sa));
		status = DOT11_SC_ASSOC_BUSY_FAIL;
		free_scb = TRUE;
		goto done;
	}

#if defined(MBSS) || defined(WLP2P)
	if (wlc_bss_assocscb_getcnt(wlc, bsscfg) >= bsscfg->maxassoc) {
		WL_ERROR(("wl%d.%d %s denied association due to max BSS association "
		          "limit\n", wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), sa));
		status = DOT11_SC_ASSOC_BUSY_FAIL;
		free_scb = TRUE;
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

#if defined(WL_MBO) && defined(MBO_AP)
	if ((BSS_MBO_ENAB(wlc, bsscfg)) && (wlc_mbo_reject_assoc_req(wlc, bsscfg))) {
		status = DOT11_SC_ASSOC_BUSY_FAIL;
		goto done;
	}
#endif /* WL_MBO && !WL_MBO_DISABLED && MBO_AP */

#ifdef PSTA
	/* Send WLC_E_PSTA_PRIMARY_INTF_IND event to indicate psta primary mac addr */
	if (ap_scb->psta_prim != NULL) {
		wl_psta_primary_intf_event_t psta_prim_evet;

		WL_ASSOC(("wl%d.%d scb:"MACF", psta_prim:"MACF"\n",
			wlc->pub->unit,	WLC_BSSCFG_IDX(bsscfg),
			ETHER_TO_MACF(scb->ea), ETHER_TO_MACF(ap_scb->psta_prim->ea)));

		memcpy(&psta_prim_evet.prim_ea, &ap_scb->psta_prim->ea, sizeof(struct ether_addr));
		wlc_bss_mac_event(wlc, bsscfg, WLC_E_PSTA_PRIMARY_INTF_IND,
			&scb->ea, WLC_E_STATUS_SUCCESS, 0, 0,
			&psta_prim_evet, sizeof(wl_psta_primary_intf_event_t));
	}
#endif /* PSTA */

	param->status = status;
	bcopy(&req_rates, &param->req_rates, sizeof(wlc_rateset_t));
	/* Copy supported mcs index bit map */
	bcopy(req_rates.mcs, scb->rateset.mcs, MCSSET_LEN);

#ifdef SPLIT_ASSOC
	if (SPLIT_ASSOC_REQ(bsscfg)) {
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
#endif /* SPLIT_ASSOC */
	{
	decision.assoc_approved = TRUE;
	decision.reject_reason = 0;
	bcopy(&scb->ea, &decision.da, ETHER_ADDR_LEN);
	wlc_ap_process_assocreq_next(wlc, bsscfg, &decision, scb, param);
	}

	return;

	/* ===== Assoc Request Validations End ===== */

done:
	param->status = status;
	wlc_ap_process_assocreq_done(ap, bsscfg, &decision, hdr, body, body_len, scb, param);

	if (free_scb) {
		/* Clear states and mark the scb for deletion. SCB free will happen
		 * from the inactivity timeout context in wlc_ap_stastimeout()
		 * Mark the scb for deletion first as some scb state change notify callback
		 * functions need to be informed that the scb is about to be deleted.
		 * (For example wlc_cfp_scb_state_upd)
		 */
		SCB_MARK_DEL(scb);
		wlc_scb_clearstatebit(wlc, scb, ASSOCIATED | AUTHORIZED);
	}

	goto body_free;

exit:
	wl_smf_stats(ap, bsscfg, hdr, param);
	/* fall through */

body_free:
#ifdef SPLIT_ASSOC
	if (SPLIT_ASSOC_REQ(bsscfg) && param->body) {
		MFREE(wlc->osh, param->body, param->buf_len);
		bzero(param, sizeof(wlc_assoc_req_t));
	}
#endif /* SPLIT_ASSOC */
	return;
}
	/*
	 * here if association request parsing is successful...
	 */

#ifdef SPLIT_ASSOC
void wlc_ap_process_assocreq_decision(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, assoc_decision_t *dc)
{
	BCM_REFERENCE(dc);
	BCM_REFERENCE(wlc);
	BCM_REFERENCE(bsscfg);

	wlc_ap_process_assocreq_next(wlc, bsscfg, dc, NULL, NULL);
}
#endif /* SPLIT_ASSOC */

static void wlc_ap_process_assocreq_next(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	assoc_decision_t * dc, struct scb *scb, wlc_assoc_req_t * param)
{
	wlc_ap_info_t *ap;
	struct dot11_management_header *hdr;
	uint8 *body;
	uint body_len;
	struct dot11_assoc_req *req;

	uint16 capability;
#if defined(BCMDBG_ERR)
	char dabuf[ETHER_ADDR_STR_LEN], *da = bcm_ether_ntoa(&dc->da, dabuf);
#endif // endif
#if  defined(BCMDBG_ERR) || defined(BCMDBG) || defined(WLMSG_ASSOC)
	char sabuf[ETHER_ADDR_STR_LEN], *sa;
#endif // endif

	WL_TRACE(("wl%d: wlc_process_assocreq_next\n", wlc->pub->unit));

	ASSERT(bsscfg != NULL);
	ASSERT(bsscfg->up);
	ASSERT(BSSCFG_AP(bsscfg));

	ap = wlc->ap;

#ifdef SPLIT_ASSOC
	if (SPLIT_ASSOC_REQ(bsscfg)) {

		scb = wlc_scbfind(wlc, bsscfg, &dc->da);
		if (scb == NULL) {
			WL_ERROR(("wl%d.%d %s could not find scb\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), da));
			return;
		}

		param = AS_SCB_CUBBY((wlc_ap_info_pvt_t *)ap, scb);
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
#endif /* SPLIT_ASSOC */

#if  defined(BCMDBG_ERR) || defined(BCMDBG) || defined(WLMSG_ASSOC)
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
		WL_ERROR(("wl%d.%d %s denied association due to status(%d)\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), da, dc->reject_reason));
		goto done;
	}

	capability = ltoh16(req->capability);

	/* WEP encryption */
	if (scb->WPA_auth == WPA_AUTH_DISABLED &&
	    WSEC_WEP_ENABLED(bsscfg->wsec))
		scb->wsec = WEP_ENABLED;

	/* XXX JQL
	 * we should have a callback mechanism to call registered scb cubby owners
	 * to reset any relevant data for the (re)association.
	 */
	wlc_ap_tpc_assoc_reset(wlc->tpc, scb);

	/* When a HT, VHT, or HE STA using TKIP or WEP only unicast cipher suite tries
	 * to associate, exclude HT, VHT, and HE IEs from assoc response to force
	 * the STA to operate in legacy mode.
	 */
	if (WSEC_ENABLED(bsscfg->wsec)) {
		bool keep_ht = TRUE;

		ASSERT(param->req_rates.count > 0);
		if (scb->wsec == TKIP_ENABLED &&
		    (wlc_ht_get_wsec_restriction(wlc->hti) & WLC_HT_TKIP_RESTRICT)) {
			keep_ht = FALSE;
		} else if (scb->wsec == WEP_ENABLED &&
		           (wlc_ht_get_wsec_restriction(wlc->hti) & WLC_HT_WEP_RESTRICT)) {
			keep_ht = FALSE;
		}

		if (!keep_ht) {
			if (N_ENAB(wlc->pub) && SCB_HT_CAP(scb))
				wlc_ht_update_scbstate(wlc->hti, scb, NULL, NULL, NULL);
#ifdef WL11AC
			if (VHT_ENAB_BAND(wlc->pub, wlc->band->bandtype) && SCB_VHT_CAP(scb))
				wlc_vht_update_scb_state(wlc->vhti,
					wlc->band->bandtype, scb, NULL, NULL, NULL, 0);
#endif /* WL11AC */
#ifdef WL11AX
			if (HE_ENAB_BAND(wlc->pub, wlc->band->bandtype) && SCB_HE_CAP(scb))
				wlc_he_update_scb_state(wlc->hei, wlc->band->bandtype, scb,
					NULL, NULL);
#endif // endif
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
		wlc_key_t *key;
		wlc_key_info_t key_info;
		wlc_key_algo_t algo;
		wlc_key_t *scb_key;
		uint8 data[WEP128_KEY_SIZE];
		size_t data_len;
		int err;

		key = wlc_keymgmt_get_bss_tx_key(wlc->keymgmt, bsscfg, FALSE, &key_info);
		algo = key_info.algo;
		if (algo == CRYPTO_ALGO_OFF)
			goto defkey_done;

		WL_ASSOC(("Def key installed \n"));
		if  (algo != CRYPTO_ALGO_WEP1 && algo != CRYPTO_ALGO_WEP128)
			goto defkey_done;

		WL_ASSOC(("wl%d: %s Inserting key \n", WLCWLUNIT(wlc), sa));
		err = wlc_key_get_data(key, data, sizeof(data), &data_len);
		if (err != BCME_OK) {
			WL_ASSOC(("wl%d: %s error %d getting default key data, key idx %d\n",
				WLCWLUNIT(wlc), sa, err, key_info.key_idx));
			goto defkey_done;
		}

		scb_key = wlc_keymgmt_get_scb_key(wlc->keymgmt, scb,
			WLC_KEY_ID_PAIRWISE, WLC_KEY_FLAG_NONE, &key_info);
		ASSERT(scb_key != NULL);
		err = wlc_key_set_data(scb_key, algo, data, data_len);
		if (err != BCME_OK) {
			WL_ERROR(("wl%d.%d: Error %d inserting key for sa %s, key idx %d\n",
				WLCWLUNIT(wlc), WLC_BSSCFG_IDX(bsscfg), err, sa, key_info.key_idx));
		}
	}

defkey_done:

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
			} else {
				wlc_scb_ampdu_enable(wlc, scb);
			}
		}
	}
done:
#ifndef WL_SAE_ASSOC_OFFLOAD
#ifdef WL_SAE
	/* STA has chosen SAE AKM */
	if ((scb->WPA_auth == WPA3_AUTH_SAE_PSK) &&
		scb->pmkid_included) {
		/* set scb state to PENDING_ASSOC */
		wlc_scb_setstatebit(wlc, scb, PENDING_ASSOC);
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
#ifdef SPLIT_ASSOC
	if (SPLIT_ASSOC_REQ(bsscfg)) {
		MFREE(wlc->osh, param->body, param->buf_len);
		bzero(param, sizeof(wlc_assoc_req_t));
	}
#endif /* SPLIT_ASSOC */
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

	/* filter rateset 'req_rates' for the BSS supported rates.  */
	wlc_rate_hwrs_filter_sort_validate(req_rates /* [in+out] */,
		&current_bss->rateset /* [in] */, FALSE, wlc->stf->op_txstreams);

	rsp_len = DOT11_ASSOC_RESP_FIXED_LEN;
	WL_ASSOC_LT(("wlc_ap_process_assocreq_done status %d\n", param->status));

	/* prepare IE mgmt calls */
	bzero(&ftcbparm, sizeof(ftcbparm));
	ftcbparm.assocresp.mcs = req_rates->mcs;
	ftcbparm.assocresp.scb = scb;
	ftcbparm.assocresp.status = param->status;
	if (param->status != DOT11_SC_SUCCESS)
		ftcbparm.assocresp.status_data = dc->reject_data;
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
	if ((pkt = wlc_frame_get_mgmt(wlc, type, &hdr->sa, &bsscfg->cur_etheraddr,
	                              &bsscfg->BSSID, rsp_len, &rsp)) == NULL) {
		param->status = DOT11_SC_ASSOC_BUSY_FAIL;
		WL_ASSOC(("wl%d.%d %s %sassociattion failed rsp_len %d\n", wlc->pub->unit,
			WLC_BSSCFG_IDX(bsscfg), sa, reassoc ? "re" : "", rsp_len));
		rsp_len = 0;
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

#if defined(BCMDBG) || defined(WLMSG_PRPKT)
	WL_ASSOC(("wl%d: JOIN: sending %s RESP ...\n", WLCWLUNIT(wlc),
	          reassoc ? "REASSOC" : "ASSOC"));

	if (WL_ASSOC_ON()) {
		struct dot11_management_header *h =
		        (struct dot11_management_header *)PKTDATA(wlc->osh, pkt);
		uint l = PKTLEN(wlc->osh, pkt);
		wlc_print_assoc_resp(wlc, h, l);
	}
#endif // endif
#ifdef WL_TRAFFIC_THRESH
	if (bsscfg->trf_scb_enable) {
		/* copying auto enable flags and zeroing data */
		if (scb && !SCB_ASSOCIATED(scb)) {
			int k;
			scb->trf_enable_flag = bsscfg->trf_scb_enable;
			for (k = 0; k < WL_TRF_MAX_QUEUE; k++) {
				/* data is reset */
				int j;
				scb->scb_trf_data[k].timestamp = 0;
				scb->scb_trf_data[k].idx = 0;
				for (j = 0; j < WL_TRF_MAX_SECS; j++) {
					scb->scb_trf_data[k].histo[j] = 0;
				}
			}
		}
	}
#endif /* WL_TRAFFIC_THRESH */
	/* send the association response */
	wlc_queue_80211_frag(wlc, pkt, bsscfg->wlcif->qi, scb, bsscfg,
		param->short_preamble, NULL, WLC_LOWEST_SCB_RSPEC(scb));

#ifdef BCM_CEVENT
	if (CEVENT_STATE(wlc->pub)) {
		wlc_send_cevent(wlc, bsscfg, SCB_EA(scb), param->status,
				0, 0, NULL, 0, CEVENT_D2C_ST_ASSOC_TX,
				CEVENT_D2C_FLAG_QUEUED | CEVENT_FRAME_DIR_TX);
	}
#endif /* BCM_CEVENT */

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

	/*
	 * scb->listen is used by the AP for timing out PS pkts,
	 * ensure pkts are held for at least one dtim period
	 */
	wlc_apps_set_listen_prd(wlc, scb, MAX(current_bss->dtim_period, listen));

	scb->assoctime = wlc->pub->now;

	wlc_scb_setstatebit(wlc, scb, ASSOCIATED);

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
	if ((mcsallow & WLC_MCS_ALLOW_VHT) &&
		WLC_VHT_FEATURES_GET(wlc->pub, WL_VHT_FEATURES_1024QAM) &&
		SCB_1024QAM_CAP(scb))
		mcsallow |= WLC_MCS_ALLOW_1024QAM;

	if (BSS_HE_ENAB(wlc, bsscfg) && SCB_HE_CAP(scb))
		mcsallow |= WLC_MCS_ALLOW_HE;

	/* req_rates => scb->rateset */
	wlc_rateset_filter(req_rates, &scb->rateset, FALSE, rates, RATE_MASK, mcsallow);

	/* re-initialize rate info
	 * Note that this wipes out any previous rate stats on the STA. Since this
	 * being called at Association time, it does not seem like a big loss.
	 */
	wlc_scb_ratesel_init(wlc, scb);

	/* Start beaconing if this is first STA */
	if (DYNBCN_ENAB(bsscfg) && wlc_bss_assocscb_getcnt(wlc, bsscfg) == 1) {
		wlc_bsscfg_bcn_enable(wlc, bsscfg);
	}

	/* notify other APs on the DS that this station has roamed */
	if (reassoc && bcmp((char*)&bsscfg->BSSID, reassoc_ap->octet, ETHER_ADDR_LEN))
		wlc_reassoc_notify(ap, &hdr->sa, bsscfg);

	/* 802.11f assoc. announce pkt */
	wlc_assoc_notify(ap, &hdr->sa, bsscfg);

#ifdef WLP2P
	if (BSS_P2P_ENAB(wlc, bsscfg))
		wlc_p2p_enab_upd(wlc->p2p, bsscfg);
#endif // endif

	/* Enable BTCX PS protection */
	wlc_btc_set_ps_protection(wlc, bsscfg); /* enable */

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

	BCM_REFERENCE(dc);
	BCM_REFERENCE(rsp);
	BCM_REFERENCE(body);
	BCM_REFERENCE(rsp_len);
	BCM_REFERENCE(scb);
	BCM_REFERENCE(body_len);

	reassoc = ((ltoh16(hdr->fc) & FC_KIND_MASK) == FC_REASSOC_REQ);
	/* send WLC_E_REASSOC_IND/WLC_E_ASSOC_IND to interested App and/or non-WIN7 OS */
	wlc_bss_mac_event(wlc, bsscfg, reassoc ? WLC_E_REASSOC_IND : WLC_E_ASSOC_IND,
		&hdr->sa, WLC_E_STATUS_SUCCESS, param->status, scb->auth_alg,
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
			&hdr->sa, WLC_E_STATUS_SUCCESS, param->status, scb->auth_alg,
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

	wl_smf_stats(ap, bsscfg, hdr, param);
}

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

void
wlc_ap_get_sta_info_vendor_oui(wlc_info_t *wlc, struct scb *scb, sta_vendor_oui_t *v_oui)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *)(wlc->ap);
	wlc_assoc_oui_t *assoc_oui;
	int i;
	assoc_oui = SCB_ASSOC_OUI_CUBBY(appvt, scb);
	if (!assoc_oui->count)
		return;
	for (i = 0; i < assoc_oui->count && i < WLC_MAX_ASSOC_OUI_NUM; i++) {
		v_oui->oui[i][0] = assoc_oui->oui[i][0];
		v_oui->oui[i][1] = assoc_oui->oui[i][1];
		v_oui->oui[i][2] = assoc_oui->oui[i][2];
	}
	v_oui->count = i;
}

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

uint8
wlc_rxchain_pwrsave_get_rxchain(wlc_info_t *wlc)
{
	uint8 rxchain = wlc->stf->rxchain;
	wlc_ap_info_t *ap = wlc->ap;
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;
	if ((appvt != NULL) && appvt->rxchain_pwrsave.pwrsave.in_power_save) {
		rxchain = appvt->rxchain_pwrsave.rxchain;
	}
	return rxchain;
}

#ifdef WL11N
/*
 * get rx_stbc HT capability, if in rxchain_pwrsave mode, return saved rx_stbc value,
 * because rx_stbc capability may be changed when enter rxchain_pwrsave mode
 */
uint8
wlc_rxchain_pwrsave_stbc_rx_get(wlc_info_t *wlc)
{
	uint8 ht_cap_rx_stbc = wlc_ht_stbc_rx_get(wlc->hti);
	wlc_ap_info_t *ap = wlc->ap;
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;
	if ((appvt != NULL) && appvt->rxchain_pwrsave.pwrsave.in_power_save) {
		ht_cap_rx_stbc = appvt->rxchain_pwrsave.ht_cap_rx_stbc;
	}
	return ht_cap_rx_stbc;
}
#endif /* WL11N */

/*
 * Check whether we are in rxchain power save mode
 */
int
wlc_ap_in_rxchain_power_save(wlc_ap_info_t *ap)
{
	wlc_ap_info_pvt_t* appvt = (wlc_ap_info_pvt_t*) ap;

	return (appvt->rxchain_pwrsave.pwrsave.in_power_save);
}
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

	if (RADIONOA_PWRSAVE_ENAB(wlc->pub)) {
		wlc_reset_rpsnoa(wlc);
		wlc_reset_pwrsave_mode(ap, PWRSAVE_RADIO);
		/* Set WAKE bit
		 * if in_power_save flag is unset, wake is set in AP mode
		 */
		wlc_set_wake_ctrl(wlc);
	} else {
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
			ASSERT(0);
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

#ifdef WL_TRAFFIC_THRESH
static void
wlc_traffic_thresh_mgmt(wlc_info_t *wlc)
{
	wlc_bsscfg_t *bsscfg;
	int32 idx;
	int i;
	if (wlc->pub->_traffic_thresh) {
		FOREACH_BSS(wlc, idx, bsscfg) {
			wlc_trf_data_t *ptr = bsscfg->trf_cfg_data;
			wlc_intfer_params_t  *params;
			if (bsscfg->traffic_thresh_enab) {
				for (i = 0; i < WL_TRF_MAX_QUEUE; i++) {
					params = &bsscfg->trf_cfg_params[i];
					if ((bsscfg->trf_cfg_enable & (1 << i)) &&
						(ptr[i].num_data != NULL)) {
						(ptr[i].cur)++;
						if (ptr[i].cur >= params->num_secs) {
							ptr[i].cur = 0;
						}
						ptr[i].count -= ptr[i].num_data[ptr[i].cur];
						ptr[i].num_data[ptr[i].cur] = 0;
					}
				}
			}
		}
	}
}
#endif /* WL_TRAFFIC_THRESH */

bool
wlc_apsta_on_radar_channel(wlc_ap_info_t *ap)
{
	ASSERT(ap);
	return ((wlc_ap_info_pvt_t *)ap)->ap_sta_onradar;
}

#ifdef WLDFS
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
#endif /* WLDFS */

#ifdef STA
/* Check if any local AP/STA chanspec overlap with STA/local AP
 * radar chanspec.
 */
void
wlc_ap_sta_onradar_upd(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_bss_info_t *current_bss = cfg->current_bss;
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) wlc->ap;
	bool dfs_slave_present = FALSE;

	/* Only when on radar channel we look for overlapping AP + STA.
	 * The moment we switch to non-radar we turn off dfs_slave_present.
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
	wlc_bsscfg_t *bsscfg_ap = NULL;
	uint bss_radar_flags = 0;
#endif /* RADAR */

	WL_TRACE(("wl%d: %s:\n", wlc->pub->unit, __FUNCTION__));

	if (wlc->state == WLC_STATE_GOING_DOWN || !wlc->pub->up) {
		WL_ERROR(("wl%d: %s: wl is going down or is not up\n",
			wlc->pub->unit, __FUNCTION__));
		return;
	}

	if (APSTA_ENAB(wlc->pub) && (!wlc->keep_ap_up) &&
		(MAP_ENAB(wlc->cfg) || DWDS_ENAB(wlc->cfg) ||
#if defined(WET) || defined(WET_DONGLE)
			(WET_ENAB(wlc) || WET_DONGLE_ENAB(wlc)) ||
#endif /* WET || WET_DONGLE */
			(wlc->cfg->_psta)) &&
			((wlc->cfg->disable_ap_up) || !(wlc->cfg->up))) {
		/* Defer the bsscfg_up operation for AP cfg, as primary STA cfg
		 * interface is either not up or roaming to find out the target beacon,
		 * this defer operation enabled by roam_complete routine once STA
		 * interface losses the number of beacons greater than threshold
		 */
		return;
	}

#if defined(STA) && defined(AP)
	/* Check if it is feasible to
	* bringup the AP.
	*/
	if (!wlc_apup_allowed(wlc)) {
		return;
	}
#endif /* STA && AP */

#ifdef RADAR
	if (RADAR_ENAB(wlc->pub) && WL11H_AP_ENAB(wlc)) {
		FOREACH_AP(wlc, i, bsscfg) {
			bss_radar_flags |= (bsscfg)->flags & (WLC_BSSCFG_SRADAR_ENAB |
				WLC_BSSCFG_AP_NORADAR_CHAN |
				WLC_BSSCFG_USR_RADAR_CHAN_SELECT);
			if (bsscfg_ap == NULL) { /* store the first AP cfg */
				bsscfg_ap = bsscfg;
			}

			/* Make sure these flags are are never combined together
			 * either within single ap bss or across all ap-bss.
			 */
			ASSERT(!(bss_radar_flags & (bss_radar_flags - 1)));
		}

		/* No random channel selection in any of bss_radar_flags modes */
		if (bsscfg_ap != NULL && !bss_radar_flags &&
		    (wlc_channel_locale_flags_in_band(wlc->cmi, BAND_5G_INDEX) & WLC_DFS_TPC)) {
			wlc_dfs_sel_chspec(wlc->dfs, TRUE, bsscfg_ap);
		}
	}
#endif /* RADAR */

	appvt->pre_tbtt_us = (MBSS_ENAB(wlc->pub)) ? MBSS_PRE_TBTT_DEFAULT_us : PRE_TBTT_DEFAULT_us;
	/* Reset to re-configure TSF registers for beaconing */
	wlc->aps_associated = 0;

	/* Bring up any enabled AP configs which aren't up yet */
	FOREACH_AP(wlc, i, bsscfg) {
		if (bsscfg->enable) {
			uint wasup = bsscfg->up;
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
#if defined(WLRSDB) && defined(WL_MODESW)
			/* Check if the pending AP was brought UP */
			else if (RSDB_ENAB(wlc->pub) && WLC_MODESW_ENAB(wlc->pub) &&
				bsscfg->up && wlc_get_ap_up_pending(wlc, bsscfg)) {
				wlc_set_ap_up_pending(wlc, bsscfg, FALSE);
			}
#endif /* WLRSDB && WL_MODESW */
		}
	}
#ifdef RXCHAIN_PWRSAVE
	wlc_reset_rxchain_pwrsave_mode(ap);
#endif // endif
#ifdef RADIO_PWRSAVE
	wlc_reset_radio_pwrsave_mode(ap);
#endif // endif

	BCM_REFERENCE(ap);
	BCM_REFERENCE(bsscfg);
}

int
wlc_bss_up(wlc_ap_info_t *ap, wlc_bsscfg_t *bsscfg)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;
	wlc_info_t *wlc = appvt->wlc;
	wlc_bss_info_t *target_bss = bsscfg->target_bss;
	int ret;

	WL_TRACE(("wl%d: %s:\n", wlc->pub->unit, __FUNCTION__));
	ASSERT(bsscfg);
	ASSERT(BSSCFG_AP(bsscfg));

#ifdef WL11N
	/* Adjust target bss rateset according to target channel bandwidth */
	wlc_rateset_bw_mcs_filter(&target_bss->rateset,
		WL_BW_CAP_40MHZ(wlc->band->bw_cap)?CHSPEC_WLC_BW(target_bss->chanspec):0);
#endif /* WL11N */

	/* Register the operating channel on MSCH scheduler. */
	ret = wlc_ap_timeslot_register(bsscfg);
	/* Other init configuration related with radio
	 * channel status will get called by 1st MSCH ON_CHAN
	 */
	return ret;
}

/* This function only cover before MSCH ON_CHAN schedule.
 * wlc_ap_down() will handle rest of down configurations
 */
int
wlc_bss_down(wlc_ap_info_t *ap, wlc_bsscfg_t *bsscfg)
{
	int ret = BCME_OK;
	UNUSED_PARAMETER(ap);

	/* Unregister the requested MSCH operating channel */
	wlc_ap_timeslot_unregister(bsscfg);
	return ret;
}

static void
wlc_ap_up_upd(wlc_ap_info_t *ap, wlc_bsscfg_t *bsscfg, bool state)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;
	wlc_info_t *wlc = appvt->wlc;
	bsscfg_state_upd_data_t st_data;
	DBGONLY(char chanbuf[CHANSPEC_STR_LEN]; )

	bzero(&st_data, sizeof(st_data));
	st_data.cfg = bsscfg;
	st_data.old_enable = bsscfg->enable;
	st_data.old_up = bsscfg->up;

	/* Assume MAC is suspended already. DO NOT suspend MAC here */
	ASSERT(BSSCFG_AP(bsscfg));

	bsscfg->up = bsscfg->associated = state;

	WL_REGULATORY(("wl%d.%d: %s: chanspec %s, up %d\n", wlc->pub->unit,
		WLC_BSSCFG_IDX(bsscfg), __FUNCTION__,
		wf_chspec_ntoa(bsscfg->current_bss->chanspec, chanbuf), state));

	wlc->aps_associated = (uint8)AP_BSS_UP_COUNT(wlc);

	wlc_ap_sta_onradar_upd(bsscfg);

	wlc_mac_bcn_promisc(wlc);
	if ((wlc->aps_associated == 0) || ((wlc->aps_associated == 1))) {
		/* No need beyond 1 */
		wlc_bmac_enable_tbtt(wlc->hw, TBTT_AP_MASK,
			wlc->aps_associated ? TBTT_AP_MASK : 0);
	}
	wlc->pub->associated = wlc->aps_associated > 0 || wlc->stas_associated > 0;

	if (state) {
		uint8 constraint;
		/* Set the power limits for this locale after computing
		 * any 11h local tx power constraints.
		 */
		constraint = wlc_tpc_get_local_constraint_qdbm(wlc->tpc);
		wlc_channel_set_txpower_limit(wlc->cmi, constraint);
	}

#ifdef BCMPCIEDEV
	if (BCMPCIEDEV_ENAB()) {
		if (bsscfg->associated) {
			wlc_cfg_set_pmstate_upd(bsscfg, FALSE);
		} else {
			wlc_cfg_set_pmstate_upd(bsscfg, TRUE);
		}
	}
#endif /* BCMPCIEDEV */
	/* Mark beacon template as invalid; Just toggle respective bits */
	if (!state) {
		wlc_bmac_psm_maccommand(wlc->hw, (MCMD_BCN0VLD | MCMD_BCN1VLD));
	}

#ifdef WLRSDB
	if (RSDB_ENAB(wlc->pub)) {
		wlc_rsdb_update_active(wlc, NULL);
	}
#endif /* WLRSDB */

	wlc_bsscfg_notif_signal(bsscfg, &st_data);

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
		wlc_scb_resetstate(wlc, scb);
		wlc_scb_setstatebit(wlc, scb, AUTHENTICATED);

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
wlc_assoc_notify(wlc_ap_info_t *ap, struct ether_addr *sta, wlc_bsscfg_t *cfg)
{
	/* XXX, Why WL initial a "DATA" packet to Host?
	 * This 802.11f IAPP packet can trigger Host to update bridge ARP table
	 * so that M1 in new 4-way handshake for STA switch to other MBSS can
	 * work properly.
	 * This kind of notify packet need to use HWA_RPH_RESERVE_COUNT and
	 * we do support it now if HWA1A enabled.
	 */

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

	wlc_sendup(wlc, cfg, NULL, p);

	return;

err:
	WL_ERROR(("wl%d: %s: pktget error\n", wlc->pub->unit, __FUNCTION__));
	WLCNTINCR(wlc->pub->_cnt->rxnobuf);
	WLCNTINCR(cfg->wlcif->_cnt->rxnobuf);
	return;
}

static void
wlc_reassoc_notify(wlc_ap_info_t *ap, struct ether_addr *sta, wlc_bsscfg_t *cfg)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;
	wlc_info_t *wlc = appvt->wlc;
	void *p;
	osl_t *osh;
	struct lu_reassoc_pkt *lu;
	struct csco_reassoc_pkt *csco;
	int len;

	osh = wlc->osh;

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

	wlc_sendup(wlc, cfg, NULL, p);

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

	wlc_sendup(wlc, cfg, NULL, p);

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

	wlc_sendup(wlc, cfg, NULL, p);

	return;

err:
	WL_ERROR(("wl%d: %s: pktget error\n", wlc->pub->unit, __FUNCTION__));
	WLCNTINCR(wlc->pub->_cnt->rxnobuf);
	WLCNTINCR(cfg->wlcif->_cnt->rxnobuf);
	return;
}

ratespec_t
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

	/* These are basic legacy rates */
	return LEGACY_RSPEC(rate);
}

static void
wlc_ap_probe_complete(wlc_info_t *wlc, void *pkt, uint txs)
{
	struct scb *scb;

	if ((scb = WLPKTTAGSCBGET(pkt)) == NULL)
		return;

#ifdef WDS
	if (SCB_LEGACY_WDS(scb))
		wlc_ap_wds_probe_complete(wlc, txs, scb);
	else
#endif // endif
	wlc_ap_sta_probe_complete(wlc, txs, scb, pkt);
}

static int
wlc_ap_sendnulldata_cb(wlc_info_t *wlc, wlc_bsscfg_t *cfg, void *pkt, void *data)
{
	BCM_REFERENCE(wlc);
	BCM_REFERENCE(data);
	BCM_REFERENCE(cfg);

	/* register packet callback */
	WLF2_PCB1_REG(pkt, WLF2_PCB1_STA_PRB);
	/* reset expiry timer setting for keepalive packets */
	/* in congested environments these fail to go out and fail to tear down assocuated peer */
	WLF_RESET_EXP_TIME(pkt);
	return BCME_OK;
}

static void
wlc_ap_sta_probe(wlc_ap_info_t *ap, struct scb *scb)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;
	wlc_info_t *wlc = appvt->wlc;
	ratespec_t rate_override;
	bool  ps_pretend = SCB_PS_PRETEND(scb);

	/* If a probe is still pending, don't send another one */
	if (scb->flags & SCB_PENDING_PROBE)
		return;

	/* use the lowest basic rate */
	rate_override = wlc_lowest_basicrate_get(scb->bsscfg);

	ASSERT(VALID_RATE(wlc, rate_override));

	if (!wlc_sendnulldata(wlc, scb->bsscfg, &scb->ea, rate_override,
		ps_pretend ? WLF_PSDONTQ : 0,
		ps_pretend ? PRIO_8021D_VO : PRIO_8021D_BE,
		wlc_ap_sendnulldata_cb, NULL))
	{
		WL_ERROR(("wl%d: wlc_ap_sta_probe: wlc_sendnulldata failed\n",
		          wlc->pub->unit));
		return;
	}

	scb->flags |= SCB_PENDING_PROBE;
}

static void
wlc_ap_sta_probe_complete(wlc_info_t *wlc, uint txstatus, struct scb *scb, void *pkt)
{
	bool infifoflush = FALSE;
	wlc_ap_info_t *ap = wlc->ap;
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *)ap;
	ap_scb_cubby_t *ap_scb;

	ASSERT(scb != NULL);

	ap_scb = AP_SCB_CUBBY(appvt, scb);
	if (!ap_scb) {
		return;
	}

	scb->flags &= ~SCB_PENDING_PROBE;

	/* Reprobe if the pkt is freed in the middle of fifo flush.
	 * SCB shouldn't be deleted then.
	 */
#ifdef WLC_LOW
	infifoflush = (wlc->hw->mac_suspend_depth > 0);
#endif /* WLC_LOW */
	BCM_REFERENCE(infifoflush);
	if (wlc->txfifo_detach_pending) {
		WL_ERROR(("wl%d.%d: STA "MACF" probe bailed out. cnt %d txs 0x%x macsusp %d\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(SCB_BSSCFG(scb)), ETHER_TO_MACF(scb->ea),
			ap_scb->grace_attempts, txstatus, infifoflush));
		appvt->reprobe_scb = TRUE;
		return;
	}

#ifdef PSPRETEND
	if (SCB_PS_PRETEND_PROBING(scb)) {
		scb->flags &= ~SCB_PSPRETEND_PROBE;
		if ((txstatus & TX_STATUS_MASK) == TX_STATUS_ACK_RCV) {
			/* probe response OK - exit PS Pretend state */
			WL_PS(("wl%d.%d: received ACK to ps pretend probe "MACF" (count %d)\n",
			        wlc->pub->unit, WLC_BSSCFG_IDX(SCB_BSSCFG(scb)),
			        ETHER_TO_MACF(scb->ea), ap_scb->grace_attempts));

			/* Assert check that the fifo was cleared before exiting ps mode */
			if (SCB_PS_PRETEND_BLOCKED(scb)) {
				WL_ERROR(("wl%d.%d: %s: SCB_PS_PRETEND_BLOCKED, "
				          "expected to see PMQ PPS entry\n", wlc->pub->unit,
				          WLC_BSSCFG_IDX(SCB_BSSCFG(scb)), __FUNCTION__));
			}

			if (!(wlc->block_datafifo & DATA_BLOCK_PS)) {
				wlc_apps_scb_ps_off(wlc, scb, FALSE);
			} else {
				/* STA should transition to PS-OFF once PS flush done */
				wlc_apps_ps_trans_upd(wlc, scb);
				WL_PS(("wl%d.%d: %s: PS flush in progress.\n",
				     wlc->pub->unit, WLC_BSSCFG_IDX(SCB_BSSCFG(scb)),
				     __FUNCTION__));
			}
		} else {
			++ap_scb->grace_attempts;
			WL_PS(("wl%d.%d: no response to ps pretend probe "MACF" (count %d)\n",
			        wlc->pub->unit, WLC_BSSCFG_IDX(SCB_BSSCFG(scb)),
			        ETHER_TO_MACF(scb->ea), ap_scb->grace_attempts));
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
	    (wlc->pub->now - scb->used < appvt->scb_activity_time) ||
	    appvt->scb_activity_time == 0) {
		ap_scb->grace_attempts = 0;
		/* update the primary PSTA also */
		if (ap_scb->psta_prim != NULL)
			ap_scb->psta_prim->used = wlc->pub->now;
		return;
	}

	/* If still more grace_attempts are left, then probe the STA again */
	if (++ap_scb->grace_attempts < appvt->scb_max_probe) {
		appvt->reprobe_scb = TRUE;
		return;
	}

	WL_ASSOC(("wl%d: wlc_ap_sta_probe_complete: no ACK from "MACF" for Null Data\n",
	          wlc->pub->unit, ETHER_TO_MACF(scb->ea)));

	if (SCB_AUTHENTICATED(scb)) {
		/* If SCB is in authenticated state then clear the ASSOCIATED, AUTHENTICATED and
		 * AUTHORIZED bits in the flag before freeing the SCB
		 */
		wlc_scb_clearstatebit(wlc, scb, ASSOCIATED | AUTHENTICATED | AUTHORIZED);
		wlc_deauth_complete(wlc, SCB_BSSCFG(scb), WLC_E_STATUS_SUCCESS, &scb->ea,
			DOT11_RC_INACTIVITY, 0);
	}

	/* free the scb */
	wlc_scbfree(wlc, scb);
	WLPKTTAGSCBSET(pkt, NULL);
}

int
wlc_ap_get_bcnprb(wlc_info_t *wlc, wlc_bsscfg_t *cfg, bool bcn, void *buf, uint *buflen)
{
	int rc = BCME_OK;
	int hdr_skip = 0;
	int pkt_len = 0;
	uint8 *pkt_data = NULL;

	ASSERT(buf);
	ASSERT(buflen);

	if (MBSS_BCN_ENAB(wlc, cfg)) {
#if defined(MBSS)
		wlc_pkt_t pkt;

		/* Fetch packet of the correct type */
		if (bcn) {
			pkt = wlc_mbss_get_bcn_template(wlc, cfg);
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

		} else {
			pkt = wlc_mbss_get_probe_template(wlc, cfg);
		}

		/* Select the data, but skip the D11 header part */
		if (pkt != NULL) {
			if (D11REV_GE(wlc->pub->corerev, 128) && bcn) {
				hdr_skip = D11_PHY_HDR_LEN;
			} else {
				hdr_skip += D11_TXH_LEN_EX(wlc);
			}
			pkt_len = PKTLEN(wlc->osh, pkt);
			pkt_data = (uint8 *)PKTDATA(wlc->osh, pkt);
		}
#endif /* MBSS */
	} else if (HWBCN_ENAB(cfg)) {
		uint type;
		ratespec_t rspec;

		pkt_len = wlc->pub->bcn_tmpl_len;
		pkt_data = (uint8*)buf;

		/* Bail if not enough room */
		if ((int)*buflen < pkt_len) {
			return BCME_BUFTOOSHORT;
		}

		if (bcn) {
			/* For beacon_info */
			type = FC_BEACON;
			hdr_skip = wlc->pub->d11tpl_phy_hdr_len;
			rspec = wlc_lowest_basic_rspec(wlc, &cfg->current_bss->rateset);
		} else {
			/* For probe_resp_info */
			type = FC_PROBE_RESP;
			rspec = (ratespec_t)0;

			if (D11REV_GE(wlc->pub->corerev, 40)) {
				hdr_skip = 0;
			} else {
				hdr_skip = wlc->pub->d11tpl_phy_hdr_len;
			}
		}
		/* Generate the appropriate packet template */
		wlc_bcn_prb_template(wlc, type, rspec, cfg, (uint16 *)pkt_data, &pkt_len);
		if (type == FC_BEACON) {
			wlc_beacon_upddur(wlc, rspec, pkt_len);
		}
	}

	/* Return management frame if able */
	pkt_data += hdr_skip;
	pkt_len -= MIN(hdr_skip, pkt_len);

	if ((int)*buflen >= pkt_len) {
		memcpy((uint8*)buf, pkt_data, pkt_len);
		*buflen = (int)pkt_len;
	} else {
		rc = BCME_BUFTOOSHORT;
	}

	return rc;
}

#ifdef BCMDBG
static int
wlc_dump_ap(wlc_ap_info_t *ap, struct bcmstrbuf *b)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;
	wlc_info_t *wlc = appvt->wlc;

	bcm_bprintf(b, "\n");

	bcm_bprintf(b, " shortslot_restrict %d scb_timeout %d\n",
		ap->shortslot_restrict, appvt->scb_timeout);

	bcm_bprintf(b, "tbtt %d pre-tbtt-us %u. max latency %u. "
		"min threshold %u. block datafifo %d "
		"\n",
		WLCNTVAL(wlc->pub->_cnt->tbtt),
		appvt->pre_tbtt_us, MBSS_PRE_TBTT_MAX_LATENCY_us,
		MBSS_PRE_TBTT_MIN_THRESH_us, wlc->block_datafifo);

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

#if defined(STA) && defined(AP)
bool
wlc_apup_allowed(wlc_info_t *wlc)
{
	bool modesw_in_prog = FALSE;
	bool busy = FALSE;
	wlc_bsscfg_t *as_cfg;
#if defined(WL_MODESW) && !defined(WL_MODESW_DISABLED)
	modesw_in_prog = (WLC_MODESW_ENAB(wlc->pub) && MODE_SWITCH_IN_PROGRESS(wlc->modesw)) ?
	TRUE:FALSE;
#endif /* WL_MODESW && !WL_MODESW_DISABLED */
	busy = ANY_SCAN_IN_PROGRESS(wlc->scan) ||
#ifdef WL11K
		wlc_rrm_inprog(wlc) ||
#endif /* WL11K */
		WLC_RM_IN_PROGRESS(wlc) ||
		modesw_in_prog;
	as_cfg = AS_IN_PROGRESS_CFG(wlc);
	busy = busy || (AS_IN_PROGRESS(wlc) &&
		(as_cfg->assoc->state != AS_WAIT_FOR_AP_CSA) &&
		(as_cfg->assoc->state != AS_WAIT_FOR_AP_CSA_ROAM_FAIL));

#ifdef BCMDBG
	if (busy) {
		WL_APSTA_UPDN(("wl%d: wlc_apup_allowed: defer AP UP, STA associating: "
			"stas/aps/associated %d/%d/%d, AS_IN_PROGRESS() %d, scan %d, rm %d"
			"modesw = %d\n", wlc->pub->unit, wlc->stas_associated, wlc->aps_associated,
			wlc->pub->associated, AS_IN_PROGRESS(wlc), SCAN_IN_PROGRESS(wlc->scan),
			WLC_RM_IN_PROGRESS(wlc), modesw_in_prog));
	}
#endif // endif
	return !busy;
}
#endif /* STA */

static void
wlc_ap_dynfb(wlc_ap_info_pvt_t *appvt)
{
	wlc_info_t *wlc = appvt->wlc;
	data_snapshot_t *snapshot = &appvt->snapshot;
	uint32 txframe_snapshot, rxframe_snapshot;
	uint32 txbytes_snapshot, rxbytes_snapshot;
	uint32 delta_txframe, delta_rxframe;
	uint32 delta_txbytes, delta_rxbytes;
	uint32 tx_pktsz = 0, rx_pktsz = 0;

	rxframe_snapshot = wlc->pub->_cnt->rxframe;
	txframe_snapshot = wlc->pub->_cnt->txframe;
	rxbytes_snapshot = wlc->pub->_cnt->rxbyte;
	txbytes_snapshot = wlc->pub->_cnt->txbyte;

	delta_txframe = txframe_snapshot - snapshot->txframe;
	delta_rxframe = rxframe_snapshot - snapshot->rxframe;
	delta_txbytes = txbytes_snapshot - snapshot->txbytes;
	delta_rxbytes = rxbytes_snapshot - snapshot->rxbytes;

	if (delta_txframe > 0)
		tx_pktsz = (delta_txbytes) / (delta_txframe);
	if (delta_rxframe > 0)
		rx_pktsz = (delta_rxbytes) / (delta_rxframe);

	if ((tx_pktsz >= wlc->active_bidir_thresh) && (rx_pktsz >= wlc->active_bidir_thresh) &&
		((wlc->active_udpv6 == TRUE) ||
		(BAND_2G(wlc->band->bandtype) && (wlc->active_udpv4 == TRUE))) &&
		(wlc->active_tcp == FALSE)) {
		wlc->active_bidir = TRUE;
		wlc->bidir_countdown = ACTIVE_BIDIR_DELAY;
	} else {
		if (wlc->bidir_countdown > 0)
			wlc->bidir_countdown--;
	}

	WL_TRACE(("band<%d> frames<%u:%u> pktsz<%u:%u> udpv6<%s> udpv4<%s> tcp<%s> "
		"bidir<%s> count<%d>\n",
		wlc->band->bandtype,
		delta_txframe, delta_rxframe,
		tx_pktsz, rx_pktsz,
		wlc->active_udpv6?"true":"false",
		wlc->active_udpv4?"true":"false",
		wlc->active_tcp?"true":"false",
		wlc->active_bidir?"TRUE":"FALSE",
		wlc->bidir_countdown));

	ASSERT(wlc->bidir_countdown <= ACTIVE_BIDIR_DELAY && wlc->bidir_countdown >= 0);
	if (wlc->bidir_countdown == 0)
		wlc->active_bidir = FALSE;
	wlc->active_udpv6 = FALSE;
	wlc->active_udpv4 = FALSE;
	wlc->active_tcp = FALSE;

	snapshot->txframe = txframe_snapshot;
	snapshot->rxframe = rxframe_snapshot;
	snapshot->txbytes = txbytes_snapshot;
	snapshot->rxbytes = rxbytes_snapshot;
}

static void
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

#if defined(STA)
	/* start AP if operation were pending on SCAN_IN_PROGRESS() or WLC_RM_IN_PROGRESS() */
	/* Find AP's that are enabled but not up to restart */
		if (APSTA_ENAB(wlc->pub) && wlc_apup_allowed(wlc)) {
			bool startap = FALSE;

			FOREACH_AP(wlc, idx, cfg) {
				/* find the first ap that is enabled but not up */
				if (cfg->enable && !cfg->up) {
					startap = TRUE;
					break;
				}
			}

			if (startap) {
				WL_APSTA_UPDN(("wl%d: wlc_watchdog -> restart downed ap\n",
				               wlc->pub->unit));
				wlc_restart_ap(ap);
			}
		}
#endif /* STA */

		if (wlc->active_bidir_thresh > 0) {
			/* dynamic frameburst is enabled only when active_bidir_thresh > 0 */
			wlc_ap_dynfb(appvt);
		}

		/* before checking for stuck tx beacons make sure atleast one
		 * ap bss is up phy is not muted(for whatever reason) and beaconing.
		 */
		if (!phy_utils_ismuted((phy_info_t *)WLC_PI(wlc)) && !SCAN_IN_PROGRESS(wlc->scan) &&
		    (appvt->txbcn_timeout > 0) && (AP_BSS_UP_COUNT(wlc) > 0)) {
			uint16 txbcn_snapshot;

			/* see if any beacons are transmitted since last
			 * watchdog timeout.
			 */
			txbcn_snapshot = wlc_read_shm(wlc, MACSTAT_ADDR(wlc, MCSTOFF_TXBCNFRM));

			/* if no beacons are transmitted for txbcn timeout period
			 * then some thing has gone bad, do the big-hammer.
			 */
			if (txbcn_snapshot == appvt->txbcn_snapshot) {
				if (++appvt->txbcn_inactivity >= appvt->txbcn_timeout) {

					WL_ERROR(("wl%d: bcn inactivity detected\n",
					          wlc->pub->unit));
					WL_ERROR(("wl%d: txbcnfrm %d prev txbcnfrm %d "
					          "txbcn inactivity %d timeout %d macctrl 0x%x\n",
					          wlc->pub->unit, txbcn_snapshot,
					          appvt->txbcn_snapshot,
					          appvt->txbcn_inactivity,
					          appvt->txbcn_timeout,
					          R_REG(wlc->osh, D11_MACCONTROL(wlc))));
					appvt->txbcn_inactivity = 0;
					appvt->txbcn_snapshot = 0;
#if !defined(DONGLEBUILD) || !(defined(BCMDBG) || defined(WL_MACDBG))
					wlc_fatal_error(wlc);
#endif /* !defined(DONGLEBUILD) || !(defined(BCMDBG) || defined(WL_MACDBG)) */
					return;
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
				    SCB_HT_CAP(scb)) {
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

	/* part 2 */
	if (AP_ENAB(wlc->pub)) {

		/* process age-outs only when not in scan progress */
		if (!SCAN_IN_PROGRESS(wlc->scan)) {
			/* age out ps queue packets */
			wlc_apps_psq_ageing(wlc);
			if (appvt->scb_timeout &&
			    (((wlc->pub->now - wlc->pub->pending_now -1) % appvt->scb_timeout) >
			     (wlc->pub->now % appvt->scb_timeout))) {
				/* Appropriate aging time is misssed by scanning.
				 * set reprobe_scb to probing now.
				 */
				appvt->reprobe_scb = TRUE;
			}
			wlc->pub->pending_now = 0;

			/* age out stas */
			if ((appvt->scb_timeout &&
			     ((wlc->pub->now % appvt->scb_timeout) == 0)) ||
#ifdef WLP2P
			    (wlc->p2p && wlc_p2p_go_scb_timeout(wlc->p2p)) ||
#endif /* WLP2P */
			    appvt->reprobe_scb) {
				wlc_ap_stas_timeout(ap);
				appvt->reprobe_scb = FALSE;
			}
			/* age out secure STAs not in authorized state */
			wlc_ap_wsec_stas_timeout(ap);
		} else {
			/* This variable is that how many times aging jumped by scan operation.
			 * This variable reset when age-out starts again after scan.
			 */
			wlc->pub->pending_now++;
		}

		if (WLC_CHANIM_ENAB(wlc->pub) && WLC_CHANIM_MODE_ACT(wlc->chanim_info))
			wlc_lq_chanim_upd_act(wlc);

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
#ifdef WL_TRAFFIC_THRESH
	wlc_traffic_thresh_mgmt(wlc);
#endif /* WL_TRAFFIC_THRESH */

#ifdef WL_MODESW
	if (appvt->recheck_160_80 != 0 && appvt->recheck_160_80 < wlc->pub->now) {
		wlc_ap_160_80_bw_switch(wlc->ap);
	}
#endif /* WL_MODESW */
}

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

static int
wlc_ap_ioctl(void *hdl, uint cmd, void *arg, uint len, struct wlc_if *wlcif)
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
		} else {
			bcmerror = BCME_NOTAP;
		}
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
		} else {
			bcmerror = BCME_NOTAP;
		}
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
			*pval = appvt->scb_timeout;
		} else {
			bcmerror = BCME_NOTAP;
		}
		break;

	case WLC_SET_SCB_TIMEOUT:
		if (AP_ENAB(wlc->pub))
			appvt->scb_timeout = val;
		else
			bcmerror = BCME_NOTAP;
		break;

	case WLC_GET_ASSOCLIST:
		ASSERT(arg != NULL);
		maclist = (struct maclist *) arg;
		ASSERT(maclist);

		/* returns a list of STAs associated with a specific bsscfg */
		if (len < (int)(sizeof(maclist->count) + (maclist->count * ETHER_ADDR_LEN))) {
			bcmerror = BCME_BADARG;
			break;
		}
		val = 0;
		FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
			if (SCB_ASSOCIATED(scb)) {
				val++;
				if (maclist->count >= (uint)val) {
					bcopy((void*)&scb->ea, (void*)&maclist->ea[val-1],
					ETHER_ADDR_LEN);
				} else {
					bcmerror = BCME_BUFTOOSHORT;
					break;
				}
			}

		}
		if (!bcmerror)
			maclist->count = val;
		break;

	case WLC_TKIP_COUNTERMEASURES:
		if (BSSCFG_AP(bsscfg) && WSEC_TKIP_ENABLED(bsscfg->wsec))
			(void)wlc_keymgmt_tkip_set_cm(wlc->keymgmt, bsscfg, (val != 0));
		else
			bcmerror = BCME_BADARG;
		break;

#ifdef RADAR
	case WLC_SET_RADAR:
		bcmerror = RADAR_ENAB(wlc->pub) ?
				wlc_iovar_op(wlc, "radar", NULL, 0, arg, len, IOV_SET, wlcif) :
				BCME_UNSUPPORTED;
		break;

	case WLC_GET_RADAR:
		ASSERT(pval != NULL);
		if (RADAR_ENAB(wlc->pub)) {
			*pval = (int32)wlc_dfs_get_radar(wlc->dfs);
		} else {
				bcmerror = BCME_UNSUPPORTED;
		}
		break;
#endif /* RADAR */

	default:
		bcmerror = BCME_UNSUPPORTED;
		break;
	}

	return (bcmerror);
}

static int
wlc_ap_doiovar(void *hdl, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif)
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

	BCM_REFERENCE(val_size);

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

	case IOV_GVAL(IOV_AP_ISOLATE):
		*ret_int_ptr = (int32)bsscfg->ap_isolate;
		break;

	case IOV_GVAL(IOV_AP):
		*((uint*)arg) = BSSCFG_AP(bsscfg);
		break;

#if defined(STA) && defined(AP)
	case IOV_SVAL(IOV_AP): {
		wlc_bsscfg_type_t type = {BSSCFG_TYPE_GENERIC, BSSCFG_SUBTYPE_NONE};
		if (!APSTA_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		type.subtype = bool_val ? BSSCFG_GENERIC_AP : BSSCFG_GENERIC_STA;
		err = wlc_bsscfg_reinit(wlc, bsscfg, &type, 0);
		break;
	}
#endif /* defined(STA) && defined(AP) */

	case IOV_SVAL(IOV_AP_ISOLATE):
		if (!BCMPCIEDEV_ENAB())
			bsscfg->ap_isolate = (uint8)int_val;
		break;
	case IOV_GVAL(IOV_SCB_ACTIVITY_TIME):
		*ret_int_ptr = (int32)appvt->scb_activity_time;
		break;

	case IOV_SVAL(IOV_SCB_ACTIVITY_TIME):
		appvt->scb_activity_time = (uint32)int_val;
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
#if defined(MBSS)
			if (MBSS_ENAB(wlc->pub))
				wlc_mbss16_upd_closednet(wlc, bsscfg);
			else
#endif // endif
				wlc_mctrl(wlc, MCTL_CLOSED_NETWORK,
				(bool_val ? MCTL_CLOSED_NETWORK : 0));
		}
		break;

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
		/* int_val  : bssidx
		 * int_val2 : operation (up, down, etc)
		 */
		bool sta = TRUE;
		wlc_bsscfg_type_t type = {BSSCFG_TYPE_GENERIC, BSSCFG_SUBTYPE_NONE};

		if (len < (int)(2 * sizeof(int))) {
			err = BCME_BUFTOOSHORT;
			break;
		}
#ifdef STA
		if (int_val2 == WLC_AP_IOV_OP_MANUAL_AP_BSSCFG_CREATE) {
			sta = FALSE;
		}
		else if (int_val2 == WLC_AP_IOV_OP_MANUAL_STA_BSSCFG_CREATE) {
			sta = TRUE;
		}
#endif // endif
		/* On an 'up', use wlc_bsscfg_alloc() to create a bsscfg if it does not exist,
		 * but on a 'down', just find the bsscfg if it already exists
		 */
		if (int_val >= 0) { /* means: caller references an exisisting bss */
			bsscfg = wlc_bsscfg_find(wlc, int_val, &err);
			if (int_val2 > WLC_AP_IOV_OP_DISABLE &&
			    bsscfg == NULL && err == BCME_NOTFOUND) {
				type.subtype = !sta ? BSSCFG_GENERIC_AP : BSSCFG_GENERIC_STA;
				bsscfg = wlc_bsscfg_alloc(wlc, int_val, &type, 0, 0, NULL);
				if (bsscfg == NULL)
					err = BCME_NOMEM;
				else if ((err = wlc_bsscfg_init(wlc, bsscfg))) {
					WL_ERROR(("wl%d: wlc_bsscfg_init %s failed (%d)\n",
					          wlc->pub->unit, sta ? "STA" : "AP", err));
					wlc_bsscfg_free(wlc, bsscfg);
					break;
				}
			} else if (bsscfg != NULL) {
				/*
				 * This means a bsscfg was already found for the given
				 * index. The user is trying to change the role, change
				 * it only if needed. If request is to become AP and we
				 * are STA OR if the request is to become STA and we are AP
				 * change the role
				 * Reinit only on explicit role change parameter
				 */

				if (((int_val2 == WLC_AP_IOV_OP_MANUAL_AP_BSSCFG_CREATE) ||
					(int_val2 == WLC_AP_IOV_OP_MANUAL_STA_BSSCFG_CREATE)) &&
					((!sta && BSSCFG_STA(bsscfg)) ||
					(sta && BSSCFG_AP(bsscfg)))) {
					type.subtype = !sta ? BSSCFG_GENERIC_AP :
						BSSCFG_GENERIC_STA;
					wlc_bsscfg_reinit(wlc, bsscfg, &type, 0);
				}
			} /* bsscfg is found */
#ifdef WLRSDB
			/* RSDB OVERRIDE: IOV_BSS does not follow the usual bsscfg: prefix model */
			if (RSDB_ENAB(wlc->pub) && (bsscfg != NULL))
				wlc = bsscfg->wlc;
#endif /* WLRSDB */
		}

#ifdef STA
		if (int_val2 == WLC_AP_IOV_OP_MANUAL_STA_BSSCFG_CREATE)
			break;
		else if (int_val2 == WLC_AP_IOV_OP_MANUAL_AP_BSSCFG_CREATE)
			break;
#endif // endif
		if (bsscfg == NULL) {
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
		} else {
			if (!bsscfg->enable) {
				WL_APSTA_UPDN(("wl%d: Ignoring DOWN, bsscfg %d already DISABLED\n",
					wlc->pub->unit, int_val));
				break;
			}
			WL_APSTA_UPDN(("wl%d: BSS down on %d (%s) -> wlc_bsscfg_disable()\n",
				wlc->pub->unit, int_val, (BSSCFG_AP(bsscfg) ? "AP" : "STA")));
			wlc_bsscfg_disable(wlc, bsscfg);
#ifdef WLDFS
			/* Turn of radar_detect if none of AP's are on radar chanspec */
			if (WLDFS_ENAB(wlc->pub) && WL11H_AP_ENAB(wlc)) {
				if (!wlc_ap_on_radarchan(wlc->ap) && !(BSSCFG_SRADAR_ENAB(bsscfg) ||
					BSSCFG_AP_NORADAR_CHAN_ENAB(bsscfg)))
					wlc_set_dfs_cacstate(wlc->dfs, OFF, bsscfg);
			}
#endif // endif
		}
		break;
	}

#if defined(STA) /* APSTA */
	case IOV_GVAL(IOV_APSTA):
		*ret_int_ptr = APSTA_ENAB(wlc->pub);
		break;

	case IOV_SVAL(IOV_APSTA): {
		wlc_bsscfg_type_t type = {BSSCFG_TYPE_GENERIC, BSSCFG_SUBTYPE_NONE};

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
		} else {
			/* Turn off APSTA: make global AP and cfg[0] same */
			WL_APSTA(("wl%d: Disabling APSTA mode\n", wlc->pub->unit));
		}
		type.subtype = bool_val ? BSSCFG_GENERIC_STA : BSSCFG_GENERIC_AP;
		err = wlc_bsscfg_reinit(wlc, bsscfg, &type, 0);
		if (err)
			break;
		wlc->pub->_ap = TRUE;
		if (bool_val) {
			wlc->default_bss->bss_type = DOT11_BSSTYPE_INFRASTRUCTURE;
		}
		wlc->pub->_apsta = bool_val;

		/* Act similarly to WLC_SET_AP */
		wlc_ap_upd(wlc, bsscfg);
		wlc->wet = FALSE;
		wlc->wet_dongle = FALSE;
		wlc_radio_mpc_upd(wlc);
		break;
	}
#endif /* APSTA */

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
		if (RADIONOA_PWRSAVE_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		*ret_int_ptr = ap->radio_pwrsave_enable;
		break;

	case IOV_SVAL(IOV_RADIO_PWRSAVE_ENABLE):
		if (RADIONOA_PWRSAVE_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}

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
		if (RADIONOA_PWRSAVE_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		*ret_int_ptr = appvt->radio_pwrsave.pwrsave.quiet_time;
		break;

	case IOV_SVAL(IOV_RADIO_PWRSAVE_QUIET_TIME):
		if (RADIONOA_PWRSAVE_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		appvt->radio_pwrsave.pwrsave.quiet_time = int_val;
		break;
	case IOV_GVAL(IOV_RADIO_PWRSAVE_PPS):
		if (RADIONOA_PWRSAVE_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		*ret_int_ptr = appvt->radio_pwrsave.pwrsave.pps_threshold;
		break;

	case IOV_SVAL(IOV_RADIO_PWRSAVE_PPS):
		if (RADIONOA_PWRSAVE_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		appvt->radio_pwrsave.pwrsave.pps_threshold = int_val;
		break;
	case IOV_GVAL(IOV_RADIO_PWRSAVE):
		if (RADIONOA_PWRSAVE_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		*ret_int_ptr = appvt->radio_pwrsave.pwrsave.in_power_save;
		break;
	case IOV_SVAL(IOV_RADIO_PWRSAVE_LEVEL):{
		uint8 dtim_period;
		uint16 beacon_period;

		if (RADIONOA_PWRSAVE_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}

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
		if (RADIONOA_PWRSAVE_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		*ret_int_ptr = appvt->radio_pwrsave.level;
		break;
	case IOV_SVAL(IOV_RADIO_PWRSAVE_STAS_ASSOC_CHECK):
		if (RADIONOA_PWRSAVE_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		appvt->radio_pwrsave.pwrsave.stas_assoc_check = int_val;
		if (int_val && RADIO_PWRSAVE_ENAB(wlc->ap) &&
		    wlc_radio_pwrsave_in_power_save(wlc->ap) &&
		    wlc_ap_stas_associated(wlc->ap)) {
			wlc_radio_pwrsave_exit_mode(wlc->ap);
			WL_INFORM(("Going out of power save as there are associated STASs!\n"));
		}
		break;
	case IOV_GVAL(IOV_RADIO_PWRSAVE_STAS_ASSOC_CHECK):
		if (RADIONOA_PWRSAVE_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}
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

		scb_probe.scb_timeout = appvt->scb_timeout;
		scb_probe.scb_activity_time = appvt->scb_activity_time;
		scb_probe.scb_max_probe = appvt->scb_max_probe;

		bcopy((char *)&scb_probe, (char *)arg, sizeof(wl_scb_probe_t));
		break;
	}

	case IOV_SVAL(IOV_SCB_PROBE): {
		wl_scb_probe_t *scb_probe = (wl_scb_probe_t *)arg;

		if (!scb_probe->scb_timeout || (!scb_probe->scb_max_probe)) {
			err = BCME_BADARG;
			break;
		}

		appvt->scb_timeout = scb_probe->scb_timeout;
		appvt->scb_activity_time = scb_probe->scb_activity_time;
		appvt->scb_max_probe = scb_probe->scb_max_probe;
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

		if (wlc->pub->up && (appvt->chanspec_selected != 0) &&
		    (WLC_BAND_PI_RADIO_CHANSPEC != appvt->chanspec_selected))
			wlc_ap_acs_update(wlc);

#ifdef WLDFS
		if (WLDFS_ENAB(wlc->pub) && WL11H_AP_ENAB(wlc) && AP_ACTIVE(wlc)) {
			if (wlc_radar_chanspec(wlc->cmi, appvt->chanspec_selected))
				wlc_set_dfs_cacstate(wlc->dfs, ON, bsscfg);
			else
				wlc_set_dfs_cacstate(wlc->dfs, OFF, bsscfg);
		}
#endif // endif
		break;

	case IOV_GVAL(IOV_PROBE_RESP_INFO): {
		uint tmp_len = len - sizeof(int_val);

		err = wlc_ap_get_bcnprb(wlc, bsscfg, FALSE, ((int32*)arg + 1), &tmp_len);
		if (err == BCME_OK) {
			*ret_int_ptr = (int32)tmp_len;
		}

	        break;
	}
	case IOV_GVAL(IOV_MODE_REQD):
		*ret_int_ptr = (int32)wlc_ap_get_opmode_cap_reqd(wlc, bsscfg);
		break;
	case IOV_SVAL(IOV_MODE_REQD):
		err = wlc_ap_set_opmode_cap_reqd(wlc, bsscfg, int_val);
		break;

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

	case IOV_SVAL(IOV_WOWL_PKT): {
	        /* Send a Wake packet to a ea */
	        /* The format of this iovar params is
		 *   pkt_len
		 *   dst
		 *   Type of pkt - WL_WOWL_MAGIC or WL_WOWL_NET
		 *   for net pkt, wl_wowl_pattern_t
		 *   for magic pkt, dst ea, sta ea
		 */
		struct type_len_ea {
			uint16 pktlen;
			struct ether_addr dst;
			uint16 type;
		} tlea;

		wl_wowl_pattern_t wl_pattern;
		void *pkt;
		uint loc = 0;
		struct ether_header *eh;

		bcopy((uint8*)arg, (uint8 *) &tlea, sizeof(struct type_len_ea));
		loc += sizeof(struct type_len_ea);

		if (tlea.type  != WL_WOWL_NET &&
		    tlea.type  != WL_WOWL_MAGIC &&
			tlea.type  != WL_WOWL_EAPID) {
			err = BCME_BADARG;
			break;
		}

		if (tlea.type == WL_WOWL_NET) {
			uint8 *pattern;
			uint8 *buf;
			uint8 *wakeup_reason;

			bcopy(((uint8 *)arg + loc),
				(uint8 *) &wl_pattern, sizeof(wl_wowl_pattern_t));
			if (tlea.pktlen <
					(wl_pattern.offset + wl_pattern.patternsize
						+ wl_pattern.reasonsize)) {
				err = BCME_RANGE;
				break;
			}

			if ((pkt = PKTGET(wlc->osh, tlea.pktlen +
			                  sizeof(struct ether_header) + TXOFF,
			                  TRUE)) == NULL) {
				err = BCME_NOMEM;
				break;
			}

			PKTPULL(wlc->osh, pkt, TXOFF);

			eh = (struct ether_header *)PKTDATA(wlc->osh, pkt);
			buf = (uint8 *)eh;

			/* Go to end to find pattern */
			pattern = ((uint8*)arg + loc + wl_pattern.patternoffset);
			bcopy(pattern, &buf[wl_pattern.offset], wl_pattern.patternsize);
	                if (wl_pattern.reasonsize) {
				wakeup_reason = ((uint8*)pattern + wl_pattern.patternsize);
				bcopy(wakeup_reason,
					&buf[wl_pattern.offset + wl_pattern.patternsize],
					wl_pattern.reasonsize);
			}
			bcopy((char *)&wlc->pub->cur_etheraddr, (char *)eh->ether_shost,
			      ETHER_ADDR_LEN);
			if (wl_pattern.offset >= ETHER_ADDR_LEN)
				bcopy((char *)&tlea.dst, (char *)eh->ether_dhost, ETHER_ADDR_LEN);

			wl_msg_level |= WL_PRPKT_VAL;
			wlc_sendpkt(wlc, pkt, NULL);
			wl_msg_level &= ~WL_PRPKT_VAL;
		} else if (tlea.type == WL_WOWL_MAGIC) {
			struct ether_addr ea;
			uint8 *buf, ptr, extended_magic_pattern[6];
			bool extended_magic = FALSE;
			int i, j;

			if (tlea.pktlen < MAGIC_PKT_MINLEN) {
				err = BCME_RANGE;
				break;
			}

			bcopy(((uint8 *)arg + loc), (char *)&ea, ETHER_ADDR_LEN);

			if (wlc_iovar_op(wlc, "wowl_ext_magic", NULL, 0,
				extended_magic_pattern, sizeof(extended_magic_pattern),
				IOV_GET, NULL) == BCME_OK)
				extended_magic = TRUE;

			if ((pkt = PKTGET(wlc->osh, tlea.pktlen +
				(extended_magic ? sizeof(extended_magic_pattern) : 0) +
			                  sizeof(struct ether_header) + TXOFF,
			                  TRUE)) == NULL) {
				err = BCME_NOMEM;
				break;
			}
			PKTPULL(wlc->osh, pkt, TXOFF);
			eh = (struct ether_header *)PKTDATA(wlc->osh, pkt);
			bcopy((char *)&wlc->pub->cur_etheraddr, (char *)eh->ether_shost,
			      ETHER_ADDR_LEN);
			bcopy((char *)&tlea.dst, (char*)eh->ether_dhost, ETHER_ADDR_LEN);
			eh->ether_type = hton16(ETHER_TYPE_MIN);
			buf = ((uint8 *)eh + sizeof(struct ether_header));
			for (i = 0; i < 6; i++)
				buf[i] = 0xff;
			ptr = 6;
			for (j = 0; j < 16; j++) {
				bcopy(&ea, buf + ptr, ETHER_ADDR_LEN);
				ptr += 6;
			}
			if (extended_magic)
				bcopy(extended_magic_pattern,
					buf + ptr,
					sizeof(extended_magic_pattern));
			wl_msg_level |= WL_PRPKT_VAL;
			wlc_sendpkt(wlc, pkt, NULL);
			wl_msg_level &= ~WL_PRPKT_VAL;
		} else if (tlea.type == WL_WOWL_EAPID) {
			uint8 id_hdr[] = {0x01, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01};
			uint8 id_len = *((uint8 *)arg + loc);
			uint16 body_len = 5 + id_len;
			uint8 *buf;

			if ((pkt = PKTGET(wlc->osh, sizeof(id_hdr) + id_len +
			                  sizeof(struct ether_header) + TXOFF,
			                  TRUE)) == NULL) {
				err = BCME_NOMEM;
				break;
			}
			PKTPULL(wlc->osh, pkt, TXOFF);

			eh = (struct ether_header *)PKTDATA(wlc->osh, pkt);
			bcopy((char *)&wlc->pub->cur_etheraddr, (char *)eh->ether_shost,
			      ETHER_ADDR_LEN);
			bcopy((char *)&tlea.dst, (char*)eh->ether_dhost, ETHER_ADDR_LEN);
			eh->ether_type = hton16(ETHER_TYPE_802_1X);

			*((uint16*)&id_hdr[2]) = hton16(body_len);
			*((uint16*)&id_hdr[6]) = hton16(body_len);

			buf = (uint8 *)eh + sizeof(struct ether_header);
			bcopy(id_hdr, buf, sizeof(id_hdr));
			buf += sizeof(id_hdr);
			bcopy((uint8*)arg + loc + 1, buf, id_len);

			wl_msg_level |= WL_PRPKT_VAL;
			wlc_sendpkt(wlc, pkt, NULL);
			wl_msg_level &= ~WL_PRPKT_VAL;
		} else
			err = BCME_UNSUPPORTED;
		break;
	}
	case IOV_SVAL(IOV_SET_RADAR): {
		uint subband;
#ifndef WLDFS
		BCM_REFERENCE(subband);
#endif /* WLDFS */
		if (p_len >= sizeof(int) * 2) {
			subband = (uint) int_val2;
		} else {
			subband = DFS_DEFAULT_SUBBANDS;
		}
		if (WLDFS_ENAB(wlc->pub)) {
			ASSERT(ret_int_ptr != NULL);
			*ret_int_ptr = wlc_dfs_set_radar(wlc->dfs, int_val, subband);
			if (*ret_int_ptr != BCME_OK) {
				err = *ret_int_ptr;
			}
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;
	}
	case IOV_GVAL(IOV_SET_RADAR):
		ASSERT(ret_int_ptr != NULL);
		if (WLDFS_ENAB(wlc->pub)) {
			*ret_int_ptr = (int32)wlc_dfs_get_radar(wlc->dfs);
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;

#ifdef WL_MCAST_FILTER_NOSTA
	case IOV_SVAL(IOV_MCAST_FILTER_NOSTA):
		bsscfg->mcast_filter_nosta = bool_val;
		break;
	case IOV_GVAL(IOV_MCAST_FILTER_NOSTA):
		*ret_int_ptr = (int32)bsscfg->mcast_filter_nosta;
		break;
#endif // endif

#if defined(BCM_CEVENT) && !defined(BCM_CEVENT_DISABLED)
	case IOV_SVAL(IOV_CEVENT):
		wlc->pub->_cevent = (uint32)int_val;
		break;

	case IOV_GVAL(IOV_CEVENT):
		*ret_int_ptr = wlc->pub->_cevent;
		break;
#endif /* BCM_CEVENT && ! BCM_CEVENT_DISABLED */

#ifdef WL_TRAFFIC_THRESH
	case IOV_SVAL(IOV_TRF_THOLD):
	{
		wl_traffic_thresh_t *req = (wl_traffic_thresh_t *) params;
		if (p_len < sizeof(wl_traffic_thresh_t)) {
			WL_ERROR(("wl%d: size error\n", wlc->pub->unit));
			err = BCME_USAGE_ERROR;
			break;
		}
		if (BSSCFG_STA(bsscfg)) {
			WL_INFORM(("wl%d: traffic_thresh settings not supported for sta\n",
					wlc->pub->unit));
			err = BCME_UNSUPPORTED;
			break;
		}
		err = wlc_traffic_thresh_set_val(wlc, bsscfg, req);
	}
		break;
	case IOV_GVAL(IOV_TRF_THOLD):
	{
		wl_traffic_thresh_t *req = (wl_traffic_thresh_t *) params;
		struct get_traffic_thresh *res = (struct get_traffic_thresh *) arg;
		if (p_len < sizeof(wl_traffic_thresh_t)) {
			WL_ERROR(("wl%d: size error\n", wlc->pub->unit));
			err = BCME_USAGE_ERROR;
			break;
		}
		if (req->type == WL_TRF_DU) {
			err = wlc_traffic_thresh_get_val(wlc, bsscfg, req, res);
		}
	}

		break;
#endif /* WL_TRAFFIC_THRESH */
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
	case IOV_SVAL(IOV_MAP):
	{
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
		(void)wlc_update_multiap_ie(wlc, bsscfg);
	}
	break;
#endif	/* MULTIAP */
	case IOV_SVAL(IOV_FAR_STA_RSSI):
	if (int_val <= 0) {
		bsscfg->far_sta_rssi = int_val;
	}
	break;

	case IOV_GVAL(IOV_FAR_STA_RSSI):
	*ret_int_ptr = (int32)bsscfg->far_sta_rssi;
	break;
	case IOV_GVAL(IOV_KEEP_AP_UP):
		*ret_int_ptr = (bool)wlc->keep_ap_up;
	break;
	case IOV_SVAL(IOV_KEEP_AP_UP):
	{
		wlc->keep_ap_up = bool_val;
		break;
	}

	case IOV_GVAL(IOV_BW_SWITCH_160):
		*ret_int_ptr = wlc->pub->_bw_switch_160;
	break;

	case IOV_SVAL(IOV_BW_SWITCH_160):

		if (int_val < WLC_BW_SWITCH_DISABLE || int_val > WLC_BW_SWITCH_ENABLE) {
			err = BCME_UNSUPPORTED;
			break;
		}
		wlc->pub->_bw_switch_160 = int_val;
	break;

	case IOV_GVAL(IOV_SCB_MARK_DEL_TIMEOUT):
		*ret_int_ptr = appvt->scb_mark_del_timeout;
	break;

	case IOV_SVAL(IOV_SCB_MARK_DEL_TIMEOUT):
		appvt->scb_mark_del_timeout = int_val;
	break;

	case IOV_GVAL(IOV_BCNPRS_TXPWR_OFFSET): {
		/* Return db unit */
		*ret_int_ptr = wlc->stf->bcnprs_txpwr_offset;
	break;
	}

	case IOV_SVAL(IOV_BCNPRS_TXPWR_OFFSET): {
		uint8	offset = (uint8)(int_val & 0xff);
		uint8	limit_offset = 0xF;

		/* limitation of supported backoff range: 31.5 vs 15.5dB */
		limit_offset = (D11REV_GE(wlc->pub->corerev, 64)) ? 0x1F : 0xF;
		offset = (offset >= limit_offset)? limit_offset : offset;

		wlc->stf->bcnprs_txpwr_offset = offset;

		if (!BSSCFG_AP(bsscfg) || !bsscfg->up || !wlc->clk) {
			err = BCME_NOTUP;
			goto exit;
		}

		if (D11REV_IS(wlc->pub->corerev, 30)) {
			/* Mimophy rev7, Lcnxnphy rev0 uses 4.1 format */
			wlc_write_shm(wlc, M_BCN_POWER_ADJUST(wlc), ((uint8)((offset << 1) & 0xf)));
			wlc_write_shm(wlc, M_PRS_POWER_ADJUST(wlc), ((uint8)((offset << 1) & 0xf)));
		} else if (D11REV_GE(wlc->pub->corerev, 40)) {
			wlc_suspend_mac_and_wait(wlc);
			wlc_beacon_phytxctl(wlc, wlc->bcn_rspec, wlc->chanspec);
			wlc_enable_mac(wlc);
		} else {
			err = BCME_UNSUPPORTED;
		}

		break;
	}

	case IOV_GVAL(IOV_TXBCN_TIMEOUT):
		*ret_int_ptr = appvt->txbcn_timeout;
		break;

	case IOV_SVAL(IOV_TXBCN_TIMEOUT):
		appvt->txbcn_timeout = int_val;
		break;

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

exit:
	return err;
}
/* initialize BSS when radio/phy set the target channel */
static void
wlc_ap_upd_onchan(wlc_ap_info_t* ap, wlc_bsscfg_t *cfg)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *)ap;
	wlc_info_t *wlc = appvt->wlc;
	int32 ret = BCME_OK;
	chanspec_t chspec;
#ifdef WDS
	wlc_if_t *wlcif;
	struct scb_iter scbiter;
	struct scb *scb;
#endif // endif
	WL_INFORM(("wl%d: BSS %d is up\n", wlc->pub->unit, cfg->_idx));

	wlc_suspend_mac_and_wait(wlc);
	wlc_BSSinit(wlc, cfg->target_bss, cfg, WLC_BSS_START);
	wlc_ap_up_upd(wlc->ap, cfg, TRUE);

	/* For corerev 30 (eg. 43217), on switching from 20 to 40MHz, update control ch position */
	if (D11REV_IS(wlc->pub->corerev, 30)) {
		uint16 bcn_offset;
		if (HWBCN_ENAB(cfg)) {
			wlc_beacon_phytxctl(wlc, wlc->bcn_rspec, wlc->chanspec);
		}
		bcn_offset = wlc_compute_bcntsfoff(wlc, wlc->bcn_rspec, FALSE, TRUE);
		ASSERT(bcn_offset != 0);
		wlc->band->bcntsfoff = bcn_offset;
		wlc_write_shm(wlc, M_BCN_TXTSF_OFFSET(wlc), bcn_offset);
	}
	wlc_enable_mac(wlc);

	/* update current_bss SSID */
	cfg->current_bss->SSID_len = cfg->SSID_len;
	bcopy(cfg->SSID, cfg->current_bss->SSID, cfg->SSID_len);

#if defined(WLRSDB) && defined(WL_MODESW)
	if (WLC_MODESW_ENAB(wlc->pub)) {
		wlc_set_ap_up_pending(wlc, cfg, FALSE);
	}
#endif // endif
#ifdef WLMCNX
	if (MCNX_ENAB(wlc->pub)) {
		wlc_mcnx_bss_upd(wlc->mcnx, cfg, TRUE);
	}
#endif // endif
	if (BSS_WME_ENAB(wlc, cfg)) {
		wlc_edcf_acp_apply(wlc, cfg, TRUE);
	}
	wlc_led_event(wlc->ledh);

	/* Interface up notification */
	WL_APSTA_UPDN(("Reporting link up on config %d (AP enabled)\n",
		WLC_BSSCFG_IDX(cfg)));
	wlc_link(wlc, TRUE, &cfg->cur_etheraddr, cfg, 0);
#ifdef WDS
	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {
		if ((wlcif = SCB_WDS(scb)) != NULL) {
			/* send WLC_E_LINK event with status UP to WDS interface */
			wlc_wds_create_link_event(wlc, wlcif->u.scb, TRUE);
		}
	}
#endif // endif
	chspec = wlc_get_home_chanspec(cfg);
	BCM_REFERENCE(chspec);
#ifdef WL11K_AP
	wlc_rrm_add_pilot_timer(wlc, cfg);
#endif // endif
	if ((ret = wlc_bsscfg_bcmc_scb_init(wlc, cfg)) != BCME_OK) {
		ASSERT(!(ret == BCME_NOMEM));
	}
#ifdef RADAR
	if (RADAR_ENAB(wlc->pub) && WL11H_AP_ENAB(wlc) && AP_ACTIVE(wlc) &&
			wlc_radar_chanspec(wlc->cmi, chspec)) {
		wlc_set_dfs_cacstate(wlc->dfs, ON, cfg);
	} else {
		wlc_set_dfs_cacstate(wlc->dfs, OFF, cfg);
	}
#endif /* RADAR */
}

static void
wlc_ap_acs_update(wlc_info_t *wlc)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *)wlc->ap;

	WL_INFORM(("wl%d: %s: changing chanspec to %d\n",
		wlc->pub->unit, __FUNCTION__, appvt->chanspec_selected));
	wlc_set_home_chanspec(wlc, appvt->chanspec_selected);
	wlc_suspend_mac_and_wait(wlc);

	wlc_set_chanspec(wlc, appvt->chanspec_selected, CHANSW_REASON(CHANSW_IOVAR));
	if (AP_ENAB(wlc->pub)) {
		wlc->bcn_rspec = wlc_lowest_basic_rspec(wlc,
			&wlc->cfg->current_bss->rateset);
		ASSERT(wlc_valid_rate(wlc, wlc->bcn_rspec,
			CHSPEC_IS2G(wlc->cfg->current_bss->chanspec) ?
			WLC_BAND_2G : WLC_BAND_5G, TRUE));
		wlc_beacon_phytxctl(wlc, wlc->bcn_rspec, wlc->chanspec);
		wlc_beacon_upddur(wlc, wlc->bcn_rspec, 0);
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

	if (BSSCFG_AP(cfg)) {
		/* allocate the AID map */
		if ((ap_cfg->aidmap = (uint8 *) MALLOCZ(appvt->wlc->osh, AIDMAPSZ)) == NULL) {
			WL_ERROR(("%s: failed to malloc aidmap\n", __FUNCTION__));
			return BCME_NOMEM;
		}
	}

	return 0;
}

static void
wlc_ap_bsscfg_deinit(void *context, wlc_bsscfg_t *cfg)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *)context;
	ap_bsscfg_cubby_t *ap_cfg;

	ASSERT(appvt != NULL);
	ASSERT(cfg != NULL);

	ap_cfg = AP_BSSCFG_CUBBY(appvt, cfg);
	if (ap_cfg == NULL)
		return;

	if (ap_cfg->msch_req_hdl) {
		wlc_msch_timeslot_unregister(appvt->wlc->msch_info, &ap_cfg->msch_req_hdl);
	}

	/* deallocate the AID map */
	if (ap_cfg->aidmap != NULL) {
		MFREE(appvt->wlc->osh, ap_cfg->aidmap, AIDMAPSZ);
		ap_cfg->aidmap = NULL;
	}
}

#if defined(BCMDBG)
static void
wlc_ap_bsscfg_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b)
{
	wlc_ap_info_pvt_t *appvt = ctx;
	ap_bsscfg_cubby_t *ap_cfg;

	ap_cfg = AP_BSSCFG_CUBBY(appvt, cfg);
	if (ap_cfg == NULL) {
		return;
	}

	bcm_bprintf(b, "     msch state %u\n", ap_cfg->msch_state);
	if (ap_cfg->aidmap != NULL) {
		bcm_bprhex(b, "     aidmap: ", TRUE, ap_cfg->aidmap, AIDMAPSZ);
	}
}
#endif // endif

#ifdef KEEP_THEM_AS_REF
/* XXX
 * AID2AIDMAP() and AIDMAP2AID() are the original AID utils version;
 * wlc_ap_aid2aidmap() and wlc_ap_aidmap2aid() are the new version
 * with partial BSS color support. Keep them here for reference as
 * the history seems have been lost.
 */
#define AID2AIDMAP(aid)	((aid & 0x3fff) - 1)
/* set the 2 MSBs of the AID */
#define AIDMAP2AID(pos)	((pos + 1) | 0xc000)
#endif // endif

/* Conversion between internal aidmap and external AID with
 * partial BSS color support.
 *
 * Partial BSS Color is based on Draft P802.11ax/D0.5 section 11.49.1
 * AID assign rule.
 */
static uint16
wlc_ap_aid2aidmap(wlc_ap_info_pvt_t *appvt, wlc_bsscfg_t *cfg, uint16 aid)
{
	uint16 pos = aid & DOT11_AID_MASK;

	ASSERT(cfg != NULL);

#ifdef WL11AX
	if (HE_ENAB(appvt->wlc->pub) &&
		wlc_he_partial_bss_color_get(appvt->wlc->hei, cfg, NULL)) {
		/* strip off partial BSS color from AID bit[5:8] */
		pos =	/* bit[0:4] */
		        (pos & 0x1f) |
		        /* bit[9:15] */
		        ((pos & 0x3e00) >> 4);
	}
#endif /* WL11AX */

#ifdef TESTBED_AP_11AX
	/* Uggliness explanation: Without properly fixing it.... This is testbed feature after
	 * all. So what is issue: mbssid gets turned on when some scbs have been enabled (bcast)
	 * aid 1. They get cleaned up after feature is enabled. As a result we clean up AID which
	 * has 'old' AID calculation. The simple workaround is this: As long as aid is higher then
	 * MAXBSSID we can substract it if mbssid is active. The idea is that MBBSID cannot be
	 * enabled with many AIDs assigned, since we have to be down..... This needs different fix
	 * if feature will ever make it to real product.
	 */
	if (wlc_mbss_mbssid_active(appvt->wlc) && (pos >= WLC_MBSS_MAXBSSID)) {
		return pos - WLC_MBSS_MAXBSSID;
	} else
#endif /* TESTBED_AP_11AX */
	return pos - 1;
}

/* 'pos' is the bit position in aidmap */
static uint16
wlc_ap_aidmap2aid(wlc_ap_info_pvt_t *appvt, wlc_bsscfg_t *cfg, uint16 pos)
{
	uint16 aid = pos;
	uint8 color;

#ifdef TESTBED_AP_11AX
	/* The first x AIDs are reserved for BCMC traffic. x is 1 for normal configuration, but
	 * when mbssid becomes active the then x becomes max mbss supported.
	 */
	if (wlc_mbss_mbssid_active(appvt->wlc)) {
		aid += WLC_MBSS_MAXBSSID; /* Max number of total MBSS */
		if (aid > (uint)appvt->wlc->pub->tunables->maxscb) {
			return 0;
		}
	} else
#endif /* TESTBED_AP_11AX */
	/* The first AID is used fro BCMC */
	aid += 1;

	ASSERT(cfg != NULL);

	BCM_REFERENCE(color);

#ifdef WL11AX
	if (HE_ENAB(appvt->wlc->pub) &&
		wlc_he_partial_bss_color_get(appvt->wlc->hei, cfg, &color)) {
		/* insert partial BSS color to AID bit[5:8] */
		aid =	/* bit[0:4] */
		        (aid & 0x1f) |
		        /* bit[5:8] */
		        (((color + (cfg->BSSID.octet[5] ^ (cfg->BSSID.octet[5] >> 4))) &
		          0x0f) << 5) |
		        /* bit[9:15] */
		        ((aid & 0x03e0) << 4);
	}
#endif /* WL11AX */

	/* set the 2 MSBs of the AID */
	return aid | ~DOT11_AID_MASK;
}

/* Return the max. AID plus 1 the AP could assign.
 * The usage of the return value is for PVB update/validation.
 * See wlc_apps.c for usage.
 */
uint16
wlc_ap_aid_max(wlc_ap_info_t *ap, wlc_bsscfg_t *cfg)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *)ap;
	wlc_info_t *wlc = appvt->wlc;

#ifdef TESTBED_AP_11AX
	if (wlc_mbss_mbssid_active(appvt->wlc)) {
		return wlc_ap_aidmap2aid(appvt, cfg, (uint16)wlc->pub->tunables->maxscb -
			WLC_MBSS_MAXBSSID);
	} else
#endif /* TESTBED_AP_11AX */
	return wlc_ap_aidmap2aid(appvt, cfg, (uint16)wlc->pub->tunables->maxscb);
}

#ifdef WLRSDB
static int
wlc_ap_bss_get(void *ctx, wlc_bsscfg_t *from_cfg, uint8 *data, int *len)
{
	wlc_ap_info_pvt_t *appvt;
	ap_bsscfg_cubby_t *ap_cfg;
	ap_bsscfg_cubby_copy_t *cp = (ap_bsscfg_cubby_copy_t *)data;
	int config_len = AP_BSSCFG_CUBBY_LEN;

	ASSERT(ctx != NULL);

	if (!BSSCFG_AP(from_cfg)) {
		*len = 0;
		return BCME_OK;
	}
	ASSERT(from_cfg != NULL);

	appvt = (wlc_ap_info_pvt_t *)ctx;
	ap_cfg = AP_BSSCFG_CUBBY(appvt, from_cfg);
	ASSERT(ap_cfg != NULL);

	if (config_len > *len) {
		*len = config_len;
		return BCME_BUFTOOSHORT;
	}

	cp->authresp_macfltr = ap_cfg->authresp_macfltr;
	if (ap_cfg->aidmap) {
		cp->aidmap = (uint8*)cp + sizeof(*cp);
		memcpy(cp->aidmap, ap_cfg->aidmap, AIDMAPSZ);
	}
	cp->opmode_cap_reqd = ap_cfg->opmode_cap_reqd;

	*len = config_len;
	return BCME_OK;
}

static int
wlc_ap_bss_set(void *ctx, wlc_bsscfg_t *to_cfg, const uint8 *data, int len)
{
	wlc_ap_info_pvt_t *appvt;
	ap_bsscfg_cubby_t *ap_cfg;
	const ap_bsscfg_cubby_copy_t *cp = (const ap_bsscfg_cubby_copy_t *)data;

	ASSERT(ctx != NULL);

	if (!BSSCFG_AP(to_cfg)) {
		return BCME_OK;
	}
	ASSERT(to_cfg != NULL);

	appvt = (wlc_ap_info_pvt_t *)ctx;
	ap_cfg = AP_BSSCFG_CUBBY(appvt, to_cfg);
	ASSERT(ap_cfg != NULL);

	ap_cfg->authresp_macfltr = cp->authresp_macfltr;
	if (cp->aidmap) {
		if (!ap_cfg->aidmap) {
			return BCME_NOMEM;
		}
		memcpy(ap_cfg->aidmap, cp->aidmap, AIDMAPSZ);
	}
	ap_cfg->opmode_cap_reqd = cp->opmode_cap_reqd;
	return BCME_OK;

}
#endif /* WLRSDB */

static uint16
wlc_bsscfg_newaid(wlc_ap_info_t *ap, wlc_bsscfg_t *cfg)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *)ap;
	wlc_info_t *wlc = appvt->wlc;
	ap_bsscfg_cubby_t *ap_cfg;
	uint16 pos, i;	/* bit position in aidmap */
	wlc_bsscfg_t *bsscfg;

	ASSERT(cfg);

	ap_cfg = AP_BSSCFG_CUBBY(appvt, cfg);

	if (ap_cfg->aidmap == NULL)
		return 0;

	/* get an unused number from aidmap */
	for (pos = 0; pos < (uint)wlc->pub->tunables->maxscb; pos++) {
		if (isclr(ap_cfg->aidmap, pos)) {
			WL_ASSOC(("wlc_bsscfg_newaid marking bit = %d for "
			          "bsscfg %d AIDMAP\n", pos,
			          WLC_BSSCFG_IDX(cfg)));
			/* mark the position being used */
			setbit(ap_cfg->aidmap, pos);
			/* To keep the AID unique across BSSes, mark the same position
			 * across other BSSes so that it will not be used for those BSSes.
			 * Note that the aidmap for a BSSCFG no more represents ONLY the STAs
			 * associated to that BSS but more than those.
			 */
			FOREACH_BSS(wlc, i, bsscfg) {
				ap_cfg = AP_BSSCFG_CUBBY(appvt, bsscfg);
				if (ap_cfg->aidmap) {
					setbit(ap_cfg->aidmap, pos);
				}
			}
			return wlc_ap_aidmap2aid(appvt, cfg, pos);
		}
	}

	/* reached full capacity */
	return 0;
}

static int
wlc_ap_scb_init(void *context, struct scb *scb)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t*)context;
	wlc_bsscfg_t *cfg = SCB_BSSCFG(scb);

	if (BSSCFG_AP(cfg) &&
	    !SCB_INTERNAL(scb)) {
		ap_scb_cubby_t **pap_scb = AP_SCB_CUBBY_LOC(appvt, scb);
		ap_scb_cubby_t *ap_scb = AP_SCB_CUBBY(appvt, scb);
		wlc_info_t *wlc = appvt->wlc;

		ASSERT(ap_scb == NULL);

		if ((ap_scb = wlc_scb_sec_cubby_alloc(wlc, scb, sizeof(*ap_scb))) == NULL) {
			WL_ERROR(("wl%d: %s: mem alloc failed, allocated %d bytes\n",
			          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			return BCME_NOMEM;
		}
		*pap_scb = ap_scb;

		scb->aid = wlc_bsscfg_newaid(&appvt->appub, cfg);
		if (scb->aid == 0) {
			return BCME_NORESOURCE;
		}
	}

	return BCME_OK;
}
#ifdef SPLIT_ASSOC
static int
wlc_as_scb_init(void *context, struct scb *scb)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t*)context;
	wlc_bsscfg_t *cfg = SCB_BSSCFG(scb);
	wlc_info_t *wlc = appvt->wlc;

	BCM_REFERENCE(wlc);
	BCM_REFERENCE(cfg);

	if (SPLIT_ASSOC_REQ(cfg) && BSSCFG_AP(cfg) && !SCB_INTERNAL(scb)) {
		wlc_assoc_req_t **pas_scb = AS_SCB_CUBBY_LOC(appvt, scb);
		wlc_assoc_req_t *as_scb = AS_SCB_CUBBY(appvt, scb);

		ASSERT(as_scb == NULL);

		if ((as_scb = wlc_scb_sec_cubby_alloc(wlc, scb, sizeof(*as_scb))) == NULL) {
			WL_ERROR(("wl%d: %s: mem alloc failed, allocated %d bytes\n",
			          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			return BCME_NOMEM;
		}
		*pas_scb = as_scb;
	}

	return BCME_OK;
}

static void
wlc_as_scb_deinit(void *context, struct scb *scb)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t*)context;
	wlc_info_t *wlc = appvt->wlc;
	wlc_bsscfg_t *bsscfg = SCB_BSSCFG(scb);

	BCM_REFERENCE(wlc);

	/* free param body if not freed during assoc stage */
	if (SPLIT_ASSOC_REQ(bsscfg)) {
		wlc_assoc_req_t **pas_scb = AS_SCB_CUBBY_LOC(appvt, scb);
		wlc_assoc_req_t *param = AS_SCB_CUBBY(appvt, scb);

		if (param != NULL) {
			if (param->body != NULL) {
				MFREE(wlc->osh, param->body, param->buf_len);
			}
			wlc_scb_sec_cubby_free(wlc, scb, param);
		}
		*pas_scb = NULL;
	}
}

static uint
wlc_as_scb_secsz(void *context, struct scb *scb)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t*)context;
	wlc_bsscfg_t *cfg = SCB_BSSCFG(scb);
	wlc_info_t *wlc = appvt->wlc;

	BCM_REFERENCE(wlc);
	BCM_REFERENCE(cfg);

	if (SPLIT_ASSOC_REQ(cfg) && BSSCFG_AP(cfg) && !SCB_INTERNAL(scb)) {
		return sizeof(wlc_assoc_req_t);
	}

	return 0;
}

#ifdef WLRSDB
/* As scb cubby update */
static int
wlc_as_scb_update(void *context, struct scb *scb, wlc_bsscfg_t* new_cfg)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t*)context;
	wlc_info_t *new_wlc = new_cfg->wlc;
	wlc_assoc_req_t *as_scb = AS_SCB_CUBBY(appvt, scb);
	as_scb->bsscfg = new_cfg;
	as_scb->ap = new_wlc->ap;
	return BCME_OK;
}
#endif /* WLRSDB */
#endif /* SPLIT_ASSOC */

static void
wlc_ap_scb_deinit(void *context, struct scb *scb)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t*)context;
	wlc_info_t *wlc = appvt->wlc;
	wlc_bsscfg_t *bsscfg = SCB_BSSCFG(scb);
	int i;
	wlc_bsscfg_t *cfg;

#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN], *ea = bcm_ether_ntoa(&scb->ea, eabuf);
#endif /* BCMDBG */
	ap_scb_cubby_t *ap_scb = AP_SCB_CUBBY(appvt, scb);
	ap_bsscfg_cubby_t *ap_cfg = AP_BSSCFG_CUBBY(appvt, bsscfg);

	/* notify the station that we are deauthenticating it */
	if (BSSCFG_AP(bsscfg) && SCB_MARKED_DEAUTH(scb)) {
		(void)wlc_senddeauth(wlc, bsscfg, scb, &scb->ea, &bsscfg->BSSID,
		                     &bsscfg->cur_etheraddr, DOT11_RC_INACTIVITY);
		wlc_deauth_complete(wlc, bsscfg, WLC_E_STATUS_SUCCESS, &scb->ea,
		              DOT11_RC_INACTIVITY, 0);
	}

	if (BSSCFG_AP(bsscfg) && SCB_AUTHENTICATED(scb)) {
		WL_ASSOC(("wl%d: AP: scb free: indicate disassoc for the STA-%s\n",
			wlc->pub->unit, ea));
		wlc_bss_mac_event(wlc, bsscfg, WLC_E_DISASSOC_IND, &scb->ea,
			WLC_E_STATUS_SUCCESS, DOT11_RC_DISASSOC_LEAVING, 0, 0, 0);
	}

	/* mark the aid unused */
	if (ap_cfg != NULL) {
		if (scb->aid && ap_cfg->aidmap) {
			uint16 pos = wlc_ap_aid2aidmap(appvt, bsscfg, scb->aid);
			ASSERT(pos < wlc->pub->tunables->maxscb);
			clrbit(ap_cfg->aidmap, pos);
			/* to keep the AID unique, clear position across other BSSes */
			FOREACH_BSS(wlc, i, cfg) {
				ap_cfg = AP_BSSCFG_CUBBY(appvt, cfg);
				if (ap_cfg->aidmap) {
					clrbit(ap_cfg->aidmap, pos);
				}
			}
		}
	}

	if (ap_scb != NULL) {
		ap_scb_cubby_t **pap_scb = AP_SCB_CUBBY_LOC(appvt, scb);

		/* free any leftover authentication state */
		if (ap_scb->challenge) {
			MFREE(wlc->osh, ap_scb->challenge, 2 + ap_scb->challenge[1]);
			ap_scb->challenge = NULL;
		}

		/* free wpaie if stored */
		if (ap_scb->wpaie) {
			MFREE(wlc->osh, ap_scb->wpaie, ap_scb->wpaie_len);
			ap_scb->wpaie_len = 0;
			ap_scb->wpaie = NULL;
		}

		wlc_scb_sec_cubby_free(wlc, scb, ap_scb);
		*pap_scb = NULL;
	}

}

static uint
wlc_ap_scb_secsz(void *context, struct scb *scb)
{
	wlc_bsscfg_t *cfg = SCB_BSSCFG(scb);

	BCM_REFERENCE(context);
	BCM_REFERENCE(cfg);

	if (BSSCFG_AP(cfg) &&
	    !SCB_INTERNAL(scb)) {
		return sizeof(ap_scb_cubby_t);
	}

	return 0;
}

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
			/* Exit power save mode if channel is quiet or
			 * if BGDFS is in progress
			 */
			if (wlc_quiet_chanspec(wlc->cmi, WLC_BAND_PI_RADIO_CHANSPEC) ||
					wlc_dfs_scan_in_progress(wlc->dfs)) {
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
		/* set in_power_save before actual rxchain switching to ensure
		 * validity of wlc_rxchain_pwrsave_get_rxchain() output
		 */
		pwrsave->in_power_save = TRUE;
		wlc_stf_rxchain_set(wlc, 0x1, FALSE);
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
					/* set in_power_save before rxchain switching to ensure
					 * validity of wlc_rxchain_pwrsave_get_rxchain()
					 */
					pwrsave->in_power_save = TRUE;
					wlc_stf_rxchain_set(wlc, 0x1, TRUE);
				} else
#endif /* RXCHAIN_PWRSAVE */
				{
					pwrsave->in_power_save = TRUE;
				}
				pwrsave->in_power_save_counter++;

				if (RADIONOA_PWRSAVE_ENAB(wlc->pub)) {
					if (wlc_set_rpsnoa(wlc)) {
						pwrsave->in_power_save = FALSE;
						WL_ERROR(("wlc_pwrsave_mode_check, "
							"rps_set_noa error\n"));
					} else {
						/* Clear WAKE bit
						 * if in_power_save flag is set,
						 * wake is cleared in AP mode
						 */
						wlc_set_wake_ctrl(wlc);
					}
				}

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
				if (RADIONOA_PWRSAVE_ENAB(wlc->pub)) {
					wlc_reset_rpsnoa(wlc);
					wlc_reset_pwrsave_mode(ap, PWRSAVE_RADIO);
					/* Set WAKE bit
					 * if in_power_save flag is unset, wake is set in AP mode
					 */
					wlc_set_wake_ctrl(wlc);
				} else {
					wl_del_timer(wlc->wl, appvt->radio_pwrsave.timer);
					if (appvt->radio_pwrsave.radio_disabled) {
						wlc_bmac_radio_hw(wlc->hw, TRUE, FALSE);
						appvt->radio_pwrsave.radio_disabled = FALSE;
					}
					appvt->radio_pwrsave.pwrsave_state = 0;
					appvt->radio_pwrsave.cncl_bcn = FALSE;
				}
#endif /* RADIO_PWRSAVE */
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

		wlc_bmac_radio_hw(wlc->hw, TRUE, FALSE);
		appvt->radio_pwrsave.radio_disabled = FALSE;
		/* Re-enter power save state */
		appvt->radio_pwrsave.pwrsave_state = 2;
	} else {
		WL_INFORM(("wl power timer OFF period starts. disabling radio\n"));
		wlc_radio_pwrsave_off_time_start(ap);
		wlc_bmac_radio_hw(wlc->hw, FALSE, FALSE);
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
	on_time += appvt->pre_tbtt_us;

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
	off_time -= (appvt->pre_tbtt_us + PHY_DISABLE_MAX_LATENCY_us);

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
		if (!RADIONOA_PWRSAVE_ENAB(wlc->pub))
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

	if (RADIONOA_PWRSAVE_ENAB(wlc->pub)) {
		wlc_reset_rpsnoa(wlc);
		wlc_reset_pwrsave_mode(ap, PWRSAVE_RADIO);
		/* Set WAKE bit
		 * if in_power_save flag is unset, wake is set in AP mode
		 */
		wlc_set_wake_ctrl(wlc);
	} else {
		appvt->radio_pwrsave.pwrsave.quiet_time_counter = 0;
		appvt->radio_pwrsave.pwrsave.in_power_save = FALSE;
		wl_del_timer(wlc->wl, appvt->radio_pwrsave.timer);
		if (appvt->radio_pwrsave.radio_disabled) {
			wlc_bmac_radio_hw(wlc->hw, TRUE, FALSE);
			appvt->radio_pwrsave.radio_disabled = FALSE;
		}
		appvt->radio_pwrsave.pwrsave_state = 0;
		appvt->radio_pwrsave.cncl_bcn = FALSE;
	}
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
	duration -= (appvt->pre_tbtt_us + PHY_ENABLE_MAX_LATENCY_us +
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

bool
wlc_radio_pwrsave_bcm_cancelled(const wlc_ap_info_t *ap)
{
	const wlc_ap_info_pvt_t *appvt = (const wlc_ap_info_pvt_t *)ap;

	return appvt->radio_pwrsave.cncl_bcn;
}

#endif /* RADIO_PWRSAVE */

/* ======================= IE mgmt routines ===================== */
/* ============================================================== */
/* Supported Rates IE */
static uint
wlc_assoc_calc_sup_rates_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	wlc_iem_ft_cbparm_t *ftcbparm = data->cbparm->ft;

	BCM_REFERENCE(ctx);

	return TLV_HDR_LEN + ftcbparm->assocresp.sup->count;
}

static int
wlc_assoc_write_sup_rates_ie(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_iem_ft_cbparm_t *ftcbparm = data->cbparm->ft;

	BCM_REFERENCE(ctx);

	bcm_write_tlv(DOT11_MNG_RATES_ID, ftcbparm->assocresp.sup->rates,
		ftcbparm->assocresp.sup->count, data->buf);

	return BCME_OK;
}

/* Extended Supported Rates IE */
static uint
wlc_assoc_calc_ext_rates_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	wlc_iem_ft_cbparm_t *ftcbparm = data->cbparm->ft;

	BCM_REFERENCE(ctx);

	if (ftcbparm->assocresp.ext->count == 0) {
		return 0;
	}

	return TLV_HDR_LEN + ftcbparm->assocresp.ext->count;
}

static int
wlc_assoc_write_ext_rates_ie(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_iem_ft_cbparm_t *ftcbparm = data->cbparm->ft;

	BCM_REFERENCE(ctx);

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

	if (data->ie == NULL || data->ie_len <= TLV_BODY_OFF) {
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

	BCM_REFERENCE(ctx);

	bzero(sup, sizeof(*sup));

	if (data->ie == NULL || data->ie_len <= TLV_BODY_OFF) {
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
		char eabuf[ETHER_ADDR_STR_LEN];
		struct scb *scb = ftpparm->assocreq.scb;
		wlc_info_t *wlc = (wlc_info_t *)ctx;

		WL_ASSOC(("wl%d: %s attempted association with no Supported Rates IE\n",
		          wlc->pub->unit, bcm_ether_ntoa(&scb->ea, eabuf)));
#endif // endif
		/* Ignore enforcing mandatory Support Rates IE check to
		 * workaround some APs in the field not including the IE,
		 * The validation will be done by wlc_combine_rateset()
		 */
		return BCME_OK;
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

	BCM_REFERENCE(ctx);

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
		scb->flags |= SCB_WPSCAP;

		return wlc_scb_save_wpa_ie(wlc, scb, wpsie);
	}

	return BCME_OK;
}

/* ============================================================== */
/* ======================= IE mgmt routines ===================== */

ratespec_t
wlc_force_bcn_rspec(wlc_info_t *wlc)
{
	wlc_ap_info_pvt_t* appvt = (wlc_ap_info_pvt_t*)wlc->ap;
	return appvt->force_bcn_rspec;
}

void
wlc_set_force_bcn_rspec(wlc_info_t *wlc, ratespec_t rspec)
{
	wlc_ap_info_pvt_t* appvt;

	/* Don't validate if force is being reset(0). */
	if (rspec) {
		ASSERT(wlc_valid_rate(wlc, rspec, WLC_BAND_AUTO, TRUE));
	}

	appvt = (wlc_ap_info_pvt_t*)wlc->ap;
	appvt->force_bcn_rspec = rspec;
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
			WLC_BAND_2G : WLC_BAND_5G, TRUE)) {
			return BCME_BADRATESET;
		}

		/* check rspec is basic rate */
		for (i = 0; i < cfg->current_bss->rateset.count; i++) {
			if ((cfg->current_bss->rateset.rates[i] & WLC_RATE_FLAG) &&
			    (cfg->current_bss->rateset.rates[i] & RATE_MASK) == rspec) {
#ifdef WL11N
				wlc_rspec_txexp_upd(wlc, &rspec);
#endif /* WL11N */
				appvt->force_bcn_rspec = rspec;
				break;
			}
		}
		if (i == cfg->current_bss->rateset.count)
			return BCME_UNSUPPORTED;
	} else {
		appvt->force_bcn_rspec = (ratespec_t) 0;
	}

	/* update beacon rate */
	wlc_update_beacon(wlc);

	return BCME_OK;
}

int
wlc_ap_sendauth(wlc_ap_info_t *ap, wlc_bsscfg_t *bsscfg,
	struct scb *scb, int auth_alg,
	int auth_seq, int status, uint8 *challenge_text,
	bool short_preamble, bool send_auth)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *) ap;
	wlc_info_t *wlc = appvt->wlc;

#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN], *sa;
#endif // endif

#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	sa = bcm_ether_ntoa(&scb->ea, eabuf);
#endif // endif
	BCM_REFERENCE(challenge_text);
	/* send authentication response */
	WL_ASSOC(("wl%d.%d %s: %s authenticated\n", wlc->pub->unit,
		WLC_BSSCFG_IDX(bsscfg), __FUNCTION__, sa));

	if ((status == DOT11_SC_SUCCESS) && scb && SCB_AUTHENTICATED(scb)) {
		wlc_bss_mac_event(wlc, bsscfg, WLC_E_AUTH_IND, &scb->ea,
			WLC_E_STATUS_SUCCESS, DOT11_SC_SUCCESS, auth_alg, 0, 0);

#ifdef RXCHAIN_PWRSAVE
		/* fast switch back from rxchain_pwrsave state upon authentication */
		wlc_reset_rxchain_pwrsave_mode(ap);
#endif /* RXCHAIN_PWRSAVE */
	}
#ifdef BCMAUTH_PSK
	if (BSS_AUTH_PSK_ACTIVE(wlc, bsscfg))
		wlc_auth_authreq_recv_update(wlc, bsscfg);
#endif /* BCMAUTH_PSK */

	if (send_auth)
		wlc_sendauth(bsscfg, &scb->ea, &bsscfg->BSSID, scb,
			auth_alg, auth_seq, status, NULL, short_preamble,
			wlc_ap_pre_sendauth_cb, scb, NULL, NULL);

	return BCME_OK;
}

void
wlc_bsscfg_bcn_disable(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	WL_APSTA_UPDN(("wl%d: wlc_bsscfg_bcn_disable %p #of stas %d\n",
	          wlc->pub->unit, OSL_OBFUSCATE_BUF(cfg),
		wlc_bss_assocscb_getcnt(wlc, cfg)));

	cfg->flags &= ~WLC_BSSCFG_HW_BCN;
	if (cfg->up) {
		wlc_suspend_mac_and_wait(wlc);
		wlc_bmac_write_ihr(wlc->hw, 0x47, 3);
		wlc_enable_mac(wlc);
	}
}

void
wlc_bsscfg_bcn_enable(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	WL_APSTA_UPDN(("wl%d: wlc_bsscfg_bcn_enable %p #of stas %d\n",
	          wlc->pub->unit, OSL_OBFUSCATE_BUF(cfg),
		wlc_bss_assocscb_getcnt(wlc, cfg)));

	cfg->flags |= WLC_BSSCFG_HW_BCN;
	wlc_bss_update_beacon(wlc, cfg);
}

void
wlc_ap_reset_grace_attempt(wlc_ap_info_t *ap, struct scb *scb, uint16 cnt)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *)ap;
	ap_scb_cubby_t *ap_scb = AP_SCB_CUBBY(appvt, scb);

	if (ap_scb != NULL && ap_scb->grace_attempts > cnt) {
		ap_scb->grace_attempts = cnt;
	}
}

bool
wlc_ap_sendnulldata(wlc_ap_info_t *ap, wlc_bsscfg_t *cfg, struct scb *scb)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *)ap;
	wlc_info_t *wlc = appvt->wlc;
	ratespec_t rate_override;

	/* If there is fifo drain in process, we do not send the probe. Partly because
	 * sending a probe might interfere with the fifo drain, but mainly because
	 * even if the data probe was successful, we cannot exit ps pretend state
	 * whilst the fifo drain was still happening. So, waiting for the fifo drain
	 * to finish first is clean, predictable and consistent.
	 * Note that when the DATA_BLOCK_PS condition clears, a probe is sent regardless.
	 */
	if (wlc->block_datafifo & DATA_BLOCK_PS) {
		WL_PS(("wl%d.%d: %s: "MACF" DATA_BLOCK_PS pending %d pkts\n",
		       wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
		       ETHER_TO_MACF(scb->ea), TXPKTPENDTOT(wlc)));
		return FALSE;
	}

	/* use the lowest basic rate */
	rate_override = wlc_lowest_basicrate_get(cfg);
	ASSERT(VALID_RATE(wlc, rate_override));

	WL_PS(("wl%d.%d: pps probe to "MACF"\n", wlc->pub->unit,
	        WLC_BSSCFG_IDX(cfg), ETHER_TO_MACF(scb->ea)));

	if (!wlc_sendnulldata(wlc, cfg, &scb->ea, rate_override,
			WLF_PSDONTQ, PRIO_8021D_VO, wlc_ap_sendnulldata_cb, NULL)) {
		WL_ERROR(("wl%d.%d: %s: wlc_sendnulldata failed\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
		return FALSE;
	}

	scb->flags |= SCB_PSPRETEND_PROBE;

	return TRUE;
}

/* Challenge Text */
static uint
wlc_auth_calc_chlng_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	wlc_iem_ft_cbparm_t *ftcbparm = data->cbparm->ft;

	BCM_REFERENCE(ctx);
	if (ftcbparm->auth.alg == DOT11_SHARED_KEY) {
		if (ftcbparm->auth.seq == 2) {
			return TLV_HDR_LEN + DOT11_CHALLENGE_LEN;
		}
	}

	return 0;
}

static int
wlc_auth_write_chlng_ie(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_iem_ft_cbparm_t *ftcbparm = data->cbparm->ft;

	if (ftcbparm->auth.alg == DOT11_SHARED_KEY) {
		wlc_info_t *wlc = (wlc_info_t *)ctx;
		uint16 status = DOT11_SC_SUCCESS;

		if (ftcbparm->auth.seq == 2) {
			wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *)wlc->ap;
			uint8 *chlng;
			wlc_bsscfg_t *cfg = data->cfg;
			struct scb *scb;
#if defined(BCMDBG) || defined(BCMDBG_ERR)
			char eabuf[ETHER_ADDR_STR_LEN];
#endif // endif
			uint i;
			ap_scb_cubby_t *ap_scb;

			scb = ftcbparm->auth.scb;
			ASSERT(scb != NULL);

			ap_scb = AP_SCB_CUBBY(appvt, scb);
			ASSERT(ap_scb != NULL);

			if (cfg->WPA_auth != WPA_AUTH_DISABLED) {
				WL_ERROR(("wl%d: %s: unhandled algo Shared Key from %s\n",
				          wlc->pub->unit, __FUNCTION__,
				          bcm_ether_ntoa(&scb->ea, eabuf)));
				status = DOT11_SC_AUTH_MISMATCH;
				goto exit;
			}

			if (ap_scb->challenge != NULL) {
				MFREE(wlc->osh, ap_scb->challenge, 2 + ap_scb->challenge[1]);
				ap_scb->challenge = NULL;
			}

			/* create the challenge text */
			if ((chlng = MALLOC(wlc->osh, 2 + DOT11_CHALLENGE_LEN)) == NULL) {
				WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
				          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
				status = DOT11_SC_FAILURE;
				goto exit;
			}
			chlng[0] = DOT11_MNG_CHALLENGE_ID;
			chlng[1] = DOT11_CHALLENGE_LEN;
			for (i = 0; i < DOT11_CHALLENGE_LEN; i++) {
				uint16 l_rand = R_REG(wlc->osh, D11_TSF_RANDOM(wlc));
				chlng[i+2] = (uint8)l_rand;
			}

			/* write to frame */
			bcopy(chlng, data->buf, 2 + DOT11_CHALLENGE_LEN);
#ifdef BCMDBG
			if (WL_ASSOC_ON()) {
				prhex("Auth challenge text #2", chlng, 2 + DOT11_CHALLENGE_LEN);
			}
#endif // endif
			ap_scb->challenge = chlng;

		exit:
			;
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
		uint16 status = DOT11_SC_SUCCESS;

		if (ftpparm->auth.seq == 3) {
			wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *)wlc->ap;
			uint8 *chlng = data->ie;
#if defined(BCMDBG) || defined(BCMDBG_ERR) || defined(WLMSG_ASSOC)
			char eabuf[ETHER_ADDR_STR_LEN];
#endif // endif
			struct scb *scb;
			ap_scb_cubby_t *ap_scb;

			scb = ftpparm->auth.scb;
			ASSERT(scb != NULL);

			ap_scb = AP_SCB_CUBBY(appvt, scb);
			ASSERT(ap_scb != NULL);

			/* Check length */
			if (data->ie_len != TLV_HDR_LEN + DOT11_CHALLENGE_LEN) {
				WL_ASSOC(("wl%d: wrong length WEP Auth Challenge from %s\n",
				          wlc->pub->unit, bcm_ether_ntoa(&scb->ea, eabuf)));
				status = DOT11_SC_AUTH_CHALLENGE_FAIL;
				goto cleanup;
			}

			/* No Challenge Text */
			if (chlng == NULL) {
				WL_ASSOC(("wl%d: no WEP Auth Challenge from %s\n",
				          wlc->pub->unit, bcm_ether_ntoa(&scb->ea, eabuf)));
				status = DOT11_SC_AUTH_CHALLENGE_FAIL;
				goto cleanup;
			}

			/* Failed Challenge Text comparison */
			if (bcmp(&ap_scb->challenge[2], &chlng[2], chlng[1]) != 0) {
				WL_ERROR(("wl%d: failed verify WEP Auth Challenge from %s\n",
				          wlc->pub->unit, bcm_ether_ntoa(&scb->ea, eabuf)));
				wlc_scb_clearstatebit(wlc, scb, AUTHENTICATED);
				status = DOT11_SC_AUTH_CHALLENGE_FAIL;
				goto cleanup;
			}

			WL_ASSOC(("wl%d: WEP Auth Challenge success from %s\n",
			          wlc->pub->unit, bcm_ether_ntoa(&scb->ea, eabuf)));
			wlc_scb_setstatebit(wlc, scb, AUTHENTICATED);
			scb->auth_alg = DOT11_SHARED_KEY;

		cleanup:
			/* free the challenge text */
			MFREE(wlc->osh, ap_scb->challenge, 2 + ap_scb->challenge[1]);
			ap_scb->challenge = NULL;
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

int
wlc_scb_save_wpa_ie(wlc_info_t *wlc, struct scb *scb, bcm_tlv_t *ie)
{
	uint ie_len = 0;
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *)wlc->ap;
	ap_scb_cubby_t *ap_scb = AP_SCB_CUBBY(appvt, scb);

	if (ap_scb == NULL) {
		return BCME_NOTAP;
	}

	if (ie != NULL) {
		ie_len = TLV_HDR_LEN + ie->len;

		/* Optimization */
		if (ap_scb->wpaie != NULL && ie != NULL &&
		    ap_scb->wpaie_len == ie_len)
			goto cp;
	}

	/* Free old WPA IE if one exists */
	if (ap_scb->wpaie != NULL) {
		MFREE(wlc->osh, ap_scb->wpaie, ap_scb->wpaie_len);
		ap_scb->wpaie_len = 0;
		ap_scb->wpaie = NULL;
	}

	if (ie != NULL) {
		/* Store the WPA IE for later retrieval */
		if ((ap_scb->wpaie = MALLOCZ(wlc->osh, ie_len)) == NULL) {
			WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__, (int)ie_len,
			          MALLOCED(wlc->osh)));
			return BCME_NOMEM;
		}

	cp:	/* copy */
		bcopy(ie, ap_scb->wpaie, ie_len);
		ap_scb->wpaie_len = (uint16)ie_len;
	}

	return BCME_OK;
}

/* copy wpaie to output buffer. buf_len is buffer size at input; it's wpaie length
 * at output including the TLV header.
 */
void
wlc_ap_find_wpaie(wlc_ap_info_t *ap, struct scb *scb, uint8 **wpaie, uint *wpaie_len)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *)ap;
	ap_scb_cubby_t *ap_scb = AP_SCB_CUBBY(appvt, scb);

	if (ap_scb != NULL) {
		*wpaie = ap_scb->wpaie;
		*wpaie_len = ap_scb->wpaie_len;
	}
	else {
		*wpaie = NULL;
		*wpaie_len = 0;
	}
}

uint
wlc_ap_get_activity_time(wlc_ap_info_t *ap)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *)ap;
	return appvt->scb_activity_time;
}

uint
wlc_ap_get_pre_tbtt(wlc_ap_info_t *ap)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *)ap;
	return appvt->pre_tbtt_us;
}

void
wlc_ap_set_chanspec(wlc_ap_info_t *ap, chanspec_t chspec)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *)ap;
	appvt->chanspec_selected = chspec;
}

struct scb *
wlc_ap_get_psta_prim(wlc_ap_info_t *ap, struct scb *scb)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *)ap;
	ap_scb_cubby_t *ap_scb = AP_SCB_CUBBY(appvt, scb);
	return ap_scb != NULL ? ap_scb->psta_prim : NULL;
}

static int
wlc_assoc_parse_psta_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;
	wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;
	wlc_bsscfg_t *cfg = data->cfg;
	struct scb *scb = ftpparm->assocreq.scb;
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *)wlc->ap;
	ap_scb_cubby_t *ap_scb = AP_SCB_CUBBY(appvt, scb);
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif // endif

	if (!BSSCFG_AP(cfg)) {
		return BCME_OK;
	}

	if (data->ie == NULL) {
		WL_ASSOC(("wl%d: %s attempted association with no primary PSTA information\n",
		          wlc->pub->unit, bcm_ether_ntoa(&scb->ea, eabuf)));
		return BCME_OK;
	}

	ASSERT(ap_scb != NULL);

	if (data->ie[TLV_LEN_OFF] == MEMBER_OF_BRCM_PROP_IE_LEN) {
		member_of_brcm_prop_ie_t *member_of_brcm_prop_ie =
		        (member_of_brcm_prop_ie_t *)data->ie;

		ap_scb->psta_prim = wlc_scbfindband(wlc, cfg, &member_of_brcm_prop_ie->ea,
			CHSPEC_WLCBANDUNIT(cfg->current_bss->chanspec));
	}

	/* WAR to fix non-primary associating before primary interface assoc */
	if (ap_scb->psta_prim == NULL) {
		WL_ASSOC(("wl%d: %s attempted association with no primary PSTA\n",
		          wlc->pub->unit, bcm_ether_ntoa(&scb->ea, eabuf)));
		ftpparm->assocreq.status = DOT11_SC_ASSOC_FAIL;
		return BCME_ERROR;
	}

	if (ap_scb->psta_prim == scb) {
		/* This is the primary PSTA so make the pointer NULL to indicate it */
		ap_scb->psta_prim = NULL;
	}

	ftpparm->assocreq.status = DOT11_SC_SUCCESS;
	return BCME_OK;
}

void
wlc_ap_channel_switch(wlc_ap_info_t *ap, wlc_bsscfg_t *cfg)
{
	ASSERT((ap && cfg));

	if (BSSCFG_AP(cfg)) {
		wlc_ap_timeslot_register(cfg);
	}
}

int
wlc_ap_timeslot_register(wlc_bsscfg_t *bsscfg)
{
	wlc_info_t *wlc;
	wlc_ap_info_pvt_t *appvt;
	ap_bsscfg_cubby_t *ap_cfg;
	wlc_msch_req_param_t *req;
	uint32 tbtt_h, tbtt_l;
	uint32 err = 0;
	uint32 min_dur;
	uint32 max_away_dur, passive_time;
	uint64 start_time;
	bool prev_awake;
	DBGONLY(char chanbuf[CHANSPEC_STR_LEN]; )

	ASSERT(bsscfg);
	wlc = bsscfg->wlc;

	if (!BSSCFG_AP(bsscfg)) {
		return BCME_ERROR;
	}

	WL_INFORM(("wl%d.%d: %s: chanspec %s\n", wlc->pub->unit,
		WLC_BSSCFG_IDX(bsscfg), __FUNCTION__,
		wf_chspec_ntoa(bsscfg->target_bss->chanspec, chanbuf)));

	ASSERT(bsscfg->current_bss);

	appvt = (wlc_ap_info_pvt_t *)wlc->ap;
	ap_cfg = AP_BSSCFG_CUBBY(appvt, bsscfg);
	ASSERT(ap_cfg != NULL);

	if (ap_cfg->msch_req_hdl) {
		/* clean up the prev status and make new start ready.
		 */
		ap_cfg->msch_state = AP_SCHED_UNREG;
		wlc_ap_prepare_off_channel(wlc, bsscfg);
		wlc_txqueue_end(wlc, bsscfg, NULL);
		wlc_msch_timeslot_unregister(wlc->msch_info, &ap_cfg->msch_req_hdl);
	}

	req = &ap_cfg->req;
	memset(req, 0, sizeof(wlc_msch_req_param_t));

	req->req_type = MSCH_RT_BOTH_FLEX;
	req->flags = 0;
	req->duration = bsscfg->current_bss->beacon_period;
	req->duration <<= MSCH_TIME_UNIT_1024US;
	req->interval = req->duration;
	req->priority = MSCH_RP_CONNECTION;

	wlc_force_ht(wlc, TRUE, &prev_awake);
	wlc_read_tsf(wlc, &tbtt_l, &tbtt_h);
	wlc_force_ht(wlc, prev_awake, NULL);
#ifdef WLMCNX
	if (MCNX_ENAB(wlc->pub))
		wlc_mcnx_l2r_tsf64(wlc->mcnx, bsscfg, tbtt_h, tbtt_l, &tbtt_h, &tbtt_l);
#endif // endif
	wlc_tsf64_to_next_tbtt64(bsscfg->current_bss->beacon_period, &tbtt_h, &tbtt_l);
#ifdef WLMCNX
	if (MCNX_ENAB(wlc->pub))
		wlc_mcnx_r2l_tsf64(wlc->mcnx, bsscfg, tbtt_h, tbtt_l, &tbtt_h, &tbtt_l);
#endif // endif
	start_time = msch_tsf_to_mschtime(wlc->msch_info, tbtt_l, tbtt_h, &req->start_time_l,
		&req->start_time_h);

	err = wlc_iovar_op(wlc, "scan_home_time", NULL, 0,
			&min_dur, sizeof(min_dur), IOV_GET, NULL);
	if (err != BCME_OK) {
		min_dur = DEFAULT_HOME_TIME;
	}
	min_dur = MS_TO_USEC(min_dur);

	err = wlc_iovar_op(wlc, "scan_home_away_time", NULL, 0,
			&max_away_dur, sizeof(max_away_dur), IOV_GET, NULL);
	if (err != BCME_OK) {
		max_away_dur = DEFAULT_HOME_AWAY_TIME;
	}
	max_away_dur = MS_TO_USEC(max_away_dur);

	err = wlc_iovar_op(wlc, "scan_passive_time", NULL, 0,
			&passive_time, sizeof(passive_time), IOV_GET, NULL);
	if (err != BCME_OK) {
		passive_time = DEFAULT_SCAN_PASSIVE_TIME;
	}
	passive_time = MS_TO_USEC(passive_time) + MSCH_PREPARE_DUR;

	req->flex.bf.min_dur = min_dur;
	req->flex.bf.max_away_dur = MAX(max_away_dur, passive_time) +
		MSCH_EXTRA_DELAY_FOR_MAX_AWAY_DUR;
	req->flex.bf.hi_prio_time_l = (uint32)(start_time & 0xFFFFFFFFU);
	req->flex.bf.hi_prio_time_h = (uint32)(start_time >> 32);
	req->flex.bf.hi_prio_interval = req->duration;

	err = wlc_msch_timeslot_register(wlc->msch_info,
		&bsscfg->target_bss->chanspec, 1,
		wlc_ap_msch_clbk, bsscfg, req, &ap_cfg->msch_req_hdl);

	if (err	== BCME_OK) {
		wlc_msch_set_chansw_reason(wlc->msch_info, ap_cfg->msch_req_hdl, CHANSW_SOFTAP);
		WL_INFORM(("wl%d.%d: %s: request success\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), __FUNCTION__));
	} else {
		WL_ERROR(("wl%d.%d: %s: request failed error %d\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), __FUNCTION__, err));
		ASSERT(0);
	}

	return err;
}

void
wlc_ap_timeslot_unregister(wlc_bsscfg_t *bsscfg)
{
	wlc_info_t *wlc;
	wlc_ap_info_pvt_t *appvt;
	ap_bsscfg_cubby_t *ap_cfg;
	DBGONLY(char chanbuf[CHANSPEC_STR_LEN]; )

	ASSERT(bsscfg);
	wlc = bsscfg->wlc;

	if (!BSSCFG_AP(bsscfg)) {
		return;
	}

	WL_INFORM(("wl%d.%d: %s: chanspec %s\n", wlc->pub->unit,
		WLC_BSSCFG_IDX(bsscfg), __FUNCTION__,
		wf_chspec_ntoa(bsscfg->current_bss->chanspec, chanbuf)));

	appvt = (wlc_ap_info_pvt_t *)wlc->ap;
	ap_cfg = AP_BSSCFG_CUBBY(appvt, bsscfg);
	ASSERT(ap_cfg != NULL);

	if (ap_cfg->msch_req_hdl) {
		wlc_msch_timeslot_unregister(wlc->msch_info, &ap_cfg->msch_req_hdl);
		ap_cfg->msch_req_hdl = NULL;
#ifdef WL_DTS
		/* disable DTS tx suppression */
		if (DTS_ENAB(wlc->pub)) {
			wlc_set_cfg_tx_stop_time(wlc, bsscfg, -1);
		}
#endif // endif
	}

}

void
wlc_ap_timeslot_update(wlc_bsscfg_t *bsscfg, uint32 start_tsf, uint32 interval)
{
	wlc_info_t *wlc;
	wlc_ap_info_pvt_t *appvt;
	ap_bsscfg_cubby_t *ap_cfg;
	wlc_msch_req_param_t *req;
	uint32 update_mask;
	uint32 tsf_l;
	int to;
	uint64 start_time;
	bool prev_awake;
	DBGONLY(char chanbuf[CHANSPEC_STR_LEN]; )

	ASSERT(bsscfg);
	wlc = bsscfg->wlc;

	if (!BSSCFG_AP(bsscfg)) {
		ASSERT(FALSE);
		return;
	}

	appvt = (wlc_ap_info_pvt_t *)wlc->ap;
	ap_cfg = AP_BSSCFG_CUBBY(appvt, bsscfg);

	ASSERT(ap_cfg != NULL);
	ASSERT(ap_cfg->msch_req_hdl);

	req = &ap_cfg->req;

	update_mask = MSCH_UPDATE_START_TIME;
	if (interval != req->interval) {
		update_mask |= MSCH_UPDATE_INTERVAL;
	}

	wlc_force_ht(wlc, TRUE, &prev_awake);
	wlc_read_tsf(wlc, &tsf_l, NULL);
	wlc_force_ht(wlc, prev_awake, NULL);

	to = (int)(start_tsf + MSCH_ONCHAN_PREPARE - tsf_l);
	if (to < MSCH_PREPARE_DUR) {
		to = MSCH_PREPARE_DUR;
	}

	start_time = msch_future_time(wlc->msch_info, (uint32)to);

	WL_INFORM(("wl%d.%d: %s: chanspec %s, start_tsf %d, tsf %d, to %d,"
		"interval %d\n", wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), __FUNCTION__,
		wf_chspec_ntoa(wlc_get_chanspec(wlc, bsscfg), chanbuf),
		start_tsf, tsf_l, to, interval));

	req->start_time_l = (uint32)(start_time & 0xFFFFFFFFU);
	req->start_time_h = (uint32)(start_time >> 32);
	req->interval = interval;

	wlc_msch_timeslot_update(wlc->msch_info, ap_cfg->msch_req_hdl, req, update_mask);
}

/** Called back by the multichannel scheduler (msch) */
static int
wlc_ap_msch_clbk(void* handler_ctxt, wlc_msch_cb_info_t *cb_info)
{
	wlc_bsscfg_t *cfg = (wlc_bsscfg_t *)handler_ctxt;
	uint32 type = cb_info->type;
	wlc_info_t *wlc;
	wlc_ap_info_pvt_t *appvt;
	ap_bsscfg_cubby_t *ap_cfg;
	chanspec_t cb_chanspec = cb_info->chanspec;
	DBGONLY(char chanbuf[CHANSPEC_STR_LEN]; )

	/* validation */
	if (cfg) {
		wlc = cfg->wlc;
	} else {
		/* The request has been cancelled, ignore the Clbk */
		return BCME_OK;
	}
	if (!BSSCFG_AP(cfg) || !cfg->enable) {
		return BCME_ERROR;
	}

	appvt = (wlc_ap_info_pvt_t *)wlc->ap;
	ap_cfg = AP_BSSCFG_CUBBY(appvt, cfg);
	ASSERT(ap_cfg != NULL);

	WL_INFORM(("wl%d.%d: %s: chanspec %s, type 0x%04x\n",
		wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
		wf_chspec_ntoa(cb_chanspec, chanbuf), type));

#ifdef WL_MODESW
	/* For 4366, Enable Operating Mode when 160Mhz capable */
	if (WL_BW_CAP_160MHZ(wlc->band->bw_cap) && WLC_PHY_160_HALF_NSS(wlc)) {
		cfg->oper_mode_enabled = TRUE;
	}
	WL_MODE_SWITCH(("wl%d: %s oper_mode 0x%x op_mode_enabled %d chanspec 0x%x"
			" type 0x%04x target_bsschspec 0x%x phychspec 0x%x\n",
			wlc->pub->unit, __FUNCTION__, cfg->oper_mode, cfg->oper_mode_enabled,
			cb_chanspec, type, cfg->target_bss->chanspec,
			WLC_BAND_PI_RADIO_CHANSPEC));
#endif /* WL_MODESW */

#ifdef WLMCHAN
	if (MCHAN_ENAB(wlc->pub) && MCHAN_ACTIVE(wlc->pub)) {
		wlc_mchan_msch_clbk(handler_ctxt, cb_info);
	}
#endif /* WLMCHN */

	/* event handling fuction start */

	/* Just before switching to the new requested channel,
	 * unless in edcrs_eu, mark the current chanspec as passive.
	 * MSCH_CT_PRE_ONCHAN to be used in callback before switching
	 * to the requested channel. Phy chanspec is still the older chanspec.
	 */
	if (type & MSCH_CT_PRE_ONCHAN) {
		if (WL11H_ENAB(wlc) &&
				!wlc->is_edcrs_eu &&
				wlc_radar_chanspec(wlc->cmi, cb_chanspec)) {
			wlc_set_quiet_chanspec_exclude(wlc->cmi, cb_chanspec,
					cfg->target_bss->chanspec);
		}
	}

	if (type & MSCH_CT_REQ_START) {
		ap_cfg->msch_state = AP_SCHED_PRE_INIT;
#ifdef PHYCAL_CACHING
		phy_chanmgr_create_ctx((phy_info_t *) WLC_PI(wlc),
			cb_chanspec);
#endif // endif
		if (!wlc_radar_chanspec(wlc->cmi, cb_chanspec) &&
				PHYMODE(wlc) != PHYMODE_BGDFS &&
#ifdef WL_AIR_IQ
				!wlc_airiq_scan_in_progress(wlc) &&
#endif /* WL_AIR_IQ */
				!wlc_dfs_scan_in_progress(wlc->dfs)) {
			wlc_full_phy_cal(wlc, cfg, PHY_PERICAL_UP_BSS);
		}
	}

	if (type & MSCH_CT_ON_CHAN) {
		/* first thing to do */
		wlc_txqueue_start(wlc, cfg, cb_chanspec, NULL);

		/* post initialization for the first call */
		if (ap_cfg->msch_state == AP_SCHED_PRE_INIT || !SCAN_IN_PROGRESS(wlc->scan)) {
			wlc_ap_upd_onchan(wlc->ap, cfg);
		}
		ap_cfg->msch_state = AP_SCHED_ON_CHAN;
#ifdef WL_DTS
		if (DTS_ENAB(wlc->pub)) {
			wlc_set_cfg_tx_stop_time(wlc, cfg,
				msch_calc_slot_duration(wlc->msch_info, cb_info));
		}
#endif /* WL_DTS */
		wlc_scb_ratesel_init_all(wlc);
		wlc_ap_return_home_channel(cfg);

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
		if (WL11H_AP_ENAB(wlc) && WLC_APSTA_ON_RADAR_CHANNEL(wlc) &&
			cfg->disable_ap_up &&
			wlc_radar_chanspec(wlc->cmi, cb_chanspec)) {
			WL_REGULATORY(("wl%d.%d: %s scan completed, back"
				"to home channel dfs ON cb_chanspec 0x%x\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__, cb_chanspec));
			wlc_set_dfs_cacstate(wlc->dfs, ON, cfg);
		}
#ifdef  WL_MODESW
		/* callback to modesw */
		WL_MODE_SWITCH(("wl%d.%d: %s: callback to modesw\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
		if (!SCAN_IN_PROGRESS(wlc->scan)) {
			wlc_modesw_ap_bwchange_notif(cfg);
		}
#endif /* WL_MODESW */
	}

	if (type & MSCH_CT_OFF_CHAN) {
		wlc_ap_prepare_off_channel(wlc, cfg);
	}

	if (type & MSCH_CT_OFF_CHAN_DONE) {
		wlc_txqueue_end(wlc, cfg, NULL);
		wlc_ap_off_channel_done(wlc, cfg);
		ap_cfg->msch_state = AP_SCHED_OFF_CHAN;
	}

	if (type & MSCH_CT_REQ_END) {
		/* The msch hdl is no more valid */
		WL_INFORM(("wl%d.%d: %s: The msch hdl is no more valid\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
		ap_cfg->msch_req_hdl = NULL;
	}

	/* NOTE: assuming that there is no multiple ch/slot control.
	 * SoftAP mode, for example, only can be activated stand-alone
	 * SLOT_SKIP should not happen in this reason.
	 */
	if (type & MSCH_CT_SLOT_START) {
	}
	if (type & MSCH_CT_SLOT_END) {
		/* TBD : pre-tbtt handling */
	}
	if (type & MSCH_CT_SLOT_SKIP) {
		/* Do Nothing for STA */
	}

	return BCME_OK;
}

/* prepare to leave home channel */
static void
wlc_ap_prepare_off_channel(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	WL_INFORM(("wl%d.%d: %s\n", wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));

	/* Must disable AP beacons and probe responses first before going away from home channel */
	wlc_ap_mute(wlc, TRUE, cfg, -1);

#ifdef PROP_TXSTATUS
	if (PROP_TXSTATUS_ENAB(wlc->pub)) {
		wlc_wlfc_mchan_interface_state_update(wlc, cfg,
			WLFC_CTL_TYPE_INTERFACE_CLOSE, FALSE);
	}
#endif /* PROP_TXSTATUS */
}

static void
wlc_ap_return_home_channel(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
#ifdef WLMCHAN
	int btc_flags = wlc_bmac_btc_flags_get(wlc->hw);
	uint16 protections = 0;
	uint16 active = 0;
#endif /* WLMCHAN */
	WL_INFORM(("wl%d.%d: %s\n", wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));

#ifdef PROP_TXSTATUS
	if (PROP_TXSTATUS_ENAB(wlc->pub)) {
#if defined(WLMCHAN) && defined(WLRSDB) && defined(WL_MODESW)
		bool if_state_upd_open = TRUE;
		/* in mode switch case open interface only after
		 * mode switch is complete.
		 */
		if (MCHAN_ACTIVE(wlc->pub) && WLC_MODESW_ENAB(wlc->pub) && RSDB_ENAB(wlc->pub)) {
			if (wlc_modesw_mchan_hw_switch_pending(wlc, cfg, TRUE)) {
				if_state_upd_open = FALSE;
			}
		}
		if (if_state_upd_open)
#endif /* WLMCHAN && WLRSDB && WL_MODESW */
		/* Open the active interface so that host
		 * start sending pkts to dongle
		 */
		wlc_wlfc_mchan_interface_state_update(wlc,
			cfg, WLFC_CTL_TYPE_INTERFACE_OPEN,
			!MCHAN_ACTIVE(wlc->pub));
	}
#endif /* PROP_TXSTATUS */

	wlc_suspend_mac_and_wait(wlc);

	/* validate the phytxctl for the beacon before turning it on */
	wlc_validate_bcn_phytxctl(wlc, NULL);

	/* Unmute - AP when in SoftAP/GO channel */
	wlc_ap_mute(wlc, FALSE, cfg, -1);

#ifdef PROP_TXSTATUS
	if (PROP_TXSTATUS_ENAB(wlc->pub)) {
#ifdef WLAMPDU
		/* send_bar on interface being opened */
		if (AMPDU_ENAB(wlc->pub)) {
			struct scb_iter scbiter;
			struct scb *scb = NULL;

			FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {
				wlc_ampdu_send_bar_cfg(wlc->ampdu_tx, scb);
			}
		}
#endif /* WLAMPDU */
		wlfc_sendup_ctl_info_now(wlc->wlfc);
	}
#endif /* PROP_TXSTATUS */

#ifdef WLMCHAN
	/*
	 * Setup BTCX protection mode according to the BSS that is
	 * being switched in.
	 */
	if (btc_flags & WL_BTC_FLAG_ACTIVE_PROT)
		active = MHF3_BTCX_ACTIVE_PROT;

	if (cfg->up) {
		protections = active;
	}
#endif /* WLMCHAN */

#ifdef WLMCNX
	if (MCNX_ENAB(wlc->pub)) {
		int bss_idx;
		/* Set the current BSS's Index in SHM (bits 10:8 of M_P2P_GO_IND_BMP
		   contain the current BSS Index
		 */
		bss_idx = wlc_mcnx_BSS_idx(wlc->mcnx, cfg);
		wlc_mcnx_shm_bss_idx_set(wlc->mcnx, bss_idx);
	}
#endif /* WLMCNX */
#ifdef WLMCHAN
	if (MCHAN_ACTIVE(wlc->pub))
		wlc_mhf(wlc, MHF3, MHF3_BTCX_ACTIVE_PROT | MHF3_BTCX_PS_PROTECT,
			protections, WLC_BAND_2G);
	else
#endif /* WLMCHAN */
		wlc_btc_set_ps_protection(wlc, cfg); /* enable */

	wlc_enable_mac(wlc);

	if (wlc->num_160mhz_assocs > 0) {
		phy_chanmgr_tdcs_enable_160m((phy_info_t*) wlc->pi, FALSE);
	} else {
		phy_chanmgr_tdcs_enable_160m((phy_info_t*) wlc->pi, TRUE);
	}

} /* wlc_ap_return_home_channel */

/* prepare to leave home channel */
static void
wlc_ap_off_channel_done(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	WL_INFORM(("wl%d.%d: %s\n", wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));

#ifdef PROP_TXSTATUS
	if (PROP_TXSTATUS_ENAB(wlc->pub)) {
		wlc_wlfc_mchan_interface_state_update(wlc, cfg,
			WLFC_CTL_TYPE_INTERFACE_CLOSE, FALSE);
		wlc_wlfc_flush_pkts_to_host(wlc, cfg);
		wlfc_sendup_ctl_info_now(wlc->wlfc);
	}
#endif /* PROP_TXSTATUS */

	/* If we are switching away from radar home_chanspec
	 * because STA scans (normal/Join/Roam) with
	 * atleast one local 11H AP in radar channel,
	 * turn of radar_detect.
	 * NOTE: Implied that upstream AP assures this radar
	 * channel is clear.
	 */
	if (WL11H_AP_ENAB(wlc) && WLC_APSTA_ON_RADAR_CHANNEL(wlc) &&
		wlc_radar_chanspec(wlc->cmi, wlc->home_chanspec)) {
		WL_REGULATORY(("wl%d.%d: %s Moving from home channel dfs OFF\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
		wlc_set_dfs_cacstate(wlc->dfs, OFF, cfg);
	}
#ifdef WL_DTS
	if (DTS_ENAB(wlc->pub)) {
		wlc_set_cfg_tx_stop_time(wlc, cfg, -1);
	}
#endif /* WL_DTS */
	if (WLC_BW_SWITCH_TDCS_EN(wlc)) {
		phy_chanmgr_tdcs_enable_160m((phy_info_t*) wlc->pi, FALSE);
	}
}

uint32
wlc_ap_getdtim_count(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	uint32 dtim_cnt;
#ifdef WLMCNX
	if (P2P_ENAB(wlc->pub)) {
		int idx;
		idx = wlc_mcnx_BSS_idx(wlc->mcnx, cfg);
		dtim_cnt = wlc_mcnx_read_shm(wlc->mcnx, M_P2P_BSS_DTIM_CNT(wlc, idx));
	}
	else
#endif // endif
	{
		wlc_bmac_copyfrom_objmem(wlc->hw, (S_DOT11_DTIMCOUNT << 2),
			&dtim_cnt, sizeof(dtim_cnt), OBJADDR_SCR_SEL);
	}
	return dtim_cnt;
}

/* This function returns the no of
* infra AP's running in the wlc.
*/
uint8
wlc_ap_count(wlc_ap_info_t *ap, bool include_p2p)
{
	wlc_ap_info_pvt_t* appvt = (wlc_ap_info_pvt_t*) ap;
	wlc_info_t *wlc = appvt->wlc;
	uint8 idx, infra_ap_count = 0, p2p_go_count = 0;
	wlc_bsscfg_t *cfg;
	FOREACH_UP_AP(wlc, idx, cfg) {
		if (BSS_P2P_ENAB(wlc, cfg))
			p2p_go_count++;
		else
			infra_ap_count++;
	}
	return (include_p2p ? (p2p_go_count + infra_ap_count) : infra_ap_count);
}

static int32
wlc_ap_verify_basic_mcs(wlc_ap_info_t *ap, const uint8 *mcs, uint32 len)
{
	uint32 i;
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *)ap;
	const uint8 *basic_mcs = appvt->basic_mcs;

	ASSERT(len >= MCSSET_LEN);
	if (len > MCSSET_LEN) {
		len = MCSSET_LEN;
	}

	for (i = 0; i < len; i++) {
		if ((basic_mcs[i] & mcs[i]) != basic_mcs[i]) {
			WL_ERROR(("%s: rate verification fail; idx=%d\n",
				__FUNCTION__, i));
			return BCME_ERROR;
		}
	}
	return BCME_OK;
}

bool
wlc_ap_on_chan(wlc_ap_info_t *ap, wlc_bsscfg_t *bsscfg)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *)ap;
	ap_bsscfg_cubby_t *ap_cfg = AP_BSSCFG_CUBBY(appvt, bsscfg);

	ASSERT(ap_cfg);

	return (ap_cfg->msch_state == AP_SCHED_ON_CHAN);
}

/*
 * Function:	wlc_ap_iface_create
 *
 * Purpose:	Function to create ap interface through interface create command.
 *
 * Parameters:
 * module_ctx	: Module context
 * if_buf	: interface create buffer
 * wl_info	: out parameter of created interface
 * err		: pointer to store the error status
 *
 * Returns:	cfg pointer - If success
 *		NULL on failure
 */
static wlc_bsscfg_t*
wlc_ap_iface_create(void *if_module_ctx, wl_interface_create_t *if_buf,
	wl_interface_info_t *wl_info, int32 *err)
{
	wlc_bsscfg_t *cfg;
	wlc_ap_info_pvt_t *appvt = if_module_ctx;
	wlc_bsscfg_type_t type = {BSSCFG_TYPE_GENERIC, BSSCFG_GENERIC_AP};

	ASSERT(appvt->wlc);

	cfg = wlc_iface_create_generic_bsscfg(appvt->wlc, if_buf, &type, err);
	if (cfg != NULL) {
		wlc_set_iface_info(cfg, wl_info, if_buf);
	}
	return (cfg);
}

/*
 * Function:	wlc_ap_iface_remove
 *
 * Purpose:	Function to remove ap interface(s) through interface_remove IOVAR.
 *
 * Input Parameters:
 *	wlc	: Pointer to wlc
 *	bsscfg	: Pointer to bsscfg corresponding to the interface
 *
 * Returns:	BCME_OK - If success
 *		Respective error code on failures.
 */
static int32
wlc_ap_iface_remove(wlc_info_t *wlc, wlc_if_t *wlcif)
{
	int32 ret;
	wlc_bsscfg_t *bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);

	if (bsscfg == NULL) {
		ret = BCME_NOTFOUND;
		goto done;
	}
	if (bsscfg->subtype != BSSCFG_GENERIC_AP) {
		ret = BCME_NOTAP;
	} else {
		if (bsscfg->enable) {
			wlc_bsscfg_disable(wlc, bsscfg);
		}
		wlc_bsscfg_free(wlc, bsscfg);
		ret = BCME_OK;
	}
done:
	return (ret);
}

void
wlc_ap_update_bw(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	chanspec_t old_chanspec, chanspec_t chanspec)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *)wlc->ap;
	ap_bsscfg_cubby_t *ap_cfg = AP_BSSCFG_CUBBY(appvt, bsscfg);
	ASSERT(ap_cfg != NULL);
	if (ap_cfg->msch_req_hdl == NULL) {
		return;
	}
	msch_update_bw(wlc->msch_info, ap_cfg->msch_req_hdl, old_chanspec, chanspec);
}

static void wlc_ap_auth_tx_complete(wlc_info_t *wlc, uint txstatus, void *arg)
{
	struct scb *scb = (struct scb *)arg;
	if (SCB_AUTHENTICATED(scb) && (txstatus & TX_STATUS_ACK_RCV)) {
		/* Check if peer was blacklisted here.
		* If Peer STA is blacklisted clear states and mark the scb for deletion.
		*/
	}
}

static int wlc_ap_pre_sendauth_cb(wlc_info_t *wlc, wlc_bsscfg_t *cfg, void *scb, void *pkt)
{
	int err = BCME_OK;
	BCM_REFERENCE(cfg);
	err = wlc_pcb_fn_register(wlc->pcb, wlc_ap_auth_tx_complete, scb, pkt);
	return err;
}

static void
wlc_ap_authresp_deauth(wlc_info_t *wlc, struct scb *scb)
{
	/* Notify HOST with a Deauth, this will ensure
	 * clearing any flow-rings (PCIe case) created during previous
	 * association of the SCB
	 */
	if (SCB_AUTHENTICATED(scb)) {
		/* If SCB is in authenticated state then clear the
		 * ASSOCIATED, AUTHENTICATED and
		 * AUTHORIZED bits in the flag before freeing the SCB
		 */
		wlc_scb_clearstatebit(wlc, scb,
			ASSOCIATED | AUTHENTICATED | AUTHORIZED);
		wlc_deauth_complete(wlc, SCB_BSSCFG(scb),
			WLC_E_STATUS_SUCCESS, &scb->ea,
			DOT11_RC_UNSPECIFIED, 0);
	}
}

#ifdef RADIONOA_PWRSAVE
void
wlc_radio_pwrsave_update_params(wlc_ap_info_t *ap, bool enable,
	uint32 pps, uint32 quiet, bool assoc_check)
{
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *)ap;
	wlc_info_t *wlc = appvt->wlc;

	/* PPS */
	appvt->radio_pwrsave.pwrsave.pps_threshold = pps;

	/* Quiet time */
	appvt->radio_pwrsave.pwrsave.quiet_time = quiet;

	/* Assoc check */
	appvt->radio_pwrsave.pwrsave.stas_assoc_check = assoc_check;
	if (assoc_check && wlc_radio_pwrsave_in_power_save(wlc->ap) &&
		wlc_ap_stas_associated(wlc->ap)) {
		wlc_radio_pwrsave_exit_mode(wlc->ap);
		WL_INFORM(("Going out of power save as there are associated STASs!\n"));
	}

	/* Enable */
	ap->radio_pwrsave_enable = appvt->radio_pwrsave.pwrsave.power_save_check = enable;
	wlc_reset_radio_pwrsave_mode(ap);
}
#endif /* RADIONOA_PWRSAVE */

#ifdef WL_GLOBAL_RCLASS
bool
wlc_ap_scb_support_global_rclass(wlc_info_t *wlc, struct scb *scb)
{
	wlc_ap_info_t *ap = wlc->ap;
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *)ap;
	ap_scb_cubby_t *ap_scb = NULL;

	if (!appvt) {
		return FALSE;
	}

	ap_scb = AP_SCB_CUBBY(appvt, scb);
	if (!ap_scb) {
		return FALSE;
	}
	return SCB_SUPPORTS_GLOBAL_RCLASS(ap_scb);
}

static int
wlc_assoc_parse_regclass_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_info_t* wlc = (wlc_info_t*)ctx;
	wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;
	struct scb *scb =  ftpparm->assocreq.scb;
	wlc_ap_info_t *ap = wlc->ap;
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *)ap;
	ap_scb_cubby_t *ap_scb = NULL;
	uint8 *cp = NULL;

	cp = data->ie;
	ap_scb = AP_SCB_CUBBY(appvt, scb);

	switch (data->ft) {
		case FC_ASSOC_REQ:
		case FC_REASSOC_REQ:
			{
				bcm_tlv_t* ie;
				uint8 i;
				uint8 *rclass_bitmap = NULL;
				uint8 ret = 0;

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
				rclass_bitmap = MALLOCZ(wlc->osh, 32);

				if (!rclass_bitmap) {
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
					setbit(rclass_bitmap, *cp);
					cp++;
				}
				/* Check global support from list of operating class values,
				 * these list of supported operating class holds operating band
				 * capability. Update scb band capability.
				 */
				ret = wlc_sta_supports_global_rclass(rclass_bitmap);
				if (ret) {
					ap_scb->flags |= SCB_SUPPORT_GLOBAL_RCLASS;
#if defined(WL_MBO) && defined(MBO_AP)
					if (BSS_MBO_ENAB(wlc, (scb->bsscfg))) {
						wlc_mbo_update_scb_band_cap(wlc, scb, &ret);
					}
#endif	/* WL_MBO && MBO_AP */
				}
				MFREE(wlc->osh, rclass_bitmap, 32);
			}
		break;
	}

	return BCME_OK;
}
#endif /* WL_GLOBAL_RCLASS */

static void
wlc_ap_scb_state_upd_cb(void *ctx, scb_state_upd_data_t *notif_data)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;
	struct scb *scb;
	uint8 oldstate;
	uint8 assoc_state_change = 0x00;
	uint8 scb_assoced;

	ASSERT(notif_data != NULL);

	scb = notif_data->scb;
	ASSERT(scb != NULL);

	if (!BSSCFG_AP(scb->bsscfg)) {
		return;
	}

	oldstate = notif_data->oldstate;
	scb_assoced = SCB_ASSOCIATED(scb);

	assoc_state_change = ((oldstate & ASSOCIATED) ^ scb_assoced);

	if (!assoc_state_change) {
		WL_INFORM(("wl%d: No change in assoc state of scb["MACF"], return\n",
			wlc->pub->unit, ETHERP_TO_MACF(&scb->ea)));

		return;
	}

	/* Bookkeeping of all 160Mhz capable STA's is done.
	 * 160-80Mhz Bandwidth switch is triggered.
	 */
	wlc_ap_160mhz_upd_bw_check(wlc, scb, scb_assoced);

#ifdef WL_GLOBAL_RCLASS
	wlc_ap_scb_no_gbl_rclass_upd(wlc, scb, scb_assoced, oldstate);
#endif /* WL_GLOBAL_RCLASS */
}

#ifdef WL_GLOBAL_RCLASS
static void
wlc_ap_scb_no_gbl_rclass_upd(wlc_info_t *wlc, struct scb *scb,
	uint8 scb_assoced, uint8 oldstate)
{
	wlc_ap_info_t *ap = wlc->ap;
	wlc_ap_info_pvt_t *appvt = (wlc_ap_info_pvt_t *)ap;
	ap_scb_cubby_t *ap_scb = NULL;

	ap_scb = AP_SCB_CUBBY(appvt, scb);

	if (ap_scb == NULL) {
		WL_INFORM(("wl%d:ap cubby of scb ["MACF"] NULL, Dump:state[%x] oldstate[%x]"
			"bsscfg->scb_without_gbl_rclass[%d]\n",
			wlc->pub->unit, ETHERP_TO_MACF(&scb->ea),
			scb->state, oldstate, scb->bsscfg->scb_without_gbl_rclass));
		return;
	}

	if (SCB_SUPPORTS_GLOBAL_RCLASS(ap_scb)) {
		WL_INFORM(("wl%d: scb["MACF"] support gbl rclass [%d] no need to process,"
			"total sta count not support global rclass[%d] return\n",
			wlc->pub->unit, ETHERP_TO_MACF(&scb->ea),
			SCB_SUPPORTS_GLOBAL_RCLASS(ap_scb),
			scb->bsscfg->scb_without_gbl_rclass));
		return;
	}

	WL_INFORM(("wl%d: scb["MACF"] state[%x] oldstate[%x] support_gbl_rclass[%d],"
		"bsscfg->scb_without_gbl_rclass[%d]\n", wlc->pub->unit,
		ETHERP_TO_MACF(&scb->ea), scb->state,
		oldstate, SCB_SUPPORTS_GLOBAL_RCLASS(ap_scb),
		scb->bsscfg->scb_without_gbl_rclass));

	if (scb_assoced) {
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
				SCB_SUPPORTS_GLOBAL_RCLASS(ap_scb),
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
