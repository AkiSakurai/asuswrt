/*
 * WBD Communication Related Declarations for Master
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
 * $Id: wbd_master_com_hdlr.h 781891 2019-12-04 05:05:53Z $
 */

#ifndef _WBD_MASTER_COM_HDLR_H_
#define _WBD_MASTER_COM_HDLR_H_

#include "wbd.h"
#include "wbd_shared.h"

/* Bit fields for flags in wbd_master_info */
#define WBD_FLAGS_MASTER_FBT_ENABLED		0x0001	/* FBT enabled in Master */
#define WBD_FLAGS_MASTER_BH_OPT_TIMER		0x0002	/* Backhaul Optimization Timer is running */
#define WBD_FLAGS_MASTER_BH_OPT_RUNNING		0x0004	/* Backhaul Optimization is Running */
#define WBD_FLAGS_MASTER_BH_OPT_PENDING		0x0008	/* Backhaul Optimization is pending */

/* Helper macros for flags on wbd_master_info */
#ifdef WLHOSTFBT
/* Check whether FBT is enabled or not */
#define WBD_FBT_ENAB(flags) ((flags) & (WBD_FLAGS_MASTER_FBT_ENABLED))
#else
#define WBD_FBT_ENAB(flags) 0
#endif /* WLHOSTFBT */
#define WBD_MINFO_IS_BH_OPT_TIMER(flags)	((flags) & WBD_FLAGS_MASTER_BH_OPT_TIMER)
#define WBD_MINFO_IS_BH_OPT_RUNNING(flags)	((flags) & WBD_FLAGS_MASTER_BH_OPT_RUNNING)
#define WBD_MINFO_IS_BH_OPT_PENDING(flags)	((flags) & WBD_FLAGS_MASTER_BH_OPT_PENDING)

/* Bit fields for flags in wbd_blanket_master */
#define WBD_FLAGS_BKT_BH_OPT_ENABLED		0x00000001	/* Backhaul Optimization feature
								 * is enabled
								 */
#define WBD_FLAGS_BKT_DONT_SEND_BCN_QRY	0x00000002	/* Don't Send Beacon Query */
#define WBD_FLAGS_BKT_DONT_USE_BCN_RPT	0x00000004	/* Don't use Beacon report */

/* Helper macros for flags on wbd_blanket_master */
#define WBD_BKT_BH_OPT_ENAB(flags)		((flags) & (WBD_FLAGS_BKT_BH_OPT_ENABLED))
#define WBD_BKT_DONT_SEND_BCN_QRY(flags)	((flags) & (WBD_FLAGS_BKT_DONT_SEND_BCN_QRY))
#define WBD_BKT_DONT_USE_BCN_RPT(flags)		((flags) & (WBD_FLAGS_BKT_DONT_USE_BCN_RPT))

/* Bit fields for wbd_master_flags NVRAM */
#define WBD_NVRAM_FLAGS_MASTER_DONT_SEND_BCN_QRY	0x0001	/* Don't Send Beacon Query */
#define WBD_NVRAM_FLAGS_MASTER_DONT_USE_BCN_RPT		0x0002	/* Don't Use Beacon report */

/* stats expiry time */
#define WBD_METRIC_EXPIRY_TIME		10

/* Initialize the communication module for master */
extern int wbd_init_master_com_handle(wbd_info_t *info);

/* Process 1905 Vendor Specific Messages at WBD Application Layer */
extern int wbd_master_process_vendor_specific_msg(ieee1905_vendor_data *msg_data);

/* Creates the blanket master for the blanket ID */
extern void wbd_master_create_master_info(wbd_info_t *info, uint8 bkt_id,
	char *bkt_name);

/* Got Associated STA link metric response */
extern void wbd_master_process_assoc_sta_metric_resp(wbd_info_t *info, unsigned char *al_mac,
	unsigned char *bssid, unsigned char *sta_mac, ieee1905_sta_link_metric *metric);

/* Got UnAssociated STA link metric response */
extern void wbd_master_unassoc_sta_metric_resp(unsigned char *al_mac,
	ieee1905_unassoc_sta_link_metric *metric);

/* Got Beacon Metrics metric response */
extern void wbd_master_beacon_metric_resp(unsigned char *al_mac, ieee1905_beacon_report *report);

/* Set AP configured flag */
extern void wbd_master_set_ap_configured(unsigned char *al_mac, unsigned char *radio_mac,
	int if_band);

/* Send channel selection request */
void wbd_master_send_channel_selection_request(wbd_info_t *info, unsigned char *al_mac,
	unsigned char *radio_mac);

/* Add the metric policy for a radio */
void wbd_master_add_metric_report_policy(wbd_info_t *info, unsigned char *al_mac,
	unsigned char* radio_mac, int if_band);

/* Update the AP channel report */
void wbd_master_update_ap_chan_report(wbd_glist_t *ap_chan_report, i5_dm_interface_type *i5_ifr);

int wbd_master_send_link_metric_requests(wbd_master_info_t *master, i5_dm_clients_type *sta);

/* Get interface info */
int wbd_master_get_interface_info_cb(char *ifname, ieee1905_ifr_info *info);

/* Handle master restart; Send renew if the agent didn't AP Auto configuration */
int wbd_master_create_ap_autoconfig_renew_timer(wbd_info_t *info);

/* Create channel prefernce query timer */
int wbd_master_create_channel_select_timer(wbd_info_t *info, unsigned char *al_mac);

/* Handle operating channel report */
void wbd_master_process_operating_chan_report(wbd_info_t *info, unsigned char *al_mac,
	ieee1905_operating_chan_report *chan_report);

/* Update the lowest Tx Power of all BSS */
void wbd_master_update_lowest_tx_pwr(wbd_info_t *info, unsigned char *al_mac,
	ieee1905_operating_chan_report *chan_report);

/* Master get Vendor Specific TLV to send for 1905 Message */
extern void wbd_master_get_vendor_specific_tlv(i5_msg_types_with_vndr_tlv_t msg_type,
	ieee1905_vendor_data *vendor_tlv);

/* Store beacon metric response */
void
wbd_master_store_beacon_metric_resp(wbd_info_t *info, unsigned char *al_mac,
	ieee1905_beacon_report *report);

/* Send BSS capability query message */
extern int wbd_master_send_bss_capability_query(wbd_info_t *info, unsigned char *al_mac,
	unsigned char* radio_mac);

/* Called when some STA joins the slave */
extern int wbd_controller_refresh_blanket_on_sta_assoc(struct ether_addr *sta_mac,
	struct ether_addr *parent_bssid, wbd_wl_sta_stats_t *sta_stats);

/* Send BSS metrics query message */
extern int wbd_master_send_bss_metrics_query(unsigned char *al_mac);

/* Send 1905 Vendor Specific backhaul STA mertric policy command, from Controller to Agent */
extern int wbd_master_send_backhaul_sta_metric_policy_vndr_cmd(wbd_info_t *info,
	unsigned char *neighbor_al_mac, unsigned char *radio_mac);
#endif /* _WBD_MASTER_COM_HDLR_H_ */
