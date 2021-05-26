/**
 * @file
 * @brief
 * scan related wrapper routines and scan results related functions
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_scan_utils.h 786933 2020-05-12 06:33:07Z $
 */

#ifndef _wlc_scan_utils_h_
#define _wlc_scan_utils_h_

#include <typedefs.h>
#include <ethernet.h>
#include <802.11.h>
#include <d11.h>
#include <wlc_cfg.h>
#include <wlc_types.h>
#include <wlc_scan.h>

/* For managing scan result lists */
struct wlc_bss_list {
	uint		count;
	wlc_bss_info_t*	ptrs[MAXBSS];
};

typedef struct scan_utl_scan_data {
	wlc_d11rxhdr_t *wrxh;
	uint8 *plcp;
	struct dot11_management_header *hdr;
	uint8 *body;
	int body_len;
	wlc_bss_info_t *bi;
} scan_utl_scan_data_t;

/* ignore list */
typedef struct iscan_ignore {
	uint8 bssid[ETHER_ADDR_LEN];
	uint16 ssid_sum;
	uint16 ssid_len;
	chanspec_t chanspec;	/* chan portion is chan on which bcn_prb was rx'd */
} iscan_ignore_t;

/* module info */
struct wlc_scan_utils {
	wlc_info_t *wlc;
	/** An unique request ID is associated to correlate request/response-set */
	uint16	escan_sync_id;
	int32	scanresults_minrssi;		/* RSSI threshold under which beacon/probe responses
						 * are tossed due to weak signal
						 */
	wlc_bss_list_t	*custom_scan_results;	/**< results from ioctl scan */
	uint	custom_scan_results_state;	/**< see WL_SCAN_RESULTS_* states in wlioctl.h */

	/* ISCAN */
	uint	custom_iscan_results_state;	/**< see WL_SCAN_RESULTS_* states in wlioctl.h */
	struct wl_timer *iscan_timer;		/**< stops iscan after iscan_duration ms */
	iscan_ignore_t	*iscan_ignore_list;	/**< networks to ignore on subsequent iscans */
	uint	iscan_ignore_last;		/* iscan_ignore_list count from prev partial scan */
	uint	iscan_ignore_count;		/**< cur number of elements in iscan_ignore_list */
	chanspec_t iscan_chanspec_last;		/**< resume chan after prev partial scan */
	uint	iscan_result_last;		/**< scanresult index in last iscanresults */
	bcm_notif_h  scan_data_h; /* scan_data notifier handle. */
	bcm_notif_h  scan_start_h; /* scan_start notifier handle. */
};

typedef void (*scan_utl_rx_scan_data_fn_t)(void *ctx, scan_utl_scan_data_t *data);

int wlc_scan_request(wlc_info_t *wlc, int bss_type, const struct ether_addr* bssid,
	int nssid, wlc_ssid_t *ssids, int scan_type, int nprobes,
	int active_time, int passive_time, int home_time,
	const chanspec_t* chanspec_list, int chanspec_num,
	chanspec_t chanspec_start, bool save_prb,
	scancb_fn_t fn, void* arg,
	int macreq, uint scan_flags, wlc_bsscfg_t *cfg,
	actcb_fn_t act_cb, void *act_cb_arg);

int wlc_custom_scan(wlc_info_t *wlc, char *arg, int arg_len,
	chanspec_t chanspec_start, int macreq, wlc_bsscfg_t *cfg);
void wlc_custom_scan_complete(void *arg, int status, wlc_bsscfg_t *cfg);

int wlc_recv_scan_parse(wlc_info_t *wlc, wlc_d11rxhdr_t *wrxh, uint8 *plcp,
	struct dot11_management_header *hdr, uint8 *body, int body_len);
int wlc_recv_scan_parse_bcn_prb(wlc_info_t *wlc, wlc_d11rxhdr_t *wrxh,
	struct ether_addr *bssid, bool beacon,
	uint8 *body, uint body_len, wlc_bss_info_t *bi);
#if BAND6G
int wlc_scan_6g_handle_fils_disc(wlc_info_t *wlc, wlc_bss_info_t *bi, uint32 short_ssid);
#endif /* BAND6G */

#define ISCAN_IN_PROGRESS(wlc)  wlc_scan_utils_iscan_inprog(wlc)
bool wlc_scan_utils_iscan_inprog(wlc_info_t *wlc);

void wlc_scan_utils_set_chanspec(wlc_info_t *wlc, chanspec_t chanspec);
void wlc_scan_utils_set_syncid(wlc_info_t *wlc, uint16 syncid);

int wlc_scan_utils_rx_scan_register(wlc_info_t *wlc, scan_utl_rx_scan_data_fn_t fn, void *arg);
int wlc_scan_utils_rx_scan_unregister(wlc_info_t *wlc, scan_utl_rx_scan_data_fn_t fn, void *arg);

#ifdef WL_SHIF
int wlc_scan_utils_shubscan_request(wlc_info_t *wlc);
#endif // endif

/* attach/detach */
wlc_scan_utils_t *wlc_scan_utils_attach(wlc_info_t *wlc);
void wlc_scan_utils_detach(wlc_scan_utils_t *sui);

#endif /* _wlc_scan_utils_h_ */
