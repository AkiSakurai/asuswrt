/*
 * Preferred network source file
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
 * $Id: wl_pfn.c 765288 2018-06-27 14:35:39Z $
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

#include <bcmutils.h>
#include <siutils.h>
#include <bcmwpa.h>
#include <ethernet.h>
#include <802.11.h>
#include <wlioctl.h>
#include <sbhndpio.h>
#include <sbhnddma.h>
#include <hnddma.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_channel.h>
#include <wlc_bsscfg.h>
#include <wlc_pub.h>
#include <wlc.h>
#include <wlc_scan.h>
#include <wlc_event.h>
#include <wlc_lq.h>
#include <wl_export.h>
#include <wl_dbg.h>
#include <wlc_dbg.h>
#include <wl_pfn.h>
#include <event_trace.h>
#include <wlc_event_utils.h>
#include <wlc_scan_utils.h>
#include <wlc_iocv.h>
#include <wlc_objregistry.h>
#include <wlc_rsdb.h>

#ifdef WL_PRQ_RAND_SEQ
#include <wlc_tx.h>
#endif // endif

#if !defined(BCMDBG)
#define WL_PFN_ERROR WL_ERROR
#endif /* BCMDBG */

/* Useful defines and enums */
#define MIN_RSSI		-32768
#define MAX_BSSID_LIST		180
#define MAX_BDCAST_NUM		50

#define PFN_DEFAULT_RETRY_SCAN    2
#define DEFAULT_SCAN_FREQ	60	/* in sec */
#define DEFAULT_LOST_DURATION	120	/* in sec */
#define DEFAULT_RSSI_MARGIN	30	/* in db */
#define DEFAULT_BKGROUND_SCAN	FALSE
#define DEFAULT_BD_SCAN		FALSE
#define INVALID_RSSI                -128
#define RSSI_WEIGHT_MOST_RECENT     10
#define NO_CHANNEL_TO_SCAN          -1

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
(sizeof(wl_pfn_gscan_cfg_t) + (((count) - 1) * sizeof(wl_pfn_gscan_ch_bucket_cfg_t)))

#define CLEAR_WLPFN_AC_FLAG(flags)	(flags &= ~AUTO_CONNECT_MASK)
#define	SET_WLPFN_AC_FLAG(flags)	(flags |= AUTO_CONNECT_MASK)

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
	IOV_PFN_MACADDR,
	IOV_PFN_GSCAN_CFG,
	IOV_PFN_ADD_SWC_BSSID,
	IOV_PFN_SSID_CFG,
	IOV_PFN_LAST
};

typedef enum tagPFNNETSTATE {
	PFN_NET_DISASSOCIATED,
	PFN_NET_ASSOCIATED
} PFNNETSTATE;

typedef enum tagPFNSCANSTATE {
	PFN_SCAN_DISABLED,
	PFN_SCAN_IDLE,
	PFN_SCAN_INPROGRESS,
	PFN_SCAN_PENDING
} PFNSCANSTATE;

typedef struct wl_pfn_bestinfo {
	wl_pfn_subnet_info_t pfnsubnet;
	uint16	flags;
	int16	RSSI; /* receive signal strength (in dBm) */
	uint32	timestamp;	/* laps in mseconds */
} wl_pfn_bestinfo_t;

typedef struct wl_pfn_bestnet {
	struct wl_pfn_bestnet *next;
	uint32 scan_ch_bucket;
	wl_pfn_bestinfo_t bestnetinfo[1]; /* bestn number of this */
} wl_pfn_bestnet_t;

#define PFN_NET_NOT_FOUND       0x1
#define PFN_NET_JUST_FOUND      0x2
#define PFN_NET_ALREADY_FOUND   0x4
#define PFN_NET_JUST_LOST       0x8

#define PFN_SSID_NOT_FOUND		0x100
#define PFN_SSID_JUST_FOUND		0x200
#define PFN_SSID_ALREADY_FOUND		0x400
#define PFN_SSID_JUST_LOST		0x800
#define PFN_INT_SSID_STATE_MASK		0xF00

#define PFN_SSID_RSSI_ON_CHANNEL	0x1000
#define PFN_SSID_INFRA			0x2000

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
#define SET_SSID_PFN_STATE(flags, state)    { \
	(flags) &= ~PFN_INT_SSID_STATE_MASK; \
	(flags) |= (state); \
	}

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

typedef struct wl_pfn_gscan_channel_bucket {
	uint8 bucket_end_index;
	uint8 bucket_freq_multiple;
	uint8 flag;
	uint8 orig_freq_multiple;
	uint8 repeat;
	uint8 max_freq_multiple;
	uint16 num_scan;
} wl_pfn_gscan_channel_bucket_t;

typedef struct wl_pfn_gscan_params {
	/* Buffer filled threshold in % to generate an event */
	uint8  buffer_threshold;
	/* No. of APs threshold to generate an evt */
	uint8  swc_nbssid_threshold;
	uint8  count_of_channel_buckets;
	uint8  swc_rssi_window_size;
	uint8  lost_ap_window;
	uint16 num_swc_bssid;
	uint16 gscan_flags;
	wl_pfn_gscan_channel_bucket_t *channel_bucket;
	/* 32 bit Bitmask for currently active ch bucket */
	uint32 cur_ch_bucket_active;
} wl_pfn_gscan_params_t;

typedef union ssid_elem {
	uint32 ssid_crc32;
	struct {
		uint8 SSID_len;
		uint8 SSID[1];
	} ssid;
} ssid_elem_t;

typedef struct wl_pfn_internal {
	struct  ether_addr	bssid;
	uint16			channel;
	int8			rssi_thresh;
	int8			rssi;
	uint16			wsec;
	uint32			wpa_auth;
	uint32			flags;
	uint32			time_stamp;
	/* Do not add fields below data */
	/* SSID/crc32ssid to follow */
	ssid_elem_t		ssid_data;
} wl_pfn_internal_t;

#define WL_PFN_INTERNAL_FIXED_LEN	OFFSETOF(wl_pfn_internal_t, ssid_data)

#define BSSIDLIST_MAX	16
#define REPEAT_MAX	100
#define EXP_MAX		5
#define WLC_BESTN_TUNABLE(wlc)		(wlc)->pub->tunables->maxbestn
#define WLC_MSCAN_TUNABLE(wlc)		(wlc)->pub->tunables->maxmscan

/* bit map in pfn internal flag */
#define SUSPEND_BIT		            0
#define PROHIBITED_BIT		        1
#define PFN_MACADDR_BIT				3
#define PFN_ONLY_MAC_OUI_SET_BIT         4
#define PFN_MACADDR_UNASSOC_ONLY_BIT     5
#define PFN_RESTRICT_LA_MACADDR_BIT      6
#define PFN_RETRY_ATTEMPT                7
#define PFN_INT_MACADDR_FLAGS_MASK     0x70

typedef struct wl_pfn_cmn_info {
	/* Broadcast or directed scan */
	int16			cur_scan_type;
	uint16			bdcastnum;
	/* Link list to hold network found as part of broadcast and not from PFN list */
	wl_pfn_bdcast_list_t	* bdcast_list;
	/* PFN task timer */
	struct wl_timer		* p_pfn_timer;
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

	/* override MAC for scan source address */
	struct ether_addr	pfn_mac;
	struct ether_addr	save_mac;
	uint8			pfn_scan_retry_threshold;
	uint8			pfn_scan_retries;
	/* gscan */
	bool			bestn_event_sent;
	wl_pfn_gscan_params_t   *pfn_gscan_cfg_param;
	wl_pfn_swc_list_t       *pfn_swc_list_hdr;
	/* wl to which the timer was attached */
	struct wl_info  *attach_wl;
	wl_ssid_ext_params_t *pfn_ssid_ext_cfg;
	/* max pfn element allowed */
	uint8 max_pfn_count;
} wl_pfn_cmn_info_t;

struct wl_pfn_info {
	wlc_info_t		*wlc;
	struct wl_pfn_cmn_info *cmn;
};

#define	BSSID_RPTLOSS_BIT	0
#define	SSID_RPTLOSS_BIT	1
#define	BSSID_RPTLOSS_MASK	1
#define	SSID_RPTLOSS_MASK	2

/* Local function prototypes */
static int wl_pfn_set_params(wl_pfn_info_t * pfn_info, void * buf);
static int wl_pfn_enable(wl_pfn_info_t * pfn_info, int enable);
static int wl_pfn_best(wl_pfn_info_t * pfn_info, void * buf, int len);
static int wl_pfn_lbest(wl_pfn_info_t * pfn_info, void * buf, int len);
static void wl_pfn_scan_complete(void * arg, int status, wlc_bsscfg_t *cfg);
static void wl_pfn_timer(void * arg);
static int wl_pfn_pause_resume(wl_pfn_info_t * pfn_info, int pause);
static int wl_pfn_start_scan(wl_pfn_info_t * pfn_info, wlc_ssid_t	* ssid,
                              int nssid, int bss_type);

static wl_pfn_bestinfo_t* wl_pfn_get_best_networks(wl_pfn_info_t *pfn_info,
	wl_pfn_bestinfo_t *bestap_ptr, int16 rssi, struct ether_addr bssid);
static void wl_pfn_updatenet(wl_pfn_info_t *pfn_info, wl_pfn_net_info_t *foundptr,
	wl_pfn_net_info_t *lostptr, uint8 foundcnt, uint8 lostcnt, uint8 reportnet);
static bool wl_pfn_attachbestn(wl_pfn_info_t * pfn_info, bool partial);
static bool is_ssid_processing_allowed(wl_pfn_info_t *pfn_info,
		wl_pfn_internal_t *pfn_internal, wlc_bss_info_t *bi, uint32 now);
static bool is_pfn_macaddr_change_enabled(wlc_info_t * wlc);
static uint32 wl_pfn_get_cur_ch_buckets(wl_pfn_info_t *pfn_info);
static void wl_pfn_fill_mac_nic_bytes(wl_pfn_info_t * pfn_info, uint8 *buf);
static int wl_pfn_set_ssid_cfg(wl_pfn_info_t *pfn_info, void * buf, int len);
static int wl_pfn_get_ssid_cfg(wl_pfn_info_t *pfn_info, void * buf, int len);
static int32 wl_pfn_calc_rssi_score(wl_pfn_info_t *pfn_info, wlc_bss_info_t *bi,
	wlc_bsscfg_t *cur_cfg, bool is_secure, bool is_same_network, bool is_cur_bssid);

/* Rommable function prototypes */
static int wl_pfn_allocate_bdcast_node(wl_pfn_info_t * pfn_info,
	uint8 ssid_len, uint8 * ssid, uint32 now);
static int wl_pfn_clear(wl_pfn_info_t * pfn_info);
static int wl_pfn_validate_set_params(wl_pfn_info_t* pfn_info, void* buf);
static int wl_pfn_add(wl_pfn_info_t * pfn_info, void * buf, int len);
static int wl_pfn_add_bssid(wl_pfn_info_t * pfn_info, void * buf, int len);
static int wl_pfn_cfg(wl_pfn_info_t * pfn_info, wlc_bsscfg_t *cfg, void * buf, int len);
static void wl_pfn_free_bdcast_list(wl_pfn_info_t * pfn_info);
static void wl_pfn_prepare_for_scan(wl_pfn_info_t* pfn_info,
	wlc_ssid_t** ssid, int* nssid, int32* bss_type);
static void wl_pfn_ageing(wl_pfn_info_t * pfn_info, uint32 now);
static int wl_pfn_send_event(wl_pfn_info_t * pfn_info,
	void *data, uint32 datalen, int event_type);
void wl_pfn_process_scan_result(wl_pfn_info_t * pfn_info, wlc_bss_info_t * bi);
static void wl_pfn_cipher2wsec(uint8 ucount, uint8 * unicast, uint16 * wsec);
static void wl_pfn_free_ssidlist(wl_pfn_info_t * pfn_info, bool freeall, bool clearjustfoundonly);
static void wl_pfn_free_bssidlist(wl_pfn_info_t * pfn_info, bool freeall, bool clearjustfoundonly);
static int wl_pfn_setscanlist(wl_pfn_info_t *pfn_info, int enable);
static int wl_pfn_updatecfg(wl_pfn_info_t * pfn_info, wl_pfn_cfg_t * pcfg, wlc_bsscfg_t *cfg);
static int wl_pfn_suspend(wl_pfn_info_t * pfn_info, int suspend);
static void wl_pfn_macaddr_apply(wl_pfn_info_t *pfn_info, bool apply);

static int wl_pfn_macaddr_set(wl_pfn_info_t * pfn_info, void * buf, int len);
static int wl_pfn_macaddr_get(wl_pfn_info_t * pfn_info, void * buf, int len,
                              void * inp, uint inp_len);
static uint8 wl_pfn_sort_remove_dup_chanspec(chanspec_t *chanspec_list, uint8 chanspec_count);

#ifdef GSCAN
static int wl_pfn_calc_num_bestn(wl_pfn_info_t * pfn_info);
static int wl_pfn_cfg_gscan(wl_pfn_info_t * pfn_info, void * buf, int len);
static void wl_pfn_update_ch_bucket_scan_num(wl_pfn_info_t *pfn_info);
static void wl_pfn_reset_ch_bucket(wl_pfn_info_t *pfn_info, bool clearall);
static int wl_pfn_add_swc_bssid(wl_pfn_info_t * pfn_info, void * buf, int len);
static void wl_pfn_free_swc_bssid_list(wl_pfn_info_t * pfn_info);
static void wl_pfn_process_swc_bssid(wl_pfn_info_t *pfn_info, wlc_bss_info_t * bi);
static void wl_pfn_evaluate_swc(wl_pfn_info_t *pfn_info);
static int8 wl_pfn_swc_calc_avg_rssi(wl_pfn_info_t *pfn_info,
   wl_pfn_swc_list_t  *swc_list_ptr);
static void wl_pfn_gen_swc_event(wl_pfn_info_t *pfn_info, uint32 change_count);
static bool is_channel_in_list(chanspec_t *chanspec_list,
                     uint16 channel, uint32 num_channels);
static uint8  wl_pfn_gscan_get_curscan_chlist(wl_pfn_info_t *pfn_info,
	chanspec_t  *chanspec_list);
static void wl_pfn_get_swc_weights(int8 *weights);
#endif /* GSCAN */

/* IOVar table */
static const bcm_iovar_t wl_pfn_iovars[] = {
	{"pfn_set", IOV_PFN_SET_PARAM,
	(0), 0, IOVT_BUFFER, OFFSETOF(wl_pfn_param_t, slow_freq)
	},
	{"pfn_cfg", IOV_PFN_CFG,
	(0), 0, IOVT_BUFFER, OFFSETOF(wl_pfn_cfg_t, flags)
	},
	{"pfn_add", IOV_PFN_ADD,
	(0), 0, IOVT_BUFFER, sizeof(wl_pfn_t)
	},
	{"pfn_add_bssid", IOV_PFN_ADD_BSSID,
	(0), 0, IOVT_BUFFER, sizeof(wl_pfn_bssid_t)
	},
	{"pfn", IOV_PFN,
	(0), 0, IOVT_BOOL, 0
	},
	{"pfnclear", IOV_PFN_CLEAR,
	(0), 0, IOVT_VOID, 0
	},
	{"pfnbest", IOV_PFN_BEST,
	(0), 0, IOVT_BUFFER, sizeof(wl_pfn_scanresults_t)
	},
	{"pfn_suspend", IOV_PFN_SUSPEND,
	(0), 0, IOVT_BOOL, 0
	},
	{"pfnlbest", IOV_PFN_LBEST,
	(0), 0, IOVT_BUFFER, sizeof(wl_pfn_lscanresults_t)
	},
	{"pfnmem", IOV_PFN_MEM,
	(0), 0, IOVT_BUFFER, 0
	},
	{"pfnrttn", IOV_PFN_RTTN,
	(0), 0, IOVT_BUFFER, 0
	},
	{"pfn_macaddr", IOV_PFN_MACADDR,
	(0), 0, IOVT_BUFFER, sizeof(wl_pfn_macaddr_cfg_t)
	},
	{"pfn_gscan_cfg", IOV_PFN_GSCAN_CFG,
	(0), 0, IOVT_BUFFER, sizeof(wl_pfn_gscan_cfg_t)
	},
	{"pfn_add_swc_bssid", IOV_PFN_ADD_SWC_BSSID,
	(0), 0, IOVT_BUFFER, sizeof(wl_pfn_significant_bssid_t)
	},
	{"pfn_ssid_cfg", IOV_PFN_SSID_CFG,
	(0), 0, IOVT_BUFFER, sizeof(wl_pfn_ssid_cfg_t)
	},
	{NULL, 0, 0, 0, 0, 0 }
};

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

static int
wl_pfn_doiovar(void *hdl, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif)
{
	wl_pfn_info_t *pfn_info = (wl_pfn_info_t *)hdl;
	int enable, suspend, rtt;
	int err = BCME_UNSUPPORTED;
	int32 *ret_int_ptr = (int32*)arg;
	int bestn;

	ASSERT(pfn_info);

	switch (actionid) {
	case IOV_SVAL(IOV_PFN_SET_PARAM):
		err = wl_pfn_set_params(pfn_info, arg);
		break;
	case IOV_SVAL(IOV_PFN_CFG): {
		wlc_bsscfg_t *bsscfg;

		/* update bsscfg w/provided interface context */
		bsscfg = wlc_bsscfg_find_by_wlcif(pfn_info->wlc, wlcif);
		ASSERT(bsscfg != NULL);

		err = wl_pfn_cfg(pfn_info, bsscfg, arg, len);
		break;
	}
#ifdef GSCAN
	case IOV_SVAL(IOV_PFN_GSCAN_CFG):
		if (GSCAN_ENAB(pfn_info->wlc->pub)) {
			err = wl_pfn_cfg_gscan(pfn_info, arg, len);
		} else {
			err = BCME_UNSUPPORTED;
		}
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
		if (GSCAN_ENAB(pfn_info->wlc->pub)) {
			err = wl_pfn_add_swc_bssid(pfn_info, arg, len);
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;
#endif /* GSCAN */
	case IOV_SVAL(IOV_PFN):
		bcopy(arg, &enable, sizeof(enable));
		err = wl_pfn_enable(pfn_info, enable);
		break;
	case IOV_GVAL(IOV_PFN):
		*ret_int_ptr = (int32)(PFN_SCAN_DISABLED != pfn_info->cmn->pfn_scan_state);
		err = BCME_OK;
		break;
	case IOV_SVAL(IOV_PFN_CLEAR):
		err = wl_pfn_clear(pfn_info);
		break;
	case IOV_GVAL(IOV_PFN_BEST):
		err = wl_pfn_best(pfn_info, arg, len);
		break;
	case IOV_SVAL(IOV_PFN_SUSPEND):
		bcopy(arg, &suspend, sizeof(suspend));
		err = wl_pfn_suspend(pfn_info, suspend);
		break;
	case IOV_GVAL(IOV_PFN_SUSPEND):
		*ret_int_ptr = ((pfn_info->cmn->intflag & (ENABLE << SUSPEND_BIT))? 1 : 0);
		err = BCME_OK;
		break;
	case IOV_GVAL(IOV_PFN_LBEST):
		err = wl_pfn_lbest(pfn_info, arg, len);
		break;
	case IOV_SVAL(IOV_PFN_MEM):
		/* Make sure PFN is not active already */
		if (PFN_SCAN_DISABLED != pfn_info->cmn->pfn_scan_state) {
			WL_PFN_ERROR(("wl%d: PFN is already active, can't service IOV_PFN_MEM\n",
				pfn_info->wlc->pub->unit));
			return BCME_EPERM;
		}
		bcopy(arg, &bestn, sizeof(bestn));
		if (bestn > 0 && bestn <= WLC_BESTN_TUNABLE(pfn_info->wlc)) {
			pfn_info->cmn->param->bestn = (uint8)bestn;
			pfn_info->cmn->bestnetsize = sizeof(wl_pfn_bestnet_t) +
			    sizeof(wl_pfn_bestinfo_t) * (pfn_info->cmn->param->bestn - 1);
			err = BCME_OK;
		} else
			err = BCME_RANGE;

		break;

	case IOV_GVAL(IOV_PFN_MEM):
		if (pfn_info->cmn->bestnetsize) {
#ifdef DONGLEBUILD
			int maxmscan;
			maxmscan = OSL_MEM_AVAIL() / pfn_info->cmn->bestnetsize;
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
		if (rtt > BESTN_MAX) {
			err = BCME_RANGE;
		} else {
			pfn_info->cmn->rttn = (uint8)*ret_int_ptr;
			err = BCME_OK;
		}
		break;
	case IOV_GVAL(IOV_PFN_SSID_CFG):
		err = wl_pfn_get_ssid_cfg(pfn_info, arg, len);
		break;
	case IOV_SVAL(IOV_PFN_SSID_CFG):
		err = wl_pfn_set_ssid_cfg(pfn_info, arg, len);
		break;
	case IOV_GVAL(IOV_PFN_MACADDR):
		err = wl_pfn_macaddr_get(pfn_info, arg, len, params, p_len);
		break;

	case IOV_SVAL(IOV_PFN_MACADDR):
		err = wl_pfn_macaddr_set(pfn_info, arg, len);
		break;

	default:
		err = BCME_UNSUPPORTED;
		break;

	}

	return err;
}

wl_pfn_info_t *
BCMATTACHFN(wl_pfn_attach)(wlc_info_t * wlc)
{
	wl_pfn_info_t *pfn_info;

	/* Allocate for pfn data */
	if (!(pfn_info = (wl_pfn_info_t *) MALLOCZ(wlc->osh, sizeof(wl_pfn_info_t)))) {
		WL_ERROR(("wl%d: PFN: MALLOCZ failed, size = %d\n", wlc->pub->unit,
			sizeof(wl_pfn_info_t)));
		goto error;
	}
	pfn_info->wlc = wlc;
	wlc->pfn = pfn_info;

	/* Check from obj registry if common info is allocated */
	pfn_info->cmn = obj_registry_get(wlc->objr, OBJR_PFN_CMN_INFO);

	if (pfn_info->cmn == NULL) {
		/* Allocate pfn common data */
		if (!(pfn_info->cmn =
			(wl_pfn_cmn_info_t *)MALLOCZ(wlc->osh, sizeof(wl_pfn_cmn_info_t)))) {
			WL_ERROR(("wl%d: PFN: MALLOCZ failed, size = %d\n", wlc->pub->unit,
				sizeof(wl_pfn_cmn_info_t)));
			goto error;
		}

		pfn_info->cmn->pfn_scan_retry_threshold = PFN_DEFAULT_RETRY_SCAN;
		/* Allocate resource for timer, but don't start it */
		pfn_info->cmn->p_pfn_timer = wl_init_timer(pfn_info->wlc->wl,
			wl_pfn_timer, pfn_info, "pfn");
		if (!pfn_info->cmn->p_pfn_timer) {
			WL_ERROR(("wl%d: Failed to allocate resoure for timer\n",
				wlc->pub->unit));
			goto error;
		}

		/* store wl to which the timer is linked */
		pfn_info->cmn->attach_wl = pfn_info->wlc->wl;

		/* store max pfn element allowed */
		pfn_info->cmn->max_pfn_count = MAX_PFN_LIST_COUNT;

		if (!(pfn_info->cmn->pfn_internal =
		    (wl_pfn_internal_t **)MALLOCZ(wlc->osh,
		    sizeof(wl_pfn_internal_t *) * pfn_info->cmn->max_pfn_count))) {
			WL_ERROR(("wl%d: PFN: MALLOC failed, size = %d\n", wlc->pub->unit,
				sizeof(wl_pfn_internal_t *) * pfn_info->cmn->max_pfn_count));
			goto error;
		}

		if (!(pfn_info->cmn->param =
		    (wl_pfn_param_t *)MALLOCZ(wlc->osh, sizeof(wl_pfn_param_t)))) {
			WL_ERROR(("wl%d: PFN: MALLOC failed, size = %d\n", wlc->pub->unit,
				sizeof(wl_pfn_param_t)));
			goto error;
		}
#ifdef GSCAN
		if (GSCAN_ENAB(pfn_info->wlc->pub) &&
		!(pfn_info->cmn->pfn_gscan_cfg_param =
		(wl_pfn_gscan_params_t *)MALLOCZ(wlc->osh, sizeof(wl_pfn_gscan_params_t)))) {
			WL_ERROR(("wl%d: PFN: MALLOC failed, size = %d\n", wlc->pub->unit,
				sizeof(wl_pfn_gscan_params_t)));
			goto error;
		}
#endif /* GSCAN */

		/* Update registry after all allocations */
		obj_registry_set(wlc->objr, OBJR_PFN_CMN_INFO, pfn_info->cmn);

#if defined(GSCAN) && !defined(WL_GSCAN_DISABLED)
		wlc->pub->cmn->_gscan = TRUE;
#else
		wlc->pub->cmn->_gscan = FALSE;
#endif /* GSCAN && !WL_GSCAN_DISABLED */
	}
	/* register a module (to handle iovars) */
	if (wlc_module_register(wlc->pub, wl_pfn_iovars, "wl_rte_iovars", pfn_info,
		wl_pfn_doiovar, NULL, NULL, NULL)) {
		WL_PFN_ERROR(("wl%d: Error registering pfn iovar\n", wlc->pub->unit));
		goto error;
	}

	wlc->pub->cmn->_wlpfn = TRUE;

	return pfn_info;

error:
	MODULE_DETACH(pfn_info, wl_pfn_detach);

	return 0;
}

int
BCMATTACHFN(wl_pfn_detach)(wl_pfn_info_t * pfn_info)
{
	int callbacks = 0;
	wl_pfn_cmn_info_t *pfn_cmn = NULL;

	/* Clean up memory */
	if (pfn_info) {
		pfn_cmn = pfn_info->cmn;
		/* Cancel the scanning timer under same wl context */
		if (pfn_cmn->attach_wl == pfn_info->wlc->wl) {
			if (pfn_cmn->p_pfn_timer) {
				if (!wl_del_timer(pfn_info->wlc->wl, pfn_cmn->p_pfn_timer))
					callbacks++;
				wl_free_timer(pfn_info->wlc->wl, pfn_cmn->p_pfn_timer);
				pfn_cmn->p_pfn_timer = NULL;
			}
		}
		if (obj_registry_unref(pfn_info->wlc->objr, OBJR_PFN_CMN_INFO) == 0) {

			/* Disable before calling 'wl_pfn_clear' */
			pfn_cmn->pfn_scan_state = PFN_SCAN_DISABLED;

			/* Free ssid list for directed scan */
			if (pfn_cmn->ssid_list != NULL) {
				MFREE(pfn_info->wlc->osh, pfn_cmn->ssid_list,
				      pfn_cmn->ssidlist_len);
				pfn_cmn->ssid_list = NULL;
			}

			wl_pfn_clear(pfn_info);

			MFREE(pfn_info->wlc->osh, pfn_cmn->pfn_internal,
			      sizeof(wl_pfn_internal_t *) * pfn_info->cmn->max_pfn_count);
			MFREE(pfn_info->wlc->osh, pfn_cmn->param, sizeof(wl_pfn_param_t));
#ifdef GSCAN
			if (pfn_cmn->pfn_gscan_cfg_param) {
				if (pfn_cmn->pfn_gscan_cfg_param->channel_bucket) {
					MFREE(pfn_info->wlc->osh,
					 pfn_cmn->pfn_gscan_cfg_param->channel_bucket,
					 (sizeof(wl_pfn_gscan_channel_bucket_t) *
					 pfn_cmn->pfn_gscan_cfg_param->count_of_channel_buckets));
				 }

				MFREE(pfn_info->wlc->osh, pfn_cmn->pfn_gscan_cfg_param,
				 sizeof(wl_pfn_gscan_params_t));
			}
#endif /* GSCAN */
			obj_registry_set(pfn_info->wlc->objr, OBJR_PFN_CMN_INFO, NULL);
			MFREE(pfn_info->wlc->osh, pfn_info->cmn, sizeof(wl_pfn_cmn_info_t));
			pfn_info->cmn = NULL;
		}
		wlc_module_unregister(pfn_info->wlc->pub, "wl_rte_iovars", pfn_info);
		MFREE(pfn_info->wlc->osh, pfn_info, sizeof(wl_pfn_info_t));
		pfn_info = NULL;
	}
	return callbacks;
}

static int
wl_pfn_validate_set_params(wl_pfn_info_t* pfn_info, void* buf)
{
	wl_pfn_cmn_info_t *pfn_cmn = pfn_info->cmn;

	/* Make sure PFN is not active already */
	if (PFN_SCAN_DISABLED != pfn_cmn->pfn_scan_state) {
		WL_PFN_ERROR(("wl%d: PFN is already active, can't service IOV_PFN_SET\n",
			pfn_info->wlc->pub->unit));
		return BCME_EPERM;
	}

	/* Copy the user/host data into internal structure */
	bcopy(buf, pfn_cmn->param, sizeof(wl_pfn_param_t));

	/* Make sure we have valid current version */
	if (PFN_VERSION != pfn_cmn->param->version) {
		WL_PFN_ERROR(("wl%d: Incorrect version expected %d, found %d\n",
			pfn_info->wlc->pub->unit, PFN_VERSION, pfn_cmn->param->version));
		return BCME_VERSION;
	}

	/* Assign default values for user parameter if necessary */
	/* Default is derived based on bzero ing of the structure from the user */
	if (!pfn_cmn->param->scan_freq)
		pfn_cmn->param->scan_freq = DEFAULT_SCAN_FREQ;

	/* Convert from sec to ms */
	pfn_cmn->param->scan_freq *= 1000;

	if (!pfn_cmn->param->lost_network_timeout)
		pfn_cmn->param->lost_network_timeout = DEFAULT_LOST_DURATION;
	/* Convert from sec to ms */
	if (pfn_cmn->param->lost_network_timeout != -1)
		pfn_cmn->param->lost_network_timeout *= 1000;

	pfn_cmn->param->slow_freq *= 1000;

	if (!pfn_cmn->param->mscan)
		pfn_cmn->param->mscan = DEFAULT_MSCAN;
	else if (pfn_cmn->param->mscan > WLC_MSCAN_TUNABLE(pfn_info->wlc))
		return BCME_RANGE;

	if (!pfn_cmn->param->bestn)
		pfn_cmn->param->bestn = DEFAULT_BESTN;
	else if (pfn_cmn->param->bestn > WLC_BESTN_TUNABLE(pfn_info->wlc))
		return BCME_RANGE;

	if (pfn_cmn->param->mscan)
		pfn_cmn->bestnetsize = sizeof(wl_pfn_bestnet_t) +
		    sizeof(wl_pfn_bestinfo_t) * (pfn_cmn->param->bestn - 1);
	else
		pfn_cmn->bestnetsize = 0;

	if (!pfn_cmn->param->repeat)
		pfn_cmn->param->repeat = DEFAULT_REPEAT;
	else if (pfn_cmn->param->repeat > REPEAT_MAX)
		return BCME_RANGE;

	if (!pfn_cmn->param->exp)
		pfn_cmn->param->exp = DEFAULT_EXP;
	else if (pfn_cmn->param->exp > EXP_MAX)
		return BCME_RANGE;

	return BCME_OK;
}

static int
wl_pfn_set_params(wl_pfn_info_t * pfn_info, void * buf)
{
	int err;

	if ((err = wl_pfn_validate_set_params(pfn_info, buf)) != BCME_OK)
		return err;

	/* if no attempt to enable autoconnect, we're done */
	if ((pfn_info->cmn->param->flags & AUTO_CONNECT_MASK) == 0)
		return BCME_OK;
	CLEAR_WLPFN_AC_FLAG(pfn_info->cmn->param->flags);
	return BCME_BADARG;
}

static int
wl_pfn_add(wl_pfn_info_t * pfn_info, void * buf, int len)
{
	wl_pfn_internal_t   * pfn_internal;
	wl_pfn_t            *pfn_ssidnet;
	uint32 mem_needed;
	bool is_precise, is_scan_active, flushall = FALSE;
	int err = BCME_OK;
	int idx_appended = pfn_info->cmn->count;

	is_scan_active = pfn_info->cmn->pfn_scan_state != PFN_SCAN_DISABLED;
	/* Make sure PFN is not active already */
	if (is_scan_active) {
		if ((err = wl_pfn_pause_resume(pfn_info, TRUE)) < 0) {
			return err;
		}
	}

	ASSERT(!((uintptr)buf & 0x3));
	pfn_ssidnet = (wl_pfn_t *)buf;
	while (len >= sizeof(wl_pfn_t)) {
		if (pfn_ssidnet->flags & WL_PFN_FLUSH_ALL_SSIDS) {
			if (!pfn_info->cmn->bssidNum &&
				!(pfn_info->cmn->param->flags &
				(ENABLE_BD_SCAN_MASK | ENABLE_BKGRD_SCAN_MASK))) {
				WL_PFN_ERROR(("wl%d: Cannot clear using pfn_add,"
					" use pfn_clear\n", pfn_info->wlc->pub->unit));
				err = BCME_EPERM;
				goto exit;
			}
			flushall = TRUE;
			idx_appended = 0;
			goto exit;
		}
		/* Check for max pfn element allowed */
		if (pfn_info->cmn->count >= pfn_info->cmn->max_pfn_count) {
			WL_PFN_ERROR(("wl%d: PFN element count %d exceeded limit of %d\n",
			   pfn_info->wlc->pub->unit, pfn_info->cmn->count + 1,
			   pfn_info->cmn->max_pfn_count));
			err = BCME_RANGE;
			goto exit;
		}
		if (pfn_ssidnet->ssid.SSID_len > DOT11_MAX_SSID_LEN) {
			WL_PFN_ERROR(("wl%d: Erroneous SSID length %d\n",
				pfn_info->wlc->pub->unit, pfn_ssidnet->ssid.SSID_len));
			err = BCME_BADARG;
			goto exit;
		}
		mem_needed = WL_PFN_INTERNAL_FIXED_LEN;
		if ((pfn_ssidnet->flags & WL_PFN_SSID_IMPRECISE_MATCH) &&
				!(pfn_ssidnet->flags & WL_PFN_HIDDEN_MASK)) {
			/* CRC32 length */
			mem_needed += sizeof(uint32);
			is_precise = FALSE;
		} else {
			/* SSID + ssid_len */
			mem_needed += (pfn_ssidnet->ssid.SSID_len + 1);
			is_precise = TRUE;
		}
		/* Allocate memory for pfn internal structure type 'wl_pfn_internal_t' */
		if (!(pfn_internal = (wl_pfn_internal_t *)MALLOCZ(pfn_info->wlc->osh,
		                      mem_needed))) {
			WL_ERROR(("wl%d: PFN MALLOC failed, size = %d\n",
			          pfn_info->wlc->pub->unit, mem_needed));
			err = BCME_NOMEM;
			goto exit;
		}

		pfn_internal->rssi_thresh = ((pfn_ssidnet->flags & WL_PFN_RSSI_MASK) >>
				WL_PFN_RSSI_SHIFT);
		pfn_internal->flags = pfn_ssidnet->flags & WL_PFN_IOVAR_FLAG_MASK;
		/* set network_found to NOT_FOUND */
		SET_SSID_PFN_STATE(pfn_internal->flags, PFN_SSID_NOT_FOUND);
		if (pfn_internal->flags & WL_PFN_HIDDEN_MASK) {
			/* Clear erroneous config, if any */
			pfn_internal->flags &= ~WL_PFN_SSID_IMPRECISE_MATCH;
			pfn_info->cmn->hiddencnt++;
		}
		if (!(pfn_internal->flags & WL_PFN_SUPPRESSLOST_MASK)) {
			pfn_info->cmn->reportloss |= ENABLE << SSID_RPTLOSS_BIT;
		}
		if (pfn_ssidnet->infra) {
			pfn_internal->flags |= PFN_SSID_INFRA;
		}
		pfn_internal->wpa_auth = (uint16)pfn_ssidnet->wpa_auth;
		pfn_internal->wsec = (uint16)pfn_ssidnet->wsec;
		if (is_precise) {
			memcpy(pfn_internal->ssid_data.ssid.SSID, pfn_ssidnet->ssid.SSID,
					pfn_ssidnet->ssid.SSID_len);
			pfn_internal->ssid_data.ssid.SSID_len = (uint8)pfn_ssidnet->ssid.SSID_len;
		} else {
			pfn_internal->ssid_data.ssid_crc32 = hndcrc32(pfn_ssidnet->ssid.SSID,
					pfn_ssidnet->ssid.SSID_len, CRC32_INIT_VALUE);
		}
		pfn_ssidnet++;
		len -= sizeof(wl_pfn_t);
		/* Store the pointer to the block and increment the count */
		pfn_info->cmn->pfn_internal[pfn_info->cmn->count++] = pfn_internal;
	}
	ASSERT(!len);
exit:
	if ((err != BCME_OK) || flushall) {
		int i;
		for (i = idx_appended; i < pfn_info->cmn->count; i++) {
			pfn_internal = pfn_info->cmn->pfn_internal[i];
			mem_needed = WL_PFN_INTERNAL_FIXED_LEN;
			if (pfn_internal->flags & WL_PFN_SSID_IMPRECISE_MATCH) {
				mem_needed += sizeof(uint32);
			} else {
				mem_needed += (pfn_internal->ssid_data.ssid.SSID_len + 1);
			}
			MFREE(pfn_info->wlc->osh, pfn_internal, mem_needed);
			pfn_info->cmn->pfn_internal[i] = NULL;
		}
		pfn_info->cmn->count = (int16)idx_appended;
	}
	if (is_scan_active) {
		err = wl_pfn_pause_resume(pfn_info, FALSE);
	}

	return err;
}

static int
wl_pfn_set_ssid_cfg(wl_pfn_info_t * pfn_info, void * buf, int len)
{
	wl_pfn_ssid_cfg_t *cfg = (wl_pfn_ssid_cfg_t *)buf;
	int err = BCME_OK;
	bool scan_in_progress;

	if (cfg->version != WL_PFN_SSID_CFG_VERSION) {
		return BCME_VERSION;
	}
	scan_in_progress = (pfn_info->cmn->pfn_scan_state == PFN_SCAN_INPROGRESS);
	/* If scan is in progress need to pause it and restart for cfg change */
	if (scan_in_progress) {
		if ((err = wl_pfn_pause_resume(pfn_info, TRUE)) < 0) {
			return err;
		}
	}
	if (cfg->flags & WL_PFN_SSID_CFG_CLEAR) {
		if (pfn_info->cmn->pfn_ssid_ext_cfg) {
			MFREE(pfn_info->wlc->osh, pfn_info->cmn->pfn_ssid_ext_cfg,
					sizeof(wl_ssid_ext_params_t));
			pfn_info->cmn->pfn_ssid_ext_cfg = NULL;
		}
		goto exit;
	}
	if (!pfn_info->cmn->pfn_ssid_ext_cfg) {
		if ((pfn_info->cmn->pfn_ssid_ext_cfg =
			(wl_ssid_ext_params_t *)MALLOC(pfn_info->wlc->osh,
			sizeof(wl_ssid_ext_params_t))) == NULL) {
			WL_PFN_ERROR(("wl%d: PFN MALLOC failed, size = %d\n",
				pfn_info->wlc->pub->unit, sizeof(wl_ssid_ext_params_t)));
			err = BCME_NOMEM;
			goto exit;
		}
	}
	memcpy(pfn_info->cmn->pfn_ssid_ext_cfg, &cfg->params, sizeof(wl_ssid_ext_params_t));
exit:
	if (scan_in_progress) {
		err = wl_pfn_pause_resume(pfn_info, FALSE);
	}
	return err;
}

static int
wl_pfn_get_ssid_cfg(wl_pfn_info_t * pfn_info, void * buf, int len)
{
	wl_pfn_ssid_cfg_t *cfg = (wl_pfn_ssid_cfg_t *)buf;

	if (!pfn_info->cmn->pfn_ssid_ext_cfg) {
		WL_PFN_ERROR(("wl%d: Params not set!\n", pfn_info->wlc->pub->unit));
		return BCME_NOTFOUND;
	}
	cfg->version = WL_PFN_SSID_CFG_VERSION;
	cfg->flags = 0;
	memcpy(&cfg->params, pfn_info->cmn->pfn_ssid_ext_cfg, sizeof(wl_ssid_ext_params_t));
	return BCME_OK;
}

static int
wl_pfn_add_bssid(wl_pfn_info_t * pfn_info, void * buf, int len)
{
	wl_pfn_bssid_t      *bssidptr;
	wl_pfn_bssid_list_t * bssid_list_ptr;
	int                 index;
	wl_pfn_cmn_info_t *pfn_cmn = pfn_info->cmn;

	/* Make sure PFN is not active already */
	if (PFN_SCAN_DISABLED != pfn_cmn->pfn_scan_state) {
		WL_PFN_ERROR(("wl%d: PFN is already active, can't service IOV_PFN_ADD_BSSID\n",
			pfn_info->wlc->pub->unit));
		return BCME_EPERM;
	}

	/* make sure alignment of 2 */
	ASSERT(!((uintptr)buf & 0x1));
	bssidptr = (wl_pfn_bssid_t *)buf;
	while (len >= sizeof (wl_pfn_bssid_t)) {
		index = bssidptr->macaddr.octet[ETHER_ADDR_LEN - 1] & 0xf;
		/* create a new node in bssid_list for new BSSID */
		if (pfn_cmn->bssidNum >= MAX_BSSID_LIST) {
			WL_PFN_ERROR(("wl%d: BSSID count %d exceeded limit of %d\n",
				pfn_info->wlc->pub->unit, pfn_cmn->bssidNum + 1, MAX_BSSID_LIST));
			return BCME_RANGE;
		}

		if (!(bssid_list_ptr = (wl_pfn_bssid_list_t *)MALLOCZ(pfn_info->wlc->osh,
		                        sizeof(wl_pfn_bssid_list_t)))) {
			WL_ERROR(("wl%d: malloc failed, size = %d\n",
			   pfn_info->wlc->pub->unit, sizeof(wl_pfn_bssid_list_t)));
			return BCME_NOMEM;
		}
		bcopy(bssidptr->macaddr.octet, bssid_list_ptr->bssid.octet, ETHER_ADDR_LEN);
		bssid_list_ptr->flags = bssidptr->flags;
		if (!(bssid_list_ptr->flags & WL_PFN_SUPPRESSLOST_MASK))
			pfn_cmn->reportloss |= ENABLE << BSSID_RPTLOSS_BIT;

		bssid_list_ptr->next = pfn_cmn->bssid_list[index];
		pfn_cmn->bssid_list[index] = bssid_list_ptr;
		pfn_cmn->bssidNum++;
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
	if (PFN_SCAN_DISABLED != pfn_info->cmn->pfn_scan_state) {
		WL_PFN_ERROR(("wl%d: PFN is already active, can't service IOV_PFN_ADD_SWC_BSSID\n",
			pfn_info->wlc->pub->unit));
		return BCME_EPERM;
	}
	/* make sure alignment of 2 */
	ASSERT(!((uintptr)buf & 0x1));
	swc_bssid_ptr = (wl_pfn_significant_bssid_t *)buf;
	mem_needed = sizeof(wl_pfn_swc_list_t) +
	             pfn_info->cmn->pfn_gscan_cfg_param->swc_rssi_window_size - 1;

	while (len >= sizeof (wl_pfn_significant_bssid_t)) {
		/* Make sure max limit is not reached */
		if (pfn_info->cmn->pfn_gscan_cfg_param->num_swc_bssid >= PFN_SWC_MAX_NUM_APS) {
			WL_PFN_ERROR(("wl%d: Greater than PFN_SWC_MAX_NUM_APS programmed!",
			pfn_info->wlc->pub->unit));
			wl_pfn_free_swc_bssid_list(pfn_info);
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
		swc_list_ptr->next = pfn_info->cmn->pfn_swc_list_hdr;
		pfn_info->cmn->pfn_swc_list_hdr = swc_list_ptr;
		swc_bssid_ptr++;
		pfn_info->cmn->pfn_gscan_cfg_param->num_swc_bssid++;
		len -= sizeof(wl_pfn_significant_bssid_t);
	}

	ASSERT(!len);

	return BCME_OK;
}

static void wl_pfn_free_swc_bssid_list(wl_pfn_info_t * pfn_info)
{
	wl_pfn_swc_list_t  *swc_list_ptr = pfn_info->cmn->pfn_swc_list_hdr;
	wl_pfn_swc_list_t *tmp;
	uint32 mem_alloced;

	mem_alloced = sizeof(wl_pfn_swc_list_t) +
	             pfn_info->cmn->pfn_gscan_cfg_param->swc_rssi_window_size - 1;

	while (swc_list_ptr) {
		tmp = swc_list_ptr->next;
		MFREE(pfn_info->wlc->osh, swc_list_ptr, mem_alloced);
		swc_list_ptr = tmp;
	}
	pfn_info->cmn->pfn_gscan_cfg_param->num_swc_bssid = 0;
	pfn_info->cmn->pfn_swc_list_hdr = NULL;
	return;
}
#endif /* GSCAN */

static int
wl_pfn_updatecfg(wl_pfn_info_t * pfn_info, wl_pfn_cfg_t * pcfg, wlc_bsscfg_t *cfg)
{
	uint16          *ptr;
	int             i, j;
	uint16		chspec;

	if (pcfg->channel_num > WL_NUMCHANNELS) {
		return BCME_RANGE;
	}

	/* validate channel list */
	for (i = 0, ptr = pcfg->channel_list; i < pcfg->channel_num; i++, ptr++) {
		if (pcfg->flags & WL_PFN_CFG_FLAGS_PROHIBITED) {
			chspec = *ptr;
			if (wf_chspec_valid(chspec) == FALSE)
				return BCME_BADARG;
		} else if (!wlc_valid_chanspec_db(pfn_info->wlc->cmi,
			CH20MHZ_CHSPEC(*ptr))) {
			return BCME_BADARG;
		}
	}
	wl_pfn_cmn_info_t *pfn_cmn = pfn_info->cmn;

	bzero(pfn_cmn->chanspec_list, sizeof(chanspec_t) * WL_NUMCHANNELS);
	pfn_cmn->chanspec_count = 0;
	if (pcfg->channel_num) {
		for (i = 0, ptr = pcfg->channel_list;
			i < pcfg->channel_num; i++, ptr++) {
			for (j = 0; j < pfn_cmn->chanspec_count; j++) {
				if (*ptr < wf_chspec_ctlchan(pfn_cmn->chanspec_list[j])) {
					/* insert new channel in the middle */
					memmove(&pfn_cmn->chanspec_list[j + 1],
					        &pfn_cmn->chanspec_list[j],
					    (pfn_cmn->chanspec_count - j) * sizeof(chanspec_t));
					pfn_cmn->chanspec_list[j] =
						CH20MHZ_CHSPEC(*ptr);
					pfn_cmn->chanspec_count++;
					break;
				} else if (*ptr == wf_chspec_ctlchan(pfn_cmn->chanspec_list[j])) {
					break;
				}
			}

	pfn_cmn->chanspec_count = (int8) pcfg->channel_num;
	ptr = pcfg->channel_list;
	for (i = 0; i < pfn_cmn->chanspec_count; i++, ptr++)
		pfn_cmn->chanspec_list[i] = CH20MHZ_CHSPEC(*ptr);
			if (j == pfn_cmn->chanspec_count) {
				/* add new channel at the end */
				pfn_cmn->chanspec_list[j] = CH20MHZ_CHSPEC(*ptr);
				pfn_cmn->chanspec_count++;
			}
		}
	}

	/* Channel list accepted and updated; accept the prohibited flag from user too */
	pfn_cmn->intflag &= ~(ENABLE << PROHIBITED_BIT);
	if (pcfg->flags & WL_PFN_CFG_FLAGS_PROHIBITED) {
		pfn_cmn->intflag |= (ENABLE << PROHIBITED_BIT);
	}

	return 0;
}

static int
wl_pfn_cfg(wl_pfn_info_t * pfn_info, wlc_bsscfg_t *cfg, void * buf, int len)
{
	wl_pfn_cfg_t        pfncfg;
	int                 err = BCME_OK;
	uint8               cur_bsscnt;

	/* Local pfncfg for size adjustment, compatibility */
	memset(&pfncfg, 0, sizeof(wl_pfn_cfg_t));
	bcopy(buf, &pfncfg, MIN(len, sizeof(wl_pfn_cfg_t)));

	if ((err = wl_pfn_updatecfg(pfn_info, &pfncfg, cfg)))
		return err;

	if (PFN_SCAN_DISABLED != pfn_info->cmn->pfn_scan_state) {
		if ((err = wl_pfn_pause_resume(pfn_info, TRUE)) < 0) {
			return err;
		}
		cur_bsscnt = pfn_info->cmn->availbsscnt;
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
		ASSERT(!pfn_info->cmn->lostcnt);
		pfn_info->cmn->foundcnt = 0;
		pfn_info->cmn->foundcnt_ssid = 0;
		pfn_info->cmn->foundcnt_bssid = 0;
		pfn_info->cmn->bestn_event_sent = FALSE;
		/* set non_cnt according to various condition */
		if (pfn_info->cmn->reporttype != pfncfg.reporttype) {
			if ((pfncfg.reporttype == WL_PFN_REPORT_ALLNET &&
				!pfn_info->cmn->reportloss) ||
				((pfncfg.reporttype == WL_PFN_REPORT_BSSIDNET) &&
				!(pfn_info->cmn->reportloss & BSSID_RPTLOSS_MASK)) ||
				((pfncfg.reporttype == WL_PFN_REPORT_SSIDNET) &&
				!(pfn_info->cmn->reportloss & SSID_RPTLOSS_MASK))) {
				pfn_info->cmn->none_cnt = NONEED_REPORTLOST;
			} else if ((pfncfg.reporttype && pfn_info->cmn->reporttype) ||
			           ((pfncfg.reporttype == WL_PFN_REPORT_ALLNET) &&
			            (pfn_info->cmn->reporttype == WL_PFN_REPORT_SSIDNET) &&
			            (pfn_info->cmn->reportloss & BSSID_RPTLOSS_MASK)) ||
			           ((pfncfg.reporttype == WL_PFN_REPORT_ALLNET) &&
			            (pfn_info->cmn->reporttype == WL_PFN_REPORT_BSSIDNET) &&
			            (pfn_info->cmn->reportloss & SSID_RPTLOSS_MASK)) ||
			           ((pfn_info->cmn->reporttype == WL_PFN_REPORT_ALLNET) &&
			            (cur_bsscnt > pfn_info->cmn->availbsscnt))) {
				pfn_info->cmn->none_cnt = 0;
			}
			pfn_info->cmn->reporttype = pfncfg.reporttype;
		}

		err = wl_pfn_pause_resume(pfn_info, FALSE);
		wl_add_timer(pfn_info->wlc->wl, pfn_info->cmn->p_pfn_timer,
			pfn_info->cmn->param->scan_freq, TRUE);
	} else {
		pfn_info->cmn->reporttype = pfncfg.reporttype;
	}

	return err;
}

#ifdef GSCAN
/* Gscan specific configs. Host may set configs or
 * just the flags.
 */
static int
wl_pfn_cfg_gscan(wl_pfn_info_t * pfn_info, void * buf, int len)
{
	int err = 0;
	int i;
	wl_pfn_gscan_cfg_t *pcfg = (wl_pfn_gscan_cfg_t *)buf;
	wl_pfn_gscan_params_t *pgscan_params = pfn_info->cmn->pfn_gscan_cfg_param;
	wl_pfn_gscan_ch_bucket_cfg_t *channel_bucket = pcfg->channel_bucket;
	wl_pfn_gscan_channel_bucket_t *ch_bucket_int;

	if (WL_GSCAN_CFG_VERSION != pcfg->version)
		return BCME_VERSION;

	pgscan_params->gscan_flags =
		pcfg->flags & (GSCAN_SEND_ALL_RESULTS_MASK | GSCAN_ALL_BUCKETS_IN_FIRST_SCAN_MASK);

	if (pcfg->flags & GSCAN_CFG_FLAGS_ONLY_MASK) {
		WL_PFN(("wl%d: wl_pfn_cfg_gscan - Only flags set\n",
		 pfn_info->wlc->pub->unit));
		return err;
	}

	if (len != GSCAN_CFG_SIZE(pcfg->count_of_channel_buckets)) {
		WL_PFN_ERROR(("wl%d: Bad params, size doesnt match expected\n",
		 pfn_info->wlc->pub->unit));
		return BCME_BADARG;
	}

	if (PFN_SCAN_DISABLED != pfn_info->cmn->pfn_scan_state) {
		WL_PFN_ERROR(("wl%d: Config change only when disabled\n",
		 pfn_info->wlc->pub->unit));
		return BCME_EPERM;
	}

	if (!pfn_info->cmn->chanspec_count) {
		WL_PFN_ERROR(("wl%d: Program channel list before gscan cfg\n",
		 pfn_info->wlc->pub->unit));
		return BCME_EPERM;
	}

	/* End index must not exceed the number of channels    */
	for (i = 0; i < pcfg->count_of_channel_buckets; i++) {
		if (channel_bucket[i].bucket_end_index >= pfn_info->cmn->chanspec_count)
			return BCME_BADARG;
	}

	if (pfn_info->cmn->pfn_gscan_cfg_param->channel_bucket) {
		MFREE(pfn_info->wlc->osh, pfn_info->cmn->pfn_gscan_cfg_param->channel_bucket,
		    (sizeof(wl_pfn_gscan_channel_bucket_t) *
		    pfn_info->cmn->pfn_gscan_cfg_param->count_of_channel_buckets));
	}

	pgscan_params->buffer_threshold = pcfg->buffer_threshold;
	pgscan_params->swc_nbssid_threshold = pcfg->swc_nbssid_threshold;
	pgscan_params->swc_rssi_window_size = pcfg->swc_rssi_window_size;
	pgscan_params->count_of_channel_buckets = pcfg->count_of_channel_buckets;
	pfn_info->cmn->pfn_scan_retry_threshold = pcfg->retry_threshold;
	pgscan_params->lost_ap_window = (uint8) pcfg->lost_ap_window;

	pgscan_params->channel_bucket = MALLOC(pfn_info->wlc->osh,
	                               (sizeof(wl_pfn_gscan_channel_bucket_t) *
	                               pgscan_params->count_of_channel_buckets));

	if (!pgscan_params->channel_bucket) {
		WL_PFN_ERROR(("wl%d: malloc failed, size = %d\n",
		   pfn_info->wlc->pub->unit,
		   (sizeof(wl_pfn_gscan_channel_bucket_t) *
		    pgscan_params->count_of_channel_buckets)));
		return BCME_NOMEM;
	}

	for (i = 0; i < pcfg->count_of_channel_buckets; i++) {
		ch_bucket_int = &(pgscan_params->channel_bucket[i]);
		ch_bucket_int->bucket_end_index = channel_bucket[i].bucket_end_index;
		ch_bucket_int->bucket_freq_multiple = channel_bucket[i].bucket_freq_multiple;
		ch_bucket_int->flag = channel_bucket[i].flag;
		ch_bucket_int->repeat = channel_bucket[i].repeat;
		ch_bucket_int->max_freq_multiple = channel_bucket[i].max_freq_multiple;
		ch_bucket_int->num_scan = 0;
		ch_bucket_int->orig_freq_multiple = ch_bucket_int->bucket_freq_multiple;
	}

	if (pfn_info->cmn->pfn_gscan_cfg_param->swc_rssi_window_size < MIN_NUM_VALID_RSSI)
		pfn_info->cmn->pfn_gscan_cfg_param->swc_rssi_window_size = MIN_NUM_VALID_RSSI;

	if (pfn_info->cmn->pfn_gscan_cfg_param->swc_rssi_window_size > PFN_SWC_RSSI_WINDOW_MAX)
		pfn_info->cmn->pfn_gscan_cfg_param->swc_rssi_window_size = PFN_SWC_RSSI_WINDOW_MAX;

	return err;
}
#endif /* GSCAN */

/* Pause/resume scan for cfg change */
static int
wl_pfn_pause_resume(wl_pfn_info_t * pfn_info, int pause)
{
	int err = BCME_OK;
	if (pause) {
		/* Stop the scanning timer */
		wl_del_timer(pfn_info->wlc->wl, pfn_info->cmn->p_pfn_timer);
	} else {
		if ((err = wl_pfn_setscanlist(pfn_info, TRUE)) < 0)
			return err;
	}
	wl_pfn_suspend(pfn_info, pause);
	if (pause) {
		err = wl_pfn_setscanlist(pfn_info, FALSE);
	}
	return err;
}

/* suspends scan */
static int
wl_pfn_suspend(wl_pfn_info_t *pfn_info, int suspend)
{
	if (suspend) {
		pfn_info->cmn->intflag |= ENABLE << SUSPEND_BIT;
		/* Abort any pending PFN scan */
		if (pfn_info->cmn->pfn_scan_state == PFN_SCAN_INPROGRESS)
			wlc_scan_abort(pfn_info->wlc->scan, WLC_E_STATUS_SUPPRESS);
	} else {
		if (!(pfn_info->cmn->intflag & (ENABLE << SUSPEND_BIT)))
			return BCME_OK;

		pfn_info->cmn->intflag &= ~(ENABLE << SUSPEND_BIT);
		if (PFN_SCAN_DISABLED == pfn_info->cmn->pfn_scan_state)
			return BCME_OK;

		/* kick off scan immediately */
		pfn_info->cmn->pfn_scan_state = PFN_SCAN_IDLE;
		wl_pfn_timer(pfn_info);
	}

	return BCME_OK;
}

#ifdef GSCAN
/* Calculates the number of bestn APs accumulted
 * across all (at most m) scans so far.
 */
static int wl_pfn_calc_num_bestn(wl_pfn_info_t * pfn_info)
{
	wl_pfn_bestnet_t *ptr = pfn_info->cmn->bestnethdr;
	wl_pfn_bestinfo_t *bestnetinfo;
	int count = 0;
	int i;

	while (ptr) {
		bestnetinfo = ptr->bestnetinfo;
		for (i = 0; i < pfn_info->cmn->param->bestn; i++) {
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

	if (PFN_VERSION != pfn_info->cmn->param->version)
		return BCME_EPERM;

	/* Make sure PFN is not in-progress */
	if (PFN_SCAN_IDLE != pfn_info->cmn->pfn_scan_state) {
		WL_PFN_ERROR(("wl%d: report pfnbest only during PFN_SCAN_IDLE\n",
			pfn_info->wlc->pub->unit));
		return BCME_EPERM;
	}

	/* make sure alignment of 4 */
	ASSERT(!((uintptr)buf & 0x3));

	pfnbest = (wl_pfn_scanresults_t *)buf;
	pfnbest->version = PFN_SCANRESULTS_VERSION;
	pfnbest->count = 0;
	pfnbestnet = pfnbest->netinfo;
	/* pre-calculate until which scan result this report is going to end */
	buflen -= sizeof(wl_pfn_scanresults_t) - sizeof(wl_pfn_net_info_t);
	scancnt = buflen / (sizeof(wl_pfn_net_info_t) * pfn_info->cmn->param->bestn);
	cnt = 0;
	while (cnt < scancnt && pfn_info->cmn->bestnethdr) {
		bestnetinfo = pfn_info->cmn->bestnethdr->bestnetinfo;
		/* get the bestn from one scan */
		for (i = 0; i < pfn_info->cmn->param->bestn; i++) {
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
		ptr = pfn_info->cmn->bestnethdr;
		pfn_info->cmn->bestnethdr = pfn_info->cmn->bestnethdr->next;
		MFREE(pfn_info->wlc->osh, ptr, pfn_info->cmn->bestnetsize);
		ASSERT(pfn_info->cmn->numofscan);
		pfn_info->cmn->numofscan--;
		cnt++;
	}
	ASSERT(buflen >= 0);
	if (!pfn_info->cmn->bestnethdr) {
		pfnbest->status = PFN_COMPLETE;
		pfn_info->cmn->bestnettail = NULL;
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

	if (PFN_VERSION != pfn_info->cmn->param->version)
		return BCME_EPERM;

	/* Make sure PFN is not disabled */
	if (PFN_SCAN_DISABLED == pfn_info->cmn->pfn_scan_state) {
		WL_PFN_ERROR(("wl%d: In PFN_SCAN_DISABLED state, no results here\n",
			pfn_info->wlc->pub->unit));
		return BCME_EPERM;
	}

	/* make sure alignment of 4 */
	ASSERT(!((uintptr)buf & 0x3));

	pfnbest = (wl_pfn_lscanresults_t *)buf;
	pfnbest->version = PFN_LBEST_SCAN_RESULT_VERSION;
	pfnbest->count = 0;
	pfnbestnet = pfnbest->netinfo;
	/* pre-calculate until which scan result this report is going to end */
	buflen -= sizeof(wl_pfn_lscanresults_t) - sizeof(wl_pfn_lnet_info_t);
	scancnt = buflen / (sizeof(wl_pfn_lnet_info_t) * pfn_info->cmn->param->bestn);
	scancnt = MIN(scancnt, MAX_CHBKT_PER_RESULT);

	if (!scancnt) {
		WL_PFN_ERROR(("wl%d: Cannot fit in a single scans' results in buffer\n",
			pfn_info->wlc->pub->unit));
		return BCME_BUFTOOSHORT;
	}
	cnt = 0;
	while (cnt < scancnt && pfn_info->cmn->bestnethdr) {
		bestnetinfo = pfn_info->cmn->bestnethdr->bestnetinfo;
		pfnbest->scan_ch_buckets[cnt] = pfn_info->cmn->bestnethdr->scan_ch_bucket;
		if (bestnetinfo->flags & PFN_PARTIAL_SCAN_MASK)
			partial = TRUE;
		else
			partial = FALSE;
		/* get the bestn from one scan */
		for (i = 0; i < pfn_info->cmn->param->bestn; i++) {
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
		ptr = pfn_info->cmn->bestnethdr;
		pfn_info->cmn->bestnethdr = pfn_info->cmn->bestnethdr->next;
		MFREE(pfn_info->wlc->osh, ptr, pfn_info->cmn->bestnetsize);
		ASSERT(pfn_info->cmn->numofscan);
		pfn_info->cmn->numofscan--;
		cnt++;
	}
	ASSERT(buflen >= 0);
	if (!pfn_info->cmn->bestnethdr) {
		pfnbest->status = PFN_COMPLETE;
		pfn_info->cmn->bestnettail = NULL;
		pfn_info->cmn->bestn_event_sent = FALSE;
	} else {
		pfnbest->status = PFN_INCOMPLETE;
	}
	return 0;
}

static void
wl_pfn_free_ssidlist(wl_pfn_info_t * pfn_info, bool freeall, bool clearjustfoundonly)
{
	int i;
	wl_pfn_internal_t *ptr;

	for (i = 0; i < pfn_info->cmn->count; i++) {
		ptr = pfn_info->cmn->pfn_internal[i];
		if (freeall) {
			uint32 mem_needed = WL_PFN_INTERNAL_FIXED_LEN;

			if (ptr->flags & WL_PFN_SSID_IMPRECISE_MATCH) {
				mem_needed += sizeof(uint32);
			} else {
				mem_needed += (ptr->ssid_data.ssid.SSID_len + 1);
			}
			MFREE(pfn_info->wlc->osh, ptr, mem_needed);
			pfn_info->cmn->pfn_internal[i] = NULL;
		} else {
			if (!clearjustfoundonly ||
				(ptr->flags & PFN_SSID_JUST_FOUND)) {
				if (!(ptr->flags & WL_PFN_SUPPRESSLOST_MASK) &&
					(ptr->flags &
					(PFN_SSID_JUST_FOUND | PFN_SSID_ALREADY_FOUND))) {
					ASSERT(pfn_info->cmn->availbsscnt);
					pfn_info->cmn->availbsscnt--;
				}
				SET_SSID_PFN_STATE(ptr->flags, PFN_SSID_NOT_FOUND);
				bzero(&ptr->bssid, ETHER_ADDR_LEN);
			}
		}
	}
	if (freeall)
		pfn_info->cmn->count = 0;
}

static void
wl_pfn_free_bssidlist(wl_pfn_info_t * pfn_info, bool freeall, bool clearjustfoundonly)
{
	int i;
	wl_pfn_bssid_list_t *bssid_list, *ptr;

	for (i = 0; i < BSSIDLIST_MAX; i++) {
		bssid_list = pfn_info->cmn->bssid_list[i];
		while (bssid_list) {
			if (bssid_list->pbssidinfo) {
				if (!clearjustfoundonly ||
				    (bssid_list->pbssidinfo->network_found ==
				      PFN_NET_JUST_FOUND)) {
					MFREE(pfn_info->wlc->osh, bssid_list->pbssidinfo,
					      sizeof(wl_pfn_bssidinfo_t));
					bssid_list->pbssidinfo = NULL;
					if (!(bssid_list->flags & WL_PFN_SUPPRESSLOST_MASK)) {
						ASSERT(pfn_info->cmn->availbsscnt);
						pfn_info->cmn->availbsscnt--;
					}
				}
			}
			ptr = bssid_list;
			bssid_list = bssid_list->next;
			if (freeall)
				MFREE(pfn_info->wlc->osh, ptr, sizeof(wl_pfn_bssid_list_t));
		}
		if (freeall)
			pfn_info->cmn->bssid_list[i] = NULL;
	}
	if (freeall)
		pfn_info->cmn->bssidNum = 0;
}

static void
wl_pfn_free_bestnet(wl_pfn_info_t * pfn_info)
{
	wl_pfn_bestnet_t *bestnet;

	/* clear bestnet */
	while (pfn_info->cmn->bestnethdr != pfn_info->cmn->bestnettail) {
		bestnet = pfn_info->cmn->bestnethdr;
		pfn_info->cmn->bestnethdr = pfn_info->cmn->bestnethdr->next;
		MFREE(pfn_info->wlc->osh, bestnet, pfn_info->cmn->bestnetsize);
	}
	if (pfn_info->cmn->bestnethdr) {
		MFREE(pfn_info->wlc->osh, pfn_info->cmn->bestnethdr, pfn_info->cmn->bestnetsize);
		pfn_info->cmn->bestnethdr = pfn_info->cmn->bestnettail = NULL;
	}
	pfn_info->cmn->numofscan = 0;
	/* clear current bestn */
	if (pfn_info->cmn->current_bestn) {
		MFREE(pfn_info->wlc->osh, pfn_info->cmn->current_bestn, pfn_info->cmn->bestnetsize);
		pfn_info->cmn->current_bestn = NULL;
	}
}

static int
wl_pfn_setscanlist(wl_pfn_info_t *pfn_info, int enable)
{
	int i, j;
	uint8 directcnt;
	wl_pfn_cmn_info_t *pfn_cmn = pfn_info->cmn;
	if (enable) {
		/* find # of directed scans */
		if (pfn_cmn->reporttype == WL_PFN_REPORT_BSSIDNET)
			directcnt = 0;
		else
			directcnt = pfn_cmn->hiddencnt;

		ASSERT(!pfn_cmn->ssid_list);
		pfn_cmn->ssidlist_len = (directcnt + 1) * sizeof(wlc_ssid_t);
		if (!(pfn_cmn->ssid_list = MALLOC(pfn_info->wlc->osh,
		     pfn_cmn->ssidlist_len))) {
			WL_PFN_ERROR(("wl%d: ssid list allocation failed. %d bytes\n",
				pfn_info->wlc->pub->unit, (directcnt + 1) * sizeof(wlc_ssid_t)));
			pfn_cmn->ssidlist_len = 0;
			if (pfn_cmn->param->mscan && pfn_cmn->numofscan &&
				(pfn_cmn->param->flags & REPORT_SEPERATELY_MASK))
				wl_pfn_send_event(pfn_info, NULL, 0, WLC_E_PFN_BEST_BATCHING);
			return BCME_NOMEM;
		}
		/* copy hidden ssid to ssid list(s) */
		for (i = j = 0; i < pfn_cmn->count && j < directcnt; i++) {
			if (pfn_cmn->pfn_internal[i]->flags & WL_PFN_HIDDEN_MASK) {
				memcpy(&pfn_cmn->ssid_list[j].SSID,
					&pfn_cmn->pfn_internal[i]->ssid_data.ssid.SSID,
					pfn_cmn->pfn_internal[i]->ssid_data.ssid.SSID_len);
				pfn_cmn->ssid_list[j++].SSID_len =
					pfn_cmn->pfn_internal[i]->ssid_data.ssid.SSID_len;
			}
		}
		/* add 0 length SSID to end of ssid list, if broadcast scan is needed */
		if ((pfn_cmn->hiddencnt < pfn_cmn->count) || pfn_cmn->bssidNum ||
			(pfn_cmn->param->flags & (ENABLE_BKGRD_SCAN_MASK | ENABLE_BD_SCAN_MASK))) {
			/* broadcast scan needed and used last */
			pfn_cmn->ssid_list[j].SSID_len = 0;
		}
	} else {
		/* Free ssid list for directed scan */
		if (pfn_cmn->ssid_list != NULL) {
			MFREE(pfn_info->wlc->osh, pfn_cmn->ssid_list,
			      pfn_cmn->ssidlist_len);
			pfn_cmn->ssid_list = NULL;
		}
	}

	return 0;
}

static int
wl_pfn_enable(wl_pfn_info_t * pfn_info, int enable)
{
	int err;
	wl_pfn_cmn_info_t *pfn_cmn = pfn_info->cmn;
	/* If the request to enable make sure we have required data from user/host */
	if (enable && (PFN_SCAN_DISABLED == pfn_cmn->pfn_scan_state)) {

		/* Check to see if the user has provided correct set of parameters by checking
		* version field. Also 'pfn_cmn->count or pfn_cmn->bssidNum' being zero is
		* vaild parameter, if user wants to just turn on back ground scanning or
		* broadcast scan without PFN list
		*/
		if (!pfn_cmn->param->version ||
		    (!pfn_cmn->count && !pfn_cmn->bssidNum &&
		!(pfn_cmn->param->flags & (ENABLE_BD_SCAN_MASK | ENABLE_BKGRD_SCAN_MASK)))) {
			WL_PFN_ERROR(("wl%d: Incomplete parameter setting\n",
			           pfn_info->wlc->pub->unit));
			return BCME_BADOPTION;
		}
		/* current_bestn expected NULL */
		if (pfn_cmn->current_bestn)
			WL_PFN_ERROR(("current_bestn not empty!\n"));

		if ((err = wl_pfn_setscanlist(pfn_info, enable)))
			return err;

		/* Were channel buckets programmed?
		 * If not this is a regular pfn scan, so sort and
		 * remove duplicate chanspecs here
		 */
#ifdef GSCAN
		if (!GSCAN_ENAB(pfn_info->wlc->pub) ||
		        !pfn_cmn->pfn_gscan_cfg_param->count_of_channel_buckets)
#endif /* GSCAN */
		{
			pfn_cmn->chanspec_count =
			         wl_pfn_sort_remove_dup_chanspec(pfn_cmn->chanspec_list,
			                                         pfn_cmn->chanspec_count);
		}

		if ((pfn_cmn->reporttype == WL_PFN_REPORT_ALLNET &&
			!pfn_cmn->reportloss) ||
			((pfn_cmn->reporttype == WL_PFN_REPORT_BSSIDNET) &&
			!(pfn_cmn->reportloss & BSSID_RPTLOSS_MASK)) ||
			((pfn_cmn->reporttype == WL_PFN_REPORT_SSIDNET) &&
			!(pfn_cmn->reportloss & SSID_RPTLOSS_MASK))) {
			pfn_cmn->none_cnt = NONEED_REPORTLOST;
		} else {
			pfn_cmn->none_cnt = 0;
		}

		/* Start the scanning timer */
		pfn_cmn->pfn_scan_state = PFN_SCAN_IDLE;
		pfn_cmn->pfn_scan_retries = 0;
		wl_add_timer(pfn_info->wlc->wl, pfn_cmn->p_pfn_timer,
			pfn_cmn->param->scan_freq, TRUE);

		/* If immediate scan enabled,
		* kick off the first scan as soon as PFN is enabled,
		* rather than wait till first interval to elapse
		*/
		if (((pfn_cmn->param->flags & IMMEDIATE_SCAN_MASK) >>
			IMMEDIATE_SCAN_BIT) == ENABLE) {
			wl_pfn_timer(pfn_info);
		}
	}

	/* Handle request to disable */
	if (!enable && (PFN_SCAN_DISABLED != pfn_cmn->pfn_scan_state)) {
		/* Stop the scanning timer */
		wl_del_timer(pfn_info->wlc->wl, pfn_cmn->p_pfn_timer);
		/* Abort any pending PFN scan */
		if (pfn_cmn->pfn_scan_state == PFN_SCAN_INPROGRESS) {
			wlc_scan_abort(pfn_info->wlc->scan, WLC_E_STATUS_ABORT);
		}
		pfn_cmn->pfn_scan_state = PFN_SCAN_DISABLED;

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
		if (GSCAN_ENAB(pfn_info->wlc->pub)) {
			wl_pfn_free_swc_bssid_list(pfn_info);
			wl_pfn_reset_ch_bucket(pfn_info, TRUE);
		}
#endif /* GSCAN */
		pfn_cmn->bestn_event_sent = FALSE;
		/* reset none_cnt, available network count, found count, lost count */
		pfn_cmn->none_cnt = 0;
		pfn_cmn->availbsscnt = 0;
		pfn_cmn->foundcnt = 0;
		pfn_cmn->lostcnt = 0;
		pfn_cmn->foundcnt_ssid = 0;
		pfn_cmn->foundcnt_bssid = 0;
		pfn_cmn->lostcnt_ssid = 0;
		pfn_cmn->lostcnt_bssid = 0;
		pfn_cmn->pfn_scan_retries = 0;
		pfn_cmn->pfn_scan_retry_threshold = PFN_DEFAULT_RETRY_SCAN;
	}

	return BCME_OK;
}

static int
wl_pfn_clear(wl_pfn_info_t * pfn_info)
{
	int i;
	wl_pfn_cmn_info_t *pfn_cmn = pfn_info->cmn;
	/* Make sure PFN is not active already */
	if (PFN_SCAN_DISABLED != pfn_cmn->pfn_scan_state) {
		WL_PFN_ERROR(("wl%d: PFN is already active, can't service IOV_PFN_CLEAR\n",
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
	if (GSCAN_ENAB(pfn_info->wlc->pub)) {
		/* Free significant bssid list */
		wl_pfn_free_swc_bssid_list(pfn_info);

		if (pfn_cmn->pfn_gscan_cfg_param->channel_bucket) {
			MFREE(pfn_info->wlc->osh, pfn_cmn->pfn_gscan_cfg_param->channel_bucket,
			 (sizeof(wl_pfn_gscan_channel_bucket_t) *
			 pfn_cmn->pfn_gscan_cfg_param->count_of_channel_buckets));
		}
		memset(pfn_cmn->pfn_gscan_cfg_param, 0, sizeof(*pfn_cmn->pfn_gscan_cfg_param));
	}
#endif /* GSCAN */
	pfn_cmn->bestn_event_sent = FALSE;
	/* Zero out the param */
	bzero(pfn_cmn->param, sizeof(wl_pfn_param_t));
	if (pfn_cmn->pfn_ssid_ext_cfg) {
		MFREE(pfn_info->wlc->osh, pfn_cmn->pfn_ssid_ext_cfg,
				sizeof(wl_ssid_ext_params_t));
		pfn_cmn->pfn_ssid_ext_cfg = NULL;
	}
	pfn_cmn->associated_idx = 0;
	/* Disable before calling 'wl_pfn_clear' */
	pfn_cmn->pfn_scan_state = PFN_SCAN_DISABLED;

	pfn_cmn->numofscan = 0;
	pfn_cmn->pfn_scan_retries = 0;
	pfn_cmn->pfn_scan_retry_threshold = PFN_DEFAULT_RETRY_SCAN;
	pfn_cmn->adaptcnt = 0;
	pfn_cmn->foundcnt = pfn_cmn->lostcnt = 0;
	pfn_cmn->foundcnt_ssid = pfn_cmn->lostcnt_ssid = 0;
	pfn_cmn->foundcnt_bssid = pfn_cmn->lostcnt_bssid = 0;
	/* clear channel spec list */
	for (i = 0; i < WL_NUMCHANNELS; i++) {
		pfn_cmn->chanspec_list [i] = 0;
	}
	pfn_cmn->chanspec_count = 0;
	pfn_cmn->bestnetsize = 0;
	pfn_cmn->hiddencnt = 0;
	pfn_cmn->currentadapt = 0;
	pfn_cmn->reporttype = 0;
	pfn_cmn->availbsscnt = 0;
	pfn_cmn->none_cnt = 0;
	pfn_cmn->reportloss = 0;

	/* Clear the pfn_mac */
	memset(&pfn_cmn->pfn_mac, 0, ETHER_ADDR_LEN);
	pfn_cmn->intflag &= ~(ENABLE << PFN_MACADDR_BIT);
	pfn_cmn->intflag &= ~(ENABLE << PFN_ONLY_MAC_OUI_SET_BIT);
	pfn_cmn->intflag &= ~(ENABLE << PFN_MACADDR_UNASSOC_ONLY_BIT);
	pfn_cmn->intflag &= ~(ENABLE << PFN_RESTRICT_LA_MACADDR_BIT);

	return BCME_OK;
}

static void
wl_pfn_free_bdcast_list(wl_pfn_info_t * pfn_info)
{
	struct wl_pfn_bdcast_list *bdcast_node;

	/* Free the entire linked list */
	while (pfn_info->cmn->bdcast_list) {
		bdcast_node = pfn_info->cmn->bdcast_list;
		pfn_info->cmn->bdcast_list = pfn_info->cmn->bdcast_list->next;
		MFREE(pfn_info->wlc->osh, bdcast_node, sizeof(struct wl_pfn_bdcast_list));
	}
	pfn_info->cmn->bdcastnum = 0;
}

static void
wl_pfn_cipher2wsec(uint8 ucount, uint8 * unicast, uint16 * wsec)
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
		default:
			ASSERT(0);
			break;
		}
	}
	return;
}

#ifdef GSCAN
int
wlc_send_pfn_full_scan_result(wlc_info_t *wlc, wlc_bss_info_t *BSS, wlc_bsscfg_t *cfg,
                        struct dot11_management_header *hdr)
{
	int ret = BCME_OK;
	wl_gscan_result_t *gscan_result;
	uint32 gscan_result_len;
	wl_gscan_bss_info_t *gscan_bssinfo;

	gscan_result_len = sizeof(wl_gscan_result_t) + BSS->bcn_prb_len +
		sizeof(wl_bss_info_t);

	if ((gscan_result = (wl_gscan_result_t *)
		MALLOCZ(wlc->osh, gscan_result_len)) == NULL) {
		WL_ERROR((WLC_BSS_MALLOC_ERR, WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg), __FUNCTION__,
			(int)gscan_result_len, MALLOCED(wlc->osh)));
		ret = BCME_NOMEM;
		goto exit;
	}

	gscan_bssinfo = (wl_gscan_bss_info_t*)gscan_result->bss_info;
	if (wlc_bss2wl_bss(wlc, BSS, &gscan_bssinfo->info,
	          sizeof(wl_bss_info_t) + BSS->bcn_prb_len, TRUE) != BCME_OK) {
		WL_PFN_ERROR(("gscan: results buffer too short %s()\n", __FUNCTION__));
		ret = BCME_ERROR;
	} else {
		gscan_result->version = WL_GSCAN_FULL_RESULT_VERSION;
		gscan_result->buflen = WL_GSCAN_RESULTS_FIXED_SIZE +
		                       WL_GSCAN_INFO_FIXED_FIELD_SIZE +
		                       gscan_bssinfo->info.length;
		gscan_result->scan_ch_bucket = wl_pfn_get_cur_ch_buckets(wlc->pfn);
		gscan_bssinfo->timestamp[0] = BSS->bcn_prb->timestamp[0];
		gscan_bssinfo->timestamp[1] = BSS->bcn_prb->timestamp[1];
		wlc_bss_mac_event(wlc, cfg, WLC_E_PFN_GSCAN_FULL_RESULT, &hdr->sa, 0,
		                 0, 0, gscan_result, gscan_result->buflen);
	}
	MFREE(wlc->osh, gscan_result, gscan_result_len);
exit:
	return ret;
}
#endif /* GSCAN */

static int
wl_pfn_sendonenet(wl_pfn_info_t *pfn_info, wlc_bss_info_t * bi, uint32 now)
{
	wlc_info_t *wlc = pfn_info->wlc;
	wl_pfn_scanresult_t *result;
	wl_bss_info_t *pfn_bi;
	uint evt_data_size = sizeof(wl_pfn_scanresult_t) + sizeof(wl_bss_info_t);

	if (bi->bcn_prb_len > DOT11_BCN_PRB_LEN) {
		evt_data_size += ROUNDUP(bi->bcn_prb_len - DOT11_BCN_PRB_LEN, 4);
	}

	result = (wl_pfn_scanresult_t *)MALLOCZ(wlc->osh,
		evt_data_size);
	if (!result) {
		WL_INFORM(("wl%d: found result allocation failed\n", WLCWLUNIT(wlc)));
		return BCME_NOMEM;
	}

	result->version = PFN_SCANRESULT_VERSION;
	result->status = PFN_COMPLETE;
	result->count = 1;
	bcopy(bi->BSSID.octet, result->netinfo.pfnsubnet.BSSID.octet, ETHER_ADDR_LEN);
	result->netinfo.pfnsubnet.channel = wf_chspec_ctlchan(bi->chanspec);
	result->netinfo.pfnsubnet.SSID_len = bi->SSID_len;
	bcopy(bi->SSID, result->netinfo.pfnsubnet.u.SSID, bi->SSID_len);
	result->netinfo.RSSI = bi->RSSI;
	/* timestamp is only meaningful when reporting best network info,
	   but not useful when reporting netfound
	*/
	result->netinfo.timestamp = 0;

	/* now put the bss_info in - above alloc should guarantee success */
	pfn_bi = (wl_bss_info_t*)result->bss_info;
	if (wlc_bss2wl_bss(wlc, bi, pfn_bi, evt_data_size, TRUE) !=
			BCME_OK) {
		/* above alloc logic should ensure no failure */
		ASSERT(!"wlc_bss2wl_bss call failed\n");
	}

	if (BCME_OK != wl_pfn_send_event(pfn_info, (void *)result,
		evt_data_size, WLC_E_PFN_NET_FOUND)) {
		WL_PFN_ERROR(("wl%d: send event fails\n",
		          pfn_info->wlc->pub->unit));
		MFREE(pfn_info->wlc->osh, result, evt_data_size);
		return BCME_ERROR;
	}
	MFREE(pfn_info->wlc->osh, result, evt_data_size);

	return BCME_OK;
}

static void
wl_pfn_calc_auth_wsec(wlc_bss_info_t *bi, uint16 *wpa_auth, uint16 *wsec)
{
	uint16 scanresult_wpa_auth = WPA_AUTH_DISABLED;
	uint16 scanresult_wsec = 0;
	int k;

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
			(scanresult_wpa_auth == WPA_AUTH_DISABLED)) {
		scanresult_wsec = WEP_ENABLED;
	}
	*wpa_auth = scanresult_wpa_auth;
	*wsec = scanresult_wsec;
	return;
}

#define BASE_RSSI_SCORE_BIAS	85
#define PFN_CALC_BASE_RSSI_SCORE(rssi)	(((int32)(rssi) + BASE_RSSI_SCORE_BIAS) * 4)

static int32
wl_pfn_calc_rssi_score(wl_pfn_info_t *pfn_info, wlc_bss_info_t *bi, wlc_bsscfg_t *cur_cfg,
		bool is_secure, bool is_same_network, bool is_cur_bssid)
{
	wl_ssid_ext_params_t *pfn_ssid_ext_cfg = pfn_info->cmn->pfn_ssid_ext_cfg;
	int32 rssi_score;
	bool is_5g;
	is_5g = CHSPEC_IS5G(bi->chanspec);
	rssi_score = PFN_CALC_BASE_RSSI_SCORE(is_cur_bssid ?
			(int32)(cur_cfg->link->rssi) : bi->RSSI);
	rssi_score = MIN(rssi_score, (int32)pfn_ssid_ext_cfg->init_score_max);
	rssi_score += is_cur_bssid ? (int32)pfn_ssid_ext_cfg->cur_bssid_bonus: 0;
	rssi_score += is_same_network ? (int32)pfn_ssid_ext_cfg->same_ssid_bonus: 0;
	rssi_score += is_secure ? (int32)pfn_ssid_ext_cfg->secure_bonus: 0;
	rssi_score += is_5g ? (int32)pfn_ssid_ext_cfg->band_5g_bonus: 0;
	return rssi_score;
}

static bool
is_pfn_ssid_match(wl_pfn_internal_t *pfn_internal, wlc_bss_info_t *bi)
{
	uint32 bi_crc32;

	if (pfn_internal->flags & WL_PFN_SSID_IMPRECISE_MATCH) {
		bi_crc32 = hndcrc32(bi->SSID, bi->SSID_len, CRC32_INIT_VALUE);
		return (bi_crc32 == pfn_internal->ssid_data.ssid_crc32);
	} else {
		return wlc_ssid_cmp(pfn_internal->ssid_data.ssid.SSID,
				bi->SSID, pfn_internal->ssid_data.ssid.SSID_len,
				bi->SSID_len);
	}
}

static bool
is_ssid_processing_allowed(wl_pfn_info_t *pfn_info, wl_pfn_internal_t *pfn_internal,
		wlc_bss_info_t *bi, uint32 now)
{
	bool ret = FALSE, is_bg = CHSPEC_IS2G(bi->chanspec);
	wlc_info_t *wlc = pfn_info->wlc;
	int8 rssi = (int8)bi->RSSI;
	uint32 flags = pfn_internal->flags;
	bool trig_a = ((flags & WL_PFN_SSID_A_BAND_TRIG) != 0);
	bool trig_bg = ((flags & WL_PFN_SSID_BG_BAND_TRIG) != 0);
	int8 rssi_thresh = pfn_internal->rssi_thresh;
	uint16 scanresult_wpa_auth, scanresult_wsec;
	wl_ssid_ext_params_t *pfn_ssid_ext_cfg = pfn_info->cmn->pfn_ssid_ext_cfg;
	wlc_bsscfg_t *cfg;
	wlc_info_t *wlc_iter;
	int idx, wlc_idx;

	/* If associated no point in reporting cur BSSID, because in
	 * the associated case the results are compared with the
	 * cur BSSID and decision is based on the RSSI score.
	 * Update:-
	 * In RSDB scenario, we need to check accross wlc for association.
	 * Also this return false skips time stamp updation in process_scan_result
	 * and leads to PFN_NET_LOST.
	 * Time stamp updation added here.
	 */
	FOREACH_WLC(wlc->cmn, wlc_idx, wlc_iter) {
		FOREACH_AS_BSS(wlc_iter, idx, cfg) {
			if (memcmp(&cfg->current_bss->BSSID.octet,
				&bi->BSSID.octet, ETHER_ADDR_LEN) == 0) {
				pfn_internal->time_stamp = now;
				WL_PFN(("wl%d time stamp updated for connected SSID \n",
					pfn_info->wlc->pub->unit));
				return FALSE;
			}
		}
	}

	wl_pfn_calc_auth_wsec(bi, &scanresult_wpa_auth, &scanresult_wsec);

	/* Bail if requested security match fails */
	if (pfn_internal->wpa_auth != WPA_AUTH_PFN_ANY) {
		if (pfn_internal->wpa_auth) {
			if (!(pfn_internal->wpa_auth & scanresult_wpa_auth))
				return FALSE;
			if (pfn_internal->wsec &&
				!(pfn_internal->wsec & scanresult_wsec))
				return FALSE;
		}
		else if (!scanresult_wpa_auth) {
			if (pfn_internal->wsec &&
				!(pfn_internal->wsec & scanresult_wsec))
				return FALSE;
			if (!pfn_internal->wsec && scanresult_wsec)
				return FALSE;
		}
		else {
			return FALSE;
		}
	}

	/* Bail if requested imode match fails */
	if (((flags & PFN_SSID_INFRA) && (bi->capability & DOT11_CAP_IBSS)) ||
		(!(flags & PFN_SSID_INFRA) && (bi->capability & DOT11_CAP_ESS))) {
		return FALSE;
	}
	/* Has per band rssi threshold been set? */
	/* Overrides only if per network threshold isnt set */
	if (pfn_ssid_ext_cfg && !rssi_thresh) {
		rssi_thresh = is_bg ?
			pfn_ssid_ext_cfg->min2G_rssi:
			pfn_ssid_ext_cfg->min5G_rssi;
	}
	/* BSSID must have min rssi if said threshold is set */
	if (!rssi_thresh || rssi >= rssi_thresh) {
		ret = ((trig_a && !is_bg) || (trig_bg && is_bg) ||
			(!trig_a && !trig_bg));
	}
	if (!ret) {
		WL_PFN(("RSSI %d thresh %d chan %d A-trig %x BG-trig %x\n", rssi,
			rssi_thresh, wf_chspec_ctlchan(bi->chanspec), trig_a, trig_bg));
		return FALSE;
	}
	/* If in associated state, is the RSSI score of the scan result
	 * better than that of the cur BSSID?
	 */
	if (pfn_ssid_ext_cfg) {
		FOREACH_WLC(wlc->cmn, wlc_idx, wlc_iter) {
			FOREACH_BSS(wlc_iter, idx, cfg) {
			   if (wlc_bss_connected(cfg)) {
				uint16 cur_wpa_auth, cur_wsec;
				int32 cur_bssid_rssi_score, my_score;
				bool is_secure;

				wl_pfn_calc_auth_wsec(cfg->current_bss, &cur_wpa_auth, &cur_wsec);
				is_secure = (cur_wpa_auth != WPA_AUTH_DISABLED &&
					cur_wpa_auth != WPA_AUTH_NONE);
				cur_bssid_rssi_score  = wl_pfn_calc_rssi_score(pfn_info,
					cfg->current_bss, cfg, is_secure, FALSE, TRUE);
				is_secure = (scanresult_wpa_auth != WPA_AUTH_DISABLED &&
					scanresult_wpa_auth != WPA_AUTH_NONE);
				my_score = wl_pfn_calc_rssi_score(pfn_info, bi, cfg, is_secure,
					((pfn_internal->flags & WL_PFN_SSID_SAME_NETWORK) != 0),
					FALSE);
				WL_PFN(("rssi %d, score %d > %d? is_secure %d\n", rssi,
					my_score, cur_bssid_rssi_score, is_secure));
				return (my_score > cur_bssid_rssi_score);
			   }
			}
		}
	}
	return ret;
}

static uint32
wl_pfn_get_cur_ch_buckets(wl_pfn_info_t *pfn_info)
{
	return pfn_info->cmn->pfn_gscan_cfg_param->cur_ch_bucket_active;
}

void
wl_pfn_process_scan_result(wl_pfn_info_t* pfn_info, wlc_bss_info_t * bi)
{
	int k;
	uint8 *ssid;
	uint8 ssid_len;
	wl_pfn_internal_t *pfn_internal = NULL;
	wl_pfn_bdcast_list_t *bdcast_node_list;
	uint32	now;
	struct ether_addr *bssid;
	wl_pfn_bssid_list_t *bssidlist = NULL;
	wl_pfn_bestinfo_t *bestptr;
	int16 oldrssi, rssidiff;
	uint8 index, chan;
	int retval;
#ifdef BCMDBG
	char ssidbuf[SSID_FMT_BUF_LEN];
#endif /* BCMDBG */

	ASSERT(pfn_info);
	wl_pfn_cmn_info_t *pfn_cmn = pfn_info->cmn;

	now = OSL_SYSUPTIME();

	ssid = bi->SSID;
	ssid_len = bi->SSID_len;
	bssid = &bi->BSSID;
	chan = wf_chspec_ctlchan(bi->chanspec);

	WL_TRACE(("wl%d: Scan result: RSSI = %d, SSID = %s\n",
		pfn_info->wlc->pub->unit, bi->RSSI,
		(wlc_format_ssid(ssidbuf, ssid, ssid_len), ssidbuf)));

	/* see if this AP is one of the bestn */
	/* best network is not limited to on channel */
	/* In case of Gscan, AP channel must belong to    */
	/* a currently scanned Gscan channel bucket       */
	if (pfn_cmn->current_bestn && (!GSCAN_ENAB(pfn_info->wlc->pub) ||
	    !pfn_cmn->pfn_gscan_cfg_param->count_of_channel_buckets ||
	    wl_pfn_is_ch_bucket_flag_enabled(pfn_info, chan, CH_BUCKET_GSCAN))) {
		bestptr = wl_pfn_get_best_networks(pfn_info,
		     pfn_cmn->current_bestn->bestnetinfo, bi->RSSI, bi->BSSID);
		if (bestptr) {
			bcopy(bi->BSSID.octet, bestptr->pfnsubnet.BSSID.octet, ETHER_ADDR_LEN);
			bestptr->pfnsubnet.SSID_len = ssid_len;
			bcopy(ssid, bestptr->pfnsubnet.u.SSID, bestptr->pfnsubnet.SSID_len);
			bestptr->pfnsubnet.channel = chan;
			bestptr->RSSI = bi->RSSI;
			bestptr->timestamp = now;
		}
	}
#ifdef GSCAN
	/* If a significant change BSSID list is configured process and update */
	if (GSCAN_ENAB(pfn_info->wlc->pub))
		wl_pfn_process_swc_bssid(pfn_info, bi);
#endif /* GSCAN */
	/* check BSSID list in pfn_info and see if any match there */
	if ((pfn_cmn->reporttype == WL_PFN_REPORT_BSSIDNET) ||
		(pfn_cmn->reporttype == WL_PFN_REPORT_ALLNET)) {
		index = bssid->octet[ETHER_ADDR_LEN -1] & 0xf;
		bssidlist = pfn_cmn->bssid_list[index];
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
						WL_PFN_ERROR(("wl%d: pbssidinfo MALLOC failed\n",
						          pfn_info->wlc->pub->unit));
						if (pfn_cmn->param->mscan && pfn_cmn->numofscan &&
						  (pfn_cmn->param->flags & REPORT_SEPERATELY_MASK))
							wl_pfn_send_event(pfn_info,
							      NULL, 0, WLC_E_PFN_BEST_BATCHING);
						break;
					}
					bssidlist->pbssidinfo->ssid.SSID_len = ssid_len;
					bcopy(ssid, bssidlist->pbssidinfo->ssid.SSID, ssid_len);
					bssidlist->pbssidinfo->rssi = bi->RSSI;
					if (!(bssidlist->flags &
					 (WL_PFN_SUPPRESSFOUND_MASK | WL_PFN_SUPPRESSLOST_MASK))) {
						bssidlist->pbssidinfo->network_found =
						                               PFN_NET_JUST_FOUND;
						pfn_cmn->foundcnt++;
						pfn_cmn->foundcnt_bssid++;
						pfn_cmn->availbsscnt++;
					} else {
						if (!(bssidlist->flags &
						      WL_PFN_SUPPRESSFOUND_MASK)) {
							bssidlist->pbssidinfo->network_found =
							                       PFN_NET_JUST_FOUND;
							pfn_cmn->foundcnt++;
							pfn_cmn->foundcnt_bssid++;
						} else {
							bssidlist->pbssidinfo->network_found =
							                    PFN_NET_ALREADY_FOUND;
							if (!(bssidlist->flags &
							      WL_PFN_SUPPRESSLOST_MASK))
								pfn_cmn->availbsscnt++;
						}
					}
				} else if (bssidlist->pbssidinfo &&
				           bssidlist->pbssidinfo->network_found ==
				           PFN_NET_JUST_LOST) {
					bssidlist->pbssidinfo->network_found =
					    PFN_NET_ALREADY_FOUND;
					if (!(bssidlist->flags & WL_PFN_SUPPRESSLOST_MASK)) {
						ASSERT(pfn_cmn->lostcnt);
						ASSERT(pfn_cmn->lostcnt_bssid);
						pfn_cmn->lostcnt--;
						pfn_cmn->lostcnt_bssid--;
					}
					if ((pfn_cmn->param->flags & ENABLE_ADAPTSCAN_MASK) ==
					    (SMART_ADAPT << ENABLE_ADAPTSCAN_BIT))
						pfn_cmn->adaptcnt = 0;
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
					if (((pfn_cmn->param->flags & ENABLE_ADAPTSCAN_MASK) ==
					    (SMART_ADAPT << ENABLE_ADAPTSCAN_BIT)) &&
					    ((bssidlist->pbssidinfo->channel != chan) ||
					     (rssidiff < -(pfn_cmn->param->rssi_margin) ||
					     rssidiff > pfn_cmn->param->rssi_margin)))
						pfn_cmn->adaptcnt = 0;
				}
				/* update channel and time_stamp */
				bssidlist->pbssidinfo->channel = chan;
				bssidlist->pbssidinfo->time_stamp = now;
				break;
			}
			bssidlist = bssidlist->next;
		}
	}
	if ((pfn_cmn->reporttype == WL_PFN_REPORT_SSIDNET) ||
		(pfn_cmn->reporttype == WL_PFN_REPORT_ALLNET)) {

		/* Walk through the PFN element array first to find a match */
		for (k = 0; k < pfn_cmn->count; k++) {
			pfn_internal = pfn_cmn->pfn_internal[k];
			if (is_pfn_ssid_match(pfn_cmn->pfn_internal[k], bi) &&
				is_ssid_processing_allowed(pfn_info, pfn_internal, bi, now)) {
				if (pfn_internal->flags & PFN_SSID_NOT_FOUND) {
					if (!(pfn_internal->flags &
					  (WL_PFN_SUPPRESSFOUND_MASK | WL_PFN_SUPPRESSLOST_MASK))) {
						SET_SSID_PFN_STATE(pfn_internal->flags,
						PFN_SSID_JUST_FOUND);
						pfn_cmn->foundcnt++;
						pfn_cmn->foundcnt_ssid++;
						pfn_cmn->availbsscnt++;
					} else if (!(pfn_internal->flags &
						WL_PFN_SUPPRESSFOUND_MASK)) {
						if (pfn_cmn->param->flags &
							IMMEDIATE_EVENT_MASK) {
							if (wl_pfn_sendonenet(pfn_info, bi, now) ==
								BCME_OK) {
								SET_SSID_PFN_STATE(
									pfn_internal->flags,
									PFN_SSID_ALREADY_FOUND);
							}
						} else {
							SET_SSID_PFN_STATE(pfn_internal->flags,
								PFN_SSID_JUST_FOUND);
							pfn_cmn->foundcnt++;
							pfn_cmn->foundcnt_ssid++;
						}
					} else {
						SET_SSID_PFN_STATE(pfn_internal->flags,
							PFN_SSID_ALREADY_FOUND);
						if (!(pfn_internal->flags &
							WL_PFN_SUPPRESSLOST_MASK))
							pfn_cmn->availbsscnt++;
					}
					pfn_internal->rssi = (int8)bi->RSSI;
					if (bi->flags & WLC_BSS_RSSI_ON_CHANNEL)
						pfn_internal->flags |= PFN_SSID_RSSI_ON_CHANNEL;
					else
						pfn_internal->flags &= ~PFN_SSID_RSSI_ON_CHANNEL;
					/* fill BSSID */
					bcopy(bi->BSSID.octet, pfn_internal->bssid.octet,
					      ETHER_ADDR_LEN);
					if ((pfn_cmn->param->flags & ENABLE_ADAPTSCAN_MASK) ==
					    (SMART_ADAPT << ENABLE_ADAPTSCAN_BIT))
						pfn_cmn->adaptcnt = 0;
					/* update channel */
					pfn_internal->channel = chan;
				} else {
					if (!bcmp(bi->BSSID.octet, pfn_internal->bssid.octet,
					          ETHER_ADDR_LEN)) {
						oldrssi = pfn_internal->rssi;
						/* update rssi */
						if (((pfn_internal->flags &
							PFN_SSID_RSSI_ON_CHANNEL) &&
							(bi->flags & WLC_BSS_RSSI_ON_CHANNEL)) ||
							(!(pfn_internal->flags &
							PFN_SSID_RSSI_ON_CHANNEL) &&
							!(bi->flags & WLC_BSS_RSSI_ON_CHANNEL))) {
							/* preserve max RSSI if the measurements are
							  * both on-channel or both off-channel
							  */
							pfn_internal->rssi =
								MAX(pfn_internal->rssi, bi->RSSI);
						} else if ((bi->flags & WLC_BSS_RSSI_ON_CHANNEL) &&
								(pfn_internal->flags &
								PFN_SSID_RSSI_ON_CHANNEL) == 0) {
							/* preserve the on-channel rssi measurement
							 * if the new measurement is off channel
							 */
							pfn_internal->rssi = (int8)bi->RSSI;
							pfn_internal->flags |=
							PFN_SSID_RSSI_ON_CHANNEL;
						}
						if (pfn_internal->flags & PFN_SSID_JUST_LOST) {
							SET_SSID_PFN_STATE(pfn_internal->flags,
								PFN_SSID_ALREADY_FOUND);
							ASSERT(pfn_cmn->lostcnt);
							ASSERT(pfn_cmn->lostcnt_ssid);
							pfn_cmn->lostcnt--;
							pfn_cmn->lostcnt_ssid--;
							if ((pfn_cmn->param->flags
								& ENABLE_ADAPTSCAN_MASK) ==
								(SMART_ADAPT <<
								ENABLE_ADAPTSCAN_BIT))
								pfn_cmn->adaptcnt = 0;
						} else if (pfn_internal->flags &
							PFN_SSID_JUST_FOUND) {
							; /* do nothing */
						} else {
							rssidiff = pfn_internal->rssi - oldrssi;
							if (((pfn_cmn->param->flags &
							   ENABLE_ADAPTSCAN_MASK) ==
							   (SMART_ADAPT << ENABLE_ADAPTSCAN_BIT)) &&
							   ((pfn_internal->channel != chan) ||
							   (rssidiff <
							   -(pfn_cmn->param->rssi_margin) ||
							   rssidiff >
							   pfn_cmn->param->rssi_margin))) {
								/* reset adaptive scan */
								pfn_cmn->adaptcnt = 0;
							}
						}
						/* update channel */
						pfn_internal->channel = chan;
					} else {
						if (bi->RSSI - pfn_internal->rssi >
							pfn_cmn->param->rssi_margin) {
							/* replace BSSID */
							bcopy(bi->BSSID.octet,
								pfn_internal->bssid.octet,
								ETHER_ADDR_LEN);
							/* update rssi, channel */
							pfn_internal->rssi = (int8)bi->RSSI;
							if (bi->flags & WLC_BSS_RSSI_ON_CHANNEL)
								pfn_internal->flags |=
									PFN_SSID_RSSI_ON_CHANNEL;
							else
								pfn_internal->flags &=
									~PFN_SSID_RSSI_ON_CHANNEL;
							if ((pfn_cmn->param->flags &
							   ENABLE_ADAPTSCAN_MASK) ==
							   (SMART_ADAPT << ENABLE_ADAPTSCAN_BIT))
								pfn_cmn->adaptcnt = 0;
							pfn_internal->channel = chan;
						}
					}
				}
				pfn_internal->time_stamp = now;
				/* To account for crc32 collisions, continue processing  */
				if (!(pfn_internal->flags & WL_PFN_SSID_IMPRECISE_MATCH))
					return;
			}
		}
	}
	/* If match is found as a part of PFN element array, don't need
	* to search in broadcast list. Also don't process the broadcast
	* list if back ground scanning is disabled
	*/
	if (!bssidlist &&
	    (pfn_cmn->param->flags & ENABLE_BKGRD_SCAN_MASK) &&
	    (pfn_cmn->reporttype == WL_PFN_REPORT_ALLNET)) {
		if (ssid_len > 32) {
			WL_PFN_ERROR(("too long ssid %d\n", ssid_len));
			return;
		}

		bdcast_node_list = pfn_cmn->bdcast_list;
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
			if (pfn_cmn->bdcastnum >= MAX_BDCAST_NUM) {
				WL_PFN_ERROR(("wl%d: too many bdcast nodes\n",
				          pfn_info->wlc->pub->unit));
				return;
			}
			if ((retval = wl_pfn_allocate_bdcast_node(pfn_info, ssid_len, ssid, now))
			     != BCME_OK) {
				WL_PFN_ERROR(("wl%d: fail to allocate new bdcast node\n",
				           pfn_info->wlc->pub->unit));
			     return;
			}
			pfn_cmn->bdcast_list->bssidinfo.network_found = PFN_NET_JUST_FOUND;
			pfn_cmn->foundcnt++;
			pfn_cmn->availbsscnt++;
			bcopy(bi->BSSID.octet, pfn_cmn->bdcast_list->bssid.octet, ETHER_ADDR_LEN);
			pfn_cmn->bdcast_list->bssidinfo.channel = chan;
			pfn_cmn->bdcast_list->bssidinfo.rssi = bi->RSSI;
			if (bi->flags & WLC_BSS_RSSI_ON_CHANNEL)
				pfn_cmn->bdcast_list->bssidinfo.flags |= WL_PFN_RSSI_ONCHANNEL;
			pfn_cmn->bdcastnum++;
			WL_TRACE(("wl%d: Added to broadcast list RSSI = %d\n",
				pfn_info->wlc->pub->unit, bi->RSSI));
		} else {
			/* update channel, time_stamp */
			pfn_cmn->bdcast_list->bssidinfo.rssi = bi->RSSI;
			bdcast_node_list->bssidinfo.channel = chan;
			bdcast_node_list->bssidinfo.time_stamp = now;
		}
	}
}

#ifdef GSCAN
/* Process significant Wifi Change BSSIDs
 * Store RSSI, channel, mark as seen
 */
static void
wl_pfn_process_swc_bssid(wl_pfn_info_t *pfn_info,  wlc_bss_info_t * bi)
{
	int8 cur_rssi;
	wl_pfn_swc_list_t  *swc_list_ptr = pfn_info->cmn->pfn_swc_list_hdr;
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
 * where flag_type is enabled. If bssid_channel = UNSPECIFIED_CHANNEL, then
 * check if there exists at least one currently scanning ch bucket where
 * flag_type is enabled. Please note a channel may belong to multiple
 * channel buckets
 */
bool wl_pfn_is_ch_bucket_flag_enabled(wl_pfn_info_t *pfn_info, uint16 bssid_channel,
     uint32 flag_type)
{
	int i, start_idx = 0, num_channels;
	chanspec_t *chan_orig_list;
	wl_pfn_gscan_channel_bucket_t *channel_bucket;
	wl_pfn_gscan_params_t   *gscan_cfg;

	gscan_cfg = pfn_info->cmn->pfn_gscan_cfg_param;
	/* An optimization, no need to check all buckets if
	 * certain flags are set when checking for CH_BUCKET_REPORT_FULL_RESULT
	 */
	if (flag_type & CH_BUCKET_REPORT_FULL_RESULT) {
		/* Has HOST turned flag on for all ch buckets?
		 * Or do ALL the current scan channel buckets
		 * have the flag set to CH_BUCKET_REPORT_FULL_RESULT?
		 */
		if (gscan_cfg->gscan_flags &
		    GSCAN_SEND_ALL_RESULTS_CUR_SCAN) {
			return TRUE;
		}
		/* Either ALL the channel buckets have the flag
		 * flag not set to CH_BUCKET_REPORT_FULL_RESULT
		 * or some do, if ALL then no need to check further
		 * unless flag_type is also looking for scan complete
		 * - we need to check further.
		 */
		if ((gscan_cfg->gscan_flags &
		     GSCAN_NO_FULL_SCAN_RESULTS_CUR_SCAN) &&
		     (flag_type != CH_BUCKET_REPORT_SCAN_COMPLETE)) {
			return FALSE;
		}
	}

	chan_orig_list = pfn_info->cmn->chanspec_list;
	for (i = 0; i < gscan_cfg->count_of_channel_buckets; i++) {
		channel_bucket = &gscan_cfg->channel_bucket[i];
		if ((gscan_cfg->cur_ch_bucket_active & (1 << i))) {
			if (bssid_channel == UNSPECIFIED_CHANNEL) {
				if (channel_bucket->flag &
				     flag_type)
						return TRUE;
			} else {
				num_channels = channel_bucket->bucket_end_index -
				               start_idx + 1;
				if (is_channel_in_list(&chan_orig_list[start_idx],
				   bssid_channel, num_channels)) {
					if (channel_bucket->flag & flag_type)
						return TRUE;
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
	wl_pfn_cmn_info_t *pfn_cmn = pfn_info->cmn;

	for (i = 0; i < pfn_cmn->param->bestn; i++) {
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
	for (i = 0; i < pfn_cmn->param->bestn; i++) {
		if (ETHER_ISNULLADDR(&ptr->pfnsubnet.BSSID)) {
			return ptr;
		}
		if (rssi > ptr->RSSI) {
			if (i < pfn_cmn->param->bestn - 1) {
				memmove(ptr + 1, ptr,
				    sizeof(wl_pfn_bestinfo_t) * (pfn_cmn->param->bestn - 1 - i));
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
	pfn_suppress_ssid = (((pfn_info->cmn->param->flags & SUPPRESS_SSID_MASK) >>
		SUPPRESS_SSID_BIT) == ENABLE) ? TRUE : FALSE;

	if (pre_scan) {
		/* save current setting */
		pfn_info->cmn->suppress_ssid_saved = (bool)suppress_ssid;
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
			suppress_ssid == pfn_info->cmn->suppress_ssid_saved)
			/* if current setting matches saved setting, done */
			return;
		/* need change */
		suppress_ssid = pfn_info->cmn->suppress_ssid_saved;
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
				if (pfn_info->cmn->param->mscan && pfn_info->cmn->numofscan &&
					(pfn_info->cmn->param->flags & REPORT_SEPERATELY_MASK))
					wl_pfn_send_event(pfn_info, NULL,
					          0, WLC_E_PFN_BEST_BATCHING);
				break;
			}
			bzero(foundresult, thisfoundlen);
			foundresult->version = PFN_SCANRESULTS_VERSION;
			foundresult->count = foundcnt;
			foundresult->scan_ch_bucket = wl_pfn_get_cur_ch_buckets(pfn_info);
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
				if (pfn_info->cmn->param->mscan && pfn_info->cmn->numofscan &&
					(pfn_info->cmn->param->flags & REPORT_SEPERATELY_MASK))
					wl_pfn_send_event(pfn_info, NULL,
					       0, WLC_E_PFN_BEST_BATCHING);
				break;
			}
			bzero(lostresult, thislostlen);
			lostresult->version = PFN_SCANRESULTS_VERSION;
			lostresult->count = lostcnt;
			lostresult->scan_ch_bucket = wl_pfn_get_cur_ch_buckets(pfn_info);
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
				WL_PFN_ERROR(("wl%d: scan found network event fail\n",
				          pfn_info->wlc->pub->unit));
			}
			MFREE(pfn_info->wlc->osh, foundresult, thisfoundlen);
		}

		if (lostcnt) {
			event_type = (reportnet == WL_PFN_REPORT_BSSIDNET) ?
			              WLC_E_PFN_BSSID_NET_LOST : WLC_E_PFN_NET_LOST;
			if (BCME_OK != wl_pfn_send_event(pfn_info, (void *)lostresult,
				thislostlen, event_type)) {
				WL_PFN_ERROR(("wl%d: scan lost network event fail\n",
				          pfn_info->wlc->pub->unit));
			}
			MFREE(pfn_info->wlc->osh, lostresult, thislostlen);
		}
		/* update found and lost count */
		pfnfdcnt -= foundcnt;
		pfn_info->cmn->foundcnt -= foundcnt;
		pfnltcnt -= lostcnt;
		pfn_info->cmn->lostcnt -= lostcnt;
	}
}

/* attach bestn from current scan to the best network linked-list and
 * conditionally send WLC_E_PFN_BEST_BATCHING
 */
static bool wl_pfn_attachbestn(wl_pfn_info_t * pfn_info, bool partial)
{
	wl_pfn_bestinfo_t *bestinfo = pfn_info->cmn->current_bestn->bestnetinfo;
	wl_pfn_bestnet_t *ptr;
	wl_pfn_bestinfo_t *pcurbest, *plastbest;
	int i;
	bool event_sent = FALSE;
	bool is_buf_threshold_set;

	is_buf_threshold_set = GSCAN_ENAB(pfn_info->wlc->pub) &&
	               pfn_info->cmn->pfn_gscan_cfg_param->buffer_threshold &&
	               (pfn_info->cmn->pfn_gscan_cfg_param->buffer_threshold < 100);

	if (bestinfo->pfnsubnet.channel) {
		if (partial)
			bestinfo->flags |= 1 << PFN_PARTIAL_SCAN_BIT;
		else
			bestinfo->flags &= ~PFN_PARTIAL_SCAN_MASK;
		pfn_info->cmn->current_bestn->scan_ch_bucket =
			wl_pfn_get_cur_ch_buckets(pfn_info);
		/* current_bestn has valid information */
		if (pfn_info->cmn->numofscan < pfn_info->cmn->param->mscan) {
			pfn_info->cmn->numofscan++;
		} else {
			/* remove the oldest one at pfnbesthdr */
			ptr = pfn_info->cmn->bestnethdr;
			pfn_info->cmn->bestnethdr = pfn_info->cmn->bestnethdr->next;
			MFREE(pfn_info->wlc->osh, ptr, pfn_info->cmn->bestnetsize);
			if (!pfn_info->cmn->bestnethdr)
				pfn_info->cmn->bestnettail = NULL;
		}

		/* If channel buckets were programmed, no point in
		 * comparing the best n since the channels scanned
		 * from scan to scan varies
		 */
		if (pfn_info->cmn->bestnettail && (!GSCAN_ENAB(pfn_info->wlc->pub) ||
		    !pfn_info->cmn->pfn_gscan_cfg_param->count_of_channel_buckets) &&
			(pfn_info->cmn->param->flags & ENABLE_ADAPTSCAN_MASK) ==
			(SMART_ADAPT << ENABLE_ADAPTSCAN_BIT)) {
			/* compare with the previous one, see if any change */
			pcurbest = pfn_info->cmn->current_bestn->bestnetinfo;
			plastbest = pfn_info->cmn->bestnettail->bestnetinfo;
			for (i = 0; i < pfn_info->cmn->param->bestn; i++) {
				if (bcmp(pcurbest->pfnsubnet.BSSID.octet,
				    plastbest->pfnsubnet.BSSID.octet,
				    ETHER_ADDR_LEN)) {
					/* reset scan frequency */
					pfn_info->cmn->adaptcnt = 0;
					break;
				}
				pcurbest++;
				plastbest++;
			}
		}
		/* add to bestnettail */
		if (pfn_info->cmn->bestnettail) {
			pfn_info->cmn->bestnettail->next = pfn_info->cmn->current_bestn;
		} else {
			pfn_info->cmn->bestnethdr = pfn_info->cmn->current_bestn;
		}
		pfn_info->cmn->bestnettail = pfn_info->cmn->current_bestn;
		pfn_info->cmn->current_bestn = NULL;
		if ((pfn_info->cmn->numofscan == pfn_info->cmn->param->mscan) &&
		    (!GSCAN_ENAB(pfn_info->wlc->pub) ||
		    (!is_buf_threshold_set || !pfn_info->cmn->bestn_event_sent)) &&
			(pfn_info->cmn->param->flags & REPORT_SEPERATELY_MASK))
			wl_pfn_send_event(pfn_info, NULL, 0, WLC_E_PFN_BEST_BATCHING);
	}

#ifdef GSCAN
	/* Send an event if the number of BSSIDs crosses a set buf threshold */
	if (GSCAN_ENAB(pfn_info->wlc->pub) && is_buf_threshold_set &&
			!pfn_info->cmn->bestn_event_sent) {
		uint32 count_of_best_n = wl_pfn_calc_num_bestn(pfn_info);
		uint32 total_capacity = pfn_info->cmn->param->mscan * pfn_info->cmn->param->bestn;

		if (total_capacity) {
			uint8 percent_full = (count_of_best_n * 100)/total_capacity;
			/* threshold breached, check if event has already been sent */
			if (percent_full >= pfn_info->cmn->pfn_gscan_cfg_param->buffer_threshold) {
				if (BCME_OK != wl_pfn_send_event(pfn_info, NULL,
					0, WLC_E_PFN_BEST_BATCHING)) {
					WL_PFN_ERROR(("wl%d: Buf thr event fail\n",
					      pfn_info->wlc->pub->unit));
				}
				else {
					event_sent = TRUE;
					WL_PFN(("wl%d: Buf thr evt sent!\n",
					 pfn_info->wlc->pub->unit));
				}
			}
		}
	}

	/* bestn_event_sent stores info on whether an event was sent
	 * in the past m scans
	 */
	if (GSCAN_ENAB(pfn_info->wlc->pub)) {
		if (pfn_info->cmn->numofscan == pfn_info->cmn->param->mscan) {
			pfn_info->cmn->bestn_event_sent = FALSE;
		} else if (event_sent) {
			pfn_info->cmn->bestn_event_sent = TRUE;
		}
	}
#endif /* GSCAN */
	return event_sent;
}

#ifdef GSCAN
static void
wl_pfn_update_ch_bucket_scan_num(wl_pfn_info_t *pfn_info)
{
	int i;
	wl_pfn_cmn_info_t *pfn_cmn = pfn_info->cmn;
	wl_pfn_gscan_channel_bucket_t *ch_bucket;
	uint8 num_ch_bucket = pfn_cmn->pfn_gscan_cfg_param->count_of_channel_buckets;
	wl_pfn_gscan_params_t *gscan_cfg = pfn_cmn->pfn_gscan_cfg_param;

	if (gscan_cfg->gscan_flags & GSCAN_ALL_BUCKETS_IN_FIRST_SCAN_MASK) {
		gscan_cfg->gscan_flags &= ~GSCAN_ALL_BUCKETS_IN_FIRST_SCAN_MASK;
		return;
	}

	ch_bucket = pfn_cmn->pfn_gscan_cfg_param->channel_bucket;
	for (i = 0; i < num_ch_bucket; i++) {
		ch_bucket[i].num_scan++;
	}
}

static void
wl_pfn_reset_ch_bucket(wl_pfn_info_t *pfn_info, bool clearall)
{
	int i;
	wl_pfn_cmn_info_t *pfn_cmn = pfn_info->cmn;
	wl_pfn_gscan_channel_bucket_t *ch_bucket;
	uint8 num_ch_bucket = pfn_cmn->pfn_gscan_cfg_param->count_of_channel_buckets;

	WL_PFN(("Reset ch bucket frequency. flag %d\n", clearall));
	ch_bucket = pfn_cmn->pfn_gscan_cfg_param->channel_bucket;
	for (i = 0; i < num_ch_bucket; i++) {
		if (clearall || ch_bucket->max_freq_multiple) {
			ch_bucket[i].num_scan = 0;
			ch_bucket[i].bucket_freq_multiple = ch_bucket[i].orig_freq_multiple;
		}
	}
}
#endif /* GSCAN */

static void
wl_pfn_scan_adaptation(wl_pfn_info_t *pfn_info)
{
	uint32 i;
	wl_pfn_cmn_info_t *pfn_cmn = pfn_info->cmn;
	uint8 repeat = pfn_cmn->param->repeat;
	uint8 exp = pfn_cmn->param->exp;
#ifdef GSCAN
	uint8 num_ch_bucket = 0;
	int32 max_bucket_time = 0;
	uint16 num_scan;
	uint8 freq_mult;
	wl_pfn_gscan_channel_bucket_t *ch_bucket;
	num_ch_bucket = GSCAN_ENAB(pfn_info->wlc->pub) ?
	         pfn_cmn->pfn_gscan_cfg_param->count_of_channel_buckets: 0;
#endif /* GSCAN */

	/* adaptive scanning */
	if (pfn_cmn->param->flags & ENABLE_ADAPTSCAN_MASK) {
#ifdef GSCAN
	   if (GSCAN_ENAB(pfn_info->wlc->pub)) {
		for (i = 0; i < num_ch_bucket; i++) {
			ch_bucket = &(pfn_cmn->pfn_gscan_cfg_param->channel_bucket[i]);
			num_scan = ch_bucket->num_scan;
			freq_mult = ch_bucket->bucket_freq_multiple;
			if (ch_bucket->max_freq_multiple && !(num_scan % freq_mult)) {
				uint8 orig_mult, currentadapt, max_freq_multiple;
				uint32 adapt_boundary;

				repeat = ch_bucket->repeat;
				currentadapt = pfn_cmn->currentadapt;
				orig_mult = ch_bucket->orig_freq_multiple;
				max_freq_multiple = ch_bucket->max_freq_multiple;
				adapt_boundary = repeat * freq_mult;
				WL_PFN(("numscan %d repeat %d mult %d next %d\n",
				num_scan, repeat, freq_mult, adapt_boundary));
				if (pfn_cmn->adaptcnt <= (repeat * orig_mult * ((1 << exp) - 1)))
					pfn_cmn->adaptcnt++;
				/* Has adaptcnt fallen back (after crossing the first boundary)
				 * to a value within the first window boundary?
				 */
				if (currentadapt && (pfn_cmn->adaptcnt < (repeat * orig_mult))) {
					WL_PFN(("Restoring bucket multiple to %d\n", orig_mult));
					ch_bucket->bucket_freq_multiple = orig_mult;
					pfn_cmn->currentadapt = 0;
					ch_bucket->num_scan = 0;
				} else  if ((num_scan == adapt_boundary) &&
				(freq_mult < max_freq_multiple)) {
					pfn_cmn->currentadapt++;
					ch_bucket->bucket_freq_multiple <<= 1;
					ch_bucket->num_scan = 0;
					WL_PFN(("Adapt bucket multiple bumped up - %d\n",
					ch_bucket->bucket_freq_multiple));
				}
				freq_mult = ch_bucket->bucket_freq_multiple;
			}
			if (max_bucket_time < (freq_mult * pfn_cmn->param->scan_freq)) {
				max_bucket_time = (freq_mult * pfn_cmn->param->scan_freq);
			}
		}
		/* XXX: For now lost_network will be set to be at least as
		 * much as the maximum channel bucket time. (Future merge of dynamic
		 * scan time management will ensure lost_network_time isn't set down
		 * if it is higher than max_bucket_time.)
		 */
		pfn_cmn->param->lost_network_timeout = max_bucket_time;
		WL_PFN(("lost time %d\n", max_bucket_time));
		if (num_ch_bucket) {
			return;
		}
	   }
#endif	/* GSCAN */
		if ((pfn_cmn->param->flags & ENABLE_ADAPTSCAN_MASK) !=
		    (SLOW_ADAPT << ENABLE_ADAPTSCAN_BIT)) {
			if (pfn_cmn->adaptcnt <= repeat * exp)
				pfn_cmn->adaptcnt++;

			if (pfn_cmn->currentadapt &&
			    (pfn_cmn->adaptcnt < repeat)) {
				ASSERT(pfn_cmn->p_pfn_timer);
				wl_del_timer(pfn_info->wlc->wl, pfn_cmn->p_pfn_timer);
				wl_add_timer(pfn_info->wlc->wl, pfn_cmn->p_pfn_timer,
				             pfn_cmn->param->scan_freq, TRUE);
				pfn_cmn->param->lost_network_timeout >>= pfn_cmn->currentadapt;
				pfn_cmn->currentadapt = 0;
				wlc_bss_mac_event(pfn_info->wlc, wlc_bsscfg_primary(pfn_info->wlc),
					WLC_E_PFN_SCAN_BACKOFF, NULL, 0, 1, 0, NULL, 0);
			} else {
				for (i = 1; i <= exp; i++) {
					if (pfn_cmn->adaptcnt == repeat * i) {
						ASSERT(pfn_cmn->p_pfn_timer);
						if (pfn_cmn->currentadapt == 0) {
							wlc_bss_mac_event(pfn_info->wlc,
								wlc_bsscfg_primary(pfn_info->wlc),
								WLC_E_PFN_SCAN_BACKOFF, NULL, 0,
								0, 0, NULL, 0);
						}
						wl_del_timer(pfn_info->wlc->wl,
						         pfn_cmn->p_pfn_timer);
						wl_add_timer(pfn_info->wlc->wl,
							pfn_cmn->p_pfn_timer,
						    pfn_cmn->param->scan_freq << i,
						    TRUE);
						pfn_cmn->currentadapt = (uint8)i;
						pfn_cmn->param->lost_network_timeout <<= 1;
						break;
					}
				}
			}
		} else {
			if (pfn_cmn->param->slow_freq &&
				(pfn_cmn->adaptcnt < pfn_cmn->param->repeat) &&
				(++pfn_cmn->adaptcnt == pfn_cmn->param->repeat)) {
					wl_del_timer(pfn_info->wlc->wl, pfn_cmn->p_pfn_timer);
					wl_add_timer(pfn_info->wlc->wl, pfn_cmn->p_pfn_timer,
					             pfn_cmn->param->slow_freq, TRUE);
			}
		}
	}
}

static void
wl_pfn_scan_complete(void * data, int status, wlc_bsscfg_t *cfg)
{
	wl_pfn_info_t *pfn_info = (wl_pfn_info_t *)data;
	uint32 now;
	wl_pfn_cmn_info_t *pfn_cmn = pfn_info->cmn;
	bool event_sent = FALSE;

	ASSERT(pfn_info);
	ASSERT(pfn_cmn->ssid_list);
	BCM_REFERENCE(event_sent);

#ifdef NLO
	/* disable network offload */
	wlc_iovar_setint(pfn_info->wlc, "nlo", FALSE);

	/* handling suppress ssid option */
	wl_pfn_cfg_supprs_ssid(pfn_info, FALSE);
#endif /* NLO */

	/* If using pfn_mac, restore the saved address */
	if (is_pfn_macaddr_change_enabled(pfn_info->wlc))
		wl_pfn_macaddr_apply(pfn_info, FALSE);

	if (status != WLC_E_STATUS_SUCCESS) {
		bool bestn_attached = FALSE;

		if ((status == WLC_E_STATUS_NEWSCAN) ||
		    (status == WLC_E_STATUS_SUPPRESS) ||
		    (pfn_cmn->pfn_scan_retries < pfn_cmn->pfn_scan_retry_threshold)) {
			wl_pfn_free_ssidlist(pfn_info, FALSE, TRUE);
			wl_pfn_free_bssidlist(pfn_info, FALSE, TRUE);
			pfn_cmn->lostcnt = 0;
			pfn_cmn->foundcnt = 0;
			pfn_cmn->lostcnt_ssid = 0;
			pfn_cmn->foundcnt_ssid = 0;
			pfn_cmn->lostcnt_bssid = 0;
			pfn_cmn->foundcnt_bssid = 0;
			/* Attach bestn if no more retries are possible */
			if ((pfn_cmn->param->flags & REPORT_SEPERATELY_MASK) &&
				pfn_cmn->param->mscan && pfn_cmn->current_bestn &&
				pfn_cmn->pfn_scan_retries >= pfn_cmn->pfn_scan_retry_threshold) {
					event_sent = wl_pfn_attachbestn(pfn_info, TRUE);
					bestn_attached = TRUE;
			}
		}
		/* If bestn is not attached then purge any results
		 * We do not want to compare these results with that of new scan
		 */
		if (!bestn_attached && pfn_cmn->current_bestn)
			memset(pfn_cmn->current_bestn, 0, pfn_cmn->bestnetsize);
		if (pfn_cmn->pfn_scan_retries < pfn_cmn->pfn_scan_retry_threshold) {
			pfn_cmn->pfn_scan_state = PFN_SCAN_PENDING;
			pfn_cmn->pfn_scan_retries++;
		} else {
			WL_PFN(("Retry threshold - %d breached\n",
			     pfn_cmn->pfn_scan_retry_threshold));
			pfn_cmn->pfn_scan_state = PFN_SCAN_IDLE;
			pfn_cmn->pfn_scan_retries = 0;
#ifdef GSCAN
			if (GSCAN_ENAB(pfn_info->wlc->pub))
				wl_pfn_update_ch_bucket_scan_num(pfn_info);
#endif /* GSCAN */
		}
		goto exit;
	}

	/* PFN_SCAN_INPROGRESS is the only vaild state this function can process */
	if (PFN_SCAN_INPROGRESS != pfn_cmn->pfn_scan_state)
		return;

	/* get current ms count for timestamp/timeout processing */
	now = OSL_SYSUPTIME();

	/* If all done scanning, age the SSIDs and do the join if necessary */
	if (pfn_cmn->param->lost_network_timeout != -1)
		wl_pfn_ageing(pfn_info, now);

#ifdef GSCAN
	if (GSCAN_ENAB(pfn_info->wlc->pub)) {
		wl_pfn_update_ch_bucket_scan_num(pfn_info);
		if (pfn_cmn->lostcnt) {
			/* Reset Adaptive buckets only */
			wl_pfn_reset_ch_bucket(pfn_info, FALSE);
		}
	}
#endif /* GSCAN */
	pfn_cmn->pfn_scan_state = PFN_SCAN_IDLE;
	pfn_cmn->pfn_scan_retries = 0;

	/* link the bestn from this scan */
	if (pfn_cmn->param->mscan && pfn_cmn->current_bestn) {
		event_sent = wl_pfn_attachbestn(pfn_info, FALSE);
	}

#ifdef GSCAN
	/* Evaluate Significant WiFi Change */
	if (GSCAN_ENAB(pfn_info->wlc->pub)) {
		if (pfn_cmn->pfn_swc_list_hdr)
			wl_pfn_evaluate_swc(pfn_info);
	}
#endif /* GSCAN */

	/* generate event at the end of scan */
	if (pfn_cmn->foundcnt || pfn_cmn->lostcnt) {
		if ((pfn_cmn->param->flags & ENABLE_ADAPTSCAN_MASK) ==
			(SMART_ADAPT << ENABLE_ADAPTSCAN_BIT))
			pfn_cmn->adaptcnt = 0;

		if (pfn_cmn->param->flags & REPORT_SEPERATELY_MASK) {
			if (pfn_cmn->foundcnt_ssid || pfn_cmn->lostcnt_ssid)
				wl_pfn_create_event(pfn_info, pfn_cmn->foundcnt_ssid,
				        pfn_cmn->lostcnt_ssid, WL_PFN_REPORT_SSIDNET);
			if (pfn_cmn->foundcnt_bssid || pfn_cmn->lostcnt_bssid) {
				wl_pfn_create_event(pfn_info, pfn_cmn->foundcnt_bssid,
				        pfn_cmn->lostcnt_bssid, WL_PFN_REPORT_BSSIDNET);
			}
		}
		else
			wl_pfn_create_event(pfn_info, pfn_cmn->foundcnt,
			                pfn_cmn->lostcnt, WL_PFN_REPORT_ALLNET);
	}

	/* handle the case where non of report-lost PNO networks are ever found */
	if (pfn_cmn->param->lost_network_timeout != -1 &&
	    pfn_cmn->none_cnt != NONEED_REPORTLOST &&
	    pfn_cmn->none_cnt != STOP_REPORTLOST &&
	    pfn_cmn->availbsscnt == 0) {
		if (++pfn_cmn->none_cnt ==
		   (pfn_cmn->param->lost_network_timeout / pfn_cmn->param->scan_freq)) {
			if (!(pfn_cmn->param->flags & REPORT_SEPERATELY_MASK) &&
			    BCME_OK != wl_pfn_send_event(pfn_info, NULL,
				0, WLC_E_PFN_SCAN_ALLGONE)) {
				WL_PFN_ERROR(("wl%d: ALLGONE event fail\n",
				           pfn_info->wlc->pub->unit));
			}
			pfn_cmn->none_cnt = STOP_REPORTLOST;
		}
	}

	/* adaptive scanning */
	wl_pfn_scan_adaptation(pfn_info);
exit:
#ifdef GSCAN
	/* Issue scan complete if active channel buckets have a flag set to do so and
	 * bestn event hasnt been sent.
	 */
	if (GSCAN_ENAB(pfn_info->wlc->pub) && !event_sent &&
	    (PFN_SCAN_IDLE == pfn_cmn->pfn_scan_state) &&
	    wl_pfn_is_ch_bucket_flag_enabled(pfn_info, UNSPECIFIED_CHANNEL,
	    CH_BUCKET_REPORT_SCAN_COMPLETE)) {
		wl_pfn_send_event(pfn_info, NULL, 0, WLC_E_PFN_SCAN_COMPLETE);
		WL_PFN(("Scan complete event sent!\n"));
		WL_EVENT_LOG((EVENT_LOG_TAG_TRACE_WL_INFO, WLC_E_PFN_SCAN_COMPLETE));
	}
#endif /* GSCAN */
	return;
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
	wl_pfn_cmn_info_t *pfn_cmn = pfn_info->cmn;

	/* check PFN list */
	if (((pfn_cmn->reporttype == WL_PFN_REPORT_SSIDNET) ||
		(pfn_cmn->reporttype == WL_PFN_REPORT_ALLNET)) &&
		((reportnet == WL_PFN_REPORT_SSIDNET) ||
		(reportnet == WL_PFN_REPORT_ALLNET))) {
		for (i = 0; i < pfn_cmn->count; i++) {
			if (!foundcnt && !lostcnt)
				return;

			pfn_internal = pfn_cmn->pfn_internal[i];
			inptr = NULL;
			if (foundcnt && foundptr &&
			    pfn_internal->flags & PFN_SSID_JUST_FOUND) {
				SET_SSID_PFN_STATE(pfn_internal->flags, PFN_SSID_ALREADY_FOUND);
				inptr = foundptr;
				foundptr++;
				foundcnt--;
				pfn_cmn->foundcnt_ssid--;
			} else if (lostcnt && lostptr &&
			    pfn_internal->flags & PFN_SSID_JUST_LOST) {
				SET_SSID_PFN_STATE(pfn_internal->flags, PFN_SSID_NOT_FOUND);
				inptr = lostptr;
				lostptr++;
				lostcnt--;
				pfn_cmn->lostcnt_ssid--;
			}
			if (inptr) {
				bcopy(pfn_internal->bssid.octet, inptr->pfnsubnet.BSSID.octet,
						ETHER_ADDR_LEN);
				/* If it is an imprecise match, FW hasn't stored the actual ssid
				 * instead it has a crc32 hash of the ssid. Hence ssid_len = 0 and
				 * should be interpreted as an imprecise ssid result for which the
				 * FW will send the index of the ssid to the HOST.
				 * The assumption is that the HOST will have to maintain
				 * the list that it configured to the FW.
				 */
				if (pfn_internal->flags & WL_PFN_SSID_IMPRECISE_MATCH) {
					inptr->pfnsubnet.SSID_len = 0;
					inptr->pfnsubnet.u.index = (uint16)i;
				} else {
					inptr->pfnsubnet.SSID_len =
						(uint8)pfn_internal->ssid_data.ssid.SSID_len;
					memcpy(inptr->pfnsubnet.u.SSID,
						pfn_internal->ssid_data.ssid.SSID,
						inptr->pfnsubnet.SSID_len);
				}
				inptr->pfnsubnet.channel = (uint8)pfn_internal->channel;
				inptr->RSSI = pfn_internal->rssi;
				inptr->timestamp = 0;
				inptr++;
			}
		}
	}
	/* check BSSID list */
	if (((pfn_cmn->reporttype == WL_PFN_REPORT_BSSIDNET) ||
		(pfn_cmn->reporttype == WL_PFN_REPORT_ALLNET)) &&
		((reportnet == WL_PFN_REPORT_BSSIDNET) ||
		(reportnet == WL_PFN_REPORT_ALLNET))) {
		for (i = 0; i < BSSIDLIST_MAX; i++) {
			bssidlist = pfn_cmn->bssid_list[i];
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
					pfn_cmn->foundcnt_bssid--;
				} else if (lostcnt && lostptr && bssidlist->pbssidinfo &&
				 bssidlist->pbssidinfo->network_found == PFN_NET_JUST_LOST) {
					bssidlist->pbssidinfo->network_found = PFN_NET_NOT_FOUND;
					inptr = lostptr;
					lostptr++;
					lostcnt--;
					pfn_cmn->lostcnt_bssid--;
				}
				if (inptr) {
					bcopy(bssidlist->bssid.octet, inptr->pfnsubnet.BSSID.octet,
					      ETHER_ADDR_LEN);
					inptr->pfnsubnet.SSID_len =
					    (uint8)bssidlist->pbssidinfo->ssid.SSID_len;
					bcopy(bssidlist->pbssidinfo->ssid.SSID,
					      inptr->pfnsubnet.u.SSID,
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
	if ((pfn_cmn->reporttype == WL_PFN_REPORT_ALLNET) &&
		(reportnet == WL_PFN_REPORT_ALLNET) &&
		(pfn_cmn->param->flags & ENABLE_BKGRD_SCAN_MASK)) {
		bdcast_node_list = pfn_cmn->bdcast_list;
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
				      inptr->pfnsubnet.u.SSID, inptr->pfnsubnet.SSID_len);
				inptr->pfnsubnet.channel =
					(uint8)bdcast_node_list->bssidinfo.channel;
				inptr->RSSI = bdcast_node_list->bssidinfo.rssi;
				inptr->timestamp = 0;
				inptr++;
			}
			if (bdcast_node_list->bssidinfo.network_found == PFN_NET_NOT_FOUND) {
				if (bdcast_node_list == pfn_cmn->bdcast_list) {
					pfn_cmn->bdcast_list = pfn_cmn->bdcast_list->next;
					MFREE(pfn_info->wlc->osh, bdcast_node_list,
					      sizeof(wl_pfn_bdcast_list_t));
					prev_bdcast_node = bdcast_node_list = pfn_cmn->bdcast_list;
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
	uint8 directcnt;

	ASSERT(ssidp != NULL);

	*bss_type = DOT11_BSSTYPE_ANY;
	*ssidp = &pfn_info->cmn->ssid_list[0];
	if (pfn_info->cmn->reporttype == WL_PFN_REPORT_BSSIDNET)
		directcnt = 0;
	else
		directcnt = pfn_info->cmn->hiddencnt;
	if ((pfn_info->cmn->hiddencnt < pfn_info->cmn->count &&
	     pfn_info->cmn->reporttype != WL_PFN_REPORT_BSSIDNET) ||
	     (GSCAN_ENAB(pfn_info->wlc->pub) && pfn_info->cmn->pfn_swc_list_hdr) ||
	    (pfn_info->cmn->bssidNum && pfn_info->cmn->reporttype != WL_PFN_REPORT_SSIDNET) ||
		(pfn_info->cmn->param->flags & (ENABLE_BKGRD_SCAN_MASK | ENABLE_BD_SCAN_MASK)))
		*nssid = directcnt + 1; /* broadcast scan needed */
	else
		*nssid = directcnt;

	/* allocate space for current bestn */
	if (pfn_info->cmn->param->mscan && !pfn_info->cmn->current_bestn && *nssid) {
		pfn_info->cmn->current_bestn = (wl_pfn_bestnet_t *)MALLOC(pfn_info->wlc->osh,
		                           pfn_info->cmn->bestnetsize);
		if (!pfn_info->cmn->current_bestn) {
			WL_INFORM(("wl%d: PFN bestn allocation failed\n",
			           pfn_info->wlc->pub->unit));
			if (pfn_info->cmn->param->mscan && pfn_info->cmn->numofscan &&
				(pfn_info->cmn->param->flags & REPORT_SEPERATELY_MASK))
				wl_pfn_send_event(pfn_info, NULL, 0, WLC_E_PFN_BEST_BATCHING);
			return;
		}
	}
	if (pfn_info->cmn->current_bestn)
		bzero(pfn_info->cmn->current_bestn, pfn_info->cmn->bestnetsize);
}

/* PFN scan retry logic. pfn_scan_state is PENDING if
 * we are aborted midway or if we fail to scan and the no.
 * of retries is within the threshold (default 0)
 * If the scan state is PENDING, then start the scan now.
 */
void wl_pfn_inform_mac_availability(wlc_info_t *wlc)
{
	wl_pfn_info_t *pfn_info = (wl_pfn_info_t *)wlc->pfn;

	if (pfn_info->cmn->pfn_scan_state != PFN_SCAN_PENDING)
		return;

	pfn_info->cmn->intflag |= (ENABLE << PFN_RETRY_ATTEMPT);
	WL_PFN(("Retrying PFN scan - %d\n", pfn_info->cmn->pfn_scan_retries));
	/* Start the scan now */
	wl_pfn_timer(pfn_info);
}

#ifdef GSCAN
/* Evaluate and send SWC event if the number of APs that
 * exhibit either:
 * 1. Weighted RSSI > or < the high or low threshold
 * 2. Not seen for past x scans
 * exceeds a set threshold percentage of APs that are being tracked
 */
static void wl_pfn_evaluate_swc(wl_pfn_info_t *pfn_info)
{
	int8 avg_rssi;
	wl_pfn_swc_list_t  *swc_list_ptr;
	uint8  swc_rssi_window_size;
	uint32 change_count = 0;
	bool was_bssid_ch_scanned;
	uint8 lost_ap_window = pfn_info->cmn->pfn_gscan_cfg_param->lost_ap_window;

	swc_list_ptr = pfn_info->cmn->pfn_swc_list_hdr;
	swc_rssi_window_size =  pfn_info->cmn->pfn_gscan_cfg_param->swc_rssi_window_size;

	WL_PFN(("wl%d: SWC eval\n", pfn_info->wlc->pub->unit));

	while (swc_list_ptr) {
		was_bssid_ch_scanned =
			wl_pfn_is_ch_bucket_flag_enabled(pfn_info,
				swc_list_ptr->channel, CH_BUCKET_GSCAN);
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
				            pfn_info->cmn->pfn_gscan_cfg_param->lost_ap_window));
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

	if (change_count >= pfn_info->cmn->pfn_gscan_cfg_param->swc_nbssid_threshold) {
		wl_pfn_gen_swc_event(pfn_info, change_count);
	}
	else {
		/* Remove report flag */
		swc_list_ptr = pfn_info->cmn->pfn_swc_list_hdr;

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
	uint16 swc_rssi_window_size = pfn_info->cmn->pfn_gscan_cfg_param->swc_rssi_window_size;
	wl_pfn_swc_list_t  *swc_list_ptr = pfn_info->cmn->pfn_swc_list_hdr;
	wl_pfn_swc_results_t *results_ptr = NULL;
	wl_pfn_significant_net_t *netptr;

	while (rem) {

		partial_count = MIN(rem, EVENT_MAX_SIGNIFICANT_CHANGE_NETCNT);

		mem_needed = sizeof(wl_pfn_swc_results_t) +
		            (partial_count - 1) * sizeof(wl_pfn_significant_net_t);
		results_ptr = (wl_pfn_swc_results_t *) MALLOC(pfn_info->wlc->osh, mem_needed);

		if (!results_ptr) {
			WL_PFN_ERROR(("wl%d: Significant change results allocation failed\n",
			 pfn_info->wlc->pub->unit));
			if (pfn_info->cmn->param->mscan && pfn_info->cmn->numofscan &&
				(pfn_info->cmn->param->flags & REPORT_SEPERATELY_MASK))
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
			WL_PFN_ERROR(("wl%d:  WLC_E_PFN_SWC event fail\n",
			 pfn_info->wlc->pub->unit));
			MFREE(pfn_info->wlc->osh, results_ptr, mem_needed);
			return;
		}
		else {
			WL_PFN(("wl%d: Significant evt sent!\n", pfn_info->wlc->pub->unit));
			MFREE(pfn_info->wlc->osh, results_ptr, mem_needed);
		}
	}
	return;
}

static void
wl_pfn_get_swc_weights(int8 *weights)
{
	/* For now hardcoded */
	int8 buf[PFN_SWC_RSSI_WINDOW_MAX] = { RSSI_WEIGHT_MOST_RECENT, 8, 6, 4, 3, 2, 1, 1 };
	memcpy(weights, buf, PFN_SWC_RSSI_WINDOW_MAX);
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
	uint32 swc_rssi_window_size = pfn_info->cmn->pfn_gscan_cfg_param->swc_rssi_window_size;
	int32 i, k = 0;
	int16 relevance_score = 0;
	int8 weights[PFN_SWC_RSSI_WINDOW_MAX];
	int16 avg_rssi = 0;

	wl_pfn_get_swc_weights(weights);

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
	wl_pfn_info_t		* pfn_info = (wl_pfn_info_t *)data;
	int32				bss_type;
	int				nssid;
	wlc_ssid_t			*ssidp;

	ASSERT(pfn_info);
	/* Allow processing only if the state is  PFN_SCAN_IDLE or
	 * PFN_SCAN_PENDING, scan is not suspended,  and driver is up
	 */
	if (((PFN_SCAN_IDLE != pfn_info->cmn->pfn_scan_state) &&
	    (PFN_SCAN_PENDING != pfn_info->cmn->pfn_scan_state)) ||
	    (pfn_info->cmn->intflag & (ENABLE << SUSPEND_BIT)))
		return;

	/* If timer triggered before being notified of
	 * mac availibility then this scan is lost, reset
	 * and move on to the next
	 */
	if ((pfn_info->cmn->intflag & (ENABLE << PFN_RETRY_ATTEMPT)) &&
			(pfn_info->cmn->pfn_scan_state == PFN_SCAN_PENDING)) {
		pfn_info->cmn->pfn_scan_state = PFN_SCAN_IDLE;
		pfn_info->cmn->pfn_scan_retries = 0;
#ifdef GSCAN
		if (GSCAN_ENAB(pfn_info->wlc->pub))
			wl_pfn_update_ch_bucket_scan_num(pfn_info);
#endif /* GSCAN */
		pfn_info->cmn->intflag &= ~(ENABLE << PFN_RETRY_ATTEMPT);
	}

	wl_pfn_prepare_for_scan(pfn_info, &ssidp, &nssid, &bss_type);
	/* Kick off scan when nssid != 0 */
	if (nssid &&
		wl_pfn_start_scan(pfn_info, ssidp, nssid, bss_type) == BCME_OK) {
		pfn_info->cmn->pfn_scan_state = PFN_SCAN_INPROGRESS;
	}
}

/* Sort and remove any duplicates that may be in the
 * array. Sort in descending order from the end of array,
 * so that MAX_CHANSPEC which replaces duplicates will
 * accumulate at the end of the array
 */
static uint8
wl_pfn_sort_remove_dup_chanspec(chanspec_t *chanspec_list, uint8 chanspec_count)
{
	int i, j;
	chanspec_t tmp;
	uint8 num_chanspec = chanspec_count;

	for (i = chanspec_count - 2; i >= 0; i--) {
		tmp = chanspec_list[i];
		for (j = i; j < chanspec_count - 1; j++) {
			if (chanspec_list[j+1] == tmp &&
				chanspec_list[j+1] != MAX_CHANSPEC) {
				tmp = MAX_CHANSPEC;
				num_chanspec--;
			}
			if (chanspec_list[j+1] > tmp)
				break;
			chanspec_list[j] = chanspec_list[j+1];
		}
		chanspec_list[j] = tmp;
	}

	return num_chanspec;
}

static int
wl_pfn_start_scan(wl_pfn_info_t * pfn_info, wlc_ssid_t	* ssid, int nssid, int bss_type)
{
	int rc;
	uint scanflags = 0;
	bool save_prb = FALSE;
	wl_pfn_cmn_info_t *pfn_cmn = pfn_info->cmn;
#ifdef GSCAN
	wl_pfn_swc_list_t  *swc_list_ptr = pfn_cmn->pfn_swc_list_hdr;
	wl_pfn_gscan_params_t *gscan_cfg = pfn_info->cmn->pfn_gscan_cfg_param;
	chanspec_t	cur_chanspec_list[WL_NUMCHANNELS];
#endif /* GSCAN */
	chanspec_t *chanspec_list = pfn_cmn->chanspec_list;
	uint8 chanspec_count = pfn_cmn->chanspec_count;
#ifdef BCMDBG
	char				ssidbuf[SSID_FMT_BUF_LEN];
#endif /* BCMDBG */

	WL_TRACE(("wl%d: Scan request : Nssid = %d, SSID = %s, SSID len = %d BSS type = %d\n",
		pfn_info->wlc->pub->unit, nssid, (wlc_format_ssid(ssidbuf,
		ssid->SSID, ssid->SSID_len), ssidbuf), ssid->SSID_len, bss_type));

	pfn_cmn->cur_scan_type = (int16)bss_type;

#ifdef NLO
	/* handling suppress ssid option */
	wl_pfn_cfg_supprs_ssid(pfn_info, TRUE);
	/* handling network offload option */
	wlc_iovar_setint(pfn_info->wlc, "nlo",
		(pfn_cmn->param->flags & ENABLE_NET_OFFLOAD_BIT) ? 1 : 0);
#endif /* NLO */

	scanflags |= WL_SCANFLAGS_OFFCHAN;
	if (pfn_cmn->intflag & (ENABLE << PROHIBITED_BIT))
		scanflags |= WL_SCANFLAGS_PROHIBITED;

	save_prb = (pfn_cmn->param->flags & IMMEDIATE_EVENT_MASK) ? TRUE : FALSE;

#ifdef GSCAN
	if (GSCAN_ENAB(pfn_info->wlc->pub)) {
		while (swc_list_ptr) {
			/* Reset rssi for next scan */
			swc_list_ptr->prssi_history[swc_list_ptr->rssi_idx] = INVALID_RSSI;
			swc_list_ptr = swc_list_ptr->next;
		}

		if (gscan_cfg->count_of_channel_buckets &&
			pfn_cmn->chanspec_count) {
			if (!(chanspec_count =
			         wl_pfn_gscan_get_curscan_chlist(pfn_info, cur_chanspec_list))) {
				wl_pfn_update_ch_bucket_scan_num(pfn_info);
				return NO_CHANNEL_TO_SCAN;
			}
			chanspec_list = cur_chanspec_list;
		}
	}
#endif /* GSCAN */

	/* If the scan request was accepted, change MAC if configured */
	if (is_pfn_macaddr_change_enabled(pfn_info->wlc)) {
		char eabuf[ETHER_ADDR_STR_LEN];
		/* If the HOST has only programmed the OUI, put in
		 * random bytes in the lower NIC specific part
		 */
		if ((pfn_cmn->intflag & (ENABLE << PFN_ONLY_MAC_OUI_SET_BIT))) {
			wl_pfn_fill_mac_nic_bytes(pfn_info,
			         &(pfn_cmn->pfn_mac.octet[DOT11_OUI_LEN]));
		}
		WL_PFN(("Changing PFN mac addr: %s\n", bcm_ether_ntoa(&pfn_cmn->pfn_mac, eabuf)));
		wl_pfn_macaddr_apply(pfn_info, TRUE);
	}

	/* Issue scan a scan which could be broadcast or directed scan */
	/* We don't check return value here because if wlc_scan_request() returns an error,
	 * the callback function wl_pfn_scan_complete() should have been called and
	 * PFN_SCAN_INPROGRESS flag should have been cleared
	 */
	if (!chanspec_count) { /* all channels */
		rc = wlc_scan_request_ex(pfn_info->wlc, bss_type, &ether_bcast, nssid, ssid,
		    DOT11_SCANTYPE_ACTIVE,	-1, -1,	-1,	-1, NULL, 0, 0, save_prb,
		    wl_pfn_scan_complete, pfn_info, WLC_ACTION_PNOSCAN, scanflags,
		    NULL, NULL, NULL);
	} else {
		rc = wlc_scan_request_ex(pfn_info->wlc, bss_type, &ether_bcast, nssid, ssid,
		    DOT11_SCANTYPE_ACTIVE,	-1, -1, -1, -1,	chanspec_list,
		    chanspec_count, 0, save_prb, wl_pfn_scan_complete, pfn_info,
		    WLC_ACTION_PNOSCAN, scanflags, NULL, NULL, NULL);
	}

	/* If scan is not successful, restore the saved address */
	if (rc != BCME_OK && is_pfn_macaddr_change_enabled(pfn_info->wlc))
		wl_pfn_macaddr_apply(pfn_info, FALSE);

	WL_EVENT_LOG((EVENT_LOG_TAG_TRACE_WL_INFO, TRACE_G_SCAN_STARTED));

	return rc;
}

#ifdef GSCAN
static uint8
wl_pfn_gscan_get_curscan_chlist(wl_pfn_info_t *pfn_info, chanspec_t  *chanspec_list)
{
	int i;
	int start_superlist_index = 0;
	int start_sublist_index = 0;
	uint32 channels_to_copy = 0;
	uint16 numofscan;
	uint8 chanspec_count = 0;
	wl_pfn_gscan_channel_bucket_t *channel_bucket;
	chanspec_t  *chan_orig_list;
	wl_pfn_gscan_params_t *gscan_cfg = pfn_info->cmn->pfn_gscan_cfg_param;

	chan_orig_list = pfn_info->cmn->chanspec_list;
	/* Three possibilities: All, some or none have flag
	 * set to CH_BUCKET_REPORT_FULL_RESULT, let's find out.
	 */
	gscan_cfg->gscan_flags |= GSCAN_FULL_SCAN_RESULTS_CUR_SCAN;
	gscan_cfg->gscan_flags |= GSCAN_NO_FULL_SCAN_RESULTS_CUR_SCAN;
	gscan_cfg->cur_ch_bucket_active = 0;

	for (i = 0; (i < gscan_cfg->count_of_channel_buckets) &&
			(start_sublist_index < WL_NUMCHANNELS); i++) {
		channel_bucket = &gscan_cfg->channel_bucket[i];
		numofscan = channel_bucket->num_scan + 1;
		if (!(numofscan % channel_bucket->bucket_freq_multiple) ||
		   (gscan_cfg->gscan_flags &
		   GSCAN_ALL_BUCKETS_IN_FIRST_SCAN_MASK)) {
			channels_to_copy = channel_bucket->bucket_end_index -
			                         start_superlist_index + 1;
			bcopy(&chan_orig_list[start_superlist_index],
			           &chanspec_list[start_sublist_index],
			           channels_to_copy * sizeof(*chan_orig_list));
			chanspec_count += ((uint8) channels_to_copy);
			start_sublist_index += channels_to_copy;
			gscan_cfg->cur_ch_bucket_active |= (1 << i);
			if (!(channel_bucket->flag & CH_BUCKET_REPORT_FULL_RESULT)) {
				gscan_cfg->gscan_flags &=
				           ~GSCAN_FULL_SCAN_RESULTS_CUR_SCAN;
			} else {
				gscan_cfg->gscan_flags &=
						~GSCAN_NO_FULL_SCAN_RESULTS_CUR_SCAN;
			}
		}
		/* end_index has been verified to be within bounds during cfg */
		start_superlist_index = channel_bucket->bucket_end_index + 1;
	}

	/* There may be duplicates, need to remove this
	 * and wlc_scan takes in channels in sorted order
	 */
	chanspec_count = wl_pfn_sort_remove_dup_chanspec(chanspec_list, chanspec_count);
	WL_PFN(("wl%d: act_chbkt %x nchan %d\n", pfn_info->wlc->pub->unit,
		gscan_cfg->cur_ch_bucket_active, chanspec_count));
#ifdef GSCAN_DBG
		{
			uint64 compact = 0;
			uint32 *ptr = (uint32 *)&compact;
			for (i = 0; i < chanspec_count;) {
				compact <<= 8;
				compact |= ((uint8) CHSPEC_CHANNEL(chanspec_list[i++]));
				if (!(i & 0x7)) {
					WL_DBG(("%08x %08x\n", ptr[1], ptr[0]));
					compact = 0;
				}
			}
			if (compact) {
				WL_DBG(("%08x %08x\n", ptr[1], ptr[0]));
			}
		}
#endif /* GSCAN_DBG */
	return chanspec_count;
}
#endif /* GSCAN */

int
wl_pfn_scan_in_progress(wl_pfn_info_t * pfn_info)
{
	return (pfn_info->cmn->pfn_scan_state == PFN_SCAN_INPROGRESS);
}

static void
wl_pfn_ageing(wl_pfn_info_t * pfn_info, uint32 now)
{
	wl_pfn_internal_t	    * pfn_internal;
	wl_pfn_bdcast_list_t    * bdcast_list;
	int                     i;
	wl_pfn_bssid_list_t *bssid_list;

#ifdef BCMDBG
	char					ssidbuf[SSID_FMT_BUF_LEN];
#endif /* BCMDBG */

	/* Evaluate PFN list for disappearence of network */
	if ((pfn_info->cmn->reporttype == WL_PFN_REPORT_SSIDNET) ||
		(pfn_info->cmn->reporttype == WL_PFN_REPORT_ALLNET)) {
		for (i = 0; i < pfn_info->cmn->count; i++) {
			pfn_internal = pfn_info->cmn->pfn_internal[i];
			if (pfn_internal->flags & WL_PFN_SUPPRESS_AGING_MASK) {
				continue;
			}
			if ((pfn_internal->flags & PFN_SSID_ALREADY_FOUND) &&
				((now - pfn_internal->time_stamp) >=
				(uint32)pfn_info->cmn->param->lost_network_timeout)) {
				WL_TRACE(("wl%d: Lost SSID idx= %d elapsed time = %d\n",
				    pfn_info->wlc->pub->unit, i,
				    now - pfn_internal->time_stamp));
				if (!(pfn_internal->flags & WL_PFN_SUPPRESSLOST_MASK)) {
					SET_SSID_PFN_STATE(pfn_internal->flags, PFN_SSID_JUST_LOST);
					pfn_info->cmn->lostcnt++;
					pfn_info->cmn->lostcnt_ssid++;
					ASSERT(pfn_info->cmn->availbsscnt);
					if (--pfn_info->cmn->availbsscnt == 0) {
						if (BCME_OK != wl_pfn_send_event(pfn_info, NULL,
							0, WLC_E_PFN_SCAN_ALLGONE)) {
							WL_PFN_ERROR(("wl%d: ALLGONE fail\n",
							      pfn_info->wlc->pub->unit));
						}
						pfn_info->cmn->none_cnt = STOP_REPORTLOST;
					}
				} else {
					SET_SSID_PFN_STATE(pfn_internal->flags, PFN_SSID_NOT_FOUND);
				}
			} else if ((pfn_internal->flags & PFN_SSID_JUST_FOUND) &&
				((now - pfn_internal->time_stamp) >=
				(uint32)pfn_info->cmn->param->lost_network_timeout)) {
				SET_SSID_PFN_STATE(pfn_internal->flags, PFN_SSID_NOT_FOUND);
				ASSERT(pfn_info->cmn->foundcnt);
				pfn_info->cmn->foundcnt--;
				pfn_info->cmn->foundcnt_ssid--;
				if (!(pfn_internal->flags & WL_PFN_SUPPRESSLOST_MASK)) {
					ASSERT(pfn_info->cmn->availbsscnt);
					pfn_info->cmn->availbsscnt--;
				}
			}
		}
	}
	/* Evaluate bssid_list for disappearence of network */
	if ((pfn_info->cmn->reporttype == WL_PFN_REPORT_BSSIDNET) ||
		(pfn_info->cmn->reporttype == WL_PFN_REPORT_ALLNET)) {
		for (i = 0; i < BSSIDLIST_MAX; i++) {
			bssid_list = pfn_info->cmn->bssid_list[i];
			while (bssid_list) {
				if (bssid_list->pbssidinfo &&
				    (bssid_list->pbssidinfo->network_found ==
				                        PFN_NET_ALREADY_FOUND) &&
				    ((now - bssid_list->pbssidinfo->time_stamp) >=
					(uint32)pfn_info->cmn->param->lost_network_timeout)) {
					if (!(bssid_list->flags & WL_PFN_SUPPRESSLOST_MASK)) {
						bssid_list->pbssidinfo->network_found =
						                    PFN_NET_JUST_LOST;
						pfn_info->cmn->lostcnt++;
						pfn_info->cmn->lostcnt_bssid++;
						ASSERT(pfn_info->cmn->availbsscnt);
						if (--pfn_info->cmn->availbsscnt == 0) {
							if (BCME_OK != wl_pfn_send_event(pfn_info,
							    NULL, 0, WLC_E_PFN_SCAN_ALLGONE)) {
								WL_PFN_ERROR(
									("wl%d: ALLGONE fail\n",
									pfn_info->wlc->pub->unit));
							}
							pfn_info->cmn->none_cnt = STOP_REPORTLOST;
						}
					} else {
						MFREE(pfn_info->wlc->osh, bssid_list->pbssidinfo,
						      sizeof(wl_pfn_bssidinfo_t));
						bssid_list->pbssidinfo = NULL;
					}
				} else if (bssid_list->pbssidinfo &&
				    (bssid_list->pbssidinfo->network_found == PFN_NET_JUST_FOUND) &&
					((now - bssid_list->pbssidinfo->time_stamp) >=
					(uint32)pfn_info->cmn->param->lost_network_timeout)) {
					bssid_list->pbssidinfo->network_found = PFN_NET_NOT_FOUND;
					ASSERT(pfn_info->cmn->foundcnt);
					pfn_info->cmn->foundcnt--;
					ASSERT(pfn_info->cmn->availbsscnt);
					pfn_info->cmn->availbsscnt--;
				}
				bssid_list = bssid_list->next;
			}
		}
	}
	/* Evaluate broadcast list for disappearence of network */
	if ((pfn_info->cmn->reporttype == WL_PFN_REPORT_ALLNET) &&
		(pfn_info->cmn->param->flags & ENABLE_BKGRD_SCAN_MASK)) {
		bdcast_list = pfn_info->cmn->bdcast_list;
		while (bdcast_list) {
			/* Check to see if given SSID is gone */
			if ((now - bdcast_list->bssidinfo.time_stamp) >=
				(uint32)pfn_info->cmn->param->lost_network_timeout) {
				WL_TRACE(("wl%d: Broadcast lost SSID = %s elapsed time = %d\n",
				    pfn_info->wlc->pub->unit, (wlc_format_ssid(ssidbuf,
				    bdcast_list->bssidinfo.ssid.SSID,
				    bdcast_list->bssidinfo.ssid.SSID_len), ssidbuf),
				    now - bdcast_list->bssidinfo.time_stamp));
				if (bdcast_list->bssidinfo.network_found == PFN_NET_ALREADY_FOUND) {
					bdcast_list->bssidinfo.network_found = PFN_NET_JUST_LOST;
					pfn_info->cmn->lostcnt++;
					ASSERT(pfn_info->cmn->availbsscnt);
					if (--pfn_info->cmn->availbsscnt == 0) {
						if (BCME_OK != wl_pfn_send_event(pfn_info, NULL,
							0, WLC_E_PFN_SCAN_ALLGONE)) {
							WL_PFN_ERROR(
							   ("wl%d: PFN_SCAN_ALLGONE event fail\n",
							     pfn_info->wlc->pub->unit));
						}
						pfn_info->cmn->none_cnt = STOP_REPORTLOST;
					}
				} else if (bdcast_list->bssidinfo.network_found ==
				                                        PFN_NET_JUST_FOUND) {
					bdcast_list->bssidinfo.network_found = PFN_NET_NOT_FOUND;
					ASSERT(pfn_info->cmn->foundcnt);
					pfn_info->cmn->foundcnt--;
					ASSERT(pfn_info->cmn->availbsscnt);
					pfn_info->cmn->availbsscnt--;
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

static bool is_pfn_macaddr_change_enabled(wlc_info_t * wlc)
{
	wl_pfn_info_t *pfn_info = (wl_pfn_info_t *)wlc->pfn;

	if (!(pfn_info->cmn->intflag & (ENABLE << PFN_MACADDR_BIT)))
		return FALSE;

	/* Change mac addr only if unassociated if the appropriate bit is set */
	if (!(pfn_info->cmn->intflag & (ENABLE << PFN_MACADDR_UNASSOC_ONLY_BIT)) ||
	    !wlc_rsdb_any_wlc_associated(wlc)) {
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
	char eabuf[ETHER_ADDR_STR_LEN];
#ifdef WL_PRQ_RAND_SEQ
	wlc_bsscfg_t *bsscfg = NULL;
#endif /* WL_PRQ_RAND_SEQ */
	getrc = wlc_iovar_op(pfn_info->wlc, "cur_etheraddr", NULL, 0,
	                     &curmac, ETHER_ADDR_LEN, IOV_GET, NULL);

	if (!apply && (getrc == BCME_OK) &&
	    memcmp(&curmac, &pfn_info->cmn->pfn_mac, ETHER_ADDR_LEN)) {
		/* Don't restore if the address changed under us */
		WL_PFN(("skipped restore %s\n", bcm_ether_ntoa(&curmac, eabuf)));
		setmac = &curmac;
	} else {
		ASSERT(apply || !ETHER_ISNULLADDR(&pfn_info->cmn->save_mac));
		if (apply) {
			setmac = &pfn_info->cmn->pfn_mac;
			pfn_info->cmn->save_mac = curmac;
		} else {
			setmac = &pfn_info->cmn->save_mac;
			WL_PFN(("restore %s\n", bcm_ether_ntoa(setmac, eabuf)));
		}

#ifdef WL_PRQ_RAND_SEQ
		bsscfg = wlc_bsscfg_find_by_hwaddr(pfn_info->wlc, &pfn_info->cmn->save_mac);
		wlc_tx_prq_rand_seq_enable(pfn_info->wlc, bsscfg, apply);
#endif // endif
		setrc = wlc_iovar_op(pfn_info->wlc, "cur_etheraddr", NULL, 0,
		                     setmac, ETHER_ADDR_LEN, IOV_SET, NULL);

		if (!apply)
			memset(&pfn_info->cmn->save_mac, 0, sizeof(pfn_info->cmn->save_mac));
	}

	if (getrc || setrc)
		WL_INFORM(("PFN MAC (%d): getrc %d setrc %d cmp %d\n",
		            apply, getrc, setrc, (setmac == &curmac)));
}

static void
wl_pfn_fill_mac_nic_bytes(wl_pfn_info_t * pfn_info, uint8 *buf)
{
	int ret;
	struct ether_addr curmac;
	uint8 *nic_specific;

	ret = wlc_iovar_op(pfn_info->wlc, "cur_etheraddr", NULL, 0,
	                     &curmac, ETHER_ADDR_LEN, IOV_GET, NULL);

	if (ret != BCME_OK)
		WL_PFN_ERROR(("Failed to get curmaddr - %d\n", ret));

	wlc_getrand(pfn_info->wlc, buf, DOT11_OUI_LEN);

	nic_specific = &curmac.octet[DOT11_OUI_LEN];
	/* HOST can set the OUI to be same as our current OUI
	 * Hence check if lower three bytes i.e. NIC specific part
	 * of MACADDR is the same as our MACADDR, if yes
	 * try getting random bytes again
	 */
	if ((ret == BCME_OK) && (memcmp(nic_specific, buf, DOT11_OUI_LEN) == 0)) {
		/* Assumption: Will not get the same 3 bytes again */
		wlc_getrand(pfn_info->wlc, buf, DOT11_OUI_LEN);
	}
	return;
}

static int
wl_pfn_macaddr_set(wl_pfn_info_t * pfn_info, void * buf, int len)
{
	bool isnull;
	wl_pfn_macaddr_cfg_t *macp = buf;

	if (macp->version == LEGACY1_WL_PFN_MACADDR_CFG_VER)
		macp->flags = WL_PFN_RESTRICT_LA_MAC_MASK;
	else if (macp->version != WL_PFN_MACADDR_CFG_VER)
		return BCME_VERSION;

	/* Multicast MACADDR not allowed */
	if (ETHER_ISMULTI(&macp->macaddr))
		return BCME_BADARG;

	struct wl_pfn_cmn_info *pfn_cmn = pfn_info->cmn;

	/* Reject changes while scanning */
	if (pfn_cmn->pfn_scan_state == PFN_SCAN_INPROGRESS)
		return BCME_EPERM;

	isnull = ETHER_ISNULLADDR(&macp->macaddr);

	if (!isnull && (macp->flags & WL_PFN_RESTRICT_LA_MAC_MASK) &&
	         !ETHER_IS_LOCALADDR(&macp->macaddr)) {
		return BCME_BADARG;
	}

	/* PFN_ONLY_MAC_OUI_SET_BIT and PFN_MACADDR_UNASSOC_ONLY_BIT
	 * have no meaning if PFN_MACADDR_BIT is unset.
	 */
	pfn_cmn->intflag |= ((macp->flags & WL_PFN_MACADDR_FLAG_MASK) << PFN_ONLY_MAC_OUI_SET_BIT);

	/* Ok -- pick up the new address */
	memcpy(&pfn_cmn->pfn_mac, &macp->macaddr, sizeof(struct ether_addr));

	if (isnull)
		pfn_cmn->intflag &= ~(ENABLE << PFN_MACADDR_BIT);
	else
		pfn_cmn->intflag |= (ENABLE << PFN_MACADDR_BIT);

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

	struct wl_pfn_cmn_info *pfn_cmn = pfn_info->cmn;

	macp = buf;
	macp->version = inp_version;
	macp->flags = (pfn_cmn->intflag & PFN_INT_MACADDR_FLAGS_MASK) >> PFN_ONLY_MAC_OUI_SET_BIT;
	memcpy(&macp->macaddr, &pfn_cmn->pfn_mac, sizeof(struct ether_addr));

	return BCME_OK;
}

static int
wl_pfn_allocate_bdcast_node(wl_pfn_info_t * pfn_info, uint8 ssid_len, uint8 * ssid,
                                      uint32 now)
{
	wl_pfn_bdcast_list_t * node_ptr;

	/* Allocate for node */
	if (!(node_ptr = (wl_pfn_bdcast_list_t *) MALLOC(pfn_info->wlc->osh,
		sizeof(wl_pfn_bdcast_list_t)))) {
		WL_ERROR(("wl%d: PFN: MALLOC failed, size = %d\n", pfn_info->wlc->pub->unit,
			sizeof(wl_pfn_bdcast_list_t)));
		if (pfn_info->cmn->param->mscan && pfn_info->cmn->numofscan &&
			(pfn_info->cmn->param->flags & REPORT_SEPERATELY_MASK))
			wl_pfn_send_event(pfn_info, NULL, 0, WLC_E_PFN_BEST_BATCHING);
		return BCME_NOMEM;
	}

	/* Copy the ssid info */
	memcpy(node_ptr->bssidinfo.ssid.SSID, ssid, ssid_len);
	node_ptr->bssidinfo.ssid.SSID_len = ssid_len;
	node_ptr->bssidinfo.time_stamp = now;

	/* Add the new node to begining of the list */
	node_ptr->next = pfn_info->cmn->bdcast_list;
	pfn_info->cmn->bdcast_list = node_ptr;

	return BCME_OK;
}

/* free malloc-ed data after calling this function */
static int
wl_pfn_send_event(wl_pfn_info_t * pfn_info, void *data, uint32 datalen, int event_type)
{
	wlc_info_t		* wlc;

	wlc = pfn_info->wlc;
	wlc_bss_mac_event(wlc, wlc_bsscfg_primary(wlc), event_type, NULL, 0, WLC_E_STATUS_SUCCESS,
		0, data, datalen);

	return BCME_OK;
}

bool
wl_pfn_scan_state_enabled(wlc_info_t *wlc)
{
	wl_pfn_info_t *pfn_info = (wl_pfn_info_t *)wlc->pfn;
	return !(pfn_info->cmn->pfn_scan_state == PFN_SCAN_DISABLED);
}

#endif /* WLPFN */
