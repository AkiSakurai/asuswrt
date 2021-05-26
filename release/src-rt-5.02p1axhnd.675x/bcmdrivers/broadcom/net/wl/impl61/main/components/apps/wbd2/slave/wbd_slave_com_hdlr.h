/*
 * WBD Communication Related Declarations
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
 * $Id: wbd_slave_com_hdlr.h 774130 2019-04-11 07:13:43Z $
 */

#ifndef _WBD_SLAVE_COM_HDLR_H_
#define _WBD_SLAVE_COM_HDLR_H_

#include "wbd.h"

#define DEFAULT_IFNAME "eth1"

/* BSD's error codes */
#define WBDE_BSD_OK			100
#define WBDE_BSD_FAIL			101
#define WBDE_BSD_IGNORE_STA		102
#define WBDE_BSD_NO_SLAVE_TO_STEER	103
#define WBDE_BSD_DS_BOUNCING_STA	104
#define WBDE_BSD_DS_UN_ASCSTA		105

/* Initialize the communication module for slave */
extern int wbd_init_slave_com_handle(wbd_info_t *info);

/* Callback function called from scheduler library */
/* to Process event by accepting the connection and processing the client */
extern void wbd_slave_process_event_fd_cb(bcm_usched_handle *handle,
	void *arg, bcm_usched_fds_entry_t *entry);

/* Retrieve fresh stats from driver for the associated STAs and update locally.
 * If sta_mac address is provided get the stats only for that STA
 * Else if isweakstas is TRUE, get only for weak STAs else get for all the associated STAs
 */
extern int wbd_slave_update_assoclist_fm_wl(i5_dm_bss_type *i5_bss,
	struct ether_addr *sta_mac, int isweakstas);

/* Send WEAK_CLIENT command and Update STA Status = Weak */
extern int wbd_slave_send_weak_client_cmd(i5_dm_bss_type *i5_bss, i5_dm_clients_type *i5_assoc_sta);

/* Send WEAK_CANCEL command and Update STA Status = Normal */
extern int wbd_slave_send_weak_cancel_cmd(i5_dm_bss_type *i5_bss, i5_dm_clients_type *i5_assoc_sta);

/* Create timer for Callback fn to process M2 is received for all Wireless Interfaces */
extern int wbd_slave_create_all_ap_configured_timer();

/* Get WBD Band Enumeration from ifname's Chanspec & Bridge Type */
extern int wbd_slave_identify_band_type(char* ifname, int *out_band);

/* Udate the WBD band enumeration for slave */
extern int wbd_slave_update_band_type(char* ifname, wbd_slave_item_t *slave);

/* Get StaPrio info if present */
extern int wbd_slave_check_taf_enable(wbd_slave_item_t* slave);

/* Get chan_info from driver */
extern int wbd_slave_get_chan_info(char* ifname,
	wbd_interface_chan_info_t* wbd_chan_info, int index_size);

/* Set chanspec through ACSD deamon running on ACCESS POINT */
extern int wbd_slave_set_chanspec_through_acsd(wbd_slave_item_t *slave_item,
	chanspec_t chspec, int option, i5_dm_interface_type *interface);

/* Wait for to Set chanspec through ACSD deamon */
extern void wbd_slave_wait_set_chanspec(wbd_slave_item_t *slave, i5_dm_interface_type *interface,
	chanspec_t chspec);

/* Set mode of ACS deamon running on Repeater to Fixed Chanspec */
extern int wbd_slave_set_acsd_mode_to_fixchspec(i5_dm_interface_type *pdmif);

extern void wbd_slave_check_bh_join_timer_cb(bcm_usched_handle *hdl, void *arg);

/* Get Interface metrics */
int wbd_slave_process_get_interface_metric_cb(wbd_info_t *info, char *ifname,
	unsigned char *ifr_mac, ieee1905_interface_metric *metric);

/* Get AP metrics */
int wbd_slave_process_get_ap_metric_cb(wbd_info_t *info, char *ifname,
	unsigned char *bssid, ieee1905_ap_metric *metric);

/* Add STAs to sta monitor to measure the RSSI */
int wbd_slave_process_get_unassoc_sta_metric_cb(wbd_info_t *info,
	ieee1905_unassoc_sta_link_metric_query *query);

/* Send beacon metrics request for each channel to a STA */
int wbd_slave_process_per_chan_beacon_metrics_query_cb(wbd_info_t *info, char *ifname,
	unsigned char *bssid, ieee1905_beacon_request *query);

/* Send beacon metrics request to a STA */
int wbd_slave_process_beacon_metrics_query_cb(wbd_info_t *info, char *ifname, unsigned char *bssid,
	ieee1905_beacon_request *query);

/* recieved Multi-AP Policy Configuration */
void wbd_slave_policy_configuration_cb(wbd_info_t *info, ieee1905_policy_config *policy,
	unsigned short rcvd_policies, ieee1905_vendor_data *in_vndr_tlv);

/* Process 1905 Vendor Specific Messages at WBD Application Layer */
extern int wbd_slave_process_vendor_specific_msg(wbd_info_t *info, ieee1905_vendor_data *msg_data);

/* RRM add neighbor AP from Agent topology */
void wbd_slave_add_nbr_from_topology(void);

/* RRM add neighbor AP from self device */
void wbd_slave_add_nbr_from_self_dev(i5_dm_device_type *sdev);

/* RRM add neighbor on bssinit */
void wbd_slave_add_bss_nbr(i5_dm_bss_type *pbss);

/* RRM add neighbor on channel change */
void wbd_slave_add_ifr_nbr(i5_dm_interface_type *i5_ifr, bool noselfbss);

/* RRM delete neighbor */
void wbd_slave_del_bss_nbr(i5_dm_bss_type *pbss);

/* Create timer for Slave to do rc restart gracefully */
int wbd_slave_create_rc_restart_timer(wbd_info_t *info);

/* Update chanspec to master, if changed */
void wbd_slave_update_chanspec_timer_cb(bcm_usched_handle *hdl, void *arg);

/* Slave get Vendor Specific TLV to send for 1905 Message */
extern void wbd_slave_get_vendor_specific_tlv(i5_msg_types_with_vndr_tlv_t msg_type,
	ieee1905_vendor_data *vendor_tlv);

/* Callbck for sending operating channel reports on channel selection request */
extern void wbd_slave_send_opchannel_reports(void);

/* Store the BSSID in the NVRAM . If the force is set, then store it directly. Else store only
 * if the NVRAM is not defined
 */
extern void wbd_slave_store_bssid_nvram(char *prefix, unsigned char *bssid, int force);

/* get chan info from firmware and send to controller */
extern int wbd_slave_send_chan_info();

/* set acsd mode to fix chanspec on agents, not runing on controller */
extern int wbd_slave_set_acsd_mode_fix_chanspec();
#endif /* _WBD_SLAVE_COM_HDLR_H_ */
