/*
 * 802.11v protocol implementation for
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
 * $Id: wlc_wnm.c 782551 2019-12-23 12:07:35Z $
 */

/**
 * @file
 * @brief
 * This file implements a part of 802.11v Wireless Network Management features (aka Wi-Fi Alliance
 * Network Power Save) for AP and STA:
 * - BSS Max Idle Period: AP advertises the period before which STA must send keep alive frames;
 * - Directed Multicast Service: STA can request AP to clone BC/MC frames in unicast A-MSDU;
 * - Flexible Multicast Service: STA can request AP to deliver BC/MC frames every n DTIM;
 * - Traffic Filtering Service: STA can request AP to discard specific frames;
 * - Sleep Mode: STA can decide to wake up only every n DTIM beacons;
 * - TIM Broadcast: STA can skip all beacons and wake up only for shorter TIM BC frames;
 * - BSS Transition Management: AP can advertise in advance the end of the association;
 * - Proxy ARP: AP proxies ARP request intended for STA (only wrapper code here).
 * - Neighbor reporting
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
#include <802.11.h>
#include <802.3.h>
#include <vlan.h>
#include <bcmip.h>
#include <bcmipv6.h>
#include <bcmarp.h>
#include <bcmicmp.h>
#include <bcmtcp.h>
#include <bcmudp.h>
#include <bcmdhcp.h>
#include <eapol.h>
#include <bcmproto.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_pub.h>
#include <wlc_channel.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_rate.h>
#include <wlc_scb.h>
#include <wlc_scb_ratesel.h>
#include <wlc_scan.h>
#include <wlc_wnm.h>
#include <wlc_assoc.h>
#include <wl_dbg.h>
#include <wlc_tpc.h>
#include <wl_export.h>
#include <wlc_keymgmt.h>
#include <bcmwpa.h>
#include <wlc_sup.h>
#include <wlc_bmac.h>
#include <wlc_ie_mgmt.h>
#include <wlc_ie_mgmt_ft.h>
#include <wlc_ie_mgmt_vs.h>
#include <wlc_csa.h>
#include <wlc_quiet.h>
#include <wl_keep_alive.h>
#include <wlc_tx.h>
#include <wlc_pcb.h>
#include <bcm_l2_filter.h>
#include <wlc_ie_helper.h>
#include <wlc_hw.h>
#include <wlc_event_utils.h>
#include <wlc_pm.h>
#include <wlc_scan_utils.h>
#include <wlc_rx.h>
#include <wlc_stf.h>
#include <wlc_dump.h>
#include <wlc_iocv.h>
#include <wlc_lq.h>
#ifdef WLRCC
#include <wlc_okc.h>
#endif /* WLRCC */

#include <wlc_scb.h>
#ifdef WLRSDB
#include <wlc_rsdb.h>
#endif /* WLRSDB */

#if defined(WL_MBO) && !defined(WL_MBO_DISABLED) && defined(MBO_AP)
#include <wlc_mbo.h>
#endif /* WL_MBO && !WL_MBO_DISABLED ** MBO_AP */

#include <wlc_apps.h>
#ifdef WLWNM_AP
static arp_table_t*
wlc_get_bss_cubby_arp_tbl_handle(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);
#endif // endif

#ifdef STA
//#define WBTEXT_BTMDBG
#define WBTEXT_TRACE(args) /* WL_PRINT(x) */
#define WL_WBTEXT(x) /* WL_PRINT(x) */

static int wlc_wnm_bss_pref_score_product(wlc_bsscfg_t *cfg, wlc_bss_info_t *bi,
		int8 bcn_rssi, uint32 *score);
static int wlc_wnm_bss_pref_score_product_wbtext(wlc_bsscfg_t *cfg, wlc_bss_info_t *bi,
		int8 bcn_rssi, uint32 *score);

#define WNM_IS_PRODUCT_POLICY(wnm) \
	((wnm->bsstrans_policy == WL_BSSTRANS_POLICY_PRODUCT) || \
	(wnm->bsstrans_policy == WL_BSSTRANS_POLICY_PRODUCT_WBTEXT))

typedef int (*wnm_bsstrans_pref_score_get_t)(wlc_bsscfg_t *cfg, wlc_bss_info_t *bi,
		int8 bcn_rssi, uint32 *score);

typedef struct wnm_pref_score_ent {
	 wnm_bsstrans_policy_type_t type;
	 wnm_bsstrans_pref_score_get_t get_cb;
} wnm_pref_score_ent_t;

/* Add entries for policy which have an option to calculate score */
static const wnm_pref_score_ent_t wnm_pref_score_entries[] = {
	{WL_BSSTRANS_POLICY_PRODUCT, wlc_wnm_bss_pref_score_product},
	{WL_BSSTRANS_POLICY_PRODUCT_WBTEXT, wlc_wnm_bss_pref_score_product_wbtext},
};

static const size_t wnm_pref_score_num_entries =
    sizeof(wnm_pref_score_entries)/sizeof(wnm_pref_score_entries[0]);
#endif /* STA */

typedef struct wnm_tclas {
	struct wnm_tclas *next;
	uint8 user_priority;
	uint8 fc_len;
	dot11_tclas_fc_t fc;
} wnm_tclas_t;
#define TCLAS_ELEM_FIXED_SIZE	OFFSETOF(wnm_tclas_t, fc)

typedef struct wnm_tfs_filter {
	struct wnm_tfs_filter *next;
	uint32 tclas_cnt;
	wnm_tclas_t *tclas_head;
	uint8 tclas_proc;
	uint8 status;
} wnm_tfs_filter_t;

/* This status is non standard and used internaly before AP response for filter is received */
#define DOT11_TFS_STATUS_NONE	0xFF
typedef struct wnm_tfs_fset {
	struct wnm_tfs_fset *next;
	wnm_tfs_filter_t *filter_head;
	uint8 status;
	uint8 tfs_id;
	uint8 actcode;
	uint8 token;
	uint8 notify;		/* set if notify frame just sent or just received */
} wnm_tfs_fset_t;

typedef struct wnm_dms_scb {
	struct wnm_dms_scb *next;
	struct scb *scb;
} wnm_dms_scb_t;

typedef struct dms_desc {
	struct dms_desc *next;
#ifdef WLWNM_AP
	wnm_dms_scb_t *dms_scb_head;
#endif /* WLWNM_AP */
#ifdef STA
	uint8 user_id;
	uint8 status;
	uint16 last_seqctrl;	/* if status is TERM */
	uint8 token;
	uint8 del;		/* Delete desc when response will be received */
#endif // endif
	uint8 dms_id;
	uint32 tclas_cnt;	/* total tclas number */
	wnm_tclas_t *tclas_head;
	uint8 tclas_proc;
} wnm_dms_desc_t;

/* Life of a DMS desc, or state transistion summary (with cause):
 *
 * DMS_STATUS_DISABLED = 0, DMS desc disabled by user:
 *	-> IN_PROG, NOT_ASSOC, NO_SUPPORT (user add in all cases)
 * DMS_STATUS_ACCEPTED = 1, Request accepted by AP:
 *	-> IN_PROG (user term), TERM (AP decision), NOT_ASSOC (disassoc)
 * DMS_STATUS_NOT_ASSOC = 2, STA not associated
 *	-> DISABLED (user term), IN_PROG (assoc), NOT_SUPPORT (assoc + no DMS)
 * DMS_STATUS_NOT_SUPPORT = 3, DMS not supported by AP
 *	-> DISABLED (user term), NOT_ASSOC (disassoc)
 * DMS_STATUS_IN_PROGRESS = 4, Request just sent (ADD case: dms_id = 0, REMOVE: != 0)
 *	-> ACCEPTED, DENIED, TERM (rx resp), NOT_ASSOC (disassoc)
 * DMS_STATUS_DENIED = 5, Request denied by AP
 *	-> DISABLED (user term), NOT_ASSOC (disassoc)
 * DMS_STATUS_TERM = 6, Request terminated by AP
 *	-> DISABLED (user term), NOT_ASSOC (disassoc)
 */

/* First delay is to check that status has been received from AP for each request sent,
 * Second one is to set up all enabled DMS descriptor after association, to avoid any
 * problem with possible EAPOL or DHCP exchange.
 * Both are in second (but not accurate, timeout at watchdog time) and must be >= 1
 */
#define DMS_DELAY_TIMEOUT	3
#define DMS_DELAY_ASSOC		3
#define DMS_RETRY_CNT		3

#define TFS_DELAY_TIMEOUT	3
#define TFS_DELAY_ASSOC		3
#define TFS_RETRY_CNT		3
#define SLEEP_DELAY_TIMEOUT	3
#define SLEEP_DELAY_RETRY_LATER	5
#define SLEEP_RETRY_CNT		3

#define TIMBC_DELAY_RETRY	3
#define TIMBC_DELAY_ASSOC	3

#define WNM_ELEMENT_LIST_ADD(s, e) \
	do { \
		(e)->next = (s); \
		(s) = (e); \
	} while (0)

#define TFS_NOTIFY_IDLIST_MAX	255
#define TFS_DEFAULT_TCLASTYPE	0x3f	/* enabling bit 5 to bit 0 by default */

typedef struct wnm_dms_info {
#ifdef WLWNM_AP
	uint8 dms_id_new;
#endif /* WLWNM_AP */
	wnm_dms_desc_t *dms_desc_head;
} wnm_dms_info_t;

typedef struct wnm_bsstrans_rateidx2rate500k {
	uint16 cck[WL_NUM_RATES_CCK]; /* 2.4G only */
	uint16 ofdm[WL_NUM_RATES_OFDM]; /* 6 to 54mbps */
	uint16 phy_n[WL_NUM_RATES_MCS_1STREAM]; /* MCS0-7 */
	uint16 phy_ac[WL_NUM_RATES_VHT]; /* MCS0-9 */
} wnm_bsstrans_rateidx2rate500k_t;

typedef struct wnm_bsstrans_roam_throttle {
	uint16 period; /* throttle period in seconds */
	uint16 period_secs; /* Number of seconds into current period */
	uint16 scans_allowed; /* no. of scans allowed in period */
	uint16 scans_done; /* no. of scans done in current period */
} wnm_bsstrans_roam_throttle_t;

/* Neighbor Report element */
typedef struct wnm_nbr_rep {
	struct wnm_nbr_rep *next;
	nbr_element_t nbr_elt;
} wnm_nbr_rep_t;

/* BTM Query Neighbor Report element List */
typedef struct btq_nbr_list {
	struct btq_nbr_list *next;
	nbr_rpt_elem_t nbr_elt;
} btq_nbr_list_t;

typedef struct timer_callback_ctx {
	wlc_wnm_info_t *wnm;		/* used to reach the correct wlc */
	int join_cfgidx;			/* cfg idx linked to the timer */
} timer_cb_ctx_t;

typedef struct wnm_bsstrans_sta_info {
	wnm_bsstrans_roam_throttle_t *throttle; /* throttle params for roam scan */
	wl_bsstrans_rssi_rate_map_t *rssi_rate_map; /* RSSI to rate map */
	wnm_bsstrans_rateidx2rate500k_t *idx2rate; /* rate idx to rate in 500K units */
	struct wl_timer *resp_timer; /* limit wait time for bsstrans_resp tx */
	wlc_bsscfg_t *join_cfg;
	bool resp_pending; /* bsstrans_resp pending for unicast req */
	bool join_pending; /* wait for bsstrans_resp tx to initiate join */
	bool use_score; /* use scoring only if bssload and bss_trans supported by all trgt bss */
	uint8 resp_token;
	uint8 req_mode;
	uint32 scoredelta;
	int8 btm_rssi_thresh;
	/* Neighbor Report element */
	uint16 max_nbr_count;
	uint16 nbrlist_size;
	wnm_nbr_rep_t *nbr_rep_head;
	struct wl_timer *nbrlist_valid_timer;	/* nbr list recvd is valid for this time */
	bool nbrlist_valtimer_active;
	uint32 nonnbr_bestscore;				/* debug info used during dump */
	struct wl_timer *disassoc_imnt_timer;	/* timer to keep current AP blacklisted */
	bool disassoc_imnttmr_active;
	struct ether_addr blklisted_bssid;			/* bssid of current associated AP */
	uint blklisted_bssid_bandunit;				/* blacklisted on this band */
	timer_cb_ctx_t *tmr_cb;				/* timer call back context pointer */
} wnm_bsstrans_sta_info_t;

/* Used for structure allocation only */
typedef struct wnm_bsstrans_sta_info_mem {
	wnm_bsstrans_sta_info_t sta_info_mem;
	wnm_bsstrans_roam_throttle_t throttle_mem;
	wl_bsstrans_rssi_rate_map_t rssi_rate_map_mem;
	wnm_bsstrans_rateidx2rate500k_t idx2rate_mem;
} wnm_bsstrans_sta_info_mem_t;

#define BSSTRANS_MAX_NEIGHBORS 16

#ifdef STA
enum {
	WNM_SLEEP_NONE = 0,		/* Feature not in use */
	WNM_SLEEP_ENTER_WAIT = 1,	/* Request sent to AP, waiting for OK */
	WNM_SLEEP_SLEEPING = 2,		/* STA is sleeping */
	WNM_SLEEP_EXIT_WAIT = 3		/* Exit requested, waiting for OK */
};

/* Wait state failed, get back previous stable state */
#define WNM_SLEEP_STATE_BACK(s)		((s) & ~1)

/* Values for WNM-Sleep Mode */
#define WNM_SLEEP_UPD		2
#define WNM_SLEEP_UPD_INTV	10

typedef struct wnm_bss_cu {
	uint8	cu_ap;
	uint8	cu_self;
	uint8	cu;
} wnm_bss_cu_t;

static int wlc_wnm_bsstrans_update_nbrlist(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg,
	uint8 *body, int *body_len);
static wnm_nbr_rep_t * wlc_wnm_get_neighbor_rep(wlc_wnm_info_t *wnm, wlc_bsscfg_t *cfg,
	struct ether_addr *ea, uint8 channel);
static void wlc_wnm_add_neighbor(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg,
	nbr_element_t *nbr_elt);
static void wlc_wnm_del_all_neighbor(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg);
static void wlc_wnm_nbrlist_validity_timeout(void *context);
static void wlc_wnm_disassocimnt_timeout(void *context);
static uint32 wlc_wnm_get_default_score(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg);
static uint32 wlc_wnm_bsstrans_get_rssi_score(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg,
	uint8 band, uint8 bcn_rssi);
static uint32 wlc_wnm_bsstrans_get_cu_score(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg,
	uint8 band, uint8 qbss_cu);
static void wlc_wnm_bsstrans_get_cu(wlc_bsscfg_t *cfg, wlc_bss_info_t *bi, wnm_bss_cu_t *bss_cu);
static bool wlc_wnm_bsstrans_validnbr_ap(wlc_wnm_info_t *wnm, wlc_bsscfg_t *cfg,
		wlc_bss_info_t *bip);
static int wlc_wnm_add_btq_nbr(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg,
	nbr_rpt_elem_t *nbr_elt);
static btq_nbr_list_t * wlc_wnm_get_btq_nbr(wlc_wnm_info_t *wnm,
		wlc_bsscfg_t *bsscfg, struct ether_addr *ea);
static int wlc_wnm_del_btq_nbr(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg,
	struct ether_addr *bssid);
static int wlc_wnm_del_all_btq_nbr(wlc_wnm_info_t *wnm,
		wlc_bsscfg_t *bsscfg);
static int wlc_wnm_get_btq_nbr_list(wlc_wnm_info_t *wnm,
		wlc_bsscfg_t *bsscfg, wl_btq_nbr_list_t *wl_btq_nbr_list,
		uint buflen);
#ifdef WL_MBO_WFA_CERT
static void wlc_wnm_btq_nbr_ie(wlc_info_t *wlc,
	nbr_rpt_elem_t* btq_nbr_elem, uint8 **bufptr);
#endif /* WL_MBO_WFA_CERT */
/* process BSS Load IE in beacon */
static int wlc_wnm_bssload_process_bl_ie(void *ctx, wlc_iem_parse_data_t *data);

#ifdef WBTEXT_BTMDBG
static void wlc_wnm_print_nbrlist(wlc_wnm_info_t *wnm, wlc_bsscfg_t *cfg);
#endif /* WBTEXT_BTMDBG */

#ifdef STA
#if defined(WNM_BSSTRANS_EXT)
#if defined(BCMDBG) || defined(BCMDBG_DUMP) || defined(WLTEST) || defined(DNG_DBGDUMP)
static int wlc_wbtext_dump(wlc_info_t *wlc, struct bcmstrbuf *b);
#endif /* BCMDBG || BCMDBG_DUMP || WLTEST || DNG_DBGDUMP */
#endif /* WNM_BSSTRANS_EXT */
#endif /* STA */

/* maximum number of BSS entries limited to ~1K action frame size */
#define MAX_BSS_LIST_SIZE	1024
#endif /* STA */

#define WLC_QBSS_CHAN_FREE_DEFAULT 0x7f /* default channel free score for unsupported AP */

#ifdef WLWNM_AP
/* wnm flags */
#define WNM_BTQ_RESP_DISABLED 0x01

#define WNM_BSSTRANS_REQ_VALIDITY_INTERVAL	100

enum {
	BSSTRANS_INIT = 0,
	BSSTRANS_WAIT_FOR_BSS_TERM = 1,
	BSSTRANS_WAIT_FOR_BSS_TERM_DELAY = 2,
	BSSTRANS_WAIT_FOR_BSS_ENABLED = 3,
	BSSTRANS_WAIT_FOR_DISASSOC = 4,
	BSSTRANS_WAIT_FOR_BSS_DOWN = 5
};

typedef struct wnm_bsstrans_req_info {
	uint16 disassoc_tmr;
	uint8 validity_intrvl;
	uint32 tsf_l;
	uint32 tsf_h;
	uint16 dur;
	uint32 bsstrans_list_len;	/* BSS trans candidate list entries length */
	uint8 *bsstrans_list;		/* BSS trans candidate list entries length */

	struct wl_timer *timer;
	uint32 timer_state;
	uint32 status;			/* use bit[8:0] to record all STA's response status. */
	uint8 delay;
	uint8 reqmode;
} wnm_bsstrans_req_info_t;

#define	WNM_PARP_TABLE_SIZE		32	/* proxyarp hash table bucket size */
#define	WNM_PARP_TABLE_MASK		0x1f	/* proxyarp hash table index mask */
#define	WNM_PARP_TABLE_INDEX(val)	(val & WNM_PARP_TABLE_MASK)

#define	WNM_PARP_TIMEOUT		600	/* proxyarp cache entry timerout duration(10 min) */
#define WNM_PARP_IS_TIMEOUT(wlc, entry)	(wlc->pub->now - entry->used > WNM_PARP_TIMEOUT)
#define	WNM_PARP_ANNOUNCE_WAIT		2	/* proxyarp announce wait duration(2 sec) */
#define WNM_PARP_ANNOUNCE_WAIT_REACH(wlc, entry) \
					(wlc->pub->now - entry->used > WNM_PARP_ANNOUNCE_WAIT)

#define ALIGN_ADJ_BUFLEN		2 /* Adjust for ETHER_HDR_LEN pull in linux */

typedef struct {
	void *p;			/* high rate TIM frame */
	void *p_low;			/* basic rate TIM frame */
	uint8 check_beacon;
} wnm_timbc_fset_t;

#endif /* WLWNM_AP */

#define BW_20_MULTIPLIER 1
#define BW_40_MULTIPLIER 2
#define BW_80_MULTIPLIER 4

/* Default values of good rssi and cu */
#define WNM_DEFAULT_RSSI 70
#define WNM_DEFAULT_CU  60

typedef struct wnm_bss_select_factor {
	uint8 low;
	uint8 high;
	uint8 factor;
	uint8 pad;
} wnm_bss_select_factor_t;

typedef struct wnm_bss_select_table {
	uint16 rssi_weightage;
	uint16 cu_weightage;
	uint16 rssi_table_size;
	uint16 cu_table_size;
	wnm_bss_select_factor_t *rssi_table;
	wnm_bss_select_factor_t *cu_table;
} wnm_bss_select_table_t;

/* XXX allocate the struct and reserve a pointer to the struct in the bsscfg
 * as the bsscfg cubby when this structure grows larger than a pointer...
 */
typedef struct {
	uint32	cap;			/* 802.11v capabilities */
	uint16	bss_max_idle_period;	/* 802.11v BSS Max Idle Period(Unit: 1000TU) */
	uint8	bss_idle_opt;		/* 802.11v BSS Max Idle Period Options */
	wnm_dms_info_t dms_info;	/* DMS descriptor for each specific BSS */
#ifdef WLWNM_AP
	int16	timbc_offset;		/* 802.11v TIM Broadcast offset(Uint: 1ms) */
	int32	timbc_rate;		/* 802.11v TIM Broadcast rate(Uint: 500K) */
	bool	timbc_tsf;		/* 802.11v TIM Broadcast TSF field on/off */
	uint16	timbc_fix_intv;		/* 802.11v TIM Broadcast interval override */
	bool	timbc_ie_update;	/* critical IE included in current beacon */
	wnm_timbc_fset_t timbc_frame;	/* 802.11v TIM Broadcast frame set */
	uint8	timbc_dsie;		/* static saved DS IE for updating check_beacon */
	uint8	timbc_htie[HT_ADD_IE_LEN];	/* static saved DS IE for updating check_beacon */
#endif /* WLWNM_AP */
#ifdef STA
	uint	dms_timer;		/* based on wlc->pub->now */

	wl_timbc_set_t timbc_set;	/* TIM BC settings requested by user */
	wl_timbc_status_t timbc_stat;	/* TIM BC settings from current AP */
	uint8	timbc_last_interval;	/* TIM BC interval sent in last req */
	uint	timbc_timer_sta;	/* based on wlc->pub->now */

	uint16	sleep_intv;		/* 802.11v WNM-Sleep Interval in unit of DTIM */
	uint8	sleep_state;		/* 802.11v WNM-Sleep mode state */
	uint8	sleep_req_cnt;		/* 802.11v WNM-Sleep request number of retries */
	uint8	sleep_pm_saved;		/* pre PM state to restore */
	uint	sleep_timer;		/* based on wlc->pub->now */
#endif /* STA */
#ifdef WLWNM_AP
	wnm_bsstrans_req_info_t bsstrans_req_info;	/* bss transition status variables */
	arp_table_t *phnd_arp_table;		/* handle to arp table */
#endif /* WLWNM_AP */
	uint16	keepalive_count;	/* nmbr of keepalives per bss_max_idle period */
	uint8	mkeepalive_index;	/* mkeepalive_index for keepalive frame to be used */
	uint16  ka_max_interval;	/* Keepalive_max_interval in seconds */
	uint32 pm_ignore_bcmc; /* conditions in which BC/MC traffic is ignored in PM mode */
#ifdef STA
	uint32	not_intim_cont;	/* number of ireceived Beacons without TIM bit set */
	uint	dms_req_cnt;		/* to limit request retries */
	uint	tfs_timer;			/* based on wlc->pub->now */
	uint	tfs_req_cnt;		/* to limit request retries */
	wnm_tfs_fset_t *tfs_fset_head;	/* TFS filter sets */
#endif /* STA */
	bool	chainable;		/* Flag for the handle_data_pkts */

	uint8	cu_trigger_percent; /* cu_trigger from roam profile */
	uint8	cu_avg_calc_dur; /* cu_calc_duration from roam profile */

	uint8	load_avg_cu_ap; /* avg_cu_ap in percentage */
	uint32	load_next_window; /* next avg_window in seconds */
	uint32	load_chan_util; /* sum_cu_ap */

	uint32	load_cu_self; /* sum_su_self */
	uint32	load_avg_cu_self; /* avg_cu_self in percentage */

	uint16	load_bcn_count; /* running counter to measure number of beacons in avg_window */
	uint16	load_bcn_per_window; /* number of bescon in last avg_window */

	uint8	load_current_cu_effective; /* in percentage */
	uint8	load_current_cu_self; /* in percentage */
	bool	trigger_rssi_cu_roam;
	wnm_bsstrans_sta_info_t *bsstrans_stainfo;
	wnm_bss_select_table_t *bss_select_table[MAXBANDS];
	uint8		btq_reason;
	/* BTQ NBR params */
	uint16		max_btq_nbr_count;
	uint16		btq_nbrlist_size;
	btq_nbr_list_t *btq_nbr_list_head; /* BTM query NBR list head */
	struct wl_timer *disassoc_timer;
	uint32 disassoc_timer_state;
	struct ether_addr sta_mac;
#ifdef WLWNM_AP
	wl_bsstrans_req_t	bsstrans_req_param;
	bool		bsstrans_req_param_configured;
	uint		msec_to_bss_down;
	uint8		flags;
#endif /* WLWNM_AP */
} wnm_bsscfg_cubby_t;

#define WNM_BSSCFG_CUBBY(wnm, cfg) ((wnm_bsscfg_cubby_t*)BSSCFG_CUBBY(cfg, (wnm)->cfgh))

#define WLC_WNM_UPDATE_TOKEN(i) (++(i)? (i) : ++(i))

#if defined(STA) && defined(WNM_BSSTRANS_EXT)
static int wlc_wnm_set_bss_select_table(wnm_bss_select_factor_cfg_t *btcfg, void *params,
	wnm_bsscfg_cubby_t *wnm_cfg);
static int wlc_wnm_get_bss_select_table(wnm_bss_select_factor_cfg_t *btcfg, void *params,
	uint len, wnm_bsscfg_cubby_t *wnm_cfg);
#endif /* STA && WNM_BSSTRANS_EXT */

typedef struct {
#ifdef WLWNM_AP
	uint32		rx_tstamp;		/* last frame received(BSS Max Idle Period) */
	uint8		timbc_status;		/* TIM Broadcast status */
	uint8		timbc_interval;		/* TIM Broadcast interval */
	ratespec_t	timbc_high_rate;	/* high rate cached for fast referencing */
	wnm_tfs_fset_t	*tfs_fset_head;		/* TFS filter sets linked list */
	bool		sleeping;		/* wnm-sleep sleeping/non-sleeping */
	uint16		sleep_interval;		/* wnm-sleep sleeping interval, allow value 0 */
#ifdef MFP
	bool		key_exp;		/* wnm-sleep key expired and need to update */
#endif /* MFP */

	bool		spp_conflict;		/* AMSDU SPP cap/req conflict(DMS) */
	uint8		bsstrans_token;
	struct {
		uint32 len;
		uint8 *data;
	} timbc_resp, dms_resp, fms_resp;
#endif /* WLWNM_AP */
	uint32		cap;			/* scb supporting features */
} wnm_scb_cubby_t;

#define SCB_WNM_CUBBY(wnm, scb) ((wnm_scb_cubby_t*)SCB_CUBBY((scb), (wnm)->scb_handle))
#define WLC_WNM_RESP_MAX_BUFSIZE		512

#define BSS_MAXTABLE_SIZE 10
#define BSS_TABLE_NUMBER 2

typedef struct wlc_wnm_cmn_info {
	wnm_tclas_t	*tclas_head;	/* for tclas IOVAR command */
	uint32		tclas_cnt;
	uint32	dms_dependency;
} wlc_wnm_cmn_info_t;

struct wlc_wnm_info {
	wlc_info_t	*wlc;
	int			cfgh;			/* wnm bsscfg cubby handle */
	uint8		req_token;		/* token used in measure requests from us */
#ifdef WLWNM_AP
	uint8		url_len;	/* session information URL length */
	uint8		*url;		/* session information URL */
	struct wl_timer	*bss_idle_timer; /* timer for 100ms */
	uint32		timestamp;	/* increate each 100ms */
	uint8		tfs_tclastype;	/* bitmask of tclas type that tfs support */
	bool		parp_discard;	/* discard ARP/ICMP6 frame if no proxyarp entry */
	bool		parp_allnode;	/* reply to all-nodes multicast L2 DST */
#endif /* WLWNM_AP */
	int			scb_handle;	/* scb cubby handle to retrieve data from scb */
#ifdef STA
	uint8		bsstrans_policy;
#endif /* STA */
	wlc_wnm_cmn_info_t *cmn;
};

#define BSSTRANS_DEF_RSSIWEIGHT 70
#define BSSTRANS_DEF_CUWEIGHT 30

/* macro to control alloc and dealloc for sta only entity */
#define WNM_BSSCFG_STA(cfg) (BSSCFG_INFRA_STA(cfg) && ((cfg)->type == BSSCFG_TYPE_GENERIC))

#ifdef STA
static wl_bsstrans_rssi_t cck[WL_NUM_RATES_CCK] = {{-90, 0}, {-89, 0}, {-89, 0}, {-89, 0}};
static wl_bsstrans_rssi_t ofdm[WL_NUM_RATES_OFDM] = {{-90, -91}, {-90, -91}, {-90, -90},
	{-89, -88}, {-86, -86}, {-83, -83}, {-79, -78}, {-77, -78}};
static wl_bsstrans_rssi_t phy_n_1x1[WL_NUM_RATES_MCS_1STREAM] = {{-90, -91}, {-89, -91},
	{-88, -88}, {-87, -87}, {-83, -84}, {-79, -82}, {-76, -78}, {-76, -75}};
static wl_bsstrans_rssi_t phy_ac_1x1[WL_NUM_RATES_VHT] = {{-90, -92}, {-90, -91},
	{-90, -90}, {-88, -89}, {-85, -84}, {-80, -81}, {-80, -79}, {-77, -78},
	{-74, -72}, {-71, -69}};
static wl_bsstrans_rssi_t phy_n_2x2[WL_NUM_RATES_MCS_1STREAM] = {{-92, -93}, {-92, -93},
	{-90, -91}, {-88, -89}, {-85, -86}, {-83, -81}, {-80, -81}, {-80, -78}};
static wl_bsstrans_rssi_t phy_ac_2x2[WL_NUM_RATES_VHT] = {{-92, -93}, {-91, -91},
	{-88, -89}, {-86, -87}, {-83, -84}, {-78, -78}, {-78, -78}, {-75, -75},
	{-72, -69}, {-67, -63}};

static uint16 cck_rate[] = { 2, 4, 11, 22};
static uint16 ofdm_rate[] = { 12, 18, 24, 36, 48, 72, 96, 108};
static uint16 phy_n_rate[] = { 13, 26, 39, 52, 78, 104, 117, 130};
static uint16 phy_ac_rate[] = { 13, 26, 39, 52, 78, 104, 117, 130, 156, 172};
static wnm_bss_select_factor_t rssi_table[] =
	{{0, 45, 100, 0}, {45, 55, 90, 0}, {55, 65, 70, 0}, {65, 75, 50, 0}, {75, 128, 20, 0}};
static wnm_bss_select_factor_t cu_table[] =
	{{0, 10, 100, 0}, {10, 30, 90, 0}, {30, 50, 70, 0}, {50, 70, 50, 0}, {70, 100, 20, 0}};
#endif /* STA */

/* iovar table */
enum wlc_wnm_iov {
	IOV_WNM = 1,			/* enable/dsiable WNM */
	IOV_WNM_MAXIDLE = 2,		/* 802.11v-2011 11.22.12 BSS Max Idle Period */
	IOV_WNM_TIMBC_OFFSET = 3,	/* 802.11v-2011 11.2.1.15 TIM broadcast */
	IOV_WNM_BSSTRANS_URL = 4,	/* config session information URL */
	IOV_WNM_BSSTRANS_REQ = 5,
	IOV_WNM_TFS_TCLASTYPE = 6,	/* config tclas support type of tfs service */
	IOV_WNM_PARP_DISCARD = 7,
	IOV_WNM_PARP_ALLNODE = 8,
	IOV_WNM_TIMBC_SET = 9,		/* 802.11v-2011 11.2.1.15 TIM broadcast */
	IOV_WNM_TIMBC_STATUS = 10,	/* 802.11v-2011 11.2.1.15 TIM broadcast */
	IOV_WNM_DMS_SET = 11,
	IOV_WNM_DMS_TERM = 12,
	IOV_WNM_SERVICE_TERM = 13,
	IOV_WNM_SLEEP_INTV = 14,	/* 802.11v-2011 11.2.1.16 WNM-Sleep Mode Interval */
	IOV_WNM_SLEEP_MODE = 15,	/* send WNM-Sleep request frame to enter/exit sleep mode */
	IOV_WNM_BSSTRANS_QUERY = 16,	/* send bss transition management query frame */
	IOV_WNM_BSSTRANS_RESP = 17,	/* Set behavior when receiving BSS Trans Req */
	IOV_WNM_TCLAS_ADD = 18,		/* add one tclas element */
	IOV_WNM_TCLAS_DEL = 19,		/* delete one tclas element */
	IOV_WNM_TCLAS_LIST = 20,	/* list all added tclas element */
	IOV_WNM_DMS_STATUS = 21,	/* list all accepted dms descriptors */
	IOV_WNM_KEEPALIVES_MAX_IDLE = 22,
	IOV_WNM_PM_IGNORE_BCMC = 23,
	IOV_WNM_DMS_DEPENDENCY = 24,	/* Conditions which must be true for sending DMS */
	IOV_WNM_BSSTRANS_RSSI_RATE_MAP = 25,	/* rssi to rate map for AP scoring */
	IOV_WNM_BSSTRANS_ROAMTHROTTLE = 26,
	IOV_WNM_SCOREDELTA = 27,
	IOV_WNM_BTM_RSSI_THRESH = 28,
	IOV_WNM_TFS_SET = 29,		/* add one tfs request element */
	IOV_WNM_TFS_TERM = 30,
	IOV_WNM_BSS_SELECT_TABLE = 31,
	IOV_WNM_BSS_SELECT_WEIGHT = 32,
	IOV_WNM_TFS_STATUS = 33,
	IOV_WNM_BTQ_NBR_ADD = 34,   /* Add NBR elem to BTQ list */
	IOV_WNM_BTQ_NBR_DEL = 35,   /* Deletes NBR elem from BTQ list */
	IOV_WNM_BTQ_NBR_LIST = 36,  /* List/Fetches all NBR elems of BTQ list */
	IOV_WNM_BSSTRANS_REQ_PARAM = 37,
	IOV_WNM_NO_BTQ_RESP = 38, /* prevent sending BTM REQ in response to a BTQ */
	IOV_LAST
};

static const bcm_iovar_t wnm_iovars[] = {
	{"wnm", IOV_WNM, (0), 0, IOVT_UINT32, 0},
	{"wnm_maxidle", IOV_WNM_MAXIDLE, (0), 0, IOVT_BUFFER, 0},
#ifdef WLWNM_AP
	{"wnm_timbc_offset", IOV_WNM_TIMBC_OFFSET, IOVF_SET_UP|IOVF_BSSCFG_AP_ONLY, 0, IOVT_BUFFER,
	sizeof(wl_timbc_offset_t)},
	{"wnm_bsstrans_req", IOV_WNM_BSSTRANS_REQ, IOVF_SET_UP|IOVF_BSSCFG_AP_ONLY, 0, IOVT_BUFFER,
	0},
	{"wnm_url", IOV_WNM_BSSTRANS_URL, IOVF_BSSCFG_AP_ONLY, 0, IOVT_BUFFER, 0},
	{"wnm_parp_discard", IOV_WNM_PARP_DISCARD, IOVF_BSSCFG_AP_ONLY, 0, IOVT_UINT8, 0},
	{"wnm_parp_allnode", IOV_WNM_PARP_ALLNODE, IOVF_BSSCFG_AP_ONLY, 0, IOVT_UINT8, 0},
#endif /* WLWNM_AP */
#ifdef STA
	{"wnm_timbc_set", IOV_WNM_TIMBC_SET, IOVF_BSSCFG_STA_ONLY, 0, IOVT_BUFFER,
	sizeof(wl_timbc_set_t)},
	{"wnm_timbc_status",
	IOV_WNM_TIMBC_STATUS, IOVF_BSSCFG_STA_ONLY, 0, IOVT_BUFFER, sizeof(wl_timbc_status_t)},
	{"wnm_dms_set", IOV_WNM_DMS_SET, IOVF_BSSCFG_STA_ONLY, 0, IOVT_BUFFER,
	sizeof(wl_dms_set_t)},
	{"wnm_dms_term",
	IOV_WNM_DMS_TERM, IOVF_BSSCFG_STA_ONLY, 0, IOVT_BUFFER, sizeof(wl_dms_term_t)},
	{"wnm_service_term",
	IOV_WNM_SERVICE_TERM, IOVF_BSSCFG_STA_ONLY, 0, IOVT_BUFFER, sizeof(wl_service_term_t)},
	{"wnm_sleep_intv", IOV_WNM_SLEEP_INTV, IOVF_BSSCFG_STA_ONLY, 0, IOVT_UINT16, 0},
	{"wnm_sleep_mode", IOV_WNM_SLEEP_MODE, IOVF_BSSCFG_STA_ONLY, 0, IOVT_BOOL, 0},
	{"wnm_bsstrans_query",
	IOV_WNM_BSSTRANS_QUERY, IOVF_SET_UP|IOVF_BSSCFG_STA_ONLY, 0, IOVT_BUFFER, 0},
	{"wnm_bsstrans_resp",
	IOV_WNM_BSSTRANS_RESP, IOVF_BSSCFG_STA_ONLY | IOVF_RSDB_SET, 0, IOVT_UINT8, 0},
#endif /* STA */
	{"wnm_btq_nbr_add",
	IOV_WNM_BTQ_NBR_ADD, 0, 0, IOVT_BUFFER, 0},
	{"wnm_btq_nbr_del",
	IOV_WNM_BTQ_NBR_DEL, 0, 0, IOVT_BUFFER, 0},
	{"wnm_btq_nbr_list",
	IOV_WNM_BTQ_NBR_LIST, 0, 0, IOVT_BUFFER, 0},
	{"tclas_add", IOV_WNM_TCLAS_ADD, (0), 0, IOVT_BUFFER, 0},
	{"tclas_del", IOV_WNM_TCLAS_DEL, (0), 0, IOVT_BUFFER, 0},
	{"tclas_list", IOV_WNM_TCLAS_LIST, (0), 0, IOVT_BUFFER, 0},
	{"wnm_dms_status", IOV_WNM_DMS_STATUS, (0), 0, IOVT_BUFFER, 0},
#ifdef STA
#ifdef KEEP_ALIVE
	{"wnm_keepalives_max_idle", IOV_WNM_KEEPALIVES_MAX_IDLE, (0), 0, IOVT_BUFFER, 0},
#endif /* KEEP_ALIVE */
	{"wnm_pm_ignore_bcmc", IOV_WNM_PM_IGNORE_BCMC, 0, 0, IOVT_BUFFER, 0},
	{"wnm_dms_dependency", IOV_WNM_DMS_DEPENDENCY, IOVF_BSSCFG_STA_ONLY, 0, IOVT_UINT32, 0},
	{"wnm_bsstrans_rssi_rate_map",
	IOV_WNM_BSSTRANS_RSSI_RATE_MAP, IOVF_BSSCFG_STA_ONLY, 0, IOVT_BUFFER, 0},
	{"wnm_bsstrans_roamthrottle",
	IOV_WNM_BSSTRANS_ROAMTHROTTLE, IOVF_BSSCFG_STA_ONLY, 0, IOVT_UINT32, 0},
	{"wnm_scoredelta",
	IOV_WNM_SCOREDELTA, IOVF_BSSCFG_STA_ONLY, 0, IOVT_UINT32, 0},
	{"wnm_btm_rssi_thresh",
	IOV_WNM_BTM_RSSI_THRESH, IOVF_BSSCFG_STA_ONLY, 0, IOVT_INT8, 0},
	{"wnm_tfs_set", IOV_WNM_TFS_SET, IOVF_BSSCFG_STA_ONLY, 0, IOVT_BUFFER,
	sizeof(wl_tfs_set_t)},
	{"wnm_tfs_term",
	IOV_WNM_TFS_TERM, IOVF_BSSCFG_STA_ONLY, 0, IOVT_BUFFER, sizeof(wl_tfs_term_t)},
#ifdef WNM_BSSTRANS_EXT
	{"wnm_bss_select_table", IOV_WNM_BSS_SELECT_TABLE, IOVF_BSSCFG_STA_ONLY, 0,
	IOVT_BUFFER, sizeof(wnm_bss_select_factor_cfg_t)},
	{"wnm_bss_select_weight", IOV_WNM_BSS_SELECT_WEIGHT, IOVF_BSSCFG_STA_ONLY, 0,
	IOVT_BUFFER, sizeof(wnm_bss_select_weight_cfg_t)},
#endif /* WNM_BSSTRANS_EXT */
#endif /* STA */
	{"wnm_tfs_status", IOV_WNM_TFS_STATUS, 0, 0, IOVT_BUFFER, 0},
#ifdef WLWNM_AP
	{"wnm_bsstrans_req_param", IOV_WNM_BSSTRANS_REQ_PARAM, IOVF_SET_UP|IOVF_BSSCFG_AP_ONLY,
	0, IOVT_BUFFER, sizeof(wl_bsstrans_req_t)},
	{"wnm_no_btq_resp", IOV_WNM_NO_BTQ_RESP, IOVF_BSSCFG_AP_ONLY, 0, IOVT_UINT32, 0},
#endif /* WLWNM_AP */
	{NULL, 0, 0, 0, 0, 0}
};

static int wlc_wnm_doiovar(void *hdl, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif);
static void wlc_wnm_watchdog(void *context);
static int wlc_wnm_bsscfg_init(void *context, wlc_bsscfg_t *cfg);
static void wlc_wnm_bsscfg_deinit(void *context, wlc_bsscfg_t *cfg);
static int wlc_wnm_wlc_up(void *ctx);
static int wlc_wnm_wlc_down(void *ctx);
static int wlc_wnm_tfs_status(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg,
	wl_tfs_status_t *list, int list_len);
#ifdef BCMDBG
static void wlc_wnm_bsscfg_dump(void *context, wlc_bsscfg_t *cfg, struct bcmstrbuf *b);
#endif /* BCMDBG */
static int wlc_wnm_scb_init(void *context, struct scb *scb);
static void wlc_wnm_scb_deinit(void *context, struct scb *scb);
#ifdef BCMDBG
static void wlc_wnm_scb_dump(void *context, struct scb *scb, struct bcmstrbuf *b);
#else
#define wlc_wnm_scb_dump	NULL
#endif /* BCMDBG */
static void wlc_wnm_dms_free(wlc_bsscfg_t *bsscfg);
static int wlc_wnm_dms_scb_cleanup(wlc_info_t *wlc, struct scb *scb);
#ifdef BCMDBG
static int wlc_wnm_tclas_delete(wlc_info_t *, wnm_tclas_t **, uint32, uint32);
#endif // endif
static int wlc_wnm_tclas_ie_prep(wnm_tclas_t *tclas_head, uint8 *buf);
static bool wlc_wnm_tclas_match(wnm_tclas_t *tclas, frame_proto_t *fp, bool allmatch);
#ifdef WLWNM_AP
static void wlc_wnm_tfs_req_free(wlc_wnm_info_t *wnm, wnm_tfs_fset_t **tfs_fset_head, int tfs_id);
static void wlc_wnm_bss_idle_timer(void *context);
static uint wlc_wnm_ars_calc_maxidle_ie_len(void *ctx, wlc_iem_calc_data_t *data);
static int wlc_wnm_ars_write_maxidle_ie(void *ctx, wlc_iem_build_data_t *data);
static int wlc_wnm_prep_timbc_resp_ie(wlc_info_t *wlc, uint8 *p, int *plen, struct scb *scb);
static int wlc_wnm_send_timbc_resp_frame(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg,
	struct scb *scb, uint8 token);
static int wlc_wnm_timbc_switch(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);
static void wlc_wnm_timbc_clrframe(wlc_info_t *wlc, wnm_bsscfg_cubby_t *wnm_cfg);
static bool wlc_wnm_timbc_support(wlc_info_t *wlc);
static void wlc_wnm_timbc_tbtt(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, wnm_bsscfg_cubby_t *wnm_cfg);
static void wlc_wnm_send_timbc_act_frame(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, void *p);
static int wlc_wnm_arq_parse_timbc_ie(void *ctx, wlc_iem_parse_data_t *data);
static uint wlc_wnm_ars_calc_timbc_ie_len(void *ctx, wlc_iem_calc_data_t *data);
static int wlc_wnm_ars_write_timcb_ie(void *ctx, wlc_iem_build_data_t *data);
static int wlc_wnm_send_tfs_resp_frame(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb,
	uint8 *tfs_resp_ie, int tfs_resp_ie_len, uint8 token);
static int wlc_wnm_parse_tfs_req_tclas(wlc_info_t *wlc, wnm_tfs_filter_t *tfs_req_subelem,
	uint8 *body, uint8 body_len);
static int wlc_wnm_parse_tfs_req_subelem(wlc_info_t *wlc, wnm_tfs_fset_t *tfs_req, uint8 *body,
	uint8 body_len, uint8 *buf, uint8 *resp_len, bool *valid);
static int wlc_wnm_parse_tfs_req_ie(wlc_info_t *wlc, struct scb *scb, uint8 *body,
	int body_len, uint8 *buf, int *buf_len);
static int wlc_wnm_send_tfs_notify_frame(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb,
	bcm_tlv_t *idlist);
static int wlc_wnm_parse_tfs_notify_resp(wlc_info_t *wlc, struct scb *scb, uint8 *body,
	int body_len);
static bool wlc_wnm_tfs_packet_handle(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb,
	frame_proto_t *fp);
static int wlc_wnm_dms_req_parse_resp_send(wlc_bsscfg_t *, struct scb *, uint8 *, int);
static int wlc_wnm_parse_sleep_req_ie(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb,
	uint8 *body, int body_len, uint8 *buf, int *buf_len);
static int wlc_wnm_send_sleep_resp_frame(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb,
	uint8 token, uint8 *buf, int buf_len);
static void wlc_wnm_bsstrans_req_timer(void *arg);
static void wlc_wnm_bsstrans_disassoc_timer(void *arg);
static void wlc_wnm_send_bsstrans_request(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg,
	struct scb *scb, uint8 token, uint8 reqmode, uint8 delay, uint8 reason, bool add_self);
static void wlc_wnm_parse_bsstrans_resp(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg,
	struct scb *scb, uint8 *body, int body_len);

static void wlc_wnm_parp_watchdog(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, bool all, uint8 *del_ea,
	bool periodic);
static void wlc_wnm_ap_btq_nbr_ie(wlc_info_t *wlc,
	nbr_rpt_elem_t* btq_nbr_elem, uint8 **bufptr, struct scb *scb, uint8 reqmode);
#if defined(WL_MBO) && !defined(WL_MBO_DISABLED) && defined(MBO_AP)
static int wlc_wnm_send_notif_resp_frame(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb,
	uint8 token, uint8 status);
#endif /* WL_MBO && !WL_MBO_DISABLED */
#endif /* WLWNM_AP */

#ifdef STA
static void wlc_wnm_tfs_free(wlc_bsscfg_t *bsscfg);
static int wlc_wnm_tfs_resp_ie_parse(wlc_info_t *wlc, wnm_bsscfg_cubby_t* wnm_cfg,
	uint8 *p, int len, int token);
static int wlc_wnm_tfs_req_frame_send(wlc_bsscfg_t *bsscfg);
static int wlc_wnm_dms_resp_frame_parse(wlc_bsscfg_t *bsscfg, int token, uint8 *body, int body_len);
static int wlc_wnm_dms_req_frame_send(wlc_bsscfg_t *bsscfg, uint type, uint user_id);
static void wlc_wnm_sleep_req_frame_send(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg);
static int wlc_wnm_sleep_req_ie_prep(wlc_wnm_info_t *wnm, wnm_bsscfg_cubby_t *wnm_cfg, uint8 *p);
static int wlc_wnm_sleep_resp_frame_parse(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg,
	struct scb *scb, uint8 *body, int body_len);
static int wlc_wnm_sleep_resp_keydata_parse(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg,
	struct scb *scb, uint8 *key, int len);
static int wlc_wnm_sleep_resp_ie_parse(wlc_wnm_info_t *, struct scb *, uint8 *, int, int);
static int wlc_wnm_set_sleep_mode(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	wlc_wnm_info_t *wnm, wnm_bsscfg_cubby_t *wnm_cfg, int int_val);
static int wlc_wnm_get_sleep_mode(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	wlc_wnm_info_t *wnm, wnm_bsscfg_cubby_t *wnm_cfg, int *ret_int_val);
static int wlc_wnm_set_sleep_intv(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	wlc_wnm_info_t *wnm, wnm_bsscfg_cubby_t *wnm_cfg, int int_val);
static int wlc_wnm_get_sleep_intv(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	wlc_wnm_info_t *wnm, wnm_bsscfg_cubby_t *wnm_cfg, int *ret_int_val);
static int wlc_wnm_timbc_req_frame_send(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg, int int_val);
static int wlc_wnm_timbc_state_update(wlc_wnm_info_t *, wlc_bsscfg_t *, wnm_bsscfg_cubby_t*);
static int wlc_wnm_sleep_add_tfs_filter(wlc_bsscfg_t *cfg);
static int wlc_wnm_sleep_remove_tfs_filter(wlc_bsscfg_t *cfg);

#ifdef KEEP_ALIVE
static int wlc_wnm_set_keepalive_max_idle(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	wnm_bsscfg_cubby_t *wnm_cfg);
static int wlc_wnm_bss_max_idle_ie_process(void *ctx, wlc_iem_parse_data_t *data);
#endif /* KEEP_ALIVE */
static void wlc_wnm_pm_ignore_bcmc_upd(wlc_bsscfg_t *bsscfg, wnm_bsscfg_cubby_t *wnm_cfg);
static void* wlc_frame_get_dms_req(wlc_info_t *wlc, wlc_wnm_info_t *wnm,
	uint32 dms_desc_list_len, uint8 **pbody);
static void wlc_bsstrans_resp_tx_complete(wlc_info_t *wlc, uint txstatus, void *arg);
static void wlc_wnm_neighbor_report_ie(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	wlc_bss_info_t *bi, uint8 **bufptr, uint32 max_score);
#endif /* STA */

#ifdef WLWNM_BRCM
static int
wlc_wnm_bss_parse_brcm_ie(void *ctx, wlc_iem_parse_data_t *data);
static void
wlc_wnm_process_brcm_ie(wlc_info_t *wlc, struct scb *scb, brcm_ie_t *brcm_ie);
#endif /* WLWNM_BRCM */

static void wlc_wnm_process_extcap_ie(wlc_info_t *wlc,  bcm_tlv_t *ie, struct scb *scb);
static int wlc_wnm_bss_parse_ext_cap_ie(void *ctx, wlc_iem_parse_data_t *data);

static void
wlc_wnm_dms_desc_free(wlc_info_t *wlc, wnm_dms_desc_t *dms_desc);
static int wlc_wnm_dms_status(wnm_bsscfg_cubby_t *wnm_cfg, wl_dms_status_t *dms_list, int list_len);

#ifdef STA
static int
wlc_wnm_dms_term(wlc_bsscfg_t *bsscfg, wnm_bsscfg_cubby_t *wnm_cfg, wl_dms_term_t *iov_term);
static int
wlc_wnm_dms_set(wlc_bsscfg_t *bsscfg, wnm_bsscfg_cubby_t *wnm_cfg, wl_dms_set_t *iov_set);
static void wlc_wnm_bsstrans_query_scancb(void *arg, int status, wlc_bsscfg_t *bsscfg);
static int
wlc_wnm_bsstrans_query_send(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg, wlc_bss_list_t *list);
static void wlc_wnm_bsstrans_resp_timeout(void *context);
static bool
wlc_wnm_rateset_contain_rate(wlc_rateset_t *rateset, uint8 rate, bool is_ht, uint32 nss);
static int wlc_wnm_tfs_set(wlc_bsscfg_t *bsscfg, wnm_bsscfg_cubby_t *wnm_cfg,
	wl_tfs_set_t *tfs_set);
static int wlc_wnm_tfs_term(wlc_bsscfg_t *bsscfg, wnm_bsscfg_cubby_t *wnm_cfg,
	wl_tfs_term_t *tfs_term);
#endif /* STA */

#ifdef WLWNM_AP
static arp_table_t* wlc_wnm_init_l2_arp_table(osl_t* osh);
static void wlc_wnm_deinit_l2_arp_table(osl_t* osh, arp_table_t* ptable);
#endif /* WLWNM_AP */

#if defined(STA) && defined(WNM_BSSTRANS_EXT)
static void
wnm_bsstrans_set_values(uint8 band, void *ptr_5g, void *ptr_2g, void *data, uint8 size);
#endif /* STA && WNM_BSSTRANS_EXT */

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

/* TODO REPLACE BY TCLAS_DEL(xx, yy, 0, -1U) */
static void wnm_tclas_free(wlc_info_t *wlc, wnm_tclas_t *curr)
{
	while (curr) {
		wnm_tclas_t *next = curr->next;
		MFREE(wlc->osh, curr, TCLAS_ELEM_FIXED_SIZE + curr->fc_len);
		curr = next;
	}
}

#ifdef WLRSDB
typedef struct {
	uint8	load_avg_cu_ap; /* avg_cu_ap in percentage */
	uint32	load_next_window; /* next avg_window in seconds */
	uint32	load_chan_util; /* sum_cu_ap */

	uint32	load_cu_self; /* sum_su_self */
	uint32	load_avg_cu_self; /* avg_cu_self in percentage */

	uint16	load_bcn_count; /* running counter to measure number of beacons in avg_window */
	uint16	load_bcn_per_window; /* number of bescon in last avg_window */
	wnm_bsstrans_sta_info_t *bsstrans_stainfo;	/* pointer to sta-related wnm info */
	wnm_bss_select_table_t *bss_select_table[MAXBANDS];	/* select tables per band */
	uint16 keepalive_count;	/* nmbr of keepalives per bss_max_idle period */
	uint16 mkeepalive_index; /* mkeepalive_index for keepalive frame to be used */
	uint16 ka_max_interval; /* Keepalive_max_interval in seconds */
	uint32 cap;
} wlc_wnm_copy_t;

#define WNM_COPY_SIZE sizeof(wlc_wnm_copy_t)

static int
wlc_wnm_bss_get(void *ctx, wlc_bsscfg_t *cfg, uint8 *data, int *len)
{
#ifdef STA
	/* retrieve the PM mode between bsscfg's */
	if (BSSCFG_INFRA_STA(cfg)) {
		wlc_wnm_info_t *wnm = ctx;
		wnm_bsscfg_cubby_t *wnm_cfg = WNM_BSSCFG_CUBBY(wnm, cfg);
		wlc_wnm_copy_t *cp = (wlc_wnm_copy_t *)data;
		int i;

		cp->load_avg_cu_ap = wnm_cfg->load_avg_cu_ap;
		cp->load_next_window = wnm_cfg->load_next_window;
		cp->load_chan_util = wnm_cfg->load_chan_util;
		cp->load_cu_self = wnm_cfg->load_cu_self;
		cp->load_avg_cu_self = wnm_cfg->load_avg_cu_self;
		cp->load_bcn_count = wnm_cfg->load_bcn_count;
		cp->load_bcn_per_window = wnm_cfg->load_bcn_per_window;
		cp->bsstrans_stainfo = wnm_cfg->bsstrans_stainfo;
		/* copy keepalive configurations from from_cfg */
		cp->keepalive_count = wnm_cfg->keepalive_count;
		cp->mkeepalive_index = wnm_cfg->mkeepalive_index;
		cp->ka_max_interval = wnm_cfg->ka_max_interval;
		/* copy current capabilities */
		cp->cap = wnm_cfg->cap;
		/* Force NULL to re-use of bsi and tables */
		wnm_cfg->bsstrans_stainfo = NULL;

		if (WNM_BSSCFG_STA(cfg)) {
			for (i = 0; i < MAXBANDS; i++) {
				cp->bss_select_table[i] = wnm_cfg->bss_select_table[i];
			}
		}
	} else {
		*len = 0; /* Set not called */
	}
#endif /* STA */
	return BCME_OK;
}
static int
wlc_wnm_bss_set(void *ctx, wlc_bsscfg_t *cfg, const uint8 *data, int len)
{
#ifdef STA
	/* copy the PM mode between bsscfg's */
	if (BSSCFG_INFRA_STA(cfg)) {
		wlc_wnm_info_t *wnm = ctx;
		wnm_bsscfg_cubby_t *wnm_cfg = WNM_BSSCFG_CUBBY(wnm, cfg);
		const wlc_wnm_copy_t *cp = (const wlc_wnm_copy_t *)data;
		int i;

		wnm_cfg->load_avg_cu_ap = cp->load_avg_cu_ap;
		wnm_cfg->load_next_window = cp->load_next_window;
		wnm_cfg->load_chan_util = cp->load_chan_util;
		wnm_cfg->load_cu_self = cp->load_cu_self;
		wnm_cfg->load_avg_cu_self = cp->load_avg_cu_self;
		wnm_cfg->load_bcn_count = cp->load_bcn_count;
		wnm_cfg->load_bcn_per_window = cp->load_bcn_per_window;
		wnm_cfg->bsstrans_stainfo = cp->bsstrans_stainfo;
		/*
		 * timer callback ctx has wnm and cfgidx
		 * cfgidx is fixed even during bsscfg move
		 * but wnm ptr should be from current wlc
		 */
		if (wnm_cfg->bsstrans_stainfo->tmr_cb) {
			wnm_cfg->bsstrans_stainfo->tmr_cb->wnm =
				cfg->wlc->wnm_info;
		 }
		/* copy keepalive configurations to to_cfg */
		wnm_cfg->keepalive_count = cp->keepalive_count;
		wnm_cfg->mkeepalive_index = cp->mkeepalive_index;
		wnm_cfg->ka_max_interval = cp->ka_max_interval;

		/* set capabilities */
		wnm_cfg->cap = cp->cap;

		if (WNM_BSSCFG_STA(cfg)) {
			for (i = 0; i < MAXBANDS; i++) {
				wnm_cfg->bss_select_table[i] = cp->bss_select_table[i];
			}
		}
	}
#endif /* STA */
	return BCME_OK;
}
#endif /* WLRSDB */

wlc_wnm_info_t *
BCMATTACHFN(wlc_wnm_attach)(wlc_info_t *wlc)
{
	wlc_wnm_info_t *wnm = NULL;
	bsscfg_cubby_params_t cubby_params;
	bool mreg = FALSE;
#ifdef WLWNM_AP
	uint16 arqfstbmp = FT2BMP(FC_ASSOC_REQ) | FT2BMP(FC_REASSOC_REQ);
#endif /* WLWNM_AP */
	uint16 arsfstbmp = FT2BMP(FC_ASSOC_RESP) | FT2BMP(FC_REASSOC_RESP);
	uint16 parse_fstbmp =
		FT2BMP(FC_ASSOC_REQ) |
		FT2BMP(FC_ASSOC_RESP) |
		FT2BMP(FC_REASSOC_REQ) |
		FT2BMP(FC_REASSOC_RESP) |
		FT2BMP(FC_BEACON) |
		0;
#ifdef STA
	uint16 bssload_parse_fstbmp =
#ifdef WL_OCE_TB
		FT2BMP(FC_PROBE_RESP) |
#endif /* WL_OCE_TB */
		FT2BMP(FC_BEACON);
#endif /* STA */

	if ((wnm = (wlc_wnm_info_t *)MALLOCZ(wlc->osh, sizeof(wlc_wnm_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}

	wnm->wlc = wlc;

	wnm->cmn = (wlc_wnm_cmn_info_t*) obj_registry_get(wlc->objr, OBJR_WNM_CMN_INFO);

	if (wnm->cmn == NULL) {
		if ((wnm->cmn = (wlc_wnm_cmn_info_t *) MALLOCZ(wlc->pub->osh,
			sizeof(*wnm->cmn))) == NULL) {
			WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			goto fail;
		}
		obj_registry_set(wlc->objr, OBJR_WNM_CMN_INFO, wnm->cmn);
	}
	/* Reference the stored value in both instances */
	(void)obj_registry_ref(wlc->objr, OBJR_WNM_CMN_INFO);

	/* register module */
	if (wlc_module_register(wlc->pub, wnm_iovars, "wnm", wnm, wlc_wnm_doiovar,
	    wlc_wnm_watchdog, wlc_wnm_wlc_up, wlc_wnm_wlc_down)) {
		WL_ERROR(("wl%d: %s wlc_module_register() failed\n", wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	mreg = TRUE;

	/* reserve cubby space in the bsscfg container for per-bsscfg private data */
	bzero(&cubby_params, sizeof(cubby_params));

	cubby_params.context = wnm;
	cubby_params.fn_init = wlc_wnm_bsscfg_init;
	cubby_params.fn_deinit = wlc_wnm_bsscfg_deinit;
#if defined(BCMDBG)
	cubby_params.fn_dump = wlc_wnm_bsscfg_dump;
#endif // endif
#ifdef WLRSDB
	cubby_params.fn_get = wlc_wnm_bss_get;
	cubby_params.fn_set = wlc_wnm_bss_set;
	cubby_params.config_size = WNM_COPY_SIZE;
#endif /* WLRSDB */

	wnm->cfgh = wlc_bsscfg_cubby_reserve_ext(wlc, sizeof(wnm_bsscfg_cubby_t),
	                                         &cubby_params);
	if (wnm->cfgh < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve_ext() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

#ifdef WLWNM_AP
	/* init internal BSS Max Idle Period timer */
	if ((wnm->bss_idle_timer = wl_init_timer(wlc->wl, wlc_wnm_bss_idle_timer, wnm,
		"wnm_bss_idle_timer")) == NULL) {
		WL_ERROR(("wl%d: %s: bss_idle_timer init failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* default support to all tclas type,i.e., type 0-5 */
	wnm->tfs_tclastype = TFS_DEFAULT_TCLASTYPE;
	/* ICMP6 NA should reply to all node multicast address */
	wnm->parp_allnode = TRUE;
#endif /* WLWNM_AP */

	/* reserve cubby in the scb container for per-scb private data */
	wnm->scb_handle = wlc_scb_cubby_reserve(wlc, sizeof(wnm_scb_cubby_t),
		wlc_wnm_scb_init, wlc_wnm_scb_deinit, wlc_wnm_scb_dump, (void *)wnm);

	if (wnm->scb_handle < 0) {
		WL_ERROR(("wl%d: %s: wlc_scb_cubby_reserve() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

#ifdef WLWNM_AP
	/* register IE mgmt calc/build callbacks */
	/* calc/build */
	if (wlc_iem_add_build_fn_mft(wlc->iemi, arsfstbmp, DOT11_MNG_BSS_MAX_IDLE_PERIOD_ID,
	     wlc_wnm_ars_calc_maxidle_ie_len, wlc_wnm_ars_write_maxidle_ie, wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_build_fn failed, maxidle ie\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	if (wlc_iem_add_build_fn_mft(wlc->iemi, arsfstbmp, DOT11_MNG_TIMBC_RESP_ID,
	     wlc_wnm_ars_calc_timbc_ie_len, wlc_wnm_ars_write_timcb_ie, wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_build_fn failed, timbc ie\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	/* pasre */
	if (wlc_iem_add_parse_fn_mft(wlc->iemi, arqfstbmp, DOT11_MNG_TIMBC_REQ_ID,
	                             wlc_wnm_arq_parse_timbc_ie, wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_parse_fn failed, timbc ie in assocreq\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#endif /* WLWNM_AP */
	/* parse */
	if (wlc_iem_add_parse_fn_mft(wlc->iemi, parse_fstbmp, DOT11_MNG_EXT_CAP_ID,
		wlc_wnm_bss_parse_ext_cap_ie, wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_add_parse_fn failed, ext cap ie in assocreq\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#ifdef WLWNM_BRCM
	if (WLWNM_BRCM_ENAB(wlc->pub)) {
		if (wlc_iem_vs_add_parse_fn_mft(wlc->iemi, parse_fstbmp, WLC_IEM_VS_IE_PRIO_BRCM,
			wlc_wnm_bss_parse_brcm_ie, wlc) != BCME_OK) {
			WL_ERROR(("wl%d: %s wlc_iem_vs_add_parse_fn failed, "
				"brcm ie in wnm assocreq\n", wlc->pub->unit, __FUNCTION__));
			goto fail;
		}
	}
#endif /* WLWNM_BRCM */

#ifdef STA
#ifdef KEEP_ALIVE
	if (KEEP_ALIVE_ENAB(wlc->pub)) {
		/* parse */
		if (wlc_iem_add_parse_fn_mft(wlc->iemi, arsfstbmp, DOT11_MNG_BSS_MAX_IDLE_PERIOD_ID,
			wlc_wnm_bss_max_idle_ie_process, wlc) != BCME_OK) {
			WL_ERROR(("wl%d: %s: wlc_iem_add_parse_fn failed\n",
				wlc->pub->unit, __FUNCTION__));
			goto fail;
		}
	}
#endif /* KEEP_ALIVE */

	if (wlc_iem_add_parse_fn_mft(wlc->iemi, bssload_parse_fstbmp, DOT11_MNG_QBSS_LOAD_ID,
		wlc_wnm_bssload_process_bl_ie, wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s: add parse_fn failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

#if defined(WNM_BSSTRANS_EXT)
#ifndef WNM_BSSTRANS_EXT_DISABLED
	wlc->pub->_bsstrans_ext = TRUE;

	/* disbaled by default. enabled if "product" policy is set. */
	wlc->pub->_bsstrans_ext_active = FALSE;
#endif /* !WNM_BSSTRANS_EXT_DISABLED */

#if defined(BCMDBG) || defined(BCMDBG_DUMP) || defined(WLTEST) || defined(DNG_DBGDUMP)
	wlc_dump_register(wlc->pub, "wbtext", (dump_fn_t)wlc_wbtext_dump, (void*)wlc);
#endif /* BCMDBG || BCMDBG_DUMP || WLTEST || DNG_DBGDUMP */
#endif /* WNM_BSSTRANS_EXT */
#endif /* STA */

#if defined(WLWNM) && !defined(WLWNM_DISABLED)
	wlc->pub->cmn->_wnm = TRUE;
#endif // endif

#if defined(WLWNM_BRCM) && !defined(WLWNM_BRCM_DISABLED)
	wlc->pub->_wnm_brcm = TRUE;
#endif /* WLWNM_BRCM && !WLWNM_BRCM_DISABLED */

	return wnm;

	/* error handling */
fail:

	if (wnm != NULL) {
		if (mreg) {
			MODULE_DETACH(wnm, wlc_wnm_detach);
		}
		else
			MFREE(wlc->osh, wnm, sizeof(wlc_wnm_info_t));
	}
	return NULL;
}

void
BCMATTACHFN(wlc_wnm_detach)(wlc_wnm_info_t *wnm)
{
	wlc_info_t *wlc;
	wlc_wnm_cmn_info_t *wnm_cmn;
#ifdef STA
	wlc_info_t *wlc_tmr;
	wlc_bsscfg_t *bsscfg;
	wnm_bsscfg_cubby_t* wnm_cfg;
	wnm_bsstrans_sta_info_t *bsi;
	int idx;
#endif /* STA */
	if (!wnm)
		return;
	wlc = wnm->wlc;

#ifdef WLWNM_AP
	if (wnm->bss_idle_timer)
		wl_free_timer(wlc->wl, wnm->bss_idle_timer);

	if (wnm->url && wnm->url_len != 0)
		MFREE(wlc->osh, wnm->url, wnm->url_len);
#endif /* WLWNM_AP */

	wnm_cmn = wnm->cmn;
	if (obj_registry_unref(wlc->objr, OBJR_WNM_CMN_INFO) == 0) {
		if (wnm_cmn != NULL) {
			if (wnm_cmn->tclas_head != NULL) {
				/* Free orphan tclas */
				wnm_tclas_free(wnm->wlc, wnm_cmn->tclas_head);
			}
			MFREE(wlc->osh, wnm_cmn, sizeof(*wnm_cmn));
		}
		obj_registry_set(wlc->objr, OBJR_WNM_CMN_INFO, NULL);
	}
#ifdef STA
	/* if RSDB is active, the wlc_info context for the bsstrans_stainfo is the primary
	 * wlc_info. Keep the resolved wlc for timer frees in a local.
	 */
	wlc_tmr = wlc;
#ifdef WLRSDB
	if (RSDB_ENAB(wlc->pub)) {
		wlc_tmr = WLC_RSDB_GET_PRIMARY_WLC(wlc);
	}
#endif /* WLRSDB */
	FOREACH_STA(wlc, idx, bsscfg) {
		wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);
		bsi = wnm_cfg->bsstrans_stainfo;

		if (!bsi) {
			continue;
		}
		if (WBTEXT_ENAB(wlc->pub) &&
			WNM_BSSCFG_STA(bsscfg)) {
			if (bsi->tmr_cb) {
				MFREE(wlc->osh, bsi->tmr_cb,	sizeof(timer_cb_ctx_t));
			}
			if (bsi->resp_timer) {
				wl_free_timer(wlc_tmr->wl, bsi->resp_timer);
			}
			if (bsi->nbrlist_valid_timer) {
				wl_free_timer(wlc_tmr->wl, bsi->nbrlist_valid_timer);
				bsi->nbrlist_valid_timer = NULL;
			}
			if (bsi->disassoc_imnt_timer) {
				wl_free_timer(wlc_tmr->wl, bsi->disassoc_imnt_timer);
				bsi->disassoc_imnttmr_active = NULL;
			}
			if (wnm_cfg->bss_select_table[0]) {
				if (wnm_cfg->bss_select_table[0]->rssi_table) {
					MFREE(wlc->osh,
					wnm_cfg->bss_select_table[0]->rssi_table,
					(sizeof(wnm_bss_select_factor_t) *
					BSS_MAXTABLE_SIZE *
					BSS_TABLE_NUMBER * MAXBANDS));
				}
				MFREE(wlc->osh, wnm_cfg->bss_select_table[0],
					(sizeof(wnm_bss_select_table_t) * MAXBANDS));
				wnm_cfg->bss_select_table[0] = NULL;
			}
		}
		MFREE(wlc->osh, bsi, sizeof(wnm_bsstrans_sta_info_mem_t));
	}
#endif /* STA */

	wlc_module_unregister(wlc->pub, "wnm", wnm);
	MFREE(wlc->osh, wnm, sizeof(wlc_wnm_info_t));
}

static int
wlc_wnm_wlc_up(void *ctx)
{
#ifdef WLWNM_AP
	wlc_wnm_info_t *wnm = ctx;

	/* 100 ms periodically wakeup to check stuff */
	wl_add_timer(wnm->wlc->wl, wnm->bss_idle_timer, 100, TRUE);
#endif /* WLWNM_AP */
	return BCME_OK;
}

static int
wlc_wnm_wlc_down(void *ctx)
{
	int callbacks = 0;
#ifdef WLWNM_AP
	wlc_wnm_info_t *wnm = ctx;
	wlc_info_t *wlc = wnm->wlc;
	wlc_bsscfg_t *bsscfg;
	wnm_bsscfg_cubby_t *wnm_cfg;
	int i;

	if (!wl_del_timer(wlc->wl, wnm->bss_idle_timer)) {
		WL_ERROR(("del bss idle timer failed\n"));
		callbacks++;
	}

	/* clear TIMBC template frame, high rate frame, and basic rate frame */
	FOREACH_AP(wlc, i, bsscfg) {
		wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);
		wlc_wnm_timbc_clrframe(wlc, wnm_cfg);
	}
#endif /* WLWNM_AP */

	return callbacks;
}

#ifdef STA
static int
wlc_wnm_bsstrans_query_send(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg, wlc_bss_list_t *list)
{
	wlc_info_t *wlc = wnm->wlc;
	wnm_bsscfg_cubby_t *wnm_cfg;
	int i, plen;
	void *p;
	dot11_bsstrans_query_t *query;
	int bss_cnt = list? list->count : 0;
	wlc_bss_info_t *bi;
	uint8 *bufptr;
	uint32 max_score;
	uint32 temp_rssi;
	struct scb *scb;

	wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);
	/* Allocate BSSTRANS Query frame */
	plen = DOT11_BSSTRANS_QUERY_LEN + bss_cnt * (TLV_HDR_LEN + DOT11_NEIGHBOR_REP_IE_FIXED_LEN);

#ifdef WL_MBO_WFA_CERT
	if (bss_cnt == 0) {
		plen = DOT11_BSSTRANS_QUERY_LEN + wnm_cfg->btq_nbrlist_size *
			(TLV_HDR_LEN + DOT11_NEIGHBOR_REP_IE_FIXED_LEN +
			TLV_HDR_LEN + DOT11_NGBR_BSSTRANS_PREF_SE_LEN);
	}
#endif /* WL_MBO_WFA_CERT */

#ifndef WL_MBO_WFA_CERT
	/* Allocate memory to add BSS Transfer SE only if bsstrans_policy
	 * is set to WNM_PRODUCT_POLICY
	 */
	if (wlc_wnm_bsstrans_is_product_policy(wnm)) {
		plen += TLV_HDR_LEN + DOT11_NGBR_BSSTRANS_PREF_SE_LEN;
	}
#endif /* WL_MBO_WFA_CERT */

	p = wlc_frame_get_action(wlc, &bsscfg->BSSID, &bsscfg->cur_etheraddr,
		&bsscfg->BSSID, plen, (uint8 **) &query, DOT11_ACTION_CAT_WNM);
	if (p == NULL) {
		WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__, DOT11_ACTION_CAT_WNM,
			MALLOCED(wlc->osh)));
		return BCME_NOMEM;
	}

	query->category = DOT11_ACTION_CAT_WNM;
	query->action = DOT11_WNM_ACTION_BSSTRANS_QUERY;
	query->token = WLC_WNM_UPDATE_TOKEN(wnm->req_token);
	query->reason = wnm_cfg->btq_reason;

	if (bss_cnt) {
		/* Fill candidate list from BSS scan list */
		bufptr = &query->data[0];
		bi = list->ptrs[bss_cnt - 1];
		max_score = wlc_bss_pref_score(bsscfg, bi, TRUE, &temp_rssi);

		for (i = 0; i < bss_cnt; i++) {
			bi = list->ptrs[i];
			if (memcmp(&bi->BSSID, &bsscfg->BSSID, ETHER_ADDR_LEN)) {
				wlc_wnm_neighbor_report_ie(wlc, bsscfg, bi, &bufptr, max_score);
			}
		}
	}
#ifdef WL_MBO_WFA_CERT
	else {
		btq_nbr_list_t* btq_nbr_elem = wnm_cfg->btq_nbr_list_head;
		bufptr = &query->data[0];
		while (btq_nbr_elem) {
			wlc_wnm_btq_nbr_ie(wlc, &btq_nbr_elem->nbr_elt, &bufptr);
			btq_nbr_elem = btq_nbr_elem->next;
		}
	}
#endif /* WL_MBO_WFA_CERT */
	scb = wlc_scbfind(wlc, bsscfg, &bsscfg->BSSID);
	wlc_sendmgmt(wlc, p, bsscfg->wlcif->qi, scb);
	return BCME_OK;
}

static void
wlc_wnm_bsstrans_query_scancb(void *arg, int status, wlc_bsscfg_t *bsscfg)
{
	wlc_info_t *wlc = (wlc_info_t*)arg;
	wlc_wnm_info_t *wnm = (wlc_wnm_info_t *)wlc->wnm_info;
	uint i;

	if (status != WLC_E_STATUS_SUCCESS) {
		WL_ERROR(("BSSTRANS scan failure %d\n", status));
		return;
	}

	WL_WNM(("+ BssTransQuery: scan: %d\n", wlc->scan_results->count));

	/* remove the BSS to which we are associated and replace it by the last one */
	for (i = 0; i < wlc->scan_results->count; i++) {
		wlc_bss_info_t *bi = wlc->scan_results->ptrs[i];
		ASSERT(bi);
		if (memcmp(&bsscfg->BSSID, &bi->BSSID, ETHER_ADDR_LEN) == 0) {
			if (bi->bcn_prb)
				MFREE(wlc->osh, bi->bcn_prb, bi->bcn_prb_len);
			MFREE(wlc->osh, bi, sizeof(wlc_bss_info_t));
			if (--wlc->scan_results->count)
				wlc->scan_results->ptrs[i] =
					wlc->scan_results->ptrs[wlc->scan_results->count];
			wlc->scan_results->ptrs[wlc->scan_results->count] = NULL;
			break;
		}
	}
	wlc_wnm_bsstrans_query_send(wnm, bsscfg, wlc->scan_results);
}

static int
bsstrans_req_skip_unprocessed_fields(uint8 reqmode, uint8 **body, int *body_len)
{
	if (reqmode & DOT11_BSSTRANS_REQMODE_BSS_TERM_INCL) {
		/* Field skipped as non relevant  */
		*body_len -= TLV_HDR_LEN + DOT11_NGBR_BSS_TERM_DUR_SE_LEN;
		*body += TLV_HDR_LEN + DOT11_NGBR_BSS_TERM_DUR_SE_LEN;
	}

	if (reqmode & DOT11_BSSTRANS_REQMODE_ESS_DISASSOC_IMNT) {
		if (*body_len < 0)
			return BCME_ERROR;
		/* First byte is URL length. Field skipped for security concerns */
		*body_len -= 1 + **body;
		*body += 1 + **body;
	}
	return BCME_OK;
}

/**
 * @param[in] body   Contains an .11 neighbor report
 *
 */
static int
bsstrans_req_get_pref_ap(wlc_bsscfg_t *cfg, uint8 *body, int *body_len,
	wl_reassoc_params_t *reassoc, uint8 *pref_max)
{
	dot11_neighbor_rep_ie_t *ngb = (dot11_neighbor_rep_ie_t *) body;

	while (*body_len > 0) {
		dot11_ngbr_bsstrans_pref_se_t *pref;
		bool pref_chspec_found = FALSE;

		if (ngb == NULL || ngb->id != DOT11_MNG_NEIGHBOR_REP_ID ||
			!bcm_valid_tlv(ngb, *body_len) ||
			ngb->len < DOT11_NEIGHBOR_REP_IE_FIXED_LEN) {
			if (ngb) {
				WL_ERROR(("%s: Failed to get preffered AP! id:%d len: %d\n",
					__FUNCTION__, ngb->id, ngb->len));
			} else {
				WL_ERROR(("%s: Failed to get preffered AP! ngb: is Null\n",
					__FUNCTION__));
			}

			return BCME_ERROR;
		}

		pref = (dot11_ngbr_bsstrans_pref_se_t*) bcm_parse_tlvs(ngb->data,
			ngb->len - DOT11_NEIGHBOR_REP_IE_FIXED_LEN,
			DOT11_NGBR_BSSTRANS_PREF_SE_ID);
		if (pref) {
			if (pref->preference > *pref_max) {
				memcpy(&reassoc->bssid, &ngb->bssid, ETHER_ADDR_LEN);
				*pref_max = pref->preference;
				pref_chspec_found = TRUE;
			}
		} else if (*pref_max == 0) {
			memcpy(&reassoc->bssid, &ngb->bssid, ETHER_ADDR_LEN);
			*pref_max = 1;
			pref_chspec_found = TRUE;
		}

		if (pref_chspec_found && ngb->channel != 0) {
			reassoc->chanspec_num = 1;
			/* Set 20Mhz chan for calling wlc_reassoc(), to avoid
			 * STA with 20Mhz bw_cap only from scanning whole band
			 */
			/* TODO:6GHZ: how will ieee neighbor report indicate 6g channels */
			reassoc->chanspec_list[0] = CH20MHZ_CHSPEC(ngb->channel);
			WL_WNM(("pref chanspec:0x%x rclass:0x%x chan:0x%x\n",
				reassoc->chanspec_list[0], ngb->reg, ngb->channel));
		}

		/* Continue with next candidate */
		ngb = (dot11_neighbor_rep_ie_t *) bcm_next_tlv((bcm_tlv_t *)ngb, body_len);
	}
	return BCME_OK;
} /* bsstrans_req_get_pref_ap */

#define PARTIAL_SCAN_MAX_CHANNELS 20

/*
 * function to identify if the candidate AP is recommended AP
 * return TRUE for valid AP
 */
static bool
wlc_wnm_bsstrans_validnbr_ap(wlc_wnm_info_t *wnm, wlc_bsscfg_t *cfg,
	wlc_bss_info_t *bip)
{
	wnm_nbr_rep_t *nbr_rep;
	wnm_bsstrans_sta_info_t *bsi;
	wnm_bsscfg_cubby_t *wnm_cfg;
	bool ret = FALSE;

	wnm_cfg = WNM_BSSCFG_CUBBY(wnm, cfg);
	bsi = wnm_cfg->bsstrans_stainfo;
	if (bsi->nbrlist_valtimer_active &&
		(bsi->req_mode & DOT11_BSSTRANS_REQMODE_PREF_LIST_INCL) &&
		(bsi->req_mode & DOT11_BSSTRANS_REQMODE_ABRIDGED)) {
		nbr_rep = wlc_wnm_get_neighbor_rep(wnm, cfg, &bip->BSSID,
				wf_chspec_ctlchan(bip->chanspec));
		/* return FALSE if bssid is not in nbrlist */
		ret = (nbr_rep == NULL) ? FALSE : TRUE;
	}

	return ret;
}

/* check wnm neighbor list for with bssid and channel */
wnm_nbr_rep_t *
wlc_wnm_get_neighbor_rep(wlc_wnm_info_t *wnm, wlc_bsscfg_t *cfg,
	struct ether_addr *ea, uint8 channel)
{
	wnm_nbr_rep_t *nbr_rep;
	wnm_bsstrans_sta_info_t *bsi;
	wnm_bsscfg_cubby_t *wnm_cfg;

	wnm_cfg = WNM_BSSCFG_CUBBY(wnm, cfg);
	bsi = wnm_cfg->bsstrans_stainfo;
	/* Find Neighbor Report element from list */
	nbr_rep = bsi->nbr_rep_head;
	while (nbr_rep) {
		if ((memcmp(&nbr_rep->nbr_elt.bssid, ea, ETHER_ADDR_LEN) == 0) &&
			(nbr_rep->nbr_elt.channel == channel))
			break;
		nbr_rep = nbr_rep->next;
	}

	return nbr_rep;
}

/* get chanspec list from wnm neighbor list */
static int
wlc_wnm_neighbor_get_chanlist(wlc_wnm_info_t *wnm, wlc_bsscfg_t *cfg,
		chanspec_t *list, uint32 maxsize)
{
	wnm_nbr_rep_t *nbr_rep;
	wnm_bsstrans_sta_info_t *bsi;
	uint32 i, idx = 0;
	bool updated = FALSE;
	wnm_bsscfg_cubby_t *wnm_cfg;

	wnm_cfg = WNM_BSSCFG_CUBBY(wnm, cfg);
	bsi = wnm_cfg->bsstrans_stainfo;

	ASSERT(bsi != NULL);
	ASSERT(list != NULL);

	/* Find Neighbor Report element from list */
	nbr_rep = bsi->nbr_rep_head;
	while (nbr_rep) {
		updated = FALSE;
		if (nbr_rep->nbr_elt.channel != 0 && idx < maxsize) {
			/* TODO:6GHZ: how will ieee neighbor report indicate 6g channels */
			for (i = 0; i < idx; i++) {
				if (list[i] == CH20MHZ_CHSPEC(nbr_rep->nbr_elt.channel)) {
					updated = TRUE;
					break;
				}
			}

			if (!updated) {
				/* Set 20Mhz chan for calling wlc_reassoc(), to avoid
				 * STA with 20Mhz bw_cap only from scanning whole band
				 */
				list[idx++] = CH20MHZ_CHSPEC(nbr_rep->nbr_elt.channel);
			}
		}

		nbr_rep = nbr_rep->next;
	}

	return BCME_OK;
}

/* prepare wnm neighbor list from BSS Trasition request frame */
static int
wlc_wnm_bsstrans_update_nbrlist(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg,
	uint8 *body, int *body_len)
{
	dot11_neighbor_rep_ie_t *ngb = (dot11_neighbor_rep_ie_t *) body;

	while (*body_len > 0) {
		if (ngb == NULL || ngb->id != DOT11_MNG_NEIGHBOR_REP_ID ||
			!bcm_valid_tlv(ngb, *body_len) ||
			ngb->len < DOT11_NEIGHBOR_REP_IE_FIXED_LEN) {
			if (ngb) {
				WL_ERROR(("%s: Failed to update bnr list!!! id:%d len: %d\n",
					__FUNCTION__, ngb->id, ngb->len));
			} else {
				WL_ERROR(("%s: Failed to update nbr list\n", __FUNCTION__));
			}
			return BCME_ERROR;
		}

		wlc_wnm_add_neighbor(wnm, bsscfg, (nbr_element_t *)ngb);

		/* Continue with next candidate */
		ngb = (dot11_neighbor_rep_ie_t *) bcm_next_tlv((bcm_tlv_t *)ngb, body_len);
	}

	return BCME_OK;
}

#ifdef WBTEXT_BTMDBG
static void
wlc_wnm_print_nbrlist(wlc_wnm_info_t *wnm, wlc_bsscfg_t *cfg)
{
	wnm_nbr_rep_t *nbr_rep;
	wnm_bsstrans_sta_info_t *bsi;
	wnm_bsscfg_cubby_t *wnm_cfg;
	wnm_cfg = WNM_BSSCFG_CUBBY(wnm, cfg);
	bsi = wnm_cfg->bsstrans_stainfo;

	WL_ERROR(("Neighbor list: \n"));
	nbr_rep = bsi->nbr_rep_head;
	while (nbr_rep) {
		WL_ERROR(("  channel: %d, bssid: %02x:%02x:%02x:%02x:%02x:%02x --> \n",
			nbr_rep->nbr_elt.channel,
			nbr_rep->nbr_elt.bssid.octet[0], nbr_rep->nbr_elt.bssid.octet[1],
			nbr_rep->nbr_elt.bssid.octet[2], nbr_rep->nbr_elt.bssid.octet[3],
			nbr_rep->nbr_elt.bssid.octet[4], nbr_rep->nbr_elt.bssid.octet[5]));
		nbr_rep = nbr_rep->next;
	}
}
#endif /* WBTEXT_BTMDBG */

/* add an element for wnm neighbor list */
static void
wlc_wnm_add_neighbor(wlc_wnm_info_t *wnm, wlc_bsscfg_t *cfg,
	nbr_element_t *nbr_elt)
{
	wnm_nbr_rep_t *nbr_rep;
	wlc_info_t *wlc;
	wnm_bsstrans_sta_info_t *bsi;
	wnm_bsscfg_cubby_t *wnm_cfg;
	wlc = wnm->wlc;

	wnm_cfg = WNM_BSSCFG_CUBBY(wnm, cfg);
	bsi = wnm_cfg->bsstrans_stainfo;

	/* Find Neighbor Report element from list */
	nbr_rep = wlc_wnm_get_neighbor_rep(wnm, cfg, &nbr_elt->bssid, nbr_elt->channel);

	if (nbr_rep) {
		/* over write existing nbr */
		memcpy(&nbr_rep->nbr_elt, nbr_elt, TLV_HDR_LEN + DOT11_NEIGHBOR_REP_IE_FIXED_LEN);
	} else {
		if (bsi->nbrlist_size >= bsi->max_nbr_count) {
			WL_ERROR(("%s: over max neighbor count (%d)\n",
				__FUNCTION__, bsi->max_nbr_count));
			return;
		}
		nbr_rep = (wnm_nbr_rep_t *)MALLOC(wlc->osh, sizeof(wnm_nbr_rep_t));
		if (nbr_rep == NULL) {
			WL_ERROR(("wl%d: %s: MALLOC(%d) failed, malloced %d bytes\n",
				WLCWLUNIT(wlc), __FUNCTION__, (int)sizeof(wnm_nbr_rep_t),
				MALLOCED(wlc->osh)));
			return;
		}
		memcpy(&nbr_rep->nbr_elt, nbr_elt, TLV_HDR_LEN + DOT11_NEIGHBOR_REP_IE_FIXED_LEN);
		nbr_rep->next = bsi->nbr_rep_head;
		bsi->nbr_rep_head = nbr_rep;
		bsi->nbrlist_size++;
	}
}

/* delete all elements from wnm neighbor list */
static void
wlc_wnm_del_all_neighbor(wlc_wnm_info_t *wnm, wlc_bsscfg_t *cfg)
{
	wnm_bsstrans_sta_info_t *bsi;
	wnm_nbr_rep_t *nbr_rep, *nbr_rep_next;
	wnm_bsscfg_cubby_t *wnm_cfg;

	wnm_cfg = WNM_BSSCFG_CUBBY(wnm, cfg);
	bsi = wnm_cfg->bsstrans_stainfo;

	nbr_rep = bsi->nbr_rep_head;
	while (nbr_rep) {
		nbr_rep_next = nbr_rep->next;
		MFREE(wnm->wlc->osh, nbr_rep, sizeof(wnm_nbr_rep_t));
		nbr_rep = nbr_rep_next;
	}
	bsi->nbr_rep_head = NULL;
	bsi->nbrlist_size = 0;

	return;
}

#ifdef WL_MBO_WFA_CERT
/* Place BTQ NBR in BTM query Frame */
static void
wlc_wnm_btq_nbr_ie(wlc_info_t *wlc, nbr_rpt_elem_t* btq_nbr_elem,
		uint8 **bufptr)
{
	uint32 bssid_info = 0;
	dot11_neighbor_rep_ie_t *nbr_rep;
	dot11_ngbr_bsstrans_pref_se_t *pref;

	nbr_rep = (dot11_neighbor_rep_ie_t *)*bufptr;
	nbr_rep->id = DOT11_MNG_NEIGHBOR_REP_ID;
	nbr_rep->len = DOT11_NEIGHBOR_REP_IE_FIXED_LEN;
	bcopy(&btq_nbr_elem->bssid, &nbr_rep->bssid, ETHER_ADDR_LEN);
	bcopy(&bssid_info, &nbr_rep->bssid_info, sizeof(nbr_rep->bssid_info));
	nbr_rep->reg = btq_nbr_elem->reg;
	nbr_rep->channel = btq_nbr_elem->channel;
	nbr_rep->phytype = 0;
	*bufptr += TLV_HDR_LEN + DOT11_NEIGHBOR_REP_IE_FIXED_LEN;
	/* Add BSS Trans Pref */
	pref = (dot11_ngbr_bsstrans_pref_se_t *) *bufptr;
	pref->sub_id = DOT11_NGBR_BSSTRANS_PREF_SE_ID;
	pref->len = 1;
	pref->preference = btq_nbr_elem->bss_trans_preference;
	nbr_rep->len += (TLV_HDR_LEN + DOT11_NGBR_BSSTRANS_PREF_SE_LEN);
	*bufptr += (TLV_HDR_LEN + DOT11_NGBR_BSSTRANS_PREF_SE_LEN);
}
#endif /* WL_MBO_WFA_CERT */

/* Function to cleanup the neighor ap list */
static void
wlc_wnm_nbrlist_validity_timeout(void *context)
{
	timer_cb_ctx_t *ctx = (timer_cb_ctx_t *)context;
	wlc_wnm_info_t *wnm = ctx->wnm;
	wlc_bsscfg_t *bsscfg;
	wnm_bsscfg_cubby_t *wnm_cfg;
	wnm_bsstrans_sta_info_t *bsi;
	/*
	 * when timeout happens bsscfg could have moved
	 * to different core. timer context is still relevant
	 * as we update it in cubby get-set oprns
	 * So directly re-using the timer context
	 */
	bsscfg = wlc_bsscfg_find_by_ID(wnm->wlc, (uint16)ctx->join_cfgidx);
	if (bsscfg) {
		wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);
		bsi = wnm_cfg->bsstrans_stainfo;
		bsi->nbrlist_valtimer_active = FALSE;
		/* Clear neighbor list and exit */
		wlc_wnm_del_all_neighbor(wnm, bsscfg);
	}
}

/* update non neighbor best score incase validity timer is active */
void
wlc_wnm_update_nonnbr_bestscore(wlc_wnm_info_t *wnm,  wlc_bsscfg_t *cfg,
	wlc_bss_info_t *bip, uint32 score)
{
	wnm_bsstrans_sta_info_t *bsi;
	wnm_bsscfg_cubby_t *wnm_cfg;

	wnm_cfg = WNM_BSSCFG_CUBBY(wnm, cfg);
	bsi = wnm_cfg->bsstrans_stainfo;
	if (bsi->nbrlist_valtimer_active &&
		!wlc_wnm_get_neighbor_rep(wnm, cfg, &bip->BSSID,
		wf_chspec_ctlchan(bip->chanspec))) {
		if (score > bsi->nonnbr_bestscore) {
			bsi->nonnbr_bestscore = score;
		}
	}
}

/* Function to cleanup blacklist related info */
static void
wlc_wnm_disassocimnt_timeout(void *context)
{
	timer_cb_ctx_t *ctx = (timer_cb_ctx_t *)context;
	wlc_wnm_info_t *wnm = ctx->wnm;
	wlc_bsscfg_t *bsscfg;
	wnm_bsstrans_sta_info_t *bsi;
	wnm_bsscfg_cubby_t *wnm_cfg;

	/*
	 * when timeout happens bsscfg could have moved
	 * to different core. timer context is still relevant
	 * as we update it in cubby get-set oprns
	 * So directly re-using the timer context
	 */
	bsscfg = wlc_bsscfg_find_by_ID(wnm->wlc, (uint16)ctx->join_cfgidx);
	if (bsscfg) {
		wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);
		bsi = wnm_cfg->bsstrans_stainfo;
		/* remove blacklisting and exit */
		bsi->disassoc_imnttmr_active = FALSE;
		memset(&bsi->blklisted_bssid, 0, ETHER_ADDR_LEN);
		bsi->blklisted_bssid_bandunit = 0;
	}
}

/* Function to check if the current AP needs to be blacklisted
 * during disassoc imminent interval
 */
bool
wlc_wnm_is_blacklisted_bss(wlc_wnm_info_t *wnm, wlc_bsscfg_t *cfg, wlc_bss_info_t *bip)
{
	wnm_bsstrans_sta_info_t *bsi;
	wnm_bsscfg_cubby_t *wnm_cfg;
	bool retval = FALSE;

	wnm_cfg = WNM_BSSCFG_CUBBY(wnm, cfg);
	bsi = wnm_cfg->bsstrans_stainfo;
	/* Blacklist if bssid and band also matches */
	if ((bsi->disassoc_imnttmr_active) &&
		(!memcmp(&bsi->blklisted_bssid, &bip->BSSID, ETHER_ADDR_LEN)) &&
		(CHSPEC_BANDUNIT(bip->chanspec) == bsi->blklisted_bssid_bandunit)) {
		retval = TRUE;
	}
	return retval;
}

/** Take the bsstrans_req frame body as input and return list of channels in neighbor IEs */
static int
bsstrans_req_get_chanlist(wlc_bsscfg_t *cfg, uint8 *body, int *body_len, chanspec_t *list,
	uint32 *n_channels)
{
	dot11_neighbor_rep_ie_t *ngb = (dot11_neighbor_rep_ie_t *) body;

	while (*body_len > 0) {
		if (ngb == NULL || ngb->id != DOT11_MNG_NEIGHBOR_REP_ID ||
			!bcm_valid_tlv(ngb, *body_len) ||
			ngb->len < DOT11_NEIGHBOR_REP_IE_FIXED_LEN) {
			if (ngb) {
				WL_ERROR(("%s: Failed to get chanlist! id:%d len: %d\n",
					__FUNCTION__, ngb->id, ngb->len));
			} else {
				WL_ERROR(("%s: Failed to get chanlist\n", __FUNCTION__));
			}
			return BCME_ERROR;
		}

		if (ngb->channel != 0 && *n_channels < PARTIAL_SCAN_MAX_CHANNELS) {
			/* Set 20Mhz chan for calling wlc_reassoc(), to avoid
			 * STA with 20Mhz bw_cap only from scanning whole band
			 */
			/* TODO:6GHZ: how will ieee neighbor report indicate 6g channels */
			list[*n_channels] = CH20MHZ_CHSPEC(ngb->channel);
			*n_channels += 1;
		}

		/* Continue with next candidate */
		ngb = (dot11_neighbor_rep_ie_t *) bcm_next_tlv((bcm_tlv_t *)ngb, body_len);
	}
	return BCME_OK;
}

/* Callback handler invoked at tx completion of bsstrans_resp frame with accept status */
static void
wlc_bsstrans_resp_tx_complete(wlc_info_t *wlc, uint txstatus, void *arg)
{
	wlc_info_t *wlc_tmr = wlc;
	wnm_bsstrans_sta_info_t *bsi = (wnm_bsstrans_sta_info_t *)arg;

	ASSERT(bsi);
	wl_del_timer(wlc_tmr->wl, bsi->resp_timer);
	/* Add zero delay timer to trigger
	 * join from wlc_wnm_bsstrans_resp_timeout
	 */
	wl_add_timer(wlc_tmr->wl, bsi->resp_timer, 0, 0);
}

/* Allocate, fill and send a bsstrans_resp frame */
static int
bsstrans_send_resp(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint8 status, struct ether_addr *bssid,
	uint8 token, struct scb *scb)
{
	dot11_bsstrans_resp_t *resp;
	int plen = DOT11_BSSTRANS_RESP_LEN;
	void *p;
	uint8 *pbody;
	int bss_list_count = 0;
	int count = 0;
	wlc_bss_info_t *bi;
	int offset;
	wnm_bsscfg_cubby_t *wnm_cfg;
	wnm_bsstrans_sta_info_t *bsi;

	wnm_cfg = WNM_BSSCFG_CUBBY(wlc->wnm_info, bsscfg);
	bsi = wnm_cfg->bsstrans_stainfo;

	WL_WNM_BSSTRANS_LOG("bsstrans-resp status %d\n", status, 0, 0, 0);

	if (status == DOT11_BSSTRANS_RESP_STATUS_ACCEPT) {
		/* Transition response must include the optional BSSID field if the
		 * STA is going to do the transition and has the information in preferred
		 * candidate list.
		 */
		plen += ETHER_ADDR_LEN;
	}
	else if (status == DOT11_BSSTRANS_RESP_STATUS_REJ_BSS_LIST_PROVIDED) {
		/* reject with BSS list provided */
		for (offset = 0;
		     (bi = wlc_assoc_get_join_target(wlc, offset)) != NULL;
		     offset ++) {
			if (!WLC_IS_CURRENT_BSSID(bsscfg, &bi->BSSID)) {
				bss_list_count ++;
			}
		}
		if (bss_list_count == 0) {
			/* empty BSS list - set status to reject */
			status = DOT11_BSSTRANS_RESP_STATUS_REJECT;
		} else {
			int bss_entry_size = TLV_HDR_LEN + DOT11_NEIGHBOR_REP_IE_FIXED_LEN +
				TLV_HDR_LEN + DOT11_NGBR_BSSTRANS_PREF_SE_LEN;
			bss_list_count = MIN(MAX_BSS_LIST_SIZE / bss_entry_size, bss_list_count);
			/* extend packet length by BSS list */
			plen += bss_list_count * bss_entry_size;
		}
	}

	p = wlc_frame_get_action(wlc, &bsscfg->BSSID, &bsscfg->cur_etheraddr,
		&bsscfg->BSSID, plen, &pbody, DOT11_ACTION_CAT_WNM);
	if (p == NULL) {
		WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__, DOT11_ACTION_CAT_WNM,
			MALLOCED(wlc->osh)));
		return BCME_NOMEM;
	}

	resp = (dot11_bsstrans_resp_t*)pbody;
	resp->category = DOT11_ACTION_CAT_WNM;
	resp->action = DOT11_WNM_ACTION_BSSTRANS_RESP;
	resp->token = token;

	resp->status = status;
	resp->term_delay = 0;

	/* For non-product policy, avoid registering
	 * packet callback so as to avoid join attempt
	 * from packet callback instead it will happen
	 * from "wlc_assoc_scan_complete" function later
	 */
	if ((status == DOT11_BSSTRANS_RESP_STATUS_ACCEPT) &&
			wlc_wnm_bsstrans_is_product_policy(wlc->wnm_info)) {
		memcpy(resp->data, bssid, ETHER_ADDR_LEN);
		/* Register tx complete callback to initiate join process */
		bsi->join_cfg = bsscfg;
		wlc_pcb_fn_register(wlc->pcb, wlc_bsstrans_resp_tx_complete, (void *)bsi, p);
		WL_WNM_BSSTRANS_LOG("Roam to AP bssid[3-0]:%x\n", *(uint32 *)bssid, 0, 0, 0);
	}
	else if (status == DOT11_BSSTRANS_RESP_STATUS_REJ_BSS_LIST_PROVIDED) {
		uint32 max_score;
		uint32 temp_rssi;
		uint8 *bufptr = resp->data;

		/* use max score from first non-candidate */
		max_score = wlc_bss_pref_score(bsscfg, wlc_assoc_get_join_target(wlc, 0),
			TRUE, &temp_rssi);

		/* build list of BSS non-candidates */
		for (offset = 0;
		     (bi = wlc_assoc_get_join_target(wlc, offset)) != NULL;
		     offset ++) {
			/* skip current BSS */
			if (!WLC_IS_CURRENT_BSSID(bsscfg, &bi->BSSID)) {
				/* add neighbor report */
				wlc_wnm_neighbor_report_ie(wlc, bsscfg, bi, &bufptr, max_score);
				count ++;
			}
		}
		ASSERT(count == bss_list_count);
	}
	wlc_sendmgmt(wlc, p, bsscfg->wlcif->qi, scb);
	return BCME_OK;
}

#define BSSTRANS_RESP_TIMEOUT 500 /* ms */

/* Invoked after bsstrans_resp(with accept status) is not transmitted for BSSTRANS_RESP_TIMEOUT
 * Don't wait anymore for bsstrans_resp tx and initiate the pending join.
 */
static void
wlc_wnm_bsstrans_resp_timeout(void *context)
{
	timer_cb_ctx_t *ctx = (timer_cb_ctx_t *)context;
	wlc_wnm_info_t *wnm = ctx->wnm;
	wlc_bsscfg_t *bsscfg;
	wnm_bsscfg_cubby_t *wnm_cfg;
	wnm_bsstrans_sta_info_t *bsi;

	/*
	 * when timeout happens bsscfg could have moved
	 * to different core. timer context is still relevant
	 * as we update it in cubby get-set oprns
	 * So directly re-using the timer context
	 */
	bsscfg = wlc_bsscfg_find_by_ID(wnm->wlc, (uint16)ctx->join_cfgidx);
	if (bsscfg) {
		wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);
		bsi = wnm_cfg->bsstrans_stainfo;
		ASSERT(bsi->join_cfg);
		wlc_join_attempt_select(bsi->join_cfg);
		wlc_wnm_bsstrans_reset_pending_join(wnm->wlc, bsscfg);
	}
}

/* Invoked at the end of roam scan to check if bss-trans-resp
 * needs to be sent to currently associated AP.
 * Input status is status to be sent in bss-trans-resp frame.
 * If the status is ACCEPT, trgt_bssid is bssid of the target network
 * which will be joined. If status is REJECT, it is NULL.
 * Returns TRUE if bsstrans_resp was queued to be transmitted.
 */
bool
wlc_wnm_bsstrans_roamscan_complete(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint8 status,
struct ether_addr *trgt_bssid)
{
	wlc_wnm_info_t *wnm = wlc->wnm_info;
	bool bsstrans_tx = FALSE;
	struct scb_iter scbiter;
	struct scb *scb;
	wnm_bsstrans_sta_info_t *bsi;
	wnm_bsscfg_cubby_t *wnm_cfg;
	wlc_info_t *wlc_tmr = wlc;

#ifdef WLRSDB
	if (RSDB_ENAB(wlc->pub)) {
		wlc_tmr = WLC_RSDB_GET_PRIMARY_WLC(wlc);
	}
#endif /* WLRSDB */

	/* This function get called from
	 * 1. wlc_join_attempt_select -> ACCEPT
	 * 2. wlc_roam_complete       -> REJECT
	 *
	 * if #1 is called, #2 returns void              -> ACCEPT
	 * if #2 is called without #1, no valid ap found -> REJECT
	 */
	wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);
	bsi = wnm_cfg->bsstrans_stainfo;

	if (!bsi->resp_pending) {
		return bsstrans_tx;
	}

	/* get the scb for this bsscfg and proceed */
	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
		break;
	}

	ASSERT(scb);
	bsi->resp_pending = FALSE;

	if (WBTEXT_ENAB(wlc->pub)) {
		/* delete all neighbors */
		wlc_wnm_del_all_neighbor(wnm, bsscfg);
	}

	/* We might have lost association while we were doing roam scan */
	if (!bsscfg->associated) {
		return bsstrans_tx;
	}

	/* cleanup wnm maxidle values from previous assoc */
	wnm_cfg->bss_max_idle_period = 0;
	wnm_cfg->bss_idle_opt = 0;

	bsstrans_tx = !bsstrans_send_resp(wlc, bsscfg, status, trgt_bssid, bsi->resp_token, scb);
	bsi->resp_token = 0;
	bsi->req_mode = 0;
	if (bsstrans_tx && status == DOT11_BSSTRANS_RESP_STATUS_ACCEPT) {
		bsi->join_pending = TRUE;
		bsi->join_cfg = bsscfg;
		/* Schedule the timer to wait for the resp frame tx */
		wl_add_timer(wlc_tmr->wl, bsi->resp_timer, BSSTRANS_RESP_TIMEOUT, 0);
		if (bsi->tmr_cb) {
			bsi->tmr_cb->wnm = wlc->wnm_info;
			bsi->tmr_cb->join_cfgidx = WLC_BSSCFG_IDX(bsscfg);
		}
	}
	return bsstrans_tx;
}

/* This function is invoked for end-product (non-plugfest) functionality.
 * Following cases are handled:
 * 1. If a req with Preferred Candidate List Included bit is set,
 *    only the channels in neigbor ie of bsstrans_req are used
 *    for scanning to find the AP to roam to.
 * 2. If the allowed number of scans have been done in current throttle period
 *    just send a reject response to AP and not do any roam scan.
 */
static int
wlc_wnm_bsstrans_req_process_product(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg, struct scb *scb,
	uint8 *body, int body_len, bool unicast_req, dot11_bsstrans_req_t* req)
{
	int retval = BCME_OK;
	chanspec_t list[PARTIAL_SCAN_MAX_CHANNELS];
	uint32 n_channels = 0;
	wnm_bsstrans_sta_info_t *bsi;
	wnm_bsscfg_cubby_t *wnm_cfg;
	wlc_info_t *wlc_tmr;
	wnm_bsstrans_roam_throttle_t *throttle;

	wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);
	bsi = wnm_cfg->bsstrans_stainfo;
	throttle = bsi->throttle;
	wlc_tmr = wnm->wlc;

#ifdef WLRSDB
	if (RSDB_ENAB(wnm->wlc->pub)) {
		wlc_tmr = WLC_RSDB_GET_PRIMARY_WLC(wnm->wlc);
	}
#endif /* WLRSDB */

	/* Max roam scans done in throttle period, send reject if unicast request */
	if (throttle->period && (throttle->scans_done == throttle->scans_allowed)) {
		WL_WNM_BSSTRANS_LOG("BTM req throttled allowed %d done %d\n",
		throttle->scans_allowed, throttle->scans_done, 0, 0);
		goto reject;
	}

	if (WBTEXT_ENAB(wnm->wlc->pub)) {
	if (bsi->resp_pending == TRUE) {
		WL_ERROR(("%s: bss transition request pending\n", __FUNCTION__));
		goto reject;
	}

	/* If abridged bit is set but pref list is not given or
	 * request was given on GC interface, discard it as invalid
	 */
	if (((req->reqmode & DOT11_BSSTRANS_REQMODE_ABRIDGED) &&
		!(req->reqmode & DOT11_BSSTRANS_REQMODE_PREF_LIST_INCL)) ||
		P2P_CLIENT(wnm->wlc, bsscfg)) {
		WL_ERROR(("%s: discarding. invalid BTM request.\n", __FUNCTION__));
		goto reject;

	}

	if (bsscfg->roam->scan_block) {
		/* return if roamscan in progress */
		WL_ERROR(("%s: discarding. roamscan inprogress.\n", __FUNCTION__));
		goto reject;
	}

	/* delete all neighbors */
	wlc_wnm_del_all_neighbor(wnm, bsscfg);

	/* delete any previously stored non neighbor best score */
	bsi->nonnbr_bestscore = 0;
	bsi->req_mode = req->reqmode;

	/* 802.11-2012 - 8.5.14.9
	 * The Preferred Candidate List Included bit set to 1 indicates that
	 * the sender expects the receiving STA to process
	 * Candidate List Entries field
	 */

	if (req->reqmode & DOT11_BSSTRANS_REQMODE_PREF_LIST_INCL) {
		/* update neighbor list */
		retval = wlc_wnm_bsstrans_update_nbrlist(wnm, bsscfg, body, &body_len);
			if (retval != BCME_OK) {
				WL_ERROR(("%s: failed to updated neighbor list\n", __FUNCTION__));
				goto reject;
			}

			memset(list, 0, sizeof(list));
			/* get channel list */
			retval = wlc_wnm_neighbor_get_chanlist(wnm, bsscfg, list, ARRAYSIZE(list));
			if (retval != BCME_OK) {
				WL_ERROR(("%s: failed to get channel list\n", __FUNCTION__));
				goto reject;
			}

#ifdef WLRCC
			/* update number of channels from chanpec list */
			for (n_channels = 0; n_channels < ARRAYSIZE(list); n_channels++) {
				if (list[n_channels] == 0) {
					break;
				}
				/* update rcc list */
				rcc_add_chanspec(bsscfg->roam, list[n_channels]);
			}
			if ((bsscfg->roam->n_rcc_channels) > 0) {
				bsscfg->roam->rcc_valid = TRUE;
			}
#endif /* WLRCC */
	}

	if (req->reqmode & DOT11_BSSTRANS_REQMODE_DISASSOC_IMMINENT) {
		/*
		 * start disassociation timer to blacklist current AP,  If previous connected
		 * AP also had sent diaassoc imminent then we dont blacklisted current AP
		 * Section 8.5.14.9 : specifies that in case disassoc timer is 0, then AP has
		 * not yet determined when it would send diassociation frame
		 */
		if (bsi->disassoc_imnttmr_active) {
			wl_del_timer(wlc_tmr->wl, bsi->disassoc_imnt_timer);
			bsi->disassoc_imnttmr_active = FALSE;
		}

		if (req->disassoc_tmr != 0) {
			wl_add_timer(wlc_tmr->wl, bsi->disassoc_imnt_timer,
				(req->disassoc_tmr) * bsscfg->current_bss->beacon_period, 0);
			bsi->disassoc_imnttmr_active = TRUE;
			if (bsi->tmr_cb) {
				bsi->tmr_cb->wnm = wlc_tmr->wnm_info;
				bsi->tmr_cb->join_cfgidx = WLC_BSSCFG_IDX(bsscfg);
			}
			memcpy(&bsi->blklisted_bssid, &bsscfg->BSSID, ETHER_ADDR_LEN);
			bsi->blklisted_bssid_bandunit =
					CHSPEC_BANDUNIT(bsscfg->current_bss->chanspec);
		 }
	}

	if (req->reqmode & DOT11_BSSTRANS_REQMODE_ABRIDGED) {
		/* start the validity interval timer */
		if (bsi->nbrlist_valtimer_active) {
			wl_del_timer(wlc_tmr->wl, bsi->nbrlist_valid_timer);
			bsi->nbrlist_valtimer_active = FALSE;
		}
		/* Section 802.11-2012: 8.5.14.9 specifies that 0 is reserved value */
		if (req->validity_intrvl != 0) {
			wl_add_timer(wlc_tmr->wl, bsi->nbrlist_valid_timer,
			   (req->validity_intrvl) * bsscfg->current_bss->beacon_period, 0);
			bsi->nbrlist_valtimer_active = TRUE;
			if (bsi->tmr_cb) {
				bsi->tmr_cb->wnm = wlc_tmr->wnm_info;
				bsi->tmr_cb->join_cfgidx = WLC_BSSCFG_IDX(bsscfg);
			}
		}
	}
	if (!(req->reqmode & DOT11_BSSTRANS_REQMODE_DISASSOC_IMMINENT)) {
		if (unicast_req) {
			retval = bsstrans_send_resp(wnm->wlc, bsscfg,
			DOT11_BSSTRANS_RESP_STATUS_REJ_NO_SUITABLE_BSS,	NULL, req->token, scb);
		}
		return retval;
	}

	/* 802.11-2012 - 8.5.14.9
	 * If ESS Disassociation Imminent bit is set,
	 * then STA is to be disassociated from the ESS
	 */
	if (req->reqmode & DOT11_BSSTRANS_REQMODE_ESS_DISASSOC_IMNT) {
		 /* send disassoc packet */
		wlc_senddisassoc(wnm->wlc, bsscfg, scb, &bsscfg->BSSID, &bsscfg->BSSID,
				&bsscfg->cur_etheraddr, DOT11_RC_DISASSOC_LEAVING);
		/* cleanup keys, etc */
		wlc_scb_disassoc_cleanup(wnm->wlc, scb);

		/* change scb state */
		wlc_scb_clearstatebit(wnm->wlc, scb, AUTHENTICATED | ASSOCIATED | AUTHORIZED);

		wlc_disassoc_complete(bsscfg, WLC_E_STATUS_SUCCESS, &bsscfg->BSSID,
				DOT11_RC_DISASSOC_LEAVING, DOT11_BSSTYPE_INFRASTRUCTURE);
	}

	/* 802.11-2012 - 8.5.14.9 and 10.23.6.4
	 * If the Disassociation Imminent bit is set,
	 * then STA should start Fast BSS Transition (802.11r)
	 */
	if (req->reqmode & DOT11_BSSTRANS_REQMODE_DISASSOC_IMMINENT) {
		/* TODO: start Fast BSS Transition (FBT) */
	}

	if (req->reqmode & DOT11_BSSTRANS_REQMODE_BSS_TERM_INCL) {
		/* TODO: not supported now */
	}

#ifdef WBTEXT_BTMDBG
	{
		int i;

		WL_ERROR(("Received BSS TM Request\n"));
		WL_ERROR(("Reqmode: 0x%x\n", req->reqmode));
		WL_ERROR(("Channel list (%d): \n", n_channels));
		for (i = 0; i < n_channels; i++) {
			WL_ERROR(("  0x%x -->\n", list[i]));
		}

		wlc_wnm_print_nbrlist(wnm, cfg);
	}
#endif /* WBTEXT_BTMDBG */
	} else if (req->reqmode & DOT11_BSSTRANS_REQMODE_ABRIDGED) {
		/* If abridged bit is set a partial scan is enough */
		retval = bsstrans_req_get_chanlist(bsscfg, body, &body_len, list, &n_channels);
		if (retval != BCME_OK) {
			WL_ERROR(("%s: discarding. invalid ap recommendation.\n",
				__FUNCTION__));
			goto reject;
		}
	}
	/* Send BSS transition reply after roam-scan completion. Record everything needed
	 * for sending response
	 */
	if (unicast_req) {
		bsi->resp_token = req->token;
		bsi->resp_pending = TRUE;
	}

#ifdef WNM_BSSTRANS_EXT
	WBTEXT_INFO(("WBTEXT DBG: roam scan started due to reason %d, reqmode:0x%x, isrcc:%d\n",
		WLC_E_REASON_BSSTRANS_REQ, req->reqmode, bsscfg->roam->rcc_valid));

	if (bsscfg->roam->rcc_valid) {
		wlc_roam_scan(bsscfg, WLC_E_REASON_BSSTRANS_REQ,
			bsscfg->roam->rcc_channels, bsscfg->roam->n_rcc_channels);
	} else
#endif /* WNM_BSSTRANS_EXT */
	{
		wlc_roam_scan(bsscfg, WLC_E_REASON_BSSTRANS_REQ, NULL, 0);
	}

	bsi->throttle->scans_done++;
	return retval;

reject:
	if (unicast_req) {
		/* all error and unhandled case shall send general reject response */
		retval = bsstrans_send_resp(wnm->wlc, bsscfg, DOT11_BSSTRANS_RESP_STATUS_REJECT,
		NULL, req->token, scb);
	}
	return retval;
}

static int
wlc_wnm_bsstrans_req_process(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg, struct scb *scb,
	uint8 *body, int body_len, bool unicast_req)
{
	wlc_info_t *wlc = wnm->wlc;
	dot11_bsstrans_req_t* req = (dot11_bsstrans_req_t*)body;
	wl_reassoc_params_t reassoc;
	uint8 pref_max = 0;
	bool roam = TRUE;
	uint8 status = DOT11_BSSTRANS_RESP_STATUS_ACCEPT;
	int retval;

	memset(&reassoc, 0, sizeof(reassoc));
	body_len -= DOT11_BSSTRANS_REQ_LEN;
	body = req->data;

	retval = bsstrans_req_skip_unprocessed_fields(req->reqmode, &body, &body_len);
	if (retval != BCME_OK) {
		WL_ERROR(("%s: error removing unsupported fields\n", __FUNCTION__));
		return retval;
	}

	if (wlc_wnm_bsstrans_is_product_policy(wnm)) {
		return wlc_wnm_bsstrans_req_process_product(wnm, bsscfg, scb, body, body_len,
			unicast_req, req);
	}

	memset(&reassoc, 0, sizeof(reassoc));
	retval = bsstrans_req_get_pref_ap(bsscfg, body, &body_len, &reassoc, &pref_max);
	if (retval != BCME_OK) {
		return retval;
	}

	if (body_len) {
		return BCME_ERROR;
	}

	switch (wnm->bsstrans_policy) {
	/* case WL_BSSTRANS_RESP_ROAM_ALWAYS handled by default */
	case WL_BSSTRANS_POLICY_ROAM_IF_MODE:
		if (req->reqmode & (DOT11_BSSTRANS_REQMODE_DISASSOC_IMMINENT |
			DOT11_BSSTRANS_REQMODE_ESS_DISASSOC_IMNT |
			DOT11_BSSTRANS_REQMODE_BSS_TERM_INCL)) {
			break;
		} else if (pref_max == 0) {
			status = DOT11_BSSTRANS_RESP_STATUS_REJECT;
		}
		/* Fall through */
	case WL_BSSTRANS_POLICY_ROAM_IF_PREF:
		if (pref_max) {
			break;
		}
		/* Fall through */
	case WL_BSSTRANS_POLICY_WAIT:
		roam = FALSE;
		break;
	}

	if (unicast_req) {
		retval = bsstrans_send_resp(wlc, bsscfg, status, &reassoc.bssid, req->token, scb);
		if (retval != BCME_OK) {
			return retval;
		}
	}

	if (roam) {
		if (req->reqmode & DOT11_BSSTRANS_REQMODE_ESS_DISASSOC_IMNT) {
			WL_WNM(("wl%d: %s: ESS disassoc imminent \n",
				wlc->pub->unit, __FUNCTION__));
			/* send disassoc packet */
			wlc_senddisassoc(wlc, bsscfg, scb, &bsscfg->BSSID, &bsscfg->BSSID,
				&bsscfg->cur_etheraddr, DOT11_RC_DISASSOC_LEAVING);
			/* cleanup keys, etc */
			wlc_scb_disassoc_cleanup(wlc, scb);

			/* change scb state */
			wlc_scb_clearstatebit(wlc, scb, AUTHENTICATED | ASSOCIATED | AUTHORIZED);

			wlc_disassoc_complete(bsscfg, WLC_E_STATUS_SUCCESS, &bsscfg->BSSID,
				DOT11_RC_DISASSOC_LEAVING, DOT11_BSSTYPE_INFRASTRUCTURE);

		} else if (pref_max) {
			WL_WNM(("wl%d: %s: Associating to bssid provided\n",
				wlc->pub->unit, __FUNCTION__));
			wlc_reassoc(bsscfg, &reassoc);
		} else {
			WL_WNM(("wl%d: %s: Roaming\n", wlc->pub->unit, __FUNCTION__));
			wlc_roam_scan(bsscfg, WLC_E_REASON_BSSTRANS_REQ, NULL, 0);
		}
	} else {
		WL_WNM(("wl%d: %s: Waiting for disassoc\n", wlc->pub->unit, __FUNCTION__));
	}

	return BCME_OK;
}
#endif /* STA */

/* check BTM query NBR list for specified bssid */
static btq_nbr_list_t *
wlc_wnm_get_btq_nbr(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg,
		struct ether_addr *ea)
{
	btq_nbr_list_t *nbr_rep;
	wnm_bsscfg_cubby_t *wnm_cfg;

	wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);

	/* Find Neighbor Report element from list */
	nbr_rep = wnm_cfg->btq_nbr_list_head;
	while (nbr_rep) {
		if (memcmp(&nbr_rep->nbr_elt.bssid, ea, ETHER_ADDR_LEN) == 0)
			break;
		nbr_rep = nbr_rep->next;
	}

	return nbr_rep;
}
static int
wlc_wnm_add_btq_nbr(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg,
		nbr_rpt_elem_t *nbr_elt)
{
	btq_nbr_list_t *btq_nbr;
	wlc_info_t *wlc;
	wnm_bsscfg_cubby_t *wnm_cfg;

	wlc = wnm->wlc;
	wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);

	/* Find Neighbor Report element from list */
	btq_nbr = wlc_wnm_get_btq_nbr(wnm, bsscfg, &nbr_elt->bssid);

	if (btq_nbr) {
		/* over write existing nbr */
		memcpy(&btq_nbr->nbr_elt, nbr_elt, sizeof(nbr_rpt_elem_t));
	} else {
		if (wnm_cfg->btq_nbrlist_size >= wnm_cfg->max_btq_nbr_count) {
			WL_ERROR(("%s: over max btq nbr count (%d)\n",
					__FUNCTION__, wnm_cfg->max_btq_nbr_count));
			return BCME_ERROR;
		}
		btq_nbr = (btq_nbr_list_t *)MALLOC(wlc->osh, sizeof(btq_nbr_list_t));
		if (btq_nbr == NULL) {
			WL_ERROR(("wl%d: %s: MALLOC(%d) failed, malloced %d bytes\n",
					WLCWLUNIT(wlc), __FUNCTION__,
					(int)sizeof(btq_nbr_list_t), MALLOCED(wlc->osh)));
			return BCME_NOMEM;
		}
		memcpy(&btq_nbr->nbr_elt, nbr_elt, sizeof(nbr_rpt_elem_t));
		btq_nbr->next = wnm_cfg->btq_nbr_list_head;
		wnm_cfg->btq_nbr_list_head = btq_nbr;
		wnm_cfg->btq_nbrlist_size++;
	}
	return BCME_OK;
}
/* delete all elements from BTQ NBR list */
static int
wlc_wnm_del_btq_nbr(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg,
		struct ether_addr *bssid)
{
	btq_nbr_list_t *btq_nbr = NULL;
	btq_nbr_list_t *btq_nbr_prev;
	wnm_bsscfg_cubby_t *wnm_cfg;
	int ret;

	wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);
	btq_nbr = btq_nbr_prev = wnm_cfg->btq_nbr_list_head;

	while (btq_nbr) {
		if (memcmp(&btq_nbr->nbr_elt.bssid, bssid, ETHER_ADDR_LEN) == 0) {
			if (btq_nbr ==  wnm_cfg->btq_nbr_list_head) {
				wnm_cfg->btq_nbr_list_head = btq_nbr->next;
			} else {
				btq_nbr_prev->next = btq_nbr->next;
			}
			break;
		}
		btq_nbr_prev = btq_nbr;
		btq_nbr = btq_nbr->next;
	}
	if (btq_nbr) {
		MFREE(wnm->wlc->osh, btq_nbr, sizeof(btq_nbr_list_t));
		btq_nbr = NULL;
		wnm_cfg->btq_nbrlist_size--;
		WL_WNM(("BTM query element deleted\n"));
		ret = BCME_OK;
	} else {
		WL_WNM(("BTM query element for deletion not found\n"));
		ret = BCME_NOTFOUND;
	}
	return ret;
}
/* delete all elements from BTQ NBR list */
static int
wlc_wnm_del_all_btq_nbr(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg)
{
	btq_nbr_list_t *btq_nbr, *btq_nbr_next;
	wnm_bsscfg_cubby_t *wnm_cfg;

	wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);
	btq_nbr = wnm_cfg->btq_nbr_list_head;
	while (btq_nbr) {
		btq_nbr_next = btq_nbr->next;
		MFREE(wnm->wlc->osh, btq_nbr, sizeof(btq_nbr_list_t));
		btq_nbr = btq_nbr_next;
	}
	wnm_cfg->btq_nbr_list_head = NULL;
	wnm_cfg->btq_nbrlist_size = 0;

	return BCME_OK;
}
/* List all elements from BTQ NBRs */
static int
wlc_wnm_get_btq_nbr_list(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg,
		wl_btq_nbr_list_t * wl_btq_nbr_list, uint buflen)
{
	btq_nbr_list_t *btq_nbr;
	uint8 count = 0;
	uint list_len;
	wnm_bsscfg_cubby_t *wnm_cfg;
	int ver = WL_BTQ_NBR_LIST_VERSION_1;

	wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);
	list_len = OFFSETOF(wl_btq_nbr_list_t, btq_nbt_elem);
	if (buflen < (list_len + sizeof(nbr_rpt_elem_t))) {
		WL_ERROR(("Buffer is too short for BTM Query NRB List\n"));
		return BCME_BUFTOOSHORT;
	}

	buflen -= list_len;
	btq_nbr = wnm_cfg->btq_nbr_list_head;
	while (btq_nbr && count < BTM_QUERY_NBR_COUNT_MAX &&
			(buflen >= (sizeof(nbr_rpt_elem_t)))) {
		wl_btq_nbr_list->btq_nbt_elem[count].channel =
			btq_nbr->nbr_elt.channel;
		wl_btq_nbr_list->btq_nbt_elem[count].reg =
			btq_nbr->nbr_elt.reg;
		wl_btq_nbr_list->btq_nbt_elem[count].bss_trans_preference =
			btq_nbr->nbr_elt.bss_trans_preference;
		memcpy(&wl_btq_nbr_list->btq_nbt_elem[count].bssid,
				&btq_nbr->nbr_elt.bssid, ETHER_ADDR_LEN);
		btq_nbr = btq_nbr->next;
		buflen -= sizeof(nbr_rpt_elem_t);
		count++;
	}

	wl_btq_nbr_list->count = count;
	wl_btq_nbr_list->version = ver;

	return BCME_OK;
}

#ifdef WLWNM_AP
/* Place BTQ NBR in BTM query Frame */
static void
wlc_wnm_ap_btq_nbr_ie(wlc_info_t *wlc, nbr_rpt_elem_t* btq_nbr_elem,
		uint8 **bufptr, struct scb *scb, uint8 reqmode)
{
	uint32 bssid_info = 0;
	dot11_neighbor_rep_ie_t *nbr_rep;
	dot11_ngbr_bsstrans_pref_se_t *pref;
#if defined(WL_MBO) && !defined(WL_MBO_DISABLED) && defined(MBO_AP)
	bool non_preferred = FALSE;
#endif /* WL_MBO && !WL_MBO_DISABLED */

	nbr_rep = (dot11_neighbor_rep_ie_t *)*bufptr;
	nbr_rep->id = DOT11_MNG_NEIGHBOR_REP_ID;
	nbr_rep->len = DOT11_NEIGHBOR_REP_IE_FIXED_LEN;
	bcopy(&btq_nbr_elem->bssid, &nbr_rep->bssid, ETHER_ADDR_LEN);
	bcopy(&bssid_info, &nbr_rep->bssid_info, sizeof(nbr_rep->bssid_info));
	nbr_rep->reg = btq_nbr_elem->reg;
	nbr_rep->channel = btq_nbr_elem->channel;
	nbr_rep->phytype = 0;
	*bufptr += TLV_HDR_LEN + DOT11_NEIGHBOR_REP_IE_FIXED_LEN;
	/* Add BSS Trans Pref */
	pref = (dot11_ngbr_bsstrans_pref_se_t *) *bufptr;
	pref->sub_id = DOT11_NGBR_BSSTRANS_PREF_SE_ID;
	pref->len = 1;

#if defined(WL_MBO) && !defined(WL_MBO_DISABLED) && defined(MBO_AP)
	/* set preferrence to 0 if channel is marked as Non preferred by
	 * SCB in MBO module
	 */
	if (BSS_MBO_ENAB(wlc, scb->bsscfg)) {
		non_preferred = wlc_mbo_is_channel_non_preferred(wlc, scb, btq_nbr_elem->channel,
				btq_nbr_elem->reg);

		pref->preference = non_preferred ? 0: btq_nbr_elem->bss_trans_preference;
		if (memcmp(&(scb->bsscfg->BSSID), &nbr_rep->bssid, ETHER_ADDR_LEN) == 0) {
			/* if BSS termination bit or Disassoc imminent bit is set
			 * for APUT, reset APUT's preference to 0
			 */
			if (reqmode & (DOT11_BSSTRANS_REQMODE_DISASSOC_IMMINENT |
				DOT11_BSSTRANS_REQMODE_BSS_TERM_INCL)) {
				pref->preference = 0;
			}
		}
	} else {

		pref->preference = btq_nbr_elem->bss_trans_preference;
	}

#else
	pref->preference = btq_nbr_elem->bss_trans_preference;
#endif /* WL_MBO && !WL_MBO_DISABLED */
	nbr_rep->len += (TLV_HDR_LEN + DOT11_NGBR_BSSTRANS_PREF_SE_LEN);
	*bufptr += (TLV_HDR_LEN + DOT11_NGBR_BSSTRANS_PREF_SE_LEN);

	/* Add Wideband channel sub element */
	if (wf_chspec_valid(btq_nbr_elem->chanspec)) {
		uint8 *wide_bw_ch;
		wide_bw_ch = wlc_write_wide_bw_chan_ie(btq_nbr_elem->chanspec,
			*bufptr, TLV_HDR_LEN + DOT11_WIDE_BW_IE_LEN);
		*bufptr = wide_bw_ch;
		nbr_rep->len += (TLV_HDR_LEN + DOT11_WIDE_BW_IE_LEN);
	}
}
#endif /* WNM_AP */

int
wlc_wnm_recv_process_wnm(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg,
	uint action_id, struct scb *scb, struct dot11_management_header *hdr,
	uint8 *body, int body_len)
{
	wlc_info_t *wlc;
	wnm_bsscfg_cubby_t *wnm_cfg = NULL;
	wnm_scb_cubby_t *wnm_scb = NULL;
	int ret = BCME_OK;

	if (wnm == NULL)
		return ret;

	if (bsscfg == NULL)
		return ret;

	if (scb == NULL)
		return ret;

	wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);
	wnm_scb = SCB_WNM_CUBBY(wnm, scb);
	wlc = wnm->wlc;

	/* Allow broadcast address for non-AP STA */
	if (BSSCFG_AP(bsscfg) && bsscfg != wlc_bsscfg_find_by_hwaddr(wlc, &hdr->da)) {
		return ret;
	}

	switch (action_id) {
#ifdef STA
	case DOT11_WNM_ACTION_BSSTRANS_REQ:
		if (body_len < DOT11_BSSTRANS_REQ_LEN) {
			WL_WNM(("WNM BSS TRANS REQ invalid length %d\n", body_len));
			return BCME_BADLEN;
		}

		wlc_bss_mac_event(wnm->wlc, bsscfg, WLC_E_BSSTRANS_REQ, &scb->ea,
			0, 0, 0, body, body_len);

		if (wlc_wnm_bsstrans_req_process(wnm, bsscfg, scb, body, (uint16) body_len,
			!ETHER_ISBCAST(&hdr->da)))
			WL_WNM(("WNM BSS Trans req malformed\n"));
		break;
#endif /* STA */
#ifdef WLWNM_AP
	case DOT11_WNM_ACTION_TIMBC_REQ: {
		dot11_timbc_req_t *req;

		if (!WNM_TIMBC_ENABLED(wnm_cfg->cap)) {
			WL_WNM(("WNM TIM Broadcast not enabled\n"));
			return ret;
		}
		if (body_len < DOT11_TIMBC_REQ_LEN) {
			WL_WNM(("WNM TIM Broadcast invalid length %d\n", body_len));
			return BCME_BADLEN;
		}
		if (!SCB_TIMBC(wnm_scb->cap)) {
			WL_WNM(("STA unsupport TIM Broadcast function\n"));
			return BCME_UNSUPPORTED;
		}

		if (WL_WNM_ON())
			prhex("Raw TIMBC Req frame", (uchar *)body, body_len);

		req = (dot11_timbc_req_t *)body;

		wlc_wnm_timbc_req_ie_process(wlc, req->data,
			TLV_HDR_LEN + DOT11_TIMBC_REQ_IE_LEN, scb);
		wlc_wnm_send_timbc_resp_frame(wnm, bsscfg, scb, req->token);
		break;
	}
	case DOT11_WNM_ACTION_TFS_REQ: {
		dot11_tfs_req_t *tfs_req_frame;
		uint8 *buf = NULL;
		int buf_len = 0;

		if (!WNM_TFS_ENABLED(wnm_cfg->cap)) {
			WL_WNM(("WNM TFS not enabled\n"));
			return BCME_NOTENABLED;
		}
		if (body_len < DOT11_TFS_REQ_LEN) {
			WL_WNM(("WNM TFS request invalid length %d\n", body_len));
			return BCME_BADLEN;
		}
		if (!SCB_TFS(wnm_scb->cap)) {
			WL_WNM(("STA unsupport TFS mode\n"));
			return BCME_ERROR;
		}

		/* A valid TFS request frame, disregard the previous TFS request elements */
		wlc_wnm_tfs_req_free(wnm, &wnm_scb->tfs_fset_head, -1);
		ASSERT(wnm_scb->tfs_fset_head == NULL);

		tfs_req_frame = (dot11_tfs_req_t *)body;
		body += DOT11_TFS_REQ_LEN;
		body_len -= DOT11_TFS_REQ_LEN;

		/* ACKed TFS response null frame in response to TFS request null frame */
		if (body_len == 0) {
			WL_WNM(("ACKed TFS resp null frame to TFS req null frame\n"));
		}
		else {
			if (!(buf = MALLOCZ(wlc->osh, DOT11_MAX_MPDU_BODY_LEN))) {
				WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__,
					DOT11_MAX_MPDU_BODY_LEN, MALLOCED(wlc->osh)));
				ret = BCME_NOMEM;
				break;
			}

			if (wlc_wnm_parse_tfs_req_ie(wlc, scb, body, body_len, buf, &buf_len)
			    != BCME_OK) {
				/* process tfs request and generate tfs response IE */
				WL_WNM(("WNM TFS req param Error\n"));
				MFREE(wlc->osh, buf, DOT11_MAX_MPDU_BODY_LEN);
				break;
			}
		}

		wlc_wnm_send_tfs_resp_frame(wlc, bsscfg, scb, buf, buf_len, tfs_req_frame->token);

		if (buf)
			MFREE(wlc->osh, buf, DOT11_MAX_MPDU_BODY_LEN);

		break;
	}
	case DOT11_WNM_ACTION_DMS_REQ: {
		if (!WNM_DMS_ENABLED(wnm_cfg->cap)) {
			WL_WNM(("WNM DMS not enabled\n"));
			return BCME_ERROR;
		}
		if (body_len < DOT11_DMS_REQ_LEN) {
			WL_WNM(("WNM DMS request invalid length %d\n", body_len));
			return BCME_BADLEN;
		}
		if (!SCB_DMS(wnm_scb->cap)) {
			WL_WNM(("STA unsupport DMS mode\n"));
			return BCME_ERROR;
		}

		wlc_wnm_dms_req_parse_resp_send(bsscfg, scb, body, body_len);

		break;
	}
	case DOT11_WNM_ACTION_WNM_SLEEP_REQ: {
		dot11_wnm_sleep_req_t *sleep_req;
		uint8 *buf = NULL;
		int buf_len = 0;

		if (!WLWNM_ENAB(wlc->pub) || !WNM_SLEEP_ENABLED(wnm_cfg->cap)) {
			WL_WNM(("WNM Sleep-Mode not enabled\n"));
			return BCME_ERROR;
		}
		if (body_len < DOT11_WNM_SLEEP_REQ_LEN) {
			WL_WNM(("WNM-Sleep request Error\n"));
			return BCME_ERROR;
		}
		if (!SCB_WNM_SLEEP(wnm_scb->cap)) {
			WL_WNM(("STA unsupport sleep mode or tfs mode\n"));
			return BCME_UNSUPPORTED;
		}
		/* A valid WNM-Sleep request frame, disregard the previous TFS request elements */
		wlc_wnm_tfs_req_free(wnm, &wnm_scb->tfs_fset_head, -1);
		ASSERT(wnm_scb->tfs_fset_head == NULL);

		sleep_req = (dot11_wnm_sleep_req_t *)body;
		body += DOT11_WNM_SLEEP_REQ_LEN;
		body_len -= DOT11_WNM_SLEEP_REQ_LEN;

		if (!(buf = MALLOCZ(wlc->osh, DOT11_MAX_MPDU_BODY_LEN))) {
			WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__,
				DOT11_MAX_MPDU_BODY_LEN, MALLOCED(wlc->osh)));
			ret = BCME_NOMEM;
			break;
		}

		if (wlc_wnm_parse_sleep_req_ie(wlc, bsscfg, scb, body, body_len,
		    buf, &buf_len) != BCME_OK) {
			WL_WNM(("WNM-Sleep param Error\n"));
			MFREE(wlc->osh, buf, DOT11_MAX_MPDU_BODY_LEN);
			break;
		}

		wlc_wnm_send_sleep_resp_frame(wlc, bsscfg, scb, sleep_req->token, buf, buf_len);
		MFREE(wlc->osh, buf, DOT11_MAX_MPDU_BODY_LEN);

		break;
	}
	case DOT11_WNM_ACTION_BSSTRANS_QUERY: {
		dot11_bsstrans_query_t *transquery = (dot11_bsstrans_query_t *)body;
		uint8 reqmode = 0;
		bool add_self_bss_info = FALSE;

		if (body_len < DOT11_BSSTRANS_QUERY_LEN) {
			WL_WNM(("WNM BSS TRANS Query invalid length %d\n", body_len));
			return BCME_BADLEN;
		}

		wlc_bss_mac_event(wnm->wlc, bsscfg, WLC_E_BSSTRANS_QUERY, &scb->ea,
			0, 0, 0, body, body_len);

		if (!(wnm_cfg->flags & WNM_BTQ_RESP_DISABLED)) {
			if (wnm->url_len != 0 && wnm->url != NULL)
				reqmode |= DOT11_BSSTRANS_REQMODE_ESS_DISASSOC_IMNT;
			if (wnm_cfg->bsstrans_req_info.bsstrans_list_len != 0 &&
			    wnm_cfg->bsstrans_req_info.bsstrans_list != NULL)
				reqmode |= DOT11_BSSTRANS_REQMODE_PREF_LIST_INCL;
#if defined(WL_MBO) && !defined(WL_MBO_DISABLED) && defined(MBO_AP)
			/* It is solicited call, last argument conveys this information to be used
			 * at time of allocating one extra neighbor report element to be filled with
			 * BSS's own infomation
			 */
			/* wnm_cfg->bsstrans_re_info.bsstrans_len and list are not
			 * being set with wnm_add_btq_nbr operation
			 */
			reqmode |= DOT11_BSSTRANS_REQMODE_PREF_LIST_INCL;
			add_self_bss_info = TRUE;

			/* send event bsstrans query with full payload to userspace */
			wlc_bss_mac_event(wlc, bsscfg, WLC_E_WNM_BSSTRANS_QUERY, &scb->ea,
				0, 0, 0, body, body_len);

#endif /* WL_MBO && !WL_MBO_DISABLED && MBO_AP */
			wlc_wnm_send_bsstrans_request(wnm, bsscfg, scb, transquery->token,
				reqmode, 0, 0, add_self_bss_info);
		}
		break;
	}
	case DOT11_WNM_ACTION_BSSTRANS_RESP: {
		wlc_wnm_parse_bsstrans_resp(wnm, bsscfg, scb, body, body_len);
		break;
	}
	case DOT11_WNM_ACTION_TFS_NOTIFY_RESP: {

		if (!WNM_TFS_ENABLED(wnm_cfg->cap)) {
			WL_WNM(("WNM TFS not enabled\n"));
			return BCME_ERROR;
		}

		if (body_len < DOT11_TFS_NOTIFY_RESP_LEN) {
			WL_WNM(("WNM TFS-notify response invalid length %d\n", body_len));
			return BCME_BADLEN;
		}

		wlc_wnm_parse_tfs_notify_resp(wlc, scb, body, body_len);
		break;
	}
#if defined(WL_MBO) && !defined(WL_MBO_DISABLED) && defined(MBO_AP)
	case DOT11_WNM_ACTION_NOTFCTN_REQ: {
		uint8 status;
		uint8 token;
		dot11_wnm_notif_req_t* notif_req;
#ifdef BCMDBG
		prhex("Raw WNM-Notification request frame ", (uchar *)body, body_len);
#endif /* BCMDBG */
		if (BSS_MBO_ENAB(wlc, bsscfg)) {
			status = wlc_mbo_process_wnm_notif(wlc, scb, body, body_len);

			notif_req = (dot11_wnm_notif_req_t*)body;
			token = notif_req->token;
			/* send event to userspace with full payload */
			wlc_bss_mac_event(wlc, bsscfg, WLC_E_WNM_NOTIFICATION_REQ,
				&scb->ea, 0, 0, 0, body, body_len);
			/* Send response */
			wlc_wnm_send_notif_resp_frame(wlc, bsscfg, scb, token, status);
		}
		break;
	}
#endif /* WL_MBO && !MBO_DISABLED && MBO_AP */
#endif /* WLWNM_AP */
#ifdef STA
	case DOT11_WNM_ACTION_TIMBC_RESP: {
		dot11_timbc_resp_t *resp = (dot11_timbc_resp_t *)body;
		dot11_timbc_resp_ie_t *ie = (dot11_timbc_resp_ie_t*) resp->data;

		if (!WNM_TIMBC_ENABLED(wnm_cfg->cap)) {
			WL_WNM(("TIMBC not enabled\n"));
			return BCME_ERROR;
		}
		if (!SCB_TIMBC(wnm_scb->cap)) {
			WL_WNM(("AP unsupport TIMBC function\n"));
			return BCME_ERROR;
		}
		if (wlc_wnm_timbc_resp_ie_process(wlc, ie, body_len - DOT11_TIMBC_RESP_LEN, scb))
			WL_WNM(("TIMBC content/len error\n"));
		break;
	}
	case DOT11_WNM_ACTION_TFS_RESP: {
		dot11_tfs_resp_t *frame = (dot11_tfs_resp_t *)body;

		if (!WNM_TFS_ENABLED(wnm_cfg->cap)) {
			WL_WNM(("WNM TFS not enabled\n"));
			return BCME_ERROR;
		}
		if (body_len < DOT11_TFS_RESP_LEN) {
			WL_WNM(("WNM TFS response invalid length %d\n", body_len));
			return BCME_BADLEN;
		}

		body += DOT11_TFS_RESP_LEN;
		body_len -= DOT11_TFS_RESP_LEN;

		if (wlc_wnm_tfs_resp_ie_parse(wlc, wnm_cfg, body, body_len, frame->token)) {
			WL_WNM(("WNM TFS resp param Error\n"));
			return BCME_ERROR;
		}

		break;
	}
	case DOT11_WNM_ACTION_TFS_NOTIFY_REQ: {
		dot11_tfs_notify_req_t *notify = (dot11_tfs_notify_req_t *)body;
		int i;
		wnm_tfs_fset_t *fset;
		if (!WNM_TFS_ENABLED(wnm_cfg->cap)) {
			WL_WNM(("WNM TFS not enabled\n"));
			return BCME_ERROR;
		}

		if (body_len < DOT11_TFS_NOTIFY_REQ_LEN ||
			body_len != notify->tfs_id_cnt + DOT11_TFS_NOTIFY_REQ_LEN) {
			WL_WNM(("WNM TFS-notify request invalid length\n"));
			return BCME_BADLEN;
		}
		for (i = 0; i < notify->tfs_id_cnt; i++) {
			for (fset = wnm_cfg->tfs_fset_head; fset; fset = fset->next) {
				if (fset->tfs_id == notify->tfs_id[i]) {
					fset->notify = TRUE;
					break;
				}
			}

			if (fset == NULL)
				WL_WNM(("WNM TFS-notify contains unregistered TFS ID: %d\n",
					notify->tfs_id[i]));
		}
		break;
	}
	case DOT11_WNM_ACTION_DMS_RESP: {
		dot11_dms_resp_t *frame;
		dot11_dms_resp_ie_t *dms_resp_ie;

		if (!WNM_DMS_ENABLED(wnm_cfg->cap))
			goto malformed;

		if (body_len <
			DOT11_DMS_RESP_LEN + DOT11_DMS_RESP_IE_LEN + DOT11_DMS_RESP_STATUS_LEN)
			goto malformed;
		frame = (dot11_dms_resp_t *)body;
		dms_resp_ie = (dot11_dms_resp_ie_t *)frame->data;
		if (dms_resp_ie->id != DOT11_MNG_DMS_RESPONSE_ID)
			goto malformed;

		if (dms_resp_ie->len > body_len
			- DOT11_DMS_RESP_LEN - DOT11_DMS_RESP_IE_LEN)
			goto malformed;

		if (wlc_wnm_dms_resp_frame_parse(bsscfg, frame->token, dms_resp_ie->data,
			dms_resp_ie->len))
			goto malformed;

		wlc_wnm_pm_ignore_bcmc_upd(bsscfg, wnm_cfg);
		break;

	malformed:
		WL_WNM(("Malformed/unexpected WNM DMS resp\n"));
		return BCME_ERROR;

	}
	case DOT11_WNM_ACTION_WNM_SLEEP_RESP: {
		if (!WLWNM_ENAB(wlc->pub) || !WNM_SLEEP_ENABLED(wnm_cfg->cap)) {
			WL_WNM(("WNM Sleep-Mode not enabled\n"));
			return BCME_ERROR;
		}
		if (body_len < DOT11_WNM_SLEEP_RESP_LEN) {
			WL_WNM(("WNM Sleep-response invalid length %d\n", body_len));
			return BCME_BADLEN;
		}

		wlc_wnm_sleep_resp_frame_parse(wnm, bsscfg, scb, body, body_len);
		break;
	}
#endif /* STA */
	default:
		break;
	}

	return ret;
}

#ifdef STA
void
wlc_wnm_recv_process_uwnm(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg,
	uint action_id, struct scb *scb, struct dot11_management_header *hdr,
	uint8 *body, int body_len)
{
	wlc_info_t *wlc;
	wnm_bsscfg_cubby_t *wnm_cfg = NULL;

	if (wnm == NULL)
		return;

	if (bsscfg == NULL)
		return;

	if (scb == NULL)
		return;

	wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);
	wlc = wnm->wlc;

	(void)wnm_cfg;
	(void)wlc;

	switch (action_id) {
	case DOT11_UWNM_ACTION_TIM: {
		break;
	}

	default:
		break;

	}
}

static void
wlc_wnm_tfs_fset_free(wlc_info_t *wlc, wnm_tfs_fset_t *fset)
{
	wnm_tfs_filter_t *filter = fset->filter_head;
	while (filter) {
		wnm_tfs_filter_t *next = filter->next;
		wnm_tclas_free(wlc, filter->tclas_head);
		MFREE(wlc->osh, filter, sizeof(wnm_tfs_filter_t));
		filter = next;
	}
	MFREE(wlc->osh, fset, sizeof(wnm_tfs_fset_t));
}
static void
wlc_wnm_tfs_free(wlc_bsscfg_t *bsscfg)
{
	wnm_bsscfg_cubby_t* wnm_cfg = WNM_BSSCFG_CUBBY(bsscfg->wlc->wnm_info, bsscfg);
	wnm_tfs_fset_t *fset = wnm_cfg->tfs_fset_head;
	while (fset) {
		wnm_tfs_fset_t *next = fset->next;
		wlc_wnm_tfs_fset_free(bsscfg->wlc, fset);
		fset = next;
	}
	wnm_cfg->tfs_fset_head = NULL;
}
static int
wlc_wnm_tfs_set(wlc_bsscfg_t *bsscfg, wnm_bsscfg_cubby_t *wnm_cfg, wl_tfs_set_t *tfs_set)
{
	wlc_wnm_info_t *wnm = bsscfg->wlc->wnm_info;
	wlc_wnm_cmn_info_t *wnm_cmn = wnm->cmn;
	struct scb *scb_ap = wlc_scbfind(wnm->wlc, bsscfg, &bsscfg->current_bss->BSSID);

	if (!WNM_TFS_ENABLED(wnm_cfg->cap))
		return BCME_NOTREADY;

	/* TCLAS already added? */
	if (tfs_set->tfs_id) {
		wnm_tfs_fset_t *fset;
		wnm_tfs_filter_t *filter;

		if (wnm_cmn->tclas_cnt == 1 && tfs_set->tclas_proc) {
			WL_WNM(("%s: TCLAS Proc cannot be set for only 1 TCLAS\n", __FUNCTION__));
			return BCME_BADARG;
		}

		/* User ID already taken ? */
		for (fset = wnm_cfg->tfs_fset_head; fset; fset = fset->next)
			if (tfs_set->tfs_id == fset->tfs_id)
				break;

		/* No matching tfs_id? so create a new filter set */
		if (fset == NULL) {
			if (wnm_cmn->tclas_cnt == 0)
				return BCME_BADARG;

			fset = MALLOC(bsscfg->wlc->osh, sizeof(wnm_tfs_fset_t));
			if (fset == NULL)
				return BCME_NOMEM;
			WNM_ELEMENT_LIST_ADD(wnm_cfg->tfs_fset_head, fset);

			fset->tfs_id = tfs_set->tfs_id;
			fset->filter_head = NULL;
		}

		fset->actcode = tfs_set->actcode;
		fset->notify = FALSE;

		if (wnm_cmn->tclas_cnt) {
			if (scb_ap == NULL)
				fset->status = TFS_STATUS_NOT_ASSOC;
			else if (!SCB_TFS(SCB_WNM_CUBBY(wnm, scb_ap)->cap))
				fset->status = TFS_STATUS_NOT_SUPPORT;
			else
				fset->status = TFS_STATUS_VALIDATING;

			filter = MALLOC(bsscfg->wlc->osh, sizeof(wnm_tfs_filter_t));
			if (filter == NULL)
				return BCME_NOMEM;
			WNM_ELEMENT_LIST_ADD(fset->filter_head, filter);

			filter->tclas_proc = tfs_set->tclas_proc;
			filter->status = DOT11_TFS_STATUS_NONE;

			/* Move all tclas in wnm_info to fset */
			filter->tclas_head = wnm_cmn->tclas_head;
			filter->tclas_cnt = wnm_cmn->tclas_cnt;
			wnm_cmn->tclas_head = NULL;
			wnm_cmn->tclas_cnt = 0;
		}

		/* Send the req frame with ALL filter set not enabled */
	} else if (wnm_cmn->tclas_cnt) {
		WL_WNM(("%s: TFS ID must be provided if TCLAS has been added\n", __FUNCTION__));
		return BCME_BADARG;
	} else if (tfs_set->send == 0 || tfs_set->tclas_proc || tfs_set->actcode) {
		WL_WNM(("%s: TFS ID or 'send' must be set\n", __FUNCTION__));
		return BCME_BADARG;
	}

	if (tfs_set->send && scb_ap && SCB_TFS(SCB_WNM_CUBBY(wnm, scb_ap)->cap))
		return wlc_wnm_tfs_req_frame_send(bsscfg);

	return BCME_OK;
}

static int
wlc_wnm_tfs_term(wlc_bsscfg_t *bsscfg, wnm_bsscfg_cubby_t *wnm_cfg, wl_tfs_term_t *tfs_term)
{
	/* Trick to modify tfs_fset_head seamlessly */
	wnm_tfs_fset_t *prev = (wnm_tfs_fset_t *) &wnm_cfg->tfs_fset_head;
	wnm_tfs_fset_t *fset;
	int req_send = FALSE;

	if (!WNM_TFS_ENABLED(wnm_cfg->cap))
		return BCME_NOTREADY;

	for (fset = prev->next; fset; prev = fset, fset = fset->next) {

		if (tfs_term->tfs_id && tfs_term->tfs_id != fset->tfs_id)
			continue;

		/* Handle also case where filter set was just set but no answer yet from AP */
		if (fset->status == TFS_STATUS_VALIDATED ||
			fset->status == TFS_STATUS_VALIDATING) {
			/* At least one filter set is enabled on AP: send remove req */
			req_send = TRUE;
			fset->status = TFS_STATUS_DISABLING;
			/* Flag to delete filter set when response frame will be Rx */
			if (tfs_term->del)
				fset->tfs_id = 0;

		} else if (tfs_term->del) {
			/* Delete now all filter sets not registered on AP side */
			prev->next = fset->next;
			wlc_wnm_tfs_fset_free(bsscfg->wlc, fset);
			/* Next iteration need valid fset */
			fset = prev;
		} else {
			fset->status = TFS_STATUS_DISABLED;
		}
	}

	if (req_send)
		return wlc_wnm_tfs_req_frame_send(bsscfg);

	return BCME_OK;
}
static int
wlc_wnm_dms_set(wlc_bsscfg_t *bsscfg, wnm_bsscfg_cubby_t *wnm_cfg, wl_dms_set_t *dms_set)
{
	wlc_wnm_info_t *wnm = bsscfg->wlc->wnm_info;
	wlc_wnm_cmn_info_t *wnm_cmn = wnm->cmn;
	wnm_dms_info_t *dms_info = &wnm_cfg->dms_info;
	struct scb *scb_ap = wlc_scbfind(wnm->wlc, bsscfg, &bsscfg->current_bss->BSSID);
	wnm_scb_cubby_t *wnm_scbap = SCB_WNM_CUBBY(wnm, scb_ap);

	if (!WNM_DMS_ENABLED(wnm_cfg->cap))
		return BCME_NOTREADY;

	/* TCLAS already added? */
	if (wnm_cmn->tclas_cnt) {
		wnm_dms_desc_t *dms_desc;

		if (dms_set->user_id == 0)
			return BCME_BADARG;

		/* User ID already taken ? */
		for (dms_desc = dms_info->dms_desc_head; dms_desc;
			dms_desc = dms_desc->next)
			if (dms_set->user_id == dms_desc->user_id)
				return BCME_BADARG;

		if (wnm_cmn->tclas_cnt == 1 && dms_set->tclas_proc)
			return BCME_BADARG;

		dms_desc = (wnm_dms_desc_t *)MALLOCZ(bsscfg->wlc->osh, sizeof(wnm_dms_desc_t));
		if (dms_desc == NULL) {
			WL_ERROR((WLC_BSS_MALLOC_ERR, WLCWLUNIT(bsscfg->wlc),
				WLC_BSSCFG_IDX(bsscfg), __FUNCTION__, (int)sizeof(wnm_dms_desc_t),
				MALLOCED(bsscfg->wlc->osh)));
			return BCME_NOMEM;
		}
		dms_desc->user_id = dms_set->user_id;
		if (scb_ap == NULL)
			dms_desc->status = DMS_STATUS_NOT_ASSOC;
		else if (!SCB_DMS(wnm_scbap->cap))
			dms_desc->status = DMS_STATUS_NOT_SUPPORT;
		else
			dms_desc->status = DMS_STATUS_IN_PROGRESS;

		dms_desc->tclas_proc = dms_set->tclas_proc;

		WNM_ELEMENT_LIST_ADD(dms_info->dms_desc_head, dms_desc);

		/* Move all tclas in wnm_info to dms_desc */
		dms_desc->tclas_head = wnm_cmn->tclas_head;
		dms_desc->tclas_cnt = wnm_cmn->tclas_cnt;
		wnm_cmn->tclas_head = NULL;
		wnm_cmn->tclas_cnt = 0;

		/* Send the req frame with ALL desc not enabled */
		dms_set->user_id = 0;
	} else {
		if (dms_set->send == 0 || dms_set->tclas_proc)
			return BCME_BADARG;
	}

	if (dms_set->send && scb_ap && SCB_DMS(wnm_scbap->cap)) {
		wnm_dms_desc_t *dms_desc;
		bool send_req = FALSE;
		for (dms_desc = dms_info->dms_desc_head; dms_desc;
			dms_desc = dms_desc->next)
			if ((dms_set->user_id == dms_desc->user_id) || (dms_set->user_id == 0)) {
				/* Check if proxy-arp is pre-requisite for DMS */
				if ((wnm->cmn->dms_dependency & DMS_DEP_PROXY_ARP) &&
					!SCB_PROXYARP(wnm_scbap->cap)) {
					dms_desc->status = DMS_STATUS_REQ_MISMATCH;
				}
				else {
					dms_desc->status = DMS_STATUS_IN_PROGRESS;
					send_req = TRUE;
				}
			}
		if (send_req == TRUE)
			return wlc_wnm_dms_req_frame_send(bsscfg, DOT11_DMS_REQ_TYPE_ADD,
				dms_set->user_id);
	}
	return BCME_OK;
}

static int
wlc_wnm_dms_term(wlc_bsscfg_t *bsscfg, wnm_bsscfg_cubby_t *wnm_cfg, wl_dms_term_t *dms_term)
{
	wnm_dms_info_t *dms_info = &wnm_cfg->dms_info;
	/* Trick to modify dms_desc_head seamlessly */
	wnm_dms_desc_t *prev = (wnm_dms_desc_t *) &dms_info->dms_desc_head;
	wnm_dms_desc_t *dms_desc;
	int req_send = FALSE;

	if (!WNM_DMS_ENABLED(wnm_cfg->cap))
		return BCME_NOTREADY;

	for (dms_desc = prev->next; dms_desc; prev = dms_desc, dms_desc = dms_desc->next) {

		if (dms_term->user_id && dms_term->user_id != dms_desc->user_id)
			continue;

		/* Handle also case where desc was just set but no answer yet from AP */
		if (dms_desc->status == DMS_STATUS_ACCEPTED) {
			/* At least one desc is enabled on AP: send remove req */
			req_send = TRUE;
			dms_desc->status = DMS_STATUS_IN_PROGRESS;

			if (dms_term->del)
				/* Flag to delete desc when response frame will be Rx */
				dms_desc->del = TRUE;

		} else if (dms_desc->status == DMS_STATUS_IN_PROGRESS) {
			dms_desc->status = DMS_STATUS_DISABLED;
			dms_desc->dms_id = 0;
			dms_desc->token = 0;

			if (dms_term->del) {
				/* Delete as it is not confirmed by AP yet. */
				prev->next = dms_desc->next;
				wlc_wnm_dms_desc_free(bsscfg->wlc, dms_desc);
				/* Next iteration need valid dms_desc */
				dms_desc = prev;
			}
		} else if (dms_term->del) {
			/* Delete now all desc not registered on AP side */
			prev->next = dms_desc->next;
			wlc_wnm_dms_desc_free(bsscfg->wlc, dms_desc);
			/* Next iteration need valid dms_desc */
			dms_desc = prev;
		} else {
			dms_desc->status = DMS_STATUS_DISABLED;
			dms_desc->dms_id = 0;
		}
	}

	if (req_send)
		return wlc_wnm_dms_req_frame_send(bsscfg, DOT11_DMS_REQ_TYPE_REMOVE,
			dms_term->user_id);

	return BCME_OK;
}
#endif /* STA */
static uint8 *
wlc_wnm_tfs_status_fset(wnm_tfs_fset_t *fset, uint8 *p, uint8 *p_end,
	uint8 *cnt, struct ether_addr *ea)
{
	/* Convert each 'wnm_tfs_fset_t' as 'wl_tfs_fset_t' */
	for (; fset; fset = fset->next) {
		wl_tfs_fset_t *wl_fset = (wl_tfs_fset_t *) p;
		wnm_tfs_filter_t *filter;

		p += WL_TFS_FSET_FIXED_SIZE;
		if (p_end < p)
			return p;

		memcpy(&wl_fset->ea, ea, ETHER_ADDR_LEN);
		wl_fset->tfs_id = fset->tfs_id;
		wl_fset->status = fset->status;
		wl_fset->actcode = fset->actcode;
		wl_fset->token = fset->token;
		wl_fset->notify = fset->notify;
		wl_fset->filter_cnt = 0;

		/* Convert each 'wnm_tfs_filter_t' as 'wl_tfs_filter_t' */
		for (filter = fset->filter_head; filter; filter = filter->next) {
			wl_tfs_filter_t *wl_filter = (wl_tfs_filter_t *) p;
			wnm_tclas_t *tclas;

			p += WL_TFS_FILTER_FIXED_SIZE;
			if (p_end < p)
				return p;

			wl_filter->status = filter->status;
			wl_filter->tclas_proc = filter->tclas_proc;
			wl_filter->tclas_cnt = 0;

			/* Convert each 'wnm_tclas_t' as 'wl_tclas_t' */
			for (tclas = filter->tclas_head; tclas; tclas = tclas->next) {
				wl_tclas_t *wl_tclas = (wl_tclas_t *)p;

				p += WL_TCLAS_FIXED_SIZE + tclas->fc_len;
				if (p_end < p)
					return p;

				wl_tclas->user_priority = tclas->user_priority;
				wl_tclas->fc_len = tclas->fc_len;
				memcpy(&wl_tclas->fc, &tclas->fc, tclas->fc_len);
				wl_filter->tclas_cnt++;
			}
			wl_fset->filter_cnt++;
		}
		(*cnt)++;
	}

	return p;
}

static int
wlc_wnm_tfs_status(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg, wl_tfs_status_t *list, int list_len)
{
	uint8 *p = (uint8 *) list->fset;
	list->fset_cnt = 0;
#ifdef WLWNM_AP
	if (BSSCFG_AP(bsscfg)) {
		struct scb *scb;
		struct scb_iter scbiter;
		FOREACH_BSS_SCB(wnm->wlc->scbstate, &scbiter, bsscfg, scb) {
			wnm_scb_cubby_t *wnm_scb = SCB_WNM_CUBBY(wnm, scb);

			p = wlc_wnm_tfs_status_fset(wnm_scb->tfs_fset_head,
				p, list_len + (uint8*) list, &list->fset_cnt, &scb->ea);
			if (list_len + (uint8*) list < p)
				return BCME_BUFTOOSHORT;
		}
	}
#endif // endif
#ifdef STA
	if (BSSCFG_STA(bsscfg)) {
		wnm_bsscfg_cubby_t *wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);

		p = wlc_wnm_tfs_status_fset(wnm_cfg->tfs_fset_head,
			p, list_len + (uint8*) list, &list->fset_cnt, &bsscfg->BSSID);
		if (list_len + (uint8*) list < p)
			return BCME_BUFTOOSHORT;
	}
#endif // endif
	return BCME_OK;
}
static int
wlc_wnm_dms_status(wnm_bsscfg_cubby_t *wnm_cfg, wl_dms_status_t *dms_list, int list_len)
{
	wnm_dms_info_t *dms_info = &wnm_cfg->dms_info;
	wnm_dms_desc_t *dms_desc;
	uint8 *p = (uint8 *) dms_list->desc;
	int desc_cnt = 0;

	if (!WNM_DMS_ENABLED(wnm_cfg->cap))
		return BCME_UNSUPPORTED;

	/* We convert each 'wnm_dms_desc_t' to 'wl_dms_desc_t' then we set the count */
	for (dms_desc = dms_info->dms_desc_head; dms_desc; dms_desc = dms_desc->next) {
		wl_dms_desc_t *wl_desc = (wl_dms_desc_t *)p;
		wnm_tclas_t *tclas;
#ifdef WLWNM_AP
		wnm_dms_scb_t *dscb;
#endif // endif
		/* Fixed parameters */
		p += WL_DMS_DESC_FIXED_SIZE;
		if (p - (uint8*)dms_list > list_len)
			return BCME_BUFTOOSHORT;

		memset(wl_desc, 0, WL_DMS_DESC_FIXED_SIZE);
#ifdef STA
		wl_desc->user_id = dms_desc->user_id;
		wl_desc->status = dms_desc->status;
		wl_desc->token = dms_desc->token;
#endif // endif
		wl_desc->dms_id = dms_desc->dms_id;
		wl_desc->tclas_proc = dms_desc->tclas_proc;
#ifdef WLWNM_AP
		/* SCB EA list */
		for (dscb = dms_desc->dms_scb_head; dscb; dscb = dscb->next) {
			struct ether_addr *ea = (struct ether_addr *)p;

			p += ETHER_ADDR_LEN;
			if (p - (uint8*)dms_list > list_len)
				return BCME_BUFTOOSHORT;
			memcpy(ea, &dscb->scb->ea, ETHER_ADDR_LEN);
		}
#endif // endif
		wl_desc->mac_len = (uint8)(p - wl_desc->data);

		/* TCLAS list */
		for (tclas = dms_desc->tclas_head; tclas; tclas = tclas->next) {
			wl_tclas_t *wl_tclas = (wl_tclas_t *)p;

			p += WL_TCLAS_FIXED_SIZE + tclas->fc_len;
			if (p - (uint8*)dms_list > list_len)
				return BCME_BUFTOOSHORT;

			wl_tclas->user_priority = tclas->user_priority;
			wl_tclas->fc_len = tclas->fc_len;
			memcpy(&wl_tclas->fc, &tclas->fc, tclas->fc_len);
		}
		wl_desc->tclas_len = (uint8)(p - wl_desc->data - wl_desc->mac_len);

		/* TSPEC not supported */
		/* DMS subelem not supported */

		desc_cnt++;
	}

	dms_list->cnt = desc_cnt;

	return BCME_OK;
}

#if defined(STA) && defined(WNM_BSSTRANS_EXT)
/* TODO:6GHZ: extend function with 6Ghz */
static void
wnm_bsstrans_set_values(uint8 band, void *ptr_5g, void *ptr_2g, void *data, uint8 size)
{
	if (!ptr_5g || !ptr_2g) {
		WL_ERROR(("null pointer\n"));
		return;
	}

	if ((band == WLC_BAND_5G) || (band == WLC_BAND_ALL)) {
		memset(ptr_5g, 0, size);
		memcpy(ptr_5g, data, size);
	}
	if ((band == WLC_BAND_2G) || (band == WLC_BAND_ALL)) {
		memset(ptr_2g, 0, size);
		memcpy(ptr_2g, data, size);
	}
}

static int
wlc_wnm_get_bss_select_table(wnm_bss_select_factor_cfg_t *btcfg,
	void *params, uint len, wnm_bsscfg_cubby_t *wnm_cfg)
{
	int err = BCME_OK;

	if (len < sizeof(*btcfg) +
			(sizeof(wnm_bss_select_factor_t) * BSS_MAXTABLE_SIZE)) {
		return BCME_BUFTOOSHORT;
	}

	memcpy(btcfg, params, sizeof(*btcfg));
	if (btcfg->type != WNM_BSS_SELECT_TYPE_RSSI &&
		btcfg->type != WNM_BSS_SELECT_TYPE_CU) {
		return BCME_RANGE;
	}

	if (btcfg->band != WLC_BAND_2G && btcfg->band != WLC_BAND_5G && btcfg->band != WLC_BAND_6G)
	{
		return BCME_RANGE;
	}

	btcfg->version = WNM_BSS_SELECT_FACTOR_VERSION;
	if (btcfg->type == WNM_BSS_SELECT_TYPE_RSSI) {
		btcfg->count = wnm_cfg->bss_select_table[btcfg->band-1]->rssi_table_size;
		memcpy(btcfg->params, wnm_cfg->bss_select_table[btcfg->band-1]->rssi_table,
			btcfg->count * sizeof(wnm_bss_select_factor_cfg_t));
	}
	else {
		btcfg->count = wnm_cfg->bss_select_table[btcfg->band-1]->cu_table_size;
		memcpy(btcfg->params, wnm_cfg->bss_select_table[btcfg->band-1]->cu_table,
			btcfg->count * sizeof(wnm_bss_select_factor_cfg_t));
	}
	return err;
}

/* TODO:6GHZ: extend function below with 6GHz */
static int
wlc_wnm_set_bss_select_table(wnm_bss_select_factor_cfg_t *btcfg,
	void *params, wnm_bsscfg_cubby_t *wnm_cfg)
{
	int err = BCME_OK;
	void *ptr_5g, *ptr_2g;

	if (btcfg->band <= WLC_BAND_AUTO || btcfg->band > WLC_BAND_6G) {
		return BCME_RANGE;
	}

	if ((btcfg->count == 0) || (btcfg->count > BSS_MAXTABLE_SIZE)) {
		return BCME_RANGE;
	}

	/* update count */
	if (btcfg->type == WNM_BSS_SELECT_TYPE_RSSI) {
		ptr_5g = &wnm_cfg->bss_select_table[WLC_BAND_5G-1]->rssi_table_size;
		ptr_2g = &wnm_cfg->bss_select_table[WLC_BAND_2G-1]->rssi_table_size;
	}
	else {
		ptr_5g = &wnm_cfg->bss_select_table[WLC_BAND_5G-1]->cu_table_size;
		ptr_2g = &wnm_cfg->bss_select_table[WLC_BAND_2G-1]->cu_table_size;
	}
	wnm_bsstrans_set_values(btcfg->band, ptr_5g, ptr_2g, &btcfg->count,
			sizeof(btcfg->count));

	/* update table */
	if (btcfg->type == WNM_BSS_SELECT_TYPE_RSSI) {
		ptr_5g = wnm_cfg->bss_select_table[WLC_BAND_5G-1]->rssi_table;
		ptr_2g = wnm_cfg->bss_select_table[WLC_BAND_2G-1]->rssi_table;
	}
	else {
		ptr_5g = wnm_cfg->bss_select_table[WLC_BAND_5G-1]->cu_table;
		ptr_2g = wnm_cfg->bss_select_table[WLC_BAND_2G-1]->cu_table;
	}
	wnm_bsstrans_set_values(btcfg->band, ptr_5g, ptr_2g, btcfg->params,
			btcfg->count * sizeof(wnm_bss_select_factor_params_t));
	return err;
}

#endif /* STA && WNM_BSSTRANS_EXT */

static int
wlc_wnm_doiovar(void *hdl, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif)
{
	wlc_wnm_info_t *wnm = (wlc_wnm_info_t *)hdl;
	wlc_info_t * wlc = wnm->wlc;
	wlc_bsscfg_t *bsscfg;
	wnm_bsscfg_cubby_t *wnm_cfg;
#ifdef STA
	wnm_bsstrans_sta_info_t *bsi;
#endif /* STA */
	int err = BCME_OK;
	int32 int_val = 0;
	int32 int_val2 = 0;
	int32 *ret_int_ptr;
#if defined(STA) && defined(WNM_BSSTRANS_EXT)
	void *ptr_5g, *ptr_2g;
#endif /* STA && WNM_BSSTRANS_EXT */

	/* update bsscfg w/provided interface context */
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);
#ifdef STA
	bsi = wnm_cfg->bsstrans_stainfo;
#endif /* STA */

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	if (p_len >= (int)sizeof(int_val) * 2)
		bcopy((void*)((uintptr)params + sizeof(int_val)), &int_val2, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	/* update wlcif pointer */
	if (wlcif == NULL)
		wlcif = bsscfg->wlcif;
	ASSERT(wlcif != NULL);

	switch (actionid) {
	case IOV_GVAL(IOV_WNM):
		*ret_int_ptr = (int32)wnm_cfg->cap;

		break;
	case IOV_SVAL(IOV_WNM): {
#ifdef WLWNM_AP
		if (BSSCFG_AP(bsscfg) && (int_val & WL_WNM_TIMBC) && !wlc_wnm_timbc_support(wlc)) {
			WL_ERROR(("TIMBC AP not supported with this chip or platform\n"));
			return BCME_UNSUPPORTED;
		}
#endif /* WLWNM_AP */
		/* Force TFS if sleep is requested */
		if (WNM_SLEEP_ENABLED(int_val))
			int_val |= WL_WNM_TFS;

#ifdef STA
		if (BSSCFG_STA(bsscfg)) {
			if (((wnm_cfg->cap ^ int_val) & WL_WNM_TFS) && wnm_cfg->tfs_fset_head) {
				WL_ERROR(("Cannot disable TFS with filters set registered\n"));
				return BCME_BUSY;
			}
			if (((wnm_cfg->cap ^ int_val) & WL_WNM_DMS) &&
				wnm_cfg->dms_info.dms_desc_head) {
				WL_ERROR(("Cannot disable DMS with descriptors registered\n"));
				return BCME_BUSY;
			}
		}
#endif /* STA */
		/* update to per bsscfg cubby */
#ifdef WLWNM_BRCM
		if (!WLWNM_BRCM_ENAB(wlc->pub))
#endif // endif
		{
			wlc_wnm_set_cap(wlc, bsscfg, int_val);
		}
#ifdef WLWNM_AP
		if (BSSCFG_AP(bsscfg)) {
			wlc_wnm_timbc_switch(wlc, bsscfg);
		}
#endif /* WLWNM_AP */

		if (int_val)
			wlc->pub->cmn->_wnm = TRUE;
		else {
			wlc_bsscfg_t *cfg;
			int idx = 0;
			bool _wnm = FALSE;
			FOREACH_BSS(wlc, idx, cfg) {
				if (wlc_wnm_get_cap(wlc, cfg) != 0) {
					_wnm = TRUE;
					break;
				}
			}
			wlc->pub->cmn->_wnm = _wnm;
		}
		break;
	}
	case IOV_GVAL(IOV_WNM_MAXIDLE): {
		*ret_int_ptr = (int32)(wnm_cfg->bss_max_idle_period);
		break;
	}
#ifdef WLWNM_AP
	case IOV_GVAL(IOV_WNM_BSSTRANS_URL): {
		wnm_url_t *url = (wnm_url_t *)arg;
		if ((uint) len < wnm->url_len + sizeof(wnm_url_t) - 1) {
			err = BCME_BUFTOOSHORT;
			break;
		}
		url->len = wnm->url_len;
		if (wnm->url_len) {
			bcopy(wnm->url, &url->data[0], wnm->url_len);
		}
		break;
	}
	case IOV_SVAL(IOV_WNM_BSSTRANS_URL): {
		wnm_url_t *url = (wnm_url_t *)arg;
		if (len < (int)sizeof(wnm_url_t) - 1) {
			err = BCME_BUFTOOSHORT;
			break;
		}
		if ((uint) len < url->len + sizeof(wnm_url_t) - 1) {
			err = BCME_BUFTOOSHORT;
			break;
		}
		if (url->len != wnm->url_len) {
			if (wnm->url_len) {
				MFREE(wlc->osh, wnm->url, wnm->url_len);
				wnm->url_len = 0;
			}
			if (url->len == 0)
				break;
			if (!(wnm->url = MALLOCZ(wlc->osh, url->len))) {
				WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__, url->len,
					MALLOCED(wlc->osh)));
				err = BCME_NOMEM;
				break;
			}
			wnm->url_len = url->len;
		}
		if (url->len)
			bcopy(&url->data[0], wnm->url, wnm->url_len);
		break;
	}
	case IOV_SVAL(IOV_WNM_MAXIDLE): {
		if (!BSSCFG_AP(bsscfg) || !WNM_MAXIDLE_ENABLED(wnm_cfg->cap)) {
			err = BCME_UNSUPPORTED;
			break;
		}

		/* Range only support for unsigned 16-bit width */
		if (int_val > 65535) {
			err = BCME_RANGE;
			break;
		}

		wnm_cfg->bss_max_idle_period = (uint16) int_val;
		wnm_cfg->bss_idle_opt = int_val2? TRUE : FALSE;

		break;
	}
	case IOV_GVAL(IOV_WNM_TIMBC_OFFSET): {
		wl_timbc_offset_t *wl_timbc_offset = (wl_timbc_offset_t *)arg;
		wl_timbc_offset->offset = wnm_cfg->timbc_offset;
		wl_timbc_offset->fix_intv = wnm_cfg->timbc_fix_intv;
		wl_timbc_offset->rate_override = wnm_cfg->timbc_rate;
		wl_timbc_offset->tsf_present = wnm_cfg->timbc_tsf;

		break;
	}
	case IOV_SVAL(IOV_WNM_TIMBC_OFFSET): {
		wl_timbc_offset_t *wl_timbc_offset = (wl_timbc_offset_t *)arg;

		if (!WNM_TIMBC_ENABLED(wnm_cfg->cap)) {
			err = BCME_UNSUPPORTED;
			break;
		}

		/* limit range within positive/negative beacon_period */
		if (wl_timbc_offset->offset >= 32767 || wl_timbc_offset->offset <= -32768) {
			WL_ERROR(("! -32768 <= (%d) <= 32767\n", wl_timbc_offset->offset));
			err = BCME_RANGE;
			break;
		}
		wnm_cfg->timbc_offset = wl_timbc_offset->offset;
		wnm_cfg->timbc_rate = wl_timbc_offset->rate_override;
		wnm_cfg->timbc_tsf = wl_timbc_offset->tsf_present;
		wnm_cfg->timbc_fix_intv = wl_timbc_offset->fix_intv;

		/* enable/disable TIM ucode interrupt */
		wlc_wnm_timbc_switch(wlc, bsscfg);

		break;
	}
	case IOV_GVAL(IOV_WNM_BSSTRANS_REQ_PARAM): {
		wl_bsstrans_req_t *bss_trans_req_param = (wl_bsstrans_req_t *)arg;

		if (len < (int)(sizeof(*bss_trans_req_param))) {
			return BCME_BUFTOOSHORT;
		}

		memcpy(bss_trans_req_param, &wnm_cfg->bsstrans_req_param,
			sizeof(*bss_trans_req_param));
		break;
	}
	case IOV_SVAL(IOV_WNM_BSSTRANS_REQ_PARAM): {
		wl_bsstrans_req_t *wl_bsstrans_req = (wl_bsstrans_req_t *)arg;

		if (!WNM_BSSTRANS_ENABLED(wnm_cfg->cap))
			return BCME_UNSUPPORTED;
		if (wl_bsstrans_req) {
			memcpy(&wnm_cfg->bsstrans_req_param, wl_bsstrans_req,
				sizeof(wnm_cfg->bsstrans_req_param));
			wnm_cfg->bsstrans_req_param_configured = TRUE;
		}
		break;
	}
	case IOV_SVAL(IOV_WNM_BSSTRANS_REQ): {
		wl_bsstrans_req_t *wl_bsstrans_req = (wl_bsstrans_req_t *)arg;
		wnm_bsstrans_req_info_t *bsstrans = &wnm_cfg->bsstrans_req_info;
		uint32 tsf_l, tsf_h;
		uint32 bsstrans_sta_num = 0;
		uint8 retry_delay = 0;
		uint8 reason = 0;

		if (!WNM_BSSTRANS_ENABLED(wnm_cfg->cap))
			return BCME_UNSUPPORTED;

		/* Return if BSS transition transaction is in progress */
		if (bsstrans->timer_state != BSSTRANS_INIT ||
			wnm_cfg->disassoc_timer_state != BSSTRANS_INIT)
			return BCME_EPERM;
		/* take cubby bsstrans_req_param in case no arg passed */
		if (len == 0) {
			if (!wnm_cfg->bsstrans_req_param_configured) {
				WL_WNM(("BSS transition request parameters not configured\n"));
				return BCME_BADARG;
			}
			wl_bsstrans_req = &wnm_cfg->bsstrans_req_param;
			WL_WNM(("Using pre configured BSS transition request parameters\n"));
		}
		/* For BSS termination request, only tsf and duration are mandatory */
		if (wl_bsstrans_req->tbtt == 0) {
			bsstrans->tsf_l = 0;
			bsstrans->tsf_h = 0;
		} else {
			/* Translate tbtt time into tsf counter in unit of us */
			wlc_read_tsf(wlc, &tsf_l, &tsf_h);
			bsstrans->tsf_l = tsf_l + wl_bsstrans_req->tbtt *
				(bsscfg->current_bss->beacon_period * DOT11_TU_TO_US);
			bsstrans->tsf_h = tsf_h;

			if (bsstrans->tsf_l < tsf_l)
				bsstrans->tsf_h++; /* carry from addition */
		}

		bsstrans->disassoc_tmr = wl_bsstrans_req->tbtt;
		bsstrans->dur = wl_bsstrans_req->dur;
		bsstrans->reqmode = wl_bsstrans_req->reqmode;

		bsstrans->timer_state = BSSTRANS_INIT;
		bsstrans->status = 1 << DOT11_BSSTRANS_RESP_STATUS_ACCEPT;
		bsstrans->delay = 0;

		if (!ETHER_ISNULLADDR(&wl_bsstrans_req->sta_mac)) {
			struct scb *scb;
			wnm_scb_cubby_t *wnm_scb;

			if (bsstrans->reqmode & DOT11_BSSTRANS_REQMODE_BSS_TERM_INCL) {
				WL_ERROR(("wl%d: %s: BSS termination not allowed "
					"when STA mac is provided\n",
					wlc->pub->unit, __FUNCTION__));
				return BCME_EPERM;
			}

			if (!(scb = wlc_scbfind(wlc, bsscfg, &wl_bsstrans_req->sta_mac)) ||
				(!SCB_ASSOCIATED(scb))) {
				return BCME_NOTASSOCIATED;
			}

			wnm_scb = SCB_WNM_CUBBY(wnm, scb);
			if (!SCB_BSSTRANS(wnm_scb->cap)) {
#ifdef BCMDBG
				char eabuf[ETHER_ADDR_STR_LEN];
				WL_ERROR(("wl%d: %s: STA %s is not  BSS transition capable\n",
					wlc->pub->unit, __FUNCTION__,
					bcm_ether_ntoa(&scb->ea, eabuf)));
#endif // endif
				return BCME_EPERM;
			}

			if (wl_bsstrans_req->token) {
				wnm_scb->bsstrans_token = wl_bsstrans_req->token;
			} else {
				WLC_WNM_UPDATE_TOKEN(wnm->req_token);
				wnm_scb->bsstrans_token = wnm->req_token;
			}
#if defined(WL_MBO) && !defined(WL_MBO_DISABLED) && defined(MBO_AP)
			retry_delay = wl_bsstrans_req->retry_delay;
			reason = wl_bsstrans_req->reason;
#endif /* WL_MBO && WL_MBO_DISABLED && MBO_AP */
			/* Per bsstrans-capable STA, send unicast req frame */
			wlc_wnm_send_bsstrans_request(wnm, bsscfg, scb, wnm_scb->bsstrans_token,
				bsstrans->reqmode, retry_delay, reason, FALSE);
			if (bsstrans->reqmode & DOT11_BSSTRANS_REQMODE_DISASSOC_IMMINENT) {
				uint msec;

				bcopy(&wl_bsstrans_req->sta_mac, &wnm_cfg->sta_mac, ETHER_ADDR_LEN);
				msec = bsstrans->disassoc_tmr * bsscfg->current_bss->beacon_period;

				/* Schedule the timer to wait for the resp frame */
				wl_add_timer(wlc->wl, wnm_cfg->disassoc_timer, msec, 0);

				wnm_cfg->disassoc_timer_state = BSSTRANS_WAIT_FOR_DISASSOC;
			}
			return err;
		}
		/* transmit BSS Transition Request to each STAs */
		if (wl_bsstrans_req->unicast) {
			struct scb *scb;
			struct scb_iter scbiter;
			FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
				wnm_scb_cubby_t *wnm_scb;
				if (!SCB_ASSOCIATED(scb))
					continue;

				wnm_scb = SCB_WNM_CUBBY(wnm, scb);
				if (!SCB_BSSTRANS(wnm_scb->cap))
					continue;

				WLC_WNM_UPDATE_TOKEN(wnm->req_token);
				wnm_scb->bsstrans_token = wnm->req_token;

				bsstrans_sta_num++;
#if defined(WL_MBO) && !defined(WL_MBO_DISABLED) && defined(MBO_AP)
				retry_delay = wl_bsstrans_req->retry_delay;
				reason = wl_bsstrans_req->reason;
#endif /* WL_MBO && !WL_MBO_DISABLED && MBO_AP */
				/* Per bsstrans-capable STA, send unicast req frame */
				wlc_wnm_send_bsstrans_request(wnm, bsscfg, scb, wnm->req_token,
					bsstrans->reqmode, retry_delay, reason, FALSE);
			}
		}
		else {
			/* transmit BSS Transition Request broadcast */
			WLC_WNM_UPDATE_TOKEN(wnm->req_token);
			bsstrans_sta_num++;
#if defined(WL_MBO) && !defined(WL_MBO_DISABLED) && defined(MBO_AP)
			retry_delay = wl_bsstrans_req->retry_delay;
			reason = wl_bsstrans_req->reason;
#endif /* WL_MBO && !WL_MBO_DISABLED && MBO_AP */
			wlc_wnm_send_bsstrans_request(wnm, bsscfg, NULL, wnm->req_token,
				bsstrans->reqmode, retry_delay, reason, FALSE);
		}

		if (bsstrans_sta_num > 0) {
			uint msec;
			uint msec_to_bss_down;
			uint bcn_period;
			uint dtim_period;

			msec = wl_bsstrans_req->tbtt * bsscfg->current_bss->beacon_period;
			bcn_period = bsscfg->current_bss->beacon_period;
			dtim_period = bsscfg->current_bss->dtim_period;
			/* For BSS termination bit, allow disassoc frame to be sent out before
			 * expiration of TSF timer, as per: 11.24.7.3
			 * BSS Termination TSF field indicates the value of the TSF timer when BSS
			 * termination will occur in the future. A BSS Termination TSF field value
			 * of 0 indicates that termination of the BSS will occur imminently.
			 * Prior to termination of the BSS, all associated STAs are disassociated
			 * by the AP.
			 *
			 * Set minimum threshold of 2 DTIM cycle to let the BSS down
			 */
			msec_to_bss_down = bcn_period * dtim_period * 2;
			if (msec <= msec_to_bss_down) {
				msec_to_bss_down = msec_to_bss_down / 2;
				msec = msec_to_bss_down;
				wnm_cfg->msec_to_bss_down = msec_to_bss_down;
			} else {
				msec -= msec_to_bss_down;
				wnm_cfg->msec_to_bss_down = msec_to_bss_down;
			}
			wl_add_timer(wlc->wl, bsstrans->timer, msec, 0);

			bsstrans->timer_state = BSSTRANS_WAIT_FOR_BSS_TERM;
		} else {
			WL_ERROR(("wl%d: %s: no BSS transition capable STA associated\n",
				wlc->pub->unit, __FUNCTION__));
			/* Non-STA is bsstrans-capable, so not to terminate BSS */
			return BCME_EPERM;
		}

		break;
	}
	case IOV_GVAL(IOV_WNM_PARP_DISCARD): {
		*ret_int_ptr = (int32)wnm->parp_discard;
		break;
	}
	case IOV_SVAL(IOV_WNM_PARP_DISCARD): {
		wnm->parp_discard = int_val? TRUE: FALSE;
		break;
	}
	case IOV_GVAL(IOV_WNM_PARP_ALLNODE): {
		*ret_int_ptr = (int32)wnm->parp_allnode;
		break;
	}
	case IOV_SVAL(IOV_WNM_PARP_ALLNODE): {
		wnm->parp_allnode = int_val? TRUE: FALSE;
		break;
	}
	case IOV_GVAL(IOV_WNM_NO_BTQ_RESP): {
		*ret_int_ptr = (wnm_cfg->flags & WNM_BTQ_RESP_DISABLED);
		break;
	}
	case IOV_SVAL(IOV_WNM_NO_BTQ_RESP): {
		if (int_val) {
			wnm_cfg->flags |=  WNM_BTQ_RESP_DISABLED;
		} else {
			wnm_cfg->flags &= !WNM_BTQ_RESP_DISABLED;
		}
		break;
	}
#endif /* WLWNM_AP */
#ifdef STA
	case IOV_SVAL(IOV_WNM_TIMBC_SET): {
		wl_timbc_set_t *req = (wl_timbc_set_t*) arg;
		wl_timbc_status_t *stat = &wnm_cfg->timbc_stat;

		int next_state;

		if (!WNM_TIMBC_ENABLED(wnm_cfg->cap))
			return BCME_UNSUPPORTED;

		memcpy(&wnm_cfg->timbc_set, req, sizeof(wnm_cfg->timbc_set));
		next_state = wlc_wnm_timbc_state_update(wnm, bsscfg, wnm_cfg);

		WL_WNM(("TIMBC set  %x: %x->%x\n", req->interval, stat->status_sta, next_state));

		/* send TIM Broadcast request if already associated */
		if (next_state != stat->status_sta) {

			if (next_state == WL_TIMBC_STATUS_ENABLE) {
				/* reset AP's status before sending new request */
				err = wlc_wnm_timbc_req_frame_send(wnm, bsscfg, req->interval);

			} else if (stat->status_sta == WL_TIMBC_STATUS_ENABLE)  {
				err = wlc_wnm_timbc_req_frame_send(wnm, bsscfg, 0);

			} else
				/* Change state only if AP is informed */
				next_state = stat->status_sta;
		}
		break;
	}
	case IOV_GVAL(IOV_WNM_TIMBC_STATUS):
		memcpy(arg, &wnm_cfg->timbc_stat, sizeof(wnm_cfg->timbc_stat));
		break;

	case IOV_SVAL(IOV_WNM_TFS_SET): {
		err = wlc_wnm_tfs_set(bsscfg, wnm_cfg, (wl_tfs_set_t *)arg);
		break;
	}
	case IOV_SVAL(IOV_WNM_TFS_TERM):
		err = wlc_wnm_tfs_term(bsscfg, wnm_cfg, (wl_tfs_term_t*) arg);
		break;
	case IOV_SVAL(IOV_WNM_DMS_SET):
		err = wlc_wnm_dms_set(bsscfg, wnm_cfg, (wl_dms_set_t *)arg);
		break;

	case IOV_SVAL(IOV_WNM_DMS_TERM):
		err = wlc_wnm_dms_term(bsscfg, wnm_cfg, (wl_dms_term_t*) arg);
		break;

	case IOV_SVAL(IOV_WNM_SERVICE_TERM): {
		wl_service_term_t *term = (wl_service_term_t *) arg;

		arg = (void*) &term->u;
		len -= sizeof(term->service);

		if (term->service == WNM_SERVICE_DMS)
			err = wlc_wnm_dms_term(bsscfg, wnm_cfg, (wl_dms_term_t*) arg);
		else
			err = BCME_BADARG;
		break;
	}
	case IOV_GVAL(IOV_WNM_SLEEP_INTV): {
		err = wlc_wnm_get_sleep_intv(wlc, bsscfg, wnm, wnm_cfg, ret_int_ptr);
		break;
	}
	case IOV_SVAL(IOV_WNM_SLEEP_INTV): {
		err = wlc_wnm_set_sleep_intv(wlc, bsscfg, wnm, wnm_cfg, int_val);
		break;
	}
	case IOV_GVAL(IOV_WNM_SLEEP_MODE): {
		err = wlc_wnm_get_sleep_mode(wlc, bsscfg, wnm, wnm_cfg, ret_int_ptr);
		break;
	}
	case IOV_SVAL(IOV_WNM_SLEEP_MODE): {
		err = wlc_wnm_set_sleep_mode(wlc, bsscfg, wnm, wnm_cfg, int_val);
		break;
	}
	case IOV_SVAL(IOV_WNM_BSSTRANS_QUERY):	{
		int ver;
		wl_bsstrans_query_t *btm_query = arg;

		if (!WNM_BSSTRANS_ENABLED(wnm_cfg->cap))
			return BCME_UNSUPPORTED;

		if (!bsscfg->associated)
			return BCME_NOTASSOCIATED;

		ver = btm_query->version;
		if (ver != WL_BSSTRANS_QUERY_VERSION_1) {
			WL_ERROR(("wl%d: %s: IOVAR structure ver mismatch, ver = %d"
					"expected version = %d\n", wlc->pub->unit, __FUNCTION__,
					btm_query->version, WL_BSSTRANS_QUERY_VERSION_1));
			return BCME_ERROR;
		}

		wnm_cfg->btq_reason = btm_query->reason;

		if (btm_query->ssid.SSID_len != 0) {
			wlc_ssid_t ssid;
			memcpy(&ssid, &btm_query->ssid, sizeof(wlc_ssid_t));

			wlc_scan_request(wlc, DOT11_BSSTYPE_INFRASTRUCTURE, &ether_bcast, 1, &ssid,
				DOT11_SCANTYPE_ACTIVE, -1, -1, -1, -1, NULL, 0, FALSE,
				wlc_wnm_bsstrans_query_scancb, wlc);
		}
		else {
			wlc_wnm_bsstrans_query_send(wnm, bsscfg, NULL);
		}
		break;
	}
	case IOV_GVAL(IOV_WNM_BSSTRANS_RESP):	{
		*ret_int_ptr = wnm->bsstrans_policy;
		break;
	}
	case IOV_SVAL(IOV_WNM_BSSTRANS_RESP):	{
		if (int_val >= WL_BSSTRANS_POLICY_MAX) {
			return BCME_RANGE;
		}

		if (WBTEXT_ENAB(wlc->pub)) {
			if (int_val == WL_BSSTRANS_POLICY_PRODUCT_WBTEXT) {
				wlc->pub->_bsstrans_ext_active = TRUE;
				/* reset wbtext counters */
				wlc_wnm_bssload_calc_reset(bsscfg);
			}
			else {
				wlc->pub->_bsstrans_ext_active = FALSE;
			}
		} else {
			/* wbtext policy is not supported on non-wbext fw */
			if (int_val == WL_BSSTRANS_POLICY_PRODUCT_WBTEXT) {
				return BCME_UNSUPPORTED;
			}
		}

		wnm->bsstrans_policy = (uint8)int_val;
		break;
	}

#ifdef KEEP_ALIVE
	case IOV_SVAL(IOV_WNM_KEEPALIVES_MAX_IDLE): {
		keepalives_max_idle_t *ka = (keepalives_max_idle_t *)arg;
		int status;
		if (!KEEP_ALIVE_ENAB(wlc->pub) ||
				!WNM_MAXIDLE_ENABLED(wnm_cfg->cap)) {
			return BCME_UNSUPPORTED;
		}
		wnm_cfg->keepalive_count = ka->keepalive_count;
		wnm_cfg->mkeepalive_index = ka->mkeepalive_index;
		wnm_cfg->ka_max_interval = ka->max_interval;

		status = wlc_wnm_set_keepalive_max_idle(wlc, bsscfg, wnm_cfg);

		if (BCME_OK != status)
			return status;
		break;
	}
	case IOV_GVAL(IOV_WNM_KEEPALIVES_MAX_IDLE): {
		keepalives_max_idle_t *ka = (keepalives_max_idle_t *)arg;
		if (!KEEP_ALIVE_ENAB(wlc->pub)) {
			return BCME_UNSUPPORTED;
		}
		if (p_len < (int)(sizeof(keepalives_max_idle_t)))
			return BCME_BUFTOOSHORT;

		ka->keepalive_count = wnm_cfg->keepalive_count;
		ka->mkeepalive_index = wnm_cfg->mkeepalive_index;
		ka->max_interval = wnm_cfg->ka_max_interval;

		break;
	}
#endif /* KEEP_ALIVE */
	case IOV_GVAL(IOV_WNM_PM_IGNORE_BCMC):
		if (D11REV_LT(wlc->pub->corerev, 40))
			return BCME_UNSUPPORTED;
		*ret_int_ptr = wnm_cfg->pm_ignore_bcmc;
		break;

	case IOV_SVAL(IOV_WNM_PM_IGNORE_BCMC):
		if (D11REV_LT(wlc->pub->corerev, 40))
			return BCME_UNSUPPORTED;
		wnm_cfg->pm_ignore_bcmc = (uint32)int_val;

		/* The conditions will be false if no clock */
		if (wlc->pub->hw_up)
			wlc_wnm_pm_ignore_bcmc_upd(bsscfg, wnm_cfg);
		break;

	case IOV_SVAL(IOV_WNM_DMS_DEPENDENCY): {
		/* Only Proxy-arp is allowed for DMS dependency now */
		wnm->cmn->dms_dependency = int_val & DMS_DEP_PROXY_ARP;
		break;
	}
	case IOV_GVAL(IOV_WNM_DMS_DEPENDENCY):
		*ret_int_ptr = wnm->cmn->dms_dependency;
		break;
#endif /* STA */
	case IOV_SVAL(IOV_WNM_BTQ_NBR_ADD):
	{
		nbr_rpt_elem_t btq_nbr_elem;
		int ver;
		memcpy(&btq_nbr_elem, arg, sizeof(nbr_rpt_elem_t));

		ver = btq_nbr_elem.version;
		if (ver != WL_RRM_NBR_RPT_VER) {
			WL_ERROR(("wl%d: %s: IOVAR structure ver mismatch, ver = %d"
				"expected version = %d\n", wlc->pub->unit, __FUNCTION__,
				btq_nbr_elem.version, WL_RRM_NBR_RPT_VER));
			return BCME_ERROR;
		}
		err = wlc_wnm_add_btq_nbr(wnm, bsscfg, &btq_nbr_elem);
		break;
	}
	case IOV_SVAL(IOV_WNM_BTQ_NBR_DEL):
	{
		struct ether_addr bssid;
		memcpy(&bssid, arg, ETHER_ADDR_LEN);
		if (ETHER_ISNULLADDR(&bssid)) {
			err = wlc_wnm_del_all_btq_nbr(wnm, bsscfg);
		} else {
			err = wlc_wnm_del_btq_nbr(wnm, bsscfg, &bssid);
		}
		break;
	}
	case IOV_GVAL(IOV_WNM_BTQ_NBR_LIST):
	{
		wl_btq_nbr_list_t * btq_nbr_list = (wl_btq_nbr_list_t *)arg;
		err = wlc_wnm_get_btq_nbr_list(wnm, bsscfg, btq_nbr_list, len);
		break;
	}
	case IOV_SVAL(IOV_WNM_TCLAS_ADD): {
		uint8 *ptr = (uint8 *)arg;
		wnm_tclas_t *tclas;
		if (len < DOT11_TCLAS_FC_MIN_LEN)
			return BCME_BADLEN;

		tclas = (wnm_tclas_t *)MALLOC(wlc->osh, TCLAS_ELEM_FIXED_SIZE + len - 1);
		if (tclas == NULL) {
			WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			return BCME_NOMEM;
		}

		tclas->next = NULL;
		/* Get the user priority */
		tclas->user_priority = *ptr++;
		memcpy(&tclas->fc, ptr, len - 1);
		tclas->fc_len = len - 1;

		WNM_ELEMENT_LIST_ADD(wnm->cmn->tclas_head, tclas);
		wnm->cmn->tclas_cnt++;
		break;
	}
#ifdef BCMDBG
	case IOV_SVAL(IOV_WNM_TCLAS_DEL): {
		uint8 *ptr = (uint8 *)arg;
		uint32 idx, cnt;
		wlc_wnm_cmn_info_t *wnm_cmn = wnm->cmn;
		if (len < 1)
			return BCME_BADLEN;

		idx = (ptr[0] >= 1) ? ptr[1] : 0;
		cnt = (ptr[0] >= 2) ? ptr[2] : wnm_cmn->tclas_cnt - idx;

		if (wnm_cmn->tclas_cnt <= idx || wnm_cmn->tclas_cnt < idx + cnt)
			return BCME_RANGE;

		wnm_cmn->tclas_cnt -= cnt;
		return wlc_wnm_tclas_delete(wlc, &wnm_cmn->tclas_head, idx, cnt);
	}
	case IOV_GVAL(IOV_WNM_TCLAS_LIST): {
		wl_tclas_list_t *tclas_list = (wl_tclas_list_t *)arg;
		wl_tclas_t *wl_tclas;
		uint8 *ptr;
		wnm_tclas_t *tclas;
		int totlen;
		wlc_wnm_cmn_info_t *wnm_cmn = wlc->wnm_info->cmn;

		if (tclas_list->num == 0)
			break;

		/* Check the total length to be returned */
		totlen = sizeof(tclas_list->num);
		for (tclas = wnm_cmn->tclas_head; tclas; tclas = tclas->next)
			totlen += WL_TCLAS_FIXED_SIZE + tclas->fc_len;

		if (totlen > (int)len)
			return BCME_BUFTOOSHORT;

		tclas_list->num = wnm->cmn->tclas_cnt;

		ptr = (uint8 *)&tclas_list->tclas[0];

		for (tclas = wnm->cmn->tclas_head; tclas; tclas = tclas->next) {
			wl_tclas = (wl_tclas_t *)ptr;

			wl_tclas->user_priority = tclas->user_priority;
			wl_tclas->fc_len = tclas->fc_len;
			memcpy(&wl_tclas->fc, &tclas->fc, tclas->fc_len);

			ptr += WL_TCLAS_FIXED_SIZE + tclas->fc_len;
		}
		break;
	}
#endif /* BCMDBG */
	case IOV_GVAL(IOV_WNM_DMS_STATUS):
		err = wlc_wnm_dms_status(wnm_cfg, (wl_dms_status_t *)arg, len);
		break;
	case IOV_GVAL(IOV_WNM_TFS_STATUS):
		if (!WNM_TFS_ENABLED(wnm_cfg->cap))
			return BCME_UNSUPPORTED;
		err = wlc_wnm_tfs_status(wnm, bsscfg, (wl_tfs_status_t *)arg, len);
		break;
#ifdef STA
	case IOV_SVAL(IOV_WNM_BSSTRANS_RSSI_RATE_MAP): {
		uint32 map_len;
		wl_bsstrans_rssi_rate_map_t *map = (wl_bsstrans_rssi_rate_map_t *)arg;

		if (map->ver != WL_BSSTRANS_RSSI_RATE_MAP_VERSION) {
			return BCME_VERSION;
		}

		map_len = MIN(map->len, sizeof(*map));
		memcpy(bsi->rssi_rate_map, map, map_len);
		break;
	}
	case IOV_GVAL(IOV_WNM_BSSTRANS_RSSI_RATE_MAP): {
		wl_bsstrans_rssi_rate_map_t *map = (wl_bsstrans_rssi_rate_map_t *)arg;

		if (p_len < (int)(sizeof(*map))) {
			return BCME_BUFTOOSHORT;
		}

		memcpy(map, bsi->rssi_rate_map, sizeof(*map));
		break;
	}
	case IOV_GVAL(IOV_WNM_BSSTRANS_ROAMTHROTTLE): {
		wl_bsstrans_roamthrottle_t *throttle = (wl_bsstrans_roamthrottle_t *)arg;
		wnm_bsstrans_roam_throttle_t *info = bsi->throttle;
		throttle->ver = WL_BSSTRANS_ROAMTHROTTLE_VERSION;
		throttle->period = info->period;
		throttle->scans_allowed = info->scans_allowed;
		break;
	}
	case IOV_SVAL(IOV_WNM_BSSTRANS_ROAMTHROTTLE): {
		wl_bsstrans_roamthrottle_t *throttle = (wl_bsstrans_roamthrottle_t *)arg;
		wnm_bsstrans_roam_throttle_t *info = bsi->throttle;
		if (throttle->ver != WL_BSSTRANS_ROAMTHROTTLE_VERSION) {
			return BCME_VERSION;
		}
		if (throttle->period && !throttle->scans_allowed) {
			return BCME_BADARG;
		}
		info->period = throttle->period;
		info->scans_allowed = throttle->scans_allowed;
		info->period_secs = 0;
		info->scans_done = 0;
		break;
	}
	case IOV_GVAL(IOV_WNM_SCOREDELTA): {
		*ret_int_ptr = bsi->scoredelta;
		break;
	}
	case IOV_SVAL(IOV_WNM_SCOREDELTA): {
		bsi->scoredelta = (uint32)int_val;
		break;
	}
	case IOV_GVAL(IOV_WNM_BTM_RSSI_THRESH): {
		*ret_int_ptr = (int32)bsi->btm_rssi_thresh;
		break;
	}
	case IOV_SVAL(IOV_WNM_BTM_RSSI_THRESH): {
		bsi->btm_rssi_thresh = (int8)int_val;
		break;
	}

#ifdef WNM_BSSTRANS_EXT
	case IOV_GVAL(IOV_WNM_BSS_SELECT_TABLE): {
		err = wlc_wnm_get_bss_select_table((wnm_bss_select_factor_cfg_t *)arg,
			params, len, wnm_cfg);
		break;
	}
	case IOV_SVAL(IOV_WNM_BSS_SELECT_TABLE): {
		err = wlc_wnm_set_bss_select_table((wnm_bss_select_factor_cfg_t *)arg,
			params, wnm_cfg);
		break;
	}

	case IOV_GVAL(IOV_WNM_BSS_SELECT_WEIGHT): {
		wnm_bss_select_weight_cfg_t *bwcfg = (wnm_bss_select_weight_cfg_t *)arg;

		if (len < sizeof(*bwcfg)) {
			return BCME_BUFTOOSHORT;
		}

		memcpy(bwcfg, params, sizeof(wnm_bss_select_weight_cfg_t));
		if (bwcfg->type != WNM_BSS_SELECT_TYPE_RSSI &&
			bwcfg->type != WNM_BSS_SELECT_TYPE_CU) {
			return BCME_RANGE;
		}
		if (bwcfg->band != WLC_BAND_2G && bwcfg->band != WLC_BAND_5G &&
		    bwcfg->band != WLC_BAND_6G) {
			return BCME_RANGE;
		}

		bwcfg->version = WNM_BSS_SELECT_WEIGHT_VERSION;
		if (bwcfg->type == WNM_BSS_SELECT_TYPE_RSSI) {
			bwcfg->weight = wnm_cfg->bss_select_table[bwcfg->band-1]->rssi_weightage;
		}
		else {
			bwcfg->weight = wnm_cfg->bss_select_table[bwcfg->band-1]->cu_weightage;
		}
		break;
	}
	case IOV_SVAL(IOV_WNM_BSS_SELECT_WEIGHT): {
		wnm_bss_select_weight_cfg_t *bwcfg = (wnm_bss_select_weight_cfg_t *)arg;

		if (bwcfg->type != WNM_BSS_SELECT_TYPE_RSSI &&
			bwcfg->type != WNM_BSS_SELECT_TYPE_CU) {
			return BCME_RANGE;
		}
		if (bwcfg->band <= WLC_BAND_AUTO || bwcfg->band > WLC_BAND_6G) {
			return BCME_RANGE;
		}
		/* TODO:6GHZ: extend code below with 6GHZ */
		if (bwcfg->type == WNM_BSS_SELECT_TYPE_RSSI) {
			ptr_5g = &wnm_cfg->bss_select_table[WLC_BAND_5G-1]->rssi_weightage;
			ptr_2g = &wnm_cfg->bss_select_table[WLC_BAND_2G-1]->rssi_weightage;
		}
		else {
			ptr_5g = &wnm_cfg->bss_select_table[WLC_BAND_5G-1]->cu_weightage;
			ptr_2g = &wnm_cfg->bss_select_table[WLC_BAND_2G-1]->cu_weightage;
		}
		wnm_bsstrans_set_values(bwcfg->band, ptr_5g, ptr_2g, &bwcfg->weight,
				sizeof(bwcfg->weight));
		break;
	}
#endif /* WNM_BSSTRANS_EXT */
#endif /* STA */

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

#ifdef STA
/* This function will do WBTEXT related STA deinitialization on cfg */
static void
wnm_wbtext_bsscfg_bsi_deinit(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc, *wlc_tmr;
	wnm_bsscfg_cubby_t *wnm_cfg;
	wnm_bsstrans_sta_info_t *bsi;
	wnm_cfg = WNM_BSSCFG_CUBBY(cfg->wlc->wnm_info, cfg);
	bsi = wnm_cfg->bsstrans_stainfo;
	wlc_tmr = wlc;

#ifdef WLRSDB
	if (RSDB_ENAB(wlc->pub)) {
		wlc_tmr = WLC_RSDB_GET_PRIMARY_WLC(wlc);
	}
#endif /* WLRSDB */

	/*
	 * just free select tables and factor tables inside them
	 * Timers if active we still want to keep them running and
	 * handle. Memory leak is taken care by ensuring that
	 * before timer add.. delete is performed
	 */
	 if (bsi->nbrlist_valid_timer) {
		wl_del_timer(wlc_tmr->wl, bsi->nbrlist_valid_timer);
		bsi->nbrlist_valtimer_active = FALSE;
	 }
	 if (bsi->disassoc_imnt_timer) {
		wl_del_timer(wlc_tmr->wl, bsi->disassoc_imnt_timer);
		bsi->disassoc_imnttmr_active = FALSE;
	 }
	 if (wnm_cfg->bss_select_table[0]) {
		if (wnm_cfg->bss_select_table[0]->rssi_table) {
			MFREE(wlc->osh, wnm_cfg->bss_select_table[0]->rssi_table,
			(sizeof(wnm_bss_select_factor_t) * BSS_MAXTABLE_SIZE *
			BSS_TABLE_NUMBER * MAXBANDS));
		}
		MFREE(wlc->osh, wnm_cfg->bss_select_table[0],
			(sizeof(wnm_bss_select_table_t) * MAXBANDS));
		wnm_cfg->bss_select_table[0] = NULL;
	}
}

/* This function will do WBTEXT related STA initialization on cfg */
static int
BCMRAMFN(wnm_wbtext_bsscfg_bsi_init)(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc, *wlc_tmr;
	wlc_wnm_info_t *wnm = wlc->wnm_info;
	wnm_bsscfg_cubby_t *wnm_cfg;
	wnm_bsstrans_sta_info_t *bsi;
	uintptr *ptr;
	uint32 i;
	wlc_tmr = wlc;

#ifdef WLRSDB
	if (RSDB_ENAB(wlc->pub)) {
		wlc_tmr = WLC_RSDB_GET_PRIMARY_WLC(wlc);
	}
#endif /* WLRSDB */

	wnm_cfg = WNM_BSSCFG_CUBBY(wnm, cfg);
	bsi = wnm_cfg->bsstrans_stainfo;
	bsi->max_nbr_count = BSSTRANS_MAX_NEIGHBORS;

	ptr = MALLOCZ(wlc->osh, (sizeof(wnm_bss_select_table_t) * MAXBANDS));
	if (ptr == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return BCME_NOMEM;
	}
	for (i = 0; i < MAXBANDS; i++) {
		wnm_cfg->bss_select_table[i] = (wnm_bss_select_table_t *)
			((uintptr)ptr + (sizeof(wnm_bss_select_table_t) * i));
	}

	ptr = MALLOCZ(wlc->osh, (sizeof(wnm_bss_select_factor_t) * BSS_MAXTABLE_SIZE *
		BSS_TABLE_NUMBER * MAXBANDS));
	if (ptr == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return BCME_NOMEM;
	}
	for (i = 0; i < MAXBANDS; i++) {
		wnm_cfg->bss_select_table[i]->rssi_table =
		(wnm_bss_select_factor_t *)((uintptr)ptr + (sizeof(wnm_bss_select_factor_t) *
				BSS_MAXTABLE_SIZE * BSS_TABLE_NUMBER * i));
		wnm_cfg->bss_select_table[i]->cu_table =
		(wnm_bss_select_factor_t *)((uintptr)ptr + (sizeof(wnm_bss_select_factor_t) *
				BSS_MAXTABLE_SIZE * BSS_TABLE_NUMBER * i) +
			(sizeof(wnm_bss_select_factor_t) * BSS_MAXTABLE_SIZE));
	}
	for (i = 0; i < MAXBANDS; i++) {
		wnm_cfg->bss_select_table[i]->rssi_weightage = BSSTRANS_DEF_RSSIWEIGHT;
		wnm_cfg->bss_select_table[i]->cu_weightage = BSSTRANS_DEF_CUWEIGHT;
		wnm_cfg->bss_select_table[i]->rssi_table_size =
			sizeof(rssi_table) / sizeof(wnm_bss_select_factor_t);
		memcpy(wnm_cfg->bss_select_table[i]->rssi_table, rssi_table, sizeof(rssi_table));
		wnm_cfg->bss_select_table[i]->cu_table_size =
			sizeof(cu_table) / sizeof(wnm_bss_select_factor_t);
		memcpy(wnm_cfg->bss_select_table[i]->cu_table, cu_table, sizeof(cu_table));
	}

	/*
	 * allocate space for timers callback context single
	 * context for all 3 timers is enough as they are
	 * linked to same BTM request
	 */
	bsi->tmr_cb = MALLOCZ(wlc->osh, sizeof(timer_cb_ctx_t));
	if (bsi->tmr_cb == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes for timer cb context \n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return BCME_NOMEM;
	}

	/* init bss_resp timer */
	if ((bsi->resp_timer = wl_init_timer(wlc_tmr->wl,
		wlc_wnm_bsstrans_resp_timeout, bsi->tmr_cb, "wnm_bsstrans_resp_timer")) == NULL) {
		WL_ERROR(("wl%d: %s: bsstrans_resp__timer init failed\n",
			wlc_tmr->pub->unit, __FUNCTION__));
		return BCME_NORESOURCE;
	}

	if ((bsi->nbrlist_valid_timer = wl_init_timer(wlc_tmr->wl,
		wlc_wnm_nbrlist_validity_timeout, bsi->tmr_cb,
		"wnm_bsstrans_nbrlist_valid_timer")) == NULL) {
		WL_ERROR(("wl%d: %s: nbrlist_valid_timer init failed\n",
			wlc_tmr->pub->unit, __FUNCTION__));
		return BCME_NORESOURCE;
	}
	if ((bsi->disassoc_imnt_timer = wl_init_timer(wlc_tmr->wl,
		wlc_wnm_disassocimnt_timeout, bsi->tmr_cb,
		"wnm_bsstrans_disassoc_imnt_timer")) == NULL) {
		WL_ERROR(("wl%d: %s: disassoc_imnt_timer init failed\n",
			wlc_tmr->pub->unit, __FUNCTION__));
		return BCME_NORESOURCE;
	}
	return BCME_OK;
}

static void
BCMRAMFN(wlc_wnm_bsscfg_bsi_dealloc)(wlc_wnm_info_t *wnm,
	wnm_bsstrans_sta_info_mem_t *bsi_ptr)
{
	MFREE(wnm->wlc->osh, bsi_ptr, sizeof(*bsi_ptr));
}

/* This function just allocates bsi_mem structure
 * Since struct embedding is present, making it a
 * BCMRAMFN
 */
static int
BCMRAMFN(wlc_wnm_bsscfg_bsi_alloc)(wlc_wnm_info_t *wnm,
wnm_bsstrans_sta_info_mem_t **bsi_ptr)
{
	if ((*bsi_ptr = (wnm_bsstrans_sta_info_mem_t *)MALLOCZ(wnm->wlc->osh,
		sizeof(wnm_bsstrans_sta_info_mem_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem for wnm_bsstrans_sta_info_mem_t, "
			"malloced %d bytes\n", wnm->wlc->pub->unit, __FUNCTION__,
			MALLOCED(wnm->wlc->osh)));
		return BCME_NOMEM;
	}
	return BCME_OK;
}

/* This function will do STA related deinitialization on cfg */
static void
wnm_bsscfg_bsi_deinit(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc, *wlc_tmr;
	wnm_bsscfg_cubby_t *wnm_cfg;
	wnm_bsstrans_sta_info_t *bsi;
	wnm_bsstrans_sta_info_mem_t *bsi_mem;

	wnm_cfg = WNM_BSSCFG_CUBBY(cfg->wlc->wnm_info, cfg);
	bsi = wnm_cfg->bsstrans_stainfo;
	wlc_tmr = wlc;

#ifdef WLRSDB
		if (RSDB_ENAB(wlc->pub)) {
			wlc_tmr = WLC_RSDB_GET_PRIMARY_WLC(wlc);
		}
#endif /* WLRSDB */

	if (bsi) {
		if (bsi->resp_timer) {
			wl_del_timer(wlc_tmr->wl, bsi->resp_timer);
			wl_free_timer(wlc_tmr->wl, bsi->resp_timer);
		}

		if (bsi->tmr_cb) {
			MFREE(wlc->osh, bsi->tmr_cb, sizeof(timer_cb_ctx_t));
		}

		if (WBTEXT_ENAB(wlc->pub) && WNM_BSSCFG_STA(cfg)) {
			wnm_wbtext_bsscfg_bsi_deinit(cfg);
		}

		/* free bsi_mem as members are freed by now */
		bsi_mem = (wnm_bsstrans_sta_info_mem_t *)bsi;
		wlc_wnm_bsscfg_bsi_dealloc(wlc->wnm_info, bsi_mem);
	}
}

static int
BCMRAMFN(wnm_bsscfg_bsi_init)(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_wnm_info_t *wnm = wlc->wnm_info;
	wnm_bsscfg_cubby_t *wnm_cfg;
	wnm_bsstrans_sta_info_mem_t *bsi_mem;
	wnm_bsstrans_sta_info_t *bsi;
	wl_bsstrans_rssi_rate_map_t *rrm;
	wnm_bsstrans_rateidx2rate500k_t *idx2rate;
	int err = BCME_OK;

	/* during clone re-use bsi on old cfg */
	if (BSSCFG_IS_RSDB_CLONE(cfg)) {
		return BCME_OK;
	}

	err = wlc_wnm_bsscfg_bsi_alloc(wnm, &bsi_mem);
	if (err != BCME_OK) {
		return err;
	}

	wnm_cfg = WNM_BSSCFG_CUBBY(wnm, cfg);
	wnm_cfg->bsstrans_stainfo = &bsi_mem->sta_info_mem;
	bsi = wnm_cfg->bsstrans_stainfo;
	bsi->throttle = &bsi_mem->throttle_mem;
	bsi->rssi_rate_map = &bsi_mem->rssi_rate_map_mem;
	bsi->idx2rate = &bsi_mem->idx2rate_mem;

	/* init rssi_rate_map */
	rrm = bsi->rssi_rate_map;
	rrm->ver = WL_BSSTRANS_RSSI_RATE_MAP_VERSION;
	rrm->len = sizeof(*rrm);
	memcpy(rrm->cck, cck, sizeof(cck));
	memcpy(rrm->ofdm, ofdm, sizeof(ofdm));
	memcpy(rrm->phy_n[0], phy_n_1x1, sizeof(phy_n_1x1));
	memcpy(rrm->phy_ac[0], phy_ac_1x1, sizeof(phy_ac_1x1));
	memcpy(rrm->phy_n[1], phy_n_2x2, sizeof(phy_n_2x2));
	memcpy(rrm->phy_ac[1], phy_ac_2x2, sizeof(phy_ac_2x2));

	/* init idx2rate table */
	idx2rate = bsi->idx2rate;
	memcpy(idx2rate->cck, cck_rate, sizeof(cck_rate));
	memcpy(idx2rate->ofdm, ofdm_rate, sizeof(ofdm_rate));
	memcpy(idx2rate->phy_n, phy_n_rate, sizeof(phy_n_rate));
	memcpy(idx2rate->phy_ac, phy_ac_rate, sizeof(phy_ac_rate));

	if (WBTEXT_ENAB(wlc->pub) && WNM_BSSCFG_STA(cfg)) {
		err = wnm_wbtext_bsscfg_bsi_init(cfg);
	}

	return err;
}
#endif /* STA */

#ifdef WLWNM_AP
static arp_table_t* wlc_wnm_init_l2_arp_table(osl_t* osh)
{
	arp_table_t *ptable = init_l2_filter_arp_table(osh);
	ASSERT(ptable);
	return ptable;
}

static void wlc_wnm_deinit_l2_arp_table(osl_t* osh, arp_table_t* ptable)
{
	deinit_l2_filter_arp_table(osh, ptable);
}
#endif /* WLWNM_AP */

/* bsscfg cubby */
static int
wlc_wnm_bsscfg_init(void *context, wlc_bsscfg_t *cfg)
{
	wlc_wnm_info_t* wnm = (wlc_wnm_info_t*)context;
	wlc_info_t *wlc = wnm->wlc;
	wnm_bsscfg_cubby_t* wnm_cfg = WNM_BSSCFG_CUBBY(wnm, cfg);
	uint32 cap = 0;
	int err = BCME_OK;
	/*
	 * init default setup.  Add default capability here
	 */

	cap |= WL_WNM_BSSTRANS;
#ifdef KEEP_ALIVE
	if (KEEP_ALIVE_ENAB(wlc->pub)) {
		cap |= WL_WNM_MAXIDLE;
	}
#endif /* KEEP_ALIVE */
#ifdef WLWNM_BRCM
	if (WLWNM_BRCM_ENAB(wlc->pub)) {
		cap = BRCM_WNM_FEATURE_SET;
	}
#endif /* WLWNM_BRCM */

	if (WBTEXT_ENAB(wlc->pub)) {
		cap |= WL_WNM_WBTEXT;
	}

	/* BSS Max Idle Period default disabled */
	wnm_cfg->bss_max_idle_period = 0;
	wnm_cfg->bss_idle_opt = 0;
	wnm_cfg->chainable = TRUE;
#ifdef STA
	wnm_cfg->sleep_intv = 10;

	memset(&wnm_cfg->timbc_set, 0, sizeof(wl_timbc_set_t));
	memset(&wnm_cfg->timbc_stat, 0, sizeof(wl_timbc_status_t));
	wnm_cfg->timbc_stat.status_ap = WL_TIMBC_STATUS_AP_UNKNOWN;
#endif // endif

#ifdef WLWNM_AP
	/* init TIM Broadcast timer */
	wnm_cfg->timbc_offset = 10;
	wnm_cfg->timbc_tsf = TRUE;
	wnm_cfg->timbc_rate = 0;
	wnm_cfg->timbc_fix_intv = 0;
	wnm_cfg->timbc_ie_update = FALSE;
	bzero(&wnm_cfg->timbc_frame, sizeof(wnm_timbc_fset_t));

	if ((wnm_cfg->bsstrans_req_info.timer = wl_init_timer(wlc->wl, wlc_wnm_bsstrans_req_timer,
	    cfg, "wnm_bsstrans_req_timer")) == NULL) {
		WL_ERROR(("wl%d: %s: wl_init_timer for bsstrans_req_timer failed\n",
			wlc->pub->unit, __FUNCTION__));
		err = BCME_ERROR;
		goto done;
	}
	if ((wnm_cfg->disassoc_timer = wl_init_timer(wlc->wl,
		wlc_wnm_bsstrans_disassoc_timer, cfg, "wnm_bsstrans_disassoc_timer")) == NULL) {
		WL_ERROR(("wl%d: %s: wl_init_timer for bsstrans_disassoc_timer failed\n",
			wlc->pub->unit, __FUNCTION__));
		err = BCME_ERROR;
		goto done;
	}
	wnm_cfg->bsstrans_req_info.timer_state = BSSTRANS_INIT;
	wnm_cfg->disassoc_timer_state = BSSTRANS_INIT;
	wnm_cfg->bsstrans_req_info.validity_intrvl = WNM_BSSTRANS_REQ_VALIDITY_INTERVAL;

	/* Init the arp table handle for proxy arp feature */
	wnm_cfg->phnd_arp_table = wlc_wnm_init_l2_arp_table(wlc->osh);
#endif /* WLWNM_AP */
	if (WLWNM_ENAB(wlc->pub)) {
		/* enable extend capability */
		wlc_wnm_set_cap(wlc, cfg, cap);
	}
	/* Setting defaults for BTM quesry List */
	wnm_cfg->max_btq_nbr_count = BTM_QUERY_NBR_COUNT_MAX;
	wnm_cfg->btq_nbrlist_size = 0;
	wnm_cfg->btq_nbr_list_head = NULL;
#ifdef STA
	if (!BSSCFG_INFRA_STA(cfg)) {
		return BCME_OK;
	}
	if ((err = wnm_bsscfg_bsi_init(cfg)) == BCME_OK) {
		return BCME_OK;
	} else {
		WL_ERROR(("wl%d: %s: cannot do bssstainfo init\n", wlc->pub->unit,
			__FUNCTION__));
		goto done;
	}
#endif /* STA */
done:
#ifdef STA
		wlc_wnm_bsscfg_deinit((void *)wnm, cfg);
#endif // endif
	return err;
}

static void
wlc_wnm_bsscfg_deinit(void *context, wlc_bsscfg_t *cfg)
{
	wlc_wnm_info_t* wnm = (wlc_wnm_info_t*)context;
	wlc_info_t *wlc;
	wnm_bsscfg_cubby_t *wnm_cfg;

	if (wnm && cfg)
		wnm_cfg = WNM_BSSCFG_CUBBY(wnm, cfg);
	else
		return;
	wlc = wnm->wlc;
#ifdef WLWNM_AP
	if (wnm_cfg->bsstrans_req_info.bsstrans_list_len) {
		MFREE(wlc->osh, wnm_cfg->bsstrans_req_info.bsstrans_list,
			wnm_cfg->bsstrans_req_info.bsstrans_list_len);
	}

	if (wnm_cfg->bsstrans_req_info.timer) {
		wl_free_timer(wlc->wl, wnm_cfg->bsstrans_req_info.timer);
		wnm_cfg->bsstrans_req_info.timer = NULL;
	}
	if (wnm_cfg->disassoc_timer) {
		wl_free_timer(wlc->wl, wnm_cfg->disassoc_timer);
		wnm_cfg->disassoc_timer = NULL;
	}
	wlc_wnm_timbc_clrframe(wlc, wnm_cfg);

	/* clear all Proxy ARP cache */
	wlc_wnm_parp_watchdog(wlc, cfg, TRUE, NULL, FALSE);
	/* deinit the arp table handle */
	wlc_wnm_deinit_l2_arp_table(wlc->osh, wnm_cfg->phnd_arp_table);
#endif /* WLWNM_AP */

	wlc_wnm_dms_free(cfg);
#ifdef STA
	wlc_wnm_tfs_free(cfg);
	wnm_bsscfg_bsi_deinit(cfg);
#endif /* STA */
	UNUSED_PARAMETER(wnm_cfg);
	UNUSED_PARAMETER(wlc);
}

void
wlc_wnm_scb_assoc(wlc_info_t *wlc, struct scb *scb)
{
#ifdef STA
	wlc_wnm_info_t* wnm = wlc->wnm_info;
	wlc_bsscfg_t *bsscfg = SCB_BSSCFG(scb);
	wnm_bsscfg_cubby_t *wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);
	wnm_scb_cubby_t *wnm_scb = SCB_WNM_CUBBY(wnm, scb);
	if (BSSCFG_STA(bsscfg) && WNM_TFS_ENABLED(wnm_cfg->cap)) {
		wnm_tfs_fset_t *fset;
		for (fset = wnm_cfg->tfs_fset_head; fset; fset = fset->next) {

			if (fset->status == TFS_STATUS_NOT_ASSOC) {
				if (SCB_TFS(wnm_scb->cap)) {
					fset->status = TFS_STATUS_VALIDATING;
					wnm_cfg->tfs_timer = wlc->pub->now + TFS_DELAY_ASSOC;
				} else
					fset->status = TFS_STATUS_NOT_SUPPORT;
			} else {
				ASSERT(fset->status == TFS_STATUS_DISABLED);
			}
		}
	}
	if (!BSSCFG_STA(bsscfg))
		return;

	if (WNM_TIMBC_ENABLED(wnm_cfg->cap)) {
		if (wnm_cfg->timbc_stat.status_sta != WL_TIMBC_STATUS_DISABLE)
			wnm_cfg->timbc_timer_sta = wlc->pub->now + TIMBC_DELAY_ASSOC + 1;
	}
	/* To account for Proxy-Arp */
	wlc_wnm_pm_ignore_bcmc_upd(bsscfg, wnm_cfg);
#endif /* STA */
}

void
wlc_wnm_scb_cleanup(wlc_info_t *wlc, struct scb *scb)
{
	wlc_wnm_info_t* wnm;
	wlc_bsscfg_t *bsscfg = SCB_BSSCFG(scb);
	if (!bsscfg)
		return;

	wnm = wlc->wnm_info;
	BCM_REFERENCE(wnm);

#ifdef STA
	if (BSSCFG_STA(bsscfg)) {
		wnm_bsscfg_cubby_t *wnm_cfg;

		wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);

		memset(&wnm_cfg->timbc_stat, 0, sizeof(wl_timbc_status_t));
		wnm_cfg->timbc_stat.status_ap = WL_TIMBC_STATUS_AP_UNKNOWN;

		/* Disable all timers */
		wnm_cfg->tfs_timer = 0;
		wnm_cfg->tfs_req_cnt = 0;
		wlc_wnm_tfs_free(bsscfg);
		wnm_cfg->dms_timer = 0;
		wnm_cfg->dms_req_cnt = 0;
		wnm_cfg->sleep_timer = 0;
		wnm_cfg->sleep_req_cnt = 0;
		wnm_cfg->timbc_timer_sta = 0;

		wlc_wnm_pm_ignore_bcmc_upd(bsscfg, wnm_cfg);
	}
#endif /* STA */
#ifdef WLWNM_AP
	if (BSSCFG_AP(bsscfg)) {
		wnm_scb_cubby_t *wnm_scb = SCB_WNM_CUBBY(wlc->wnm_info, scb);
		wnm_scb->rx_tstamp = 0;
		wnm_scb->timbc_status = DOT11_TIMBC_STATUS_RESERVED;
		wnm_scb->timbc_interval = 0;
		wnm_scb->timbc_high_rate = 0;
		wlc_wnm_tfs_req_free(wlc->wnm_info, &wnm_scb->tfs_fset_head, -1);
		if (wnm_scb->timbc_resp.data != NULL) {
			MFREE(wlc->osh, wnm_scb->timbc_resp.data, WLC_WNM_RESP_MAX_BUFSIZE);
			wnm_scb->timbc_resp.len = 0;
			wnm_scb->timbc_resp.data = NULL;
		}

		/* clear Proxy ARP cache of specific Ethernet Address */
		wlc_wnm_parp_watchdog(wlc, bsscfg, FALSE, (uint8 *)&scb->ea, FALSE);
	}
#endif /* WLWNM_AP */
	wlc_wnm_dms_scb_cleanup(wlc, scb);
}

uint32
wlc_wnm_get_cap(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	wlc_wnm_info_t* wnm;
	wnm_bsscfg_cubby_t *wnm_cfg;
	uint32 cap = 0;

	wnm = wlc->wnm_info;
	if (wnm != NULL) {
		wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);

		cap = wnm_cfg->cap;
	}
	return cap;
}

void
wlc_wnm_set_cap(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint32 cap)
{
	wlc_wnm_info_t* wnm;
	wnm_bsscfg_cubby_t *wnm_cfg;

	wnm = wlc->wnm_info;
	if (wnm != NULL) {
		wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);

		wnm_cfg->cap = cap;

#ifdef WLWNM_BRCM
		if (BSSCFG_STA(bsscfg) || !WLWNM_BRCM_ENAB(wlc->pub))
#endif // endif
		{
			/* eixt_cap 11-th bit for FMS service */
			wlc_bsscfg_set_ext_cap(bsscfg, DOT11_EXT_CAP_FMS, WNM_FMS_ENABLED(cap));

			/* ext_cap 16-th bit for TFS service */
			wlc_bsscfg_set_ext_cap(bsscfg, DOT11_EXT_CAP_TFS, WNM_TFS_ENABLED(cap));

			/* ext_cap 17-th bit for WNM-Sleep */
			wlc_bsscfg_set_ext_cap(bsscfg,
				DOT11_EXT_CAP_WNM_SLEEP, WNM_SLEEP_ENABLED(cap));

			/* ext_cap 18-th bit for TIM Broadcast */
			wlc_bsscfg_set_ext_cap(bsscfg, DOT11_EXT_CAP_TIMBC, WNM_TIMBC_ENABLED(cap));

			/* ext_cap 19-th bit for BSS Transition */
			wlc_bsscfg_set_ext_cap(bsscfg, DOT11_EXT_CAP_BSSTRANS_MGMT,
				WNM_BSSTRANS_ENABLED(cap));
			/* ext_cap 26-th bit for DMS service */
			wlc_bsscfg_set_ext_cap(bsscfg, DOT11_EXT_CAP_DMS, WNM_DMS_ENABLED(cap));

			/* ext_cap 46-th bit for WNM-Notification */
			wlc_bsscfg_set_ext_cap(bsscfg,
				DOT11_EXT_CAP_WNM_NOTIF, WNM_NOTIF_ENABLED(cap));
#ifdef WLWNM_AP
			/* ext_cap 12-th bit for proxyarp */
			wlc_bsscfg_set_ext_cap(bsscfg,
				DOT11_EXT_CAP_PROXY_ARP, WNM_PROXYARP_ENABLED(cap));
#endif // endif
		}

#ifdef WLWNM_AP
		if (!WNM_PROXYARP_ENABLED(cap)) {
			/* clear all Proxy ARP cache */
			wlc_wnm_parp_watchdog(wlc, bsscfg, TRUE, NULL, FALSE);
		}

		if (!WNM_BSSTRANS_ENABLED(cap)) {
			wnm_bsstrans_req_info_t *bsstrans = &wnm_cfg->bsstrans_req_info;
			if (bsstrans->timer_state != BSSTRANS_INIT) {
				/* restore state to init value */
				bsstrans->timer_state = BSSTRANS_INIT;

				/* Enable this bsscfg and delete this timer */
				wl_del_timer(wlc->wl, bsstrans->timer);

				if (!bsscfg->enable)
					wlc_bsscfg_enable(wlc, bsscfg);
			}
		}
#endif /* WLWNM_AP */
		/* finall setup beacon and probe response ext cap */
		if (bsscfg->up &&
		    (BSSCFG_AP(bsscfg) || (!bsscfg->BSS && !BSS_TDLS_ENAB(wlc, bsscfg)))) {
			/* update AP or IBSS beacons */
			wlc_bss_update_beacon(wlc, bsscfg);
			/* update AP or IBSS probe responses */
			wlc_bss_update_probe_resp(wlc, bsscfg, TRUE);
		}

		/* Set/Reset the handle_data_pkts */
		if (!WNM_PROXYARP_ENABLED(wnm_cfg->cap)&&
			!WNM_TFS_ENABLED(wnm_cfg->cap) &&
			!WNM_SLEEP_ENABLED(wnm_cfg->cap) &&
			!WNM_DMS_ENABLED(wnm_cfg->cap) &&
			!WNM_FMS_ENABLED(wnm_cfg->cap)) {
			wnm_cfg->chainable = TRUE;
		} else {
			wnm_cfg->chainable = FALSE;
		}
	}
}

uint32
wlc_wnm_get_scbcap(wlc_info_t *wlc, struct scb *scb)
{
	wnm_scb_cubby_t *wnm_scb;
	uint32 cap = 0;

	if (wlc->wnm_info != NULL) {
		wnm_scb = SCB_WNM_CUBBY(wlc->wnm_info, scb);
		cap = wnm_scb->cap;
	}
	return cap;
}

uint32
wlc_wnm_set_scbcap(wlc_info_t *wlc, struct scb *scb, uint32 cap)
{
	wnm_scb_cubby_t *wnm_scb;

	if (cap >= WL_WNM_MAX) {
		WL_ERROR(("WNM Cap out of range (%x)\n", cap));
		ASSERT(cap < WL_WNM_MAX);
	}

	if (wlc->wnm_info != NULL) {
		wnm_scb = SCB_WNM_CUBBY(wlc->wnm_info, scb);
		wnm_scb->cap = cap;
	}

	return BCME_OK;
}

#ifdef BCMDBG
static void
wlc_wnm_bsscfg_dump(void *context, wlc_bsscfg_t *cfg, struct bcmstrbuf *b)
{
	/* TODO */
}
#endif // endif

static int
wlc_wnm_scb_init(void *context, struct scb *scb)
{
	wlc_wnm_info_t *wnm = (wlc_wnm_info_t *)context;
	wnm_scb_cubby_t *wnm_scb = SCB_WNM_CUBBY(wnm, scb);
#ifdef WLWNM_AP
	wnm_scb->rx_tstamp = wnm->timestamp;
	wnm_scb->spp_conflict = FALSE;
#endif /* WLWNM_AP */
	wnm_scb->cap = 0;

	return BCME_OK;
}

static void
wlc_wnm_scb_deinit(void *context, struct scb *scb)
{

}

#ifdef BCMDBG
static const bcm_bit_desc_t scb_capstr[] =
{
	{WL_WNM_BSSTRANS,	"WNM_BSSTRANS"},
	{WL_WNM_PROXYARP,	"WNM_PROXY_ARP"},
	{WL_WNM_MAXIDLE,	"WNM_MAXIDLE"},
	{WL_WNM_TIMBC,		"WNM_TIMBC"},
	{WL_WNM_TFS,		"WNM_TFS"},
	{WL_WNM_SLEEP,		"WNM_SLEEP_MODE"},
	{WL_WNM_DMS,		"WNM_DMS"},
	{WL_WNM_FMS,		"WNM_FMS"},
	{WL_WNM_NOTIF,		"WNM_NOTIFICATION"},
	{0, NULL}
};

static void
wlc_wnm_scb_dump(void *context, struct scb *scb, struct bcmstrbuf *b)
{
	char capbuf[256];
	wlc_wnm_info_t *wnm = (wlc_wnm_info_t *)context;

	bcm_format_flags(scb_capstr, wlc_wnm_get_scbcap(wnm->wlc, scb), capbuf, 256);

	bcm_bprintf(b, "     WNM cap: 0x%x", wlc_wnm_get_scbcap(wnm->wlc, scb));
	if (capbuf[0] != '\0')
		bcm_bprintf(b, " [%s]", capbuf);
	bcm_bprintf(b, "\n");
}
#endif /* BCMDBG */

uint16
wlc_wnm_maxidle(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	wlc_wnm_info_t* wnm = wlc->wnm_info;
	wnm_bsscfg_cubby_t* wnm_cfg = NULL;

	if (wnm != NULL) {
		wnm_cfg = WNM_BSSCFG_CUBBY(wnm, cfg);
		if (wnm_cfg != NULL) {
			return wnm_cfg->bss_max_idle_period;
		}
	}

	return 0;
}

#ifdef WLWNM_AP
bool
wlc_wnm_bss_idle_opt(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	if (wlc->wnm_info != NULL && cfg != NULL) {
		wnm_bsscfg_cubby_t* wnm_cfg = WNM_BSSCFG_CUBBY(wlc->wnm_info, cfg);
		return wnm_cfg->bss_idle_opt;
	}

	return 0;
}

static void
wlc_wnm_tfs_req_free(wlc_wnm_info_t *wnm, wnm_tfs_fset_t **tfs_fset_head, int tfs_id)
{
	wnm_tfs_fset_t *tfs_req_curr = *tfs_fset_head;
	wnm_tfs_fset_t *tfs_req_prev = NULL;
	wnm_tfs_fset_t *tfs_req_d = NULL;

	/* Free all tfs req or the matched tfs req */
	while (tfs_req_curr != NULL) {
		if (tfs_id < 0 || tfs_id == tfs_req_curr->tfs_id) {
			tfs_req_d = tfs_req_curr;
			if (tfs_req_prev == NULL)
				*tfs_fset_head = tfs_req_curr->next;
			else
				tfs_req_prev->next = tfs_req_curr->next;
		}
		else {
			tfs_req_prev = tfs_req_curr;
		}

		tfs_req_curr = tfs_req_curr->next;

		if (tfs_req_d) {
			wnm_tfs_filter_t *filter_curr, *filter_prev;
			filter_curr = tfs_req_d->filter_head;

			WL_WNM(("wl%d: %s: free tfs req %p with tfs id %d\n",
				wnm->wlc->pub->unit, __FUNCTION__, OSL_OBFUSCATE_BUF(tfs_req_d),
				tfs_req_d->tfs_id));

			while (filter_curr != NULL) {
				/* Free tclas in filter */
				wnm_tclas_free(wnm->wlc, filter_curr->tclas_head);

				/* Move to the next filter and free the current one */
				filter_prev = filter_curr;
				filter_curr = filter_curr->next;
				MFREE(wnm->wlc->osh, filter_prev, sizeof(wnm_tfs_filter_t));
			}
			MFREE(wnm->wlc->osh, tfs_req_d, sizeof(wnm_tfs_fset_t));
			tfs_req_d = NULL;

			if (tfs_id > 0)
				return;
		}
	}
}
#endif /* WLWNM_AP */

static bool
wlc_wnm_tclas_comp_type0(uint8 mask, struct ether_header *eh, dot11_tclas_fc_0_eth_t *fc)
{
	bool conflict = FALSE;
	/* check SRC EA */
	if (mboolisset(mask, DOT11_TCLAS_MASK_0) &&
	    bcmp((void *)eh->ether_shost, (void *)fc->sa, ETHER_ADDR_LEN)) {
		conflict = TRUE;
		goto done_0;
	}
	/* check DST EA */
	if (mboolisset(mask, DOT11_TCLAS_MASK_1) &&
	    bcmp((void *)eh->ether_dhost, (void *)fc->da, ETHER_ADDR_LEN)) {
		conflict = TRUE;
		goto done_0;
	}
	/* check Next Proto */
	if (mboolisset(mask, DOT11_TCLAS_MASK_2) && eh->ether_type != fc->eth_type)
		conflict = TRUE;
done_0:
	return conflict;
}

static bool
wlc_wnm_tclas_comp_type1(uint8 mask, struct ipv4_hdr *iph, struct bcmudp_hdr *udph,
	dot11_tclas_fc_1_ipv4_t *fc)
{
	bool conflict = FALSE;

	if (mboolisset(mask, DOT11_TCLAS_MASK_0) && IP_VER(iph) != fc->version) {
		conflict = TRUE;
		goto done_1;
	}
	if (mboolisset(mask, DOT11_TCLAS_MASK_1) &&
	    bcmp((void *)iph->src_ip, (void *)&fc->src_ip, IPV4_ADDR_LEN)) {
		conflict = TRUE;
		goto done_1;
	}
	if (mboolisset(mask, DOT11_TCLAS_MASK_2) &&
	    bcmp((void *)iph->dst_ip, (void *)&fc->dst_ip, IPV4_ADDR_LEN)) {
		conflict = TRUE;
		goto done_1;
	}
	if (mboolisset(mask, DOT11_TCLAS_MASK_3) && udph->src_port != fc->src_port) {
		conflict = TRUE;
		goto done_1;
	}
	if (mboolisset(mask, DOT11_TCLAS_MASK_4) && udph->dst_port != fc->dst_port) {
		conflict = TRUE;
		goto done_1;
	}
	if (mboolisset(mask, DOT11_TCLAS_MASK_5) && ((iph->tos >> 2) != (fc->dscp & 0x3f))) {
		conflict = TRUE;
		goto done_1;
	}
	if (mboolisset(mask, DOT11_TCLAS_MASK_6) && iph->prot != fc->protocol)
		conflict = TRUE;
done_1:
	return conflict;
}

static bool
wlc_wnm_tclas_comp_type2(uint8 mask, uint16 tci, dot11_tclas_fc_2_8021q_t *fc)
{

	bool conflict = FALSE;
	/* check SRC EA */
	if (mboolisset(mask, DOT11_TCLAS_MASK_0) && tci != fc->tci)
		conflict = TRUE;

	return conflict;
}

static bool
wlc_wnm_tclas_comp_type3(uint8 *frame_data, int frame_len, uint16 filter_len,
	dot11_tclas_fc_3_filter_t *fc)
{
	bool conflict = FALSE;
	uint8 *filter_pattern = fc->data;
	uint8 *filter_mask = (uint8 *)(&fc->data[0] + filter_len);
	int idx;

	/* check frame length first */
	if ((filter_len + fc->offset) > frame_len) {
		conflict = TRUE;
		goto done_3;
	}

	frame_data += fc->offset;
	for (idx = 0; idx < filter_len; idx++) {
		if ((frame_data[idx] & filter_mask[idx]) !=
		    (filter_pattern[idx] & filter_mask[idx])) {
			conflict = TRUE;
			break;
		}
	}

done_3:
	return conflict;
}

#define wlc_wnm_tclas_comp_type4_v4(mask, iph, udph, pa4h) \
	wlc_wnm_tclas_comp_type1(mask, iph, udph, pa4h)

static bool
wlc_wnm_tclas_comp_type4_v6(uint8 mask, struct ipv6_hdr *ip6h, struct bcmudp_hdr *udph,
	dot11_tclas_fc_4_ipv6_t *fc)
{
	bool conflict = FALSE;

	if (mboolisset(mask, DOT11_TCLAS_MASK_0) && IP_VER(ip6h) != fc->version) {
		conflict = TRUE;
		goto done_4;
	}
	if (mboolisset(mask, DOT11_TCLAS_MASK_1) &&
	    bcmp((void *)&ip6h->saddr, (void *)fc->saddr, IPV6_ADDR_LEN)) {
		conflict = TRUE;
		goto done_4;
	}
	if (mboolisset(mask, DOT11_TCLAS_MASK_2) &&
	    bcmp((void *)&ip6h->daddr, (void *)fc->daddr, IPV6_ADDR_LEN)) {
		conflict = TRUE;
		goto done_4;
	}
	if (mboolisset(mask, DOT11_TCLAS_MASK_3) && udph->src_port != fc->src_port) {
		conflict = TRUE;
		goto done_4;
	}
	if (mboolisset(mask, DOT11_TCLAS_MASK_4) && udph->dst_port != fc->dst_port) {
		conflict = TRUE;
		goto done_4;
	}
	if (mboolisset(mask, DOT11_TCLAS_MASK_5) &&
	    (((ip6h->priority << 4) | (ip6h->flow_lbl[0] & 0xf)) >> 2) != (fc->dscp & 0x3f)) {
		conflict = TRUE;
		goto done_4;
	}
	if (mboolisset(mask, DOT11_TCLAS_MASK_6) && ip6h->nexthdr != fc->nexthdr) {
		conflict = TRUE;
		goto done_4;
	}
	if (mboolisset(mask, DOT11_TCLAS_MASK_7) &&
	    bcmp((void *)ip6h->flow_lbl, (void *)fc->flow_lbl, 3)) {
		conflict = TRUE;
		goto done_4;
	}

done_4:
	return conflict;
}

static bool
wlc_wnm_tclas_comp_type5(uint8 mask, uint16 vlan_tag, dot11_tclas_fc_5_8021d_t *fc)
{

	bool conflict = FALSE;
	/* check SRC EA */
	if (mboolisset(mask, DOT11_TCLAS_MASK_0) && (vlan_tag >> 13) != fc->pcp) {
		conflict = TRUE;
		goto done_5;
	}
	if (mboolisset(mask, DOT11_TCLAS_MASK_1) && (vlan_tag & 0x1000) != fc->cfi) {
		conflict = TRUE;
		goto done_5;
	}
	if (mboolisset(mask, DOT11_TCLAS_MASK_2) && (vlan_tag & 0xfff) != fc->vid) {
		conflict = TRUE;
	}

done_5:
	return conflict;
}

static bool
wlc_wnm_tclas_match(wnm_tclas_t *tclas, frame_proto_t *fp, bool allmatch)
{
	uint8 matched = 0;
	uint8 type, mask;
	bool conflict;

	while (tclas) {
		conflict = FALSE;
		type = tclas->fc.hdr.type;
		mask = tclas->fc.hdr.mask;

		/* real matching frame and TCLAS parameters */
		switch (type) {
		case DOT11_TCLAS_FC_0_ETH: {
			struct ether_header *eh = (struct ether_header *)fp->l2;
			dot11_tclas_fc_0_eth_t *fc = &tclas->fc.t0_eth;

			/* skip non ether header */
			if (fp->l2_t != FRAME_L2_ETH_H) {
				conflict = TRUE;
				break;
			}

			conflict = wlc_wnm_tclas_comp_type0(mask, eh, fc);

			break;
		}
		case DOT11_TCLAS_FC_1_IP: {
			struct ipv4_hdr *iph = (struct ipv4_hdr *)fp->l3;
			/* we use udhp here just because tcp/udp have the same port offset */
			struct bcmudp_hdr *udph = (struct bcmudp_hdr *)fp->l4;
			dot11_tclas_fc_1_ipv4_t *fc = &tclas->fc.t1_ipv4;

			/* skip non ipv4 header */
			if (fp->l3_t != FRAME_L3_IP_H) {
				conflict = TRUE;
				break;
			}

			conflict = wlc_wnm_tclas_comp_type1(mask, iph, udph, fc);

			break;
		}
		case DOT11_TCLAS_FC_2_8021Q: {
			struct ethervlan_header *evh = (struct ethervlan_header *)fp->l2;
			struct dot3_mac_llc_snapvlan_header *svh =
				(struct dot3_mac_llc_snapvlan_header *)fp->l2;
			dot11_tclas_fc_2_8021q_t *fc = &tclas->fc.t2_8021q;
			uint16 tci;

			if ((fp->l2_t != FRAME_L2_ETHVLAN_H) && (fp->l2_t != FRAME_L2_SNAPVLAN_H)) {
				conflict = TRUE;
				break;
			}

			if (fp->l2_t == FRAME_L2_ETHVLAN_H)
				tci = evh->vlan_tag;
			else
				tci = svh->vlan_tag;

			conflict = wlc_wnm_tclas_comp_type2(mask, tci, fc);

			break;
		}
		case DOT11_TCLAS_FC_3_OFFSET: {
			uint16 filter_len = (tclas->fc_len - 5)/2;
			dot11_tclas_fc_3_filter_t *fc = &tclas->fc.t3_filter;

			/* reduce half to get real pattern length */
			conflict = wlc_wnm_tclas_comp_type3(fp->l3, fp->l3_len, filter_len, fc);

			break;
		}
		case DOT11_TCLAS_FC_4_IP_HIGHER: {
			struct ipv4_hdr *iph = (struct ipv4_hdr *)fp->l3;
			struct ipv6_hdr *ip6h = (struct ipv6_hdr *)fp->l3;
			struct bcmudp_hdr *udph = (struct bcmudp_hdr *)fp->l4;
			dot11_tclas_fc_4_ipv4_t *pa4h = &tclas->fc.t4_ipv4;
			dot11_tclas_fc_4_ipv6_t *pa6h = &tclas->fc.t4_ipv6;

			if ((fp->l3_t != FRAME_L3_IP_H) && (fp->l3_t != FRAME_L3_IP6_H)) {
				conflict = TRUE;
				break;
			}

			if (pa4h->version == IP_VER_4)
				conflict = wlc_wnm_tclas_comp_type4_v4(mask, iph, udph, pa4h);
			else
				conflict = wlc_wnm_tclas_comp_type4_v6(mask, ip6h, udph, pa6h);

			break;
		}
		case DOT11_TCLAS_FC_5_8021D: {
			struct ethervlan_header *evh = (struct ethervlan_header *)fp->l2;
			struct dot3_mac_llc_snapvlan_header *svh =
				(struct dot3_mac_llc_snapvlan_header *)fp->l2;
			dot11_tclas_fc_5_8021d_t *fc = &tclas->fc.t5_8021d;
			uint16 vlan_tag;

			if ((fp->l2_t != FRAME_L2_ETHVLAN_H) && (fp->l2_t != FRAME_L2_SNAPVLAN_H)) {
				conflict = TRUE;
				break;
			}
			if (fp->l2_t == FRAME_L2_ETHVLAN_H)
				vlan_tag = ntoh16(evh->vlan_tag);
			else
				vlan_tag = ntoh16(svh->vlan_tag);

			conflict = wlc_wnm_tclas_comp_type5(mask, vlan_tag, fc);

			break;
		}
		default:
			conflict = TRUE;
			break;
		}

		if (conflict) {
			/* request to match all TCLAS but we missed it */
			if (allmatch)
				return FALSE;
		} else
			matched |= 1;

		tclas = tclas->next;
	}

	if (matched)
		return TRUE;

	return FALSE;
}

#ifdef WLWNM_AP
void
wlc_wnm_rx_tstamp_update(wlc_info_t *wlc, struct scb *scb)
{
	wlc_wnm_info_t *wnm = wlc->wnm_info;
	wnm_scb_cubby_t *wnm_scb = SCB_WNM_CUBBY(wnm, scb);

	wnm_scb->rx_tstamp = wnm->timestamp;
}

bool
wlc_wnm_timbc_req_ie_process(wlc_info_t *wlc, uint8 *tlvs, int len, struct scb *scb)
{
	dot11_timbc_req_ie_t *ie_tlv;
	wnm_bsscfg_cubby_t *wnm_cfg;
	wnm_scb_cubby_t *wnm_scb;

	if (wlc->wnm_info == NULL)
		return FALSE;

	wnm_cfg = WNM_BSSCFG_CUBBY(wlc->wnm_info, scb->bsscfg);

	ASSERT(scb != NULL);

	wnm_scb = SCB_WNM_CUBBY(wlc->wnm_info, scb);

	ie_tlv = (dot11_timbc_req_ie_t *)
		 bcm_parse_tlvs(tlvs, len, DOT11_MNG_TIMBC_REQ_ID);

	if (!ie_tlv)
		return FALSE;
	if (ie_tlv->len != DOT11_TIMBC_REQ_IE_LEN)
		return FALSE;

	/* AP have enabled TIM_BC but have not setup TIM_BC offset.  Deny request */
	if (wnm_cfg->timbc_offset == 0) {
		wnm_scb->timbc_status = DOT11_TIMBC_STATUS_DENY;
		wnm_scb->timbc_interval = 0;
	}
	else {
		/* interval == 0 imply stop tim service */
		if (ie_tlv->interval == 0)
			wnm_scb->timbc_status = DOT11_TIMBC_STATUS_ACCEPT;
		else
			wnm_scb->timbc_status = DOT11_TIMBC_STATUS_ACCEPT_TSTAMP;

		wnm_scb->timbc_interval = ie_tlv->interval;
		wnm_scb->timbc_high_rate = wlc_scb_ratesel_get_primary(wlc, scb, NULL);

		if (wnm_cfg->timbc_fix_intv != 0) {
			wnm_scb->timbc_status = DOT11_TIMBC_STATUS_OVERRIDDEN;
			wnm_scb->timbc_interval = wnm_cfg->timbc_fix_intv;
		}
	}
	return TRUE;
}

int
wlc_wnm_scb_timbc_status(wlc_info_t *wlc, struct scb *scb)
{
	wnm_scb_cubby_t *wnm_scb = SCB_WNM_CUBBY(wlc->wnm_info, scb);

	return wnm_scb->timbc_status;
}

int
wlc_wnm_prep_timbc_resp_ie(wlc_info_t *wlc, uint8 *p, int *plen, struct scb *scb)
{
	wnm_scb_cubby_t *wnm_scb = SCB_WNM_CUBBY(wlc->wnm_info, scb);
	dot11_timbc_resp_ie_t *ie = (dot11_timbc_resp_ie_t *)p;
	int err = BCME_OK;

	ie->id = DOT11_MNG_TIMBC_RESP_ID;
	ie->status = wnm_scb->timbc_status;

	switch (ie->status) {
	case DOT11_TIMBC_STATUS_DENY:
		ie->len = DOT11_TIMBC_DENY_RESP_IE_LEN;
		break;
	case DOT11_TIMBC_STATUS_ACCEPT:
	case DOT11_TIMBC_STATUS_ACCEPT_TSTAMP:
	case DOT11_TIMBC_STATUS_OVERRIDDEN: {
		wnm_bsscfg_cubby_t *wnm_cfg;
		uint16 high_rate = wf_rspec_to_rate(wnm_scb->timbc_high_rate)/500;
		uint16 low_rate = wf_rspec_to_rate(wlc->bcn_rspec)/500;

		wnm_cfg = WNM_BSSCFG_CUBBY(wlc->wnm_info, scb->bsscfg);

		ie->len = DOT11_TIMBC_ACCEPT_RESP_IE_LEN;
		ie->interval = wnm_scb->timbc_interval;
		htol32_ua_store(wnm_cfg->timbc_offset, (uint8*)&(ie->offset));
		/* in unit of 0.5 Mb/s */
		htol16_ua_store(high_rate, (uint8*)&(ie->high_rate));
		/* in unit of 0.5 Mb/s */
		htol16_ua_store(low_rate, (uint8*)&(ie->low_rate));

		break;
	}
	default:
		WL_WNM(("WNM TIM status incorrect: %d\n", ie->status));
		err = BCME_ERROR;
		break;
	}

	*plen = ie->len;

	return err;
}

void
wlc_wnm_tbtt(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	wnm_bsscfg_cubby_t *wnm_cfg;

	if (wlc->wnm_info == NULL || bsscfg == NULL)
		return;

	wnm_cfg = (wnm_bsscfg_cubby_t *)WNM_BSSCFG_CUBBY(wlc->wnm_info, bsscfg);

	/* no need to send tim broadcast since it is aligned to TBTT */
	if (WNM_TIMBC_ENABLED(wnm_cfg->cap) && wnm_cfg->timbc_offset != 0)
		wlc_wnm_timbc_tbtt(wlc, bsscfg, wnm_cfg);

}

void
wlc_wnm_tttt(wlc_info_t *wlc)
{
	wlc_bsscfg_t *bsscfg;
	wnm_bsscfg_cubby_t *wnm_cfg;
	int idx = 0;

	if (wlc->wnm_info == NULL)
		return;

	FOREACH_UP_AP(wlc, idx, bsscfg) {
		wnm_cfg = WNM_BSSCFG_CUBBY(wlc->wnm_info, bsscfg);

		if (!WNM_TIMBC_ENABLED(wnm_cfg->cap) || wnm_cfg->timbc_offset == 0)
			continue;

		/* send high rate TIM frame first */
		if (wnm_cfg->timbc_frame.p != NULL) {
			/* spec request to send high rate TIM Bcast first */
			wlc_wnm_send_timbc_act_frame(wlc, bsscfg, wnm_cfg->timbc_frame.p);
		}

		/* send basic rate TIM frame then */
		if (wnm_cfg->timbc_frame.p_low != NULL) {
			/* spec request to send high rate TIM Bcast first */
			wlc_wnm_send_timbc_act_frame(wlc, bsscfg, wnm_cfg->timbc_frame.p_low);
		}

		/* once completed, let wlc_dotxstatus perform PKTFREE and remove local pointer */
		wnm_cfg->timbc_frame.p = NULL;
		wnm_cfg->timbc_frame.p_low = NULL;
	}

	return;
}

static void
wlc_wnm_send_timbc_act_frame(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, void *p)
{

	wlc_txh_info_t txh_info;
	struct scb* scb = WLC_BCMCSCB_GET(wlc, bsscfg);

	WLPKTTAGSCBSET(p, scb);

	wlc_get_txh_info(wlc, p, &txh_info);

	wlc_bmac_txfifo(wlc->hw, TX_ATIM_FIFO, p, TRUE, txh_info.TxFrameID, 1);
}

int
wlc_wnm_scb_sm_interval(wlc_info_t *wlc, struct scb *scb)
{
	wnm_scb_cubby_t *wnm_scb = SCB_WNM_CUBBY(wlc->wnm_info, scb);

	if (wnm_scb->sleeping && wnm_scb->sleep_interval != 0)
		return wnm_scb->sleep_interval;
	else
		return 0;
}

bool
wlc_wnm_dms_amsdu_on(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	wlc_wnm_info_t *wnm = wlc->wnm_info;
	wnm_bsscfg_cubby_t* wnm_cfg = NULL;
	bool amsdu_enabled = AMSDU_TX_ENAB(wlc->pub);
	if (wnm && cfg)
		wnm_cfg = WNM_BSSCFG_CUBBY(wnm, cfg);

	/* 802.11v-dms requests to aggregate frame in amsdu */
	if (amsdu_enabled &&
		WLWNM_ENAB(wlc->pub) &&
		wnm_cfg &&
		WNM_DMS_ENABLED(wnm_cfg->cap) &&
		wnm_cfg->dms_info.dms_desc_head != NULL) {
		return TRUE;
	}

	return FALSE;
}

void
wlc_wnm_dms_spp_conflict(wlc_info_t *wlc, struct scb *scb)
{
	wnm_scb_cubby_t *wnm_scb = SCB_WNM_CUBBY(wlc->wnm_info, scb);

	wnm_scb->spp_conflict = TRUE;
}

static void
wlc_wnm_bss_idle_timer(void *context)
{
	wlc_wnm_info_t *wnm = (wlc_wnm_info_t *)context;
	wlc_info_t *wlc = wnm->wlc;
	struct scb_iter scbiter;
	struct scb *scb;

	if (!WLWNM_ENAB(wlc->pub))
		return;

	wnm->timestamp++;

	/* BSS Max Idle Period check in 100ms scale */
	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		wlc_bsscfg_t *cfg = SCB_BSSCFG(scb);
		/* 1000TU in 100ms scale */
		uint32 bss_max_idle_prd = (wlc_wnm_maxidle(wlc, cfg) * 1280) / 125;
		wnm_scb_cubby_t *wnm_scb = SCB_WNM_CUBBY(wnm, scb);

		if (SCB_MARKED_DEL(scb) || SCB_DEL_IN_PROGRESS(scb)) {
			continue;
		}

		if (BSSCFG_AP(cfg) && SCB_ASSOCIATED(scb) && bss_max_idle_prd &&
			(wnm->timestamp - wnm_scb->rx_tstamp) > bss_max_idle_prd) {
#ifdef BCMDBG
			char eabuf[ETHER_ADDR_STR_LEN];
			WL_ASSOC(("wl%d: BSS Max Idle Period timeout(%d)(%d - %d)."
				" Disassoc(%s)\n", wlc->pub->unit, bss_max_idle_prd,
				wnm_scb->rx_tstamp, wnm->timestamp,
				bcm_ether_ntoa(&scb->ea, eabuf)));
#endif /* BCMDBG */
			wlc_senddisassoc(wlc, cfg, scb, &scb->ea, &cfg->BSSID, &cfg->cur_etheraddr,
				DOT11_RC_INACTIVITY);
			wlc_scb_resetstate(wlc, scb);

			wlc_bss_mac_event(wlc, cfg, WLC_E_DISASSOC_IND, &scb->ea,
				WLC_E_STATUS_SUCCESS, DOT11_RC_INACTIVITY, 0, NULL, 0);
		}
	}
}

static uint
wlc_wnm_ars_calc_maxidle_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;
	wlc_bsscfg_t *cfg = data->cfg;
	uint ie_len = 0;

	if (WLWNM_ENAB(wlc->pub)) {
		uint32 wnm_cap = wlc_wnm_get_cap(wlc, cfg);

		if (WNM_MAXIDLE_ENABLED(wnm_cap) &&
		    wlc_wnm_maxidle(wlc, cfg)) {
			ie_len = sizeof(dot11_bss_max_idle_period_ie_t);
		}
	}

	return ie_len;
}

static int
wlc_wnm_ars_write_maxidle_ie(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;
	wlc_bsscfg_t *cfg = data->cfg;

	if (WLWNM_ENAB(wlc->pub)) {
		uint32 wnm_cap = wlc_wnm_get_cap(wlc, cfg);

		/* write BSS Max Idle Period IE if it exists */
		if (WNM_MAXIDLE_ENABLED(wnm_cap) && wlc_wnm_maxidle(wlc, cfg)) {
			dot11_bss_max_idle_period_ie_t maxidle_cntx;
			uint16 idle_period = wlc_wnm_maxidle(wlc, cfg);

			memset(&maxidle_cntx, 0, sizeof(maxidle_cntx));
			htol16_ua_store(idle_period, &maxidle_cntx.max_idle_period);

			if (WSEC_ENABLED(cfg->wsec) && wlc_wnm_bss_idle_opt(wlc, cfg)) {
				maxidle_cntx.idle_opt = DOT11_BSS_MAX_IDLE_PERIOD_OPT_PROTECTED;
			}

			bcm_write_tlv(DOT11_MNG_BSS_MAX_IDLE_PERIOD_ID,
				&maxidle_cntx.max_idle_period, DOT11_BSS_MAX_IDLE_PERIOD_IE_LEN,
				data->buf);
		}
	}

	return BCME_OK;
}

static int
wlc_wnm_send_timbc_resp_frame(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg, struct scb *scb,
	uint8 token)
{
	wlc_info_t *wlc = wnm->wlc;
	void *p;
	uint8 *pbody;
	int bodylen;
	int resp_len;
	dot11_timbc_resp_t *resp;
	dot11_timbc_resp_ie_t *resp_ie;
	int err = BCME_OK;

	bodylen = DOT11_TIMBC_RESP_LEN + TLV_HDR_LEN + DOT11_TIMBC_ACCEPT_RESP_IE_LEN;

	if ((p = wlc_frame_get_action(wlc, &scb->ea, &bsscfg->cur_etheraddr,
		&bsscfg->BSSID, bodylen, &pbody, DOT11_ACTION_CAT_WNM)) == NULL) {
		err = BCME_ERROR;
		WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__, DOT11_ACTION_CAT_WNM,
			MALLOCED(wlc->osh)));
		goto done;
	}

	/* Prepare TIM Broadcast frame fields */
	resp = (dot11_timbc_resp_t *)pbody;
	resp->category = DOT11_ACTION_CAT_WNM;
	resp->action = DOT11_WNM_ACTION_TIMBC_RESP;
	resp->token = token;

	resp_ie = (dot11_timbc_resp_ie_t *)&resp->data[0];
	resp_len = 0;

	if (wlc_wnm_prep_timbc_resp_ie(wlc, (uint8 *)resp_ie, &resp_len, scb) == BCME_OK) {
		PKTSETLEN(wlc->osh, p, DOT11_TIMBC_RESP_LEN + TLV_HDR_LEN +
			resp_len + DOT11_MGMT_HDR_LEN);
	}
	else {
		/* error case.  free allocated packet */
		PKTFREE(wlc->osh, p, FALSE);
		err = BCME_ERROR;
		WL_WNM(("wl%d: %s: get timbc_ie error\n", wlc->pub->unit, __FUNCTION__));
		goto done;
	}

	if (WL_WNM_ON())
		prhex("Raw TIMBC Resp body", (uchar *)pbody, bodylen);

	wlc_sendmgmt(wlc, p, bsscfg->wlcif->qi, scb);
done:
	return err;
}

static int
wlc_wnm_timbc_switch(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	wnm_bsscfg_cubby_t *wnm_cfg = NULL;
	uint16 timbc_switch = 0;
	int32 timbc_offset = 0;
	uint32 timbc_intmask = 0;
	uint m_timbc_offset;

	if (wlc->wnm_info == NULL || bsscfg == NULL)
		return BCME_UNSUPPORTED;

	wnm_cfg = WNM_BSSCFG_CUBBY(wlc->wnm_info, bsscfg);

	if (!D11REV_GE(wlc->pub->corerev, 42)) {
		WL_WNM(("Unable to handle TIMBC switch.  Unsupported d11core (%d)\n",
			wlc->pub->corerev));
		return BCME_UNSUPPORTED;
	}
	/* 4360 M_TIBMC_OFFSET is in 0x65 * 2 */
	/* gen by autoshm */
	m_timbc_offset = M_TIMBC_OFFSET(wlc);

	if (WNM_TIMBC_ENABLED(wnm_cfg->cap) && wnm_cfg->timbc_offset != 0 && wlc->pub->up) {
		timbc_switch = MHF1_TIMBC_EN;
		timbc_intmask = MI_TTTT;

		/* convert TIMBC offset from us in unit to 8us in unit(ucode unit) */
		timbc_offset = wnm_cfg->timbc_offset;
		while (timbc_offset < 0) {
			timbc_offset += (wlc->default_bss->beacon_period * 1024);
		}
		if (wnm_cfg->timbc_offset > 0)
			timbc_offset += 400;
		else if (wnm_cfg->timbc_offset < 0)
			timbc_offset += 200;

		if (timbc_offset > 0) {
			timbc_offset = timbc_offset / 8;
		}

		/* write TIMBC offset in unit of 8us */
		wlc_write_shm(wlc, m_timbc_offset, (uint16)timbc_offset);
	}

	/* enable/disable TIMBC interrupt support */
	wlc_bmac_mhf(wlc->hw, MHF1, MHF1_TIMBC_EN, timbc_switch, WLC_BAND_ALL);

	/* turned on TTTT interrupt mask */
	wlc_bmac_set_defmacintmask(wlc->hw, MI_TTTT, timbc_intmask);

	return BCME_OK;
}

static void
wlc_wnm_timbc_clrframe(wlc_info_t *wlc, wnm_bsscfg_cubby_t *wnm_cfg)
{
	if (wnm_cfg->timbc_frame.p != NULL) {
		PKTFREE(wlc->osh, wnm_cfg->timbc_frame.p, FALSE);
		wnm_cfg->timbc_frame.p = NULL;
	}
	if (wnm_cfg->timbc_frame.p_low != NULL) {
		PKTFREE(wlc->osh, wnm_cfg->timbc_frame.p_low, FALSE);
		wnm_cfg->timbc_frame.p_low = NULL;
	}
}

static void *
wlc_frame_get_timframe(wlc_info_t *wlc, wlc_bsscfg_t *cfg, ratespec_t rspec_override,
	uint8 check_beacon, bool tsf_present)
{
	wlc_pkt_t pkt = NULL;
	uint8 buf[257] = {0}; /* 257(TIM IE len) */
	uint8 *pbody;
	dot11_timbc_t *tim_frame;
	uint tim_ie_len;
	wlc_iem_ft_cbparm_t ftcbparm;
	wlc_iem_cbparm_t cbparm;
	uint16 mactxcontrolhigh;
	uint16 abi_mimoantsel;

	bzero(&ftcbparm, sizeof(ftcbparm));
	bzero(&cbparm, sizeof(cbparm));
	cbparm.ft = &ftcbparm;

	/* retrieve TIM IE and length */
	wlc_iem_build_ie(wlc->iemi, cfg, FC_BEACON, DOT11_MNG_TIM_ID, NULL, &cbparm, buf, 256);
	tim_ie_len = wlc_iem_calc_ie_len(wlc->iemi, cfg, FC_BEACON, DOT11_MNG_TIM_ID,
	                                 NULL, &cbparm);

	pkt = wlc_frame_get_action(wlc, &ether_bcast, &cfg->cur_etheraddr,
		&cfg->BSSID, DOT11_TIMBC_HDR_LEN + tim_ie_len, &pbody, DOT11_ACTION_CAT_UWNM);
	if (pkt == NULL) {
		WL_ERROR(("Could not allocate SW probe template\n"));
		return NULL;
	}

	/* Generate TIM frame template body */
	bzero(pbody, DOT11_TIMBC_HDR_LEN + tim_ie_len);

	/* construct TIM frame content */
	tim_frame = (dot11_timbc_t *)pbody;
	tim_frame->category = DOT11_ACTION_CAT_UWNM;
	tim_frame->action = DOT11_UWNM_ACTION_TIM;
	tim_frame->check_beacon = check_beacon;

	bcopy(buf, pbody + DOT11_TIMBC_HDR_LEN, tim_ie_len);

	/* setup packet length */
	PKTSETLEN(wlc->osh, pkt, DOT11_MGMT_HDR_LEN + DOT11_TIMBC_HDR_LEN + tim_ie_len);
	WLPKTTAGBSSCFGSET(pkt, WLC_BSSCFG_IDX(cfg));

	rspec_override &= ~WL_RSPEC_LDPC;
	wlc_mgmt_ctl_d11hdrs(wlc, pkt, wlc->band->hwrs_scb, TX_ATIM_FIFO, rspec_override);

	if (D11REV_GE(wlc->pub->corerev, 42)) {
		/* AC TIMBC frame TSF present flag is in bit5 of MacTxControlHigh */
		mactxcontrolhigh = wlc_get_txh_mactxcontrolhigh(wlc, pkt);
		if (tsf_present) {
			mactxcontrolhigh |= D11AC_TXC_TIMBC_TSF;
		} else {
			mactxcontrolhigh &= ~(D11AC_TXC_TIMBC_TSF);
		}
		wlc_set_txh_mactxcontrolhigh(wlc, pkt, mactxcontrolhigh);
	} else {
		/* N TIMBC frame TSF present flag is in bit13 of CTX_BSSIDX_MIMOANT */
		abi_mimoantsel  = wlc_get_txh_abi_mimoantsel(wlc, pkt);
		if (tsf_present) {
			abi_mimoantsel |= ABI_MAS_TIMBC_TSF;
		} else {
			abi_mimoantsel &= ~(ABI_MAS_TIMBC_TSF);
		}
		wlc_set_txh_abi_mimoantsel(wlc, pkt, abi_mimoantsel);
	}

	return pkt;
}

static bool
wlc_wnm_timbc_support(wlc_info_t *wlc)
{
	/* post corerev 40, support start from 4360b0 */
	if (D11REV_GE(wlc->pub->corerev, 42))
		return TRUE;

	return FALSE;
}

static void
wlc_wnm_timbc_tbtt(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, wnm_bsscfg_cubby_t *wnm_cfg)
{
	struct scb *scb;
	struct scb_iter scbiter;
	bool flush_tim = FALSE;
	uint32 rate = 0xffffffff;
	ratespec_t tx_ratespec = 0;
	wlc_iem_ft_cbparm_t ftcbparm;
	wlc_iem_cbparm_t cbparm;
	uint ielen;

	/* update check_beacon for each tbtt */
	bzero(&ftcbparm, sizeof(ftcbparm));
	bzero(&cbparm, sizeof(cbparm));
	cbparm.ft = &ftcbparm;

	if ((ielen = wlc_iem_calc_ie_len(wlc->iemi, bsscfg, FC_BEACON, DOT11_MNG_DS_PARMS_ID,
	                                 NULL, &cbparm)) != 0) {
		uint8 dsie[4] = {0};
		wlc_iem_build_ie(wlc->iemi, bsscfg, FC_BEACON, DOT11_MNG_DS_PARMS_ID,
		                 NULL, &cbparm, dsie, 4);
		if (dsie[TLV_HDR_LEN] != wnm_cfg->timbc_dsie) {
			wnm_cfg->timbc_ie_update = TRUE;
			wnm_cfg->timbc_dsie = dsie[TLV_HDR_LEN];
		}
	}

	if ((ielen = wlc_iem_calc_ie_len(wlc->iemi, bsscfg, FC_BEACON, DOT11_MNG_HT_ADD,
	                                 NULL, &cbparm)) != 0) {
		uint8 htie[TLV_HDR_LEN + HT_ADD_IE_LEN] = {0};
		wlc_iem_build_ie(wlc->iemi, bsscfg, FC_BEACON, DOT11_MNG_HT_ADD,
		                 NULL, &cbparm, htie, TLV_HDR_LEN + HT_ADD_IE_LEN);
		if (bcmp(&htie[TLV_HDR_LEN], wnm_cfg->timbc_htie, HT_ADD_IE_LEN)) {
			wnm_cfg->timbc_ie_update = TRUE;
			bcopy(&htie[TLV_HDR_LEN], wnm_cfg->timbc_htie, HT_ADD_IE_LEN);
		}
	}

	/* iterate through SCB in BSSCFG to get maximum ratespec for high speed TIM frame */
	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
		wnm_scb_cubby_t *wnm_scb = SCB_WNM_CUBBY(wlc->wnm_info, scb);
		ASSERT(wnm_scb != NULL);

		if (SCB_TIMBC(wnm_scb->cap) && wnm_scb->timbc_interval != 0) {
			uint16 mod_result = wnm_cfg->timbc_offset > 0? 0:
				wnm_scb->timbc_interval - 1;

			if (WLCNTVAL(wlc->pub->_cnt->tbtt) % wnm_scb->timbc_interval ==
			    mod_result) {
				uint32 scb_rate =
					wf_rspec_to_rate(wnm_scb->timbc_high_rate);

				flush_tim = TRUE;

				if (rate > scb_rate) {
					rate = scb_rate;
					tx_ratespec = wnm_scb->timbc_high_rate;
				}
			}
		}
	}

	if (flush_tim && tx_ratespec) {
		bool ie_include = FALSE; /* critical ie included */

		bzero(&ftcbparm, sizeof(ftcbparm));
		bzero(&cbparm, sizeof(cbparm));
		cbparm.ft = &ftcbparm;

		/* free existing TIM frame if it's not send out, and keep template */
		wlc_wnm_timbc_clrframe(wlc, wnm_cfg);

		if (BSSCFG_AP(bsscfg)) {
			if (WL11H_ENAB(wlc)) {
				if (wlc_iem_calc_ie_len(wlc->iemi, bsscfg, FC_BEACON,
				                        DOT11_MNG_CHANNEL_SWITCH_ID, NULL, &cbparm))
					ie_include = TRUE;
				if (wlc_iem_calc_ie_len(wlc->iemi, bsscfg, FC_BEACON,
				                        DOT11_MNG_EXT_CSA_ID, NULL, &cbparm))
					ie_include = TRUE;
			}
			if (BSS_WL11H_ENAB(wlc, bsscfg) &&
			    wlc_iem_calc_ie_len(wlc->iemi, bsscfg, FC_BEACON,
			                        DOT11_MNG_CHANNEL_SWITCH_ID, NULL, &cbparm))
				ie_include = TRUE;
		}

		/* increasing check_beacon field if it needed */
		if (wnm_cfg->timbc_ie_update || ie_include) {
			wnm_cfg->timbc_frame.check_beacon++;
			wnm_cfg->timbc_ie_update = FALSE;
		}

		/* override rate if setup by user */
		if (wnm_cfg->timbc_rate)
			tx_ratespec = wnm_cfg->timbc_rate;

		/* Allocate basic ratespec TIM frame */
		if ((wnm_cfg->timbc_frame.p_low = wlc_frame_get_timframe(wlc, bsscfg, 0,
		    wnm_cfg->timbc_frame.check_beacon, wnm_cfg->timbc_tsf)) == NULL) {
			WL_ERROR(("%s %d: TIM basic rate frame allocation failed\n",
				__func__, __LINE__));
			return;
		}

		/* Allocate high ratespec TIM frame */
		if ((wnm_cfg->timbc_frame.p = wlc_frame_get_timframe(wlc, bsscfg, tx_ratespec,
		    wnm_cfg->timbc_frame.check_beacon, wnm_cfg->timbc_tsf)) == NULL) {
			WL_ERROR(("%s %d: TIM high rate frame allocation failed\n",
				__func__, __LINE__));
			return;
		}
	}
}

static int
wlc_wnm_arq_parse_timbc_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;
	wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;
	wlc_bsscfg_t *bsscfg = data->cfg;
	bcm_tlv_t *timbc_ie = (bcm_tlv_t *)data->ie;
	struct scb *scb = ftpparm->assocreq.scb;
	if (WLWNM_ENAB(wlc->pub)) {
		if (WNM_TIMBC_ENABLED(wlc_wnm_get_cap(wlc, bsscfg)) && timbc_ie != NULL) {
			wlc_wnm_timbc_req_ie_process(wlc, (uint8 *)timbc_ie,
				TLV_HDR_LEN + DOT11_TIMBC_REQ_IE_LEN, scb);
			WL_WNM(("Receive TIM IE in assoc req\n"));
		}
		else {
			wnm_scb_cubby_t *wnm_scb = SCB_WNM_CUBBY(wlc->wnm_info, scb);
			ASSERT(wnm_scb != NULL);
			wnm_scb->timbc_status = DOT11_TIMBC_STATUS_RESERVED;
			wnm_scb->timbc_interval = 0;
			wnm_scb->timbc_high_rate = 0;
		}
	}

	return BCME_OK;
}

static uint
wlc_wnm_ars_calc_timbc_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;
	wlc_iem_ft_cbparm_t *ftcbparm = data->cbparm->ft;
	wlc_bsscfg_t *cfg = data->cfg;
	struct scb *scb = ftcbparm->assocresp.scb;
	uint ie_len = 0;

	if (WLWNM_ENAB(wlc->pub)) {
		uint32 wnm_cap = wlc_wnm_get_cap(wlc, cfg);
		uint32 wnm_scbcap = wlc_wnm_get_scbcap(wlc, scb);
		int timbc_status = wlc_wnm_scb_timbc_status(wlc, scb);

		/* set TIM Broadcast length if it exists */
		if (WNM_TIMBC_ENABLED(wnm_cap) && SCB_TIMBC(wnm_scbcap) &&
		    timbc_status < DOT11_TIMBC_STATUS_RESERVED) {
			if (timbc_status == DOT11_TIMBC_STATUS_DENY)
				ie_len = TLV_HDR_LEN + DOT11_TIMBC_DENY_RESP_IE_LEN;
			else
				ie_len = TLV_HDR_LEN + DOT11_TIMBC_ACCEPT_RESP_IE_LEN;
		}
	}
	return ie_len;
}

static int
wlc_wnm_ars_write_timcb_ie(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;
	wlc_iem_ft_cbparm_t *ftcbparm = data->cbparm->ft;
	wlc_bsscfg_t *cfg = data->cfg;
	struct scb *scb = ftcbparm->assocresp.scb;

	if (WLWNM_ENAB(wlc->pub)) {
		uint32 wnm_cap = wlc_wnm_get_cap(wlc, cfg);
		uint32 wnm_scbcap = wlc_wnm_get_scbcap(wlc, scb);
		int timbc_status = wlc_wnm_scb_timbc_status(wlc, scb);

		/* set TIM Broadcast ie it exists */
		if (WNM_TIMBC_ENABLED(wnm_cap) && SCB_TIMBC(wnm_scbcap) &&
		    timbc_status < DOT11_TIMBC_STATUS_RESERVED) {
			dot11_timbc_resp_ie_t resp_ie;
			int resp_len = 0;
			if (wlc_wnm_prep_timbc_resp_ie(wlc, (uint8 *)&resp_ie, &resp_len,
			    scb) == BCME_OK) {

				bcm_write_tlv(DOT11_MNG_TIMBC_RESP_ID, &resp_ie.status,
					resp_len, data->buf);
			}
		}
	}

	return BCME_OK;
}

static int
wlc_wnm_send_tfs_resp_frame(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb,
	uint8 *tfs_resp_ie, int tfs_resp_ie_len, uint8 token)
{
	int maxlen;
	void *p;
	uint8 *pbody;
	int bodylen;
	dot11_tfs_resp_t *resp;

	maxlen = tfs_resp_ie_len + DOT11_TFS_RESP_LEN;

	if ((p = wlc_frame_get_action(wlc, &scb->ea, &bsscfg->cur_etheraddr,
		&bsscfg->BSSID, maxlen, &pbody, DOT11_ACTION_CAT_WNM)) == NULL) {
		return BCME_ERROR;
	}

	/* Prepare TFS response frame fields */
	resp = (dot11_tfs_resp_t *)pbody;
	resp->category = DOT11_ACTION_CAT_WNM;
	resp->action = DOT11_WNM_ACTION_TFS_RESP;
	resp->token = token;

	bodylen = DOT11_TFS_RESP_LEN;

	/* Copy tfs response ie */
	if (tfs_resp_ie && tfs_resp_ie_len > 0) {
		bcopy(tfs_resp_ie, resp->data, tfs_resp_ie_len);
		bodylen += tfs_resp_ie_len;
	}

	wlc_sendmgmt(wlc, p, bsscfg->wlcif->qi, scb);

	return BCME_OK;
}

static int
wlc_wnm_parse_tfs_req_tclas(wlc_info_t *wlc, wnm_tfs_filter_t *tfs_req_subelem, uint8 *body,
	uint8 body_len)
{
	uint8 *p = body;
	uint8 *p_end = body + body_len;
	uint8 status = DOT11_TFS_STATUS_ACCEPT;
	uint8 tclas_proc_cnt = 0;
	uint8 p_len = body_len;

	while (bcm_valid_tlv((bcm_tlv_t *)p, p_len)) {
		if (*p == DOT11_MNG_TCLAS_ID) {
			dot11_tclas_ie_t *tclas_ie = (dot11_tclas_ie_t *)p;
			wnm_tclas_t *tclas;

			// tclas data: user priority is 1 byte plus frame classifier field is 3..255
			// Minimum length should be at least 4 bytes

			if ((tclas_ie->len < DOT11_TCLAS_FC_MIN_LEN) ||
			    (tclas_ie->len > DOT11_TCLAS_FC_MAX_LEN)) {
				WL_WNM(("TCLAS length error (%d)\n", tclas_ie->len));
				status = DOT11_TFS_STATUS_DENY_FORMAT;
				break;
			}

			if (!isset(&wlc->wnm_info->tfs_tclastype, tclas_ie->fc.hdr.type)) {
				WL_WNM(("TCLAS Type not supported:%d\n", tclas_ie->fc.hdr.type));
				status = DOT11_TFS_STATUS_DENY_POLICY;
				break;
			}

			if ((tclas = MALLOCZ(wlc->osh, TCLAS_ELEM_FIXED_SIZE +
			                     tclas_ie->len - 1)) == NULL) {
				WL_WNM(("TCLAS allocating error\n"));
				status = DOT11_TFS_STATUS_DENY_RESOURCE;
				break;
			}

			tclas->user_priority = tclas_ie->user_priority;
			/* Exclude user priority byte */
			tclas->fc_len = tclas_ie->len - 1;
			bcopy(&tclas_ie->fc, &tclas->fc, tclas->fc_len);

			tfs_req_subelem->tclas_cnt++;

			WNM_ELEMENT_LIST_ADD(tfs_req_subelem->tclas_head, tclas);

			p += tclas_ie->len + TLV_HDR_LEN;
			p_len -= tclas_ie->len + TLV_HDR_LEN;
		}
		else if (*p == DOT11_MNG_TCLAS_PROC_ID) {
			dot11_tclas_proc_ie_t *proc_ie = (dot11_tclas_proc_ie_t *)p;

			// tclas proc ie contains proces field of only 1 byte
			if (proc_ie->len != DOT11_TCLAS_PROC_LEN) {
				WL_WNM(("TCLAS proc length error (%d)\n", proc_ie->len));
				status = DOT11_TFS_STATUS_DENY_FORMAT;
				break;
			}

			if (proc_ie->process > DOT11_TCLAS_PROC_NONMATCH) {
				WL_WNM(("invalid TCLAS_PROC process value(%d)\n",
					proc_ie->process));
				status = DOT11_TFS_STATUS_DENY_RESOURCE;
				break;
			}

			p += DOT11_TCLAS_PROC_IE_LEN;
			p_len -= DOT11_TCLAS_PROC_IE_LEN;

			if (p != p_end) {
				WL_WNM(("invalid TCLAS_PROC process order\n"));
				status = DOT11_TFS_STATUS_DENY_FORMAT;
				break;
			}

			if (proc_ie->process > DOT11_TCLAS_PROC_MATCHONE) {
				WL_WNM(("invalid TCLAS_PROC match type (%d)\n", proc_ie->process));
				status = DOT11_TFS_STATUS_DENY_FORMAT;
				break;
			}

			tfs_req_subelem->tclas_proc = proc_ie->process;
			tclas_proc_cnt++;
		}
		else {
			WL_WNM(("unknown subelement (%d)\n", *p));
			status = DOT11_TFS_STATUS_DENY_FORMAT;
			break;
		}
	}

	if ((tfs_req_subelem->tclas_cnt > 1 && tclas_proc_cnt != 1) ||
	    (tfs_req_subelem->tclas_cnt == 1 && tclas_proc_cnt != 0)) {
		WL_WNM(("incorrect TCLAS(%d)/TCLAS_PROC(%d) count\n",
			tfs_req_subelem->tclas_cnt, tclas_proc_cnt));
		status = DOT11_TFS_STATUS_DENY_FORMAT;
	}

	return status;
}

static int
wlc_wnm_parse_tfs_req_subelem(wlc_info_t *wlc, wnm_tfs_fset_t *tfs_req, uint8 *body,
	uint8 body_len, uint8 *buf, uint8 *resp_len, bool *valid)
{
	uint8 *p = body;
	uint8 *p_end = body + body_len;
	dot11_tfs_se_t *se;
	dot11_tfs_status_se_t *st_se;
	wnm_tfs_filter_t *subelem;
	int buf_len = 0;
	int err = BCME_OK;
	wlc_wnm_cmn_info_t *wnm_cmn = wlc->wnm_info->cmn;

	while (p < p_end) {
		se = (dot11_tfs_se_t *)p;
		/* init TFS status subelement */
		st_se = (dot11_tfs_status_se_t *)(buf + buf_len);
		st_se->sub_id = DOT11_TFS_RESP_TFS_STATUS_SE_ID;
		st_se->len = DOT11_TFS_STATUS_SE_LEN;
		st_se->resp_st = DOT11_TFS_STATUS_ACCEPT;
		buf_len += TLV_HDR_LEN + DOT11_TFS_STATUS_SE_LEN;

		if (se->sub_id == DOT11_TFS_REQ_TFS_SE_ID) {
			if ((subelem = MALLOCZ(wlc->osh, sizeof(wnm_tfs_filter_t))) == NULL) {
				WL_ERROR(("wl%d: %s: MALLOC(%d) failed, malloced %d bytes\n",
					WLCWLUNIT(wlc), __FUNCTION__, (int)sizeof(wnm_tfs_filter_t),
					MALLOCED(wlc->osh)));
				err = BCME_ERROR;
				break;
			}
			bzero(subelem, sizeof(wnm_tfs_filter_t));
			WNM_ELEMENT_LIST_ADD(tfs_req->filter_head, subelem);

			/* if any resp status is not accept, turn off valid value to free
			 * whole tfs by upper layer.
			 */
			if ((st_se->resp_st = wlc_wnm_parse_tfs_req_tclas(wlc, subelem,
			     se->data, se->len)) != DOT11_TFS_STATUS_ACCEPT)
				*valid = FALSE;

			/* replace DENY to ALTERNATIVE if proposed TCLAS entry existed */
			if (st_se->resp_st == DOT11_TFS_STATUS_DENY_POLICY &&
			    wnm_cmn->tclas_head != NULL &&
				wnm_cmn->tclas_cnt > 0) {
					dot11_tfs_se_t *resp_se = (dot11_tfs_se_t *)st_se->data;
					int ext_len =
						wlc_wnm_tclas_ie_prep(wnm_cmn->tclas_head,
							resp_se->data);

					if ((buf_len += TLV_HDR_LEN + ext_len) < 255) {
						resp_se->sub_id =  DOT11_TFS_RESP_TFS_SE_ID;
						resp_se->len = ext_len;
						st_se->resp_st =
							DOT11_TFS_STATUS_ALTPREF_TCLAS_UNSUPP;
					} else {
						WL_WNM(("TFS alt length error. buf_len:%d,"
							"ext_len: %d\n",
							buf_len, ext_len));
						err = BCME_ERROR;
						break;
					}
			}
		} else if (se->sub_id == DOT11_TFS_RESP_VENDOR_SE_ID) {
			WL_WNM(("wl%d: Got TFS req vendor subelement from STA, skip it\n",
				wlc->pub->unit));
		}
		else {
			WL_WNM(("wl%d: TFS subelement id(%d) unsupported\n", wlc->pub->unit,
				se->sub_id));
			err = BCME_ERROR;
			break;
		}

		p += (se->len + TLV_HDR_LEN);
	}

	if (buf_len > 255) {
		WL_WNM(("TFS se length error. buf_len:%d\n", buf_len));
		err = BCME_ERROR;
	}
	else {
		*resp_len += buf_len;
	}

	return err;
}

static int
wlc_wnm_parse_tfs_req_ie(wlc_info_t *wlc, struct scb *scb, uint8 *body,
	int body_len, uint8 *buf, int *buf_len)
{
	dot11_tfs_req_ie_t *req_ie;
	dot11_tfs_resp_ie_t *resp_ie;
	uint8 *p = body;
	uint8 *p_end = body + body_len;
	wnm_tfs_fset_t *tfs_list = NULL, *wnm_tfs_req = NULL;
	bool valid = TRUE;
	int err = BCME_OK;

	wnm_scb_cubby_t *wnm_scb = SCB_WNM_CUBBY(wlc->wnm_info, scb);

	*buf_len = 0;
	while (p < p_end) {
		req_ie = (dot11_tfs_req_ie_t *)p;
		resp_ie = (dot11_tfs_resp_ie_t *)(buf + *buf_len);
		resp_ie->id = DOT11_MNG_TFS_RESPONSE_ID;
		resp_ie->len = DOT11_TFS_RESP_IE_LEN;
		resp_ie->tfs_id = req_ie->tfs_id;

		if (req_ie->id != DOT11_MNG_TFS_REQUEST_ID || ((p_end - p) < req_ie->len)) {
			WL_WNM(("Invalid WNM TFS IE.  ID:%d, len:%d, real len:%d.\n",
			        req_ie->id, req_ie->len, (uint)(p_end - p)));
			valid = FALSE;
			err = BCME_ERROR;
			break;
		}

		/* got a valid TFS request element.  Construct TFS rule */
		if ((wnm_tfs_req = MALLOCZ(wlc->osh, sizeof(wnm_tfs_fset_t))) == NULL) {
			WL_ERROR(("wl%d: %s: MALLOC(%d) failed, malloced %d bytes\n",
				WLCWLUNIT(wlc), __FUNCTION__, (int)sizeof(wnm_tfs_fset_t),
				MALLOCED(wlc->osh)));
			valid = FALSE;
			err = BCME_ERROR;
			break;
		}

		/* Construct tfs_req filter */
		if (wlc_wnm_parse_tfs_req_subelem(wlc, wnm_tfs_req, req_ie->data, req_ie->len - 2,
		    resp_ie->data, &resp_ie->len, &valid) != BCME_OK) {
			WL_WNM(("WNM TFS filter construct error\n"));
			valid = FALSE;
			err = BCME_ERROR;
			break;
		}

		/* shift to next TFS Request Element, and TFS Response Element */
		p += TLV_HDR_LEN + req_ie->len;
		*buf_len += TLV_HDR_LEN + resp_ie->len;

		/* Add this wnm_tfs_req into tfs_list */
		wnm_tfs_req->tfs_id = req_ie->tfs_id;
		wnm_tfs_req->actcode = req_ie->actcode;
		wnm_tfs_req->next = tfs_list;
		if (wnm_tfs_req->actcode & DOT11_TFS_ACTCODE_NOTIFY)
			wnm_tfs_req->notify = TRUE;
		tfs_list = wnm_tfs_req;
	}

	/* if there's any invalid tfsreq, free all tfsreq */
	if (tfs_list && valid) {
		ASSERT(wnm_scb->tfs_fset_head == NULL);
		wnm_scb->tfs_fset_head = tfs_list;
	}
	else {
		/* delete wnm_tfs_req and attached subelement/tclas element */
		wlc_wnm_tfs_req_free(wlc->wnm_info, &tfs_list, -1);

		/* free wnm_tfs_req for failed case */
		if ((!valid) && wnm_tfs_req) {
			MFREE(wlc->osh, wnm_tfs_req, sizeof(wnm_tfs_fset_t));
		}
	}

	return err;
}

static int
wlc_wnm_send_tfs_notify_frame(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb,
	bcm_tlv_t *idlist)
{
	void *p;
	uint8 *pbody;
	int bodylen;
	dot11_tfs_notify_req_t *nreq;
	bodylen = DOT11_TFS_NOTIFY_REQ_LEN + idlist->len;

	/* allocate action frame */
	if ((p = wlc_frame_get_action(wlc, &scb->ea, &bsscfg->cur_etheraddr,
		&bsscfg->BSSID, bodylen, &pbody, DOT11_ACTION_CAT_WNM)) == NULL) {
		WL_ERROR(("WNM-Sleep allocate management frame error: Out of mem\n"));
		return BCME_ERROR;
	}

	/* Construct TFS notify frame fields */
	nreq = (dot11_tfs_notify_req_t *)pbody;
	nreq->category = DOT11_ACTION_CAT_WNM;
	nreq->action = DOT11_WNM_ACTION_TFS_NOTIFY_REQ;
	nreq->tfs_id_cnt = idlist->len;
	bcopy(&idlist->data, &nreq->tfs_id, idlist->len);

	wlc_sendmgmt(wlc, p, bsscfg->wlcif->qi, scb);

	return BCME_OK;
}

static int
wlc_wnm_parse_tfs_notify_resp(wlc_info_t *wlc, struct scb *scb, uint8 *body, int body_len)
{
	dot11_tfs_notify_resp_t *nresp = (dot11_tfs_notify_resp_t *)body;
	wnm_tfs_fset_t *tfs = NULL;
	uint8 *tfsid = nresp->tfs_id;
	wnm_scb_cubby_t *wnm_scb = SCB_WNM_CUBBY(wlc->wnm_info, scb);
	bool tfsid_found;
	int idx;

	if ((nresp->tfs_id_cnt + DOT11_TFS_NOTIFY_RESP_LEN) != body_len) {
		WL_WNM(("TFS Notify Resp length error. idcnt:%d body_len:%d\n",
			nresp->tfs_id_cnt, body_len - DOT11_TFS_NOTIFY_RESP_LEN));
		return BCME_ERROR;
	}

	for (idx = 0; idx < nresp->tfs_id_cnt; idx++, tfsid++) {
		tfs = wnm_scb->tfs_fset_head;
		tfsid_found = FALSE;

		while (tfs) {
			if (tfs->tfs_id == *tfsid) {
				tfsid_found = TRUE;
				if (tfs->actcode & DOT11_TFS_ACTCODE_NOTIFY)
					tfs->notify = TRUE;
				break;
			}
			tfs = tfs->next;
		}
		if (!tfsid_found)
			WL_WNM(("TFS ID (%d) not found in list?\n", *tfsid));
	}

	return BCME_OK;
}

/* return true if the frame should be bypass */
static bool
wlc_wnm_tfs_packet_handle(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb,
	frame_proto_t *fp)
{
	uint8 buf[TFS_NOTIFY_IDLIST_MAX + 2] = {0};
	wnm_tfs_filter_t *sub;
	bool remove_tfs = FALSE;
	bool send_frame = FALSE;
	bcm_tlv_t *idlist = (bcm_tlv_t *)buf;
	uint8 *idptr = idlist->data;
	wnm_tfs_fset_t *tfs = NULL;
	wnm_scb_cubby_t *wnm_scb = SCB_WNM_CUBBY(wlc->wnm_info, scb);
	bool match_alltclas;
	bool allse_matched;

	if ((tfs = wnm_scb->tfs_fset_head) == NULL)
		return TRUE;

	/* iterate TFS requests */
	while (tfs) {
		/* init first TFS sub-emement */
		sub = tfs->filter_head;

		allse_matched = TRUE;
		/* iterate TFS subelements */
		while (sub) {
			/* iterate TCLAS with all match case */
			if (sub->tclas_proc == DOT11_TCLAS_PROC_MATCHALL)
				match_alltclas = TRUE;
			else if (sub->tclas_proc == DOT11_TCLAS_PROC_MATCHONE)
				match_alltclas = FALSE;
			else {
				match_alltclas = FALSE;
				WL_WNM(("Unsupported tclas_proc value: %d\n",
					sub->tclas_proc));
			}
			if (!wlc_wnm_tclas_match(sub->tclas_head, fp, match_alltclas)) {
				allse_matched = FALSE;
				break;
			}

			sub = sub->next;
		}

		/* if all TFS Subelement matched, bypass frame and run correcpond action */
		if (allse_matched) {
			send_frame = TRUE;
			if (tfs->notify) {
				*idptr++ = tfs->tfs_id;
				idlist->len++;
				tfs->notify = FALSE;
			}

			/* remove target tfs request */
			if (tfs->actcode & DOT11_TFS_ACTCODE_DELETE)
				remove_tfs = TRUE;
		}

		tfs = tfs->next;
	}

	/* send out notify frame if we have catch frame matching tfs rule with notify action */
	if (idlist->len)
		wlc_wnm_send_tfs_notify_frame(wlc, bsscfg, scb, idlist);

	/* remove all tfs request */
	if (remove_tfs) {
		wlc_wnm_tfs_req_free(wlc->wnm_info, &wnm_scb->tfs_fset_head, -1);
	}

	return send_frame;
}
#endif /* WLWNM_AP */

static int
wlc_wnm_tclas_cnt_get(uint8 *buf, int buf_len)
{
	uint8 *p = buf;
	uint8 *p_end = buf + buf_len;
	bcm_tlv_t *tlv;
	int num = 0;

	while (p < p_end) {
		tlv = bcm_parse_tlvs(p, (int)(p_end - p), DOT11_MNG_TCLAS_ID);
		if (tlv == NULL)
			break;
		num++;
		p += tlv->len + TLV_HDR_LEN;
	}
	return num;
}

static bool
wlc_wnm_tclas_find(wnm_tclas_t *tclas_head, dot11_tclas_ie_t *tclas_ie)
{
	wnm_tclas_t *tclas;
	for (tclas = tclas_head; tclas; tclas = tclas->next) {
		if (tclas_ie->user_priority != tclas->user_priority ||
			tclas_ie->len - 1 != tclas->fc_len)
			continue;
		if (memcmp(&tclas_ie->fc, &tclas->fc, tclas->fc_len) == 0)
			return TRUE;
	}
	return FALSE;
}

static wnm_dms_desc_t *
wlc_wnm_dms_find_desc(wnm_dms_desc_t *dms_desc_head,
	int token, int dms_id, int status, uint8 *data, int data_len)
{
	wnm_dms_desc_t *dms_desc;
	int tclas_cnt = 0;
	int i;

	if (data_len)  {
		tclas_cnt = wlc_wnm_tclas_cnt_get(data, data_len);
		if (tclas_cnt == 0)
			return NULL;
	}

	for (dms_desc = dms_desc_head; dms_desc; dms_desc = dms_desc->next) {
#ifdef STA
		/* Compare token if provided */
		if (token && token != dms_desc->token)
			continue;

		/* Compare status if provided */
		if (status != -1 && status != dms_desc->status)
			continue;
#endif // endif
		/* Compare DMS ID if provided */
		if (dms_id && dms_id != dms_desc->dms_id)
			continue;

		if (data_len) {
			uint8 *data_tmp = data;
			/* Compare TCLAS count */
			if (dms_desc->tclas_cnt != tclas_cnt)
				continue;

			/* Check that each received TCLAS is matching an internal one */
			for (i = 0; i < tclas_cnt; i++) {
				dot11_tclas_ie_t *tclas_ie = (dot11_tclas_ie_t *)data_tmp;

				data_tmp += tclas_ie->len + TLV_HDR_LEN;

				if (!wlc_wnm_tclas_find(dms_desc->tclas_head, tclas_ie))
					break;
			}

			if (i < tclas_cnt)
				continue;

			/* Check that TCLAS Proc is present and matching, if several TCLAS */
			if (tclas_cnt > 1) {
				dot11_tclas_proc_ie_t *ie = (dot11_tclas_proc_ie_t *) data_tmp;

				if (data + data_len - data_tmp < DOT11_TCLAS_PROC_IE_LEN)
					continue;

				if (ie->process != dms_desc->tclas_proc)
					continue;
			}
		}

		/* Matched! */
		return dms_desc;
	}

	return NULL;
}

#ifdef WLWNM_AP
static int
wlc_wnm_dms_tclas_verdict(uint8 *data, int len)
{
	uint8 *data_end = data + len;
	int tclas_cnt = 0;
	int mcast_cnt = 0;

	while (data < data_end) {
		dot11_tclas_ie_t *ie = (dot11_tclas_ie_t *)data;
		dot11_tclas_fc_t *fc;
		if (ie->id != DOT11_MNG_TCLAS_ID)
			break;
		if (ie->len < 1 + DOT11_TCLAS_FC_MIN_LEN) {
			WL_WNM(("WNM TCLAS IE length incorrect\n"));
			return BCME_ERROR;
		}

		tclas_cnt++;
		fc = (dot11_tclas_fc_t *) &ie->fc;

		if (fc->hdr.type == DOT11_TCLAS_FC_0_ETH) {
			if (ie->len != 1 + DOT11_TCLAS_FC_0_ETH_LEN)
				return BCME_ERROR;
			/* deny non multicast request */
			if (ETHER_ISMULTI(fc->t0_eth.da))
				mcast_cnt++;
		}
		else if (fc->hdr.type == DOT11_TCLAS_FC_1_IP ||
			(fc->hdr.type == DOT11_TCLAS_FC_4_IP_HIGHER &&
			fc->t4_ipv4.version == IP_VER_4)) {
			uint32 ipv4 = ntoh32(fc->t1_ipv4.dst_ip);
			if (ie->len != 1 + DOT11_TCLAS_FC_1_IPV4_LEN)
				return BCME_ERROR;
			/* deny non multicast request */
			if (IPV4_ISMULTI(ipv4))
				mcast_cnt++;
		}
		else if (fc->hdr.type == DOT11_TCLAS_FC_4_IP_HIGHER &&
			fc->t4_ipv6.version == IP_VER_6) {
			if (ie->len != 1 + DOT11_TCLAS_FC_4_IPV6_LEN)
				return BCME_ERROR;
			/* deny non multicast request */
			if (fc->t4_ipv6.daddr[0] == 0xff)
				mcast_cnt++;
		}
		else {
			WL_WNM(("WNM TCLAS type not supported: %d\n", fc->hdr.type));
			return BCME_ERROR;
		}

		data += ie->len + TLV_HDR_LEN;
	}

	/* Check the TCLAS PROC IE and if we must have one or all MCAST TCLAS */
	if (tclas_cnt == 0)
		return BCME_ERROR;
	else if (tclas_cnt == 1) {
		if (data != data_end || mcast_cnt != 1)
			return BCME_ERROR;
	} else {
		dot11_tclas_proc_ie_t *ie = (dot11_tclas_proc_ie_t *)data;
		if (ie->id != DOT11_MNG_TCLAS_PROC_ID ||
			ie->len + 2 != DOT11_TCLAS_PROC_IE_LEN ||
			data + DOT11_TCLAS_PROC_IE_LEN != data_end)
			return BCME_ERROR;

		if (ie->process == DOT11_TCLAS_PROC_MATCHALL) {
			if (mcast_cnt == 0)
				return BCME_ERROR;
		}
		else if (ie->process == DOT11_TCLAS_PROC_MATCHONE) {
			if (mcast_cnt != tclas_cnt)
				return BCME_ERROR;
		}
		else
			return BCME_ERROR;
	}

	return BCME_OK;
}
#endif /* WLWNM_AP */

static void
wlc_wnm_dms_desc_free(wlc_info_t *wlc, wnm_dms_desc_t *dms_desc)
{
#ifdef WLWNM_AP
	wnm_dms_scb_t *dms_scb;
#endif /* WLWNM_AP */

	ASSERT(dms_desc);
#ifdef WLWNM_AP
	dms_scb = dms_desc->dms_scb_head;
	while (dms_scb) {
		wnm_dms_scb_t *next = dms_scb->next;
		MFREE(wlc->osh, dms_scb, sizeof(wnm_dms_scb_t));
		dms_scb = next;
	}
#endif /* WLWNM_AP */

	wnm_tclas_free(wlc, dms_desc->tclas_head);

	/* TSPEC not supported */
	/* DMS subelem not supported */

	MFREE(wlc->osh, dms_desc, sizeof(wnm_dms_desc_t));
}

#ifdef WLWNM_AP
/* Remove and specific DMS SCB from desc. Return NULL if no more SCB */
static wnm_dms_scb_t*
wlc_wnm_dms_scb_remove(wlc_info_t *wlc, wnm_dms_desc_t *dms_desc, struct scb *target)
{
	/* Trick to modify dms_scb_head seamlessly */
	wnm_dms_scb_t *dms_scb = (wnm_dms_scb_t *) &dms_desc->dms_scb_head;
	wnm_dms_scb_t *prev;

	ASSERT(target);

	do {
		prev = dms_scb;
		dms_scb = dms_scb->next;

		if (dms_scb == NULL)
			return dms_desc->dms_scb_head;
	} while (target != dms_scb->scb);

	prev->next = dms_scb->next;
	MFREE(wlc->osh, dms_scb, sizeof(wnm_dms_scb_t));
	return dms_desc->dms_scb_head;
}
#endif /* WLWNM_AP */

#ifdef STA
/* Remove and free specific DMS desc */
static void
wlc_wnm_dms_desc_remove(wlc_bsscfg_t *bsscfg, wnm_dms_desc_t *target)
{
	wnm_bsscfg_cubby_t* wnm_cfg = WNM_BSSCFG_CUBBY(bsscfg->wlc->wnm_info, bsscfg);
	/* Trick to modify dms_desc_head seamlessly */
	wnm_dms_desc_t *prev = (wnm_dms_desc_t *) &wnm_cfg->dms_info.dms_desc_head;
	wnm_dms_desc_t *dms_desc = prev;

	ASSERT(target);

	while (target != dms_desc) {
		if (dms_desc == NULL) {
			WL_WNM(("%s: DMS desc not found!\n", __FUNCTION__));
			return;
		}
		prev = dms_desc;
		dms_desc = dms_desc->next;
	}

	prev->next = dms_desc->next;
	wlc_wnm_dms_desc_free(bsscfg->wlc, dms_desc);
}
#endif /* STA */

static void
wlc_wnm_dms_free(wlc_bsscfg_t *bsscfg)
{
	wnm_bsscfg_cubby_t* wnm_cfg = WNM_BSSCFG_CUBBY(bsscfg->wlc->wnm_info, bsscfg);
	wnm_dms_desc_t *dms_desc = wnm_cfg->dms_info.dms_desc_head;
	while (dms_desc) {
		wnm_dms_desc_t *next = dms_desc->next;
		wlc_wnm_dms_desc_free(bsscfg->wlc, dms_desc);
		dms_desc = next;
	}

	wnm_cfg->dms_info.dms_desc_head = NULL;
}

#ifdef WLWNM_AP
static int
wlc_wnm_dms_req_parse_tclas(wlc_info_t *wlc, wnm_dms_desc_t *dms_desc, uint8 *body, uint8 body_len)
{
	int cnt = 0;

	/* For each TCLAS in the request, build a corresponding internal one */
	while (body_len) {
		wnm_tclas_t *tclas;
		dot11_tclas_ie_t *ie = (dot11_tclas_ie_t *)body;

		if (ie->id != DOT11_MNG_TCLAS_ID)
			break;

		if (TLV_HDR_LEN + ie->len > body_len) {
			WL_WNM(("wl%d: %s: TCLAS len error\n", wlc->pub->unit, __FUNCTION__));
			return BCME_ERROR;
		}

		tclas = MALLOCZ(wlc->osh, TCLAS_ELEM_FIXED_SIZE + ie->len - 1);
		if (tclas == NULL) {
			WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__,
			          (int)(TCLAS_ELEM_FIXED_SIZE + ie->len - 1), MALLOCED(wlc->osh)));
			return BCME_NOMEM;
		}

		tclas->user_priority = ie->user_priority;
		tclas->fc_len = ie->len - 1;
		memcpy(&tclas->fc, &ie->fc, tclas->fc_len);

		WNM_ELEMENT_LIST_ADD(dms_desc->tclas_head, tclas);

		body += TLV_HDR_LEN + ie->len;
		body_len -= TLV_HDR_LEN + ie->len;
		cnt++;
	}

	dms_desc->tclas_cnt += cnt;

	/* Parse TC Proc if several TCLAS */
	if (cnt > 1) {
		dot11_tclas_proc_ie_t *proc_ie = (dot11_tclas_proc_ie_t *)body;
		if (proc_ie->id != DOT11_MNG_TCLAS_PROC_ID ||
			proc_ie->len != 1 ||
			body_len < DOT11_TCLAS_PROC_IE_LEN)
			return BCME_ERROR;

		dms_desc->tclas_proc = proc_ie->process;

		body += DOT11_TCLAS_PROC_IE_LEN;
		body_len -= DOT11_TCLAS_PROC_IE_LEN;
	}

	/* TSPEC not supported */
	/* DMS subelem not supported */

	return BCME_OK;
}

static int
wlc_wnm_dms_desc_add_scb(wlc_info_t *wlc, wnm_dms_desc_t *dms_desc, struct scb *scb)
{
	wnm_dms_scb_t *dms_scb;

	for (dms_scb = dms_desc->dms_scb_head; dms_scb; dms_scb = dms_scb->next)
		if (bcmp(&dms_scb->scb->ea, &scb->ea, ETHER_ADDR_LEN) == 0)
			return BCME_OK;

	/* This scb is not in the dms_desc's dms_scb list */
	dms_scb = MALLOCZ(wlc->osh, sizeof(wnm_dms_scb_t));
	if (dms_scb == NULL) {
		WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__, (int)sizeof(wnm_dms_scb_t),
			MALLOCED(wlc->osh)));
		return BCME_NOMEM;
	}

	/* Attach this dms_scb into dms_desc's dms_scb list */
	dms_scb->scb = scb;
	WNM_ELEMENT_LIST_ADD(dms_desc->dms_scb_head, dms_scb);

	return BCME_OK;
}

static int
wlc_wnm_dms_req_parse(wlc_bsscfg_t *bsscfg, struct scb *scb,
	dot11_dms_req_desc_t *dms_req_desc, dot11_dms_resp_st_t *dms_stat, int *dms_stat_len)
{
	wlc_info_t *wlc = bsscfg->wlc;
	wlc_wnm_info_t *wnm = wlc->wnm_info;
	wnm_bsscfg_cubby_t* wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);
	wnm_dms_info_t *dms_info = &wnm_cfg->dms_info;
	wnm_dms_desc_t *dms_desc;
	int ret;

	if (dms_req_desc->type == DOT11_DMS_REQ_TYPE_ADD) {
		wnm_scb_cubby_t *wnm_scb = SCB_WNM_CUBBY(wlc->wnm_info, scb);

		if (dms_req_desc->len < 1 + DOT11_TCLAS_IE_LEN + DOT11_TCLAS_FC_MIN_LEN)
			goto deny;

		/* If A-MSDU PP and SPP are both forbidden, DMS cannot be set */
		if (WSEC_AES_ENABLED(bsscfg->wsec) && wnm_scb->spp_conflict == TRUE)
			goto deny;

		ret = wlc_wnm_dms_tclas_verdict(dms_req_desc->data, dms_req_desc->len - 1);
		if (ret != BCME_OK)
			goto deny;

		/* Check if this dms_req_desc is already existing or not */
		dms_desc = wlc_wnm_dms_find_desc(dms_info->dms_desc_head, 0, 0, -1,
			dms_req_desc->data, dms_req_desc->len - 1);

		if (dms_desc) {
			ret = wlc_wnm_dms_desc_add_scb(wlc, dms_desc, scb);
			if (ret != BCME_OK)
				goto deny;
		} else {
			/* Allocate new internal desc */
			dms_desc = MALLOCZ(wlc->osh, sizeof(wnm_dms_desc_t));
			if (dms_desc == NULL) {
				WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__,
					(int)sizeof(wnm_dms_desc_t), MALLOCED(wlc->osh)));
				goto deny;
			}

			/* Add STA's SCB */
			ret = wlc_wnm_dms_desc_add_scb(wlc, dms_desc, scb);
			if (ret != BCME_OK)
				goto free_deny;

			/* Add TCLASs */
			ret = wlc_wnm_dms_req_parse_tclas(wlc, dms_desc,
				dms_req_desc->data, dms_req_desc->len - 1);
			if (ret != BCME_OK) {
			free_deny:
				wlc_wnm_dms_desc_free(wlc, dms_desc);
				goto deny;
			}

			/* Choose new DMS ID */
			WLC_WNM_UPDATE_TOKEN(dms_info->dms_id_new);
			dms_desc->dms_id = dms_info->dms_id_new;

			/* Add this dms_desc into dms_desc_head */
			WNM_ELEMENT_LIST_ADD(dms_info->dms_desc_head, dms_desc);
		}

		/* Prepare dms resp status field , last seq num set below */
		dms_stat->type = DOT11_DMS_RESP_TYPE_ACCEPT;
		dms_stat->dms_id = dms_desc->dms_id;

		/* Copy tclas, tclas proc, tspec, opt subelem from the req */
		memcpy(dms_stat->data, dms_req_desc->data, dms_req_desc->len - 1);
		dms_stat->len = 3 + (dms_req_desc->len - 1);

	} else if (dms_req_desc->type == DOT11_DMS_REQ_TYPE_REMOVE) {
		/* Trick to modify dms_desc_head seamlessly */
		wnm_dms_desc_t *prev = (wnm_dms_desc_t *) &dms_info->dms_desc_head;

		for (dms_desc = prev->next; dms_desc;
			prev = dms_desc, dms_desc = dms_desc->next) {
			if (dms_req_desc->dms_id == dms_desc->dms_id)
				break;
		}

		if (dms_desc == NULL || dms_req_desc->len != 1) {
			dms_req_desc->len = 1;
			goto deny;
		}

		/* Prepare dms resp status field */
		dms_stat->type = DOT11_DMS_RESP_TYPE_TERM;
		dms_stat->dms_id = dms_desc->dms_id;
		dms_stat->len = 3;

		/* Remove matching SCB from DMS desc, and remove desc if no more SCB */
		if (wlc_wnm_dms_scb_remove(wlc, dms_desc, scb) == NULL) {
			prev->next = dms_desc->next;
			wlc_wnm_dms_desc_free(wlc, dms_desc);
			dms_desc = NULL;
		}

	} else if (dms_req_desc->type == DOT11_DMS_REQ_TYPE_CHANGE) {
		/* Change only for TSPEC: not supported */
		WL_WNM(("wl%d: %s: dms descriptor req type change\n",
			wlc->pub->unit, __FUNCTION__));
		deny:
			dms_stat->type = DOT11_DMS_RESP_TYPE_DENY;
			dms_stat->dms_id = 0;
			memcpy(dms_stat->data, dms_req_desc->data, dms_req_desc->len - 1);
			dms_stat->len = 3 + (dms_req_desc->len - 1);
	} else {
		WL_WNM(("wl%d: %s: forbidden dms desc req_type %d\n",
			wlc->pub->unit, __FUNCTION__, dms_req_desc->type));
		return BCME_ERROR;
	}

	/* If lsc is not supported, set it to be 0xFFFF */
	htol16_ua_store(0xFFFF, (uint8 *) &dms_stat->lsc);

	*dms_stat_len = 2 + dms_stat->len;

	return BCME_OK;
}

static int
wlc_wnm_dms_req_parse_resp_send(wlc_bsscfg_t *bsscfg, struct scb *scb, uint8 *req_body, int req_len)
{
	wlc_info_t *wlc = bsscfg->wlc;
	dot11_dms_req_t *dms_req_frame = (dot11_dms_req_t *)req_body;

	uint8 *resp_body;
	int resp_offset = 0;
	int req_offset = 0;
	dot11_dms_resp_t *dms_resp_frame;

	/* Allocate twice the size of req frame since resp is bigger than req */
	void *p = wlc_frame_get_action(wlc, &scb->ea, &bsscfg->cur_etheraddr,
		&bsscfg->BSSID, req_len * 2, &resp_body, DOT11_ACTION_CAT_WNM);

	if (p == NULL)
		return BCME_NOMEM;

	/* Prepare DMS response frame fields */
	dms_resp_frame = (dot11_dms_resp_t *)resp_body;
	dms_resp_frame->category = DOT11_ACTION_CAT_WNM;
	dms_resp_frame->action = DOT11_WNM_ACTION_DMS_RESP;
	dms_resp_frame->token = dms_req_frame->token;

	if (dms_req_frame->token == 0) {
		WL_WNM(("WNM DMS request token error\n"));
		goto proc_error;
	}

	/* Jump to DMS req/resp elements */
	req_offset += DOT11_DMS_REQ_LEN;
	resp_offset += DOT11_DMS_RESP_LEN;

	/* Parse each request IE and build corresponding resp IE */
	while (req_offset != req_len) {
		dot11_dms_req_ie_t *req_ie = (dot11_dms_req_ie_t *) (req_body + req_offset);
		uint req_ie_offset = 0;

		dot11_dms_resp_ie_t *resp_ie = (dot11_dms_resp_ie_t *) (resp_body + resp_offset);
		uint8 *resp_stat = resp_ie->data;
		int resp_stat_len;

		if (TLV_HDR_LEN + req_ie->len > req_len) {
			WL_WNM(("WNM DMS request IE len Error\n"));
			goto proc_error;
		}

		if (req_ie->id != DOT11_MNG_DMS_REQUEST_ID) {
			WL_WNM(("WNM DMS request IE ID Error\n"));
			goto proc_error;
		}

		/* Parse each request desc and build corresponding resp status */
		while (req_ie_offset != req_ie->len) {
			dot11_dms_req_desc_t *dms_req_desc =
				(dot11_dms_req_desc_t *) (req_ie->data + req_ie_offset);

			/* Make sure dms_req_desc length is valid (1 + length of TCLAS elements). */
			if ((dms_req_desc->len == 0) ||
			    (2 + dms_req_desc->len > req_ie->len - req_ie_offset)) {
				WL_WNM(("WNM DMS request desc len Error\n"));
				goto proc_error;
			}

			if (wlc_wnm_dms_req_parse(bsscfg, scb, dms_req_desc,
				(dot11_dms_resp_st_t *) resp_stat, &resp_stat_len)) {
				goto proc_error;
			}

			req_ie_offset += 2 + dms_req_desc->len;

			/* If resp IE too big, add new resp IE and move prev status in it */
			if (resp_stat + resp_stat_len - (uint8 *) resp_ie >
				TLV_HDR_LEN + 255) {
				memmove(resp_stat + TLV_HDR_LEN, resp_stat, resp_stat_len);
				resp_ie->id = DOT11_MNG_DMS_RESPONSE_ID;
				resp_ie->len = resp_stat - resp_ie->data;
				resp_ie = (dot11_dms_resp_ie_t *) resp_stat;
			}
			resp_stat += resp_stat_len;
		}

		/* Finalize this IE */
		resp_ie->id = DOT11_MNG_DMS_RESPONSE_ID;
		resp_ie->len = resp_stat - resp_ie->data;

		req_offset += TLV_HDR_LEN + req_ie->len;
		resp_offset += TLV_HDR_LEN + resp_ie->len;
	}

	/* We do not know what is the header length as MFP can be used */
	PKTSETLEN(wlc->osh, p, resp_body - PKTDATA(wlc->osh, p) + resp_offset);

	wlc_sendmgmt(wlc, p, bsscfg->wlcif->qi, NULL);

	return BCME_OK;

proc_error:
	PKTFREE(wlc->osh, p, FALSE);
	return BCME_ERROR;
}
#endif /* WLWNM_AP */

#ifdef STA
/* Different DNS frame responses type can be:
 * - rsp to _REQ_TYPE_ADD, token != 0, status is _RESP_TYPE_ACCEPT or _RESP_TYPE_DENY, with TCLAS
 * - rsp to _REQ_TYPE_REMOVE, token != 0, status is _RESP_TYPE_TERM or _RESP_TYPE_DENY, with TCLAS
 * - non sollicited rsp, token == 0, status is _RESP_TYPE_TERM, no TCLAS
 * - _REQ_TYPE_CHANGE is not supported
 */
static int
wlc_wnm_dms_resp_frame_parse(wlc_bsscfg_t *bsscfg, int token, uint8 *body, int body_len)
{
	wlc_info_t *wlc = bsscfg->wlc;
	wnm_bsscfg_cubby_t* wnm_cfg = WNM_BSSCFG_CUBBY(wlc->wnm_info, bsscfg);
	wnm_dms_info_t *dms_info = &wnm_cfg->dms_info;
	uint8 *body_end = body + body_len;
	wnm_dms_desc_t *dms_desc;

	while (body < body_end) {
		dot11_dms_resp_st_t *dms_resp_st = (dot11_dms_resp_st_t *)body;
		int status = DMS_STATUS_IN_PROGRESS;

		if (dms_resp_st->len < DOT11_DMS_RESP_STATUS_LEN - 2)
			return BCME_ERROR;

		if (dms_resp_st->type > DOT11_DMS_RESP_TYPE_TERM)
			return BCME_ERROR;

		if (dms_resp_st->type == DOT11_DMS_RESP_TYPE_DENY)
			WL_WNM(("wl%d: %s: DMS req token %d denied by AP\n",
				wlc->pub->unit, __FUNCTION__, token));

		if (dms_resp_st->type == DOT11_DMS_RESP_TYPE_TERM) {
			if (dms_resp_st->len != DOT11_DMS_RESP_STATUS_LEN - 2)
				return BCME_ERROR;
			if (token == 0)
				status = DMS_STATUS_ACCEPTED;
		}

		dms_desc = wlc_wnm_dms_find_desc(dms_info->dms_desc_head, token, 0,
			status, dms_resp_st->data, dms_resp_st->len - 3);

		if (dms_desc == NULL && dms_resp_st->type == DOT11_DMS_RESP_TYPE_DENY) {
			/* If TCLAS was modified by AP, we retry and only check by token
				* and status.
				* This can be a problem if several descriptors were sent and
				* corresponding status fields are not sent in the same order.
				*/
			dms_desc = wlc_wnm_dms_find_desc(dms_info->dms_desc_head, token, 0,
				status, NULL, 0);
		}

		if (dms_desc == NULL) {
			/* This is possible when we removed the request before we got
			 *  response from AP. Send remove req to AP.
			 */
			if (dms_resp_st->type == DOT11_DMS_RESP_TYPE_ACCEPT) {
				uint8 *p, *req_body;
				dot11_dms_req_desc_t *desc;

				/* Received success without outstanding req; send remove req */
				p = (uint8 *)wlc_frame_get_dms_req(wlc, wlc->wnm_info,
					DOT11_DMS_REQ_DESC_LEN, &req_body);

				if (p == NULL)
					return BCME_ERROR;
				/* Prepare DMS request element fields */
				desc = (dot11_dms_req_desc_t *)req_body;
				desc->dms_id = dms_resp_st->dms_id;
				desc->len = DOT11_DMS_REQ_DESC_LEN - TLV_HDR_LEN;
				desc->type = DOT11_DMS_REQ_TYPE_REMOVE;
				wlc_sendmgmt(wlc, p, wlc->cfg->wlcif->qi, NULL);
				body += dms_resp_st->len + TLV_HDR_LEN;
				continue;
			}

			WL_WNM(("wl%d: %s: DMS status in AP response not matching\n",
				wlc->pub->unit, __FUNCTION__));
			return BCME_ERROR;
		}

		if (dms_desc->dms_id == 0) {
			/* This was an ADD requested */
			if (dms_resp_st->type == DOT11_DMS_RESP_TYPE_ACCEPT)
				dms_desc->status = DMS_STATUS_ACCEPTED;
			else
				dms_desc->status = DMS_STATUS_DENIED;
			dms_desc->dms_id = dms_resp_st->dms_id;

		} else {
			if (token) {
				/* This was a REMOVE requested by the STA */
				if (dms_desc->del) {
					/* User want to discard the desc */
					/* TODO: do this later after handling last_seqctrl */
					wlc_wnm_dms_desc_remove(bsscfg, dms_desc);
					dms_desc = NULL;
				} else {
					dms_desc->status = DMS_STATUS_DISABLED;
					dms_desc->dms_id = 0;
					dms_desc->last_seqctrl = dms_resp_st->lsc;
					dms_desc->token = 0;
				}
			} else {
				/* The AP decided to terminate this DMS desc */
				dms_desc->status = DMS_STATUS_TERM;
				dms_desc->dms_id = 0;
				dms_desc->last_seqctrl = dms_resp_st->lsc;
			}
		}

		/* TSPEC not supported */
		/* DMS subelem not supported */

		body += dms_resp_st->len + TLV_HDR_LEN;
	}

	return BCME_OK;
}
#endif /* STA */

static int
wlc_wnm_dms_scb_cleanup(wlc_info_t *wlc, struct scb *scb)
{
	wnm_bsscfg_cubby_t* wnm_cfg;
	wnm_dms_desc_t *prev, *dms_desc;

	if (wlc->wnm_info && scb && SCB_BSSCFG(scb))
		wnm_cfg = WNM_BSSCFG_CUBBY(wlc->wnm_info, SCB_BSSCFG(scb));
	else
		return BCME_OK;

	/* Trick to modify dms_desc_head seamlessly */
	prev = (wnm_dms_desc_t *) &wnm_cfg->dms_info.dms_desc_head;

	/* Try to remove SCB from all desc, if desc has no more scb free it */
	for (dms_desc = prev->next; dms_desc;
		prev = dms_desc, dms_desc = dms_desc->next) {

		/* TODO: handle simultaneous AP + STA config */
#ifdef WLWNM_AP
		/* For AP, try to remove SCB's STA from all desc */
		if (BSSCFG_AP(SCB_BSSCFG(scb)))
			if (wlc_wnm_dms_scb_remove(wlc, dms_desc, scb) == NULL) {
				prev->next = dms_desc->next;
				wlc_wnm_dms_desc_free(wlc, dms_desc);
				dms_desc = prev;
			}
#endif // endif
#ifdef STA
		/* For STA, set all desc as disabled for next association */
		if (BSSCFG_STA(SCB_BSSCFG(scb))) {
			if (dms_desc->del) {
				prev->next = dms_desc->next;
				wlc_wnm_dms_desc_free(wlc, dms_desc);
				/* Next iteration need valid dms_desc */
				dms_desc = prev;
			} else if (dms_desc->status != DMS_STATUS_DISABLED) {
				dms_desc->status = DMS_STATUS_NOT_ASSOC;
				dms_desc->dms_id = 0;
			}
		}
#endif // endif
	}
#ifdef STA
	if (wnm_cfg->timbc_stat.status_sta != WL_TIMBC_STATUS_DISABLE) {
		wnm_cfg->timbc_stat.status_sta = WL_TIMBC_STATUS_NOT_ASSOC;
		/* TODO: disable ucode reg if needed? */
	}
#endif // endif
	return BCME_OK;
}

#ifdef BCMDBG
static int
wlc_wnm_tclas_delete(wlc_info_t *wlc, wnm_tclas_t **head, uint32 idx, uint32 cnt)
{
	/* Trick to simplify processing, as 'next' is the first member of the struct */
	wnm_tclas_t *prev = (wnm_tclas_t *)head;

	while (idx--) {
		prev = prev->next;
		if (prev == NULL)
			return BCME_RANGE;
	}

	while (cnt--) {
		wnm_tclas_t *curr = prev->next;
		if (curr == NULL)
			return BCME_RANGE;

		prev->next = curr->next;
		MFREE(wlc->osh, curr, TCLAS_ELEM_FIXED_SIZE + curr->fc_len);
	}

	return BCME_OK;
}
#endif /* BCMDBG */

static int
wlc_wnm_tclas_ie_prep(wnm_tclas_t *tclas, uint8 *p)
{
	dot11_tclas_ie_t *ie;
	int retlen = 0;

	for (; tclas; tclas = tclas->next) {
		ie = (dot11_tclas_ie_t *)p;
		ie->id = DOT11_MNG_TCLAS_ID;
		ie->len = tclas->fc_len + 1;
		ie->user_priority = tclas->user_priority;
		memcpy(&ie->fc, &tclas->fc, tclas->fc_len);

		p += ie->len + TLV_HDR_LEN;
		retlen += ie->len + TLV_HDR_LEN;
	}

	return retlen;
}

#ifdef WLWNM_AP
static int
wlc_wnm_parse_sleep_req_ie(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb, uint8 *body,
	int body_len, uint8 *buf, int *buf_len)
{
	dot11_wnm_sleep_ie_t *req_ie = (dot11_wnm_sleep_ie_t *)body;
	dot11_wnm_sleep_ie_t *resp_ie = (dot11_wnm_sleep_ie_t *)buf;
	int tfs_resp_len = 0;
	bool sleeping = FALSE;
	uint16 sleep_interval = 0;
	wnm_scb_cubby_t *wnm_scb = SCB_WNM_CUBBY(wlc->wnm_info, scb);

	if (req_ie->id != DOT11_MNG_WNM_SLEEP_MODE_ID) {
		WL_WNM(("sleep_ie ID error (%d)\n", req_ie->id));
		return BCME_ERROR;
	}

	/* move to TFS IE start position */
	body += (DOT11_WNM_SLEEP_IE_LEN + TLV_HDR_LEN);
	body_len -= (DOT11_WNM_SLEEP_IE_LEN + TLV_HDR_LEN);

	if (body_len < 0 || req_ie->len != DOT11_WNM_SLEEP_IE_LEN) {
		WL_WNM(("sleep_ie length error (%d), bodylen (%d)\n", req_ie->len, body_len));
		return BCME_ERROR;
	}

	/* setup sleep resp ie */
	bcopy((void *)req_ie, (void *)resp_ie, DOT11_WNM_SLEEP_IE_LEN + TLV_HDR_LEN);
	resp_ie->resp_status = DOT11_WNM_SLEEP_RESP_ACCEPT;

	buf += (DOT11_WNM_SLEEP_IE_LEN + TLV_HDR_LEN);
	*buf_len = (DOT11_WNM_SLEEP_IE_LEN + TLV_HDR_LEN);

	if (req_ie->act_type == DOT11_WNM_SLEEP_ACT_TYPE_ENTER) {
		sleeping = TRUE;
		sleep_interval = ltoh16_ua(&(req_ie->interval));
		if (body_len < DOT11_TFS_REQ_IE_LEN) {
			WL_WNM(("Missing TFS ie in sleep_request %d\n", body_len));
			resp_ie->resp_status = DOT11_WNM_SLEEP_RESP_DENY;
			return BCME_ERROR;
		}
	}
	else if (req_ie->act_type == DOT11_WNM_SLEEP_ACT_TYPE_EXIT) {
		sleeping = FALSE;
		/* Reserved field */
		htol16_ua_store(0, &resp_ie->interval);
#ifdef MFP
		if (WLC_MFP_ENAB(wlc->pub) && SCB_MFP(scb) && wnm_scb->key_exp) {
			resp_ie->resp_status = DOT11_WNM_SLEEP_RESP_UPDATE;
			wnm_scb->key_exp = FALSE;
		}
#endif /* MFP */
	}
	else {
		WL_WNM(("STA action type incorrect(%d)\n", req_ie->act_type));
		resp_ie->resp_status = DOT11_WNM_SLEEP_RESP_DENY;
		return BCME_OK;
	}

	if (body_len > 0) {
		/* parsing TFS ie to check the whole request is valid */
		if (wlc_wnm_parse_tfs_req_ie(bsscfg->wlc, scb, body, body_len,
		    buf, &tfs_resp_len) != BCME_OK) {
			WL_WNM(("TFS param in sleep req Error\n"));
			resp_ie->resp_status = DOT11_WNM_SLEEP_RESP_DENY;
			return BCME_OK;
		}
		*buf_len += tfs_resp_len;
	}

	if (resp_ie->resp_status <= DOT11_WNM_SLEEP_RESP_UPDATE) {
		/* write to scb if the request is permit */
		wnm_scb->sleeping = sleeping;
		wnm_scb->sleep_interval = sleep_interval;

		/* sendup STA_SLEEP event for NAS to monitor if STA needs to rekey or not */
		wlc_bss_mac_event(bsscfg->wlc, bsscfg, WLC_E_WNM_STA_SLEEP, &scb->ea,
			wnm_scb->sleeping? 1: 0, SCB_MFP(scb), 0, NULL, 0);
	}

	WL_WNM(("SLEEP mode: %s, interval(%d) - %d\n", req_ie->act_type? "Exit": "Enter",
		wnm_scb->sleep_interval, *buf_len));

	return BCME_OK;
}

#ifdef MFP
static int
wlc_wnm_prep_keydata_ie(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb, uint8 *p)
{
	dot11_wnm_sleep_subelem_gtk_t *gtk = (dot11_wnm_sleep_subelem_gtk_t *)p;
	dot11_wnm_sleep_subelem_igtk_t *igtk;
	wlc_key_t *key;
	wlc_key_info_t key_info;

	/* Get GTK */
	key = wlc_keymgmt_get_bss_tx_key(wlc->keymgmt, bsscfg, FALSE, &key_info);

	/* construct GTK info */
	gtk->sub_id = DOT11_WNM_SLEEP_SUBELEM_ID_GTK;
	gtk->len = DOT11_WNM_SLEEP_SUBELEM_GTK_FIXED_LEN + key_info.key_len;
	gtk->key_length = key_info.key_len;

	ASSERT(bsscfg == wlc_bsscfg_primary(wlc));
	ASSERT(!(bsscfg->WPA_auth & WPA_AUTH_NONE));

	/* set gtk key id, data and rsc/seq  */
	htol16_ua_store(key_info.key_id, &(gtk->key_info));
	wlc_key_get_data(key, gtk->key, key_info.key_len, NULL);
	(void)wlc_key_get_seq(key, gtk->rsc, 6, 0, FALSE);
	htol16_ua_store(0, gtk->rsc + 6);

	/* construct IGTK info */
	igtk = (dot11_wnm_sleep_subelem_igtk_t *)(p + TLV_HDR_LEN + gtk->len);
	igtk->sub_id = DOT11_WNM_SLEEP_SUBELEM_ID_IGTK;
	igtk->len = DOT11_WNM_SLEEP_SUBELEM_IGTK_LEN;

	key = wlc_keymgmt_get_bss_tx_key(wlc->keymgmt, bsscfg, TRUE /* igtk */, &key_info);

	/* set igtk key id, data and rsc/seq  */
	htol16_ua_store(key_info.key_id, (uint8*)&(igtk->key_id));
	wlc_key_get_data(key, igtk->key, key_info.key_len, NULL);
	(void)wlc_key_get_seq(key, igtk->pn, 6, 0, FALSE);

	return 2 * TLV_HDR_LEN + gtk->len + igtk->len;
}
#endif /* MFP */

static int
wlc_wnm_send_sleep_resp_frame(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb,
	uint8 token, uint8 *buf, int buf_len)
{
	void *p;
	uint8 *pbody;
	int body_len = DOT11_WNM_SLEEP_RESP_LEN + buf_len;
	uint16 key_len = 0;
	dot11_wnm_sleep_resp_t *resp;
#ifdef MFP
	/* If scb has MFP, allocate packet with maximum key length */
	if (WLC_MFP_ENAB(wlc->pub)) {
		dot11_wnm_sleep_ie_t *resp_ie = (dot11_wnm_sleep_ie_t *)buf;
		if (SCB_MFP(scb) && wlc->mfp &&
			resp_ie->resp_status <= DOT11_WNM_SLEEP_RESP_UPDATE) {
			key_len = (TLV_HDR_LEN + DOT11_WNM_SLEEP_SUBELEM_GTK_MAX_LEN) +
				(TLV_HDR_LEN + DOT11_WNM_SLEEP_SUBELEM_IGTK_LEN);
		}
	}
#endif /* MFP */
	if ((p = wlc_frame_get_action(wlc, &scb->ea, &bsscfg->cur_etheraddr,
		&bsscfg->BSSID, body_len + key_len, &pbody, DOT11_ACTION_CAT_WNM)) == NULL) {
		return BCME_ERROR;
	}

	/* Prepare DMS response frame fields */
	resp = (dot11_wnm_sleep_resp_t *)pbody;
	resp->category = DOT11_ACTION_CAT_WNM;
	resp->action = DOT11_WNM_ACTION_WNM_SLEEP_RESP;
	resp->token = token;

	/* shift to key data field */
	pbody += DOT11_WNM_SLEEP_RESP_LEN;
#ifdef MFP
	if (WLC_MFP_ENAB(wlc->pub) && key_len) {
		/* scb has MFP, add key and update key_len to exact length */
		key_len = wlc_wnm_prep_keydata_ie(wlc, bsscfg, scb, pbody);
		/* shift to SLEEP IE field */
		pbody += key_len;
	}
#endif /* MFP */
	htol16_ua_store(key_len, (uint8*)&(resp->key_len));

	memcpy(pbody, buf, buf_len);
	pbody += buf_len;

	PKTSETLEN(wlc->osh, p, pbody - PKTDATA(wlc->osh, p));
	wlc_sendmgmt(wlc, p, bsscfg->wlcif->qi, scb);

	return BCME_OK;
}

#ifdef MFP
void
wlc_wnm_sleep_key_update(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	struct scb *scb;
	struct scb_iter scbiter;
	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
		wnm_scb_cubby_t *wnm_scb = SCB_WNM_CUBBY(wlc->wnm_info, scb);

		if (SCB_MFP(scb) && wnm_scb->sleeping)
			wnm_scb->key_exp = TRUE;
	}
}
#endif /* MFP */

static void
wlc_wnm_bsstrans_req_timer(void *arg)
{
	wlc_bsscfg_t *bsscfg = (wlc_bsscfg_t *)arg;
	wlc_info_t *wlc = bsscfg->wlc;
	wnm_bsscfg_cubby_t *wnm_cfg = WNM_BSSCFG_CUBBY(wlc->wnm_info, bsscfg);
	wnm_bsstrans_req_info_t *bsstrans = &wnm_cfg->bsstrans_req_info;

	switch (bsstrans->timer_state) {
	case BSSTRANS_WAIT_FOR_BSS_TERM:
		/* for response with reject status bit 0 and one bit x correspond to each
		 * reject status will be set except bit 6(BSSTRANS_RESP_STATUS_REJ_TERM_DELAY_REQ)
		 */
		if (!(bsstrans->status & (1 << DOT11_BSSTRANS_RESP_STATUS_REJ_TERM_DELAY_REQ))) {
			struct scb_iter scbiter;
			struct scb *scb = NULL;

			/* Termination accepted, disassoc all STAs and disable this bsscfg */
			FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
				if (!SCB_ASSOCIATED(scb))
					continue;
				wlc_senddisassoc(wlc, bsscfg, scb, &scb->ea, &bsscfg->BSSID,
						&bsscfg->cur_etheraddr, DOT11_RC_BUSY);
				if (!SCB_PS(scb)) {
					wlc_scb_resetstate(wlc, scb);
					wlc_scb_setstatebit(wlc, scb, AUTHENTICATED);

					wlc_bss_mac_event(wlc, bsscfg, WLC_E_DISASSOC_IND, &scb->ea,
					WLC_E_STATUS_SUCCESS, DOT11_RC_BUSY, 0, NULL, 0);
				} else {
					/* reset the state once disassoc packet being transmit at
					 * reception of ps poll frame or QoS NUll packet from
					 * client
					 */
					wlc_apps_set_change_scb_state(wlc, scb, TRUE);
				}
			}

			if (bsstrans->dur) {
				/* wait for msec_to_bss_down msec to bring down
				 * the bsscfg, timer is for allowing all disassoc packets to drain
				 * out.
				 * TODO: Implement packet callback mechanism from wlc_dotxstatus
				 * to call wlc_bsscfg_disable once all sta receives disassoc frame
				 * for disassoc imminent bit or bss termination bit in reqmode
				 */
				uint msec = wnm_cfg->msec_to_bss_down;
				/* Schedule the timer again to enable this bsscfg */
				wl_add_timer(wlc->wl, bsstrans->timer, msec, 0);

				bsstrans->timer_state = BSSTRANS_WAIT_FOR_BSS_DOWN;
			}
			else {
				/* no BSS Termination included, we don't need to timer for wakeup */
				bsstrans->timer_state = BSSTRANS_INIT;
			}
		} else {
			/* atleast one sta respond back with reject with delay status, if delay
			 * is configured, initiate the timer or else reset the state machine
			 * to BSSTRANS_INIT
			 */
			if (bsstrans->delay > 0) {
				/* STA requests BSS termination delay minutes */
				wl_add_timer(wlc->wl, bsstrans->timer,
					bsstrans->delay * 60000, 0);

				bsstrans->timer_state = BSSTRANS_WAIT_FOR_BSS_TERM_DELAY;
			} else {
				/* Init the timer state to mark the term transaction is finished
				 * since at least one STA rejects BSS termination.
				 */
				bsstrans->timer_state = BSSTRANS_INIT;
			}
		}
		break;
	case BSSTRANS_WAIT_FOR_BSS_TERM_DELAY: {
		struct scb_iter scbiter;
		struct scb *scb = NULL;

		/* Force termination, disassoc all STAs and disable this bsscfg */
		FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
			if (!SCB_ASSOCIATED(scb))
				continue;

			wlc_senddisassoc(wlc, bsscfg, scb, &scb->ea, &bsscfg->BSSID,
				&bsscfg->cur_etheraddr, DOT11_RC_BUSY);
			wlc_scb_resetstate(wlc, scb);
			wlc_scb_setstatebit(wlc, scb, AUTHENTICATED);

			wlc_bss_mac_event(wlc, bsscfg, WLC_E_DISASSOC_IND, &scb->ea,
				WLC_E_STATUS_SUCCESS, DOT11_RC_BUSY, 0, NULL, 0);
		}
		/* Disassoc all STAs and disable this bsscfg */
		wlc_bsscfg_disable(wlc, bsscfg);

		/* Schedule the timer again to enable this bsscfg */
		wl_add_timer(wlc->wl, bsstrans->timer, bsstrans->dur * 60000, 0);

		bsstrans->timer_state = BSSTRANS_WAIT_FOR_BSS_ENABLED;

		break;
	}
	case BSSTRANS_WAIT_FOR_BSS_DOWN:
		wlc_bsscfg_disable(wlc, bsscfg);

		/* Schedule the timer again to enable this bsscfg */
		wl_add_timer(wlc->wl, bsstrans->timer, bsstrans->dur * 60000, 0);

		bsstrans->timer_state = BSSTRANS_WAIT_FOR_BSS_ENABLED;
		break;
	case BSSTRANS_WAIT_FOR_BSS_ENABLED:
		/* Enable this bsscfg and delete this timer */
		wlc_bsscfg_enable(wlc, bsscfg);

		bsstrans->timer_state = BSSTRANS_INIT;
		break;
	default:
		break;
	}
}

static void
wlc_wnm_bsstrans_disassoc_timer(void *arg)
{
	wlc_bsscfg_t *bsscfg = (wlc_bsscfg_t *)arg;
	wlc_info_t *wlc = bsscfg->wlc;
	wnm_bsscfg_cubby_t *wnm_cfg = WNM_BSSCFG_CUBBY(wlc->wnm_info, bsscfg);
	struct scb *scb;

	if (!(scb = wlc_scbfind(wlc, bsscfg, &wnm_cfg->sta_mac)) ||
		(!SCB_ASSOCIATED(scb))) {
		return;
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

	wnm_cfg->disassoc_timer_state = BSSTRANS_INIT;
}
/* last 3 arguments are MBO specific extra argument
 * delay - delay in seconds to avoid join process with BSS
 * reason - reason for steering
 * add_self - allocate one extra neighbor report element space to be
 *		  filled with BSS's own information.
 */
static void
wlc_wnm_send_bsstrans_request(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg, struct scb *scb,
	uint8 token, uint8 reqmode, uint8 delay, uint8 reason, bool add_self)
{
	wlc_info_t *wlc = wnm->wlc;
	wnm_bsscfg_cubby_t *wnm_cfg;
	wnm_bsstrans_req_info_t *bsstrans;
	dot11_bsstrans_req_t *transreq;
	void *p;
	uint8 *pbody, *pdata, *bufend;
	int maxlen;
#if defined(WL_MBO) && !defined(WL_MBO_DISABLED) && defined(MBO_AP)
	bool assoc_retry_attr = FALSE;
	uint8 transition_reason = reason;
	uint8 retry_delay = delay;
#endif /* WL_MBO && !WL_MBO_DISABLED && MBO_AP */
	btq_nbr_list_t* btq_nbr_elem = NULL;

	if (BSSCFG_STA(bsscfg))
		return;

	wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);
	bsstrans = &wnm_cfg->bsstrans_req_info;

	maxlen = DOT11_BSSTRANS_REQ_LEN;

	if (reqmode & DOT11_BSSTRANS_REQMODE_BSS_TERM_INCL)
		maxlen += TLV_HDR_LEN + DOT11_NGBR_BSS_TERM_DUR_SE_LEN;
	if (reqmode & DOT11_BSSTRANS_REQMODE_ESS_DISASSOC_IMNT)
		maxlen += wnm->url_len + 1; /* include URL_Length byte */
	if (reqmode & DOT11_BSSTRANS_REQMODE_PREF_LIST_INCL) {
		/* check for AP's own entry in Neighbor list, in case it is
		 * present, no need to allocate extra memory to serve
		 * MBO requirement i.e. add AP's own BSS information in
		 * BSS transition candidate entry
		 */
		maxlen += wnm_cfg->btq_nbrlist_size *
			(TLV_HDR_LEN + DOT11_NEIGHBOR_REP_IE_FIXED_LEN +
			TLV_HDR_LEN + DOT11_NGBR_BSSTRANS_PREF_SE_LEN +
			TLV_HDR_LEN + DOT11_WIDE_BW_IE_LEN);

		btq_nbr_elem = wnm_cfg->btq_nbr_list_head;
		while (scb && btq_nbr_elem) {
			if (memcmp(&scb->bsscfg->BSSID, &(btq_nbr_elem->nbr_elt.bssid),
				ETHER_ADDR_LEN) == 0) {
				/* self bss info already added, reset add_self */
				add_self = FALSE;
			}
			btq_nbr_elem = btq_nbr_elem->next;
		}
	}

#if defined(WL_MBO) && !defined(WL_MBO_DISABLED) && defined(MBO_AP)
	if (BSS_MBO_ENAB(wlc, bsscfg)) {
		if (scb && SCB_MBO(scb)) {
			maxlen += wlc_mbo_calc_len_mbo_ie_bsstrans_req(reqmode, &assoc_retry_attr);
		}
		if (add_self) {
			/* Allocate space for BSS's own Neighbor report element */
			maxlen += (TLV_HDR_LEN + DOT11_NEIGHBOR_REP_IE_FIXED_LEN +
				TLV_HDR_LEN + DOT11_NGBR_BSSTRANS_PREF_SE_LEN);
		}
	}
#endif /* WL_MBO && !WL_MBO_DISABLED && MBO_AP */
	if ((p = wlc_frame_get_action(wlc, (scb? &scb->ea: &ether_bcast),
	    &bsscfg->cur_etheraddr, &bsscfg->BSSID, maxlen, &pbody,
	    DOT11_ACTION_CAT_WNM)) == NULL) {
		WL_ERROR(("wl%d: %s: no memory for BSS Management request\n",
		           wlc->pub->unit, __FUNCTION__));
		return;
	}

	transreq = (dot11_bsstrans_req_t *)pbody;
	transreq->category = DOT11_ACTION_CAT_WNM;
	transreq->action = DOT11_WNM_ACTION_BSSTRANS_REQ;
	transreq->token = token;
	transreq->reqmode = reqmode;

	if (reqmode & DOT11_BSSTRANS_REQMODE_DISASSOC_IMMINENT)
		htol16_ua_store(bsstrans->disassoc_tmr, (uint8 *)&transreq->disassoc_tmr);
	else
		transreq->disassoc_tmr = 0; /* 0 for reserved */

	/* As per 802.11 Spec, Validity Interval should not depend on */
	/* DOT11_BSSTRANS_REQMODE_PREF_LIST_INCL bit and must not be */
	/* 0 anytime, as 0 is Reserved value */
	/* if (reqmode & DOT11_BSSTRANS_REQMODE_PREF_LIST_INCL) */
		transreq->validity_intrvl = bsstrans->validity_intrvl;
	/* else */
		/* transreq->validity_intrvl = 0; */

	pdata = &transreq->data[0];
	bufend = pbody + maxlen;

	if (reqmode & DOT11_BSSTRANS_REQMODE_BSS_TERM_INCL) {
		dot11_ngbr_bss_term_dur_se_t *subelem;
		subelem = (dot11_ngbr_bss_term_dur_se_t *)pdata;

		subelem->sub_id = DOT11_NGBR_BSS_TERM_DUR_SE_ID;
		subelem->len = DOT11_NGBR_BSS_TERM_DUR_SE_LEN;

		htol32_ua_store(bsstrans->tsf_l, (uint8 *)&subelem->tsf[0]);
		htol32_ua_store(bsstrans->tsf_h, (uint8 *)&subelem->tsf[4]);

		htol16_ua_store(bsstrans->dur, (uint8 *)&subelem->duration);

		pdata += TLV_HDR_LEN + DOT11_NGBR_BSS_TERM_DUR_SE_LEN;
	}

	if (reqmode & DOT11_BSSTRANS_REQMODE_ESS_DISASSOC_IMNT) {
		wnm_url_t *url = (wnm_url_t *)pdata;
		url->len = wnm->url_len;

		if (wnm->url && wnm->url_len) {
			bcopy(wnm->url, &url->data[0], url->len);

			/* include URL_Length byte */
			pdata += url->len + 1;
		}
		else
			transreq->reqmode &= ~DOT11_BSSTRANS_REQMODE_ESS_DISASSOC_IMNT;
	}
	if (reqmode & DOT11_BSSTRANS_REQMODE_PREF_LIST_INCL) {
		btq_nbr_elem = wnm_cfg->btq_nbr_list_head;
		while (btq_nbr_elem) {
			wlc_wnm_ap_btq_nbr_ie(wlc, &btq_nbr_elem->nbr_elt, &pdata, scb, reqmode);
			btq_nbr_elem = btq_nbr_elem->next;
		}
	}

#if defined(WL_MBO) && !defined(WL_MBO_DISABLED) && defined(MBO_AP)
	if (BSS_MBO_ENAB(wlc, bsscfg)) {
		if (add_self) {
			/* fill BSS's own information also in Neighbor report with BSS transition
			 * candidate preference element with preference value as 0
			 */
			wlc_create_nbr_element_own_bss(wlc, bsscfg, &pdata);
		}
		if (scb && SCB_MBO(scb)) {
			/* fill MBO IE with Assoc retry delay attribute and Transition reason code
			 * attribute
			 */
			wlc_mbo_add_mbo_ie_bsstrans_req(wlc, &pdata, assoc_retry_attr, retry_delay,
				transition_reason);
		}
	}
#endif /* WL_MBO && !WL_MBO_DISABLES && MBO_AP */

#ifdef BCMDBG
	prhex("Raw BSS Trans Req body", (uchar *)pbody, maxlen);
#endif /* BCMDBG */
	if ((bufend - pdata) > 0) {
		/* adjust packet data len if few ie are conditionally skipped */
		PKTSETLEN(wlc->osh, p, (PKTLEN(wlc->osh, p) - (bufend - pdata)));
	}
	wlc_sendmgmt(wlc, p, bsscfg->wlcif->qi, scb);
}

static void
wlc_wnm_parse_bsstrans_resp(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg, struct scb *scb,
	uint8 *body, int body_len)
{
	dot11_bsstrans_resp_t *bsstrans_resp = (dot11_bsstrans_resp_t *)body;
	wnm_bsscfg_cubby_t *wnm_cfg;
	wnm_bsstrans_req_info_t *bsstrans;
	wlc_info_t *wlc;
#if defined(BCMDBG) || defined(WLMSG_INFORM)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_INFORM */
	wnm_scb_cubby_t *wnm_scb = SCB_WNM_CUBBY(wnm, scb);
	wlc = wnm->wlc;

	wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);
	bsstrans = &wnm_cfg->bsstrans_req_info;

	if (body_len < DOT11_BSSTRANS_RESP_LEN)
		return;
	if (!WLWNM_ENAB(wlc->pub) || !WNM_BSSTRANS_ENABLED(wnm_cfg->cap)) {
		WL_ERROR(("wl%d.%d: WNM BSS Trans not enabled\n", wlc->pub->unit,
			WLC_BSSCFG_IDX(bsscfg)));
		return;
	}

	wlc_bss_mac_event(wlc, bsscfg, WLC_E_BSSTRANS_RESP, &scb->ea, 0, 0, 0, body, body_len);

	if (!SCB_BSSTRANS(wnm_scb->cap)) {
		WL_ERROR(("wl%d.%d: STA unsupport BSS Trans mode\n", wlc->pub->unit,
			WLC_BSSCFG_IDX(bsscfg)));
		return;
	}
#if defined(WL_MBO) && !defined(WL_MBO_DISABLED) && defined(MBO_AP)
	if (bsstrans_resp->status != DOT11_BSSTRANS_RESP_STATUS_ACCEPT) {
		/* parse for MBO_IE with Transition rejection attribute if
		 * present
		 */
		if ((wlc_mbo_process_bsstrans_resp(wlc, scb, body, body_len) == BCME_NOTFOUND)) {
			WL_WNM(("wl%d.%d: No MBO IE in BSS Transition response frame"
				"from "MACF" \n", wlc->pub->unit,
				WLC_BSSCFG_IDX(bsscfg), ETHER_TO_MACF(scb->ea)));
		}
	}
#endif /* WL_MBO && !MBO_DISABLED && MBO_AP */
	if (bsstrans->timer_state == BSSTRANS_WAIT_FOR_BSS_TERM &&
	    bsstrans_resp->token == wnm_scb->bsstrans_token) {
		/* This resp frame is in response for the BSS transition req frame
		 * from AP for BSS termination. Update the status and termination delay.
		 */
#if defined(BCMDBG) || defined(WLMSG_INFORM)
		WL_WNM(("wl%d: %s: bss transition resp token 0x%02x, status 0x%02x from %s\n",
			wnm->wlc->pub->unit, __FUNCTION__, bsstrans_resp->token,
			bsstrans_resp->status, bcm_ether_ntoa(&scb->ea, eabuf)));
#endif /* BCMDBG || WLMSG_INFORM */

		/* Collect the status and termination delay from STAs */
		if (bsstrans_resp->status <= DOT11_BSSTRANS_RESP_STATUS_REJ_LEAVING_ESS)
			bsstrans->status |= (1 << bsstrans_resp->status);

		if (bsstrans_resp->status == DOT11_BSSTRANS_RESP_STATUS_REJ_TERM_DELAY_REQ) {
			bsstrans->delay = MAX(bsstrans->delay, bsstrans_resp->term_delay);
		}
	}
}

static arp_table_t*
wlc_get_bss_cubby_arp_tbl_handle(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	wnm_bsscfg_cubby_t *wnm_cfg = WNM_BSSCFG_CUBBY(wlc->wnm_info, bsscfg);
	return wnm_cfg->phnd_arp_table;
}

void
wlc_wnm_parp_watchdog(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, bool all, uint8 *del_ea,
	bool periodic)
{
	arp_table_t *ptable = wlc_get_bss_cubby_arp_tbl_handle(wlc, bsscfg);
	if  (ptable != NULL) {
		bcm_l2_filter_arp_table_update(wlc->osh, ptable, all, del_ea, periodic,
			wlc->pub->now);
	}
}

/*
 * Moved from wlc_l2_filter.c
 */
static uint8
wnm_proxyarp_arp_handle(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, frame_proto_t *fp, void **reply,
	uint8 *reply_to_bss, bool istx)
{
	wnm_bsscfg_cubby_t *wnm_cfg = WNM_BSSCFG_CUBBY(wlc->wnm_info, bsscfg);
	struct bcmarp *arp = (struct bcmarp *)fp->l3;
	parp_entry_t *entry;
	uint16 op = ntoh16(arp->oper);
#ifdef BCMDBG
	char ipbuf[64], eabuf[32];
#endif /* BCMDBG */

	/* basic ether addr check */
	if (ETHER_ISNULLADDR(arp->src_eth) || ETHER_ISBCAST(arp->src_eth) ||
	    ETHER_ISMULTI(arp->src_eth)) {
#ifdef BCMDBG
		WL_ERROR(("wl%d.%d: Invalid Ether addr(%s)\n", wlc->pub->unit,
			WLC_BSSCFG_IDX(bsscfg),
			bcm_ether_ntoa((struct ether_addr *)arp->src_eth, eabuf)));
#endif /* BCMDBG */
		return FRAME_NOP;
	}

	if (op > ARP_OPC_REPLY) {
		WL_ERROR(("wl%d.%d: Invalid ARP operation(%d)\n", wlc->pub->unit,
			WLC_BSSCFG_IDX(bsscfg), op));
		return FRAME_NOP;
	}

	/* handle learning on ARP-REQ|ARP-REPLY|ARP-Announcement */
	if (!IPV4_ADDR_NULL(arp->src_ip) && !IPV4_ADDR_BCAST(arp->src_ip)) {
		entry = bcm_l2_filter_parp_findentry(wnm_cfg->phnd_arp_table,
			arp->src_ip, IP_VER_4, TRUE, wlc->pub->now);
		if (entry == NULL) {
#ifdef BCMDBG
			WL_ERROR(("wl%d.%d: Add parp_entry by ARP %s %s\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg),
				bcm_ether_ntoa((struct ether_addr *)arp->src_eth, eabuf),
				bcm_ip_ntoa((struct ipv4_addr *)arp->src_ip, ipbuf)));
#endif /* BCMDBG */
			bcm_l2_filter_parp_addentry(wlc->osh, wnm_cfg->phnd_arp_table,
				(struct ether_addr *)arp->src_eth, arp->src_ip, IP_VER_4,
				TRUE, wlc->pub->now);
		}
	}
	/* only learning ARP-Probe(DAD) in receiving path */
	else {
		if (!istx && op == ARP_OPC_REQUEST) {
			entry = bcm_l2_filter_parp_findentry(wnm_cfg->phnd_arp_table, arp->dst_ip,
				IP_VER_4, TRUE, wlc->pub->now);
			if (entry == NULL)
				entry = bcm_l2_filter_parp_findentry(wnm_cfg->phnd_arp_table,
					arp->dst_ip, IP_VER_4, FALSE, wlc->pub->now);

			/* no such entry exist */
			if (entry == NULL) {
#ifdef BCMDBG
				WL_ERROR(("wl%d.%d: create candidate parp_entry by ARP "
					"%s %s\n", wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg),
					bcm_ether_ntoa((struct ether_addr *)arp->src_eth,
					eabuf), bcm_ip_ntoa((struct ipv4_addr *)arp->dst_ip,
					ipbuf)));
#endif /* BCMDBG */
				bcm_l2_filter_parp_addentry(wlc->osh, wnm_cfg->phnd_arp_table,
					(struct ether_addr *)arp->src_eth, arp->dst_ip,
					IP_VER_4, FALSE, wlc->pub->now);
			}
		}
	}

	/* perform candidate entry delete if some STA reply with ARP-Announcement */
	if (op == ARP_OPC_REPLY) {
		entry = bcm_l2_filter_parp_findentry(wnm_cfg->phnd_arp_table, arp->src_ip,
			IP_VER_4, FALSE, wlc->pub->now);
		if (entry) {
			struct ether_addr ea;
			bcopy(&entry->ea, &ea, ETHER_ADDR_LEN);
#ifdef BCMDBG
			WL_ERROR(("wl%d.%d: withdraw candidate parp_entry IPv4 %s %s\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg),
				bcm_ether_ntoa(&ea, eabuf),
				bcm_ip_ntoa((struct ipv4_addr *)arp->src_ip, ipbuf)));
#endif /* BCMDBG */
			bcm_l2_filter_parp_delentry(wlc->osh, wnm_cfg->phnd_arp_table,
				&ea, arp->src_ip, IP_VER_4, FALSE);
		}
	}

	/* handle sending path */
	if (istx) {
		/* Drop ARP-Announcement(Gratuitous ARP) on sending path */
		if (bcmp(arp->src_ip, arp->dst_ip, IPV4_ADDR_LEN) == 0) {
			return FRAME_DROP;
		}

		/* try to reply if trying to send arp request frame */
		if (op == ARP_OPC_REQUEST) {
			struct scb *scb;
			struct bcmarp *arp_reply;
			uint16 pktlen = ETHER_HDR_LEN + ARP_DATA_LEN;
			bool snap = FALSE;

			if ((entry = bcm_l2_filter_parp_findentry(wnm_cfg->phnd_arp_table,
				arp->dst_ip, IP_VER_4, TRUE, wlc->pub->now)) == NULL) {
				if (wlc->wnm_info->parp_discard)
					return FRAME_DROP;
				else
					return FRAME_NOP;
			}

			/* STA asking itself address. drop this frame */
			if (bcmp(arp->src_eth, (uint8 *)&entry->ea, ETHER_ADDR_LEN) == 0) {
				return FRAME_DROP;
			}

			/* STA asking to some address not belong to BSS.  Drop frame */
			if ((scb = wlc_scbfind(wlc, bsscfg, &entry->ea)) == NULL) {
				return FRAME_DROP;
			}

			/* determine dst is within bss or outside bss */
			scb = wlc_scbfind(wlc, bsscfg, (struct ether_addr *)arp->src_eth);
			if (scb != NULL) {
				/* dst is within bss, mark it */
				*reply_to_bss = 1;
			}

			if (fp->l2_t == FRAME_L2_SNAP_H || fp->l2_t == FRAME_L2_SNAPVLAN_H) {
				pktlen += SNAP_HDR_LEN + ETHER_TYPE_LEN;
				snap = TRUE;
			}

			/* Create 42-byte arp-reply data frame */
			if ((*reply = bcm_l2_filter_proxyarp_alloc_reply(wlc->osh, pktlen,
				&entry->ea, (struct ether_addr *)arp->src_eth,
				ETHER_TYPE_ARP, snap, (void **)&arp_reply)) == NULL) {
				WL_ERROR(("wl%d.%d: failed to allocate reply frame. drop it\n",
					wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg)));
				return FRAME_NOP;
			}

			/* copy first 6 bytes from ARP-Req to ARP-Reply(htype, ptype, hlen, plen) */
			bcopy(arp, arp_reply, ARP_OPC_OFFSET);
			hton16_ua_store(ARP_OPC_REPLY, &arp_reply->oper);
			bcopy(&entry->ea, arp_reply->src_eth, ETHER_ADDR_LEN);
			bcopy(&entry->ip.data, arp_reply->src_ip, IPV4_ADDR_LEN);
			bcopy(arp->src_eth, arp_reply->dst_eth, ETHER_ADDR_LEN);
			bcopy(arp->src_ip, arp_reply->dst_ip, IPV4_ADDR_LEN);

			return FRAME_TAKEN;
		}
		/* ARP REPLY */
		else {
			entry = bcm_l2_filter_parp_findentry(wnm_cfg->phnd_arp_table,
				arp->src_ip, IP_VER_4, TRUE, wlc->pub->now);

			/* If SMAC-SIP in reply frame is inconsistent
			 * to exist entry, drop frame(HS2-4.5.C)
			 */
			if (entry && bcmp(arp->src_eth, &entry->ea, ETHER_ADDR_LEN) != 0) {
				return FRAME_DROP;
			}
		}
	}

	return FRAME_NOP;
}

static uint8
wnm_proxyarp_dhcp4_handle(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, frame_proto_t *fp, bool istx)
{
	uint8 *dhcp;
	bcm_tlv_t *msg_type;
	uint16 opt_len, offset = DHCP_OPT_OFFSET;
	wnm_bsscfg_cubby_t *wnm_cfg = WNM_BSSCFG_CUBBY(wlc->wnm_info, bsscfg);
	struct scb *scb;
	arp_table_t* ptable;
	uint8 smac_addr[ETHER_ADDR_LEN];
	uint8 cmac_addr[ETHER_ADDR_LEN];
#ifdef BCMDBG
	char ipbuf[64], eabuf[32];
#endif /* BCMDBG */

	dhcp = (uint8 *)(fp->l4 + UDP_HDR_LEN);

	/* First option must be magic cookie */
	if ((dhcp[offset + 0] != 0x63) || (dhcp[offset + 1] != 0x82) ||
	    (dhcp[offset + 2] != 0x53) || (dhcp[offset + 3] != 0x63))
		return FRAME_NOP;

	/* skip 4 byte magic cookie and calculate dhcp opt len */
	offset += 4;
	opt_len = fp->l4_len - UDP_HDR_LEN - offset;

	ptable = wnm_cfg->phnd_arp_table;
	bcm_l2_filter_parp_get_smac(ptable, smac_addr);
	bcm_l2_filter_parp_get_cmac(ptable, cmac_addr);
	/* sending path, process DHCP Ack frame only */
	if (istx) {
		msg_type = bcm_parse_tlvs(&dhcp[offset], opt_len, DHCP_OPT_MSGTYPE);
		if (msg_type == NULL || msg_type->data[0] != DHCP_OPT_MSGTYPE_ACK)
			return FRAME_NOP;

		/* compared to DHCP Req client mac */
		if (bcmp((void*)cmac_addr, &dhcp[DHCP_CHADDR_OFFSET], ETHER_ADDR_LEN)) {
#ifdef BCMDBG
			WL_ERROR(("wl%d.%d: Unmatch DHCP Req Client MAC (%s)",
				wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg),
				bcm_ether_ntoa((struct ether_addr *)cmac_addr, eabuf)));
			WL_ERROR(("to DHCP Ack Client MAC(%s)\n",
				bcm_ether_ntoa((struct ether_addr *)&dhcp[DHCP_CHADDR_OFFSET],
				eabuf)));
#endif /* BCMDBG */
			return FRAME_NOP;
		}

		/* If client transmit DHCP Inform, server will response DHCP Ack with NULL YIADDR */
		if (IPV4_ADDR_NULL(&dhcp[DHCP_YIADDR_OFFSET]))
			return FRAME_NOP;

		/* handle learning if EA belong to STA in BSS */
		if ((scb = wlc_scbfind(wlc, bsscfg, (struct ether_addr*)smac_addr))
		    != NULL) {
			parp_entry_t *entry = bcm_l2_filter_parp_findentry(ptable,
				&dhcp[DHCP_YIADDR_OFFSET], IP_VER_4, TRUE, wlc->pub->now);

			if (entry == NULL) {
#ifdef BCMDBG
				WL_ERROR(("wl%d.%d: Add parp_entry by DHCP4 %s %s\n",
					wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg),
					bcm_ether_ntoa(&scb->ea, eabuf),
					bcm_ip_ntoa((struct ipv4_addr *)
						&dhcp[DHCP_YIADDR_OFFSET], ipbuf)));
#endif /* BCMDBG */
				bcm_l2_filter_parp_addentry(wlc->osh, ptable, &scb->ea,
					&dhcp[DHCP_YIADDR_OFFSET], IP_VER_4, TRUE, wlc->pub->now);
			}
			else {
				/* XXX error! STA resend DHCP request or
				 * DHCP have the same ip wih other STA
				 */
			}
		}
	}
	else {	/* receiving path, process DHCP Req frame only */
		struct ether_header *eh = (struct ether_header *)fp->l2;

		msg_type = bcm_parse_tlvs(&dhcp[offset], opt_len, DHCP_OPT_MSGTYPE);
		if (msg_type == NULL || msg_type->data[0] != DHCP_OPT_MSGTYPE_REQ)
			return FRAME_NOP;

		/* basic ether addr check */
		if (ETHER_ISNULLADDR(&dhcp[DHCP_CHADDR_OFFSET]) ||
		    ETHER_ISBCAST(&dhcp[DHCP_CHADDR_OFFSET]) ||
		    ETHER_ISMULTI(&dhcp[DHCP_CHADDR_OFFSET]) ||
		    ETHER_ISNULLADDR(eh->ether_shost) ||
		    ETHER_ISBCAST(eh->ether_shost) ||
		    ETHER_ISMULTI(eh->ether_shost)) {
#ifdef BCMDBG
			WL_WNM(("wl%d.%d: Invalid Ether addr(%s)",
				wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg),
				bcm_ether_ntoa((struct ether_addr *)eh->ether_shost, eabuf)));
			WL_WNM(("(%s) of DHCP Req pkt\n",
				bcm_ether_ntoa((struct ether_addr *)&dhcp[DHCP_CHADDR_OFFSET],
				eabuf)));
#endif /* BCMDBG */
			return FRAME_NOP;
		}
		/*
		 * In URE mode, EA might different from Client Mac in BOOTP and SMAC in L2 hdr.
		 * We need to saved SMAC addr and Client Mac.  So that when receiving DHCP Ack,
		 * we can compare saved Client Mac and Client Mac in DHCP Ack frame.  If it's
		 * matched, then our target MAC would be saved L2 SMAC
		 */
		bcm_l2_filter_parp_set_smac(ptable, eh->ether_shost);
		bcm_l2_filter_parp_set_cmac(ptable, &dhcp[DHCP_CHADDR_OFFSET]);
	}

	return FRAME_NOP;
}

static uint8
proxyarp_icmp6_handle(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, frame_proto_t *fp, void **reply,
	uint8 *reply_to_bss, bool istx)
{
	wnm_bsscfg_cubby_t *wnm_cfg = WNM_BSSCFG_CUBBY(wlc->wnm_info, bsscfg);
	struct ether_header *eh = (struct ether_header *)fp->l2;
	struct ipv6_hdr *ipv6_hdr = (struct ipv6_hdr *)fp->l3;
	struct bcm_nd_msg *nd_msg = (struct bcm_nd_msg *)(fp->l3 + sizeof(struct ipv6_hdr));
	parp_entry_t *entry;
	struct ether_addr *entry_ea = NULL;
	uint8 *entry_ip = NULL;
	bool dad = FALSE;	/* Duplicate Address Detection */
	uint8 link_type = 0;
	bcm_tlv_t *link_addr = NULL;
	uint16 ip6_icmp6_len = sizeof(struct ipv6_hdr) + sizeof(struct bcm_nd_msg);
#ifdef BCMDBG
	char ipbuf[64], eabuf[32];
#endif /* BCMDBG */

	/* basic check */
	if ((fp->l3_len < ip6_icmp6_len) ||
	    (ipv6_hdr->nexthdr != ICMPV6_HEADER_TYPE))
		return FRAME_NOP;

	/* Neighbor Solicitation */
	if (nd_msg->icmph.icmp6_type == ICMPV6_PKT_TYPE_NS) {
		link_type = ICMPV6_ND_OPT_TYPE_SRC_MAC;
		if (IPV6_ADDR_NULL(ipv6_hdr->saddr.addr)) {
			/* ip6 src field is null, set offset to icmp6 target field */
			entry_ip = nd_msg->target.addr;
			dad = TRUE;
		}
		else {
			/* ip6 src field not null, set offset to ip6 src field */
			entry_ip = ipv6_hdr->saddr.addr;
		}
	}
	/* Neighbor Advertisement */
	else if (nd_msg->icmph.icmp6_type == ICMPV6_PKT_TYPE_NA) {
		link_type = ICMPV6_ND_OPT_TYPE_TARGET_MAC;
		entry_ip = nd_msg->target.addr;
	}
	else
	/* not an interesting frame, return without action */
		return FRAME_NOP;

	/* if icmp6-option exists, retrieve layer2 link address from icmp6-option */
	if (fp->l3_len > ip6_icmp6_len) {
		link_addr = parse_nd_options(fp->l3 + ip6_icmp6_len,
			fp->l3_len - ip6_icmp6_len, link_type);

		if (link_addr && link_addr->len == ICMPV6_ND_OPT_LEN_LINKADDR)
			entry_ea = (struct ether_addr *)&link_addr->data;
	}
	/* if no ea, retreive layer2 link address from ether header */
	if (entry_ea == NULL)
		entry_ea = (struct ether_addr *)eh->ether_shost;

	/* basic ether addr check */
	if (ETHER_ISNULLADDR(eh->ether_shost) || ETHER_ISBCAST(eh->ether_shost) ||
	    ETHER_ISMULTI(eh->ether_shost)) {
#ifdef BCMDBG
		WL_ERROR(("wl%d.%d: Invalid Ether addr(%s) of icmp6 pkt\n", wlc->pub->unit,
			WLC_BSSCFG_IDX(bsscfg),
			bcm_ether_ntoa((struct ether_addr *)eh->ether_shost, eabuf)));
#endif /* BCMDBG */
		return FRAME_NOP;
	}

	/* handle learning on Neighbor-Advertisement | Neighbor-Solicition(non-DAD) */
	if (nd_msg->icmph.icmp6_type == ICMPV6_PKT_TYPE_NA ||
	    (nd_msg->icmph.icmp6_type == ICMPV6_PKT_TYPE_NS && !dad)) {
		entry = bcm_l2_filter_parp_findentry(wnm_cfg->phnd_arp_table,
			entry_ip, IP_VER_6, TRUE, wlc->pub->now);
		if (entry == NULL) {
#ifdef BCMDBG
			WL_ERROR(("wl%d.%d: Add new parp_entry by ICMP6 %s %s\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg),
				bcm_ether_ntoa(entry_ea, eabuf),
				bcm_ipv6_ntoa((void *)entry_ip, ipbuf)));
#endif /* BCMDBG */
			bcm_l2_filter_parp_addentry(wlc->osh, wnm_cfg->phnd_arp_table,
				entry_ea, entry_ip, IP_VER_6, TRUE, wlc->pub->now);
		}
	}
	/* only learning Neighbor-Solicition(DAD) in receiving path */
	else {
		if (!istx) {
			entry = bcm_l2_filter_parp_findentry(wnm_cfg->phnd_arp_table,
				entry_ip, IP_VER_6, TRUE, wlc->pub->now);
			if (entry == NULL)
				entry = bcm_l2_filter_parp_findentry(wnm_cfg->phnd_arp_table,
					entry_ip, IP_VER_6, FALSE, wlc->pub->now);

			/* no such entry exist, add candidate */
			if (entry == NULL) {
#ifdef BCMDBG
				WL_ERROR(("wl%d.%d: create candidate parp_entry "
					"by ICMP6 %s %s\n",
					wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg),
					bcm_ether_ntoa((struct ether_addr *)entry_ea,
					eabuf), bcm_ipv6_ntoa((void *)entry_ip, ipbuf)));
#endif /* BCMDBG */
				bcm_l2_filter_parp_addentry(wlc->osh, wnm_cfg->phnd_arp_table,
					entry_ea, entry_ip, IP_VER_6, FALSE, wlc->pub->now);
			}
		}
	}

	/* perform candidate entry delete if some STA reply with Neighbor-Advertisement */
	if (nd_msg->icmph.icmp6_type == ICMPV6_PKT_TYPE_NA) {
		entry = bcm_l2_filter_parp_findentry(wnm_cfg->phnd_arp_table,
			entry_ip, IP_VER_6, FALSE, wlc->pub->now);
		if (entry) {
			struct ether_addr ea;
			bcopy(&entry->ea, &ea, ETHER_ADDR_LEN);
#ifdef BCMDBG
			WL_ERROR(("wl%d.%d: withdraw candidate parp_entry IPv6 %s %s\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg),
				bcm_ether_ntoa(&ea, eabuf),
				bcm_ipv6_ntoa((void *)entry_ip, ipbuf)));
#endif /* BCMDBG */
			bcm_l2_filter_parp_delentry(wlc->osh, wnm_cfg->phnd_arp_table, &ea,
				entry_ip, IP_VER_6, FALSE);
		}
	}

	/* handle sending path */
	if (istx) {
		if (nd_msg->icmph.icmp6_type == ICMPV6_PKT_TYPE_NA) {
			/* Unsolicited Network Advertisment from STA, drop frame(HS2-4.5.F) */
			if (!(nd_msg->icmph.opt.nd_advt.router) &&
					(!nd_msg->icmph.opt.nd_advt.solicited)) {

				struct nd_msg_opt *nd_msg_opt_reply = (struct nd_msg_opt *)
					(((uint8 *)nd_msg) + sizeof(struct bcm_nd_msg));
				if (nd_msg_opt_reply != NULL) {
					if (nd_msg_opt_reply->type != ICMPV6_ND_OPT_TYPE_TARGET_MAC)
						return FRAME_DROP;
				}
			}
		}

	/* try to reply if trying to send arp request frame */
		if (nd_msg->icmph.icmp6_type == ICMPV6_PKT_TYPE_NS) {
			struct scb *scb;
			struct ether_addr *reply_mac;
			struct ipv6_hdr *ipv6_reply;
			struct bcm_nd_msg *nd_msg_reply;
			struct nd_msg_opt *nd_msg_opt_reply;
			uint16 pktlen = ETHER_HDR_LEN + sizeof(struct ipv6_hdr) +
				sizeof(struct bcm_nd_msg) + sizeof(struct nd_msg_opt);
			bool snap = FALSE;
			uint8 ipv6_mcast_allnode_ea[6] = {0x33, 0x33, 0x0, 0x0, 0x0, 0x1};
			uint8 ipv6_mcast_allnode_ip[16] = {0xff, 0x2, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
				0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1};

			if ((entry = bcm_l2_filter_parp_findentry(wnm_cfg->phnd_arp_table,
				nd_msg->target.addr, IP_VER_6,
				TRUE, wlc->pub->now)) == NULL) {
				if (wlc->wnm_info->parp_discard)
					return FRAME_DROP;
				else
					return FRAME_NOP;
			}

			/* STA asking itself address. drop this frame */
			if (bcmp(entry_ea, (uint8 *)&entry->ea, ETHER_ADDR_LEN) == 0) {
				return FRAME_DROP;
			}

			/* STA asking to some address not belong to BSS.  Drop frame */
			if ((scb = wlc_scbfind(wlc, bsscfg, &entry->ea)) == NULL) {
				return FRAME_DROP;
			}

			/* determine dst is within bss or outside bss */
			scb = wlc_scbfind(wlc, bsscfg, (struct ether_addr *)eh->ether_shost);
			if (scb != NULL) {
				/* dst is within bss, mark it */
				*reply_to_bss = 1;
			}

			if (fp->l2_t == FRAME_L2_SNAP_H || fp->l2_t == FRAME_L2_SNAPVLAN_H) {
				pktlen += SNAP_HDR_LEN + ETHER_TYPE_LEN;
				snap = TRUE;
			}

			/* Create 72 bytes neighbor advertisement data frame */
			/* determine l2 mac address is unicast or ipv6 mcast */
			if (dad) {
				/* XXX NS DA should use ipv6_mcast_allnode_ea.
				 * however, wireshark can't interpreter encrypted frame
				 * with ipv6 mcast ea in HS2.0, to fix this on plugfest,
				 * we use ucast ea as NA DA.
				 */
				if (wlc->wnm_info->parp_allnode)
					reply_mac = (struct ether_addr *)ipv6_mcast_allnode_ea;
				else
					reply_mac = (struct ether_addr *)eh->ether_shost;
			}
			else {
				reply_mac = entry_ea;
			}
			if ((*reply = bcm_l2_filter_proxyarp_alloc_reply(wlc->osh, pktlen,
				&entry->ea, reply_mac, ETHER_TYPE_IPV6,
				snap, (void **)&ipv6_reply)) == NULL) {
				return FRAME_NOP;
			}

			/* construct 40 bytes ipv6 header */
			bcopy((uint8 *)ipv6_hdr, (uint8 *)ipv6_reply, sizeof(struct ipv6_hdr));
			hton16_ua_store(sizeof(struct bcm_nd_msg) + sizeof(struct nd_msg_opt),
				&ipv6_reply->payload_len);
			ipv6_reply->hop_limit = 255;

			bcopy(nd_msg->target.addr, ipv6_reply->saddr.addr, IPV6_ADDR_LEN);
			/* if Duplicate address detected, filled all-node address as destination */
			if (dad)
				bcopy(ipv6_mcast_allnode_ip, ipv6_reply->daddr.addr,
					IPV6_ADDR_LEN);
			else
				bcopy(entry_ip, ipv6_reply->daddr.addr, IPV6_ADDR_LEN);

			/* Create 32 bytes icmpv6 NA frame body */
			nd_msg_reply = (struct bcm_nd_msg *)
				(((uint8 *)ipv6_reply) + sizeof(struct ipv6_hdr));
			nd_msg_reply->icmph.icmp6_type = ICMPV6_PKT_TYPE_NA;
			nd_msg_reply->icmph.icmp6_code = 0;
			nd_msg_reply->icmph.opt.reserved = 0;
			nd_msg_reply->icmph.opt.nd_advt.override = 1;
			/* from observing win7 behavior, only non dad will set solicited flag */
			if (!dad)
				nd_msg_reply->icmph.opt.nd_advt.solicited = 1;
			bcopy(nd_msg->target.addr, nd_msg_reply->target.addr, IPV6_ADDR_LEN);
			nd_msg_opt_reply = (struct nd_msg_opt *)
				(((uint8 *)nd_msg_reply) + sizeof(struct bcm_nd_msg));
			nd_msg_opt_reply->type = ICMPV6_ND_OPT_TYPE_TARGET_MAC;
			nd_msg_opt_reply->len = ICMPV6_ND_OPT_LEN_LINKADDR;
			bcopy((uint8 *)&entry->ea, nd_msg_opt_reply->mac_addr, ETHER_ADDR_LEN);

			/* calculate ICMPv6 check sum */
			nd_msg_reply->icmph.icmp6_cksum = 0;
			nd_msg_reply->icmph.icmp6_cksum = calc_checksum(ipv6_reply->saddr.addr,
				ipv6_reply->daddr.addr,
				sizeof(struct bcm_nd_msg) + sizeof(struct nd_msg_opt),
				IP_PROT_ICMP6, (uint8 *)nd_msg_reply);

			return FRAME_TAKEN;
		}
	}

	return FRAME_NOP;
}
#endif /* WLWNM_AP */

#ifdef STA
int
wlc_wnm_timbc_resp_ie_process(wlc_info_t *wlc, dot11_timbc_resp_ie_t *ie, int len, struct scb *scb)
{
	wlc_bsscfg_t *bsscfg = SCB_BSSCFG(scb);
	wnm_bsscfg_cubby_t *wnm_cfg = WNM_BSSCFG_CUBBY(wlc->wnm_info, bsscfg);
	wl_timbc_status_t *stat = &wnm_cfg->timbc_stat;
	int next_state;

	/* Check min real size up to status */
	if (len < TLV_HDR_LEN + DOT11_TIMBC_DENY_RESP_IE_LEN)
		return BCME_ERROR;

	/* Check content */
	if (ie->id != DOT11_MNG_TIMBC_RESP_ID || ie->status > DOT11_TIMBC_STATUS_OVERRIDDEN)
		return BCME_ERROR;

	/* Check IE size for deny */
	if (ie->status == DOT11_TIMBC_STATUS_DENY) {
		if (ie->len != DOT11_TIMBC_DENY_RESP_IE_LEN)
			return BCME_ERROR;

	/* Check IE and real size for all accept cases */
	} else if (ie->len != DOT11_TIMBC_ACCEPT_RESP_IE_LEN ||
		len < TLV_HDR_LEN + DOT11_TIMBC_ACCEPT_RESP_IE_LEN) {
		return BCME_ERROR;

	/* Check that at least one rate is set */
	} else if (!ie->high_rate && !ie->low_rate) {
		return BCME_ERROR;
	}

	stat->status_ap = ie->status;
	if (ie->status == DOT11_TIMBC_STATUS_DENY) {
		stat->interval = 0;
		stat->offset = 0;
		stat->rate_high = 0;
		stat->rate_low = 0;
	} else {
		stat->interval = ie->interval;
		stat->offset = ltoh32_ua(&ie->offset);
		stat->rate_high = ltoh16_ua(&ie->high_rate);
		stat->rate_low = ltoh16_ua(&ie->low_rate);
	}
	WL_WNM(("TIMBC resp: %u - %u %u - %u %u\n",
		stat->status_ap, stat->interval, stat->offset, stat->rate_high, stat->rate_low));

	next_state = wlc_wnm_timbc_state_update(wlc->wnm_info, bsscfg, wnm_cfg);

	/* State change? */
	if (stat->status_sta != next_state) {
		WL_WNM(("TIMBC rx resp, tx req again? %x != %x\n", stat->status_sta, next_state));

		if (bsscfg->assoc->state == AS_SENT_ASSOC ||
			bsscfg->assoc->state == AS_REASSOC_RETRY) {
			/* Cannot send frames now */
			if (wnm_cfg->timbc_last_interval) {
				stat->status_sta = WL_TIMBC_STATUS_ENABLE;
			} else {
				stat->status_sta = WL_TIMBC_STATUS_DISABLE;
			}
			wnm_cfg->timbc_timer_sta = wlc->pub->now + TIMBC_DELAY_ASSOC;

		} else {
			/* TODO: set timer to try again if denied in a normal case */
			if (next_state == WL_TIMBC_STATUS_REQ_MISMATCH &&
				stat->status_sta == WL_TIMBC_STATUS_ENABLE &&
				wnm_cfg->timbc_last_interval != 0)
			{
				/* We just get AP TIMBC settings and they do not match */
				wlc_wnm_timbc_req_frame_send(wlc->wnm_info, bsscfg, 0);

			} else
				stat->status_sta = (uint8)next_state;
		}
	}

	return BCME_OK;
}

static int
wlc_wnm_timbc_req_frame_send(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg, int int_val)
{
	wlc_info_t *wlc = wnm->wlc;
	wnm_bsscfg_cubby_t *wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);
	void *p;
	uint8 *pbody;
	int bodylen;
	struct scb *scb;
	dot11_timbc_req_t *req;
	dot11_timbc_req_ie_t *req_ie;

	bodylen = DOT11_TIMBC_REQ_LEN + TLV_HDR_LEN + DOT11_TIMBC_REQ_IE_LEN;

	if ((p = wlc_frame_get_action(wlc, &bsscfg->BSSID, &bsscfg->cur_etheraddr,
		&bsscfg->BSSID, bodylen, &pbody, DOT11_ACTION_CAT_WNM)) == NULL) {
		WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__, DOT11_ACTION_CAT_WNM,
			MALLOCED(wlc->osh)));
		return BCME_ERROR;
	}

	/* Prepare TIM Broadcast frame fields */
	req = (dot11_timbc_req_t *)pbody;
	req->category = DOT11_ACTION_CAT_WNM;
	req->action = DOT11_WNM_ACTION_TIMBC_REQ;
	WLC_WNM_UPDATE_TOKEN(wnm->req_token);
	req->token = wnm->req_token;

	req_ie = (dot11_timbc_req_ie_t *)&req->data[0];
	req_ie->id = DOT11_MNG_TIMBC_REQ_ID;
	req_ie->len = DOT11_TIMBC_REQ_IE_LEN;
	req_ie->interval = (uint8)int_val;

	scb = wlc_scbfind(wlc, bsscfg, &bsscfg->BSSID);
	wlc_sendmgmt(wlc, p, bsscfg->wlcif->qi, scb);

	wnm_cfg->timbc_last_interval = (uint8)int_val;
	wnm_cfg->timbc_stat.status_ap = WL_TIMBC_STATUS_AP_UNKNOWN;

	/* set timeout */
	wnm_cfg->timbc_timer_sta = wlc->pub->now + TIMBC_DELAY_RETRY;

	return BCME_OK;
}

static int
wlc_wnm_timbc_state_update(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg, wnm_bsscfg_cubby_t* wnm_cfg)
{
	wl_timbc_set_t *sett = &wnm_cfg->timbc_set;
	wl_timbc_status_t *stat = &wnm_cfg->timbc_stat;
	struct scb *scb = NULL;
	wnm_scb_cubby_t *wnm_scb = NULL;

	/* Disable if requested to do so */
	if (sett->interval == 0)
		return WL_TIMBC_STATUS_DISABLE;

	/* Check if we are building (re)assoc req or parsing resp */
	if (bsscfg->assoc->state == AS_SENT_ASSOC || bsscfg->assoc->state == AS_REASSOC_RETRY) {
		struct scb_iter scbiter;

		/* Search from flags for AP's scb as it is not yet linked with bsscfg */
		FOREACHSCB(wnm->wlc->scbstate, &scbiter, scb)
			if (SCB_AUTHENTICATED(scb) && SCB_ASSOCIATING(scb))
				break;
	} else {
		/* Quit if STA not associated */
		if (!bsscfg->associated)
			return WL_TIMBC_STATUS_NOT_ASSOC;

		/* Disable if all DMS required and some are not */
		if (sett->flags & WL_TIMBC_SET_DMS_ACCEPTED) {
			wnm_dms_desc_t *desc;
			for (desc = wnm_cfg->dms_info.dms_desc_head; desc; desc = desc->next)
				/* All DMS desc must be enabled at AP or disabled by user */
				if (desc->status > DMS_STATUS_ACCEPTED)
					return WL_TIMBC_STATUS_REQ_MISMATCH;
		}

		scb = wlc_scbfind(wnm->wlc, bsscfg, &bsscfg->current_bss->BSSID);
	}
	ASSERT(scb);
	wnm_scb = SCB_WNM_CUBBY(wnm, scb);
	/* Quit if TIMBC not supported by AP */
	if (!SCB_TIMBC(wnm_scb->cap))
		return WL_TIMBC_STATUS_NOT_SUPPORT;

	/* Disable if Proxy ARP required and not present */
	if ((sett->flags & WL_TIMBC_SET_PROXY_ARP) && !SCB_PROXYARP(wnm_scb->cap))
		return WL_TIMBC_STATUS_REQ_MISMATCH;

	/* Send request if we do not know AP TIMBC config */
	if (stat->status_ap == WL_TIMBC_STATUS_AP_UNKNOWN &&
		((sett->flags & (WL_TIMBC_SET_TSF_REQUIRED|WL_TIMBC_SET_NO_OVERRIDE)) ||
		sett->rate_min || sett->rate_max))
		return WL_TIMBC_STATUS_ENABLE;

	/* Disable if TSF required and not present */
	if (sett->flags & WL_TIMBC_SET_TSF_REQUIRED &&
		!(stat->status_ap & DOT11_TIMBC_STATUS_ACCEPT_TSTAMP))
		return WL_TIMBC_STATUS_REQ_MISMATCH;

	/* Disable if original interval not accepted */
	if ((sett->flags & WL_TIMBC_SET_NO_OVERRIDE) && (sett->interval != stat->interval))
		return WL_TIMBC_STATUS_REQ_MISMATCH;

	/* Disable if rate(s) not high enough */
	if (sett->rate_min &&
		sett->rate_min > stat->rate_high && sett->rate_min > stat->rate_low)
		return WL_TIMBC_STATUS_REQ_MISMATCH;

	/* Disable if rate(s) not low enough */
	if (sett->rate_max &&
		(!stat->rate_high || sett->rate_max < stat->rate_high) &&
		(!stat->rate_low || sett->rate_max < stat->rate_low))
		return WL_TIMBC_STATUS_REQ_MISMATCH;

	if (stat->status_ap == DOT11_TIMBC_STATUS_DENY)
		return WL_TIMBC_STATUS_DENIED;

	return WL_TIMBC_STATUS_ENABLE;
}

uint8 *
wlc_wnm_timbc_assoc(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint8 *pbody)
{
	wlc_wnm_info_t *wnm = wlc->wnm_info;
	wnm_bsscfg_cubby_t* wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);
	int next_state = wlc_wnm_timbc_state_update(wnm, bsscfg, wnm_cfg);

	WL_WNM(("TIMBC assoc %x: %x->%x\n",
		wnm_cfg->timbc_set.interval, wnm_cfg->timbc_stat.status_sta, next_state));

	if (next_state == WL_TIMBC_STATUS_ENABLE) {

		pbody = bcm_write_tlv(DOT11_MNG_TIMBC_REQ_ID, &wnm_cfg->timbc_set.interval,
			DOT11_TIMBC_REQ_IE_LEN, pbody);

		wnm_cfg->timbc_last_interval = wnm_cfg->timbc_set.interval;

		/* Set timer for potential retries */
		wnm_cfg->timbc_timer_sta = wlc->pub->now + TIMBC_DELAY_RETRY;
	}

	return pbody;
}
static int
wlc_wnm_parse_tfs_resp_se(wnm_bsscfg_cubby_t* wnm_cfg, wnm_tfs_fset_t *fset,
	uint8 *body, int body_len)
{
	wnm_tfs_filter_t *filter = fset->filter_head;

	fset->status = TFS_STATUS_VALIDATED;

	while (body_len) {
		dot11_tfs_status_se_t *sse = (dot11_tfs_status_se_t *) body;

		if (!bcm_valid_tlv(sse, body_len)) {
			WL_WNM(("Malformed TFS status SE\n"));
			return BCME_ERROR;
		}

		if (sse->sub_id == DOT11_TFS_RESP_TFS_STATUS_SE_ID) {
			if (sse->len < DOT11_TFS_STATUS_SE_LEN) {
				WL_WNM(("Malformed TFS status SE 2\n"));
				return BCME_ERROR;
			}

			if (filter == NULL) {
				WL_WNM(("TFS status SE does not match internal one\n"));
				return BCME_ERROR;
			}

			filter->status = sse->resp_st;
			if (filter->status != DOT11_TFS_STATUS_ACCEPT)
				fset->status = TFS_STATUS_DENIED;

			if (filter->status == DOT11_TFS_STATUS_ALTPREF_POLICY ||
				filter->status == DOT11_TFS_STATUS_ALTPREF_TCLAS_UNSUPP) {
				dot11_tfs_se_t *se = (dot11_tfs_se_t *) body;
				if (se->sub_id != DOT11_TFS_RESP_TFS_SE_ID ||
					!bcm_valid_tlv(se, sse->len - DOT11_TFS_STATUS_SE_LEN) ||
					(se->len < TLV_HDR_LEN
						+ DOT11_TCLAS_IE_LEN + DOT11_TCLAS_FC_MIN_LEN)) {
					WL_WNM(("Missing/malformed TFS SE IE\n"));
					return BCME_ERROR;
				}

				/* TODO: We can override all internal TCLASs here if policy is to
				 * accept AP Alternate Preferred TCLASs.
				 */
				body += TLV_HDR_LEN + se->len;
				body_len -= TLV_HDR_LEN + se->len;
			}

			filter = filter->next;
		}
		/* SE can also be a vendor one, and it is ignored in this case */

		body += TLV_HDR_LEN + sse->len;
		body_len -= TLV_HDR_LEN + sse->len;
	}

	if (filter) {
		WL_WNM(("Filters missing in TFS response\n"));
		return BCME_ERROR;
	}

	return BCME_OK;
}
static int
wlc_wnm_tfs_resp_ie_parse(wlc_info_t *wlc, wnm_bsscfg_cubby_t* wnm_cfg, uint8 *body,
	int body_len, int token)
{
	/* Trick to modify tfs_fset_head seamlessly */
	wnm_tfs_fset_t *prev = (wnm_tfs_fset_t *) &wnm_cfg->tfs_fset_head;
	wnm_tfs_fset_t *fset;

	while (body_len) {
		dot11_tfs_resp_ie_t *resp = (dot11_tfs_resp_ie_t *) body;

		if (resp->id != DOT11_MNG_TFS_RESPONSE_ID ||
			!bcm_valid_tlv(resp, body_len) ||
			resp->len < DOT11_TFS_RESP_IE_LEN) {
			WL_WNM(("Malformed TFS response IE\n"));
			return BCME_ERROR;
		}

		/* User ID already taken ? */
		for (fset = wnm_cfg->tfs_fset_head; fset; fset = fset->next)
			if (resp->tfs_id == fset->tfs_id)
				break;

		if (fset == NULL) {
			WL_WNM(("TFS resp ID %d do not match any internal one\n", resp->tfs_id));
			return BCME_ERROR;
		}

		if (fset->token != token) {
			WL_WNM(("TFS response token mismatch: %d != %d\n", fset->token, token));
			return BCME_ERROR;
		}

		if (wlc_wnm_parse_tfs_resp_se(wnm_cfg, fset,
			resp->data, resp->len - DOT11_TFS_RESP_IE_LEN))
			return BCME_ERROR;

		body += TLV_HDR_LEN + resp->len;
		body_len -= TLV_HDR_LEN + resp->len;
	}

	for (fset = prev->next; fset; prev = fset, fset = fset->next) {

		if (fset->status != TFS_STATUS_DISABLING)
			continue;

		/* TFS ID is used to tag the filter for deletipon */
		if (fset->tfs_id)
			fset->status = TFS_STATUS_DISABLED;
		else {
			/* Delete filter now that AP has sent confirmation */
			prev->next = fset->next;
			wlc_wnm_tfs_fset_free(wlc, fset);
			/* Next iteration need valid fset */
			fset = prev;
		}
	}

	wnm_cfg->tfs_timer = 0;
	wnm_cfg->tfs_req_cnt = 0;

	return BCME_OK;
}
static int
wlc_wnm_tfs_req_filter_prep(wnm_bsscfg_cubby_t* wnm_cfg, uint8 *body, wnm_tfs_filter_t *filter)
{
	uint8 *body_start = body;

	body += wlc_wnm_tclas_ie_prep(filter->tclas_head, body);

	/* Add possible TCLAS Proc */
	if (filter->tclas_cnt > 1) {
		dot11_tclas_proc_ie_t *tcpro_ie = (dot11_tclas_proc_ie_t *) body;
		tcpro_ie->id = DOT11_MNG_TCLAS_PROC_ID;
		tcpro_ie->len = DOT11_TCLAS_PROC_IE_LEN - TLV_HDR_LEN;
		tcpro_ie->process = filter->tclas_proc;
		body += DOT11_TCLAS_PROC_IE_LEN;
	}

	return (int)(body - body_start);
}
static int
wlc_wnm_tfs_req_fset_prep(wnm_bsscfg_cubby_t* wnm_cfg, uint8 *body, wnm_tfs_fset_t *fset)
{
	uint8 *body_start = body;
	wnm_tfs_filter_t *filter;

	for (filter = fset->filter_head; filter; filter = filter->next) {
		dot11_tfs_se_t *se = (dot11_tfs_se_t *)body;
		se->sub_id = DOT11_TFS_REQ_TFS_SE_ID;
		body += TLV_HDR_LEN + wlc_wnm_tfs_req_filter_prep(wnm_cfg, se->data, filter);
		se->len = (uint8)(body - (TLV_HDR_LEN + (uint8*) se));
	}

	return (int)(body - body_start);
}
static int
wlc_wnm_tfs_req_ie_prep(wnm_bsscfg_cubby_t* wnm_cfg, uint8 *body, uint8 token)
{
	uint8 *body_start = body;
	wnm_tfs_fset_t *fset;

	for (fset = wnm_cfg->tfs_fset_head; fset; fset = fset->next) {
		/* Prepare TFS request element fields */
		dot11_tfs_req_ie_t *req_ie = (dot11_tfs_req_ie_t *) body;

		if (fset->status != TFS_STATUS_VALIDATED &&
			fset->status != TFS_STATUS_VALIDATING)
			continue;

		req_ie->id = DOT11_MNG_TFS_REQUEST_ID;
		req_ie->tfs_id = fset->tfs_id;
		req_ie->actcode = fset->actcode;
		body += TLV_HDR_LEN + DOT11_TFS_REQ_IE_LEN +
			wlc_wnm_tfs_req_fset_prep(wnm_cfg, req_ie->data, fset);
		/* TODO: handle > 255 cases */
		req_ie->len = (uint8)(body - (TLV_HDR_LEN + (uint8*) req_ie));

		fset->token = token;
		fset->notify = FALSE;
	}

	return (int)(body - body_start);
}
static int
wlc_wnm_tfs_req_frame_send(wlc_bsscfg_t *bsscfg)
{
	wlc_info_t *wlc = bsscfg->wlc;
	wnm_bsscfg_cubby_t* wnm_cfg = WNM_BSSCFG_CUBBY(wlc->wnm_info, bsscfg);
	struct scb *scb;
	void *p;
	uint8 *body;
	int body_len = DOT11_TFS_REQ_LEN;
	dot11_tfs_req_t *req;
	wnm_tfs_fset_t *fset;

	if (wnm_cfg->tfs_fset_head == NULL)
		return BCME_NOTREADY;

	/* Consider max possible IE size for frame allocation */
	for (fset = wnm_cfg->tfs_fset_head; fset; fset = fset->next)
		if (fset->status == TFS_STATUS_VALIDATED || fset->status == TFS_STATUS_VALIDATING)
			body_len += TLV_HDR_LEN + 255;
	body_len = MIN(DOT11_MAX_MPDU_BODY_LEN, body_len);

	p = wlc_frame_get_action(wlc, &bsscfg->BSSID, &bsscfg->cur_etheraddr,
		&bsscfg->BSSID, body_len, &body, DOT11_ACTION_CAT_WNM);
	if (p == NULL)
		return BCME_ERROR;

	/* Prepare TFS request frame fields */
	req = (dot11_tfs_req_t *) body;
	req->category = DOT11_ACTION_CAT_WNM;
	req->action = DOT11_WNM_ACTION_TFS_REQ;
	req->token = WLC_WNM_UPDATE_TOKEN(wlc->wnm_info->req_token);
	body += DOT11_TFS_REQ_LEN;
	/* TODO: handle > DOT11_MAX_MPDU_BODY_LEN cases */
	body += wlc_wnm_tfs_req_ie_prep(wnm_cfg, body, req->token);

	PKTSETLEN(wlc->osh, p, ((uint)(body - PKTDATA(wlc->osh, p))));
	prhex("Raw TFS Req", (uint8*) req, (uint)(body - (uint8*) req));

	scb = wlc_scbfind(wlc, bsscfg, &bsscfg->BSSID);
	wlc_sendmgmt(wlc, p, bsscfg->wlcif->qi, scb);

	/* set time out to manage requests in progress not answered */
	wnm_cfg->tfs_timer = wlc->pub->now + TFS_DELAY_TIMEOUT;

	return BCME_OK;
}

static int
wlc_wnm_dms_desc_prep(wnm_dms_info_t *dms_info, uint8 *p, uint type, uint token, uint user_id)
{
	wnm_dms_desc_t *dms_desc;
	int retlen = 0;

	for (dms_desc = dms_info->dms_desc_head; dms_desc; dms_desc = dms_desc->next) {
		dot11_dms_req_desc_t *desc = (dot11_dms_req_desc_t *)p;

		if (user_id && user_id != dms_desc->user_id)
			continue;

		if (dms_desc->status != DMS_STATUS_IN_PROGRESS)
			continue;

		/* Update internal desc */
		dms_desc->token = (uint8) token;

		desc->dms_id = dms_desc->dms_id;
		/* length filled later */
		desc->type = (uint8) type;
		p += DOT11_DMS_REQ_DESC_LEN;

		if (type == DOT11_DMS_REQ_TYPE_ADD) {
			/* Add all TCLAS */
			p += wlc_wnm_tclas_ie_prep(dms_desc->tclas_head, desc->data);

			/* Add possible TCLAS Proc */
			if (dms_desc->tclas_cnt > 1) {
				dot11_tclas_proc_ie_t *tcpro_ie = (dot11_tclas_proc_ie_t *) p;
				tcpro_ie->id = DOT11_MNG_TCLAS_PROC_ID;
				tcpro_ie->len = DOT11_TCLAS_PROC_IE_LEN - TLV_HDR_LEN;
				tcpro_ie->process = dms_desc->tclas_proc;
				p += DOT11_TCLAS_PROC_IE_LEN;
			}

			/* TSPEC not supported */
		}
		/* DMS subelem not supported */

		/* length is full descriptor minus DMSID and length field */
		desc->len = (uint8)(p - ((uint8*) desc) - 2);
		retlen += (int)(p - (uint8*) desc);
	}

	return retlen;
}

/* Merge this function with wlc_wnm_dms_desc_prep() ? */
static int
wlc_wnm_dms_desc_len(wnm_dms_info_t *dms_info, uint type, uint user_id)
{
	int retlen = 0;
	wnm_dms_desc_t *dms_desc;

	/* Calculate the total length of dms request descriptor list */
	for (dms_desc = dms_info->dms_desc_head; dms_desc; dms_desc = dms_desc->next) {
		wnm_tclas_t *tclas_curr;

		if (user_id && user_id != dms_desc->user_id)
			continue;

		if (dms_desc->status != DMS_STATUS_IN_PROGRESS)
			continue;

		if (type == DOT11_DMS_REQ_TYPE_ADD) {
			/* Calculate the total tclas length in this dms_desc */
			tclas_curr = dms_desc->tclas_head;
			while (tclas_curr) {
				retlen += DOT11_TCLAS_IE_LEN + tclas_curr->fc_len;
				tclas_curr = tclas_curr->next;
			}

			if (dms_desc->tclas_cnt > 1)
				retlen += DOT11_TCLAS_PROC_IE_LEN;

			/* TSPEC not supported */
		}

		retlen += DOT11_DMS_REQ_DESC_LEN;

		/* DMS subelem not supported */
	}

	return retlen;
}

static void*
wlc_frame_get_dms_req(wlc_info_t *wlc, wlc_wnm_info_t *wnm,
	uint32 dms_desc_list_len, uint8 **pbody)
{
	void *p;
	uint8 *body;
	dot11_dms_req_t *req;
	dot11_dms_req_ie_t *req_ie;
	uint32 body_len = dms_desc_list_len + DOT11_DMS_REQ_LEN + DOT11_DMS_REQ_IE_LEN;

	p = wlc_frame_get_action(wlc, &wlc->cfg->BSSID, &wlc->pub->cur_etheraddr,
		&wlc->cfg->BSSID, body_len, &body, DOT11_ACTION_CAT_WNM);
	if (p == NULL)
		return NULL;

	/* Prepare DMS request frame fields */
	req = (dot11_dms_req_t *) body;
	req->category = DOT11_ACTION_CAT_WNM;
	req->action = DOT11_WNM_ACTION_DMS_REQ;
	WLC_WNM_UPDATE_TOKEN(wnm->req_token);
	req->token = wnm->req_token;
	body += DOT11_DMS_REQ_LEN;

	/* Prepare DMS request element fields */
	req_ie = (dot11_dms_req_ie_t *) body;
	req_ie->id = DOT11_MNG_DMS_REQUEST_ID;
	req_ie->len = (uint8) dms_desc_list_len;
	body += DOT11_DMS_REQ_IE_LEN;

	*pbody = body;
	return p;
}

static int
wlc_wnm_dms_req_frame_send(wlc_bsscfg_t *bsscfg, uint type, uint user_id)
{
	wlc_info_t *wlc = bsscfg->wlc;
	wlc_wnm_info_t *wnm = wlc->wnm_info;
	wnm_bsscfg_cubby_t* wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);
	wnm_dms_info_t *dms_info = &wnm_cfg->dms_info;
	void *p;
	uint8 *body;
	uint32 dms_desc_list_len;

	if (dms_info->dms_desc_head == NULL)
		return BCME_NOTREADY;

	/* TODO: manage len > 255 case */
	dms_desc_list_len = wlc_wnm_dms_desc_len(dms_info, type, user_id);
	if (dms_desc_list_len == 0 || dms_desc_list_len > 255) {
		WL_WNM(("wl%d: %s: No DMS desc valid for this request, or too many\n",
			wnm->wlc->pub->unit, __FUNCTION__));
		return BCME_ERROR;
	}

	p = wlc_frame_get_dms_req(wlc, wlc->wnm_info, dms_desc_list_len, &body);

	if (p == NULL)
		return BCME_ERROR;

	body += wlc_wnm_dms_desc_prep(dms_info, body, type, wnm->req_token, user_id);

#ifdef BCMDBG
	prhex("DMS-req descr list", body - dms_desc_list_len, dms_desc_list_len);
#endif /* BCMDBG */

	wlc_sendmgmt(wlc, p, wlc->cfg->wlcif->qi, NULL);

	return BCME_OK;
}
static void
wlc_wnm_sleep_req_frame_send(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg)
{
	wlc_info_t *wlc = wnm->wlc;
	wnm_bsscfg_cubby_t *wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);
	struct scb *scb;
	void *p;
	int maxlen = DOT11_WNM_SLEEP_REQ_LEN + TLV_HDR_LEN + DOT11_WNM_SLEEP_IE_LEN;
	uint8 *body;
	dot11_wnm_sleep_req_t *req;
	wnm_tfs_fset_t *fset;

	/* Consider max possible IE size for frame allocation */
	for (fset = wnm_cfg->tfs_fset_head; fset; fset = fset->next)
		if (fset->status == TFS_STATUS_VALIDATED || fset->status == TFS_STATUS_VALIDATING)
			maxlen += TLV_HDR_LEN + 255;
	maxlen = MIN(DOT11_MAX_MPDU_BODY_LEN, maxlen);
	if ((p = wlc_frame_get_mgmt(wlc, FC_ACTION, &bsscfg->BSSID,
		&bsscfg->cur_etheraddr, &bsscfg->BSSID, maxlen, &body)) == NULL) {
		return;
	}

	/* Prepare WNM-Sleep request frame fields */
	req = (dot11_wnm_sleep_req_t *)body;
	req->category = DOT11_ACTION_CAT_WNM;
	req->action = DOT11_WNM_ACTION_WNM_SLEEP_REQ;
	WLC_WNM_UPDATE_TOKEN(wnm->req_token);
	req->token = wnm->req_token;

	body += DOT11_WNM_SLEEP_REQ_LEN;
	body += wlc_wnm_sleep_req_ie_prep(wnm, wnm_cfg, body);
	/* TODO: handle > DOT11_MAX_MPDU_BODY_LEN cases */
	body += wlc_wnm_tfs_req_ie_prep(wnm_cfg, body, req->token);

	ASSERT(body - PKTDATA(wlc->osh, p) <= maxlen);
	PKTSETLEN(wlc->osh, p, (uint)(body - PKTDATA(wlc->osh, p)));

	scb = wlc_scbfind(wlc, bsscfg, &bsscfg->BSSID);
	wlc_sendmgmt(wlc, p, bsscfg->wlcif->qi, scb);

	/* set time out to manage requests in progress not answered */
	wnm_cfg->sleep_timer = wlc->pub->now + SLEEP_DELAY_TIMEOUT;
}

static int
wlc_wnm_sleep_req_ie_prep(wlc_wnm_info_t *wnm, wnm_bsscfg_cubby_t *wnm_cfg, uint8 *p)
{
	dot11_wnm_sleep_ie_t *ie = (dot11_wnm_sleep_ie_t *)p;

	ie->id = DOT11_MNG_WNM_SLEEP_MODE_ID;
	ie->len = DOT11_WNM_SLEEP_IE_LEN;

	if (wnm_cfg->sleep_state == WNM_SLEEP_ENTER_WAIT)
		ie->act_type = DOT11_WNM_SLEEP_ACT_TYPE_ENTER;
	else if (wnm_cfg->sleep_state == WNM_SLEEP_EXIT_WAIT)
		ie->act_type = DOT11_WNM_SLEEP_ACT_TYPE_EXIT;
	else
		ASSERT(0);

	ie->resp_status = 0;

	/* TODO: limit interval to BSS Max Idle time */
	if (ie->act_type == DOT11_WNM_SLEEP_ACT_TYPE_ENTER)
		htol16_ua_store(wnm_cfg->sleep_intv, (uint8*)&(ie->interval));
	else
		ie->interval = 0;

	return (TLV_HDR_LEN + DOT11_WNM_SLEEP_IE_LEN);
}

static int
wlc_wnm_sleep_resp_frame_parse(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg, struct scb *scb,
	uint8 *body, int body_len)
{
	dot11_wnm_sleep_resp_t *frame;
	int offset = DOT11_WNM_SLEEP_RESP_LEN;
	wnm_bsscfg_cubby_t *wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);

	frame = (dot11_wnm_sleep_resp_t *)body;

	/* TODO: Problem here: the token may be updated by other requests sent */
	if (frame->token != wnm->req_token) {
		WL_WNM(("wl%d: %s: unmatched req_token 0x%02x, resp_token 0x%02x\n",
			wnm->wlc->pub->unit, __FUNCTION__, wnm->req_token, frame->token));

		return BCME_ERROR;
	}

	if (frame->key_len > 0) {
		if ((frame->key_len + DOT11_WNM_SLEEP_RESP_LEN) >= body_len) {
			WL_WNM(("wl%d: %s: key data length too large\n",
				wnm->wlc->pub->unit, __FUNCTION__));
			return BCME_ERROR;
		}

		if (wlc_wnm_sleep_resp_keydata_parse(wnm, bsscfg, scb, frame->data,
			(int) frame->key_len)) {
			WL_WNM(("WNM Sleep key parsing error\n"));
			return BCME_ERROR;
		}
	}

	offset += frame->key_len;
	if (wlc_wnm_sleep_resp_ie_parse(wnm, scb, body + offset,
	    TLV_HDR_LEN + DOT11_WNM_SLEEP_IE_LEN, frame->key_len)) {
		WL_WNM(("WNM Sleep resp ie parsing error\n"));
		return BCME_ERROR;
	}

	offset += (TLV_HDR_LEN + DOT11_WNM_SLEEP_IE_LEN);

	if (wlc_wnm_tfs_resp_ie_parse(wnm->wlc, wnm_cfg,
		body + offset, body_len - offset, frame->token)) {
		WL_WNM(("WNM Sleep TFS IE error\n"));
		return BCME_ERROR;
	}

	wnm_cfg->sleep_req_cnt = 0;
	return BCME_OK;
}

static int
wlc_wnm_sleep_resp_keydata_parse(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg,
	struct scb *scb, uint8 *key, int len)
{
	int err;
#ifdef MFP
	err = BCME_OK;
	while (len) {
		wlc_info_t *wlc = wnm->wlc;
		if (TLV_HDR_LEN + key[1] > len) {
			return BCME_BADLEN;
		}

		if (key[0] == DOT11_WNM_SLEEP_SUBELEM_ID_GTK) {
			/* process gtk */
			dot11_wnm_sleep_subelem_gtk_t *gtk =
				(dot11_wnm_sleep_subelem_gtk_t*) key;

			if (gtk->len != DOT11_WNM_SLEEP_SUBELEM_GTK_FIXED_LEN + gtk->key_length) {
				err = BCME_ERROR;
				goto done;
			}
#ifdef BCMSUP_PSK
			if (SUP_ENAB(wlc->pub)) {
				wlc_wpa_sup_gtk_update(wlc->idsup, bsscfg,
					gtk->key_info & WPA2_GTK_INDEX_MASK,
					gtk->key_length, gtk->key, gtk->rsc);
			}
#endif /* BCMSUP_PSK */

		} else if (key[0] == DOT11_WNM_SLEEP_SUBELEM_ID_IGTK) {
			wlc_key_t *wlc_key;
			wlc_key_info_t wlc_key_info;

			/* process igtk */
			dot11_wnm_sleep_subelem_igtk_t *igtk =
				(dot11_wnm_sleep_subelem_igtk_t *) key;

			if (igtk->len != DOT11_WNM_SLEEP_SUBELEM_IGTK_LEN) {
				err = BCME_ERROR;
				goto done;
			}

			wlc_key = wlc_keymgmt_get_bss_key(wlc->keymgmt, bsscfg,
				(wlc_key_id_t)(igtk->key_id), &wlc_key_info);
			err =  wlc_key_set_data(wlc_key, CRYPTO_ALGO_BIP, igtk->key,
				igtk->len - sizeof(igtk->key_id) - sizeof(igtk->pn));
			if (err != BCME_OK)
				goto done;
			err = wlc_key_set_seq(wlc_key, igtk->pn, sizeof(igtk->pn), 0, FALSE);
			if (err != BCME_OK) {
				break;
			}
		}
		len -= TLV_HDR_LEN + key[1];
		key += TLV_HDR_LEN + key[1];
	}

done:

#else /* MFP */
	err = BCME_UNSUPPORTED;
#endif /* !MFP */

	return err;
}

static int
wlc_wnm_sleep_resp_ie_parse(wlc_wnm_info_t *wnm, struct scb *scb, uint8 *p, int len, int key_len)
{
	bcm_tlv_t *tlv;
	dot11_wnm_sleep_ie_t *ie;
	wnm_bsscfg_cubby_t *wnm_cfg = WNM_BSSCFG_CUBBY(wnm, scb->bsscfg);

	tlv = bcm_parse_tlvs(p, len, DOT11_MNG_WNM_SLEEP_MODE_ID);
	if (tlv == NULL)
		return BCME_ERROR;

	ie = (dot11_wnm_sleep_ie_t *)tlv;
	if (ie->len != DOT11_WNM_SLEEP_IE_LEN) {
		WL_WNM(("wl%d: %s: incorrect sleep resp ie length %d bytes\n",
			wnm->wlc->pub->unit, __FUNCTION__, ie->len));
		return BCME_ERROR;
	}

	if (ie->act_type == DOT11_WNM_SLEEP_ACT_TYPE_ENTER &&
		wnm_cfg->sleep_state == WNM_SLEEP_ENTER_WAIT)
	{
		switch (ie->resp_status) {
		case DOT11_WNM_SLEEP_RESP_UPDATE:
			if (!key_len)
				WL_WNM(("wl%d: %s: no key but update ie\n",
					wnm->wlc->pub->unit, __FUNCTION__));
		case DOT11_WNM_SLEEP_RESP_ACCEPT: {
			/* Put STA into WNM-Sleep mode here */
			WL_WNM(("wl%d: %s: enter WNM-Sleep mode accepted\n",
				wnm->wlc->pub->unit, __FUNCTION__));

			wnm_cfg->sleep_state = WNM_SLEEP_SLEEPING;
			wnm_cfg->sleep_intv = ie->interval;

			break;
		}
		case DOT11_WNM_SLEEP_RESP_DENY:
			/* AP refused, abandon procedure */
			WL_WNM(("wl%d: %s: AP refused enter sleep req\n",
				wnm->wlc->pub->unit, __FUNCTION__));
			wnm_cfg->sleep_state = WNM_SLEEP_NONE;

			break;
		case DOT11_WNM_SLEEP_RESP_DENY_TEMP:
			/* AP refused temporarily, abandon procedure */
			WL_WNM(("wl%d: %s: AP postpone enter sleep req\n",
				wnm->wlc->pub->unit, __FUNCTION__));
			wnm_cfg->sleep_timer = wnm->wlc->pub->now + SLEEP_DELAY_RETRY_LATER;
			break;

		case DOT11_WNM_SLEEP_RESP_DENY_KEY:
		case DOT11_WNM_SLEEP_RESP_DENY_INUSE:
			/* TODO */
			break;
		default:
			WL_WNM(("wl%d: %s: WNM-Sleep response status not supported %d\n",
				wnm->wlc->pub->unit, __FUNCTION__, ie->resp_status));
			return BCME_ERROR;
		}
	} else if (ie->act_type == DOT11_WNM_SLEEP_ACT_TYPE_EXIT &&
		wnm_cfg->sleep_state == WNM_SLEEP_EXIT_WAIT)
	{
		switch (ie->resp_status) {
		case DOT11_WNM_SLEEP_RESP_ACCEPT:
			WL_WNM(("wl%d: %s: exit WNM-Sleep mode accepted\n",
				wnm->wlc->pub->unit, __FUNCTION__));

			/* Put STA out of WNM-Sleep mode here */
			wnm_cfg->sleep_state = WNM_SLEEP_NONE;

			if (wlc_wnm_sleep_remove_tfs_filter(wnm->wlc->cfg) != BCME_OK) {
				WL_ERROR(("Error removing TFS for WNM-Sleep mode\n"));
				return BCME_ERROR;
			}
			break;
		case DOT11_WNM_SLEEP_RESP_DENY: /* Forced to stay in sleep!? */

		case DOT11_WNM_SLEEP_RESP_UPDATE:
		case DOT11_WNM_SLEEP_RESP_DENY_TEMP:
		case DOT11_WNM_SLEEP_RESP_DENY_INUSE:
		case DOT11_WNM_SLEEP_RESP_DENY_KEY:
			/* In all refused cases, retry later */
			WL_WNM(("wl%d: %s: AP postpone enter sleep req\n",
				wnm->wlc->pub->unit, __FUNCTION__));
			wnm_cfg->sleep_timer = wnm->wlc->pub->now + SLEEP_DELAY_RETRY_LATER;
			break;
		default:
			WL_WNM(("WNM Sleep resp status error\n"));
			return BCME_ERROR;
		}
	} else {
			WL_WNM(("wl%d: %s: incorrect action type 0x%02x\n",
				wnm->wlc->pub->unit, __FUNCTION__, ie->act_type));
			return BCME_ERROR;
	}

	return BCME_OK;
}

static int
wlc_wnm_exit_sleep_mode(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_wnm_info_t *wnm = wlc->wnm_info;
	wnm_bsscfg_cubby_t *wnm_cfg = WNM_BSSCFG_CUBBY(wnm, cfg);
	int err = BCME_OK;

	WL_WNM(("wl%d.%d: Exit WNM-Sleep mode. Reset DTIM period to %d\n",
		wlc->pub->unit, WLC_BSSCFG_IDX(cfg),
		cfg->current_bss->dtim_period));

	wnm_cfg->not_intim_cont = 0;

	(void)wlc_wnm_set_sleep_mode(wlc, cfg, wnm, wnm_cfg, FALSE);

	/* reset DTIM interval to the initial */
	err = wlc_set_dtim_period(cfg, cfg->current_bss->dtim_period);

	return err;
}

static int
wlc_wnm_upd_sleep_period(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_wnm_info_t *wnm = wlc->wnm_info;
	wnm_bsscfg_cubby_t *wnm_cfg = WNM_BSSCFG_CUBBY(wnm, cfg);
	uint16 sleep_prd;
	int err = BCME_OK;

	if ((err = wlc_get_dtim_period(cfg, &sleep_prd)) != BCME_OK) {
		WL_ERROR(("wlc_get_dtim_period error %d\n", err));
		return err;
	}

	/* set new sleep period in number of DTIMs */
	sleep_prd = MIN(sleep_prd + WNM_SLEEP_UPD, wnm_cfg->sleep_intv);

	WL_WNM(("Update sleep period to %d\n", sleep_prd));

	err = wlc_set_dtim_period(cfg, sleep_prd);

	return err;
}

/* This function is an entry point to WNM-Sleep Mode extension for PM_FAST.
 * It is called by wlc_bcn_parse_tim_ie each DTIM interval when STA
 * is in PM_FAST mode.
 */
int
wlc_wnm_update_sleep_mode(wlc_bsscfg_t *cfg, bool intim)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_wnm_info_t *wnm = wlc->wnm_info;
	wnm_bsscfg_cubby_t *wnm_cfg = WNM_BSSCFG_CUBBY(wnm, cfg);
	int err = BCME_OK;

	ASSERT(cfg->pm->PM == PM_FAST);

	if (wnm_cfg->sleep_state != WNM_SLEEP_SLEEPING) {
		return BCME_NOTREADY;
	}
	if (intim) {
		/* exit WNM-Sleep mode */
		err = wlc_wnm_exit_sleep_mode(cfg);
	} else if (!(++wnm_cfg->not_intim_cont % WNM_SLEEP_UPD_INTV)) {
		/* update sleep period */
		err = wlc_wnm_upd_sleep_period(cfg);
	}

	return err;
}

/* This function is called by PM_FAST mode when PS mode is entered */
int
wlc_wnm_enter_sleep_mode(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_wnm_info_t *wnm = wlc->wnm_info;
	wnm_bsscfg_cubby_t *wnm_cfg = WNM_BSSCFG_CUBBY(wnm, cfg);
	int err = BCME_OK;
	int sleep_intv;

	if (!wnm_cfg->bss_max_idle_period) {
		return BCME_ERROR;
	}

	/* set max sleep interval in number of DTIMs */
	sleep_intv = wnm_cfg->bss_max_idle_period * 1000 /
		(cfg->current_bss->dtim_period * cfg->current_bss->beacon_period);

	if ((err = wlc_wnm_set_sleep_intv(wlc, cfg, wnm, wnm_cfg, sleep_intv)) != BCME_OK) {
		WL_ERROR(("Error %d entering WNM-Sleep mode\n", err));
		return err;
	}

	WL_WNM(("entering WNM-Sleep mode for max sleep period %d\n", sleep_intv));

	err = wlc_wnm_set_sleep_mode(wlc, cfg, wnm, wnm_cfg, TRUE);

	return err;
}

/* add TFS for Sleep-Mode */
static int
wlc_wnm_sleep_add_tfs_filter(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;

	int err = BCME_OK;

	uint8 len = DOT11_TCLAS_FC_0_ETH_LEN + 1;
	wl_tfs_set_t tfs = {0, 1, 0, 0};
	dot11_tclas_fc_t fc;

	fc.data[0] = 0;
	fc.hdr.type = DOT11_TCLAS_FC_0_ETH;
	fc.hdr.mask = 0;

	err = wlc_iovar_op(wlc, "tclas_add", NULL, 0, &fc,
		len, IOV_SET, cfg->wlcif);

	if (err != BCME_OK) {
		return err;
	}

	err = wlc_iovar_op(wlc, "wnm_tfs_set", NULL, 0, &tfs,
		sizeof(tfs), IOV_SET, cfg->wlcif);

	return err;
}

/* remove Sleep-Mode TFS filter */
static int
wlc_wnm_sleep_remove_tfs_filter(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;

	int err = BCME_OK;

	wl_tfs_term_t tfs = {1, 1};

	err = wlc_iovar_op(wlc, "wnm_tfs_term", NULL, 0, &tfs,
		sizeof(tfs), IOV_SET, cfg->wlcif);

	return err;
}
#endif /* STA */

#ifdef WLWNM_AP
static int BCMFASTPATH
wlc_wnm_proxyarp_packets_handle(wlc_bsscfg_t *bsscfg, void *sdu, frame_proto_t *fp, bool istx)
{
	void *reply = NULL;
	uint8 reply_to_bss = 0;
	wlc_info_t *wlc = bsscfg->wlc;
	uint8 result = FRAME_NOP;
	/* update ipv4 and reply */
	if (fp->l3_t == FRAME_L3_ARP_H)
		result = wnm_proxyarp_arp_handle(wlc, bsscfg, fp, &reply, &reply_to_bss, istx);
	else if (fp->l4_t == FRAME_L4_UDP_H)
		result = wnm_proxyarp_dhcp4_handle(wlc, bsscfg, fp, istx);
	else if (fp->l4_t == FRAME_L4_ICMP6_H)
		result = proxyarp_icmp6_handle(wlc, bsscfg, fp, &reply, &reply_to_bss, istx);

	switch (result) {
	case FRAME_TAKEN:
		if (reply != NULL) {
			if (reply_to_bss) {
				WL_WNM(("wl%d: sendout proxy-reply frame\n",
					WLC_BSSCFG_IDX(bsscfg)));
				wlc_sendpkt(wlc, reply, bsscfg->wlcif);
			}
			else {
				WL_WNM(("wl%d: sendup proxy-reply frame\n",
					WLC_BSSCFG_IDX(bsscfg)));
				wlc_sendup_msdus(wlc, bsscfg, NULL, reply);
			}

			/* return OK to drop original packet */
			return BCME_OK;
		}

		break;
	case FRAME_DROP:
		return BCME_OK;

		break;
	default:
		break;
	}

	/* return fail to let original packet keep traverse */
	return BCME_ERROR;
}
#endif /* WLWNM_AP */

/*
 * Process WNM related packets.
 * return BCME_OK if the original packet need to be drop
 * return BCME_ERROR to let original packet go through
 */
int BCMFASTPATH
wlc_wnm_packets_handle(wlc_bsscfg_t *bsscfg, void *sdu, bool istx)
{
	wlc_info_t *wlc;
	wlc_wnm_info_t *wnm;
	wnm_bsscfg_cubby_t* wnm_cfg = NULL;
	frame_proto_t fp;
	int err = WNM_NOP;

	ASSERT(bsscfg != NULL);
	wlc = bsscfg->wlc;
	wnm = wlc->wnm_info;

	if (wnm == NULL)
		goto done;

	wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);
	/* return if none of following capability enabled */
	if (!WNM_PROXYARP_ENABLED(wnm_cfg->cap) &&
	    !WNM_TFS_ENABLED(wnm_cfg->cap) &&
	    !WNM_SLEEP_ENABLED(wnm_cfg->cap) &&
	    !WNM_DMS_ENABLED(wnm_cfg->cap) &&
	    !WNM_FMS_ENABLED(wnm_cfg->cap))
		goto done;

	/* get frame type */
	if (hnd_frame_proto(PKTDATA(wlc->osh, sdu), PKTLEN(wlc->osh, sdu), &fp) != BCME_OK)
		goto done;
#ifdef STA
	if (BSSCFG_STA(bsscfg)) {
		/*
		 * process DMS in receiving path. If A1 is multicast and packet matches
		 * any tclas in accepted DMS, drop it.
		 */
		if (!istx) {
			wnm_dms_desc_t *dms;
			for (dms = wnm_cfg->dms_info.dms_desc_head; dms; dms = dms->next) {
				/* If matching any accepted DMS, drop the 802.11 multicast copy */
				if ((dms->status == DMS_STATUS_ACCEPTED) &&
					wlc_wnm_tclas_match(dms->tclas_head, &fp, TRUE))
					err = WNM_DROP;
			}
		}
	}
	else
#endif /* STA */
	{
#ifdef WLWNM_AP

		if (fp.l3_t == FRAME_L3_8021X_EAPOLKEY_H)
			goto done;

		/* process proxyarp if it turned on */
		if (WNM_PROXYARP_ENABLED(wnm_cfg->cap) &&
		    wlc_wnm_proxyarp_packets_handle(bsscfg, sdu, &fp, istx) == BCME_OK) {
			/* drop original frame should return BCME_OK */
			err = WNM_DROP;
			goto done;
		}

		/* process TFS in sending path if the scb's TFS/Sleep turned on */
		if (istx &&
		    (WNM_TFS_ENABLED(wnm_cfg->cap) || WNM_SLEEP_ENABLED(wnm_cfg->cap)) &&
		    !ETHER_ISNULLADDR((struct ether_addr *)fp.l2)) {
			struct scb_iter scbiter;
			struct scb *scb = NULL;
			wnm_scb_cubby_t *wnm_scb;

			/* multicast/broadcast frame, need to iterate all scb in bss */
			if (ETHER_ISBCAST((struct ether_addr *)fp.l2) ||
			    ETHER_ISMULTI((struct ether_addr *)fp.l2)) {
				/* process TFS of each scb and bypass original frame */
				FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
					wnm_scb = SCB_WNM_CUBBY(wnm, scb);
					if ((SCB_TFS(wnm_scb->cap) ||
						SCB_WNM_SLEEP(wnm_scb->cap)) &&
						wnm_scb->tfs_fset_head != NULL) {
						wlc_wnm_tfs_packet_handle(wlc, bsscfg, scb, &fp);
					}
				}
			}
			else {
				scb = wlc_scbfind(wlc, bsscfg, (struct ether_addr *)fp.l2);
				if (scb) {
				wnm_scb = SCB_WNM_CUBBY(wnm, scb);
				/* this scb have subscribe TFS service, process this frame */
				if ((SCB_TFS(wnm_scb->cap) || SCB_WNM_SLEEP(wnm_scb->cap)) &&
					wnm_scb->tfs_fset_head != NULL) {
					/* send original frame if returned true */
					if (wlc_wnm_tfs_packet_handle(wlc, bsscfg, scb, &fp))
						goto done;

					/* if this frame is not matching tfs rule, drop it */
					err = WNM_DROP;
					goto done;
				}
				}
			}
		}

		/*
		 * process DMS in sending path if the packet is multicast after TFS,
		 * not a cloned frame, and bss have DMS rule
		 */
		if (istx &&
		    wlc_wnm_dms_amsdu_on(wlc, bsscfg) &&
		    ETHER_ISMULTI(fp.l2) &&
		    fp.l4_t != FRAME_L4_IGMP_H) {
			wnm_dms_desc_t *dms;
			uint8 assoc = wlc_bss_assocscb_getcnt(wlc, bsscfg);
			bool drop_mcast = FALSE;

			for (dms = wnm_cfg->dms_info.dms_desc_head; dms; dms = dms->next) {
				/* matching cur DMS rule, clone packets and
				 * send to registered scbs
				 */
				if (wlc_wnm_tclas_match(dms->tclas_head, &fp, TRUE)) {
					void *sdu_clone;
					wnm_dms_scb_t *cur;
					uint8 dmscb_cnt = 0;

					for (cur = dms->dms_scb_head; cur; cur = cur->next) {
						if (cur->scb == NULL ||
						    (sdu_clone = PKTDUP(wlc->osh, sdu)) == NULL) {
							WL_WNM(("dms clone error\n"));
							break;
						}

						/* save current frame with target scb */
						WLPKTTAGSCBSET(sdu_clone, cur->scb);

						/* Send the packet using bsscfg wlcif */
						wlc_sendpkt(wlc, sdu_clone, bsscfg->wlcif);

						dmscb_cnt++;
					}
					/*
					 * If all SCB subscribing this multicast frame,
					 * drop original multicast frame.
					 */
					if (assoc == dmscb_cnt)
						drop_mcast = TRUE;
				}
			}

			if (drop_mcast)
				err = WNM_DROP;
		}
#endif /* WLWNM_AP */
	}
done:
	return err;
}

static void
wlc_wnm_watchdog(void *context)
{
	wlc_wnm_info_t *wnm = (wlc_wnm_info_t *) context;
	wlc_info_t *wlc = wnm->wlc;
	wlc_bsscfg_t *bsscfg;
	wnm_bsscfg_cubby_t* wnm_cfg;
	int idx;
#ifdef STA
	wnm_bsstrans_roam_throttle_t *throttle;
#endif /* STA */
#ifdef WLWNM_AP
	FOREACH_UP_AP(wlc, idx, bsscfg) {
		wnm_cfg = WNM_BSSCFG_CUBBY(wlc->wnm_info, bsscfg);
		/* disable if proxyarp is not enabled */
		if (!WNM_PROXYARP_ENABLED(wnm_cfg->cap))
			continue;

		wlc_wnm_parp_watchdog(wlc, bsscfg, FALSE, NULL, TRUE);
	}
#endif /* WLWNM_AP */
#ifdef STA
	FOREACH_AS_STA(wlc, idx, bsscfg) {
		if (!bsscfg->BSS)
			continue;

		wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);

		if (wnm_cfg->tfs_timer == wlc->pub->now) {
			wnm_tfs_fset_t *fset;
			for (fset = wnm_cfg->tfs_fset_head; fset; fset = fset->next) {
				/* Check if there is pending req */
				if (fset->status == TFS_STATUS_VALIDATING ||
					fset->status == TFS_STATUS_DISABLING) {
					if (wnm_cfg->tfs_req_cnt++ < TFS_RETRY_CNT) {
						wlc_wnm_tfs_req_frame_send(bsscfg);
						break;
					} else
						fset->status = TFS_STATUS_DENIED;
				}
			}
		}
		if (wnm_cfg->timbc_timer_sta == wlc->pub->now) {
			wl_timbc_set_t *set = &wnm_cfg->timbc_set;
			wl_timbc_status_t *stat = &wnm_cfg->timbc_stat;

			int next_state = wlc_wnm_timbc_state_update(wnm, bsscfg, wnm_cfg);

			WL_WNM(("TIMBC timer update: %x: %x->%x\n",
				set->interval, stat->status_sta, next_state));

			/* send TIM Broadcast request if already associated */
			if (next_state != stat->status_sta) {

				if (next_state == WL_TIMBC_STATUS_ENABLE) {
					wlc_wnm_timbc_req_frame_send(wnm, bsscfg, set->interval);

				} else if (stat->status_sta == WL_TIMBC_STATUS_ENABLE)  {
					wlc_wnm_timbc_req_frame_send(wnm, bsscfg, 0);

				} else
					/* Change state only if AP is informed */
					next_state = stat->status_sta;
			}
		}

		if (wnm_cfg->sleep_timer == wlc->pub->now) {
			switch (wnm_cfg->sleep_state) {
			case WNM_SLEEP_ENTER_WAIT:
			case WNM_SLEEP_EXIT_WAIT:
				wnm_cfg->sleep_req_cnt++;
				if (wnm_cfg->sleep_req_cnt < SLEEP_RETRY_CNT) {
					wlc_wnm_sleep_req_frame_send(wnm, bsscfg);
					wnm_cfg->sleep_timer =
						wnm->wlc->pub->now + SLEEP_DELAY_TIMEOUT;

				} else {
					WL_WNM(("wl%d: %s: No rsp to sleep requests (state %d)\n",
						wnm->wlc->pub->unit, __FUNCTION__,
						wnm_cfg->sleep_state));

					/* AP did not respond to exit req, disconn from the nw */
					if (WNM_SLEEP_EXIT_WAIT == wnm_cfg->sleep_state) {
						WL_WNM(("AP dnt respond to sleep-exit(state %d)\n",
							wnm_cfg->sleep_state));
						wlc_disassociate_client(bsscfg, TRUE);
					}

					/* come back to previous state */
					wnm_cfg->sleep_state = WNM_SLEEP_NONE;

					/* + indication to user? other actions */
				}
				break;

			case WNM_SLEEP_SLEEPING:
			case WNM_SLEEP_NONE:
				WL_WNM(("wl%d: %s: Should not occur (state %d)\n",
					wnm->wlc->pub->unit, __FUNCTION__, wnm_cfg->sleep_state));
				break;
			}
		}
	}

	FOREACH_AS_STA(wlc, idx, bsscfg) {
		wnm_cfg = WNM_BSSCFG_CUBBY(wlc->wnm_info, bsscfg);
		throttle = wnm_cfg->bsstrans_stainfo->throttle;
		if (throttle->period) {
			throttle->period_secs++;
			/* End of period; start a new one */
			if (throttle->period_secs == throttle->period) {
				throttle->period_secs = 0;
				throttle->scans_done = 0;
			}
		}
	}
#endif /* STA */

}

#ifdef STA
#ifdef KEEP_ALIVE
static void
wlc_wnm_set_bss_max_idle_prd(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint16 new_prd, uint8 new_opt)
{
	wlc_wnm_info_t *wnm = wlc->wnm_info;
	wnm_bsscfg_cubby_t *wnm_cfg = WNM_BSSCFG_CUBBY(wnm, cfg);
	if (wnm_cfg != NULL) {
		wnm_cfg->bss_max_idle_period = new_prd; /* in 1000TUs */
		wnm_cfg->bss_idle_opt = new_opt;

		if (!WNM_MAXIDLE_ENABLED(wnm_cfg->cap))
			return;
		wlc_wnm_set_keepalive_max_idle(wlc, cfg, wnm_cfg);
	}
}

/* check if current AP is maxidle capable */
bool
wlc_wnm_maxidle_upd_reqd(wlc_info_t *wlc, struct scb *scb)
{
	wnm_scb_cubby_t *wnm_scb = NULL;
	bool ret = FALSE;

	ASSERT(scb);
	wnm_scb = SCB_WNM_CUBBY(wlc->wnm_info, scb);

	if (SCB_MAXIDLE(wnm_scb->cap)) {
		ret = TRUE;
	}
	return ret;
}

static int
wlc_wnm_bss_max_idle_ie_process(void *ctx, wlc_iem_parse_data_t *data)
{
	uint16 idle_period;
	wlc_info_t *wlc = (wlc_info_t *)ctx;
	wlc_bsscfg_t *bsscfg = data->cfg;
	wnm_bsscfg_cubby_t *wnm_cfg = NULL;
	dot11_bss_max_idle_period_ie_t *maxidle_ie = (dot11_bss_max_idle_period_ie_t *)data->ie;
	wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;
	struct scb *scb = ftpparm->assocresp.scb;
	wnm_scb_cubby_t *wnm_scb = NULL;

	ASSERT(bsscfg != NULL);
	ASSERT(wlc->wnm_info);
	wnm_cfg = WNM_BSSCFG_CUBBY(wlc->wnm_info, bsscfg);
	wnm_scb = SCB_WNM_CUBBY(wlc->wnm_info, scb);

	if (wnm_cfg != NULL && WNM_MAXIDLE_ENABLED(wnm_cfg->cap) &&
		maxidle_ie != NULL && maxidle_ie->len == DOT11_BSS_MAX_IDLE_PERIOD_IE_LEN) {
		idle_period = ltoh16_ua(&(maxidle_ie->max_idle_period));
		wnm_scb->cap |= WL_WNM_MAXIDLE;
		wlc_wnm_set_bss_max_idle_prd(wlc, bsscfg, idle_period, maxidle_ie->idle_opt);
	}
	return BCME_OK;
}
static int
wlc_wnm_set_keepalive_max_idle(wlc_info_t *wlc, wlc_bsscfg_t *cfg, wnm_bsscfg_cubby_t *wnm_cfg)
{
	uint32 override_period_msec;
	uint32 max_interval_ms = wnm_cfg->ka_max_interval * 1000;

	if (wnm_cfg->keepalive_count == 0 || wnm_cfg->bss_max_idle_period == 0)
		override_period_msec = 0;
	else
		/* Program the period such that last keepalive goes out at 95% of bss-max-idle */
		override_period_msec = 973 * wnm_cfg->bss_max_idle_period /
			wnm_cfg->keepalive_count;

	/* Floor the override_period_msec to 300 */
	if ((override_period_msec != 0) && (override_period_msec < 300))
		override_period_msec = 300;
	/* Cap the override_period_msec to max_interval */
	if (max_interval_ms)
		override_period_msec = (override_period_msec > max_interval_ms) ?
			max_interval_ms: override_period_msec;
	return wl_keep_alive_upd_override_period(wlc, cfg, override_period_msec,
		wnm_cfg->mkeepalive_index);
}
#endif /* KEEP_ALIVE */

#if defined(STA)
void
wlc_wnm_bssload_calc_reset(wlc_bsscfg_t *cfg)
{
	wnm_bsscfg_cubby_t *wnm_cfg = WNM_BSSCFG_CUBBY(cfg->wlc->wnm_info, cfg);

	wnm_cfg->load_chan_util = 0;
	wnm_cfg->load_bcn_count = 0;
	wnm_cfg->load_bcn_per_window = 0;
	wnm_cfg->load_avg_cu_ap = 0;
	wnm_cfg->load_avg_cu_self = 0;
	wnm_cfg->trigger_rssi_cu_roam = FALSE;
	wnm_cfg->load_next_window = cfg->wlc->pub->now + wnm_cfg->cu_avg_calc_dur;
}

void
wlc_wnm_set_cu_trigger_percent(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint8 cu_trigger)
{
	wnm_bsscfg_cubby_t *wnm_cfg = WNM_BSSCFG_CUBBY(wlc->wnm_info, cfg);

	wnm_cfg->cu_trigger_percent = cu_trigger;
}

int
wlc_wnm_get_cu_trigger_percent(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	wnm_bsscfg_cubby_t *wnm_cfg = WNM_BSSCFG_CUBBY(wlc->wnm_info, cfg);

	if (!wnm_cfg->cu_trigger_percent)
		return BCME_ERROR;

	return wnm_cfg->cu_trigger_percent;
}

void
wlc_wnm_set_cu_avg_calc_dur(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint8 cu_calc_dur)
{
	wnm_bsscfg_cubby_t *wnm_cfg = WNM_BSSCFG_CUBBY(wlc->wnm_info, cfg);

	wnm_cfg->cu_avg_calc_dur = cu_calc_dur;
}

int
wlc_wnm_is_wnmsleeping(wlc_info_t * wlc)
{

	int err;
	wlc_wnm_info_t *wnm;
	wnm_bsscfg_cubby_t* wnm_cfg;
	wlc_bsscfg_t *bsscfg;
	int idx;

	err = BCME_OK;
	wnm = wlc->wnm_info;
	FOREACH_AS_STA(wlc, idx, bsscfg) {
		if (!bsscfg->BSS)
			continue;
		if (wnm) {
			wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);
			if ((wnm_cfg) && (WNM_SLEEP_ENABLED(wnm_cfg->cap)) &&
				((WNM_SLEEP_SLEEPING == wnm_cfg->sleep_state)||
				(WNM_SLEEP_EXIT_WAIT == wnm_cfg->sleep_state))) {
				err = BCME_NOTREADY;
				WL_WNM(("STA in WNM sleep, cannot install keys \n"));
			}
		}
	}
	return err;
}

bool
wlc_wnm_get_trigger_rssi_cu_roam(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	wnm_bsscfg_cubby_t *wnm_cfg = WNM_BSSCFG_CUBBY(wlc->wnm_info, cfg);

	return wnm_cfg->trigger_rssi_cu_roam;
}

static void
wlc_wnm_avg_bssload_calc(wlc_bsscfg_t *cfg, uint8 channel_utilization)
{
	wlc_roam_t *roam = cfg->roam;
	uint8 cu_ap;
	uint8 cu_self = 0;
	uint32 cur_avg_cu_ap = 0;
	uint32 cur_avg_cu_self = 0;
	chanim_stats_t *chanim_stats = NULL;
	wnm_bsscfg_cubby_t *wnm_cfg = WNM_BSSCFG_CUBBY(cfg->wlc->wnm_info, cfg);
	BCM_REFERENCE(chanim_stats);
	BCM_REFERENCE(roam);

	if ((!WBTEXT_ACTIVE(cfg->wlc->pub) || !wnm_cfg->cu_trigger_percent) &&
#ifdef WL_OCE
	    (!OCE_ENAB(cfg->wlc->pub)) &&
#endif /* WL_OCE */
	TRUE) {
		WL_WBTEXT(("%s: roam_prof[%d].cu:%d%%. Ignoring QBSS IE.\n",
			__FUNCTION__, roam->roam_prof_idx, wnm_cfg->cu_trigger_percent));
		return;
	}

	WBTEXT_TRACE(("process BSS Load IE: channel_utilization %u\n",
		channel_utilization));

	/* convert channel load to percentage */
	cu_ap = channel_utilization * 100 / WL_MAX_CHANNEL_USAGE;

#ifdef WLCHANIM
	chanim_stats = wlc_lq_chanspec_to_chanim_stats(cfg->wlc->chanim_info,
			cfg->current_bss->chanspec);
	/* don't consider cu_self when scan in progress */
	if (chanim_stats && !roam->scan_block) {
		/* get cu due to self tx/rx */
		cu_self += chanim_stats->ccastats[CCASTATS_TXDUR];
		/* cap max percentage to 100 */
		wnm_cfg->load_current_cu_self = MIN(cu_self, 100);
	}
#endif /* WLCHANIM */

	/* prevent current_cu_effective overflow */
	wnm_cfg->load_current_cu_effective = (cu_ap > cu_self) ? (cu_ap - cu_self) : 0;
	ASSERT(wnm_cfg->load_current_cu_self <= 100);
	ASSERT(wnm_cfg->load_current_cu_effective <= 100);

	if (wnm_cfg->load_current_cu_effective < wnm_cfg->cu_trigger_percent) {
		/* reset wbtext counters */
		wlc_wnm_bssload_calc_reset(cfg);

		WL_WBTEXT(("%s: roam_prof[%d].cu:%d%%, eff_cu:%d%%."
			"Resetting RSSI+CU roam.\n", __FUNCTION__,
			roam->roam_prof_idx, wnm_cfg->cu_trigger_percent,
			wnm_cfg->load_current_cu_effective));
		return;
	}

	if (wnm_cfg->load_next_window <= cfg->wlc->pub->now) {
		wnm_cfg->load_next_window = cfg->wlc->pub->now + wnm_cfg->cu_avg_calc_dur;
		wnm_cfg->trigger_rssi_cu_roam = TRUE;

		if (wnm_cfg->load_bcn_per_window) {
			/* adjust sum_cu_ap and sum_cu_self */
			cur_avg_cu_ap = wnm_cfg->load_chan_util / wnm_cfg->load_bcn_per_window;
			cur_avg_cu_self = wnm_cfg->load_cu_self / wnm_cfg->load_bcn_per_window;
			wnm_cfg->load_chan_util = cur_avg_cu_ap * wnm_cfg->load_bcn_count;
			wnm_cfg->load_cu_self = cur_avg_cu_self * wnm_cfg->load_bcn_count;
		}

		/* number of beacons received per window */
		wnm_cfg->load_bcn_per_window = wnm_cfg->load_bcn_count;
		wnm_cfg->load_bcn_count = 0;
	}

	wnm_cfg->load_bcn_count++;
	if (wnm_cfg->load_bcn_per_window) {
		cur_avg_cu_ap = wnm_cfg->load_chan_util / wnm_cfg->load_bcn_per_window;
		cur_avg_cu_self = wnm_cfg->load_cu_self / wnm_cfg->load_bcn_per_window;
		wnm_cfg->load_chan_util -= cur_avg_cu_ap;
		wnm_cfg->load_cu_self -= cur_avg_cu_self;
	}

	wnm_cfg->load_chan_util += channel_utilization;
	wnm_cfg->load_cu_self += cu_self;

	if (wnm_cfg->load_bcn_per_window) {
		wnm_cfg->load_avg_cu_ap =
			(wnm_cfg->load_chan_util / wnm_cfg->load_bcn_per_window) * 100 /
			WL_MAX_CHANNEL_USAGE;
		/* update current_bss CU with latest CU from beacon */
		/* convert channel utilization to channel free score */
		cfg->current_bss->qbss_load_chan_free = (uint8)WLC_QBSS_LOAD_CHAN_FREE_MAX -
			(wnm_cfg->load_chan_util / wnm_cfg->load_bcn_per_window);
		wnm_cfg->load_avg_cu_self =
			wnm_cfg->load_cu_self / wnm_cfg->load_bcn_per_window;
	}

	WBTEXT_TRACE(("bcn_count:(%d:%d), cu_ap:%d%%(%d%%:%d%%:%d:%d), cu_self:%d(%d:%d:%d)\n",
		wnm_cfg->load_bcn_count, wnm_cfg->load_bcn_per_window, wnm_cfg->load_avg_cu_ap,
		cur_avg_cu_ap * 100 / WL_MAX_CHANNEL_USAGE,
		channel_utilization * 100 / WL_MAX_CHANNEL_USAGE,
		channel_utilization, wnm_cfg->load_chan_util,
		wnm_cfg->load_avg_cu_self, cur_avg_cu_self, cu_self, wnm_cfg->load_cu_self));
}

/* process BSS Load IE in beacon */
static int
wlc_wnm_bssload_process_bl_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_bsscfg_t *cfg = data->cfg;
	dot11_qbss_load_ie_t *ie;

	ASSERT(cfg);

	if (!BSSCFG_STA(cfg))
		return BCME_OK;
	if (!cfg->BSS)
		return BCME_OK;
	if (data->ie_len < 7)
		/* A valid BSS Load IE is 7 bytes long */
		return BCME_OK;
	if (data->ie == NULL)
		return BCME_OK;
	ie = (dot11_qbss_load_ie_t*) data->ie;

	wlc_wnm_avg_bssload_calc(cfg, ie->channel_utilization);

	return BCME_OK;
}

/* this get called on every beacon if rssi trigger is hit */
bool
wlc_wnm_bsstrans_roam_required(wlc_info_t *wlc, wlc_bsscfg_t *cfg, int *reason)
{
	wlc_roam_t *roam = cfg->roam;
	wnm_bsscfg_cubby_t *wnm_cfg = WNM_BSSCFG_CUBBY(wlc->wnm_info, cfg);

	ASSERT(roam);

	if (!wlc_wnm_bsstrans_is_product_policy(wlc->wnm_info)) {
		WL_WBTEXT(("%s: Unsupported policy\n", __FUNCTION__));
		*reason = BCME_UNSUPPORTED;
		return FALSE;
	}
	if (!wnm_cfg->cu_trigger_percent) {
		/* cu_trigger is not configured for roam_profile[0] */
		WL_WBTEXT(("%s: CU ROAM trigger is disabled\n",	__FUNCTION__));
		*reason = BCME_UNSUPPORTED;
		return FALSE;
	}
	if (!wnm_cfg->trigger_rssi_cu_roam) {
		WL_WBTEXT(("%s: QBSS Load IE not supported or roam not triggered.\n",
			__FUNCTION__));
		*reason = BCME_NOTREADY;
		return FALSE;
	}

	/* wnm roam is required if
	 * (-65 < RSSI_TRIG) && (95 > CU_TRIG)
	 * DEFAULT_RSSI_TRIGGER: -60
	 * DEFAULT_CU_TRIGGER: 70
	 */
	if ((cfg->link->rssi < wlc->band->roam_trigger) &&
		wnm_cfg->trigger_rssi_cu_roam) {
		WL_WBTEXT(("rssi:(%d:%d), cu:(%d:%d)\n",
			cfg->link->rssi, wlc->band->roam_trigger,
			wnm_cfg->load_current_cu_effective, wnm_cfg->cu_trigger_percent));

		/* Already roaming? */
		if (!roam->scan_block) {
			WBTEXT_INFO(("WBTEXT DBG: RSSI:(%d<%d), CU:(%d>%d)\n",
				cfg->link->rssi, wlc->band->roam_trigger,
				wnm_cfg->load_current_cu_effective,
				wnm_cfg->cu_trigger_percent));
		}
		return TRUE;
	}

	return FALSE;
}

/* return FALSE to stop roam scan */
bool
wlc_wnm_bsstrans_check_for_roamthrash(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	wnm_bsscfg_cubby_t *wnm_cfg = WNM_BSSCFG_CUBBY(wlc->wnm_info, cfg);

	if (!wnm_cfg->cu_trigger_percent || !wnm_cfg->trigger_rssi_cu_roam) {
		WL_WBTEXT(("%s: roam is not triggered due to wbtext (%d:%d)\n",
			__FUNCTION__, wnm_cfg->cu_trigger_percent,
			wnm_cfg->trigger_rssi_cu_roam));
		return FALSE;
	}

	/* Ignore RSSI thrashing about the roam_trigger */
	/* ((-65)-(-75) > 5)=> (10 > 5)=> TRUE=> STOP ROAM SCAN */
	if ((cfg->link->rssi - wlc->band->roam_trigger) >=
		wlc->roam_rssi_cancel_hysteresis) {
		return FALSE;
	}

	/* ((70-60) > 5)=> (10 > 5)=> TRUE=> STOP ROAM SCAN */
	if ((wnm_cfg->cu_trigger_percent > wnm_cfg->load_current_cu_effective) &&
		(wnm_cfg->cu_trigger_percent - wnm_cfg->load_current_cu_effective) >
		ROAM_CU_THRASH_DIFF) {
		return FALSE;
	}

	return TRUE;
}

#endif /* STA */

static void
wlc_wnm_pm_ignore_bcmc_upd(wlc_bsscfg_t *bsscfg, wnm_bsscfg_cubby_t *wnm_cfg)
{
	uint32 ignore_conditions = wnm_cfg->pm_ignore_bcmc;
	bool ignore_bcmc = TRUE;
	wlc_info_t *wlc = bsscfg->wlc;

	if (ignore_conditions) {
		if (ignore_conditions & PM_IGNORE_BCMC_PROXY_ARP) {
			wlc_wnm_info_t *wnm = bsscfg->wlc->wnm_info;
			struct scb *scb_ap = wlc_scbfind(wnm->wlc, bsscfg,
				&bsscfg->current_bss->BSSID);
			wnm_scb_cubby_t *wnm_scb = SCB_WNM_CUBBY(wnm, scb_ap);
			if (!scb_ap || !SCB_PROXYARP(wnm_scb->cap))
				ignore_bcmc = FALSE;
		}
		if (ignore_conditions & PM_IGNORE_BCMC_ALL_DMS_ACCEPTED) {
			wnm_dms_info_t *dms_info = &wnm_cfg->dms_info;
			wnm_dms_desc_t *dms_desc;
			for (dms_desc = dms_info->dms_desc_head; dms_desc;
				dms_desc = dms_desc->next)
				if (dms_desc->status != DMS_STATUS_ACCEPTED)
					ignore_bcmc = FALSE;
		}
	} else
		ignore_bcmc = FALSE;

	wlc_pm_ignore_bcmc(wlc->pm, ignore_bcmc);
}
void
wlc_wnm_check_dms_req(wlc_info_t *wlc, struct ether_addr *ea)
{
	wlc_wnm_info_t *wnm_info = wlc->wnm_info;
	wnm_bsscfg_cubby_t *wnm_cfg = WNM_BSSCFG_CUBBY(wnm_info, wlc->cfg);
	struct scb *scb = wlc_scbfind(wlc, wlc->cfg, ea);
	wnm_scb_cubby_t *wnm_scb = SCB_WNM_CUBBY(wnm_info, scb);

	ASSERT(wnm_cfg != NULL);
	if (BSSCFG_STA(wlc->cfg)) {
		wnm_dms_info_t *dms_info = &wnm_cfg->dms_info;
		wnm_dms_desc_t *dms_desc;
		int req_send = FALSE;
		for (dms_desc = dms_info->dms_desc_head; dms_desc;
			dms_desc = dms_desc->next) {
			if (dms_desc->status == DMS_STATUS_NOT_ASSOC) {
				if (SCB_DMS(wnm_scb->cap)) {
					if ((wnm_info->cmn->dms_dependency & DMS_DEP_PROXY_ARP) &&
						!SCB_PROXYARP(wnm_scb->cap)) {
						dms_desc->status = DMS_STATUS_REQ_MISMATCH;
					}
					else {
						dms_desc->status = DMS_STATUS_IN_PROGRESS;
						req_send = TRUE;
					}
				} else
					dms_desc->status = DMS_STATUS_NOT_SUPPORT;
			} else {
				ASSERT(dms_desc->status == DMS_STATUS_DISABLED);
			}
		}

		if (req_send)
			wlc_wnm_dms_req_frame_send(wlc->cfg, DOT11_DMS_REQ_TYPE_ADD, 0);
	}
}

/* Check if the input rate is present in the rateset. Return TRUE if present, FALSE otherwise */
static bool
wlc_wnm_rateset_contain_rate(wlc_rateset_t *rateset, uint8 rate, bool is_ht, uint32 nss)
{
	uint32 i;
	if (is_ht) {
		/* For HT, input is index */
		return !!(rateset->mcs[nss - 1] & (1<<rate));
	}
	for (i = 0; i < rateset->count; i++) {
		if (rateset->rates[i] == rate) {
			return TRUE;
		}
	}
	return FALSE;
}

static uint32
wlc_wnm_bsstrans_get_rssi_score(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg,
	uint8 band, uint8 bcn_rssi)
{
	wnm_bss_select_table_t *bss_select_table;
	wnm_bsscfg_cubby_t *wnm_cfg;
	uint32 rssi_factor = 0;
	int idx;
	wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);
	bss_select_table = wnm_cfg->bss_select_table[band - 1];

#ifdef BCMDBG
	for (idx = 0; idx < bss_select_table->rssi_table_size; idx++) {
		WL_WBTEXT(("rssi[%d]-> %d:%d:%d\n", idx,
			bss_select_table->rssi_table[idx].low,
			bss_select_table->rssi_table[idx].high,
			bss_select_table->rssi_table[idx].factor));
	}
#endif /* BCMDBG */

	for (idx = 0; idx < bss_select_table->rssi_table_size; idx++) {
		if ((bcn_rssi > bss_select_table->rssi_table[idx].low) &&
			(bcn_rssi <= bss_select_table->rssi_table[idx].high)) {
			rssi_factor = bss_select_table->rssi_table[idx].factor;
			break;
		}
	}

	WL_WBTEXT(("bcn_rssi: %d, rssi_factor: %d, rssi_weightage: %d, score: %d\n",
		bcn_rssi, rssi_factor, bss_select_table->rssi_weightage,
		rssi_factor * bss_select_table->rssi_weightage));

	return rssi_factor * bss_select_table->rssi_weightage;
}

static uint32
wlc_wnm_bsstrans_get_cu_score(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg,
	uint8 band, uint8 qbss_cu)
{
	wnm_bss_select_table_t *bss_select_table;
	wnm_bsscfg_cubby_t *wnm_cfg;
	uint32 cu_factor = 0;
	int idx;

	wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);
	bss_select_table = (wnm_bss_select_table_t *)wnm_cfg->bss_select_table[band - 1];

#ifdef BCMDBG
	for (idx = 0; idx < bss_select_table->cu_table_size; idx++) {
		WL_WBTEXT(("cu[%d]-> %d:%d:%d\n", idx,
			bss_select_table->cu_table[idx].low,
			bss_select_table->cu_table[idx].high,
			bss_select_table->cu_table[idx].factor));
	}
#endif /* BCMDBG */

	for (idx = 0; idx < bss_select_table->cu_table_size; idx++) {
		if ((qbss_cu >= bss_select_table->cu_table[idx].low) &&
			(qbss_cu <= bss_select_table->cu_table[idx].high)) {
			cu_factor = bss_select_table->cu_table[idx].factor;
			break;
		}
	}

	WL_WBTEXT(("cu: %d, cu_factor: %d, cu_weightage: %d, score: %d\n",
		qbss_cu, cu_factor, bss_select_table->cu_weightage,
		cu_factor * bss_select_table->cu_weightage));

	return cu_factor * bss_select_table->cu_weightage;
}

/* This function bumps up scores of non neighbor APs that are above the associated AP's score
 * plus score delta. Input param score contains the score threshold. This is done when prority
 * needs to be given to preference candidates amongst join list
 */
bool
wlc_wnm_update_join_pref_score(wlc_wnm_info_t *wnm, wlc_bsscfg_t *cfg,
	wlc_bss_list_t *join_targets, void *join_pref_list, uint32 current_score)
{
	wlc_info_t *wlc = wnm->wlc;
	wlc_bss_info_t **bip = join_targets->ptrs;
	wnm_bsstrans_sta_info_t *bsi = NULL;
	bool retval = FALSE;
	uint32 target_score = 0;
	wl_roam_prof_t *roam_prof;
	uint8 k;
	wnm_bsscfg_cubby_t *wnm_cfg;
	wnm_cfg = WNM_BSSCFG_CUBBY(wnm, cfg);
	bsi = wnm_cfg->bsstrans_stainfo;

	target_score = current_score + bsi->scoredelta;

	/* BTM request with DISASSOC_IMMINENT & ABRIDGED & PREF_LIST_INCL
	 * is a special case.
	 * current_score will be 0 as DISASSOC_IMMINENT is set
	 * current_score is derived from default score (5000) and corresponding scoredelta
	 * This filters pref list APs into good & bad list
	 * Adding BSSTRANS REQ roam reason check ensures that it is done for for this case
	 */
	if ((cfg->roam->reason == WLC_E_REASON_BSSTRANS_REQ) &&
		(bsi->req_mode & DOT11_BSSTRANS_REQMODE_ABRIDGED) &&
		(bsi->req_mode & DOT11_BSSTRANS_REQMODE_PREF_LIST_INCL) &&
		(bsi->req_mode & DOT11_BSSTRANS_REQMODE_DISASSOC_IMMINENT)) {
		uint32 default_score = 0, scoredelta = 0;

		/* derive good AP score and score delta to get target_score */
		default_score = wlc_wnm_get_default_score(wnm, cfg);

		/* score delta should be w.r.t default score */
		/* wbt_ext profile is always at idx 0 */
		roam_prof = &wlc->band->roam_prof[WBTEXT_SCOREDELTA_PROF];
		scoredelta = default_score * roam_prof->roam_delta / 100;
		target_score = default_score + scoredelta;
	}

	/* Need to bump up when ABRIDGED & PREF_LIST is set in following cases:
	 *
	 * ex., current_score = 6000  non_nbr1_score = 10000 non_nbr2_score = 9000
	 * nbr1_score = 8000 nbr2_score = 3000
	 * ensuring we try nbr1 ahead of non_nbr1 or non_nbr2
	 * Need to bump up for nbr1 while ignoring nbr2 as it is below good score
	 *
	 * candidate_scores = 10000[],  9000[],  8000[nbr1],  3000[nbr2]
	 * intended_scores = 18000[nb1], 10000[], 9000[], 3000[nbr2]
	 */
	if (bsi->nbrlist_valtimer_active &&
		(bsi->req_mode & DOT11_BSSTRANS_REQMODE_ABRIDGED) &&
		(bsi->req_mode & DOT11_BSSTRANS_REQMODE_PREF_LIST_INCL)) {
		for (k = 0; k < join_targets->count; k++) {
			if (wlc_wnm_get_neighbor_rep(wnm, cfg, &bip[k]->BSSID,
				wf_chspec_ctlchan(bip[k]->chanspec))) {
				/* add non-nbr best score to pref score */
				retval |= wlc_update_pref_score(join_pref_list, k,
					target_score, bsi->nonnbr_bestscore);
			}
		}
	}

	/* nonnbr_bestscore is no longer valid going forward .. clear it */
	bsi->nonnbr_bestscore = 0;
	return retval;
}

/** Derive good AP score in case IMNT, ABRIDGED, PREF_LIST are all set */
static uint32
wlc_wnm_get_default_score(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg)
{
	uint32 score;
	uint8 bandtype = CHSPEC_BANDTYPE(bsscfg->current_bss->chanspec);

	score = wlc_wnm_bsstrans_get_rssi_score(wnm, bsscfg, bandtype, WNM_DEFAULT_RSSI) +
		wlc_wnm_bsstrans_get_cu_score(wnm, bsscfg, bandtype, WNM_DEFAULT_CU);
	return (score);
}

static const wnm_pref_score_ent_t *
wnm_get_pref_score_ent(wlc_wnm_info_t *wnm, wnm_bsstrans_policy_type_t type)
{
	size_t i;
	const wnm_pref_score_ent_t *ent = NULL;
	for (i = 0; i < wnm_pref_score_num_entries; ++i) {
		if (wnm_pref_score_entries[i].type == type) {
			ent = &wnm_pref_score_entries[i];
			break;
		}
	}
	return ent;
}

int
wlc_wnm_bss_pref_score_rssi(wlc_bsscfg_t *cfg, wlc_bss_info_t *bi, int8 bcn_rssi, uint32 *score)
{
	wlc_info_t *wlc = cfg->wlc;
	const wnm_pref_score_ent_t *entry;
	int err = BCME_ERROR;

	if (!wlc_wnm_bsstrans_is_product_policy(wlc->wnm_info)) {
		return BCME_ERROR;
	}

	entry = wnm_get_pref_score_ent(wlc->wnm_info, wlc->wnm_info->bsstrans_policy);

	if (entry->get_cb) {
		err = (*entry->get_cb)(cfg, bi, bcn_rssi, score);
	}

	return err;
}

#define MIN_SUPPORTED_MCS_RATES 7
/* Returns the score for a bss based on the phy, bw, Nss, chan-free and rssi
 * from the received bcn/prb-resp. Returns error if score should not/cannot be computed.
 */
static int
wlc_wnm_bss_pref_score_product(wlc_bsscfg_t *cfg, wlc_bss_info_t *bi,
	int8 bcn_rssi, uint32 *score)
{
	wlc_info_t *wlc = cfg->wlc;
	wnm_bsscfg_cubby_t *wnm_cfg = WNM_BSSCFG_CUBBY(wlc->wnm_info, cfg);
	wnm_bsstrans_sta_info_t *bsi = wnm_cfg->bsstrans_stainfo;
	wl_bsstrans_rssi_rate_map_t *map = bsi->rssi_rate_map;
	wnm_bsstrans_rateidx2rate500k_t *idx2rate = bsi->idx2rate;

	uint32 bw = 1; /* Multiple of 20Mhz */
	uint32 nss = 1;
	int32 maxrateidx = 0, i;
	wl_bsstrans_rssi_t *map_rssi;
	uint16 *rate_500k; /* pointer to rate table in 500K units */
	bool is_ofdm, is_ht, is_2g, is_vht;

	if (bsi->use_score == FALSE) {
		return BCME_ERROR;
	}

	is_ht = bi->flags & WLC_BSS_HT;
	is_ofdm = wlc_rateset_isofdm(bi->rateset.count, bi->rateset.rates);
	is_2g = CHSPEC_IS2G(bi->chanspec);
	is_vht = bi->flags2 & WLC_BSS_VHT;

	if (bi->flags2 & WLC_BSS_80MHZ) {
		bw = 4;
	} else if (bi->flags & WLC_BSS_40MHZ) {
		bw = 2;
	}

#ifdef WL11AC
	if (is_vht) {
		uint32 mcs_vht;
		uint16 vht_mcsmap = 0;

		/* Intersect self and peer mcsmap */
		vht_mcsmap = wlc_rateset_filter_mcsmap(bi->rateset.vht_mcsmap,
			wlc->stf->op_txstreams, WLC_VHT_FEATURES_MCS_GET(wlc->pub));

		/* Detemine nss supported in intersected map */
		for (i = RSSI_RATE_MAP_MAX_STREAMS; i > 0; i--) {
			mcs_vht = VHT_MCS_MAP_GET_MCS_PER_SS(i, vht_mcsmap);
			if (mcs_vht != VHT_CAP_MCS_MAP_NONE) {
				nss = i;
				break;
			}
		}
		/* Bad AP: Has VHT IE but does not support even 1 stream mcs */
		if (mcs_vht == VHT_CAP_MCS_MAP_NONE) {
			return BCME_ERROR;
		}
		map_rssi = map->phy_ac[nss - 1];
		rate_500k = idx2rate->phy_ac;
		maxrateidx = MIN_SUPPORTED_MCS_RATES + mcs_vht;
	} else
#endif /* WL11AC */

	if (is_ht) {
		for (i = RSSI_RATE_MAP_MAX_STREAMS; i > 0; i--) {
			if (bi->rateset.mcs[i - 1] && i <= wlc->stf->op_txstreams) {
				nss = i;
				break;
			}
		}
		map_rssi = map->phy_n[nss - 1];
		rate_500k = idx2rate->phy_n;
		maxrateidx = WL_NUM_RATES_MCS_1STREAM - 1;
	} else if (is_ofdm) {
		map_rssi = map->ofdm;
		rate_500k = idx2rate->ofdm;
		maxrateidx = WL_NUM_RATES_OFDM - 1;
	} else { /* CCK */
		map_rssi = map->cck;
		rate_500k = idx2rate->cck;
		maxrateidx = WL_NUM_RATES_CCK - 1;
	}

	/* Map rssi to rate:
	 * The rate should be supported by peer; else select next lower rate.
	 * The rssi required for the rate should be less than bcn_rssi.
	 */
	for (i = maxrateidx; i > 0; i--) {
		if ((is_2g && map_rssi[i].rssi_2g <= bcn_rssi) ||
			(!is_2g && map_rssi[i].rssi_5g <= bcn_rssi)) {
			/* VHT supports all rates till maxrateidx */
			if (is_vht) {
				break;
			}
			/* For HT pass mcs index as rateset has indices */
			if (is_ht &&
				wlc_wnm_rateset_contain_rate(&bi->rateset, (uint8)i, is_ht, nss)) {
				break;
			}
			if (!is_ht && wlc_wnm_rateset_contain_rate(&bi->rateset,
				(uint8)rate_500k[i], FALSE, 1)) {
				break;
			}
		}
	}

	*score = rate_500k[i] * nss * bw * bi->qbss_load_chan_free;

	WL_WNM_BSSTRANS_LOG("bss[4:5] %x:%x rate_idx %d rssi %d ",
		bi->BSSID.octet[4], bi->BSSID.octet[5], i, bcn_rssi);
	WL_WNM_BSSTRANS_LOG("bw %d nss %d chan_free %d score %d\n",
		bw, nss, bi->qbss_load_chan_free, *score);

	return BCME_OK;
}

static void
wlc_wnm_bsstrans_get_cu(wlc_bsscfg_t *cfg, wlc_bss_info_t *bi, wnm_bss_cu_t *bss_cu)
{
	wnm_bsscfg_cubby_t *wnm_cfg = WNM_BSSCFG_CUBBY(cfg->wlc->wnm_info, cfg);
	uint8 chan_free;

	if (bi->qbss_load_chan_free == 0) {
		/* AP don't habe QBSS IE support, give default channel load */
		chan_free = WLC_QBSS_CHAN_FREE_DEFAULT;
	} else {
		chan_free = bi->qbss_load_chan_free;
	}
	/* convert channel load to percentage */
	bss_cu->cu_ap = ((uint8)WLC_QBSS_LOAD_CHAN_FREE_MAX - chan_free) * 100 /
		(uint8)WLC_QBSS_LOAD_CHAN_FREE_MAX;
	bss_cu->cu_self = 0;

	/* reduce self traffic if in same control channel */
	if (wf_chspec_ctlchan(cfg->current_bss->chanspec) ==
		wf_chspec_ctlchan(bi->chanspec)) {
		bss_cu->cu_self = wnm_cfg->load_current_cu_self;
	}

	/* prevent cu_effective overflow */
	bss_cu->cu = (bss_cu->cu_ap > bss_cu->cu_self) ?
		(bss_cu->cu_ap - bss_cu->cu_self) : 0;

	return;
}

void
wlc_wnm_bsstrans_print_score(wlc_wnm_info_t *wnm, wlc_bsscfg_t *cfg,
	wlc_bss_info_t *bi, int16 rssi, uint32 score)
{
	wnm_bss_cu_t bss_cu;

	/* XXX: Enabling debug print for "wbtext" svt verification.
	 * This will be disabled in production image later.
	 */
	wlc_wnm_bsstrans_get_cu(cfg, bi, &bss_cu);
	wlc_wnm_bsstrans_validnbr_ap(cfg->wlc->wnm_info, cfg, bi);

	WBTEXT_INFO(("WBTEXT DBG: %s: bssid:%02x:%02x:%02x:%02x, "
		"chan:%04x, rssi:%d(%d:%d), cu:%d(%d:%d)%%, score:%d, rec:%d\n",
		(bi == cfg->current_bss) ? "associated" : "candidate",
		bi->BSSID.octet[2], bi->BSSID.octet[3],
		bi->BSSID.octet[4], bi->BSSID.octet[5],
		bi->chanspec, rssi, bi->RSSI, bi->RSSI - rssi,
		bss_cu.cu, bss_cu.cu_ap, bss_cu.cu_self, score,
		wlc_wnm_bsstrans_validnbr_ap(cfg->wlc->wnm_info, cfg, bi)));
}

static int
wlc_wnm_bss_pref_score_product_wbtext(wlc_bsscfg_t *cfg, wlc_bss_info_t *bi,
	int8 bcn_rssi, uint32 *score)
{
	wlc_info_t *wlc = cfg->wlc;
	wnm_bss_cu_t bss_cu;
	uint8 band;
	uint8 rssi;

	band = CHSPEC_BANDTYPE(bi->chanspec);

	/* convert negative value to positive */
	rssi = -bcn_rssi;

	wlc_wnm_bsstrans_get_cu(cfg, bi, &bss_cu);

	/* score = (RSSIfactor * RSSIweight) + (CUfactor * CUweight) */
	*score = wlc_wnm_bsstrans_get_rssi_score(wlc->wnm_info, cfg, band, rssi) +
		wlc_wnm_bsstrans_get_cu_score(wlc->wnm_info, cfg, band, bss_cu.cu);

	WBTEXT_TRACE(("WBTEXT TRACE: bssid:%02x:%02x:%02x:%02x:%02x:%02x, "
		"rssi:%d, cu:%d(%d:%d), score:%d\n",
		bi->BSSID.octet[0], bi->BSSID.octet[1],
		bi->BSSID.octet[2], bi->BSSID.octet[3],
		bi->BSSID.octet[4], bi->BSSID.octet[5],
		rssi, bss_cu.cu, bss_cu.cu_ap, bss_cu.cu_self, *score));

	return BCME_OK;
}

/* Check if all targets support bss transition cap and bss_load element;
 * and update channel-free value in bss_info.
 */
void
wlc_wnm_process_join_trgts_bsstrans(wlc_wnm_info_t *wnm, wlc_bsscfg_t *cfg,
	wlc_bss_info_t **bip, int trgt_count)
{
	struct dot11_bcn_prb *bcn;
	uint8 *tlvs;
	uint16 tlvs_len;
	dot11_qbss_load_ie_t *qbss_load_ie;
	int i;
#ifdef WL11AC
	dot11_extcap_ie_t *extcap;
#endif /* WL11AC */
	bool use_score = TRUE;
	uint16 sta_cnt;
	wnm_bsscfg_cubby_t *wnm_cfg;

	for (i = 0; i < trgt_count; i++) {
		bcn = bip[i]->bcn_prb;
		tlvs = ((uint8*)bcn + DOT11_BCN_PRB_LEN);
		tlvs_len = bip[i]->bcn_prb_len - DOT11_BCN_PRB_LEN;

#ifdef WL11AC
		if (bip[i]->flags2 & WLC_BSS_VHT) {
			extcap = (dot11_extcap_ie_t *)
				bcm_parse_tlvs(tlvs, tlvs_len, DOT11_MNG_EXT_CAP_ID);
			if (extcap == NULL || extcap->len < DOT11_EXTCAP_LEN_BSSTRANS ||
				!isset(extcap->cap, DOT11_EXT_CAP_BSSTRANS_MGMT)) {
				WL_WNM_BSSTRANS_LOG("bss[4:5] %x:%x doesn't support bsstrans\n",
					bip[i]->BSSID.octet[4], bip[i]->BSSID.octet[5], 0, 0);
				use_score = FALSE;
				continue;
			}
		}
#endif /* WL11AC */

		qbss_load_ie = (dot11_qbss_load_ie_t *)
			bcm_parse_tlvs(tlvs, tlvs_len, DOT11_MNG_QBSS_LOAD_ID);
		if (qbss_load_ie == NULL ||
			qbss_load_ie->length != (sizeof(*qbss_load_ie) - TLV_HDR_LEN)) {
			WL_WNM_BSSTRANS_LOG("bss[4:5] %x:%x doesn't support load ie\n",
				bip[i]->BSSID.octet[4], bip[i]->BSSID.octet[5], 0, 0);
			use_score = FALSE;
			continue;
		}

		/* convert channel utilization to channel free score */
		bip[i]->qbss_load_chan_free =
			(uint8)WLC_QBSS_LOAD_CHAN_FREE_MAX - qbss_load_ie->channel_utilization;
		bip[i]->qbss_load_aac = ltoh16_ua((uint8 *)&qbss_load_ie->aac);
		sta_cnt = ltoh16_ua((uint8 *)&qbss_load_ie->station_count);
		if (sta_cnt == 0xffff) {
			bip[i]->flags2 |= WLC_BSS_MAX_STA_CNT;
		}
	}
	wnm_cfg = WNM_BSSCFG_CUBBY(wnm, cfg);
	wnm_cfg->bsstrans_stainfo->use_score = use_score;
	return;
}

/* Returns bool to indicate if associated bss score should be set to zero. */
bool
wlc_wnm_bsstrans_zero_assoc_bss_score(wlc_wnm_info_t *wnm, wlc_bsscfg_t *cfg)
{
	bool zero_score = TRUE;
	wnm_bsscfg_cubby_t *wnm_cfg;

	/* Keep the plugfest behavior intact for non-product */
	if (!wlc_wnm_bsstrans_is_product_policy(wnm)) {
		return TRUE;
	}

	wnm_cfg = WNM_BSSCFG_CUBBY(wnm, cfg);
	/* If any of the following bits are set return TRUE */
	zero_score = !!(wnm_cfg->bsstrans_stainfo->req_mode &
		(DOT11_BSSTRANS_REQMODE_DISASSOC_IMMINENT |
		DOT11_BSSTRANS_REQMODE_ESS_DISASSOC_IMNT |
		DOT11_BSSTRANS_REQMODE_BSS_TERM_INCL));
	if (zero_score) {
		WL_WNM_BSSTRANS_LOG("Associated bss score zeroed\n", 0, 0, 0, 0);
	}
	return zero_score;
}

void
wlc_wnm_bsstrans_reset_pending_join(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	wnm_bsscfg_cubby_t *wnm_cfg;
	wnm_cfg = WNM_BSSCFG_CUBBY(wlc->wnm_info, cfg);

	if (wnm_cfg->bsstrans_stainfo) {
		wnm_cfg->bsstrans_stainfo->join_cfg = NULL;
		wnm_cfg->bsstrans_stainfo->join_pending = FALSE;
	}
}

uint32
wlc_wnm_bsstrans_get_scoredelta(wlc_wnm_info_t *wnm, wlc_bsscfg_t *cfg)
{
	wnm_bsscfg_cubby_t *wnm_cfg;
	wnm_cfg = WNM_BSSCFG_CUBBY(wnm, cfg);
	return wnm_cfg->bsstrans_stainfo->scoredelta;
}

/*
 * updates score delta as per score delta percentage (when WBTEXT is active).
 *
 * score delta percentage is available only in profile[0], -
 * which is applicable to all profiles.
 */
void
wlc_wnm_bsstrans_update_scoredelta(wlc_wnm_info_t *wnm, wlc_bsscfg_t *cfg, uint32 score)
{
	wlcband_t *band = wnm->wlc->band;
	wl_roam_prof_t *roam_prof;
	wnm_bsscfg_cubby_t *wnm_cfg;
	wnm_bsstrans_sta_info_t *bsi;

	wnm_cfg = WNM_BSSCFG_CUBBY(wnm, cfg);
	bsi = wnm_cfg->bsstrans_stainfo;

	roam_prof = &band->roam_prof[WBTEXT_SCOREDELTA_PROF];
	ASSERT(roam_prof->channel_usage != 0);

	/* update score delta for pruning candidate AP list */
	bsi->scoredelta = score * roam_prof->roam_delta / 100;

	WL_WBTEXT(("%s: score: %d, percentage: %d%%, scoredelta: %d\n", __FUNCTION__,
		score, roam_prof->roam_delta, bsi->scoredelta));
}

int8
wlc_wnm_btm_get_rssi_thresh(wlc_wnm_info_t *wnm, wlc_bsscfg_t *cfg)
{
	wnm_bsscfg_cubby_t *wnm_cfg;
	wnm_cfg = WNM_BSSCFG_CUBBY(wnm, cfg);
	return wnm_cfg->bsstrans_stainfo->btm_rssi_thresh;
}

bool
wlc_wnm_bsstrans_is_product_policy(wlc_wnm_info_t *wnm)
{
	wnm_bsscfg_cubby_t *wnm_cfg = WNM_BSSCFG_CUBBY(wnm, wnm->wlc->cfg);
	return (WNM_BSSTRANS_ENABLED(wnm_cfg->cap) &&
		WNM_IS_PRODUCT_POLICY(wnm));
}

#define MAX_PREFERENCE_BITS 8
/* Add preference subelement to neighbor element.
 * Compute scaled (1-255) preference value from bss-score.
 * Input: wlc, bss_list, buf to write sub-element, and max_score.
 * Output: Length of sub-elements added.
 */
static uint32
wlc_wnm_btm_nbr_add_pref_se(wlc_info_t *wlc, wlc_bss_info_t *bi, uint8 **buf, uint32 max_score,
	uint32 score)
{
	dot11_ngbr_bsstrans_pref_se_t *pref;
	uint32 se_len = 0;

	if (!wlc_wnm_bsstrans_is_product_policy(wlc->wnm_info)) {
		return se_len;
	}
	pref = (dot11_ngbr_bsstrans_pref_se_t *) *buf;
	pref->sub_id = DOT11_NGBR_BSSTRANS_PREF_SE_ID;
	pref->len = 1;
	max_score >>= MAX_PREFERENCE_BITS;
	/* Avoid divide by zero */
	if (max_score == 0) {
		max_score = 1;
	}
	score /= max_score;
	/* Zero score not allowed in BTM Query */
	if (score == 0) {
		score = 1;
	}
	/* Clamp the score to 255 */
	if (score > 255) {
		score = 255;
	}
	pref->preference = (uint8) score;
	se_len = TLV_HDR_LEN + DOT11_NGBR_BSSTRANS_PREF_SE_LEN;
	*buf += TLV_HDR_LEN + DOT11_NGBR_BSSTRANS_PREF_SE_LEN;
	WL_WNM_BSSTRANS_LOG("bss[4:5] %x:%x pref %d\n", bi->BSSID.octet[4],
		bi->BSSID.octet[5], pref->preference, 0);
	return se_len;
}

static void
wlc_wnm_neighbor_report_ie(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	wlc_bss_info_t *bi, uint8 **bufptr, uint32 max_score)
{
	uint32 bssid_info, n_bssid_info;
	dot11_neighbor_rep_ie_t *nbr_rep;
	uint32 score, temp_rssi;

	nbr_rep = (dot11_neighbor_rep_ie_t *)*bufptr;
	nbr_rep->id = DOT11_MNG_NEIGHBOR_REP_ID;
	nbr_rep->len = DOT11_NEIGHBOR_REP_IE_FIXED_LEN;
	bcopy(&bi->BSSID, &nbr_rep->bssid, ETHER_ADDR_LEN);
	bssid_info = 0;
	if (bi->capability & DOT11_CAP_SPECTRUM)
		bssid_info |= DOT11_NGBR_BI_CAP_SPEC_MGMT;
	n_bssid_info = hton32(bssid_info);
	bcopy(&n_bssid_info, &nbr_rep->bssid_info, sizeof(nbr_rep->bssid_info));
	nbr_rep->reg = wlc_get_regclass(wlc->cmi, bi->chanspec);
	nbr_rep->channel = CHSPEC_CHANNEL(bi->chanspec);
	nbr_rep->phytype = 0;
	*bufptr += TLV_HDR_LEN + DOT11_NEIGHBOR_REP_IE_FIXED_LEN;
	score = wlc_bss_pref_score(cfg, bi, TRUE, &temp_rssi);
	nbr_rep->len += (uint8) wlc_wnm_btm_nbr_add_pref_se(wlc,
		bi, bufptr, max_score, score);
}

static int
wlc_wnm_set_sleep_mode(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	wlc_wnm_info_t *wnm, wnm_bsscfg_cubby_t *wnm_cfg, int mode)
{
	struct scb *scb;
	wnm_scb_cubby_t *wnm_scb;
	if (!WNM_SLEEP_ENABLED(wnm_cfg->cap))
		return BCME_UNSUPPORTED;

	if (!bsscfg->associated)
		return BCME_NOTASSOCIATED;

	scb = wlc_scbfind(wlc, bsscfg, &bsscfg->BSSID);
	wnm_scb = SCB_WNM_CUBBY(wnm, scb);
	if (!SCB_WNM_SLEEP(wnm_scb->cap)) {
		WL_WNM(("Sleep not supported by AP\n"));
		return BCME_UNSUPPORTED;
	}

	if (wnm_cfg->sleep_state == WNM_SLEEP_NONE) {
		if (mode) {
			if (wlc_wnm_sleep_add_tfs_filter(bsscfg) != BCME_OK) {
				WL_ERROR(("Error setting TFS for WNM-Sleep mode\n"));
				return BCME_EPERM;
			}
			wnm_cfg->sleep_state = WNM_SLEEP_ENTER_WAIT;
		} else
			return BCME_OK;

	} else if (wnm_cfg->sleep_state == WNM_SLEEP_SLEEPING) {
		if (!mode)
			wnm_cfg->sleep_state = WNM_SLEEP_EXIT_WAIT;
		else
			return BCME_OK;
	} else
		return BCME_NOTREADY;

	wnm_cfg->sleep_req_cnt = 0;
	wlc_wnm_sleep_req_frame_send(wnm, bsscfg);
	wnm_cfg->sleep_timer = wlc->pub->now + SLEEP_DELAY_TIMEOUT;

	return BCME_OK;
}

static int
wlc_wnm_get_sleep_mode(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	wlc_wnm_info_t *wnm, wnm_bsscfg_cubby_t *wnm_cfg, int *mode)
{
	if (!WNM_SLEEP_ENABLED(wnm_cfg->cap))
		return BCME_UNSUPPORTED;

	if (!bsscfg->associated)
		return BCME_NOTASSOCIATED;

	*mode = (int)(wnm_cfg->sleep_state == WNM_SLEEP_SLEEPING);

	return BCME_OK;
}

static int
wlc_wnm_set_sleep_intv(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	wlc_wnm_info_t *wnm, wnm_bsscfg_cubby_t *wnm_cfg, int int_val)
{
	if (!WNM_SLEEP_ENABLED(wnm_cfg->cap))
		return BCME_UNSUPPORTED;

	/* Range only support for unsigned 16-bit width */
	if (int_val > 65535)
		return BCME_RANGE;

	wnm_cfg->sleep_intv = (uint16)int_val;
	return BCME_OK;
}

static int
wlc_wnm_get_sleep_intv(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	wlc_wnm_info_t *wnm, wnm_bsscfg_cubby_t *wnm_cfg, int *ret_int_ptr)
{
	if (!WNM_SLEEP_ENABLED(wnm_cfg->cap))
		return BCME_UNSUPPORTED;

	*ret_int_ptr = (int)(wnm_cfg->sleep_intv);

	return BCME_OK;
}

#endif /* STA */

static int
wlc_wnm_bss_parse_ext_cap_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;
	struct scb *scb = wlc_iem_parse_get_assoc_bcn_scb(data);

	if (scb != NULL && data->ie != NULL) {
		wlc_wnm_process_extcap_ie(wlc, (bcm_tlv_t *)data->ie, scb);
	}

	return BCME_OK;
}

static void
wlc_wnm_process_extcap_ie(wlc_info_t *wlc,  bcm_tlv_t *ie, struct scb *scb)
{
	dot11_extcap_ie_t *extcap_ie_tlv = (dot11_extcap_ie_t *)ie;
	dot11_extcap_t *cap;

	ASSERT(ie != NULL);
	ASSERT(scb != NULL);

	cap = (dot11_extcap_t*)extcap_ie_tlv->cap;

	if (WLWNM_ENAB(wlc->pub) &&
#ifdef WLWNM_BRCM
		!(WLWNM_BRCM_ENAB(wlc->pub) && BSSCFG_AP(SCB_BSSCFG(scb))) &&
#endif // endif
		TRUE) {
		uint32 wnmcap = 0;
		/* ext_cap 11-th bit for FMS service */
		if ((extcap_ie_tlv->len >= DOT11_EXTCAP_LEN_FMS) &&
		    isset(cap->extcap, DOT11_EXT_CAP_FMS))
			wnmcap |= WL_WNM_FMS;

		/* ext_cap 12-th bit for PROXYARP service */
		if ((extcap_ie_tlv->len >= DOT11_EXTCAP_LEN_PROXY_ARP) &&
		    isset(cap->extcap, DOT11_EXT_CAP_PROXY_ARP))
			wnmcap |= WL_WNM_PROXYARP;

		/* ext_cap 16-th bit for TFS service */
		if ((extcap_ie_tlv->len >= DOT11_EXTCAP_LEN_TFS) &&
		    isset(cap->extcap, DOT11_EXT_CAP_TFS))
			wnmcap |= WL_WNM_TFS;

		/* ext_cap 17-th bit for WNM-Sleep service */
		if ((extcap_ie_tlv->len >= DOT11_EXTCAP_LEN_WNM_SLEEP) &&
		    isset(cap->extcap, DOT11_EXT_CAP_WNM_SLEEP))
			wnmcap |= WL_WNM_SLEEP;

		/* ext_cap 18-th bit for TIM Broadcast service */
		if ((extcap_ie_tlv->len >= DOT11_EXTCAP_LEN_TIMBC) &&
		    isset(cap->extcap, DOT11_EXT_CAP_TIMBC))
			wnmcap |= WL_WNM_TIMBC;

		/* ext_cap 19-th bit for BSS-Transition service */
		if ((extcap_ie_tlv->len >= DOT11_EXTCAP_LEN_BSSTRANS) &&
		    isset(cap->extcap, DOT11_EXT_CAP_BSSTRANS_MGMT))
			wnmcap |= WL_WNM_BSSTRANS;

		/* ext_cap 26-th bit for DMS service */
		if ((extcap_ie_tlv->len >= DOT11_EXTCAP_LEN_DMS) &&
		    isset(cap->extcap, DOT11_EXT_CAP_DMS))
			wnmcap |= WL_WNM_DMS;

		/* ext_cap 46-th bit for DMS service */
		if ((extcap_ie_tlv->len >= DOT11_EXTCAP_LEN_WNM_NOTIFICATION) &&
		    isset(cap->extcap, DOT11_EXT_CAP_WNM_NOTIF))
			wnmcap |= WL_WNM_NOTIF;

		/* saved to WNM scb cubby */
		wlc_wnm_set_scbcap(wlc, scb, wnmcap);
	}
}

/* Returns wnm-chainable flag */
bool BCMFASTPATH
wlc_wnm_pkt_chainable(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg)
{
	wnm_bsscfg_cubby_t* wnm_cfg = NULL;

	ASSERT(bsscfg != NULL);

	if (wnm == NULL)
		return TRUE;

	wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);

	return wnm_cfg->chainable;
}

#ifdef WLWNM_BRCM
static int
wlc_wnm_bss_parse_brcm_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;
	struct scb *scb = wlc_iem_parse_get_assoc_bcn_scb(data);

	if (scb != NULL && data->ie != NULL) {
		wlc_wnm_process_brcm_ie(wlc, scb, (brcm_ie_t *)data->ie);
	}

	return BCME_OK;
}

static void
wlc_wnm_process_brcm_ie(wlc_info_t *wlc, struct scb *scb, brcm_ie_t *brcm_ie)
{
	ASSERT(scb != NULL);

	if (WLWNM_BRCM_ENAB(wlc->pub)) {
		if (brcm_ie->len <= (OFFSETOF(brcm_ie_t, flags1) - TLV_HDR_LEN)) {
			return;
		}

		if (brcm_ie->flags1 & BRF1_WNM) {
			wlc_wnm_set_scbcap(wlc, scb, BRCM_WNM_FEATURE_SET);
		}
	}
}
#endif /* WLWNM_BRCM */

/* scb = NULL in case of destition is AP */
int
wlc_wnm_notif_req_send(wlc_wnm_info_t *wnm, wlc_bsscfg_t *bsscfg, struct scb *scb,
	uint8 type, uint8 *sub_elem_buf, uint16 se_buf_len)
{
	wlc_info_t *wlc = wnm->wlc;
	void *p;
	dot11_wnm_notif_req_t *notif_req;
	uint8 *bufptr;
	uint16 plen = DOT11_WNM_NOTIF_REQ_LEN;  /* Fixed Len */
	struct ether_addr *dea = NULL;
	int ret = BCME_OK;

	if (!scb && !bsscfg->associated) {
		ret = BCME_NOTASSOCIATED;
		goto fail;
	}

	/* adjust frame len */
	plen += se_buf_len;

	/* set destination */
	if (scb) {
		dea = &scb->ea;
	} else {
		/* TO AP */
		dea = &bsscfg->BSSID;
		scb = wlc_scbfind(wlc, bsscfg, dea);
	}

	p = wlc_frame_get_action(wlc, dea, &bsscfg->cur_etheraddr,
		&bsscfg->BSSID, plen, (uint8 **) &notif_req, DOT11_ACTION_CAT_WNM);
	if (p == NULL) {
		WL_ERROR(("wl%d: %s: MALLOC(%d) failed, malloced %d bytes\n", WLCWLUNIT(wlc),
			__FUNCTION__, DOT11_ACTION_CAT_WNM, MALLOCED(wlc->osh)));
		ret = BCME_NOMEM;
		goto fail;
	}

	notif_req->category = DOT11_ACTION_CAT_WNM;
	notif_req->action = DOT11_WNM_ACTION_NOTFCTN_REQ;
	notif_req->token = WLC_WNM_UPDATE_TOKEN(wnm->req_token);
	notif_req->type = type;
	/* copy sub elements buf */
	if (sub_elem_buf && se_buf_len != 0) {
		bufptr = &notif_req->data[0];
		memcpy(bufptr, sub_elem_buf, se_buf_len);
	}

	if (!wlc_sendmgmt(wlc, p, bsscfg->wlcif->qi, scb)) {
		WL_ERROR(("wl%d: %s:wlc_sendmgmt() failed  \n", WLCWLUNIT(wlc),
			__FUNCTION__));
		ret = BCME_ERROR;
	}
fail:
	return ret;
}

#ifdef STA
#if defined(WNM_BSSTRANS_EXT)
#if defined(BCMDBG) || defined(BCMDBG_DUMP) || defined(WLTEST) || defined(DNG_DBGDUMP)
/** This function lists dump option */
static int
wlc_wbtext_dump(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	wlc_roam_t *roam = NULL;
	wnm_bsscfg_cubby_t *wnm_cfg = NULL;
	wlc_bsscfg_t *cfg;
	int idx;
	enum wlc_bandunit bandunit;

	FOREACH_AS_STA(wlc, idx, cfg) {
		roam = cfg->roam;
		wnm_cfg = WNM_BSSCFG_CUBBY(wlc->wnm_info, cfg);
		bandunit = CHSPEC_BANDUNIT(cfg->current_bss->chanspec);

		bcm_bprintf(b, "wl%d.%d: Policy:%d, Roam Profile:%d\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(cfg),
				wlc_wnm_bsstrans_is_product_policy(wlc->wnm_info) ?
				wlc->wnm_info->bsstrans_policy : 0,
				roam->roam_prof_idx);

		bcm_bprintf(b, "BSSID:(%02X:%02X:%02X:%02X:%02X:%02X), Chan:%d\n",
				cfg->BSSID.octet[0], cfg->BSSID.octet[1], cfg->BSSID.octet[2],
				cfg->BSSID.octet[3], cfg->BSSID.octet[4], cfg->BSSID.octet[5],
				CHSPEC_CHANNEL(cfg->current_bss->chanspec));
		bcm_bprintf(b, "RSSI(cur:%d, trig:%d)\n",
				cfg->link->rssi, wlc->bandstate[bandunit]->roam_trigger);
		bcm_bprintf(b, "CU(cur:%d, trig:%d), Count(mon:%d, dur:%d), Avg(ap:%d, self:%d)\n",
				wnm_cfg->load_current_cu_effective, wnm_cfg->cu_trigger_percent,
				wnm_cfg->trigger_rssi_cu_roam ? wnm_cfg->cu_avg_calc_dur :
				(wnm_cfg->cu_trigger_percent == 0) ? 0 : (wlc->pub->now +
				wnm_cfg->cu_avg_calc_dur - wnm_cfg->load_next_window),
				wnm_cfg->cu_avg_calc_dur, wnm_cfg->load_avg_cu_ap,
				wnm_cfg->load_avg_cu_self);
		bcm_bprintf(b, "Roam triggered: %s (RSSI:%d, CU:%d)\n\n",
				((cfg->link->rssi < wlc->bandstate[bandunit]->roam_trigger) &&
				wnm_cfg->trigger_rssi_cu_roam) ? "TRUE" : "FALSE",
				cfg->link->rssi < wlc->bandstate[bandunit]->roam_trigger,
				wnm_cfg->trigger_rssi_cu_roam);
	}

	return 0;
}
#endif /* BCMDBG || BCMDBG_DUMP || WLTEST || DNG_DBGDUMP */
#endif /* WNM_BSSTRANS_EXT */
#endif /* STA */

#if defined(WL_MBO) && !defined(WL_MBO_DISABLED) && defined(MBO_AP)
static int
wlc_wnm_send_notif_resp_frame(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb,
	uint8 token, uint8 status)
{
	void *p;
	uint8 *pbody;
	int body_len = DOT11_WNM_NOTIF_RESP_LEN;
	dot11_wnm_notif_resp_t *resp;

	/* TODO: Take care of MFP */
	if ((p = wlc_frame_get_action(wlc, &scb->ea, &bsscfg->cur_etheraddr,
		&bsscfg->BSSID, body_len, &pbody, DOT11_ACTION_CAT_WNM)) == NULL) {
		return BCME_ERROR;
	}

	/* Prepare DMS response frame fields */
	resp = (dot11_wnm_notif_resp_t *)pbody;
	resp->category = DOT11_ACTION_CAT_WNM;
	resp->action = DOT11_WNM_ACTION_NOTFCTN_RESP;
	resp->token = token;
	resp->status = status;
#ifdef BCMDBG
	prhex("Raw WNM-Notification Resp Body", (uchar*)pbody, body_len);
#endif	/* BCMDBG */
	wlc_sendmgmt(wlc, p, bsscfg->wlcif->qi, scb);

	return BCME_OK;
}
#endif /* WL_MBO && WL_MBO_DISABLED && MBO_AP */
#if (defined(WL_MBO) && defined(MBO_AP)) || defined(WL11K_AP)
/* Create Neighbor report element with Bss's own information along with BSS Transition
 * candidate preference subelement. This information is required in response to
 * BTM query from client
 */
void
wlc_create_nbr_element_own_bss(wlc_info_t* wlc, wlc_bsscfg_t *bsscfg, uint8 **ptr)
{
	dot11_neighbor_rep_ie_t *nbrrep;
	dot11_ngbr_bsstrans_pref_se_t *pref;
	int bssid_info = 0x00;

	/* Fill AP's Own BSS information in Neighbor report element */
	nbrrep = (dot11_neighbor_rep_ie_t *)*ptr;
	nbrrep->id = DOT11_MNG_NEIGHBOR_REP_ID;
	nbrrep->len = DOT11_NEIGHBOR_REP_IE_FIXED_LEN +
			DOT11_NGBR_BSSTRANS_PREF_SE_LEN + TLV_HDR_LEN;

	memcpy(&nbrrep->bssid, &(bsscfg->BSSID.octet), ETHER_ADDR_LEN);

	store32_ua((uint8 *)&nbrrep->bssid_info, bssid_info);

	nbrrep->reg = wlc_get_regclass(wlc->cmi, bsscfg->current_bss->chanspec);
	nbrrep->channel = CHSPEC_CHANNEL(bsscfg->current_bss->chanspec);
#ifdef WL11AC
	if (bsscfg->current_bss->flags2 & WLC_BSS_VHT) {
		nbrrep->phytype = 9;
	} else
#else
	if (bsscfg->current_bss->flags & WLC_BSS_HT) {
		nbrrep->phytype = 7;
	} else
#endif /* WL11AC */
	{
		nbrrep->phytype = 0;
	}

	/* Add preference subelement */
	pref = (dot11_ngbr_bsstrans_pref_se_t*) nbrrep->data;
	pref->sub_id = DOT11_NGBR_BSSTRANS_PREF_SE_ID;
	pref->len = DOT11_NGBR_BSSTRANS_PREF_SE_LEN;
	/* preference of its own BSS to zero */
	pref->preference = 0;

	*ptr += TLV_HDR_LEN + DOT11_NEIGHBOR_REP_IE_FIXED_LEN + TLV_HDR_LEN
			+ DOT11_NGBR_BSSTRANS_PREF_SE_LEN;
}

#endif /* (WL_MBO && MBO_AP) || 11K_AP */
