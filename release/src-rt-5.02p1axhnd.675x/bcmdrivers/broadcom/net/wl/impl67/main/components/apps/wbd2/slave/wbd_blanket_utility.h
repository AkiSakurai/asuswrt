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
 * $Id: wbd_blanket_utility.h 791063 2020-09-15 07:32:51Z $
 */

#ifndef _WBD_BLANKET_UTILITY_H_
#define _WBD_BLANKET_UTILITY_H_

#include "wbd.h"

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
 * @param reason	assoc/disassoc status/Reason code
 * @return		status of the call. 0 Success. Non Zero Failure
 */
int blanket_sta_assoc_disassoc(struct ether_addr *bssid, struct ether_addr *mac, int isAssoc,
	unsigned short time_elapsed, unsigned char notify, unsigned char *assoc_frame,
	unsigned int assoc_frame_len, uint16 reason);

#if defined(MULTIAPR2)
/** STA has assoc Failed connection
 * @param mac		MAC address of the STA
 * @param status	Failed connection association status code
 */
int
blanket_sta_assoc_failed_connection(struct ether_addr *mac, uint16 status);
#endif /* MULTIAPR2 */

/** @brief Add All STA's Currently Associated with the BSS
 *
 * @param ifname	interface name of the BSS
 * @param bssid		BSSID of the BSS on which STA  associated
 *
 * @return		status of the call. 0 Success. Non Zero Failure
 */
int blanket_add_all_associated_stas(char *ifname, struct ether_addr *bssid);

/** @brief Check if a STA is in assoclist of the BSS or not. Add if present
 *
 * @param ifname	interface name of the BSS
 * @param bssid		BSSID of the BSS on which STA  associated
 * @param sta_mac	MAC address of the STA
 *
 * @return		status of the call. 0 Success. Non Zero Failure
 */
int blanket_check_and_add_sta_from_assoclist(char *ifname, struct ether_addr *bssid,
	struct ether_addr *sta_mac);

/* Get Backhaul Associated STA metrics */
int wbd_slave_get_backhaul_assoc_sta_metric(char *ifname, i5_dm_clients_type *i5_sta);

/* Block or unblock backhaul STA assocaition. Incase of block, disassociate all backhaul STAs on
 * the self device
 */
extern void wbd_slave_block_unblock_backhaul_sta_assoc(int unblock);

/* We need to disable or enabled the roaming. If the i5_ifr is provided, it is for disable the
 * roaming, then disable in all the other STA interfaces except i5_ifr
 */
void wbd_ieee1905_set_bh_sta_params(t_i5_bh_sta_cmd cmd, i5_dm_interface_type *i5_ifr);

/* Get the interface metric like channel utilization and noise ect.. */
extern int wbd_blanket_get_interface_metric(char *ifname, ieee1905_interface_metric *metric);

#if defined(MULTIAPR2)
/* Get backhaul STA profile of the connected device from sta_info. The ifanme can be WDS ifname
 * in case of upstream AP or backhaul STA interface name
 */
unsigned char wbd_blanket_get_bh_sta_profile(char *ifname);

/* set or unset association disallowed attribute in beacon */
int wbd_blanket_mbo_assoc_disallowed(char *ifname, unsigned char reason);

/* Set DFS channel clear, as indicated by the controller */
extern void wbd_blanket_set_dfs_chan_clear(char *ifname, ieee1905_chan_pref_rc_map *chan_pref);
#endif /* MULTIAPR2 */

/* Check if any of the STA interface is connected to upstream AP */
i5_dm_interface_type *wbd_slave_is_any_bsta_associated(bool update_map_flags, bool get_bssid);

/* Set all backhaul STA credentials by extracting it from backhaul BSS.
 * This is helpful if the onboarding was over the Ethernet
 */
void wbd_set_bh_sta_cred_from_bh_bss(ieee1905_client_bssinfo_type *bss);

/* Set roaming state. If RSSI based roaming, set roam trigger. state values allowed
 * 0 which is all roaming allowed(WLC_ROAM_STATE_ENABLED)
 * 1 Roaming disabled(WLC_ROAM_STATE_DISABLED)
 * 2 low RSSI roaming(WLC_ROAM_STATE_LOW_RSSI)
 */
void wbd_slave_set_bh_sta_roam_state(int state);

/* Disconnect all the backhaul STA interfaces except i5_ifr */
void wbd_slave_disconnect_all_bstas(i5_dm_interface_type *i5_ifr);
#endif /* _WBD_BLANKET_UTILITY_H_ */
