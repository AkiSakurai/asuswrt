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
 * $Id: wbd_master.c 791168 2020-09-18 06:14:13Z $
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
#ifdef BCM_APPEVENTD
#include "wbd_appeventd.h"
#include "appeventd_wbd.h"
#endif /* BCM_APPEVENTD */

wbd_info_t *g_wbdinfo = NULL;
unsigned int wbd_msglevel = WBD_DEBUG_DEFAULT;
char g_wbd_process_name[BKT_MAX_PROCESS_NAME];

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

	ret = wbd_add_timers(info->hdl, info, WBD_SEC_MICROSEC(info->max.tm_probe),
		wbd_ds_timeout_prbsta, 1);
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

	ieee1905_deinit();

	/* Reset backhaul optimization complete flag */
	blanket_nvram_prefix_set(NULL, WBD_NVRAM_BH_OPT_COMPLETE, "0");

	WBD_ASSERT_ARG(info, WBDE_INV_ARG);

	wbd_ds_blanket_master_cleanup(info);
	wbd_com_deinit(info->com_cli_hdl);
	wbd_com_deinit(info->com_serv_hdl);
	wbd_info_cleanup(info);
	g_wbdinfo = NULL;

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
	char prefix[IFNAMSIZ+2] = {0};
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

		bss.map_flag = (uint8)blanket_get_config_val_uint(prefix, NVRAM_MAP, 0);

		nvval = blanket_nvram_prefix_safe_get(prefix, NVRAM_CLOSED);
		if (strcmp(nvval, "1") == 0)
			bss.Closed = 1;
		/* If it is dedicated backhaul and the closed NVRAM is not defined then by
		 * default set it to closed
		 */
		if (I5_IS_BSS_BACKHAUL(bss.map_flag) && !I5_IS_BSS_FRONTHAUL(bss.map_flag) &&
			(nvval[0] == '\0')) {
			bss.Closed = 1;
		}

		WBD_INFO("prefix: [%s] band: [0x%x] SSID: [%s] AuthType [%d] EncryptType: [%d] "
			"key [%s] map_flag [0x%x] closed [%d]\n",
			prefix, bss.band_flag, bss.ssid.SSID,
			bss.AuthType, bss.EncryptType, bss.NetworkKey.key,
			bss.map_flag, bss.Closed);

		ieee1905_add_bssto_controller_table(&bss);

		/* If it supports fronthaul BSS, then create a blanket out of that */
		if (I5_IS_BSS_FRONTHAUL(bss.map_flag)) {
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
	int ret = WBDE_OK;
	wbd_device_item_t *device_vndr;
	WBD_ENTER();

	WBD_ASSERT_ARG(i5_device, WBDE_INV_ARG);
	WBD_ASSERT_ARG(i5_device->vndr_data, WBDE_INV_ARG);

	WBD_DEBUG("Received callback: Device["MACDBG"] got removed\n",
		MAC2STRDBG(i5_device->DeviceId));

	device_vndr = (wbd_device_item_t*)i5_device->vndr_data;

	/* Remove some of the timers created for which the device pointer is passed as an
	 * argument to the timer callback. If we don't remove here then in timer callback it
	 * will use the freed pointer
	 */
	if (device_vndr->flags & WBD_DEV_FLAG_CHAN_PREFERENCE_TIMER)  {
		wbd_remove_timers(g_wbdinfo->hdl, wbd_master_send_channel_preference_query_cb,
			i5_device);
	}
	if (device_vndr->flags & WBD_DEV_FLAG_CHAN_SELECT_TIMER)  {
		wbd_remove_timers(g_wbdinfo->hdl, wbd_master_send_channel_select_cb, i5_device);
	}

#if defined(MULTIAPR2)
	if (device_vndr->flags & WBD_DEV_FLAG_ONBOOT_CHSCAN_TIMER)  {
		wbd_remove_timers(g_wbdinfo->hdl, wbd_master_send_onboot_channel_scan_req_cb,
			i5_device);
	}
#endif /* MULTIAPR2 */

	wbd_ds_device_deinit(i5_device);

end:
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

	WBD_SAFE_GET_MASTER_INFO(g_wbdinfo, WBD_BKT_ID_BR0, master_info, &ret);

	i5_bss = (i5_dm_bss_type*)WBD_I5LL_PARENT(i5_assoc_sta);
	WBD_INFO("%sSTA "MACDBG" associated on BSS "MACDBG"\n",
		I5_CLIENT_IS_BSTA(i5_assoc_sta)? "Backhaul ": "", MAC2STRDBG(i5_assoc_sta->mac),
		MAC2STRDBG(i5_bss->BSSID));
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
	WBD_ENTER();

	WBD_SAFE_GET_MASTER_INFO(g_wbdinfo, WBD_BKT_ID_BR0, master_info, &ret);

	i5_bss = (i5_dm_bss_type*)WBD_I5LL_PARENT(i5_assoc_sta);
	WBD_DEBUG("Received callback: STA "MACDBG" got Removed from BSS "MACDBG"\n",
		MAC2STRDBG(i5_assoc_sta->mac), MAC2STRDBG(i5_bss->BSSID));

	master_info->blanket_client_count--;

	wbd_ds_remove_beacon_report(g_wbdinfo, (struct ether_addr*)&(i5_assoc_sta->mac));

	/* Remove a STA item from all peer BSS' Monitor STA List based on ssid */

	wbd_ds_remove_sta_fm_peer_devices_monitorlist((struct ether_addr*)i5_bss->BSSID,
		(struct ether_addr*)i5_assoc_sta->mac, &i5_bss->ssid);

	/* Create and store log message for disassoc. */
	snprintf(logmsg, sizeof(logmsg), CLI_CMD_LOGS_DISASSOC,
		wbd_get_formated_local_time(timestamp, sizeof(timestamp)),
		MAC2STRDBG(i5_assoc_sta->mac), MAC2STRDBG(i5_bss->BSSID));
	wbd_ds_add_logs_in_master(g_wbdinfo->wbd_master, logmsg);

	wbd_ds_sta_deinit(i5_assoc_sta);

end:
	WBD_EXIT();
}

/* Callback from IEEE 1905 module to init the interface on device */
static void
wbd_ieee1905_interface_init_cb(i5_dm_interface_type *i5_ifr)
{
	if (!i5_ifr) {
		return;
	}

	wbd_ds_interface_init(i5_ifr);
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

/* Callback from IEEE 1905 module to inform the Non-operable Channels update of an interface */
static void
wbd_ieee1905_nonoperable_channel_update_cb(i5_dm_interface_type *i5_ifr, int dynamic)
{
	wbd_ifr_item_t *ifr_vndr_data = (wbd_ifr_item_t*)i5_ifr->vndr_data;
	if (!ifr_vndr_data) {
		return;
	}
	WBD_INFO("IFR["MACDBG"] Updating %s Non-operable Channels of an interface.\n",
		MAC2STRDBG(i5_ifr->InterfaceId), dynamic ? "DYNAMIC" : "STATIC");

	if (!dynamic) {
		/* Update STATIC Non-operable Chanspec List of an interface */
		blanket_update_static_nonoperable_chanspec_list(i5_ifr->band,
			&i5_ifr->ApCaps.RadioCaps, &ifr_vndr_data->static_exclude_chlist);

	} else {
		/* Update DYNAMIC Non-operable Chanspec List of an interface */
		blanket_update_dynamic_nonoperable_chanspec_list(i5_ifr->band,
			&i5_ifr->ChanPrefs, &ifr_vndr_data->dynamic_exclude_chlist);
	}
	/* IF STATIC & Static List count > 0 || DYNAMIC & Dynamic List count > 0 , then Send */
	if ((!dynamic && (ifr_vndr_data->static_exclude_chlist.count > 0)) ||
		(dynamic && (ifr_vndr_data->dynamic_exclude_chlist.count > 0))) {

		/* Send Consolidated Exclude list to ACSD */
		wbd_master_process_nonoperable_channel_update(i5_ifr, dynamic);
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

/* Callback from IEEE 1905 module to inform 1905 AP-Autoconfiguration Response message from
 * controller
 */
static void
wbd_ieee1905_ap_auto_config_resp_cb(i5_dm_device_type *i5_device)
{
	char *ptr, *nvval;
	int len = WBD_STR_MAC_LEN + 1 /* Space */ + 1 /* Null terminator */;
	int ret = WBDE_OK;
	char ctrl_al_mac[WBD_STR_MAC_LEN + 1 /* Null terminator */] = {0};

	WBD_ENTER();

	WBD_ASSERT_ARG(i5_device, WBDE_INV_ARG);

	snprintf(ctrl_al_mac, sizeof(ctrl_al_mac), ""MACDBG"", MAC2STRDBG(i5_device->DeviceId));

	WBD_ERROR("Detected another controller[%s] in the network.\n", ctrl_al_mac);

#ifdef BCM_APPEVENTD
	wbd_appevetd_controller_detected(APP_E_WBD_MASTER_ANOTHER_CONTROLLER_FOUND,
		(struct ether_addr*)i5_device->DeviceId);
#endif	/* BCM_APPEVENTD */

	/* Check if there are already duplicate controllers found, if yes than append currently
	 * found controller's al mac to the existing value of the nvram.
	 */
	ptr = blanket_nvram_prefix_safe_get(NULL, NVRAM_MAP_DUP_CONTROLLER);

	/* If first entry in the nvram value is same as controller mac no need to update again */
	if (ptr == strstr(ptr, ctrl_al_mac)) {
		goto end;
	}

	len += strlen(ptr) + 1 /* Null terminator */;
	nvval = (char*)wbd_malloc(len, &ret);
	WBD_ASSERT_MSG("Mem alloc of %d bytes failed for buffer to store "
		"duplicate controller mac\n", len);

	if (ptr[0] != '\0') {
		char *tmp;

		/* Update the nvram value with ctrl_al_mac as first entry in nvram */
		if ((tmp = strdup(ptr)) != NULL) {
			remove_from_list(ctrl_al_mac, tmp, strlen(tmp));
			snprintf(nvval, len, "%s %s", ctrl_al_mac, tmp);
			free(tmp);
		} else {
			WBD_ERROR("Mem alloc failed for buffer to store nvram value\n");
			free(nvval);
			goto end;
		}
	} else {
		snprintf(nvval, len, "%s", ctrl_al_mac);
	}

	/* Set nvram value, followed by nvram commit and free the nvval buffer */
	blanket_nvram_prefix_set(NULL, NVRAM_MAP_DUP_CONTROLLER, nvval);
	wbd_do_rc_restart_reboot(WBD_FLG_NV_COMMIT);
	free(nvval);

end:
	if (!blanket_get_config_val_int(NULL, NVRAM_MAP_KEEP_CONTOLLER_UP, 0)) {
		wbd_master_update_multiap_mode_to_agent();
		wbd_do_rc_restart_reboot(WBD_FLG_NV_COMMIT | WBD_FLG_RC_RESTART);
	}

	WBD_EXIT();
}

/* Callback function to receive CAC completion. All the status except RADAR found are ignored here.
 * If the status is RADAR DETECTED, this fucntion will send CAC termination message to all other
 * R2 agents so that CAC can be stopped.
 */
void
wbd_ieee1905_process_cac_complete_cb(uint8 *al_mac, ieee1905_cac_completion_list_t *cac_list)
{
	int i;
	int ret = WBDE_OK;
	ieee1905_radio_cac_completion_t *cac_compl;
	i5_dm_device_type *device;
	i5_dm_interface_type *src_ifr;

	WBD_ENTER();

	if (!al_mac || !cac_list) {
		WBD_ERROR("NULL callback data\n");
		return;
	}
	WBD_INFO("CAC completion received from ["MACDBG"] radio count [%d]\n",
		MAC2STRDBG(al_mac), cac_list->count);

	device = wbd_ds_get_i5_device(al_mac, &ret);
	WBD_ASSERT();

	for (i = 0; i < cac_list->count; i++) {
		cac_compl = &cac_list->params[i];

		src_ifr = wbd_ds_get_i5_ifr_in_device(device, cac_compl->mac, &ret);
		WBD_ASSERT();

		WBD_INFO("CAC completion on Interface["MACDBG"] Opclass[%d] "
			"Channel[%d] status [%d] radar pairs count [%d]\n",
			MAC2STRDBG(cac_compl->mac), cac_compl->opclass, cac_compl->chan,
			cac_compl->status, cac_compl->n_opclass_chan);

		/* If the status is RADAR detected, propagate it to all the other agents so that any
		 * running CAC on this channel can be stopped. wbd_master_broadcast_vendor_msg_zwdfs
		 * will send zwdfs vendor command for R1 agents and CAC termination to
		 * R2 and above agents
		 */
		if (cac_compl->status == MAP_CAC_STATUS_RADAR_DETECTED) {
			wbd_cmd_vndr_zwdfs_msg_t zwdfs_msg;

			memset(&zwdfs_msg, 0, sizeof(zwdfs_msg));
			zwdfs_msg.cntrl_chan = cac_compl->chan;
			zwdfs_msg.opclass = cac_compl->opclass;
			//TODO:add a vendor TLV to CAC completion reason for backward compatiability
			zwdfs_msg.reason = WL_CHAN_REASON_DFS_AP_MOVE_RADAR_FOUND;
			wbd_master_broadcast_vendor_msg_zwdfs(al_mac, &zwdfs_msg, src_ifr->band);
		}
	}
end:
	WBD_EXIT();

}

/* Callback funtion to indicate active channels are updated for the device. So prepare common
 * active channels across device again and send it to all the agents
 */
void
wbd_ieee1905_process_cac_status_cb(i5_dm_device_type *device)
{
	wbd_master_prepare_and_send_dfs_chan_info(device);
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
	cbs->interface_init = wbd_ieee1905_interface_init_cb;
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
	cbs->nonoperable_channel_update = wbd_ieee1905_nonoperable_channel_update_cb;
#if defined(MULTIAPR2)
	cbs->process_tunneled_msg = wbd_ieee1905_process_tunneled_msg_cb;
	cbs->channel_scan_rpt = wbd_ieee1905_channel_scan_rpt_cb;
	cbs->ap_auto_config_resp = wbd_ieee1905_ap_auto_config_resp_cb;
	cbs->process_cac_complete = wbd_ieee1905_process_cac_complete_cb;
	cbs->process_cac_status = wbd_ieee1905_process_cac_status_cb;
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

/* Create Traffic separation policy from NVRAM */
static int wbd_master_fill_vlan_policy(ieee1905_config *config, uint16 sec_vlan_id)
{
	unsigned short vlan_id;
	unsigned char map;
	char name[NVRAM_MAX_VALUE_LEN], *nvval, *next = NULL, prefix[NVRAM_MAX_VALUE_LEN+2] = {0};
	ieee1905_ssid_type ssid;
	ieee1905_ts_policy_t *ts_policy = NULL;
	int is_guest_present = 0, ret = -1;
	WBD_ENTER();

	/* Read BSS info names */
	nvval = blanket_nvram_safe_get(NVRAM_MAP_BSS_NAMES);
	if (strlen(nvval) <= 0) {
		WBD_WARNING("NVRAM[%s] Not set. Cannot read BSS details\n", NVRAM_MAP_BSS_NAMES);
		goto end;
	}

	/* For each bss info names */
	foreach(name, nvval, next) {

		snprintf(prefix, sizeof(prefix), "%s_", name);

		map = (unsigned char)blanket_get_config_val_uint(prefix,
			NVRAM_MAP, 0);
		/* If its guest, set the variable */
		if (map & IEEE1905_MAP_FLAG_GUEST) {
			is_guest_present = 1;
		}

		vlan_id = (unsigned short)blanket_get_config_val_uint(prefix,
			NVRAM_MAP_VLAN_ID, 0);
		/* If VLAN ID is not specified use default VLAN ID's. For non guest, use primary
		 * vlan id else use secondary vlan id
		 */
		if (vlan_id == 0) {
			if (map & IEEE1905_MAP_FLAG_GUEST) {
				vlan_id = sec_vlan_id;
			} else {
				vlan_id = config->prim_vlan_id;
			}
		}

		/* Add VLAN ID to policy list. this return the pointer to the policy if added
		 * successfully or if the VLAN ID is already present in the list
		 */
		WBD_INFO("VLAN ID[%d] for prefix[%s]\n", vlan_id, prefix);
		ts_policy = i5DmAddVLANIDToList(&config->ts_policy_list, vlan_id);
		if (ts_policy == NULL) {
			WBD_WARNING("For prefix[%s] Failed to add VLAN ID[%d] to list\n",
				prefix, vlan_id);
			goto end;
		}

		memset(&ssid, 0, sizeof(ssid));
		nvval = blanket_nvram_prefix_safe_get(prefix, NVRAM_SSID);
		memcpy(ssid.SSID, nvval, sizeof(ssid.SSID));
		ssid.SSID_len = strlen(nvval);

		/* Add SSID to the VLAN policy list */
		WBD_INFO("VLAN ID[%d] for prefix[%s] ssid[%s] len[%d]\n", vlan_id, prefix,
			ssid.SSID, ssid.SSID_len);
		if (i5DmAddSSIDToList(&config->ts_policy_list, &ts_policy->ssid_list,
			&ssid) == NULL) {
			WBD_WARNING("For VLAN ID[%d] prefix[%s] Failed to add SSID[%s] of len[%d] "
				"to list\n", vlan_id, prefix, ssid.SSID, ssid.SSID_len);
			goto end;
		}
	}

	if (is_guest_present) {
		config->flags |= I5_INIT_FLAG_TS_ACTIVE;
		WBD_INFO("Guest Enabled. Primary VLAN ID[%d] Default PCP[%d] Flags[%x]\n\n",
			config->prim_vlan_id, config->default_pcp, config->flags);
		ret = 0;
	} else {
		WBD_INFO("Guest is not enabled\n");
	}

end:
	WBD_EXIT();
	return ret;
}

/* Read the VLAN(Traffic separation) configuration */
static void
wbd_master_read_vlan_config(ieee1905_config *config)
{
	int ret = -1;
	uint16 sec_vlan_id;
	WBD_ENTER();

	memset(config, 0, sizeof(*config));

	ieee1905_glist_init(&config->ts_policy_list);

	wbd_get_basic_common_vlan_config(config, &sec_vlan_id);

	if (config->flags & I5_INIT_FLAG_TS_SUPPORTED) {
		ret = wbd_master_fill_vlan_policy(config, sec_vlan_id);
	}

	if (ret != WBDE_OK) {
		i5DmTSPolicyCleanup(&config->ts_policy_list);
		WBD_INFO("Failed to load traffic separation policy either due to guest is not "
			"enabled or failed to read the data\n");
	}

	WBD_EXIT();
}

/* main entry point */
int
main(int argc, char *argv[])
{
	int ret = WBDE_OK;
	char wbd_ifnames[NVRAM_MAX_VALUE_LEN] = {0};
	int map_mode;
	ieee1905_call_bks_t cbs;
	ieee1905_config config;
	char fname[WBD_MAX_BUF_256];

	/* Set Process Name */
	WBDSTRNCPY(g_wbd_process_name, "Master", sizeof(g_wbd_process_name));

	/* Copy and set file name */
	blanket_log_get_default_nvram_filename(fname, sizeof(fname));

	/* Filename exists remove to avoid duplication */
	if (remove(fname) != 0) {
		WBD_WARNING("Unable to delete %s: %s...\n", fname, strerror(errno));
	}

	map_mode = blanket_get_config_val_int(NULL, WBD_NVRAM_MULTIAP_MODE, MAP_MODE_FLAG_DISABLED);

	/* If the mode is not controller, the master exe should exit */
	if (!MAP_IS_CONTROLLER(map_mode)) {
		WBD_WARNING("Multi-AP Mode (%d) not configured as controller..\n", map_mode);
		goto end;
	}

	memset(&cbs, 0, sizeof(cbs));

	WBD_DEBUG("Starting wbd_master daemon...\n");

	/* Parse common cli arguments */
	wbd_parse_cli_args(argc, argv);

	/* Provide necessary info to debug_monitor for service restart */
	dm_register_app_restart_info(getpid(), argc, argv, NULL);

	wbd_master_init_blanket_module();

	/* Give a dummy call to read wbd ifnames, so that it can set any FBT releated NVRAMs */
	wbd_read_actual_ifnames(wbd_ifnames, sizeof(wbd_ifnames), TRUE);
	WBD_INFO("Actual wbd_ifnames[%s]\n", wbd_ifnames);

	/* Allocate & Initialize the info structure */
	g_wbdinfo = wbd_info_init(&ret);
	WBD_ASSERT_MSG("wbd_info_init failed: %d (%s)\n", ret, wbderrorstr(ret));

	/* Allocate & Initialize Blanket Master structure object */
	ret = wbd_ds_blanket_master_init(g_wbdinfo);
	WBD_ASSERT_MSG("wbd_ds_blanket_master_init failed: %d (%s)\n", ret, wbderrorstr(ret));

	wbd_master_register_ieee1905_callbacks(&cbs);

	wbd_master_read_vlan_config(&config);

	ret = ieee1905_module_init(g_wbdinfo, MAP_MODE_FLAG_CONTROLLER,
		(MAP_IS_CONTROLLER(g_wbdinfo->map_mode) ? 1 : 0), &cbs, &config);
	WBD_ASSERT();
	/* If the guest is enabled, clean the traffic separation policy which is loaded to
	 * local variable
	 */
	if (config.flags & I5_INIT_FLAG_TS_ACTIVE) {
		i5DmTSPolicyCleanup(&config.ts_policy_list);
	}

	wbd_master_add_bssinfo_to_controller();

	/* Initialize Master */
	ret = wbd_init_master(g_wbdinfo);
	WBD_ASSERT_MSG("wbd_init_master failed: %d (%s)\n", ret, wbderrorstr(ret));

	/* WBD & 1905 are initialized properly. Now enable signal handlers */
	signal(SIGTERM, wbd_signal_hdlr);
	signal(SIGINT, wbd_signal_hdlr);
	signal(SIGPWR, wbd_master_tty_hdlr);

	blanket_start_multiap_messaging();

	/* Main loop which keeps on checking for the timers and fd's */
	wbd_run(g_wbdinfo->hdl);

end:
	/* Exit Master */
	wbd_exit_master(g_wbdinfo);
	WBD_DEBUG("Exited wbd_master daemon...\n");

	return ret;
}
