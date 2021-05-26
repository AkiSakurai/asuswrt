/*
 * WBD WL utility for both Master and Slave (Linux)
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
 * $Id: wbd_wl_utility.h 769533 2018-11-19 10:53:16Z $
 */

#ifndef _WBD_WL_UTILITY_H_
#define _WBD_WL_UTILITY_H_

#include "wbd.h"

/* Fetch Interface Info for AP */
extern int wbd_wl_fill_interface_info(wbd_wl_interface_t *ifr_info);

/* Send assoc decision */
extern int wbd_wl_send_assoc_decision(char *ifname, uint32 info_flags, uint8 *data, int len,
	struct ether_addr *sta_mac, uint32 evt_type);

/* Send Action frame to STA, Measure rssi from the response */
extern int wbd_wl_actframe_to_sta(char *ifname, struct ether_addr* ea);

/* Get dfs pref chan list from driver */
extern int wbd_wl_get_dfs_forced_chspec(char* ifname, wl_dfs_forced_t *smbuf, int size);

/* Set dfs pref chan list */
extern int wbd_wl_set_dfs_forced_chspec(char* ifname, wl_dfs_forced_t* dfs_frcd,
	int ioctl_size);

/* Send beacon request to sta */
extern int wbd_wl_send_beacon_request(char *ifname, wlc_ssid_t *ssid,
	struct ether_addr *bssid, struct ether_addr *sta_mac,
	uint8 channel, int *delay);

/* Find ie for given id and type */
extern bcm_tlv_t *wbd_wl_find_ie(uint8 id, void *tlvs, int tlvs_len, const char *voui, uint8 *type,
	int type_len);

/* Get channel availablity in percentage */
extern int wbd_wl_get_link_availability(char *ifname, unsigned short *link_available);
#endif /* _WBD_WL_UTILITY_H_ */
