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
 * $Id: wbd_slave.c 781736 2019-11-27 09:57:31Z $
 */

#include <getopt.h>

#include "wbd.h"
#include "wbd_shared.h"
#include "wbd_ds.h"
#include "wbd_com.h"
#include "wbd_json_utility.h"
#include "wbd_sock_utility.h"
#include "wbd_wl_utility.h"
#include "wbd_blanket_utility.h"
#include "wbd_plc_utility.h"
#include "wbd_slave_control.h"
#include "wbd_slave_com_hdlr.h"
#include "blanket.h"
#include <common_utils.h>
#ifdef BCM_APPEVENTD
#include "wbd_appeventd.h"
#include "appeventd_wbd.h"
#endif /* BCM_APPEVENTD */

wbd_info_t *g_wbdinfo = NULL;
unsigned int wbd_msglevel = WBD_DEBUG_DEFAULT;
char g_wbd_process_name[WBD_MAX_PROCESS_NAME];

/* Read "wbd_ifnames" NVRAM and get actual ifnames */
extern int wbd_read_actual_ifnames(char *wbd_ifnames1, int len1, bool create);
/* Find First DWDS Primary Interface, with mode = STA */
extern int wbd_find_dwds_sta_primif(char *ifname, int len, char *ifname1, int len1);

/* Get Slave module info */
wbd_info_t *
wbd_get_ginfo()
{
	return (g_wbdinfo);
}

/* Gets Slave NVRAM settings */
static int
wbd_slave_retrieve_nvram_config(wbd_slave_item_t *slave)
{
	char *str, *prefix;
	int ret = WBDE_OK, num = 0;
	wbd_wc_thld_t threshold_cfg;
	WBD_ENTER();

	prefix = slave->wbd_ifr.prefix;
	slave->wc_info.wc_algo = blanket_get_config_val_int(prefix, WBD_NVRAM_WC_ALGO, 0);
	if (slave->wc_info.wc_algo >= wbd_get_max_wc_algo())
		slave->wc_info.wc_algo = 0;

	/* Read threshold index and prepare threshold config from predefined entries */
	slave->wc_info.wc_thld = blanket_get_config_val_int(prefix, WBD_NVRAM_WC_THLD_IDX, 0);
	if (slave->wc_info.wc_thld >= wbd_get_max_wc_thld())
		slave->wc_info.wc_thld = 0;
	memcpy(&slave->wc_info.wc_thld_cfg, wbd_get_wc_thld(slave),
		sizeof(slave->wc_info.wc_thld_cfg));

	/* read threshold from NVRAM */
	str = blanket_nvram_prefix_safe_get(prefix, WBD_NVRAM_WC_THLD);
	if (str) {
		num = sscanf(str, "%d %f %x",
			&threshold_cfg.t_rssi,
			&threshold_cfg.tx_rate,
			&threshold_cfg.flags);
		if (num == 3) {
			memcpy(&slave->wc_info.wc_thld_cfg, &threshold_cfg,
				sizeof(slave->wc_info.wc_thld_cfg));
		} else {
			WBD_WARNING("%s : %s%s = %s\n", wbderrorstr(WBDE_INV_NVVAL),
				prefix, WBD_NVRAM_WC_THLD, str);
		}
	}

	if (blanket_get_config_val_int(NULL, NVRAM_MAP_STA_WNM_CHECK, WBD_DEF_STA_WNM_CHECK)) {
		slave->parent->parent->flags |= WBD_INFO_FLAGS_STA_WNM_CHECK;
	}

	if (blanket_get_config_val_int(NULL, NVRAM_MAP_PER_CHAN_BCN_REQ,
		WBD_DEF_PER_CHAN_BCN_REQ)) {
		slave->parent->parent->flags |= WBD_INFO_FLAGS_PER_CHAN_BCN_REQ;
	}

	WBD_DEBUG("ifname[%s] prefix[%s] weak_sta_algo[%d] weak_sta_threshold[%d] %s rssi[%d] "
		"tx_rate[%f] flags[0x%X] slave_flags[0x%X] BSSID_Info[0x%x]\n",
		slave->wbd_ifr.ifr.ifr_name, prefix,
		slave->wc_info.wc_algo,
		slave->wc_info.wc_thld,
		WBD_NVRAM_WC_THLD,
		slave->wc_info.wc_thld_cfg.t_rssi,
		slave->wc_info.wc_thld_cfg.tx_rate,
		slave->wc_info.wc_thld_cfg.flags,
		slave->flags, slave->wbd_ifr.bssid_info);

	WBD_EXIT();
	return ret;
}

/* Based on info mode & uplink connectivity, Slave decides type common to all Slaves on a device */
static int
wbd_slave_identify_slave_type(wbd_info_t *info, wbd_slave_type_t *out_slave_type)
{
	int ret = WBDE_OK;
	char name[IFNAMSIZ] = {0}, name1[IFNAMSIZ] = {0};
	WBD_ENTER();

	/* Get the slave type */
	/* TODO : We just added only for ETHERNET, DWDS and PLC slaves. Needs to be
	 * done for MOCA interfaces
	 */

	/* Check if WBD Application Mode is Master */
	if (MAP_IS_CONTROLLER(info->map_mode)) {

		/* Slave Type is Master (PMSlave - Slave Running in Master) */
		*out_slave_type = WBD_SLAVE_TYPE_MASTER;
		goto end;
	}

	/* Find First DWDS Primary Interface, with mode = STA */
	ret = wbd_find_dwds_sta_primif(name, sizeof(name), name1, sizeof(name1));
	if (ret == WBDE_OK) {

		/* Slave Type is DWDS */
		*out_slave_type = WBD_SLAVE_TYPE_DWDS;
		goto end;
	}

#ifdef PLC_WBD
	/* Check if PLC is enabled */
	if (wbd_plc_is_enabled(&info->plc_info)) {

		/* Slave Type is PLC */
		*out_slave_type = WBD_SLAVE_TYPE_PLC;
		goto end;
	}
#endif /* PLC_WBD */

	/* Slave Type is Ethernet, By Default */
	*out_slave_type = WBD_SLAVE_TYPE_ETHERNET;

end:
	WBD_INFO("Slave Type[%d]\n", *out_slave_type);
	WBD_EXIT();
	return ret;
}

/* Fm driver, Fill Slave's Interface info to local ds */
static int
wbd_slave_fill_interface_info(wbd_info_t* info)
{
	int ret = WBDE_OK, in_band = WBD_BAND_LAN_2G, ncount = 0;
	wbd_slave_item_t *slave = NULL;
	char wbd_ifnames[NVRAM_MAX_VALUE_LEN] = {0};
	char name[IFNAMSIZ] = {0}, var_intf[IFNAMSIZ] = {0}, *next_intf;
	wbd_slave_type_t slave_type;
	WBD_ENTER();

	/* Based on info mode & uplink connectivity, Slave decides type common to all Slaves */
	wbd_slave_identify_slave_type(info, &slave_type);

	/* Read "wbd_ifnames" NVRAM, get actual ifnames */
	wbd_read_actual_ifnames(wbd_ifnames, sizeof(wbd_ifnames), TRUE);
	WBD_INFO("Actual wbd_ifnames[%s]\n", wbd_ifnames);

	/* Traverse wbd_ifnames for each ifname */
	foreach(var_intf, wbd_ifnames, next_intf) {

		/* Copy interface name temporarily */
		WBDSTRNCPY(name, var_intf, sizeof(name) - 1);

		/* Check if valid Wireless Interface and its Primary Radio is enabled or not */
		if (!blanket_is_interface_enabled(name, FALSE, &ret)) {
			continue; /* Skip non-Wireless & disabled Wireless Interface */
		}

		/* Allocate & Initialize Slave Info struct for this Band, add to br0_slave_list */
		ret = wbd_ds_create_slave_for_band(info->wbd_slave, in_band, &slave);
		if (ret != WBDE_OK) {
			continue; /* Skip Slave creation for Invalid Interface */
		}

		/* Copy interface name to WBD Interface Info */
		WBDSTRNCPY(slave->wbd_ifr.ifr.ifr_name, name, IFNAMSIZ - 1);

		/* Initialize wlif module handle. */
		slave->wlif_hdl = wl_wlif_init(info->hdl, slave->wbd_ifr.ifr.ifr_name,
			NULL, NULL);

		/* Get Interface details from driver */
		ncount = 0;
		while (ncount < WBD_N_RETRIES) {
			ret = wbd_wl_fill_interface_info(&slave->wbd_ifr);
			if (ret != WBDE_OK) {
				sleep(WBD_SLEEP_GETIFR_FAIL);
				ncount++;
				continue;
			}
			break;
		}

		/* Assign common Slave Type and Uplink MAC to all Slaves on this device */
		slave->slave_type = slave_type;
	}

	WBD_EXIT();

	/*
	 * This function can return WBDE_OK always. Even if wbd_wl_fill_interface_info() fails
	 * here, wbd_init_slave_item_timer_cb() function takes care of filling it at a later point
	 */
	return WBDE_OK;
}

/* Initialises slave item data structure */
void
wbd_init_slave_item_timer_cb(bcm_usched_handle *hdl, void *arg)
{
	int ret = WBDE_OK;
	unsigned char map;
	wbd_slave_item_t *slave = (wbd_slave_item_t *)arg;
	char *name;
	WBD_ENTER();

	if (!slave) {
		WBD_WARNING("Invalid slave\n");
		goto end;
	}

	name = slave->wbd_ifr.ifr.ifr_name;
	if (!slave->wbd_ifr.enabled &&
		(ret = wbd_wl_fill_interface_info(&slave->wbd_ifr)) != WBDE_OK) {
		WBD_DEBUG("Slave not enabled for interface [%s]. "
			"start a timer to check again\n", name);
		wbd_remove_timers(hdl, wbd_init_slave_item_timer_cb, slave);
		ret = wbd_add_timers(hdl, slave,
			WBD_SEC_MICROSEC(WBD_SLEEP_GETIFR_FAIL * 10),
			wbd_init_slave_item_timer_cb, 0);
		if (ret != WBDE_OK) {
			WBD_WARNING("Interval[%d] Failed to slave update timer\n",
				WBD_SLEEP_GETIFR_FAIL * 10);
		}
		goto end;
	}

	/* Gets NVRAM settings for this Slave */
	wbd_slave_retrieve_nvram_config(slave);

	/* Init sta traffic info for slave, if present */
	if (!wbd_slave_check_taf_enable(slave)) {
		WBD_DEBUG("NVRAM taf_enable is not enable \n");
	}
	/* Initialize stamon module for Slave's interface */
	ret = wbd_ds_slave_stamon_init(slave);
	WBD_ASSERT();

	/* Get MAP */
	map = wbd_get_map_flags(slave->wbd_ifr.prefix);
	WBD_INFO("map: %d prefix: [%s]\n", map, slave->wbd_ifr.prefix);

	/* Add this BSS to the multiAP library */
	ieee1905_add_bss((unsigned char *)&slave->wbd_ifr.radio_mac,
		(unsigned char *)&slave->wbd_ifr.bssid,
		slave->wbd_ifr.blanket_ssid.SSID, slave->wbd_ifr.blanket_ssid.SSID_len,
		slave->wbd_ifr.chanspec, name, map);

	/* Add all the associated clients */
	blanket_add_all_associated_stas(name, &slave->wbd_ifr.bssid);

	/* Identify and Update the band in */
	ret = wbd_slave_update_band_type(name, slave);

	WBD_CHECK_MSG("Band[%d] ifname[%s] Slave["MACF"]. Faield to get chan info. %s\n",
		slave->band, name, ETHER_TO_MACF(slave->wbd_ifr.mac), wbderrorstr(ret));

#ifdef PLC_WBD
	if (slave_type == WBD_SLAVE_TYPE_PLC) {
		wbd_plc_get_local_plc_mac(&slave->parent->parent->plc_info,
			&slave->wbd_ifr.plc_mac);
	}
#endif /* PLC_WBD */

	WBD_INFO("Interface Info read success : ifname[%s] Slave["MACF"]\n",
		name, ETHER_TO_MACF(slave->wbd_ifr.mac));
end:
	WBD_EXIT();
}

/* Read blanket slave NVRAMs */
static void
wbd_read_blanket_slave_nvrams(wbd_blanket_slave_t *bkt_slave)
{
	/* Get NVRAM : AP auto configuration search threshold */
	bkt_slave->n_ap_auto_config_search_thld = (uint8)blanket_get_config_val_uint(NULL,
		WBD_NVRAM_AP_CONFIG_SEARCH_THLD, WBD_DEF_AP_CONFIG_SEARCH_THLD);

	WBD_INFO("AP Auto Config Search Treshold[%d]\n", bkt_slave->n_ap_auto_config_search_thld);
}

/* Initialize the slave module */
static int
wbd_init_slave(wbd_info_t *info)
{
	int ret = WBDE_OK;
	wbd_slave_item_t *slave = NULL;
	dll_t *slave_item_p;
	WBD_ENTER();

#ifdef PLC_WBD
	/* Init PLC interface */
	wbd_plc_init(&info->plc_info);
#endif /* PLC_WBD */

	/* Read backhual NVRAM config */
	wbd_retrieve_backhaul_nvram_config(&info->wbd_slave->metric_policy_bh);
	blanket_nvram_prefix_set(NULL, NVRAM_MAP_AGENT_CONFIGURED, "0");

	/* Read blanket slave NVRAM config */
	wbd_read_blanket_slave_nvrams(info->wbd_slave);

	/* Fm driver, Fill Slave's Interface info to local ds */
	ret = wbd_slave_fill_interface_info(info);
	WBD_ASSERT_MSG("Failed to get interface info... Aborting...\n");

	/* Try to open the server FD */
	info->server_fd = wbd_try_open_server_fd(EAPD_WKSP_WBD_TCP_SLAVE_PORT, &ret);
	WBD_ASSERT_MSG("Failed to create server socket\n");

	/* Try to open the server FD for CLI */
	info->cli_server_fd = wbd_try_open_server_fd(EAPD_WKSP_WBD_TCP_SLAVECLI_PORT, &ret);
	WBD_ASSERT_MSG("Failed to create CLI socket\n");

	/* Try to open the Event FD and add it to scheduler */
	info->event_fd = wbd_try_open_event_fd(EAPD_WKSP_WBD_UDP_SPORT, &ret);
	WBD_ASSERT_MSG("Failed to create event socket for all the events from driver\n");
	ret = wbd_add_fd_to_schedule(info->hdl, info->event_fd,
		info, wbd_slave_process_event_fd_cb);
	WBD_ASSERT();

	/* Try to open the Event FD for getting only BSS transition response */
	info->event_steer_fd = wbd_try_open_event_fd(EAPD_WKSP_WBD_UDP_MPORT, &ret);
	WBD_ASSERT_MSG("Failed to create event socket for BSS transition response\n");

	/* Initialize the communication module for slave */
	ret = wbd_init_slave_com_handle(info);
	WBD_ASSERT();

	/* Traverse br0 Slave Item List for this Blanket Slave for each band */
	foreach_glist_item(slave_item_p, info->wbd_slave->br0_slave_list) {
		/* Choose Slave Info based on band */
		slave = (wbd_slave_item_t*)slave_item_p;
		wbd_init_slave_item_timer_cb(info->hdl, slave);
	}
	ret = wbd_add_timers(info->hdl, NULL, WBD_SEC_MICROSEC(info->max.tm_update_chspec),
		wbd_slave_update_chanspec_timer_cb, 1);
	if (ret != WBDE_OK) {
		WBD_WARNING("Interval[%d] Failed to create update chanspec timer\n",
			info->max.tm_update_chspec);
	}

end:
	WBD_EXIT();
	return ret;
}

/* Callback from IEEE 1905 module to get interface info */
static int
wbd_slave_get_interface_info_cb(char *ifname, ieee1905_ifr_info *info)
{
	int ret = WBDE_OK;
	char prefix[IFNAMSIZ];
	wl_bss_info_t *bss_info;
	blanket_ap_caps_t ap_caps;
	chanspec_t chanspec;
	WBD_ENTER();

	memset(&ap_caps, 0, sizeof(ap_caps));

	/* Get Prefix from OS specific interface name */
	blanket_get_interface_prefix(ifname, prefix, sizeof(prefix));
	info->mapFlags = wbd_get_map_flags(prefix);

	/* Check interface (for valid BRCM wl interface) */
	(void)blanket_probe(ifname);

	blanket_get_chanspec(ifname, &chanspec);
	info->chanspec = (unsigned short)chanspec;

	/* If its STA interface get the BSSID to fill in the media specific info */
	if (I5_IS_BSS_STA(info->mapFlags)) {
		if (blanket_get_bssid(ifname, (struct ether_addr*)info->bssid) == WBDE_OK) {
			if (!(ETHER_ISNULLADDR((struct ether_addr*)info->bssid))) {
				wbd_slave_store_bssid_nvram(prefix, info->bssid, 0);
			}
		}
	}

	/* Get BSS Info */
	ret = blanket_get_bss_info(ifname, &bss_info);
	WBD_ASSERT();

	/* Get HT capabilities */
	(void)blanket_get_ht_cap(ifname, bss_info, &ap_caps.ht_caps);
	info->ap_caps.HTCaps = ap_caps.ht_caps;

	/* Get VHT capabilities */
	(void)blanket_get_vht_cap(ifname, bss_info, &ap_caps.vht_caps);
	memcpy(&info->ap_caps.VHTCaps, &ap_caps.vht_caps, sizeof(info->ap_caps.VHTCaps));
	info->ap_caps.RadioCaps.maxBSSSupported = (unsigned char)blanket_get_config_val_int(NULL,
		NVRAM_MAP_MAX_BSS, WBD_MAX_BSS_SUPPORTED);

	/* Get HE capabilities */
	(void)blanket_get_he_cap(ifname, bss_info, &ap_caps.he_caps);
	memcpy(&info->ap_caps.HECaps, &ap_caps.he_caps, sizeof(info->ap_caps.HECaps));

	/* Allocate buffer for getting List of operating class */
	ap_caps.radio_caps.list = (uint8 *)wbd_malloc(WBD_MAX_BUF_512, &ret);
	WBD_ASSERT();

	/* Fill radio capability list. For multi channel use center channel else use control
	 * channel
	 */
	(void)blanket_get_radio_cap(ifname, &ap_caps.radio_caps, WBD_MAX_BUF_512);

	/* Free buffer for getting List of operating class, in case of error */
	if (ap_caps.radio_caps.list && !ap_caps.radio_caps.valid) {
		free(ap_caps.radio_caps.list);
		ap_caps.radio_caps.list = NULL;
		info->ap_caps.RadioCaps.Valid = 0;
	} else {
		info->ap_caps.RadioCaps.Valid = 1;
		info->ap_caps.RadioCaps.List = ap_caps.radio_caps.list;
		info->ap_caps.RadioCaps.Len = ap_caps.radio_caps.len;
	}

end:
	WBD_EXIT();
	return ret;
}

/* Exit the slave module */
void
wbd_exit_slave(wbd_info_t *info)
{
	int ret = WBDE_OK;
	WBD_ENTER();

	BCM_REFERENCE(ret);
	WBD_ASSERT_ARG(info, WBDE_INV_ARG);

	blanket_nvram_prefix_set(NULL, NVRAM_MAP_AGENT_CONFIGURED, "0");
	/* Set flag to mark it as application is closing */
	info->flags |= WBD_INFO_FLAGS_CLOSING_APP;

	/* Deinit IEEE1905 module only if it is initialized. Because, if the WBD is disabled,
	 * it will not get initialized
	 */
	if (WBD_IEEE1905_INIT(info->flags)) {
		ieee1905_deinit();
	}
	wbd_ds_blanket_slave_cleanup(info);
	wbd_com_deinit(info->com_cli_hdl);
	wbd_com_deinit(info->com_serv_hdl);
	wbd_info_cleanup(info);

end:
	WBD_EXIT();
}

/* Signal handler to display tty output. */
void
wbd_slave_tty_hdlr(int sig)
{
	BCM_REFERENCE(sig);

	wbd_tty_hdlr(WBD_SLAVE_FILE_TTY);
}

/* Signal handler */
void
wbd_signal_hdlr(int sig)
{
	WBD_ERROR("Signal : %d WBD going to shutdown\n", sig);
	wbd_exit_slave(g_wbdinfo);
	exit(1);
}

/* Callback from IEEE 1905 module when the agent gets STEER request */
static void
wbd_ieee1905_steer_request_cb(ieee1905_steer_req *steer_req, ieee1905_vendor_data *in_vndr_tlv)
{
	wbd_slave_process_map_steer_request_cb(g_wbdinfo, steer_req, in_vndr_tlv);
}

/* Callback from IEEE 1905 module when the agent gets block/unblock STA request */
static void
wbd_ieee1905_block_unblock_sta_request_cb(ieee1905_block_unblock_sta *block_unblock_sta)
{
	wbd_slave_process_map_block_unblock_sta_request_cb(g_wbdinfo, block_unblock_sta);
}

/* Callback from IEEE 1905 module to prepare channel preference report */
static void
wbd_ieee1905_prepare_channel_pref_cb(i5_dm_interface_type *i5_intf,
	ieee1905_chan_pref_rc_map_array *cp)
{
	wbd_slave_prepare_local_channel_preference(i5_intf, cp);
}

/* Callback from IEEE 1905 module when the agent gets channel Selection request */
static void
wbd_ieee1905_recv_chan_selection_request_cb(unsigned char *al_mac, unsigned char *interface_mac,
	ieee1905_chan_pref_rc_map_array *cp, unsigned char rclass_local_count,
	ieee1905_chan_pref_rc_map *local_chan_pref)
{
	wbd_slave_process_chan_selection_request_cb(g_wbdinfo, al_mac, interface_mac, cp,
		rclass_local_count, local_chan_pref);
}
/* Callback from IEEE 1905 module when the agent gets channel Selection request */
static void
wbd_ieee1905_send_opchannel_report_cb(void)
{
	wbd_slave_send_opchannel_reports();
}

/* Callback from IEEE 1905 module when the agent gets Tx power limit */
static void
wbd_ieee1905_set_tx_power_limit_cb(char *ifname, unsigned char tx_power_limit)
{
	wbd_slave_process_set_tx_power_limit_cb(g_wbdinfo, ifname, tx_power_limit);
}

/* Callback from IEEE 1905 module to get the backhual link metric */
static int
wbd_ieee1905_get_backhaul_link_metric_cb(char *ifname, unsigned char *interface_mac,
	ieee1905_backhaul_link_metric *metric)
{
	return (wbd_slave_process_get_backhaul_link_metric_cb(g_wbdinfo, ifname,
		interface_mac, metric));
}

/* Callback from IEEE 1905 module to get the interface metrics */
static int
wbd_ieee1905_get_interface_metric_cb(char *ifname, unsigned char *ifr_mac,
	ieee1905_interface_metric *metric)
{
	WBD_DEBUG("Received callback: Ifname[%s] Interface["MACDBG"] Interface Metrics\n",
		ifname, MAC2STRDBG(ifr_mac));
	return (wbd_slave_process_get_interface_metric_cb(g_wbdinfo, ifname, ifr_mac, metric));
}

/* Callback from IEEE 1905 module to get the AP metrics */
static int
wbd_ieee1905_get_ap_metric_cb(char *ifname, unsigned char *bssid, ieee1905_ap_metric *metric)
{
	WBD_DEBUG("Received callback: Ifname[%s] BSS["MACDBG"] AP Metrics\n",
		ifname, MAC2STRDBG(bssid));
	return (wbd_slave_process_get_ap_metric_cb(g_wbdinfo, ifname, bssid, metric));
}

/* Callback from IEEE 1905 module to get the associated STA link metrics and STA traffic stats */
static int
wbd_ieee1905_get_assoc_sta_metric_cb(char *ifname, unsigned char *bssid, unsigned char *sta_mac,
	ieee1905_sta_link_metric *metric, ieee1905_sta_traffic_stats *traffic_stats,
	ieee1905_vendor_data *out_vndr_tlv)
{
	WBD_DEBUG("Received callback: Ifname[%s] BSS["MACDBG"] STA["MACDBG"] Assoc STA Metrics\n",
		ifname, MAC2STRDBG(bssid), MAC2STRDBG(sta_mac));
	return (wbd_slave_process_get_assoc_sta_metric(ifname, bssid, sta_mac,
		metric, traffic_stats, out_vndr_tlv));
}

/* Callback from IEEE 1905 module to get the un associated STA link metrics */
static int
wbd_ieee1905_get_unassoc_sta_metric_cb(ieee1905_unassoc_sta_link_metric_query *query)
{
	return (wbd_slave_process_get_unassoc_sta_metric_cb(g_wbdinfo, query));
}

/* Callback from IEEE 1905 module to deinit the device */
static void
wbd_ieee1905_device_deinit_cb(i5_dm_device_type *i5_device)
{
	i5_dm_device_type *self_device = i5DmGetSelfDevice();
	WBD_ENTER();

	WBD_DEBUG("Received callback: Device["MACDBG"] %s got removed\n",
		MAC2STRDBG(i5_device->DeviceId),
		I5_IS_MULTIAP_CONTROLLER(i5_device->flags) ? "Controller" : "Agent");

	/* If the device being removed is controller and the self device is not the agent
	 * running on the controller and not upstream AP and the device is getting removed when the
	 * application is not closing, then only disable the backhaul BSS
	 */
	if (!WBD_CLOSING_APP(g_wbdinfo->flags) && !I5_IS_CTRLAGENT(self_device->flags) &&
		!WBD_UAP_ENAB(g_wbdinfo->flags) && I5_IS_MULTIAP_CONTROLLER(i5_device->flags)) {
		WBD_ERROR("Controller device "MACDBG" removed. Blocking backhaul BSS\n",
			MAC2STRDBG(i5_device->DeviceId));
		wbd_slave_block_unblock_backhaul_sta_assoc(0);
	}

	WBD_EXIT();
}

/* Callback from IEEE 1905 module to init the interface on device */
static void
wbd_ieee1905_interface_init_cb(i5_dm_interface_type *i5_ifr)
{
	i5_dm_device_type *i5_device;

	if (!i5_ifr) {
		return;
	}

	/* If not self device, dont init */
	i5_device = (i5_dm_device_type*)WBD_I5LL_PARENT(i5_ifr);
	if (!i5DmDeviceIsSelf(i5_device->DeviceId)) {
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

/* Callback from IEEE 1905 module to init the BSS on interface */
static void
wbd_ieee1905_bss_init_cb(i5_dm_bss_type *pbss)
{
	wbd_ds_bss_init(pbss);

	/* Add static neighbor list entry of this bss */
	wbd_slave_add_bss_nbr(pbss);
}

/* Callback from IEEE 1905 module to deinit the BSS on interface */
static void
wbd_ieee1905_bss_deinit_cb(i5_dm_bss_type *pbss)
{
	wbd_ds_bss_deinit(pbss);

	/* Delete static neighbor list entry */
	wbd_slave_del_bss_nbr(pbss);
}

/* Callback from IEEE 1905 module to Create the BSS on interface */
static int
wbd_ieee1905_create_bss_on_ifr_cb(char *ifname,
	ieee1905_glist_t *bssinfo_list)
{
	return (wbd_slave_create_bss_on_ifr(g_wbdinfo, ifname, bssinfo_list));
}

/* Callback from IEEE 1905 module to get interface info */
static int
wbd_ieee1905_get_interface_info_cb(char *ifname, ieee1905_ifr_info *info)
{
	return (wbd_slave_get_interface_info_cb(ifname, info));
}

/* Callback from IEEE 1905 module when the agent gets backhaul STEER request */
static void
wbd_ieee1905_bh_steer_request_cb(char *ifname, ieee1905_backhaul_steer_msg *bh_steer_req)
{
	WBD_DEBUG("Received callback: Backhaul STA["MACDBG"] is moving to BSSID["MACDBG"]\n",
		MAC2STRDBG(bh_steer_req->bh_sta_mac), MAC2STRDBG(bh_steer_req->trgt_bssid));
	wbd_slave_process_bh_steer_request_cb(g_wbdinfo, ifname, bh_steer_req);
}

/* Callback from IEEE 1905 module when the agent gets beacon metrics query */
static int
wbd_ieee1905_beacon_metrics_query_cb(char *ifname, unsigned char *bssid,
	ieee1905_beacon_request *query)
{
	WBD_DEBUG("Received callback: Ifname[%s] BSS["MACDBG"] STA["MACDBG"]Beacon metrics query\n",
		ifname, MAC2STRDBG(bssid), MAC2STRDBG(query->sta_mac));
	if (WBD_PER_CHAN_BCN_REQ(g_wbdinfo->flags))
		return wbd_slave_process_per_chan_beacon_metrics_query_cb(g_wbdinfo, ifname,
			bssid, query);
	else
		return wbd_slave_process_beacon_metrics_query_cb(g_wbdinfo, ifname, bssid, query);
}

/* Callback from IEEE 1905 module when the agent gets Multi-AP Policy configuration */
static void
wbd_ieee1905_policy_configuration_cb(ieee1905_policy_config *policy, unsigned short rcvd_policies,
	ieee1905_vendor_data *in_vndr_tlv)
{
	WBD_DEBUG("Received callback: Multi-AP Policy Configuration\n");
	return (wbd_slave_policy_configuration_cb(g_wbdinfo, policy, rcvd_policies, in_vndr_tlv));
}

/* Callback from IEEE 1905 module when the STA got added in the data model */
static void
wbd_ieee1905_sta_added_cb(i5_dm_clients_type *i5_assoc_sta)
{
	i5_dm_bss_type *i5_bss;
	i5_dm_interface_type *i5_ifr;
	i5_dm_device_type *i5_device;

	WBD_DEBUG("Received callback: STA["MACDBG"] got added\n", MAC2STRDBG(i5_assoc_sta->mac));
	i5_bss = (i5_dm_bss_type*)WBD_I5LL_PARENT(i5_assoc_sta);

	wbd_ds_sta_init(g_wbdinfo, i5_assoc_sta);

	/* If the BSS is backhaul BSS, then its a backhaul STA. Create the bakchual STA weak
	 * client watch dog timer if not exists
	 */
	if (I5_CLIENT_IS_BSTA(i5_assoc_sta)) {
		i5_ifr = (i5_dm_interface_type*)WBD_I5LL_PARENT(i5_bss);
		i5_device = (i5_dm_device_type*)WBD_I5LL_PARENT(i5_ifr);
		if (i5_device == i5DmGetSelfDevice()) {
			WBD_INFO("Backhaul ifname[%s] STA["MACDBG"] Added. Create Weakclient "
				"watchdog timer\n", i5_bss->ifname, MAC2STRDBG(i5_assoc_sta->mac));
			wbd_slave_create_backhaul_sta_weak_client_watchdog_timer(g_wbdinfo);
		}
	}
}

/* Callback from IEEE 1905 module when the STA got removed from the data model */
static void
wbd_ieee1905_sta_removed_cb(i5_dm_clients_type *i5_assoc_sta)
{
	WBD_DEBUG("Received callback: STA["MACDBG"] got Removed\n", MAC2STRDBG(i5_assoc_sta->mac));
	wbd_ds_remove_beacon_report(g_wbdinfo, (struct ether_addr*)&(i5_assoc_sta->mac));
	return (wbd_ds_sta_deinit(i5_assoc_sta));
}

/* Callback from IEEE 1905 module to inform AP auto configuration */
static void
wbd_ieee1905_ap_configured_cb(unsigned char *al_mac, unsigned char *radio_mac,
	int if_band)
{
	WBD_DEBUG("Received callback: AP autoconfigured on Device ["MACDBG"] Radio["MACDBG"]"
		"in band [%d]\n", MAC2STRDBG(al_mac), MAC2STRDBG(radio_mac), if_band);
	/* Check if M2 is received for all Wireless Interfaces */
	if (i5DmIsAllInterfacesConfigured()) {

		WBD_INFO("Received callback: M2 is received for all Wireless Interfaces, "
			"Create all_ap_configured timer\n");

		if ((wbd_ds_is_fbt_possible_on_agent() == WBDE_DS_FBT_NT_POS) &&
			(!(g_wbdinfo->flags & WBD_INFO_FLAGS_RC_RESTART))) {
#ifdef BCM_APPEVENTD
			/* Raise and send MAP init end event to appeventd. */
			wbd_appeventd_map_init(APP_E_WBD_SLAVE_MAP_INIT_END,
				(struct ether_addr*)ieee1905_get_al_mac(), MAP_INIT_END,
				MAP_APPTYPE_SLAVE);
#endif /* BCM_APPEVENTD */
			blanket_nvram_prefix_set(NULL, NVRAM_MAP_AGENT_CONFIGURED, "1");
		}
		wbd_slave_create_all_ap_configured_timer();
	}
}

/* Callback from IEEE 1905 module when the Controller wants to Process vendor specific message */
static int
wbd_ieee1905_process_vendor_specific_msg_cb(ieee1905_vendor_data *msg_data)
{
	WBD_DEBUG("Received ieee1905 callback: Process vendor specific message\n");
	return wbd_slave_process_vendor_specific_msg(g_wbdinfo, msg_data);
}

/* Callback from IEEE 1905 module to Get Vendor Specific TLV to send */
static void
wbd_ieee1905_get_vendor_specific_tlv_cb(i5_msg_types_with_vndr_tlv_t msg_type,
	ieee1905_vendor_data *out_vendor_tlv)
{
	WBD_DEBUG("Received callback: Get Vendor Specific TLV\n");
	return wbd_slave_get_vendor_specific_tlv(msg_type, out_vendor_tlv);
}

/* Callback from IEEE 1905 module to indicate channel change of interface on device */
static void
wbd_ieee1905_interface_chan_change_cb(i5_dm_interface_type *i5_ifr)
{
	i5_dm_device_type *i5_device;

	if (!i5_ifr) {
		WBD_ERROR("ifr NULL \n");
		return;
	}

	/* If self device, dont add neighbors */
	i5_device = (i5_dm_device_type*)WBD_I5LL_PARENT(i5_ifr);
	if (i5DmDeviceIsSelf(i5_device->DeviceId)) {
		return;
	}

	/* Add static neighbor entry of this interface bss's with updated channel */
	wbd_slave_add_ifr_nbr(i5_ifr, FALSE);
}

/* Callback from IEEE 1905 module to inform 1905 AP-Autoconfiguration Response message from
 * controller
 */
static void
wbd_ieee1905_ap_auto_config_resp_cb(i5_dm_device_type *i5_device)
{
	int ret = WBDE_OK;
	wbd_blanket_slave_t *wbd_slave;
	i5_dm_device_type *self_device = i5DmGetSelfDevice();
	WBD_ENTER();

	/* Enable all the backhaul BSS as controller is detected */
	WBD_DEBUG("Received callback: 1905 AP-Autoconfiguration Response from Device["MACDBG"]\n",
		MAC2STRDBG(i5_device->DeviceId));
	WBD_ASSERT_ARG(g_wbdinfo->wbd_slave, WBDE_INV_ARG);

	wbd_slave = (wbd_blanket_slave_t*)g_wbdinfo->wbd_slave;

	/* If the device being removed is controller and the self device is not the agent
	 * running on the controller, then only enable the backhaul BSS
	 */
	if (!I5_IS_CTRLAGENT(self_device->flags) && I5_IS_MULTIAP_CONTROLLER(i5_device->flags)) {
		WBD_INFO("Controller device "MACDBG" detected. Unblocking backhaul BSS\n",
			MAC2STRDBG(i5_device->DeviceId));
			wbd_slave_block_unblock_backhaul_sta_assoc(1);
	}
	wbd_slave->n_ap_auto_config_search = 0;

end:
	WBD_EXIT();
}

/* Callback from IEEE 1905 module to inform 1905 AP-Autoconfiguration search message sent */
static void
wbd_ieee1905_ap_auto_config_search_sent()
{
	int ret = WBDE_OK;
	wbd_blanket_slave_t *wbd_slave;
	i5_dm_device_type *self_device = i5DmGetSelfDevice();
	WBD_ENTER();

	/* Enable all the backhaul BSS as controller is detected */
	WBD_DEBUG("Received callback: 1905 AP-Autoconfiguration Search Sent\n");
	WBD_ASSERT_ARG(g_wbdinfo->wbd_slave, WBDE_INV_ARG);

	wbd_slave = (wbd_blanket_slave_t*)g_wbdinfo->wbd_slave;

	wbd_slave->n_ap_auto_config_search++;

	/* If the count of AP auto configuration search exceeds max limit block the backhaul BSS */
	if (!I5_IS_CTRLAGENT(self_device->flags) &&
		(wbd_slave->n_ap_auto_config_search > wbd_slave->n_ap_auto_config_search_thld)) {
		WBD_ERROR("AP-Autoconfiguration search count %d exceeds threshold of %d. Assuming "
			"controller not reachable and blocking backhaul BSS\n",
			wbd_slave->n_ap_auto_config_search,
			wbd_slave->n_ap_auto_config_search_thld);
		wbd_slave_block_unblock_backhaul_sta_assoc(0);
	}

end:
	WBD_EXIT();
}

/* Check if the STA is associated or not */
static int
wbd_blanket_is_sta_associated(char *ifname)
{
	struct ether_addr bssid;

	/* If the BSSID is not NULL for a interface, then it is connected to upstrem AP */
	if (blanket_get_bssid(ifname, (struct ether_addr*)&bssid) == WBDE_OK) {
		if (!(ETHER_ISNULLADDR((struct ether_addr*)&bssid))) {
			WBD_INFO("ifname[%s] is associated to BSSID["MACF"]\n", ifname,
				ETHER_TO_MACF(bssid));
			return 1;
		}
	}

	return 0;
}

static void
wbd_ieee1905_set_bh_sta_params(t_i5_bh_sta_cmd cmd)
{
	int ret = WBDE_OK;
	i5_dm_device_type *i5_self_device;
	i5_dm_interface_type *i5_iter_ifr;
	wbd_ifr_item_t *ifr_vndr_data;
	char tmp[256];
	WBD_ENTER();

	WBD_SAFE_GET_I5_SELF_DEVICE(i5_self_device, &ret);

	foreach_i5glist_item(i5_iter_ifr, i5_dm_interface_type,
		i5_self_device->interface_list) {
		if (!I5_IS_BSS_STA(i5_iter_ifr->mapFlags)) {
			continue;
		}
		WBD_ASSERT_ARG(i5_iter_ifr->vndr_data, WBDE_INV_ARG);
		ifr_vndr_data = (wbd_ifr_item_t*)i5_iter_ifr->vndr_data;

		switch (cmd) {
			case IEEE1905_BH_STA_ROAM_DISB_VAP_UP:
				WBD_INFO("Disable backhaul STA("MACF") roaming and up virtual "
					"APs even if the backhaul STA is not associated\n",
					ETHERP_TO_MACF(i5_iter_ifr->InterfaceId));
				blanket_set_keep_ap_up(i5_iter_ifr->ifname, 1); /* VAP UP */
				if (WBD_IS_HAPD_ENABLED(wbd_get_ginfo()->flags)) {
					/* If hostapd is enabled, roaming in firmware is disabled
					 * from wlconf. Actual raoming will be handled by
					 * wpa_supplicant. So here raoming in wpa_supplicant
					 * should be disabled
					 */
					snprintf(tmp, sizeof(tmp), "wpa_cli -p %s/%s_wpa_supplicant"
						" -i %s ap_scan 0", WBD_HAPD_SUPP_CTRL_DIR,
						i5_iter_ifr->wlParentName, i5_iter_ifr->ifname);
					system(tmp);
					WBD_INFO("%s\n", tmp);
					ifr_vndr_data->flags |= WBD_FLAGS_IFR_AP_SCAN_DISABLED;
				} else {
					/* ROAM OFF IOVAR - disable roaming */
					blanket_set_roam_off(i5_iter_ifr->ifname, 1);
				}
				break;
			case IEEE1905_BH_STA_ROAM_ENAB_VAP_FOLLOW:
				WBD_INFO("Enable backhaul STA("MACF") roaming and up virtual "
					"APs up only if the backhaul STA is associated\n",
					ETHERP_TO_MACF(i5_iter_ifr->InterfaceId));
				blanket_set_keep_ap_up(i5_iter_ifr->ifname, 0); /* VAP Follow */
				if (WBD_IS_HAPD_ENABLED(wbd_get_ginfo()->flags)) {
					snprintf(tmp, sizeof(tmp), "wpa_cli -p %s/%s_wpa_supplicant"
						" -i %s ap_scan 1", WBD_HAPD_SUPP_CTRL_DIR,
						i5_iter_ifr->wlParentName, i5_iter_ifr->ifname);
					system(tmp);
					WBD_INFO("%s\n", tmp);
					ifr_vndr_data->flags &= ~WBD_FLAGS_IFR_AP_SCAN_DISABLED;
					/* Perform scan only if the STA is not associated */
					if (!wbd_blanket_is_sta_associated(i5_iter_ifr->ifname)) {
						snprintf(tmp, sizeof(tmp), "wpa_cli -p "
							"%s/%s_wpa_supplicant -i %s scan",
							WBD_HAPD_SUPP_CTRL_DIR,
							i5_iter_ifr->wlParentName,
							i5_iter_ifr->ifname);
						system(tmp);
						WBD_INFO("%s\n", tmp);
					}
				} else {
					/* ROM OFF IOVAR - enable roaming */
					blanket_set_roam_off(i5_iter_ifr->ifname, 0);
				}
				break;
			default:
				WBD_ERROR("Unknown backhaul STA configuration cmd: %d\n", cmd);
				break;
		}
	}
end:
	WBD_EXIT();
}

/* Initialize ieee 1905 module */
static void
wbd_slave_register_ieee1905_callbacks()
{
	ieee1905_call_bks_t cbs;

	memset(&cbs, 0, sizeof(cbs));
	cbs.steer_req = wbd_ieee1905_steer_request_cb;
	cbs.block_unblock_sta_req = wbd_ieee1905_block_unblock_sta_request_cb;
	cbs.prepare_channel_pref = wbd_ieee1905_prepare_channel_pref_cb;
	cbs.recv_chan_selection_req = wbd_ieee1905_recv_chan_selection_request_cb;
	cbs.send_opchannel_rpt = wbd_ieee1905_send_opchannel_report_cb;
	cbs.set_tx_power_limit = wbd_ieee1905_set_tx_power_limit_cb;
	cbs.backhaul_link_metric = wbd_ieee1905_get_backhaul_link_metric_cb;
	cbs.interface_metric = wbd_ieee1905_get_interface_metric_cb;
	cbs.ap_metric = wbd_ieee1905_get_ap_metric_cb;
	cbs.assoc_sta_metric = wbd_ieee1905_get_assoc_sta_metric_cb;
	cbs.unassoc_sta_metric = wbd_ieee1905_get_unassoc_sta_metric_cb;
	cbs.device_deinit = wbd_ieee1905_device_deinit_cb;
	cbs.interface_init = wbd_ieee1905_interface_init_cb;
	cbs.interface_deinit = wbd_ieee1905_interface_deinit_cb;
	cbs.bss_init = wbd_ieee1905_bss_init_cb;
	cbs.bss_deinit = wbd_ieee1905_bss_deinit_cb;
	cbs.create_bss_on_ifr = wbd_ieee1905_create_bss_on_ifr_cb;
	cbs.get_interface_info = wbd_ieee1905_get_interface_info_cb;
	cbs.backhaul_steer_req = wbd_ieee1905_bh_steer_request_cb;
	cbs.beacon_metrics_query = wbd_ieee1905_beacon_metrics_query_cb;
	cbs.configure_policy = wbd_ieee1905_policy_configuration_cb;
	cbs.client_init = wbd_ieee1905_sta_added_cb;
	cbs.client_deinit = wbd_ieee1905_sta_removed_cb;
	cbs.ap_configured = wbd_ieee1905_ap_configured_cb;
	cbs.process_vendor_specific_msg = wbd_ieee1905_process_vendor_specific_msg_cb;
	cbs.get_vendor_specific_tlv = wbd_ieee1905_get_vendor_specific_tlv_cb;
	cbs.interface_chan_change = wbd_ieee1905_interface_chan_change_cb;
	cbs.ap_auto_config_resp = wbd_ieee1905_ap_auto_config_resp_cb;
	cbs.ap_auto_config_search_sent = wbd_ieee1905_ap_auto_config_search_sent;
	cbs.set_bh_sta_params = wbd_ieee1905_set_bh_sta_params;

	ieee1905_register_callbacks(&cbs);
}

/* Initialize the blanket module */
void
wbd_slave_init_blanket_module()
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

	/* Set Process Name */
	snprintf(g_wbd_process_name, sizeof(g_wbd_process_name), "Slave");

	/* Parse common cli arguments */
	wbd_parse_cli_args(argc, argv);

	WBD_INFO("WBD MAIN START...\n");

	signal(SIGTERM, wbd_signal_hdlr);
	signal(SIGINT, wbd_signal_hdlr);
	signal(SIGPWR, wbd_slave_tty_hdlr);

	wbd_slave_init_blanket_module();

	/* Allocate & Initialize the info structure */
	g_wbdinfo = wbd_info_init(&ret);
	WBD_ASSERT();

	/* If agent is not supported exit the slave */
	if (!MAP_IS_AGENT(g_wbdinfo->map_mode)) {
		WBD_WARNING("Multi-AP mode (%d) not configured as Agent...\n",
			g_wbdinfo->map_mode);
		goto end;
	}

	/* Allocate & Initialize Blanket Slave structure object */
	ret = wbd_ds_blanket_slave_init(g_wbdinfo);
	WBD_ASSERT();

	ret = ieee1905_module_init(g_wbdinfo, MAP_MODE_FLAG_AGENT, 0);
	WBD_ASSERT();

	wbd_slave_register_ieee1905_callbacks();

	/* Initialize Slave */
	ret = wbd_init_slave(g_wbdinfo);
	WBD_ASSERT();

	blanket_start_multiap_messaging();

	/* Provide necessary info to debug_monitor for service restart */
	dm_register_app_restart_info(getpid(), argc, argv, NULL);
	/* Main loop which keeps on checking for the timers and fd's */
	wbd_run(g_wbdinfo->hdl);

end:
	/* Exit Slave */
	WBD_INFO("Exiting the Slave\n");
	wbd_exit_slave(g_wbdinfo);
	WBD_INFO("WiFi Blanket Daemon End...\n");

	return ret;
}
