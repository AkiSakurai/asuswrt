/*
 * WBD Appeventd include file
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
 * $Id: wbd_appeventd.h 777269 2019-07-25 11:35:49Z $
 */

#ifndef _WBD_APPEVENTD_H_
#define _WBD_APPEVENTD_H_

#include "appeventd.h"
#include "appeventd_wbd.h"

/*
 * Routine to send  MAP init Start/End event from wbd to appeventd.
 * Params:
 * @evt: Event id
 * @device_id: AL MAC address of Controller device.
 * @status: Start = 1, end = 2.
 * @app_id: Master =1, Slave = 2.
 */
extern void wbd_appeventd_map_init(int evt, struct ether_addr *device_id, map_init_status_t status,
	map_apptype_t app_id);

/*
 * Routine to send  weak sta event from wbd to appeventd.
 * Params:
 * @evt: Event id
 * @ifname: Interface name.
 * @sta_mac: Sta mac address.
 * @rssi: Sta rssi value.
 * @tx_failures: Tx fail count.
 * @tx_rate: Tx rate.
 */
extern void wbd_appeventd_weak_sta(int evt, char *ifname, struct ether_addr *sta_mac,
	int rssi, uint32 tx_failures, float tx_rate);

/*
 * Routine to send steer start event from wbd to appeventd.
 * Params:
 * @evt: Event id
 * @ifname: Interface name.
 * @sta_mac: Sta mac address.
 * @dst_mac: Destination slave's bssid.
 * @src_rssi: Sta rssi at source AP.
 * @dst_rssi: Sta rssi at target AP.
 */
extern void wbd_appeventd_steer_start(int evt, char *ifname, struct ether_addr *sta_mac,
	struct ether_addr *dst_mac, int src_ssi, int dst_rssi);

/*
 * Routine to send steer response event from wbd to appeventd.
 * Params:
 * @evt: Event id
 * @ifname: Interface name.
 * @sta_mac: Sta mac address.
 * @resp: Steer response.
 */
extern void wbd_appeventd_steer_resp(int evt, char *ifname, struct ether_addr *sta_mac, int resp);

/*
 * Routine to send steer complete event from wbd to appeventd.
 * Params:
 * @evt: Event id
 * @ifname: Interface name.
 * @sta_mac: Sta mac address.
 * @src_mac: Source slave bssid.
 * @dst_mac: Target slave bssid.
 * @rssi: Sta rssi.
 * @tx_rate: Sta tx-rate.
 */
extern void wbd_appeventd_steer_complete(int evt, char *ifname, struct ether_addr *sta_mac,
	struct ether_addr *src_mac, struct ether_addr *dst_mac, int rssi, float tx_rate);
#endif /* _WBD_APPEVENTD_H_ */
