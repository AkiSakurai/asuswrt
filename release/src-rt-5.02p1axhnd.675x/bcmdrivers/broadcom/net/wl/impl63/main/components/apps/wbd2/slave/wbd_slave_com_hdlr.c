/*
 * WBD Communication Related Definitions
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
 * $Id: wbd_slave_com_hdlr.c 782305 2019-12-17 04:58:40Z $
 */

#include "wbd.h"
#include "wbd_shared.h"
#include "wbd_ds.h"
#include "wbd_com.h"
#include "wbd_sock_utility.h"
#include "wbd_json_utility.h"
#include "wbd_wl_utility.h"
#include "wbd_slave_control.h"
#include "wbd_slave_com_hdlr.h"
#include "wbd_blanket_utility.h"
#ifdef PLC_WBD
#include "wbd_plc_utility.h"
#endif /* PLC_WBD */

#include "ieee1905.h"
#include "ieee1905_tlv.h"
#include "wbd_tlv.h"

#include <shutils.h>
#include <wlutils.h>
#ifdef BCM_APPEVENTD
#include "wbd_appeventd.h"
#endif /* BCM_APPEVENTD */
#include <wbd_rc_shared.h>
#include "blanket.h"
#include <linux/if_ether.h>

/* process WLC_E_AP_CHAN_CHANGE event based on following flags */
#define IGNORE_DFS_AP_MOVE_START		0X00
#define IGNORE_DFS_AP_MOVE_RADAR_FOUND		0X01
#define IGNORE_DFS_AP_MOVE_ABORTED		0X02
#define IGNORE_DFS_AP_MOVE_STUNT		0X03
#define IGNORE_CSA				0X04

#define	WBD_TAF_STA_FMT				"toa-sta-%d"
#define	WBD_TAF_BSS_FMT				"toa-bss-%d"
#define WBD_TAF_DEF_FMT				"toa-defs"

/* Macros for ESP */
#define DATA_FORMAT_SHIFT	3
#define BA_WSIZE_SHIFT		5

#define ESP_BE			1
#define ESP_AMSDU_ENABLED	(1 << DATA_FORMAT_SHIFT)
#define ESP_AMPDU_ENABLED	(2 << DATA_FORMAT_SHIFT)
#define ESP_BA_WSIZE_NONE	(0 << BA_WSIZE_SHIFT)
#define ESP_BA_WSIZE_2		(1 << BA_WSIZE_SHIFT)
#define ESP_BA_WSIZE_4		(2 << BA_WSIZE_SHIFT)
#define ESP_BA_WSIZE_6		(3 << BA_WSIZE_SHIFT)
#define ESP_BA_WSIZE_8		(4 << BA_WSIZE_SHIFT)
#define ESP_BA_WSIZE_16		(5 << BA_WSIZE_SHIFT)
#define ESP_BA_WSIZE_32		(6 << BA_WSIZE_SHIFT)
#define ESP_BA_WSIZE_64		(7 << BA_WSIZE_SHIFT)

/* DFS CAC request status on radio after receiving cac msg from controller */
#define MAP_CAC_RQST_RCVD			0x01
#define MAP_CAC_RQST_RUNNING			0x02
#define MAP_CAC_RQST_COMPLETE			0x04
#define MAP_CAC_RQST_SUCCESS			0x08
#define MAP_CAC_RQST_ABORT			0x10
#define MAP_CAC_RQST_METHOD_NOT_SUPPORT		0x20

#define MAP_CAC_METHOD_MASK				0XE0 /* 5 - 7 bits */
#define MAP_CAC_ACTION_MASK				0X18 /* 3 - 4 bits */

#define GET_MAP_CAC_METHOD(flags)		(((flags & MAP_CAC_METHOD_MASK) >> 5))
#define GET_MAP_CAC_COMPLETION_ACTION(flags)	(((flags & MAP_CAC_ACTION_MASK) >> 3))

#define WBD_CAC_METHOD_CONTINOUS_CAC		0X00 /* map with MAP_CAC_METHOD_CONTINOUS_CAC */
#define WBD_CAC_METHOD_MIMO_DIMENSION_REDUCED	0X02

#define WBD_SWAP(a, b, T) do { T _t; _t = a; a = b; b = _t; } while (0)

/* Information for slave's beacon request timer callback */
typedef struct wbd_bcn_req_arg {
	wbd_info_t *info;	/* wbd_info pointer */
	char ifname[IFNAMSIZ];	/* interface on which sta is associated */
	unsigned char neighbor_al_mac[IEEE1905_MAC_ADDR_LEN]; /* AL ID from where request came */
	blanket_bcnreq_t bcnreq_extn;	/* beacon request parameters */
	uint8 chan_count;	/* Count of channels on which beacon request to be sent */
	uint8 curr_chan_idx;	/* Index of current channel */
	uint8 *chan;		/* Array of channels */
	uint8 subelement[WBD_MAX_BUF_64];	/* subelements array */
	uint8 subelem_len;	/* length of subelements */
} wbd_bcn_req_arg_t;

/* Information for Slave's action frame timer callback */
typedef struct wbd_actframe_arg {
	wbd_slave_item_t *slave;
	i5_dm_bss_type *i5_bss;	/* BSS where the STA is associated */
	int af_count; /* Number of actframes to send */
	struct ether_addr sta_mac; /* STA mac address */
} wbd_actframe_arg_t;

/* structure to hold the params to retry the STEER in case of STA not accepts */
typedef struct wbd_slave_steer_retry_arg {
	wbd_info_t *info;		/* WBD Info */
	int btm_supported;		/* STA supports BTM or not */
	struct timeval assoc_time;	/* Last associated time */
	ieee1905_steer_req steer_req;	/* Steer request */
	ieee1905_bss_list tbss_info;	/* Target BSS info */
	ieee1905_sta_list sta_info;	/* STA Info(MAC) */
} wbd_slave_steer_retry_arg_t;

/* structure to hold the params to send the unassociated STA link metrics */
typedef struct wbd_slave_map_monitor_arg {
	wbd_slave_item_t *slave;	/* Slave item */
	unsigned char neighbor_al_mac[IEEE1905_MAC_ADDR_LEN];	/* AL ID from where request
								 * came (Used only at agent)
								 */
	wbd_maclist_t *maclist;
	uint8 channel;		/* channel at which monitor request came */
	uint8 rclass;		/* Operating class for the channel specified */
} wbd_slave_map_monitor_arg_t;

/* structure to hold the params send the beacon report after time expires */
typedef struct wbd_slave_map_beacon_report_arg {
	wbd_info_t *info;		/* WBD Info */
	struct ether_addr sta_mac;	/* MAC address of STA */
} wbd_slave_map_beacon_report_arg_t;

#ifdef WLHOSTFBT
/* Check whether FBT enabling is possible or not. First it checks for psk2 and then wbd_fbt */
extern int wbd_is_fbt_possible(char *prefix);
#endif /* WLHOSTFBT */

/* ------------------------------------ Static Declarations ------------------------------------ */

/* Allocates memory for stamon maclist */
static bcm_stamon_maclist_t*
wbd_slave_alloc_stamon_maclist_struct(int ncount);
/* Creates maclist structure for input to stamon */
static int
wbd_slave_prepare_stamon_maclist(struct ether_addr *mac,
	bcm_stamon_maclist_t **stamonlist);
/* Remove a STA MAC from STAMON */
static int
wbd_slave_remove_sta_fm_stamon(wbd_slave_item_t *slave_item, struct ether_addr *mac,
	BCM_STAMON_STATUS *status);
/* Add MAC address and Priority to stamon maclist on index */
static int
wbd_slave_add_sta_to_stamon_maclist(bcm_stamon_maclist_t *list, struct ether_addr *mac,
	bcm_stamon_prio_t priority, chanspec_t chspec, int idx,
	bcm_offchan_sta_cbfn *cbfn, void *arg);
/* Parse and process the EVENT from EAPD */
static int
wbd_slave_process_event_msg(wbd_info_t* info, char* pkt, int len);
#ifdef PLC_WBD
/* Retrieve fresh stats from PLC for all the PLC associated nodes and update locally */
static int
wbd_slave_update_plc_assoclist(wbd_slave_item_t *slave);
#endif /* PLC_WBD */
/* Processes WEAK_CLIENT response */
static void
wbd_slave_process_weak_client_cmd_resp(unsigned char *neighbor_al_mac,
	unsigned char *tlv_data, unsigned short tlv_data_len);
/* Callback fn for Slave to do rc restart gracefully */
static void
wbd_slave_rc_restart_timer_cb(bcm_usched_handle *hdl, void *arg);
/* Slave updates WEAK_CLIENT_BSD Data */
static int
wbd_slave_process_weak_client_bsd_data(i5_dm_bss_type *i5_bss,
	wbd_cmd_weak_client_bsd_t *cmdweakclient);
/* Processes WEAK_CLIENT_BSD request */
static void
wbd_slave_process_weak_client_bsd_cmd(wbd_com_handle *hndl, int childfd, void *cmddata, void *arg);
/* Slave updates WEAK_CANCEL_BSD Data */
static int
wbd_slave_process_weak_cancel_bsd_data(i5_dm_bss_type *i5_bss,
	wbd_cmd_weak_cancel_bsd_t *cmdweakcancel);
/* Processes WEAK_CANCEL_BSD request */
static void
wbd_slave_process_weak_cancel_bsd_cmd(wbd_com_handle *hndl, int childfd, void *cmddata, void *arg);
/* Processes STA_STATUS_BSD request */
static void
wbd_slave_process_sta_status_bsd_cmd(wbd_com_handle *hndl, int childfd, void *cmddata, void *arg);
/* Callback to send ACTION frame to STA */
static void
wbd_slave_actframe_timer_cb(bcm_usched_handle *hdl, void *arg);
/* Callback to send beacon request frame to weak STA */
static void
wbd_slave_bcn_req_timer_cb(bcm_usched_handle *hdl, void *arg);
/* Convert WBD error code to BSD error code */
static int
wbd_slave_wbd_to_bsd_error_code(int error_code);
/* Processes REMOVE_CLIENT_REQ cmd */
static void
wbd_slave_process_remove_client_cmd(unsigned char *neighbor_al_mac,
	unsigned char *tlv_data, unsigned short tlv_data_len);
/* Process BLOCK_CLIENT_BSD request */
static void
wbd_slave_process_blk_client_bsd_cmd(wbd_com_handle *hndl, int childfd, void *cmddata, void *arg);
/* get taf's traffic configuration */
static int wbd_slave_get_taf_conf(wbd_slave_item_t* slave);
/* get taf's list configuration */
static int wbd_slave_get_taf_list(wbd_taf_list_t* list, const char* keyfmt);
/* get taf's def traffic configuration */
static int wbd_slave_get_taf_defprio(wbd_slave_item_t* slave);
/* Callback function indicating off channel STA addition to stamon driver */
static void wbd_slave_offchan_sta_cb(void *arg, struct ether_addr *ea);
/* Send beacon report */
static int
wbd_slave_store_beacon_report(wbd_slave_item_t *slave, uint8 *data, struct ether_addr *sta_mac);
/* trigger timer to send action frame to client */
static int wbd_slave_send_action_frame_to_sta(wbd_info_t* info, wbd_slave_item_t *slave,
	i5_dm_bss_type *i5_bss, struct ether_addr* mac_addr);
/* -------------------------- Add New Functions above this -------------------------------- */

/* Get the SLAVELIST CLI command data */
static int
wbd_slave_process_slavelist_cli_cmd_data(wbd_info_t *info,
	wbd_cli_send_data_t *clidata, char **outdataptr);
/* Processes the SLAVELIST CLI command */
static void
wbd_slave_process_slavelist_cli_cmd(wbd_com_handle *hndl, int childfd, void *cmddata, void *arg);
/* Get the INFO CLI command data */
static int
wbd_slave_process_info_cli_cmd_data(wbd_info_t *info,
	wbd_cli_send_data_t *clidata, char **outdataptr);
/* Processes the INFO CLI command */
static void
wbd_slave_process_info_cli_cmd(wbd_com_handle *hndl, int childfd, void *cmddata, void *arg);
/* Get the CLIENTLIST CLI command data */
static int
wbd_slave_process_clientlist_cli_cmd_data(wbd_info_t *info,
	wbd_cli_send_data_t *clidata, char **outdataptr);
/* Processes the CLIENTLIST CLI command */
static void
wbd_slave_process_clientlist_cli_cmd(wbd_com_handle *hndl, int childfd, void *cmddata, void *arg);
/* Processes the MONITORADD CLI command data */
static int
wbd_slave_process_monitoradd_cli_cmd_data(wbd_info_t *info, wbd_cli_send_data_t *clidata,
	char **outdataptr);
/* Processes the MONITORADD CLI command  */
static void
wbd_slave_process_monitoradd_cli_cmd(wbd_com_handle *hndl, int childfd, void *cmddata, void *arg);
/* Processes the MONITORDEL CLI command data */
static int
wbd_slave_process_monitordel_cli_cmd_data(wbd_info_t *info,
	wbd_cli_send_data_t *clidata, char **outdataptr);
/* Processes the MONITORDEL CLI command */
static void
wbd_slave_process_monitordel_cli_cmd(wbd_com_handle *hndl, int childfd, void *cmddata, void *arg);
/* Get the MONITORLIST CLI command data */
static int
wbd_slave_process_monitorlist_cli_cmd_data(wbd_info_t *info,
	wbd_cli_send_data_t *clidata, char **outdataptr);
/* Processes the MONITORLIST CLI command */
static void
wbd_slave_process_monitorlist_cli_cmd(wbd_com_handle *hndl, int childfd, void *cmddata, void *arg);
/* Processes the WEAK_CLIENT CLI command data */
static int
wbd_slave_process_weak_client_cli_cmd_data(wbd_info_t *info, wbd_cli_send_data_t *clidata,
	char **outdataptr);
/* Processes the WEAK_CLIENT CLI command  */
static void
wbd_slave_process_weak_client_cli_cmd(wbd_com_handle *hndl, int childfd, void *cmddata, void *arg);
/* Processes the WEAK_CANCEL CLI command data */
static int
wbd_slave_process_weak_cancel_cli_cmd_data(wbd_info_t *info, wbd_cli_send_data_t *clidata,
	char **outdataptr);
/* Processes the WEAK_CANCEL CLI command  */
static void
wbd_slave_process_weak_cancel_cli_cmd(wbd_com_handle *hndl, int childfd, void *cmddata, void *arg);
/* Callback for exception from communication handler for slave server */
static void
wbd_slave_com_server_exception(wbd_com_handle *hndl, void *arg, WBD_COM_STATUS status);
/* Callback for exception from communication handler for CLI */
static void
wbd_slave_com_cli_exception(wbd_com_handle *hndl, void *arg, WBD_COM_STATUS status);
/* Register all the commands for master server to communication handle */
static int
wbd_slave_register_server_command(wbd_info_t *info);
/* send operating channel report to master */
static void
wbd_slave_send_operating_chan_report(i5_dm_interface_type *sifr);
/* Processes BSS capability query message */
static void wbd_slave_process_bss_capability_query_cmd(unsigned char *neighbor_al_mac,
	unsigned char *tlv_data, unsigned short tlv_data_len);
/* Send BSS capability report message */
static void wbd_slave_send_bss_capability_report_cmd(unsigned char *neighbor_al_mac,
	unsigned char *radio_mac);
/* Processes BSS metrics query message */
static void wbd_slave_process_bss_metrics_query_cmd(unsigned char *neighbor_al_mac,
	unsigned char *tlv_data, unsigned short tlv_data_len);
/* Send BSS metrics report message */
static void wbd_slave_send_bss_metrics_report_cmd(unsigned char *neighbor_al_mac);
/* Handle zero wait dfs message from controller */
static int wbd_slave_process_zwdfs_msg(unsigned char* neighbor_al_mac, unsigned char *tlv_data,
	unsigned short tlv_data_len);
/* Send 1905 Vendor Specific zero wait dfs message from Agent to Controller */
static int wbd_slave_send_vendor_msg_zwdfs(wbd_cmd_vndr_zwdfs_msg_t *msg);
/* Processes backhaul STA metric policy vendor message */
static void wbd_slave_process_backhaul_sta_metric_policy_cmd(wbd_info_t *info,
	unsigned char *neighbor_al_mac, unsigned char *tlv_data, unsigned short tlv_data_len);
/* Processes NVRAM set vendor message */
static void wbd_slave_process_vndr_nvram_set_cmd(wbd_info_t *info, unsigned char *neighbor_al_mac,
	unsigned char *tlv_data, unsigned short tlv_data_len);
/* Process vendor specific chan set command */
static int wbd_slave_process_vndr_set_chan_cmd(unsigned char* neighbor_al_mac,
	unsigned char *tlv_data, unsigned short tlv_data_len);
/* Check chan_info and update to master for the same */
static void wbd_slave_chk_and_send_chan_config_info(i5_dm_interface_type *pdmif,
	bool send_explictly);
/* Use controller's chan info and local chanspecs list to prepare dfs_channel_forced
 * list and pass to firmware via "dfs_channel_forced" iovar
 */
static void wbd_slave_process_dfs_chan_info(unsigned char *src_al_mac, unsigned char *tlv_data,
	unsigned short tlv_data_len);
/* Create dfs_pref_chan_list */
static void
wbd_slave_prepare_dfs_list(wbd_dfs_chan_info_t *chan_info, i5_dm_interface_type *pdmif,
	uint8 band, wl_dfs_forced_t* dfs_frcd);
/* Get channel utilization */
static int wbd_slave_get_chan_util(char *ifname, unsigned char *chan_util);
#if defined(MULTIAPR2)
/* Send Association status notification request to controller */
static void wbd_slave_send_association_status_notification(i5_dm_bss_type *bss);
/* send tunnel message to controller */
static void wbd_slave_send_tunneled_msg(ieee1905_tunnel_msg_t *tunnel_msg);
/* process cac start request message from controller and trigger DFS CAC if required */
static void wbd_slave_process_cac_start_request(wbd_info_t *info, ieee1905_cac_rqst_list_t *pmsg);
/* Process cac terminate message from controller and stop DFS CAC if running */
static void wbd_slave_process_cac_terminate_rqst(wbd_info_t *info,
	ieee1905_cac_termination_list_t *pmsg);
/* Prepare CAC capability for each 5G capable radio */
static void wbd_slave_prepare_radio_cac_capability(i5_dm_interface_type *pdmif, uint8 **pmem,
	uint16 *payload_len);
/* Start On Boot local Channel Scan on Agent for all Radios */
static void wbd_slave_process_onboot_channel_scan(bcm_usched_handle *hdl, void *arg);
/* Get driver capability and check wheather radio is capable of BGDFS */
static int wbd_slave_get_driver_capability(char *ifname, wbd_ifr_item_t *ifr_vndr_info);
/* Prepare CAC capability for Non EDCRS compliant countries */
static int wbd_slave_prepare_non_edcrs_cac_capability_radar_info(i5_dm_interface_type *pdmif,
	uint8 *pbuf, uint16 *payload_len);
/* Prepare CAC capability for EDCRS compliant countries with only dfs weather channel list */
static int wbd_slave_prepare_edcrs_cac_capability_weather_info(i5_dm_interface_type *pdmif,
	uint8 *pbuf, uint16 *payload_len);
/* Prepare CAC capability for EDCRS compliant countries excluding dfs weather channel list */
static int wbd_slave_prepare_edcrs_cac_capability_radar_info(i5_dm_interface_type *pdmif,
	uint8 *pbuf, uint16 *payload_len);
/* Prepare cac completion report for radio who served CAC start request */
static int wbd_slave_prepare_list_of_radios_serving_cac_rqst(uint8 **pbuf, uint32 *payload_len);
/* Prepare cac completion report for all radio who declined the cac start request */
static int wbd_slave_prepare_list_of_radios_decline_cac_rqst(uint8 **pbuf, uint32 *payload_len);
/* Create active channels(done CAC) list */
static void wbd_slave_prepare_active_list(i5_dm_interface_type *pdmif, uint8 **pbuf,
	uint16 *payload_len);
/* Create radar detected chan list */
static void wbd_slave_prepare_radar_detected_chan_list(i5_dm_interface_type *pdmif, uint8 **pbuf,
	uint16 *payload_len);
/* Create ongoing CAC chan list */
static void wbd_slave_prepare_ongoing_cac_chan_list(i5_dm_interface_type *pdmif, uint8 **pbuf,
	uint16 *payload_len);
/* check interface's radio capabiliy to find opclass is valid or not */
static bool wbd_slave_opclass_valid_for_interface(i5_dm_interface_type *pdmif, uint8 opclass);
/* check chan validity from radio capability of interface */
static bool wbd_slave_chan_valid_for_interface(i5_dm_interface_type *pdmif, uint8 opclass,
	uint8 chan);
#endif /* MULTIAPR2 */
/* ------------------------------------ Static Declarations ------------------------------------ */

extern void wbd_exit_slave(wbd_info_t *info);

/* Get WBD Band Enumeration from ifname's Chanspec & Bridge Type */
int
wbd_slave_identify_band_type(char* ifname, int *out_band)
{
	int ret = WBDE_OK, bridge_dgt = WBD_BRIDGE_LAN;
	chanspec_t ifr_chanspec;
	WBD_ENTER();

	WBD_ASSERT_ARG(out_band, WBDE_INV_ARG);
	*out_band = WBD_BAND_LAN_INVALID;

	/* Get Chanspec from interface name */
	ret = blanket_get_chanspec(ifname, &ifr_chanspec);
	WBD_ASSERT();

	/* Get Bridge from interface name */
	ret = wbd_get_ifname_bridge(ifname, &bridge_dgt);
	WBD_ASSERT();

	/* Get WBD Band Enumeration from ifname's Chanspec & Bridge Type */
	ret = wbd_identify_wbd_band_type(bridge_dgt, ifr_chanspec, out_band);
	WBD_ASSERT();

end:
	WBD_EXIT();
	return ret;
}

/* Udate the WBD band enumeration for slave */
int
wbd_slave_update_band_type(char* ifname, wbd_slave_item_t *slave)
{
	int ret = WBDE_OK, bridge_dgt = WBD_BRIDGE_LAN;
	int band = WBD_BAND_LAN_INVALID;
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(slave, WBDE_INV_ARG);

	/* Get Bridge from interface name */
	ret = wbd_get_ifname_bridge(ifname, &bridge_dgt);
	WBD_ASSERT();

	/* Get WBD Band Enumeration from ifname's Chanspec & Bridge Type */
	ret = wbd_identify_wbd_band_type(bridge_dgt, slave->wbd_ifr.chanspec, &band);
	WBD_ASSERT();

	/* Update the band */
	slave->band = band;

end:
	WBD_INFO("ifname[%s] Bridge[%d] Chanspec[0x%x] WBD_BAND[%d]\n",
		ifname, bridge_dgt, slave ? slave->wbd_ifr.chanspec : 0x00,
		slave ? slave->band : WBD_BAND_LAN_INVALID);
	WBD_EXIT();
	return ret;

}

int
wbd_slave_check_taf_enable(wbd_slave_item_t* slave)
{
	int taf_enable = 0;
	WBD_ENTER();

	taf_enable = blanket_get_config_val_int(slave->wbd_ifr.prefix, NVRAM_TAF_ENABLE, 0);
	taf_enable ? wbd_slave_get_taf_conf(slave): taf_enable;

	WBD_EXIT();
	return taf_enable;
}

static int
wbd_slave_get_taf_defprio(wbd_slave_item_t* slave)
{
	int ret = WBDE_OK;
	char keybuf[WBD_MAX_BUF_16];
	const char* keyfmt = WBD_TAF_DEF_FMT;
	char *val = NULL;
	WBD_ENTER();

	if (!slave->taf_info) {
		wbd_ds_slave_free_taf_list(slave->taf_info);
		goto end;
	}
	memset(keybuf, 0, sizeof(keybuf));
	snprintf(keybuf, sizeof(keybuf), keyfmt);

	val = blanket_nvram_safe_get(keybuf);
	if (!strcmp(val, "")) {
		WBD_DEBUG("No toa-def Nvram entries present \n");
		goto end;
	}

	slave->taf_info->pdef = (char*)wbd_malloc(WBD_MAX_BUF_128, &ret);
	WBD_ASSERT_ARG(slave->taf_info->pdef, WBDE_INV_ARG);

	strncpy(slave->taf_info->pdef, val, strlen(val));
end:
	WBD_EXIT();
	return ret;
}

static int
wbd_slave_get_taf_list(wbd_taf_list_t* list, const char* keyfmt)
{
	int ret = WBDE_OK;
	char token[WBD_MAX_BUF_64];
	char keybuf[WBD_MAX_BUF_16];
	int i;
	char *val = NULL;
	char **ptr = NULL;
	int index = 0;
	WBD_ENTER();
#define WBD_MAX_STAPRIO		10 /* Hard Set at present */

	ptr = (char**)wbd_malloc((sizeof(char*) * WBD_MAX_STAPRIO), &ret);
	WBD_ASSERT();

	list->pStr = ptr;

	for (i = 0; i < WBD_MAX_STAPRIO; i++) {
		snprintf(keybuf, sizeof(keybuf), keyfmt, (i+1));
		val = blanket_nvram_safe_get(keybuf);
		if (!strlen(val)) {
			WBD_DEBUG("NO Entry for %s \n", keybuf);
			break;
		}
		memset(token, 0, sizeof(token));
		strncpy(token, val, strlen(val));
		list->pStr[index] = (char*)wbd_malloc(WBD_MAX_BUF_128, &ret);
		WBD_ASSERT();
		strncpy(list->pStr[index], token, sizeof(token));
		list->count++;
		index++;
	}

end:
	WBD_EXIT();
	return ret;
}

static int
wbd_slave_get_taf_conf(wbd_slave_item_t* slave)
{
	int ret = WBDE_OK;
	WBD_ENTER();

	slave->taf_info = (wbd_taf_params_t*)wbd_malloc
		(sizeof(wbd_taf_params_t), &ret);

	WBD_ASSERT_MSG("Slave["MACF"] Failed to allocate memory\n",
		ETHER_TO_MACF(slave->wbd_ifr.mac));

	ret =  wbd_slave_get_taf_list(&(slave->taf_info->sta_list), WBD_TAF_STA_FMT);
	if (ret != WBDE_OK) {
		WBD_DEBUG("NO TAF-STA specific NVRAMS \n");
		if (ret == WBDE_MALLOC_FL) {
			wbd_ds_slave_free_taf_list(slave->taf_info);
			slave->taf_info = NULL;
			goto end;
		}
	}

	ret =  wbd_slave_get_taf_list(&(slave->taf_info->bss_list), WBD_TAF_BSS_FMT);
	if (ret != WBDE_OK) {
		WBD_DEBUG("NO TAF-BSS specific NVRAMS \n");
		if (ret == WBDE_MALLOC_FL) {
			wbd_ds_slave_free_taf_list(slave->taf_info);
			slave->taf_info = NULL;
			goto end;
		}
	}

	ret =  wbd_slave_get_taf_defprio(slave);
end:
	WBD_EXIT();
	return ret;
}

/* Get interface specific chan_info for all available channels */
int
wbd_slave_get_chan_info(char* ifname, wbd_interface_chan_info_t* wbd_chan_info, int index_size)
{
	uint bitmap, channel;
	int ret = WBDE_OK, iter = 0;
	WBD_ENTER();

	WBD_ASSERT_ARG(wbd_chan_info, WBDE_INV_ARG);

	wbd_chan_info->count = 0;

	for (channel = 0; channel <= MAXCHANNEL; channel++) {
		bitmap = 0x00;
		/* Get Interface specific chan_info */
		ret = blanket_get_chan_info(ifname, channel, &bitmap);
		WBD_ASSERT();

		/* Extract timestamo information */
		bitmap &= WBD_CHAN_INFO_BITMAP_MASK;

		if (!(bitmap & WL_CHAN_VALID_HW)) {
			/* Invalid channel */
			continue;
		}

		if (!(bitmap & WL_CHAN_VALID_SW)) {
			/* Not supported in current locale */
			continue;
		}

		/* Prevent Buffer overflow */
		if (iter < index_size) {
			wbd_chan_info->chinfo[iter].channel = (uint8)channel;
			/* Exclude the Minute information from bitmap */
			wbd_chan_info->chinfo[iter].bitmap = bitmap;
			iter++;
		} else {
			break;
		}
	}

	wbd_chan_info->count = iter;
	/* Just to print the channel info */
	if (WBD_WL_DUMP_ENAB) {
		WBD_DEBUG("ifname[%s] index_size[%d] count[%d] and [channel, bitmap] [ ", ifname,
			index_size, wbd_chan_info->count);
		for (iter = 0; iter < wbd_chan_info->count; iter++) {
			WBD_DEBUG("[%d, 0x%x] ", wbd_chan_info->chinfo[iter].channel,
				wbd_chan_info->chinfo[iter].bitmap);
		}
		WBD_DEBUG(" ]\n");
	}

end:
	WBD_EXIT();
	return ret;
}

/* Allocates memory for stamon maclist */
static bcm_stamon_maclist_t*
wbd_slave_alloc_stamon_maclist_struct(int ncount)
{
	int ret = WBDE_OK, buflen;
	bcm_stamon_maclist_t *tmp = NULL;
	WBD_ENTER();

	buflen = sizeof(bcm_stamon_maclist_t) + (sizeof(bcm_stamon_macinfo_t) * (ncount - 1));

	tmp = (bcm_stamon_maclist_t*)wbd_malloc(buflen, &ret);
	WBD_ASSERT();

	tmp->count = ncount;
end:
	WBD_EXIT();
	return tmp;
}

/* Delete a MAC from STAMON */
static int
wbd_slave_remove_sta_fm_stamon(wbd_slave_item_t *slave_item, struct ether_addr *mac,
	BCM_STAMON_STATUS *outstatus)
{
	int ret = WBDE_OK;
	BCM_STAMON_STATUS status = BCM_STAMONE_OK;
	bcm_stamon_maclist_t *tmp = NULL;
	WBD_ENTER();

	ret = wbd_slave_prepare_stamon_maclist(mac, &tmp);
	if (ret != WBDE_OK && !tmp) {
		WBD_WARNING("Band[%d] Slave["MACF"] MAC["MACF"]. Failed to prepare stamon maclist. "
			"Error : %s\n", slave_item->band, ETHER_TO_MACF(slave_item->wbd_ifr.mac),
			ETHERP_TO_MACF(mac), wbderrorstr(ret));
		goto end;
	}

	status = bcm_stamon_command(slave_item->stamon_hdl, BCM_STAMON_CMD_DEL, (void*)tmp, NULL);
	if (status != BCM_STAMONE_OK) {
		ret = WBDE_STAMON_ERROR;
		goto end;
	}

end:
	if (outstatus)
		*outstatus = status;

	if (tmp) {
		free(tmp);
	}
	WBD_EXIT();
	return ret;
}

/* Add MAC address and Priority to stamon maclist on index */
static int
wbd_slave_add_sta_to_stamon_maclist(bcm_stamon_maclist_t *list, struct ether_addr *mac,
	bcm_stamon_prio_t priority, chanspec_t chspec, int idx,
	bcm_offchan_sta_cbfn *cbfn, void *arg)
{
	WBD_ENTER();

	list->macinfo[idx].priority = priority;
	list->macinfo[idx].chspec = chspec;
	list->macinfo[idx].arg = arg;
	list->macinfo[idx].cbfn = cbfn;
	memcpy(&list->macinfo[idx].ea, mac, sizeof(list->macinfo[idx].ea));
	idx++;

	WBD_EXIT();
	return idx;
}

static void wbd_slave_offchan_sta_cb(void *arg, struct ether_addr *ea)
{
	/* Intentionally left blank for 1905, so that existing routine
	 * bcm_stamon_add_stas_to_driver in bcm stamon library
	 * able to add sta with chanspec different from agent's chanspec
	 * to sta monitor module with non zero off chan timer.
	 */
}

/* Retry the STEER */
static void
wbd_slave_steer_retry_timer_cb(bcm_usched_handle *hdl, void *arg)
{
	int ret = WBDE_OK;
	i5_dm_bss_type *i5_bss;
	wbd_slave_item_t *slave;
	wbd_assoc_sta_item_t* sta = NULL;
	i5_dm_clients_type* i5_assoc_sta = NULL;
	wbd_slave_steer_retry_arg_t* steer_retry_arg = NULL;
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(arg, WBDE_INV_ARG);

	steer_retry_arg = (wbd_slave_steer_retry_arg_t*)arg;

	/* Find slave based on MAC address */
	slave = wbd_ds_find_slave_addr_in_blanket_slave(steer_retry_arg->info->wbd_slave,
		(struct ether_addr*)&steer_retry_arg->steer_req.source_bssid, WBD_CMP_MAC, &ret);
	WBD_ASSERT_MSG("Slave["MACDBG"] STA["MACDBG"] %s\n",
		MAC2STRDBG(steer_retry_arg->steer_req.source_bssid),
		MAC2STRDBG(steer_retry_arg->sta_info.mac), wbderrorstr(ret));

	/* Find I5 BSS based on MAC address */
	WBD_DS_GET_I5_SELF_BSS(steer_retry_arg->steer_req.source_bssid, i5_bss, &ret);
	WBD_ASSERT_MSG("BSS["MACDBG"] STA["MACDBG"] %s\n",
		MAC2STRDBG(steer_retry_arg->steer_req.source_bssid),
		MAC2STRDBG(steer_retry_arg->sta_info.mac), wbderrorstr(ret));

	/* Retrive the sta from slave's assoclist */
	i5_assoc_sta = wbd_ds_find_sta_in_bss_assoclist(i5_bss,
		(struct ether_addr*)&(steer_retry_arg->sta_info.mac), &ret, &sta);
	if (!i5_assoc_sta) {
		WBD_INFO("Slave["MACF"] STA["MACF"] Failed to find "
			"STA in assoclist. Error : %s\n",
			MAC2STRDBG(steer_retry_arg->steer_req.source_bssid),
			MAC2STRDBG(steer_retry_arg->sta_info.mac), wbderrorstr(ret));
		goto end;
	}

	/* CSTYLED */
	if (timercmp(&i5_assoc_sta->assoc_tm, &steer_retry_arg->assoc_time, !=)) {
		WBD_INFO("Slave["MACF"] STA["MACDBG"] STAAssocTime["TIMEVALF"] "
			"TimerAssocTime["TIMEVALF"]. STA Assoc Time is not matching with the "
			"assoc time from timer created\n",
			ETHER_TO_MACF(slave->wbd_ifr.mac),
			MAC2STRDBG(steer_retry_arg->sta_info.mac),
			TIMEVAL_TO_TIMEVALF(i5_assoc_sta->assoc_tm),
			TIMEVAL_TO_TIMEVALF(steer_retry_arg->assoc_time));
		goto end;
	}

	/* If the STA status is NORMAL or WEAK, No need to retry STEER */
	if (sta->status == WBD_STA_STATUS_NORMAL || sta->status == WBD_STA_STATUS_WEAK) {
		WBD_INFO("Slave["MACF"] STA["MACDBG"] Status[0x%x] is normal or weak. "
			"So no need to send weak client\n",
			ETHER_TO_MACF(slave->wbd_ifr.mac),
			MAC2STRDBG(steer_retry_arg->sta_info.mac), sta->status);
		goto end;
	}

	/* For the 1st time directly issue the BSS transition request. Next time onwards,
	 * issue the WEAK_CLIENT so that let it find the TBSS
	 */
	if (sta->steer_fail_count == 1) {
		wbd_do_map_steer(slave, &steer_retry_arg->steer_req, &steer_retry_arg->sta_info,
			&steer_retry_arg->tbss_info, steer_retry_arg->btm_supported, 1);
		goto end;
	}

	/* Send WEAK_CLIENT command and Update STA Status = Weak */
	ret = wbd_slave_send_weak_client_cmd(i5_bss, i5_assoc_sta);

end:
	if (steer_retry_arg) {
		free(steer_retry_arg);
	}

	WBD_EXIT();
}

/* Calculate timeout to retry the STEER. The series of time will be ex: 5, 10, 20, 40 ... */
static uint32
wbd_slave_get_steer_retry_timeout(int tm_gap, wbd_assoc_sta_item_t *sta)
{
	/* First retry use initial timeout and store it in steer_retry_timeout variable */
	if (sta->steer_fail_count == 1) {
		sta->steer_retry_timeout = tm_gap;
	} else {
		/* Double of previous timeout */
		sta->steer_retry_timeout *= 2;
	}

	return sta->steer_retry_timeout;
}

/* Create the timer to retry the STEER again */
int
wbd_slave_create_steer_retry_timer(wbd_slave_item_t *slave, i5_dm_clients_type *sta,
	ieee1905_steer_req *steer_req, ieee1905_bss_list *bss_info, int btm_supported)
{
	int ret = WBDE_OK;
	wbd_slave_steer_retry_arg_t *param = NULL;
	wbd_assoc_sta_item_t *assoc_sta;
	wbd_info_t *info = slave->parent->parent;
	uint32 timeout = 0;
	WBD_ENTER();

	assoc_sta = (wbd_assoc_sta_item_t*)sta->vndr_data;
	if (assoc_sta == NULL) {
		WBD_WARNING("STA["MACDBG"] NULL Vendor Data\n", MAC2STRDBG(sta->mac));
		goto end;
	}

	WBD_INFO("Slave["MACF"] STA["MACDBG"] RetryCount[%d] TmGap[%d] SteerFailCount[%d] "
		"Create STEER Retry Timer\n",
		ETHER_TO_MACF(slave->wbd_ifr.mac), MAC2STRDBG(sta->mac),
		info->steer_retry_config.retry_count, info->steer_retry_config.tm_gap,
		assoc_sta->steer_fail_count);

	/* If -1(Infinite) Always create timer to retry */
	if (info->steer_retry_config.retry_count != -1) {
		/* check if the retry count is exceeded or not */
		if (assoc_sta->steer_fail_count > info->steer_retry_config.retry_count) {
			WBD_INFO("Slave["MACF"] STA["MACDBG"] STEER Retry count exceeded\n",
				ETHER_TO_MACF(slave->wbd_ifr.mac), MAC2STRDBG(sta->mac));
			goto end;
		}
	}

	param = (wbd_slave_steer_retry_arg_t*)wbd_malloc(sizeof(*param), &ret);
	WBD_ASSERT_MSG("Slave["MACF"] STA["MACDBG"] Failed to allocate STEER_RETRY param\n",
		ETHER_TO_MACF(slave->wbd_ifr.mac), MAC2STRDBG(sta->mac));

	param->info = info;
	memcpy(&param->assoc_time, &sta->assoc_tm, sizeof(param->assoc_time));
	memcpy(param->steer_req.neighbor_al_mac, steer_req->neighbor_al_mac,
		sizeof(param->steer_req.neighbor_al_mac));
	memcpy(param->steer_req.source_bssid, steer_req->source_bssid,
		sizeof(param->steer_req.source_bssid));
	param->steer_req.request_flags = steer_req->request_flags;
	param->steer_req.opportunity_window = steer_req->opportunity_window;
	param->steer_req.dissassociation_timer = steer_req->dissassociation_timer;
	memcpy(param->sta_info.mac, sta->mac, sizeof(param->sta_info.mac));
	memcpy(param->tbss_info.bssid, bss_info->bssid, sizeof(param->tbss_info.bssid));
	param->tbss_info.target_op_class = bss_info->target_op_class;
	param->tbss_info.target_channel = bss_info->target_channel;
	param->btm_supported = btm_supported;

	/* Get timeout to retry the STEER */
	timeout = wbd_slave_get_steer_retry_timeout(info->steer_retry_config.tm_gap, assoc_sta);

	/* Create a timer */
	ret = wbd_add_timers(info->hdl, param, WBD_SEC_MICROSEC(timeout),
		wbd_slave_steer_retry_timer_cb, 0);
	WBD_ASSERT_MSG("Slave["MACF"] STA["MACDBG"] Interval[%d] Failed to create "
		"STEER Retry timer\n",
		ETHER_TO_MACF(slave->wbd_ifr.mac), MAC2STRDBG(sta->mac), timeout);

end: /* Check Slave Pointer before using it below */

	WBD_EXIT();
	return ret;
}

/* Creates maclist structure for input to stamon */
static int
wbd_slave_prepare_stamon_maclist(struct ether_addr *mac,
	bcm_stamon_maclist_t **stamonlist)
{
	int ret = WBDE_OK;
	bcm_stamon_maclist_t *tmp = NULL;
	WBD_ENTER();

	tmp = wbd_slave_alloc_stamon_maclist_struct(1);
	if (!tmp) {
		ret = WBDE_MALLOC_FL;
		goto end;
	}

	tmp->macinfo[0].priority = BCM_STAMON_PRIO_MEDIUM;
	memcpy(&tmp->macinfo[0].ea, mac, sizeof(tmp->macinfo[0].ea));

	if (stamonlist) {
		*stamonlist = tmp;
	}

end:
	WBD_EXIT();
	return ret;
}

/* Get STA info from driver */
static int
wbd_slave_fill_sta_stats(char *ifname, struct ether_addr* addr, wbd_wl_sta_stats_t *wbd_sta)
{
	/* At present gets stats for Assoc sta only */
	int ret = WBDE_OK;
	sta_info_t sta_info;

	ret = blanket_get_rssi(ifname, addr, &wbd_sta->rssi);
	WBD_ASSERT();

	/* get other stats from STA_INFO iovar */
	ret = blanket_get_sta_info(ifname, addr, &sta_info);
	WBD_ASSERT();

	wbd_sta->tx_rate = (sta_info.tx_rate / 1000);

end:
	return ret;
}

/* Callback to send ACTION frame to STA */
static void
wbd_slave_actframe_timer_cb(bcm_usched_handle *hdl, void *arg)
{
	int ret = WBDE_OK;
	wbd_actframe_arg_t* param = NULL;
	maclist_t *maclist = NULL;
	bool sta_found_in_assoclist = FALSE;
	uint count = 0;
	wbd_assoc_sta_item_t* assoc_sta = NULL;
	i5_dm_clients_type *i5_assoc_sta;
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(arg, WBDE_INV_ARG);

	param = (wbd_actframe_arg_t*)arg;

	maclist = (maclist_t*)wbd_malloc(WBD_MAX_BUF_64, &ret);
	WBD_ASSERT();

	ret = blanket_get_assoclist(param->slave->wbd_ifr.ifr.ifr_name, maclist, WBD_MAX_BUF_64);
	/* check if mac in param still in mac list, else remove the timer and exit */
	for (count = 0; count < maclist->count; count++) {
		if (!memcmp(&(param->sta_mac), &(maclist->ea[count]), sizeof(param->sta_mac))) {
			sta_found_in_assoclist = TRUE;
			break;
		}
	}

	if (sta_found_in_assoclist) {
		ret = wbd_wl_actframe_to_sta(param->slave->wbd_ifr.ifr.ifr_name,
			&(param->sta_mac));
		param->af_count--;
	}
	if (!sta_found_in_assoclist || (param->af_count <= 0)) {
		/* stop the timer */
		wbd_remove_timers(param->slave->parent->parent->hdl,
			wbd_slave_actframe_timer_cb, param);
		WBD_DEBUG("Band[%d] Slave["MACF"] STA["MACF"] , deleting "
			"action frame timer\n", param->slave->band,
			ETHER_TO_MACF(param->slave->wbd_ifr.mac), ETHER_TO_MACF(param->sta_mac));

		i5_assoc_sta = wbd_ds_find_sta_in_bss_assoclist(param->i5_bss,
			&param->sta_mac, &ret, &assoc_sta);
		if (i5_assoc_sta) {
			assoc_sta->is_offchan_actframe_tm = 0;
		}
		free(param);
		param = NULL;
	}
end:
	if (maclist) {
		free(maclist);
		maclist = NULL;
	}
	WBD_EXIT();
}

/* Send error beacon report */
static void
wbd_slave_map_send_beacon_report_error(wbd_info_t *info, uint8 resp, unsigned char *al_mac,
	unsigned char *sta_mac)
{
	ieee1905_beacon_report report;

	memset(&report, 0, sizeof(report));
	memcpy(&report.neighbor_al_mac, al_mac, sizeof(report.neighbor_al_mac));
	memcpy(&report.sta_mac, sta_mac, sizeof(report.sta_mac));
	report.response = resp;
	ieee1905_send_beacon_report(&report);
	wbd_ds_remove_beacon_report(info, (struct ether_addr *)sta_mac);
}

/* Timer callback to Send beacon report */
static void
wbd_slave_map_send_beacon_report_cb(bcm_usched_handle *hdl, void *arg)
{
	int ret = WBDE_OK;
	wbd_slave_map_beacon_report_arg_t* param = NULL;
	wbd_beacon_reports_t *report;
	ieee1905_beacon_report map_report;
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(arg, WBDE_INV_ARG);

	param = (wbd_slave_map_beacon_report_arg_t*)arg;
	WBD_INFO("STA["MACF"] Beacon report send\n", ETHER_TO_MACF(param->sta_mac));

	if ((report = wbd_ds_find_item_fm_beacon_reports(param->info, &param->sta_mac,
		&ret)) == NULL) {
		WBD_INFO("STA["MACF"] Beacon report not found\n", ETHER_TO_MACF(param->sta_mac));
		goto end;
	}

	if (report->report_element_count <= 0) {
		wbd_slave_map_send_beacon_report_error(param->info,
			IEEE1905_BEACON_REPORT_RESP_FLAG_NO_REPORT,
			(unsigned char*)&report->neighbor_al_mac,
			(unsigned char*)&report->sta_mac);
		goto end;
	}

	/* Report elements present so send it */
	memset(&map_report, 0, sizeof(map_report));
	memcpy(&map_report.neighbor_al_mac, &report->neighbor_al_mac,
		sizeof(map_report.neighbor_al_mac));
	memcpy(&map_report.sta_mac, &report->sta_mac, sizeof(map_report.sta_mac));
	map_report.response = IEEE1905_BEACON_REPORT_RESP_FLAG_SUCCESS;
	map_report.report_element_count = report->report_element_count;
	map_report.report_element_len = report->report_element_len;
	map_report.report_element = report->report_element;
	ieee1905_send_beacon_report(&map_report);
end:
	if (param) {
		free(param);
	}
	WBD_EXIT();
}

/* Callback to send beacon request frame to STA */
static void
wbd_slave_bcn_req_timer_cb(bcm_usched_handle *hdl, void *arg)
{
	int ret = WBDE_OK;
	wbd_bcn_req_arg_t *bcn_arg = NULL;
	int timeout = 0;
	bcnreq_t *bcnreq;
	int token = 0;
	blanket_bcnreq_t bcnreq_extn;
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(arg, WBDE_INV_ARG);

	bcn_arg = (wbd_bcn_req_arg_t *)arg;

	bcnreq_extn = bcn_arg->bcnreq_extn;
	bcnreq = &bcnreq_extn.bcnreq;

	if (bcn_arg->curr_chan_idx < bcn_arg->chan_count) {
		bcnreq->channel = bcn_arg->chan[bcn_arg->curr_chan_idx];
		bcnreq_extn.opclass = 0;
		bcn_arg->curr_chan_idx++;
		WBD_DEBUG("Beacon request for channel %d curr_chan_idx[%d] total[%d]\n",
			bcnreq->channel, bcn_arg->curr_chan_idx, bcn_arg->chan_count);
	}

	ret = blanket_send_beacon_request(bcn_arg->ifname, &bcnreq_extn, bcn_arg->subelement,
		bcn_arg->subelem_len, &token);
	if (ret != WBDE_OK) {
		WBD_WARNING("Ifname[%s] STA["MACF"] Beacon request failed\n",
			bcn_arg->ifname, ETHER_TO_MACF(bcnreq->da));
		goto end;
	}

	WBD_INFO("Ifname[%s] STA["MACF"] Token[%d] channel[%d]\n", bcn_arg->ifname,
		ETHER_TO_MACF(bcnreq->da), token, bcnreq->channel);

	if (bcn_arg->curr_chan_idx >= bcn_arg->chan_count) {
		/* Don't schedule the timer since this is the last channel
		 * on which beacon request needs to be sent
		*/
		if (bcn_arg && bcn_arg->chan) {
			free(bcn_arg->chan);
		}

		if (bcn_arg) {
			free(bcn_arg);
		}

		goto end;
	}

	/* There is no function available to update timer. remove_flag is set only
	 * after the completion of this callback. So, remove and add the timer with
	 * new timeout value, which will serve the purpose of update timer
	 */
	wbd_remove_timers(bcn_arg->info->hdl, wbd_slave_bcn_req_timer_cb, bcn_arg);

	timeout = WBD_MIN_BCN_REQ_DELAY + bcnreq->dur;

	ret = wbd_add_timers(bcn_arg->info->hdl, bcn_arg, WBD_MSEC_USEC(timeout),
		wbd_slave_bcn_req_timer_cb, 0);

end:
	if (ret != WBDE_OK) {
		if (bcn_arg && bcn_arg->chan) {
			free(bcn_arg->chan);
		}

		if (bcn_arg) {
			free(bcn_arg);
		}
	}

	WBD_EXIT();
	return;
}

int
wbd_slave_is_backhaul_sta_associated(char *ifname, struct ether_addr *out_bssid)
{
	int err = WBDE_OK;
	sta_info_t sta_info;
	struct ether_addr cur_bssid;

	err = blanket_try_get_bssid(ifname, WBD_MAX_GET_BSSID_TRY,
		WBD_GET_BSSID_USECOND_GAP, &cur_bssid);
	if (err == WBDE_OK) {
		memset(&sta_info, 0, sizeof(sta_info));
		err = blanket_get_sta_info(ifname, &cur_bssid, &sta_info);
	}

	/* The STA association is complete if authorized flag is set in sta_info */
	if (err == WBDE_OK && (sta_info.flags & WL_STA_AUTHO)) {
		WBD_INFO("Backhaul STA interface %s assoicated on ["MACF"]\n",
			ifname, ETHER_TO_MACF(cur_bssid));
		if (out_bssid) {
			memcpy(out_bssid, &cur_bssid, sizeof(cur_bssid));
		}
		return TRUE;
	}
	return FALSE;
}

void
wbd_slave_check_bh_join_timer_cb(bcm_usched_handle *hdl, void *arg)
{
	wbd_bh_steer_check_arg_t *param = NULL;
	struct ether_addr cur_bssid;
	WBD_ENTER();

	/* Validate arg */
	if (!arg) {
		WBD_ERROR("Invalid argument\n");
		goto end;
	}

	param = (wbd_bh_steer_check_arg_t *)arg;
	if (!param->bh_steer_msg) {
		WBD_ERROR("Steering parameters NULL\n");
		free(param);
		goto end;
	}

	param->check_cnt--;

	if (wbd_slave_is_backhaul_sta_associated(param->ifname, &cur_bssid) &&
		(eacmp(&cur_bssid, &param->bh_steer_msg->trgt_bssid) == 0)) {
		param->bh_steer_msg->resp_status_code = 0; /* Success */
		/* Store the BSSID */
		wbd_slave_store_bssid_nvram(param->prefix, param->bh_steer_msg->trgt_bssid, 1);
	} else if (param->check_cnt <= 0) {
		param->bh_steer_msg->resp_status_code = 1; /* Auth or Assoc failed */
	} else {
		goto end;
	}
	ieee1905_send_bh_steering_repsonse(param->bh_steer_msg);
	wbd_remove_timers(hdl, wbd_slave_check_bh_join_timer_cb, param);
	free(param->bh_steer_msg);
	free(param);
end:
	WBD_EXIT();
}

/* Process backhaul STA disassociation event */
static void
wbd_slave_process_bhsta_link_down_event(i5_dm_interface_type *i5_ifr)
{
	i5_dm_device_type *i5_device;
	i5_dm_interface_type *i5_upstream_ifr;

	ieee1905_bSTA_disassociated_from_backhaul_ap(i5_ifr->InterfaceId);

	/* If the upstream connection is via ethernet, then no need to
	 * enable roaming in STAs
	 */
	i5_device = i5DmFindController();
	if (i5_device && i5_device->psock) {
		i5_upstream_ifr = i5DmInterfaceFind(i5DmGetSelfDevice(),
			((i5_socket_type*)i5_device->psock)->u.sll.mac_address);
		if (i5_upstream_ifr && !i5DmIsInterfaceWireless(i5_upstream_ifr->MediaType)) {
			/* If upstream connection is via ethernet, then no need to start roam */
			WBD_INFO("Disassociated, but has ethernet connection\n");
			goto end;
		}
	}

	/* Unset the agent configured NVRAM. As this will reconfigure once the backhaul
	 * connection established
	 */
	blanket_nvram_prefix_set(NULL, NVRAM_MAP_AGENT_CONFIGURED, "0");

	wbd_ieee1905_set_bh_sta_params(IEEE1905_BH_STA_ROAM_ENAB_VAP_FOLLOW, NULL);

end:
	return;
}

#if defined(MULTIAPR2)
/* This callback function will get trigger every interval(Minute)
 *  to reset the count value of reporting rate.
 *  Agent should report unsuccessfull association event to controller.
 *  But Agent should not report more than limit/reporting rate within a minute.
 *  So maintaining counter and reset the counter every minute
 */
void
wbd_slave_reset_unsuccessful_assoc_count_timer_cb(bcm_usched_handle *hdl, void *arg)
{
	int ret = WBDE_OK;
	ieee1905_unsuccessful_assoc_config_t *param = NULL;
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(arg, WBDE_INV_ARG);
	param = (ieee1905_unsuccessful_assoc_config_t *)arg;
	param->count = 0;
end:
	WBD_EXIT();
}

/* Get multiap R2 channel bandwidth string for the chanspec */
static char
wbd_slave_get_chscan_bw_string(chanspec_t chanspec, char *bw, unsigned char bw_sz)
{
	if (CHSPEC_IS10(chanspec)) {
		snprintf(bw, bw_sz, "%s", MAP_CH_BW_10);
		return strlen(MAP_CH_BW_10);
	} else if (CHSPEC_IS20(chanspec)) {
		snprintf(bw, bw_sz, "%s", MAP_CH_BW_20);
		return strlen(MAP_CH_BW_20);
	} else if (CHSPEC_IS40(chanspec)) {
		snprintf(bw, bw_sz, "%s", MAP_CH_BW_40);
		return strlen(MAP_CH_BW_40);
	} else if (CHSPEC_IS80(chanspec)) {
		snprintf(bw, bw_sz, "%s", MAP_CH_BW_80);
		return strlen(MAP_CH_BW_80);
	} else if (CHSPEC_IS160(chanspec)) {
		snprintf(bw, bw_sz, "%s", MAP_CH_BW_160);
		return strlen(MAP_CH_BW_160);
	} else if (CHSPEC_IS8080(chanspec)) {
		snprintf(bw, bw_sz, "%s", MAP_CH_BW_8080);
		return strlen(MAP_CH_BW_8080);
	}

	return 0;
}

/* Parse and Process the ESCAN EVENT from EAPD */
static int
wbd_slave_process_event_escan(i5_dm_interface_type *evt_ifr, char *ifname, bcm_event_t *pvt_data)
{
	int ret = WBDE_OK;
	bool onboot = FALSE;
	uint32 escan_event_status;
	wl_escan_result_t *escan_data = NULL;
	ieee1905_policy_config *policy_config = ieee1905_get_policy_config();
	i5_dm_device_type *selfdevice = i5DmGetSelfDevice();
	wbd_ifr_item_t *ifr_vndr_data = NULL;
	i5_dm_device_type *i5_device_cntrl = i5DmFindController();
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(ifname, WBDE_INV_ARG);
	WBD_ASSERT_ARG(pvt_data, WBDE_INV_ARG);
	WBD_ASSERT_ARG(evt_ifr, WBDE_INV_ARG);
	WBD_ASSERT_ARG(evt_ifr->vndr_data, WBDE_INV_ARG);

	ifr_vndr_data = (wbd_ifr_item_t*)evt_ifr->vndr_data;

	/* If channel scan is not initiated by MultiAP Controller AND Independent Scan
	 * Reporting is Disabled by the Controller. Do not process further
	 */
	if (!I5_IS_CHSCAN_RUNNING(evt_ifr->flags) &&
		!I5_IS_CHSCAN_INDEP_SCAN_ENAB((&policy_config->chscanrpt_config))) {

		WBD_DEBUG("Channel scan is not running or independent scan "
			"reporting is off. Ifr Flag[%x] Policy[%x]\n",
			evt_ifr->flags, policy_config->chscanrpt_config.chscan_rpt_policy_flag);

		goto end;
	}

	escan_event_status = ntoh32(pvt_data->event.status);
	escan_data = (wl_escan_result_t*)(pvt_data + 1);

	/* Save OnBoot / Requested scan_type flag, as in SUCCESS / ABORT case, it will change */
	onboot = I5_IS_CHSCAN_ONBOOT(evt_ifr->flags) ? TRUE : FALSE;

#ifdef MULTIAP_PLUGFEST
	/* Get NVRAM to check whether onboot scan should be forced or not */
	if (blanket_get_config_val_int(NULL, NVRAM_MAP_CHSCAN_FORCE_ONBOOT, 0)) {
		onboot = TRUE;
	}
#endif /* MULTIAP_PLUGFEST */

	/* Escan not finished */
	if (escan_event_status == WLC_E_STATUS_PARTIAL) {

		wl_bss_info_t *bi = &escan_data->bss_info[0];
		ieee1905_chscan_result_item *chscan_result;
		ieee1905_chscan_result_nbr_item *nbr_bss;
		char ssidbuf[SSID_FMT_BUF_LEN] = "";

		WBD_INFO("ON ESCAN PARTIAL: Result ifname[%s], MAC["MACDBG"] BSSID["MACF"] "
			"RSSI[%d] ctrl_chan[%d]\n", ifname, MAC2STRDBG(evt_ifr->InterfaceId),
			ETHER_TO_MACF(bi->BSSID), bi->RSSI, bi->ctl_ch);

		if (bi->RSSI == WLC_RSSI_INVALID) {
			WBD_DEBUG("\n\nInvalid RSSI\n");
			goto end;
		}

		/* Find the channel number in scan result of a device */
		chscan_result = i5DmFindChannelInScanResult(&selfdevice->stored_chscan_results,
			evt_ifr->InterfaceId, bi->ctl_ch);

		if (chscan_result == NULL) {
			WBD_DEBUG("ON ESCAN PARTIAL: ifname[%s] IFR["MACDBG"] Chan[%d] not "
				"present\n", ifname, MAC2STRDBG(evt_ifr->InterfaceId), bi->ctl_ch);
			goto end;
		}

		/* Check if we've received info of same BSSID */
		nbr_bss = i5DmFindBSSIDInScanResult(chscan_result, (unsigned char*)&bi->BSSID);

		if (!nbr_bss) {
			/* New Neighbor BSS. Allocate memory and save it */
			nbr_bss = (ieee1905_chscan_result_nbr_item*)
				wbd_malloc(sizeof(*nbr_bss), &ret);
			WBD_ASSERT();

			ieee1905_glist_append(&chscan_result->neighbor_list, (dll_t*)nbr_bss);

			chscan_result->num_of_neighbors++;

			WBD_INFO("ON ESCAN PARTIAL: New Neighbor BSS ifname[%s] IFR["MACDBG"] "
				"BSSID["MACF"] Appended\n", ifname,
				MAC2STRDBG(evt_ifr->InterfaceId), ETHER_TO_MACF(bi->BSSID));
		}

		/* Update the Neighbor BSS fields */
		memcpy(nbr_bss->nbr_bssid, bi->BSSID.octet, sizeof(nbr_bss->nbr_bssid));
		if ((bi->SSID_len > 0) && (bi->SSID_len <= sizeof(nbr_bss->nbr_ssid.SSID))) {
			memcpy(nbr_bss->nbr_ssid.SSID, bi->SSID, bi->SSID_len);
			nbr_bss->nbr_ssid.SSID_len = bi->SSID_len;
			wbd_wl_format_ssid(ssidbuf, bi->SSID, bi->SSID_len);
		}
		nbr_bss->nbr_rcpi = (unsigned char)(WBD_RSSI_TO_RCPI(bi->RSSI));
		nbr_bss->ch_bw_length = wbd_slave_get_chscan_bw_string(bi->chanspec,
			(char*)nbr_bss->ch_bw, sizeof(nbr_bss->ch_bw));

		/* Fetch ChanUtil & StaCnt, if BSSLoad Element Present. Else omitted */
		if (dtoh32(bi->ie_length) && (0 == blanket_get_qbss_load_element(bi,
			&nbr_bss->channel_utilization, &nbr_bss->station_count))) {
			nbr_bss->chscan_result_nbr_flag |= MAP_CHSCAN_RES_NBR_BSSLOAD_PRESENT;
		} else {
			nbr_bss->chscan_result_nbr_flag = 0;
			nbr_bss->channel_utilization = 0;
			nbr_bss->station_count = 0;
		}

		chscan_result->scan_status_code = MAP_CHSCAN_STATUS_SUCCESS;
		/* 1 : Scan was an Active scan */
		chscan_result->chscan_result_flag |= MAP_CHSCAN_RES_SCANTYPE;
		chscan_result->noise = (unsigned char)bi->phy_noise;
		wbd_slave_get_chan_util(ifname, &(chscan_result->utilization));

		WBD_INFO("ON ESCAN PARTIAL: NBR_BSSID["MACF"] NBR_SSID[%s] NBR_RCPI[%d] "
			"NBR_CHBW[%s] NBR_SCAN_STATUS[%d] NBR_NOISE[%d] NBR_UTIL[%d]\n",
			ETHERP_TO_MACF(nbr_bss->nbr_bssid), ssidbuf,
			nbr_bss->nbr_rcpi, nbr_bss->ch_bw, chscan_result->scan_status_code,
			chscan_result->noise, chscan_result->utilization);

	}

	/* Escan is finished */
	else if (escan_event_status == WLC_E_STATUS_SUCCESS) {

		WBD_INFO("ON ESCAN SUCCESS: Ifanme[%s] Escan Success!\n", ifname);

		/* Unset Flag indicating Running a Requested Channel Scan on this radio */
		I5DmSetChScanRunning(evt_ifr, onboot ?
			I5_CHSCAN_ONBOOT : I5_CHSCAN_REQUESTED, I5_CHSCAN_STOP);

		/* Update Status Code = SUCCESS, of relavant Results in SelfDevice for a Radio */
		i5DmUpdateChannelScanStatusPerRadio(selfdevice,
			&ifr_vndr_data->last_fresh_chscan_req, -1, MAP_CHSCAN_STATUS_SUCCESS);

	/* Escan aborted */
	} else {

		WBD_INFO("ON ESCAN ABORT: Ifanme[%s] Sync_id: %d, Status:%d, Misc. Error/Abort\n",
			ifname, escan_data->sync_id, escan_event_status);

		/* Unset Flag indicating Running a Requested Channel Scan on this radio */
		I5DmSetChScanRunning(evt_ifr, onboot ?
			I5_CHSCAN_ONBOOT : I5_CHSCAN_REQUESTED, I5_CHSCAN_STOP);

		/* Update Status Code = ABORTED, of relavant Results in SelfDevice for a Radio */
		i5DmUpdateChannelScanStatusPerRadio(selfdevice,
			&ifr_vndr_data->last_fresh_chscan_req, -1, MAP_CHSCAN_STATUS_ABORTED);
	}

	/* If Result is other than PARTIAL, and if it was not OnBoot Scan, Send ChScan Results */
	if ((escan_event_status != WLC_E_STATUS_PARTIAL) && !onboot) {

		ieee1905_chscan_req_msg last_chscan_req;

		/* Initialize Channel Scan Request list from Last Request Data */
		last_chscan_req.chscan_req_msg_flag = MAP_CHSCAN_REQ_FRESH_SCAN;
		last_chscan_req.num_of_radios = 1;
		ieee1905_glist_init(&last_chscan_req.radio_list);
		ieee1905_glist_append(&last_chscan_req.radio_list,
			(dll_t*)&ifr_vndr_data->last_fresh_chscan_req);

		WBD_INFO("ON ESCAN: Sending Requested {FRESH} Channel Scan Report "
			"Message to ["MACF"]\n", ETHERP_TO_MACF(i5_device_cntrl->DeviceId));

		/* Send Channel Scan Report Message to a Multi AP Device */
		ieee1905_send_requested_stored_channel_scan(&last_chscan_req);
	}
end:
	WBD_EXIT();
	return ret;
}
#endif /* MULTIAPR2 */

/* Parse and process the EVENT from EAPD */
static int
wbd_slave_process_event_msg(wbd_info_t* info, char* pkt, int len)
{
	int ret = WBDE_OK, in_band;
	bcm_event_t *pvt_data;
	uint32 datalen;
	struct ether_addr *sta_mac;
	char *ifname;
	struct ether_header *eth_hdr;
	uint8 *data;
	uint16 ether_type;
	uint32 evt_type;
	wbd_slave_item_t *slave = NULL;
	dll_t *slave_item_p;
	i5_dm_interface_type *evt_ifr = NULL;
	i5_dm_device_type *selfdevice = i5DmGetSelfDevice();
	WBD_ENTER();

	ifname = (char *)pkt;
	eth_hdr = (struct ether_header *)(ifname + IFNAMSIZ);

	if ((ether_type = ntohs(eth_hdr->ether_type) != ETHER_TYPE_BRCM)) {
		WBD_WARNING("Ifname[%s] recved non BRCM ether type 0x%x\n", ifname, ether_type);
		ret = WBDE_EAPD_ERROR;
		goto end;
	}

	pvt_data = (bcm_event_t *)(ifname + IFNAMSIZ);
	evt_type = ntoh32(pvt_data->event.event_type);
	sta_mac = (struct ether_addr *)(&(pvt_data->event.addr));
	data = (uint8 *)(pvt_data + 1);
	datalen = ntoh32(pvt_data->event.datalen);

	/* 1. Send the backhaul STA association event to the ieee1905. As the STA interface won't
	 * be available as slave item, checking for WLC_E_ASSOC here only before getting the
	 * slave item.
	 * 2. Parse WLC_E_LINK event and check if the link is down or not
	 */
	if (evt_type == WLC_E_ASSOC || evt_type == WLC_E_REASSOC || evt_type == WLC_E_LINK) {
#if defined(MULTIAPR2)
		multiap_ie_t *map_ie = NULL;
		uint8 ie_type = MAP_IE_TYPE;
#endif /* MULTIAPR2 */
		i5_dm_device_type *self_dev = NULL;
		i5_dm_interface_type *ifr = NULL;
		unsigned short prim_vlan_id = 0;
		int is_8021q_present = 0;
		uint16 flags;

		/* We are not interested in link up event */
		flags = ntoh16(pvt_data->event.flags);
		if (evt_type == WLC_E_LINK && (flags & WLC_EVENT_MSG_LINK)) {
			WBD_DEBUG("Ifname[%s] Event[0x%x] for STA["MACF"]. Link up event\n",
				ifname, evt_type, ETHERP_TO_MACF(sta_mac));
			goto end;
		}

		WBD_INFO("Ifname[%s] Event[0x%x] for STA["MACF"]\n",
			ifname, evt_type, ETHERP_TO_MACF(sta_mac));

#if defined(MULTIAPR2)
		if (info->map_profile >= ieee1905_map_profile2) {
			/* Find multiap IE with WFA_OUI */
			map_ie = (multiap_ie_t*)wbd_wl_find_ie(DOT11_MNG_VS_ID, data, datalen,
				WFA_OUI, &ie_type, 1);
			if (map_ie && map_ie->len > MIN_MAP_IE_LEN) {
				int tlv_len = 0;
				bcm_tlv_t *tlv = NULL;

				tlv_len = map_ie->len;
				for (tlv = (bcm_tlv_t *)map_ie->attr; tlv != NULL;
					tlv = (bcm_tlv_t *)bcm_next_tlv((bcm_tlv_t *)tlv,
						&tlv_len)) {

					if (tlv->id == MAP_8021Q_SETTINGS_SE_ID) {
						multiap_def_8021Q_settings_se_t *set =
							(multiap_def_8021Q_settings_se_t*)tlv;
						prim_vlan_id = dtoh16(set->prim_vlan_id);
						if (prim_vlan_id > 0) {
							is_8021q_present = 1;
						}
						WBD_INFO("ifname[%s] Primary VLAN ID[%d] "
							"is_8021q_present[%d]\n\n\n", ifname,
							prim_vlan_id, is_8021q_present);
					}
				}
			}
		}
#endif /* MULTIAPR2 */

		/* Find the interface for matching ifname */
		WBD_SAFE_GET_I5_SELF_DEVICE(self_dev, &ret);

		foreach_i5glist_item(ifr, i5_dm_interface_type, self_dev->interface_list) {
			if (strcmp(ifr->ifname, ifname) == 0) {
				break;
			}
		}

		if (ifr == NULL) {
			goto end;
		}

		if (I5_IS_BSS_STA(ifr->mapFlags)) {
			if (evt_type == WLC_E_ASSOC || evt_type == WLC_E_REASSOC) {
				WBD_INFO("ifname[%s] associated, so disable roam on other "
					"interface\n", ifname);
				/* Disable roam on other STA interfaces */
				wbd_ieee1905_set_bh_sta_params(IEEE1905_BH_STA_ROAM_DISB_VAP_UP,
					ifr);

				/* Inform MultiAP module to do renew */
				WBD_INFO("%s joined with vlan %d\n", ifname, prim_vlan_id);
				ieee1905_bSTA_associated_to_backhaul_ap(ifr->InterfaceId, ifname,
					prim_vlan_id, is_8021q_present);
			} else if (evt_type == WLC_E_LINK) {
				wbd_slave_process_bhsta_link_down_event(ifr);
			}
		} else {
			WBD_INFO("Ifname[%s] Event[0x%x] for STA["MACF"] "
				"Not a bSTA inteface\n", ifname, evt_type,
				ETHERP_TO_MACF(sta_mac));
		}
		goto end;
	}

	/* Get WBD Band Enumeration from ifname's Chanspec & Bridge Type */
	ret = wbd_slave_identify_band_type(ifname, &in_band);
	WBD_ASSERT();

	/* Probe events will come only on primary interface. For dualband repeater primary
	 * interface of 5G will be sta and won't have slave for that interface. Add sta in
	 * probe sta table and update the band capability of the slave.
	*/
	if (evt_type == WLC_E_PROBREQ_MSG) {
		WBD_PROBE("Band[%d] Ifname[%s] STA["MACF"] Probe Rquest\n",
			in_band, ifname, ETHERP_TO_MACF(sta_mac));
		if (WBD_IS_DUAL_BAND(in_band)) {
			wbd_ds_add_item_to_probe_sta_table(wbd_get_ginfo(), sta_mac, in_band);
		}
		goto end;
	}

	/* Band calculation based on firmware's chanspec can lead to Unknown
	 * slave situation in following scenario.
	 * RADAR detected during CAC duration, in this case Band calculation
	 * on basis of chanspec at firmware lead to unknown slave for the
	 * calculated Band. Hence it lead to ignore event from Valid Slave.
	 * We need to check once again on the basic of ifname of event with
	 * existing slave's ifname. Chanspec can be different e.g.
	 * RADAR detected or CSA to Non DFS chanspec to DFS chanspec.
	 */
	/* Traverse br0 Slave Item List for this Blanket Slave for each band */
	foreach_glist_item(slave_item_p, info->wbd_slave->br0_slave_list) {

		slave = (wbd_slave_item_t*)slave_item_p;
		/* Check if the event is for correct slave or not */
		if (strcmp(ifname, slave->wbd_ifr.ifr.ifr_name) == 0) {
			WBD_DEBUG("Event[0x%x] ifname[%s] Slave [%s] found\n", evt_type,
				ifname, slave->wbd_ifr.ifr.ifr_name);
			break;
		}
		slave = NULL;
	}

	if (!slave) {
		/* We got an event but we dont have a slave on the interface on which
		 * event occured.
		 */
		WBD_INFO("Event[0x%x] ifname[%s]. Unknown Slave\n", evt_type, ifname);
		evt_ifr = i5DmFindInterfaceFromIfname(selfdevice, ifname, strlen(ifname));
		if (evt_ifr == NULL) {
			WBD_INFO("Event[0x%x] ifname[%s]. Ifr Not Present\n", evt_type, ifname);
			ret = WBDE_EAPD_ERROR;
			goto end;
		}
		WBD_INFO("Event[0x%x] ifname[%s]. Ifr Present\n", evt_type, ifname);
	}

	switch (evt_type) {
		/* handle split assoc requests for not allowing repeater to repeater connection */
		case WLC_E_PRE_ASSOC_IND:
		case WLC_E_PRE_REASSOC_IND:
		{
			ret = wbd_wl_send_assoc_decision(ifname, info->flags, data, len,
				sta_mac, evt_type);
			goto end;
		}
		break;

		case WLC_E_DEAUTH:	/* 5 */
		case WLC_E_DEAUTH_IND: /* 6 */
		case WLC_E_DISASSOC_IND: /* 12 */
		{
			uint32 reason;

			reason = ntoh32(pvt_data->event.reason);
			if (!slave) {
				ret = WBDE_EAPD_ERROR;
				goto end;
			}

			ret = blanket_sta_assoc_disassoc(&slave->wbd_ifr.bssid, sta_mac, 0, 0, 1,
				NULL, 0, (uint16)reason);
			if (ret != 0) {
				WBD_WARNING("Band[%d] Slave["MACF"] Ifname[%s] Deauth Event[0x%x] "
					"for STA["MACF"]. Failed to Add to MultiAP Error : %d\n",
					in_band, ETHER_TO_MACF(slave->wbd_ifr.mac), ifname,
					evt_type, ETHERP_TO_MACF(sta_mac), ret);
			}
		}
		break;

		/* update sta info list */
		case WLC_E_ASSOC_REASSOC_IND_EXT:
		{
			multiap_ie_t *map_ie = NULL;
			i5_dm_bss_type *i5_bss = NULL;
			wbd_assoc_sta_item_t *sta = NULL;
			i5_dm_clients_type *i5_assoc_sta = NULL;
#if defined(MULTIAPR2)
			ieee1905_tunnel_msg_t tunnel_msg;
#endif /* MULTIAPR2 */
			bool backhaul_sta = FALSE;
			uint8 ie_type = MAP_IE_TYPE;
			bool reassoc;
			struct dot11_management_header *hdr = (struct dot11_management_header*)data;
			uint8 *frame, *ies;
			uint32 frame_len, ies_len;
			uint32 reason;

			reason = ntoh32(pvt_data->event.reason);
			if (!slave) {
				ret = WBDE_EAPD_ERROR;
				goto end;
			}

			reassoc = ((ltoh16(hdr->fc) & FC_KIND_MASK) == FC_REASSOC_REQ);
			frame = data + sizeof(struct dot11_management_header);
			frame_len = datalen - sizeof(struct dot11_management_header);
			if (reassoc) {
				ies = frame + DOT11_REASSOC_REQ_FIXED_LEN;
				ies_len = frame_len - DOT11_REASSOC_REQ_FIXED_LEN;
			} else {
				ies = frame + DOT11_ASSOC_REQ_FIXED_LEN;
				ies_len = frame_len - DOT11_ASSOC_REQ_FIXED_LEN;
			}

			/* if it is Backhaul_sta, skip */
			map_ie = (multiap_ie_t*)wbd_wl_find_ie(DOT11_MNG_VS_ID, ies, ies_len,
				WFA_OUI, &ie_type, 1);

			if (map_ie && map_ie->len > MIN_MAP_IE_LEN) {
				int tlv_len = 0;
				bcm_tlv_t *tlv = NULL;

				tlv_len = map_ie->len;
				for (tlv = (bcm_tlv_t *)map_ie->attr; tlv != NULL;
					tlv = (bcm_tlv_t *)bcm_next_tlv
					((bcm_tlv_t *)tlv, &tlv_len)) {

					if (tlv->id  == MAP_EXT_ATTR) {
						multiap_ext_attr_t *attr = (multiap_ext_attr_t*)tlv;

						if (attr->attr_val & IEEE1905_BACKHAUL_STA) {
							backhaul_sta = TRUE;
							break;
						}
						 break; /* Only need MAP extension atrribute IE */
					}
				}
			}

			ret = blanket_sta_assoc_disassoc(&slave->wbd_ifr.bssid, sta_mac, 1, 0, 1,
				frame, frame_len, (uint16)reason);
			if (ret != 0) {
				WBD_WARNING("Band[%d] Slave["MACF"] Ifname[%s] Assoc Event[0x%x] "
					"for STA["MACF"]. Failed to Add to MultiAP Error : %d\n",
					in_band, ETHER_TO_MACF(slave->wbd_ifr.mac), ifname,
					evt_type, ETHERP_TO_MACF(sta_mac), ret);
			}

			if (backhaul_sta) {
				WBD_DS_GET_I5_SELF_BSS((unsigned char*)&slave->wbd_ifr.bssid,
					i5_bss, &ret);
				if (!i5_bss) {
					WBD_ERROR("bss should not be NULL ... skip \n");
					goto end;
				}
				i5_assoc_sta = wbd_ds_find_sta_in_bss_assoclist(i5_bss,
					sta_mac, &ret, &sta);
				if (!i5_assoc_sta) {
					WBD_ERROR("client info in MultiAp Database missing\n");
					goto end;
				}
				/* update backhaul contents */
				i5_assoc_sta->flags |= I5_CLIENT_FLAG_BSTA;
			}
			WBD_INFO("Band[%d] Slave["MACF"] Ifname[%s] %sAssoc Event[0x%x] for "
				"STA["MACF"]. DataLen[%d] FrameLen[%d] IELen[%d]\n",
				in_band, ETHER_TO_MACF(slave->wbd_ifr.mac), ifname,
				reassoc ? "Re" : "", evt_type, ETHERP_TO_MACF(sta_mac),
				datalen, frame_len, ies_len);
#if defined(MULTIAPR2)
			/* send tunnel message to controller for (re)assoc event from
			 * sta
			 */
			memset(&tunnel_msg, 0, sizeof(tunnel_msg));
			WBD_INFO(" rcvd notification frame from sta["MACF"] of len [%d]\n",
				ETHERP_TO_MACF(sta_mac), datalen);

			memcpy(&tunnel_msg.source_mac, sta_mac, ETHER_ADDR_LEN);
			tunnel_msg.payload = data;
			tunnel_msg.payload_len = datalen;

			tunnel_msg.payload_type = (reassoc ?
				ieee1905_tunnel_msg_payload_re_assoc_rqst :
				ieee1905_tunnel_msg_payload_assoc_rqst);

			WBD_INFO(" send tunnel msg: payload type[%d] for sta["MACF"] "
				"payload len[%d] \n", tunnel_msg.payload_type,
				ETHERP_TO_MACF(sta_mac), datalen);

			wbd_slave_send_tunneled_msg(&tunnel_msg);
#endif /* MULTIAPR2 */
		}
		break;

		case WLC_E_RADAR_DETECTED:
		{
			i5_dm_device_type *self_dev = NULL;
			i5_dm_interface_type *ifr = NULL;
			i5_dm_bss_type *bss = NULL;
			wl_event_radar_detect_data_t *radar_data = NULL;
			int ret = WBDE_OK;

			WBD_SAFE_GET_I5_SELF_DEVICE(self_dev, &ret);

			radar_data = (wl_event_radar_detect_data_t*)data;
			if (!radar_data) {
				WBD_DEBUG(" Invalid event data passed at radar, exit\n");
				goto end;
			}
			foreach_i5glist_item(ifr, i5_dm_interface_type, self_dev->interface_list) {
				if (!i5DmIsInterfaceWireless(ifr->MediaType) ||
					!ifr->BSSNumberOfEntries) {
					continue;
				}

				/* Loop for all the BSSs in Interface */
				foreach_i5glist_item(bss, i5_dm_bss_type, ifr->bss_list) {
					if (!strncmp(bss->ifname, ifname, strlen(bss->ifname))) {
						WBD_INFO(" RADAR event for %s on chanspec 0x%x\n",
							bss->ifname, ifr->chanspec);
						ifr->chanspec = radar_data->target_chanspec;
						break;
					}
				}
				if (bss) {
					break;
				}
			}
			if (!bss) {
				WBD_DEBUG(" skip RADAR event, interface not found \n");
				goto end;
			}

			WBD_DEBUG("Band[%d] Slave["MACF"] Ifname[%s] RADAR DETECTED, current "
				"chanspec[0x%x], target chanspec[0x%x]\n", in_band,
				ETHER_TO_MACF(slave->wbd_ifr.mac), ifname,
				radar_data->current_chanspec, radar_data->target_chanspec);

			/* Send channel pref report with current radar channel as inoperable */
			ieee1905_send_chan_preference_report();
			wbd_slave_send_operating_chan_report(ifr);
			ieee1905_notify_channel_change(ifr);
			wbd_slave_add_ifr_nbr(ifr, TRUE);
			/* check and send chan info to controller */
			wbd_slave_chk_and_send_chan_config_info(ifr, FALSE);
		}
		break;
		case WLC_E_RRM:
		{
			if (!slave) {
				ret = WBDE_EAPD_ERROR;
				goto end;
			}

			WBD_DEBUG("Band[%d] Slave["MACF"] Ifname[%s] RRM EVENT\n", in_band,
				ETHER_TO_MACF(slave->wbd_ifr.mac), ifname);
			wbd_slave_store_beacon_report(slave, data, sta_mac);
		}
		break;

		case WLC_E_AP_CHAN_CHANGE:
		{
			/* Firmware generates and share AP_CHAN_CHANGE event with reason based
			 * on following condition:
			 *
			 * 1: CSA to DFS Radar channel
			 *
			 * 2: DFS_AP_MOVE start on DFS Radar channel
			 *
			 * 3: DFS_AP_MOVE radar or abort to indicate stop Scanning
			 *
			 *    Reasons from Firmware are:
			 *
			 *    a: REASON_CSA
			 *    b: REASON_DFS_AP_MOVE_START
			 *    c: REASON_DFS_AP_MOVE_RADAR_FOUND
			 *    d: REASON_DFS_AP_MOVE_ABORT
			 *    e: REASON_DFS_AP_MOVE_SUCCESS
			 *    f: REASON_DFS_AP_MOVE_STUNT
			 *
			 *    DFS_AP_MOVE_START reason: Master initiates dfs_ap_move across all
			 *    repeaters running in slave mode. Present impelentation only supports
			 *    dfs_ap_move initiation only through Master AP.
			 *
			 *    DFS_AP_MOVE_RADAR and ABORT reason: Both Master and Slave repeater
			 *    can generate this reason, on reception Master stops dfs_ap_move
			 *    activity at all other repeaters.
			 *
			 *    CSA reason: Both Master and Slave can generate event with this reason,
			 *    On reception Master informs and initiates CSA at all other repeaters.
			 *
			 *    DFS_AP_MOVE_SUCCESS reason: To inform and update slave's new chanspec,
			 *    Each slave recieves this reason on completion and send message to
			 *    Master. On reception Master updates slave's chancpec in it's database.
			 *
			 */
			chanspec_t current_chanspec;
			char *ifname = NULL;
			wl_event_change_chan_t *evt_data = NULL;
			wl_chan_change_reason_t reason;
			wbd_cmd_vndr_zwdfs_msg_t zwdfs_msg;
			i5_dm_interface_type *pdmif;
			i5_dm_device_type *i5_self_device = NULL;
			wbd_ifr_item_t *ifr_vndr_data = NULL;

			if (!slave) {
				ret = WBDE_EAPD_ERROR;
				goto end;
			}

			ifname = slave->wbd_ifr.ifr.ifr_name;
			evt_data = (wl_event_change_chan_t*)data;
			if ((evt_data->length != WL_CHAN_CHANGE_EVENT_LEN_VER_1) ||
				(evt_data->version != WL_CHAN_CHANGE_EVENT_VER_1)) {

				WBD_ERROR("Band[%d] Slave["MACF"] Ifname[%s] WLC_E_AP_CHAN_CHANGE"
				"event skipped. Length or version mismatch \n", in_band,
				ETHER_TO_MACF(slave->wbd_ifr.mac), ifname);

				break;
			}
			reason = evt_data->reason;

			WBD_SAFE_GET_I5_SELF_DEVICE(i5_self_device, &ret);
			pdmif = wbd_ds_get_i5_interface((uchar*)i5_self_device->DeviceId,
					(uchar*)&slave->wbd_ifr.mac, &ret);

			if (!pdmif) {
				WBD_ERROR(" No valid interface exist for this event, skip \n");
				goto end;
			}
			ifr_vndr_data = (wbd_ifr_item_t*)pdmif->vndr_data;
			if (!ifr_vndr_data) {
				WBD_ERROR(" Invalid ifr vendor data plz Debug, exiting ..\n");
				goto end;
			}
			/* ignore the event if slave has itself initiated in response to
			 * command from wbd master
			 */
			if ((reason == WL_CHAN_REASON_DFS_AP_MOVE_START) &&
				(isset(slave->dfs_event, IGNORE_DFS_AP_MOVE_START))) {
				clrbit(slave->dfs_event, IGNORE_DFS_AP_MOVE_START);
				break;
			}

			if ((reason == WL_CHAN_REASON_DFS_AP_MOVE_RADAR_FOUND) &&
				(isset(slave->dfs_event, IGNORE_DFS_AP_MOVE_RADAR_FOUND))) {
				clrbit(slave->dfs_event, IGNORE_DFS_AP_MOVE_RADAR_FOUND);
				break;
			}

			if ((reason == WL_CHAN_REASON_DFS_AP_MOVE_ABORTED) &&
				(isset(slave->dfs_event, IGNORE_DFS_AP_MOVE_ABORTED))) {
				clrbit(slave->dfs_event, IGNORE_DFS_AP_MOVE_ABORTED);

#if defined(MULTIAPR2)
				if (ifr_vndr_data->cac_rqst_status & MAP_CAC_RQST_RUNNING) {
					/* Mark cac status ABORT, with Reset of
					 * MAP_CAC_RQST_RCVD and MAP_CAC_RQST_RUNNING
					 * Reset MAP_CAC_RQST_RCVD as cac complete TLV
					 * for this radio is not required anymore.
					 */
					ifr_vndr_data->cac_rqst_status |= MAP_CAC_RQST_ABORT;
					ifr_vndr_data->cac_rqst_status &=
						~(MAP_CAC_RQST_RUNNING | MAP_CAC_RQST_RCVD);

				}
#endif /* MULTIAPR2 */

				break;
			}

			if ((reason == WL_CHAN_REASON_DFS_AP_MOVE_STUNT) &&
				(isset(slave->dfs_event, IGNORE_DFS_AP_MOVE_STUNT))) {
				clrbit(slave->dfs_event, IGNORE_DFS_AP_MOVE_STUNT);
				break;
			}

			if ((reason == WL_CHAN_REASON_CSA) &&
				(isset(slave->dfs_event, IGNORE_CSA))) {
				clrbit(slave->dfs_event, IGNORE_CSA);
				break;
			}
			/* for CSA, DFS_AP_MOVE_STOP update local slave's chanspec, for
			 * DFS_AP_MOVE_START first inform Master for the target chanspec and
			 * later update with current chanspec as dfs_ap_move operation can fail
			 */
			current_chanspec = slave->wbd_ifr.chanspec;

			WBD_INFO("AP_CHAN_CHANGE event: Band[%d] Slave["MACF"] Ifname[%s],"
				" Firmware reason[%d] current chanspec[0x%x], target"
				" chanspec[0x%x]\n", in_band, ETHER_TO_MACF(slave->wbd_ifr.mac),
				ifname, reason, current_chanspec, evt_data->target_chanspec);

			/* prepare zwdfs msg, forward to 1905 to be passed to controller */
			memset(&zwdfs_msg, 0, sizeof(zwdfs_msg));

			blanket_get_global_rclass(evt_data->target_chanspec, &(zwdfs_msg.opclass));

			zwdfs_msg.cntrl_chan = wf_chspec_ctlchan(evt_data->target_chanspec);
			zwdfs_msg.reason = evt_data->reason;
			memcpy(&zwdfs_msg.mac, &slave->wbd_ifr.mac, ETHER_ADDR_LEN);

			/* ignore DFS_AP_MOVE success call to controller */
			if ((reason == WL_CHAN_REASON_DFS_AP_MOVE_SUCCESS) ||
				reason == WL_CHAN_REASON_DFS_AP_MOVE_STUNT_SUCCESS) {
#if defined(MULTIAPR2)
				WBD_DEBUG("complete dfs_ap_move success or stunt success: rc[%d] "
					"cac status[%x]\n", reason,
					ifr_vndr_data->cac_rqst_status);

				if (ifr_vndr_data->cac_rqst_status & MAP_CAC_RQST_RUNNING) {
					ifr_vndr_data->cac_rqst_status |=
						(MAP_CAC_RQST_SUCCESS | MAP_CAC_RQST_COMPLETE);

					ifr_vndr_data->cac_rqst_status &= ~MAP_CAC_RQST_RUNNING;
					WBD_DEBUG("send chan preference report \n");

					/* send channel preference report to controller */
					ieee1905_send_chan_preference_report();
				}
#endif /* MULTIAPR2 */
				break;
			}
			wbd_slave_send_vendor_msg_zwdfs(&zwdfs_msg);
		}
		break;

#if defined(MULTIAPR2)
		case WLC_E_MBO_CAPABILITY_STATUS:
		{
			i5_dm_device_type *self_dev = NULL;
			i5_dm_interface_type *ifr = NULL;
			i5_dm_bss_type *bss = NULL;
			wlc_mbo_bss_status_t *mbo_status = NULL;
			wbd_bss_item_t *wbd_bss = NULL;

			WBD_SAFE_GET_I5_SELF_DEVICE(self_dev, &ret);

			mbo_status = (wlc_mbo_bss_status_t *)data;
			if (!mbo_status) {
				WBD_DEBUG(" Invalid event data passed for mbo_bss_status, exit\n");
				goto end;
			}

			foreach_i5glist_item(ifr, i5_dm_interface_type, self_dev->interface_list) {
				if (!i5DmIsInterfaceWireless(ifr->MediaType) ||
					!ifr->BSSNumberOfEntries) {
					continue;
				}
				bss = wbd_ds_get_i5_bss_match_ifname_in_ifr(ifr,
					(unsigned char*)ifname, &ret);
				if (bss) {
					break;
				}
			}
			if (!bss) {
				WBD_DEBUG("skip MBO Capability status event, bss not found \n");
				goto end;
			}
			WBD_DEBUG("Band[%d] Slave["MACF"] Ifname[%s], mbo_bss_status "
				"assoc_allowance_status[%d], ap_attr[%d]\n", in_band,
				ETHER_TO_MACF(slave->wbd_ifr.mac), ifname,
				mbo_status->assoc_allowance_status, mbo_status->ap_attr);

			bss->assoc_allowance_status = !mbo_status->assoc_allowance_status;
			/* update bss's mbo params */
			wbd_bss = (wbd_bss_item_t*)bss->vndr_data;
			wbd_bss->mbo_params.ap_attr = mbo_status->ap_attr;
			wbd_bss->mbo_params.mbo_enable = mbo_status->mbo_bss_enable;
			/* send association notification request message to controller */
			wbd_slave_send_association_status_notification(bss);
		}
		break;

		case WLC_E_WNM_NOTIFICATION_REQ:
		case WLC_E_WNM_BSSTRANS_QUERY:
		case WLC_E_GAS_RQST_ANQP_QUERY:
		case WLC_E_BSSTRANS_QUERY:
		{
			ieee1905_tunnel_msg_t tunnel_msg;

			memset(&tunnel_msg, 0, sizeof(tunnel_msg));
			WBD_INFO(" rcvd notification frame from sta["MACF"] of len [%d]\n",
				ETHERP_TO_MACF(sta_mac), datalen);

			memcpy(&tunnel_msg.source_mac, sta_mac, ETHER_ADDR_LEN);
			tunnel_msg.payload = data;
			tunnel_msg.payload_len = datalen;
			if (evt_type == WLC_E_WNM_NOTIFICATION_REQ) {
				tunnel_msg.payload_type = ieee1905_tunnel_msg_payload_wnm_rqst;
			} else if ((evt_type == WLC_E_WNM_BSSTRANS_QUERY) || (evt_type == WLC_E_BSSTRANS_QUERY)) {
				tunnel_msg.payload_type = ieee1905_tunnel_msg_payload_btm_query;
			} else if (evt_type == WLC_E_GAS_RQST_ANQP_QUERY) {
				tunnel_msg.payload_type = ieee1905_tunnel_msg_payload_anqp_rqst;
			} else {
				WBD_INFO("unknown evt_type[%d], dont process, exit \n", evt_type);
				/* Unknown event, dont process further, exit */
				break;
			}
			wbd_slave_send_tunneled_msg(&tunnel_msg);

			/* Send BTM request from application for the BTM Query from a STA */
			if ((info->wbd_slave->flags & WBD_BKT_SLV_FLAGS_WNM_NO_BTQ_RESP) &&
				((evt_type == WLC_E_WNM_BSSTRANS_QUERY) || (evt_type == WLC_E_BSSTRANS_QUERY))) {
				wbd_slave_send_bss_trans_for_btm_query(sta_mac);
			}
		}
		break;

		case WLC_E_ASSOC_FAIL:	     /* 188 Association fail event */
		case WLC_E_REASSOC_FAIL:     /* 189 Re-association Fail event */
		case WLC_E_AUTH_FAIL:	     /* 190 Authentication fail event */
		{
			/* Unsuccessful Association Policy TLV with the Report Unsuccessful
			 * Associations bit set to 1, and if the Multi-AP Agent has sent
			 * fewer than the maximum number of Failed Connection notifications
			 * in the preceding minute, and the Multi-AP Agent detects that Wi-Fi
			 * client has made a failed attempt to connect to any BSS operated by
			 * the Multi-AP Agent, the Multi-AP Agent shall send to the
			 * Multi-AP Controller a Failed Connection message including a STA
			 * MAC Address TLV identifying the client that has attempted to connect
			 * and a Status Code TLV with the Status Code set to a non-zero value that
			 * indicates the reason for association or authentication failure or a
			 * Status Code TLV with the Status Code set to zero and a Reason Code TLV
			 * with the Reason Code indicating the reason the STA was disassociated
			 * or deauthenticated.
			 */
			uint32 reason;

			reason = ntoh32(pvt_data->event.reason);
			if (info->wbd_slave->unsuccessful_assoc_config.report_flag &
				MAP_UNSUCCESSFUL_ASSOC_FLAG_REPORT) {
				if (info->wbd_slave->unsuccessful_assoc_config.count <
					info->wbd_slave->unsuccessful_assoc_config.max_reporting_rate) {
					ret = blanket_sta_assoc_failed_connection(sta_mac, (uint16)reason);

					if (ret != 0) {
						WBD_WARNING("Band[%d] Slave["MACF"] Ifname[%s] "
							"Unsuccessful Association Event[0x%x] "
							"for STA["MACF"]. Failed to Add to "
							"MultiAP Error : %d\n", in_band,
							ETHER_TO_MACF(slave->wbd_ifr.mac), ifname,
							evt_type, ETHERP_TO_MACF(sta_mac), ret);
					} else {
						info->wbd_slave->unsuccessful_assoc_config.count++;
					}
				}
			}
		}

		/* To Send Channel Scan Results */
		case WLC_E_ESCAN_RESULT:
		{
			if (!evt_ifr) {
				evt_ifr = i5DmFindInterfaceFromIfname(selfdevice, ifname,
					strlen(ifname));
				if (!evt_ifr) {
					WBD_DEBUG("ifname %s not present in IFR List\n", ifname);
					goto end;
				}
			}

			/* Parse and Process the ESCAN EVENT from EAPD */
			wbd_slave_process_event_escan(evt_ifr, ifname, pvt_data);
			break;
		}
#endif /* MULTIAPR2 */

		case WLC_E_CAC_STATE_CHANGE:
		{
			wlc_cac_event_t *cac_event = NULL;
			wl_dfs_status_all_t *all = NULL;
			wl_dfs_sub_status_t *sub0 = NULL;
			wbd_ifr_item_t *ifr_vndr_data = NULL;

			if (!evt_ifr) {
				evt_ifr = i5DmFindInterfaceFromIfname(selfdevice, ifname,
					strlen(ifname));
				if (!evt_ifr) {
					WBD_DEBUG("ifname %s not present in IFR List\n", ifname);
					goto end;
				}
			}

			ifr_vndr_data = (wbd_ifr_item_t*)evt_ifr->vndr_data;

			if (!ifr_vndr_data) {
				WBD_ERROR(" Invalid ifr vendor data plz Debug, exiting ..\n");
				goto end;
			}

			/* extract event information */
			cac_event = (wlc_cac_event_t *)data;
			all = (wl_dfs_status_all_t*)&(cac_event->scan_status);
			sub0 = &(all->dfs_sub_status[0]);

			/* as of now monitoring only for full time CAC, so sub0 is sufficient */
			if (sub0->state == WL_DFS_CACSTATE_PREISM_CAC) {
				WBD_DEBUG("FULL Time CAC started, wait for CACSTATE_ISM state \n");
				break;
			}
			if (sub0->state == WL_DFS_CACSTATE_ISM) {
#ifdef MULTIAPR2
				/* CAC state over, and DFS state machine switched
				 * to ISM with success
				 */
				if (ifr_vndr_data->cac_rqst_status & MAP_CAC_RQST_RUNNING) {
					ifr_vndr_data->cac_rqst_status |=
						(MAP_CAC_RQST_SUCCESS | MAP_CAC_RQST_COMPLETE);

					ifr_vndr_data->cac_rqst_status &= ~MAP_CAC_RQST_RUNNING;
					WBD_DEBUG("NEW CAC state change is ISM for["MACF"], "
						"cac_rqst_staus[%x] \n",
						ETHERP_TO_MACF(pdmif->InterfaceId),
						ifr_vndr_data->cac_rqst_status);
				}
#endif /* MULTIAPR2 */

				WBD_DEBUG("ifname[%s] send chan preference report \n", ifname);

				/* send channel preference report to controller */
				ieee1905_send_chan_preference_report();
			}
			break;
		}

		default:
			WBD_INFO("Band[%d] Slave["MACF"] Ifname[%s] Event[0x%x] UnKnown Event\n",
				in_band, ETHER_TO_MACF(slave->wbd_ifr.mac), ifname, evt_type);
			break;
	}

end: /* Check Slave Pointer before using it below */

	/* good packet may be this is destined to us, if no error */
	WBD_EXIT();
	return ret;
}

/* Callback function called from scheduler library */
/* to Process event by accepting the connection and processing the client */
void
wbd_slave_process_event_fd_cb(bcm_usched_handle *handle, void *arg, bcm_usched_fds_entry_t *entry)
{
	int ret = WBDE_OK, rcv_ret;
	wbd_info_t *info = (wbd_info_t*)arg;
	char read_buf[WBD_BUFFSIZE_4K] = {0};
	WBD_ENTER();

	BCM_REFERENCE(ret);
	/* Get the data from client */
	rcv_ret = wbd_socket_recv_bindata(info->event_fd, read_buf, sizeof(read_buf));
	if ((rcv_ret <= 0)) {
		WBD_WARNING("Failed to recieve event. Error code : %d\n", rcv_ret);
		ret = WBDE_EAPD_ERROR;
		goto end;
	}
	/* Parse and process the EVENT from EAPD */
	ret = wbd_slave_process_event_msg(info, read_buf, rcv_ret);
end:
	WBD_EXIT();
}

/* Retrieve fresh stats from driver for the associated STAs and update locally.
 * If sta_mac address is provided get the stats only for that STA
 * Else if isweakstas is TRUE, get only for weak STAs else get for all the associated STAs
 */
int
wbd_slave_update_assoclist_fm_wl(i5_dm_bss_type *i5_bss, struct ether_addr *sta_mac,
	int isweakstas)
{
	int ret = WBDE_OK;
	wbd_assoc_sta_item_t* sta_item;
	wbd_assoc_sta_item_t outitem;
	i5_dm_clients_type *i5_sta;
	WBD_ENTER();

	/* Travese STA List associated with this BSS */
	foreach_i5glist_item(i5_sta, i5_dm_clients_type, i5_bss->client_list) {

		sta_item = (wbd_assoc_sta_item_t*)i5_sta->vndr_data;
		if (sta_item == NULL) {
			WBD_WARNING("STA["MACDBG"] NULL Vendor Data\n", MAC2STRDBG(i5_sta->mac));
			continue;
		}

		/* If STA mac address is provided, get the stats only for that STA.
		 * Else Check if the stats to be get only for WEAK STAs or for all the STAs
		 */
		if (sta_mac != NULL) {
			if (memcmp(sta_mac, i5_sta->mac, MAC_ADDR_LEN))
				continue;
		} else if ((isweakstas == TRUE) && (sta_item->status != WBD_STA_STATUS_WEAK)) {
			continue;
		}

		/* Initialize local variables for this iteration */
		memset(&outitem, 0, sizeof(outitem));

		/* Get the STA info from WL driver for this STA item */
		if ((ret = wbd_slave_fill_sta_stats(i5_bss->ifname,
			(struct ether_addr*)&i5_sta->mac, &(sta_item->stats))) != WBDE_OK) {
			WBD_WARNING("BSS["MACDBG"] STA["MACDBG"] Wl fill stats failed. WL "
				"error : %s\n", MAC2STRDBG(i5_bss->BSSID),
				MAC2STRDBG(i5_sta->mac), wbderrorstr(ret));
			continue;
		}

		/* Now update the stats to be shared with Master */
		sta_item->stats.tx_rate = outitem.stats.tx_rate;
		sta_item->stats.rssi = outitem.stats.rssi;
	}

	WBD_EXIT();
	return ret;
}

#ifdef PLC_WBD
/* Retrieve fresh stats from PLC for all the PLC associated nodes and update locally */
static int
wbd_slave_update_plc_assoclist(wbd_slave_item_t *slave)
{
	wbd_plc_assoc_info_t *assoc_info = NULL;
	wbd_plc_sta_item_t *new_plc_sta;
	wbd_plc_info_t *plc_info;
	int ret = WBDE_OK;
	int i;

	WBD_ENTER();

	plc_info = &slave->parent->parent->plc_info;
	/* Do nothing if PLC is disabled */
	if (!plc_info->enabled) {
		ret = WBDE_PLC_DISABLED;
		goto end;
	}

	/* Get the assoclist */
	ret = wbd_plc_get_assoc_info(plc_info, &assoc_info);
	if (ret != WBDE_OK) {
		WBD_WARNING("Band[%d] Slave["MACF"]. Failed to get PLC assoc info : %s\n",
			slave->band, ETHER_TO_MACF(slave->wbd_ifr.mac), wbderrorstr(ret));
		goto end;
	}

	for (i = 0; i < assoc_info->count; i++) {
		WBD_INFO("Band[%d] Slave["MACF"] PLC rate to "MACF": tx %f rx %f\n",
			slave->band, ETHER_TO_MACF(slave->wbd_ifr.mac),
			ETHER_TO_MACF(assoc_info->node[i].mac),
			assoc_info->node[i].tx_rate,
			assoc_info->node[i].rx_rate);
	}

	/* Clear the list and add them */
	wbd_ds_slave_item_cleanup_plc_sta_list(slave);

	/* Add remote plc_sta to plc_sta_list */
	for (i = 0; i < assoc_info->count; i++) {
		/* Create a PLC STA item */
		new_plc_sta = (wbd_plc_sta_item_t*) wbd_malloc(sizeof(*new_plc_sta), &ret);
		WBD_ASSERT_MSG("Band[%d] Slave["MACF"] STA["MACF"]. %s\n", slave->band,
			ETHER_TO_MACF(slave->wbd_ifr.mac), ETHER_TO_MACF(new_plc_sta->mac),
			wbderrorstr(ret));

		/* Fill & Initialize PLC STA item data */
		eacopy(&assoc_info->node[i].mac, &new_plc_sta->mac);
		new_plc_sta->tx_rate = assoc_info->node[i].tx_rate;
		new_plc_sta->rx_rate = assoc_info->node[i].rx_rate;

		/* Add this new PLC STA item to plc_sta_list of this Slave */
		wbd_ds_glist_append(&slave->plc_sta_list, (dll_t *) new_plc_sta);
	}

end:
	if (assoc_info)
		free(assoc_info);

	WBD_EXIT();
	return ret;
}
#endif /* PLC_WBD */

/* Processes WEAK_CLIENT response */
static void
wbd_slave_process_weak_client_cmd_resp(unsigned char *neighbor_al_mac,
	unsigned char *tlv_data, unsigned short tlv_data_len)
{
	int ret = WBDE_OK;
	wbd_cmd_weak_client_resp_t cmdrsp;
	wbd_assoc_sta_item_t *sta = NULL;
	i5_dm_bss_type *i5_bss = NULL;
	WBD_ENTER();

	/* Validate Message data */
	WBD_ASSERT_MSGDATA(tlv_data, "WEAK_CLIENT_RESP");

	memset(&cmdrsp, 0, sizeof(cmdrsp));

	/* Decode Vendor Specific TLV for Message : weak client response on receive */
	ret = wbd_tlv_decode_weak_client_response((void *)&cmdrsp, tlv_data, tlv_data_len);
	if (ret != WBDE_OK) {
		WBD_WARNING("Failed to decode the weak client response TLV\n");
		goto end;
	}

	WBD_INFO("Weak Client Vendor Specific Command Response : BSSID["MACF"] STA["MACF"] "
		"ErrorCode[%d] Dwell[%d]\n",
		ETHER_TO_MACF(cmdrsp.BSSID), ETHER_TO_MACF(cmdrsp.sta_mac),
		cmdrsp.error_code, cmdrsp.dwell_time);

	WBD_SAFE_GET_I5_SELF_BSS((unsigned char*)&cmdrsp.BSSID, i5_bss, &ret);

	/* Get STA pointer of Weak client */
	if (wbd_ds_find_sta_in_bss_assoclist(i5_bss, &cmdrsp.sta_mac, &ret, &sta) == NULL) {
		WBD_WARNING("Slave["MACDBG"] STA["MACF"]. %s\n",
			MAC2STRDBG(i5_bss->BSSID), ETHER_TO_MACF(cmdrsp.sta_mac),
			wbderrorstr(ret));
		goto end;
	}

	/* Save the response code and dwell time got from the master on weak client response */
	sta->error_code = wbd_wc_resp_reason_code_to_error(cmdrsp.error_code);
	sta->dwell_time = cmdrsp.dwell_time;

	/* If the STA is bouncing STA, update the dwell start time */
	if (sta->error_code == WBDE_DS_BOUNCING_STA) {
		sta->dwell_start = time(NULL);
		WBD_INFO("Slave["MACDBG"] STA["MACF"] error[%d] dwell[%lu] dwell_start[%lu]\n",
			MAC2STRDBG(i5_bss->BSSID), ETHER_TO_MACF(cmdrsp.sta_mac),
			sta->error_code, (unsigned long)sta->dwell_time,
			(unsigned long)sta->dwell_start);
	}

	if (sta->error_code == WBDE_IGNORE_STA) {
		/* If the error code is ignore update the status */
		sta->status = WBD_STA_STATUS_IGNORE;
		WBD_DEBUG("Slave["MACDBG"] Updated STA["MACF"] as Ignored\n",
			MAC2STRDBG(i5_bss->BSSID), ETHER_TO_MACF(cmdrsp.sta_mac));
	} else if (sta->error_code != WBDE_OK) {
		sta->status = WBD_STA_STATUS_NORMAL;
		WBD_DEBUG("Slave["MACDBG"] Updated STA["MACF"] as Normal Due to Error[%s] "
			"from Master\n", MAC2STRDBG(i5_bss->BSSID),
			ETHER_TO_MACF(cmdrsp.sta_mac), wbderrorstr(sta->error_code));
	}

end: /* Check Slave Pointer before using it below */

	WBD_EXIT();
}

/* Send WEAK_CLIENT command and Update STA Status = Weak */
int
wbd_slave_send_weak_client_cmd(i5_dm_bss_type *i5_bss, i5_dm_clients_type *i5_assoc_sta)
{
	int ret = WBDE_OK;
	i5_dm_device_type *controller_device = NULL;
	wbd_assoc_sta_item_t *assoc_sta;
	WBD_ENTER();

	assoc_sta = (wbd_assoc_sta_item_t*)i5_assoc_sta->vndr_data;
	WBD_SAFE_GET_I5_CTLR_DEVICE(controller_device, &ret);

	ret = ieee1905_send_assoc_sta_link_metric(controller_device->DeviceId, i5_assoc_sta->mac);

	WBD_CHECK_ERR_MSG(WBDE_COM_ERROR, "Slave["MACDBG"] STA["MACDBG"] Failed to send "
		"WEAK_CLIENT, Error : %s\n", MAC2STRDBG(i5_bss->BSSID),
		MAC2STRDBG(i5_assoc_sta->mac), wbderrorstr(ret));

end:
	if (assoc_sta) {
		assoc_sta->error_code = WBDE_OK;

		/* If successfully sent WEAK_CLIENT, Update STA's Status = Weak */
		if (ret == WBDE_OK) {
			assoc_sta->status = WBD_STA_STATUS_WEAK;

#ifdef BCM_APPEVENTD
			/* Send weak client event to appeventd. */
			wbd_appeventd_weak_sta(APP_E_WBD_SLAVE_WEAK_CLIENT, i5_bss->ifname,
				(struct ether_addr*)&i5_assoc_sta->mac, assoc_sta->stats.rssi,
				assoc_sta->stats.tx_failures, assoc_sta->stats.tx_rate);
#endif /* BCM_APPEVENTD */
		}
	} else {
		WBD_WARNING("STA["MACDBG"] NULL Vendor Data\n", MAC2STRDBG(i5_assoc_sta->mac));
	}

	WBD_EXIT();
	return ret;
}

/* Send WEAK_CANCEL command and Update STA Status = Normal */
int
wbd_slave_send_weak_cancel_cmd(i5_dm_bss_type *i5_bss, i5_dm_clients_type *i5_assoc_sta)
{
	int ret = WBDE_OK;
	i5_dm_device_type *controller_device;
	wbd_assoc_sta_item_t *assoc_sta;
	WBD_ENTER();

	WBD_SAFE_GET_I5_CTLR_DEVICE(controller_device, &ret);

	ret = ieee1905_send_assoc_sta_link_metric(controller_device->DeviceId, i5_assoc_sta->mac);

	WBD_CHECK_ERR_MSG(WBDE_COM_ERROR, "Slave["MACDBG"] STA["MACDBG"] Failed to send "
		"WEAK_CANCEL, Error : %s\n", MAC2STRDBG(i5_bss->BSSID),
		MAC2STRDBG(i5_assoc_sta->mac), wbderrorstr(ret));

end:
	/* If successfully sent WEAK_CANCEL, Update STA's Status = Normal */
	if (ret == WBDE_OK) {
		assoc_sta = (wbd_assoc_sta_item_t*)i5_assoc_sta->vndr_data;
		if (assoc_sta == NULL) {
			WBD_WARNING("STA["MACDBG"] NULL Vendor Data\n",
				MAC2STRDBG(i5_assoc_sta->mac));
		} else {
			assoc_sta->status = WBD_STA_STATUS_NORMAL;

			/* Reset the Steer fail count also */
			assoc_sta->steer_fail_count = 0;
		}
	}

	WBD_EXIT();
	return ret;
}

/* Set mode of ACS deamon running on Repeater to Fixed Chanspec */
int
wbd_slave_set_acsd_mode_to_fixchspec(i5_dm_interface_type *pdmif)
{
	wbd_info_t *info = NULL;
	wbd_com_handle *com_hndl = NULL;
	int ret = WBDE_OK, mode;
	char tmpbuf[WBD_MAX_BUF_512];
	int sock_options = 0x0000;
	WBD_ENTER();

	info = wbd_get_ginfo();

	WBD_ASSERT_ARG(pdmif, WBDE_INV_ARG);

	/* Set ACS daemon's mode to Fixed Chanspec, only for Repeater not at RootAP */
	if (MAP_IS_CONTROLLER(info->map_mode)) {
		goto end;
	}

	mode = 5; /* ACS_MODE_FIXCHSPEC */

	snprintf(tmpbuf, sizeof(tmpbuf), "set&ifname=%s&param=mode&value=%d", pdmif->ifname, mode);

	sock_options =	WBD_COM_FLAG_CLIENT | WBD_COM_FLAG_BLOCKING_SOCK
			| WBD_COM_FLAG_NO_RESPONSE;

	com_hndl = wbd_com_init(info->hdl, INVALID_SOCKET, sock_options, NULL, NULL, info);

	if (!com_hndl) {
		WBD_ERROR("Band[%d] interface["MACF"] Failed to initialize the communication"
			"module for setting Fixed Chanspec[0x%x] Mode to ACSD\n", pdmif->band,
			ETHERP_TO_MACF(&pdmif->InterfaceId), pdmif->chanspec);
		ret = WBDE_COM_ERROR;
		goto end;
	}

	/* Send the command to ACSD */
	ret = wbd_com_connect_and_send_cmd(com_hndl, ACSD_DEFAULT_CLI_PORT,
		WBD_LOOPBACK_IP, tmpbuf, NULL);
	WBD_CHECK_MSG("Band[%d] interface["MACF"] Failed to set Fixed Chanspec[0x%x] Mode to ACSD. "
		"Error : %s\n", pdmif->band, ETHERP_TO_MACF(&pdmif->InterfaceId),
		pdmif->chanspec, wbderrorstr(ret));

end:
	if (com_hndl) {
		wbd_com_deinit(com_hndl);
	}

	WBD_EXIT();
	return ret;
}

/* Set chanspec through ACSD deamon running on ACCESS POINT */
/* TODO : check for the new chanspec logic */
int
wbd_slave_set_chanspec_through_acsd(wbd_slave_item_t *slave_item, chanspec_t chspec, int opt,
	i5_dm_interface_type *interface)
{
	int ret = WBDE_OK;
	char *nvval = NULL;
	char tmpbuf[WBD_MAX_BUF_512];
	wbd_com_handle *com_hndl = NULL;
	int sock_options = 0x0000;
	wbd_chan_change_reason_t reason, arg;
	WBD_ENTER();

	/* Fetch nvram param */
	nvval = blanket_nvram_prefix_safe_get(slave_item->wbd_ifr.primary_prefix, "ifname");

	arg = (wbd_chan_change_reason_t)opt;
	/* default option */
	reason = WBD_REASON_CSA;

	if (arg == WBD_REASON_DFS_AP_MOVE_START) {
		/* set dfs_event flag ignore_dfs_ap_move start, so that wbd_proces_event routine
		 * can ignore these events as it is generated by wbd itself.
		 */
		setbit(slave_item->dfs_event, IGNORE_DFS_AP_MOVE_START);
		reason = WL_CHAN_REASON_DFS_AP_MOVE_START;

	} else if (arg == WBD_REASON_DFS_AP_MOVE_RADAR_FOUND) {
		/* set dfs_event flag ignore_dfs_ap_move stop, so that wbd_proces_event routine
		 * can ignore these events as it is generated by wbd itself.
		 */
		setbit(slave_item->dfs_event, IGNORE_DFS_AP_MOVE_ABORTED);
		reason = WL_CHAN_REASON_DFS_AP_MOVE_RADAR_FOUND;

	} else if (arg == WBD_REASON_DFS_AP_MOVE_ABORTED) {
		/* set dfs_event flag ignore_dfs_ap_move aborted, so that wbd_proces_event routine
		 * can ignore these events as it is generated by wbd itself.
		 */
		setbit(slave_item->dfs_event, IGNORE_DFS_AP_MOVE_ABORTED);
		reason = WL_CHAN_REASON_DFS_AP_MOVE_ABORTED;

	} else if (arg == WBD_REASON_DFS_AP_MOVE_STUNT) {
		/* set dfs_event flag ignore_dfs_ap_move stunt, so that wbd_proces_event routine
		 * can ignore these events as it is generated by wbd itself.
		 */
		setbit(slave_item->dfs_event, IGNORE_DFS_AP_MOVE_STUNT);
		reason = WL_CHAN_REASON_DFS_AP_MOVE_STUNT;

	} else if (arg == WBD_REASON_CSA_TO_DFS_CHAN_FOR_CAC_ONLY) {
		reason = WL_CHAN_REASON_CSA_TO_DFS_CHAN_FOR_CAC_ONLY;
	} else if (arg == WBD_REASON_CSA) {
		setbit(slave_item->dfs_event, IGNORE_CSA);
	} else {
		/* For all other arg, WBD_REASON_CSA is default option to
		 * set the channel via acsd
		 */
	}

	snprintf(tmpbuf, sizeof(tmpbuf), "set&ifname=%s&param=chanspec&value=%d&option=%d",
		nvval, chspec, (int)reason);

	sock_options =	WBD_COM_FLAG_CLIENT | WBD_COM_FLAG_BLOCKING_SOCK
			| WBD_COM_FLAG_NO_RESPONSE;

	com_hndl = wbd_com_init(slave_item->parent->parent->hdl,
		INVALID_SOCKET, sock_options, NULL, NULL, slave_item->parent->parent);

	if (!com_hndl) {
		WBD_ERROR("Band[%d] Slave["MACF"] Failed to initialize the communication module "
			"for changing chanspec[0x%x] through ACSD\n", slave_item->band,
			ETHER_TO_MACF(slave_item->wbd_ifr.mac), chspec);
		ret = WBDE_COM_ERROR;
		goto end;
	}

	/* Send the command to ACSD */
	ret = wbd_com_connect_and_send_cmd(com_hndl, ACSD_DEFAULT_CLI_PORT,
		WBD_LOOPBACK_IP, tmpbuf, NULL);
	WBD_CHECK_MSG("Band[%d] Slave["MACF"] Failed to send change Chanspec[0x%x] to ACSD. "
		"Error : %s\n", slave_item->band, ETHER_TO_MACF(slave_item->wbd_ifr.mac),
		chspec, wbderrorstr(ret));
	if (ret == WBDE_OK) {
		wbd_slave_wait_set_chanspec(slave_item, interface, chspec);
	}

end:
	if (com_hndl) {
		wbd_com_deinit(com_hndl);
	}

	WBD_EXIT();
	return ret;
}

#ifdef WLHOSTFBT
/* Get FBT_CONFIG_REQ Data from 1905 Device */
static int
wbd_slave_get_fbt_config_req_data(wbd_cmd_fbt_config_t *fbt_config_req)
{
	int ret = WBDE_OK;
	struct ether_addr bssid, br_mac;
	wbd_fbt_bss_entry_t new_fbt_bss;
	unsigned char al_mac[MAC_ADDR_LEN], map = 0;
	char wbd_ifnames[NVRAM_MAX_VALUE_LEN] = {0};
	char var_intf[IFNAMSIZ] = {0}, ifname[IFNAMSIZ] = {0}, *next_intf;
	WBD_ENTER();

	/* Initialize fbt_config_req's Total item count in caller  */
	WBD_INFO("Get FBT Config Request Data\n");

	/* Get DeviceID AL MAC Address */
	memcpy(al_mac, ieee1905_get_al_mac(), sizeof(al_mac));

	/* Get bridge address */
	wbd_get_bridge_mac(&br_mac);

	WBDSTRNCPY(wbd_ifnames, blanket_nvram_safe_get(WBD_NVRAM_IFNAMES),
		sizeof(wbd_ifnames));

	/* Traverse wbd_ifnames for each ifname */
	foreach(var_intf, wbd_ifnames, next_intf) {

		char prefix[IFNAMSIZ];

		/* Copy interface name temporarily */
		WBDSTRNCPY(ifname, var_intf, sizeof(ifname));

		/* Get prefix of the interface from Driver */
		blanket_get_interface_prefix(ifname, prefix, sizeof(prefix));

		/* Get the BSSID/MAC of the BSS */
		wbd_ether_atoe(blanket_nvram_prefix_safe_get(prefix, NVRAM_HWADDR), &bssid);

		/* Get IEEE1905_MAP_FLAG_XXX flag value for this BSS */
		map = wbd_get_map_flags(prefix);

		/* Loop for below activity, only for Fronthaul BSS */
		if (!I5_IS_BSS_FRONTHAUL(map)) {
			continue;
		}

		/* If FBT not possible on this BSS, go for next */
		if (!wbd_is_fbt_possible(prefix)) {
			continue;
		}

		WBD_INFO("Adding FBT Data to FBT_CONFIG_REQ : BSS[%s]\n", ifname);

		/* Fill FBT Data for this BSS & Save FBT info of this BSS */
		memset(&new_fbt_bss, 0, sizeof(new_fbt_bss));
		memset(&(new_fbt_bss.fbt_info), 0, sizeof(new_fbt_bss.fbt_info));

		/* Fill Blanket ID, AL_MAC for this BSS */
		new_fbt_bss.blanket_id = I5_IS_BSS_GUEST(map) ?
			WBD_BKT_ID_BR1 : WBD_BKT_ID_BR0;
		memcpy(new_fbt_bss.al_mac, al_mac, sizeof(new_fbt_bss.al_mac));

		/* Get/Generate R0KH_ID for this BSS */
		wbd_get_r0khid(prefix, new_fbt_bss.r0kh_id, sizeof(new_fbt_bss.r0kh_id), 1);
		new_fbt_bss.len_r0kh_id = strlen(new_fbt_bss.r0kh_id);

		/* Get/Generate R0KH_KEY for this BSS */
		wbd_get_r0khkey(prefix, new_fbt_bss.r0kh_key, sizeof(new_fbt_bss.r0kh_key), 1);
		new_fbt_bss.len_r0kh_key = strlen(new_fbt_bss.r0kh_key);

		/* Prepare a new FBT BSS item for Slave's FBT Request List */
		memcpy(new_fbt_bss.bssid, (unsigned char*)&bssid,
			sizeof(new_fbt_bss.bssid));
		memcpy(new_fbt_bss.bss_br_mac, (unsigned char*)&br_mac,
			sizeof(new_fbt_bss.bss_br_mac));

		WBD_INFO("Adding new entry to FBT_CONFIG_REQ : "
			"Blanket[BR%d] Device["MACDBG"] "
			"BRDG["MACDBG"] BSS["MACDBG"] "
			"R0KH_ID[%s] LEN_R0KH_ID[%d] "
			"R0KH_Key[%s] LEN_R0KH_Key[%d]\n",
			new_fbt_bss.blanket_id,
			MAC2STRDBG(new_fbt_bss.al_mac),
			MAC2STRDBG(new_fbt_bss.bss_br_mac),
			MAC2STRDBG(new_fbt_bss.bssid),
			new_fbt_bss.r0kh_id, new_fbt_bss.len_r0kh_id,
			new_fbt_bss.r0kh_key, new_fbt_bss.len_r0kh_key);

		/* Add a FBT BSS item in a Slave's FBT Request List, Total item count++ */
		wbd_add_item_in_fbt_cmdlist(&new_fbt_bss, fbt_config_req, NULL);
	}

	WBD_EXIT();
	return ret;
}

/* Send 1905 Vendor Specific FBT_CONFIG_REQ command, from Agent to Controller */
int
wbd_slave_send_fbt_config_request_cmd()
{
	int ret = WBDE_OK;
	ieee1905_vendor_data vndr_msg_data;
	wbd_cmd_fbt_config_t *fbt_config_req; /* FBT Config Request Data */
	i5_dm_device_type *controller_device = NULL;
	wbd_info_t *info = wbd_get_ginfo();
	WBD_ENTER();

	memset(&vndr_msg_data, 0, sizeof(vndr_msg_data));

	/* Remove all Previous FBT Information data items on this Device */
	wbd_ds_glist_cleanup(&(info->wbd_fbt_config.entry_list));

	/* Initialize FBT Config Request Data, Utilize/Update Slave's FBT Config Object */
	fbt_config_req = &(info->wbd_fbt_config);
	memset(fbt_config_req, 0, sizeof(*fbt_config_req));
	wbd_ds_glist_init(&(fbt_config_req->entry_list));

	/* Check if FBT is possible on this 1905 Device or not */
	ret = wbd_ds_is_fbt_possible_on_agent();
	WBD_ASSERT_MSG("Device["MACDBG"]: %s\n",
		MAC2STRDBG(ieee1905_get_al_mac()), wbderrorstr(ret));

	/* Allocate Dynamic mem for Vendor data from App */
	vndr_msg_data.vendorSpec_msg =
		(unsigned char *)wbd_malloc(IEEE1905_MAX_VNDR_DATA_BUF, &ret);
	WBD_ASSERT();

	WBD_SAFE_GET_I5_CTLR_DEVICE(controller_device, &ret);

	/* Fill Destination AL_MAC in Vendor data */
	memcpy(vndr_msg_data.neighbor_al_mac,
		controller_device->DeviceId, IEEE1905_MAC_ADDR_LEN);

	/* Get FBT Config Request Data from 1905 Device */
	wbd_slave_get_fbt_config_req_data(fbt_config_req);

	/* Encode Vendor Specific TLV for Message : FBT_CONFIG_REQ to send */
	wbd_tlv_encode_fbt_config_request((void *)fbt_config_req,
		vndr_msg_data.vendorSpec_msg, &vndr_msg_data.vendorSpec_len);

	WBD_INFO("Send FBT_CONFIG_REQ from Device["MACDBG"] to Device["MACDBG"]\n",
		MAC2STRDBG(ieee1905_get_al_mac()), MAC2STRDBG(controller_device->DeviceId));

	/* Send Vendor Specific Message : FBT_CONFIG_REQ */
	ieee1905_send_vendor_specific_msg(&vndr_msg_data);

	WBD_CHECK_ERR_MSG(WBDE_DS_1905_ERR, "Failed to send "
		"FBT_CONFIG_REQ from Device["MACDBG"], Error : %s\n",
		MAC2STRDBG(ieee1905_get_al_mac()), wbderrorstr(ret));

	/* FBT_CONFIG_REQ is sent to the controller. So, update the flag accordingly */
	wbd_get_ginfo()->wbd_slave->flags |= WBD_BKT_SLV_FLAGS_FBT_REQ_SENT;

end:
	if (vndr_msg_data.vendorSpec_msg) {
		free(vndr_msg_data.vendorSpec_msg);
	}
	WBD_EXIT();
	return ret;
}

/* Unset all the NVRAMs corresponding to a particular ap name */
static void
wbd_slave_unset_peer_fbt_nvrams(char *prefix, char *ap_name)
{
	char data[WBD_MAX_BUF_256] = {0};

	/* [1] Unset NVRAM R0KH_ID for this BSS */
	blanket_nvram_prefix_unset(prefix, strcat_r(ap_name, NVRAM_FBT_R0KH_ID, data));

	/* [2] Unset NVRAM R0KH_ID_LEN for this BSS */
	blanket_nvram_prefix_unset(prefix, strcat_r(ap_name, NVRAM_FBT_R0KH_ID_LEN, data));

	/* [3] Unset NVRAM R0KH_KEY for this BSS */
	blanket_nvram_prefix_unset(prefix, strcat_r(ap_name, NVRAM_FBT_R0KH_KEY, data));

	/* [4] Unset NVRAM R1KH_ID for this BSS */
	blanket_nvram_prefix_unset(prefix, strcat_r(ap_name, NVRAM_FBT_R1KH_ID, data));

	/* [5] Unset NVRAM R1KH_KEY for this BSS */
	blanket_nvram_prefix_unset(prefix, strcat_r(ap_name, NVRAM_FBT_R1KH_KEY, data));

	/* [6] Unset NVRAM ADDR for this BSS */
	blanket_nvram_prefix_unset(prefix, strcat_r(ap_name, NVRAM_FBT_ADDR, data));

	/* [7] Unset NVRAM BR_ADDR for this BSS */
	blanket_nvram_prefix_unset(prefix, strcat_r(ap_name, NVRAM_FBT_BR_ADDR, data));
}

/* Remove all the non active peer FBT NVRAMs. Compare old fbt_aps and new fbt_aps and remove all
 * the NVRAMs which are not there in the new list
 */
static uint32
wbd_slave_remove_nonactive_peer_fbt_nvrams(char *prefix, char *old_fbt_aps, char *new_fbt_aps)
{
	uint32 rc_flags = 0, max_len;
	char *next;
	char ap_name[WBD_MAX_BUF_256] = {0}, ap_name_base[WBD_MAX_BUF_256] = {0};

	/* Get the max length among old and new fbt_aps list */
	max_len = strlen(old_fbt_aps);
	if (max_len < strlen(new_fbt_aps)) {
		max_len = strlen(new_fbt_aps);
	}

	/* If both are equal, no need to traverse the list */
	if (memcmp(old_fbt_aps, new_fbt_aps, max_len) == 0) {
		goto end;
	}

	/* Traverse through the old fbt_aps list and remove if it is not in the new fbt_aps list */
	foreach(ap_name_base, old_fbt_aps, next) {
		if (find_in_list(new_fbt_aps, ap_name_base)) {
			continue;
		}

		memset(ap_name, 0, sizeof(ap_name));
		snprintf(ap_name, sizeof(ap_name), "%s_", ap_name_base);
		wbd_slave_unset_peer_fbt_nvrams(prefix, ap_name);
		rc_flags = 1;
	}

end:
	return rc_flags;
}

/* Create the NVRAMs required for FBT for Neighbor APs */
uint32
wbd_slave_create_peer_fbt_nvrams(char* ifname, wbd_fbt_bss_entry_t *fbt_bss_info,
	wbd_cmd_fbt_config_t *fbt_config_resp, int *error)
{
	int ret = WBDE_OK;
	char prefix[IFNAMSIZ] = {0};
	dll_t *fbt_item_p = NULL;
	wbd_fbt_bss_entry_t *fbt_data = NULL;
	char new_value[WBD_MAX_BUF_256] = {0};
	char ap_name[WBD_MAX_BUF_256] = {0}, ap_name_base[WBD_MAX_BUF_256] = {0};
	char data[WBD_MAX_BUF_256] = {0};
	uint32 rc_flags = 0;
	char wlx_fbt_aps[NVRAM_MAX_VALUE_LEN] = {0}, old_wlx_fbt_aps[NVRAM_MAX_VALUE_LEN] = {0};
	WBD_ENTER();

	memset(data, 0, sizeof(data));

	/* Validate arg */
	WBD_ASSERT_ARG(ifname, WBDE_INV_ARG);
	WBD_ASSERT_ARG(fbt_bss_info, WBDE_INV_ARG);
	WBD_ASSERT_ARG(fbt_config_resp, WBDE_INV_ARG);

	/* Get prefix of the interface from Driver */
	blanket_get_interface_prefix(ifname, prefix, sizeof(prefix));

	/* Fetch NVRAM FBT_APS for this BSS */
	WBDSTRNCPY(old_wlx_fbt_aps, blanket_nvram_prefix_safe_get(prefix, NVRAM_FBT_APS),
		sizeof(old_wlx_fbt_aps));
	memset(wlx_fbt_aps, 0, sizeof(wlx_fbt_aps));

	/* Travese FBT Config Response List items */
	foreach_glist_item(fbt_item_p, fbt_config_resp->entry_list) {

		fbt_data = (wbd_fbt_bss_entry_t *)fbt_item_p;

		/* Loop for below activity, only for BSSID other than Self BSS's BSSID */
		if (memcmp(fbt_bss_info->bssid, fbt_data->bssid, MAC_ADDR_LEN) == 0) {
			continue;
		}

		/* AP_NAME_BASE = Prefix + R0KH_ID for this ITER_AP, and Generate AP_NAME */
		snprintf(ap_name_base, sizeof(ap_name_base), "%s%s", prefix, fbt_data->r0kh_id);
		memset(ap_name, 0, sizeof(ap_name));
		snprintf(ap_name, sizeof(ap_name), "%s_", ap_name_base);

		/* Loop for below activity, only if new AP_NAME_BASE is not in FBT_APS NVRAM */
		if (find_in_list(wlx_fbt_aps, ap_name_base)) {
			continue;
		}

		/* [1] Save NVRAM R0KH_ID for this BSS */
		rc_flags |= blanket_nvram_prefix_match_set(NULL,
			strcat_r(ap_name, NVRAM_FBT_R0KH_ID, data),
			fbt_data->r0kh_id, FALSE);

		/* [2] Save NVRAM R0KH_ID_LEN for this BSS */
		memset(new_value, 0, sizeof(new_value));
		snprintf(new_value, sizeof(new_value), "%d", fbt_data->len_r0kh_id);
		rc_flags |= blanket_nvram_prefix_match_set(NULL,
			strcat_r(ap_name, NVRAM_FBT_R0KH_ID_LEN, data),
			new_value, FALSE);

		/* [3] Save NVRAM R0KH_KEY for this BSS */
		rc_flags |= blanket_nvram_prefix_match_set(NULL,
			strcat_r(ap_name, NVRAM_FBT_R0KH_KEY, data),
			fbt_data->r0kh_key, FALSE);

		/* [4] Save NVRAM R1KH_ID for this BSS */
		memset(new_value, 0, sizeof(new_value));
		rc_flags |= blanket_nvram_prefix_match_set(NULL,
			strcat_r(ap_name, NVRAM_FBT_R1KH_ID, data),
			wbd_ether_etoa(fbt_data->bssid, new_value), FALSE);

		/* [5] Save NVRAM R1KH_KEY for this BSS */
		rc_flags |= blanket_nvram_prefix_match_set(NULL,
			strcat_r(ap_name, NVRAM_FBT_R1KH_KEY, data),
			fbt_bss_info->r0kh_key, FALSE);

		/* [6] Save NVRAM ADDR for this BSS */
		memset(new_value, 0, sizeof(new_value));
		rc_flags |= blanket_nvram_prefix_match_set(NULL,
			strcat_r(ap_name, NVRAM_FBT_ADDR, data),
			wbd_ether_etoa(fbt_data->bssid, new_value), FALSE);

		/* [7] Save NVRAM BR_ADDR for this BSS */
		memset(new_value, 0, sizeof(new_value));
		rc_flags |= blanket_nvram_prefix_match_set(NULL,
			strcat_r(ap_name, NVRAM_FBT_BR_ADDR, data),
			wbd_ether_etoa(fbt_data->bss_br_mac, new_value), FALSE);

		/* Add this ITER_AP's AP_NAME_BASE to FBT_APS NVRAM value */
		add_to_list(ap_name_base, wlx_fbt_aps, sizeof(wlx_fbt_aps));
		WBD_INFO("Adding %s in %sfbt_aps[%s]\n", ap_name_base, prefix, wlx_fbt_aps);
	}

	/* [8] Save NVRAM FBT_APS for this BSS */
	rc_flags |= blanket_nvram_prefix_match_set(prefix, NVRAM_FBT_APS, wlx_fbt_aps, FALSE);

	/* Remove all the non active FBT peer NVRAMs */
	rc_flags |= wbd_slave_remove_nonactive_peer_fbt_nvrams(prefix, old_wlx_fbt_aps,
		wlx_fbt_aps);

end:
	if (rc_flags) {
		rc_flags = WBD_FLG_NV_COMMIT;
		wbd_do_rc_restart_reboot(rc_flags);
	}

	WBD_INFO("%s in PEER_NVRAMs on getting new FBT_CONFIG_RESP "
		"data for BSS[%s]. Old[%s] New[%s]\n", (rc_flags) ? "Changes" : "NO Changes",
		ifname, old_wlx_fbt_aps, wlx_fbt_aps);

	if (error) {
		*error = ret;
	}
	WBD_EXIT();
	return rc_flags;
}

/* Create the NVRAMs required for FBT for Self AP */
uint32
wbd_slave_create_self_fbt_nvrams(char* ifname, wbd_fbt_bss_entry_t *fbt_bss_info, int *error)
{
	int ret = WBDE_OK;
	char prefix[IFNAMSIZ] = {0};
	char r1kh_id[WBD_FBT_R0KH_ID_LEN] = {0};
	char new_value[WBD_MAX_BUF_256] = {0};
	uint32 rc_flags = 0;
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(ifname, WBDE_INV_ARG);
	WBD_ASSERT_ARG(fbt_bss_info, WBDE_INV_ARG);

	/* Get prefix of the interface from Driver */
	blanket_get_interface_prefix(ifname, prefix, sizeof(prefix));

	/* [1] Save NVRAM WBD_FBT for this BSS. Add "psk2ft" in AKM */
	if (wbd_is_fbt_nvram_enabled(prefix) != WBD_FBT_DEF_FBT_ENABLED) {
		wbd_enable_fbt(prefix);
		rc_flags |= WBD_FLG_NV_COMMIT|WBD_FLG_RC_RESTART;
	}

	/* [2] Save NVRAM FBT_AP for this BSS */
	memset(new_value, 0, sizeof(new_value));
	snprintf(new_value, sizeof(new_value), "%d",
		WBD_FBT_DEF_AP);
	rc_flags |= blanket_nvram_prefix_match_set(prefix, NVRAM_FBT_AP,
		new_value, FALSE);

	/* [3] Save NVRAM R0KH_ID for this BSS */
	rc_flags |= blanket_nvram_prefix_match_set(prefix, NVRAM_FBT_R0KH_ID,
		fbt_bss_info->r0kh_id, FALSE);

	/* [4] Save NVRAM R0KH_KEY for this BSS */
	rc_flags |= blanket_nvram_prefix_match_set(prefix, NVRAM_FBT_R0KH_KEY,
		fbt_bss_info->r0kh_key, FALSE);

	/* [5] Get/Generate R1KH_ID for this BSS */
	wbd_get_r1khid(prefix, r1kh_id, sizeof(r1kh_id), 0);
	rc_flags |= blanket_nvram_prefix_match_set(prefix, NVRAM_FBT_R1KH_ID,
		r1kh_id, FALSE);

	/* [6] Save NVRAM FBT_MDID for this BSS */
	memset(new_value, 0, sizeof(new_value));
	snprintf(new_value, sizeof(new_value), "%d",
		fbt_bss_info->fbt_info.mdid);
	rc_flags |= blanket_nvram_prefix_match_set(prefix, NVRAM_FBT_MDID,
		new_value, FALSE);

	/* [7] Save NVRAM FBTOVERDS for this BSS */
	memset(new_value, 0, sizeof(new_value));
	snprintf(new_value, sizeof(new_value), "%d",
		(fbt_bss_info->fbt_info.ft_cap_policy & FBT_MDID_CAP_OVERDS));
	rc_flags |= blanket_nvram_prefix_match_set(prefix, NVRAM_FBT_OVERDS,
		new_value, FALSE);

	/* [8] Save NVRAM FBT_REASSOC_TIME for this BSS */
	memset(new_value, 0, sizeof(new_value));
	snprintf(new_value, sizeof(new_value), "%d",
		fbt_bss_info->fbt_info.tie_reassoc_interval);
	rc_flags |= blanket_nvram_prefix_match_set(prefix, NVRAM_FBT_REASSOC_TIME,
		new_value, FALSE);

end:
	if (rc_flags) {
		rc_flags = WBD_FLG_NV_COMMIT|WBD_FLG_RC_RESTART;
	}

	WBD_INFO("%s in SELF_NVRAMs on getting new FBT_CONFIG_RESP "
	"data for BSS[%s]\n", (rc_flags) ? "Changes" : "NO Changes", ifname);

	if (error) {
		*error = ret;
	}
	WBD_EXIT();
	return rc_flags;
}

/* Process 1905 Vendor Specific FBT_CONFIG_RESP Message */
static int
wbd_slave_process_fbt_config_resp_cmd(unsigned char *neighbor_al_mac,
	unsigned char *tlv_data, unsigned short tlv_data_len)
{
	int ret = WBDE_OK;
	wbd_cmd_fbt_config_t fbt_config_resp; /* FBT Config Response Data */
	dll_t *fbt_item_p = NULL;
	wbd_fbt_bss_entry_t *fbt_data = NULL;
	uint32 rc_flags = 0;
	struct ether_addr bssid;
	unsigned char map = 0;
	char wbd_ifnames[NVRAM_MAX_VALUE_LEN] = {0};
	char var_intf[IFNAMSIZ] = {0}, ifname[IFNAMSIZ] = {0}, *next_intf;
	wbd_info_t *info = wbd_get_ginfo();
	wbd_cmd_fbt_config_t *agnt_fbt_config = &(info->wbd_fbt_config);
	WBD_ENTER();

	/* If FBT_CONFIG_REQ is not sent to the controller, do not process the response */
	if (!(info->wbd_slave->flags & WBD_BKT_SLV_FLAGS_FBT_REQ_SENT)) {
		WBD_WARNING("Not Processing FBT config response as a request was never sent\n");
		WBD_EXIT();
		return ret;
	}

	/* Initialize FBT Config Response Data */
	memset(&fbt_config_resp, 0, sizeof(fbt_config_resp));
	wbd_ds_glist_init(&(fbt_config_resp.entry_list));

	/* Decode Vendor Specific TLV for Message : FBT_CONFIG_RESP on receive */
	ret = wbd_tlv_decode_fbt_config_response((void *)&fbt_config_resp, tlv_data, tlv_data_len);
	WBD_ASSERT_MSG("Failed to decode FBT Config Response From DEVICE["MACDBG"]\n",
		MAC2STRDBG(neighbor_al_mac));

	/* Travese FBT Config Response List items */
	foreach_glist_item(fbt_item_p, fbt_config_resp.entry_list) {

		fbt_data = (wbd_fbt_bss_entry_t *)fbt_item_p;

		WBD_INFO("From DEVICE["MACDBG"] BRDG["MACDBG"] BSS["MACDBG"] "
			"Received FBT_CONFIG_RESP : "
			"R0KH_ID[%s] LEN_R0KH_ID[%d] "
			"R0KH_Key[%s] LEN_R0KH_Key[%d] "
			"MDID[%d] FT_CAP[%d] FT_REASSOC[%d]\n", MAC2STRDBG(neighbor_al_mac),
			MAC2STRDBG(fbt_data->bss_br_mac), MAC2STRDBG(fbt_data->bssid),
			fbt_data->r0kh_id, fbt_data->len_r0kh_id,
			fbt_data->r0kh_key, fbt_data->len_r0kh_key,
			fbt_data->fbt_info.mdid,
			fbt_data->fbt_info.ft_cap_policy,
			fbt_data->fbt_info.tie_reassoc_interval);
	}

	WBDSTRNCPY(wbd_ifnames, blanket_nvram_safe_get(WBD_NVRAM_IFNAMES),
		sizeof(wbd_ifnames));

	/* Traverse wbd_ifnames for each ifname */
	foreach(var_intf, wbd_ifnames, next_intf) {

		char prefix[IFNAMSIZ];
		wbd_fbt_bss_entry_t *matching_fbt_resp_info = NULL, *matching_fbt_bss_info = NULL;

		/* Copy interface name temporarily */
		WBDSTRNCPY(ifname, var_intf, sizeof(ifname));

		/* Get prefix of the interface from Driver */
		blanket_get_interface_prefix(ifname, prefix, sizeof(prefix));

		/* Get the BSSID/MAC of the BSS */
		wbd_ether_atoe(blanket_nvram_prefix_safe_get(prefix, NVRAM_HWADDR), &bssid);

		/* Get IEEE1905_MAP_FLAG_XXX flag value for this BSS */
		map = wbd_get_map_flags(prefix);

		/* Loop for below activity, only for Fronthaul BSS */
		if (!I5_IS_BSS_FRONTHAUL(map)) {
			continue;
		}

		/* If FBT not possible on this BSS, go for next */
		if (!wbd_is_fbt_possible(prefix)) {
			continue;
		}

		/* Find matching fbt_info for this bssid, In FBT_CONFIG_RESP */
		matching_fbt_resp_info = wbd_find_fbt_bss_item_for_bssid((unsigned char*)&bssid,
			&fbt_config_resp);
		if (!matching_fbt_resp_info) {
			/* Disable FBT, if it is not already disabled */
			int fbt = blanket_get_config_val_int(prefix, WBD_NVRAM_FBT, WBD_FBT_NOT_DEFINED);
			if (fbt != WBD_FBT_DEF_FBT_DISABLED) {
				WBD_INFO("No FBT data for BSS["MACDBG"] prefix[%s]. Disable FBT\n",
					MAC2STRDBG((unsigned char*)&bssid), prefix);
				wbd_disable_fbt(prefix);
				rc_flags |= WBD_FLG_NV_COMMIT|WBD_FLG_RC_RESTART;
			}
			continue;
		}

		/* Find matching fbt_info for this bssid, In Slave's FBT Data */
		matching_fbt_bss_info = wbd_find_fbt_bss_item_for_bssid((unsigned char*)&bssid,
			agnt_fbt_config);
		if (!matching_fbt_bss_info) {
			continue;
		}

		WBD_INFO("Updating FBT Data from FBT_CONFIG_RESP : BSS[%s]\n", ifname);

		/* Store matching fbt_info details to Vendor Specific Data */
		matching_fbt_bss_info->fbt_info.mdid =
			matching_fbt_resp_info->fbt_info.mdid;
		matching_fbt_bss_info->fbt_info.ft_cap_policy =
			matching_fbt_resp_info->fbt_info.ft_cap_policy;
		matching_fbt_bss_info->fbt_info.tie_reassoc_interval =
			matching_fbt_resp_info->fbt_info.tie_reassoc_interval;

		/* Create the NVRAMs required for FBT for Self AP */
		rc_flags |= wbd_slave_create_self_fbt_nvrams(ifname, matching_fbt_bss_info, &ret);

		/* Create the NVRAMs required for FBT for Neighbor APs */
		rc_flags |= wbd_slave_create_peer_fbt_nvrams(ifname, matching_fbt_bss_info,
			&fbt_config_resp, &ret);
	}

end:
	/* Remove all FBT Config Response data items */
	wbd_ds_glist_cleanup(&(fbt_config_resp.entry_list));

	/* If required, Execute nvram commit/rc restart/reboot commands */
	if (rc_flags & WBD_FLG_RC_RESTART) {
		WBD_INFO("Creating rc restart timer\n");
		wbd_slave_create_rc_restart_timer(info);
		info->flags |= WBD_INFO_FLAGS_RC_RESTART;
	}

	/* Check if Restart is done if no more restart required.
	 * Whenever new device is connected with controller
	 * this is raise the event and set the NVRAM.
	 * So to avoid setting each time, check and if NVRAM is not set
	 * then only set the NVRAM and rasie the event.
	 */
	if (!(info->flags & WBD_INFO_FLAGS_RC_RESTART) &&
		(atoi(blanket_nvram_safe_get(NVRAM_MAP_AGENT_CONFIGURED)) == 0)) {
#ifdef BCM_APPEVENTD
		/* Raise and send MAP init end event to appeventd. */
		wbd_appeventd_map_init(APP_E_WBD_SLAVE_MAP_INIT_END,
			(struct ether_addr*)ieee1905_get_al_mac(), MAP_INIT_END,
			MAP_APPTYPE_SLAVE);
#endif /* BCM_APPEVENTD */
		blanket_nvram_prefix_set(NULL, NVRAM_MAP_AGENT_CONFIGURED, "1");
	}

	WBD_EXIT();
	return ret;
}
#endif /* WLHOSTFBT */

#if defined(MULTIAPR2)
/* Send Requested { STORED } Channel Scan Report to Controller */
static int
wbd_slave_process_requested_stored_channel_scan(unsigned char *src_al_mac,
	ieee1905_chscan_req_msg *chscan_req)
{
	WBD_ENTER();

	/* Send Channel Scan Report Message to a Multi AP Device */
	if (chscan_req) {
		ieee1905_send_requested_stored_channel_scan(chscan_req);
	}

	WBD_EXIT();
	return WBDE_OK;
}

/* Run the escan for radio */
static int
wbd_slave_escan_run_per_radio(i5_dm_interface_type *self_ifr,
	ieee1905_per_radio_opclass_list *ifr, t_I5_CHSCAN_TYPE scan_type)
{
	int ret = 0;
	i5_dm_device_type *selfdevice = i5DmGetSelfDevice();
	int params_size = (WL_SCAN_PARAMS_FIXED_SIZE +
		(uint)(uintptr)&((wl_escan_params_t *)0)->params) +
		(WL_NUMCHANNELS * sizeof(uint16));
	wl_escan_params_t *params;
	WBD_ENTER();

	/* If channel scan is running, Abort the scan */
	if (I5_IS_CHSCAN_RUNNING(self_ifr->flags)) {

		if (blanket_escan_abort(self_ifr->ifname) != 0) {
			WBD_ERROR("ifname %s Failed to Abort Scan\n", self_ifr->ifname);
		}

		/* Update Status Code = ABORTED, of relavant Results in SelfDevice for a Radio */
		i5DmUpdateChannelScanStatusPerRadio(selfdevice, ifr,
			-1, MAP_CHSCAN_STATUS_ABORTED);

		WBD_ERROR("ifname %s Aborted the Channel Scan\n", self_ifr->ifname);
	}

	params_size += WL_SCAN_PARAMS_SSID_MAX * sizeof(wlc_ssid_t);

	params = (wl_escan_params_t*)wbd_malloc(params_size, &ret);
	WBD_ASSERT();

	/* Prepare scan params from Cchannel Scan Request */
	blanket_escan_prep_cs(ifr, &params->params, &params_size);

	/* (Re)Initialize relavant Channel Scan Result objects in Stored Results */
	i5DmChannelScanResultItemInitPerRadio(ifr, &selfdevice->stored_chscan_results);

	/* Set Flag indicating Running a Requested Channel Scan on this radio */
	I5DmSetChScanRunning(self_ifr, scan_type, I5_CHSCAN_RUNNING);

	params->version = htod32(ESCAN_REQ_VERSION);
	params->action = htod16(WL_SCAN_ACTION_START);

#ifdef __linux__
	srand((unsigned)time(NULL));
	params->sync_id = rand() & 0xffff;
#else
	params->sync_id = 4321;
#endif /* __linux__ */
	params->sync_id = htod16(params->sync_id);

	params_size += (uint)(uintptr)&((wl_escan_params_t *)0)->params;

	/* Start the escan now for this radio */
	if (blanket_escan_start(self_ifr->ifname, params, params_size) != 0) {

		WBD_ERROR("ifname %s Failed to Start Scan\n", self_ifr->ifname);

		/* Set Flag indicating Running a Requested Channel Scan on this radio */
		I5DmSetChScanRunning(self_ifr, scan_type, I5_CHSCAN_STOP);
	}

end:
	if (params) {
		free(params);
	}
	WBD_EXIT();
	return ret;
}

/* Send Requested { FRESH } Channel Scan Report to Controller */
static int
wbd_slave_process_requested_fresh_channel_scan(ieee1905_chscan_req_msg *chscan_req,
	t_I5_CHSCAN_TYPE scan_type)
{
	int ret = WBDE_OK, radio_found = 0;
	i5_dm_device_type *self_device = i5DmGetSelfDevice();
	i5_dm_interface_type *self_ifr = NULL;
	wbd_ifr_item_t *ifr_vndr_data = NULL;
	ieee1905_per_radio_opclass_list *dst_ifr = NULL, *ifr = NULL, *caps_ifr = NULL;
	ieee1905_chscan_req_msg caps_chscan_req;
	ieee1905_per_opclass_chan_list *opcls = NULL;
	i5_dm_rc_chan_map_type *rc_chan_map = NULL;

	WBD_ENTER();

	/* Validate arg */
	if (!chscan_req) {
		chscan_req = &caps_chscan_req;
	}

	/* Create Channel Scan Request Object in Self Device for OnBoot Scan */
	i5DmCreateChScanRequestFmCapabilities(&caps_chscan_req);

	/* Extract details for each radio */
	foreach_iglist_item(ifr, ieee1905_per_radio_opclass_list, chscan_req->radio_list) {

		radio_found = 0;
		rc_chan_map = NULL;

		/* Validate radio in Self Device */
		self_ifr = i5DmInterfaceFind(self_device, ifr->radio_mac);
		if (!self_ifr || !self_ifr->vndr_data ||
			!i5DmIsInterfaceWireless(self_ifr->MediaType)) {
			WBD_ERROR("IFR["MACF"] Not Found or Not Valid Wireless Interface.\n",
				ETHERP_TO_MACF(ifr->radio_mac));
			continue;
		}
		ifr_vndr_data = (wbd_ifr_item_t*)self_ifr->vndr_data;

		/* Validate details for each Radio : Number of Operating Classes */
		dst_ifr = ifr;
		if (ifr->num_of_opclass <= 0) {

			/* Extract details for each radio */
			foreach_iglist_item(caps_ifr, ieee1905_per_radio_opclass_list,
				caps_chscan_req.radio_list) {

				/* Skip the Results with Non-Matching Radios */
				if (memcmp(ifr->radio_mac, caps_ifr->radio_mac,
					sizeof(ifr->radio_mac)) == 0) {
					radio_found = 1;
					break;
				}
			}
			dst_ifr = (radio_found) ? caps_ifr : ifr;
		}

		WBD_INFO("IFR[%s] num_opclass[%d]\n", self_ifr->wlParentName, ifr->num_of_opclass);

		/* Validate details for each Operating Class : Number of Channels */
		foreach_iglist_item(opcls, ieee1905_per_opclass_chan_list, dst_ifr->opclass_list) {

			if (!opcls->supported) {
				continue;
			}

			/* Extract Num of Chan and Chan List from rc_map for cur Operating Class */
			if (opcls->num_of_channels <= 0) {

				rc_chan_map = i5DmGetChannelListFmRCMap(opcls->opclass_val);
				if (!rc_chan_map) {
					WBD_ERROR("Invalid regclass: %d\n", opcls->opclass_val);
					continue;
				}
				memcpy(opcls->chan_list, &(rc_chan_map->channel[0]),
					rc_chan_map->count);
				memset(opcls->supported_chan_list, 1, rc_chan_map->count);
				opcls->num_of_channels = rc_chan_map->count;
			}

			WBD_INFO("IFR[%s] opclass[%d] num_of_channels[%d] opclass_supported[%d]\n",
				self_ifr->wlParentName, opcls->opclass_val, opcls->num_of_channels,
				opcls->supported);

			/* Extract details of each Channel */
			int idx_chan = 0;
			for (idx_chan = 0; idx_chan < opcls->num_of_channels; idx_chan++) {

				WBD_INFO("IFR[%s] opclass[%d] num_of_channels[%d] "
					"supported_chans[%d] chan[%d]=[%d]\n",
					self_ifr->wlParentName, opcls->opclass_val,
					opcls->num_of_channels,
					opcls->supported_chan_list[idx_chan],
					idx_chan, opcls->chan_list[idx_chan]);
			}

		}

		/* If Channel Scan is not Currently running on this Radio */
		if (!I5_IS_CHSCAN_RUNNING(self_ifr->flags)) {

			/* Save Channel Scan Request data for this Radio */
			i5DmCopyChScanRequestPerRadio(
				&ifr_vndr_data->last_fresh_chscan_req, dst_ifr);
		}

		/* Run ESCAN for this radio to perform Requested {FRESH} Channel Scan */
		wbd_slave_escan_run_per_radio(self_ifr,
			&ifr_vndr_data->last_fresh_chscan_req, scan_type);
	}

	/* Free the memory allocated for Channel Scan Request Msg structure */
	i5DmChannelScanRequestInfoFree(&caps_chscan_req);

	WBD_EXIT();
	return ret;
}

/* Start Requested Channel Scan on Agent and Send Requested Channel Scan Report to Controller */
static int
wbd_slave_process_requested_channel_scan(unsigned char *src_al_mac,
	ieee1905_chscan_req_msg *chscan_req)
{
	int ret = WBDE_OK;
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(src_al_mac, WBDE_INV_ARG);
	WBD_ASSERT_ARG(chscan_req, WBDE_INV_ARG);

	/* For Requested Channel Scan - Stored */
	if (!(chscan_req->chscan_req_msg_flag & MAP_CHSCAN_REQ_FRESH_SCAN)) {

		/* Send Requested { STORED } Channel Scan Report to Controller */
		wbd_slave_process_requested_stored_channel_scan(src_al_mac, chscan_req);

	/* For Requested Channel Scan - Fresh */
	} else {

		/* Send Requested { FRESH } Channel Scan Report to Controller */
		wbd_slave_process_requested_fresh_channel_scan(chscan_req, I5_CHSCAN_REQUESTED);
	}
end:
	WBD_EXIT();
	return ret;
}

/* Start On Boot local Channel Scan on Agent for all Radios */
static void
wbd_slave_process_onboot_channel_scan(bcm_usched_handle *hdl, void *arg)
{
	WBD_ENTER();

	/* Send Requested { FRESH } Channel Scan Report to Controller */
	wbd_slave_process_requested_fresh_channel_scan(NULL, I5_CHSCAN_ONBOOT);

	WBD_EXIT();
	return;
}

/* Create OnBoot Channel Scan timer to Perform ONBOOT Channel Scan on this Agent  */
int
wbd_slave_create_onboot_channel_scan_timer(wbd_info_t *info)
{
	int ret = WBDE_OK;
	uint8 onboot_scan = 0;
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(info, WBDE_INV_ARG);

	/* Get NVRAM setting for Channel Scan Capabilities Flags : ONBOOT Scan / Requested Scan */
	onboot_scan = (uint8)blanket_get_config_val_int(NULL,
		NVRAM_MAP_CHSCAN_ONBOOT_SCAN, DEF_MAP_CHSCAN_ONBOOT_SCAN);

#ifdef MULTIAP_PLUGFEST
	/* Get NVRAM to check whether onboot scan should be forced or not */
	if (blanket_get_config_val_int(NULL, NVRAM_MAP_CHSCAN_FORCE_ONBOOT, 0) &&
		blanket_get_config_val_int(NULL, NVRAM_MAP_ONBOARDED, 0)) {
		WBD_INFO("Forced OnBoot Channel Scan is Enabled\n");
		onboot_scan = (uint8)1;
	}
#endif /* MULTIAP_PLUGFEST */

	if (!onboot_scan) {
		WBD_INFO("OnBoot Channel Scan Disabled\n");
		goto end;
	}

	WBD_INFO("Slave Creating timer to Perform OnBoot Channel Scan, timeout [%d] \n",
		info->max.tm_do_onboot_scan);

	/* Add the timer */
	ret = wbd_add_timers(info->hdl, info,
		WBD_SEC_MICROSEC((info->max.tm_do_onboot_scan)),
		wbd_slave_process_onboot_channel_scan, 0);

	/* If Add Timer not Succeeded */
	if (ret != WBDE_OK) {
		if (ret == WBDE_USCHED_TIMER_EXIST) {
			WBD_INFO("OnBoot Channel Scan Request timer already exist\n");
		} else {
			WBD_WARNING("Timeout[%d] Slave Failed to create OnBoot Channel Scan "
				"timer Error: %d\n", (info->max.tm_do_onboot_scan), ret);
		}
	}

end:
	WBD_EXIT();
	return ret;
}
#endif /* MULTIAPR2 */

/* Callback fn to process M2 is received for all Wireless Interfaces */
static void
wbd_slave_process_all_ap_configured(bcm_usched_handle *hdl, void *arg)
{
	wbd_info_t *info = (wbd_info_t *)arg;
	int ret = WBDE_OK;
	WBD_ENTER();

	WBD_ASSERT_ARG(info, WBDE_INV_ARG);

	/* Add static neighbor list entries from topology */
	wbd_slave_add_nbr_from_topology();

	WBD_DEBUG("send chan info to controller\n");
	/* send interface specific chan info to controller in vendor message */
	wbd_slave_send_chan_info();

	WBD_DEBUG("configure ACSD fixchanspec mode on agent \n");
	/* configure acsd to opearate in Fix chanspec mode on all agents,
	 * objective is to prevent acsd on repeaters not select any channel
	 * on it's own. In case MultiAP daemon wishes to change the channel
	 * on agent, it issues wbd_slave_set_chanspec_through_acsd on agent
	 */
	wbd_slave_set_acsd_mode_fix_chanspec();
	/* Reset all_ap_configured timer created flag */
	info->flags &= ~WBD_INFO_FLAGS_ALL_AP_CFGRED_TM;

end:
	WBD_EXIT();
}

/* Create timer for Callback fn to process M2 is received for all Wireless Interfaces */
int
wbd_slave_create_all_ap_configured_timer()
{
	int ret = WBDE_OK;
	WBD_ENTER();

	/* If all_ap_configured timer already created, don't create again */
	if (wbd_get_ginfo()->flags & WBD_INFO_FLAGS_ALL_AP_CFGRED_TM) {
		WBD_INFO("all_ap_configured timer already created\n");
		goto end;
	}

	/* Add the timer */
	ret = wbd_add_timers(wbd_get_ginfo()->hdl, wbd_get_ginfo(),
		WBD_SEC_MICROSEC(wbd_get_ginfo()->max.tm_process_ap_configure),
		wbd_slave_process_all_ap_configured, 0);

	/* If Add Timer Succeeded */
	if (ret == WBDE_OK) {
		/* Set all_ap_configured timer created flag, else error */
		wbd_get_ginfo()->flags |= WBD_INFO_FLAGS_ALL_AP_CFGRED_TM;
	} else {
		WBD_WARNING("Failed to create all_ap_configured timer for Interval[%d]\n",
			wbd_get_ginfo()->max.tm_process_ap_configure);
	}

end:
	WBD_EXIT();
	return ret;
}

/* Process 1905 Vendor Specific Messages at WBD Application Layer */
int
wbd_slave_process_vendor_specific_msg(wbd_info_t *info, ieee1905_vendor_data *msg_data)
{
	int ret = WBDE_OK;
	unsigned char *tlv_data = NULL;
	unsigned short pos, tlv_data_len, tlv_hdr_len;
	WBD_ENTER();

	/* Store TLV Hdr len */
	tlv_hdr_len = sizeof(i5_tlv_t);

	/* Initialize data pointers and counters */
	for (pos = 0, tlv_data_len = 0;

		/* Loop till we reach end of vendor data */
		(msg_data->vendorSpec_len - pos) > 0;

		/* Increament the pointer with current TLV Header + TLV data */
		pos += tlv_hdr_len + tlv_data_len) {

		/* For TLV, Initialize data pointers and counters */
		tlv_data = &msg_data->vendorSpec_msg[pos];

		/* Pointer is at the next TLV */
		i5_tlv_t *ptlv = (i5_tlv_t *)tlv_data;

		/* Get next TLV's data length (Hdr bytes skipping done in fn wbd_tlv_decode_xxx) */
		tlv_data_len = ntohs(ptlv->length);

		WBD_DEBUG("vendorSpec_len[%d] tlv_hdr_len[%d] tlv_data_len[%d] pos[%d]\n",
			msg_data->vendorSpec_len, tlv_hdr_len, tlv_data_len, pos);

		switch (ptlv->type) {

			case WBD_TLV_FBT_CONFIG_RESP_TYPE:
				WBD_INFO("Processing %s vendorSpec_len[%d]\n",
					wbd_tlv_get_type_str(WBD_TLV_FBT_CONFIG_RESP_TYPE),
					msg_data->vendorSpec_len);
#ifdef WLHOSTFBT
				/* Process 1905 Vendor Specific FBT_CONFIG_RESP Message */
				wbd_slave_process_fbt_config_resp_cmd(
					msg_data->neighbor_al_mac, tlv_data, tlv_data_len);
#endif /* WLHOSTFBT */
				break;

			case WBD_TLV_WEAK_CLIENT_RESP_TYPE:
				WBD_INFO("Processing %s vendorSpec_len[%d]\n",
					wbd_tlv_get_type_str(ptlv->type),
					msg_data->vendorSpec_len);
				/* Process 1905 Vendor Specific weak client response Message */
				wbd_slave_process_weak_client_cmd_resp(
					msg_data->neighbor_al_mac, tlv_data, tlv_data_len);
				break;

			case WBD_TLV_BSS_CAPABILITY_QUERY_TYPE:
				WBD_INFO("Processing %s vendorSpec_len[%d]\n",
					wbd_tlv_get_type_str(ptlv->type),
					msg_data->vendorSpec_len);
				/* Process 1905 Vendor Specific BSS Capability Query Message */
				wbd_slave_process_bss_capability_query_cmd(
					msg_data->neighbor_al_mac, tlv_data, tlv_data_len);
				break;

			case WBD_TLV_REMOVE_CLIENT_REQ_TYPE:
				WBD_INFO("Processing %s vendorSpec_len[%d]\n",
					wbd_tlv_get_type_str(WBD_TLV_REMOVE_CLIENT_REQ_TYPE),
					msg_data->vendorSpec_len);
				/* Process 1905 Vendor Specific Remove Client Request Vendor TLV */
				wbd_slave_process_remove_client_cmd(
					msg_data->neighbor_al_mac, tlv_data, tlv_data_len);
				break;

			case WBD_TLV_BSS_METRICS_QUERY_TYPE:
				WBD_INFO("Processing %s vendorSpec_len[%d]\n",
					wbd_tlv_get_type_str(ptlv->type),
					msg_data->vendorSpec_len);
				/* Process 1905 Vendor Specific BSS Metrics Query Message */
				wbd_slave_process_bss_metrics_query_cmd(
					msg_data->neighbor_al_mac, tlv_data, tlv_data_len);
				break;

			case WBD_TLV_ZWDFS_TYPE:
				WBD_INFO("Processing %s vendorSpec_len[%d]\n",
					wbd_tlv_get_type_str(ptlv->type),
					msg_data->vendorSpec_len);
				/* Process 1905 Vendor Specific ZWDFS messgae */
				wbd_slave_process_zwdfs_msg(msg_data->neighbor_al_mac, tlv_data,
					tlv_data_len);
				break;

			case WBD_TLV_BH_STA_METRIC_POLICY_TYPE:
				WBD_INFO("Processing %s vendorSpec_len[%d]\n",
					wbd_tlv_get_type_str(ptlv->type),
					msg_data->vendorSpec_len);
				/* Process Vendor Specific backhaul STA mertric policy Message */
				wbd_slave_process_backhaul_sta_metric_policy_cmd(info,
					msg_data->neighbor_al_mac, tlv_data, tlv_data_len);
				break;

			case WBD_TLV_NVRAM_SET_TYPE:
				WBD_INFO("Processing %s vendorSpec_len[%d]\n",
					wbd_tlv_get_type_str(ptlv->type),
					msg_data->vendorSpec_len);
				/* Process Vendor Specific NVRAM Set Message */
				wbd_slave_process_vndr_nvram_set_cmd(info,
					msg_data->neighbor_al_mac, tlv_data, tlv_data_len);
				break;
			case WBD_TLV_CHAN_SET_TYPE:
				WBD_INFO("Processing %s vendorSpec_len[%d]\n",
					wbd_tlv_get_type_str(ptlv->type),
					msg_data->vendorSpec_len);
				/* Process 1905 Vendor Specific set channel */
				wbd_slave_process_vndr_set_chan_cmd(msg_data->neighbor_al_mac,
					tlv_data, tlv_data_len);
				break;

			case WBD_TLV_DFS_CHAN_INFO_TYPE:
				WBD_INFO("Processing %s vendorSpec_len[%d]\n",
					wbd_tlv_get_type_str(ptlv->type),
					msg_data->vendorSpec_len);

				/* Process 1905 Vendor Specific chan info and chanspec list */
				wbd_slave_process_dfs_chan_info(msg_data->neighbor_al_mac,
					tlv_data, tlv_data_len);
				break;
			default:
				WBD_WARNING("Vendor TLV[%s] processing not Supported by Slave.\n",
					wbd_tlv_get_type_str(ptlv->type));
				break;
		}

	}

	WBD_DEBUG("vendorSpec_len[%d] tlv_hdr_len[%d] tlv_data_len[%d] pos[%d]\n",
		msg_data->vendorSpec_len, tlv_hdr_len, tlv_data_len, pos);

	WBD_EXIT();
	return ret;
}

/* Slave get Vendor Specific TLV to send for 1905 Message */
void
wbd_slave_get_vendor_specific_tlv(i5_msg_types_with_vndr_tlv_t msg_type,
	ieee1905_vendor_data *out_vendor_tlv)
{
	WBD_ENTER();

	WBD_INFO("Vendor Specific TLV requested for : %s\n", wbd_get_1905_msg_str(msg_type));

	switch (msg_type) {

		case i5MsgMultiAPPolicyConfigRequestValue :
			/* Get Vendor TLV for 1905 Message : MultiAP Policy Config Request */
			break;

		default:
			WBD_WARNING("Invalid option\n");
			break;
	}

	WBD_EXIT();
}

/* Callback fn for Slave to do rc restart gracefully */
static void
wbd_slave_rc_restart_timer_cb(bcm_usched_handle *hdl, void *arg)
{
	wbd_info_t *info = (wbd_info_t *)arg;
	int ret = WBDE_OK;
	WBD_ENTER();

	WBD_ASSERT_ARG(info, WBDE_INV_ARG);
	/* nvram commit, rc restart  */
	if (info->flags & WBD_INFO_FLAGS_RC_RESTART) {
		wbd_do_rc_restart_reboot(WBD_FLG_RC_RESTART | WBD_FLG_NV_COMMIT);
	}
end:
	WBD_EXIT();
}

/* Create timer for Slave to do rc restart gracefully */
int
wbd_slave_create_rc_restart_timer(wbd_info_t *info)
{
	int ret = WBDE_OK;
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(info, WBDE_INV_ARG);

	/* If rc restart timer already created, don't create again */
	if (info->flags & WBD_INFO_FLAGS_IS_RC_RESTART_TM) {
		WBD_INFO("Restart timer already created\n");
		/* Postpone the timer by removing the existing timer and creating it again */
		wbd_remove_timers(info->hdl, wbd_slave_rc_restart_timer_cb, info);
		info->flags &= ~WBD_INFO_FLAGS_IS_RC_RESTART_TM;
	}

	/* Add the timer */
	ret = wbd_add_timers(info->hdl, info,
		WBD_SEC_MICROSEC(info->max.tm_rc_restart),
		wbd_slave_rc_restart_timer_cb, 0);

	/* If Add Timer Succeeded */
	if (ret == WBDE_OK) {
		/* Set rc restart timer created flag, else error */
		info->flags |= WBD_INFO_FLAGS_IS_RC_RESTART_TM;
	} else {
		WBD_WARNING("Failed to create rc restart timer for Interval[%d]\n",
			info->max.tm_rc_restart);
	}

end:
	WBD_EXIT();
	return ret;
}

/* Processes all WEAK_CLIENT data from BSD or from CLI */
static int
wbd_slave_process_weak_client_data(i5_dm_bss_type *i5_bss, struct ether_addr *sta_mac,
	wbd_cmd_weak_client_bsd_t *cmdweakclient)
{
	int ret = WBDE_OK;
	i5_dm_clients_type *i5_assoc_sta;
	wbd_assoc_sta_item_t *assoc_sta;
	WBD_ENTER();

	/* Get Assoc item pointer */
	i5_assoc_sta = wbd_ds_find_sta_in_bss_assoclist(i5_bss, sta_mac, &ret, &assoc_sta);
	WBD_ASSERT_MSG("Slave["MACDBG"] STA["MACF"]. %s\n", MAC2STRDBG(i5_bss->BSSID),
			ETHERP_TO_MACF(sta_mac), wbderrorstr(ret));

	/* No monitoring/steering on guest bss */
	if (I5_IS_BSS_GUEST(i5_bss->mapFlags)) {
		WBD_WARNING("Slave["MACDBG"] STA["MACF"] in Guest network\n",
			MAC2STRDBG(i5_bss->BSSID), ETHERP_TO_MACF(sta_mac));
		ret = WBDE_IGNORE_STA;
		goto end;
	}
	/* If its in ignore list */
	if (assoc_sta->status == WBD_STA_STATUS_IGNORE) {
		WBD_WARNING("Slave["MACDBG"] STA["MACF"] In Ignore list\n",
			MAC2STRDBG(i5_bss->BSSID), ETHERP_TO_MACF(sta_mac));
		ret = WBDE_IGNORE_STA;
		goto end;
	}
	if (I5_IS_BSS_STA(i5_assoc_sta->flags)) {
		WBD_WARNING("Slave["MACDBG"] STA["MACF"] is backhaul sta, skip\n",
			MAC2STRDBG(i5_bss->BSSID), ETHERP_TO_MACF(sta_mac));
		ret = WBDE_IGNORE_STA;
		goto end;
	}

	WBD_STEER("slave["MACDBG"] Found weak STA["MACF"]\n", MAC2STRDBG(i5_bss->BSSID),
		ETHERP_TO_MACF(sta_mac));

	if (cmdweakclient) {
		/* Update the stats locally from the stats we got it from the BSD */
		assoc_sta->stats.active = time(NULL);
		assoc_sta->stats.idle_rate = cmdweakclient->datarate;
		assoc_sta->stats.rssi = cmdweakclient->rssi;
		assoc_sta->stats.tx_tot_failures = cmdweakclient->tx_tot_failures;
		assoc_sta->stats.tx_failures = cmdweakclient->tx_failures;
		assoc_sta->stats.tx_rate = cmdweakclient->tx_rate;
		assoc_sta->stats.rx_tot_bytes = cmdweakclient->rx_tot_bytes;
		assoc_sta->stats.rx_tot_pkts = cmdweakclient->rx_tot_pkts;
	}

	/* Send WEAK_CLIENT command and Update STA Status = Weak */
	ret = wbd_slave_send_weak_client_cmd(i5_bss, i5_assoc_sta);

end: /* Check Slave Pointer before using it below */

	WBD_EXIT();
	return ret;
}

/* Slave updates WEAK_CLIENT_BSD Data */
static int
wbd_slave_process_weak_client_bsd_data(i5_dm_bss_type *i5_bss,
	wbd_cmd_weak_client_bsd_t *cmdweakclient)
{
	int ret = WBDE_OK;
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(i5_bss, WBDE_INV_ARG);

	/* Process WEAK_CLIENT data and send it to master */
	ret = wbd_slave_process_weak_client_data(i5_bss, &cmdweakclient->mac, cmdweakclient);

end: /* Check Slave Pointer before using it below */

	WBD_EXIT();
	return ret;
}

/* Processes WEAK_CLIENT_BSD request */
static void
wbd_slave_process_weak_client_bsd_cmd(wbd_com_handle *hndl, int childfd, void *cmddata, void *arg)
{
	int ret = WBDE_OK;
	wbd_cmd_weak_client_bsd_t *cmdweakclient = (wbd_cmd_weak_client_bsd_t*)cmddata;
	i5_dm_bss_type *i5_bss;
	i5_dm_interface_type *i5_ifr;
	char response[WBD_MAX_BUF_256];
	WBD_ENTER();

	/* Validate Message data */
	WBD_ASSERT_MSGDATA(cmdweakclient, "WEAK_CLIENT_BSD");

	/* Choose BSS based on mac */
	WBD_DS_GET_I5_SELF_BSS((unsigned char*)&cmdweakclient->cmdparam.dst_mac, i5_bss, &ret);
	WBD_ASSERT_MSG("Slave["MACF"] %s\n",
		ETHER_TO_MACF(cmdweakclient->cmdparam.dst_mac), wbderrorstr(ret));

	/* If the interface is not configured, dont accept weak client */
	i5_ifr = (i5_dm_interface_type*)WBD_I5LL_PARENT(i5_bss);
	if (!i5_ifr->isConfigured) {
		ret = WBDE_AGENT_NT_JOINED;
		WBD_WARNING("Band[%d] WEAK_CLIENT_BSD from BSD["MACF"] to ["MACF"]. Error : %s",
			cmdweakclient->cmdparam.band,
			ETHER_TO_MACF(cmdweakclient->cmdparam.src_mac),
			ETHER_TO_MACF(cmdweakclient->cmdparam.dst_mac), wbderrorstr(ret));
		goto end;
	}

	/* process the WEAK_CLIENT_BSD data */
	ret = wbd_slave_process_weak_client_bsd_data(i5_bss, cmdweakclient);
	WBD_ASSERT();

end: /* Check Slave Pointer before using it below */

	if (ret == WBDE_DS_UNKWN_SLV) {
		WBD_WARNING("Band[%d] WEAK_CLIENT_BSD from BSD["MACF"] to ["MACF"]. Error : %s",
			cmdweakclient->cmdparam.band,
			ETHER_TO_MACF(cmdweakclient->cmdparam.src_mac),
			ETHER_TO_MACF(cmdweakclient->cmdparam.dst_mac), wbderrorstr(ret));
	}

	/* Creates the WEAK_CLIENT_BSD response to send back */
	if (cmdweakclient) {
		snprintf(response, sizeof(response), "resp&mac="MACF"&errorcode=%d",
			ETHER_TO_MACF(cmdweakclient->mac), wbd_slave_wbd_to_bsd_error_code(ret));
		if (wbd_com_send_cmd(hndl, childfd, response, NULL) != WBDE_OK) {
			WBD_WARNING("Band[%d] Slave["MACF"] WEAK_CLIENT_BSD : %s\n",
				cmdweakclient->cmdparam.band,
				ETHER_TO_MACF(cmdweakclient->cmdparam.dst_mac),
				wbderrorstr(WBDE_SEND_RESP_FL));
		}

		free(cmdweakclient);
	}

	WBD_EXIT();
}

/* Slave updates WEAK_CANCEL_BSD Data */
static int
wbd_slave_process_weak_cancel_bsd_data(i5_dm_bss_type *i5_bss,
	wbd_cmd_weak_cancel_bsd_t *cmdweakcancel)
{
	int ret = WBDE_OK;
	i5_dm_clients_type *i5_assoc_sta;
	wbd_assoc_sta_item_t *assoc_sta;
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(i5_bss, WBDE_INV_ARG);

	/* Get Assoc item pointer */
	i5_assoc_sta = wbd_ds_find_sta_in_bss_assoclist(i5_bss, &cmdweakcancel->mac, &ret,
		&assoc_sta);
	WBD_ASSERT_MSG("Slave["MACDBG"] STA["MACF"]. %s\n", MAC2STRDBG(i5_bss->BSSID),
			ETHER_TO_MACF(cmdweakcancel->mac), wbderrorstr(ret));

	/* Update the stats locally from the stats we got it from the BSD */
	assoc_sta->stats.active = time(NULL);
	assoc_sta->stats.idle_rate = cmdweakcancel->datarate;
	assoc_sta->stats.rssi = cmdweakcancel->rssi;
	assoc_sta->stats.tx_tot_failures = cmdweakcancel->tx_tot_failures;
	assoc_sta->stats.tx_failures = cmdweakcancel->tx_failures;
	assoc_sta->stats.tx_rate = cmdweakcancel->tx_rate;
	assoc_sta->stats.rx_tot_bytes = cmdweakcancel->rx_tot_bytes;
	assoc_sta->stats.rx_tot_pkts = cmdweakcancel->rx_tot_pkts;

	/* Check if its already weak client */
	if (assoc_sta->status == WBD_STA_STATUS_WEAK ||
		assoc_sta->status == WBD_STA_STATUS_WEAK_STEERING) {
		WBD_STEER("Band[%d] slave["MACDBG"] Weak STA["MACF"] became strong status[%d]\n",
			cmdweakcancel->cmdparam.band, MAC2STRDBG(i5_bss->BSSID),
			ETHER_TO_MACF(cmdweakcancel->mac), assoc_sta->status);
		/* Send WEAK_CANCEL command and Update STA Status = Normal */
		ret = wbd_slave_send_weak_cancel_cmd(i5_bss, i5_assoc_sta);
	}

end: /* Check Slave Pointer before using it below */

	WBD_EXIT();
	return ret;
}

/* Processes WEAK_CANCEL_BSD request */
static void
wbd_slave_process_weak_cancel_bsd_cmd(wbd_com_handle *hndl, int childfd, void *cmddata, void *arg)
{
	int ret = WBDE_OK;
	wbd_cmd_weak_cancel_bsd_t *cmdweakcancel = (wbd_cmd_weak_cancel_bsd_t*)cmddata;
	i5_dm_bss_type *i5_bss;
	char response[WBD_MAX_BUF_256];
	WBD_ENTER();

	/* Validate Message data */
	WBD_ASSERT_MSGDATA(cmdweakcancel, "WEAK_CANCEL_BSD");

	/* Choose BSS based on mac */
	WBD_DS_GET_I5_SELF_BSS((unsigned char*)&cmdweakcancel->cmdparam.dst_mac, i5_bss, &ret);
	WBD_ASSERT_MSG("Slave["MACF"] %s\n",
		ETHER_TO_MACF(cmdweakcancel->cmdparam.dst_mac), wbderrorstr(ret));

	/* process the WEAK_CANCEL_BSD data */
	ret = wbd_slave_process_weak_cancel_bsd_data(i5_bss, cmdweakcancel);
	WBD_ASSERT();

end: /* Check Slave Pointer before using it below */

	if (ret == WBDE_DS_UNKWN_SLV) {
		WBD_WARNING("Band[%d] WEAK_CANCEL_BSD from BSD["MACF"] to ["MACF"]. Error : %s",
			cmdweakcancel->cmdparam.band,
			ETHER_TO_MACF(cmdweakcancel->cmdparam.src_mac),
			ETHER_TO_MACF(cmdweakcancel->cmdparam.dst_mac), wbderrorstr(ret));
	}

	/* Creates the WEAK_CANCEL_BSD response to send back */
	if (cmdweakcancel) {
		snprintf(response, sizeof(response), "resp&mac="MACF"&errorcode=%d",
			ETHER_TO_MACF(cmdweakcancel->mac), wbd_slave_wbd_to_bsd_error_code(ret));
		if (wbd_com_send_cmd(hndl, childfd, response, NULL) != WBDE_OK) {
			WBD_WARNING("Band[%d] Slave["MACF"] WEAK_CANCEL_BSD : %s\n",
				cmdweakcancel->cmdparam.band,
				ETHER_TO_MACF(cmdweakcancel->cmdparam.dst_mac),
				wbderrorstr(WBDE_SEND_RESP_FL));
		}

		free(cmdweakcancel);
	}

	WBD_EXIT();
}

/* Processes STA_STATUS_BSD request */
static void
wbd_slave_process_sta_status_bsd_cmd(wbd_com_handle *hndl, int childfd, void *cmddata, void *arg)
{
	int ret = WBDE_OK;
	wbd_cmd_sta_status_bsd_t *cmdstastatus = (wbd_cmd_sta_status_bsd_t*)cmddata;
	i5_dm_bss_type *i5_bss;
	wbd_assoc_sta_item_t *assoc_sta;
	char response[WBD_MAX_BUF_256];
	uint32 dwell_time = 0;
	WBD_ENTER();

	/* Validate Message data */
	WBD_ASSERT_MSGDATA(cmdstastatus, "STA_STATUS_BSD");

	/* Choose BSS based on mac */
	WBD_DS_GET_I5_SELF_BSS((unsigned char*)&cmdstastatus->cmdparam.dst_mac, i5_bss, &ret);
	WBD_ASSERT_MSG("Slave["MACF"] %s\n",
		ETHER_TO_MACF(cmdstastatus->cmdparam.dst_mac), wbderrorstr(ret));

	/* Get Assoc item pointer */
	wbd_ds_find_sta_in_bss_assoclist(i5_bss, &cmdstastatus->sta_mac, &ret,
		&assoc_sta);
	WBD_ASSERT_MSG("Slave["MACDBG"] STA["MACF"]. %s\n", MAC2STRDBG(i5_bss->BSSID),
			ETHER_TO_MACF(cmdstastatus->sta_mac), wbderrorstr(ret));

	ret = assoc_sta->error_code;
	dwell_time = assoc_sta->dwell_time;

end: /* Check Slave Pointer before using it below */

	if (ret == WBDE_DS_UNKWN_SLV) {
		WBD_WARNING("Band[%d] STA_STATUS_BSD from BSD["MACF"] to ["MACF"]. Error : %s",
			cmdstastatus->cmdparam.band, ETHER_TO_MACF(cmdstastatus->cmdparam.src_mac),
			ETHER_TO_MACF(cmdstastatus->cmdparam.dst_mac), wbderrorstr(ret));
	}
	/* Creates the STA_STATUS_BSD response to send back */
	if (cmdstastatus) {
		snprintf(response, sizeof(response), "resp&mac="MACF"&errorcode=%d&dwell=%lu",
			ETHER_TO_MACF(cmdstastatus->sta_mac), wbd_slave_wbd_to_bsd_error_code(ret),
			(unsigned long)dwell_time);
		if (wbd_com_send_cmd(hndl, childfd, response, NULL) != WBDE_OK) {
			WBD_WARNING("Band[%d] Slave["MACF"] STA_STATUS_BSD : %s\n",
				cmdstastatus->cmdparam.band,
				ETHER_TO_MACF(cmdstastatus->cmdparam.dst_mac),
				wbderrorstr(WBDE_SEND_RESP_FL));
		}

		free(cmdstastatus);
	}

	WBD_EXIT();
}

/* Convert WBD error code to BSD error code */
static int
wbd_slave_wbd_to_bsd_error_code(int error_code)
{
	int ret_code = 0;
	WBD_ENTER();

	switch (error_code) {
		case WBDE_OK:
			ret_code = WBDE_BSD_OK;
			break;

		case WBDE_IGNORE_STA:
			ret_code = WBDE_BSD_IGNORE_STA;
			break;

		case WBDE_NO_SLAVE_TO_STEER:
			ret_code = WBDE_BSD_NO_SLAVE_TO_STEER;
			break;

		case WBDE_DS_BOUNCING_STA:
			ret_code = WBDE_BSD_DS_BOUNCING_STA;
			break;

		default:
			ret_code = WBDE_BSD_FAIL;
			break;
	}

	WBD_EXIT();
	return ret_code;
}

/* -------------------------- Add New Functions above this -------------------------------- */

/* Get the SLAVELIST CLI command data */
static int
wbd_slave_process_slavelist_cli_cmd_data(wbd_info_t *info, wbd_cli_send_data_t *clidata,
	char **outdataptr)
{
	int ret = WBDE_OK, band_check = 0, outlen = WBD_MAX_BUF_8192, len = 0, count = 0;
	char *outdata = NULL;
	i5_dm_device_type *device;
	i5_dm_interface_type *ifr;
	i5_dm_bss_type *bss;

	outdata = (char*)wbd_malloc(outlen, &ret);
	WBD_ASSERT();

	/* Validate fn args */
	WBD_ASSERT_ARG(info, WBDE_INV_ARG);
	WBD_ASSERT_MSGDATA(clidata, "SLAVELIST_CLI");

	/* Check if Band is requested */
	if (WBD_BAND_VALID((clidata->band))) {
		band_check = 1;
	}

	WBD_SAFE_GET_I5_SELF_DEVICE(device, &ret);
	ret = wbd_snprintf_i5_device(device, CLI_CMD_I5_DEV_FMT, &outdata, &outlen, &len,
		WBD_MAX_BUF_8192);
	foreach_i5glist_item(ifr, i5_dm_interface_type, device->interface_list) {
		/* Skip non-wireless or interfaces with zero bss count */
		if (!i5DmIsInterfaceWireless(ifr->MediaType) || !ifr->BSSNumberOfEntries) {
			continue;
		}
		/* Band specific validation */
		if (band_check && clidata->band == WBD_BAND_FROM_CHANNEL(
			CHSPEC_CHANNEL(ifr->chanspec))) {
			continue;
		}
		foreach_i5glist_item(bss, i5_dm_bss_type, ifr->bss_list) {
			ret = wbd_snprintf_i5_bss(bss, CLI_CMD_I5_BSS_FMT, &outdata, &outlen,
				&len, WBD_MAX_BUF_8192, FALSE, FALSE);
			count++;
		}
	}

	if (!count) {
		snprintf(outdata + strlen(outdata), outlen - strlen(outdata),
			"No entry found \n");
	}

end: /* Check Slave Pointer before using it below */

	if (ret != WBDE_OK) {
		snprintf(outdata + strlen(outdata), outlen - strlen(outdata),
			"%s\n", wbderrorstr(ret));
	}
	if (outdataptr) {
		*outdataptr = outdata;
	}
	return ret;
}

/* Processes the SLAVELIST CLI command */
static void
wbd_slave_process_slavelist_cli_cmd(wbd_com_handle *hndl, int childfd, void *cmddata, void *arg)
{
	int ret = WBDE_OK;
	wbd_cli_send_data_t *clidata = (wbd_cli_send_data_t*)cmddata;
	wbd_info_t *info = (wbd_info_t*)arg;
	char *outdata = NULL;

	BCM_REFERENCE(ret);
	ret = wbd_slave_process_slavelist_cli_cmd_data(info, clidata, &outdata);

	if (outdata) {
		if (wbd_com_send_cmd(hndl, childfd, outdata, NULL) != WBDE_OK) {
			WBD_WARNING("SLAVELIST CLI : %s\n", wbderrorstr(WBDE_SEND_RESP_FL));
		}
		free(outdata);
	}

	if (clidata)
		free(clidata);

	return;
}

/* Get the INFO CLI command data */
static int
wbd_slave_process_info_cli_cmd_data(wbd_info_t *info, wbd_cli_send_data_t *clidata,
	char **outdataptr)
{
	int ret = WBDE_OK, outlen = WBD_MAX_BUF_8192, len = 0;
	char *outdata = NULL;
	int count = 0, band_check = 0;
	i5_dm_device_type *device;
	i5_dm_interface_type *ifr;
	i5_dm_bss_type *bss;

	outdata = (char*)wbd_malloc(outlen, &ret);
	WBD_ASSERT();

	/* Validate fn arg */
	WBD_ASSERT_ARG(info, WBDE_INV_ARG);
	WBD_ASSERT_MSGDATA(clidata, "INFO_CLI");

	/* Check if Band is requested */
	if (WBD_BAND_VALID((clidata->band))) {
		band_check = 1;
	}

	WBD_SAFE_GET_I5_CTLR_DEVICE(device, &ret);
	ret = wbd_snprintf_i5_device(device, CLI_CMD_I5_DEV_FMT, &outdata, &outlen, &len,
		WBD_MAX_BUF_8192);

	WBD_SAFE_GET_I5_SELF_DEVICE(device, &ret);
	ret = wbd_snprintf_i5_device(device, CLI_CMD_I5_DEV_FMT, &outdata, &outlen, &len,
		WBD_MAX_BUF_8192);
	foreach_i5glist_item(ifr, i5_dm_interface_type, device->interface_list) {
		/* Skip non-wireless or interfaces with zero bss count */
		if (!i5DmIsInterfaceWireless(ifr->MediaType) || !ifr->BSSNumberOfEntries) {
			continue;
		}
		/* Band specific validation */
		if (band_check && clidata->band == WBD_BAND_FROM_CHANNEL(
			CHSPEC_CHANNEL(ifr->chanspec))) {
			continue;
		}
		foreach_i5glist_item(bss, i5_dm_bss_type, ifr->bss_list) {
			ret = wbd_snprintf_i5_bss(bss, CLI_CMD_I5_BSS_FMT, &outdata, &outlen,
				&len, WBD_MAX_BUF_8192, TRUE, TRUE);
			count++;
		}
	}

	if (!count) {
		snprintf(outdata + strlen(outdata), outlen - strlen(outdata),
			"No entry found \n");
	}

end: /* Check Slave Pointer before using it below */

	if (ret != WBDE_OK) {
		snprintf(outdata + strlen(outdata), outlen - strlen(outdata),
			"%s\n", wbderrorstr(ret));
	}
	if (outdataptr) {
		*outdataptr = outdata;
	}
	return ret;
}

/* Processes the INFO CLI command */
static void
wbd_slave_process_info_cli_cmd(wbd_com_handle *hndl, int childfd, void *cmddata, void *arg)
{
	int ret = WBDE_OK;
	wbd_cli_send_data_t *clidata = (wbd_cli_send_data_t*)cmddata;
	wbd_info_t *info = (wbd_info_t*)arg;
	char *outdata = NULL;

	BCM_REFERENCE(ret);
	ret = wbd_slave_process_info_cli_cmd_data(info, clidata, &outdata);

	if (outdata) {
		if (wbd_com_send_cmd(hndl, childfd, outdata, NULL) != WBDE_OK) {
			WBD_WARNING("INFO CLI : %s\n", wbderrorstr(WBDE_SEND_RESP_FL));
		}
		free(outdata);
	}

	if (clidata)
		free(clidata);

	return;
}

/* Get the CLIENTLIST CLI command data */
static int
wbd_slave_process_clientlist_cli_cmd_data(wbd_info_t *info,
	wbd_cli_send_data_t *clidata, char **outdataptr)
{
	int ret = WBDE_OK, len = 0, outlen = WBD_MAX_BUF_8192, count = 0, band_check = 0;
	char *outdata = NULL;
	i5_dm_device_type *device;
	i5_dm_interface_type *ifr;
	i5_dm_bss_type *bss;

	/* Validate fn args */
	WBD_ASSERT_ARG(info, WBDE_INV_ARG);
	WBD_ASSERT_MSGDATA(clidata, "CLIENTLIST_CLI");

	outdata = (char*)wbd_malloc(outlen, &ret);
	WBD_ASSERT();

	/* Check if Band is requested */
	if (WBD_BAND_VALID((clidata->band))) {
		band_check = 1;
	}

	WBD_SAFE_GET_I5_SELF_DEVICE(device, &ret);
	ret = wbd_snprintf_i5_device(device, CLI_CMD_I5_DEV_FMT, &outdata, &outlen, &len,
		WBD_MAX_BUF_8192);
	foreach_i5glist_item(ifr, i5_dm_interface_type, device->interface_list) {
		/* Skip non-wireless or interfaces with zero bss count */
		if (!i5DmIsInterfaceWireless(ifr->MediaType) || !ifr->BSSNumberOfEntries) {
			continue;
		}
		/* Band specific validation */
		if (band_check && clidata->band == WBD_BAND_FROM_CHANNEL(
			CHSPEC_CHANNEL(ifr->chanspec))) {
			continue;
		}
		foreach_i5glist_item(bss, i5_dm_bss_type, ifr->bss_list) {
			ret = wbd_snprintf_i5_clients(bss, &outdata, &outlen, &len,
				WBD_MAX_BUF_8192, TRUE);
			count++;
		}
	}

	if (!count) {
		snprintf(outdata + strlen(outdata), outlen - strlen(outdata),
			"No entry found \n");
	}

end: /* Check Slave Pointer before using it below */

	if (ret != WBDE_OK) {
		snprintf(outdata + strlen(outdata), outlen - strlen(outdata),
			"%s\n", wbderrorstr(ret));
	}
	if (outdataptr) {
		*outdataptr = outdata;
	}
	return ret;
}

/* Processes the CLIENTLIST CLI command */
static void
wbd_slave_process_clientlist_cli_cmd(wbd_com_handle *hndl, int childfd, void *cmddata, void *arg)
{
	int ret = WBDE_OK;
	wbd_cli_send_data_t *clidata = (wbd_cli_send_data_t*)cmddata;
	wbd_info_t *info = (wbd_info_t*)arg;
	char *outdata = NULL;

	BCM_REFERENCE(ret);
	ret = wbd_slave_process_clientlist_cli_cmd_data(info, clidata, &outdata);

	if (outdata) {
		if (wbd_com_send_cmd(hndl, childfd, outdata, NULL) != WBDE_OK) {
			WBD_WARNING("CLIENTLIST CLI : %s\n", wbderrorstr(WBDE_SEND_RESP_FL));
		}
		free(outdata);
	}

	if (clidata)
		free(clidata);

	return;
}

/* Processes the MONITORADD CLI command data */
static int
wbd_slave_process_monitoradd_cli_cmd_data(wbd_info_t *info, wbd_cli_send_data_t *clidata,
	char **outdataptr)
{
	int ret = WBDE_OK;
	BCM_STAMON_STATUS status;
	bcm_stamon_maclist_t *tmp = NULL;
	struct ether_addr mac;
	char *outdata = NULL;
	int outlen = WBD_MAX_BUF_256;
	wbd_slave_item_t *slave = NULL;

	outdata = (char*)wbd_malloc(outlen, &ret);
	WBD_ASSERT();

	/* Validate arg */
	WBD_ASSERT_ARG(info, WBDE_INV_ARG);

	/* Validate Message data */
	WBD_ASSERT_MSGDATA(clidata, "MONITORADD_CLI");

	/* Choose Slave Info based on mac */
	WBD_GET_VALID_MAC(clidata->bssid, &mac, "MONITORADD_CLI", WBDE_INV_BSSID);
	WBD_SAFE_GET_SLAVE_ITEM(info, (&mac), WBD_CMP_MAC, slave, (&ret));

	/* Get Validated Non-NULL MAC */
	WBD_GET_VALID_MAC(clidata->mac, &mac, "MONITORADD_CLI", WBDE_INV_MAC);

	/* Add the MAC to stamon list */
	ret = wbd_slave_prepare_stamon_maclist(&mac, &tmp);
	if (ret != WBDE_OK && !tmp) {
		goto end;
	}

	/* Issue stamon add command */
	status = bcm_stamon_command(slave->stamon_hdl, BCM_STAMON_CMD_ADD,
		(void*)tmp, NULL);
	if (status != BCM_STAMONE_OK) {
		WBD_WARNING("Band[%d] Slave["MACF"] Failed to add MAC["MACF"]. Stamon error : %s\n",
			slave->band, ETHER_TO_MACF(slave->wbd_ifr.mac),
			ETHER_TO_MACF(mac), bcm_stamon_strerror(status));
		ret = WBDE_STAMON_ERROR;
		goto end;
	}

end: /* Check Slave Pointer before using it below */

	if (ret == WBDE_OK) {
		snprintf(outdata, outlen, "Successfully Added MAC : %s\n", clidata->mac);
	} else {
		snprintf(outdata, outlen, "Failed to Add. Error : %s\n", wbderrorstr(ret));
	}
	if (outdataptr) {
		*outdataptr = outdata;
	}
	if (tmp) {
		free(tmp);
	}

	return ret;
}

/* Processes the MONITORADD CLI command  */
static void
wbd_slave_process_monitoradd_cli_cmd(wbd_com_handle *hndl, int childfd, void *cmddata, void *arg)
{
	int ret = WBDE_OK;
	wbd_cli_send_data_t *clidata = (wbd_cli_send_data_t*)cmddata;
	wbd_info_t *info = (wbd_info_t*)arg;
	char *outdata = NULL;

	BCM_REFERENCE(ret);
	ret = wbd_slave_process_monitoradd_cli_cmd_data(info, clidata, &outdata);

	if (outdata) {
		if (wbd_com_send_cmd(hndl, childfd, outdata, NULL) != WBDE_OK) {
			WBD_WARNING("MONITORADD CLI : %s\n", wbderrorstr(WBDE_SEND_RESP_FL));
		}
		free(outdata);
	}

	if (clidata)
		free(clidata);

	return;
}

/* Processes the MONITORDEL CLI command data */
static int
wbd_slave_process_monitordel_cli_cmd_data(wbd_info_t *info, wbd_cli_send_data_t *clidata,
	char **outdataptr)
{
	int ret = WBDE_OK;
	BCM_STAMON_STATUS status;
	struct ether_addr mac;
	char *outdata = NULL;
	int outlen = WBD_MAX_BUF_256;
	wbd_slave_item_t *slave = NULL;

	outdata = (char*)wbd_malloc(outlen, &ret);
	WBD_ASSERT();

	/* Validate arg */
	WBD_ASSERT_ARG(info, WBDE_INV_ARG);

	/* Validate Message data */
	WBD_ASSERT_MSGDATA(clidata, "MONITORDEL_CLI");

	/* Choose Slave Info based on mac */
	WBD_GET_VALID_MAC(clidata->bssid, &mac, "MONITORDEL_CLI", WBDE_INV_BSSID);
	WBD_SAFE_GET_SLAVE_ITEM(info, (&mac), WBD_CMP_MAC, slave, (&ret));

	if (strlen(clidata->mac) > 0) {

		/* Get Validated Non-NULL MAC */
		WBD_GET_VALID_MAC(clidata->mac, &mac, "MONITORDEL_CLI", WBDE_INV_MAC);

		ret = wbd_slave_remove_sta_fm_stamon(slave, &mac, &status);
		goto end;
	}

	/* If the MAC field is empty delete all */
	status = bcm_stamon_command(slave->stamon_hdl, BCM_STAMON_CMD_DEL, NULL, NULL);
	if (status != BCM_STAMONE_OK) {
		WBD_WARNING("Band[%d] Slave["MACF"] Failed to delete STA. Stamon error : %s\n",
			slave->band, ETHER_TO_MACF(slave->wbd_ifr.mac),
			bcm_stamon_strerror(status));
		ret = WBDE_STAMON_ERROR;
		goto end;
	}

end: /* Check Slave Pointer before using it below */

	if (ret == WBDE_OK) {
		if (strlen(clidata->mac) <= 0) {
			snprintf(outdata, outlen, "Successfully Deleted All STAs\n");
		} else {
			snprintf(outdata, outlen, "Successfully Deleted MAC : %s\n", clidata->mac);
		}
	} else {
		snprintf(outdata, outlen, "Failed to Delete. Error : %s. Stamon Error : %s\n",
			wbderrorstr(ret), bcm_stamon_strerror(status));
	}
	if (outdataptr) {
		*outdataptr = outdata;
	}

	return ret;
}

/* Processes the MONITORDEL CLI command  */
static void
wbd_slave_process_monitordel_cli_cmd(wbd_com_handle *hndl, int childfd, void *cmddata, void *arg)
{
	int ret = WBDE_OK;
	wbd_cli_send_data_t *clidata = (wbd_cli_send_data_t*)cmddata;
	wbd_info_t *info = (wbd_info_t*)arg;
	char *outdata = NULL;

	BCM_REFERENCE(ret);
	ret = wbd_slave_process_monitordel_cli_cmd_data(info, clidata, &outdata);

	if (outdata) {
		if (wbd_com_send_cmd(hndl, childfd, outdata, NULL) != WBDE_OK) {
			WBD_WARNING("MONITORDEL CLI : %s\n", wbderrorstr(WBDE_SEND_RESP_FL));
		}
		free(outdata);
	}

	if (clidata)
		free(clidata);

	return;
}

/* Get the MONITORLIST CLI command data */
static int
wbd_slave_process_monitorlist_cli_cmd_data(wbd_info_t *info, wbd_cli_send_data_t *clidata,
	char **outdataptr)
{
	int ret = WBDE_OK, index = 0;
	BCM_STAMON_STATUS status;
	struct ether_addr mac;
	bcm_stamon_maclist_t *tmp = NULL;
	bcm_stamon_list_info_t *list = NULL;
	char *outdata = NULL;
	int outlen = WBD_MAX_BUF_2048, totlen = 0, i;
	char tmpline[WBD_MAX_BUF_512];
	wbd_slave_item_t *slave = NULL;

	outdata = (char*)wbd_malloc(outlen, &ret);
	WBD_ASSERT();

	/* Validate arg */
	WBD_ASSERT_ARG(info, WBDE_INV_ARG);

	/* Validate Message data */
	WBD_ASSERT_MSGDATA(clidata, "MONITORLIST_CLI");

	/* Choose Slave Info based on mac */
	WBD_GET_VALID_MAC(clidata->bssid, &mac, "MONITORLIST_CLI", WBDE_INV_BSSID);
	WBD_SAFE_GET_SLAVE_ITEM(info, &mac, WBD_CMP_MAC, slave, (&ret));

	if (strlen(clidata->mac) > 0) {

		/* Get Validated Non-NULL MAC */
		WBD_GET_VALID_MAC(clidata->mac, &mac, "MONITORLIST_CLI", WBDE_INV_MAC);

		ret = wbd_slave_prepare_stamon_maclist(&mac, &tmp);
		WBD_ASSERT();

		status = bcm_stamon_command(slave->stamon_hdl, BCM_STAMON_CMD_GET, (void*)tmp,
			(void**)&list);
	} else {
		/* If the MAC field is empty get all */
		status = bcm_stamon_command(slave->stamon_hdl, BCM_STAMON_CMD_GET, NULL,
			(void**)&list);
	}

	if (status != BCM_STAMONE_OK && !list) {
		snprintf(outdata, outlen, "Stamon error : %s\n",
			bcm_stamon_strerror(status));
		ret = WBDE_STAMON_ERROR;
		goto end;
	}

	snprintf(outdata + strlen(outdata), outlen - strlen(outdata),
		"Total no of Monitored STA's %d\n", list->list_info_len);

	totlen = strlen(outdata);

	for (i = 0; i < list->list_info_len; i++) {
		snprintf(tmpline, sizeof(tmpline), "\tSTA[%d] : "MACF"   RSSI  %d   Status  %d\n",
			++index, ETHER_TO_MACF(list->info[i].ea),
			list->info[i].rssi, list->info[i].status);

		wbd_strcat_with_realloc_buffer(&outdata, &outlen, &totlen,
			WBD_MAX_BUF_2048, tmpline);
	}

end: /* Check Slave Pointer before using it below */

	if (ret != WBDE_OK) {
		snprintf(outdata + strlen(outdata), outlen - strlen(outdata),
			"%s\n", wbderrorstr(ret));
	}
	if (outdataptr) {
		*outdataptr = outdata;
	}

	if (tmp)
		free(tmp);
	if (list)
		free(list);

	return ret;
}

/* Processes the MONITORLIST CLI command */
static void
wbd_slave_process_monitorlist_cli_cmd(wbd_com_handle *hndl, int childfd, void *cmddata, void *arg)
{
	int ret = WBDE_OK;
	wbd_cli_send_data_t *clidata = (wbd_cli_send_data_t*)cmddata;
	wbd_info_t *info = (wbd_info_t*)arg;
	char *outdata = NULL;

	BCM_REFERENCE(ret);
	ret = wbd_slave_process_monitorlist_cli_cmd_data(info, clidata, &outdata);

	if (outdata) {
		if (wbd_com_send_cmd(hndl, childfd, outdata, NULL) != WBDE_OK) {
			WBD_WARNING("MONITORLIST CLI : %s\n", wbderrorstr(WBDE_SEND_RESP_FL));
		}
		free(outdata);
	}

	if (clidata)
		free(clidata);

	return;
}

/* Processes the WEAK_CLIENT CLI command data */
static int
wbd_slave_process_weak_client_cli_cmd_data(wbd_info_t *info, wbd_cli_send_data_t *clidata,
	char **outdataptr)
{
	int ret = WBDE_OK;
	char *outdata = NULL;
	int outlen = WBD_MAX_BUF_256;
	struct ether_addr mac;
	i5_dm_bss_type *i5_bss;

	outdata = (char*)wbd_malloc(outlen, &ret);
	WBD_ASSERT();

	/* Validate arg */
	WBD_ASSERT_ARG(info, WBDE_INV_ARG);

	/* Validate Message data */
	WBD_ASSERT_MSGDATA(clidata, "WEAK_CLIENT_CLI");

	/* Validate bssid of clidata */
	WBD_GET_VALID_MAC(clidata->bssid, &mac, "WEAK_CLIENT_CLI", WBDE_INV_BSSID);

	/* Choose BSS based on mac */
	WBD_DS_GET_I5_SELF_BSS((unsigned char*)&mac, i5_bss, &ret);
	WBD_ASSERT_MSG("Slave["MACF"] %s\n", ETHER_TO_MACF(mac), wbderrorstr(ret));

	/* Get Validated Non-NULL MAC */
	WBD_GET_VALID_MAC(clidata->mac, &mac, "WEAK_CLIENT_CLI", WBDE_INV_MAC);

	/* Process WEAK_CLIENT data and send it to master */
	ret = wbd_slave_process_weak_client_data(i5_bss, &mac, NULL);

end: /* Check Slave Pointer before using it below */

	if (ret == WBDE_OK) {
		snprintf(outdata, outlen, "MAC : %s is WEAK\n", clidata->mac);
	} else {
		snprintf(outdata, outlen, "Failed to Issue WEAK_CLIENT. Error : %s\n",
			wbderrorstr(ret));
	}
	if (outdataptr) {
		*outdataptr = outdata;
	}

	return ret;
}

/* Processes the WEAK_CLIENT CLI command  */
static void
wbd_slave_process_weak_client_cli_cmd(wbd_com_handle *hndl, int childfd, void *cmddata, void *arg)
{
	int ret = WBDE_OK;
	wbd_cli_send_data_t *clidata = (wbd_cli_send_data_t*)cmddata;
	wbd_info_t *info = (wbd_info_t*)arg;
	char *outdata = NULL;

	BCM_REFERENCE(ret);
	ret = wbd_slave_process_weak_client_cli_cmd_data(info, clidata, &outdata);

	if (outdata) {
		if (wbd_com_send_cmd(hndl, childfd, outdata, NULL) != WBDE_OK) {
			WBD_WARNING("WEAK_CLIENT CLI : %s\n", wbderrorstr(WBDE_SEND_RESP_FL));
		}
		free(outdata);
	}

	if (clidata)
		free(clidata);

	return;
}

/* Processes the WEAK_CANCEL CLI command data */
static int
wbd_slave_process_weak_cancel_cli_cmd_data(wbd_info_t *info, wbd_cli_send_data_t *clidata,
	char **outdataptr)
{
	int ret = WBDE_OK;
	char *outdata = NULL;
	int outlen = WBD_MAX_BUF_256;
	struct ether_addr mac;
	i5_dm_bss_type *i5_bss;
	i5_dm_clients_type *i5_assoc_sta;
	wbd_assoc_sta_item_t *assoc_sta;

	outdata = (char*)wbd_malloc(outlen, &ret);
	WBD_ASSERT();

	/* Validate arg */
	WBD_ASSERT_ARG(info, WBDE_INV_ARG);

	/* Validate Message data */
	WBD_ASSERT_MSGDATA(clidata, "WEAK_CANCEL_CLI");

	/* Validate bssid of clidata */
	WBD_GET_VALID_MAC(clidata->bssid, &mac, "WEAK_CANCEL_CLI", WBDE_INV_BSSID);

	/* Choose BSS based on mac */
	WBD_DS_GET_I5_SELF_BSS((unsigned char*)&mac, i5_bss, &ret);
	WBD_ASSERT_MSG("Slave["MACF"] %s\n", ETHER_TO_MACF(mac), wbderrorstr(ret));

	/* Get Validated Non-NULL MAC */
	WBD_GET_VALID_MAC(clidata->mac, &mac, "WEAK_CANCEL_CLI", WBDE_INV_MAC);

	/* Get Assoc item pointer */
	i5_assoc_sta = wbd_ds_find_sta_in_bss_assoclist(i5_bss, &mac, &ret, &assoc_sta);
	WBD_ASSERT_MSG("Slave["MACDBG"] STA["MACF"]. %s\n", MAC2STRDBG(i5_bss->BSSID),
			ETHER_TO_MACF(mac), wbderrorstr(ret));

	/* Check if its already weak client */
	if (assoc_sta->status == WBD_STA_STATUS_WEAK ||
		assoc_sta->status == WBD_STA_STATUS_WEAK_STEERING) {
		/* Send WEAK_CANCEL command and Update STA Status = Normal */
		ret = wbd_slave_send_weak_cancel_cmd(i5_bss, i5_assoc_sta);
	}

end: /* Check Slave Pointer before using it below */

	if (ret == WBDE_OK) {
		snprintf(outdata, outlen, "MAC : %s is Normal\n", clidata->mac);
	} else {
		snprintf(outdata, outlen, "Failed to Issue WEAK_CANCEL. Error : %s\n",
			wbderrorstr(ret));
	}
	if (outdataptr) {
		*outdataptr = outdata;
	}

	return ret;
}

/* Processes the WEAK_CANCEL CLI command  */
static void
wbd_slave_process_weak_cancel_cli_cmd(wbd_com_handle *hndl, int childfd, void *cmddata, void *arg)
{
	int ret = WBDE_OK;
	wbd_cli_send_data_t *clidata = (wbd_cli_send_data_t*)cmddata;
	wbd_info_t *info = (wbd_info_t*)arg;
	char *outdata = NULL;

	BCM_REFERENCE(ret);
	ret = wbd_slave_process_weak_cancel_cli_cmd_data(info, clidata, &outdata);

	if (outdata) {
		if (wbd_com_send_cmd(hndl, childfd, outdata, NULL) != WBDE_OK) {
			WBD_WARNING("WEAK_CANCEL CLI : %s\n", wbderrorstr(WBDE_SEND_RESP_FL));
		}
		free(outdata);
	}

	if (clidata)
		free(clidata);

	return;
}

/* Processes REMOVE_CLIENT_REQ cmd */
static void
wbd_slave_process_remove_client_cmd(unsigned char *neighbor_al_mac,
	unsigned char *tlv_data, unsigned short tlv_data_len)
{
	int ret = WBDE_OK;
	wbd_cmd_remove_client_t cmd;
	i5_dm_clients_type *i5_assoc_sta = NULL;
	i5_dm_bss_type *i5_parent_bss = NULL;
	struct ether_addr *parent_bssid = NULL;
	i5_dm_device_type *sdev = NULL;
	i5_dm_interface_type *ifr = NULL;

	WBD_ENTER();

	/* Validate Message data */
	WBD_ASSERT_MSGDATA(tlv_data, "REMOVE_CLIENT_REQ");

	memset(&cmd, 0, sizeof(cmd));
	WBD_SAFE_GET_I5_SELF_DEVICE(sdev, &ret);

	/* Decode Vendor Specific TLV for Message : weak client response on receive */
	ret = wbd_tlv_decode_remove_client_request((void *)&cmd, tlv_data, tlv_data_len);
	if (ret != WBDE_OK) {
		WBD_WARNING("Failed to decode the REMOVE_CLIENT_REQ TLV\n");
		goto end;
	}

	foreach_i5glist_item(ifr, i5_dm_interface_type, sdev->interface_list) {

		foreach_i5glist_item(i5_parent_bss, i5_dm_bss_type, ifr->bss_list) {

			if (!(ETHER_ISNULLADDR((struct ether_addr*)&cmd.bssid))) {
				if (!memcmp(&cmd.bssid, &i5_parent_bss->BSSID, MAC_ADDR_LEN)) {
					continue;
				}
			}

			i5_assoc_sta = i5DmFindClientInBSS(i5_parent_bss,
				(unsigned char*)&cmd.stamac);
			if (i5_assoc_sta == NULL || i5_assoc_sta->vndr_data == NULL) {
				continue;
			}

			parent_bssid = (struct ether_addr*)i5_parent_bss->BSSID;

			/* Remove a STA item from a Slave's Assoc STA List */
			ret = blanket_sta_assoc_disassoc(parent_bssid, &cmd.stamac, 0, 0,
				1, NULL, 0, 0);
			if (ret == WBDE_OK) {
				/* Update Driver to remove stale client entry,
				 *  at present reason is fixed i.e.
				 *  DOT11RC_DEAUTH_LEAVING, it can be generalized
				 *  if required.
				 */
				ret = blanket_deauth_sta(i5_parent_bss->ifname, &cmd.stamac,
					DOT11_RC_DEAUTH_LEAVING);
			} else {
				WBD_WARNING("BSS["MACF"] STA["MACF"]. "
					"Failed to remove from assoclist. Error : %s\n",
					ETHER_TO_MACF(*parent_bssid),
					ETHER_TO_MACF(cmd.stamac), wbderrorstr(ret));
			}
		}
	}

end:
	WBD_EXIT();
}

/* Processes BLOCK_CLIENT_BSD request */
static void
wbd_slave_process_blk_client_bsd_cmd(wbd_com_handle *hndl, int childfd, void *cmddata, void *arg)
{
	int ret = WBDE_OK;
	wbd_cmd_block_client_t *cmdblkclient = (wbd_cmd_block_client_t*)cmddata;
	wbd_info_t *info = (wbd_info_t*)arg;
	i5_dm_bss_type *i5_bss;
	char response[WBD_MAX_BUF_256] = {0};

	WBD_ENTER();

	/* Validate Message data */
	WBD_ASSERT_MSGDATA(cmdblkclient, "BLOCK_CLIENT_BSD");

	/* Choose BSS based on mac */
	WBD_DS_GET_I5_SELF_BSS((unsigned char*)&cmdblkclient->cmdparam.dst_mac, i5_bss, &ret);
	WBD_ASSERT_MSG("Slave["MACF"] %s\n",
		ETHER_TO_MACF(cmdblkclient->cmdparam.dst_mac), wbderrorstr(ret));

	/* Dont send block client if the timeout is not provided */
	if (info->max.tm_blk_sta <= 0) {
		goto end;
	}

	wbd_slave_send_assoc_control(info->max.tm_blk_sta, i5_bss->BSSID,
		(unsigned char*)&cmdblkclient->tbss_mac, &cmdblkclient->mac);

end:

	 /* Creates the BLOCK_CLIENT_BSD response to send back */
	 if (cmdblkclient) {
		snprintf(response, sizeof(response), "resp&mac="MACF"&errorcode=%d",
			ETHER_TO_MACF(cmdblkclient->cmdparam.src_mac),
			wbd_slave_wbd_to_bsd_error_code(ret));
		if (wbd_com_send_cmd(hndl, childfd, response, NULL) != WBDE_OK) {
			WBD_WARNING("Band[%d] Slave["MACF"] BLOCK_CLIENT_BSD : %s\n",
				cmdblkclient->cmdparam.band,
				ETHER_TO_MACF(cmdblkclient->cmdparam.dst_mac),
				wbderrorstr(WBDE_SEND_RESP_FL));
		}

		free(cmdblkclient);
	}

	WBD_EXIT();
}

/* Timer callback to read MAP's monitored STAs */
static void
wbd_slave_map_monitored_rssi_read_cb(bcm_usched_handle *hdl, void *arg)
{
	int ret = WBDE_OK;
	uint i, k;
	BCM_STAMON_STATUS status;
	bcm_stamon_list_info_t *list = NULL;
	wbd_slave_item_t *slave = NULL;
	wbd_slave_map_monitor_arg_t* monitor_arg = NULL;
	ieee1905_unassoc_sta_link_metric metric;
	ieee1905_unassoc_sta_link_metric_list *staInfo;
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(arg, WBDE_INV_ARG);

	monitor_arg = (wbd_slave_map_monitor_arg_t*)arg;
	slave = monitor_arg->slave;
	memset(&metric, 0, sizeof(metric));
	wbd_ds_glist_init((wbd_glist_t*)&metric.sta_list);

	/* get the list from stamon */
	status = bcm_stamon_command(slave->stamon_hdl, BCM_STAMON_CMD_GET, NULL, (void**)&list);
	if (status != BCM_STAMONE_OK && !list) {
		WBD_WARNING("Band[%d] Slave["MACF"] Failed to Get stamon stats. Stamon error : %s",
			slave->band, ETHER_TO_MACF(slave->wbd_ifr.mac),
			bcm_stamon_strerror(status));
		ret = WBDE_STAMON_ERROR;
		goto end;
	}

	/* Now parse through the list and create list to send it */
	for (i = 0; i < list->list_info_len; i++) {
		/* Check if is there any error for this STA */
		if (list->info[i].status != BCM_STAMONE_OK)
			continue;

		/* Find the MAC address */
		for (k = 0; k < monitor_arg->maclist->count; k++) {
			if (eacmp(&monitor_arg->maclist->mac[k], &list->info[i].ea)) {
				continue;
			}
			WBD_INFO("Found STAMON STA["MACF"] rssi %d\n",
				ETHER_TO_MACF(list->info[i].ea), list->info[i].rssi);
			/* Create the STA entry */
			staInfo =
				(ieee1905_unassoc_sta_link_metric_list*)wbd_malloc(sizeof(*staInfo),
				&ret);
			WBD_ASSERT();
			clock_gettime(CLOCK_REALTIME, &staInfo->queried);
			memcpy(staInfo->mac, &list->info[i].ea, ETHER_ADDR_LEN);
			/* TODO:add proper chanspec either from firmware or from local bcm stamon
			 * list for this sta
			 */
			staInfo->channel = monitor_arg->channel;
			staInfo->rcpi = WBD_RSSI_TO_RCPI(list->info[i].rssi);
			wbd_ds_glist_append((wbd_glist_t*)&metric.sta_list, (dll_t*)staInfo);
		}
	}
	if (list)
		free(list);
	/* Delete all STAs from monitor list */
	for (k = 0; k < monitor_arg->maclist->count; k++) {
		wbd_slave_remove_sta_fm_stamon(slave,
			(struct ether_addr*)&monitor_arg->maclist->mac[k], &status);
	}
	/* Send the list to IEEE1905 module */
	memcpy(&metric.neighbor_al_mac, monitor_arg->neighbor_al_mac,
		sizeof(metric.neighbor_al_mac));
	metric.opClass = monitor_arg->rclass;
	ieee1905_send_unassoc_sta_link_metric(&metric);
end:
	wbd_ds_glist_cleanup((wbd_glist_t*)&metric.sta_list);

	if (monitor_arg) {
		if (monitor_arg->maclist) {
			free(monitor_arg->maclist);
			monitor_arg->maclist = NULL;
		}
		free(monitor_arg);
	}

	WBD_EXIT();
}

/* Get channel utilization */
static int
wbd_slave_get_chan_util(char *ifname, unsigned char *chan_util)
{
	int ret = WBDE_OK, buflen = WBD_MAX_BUF_512;
	int busy = 0;
	char *data_buf = NULL;
	wl_chanim_stats_t *list;
	WBD_ENTER();

	data_buf = (char*)wbd_malloc(buflen, &ret);
	WBD_ASSERT_MSG("Ifname[%s] Failed to allocate memory for chanim_stats\n", ifname);
	ret = blanket_get_chanim_stats(ifname, WL_CHANIM_COUNT_ONE, data_buf, buflen);
	if (ret != WBDE_OK) {
		WBD_WARNING("Ifname[%s] Failed to get the chanim_stats. Err[%d]\n",
			ifname, ret);
		goto end;
	}

	list = (wl_chanim_stats_t*)data_buf;
	if (list->count <= 0) {
		WBD_WARNING("Ifname[%s] No chanim stats\n", ifname);
		goto end;
	}
	busy = list->stats[0].ccastats[1] + list->stats[0].ccastats[2] +
		list->stats[0].ccastats[3] + list->stats[0].ccastats[4] +
		list->stats[0].ccastats[7] + list->stats[0].ccastats[8];
	*chan_util = (unsigned char)((busy * 255) / 100);
	WBD_DEBUG("Ifname[%s] Busy[%d] chan_util[%d]\n", ifname, busy, *chan_util);
end:
	if (data_buf) {
		free(data_buf);
	}
	WBD_EXIT();
	return ret;

}

/* Get Interface metrics */
int
wbd_slave_process_get_interface_metric_cb(wbd_info_t *info, char *ifname,
	unsigned char *ifr_mac, ieee1905_interface_metric *metric)
{
	int ret = WBDE_OK;
	WBD_ENTER();

	ret = wbd_blanket_get_interface_metric(ifname, metric);

	WBD_EXIT();
	return ret;
}

/* Get AP metrics */
int
wbd_slave_process_get_ap_metric_cb(wbd_info_t *info, char *ifname,
	unsigned char *bssid, ieee1905_ap_metric *metric)
{
	int ret = WBDE_OK;
	int amsdu = 0, ampdu = 0, ampdu_ba_wsize = 0;
	int rate = 0, ppdu_time = 0;
	unsigned char chan_util;
#if defined(MULTIAPR2)
	blanket_counters_t counters;
#endif /* MULTIAPR2 */
	WBD_ENTER();

	ret = blanket_is_amsdu_enabled(ifname, &amsdu);
	ret = blanket_is_ampdu_enabled(ifname, &ampdu);
	ret = blanket_get_ampdu_ba_wsize(ifname, &ampdu_ba_wsize);
	ret = blanket_get_rate(ifname, &rate);
	ret = wbd_slave_get_chan_util(ifname, &chan_util);

	WBD_INFO("amsdu = %d, ampdu = %d, ampdu_ba_wsize =%d, rate = %d\n",
		amsdu, ampdu, ampdu_ba_wsize, rate);
	memset(metric, 0, sizeof(*metric));

	metric->include_bit_esp = IEEE1905_INCL_BIT_ESP_BE;

	/* access category */
	metric->esp_ac_be[0] |= ESP_BE;

	/* data format */
	if (ampdu) {
		metric->esp_ac_be[0] |= ESP_AMPDU_ENABLED;
	}

	if (amsdu) {
		metric->esp_ac_be[0] |= ESP_AMSDU_ENABLED;
	}

	/* BA window size */
	if (ampdu_ba_wsize == 64) {
		metric->esp_ac_be[0] |= ESP_BA_WSIZE_64;
	} else if (ampdu_ba_wsize >= 32) {
		metric->esp_ac_be[0] |= ESP_BA_WSIZE_32;
	} else if (ampdu_ba_wsize >= 16) {
		metric->esp_ac_be[0] |= ESP_BA_WSIZE_16;
	} else if (ampdu_ba_wsize >= 8) {
		metric->esp_ac_be[0] |= ESP_BA_WSIZE_8;
	} else if (ampdu_ba_wsize >= 6) {
		metric->esp_ac_be[0] |= ESP_BA_WSIZE_6;
	} else if (ampdu_ba_wsize >= 4) {
		metric->esp_ac_be[0] |= ESP_BA_WSIZE_4;
	} else if (ampdu_ba_wsize >= 2) {
		metric->esp_ac_be[0] |= ESP_BA_WSIZE_2;
	} else {
		metric->esp_ac_be[0] |= ESP_BA_WSIZE_NONE;
	}

	/* Estimated Air time fraction
	 * 255 representing 100%
	 */
	metric->esp_ac_be[1] = 255 - chan_util;

	/* Data PPDU Duration Target
	 * Duration to transmit 1 packet in
	 * units of 50 microseconds
	 */
	if (rate) {
		ppdu_time =  (1500 * 8) / (rate * 50);
	}

	if (ppdu_time) {
		metric->esp_ac_be[2] = (unsigned char)ppdu_time;
	} else {
		/* Avoid sending out 0 as ppdu_time */
		metric->esp_ac_be[2] = 1;
	}

#if defined(MULTIAPR2)
	memset(&counters, 0, sizeof(counters));
	blanket_get_counters(ifname, &counters);
	metric->unicastBytesSent = (unsigned int)counters.txbyte;
	metric->unicastBytesReceived = (unsigned int)counters.rxbyte;
	metric->multicastBytesSent = (unsigned int)counters.txmcastframe;
	metric->multicastBytesReceived = (unsigned int)counters.rxmcastframe;
	metric->broadcastBytesSent = (unsigned int)counters.txbcastframe;
	metric->broadcastBytesReceived = (unsigned int)counters.rxbcastframe;

	WBD_INFO("Ifname[%s] ESP IE = 0x%x %x %x txbyte[%d] rxbyte[%d] txmcastframe[%d] "
		"rxmcastframe[%d] txbcastframe[%d] rxbcastframe[%d]\n",
		ifname, metric->esp_ac_be[0], metric->esp_ac_be[1], metric->esp_ac_be[2],
		metric->unicastBytesSent, metric->unicastBytesReceived, metric->multicastBytesSent,
		metric->multicastBytesReceived, metric->broadcastBytesSent,
		metric->broadcastBytesReceived);
#endif /* MULTIAPR2 */
	WBD_EXIT();
	return ret;
}

/* Add STAs to sta monitor to measure the RSSI */
int
wbd_slave_process_get_unassoc_sta_metric_cb(wbd_info_t *info,
	ieee1905_unassoc_sta_link_metric_query *query)
{
	int ret = WBDE_OK, i, band;
	BCM_STAMON_STATUS status;
	uint bw = 0, channel;
	chanspec_t chspec;
	bcm_stamon_maclist_t *stamonlist = NULL;
	wbd_slave_item_t *slave = NULL;
	wbd_slave_map_monitor_arg_t *param = NULL;
	unassoc_query_per_chan_rqst *prqst = NULL;
	wbd_maclist_t *maclist = NULL;
	int maclist_size = 0;
	uint8 nextidx = 0;
	i5_dm_device_type *self_dev;
	i5_dm_interface_type *i5_ifr;
	i5_dm_bss_type *i5_bss, *i5_assoc_bss;
	i5_dm_clients_type *i5_assoc_sta;
	WBD_ENTER();

	WBD_ASSERT_ARG(query, WBDE_INV_ARG);
	if (query->chCount <= 0) {
		WBD_WARNING("No Mac list \n");
		goto end;
	}

	WBD_SAFE_GET_I5_SELF_DEVICE(self_dev, &ret);

	prqst = query->data;
	for (i = 0; i < query->chCount; i++) {
		/* get channel */
		struct ether_addr *mac_addr = NULL;
		struct ether_addr *maclist_ea = NULL;
		uint8 n_sta = 0;
		uint8 count = 0;
		bcm_offchan_sta_cbfn *cbfn = NULL;
		channel = prqst[i].chan;

		/* Prepare the chanspec */
		blanket_get_bw_from_rc(query->opClass, &bw);
		chspec = wf_channel2chspec(channel, bw);
		if (!wf_chspec_valid(chspec)) {
			/* traverse next element */
			WBD_INFO("Invalid chanspec. RClass[%d] bw[0x%x] chspec[0x%x]\n",
				query->opClass, bw, chspec);
			continue;
		}

		n_sta = prqst[i].n_sta;
		if (!n_sta) {
			/* traverse next element */
			continue;
		}
		stamonlist = wbd_slave_alloc_stamon_maclist_struct(n_sta);
		if (!stamonlist) {
			ret = WBDE_MALLOC_FL;
			WBD_WARNING("Error: %s\n", wbderrorstr(ret));
			goto end;
		}
		maclist_size = OFFSETOF(wbd_maclist_t, mac) + (n_sta * sizeof(struct ether_addr));
		maclist = (wbd_maclist_t*)wbd_malloc(maclist_size, &ret);
		WBD_ASSERT();

		mac_addr = (struct ether_addr*)prqst[i].mac_list;
		maclist_ea = (struct ether_addr*)&(maclist->mac);

		/* start for each element */
		nextidx = 0;
		for (count = 0; count < n_sta; count++) {
			uint8 map_flags = IEEE1905_MAP_FLAG_FRONTHAUL;
			wbd_assoc_sta_item_t* assoc_sta = NULL;
			i5_dm_device_type *i5_assoc_dev = NULL;
			i5_dm_interface_type *i5_assoc_ifr = NULL;

			/* Find the STA in the topology */
			i5_assoc_sta = wbd_ds_find_sta_in_topology((unsigned char*)&mac_addr[count],
				&ret);
			if (i5_assoc_sta) {
				assoc_sta = (wbd_assoc_sta_item_t*)i5_assoc_sta->vndr_data;
				i5_assoc_bss = (i5_dm_bss_type*)WBD_I5LL_PARENT(i5_assoc_sta);
				i5_assoc_ifr = (i5_dm_interface_type*)WBD_I5LL_PARENT(i5_assoc_bss);
				i5_assoc_dev = (i5_dm_device_type*)WBD_I5LL_PARENT(i5_assoc_ifr);
			} else {
				WBD_INFO("STA["MACF"] not found in topology\n",
					ETHER_TO_MACF(mac_addr[count]));
			}

			/* Get the band, then get the BSS in the local device based of map flags.
			 * From the that BSSID, get the slave item
			 */
			band = WBD_BAND_FROM_CHANNEL(channel);
			if (i5_assoc_sta && I5_CLIENT_IS_BSTA(i5_assoc_sta)) {
				map_flags = IEEE1905_MAP_FLAG_BACKHAUL;
				/* If the STA is backhaul STA, dont monitor if the STA MAC address
				 * is in self device
				 */
				if (wbd_ds_get_self_i5_interface((unsigned char*)&mac_addr[count],
					&ret)) {
					WBD_INFO("STA["MACDBG"] is backhaul STA and its from "
						"same device\n", MAC2STRDBG(i5_assoc_sta->mac));
					continue;
				}
			}

			/* Find the BSS based on MAP flags and Band. So that the backhaul STA
			 * will be monitored in backhaul BSS and Fronthaul STA will be monitored
			 * in fronthaul BSS. By defualt MAP Flags is fronthaul. So if it is
			 * not able to find the backhaul BSS, it will still use the fronthaul BSS
			 * with matching band
			 */
			WBD_DS_FIND_I5_BSS_IN_DEVICE_FOR_BAND_AND_MAPFLAG(self_dev, band, i5_bss,
				map_flags, &ret);
			if ((ret != WBDE_OK) || !i5_bss) {
				WBD_INFO("Device["MACDBG"] Band[%d] : %s\n",
					MAC2STRDBG(self_dev->DeviceId), band, wbderrorstr(ret));
				continue;
			}
			WBD_DEBUG("Device["MACDBG"] Band[%d] BSS["MACDBG"]\n",
				MAC2STRDBG(self_dev->DeviceId), band, MAC2STRDBG(i5_bss->BSSID));
			i5_ifr = (i5_dm_interface_type*)WBD_I5LL_PARENT(i5_bss);
			if (chspec != i5_ifr->chanspec) {
				cbfn = wbd_slave_offchan_sta_cb;
			}

			slave = wbd_ds_find_slave_addr_in_blanket_slave(info->wbd_slave,
				(struct ether_addr*)i5_bss->BSSID, WBD_CMP_BSSID, &ret);
			if (!slave) {
				/* look for another element in query */
				WBD_INFO("Device["MACDBG"] Band[%d] BSSID["MACDBG"] : %s\n",
					MAC2STRDBG(self_dev->DeviceId), band,
					MAC2STRDBG(i5_bss->BSSID), wbderrorstr(ret));
				continue;
			}

			/* if passed STA mac address belongs to assoclist in slave,
			 * skip the operation and send action frame to this sta. Let
			 * other agent listen to the response from sta and measure
			 * the rssi
			 */
			if (i5_assoc_sta && (i5_assoc_dev == i5DmGetSelfDevice())) {
				if (assoc_sta == NULL) {
					WBD_WARNING("STA["MACDBG"] NULL Vendor Data\n",
						MAC2STRDBG(i5_assoc_sta->mac));
					continue;
				}

				/* Create a timer to send ACTION frame to the sta */
				if (assoc_sta->is_offchan_actframe_tm) {
					continue;
				}
				ret = wbd_slave_send_action_frame_to_sta(info, slave,
					(i5_dm_bss_type*)WBD_I5LL_PARENT(i5_assoc_sta),
					(struct ether_addr*)&i5_assoc_sta->mac);
				if (ret == WBDE_OK) {
					assoc_sta->is_offchan_actframe_tm = 1;
				}
			} else {
				nextidx = wbd_slave_add_sta_to_stamon_maclist(stamonlist,
					(struct ether_addr*)&mac_addr[count],
					BCM_STAMON_PRIO_MEDIUM, chspec,	nextidx,
					cbfn, (void*)slave);
				memcpy(&maclist_ea[count], &mac_addr[count],
					sizeof(struct ether_addr));

				maclist->count++;
			}
		}
		/* Add the list to the stamon module */
		if (nextidx > 0) {
			stamonlist->count = nextidx;
			status = bcm_stamon_command(slave->stamon_hdl, BCM_STAMON_CMD_ADD,
				(void*)stamonlist, NULL);
			if (status != BCM_STAMONE_OK) {
				WBD_WARNING("Band[%d] Slave["MACF"] Failed to add %d MAC to stamon "
					"Stamon error : %s\n", slave->band,
					ETHER_TO_MACF(slave->wbd_ifr.mac), stamonlist->count,
					bcm_stamon_strerror(status));
				ret = WBDE_STAMON_ERROR;
				goto end;
			}
			/* Create timer for getting the measured RSSI */
			param = (wbd_slave_map_monitor_arg_t*)wbd_malloc(sizeof(*param), &ret);
			WBD_ASSERT_MSG("Band[%d] Slave["MACF"] Monitor param malloc failed\n",
				slave->band, ETHER_TO_MACF(slave->wbd_ifr.mac));

			param->slave = slave;
			memcpy(param->neighbor_al_mac, query->neighbor_al_mac,
				sizeof(param->neighbor_al_mac));
			param->maclist = maclist;
			param->channel = channel;
			param->rclass = query->opClass;
			ret = wbd_add_timers(info->hdl, param,
				WBD_SEC_MICROSEC(info->max.tm_map_monitor_read),
				wbd_slave_map_monitored_rssi_read_cb, 0);
		} else {
			/* no sta added to stamon list, release memory if allocated */
			if (maclist) {
				free(maclist);
				maclist = NULL;
			}
		}
		if (stamonlist) {
			free(stamonlist);
			stamonlist = NULL;
		}
	} /* iterate next element list */

	return WBDE_OK;
end:
	if (maclist) {
		free(maclist);
		maclist = NULL;
	}
	if (stamonlist) {
		free(stamonlist);
		stamonlist = NULL;
	}
	WBD_EXIT();
	return ret;
}

/* trigger timer to send action frame to client */
static int
wbd_slave_send_action_frame_to_sta(wbd_info_t* info, wbd_slave_item_t *slave,
	i5_dm_bss_type *i5_bss, struct ether_addr* mac_addr)
{
	int ret = WBDE_OK;
	wbd_actframe_arg_t* actframe_arg = NULL;

	if (!info || !slave) {
		ret = WBDE_INV_ARG;
		return ret;
	}
	if (!mac_addr) {
		ret = WBDE_NULL_MAC;
		return ret;
	}
	actframe_arg = (wbd_actframe_arg_t*)wbd_malloc(sizeof(*actframe_arg), &ret);
	WBD_ASSERT();

	actframe_arg->slave = slave;
	actframe_arg->i5_bss = i5_bss;
	memcpy(&(actframe_arg->sta_mac), mac_addr, sizeof(actframe_arg->sta_mac));

	if (actframe_arg && info->max.offchan_af_interval > 0) {
		actframe_arg->af_count = info->max.offchan_af_period /
			info->max.offchan_af_interval;
		ret = wbd_add_timers(info->hdl, actframe_arg,
			WBD_MSEC_USEC(info->max.offchan_af_interval),
			wbd_slave_actframe_timer_cb, 1);

	}
end:
	return ret;
}

/* Send beacon metrics request to a STA */
int wbd_slave_process_beacon_metrics_query_cb(wbd_info_t *info, char *ifname, unsigned char *bssid,
	ieee1905_beacon_request *query)
{
	int ret = WBDE_OK;
	bcnreq_t *bcnreq;
	uint8 len = 0, idx, k = 0;
	uint8 subelement[WBD_MAX_BUF_256];
	int token = 0;
	wbd_beacon_reports_t *report = NULL;
	wbd_slave_map_beacon_report_arg_t *param = NULL;
	int timeout = 0;
	blanket_bcnreq_t bcnreq_extn;
	WBD_ENTER();

	memset(&bcnreq_extn, 0, sizeof(bcnreq_extn));
	bcnreq = &bcnreq_extn.bcnreq;
	/* Initialize Beacon Request Frame data */
	bcnreq->bcn_mode = DOT11_RMREQ_BCN_ACTIVE;
	bcnreq->dur = 10;
	bcnreq->channel = query->channel;
	eacopy(query->sta_mac, &bcnreq->da);
	bcnreq->random_int = 0x0000;
	if (query->ssid.SSID_len > 0) {
		memcpy(bcnreq->ssid.SSID, query->ssid.SSID, query->ssid.SSID_len);
		bcnreq->ssid.SSID_len = query->ssid.SSID_len;
	}
	bcnreq->reps = 0x0000;
	eacopy(bssid, &bcnreq_extn.src_bssid);
	eacopy(&query->bssid, &bcnreq_extn.target_bssid);
	bcnreq_extn.opclass = query->opclass;

	/* Prepare subelement TLVs */
	/* Include reporting detail */
	subelement[len] = 2;
	subelement[len + 1] = 1;
	subelement[len + TLV_HDR_LEN] = query->reporting_detail;
	len += 1 + TLV_HDR_LEN;

	/* Include AP Channel Report */
	if (query->ap_chan_report) {
		for (idx = 0; idx < query->ap_chan_report_count; idx++) {
			subelement[len] = 51;
			subelement[len + 1] = query->ap_chan_report[k]; k++;
			memcpy(&subelement[len + TLV_HDR_LEN], &query->ap_chan_report[k],
				subelement[len + 1]);
			k += subelement[len + 1];
			len += subelement[len + 1] + TLV_HDR_LEN;
		}
	}

	/* Include element IDs */
	if (query->element_list) {
		subelement[len] = 10;
		subelement[len + 1] = query->element_ids_count;
		memcpy(&subelement[len + TLV_HDR_LEN], query->element_list, subelement[len + 1]);
		len += subelement[len + 1] + TLV_HDR_LEN;
	}

	ret = blanket_send_beacon_request(ifname, &bcnreq_extn, subelement, len, &token);
	if (ret != WBDE_OK) {
		WBD_WARNING("Ifname[%s] STA["MACDBG"] Beacon request failed\n",
			ifname, MAC2STRDBG(query->sta_mac));
		goto end;
	}
	WBD_INFO("Ifname[%s] STA["MACDBG"] \n", ifname, MAC2STRDBG(query->sta_mac));
	report = wbd_ds_add_item_to_beacon_reports(info,
		(struct ether_addr*)&query->neighbor_al_mac, 0,
		(struct ether_addr*)&query->sta_mac);
	if (!report) {
		WBD_WARNING("Ifname[%s] STA["MACDBG"] Beacon request add failed\n",
			ifname, MAC2STRDBG(query->sta_mac));
		goto end;
	}

	/* Create timer for checking if the STA sent beacon report or not */
	param = (wbd_slave_map_beacon_report_arg_t*)wbd_malloc(sizeof(*param), &ret);
	WBD_ASSERT_MSG("Ifname[%s] STA["MACDBG"] Beacon report param malloc failed\n",
		ifname, MAC2STRDBG(query->sta_mac));

	param->info = info;
	memcpy(&param->sta_mac, &query->sta_mac, sizeof(param->sta_mac));

	/* Timeout for beacon report sending will be once we exhaust sending all
	 * beacon requests + beacon request duration of each request +
	 * beacon response wait timeout
	*/
	if (query->ap_chan_report_count) {
		timeout = (query->ap_chan_report_count - 1) *
			(WBD_MIN_BCN_REQ_DELAY + bcnreq->dur) +
			info->max.tm_map_send_bcn_report;
	} else {
		timeout = info->max.tm_map_send_bcn_report;
	}

	ret = wbd_add_timers(info->hdl, param, WBD_MSEC_USEC(timeout),
		wbd_slave_map_send_beacon_report_cb, 0);
	if (ret != WBDE_OK) {
		WBD_ERROR("Ifname[%s] STA["MACDBG"] Token[%d] Interval[%d] Failed to create "
			"beacon report timer\n", ifname, MAC2STRDBG(query->sta_mac), token,
			info->max.tm_map_send_bcn_report);
		goto end;
	}
end:
	if (ret != WBDE_OK) {
		if (param) {
			free(param);
		}
		wbd_ds_remove_beacon_report(info, (struct ether_addr*)&query->sta_mac);
		wbd_slave_map_send_beacon_report_error(info,
			IEEE1905_BEACON_REPORT_RESP_FLAG_UNSPECIFIED,
			query->neighbor_al_mac, query->sta_mac);
	}
	WBD_EXIT();
	return ret;
}

/* Send beacon metrics request for each channel to a STA */
int wbd_slave_process_per_chan_beacon_metrics_query_cb(wbd_info_t *info, char *ifname,
	unsigned char *bssid, ieee1905_beacon_request *query)
{
	int ret = WBDE_OK;
	bcnreq_t *bcnreq;
	uint8 len = 0, idx, k = 0;
	uint8 subelement[WBD_MAX_BUF_64];
	uint8 chan[WBD_MAX_BUF_64];
	uint8 chan_count = 0, tmp_chan_count = 0;
	wbd_beacon_reports_t *report = NULL;
	wbd_slave_map_beacon_report_arg_t *param = NULL;
	wbd_bcn_req_arg_t *bcn_arg = NULL;
	time_t now = 0;
	int timeout = 0, diff = 0;
	blanket_bcnreq_t bcnreq_extn;

	WBD_ENTER();

	/* Check whether we already have reports recieved within WBD_MIN_BCN_METRIC_QUERY_DELAY */
	report = wbd_ds_find_item_fm_beacon_reports(info,
		(struct ether_addr*)&(query->sta_mac), &ret);

	if (report) {
		now = time(NULL);
		diff = now - report->timestamp;
		if (diff > WBD_MIN_BCN_METRIC_QUERY_DELAY) {
			/* Reports are stale free the earlier reports */
			WBD_INFO("Reports are old send beacon request to sta["MACDBG"]"
				"now[%lu]  timestamp[%lu] diff[%d]\n",
				MAC2STRDBG(query->sta_mac), now, report->timestamp, diff);

			wbd_ds_remove_beacon_report(info, (struct ether_addr*)&(query->sta_mac));
		} else {
			/* Already present reports can be sent */
			WBD_INFO("Reports are available for sta["MACDBG"]"
				"now[%lu]  timestamp[%lu] diff[%d]\n",
				MAC2STRDBG(query->sta_mac), now, report->timestamp, diff);

			goto sendreport;
		}
	} else {
		WBD_INFO("No Previous Reports are available for sta["MACDBG"] \n",
			MAC2STRDBG(query->sta_mac));
		ret = WBDE_OK;
	}

	memset(&bcnreq_extn, 0, sizeof(bcnreq_extn));
	bcnreq = &bcnreq_extn.bcnreq;
	/* Initialize Beacon Request Frame data */
	bcnreq->bcn_mode = DOT11_RMREQ_BCN_ACTIVE;
	bcnreq->dur = 10; /* Duration of the measurement in terms of TU's */
	bcnreq->channel = query->channel;
	eacopy(query->sta_mac, &bcnreq->da);
	bcnreq->random_int = 0x0000;
	if (query->ssid.SSID_len > 0) {
		memcpy(bcnreq->ssid.SSID, query->ssid.SSID, query->ssid.SSID_len);
		bcnreq->ssid.SSID_len = query->ssid.SSID_len;
	}
	bcnreq->reps = 0x0000;
	eacopy(bssid, &bcnreq_extn.src_bssid);
	eacopy(&query->bssid, &bcnreq_extn.target_bssid);
	bcnreq_extn.opclass = query->opclass;

	/* Prepare subelement TLVs */
	/* Include reporting detail */
	subelement[len] = 2;
	subelement[len + 1] = 1;
	subelement[len + TLV_HDR_LEN] = query->reporting_detail;
	len += 1 + TLV_HDR_LEN;

	/* Include element IDs */
	if (query->element_list) {
		subelement[len] = 10;
		subelement[len + 1] = query->element_ids_count;
		memcpy(&subelement[len + TLV_HDR_LEN], query->element_list, subelement[len + 1]);
		len += subelement[len + 1] + TLV_HDR_LEN;
	}

	bcn_arg = (wbd_bcn_req_arg_t *) wbd_malloc(sizeof(*bcn_arg), &ret);
	WBD_ASSERT_MSG("Ifname[%s] STA["MACDBG"] Beacon report bcn_arg malloc failed\n",
		ifname, MAC2STRDBG(query->sta_mac));

	memset(bcn_arg, 0, sizeof(*bcn_arg));

	bcn_arg->info = info;
	strncpy(bcn_arg->ifname, ifname, IFNAMSIZ);
	eacopy(query->neighbor_al_mac, bcn_arg->neighbor_al_mac);
	bcn_arg->bcnreq_extn = bcnreq_extn;
	memcpy(bcn_arg->subelement, subelement, len);
	bcn_arg->subelem_len = len;

	/* Include AP Channel Report */
	if (query->ap_chan_report) {
		for (idx = 0; idx < query->ap_chan_report_count; idx++) {
			/* Here first byte of ap_chan_report is total length of channels
			 * corresponding to single rclass and second byte is rclass.
			*/
			tmp_chan_count = query->ap_chan_report[k] - 1;
			k += 2;
			memcpy(&chan[chan_count], &query->ap_chan_report[k], tmp_chan_count);
			k += tmp_chan_count;
			chan_count += tmp_chan_count;
		}

		if (chan_count) {
			bcn_arg->chan = (uint8 *) wbd_malloc(chan_count, &ret);
			WBD_ASSERT_MSG("Ifname[%s] STA["MACDBG"] Beacon report malloc failed\n",
				ifname, MAC2STRDBG(query->sta_mac));

			memset(bcn_arg->chan, 0, chan_count);
			memcpy(bcn_arg->chan, chan, chan_count);
			bcn_arg->chan_count = chan_count;
		} else {
			bcn_arg->chan = NULL;
			bcn_arg->chan_count = 0;
		}
	}

	ret = wbd_add_timers(info->hdl, bcn_arg, 0,
			wbd_slave_bcn_req_timer_cb, 0);

	if (ret != WBDE_OK) {
		WBD_ERROR("Ifname[%s] STA["MACDBG"] Failed to create timer\n",
			ifname, MAC2STRDBG(query->sta_mac));
	}

	report = wbd_ds_add_item_to_beacon_reports(bcn_arg->info,
		(struct ether_addr*)&query->neighbor_al_mac, 0,
		(struct ether_addr*)&query->sta_mac);
	if (!report) {
		WBD_WARNING("Ifname[%s] STA["MACDBG"] Beacon request add failed\n",
			ifname, MAC2STRDBG(query->sta_mac));
		goto end;
	}

	/* Timeout for beacon report sending will be once we exhaust sending all
	 * beacon requests + beacon request duration of each request +
	 * beacon response wait timeout
	*/
	if (chan_count) {
		timeout = (chan_count - 1) * (WBD_MIN_BCN_REQ_DELAY + bcnreq->dur) +
			info->max.tm_map_send_bcn_report;
	} else {
		timeout = info->max.tm_map_send_bcn_report;
	}

sendreport:
	/* Create timer for checking if the STA sent beacon report or not */
	param = (wbd_slave_map_beacon_report_arg_t*)wbd_malloc(sizeof(*param), &ret);
	WBD_ASSERT_MSG("Ifname[%s] STA["MACDBG"] Beacon report param malloc failed\n",
		ifname, MAC2STRDBG(query->sta_mac));

	param->info = info;
	memcpy(&param->sta_mac, &query->sta_mac, sizeof(param->sta_mac));

	ret = wbd_add_timers(info->hdl, param,
		WBD_MSEC_USEC(timeout),
		wbd_slave_map_send_beacon_report_cb, 0);
	if (ret != WBDE_OK) {
		WBD_WARNING("Ifname[%s] STA["MACDBG"] Interval[%d] Failed to create "
			"beacon report timer\n", ifname, MAC2STRDBG(query->sta_mac),
			timeout);
		goto end;
	}

end:
	if (ret != WBDE_OK) {
		if (param) {
			free(param);
		}
		wbd_remove_timers(info->hdl, wbd_slave_bcn_req_timer_cb, bcn_arg);
		wbd_ds_remove_beacon_report(info, (struct ether_addr*)query->sta_mac);
		wbd_slave_map_send_beacon_report_error(info,
			IEEE1905_BEACON_REPORT_RESP_FLAG_UNSPECIFIED, query->neighbor_al_mac,
			query->sta_mac);

		if (bcn_arg && bcn_arg->chan) {
			free(bcn_arg->chan);
		}

		if (bcn_arg) {
			free(bcn_arg);
		}
	}

	WBD_EXIT();
	return ret;
}

/* Store beacon report */
static int
wbd_slave_store_beacon_report(wbd_slave_item_t *slave, uint8 *data, struct ether_addr *sta_mac)
{
	int ret = WBDE_OK;
	wl_rrm_event_t *event;
	dot11_rm_ie_t *ie;
	wbd_beacon_reports_t *report;
	uint8 *tmpbuf = NULL;
	uint16 len = 0;
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(slave, WBDE_INV_ARG);
	WBD_ASSERT_ARG(data, WBDE_INV_ARG);

	/* Validate NULL MAC */
	WBD_ASSERT_NULLMAC(&slave->wbd_ifr.bssid, "RRM_EVENT");

	/* get subtype of the event */
	event = (wl_rrm_event_t *)data;
	WBD_INFO("Band[%d] Slave["MACF"] version:0x%02x len:0x%02x cat:0x%02x "
		"subevent:0x%02x\n", slave->band, ETHER_TO_MACF(slave->wbd_ifr.mac),
		event->version, event->len, event->cat, event->subevent);

	/* copy event data based on subtype */
	if (event->subevent != DOT11_MEASURE_TYPE_BEACON) {
		WBD_ERROR("Band[%d] Slave["MACF"] Unhandled event subtype: 0x%2X\n",
			slave->band, ETHER_TO_MACF(slave->wbd_ifr.mac), event->subevent);
		goto end;
	}

	ie = (dot11_rm_ie_t *)(event->payload);
	dot11_rmrep_bcn_t *rmrep_bcn;
	rmrep_bcn = (dot11_rmrep_bcn_t *)&ie[1];
	WBD_INFO("Band[%d] Slave["MACF"] Token[%d] BEACON EVENT, regclass: %d, channel: %d, "
		"rcpi: %d, bssid["MACF"]\n", slave->band, ETHER_TO_MACF(slave->wbd_ifr.mac),
		ie->token, rmrep_bcn->reg, rmrep_bcn->channel, rmrep_bcn->rcpi,
		ETHER_TO_MACF(rmrep_bcn->bssid));
	/* Traverse through each beaocn report */
	if ((report = wbd_ds_find_item_fm_beacon_reports(slave->parent->parent,
		sta_mac, &ret)) == NULL) {
		WBD_INFO("Token[%d] For STA["MACF"] not found\n",
			ie->token, ETHERP_TO_MACF(sta_mac));
		goto end;
	}

	/* minimum length field is 3 which inclued toke, mode and type */
	if (ie->len <= 3) {
		goto end;
	}

	/* Total length will be ie len plus 2(for element ID and length field) */
	len = report->report_element_len + ie->len + 2;
	/* total len should be less than ethernet frame len minus 11(which is minimum length of the
	 * beacon metrics response TLV
	 */
	if (len > ETH_FRAME_LEN - 11) {
		WBD_INFO("Token[%d] For STA["MACF"] Total length of the beacon report[%d] "
			"exceeding[%d]\n",
			ie->token, ETHERP_TO_MACF(sta_mac), len, ETH_FRAME_LEN);
		goto end;
	}

	tmpbuf = (uint8*)wbd_malloc(len, &ret);
	WBD_ASSERT_MSG("Band[%d] Slave["MACF"] STA["MACF"] Beacon report element malloc failed\n",
		slave->band, ETHER_TO_MACF(slave->wbd_ifr.mac), ETHERP_TO_MACF(sta_mac));
	if (report->report_element) {
		memcpy(tmpbuf, report->report_element, report->report_element_len);
		free(report->report_element);
		report->report_element = NULL;
	}
	memcpy(tmpbuf + report->report_element_len, ie, ie->len + 2);
	report->report_element = tmpbuf;
	report->report_element_count++;
	report->report_element_len = len;
	report->timestamp = time(NULL);
	WBD_INFO("Band[%d] Slave["MACF"] timestamp[%lu] IE Len[%d] Total Len[%d] Count[%d]\n",
		slave->band, ETHER_TO_MACF(slave->wbd_ifr.mac), report->timestamp, ie->len,
		report->report_element_len, report->report_element_count);
end:
	WBD_EXIT();
	return ret;
}

/* Process channel utilization threshold check */
static void
wbd_slave_chan_util_check_timer_cb(bcm_usched_handle *hdl, void *arg)
{
	int ret = WBDE_OK;
	unsigned char chan_util;
	unsigned char *mac = (unsigned char*)arg;
	i5_dm_device_type *i5_device_cntrl;
	i5_dm_interface_type *i5_ifr = NULL;
	wbd_ifr_item_t *ifr_vndr_data;
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(arg, WBDE_INV_ARG);

	/* Find Interface in Self Device, matching to Radio MAC */
	if ((i5_ifr = wbd_ds_get_self_i5_interface(mac, &ret)) == NULL) {
		WBD_INFO("For["MACDBG"]. %s\n", MAC2STRDBG(mac), wbderrorstr(ret));
		goto remove_timer;
	}
	ifr_vndr_data = (wbd_ifr_item_t*)i5_ifr->vndr_data;
	/* if there is no vendor data or chan util thld is 0, remove the timer */
	if (!ifr_vndr_data || ifr_vndr_data->chan_util_thld == 0) {
		goto remove_timer;
	}

	/* Get channel utilization */
	wbd_slave_get_chan_util(i5_ifr->ifname, &chan_util);
	WBD_DEBUG("Ifname[%s] chan_util[%d] thld[%d] reported[%d]\n", i5_ifr->ifname,
		chan_util, ifr_vndr_data->chan_util_thld, ifr_vndr_data->chan_util_reported);

	/* Send AP metrics if the channel utilization is crossed threshold and we haven't reported
	 * when it crossed previously or channel utilization is less than threshold and we haven't
	 * reported previously when it came down.
	 */
	if (((chan_util > ifr_vndr_data->chan_util_thld) &&
		(ifr_vndr_data->chan_util_reported < ifr_vndr_data->chan_util_thld)) ||
		((chan_util < ifr_vndr_data->chan_util_thld) &&
		(ifr_vndr_data->chan_util_reported > ifr_vndr_data->chan_util_thld))) {
		/* Send AP Metrics */
		i5_device_cntrl = i5DmFindController();
		if (i5_device_cntrl) {
			ieee1905_send_ap_metrics_response(i5_device_cntrl->DeviceId, mac);
			ifr_vndr_data->chan_util_reported = chan_util;
			WBD_DEBUG("Ifname[%s] chan_util[%d] thld[%d] reported[%d] Sent AP Metrics "
				"Response.\n", i5_ifr->ifname, chan_util,
				ifr_vndr_data->chan_util_thld, ifr_vndr_data->chan_util_reported);
		}
	}

	goto end;

remove_timer:
	wbd_remove_timers(hdl, wbd_slave_chan_util_check_timer_cb, arg);
	free(arg);

end:
	WBD_EXIT();
	return;
}

/* process channel utilization policy */
static void
wbd_slave_process_chan_util_policy(i5_dm_interface_type *i5_ifr, ieee1905_ifr_metricrpt *metricrpt)
{
	int ret = WBDE_OK;
	wbd_ifr_item_t *ifr_vndr_data;
	unsigned char *mac;

	WBD_ASSERT_ARG(i5_ifr->vndr_data, WBDE_INV_ARG);
	ifr_vndr_data = (wbd_ifr_item_t*)i5_ifr->vndr_data;

	WBD_DEBUG("IFR["MACDBG"] ap_mtrc_chan_util[%d] ifr_vndr_data->chan_util_thld[%d]\n",
		MAC2STRDBG(i5_ifr->InterfaceId), metricrpt->ap_mtrc_chan_util,
		ifr_vndr_data->chan_util_thld);
	/* Check for AP Metrics Channel Utilization Reporting Threshold. AP Metrics
	 * Channel Utilization Reporting Threshold is greate than 0, send ap metrics
	 * report when it crosses the threshold
	 */
	if (metricrpt->ap_mtrc_chan_util > 0) {
		/* If it is already there, no need to process */
		if (ifr_vndr_data->chan_util_thld > 0) {
			goto end;
		}

		ifr_vndr_data->chan_util_thld = metricrpt->ap_mtrc_chan_util;
		ifr_vndr_data->chan_util_reported = 0;

		mac = (unsigned char*)wbd_malloc(MAC_ADDR_LEN, &ret);
		WBD_ASSERT_MSG("IFR["MACDBG"] Failed to alloc chan_util_check arg\n",
			MAC2STRDBG(i5_ifr->InterfaceId));

		memcpy(mac, i5_ifr->InterfaceId, MAC_ADDR_LEN);
		/* Create a timer to send REMOVE_CLIENT_REQ cmd to selected BSS */
		ret = wbd_add_timers(wbd_get_ginfo()->hdl, (void*)mac,
			WBD_SEC_MICROSEC(wbd_get_ginfo()->max.tm_wd_weakclient),
			wbd_slave_chan_util_check_timer_cb, 1);
		if (ret != WBDE_OK) {
			WBD_WARNING("Interval[%d] Failed to create chan_util_check timer\n",
				wbd_get_ginfo()->max.tm_wd_weakclient);
			free(mac);
		}

		/* 1st time process immediately */
		wbd_slave_chan_util_check_timer_cb(wbd_get_ginfo()->hdl, (void*)mac);
	} else {
		/* make threshold to 0. When the timer gets called, it will remove the timer */
		ifr_vndr_data->chan_util_thld = 0;
	}

end:
	return;
}

/* Recieved Multi-AP Metric report policy configuration */
static void
wbd_slave_metric_report_policy_rcvd(wbd_info_t *info, ieee1905_metricrpt_config *metric_policy,
	wbd_cmd_vndr_metric_policy_config_t *vndr_policy)
{
	int ret = WBDE_OK, t_rssi = 0;
	uint32 rc_restart = 0;
	char new_val[WBD_MAX_BUF_128], prefix[IFNAMSIZ];
	dll_t *item_p;
	ieee1905_ifr_metricrpt *metricrpt = NULL;
	i5_dm_interface_type *i5_ifr = NULL;
	wbd_metric_policy_ifr_entry_t *vndr_pol_data = NULL;
	wbd_vndr_metric_rpt_policy_t def_vndr_pol, *ptr_vndr_pol = NULL;
	WBD_ENTER();

	def_vndr_pol.t_idle_rate = WBD_STA_METRICS_REPORTING_IDLE_RATE_THLD;
	def_vndr_pol.t_tx_rate = WBD_STA_METRICS_REPORTING_TX_RATE_THLD;
	def_vndr_pol.t_tx_failures = WBD_STA_METRICS_REPORTING_TX_FAIL_THLD;
	def_vndr_pol.flags = WBD_WEAK_STA_POLICY_FLAG_RSSI;

	/* Travese Metric Policy Config List items */
	foreach_glist_item(item_p, metric_policy->ifr_list) {

		metricrpt = (ieee1905_ifr_metricrpt*)item_p;
		vndr_pol_data = NULL;

		WBD_INFO("Metric Report Policy For["MACDBG"]\n", MAC2STRDBG(metricrpt->mac));

		/* Find Interface in Self Device, matching to Radio MAC, of this Metric Pol Item */
		if ((i5_ifr = wbd_ds_get_self_i5_interface(metricrpt->mac, &ret)) == NULL) {
			WBD_INFO("Metric Report Policy For["MACDBG"]. %s\n",
				MAC2STRDBG(metricrpt->mac), wbderrorstr(ret));
			continue;
		}

		wbd_slave_process_chan_util_policy(i5_ifr, metricrpt);

		/* Convert RCPI to RSSI */
		t_rssi = WBD_RCPI_TO_RSSI(metricrpt->sta_mtrc_rssi_thld);

		/* Find matching Vendor Metric Policy for this Radio MAC */
		vndr_pol_data = wbd_find_vndr_metric_policy_for_radio(metricrpt->mac, vndr_policy);

		WBD_INFO("Find Vendor Metric Report Policy For Radio["MACDBG"] : %s. %s\n",
			MAC2STRDBG(metricrpt->mac), vndr_pol_data ? "Success" : "Failure",
			vndr_pol_data ? "" : "Using Default Vendor Policy.");

		/* Get Vendor Metric Policy pointer */
		ptr_vndr_pol = vndr_pol_data ? (&(vndr_pol_data->vndr_policy)) : (&def_vndr_pol);

		/* Get Prefix for this Radio MAC */
		snprintf(prefix, sizeof(prefix), "%s_", i5_ifr->wlParentName);

		/* Prepare New Value of "wbd_weak_sta_cfg" from Policy Received from Controller */
		snprintf(new_val, sizeof(new_val), "%d %d %d %d %d 0x%x",
			ptr_vndr_pol->t_idle_rate, t_rssi, metricrpt->sta_mtrc_rssi_hyst,
			ptr_vndr_pol->t_tx_rate, ptr_vndr_pol->t_tx_failures, ptr_vndr_pol->flags);

		/* Match NVRAM and NEW value, and if mismatch, Set new value in NVRAM */
		rc_restart |= blanket_nvram_prefix_match_set(prefix, WBD_NVRAM_WEAK_STA_CFG,
			new_val, FALSE);

		WBD_INFO("MAC["MACDBG"] IDLE_RATE[%d] RCPI[%d] RSSI[%d] Hysterisis[%d] "
			"PHY_RATE[%d] TX_FAILURES[%d] FLAGS[0x%X] "
			"NVRAM[%s%s=%s]\n", MAC2STRDBG(metricrpt->mac), ptr_vndr_pol->t_idle_rate,
			metricrpt->sta_mtrc_rssi_thld, t_rssi, metricrpt->sta_mtrc_rssi_hyst,
			ptr_vndr_pol->t_tx_rate, ptr_vndr_pol->t_tx_failures,
			ptr_vndr_pol->flags, prefix, WBD_NVRAM_WEAK_STA_CFG, new_val);
	}

	WBD_INFO("Weak STA Config %s Changed.%s\n", rc_restart ? "" : "NOT",
		rc_restart ? "RC Restart...!!" : "");

	/* If required, Execute nvram commit/rc restart/reboot commands */
	if (rc_restart) {
		WBD_INFO("Creating rc restart timer\n");
		wbd_slave_create_rc_restart_timer(info);
		info->flags |= WBD_INFO_FLAGS_RC_RESTART;
	}

	WBD_EXIT();
}

/* recieved Multi-AP Policy Configuration */
void
wbd_slave_policy_configuration_cb(wbd_info_t *info, ieee1905_policy_config *policy,
	unsigned short rcvd_policies, ieee1905_vendor_data *in_vndr_tlv)
{
	int ret = WBDE_OK;
	wbd_cmd_vndr_metric_policy_config_t vndr_policy; /* Metric Policy Vendor Data */
	WBD_ENTER();

	/* If metric report policy configuration recieved */
	if (rcvd_policies & MAP_POLICY_TYPE_FLAG_METRIC_REPORT) {

		/* Initialize Metric Policy Config Data */
		memset(&vndr_policy, 0, sizeof(vndr_policy));
		wbd_ds_glist_init(&(vndr_policy.entry_list));

		if (in_vndr_tlv && in_vndr_tlv->vendorSpec_msg) {

			/* Decode Vendor Specific TLV : Metric Policy Vendor Data on receive */
			ret = wbd_tlv_decode_vndr_metric_policy((void *)&vndr_policy,
				in_vndr_tlv->vendorSpec_msg, in_vndr_tlv->vendorSpec_len);
			WBD_ASSERT_MSG("Failed to decode Metrics Policy\n");
		}

		/* Process Recieved Multi-AP Metric Report Policy Configuration */
		wbd_slave_metric_report_policy_rcvd(info, &policy->metricrpt_config, &vndr_policy);

		/* If VNDR_METRIC_POLICY List is filled up */
		if (vndr_policy.num_entries > 0) {

			/* Remove all VNDR_METRIC_POLICY data items */
			wbd_ds_glist_cleanup(&(vndr_policy.entry_list));
		}

	}

#if defined(MULTIAPR2)
	/* If traffic separation policy configuration recieved */
	if (rcvd_policies &
		(MAP_POLICY_TYPE_FLAG_TS_8021QSET | MAP_POLICY_TYPE_FLAG_TS_POLICY)) {
		if (wbd_slave_process_traffic_separation_policy(policy, rcvd_policies) == 0) {
			WBD_INFO("Creating rc restart timer\n");
			info->flags |= WBD_INFO_FLAGS_RC_RESTART;
			wbd_slave_create_rc_restart_timer(info);
		}
	}
	/* If Unsuccessful Association report policy configuration recieved */
	if (rcvd_policies & MAP_POLICY_TYPE_FLAG_UNSUCCESSFUL_ASSOCIATION) {
		memcpy(&(info->wbd_slave->unsuccessful_assoc_config),
			&(policy->unsuccessful_assoc_config),
			sizeof(info->wbd_slave->unsuccessful_assoc_config));

		if (info->wbd_slave->unsuccessful_assoc_config.report_flag &
				MAP_UNSUCCESSFUL_ASSOC_FLAG_REPORT) {
			ret = wbd_add_timers(info->hdl,
				&info->wbd_slave->unsuccessful_assoc_config,
				WBD_SEC_MICROSEC(IEEE1905_RESET_UNSUCCESSFUL_ASSOC_COUNT_TIEMOUT),
				wbd_slave_reset_unsuccessful_assoc_count_timer_cb, 1);
			if (ret != WBDE_OK) {
				WBD_WARNING("Failed to create reset unsuccessful association "
					"count timer for Interval[%d]\n",
					IEEE1905_RESET_UNSUCCESSFUL_ASSOC_COUNT_TIEMOUT);
			}
		} else {
			wbd_remove_timers(info->hdl,
				wbd_slave_reset_unsuccessful_assoc_count_timer_cb,
				&info->wbd_slave->unsuccessful_assoc_config);
		}
	}
#endif /* MULTIAPR2 */

end:
	WBD_EXIT();
}

/* Callback for exception from communication handler for slave server */
static void
wbd_slave_com_server_exception(wbd_com_handle *hndl, void *arg, WBD_COM_STATUS status)
{
	WBD_ERROR("Exception from slave server\n");
}

/* Callback for exception from communication handler for CLI */
static void
wbd_slave_com_cli_exception(wbd_com_handle *hndl, void *arg, WBD_COM_STATUS status)
{
	WBD_ERROR("Exception from CLI server\n");
}

/* Callback for exception from communication handler for EVENTD */
/* static void
wbd_slave_com_eventd_exception(wbd_com_handle *hndl, void *arg, WBD_COM_STATUS status)
{
	WBD_ERROR("Exception from EVENTD server\n");
}
*/

/* Register all the commands for master server to communication handle */
static int
wbd_slave_register_server_command(wbd_info_t *info)
{
	/* Commands from BSD */
	wbd_com_register_cmd(info->com_serv_hdl, WBD_CMD_WEAK_CLIENT_BSD_REQ,
		wbd_slave_process_weak_client_bsd_cmd, info, wbd_json_parse_weak_client_bsd_cmd);

	wbd_com_register_cmd(info->com_serv_hdl, WBD_CMD_WEAK_CANCEL_BSD_REQ,
		wbd_slave_process_weak_cancel_bsd_cmd, info, wbd_json_parse_weak_cancel_bsd_cmd);

	wbd_com_register_cmd(info->com_serv_hdl, WBD_CMD_STA_STATUS_BSD_REQ,
		wbd_slave_process_sta_status_bsd_cmd, info, wbd_json_parse_sta_status_bsd_cmd);

	wbd_com_register_cmd(info->com_serv_hdl, WBD_CMD_BLOCK_CLIENT_BSD_REQ,
		wbd_slave_process_blk_client_bsd_cmd, info, wbd_json_parse_blk_client_bsd_cmd);

	/* Now register CLI commands */
	wbd_com_register_cmd(info->com_cli_hdl, WBD_CMD_CLI_VERSION,
		wbd_process_version_cli_cmd, info, wbd_json_parse_cli_cmd);

	wbd_com_register_cmd(info->com_cli_hdl, WBD_CMD_CLI_SLAVELIST,
		wbd_slave_process_slavelist_cli_cmd, info, wbd_json_parse_cli_cmd);

	wbd_com_register_cmd(info->com_cli_hdl, WBD_CMD_CLI_INFO,
		wbd_slave_process_info_cli_cmd, info, wbd_json_parse_cli_cmd);

	wbd_com_register_cmd(info->com_cli_hdl, WBD_CMD_CLI_CLIENTLIST,
		wbd_slave_process_clientlist_cli_cmd, info, wbd_json_parse_cli_cmd);

	wbd_com_register_cmd(info->com_cli_hdl, WBD_CMD_CLI_MONITORLIST,
		wbd_slave_process_monitorlist_cli_cmd, info, wbd_json_parse_cli_cmd);

	wbd_com_register_cmd(info->com_cli_hdl, WBD_CMD_CLI_MONITORADD,
		wbd_slave_process_monitoradd_cli_cmd, info, wbd_json_parse_cli_cmd);

	wbd_com_register_cmd(info->com_cli_hdl, WBD_CMD_CLI_MONITORDEL,
		wbd_slave_process_monitordel_cli_cmd, info, wbd_json_parse_cli_cmd);

	wbd_com_register_cmd(info->com_cli_hdl, WBD_CMD_CLI_WEAK_CLIENT,
		wbd_slave_process_weak_client_cli_cmd, info, wbd_json_parse_cli_cmd);

	wbd_com_register_cmd(info->com_cli_hdl, WBD_CMD_CLI_WEAK_CANCEL,
		wbd_slave_process_weak_cancel_cli_cmd, info, wbd_json_parse_cli_cmd);

	wbd_com_register_cmd(info->com_cli_hdl, WBD_CMD_CLI_MSGLEVEL,
		wbd_process_set_msglevel_cli_cmd, info, wbd_json_parse_cli_cmd);

	/* Now register EVENTD commands */

	return WBDE_OK;
}

/* Initialize the communication module for slave */
int
wbd_init_slave_com_handle(wbd_info_t *info)
{
	/* Initialize communication module for server FD */
	info->com_serv_hdl = wbd_com_init(info->hdl, info->server_fd, 0x0000,
		wbd_json_parse_cmd_name, wbd_slave_com_server_exception, info);
	if (!info->com_serv_hdl) {
		WBD_ERROR("Failed to initialize the communication module for slave server\n");
		return WBDE_COM_ERROR;
	}

	/* Initialize communication module for CLI */
	info->com_cli_hdl = wbd_com_init(info->hdl, info->cli_server_fd, 0x0000,
		wbd_json_parse_cli_cmd_name, wbd_slave_com_cli_exception, info);
	if (!info->com_cli_hdl) {
		WBD_ERROR("Failed to initialize the communication module for CLI server\n");
		return WBDE_COM_ERROR;
	}

	/* Initialize communication module for EVENTFD */
	/* info->com_eventd_hdl = wbd_com_init(info->hdl, info->event_fd, WBD_COM_SERV_TYPE_BIN,
		wbd_json_parse_cmd_name, wbd_slave_com_eventd_exception, info);
	if (!info->com_cli_hdl) {
		WBD_ERROR("Failed to initialize the communication module for EVENTD server\n");
		return WBDE_COM_ERROR;
	}
	*/

	wbd_slave_register_server_command(info);

	return WBDE_OK;
}

/* Utility to loop in all bss of self device
 * and add rrm static neighbor list
*/
static void
wbd_slave_add_nbr(i5_dm_bss_type *nbss, chanspec_t nchanspec,
	i5_dm_device_type *sdev, bool noselfbss)
{
	i5_dm_interface_type *sifr = NULL;
	i5_dm_bss_type *sbss = NULL;
	blanket_nbr_info_t bkt_nbr;

	WBD_INFO("Adding Neighbor["MACDBG"] chanspec = %x \n",
			MAC2STRDBG(nbss->BSSID), nchanspec);

	memset(&bkt_nbr, 0, sizeof(bkt_nbr));

	eacopy((struct ether_addr *)(nbss->BSSID), &(bkt_nbr.bssid));

	bkt_nbr.channel = wf_chspec_ctlchan(nchanspec);
	foreach_i5glist_item(sifr, i5_dm_interface_type, sdev->interface_list) {

		/* skip if this is not wireless interface */
		if (!i5DmIsInterfaceWireless(sifr->MediaType) ||
			!sifr->BSSNumberOfEntries) {
			continue;
		}

		foreach_i5glist_item(sbss, i5_dm_bss_type, sifr->bss_list) {
			/* Add Bss in neighbor list only if ssid is same */
			if (memcmp(sbss->ssid.SSID, nbss->ssid.SSID, sbss->ssid.SSID_len)) {
				continue;
			}

			/* skip for own bss */
			if (noselfbss && (memcmp(sbss->BSSID, nbss->BSSID, MAC_ADDR_LEN) == 0)) {
				continue;
			}

			blanket_get_rclass(sbss->ifname, nchanspec, &(bkt_nbr.reg));
			memcpy(&(bkt_nbr.ssid.SSID), &(nbss->ssid.SSID), nbss->ssid.SSID_len);
			bkt_nbr.ssid.SSID_len = strlen((const char*)bkt_nbr.ssid.SSID);
			bkt_nbr.chanspec = nchanspec;
			blanket_add_nbr(sbss->ifname, &bkt_nbr);
		}
	}
}

/* Add AP's from whole blanket in static neighbor list */
void
wbd_slave_add_nbr_from_topology()
{
	int ret = WBDE_OK;
	i5_dm_network_topology_type *topology;
	i5_dm_device_type *dev = NULL, *sdev = NULL;
	i5_dm_interface_type *ifr = NULL;
	i5_dm_bss_type *bss = NULL;
	WBD_ENTER();

	WBD_SAFE_GET_I5_SELF_DEVICE(sdev, &ret);

	/* Get Topology of Agent from 1905 lib */
	topology = (i5_dm_network_topology_type *)ieee1905_get_datamodel();

	/* Loop for all the Devices in Agent Topology */
	foreach_i5glist_item(dev, i5_dm_device_type, topology->device_list) {

		/* skip for self device */
		if (memcmp(sdev->DeviceId, dev->DeviceId, MAC_ADDR_LEN) == 0) {
			continue;
		}

		foreach_i5glist_item(ifr, i5_dm_interface_type, dev->interface_list) {
			if (!i5DmIsInterfaceWireless(ifr->MediaType) ||
				!ifr->BSSNumberOfEntries) {
				continue;
			}

			/* Loop for all the BSSs in Interface */
			foreach_i5glist_item(bss, i5_dm_bss_type, ifr->bss_list) {

				/* add neighbor in self device bss */
				wbd_slave_add_nbr(bss, ifr->chanspec, sdev, FALSE);
			}
		}
	}

end:
	wbd_slave_add_nbr_from_self_dev(sdev);
	WBD_EXIT();
}

/* Add self device AP's in static neighbor list */
void
wbd_slave_add_nbr_from_self_dev(i5_dm_device_type *sdev)
{
	i5_dm_interface_type *ifr = NULL;
	i5_dm_bss_type *bss = NULL;
	WBD_ENTER();

	/* Loop through own device */
	foreach_i5glist_item(ifr, i5_dm_interface_type, sdev->interface_list) {
		if (!i5DmIsInterfaceWireless(ifr->MediaType) ||
			!ifr->BSSNumberOfEntries) {
			continue;
		}

		/* Loop for all the BSSs in Interface */
		foreach_i5glist_item(bss, i5_dm_bss_type, ifr->bss_list) {

			/* add neighbor in self device bss */
			wbd_slave_add_nbr(bss, ifr->chanspec, sdev, TRUE);
		}
	}

	WBD_EXIT();
}

/* Add new AP in static neighbor list */
void
wbd_slave_add_bss_nbr(i5_dm_bss_type *bss)
{
	int ret = WBDE_OK;
	i5_dm_device_type *dev = NULL, *sdev = NULL;
	i5_dm_interface_type *ifr = NULL;
	WBD_ENTER();

	/* Get self device */
	WBD_SAFE_GET_I5_SELF_DEVICE(sdev, &ret);

	/* get interface of bss */
	ifr = (i5_dm_interface_type *)WBD_I5LL_PARENT(bss);

	/* get device of bss */
	dev = (i5_dm_device_type *)WBD_I5LL_PARENT(ifr);

	/* skip for self device */
	if (memcmp(sdev->DeviceId, dev->DeviceId, MAC_ADDR_LEN) == 0) {
		goto end;
	}

	/* add neighbor in self device bss */
	wbd_slave_add_nbr(bss, ifr->chanspec, sdev, FALSE);

end:
	WBD_EXIT();
}

/* Delete an AP from static neighbor list */
void
wbd_slave_del_bss_nbr(i5_dm_bss_type *bss)
{
	int ret = WBDE_OK;
	i5_dm_device_type *dev = NULL, *sdev = NULL;
	i5_dm_interface_type *ifr = NULL, *sifr = NULL;
	i5_dm_bss_type *sbss = NULL;
	WBD_ENTER();

	/* Get self device */
	WBD_SAFE_GET_I5_SELF_DEVICE(sdev, &ret);

	/* get interface of bss */
	ifr = (i5_dm_interface_type *)WBD_I5LL_PARENT(bss);

	/* get device of bss */
	dev = (i5_dm_device_type *)WBD_I5LL_PARENT(ifr);

	/* skip for self device */
	if (memcmp(sdev->DeviceId, dev->DeviceId, MAC_ADDR_LEN) == 0) {
		goto end;
	}

	WBD_INFO("Deleting Neighbor["MACDBG"] \n", MAC2STRDBG(bss->BSSID));

	foreach_i5glist_item(sifr, i5_dm_interface_type, sdev->interface_list) {
		foreach_i5glist_item(sbss, i5_dm_bss_type, sifr->bss_list) {
			blanket_del_nbr(sbss->ifname, (struct ether_addr *)(bss->BSSID));
		}
	}

end:
	WBD_EXIT();
}

/* Add neighbor in static neiohgbor list entry with updated channel */
void
wbd_slave_add_ifr_nbr(i5_dm_interface_type *i5_ifr, bool noselfbss)
{
	int ret = WBDE_OK;
	i5_dm_device_type *sdev = NULL;
	i5_dm_bss_type *bss = NULL;
	WBD_ENTER();

	WBD_SAFE_GET_I5_SELF_DEVICE(sdev, &ret);

	/* Loop for all the BSSs in Interface */
	foreach_i5glist_item(bss, i5_dm_bss_type, i5_ifr->bss_list) {

		/* add neighbor in self device bss */
		wbd_slave_add_nbr(bss, i5_ifr->chanspec, sdev, noselfbss);
	}
end:
	WBD_EXIT();
}

void
wbd_slave_send_opchannel_reports(void)
{
	int ret = WBDE_OK;
	i5_dm_device_type *sdev = NULL;
	i5_dm_interface_type *sifr = NULL;

	WBD_ENTER();

	WBD_SAFE_GET_I5_SELF_DEVICE(sdev, &ret);
	foreach_i5glist_item(sifr, i5_dm_interface_type, sdev->interface_list) {
		if (sifr->msg_stat_flag == I5_MSG_RESP_SENT) {
			chanspec_t chanspec = 0;
			/* Get chanspec from firmware */
			blanket_get_chanspec(sifr->ifname, &chanspec);
			if (chanspec != sifr->chanspec) {
				sifr->chanspec = chanspec;
				ieee1905_notify_channel_change(sifr);
			}
			blanket_get_global_rclass(sifr->chanspec, &(sifr->opClass));
			wbd_slave_send_operating_chan_report(sifr);
			sifr->msg_stat_flag = I5_MSG_OP_CHAN_SENT;
			wbd_slave_add_ifr_nbr(sifr, TRUE);
		}
	}
end:
	WBD_EXIT();
}

static void
wbd_slave_send_operating_chan_report(i5_dm_interface_type *sifr)
{
	ieee1905_operating_chan_report chan_rpt;
	uint8 opclass;
	int tx_pwr;
	int ret = WBDE_OK;

	WBD_ENTER();

	memset(&chan_rpt, 0, sizeof(chan_rpt));
	chan_rpt.list =	(operating_rpt_opclass_chan_list *)wbd_malloc(
			sizeof(operating_rpt_opclass_chan_list), &ret);
	WBD_ASSERT();

	memcpy(&chan_rpt.radio_mac, &sifr->InterfaceId, sizeof(chan_rpt.radio_mac));
	/* update with single chan in case of radar */
	chan_rpt.n_op_class = 1;
	blanket_get_global_rclass(sifr->chanspec, &opclass);
	chan_rpt.list->op_class = opclass;
	chan_rpt.list->chan = wf_chspec_ctlchan(sifr->chanspec);
	blanket_get_txpwr_target_max(sifr->ifname, &tx_pwr);
	chan_rpt.tx_pwr = (uint8)tx_pwr;
	/* Send operating chan report to 1905 to Controller */
	ieee1905_send_operating_chan_report(&chan_rpt);
end:
	if (chan_rpt.list) {
		free(chan_rpt.list);
	}
	WBD_EXIT();
}

/* set acsd mode to fix chanspec on interfaces of repeaters */
int
wbd_slave_set_acsd_mode_fix_chanspec(void)
{
	i5_dm_device_type *sdev = NULL;
	i5_dm_interface_type *pdmif = NULL;
	wbd_info_t *info = NULL;
	uint32 is_dedicated_bh = 0;
	int nvram_single_chan_bh = 0;
	int ret = WBDE_OK;

	WBD_ENTER();

	info = wbd_get_ginfo();

	/* set acs fix chanspec mode for:
	 * - agent not runing on controller
	 * - MultiAP toplogy operating in single channel mode
	 */
	if (!MAP_IS_AGENT(info->map_mode) || MAP_IS_CONTROLLER(info->map_mode) ||
		(WBD_MCHAN_ENAB(info->flags))) {
		WBD_DEBUG(" acsd fix chanspec mode not required, exit \n");
		goto end;
	}

	WBD_SAFE_GET_I5_SELF_DEVICE(sdev, &ret);

	nvram_single_chan_bh = blanket_get_config_val_int(NULL, WBD_NVRAM_ENAB_SINGLE_CHAN_BH, 0);

	foreach_i5glist_item(pdmif, i5_dm_interface_type, sdev->interface_list) {
		if (!i5DmIsInterfaceWireless(pdmif->MediaType)) {
			continue;
		}
		/* For Single-channel, Repeaters need to be in Fixed Chanspec mode
		 * For Multi-channel, Repeaters can run ACSD to pick best channel
		 * Exception: even in single channel mode, dedicated backhaul can
		 * operate in Multi-channel if it is not explicitly disabled setting
		 * nvram.
		 */
		is_dedicated_bh = wbd_ds_is_interface_dedicated_backhaul(pdmif);
		if (!is_dedicated_bh || (is_dedicated_bh && nvram_single_chan_bh)) {

			WBD_DEBUG("set acs mode fix chanspec for radio:["MACF"] \n",
				ETHERP_TO_MACF(&pdmif->InterfaceId));

			/* Set mode of ACS running on Repeater to Fixed Chanspec */
			wbd_slave_set_acsd_mode_to_fixchspec(pdmif);
		}
	}

end:
	WBD_EXIT();
	return ret;
}

int
wbd_slave_send_chan_info(void)
{
	int ret = WBDE_OK;
	i5_dm_device_type *sdev = NULL;
	i5_dm_interface_type *pdmif = NULL;
	i5_dm_bss_type *bss = NULL;
	wbd_ifr_item_t *ifr_vndr_info = NULL;
	wbd_info_t *info = NULL;
	unsigned int max_index = 0;
	bool fronthaul_bss_configured = FALSE;

	WBD_ENTER();

	info = wbd_get_ginfo();

	WBD_SAFE_GET_I5_SELF_DEVICE(sdev, &ret);

	/* send chan info for 5g interface, configured with atleast 1 bss as fronthaul BSS */
	foreach_i5glist_item(pdmif, i5_dm_interface_type, sdev->interface_list) {
		WBD_DEBUG("interface["MACF"] band[%d] mapflags[%x] \n",
			ETHERP_TO_MACF(pdmif->InterfaceId), pdmif->band, pdmif->mapFlags);

		ifr_vndr_info = (wbd_ifr_item_t*)pdmif->vndr_data;
		if (!ifr_vndr_info) {
			WBD_ERROR(" No valid vendor info for this interface, exit\n");
			continue;
		}

		foreach_i5glist_item(bss, i5_dm_bss_type, pdmif->bss_list) {
			if (I5_IS_BSS_FRONTHAUL(bss->mapFlags)) {
				WBD_DEBUG(" prepare chan info for bss["MACF"] with mapflags[%x] \n",
					ETHERP_TO_MACF(&bss->BSSID), bss->mapFlags);

				fronthaul_bss_configured = TRUE;
				break;
			}
		}

		if (!fronthaul_bss_configured || !WBD_BAND_TYPE_LAN_5G(pdmif->band) ||
			WBD_MCHAN_ENAB(info->flags)) {

			/* free vndr_info->chan info memory if exist */
			if (ifr_vndr_info->chan_info) {
				free(ifr_vndr_info->chan_info);
				ifr_vndr_info->chan_info = NULL;
			}
			continue;
		}

		/* prepare chan info for 5G interface configured with Fronthaul BSS */
		max_index = (sizeof(ifr_vndr_info->chan_info->chinfo))/
			(sizeof(ifr_vndr_info->chan_info->chinfo[1]));

		WBD_INFO("alloc buffer for max_index[%d] chan info size[%zu] block size[%zu]\n",
			max_index, sizeof(ifr_vndr_info->chan_info->chinfo),
			sizeof(ifr_vndr_info->chan_info->chinfo[1]));

		wbd_slave_get_chan_info(pdmif->ifname, ifr_vndr_info->chan_info, max_index);

		wbd_slave_chk_and_send_chan_config_info(pdmif, TRUE);
	}
end:
	WBD_EXIT();
	return ret;
}

#if defined(MULTIAPR2)
static void
wbd_slave_send_association_status_notification(i5_dm_bss_type *bss)
{
	ieee1905_association_status_notification assoc_notif;
	int ret = WBDE_OK;

	WBD_ENTER();

	memset(&assoc_notif, 0, sizeof(assoc_notif));
	WBD_ASSERT_ARG(bss, WBDE_INV_ARG);

	memset(&assoc_notif, 0, sizeof(assoc_notif));
	assoc_notif.list = (association_status_notification_bss *)wbd_malloc(
			sizeof(association_status_notification_bss), &ret);
	WBD_ASSERT();

	memcpy(&assoc_notif.list->bssid, &bss->BSSID, sizeof(assoc_notif.list->bssid));
	/* current support is for 1 bssid */
	assoc_notif.count = 1;
	assoc_notif.list->assoc_allowance_status = bss->assoc_allowance_status;
	/* Send 1905 association status notification to Controller */
	ieee1905_send_association_status_notification(&assoc_notif);
end:
	if (assoc_notif.list) {
		free(assoc_notif.list);
	}
	WBD_EXIT();
}
#endif /* MULTIAPR2 */

void
wbd_slave_update_chanspec_timer_cb(bcm_usched_handle *hdl, void *arg)
{
	int ret = WBDE_OK;
	i5_dm_device_type *sdev = NULL;
	i5_dm_interface_type *sifr = NULL;
	i5_dm_bss_type *bss = NULL;

	WBD_ENTER();

	WBD_SAFE_GET_I5_SELF_DEVICE(sdev, &ret);

	foreach_i5glist_item(sifr, i5_dm_interface_type, sdev->interface_list) {
		chanspec_t chanspec;
		int tx_pwr;

		if (!WBD_I5LL_NEXT(&sifr->bss_list)) {
			continue;
		}

		blanket_get_chanspec(sifr->ifname, &chanspec);
		blanket_get_txpwr_target_max(sifr->ifname, &tx_pwr);
		if ((chanspec == sifr->chanspec) &&
			(!sifr->TxPowerLimit || sifr->TxPowerLimit == tx_pwr)) {
			continue;
		}
		WBD_INFO("Chanspec/tx power changed for interface [%s] "
			"chanspec: new [0x%x] old [0x%x] Tx power: new[%d] old [%d]\n",
			sifr->ifname, chanspec, sifr->chanspec, tx_pwr, sifr->TxPowerLimit);
		if (sifr->chanspec != chanspec) {
			sifr->chanspec = chanspec;
			ieee1905_notify_channel_change(sifr);
		}
		sifr->TxPowerLimit = (unsigned char)tx_pwr;
		wbd_slave_send_operating_chan_report(sifr);
		/* check if agent needs to inform controller about
		 * any recent change in chan info if any.
		 * if yes.. prepare vendor message with new chan info
		 * and list of chanspec and send to controller.
		 */
		/* only fronthaul interface chan info is required */
		if (WBD_BAND_TYPE_LAN_5G(sifr->band)) {
			foreach_i5glist_item(bss, i5_dm_bss_type, sifr->bss_list) {
				if (I5_IS_BSS_FRONTHAUL(bss->mapFlags)) {
					WBD_DEBUG("check existing chan info for bss["MACF"]"
						"with mapflags[%x], with new chan info"
						"if different than existing\n",
						ETHERP_TO_MACF(&bss->BSSID), bss->mapFlags);
					wbd_slave_chk_and_send_chan_config_info(sifr, TRUE);
					break;
				}
			}
		}
		wbd_slave_add_ifr_nbr(sifr, TRUE);
	}
end:
	WBD_EXIT();
}

/* Send BSS capability report message */
static void
wbd_slave_send_bss_capability_report_cmd(unsigned char *neighbor_al_mac, unsigned char *radio_mac)
{
	int ret = WBDE_OK;
	i5_dm_interface_type *i5_ifr = NULL;
	ieee1905_vendor_data vndr_msg_data;
	WBD_ENTER();

	/* Fill vndr_msg_data struct object to send Vendor Message */
	memset(&vndr_msg_data, 0x00, sizeof(vndr_msg_data));
	memcpy(vndr_msg_data.neighbor_al_mac, neighbor_al_mac, IEEE1905_MAC_ADDR_LEN);

	WBD_SAFE_GET_I5_SELF_IFR(radio_mac, i5_ifr, &ret);

	/* Allocate Dynamic mem for Vendor data from App */
	vndr_msg_data.vendorSpec_msg =
		(unsigned char *)wbd_malloc(IEEE1905_MAX_VNDR_DATA_BUF, &ret);
	WBD_ASSERT();

	/* Update the BSS capability */
	wbd_slave_update_bss_capability(i5_ifr);

	WBD_INFO("Send BSS capability report from Device["MACDBG"] to Device["MACDBG"]\n",
		MAC2STRDBG(ieee1905_get_al_mac()), MAC2STRDBG(vndr_msg_data.neighbor_al_mac));

	/* Encode Vendor Specific TLV for Message : BSS capability report to send */
	ret = wbd_tlv_encode_bss_capability_report((void *)i5_ifr,
		vndr_msg_data.vendorSpec_msg, &vndr_msg_data.vendorSpec_len);
	WBD_ASSERT_MSG("Failed to encode BSS capability Report which needs to be sent "
		"from Device["MACDBG"] to Device["MACDBG"]\n",
		MAC2STRDBG(ieee1905_get_al_mac()), MAC2STRDBG(vndr_msg_data.neighbor_al_mac));

	/* Send Vendor Specific Message : BSS capability report */
	ret = ieee1905_send_vendor_specific_msg(&vndr_msg_data);

	WBD_CHECK_ERR_MSG(WBDE_DS_1905_ERR, "Failed to send "
		"BSS capability Report from Device["MACDBG"] to Device["MACDBG"], Error : %s\n",
		MAC2STRDBG(ieee1905_get_al_mac()), MAC2STRDBG(vndr_msg_data.neighbor_al_mac),
		wbderrorstr(ret));

end:

	if (vndr_msg_data.vendorSpec_msg) {
		free(vndr_msg_data.vendorSpec_msg);
	}
	WBD_EXIT();
}

/* Processes BSS capability query message */
static void
wbd_slave_process_bss_capability_query_cmd(unsigned char *neighbor_al_mac,
	unsigned char *tlv_data, unsigned short tlv_data_len)
{
	int ret = WBDE_OK;
	unsigned char radio_mac[ETHER_ADDR_LEN];
	WBD_ENTER();

	memset(radio_mac, 0, sizeof(radio_mac));

	/* Validate Message data */
	WBD_ASSERT_MSGDATA(tlv_data, "BSS Capability Query");

	/* Decode Vendor Specific TLV for Message : BSS capability query on receive */
	ret = wbd_tlv_decode_bss_capability_query((void *)&radio_mac, tlv_data, tlv_data_len);
	if (ret != WBDE_OK) {
		WBD_WARNING("Failed to decode the BSS capability query TLV\n");
		goto end;
	}

	WBD_INFO("BSS capability query Vendor Specific Command : Radio MAC["MACDBG"]\n",
		MAC2STRDBG(radio_mac));

	/* Send BSS capability report */
	wbd_slave_send_bss_capability_report_cmd(neighbor_al_mac, radio_mac);

end:
	WBD_EXIT();
}

/* Calculate the Average tx_rate of all the BSS w.r.t all associated sta */
static void
wbd_device_calculate_avg_tx_rate(i5_dm_device_type *i5_device)
{
	int max_rate = 0;
	wbd_bss_item_t *bss_vndr_data;
	i5_dm_interface_type *i5_ifr;
	i5_dm_bss_type *i5_bss;
	WBD_ENTER();

	foreach_i5glist_item(i5_ifr, i5_dm_interface_type, i5_device->interface_list) {
		foreach_i5glist_item(i5_bss, i5_dm_bss_type, i5_ifr->bss_list) {

			bss_vndr_data = (wbd_bss_item_t*)i5_bss->vndr_data;
			if (bss_vndr_data == NULL) {
				WBD_WARNING("BSS["MACDBG"] NULL Vendor Data\n",
					MAC2STRDBG(i5_bss->BSSID));
				continue;
			}

			blanket_get_max_rate(i5_bss->ifname, NULL, &max_rate);
			bss_vndr_data->max_tx_rate = (uint32)max_rate;
			bss_vndr_data->avg_tx_rate = bss_vndr_data->max_tx_rate;

			WBD_DEBUG("Slave["MACDBG"] max_rate[%d]\n",
				MAC2STRDBG(i5_bss->BSSID), bss_vndr_data->max_tx_rate);
		}
	}

	WBD_EXIT();
}

/* Processes BSS metrics query message */
static void
wbd_slave_process_bss_metrics_query_cmd(unsigned char *neighbor_al_mac,
	unsigned char *tlv_data, unsigned short tlv_data_len)
{
	int ret = WBDE_OK;
	unsigned char radio_mac[ETHER_ADDR_LEN];
	WBD_ENTER();

	memset(radio_mac, 0, sizeof(radio_mac));

	/* Validate Message data */
	WBD_ASSERT_MSGDATA(tlv_data, "BSS metrics Query");

	/* Decode Vendor Specific TLV for Message : BSS metrics query on receive */
	ret = wbd_tlv_decode_bss_metrics_query(NULL, tlv_data, tlv_data_len);
	WBD_ASSERT_MSG("Failed to decode the BSS metrics query TLV From Device["MACDBG"]\n",
		MAC2STRDBG(neighbor_al_mac));

	/* Send BSS metrics report */
	wbd_slave_send_bss_metrics_report_cmd(neighbor_al_mac);

end:
	WBD_EXIT();
}

/* Send BSS metrics report message */
static void
wbd_slave_send_bss_metrics_report_cmd(unsigned char *neighbor_al_mac)
{
	int ret = WBDE_OK;
	i5_dm_device_type *i5_device;
	ieee1905_vendor_data vndr_msg_data;
	WBD_ENTER();

	/* Fill vndr_msg_data struct object to send Vendor Message */
	memset(&vndr_msg_data, 0x00, sizeof(vndr_msg_data));
	memcpy(vndr_msg_data.neighbor_al_mac, neighbor_al_mac, IEEE1905_MAC_ADDR_LEN);

	/* Allocate Dynamic mem for Vendor data from App */
	vndr_msg_data.vendorSpec_msg =
		(unsigned char *)wbd_malloc(IEEE1905_MAX_VNDR_DATA_BUF, &ret);
	WBD_ASSERT();

	WBD_INFO("Send BSS metrics report from Device["MACDBG"] to Device["MACDBG"]\n",
		MAC2STRDBG(ieee1905_get_al_mac()), MAC2STRDBG(vndr_msg_data.neighbor_al_mac));

	WBD_SAFE_GET_I5_SELF_DEVICE(i5_device, &ret);

	wbd_device_calculate_avg_tx_rate(i5_device);

	/* Encode Vendor Specific TLV for Message : BSS metrics report to send */
	ret = wbd_tlv_encode_bss_metrics_report((void *)i5_device,
		vndr_msg_data.vendorSpec_msg, &vndr_msg_data.vendorSpec_len);
	WBD_ASSERT_MSG("Failed to encode BSS metrics Report which needs to be sent "
		"from Device["MACDBG"] to Device["MACDBG"]\n",
		MAC2STRDBG(ieee1905_get_al_mac()), MAC2STRDBG(vndr_msg_data.neighbor_al_mac));

	/* Send Vendor Specific Message : BSS metrics report */
	ret = ieee1905_send_vendor_specific_msg(&vndr_msg_data);

	WBD_CHECK_ERR_MSG(WBDE_DS_1905_ERR, "Failed to send "
		"BSS metrics Report from Device["MACDBG"] to Device["MACDBG"], Error : %s\n",
		MAC2STRDBG(ieee1905_get_al_mac()), MAC2STRDBG(vndr_msg_data.neighbor_al_mac),
		wbderrorstr(ret));

end:

	if (vndr_msg_data.vendorSpec_msg) {
		free(vndr_msg_data.vendorSpec_msg);
	}
	WBD_EXIT();
}

/* start/stop DFS_AP_MOVE on reception of zwdfs msg from controller via
 * acsd. skip this process for interface operating in apsta mode
 */
static int
wbd_slave_process_zwdfs_msg(unsigned char* neighbor_al_mac, unsigned char *tlv_data,
	unsigned short tlv_data_len)
{
	int ret = WBDE_OK;
	i5_dm_device_type *self_device = NULL;
	wbd_cmd_vndr_zwdfs_msg_t zwdfs_msg;
	wl_chan_change_reason_t reason;
	i5_dm_interface_type *pdmif = NULL;
	wbd_slave_item_t *slave = NULL;
	chanspec_t chanspec;
	uint bw = 0;
	wbd_info_t *info = wbd_get_ginfo();

	if (!tlv_data || !tlv_data_len || !neighbor_al_mac || !info)
	{
		WBD_ERROR("Invalid argument passed, exit \n");
		return WBDE_INV_ARG;
	}

	memset(&zwdfs_msg, 0, sizeof(zwdfs_msg));

	WBD_SAFE_GET_I5_SELF_DEVICE(self_device, &ret);

	wbd_tlv_decode_zwdfs_msg((void*)&zwdfs_msg, tlv_data, tlv_data_len);

	WBD_INFO("Decode zwdfs message: reason[%d], cntrl_chan[%d] opclass[%d] "
		"from device:["MACF"] to interface:["MACF"]\n",
		zwdfs_msg.reason, zwdfs_msg.cntrl_chan, zwdfs_msg.opclass,
		ETHERP_TO_MACF(neighbor_al_mac), ETHERP_TO_MACF(&zwdfs_msg.mac));

	WBD_SAFE_GET_SLAVE_ITEM(info, &zwdfs_msg.mac, WBD_CMP_MAC, slave, (&ret));

	pdmif = wbd_ds_get_i5_interface((uchar*)self_device->DeviceId,
			(uchar*)&zwdfs_msg.mac, &ret);
	WBD_ASSERT();

	reason = zwdfs_msg.reason;

	if (slave->wbd_ifr.apsta) {
		WBD_DEBUG("Repeater is in APSTA mode, Not required, return \n");
		/* Firmware is synchronizing channel switch across APSTA
		 * repeater and Root AP
		 */
		goto end;
	}
	if (reason == WL_CHAN_REASON_DFS_AP_MOVE_SUCCESS) {
		goto end;
	}

	blanket_get_bw_from_rc(zwdfs_msg.opclass, &bw);
	chanspec = wf_channel2chspec(zwdfs_msg.cntrl_chan, bw);
	/* Update the chanspec through ACSD */
	wbd_slave_set_chanspec_through_acsd(slave, chanspec, reason, pdmif);

end:
	(void)(pdmif);
	return ret;
}

/* Send 1905 Vendor Specific AP_CHAN_CHANGE from Agent to Controller */
static int
wbd_slave_send_vendor_msg_zwdfs(wbd_cmd_vndr_zwdfs_msg_t *msg)
{
	int ret = WBDE_OK;
	ieee1905_vendor_data vndr_msg_data;
	i5_dm_device_type *controller_device = NULL;

	WBD_ENTER();

	memset(&vndr_msg_data, 0, sizeof(vndr_msg_data));

	WBD_ASSERT_ARG(msg, WBDE_INV_ARG);
	/* Allocate Dynamic mem for Vendor data from App */
	vndr_msg_data.vendorSpec_msg =
		(unsigned char *)wbd_malloc((sizeof(*msg) + sizeof(i5_tlv_t)), &ret);
	WBD_ASSERT();

	WBD_SAFE_GET_I5_CTLR_DEVICE(controller_device, &ret);

	/* Fill Destination AL_MAC in Vendor data */
	memcpy(vndr_msg_data.neighbor_al_mac,
		controller_device->DeviceId, IEEE1905_MAC_ADDR_LEN);

	WBD_INFO("Send vendor msg ZWDFS with reason[%d] from Device["MACDBG"] to"
		" Device["MACDBG"] for  src interface["MACF"] cntrl_chan[%d],"
		" opclass[%d]\n",
		msg->reason, MAC2STRDBG(ieee1905_get_al_mac()),
		MAC2STRDBG(controller_device->DeviceId), ETHERP_TO_MACF(&msg->mac),
		msg->cntrl_chan, msg->opclass);

	wbd_tlv_encode_zwdfs_msg((void*)msg, vndr_msg_data.vendorSpec_msg,
		&vndr_msg_data.vendorSpec_len);

	WBD_DEBUG("send zwdfs message vendor data len[%d]\n", vndr_msg_data.vendorSpec_len);

	/* Send Vendor Specific Message */
	ieee1905_send_vendor_specific_msg(&vndr_msg_data);

	WBD_CHECK_ERR_MSG(WBDE_DS_1905_ERR, "Failed to send "
		"AP_CHAN_CHANGE msg from Device["MACDBG"], Error : %s\n",
		MAC2STRDBG(ieee1905_get_al_mac()), wbderrorstr(ret));

end:
	if (vndr_msg_data.vendorSpec_msg) {
		free(vndr_msg_data.vendorSpec_msg);
	}
	WBD_EXIT();
	return ret;
}

static int
wbd_slave_process_vndr_set_chan_cmd(unsigned char* neighbor_al_mac, unsigned char *tlv_data,
	unsigned short tlv_data_len)
{
	int ret = WBDE_OK;
	i5_dm_interface_type *pdmif = NULL;
	wbd_slave_item_t *slave = NULL;
	wbd_cmd_vndr_set_chan_t *msg;
	chanspec_t chanspec;
	uint bw = 0;
	wbd_info_t *info = wbd_get_ginfo();
	i5_dm_bss_type *pbss;

	if (!tlv_data || !tlv_data_len || !neighbor_al_mac || !info)
	{
		WBD_ERROR("Invalid argument passed, exit \n");
		return WBDE_INV_ARG;
	}

	tlv_data += sizeof(i5_tlv_t);
	msg = (wbd_cmd_vndr_set_chan_t *)tlv_data;
	WBD_INFO("Decode message: cntrl_chan[%d] opclass[%d] "
		"from device ["MACF"] to interface:["MACF"] \n",
		msg->cntrl_chan, msg->rclass,
		ETHERP_TO_MACF(neighbor_al_mac), ETHERP_TO_MACF(&msg->mac));

	WBD_SAFE_GET_I5_SELF_IFR((uchar*)&msg->mac, pdmif, &ret);
	pdmif->msg_stat_flag = I5_MSG_RECEIVED;
	pbss = (i5_dm_bss_type *)pdmif->bss_list.ll.next;
	if (!pbss) {
		WBD_INFO("No bss on interface ["MACF"]. Skip setting channel\n",
			ETHERP_TO_MACF(&pdmif->InterfaceId));
		goto end;
	}

	if (pdmif->mapFlags & IEEE1905_MAP_FLAG_STA &&
		wbd_slave_is_backhaul_sta_associated(pdmif->ifname, NULL)) {
		/* no need to set channel on sta interface if its associated.
		 * sta follows upstream AP
		 */
		WBD_INFO("No need to set channel on an associated sta interface \n");
		goto end;
	}
	WBD_SAFE_GET_SLAVE_ITEM(info, (struct ether_addr *)pbss->BSSID, WBD_CMP_MAC, slave, (&ret));

	blanket_get_bw_from_rc(msg->rclass, &bw);
	chanspec = wf_channel2chspec(msg->cntrl_chan, bw);
	WBD_INFO(" passed chanspec to acsd [%x] \n", chanspec);
	/* Update the chanspec through ACSD */
	wbd_slave_set_chanspec_through_acsd(slave, chanspec, WBD_REASON_CSA, pdmif);
	/* set chanspec to pdmif */
	pdmif->chanspec = chanspec;
end:
	(void)(pdmif);
	return ret;
}

/* Processes backhaul STA metric policy vendor message */
static void
wbd_slave_process_backhaul_sta_metric_policy_cmd(wbd_info_t *info, unsigned char *neighbor_al_mac,
	unsigned char *tlv_data, unsigned short tlv_data_len)
{
	int ret = WBDE_OK;
	uint32 rc_restart = 0;
	wbd_weak_sta_policy_t metric_policy;
	char new_val[WBD_MAX_BUF_128];
	WBD_ENTER();

	/* Validate Message data */
	WBD_ASSERT_MSGDATA(tlv_data, "Backhaul STA Metric Policy");

	memset(&metric_policy, 0x00, sizeof(metric_policy));

	/* Decode Vendor Specific TLV for Message : Backhaul STA Vendor Metric Reporting Policy
	 * Config on receive
	 */
	ret = wbd_tlv_decode_backhaul_sta_metric_report_policy((void*)&metric_policy, tlv_data,
		tlv_data_len);
	WBD_ASSERT_MSG("Failed to decode the Backhaul STA Metric Policy TLV From "
		"Device["MACDBG"]\n", MAC2STRDBG(neighbor_al_mac));

	/* Prepare New Value of "wbd_weak_sta_cfg_bh" from Policy Received from Controller */
	snprintf(new_val, sizeof(new_val), "%d %d %d %d %d 0x%04x",
		metric_policy.t_idle_rate, metric_policy.t_rssi, metric_policy.t_hysterisis,
		metric_policy.t_tx_rate, metric_policy.t_tx_failures, metric_policy.flags);

	/* Match NVRAM and NEW value, and if mismatch, Set new value in NVRAM */
	rc_restart |= blanket_nvram_prefix_match_set(NULL, WBD_NVRAM_WEAK_STA_CFG_BH,
		new_val, FALSE);

	WBD_INFO("Backhaul STA Metric Policy from device["MACDBG"] t_rssi[%d] t_hysterisis[%d] "
		"t_idle_rate[%d] t_tx_rate[%d] t_tx_failures[%d] flags[0x%x] NVRAM[%s=%s]\n",
		MAC2STRDBG(neighbor_al_mac), metric_policy.t_rssi, metric_policy.t_hysterisis,
		metric_policy.t_idle_rate, metric_policy.t_tx_rate, metric_policy.t_tx_failures,
		metric_policy.flags, WBD_NVRAM_WEAK_STA_CFG_BH, new_val);

	WBD_INFO("Backhaul Weak STA Config %s Changed.%s\n", rc_restart ? "" : "NOT",
		rc_restart ? "RC Restart...!!" : "");

	/* If required, Execute nvram commit/rc restart/reboot commands */
	if (rc_restart) {
		WBD_INFO("Creating rc restart timer\n");
		wbd_slave_create_rc_restart_timer(info);
		info->flags |= WBD_INFO_FLAGS_RC_RESTART;
	}
end:
	WBD_EXIT();
}

/* Processes NVRAM set vendor message */
static void
wbd_slave_process_vndr_nvram_set_cmd(wbd_info_t *info, unsigned char *neighbor_al_mac,
	unsigned char *tlv_data, unsigned short tlv_data_len)
{
	int ret = WBDE_OK, idx, k;
	uint32 rc_restart = 0;
	wbd_cmd_vndr_nvram_set_t cmd;
	i5_dm_interface_type *i5_ifr;
	i5_dm_bss_type *i5_bss;
	char prefix[IFNAMSIZ];
	WBD_ENTER();

	/* Validate Message data */
	WBD_ASSERT_MSGDATA(tlv_data, "NVRAM Set Vendor Message");

	memset(&cmd, 0x00, sizeof(cmd));

	/* Decode Vendor Specific TLV for Message : NVRAM set on receive */
	ret = wbd_tlv_decode_nvram_set((void*)&cmd, tlv_data,
		tlv_data_len);
	WBD_ASSERT_MSG("Failed to decode the NVRAM Set TLV From Device["MACDBG"]\n",
		MAC2STRDBG(neighbor_al_mac));

	/* Set all the common NVRAMs (Without Prefix) */
	for (idx = 0; idx < cmd.n_common_nvrams; idx++) {
		/* Match NVRAM and NEW value, and if mismatch, Set new value in NVRAM */
		if (strlen(cmd.common_nvrams[idx].name) > 0 &&
			strlen(cmd.common_nvrams[idx].value) > 0) {
			rc_restart |= blanket_nvram_prefix_match_set(NULL,
				cmd.common_nvrams[idx].name,
				cmd.common_nvrams[idx].value, FALSE);
			WBD_DEBUG("NVRAM Set from device["MACDBG"] NVRAM[%s=%s]\n",
				MAC2STRDBG(neighbor_al_mac),
				cmd.common_nvrams[idx].name, cmd.common_nvrams[idx].value);
		}
	}

	/* Set all the Radio NVRAMs (With Primary Prefix) */
	for (idx = 0; idx < cmd.n_radios; idx++) {
		i5_ifr = wbd_ds_get_self_i5_interface(cmd.radio_nvrams[idx].mac, &ret);
		if (i5_ifr == NULL) {
			WBD_INFO("For["MACDBG"]. %s\n", MAC2STRDBG(cmd.radio_nvrams[idx].mac),
				wbderrorstr(ret));
			continue;
		}

		/* Prefix for this Radio */
		snprintf(prefix, sizeof(prefix), "%s_", i5_ifr->wlParentName);

		for (k = 0; k < cmd.radio_nvrams[idx].n_nvrams; k++) {
			/* Match NVRAM and NEW value, and if mismatch, Set new value in NVRAM */
			if (strlen(cmd.radio_nvrams[idx].nvrams[k].name) > 0 &&
				strlen(cmd.radio_nvrams[idx].nvrams[k].value) > 0) {
				rc_restart |= blanket_nvram_prefix_match_set(prefix,
					cmd.radio_nvrams[idx].nvrams[k].name,
					cmd.radio_nvrams[idx].nvrams[k].value, FALSE);
				WBD_DEBUG("Radio NVRAM Set from device["MACDBG"] NVRAM[%s_%s=%s]\n",
					MAC2STRDBG(neighbor_al_mac), prefix,
					cmd.radio_nvrams[idx].nvrams[k].name,
					cmd.radio_nvrams[idx].nvrams[k].value);
			}
		}
	}

	/* Set all the BSS NVRAMs (With MBSS Prefix) */
	for (idx = 0; idx < cmd.n_bsss; idx++) {
		i5_bss = wbd_ds_get_self_i5_bss(cmd.bss_nvrams[idx].mac, &ret);
		if (i5_bss == NULL) {
			WBD_INFO("For["MACDBG"]. %s\n", MAC2STRDBG(cmd.bss_nvrams[idx].mac),
				wbderrorstr(ret));
			continue;
		}

		/* Get Prefix for this BSS */
		blanket_get_interface_prefix(i5_bss->ifname, prefix, sizeof(prefix));

		for (k = 0; k < cmd.bss_nvrams[idx].n_nvrams; k++) {
			/* Match NVRAM and NEW value, and if mismatch, Set new value in NVRAM */
			if (strlen(cmd.bss_nvrams[idx].nvrams[k].name) > 0 &&
				strlen(cmd.bss_nvrams[idx].nvrams[k].value) > 0) {
				rc_restart |= blanket_nvram_prefix_match_set(prefix,
					cmd.bss_nvrams[idx].nvrams[k].name,
					cmd.bss_nvrams[idx].nvrams[k].value, FALSE);
				WBD_DEBUG("BSS NVRAM Set from device["MACDBG"] NVRAM[%s_%s=%s]\n",
					MAC2STRDBG(neighbor_al_mac), prefix,
					cmd.bss_nvrams[idx].nvrams[k].name,
					cmd.bss_nvrams[idx].nvrams[k].value);
			}
		}
	}

	WBD_INFO("NVRAM Set %s Changed.%s\n", rc_restart ? "" : "NOT",
		rc_restart ? "RC Restart...!!" : "");

	/* If required, Execute nvram commit/rc restart/reboot commands */
	if (rc_restart) {
		WBD_INFO("Creating rc restart timer\n");
		wbd_slave_create_rc_restart_timer(info);
		info->flags |= WBD_INFO_FLAGS_RC_RESTART;
	}
end:
	wbd_free_nvram_sets(&cmd);
	WBD_EXIT();
}

#if defined(MULTIAPR2)
/* Processes Channel Scan Request by Agent */
void
wbd_slave_process_channel_scan_req_cb(unsigned char *src_al_mac,
	ieee1905_chscan_req_msg *chscan_req)
{
	int ret = WBDE_OK, idx_chan = 0;
	ieee1905_per_radio_opclass_list *ifr = NULL;
	i5_dm_device_type *self_device = NULL;
	i5_dm_interface_type *i5_ifr = NULL;
	WBD_ENTER();

	/* Validate Message data */
	WBD_ASSERT_MSGDATA(chscan_req, "Channel Scan Request");

	WBD_SAFE_GET_I5_SELF_DEVICE(self_device, &ret);

	/* Extract Channel Scan Request TLV Flags */
	WBD_DEBUG("Flags = [0x%X] ", chscan_req->chscan_req_msg_flag);

	/* Extract Number of radios : upon which channel scans are requested */
	WBD_DEBUG("Num of Radios = [%d] \n", chscan_req->num_of_radios);

	/* Extract details for each radio */
	foreach_iglist_item(ifr, ieee1905_per_radio_opclass_list, chscan_req->radio_list) {

		ieee1905_per_opclass_chan_list *opcls = NULL;

		/* Find IFR in Self Device, Find Vendor IFR Data */
		i5_ifr = i5DmInterfaceFind(self_device, ifr->radio_mac);
		if (i5_ifr == NULL) {
			WBD_WARNING("IFR["MACF"] Not Found\n", ETHERP_TO_MACF(ifr->radio_mac));
			continue;
		}

		/* Extract Radio Unique Identifier of a radio of the Multi-AP Agent */
		WBD_DEBUG("IFR["MACF"] ", ETHERP_TO_MACF(ifr->radio_mac));

		/* Extract Number of Operating Classes */
		WBD_DEBUG("Num of OpClass = [%d] \n", ifr->num_of_opclass);

		/* Extract details for each Operating Class */
		foreach_iglist_item(opcls, ieee1905_per_opclass_chan_list, ifr->opclass_list) {

			/* Extract Operating Class Value */
			WBD_DEBUG("OpClass = [%d] ", opcls->opclass_val);

			/* Extract Number of Channels specified in the Channel List */
			WBD_DEBUG("Num of Channels = [%d] \n", opcls->num_of_channels);

			/* Extract Channel List */
			for (idx_chan = 0; idx_chan < opcls->num_of_channels; idx_chan++) {
				WBD_DEBUG("Channel = [%d] \n", opcls->chan_list[idx_chan]);
				WBD_DEBUG("Channel Supported = [%d] \n",
					opcls->supported_chan_list[idx_chan]);
			}
		}
	}

	/* Start Requested Channel Scan on Agent, requested by Controller */
	wbd_slave_process_requested_channel_scan(src_al_mac, chscan_req);

end:
	WBD_EXIT();
}
#endif /* MULTIAPR2 */

/* Store the BSSID in the NVRAM . If the force is set, then store it directly. Else store only
 * if the NVRAM is not defined
 */
void
wbd_slave_store_bssid_nvram(char *prefix, unsigned char *bssid, int force)
{
	char *nvval;
	char str_mac[WBD_STR_MAC_LEN + 1] = {0};

	wbd_ether_etoa(bssid, str_mac);

	/* force is 1, set directly */
	if (force) {
		blanket_nvram_prefix_set(prefix, NVRAM_BSSID, str_mac);
		nvram_commit();
	} else {
		/* Set only if the NVRAM is not set */
		nvval = blanket_nvram_prefix_safe_get(prefix, NVRAM_BSSID);
		if (!nvval || (strlen(nvval) < WBD_STR_MAC_LEN)) {
			blanket_nvram_prefix_set(prefix, NVRAM_BSSID, str_mac);
			nvram_commit();
		}
	}
}

/* Use controller's chan info and local chanspecs list to prepare dfs_channel_forced
 * list and pass to firmware via "dfs_channel_forced" iovar
 */
static void
wbd_slave_process_dfs_chan_info(unsigned char *src_al_mac, unsigned char *tlv_data,
	unsigned short tlv_data_len)
{
	i5_dm_interface_type *pdmif = NULL;
	wbd_cmd_vndr_controller_dfs_chan_info_t chan_info_msg;
	wl_dfs_forced_t *dfs_frcd = NULL;
	int ret = WBDE_OK;
	uint32 ioctl_size = 0;

	WBD_ENTER();
	memset(&chan_info_msg, 0, sizeof(chan_info_msg));

	ret = wbd_tlv_decode_dfs_chan_info((void*)&chan_info_msg, tlv_data, tlv_data_len);
	WBD_ASSERT();

	WBD_DEBUG("dfs chan msg : mac["MACF"] band[%d] chan_info count[%d]\n",
		ETHERP_TO_MACF(&chan_info_msg.mac), chan_info_msg.band,
		chan_info_msg.chan_info->count);

	WBD_SAFE_GET_I5_SELF_IFR((uchar*)&chan_info_msg.mac, pdmif, &ret);
	/* prepare the dfs list */
	dfs_frcd = (wl_dfs_forced_t*)wbd_malloc(WBD_MAX_BUF_512, &ret);

	/* Presume: chan info from master is different than agent's curret chan info
	 * and proceed to prepare dfs list with provided chan info
	 */
	wbd_slave_prepare_dfs_list(chan_info_msg.chan_info, pdmif, pdmif->band, dfs_frcd);

	ioctl_size = WL_DFS_FORCED_PARAMS_FIXED_SIZE +
		(dfs_frcd->chspec_list.num * sizeof(chanspec_t));

	dfs_frcd->version = DFS_PREFCHANLIST_VER;
	/* set dfs force list */
	blanket_set_dfs_forced_chspec(pdmif->ifname, dfs_frcd, ioctl_size);
end:
	if (dfs_frcd) {
		free(dfs_frcd);
	}
	if (chan_info_msg.chan_info) {
		free(chan_info_msg.chan_info);
	}
	WBD_EXIT();
}

/* Get list of channels  in 20 Mhz BW by issuing chan info iovar
 * and compare with slave's local data base if present. Inform
 * controller via vendor message with TLV: CHAN_INFO and chan info
 * payload.
 */
static void
wbd_slave_chk_and_send_chan_config_info(i5_dm_interface_type *intf, bool send_explictly)
{
	ieee1905_vendor_data vndr_msg;
	i5_dm_device_type *controller_device = NULL;
	wbd_ifr_item_t *ifr_vndr_info = NULL;
	wbd_interface_chan_info_t *wl_chan_info = NULL;
	wbd_interface_chan_info_t *chan_info = NULL;
	wbd_cmd_vndr_intf_chan_info_t *chan_info_msg = NULL;
	wbd_info_t *info = NULL;
	char *ifname = NULL;
	int len = WBD_MAX_BUF_512;
	unsigned int max_index = 0;
	int ret = WBDE_OK;

	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(intf, WBDE_INV_ARG);
	memset(&vndr_msg, 0, sizeof(vndr_msg));

	info = wbd_get_ginfo();

	ifr_vndr_info = (wbd_ifr_item_t *)intf->vndr_data;

	/* perform chan_info for 5G BAND ONLY and single chan case */
	if (!WBD_BAND_TYPE_LAN_5G(intf->band) ||
		(WBD_MCHAN_ENAB(info->flags))) {
		goto end;
	}

	WBD_SAFE_GET_I5_CTLR_DEVICE(controller_device, &ret);

	if (send_explictly) {
		/* explicit request at init time to let controller
		 * know of Fronthaul 5g interface chan info.
		 */
		goto xmit_chan_info;
	}
	/* WBD logic got to know of channel change or radar event,
	 * get chan info from firmware. Compare with existing chan info
	 * and send new chan info if required.
	 */
	ifname = intf->ifname;
	wl_chan_info = (wbd_interface_chan_info_t*)wbd_malloc(len, &ret);
	WBD_ASSERT();

	max_index = (sizeof(ifr_vndr_info->chan_info->chinfo))/
		(sizeof(ifr_vndr_info->chan_info->chinfo[1]));

	ret = wbd_slave_get_chan_info(ifname, wl_chan_info, max_index);

	WBD_INFO("Band[%d] Slave["MACF"] cur chinfo count[%d] prv chinfo count [%d] compare\n",
		intf->band, ETHERP_TO_MACF(&intf->InterfaceId), wl_chan_info->count,
		ifr_vndr_info->chan_info->count);

	if (!memcmp((uchar*)wl_chan_info, (uchar*)ifr_vndr_info->chan_info,
		wl_chan_info->count * (sizeof(wbd_chan_info_t)))) {
		/* no need to send chan info, as no change in chan info
		 * information
		 */
		WBD_DEBUG("NO change in chan info, no need to send chan info\n");
		goto end;
	}

xmit_chan_info:
	chan_info = send_explictly ? ifr_vndr_info->chan_info : wl_chan_info;
	/* chan info is different, prepare vendor message to send chan info to
	 * controller
	 */
	vndr_msg.vendorSpec_msg =
		(unsigned char *)wbd_malloc(IEEE1905_MAX_VNDR_DATA_BUF, &ret);
	WBD_ASSERT();

	/* Fill Destination AL_MAC in Vendor data */
	memcpy(vndr_msg.neighbor_al_mac,
		controller_device->DeviceId, IEEE1905_MAC_ADDR_LEN);

	WBD_DEBUG("new chan info, update controller for chan info and chanspec list \n");

	chan_info_msg = (wbd_cmd_vndr_intf_chan_info_t *)wbd_malloc(
		sizeof(wbd_cmd_vndr_intf_chan_info_t), &ret);

	WBD_ASSERT();

	memcpy(&chan_info_msg->mac, &intf->InterfaceId, ETHER_ADDR_LEN);
	chan_info_msg->band = intf->band;
	chan_info_msg->chan_info = chan_info;
	WBD_DEBUG("send msg to MAC["MACF"] band[%d]\n",	ETHERP_TO_MACF(&chan_info_msg->mac),
		chan_info_msg->band);

	wbd_tlv_encode_chan_info((void*)chan_info_msg, vndr_msg.vendorSpec_msg,
		&vndr_msg.vendorSpec_len);

	WBD_DEBUG("CHAN_INFO vendor msg len[%d] MACF["MACF"] band[%d]\n",
		vndr_msg.vendorSpec_len, ETHERP_TO_MACF(&chan_info_msg->mac),
		chan_info_msg->band);

	/* Send Vendor Specific Message */
	ieee1905_send_vendor_specific_msg(&vndr_msg);
end:
	if (send_explictly) {
		if (wl_chan_info) {
			free(wl_chan_info);
		}
	} else {
		/* remove earlier interface chan_info and store latest one */
		free(ifr_vndr_info->chan_info);
		ifr_vndr_info->chan_info = wl_chan_info;
	}
	if (vndr_msg.vendorSpec_msg) {
		free(vndr_msg.vendorSpec_msg);
	}
	if (chan_info_msg) {
		free(chan_info_msg);
	}
	WBD_EXIT();
}

/* Create dfs_pref_chan_list */
static void
wbd_slave_prepare_dfs_list(wbd_dfs_chan_info_t *chan_info, i5_dm_interface_type *pdmif,
	uint8 band, wl_dfs_forced_t* dfs_frcd)
{
	wl_uint32_list_t *chanspec_list = NULL;
	int i = 0, k = 0;
	uint8 sub_channel;
	chanspec_t chspec;
	bool use_channel;
	chanspec_t curr_ch = 0x00;
	uint16 buflen = 0;
	int ret = WBDE_OK;

	WBD_ENTER();

	dfs_frcd->chspec_list.num = 0;

	/* use agent's interface current chanspec and list of chanspec
	 * to prepare dfs list
	 */
	curr_ch = pdmif->chanspec;

	/* get list of chanspec, free after use */
	buflen = (MAXCHANNEL + 1) * sizeof(uint32);

	chanspec_list =  (wl_uint32_list_t *)wbd_malloc(buflen, &ret);
	WBD_ASSERT_MSG(" Failed to allocate memory for chanspecs\n");

	blanket_get_chanspecs_list(pdmif->ifname, chanspec_list, buflen);

	WBD_DEBUG("prepare dfs chan list with chanspec[%x] chan list entries[%d] \n",
		curr_ch, chanspec_list->count);

	/* given up to 80/160 MHZ chanspecs; checks if it is a DFS channel but pre-cleared to use */
	for (i = (chanspec_list->count -1); i >= 0; i--) {
		/* Reverese traversal */
		chspec = (unsigned short)chanspec_list->element[i];

		use_channel = TRUE;

		/* could have a condition here to reject chspec not matching current bw */
		WBD_DEBUG("Band[%d] chspec[0x%x] ChanspecsCount[%d] chinfocount[%d]\n",
			band, chspec, chanspec_list->count, chan_info->count);

		FOREACH_20_SB(chspec, sub_channel) {
			/* skip chspec if sub_channel does not belong to
			 * common dfs chan info received from controller
			 */
			if (!wbd_check_chan_good_candidate(sub_channel, chan_info)) {
				WBD_DEBUG("sub_channel[%d] chanspec[%x] ignore for dfs list \n",
					sub_channel, chspec);
				use_channel = FALSE;
				break;
			}
		}
		WBD_DEBUG("use_channel[%d] for chanspec[%x] \n", use_channel, chspec);
		/* when all subchannels are in good condition this chanspec is a good candidate */
		if (use_channel) {
			dfs_frcd->chspec_list.list[k++] = chspec;
			dfs_frcd->chspec_list.num++;
			WBD_DEBUG("Band[%d] list chanspec[0x%x] index[%d] \n", band, chspec, k);
		}
	}

	WBD_DEBUG("dfs chan list entries[%d] \n", dfs_frcd->chspec_list.num);
end:
	if (chanspec_list) {
		free(chanspec_list);
	}
	WBD_EXIT();
}

/* Waiting for maximum delay in acsd to set chanspec,
 * before slave sending operating channel report.
 */
void
wbd_slave_wait_set_chanspec(wbd_slave_item_t *slave, i5_dm_interface_type *interface,
	chanspec_t chspec)
{
	int delay = 0;
	chanspec_t chanspec;

	while (delay <= WBD_MAX_ACSD_SET_CHSPEC_DELAY) {
		chanspec = 0;
		blanket_get_chanspec(interface->ifname, &chanspec);
		if (chanspec != chspec) {
			usleep(WBD_GET_CHSPEC_GAP * 1000);
			delay += WBD_GET_CHSPEC_GAP;
			continue;
		}
		break;
	}
	slave->wbd_ifr.chanspec = chanspec;
}

#if defined(MULTIAPR2)
/* send tunnel message to controller */
static void
wbd_slave_send_tunneled_msg(ieee1905_tunnel_msg_t *tunnel_msg)
{
	WBD_ENTER();

	WBD_INFO("tunnel msg for sta["MACF"] with tunnel msg type[%d] \n",
		ETHERP_TO_MACF(tunnel_msg->source_mac), tunnel_msg->payload_type);

	ieee1905_send_tunneled_msg(tunnel_msg);
	WBD_EXIT();
}

/* Remove traffic separation policy NVRAMs */
static int
wbd_slave_remove_traffic_separation_policy()
{
	int ret = -1;
	char *nvval, *next = NULL;
	char strvlan[WBD_MAX_BUF_16], nvname[WBD_MAX_BUF_128];
	WBD_ENTER();

	nvval = blanket_nvram_safe_get(NVRAM_MAP_TS_VLANS);
	WBD_INFO("[%s]=[%s]\n", NVRAM_MAP_TS_VLANS, nvval);

	foreach(strvlan, nvval, next) {
		snprintf(nvname, sizeof(nvname), "map_ts_%s_ssid", strvlan);
		if (strlen(blanket_nvram_safe_get(nvname)) > 0) {
			blanket_nvram_prefix_unset(NULL, nvname);
			WBD_INFO("Unset NVRAM %s\n", nvname);
			ret = 0;
		}
	}

	if (strlen(nvval) > 0) {
		blanket_nvram_prefix_unset(NULL, NVRAM_MAP_TS_VLANS);
		WBD_INFO("Unset NVRAM %s\n", NVRAM_MAP_TS_VLANS);
		ret = 0;
	}

	if (strlen(blanket_nvram_safe_get(NVRAM_MAP_TS_PRIM_VLAN_ID)) > 0) {
		blanket_nvram_prefix_unset(NULL, NVRAM_MAP_TS_PRIM_VLAN_ID);
		WBD_INFO("Unset NVRAM %s\n", NVRAM_MAP_TS_PRIM_VLAN_ID);
		ret = 0;
	}

	if (strlen(blanket_nvram_safe_get(NVRAM_MAP_TS_DEF_PCP)) > 0) {
		blanket_nvram_prefix_unset(NULL, NVRAM_MAP_TS_DEF_PCP);
		WBD_INFO("Unset NVRAM %s\n", NVRAM_MAP_TS_DEF_PCP);
		ret = 0;
	}

	return ret;
}

/* Move the SSID's ifname to lan1_ifnames */
static int
wbd_slave_ts_move_ssid_to_secondary(unsigned char *ssid)
{
	int ret = -1, tmpret;
	char *nvval;
	char var_intf[IFNAMSIZ] = {0}, *next_intf;
	char prefix[IFNAMSIZ];

	nvval = blanket_nvram_safe_get(WBD_NVRAM_IFNAMES);

	/* Traverse wbd_ifnames for each ifname */
	foreach(var_intf, nvval, next_intf) {
		blanket_get_interface_prefix(var_intf, prefix, sizeof(prefix));

		if (strcmp(blanket_nvram_prefix_safe_get(prefix, NVRAM_SSID), (char*)ssid) != 0) {
			continue;
		}

		/* SSID is matching with the ifname. So move it to secondary */
		WBD_INFO("Moving SSID %s ifname %s to lan1_ifanmes\n", ssid, var_intf);
		tmpret = wbd_slave_move_ifname_to_list(var_intf, NVRAM_LAN_IFNAMES,
			NVRAM_LAN1_IFNAMES);
		if (tmpret == 0) {
			WBD_INFO("Moved %s to guest\n", var_intf);
			ret = 0;
		}
	}

	return ret;
}

/* Update SSID for the VLANs return 0 if there is any change in the settings */
static int
wbd_slave_process_traffic_separation_ssids_policy(ieee1905_ts_policy_t *ts_policy, char *strvlan,
	short int primary_vlan_id)
{
	int ret = -1;
	dll_t *item_p;
	ieee1905_ssid_list_type *ssid_list;
	char *nvval;
	char nvval_ssid_list[NVRAM_MAX_VALUE_LEN], nvname[NVRAM_MAX_PARAM_LEN];
	char strssid[WBD_MAX_BUF_32];

	/* Create NVRAM name in the format map_ts_vlanid_ssid */
	snprintf(nvname, sizeof(nvname), "map_ts_%s_ssid", strvlan);
	nvval = blanket_nvram_safe_get(nvname);
	memset(nvval_ssid_list, 0, sizeof(nvval_ssid_list));
	WBDSTRNCPY(nvval_ssid_list, nvval, sizeof(nvval_ssid_list));
	WBD_INFO("[%s]=[%s]\n", nvname, nvval_ssid_list);

	/* Travese Traffic Separation SSID List items */
	foreach_glist_item(item_p, ts_policy->ssid_list) {

		ssid_list = (ieee1905_ssid_list_type*)item_p;

		/* Check if the SSID is present in the list */
		if (!find_in_list(nvval_ssid_list, (const char*)ssid_list->ssid.SSID)) {
			memset(strssid, 0, sizeof(strssid));
			memcpy(strssid, ssid_list->ssid.SSID, ssid_list->ssid.SSID_len);
			if (add_to_list((const char*)ssid_list->ssid.SSID, nvval_ssid_list,
				sizeof(nvval_ssid_list)) == 0) {
				nvram_set(nvname, nvval_ssid_list);
				WBD_INFO("New [%s]=[%s]\n", nvname, nvval_ssid_list);
				ret = 0;
			}
			/* Now move the ifname to secondary bridge if the SSID belongs to non
			 * primary VLAN
			 */
			if (atoi(strvlan) != primary_vlan_id) {
				ret = wbd_slave_ts_move_ssid_to_secondary(ssid_list->ssid.SSID);
			}
		}
	}

	return ret;
}

/* Process the traffic separation policy. Return 0 If there is any change in the policy So that
 * the caller can restart
 */
int
wbd_slave_process_traffic_separation_policy(ieee1905_policy_config *policy,
	unsigned char rcvd_policies)
{
	int ret = -1;
	dll_t *item_p;
	ieee1905_ts_policy_t *ts_policy;
	char *nvval;
	char strvlan[WBD_MAX_BUF_16];
	char nvval_ts_list[NVRAM_MAX_VALUE_LEN] = {0};
	WBD_ENTER();

	/* If there is no SSID to VLANID mapping, then remove the traffic separation policy */
	if (policy->ts_policy_list.count == 0) {
		if (wbd_slave_remove_traffic_separation_policy() == 0) {
			ret = 0;
		}
		goto end;
	}

	nvval = blanket_nvram_safe_get(NVRAM_MAP_TS_VLANS);
	memset(nvval_ts_list, 0, sizeof(nvval_ts_list));
	WBDSTRNCPY(nvval_ts_list, nvval, sizeof(nvval_ts_list));
	WBD_INFO("[%s]=[%s]\n", NVRAM_MAP_TS_VLANS, nvval_ts_list);

	/* Travese Traffic Separation Policy Config List items */
	foreach_glist_item(item_p, policy->ts_policy_list) {

		ts_policy = (ieee1905_ts_policy_t*)item_p;

		/* Check if the VLAN ID is present in the list */
		snprintf(strvlan, sizeof(strvlan), "%d", ts_policy->vlan_id);
		if (!find_in_list(nvval_ts_list, strvlan)) {
			if (add_to_list(strvlan, nvval_ts_list, sizeof(nvval_ts_list)) == 0) {
				nvram_set(NVRAM_MAP_TS_VLANS, nvval_ts_list);
				WBD_INFO("New [%s]=[%s]\n", NVRAM_MAP_TS_VLANS, nvval_ts_list);
				ret = 0;
			}
		}

		if (wbd_slave_process_traffic_separation_ssids_policy(ts_policy, strvlan,
			policy->prim_vlan_id) == 0) {
			ret = 0;
		}
	}

	/* Now apply 802.1Q Default settings */
	if (rcvd_policies & MAP_POLICY_TYPE_FLAG_TS_8021QSET) {
		snprintf(strvlan, sizeof(strvlan), "%d", policy->prim_vlan_id);
		if (blanket_nvram_prefix_match_set(NULL, NVRAM_MAP_TS_PRIM_VLAN_ID, strvlan,
			FALSE)) {
			WBD_INFO("New [%s]=[%s]\n", NVRAM_MAP_TS_PRIM_VLAN_ID, strvlan);
			ret = 0;
		}
		snprintf(strvlan, sizeof(strvlan), "%d", policy->default_pcp);
		if (blanket_nvram_prefix_match_set(NULL, NVRAM_MAP_TS_DEF_PCP, strvlan, FALSE)) {
			WBD_INFO("New [%s]=[%s]\n", NVRAM_MAP_TS_DEF_PCP, strvlan);
			ret = 0;
		}
	}

end:
	WBD_EXIT();
	return ret;
}

/* Process tunnel CAC message from controller */
void wbd_slave_process_cac_msg(wbd_info_t *info, uint8 *al_mac, void *msg, uint32 msg_type)
{
	WBD_ENTER();

	switch (msg_type) {
		case MAP_CAC_RQST:
		{
			wbd_slave_process_cac_start_request(info, (ieee1905_cac_rqst_list_t*)msg);
		}
		break;

		case MAP_CAC_TERMINATE:
		{
			wbd_slave_process_cac_terminate_rqst(info,
				(ieee1905_cac_termination_list_t*)msg);
		}
		break;

		default:
		{
			WBD_ERROR("Invalid CAC msg[%d] recvd, exit \n", msg_type);
		}
		break;
	}
	WBD_EXIT();
}

/* Process CAC Request message from controller, initiate CAC on radio if required. */
static void
wbd_slave_process_cac_start_request(wbd_info_t *info, ieee1905_cac_rqst_list_t *pmsg)
{
	i5_dm_interface_type *pdmif = NULL;
	wbd_slave_item_t *slave = NULL;
	wbd_ifr_item_t *ifr_vndr_data;
	ieee1905_radio_cac_rqst_t *radio_cac_params = NULL;
	int ret = 0;
	uint bw = 0;
	uint16 chanspec = 0;
	uint8 cac_method = 0;
	uint8 cac_action = 0;
	uint8 reason = 0;
	uint8 i = 0;

	WBD_ENTER();
	if (!pmsg) {
		WBD_ERROR("Invalid CAC request arguments, exit \n");
		goto end;
	}

	WBD_INFO("CAC start request for n_radios[%d] \n", pmsg->count);

	radio_cac_params = &pmsg->params[0];

	for (i = 0; i < pmsg->count; i++) {
		blanket_get_bw_from_rc(radio_cac_params[i].opclass, &bw);
		chanspec = wf_channel2chspec(radio_cac_params[i].chan, bw);

		WBD_SAFE_GET_I5_SELF_IFR((uchar*)&radio_cac_params[i].mac, pdmif, &ret);

		ifr_vndr_data = (wbd_ifr_item_t*)pdmif->vndr_data;
		if (!ifr_vndr_data) {
			WBD_ERROR(" Invalid ifr vendor data plz Debug, exiting ..\n");
			goto end;
		}
		WBD_SAFE_GET_SLAVE_ITEM(info, (struct ether_addr*)&radio_cac_params[i].mac,
			WBD_CMP_MAC, slave, (&ret));

		WBD_INFO("CAC start request for ["MACF"] opclass[%d] chan[%d] flags[%x] \n",
			ETHERP_TO_MACF(radio_cac_params[i].mac), radio_cac_params[i].opclass,
			radio_cac_params[i].chan, radio_cac_params[i].flags);

		cac_method = GET_MAP_CAC_METHOD(radio_cac_params[i].flags);

		/* get action code, based on that decide DFS_STUNT is required or
		 * not after DFS_AP_MOVE START
		 */
		cac_action = GET_MAP_CAC_COMPLETION_ACTION(radio_cac_params[i].flags);

		/* cac method check */
		if (ifr_vndr_data->cac_method_flag != cac_method) {
			WBD_ERROR("Invalid cac method for radio["MACF"] support[%x] cac_method[%d]"
				" \n", ETHERP_TO_MACF(radio_cac_params[i].mac),
				ifr_vndr_data->cac_method_flag, cac_method);

			ifr_vndr_data->cac_rqst_status |= MAP_CAC_RQST_METHOD_NOT_SUPPORT;
			continue;

		}

		WBD_INFO("cac_action[%d] \n", cac_action);

		if (cac_method == MAP_CAC_METHOD_MIMO_DIMENSION_REDUCED_CAC) {
			reason = WL_CHAN_REASON_DFS_AP_MOVE_START;
		} else if (cac_method == MAP_CAC_METHOD_CONTINOUS_CAC) {
			if (cac_action == MAP_CAC_ACTION_STAY_ON_NEW_CHANNEL_MONITOR_RADAR) {
				reason = WL_CHAN_REASON_CSA;
			} else {
				reason = WL_CHAN_REASON_CSA_TO_DFS_CHAN_FOR_CAC_ONLY;
			}
		}

		WBD_INFO("Before initiate CAC request for ["MACF"] current cac state[%x] \n",
			ETHERP_TO_MACF(&pdmif->InterfaceId), ifr_vndr_data->cac_rqst_status);

		/* abort any ongoing cac, if any cac is running while taking new
		 * request
		 */
		if (ifr_vndr_data->cac_rqst_status == (MAP_CAC_RQST_RUNNING | MAP_CAC_RQST_RCVD)) {
			if (ifr_vndr_data->cac_rqst_opclass == radio_cac_params[i].opclass) {
				if (ifr_vndr_data->cac_rqst_chan == radio_cac_params[i].chan) {

					WBD_INFO("cac is already running with same params"
						" chan[%d] opclass[%d] \n",
						radio_cac_params[i].chan,
						radio_cac_params[i].opclass);
					 return;
				}
			} else {
				WBD_INFO("abort Running CAC first, restart again \n");
				/* stop ongoing CAC, begin new one */
				reason = WL_CHAN_REASON_DFS_AP_MOVE_ABORTED;
				wbd_slave_set_chanspec_through_acsd(slave, chanspec, reason,
					pdmif);
			}
		}

		/* Backup of current chanspec, it is required for continuos CAC with return to
		 * previous channel case.
		 */
		ifr_vndr_data->old_chanspec = pdmif->chanspec;

		WBD_INFO("Initiate CAC reason[%d] for chanspec[%x] \n", reason, chanspec);
		/* Update the chanspec through ACSD */
		wbd_slave_set_chanspec_through_acsd(slave, chanspec, reason, pdmif);

		if ((cac_method == MAP_CAC_METHOD_MIMO_DIMENSION_REDUCED_CAC) &&
			(cac_action == MAP_CAC_ACTION_RETURN_TO_PREVIOUS_CHANNEL)) {
			/* issue stunt sub command after dfs_ap_move start */
			reason = WL_CHAN_REASON_DFS_AP_MOVE_STUNT;
			WBD_INFO("issue stunt command to DFS_AP_MOVE iovar \n");
			wbd_slave_set_chanspec_through_acsd(slave, chanspec, reason, pdmif);
		}

		WBD_INFO("Initiate CAC for ["MACF"] for chanspec[%x] with method[%d] expected"
			" action after cac[%d] \n", ETHERP_TO_MACF(&pdmif->InterfaceId), chanspec,
			cac_method, ifr_vndr_data->cac_rqst_status);

		/* start fresh, reset ABORT status if set with terminate CAC */
		ifr_vndr_data->cac_rqst_status = 0;
		ifr_vndr_data->cac_rqst_status |= (MAP_CAC_RQST_RCVD | MAP_CAC_RQST_RUNNING);
		ifr_vndr_data->cac_rqst_opclass = radio_cac_params[i].opclass;
		ifr_vndr_data->cac_rqst_chan = radio_cac_params[i].chan;

		WBD_INFO("After starting CAC rqst for interface["MACF"] cac state[%x] \n",
			ETHERP_TO_MACF(&pdmif->InterfaceId), ifr_vndr_data->cac_rqst_status);
	}
end:
	/* send channel preference report message for all radio which either not able to
	 * process cac start request or invalid cac start request
	 */
	ieee1905_send_chan_preference_report();
	WBD_EXIT();
}

/* Process CAC terminate message from controller to stop ongoing CAC on 5G radio */
static void
wbd_slave_process_cac_terminate_rqst(wbd_info_t *info, ieee1905_cac_termination_list_t *pmsg)
{
	i5_dm_interface_type *pdmif = NULL;
	ieee1905_radio_cac_params_t *radio_cac_params = NULL;
	wbd_slave_item_t *slave = NULL;
	wbd_ifr_item_t *ifr_vndr_data = NULL;
	int ret = 0;
	uint bw = 0;
	uint16 chanspec = 0;
	uint8 reason = 0;
	uint8 i = 0;

	WBD_ENTER();

	if (!pmsg) {
		WBD_ERROR("Invalid CAC terminate arguments, exit \n");
		goto end;
	}

	WBD_INFO("CAC terminate for n_radio[%d] \n", pmsg->count);

	radio_cac_params = &pmsg->params[0];

	for (i = 0; i < pmsg->count; i++) {

		/* issue dfs cac to radio */
		blanket_get_bw_from_rc(radio_cac_params[i].opclass, &bw);
		chanspec = wf_channel2chspec(radio_cac_params[i].chan, bw);

		reason = WL_CHAN_REASON_DFS_AP_MOVE_ABORTED;
		WBD_SAFE_GET_I5_SELF_IFR((uchar*)&radio_cac_params[i].mac, pdmif, &ret);

		WBD_INFO("CAC terminate rqst for: ["MACF"] ifname[%s] opclass[%d] chan[%d] \n",
			ETHERP_TO_MACF(&pdmif->InterfaceId), pdmif->ifname,
			radio_cac_params[i].opclass, radio_cac_params[i].chan);

		ifr_vndr_data = (wbd_ifr_item_t*)pdmif->vndr_data;
		if (!ifr_vndr_data) {
			WBD_ERROR(" Invalid ifr vendor data plz Debug, move to another radio\n");
			continue;
		}

		WBD_SAFE_GET_SLAVE_ITEM(info, (struct ether_addr*)&radio_cac_params[i].mac,
			WBD_CMP_MAC, slave, (&ret));

		if (ifr_vndr_data->cac_method_flag == MAP_CAC_METHOD_CONTINOUS_CAC) {
			/* fall back to original chanspec for full time CAC */
			reason = WL_CHAN_REASON_CSA;

			chanspec = ifr_vndr_data->old_chanspec;
		}

		/* Update the chanspec through ACSD */
		wbd_slave_set_chanspec_through_acsd(slave, chanspec, reason, pdmif);
	}
	(void)(radio_cac_params);
end:
	WBD_EXIT();
}

/* Prepare CAC capability for Multi AP agent with all cac capable radios */
void
wbd_slave_prepare_cac_capability(uint8 **pbuf, uint16 *payload_len)
{
	i5_dm_device_type *self_dev = NULL;
	i5_dm_interface_type *pdmif = NULL;
	ieee1905_cac_capabilities_t *cac_cap = NULL;
	uint8 *ptr = NULL;
	uint16 filled_len = 0;
	uint8 cac_capable_radio = 0;
	int ret = WBDE_OK;

	WBD_ENTER();

	cac_cap = (ieee1905_cac_capabilities_t*)*pbuf;
	ptr = *pbuf;

	/* TODO: use country IOVAR */
	memcpy((uint8*)&cac_cap->country[0], "US", 2);

	WBD_SAFE_GET_I5_SELF_DEVICE(self_dev, &ret);

	ptr += OFFSETOF(ieee1905_cac_capabilities_t, ifr_info);
	/* traverse each interface in self device and add cac capability for valid
	 * interface i.e. 5G and cac capable
	 */
	foreach_i5glist_item(pdmif, i5_dm_interface_type, self_dev->interface_list) {

		if (!WBD_BAND_TYPE_LAN_5G(pdmif->band) ||
			!i5DmIsInterfaceWireless(pdmif->MediaType)) {
			continue;
		}

		wbd_slave_prepare_radio_cac_capability(pdmif, &ptr, &filled_len);
		cac_capable_radio++;

		ptr += filled_len;

		WBD_DEBUG("Done CAC capability for ["MACF"] with payload[%d] \n",
			ETHERP_TO_MACF(&pdmif->InterfaceId), filled_len);
	}
	cac_cap->n_radio = cac_capable_radio;

	WBD_DEBUG("Total CAC capable radio [%d] \n", cac_capable_radio);
	*payload_len = filled_len + 3; /* country(2 byte) + number of radio(1 byte) */
end:
	WBD_EXIT();
}

/* As of now getting driver capability just to check whether
 * driver supports 3+1 BGDFS feature or not. Hence not storing
 * capability for future use.
 */
static int
wbd_slave_get_driver_capability(char *ifname, wbd_ifr_item_t *ifr_vndr_info)
{
	int ret = WBDE_OK;
#ifndef MULTIAP_PLUGFEST
	char driver_capability[WBD_MAX_BUF_8192];
	char *str = NULL;
	char *token = " ";

	WBD_ENTER();

	WBD_ASSERT_ARG(ifname, WBDE_INV_ARG);
	WBD_ASSERT_ARG(ifr_vndr_info, WBDE_INV_ARG);
	memset(driver_capability, 0, sizeof(driver_capability));

	ret = blanket_get_driver_capability(ifname, driver_capability, sizeof(driver_capability));
	WBD_ASSERT();

	str = strtok(driver_capability, token);
	/* since this interface is 5G, Continous CAC is by default enabled */
	ifr_vndr_info->cac_method_flag = WBD_CAC_METHOD_CONTINOUS_CAC;

	while (str != NULL) {
		if (!strncmp(str, "bgdfs", strlen("bgdfs"))) {
			ifr_vndr_info->cac_method_flag =
				WBD_CAC_METHOD_MIMO_DIMENSION_REDUCED;
			break;
		}

		str = strtok(NULL, token);
	}
	WBD_INFO("ifname[%s] support cac method [%x] \n", ifname, ifr_vndr_info->cac_method_flag);
end:
	WBD_EXIT();
#else
	ifr_vndr_info->cac_method_flag = WBD_CAC_METHOD_CONTINOUS_CAC;
	WBD_INFO("ifname[%s] support cac method [%x] \n", ifname, ifr_vndr_info->cac_method_flag);
#endif /* MULTIAP_PLUGFEST */
	return ret;
}

static void
wbd_slave_prepare_radio_cac_capability(i5_dm_interface_type *pdmif, uint8 **pmem,
	uint16 *payload_len)
{
	ieee1905_ifr_cac_info_t *radio_info = (ieee1905_ifr_cac_info_t*)*pmem;
	uint8 *pbuf, *pstart, *plocal_buf;
	int ret = WBDE_OK;
	uint32 is_edcrs_eu = 0;
	uint16 len = 0;

	WBD_ENTER();

	pstart = pbuf = *pmem;

	if (!pdmif) {
		WBD_ERROR(" No valid interface exist for this event, skip \n");
		goto end;
	}

	/* CAC capability TLV information per radio:
	 * ------------------------------------------------------------------------------------
	 * | MAC(6 Byte) | Number of types of CAC (1 bytes) | variable info * number of types |
	 * -----------------------------------------------------------------------------------
	 *
	 * Number of types of CAC - combinations of CAC(3+1/Full Time CAC)
	 *			and number of seconds required to complete CAC
	 *
	 * variable info for CAC Radio:
	 *	CAC mode support - 1 Byte (3+1 BGDFS/Full time CAC)
	 *	Number of seconds required to complete CAC - 3 Bytes
	 *	Number of classes supported for Given mode - 1 Byte
	 *	variable :
	 *	---------------------------------------------------------
	 *	|operating class | number of channels | list of channels |
	 *	---------------------------------------------------------
	 */

	/* copy mac address */
	memcpy(&radio_info->mac, (uchar*)pdmif->InterfaceId, ETHER_ADDR_LEN);

	/* Default CAC mode types = 1, for duration of 60 seconds for all DFS radar channels */
	radio_info->n_cac_types = 1;

	blanket_get_is_edcrs_eu(pdmif->ifname, &is_edcrs_eu);

	if (is_edcrs_eu) {
		/* overwrite CAC mode types for EDCRS
		 * need 2 different CAC duration:
		 * - one for DFS radar channel - 60 sec
		 * - another one for weather radar channel - 600 sec
		 */
		radio_info->n_cac_types = 2;
	}

	pbuf += OFFSETOF(ieee1905_ifr_cac_info_t, mode_info);

	/* Maintain local buffer and payload len to hold cac mode info
	 * per radio. Memcpy local buffer at finish to caller's buffer
	 */
	plocal_buf = (uint8*)wbd_malloc(WBD_MAX_BUF_512, &ret);

	if (radio_info->n_cac_types == 1) {
		/* single cac capability list is required */
		wbd_slave_prepare_non_edcrs_cac_capability_radar_info(pdmif, plocal_buf, &len);
		if (len > WBD_MAX_BUF_512) {
			WBD_ERROR("Buffer overflowed while preparing CAC capability, Debug..\n");
			goto end;
		}
	} else {
		/* need two cac capability info:
		 * - DFS radar channels for 60 seconds
		 * - DFS weather radar channels for 600 seconds
		 */
		wbd_slave_prepare_edcrs_cac_capability_radar_info(pdmif, plocal_buf, &len);
		plocal_buf += len;
		wbd_slave_prepare_edcrs_cac_capability_weather_info(pdmif, plocal_buf, &len);
		if (len > WBD_MAX_BUF_512) {
			WBD_ERROR("Buffer overflowed while preparing CAC capability, Debug..\n");
			goto end;
		}
	}
	memcpy(pbuf, plocal_buf, len);
	pbuf += len;
	*payload_len += pbuf - pstart;

end:
	WBD_DEBUG("radio["MACF"] CAC INFO payload [%d] pbuf[%p] pstart[%p] \n",
		ETHERP_TO_MACF(&pdmif->InterfaceId), *payload_len, pbuf, pstart);

	if (plocal_buf) {
		free(plocal_buf);
	}
	WBD_EXIT();
}

/* List of all radar channels for cac time 60 seconds for NON-EDCRS compliant countries */
static int
wbd_slave_prepare_non_edcrs_cac_capability_radar_info(i5_dm_interface_type *pdmif,
	uint8 *pbuf, uint16 *payload_len)
{
	ieee1905_radio_caps_type *RadioCaps = &pdmif->ApCaps.RadioCaps;
	int rcaps_rc_count = RadioCaps->List ? RadioCaps->List[0] : 0;
	const i5_dm_rc_chan_map_type *rc_chan_map = i5DmGetRegClassChannelMap();
	ieee1905_cac_mode_info_t *mode_info = NULL;
	ieee1905_rc_chan_list_t *list = NULL;
	wbd_ifr_item_t *ifr_vndr_data = NULL;
	int ret = WBDE_OK;
	uint16 bytes_to_rd;
	uint16 bytes_rd = 0;
	uint16 i = 0;
	uint8 *ptr = RadioCaps->List;
	uint8 n_cac_support_opclass = 0;
	uint8 *pstart = NULL;
	uint8 seconds[3] = {0};

	WBD_ENTER();

	bytes_to_rd = RadioCaps->Len;
	ptr++; /* skip number of opclass info */

	pstart = pbuf;

	if (!pdmif) {
		WBD_ERROR(" No valid interface exist for this event, skip \n");
		goto end;
	}

	ifr_vndr_data = (wbd_ifr_item_t*)pdmif->vndr_data;
	if (!ifr_vndr_data) {
		WBD_ERROR(" Invalid ifr vendor data plz Debug, exiting ..\n");
		goto end;
	}

	wbd_slave_get_driver_capability(pdmif->ifname, ifr_vndr_data);
	mode_info = (ieee1905_cac_mode_info_t*)pbuf;

	/* fill mode, seconds related CAC info for interface */
	mode_info->mode =
		(ifr_vndr_data->cac_method_flag == WBD_CAC_METHOD_MIMO_DIMENSION_REDUCED) ?
		MAP_CAC_METHOD_MIMO_DIMENSION_REDUCED_CAC :
		MAP_CAC_METHOD_CONTINOUS_CAC;

	/* Number of seconds required to complete CAC : 3 bytes.
	 * irrespective of dfs radar channel or weather radar channel
	 * CAC time is 60 seconds for countries other than "EDCRS".
	 *
	 * Hence single type of CAC is sufficient to fill cac capability for
	 * this radio.
	 */
	ifr_vndr_data->seconds = 60;

	memcpy((uint8*)&seconds, (uint8*)&(ifr_vndr_data->seconds), 3);
	WBD_SWAP(seconds[0], seconds[2], uint8);

	memcpy(&mode_info->seconds, (uint8*)&seconds, 3);

	pbuf += OFFSETOF(ieee1905_cac_mode_info_t, list);
	/* add rclass if chan info for supported channels becomes non prefered. */
	for (i = 0; (i < rcaps_rc_count) && (bytes_rd < bytes_to_rd); i++) {
		radio_cap_sub_info_t *radio_sub_info = NULL;
		uint bitmap = 0;
		uint16 chan_bitmap[11] = {0x00};
		uint8 chan_cnt = 0;
		uint8 rclass_cnt = 0;
		uint8 index = 0; /* for rclass_chan_list */
		uint8 chan_index = 0; /* for chan list */
		uint8 chan = 0;
		bool add_rclass = FALSE; /* add opclass to cac capability list */

		radio_sub_info = (radio_cap_sub_info_t *)ptr;

		list = (ieee1905_rc_chan_list_t*)pbuf;
		/* consider only rclass supported by agent's interface */
		for (rclass_cnt = 0; rclass_cnt < REGCLASS_MAX_COUNT; rclass_cnt++) {
			if (rc_chan_map[rclass_cnt].regclass == radio_sub_info->regclass) {
				break;
			}
		}
		if (rclass_cnt == REGCLASS_MAX_COUNT) {
			WBD_ERROR(" Invalid RCLASS NOT expected, bail out ...\n");
			goto end;
		}

		/* exclude list of channels present in list from radio caps,
		 * these channels are being marked as non operable
		 */
		for (chan_cnt = 0; chan_cnt < radio_sub_info->n_chan; chan_cnt++) {
			setbit(chan_bitmap, radio_sub_info->list_chan[chan_cnt]);
		}

		for (chan_cnt = 0; chan_cnt < rc_chan_map[rclass_cnt].count; chan_cnt++) {
			/* few channels in radio capabilities marked as non operable, exclude them
			 * while prreparing channel preference for corresponding operating class
			 */
			bitmap = 0;
			chan = rc_chan_map[rclass_cnt].channel[chan_cnt];
			if (isset(chan_bitmap, chan)) {
				/* skip this channel */
				continue;
			}
			if ((radio_sub_info->regclass == 128) ||
				(radio_sub_info->regclass == 129) ||
				(radio_sub_info->regclass == 130)) {

				/* No valid chan info for center channels for 80/80+80/
				 * 160 MHZ.
				 * Utilize radio capability operable center channels
				 * to update cac capability
				 */
				if ((chan == 58) || (chan == 114) || (chan == 122)) {
					list[index].chan[chan_index++] = chan;
					list[index].n_chan += 1;
				} else {
					continue;
				}
			} else {
				/* if channel is not radar, skip */
				blanket_get_chan_info(pdmif->ifname, chan, &bitmap);

				if (!(bitmap & WL_CHAN_RADAR)) {
					continue;
				}
				list[index].chan[chan_index++] = chan;
				list[index].n_chan += 1;
			}
			add_rclass = TRUE;
		}
		if (add_rclass) {
			list[index].opclass = radio_sub_info->regclass;
			n_cac_support_opclass++;
			/* update total payload_len to be send to caller along with filled buffer */
			pbuf += sizeof(ieee1905_rc_chan_list_t) + list[index].n_chan;
			index++;
		}
		/* read next oplcass from radio capability and update bytes_rd value */
		ptr += sizeof(radio_cap_sub_info_t) + radio_sub_info->n_chan;
		bytes_rd += sizeof(radio_cap_sub_info_t) + radio_sub_info->n_chan;
	}
	*payload_len = pbuf - pstart;
	mode_info->n_opclass = n_cac_support_opclass;
end:
	WBD_EXIT();
	return ret;
}

/* List of weather radar channels for cac time 600 seconds for edcrs compliant countries */
static int
wbd_slave_prepare_edcrs_cac_capability_weather_info(i5_dm_interface_type *pdmif,
	uint8 *pbuf, uint16 *payload_len)
{
	ieee1905_radio_caps_type *RadioCaps = &pdmif->ApCaps.RadioCaps;
	int rcaps_rc_count = RadioCaps->List ? RadioCaps->List[0] : 0;
	const i5_dm_rc_chan_map_type *rc_chan_map = i5DmGetRegClassChannelMap();
	ieee1905_cac_mode_info_t *mode_info = NULL;
	wbd_ifr_item_t *ifr_vndr_data = NULL;
	ieee1905_rc_chan_list_t *list = NULL;
	int ret = WBDE_OK;
	uint16 bytes_to_rd;
	uint16 bytes_rd = 0;
	uint16 i = 0;
	uint8 *ptr = RadioCaps->List;
	uint8 n_cac_support_opclass = 0;
	uint8 *pstart = NULL;
	uint8 seconds[3] = {0};

	WBD_ENTER();

	bytes_to_rd = RadioCaps->Len;
	ptr++; /* skip number of opclass info */

	pstart = pbuf;

	if (!pdmif) {
		WBD_ERROR(" No valid interface exist for this event, skip \n");
		goto end;
	}

	ifr_vndr_data = (wbd_ifr_item_t*)pdmif->vndr_data;
	if (!ifr_vndr_data) {
		WBD_ERROR(" Invalid ifr vendor data plz Debug, exiting ..\n");
		goto end;
	}

	wbd_slave_get_driver_capability(pdmif->ifname, ifr_vndr_data);
	mode_info = (ieee1905_cac_mode_info_t*)pbuf;
	/* fill mode, seconds, total opclass support for interface */
	mode_info->mode =
		(ifr_vndr_data->cac_method_flag == WBD_CAC_METHOD_MIMO_DIMENSION_REDUCED) ?
		MAP_CAC_METHOD_MIMO_DIMENSION_REDUCED_CAC :
		MAP_CAC_METHOD_CONTINOUS_CAC;

	/* number of seconds required to complete CAC : 3 bytes */
	ifr_vndr_data->seconds = 600; /* TODO: add new IOVAR to get CAC time */

	memcpy((uint8*)&seconds, (uint8*)&(ifr_vndr_data->seconds), 3);
	WBD_SWAP(seconds[0], seconds[2], uint8);

	memcpy(&mode_info->seconds, (uint8*)&seconds, 3);
	pbuf += OFFSETOF(ieee1905_cac_mode_info_t, list);

	/* add rclass if chan info for supported channels becomes non prefered. */
	for (i = 0; (i < rcaps_rc_count) && (bytes_rd < bytes_to_rd); i++) {
		radio_cap_sub_info_t *radio_sub_info = NULL;
		uint bitmap = 0;
		uint8 chan_cnt = 0;
		uint8 rclass_cnt = 0;
		uint16 chan_bitmap[11] = {0x00};
		uint8 index = 0; /* for rclass_chan_list */
		uint8 chan_index = 0; /* for chan list */
		uint8 chan = 0;
		bool add_rclass = FALSE; /* add opclass to cac capability list */

		radio_sub_info = (radio_cap_sub_info_t *)ptr;

		list = (ieee1905_rc_chan_list_t*)pbuf;
		/* consider only rclass supported by agent's interface */
		for (rclass_cnt = 0; rclass_cnt < REGCLASS_MAX_COUNT; rclass_cnt++) {
			if (rc_chan_map[rclass_cnt].regclass == radio_sub_info->regclass) {
				break;
			}
		}
		if (rclass_cnt == REGCLASS_MAX_COUNT) {
			WBD_ERROR(" Invalid RCLASS NOT expected, bail out ...\n");
			goto end;
		}

		/* exclude list of channels present in list from radio caps,
		 * these channels are being marked as non operable
		 */
		for (chan_cnt = 0; chan_cnt < radio_sub_info->n_chan; chan_cnt++) {
			setbit(chan_bitmap, radio_sub_info->list_chan[chan_cnt]);
		}

		for (chan_cnt = 0; chan_cnt < rc_chan_map[rclass_cnt].count; chan_cnt++) {
			/* few channels in radio capabilities marked as non operable, exclude them
			 * while prreparing channel preference for corresponding operating class
			 */
			bitmap = 0;
			chan = rc_chan_map[rclass_cnt].channel[chan_cnt];
			if (isset(chan_bitmap, chan)) {
				/* skip this channel */
				continue;
			}
			/* if channel is not radar, skip */
			blanket_get_chan_info(pdmif->ifname, chan, &bitmap);

			if (!(bitmap & WL_CHAN_RADAR)) {
				continue;
			}
			/* include DFS weather channels in CAC capability for
			 * radio
			 */
			if ((chan == 106) || (chan == 114) || (chan == 120) ||
				(chan == 122) || (chan == 124) || (chan == 128)) {

				/* update tmp buffer with chan and rclass */
				list[index].opclass =
					radio_sub_info->regclass;
				/* add this channel to list */
				list[index].chan[chan_index++] = chan;

				list[index].n_chan += 1;
				add_rclass = TRUE;
			}
		}
		if (add_rclass) {
			list[index].opclass = radio_sub_info->regclass;
			n_cac_support_opclass++;
			/* update total payload_len to be send to caller along with filled buffer */
			pbuf += sizeof(ieee1905_rc_chan_list_t) + list[index].n_chan;

			index++;
		}
		/* read next oplcass from radio capability and update bytes_rd value */
		ptr += sizeof(radio_cap_sub_info_t) + radio_sub_info->n_chan;
		bytes_rd += sizeof(radio_cap_sub_info_t) + radio_sub_info->n_chan;
	}
	*payload_len = pbuf - pstart;
	mode_info->n_opclass = n_cac_support_opclass;
end:
	WBD_EXIT();
	return ret;
}

/* List of radar channels for cac time of 60 seconds for edcrs compliant countries */
static int
wbd_slave_prepare_edcrs_cac_capability_radar_info(i5_dm_interface_type *pdmif,
	uint8 *pbuf, uint16 *payload_len)
{
	ieee1905_radio_caps_type *RadioCaps = &pdmif->ApCaps.RadioCaps;
	int rcaps_rc_count = RadioCaps->List ? RadioCaps->List[0] : 0;
	const i5_dm_rc_chan_map_type *rc_chan_map = i5DmGetRegClassChannelMap();
	ieee1905_cac_mode_info_t *mode_info = NULL;
	ieee1905_rc_chan_list_t *list = NULL;
	wbd_ifr_item_t *ifr_vndr_data = NULL;
	int ret = WBDE_OK;
	uint16 bytes_to_rd;
	uint16 bytes_rd = 0;
	uint16 i = 0;
	uint8 *ptr = RadioCaps->List;
	uint8 n_cac_support_opclass = 0;
	uint8 *pstart = NULL;
	uint8 seconds[3] = {0};

	WBD_ENTER();

	bytes_to_rd = RadioCaps->Len;
	ptr++; /* skip number of opclass info */

	pstart = pbuf;

	if (!pdmif) {
		WBD_ERROR(" No valid interface exist for this event, skip \n");
		goto end;
	}

	ifr_vndr_data = (wbd_ifr_item_t*)pdmif->vndr_data;
	if (!ifr_vndr_data) {
		WBD_ERROR(" Invalid ifr vendor data plz Debug, exiting ..\n");
		goto end;
	}

	wbd_slave_get_driver_capability(pdmif->ifname, ifr_vndr_data);
	mode_info = (ieee1905_cac_mode_info_t*)pbuf;
	/* fill mode, seconds, total opclass support for interface */
	mode_info->mode =
		(ifr_vndr_data->cac_method_flag == WBD_CAC_METHOD_MIMO_DIMENSION_REDUCED) ?
		MAP_CAC_METHOD_MIMO_DIMENSION_REDUCED_CAC :
		MAP_CAC_METHOD_CONTINOUS_CAC;

	/* number of seconds required to complete CAC : 3 bytes */
	ifr_vndr_data->seconds = 600; /* TODO: add new IOVAR to get CAC time */

	memcpy((uint8*)&seconds, (uint8*)&(ifr_vndr_data->seconds), 3);
	WBD_SWAP(seconds[0], seconds[2], uint8);

	memcpy(&mode_info->seconds, (uint8*)&seconds, 3);
	pbuf += OFFSETOF(ieee1905_cac_mode_info_t, list);

	/* add rclass if chan info for supported channels becomes non prefered. */
	for (i = 0; (i < rcaps_rc_count) && (bytes_rd < bytes_to_rd); i++) {
		radio_cap_sub_info_t *radio_sub_info = NULL;
		uint bitmap = 0;
		uint8 chan_cnt = 0;
		uint8 rclass_cnt = 0;
		uint16 chan_bitmap[11] = {0x00};
		uint8 index = 0; /* for rclass_chan_list */
		uint8 chan_index = 0; /* for chan list */
		uint8 chan = 0;
		bool add_rclass = FALSE; /* add opclass to cac capability list */

		radio_sub_info = (radio_cap_sub_info_t *)ptr;

		list = (ieee1905_rc_chan_list_t*)pbuf;
		/* consider only rclass supported by agent's interface */
		for (rclass_cnt = 0; rclass_cnt < REGCLASS_MAX_COUNT; rclass_cnt++) {
			if (rc_chan_map[rclass_cnt].regclass == radio_sub_info->regclass) {
				break;
			}
		}
		if (rclass_cnt == REGCLASS_MAX_COUNT) {
			WBD_ERROR(" Invalid RCLASS NOT expected, bail out ...\n");
			goto end;
		}

		/* exclude list of channels present in list from radio caps,
		 * these channels are being marked as non operable
		 */
		for (chan_cnt = 0; chan_cnt < radio_sub_info->n_chan; chan_cnt++) {
			setbit(chan_bitmap, radio_sub_info->list_chan[chan_cnt]);
		}

		for (chan_cnt = 0; chan_cnt < rc_chan_map[rclass_cnt].count; chan_cnt++) {
			/* few channels in radio capabilities marked as non operable, exclude them
			 * while prreparing channel preference for corresponding operating class
			 */
			bitmap = 0;
			chan = rc_chan_map[rclass_cnt].channel[chan_cnt];
			if (isset(chan_bitmap, chan)) {
				/* skip this channel */
				continue;
			}
			/* if channel is not radar, skip */
			blanket_get_chan_info(pdmif->ifname, chan, &bitmap);

			if (!(bitmap & WL_CHAN_RADAR)) {
				continue;
			}
			/* Exclude DFS weather channels for opclass */
			if ((chan == 106) || (chan == 114) || (chan == 120) ||
				(chan == 122) || (chan == 124) || (chan == 128)) {
				continue;
			}
			/* update tmp buffer with chan and rclass */
			list[index].opclass =
				radio_sub_info->regclass;
			/* add this channel to list */
			list[index].chan[chan_index++] = chan;

			list[index].n_chan += 1;
			add_rclass = TRUE;

		}
		if (add_rclass) {
			list[index].opclass = radio_sub_info->regclass;
			n_cac_support_opclass++;
			/* update total payload_len to be send to caller along with filled buffer */
			pbuf += sizeof(ieee1905_rc_chan_list_t) + list[index].n_chan;

			index++;
		}
		/* read next oplcass from radio capability and update bytes_rd value */
		ptr += sizeof(radio_cap_sub_info_t) + radio_sub_info->n_chan;
		bytes_rd += sizeof(radio_cap_sub_info_t) + radio_sub_info->n_chan;
	}
	*payload_len = pbuf - pstart;
	mode_info->n_opclass = n_cac_support_opclass;
end:
	WBD_EXIT();
	return ret;
}
/* Prepare CAC completion TLV payload for all Radio received CAC request from
 * controller.
 */
void
wbd_slave_prepare_cac_complete_tlv_payload(uint8 **pbuf, uint32 *payload_len)
{
	WBD_ENTER();

	/* Logic sends only one list at a time.
	 * It may be list of radios who declined cac start request, OR
	 * It may be list of radios who accepted cac start request and
	 * after serving cac request preparing the status with
	 * success/radar etc.
	 */
	wbd_slave_prepare_list_of_radios_decline_cac_rqst(pbuf, payload_len);
	if (*payload_len != 0) {
		/* few radios decline cac start request, send this list.
		 * for other radios if perfoming cac request prepare
		 * cac complete payload on receiving event WLC_E_AP_CHAN_CHANGE
		 * OR WLC_E_CAC_STATE_CHANGE.
		 */
		WBD_DEBUG("CAC complete TLV with radios declined cac start rqst\n");
		goto end;
	}

	wbd_slave_prepare_list_of_radios_serving_cac_rqst(pbuf, payload_len);
	WBD_DEBUG("CAC complete TLV with radios served cac start rqst \n");
end:
	WBD_INFO("CAC complete: payload_len[%d] \n", *payload_len);
}

/* Prepare cac completion report for all radio who declined the cac start request */
static int
wbd_slave_prepare_list_of_radios_decline_cac_rqst(uint8 **pbuf, uint32 *payload_len)
{
	ieee1905_cac_completion_list_t *cac_msg = NULL;
	i5_dm_device_type *sdev = NULL;
	i5_dm_interface_type *sifr = NULL;
	wbd_ifr_item_t *ifr_vndr_data;
	int ret = WBDE_OK;
	uint8 index = 0;
	uint16 bytes_copied = 0;

	WBD_ENTER();

	WBD_ASSERT_ARG(*pbuf, WBDE_INV_ARG);

	cac_msg = (ieee1905_cac_completion_list_t*)(*pbuf);

	WBD_SAFE_GET_I5_SELF_DEVICE(sdev, &ret);
	foreach_i5glist_item(sifr, i5_dm_interface_type, sdev->interface_list) {
		/*
		 * Firmware event WLC_E_DFS_AP_MOVE triggers this call everytime
		 * CAC finishes on radio.
		 *
		 * Wait for all 5G radio(with status: MAP_CAC_RQST_RUNNING) finishes
		 * DFS CAC.
		 *
		 * Traverse all 5G capable radios and prepare CAC status for each
		 * radio, reset each radio's cac_status to MAP_CAC_RQST_COMPLETE.
		 *
		 */
		ifr_vndr_data = (wbd_ifr_item_t*)sifr->vndr_data;
		if (!ifr_vndr_data) {
			WBD_ERROR(" Invalid ifr vendor data plz Debug, exiting ..\n");
			continue;
		}

		if (!(ifr_vndr_data->cac_rqst_status & MAP_CAC_RQST_METHOD_NOT_SUPPORT)) {
			continue;
		}

		WBD_INFO("fill CAC complete details for ["MACF"] cac_rqst_status[%d] "
			"opclass[%d] chan[%d] \n", ETHERP_TO_MACF(&sifr->InterfaceId),
			ifr_vndr_data->cac_rqst_status, ifr_vndr_data->cac_rqst_opclass,
			ifr_vndr_data->cac_rqst_chan);

		/* assuming all radios completed CAC */
		memcpy(&cac_msg->params[index].mac, sifr->InterfaceId, ETHER_ADDR_LEN);

		cac_msg->params[index].opclass = ifr_vndr_data->cac_rqst_opclass;
		cac_msg->params[index].chan = ifr_vndr_data->cac_rqst_chan;
		cac_msg->params[index].status = 2; /* assume success */

		/* assume no radar found on any sub channel.
		 * TODO: For radar case, consider WLC_E_RADAR_EVENT and use corresponding
		 * channels to populate CAC completion report
		 */
		cac_msg->params[index].n_opclass_chan = 0;
		cac_msg->params[index].list[0].op_class = 0;
		cac_msg->params[index].list[0].chan = 0;

		/* reset cac_rqst_status */
		ifr_vndr_data->cac_rqst_status = 2; /* for CAC method not support on interface */

		bytes_copied += 12; // hardcoded
		/* use:
		 * bytes_copied += sizeof(cac_complet_info) +
		 *	(cac_msg->cac_complete_info[index].n_op_class *
		 *				(sizeof(operating_chan_report) -1));
		 */

		cac_msg->count++;
		index++;
		/* reset cac method info */
		ifr_vndr_data->cac_rqst_status &= ~MAP_CAC_RQST_METHOD_NOT_SUPPORT;
	}
	*payload_len = bytes_copied + 1; /* 1 byte for count */

	WBD_INFO("CAC complete for decline rqst: payload_len[%d] \n", *payload_len);
	prhex("DEBUG_PKT -> cac completion TLV paload: \n", (uchar*)cac_msg, (bytes_copied+1));
end:
	WBD_EXIT();
	return WBDE_OK;
}

/* Prepare cac completion report for radio who served CAC start request */
static int
wbd_slave_prepare_list_of_radios_serving_cac_rqst(uint8 **pbuf, uint32 *payload_len)
{
	ieee1905_cac_completion_list_t *cac_msg = NULL;
	i5_dm_device_type *sdev = NULL;
	i5_dm_interface_type *sifr = NULL;
	wbd_ifr_item_t *ifr_vndr_data;
	int ret = WBDE_OK;
	uint8 index = 0;
	uint16 bytes_copied = 0;

	WBD_ENTER();

	WBD_ASSERT_ARG(*pbuf, WBDE_INV_ARG);

	cac_msg = (ieee1905_cac_completion_list_t*)(*pbuf);

	WBD_SAFE_GET_I5_SELF_DEVICE(sdev, &ret);
	foreach_i5glist_item(sifr, i5_dm_interface_type, sdev->interface_list) {
		/*
		 * Firmware event WLC_E_DFS_AP_MOVE triggers this call everytime
		 * CAC finishes on radio.
		 *
		 * Wait for all 5G radio(with status: MAP_CAC_RQST_RUNNING) finishes
		 * DFS CAC.
		 *
		 * Traverse all 5G capable radios and prepare CAC status for each
		 * radio, reset each radio's cac_status to MAP_CAC_RQST_COMPLETE.
		 *
		 */
		ifr_vndr_data = (wbd_ifr_item_t*)sifr->vndr_data;
		if (!ifr_vndr_data) {
			WBD_ERROR(" Invalid ifr vendor data plz Debug, exiting ..\n");
			continue;
		}

		if (!WBD_BAND_TYPE_LAN_5G(sifr->band) ||
			(!(ifr_vndr_data->cac_rqst_status & (MAP_CAC_RQST_RCVD)))) {
			continue;
		}
		if (ifr_vndr_data->cac_rqst_status & MAP_CAC_RQST_RUNNING) {
			/* wait for all radios to complete CAC
			 * TODO: what shud we do in RADAR during CAC ....?
			 */
			break;
		}

		WBD_INFO("fill CAC complete details for ["MACF"] cac_rqst_status[%d] "
			"opclass[%d] chan[%d] \n", ETHERP_TO_MACF(&sifr->InterfaceId),
			ifr_vndr_data->cac_rqst_status, ifr_vndr_data->cac_rqst_opclass,
			ifr_vndr_data->cac_rqst_chan);

		/* assuming all radios completed CAC */
		memcpy(&cac_msg->params[index].mac, sifr->InterfaceId, ETHER_ADDR_LEN);

		cac_msg->params[index].opclass = ifr_vndr_data->cac_rqst_opclass;
		cac_msg->params[index].chan = ifr_vndr_data->cac_rqst_chan;
		cac_msg->params[index].status = 0; /* assume success */

		/* assume no radar found on any sub channel.
		 * TODO: For radar case, consider WLC_E_RADAR_EVENT and use corresponding
		 * channels to populate CAC completion report
		 */
		cac_msg->params[index].n_opclass_chan = 0;
		cac_msg->params[index].list[0].op_class = 0;
		cac_msg->params[index].list[0].chan = 0;

		/* reset cac_rqst_status */
		ifr_vndr_data->cac_rqst_status = 0;

		bytes_copied += 12; // hardcoded
		/* use:
		 * bytes_copied += sizeof(cac_complet_info) +
		 *	(cac_msg->cac_complete_info[index].n_op_class *
		 *				(sizeof(operating_chan_report) -1));
		 */

		cac_msg->count++;
		index++;
	}
	*payload_len = bytes_copied + 1;

	WBD_INFO("CAC complete: payload_len[%d] \n", *payload_len);
	prhex("DEBUG_PKT -> cac completion TLV paload: \n", (uchar*)cac_msg, (bytes_copied+1));
end:
	WBD_EXIT();
	return WBDE_OK;
}

/* Prepare CAC status payload for all Radio received CAC request from
 * controller.
 */
void
wbd_slave_prepare_cac_status_payload(uint8 **pbuf, uint16 *payload_len)
{
	i5_dm_device_type *sdev = NULL;
	i5_dm_interface_type *sifr = NULL;
	int ret = WBDE_OK;
	uint8 *ptr = NULL;
	uint16 filled_len = 0;
	uint16 bytes_filled = 0;

	WBD_ENTER();

	ptr = *pbuf;

	WBD_SAFE_GET_I5_SELF_DEVICE(sdev, &ret);

	/* prepare available channels in agent */
	foreach_i5glist_item(sifr, i5_dm_interface_type, sdev->interface_list) {
		filled_len = 0;
		if (!i5DmIsInterfaceWireless(sifr->MediaType) ||
			!WBD_BAND_TYPE_LAN_5G(sifr->band)) {
			continue;
		}
		/* prepare list of available channels for all interfaces in agent */
		wbd_slave_prepare_active_list(sifr, &ptr, &filled_len);
		ptr += filled_len;
		bytes_filled += filled_len;
	}
	WBD_DEBUG("bytes_filled[%d] for available channels in cac status \n", bytes_filled);

	/* prepare radar detected channels in agent */
	foreach_i5glist_item(sifr, i5_dm_interface_type, sdev->interface_list) {
		filled_len = 0;
		if (!i5DmIsInterfaceWireless(sifr->MediaType) ||
			!WBD_BAND_TYPE_LAN_5G(sifr->band)) {
			continue;
		}
		/* prepare list of available channels for all interfaces in agent */
		wbd_slave_prepare_radar_detected_chan_list(sifr, &ptr, &filled_len);
		ptr += filled_len;
		bytes_filled += filled_len;
	}
	WBD_DEBUG("bytes_filled[%d] after radar detected channels in cac status \n", bytes_filled);

	/* prepare ongoing CAC channels in agent */
	foreach_i5glist_item(sifr, i5_dm_interface_type, sdev->interface_list) {
		filled_len = 0;
		if (!i5DmIsInterfaceWireless(sifr->MediaType) ||
			!WBD_BAND_TYPE_LAN_5G(sifr->band)) {
			continue;
		}
		/* prepare list of available channels for all interfaces in agent */
		wbd_slave_prepare_ongoing_cac_chan_list(sifr, &ptr, &filled_len);
		ptr += filled_len;
		bytes_filled += filled_len;
	}
	WBD_DEBUG("bytes_filled[%d] after ongoing CAC channels in cac status \n", bytes_filled);

	*payload_len = bytes_filled;
end:
	WBD_INFO("CAC status: payload_len[%d] \n", *payload_len);
	WBD_EXIT();
}

/* Create active channels(done CAC) list */
static void
wbd_slave_prepare_active_list(i5_dm_interface_type *pdmif, uint8 **pbuf, uint16 *payload_len)
{
	ieee1905_available_opclass_chan_list_t *active_list = NULL;
	ieee1905_opclass_chan_info_t *chan_info = NULL;
	i5_dm_rc_chan_map_type *g_rc_chan_map = NULL;
	uint32 g_rc_chan_map_count = 0;
	int i = 0, j, k;
	uint bitmap = 0;
	uint8 non_dfs_chan_bitmap[21] = {0};
	uint8 dfs_chan_bitmap[21] = {0};
	uint8 index = 0;
	uint8 opclass = 0;
	uint8 chan = 0;

	WBD_ENTER();

	/* Plugfest support for 20, 40 MHZ.
	 * TODO: for 80/80+80/160 :
	 *	- Read chanspec list
	 *	- get chan info from sub bands of chanspec.
	 *	- prepare list with center channels
	 */
	g_rc_chan_map = i5DmGetRCChannelMap(&g_rc_chan_map_count);

	active_list = (ieee1905_available_opclass_chan_list_t*)*pbuf;
	chan_info = (ieee1905_opclass_chan_info_t*)&active_list->info;

	/* only considering 20 and 40 BW
	 * TODO: Read chanspec list for 80/80+80/160
	 */
	for (i = 0; i < g_rc_chan_map_count; i++) {
		memset(non_dfs_chan_bitmap, 0, sizeof(non_dfs_chan_bitmap));
		memset(dfs_chan_bitmap, 0, sizeof(dfs_chan_bitmap));

		opclass = g_rc_chan_map[i].regclass;

		if (!wbd_slave_opclass_valid_for_interface(pdmif, opclass)) {
			continue;
		}

		WBD_DEBUG("opclass[%d] valid for [%s] \n", opclass, pdmif->ifname);

		for (j = 0; j < g_rc_chan_map[i].count; j++) {
			bitmap = 0;
			chan = g_rc_chan_map[i].channel[j];
			if (!wbd_slave_chan_valid_for_interface(pdmif, opclass, chan)) {
				WBD_DEBUG("opclass[%d] valid but chan[%d] non operable for [%s] \n",
					opclass, chan, pdmif->ifname);
				continue;
			}

			WBD_DEBUG("opclass[%d] chan[%d] is valid for [%s] \n", opclass, chan,
				pdmif->ifname);
			/* get corresponding chan list and get bitmap for them */
			blanket_get_chan_info(pdmif->ifname, chan, &bitmap);

			if (!(bitmap & WL_CHAN_RADAR)) {
				if (!isset(non_dfs_chan_bitmap, chan)) {
					setbit(non_dfs_chan_bitmap, chan);
				}
			} else {
				if (!(bitmap & WL_CHAN_PASSIVE) &&
					!(bitmap & WL_CHAN_INACTIVE)) {
					if (!isset(dfs_chan_bitmap, chan)) {
						setbit(dfs_chan_bitmap, chan);
					}
				}
			}
		}
		/* first read non dfs chan bitmap followed with non dfs chan bitmap */
		for (k = 0; k < sizeof(non_dfs_chan_bitmap)*8; k++) {
			if (isset(non_dfs_chan_bitmap, k)) {
				chan_info[index].opclass = opclass;
				chan_info[index].chan = k;
				index++;
				//TODO: chan_info[index].duration = {0};
			}
		}

		/* first read non dfs chan bitmap followed with non dfs chan bitmap */
		for (k = 0; k < sizeof(dfs_chan_bitmap)*8; k++) {
			if (isset(dfs_chan_bitmap, k)) {
				chan_info[index].opclass = opclass;
				chan_info[index].chan = k;
				index++;
				//TODO: chan_info[index].duration = {0}; /* read dfs_status ? */
			}
		}
	}
	active_list->n_count = index;

	if (active_list->n_count > 0) {
		*payload_len = (index * sizeof(ieee1905_opclass_chan_info_t)) +
			OFFSETOF(ieee1905_available_opclass_chan_list_t, info);
	} else {
		*payload_len = OFFSETOF(ieee1905_available_opclass_chan_list_t, info);
	}

	WBD_DEBUG("CAC status available chan list : n_count[%d] payload_len[%d] \n",
		active_list->n_count, *payload_len);

	WBD_EXIT();
}

/* Create radar detected chan list */
static void
wbd_slave_prepare_radar_detected_chan_list(i5_dm_interface_type *pdmif, uint8 **pbuf,
	uint16 *payload_len)
{
	ieee1905_radar_opclass_chan_list_t *radar_list = NULL;
	ieee1905_opclass_chan_info_t *chan_info = NULL;
	i5_dm_rc_chan_map_type *g_rc_chan_map = NULL;
	uint32 g_rc_chan_map_count = 0;
	int i = 0, j, k;
	uint bitmap = 0;
	uint8 dfs_chan_bitmap[21] = {0};
	uint8 index = 0;
	uint8 opclass = 0;
	uint8 chan = 0;

	WBD_ENTER();

	/* Plugfest support for 20, 40 MHZ.
	 * TODO: for 80/80+80/160 :
	 *	- Read chanspec list
	 *	- get chan info from sub bands of chanspec.
	 *	- prepare list with center channels
	 */
	g_rc_chan_map = i5DmGetRCChannelMap(&g_rc_chan_map_count);

	radar_list = (ieee1905_radar_opclass_chan_list_t*)*pbuf;
	chan_info = (ieee1905_opclass_chan_info_t*)&radar_list->info;

	/* only considering 20 and 40 BW
	 * TODO: Read chanspec list for 80/80+80/160
	 */
	for (i = 0; i < g_rc_chan_map_count; i++) {
		memset(dfs_chan_bitmap, 0, sizeof(dfs_chan_bitmap));
		opclass = g_rc_chan_map[i].regclass;

		if (!wbd_slave_opclass_valid_for_interface(pdmif, opclass)) {
			continue;
		}

		WBD_DEBUG("opclass[%d] valid for [%s] \n", opclass, pdmif->ifname);

		for (j = 0; j < g_rc_chan_map[i].count; j++) {
			chan = g_rc_chan_map[i].channel[j];
			if (!wbd_slave_chan_valid_for_interface(pdmif, opclass, chan)) {
				WBD_DEBUG("opclass[%d] valid but chan[%d] non operable for [%s] \n",
					opclass, chan, pdmif->ifname);
				continue;
			}

			WBD_DEBUG("opclass[%d] chan[%d] is valid for [%s] \n", opclass, chan,
				pdmif->ifname);
			/* get corresponding chan list and get bitmap for them */
			blanket_get_chan_info(pdmif->ifname, chan, &bitmap);
			if (!(bitmap & WL_CHAN_RADAR)) {
				continue;
			} else {
				if ((bitmap & WL_CHAN_INACTIVE)) {
					if (!isset(dfs_chan_bitmap, chan)) {
						setbit(dfs_chan_bitmap, chan);
					}
				}
			}
		}
		/* read non dfs chan bitmap followed with non dfs chan bitmap */
		for (k = 0; k < sizeof(dfs_chan_bitmap)*8; k++) {
			if (isset(dfs_chan_bitmap, k)) {
				chan_info[index].opclass = opclass;
				chan_info[index].chan = k;
				//chan_info[index].duration = {0}; /* hardcoded, bitmap[4 Bytes] */
				index++;
			}
		}
	}
	radar_list->n_count = index;

	if (radar_list->n_count > 0) {
		*payload_len = (index * sizeof(ieee1905_opclass_chan_info_t)) +
			OFFSETOF(ieee1905_radar_opclass_chan_list_t, info);
	} else {
		*payload_len = OFFSETOF(ieee1905_radar_opclass_chan_list_t, info);
	}

	WBD_DEBUG("CAC status radar chan list : n_count[%d] payload_len[%d] \n",
		radar_list->n_count, *payload_len);

	WBD_EXIT();
}

/* Create ongoing CAC chan list */
static void
wbd_slave_prepare_ongoing_cac_chan_list(i5_dm_interface_type *pdmif, uint8 **pbuf,
	uint16 *payload_len)
{
	ieee1905_ongoing_cac_list_t *cac_info = NULL;
	ieee1905_ongoing_cac_opclass_chan_info_t *chan_info = NULL;
	wl_dfs_status_t dfs_status;
	chanspec_t chanspec;
	uint8 sub_channel;
	uint8 rclass = 0;
	uint8 seconds[3] = {0};
	uint8 index = 0;

	WBD_ENTER();

	memset(&dfs_status, 0, sizeof(dfs_status));

	blanket_get_dfs_status(pdmif->ifname, &dfs_status);

	if (dfs_status.state != WL_DFS_CACSTATE_PREISM_CAC) {
		goto end;
	}

	if (blanket_get_chanspec(pdmif->ifname, &chanspec) != WBDE_OK) {
		goto end;
	}

	blanket_get_rclass(pdmif->ifname, chanspec, &rclass);

	if (rclass == 0) {
		WBD_ERROR("Zero RCLASS for chanspec[%x], Debug..., exit\n",
			dfs_status.chanspec_cleared);
		goto end;
	}
	cac_info = (ieee1905_ongoing_cac_list_t*)*pbuf;
	chan_info = (ieee1905_ongoing_cac_opclass_chan_info_t*)&cac_info->info;

	/* get rclass from chanspec */
	FOREACH_20_SB(chanspec, sub_channel) {
		chan_info[index].opclass = rclass;
		chan_info[index].chan = sub_channel;
		memcpy((uint8*)&seconds, (uint8*)&(dfs_status.duration), 3);
		WBD_SWAP(seconds[0], seconds[2], uint8);

		memcpy(&chan_info[index].duration, (uint8*)&seconds, 3);

		index++;
	}

	cac_info->n_count = index;

	if (cac_info->n_count > 0) {
		*payload_len = (index * sizeof(ieee1905_ongoing_cac_opclass_chan_info_t)) +
			OFFSETOF(ieee1905_ongoing_cac_list_t, info);
	} else {
		*payload_len = OFFSETOF(ieee1905_ongoing_cac_list_t, info);
	}

	WBD_DEBUG("CAC status ongoing cac info : n_count[%d] payload_len[%d] \n",
		cac_info->n_count, *payload_len);
end:
	WBD_EXIT();
}

/* check opclass validity from radio capability of interface */
static bool
wbd_slave_opclass_valid_for_interface(i5_dm_interface_type *pdmif, uint8 opclass)
{

	ieee1905_radio_caps_type *RadioCaps = &pdmif->ApCaps.RadioCaps;
	radio_cap_sub_info_t *radio_sub_info = NULL;
	int rcaps_rc_count = RadioCaps->List ? RadioCaps->List[0] : 0;
	uint8 *ptr = RadioCaps->List;
	uint16 bytes_to_rd;
	uint16 bytes_rd = 0;
	uint16 i = 0;

	WBD_ENTER();

	WBD_DEBUG("total valid opclass[%d] for interface["MACF"] \n", *ptr,
		ETHERP_TO_MACF(pdmif->InterfaceId));

	bytes_to_rd = RadioCaps->Len;
	ptr++; /* skip number of opclass info */

	for (i = 0; (i < rcaps_rc_count) && (bytes_rd < bytes_to_rd); i++) {
		radio_sub_info = (radio_cap_sub_info_t *)ptr;
		if (radio_sub_info->regclass == opclass) {
			return TRUE;
		}
		/* read next oplcass from radio capability and update bytes_rd value */
		ptr += sizeof(radio_cap_sub_info_t) + radio_sub_info->n_chan;
		bytes_rd += sizeof(radio_cap_sub_info_t) + radio_sub_info->n_chan;
	}
	return FALSE;
}

/* check chan validity from radio capability of interface */
static bool
wbd_slave_chan_valid_for_interface(i5_dm_interface_type *pdmif, uint8 opclass, uint8 chan)
{
	ieee1905_radio_caps_type *RadioCaps = &pdmif->ApCaps.RadioCaps;
	radio_cap_sub_info_t *radio_sub_info = NULL;
	int rcaps_rc_count = RadioCaps->List ? RadioCaps->List[0] : 0;
	uint16 bytes_to_rd;
	uint16 bytes_rd = 0;
	uint16 i = 0;
	uint8 chan_cnt = 0;
	uint8 *ptr = RadioCaps->List;

	WBD_ENTER();

	WBD_DEBUG("total valid opclass[%d] for interface["MACF"] \n", *ptr,
		ETHERP_TO_MACF(pdmif->InterfaceId));

	bytes_to_rd = RadioCaps->Len;
	ptr++; /* skip number of opclass info */

	for (i = 0; (i < rcaps_rc_count) && (bytes_rd < bytes_to_rd); i++) {
		radio_sub_info = (radio_cap_sub_info_t *)ptr;
		if (radio_sub_info->regclass != opclass) {
			/* read next oplcass from radio capability and update bytes_rd value */
			ptr += sizeof(radio_cap_sub_info_t) + radio_sub_info->n_chan;
			bytes_rd += sizeof(radio_cap_sub_info_t) + radio_sub_info->n_chan;
			continue;
		}

		for (chan_cnt = 0; chan_cnt < radio_sub_info->n_chan; chan_cnt++) {
			if (radio_sub_info->list_chan[chan_cnt] == chan) {
				/* radio caps contains non operable chan in operable
				 * opclass.
				 */
				return FALSE;
			}
		}
		/* found valid opclas and chan not present in list, means valid chan */
		return TRUE;
	}
	return FALSE;
}
#endif /* MULTIAPR2 */
