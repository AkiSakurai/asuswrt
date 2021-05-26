/*
 * OCE implementation for
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
 * $Id$
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 */

/**
 * @file
 * @brief
 * This file implements a part of WFA OCE features
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <osl.h>
#include <wl_dbg.h>
#include <bcmendian.h>
#include <wlioctl.h>
#include <wlc_bsscfg.h>
#include <wl_export.h>

#include <wlc_pub.h>
#include <wlc.h>
#include <wlc_types.h>
#include <wlc_ie_mgmt_types.h>
#include <wlc_ie_mgmt.h>
#include <wlc_ie_mgmt_vs.h>
#include <wlc_ie_mgmt_ft.h>
#include <wlc_ie_mgmt_lib.h>
#include <wlc_ie_helper.h>
#include <wlc_ap.h>
#include <wlc_vht.h>
#include <wlc_rspec.h>
#include <wlc_assoc.h>
#include <oce.h>
#include <fils.h>
#include <wlc_oce.h>
#include "wlc_mbo_oce_priv.h"
#include <bcmwpa.h>
#include <wlc_scan_utils.h>
#include <wlc_lq.h>
#include <phy_noise_api.h>
#include <wlc_rx.h>
#include <wlc_tx.h>
#ifdef WLMCNX
#include <wlc_mcnx.h>
#endif /* WLMCNX */
#include <wlc_esp.h>
#include <wlc_ie_reg.h>
#include <wlc_wnm.h>
#include <wlc_rrm.h>
#include <bcmiov.h>
#include <wlc_dump.h>
#include <wlc_scan_priv.h>
#include <wlc_chanctxt.h>
#include <wlc_stf.h>
#include <wlc_scan.h>
#if BAND6G
#include <bcmwifi_rclass.h>
#include <wlc_channel.h>
#endif /* BAND6G */

#define OCE_PROBE_DEFFERAL_TIME		15U
#define OCE_DEF_RSSI_DELTA		7U
#define OCE_DEF_RWAN_UPLINK_DELTA	13U /* change of 4 times */
#define OCE_DEF_RWAN_DOWNLINK_DELTA	13U /* change of 4 times */
#define OCE_DEF_RWAN_WINDOW_DUR		10U /* window duration to test rwan metrics */

#define	OCE_PRB_REQ_RATE_DISABLE	0x2 /* disable oce rates for probe request */

static const uint8 oce_pref_channels[] = {1, 6, 11};

typedef struct oce_delayed_bssid_list oce_delayed_bssid_list_t;
struct oce_delayed_bssid_list {
	oce_delayed_bssid_list_t *next;
	oce_delayed_bssid_list_t *prev;
	struct ether_addr bssid;		/* reject bssid */
	uint32 release_tm;			/* timestamp to release from the list (ms) */
	int8 release_rssi;			/* min rssi value to release from the list */
};

#define MAX_PROBE_SUPPRESS_BSS	10U
#define TBTT_WAIT_EXTRA_TIME	5000U
#define TBTT_WAIT_MAX_TIME	102000UL

#define MAX_TSF_L	0xffffffffUL

#define OCE_BSSCFG_ENABLE		0x01
#define OCE_BSSCFG_IS_ENABLED(mbc)	(obc->flags & OCE_BSSCFG_ENABLE)

typedef struct prb_suppress_bss_item {
	dll_t	node;
	struct ether_addr bssid;		/* suppression bssid */
	uint32	short_ssid;			/* suppression short-ssid */
	uint32	bcn_prb;
	uint32	ts;
	uint32	time_to_tbtt;
} prb_suppress_bss_item_t;

typedef struct wlc_oce_rwan_info {
	uint8 uplink_trigger;
	uint8 downlink_trigger;
	uint8 window_time;
	uint8 pad;
} wlc_oce_rwan_info_t;

typedef struct wlc_oce_rwan_statcs {
	uint32 next_window; /* next windows in secs */
	bool roam_trigger;
	uint8 pad;
} wlc_oce_rwan_statcs_t;

/* Per 802.11-2016 Annex E-4 global opclass and channels */
/* include only subset */
#define OPCLASS_MAX_CHANNELS	13U
#define NO_OF_OPCLASS_CHLIST	19U

/* This marks the start of a packed structure section. */
#include <packed_section_start.h>
BWL_PRE_PACKED_STRUCT struct wlc_oce_opcls_chlist {
	uint8 cnt;
	uint8 opclass;
	uint8 chlist[OPCLASS_MAX_CHANNELS];
} BWL_POST_PACKED_STRUCT;
typedef struct wlc_oce_opcls_chlist wlc_oce_opcls_chlist_t;
/* This marks the end of a packed structure section. */
#include <packed_section_end.h>

typedef struct wlc_oce_cmn_info {
	uint8 probe_defer_time;	/* probe request deferral time */
	uint8 max_channel_time; /* probe response listen time */
	uint8 fd_tx_period;	/* FILS-discovery frame TX period */
	uint8 fd_tx_duration;	/* FILS-discovery frame TX duration */
	uint16 rssi_delta;	/* RSSI delta */
	wlc_oce_rwan_info_t rwan_info;
	/* MBO-OCE IE attributes handlers */
	wlc_mbo_oce_ie_build_hndl_t mbo_oce_build_h;
	wlc_mbo_oce_ie_build_data_t	mbo_oce_build_data;
	wlc_mbo_oce_ie_parse_hndl_t mbo_oce_parse_h;
	wlc_mbo_oce_ie_parse_data_t	mbo_oce_parse_data;
	wlc_esp_ie_parse_hndl_t	esp_parse_h;
	wlc_esp_ie_parse_data_t	esp_parse_data;
	wlc_esp_ie_build_hndl_t	esp_build_h;
	wlc_esp_ie_build_data_t	esp_build_data;
	wlc_ier_reg_t *ier_fd_act_frame;
	uint8 fd_frame_count;
	uint32 rnr_scan_period;	/* rnr scan timer period */
	uint8 disable_oce_prb_req_rate;
	wlc_oce_opcls_chlist_t opcls_chlist[NO_OF_OPCLASS_CHLIST];
	uint8 control_field;
	uint32 short_ssid;		/* to be used in FD frame */
	uint16 fd_cap;			/* to be used in FD frame */
	uint8 apcfg_idx_xmit_fd;	/* bsscfg sending FILS discovery frame
					 * in MBSS configuration
					 */
} wlc_oce_cmn_info_t;

struct wlc_oce_info {
	wlc_info_t	*wlc;
	/* shared info */
	wlc_oce_cmn_info_t*	oce_cmn_info;
	bcm_iov_parse_context_t *iov_parse_ctx;
	uint32 fils_req_params_bitmap;
	struct wl_timer *fd_timer;	/* FILS Discovery frame timer */
	uint32	up_tick;
	int      cfgh;    /* oce bsscfg cubby handle */
	uint8	scan_ssid[DOT11_MAX_SSID_LEN];
	uint8	scan_ssid_len;
	uint32	short_ssid;
	dll_t prb_suppress_bss_list;
	uint16	prb_suppress_bss_count;
	dll_pool_t* prb_suppress_bss_pool;
	wifi_oce_probe_suppress_bssid_attr_t *prb_suppress_bss_attr;
	wifi_oce_probe_suppress_ssid_attr_t *prb_suppress_ssid_attr;
};

/* NOTE: This whole struct is being cloned across with its contents and
 * original memeory allocation intact.
 */
typedef struct wlc_oce_bsscfg_cubby {
	oce_delayed_bssid_list_t *list_head;
	uint8 rwan_capacity; /* reduced wan metrics - available capacity */
	wlc_oce_rwan_statcs_t rwan_statcs;
	uint8	*rnrc_sssid_list;
	uint8	rnrc_sssid_list_size;
	uint8	red_wan_links; /* links values from reduced wan metrics */
	int8	ass_rej_rssi_thd;	/* Assoc rejection RSSI Threashold */
	uint8	retry_delay;		/* duration in seconds, to not accept assoc request */
	uint8	flags;
} wlc_oce_bsscfg_cubby_t;

#define OCE_BSSCFG_CUBBY_LOC(oce, cfg) ((wlc_oce_bsscfg_cubby_t **)BSSCFG_CUBBY(cfg, (oce)->cfgh))
#define OCE_BSSCFG_CUBBY(oce, cfg) (*OCE_BSSCFG_CUBBY_LOC(oce, cfg))

#define MAX_CHANNEL_TIME	255		/* TU = 1msec */

/* FILS Discovery frame transmission timer */
#define FD_TX_PERIOD		20 /* ms */
#define FD_TX_DURATION		3 /* minutes */

#define IEM_OCE_FD_FRAME_PARSE_CB_MAX	1

/* iovar table */
enum {
	IOV_OCE = 0,
	IOV_OCE_LAST
};

static const bcm_iovar_t oce_iovars[] = {
	{"oce", IOV_OCE, 0, 0, IOVT_BUFFER, 0},
	{NULL, 0, 0, 0, 0, 0}
};

/* module registration functionality */
static void wlc_oce_watchdog(void *ctx);
static int wlc_oce_wlc_up(void *ctx);
static int wlc_oce_wlc_down(void *ctx);

/* cubby registration functionality */
static int wlc_oce_bsscfg_init(void *ctx, wlc_bsscfg_t *cfg);
static void wlc_oce_bsscfg_deinit(void *ctx, wlc_bsscfg_t *cfg);

/* iovar handlers */
static void *
oce_iov_context_alloc(void *ctx, uint size);
static void
oce_iov_context_free(void *ctx, void *iov_ctx, uint size);
static int
BCMATTACHFN(oce_iov_get_digest_cb)(void *ctx, bcm_iov_cmd_digest_t **dig);
static int
wlc_oce_iov_cmd_validate(const bcm_iov_cmd_digest_t *dig, uint32 actionid,
	const uint8 *ibuf, size_t ilen, uint8 *obuf, size_t *olen);
static int wlc_oce_iov_set_enable(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen);
static int wlc_oce_iov_get_enable(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen);
static int wlc_oce_iov_set_probe_def_time(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen);
static int wlc_oce_iov_get_probe_def_time(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen);
#ifdef WL_OCE_AP
static int wlc_oce_iov_set_fd_tx_period(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen);
static int wlc_oce_iov_get_fd_tx_period(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen);
static int wlc_oce_iov_set_fd_tx_duration(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen);
static int wlc_oce_iov_get_fd_tx_duration(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen);
static int wlc_oce_iov_set_ass_rej_rssi_thd(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen);
static int wlc_oce_iov_get_ass_rej_rssi_thd(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen);
static int wlc_oce_iov_set_redwan_links(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen);
static int wlc_oce_iov_get_redwan_links(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen);
#ifdef WLWNM
static int wlc_oce_iov_get_cu_trigger(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen);
static int wlc_oce_iov_set_cu_trigger(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen);
#endif /* WLWNM */
static int wlc_oce_iov_set_retry_delay(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen);
static int wlc_oce_iov_get_retry_delay(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen);
#endif /* WL_OCE_AP */

static void wlc_oce_scan_rx_callback(void *ctx, scan_utl_scan_data_t *sd);
static void wlc_oce_reset_environment(wlc_oce_info_t* oce);
static void wlc_oce_scan_start_callback(void *ctx, wlc_ssid_t *ssid);
static ratespec_t wlc_oce_get_oce_compat_rspec(wlc_oce_info_t *oce);
static uint16 wlc_oce_write_oce_cap_ind_attr(uint8 *cp, uint8 buf_len, bool ap,
	uint8 control_field);
static uint16 wlc_oce_write_fils_req_param_element(wlc_oce_info_t *oce,
	uint8 *cp, uint8 buf_len);
static uint16 wlc_oce_get_fils_req_params_size(wlc_oce_info_t *oce);
static void wlc_oce_add_cap_ind(wlc_mbo_oce_attr_build_data_t *data, uint16 *attr_len,
	uint16 *total_len, uint8 *cp, bool ap, uint8 control_field);
static int wlc_oce_ie_build_fn(void *ctx, wlc_mbo_oce_attr_build_data_t *data);
static int wlc_oce_ie_parse_fn(void *ctx, wlc_mbo_oce_attr_parse_data_t *data);
static int wlc_oce_fils_element_build_fn(void *ctx, wlc_iem_build_data_t *data);
static uint wlc_oce_fils_element_calc_len(void *ctx, wlc_iem_calc_data_t *data);
#ifdef WL_OCE_AP
static int wlc_oce_ap_chrep_element_build_fn(void *ctx, wlc_iem_build_data_t *data);
static uint wlc_oce_ap_chrep_element_calc_len(void *ctx, wlc_iem_calc_data_t *data);
static uint16 wlc_oce_write_ap_chrep_element(wlc_oce_info_t *oce,
	uint8 *cp, uint buf_len);
static uint wlc_oce_get_ap_chrep_size(wlc_oce_info_t *oce);
static void wlc_oce_update_ap_chrep(wlc_oce_info_t* oce, wlc_bsscfg_t *bsscfg);
#endif /* WL_OCE_AP */
static int wlc_oce_rnr_element_parse_fn(void *ctx, wlc_iem_parse_data_t *data);
static int wlc_oce_parse_rnr_element(wlc_oce_info_t *oce,
	uint8* buf, uint16 buflen);
static int wlc_oce_parse_fd_action_frame(wlc_oce_info_t *oce, wlc_d11rxhdr_t *wrxh,
	struct dot11_management_header *hdr, uint8 *body, uint body_len);
#ifdef WL_ESP
static int wlc_oce_esp_ie_parse_fn(void *ctx, wlc_esp_attr_parse_data_t *data);
static void wlc_oce_parse_esp_element(wlc_oce_info_t *oce, wlc_bss_info_t *bi,
	uint8* buf, uint16 buflen);
#endif /* WL_ESP */
static int wlc_oce_bssload_ie_parse_fn(void *ctx, wlc_iem_parse_data_t *data);
static void wlc_oce_parse_bssload_ie(wlc_oce_info_t *oce, wlc_bss_info_t *bi,
        uint8* buf, uint16 buflen);

/* delayed list functionality */
static oce_delayed_bssid_list_t* wlc_oce_find_in_delayed_list(
	wlc_oce_info_t *oce, wlc_bsscfg_t *bsscfg, struct ether_addr *bssid);
static int wlc_oce_add_to_delayed_list(wlc_oce_info_t *oce, wlc_bsscfg_t *bsscfg,
	wlc_bss_info_t *bss, uint8 delay, uint8 delta);
static int wlc_oce_rm_from_delayed_list(wlc_oce_info_t *oce, wlc_bsscfg_t *bsscfg,
	oce_delayed_bssid_list_t *mmbr);
static int wlc_oce_rm_delayed_list(wlc_oce_info_t *oce, wlc_bsscfg_t *bsscfg);

/* probe suppress bss list functionality */
static prb_suppress_bss_item_t* wlc_oce_find_in_prb_suppress_bss_list(wlc_oce_info_t *oce,
	struct ether_addr *bssid);
static int wlc_oce_add_to_prb_suppress_bss_list(wlc_oce_info_t *oce,
	struct ether_addr *bssid, uint32 short_ssid, uint32 ts, uint32 time_to_tbtt, bool bcn_prb);

#ifdef WL_OCE_AP
#ifdef WLMCNX
static void wlc_oce_pretbtt_callback(void *ctx, wlc_mcnx_intr_data_t *notif_data);
#endif // endif
static void wlc_oce_set_bcn_rate(wlc_oce_info_t *oce);
uint8 wlc_chanspec_ac2opclass(chanspec_t chanspec);
#ifdef WL_ESP_AP
static int wlc_oce_esp_ie_build_fn(void *ctx, wlc_esp_attr_build_data_t *data);
#endif /* WL_ESP_AP */
#endif /* WL_OCE_AP */

#if defined(BCMDBG)
static int wlc_oce_dump(void *ctx, struct bcmstrbuf *b);
#endif // endif
static int wlc_oce_set_fd_cap(wlc_oce_info_t *oce);

#define MAX_SET_ENABLE		8
#define MIN_SET_ENABLE		MAX_SET_ENABLE
#define MAX_SET_PROBE_DEF_TIME		8
#define MIN_SET_PROBE_DEF_TIME		MAX_SET_PROBE_DEF_TIME
#ifdef WL_OCE_AP
#define MAX_SET_FD_TX_PERIOD		8
#define MIN_SET_FD_TX_PERIOD		MAX_SET_FD_TX_PERIOD
#define MAX_SET_FD_TX_DURATION		8
#define MIN_SET_FD_TX_DURATION		MAX_SET_FD_TX_DURATION
#define MAX_SET_RSSI_TH		8
#define MIN_SET_RSSI_TH		MAX_SET_RSSI_TH
#define MAX_SET_CU_TRIGGER	8
#define MIN_SET_CU_TRIGGER	MAX_SET_CU_TRIGGER
#define MIN_SET_RETRY_DELAY	0
#define MAX_SET_RETRY_DELAY	255
#endif /* WL_OCE_AP */

static const bcm_iov_cmd_info_t oce_sub_cmds[] = {
	{WL_OCE_CMD_ENABLE, BCM_IOV_CMD_FLAG_NONE,
	0, BCM_XTLV_OPTION_ALIGN32, wlc_oce_iov_cmd_validate,
	wlc_oce_iov_get_enable, wlc_oce_iov_set_enable,
	0, MIN_SET_ENABLE, MAX_SET_ENABLE, 0, 0
	},
	{WL_OCE_CMD_PROBE_DEF_TIME, BCM_IOV_CMD_FLAG_NONE,
	0, BCM_XTLV_OPTION_ALIGN32, wlc_oce_iov_cmd_validate,
	wlc_oce_iov_get_probe_def_time, wlc_oce_iov_set_probe_def_time,
	0, MIN_SET_PROBE_DEF_TIME, MAX_SET_PROBE_DEF_TIME, 0, 0
	},
#ifdef WL_OCE_AP
	{WL_OCE_CMD_FD_TX_PERIOD, BCM_IOV_CMD_FLAG_NONE,
	0, BCM_XTLV_OPTION_ALIGN32, wlc_oce_iov_cmd_validate,
	wlc_oce_iov_get_fd_tx_period, wlc_oce_iov_set_fd_tx_period, 0,
	MIN_SET_FD_TX_PERIOD, MAX_SET_FD_TX_PERIOD, 0, 0
	},
	{WL_OCE_CMD_FD_TX_DURATION, BCM_IOV_CMD_FLAG_NONE,
	0, BCM_XTLV_OPTION_ALIGN32, wlc_oce_iov_cmd_validate,
	wlc_oce_iov_get_fd_tx_duration, wlc_oce_iov_set_fd_tx_duration, 0,
	MIN_SET_FD_TX_DURATION, MAX_SET_FD_TX_DURATION, 0, 0
	},
	{WL_OCE_CMD_RSSI_TH, BCM_IOV_CMD_FLAG_NONE,
	0, BCM_XTLV_OPTION_ALIGN32, wlc_oce_iov_cmd_validate,
	wlc_oce_iov_get_ass_rej_rssi_thd, wlc_oce_iov_set_ass_rej_rssi_thd, 0,
	MIN_SET_RSSI_TH, MAX_SET_RSSI_TH, 0, 0
	},
	{WL_OCE_CMD_RWAN_LINKS, BCM_IOV_CMD_FLAG_NONE,
	0, BCM_XTLV_OPTION_ALIGN32, wlc_oce_iov_cmd_validate,
	wlc_oce_iov_get_redwan_links, wlc_oce_iov_set_redwan_links, 0,
	MIN_SET_RSSI_TH, MAX_SET_RSSI_TH, 0, 0
	},
#ifdef WLWNM
	{WL_OCE_CMD_CU_TRIGGER, BCM_IOV_CMD_FLAG_NONE,
	0, BCM_XTLV_OPTION_ALIGN32, wlc_oce_iov_cmd_validate,
	wlc_oce_iov_get_cu_trigger, wlc_oce_iov_set_cu_trigger, 0,
	MIN_SET_CU_TRIGGER, MAX_SET_CU_TRIGGER, 0, 0
	},
#endif /* WLWNM */
	{WL_OCE_CMD_RETRY_DELAY, BCM_IOV_CMD_FLAG_NONE,
	0, BCM_XTLV_OPTION_ALIGN32, wlc_oce_iov_cmd_validate,
	wlc_oce_iov_get_retry_delay, wlc_oce_iov_set_retry_delay, 0,
	MIN_SET_RETRY_DELAY, MAX_SET_RETRY_DELAY, 0, 0
	}
#endif /* WL_OCE_AP */
};

#define SUBCMD_TBL_SZ(_cmd_tbl)  (sizeof(_cmd_tbl)/sizeof(*_cmd_tbl))

static int
wlc_oce_doiovar(void *hdl, uint32 actionid,
	void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif)
{
	wlc_oce_info_t *oce = (wlc_oce_info_t *)hdl;
	int32 int_val = 0;
	int err = BCME_OK;

	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	switch (actionid) {
		case IOV_GVAL(IOV_OCE):
		case IOV_SVAL(IOV_OCE):
		{
			err = bcm_iov_doiovar(oce->iov_parse_ctx, actionid, p, plen, a, alen,
				vsize, wlcif);
			break;
		}
		default:
			err = BCME_UNSUPPORTED;
			break;
	}
	return err;
}

wlc_oce_info_t *
BCMATTACHFN(wlc_oce_attach)(wlc_info_t *wlc)
{
	wlc_oce_info_t *oce = NULL;
	bcm_iov_parse_context_t *parse_ctx = NULL;
	bcm_iov_parse_config_t parse_cfg;
	wlc_oce_cmn_info_t *oce_cmn = NULL;
	bsscfg_cubby_params_t cubby_params;
	int ret = BCME_OK;
	uint16 fils_elm_build_fstbmp = FT2BMP(FC_PROBE_REQ);
	uint16 rnr_elm_parse_fstbmp = FT2BMP(FC_PROBE_RESP) |
		FT2BMP(FC_BEACON) |
		FT2BMP(WLC_IEM_FC_SCAN_BCN) |
		FT2BMP(WLC_IEM_FC_SCAN_PRBRSP);
	uint16 bssload_parse_fstbmp = FT2BMP(WLC_IEM_FC_SCAN_BCN) |
		FT2BMP(WLC_IEM_FC_SCAN_PRBRSP);
	uint16 ap_chrep_elm_build_fstbmp = FT2BMP(FC_PROBE_RESP) | FT2BMP(FC_BEACON);

	oce = (wlc_oce_info_t *)MALLOCZ(wlc->osh, sizeof(*oce));
	if (oce == NULL) {
		WL_ERROR(("wl%d: %s:out of mem. alloced %u bytes\n",
			wlc->pub->unit, __FUNCTION__,  MALLOCED(wlc->osh)));
		goto fail;
	}

	oce_cmn = (wlc_oce_cmn_info_t *)MALLOCZ(wlc->osh, sizeof(*oce_cmn));
	if (oce_cmn == NULL) {
		WL_ERROR(("wl%d: %s:out of mem. alloced %u bytes\n",
			wlc->pub->unit, __FUNCTION__,  MALLOCED(wlc->osh)));
		goto fail;
	}

	oce_cmn->probe_defer_time = OCE_PROBE_DEFFERAL_TIME;
	oce_cmn->rssi_delta = OCE_DEF_RSSI_DELTA;
	oce_cmn->fd_tx_period = FD_TX_PERIOD;

	oce_cmn->rwan_info.uplink_trigger = OCE_DEF_RWAN_UPLINK_DELTA;
	oce_cmn->rwan_info.downlink_trigger = OCE_DEF_RWAN_DOWNLINK_DELTA;
	oce_cmn->rwan_info.window_time = OCE_DEF_RWAN_WINDOW_DUR;

	/* register OCE attributes build/parse callbacks */
	oce_cmn->mbo_oce_build_data.build_fn = wlc_oce_ie_build_fn;
	oce_cmn->mbo_oce_build_data.fstbmp = FT2BMP(FC_ASSOC_REQ) |
#ifdef WL_OCE_AP
		FT2BMP(FC_BEACON) |
#endif // endif
		FT2BMP(FC_REASSOC_REQ) |
		FT2BMP(FC_ASSOC_RESP) |
		FT2BMP(FC_REASSOC_RESP) |
		FT2BMP(FC_PROBE_RESP) |
		FT2BMP(FC_PROBE_REQ);
	oce_cmn->mbo_oce_build_data.ctx = oce;

	oce_cmn->mbo_oce_build_h =
		wlc_mbo_oce_register_ie_build_cb(wlc->mbo_oce,
			&oce_cmn->mbo_oce_build_data);

	oce_cmn->mbo_oce_parse_data.parse_fn = wlc_oce_ie_parse_fn;
	oce_cmn->mbo_oce_parse_data.fstbmp = FT2BMP(FC_BEACON) |
		FT2BMP(WLC_IEM_FC_SCAN_BCN) |
		FT2BMP(WLC_IEM_FC_SCAN_PRBRSP) |
#ifdef WL_OCE_AP
		FT2BMP(FC_ASSOC_REQ) |
		FT2BMP(FC_REASSOC_REQ) |
#endif // endif
		FT2BMP(FC_PROBE_RESP) |
		FT2BMP(FC_ASSOC_RESP) |
		FT2BMP(FC_REASSOC_RESP);
	oce_cmn->mbo_oce_parse_data.ctx = oce;
	oce_cmn->mbo_oce_parse_h =
		wlc_mbo_oce_register_ie_parse_cb(wlc->mbo_oce,
			&oce_cmn->mbo_oce_parse_data);

#ifdef WL_ESP
	/* register ESP IE parse callbacks */
	oce_cmn->esp_parse_data.parse_fn = wlc_oce_esp_ie_parse_fn;
	oce_cmn->esp_parse_data.fstbmp = FT2BMP(FC_BEACON) |
		FT2BMP(WLC_IEM_FC_SCAN_BCN) |
		FT2BMP(WLC_IEM_FC_SCAN_PRBRSP) |
		FT2BMP(FC_PROBE_RESP);
	oce_cmn->esp_parse_data.ctx = oce;
	oce_cmn->esp_parse_h =
		wlc_esp_register_ie_parse_cb(wlc->esp,
			&oce_cmn->esp_parse_data);
#endif /* WL_ESP */
#ifdef WL_ESP_AP
	oce_cmn->esp_build_data.build_fn = wlc_oce_esp_ie_build_fn;
	oce_cmn->esp_build_data.fstbmp = FT2BMP(FC_BEACON) | FT2BMP(FC_PROBE_RESP);
	oce_cmn->esp_build_data.ctx = oce;
	oce_cmn->esp_build_h = wlc_esp_register_ie_build_cb(wlc->esp,
			&oce_cmn->esp_build_data);
#endif /* WL_ESP_AP */
	oce->oce_cmn_info = oce_cmn;

	oce->wlc = wlc;

	/* parse config */
	memset(&parse_cfg, 0, sizeof(parse_cfg));
	parse_cfg.alloc_fn = (bcm_iov_malloc_t)oce_iov_context_alloc;
	parse_cfg.free_fn = (bcm_iov_free_t)oce_iov_context_free;
	parse_cfg.dig_fn = (bcm_iov_get_digest_t)oce_iov_get_digest_cb;
	parse_cfg.max_regs = 1;
	parse_cfg.alloc_ctx = (void *)oce;

	/* parse context */
	ret = bcm_iov_create_parse_context((const bcm_iov_parse_config_t *)&parse_cfg,
		&parse_ctx);
	if (ret != BCME_OK) {
		WL_ERROR(("wl%d: %s parse context creation failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	oce->iov_parse_ctx = parse_ctx;

#if defined(BCMDBG)
	if (wlc_dump_register(wlc->pub, "oce", wlc_oce_dump, oce) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_dumpe_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#endif // endif

	/* register module */
	ret = wlc_module_register(wlc->pub, oce_iovars, "OCE", oce,
		wlc_oce_doiovar, wlc_oce_watchdog,
		wlc_oce_wlc_up, wlc_oce_wlc_down);
	if (ret != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_module_register() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* reserve cubby in the bsscfg container for per-bsscfg private data */
	memset(&cubby_params, 0, sizeof(cubby_params));

	cubby_params.context = oce;
	cubby_params.fn_init = wlc_oce_bsscfg_init;
	cubby_params.fn_deinit = wlc_oce_bsscfg_deinit;

	oce->cfgh = wlc_bsscfg_cubby_reserve_ext(wlc, sizeof(wlc_oce_bsscfg_cubby_t *),
		&cubby_params);
	if (oce->cfgh < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve() failed\n",
			wlc->pub->unit, __FUNCTION__));
			goto fail;
	}

	/* register oce subcommands */
	ret = bcm_iov_register_commands(oce->iov_parse_ctx, (void *)oce,
		&oce_sub_cmds[0], (size_t)SUBCMD_TBL_SZ(oce_sub_cmds), NULL, 0);

#ifdef WL_OCE_AP
#ifdef WLMCNX
	oce->oce_cmn_info->fd_tx_duration = FD_TX_DURATION;
	if (MCNX_ENAB(wlc->pub)) {
		ret = wlc_mcnx_intr_register(wlc->mcnx, wlc_oce_pretbtt_callback, oce);
		if (ret != BCME_OK) {
			WL_ERROR(("wl%d: wlc_mcnx_intr_register failed (tbtt)\n",
				wlc->pub->unit));
			goto fail;
		}
	}
#endif /* WLMCNX */
#endif /* WL_OCE_AP */

	ret = wlc_scan_utils_rx_scan_register(wlc, wlc_oce_scan_rx_callback, oce);
	if (ret != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_scan_utils_rx_scan_register() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	ret = wlc_scan_start_register(wlc, wlc_oce_scan_start_callback, oce);
	if (ret != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_scan_utils_scan_start_register() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* register FILS elements build callbacks */
	ret = wlc_iem_add_build_fn_mft(wlc->iemi,
			fils_elm_build_fstbmp, DOT11_MNG_FILS_REQ_PARAMS,
			wlc_oce_fils_element_calc_len, wlc_oce_fils_element_build_fn, oce);
	if (ret != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_build_fn_mft failed %d, \n",
			wlc->pub->unit, __FUNCTION__, ret));
		goto fail;
	}

#ifdef WL_OCE_AP
	/* Ap channel report build callbacks, effective only when rrm module is not enabled
	 * or scan method is being used to populate nbr entries and ap channel report
	 */
	ret = wlc_iem_add_build_fn_mft(wlc->iemi,
			ap_chrep_elm_build_fstbmp, DOT11_MNG_AP_CHREP_ID,
			wlc_oce_ap_chrep_element_calc_len, wlc_oce_ap_chrep_element_build_fn, oce);
	if (ret != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_build_fn_mft failed %d, \n",
			wlc->pub->unit, __FUNCTION__, ret));
		goto fail;
	}
#endif /* WL_OCE_AP */

	/* register RNR element parse callbacks */
	ret = wlc_iem_add_parse_fn_mft(wlc->iemi, rnr_elm_parse_fstbmp, DOT11_MNG_RNR_ID,
		wlc_oce_rnr_element_parse_fn, oce);
	if (ret != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_parse_fn_mft failed %d, \n",
			wlc->pub->unit, __FUNCTION__, ret));
		goto fail;
	}

	if ((oce->oce_cmn_info->ier_fd_act_frame = wlc_ier_create_registry(wlc->ieri,
		0, IEM_OCE_FD_FRAME_PARSE_CB_MAX)) == NULL) {
		WL_ERROR(("wl%d: %s: wlc_ier_create_registry failed, FILS Discovery Action frame\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	ret = wlc_ier_add_parse_fn(oce->oce_cmn_info->ier_fd_act_frame, DOT11_MNG_RNR_ID,
			wlc_oce_rnr_element_parse_fn, oce);
	if (ret != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_ier_add_parse_fn failed, RNR IE in FD Action frame\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	dll_init(&oce->prb_suppress_bss_list);
	oce->prb_suppress_bss_pool = dll_pool_init(wlc->osh,
		MAX_PROBE_SUPPRESS_BSS, sizeof(prb_suppress_bss_item_t));

	if (!oce->prb_suppress_bss_pool) {
		WL_ERROR(("wl%d: %s: pool init failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	oce->prb_suppress_bss_attr = MALLOCZ(wlc->osh,
		OCE_PROBE_SUPPRESS_BSSID_ATTR_SIZE + (ETHER_ADDR_LEN * MAX_PROBE_SUPPRESS_BSS));

	if (!oce->prb_suppress_bss_attr) {
		WL_ERROR(("wl%d: %s: prb_suppress_bss_attr alloc failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	oce->prb_suppress_ssid_attr = MALLOCZ(wlc->osh,
		OCE_PROBE_SUPPRESS_SSID_ATTR_SIZE + (SHORT_SSID_LEN * MAX_PROBE_SUPPRESS_BSS));

	if (!oce->prb_suppress_ssid_attr) {
		WL_ERROR(("wl%d: %s: prb_suppress_ssid_attr alloc failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* register BSSLOAD IE parse callbacks */
	ret = wlc_iem_add_parse_fn_mft(wlc->iemi,
			bssload_parse_fstbmp, DOT11_MNG_QBSS_LOAD_ID,
			wlc_oce_bssload_ie_parse_fn, oce);
	if (ret != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_parse_fn_mft failed %d, \n",
			wlc->pub->unit, __FUNCTION__, ret));
		goto fail;
	}

	wlc->pub->cmn->_oce = TRUE;

	return oce;

fail:
	MODULE_DETACH(oce, wlc_oce_detach);
	return NULL;
}

void
BCMATTACHFN(wlc_oce_detach)(wlc_oce_info_t* oce)
{
	if (oce) {
		if (oce->oce_cmn_info) {
			if (oce->oce_cmn_info->ier_fd_act_frame != NULL) {
				wlc_ier_destroy_registry(oce->oce_cmn_info->ier_fd_act_frame);
			}

			if (oce->prb_suppress_bss_pool) {
				wlc_oce_flush_prb_suppress_bss_list(oce);
				dll_pool_detach(oce->wlc->osh, oce->prb_suppress_bss_pool,
				MAX_PROBE_SUPPRESS_BSS, sizeof(prb_suppress_bss_item_t));
				oce->prb_suppress_bss_pool = NULL;
			}

			if (oce->prb_suppress_ssid_attr) {
				MFREE(oce->wlc->osh, oce->prb_suppress_ssid_attr,
					OCE_PROBE_SUPPRESS_SSID_ATTR_SIZE
						 + (SHORT_SSID_LEN * MAX_PROBE_SUPPRESS_BSS));
			}

			if (oce->prb_suppress_bss_attr) {
				MFREE(oce->wlc->osh, oce->prb_suppress_bss_attr,
					OCE_PROBE_SUPPRESS_BSSID_ATTR_SIZE
						 + (ETHER_ADDR_LEN * MAX_PROBE_SUPPRESS_BSS));
			}

			wlc_mbo_oce_unregister_ie_build_cb(oce->wlc->mbo_oce,
				oce->oce_cmn_info->mbo_oce_build_h);

			wlc_mbo_oce_unregister_ie_parse_cb(oce->wlc->mbo_oce,
				oce->oce_cmn_info->mbo_oce_parse_h);

#ifdef WL_ESP
			wlc_esp_unregister_ie_parse_cb(oce->wlc->esp,
				oce->oce_cmn_info->esp_parse_h);
#endif // endif

			MFREE(oce->wlc->osh, oce->oce_cmn_info, sizeof(*oce->oce_cmn_info));
			oce->oce_cmn_info = NULL;
		}

#ifdef WL_OCE_AP
#ifdef WLMCNX
		if (MCNX_ENAB(oce->wlc->pub)) {
			wlc_mcnx_intr_unregister(oce->wlc->mcnx, wlc_oce_pretbtt_callback, oce);
		}
#endif /* WLMCNX */
#endif /* WL_OCE_AP */

		wlc_scan_utils_rx_scan_unregister(oce->wlc, wlc_oce_scan_rx_callback, oce);
		wlc_scan_start_unregister(oce->wlc, wlc_oce_scan_start_callback, oce);

		(void)bcm_iov_free_parse_context(&oce->iov_parse_ctx,
			(bcm_iov_free_t)oce_iov_context_free);

		wlc_module_unregister(oce->wlc->pub, "OCE", oce);
		MFREE(oce->wlc->osh, oce, sizeof(*oce));
	}
}

static void
wlc_oce_watchdog(void *ctx)
{

}

static int
wlc_oce_wlc_up(void *ctx)
{
#ifdef WL_OCE_AP
	wlc_oce_info_t *oce = (wlc_oce_info_t *)ctx;

	if (!OCE_ENAB(oce->wlc->pub)) {
		return BCME_OK;
	}

	wlc_oce_set_fd_cap(oce);
	wlc_oce_set_bcn_rate(oce);
#endif /* WL_OCE_AP */

	return BCME_OK;
}

static int
wlc_oce_wlc_down(void *ctx)
{
#ifdef WL_OCE_AP
	wlc_oce_info_t *oce = (wlc_oce_info_t *)ctx;

	oce->oce_cmn_info->apcfg_idx_xmit_fd = 0;
	oce->oce_cmn_info->fd_cap = 0;
	oce->oce_cmn_info->short_ssid = 0;
#endif /* WL_OCE_AP */

	return BCME_OK;
}

static int
wlc_oce_bsscfg_init(void *ctx, wlc_bsscfg_t *cfg)
{
	int ret = BCME_OK;
	wlc_oce_info_t *oce = (wlc_oce_info_t *)ctx;
	wlc_oce_bsscfg_cubby_t *obc = NULL;
	wlc_oce_bsscfg_cubby_t **pobc = NULL;
	wlc_info_t *wlc;

	ASSERT(oce != NULL);

	wlc = cfg->wlc;
	pobc = OCE_BSSCFG_CUBBY_LOC(oce, cfg);
	obc = MALLOCZ(wlc->osh, sizeof(*obc));
	if (obc == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		ret = BCME_NOMEM;
		goto fail;
	}
	*pobc = obc;
#ifdef WL_OCE_AP
	obc->rnrc_sssid_list = MALLOCZ(oce->wlc->osh, BCM_TLV_MAX_DATA_SIZE);
	if (obc->rnrc_sssid_list == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		ret = BCME_NOMEM;
		goto fail;
	}

	obc->ass_rej_rssi_thd = OCE_ASS_REJ_RSSI_INVALID;
	obc->red_wan_links = (OCE_REDUCED_WAN_METR_DEF_UPLINK_CAP << 4) +
			OCE_REDUCED_WAN_METR_DEF_DOWNLINK_CAP;
#endif /* WL_OCE_AP */

	return ret;
fail:
	if (obc) {
		wlc_oce_bsscfg_deinit(ctx, cfg);
	}
	return ret;
}

static void
wlc_oce_bsscfg_deinit(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_oce_info_t *oce = (wlc_oce_info_t *)ctx;
	wlc_oce_bsscfg_cubby_t *obc = NULL;
	wlc_oce_bsscfg_cubby_t **pobc = NULL;
	wlc_info_t *wlc;

	ASSERT(oce != NULL);
	wlc = cfg->wlc;
	pobc = OCE_BSSCFG_CUBBY_LOC(oce, cfg);
	obc = *pobc;
	if (obc != NULL) {
#ifdef WL_OCE_AP
		if (obc->rnrc_sssid_list) {
			MFREE(oce->wlc->osh, obc->rnrc_sssid_list, BCM_TLV_MAX_DATA_SIZE);
			obc->rnrc_sssid_list = NULL;
		}
#endif /* WL_OCE_AP */

		MFREE(wlc->osh, obc, sizeof(*obc));
	}
	pobc = NULL;
	return;
}

/* Return found linked list member equals to given bssid or null */
static oce_delayed_bssid_list_t*
wlc_oce_find_in_delayed_list(wlc_oce_info_t *oce, wlc_bsscfg_t *bsscfg,
	struct ether_addr *bssid)
{
	wlc_oce_bsscfg_cubby_t *occ = NULL;
	oce_delayed_bssid_list_t *cur = NULL;

	ASSERT(oce);
	ASSERT(bsscfg);

	occ = OCE_BSSCFG_CUBBY(oce, bsscfg);

	cur = occ->list_head;

	while (cur != NULL) {

		if (eacmp(bssid, &cur->bssid) == 0) {
			return cur;
		}
		cur = cur->next;
	}

	return NULL;
}

static int
wlc_oce_add_to_delayed_list(wlc_oce_info_t *oce, wlc_bsscfg_t *bsscfg,
	wlc_bss_info_t *bss, uint8 delay, uint8 delta)
{
	wlc_oce_bsscfg_cubby_t *occ = NULL;
	wlc_info_t *wlc = NULL;
	oce_delayed_bssid_list_t *new = NULL;
	struct ether_addr *bssid = &bss->BSSID;

	ASSERT(oce);
	ASSERT(bsscfg);

	wlc = oce->wlc;
	occ = OCE_BSSCFG_CUBBY(oce, bsscfg);

	new = wlc_oce_find_in_delayed_list(oce, bsscfg, bssid);
	if (new == NULL) {

		/* allocate */
		new = MALLOCZ(wlc->osh, sizeof(*new));
		if (new == NULL) {
			WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			return BCME_NOMEM;
		}
		eacopy(bssid, &new->bssid);

		if (occ->list_head) {
			new->next = occ->list_head;
			occ->list_head->prev = new;
		}
		else {
			new->next = NULL;
		}

		new->prev = NULL;
		occ->list_head = new;
	}

	new->release_rssi = bss->RSSI + delta;
	new->release_tm = OSL_SYSUPTIME() + delay * 1000;

	return BCME_OK;
}

static int
wlc_oce_rm_from_delayed_list(wlc_oce_info_t *oce, wlc_bsscfg_t *bsscfg,
	oce_delayed_bssid_list_t *mmbr)
{
	ASSERT(oce);
	ASSERT(bsscfg);
	ASSERT(mmbr);

	if (mmbr->prev)
		mmbr->prev->next = mmbr->next;

	if (mmbr->next)
		mmbr->next->prev = mmbr->prev;

	if (mmbr->prev == NULL) {
		wlc_oce_bsscfg_cubby_t * occ = OCE_BSSCFG_CUBBY(oce, bsscfg);

		if (mmbr->next == NULL) {
			occ->list_head = NULL;
		} else {
			occ->list_head = mmbr->next;
		}
	}

	MFREE(oce->wlc->osh, mmbr, sizeof(*mmbr));

	return BCME_OK;
}

static int
wlc_oce_rm_delayed_list(wlc_oce_info_t *oce, wlc_bsscfg_t *bsscfg)
{
	oce_delayed_bssid_list_t *mmbr = NULL;
	wlc_info_t *wlc = NULL;
	wlc_oce_bsscfg_cubby_t *occ = NULL;

	ASSERT(oce);
	ASSERT(bsscfg);

	occ = OCE_BSSCFG_CUBBY(oce, bsscfg);
	mmbr = occ->list_head;
	if (mmbr == NULL)
		return BCME_OK;

	wlc = oce->wlc;

	while (mmbr != NULL) {
		oce_delayed_bssid_list_t *tmp = mmbr->next;

		MFREE(wlc->osh, mmbr, sizeof(*mmbr));

		mmbr = tmp;
	}

	occ->list_head = NULL;

	return BCME_OK;
}

/* Return found linked list member equals to given bssid or null */
static prb_suppress_bss_item_t*
wlc_oce_find_in_prb_suppress_bss_list(wlc_oce_info_t *oce,
	struct ether_addr *bssid)
{
	dll_t *item_p, *next_p;

	ASSERT(oce);

	for (item_p = dll_head_p(&oce->prb_suppress_bss_list);
		!dll_end(&oce->prb_suppress_bss_list, item_p);
		item_p = next_p)
	{
		prb_suppress_bss_item_t *p = (prb_suppress_bss_item_t*)item_p;

		if (eacmp(bssid, &p->bssid) == 0) {
			return p;
		}

		next_p = dll_next_p(item_p);
	}

	return NULL;
}

static int
wlc_oce_add_to_prb_suppress_bss_list(wlc_oce_info_t *oce,
	struct ether_addr *bssid, uint32 short_ssid, uint32 ts, uint32 time_to_tbtt, bool bcn_prb)
{
	prb_suppress_bss_item_t *new = NULL;

	ASSERT(oce);

	new = wlc_oce_find_in_prb_suppress_bss_list(oce, bssid);

	if (new == NULL) {
		/* allocate */
		new = dll_pool_alloc(oce->prb_suppress_bss_pool);
		if (new == NULL) {
			WL_ERROR(("wl%d: %s: alloc failed. out of pool memory\n",
				oce->wlc->pub->unit, __FUNCTION__));
			return BCME_NOMEM;
		}
		eacopy(bssid, &new->bssid);
		new->short_ssid = short_ssid;

		dll_append(&oce->prb_suppress_bss_list, (dll_t *)new);
		oce->prb_suppress_bss_count++;
	}

	/* override FILS DF with beacon and probe response */
	if (bcn_prb) {
		new->bcn_prb = bcn_prb;
	} else {
		new->ts = ts;
		new->time_to_tbtt = time_to_tbtt;
	}

	return BCME_OK;
}

void
wlc_oce_flush_prb_suppress_bss_list(wlc_oce_info_t *oce)
{
	dll_t *item_p, *next_p;

	ASSERT(oce);

	for (item_p = dll_head_p(&oce->prb_suppress_bss_list);
		!dll_end(&oce->prb_suppress_bss_list, item_p);
		item_p = next_p)
	{
		prb_suppress_bss_item_t *p = (prb_suppress_bss_item_t*)item_p;
		next_p = dll_next_p(item_p);

		memset(&p->bssid, 0, sizeof(p->bssid));
		p->short_ssid = 0;
		p->bcn_prb = 0;

		dll_delete(item_p);
		dll_pool_free(oce->prb_suppress_bss_pool, item_p);

		oce->prb_suppress_bss_count--;
	}
}

static prb_suppress_bss_item_t*
wlc_oce_get_highest_tbtt_from_suppressed_list(wlc_oce_info_t *oce)
{
	prb_suppress_bss_item_t* bss = NULL;
	uint32 tbtt = 0;
	dll_t *item_p, *next_p;

	ASSERT(oce);

	for (item_p = dll_head_p(&oce->prb_suppress_bss_list);
		!dll_end(&oce->prb_suppress_bss_list, item_p);
		item_p = next_p)
	{
		prb_suppress_bss_item_t *p = (prb_suppress_bss_item_t*)item_p;

		if (!p->bcn_prb && tbtt < p->time_to_tbtt) {
			tbtt = p->time_to_tbtt;
			bss = p;
		}

		next_p = dll_next_p(item_p);
	}

	return bss;
}

static void
wlc_oce_update_current_slot_duration(wlc_oce_info_t *oce)
{
	prb_suppress_bss_item_t* bss;

	bss = wlc_oce_get_highest_tbtt_from_suppressed_list(oce);

	if (bss) {
		wlc_info_t *wlc = oce->wlc;
		uint32 tsf_l, tsf_h, duration, d = 0;

		wlc_read_tsf(wlc, &tsf_l, &tsf_h);

		/* time since fils df reception */
		if (tsf_l > bss->ts) {
			d = tsf_l - bss->ts;
		} else {
			/* wrap around */
			d = MAX_TSF_L - (bss->ts - tsf_l);
		}

		if (d >= bss->time_to_tbtt) {
			WL_OCE_INFO(("OCE skip bcn time update, d %d >= tbtt %d\n",
				d, bss->time_to_tbtt));
			return;
		}

		duration = (bss->time_to_tbtt - d) + TBTT_WAIT_EXTRA_TIME;

		if (duration > TBTT_WAIT_MAX_TIME) {
			WL_OCE_INFO(("OCE: required slot duration is too long %d us\n", duration));
			return;
		}

		WL_OCE_INFO(("OCE update current slot duration to %d us\n", duration));

		wlc_scan_update_current_slot_duration(wlc, duration);
	}

	return;
}

/* Check whether pointed-to IE looks like MBO_OCE. */
#define bcm_is_mbo_oce_ie(ie, tlvs, len)	bcm_has_ie(ie, tlvs, len, \
	(const uint8 *)MBO_OCE_OUI, WFA_OUI_LEN, MBO_OCE_OUI_TYPE)

/* Return OCE Capabilities Indication Attribute or NULL */
wifi_oce_cap_ind_attr_t*
wlc_oce_find_cap_ind_attr(uint8 *parse, uint16 buf_len)
{
	bcm_tlv_t *ie;
	uint len = buf_len;
	const uint8 *ptr = NULL;

	if (!parse || !buf_len) {
		return NULL;
	}
	ptr = (const uint8*)parse;

	while ((ie = bcm_find_tlv(ptr, (int)len, MBO_OCE_IE_ID))) {
		if (bcm_is_mbo_oce_ie((uint8 *)ie, &ptr, &len)) {

			wifi_oce_cap_ind_attr_t *oce_cap_ind_attr;
			uint16 len_tmp = ie->len - MBO_OCE_IE_NO_ATTR_LEN;
			uint8 *ie_buf = (uint8*)ie + MBO_OCE_IE_HDR_SIZE;

			if (ie->len < (MBO_OCE_IE_NO_ATTR_LEN + OCE_CAP_INDICATION_ATTR_SIZE)) {
				break;
			}

			oce_cap_ind_attr = (wifi_oce_cap_ind_attr_t*)bcm_find_tlv(ie_buf,
				len_tmp, OCE_ATTR_OCE_CAPABILITY_INDICATION);

			if (oce_cap_ind_attr) {
				return oce_cap_ind_attr;
			} else if (len > (ie->len + (uint)TLV_HDR_LEN)) {
				parse += (ie->len + TLV_HDR_LEN);
				len -= (ie->len + TLV_HDR_LEN);
			} else {
				break;
			}
		}
	}

	return NULL;
}

/* Return OCE BSSID Attribute or NULL */
wifi_oce_probe_suppress_bssid_attr_t*
wlc_oce_get_prb_suppr_bssid_attr(uint8 *parse, uint16 buf_len)
{
	bcm_tlv_t *ie;
	uint len = buf_len;
	const uint8 *ptr = NULL;

	if (!parse || !buf_len) {
		return NULL;
	}
	ptr = (const uint8*)parse;

	while ((ie = bcm_find_tlv(ptr, (int)len, MBO_OCE_IE_ID))) {
		if (bcm_is_mbo_oce_ie((uint8 *)ie, &ptr, &len)) {

			wifi_oce_probe_suppress_bssid_attr_t *oce_bssid_suppr_attr;
			uint16 len_tmp = ie->len - MBO_OCE_IE_NO_ATTR_LEN;
			uint8 *ie_buf = (uint8*)ie + MBO_OCE_IE_HDR_SIZE;

			if (ie->len < (MBO_OCE_IE_NO_ATTR_LEN + OCE_CAP_INDICATION_ATTR_SIZE +
				OCE_PROBE_SUPPRESS_BSSID_ATTR_SIZE)) {
				break;
			}

			oce_bssid_suppr_attr = (wifi_oce_probe_suppress_bssid_attr_t *)
				bcm_find_tlv(ie_buf, len_tmp, OCE_ATTR_PROBE_SUPPRESS_BSSID);

			if (oce_bssid_suppr_attr) {
				return oce_bssid_suppr_attr;
			} else if (len > (ie->len + (uint)TLV_HDR_LEN)) {
				parse += (ie->len + TLV_HDR_LEN);
				len -= (ie->len + TLV_HDR_LEN);
			} else {
				break;
			}
		}
	}

	return NULL;
}

/* Return OCE SSID Attribute or NULL */
wifi_oce_probe_suppress_ssid_attr_t*
wlc_oce_get_prb_suppr_ssid_attr(uint8 *parse, uint16 buf_len)
{
	bcm_tlv_t *ie;
	uint len = buf_len;
	const uint8 *ptr = NULL;

	if (!parse || !buf_len) {
		return NULL;
	}
	ptr = (const uint8*)parse;

	while ((ie = bcm_find_tlv(ptr, (int)len, MBO_OCE_IE_ID))) {
		if (bcm_is_mbo_oce_ie((uint8 *)ie, &ptr, &len)) {

			wifi_oce_probe_suppress_ssid_attr_t *oce_ssid_suppr_attr;
			uint16 len_tmp = ie->len - MBO_OCE_IE_NO_ATTR_LEN;
			uint8 *ie_buf = (uint8*)ie + MBO_OCE_IE_HDR_SIZE;

			if (ie->len < (MBO_OCE_IE_NO_ATTR_LEN + OCE_CAP_INDICATION_ATTR_SIZE +
				OCE_PROBE_SUPPRESS_SSID_ATTR_SIZE)) {
				break;
			}

			oce_ssid_suppr_attr = (wifi_oce_probe_suppress_ssid_attr_t *)
				bcm_find_tlv(ie_buf, len_tmp, OCE_ATTR_PROBE_SUPPRESS_SSID);

			if (oce_ssid_suppr_attr) {
				return oce_ssid_suppr_attr;
			} else if (len > (ie->len + (uint)TLV_HDR_LEN)) {
				parse += (ie->len + TLV_HDR_LEN);
				len -= (ie->len + TLV_HDR_LEN);
			} else {
				break;
			}
		}
	}

	return NULL;
}

static bool
wlc_oce_check_oce_capability(scan_utl_scan_data_t *sd)
{
	if (wlc_oce_find_cap_ind_attr(sd->body + DOT11_BCN_PRB_LEN,
			sd->body_len - DOT11_BCN_PRB_LEN)) {
		return TRUE;
	}

	return FALSE;
}

static void
wlc_oce_scan_rx_callback(void* ctx, scan_utl_scan_data_t *sd)
{
	wlc_oce_info_t *oce;
	wlc_info_t *wlc;

	ASSERT(ctx && sd);

	oce = (wlc_oce_info_t *)ctx;
	wlc = oce->wlc;

	if (!ETHER_ISMULTI(&wlc->scan->bssid) &&
	    !ETHER_ISNULLADDR(&wlc->scan->bssid)) {
		wlc_scan_probe_suppress(wlc->scan);
	}

	if (wlc_oce_check_oce_capability(sd)) {
#if defined(BCMDBG) || defined(WLMSG_INFORM)
		char eabuf[ETHER_ADDR_STR_LEN];
#endif // endif
		uint32 sup_short_ssid = wlc_calc_short_ssid(sd->bi->SSID, sd->bi->SSID_len);

		WL_OCE_INFO(("prb/bcn: add bssid %s ssid %s short_ssid %x to the list\n",
			bcm_ether_ntoa(&sd->bi->BSSID, eabuf),
			oce->scan_ssid, sup_short_ssid));

		wlc_oce_add_to_prb_suppress_bss_list(oce, &sd->bi->BSSID, sup_short_ssid,
			0, 0, TRUE);
	}

	wlc_oce_detect_environment(oce, sd->body, sd->body_len);
	if (!wlc_erp_find(oce->wlc, sd->body, sd->body_len, NULL, NULL)) {
		oce->oce_cmn_info->control_field |= OCE_11b_ONLY_AP_PRESENT;
	}
}

void
wlc_oce_detect_environment(wlc_oce_info_t *oce, uint8* body, uint16 body_len)
{
	wifi_oce_cap_ind_attr_t* oce_cap_ind_attr;

	oce_cap_ind_attr = wlc_oce_find_cap_ind_attr(body + DOT11_BCN_PRB_LEN,
			body_len - DOT11_BCN_PRB_LEN);

	if (!oce_cap_ind_attr) {
		oce->oce_cmn_info->control_field |= NON_OCE_AP_PRESENT;
	}
}

static void
wlc_oce_reset_environment(wlc_oce_info_t* oce)
{
	oce->oce_cmn_info->control_field = 0;
}

static void
wlc_oce_scan_start_callback(void *ctx, wlc_ssid_t *ssid)
{
	wlc_oce_info_t *oce;

	ASSERT(ctx);
	oce = (wlc_oce_info_t *)ctx;

	if ((ssid != NULL) && ssid->SSID_len) {
		if (ssid->SSID_len > DOT11_MAX_SSID_LEN) {
			WL_ERROR(("wl%d: %s: bad ssid length %d\n",
				oce->wlc->pub->unit, __FUNCTION__, ssid->SSID_len));
			return;
		}

		memcpy(oce->scan_ssid, ssid->SSID, ssid->SSID_len);
		oce->scan_ssid_len = ssid->SSID_len;
		oce->short_ssid = wlc_calc_short_ssid(ssid->SSID, ssid->SSID_len);
	} else {
		oce->scan_ssid_len = 0;
		oce->short_ssid = 0;
	}

	WL_OCE_INFO(("wl%d: %s: scan ssid %s short_ssid %x len %d\n",
		oce->wlc->pub->unit, __FUNCTION__, oce->scan_ssid,
		oce->short_ssid, ssid->SSID_len));

	wlc_oce_reset_environment(oce);
}

/* Pick up rate compatible with OCE requirements (minimum 5.5 mbps) */
static ratespec_t
wlc_oce_get_oce_compat_rspec(wlc_oce_info_t *oce)
{
	wlc_info_t *wlc = oce->wlc;
	wlc_rateset_t default_rateset;
	wlc_rateset_t basic_rateset;
	int i;

	wlc_default_rateset(wlc, &default_rateset);

	/* filter basic rates */
	wlc_rateset_filter(&default_rateset, &basic_rateset,
			TRUE, WLC_RATES_CCK_OFDM, RATE_MASK_FULL, FALSE);

	for (i = 0; i < basic_rateset.count; i++) {
		uint8 r = basic_rateset.rates[i] & RATE_MASK;
		if (r >= WLC_RATE_5M5) {
			/*
			 * Send broadcast probe request at 6Mbps for better
			 * sensitivity. OCE spec recommends min of 5.5Mbps.
			 */
			return OFDM_RSPEC(WLC_RATE_6M);
		}
	}

	return OFDM_RSPEC(WLC_RATE_1M);
}

/* OCE probe request. */
/* Transmit rate is 5.5 Mbps minimum (WFA OCE spec v5 3.3) */
bool
wlc_oce_send_probe(wlc_oce_info_t *oce, void *p)
{
	wlc_info_t *wlc = oce->wlc;
	ratespec_t rate_override;

	if (wlc_scan_get_num_passes_left(wlc) == 1) {
		wlc_oce_update_current_slot_duration(wlc->oce);
	}

	wlc->scan->state &= ~SCAN_STATE_PRB_DEFERRED;

	if ((BAND_5G(wlc->band->bandtype))) {
		return wlc_sendmgmt(wlc, p, wlc->active_queue, NULL);
	}

	if (oce->oce_cmn_info->disable_oce_prb_req_rate == TRUE) {
		rate_override = WLC_LOWEST_BAND_RSPEC(wlc->band);
	} else
		rate_override = wlc_oce_get_oce_compat_rspec(oce);

	return wlc_queue_80211_frag(wlc, p, wlc->active_queue,
		NULL, NULL, FALSE, NULL, rate_override);
}

/* Return OCE Probe Request Deferral time (WFA OCE spec v5 3.2) */
uint8
wlc_oce_get_probe_defer_time(wlc_oce_info_t *oce)
{
	ASSERT(oce);

	return oce->oce_cmn_info->probe_defer_time;
}

/* Write OCE Capabilities Attribute,
 * OCE Tech Spec v5 section 4.2.1.
 */
static uint16
wlc_oce_write_oce_cap_ind_attr(uint8 *cp, uint8 buf_len, bool ap, uint8 control_field)
{
	uint8 data = OCE_RELEASE;

	if (ap) {
		data |= control_field;
		// XXX: Infra AP should not set STA_CFON
	}

	ASSERT(buf_len >= OCE_CAP_INDICATION_ATTR_SIZE);

	bcm_write_tlv(OCE_ATTR_OCE_CAPABILITY_INDICATION, &data, sizeof(data), cp);

	return OCE_CAP_INDICATION_ATTR_SIZE;
}

static uint16
wlc_oce_get_prb_suppress_bssid_attr_len(wlc_oce_info_t *oce)
{
	uint16 attr_size = 0;

	ASSERT(oce);

	if (!oce->prb_suppress_bss_count) {
		return 0;
	}

	attr_size = OCE_PROBE_SUPPRESS_BSSID_ATTR_SIZE
		+ (ETHER_ADDR_LEN * oce->prb_suppress_bss_count);

	return attr_size;
}

/* Write Probe Suppress BSSID Attribute,
 * OCE Tech Spec v0.0.11 section 4.2.5.
 */
static uint16
wlc_oce_write_prb_suppress_bssid_attr(wlc_oce_info_t *oce, uint8 *cp, uint8 buf_len)
{
	uint16 attr_size = 0;
	uint8* ptr;
	dll_t *item_p, *next_p;

	ASSERT(oce);

	attr_size = wlc_oce_get_prb_suppress_bssid_attr_len(oce);

	if (!attr_size) {
		return 0;
	}

	if (buf_len < attr_size) {
		ASSERT(0);
		return 0;
	}

	ptr = oce->prb_suppress_bss_attr->bssid_list;

	for (item_p = dll_head_p(&oce->prb_suppress_bss_list);
		!dll_end(&oce->prb_suppress_bss_list, item_p);
		item_p = next_p)
	{
		prb_suppress_bss_item_t *p = (prb_suppress_bss_item_t*)item_p;

		eacopy((uint8*)&p->bssid, ptr);
		ptr += ETHER_ADDR_LEN;

		next_p = dll_next_p(item_p);
	}

	bcm_write_tlv(OCE_ATTR_PROBE_SUPPRESS_BSSID,
		oce->prb_suppress_bss_attr->bssid_list,
		attr_size - OCE_PROBE_SUPPRESS_BSSID_ATTR_SIZE, cp);

	return attr_size;
}

static uint16
wlc_oce_get_prb_suppress_ssid_attr_len(wlc_oce_info_t *oce)
{
	uint8 attr_size = 0, len = 0;
	dll_t *item_p;

	ASSERT(oce);

	if (!oce->prb_suppress_bss_count) {
		return 0;
	}

	for (item_p = dll_head_p(&oce->prb_suppress_bss_list);
		!dll_end(&oce->prb_suppress_bss_list, item_p);
		item_p = dll_next_p(item_p))
	{
		dll_t *rnr_p;
		prb_suppress_bss_item_t *p = (prb_suppress_bss_item_t*)item_p;

		if (p->short_ssid == 0) {
			continue;
		}

		/* XXX: Iterate thru the list and clear dups since our dll is
		 * small as MAX_PROBE_SUPPRESS_BSS (10).
		 */
		for (rnr_p = dll_next_p(item_p);
			!dll_end(&oce->prb_suppress_bss_list, rnr_p);
			rnr_p = dll_next_p(rnr_p))
		{
			prb_suppress_bss_item_t *ip = (prb_suppress_bss_item_t*)rnr_p;

			if (ip->short_ssid == p->short_ssid) {
				ip->short_ssid = 0;
			}
		}

		/* valid short_ssid */
		len += SHORT_SSID_LEN;
	}

	attr_size = OCE_PROBE_SUPPRESS_SSID_ATTR_SIZE + len;

	return attr_size;
}

/* Write Probe Suppress SSID Attribute, ********
 * OCE Tech Spec v0.0.11 section 4.2.6.
 */
static uint16
wlc_oce_write_prb_suppress_ssid_attr(wlc_oce_info_t *oce, uint8 *cp, uint8 buf_len)
{
	uint16 attr_size = 0;
	dll_t *item_p;
	uint8 *ptr;

	ASSERT(oce);

	attr_size = wlc_oce_get_prb_suppress_ssid_attr_len(oce);

	if (!attr_size) {
		return 0;
	}

	if (buf_len < attr_size) {
		ASSERT(0);
		return 0;
	}

	ptr = oce->prb_suppress_ssid_attr->ssid_list;
	bzero(ptr, SHORT_SSID_LEN * MAX_PROBE_SUPPRESS_BSS);

	/* iterate thru the list and copy non-zero short ssids */
	for (item_p = dll_head_p(&oce->prb_suppress_bss_list);
		!dll_end(&oce->prb_suppress_bss_list, item_p);
		item_p = dll_next_p(item_p))
	{
		prb_suppress_bss_item_t *p = (prb_suppress_bss_item_t*)item_p;

		if (p->short_ssid) {
			htol32_ua_store(p->short_ssid, ptr);
			ptr += SHORT_SSID_LEN;
		}
	}

	bcm_write_tlv(OCE_ATTR_PROBE_SUPPRESS_SSID,
		oce->prb_suppress_ssid_attr->ssid_list,
		attr_size - OCE_PROBE_SUPPRESS_SSID_ATTR_SIZE, cp);

	return attr_size;
}

#ifdef WL_OCE_AP
/* Write OCE Association Rejection Attribute,
 * OCE Tech Spec v5 section 4.2.2.
 */
static uint16
wlc_oce_write_oce_ass_rej_attr(uint8 *cp, uint8 buf_len, uint8 rssi_delta, uint8 retry_delay)
{
	/* Attribute data - delta RSSI, retry delay (secs) */
	uint16 data = (retry_delay << 8) + rssi_delta;

	ASSERT(buf_len >= OCE_RSSI_ASSOC_REJ_ATTR_SIZE);

	bcm_write_tlv(OCE_ATTR_RSSI_BASED_ASSOC_REJECTION, &data, sizeof(data), cp);

	return OCE_RSSI_ASSOC_REJ_ATTR_SIZE;
}

/* Write OCE Reduced WAN Metrics attribute,
 * OCE Tech Spec v5 section 4.2.3.
 */
static uint16
wlc_oce_write_reduced_wan_metr_attr(wlc_oce_info_t *oce, wlc_bsscfg_t *bsscfg, uint8 *cp,
	uint8 buf_len)
{
	wlc_oce_bsscfg_cubby_t *obc;
	/* Attribute data -  Uplink Available Capacity (4 bits),
	 * Downlink Available Capacity (4 bits)
	 */
	uint8 data;
	obc = OCE_BSSCFG_CUBBY(oce, bsscfg);
	ASSERT(obc);

	data = obc->red_wan_links;

	ASSERT(buf_len >= OCE_REDUCED_WAN_METR_ATTR_SIZE);

	bcm_write_tlv(OCE_ATTR_REDUCED_WAN_METRICS, &data, sizeof(data), cp);

	return OCE_REDUCED_WAN_METR_ATTR_SIZE;
}

uint16
wlc_oce_if_valid_assoc(wlc_oce_info_t *oce, wlc_bsscfg_t *bsscfg, int8 *rssi,
	uint8 *rej_rssi_delta)
{
	wlc_oce_bsscfg_cubby_t *obc = NULL;
	ASSERT(oce);

	obc = OCE_BSSCFG_CUBBY(oce, bsscfg);
	ASSERT(obc);

	if (obc->ass_rej_rssi_thd && (*rssi < obc->ass_rej_rssi_thd)) {

		*rej_rssi_delta = obc->ass_rej_rssi_thd - *rssi;
		return OCE_ASSOC_REJECT_RC_INSUFFICIENT_RSSI;
	}

	return DOT11_SC_SUCCESS;
}

/* Write OCE RNR completeness attribute,
 * OCE Tech Spec v18 section 4.2.4.
 */
static uint16
wlc_oce_get_rnr_completeness_attr_len(wlc_oce_info_t *oce, wlc_bsscfg_t *cfg)
{
	uint16 attr_size = 0;
	wlc_oce_bsscfg_cubby_t *obc;

	ASSERT(oce);
	ASSERT(cfg);

	obc = OCE_BSSCFG_CUBBY(oce, cfg);
	ASSERT(obc);

	if (!obc->rnrc_sssid_list_size) {
		return 0;
	}

	if (!isset(&cfg->wlc->nbr_discovery_cap, WLC_6G_CAP_RNR_IE)) {
		/* reset sssid list size */
		obc->rnrc_sssid_list_size = 0;
		return 0;
	}

	attr_size = OCE_RNR_COMPLETENESS_ATTR_SIZE + obc->rnrc_sssid_list_size;

	return attr_size;
}

/* Write OCE RNR completeness attribute,
 * OCE Tech Spec v18 section 4.2.4.
 */
static uint16
wlc_oce_write_rnr_completeness_attr(wlc_oce_info_t *oce, wlc_bsscfg_t *cfg, uint8 *cp,
	uint8 buf_len)
{
	uint16 attr_size = 0;
	wlc_oce_bsscfg_cubby_t *obc;

	ASSERT(oce);
	ASSERT(cfg);

	obc = OCE_BSSCFG_CUBBY(oce, cfg);
	if (!(attr_size = wlc_oce_get_rnr_completeness_attr_len(oce, cfg))) {
		return 0;
	}

	if (buf_len < attr_size) {
		ASSERT(0);
		return 0;
	}

	/* attr_size - OCE_RNR_COMPLETENESS_ATTR_SIZE */
	bcm_write_tlv(OCE_ATTR_RNR_COMPLETENESS,
		obc->rnrc_sssid_list,
		attr_size - OCE_RNR_COMPLETENESS_ATTR_SIZE, cp);

	return attr_size;
}
#endif /* WL_OCE_AP */

static void
wlc_oce_add_cap_ind(wlc_mbo_oce_attr_build_data_t *data, uint16 *attr_len,
	uint16 *total_len, uint8 *cp, bool ap, uint8 control_field)
{
	uint16 buf_len = data->buf_len;

	if (!data->buf_len) {
		/* return attributes length if buffer len is 0 */
		*total_len += OCE_CAP_INDICATION_ATTR_SIZE;
	} else {
		/* write OCE Capability Indication attr */
		*attr_len = wlc_oce_write_oce_cap_ind_attr(cp, buf_len, ap, control_field);
		cp += *attr_len;
		buf_len -= *attr_len;
		*total_len += *attr_len;
	}
}

/* Add OCE attributes to MBO_OCE IE */
static int
wlc_oce_ie_build_fn(void *ctx, wlc_mbo_oce_attr_build_data_t *data)
{
	uint8 *cp = NULL;

	uint16 attr_len = 0;
	uint16 total_len = 0;
#ifdef WL_OCE_AP
	uint16 buf_len = data->buf_len;
	wlc_oce_info_t *oce = (wlc_oce_info_t *)ctx;
	wlc_oce_bsscfg_cubby_t *obc = NULL;
#endif /* WL_OCE_AP */
	wlc_iem_ft_cbparm_t *ft;
	uint8 control_field = 0;

#ifdef WL_OCE_AP
	obc = OCE_BSSCFG_CUBBY(oce, data->cfg);
	if (!OCE_BSSCFG_IS_ENABLED(obc)) {
		return 0;
	}
#endif /* WL_OCE_AP */
	cp = data->buf;
	ft = data->cbparm->ft;
	control_field = oce->oce_cmn_info->control_field;

	switch (data->ft) {
		case FC_ASSOC_RESP:
		case FC_REASSOC_RESP:
#ifdef WL_OCE_AP
		{
			if (ft->assocresp.status == OCE_ASSOC_REJECT_RC_INSUFFICIENT_RSSI) {
				if (!data->buf_len) {
					/* return attributes length if buffer len is 0 */
					total_len += OCE_RSSI_ASSOC_REJ_ATTR_SIZE;
				} else {
					/* write OCE Association Rejection attr */
					attr_len = wlc_oce_write_oce_ass_rej_attr(cp, buf_len,
						ft->assocresp.status_data, obc->retry_delay);
					cp += attr_len;
					buf_len += attr_len;
					total_len += attr_len;
				}
			}

			wlc_oce_add_cap_ind(data, &attr_len, &total_len, cp, TRUE, control_field);
		}
		break;
#endif /* WL_OCE_AP */
		case FC_BEACON:
		case FC_PROBE_RESP:
#ifdef WL_OCE_AP
		{
			if (!data->buf_len) {
				/* return attributes length if buffer len is 0 */
				total_len += OCE_REDUCED_WAN_METR_ATTR_SIZE;
				total_len += wlc_oce_get_rnr_completeness_attr_len(oce, data->cfg);
			} else {
				/* write OCE Reduced WAN Metrics attr */
				attr_len = wlc_oce_write_reduced_wan_metr_attr(oce, data->cfg,
					cp, buf_len);
				cp += attr_len;
				buf_len -= attr_len;
				total_len += attr_len;

				/* write RNR completeness attribute */
				attr_len = wlc_oce_write_rnr_completeness_attr(oce, data->cfg, cp,
					buf_len);
				cp += attr_len;
				buf_len -= attr_len;
				total_len += attr_len;
			}

			wlc_oce_add_cap_ind(data, &attr_len, &total_len, cp, TRUE, control_field);
		}
		break;
#endif /* WL_OCE_AP */
		case FC_ASSOC_REQ:
		case FC_REASSOC_REQ:
			wlc_oce_add_cap_ind(data, &attr_len, &total_len, cp, FALSE, control_field);
			break;
		case FC_PROBE_REQ:
		{
			if (!ft->prbreq.da || ETHER_ISBCAST(ft->prbreq.da)) {
				if (!data->buf_len) {
					/* return attributes length if buffer len is 0 */
					total_len += wlc_oce_get_prb_suppress_bssid_attr_len(oce);

					/* add ssid attribute if wildcard SSID */
					if (!ft->prbreq.ssid_len || !ft->prbreq.ssid) {
						total_len +=
							wlc_oce_get_prb_suppress_ssid_attr_len(oce);
					}
				} else {
					/* write Probe Suppression BSSID attr */
					attr_len = wlc_oce_write_prb_suppress_bssid_attr(oce,
							cp, buf_len);
					cp += attr_len;
					buf_len -= attr_len;
					total_len += attr_len;

					/* add ssid attribute if wildcard SSID */
					if (!ft->prbreq.ssid_len || !ft->prbreq.ssid) {
						/* write Probe Suppression SSID attr */
						attr_len = wlc_oce_write_prb_suppress_ssid_attr(oce,
							cp, buf_len);
						cp += attr_len;
						buf_len -= attr_len;
						total_len += attr_len;
					}
				}
			}
			wlc_oce_add_cap_ind(data, &attr_len, &total_len, cp, FALSE, control_field);
		}
		break;
		default:
			ASSERT(0);
	}

	return total_len;
}

static void
wlc_oce_parse_bcnprb_at_scan(wlc_oce_info_t *oce,
	wlc_mbo_oce_attr_parse_data_t *data)
{
	wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;
	wlc_bss_info_t *bi = ftpparm->scan.result;
	oce_delayed_bssid_list_t *to_find = NULL;
	wlc_bsscfg_t *bsscfg = NULL;
	wifi_oce_reduced_wan_metrics_attr_t *rwan_met = NULL;

	bsscfg = data->cfg;

	if (bsscfg && bi) {
		to_find = wlc_oce_find_in_delayed_list(oce, bsscfg, &bi->BSSID);
		if (to_find) {
			uint32 cur_tm = OSL_SYSUPTIME();

			/* Updated due to OCE TechSpec v0.0.11
			 * BSSID found in delayed list.
			 * If it's delay period expired OR
			 * RSSI level gets min needed, so
			 * remove this BSSID from delyed list.
			 */
			if ((cur_tm >= to_find->release_tm) ||
			    (bi->RSSI >= to_find->release_rssi)) {
				wlc_oce_rm_from_delayed_list(oce, bsscfg, to_find);
			}
			else
				bi->flags2 |= WLC_BSS2_OCE_ASSOC_REJD;
		}
	}

	/* OCE Reduced WAN Metrics attribute */
	rwan_met = (wifi_oce_reduced_wan_metrics_attr_t *)
		bcm_find_tlv(data->ie, data->ie_len, OCE_ATTR_REDUCED_WAN_METRICS);

	if ((rwan_met != NULL) && bi) {
		bi->rwan_links = rwan_met->avail_capacity;
	}
}

bool
wlc_oce_get_trigger_rwan_roam(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	wlc_oce_bsscfg_cubby_t *occ = OCE_BSSCFG_CUBBY(wlc->oce, cfg);

	return occ->rwan_statcs.roam_trigger;
}

void
wlc_oce_reset_rwan_statcs(wlc_bsscfg_t *cfg)
{
	wlc_oce_bsscfg_cubby_t *occ = NULL;
	wlc_oce_rwan_statcs_t *rwan_statcs = NULL;

	occ = OCE_BSSCFG_CUBBY(cfg->wlc->oce, cfg);
	rwan_statcs = &occ->rwan_statcs;

	rwan_statcs->roam_trigger = FALSE;
	rwan_statcs->next_window = 0;
}

static void
wlc_oce_parse_bcnprb(wlc_oce_info_t *oce, wlc_mbo_oce_attr_parse_data_t *data)
{
	wlc_bsscfg_t *bsscfg = NULL;
	wifi_oce_reduced_wan_metrics_attr_t *rwan_met = NULL;
	wlc_oce_bsscfg_cubby_t *occ = NULL;
	wlc_oce_rwan_info_t *rwan_info = NULL;
	wlc_oce_rwan_statcs_t *rwan_statcs = NULL;
	uint8 uplink, downlink;

	bsscfg = data->cfg;
	occ = OCE_BSSCFG_CUBBY(oce, bsscfg);
	rwan_info = &oce->oce_cmn_info->rwan_info;
	rwan_statcs = &occ->rwan_statcs;

	/* OCE Reduced WAN Metrics attribute */
	rwan_met = (wifi_oce_reduced_wan_metrics_attr_t *)
		bcm_find_tlv(data->ie, data->ie_len,
		OCE_ATTR_REDUCED_WAN_METRICS);

	if (rwan_met == NULL) {
		return;
	}

	uplink = (rwan_met->avail_capacity >> 4) & 0xf;
	downlink = rwan_met->avail_capacity & 0xf;

	if (((uplink >= rwan_info->uplink_trigger) &&
	    downlink >= rwan_info->downlink_trigger)) {
		wlc_oce_reset_rwan_statcs(bsscfg);

		return;
	}

	if (!rwan_statcs->next_window)
		rwan_statcs->next_window = bsscfg->wlc->pub->now + rwan_info->window_time;

	if (rwan_statcs->next_window <= bsscfg->wlc->pub->now)
		rwan_statcs->roam_trigger = TRUE;
}

static int
wlc_oce_parse_assoc(wlc_oce_info_t *oce, wlc_mbo_oce_attr_parse_data_t *data)
{
	wlc_bss_info_t *target_bss = NULL;
	wlc_bsscfg_t *bsscfg = NULL;
	wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;
	wifi_oce_rssi_assoc_rej_attr_t *assoc_rej = NULL;
	int ret = BCME_OK;

	bsscfg = data->cfg;
	target_bss = bsscfg->target_bss;

	/* OCE Association Rejection attribute */
	assoc_rej = (wifi_oce_rssi_assoc_rej_attr_t *)
		bcm_find_tlv(data->ie, data->ie_len,
		OCE_ATTR_RSSI_BASED_ASSOC_REJECTION);

	if ((ftpparm->assocresp.status == OCE_ASSOC_REJECT_RC_INSUFFICIENT_RSSI) &&
		assoc_rej) {
		ret = wlc_oce_add_to_delayed_list(oce, bsscfg, target_bss,
				assoc_rej->retry_delay,
				assoc_rej->delta_rssi);
		if (ret != BCME_OK) {
			WL_ERROR(("wl%d: %s: failed to add reject bssid to the list\n",
				oce->wlc->pub->unit, __FUNCTION__));
			return BCME_NOTFOUND;
		}

		target_bss->flags2 |= WLC_BSS2_OCE_ASSOC_REJD;
	} else {
		wlc_oce_rm_delayed_list(oce, bsscfg);
	}

	return BCME_OK;
}

static int
wlc_oce_ie_parse_fn(void *ctx, wlc_mbo_oce_attr_parse_data_t *data)
{
	wlc_oce_info_t *oce = (wlc_oce_info_t *)ctx;
	wifi_oce_cap_ind_attr_t *ap_cap_attr = NULL;
	int ret = BCME_OK;

	if (data->ie) {
		/* OCE Capability Indication attribute */
		ap_cap_attr = (wifi_oce_cap_ind_attr_t *)
			bcm_find_tlv(data->ie, data->ie_len,
			OCE_ATTR_OCE_CAPABILITY_INDICATION);
	}

	if (ap_cap_attr == NULL) {
		return BCME_OK;
	}

	switch (data->ft) {
		case WLC_IEM_FC_SCAN_BCN:
		case WLC_IEM_FC_SCAN_PRBRSP:
			wlc_oce_parse_bcnprb_at_scan(oce, data);
			break;
#ifdef WL_OCE_AP
		case FC_ASSOC_REQ:
		case FC_REASSOC_REQ:
			break;
#endif // endif
		case FC_ASSOC_RESP:
		case FC_REASSOC_RESP:
			ret = wlc_oce_parse_assoc(oce, data);
			break;
		case FC_BEACON:
		case FC_PROBE_RESP:
			wlc_oce_parse_bcnprb(oce, data);
			break;
		default:
		{
			ASSERT(0);
		}
	}

	return ret;
}

/* Write FILS Request Parameters element for probe req,
 * OCE Tech Spec v5 section  3.7,
 * 802.11ai draft 6.2 section 8.4.2.173
 */
static int
wlc_oce_fils_element_build_fn(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_oce_info_t *oce = (wlc_oce_info_t *)ctx;
	wlc_oce_bsscfg_cubby_t *obc = NULL;
	uint8 *cp = NULL;

	obc = OCE_BSSCFG_CUBBY(oce, data->cfg);
	if (!OCE_BSSCFG_IS_ENABLED(obc)) {
		return 0;
	}

	cp = data->buf;

	switch (data->ft) {
		case FC_PROBE_REQ:
			(void)wlc_oce_write_fils_req_param_element(oce, cp, data->buf_len);
			break;
		default:
			break;
	}

	return BCME_OK;
}

/* Calculate bytes needed to write FILS Request Parameters element.
 * OCE Tech Spec v5 section 3.7,
 * 802.11ai section 9.4.2.178
 */
static uint
wlc_oce_fils_element_calc_len(void *ctx, wlc_iem_calc_data_t *data)
{
	wlc_oce_info_t *oce = (wlc_oce_info_t *)ctx;
	wlc_oce_bsscfg_cubby_t *obc = NULL;

	obc = OCE_BSSCFG_CUBBY(oce, data->cfg);
	if (!OCE_BSSCFG_IS_ENABLED(obc)) {
		return 0;
	}

	return wlc_oce_get_fils_req_params_size(oce) + BCM_TLV_EXT_HDR_SIZE;
}

/* Write FILS Request Parameters Element,
 * OCE Tech Spec v5 section 3.7,
 * 802.11ai section 9.4.2.178
 */
static uint16
wlc_oce_write_fils_req_param_element(wlc_oce_info_t *oce, uint8 *cp, uint8 buf_len)
{
	uint16 size = wlc_oce_get_fils_req_params_size(oce);
	uint8* fils_params = MALLOCZ(oce->wlc->osh, size);

	if (!fils_params) {
		WL_ERROR(("wl%d: %s:out of mem. alloced %u bytes\n",
			oce->wlc->pub->unit, __FUNCTION__,  MALLOCED(oce->wlc->osh)));
		return 0;
	}

	ASSERT(buf_len >=
		(BCM_TLV_EXT_HDR_SIZE + size));

	fils_params[0] = oce->fils_req_params_bitmap;
	fils_params[1] = oce->oce_cmn_info->max_channel_time;

	bcm_write_tlv_ext(DOT11_MNG_ID_EXT_ID, FILS_REQ_PARAMS_EXT_ID, fils_params, size, cp);

	MFREE(oce->wlc->osh, fils_params, size);

	return size + BCM_TLV_EXT_HDR_SIZE;
}

static uint16 wlc_oce_get_fils_req_params_size(wlc_oce_info_t *oce)
{
	uint16 size = 2;

	/* additional parameters size added here... */

	return size;
}

uint8
wlc_oce_get_pref_channels(chanspec_t *chanspec_list)
{
	int i;

	for (i = 0; i < sizeof(oce_pref_channels); i++) {
		chanspec_list[i] = CH20MHZ_CHSPEC(oce_pref_channels[i]);
	}

	return sizeof(oce_pref_channels);
}

bool wlc_oce_is_pref_channel(chanspec_t chanspec)
{
	int i;

	for (i = 0; i < sizeof(oce_pref_channels); i++) {
		if (CHSPEC_CHANNEL(chanspec) == oce_pref_channels[i])
			return TRUE;
	}

	return FALSE;
}

/* Set Max Channel Time Indication in units of 1ms
 * OCE Tech Spec v5 section 3.7,
 * 802.11ai draft 6.2 section 8.4.2.173
 */
void
wlc_oce_set_max_channel_time(wlc_oce_info_t *oce, uint16 time)
{
	ASSERT(oce);

	if (time < MAX_CHANNEL_TIME) {
		oce->oce_cmn_info->max_channel_time = (uint8)time;
	} else {
		oce->oce_cmn_info->max_channel_time = MAX_CHANNEL_TIME;
	}
}

/** FILS discovery frame */
static int
wlc_oce_parse_fd_action_frame(wlc_oce_info_t *oce, wlc_d11rxhdr_t *wrxh,
	struct dot11_management_header *hdr, uint8 *body, uint body_len)
{
	wlc_info_t *wlc;
	wlc_iem_upp_t upp;
	wlc_iem_ft_pparm_t ftpparm;
	wlc_iem_pparm_t pparm;
	wlc_bss_info_t bi;

	ASSERT(oce && body);

	if (body_len < (DOT11_ACTION_HDR_LEN + FD_INFO_FIELD_HDR_LEN)) {
		return BCME_BADLEN;
	}

	(void)wlc_oce_parse_fils_discovery(oce, wrxh, &hdr->bssid,
			body, body_len, &bi);

	wlc = oce->wlc;

	body += DOT11_ACTION_HDR_LEN;
	body_len -= DOT11_ACTION_HDR_LEN;

	/* prepare IE mgmt calls */
	wlc_iem_parse_upp_init(wlc->iemi, &upp);
	bzero(&ftpparm, sizeof(ftpparm));
	bzero(&pparm, sizeof(pparm));
	pparm.ft = &ftpparm;

	/* parse IEs */
	return wlc_ier_parse_frame(oce->oce_cmn_info->ier_fd_act_frame,
		NULL, WLC_IEM_FC_IER, &upp, &pparm, body, body_len);
}

int
wlc_oce_recv_fils(wlc_oce_info_t *oce, wlc_bsscfg_t *bsscfg,
	uint action_id, wlc_d11rxhdr_t *wrxh, uint8 *plcp,
	struct dot11_management_header *hdr, uint8 *body, int body_len)
{
	switch (action_id) {
	case DOT11_FILS_ACTION_DISCOVERY: {
		(void)wlc_oce_parse_fd_action_frame(oce, wrxh,
			hdr, body, body_len);
		break;
	}
	default:
		return BCME_UNSUPPORTED;
	};

	return BCME_OK;
}

/** FILS discovery frame */
static int wlc_oce_parse_fd_capability(wlc_oce_info_t *oce,
	fils_discovery_info_field_t	*fd_info, wlc_bss_info_t *bi)
{
	uint16 fc = ltoh16_ua(&fd_info->framecontrol);
	uint16 fd_cap;
	uint8 fd_cap_offset = 0;

	if (!FD_INFO_IS_CAP_PRESENT(fc)) {
		return BCME_NOTFOUND;
	}

	fd_cap_offset += FD_INFO_SSID_LENGTH(fc) + 1;

	if (FD_INFO_IS_LENGTH_PRESENT(fc)) {
		fd_cap_offset += FD_INFO_LENGTH_FIELD_SIZE;
	}

	fd_cap = ltoh16_ua(fd_info->disc_info + fd_cap_offset);

	if (FD_CAP_ESS(fd_cap)) {
		bi->capability |= DOT11_CAP_ESS;
	}

	/* farther FD capability fields parsed here */

	return BCME_OK;
}

static int wlc_oce_parse_ssid(wlc_oce_info_t *oce,
	fils_discovery_info_field_t	*fd_info, wlc_bss_info_t *bi, uint32 *short_ssid)
{
	uint16 fc = ltoh16_ua(&fd_info->framecontrol);
	uint8 ssid_len = 0;

	ssid_len = FD_INFO_SSID_LENGTH(fc) + 1;

	if (FD_INFO_IS_SHORT_SSID_PRESENT(fc)) {
		if (ssid_len != SHORT_SSID_LEN) {
			WL_ERROR(("%s: Wrong short ssid length %d\n", __FUNCTION__, ssid_len));
			return BCME_BADSSIDLEN;
		}

		*short_ssid = ltoh32_ua(fd_info->disc_info);
		bi->SSID_len = 0;
	} else {
		memcpy(bi->SSID, fd_info->disc_info, ssid_len);
		bi->SSID_len = ssid_len;
	}

	return BCME_OK;
}

bool wlc_oce_is_fils_discovery(struct dot11_management_header *hdr)
{
	uint8 cat, act;

	ASSERT(hdr);
	cat = *((uint8 *)hdr + DOT11_MGMT_HDR_LEN);	/* peek for category field */
	act = *((uint8 *)hdr + DOT11_MGMT_HDR_LEN + 1);	/* peek for action field */

	return (cat == DOT11_ACTION_CAT_PUBLIC && act == DOT11_FILS_ACTION_DISCOVERY);
}

/*
 * Parse FILS Discovery Frame.
 * OCE Tech Spec v.0.0.7, 3.3. 802.11ai, 8.6.8.36.
 */
int
wlc_oce_parse_fils_discovery(wlc_oce_info_t *oce, wlc_d11rxhdr_t *wrxh,
	struct ether_addr *bssid, uint8 *body, uint body_len, wlc_bss_info_t *bi)
{
	wlc_info_t *wlc = oce->wlc;
	fils_discovery_info_field_t	*fd_info =
		(fils_discovery_info_field_t*) (body + DOT11_ACTION_HDR_LEN);
	uint32 short_ssid = 0;
	uint32 tsf_l, tsf_h, next_tbtt, ts;
	int err = BCME_OK;
#if defined(BCMDBG) || defined(WLMSG_INFORM)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif // endif

	memset(bi, 0, sizeof(wlc_bss_info_t));

	/* get (short)ssid */
	(void)wlc_oce_parse_ssid(oce, fd_info, bi, &short_ssid);

	bi->beacon_period = ltoh16_ua(&fd_info->bcninterval);

	/* calc next tbtt info */
	ts = ltoh32_ua(&fd_info->timestamp);

	next_tbtt = CEIL(ts, bi->beacon_period * 1024) * (bi->beacon_period * 1024);

	wlc_read_tsf(wlc, &tsf_l, &tsf_h);

#if defined(BCMDBG) || defined(WLMSG_INFORM)
	WL_OCE_INFO(("fd frame: add bssid %s short ssid %x %x to the list\n",
		bcm_ether_ntoa(bssid, eabuf), oce->short_ssid, short_ssid));
#endif // endif

	wlc_oce_add_to_prb_suppress_bss_list(oce, bssid, short_ssid, tsf_l, next_tbtt - ts, FALSE);

	/* get BSS capabilities */
	(void)wlc_oce_parse_fd_capability(oce, fd_info, bi);

	memcpy((char *)&bi->BSSID, (char *)bssid, ETHER_ADDR_LEN);

	bi->RSSI = wrxh->rssi;

	bi->flags2 |= (bi->RSSI == WLC_RSSI_INVALID) ? WLC_BSS2_RSSI_INVALID : 0;

	bi->phy_noise = phy_noise_avg(WLC_PI(wlc));

	bi->SNR = (int16)wlc_lq_recv_snr_compute(wlc, (int8)bi->RSSI, bi->phy_noise);

	bi->chanspec = wlc_recv_mgmt_rx_chspec_get(wlc, wrxh);

	bi->bss_type = DOT11_BSSTYPE_INFRASTRUCTURE;

	bi->flags = 0;
	bi->flags2 = (bi->RSSI == WLC_RSSI_INVALID) ? WLC_BSS2_RSSI_INVALID : 0;

	wlc_default_rateset(wlc, &bi->rateset);

#if BAND6G
	if (CHSPEC_IS6G(bi->chanspec) && SCAN_IN_PROGRESS(wlc->scan)) {
		err = wlc_scan_6g_handle_fils_disc(wlc, bi, short_ssid);
	}
#endif /* BAND6G */
	return err;
} /* wlc_oce_parse_fils_discovery */

void
wlc_oce_process_assoc_reject(wlc_bsscfg_t *cfg, struct scb *scb, uint16 fk,
	uint8 *body, uint body_len)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_iem_upp_t upp;
	wlc_iem_ft_pparm_t ftpparm;
	wlc_iem_pparm_t pparm;

	body += sizeof(struct dot11_assoc_resp);
	body_len -= sizeof(struct dot11_assoc_resp);

	/* prepare IE mgmt calls */
	wlc_iem_parse_upp_init(wlc->iemi, &upp);
	bzero(&ftpparm, sizeof(ftpparm));
	ftpparm.assocresp.scb = scb;
	ftpparm.assocresp.status = OCE_ASSOC_REJECT_RC_INSUFFICIENT_RSSI;
	bzero(&pparm, sizeof(pparm));
	pparm.ft = &ftpparm;

	/* parse IEs */
	wlc_iem_parse_frame(wlc->iemi, cfg, fk, &upp, &pparm,
		body, body_len);
}

/* Parse RNR element in beacon/probe resp/action frame,
 * WFA OCE Tech Spec v5 3.4, 802.11ai D11 9.4.2.171
 */
static int
wlc_oce_rnr_element_parse_fn(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_oce_info_t *oce = (wlc_oce_info_t *)ctx;

	ASSERT(ctx && data);

	switch (data->ft) {
		case FC_ACTION:
		case FC_BEACON:
		case FC_PROBE_RESP:
		case WLC_IEM_FC_SCAN_PRBRSP:
		case WLC_IEM_FC_SCAN_BCN:
			if (data->ie) {
				WL_OCE_INFO(("Received RNR ie\n"));
				(void)wlc_oce_parse_rnr_element(oce, data->ie, data->ie_len);
			}
			break;
		default:
			break;
	}

	return BCME_OK;
}

#if BAND6G
/* Parse RNR elemnts */
static int
wlc_oce_6g_parse_rnr_tbtt_info(wlc_oce_info_t *oce, rnr_tbtt_info_field_t *tbtt_info,
	uint8 tbtt_info_length, bcm_tlv_t *ssid_ie, chanspec_t chanspec)
{
	wlc_info_t *wlc = oce->wlc;
	wlc_scan_6g_chan_prof_t *chan6g_prof = NULL;
	int err = BCME_OK;

	/* ALLOCATE MEMORY HERE */
	chan6g_prof = (wlc_scan_6g_chan_prof_t *)MALLOCZ(wlc->osh,
		sizeof(wlc_scan_6g_chan_prof_t));

	if (chan6g_prof == NULL) {
		WL_ERROR(("wl%d: wlc_oce_parse_rnr_element:out of mem."
			" alloced %u bytes\n",
			wlc->pub->unit, MALLOCED(wlc->osh)));
		err = BCME_NOMEM;
		goto exit;
	}

	chan6g_prof->bssid = ether_bcast;
	chan6g_prof->next_list = NULL;
	chan6g_prof->bssid_valid = FALSE;

	switch (tbtt_info_length) {
		case NBR_AP_TBTT_BSSID_SHORT_SSID_BSS_LEN:
			chan6g_prof->bss_params = tbtt_info->len12_t.bss_params;
			chan6g_prof->short_ssid =
				ltoh32(tbtt_info->len12_t.short_ssid);
			chan6g_prof->bssid = tbtt_info->len12_t.bssid;
			chan6g_prof->is_short_ssid = TRUE;
			chan6g_prof->bssid_valid = TRUE;
			break;
		case NBR_AP_TBTT_BSSID_SHORT_SSID_LEN:
			chan6g_prof->short_ssid =
				ltoh32(tbtt_info->len11_t.short_ssid);
			chan6g_prof->bssid = tbtt_info->len11_t.bssid;
			chan6g_prof->is_short_ssid = TRUE;
			chan6g_prof->bssid_valid = TRUE;
			break;
		case NBR_AP_TBTT_BSSID_BSS_LEN:
			chan6g_prof->bssid = tbtt_info->len8_t.bssid;
			chan6g_prof->bss_params = tbtt_info->len8_t.bss_params;
			chan6g_prof->bssid_valid = TRUE;
			break;
		case NBR_AP_TBTT_BSSID_LEN:
			chan6g_prof->bssid = tbtt_info->len7_t.bssid;
			chan6g_prof->bssid_valid = TRUE;
			break;
		case NBR_AP_TBTT_BSS_SHORT_SSID_LEN:
			chan6g_prof->bss_params = tbtt_info->len6_t.bss_params;
			chan6g_prof->short_ssid =
				ltoh32(tbtt_info->len6_t.short_ssid);
			chan6g_prof->is_short_ssid = TRUE;
			break;
		case NBR_AP_TBTT_SHORT_SSID_LEN:
			chan6g_prof->short_ssid =
				ltoh32(tbtt_info->len5_t.short_ssid);
			chan6g_prof->is_short_ssid = TRUE;
			break;
		case NBR_AP_TBTT_BSS_LEN:
			chan6g_prof->bss_params = tbtt_info->len2_t.bss_params;
			break;
	}

	if (RNR_BSS_PARAMS_SAME_SSID(chan6g_prof->bss_params) && (ssid_ie) &&
			(ssid_ie->len <= DOT11_MAX_SSID_LEN)) {
		/* Filling the ssid element and also checking if the already
		 * filled short_ssid is correct.
		 */
		uint32 calc_short_ssid = wlc_calc_short_ssid(ssid_ie->data, ssid_ie->len);
		if (chan6g_prof->is_short_ssid && (chan6g_prof->short_ssid != calc_short_ssid)) {
			WL_ERROR(("SSID and short_ssid donot match\n"));
			MFREE(wlc->osh, chan6g_prof,
				sizeof(wlc_scan_6g_chan_prof_t));
			err = BCME_ERROR;
			goto exit;
		} else {
			memcpy(chan6g_prof->SSID, ssid_ie->data, ssid_ie->len);
			chan6g_prof->ssid_length = ssid_ie->len;
			chan6g_prof->short_ssid = calc_short_ssid;
			chan6g_prof->is_short_ssid = TRUE;
		}
	}

	/* Save the list */
	err = wlc_scan_6g_add_to_chan_prof_list(wlc, chan6g_prof, chanspec, TRUE);
exit:
	return err;
}
#endif /* BAND6G */
static int
wlc_oce_parse_rnr_element(wlc_oce_info_t *oce, uint8* buf, uint16 buflen)
{
	neighbor_ap_info_field_t ap_info;
	uint16 tbtt_count;
	uint8 tbtt_info_length;
	bool chan_6g_valid = TRUE;
	wlc_info_t *wlc;
	uint8* cp;
	int16 len;
	uint16 i;
#if BAND6G
	bcm_tlv_t *ssid_ie;
	chanspec_t chanspec = 0;
	int err = BCME_OK;
#endif /* BAND6G */

	ASSERT(oce && buf);

	wlc = oce->wlc;
	if (buflen < FILS_RNR_ELEM_HDR_LEN) {
		WL_ERROR(("wl%d: wrong buflen: %d\n", wlc->pub->unit, buflen));
		return BCME_BADLEN;
	}

	len = buflen - FILS_RNR_ELEM_HDR_LEN;
	cp = buf + FILS_RNR_ELEM_HDR_LEN;

#if BAND6G
	ssid_ie = bcm_parse_tlvs(buf + DOT11_BCN_PRB_LEN, buflen - DOT11_BCN_PRB_LEN,
		DOT11_MNG_SSID_ID);
#endif /* BAND6G */

	while (len >= NEIGHBOR_AP_INFO_FIELD_HDR_LEN) {
		memcpy((uint8*)&ap_info, cp, NEIGHBOR_AP_INFO_FIELD_HDR_LEN);

		ap_info.tbtt_info_header = ltoh16_ua(&ap_info.tbtt_info_header);
		tbtt_info_length = TBTT_INFO_HDR_LENGTH(ap_info.tbtt_info_header);

		tbtt_count = TBTT_INFO_HDR_COUNT(ap_info.tbtt_info_header);

		cp += NEIGHBOR_AP_INFO_FIELD_HDR_LEN;
		len -= NEIGHBOR_AP_INFO_FIELD_HDR_LEN;

		if (len < tbtt_info_length && len >= tbtt_count*tbtt_info_length) {
			WL_ERROR(("wlc_oce_parse_rnr_element: Wrong RNR ie length!\n"));
			return BCME_ERROR;
		}
#if BAND6G
		if ((ap_info.op_class >= BCMWIFI_OP_CLASS_6G_20MHZ) &&
				(ap_info.op_class <= BCMWIFI_OP_CLASS_6G_80_P_80MHZ)) {
			bcmwifi_rclass_get_chanspec_from_chan(BCMWIFI_RCLASS_TYPE_GBL,
				ap_info.op_class, ap_info.channel, &chanspec);
			chan_6g_valid = wlc_valid_chanspec_db(wlc->cmi, chanspec);
		}
		if (!chan_6g_valid) {
			return BCME_BADCHAN;
		}

		if (chanspec == 0) {
			return BCME_BADCHAN;
		}
#endif /* BAND6G */
		WL_ERROR(("Neighbor AP Info Field: channel %d tbtt info count %d "
			"tbtt info len %d\n",
			ap_info.channel, tbtt_count, tbtt_info_length));

		for (i = 0; i <= tbtt_count; i++) {
			rnr_tbtt_info_field_t tbtt_info;

			/* Does RNR contains only 6G AP? Otherwise we need
			 * to keep the old code in else part
			 */
			if ((tbtt_info_length >= 1) && chan_6g_valid) {
				memcpy((uint8*)&tbtt_info, cp, tbtt_info_length);
#if BAND6G
				/* TODO: This function formulate the results and save
				 * it in scan_info structure. Currently this is strictly
				 * tied up with 6G chanspec. Need to consider 2G/5G RNR
				 * element as well
				 */
				err = wlc_oce_6g_parse_rnr_tbtt_info(oce, &tbtt_info,
					tbtt_info_length, ssid_ie, chanspec);
				if (err != BCME_OK) {
					return err;
				}
#endif /* BAND6G */
			}

			cp += tbtt_info_length;
			len -= tbtt_info_length;
		}
	}

	return BCME_OK;
}

#ifdef WL_OCE_AP
/*
 * Channel spec to Global op class conversion routine per 802.11-2016 Annex-E
 * table E-4. RNR IE operating class field specifically mentiones E-4. There
 * is per region op class implemention in wlc_channel.c but that cannot be
 * used RNR IE.
 */
static struct _global_opclass {
	uint8 global_opclass_20m[6];
	uint8 global_opclass_40m[10];
	uint8 global_opclass_80m[2];
	uint8 global_opclass_160m[1];
} global_opclass = {
	{81, 115, 118, 121, 124, 125},
	{83, 84, 116, 117, 119, 120, 122, 123, 126, 127},
	{128, 130},
	{129}};

static uint8 country_info_global_opclass_chan[] = {13, 48, 64, 144, 161, 169};

uint8
wlc_chanspec_ac2opclass(chanspec_t chanspec)
{
	uint16 primary_chan, bw, sb;
	uint i;

	primary_chan = wf_chspec_ctlchan(chanspec);
	bw = chanspec & WL_CHANSPEC_BW_MASK;
	sb = chanspec & WL_CHANSPEC_CTL_SB_MASK;

	for (i = 0; i < sizeof(country_info_global_opclass_chan); i++)
		if (primary_chan <= country_info_global_opclass_chan[i]) {
			if (bw == WL_CHANSPEC_BW_20)
				return global_opclass.global_opclass_20m[i];
			if (bw == WL_CHANSPEC_BW_40) {
				if (sb == WL_CHANSPEC_CTL_SB_L)
					return global_opclass.global_opclass_40m[i * 2];
				return global_opclass.global_opclass_40m[i * 2 + 1];
			}
		}

	if (bw == WL_CHANSPEC_BW_80)
		return 128;
	if (bw == WL_CHANSPEC_BW_160)
		return 129;
	return 0;
}

static int
wlc_oce_opclass2index(uint8 opclass)
{
	int i = 0;
	uint8 *ptr = (uint8 *)&global_opclass;

	for (i = 0; i < sizeof(global_opclass); i++) {
		if (opclass == *ptr++) {
			break;
		}
	}

	return i;
}

static int
wlc_oce_write_neighbor_ap_info_field(wlc_oce_info_t *oce, wlc_bss_info_t *bi,
	bool filter_neighbor_ap, uint8* buf)
{
	neighbor_ap_info_field_t info;
	tbtt_info_field_t tbtt_info;
	uint16 tbtt_info_hdr = 0;
	uint8 tbtt_count = 1; /* prepare NBR AP information field for every NBR */

	ASSERT(oce);

	memset(&info, 0, sizeof(info));
	memset(&tbtt_info, 0, sizeof(tbtt_info));

	/* prepare NBR AP information field */
	info.channel = wf_chspec_ctlchan(bi->chanspec);
	info.op_class = wlc_chanspec_ac2opclass(bi->chanspec);

	TBTT_INFO_HDR_SET_LENGTH(tbtt_info_hdr, (TBTT_INFO_FIELD_HDR_LEN - 1));

	if (filter_neighbor_ap) {
		TBTT_INFO_HDR_SET_FN_AP(tbtt_info_hdr, 1);
	}

	ASSERT(tbtt_count >= 1);
	TBTT_INFO_HDR_SET_COUNT(tbtt_info_hdr, (tbtt_count - 1));

	htol16_ua_store(tbtt_info_hdr, &info.tbtt_info_header);
	memcpy(buf, (uint8*)&info, NEIGHBOR_AP_INFO_FIELD_HDR_LEN);

	/* prepare tbtt information field for NBR */
	tbtt_info.tbtt_offset = 0xff;

	memcpy(tbtt_info.bssid, &bi->BSSID, ETHER_ADDR_LEN);

	tbtt_info.short_ssid = wlc_calc_short_ssid(bi->SSID, bi->SSID_len);
	htol32_ua_store(tbtt_info.short_ssid, &tbtt_info.short_ssid);

	buf += NEIGHBOR_AP_INFO_FIELD_HDR_LEN;
	memcpy(buf, (uint8*)&tbtt_info, (TBTT_INFO_FIELD_HDR_LEN - 1));

	return BCME_OK;
}

void
wlc_oce_reset_ssid_list(wlc_bsscfg_t *bsscfg)
{
	wlc_info_t *wlc;
	wlc_oce_info_t *oce;
	wlc_oce_bsscfg_cubby_t *obc;

	if (!BSSCFG_AP(bsscfg)) {
		return;
	}

	wlc = bsscfg->wlc;
	oce = wlc->oce;
	obc = OCE_BSSCFG_CUBBY(oce, bsscfg);

	obc->rnrc_sssid_list_size = 0;
	bzero(obc->rnrc_sssid_list, BCM_TLV_MAX_DATA_SIZE);
}

void
wlc_oce_set_sssid_for_rnr_completeness_attr(wlc_bsscfg_t *bsscfg, uint32 short_ssid)
{
	wlc_info_t *wlc;
	wlc_oce_info_t *oce;
	wlc_oce_bsscfg_cubby_t *obc;
	uint8 *sscp = NULL;

	if (!BSSCFG_AP(bsscfg)) {
		return;
	}

	wlc = bsscfg->wlc;
	oce = wlc->oce;
	obc = OCE_BSSCFG_CUBBY(oce, bsscfg);

	sscp = obc->rnrc_sssid_list + obc->rnrc_sssid_list_size;

	memcpy(sscp, (uint8 *)&short_ssid, SHORT_SSID_LEN);

	obc->rnrc_sssid_list_size += SHORT_SSID_LEN;
}

static void
wlc_oce_update_rnr_nbr_ap_info(wlc_oce_info_t* oce, wlc_bsscfg_t *bsscfg)
{
	wlc_oce_bsscfg_cubby_t *obc;
	wlc_bsscfg_t *cfg;
	wlc_bss_info_t *obi;
	uint32 short_ssid;
	uint8 *cp = NULL, *sscp;
	uint8 len, i, j;
	uint8 channel = 0, upd_sssid = 0;
	uint8 obi_channel;
	uint8 bss_count;
	chanvec_t channels;
	bool filter_neighbor_ap = FALSE;

	ASSERT(oce);
	ASSERT(bsscfg);

	bss_count = oce->wlc->scan_results->count;
	obc = OCE_BSSCFG_CUBBY(oce, bsscfg);
	ASSERT(obc);

	/* rnr nbr ap info is in wlc
	 * ssid list in oce's bsscfg cubby
	 */
	bsscfg->rnr_nbr_ap_info_size = 0;
	obc->rnrc_sssid_list_size = 0;

	if (!oce->wlc->rnr_update_period || !OCE_BSSCFG_IS_ENABLED(obc)) {
		RNR_UPDATE_DEL_TIMER(oce->wlc);
		return;
	}
	/* count channels with BSS presence */
	memset(&channels, 0, sizeof(chanvec_t));
	for (i = 0; i < bss_count; i++) {
		wlc_bss_info_t *bi = oce->wlc->scan_results->ptrs[i];

		if (isclr(channels.vec, CHSPEC_CHANNEL(bi->chanspec))) {
			setbit(channels.vec, CHSPEC_CHANNEL(bi->chanspec));
		}
	}
	/* make sure home channel is set */
	setbit(channels.vec, CHSPEC_CHANNEL(bsscfg->current_bss->chanspec));

	channel = bsscfg->current_bss->chanspec;

	/* clear the old one */
	bzero(bsscfg->rnr_nbr_ap_info, BCM_TLV_MAX_DATA_SIZE);
	bsscfg->rnr_nbr_ap_info_size = BCM_TLV_MAX_DATA_SIZE;

	bzero(obc->rnrc_sssid_list, BCM_TLV_MAX_DATA_SIZE);

	cp = bsscfg->rnr_nbr_ap_info;
	len = bsscfg->rnr_nbr_ap_info_size;
	sscp = obc->rnrc_sssid_list;

	/* Write any My-BSSes */
	FOREACH_UP_AP(oce->wlc, i, cfg) {
		if (cfg == bsscfg)
			continue;

		if (len >= (NEIGHBOR_AP_INFO_FIELD_HDR_LEN + (TBTT_INFO_FIELD_HDR_LEN - 1))) {

			wlc_oce_write_neighbor_ap_info_field(oce, bsscfg->current_bss,
				filter_neighbor_ap, cp);

			/* copy short ssid for rnr completeness attribute */
			short_ssid = wlc_calc_short_ssid(bsscfg->current_bss->SSID,
				bsscfg->current_bss->SSID_len);
			htol32_ua_store(short_ssid, &short_ssid);
			memcpy(sscp, (uint8 *)&short_ssid, SHORT_SSID_LEN);
			sscp += SHORT_SSID_LEN;

			len -= (NEIGHBOR_AP_INFO_FIELD_HDR_LEN + (TBTT_INFO_FIELD_HDR_LEN - 1));
			cp += (NEIGHBOR_AP_INFO_FIELD_HDR_LEN + (TBTT_INFO_FIELD_HDR_LEN - 1));
		}
	}

	for (j = 0; (j < MAXCHANNEL) &&
		(len >= (NEIGHBOR_AP_INFO_FIELD_HDR_LEN + (TBTT_INFO_FIELD_HDR_LEN - 1))); j++) {

		if (!isset(channels.vec, j)) {
			continue;
		}

		/* write BSSes with same as my SSID */
		for (i = 0; i < bss_count; i++) {
			obi = oce->wlc->scan_results->ptrs[i];
			obi_channel = CHSPEC_CHANNEL(obi->chanspec);

			if ((obi_channel == channel) && ((obi->SSID_len == bsscfg->SSID_len) &&
				!memcmp(obi->SSID, bsscfg->SSID, bsscfg->SSID_len))) {

				wlc_oce_write_neighbor_ap_info_field(oce, obi, filter_neighbor_ap,
					cp);

				/* Just one of the S-SSID is needed in RNR
				 * Completeness attribute.
				 */
				if (!upd_sssid) {
					short_ssid = wlc_calc_short_ssid(obi->SSID, obi->SSID_len);
					htol32_ua_store(short_ssid, &short_ssid);
					memcpy(sscp, (uint8 *)&short_ssid, SHORT_SSID_LEN);
					sscp += SHORT_SSID_LEN;
					upd_sssid = 1;
				}

				len -= (NEIGHBOR_AP_INFO_FIELD_HDR_LEN +
						(TBTT_INFO_FIELD_HDR_LEN - 1));
				cp += (NEIGHBOR_AP_INFO_FIELD_HDR_LEN +
						(TBTT_INFO_FIELD_HDR_LEN - 1));
				/* just one is sufficient */
				break;
			}
		}
		if (upd_sssid) {
			break;
		}
	}

	for (j = 0; (j < MAXCHANNEL) &&
		(len >= (NEIGHBOR_AP_INFO_FIELD_HDR_LEN + (TBTT_INFO_FIELD_HDR_LEN -1))); j++) {

		if (!isset(channels.vec, j)) {
			continue;
		}
		/* write any other BSSes */
		for (i = 0; ((i < bss_count) && (len >= (TBTT_INFO_FIELD_HDR_LEN -1))); i++) {
			/* OCE: standard 3.4:
			 * An OCE AP shall include in the Reduced Neighbor Report element
			 * at least the BSS that it is currently operating on different
			 * channels/bands from the BSS that is sending the Reduced Neighbor
			 * Report element (irrespective of the ESS of those BSS). The Neighbor
			 * AP TBTT Offset field indicated for those BSS shall be a valid (non-255)
			 * value.
			 */
			obi = oce->wlc->scan_results->ptrs[i];
			obi_channel = CHSPEC_CHANNEL(obi->chanspec);
			if (j != obi_channel) {
				continue;
			}

			wlc_oce_write_neighbor_ap_info_field(oce, obi, filter_neighbor_ap, cp);

			/* copy short ssid  for rnr completeness attribute. */
			short_ssid = wlc_calc_short_ssid(obi->SSID, obi->SSID_len);
			htol32_ua_store(short_ssid, &short_ssid);
			memcpy(sscp, (uint8 *)&short_ssid, SHORT_SSID_LEN);
			sscp += SHORT_SSID_LEN;

			len -= (NEIGHBOR_AP_INFO_FIELD_HDR_LEN + (TBTT_INFO_FIELD_HDR_LEN - 1));
			cp += (NEIGHBOR_AP_INFO_FIELD_HDR_LEN + (TBTT_INFO_FIELD_HDR_LEN - 1));

			clrbit(channels.vec, j);
			break;
		}
	}

	obc->rnrc_sssid_list_size = sscp - obc->rnrc_sssid_list;
	bsscfg->rnr_nbr_ap_info_size = cp - bsscfg->rnr_nbr_ap_info;
	return;
}

void
wlc_oce_scan_complete_cb(void *ctx, int status, wlc_bsscfg_t *bsscfg)
{
	wlc_info_t *wlc = (wlc_info_t*)ctx;
	wlc_oce_info_t *oce = wlc->oce;
	wlc_bsscfg_t *cfg;
	uint8 idx;

	ASSERT(oce);
	ASSERT(bsscfg);
	/* XXX: just one scanresults are sufficient to prepare neighbor info
	 * for all other BSSes.
	 */
	FOREACH_UP_AP(wlc, idx, cfg) {
		wlc_oce_update_rnr_nbr_ap_info(oce, cfg);
		wlc_oce_update_ap_chrep(oce, cfg);
		/* OCE Standard: 3.4 Reduced neighbor report and ap channel report
		 * Note: If an OCE AP is operating multiple BSS on the same channel
		 * (multiple VAPs), it is not required to transmit a Reduced
		 * Neighbor Report element on all the BSSs
		 */
		break;
	}

	wlc_suspend_mac_and_wait(wlc);
	wlc_bss_update_beacon(wlc, bsscfg);
	wlc_bss_update_probe_resp(wlc, bsscfg, FALSE);
	wlc_enable_mac(wlc);
}

/** FILS discovery frame */
static int
wlc_oce_get_fd_frame_len(wlc_oce_info_t *oce, wlc_bsscfg_t *bsscfg)
{
	uint16 frame_len;
	uint8 buf[257];
	wlc_info_t *wlc = bsscfg->wlc;

	frame_len = DOT11_ACTION_HDR_LEN +
			FD_INFO_FIELD_HDR_LEN +
			SHORT_SSID_LEN +
			FD_INFO_CAP_SUBFIELD_SIZE + FD_INFO_LENGTH_FIELD_SIZE;

	if (BAND_6G(wlc->band->bandtype)) {
		frame_len += (uint)(wlc_write_transmit_power_envelope_ie_6g(wlc,
				bsscfg->current_bss->chanspec,
				buf, sizeof(buf)) - buf);
	}

	return frame_len;
}

/** FILS discovery frame */
int
wlc_oce_write_fd_info_field(wlc_oce_info_t *oce, wlc_bsscfg_t *bsscfg,
	uint8 *buf, uint16 buf_len)
{
	fils_discovery_info_field_t *fd_info =
		(fils_discovery_info_field_t*) buf;
	wlc_info_t *wlc = bsscfg->wlc;
	uint32 discinfo_len = 0;
	uint32 tsf_l, tsf_h;

	memset(buf, 0, buf_len);

	/* beacon interval */
	fd_info->bcninterval = htol16(bsscfg->current_bss->beacon_period);

	/* TSF */
	wlc_read_tsf(oce->wlc, &tsf_l, &tsf_h);

	fd_info->timestamp[0] = htol32(tsf_l);
	fd_info->timestamp[1] = htol32(tsf_h);

	memcpy(fd_info->disc_info, &oce->oce_cmn_info->short_ssid, SHORT_SSID_LEN);

	fd_info->disc_info[SHORT_SSID_LEN] = sizeof(oce->oce_cmn_info->fd_cap);

	memcpy(fd_info->disc_info + SHORT_SSID_LEN + FD_INFO_LENGTH_FIELD_SIZE,
		&oce->oce_cmn_info->fd_cap,
		sizeof(oce->oce_cmn_info->fd_cap));
	discinfo_len = SHORT_SSID_LEN;
	discinfo_len += sizeof(oce->oce_cmn_info->fd_cap);
	discinfo_len += FD_INFO_LENGTH_FIELD_SIZE; /* length octet */

	FD_INFO_SET_SHORT_SSID_PRESENT(fd_info->framecontrol);
	FD_INFO_SET_SSID_LENGTH(fd_info->framecontrol, SHORT_SSID_LEN - 1);

	FD_INFO_SET_CAP_PRESENT(fd_info->framecontrol);
	FD_INFO_SET_LENGTH_PRESENT(fd_info->framecontrol);

	if (oce->oce_cmn_info->control_field & OCE_11b_ONLY_AP_PRESENT) {
		FD_INFO_SET_11B_AP_NBR_PRESENT(fd_info->framecontrol);
	}
	if (oce->oce_cmn_info->control_field & NON_OCE_AP_PRESENT) {
		FD_INFO_SET_NON_OCE_AP_PRESENT(fd_info->framecontrol);
	}

	fd_info->framecontrol = htol16(fd_info->framecontrol);

	if (BAND_6G(wlc->band->bandtype)) {
		(wlc_write_transmit_power_envelope_ie_6g(wlc,
		     bsscfg->current_bss->chanspec, fd_info->disc_info + discinfo_len,
		     buf_len - (sizeof(fils_discovery_info_field_t) + discinfo_len)));
	}

	return BCME_OK;
}

/* FILS Discovery Frame: 802.11ai D11, 9.6.8.36 */
static wlc_pkt_t
wlc_oce_init_fd_frame(wlc_oce_info_t *oce, wlc_bsscfg_t *bsscfg)
{
	dot11_action_frmhdr_t* fd_action_frame;
	uint8 *pbody;
	wlc_pkt_t pkt;

	uint16 frame_len = wlc_oce_get_fd_frame_len(oce, bsscfg);

	pkt = wlc_frame_get_action(oce->wlc, &ether_bcast, &bsscfg->cur_etheraddr,
		&bsscfg->BSSID, frame_len, &pbody, DOT11_ACTION_CAT_PUBLIC);

	if (!pkt) {
		WL_ERROR(("%s, Could not allocate fils dicovery frame\n", __FUNCTION__));
		return NULL;
	}

	fd_action_frame = (dot11_action_frmhdr_t*) pbody;

	fd_action_frame->category = DOT11_ACTION_CAT_PUBLIC;
	fd_action_frame->action = DOT11_FILS_ACTION_DISCOVERY;

	wlc_oce_write_fd_info_field(oce, bsscfg,
		pbody + DOT11_ACTION_HDR_LEN, frame_len - DOT11_ACTION_HDR_LEN);

	//prhex("SW MANAGED FD: \n", PKTDATA(oce->wlc->osh, pkt), PKTLEN(oce->wlc->osh, pkt));
	return pkt;
}

/* FILS Discovery Frame transmission,
 * WFA-OCE spec v0.0.7, 3.3
 */
int
wlc_oce_send_fd_frame(wlc_oce_info_t *oce, wlc_bsscfg_t *bsscfg)
{
	wlc_pkt_t pkt;
	ratespec_t rate_override = 0;

	if (!(pkt = wlc_oce_init_fd_frame(oce, bsscfg))) {
		return BCME_NOMEM;
	}

	rate_override = WLC_RATE_6M;

	PKTSETPRIO(pkt, MAXPRIO);
	WLPKTTAG(pkt)->flags |= WLF_PSDONTQ;
	WLPKTTAG(pkt)->flags |= WLF_CTLMGMT;
	WLPKTTAG(pkt)->flags3 |= WLF3_TXQ_SHORT_LIFETIME;

	/* life time of pkt 20 msec */
	wlc_lifetime_set(oce->wlc, pkt, oce->wlc->upr_fd_info->period * 1000);

	if (wlc_queue_80211_frag(oce->wlc, pkt, bsscfg->wlcif->qi,
		oce->wlc->band->hwrs_scb, bsscfg, FALSE, NULL, rate_override)) {
		oce->wlc->upr_fd_info->n_pkts_xmit++;
	} else {
		oce->wlc->upr_fd_info->n_pkts_xmit_fail++;
	}
	return BCME_OK;
}

#ifdef WLMCNX
/* Use WLC APTT method to prepare FD discovery frame, MCNX will
 * not be enabled at time of enabling OCE-AP.
 * FILS discovery frame
 */
static void
wlc_oce_pretbtt_callback(void *ctx, wlc_mcnx_intr_data_t *notif_data)
{
	wlc_oce_info_t *oce = (wlc_oce_info_t*) ctx;
	wlc_info_t *wlc = oce->wlc;
	wlc_bsscfg_t *bsscfg;
	int i;

	ASSERT(ctx);

	if (!oce->oce_cmn_info->fd_tx_period) {
		/* if fd disabled */
		return;
	}

	if (notif_data->intr == M_P2P_I_PRE_TBTT) {
		oce->oce_cmn_info->fd_frame_count = 0;

		/* 3min wait time is needed only for CFON. */
		if ((OSL_SYSUPTIME() - oce->up_tick) < oce->oce_cmn_info->fd_tx_duration) {
			/* calculate number of FD frames per beacon interval */
			FOREACH_UP_AP(wlc, i, bsscfg) {
			oce->oce_cmn_info->fd_frame_count +=
			(bsscfg->current_bss->beacon_period / oce->oce_cmn_info->fd_tx_period) - 1;
			/* just one bss is ok since rnr ie is sent */
			if (oce->oce_cmn_info->rnr_scan_period)
				break;
			}

			/* initiate FD frame transmission */
			WLC_UPR_FD_START_TIMER(wlc->upr_fd_info);
		}
	}
}

#else

/** FILS discovery frame */
void
wlc_oce_pretbtt_fd_callback(wlc_oce_info_t *oce)
{
	wlc_info_t *wlc;
	wlc_bsscfg_t *bsscfg;
	int i;

	ASSERT(oce);
	wlc = oce->wlc;

	if (!oce->oce_cmn_info->fd_tx_period ||
		!isset(&(wlc->nbr_discovery_cap), WLC_6G_CAP_20TU_FILS_DISCOVERY)) {
		/* if fd disabled */
		return;
	}

	oce->oce_cmn_info->fd_frame_count = 0;
	FOREACH_UP_AP(wlc, i, bsscfg) {
		/* BIs in AP (MBSS) are same; use default. */
		oce->oce_cmn_info->fd_frame_count +=
			(wlc->default_bss->beacon_period / oce->oce_cmn_info->fd_tx_period) - 1;
		/* just one bss is ok since rnr ie is sent */
		if (oce->wlc->rnr_update_period)
			break;
	}
}
#endif /* WLMCNX */

/* Add AP channel report element in beacon/probe resp per 802.11-2016,
 * 9.4.2.36 and WFA OCE spec v18, 3.4.
 */
static uint
wlc_oce_ap_chrep_element_calc_len(void *ctx, wlc_iem_calc_data_t *data)
{
	wlc_oce_info_t *oce = (wlc_oce_info_t *)ctx;
	wlc_oce_bsscfg_cubby_t *obc = NULL;

	ASSERT(oce);

	if (WL11K_ENAB(oce->wlc->pub) && BSSCFG_AP(data->cfg) &&
		wlc_rrm_enabled(oce->wlc->rrm_info, data->cfg)) {
		return 0;
	}

	obc = OCE_BSSCFG_CUBBY(oce, data->cfg);
	if (!OCE_BSSCFG_IS_ENABLED(obc)) {
		return 0;
	}

	return wlc_oce_get_ap_chrep_size(oce);
}

static int
wlc_oce_ap_chrep_element_build_fn(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_oce_info_t *oce = (wlc_oce_info_t *)ctx;
	wlc_oce_bsscfg_cubby_t *obc = NULL;
	uint8 *cp = NULL;

	ASSERT(oce);

	obc = OCE_BSSCFG_CUBBY(oce, data->cfg);
	if (!OCE_BSSCFG_IS_ENABLED(obc)) {
		return BCME_OK;
	}
	cp = data->buf;

	switch (data->ft) {
		case FC_BEACON:
		case FC_PROBE_RESP:
			(void)wlc_oce_write_ap_chrep_element(oce, cp, data->buf_len);
			break;
		default:
			break;
	}

	return BCME_OK;
}
static uint16
wlc_oce_write_ap_chrep_element(wlc_oce_info_t *oce, uint8 *cp, uint buf_len)
{
	uint8 i, attr_size = 0;

	ASSERT(oce);

	attr_size = wlc_oce_get_ap_chrep_size(oce);

	if (!attr_size) {
		return 0;
	}

	if (buf_len < attr_size) {
		ASSERT(0);
		return 0;
	}

	for (i = 0; i < NO_OF_OPCLASS_CHLIST; i++) {
		if (oce->oce_cmn_info->opcls_chlist[i].cnt) {
			bcm_write_tlv(DOT11_MNG_AP_CHREP_ID,
				&oce->oce_cmn_info->opcls_chlist[i].opclass,
				oce->oce_cmn_info->opcls_chlist[i].cnt + 1, cp);
			cp += (oce->oce_cmn_info->opcls_chlist[i].cnt + 1 + BCM_TLV_HDR_SIZE);
		}
	}

	return attr_size;
}

static uint
wlc_oce_get_ap_chrep_size(wlc_oce_info_t *oce)
{
	uint8 i, size = 0;

	ASSERT(oce);

	if (!OCE_ENAB(oce->wlc->pub)) {
		return 0;
	}

	for (i = 0; i < NO_OF_OPCLASS_CHLIST; i++) {
		if (oce->oce_cmn_info->opcls_chlist[i].cnt) {
			/* no. of chans + opclass + tlv hdr size */
			size += (oce->oce_cmn_info->opcls_chlist[i].cnt + 1 + BCM_TLV_HDR_SIZE);
		}
	}

	return size;
}

static void
wlc_oce_update_ap_chrep(wlc_oce_info_t* oce, wlc_bsscfg_t *bsscfg)
{
	wlc_oce_cmn_info_t *oce_cmn;
	wlc_oce_bsscfg_cubby_t *obc = NULL;
	uint8 i;
	uint16 bss_count;
	chanvec_t channels;

	ASSERT(oce);
	ASSERT(bsscfg);

	bss_count = oce->wlc->scan_results->count;
	oce_cmn = oce->oce_cmn_info;
	ASSERT(oce_cmn);

	obc = OCE_BSSCFG_CUBBY(oce, bsscfg);
	if (!oce->wlc->rnr_update_period || !OCE_BSSCFG_IS_ENABLED(obc)) {
		RNR_UPDATE_DEL_TIMER(oce->wlc);
		return;
	}

	/* reset */
	memset(&oce_cmn->opcls_chlist, 0, sizeof(oce_cmn->opcls_chlist));

	/* channels found in scan */
	memset(&channels, 0, sizeof(chanvec_t));
	for (i = 0; i < bss_count; i++) {
		wlc_bss_info_t *bi = oce->wlc->scan_results->ptrs[i];
		uint16 primary_chan = wf_chspec_ctlchan(bi->chanspec);

		if (isclr(channels.vec, primary_chan)) {
			uint8 opclass, opi;

			setbit(channels.vec, primary_chan);

			/* convert all the chanspecs to 20MHz chanspec and
			 * report their primary channels in AP channel report
			 * to save space.
			 */
			opclass = wlc_chanspec_ac2opclass(CH20MHZ_CHSPEC(primary_chan));
			opi = wlc_oce_opclass2index(opclass);

			ASSERT(oce_cmn->opcls_chlist[opi].cnt < OPCLASS_MAX_CHANNELS);
			oce_cmn->opcls_chlist[opi].opclass = opclass;
			oce_cmn->opcls_chlist[opi].chlist[oce_cmn->opcls_chlist[opi].cnt++] =
				primary_chan;
		}
	}

	return;
}
#endif /* WL_OCE_AP */

#ifdef WL_ESP
/* Parse ESP element in beacon/probe resp frames,
 * WFA OCE Tech Spec v0.0.13 3.15, 802.11-2016 9.4.2.174
 */
static int
wlc_oce_esp_ie_parse_fn(void *ctx, wlc_esp_attr_parse_data_t *data)
{
	wlc_oce_info_t *oce = (wlc_oce_info_t *)ctx;

	ASSERT(ctx && data);

	if (data->ie_len < DOT11_ESP_IE_INFO_LIST_SIZE)
		return BCME_OK;

	switch (data->ft) {
		case WLC_IEM_FC_SCAN_PRBRSP:
		case WLC_IEM_FC_SCAN_BCN:
		{
			wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;
			wlc_bss_info_t *bi = ftpparm->scan.result;

			if (data->ie) {
				(void)wlc_oce_parse_esp_element(oce, bi, data->ie, data->ie_len);
			}
		}
			break;
		case FC_BEACON:
		case FC_PROBE_RESP:
		default:
			break;
	}

	return BCME_OK;
}
static void
wlc_oce_parse_esp_element(wlc_oce_info_t *oce, wlc_bss_info_t *bi,
	uint8* buf, uint16 buflen)
{
	dot11_esp_ie_info_list_t *list;
	uint16 nbr_of_lists;
	uint16 i;

	list = (dot11_esp_ie_info_list_t *)buf;
	nbr_of_lists = buflen/DOT11_ESP_IE_INFO_LIST_SIZE;

	/* looking only for Estimated AirTime Fraction of Best Effort */
	for (i = 0; i < nbr_of_lists; i++) {
		uint8 ac_df_baws = list->ac_df_baws;
		if (ac_df_baws & DOT11_ESP_INFO_LIST_AC_BE) {
			bi->eat_frac = list->eat_frac;
			break;
		}
		list++;
	}
}
#endif /* WL_ESP */

#ifdef WL_ESP_AP
/* Build ESP element in beacon/probe resp frames,
 * WFA OCE Tech Spec v0.0.13 3.15, 802.11-2016 9.4.2.174
 */
static int
wlc_oce_esp_ie_build_fn(void *ctx, wlc_esp_attr_build_data_t *data)
{
	wlc_oce_info_t *oce = (wlc_oce_info_t *)ctx;
	wlc_info_t *wlc = oce->wlc;
	int len = 0;
	uint8 *cp = NULL;

	ASSERT(ctx);
	ASSERT(data);

	if (!data->buf_len) {
		return wlc_esp_ie_calc_len_with_dynamic_list(wlc->esp);
	}

	cp = data->buf;
	switch (data->ft) {
		case FC_BEACON:
		case FC_PROBE_RESP:
		{
			len = wlc_esp_write_esp_element(wlc, cp, data->buf_len);
			break;
		}
		default:
			break;
	}

	return len;
}
#endif /* WL_ESP_AP */

/* validation function for IOVAR */
static int
wlc_oce_iov_cmd_validate(const bcm_iov_cmd_digest_t *dig, uint32 actionid,
	const uint8 *ibuf, size_t ilen, uint8 *obuf, size_t *olen)
{
	int ret = BCME_OK;
	wlc_info_t *wlc = NULL;
	wlc_oce_info_t *oce = NULL;

	ASSERT(dig);
	oce = (wlc_oce_info_t *)dig->cmd_ctx;
	ASSERT(oce);
	wlc = oce->wlc;

	UNUSED_PARAMETER(wlc);

	if (!OCE_ENAB(wlc->pub) &&
		(dig->cmd_info->cmd != WL_OCE_CMD_ENABLE)) {
		WL_ERROR(("wl%d: %s: Command unsupported\n",
			wlc->pub->unit, __FUNCTION__));
		ret = BCME_UNSUPPORTED;
		goto fail;
	}
fail:
	return ret;
}

/* iovar context alloc */
static void *
oce_iov_context_alloc(void *ctx, uint size)
{
	uint8 *iov_ctx = NULL;
	wlc_oce_info_t *oce = (wlc_oce_info_t *)ctx;

	ASSERT(oce != NULL);

	iov_ctx = MALLOCZ(oce->wlc->osh, size);
	if (iov_ctx == NULL) {
		WL_ERROR(("wl%d: %s: MALLOC failed, malloced %d bytes\n",
			oce->wlc->pub->unit, __FUNCTION__, MALLOCED(oce->wlc->osh)));
	}

	return iov_ctx;
}

/* iovar context free */
static void
oce_iov_context_free(void *ctx, void *iov_ctx, uint size)
{
	wlc_oce_info_t *oce = (wlc_oce_info_t *)ctx;

	ASSERT(oce != NULL);
	if (iov_ctx) {
		MFREE(oce->wlc->osh, iov_ctx, size);
	}
}

/* command digest alloc function */
static int
BCMATTACHFN(oce_iov_get_digest_cb)(void *ctx, bcm_iov_cmd_digest_t **dig)
{
	wlc_oce_info_t *oce = (wlc_oce_info_t *)ctx;
	int ret = BCME_OK;
	uint8 *iov_cmd_dig = NULL;

	ASSERT(oce != NULL);
	iov_cmd_dig = MALLOCZ(oce->wlc->osh, sizeof(bcm_iov_cmd_digest_t));
	if (iov_cmd_dig == NULL) {
		WL_ERROR(("wl%d: %s: MALLOC failed, malloced %d bytes\n",
			oce->wlc->pub->unit, __FUNCTION__, MALLOCED(oce->wlc->osh)));
		ret = BCME_NOMEM;
		goto fail;
	}
	*dig = (bcm_iov_cmd_digest_t *)iov_cmd_dig;
fail:
	return ret;
}

/* "wl oce enable <>" handler */
static int
wlc_oce_iov_set_enable(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen)
{
	int ret = BCME_OK;
	uint16 len;
	uint16 type;
	wlc_oce_info_t *oce = (wlc_oce_info_t *)dig->cmd_ctx;
	wlc_oce_bsscfg_cubby_t *obc = NULL;
	const bcm_xtlv_t *ptlv = (const bcm_xtlv_t *)ibuf;
	const uint8 *data;

	if (!ibuf || !ilen) {
		return BCME_BADARG;
	}
	obc = OCE_BSSCFG_CUBBY(oce, dig->bsscfg);

	bcm_xtlv_unpack_xtlv(ptlv, &type, &len, &data, BCM_XTLV_OPTION_ALIGN32);

	if (type != WL_OCE_XTLV_ENABLE || !len) {
		WL_ERROR(("wl%d: %s: wrong xtlv: id %d len %d\n",
			oce->wlc->pub->unit, __FUNCTION__, type, len));

		return BCME_BADARG;
	}

	if (*data & OCE_PRB_REQ_RATE_DISABLE) {
		oce->oce_cmn_info->disable_oce_prb_req_rate = TRUE;
	} else {
		oce->oce_cmn_info->disable_oce_prb_req_rate = FALSE;
	}

#ifdef WL_OCE_AP
	if (OCE_ENAB(oce->wlc->pub)) {
		oce->oce_cmn_info->fd_tx_period = FD_TX_PERIOD;
		if (*data) {
			obc->flags |= OCE_BSSCFG_ENABLE;
		} else {
			obc->flags &= ~OCE_BSSCFG_ENABLE;
		}
	} else {
		oce->oce_cmn_info->fd_tx_period = 0;
		oce->oce_cmn_info->rnr_scan_period = 0;
	}
	/* set/reset beacon rate */
	wlc_oce_set_bcn_rate(oce);
#endif /* WL_OCE_AP */

	wlc_update_beacon(oce->wlc);
	wlc_update_probe_resp(oce->wlc, FALSE);

	return ret;
}

/* "wl oce enable" handler */
static int
wlc_oce_iov_get_enable(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen)
{
	int ret = BCME_OK;
	uint16 xtlv_size = 0;
	wlc_oce_info_t *oce = (wlc_oce_info_t *)dig->cmd_ctx;
	wlc_oce_bsscfg_cubby_t *obc = NULL;
	uint16 buflen = 0;
	uint8 enable;

	if (!obuf || !*olen) {
		ret = BCME_BADARG;
		goto fail;
	}

	obc = OCE_BSSCFG_CUBBY(oce, dig->bsscfg);
	xtlv_size = bcm_xtlv_size_for_data(sizeof(enable),
		BCM_XTLV_OPTION_ALIGN32);
	if (xtlv_size > *olen) {
		WL_OCE_ERR(("wl%d: %s: short buffer length %d expected %u\n",
			oce->wlc->pub->unit, __FUNCTION__, (int)*olen, xtlv_size));
		*olen = 0;
		ret = BCME_BUFTOOSHORT;
		goto fail;
	}
	buflen = *olen;

	enable = OCE_BSSCFG_IS_ENABLED(obc) ? 1 : 0;

	ret = bcm_pack_xtlv_entry(&obuf, &buflen, WL_OCE_XTLV_ENABLE, sizeof(enable),
			&enable, BCM_XTLV_OPTION_ALIGN32);
	if (ret != BCME_OK) {
		WL_OCE_ERR(("wl%d: %s: packing xtlv failed\n",
			oce->wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	*olen = *olen - buflen;

fail:
	return ret;
}

/* "wl oce probe_def_time <>" handler */
static int
wlc_oce_iov_set_probe_def_time(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen)
{
	int ret = BCME_OK;
	uint16 len;
	uint16 type;
	const bcm_xtlv_t *ptlv = (const bcm_xtlv_t *)ibuf;
	const uint8 *data;
	wlc_oce_info_t *oce = (wlc_oce_info_t *)dig->cmd_ctx;

	if (!ibuf || !ilen) {
		return BCME_BADARG;
	}

	bcm_xtlv_unpack_xtlv(ptlv, &type, &len, &data, BCM_XTLV_OPTION_ALIGN32);

	if (type != WL_OCE_XTLV_PROBE_DEF_TIME || !len) {
		WL_ERROR(("wl%d: %s: wrong xtlv: id %d len %d\n",
			oce->wlc->pub->unit, __FUNCTION__, type, len));

		return BCME_BADARG;
	}

	oce->oce_cmn_info->probe_defer_time = *data;

	return ret;
}

/* "wl oce probe_def_time" handler */
static int
wlc_oce_iov_get_probe_def_time(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen)
{
	int ret = BCME_OK;
	uint16 xtlv_size = 0;
	wlc_oce_info_t *oce = (wlc_oce_info_t *)dig->cmd_ctx;
	uint16 buflen = 0;

	if (!obuf || !*olen) {
		ret = BCME_BADARG;
		goto fail;
	}

	xtlv_size = bcm_xtlv_size_for_data(sizeof(oce->oce_cmn_info->probe_defer_time),
		BCM_XTLV_OPTION_ALIGN32);
	if (xtlv_size > *olen) {
		WL_OCE_ERR(("wl%d: %s: short buffer length %d expected %u\n",
			oce->wlc->pub->unit, __FUNCTION__, (int)*olen, xtlv_size));
		*olen = 0;
		ret = BCME_BUFTOOSHORT;
		goto fail;
	}
	buflen = *olen;
	ret = bcm_pack_xtlv_entry(&obuf, &buflen, WL_OCE_XTLV_PROBE_DEF_TIME,
		sizeof(oce->oce_cmn_info->probe_defer_time),
		&oce->oce_cmn_info->probe_defer_time,
		BCM_XTLV_OPTION_ALIGN32);
	if (ret != BCME_OK) {
		WL_OCE_ERR(("wl%d: %s: packing xtlv failed\n",
			oce->wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	*olen = *olen - buflen;

fail:
	return ret;
}

static int
wlc_oce_bssload_ie_parse_fn(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_oce_info_t *oce = (wlc_oce_info_t *)ctx;

	ASSERT(ctx && data);

	if (data->ie == NULL || data->ie_len < BSS_LOAD_IE_SIZE)
		return BCME_OK;

	switch (data->ft) {
		case WLC_IEM_FC_SCAN_PRBRSP:
		case WLC_IEM_FC_SCAN_BCN:
		{
			wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;
			wlc_bss_info_t *bi = ftpparm->scan.result;

			if (data->ie) {
				(void)wlc_oce_parse_bssload_ie(oce, bi, data->ie, data->ie_len);
			}
		}
			break;
		default:
			break;
	}

	return BCME_OK;
}

static void
wlc_oce_parse_bssload_ie(wlc_oce_info_t *oce, wlc_bss_info_t *bi,
        uint8* buf, uint16 buflen)
{
	dot11_qbss_load_ie_t *data;

	ASSERT(oce && buf);

	data = (dot11_qbss_load_ie_t *)buf;

	bi->qbss_load_chan_free = (uint8)WLC_QBSS_LOAD_CHAN_FREE_MAX -
		data->channel_utilization;
}

void
oce_calc_join_pref(wlc_bsscfg_t *cfg, wlc_bss_info_t **bip, uint bss_cnt,
        join_pref_t *join_pref)
{
	uint j;

	/*
	 * Recalculate new score as sum of
	 * 40% prev score,
	 * 20% chan free from BSSLOAD IE,
	 * 20% Estimated Airtime Fraction from ESP IE,
	 * 20% Reduced WAN metrics from OCE IE
	 */
	for (j = 0; j < bss_cnt; j++) {
		join_pref[j].score = ((join_pref[j].score / 5 * 2) +
			(bip[j]->qbss_load_chan_free / 5) +
			(bip[j]->eat_frac / 5) +
			(bip[j]->rwan_links / 5));

		WL_OCE_INFO(("OCE: candidate to assoc,  chan_free %d, esp %d, rwan %d, score %d\n",
			bip[j]->qbss_load_chan_free, bip[j]->eat_frac,
			bip[j]->rwan_links, join_pref[j].score));
	}
}

#ifdef WL_OCE_AP
static int
wlc_oce_iov_set_retry_delay(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen)
{
	int ret = BCME_OK;
	uint16 len;
	uint16 type;
	wlc_oce_info_t *oce = (wlc_oce_info_t *)dig->cmd_ctx;
	wlc_oce_bsscfg_cubby_t *obc = NULL;
	const bcm_xtlv_t *ptlv = (const bcm_xtlv_t *)ibuf;
	const uint8 *data;

	if (!ibuf || !ilen) {
		return BCME_BADARG;
	}

	obc = OCE_BSSCFG_CUBBY(oce, dig->bsscfg);
	if (!OCE_BSSCFG_IS_ENABLED(obc)) {
		WL_ERROR(("wl%d.%d oce is not enable, enable first\n", oce->wlc->pub->unit,
			WLC_BSSCFG_IDX(dig->bsscfg)));
		return BCME_UNSUPPORTED;
	}

	bcm_xtlv_unpack_xtlv(ptlv, &type, &len, &data, BCM_XTLV_OPTION_ALIGN32);

	if (type != WL_OCE_XTLV_RETRY_DELAY || !len) {
		WL_ERROR(("wl%d: %s: wrong xtlv: id %d len %d\n",
			oce->wlc->pub->unit, __FUNCTION__, type, len));

		return BCME_BADARG;
	}

	obc->retry_delay = *data;

	return ret;
}

/* "wl oce enable" handler */
static int
wlc_oce_iov_get_retry_delay(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen)
{
	int ret = BCME_OK;
	uint16 xtlv_size = 0;
	wlc_oce_info_t *oce = (wlc_oce_info_t *)dig->cmd_ctx;
	wlc_oce_bsscfg_cubby_t *obc = NULL;
	uint16 buflen = 0;

	if (!obuf || !*olen) {
		ret = BCME_BADARG;
		goto fail;
	}

	obc = OCE_BSSCFG_CUBBY(oce, dig->bsscfg);
	if (!OCE_BSSCFG_IS_ENABLED(obc)) {
		WL_ERROR(("wl%d.%d oce is not enable, enable first\n", oce->wlc->pub->unit,
			WLC_BSSCFG_IDX(dig->bsscfg)));
		return BCME_UNSUPPORTED;
	}

	xtlv_size = bcm_xtlv_size_for_data(sizeof(obc->retry_delay), BCM_XTLV_OPTION_ALIGN32);
	if (xtlv_size > *olen) {
		WL_OCE_ERR(("wl%d: %s: short buffer length %d expected %u\n",
			oce->wlc->pub->unit, __FUNCTION__, (int)*olen, xtlv_size));
		*olen = 0;
		ret = BCME_BUFTOOSHORT;
		goto fail;
	}
	buflen = *olen;
	ret = bcm_pack_xtlv_entry(&obuf, &buflen, WL_OCE_XTLV_RETRY_DELAY, sizeof(obc->retry_delay),
			&obc->retry_delay, BCM_XTLV_OPTION_ALIGN32);
	if (ret != BCME_OK) {
		WL_OCE_ERR(("wl%d: %s: packing xtlv failed\n",
			oce->wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	*olen = *olen - buflen;

fail:
	return ret;
}

/** FILS discovery frame "wl oce fd_tx_period <>" handler */
static int
wlc_oce_iov_set_fd_tx_period(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen)
{
	int ret = BCME_OK;
	uint16 len;
	uint16 type;
	const bcm_xtlv_t *ptlv = (const bcm_xtlv_t *)ibuf;
	const uint8 *data;
	wlc_oce_info_t *oce = (wlc_oce_info_t *)dig->cmd_ctx;
	wlc_oce_bsscfg_cubby_t *obc = NULL;

	if (!ibuf || !ilen) {
		return BCME_BADARG;
	}

	obc = OCE_BSSCFG_CUBBY(oce, dig->bsscfg);
	if (!OCE_BSSCFG_IS_ENABLED(obc)) {
		WL_ERROR(("wl%d.%d oce is not enable, enable first\n", oce->wlc->pub->unit,
			WLC_BSSCFG_IDX(dig->bsscfg)));
		return BCME_UNSUPPORTED;
	}

	bcm_xtlv_unpack_xtlv(ptlv, &type, &len, &data, BCM_XTLV_OPTION_ALIGN32);

	if (type != WL_OCE_XTLV_FD_TX_PERIOD || !len) {
		WL_ERROR(("wl%d: %s: wrong xtlv: id %d len %d\n",
			oce->wlc->pub->unit, __FUNCTION__, type, len));

		return BCME_BADARG;
	}

	oce->oce_cmn_info->fd_tx_period = *data;

	return ret;
}

/** FILS discovery frame "wl oce fd_tx_period" handler */
static int
wlc_oce_iov_get_fd_tx_period(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen)
{
	int ret = BCME_OK;
	uint16 xtlv_size = 0;
	wlc_oce_info_t *oce = (wlc_oce_info_t *)dig->cmd_ctx;
	wlc_oce_bsscfg_cubby_t *obc = NULL;
	uint16 buflen = 0;

	if (!obuf || !*olen) {
		ret = BCME_BADARG;
		goto fail;
	}

	obc = OCE_BSSCFG_CUBBY(oce, dig->bsscfg);
	if (!OCE_BSSCFG_IS_ENABLED(obc)) {
		WL_ERROR(("wl%d.%d oce is not enable, enable first\n", oce->wlc->pub->unit,
			WLC_BSSCFG_IDX(dig->bsscfg)));
		return BCME_UNSUPPORTED;
	}
	xtlv_size = bcm_xtlv_size_for_data(sizeof(oce->oce_cmn_info->fd_tx_period),
		BCM_XTLV_OPTION_ALIGN32);
	if (xtlv_size > *olen) {
		WL_OCE_ERR(("wl%d: %s: short buffer length %d expected %u\n",
			oce->wlc->pub->unit, __FUNCTION__, (int)*olen, xtlv_size));
		*olen = 0;
		ret = BCME_BUFTOOSHORT;
		goto fail;
	}
	buflen = *olen;
	ret = bcm_pack_xtlv_entry(&obuf, &buflen, WL_OCE_XTLV_FD_TX_PERIOD,
			sizeof(oce->oce_cmn_info->fd_tx_period), &oce->oce_cmn_info->fd_tx_period,
			BCM_XTLV_OPTION_ALIGN32);
	if (ret != BCME_OK) {
		WL_OCE_ERR(("wl%d: %s: packing xtlv failed\n",
			oce->wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	*olen = *olen - buflen;

fail:
	return ret;
}

/** FILS discovery frame "wl oce fd_tx_duration <>" handler */
static int
wlc_oce_iov_set_fd_tx_duration(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen)
{
	int ret = BCME_OK;
	uint16 len;
	uint16 type;
	const bcm_xtlv_t *ptlv = (const bcm_xtlv_t *)ibuf;
	const uint8 *data;
	wlc_oce_info_t *oce = (wlc_oce_info_t *)dig->cmd_ctx;
	wlc_oce_bsscfg_cubby_t *obc = NULL;

	if (!ibuf || !ilen) {
		return BCME_BADARG;
	}

	obc = OCE_BSSCFG_CUBBY(oce, dig->bsscfg);
	if (!OCE_BSSCFG_IS_ENABLED(obc)) {
		WL_ERROR(("wl%d.%d oce is not enable, enable first\n", oce->wlc->pub->unit,
			WLC_BSSCFG_IDX(dig->bsscfg)));
		return BCME_UNSUPPORTED;
	}
	bcm_xtlv_unpack_xtlv(ptlv, &type, &len, &data, BCM_XTLV_OPTION_ALIGN32);

	if (type != WL_OCE_XTLV_FD_TX_DURATION || !len) {
		WL_ERROR(("wl%d: %s: wrong xtlv: id %d len %d\n",
			oce->wlc->pub->unit, __FUNCTION__, type, len));

		return BCME_BADARG;
	}

	oce->oce_cmn_info->fd_tx_duration = *data;

	return ret;
}

/** FILS discovery frame "wl oce fd_tx_duration" handler */
static int
wlc_oce_iov_get_fd_tx_duration(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen)
{
	int ret = BCME_OK;
	uint16 xtlv_size = 0;
	wlc_oce_info_t *oce = (wlc_oce_info_t *)dig->cmd_ctx;
	wlc_oce_bsscfg_cubby_t *obc = NULL;
	uint16 buflen = 0;

	if (!obuf || !*olen) {
		ret = BCME_BADARG;
		goto fail;
	}

	obc = OCE_BSSCFG_CUBBY(oce, dig->bsscfg);
	if (!OCE_BSSCFG_IS_ENABLED(obc)) {
		WL_ERROR(("wl%d.%d oce is not enable, enable first\n", oce->wlc->pub->unit,
			WLC_BSSCFG_IDX(dig->bsscfg)));
		return BCME_UNSUPPORTED;
	}
	xtlv_size = bcm_xtlv_size_for_data(sizeof(oce->oce_cmn_info->fd_tx_duration),
		BCM_XTLV_OPTION_ALIGN32);
	if (xtlv_size > *olen) {
		WL_OCE_ERR(("wl%d: %s: short buffer length %d expected %u\n",
			oce->wlc->pub->unit, __FUNCTION__, (int)*olen, xtlv_size));
		*olen = 0;
		ret = BCME_BUFTOOSHORT;
		goto fail;
	}
	buflen = *olen;
	ret = bcm_pack_xtlv_entry(&obuf, &buflen, WL_OCE_XTLV_FD_TX_DURATION,
		sizeof(oce->oce_cmn_info->fd_tx_duration),
		&oce->oce_cmn_info->fd_tx_duration,
		BCM_XTLV_OPTION_ALIGN32);
	if (ret != BCME_OK) {
		WL_OCE_ERR(("wl%d: %s: packing xtlv failed\n",
			oce->wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	*olen = *olen - buflen;

fail:
	return ret;
}

/* "wl oce rssi_th <>" handler */
static int
wlc_oce_iov_set_ass_rej_rssi_thd(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen)
{
	int ret = BCME_OK;
	uint16 len;
	uint16 type;
	const bcm_xtlv_t *ptlv = (const bcm_xtlv_t *)ibuf;
	const uint8 *data;
	wlc_oce_info_t *oce = (wlc_oce_info_t *)dig->cmd_ctx;
	wlc_oce_bsscfg_cubby_t *obc = NULL;

	obc = OCE_BSSCFG_CUBBY(oce, dig->bsscfg);

	if (!OCE_BSSCFG_IS_ENABLED(obc)) {
		WL_ERROR(("wl%d.%d oce is not enable, enable first\n", oce->wlc->pub->unit,
			WLC_BSSCFG_IDX(dig->bsscfg)));
		return BCME_UNSUPPORTED;
	}
	bcm_xtlv_unpack_xtlv(ptlv, &type, &len, &data, BCM_XTLV_OPTION_ALIGN32);

	if (type != WL_OCE_XTLV_RSSI_TH || !len) {
		WL_ERROR(("wl%d: %s: wrong xtlv: id %d len %d\n",
			oce->wlc->pub->unit, __FUNCTION__, type, len));

		return BCME_BADARG;
	}

	obc->ass_rej_rssi_thd = *data;
	return ret;
}

/* "wl oce rssi_th" handler */
static int
wlc_oce_iov_get_ass_rej_rssi_thd(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen)
{
	int ret = BCME_OK;
	uint16 xtlv_size = 0;
	wlc_oce_info_t *oce = (wlc_oce_info_t *)dig->cmd_ctx;
	wlc_oce_bsscfg_cubby_t *obc = NULL;
	uint16 buflen = 0;

	if (!obuf || !*olen) {
		ret = BCME_BADARG;
		goto fail;
	}

	obc = OCE_BSSCFG_CUBBY(oce, dig->bsscfg);

	if (!OCE_BSSCFG_IS_ENABLED(obc)) {
		WL_ERROR(("wl%d.%d oce is not enable, enable first\n", oce->wlc->pub->unit,
			WLC_BSSCFG_IDX(dig->bsscfg)));
		return BCME_UNSUPPORTED;
	}

	xtlv_size = bcm_xtlv_size_for_data(sizeof(obc->ass_rej_rssi_thd),
		BCM_XTLV_OPTION_ALIGN32);
	if (xtlv_size > *olen) {
		WL_OCE_ERR(("wl%d: %s: short buffer length %d expected %u\n",
			oce->wlc->pub->unit, __FUNCTION__, (int)*olen, xtlv_size));
		*olen = 0;
		ret = BCME_BUFTOOSHORT;
		goto fail;
	}

	buflen = *olen;
	ret = bcm_pack_xtlv_entry(&obuf, &buflen, WL_OCE_XTLV_RSSI_TH,
		sizeof(obc->ass_rej_rssi_thd),
		(const uint8*)&obc->ass_rej_rssi_thd,
		BCM_XTLV_OPTION_ALIGN32);
	if (ret != BCME_OK) {
		WL_OCE_ERR(("wl%d: %s: packing xtlv failed\n",
			oce->wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	*olen = *olen - buflen;

fail:
	return ret;
}

/* "wl oce redwan_links" handler */
static int
wlc_oce_iov_get_redwan_links(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen)
{
	int ret = BCME_OK;
	uint16 xtlv_size = 0;
	wlc_oce_info_t *oce = (wlc_oce_info_t *)dig->cmd_ctx;
	wlc_oce_bsscfg_cubby_t *obc = NULL;
	uint16 buflen = 0;

	if (!obuf || !*olen) {
		ret = BCME_BADARG;
		goto fail;
	}

	obc = OCE_BSSCFG_CUBBY(oce, dig->bsscfg);

	if (!OCE_BSSCFG_IS_ENABLED(obc)) {
		WL_ERROR(("wl%d.%d oce is not enable, enable first\n", oce->wlc->pub->unit,
			WLC_BSSCFG_IDX(dig->bsscfg)));
		return BCME_UNSUPPORTED;
	}
	xtlv_size = bcm_xtlv_size_for_data(sizeof(obc->red_wan_links),
		BCM_XTLV_OPTION_ALIGN32);
	if (xtlv_size > *olen) {
		WL_OCE_ERR(("wl%d: %s: short buffer length %d expected %u\n",
			oce->wlc->pub->unit, __FUNCTION__, (int)*olen, xtlv_size));
		*olen = 0;
		ret = BCME_BUFTOOSHORT;
		goto fail;
	}

	buflen = *olen;
	ret = bcm_pack_xtlv_entry(&obuf, &buflen, WL_OCE_XTLV_RWAN_LINKS,
			sizeof(obc->red_wan_links),
			(const uint8*)&obc->red_wan_links,
			BCM_XTLV_OPTION_ALIGN32);
	if (ret != BCME_OK) {
		WL_OCE_ERR(("wl%d: %s: packing xtlv failed\n",
			oce->wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	*olen = *olen - buflen;

fail:
	return ret;
}

/* "wl oce redwan_links <>" handler */
static int
wlc_oce_iov_set_redwan_links(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen)
{
	int ret = BCME_OK;
	uint16 len;
	uint16 type;
	const bcm_xtlv_t *ptlv = (const bcm_xtlv_t *)ibuf;
	const uint8 *data;
	wlc_oce_info_t *oce = (wlc_oce_info_t *)dig->cmd_ctx;
	wlc_oce_bsscfg_cubby_t *obc = NULL;

	bcm_xtlv_unpack_xtlv(ptlv, &type, &len, &data, BCM_XTLV_OPTION_ALIGN32);

	obc = OCE_BSSCFG_CUBBY(oce, dig->bsscfg);

	if (!OCE_BSSCFG_IS_ENABLED(obc)) {
		WL_ERROR(("wl%d.%d oce is not enable, enable first\n", oce->wlc->pub->unit,
			WLC_BSSCFG_IDX(dig->bsscfg)));
		return BCME_UNSUPPORTED;
	}

	if (type != WL_OCE_XTLV_RWAN_LINKS || !len) {
		WL_ERROR(("wl%d: %s: wrong xtlv: id %d len %d\n",
			oce->wlc->pub->unit, __FUNCTION__, type, len));

		return BCME_BADARG;
	}

	obc->red_wan_links = *data;

	wlc_update_beacon(oce->wlc);
	wlc_update_probe_resp(oce->wlc, FALSE);

	return ret;
}

#ifdef WLWNM
/* "wl oce cu_trigger" handler */
static int
wlc_oce_iov_get_cu_trigger(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen)
{
	int ret = BCME_OK;
	uint16 xtlv_size = 0;
	wlc_oce_info_t *oce = (wlc_oce_info_t *)dig->cmd_ctx;
	uint16 buflen = 0;
	wlc_bsscfg_t *bsscfg;
	wlc_info_t *wlc;
	int i;
	uint8 roam_cu_trigger = 0; /* channel utilization roam trigger (%) */

	if (!obuf || !*olen) {
		ret = BCME_BADARG;
		goto fail;
	}

	wlc = oce->wlc;

	FOREACH_BSS(wlc, i, bsscfg) {
		if (BSSCFG_INFRA_STA(bsscfg) &&
			WLC_BSS_CONNECTED(bsscfg)) {
				roam_cu_trigger =
					wlc_wnm_get_cu_trigger_percent(wlc, bsscfg);
		}
	}

	if (!roam_cu_trigger) {
		WL_OCE_ERR(("wl%d: %s: bss not found\n",
			oce->wlc->pub->unit, __FUNCTION__));
		*olen = 0;
		ret = BCME_NOTFOUND;
		goto fail;
	}

	xtlv_size = bcm_xtlv_size_for_data(sizeof(roam_cu_trigger),
		BCM_XTLV_OPTION_ALIGN32);
	if (xtlv_size > *olen) {
		WL_OCE_ERR(("wl%d: %s: short buffer length %d expected %u\n",
			oce->wlc->pub->unit, __FUNCTION__, (int)*olen, xtlv_size));
		*olen = 0;
		ret = BCME_BUFTOOSHORT;
		goto fail;
	}

	buflen = *olen;
	ret = bcm_pack_xtlv_entry(&obuf, &buflen, WL_OCE_XTLV_CU_TRIGGER,
			sizeof(roam_cu_trigger),
			(const uint8*)&roam_cu_trigger,
			BCM_XTLV_OPTION_ALIGN32);
	if (ret != BCME_OK) {
		WL_OCE_ERR(("wl%d: %s: packing xtlv failed\n",
			oce->wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	*olen = *olen - buflen;

fail:
	return ret;
}

/* "wl oce cu_trigger <>" handler */
static int
wlc_oce_iov_set_cu_trigger(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen)
{
	int ret = BCME_OK;
	uint16 len;
	uint16 type;
	const bcm_xtlv_t *ptlv = (const bcm_xtlv_t *)ibuf;
	const uint8 *data;
	wlc_oce_info_t *oce = (wlc_oce_info_t *)dig->cmd_ctx;
	wlc_bsscfg_t *bsscfg;
	wlc_info_t *wlc;
	int i;

	bcm_xtlv_unpack_xtlv(ptlv, &type, &len, &data, BCM_XTLV_OPTION_ALIGN32);

	if (type != WL_OCE_XTLV_CU_TRIGGER || !len) {
		WL_ERROR(("wl%d: %s: wrong xtlv: id %d len %d\n",
			oce->wlc->pub->unit, __FUNCTION__, type, len));

		return BCME_BADARG;
	}

	wlc = oce->wlc;

	FOREACH_BSS(wlc, i, bsscfg) {
		if (BSSCFG_INFRA_STA(bsscfg) &&
			WLC_BSS_CONNECTED(bsscfg)) {
				wlc_wnm_set_cu_trigger_percent(wlc, bsscfg, *data);
		}
	}

	return ret;
}
#endif /* WLWNM */

static void
wlc_oce_set_bcn_rate(wlc_oce_info_t *oce)
{
	wlc_bsscfg_t *cfg;
	wlc_oce_bsscfg_cubby_t *obc = NULL;
	uint8 i;
	bool oce_ap_present = FALSE;

	/* check for any OCE enable bsscfg */
	FOREACH_UP_AP(oce->wlc, i, cfg) {
		obc = OCE_BSSCFG_CUBBY(oce, cfg);
		if (OCE_BSSCFG_IS_ENABLED(obc)) {
			oce_ap_present = TRUE;
			break;
		}
	}

	if (oce_ap_present && BAND_2G(oce->wlc->band->bandtype) &&
	    wlc_ap_forced_bcn_rspec_get(oce->wlc) == 0) {
		/* OCE demands min beacon transmit rate to be 5.5 Mbps OOB (WFA OCE spec v5 3.10) */
		wlc_ap_oce_forced_bcn_rspec_set(oce->wlc, LEGACY_RSPEC(WLC_RATE_5M5));
	} else {
		/* reset beacon rate */
		wlc_ap_oce_forced_bcn_rspec_set(oce->wlc, 0);
	}
}

int
wlc_oce_update_control_field(wlc_oce_info_t *oce, uint8 option)
{
	if (!OCE_ENAB(oce->wlc->pub)) {
		WL_INFORM(("wl%d: %s: OCE is not enabled, cannot update control field\n",
			oce->wlc->pub->unit, __FUNCTION__));
		return BCME_ERROR;
	}
	oce->oce_cmn_info->control_field |= option;
	return BCME_OK;
}
#endif /* WL_OCE_AP */

#if defined(BCMDBG)
static int
wlc_oce_dump(void *ctx, struct bcmstrbuf *b)
{
	wlc_oce_info_t *oce = (wlc_oce_info_t *)ctx;

	bcm_bprintf(b, "OCE: %d \n", OCE_ENAB(oce->wlc->pub));
	bcm_bprintf(b, "     up_tick %u sys_up_time:%u\n", oce->up_tick, OSL_SYSUPTIME());
	bcm_bprintf(b, "OCE AP :\n");
	bcm_bprintf(b, "     FD frame tx period	%d \n", oce->oce_cmn_info->fd_tx_period);
	bcm_bprintf(b, "     FD frame tx duration %d \n", oce->oce_cmn_info->fd_tx_duration);
	bcm_bprintf(b, "     FD frame tx count %d \n", oce->oce_cmn_info->fd_frame_count);
	bcm_bprintf(b, "     rssi delta %d \n", oce->oce_cmn_info->rssi_delta);
	bcm_bprintf(b, "     RNR scan period	%d \n", oce->oce_cmn_info->rnr_scan_period);
	bcm_bprintf(b, "OCE STA :\n");
	bcm_bprintf(b, "     probe req deferral time %d \n",
		oce->oce_cmn_info->probe_defer_time);
	bcm_bprintf(b, "     probe resp listen time %d \n", oce->oce_cmn_info->max_channel_time);

	return BCME_OK;
}
#endif // endif

/** FILS discovery */
uint8
wlc_oce_get_apcfg_idx_used_for_fd_frame(wlc_info_t *wlc)
{
	wlc_oce_info_t *oce = NULL;
	oce = wlc->oce;

	return oce->oce_cmn_info->apcfg_idx_xmit_fd;
}

void
wlc_oce_set_short_ssid(wlc_info_t *wlc, wlc_bsscfg_t *apcfg)
{
	wlc_oce_info_t *oce = wlc->oce;
	uint32 short_ssid = 0;

	short_ssid = wlc_calc_short_ssid(apcfg->SSID, apcfg->SSID_len);
	htol32_ua_store(short_ssid, &oce->oce_cmn_info->short_ssid);
}

/** FILS discovery */
static int
wlc_oce_set_fd_cap(wlc_oce_info_t *oce)
{
	uint16 cap = 0, chan_width = 0, phy_index = 0, nss = 0, bw = 0;
	/* check if it needs to be change, refer: IEEE P802.11ax/D4.3, August 2019
	 * Table: Table 9-385?FILS Minimum Rate
	 */
	uint16 fils_minimum_rate = FD_CAP_FILS_MIN_RATE_3;
	uint8 rxchain = 0;

	oce->up_tick = OSL_SYSUPTIME();

	if (!OCE_ENAB(oce->wlc->pub)) {
		return BCME_UNSUPPORTED;
	}

	/* FILS Discovery capability subfield format:
	-------------------------------------------------------------------------------------------
	B0
	|ESS   | Privacy | BSS operating chan | Max spatial  | Reserved |Multiple  | Phy   | Fils |
	|      |         | width              | stream       |          | BSSID    | index | Min. |
	|      |         |                    |              |          | presence |       | rate |
	| 1 bit| 1 bit   | 3 bit              | 3 bit        |  1 bit   |  1 bit   | 3 bit | 3 bit|
	-------------------------------------------------------------------------------------------
	*/
	FD_CAP_SET_ESS(cap);

	/* 802.11-2016: 9.4.1.4
	 * if data confidentiality is required for all data frames
	 * exchanged within BSS, set privacy bit 1
	 */
	bw = CHSPEC_BW(oce->wlc->home_chanspec);
	if ((bw == WL_CHANSPEC_BW_160) || (bw == WL_CHANSPEC_BW_8080)) {
		chan_width = FD_CAP_BSS_CH_WIDTH_160_80p80_MHZ;
	} else if (bw == WL_CHANSPEC_BW_80) {
		chan_width = FD_CAP_BSS_CH_WIDTH_80_MHZ;
	} else if (bw == WL_CHANSPEC_BW_40) {
		chan_width = FD_CAP_BSS_CH_WIDTH_40_MHZ;
	} else {
		chan_width = FD_CAP_BSS_CH_WIDTH_20_22_MHZ;
	}
	chan_width <<= FD_CAP_BSS_CH_WIDTH_BIT_SHIFT;
	cap |= chan_width;

	if (oce->wlc->stf) {
		rxchain = oce->wlc->stf->hw_rxchain;
	}
	nss = WLC_BITSCNT(rxchain);
	nss <<= FD_CAP_MAX_NSS_BIT_SHIFT;
	cap |= nss;

	if (WLC_HE_CAP_PHY(oce->wlc)) {
		/* 11ax capable */
		phy_index = FD_CAP_PHY_INDEX_HE;
		fils_minimum_rate = FD_CAP_FILS_MIN_RATE_4;
	} else if (VHT_ENAB(oce->wlc->pub)) {
		/* 11ac capable */
		phy_index = FD_CAP_PHY_INDEX_VHT;
	} else if (N_ENAB(oce->wlc->pub)) {
		/* 11b/g capable */
		phy_index = FD_CAP_PHY_INDEX_HT;
	} else {
		phy_index = FD_CAP_PHY_INDEX_ERP_OFDM;
	}
	phy_index <<= FD_CAP_PHY_INDEX_BIT_SHIFT;
	cap |= phy_index;

	fils_minimum_rate <<= FD_CAP_FILS_MIN_RATE_BIT_SHIFT;
	cap |= fils_minimum_rate;

	htol16_ua_store(cap, &oce->oce_cmn_info->fd_cap);
	return BCME_OK;
}

bool
wlc_oce_bsscfg_is_enabled(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	wlc_oce_info_t *oce = wlc->oce;
	wlc_oce_bsscfg_cubby_t *obc;

	if (!oce) {
		return FALSE;
	}

	obc = OCE_BSSCFG_CUBBY(oce, cfg);

	return (obc ? (OCE_BSSCFG_IS_ENABLED(oce)): FALSE);
}
