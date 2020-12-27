/*
 * WBD daemon (Linux)
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
 * $Id: wbd_master.c 781797 2019-11-29 05:13:50Z $
 */

#include <signal.h>
#include <getopt.h>
#include "wbd.h"
#include "wbd_shared.h"
#include "wbd_ds.h"
#include "wbd_com.h"
#include "wbd_json_utility.h"
#include "wbd_sock_utility.h"
#include "wbd_master_com_hdlr.h"
#include "wbd_master_control.h"
#include "blanket.h"
#include "ieee1905.h"
#include <common_utils.h>

wbd_info_t *g_wbdinfo = NULL;
unsigned int wbd_msglevel = WBD_DEBUG_DEFAULT;
char g_wbd_process_name[WBD_MAX_PROCESS_NAME];

/* Read "wbd_ifnames" NVRAM and get actual ifnames */
extern int wbd_read_actual_ifnames(char *wbd_ifnames1, int len1, bool create);

/* Get Master module info */
wbd_info_t *
wbd_get_ginfo()
{
	return (g_wbdinfo);
}

/* Gets Blanket Master NVRAM settings */
static int
wbd_master_retrieve_blanket_nvram_config(wbd_blanket_master_t *wbd_master)
{
	char *str;
	int ret = WBDE_OK, num = 0;
	uint16 flags = 0;
	WBD_ENTER();

	/* Read bouncing STA CFG */
	str = blanket_nvram_safe_get(WBD_NVRAM_BOUNCE_DETECT);
	if (str) {
		num = sscanf(str, "%d %d %d",
			&wbd_master->bounce_cfg.window, &wbd_master->bounce_cfg.cnt,
			&wbd_master->bounce_cfg.dwell_time);
		if (num != 3) {
			wbd_master->bounce_cfg.window = BSD_BOUNCE_DETECT_WIN;
			wbd_master->bounce_cfg.cnt = BSD_BOUNCE_DETECT_CNT;
			wbd_master->bounce_cfg.dwell_time = BSD_BOUNCE_DETECT_DWELL;
			WBD_WARNING("%s : %s = %s\n",
				wbderrorstr(WBDE_INV_NVVAL), WBD_NVRAM_BOUNCE_DETECT, str);
		}
	}

	/* Read bouncing STA CFG for Backhaul */
	str = blanket_nvram_safe_get(WBD_NVRAM_BOUNCE_DETECT_BH);
	if (str) {
		num = sscanf(str, "%d %d %d",
			&wbd_master->bounce_cfg_bh.window, &wbd_master->bounce_cfg_bh.cnt,
			&wbd_master->bounce_cfg_bh.dwell_time);
		if (num != 3) {
			wbd_master->bounce_cfg_bh.window = WBD_BOUNCE_DETECT_WIN_BH;
			wbd_master->bounce_cfg_bh.cnt = WBD_BOUNCE_DETECT_CNT_BH;
			wbd_master->bounce_cfg_bh.dwell_time = WBD_BOUNCE_DETECT_DWELL_BH;
			WBD_WARNING("%s : %s = %s\n",
				wbderrorstr(WBDE_INV_NVVAL), WBD_NVRAM_BOUNCE_DETECT_BH, str);
		}
	}

	/* Get NVRAM : Number of TBSS retries for STA in backhaul optimization */
	wbd_master->max_bh_opt_try = (uint8)blanket_get_config_val_uint(NULL,
		WBD_NVRAM_BH_OPT_TRY, WBD_BH_OPT_MAX_TRY);

	/* Get NVRAM : Number of backhaul optimization retries allowed upon weak STA processing
	 * failure due to formation of loop
	 */
	wbd_master->max_bh_opt_try_on_weak = (uint8)blanket_get_config_val_uint(NULL,
		WBD_NVRAM_BH_OPT_TRY_ON_WEAK, WBD_BH_OPT_MAX_TRY_ON_WEAK);

	/* If backhaul optimization is enabled, set the flag as enabled */
	if (blanket_get_config_val_int(NULL, WBD_NVRAM_BH_OPT, WBD_DEF_BH_OPT)) {
		wbd_master->flags |= WBD_FLAGS_BKT_BH_OPT_ENABLED;
	}

	/* Get NVRAM : Master flags */
	flags = blanket_get_config_val_uint(NULL, WBD_NVRAM_MASTER_FLAGS, 0);
	if (flags & WBD_NVRAM_FLAGS_MASTER_DONT_SEND_BCN_QRY) {
		wbd_master->flags |= WBD_FLAGS_BKT_DONT_SEND_BCN_QRY;
	}
	if (flags & WBD_NVRAM_FLAGS_MASTER_DONT_USE_BCN_RPT) {
		wbd_master->flags |= WBD_FLAGS_BKT_DONT_USE_BCN_RPT;
	}

	WBD_DEBUG("%s window[%d] cnt[%d] dwell_time[%d] %s window[%d] cnt[%d] dwell_time[%d] "
		"BH OPT Try[%d] Flags[0x%08x]\n",
		WBD_NVRAM_BOUNCE_DETECT, wbd_master->bounce_cfg.window, wbd_master->bounce_cfg.cnt,
		wbd_master->bounce_cfg.dwell_time,
		WBD_NVRAM_BOUNCE_DETECT_BH, wbd_master->bounce_cfg_bh.window,
		wbd_master->bounce_cfg_bh.cnt, wbd_master->bounce_cfg_bh.dwell_time,
		wbd_master->max_bh_opt_try, wbd_master->flags);

	WBD_EXIT();
	return ret;
}

/* Add Ignore MACs to the IEEE1905 controller */
static void
wbd_add_ignore_maclist_to_controller(wbd_info_t *info)
{
	int ret = WBDE_OK, sz = 0, idx = 0;
	wbd_mac_item_t *mac_item = NULL;
	dll_t *mac_item_p;
	unsigned char *macs;
	WBD_ENTER();

	if (info->wbd_master->ignore_maclist.count <= 0) {
		WBD_INFO("Ignore List is empty\n");
		goto end;
	}

	/* Allocate memory for all the MAC addresses */
	sz = (info->wbd_master->ignore_maclist.count * ETHER_ADDR_LEN);
	macs = (unsigned char*)wbd_malloc(sz, &ret);
	WBD_ASSERT_MSG("Failed to allocate MAC List of size %d\n", sz);

	/* Traverse Ignore MAC List */
	foreach_glist_item(mac_item_p, info->wbd_master->ignore_maclist) {

		mac_item = (wbd_mac_item_t*)mac_item_p;
		memcpy(&macs[idx], &mac_item->mac, ETHER_ADDR_LEN);
		idx += ETHER_ADDR_LEN;
	}

	/* Send all the MACs to the controller */
	ieee1905_add_mac_to_local_steering_disallowed_list(macs,
		info->wbd_master->ignore_maclist.count);

end:
	WBD_EXIT();
}

/* Initialize the Master module */
static int
wbd_init_master(wbd_info_t *info)
{
	int ret = WBDE_OK;
	WBD_ENTER();

	/* Try to open the server FD */
	info->server_fd = wbd_try_open_server_fd(EAPD_WKSP_WBD_TCP_MASTER_PORT, &ret);
	WBD_ASSERT_MSG("Failed to create server socket at port (0x%x) for master. Err=%d (%s)\n",
		EAPD_WKSP_WBD_TCP_MASTER_PORT, ret, wbderrorstr(ret));

	/* Try to open the server FD for CLI */
	info->cli_server_fd = wbd_try_open_server_fd(EAPD_WKSP_WBD_TCP_MASTERCLI_PORT, &ret);
	WBD_ASSERT_MSG("Failed to create server socket at port (0x%x) for CLI. Err=%d (%s)\n",
		EAPD_WKSP_WBD_TCP_MASTERCLI_PORT, ret, wbderrorstr(ret));

	/* Initialize the communication module for slave */
	ret = wbd_init_master_com_handle(info);
	WBD_ASSERT_MSG("wbd_init_master_com_handle failed: %d (%s)\n", ret, wbderrorstr(ret));

	/* Reset backhaul optimization complete flag */
	blanket_nvram_prefix_set(NULL, WBD_NVRAM_BH_OPT_COMPLETE, "0");

	/* Get the igonred MAClist from NVRAM */
	ret = wbd_retrieve_maclist_from_nvram(WBD_NVRAM_IGNR_MACLST,
		&info->wbd_master->ignore_maclist);

	/* Add Ignore MACs to the IEEE1905 controller */
	wbd_add_ignore_maclist_to_controller(info);

	/* Gets Blanket Master NVRAM settings */
	wbd_master_retrieve_blanket_nvram_config(info->wbd_master);

end:
	WBD_EXIT();
	return ret;
}

/* Exit the Master module */
static void
wbd_exit_master(wbd_info_t *info)
{
	int ret = WBDE_OK;
	WBD_ENTER();

	BCM_REFERENCE(ret);
	WBD_ASSERT_ARG(info, WBDE_INV_ARG);

	/* Reset backhaul optimization complete flag */
	blanket_nvram_prefix_set(NULL, WBD_NVRAM_BH_OPT_COMPLETE, "0");

	/* Deinit IEEE1905 module only if it is initialized. Because, if the WBD is disabled,
	 * it will not get initialized
	 */
	if (WBD_IEEE1905_INIT(info->flags)) {
		ieee1905_deinit();
	}
	wbd_ds_blanket_master_cleanup(info);
	wbd_com_deinit(info->com_cli_hdl);
	wbd_com_deinit(info->com_serv_hdl);
	wbd_info_cleanup(info);

end:
	WBD_EXIT();
}

/* Signal handler to display tty output. */
void
wbd_master_tty_hdlr(int sig)
{
	BCM_REFERENCE(sig);

	wbd_tty_hdlr(WBD_MASTER_FILE_TTY);
}

/* Signal handler */
void
wbd_signal_hdlr(int sig)
{
	WBD_ERROR("Signal : %d WBD Master going to shutdown\n", sig);
	wbd_exit_master(g_wbdinfo);
	exit(0);
}

/* Add bssinfo table in controller */
static void
wbd_master_add_bssinfo_to_controller()
{
	ieee1905_client_bssinfo_type bss;
	char prefix[IFNAMSIZ] = {0};
	char *nvval = NULL;
	char bss_info_names[NVRAM_MAX_VALUE_LEN] = {0};
	char bss_name[IFNAMSIZ] = {0}, akm[WBD_MAX_BUF_32];
	char *next_name, *next_akm;
	uint8 bkt_id = 0;

	/* Read and update bss info for each bss name provided */
	WBDSTRNCPY(bss_info_names, blanket_nvram_safe_get(NVRAM_MAP_BSS_NAMES),
		sizeof(bss_info_names));

	foreach(bss_name, bss_info_names, next_name) {

		memset(&bss, 0, sizeof(bss));

		sprintf(prefix, "%s_", bss_name);
		bss.band_flag = (uint8)blanket_get_config_val_uint(prefix, NVRAM_BAND_FLAG,
			WBD_BAND_LAN_INVALID);

		nvval = blanket_nvram_prefix_safe_get(prefix, NVRAM_SSID);
		memcpy(bss.ssid.SSID, nvval, sizeof(bss.ssid.SSID));
		bss.ssid.SSID_len = strlen(nvval);

		nvval = blanket_nvram_prefix_safe_get(prefix, NVRAM_AKM);
		foreach(akm, nvval, next_akm) {
			if (!strcmp(akm, "wpa2")) {
				bss.AuthType |= IEEE1905_AUTH_WPA2;
			} else if (!strcmp(akm, "wpa")) {
				bss.AuthType |= IEEE1905_AUTH_WPA;
			} else if (!strcmp(akm, "shared")) {
				bss.AuthType |= IEEE1905_AUTH_SHARED;
			} else if (!strcmp(akm, "psk")) {
				bss.AuthType |= IEEE1905_AUTH_WPAPSK;
			} else if (!strcmp(akm, "psk2")) {
				bss.AuthType |= IEEE1905_AUTH_WPA2PSK;
			} else if (!strcmp(akm, "sae")) {
				bss.AuthType |= IEEE1905_AUTH_SAE;
			}
		}

		/* Set to open if none of the above is set */
		if (bss.AuthType == 0) {
			bss.AuthType = IEEE1905_AUTH_OPEN;
		}

		bss.EncryptType = IEEE1905_ENCR_NONE;
		nvval = blanket_nvram_prefix_safe_get(prefix, NVRAM_CRYPTO);
		if (!strcmp(nvval, "wep")) {
			bss.EncryptType = IEEE1905_ENCR_WEP;
		} else if (!strcmp(nvval, "tkip")) {
			bss.EncryptType = IEEE1905_ENCR_TKIP;
		} else if (!strcmp(nvval, "aes")) {
			bss.EncryptType = IEEE1905_ENCR_AES;
		} else if (!strcmp(nvval, "tkip+aes")) {
			bss.EncryptType = IEEE1905_ENCR_TKIP | IEEE1905_ENCR_AES;
		}

		nvval = blanket_nvram_prefix_safe_get(prefix, NVRAM_WPA_PSK);
		bss.NetworkKey.key_len = strlen(nvval);
		snprintf((char*)bss.NetworkKey.key, sizeof(bss.NetworkKey.key), "%s", nvval);

		nvval = blanket_nvram_prefix_safe_get(prefix, NVRAM_MAP);
		if (strcmp(nvval, "1") == 0) {
			bss.FrontHaulBSS = 1;
		} else if (strcmp(nvval, "2") == 0) {
			bss.BackHaulBSS = 1;
		} else if (strcmp(nvval, "3") == 0) {
			bss.FrontHaulBSS = 1;
			bss.BackHaulBSS = 1;
		} else if (strcmp(nvval, "9") == 0) {
			bss.FrontHaulBSS = 1;
			bss.Guest = 1;
		}

		WBD_INFO("prefix: [%s] band: [0x%x] SSID: [%s] AuthType [%d] EncryptType: [%d] "
			"key [%s] fh_bss [%d] bh_bss [%d] guest [%d]\n",
			prefix, bss.band_flag, bss.ssid.SSID,
			bss.AuthType, bss.EncryptType, bss.NetworkKey.key,
			bss.FrontHaulBSS, bss.BackHaulBSS, bss.Guest);

		ieee1905_add_bssto_controller_table(&bss);

		/* If it supports fronthaul BSS, then create a blanket out of that */
		if (bss.FrontHaulBSS) {
			bkt_id = WBD_BKT_ID_BR0; /* Default Blanket ID for now */
			wbd_master_create_master_info(g_wbdinfo, bkt_id, bss_name);
		}
	}
}

/* Callback from IEEE 1905 module when the Controller wants to Process vendor specific message */
static int
wbd_ieee1905_process_vendor_specific_msg_cb(ieee1905_vendor_data *msg_data)
{
	WBD_DEBUG("Received ieee1905 callback: Process vendor specific message\n");
	return wbd_master_process_vendor_specific_msg(msg_data);
}

/* Callback from IEEE 1905 module to Get Vendor Specific TLV to send */
static void
wbd_ieee1905_get_vendor_specific_tlv_cb(i5_msg_types_with_vndr_tlv_t msg_type,
	ieee1905_vendor_data *out_vendor_tlv)
{
	WBD_DEBUG("Received callback: Get Vendor Specific TLV\n");
	return wbd_master_get_vendor_specific_tlv(msg_type, out_vendor_tlv);
}

/* Callback from IEEE 1905 module to init the device */
static void
wbd_ieee1905_device_init_cb(i5_dm_device_type *i5_device)
{
	WBD_ENTER();

	WBD_DEBUG("Received callback: Device["MACDBG"] got added\n",
		MAC2STRDBG(i5_device->DeviceId));

	wbd_ds_device_init(i5_device);
	/* Handle device removal and getting added. */
	wbd_master_create_ap_autoconfig_renew_timer(g_wbdinfo);

	WBD_EXIT();
}

/* Callback from IEEE 1905 module to deinit the device */
static void
wbd_ieee1905_device_deinit_cb(i5_dm_device_type *i5_device)
{
	WBD_ENTER();

	WBD_DEBUG("Received callback: Device["MACDBG"] got removed\n",
		MAC2STRDBG(i5_device->DeviceId));

	wbd_ds_device_deinit(i5_device);

	WBD_EXIT();
}

/* Callback from IEEE 1905 module to init the BSS on interface */
static void
wbd_ieee1905_bss_init_cb(i5_dm_bss_type *i5_bss)
{
	int ret = WBDE_OK;
	wbd_master_info_t *master_info;
	i5_dm_device_type *i5_device;
	i5_dm_interface_type *i5_ifr;
	WBD_ENTER();

	WBD_SAFE_GET_MASTER_INFO(g_wbdinfo, WBD_BKT_ID_BR0, master_info, &ret);

	wbd_ds_bss_init(i5_bss);

	master_info->blanket_bss_count++;

	wbd_ds_add_monitorlist_fm_peer_devices_assoclist(i5_bss);

	/* Check if we should start all the backhaul STAs optimization in the network if the BSS
	 * is backhaul
	 */
	i5_ifr = (i5_dm_interface_type*)WBD_I5LL_PARENT(i5_bss);
	i5_device = (i5_dm_device_type*)WBD_I5LL_PARENT(i5_ifr);
	if (I5_HAS_BH_BSS(i5_device->flags)) {
		wbd_master_create_bh_opt_timer(NULL);
	}

end:
	WBD_EXIT();
}

/* Callback from IEEE 1905 module to deinit the BSS on interface */
static void
wbd_ieee1905_bss_deinit_cb(i5_dm_bss_type *i5_bss)
{
	int ret = WBDE_OK;
	wbd_master_info_t *master_info;
	WBD_ENTER();

	WBD_SAFE_GET_MASTER_INFO(g_wbdinfo, WBD_BKT_ID_BR0, master_info, &ret);
	wbd_ds_bss_deinit(i5_bss);
	master_info->blanket_bss_count--;

end:
	WBD_EXIT();
}

/* Callback from IEEE 1905 module when the STA got added in the data model */
static void
wbd_ieee1905_sta_added_cb(i5_dm_clients_type *i5_assoc_sta)
{
	int ret = WBDE_OK;
	wbd_master_info_t *master_info;
	i5_dm_bss_type *i5_bss;
	wbd_wl_sta_stats_t sta_stats;
	char logmsg[WBD_LOGS_BUF_128] = {0}, timestamp[WBD_MAX_BUF_32] = {0};
	WBD_ENTER();

	WBD_DEBUG("Received callback: STA["MACDBG"] got added\n", MAC2STRDBG(i5_assoc_sta->mac));

	WBD_SAFE_GET_MASTER_INFO(g_wbdinfo, WBD_BKT_ID_BR0, master_info, &ret);

	i5_bss = (i5_dm_bss_type*)WBD_I5LL_PARENT(i5_assoc_sta);
	wbd_ds_sta_init(g_wbdinfo, i5_assoc_sta);

	master_info->blanket_client_count++;

	/* Remove STA from current BSS's monitor list, Check for Igonre maclist */
	wbd_ds_add_sta_in_controller(g_wbdinfo->wbd_master, i5_assoc_sta);

	/* Inform One STA has joined */
	memset(&sta_stats, 0, sizeof(sta_stats));
	wbd_controller_refresh_blanket_on_sta_assoc((struct ether_addr*)i5_assoc_sta->mac,
		(struct ether_addr*)i5_bss->BSSID, &sta_stats);

	/* Create and store log message for assoc. */
	snprintf(logmsg, sizeof(logmsg), CLI_CMD_LOGS_ASSOC,
		wbd_get_formated_local_time(timestamp, sizeof(timestamp)),
		MAC2STRDBG(i5_assoc_sta->mac), MAC2STRDBG(i5_bss->BSSID));
	wbd_ds_add_logs_in_master(g_wbdinfo->wbd_master, logmsg);

	/* If the associated STA is backhaul, then start the timer which will start backhaul
	 * optimization
	 */
	if (I5_CLIENT_IS_BSTA(i5_assoc_sta)) {
		WBD_INFO("BSS["MACDBG"] Backhaul STA["MACDBG"] Associated\n",
			MAC2STRDBG(i5_bss->BSSID), MAC2STRDBG(i5_assoc_sta->mac));
		wbd_master_create_bh_opt_timer(i5_assoc_sta);
	}

end:
	WBD_EXIT();
}

/* Callback from IEEE 1905 module when the STA got removed from the data model */
static void
wbd_ieee1905_sta_removed_cb(i5_dm_clients_type *i5_assoc_sta)
{
	int ret = WBDE_OK;
	wbd_master_info_t *master_info;
	i5_dm_bss_type *i5_bss;
	char logmsg[WBD_LOGS_BUF_128] = {0}, timestamp[WBD_MAX_BUF_32] = {0};
	uint8 map_flags = IEEE1905_MAP_FLAG_FRONTHAUL;
	WBD_ENTER();

	WBD_DEBUG("Received callback: STA got Removed\n");

	WBD_SAFE_GET_MASTER_INFO(g_wbdinfo, WBD_BKT_ID_BR0, master_info, &ret);

	i5_bss = (i5_dm_bss_type*)WBD_I5LL_PARENT(i5_assoc_sta);
	master_info->blanket_client_count--;

	wbd_ds_remove_beacon_report(g_wbdinfo, (struct ether_addr*)&(i5_assoc_sta->mac));

	/* Remove a STA item from all peer BSS' Monitor STA List based on mapFlags */
	if (I5_CLIENT_IS_BSTA(i5_assoc_sta)) {
		map_flags = IEEE1905_MAP_FLAG_BACKHAUL;
	}
	wbd_ds_remove_sta_fm_peer_devices_monitorlist((struct ether_addr*)i5_bss->BSSID,
		(struct ether_addr*)i5_assoc_sta->mac, map_flags);

	/* Create and store log message for disassoc. */
	snprintf(logmsg, sizeof(logmsg), CLI_CMD_LOGS_DISASSOC,
		wbd_get_formated_local_time(timestamp, sizeof(timestamp)),
		MAC2STRDBG(i5_assoc_sta->mac), MAC2STRDBG(i5_bss->BSSID));
	wbd_ds_add_logs_in_master(g_wbdinfo->wbd_master, logmsg);

	wbd_ds_sta_deinit(i5_assoc_sta);

end:
	WBD_EXIT();
}

/* Callback from IEEE 1905 module to deinit the interface on device */
static void
wbd_ieee1905_interface_deinit_cb(i5_dm_interface_type *i5_ifr)
{
	wbd_ds_interface_deinit(i5_ifr);
}

/* Callback from IEEE 1905 module when the controller gets Associated STA link metric response */
static void
wbd_ieee1905_assoc_sta_metric_resp_cb(unsigned char *al_mac, unsigned char *bssid,
	unsigned char *sta_mac, ieee1905_sta_link_metric *metric)
{
	WBD_DEBUG("Received callback: Assocated STA Link Metrics Response\n");
	return (wbd_master_process_assoc_sta_metric_resp(g_wbdinfo, al_mac,
		bssid, sta_mac, metric));
}

/* Callback from IEEE 1905 module when the controller gets UnAssociated STA link metric response */
static void
wbd_ieee1905_unassoc_sta_metric_resp_cb(unsigned char *al_mac,
	ieee1905_unassoc_sta_link_metric *metric)
{
	WBD_DEBUG("Received callback: UnAssocated STA Link Metrics Response\n");
	return (wbd_master_unassoc_sta_metric_resp(al_mac, metric));
}

/* Callback from IEEE 1905 module when the controller gets beacon metric response */
static void
wbd_ieee1905_beacon_metric_resp_cb(unsigned char *al_mac, ieee1905_beacon_report *report)
{
	WBD_DEBUG("Received callback: Beacon Metrics Response\n");
	wbd_master_store_beacon_metric_resp(g_wbdinfo, al_mac, report);
	return (wbd_master_beacon_metric_resp(al_mac, report));
}

/* Callback from IEEE 1905 module to inform AP auto configuration */
static void
wbd_ieee1905_ap_configured_cb(unsigned char *al_mac, unsigned char *radio_mac,
	int if_band)
{
#if defined(MULTIAPR2)
	static bool onboot_chscan = FALSE;
#endif /* MULTIAPR2 */

	WBD_INFO("Received callback: AP autoconfigured on Device ["MACDBG"] Radio["MACDBG"]"
		"in band [%d]\n", MAC2STRDBG(al_mac), MAC2STRDBG(radio_mac), if_band);
	wbd_master_set_ap_configured(al_mac, radio_mac, if_band);
	wbd_master_send_policy_config(g_wbdinfo, al_mac, radio_mac, if_band);
	wbd_master_send_bss_capability_query(g_wbdinfo, al_mac, radio_mac);
	wbd_master_send_backhaul_sta_metric_policy_vndr_cmd(g_wbdinfo, al_mac, radio_mac);
	wbd_master_create_channel_select_timer(g_wbdinfo, al_mac);

#if defined(MULTIAPR2)
	/* Create OnBoot Channel Scan Request timer to Get Channel Scan Report from this Agent */
	if (!onboot_chscan) {
		onboot_chscan = TRUE;
		wbd_master_create_onboot_channel_scan_req_timer(g_wbdinfo, al_mac);
	}
#endif /* MULTIAPR2 */
}

/* Callback from IEEE 1905 module to inform operating channel report updation on the radio */
static void
wbd_ieee1905_operating_channel_report_cb(unsigned char *al_mac,
	ieee1905_operating_chan_report *chan_report)
{
	WBD_DEBUG("Received callback for operating channel report from Device ["MACDBG"] \n",
		MAC2STRDBG(al_mac));
	wbd_master_process_operating_chan_report(g_wbdinfo, al_mac, chan_report);
	wbd_master_update_lowest_tx_pwr(g_wbdinfo, al_mac, chan_report);
}

/* Callback from IEEE 1905 module to get interface info */
static int
wbd_ieee1905_get_interface_info_cb(char *ifname, ieee1905_ifr_info *info)
{
	return (wbd_master_get_interface_info_cb(ifname, info));
}

/* Callback from IEEE 1905 module to get steering response */
static void
wbd_ieee1905_steering_btm_report_cb(ieee1905_btm_report *btm_report)
{
	char logmsg[WBD_LOGS_BUF_128] = {0}, timestamp[WBD_MAX_BUF_32] = {0};

	WBD_DEBUG("Received callback for steering btm report for sta ["MACDBG"] \n",
		MAC2STRDBG(btm_report->sta_mac));

	/* Create and store log message for steer response. */
	if (ETHER_ISNULLADDR((struct ether_addr*)&btm_report->trgt_bssid)) {
		snprintf(logmsg, sizeof(logmsg), CLI_CMD_LOGS_STEER_RESP,
			wbd_get_formated_local_time(timestamp, sizeof(timestamp)),
			MAC2STRDBG(btm_report->sta_mac),
			btm_report->status == 0 ? "Accept" : "Reject", btm_report->status);
	} else {
		snprintf(logmsg, sizeof(logmsg), CLI_CMD_LOGS_STEER_RESP_TBSS,
			wbd_get_formated_local_time(timestamp, sizeof(timestamp)),
			MAC2STRDBG(btm_report->sta_mac),
			btm_report->status == 0 ? "Accept" : "Reject", btm_report->status,
			MAC2STRDBG(btm_report->trgt_bssid));
	}

	wbd_ds_add_logs_in_master(g_wbdinfo->wbd_master, logmsg);
}

/* Callback from IEEE 1905 module to inform the DFS status update on the operating channel of an
 * interface
 */
static void
wbd_ieee1905_operating_channel_dfs_update_cb(i5_dm_interface_type *i5_ifr, unsigned char old_flags,
	unsigned char new_flags)
{
	int has_bh_bss = 0;
	i5_dm_bss_type *i5_bss;

	/* Check if it has backhaul BSS or not */
	foreach_i5glist_item(i5_bss, i5_dm_bss_type, i5_ifr->bss_list) {
		if (I5_IS_BSS_BACKHAUL(i5_bss->mapFlags)) {
			has_bh_bss = 1;
			break;
		}
	}

	WBD_INFO("IFR["MACDBG"] DFS status changed old_flags[%x] new_flags[%x] has_bh_bss[%d]\n",
		MAC2STRDBG(i5_ifr->InterfaceId), old_flags, new_flags, has_bh_bss);

	/* If the CAC is completed, start backhaul optimization */
	if (has_bh_bss && (I5_IS_IFR_CAC_PENDING(old_flags)) &&
		!(I5_IS_IFR_CAC_PENDING(new_flags))) {
		wbd_master_create_bh_opt_timer(NULL);
	}
}

#if defined(MULTIAPR2)
/* Callback from IEEE 1905 module to process tunnel message */
static void
wbd_ieee1905_process_tunneled_msg_cb(unsigned char *al_mac, ieee1905_tunnel_msg_t *msg)
{
	WBD_DEBUG("Received callback for handle tunnel message from Device ["MACDBG"] \n",
		MAC2STRDBG(al_mac));
	wbd_master_process_tunneled_msg(g_wbdinfo, al_mac, msg);
}

/* Callback from IEEE 1905 module to Process Channel Scan Report by Controller */
static void
wbd_ieee1905_channel_scan_rpt_cb(unsigned char *src_al_mac, time_t ts_chscan_rpt,
	ieee1905_chscan_report_msg *chscan_rpt)
{
	WBD_DEBUG("Received callback: Process Channel Scan Report by Controller\n");
	return wbd_master_process_channel_scan_rpt_cb(src_al_mac, ts_chscan_rpt, chscan_rpt);
}
#endif /* MULTIAPR2 */

/* Register ieee1905 callbacks. */
static void
wbd_master_register_ieee1905_callbacks(ieee1905_call_bks_t *cbs)
{
	cbs->process_vendor_specific_msg = wbd_ieee1905_process_vendor_specific_msg_cb;
	cbs->device_init = wbd_ieee1905_device_init_cb;
	cbs->device_deinit = wbd_ieee1905_device_deinit_cb;
	cbs->bss_init = wbd_ieee1905_bss_init_cb;
	cbs->bss_deinit = wbd_ieee1905_bss_deinit_cb;
	cbs->client_init = wbd_ieee1905_sta_added_cb;
	cbs->client_deinit = wbd_ieee1905_sta_removed_cb;
	cbs->interface_deinit = wbd_ieee1905_interface_deinit_cb;
	cbs->assoc_sta_metric_resp = wbd_ieee1905_assoc_sta_metric_resp_cb;
	cbs->unassoc_sta_metric_resp = wbd_ieee1905_unassoc_sta_metric_resp_cb;
	cbs->beacon_metric_resp = wbd_ieee1905_beacon_metric_resp_cb;
	cbs->ap_configured = wbd_ieee1905_ap_configured_cb;
	cbs->operating_chan_report = wbd_ieee1905_operating_channel_report_cb;
	cbs->get_interface_info = wbd_ieee1905_get_interface_info_cb;
	cbs->get_vendor_specific_tlv = wbd_ieee1905_get_vendor_specific_tlv_cb;
	cbs->steering_btm_rpt = wbd_ieee1905_steering_btm_report_cb;
	cbs->operating_channel_dfs_update = wbd_ieee1905_operating_channel_dfs_update_cb;
#if defined(MULTIAPR2)
	cbs->process_tunneled_msg = wbd_ieee1905_process_tunneled_msg_cb;
	cbs->channel_scan_rpt = wbd_ieee1905_channel_scan_rpt_cb;
#endif /* MULTIAPR2 */
}

/* Initialize the blanket module */
void
wbd_master_init_blanket_module()
{
	blanket_module_info_t bkt_info;

	/* Initialize blanket module */
	memset(&bkt_info, 0, sizeof(bkt_info));
	bkt_info.msglevel = blanket_get_config_val_uint(NULL, NVRAM_BKT_MSGLEVEL,
		BKT_DEBUG_DEFAULT);
	blanket_module_init(&bkt_info);
}

/* main entry point */
int
main(int argc, char *argv[])
{
	int ret = WBDE_OK;
	char wbd_ifnames[NVRAM_MAX_VALUE_LEN] = {0};
	ieee1905_call_bks_t cbs;

	memset(&cbs, 0, sizeof(cbs));

	WBD_DEBUG("Starting wbd_master daemon...\n");
	/* Set Process Name */
	snprintf(g_wbd_process_name, sizeof(g_wbd_process_name), "Master");

	/* Parse common cli arguments */
	wbd_parse_cli_args(argc, argv);

	signal(SIGTERM, wbd_signal_hdlr);
	signal(SIGINT, wbd_signal_hdlr);
	signal(SIGPWR, wbd_master_tty_hdlr);

	wbd_master_init_blanket_module();

	/* Give a dummy call to read wbd ifnames, so that it can set any FBT releated NVRAMs */
	wbd_read_actual_ifnames(wbd_ifnames, sizeof(wbd_ifnames), TRUE);
	WBD_INFO("Actual wbd_ifnames[%s]\n", wbd_ifnames);

	/* Allocate & Initialize the info structure */
	g_wbdinfo = wbd_info_init(&ret);
	WBD_ASSERT_MSG("wbd_info_init failed: %d (%s)\n", ret, wbderrorstr(ret));

	/* If the mode is not controller, the master exe should exit */
	if (!MAP_IS_CONTROLLER(g_wbdinfo->map_mode)) {
		WBD_WARNING("Multi-AP Mode (%d) not configured as controller..\n",
			g_wbdinfo->map_mode);
		goto end;
	}

	/* Allocate & Initialize Blanket Master structure object */
	ret = wbd_ds_blanket_master_init(g_wbdinfo);
	WBD_ASSERT_MSG("wbd_ds_blanket_master_init failed: %d (%s)\n", ret, wbderrorstr(ret));

	wbd_master_register_ieee1905_callbacks(&cbs);

	ret = ieee1905_module_init(g_wbdinfo, MAP_MODE_FLAG_CONTROLLER,
		(MAP_IS_CONTROLLER(g_wbdinfo->map_mode) ? 1 : 0), &cbs);
	WBD_ASSERT();

	wbd_master_add_bssinfo_to_controller();

	/* Initialize Master */
	ret = wbd_init_master(g_wbdinfo);
	WBD_ASSERT_MSG("wbd_init_master failed: %d (%s)\n", ret, wbderrorstr(ret));

	blanket_start_multiap_messaging();

	/* Provide necessary info to debug_monitor for service restart */
	dm_register_app_restart_info(getpid(), argc, argv, NULL);
	/* Main loop which keeps on checking for the timers and fd's */
	wbd_run(g_wbdinfo->hdl);

end:
	/* Exit Master */
	wbd_exit_master(g_wbdinfo);
	WBD_DEBUG("Exited wbd_master daemon...\n");

	return ret;
}
