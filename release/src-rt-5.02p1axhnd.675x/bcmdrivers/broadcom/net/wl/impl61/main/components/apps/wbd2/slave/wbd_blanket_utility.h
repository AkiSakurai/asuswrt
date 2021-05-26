/*
 * WBD Blanket utility for Slave (Linux)
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
 * $Id: wbd_blanket_utility.h 781736 2019-11-27 09:57:31Z $
 */

#ifndef _WBD_BLANKET_UTILITY_H_
#define _WBD_BLANKET_UTILITY_H_

#include "wbd.h"

#ifdef WLHOSTFBT
/* Check whether FBT is enabled or not */
#define WBD_FBT_ENAB(flags) ((flags) & (WBD_FLAGS_BSS_FBT_ENABLED))
#else
#define WBD_FBT_ENAB(flags) 0
#endif /* WLHOSTFBT */

/* Get STA link metrics */
int wbd_slave_process_get_assoc_sta_metric(char *ifname, unsigned char *bssid,
	unsigned char *sta_mac, ieee1905_sta_link_metric *metric,
	ieee1905_sta_traffic_stats *traffic_stats, ieee1905_vendor_data *out_vndr_tlv);

/* Update BSS capability for all BSS in a interface */
void wbd_slave_update_bss_capability(i5_dm_interface_type *i5_ifr);

/** @brief STA has Associated or Disassociated on this device
 *
 * @param bssid		BSSID of the BSS on which STA  associated or disassociated
 * @param mac		MAC address of the STA
 * @param isAssoc	1 if STA Associated, 0 if STA Disassociated
 * @param time_elapsed	Seconds since assoc
 * @param notify	Notify immediately to neighbors
 * @param assoc_frame	Frame body of the (Re)Association request frame
 * @param assoc_frame_len	Length of the (Re)Association Request frame length
 *
 * @return		status of the call. 0 Success. Non Zero Failure
 */
int blanket_sta_assoc_disassoc(struct ether_addr *bssid, struct ether_addr *mac, int isAssoc,
	unsigned short time_elapsed, unsigned char notify, unsigned char *assoc_frame,
	unsigned int assoc_frame_len);

/** @brief Add All STA's Currently Associated with the BSS
 *
 * @param ifname	interface name of the BSS
 * @param bssid		BSSID of the BSS on which STA  associated
 *
 * @return		status of the call. 0 Success. Non Zero Failure
 */
int blanket_add_all_associated_stas(char *ifname, struct ether_addr *bssid);

/* Get Backhaul Associated STA metrics */
int wbd_slave_get_backhaul_assoc_sta_metric(char *ifname, i5_dm_clients_type *i5_sta);

/* Block or unblock backhaul STA assocaition. Incase of block, disassociate all backhaul STAs on
 * the self device
 */
extern void wbd_slave_block_unblock_backhaul_sta_assoc(int unblock);
#endif /* _WBD_BLANKET_UTILITY_H_ */
