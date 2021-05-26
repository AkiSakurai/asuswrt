/*
 * WBD Appeventd routine handler file
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
 * $Id: wbd_appeventd.c 777334 2019-07-29 08:58:01Z $
 */

#include "wbd.h"
#include "bcm_steering.h"
#include "appeventd_wbd.h"

#define WBD_APPEVENT_BUFSIZE	1024

/* Copy device_id, init status, into buf and send as event data. */
void
wbd_appeventd_map_init(int evt, struct ether_addr *device_id, map_init_status_t status,
	map_apptype_t app_id)
{
	unsigned char app_data[WBD_APPEVENT_BUFSIZE] = {0};
	app_event_wbd_map_init_t *map_init = NULL;

	/* Prepare event data. */
	map_init = (app_event_wbd_map_init_t*)app_data;
	eacopy(device_id, &(map_init->device_id));
	map_init->status = status; /* Start = 1, end = 2 */
	map_init->app_id = app_id; /* Master = 1, Slave = 2 */

	app_event_sendup(evt, APP_E_WBD_STATUS_SUCCESS,
		app_data, sizeof(*map_init));
}

/* Copy ifname, sta_mac, rssi, tx_failures, and tx_rate into buf and send as event data. */
void
wbd_appeventd_weak_sta(int evt, char *ifname, struct ether_addr *sta_mac, int rssi,
	uint32 tx_failures, float tx_rate)
{
	unsigned char app_data[WBD_APPEVENT_BUFSIZE] = {0};
	app_event_wbd_weak_sta_t *weak_sta = NULL;

	/* Prepare event data. */
	weak_sta = (app_event_wbd_weak_sta_t*)app_data;
	WBDSTRNCPY(weak_sta->ifname, ifname, sizeof(weak_sta->ifname));
	eacopy(sta_mac, &(weak_sta->sta_addr));
	weak_sta->rssi = rssi;
	weak_sta->tx_failures = tx_failures;
	weak_sta->tx_rate = tx_rate;

	app_event_sendup(evt, APP_E_WBD_STATUS_SUCCESS,
		app_data, sizeof(*weak_sta));
}

/* Copy ifname, sta_mac, dst_mac and rssi into buf and send as event data. */
void
wbd_appeventd_steer_start(int evt, char *ifname, struct ether_addr *sta_mac,
	struct ether_addr *dst_mac, int src_rssi, int dst_rssi)
{
	unsigned char app_data[WBD_APPEVENT_BUFSIZE] = {0};
	app_event_wbd_steer_sta_t *steer_sta = NULL;

	/* Prepare event data. */
	steer_sta = (app_event_wbd_steer_sta_t*)app_data;
	WBDSTRNCPY(steer_sta->ifname, ifname, sizeof(steer_sta->ifname));
	eacopy(sta_mac, &(steer_sta->sta_addr));
	eacopy(dst_mac, &(steer_sta->dst_addr));
	steer_sta->src_rssi = src_rssi;
	steer_sta->dst_rssi = dst_rssi;

	app_event_sendup(evt, APP_E_WBD_STATUS_SUCCESS,
		app_data, sizeof(*steer_sta));
}

/* copy ifname, sta_mac and resp into buf and send as event data. */
void
wbd_appeventd_steer_resp(int evt, char *ifname, struct ether_addr *sta_mac, int resp)
{
	unsigned char app_data[WBD_APPEVENT_BUFSIZE] = {0};
	app_event_wbd_steer_resp_t *steer_resp = NULL;
	char *resp_str = WBD_STEER_RESPONSE_ACCEPT;

	if (resp == WLIFU_BSS_TRANS_RESP_REJECT) {
		resp_str = WBD_STEER_RESPONSE_REJECT;
	} else if (resp == WLIFU_BSS_TRANS_RESP_UNKNOWN) {
		resp_str = WBD_STEER_RESPONSE_UNKNOWN;
	}

	/* Prepare event data. */
	steer_resp = (app_event_wbd_steer_resp_t*)app_data;
	WBDSTRNCPY(steer_resp->ifname, ifname, sizeof(steer_resp->ifname));
	eacopy(sta_mac, &(steer_resp->sta_addr));
	steer_resp->resp_code = resp;
	WBDSTRNCPY(steer_resp->resp, resp_str, sizeof(steer_resp->resp));

	app_event_sendup(evt, APP_E_WBD_STATUS_SUCCESS,
		app_data, sizeof(*steer_resp));
}

/* Copy ifname, sta_mac, src_mac and dst_mac into buf and send as event data. */
void
wbd_appeventd_steer_complete(int evt, char *ifname, struct ether_addr *sta_mac,
	struct ether_addr *src_mac, struct ether_addr *dst_mac, int rssi, float tx_rate)
{
	unsigned char app_data[WBD_APPEVENT_BUFSIZE] = {0};
	app_event_wbd_steer_complete_t *steer_complete = NULL;

	/* Prepare event data. */
	steer_complete = (app_event_wbd_steer_complete_t*)app_data;
	WBDSTRNCPY(steer_complete->ifname, ifname, sizeof(steer_complete->ifname));
	eacopy(sta_mac, &(steer_complete->sta_addr));
	eacopy(src_mac, &(steer_complete->src_addr));
	eacopy(dst_mac, &(steer_complete->dst_addr));
	eacopy(sta_mac, &(steer_complete->sta_stats.sta_addr));
	steer_complete->sta_stats.rssi = rssi;
	steer_complete->sta_stats.tx_rate = tx_rate;

	app_event_sendup(evt, APP_E_WBD_STATUS_SUCCESS,
		app_data, sizeof(*steer_complete));
}
