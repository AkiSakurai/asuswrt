/*
 * WBD Weak Client identification Policy declarations
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
 * $Id: wbd_slave_control.h 618311 2016-02-10 15:06:06Z $
 */

#ifndef _WBD_SLAVE_CONTROL_H_
#define _WBD_SLAVE_CONTROL_H_

#include "ieee1905.h"
#include "wbd.h"
#include "wbd_shared.h"
#include "wbd_ds.h"

/* Structure to hold params to check if the backhaul steered to target bssid */
typedef struct wbd_bh_steer_check_arg {
	ieee1905_backhaul_steer_msg *bh_steer_msg;
	char ifname[IFNAMSIZ];
	char prefix[IFNAMSIZ];
	int check_cnt;
} wbd_bh_steer_check_arg_t;

/* Flags to be used in wbd_slave_steer_resp_cb_data_t structure */
#define WBD_STEER_RESP_CB_FLAGS_RETRY	0x01	/* Steer retry is happening */

/* Structure to be sent back to slave from steering library upon processing the BTM response */
typedef struct wbd_slave_steer_resp_cb_data {
	uint8 flags;				/* Flags of type WBD_STEER_RESP_CB_FLAGS_XXX */
	unsigned short request_flags;		/* IEEE1905_STEER_FLAGS_XXX values */
	struct ether_addr req_al_mac;		/* ALMAC from where the STEER request came */
	wl_wlif_bss_trans_data_t ioctl_data;	/* Steering request data */
} wbd_slave_steer_resp_cb_data_t;

/* Get the number of threshold policies defined */
extern int wbd_get_max_wc_thld();

/* Get the deciding weak client algorithms */
extern int wbd_get_max_wc_algo();

/* Get the threshold policy based on index */
extern wbd_wc_thld_t *wbd_get_wc_thld(wbd_slave_item_t *slave_item);

/* Create the timer to check for weak clients & send WEAK_CLIENT command to Master, if any */
extern int wbd_slave_create_identify_weak_client_timer(wbd_slave_item_t *slave_item);

/* Remove timer to check for weak clients */
extern int wbd_slave_remove_identify_weak_client_timer(wbd_slave_item_t *slave_item);

/* Callback from IEEE 1905 module when the agent gets STEER request */
extern void wbd_slave_process_map_steer_request_cb(wbd_info_t *info, ieee1905_steer_req *steer_req,
	ieee1905_vendor_data *in_vndr_tlv);

/* Callback from IEEE 1905 module when the agent gets block/unblock STA request */
extern void wbd_slave_process_map_block_unblock_sta_request_cb(wbd_info_t *info,
	ieee1905_block_unblock_sta *block_unblock_sta);

/* Callback from IEEE 1905 module for channel preference report */
void wbd_slave_prepare_local_channel_preference(i5_dm_interface_type *i5_intf,
	ieee1905_chan_pref_rc_map_array *cp);

/* Callback from IEEE 1905 module when the agent gets Channel Selection request */
extern void wbd_slave_process_chan_selection_request_cb(wbd_info_t *info, unsigned char *al_mac,
	unsigned char *interface_mac, ieee1905_chan_pref_rc_map_array *cp,
	unsigned char rclass_local_count, ieee1905_chan_pref_rc_map *local_chan_pref);

/* Callback from IEEE 1905 module when the agent gets set tx power limit */
extern void wbd_slave_process_set_tx_power_limit_cb(wbd_info_t *info,
	char *ifname, unsigned char tx_power_limit);

/* Callback from IEEE 1905 module to get the backhual link metric */
int wbd_slave_process_get_backhaul_link_metric_cb(wbd_info_t *info, char *ifname,
	unsigned char *interface_mac, ieee1905_backhaul_link_metric *metric);

/* This creates the BSS on the interface */
int wbd_slave_create_bss_on_ifr(wbd_info_t *info, char *ifname,
	ieee1905_glist_t *bssinfo_list, ieee1905_policy_config *policy, unsigned char policy_type);

/* Callback from IEEE 1905 module to steer backhaul to a different bss */
void wbd_slave_process_bh_steer_request_cb(bcm_usched_handle *hdl, void *arg);

/* Passes the association control request to the ieee1905 */
extern void wbd_slave_send_assoc_control(int blk_time, unsigned char *source_bssid,
	unsigned char *trgt_bssid, struct ether_addr *sta_mac);

/* Do Mandate and Opportunity steer */
extern void wbd_do_map_steer(wbd_slave_item_t *slave, ieee1905_steer_req *steer_req,
	ieee1905_sta_list *sta_info, ieee1905_bss_list *bss_info, int btm_supported,
	uint8 is_retry);

/* Create the bakchual STA weak client watch dog timer */
extern void wbd_slave_create_backhaul_sta_weak_client_watchdog_timer(wbd_info_t *info);

/* Remove the bakchual STA weak client watch dog timer */
extern void wbd_slave_remove_backhaul_sta_weak_client_watchdog_timer(wbd_info_t *info);

/* Move ifname from NVRAM "nvname_from" to NVRAM "nvname_to" */
int wbd_slave_move_ifname_to_list(char *ifname, char *nvname_from, char *nvname_to);

/* Get the number of best BSTA wieghtage's defined */
int wbd_get_max_best_bsta_wght();

/* Get the weightage policy based on index */
wbd_best_bsta_wght_t *wbd_get_best_bsta_wght(wbd_blanket_slave_t *bkt_slave);

/* If multiple backhaul STA's connected, then choose the best one */
i5_dm_interface_type* wbd_slave_choose_best_bh_sta();

/* Create the bakchual STA interface watch dog timer */
void wbd_slave_create_backhaul_sta_interface_watchdog_timer(wbd_info_t *info);
#endif /* _WBD_SLAVE_CONTROL_H_ */
