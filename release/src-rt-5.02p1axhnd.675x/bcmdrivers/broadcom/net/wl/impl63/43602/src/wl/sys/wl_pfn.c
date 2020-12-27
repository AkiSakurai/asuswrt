/*
 * Preferred network source file
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
 * $Id: wl_pfn.c 666951 2016-10-25 09:44:01Z $
 *
 */

/**
 * @file
 * @brief
 * Preferred network offload is for host to configure dongle with a list of preferred networks.
 * So dongle can perform PNO scan based on these preferred networks and notify host of its
 * findings.
 */

/**
 * @file
 * @brief
 * XXX Twiki: [PreferredNetworkOffload] [PreferredNetworkOffloadEnhance]
 */

/* This define is to help mogrify the code */
/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>

#ifdef WLPFN

#include <typedefs.h>
#include <osl.h>
#include <wl_dbg.h>
#include <event_log.h>
#include <event_trace.h>

#include <bcmutils.h>
#include <siutils.h>
#include <bcmwpa.h>
#include <proto/ethernet.h>
#include <proto/802.11.h>

#include <wlioctl.h>
#include <wlc_pub.h>
#include <wl_dbg.h>
#include <event_log.h>

#include <sbhndpio.h>
#include <sbhnddma.h>
#include <hnddma.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_key.h>
#include <wlc_channel.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scan.h>

#include <wl_export.h>

#include <wlc_mpf.h>
#include <wl_pfn.h>
#if defined(WL_SHIF)
#include <bcm_shif.h>
#include <hndgci.h>
#include <wlc_shub.h>
#endif /* defined(WL_SHIF) */

/* Useful defines and enums */
#define MIN_RSSI		-32768
#define MAX_BSSID_LIST		150
#define MAX_BDCAST_NUM		50

#define DEFAULT_SCAN_FREQ	60	/* in sec */
#define DEFAULT_LOST_DURATION	120	/* in sec */
#define DEFAULT_RSSI_MARGIN	30	/* in db */
#define DEFAULT_BKGROUND_SCAN	FALSE
#define DEFAULT_BD_SCAN		FALSE
#define DEFAULT_SCAN_MARGIN	5000	/* in msec */
#define INVALID_RSSI                -128
#define RSSI_WEIGHT_MOST_RECENT     10

#define WL_PFN_RSSI_ONCHANNEL	0x01

#define EVENT_DATABUF_MAXLEN	(512-sizeof(bcm_event_t))
#define EVENT_MAX_NETCNT \
((EVENT_DATABUF_MAXLEN - sizeof(wl_pfn_scanresults_t)) / sizeof(wl_pfn_net_info_t) + 1)
#define EVENT_DATA_MAXLEN \
(sizeof(wl_pfn_scanresults_t) +  (EVENT_MAX_NETCNT - 1) * sizeof(wl_pfn_net_info_t))

#define EVENT_MAX_LNETCNT \
((EVENT_DATABUF_MAXLEN - sizeof(wl_pfn_lscanresults_t)) / sizeof(wl_pfn_lnet_info_t) + 1)
#define EVENT_LDATA_MAXLEN \
(sizeof(wl_pfn_lscanresults_t) +  (EVENT_MAX_NETCNT - 1) * sizeof(wl_pfn_lnet_info_t))

#define EVENT_MAX_SIGNIFICANT_CHANGE_NETCNT \
(((EVENT_DATABUF_MAXLEN - sizeof(wl_pfn_swc_results_t)) / sizeof(wl_pfn_significant_net_t)) + 1)

#define GSCAN_CFG_SIZE(count) \
(sizeof(wl_pfn_gscan_cfg_t) + ((count - 1) * sizeof(wl_pfn_gscan_channel_bucket_t)))

#define CLEAR_WLPFN_AC_FLAG(flags)	(flags &= ~AUTO_CONNECT_MASK)
#define	SET_WLPFN_AC_FLAG(flags)	(flags |= AUTO_CONNECT_MASK)

#if !defined(WL_SHIF)
typedef struct shif_info shif_info_t;
#else
#define SH_MAX_CMD_LEN	3
#define	SH_SCAN_CMD	"0:\n"
#define	SH_SCAN_RESP	"1:\n"
#define	SH_MOTION_CMD	"2:\n"
#define	SH_IDLE_CMD	"3:\n"
#endif /* WL_SHIF */

#define	NONEED_REPORTLOST		0xfe
#define STOP_REPORTLOST			0xff
enum {
	IOV_PFN_SET_PARAM,
	IOV_PFN_CFG,
	IOV_PFN_ADD,
	IOV_PFN_ADD_BSSID,
	IOV_PFN,
	IOV_PFN_CLEAR,
	IOV_PFN_BEST,
	IOV_PFN_SUSPEND,
	IOV_PFN_LBEST,
	IOV_PFN_MEM,
	IOV_PFN_RTTN,
	IOV_PFN_MPFSET,
	IOV_PFN_BEST_BSSID,
	IOV_PFN_OVERRIDE,
	IOV_PFN_MACADDR,
	IOV_PFN_SHUB_REQ,
#ifdef GSCAN
	IOV_PFN_GSCAN_CFG,
	IOV_PFN_ADD_SWC_BSSID,
#endif /* GSCAN */
	IOV_PFN_LAST
};

typedef enum tagPFNNETSTATE {
	PFN_NET_DISASSOCIATED,
	PFN_NET_ASSOCIATED
} PFNNETSTATE;

typedef enum tagPFNSCANSTATE {
	PFN_SCAN_DISABLED,
	PFN_SCAN_IDLE,
	PFN_SCAN_INPROGRESS
} PFNSCANSTATE;

typedef struct wl_pfn_bestinfo {
	wl_pfn_subnet_info_t pfnsubnet;
	uint16	flags;
	int16	RSSI; /* receive signal strength (in dBm) */
	uint32	timestamp; /* laps in mseconds */
} wl_pfn_bestinfo_t;

typedef struct wl_pfn_bestinfo_bssid {
	struct ether_addr BSSID;
	uint8 channel;
	uint16 flags;
	int16 RSSI;
	uint32 timestamp;
} wl_pfn_bestinfo_bssid_t;

typedef struct wl_pfn_bestnet {
	struct wl_pfn_bestnet *next;
	wl_pfn_bestinfo_t bestnetinfo[1]; /* bestn number of these */
} wl_pfn_bestnet_t;

typedef struct wl_pfn_bestnet_b {
	struct wl_pfn_bestnet *next;
	wl_pfn_bestinfo_bssid_t bestnetinfo_b[1]; /* bestn number of these */
} wl_pfn_bestnet_b_t;

#define WL_PFN_BESTNET_FIXED OFFSETOF(wl_pfn_bestnet_t, bestnetinfo)
#define WL_PFN_BESTNET_B_FIXED OFFSETOF(wl_pfn_bestnet_b_t, bestnetinfo_b)

#define PFN_NET_NOT_FOUND       0x1
#define PFN_NET_JUST_FOUND      0x2
#define PFN_NET_ALREADY_FOUND   0x4
#define PFN_NET_JUST_LOST       0x8

/* Do not change order */
#define PFN_SIGNIFICANT_BSSID_NEVER_SEEN             0x0
#define PFN_SIGNIFICANT_BSSID_LOST                   0x1
#define PFN_SIGNIFICANT_BSSID_SEEN                   0x2
#define PFN_SIGNIFICANT_BSSID_RSSI_REPORTED_LOW      0x4
#define PFN_SIGNIFICANT_BSSID_RSSI_CHANGE_LOW        0x8
#define PFN_SIGNIFICANT_BSSID_RSSI_REPORTED_HIGH     0x10
#define PFN_SIGNIFICANT_BSSID_RSSI_CHANGE_HIGH       0x20
#define PFN_SIGNIFICANT_CHANGE_FLAG_MASK             0x3F
#define PFN_REPORT_CHANGE                            0xC0

#define MIN_NUM_VALID_RSSI     4
#define UNSPECIFIED_CHANNEL    0
#define GSCAN_FULL_SCAN_RESULTS_CUR_SCAN     (1 << 1)
#define GSCAN_NO_FULL_SCAN_RESULTS_CUR_SCAN  (1 << 2)
#define GSCAN_SEND_ALL_RESULTS_CUR_SCAN     (GSCAN_SEND_ALL_RESULTS_MASK | \
								GSCAN_FULL_SCAN_RESULTS_CUR_SCAN)

typedef struct wl_pfn_bssidinfo {
	uint8		network_found;
	uint8		flags;
	uint16		channel;
	int16		rssi;
	/* updated to the current system time in milisec upon finding the network */
	uint32		time_stamp;
	wlc_ssid_t	ssid;
} wl_pfn_bssidinfo_t;

typedef struct wl_pfn_bssid_list {
	/* link to next node */
	struct wl_pfn_bssid_list	*next;
	struct  ether_addr		bssid;
	/* bit4: suppress_lost, bit3: suppress_found */
	uint16				flags;
	wl_pfn_bssidinfo_t		*pbssidinfo;
} wl_pfn_bssid_list_t;

/* structure defination */
struct wl_pfn_bdcast_list {
	/* link to next node */
	struct wl_pfn_bdcast_list	* next;
	struct  ether_addr		bssid;
	wl_pfn_bssidinfo_t		bssidinfo;
};

#ifdef GSCAN
typedef struct wl_pfn_swc_list {
	struct wl_pfn_swc_list     *next;
	uint8                        missing_scan_count;
	uint8                        channel;
	struct  ether_addr           bssid;
	uint8                        flags;
	uint8                        rssi_idx;
	int8                         rssi_low_threshold;
	int8                         rssi_high_threshold;
	int8                         prssi_history[1];
} wl_pfn_swc_list_t;

typedef struct wl_pfn_gscan_params {
	/* Buffer filled threshold in % to generate an event */
	uint8  buffer_threshold;
	/* No. of APs threshold to generate an evt */
	uint8  swc_nbssid_threshold;
	uint8  count_of_channel_buckets;
	uint8  num_channels_in_scan;
	uint8  swc_rssi_window_size;
	uint8  lost_ap_window;
	uint16 num_scan;
	uint16 num_swc_bssid;
	uint16 gscan_flags;
	wl_pfn_gscan_channel_bucket_t *channel_bucket;
	chanspec_t  *chan_orig_list;
	chanspec_t  *chan_scanned;
} wl_pfn_gscan_params_t;
#endif /* GSCAN */

typedef struct wl_pfn_internal {
	struct  ether_addr	bssid;
	uint8			network_found;
	/* True if network was found during broad cast scan */
	uint8			flags;
	uint16			channel;
	int16			rssi;
	/* updated to the current system time in milisec upon finding the network */
	uint32			time_stamp;
	/* PFN data sent by user */
	wl_pfn_t		pfn;
#if defined(BCMROMBUILD) || defined(BCMROMOFFLOAD)
	void			*pad;
#endif // endif
} wl_pfn_internal_t;

typedef struct wl_pfn_group_info {
	int32  gp_scanfreq;  /* active freq for this group, overrides global */
	int32  gp_losttime;
	uint8  gp_adaptcnt;
	uint8  gp_curradapt;
	wl_pfn_mpf_state_params_t mpf_states[WL_PFN_MPF_STATES_MAX];
} wl_pfn_group_info_t;

#define BSSIDLIST_MAX			16
#define REPEAT_MAX			100
#define EXP_MAX				5
#define WLC_BESTN_TUNABLE(wlc)		(wlc)->pub->tunables->maxbestn
#define WLC_MSCAN_TUNABLE(wlc)		(wlc)->pub->tunables->maxmscan

/* bit map in pfn internal flag */
#define SUSPEND_BIT                      0
#define PROHIBITED_BIT                   1
#define HISTORY_OFF_BIT                  2
#define PFN_MACADDR_BIT                  3
#define PFN_ONLY_MAC_OUI_SET_BIT         4
#define PFN_MACADDR_UNASSOC_ONLY_BIT     5

struct wl_pfn_info {
	/* Broadcast or directed scan */
	int16			cur_scan_type;
	uint16			bdcastnum;
	/* Link list to hold network found as part of broadcast and not from PFN list */
	wl_pfn_bdcast_list_t	* bdcast_list;
	/* PFN task timer */
	struct wl_timer		* p_pfn_timer;
	wlc_info_t		* wlc;
	/* Number of SSID based PFN network registered by user */
	int16			count;
	/* Pointer array for SSID based pfn network private data */
	wl_pfn_internal_t	**pfn_internal;
	/* Current index into the PFN list where scanning in progress */
	int16			cur_scan_index;
	/* Index to PFN array of associated network */
	int16			associated_idx;
	/* Current state of scan state machine */
	PFNSCANSTATE		pfn_scan_state;
	/* Current state of network state */
	PFNNETSTATE		pfn_network_state;
	/* PFN parameter data sent by user */
	wl_pfn_param_t		*param;
	/* SSID arrays for directed scanning */
	wlc_ssid_t		*ssid_list;
    uint8			nbss, nibss;
	uint32			ssidlist_len;
	/* pointer to memory holding bestnet for current scan */
	wl_pfn_bestnet_t	*current_bestn;
	/* best networks in history */
	wl_pfn_bestnet_t	*bestnethdr;
	wl_pfn_bestnet_t	*bestnettail;
	/* number of scan in bestnet */
	uint16			numofscan;
	/* count of BSSID based PFN networks */
	uint16			bssidNum;
	/* Pointer array for BSSID based PFN network */
	wl_pfn_bssid_list_t	*bssid_list[BSSIDLIST_MAX];
	/* counter for adaptive scanning */
	uint8			adaptcnt;
	/* number of found and lost networks in each scan */
	uint8			foundcnt;
	uint8			lostcnt;
	/* channel spec list */
	int8			chanspec_count;
	chanspec_t		chanspec_list[WL_NUMCHANNELS];
	/* size fo bestn bestnetinfo in bestnet */
	uint16			bestnetsize;
	/* number of hidden networks */
	uint8			hiddencnt;
	uint8			currentadapt;
	uint32			reporttype;
	uint8			availbsscnt;
	uint8			none_cnt;
	uint8			reportloss;
	/* internal flag: bit 0 (suspend/resume) */
	uint16			intflag;
	int32			slow_freq; /* slow scan period */
#ifdef NLO
	/* saved suppress ssid setting */
	bool			suppress_ssid_saved;
#endif /* NLO */
	uint8					foundcnt_ssid;
	uint8					lostcnt_ssid;
	uint8					foundcnt_bssid;
	uint8					lostcnt_bssid;
	uint8					rttn;

	/* dynamic scan time management */
	uint32			last_scantime;	    /* last overall scan time */
	int32			a_scanfreq;         /* shifted/adapted scan freq */
	int32			a_losttime;         /* shifted/adapted lost timeout */
	uint16			scan_margin;	    /* combine scans this close together */
	bool			timer_active;	    /* flag indicating if timer is set */
	uint8			scangroups;	    /* bitmask of groups being scanned */
	uint8			activegroups;	    /* bitmask of activity for adaptive scan */
	bool			mpf_registered;	    /* is MPF registration complete */
	uint8			mpf_state;	    /* current state if mpf is in use */
	uint8			ngroups;	    /* number of groups supported */

	/* Per-group overrides.  Groups for now: SSID (0) and BSSID (1) */
	uint32			*last_gscan; /* per-group last time */
	wl_pfn_group_info_t	**groups;

	/* temporary global override */
	/* XXX While timer active awaiting an override, start_ms is nonzero
	 * and end_ms zero; while timer active during an override, start_ms
	 * is zero and end_ms nonzero.  So force values to zero as needed,
	 * and don't allow zero as a real value (change to 1).
	 */
	struct wl_timer		*ovtimer;	    /* timer for override */
	wl_pfn_override_param_t ovparam;	    /* temporary override parameters */
	uint32			ovref_ms;	    /* timer start time (OSL ms) */
	uint32 			start_ms;	    /* expected OSL (ms) time to start */
	uint32			end_ms;		    /* expected OSL (ms) time to end */
	wl_pfn_mpf_state_params_t savedef;	    /* saved defaults during temporary */

	/* override MAC for scan source address */
	struct ether_addr	pfn_mac;
	struct ether_addr	save_mac;

	/* sensor hub scan */
	shif_info_t		*shifp;
	uint8			bdlistcnt_thisscan;
	uint8			gpspfn;
	int16			*gps_saveflags;
	uint8			scan_retry;
#ifdef GSCAN
	/* gscan */
	bool					bestn_event_sent;
	wl_pfn_gscan_params_t   *pfn_gscan_cfg_param;
	wl_pfn_swc_list_t       *pfn_swc_list_hdr;
#endif /* GSCAN */
};

#define	BSSID_RPTLOSS_BIT	0
#define	SSID_RPTLOSS_BIT	1
#define	BSSID_RPTLOSS_MASK	1
#define	SSID_RPTLOSS_MASK	2

/* Number of retries for gps scan */
#define PFN_GPS_SCAN_RETRY		3
#define PFN_GPS_SCAN_RETRY_INTERVAL	2000

/* Local function prototypes */
static int wl_pfn_set_params(wl_pfn_info_t * pfn_info, void * buf);
static int wl_pfn_enable(wl_pfn_info_t * pfn_info, int enable);
static int wl_pfn_best(wl_pfn_info_t * pfn_info, void * buf, int len);
static int wl_pfn_lbest(wl_pfn_info_t * pfn_info, void * buf, int len);
static int wl_pfn_best_bssid(wl_pfn_info_t * pfn_info, void * buf, int len);
static void wl_pfn_scan_complete(void * arg, int status, wlc_bsscfg_t *cfg);
static void wl_pfn_timer(void * arg);
static int wl_pfn_start_scan(wl_pfn_info_t * pfn_info, wlc_ssid_t	* ssid,
                              int nssid, int bss_type);
static wl_pfn_bestinfo_t* wl_pfn_get_best_networks(wl_pfn_info_t *pfn_info,
	wl_pfn_bestinfo_t *bestap_ptr, int16 rssi, struct ether_addr bssid);
static wl_pfn_bestinfo_bssid_t* wl_pfn_get_best_networks_b(wl_pfn_info_t *pfn_info,
	wl_pfn_bestinfo_bssid_t *bestap_ptr, int16 rssi, struct ether_addr bssid);
static void wl_pfn_updatenet(wl_pfn_info_t *pfn_info, wl_pfn_net_info_t *foundptr,
	wl_pfn_net_info_t *lostptr, uint8 foundcnt, uint8 lostcnt, uint8 reportnet);
static void wl_pfn_attachbestn(wl_pfn_info_t * pfn_info, bool partial);
#ifdef GSCAN
static int wl_pfn_calc_num_bestn(wl_pfn_info_t * pfn_info);
static int wl_pfn_cfg_gscan(wl_pfn_info_t * pfn_info, void * buf, int len);
static int wl_pfn_add_swc_bssid(wl_pfn_info_t * pfn_info, void * buf, int len);
static void wl_pfn_free_swc_bssid_list(wl_pfn_info_t * pfn_info);
static void wl_pfn_process_swc_bssid(wl_pfn_info_t *pfn_info, wlc_bss_info_t * bi);
static void wl_pfn_evaluate_swc(wl_pfn_info_t *pfn_info);
static int8 wl_pfn_swc_calc_avg_rssi(wl_pfn_info_t *pfn_info,
   wl_pfn_swc_list_t  *swc_list_ptr);
static void wl_pfn_gen_swc_event(wl_pfn_info_t *pfn_info, uint32 change_count);
static bool is_channel_in_list(chanspec_t *chanspec_list,
                     uint16 channel, uint32 num_channels);
#endif /* GSCAN */
static bool is_pfn_macaddr_change_enabled(wlc_info_t * wlc);

/* Rommable function prototypes */
static int wl_pfn_allocate_bdcast_node(wl_pfn_info_t * pfn_info,
	uint8 ssid_len, uint8 * ssid, uint32 now);
static int wl_pfn_clear(wl_pfn_info_t * pfn_info);
static int wl_pfn_validate_set_params(wl_pfn_info_t* pfn_info, void* buf);
static int wl_pfn_add(wl_pfn_info_t * pfn_info, void * buf, int len);
static int wl_pfn_add_bssid(wl_pfn_info_t * pfn_info, void * buf, int len);
static int wl_pfn_cfg(wl_pfn_info_t * pfn_info, void * buf, int len);
static void wl_pfn_free_bdcast_list(wl_pfn_info_t * pfn_info);
static void wl_pfn_prepare_for_scan(wl_pfn_info_t* pfn_info,
	wlc_ssid_t** ssid, int* nssid, int32* bss_type);
static void wl_pfn_ageing(wl_pfn_info_t * pfn_info, uint32 now);
static int wl_pfn_send_event(wl_pfn_info_t * pfn_info,
	void *data, uint32 datalen, int event_type);
void wl_pfn_process_scan_result(wl_pfn_info_t * pfn_info, wlc_bss_info_t * bi);
static void wl_pfn_cipher2wsec(uint8 ucount, uint8 * unicast, uint32 * wsec);
static void wl_pfn_free_ssidlist(wl_pfn_info_t * pfn_info, bool freeall, bool clearjustfoundonly);
static void wl_pfn_free_bssidlist(wl_pfn_info_t * pfn_info, bool freeall, bool clearjustfoundonly);
static void wl_pfn_free_bestnet(wl_pfn_info_t * pfn_info);
static int wl_pfn_setscanlist(wl_pfn_info_t *pfn_info, int enable);
static int wl_pfn_updatecfg(wl_pfn_info_t * pfn_info, wl_pfn_cfg_t * pcfg);
static int wl_pfn_suspend(wl_pfn_info_t * pfn_info, int suspend);
static int wl_pfn_override_set(wl_pfn_info_t * pfn_info, void * buf, int len);
static int wl_pfn_override_get(wl_pfn_info_t * pfn_info, void * buf, int len,
                               void * inp, uint inp_len);
static void wl_pfn_ovtimer(void * arg);
static void wl_pfn_ovactivate(wl_pfn_info_t *pfn_info, bool on);
static void wl_pfn_override_cancel(wl_pfn_info_t *pfn_info);
static void wl_pfn_macaddr_apply(wl_pfn_info_t *pfn_info, bool apply);

static int wl_pfn_macaddr_set(wl_pfn_info_t * pfn_info, void * buf, int len);
static int wl_pfn_macaddr_get(wl_pfn_info_t * pfn_info, void * buf, int len,
                              void * inp, uint inp_len);

#ifdef WL_MPF
static int wl_pfn_mpfset(wl_pfn_info_t * pfn_info, void * buf, int len);
static int wl_pfn_mpfget(wl_pfn_info_t * pfn_info, void * buf, int len, void * inp, uint inp_len);
static void wl_pfn_mpf_notify(void *handle, uintptr subhandle, uint old_state, uint new_state);
#endif /* WL_MPF */

void wl_pfn_start_timer(wl_pfn_info_t *pfn_info, bool now);

#if defined(WL_SHIF)
#if defined(HNDGCI)
static int wl_pfn_gps_scan(wl_pfn_info_t * pfn_info, int gps_enable);
static void wl_pfn_shub_cb(wl_pfn_info_t * pfn_info, char *buf, int len);
#endif /* defined(HNDGCI) */
static void wl_pfn_gps_scanresult(wl_pfn_info_t * pfn_info);
static void wl_pfn_gps_send_scanresult(wl_pfn_info_t *pfn_info, char *buf, uint16 len);
#endif /* defined(WL_SHIF) */

/* IOVar table */
static const bcm_iovar_t wl_pfn_iovars[] = {
	{"pfn_set", IOV_PFN_SET_PARAM,
#if defined(WLC_PATCH) && defined(BCM43362A2)
	(0), IOVT_BUFFER, sizeof(wl_pfn_param_t)
#else
	(0), IOVT_BUFFER, OFFSETOF(wl_pfn_param_t, slow_freq)
#endif // endif
	},
	{"pfn_cfg", IOV_PFN_CFG,
	(0), IOVT_BUFFER, OFFSETOF(wl_pfn_cfg_t, flags)
	},
	{"pfn_add", IOV_PFN_ADD,
	(0), IOVT_BUFFER, sizeof(wl_pfn_t)
	},
	{"pfn_add_bssid", IOV_PFN_ADD_BSSID,
	(0), IOVT_BUFFER, sizeof(wl_pfn_bssid_t)
	},
	{"pfn", IOV_PFN,
	(0), IOVT_BOOL, 0
	},
	{"pfnclear", IOV_PFN_CLEAR,
	(0), IOVT_VOID, 0
	},
	{"pfnbest", IOV_PFN_BEST,
	(0), IOVT_BUFFER, sizeof(wl_pfn_scanresults_t)
	},
	{"pfn_suspend", IOV_PFN_SUSPEND,
	(0), IOVT_BOOL, 0
	},
	{"pfnlbest", IOV_PFN_LBEST,
	(0), IOVT_BUFFER, sizeof(wl_pfn_lscanresults_t)
	},
	{"pfnmem", IOV_PFN_MEM,
	(0), IOVT_BUFFER, 0
	},
	{"pfnrttn", IOV_PFN_RTTN,
	(0), IOVT_BUFFER, 0
	},
	{"pfnbest_bssid", IOV_PFN_BEST_BSSID,
	(0), IOVT_BUFFER, sizeof(wl_pfn_scanhist_bssid_t),
	},
#ifdef WL_MPF
	{"pfn_mpfset", IOV_PFN_MPFSET,
	(0), IOVT_BUFFER, sizeof(wl_pfn_mpf_param_t)
	},
#endif /* WL_MPF */
	{"pfn_override", IOV_PFN_OVERRIDE,
	(0), IOVT_BUFFER, sizeof(wl_pfn_override_param_t)
	},
	{"pfn_macaddr", IOV_PFN_MACADDR,
	(0), IOVT_BUFFER, sizeof(wl_pfn_macaddr_cfg_t)
	},
#if defined(WL_SHIF)
	{"pfn_shub_req", IOV_PFN_SHUB_REQ,
	(0), IOVT_UINT32, 0
	},
#endif /* defined(WL_SHIF) */
#ifdef GSCAN
	{"pfn_gscan_cfg", IOV_PFN_GSCAN_CFG,
	(0), IOVT_BUFFER, sizeof(wl_pfn_gscan_cfg_t)
	},
	{"pfn_add_swc_bssid", IOV_PFN_ADD_SWC_BSSID,
	(0), IOVT_BUFFER, sizeof(wl_pfn_significant_bssid_t)
	},
#endif /* GSCAN */
	{NULL, 0, 0, 0, 0 }
};

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

static int
wl_pfn_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif)
{
	wl_pfn_info_t *pfn_info = (wl_pfn_info_t *)hdl;
	int enable, suspend, rtt;
	int err = BCME_UNSUPPORTED;
	int32 *ret_int_ptr = (int32*)arg;
	int bestn, maxmscan;

	ASSERT(pfn_info);
	ASSERT(pfn_info->wlc && pfn_info->wlc->pub);

	switch (actionid) {
	case IOV_SVAL(IOV_PFN_SET_PARAM):
		err = wl_pfn_set_params(pfn_info, arg);
		break;
	case IOV_SVAL(IOV_PFN_CFG):
		err = wl_pfn_cfg(pfn_info, arg, len);
		break;
#ifdef GSCAN
	case IOV_SVAL(IOV_PFN_GSCAN_CFG):
		err = wl_pfn_cfg_gscan(pfn_info, arg, len);
		break;
#endif /* GSCAN */
	case IOV_SVAL(IOV_PFN_ADD):
		err = wl_pfn_add(pfn_info, arg, len);
		break;
	case IOV_SVAL(IOV_PFN_ADD_BSSID):
		err = wl_pfn_add_bssid(pfn_info, arg, len);
		break;
#ifdef GSCAN
	case IOV_SVAL(IOV_PFN_ADD_SWC_BSSID):
		err = wl_pfn_add_swc_bssid(pfn_info, arg, len);
		break;
#endif /* GSCAN */
	case IOV_SVAL(IOV_PFN):
		bcopy(arg, &enable, sizeof(enable));
		err = wl_pfn_enable(pfn_info, enable);
		break;
	case IOV_GVAL(IOV_PFN):
		*ret_int_ptr = (int32)(PFN_SCAN_DISABLED != pfn_info->pfn_scan_state);
		err = BCME_OK;
		break;
	case IOV_SVAL(IOV_PFN_CLEAR):
		err = wl_pfn_clear(pfn_info);
		break;
	case IOV_GVAL(IOV_PFN_BEST):
		if (pfn_info->param->flags & BESTN_BSSID_ONLY_MASK)
			err = BCME_NOTFOUND;
		else
			err = wl_pfn_best(pfn_info, arg, len);
		break;
	case IOV_SVAL(IOV_PFN_SUSPEND):
		bcopy(arg, &suspend, sizeof(suspend));
		err = wl_pfn_suspend(pfn_info, suspend);
		break;
	case IOV_GVAL(IOV_PFN_SUSPEND):
		*ret_int_ptr = ((pfn_info->intflag & (ENABLE << SUSPEND_BIT))? 1 : 0);
		err = BCME_OK;
		break;
	case IOV_GVAL(IOV_PFN_LBEST):
		err = wl_pfn_lbest(pfn_info, arg, len);
		break;
	case IOV_SVAL(IOV_PFN_MEM):
		/* Make sure PFN is not active already */
		if (PFN_SCAN_DISABLED != pfn_info->pfn_scan_state) {
			WL_ERROR(("wl%d: PFN is already active, can't service IOV_PFN_MEM\n",
				pfn_info->wlc->pub->unit));
			return BCME_EPERM;
		}
		bcopy(arg, &bestn, sizeof(bestn));
		if (bestn > 0 && bestn <= WLC_BESTN_TUNABLE(pfn_info->wlc)) {
			pfn_info->param->bestn = (uint8)bestn;
			pfn_info->bestnetsize = sizeof(wl_pfn_bestnet_t) +
			    sizeof(wl_pfn_bestinfo_t) * (pfn_info->param->bestn - 1);
			err = BCME_OK;
		} else
			err = BCME_RANGE;
		break;
	case IOV_GVAL(IOV_PFN_MEM):
		if (pfn_info->bestnetsize) {
#ifdef DONGLEBUILD
			maxmscan = OSL_MEM_AVAIL() / pfn_info->bestnetsize;
			*ret_int_ptr = MIN(maxmscan, WLC_MSCAN_TUNABLE(pfn_info->wlc));
#else
			*ret_int_ptr = WLC_MSCAN_TUNABLE(pfn_info->wlc);
#endif // endif
			err = BCME_OK;
		} else {
			err = BCME_ERROR;
		}
		break;
	case IOV_SVAL(IOV_PFN_RTTN): /* set # of bestn that needs to report rtt */
		bcopy(arg, &rtt, sizeof(rtt));
		if (rtt > BESTN_MAX)
			err = BCME_RANGE;
		else {
			pfn_info->rttn = (uint8)*ret_int_ptr;
			err = BCME_OK;
		}
		break;
	case IOV_GVAL(IOV_PFN_BEST_BSSID):
		if (pfn_info->param->flags & BESTN_BSSID_ONLY_MASK)
			err = wl_pfn_best_bssid(pfn_info, arg, len);
		else
			err = BCME_NOTFOUND;
		break;
#ifdef WL_MPF
	case IOV_SVAL(IOV_PFN_MPFSET):
		if (MPF_ENAB(pfn_info->wlc->pub))
			err = wl_pfn_mpfset(pfn_info, arg, len);
		else
			err = BCME_UNSUPPORTED;
		break;

	case IOV_GVAL(IOV_PFN_MPFSET):
		if (MPF_ENAB(pfn_info->wlc->pub))
			err = wl_pfn_mpfget(pfn_info, arg, len, params, p_len);
		else
			err = BCME_UNSUPPORTED;
		break;
#endif /* WL_MPF */
	case IOV_GVAL(IOV_PFN_OVERRIDE):
		err = wl_pfn_override_get(pfn_info, arg, len, params, p_len);
		break;
	case IOV_SVAL(IOV_PFN_OVERRIDE):
		err = wl_pfn_override_set(pfn_info, arg, len);
		break;
	case IOV_GVAL(IOV_PFN_MACADDR):
		err = wl_pfn_macaddr_get(pfn_info, arg, len, params, p_len);
		break;
	case IOV_SVAL(IOV_PFN_MACADDR):
		err = wl_pfn_macaddr_set(pfn_info, arg, len);
		break;
#if defined(WL_SHIF)
	case IOV_SVAL(IOV_PFN_SHUB_REQ):
		/* will be extended to support different types of req */
		if (SHIF_ENAB(pfn_info->wlc->pub)) {
			int32 a_scanfreq;

			memcpy(&a_scanfreq, arg, sizeof(a_scanfreq));
			pfn_info->a_scanfreq = a_scanfreq;
		}
		break;
#endif /* defined(WL_SHIF) */
	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

wl_pfn_info_t *
BCMATTACHFN(wl_pfn_attach)(wlc_info_t * wlc)
{
	wl_pfn_info_t	* pfn_info;
	uint8		ngroups;
	uint		infosize;

	STATIC_ASSERT(WL_PFN_BESTNET_FIXED == WL_PFN_BESTNET_B_FIXED);

	ngroups = WL_PFN_MPF_MAX_GROUPS;
	infosize = ROUNDUP(sizeof(wl_pfn_info_t), sizeof(uint32)) +
	        (ngroups * sizeof(uint32)) +
	        (ngroups * sizeof(wl_pfn_group_info_t *));

	/* Allocate for pfn data */
	if (!(pfn_info = (wl_pfn_info_t *)MALLOCZ(wlc->osh, infosize))) {
		WL_ERROR(("wl%d: PFN: MALLOC failed, size = %d\n", wlc->pub->unit,
			infosize));
		goto error;
	}
	pfn_info->wlc = wlc;
	wlc->pfn = pfn_info;
	pfn_info->scan_margin = DEFAULT_SCAN_MARGIN;

	pfn_info->ngroups = ngroups;
	pfn_info->last_gscan = (uint32*)((uintptr)pfn_info +
	                                 ROUNDUP(sizeof(wl_pfn_info_t), sizeof(uint32)));
	pfn_info->groups = (wl_pfn_group_info_t**)&pfn_info->last_gscan[ngroups];

	pfn_info->scangroups = (1 << WL_PFN_MPF_GROUP_SSID) | (1 << WL_PFN_MPF_GROUP_BSSID);

	/* Allocate resource for timer, but don't start it */
	pfn_info->p_pfn_timer = wl_init_timer(pfn_info->wlc->wl,
		wl_pfn_timer, pfn_info, "pfn");
	if (!pfn_info->p_pfn_timer) {
		WL_ERROR(("wl%d: Failed to allocate resoure for timer\n", wlc->pub->unit));
		goto error;
	}

	if (!(pfn_info->pfn_internal =
	    (wl_pfn_internal_t **)MALLOCZ(wlc->osh,
	    sizeof(wl_pfn_internal_t *) * MAX_PFN_LIST_COUNT))) {
		WL_ERROR(("wl%d: PFN: MALLOC failed, size = %d\n", wlc->pub->unit,
			sizeof(wl_pfn_internal_t *) * MAX_PFN_LIST_COUNT));
		goto error;
	}

	if (!(pfn_info->param =
	    (wl_pfn_param_t *)MALLOCZ(wlc->osh, sizeof(wl_pfn_param_t)))) {
		WL_ERROR(("wl%d: PFN: MALLOC failed, size = %d\n", wlc->pub->unit,
			sizeof(wl_pfn_param_t)));
		goto error;
	}

#ifdef GSCAN
	if (!(pfn_info->pfn_gscan_cfg_param =
	    (wl_pfn_gscan_params_t *)MALLOCZ(wlc->osh, sizeof(wl_pfn_gscan_params_t)))) {
		WL_ERROR(("wl%d: PFN: MALLOC failed, size = %d\n", wlc->pub->unit,
			sizeof(wl_pfn_gscan_params_t)));
		goto error;
	}
#endif /* GSCAN */

	/* register a module (to handle iovars) */
	if (wlc_module_register(wlc->pub, wl_pfn_iovars, "wl_rte_iovars", pfn_info,
		wl_pfn_doiovar, NULL, NULL, NULL)) {
		WL_ERROR(("wl%d: Error registering pfn iovar\n", wlc->pub->unit));
		goto error;
	}

	/* Allocate the optional override timer, and initialize the iovar version */
	pfn_info->ovtimer = wl_init_timer(pfn_info->wlc->wl, wl_pfn_ovtimer, pfn_info, "pfnov");
	if (!pfn_info->ovtimer) {
		WL_ERROR(("wl%d: Failed to allocate ovtimer\n", wlc->pub->unit));
		goto error;
	}
	pfn_info->ovparam.version = WL_PFN_OVERRIDE_VERSION;

#if defined(WL_ENAB_RUNTIME_CHECK) || !defined(DONGLEBUILD)
	/* this value only read by WLPFN_ENAB in these compile conditions */
	wlc->pub->_wlpfn = TRUE;
#endif /* defined(WL_ENAB_RUNTIME_CHECK) || !defined(DONGLEBUILD) */

#if defined(WL_SHIF) && !defined(WL_SHIF_DISABLED)
#ifdef HNDGCI
	/* register callback and delimiter to GCI uart */
	if (hndgci_uart_config_rx_complete('\n', -1, 10,
		(gci_rx_cb_t)wl_pfn_shub_cb, (void *)pfn_info)) {
		WL_ERROR(("wl%d: Error registering GCI uart callback\n", wlc->pub->unit));
		goto error;
	}
#endif /* HNDGCI */
#endif /* defined(WL_SHIF) && !defined(WL_SHIF_DISABLED) */

	return pfn_info;

error:
	wl_pfn_detach(pfn_info);

	return 0;
}

int
BCMATTACHFN(wl_pfn_detach)(wl_pfn_info_t * pfn_info)
{
	int callbacks = 0;
	uint infosize;
#ifdef WL_MPF
	uint groupid;
#endif // endif

	ASSERT(pfn_info && pfn_info->wlc && pfn_info->wlc->pub);

	/* Clean up memory */
	if (pfn_info) {

		/* Cancel the scanning timer */
		if (pfn_info->p_pfn_timer) {
			if (!wl_del_timer(pfn_info->wlc->wl, pfn_info->p_pfn_timer))
				callbacks++;

			pfn_info->timer_active = FALSE;
			wl_free_timer(pfn_info->wlc->wl, pfn_info->p_pfn_timer);
		}

		/* Cancel the override timer */
		if (pfn_info->ovtimer) {
			if (!wl_del_timer(pfn_info->wlc->wl, pfn_info->ovtimer))
				callbacks++;

			wl_free_timer(pfn_info->wlc->wl, pfn_info->ovtimer);
		}

		/* Disable before calling 'wl_pfn_clear' */
		pfn_info->pfn_scan_state = PFN_SCAN_DISABLED;

		/* Free ssid list for directed scan */
		if (pfn_info->ssid_list != NULL) {
			MFREE(pfn_info->wlc->osh, pfn_info->ssid_list,
			      pfn_info->ssidlist_len);
			pfn_info->ssid_list = NULL;
		}

		wl_pfn_clear(pfn_info);

		wlc_module_unregister(pfn_info->wlc->pub, "wl_rte_iovars", pfn_info);

		MFREE(pfn_info->wlc->osh, pfn_info->pfn_internal,
		      sizeof(wl_pfn_internal_t *) * MAX_PFN_LIST_COUNT);
		MFREE(pfn_info->wlc->osh, pfn_info->param, sizeof(wl_pfn_param_t));
#ifdef GSCAN
		if (pfn_info->pfn_gscan_cfg_param) {
			if (pfn_info->pfn_gscan_cfg_param->chan_orig_list) {
				MFREE(pfn_info->wlc->osh,
				pfn_info->pfn_gscan_cfg_param->chan_orig_list,
				((sizeof(chanspec_t) * pfn_info->chanspec_count)));
			}

			if (pfn_info->pfn_gscan_cfg_param->chan_scanned) {
				MFREE(pfn_info->wlc->osh,
				pfn_info->pfn_gscan_cfg_param->chan_scanned,
				((sizeof(chanspec_t) * pfn_info->chanspec_count)));
			}

			if (pfn_info->pfn_gscan_cfg_param->channel_bucket) {
				MFREE(pfn_info->wlc->osh,
				 pfn_info->pfn_gscan_cfg_param->channel_bucket,
				 (sizeof(wl_pfn_gscan_channel_bucket_t) *
				 pfn_info->pfn_gscan_cfg_param->count_of_channel_buckets));
			 }

			MFREE(pfn_info->wlc->osh, pfn_info->pfn_gscan_cfg_param,
			 sizeof(wl_pfn_gscan_params_t));
		}
#endif /* GSCAN */

#ifdef WL_MPF
		if (MPF_ENAB(pfn_info->wlc->pub)) {
			for (groupid = 0; groupid < pfn_info->ngroups; groupid++) {
				if (pfn_info->groups[groupid]) {
					MFREE(pfn_info->wlc->osh, pfn_info->groups[groupid],
					      sizeof(wl_pfn_group_info_t));
					pfn_info->groups[groupid] = NULL;
				}
			}
			if (pfn_info->mpf_registered) {
				int err;
				err = wlc_mpf_unregister(pfn_info->wlc, 0,
				                         wl_pfn_mpf_notify, pfn_info, 0);
				if (err) {
					WL_ERROR(("pfn_detach: mpf_unregister error %d\n", err));
				}
			}
		}
#endif /* WL_MPF */

#if defined(WL_SHIF) && !defined(WL_SHIF_DISABLED)
		if (pfn_info->shifp) {
			bcm_shif_deinit(pfn_info->shifp);
			pfn_info->shifp = NULL;
		}
#endif /* defined(WL_SHIF) && !defined(WL_SHIF_DISABLED) */

		infosize = ROUNDUP(sizeof(wl_pfn_info_t), sizeof(uint32)) +
		        (pfn_info->ngroups * sizeof(uint32)) +
		        (pfn_info->ngroups * sizeof(wl_pfn_group_info_t *));
		MFREE(pfn_info->wlc->osh, pfn_info, infosize);
		pfn_info = NULL;
	}

	return callbacks;
}

/*
 * Schedule a timer for the next scan, as a delta from the
 * most recent scan or from, or from now (as per the 'new' arg).
 */
void
wl_pfn_start_timer(wl_pfn_info_t *pfn_info, bool new)
{
	uint32 now = OSL_SYSUPTIME();
	uint groupid;
	wl_pfn_group_info_t *group = NULL;
	uint32 gtime, mintime;

	if (pfn_info->timer_active) {
		WL_MPF_ERR(("START_TIMER: bailing, timer already active\n"));
		return;
	}

	/* For a new enable, everything is relative to now */
	if (new) {
		if (MPF_ENAB(pfn_info->wlc->pub)) {
			for (groupid = 0; groupid < pfn_info->ngroups; groupid++)
				pfn_info->last_gscan[groupid] = now;
		}
		pfn_info->last_scantime = now;
	}

	if (MPF_ENAB(pfn_info->wlc->pub)) {
		/* Find the minimum of the various wakeup time requirements */
		WL_MPF_DEBUG(("START_TIMER: state %d\n", pfn_info->mpf_state));
		for (mintime = 0xffffffff, groupid = 0; groupid < pfn_info->ngroups; groupid++) {
			if (!pfn_info->ovparam.duration || pfn_info->ovparam.start_offset)
				group = pfn_info->groups[groupid];
			gtime = pfn_info->last_gscan[groupid] +
			        ((group && group->gp_scanfreq)
			         ? group->gp_scanfreq : pfn_info->a_scanfreq);
			gtime -= now;
			if ((int32)gtime < 0)
				gtime = 0;
			WL_MPF_DEBUG(("(%d: %4u.%3u) ", groupid, gtime / 1000, gtime % 1000));
			if (gtime < mintime)
				mintime = gtime;
		}
		WL_MPF_DEBUG((": set %4u.%3u\n", mintime / 1000, mintime % 1000));
	} else {
		mintime = pfn_info->last_scantime + pfn_info->a_scanfreq;
		mintime -= now;
		if ((int32)mintime < 0) {
			mintime = 0;
		}
	}

	pfn_info->timer_active = TRUE;
	wl_add_timer(pfn_info->wlc->wl, pfn_info->p_pfn_timer, mintime, FALSE);
}

static int
wl_pfn_validate_set_params(wl_pfn_info_t* pfn_info, void* buf)
{
	/* Make sure PFN is not active already */
	if (PFN_SCAN_DISABLED != pfn_info->pfn_scan_state) {
		WL_ERROR(("wl%d: PFN is already active, can't service IOV_PFN_SET\n",
			pfn_info->wlc->pub->unit));
		return BCME_EPERM;
	}

	/* Copy the user/host data into internal structure */
	bcopy(buf, pfn_info->param, sizeof(wl_pfn_param_t));

	/* Make sure we have valid current version */
	if (PFN_VERSION != pfn_info->param->version) {
		WL_ERROR(("wl%d: Incorrect version expected %d, found %d\n",
			pfn_info->wlc->pub->unit, PFN_VERSION, pfn_info->param->version));
		return BCME_VERSION;
	}

	/* Assign default values for user parameter if necessary */
	/* Default is derived based on bzero ing of the structure from the user */
	if (!pfn_info->param->scan_freq)
		pfn_info->param->scan_freq = DEFAULT_SCAN_FREQ;

	/* Convert from sec to ms */
	pfn_info->param->scan_freq *= 1000;

	if (!pfn_info->param->lost_network_timeout)
		pfn_info->param->lost_network_timeout = DEFAULT_LOST_DURATION;
	/* Convert from sec to ms */
	if (pfn_info->param->lost_network_timeout != -1)
		pfn_info->param->lost_network_timeout *= 1000;

#if defined(WLC_PATCH) && defined(BCM43362A2)
	bcopy(buf + sizeof(wl_pfn_param_t), &pfn_info->slow_freq, sizeof(pfn_info->slow_freq));
	pfn_info->slow_freq *= 1000;
#else
	pfn_info->param->slow_freq *= 1000;
#endif /* WLC_PATCH && BCM43362A2 */

	if (!pfn_info->param->mscan)
		pfn_info->param->mscan = DEFAULT_MSCAN;
	else if (pfn_info->param->mscan > WLC_MSCAN_TUNABLE(pfn_info->wlc))
		return BCME_RANGE;

	if (!pfn_info->param->bestn)
		pfn_info->param->bestn = DEFAULT_BESTN;
	else if (pfn_info->param->bestn > WLC_BESTN_TUNABLE(pfn_info->wlc))
		return BCME_RANGE;

	if (pfn_info->param->mscan) {
		uint netsize = sizeof(wl_pfn_bestinfo_t);

		if (pfn_info->param->flags & BESTN_BSSID_ONLY_MASK)
			netsize = sizeof(wl_pfn_bestinfo_bssid_t);

		pfn_info->bestnetsize = WL_PFN_BESTNET_FIXED +
		        (netsize * pfn_info->param->bestn);
	} else {
		pfn_info->bestnetsize = 0;
	}

	if (!pfn_info->param->repeat)
		pfn_info->param->repeat = DEFAULT_REPEAT;
	else if (pfn_info->param->repeat > REPEAT_MAX)
		return BCME_RANGE;

	if (!pfn_info->param->exp)
		pfn_info->param->exp = DEFAULT_EXP;
	else if (pfn_info->param->exp > EXP_MAX)
		return BCME_RANGE;

	return BCME_OK;
}

void
wl_pfn_override_cancel(wl_pfn_info_t *pfn_info)
{
	if (pfn_info->ovparam.start_offset || pfn_info->ovparam.duration) {
		WL_PFN_INFO(("PNO Override: cancel previous (%d, %d)\n",
		             pfn_info->ovparam.start_offset, pfn_info->ovparam.duration));

		/* If we're in an override period, revert to defaults first */
		if (!pfn_info->ovparam.start_offset)
			wl_pfn_ovactivate(pfn_info, FALSE);

		pfn_info->ovparam.start_offset = 0;
		pfn_info->ovparam.duration = 0;
	}
}

static int
wl_pfn_set_params(wl_pfn_info_t * pfn_info, void * buf)
{
	int err;

	/* Cancel any temporary override */
	wl_pfn_override_cancel(pfn_info);

	/* Validate and assign the default parameters */
	if ((err = wl_pfn_validate_set_params(pfn_info, buf)) != BCME_OK)
		return err;

	/* if no attempt to enable autoconnect, we're done */
	if ((pfn_info->param->flags & AUTO_CONNECT_MASK) == 0)
		return BCME_OK;
	CLEAR_WLPFN_AC_FLAG(pfn_info->param->flags);
	return BCME_BADARG;
}

static int
wl_pfn_add(wl_pfn_info_t * pfn_info, void * buf, int len)
{
	wl_pfn_internal_t   * pfn_internal;
	wl_pfn_t            *pfn_ssidnet;

	/* Make sure PFN is not active already */
	if (PFN_SCAN_DISABLED != pfn_info->pfn_scan_state) {
		WL_ERROR(("wl%d: PFN is already active, can't service IOV_PFN_ADD\n",
			pfn_info->wlc->pub->unit));
		return BCME_EPERM;
	}

	ASSERT(!((uintptr)buf & 0x3));
	pfn_ssidnet = (wl_pfn_t *)buf;
	while (len >= sizeof(wl_pfn_t)) {
		/* Check for max pfn element allowed */
		if (pfn_info->count >= MAX_PFN_LIST_COUNT) {
			WL_ERROR(("wl%d: PFN element count %d exceeded limit of %d\n",
			   pfn_info->wlc->pub->unit, pfn_info->count + 1, MAX_PFN_LIST_COUNT));
			return BCME_RANGE;
		}

		/* Allocate memory for pfn internal structure type 'wl_pfn_internal_t' */
		if (!(pfn_internal = (wl_pfn_internal_t *)MALLOC(pfn_info->wlc->osh,
		                      sizeof(wl_pfn_internal_t)))) {
			WL_ERROR(("wl%d: PFN MALLOC failed, size = %d\n",
			          pfn_info->wlc->pub->unit, sizeof(wl_pfn_internal_t)));
			return BCME_NOMEM;
		}
		bzero(pfn_internal, sizeof(wl_pfn_internal_t));

		/* Copy the user/host data into intenal structre */
		bcopy(pfn_ssidnet, &pfn_internal->pfn, sizeof(wl_pfn_t));
		/* set network_found to NOT_FOUND */
		pfn_internal->network_found = PFN_NET_NOT_FOUND;
		pfn_ssidnet++;
		len -= sizeof(wl_pfn_t);

		if (pfn_internal->pfn.flags & WL_PFN_HIDDEN_MASK)
			pfn_info->hiddencnt++;

		if (!(pfn_internal->pfn.flags & WL_PFN_SUPPRESSLOST_MASK)) {
			pfn_info->reportloss |= ENABLE << SSID_RPTLOSS_BIT;
		}

		/* Store the pointer to the block and increment the count */
		pfn_info->pfn_internal[pfn_info->count++] = pfn_internal;
	}
	ASSERT(!len);

	return BCME_OK;
}

static int
wl_pfn_add_bssid(wl_pfn_info_t * pfn_info, void * buf, int len)
{
	wl_pfn_bssid_t      *bssidptr;
	wl_pfn_bssid_list_t * bssid_list_ptr;
	int                 index;

	/* Make sure PFN is not active already */
	if (PFN_SCAN_DISABLED != pfn_info->pfn_scan_state) {
		WL_ERROR(("wl%d: PFN is already active, can't service IOV_PFN_ADD_BSSID\n",
			pfn_info->wlc->pub->unit));
		return BCME_EPERM;
	}

	/* make sure alignment of 2 */
	ASSERT(!((uintptr)buf & 0x1));
	bssidptr = (wl_pfn_bssid_t *)buf;
	while (len >= sizeof (wl_pfn_bssid_t)) {
		index = bssidptr->macaddr.octet[ETHER_ADDR_LEN - 1] & 0xf;
		/* create a new node in bssid_list for new BSSID */
		if (pfn_info->bssidNum >= MAX_BSSID_LIST) {
			WL_ERROR(("wl%d: BSSID count %d exceeded limit of %d\n",
				pfn_info->wlc->pub->unit, pfn_info->bssidNum + 1, MAX_BSSID_LIST));
			return BCME_RANGE;
		}

		if (!(bssid_list_ptr = (wl_pfn_bssid_list_t *)MALLOC(pfn_info->wlc->osh,
		                        sizeof(wl_pfn_bssid_list_t)))) {
			WL_ERROR(("wl%d: malloc failed, size = %d\n",
			   pfn_info->wlc->pub->unit, sizeof(wl_pfn_bssid_list_t)));
			return BCME_NOMEM;
		}
		bzero(bssid_list_ptr, sizeof(wl_pfn_bssid_list_t));
		bcopy(bssidptr->macaddr.octet, bssid_list_ptr->bssid.octet, ETHER_ADDR_LEN);
		bssid_list_ptr->flags = bssidptr->flags;
		if (!(bssid_list_ptr->flags & WL_PFN_SUPPRESSLOST_MASK))
			pfn_info->reportloss |= ENABLE << BSSID_RPTLOSS_BIT;

		bssid_list_ptr->next = pfn_info->bssid_list[index];
		pfn_info->bssid_list[index] = bssid_list_ptr;
		pfn_info->bssidNum++;
		bssidptr++;
		len -= sizeof(wl_pfn_bssid_t);
	}
	ASSERT(!len);

	return BCME_OK;
}

#ifdef GSCAN
/* Add BSSIDs that will need to be tracked for Significant WiFi Change
 * A change is said to occur if rssi crosses low/high threshold
 * (unique for each bssid) or if its LOST after x consecutive scans
 * If m or greater number of APs have this change, send a
 * event to host indicating significant change.
 */
static int
wl_pfn_add_swc_bssid(wl_pfn_info_t * pfn_info, void * buf, int len)
{
	wl_pfn_significant_bssid_t  *swc_bssid_ptr;
	wl_pfn_swc_list_t  *swc_list_ptr;
	uint32 mem_needed;

	/* Make sure PFN is not active already */
	if (PFN_SCAN_DISABLED != pfn_info->pfn_scan_state) {
		WL_ERROR(("wl%d: PFN is already active, can't service IOV_PFN_ADD_SWC_BSSID\n",
			pfn_info->wlc->pub->unit));
		return BCME_EPERM;
	}
	/* make sure alignment of 2 */
	ASSERT(!((uintptr)buf & 0x1));
	swc_bssid_ptr = (wl_pfn_significant_bssid_t *)buf;
	mem_needed = sizeof(wl_pfn_swc_list_t) +
	             pfn_info->pfn_gscan_cfg_param->swc_rssi_window_size - 1;

	while (len >= sizeof (wl_pfn_significant_bssid_t)) {
		/* Make sure max limit is not reached */
		if (pfn_info->pfn_gscan_cfg_param->num_swc_bssid >= PFN_SWC_MAX_NUM_APS) {
			WL_ERROR(("wl%d: Greater than PFN_SWC_MAX_NUM_APS programmed!",
			pfn_info->wlc->pub->unit));
			return BCME_RANGE;
		}

		if (!(swc_list_ptr = (wl_pfn_swc_list_t *)MALLOCZ(pfn_info->wlc->osh,
		                        mem_needed))) {
			WL_ERROR(("wl%d: malloc failed, size = %d\n",
			   pfn_info->wlc->pub->unit, sizeof(wl_pfn_swc_list_t)));
			wl_pfn_free_swc_bssid_list(pfn_info);
			return BCME_NOMEM;
		}

		memcpy(swc_list_ptr->bssid.octet, swc_bssid_ptr->macaddr.octet, ETHER_ADDR_LEN);
		swc_list_ptr->rssi_low_threshold =
		           swc_bssid_ptr->rssi_low_threshold;
		swc_list_ptr->rssi_high_threshold =
		           swc_bssid_ptr->rssi_high_threshold;
		swc_list_ptr->next = pfn_info->pfn_swc_list_hdr;
		pfn_info->pfn_swc_list_hdr = swc_list_ptr;
		swc_bssid_ptr++;
		pfn_info->pfn_gscan_cfg_param->num_swc_bssid++;
		len -= sizeof(wl_pfn_significant_bssid_t);
	}

	ASSERT(!len);

	return BCME_OK;
}

static void wl_pfn_free_swc_bssid_list(wl_pfn_info_t * pfn_info)
{
	wl_pfn_swc_list_t  *swc_list_ptr = pfn_info->pfn_swc_list_hdr;
	wl_pfn_swc_list_t *tmp;
	uint32 mem_alloced;

	mem_alloced = sizeof(wl_pfn_swc_list_t) +
	             pfn_info->pfn_gscan_cfg_param->swc_rssi_window_size - 1;

	while (swc_list_ptr) {
		tmp = swc_list_ptr->next;
		MFREE(pfn_info->wlc->osh, swc_list_ptr, mem_alloced);
		swc_list_ptr = tmp;
	}
	pfn_info->pfn_gscan_cfg_param->num_swc_bssid = 0;
	pfn_info->pfn_swc_list_hdr = NULL;
	return;
}
#endif /* GSCAN */

static int
wl_pfn_updatecfg(wl_pfn_info_t * pfn_info, wl_pfn_cfg_t * pcfg)
{
	uint16          *ptr;
	int             i, j;
	uint16		chspec;

	if (pcfg->channel_num >= WL_NUMCHANNELS)
		return BCME_RANGE;

	/* validate channel list */
	for (i = 0, ptr = pcfg->channel_list; i < pcfg->channel_num; i++, ptr++) {
		if (pcfg->flags & WL_PFN_CFG_FLAGS_PROHIBITED) {
			chspec = *ptr;
			j = CHSPEC_IS5G(chspec) ? WF_CHAN_FACTOR_5_G : WF_CHAN_FACTOR_2_4_G;
			if (wf_channel2mhz(CHSPEC_CHANNEL(chspec), j) == -1)
				return BCME_BADARG;
		} else if (!wlc_valid_chanspec_db(pfn_info->wlc->cmi, CH20MHZ_CHSPEC(*ptr))) {
			return BCME_BADARG;
		}
	}

	bzero(pfn_info->chanspec_list, sizeof(chanspec_t) * WL_NUMCHANNELS);
	pfn_info->chanspec_count = 0;
	if (pcfg->channel_num) {
		for (i = 0, ptr = pcfg->channel_list;
			i < pcfg->channel_num; i++, ptr++) {
			for (j = 0; j < pfn_info->chanspec_count; j++) {
				if (*ptr < CHSPEC_CHANNEL(pfn_info->chanspec_list[j])) {
					/* insert new channel in the middle */
					memmove(&pfn_info->chanspec_list[j + 1],
					        &pfn_info->chanspec_list[j],
					    (pfn_info->chanspec_count - j) * sizeof(chanspec_t));
					pfn_info->chanspec_list[j] = CH20MHZ_CHSPEC(*ptr);
					pfn_info->chanspec_count++;
					break;
				} else if (*ptr == CHSPEC_CHANNEL(pfn_info->chanspec_list[j])) {
#ifdef GSCAN
					/* mark as duplicate */
					*ptr = 0;
#endif /* GSCAN */
					break;
				}
			}

			if (j == pfn_info->chanspec_count) {
				/* add new channel at the end */
				pfn_info->chanspec_list[j] = CH20MHZ_CHSPEC(*ptr);
				pfn_info->chanspec_count++;
			}
		}
	}

#ifdef GSCAN
	/* Gscan CFG in DHD will ensure that no duplicate channels are sent down
	 * However when gscan isnt used this may not be the case, hence the copying of
	 * the original list in a new loop instead of in the previous loop
	 */
	pfn_info->pfn_gscan_cfg_param->chan_orig_list = (chanspec_t *)
	      MALLOC(pfn_info->wlc->osh, ((sizeof(chanspec_t) * pfn_info->chanspec_count)));

	if (!pfn_info->pfn_gscan_cfg_param->chan_orig_list)
		return BCME_NOMEM;

	pfn_info->pfn_gscan_cfg_param->chan_scanned = (chanspec_t *)
	     MALLOC(pfn_info->wlc->osh, ((sizeof(chanspec_t) * pfn_info->chanspec_count)));

	if (!pfn_info->pfn_gscan_cfg_param->chan_scanned) {
		MFREE(pfn_info->wlc->osh, pfn_info->pfn_gscan_cfg_param->chan_orig_list,
		               ((sizeof(chanspec_t) * pfn_info->chanspec_count)));
		return BCME_NOMEM;
	}

	memset(pfn_info->pfn_gscan_cfg_param->chan_orig_list, 0,
	    ((sizeof(chanspec_t) * pfn_info->chanspec_count)));

	for (i = 0, ptr = pcfg->channel_list; i < pfn_info->chanspec_count; ptr++) {
		if (*ptr) {
			pfn_info->pfn_gscan_cfg_param->chan_orig_list[i++] = CH20MHZ_CHSPEC(*ptr);
		}
	}
#endif /* GSCAN */

	/* Channel list accepted and updated; accept the prohibited flag from user too */
	pfn_info->intflag &= ~(ENABLE << PROHIBITED_BIT);
	if (pcfg->flags & WL_PFN_CFG_FLAGS_PROHIBITED) {
		pfn_info->intflag |= (ENABLE << PROHIBITED_BIT);
	}

	return 0;
}

static int
wl_pfn_cfg(wl_pfn_info_t * pfn_info, void * buf, int len)
{
	wl_pfn_cfg_t        pfncfg;
	int                 err;
	uint8               cur_bsscnt;

	/* Local pfncfg for size adjustment, compatibility */
	memset(&pfncfg, 0, sizeof(wl_pfn_cfg_t));
	bcopy(buf, &pfncfg, MIN(len, sizeof(wl_pfn_cfg_t)));

#ifdef GSCAN
	if (pfn_info->pfn_gscan_cfg_param->chan_orig_list) {
		MFREE(pfn_info->wlc->osh,
		    pfn_info->pfn_gscan_cfg_param->chan_orig_list,
		    ((sizeof(chanspec_t) * pfn_info->chanspec_count)));
	}

	if (pfn_info->pfn_gscan_cfg_param->chan_scanned) {
		MFREE(pfn_info->wlc->osh,
		    pfn_info->pfn_gscan_cfg_param->chan_scanned,
		    ((sizeof(chanspec_t) * pfn_info->chanspec_count)));
	}
#endif /* GSCAN */

	if ((err = wl_pfn_updatecfg(pfn_info, &pfncfg)))
		return err;

	/* Pick up the HISTORY_OFF bit, toss history if required */
	pfn_info->intflag &= ~(ENABLE << HISTORY_OFF_BIT);
	if (pfncfg.flags & WL_PFN_CFG_FLAGS_HISTORY_OFF) {
		pfn_info->intflag |= (ENABLE << HISTORY_OFF_BIT);

		/* free bestnet info (including current) */
		wl_pfn_free_bestnet(pfn_info);
	}

	if (PFN_SCAN_DISABLED != pfn_info->pfn_scan_state) {
		/* Stop the scanning timer */
		wl_del_timer(pfn_info->wlc->wl, pfn_info->p_pfn_timer);
		pfn_info->timer_active = FALSE;

		/* Abort any pending PFN scan */
		if (pfn_info->pfn_scan_state == PFN_SCAN_INPROGRESS) {
			wlc_scan_abort(pfn_info->wlc->scan, WLC_E_STATUS_ABORT);
		}
		/* free ssid scan list */
		if ((err = wl_pfn_setscanlist(pfn_info, FALSE)))
			return err;

		cur_bsscnt = pfn_info->availbsscnt;
		/* reset ssidlist or bssidlist according to new report type in pfncfg */
		if (pfncfg.reporttype == WL_PFN_REPORT_SSIDNET) {
			wl_pfn_free_ssidlist(pfn_info, FALSE, TRUE);
			wl_pfn_free_bssidlist(pfn_info, FALSE, FALSE);
			wl_pfn_free_bdcast_list(pfn_info);
		} else if (pfncfg.reporttype == WL_PFN_REPORT_BSSIDNET) {
			wl_pfn_free_bssidlist(pfn_info, FALSE, TRUE);
			wl_pfn_free_ssidlist(pfn_info, FALSE, FALSE);
			wl_pfn_free_bdcast_list(pfn_info);
		} else {
			wl_pfn_free_ssidlist(pfn_info, FALSE, TRUE);
			wl_pfn_free_bssidlist(pfn_info, FALSE, TRUE);
		}
		/* reset foundcnt, lostcnt for new scan */
		ASSERT(!pfn_info->lostcnt);
		pfn_info->foundcnt = 0;
		pfn_info->foundcnt_ssid = 0;
		pfn_info->foundcnt_bssid = 0;
#ifdef GSCAN
		pfn_info->bestn_event_sent = FALSE;
#endif /* GSCAN */
		/* set non_cnt according to various condition */
		if (pfn_info->reporttype != pfncfg.reporttype) {
			if ((pfncfg.reporttype == WL_PFN_REPORT_ALLNET &&
				!pfn_info->reportloss) ||
				((pfncfg.reporttype == WL_PFN_REPORT_BSSIDNET) &&
				!(pfn_info->reportloss & BSSID_RPTLOSS_MASK)) ||
				((pfncfg.reporttype == WL_PFN_REPORT_SSIDNET) &&
				!(pfn_info->reportloss & SSID_RPTLOSS_MASK))) {
				pfn_info->none_cnt = NONEED_REPORTLOST;
			} else if ((pfncfg.reporttype && pfn_info->reporttype) ||
			           ((pfncfg.reporttype == WL_PFN_REPORT_ALLNET) &&
			            (pfn_info->reporttype == WL_PFN_REPORT_SSIDNET) &&
			            (pfn_info->reportloss & BSSID_RPTLOSS_MASK)) ||
			           ((pfncfg.reporttype == WL_PFN_REPORT_ALLNET) &&
			            (pfn_info->reporttype == WL_PFN_REPORT_BSSIDNET) &&
			            (pfn_info->reportloss & SSID_RPTLOSS_MASK)) ||
			           ((pfn_info->reporttype == WL_PFN_REPORT_ALLNET) &&
			            (cur_bsscnt > pfn_info->availbsscnt))) {
				pfn_info->none_cnt = 0;
			}
			pfn_info->reporttype = pfncfg.reporttype;
		}

		/* setup ssid scan list */
		if ((err = wl_pfn_setscanlist(pfn_info, TRUE)))
			return err;

		/* restart scanning, kicking off a scan immediately */
		pfn_info->pfn_scan_state = PFN_SCAN_IDLE;
		wl_pfn_timer(pfn_info);
	} else {
		pfn_info->reporttype = pfncfg.reporttype;
	}

	return 0;
}

#ifdef GSCAN
static int
wl_pfn_cfg_gscan(wl_pfn_info_t * pfn_info, void * buf, int len)
{
	int err = 0;
	int i;
	bool valid = FALSE;
	wl_pfn_gscan_cfg_t *pcfg = (wl_pfn_gscan_cfg_t *)buf;
	wl_pfn_gscan_params_t *pgscan_params = pfn_info->pfn_gscan_cfg_param;
	wl_pfn_gscan_channel_bucket_t *channel_bucket = pcfg->channel_bucket;

	pgscan_params->gscan_flags =
	      pcfg->flags & GSCAN_SEND_ALL_RESULTS_MASK;

	if (pcfg->flags & GSCAN_CFG_FLAGS_ONLY_MASK) {
		WL_PFN(("wl%d: wl_pfn_cfg_gscan - Only flags set\n",
		 pfn_info->wlc->pub->unit));
		return err;
	}

	if (len != GSCAN_CFG_SIZE(pcfg->count_of_channel_buckets)) {
		WL_ERROR(("wl%d: Bad params, size doesnt match expected\n",
		 pfn_info->wlc->pub->unit));
		return BCME_BADARG;
	}

	if (PFN_SCAN_DISABLED != pfn_info->pfn_scan_state) {
		WL_ERROR(("wl%d: Config change only when disabled\n",
		 pfn_info->wlc->pub->unit));
		return BCME_EPERM;
	}

	/* End index must not exceed the number of channels    */
	for (i = 0; i < pcfg->count_of_channel_buckets; i++) {
		if (channel_bucket[i].bucket_freq_multiple ==  1)
			valid = TRUE;
		if (channel_bucket[i].bucket_end_index >= pfn_info->chanspec_count)
			return BCME_BADARG;
	}

	/* Need to have at least one channel bucket with base multiple 1 */
	if (!valid)
		return BCME_BADARG;

	if (pfn_info->pfn_gscan_cfg_param->channel_bucket) {
		MFREE(pfn_info->wlc->osh, pfn_info->pfn_gscan_cfg_param->channel_bucket,
		    (sizeof(wl_pfn_gscan_channel_bucket_t) *
		    pfn_info->pfn_gscan_cfg_param->count_of_channel_buckets));
	}

	pgscan_params->buffer_threshold = pcfg->buffer_threshold;
	pgscan_params->swc_nbssid_threshold = pcfg->swc_nbssid_threshold;
	pgscan_params->swc_rssi_window_size = pcfg->swc_rssi_window_size;
	pgscan_params->count_of_channel_buckets = (uint8) pcfg->count_of_channel_buckets;
	pgscan_params->lost_ap_window = (uint8) pcfg->lost_ap_window;
	pgscan_params->num_scan = 0;

	pgscan_params->channel_bucket = MALLOC(pfn_info->wlc->osh,
	                               (sizeof(wl_pfn_gscan_channel_bucket_t) *
	                               pgscan_params->count_of_channel_buckets));

	if (!pgscan_params->channel_bucket) {
		WL_ERROR(("wl%d: malloc failed, size = %d\n",
		   pfn_info->wlc->pub->unit,
		   (sizeof(wl_pfn_gscan_channel_bucket_t) *
		    pgscan_params->count_of_channel_buckets)));
		return BCME_NOMEM;
	}

	memcpy(pgscan_params->channel_bucket, pcfg->channel_bucket,
	     (sizeof(*pgscan_params->channel_bucket) * pgscan_params->count_of_channel_buckets));

	if (pfn_info->pfn_gscan_cfg_param->swc_rssi_window_size < MIN_NUM_VALID_RSSI)
		pfn_info->pfn_gscan_cfg_param->swc_rssi_window_size = MIN_NUM_VALID_RSSI;

	if (pfn_info->pfn_gscan_cfg_param->swc_rssi_window_size > PFN_SWC_RSSI_WINDOW_MAX)
		pfn_info->pfn_gscan_cfg_param->swc_rssi_window_size = PFN_SWC_RSSI_WINDOW_MAX;

	return err;
}
#endif /* GSCAN */

static int
wl_pfn_suspend(wl_pfn_info_t *pfn_info, int suspend)
{
	if (suspend) {
		pfn_info->intflag |= ENABLE << SUSPEND_BIT;
		/* Abort any pending PFN scan */
		if (pfn_info->pfn_scan_state == PFN_SCAN_INPROGRESS)
			wlc_scan_abort(pfn_info->wlc->scan, WLC_E_STATUS_SUPPRESS);
	} else {
		if (!(pfn_info->intflag & (ENABLE << SUSPEND_BIT)))
			return BCME_OK;

		pfn_info->intflag &= ~(ENABLE << SUSPEND_BIT);
		if (PFN_SCAN_DISABLED == pfn_info->pfn_scan_state)
			return BCME_OK;

		/* kick off scan immediately */
		pfn_info->pfn_scan_state = PFN_SCAN_IDLE;
		wl_pfn_timer(pfn_info);
	}

	return BCME_OK;
}

#ifdef GSCAN
static int wl_pfn_calc_num_bestn(wl_pfn_info_t * pfn_info)
{
	wl_pfn_bestnet_t *ptr = pfn_info->bestnethdr;
	wl_pfn_bestinfo_t *bestnetinfo;
	int count = 0;
	int i;

	while (ptr) {
		bestnetinfo = ptr->bestnetinfo;
		for (i = 0; i < pfn_info->param->bestn; i++) {
			if (bestnetinfo->pfnsubnet.SSID_len ||
			   !ETHER_ISNULLADDR(&bestnetinfo->pfnsubnet.BSSID)) {
				count++;
			}
			bestnetinfo++;
		}
		ptr = ptr->next;
	}

	return count;
}
#endif /* GSCAN */

static int
wl_pfn_best(wl_pfn_info_t * pfn_info, void * buf, int len)
{
	wl_pfn_scanresults_t *pfnbest;
	wl_pfn_net_info_t *pfnbestnet;
	wl_pfn_bestinfo_t *bestnetinfo;
	wl_pfn_bestnet_t *ptr;
	uint32 now = OSL_SYSUPTIME();
	int i, buflen = len;
	int scancnt, cnt;

	if (PFN_VERSION != pfn_info->param->version)
		return BCME_EPERM;

	/* Make sure PFN is not in-progress */
	if (PFN_SCAN_IDLE != pfn_info->pfn_scan_state) {
		WL_ERROR(("wl%d: report pfnbest only during PFN_SCAN_IDLE\n",
			pfn_info->wlc->pub->unit));
		return BCME_EPERM;
	}

	/* make sure alignment of 4 */
	ASSERT(!((uintptr)buf & 0x3));

	pfnbest = (wl_pfn_scanresults_t *)buf;
	pfnbest->version = PFN_SCANRESULT_VERSION;
	pfnbest->count = 0;
	pfnbestnet = pfnbest->netinfo;
	/* pre-calculate until which scan result this report is going to end */
	buflen -= sizeof(wl_pfn_scanresults_t) - sizeof(wl_pfn_net_info_t);
	scancnt = buflen / (sizeof(wl_pfn_net_info_t) * pfn_info->param->bestn);
	cnt = 0;
	while (cnt < scancnt && pfn_info->bestnethdr) {
		bestnetinfo = pfn_info->bestnethdr->bestnetinfo;
		/* get the bestn from one scan */
		for (i = 0; i < pfn_info->param->bestn; i++) {
			if (bestnetinfo->pfnsubnet.SSID_len ||
				!ETHER_ISNULLADDR(&bestnetinfo->pfnsubnet.BSSID)) {
				bcopy(bestnetinfo, pfnbestnet, sizeof(wl_pfn_subnet_info_t));
				pfnbestnet->RSSI = bestnetinfo->RSSI;
				/* elapsed time, msecond to second */
				pfnbestnet->timestamp = (uint16)((now - bestnetinfo->timestamp)
				                        / 1000);
				pfnbest->count++;
				pfnbestnet++;
				buflen -= sizeof(wl_pfn_net_info_t);
				ASSERT(buflen >= sizeof(wl_pfn_net_info_t));
			}
			bestnetinfo++;
		}
		ptr = pfn_info->bestnethdr;
		pfn_info->bestnethdr = pfn_info->bestnethdr->next;
		MFREE(pfn_info->wlc->osh, ptr, pfn_info->bestnetsize);
		ASSERT(pfn_info->numofscan);
		pfn_info->numofscan--;
		cnt++;
	}
	ASSERT(buflen >= 0);
	if (!pfn_info->bestnethdr) {
		pfnbest->status = PFN_COMPLETE;
		pfn_info->bestnettail = NULL;
	} else {
		pfnbest->status = PFN_INCOMPLETE;
	}
	return 0;
}

static int
wl_pfn_lbest(wl_pfn_info_t * pfn_info, void * buf, int len)
{
	wl_pfn_lscanresults_t *pfnbest;
	wl_pfn_lnet_info_t *pfnbestnet;
	wl_pfn_bestinfo_t *bestnetinfo;
	wl_pfn_bestnet_t *ptr;
	uint32 now = OSL_SYSUPTIME();
	int i, buflen = len;
	int scancnt, cnt;
	bool partial = FALSE;

	if (PFN_VERSION != pfn_info->param->version)
		return BCME_EPERM;

	/* Make sure PFN is not in-progress */
	if (PFN_SCAN_IDLE != pfn_info->pfn_scan_state) {
		WL_ERROR(("wl%d: report pfnbest only during PFN_SCAN_IDLE\n",
			pfn_info->wlc->pub->unit));
		return BCME_EPERM;
	}

	/* make sure alignment of 4 */
	ASSERT(!((uintptr)buf & 0x3));

	pfnbest = (wl_pfn_lscanresults_t *)buf;
	pfnbest->version = PFN_SCANRESULT_VERSION;
	pfnbest->count = 0;
	pfnbestnet = pfnbest->netinfo;
	/* pre-calculate until which scan result this report is going to end */
	buflen -= sizeof(wl_pfn_lscanresults_t) - sizeof(wl_pfn_lnet_info_t);
	scancnt = buflen / (sizeof(wl_pfn_lnet_info_t) * pfn_info->param->bestn);
	cnt = 0;
	while (cnt < scancnt && pfn_info->bestnethdr) {
		bestnetinfo = pfn_info->bestnethdr->bestnetinfo;
		if (bestnetinfo->flags & PFN_PARTIAL_SCAN_MASK)
			partial = TRUE;
		else
			partial = FALSE;
		/* get the bestn from one scan */
		for (i = 0; i < pfn_info->param->bestn; i++) {
			if (bestnetinfo->pfnsubnet.SSID_len ||
				!ETHER_ISNULLADDR(&bestnetinfo->pfnsubnet.BSSID)) {
				bcopy(bestnetinfo, pfnbestnet, sizeof(wl_pfn_subnet_info_t));
				pfnbestnet->RSSI = bestnetinfo->RSSI;
				if (partial == TRUE)
					pfnbestnet->flags |= 1 << PFN_PARTIAL_SCAN_BIT;
				else
					pfnbestnet->flags &= ~PFN_PARTIAL_SCAN_MASK;
				pfnbestnet->timestamp = (uint32)(now - bestnetinfo->timestamp);
				pfnbestnet->rtt0 = 0;
				pfnbestnet->rtt1 = 0;
				pfnbest->count++;
				pfnbestnet++;
				buflen -= sizeof(wl_pfn_lnet_info_t);
				ASSERT(buflen >= sizeof(wl_pfn_lnet_info_t));
			}
			bestnetinfo++;
		}
		ptr = pfn_info->bestnethdr;
		pfn_info->bestnethdr = pfn_info->bestnethdr->next;
		MFREE(pfn_info->wlc->osh, ptr, pfn_info->bestnetsize);
		ASSERT(pfn_info->numofscan);
		pfn_info->numofscan--;
		cnt++;
	}
	ASSERT(buflen >= 0);
	if (!pfn_info->bestnethdr) {
		pfnbest->status = PFN_COMPLETE;
		pfn_info->bestnettail = NULL;
	} else {
		pfnbest->status = PFN_INCOMPLETE;
	}
	return 0;
}

static int
wl_pfn_best_bssid(wl_pfn_info_t * pfn_info, void * buf, int len)
{
	wl_pfn_scanhist_bssid_t *pfnbest;
	wl_pfn_net_info_bssid_t *pfnbestnet;
	wl_pfn_bestinfo_bssid_t *bestnetinfo;
	wl_pfn_bestnet_b_t *ptr;
	uint32 now = OSL_SYSUPTIME();
	int i, buflen = len;
	int scancnt, cnt;

	if (PFN_VERSION != pfn_info->param->version)
		return BCME_EPERM;

	/* Make sure PFN is not in-progress */
	if (PFN_SCAN_IDLE != pfn_info->pfn_scan_state) {
		WL_ERROR(("wl%d: report pfnbest only during PFN_SCAN_IDLE\n",
			pfn_info->wlc->pub->unit));
		return BCME_EPERM;
	}

	/* make sure alignment of 4 */
	ASSERT(!((uintptr)buf & 0x3));

	pfnbest = (wl_pfn_scanhist_bssid_t *)buf;
	pfnbest->version = PFN_SCANRESULT_VERSION;
	pfnbest->count = 0;
	pfnbestnet = pfnbest->netinfo;
	/* pre-calculate until which scan result this report is going to end */
	buflen -= sizeof(wl_pfn_scanhist_bssid_t) - sizeof(wl_pfn_net_info_bssid_t);
	scancnt = buflen / (sizeof(wl_pfn_net_info_bssid_t) * pfn_info->param->bestn);
	cnt = 0;
	while (cnt < scancnt && pfn_info->bestnethdr) {
		bestnetinfo = ((wl_pfn_bestnet_b_t*)pfn_info->bestnethdr)->bestnetinfo_b;
		/* get the bestn from one scan */
		for (i = 0; i < pfn_info->param->bestn; i++) {
			if (!ETHER_ISNULLADDR(&bestnetinfo->BSSID)) {
				bcopy(&bestnetinfo->BSSID, &pfnbestnet->BSSID, ETHER_ADDR_LEN);
				pfnbestnet->channel = bestnetinfo->channel;
				pfnbestnet->RSSI = (int8)bestnetinfo->RSSI;
				/* elapsed time, msecond to second */
				pfnbestnet->timestamp = (uint16)((now - bestnetinfo->timestamp)
				                        / 1000);
				pfnbestnet->flags = 0;
				pfnbest->count++;
				pfnbestnet++;
				buflen -= sizeof(wl_pfn_net_info_bssid_t);
				ASSERT(buflen >= sizeof(wl_pfn_net_info_bssid_t));
			}
			bestnetinfo++;
		}
		ptr = (wl_pfn_bestnet_b_t*)pfn_info->bestnethdr;
		pfn_info->bestnethdr = pfn_info->bestnethdr->next;
		MFREE(pfn_info->wlc->osh, ptr, pfn_info->bestnetsize);
		ASSERT(pfn_info->numofscan);
		pfn_info->numofscan--;
		cnt++;
	}
	ASSERT(buflen >= 0);
	if (!pfn_info->bestnethdr) {
		pfnbest->status = PFN_COMPLETE;
		pfn_info->bestnettail = NULL;
	} else {
		pfnbest->status = PFN_INCOMPLETE;
	}
	return 0;
}

static void
wl_pfn_free_ssidlist(wl_pfn_info_t * pfn_info, bool freeall, bool clearjustfoundonly)
{
	int i;
	for (i = 0; i < pfn_info->count; i++) {
		if (freeall) {
			MFREE(pfn_info->wlc->osh, pfn_info->pfn_internal[i],
			      sizeof(wl_pfn_internal_t));
			pfn_info->pfn_internal[i] = NULL;
		} else {
			if (!clearjustfoundonly ||
			    (pfn_info->pfn_internal[i]->network_found ==
			    PFN_NET_JUST_FOUND)) {
				if (!(pfn_info->pfn_internal[i]->pfn.flags &
				      WL_PFN_SUPPRESSLOST_MASK) &&
				    (pfn_info->pfn_internal[i]->network_found &
				    (PFN_NET_JUST_FOUND | PFN_NET_ALREADY_FOUND))) {
					ASSERT(pfn_info->availbsscnt);
					pfn_info->availbsscnt--;
				}
				pfn_info->pfn_internal[i]->network_found = PFN_NET_NOT_FOUND;
				bzero(&pfn_info->pfn_internal[i]->bssid, ETHER_ADDR_LEN);
			}
		}
	}
	if (freeall)
		pfn_info->count = 0;
}

static void
wl_pfn_free_bssidlist(wl_pfn_info_t * pfn_info, bool freeall, bool clearjustfoundonly)
{
	int i;
	wl_pfn_bssid_list_t *bssid_list, *ptr;

	for (i = 0; i < BSSIDLIST_MAX; i++) {
		bssid_list = pfn_info->bssid_list[i];
		while (bssid_list) {
			if (bssid_list->pbssidinfo) {
				if (!clearjustfoundonly ||
				    (bssid_list->pbssidinfo->network_found ==
				      PFN_NET_JUST_FOUND)) {
					MFREE(pfn_info->wlc->osh, bssid_list->pbssidinfo,
					      sizeof(wl_pfn_bssidinfo_t));
					bssid_list->pbssidinfo = NULL;
					if (!(bssid_list->flags & WL_PFN_SUPPRESSLOST_MASK)) {
						ASSERT(pfn_info->availbsscnt);
						pfn_info->availbsscnt--;
					}
				}
			}
			ptr = bssid_list;
			bssid_list = bssid_list->next;
			if (freeall)
				MFREE(pfn_info->wlc->osh, ptr, sizeof(wl_pfn_bssid_list_t));
		}
		if (freeall)
			pfn_info->bssid_list[i] = NULL;
	}
	if (freeall)
		pfn_info->bssidNum = 0;
}

static void
wl_pfn_free_bestnet(wl_pfn_info_t * pfn_info)
{
	wl_pfn_bestnet_t *bestnet;

	/* clear bestnet */
	while (pfn_info->bestnethdr != pfn_info->bestnettail) {
		bestnet = pfn_info->bestnethdr;
		pfn_info->bestnethdr = pfn_info->bestnethdr->next;
		MFREE(pfn_info->wlc->osh, bestnet, pfn_info->bestnetsize);
	}
	if (pfn_info->bestnethdr) {
		MFREE(pfn_info->wlc->osh, pfn_info->bestnethdr, pfn_info->bestnetsize);
		pfn_info->bestnethdr = pfn_info->bestnettail = NULL;
	}
	pfn_info->numofscan = 0;
	/* clear current bestn */
	if (pfn_info->current_bestn) {
		MFREE(pfn_info->wlc->osh, pfn_info->current_bestn, pfn_info->bestnetsize);
		pfn_info->current_bestn = NULL;
	}
}

static int
wl_pfn_setscanlist(wl_pfn_info_t *pfn_info, int enable)
{
	int i, j;
	uint8 directcnt;

	if (enable) {
		/* Set up maximal list (all hidden SSIDs plus bcast) in advance */
		ASSERT(!pfn_info->ssid_list);

		directcnt = pfn_info->hiddencnt;
		pfn_info->ssidlist_len = (directcnt + 1) * sizeof(wlc_ssid_t);

		if (!(pfn_info->ssid_list = MALLOCZ(pfn_info->wlc->osh, pfn_info->ssidlist_len))) {
			WL_ERROR(("wl%d: ssid list allocation failed. %d bytes\n",
				pfn_info->wlc->pub->unit, (directcnt + 1) * sizeof(wlc_ssid_t)));
			pfn_info->ssidlist_len = 0;
			if (pfn_info->numofscan)
				wl_pfn_send_event(pfn_info, NULL, 0, WLC_E_PFN_BEST_BATCHING);
			return BCME_NOMEM;
		}

		/* copy hidden ssid to ssid list(s) */
		for (i = j = 0; i < pfn_info->count && j < directcnt; i++) {
			if (pfn_info->pfn_internal[i]->pfn.flags & WL_PFN_HIDDEN_MASK) {
				bcopy(&pfn_info->pfn_internal[i]->pfn.ssid,
					&pfn_info->ssid_list[j++], sizeof(wlc_ssid_t));
			}
		}
	} else {
		/* Free ssid list for directed scan */
		if (pfn_info->ssid_list != NULL) {
			MFREE(pfn_info->wlc->osh, pfn_info->ssid_list,
			      pfn_info->ssidlist_len);
			pfn_info->ssid_list = NULL;
		}
	}

	return 0;
}

static int
wl_pfn_enable(wl_pfn_info_t * pfn_info, int enable)
{
	int err;
	uint groupid;

	/* If the request to enable make sure we have required data from user/host */
	if (enable && (PFN_SCAN_DISABLED == pfn_info->pfn_scan_state)) {

		/* Check to see if the user has provided correct set of parameters by checking
		* version field. Also 'pfn_info->count or pfn_info->bssidNum' being zero is
		* vaild parameter, if user wants to just turn on back ground scanning or
		* broadcast scan without PFN list
		*/
		if (!pfn_info->param->version ||
		    (!pfn_info->count && !pfn_info->bssidNum &&
		!(pfn_info->param->flags & (ENABLE_BD_SCAN_MASK | ENABLE_BKGRD_SCAN_MASK)))) {
			WL_ERROR(("wl%d: Incomplete parameter setting\n",
			           pfn_info->wlc->pub->unit));
			return BCME_BADOPTION;
		}
		/* current_bestn expected NULL */
		if (pfn_info->current_bestn)
			WL_ERROR(("current_bestn not empty!\n"));

		if ((err = wl_pfn_setscanlist(pfn_info, enable)))
			return err;

		if ((pfn_info->reporttype == WL_PFN_REPORT_ALLNET &&
			!pfn_info->reportloss) ||
			((pfn_info->reporttype == WL_PFN_REPORT_BSSIDNET) &&
			!(pfn_info->reportloss & BSSID_RPTLOSS_MASK)) ||
			((pfn_info->reporttype == WL_PFN_REPORT_SSIDNET) &&
			!(pfn_info->reportloss & SSID_RPTLOSS_MASK))) {
			pfn_info->none_cnt = NONEED_REPORTLOST;
		} else {
			pfn_info->none_cnt = 0;
		}

		pfn_info->pfn_scan_state = PFN_SCAN_IDLE;

		/* XXX JAS: Shouldn't we have been clearing adaptcnt here?  Else
		 * it seems possible that once we reach maximum backoff in a silent
		 * environment, a disable/enable would stay at fast scanning w/o
		 * any backoff, but with a large network timeout...
		 *
		 * Clearing it with the modified timer mechanism here, which also
		 * means resetting the values here.
		 */
		pfn_info->adaptcnt = 0;
		pfn_info->currentadapt = 0;
		pfn_info->a_scanfreq = pfn_info->param->scan_freq;
		pfn_info->a_losttime = pfn_info->param->lost_network_timeout;

		if (MPF_ENAB(pfn_info->wlc->pub)) {
			/* Reset the active scan frequency for each group too */
			for (groupid = 0; groupid < pfn_info->ngroups; groupid++) {
				wl_pfn_group_info_t *group;
				uint16 state = pfn_info->mpf_state;
				if ((group = pfn_info->groups[groupid])) {
					group->gp_scanfreq =
					        group->mpf_states[state].scan_freq;
					group->gp_losttime =
					        group->mpf_states[state].lost_network_timeout;
					group->gp_adaptcnt = 0;
					group->gp_curradapt = 0;

					/* Allow override of all active parameters */
					if (pfn_info->ovparam.duration &&
					    !pfn_info->ovparam.start_offset) {
						group->gp_scanfreq =
						        pfn_info->ovparam.override.scan_freq;
						group->gp_losttime =
						        pfn_info->ovparam.
						        override.lost_network_timeout;
					}
				}
			}
		}
		pfn_info->activegroups = 0;

		/* If immediate scan requested, kick it off; else
		 * just schedule the next one starting from now.
		 */
		if (((pfn_info->param->flags & IMMEDIATE_SCAN_MASK) >>
			IMMEDIATE_SCAN_BIT) == ENABLE) {
			wl_pfn_timer(pfn_info);
		} else {
			wl_pfn_start_timer(pfn_info, TRUE);
		}
	}

	/* Handle request to disable */
	if (!enable && (PFN_SCAN_DISABLED != pfn_info->pfn_scan_state)) {
		/* Stop the scanning timer */
		wl_del_timer(pfn_info->wlc->wl, pfn_info->p_pfn_timer);
		pfn_info->timer_active = FALSE;

		/* Abort any pending PFN scan */
		if (pfn_info->pfn_scan_state == PFN_SCAN_INPROGRESS) {
			wlc_scan_abort(pfn_info->wlc->scan, WLC_E_STATUS_ABORT);
		}
		pfn_info->pfn_scan_state = PFN_SCAN_DISABLED;

		if ((err = wl_pfn_setscanlist(pfn_info, enable)))
			return err;

		/* free all bestnet related info */
		wl_pfn_free_bestnet(pfn_info);
		/* Reset all the found/lost networks */
		/* so that events are sent to host after pfn enable */
		wl_pfn_free_ssidlist(pfn_info, FALSE, FALSE);
		wl_pfn_free_bssidlist(pfn_info, FALSE, FALSE);
		wl_pfn_free_bdcast_list(pfn_info);
#ifdef GSCAN
		wl_pfn_free_swc_bssid_list(pfn_info);
		pfn_info->bestn_event_sent = FALSE;
		pfn_info->pfn_gscan_cfg_param->num_scan = 0;
#endif /* GSCAN */
		/* reset none_cnt, available network count, found count, lost count */
		pfn_info->none_cnt = 0;
		pfn_info->availbsscnt = 0;
		pfn_info->foundcnt = 0;
		pfn_info->lostcnt = 0;
		pfn_info->foundcnt_ssid = 0;
		pfn_info->foundcnt_bssid = 0;
		pfn_info->lostcnt_ssid = 0;
		pfn_info->lostcnt_bssid = 0;
	}

	return BCME_OK;
}

static int
wl_pfn_clear(wl_pfn_info_t * pfn_info)
{
	int i;

	/* Make sure PFN is not active already */
	if (PFN_SCAN_DISABLED != pfn_info->pfn_scan_state) {
		WL_ERROR(("wl%d: PFN is already active, can't service IOV_PFN_CLEAR\n",
			pfn_info->wlc->pub->unit));
		return BCME_EPERM;
	}

	/* Free resources allocated during IOV_PFN_ADD */
	wl_pfn_free_ssidlist(pfn_info, TRUE, FALSE);
	wl_pfn_free_bssidlist(pfn_info, TRUE, FALSE);

	/* Free the link list of BSS for given ESS from PFN list */
	wl_pfn_free_bdcast_list(pfn_info);

	/* free all bestnet related info */
	wl_pfn_free_bestnet(pfn_info);

#ifdef GSCAN
	/* Free significant bssid list */
	wl_pfn_free_swc_bssid_list(pfn_info);

	if (pfn_info->pfn_gscan_cfg_param->chan_orig_list) {
		MFREE(pfn_info->wlc->osh,
		pfn_info->pfn_gscan_cfg_param->chan_orig_list,
		((sizeof(chanspec_t) * pfn_info->chanspec_count)));
	}
	if (pfn_info->pfn_gscan_cfg_param->chan_scanned) {
		MFREE(pfn_info->wlc->osh,
		pfn_info->pfn_gscan_cfg_param->chan_scanned,
		((sizeof(chanspec_t) * pfn_info->chanspec_count)));
	}

	if (pfn_info->pfn_gscan_cfg_param->channel_bucket) {
		MFREE(pfn_info->wlc->osh, pfn_info->pfn_gscan_cfg_param->channel_bucket,
		 (sizeof(wl_pfn_gscan_channel_bucket_t) *
		 pfn_info->pfn_gscan_cfg_param->count_of_channel_buckets));
	}
	memset(pfn_info->pfn_gscan_cfg_param, 0, sizeof(*pfn_info->pfn_gscan_cfg_param));
	pfn_info->bestn_event_sent = FALSE;
#endif /* GSCAN */
	/* Zero out the param */
	bzero(pfn_info->param, sizeof(wl_pfn_param_t));

	pfn_info->associated_idx = 0;
	/* Disable before calling 'wl_pfn_clear' */
	pfn_info->pfn_scan_state = PFN_SCAN_DISABLED;

	pfn_info->numofscan = 0;
	pfn_info->adaptcnt = 0;
	pfn_info->activegroups = 0;
	pfn_info->foundcnt = pfn_info->lostcnt = 0;
	pfn_info->foundcnt_ssid = pfn_info->lostcnt_ssid = 0;
	pfn_info->foundcnt_bssid = pfn_info->lostcnt_bssid = 0;
	/* clear channel spec list */
	for (i = 0; i < WL_NUMCHANNELS; i++) {
		pfn_info->chanspec_list [i] = 0;
	}
	pfn_info->chanspec_count = 0;
	pfn_info->bestnetsize = 0;
	pfn_info->hiddencnt = 0;
	pfn_info->currentadapt = 0;
	pfn_info->reporttype = 0;
	pfn_info->availbsscnt = 0;
	pfn_info->none_cnt = 0;
	pfn_info->reportloss = 0;

	/* Cancel any temporary override */
	wl_pfn_override_cancel(pfn_info);

	/* Clear the pfn_mac */
	memset(&pfn_info->pfn_mac, 0, ETHER_ADDR_LEN);
	pfn_info->intflag &= ~(ENABLE << PFN_MACADDR_BIT);
	pfn_info->intflag &= ~(ENABLE << PFN_ONLY_MAC_OUI_SET_BIT);
	pfn_info->intflag &= ~(ENABLE << PFN_MACADDR_UNASSOC_ONLY_BIT);

	return BCME_OK;
}

static void
wl_pfn_free_bdcast_list(wl_pfn_info_t * pfn_info)
{
	struct wl_pfn_bdcast_list *bdcast_node;

	/* Free the entire linked list */
	while (pfn_info->bdcast_list) {
		bdcast_node = pfn_info->bdcast_list;
		pfn_info->bdcast_list = pfn_info->bdcast_list->next;
		MFREE(pfn_info->wlc->osh, bdcast_node, sizeof(struct wl_pfn_bdcast_list));
	}
	pfn_info->bdcastnum = 0;
}

static void
wl_pfn_cipher2wsec(uint8 ucount, uint8 * unicast, uint32 * wsec)
{
	int i;

	for (i = 0; i < ucount; i++) {
		switch (unicast[i]) {
		case WPA_CIPHER_WEP_40:
		case WPA_CIPHER_WEP_104:
			*wsec |= WEP_ENABLED;
			break;
		case WPA_CIPHER_TKIP:
			*wsec |= TKIP_ENABLED;
			break;
		case WPA_CIPHER_AES_OCB:
		case WPA_CIPHER_AES_CCM:
			*wsec |= AES_ENABLED;
			break;
		}
	}
	return;
}

static int
wl_pfn_sendonenet(wl_pfn_info_t *pfn_info, wlc_bss_info_t * bi, uint32 now)
{
	wlc_info_t *wlc = pfn_info->wlc;
	wl_pfn_scanresult_t *result;
	uint evt_data_size = sizeof(wl_pfn_scanresult_t);

	if (bi->bcn_prb_len > DOT11_BCN_PRB_LEN) {
		evt_data_size += ROUNDUP(bi->bcn_prb_len - DOT11_BCN_PRB_LEN, 4);
	}

	result = (wl_pfn_scanresult_t *)MALLOCZ(wlc->osh,
		evt_data_size);
	if (!result) {
		WL_INFORM(("wl%d: found result allcation failed\n",
		            wlc->pub->unit));
		return BCME_NOMEM;
	}

	result->version = PFN_SCANRESULT_VERSION;
	result->status = PFN_COMPLETE;
	result->count = 1;
	bcopy(bi->BSSID.octet, result->netinfo.pfnsubnet.BSSID.octet, ETHER_ADDR_LEN);
	result->netinfo.pfnsubnet.channel = CHSPEC_CHANNEL(bi->chanspec);
	result->netinfo.pfnsubnet.SSID_len = bi->SSID_len;
	bcopy(bi->SSID, result->netinfo.pfnsubnet.SSID, bi->SSID_len);
	result->netinfo.RSSI = bi->RSSI;
	/* timestamp is only meaningful when reporting best network info,
	   but not useful when reporting netfound
	*/
	result->netinfo.timestamp = 0;

	/* now put the bss_info in */
	wlc_bss2wl_bss(wlc, bi, &result->bss_info, evt_data_size, TRUE);

	if (BCME_OK != wl_pfn_send_event(pfn_info, (void *)result,
		evt_data_size, WLC_E_PFN_NET_FOUND)) {
		WL_ERROR(("wl%d: send event fails\n",
		          pfn_info->wlc->pub->unit));
		MFREE(pfn_info->wlc->osh, result, evt_data_size);
		return BCME_ERROR;
	}

	return BCME_OK;
}

void
wl_pfn_process_scan_result(wl_pfn_info_t* pfn_info, wlc_bss_info_t * bi)
{
	int k;
	uint8 *ssid;
	uint8 ssid_len;
	wl_pfn_internal_t *pfn_internal;
	wl_pfn_bdcast_list_t *bdcast_node_list;
	uint32	now;
	uint scanresult_wpa_auth = WPA_AUTH_DISABLED;
	uint32 scanresult_wsec = 0;
	struct ether_addr *bssid;
	wl_pfn_bssid_list_t *bssidlist = NULL;
	int16 oldrssi, rssidiff;
	uint8 index, chan;
	uint8 oldfound;
	int retval;
#ifdef BCMDBG
	char ssidbuf[SSID_FMT_BUF_LEN];
#endif /* BCMDBG */

	ASSERT(pfn_info);

	now = OSL_SYSUPTIME();

	ssid = bi->SSID;
	ssid_len = bi->SSID_len;
	bssid = &bi->BSSID;
	chan = CHSPEC_CHANNEL(bi->chanspec);

	WL_TRACE(("wl%d: Scan result: RSSI = %d, SSID = %s\n",
		pfn_info->wlc->pub->unit, bi->RSSI,
		(wlc_format_ssid(ssidbuf, ssid, ssid_len), ssidbuf)));

	/* see if this AP is one of the bestn */
	/* best network is not limited to on channel */
	if (pfn_info->current_bestn) {
		if (pfn_info->param->flags & BESTN_BSSID_ONLY_MASK) {
			wl_pfn_bestinfo_bssid_t *bestptr;
			bestptr = ((wl_pfn_bestnet_b_t*)pfn_info->current_bestn)->bestnetinfo_b;
			bestptr = wl_pfn_get_best_networks_b(pfn_info, bestptr,
			                                       bi->RSSI, bi->BSSID);
			if (bestptr) {
				bcopy(bi->BSSID.octet, bestptr->BSSID.octet, ETHER_ADDR_LEN);
				bestptr->channel = chan;
				bestptr->RSSI = bi->RSSI;
				bestptr->timestamp = now;
			}
		} else {
			wl_pfn_bestinfo_t *bestptr;
			bestptr = pfn_info->current_bestn->bestnetinfo;
			bestptr = wl_pfn_get_best_networks(pfn_info, bestptr,
			                                   bi->RSSI, bi->BSSID);
			if (bestptr) {
				bcopy(bi->BSSID.octet, bestptr->pfnsubnet.BSSID.octet,
				      ETHER_ADDR_LEN);
				bestptr->pfnsubnet.SSID_len = ssid_len;
				bcopy(ssid, bestptr->pfnsubnet.SSID, bestptr->pfnsubnet.SSID_len);
				bestptr->pfnsubnet.channel = chan;
				bestptr->RSSI = bi->RSSI;
				bestptr->timestamp = now;
			}
		}
	}
#ifdef GSCAN
	/* If a significant change BSSID list is configured process and update */
	wl_pfn_process_swc_bssid(pfn_info, bi);
#endif /* GSCAN */
	/* check BSSID list in pfn_info and see if any match there */
	oldfound = pfn_info->foundcnt;
	if (((pfn_info->reporttype == WL_PFN_REPORT_BSSIDNET) ||
	     (pfn_info->reporttype == WL_PFN_REPORT_ALLNET)) &&
	    isset(&pfn_info->scangroups, WL_PFN_MPF_GROUP_BSSID)) {
		index = bssid->octet[ETHER_ADDR_LEN -1] & 0xf;
		bssidlist = pfn_info->bssid_list[index];
		while (bssidlist)
		{
			int8 rssi = (bssidlist->flags & WL_PFN_RSSI_MASK)
				>> WL_PFN_RSSI_SHIFT;

			if (!bcmp(bssidlist->bssid.octet, bssid->octet, ETHER_ADDR_LEN) &&
				(rssi == 0 || bi->RSSI >= rssi)) {
				if (!bssidlist->pbssidinfo) {
				     bssidlist->pbssidinfo =
				        (wl_pfn_bssidinfo_t *)MALLOC(pfn_info->wlc->osh,
				                              sizeof(wl_pfn_bssidinfo_t));
					if (!bssidlist->pbssidinfo) {
						WL_ERROR(("wl%d: pbssidinfo MALLOC failed\n",
						          pfn_info->wlc->pub->unit));
						if (pfn_info->numofscan)
							wl_pfn_send_event(pfn_info, NULL, 0,
							                  WLC_E_PFN_BEST_BATCHING);
						break;
					}
					bssidlist->pbssidinfo->ssid.SSID_len = ssid_len;
					bcopy(ssid, bssidlist->pbssidinfo->ssid.SSID, ssid_len);
					bssidlist->pbssidinfo->rssi = bi->RSSI;
					if (!(bssidlist->flags &
					 (WL_PFN_SUPPRESSFOUND_MASK | WL_PFN_SUPPRESSLOST_MASK))) {
						bssidlist->pbssidinfo->network_found =
						                               PFN_NET_JUST_FOUND;
						pfn_info->foundcnt++;
						pfn_info->foundcnt_bssid++;
						pfn_info->availbsscnt++;
					} else {
						if (!(bssidlist->flags &
						      WL_PFN_SUPPRESSFOUND_MASK)) {
							bssidlist->pbssidinfo->network_found =
							                       PFN_NET_JUST_FOUND;
							pfn_info->foundcnt++;
							pfn_info->foundcnt_bssid++;
						} else {
							bssidlist->pbssidinfo->network_found =
							                    PFN_NET_ALREADY_FOUND;
							if (!(bssidlist->flags &
							      WL_PFN_SUPPRESSLOST_MASK))
								pfn_info->availbsscnt++;
						}
					}
				} else if (bssidlist->pbssidinfo &&
				           bssidlist->pbssidinfo->network_found ==
				           PFN_NET_JUST_LOST) {
					bssidlist->pbssidinfo->network_found =
					    PFN_NET_ALREADY_FOUND;
					if (!(bssidlist->flags & WL_PFN_SUPPRESSLOST_MASK)) {
						ASSERT(pfn_info->lostcnt);
						ASSERT(pfn_info->lostcnt_bssid);
						pfn_info->lostcnt--;
						pfn_info->lostcnt_bssid--;
					}
					setbit(&pfn_info->activegroups, WL_PFN_MPF_GROUP_BSSID);
				}
				oldrssi = bssidlist->pbssidinfo->rssi;
				/* update rssi */
				if (((bssidlist->pbssidinfo->flags & WL_PFN_RSSI_ONCHANNEL) &&
				    (bi->flags & WLC_BSS_RSSI_ON_CHANNEL)) ||
				    (!(bssidlist->pbssidinfo->flags & WL_PFN_RSSI_ONCHANNEL) &&
				    !(bi->flags & WLC_BSS_RSSI_ON_CHANNEL))) {
					/* preserve max RSSI if the measurements are
					 * both on-channel or both off-channel
					 */
					bssidlist->pbssidinfo->rssi =
					                 MAX(bssidlist->pbssidinfo->rssi, bi->RSSI);
				} else if ((bi->flags & WLC_BSS_RSSI_ON_CHANNEL) &&
				    (bssidlist->pbssidinfo->flags & WL_PFN_RSSI_ONCHANNEL) == 0) {
					/* preserve the on-channel rssi measurement
					 * if the new measurement is off channel
					 */
					bssidlist->pbssidinfo->rssi = bi->RSSI;
					bssidlist->pbssidinfo->flags |= WL_PFN_RSSI_ONCHANNEL;
				}
				/* detect any change to turn off adaptive scan */
				if ((bssidlist->pbssidinfo->network_found == PFN_NET_JUST_FOUND) ||
				 (bssidlist->pbssidinfo->network_found == PFN_NET_ALREADY_FOUND)) {
					rssidiff = bssidlist->pbssidinfo->rssi - oldrssi;
					if ((bssidlist->pbssidinfo->channel != chan) ||
					    (rssidiff < -(pfn_info->param->rssi_margin) ||
					     rssidiff > pfn_info->param->rssi_margin))
						setbit(&pfn_info->activegroups,
						       WL_PFN_MPF_GROUP_BSSID);
				}
				/* update channel and time_stamp */
				bssidlist->pbssidinfo->channel = chan;
				bssidlist->pbssidinfo->time_stamp = now;
				break;
			}
			bssidlist = bssidlist->next;
		}
	}
	/* Found nets count as activity too */
	if (pfn_info->foundcnt > oldfound)
		setbit(&pfn_info->activegroups, WL_PFN_MPF_GROUP_BSSID);

	/* Now check the configured SSIDs */
	oldfound = pfn_info->foundcnt;
	if (((pfn_info->reporttype == WL_PFN_REPORT_SSIDNET) ||
	     (pfn_info->reporttype == WL_PFN_REPORT_ALLNET)) &&
	    isset(&pfn_info->scangroups, WL_PFN_MPF_GROUP_SSID)) {
		if (bi->wpa.flags) {
			for (k = 0; k < bi->wpa.acount; k++) {
				if (bi->wpa.auth[k] == RSN_AKM_UNSPECIFIED)
					scanresult_wpa_auth |= WPA_AUTH_UNSPECIFIED;
				else
					scanresult_wpa_auth |= WPA_AUTH_PSK;
			}
			wl_pfn_cipher2wsec(bi->wpa.ucount, bi->wpa.unicast, &scanresult_wsec);
		}
		if (bi->wpa2.flags) {
			for (k = 0; k < bi->wpa2.acount; k++) {
				if (bi->wpa2.auth[k] == RSN_AKM_UNSPECIFIED)
					scanresult_wpa_auth |= WPA2_AUTH_UNSPECIFIED;
				else
					scanresult_wpa_auth |= WPA2_AUTH_PSK;
			}
			wl_pfn_cipher2wsec(bi->wpa2.ucount, bi->wpa2.unicast, &scanresult_wsec);
		}

		/* When psk/psk2 set, privacy bit is set, too */
		if ((bi->capability & DOT11_CAP_PRIVACY) &&
			(scanresult_wpa_auth == WPA_AUTH_DISABLED))
			scanresult_wsec = WEP_ENABLED;

		/* Walk through the PFN element array first to find a match */
		for (k = 0; k < pfn_info->count; k++) {
			int8 rssi;
			pfn_internal = pfn_info->pfn_internal[k];

			rssi = (pfn_internal->pfn.flags & WL_PFN_RSSI_MASK)
				>> WL_PFN_RSSI_SHIFT;

			/* Bail if requested security match fails */
			if (pfn_internal->pfn.wpa_auth != WPA_AUTH_PFN_ANY) {
				if (pfn_internal->pfn.wpa_auth) {
					if (!(pfn_internal->pfn.wpa_auth & scanresult_wpa_auth))
						continue;
					if (pfn_internal->pfn.wsec &&
					   !(pfn_internal->pfn.wsec & scanresult_wsec))
						continue;
				}
				else if (!scanresult_wpa_auth) {
					if (pfn_internal->pfn.wsec &&
						!(pfn_internal->pfn.wsec & scanresult_wsec))
						continue;
					if (!pfn_internal->pfn.wsec && scanresult_wsec)
						continue;
				}
				else
					continue;
			}

			/* Bail if requested imode match fails */
			if ((pfn_internal->pfn.infra && (bi->capability & DOT11_CAP_IBSS)) ||
				(!pfn_internal->pfn.infra && (bi->capability & DOT11_CAP_ESS)))
				continue;

			if ((pfn_internal->pfn.ssid.SSID_len == ssid_len) &&
				!bcmp(ssid, pfn_internal->pfn.ssid.SSID, ssid_len) &&
				(rssi == 0 || bi->RSSI >= rssi)) {
				if (pfn_internal->network_found == PFN_NET_NOT_FOUND) {

					if (!(pfn_internal->pfn.flags &
						WL_PFN_SUPPRESSFOUND_MASK)) {
						/* Report found: either now or later */
						if (pfn_info->param->flags & IMMEDIATE_EVENT_MASK) {
							if (wl_pfn_sendonenet(pfn_info, bi, now)
								== BCME_OK) {
								pfn_internal->network_found =
									PFN_NET_ALREADY_FOUND;
							}
						} else {
							pfn_internal->network_found =
								PFN_NET_JUST_FOUND;
							pfn_info->foundcnt++;
							pfn_info->foundcnt_ssid++;
						}
					} else {
						/* Found event suppressed, just note internally */
						pfn_internal->network_found =
						PFN_NET_ALREADY_FOUND;
					}

					if (!(pfn_internal->pfn.flags &
						WL_PFN_SUPPRESSLOST_MASK)) {
						pfn_info->availbsscnt++;
					}

					pfn_internal->rssi = bi->RSSI;
					if (bi->flags & WLC_BSS_RSSI_ON_CHANNEL)
						pfn_internal->flags |= WL_PFN_RSSI_ONCHANNEL;
					else
						pfn_internal->flags &= ~WL_PFN_RSSI_ONCHANNEL;
					/* fill BSSID */
					bcopy(bi->BSSID.octet, pfn_internal->bssid.octet,
					      ETHER_ADDR_LEN);
					setbit(&pfn_info->activegroups, WL_PFN_MPF_GROUP_SSID);
					/* update channel */
					pfn_internal->channel = chan;
				} else {
					if (!bcmp(bi->BSSID.octet, pfn_internal->bssid.octet,
					          ETHER_ADDR_LEN)) {
						oldrssi = pfn_internal->rssi;
						/* update rssi */
						if (((pfn_internal->flags &
						                     WL_PFN_RSSI_ONCHANNEL) &&
						    (bi->flags & WLC_BSS_RSSI_ON_CHANNEL)) ||
						    (!(pfn_internal->flags &
						                     WL_PFN_RSSI_ONCHANNEL) &&
						    !(bi->flags & WLC_BSS_RSSI_ON_CHANNEL))) {
							/* preserve max RSSI if the measurements are
							  * both on-channel or both off-channel
							  */
							pfn_internal->rssi =
							          MAX(pfn_internal->rssi, bi->RSSI);
						} else if ((bi->flags & WLC_BSS_RSSI_ON_CHANNEL) &&
						           (pfn_internal->flags &
						                   WL_PFN_RSSI_ONCHANNEL) == 0) {
							/* preserve the on-channel rssi measurement
							  * if the new measurement is off channel
							  */
							pfn_internal->rssi = bi->RSSI;
							pfn_internal->flags |=
							             WL_PFN_RSSI_ONCHANNEL;
						}
						if (pfn_internal->network_found ==
						                        PFN_NET_JUST_LOST) {
							pfn_internal->network_found =
							                    PFN_NET_ALREADY_FOUND;
							ASSERT(pfn_info->lostcnt);
							ASSERT(pfn_info->lostcnt_ssid);
							pfn_info->lostcnt--;
							pfn_info->lostcnt_ssid--;
							setbit(&pfn_info->activegroups,
							       WL_PFN_MPF_GROUP_SSID);
						} else if (pfn_internal->network_found ==
						           PFN_NET_JUST_FOUND) {
							; /* do nothing */
						} else {
							rssidiff = pfn_internal->rssi - oldrssi;
							if (((pfn_internal->channel != chan) ||
							     (rssidiff <
							      -(pfn_info->param->rssi_margin) ||
							      rssidiff >
							      pfn_info->param->rssi_margin))) {
								/* adaptive scan reset condition */
								setbit(&pfn_info->activegroups,
								       WL_PFN_MPF_GROUP_SSID);
							}
						}
						/* update channel */
						pfn_internal->channel = chan;
					} else {
						if (bi->RSSI - pfn_internal->rssi >
							pfn_info->param->rssi_margin) {
							/* replace BSSID */
							bcopy(bi->BSSID.octet,
							      pfn_internal->bssid.octet,
							      ETHER_ADDR_LEN);
							/* update rssi, channel */
							pfn_internal->rssi = bi->RSSI;
							if (bi->flags & WLC_BSS_RSSI_ON_CHANNEL)
								pfn_internal->flags |=
								             WL_PFN_RSSI_ONCHANNEL;
							else
								pfn_internal->flags &=
								            ~WL_PFN_RSSI_ONCHANNEL;
							setbit(&pfn_info->activegroups,
							       WL_PFN_MPF_GROUP_SSID);
							pfn_internal->channel = chan;
						}
					}
				}
				pfn_internal->time_stamp = now;
				/* Found networks count as activity */
				if (pfn_info->foundcnt > oldfound)
					setbit(&pfn_info->activegroups, WL_PFN_MPF_GROUP_SSID);
				return;
			}
		}
	}

	/* If match is found as a part of PFN element array, don't need
	* to search in broadcast list. Also don't process the broadcast
	* list if back ground scanning is disabled
	*/
	if (!bssidlist &&
	    (pfn_info->param->flags & ENABLE_BKGRD_SCAN_MASK) &&
	    (pfn_info->reporttype == WL_PFN_REPORT_ALLNET)) {
		if (ssid_len > 32) {
			WL_ERROR(("too long ssid %d\n", ssid_len));
			return;
		}

		bdcast_node_list = pfn_info->bdcast_list;
		/* Traverse ssid list found through background scan */
		while (bdcast_node_list) {
			/* Match ssid name */
			if ((bdcast_node_list->bssidinfo.ssid.SSID_len == ssid_len) &&
				!bcmp(ssid, &bdcast_node_list->bssidinfo.ssid.SSID, ssid_len))
				break;
			bdcast_node_list = bdcast_node_list->next;
		}

		/* Not found in the list, add to the list */
		if (!bdcast_node_list) {
			if (pfn_info->bdcastnum >= MAX_BDCAST_NUM) {
				WL_ERROR(("wl%d: too many bdcast nodes\n",
				          pfn_info->wlc->pub->unit));
				return;
			}
			if ((retval = wl_pfn_allocate_bdcast_node(pfn_info, ssid_len, ssid, now))
			     != BCME_OK) {
				WL_ERROR(("wl%d: fail to allocate new bdcast node\n",
				           pfn_info->wlc->pub->unit));
			     return;
			}
			pfn_info->bdcast_list->bssidinfo.network_found = PFN_NET_JUST_FOUND;
			pfn_info->foundcnt++;
			pfn_info->availbsscnt++;
			bcopy(bi->BSSID.octet, pfn_info->bdcast_list->bssid.octet, ETHER_ADDR_LEN);
			pfn_info->bdcast_list->bssidinfo.channel = chan;
			pfn_info->bdcast_list->bssidinfo.rssi = bi->RSSI;
			if (bi->flags & WLC_BSS_RSSI_ON_CHANNEL)
				pfn_info->bdcast_list->bssidinfo.flags |= WL_PFN_RSSI_ONCHANNEL;
			pfn_info->bdcastnum++;
			WL_TRACE(("wl%d: Added to broadcast list RSSI = %d\n",
				pfn_info->wlc->pub->unit, bi->RSSI));
#if defined(WL_SHIF)
			/* increment the counter */
			if (SHIF_ENAB(pfn_info->wlc->pub) && (pfn_info->gpspfn == TRUE))
				pfn_info->bdlistcnt_thisscan++;
#endif /* defined(WL_SHIF) */
		} else {
			/* update channel, time_stamp */
			pfn_info->bdcast_list->bssidinfo.rssi = bi->RSSI;
			bdcast_node_list->bssidinfo.channel = chan;
			bdcast_node_list->bssidinfo.time_stamp = now;
		}
	}
}

#ifdef GSCAN
static void
wl_pfn_process_swc_bssid(wl_pfn_info_t *pfn_info,  wlc_bss_info_t * bi)
{
	int8 cur_rssi;
	wl_pfn_swc_list_t  *swc_list_ptr = pfn_info->pfn_swc_list_hdr;
	int8 new_rssi = (int8) bi->RSSI;
	struct ether_addr *bssid = &bi->BSSID;
	uint16 channel = wf_chspec_ctlchan(bi->chanspec);

	while (swc_list_ptr) {
		if (!bcmp(swc_list_ptr->bssid.octet, bssid->octet, ETHER_ADDR_LEN)) {
			cur_rssi =
			swc_list_ptr->prssi_history[swc_list_ptr->rssi_idx];
			swc_list_ptr->missing_scan_count = 0;
			swc_list_ptr->channel = (uint8) channel;
			/* Keep the highest rssi received in a scan for averaging
			 * Initial value of cur_rssi = -128
			 */
			if (bi->flags & WLC_BSS_RSSI_ON_CHANNEL &&
			    new_rssi > cur_rssi) {
				swc_list_ptr->prssi_history[swc_list_ptr->rssi_idx] = new_rssi;
				/* Updated the RSSI, was this BSSID never seen before?
				 * Update the state flag
				 */
				if (swc_list_ptr->flags == PFN_SIGNIFICANT_BSSID_NEVER_SEEN)
					swc_list_ptr->flags = PFN_SIGNIFICANT_BSSID_SEEN;
			 }
			return;
		}
		swc_list_ptr = swc_list_ptr->next;
	}
}

/* Checks if bssid_channel belongs to a currently scanning ch bucket
 * where report_type is enabled. If bssid_channel = UNSPECIFIED_CHANNEL, then
 * check if there exists at least one currently scanning ch bucket where
 * report_type is enabled
 */
bool wl_pfn_is_reporting_enabled(wl_pfn_info_t *pfn_info, uint16 bssid_channel,
     uint32 report_type)
{
	int i, start_idx = 0, num_channels;
	uint16 numofscan;
	chanspec_t *chan_orig_list;
	wl_pfn_gscan_channel_bucket_t *channel_bucket;

	if (report_type & CH_BUCKET_REPORT_FULL_RESULT) {
		/* Has HOST turned flag on for all ch buckets?
		 * Or do ALL the current scan channel buckets
		 * have the report_flag set to CH_BUCKET_REPORT_FULL_RESULT?
		 */
		if (pfn_info->pfn_gscan_cfg_param->gscan_flags &
		    GSCAN_SEND_ALL_RESULTS_CUR_SCAN) {
			return TRUE;
		}
		/* Either ALL the channel buckets have the flag
		 * report_flag not set to CH_BUCKET_REPORT_FULL_RESULT
		 * or some do, if ALL then no need to check further
		 * unless report_type is also looking for scan complete
		 * - we need to check further.
		 */
		if ((pfn_info->pfn_gscan_cfg_param->gscan_flags &
		     GSCAN_NO_FULL_SCAN_RESULTS_CUR_SCAN) &&
		     (report_type != CH_BUCKET_REPORT_SCAN_COMPLETE)) {
			return FALSE;
		}
	}

	numofscan = pfn_info->pfn_gscan_cfg_param->num_scan + 1;
	chan_orig_list = pfn_info->pfn_gscan_cfg_param->chan_orig_list;
	for (i = 0; i < pfn_info->pfn_gscan_cfg_param->count_of_channel_buckets; i++) {
		channel_bucket = &pfn_info->pfn_gscan_cfg_param->channel_bucket[i];
		if (!(numofscan % channel_bucket->bucket_freq_multiple)) {
			if (bssid_channel == UNSPECIFIED_CHANNEL) {
				if (channel_bucket->report_flag &
				     report_type)
						return TRUE;
			} else {
				num_channels = channel_bucket->bucket_end_index -
				               start_idx + 1;
				if (is_channel_in_list(&chan_orig_list[start_idx],
				   bssid_channel, num_channels)) {
					return ((channel_bucket->report_flag & report_type) ?
					           TRUE: FALSE);
				}
			}
		}
		start_idx = channel_bucket->bucket_end_index + 1;
	}

	return FALSE;
}

/* find if channel is in the list supplied */
static bool
is_channel_in_list(chanspec_t *chanspec_list, uint16 channel, uint32 num_channels)
{
	uint32 i;
	bool ret = FALSE;

	for (i = 0; i <  num_channels; i ++) {
		if (channel == wf_chspec_ctlchan(chanspec_list[i])) {
			ret = TRUE;
			break;
		}
	}
	return ret;
}
#endif /* GSCAN */

static wl_pfn_bestinfo_t*
wl_pfn_get_best_networks(wl_pfn_info_t *pfn_info, wl_pfn_bestinfo_t *bestap_ptr,
                              int16 rssi, struct ether_addr bssid)
{
	int i, j;
	wl_pfn_bestinfo_t *ptr = bestap_ptr;

	for (i = 0; i < pfn_info->param->bestn; i++) {
		if (!bcmp(ptr->pfnsubnet.BSSID.octet, bssid.octet, ETHER_ADDR_LEN)) {
			if (rssi > ptr->RSSI) {
				for (j = 0; j < i; j++)
				{
					if (rssi > (bestap_ptr + j)->RSSI)
					{
						memmove(bestap_ptr + j + 1, bestap_ptr + j,
						        sizeof(wl_pfn_bestinfo_t) * (i - j));
						return (bestap_ptr + j);
					}
				}
				ptr->RSSI = rssi;
			}
			return NULL;
		}
		ptr++;
	}
	ptr = bestap_ptr;
	for (i = 0; i < pfn_info->param->bestn; i++) {
		if (ETHER_ISNULLADDR(&ptr->pfnsubnet.BSSID)) {
			return ptr;
		}
		if (rssi > ptr->RSSI) {
			if (i < pfn_info->param->bestn - 1) {
				memmove(ptr + 1, ptr,
				    sizeof(wl_pfn_bestinfo_t) * (pfn_info->param->bestn - 1 - i));
			}
			return ptr;
		}
		ptr++;
	}
	return NULL;
}

static wl_pfn_bestinfo_bssid_t*
wl_pfn_get_best_networks_b(wl_pfn_info_t *pfn_info, wl_pfn_bestinfo_bssid_t *bestap_ptr,
                              int16 rssi, struct ether_addr bssid)
{
	int i, j;
	wl_pfn_bestinfo_bssid_t *ptr = bestap_ptr;

	for (i = 0; i < pfn_info->param->bestn; i++) {
		if (!bcmp(ptr->BSSID.octet, bssid.octet, ETHER_ADDR_LEN)) {
			if (rssi > ptr->RSSI) {
				for (j = 0; j < i; j++)
				{
					if (rssi > (bestap_ptr + j)->RSSI)
					{
						memmove(bestap_ptr + j + 1, bestap_ptr + j,
						        sizeof(wl_pfn_bestinfo_bssid_t) * (i - j));
						return (bestap_ptr + j);
					}
				}
				ptr->RSSI = rssi;
			}
			return NULL;
		}
		ptr++;
	}
	ptr = bestap_ptr;
	for (i = 0; i < pfn_info->param->bestn; i++) {
		if (ETHER_ISNULLADDR(&ptr->BSSID)) {
			return ptr;
		}
		if (rssi > ptr->RSSI) {
			if (i < pfn_info->param->bestn - 1) {
				memmove(ptr + 1, ptr,
				        sizeof(wl_pfn_bestinfo_bssid_t) *
				        (pfn_info->param->bestn - 1 - i));
			}
			return ptr;
		}
		ptr++;
	}
	return NULL;
}

#ifdef NLO
/* config suppress ssid based on pfn option */
static void
wl_pfn_cfg_supprs_ssid(wl_pfn_info_t *pfn_info, bool pre_scan)
{
	int suppress_ssid;
	int pfn_suppress_ssid;

	/* get current suppress ssid setting */
	wlc_iovar_op(pfn_info->wlc, "scan_suppress_ssid", NULL, 0,
		&suppress_ssid, sizeof(suppress_ssid), IOV_GET, NULL);
	suppress_ssid = suppress_ssid ? TRUE : FALSE;

	/* get pfn suppress ssid config */
	pfn_suppress_ssid = (((pfn_info->param->flags & SUPPRESS_SSID_MASK) >>
		SUPPRESS_SSID_BIT) == ENABLE) ? TRUE : FALSE;

	if (pre_scan) {
		/* save current setting */
		pfn_info->suppress_ssid_saved = (bool)suppress_ssid;
		if (suppress_ssid == pfn_suppress_ssid)
			/* if current setting matches pfn setting, done */
			return;
		/* need change */
		suppress_ssid = pfn_suppress_ssid;
	} else {
		/* make no change if current setting is different than
		 * pfn setting(interrupted by other scan req) or current
		 * setting is same as pre-scan saved setting
		 */
		if (suppress_ssid != pfn_suppress_ssid ||
			suppress_ssid == pfn_info->suppress_ssid_saved)
			/* if current setting matches saved setting, done */
			return;
		/* need change */
		suppress_ssid = pfn_info->suppress_ssid_saved;
	}

	/* configure suppress ssid */
	wlc_iovar_op(pfn_info->wlc, "scan_suppress_ssid", NULL, 0,
		&(suppress_ssid), sizeof(suppress_ssid), IOV_SET, NULL);
}
#endif /* NLO */

static void
wl_pfn_create_event(wl_pfn_info_t *pfn_info, uint8 pfnfdcnt, uint8 pfnltcnt, uint8 reportnet)
{
	wlc_info_t * wlc;
	wl_pfn_scanresults_t *foundresult = NULL, *lostresult = NULL;
	uint8 foundcnt = 0, lostcnt = 0;
	int32 thisfoundlen = 0, thislostlen = 0;
	wl_pfn_net_info_t *foundptr, *lostptr;
	int event_type;

	ASSERT(pfn_info);

	wlc = pfn_info->wlc;

	while (pfnfdcnt || pfnltcnt) {
		foundresult = lostresult = NULL;
		foundcnt = MIN(pfnfdcnt, EVENT_MAX_NETCNT);
		lostcnt = MIN(pfnltcnt, EVENT_MAX_NETCNT);
		thisfoundlen = sizeof(wl_pfn_scanresults_t) +
		         (foundcnt - 1) * sizeof(wl_pfn_net_info_t);
		thislostlen = sizeof(wl_pfn_scanresults_t) +
		         (lostcnt - 1) * sizeof(wl_pfn_net_info_t);

		if (foundcnt) {
			foundresult = (wl_pfn_scanresults_t *)MALLOC(wlc->osh,
			                                      thisfoundlen);
			if (!foundresult) {
				WL_INFORM(("wl%d: PFN found scanresults allcation failed\n",
				           wlc->pub->unit));
				if (pfn_info->numofscan)
					wl_pfn_send_event(pfn_info, NULL, 0,
					                  WLC_E_PFN_BEST_BATCHING);
				break;
			}
			bzero(foundresult, thisfoundlen);
			foundresult->version = PFN_SCANRESULT_VERSION;
			foundresult->count = foundcnt;
			foundptr = foundresult->netinfo;
			if (foundcnt < pfnfdcnt)
				foundresult->status = PFN_INCOMPLETE;
			else
				foundresult->status = PFN_COMPLETE;
		} else {
			foundptr = NULL;
		}

		if (lostcnt) {
			lostresult = (wl_pfn_scanresults_t *)MALLOC(wlc->osh,
			                                     thislostlen);
			if (!lostresult) {
				WL_INFORM(("wl%d: PFN lost scanresults allcation failed\n",
				           wlc->pub->unit));
				MFREE(pfn_info->wlc->osh, foundresult, thisfoundlen);
				if (pfn_info->numofscan)
					wl_pfn_send_event(pfn_info, NULL, 0,
					                  WLC_E_PFN_BEST_BATCHING);
				break;
			}
			bzero(lostresult, thislostlen);
			lostresult->version = PFN_SCANRESULT_VERSION;
			lostresult->count = lostcnt;
			lostptr = lostresult->netinfo;
			if (lostcnt < pfnltcnt)
				lostresult->status = PFN_INCOMPLETE;
			else
				lostresult->status = PFN_COMPLETE;
		} else {
			lostptr = NULL;
		}

		wl_pfn_updatenet(pfn_info, foundptr, lostptr, foundcnt, lostcnt, reportnet);

		WL_PFN(("wl%d: Found cnt %d lost cnt %d\n",
		 pfn_info->wlc->pub->unit, foundcnt, lostcnt));

		if (foundcnt) {
			event_type = (reportnet == WL_PFN_REPORT_BSSIDNET) ?
			             WLC_E_PFN_BSSID_NET_FOUND : WLC_E_PFN_NET_FOUND;
			if (BCME_OK != wl_pfn_send_event(pfn_info, (void *)foundresult,
				thisfoundlen, event_type)) {
				WL_ERROR(("wl%d: scan found network event fail\n",
				          pfn_info->wlc->pub->unit));
				MFREE(pfn_info->wlc->osh, foundresult, thisfoundlen);
			}
		}

		if (lostcnt) {
			event_type = (reportnet == WL_PFN_REPORT_BSSIDNET) ?
			              WLC_E_PFN_BSSID_NET_LOST : WLC_E_PFN_NET_LOST;
			if (BCME_OK != wl_pfn_send_event(pfn_info, (void *)lostresult,
				thislostlen, event_type)) {
				WL_ERROR(("wl%d: scan lost network event fail\n",
				          pfn_info->wlc->pub->unit));
				MFREE(pfn_info->wlc->osh, lostresult, thislostlen);
			}
		}
		/* update found and lost count */
		pfnfdcnt -= foundcnt;
		pfn_info->foundcnt -= foundcnt;
		pfnltcnt -= lostcnt;
		pfn_info->lostcnt -= lostcnt;
	}
}

/* attach bestn from current scan to the best network linked-list */
static void wl_pfn_attachbestn(wl_pfn_info_t * pfn_info, bool partial)
{
	wl_pfn_bestinfo_t *bestinfo = pfn_info->current_bestn->bestnetinfo;
	wl_pfn_bestnet_t *ptr;
	int i;
#ifdef GSCAN
	bool is_buf_threshold_set;
#endif /* GSCAN */

	STATIC_ASSERT(OFFSETOF(wl_pfn_bestinfo_t, pfnsubnet.channel) ==
	              OFFSETOF(wl_pfn_bestinfo_bssid_t, channel));

#ifdef GSCAN
	is_buf_threshold_set = pfn_info->pfn_gscan_cfg_param->buffer_threshold &&
	        (pfn_info->pfn_gscan_cfg_param->buffer_threshold < 100);
#endif /* GSCAN */

	if (bestinfo->pfnsubnet.channel) {
		if (partial)
			bestinfo->flags |= 1 << PFN_PARTIAL_SCAN_BIT;
		else
			bestinfo->flags &= ~PFN_PARTIAL_SCAN_MASK;
		/* current_bestn has valid information */
		if (pfn_info->numofscan < pfn_info->param->mscan) {
			pfn_info->numofscan++;
		} else {
			/* remove the oldest one at pfnbesthdr */
			ptr = pfn_info->bestnethdr;
			pfn_info->bestnethdr = pfn_info->bestnethdr->next;
			MFREE(pfn_info->wlc->osh, ptr, pfn_info->bestnetsize);
			if (!pfn_info->bestnethdr)
				pfn_info->bestnettail = NULL;
		}

		if (pfn_info->bestnettail &&
			(pfn_info->param->flags & ENABLE_ADAPTSCAN_MASK) ==
			(SMART_ADAPT << ENABLE_ADAPTSCAN_BIT)) {
			/* compare with the previous one, see if any change */
			if (pfn_info->param->flags & BESTN_BSSID_ONLY_MASK) {
				wl_pfn_bestinfo_bssid_t *pcurbest, *plastbest;

				pcurbest = ((wl_pfn_bestnet_b_t*)pfn_info->current_bestn)
				        ->bestnetinfo_b;
				plastbest = ((wl_pfn_bestnet_b_t*)pfn_info->bestnettail)
				        ->bestnetinfo_b;
				for (i = 0; i < pfn_info->param->bestn; i++) {
					if (bcmp(pcurbest->BSSID.octet,
					         plastbest->BSSID.octet,
					         ETHER_ADDR_LEN)) {
						/* reset scan frequency */
						pfn_info->adaptcnt = 0;
						break;
					}
					pcurbest++;
					plastbest++;
				}
			} else {
				wl_pfn_bestinfo_t *pcurbest, *plastbest;

				pcurbest = pfn_info->current_bestn->bestnetinfo;
				plastbest = pfn_info->bestnettail->bestnetinfo;
				for (i = 0; i < pfn_info->param->bestn; i++) {
					if (bcmp(pcurbest->pfnsubnet.BSSID.octet,
					         plastbest->pfnsubnet.BSSID.octet,
					         ETHER_ADDR_LEN)) {
						/* reset scan frequency */
						pfn_info->adaptcnt = 0;
						break;
					}
					pcurbest++;
					plastbest++;
				}
			}
		}
		/* add to bestnettail */
		if (pfn_info->bestnettail) {
			pfn_info->bestnettail->next = pfn_info->current_bestn;
		} else {
			pfn_info->bestnethdr = pfn_info->current_bestn;
		}
		pfn_info->bestnettail = pfn_info->current_bestn;
		pfn_info->current_bestn = NULL;
#ifdef GSCAN
		if ((pfn_info->numofscan == pfn_info->param->mscan) &&
			(!is_buf_threshold_set || !pfn_info->bestn_event_sent)) {
#else
		if (pfn_info->numofscan == pfn_info->param->mscan) {
#endif /* GSCAN */
			wl_pfn_send_event(pfn_info, NULL, 0, WLC_E_PFN_BEST_BATCHING);
#ifdef GSCAN
			pfn_info->bestn_event_sent = TRUE;
#endif /* GSCAN */
		}
	}

#ifdef GSCAN
	/* Send an event if the number of BSSIDs crosses a set threshold */
	if (is_buf_threshold_set && !pfn_info->bestn_event_sent) {
		uint32 count_of_best_n = wl_pfn_calc_num_bestn(pfn_info);
		uint32 total_capacity = pfn_info->param->mscan * pfn_info->param->bestn;

		if (total_capacity) {
			uint8 percent_full = (count_of_best_n * 100)/total_capacity;
			/* threshold breached, check if event has already been sent */
			if (percent_full >= pfn_info->pfn_gscan_cfg_param->buffer_threshold) {
				if (BCME_OK != wl_pfn_send_event(pfn_info, NULL,
					0, WLC_E_PFN_BEST_BATCHING)) {
					WL_ERROR(("wl%d: Buf thr event fail\n",
					      pfn_info->wlc->pub->unit));
				}
				else {
					pfn_info->bestn_event_sent = TRUE;
					WL_PFN(("wl%d: Buf thr evt sent!\n",
					 pfn_info->wlc->pub->unit));
				}
			}
		}
	}

	if (pfn_info->numofscan == pfn_info->param->mscan)
		pfn_info->bestn_event_sent = FALSE;
#endif /* GSCAN */
}

#define PFN_ADAPT(bits) (((bits) & ENABLE_ADAPTSCAN_MASK) >> ENABLE_ADAPTSCAN_BIT)
#define MPF_ADAPT(bits) (((bits) & WL_PFN_MPF_ADAPTSCAN_MASK) >> WL_PFN_MPF_ADAPTSCAN_BIT)
#define ADAPTVAL(pfni, gparam) \
	((MPF_ENAB((pfni)->wlc->pub) && ((gparam)->flags & WL_PFN_MPF_ADAPT_ON_MASK)) \
	 ? MPF_ADAPT((gparam)->flags)					\
	 : PFN_ADAPT((pfni)->param->flags))

static uint8
wl_pfn_mpfgrp_adaptation(wl_pfn_info_t *pfn_info)
{
	int i, groupid;
	wl_pfn_group_info_t *group = NULL;
	uint8 shared = 0;
	uint8 state = pfn_info->mpf_state;
	uint8 adapt, exp, repeat;
	uint32 slow_freq;
	wl_pfn_mpf_state_params_t *gparam;

	/* Adaptation for each group: if a group was scanned (scangroups)
	 * its count should go up, which can increase its backoff.  If it
	 * had activity (activegroups) and it's configured for smart adapt,
	 * then it should get reset.  (Activity can be leftover from an
	 * earlier aborted scan, so activegroups may not be a just a subset
	 * of scangroups.)
	 *
	 * Groups without config for the current state use shared/global
	 * scan params (scanning together); shared params must be updated
	 * only once.  (This is *different* than a group with independent
	 * scan frequency but using the shared adaptation parameters.)
	 *
	 * So while checking groups, accumulate bits for the shared group,
	 * and handle it separately at the end (by the caller).
	 */
	for (groupid = 0; groupid < pfn_info->ngroups; groupid++) {
		if (!pfn_info->ovparam.duration || pfn_info->ovparam.start_offset)
			group = pfn_info->groups[groupid];

		/* If using shared/common config, just mark for later */
		if (!group || !group->gp_scanfreq) {
			shared |= (1 << groupid);
			continue;
		}

		/* Determine the adaptation config (group override, or shared) */
		gparam = &group->mpf_states[state];
		if (gparam->flags & WL_PFN_MPF_ADAPT_ON_MASK)
			adapt = MPF_ADAPT(gparam->flags);
		else
			adapt = PFN_ADAPT(pfn_info->param->flags);
		exp = gparam->exp ? gparam->exp : pfn_info->param->exp;
		repeat = gparam->repeat ? gparam->repeat : pfn_info->param->repeat;
		slow_freq = gparam->slow_freq ? gparam->slow_freq : pfn_info->param->slow_freq;

		/* If no adaptation, we're done */
		if (adapt == OFF_ADAPT)
			continue;

		/* Slow adaptation bumps adaptcnt until repeat */
		if (adapt == SLOW_ADAPT) {
			if (slow_freq && isset(&pfn_info->scangroups, groupid) &&
			    (group->gp_adaptcnt < repeat) && (++group->gp_adaptcnt == repeat)) {
				group->gp_scanfreq = slow_freq;
			}
			continue;
		}

		/* If smart and there was group activity, clear the group adaptcnt */
		if ((adapt == SMART_ADAPT) && isset(&pfn_info->activegroups, groupid)) {
			group->gp_adaptcnt = 0;
			clrbit(&pfn_info->activegroups, groupid);
		}

		/* If we didn't finish the scan for this group, don't count it */
		if (!isset(&pfn_info->scangroups, groupid))
			continue;

		/* Cap the count to avoid wrap! */
		if (group->gp_adaptcnt <= repeat * exp)
			group->gp_adaptcnt++;

		/* If we are backed off but just got smart, reset timing */
		if (group->gp_curradapt && (group->gp_adaptcnt < repeat)) {
			group->gp_scanfreq = gparam->scan_freq;
			group->gp_losttime = gparam->lost_network_timeout;
			group->gp_curradapt = 0;
		} else {
			/* See if we just reached a new backoff spot */
			for (i = 1; i <= exp; i++) {
				if (group->gp_adaptcnt == repeat * i) {
					group->gp_curradapt = (uint8)i;
					group->gp_scanfreq = gparam->scan_freq << i;
					group->gp_losttime = gparam->lost_network_timeout << i;
					break;
				}
			}
		}
	}

	return shared;
}

static void
wl_pfn_scan_adaptation(wl_pfn_info_t *pfn_info)
{
	int i;
	uint8 adapt;
	uint8 shared = 0xff;

	/* Do per-group adaptation, limit shared to specific groups */
	if (MPF_ENAB(pfn_info->wlc->pub)) {
		shared = wl_pfn_mpfgrp_adaptation(pfn_info);
	}

	/* Now handle adaptation for the shared/common group(s).  If MPF, this
	 * means using those groups marked as shared from the above call.  For
	 * non-MPF this means everything is treated as shared.  Bump adaptcnt
	 * to account for the scan completion; if there was any activity and
	 * smart adaptation is used, reset adaptcnt.
	 */
	do {
		adapt = PFN_ADAPT(pfn_info->param->flags);
		if (adapt == OFF_ADAPT)
			break;

		if (adapt == SLOW_ADAPT) {
			if (pfn_info->param->slow_freq && (pfn_info->scangroups & shared) &&
			    (pfn_info->adaptcnt < pfn_info->param->repeat) &&
			    (++pfn_info->adaptcnt == pfn_info->param->repeat)) {
				pfn_info->a_scanfreq = pfn_info->param->slow_freq;
			}
			break;
		}

		/* Smart and strict to exponential backoff, smart may reset */
		if ((adapt == SMART_ADAPT) && (pfn_info->activegroups & shared)) {
			pfn_info->adaptcnt = 0;
			pfn_info->activegroups &= ~shared;
		}

		/* If shared groups didn't complete the scan, we're done */
		if ((pfn_info->scangroups & shared) == 0)
			break;

		if (pfn_info->adaptcnt <= pfn_info->param->repeat * pfn_info->param->exp)
			pfn_info->adaptcnt++;

		if (pfn_info->currentadapt && (pfn_info->adaptcnt < pfn_info->param->repeat)) {
			pfn_info->a_scanfreq = pfn_info->param->scan_freq;
			pfn_info->a_losttime = pfn_info->param->lost_network_timeout;
			pfn_info->currentadapt = 0;
		} else {
			for (i = 1; i <= pfn_info->param->exp; i++) {
				if (pfn_info->adaptcnt == pfn_info->param->repeat * i) {
					ASSERT(pfn_info->currentadapt == i - 1);
					pfn_info->currentadapt = (uint8)i;
					pfn_info->a_scanfreq <<= 1;
					pfn_info->a_losttime <<= 1;
				}
			}
		}
	} while (0);
}

static void
wl_pfn_scan_complete(void * data, int status, wlc_bsscfg_t *cfg)
{
	wl_pfn_info_t	*pfn_info = (wl_pfn_info_t *)data;
	uint32	now;

	ASSERT(pfn_info);
	ASSERT(pfn_info->ssid_list);

#ifdef NLO
	/* disable network offload */
	wlc_iovar_setint(pfn_info->wlc, "nlo", FALSE);

	/* handling suppress ssid option */
	wl_pfn_cfg_supprs_ssid(pfn_info, FALSE);
#endif /* NLO */

#ifdef GSCAN
	/* Issue scan complete if full scan results were
	 * being sent in current scan
	 */
	if (wl_pfn_is_reporting_enabled(pfn_info, UNSPECIFIED_CHANNEL,
	    CH_BUCKET_REPORT_SCAN_COMPLETE)) {
		wl_pfn_send_event(pfn_info, NULL, 0, WLC_E_PFN_SCAN_COMPLETE);
	}
#endif /* GSCAN */

	/* If using pfn_mac, restore the saved address */
	if (is_pfn_macaddr_change_enabled(pfn_info->wlc))
		wl_pfn_macaddr_apply(pfn_info, FALSE);

	if (status != WLC_E_STATUS_SUCCESS) {
		if ((status == WLC_E_STATUS_NEWSCAN) ||
		    (status == WLC_E_STATUS_SUPPRESS)) {
			wl_pfn_free_ssidlist(pfn_info, FALSE, TRUE);
			wl_pfn_free_bssidlist(pfn_info, FALSE, TRUE);
			pfn_info->lostcnt = 0;
			pfn_info->foundcnt = 0;
#ifdef GSCAN
			pfn_info->bestn_event_sent = FALSE;
#endif /* GSCAN */
			pfn_info->lostcnt_ssid = 0;
			pfn_info->foundcnt_ssid = 0;
			pfn_info->lostcnt_bssid = 0;
			pfn_info->foundcnt_bssid = 0;
			if ((pfn_info->param->flags & REPORT_SEPERATELY_MASK) &&
				pfn_info->param->mscan && pfn_info->current_bestn) {
				wl_pfn_attachbestn(pfn_info, TRUE);
			}
		}
#if defined(WL_SHIF)
		if (SHIF_ENAB(pfn_info->wlc->pub) &&
			(pfn_info->scan_retry > PFN_GPS_SCAN_RETRY)) {
			pfn_info->scan_retry = 0;
			if (pfn_info->gpspfn == TRUE)
				wl_pfn_gps_scanresult(pfn_info);
		} else {
			pfn_info->scan_retry++;
		}
#endif /* defined(WL_SHIF) */
		pfn_info->pfn_scan_state = PFN_SCAN_IDLE;
		goto reschedule;
	}

	/* PFN_SCAN_INPROGRESS is the only vaild state this function can process */
	if (PFN_SCAN_INPROGRESS != pfn_info->pfn_scan_state)
		return;

	/* get current ms count for timestamp/timeout processing */
	now = OSL_SYSUPTIME();

	/* If all done scanning, age the SSIDs and do the join if necessary */
	if (pfn_info->param->lost_network_timeout != -1)
		wl_pfn_ageing(pfn_info, now);

	pfn_info->pfn_scan_state = PFN_SCAN_IDLE;

	/* link the bestn from this scan */
	if (pfn_info->param->mscan && pfn_info->current_bestn) {
		wl_pfn_attachbestn(pfn_info, FALSE);
	}

#ifdef GSCAN
	/* Evaluate Significant WiFi Change */
	if (pfn_info->pfn_swc_list_hdr)
		wl_pfn_evaluate_swc(pfn_info);
	pfn_info->pfn_gscan_cfg_param->num_scan++;
#endif /* GSCAN */

	/* generate event at the end of scan */
	if (pfn_info->foundcnt || pfn_info->lostcnt) {
		if (pfn_info->param->flags & REPORT_SEPERATELY_MASK) {
			if (pfn_info->foundcnt_ssid || pfn_info->lostcnt_ssid)
				wl_pfn_create_event(pfn_info, pfn_info->foundcnt_ssid,
				        pfn_info->lostcnt_ssid, WL_PFN_REPORT_SSIDNET);
			if (pfn_info->foundcnt_bssid || pfn_info->lostcnt_bssid) {
				wl_pfn_create_event(pfn_info, pfn_info->foundcnt_bssid,
				        pfn_info->lostcnt_bssid, WL_PFN_REPORT_BSSIDNET);
			}
		}
		else
			wl_pfn_create_event(pfn_info, pfn_info->foundcnt,
			                pfn_info->lostcnt, WL_PFN_REPORT_ALLNET);
	}

	/* handle the case where non of report-lost PNO networks are ever found */
	if (pfn_info->param->lost_network_timeout != -1 &&
	    pfn_info->none_cnt != NONEED_REPORTLOST &&
	    pfn_info->none_cnt != STOP_REPORTLOST &&
	    pfn_info->availbsscnt == 0) {
		if (++pfn_info->none_cnt ==
		   (pfn_info->param->lost_network_timeout / pfn_info->param->scan_freq)) {
			if (!(pfn_info->param->flags & REPORT_SEPERATELY_MASK) &&
			    BCME_OK != wl_pfn_send_event(pfn_info, NULL,
				0, WLC_E_PFN_SCAN_ALLGONE)) {
				WL_ERROR(("wl%d: ALLGONE event fail\n",
				           pfn_info->wlc->pub->unit));
			}
			pfn_info->none_cnt = STOP_REPORTLOST;
		}
	}

	/* adaptive scanning */
	wl_pfn_scan_adaptation(pfn_info);

#if defined(WL_SHIF)
	if (SHIF_ENAB(pfn_info->wlc->pub) && (pfn_info->gpspfn == TRUE))
		wl_pfn_gps_scanresult(pfn_info);
#endif /* defined(WL_SHIF) */

reschedule:
	/* As long as we're enabled, schedule the next one */
	if (PFN_SCAN_DISABLED != pfn_info->pfn_scan_state) {
		wl_pfn_start_timer(pfn_info, FALSE);
	}
}

static void
wl_pfn_updatenet(wl_pfn_info_t *pfn_info, wl_pfn_net_info_t *foundptr,
                    wl_pfn_net_info_t *lostptr, uint8 foundcnt, uint8 lostcnt, uint8 reportnet)
{
	int i;
	wl_pfn_internal_t *pfn_internal;
	wl_pfn_bssid_list_t *bssidlist;
	wl_pfn_net_info_t *inptr;
	wl_pfn_bdcast_list_t *bdcast_node_list, *prev_bdcast_node;

	/* check PFN list */
	if (((pfn_info->reporttype == WL_PFN_REPORT_SSIDNET) ||
		(pfn_info->reporttype == WL_PFN_REPORT_ALLNET)) &&
		((reportnet == WL_PFN_REPORT_SSIDNET) ||
		(reportnet == WL_PFN_REPORT_ALLNET))) {
		for (i = 0; i < pfn_info->count; i++) {
			if (!foundcnt && !lostcnt)
				return;

			pfn_internal = pfn_info->pfn_internal[i];
			inptr = NULL;
			if (foundcnt && foundptr &&
			    pfn_internal->network_found == PFN_NET_JUST_FOUND) {
				pfn_internal->network_found = PFN_NET_ALREADY_FOUND;
				inptr = foundptr;
				foundptr++;
				foundcnt--;
				pfn_info->foundcnt_ssid--;
			} else if (lostcnt && lostptr &&
			    pfn_internal->network_found == PFN_NET_JUST_LOST) {
				pfn_internal->network_found = PFN_NET_NOT_FOUND;
				inptr = lostptr;
				lostptr++;
				lostcnt--;
				pfn_info->lostcnt_ssid--;
			}
			if (inptr) {
				bcopy(pfn_internal->bssid.octet, inptr->pfnsubnet.BSSID.octet,
				      ETHER_ADDR_LEN);
				inptr->pfnsubnet.SSID_len =
				      (uint8)pfn_internal->pfn.ssid.SSID_len;
				bcopy(pfn_internal->pfn.ssid.SSID,
				      inptr->pfnsubnet.SSID, inptr->pfnsubnet.SSID_len);
				inptr->pfnsubnet.channel = (uint8)pfn_internal->channel;
				inptr->RSSI = pfn_internal->rssi;
				inptr->timestamp = 0;
				inptr++;
			}
		}
	}
	/* check BSSID list */
	if (((pfn_info->reporttype == WL_PFN_REPORT_BSSIDNET) ||
		(pfn_info->reporttype == WL_PFN_REPORT_ALLNET)) &&
		((reportnet == WL_PFN_REPORT_BSSIDNET) ||
		(reportnet == WL_PFN_REPORT_ALLNET))) {
		for (i = 0; i < BSSIDLIST_MAX; i++) {
			bssidlist = pfn_info->bssid_list[i];
			while (bssidlist) {
				if (!foundcnt && !lostcnt)
					return;

				inptr = NULL;
				if (foundcnt && foundptr && bssidlist->pbssidinfo &&
				    bssidlist->pbssidinfo->network_found == PFN_NET_JUST_FOUND) {
					bssidlist->pbssidinfo->network_found =
					                         PFN_NET_ALREADY_FOUND;
					inptr = foundptr;
					foundptr++;
					foundcnt--;
					pfn_info->foundcnt_bssid--;
				} else if (lostcnt && lostptr && bssidlist->pbssidinfo &&
				 bssidlist->pbssidinfo->network_found == PFN_NET_JUST_LOST) {
					bssidlist->pbssidinfo->network_found = PFN_NET_NOT_FOUND;
					inptr = lostptr;
					lostptr++;
					lostcnt--;
					pfn_info->lostcnt_bssid--;
				}
				if (inptr) {
					bcopy(bssidlist->bssid.octet, inptr->pfnsubnet.BSSID.octet,
					      ETHER_ADDR_LEN);
					inptr->pfnsubnet.SSID_len =
					    (uint8)bssidlist->pbssidinfo->ssid.SSID_len;
					bcopy(bssidlist->pbssidinfo->ssid.SSID,
					      inptr->pfnsubnet.SSID,
					      inptr->pfnsubnet.SSID_len);
					inptr->pfnsubnet.channel =
						(uint8)bssidlist->pbssidinfo->channel;
					inptr->RSSI = bssidlist->pbssidinfo->rssi;
					inptr->timestamp = 0;
					inptr++;
				}
				if (bssidlist->pbssidinfo &&
					bssidlist->pbssidinfo->network_found == PFN_NET_NOT_FOUND) {
					MFREE(pfn_info->wlc->osh, bssidlist->pbssidinfo,
					      sizeof(wl_pfn_bssidinfo_t));
					bssidlist->pbssidinfo = NULL;
				}
				bssidlist = bssidlist->next;
			}
		}
	}
	/* check bdcast_list */
	if ((pfn_info->reporttype == WL_PFN_REPORT_ALLNET) &&
		(reportnet == WL_PFN_REPORT_ALLNET) &&
		(pfn_info->param->flags & ENABLE_BKGRD_SCAN_MASK) &&
#if defined(WL_SHIF)
		(SHIF_ENAB(pfn_info->wlc->pub) && (pfn_info->gps_saveflags ?
		(*pfn_info->gps_saveflags & ENABLE_BKGRD_SCAN_MASK) : TRUE)) &&
#endif /* defined(WL_SHIF) */
		1) {
		bdcast_node_list = pfn_info->bdcast_list;
		prev_bdcast_node = bdcast_node_list;
		/* Traverse ssid list found through background scan */
		while (bdcast_node_list) {
			if (!foundcnt && !lostcnt)
				return;

			inptr = NULL;
			if (foundcnt && foundptr &&
			    (bdcast_node_list->bssidinfo.network_found == PFN_NET_JUST_FOUND)) {
				bdcast_node_list->bssidinfo.network_found = PFN_NET_ALREADY_FOUND;
				inptr = foundptr;
				foundptr++;
				foundcnt--;
			}  else if (lostcnt && lostptr &&
			    bdcast_node_list->bssidinfo.network_found == PFN_NET_JUST_LOST) {
				bdcast_node_list->bssidinfo.network_found = PFN_NET_NOT_FOUND;
				inptr = lostptr;
				lostptr++;
				lostcnt--;
			}
			if (inptr) {
				bcopy(bdcast_node_list->bssid.octet, inptr->pfnsubnet.BSSID.octet,
				      ETHER_ADDR_LEN);
				inptr->pfnsubnet.SSID_len =
				      (uint8)bdcast_node_list->bssidinfo.ssid.SSID_len;
				bcopy(bdcast_node_list->bssidinfo.ssid.SSID,
				      inptr->pfnsubnet.SSID, inptr->pfnsubnet.SSID_len);
				inptr->pfnsubnet.channel =
					(uint8)bdcast_node_list->bssidinfo.channel;
				inptr->RSSI = bdcast_node_list->bssidinfo.rssi;
				inptr->timestamp = 0;
				inptr++;
			}
			if (bdcast_node_list->bssidinfo.network_found == PFN_NET_NOT_FOUND) {
				if (bdcast_node_list == pfn_info->bdcast_list) {
					pfn_info->bdcast_list = pfn_info->bdcast_list->next;
					MFREE(pfn_info->wlc->osh, bdcast_node_list,
					      sizeof(wl_pfn_bdcast_list_t));
					prev_bdcast_node = bdcast_node_list = pfn_info->bdcast_list;
				} else {
					prev_bdcast_node->next = bdcast_node_list->next;
					MFREE(pfn_info->wlc->osh, bdcast_node_list,
					      sizeof(wl_pfn_bdcast_list_t));
					bdcast_node_list = prev_bdcast_node->next;
				}
			} else {
				prev_bdcast_node = bdcast_node_list;
				bdcast_node_list = bdcast_node_list->next;
			}
		}
	}
}

static void
wl_pfn_prepare_for_scan(wl_pfn_info_t* pfn_info, wlc_ssid_t** ssidp, int* nssid,
                                  int32* bss_type)
{
	uint8 directcnt = 0;

	ASSERT(ssidp != NULL);

	*bss_type = DOT11_BSSTYPE_ANY;

	/* If SSID scanning -- meaning SSID reporting is on and the timer
	 * indicates SSID group in scangroups -- use the directed list of
	 * hidden SSIDs.  Otherwise, skip that part of the list and point
	 * to the last entry (broadcast SSID) in case we need it.
	 */
	if ((pfn_info->reporttype != WL_PFN_REPORT_BSSIDNET) &&
	    isset(&pfn_info->scangroups, WL_PFN_MPF_GROUP_SSID)) {
		*ssidp = &pfn_info->ssid_list[0];
		directcnt = pfn_info->hiddencnt;
	} else {
		*ssidp = &pfn_info->ssid_list[pfn_info->hiddencnt];
		directcnt = 0;
	}

	/* Append broadcast SSID on several conditions:
	 *   - We are SSID scanning and there are NON-hidden SSIDs, OR
	 *   - We are BSSID scanning (configured, time, reporting), OR
	 *   - Specific user-request bits are set.
	 */
	if (((pfn_info->hiddencnt < pfn_info->count) &&
	     (pfn_info->reporttype != WL_PFN_REPORT_BSSIDNET) &&
	     (isset(&pfn_info->scangroups, WL_PFN_MPF_GROUP_SSID))) ||
#ifdef GSCAN
	     (pfn_info->pfn_swc_list_hdr) ||
#endif /* GSCAN */
	    (pfn_info->bssidNum &&
	     (pfn_info->reporttype != WL_PFN_REPORT_SSIDNET) &&
	     (isset(&pfn_info->scangroups, WL_PFN_MPF_GROUP_BSSID))) ||
	    (pfn_info->param->flags & (ENABLE_BKGRD_SCAN_MASK | ENABLE_BD_SCAN_MASK)))
		*nssid = directcnt + 1; /* broadcast scan needed */
	else
		*nssid = directcnt;

	/* allocate space for current bestn */
	if (pfn_info->param->mscan && !pfn_info->current_bestn && *nssid &&
	    !(pfn_info->intflag & (ENABLE << HISTORY_OFF_BIT))) {
		pfn_info->current_bestn = (wl_pfn_bestnet_t *)MALLOCZ(pfn_info->wlc->osh,
		                           pfn_info->bestnetsize);
		if (!pfn_info->current_bestn) {
			WL_INFORM(("wl%d: PFN bestn allocation failed\n",
			           pfn_info->wlc->pub->unit));
			if (pfn_info->numofscan)
				wl_pfn_send_event(pfn_info, NULL, 0, WLC_E_PFN_BEST_BATCHING);
			if ((pfn_info->param->flags & BESTN_BSSID_ONLY_MASK) &&
			    ((pfn_info->current_bestn = pfn_info->bestnethdr))) {
				pfn_info->bestnethdr = pfn_info->bestnethdr->next;
				pfn_info->numofscan--;
			}
			return;
		}
	}
}

#ifdef GSCAN
static void wl_pfn_evaluate_swc(wl_pfn_info_t *pfn_info)
{
	int8 avg_rssi;
	wl_pfn_swc_list_t  *swc_list_ptr;
	uint8  swc_rssi_window_size;
	uint32 change_count = 0;
	bool was_bssid_ch_scanned;
	uint8  num_channels_in_scan;
	uint8 lost_ap_window = pfn_info->pfn_gscan_cfg_param->lost_ap_window;
	chanspec_t *chan_scanned = pfn_info->pfn_gscan_cfg_param->chan_scanned;

	swc_list_ptr = pfn_info->pfn_swc_list_hdr;
	swc_rssi_window_size =  pfn_info->pfn_gscan_cfg_param->swc_rssi_window_size;

	WL_PFN(("wl%d: SWC eval\n", pfn_info->wlc->pub->unit));

	num_channels_in_scan =
	       pfn_info->pfn_gscan_cfg_param->num_channels_in_scan;

	while (swc_list_ptr) {
		was_bssid_ch_scanned =
		is_channel_in_list(chan_scanned, swc_list_ptr->channel,
		              num_channels_in_scan);
		if (swc_list_ptr->flags != PFN_SIGNIFICANT_BSSID_NEVER_SEEN &&
		    was_bssid_ch_scanned) {
			/* First evaluate if this is a LOST AP   */
			if (swc_list_ptr->missing_scan_count  >=
			    lost_ap_window) {
				swc_list_ptr->flags =
				PFN_SIGNIFICANT_BSSID_LOST | PFN_REPORT_CHANGE;
				change_count++;
				WL_PFN(("wl%d: Lost AP %d >= %d scans\n", pfn_info->wlc->pub->unit,
				            swc_list_ptr->missing_scan_count,
				            pfn_info->pfn_gscan_cfg_param->lost_ap_window));
			} else {
				/* OK not a LOST AP */
				/* Was the BSSID not seen in this scan?? */
				avg_rssi = 0;
				if (swc_list_ptr->prssi_history[swc_list_ptr->rssi_idx] ==
				    INVALID_RSSI) {
					swc_list_ptr->missing_scan_count++;
				} else {
					avg_rssi = wl_pfn_swc_calc_avg_rssi(pfn_info,
					           swc_list_ptr);
				}

				/* Is this is a significant change?? */
				if (avg_rssi) {
					if (avg_rssi < swc_list_ptr->rssi_high_threshold &&
					    avg_rssi > swc_list_ptr->rssi_low_threshold) {
						swc_list_ptr->flags = PFN_SIGNIFICANT_BSSID_SEEN;
					} else if (avg_rssi >= swc_list_ptr->rssi_high_threshold) {
						if (swc_list_ptr->flags !=
						    PFN_SIGNIFICANT_BSSID_RSSI_REPORTED_HIGH) {
							swc_list_ptr->flags =
							PFN_SIGNIFICANT_BSSID_RSSI_CHANGE_HIGH;
							swc_list_ptr->flags |= PFN_REPORT_CHANGE;

							change_count++;
							WL_PFN(("wl%d: High %d >= %d \n",
							  pfn_info->wlc->pub->unit, avg_rssi,
							  swc_list_ptr->rssi_high_threshold));
						}
					} else if (swc_list_ptr->flags !=
					          PFN_SIGNIFICANT_BSSID_RSSI_REPORTED_LOW) {
						swc_list_ptr->flags =
						PFN_SIGNIFICANT_BSSID_RSSI_CHANGE_LOW;
						swc_list_ptr->flags |= PFN_REPORT_CHANGE;
						change_count++;
						WL_PFN(("wl%d: Low %d <= %d \n",
						 pfn_info->wlc->pub->unit,
						 avg_rssi, swc_list_ptr->rssi_high_threshold));
					}
				}

				 /* Update index for the next scan in the rssi ring buffer */
				swc_list_ptr->rssi_idx++;
				if (swc_list_ptr->rssi_idx >=  swc_rssi_window_size)
					swc_list_ptr->rssi_idx = 0;

			}
		}
		swc_list_ptr = swc_list_ptr->next;
	}

	WL_PFN(("wl%d: change_count %d\n", pfn_info->wlc->pub->unit, change_count));

	if (change_count >= pfn_info->pfn_gscan_cfg_param->swc_nbssid_threshold) {
		wl_pfn_gen_swc_event(pfn_info, change_count);
	}
	else {
		/* Remove report flag */
		swc_list_ptr = pfn_info->pfn_swc_list_hdr;

		while (swc_list_ptr) {
			swc_list_ptr->flags &= PFN_SIGNIFICANT_CHANGE_FLAG_MASK;
			swc_list_ptr = swc_list_ptr->next;
		}
	}
	return;
}

static void wl_pfn_gen_swc_event(wl_pfn_info_t *pfn_info, uint32 change_count)
{
	uint32 mem_needed;
	uint32 rem = change_count;
	uint32 partial_count;
	uint8 flag;
	uint16 swc_rssi_window_size = pfn_info->pfn_gscan_cfg_param->swc_rssi_window_size;
	wl_pfn_swc_list_t  *swc_list_ptr = pfn_info->pfn_swc_list_hdr;
	wl_pfn_swc_results_t *results_ptr = NULL;
	wl_pfn_significant_net_t *netptr;

	while (rem) {

		partial_count = MIN(rem, EVENT_MAX_SIGNIFICANT_CHANGE_NETCNT);

		mem_needed = sizeof(wl_pfn_swc_results_t) +
		            (partial_count - 1) * sizeof(wl_pfn_significant_net_t);
		results_ptr = (wl_pfn_swc_results_t *) MALLOC(pfn_info->wlc->osh, mem_needed);

		if (!results_ptr) {
			WL_ERROR(("wl%d: Significant change results allocation failed\n",
			 pfn_info->wlc->pub->unit));
			if (pfn_info->param->mscan && pfn_info->numofscan &&
				(pfn_info->param->flags & REPORT_SEPERATELY_MASK))
				wl_pfn_send_event(pfn_info, NULL,
				   0, WLC_E_PFN_BEST_BATCHING);
			return;
		}

		results_ptr->version = PFN_SWC_SCANRESULT_VERSION;
		results_ptr->pkt_count = (uint16)partial_count;
		results_ptr->total_count = (uint16)change_count;
		netptr = results_ptr->list;
		rem = rem - partial_count;

		while (swc_list_ptr && partial_count) {
			flag = swc_list_ptr->flags & PFN_REPORT_CHANGE;
			if (flag) {
				uint16 i, k = 0;
				int8 *prssi_history = swc_list_ptr->prssi_history;
				int8 *out_rssi = netptr->rssi;

				/* Copy oldest to newest */
				for (i = swc_list_ptr->rssi_idx; i < swc_rssi_window_size; i++, k++)
					out_rssi[k] = prssi_history[i];
				for (i = 0; i < swc_list_ptr->rssi_idx; i++, k++)
					out_rssi[k] = prssi_history[i];

				for (; k < PFN_SWC_RSSI_WINDOW_MAX; k++)
					out_rssi[k] = 0;

				netptr->channel = swc_list_ptr->channel;
				memcpy(netptr->BSSID.octet,
				   swc_list_ptr->bssid.octet, ETHER_ADDR_LEN);
				netptr->flags =
				  swc_list_ptr->flags & PFN_SIGNIFICANT_CHANGE_FLAG_MASK;
				/* Remove to report flag */
				swc_list_ptr->flags = (uint8) netptr->flags;

				/* For now reset only LOST APs */
				if (swc_list_ptr->flags == PFN_SIGNIFICANT_BSSID_LOST)	{
					bzero(swc_list_ptr->prssi_history,
					 swc_rssi_window_size);
					swc_list_ptr->rssi_idx = 0;
					swc_list_ptr->missing_scan_count = 0;
			    }

				/* Change flag
				 * BSSID_LOST -> BSSID_NEVER_SEEN
				 * BSSID_RSSI_CHANGE_LOW -> BSSID_RSSI_REPORTED_LOW
				 * BSSID_RSSI_CHANGE_HIGH -> BSSID_RSSI_REPORTED_HIGH
				 */
				swc_list_ptr->flags = swc_list_ptr->flags >> 1;

				partial_count--;
				netptr++;
			}
			swc_list_ptr = swc_list_ptr->next;
		}

		if (BCME_OK != wl_pfn_send_event(pfn_info, (void *)results_ptr,
			mem_needed, WLC_E_PFN_SWC)) {
			WL_ERROR(("wl%d:  WLC_E_PFN_SWC event fail\n",
			 pfn_info->wlc->pub->unit));
			MFREE(pfn_info->wlc->osh, results_ptr, mem_needed);
			return;
		}
		else {
			WL_PFN(("wl%d: Significant evt sent!\n", pfn_info->wlc->pub->unit));
		}
	}
	return;
}

/* Calculate average RSSI of past swc_rssi_window_size scans
 * for determining a "significant wifi change"
 */
static int8
wl_pfn_swc_calc_avg_rssi(wl_pfn_info_t *pfn_info,
                         wl_pfn_swc_list_t  *swc_list_ptr)
{
	int8 ret_avg_rssi = 0;
	int8 *prssi_history = swc_list_ptr->prssi_history;
	uint32 swc_rssi_window_size = pfn_info->pfn_gscan_cfg_param->swc_rssi_window_size;
	int32 i, k = 0;
	int16 relevance_score = 0;
	int8 weights[PFN_SWC_RSSI_WINDOW_MAX] = { RSSI_WEIGHT_MOST_RECENT, 8, 6, 4, 3, 2, 1, 1 };
	int16 avg_rssi = 0;

	/* Start from newest to oldest */
	for (i = swc_list_ptr->rssi_idx; i >= 0; i--, k++) {
		if (prssi_history[i] && prssi_history[i] != INVALID_RSSI) {
			avg_rssi += (int16)prssi_history[i] * weights[k];
			relevance_score += weights[k];
		}
	}
	for (i = swc_rssi_window_size - 1; i >= (swc_list_ptr->rssi_idx + 1); i--, k++) {
		if (prssi_history[i] && prssi_history[i] != INVALID_RSSI) {
			avg_rssi += (int16)prssi_history[i] * weights[k];
			relevance_score += weights[k];
		}
	}
	/* The rssi values used to calculate the average rssi should be relevant
	 * Doesn't make sense to calculate average rssi = most recent + 8 scans ago.
	 * Also note this function is called only when the most recent rssi != INVALID_RSSI
	 */
	if ((relevance_score - RSSI_WEIGHT_MOST_RECENT) > (RSSI_WEIGHT_MOST_RECENT >> 1)) {
		ret_avg_rssi = (int8)(avg_rssi/relevance_score);
	}
	return ret_avg_rssi;
}
#endif /* GSCAN */

static void
wl_pfn_timer(void * data)
{
	wl_pfn_info_t		*pfn_info = (wl_pfn_info_t *)data;
	int32			bss_type;
	int			nssid;
	wlc_ssid_t		*ssidp;

	uint32			now;
	uint8			scangroups;

	bool			forceall;
	uint			groupid;
	wl_pfn_group_info_t	*group = NULL;
	int32			freq;

	ASSERT(pfn_info);

	/* If we were called directly (not by timer) force all groups */
	forceall = !pfn_info->timer_active;
	pfn_info->timer_active = FALSE;

	/* Pick up the current time */
	now = OSL_SYSUPTIME();

	if (MPF_ENAB(pfn_info->wlc->pub)) {
		WL_MPF_DEBUG(("PFN_TIMER: state %d%s\n",
		              pfn_info->mpf_state, (uintptr)(forceall ? " (force)" : "")));

		/* Figure out which groups are due to be scanned */
		for (scangroups = 0, groupid = 0; groupid < pfn_info->ngroups; groupid++) {
			if (!pfn_info->ovparam.duration || pfn_info->ovparam.start_offset)
				group = pfn_info->groups[groupid];
			if (group && group->gp_scanfreq)
				freq = group->gp_scanfreq;
			else
				freq = pfn_info->a_scanfreq;

			WL_MPF_DEBUG(("Group %d: freq %d%s, last %6u.%3u: (%s)\n",
			              groupid, freq,
			              ((group && group->gp_scanfreq) ? "" : "(default)"),
			              pfn_info->last_gscan[groupid] / 1000,
			              pfn_info->last_gscan[groupid] % 1000,
			              (((now - pfn_info->last_gscan[groupid]) >=
			                (uint32)(freq - pfn_info->scan_margin)) ?
			               "scan" : (forceall ? "force" : "skip"))));
			if (forceall ||
			    ((now - pfn_info->last_gscan[groupid]) >=
			     (uint32)(freq - pfn_info->scan_margin))) {
				pfn_info->last_gscan[groupid] = now;
				scangroups |= 1 << groupid;
			}
		}
		WL_MPF_INFO(("PFN_TIMER: scangroups %02x\n", scangroups));

		/* override default scangroups with MPF scangroups */
		pfn_info->scangroups = scangroups;
	}

	/* Update the global last scantime, and indicate scan groups */
	pfn_info->last_scantime = now;

	/* Allow processing only if the state is  PFN_SCAN_IDLE,
	 * and scan is not suspended.
	 */
	if ((PFN_SCAN_IDLE == pfn_info->pfn_scan_state) &&
	    !(pfn_info->intflag & (ENABLE << SUSPEND_BIT))) {
		wl_pfn_prepare_for_scan(pfn_info, &ssidp, &nssid, &bss_type);
		/* Kick off scan when nssid != 0 */
		if (nssid && wl_pfn_start_scan(pfn_info, ssidp, nssid, bss_type) == BCME_OK) {
			WL_MPF_INFO(("Kick scan %d ssids (%p vs. %p), last len %d\n",
			             nssid, (uintptr)ssidp, (uintptr)pfn_info->ssid_list,
			             ssidp[nssid-1].SSID_len));
			pfn_info->pfn_scan_state = PFN_SCAN_INPROGRESS;
		} else {
			WL_MPF_INFO(("Skipped scan? (nssid %d)\n", nssid));
		}
	}

	/* Schedule the next timer if necessary */
	if ((PFN_SCAN_DISABLED != pfn_info->pfn_scan_state) &&
	    (PFN_SCAN_INPROGRESS != pfn_info->pfn_scan_state)) {
		WL_MPF_INFO(("PFN_TIME: start timer: state %d active %d\n",
		             pfn_info->pfn_scan_state, pfn_info->timer_active));
		wl_pfn_start_timer(pfn_info, FALSE);
	}
}

static int
wl_pfn_start_scan(wl_pfn_info_t * pfn_info, wlc_ssid_t	* ssid, int nssid, int bss_type)
{
	int rc;
	uint scanflags = 0;
#ifdef GSCAN
	wl_pfn_swc_list_t  *swc_list_ptr = pfn_info->pfn_swc_list_hdr;
#endif /* GSCAN */
	chanspec_t *chanspec_list = pfn_info->chanspec_list;
	uint8 chanspec_count = pfn_info->chanspec_count;
#ifdef BCMDBG
	char				ssidbuf[SSID_FMT_BUF_LEN];
#endif /* BCMDBG */

	WL_TRACE(("wl%d: Scan request : Nssid = %d, SSID = %s, SSID len = %d BSS type = %d\n",
		pfn_info->wlc->pub->unit, nssid, (wlc_format_ssid(ssidbuf,
		ssid->SSID, ssid->SSID_len), ssidbuf), ssid->SSID_len, bss_type));

	pfn_info->cur_scan_type = (int16)bss_type;

#ifdef NLO
	/* handling suppress ssid option */
	wl_pfn_cfg_supprs_ssid(pfn_info, TRUE);
	/* handling network offload option */
	wlc_iovar_setint(pfn_info->wlc, "nlo",
		(pfn_info->param->flags & ENABLE_NET_OFFLOAD_BIT) ? 1 : 0);
#endif /* NLO */

	scanflags |= WL_SCANFLAGS_OFFCHAN;
	if (pfn_info->intflag & (ENABLE << PROHIBITED_BIT))
		scanflags |= WL_SCANFLAGS_PROHIBITED;

#ifdef GSCAN
	while (swc_list_ptr) {
		/* Reset rssi for next scan */
		swc_list_ptr->prssi_history[swc_list_ptr->rssi_idx] = INVALID_RSSI;
		swc_list_ptr = swc_list_ptr->next;
	}

	if (pfn_info->pfn_gscan_cfg_param->count_of_channel_buckets &&
	    pfn_info->chanspec_count) {
		int i, j;
		int start_superlist_index = 0;
		int start_sublist_index = 0;
		uint32 channels_to_copy = 0;
		uint16 numofscan;
		wl_pfn_gscan_channel_bucket_t *channel_bucket;
		chanspec_t  *chan_orig_list;
		chanspec_t tmp;

		chanspec_count = 0;
		/* num_scan incremented at the end of scan */
		numofscan = pfn_info->pfn_gscan_cfg_param->num_scan + 1;
		chanspec_list = pfn_info->pfn_gscan_cfg_param->chan_scanned;
		chan_orig_list = pfn_info->pfn_gscan_cfg_param->chan_orig_list;
		/* Three possibilities: All, some or none have report_flag
		 * set to CH_BUCKET_REPORT_FULL_RESULT, let's find out.
		 */
		pfn_info->pfn_gscan_cfg_param->gscan_flags |= GSCAN_FULL_SCAN_RESULTS_CUR_SCAN;
		pfn_info->pfn_gscan_cfg_param->gscan_flags |= GSCAN_NO_FULL_SCAN_RESULTS_CUR_SCAN;

		for (i = 0; i < pfn_info->pfn_gscan_cfg_param->count_of_channel_buckets; i++) {
			channel_bucket = &pfn_info->pfn_gscan_cfg_param->channel_bucket[i];

			if (!(numofscan % channel_bucket->bucket_freq_multiple)) {
				channels_to_copy = channel_bucket->bucket_end_index -
				       start_superlist_index + 1;
				bcopy(&chan_orig_list[start_superlist_index],
				      &chanspec_list[start_sublist_index],
				      channels_to_copy * sizeof(*chan_orig_list));
				chanspec_count += ((uint8) channels_to_copy);
				start_sublist_index += channels_to_copy;
				if (!(channel_bucket->report_flag & CH_BUCKET_REPORT_FULL_RESULT)) {
					pfn_info->pfn_gscan_cfg_param->gscan_flags &=
					            ~GSCAN_FULL_SCAN_RESULTS_CUR_SCAN;
				} else {
					pfn_info->pfn_gscan_cfg_param->gscan_flags &=
					            ~GSCAN_NO_FULL_SCAN_RESULTS_CUR_SCAN;
				}
			}
			start_superlist_index = channel_bucket->bucket_end_index + 1;
		}

		pfn_info->pfn_gscan_cfg_param->num_channels_in_scan = chanspec_count;

		/* wl_scan needs channels in sorted order (optimization) */
		for (i = 1; i < chanspec_count; i++) {
			tmp = chanspec_list[i];
			for (j = i; j > 0 && chanspec_list[j-1] > tmp; j--) {
				chanspec_list[j] = chanspec_list[j-1];
			}
			chanspec_list[j] = tmp;
		}

		WL_PFN(("wl%d: num_channels %d\n", pfn_info->wlc->pub->unit, chanspec_count));
#ifdef GSCAN_DBG
		for (i = 0; i < chanspec_count; i++) {
			WL_PFN(("wl%d: %d\n", pfn_info->wlc->pub->unit,
			 CHSPEC_CHANNEL(chanspec_list[i])));
		}
#endif /* GSCAN_DBG */

	}
#endif /* GSCAN */

	/* Issue scan a scan which could be broadcast or directed scan */
	/* We don't check return value here because if wlc_scan_request() returns an error,
	 * the callback function wl_pfn_scan_complete() should have been called and
	 * PFN_SCAN_INPROGRESS flag should have been cleared
	 */
	if (!chanspec_count) { /* all channels */
		rc = wlc_scan_request_ex(pfn_info->wlc, bss_type, &ether_bcast, nssid, ssid,
		    DOT11_SCANTYPE_ACTIVE,	-1, -1,	-1,	-1, NULL, 0, 0, FALSE,
		    wl_pfn_scan_complete, pfn_info, WLC_ACTION_PNOSCAN, scanflags,
		    NULL, NULL, NULL);
	} else {
		rc = wlc_scan_request_ex(pfn_info->wlc, bss_type, &ether_bcast, nssid, ssid,
		    DOT11_SCANTYPE_ACTIVE,	-1, -1, -1, -1,	chanspec_list,
		    chanspec_count, 0, FALSE, wl_pfn_scan_complete, pfn_info,
		    WLC_ACTION_PNOSCAN, scanflags, NULL, NULL, NULL);
	}

	WL_EVENT_LOG(EVENT_LOG_TAG_TRACE_WL_INFO, TRACE_G_SCAN_STARTED);

	/* If the scan request was accepted, change MAC if configured */
	if ((rc == BCME_OK) && is_pfn_macaddr_change_enabled(pfn_info->wlc)) {
		/* If the HOST has only programmed the OUI, put in
		 * random bytes in the lower NIC specific part
		 */
		if ((pfn_info->intflag & (ENABLE << PFN_ONLY_MAC_OUI_SET_BIT)))
			wlc_getrand(pfn_info->wlc, &(pfn_info->pfn_mac.octet[DOT11_OUI_LEN]),
			    DOT11_OUI_LEN);

		WL_PFN(("Changing PFN mac addr: %02x:%02x:%02x:%02x:%02x:%02x\n",
		    pfn_info->pfn_mac.octet[0], pfn_info->pfn_mac.octet[1],
		    pfn_info->pfn_mac.octet[2], pfn_info->pfn_mac.octet[3],
		    pfn_info->pfn_mac.octet[4], pfn_info->pfn_mac.octet[5]));
		wl_pfn_macaddr_apply(pfn_info, TRUE);
	}

	return rc;
}

int
wl_pfn_scan_in_progress(wl_pfn_info_t * pfn_info)
{
	return (pfn_info->pfn_scan_state == PFN_SCAN_INPROGRESS);
}

static void
wl_pfn_ageing(wl_pfn_info_t * pfn_info, uint32 now)
{
	wl_pfn_internal_t	    * pfn_internal;
	wl_pfn_bdcast_list_t    * bdcast_list;
	int                     i;
	wl_pfn_bssid_list_t *bssid_list;
	wl_pfn_group_info_t *group;
	uint32 timeout;

#ifdef BCMDBG
	char					ssidbuf[SSID_FMT_BUF_LEN];
#endif /* BCMDBG */

	/* Evaluate PFN list for disappearence of network using (group or global) timeout */
	if ((group = pfn_info->groups[WL_PFN_MPF_GROUP_SSID]) && group->gp_scanfreq &&
	    (!pfn_info->ovparam.duration || pfn_info->ovparam.start_offset))

		timeout = group->gp_losttime;
	else
		timeout = pfn_info->a_losttime;

	if (((pfn_info->reporttype == WL_PFN_REPORT_SSIDNET) ||
	     (pfn_info->reporttype == WL_PFN_REPORT_ALLNET)) &&
	    isset(&pfn_info->scangroups, WL_PFN_MPF_GROUP_SSID) && timeout && (timeout != -1)) {
		for (i = 0; i < pfn_info->count; i++) {
			pfn_internal = pfn_info->pfn_internal[i];
			if ((pfn_internal->network_found == PFN_NET_ALREADY_FOUND) &&
			    ((now - pfn_internal->time_stamp) >= (uint32)timeout)) {
				WL_TRACE(("wl%d: Lost SSID = %s elapsed time = %d\n",
				    pfn_info->wlc->pub->unit,
				    (wlc_format_ssid(ssidbuf, pfn_internal->pfn.ssid.SSID,
				    pfn_internal->pfn.ssid.SSID_len), ssidbuf),
				    now - pfn_internal->time_stamp));
				if (!(pfn_internal->pfn.flags & WL_PFN_SUPPRESSLOST_MASK)) {
					pfn_internal->network_found = PFN_NET_JUST_LOST;
					pfn_info->lostcnt++;
					pfn_info->lostcnt_ssid++;
					setbit(&pfn_info->activegroups, WL_PFN_MPF_GROUP_SSID);
					ASSERT(pfn_info->availbsscnt);
					if (--pfn_info->availbsscnt == 0) {
						if (BCME_OK != wl_pfn_send_event(pfn_info, NULL,
							0, WLC_E_PFN_SCAN_ALLGONE)) {
							WL_ERROR(("wl%d: ALLGONE fail\n",
							      pfn_info->wlc->pub->unit));
						}
						pfn_info->none_cnt = STOP_REPORTLOST;
					}
				} else {
					pfn_internal->network_found = PFN_NET_NOT_FOUND;
				}
			} else if ((pfn_internal->network_found == PFN_NET_JUST_FOUND) &&
			           ((now - pfn_internal->time_stamp) >= (uint32)timeout)) {
				pfn_internal->network_found = PFN_NET_NOT_FOUND;
				ASSERT(pfn_info->foundcnt);
				pfn_info->foundcnt--;
				pfn_info->foundcnt_ssid--;
				if (!(pfn_internal->pfn.flags & WL_PFN_SUPPRESSLOST_MASK)) {
					ASSERT(pfn_info->availbsscnt);
					pfn_info->availbsscnt--;
				}
			}
		}
	}

	/* Evaluate bssid_list for disappearence of network using (group or global) timeout */
	if ((group = pfn_info->groups[WL_PFN_MPF_GROUP_BSSID]) && group->gp_scanfreq &&
	    (!pfn_info->ovparam.duration || pfn_info->ovparam.start_offset))
		timeout = group->gp_losttime;
	else
		timeout = pfn_info->a_losttime;

	if (((pfn_info->reporttype == WL_PFN_REPORT_BSSIDNET) ||
	     (pfn_info->reporttype == WL_PFN_REPORT_ALLNET)) &&
	    isset(&pfn_info->scangroups, WL_PFN_MPF_GROUP_BSSID) && timeout && (timeout != -1)) {
		for (i = 0; i < BSSIDLIST_MAX; i++) {
			bssid_list = pfn_info->bssid_list[i];
			while (bssid_list) {
				if (bssid_list->pbssidinfo &&
				    (bssid_list->pbssidinfo->network_found ==
				                        PFN_NET_ALREADY_FOUND) &&
				    ((now - bssid_list->pbssidinfo->time_stamp) >=
				     (uint32)timeout)) {
					if (!(bssid_list->flags & WL_PFN_SUPPRESSLOST_MASK)) {
						bssid_list->pbssidinfo->network_found =
						                    PFN_NET_JUST_LOST;
						pfn_info->lostcnt++;
						pfn_info->lostcnt_bssid++;
						setbit(&pfn_info->activegroups,
						       WL_PFN_MPF_GROUP_BSSID);
						ASSERT(pfn_info->availbsscnt);
						if (--pfn_info->availbsscnt == 0) {
							if (BCME_OK != wl_pfn_send_event(pfn_info,
							    NULL, 0, WLC_E_PFN_SCAN_ALLGONE)) {
								WL_ERROR(("wl%d: ALLGONE fail\n",
								      pfn_info->wlc->pub->unit));
							}
							pfn_info->none_cnt = STOP_REPORTLOST;
						}
					} else {
						MFREE(pfn_info->wlc->osh, bssid_list->pbssidinfo,
						      sizeof(wl_pfn_bssidinfo_t));
						bssid_list->pbssidinfo = NULL;
					}
				} else if (bssid_list->pbssidinfo &&
				    (bssid_list->pbssidinfo->network_found == PFN_NET_JUST_FOUND) &&
				           ((now - bssid_list->pbssidinfo->time_stamp) >=
				            (uint32)timeout)) {
					bssid_list->pbssidinfo->network_found = PFN_NET_NOT_FOUND;
					ASSERT(pfn_info->foundcnt);
					pfn_info->foundcnt--;
					ASSERT(pfn_info->availbsscnt);
					pfn_info->availbsscnt--;
				}
				bssid_list = bssid_list->next;
			}
		}
	}
	/* Evaluate broadcast list for disappearence of network */
	if ((pfn_info->reporttype == WL_PFN_REPORT_ALLNET) &&
		(pfn_info->param->flags & ENABLE_BKGRD_SCAN_MASK)) {
		bdcast_list = pfn_info->bdcast_list;
		while (bdcast_list) {
			/* Check to see if given SSID is gone */
			if ((now - bdcast_list->bssidinfo.time_stamp) >=
				(uint32)pfn_info->param->lost_network_timeout) {
				WL_TRACE(("wl%d: Broadcast lost SSID = %s elapsed time = %d\n",
				    pfn_info->wlc->pub->unit, (wlc_format_ssid(ssidbuf,
				    bdcast_list->bssidinfo.ssid.SSID,
				    bdcast_list->bssidinfo.ssid.SSID_len), ssidbuf),
				    now - bdcast_list->bssidinfo.time_stamp));
				if (bdcast_list->bssidinfo.network_found == PFN_NET_ALREADY_FOUND) {
					bdcast_list->bssidinfo.network_found = PFN_NET_JUST_LOST;
					pfn_info->lostcnt++;
					ASSERT(pfn_info->availbsscnt);
					if (--pfn_info->availbsscnt == 0) {
						if (BCME_OK != wl_pfn_send_event(pfn_info, NULL,
							0, WLC_E_PFN_SCAN_ALLGONE)) {
							WL_ERROR(
							   ("wl%d: PFN_SCAN_ALLGONE event fail\n",
							     pfn_info->wlc->pub->unit));
						}
						pfn_info->none_cnt = STOP_REPORTLOST;
					}
				} else if (bdcast_list->bssidinfo.network_found ==
				                                        PFN_NET_JUST_FOUND) {
					bdcast_list->bssidinfo.network_found = PFN_NET_NOT_FOUND;
					ASSERT(pfn_info->foundcnt);
					pfn_info->foundcnt--;
					ASSERT(pfn_info->availbsscnt);
					pfn_info->availbsscnt--;
				}
			}
			bdcast_list = bdcast_list->next;
		}
	}
}

void
wl_pfn_event(wl_pfn_info_t * pfn_info, wlc_event_t * e)
{
	return;
}

static int
wl_pfn_allocate_bdcast_node(wl_pfn_info_t * pfn_info, uint8 ssid_len, uint8 * ssid,
                                      uint32 now)
{
	wl_pfn_bdcast_list_t * node_ptr;

	/* Allocate for node */
	if (!(node_ptr = (wl_pfn_bdcast_list_t *) MALLOCZ(pfn_info->wlc->osh,
		sizeof(wl_pfn_bdcast_list_t)))) {
		WL_ERROR(("wl%d: PFN: MALLOC failed, size = %d\n", pfn_info->wlc->pub->unit,
			sizeof(wl_pfn_bdcast_list_t)));
		if (pfn_info->numofscan)
			wl_pfn_send_event(pfn_info, NULL, 0, WLC_E_PFN_BEST_BATCHING);
		return BCME_NOMEM;
	}

	/* Copy the ssid info */
	memcpy(node_ptr->bssidinfo.ssid.SSID, ssid, ssid_len);
	node_ptr->bssidinfo.ssid.SSID_len = ssid_len;
	node_ptr->bssidinfo.time_stamp = now;

	/* Add the new node to begining of the list */
	node_ptr->next = pfn_info->bdcast_list;
	pfn_info->bdcast_list = node_ptr;

	return BCME_OK;
}

/* free data when return due to failure, should done in scan_complete */
static int
wl_pfn_send_event(wl_pfn_info_t * pfn_info, void *data, uint32 datalen, int event_type)
{
	wlc_info_t		* wlc;
	wlc_event_t		* e;

	wlc = pfn_info->wlc;

	e = wlc_event_alloc(wlc->eventq, event_type);

	if (e == NULL) {
		WL_ERROR(("wl%d: PFN wlc_event_alloc failed\n", wlc->pub->unit));
		return BCME_NOMEM;
	}

	e->event.event_type = event_type;
	e->event.status = WLC_E_STATUS_SUCCESS;

	/* Send the SSID name and length as a data */
	e->event.datalen = datalen;
	e->data = data;

	wlc_event_if(wlc, wlc->cfg, e, NULL);

#ifdef NLO
	wlc_process_event(wlc, e);
#else
	wlc_eventq_enq(wlc->eventq, e);
#endif /* NLO */
	return BCME_OK;
}

bool
wl_pfn_scan_state_enabled(wlc_info_t *wlc)
{
	wl_pfn_info_t *pfn_info = (wl_pfn_info_t *)wlc->pfn;
	return !(pfn_info->pfn_scan_state == PFN_SCAN_DISABLED);
}

static void
wl_pfn_ovtimer(void * arg)
{
	wl_pfn_info_t *pfn_info = (wl_pfn_info_t *)arg;

	WL_PFN_INFO(("PNO Override Timer: %d, %d\n",
	             pfn_info->ovparam.start_offset, pfn_info->ovparam.duration));

	if (pfn_info->ovparam.start_offset) {
		pfn_info->ovparam.start_offset = 0;
		pfn_info->ovref_ms = OSL_SYSUPTIME();
		wl_add_timer(pfn_info->wlc->wl, pfn_info->ovtimer,
		             (pfn_info->ovparam.duration * 1000), FALSE);
		wl_pfn_ovactivate(pfn_info, TRUE);
	} else {
		pfn_info->ovparam.duration = 0;
		wl_pfn_ovactivate(pfn_info, FALSE);
	}

	/* If enabled and between scans resync timer based on updated params;
	 * a scan in progress will reschedule with the new params upon completion,
	 * should be no need to abort...
	 */
	if ((pfn_info->pfn_scan_state == PFN_SCAN_IDLE) && (pfn_info->timer_active)) {
		WL_PFN_INFO(("PNO Override timer: start PNO timer\n"));
		wl_del_timer(pfn_info->wlc->wl, pfn_info->p_pfn_timer);
		pfn_info->timer_active = FALSE;
		wl_pfn_start_timer(pfn_info, FALSE);
	}
}

static void
wl_pfn_ovactivate(wl_pfn_info_t *pfn_info, bool on)
{
	wl_pfn_param_t		*param;
#ifdef WL_MPF
	uint8			groupid;
#endif /* WL_MPF */

	WL_PFN_INFO(("PNO Override Activate %d\n", on));

	param = pfn_info->param;

	if (on) {
		/* Activate override: save defaults, copy temps */
		pfn_info->savedef.scan_freq = param->scan_freq;
		pfn_info->savedef.lost_network_timeout = param->lost_network_timeout;
		pfn_info->savedef.flags = param->flags;
		pfn_info->savedef.exp = param->exp;
		pfn_info->savedef.repeat = param->repeat;
		pfn_info->savedef.slow_freq = param->slow_freq;

		param->scan_freq = pfn_info->ovparam.override.scan_freq;
		param->lost_network_timeout = pfn_info->ovparam.override.lost_network_timeout;
		if (pfn_info->ovparam.override.flags & WL_PFN_MPF_ADAPT_ON_MASK) {
			param->flags &= ~ENABLE_ADAPTSCAN_MASK;
			param->flags |= (MPF_ADAPT(pfn_info->ovparam.override.flags) <<
			                 ENABLE_ADAPTSCAN_BIT);
			param->exp = pfn_info->ovparam.override.exp;
			param->repeat = pfn_info->ovparam.override.repeat;
			param->slow_freq = pfn_info->ovparam.override.slow_freq;
		}

#ifdef WL_MPF
		/* Reset all MPF group adaptation counters */
		for (groupid = 0; groupid < WL_PFN_MPF_MAX_GROUPS; groupid++) {
			wl_pfn_group_info_t *group;
			if ((group = pfn_info->groups[groupid])) {
				group->gp_adaptcnt = 0;
				group->gp_curradapt = 0;
			}
		}
#endif /* WL_MPF */
	} else {
		/* Deactivate override: copy back saved defaults */
		param->scan_freq = pfn_info->savedef.scan_freq;
		param->lost_network_timeout = pfn_info->savedef.lost_network_timeout;
		param->flags = pfn_info->savedef.flags;
		param->exp = pfn_info->savedef.exp;
		param->repeat = pfn_info->savedef.repeat;
		param->slow_freq = pfn_info->savedef.slow_freq;
#ifdef WL_MPF
		/* Reset all MPF group adaptation counters, restore current scan times */
		for (groupid = 0; groupid < WL_PFN_MPF_MAX_GROUPS; groupid++) {
			wl_pfn_group_info_t *group;
			uint16 state = pfn_info->mpf_state;
			if ((group = pfn_info->groups[groupid])) {
				group->gp_scanfreq = group->mpf_states[state].scan_freq;
				group->gp_losttime = group->mpf_states[state].lost_network_timeout;
				group->gp_adaptcnt = 0;
				group->gp_curradapt = 0;
			}
		}
#endif /* WL_MPF */
	}

	/* Reset the (global) active timer, and adaptation-related counters */
	pfn_info->a_scanfreq = param->scan_freq;
	pfn_info->a_losttime = param->lost_network_timeout;
	pfn_info->currentadapt = 0;
	pfn_info->adaptcnt = 0;
}

static bool is_pfn_macaddr_change_enabled(wlc_info_t * wlc)
{
	wl_pfn_info_t *pfn_info = (wl_pfn_info_t *)wlc->pfn;

	if (!(pfn_info->intflag & (ENABLE << PFN_MACADDR_BIT)))
		return FALSE;

	/* Change mac addr only if unassociated if the appropriate bit is set */
	if (!(pfn_info->intflag & (ENABLE << PFN_MACADDR_UNASSOC_ONLY_BIT)) ||
	    !wlc->stas_connected) {
		return TRUE;
	}
	return FALSE;
}

static void
wl_pfn_macaddr_apply(wl_pfn_info_t *pfn_info, bool apply)
{
	int getrc, setrc = BCME_OK;
	struct ether_addr curmac;
	struct ether_addr *setmac = NULL;

	getrc = wlc_iovar_op(pfn_info->wlc, "cur_etheraddr", NULL, 0,
	                     &curmac, ETHER_ADDR_LEN, IOV_GET, NULL);

	if (!apply && (getrc == BCME_OK) &&
	    memcmp(&curmac, &pfn_info->pfn_mac, ETHER_ADDR_LEN)) {
		/* Don't restore if the address changed under us */
		WL_PFN_INFO(("PFN MAC: restore skipped\n"));
		WL_PFN(("skipped restore %02x:%02x:%02x:%02x:%02x:%02x\n",
		    curmac.octet[0], curmac.octet[1], curmac.octet[2],
		    curmac.octet[3], curmac.octet[4], curmac.octet[5]));
		setmac = &curmac;
	} else {
		ASSERT(apply || !ETHER_ISNULLADDR(&pfn_info->save_mac));
		if (apply) {
			setmac = &pfn_info->pfn_mac;
			pfn_info->save_mac = curmac;
		} else {
			setmac = &pfn_info->save_mac;
			WL_PFN(("restore %02x:%02x:%02x:%02x:%02x:%02x\n",
			    setmac->octet[0], setmac->octet[1], setmac->octet[2],
			    setmac->octet[3], setmac->octet[4], setmac->octet[5]));
		}
		setrc = wlc_iovar_op(pfn_info->wlc, "cur_etheraddr", NULL, 0,
		                     setmac, ETHER_ADDR_LEN, IOV_SET, NULL);

		if (!apply)
			memset(&pfn_info->save_mac, 0, sizeof(pfn_info->save_mac));
	}

	if (getrc || setrc)
		WL_PFN_ERR(("PFN MAC (%d): getrc %d setrc %d cmp %d\n",
		            apply, getrc, setrc, (setmac == &curmac)));
}

static int
wl_pfn_override_set(wl_pfn_info_t * pfn_info, void * buf, int len)
{
	wl_pfn_override_param_t ovparams;
	int err = BCME_OK;

	memcpy(&ovparams, buf, sizeof(ovparams));

	if (ovparams.version != WL_PFN_OVERRIDE_VERSION)
		return BCME_VERSION;

	WL_PFN_INFO(("PNO Override Set: ver %d scanf %d sched %08x cur %08x\n",
	             ovparams.version, ovparams.override.scan_freq,
	             ((ovparams.start_offset << 16) | ovparams.duration),
	             (pfn_info->ovparam.start_offset << 16) | pfn_info->ovparam.duration));

	if (ovparams.start_offset && !ovparams.duration) {
		err = BCME_BADARG;
	} else if (!ovparams.start_offset && !ovparams.duration) {
		wl_pfn_override_cancel(pfn_info);
	} else if (pfn_info->ovparam.start_offset || pfn_info->ovparam.duration) {
		err = BCME_BUSY;
	} else {
		/* Convert secs to msecs */
		ovparams.override.scan_freq *= 1000;
		ovparams.override.lost_network_timeout *= 1000;
		ovparams.override.slow_freq *= 1000;

		/* Check adaptive parameters range */
		if ((ovparams.override.repeat > REPEAT_MAX) || (ovparams.override.exp > EXP_MAX)) {
			WL_ERROR(("wl%d: %s: bad repeat %d or exp %d\n",
			          pfn_info->wlc->pub->unit, __FUNCTION__,
			          ovparams.override.repeat, ovparams.override.exp));
			return BCME_RANGE;
		}

		/* If overriding adaptive backoff, must have relevant params */
		if (ovparams.override.flags & WL_PFN_MPF_ADAPT_ON_MASK) {
			uint adapt = ovparams.override.flags & WL_PFN_MPF_ADAPTSCAN_MASK;
			adapt = adapt >> WL_PFN_MPF_ADAPTSCAN_BIT;
			if (adapt != OFF_ADAPT) {
				if (ovparams.override.repeat == 0)
					return BCME_BADOPTION;
				if (adapt == SLOW_ADAPT) {
					if (ovparams.override.slow_freq == 0)
						return BCME_BADOPTION;
				} else {
					if (ovparams.override.exp == 0)
						return BCME_BADOPTION;
				}
			}
		}

		/* Ok -- copy them over into the pfn structure */
		memcpy(&pfn_info->ovparam, &ovparams, sizeof(ovparams));
		pfn_info->ovref_ms = OSL_SYSUPTIME();
		if (ovparams.start_offset) {
			wl_add_timer(pfn_info->wlc->wl, pfn_info->ovtimer,
			             (ovparams.start_offset * 1000), FALSE);
			WL_PFN_INFO(("PNO Override: started %d-sec timer\n",
			             ovparams.start_offset));
		} else {
			/* Fake an immediate timeout */
			pfn_info->ovparam.start_offset = 1;
			pfn_info->ovref_ms -= 1000;
			wl_pfn_ovtimer(pfn_info);
		}
	}

	return err;
}

static int
wl_pfn_override_get(wl_pfn_info_t * pfn_info, void * buf, int len, void * inp, uint inp_len)
{
	wl_pfn_override_param_t ovparams;
	uint32 adjusttime;

	/* Make sure we have sufficent input length, and check the version */
	if (inp_len < sizeof(ovparams)) {
		WL_ERROR(("wl%d: %s: Bad input length %d < %d\n", pfn_info->wlc->pub->unit,
		          __FUNCTION__, inp_len, sizeof(ovparams)));
		return BCME_BUFTOOSHORT;
	}

	memcpy(&ovparams, inp, sizeof(ovparams));

	if (ovparams.version != WL_PFN_OVERRIDE_VERSION)
		return BCME_VERSION;

	memcpy(&ovparams, &pfn_info->ovparam, sizeof(ovparams));
	adjusttime = (OSL_SYSUPTIME() - pfn_info->ovref_ms)/1000;
	if (ovparams.start_offset) {
		ovparams.start_offset -= MIN(adjusttime, ovparams.start_offset);
	} else if (ovparams.duration) {
		ovparams.duration -= MIN(adjusttime, ovparams.duration);
	}

	/* Convert msecs to secs */
	ovparams.override.scan_freq /= 1000;
	ovparams.override.lost_network_timeout /= 1000;
	ovparams.override.slow_freq /= 1000;

	/* Copy to the return buffer */
	memcpy(buf, &ovparams, sizeof(ovparams));

	return BCME_OK;
}

static int
wl_pfn_macaddr_set(wl_pfn_info_t * pfn_info, void * buf, int len)
{
	bool isnull;
	wl_pfn_macaddr_cfg_t *macp = buf;

	/* Reject changes while scanning */
	if (pfn_info->pfn_scan_state == PFN_SCAN_INPROGRESS)
		return BCME_EPERM;

	isnull = ETHER_ISNULLADDR(&macp->macaddr);

	/* A few initial input checks */
	if (!isnull && ETHER_ISMULTI(&macp->macaddr)) {
		return BCME_BADARG;
	}

	if (macp->version == LEGACY1_WL_PFN_MACADDR_CFG_VER) {
		if (!isnull && !ETHER_IS_LOCALADDR(&macp->macaddr)) {
			return BCME_BADARG;
		}
	} else if (macp->version == WL_PFN_MACADDR_CFG_VER) {
		/* PFN_ONLY_MAC_OUI_SET_BIT and PFN_MACADDR_UNASSOC_ONLY_BIT
		 * have no meaning if PFN_MACADDR_BIT is unset.
		 */
		if (macp->flags & WL_PFN_MAC_OUI_ONLY_MASK)
			pfn_info->intflag |= (ENABLE << PFN_ONLY_MAC_OUI_SET_BIT);
		else
			pfn_info->intflag &= ~(ENABLE << PFN_ONLY_MAC_OUI_SET_BIT);

		if (macp->flags & WL_PFN_SET_MAC_UNASSOC_MASK)
			pfn_info->intflag |= (ENABLE << PFN_MACADDR_UNASSOC_ONLY_BIT);
		else
			pfn_info->intflag &= ~(ENABLE << PFN_MACADDR_UNASSOC_ONLY_BIT);

	} else {
		return BCME_VERSION;
	}
	/* Ok -- pick up the new address */
	memcpy(&pfn_info->pfn_mac, &macp->macaddr, sizeof(struct ether_addr));

	if (isnull)
		pfn_info->intflag &= ~(ENABLE << PFN_MACADDR_BIT);
	else
		pfn_info->intflag |= (ENABLE << PFN_MACADDR_BIT);

	return BCME_OK;
}

static int
wl_pfn_macaddr_get(wl_pfn_info_t * pfn_info, void * buf, int len, void * inp, uint inp_len)
{
	wl_pfn_macaddr_cfg_t *macp = inp;
	uint8 inp_version = macp->version;

	if (inp_len < sizeof(*macp))
		return BCME_BUFTOOSHORT;

	/* LEGACY1_WL_PFN_MACADDR_CFG_VER and WL_PFN_MACADDR_CFG_VER
	 * are compatible, so can do both here, should modify if in future
	 * the version is bumped to an incompatible one
	 */
	if (inp_version != WL_PFN_MACADDR_CFG_VER &&
	    inp_version != LEGACY1_WL_PFN_MACADDR_CFG_VER)
		return BCME_VERSION;

	macp = buf;
	macp->version = inp_version;
	macp->flags = 0;
	memcpy(&macp->macaddr, &pfn_info->pfn_mac, sizeof(struct ether_addr));

	return BCME_OK;
}

#if defined(WL_SHIF)
#if defined(HNDGCI)
static void
wl_pfn_shub_cb(wl_pfn_info_t * pfn_info, char *buf, int len)
{
	if ((len == SH_MAX_CMD_LEN)) {
		if (!strncmp(buf, SH_SCAN_CMD, strlen(SH_SCAN_CMD))) {
			wl_pfn_gps_scan(pfn_info, TRUE);
		} else if (!strncmp(buf, SH_MOTION_CMD, strlen(SH_MOTION_CMD))) {
			pfn_info->a_scanfreq = SH_MOTION_SCANFREQ;
		} else if (!strncmp(buf, SH_IDLE_CMD, strlen(SH_IDLE_CMD))) {
			pfn_info->a_scanfreq = SH_IDLE_SCANFREQ;
		}
	} else {
		WL_ERROR(("wl%d: invalid GPS command\n",
			pfn_info->wlc->pub->unit));
	}
}

static int
wl_pfn_gps_scan(wl_pfn_info_t * pfn_info, int gps_enable)
{
	wl_pfn_param_t param;

	if (gps_enable) {
		if (pfn_info->pfn_scan_state == PFN_SCAN_INPROGRESS)
			return BCME_ERROR;

		ASSERT(!pfn_info->gps_saveflags);
		if (pfn_info->pfn_scan_state == PFN_SCAN_DISABLED) {
			bzero(&param, sizeof(wl_pfn_param_t));
			param.version = PFN_VERSION;
			param.scan_freq = DEFAULT_SCAN_FREQ;
			param.lost_network_timeout = DEFAULT_LOST_DURATION;
			param.flags |= ENABLE_BKGRD_SCAN_MASK | IMMEDIATE_SCAN_MASK;
			wl_pfn_set_params(pfn_info, &param);
			wl_pfn_enable(pfn_info, TRUE);
			if (!pfn_info->scan_retry &&
				PFN_SCAN_INPROGRESS != pfn_info->pfn_scan_state) {
				wl_pfn_enable(pfn_info, FALSE);
				wl_pfn_clear(pfn_info);
				return BCME_ERROR;
			}
		} else if (pfn_info->pfn_scan_state == PFN_SCAN_IDLE) {
			if (!(pfn_info->gps_saveflags = (int16 *)MALLOC(pfn_info->wlc->osh,
				sizeof(int16)))) {
				WL_ERROR(("wl%d: malloc failed, size = %d\n",
				pfn_info->wlc->pub->unit, sizeof(wl_pfn_param_t)));
				return BCME_NOMEM;
			}
			*pfn_info->gps_saveflags = pfn_info->param->flags;
			pfn_info->param->flags |= ENABLE_BKGRD_SCAN_MASK;
		}
		pfn_info->gpspfn = TRUE;
	} else {
		wl_pfn_enable(pfn_info, FALSE);
		wl_pfn_clear(pfn_info);
		pfn_info->gpspfn = FALSE;
	}

	return BCME_OK;
}
#endif /* defined(HNDGCI) */

#define GPS_SCAN_REPORT_ENTRY_SIZE_PER_BSSID	16
#define GPS_SCAN_REPORT_MIN_SIZE		8
static void
wl_pfn_gps_scanresult(wl_pfn_info_t * pfn_info)
{
	wl_pfn_bdcast_list_t *bdcast_node_list = pfn_info->bdcast_list;
	wlc_info_t *wlc = pfn_info->wlc;
	uint16	gps_scan_report_size = GPS_SCAN_REPORT_MIN_SIZE;
	char	cmdtype = 1;
	char	*buf;

	gps_scan_report_size += (pfn_info->bdlistcnt_thisscan *
		GPS_SCAN_REPORT_ENTRY_SIZE_PER_BSSID);

	buf = MALLOCZ(wlc->osh, gps_scan_report_size);

	if (!buf) {
		WL_ERROR(("wl%d: malloc failed, size = %d\n",
			pfn_info->wlc->pub->unit, gps_scan_report_size));
		return;
	}

	/* send the total count */
	snprintf(buf, GPS_SCAN_REPORT_MIN_SIZE - 1,
		"%d:%02x;[", cmdtype, pfn_info->bdlistcnt_thisscan);

	/* Traverse bdcast list found through background scan */
	while (pfn_info->bdlistcnt_thisscan &&
		bdcast_node_list) {

		/* send BSSID(6 bytes) and RSSI(2 bytes) */
		snprintf(buf + strlen(buf),
		GPS_SCAN_REPORT_ENTRY_SIZE_PER_BSSID + 1,
		"%02x%02x%02x%02x%02x%02x,%02x;",
		bdcast_node_list->bssid.octet[0],
		bdcast_node_list->bssid.octet[1],
		bdcast_node_list->bssid.octet[2],
		bdcast_node_list->bssid.octet[3],
		bdcast_node_list->bssid.octet[4],
		bdcast_node_list->bssid.octet[5],
		-bdcast_node_list->bssidinfo.rssi);
		bdcast_node_list = bdcast_node_list->next;
		pfn_info->bdlistcnt_thisscan--;
	}
	snprintf(buf + strlen(buf), 2, "]");
	*(buf + strlen(buf)) = '\n';

	wl_pfn_gps_send_scanresult(pfn_info, buf, gps_scan_report_size);

	ASSERT(!pfn_info->bdlistcnt_thisscan);

	if (!pfn_info->gps_saveflags) {
		wl_pfn_enable(pfn_info, FALSE);
		wl_pfn_clear(pfn_info);
	} else {
		if (!(*pfn_info->gps_saveflags & ENABLE_BKGRD_SCAN_MASK))
			wl_pfn_free_bdcast_list(pfn_info);
		pfn_info->param->flags = *pfn_info->gps_saveflags;
		MFREE(pfn_info->wlc->osh, pfn_info->gps_saveflags,
			sizeof(int16));
		pfn_info->gps_saveflags = NULL;
	}

	pfn_info->gpspfn = FALSE;
}

static void
wl_pfn_gps_send_scanresult(wl_pfn_info_t *pfn_info, char *buf, uint16 len)
{
	wlc_info_t	*wlc = pfn_info->wlc;
#ifdef HNDGCI
	si_t	*sih = pfn_info->wlc->pub->sih;
#endif /* HNDGCI */
	char	*gci_buf;
	int	ret = BCME_OK;

	/*
	 * allocate buffer for GCI UART async tx
	 * This will be freed by GCI UART
	 */
	gci_buf = (char *)MALLOC(wlc->osh, len);
	bcopy(buf, gci_buf, len);
#ifdef HNDGCI
	hndgci_uart_tx(sih, gci_buf, len);
	BCM_REFERENCE(ret);
#else
	ret = bcm_shif_send(pfn_info->shifp, gci_buf, len);
	if (ret != BCME_OK) {
		WL_ERROR(("wl%d: failed to transmit results error_code = %d\n",
			pfn_info->wlc->pub->unit, ret));
	}

#endif /* HNDGCI */
}
#endif /* defined(WL_SHIF) */

#ifdef WL_MPF
static int
wl_pfn_mpfset(wl_pfn_info_t * pfn_info, void * buf, int len)
{
	wl_pfn_mpf_param_t ioparams;
	wl_pfn_group_info_t *groupi;
	uint16 groupid, state;
	int err;

	/* Make sure PFN is not active already */
	if (PFN_SCAN_DISABLED != pfn_info->pfn_scan_state) {
		WL_ERROR(("wl%d: PFN is already active, can't service IOV_PFN_MPFSET\n",
			pfn_info->wlc->pub->unit));
		return BCME_EPERM;
	}

	/* Reject excess */
	if (len > sizeof(ioparams))
		return BCME_BUFTOOLONG;

	/* Min length already checked by iov infrastructure */
	memcpy(&ioparams, buf, sizeof(ioparams));

	if (ioparams.version != WL_PFN_MPF_VERSION)
		return BCME_VERSION;

	if (ioparams.groupid >= pfn_info->ngroups) {
		WL_ERROR(("wl%d: %s: bad groupid %d\n", pfn_info->wlc->pub->unit,
		          __FUNCTION__, ioparams.groupid));
		return BCME_RANGE;
	}
	groupid = ioparams.groupid;

	/* Input verification: for regular params we only check a couple items for
	 * range limits, so check the same ones here for states being set.
	 */
	for (state = 0; state < WL_PFN_MPF_STATES_MAX; state++) {
		if (ioparams.state[state].scan_freq == 0)
			continue;

		/* Convert secs ot msecs */
		ioparams.state[state].scan_freq *= 1000;
		ioparams.state[state].lost_network_timeout *= 1000;
		ioparams.state[state].slow_freq *= 1000;

		if ((ioparams.state[state].repeat > REPEAT_MAX) ||
		    (ioparams.state[state].exp > EXP_MAX)) {
			WL_ERROR(("wl%d: %s: bad repeat %d or exp %d for state %d\n",
			          pfn_info->wlc->pub->unit, __FUNCTION__,
			          ioparams.state[state].repeat, ioparams.state[state].exp, state));
			return BCME_RANGE;
		}

		/* If overriding adaptive backoff, must have params */
		if (ioparams.state[state].flags & WL_PFN_MPF_ADAPT_ON_MASK) {
			uint adapt = ioparams.state[state].flags & WL_PFN_MPF_ADAPTSCAN_MASK;
			adapt = adapt >> WL_PFN_MPF_ADAPTSCAN_BIT;
			if (adapt != OFF_ADAPT) {
				if (ioparams.state[state].repeat == 0)
					return BCME_BADOPTION;
				if (adapt == SLOW_ADAPT) {
					if (ioparams.state[state].slow_freq == 0)
						return BCME_BADOPTION;
				} else {
					if (ioparams.state[state].exp == 0)
						return BCME_BADOPTION;
				}
			}
		}
	}

	/* At this point the only possible failure is MALLOC of the group
	 * info block.  If we free any old block first, that should be enough
	 * to allocate a new one [assuming single-threading!] so it's safe to
	 * free here before allocating the new one.
	 */
	if ((groupi = pfn_info->groups[groupid]) == NULL) {
		groupi = MALLOC(pfn_info->wlc->osh, sizeof(wl_pfn_group_info_t));
		pfn_info->groups[groupid] = groupi;
		if (groupi == NULL)
			return BCME_NOMEM;
	}

	memcpy(groupi->mpf_states, ioparams.state, sizeof(groupi->mpf_states));

	/* If we're not yet registered with mpf, do it now */
	if (!pfn_info->mpf_registered) {
		err = wlc_mpf_register(pfn_info->wlc, 0, wl_pfn_mpf_notify, pfn_info, 0, 0);
		if (err) {
			WL_MPF_ERR(("wl_pfn_mpfset: registration failure %d\n", err));

			MFREE(pfn_info->wlc->osh, groupi, sizeof(wl_pfn_group_info_t));
			pfn_info->groups[groupid] = NULL;

			if (err != BCME_NOMEM)
				err = BCME_ERROR;
			return err;
		}
		pfn_info->mpf_registered = TRUE;

		state = pfn_info->mpf_state;
		err = wlc_mpf_current_state(pfn_info->wlc, 0, &state);
		WL_MPF_INFO(("wl_pfn_mpfset: get mpf state %d (error %d)\n", state, err));
		if ((err && err != BCME_NOTREADY) || (state >= WL_PFN_MPF_STATES_MAX)) {
			WL_ERROR(("wl_pfn_mpfset: get_current_state err %d state %d\n",
			          err, state));
			wlc_mpf_unregister(pfn_info->wlc, 0, wl_pfn_mpf_notify, pfn_info, 0);
			pfn_info->mpf_registered = FALSE;

			MFREE(pfn_info->wlc->osh, groupi, sizeof(wl_pfn_group_info_t));
			pfn_info->groups[groupid] = NULL;

			return BCME_ERROR;
		}
		pfn_info->mpf_state = state;
	}

	return BCME_OK;
}

static int
wl_pfn_mpfget(wl_pfn_info_t * pfn_info, void * buf, int len, void * inp, uint inp_len)
{
	wl_pfn_mpf_param_t ioparams;
	uint16 groupid, state;

	/* Don't care in this case if the return buf is longer...  but make sure we have
	 * sufficient input length, in case it's different.
	 */
	if (inp_len < OFFSETOF(wl_pfn_mpf_param_t, state)) {
		WL_ERROR(("wl%d: %s: Bad input length %d < %d\n", pfn_info->wlc->pub->unit,
		          __FUNCTION__, inp_len, OFFSETOF(wl_pfn_mpf_param_t, state)));
		return BCME_BUFTOOSHORT;
	}

	memcpy(&ioparams, inp, OFFSETOF(wl_pfn_mpf_param_t, state));

	if (ioparams.version != WL_PFN_MPF_VERSION)
		return BCME_VERSION;

	if (ioparams.groupid >= pfn_info->ngroups) {
		WL_ERROR(("wl%d: %s: bad groupid %d\n", pfn_info->wlc->pub->unit,
		          __FUNCTION__, ioparams.groupid));
		return BCME_RANGE;
	}
	groupid = ioparams.groupid;

	if (pfn_info->groups[groupid]) {
		memcpy(ioparams.state, pfn_info->groups[groupid]->mpf_states,
		       sizeof(ioparams.state));
	} else {
		memset(ioparams.state, 0, sizeof(ioparams.state));
	}

	/* Convert msecs to secs */
	for (state = 0; state < WL_PFN_MPF_STATES_MAX; state++) {
		if (ioparams.state[state].scan_freq == 0)
			continue;
		ioparams.state[state].scan_freq /= 1000;
		ioparams.state[state].lost_network_timeout /= 1000;
		ioparams.state[state].slow_freq /= 1000;
	}

	memcpy(buf, &ioparams, sizeof(ioparams));

	return BCME_OK;
}

static void
wl_pfn_mpf_notify(void *handle, uintptr subhandle, uint old_state, uint new_state)
{
	wl_pfn_info_t *pfn_info = handle;
	wl_pfn_group_info_t *group;
	uint8 groupid;

	ASSERT(pfn_info);

	WL_MPF_INFO(("Notified from MPF: state %d -> %d\n", old_state, new_state));
	WL_MPF_INFO(("Current state believed to be %d\n", pfn_info->mpf_state));

	if (new_state >= WL_PFN_MPF_STATES_MAX) {
		WL_ERROR(("pfn_mpf_notify: bad state %d, ignoring\n", new_state));
		WL_MPF_ERR(("pfn_mpf_notify: bad state %d, ignoring\n", new_state));
		return;
	}

	if (new_state == pfn_info->mpf_state) {
		WL_MPF_INFO(("pfn_mpf_notify: already in state %d, mpf says was %d\n",
		             new_state, old_state));
	}

	/* Change the state and reeet the timers for this state */
	pfn_info->mpf_state = new_state;

	for (groupid = 0; groupid < pfn_info->ngroups; groupid++) {
		if ((group = pfn_info->groups[groupid])) {
			group->gp_scanfreq = group->mpf_states[new_state].scan_freq;
			group->gp_losttime = group->mpf_states[new_state].lost_network_timeout;
			group->gp_adaptcnt = 0;
			group->gp_curradapt = 0;
		}
	}

	/* If no scan in progress (awaiting timer pop) call to check scans now */
	if ((pfn_info->pfn_scan_state == PFN_SCAN_IDLE) && (pfn_info->timer_active)) {
		wl_pfn_timer(pfn_info);
	}
}
#endif /* WL_MPF */

#endif /* WLPFN */
