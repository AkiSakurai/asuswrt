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
 * $Id: wbd_master_com_hdlr.c 782227 2019-12-13 04:00:08Z $
 */

#include "wbd.h"
#include "wbd_shared.h"
#include "wbd_ds.h"
#include "wbd_com.h"
#include "wbd_sock_utility.h"
#include "wbd_json_utility.h"
#include "wbd_master_control.h"
#ifdef BCM_APPEVENTD
#include "wbd_appeventd.h"
#include "appeventd_wbd.h"
#endif /* BCM_APPEVENTD */

#include "ieee1905_tlv.h"
#include "ieee1905.h"
#include "wbd_tlv.h"
#include "wbd_master_com_hdlr.h"

/* Minimum 2 BSS are required for having the same client entry to create
 * the REMOVE_CLIENT_REQ timer
 */
#define WBD_MIN_BSS_TO_CREATE_TIMER	2

/* Timer restart count for unconfigured radios to send renew */
#define WBD_RENEW_TIMER_RESTART_COUNT	10

/* timeout to send channel selection reuqest after channel preference query */
#define WBD_TM_CHAN_SELECT_AFTER_QUERY	2

/* 5g channel maximum control channel */
#define MAX_5G_CONTROL_CHANNEL	165
/* structure to hold the params to remove stale client from BSS's assocl list i.e.
 * remove client from associated on the following condition:
 * client didnt sent disassoc to first BSS && client roam to another strong BSS
 */
typedef struct wbd_remove_client_arg {
	struct ether_addr parent_bssid;	/* Parent BSS's BSSID */
	struct ether_addr sta_mac;	/* STA mac address */
} wbd_remove_client_arg_t;

/* Flags used in the nvram_list_t structure */
#define WBD_NVRAM_FLAG_STR	0x00	/* NVRAM value is of type string */
#define WBD_NVRAM_FLAG_VALINT	0x01	/* NVRAM value is of type integer */

/* List of NVRAMs */
typedef struct nvram_list {
	char name[WBD_MAX_BUF_64];	/* Name of the NVRAM */
	uint8 flags;			/* Flags of type WBD_NVRAM_FLAG_XXX */
	int def;			/* Default value in case WBD_NVRAM_FLAG_VALINT flag */
} nvram_list_t;

/* List of NVRAMs to be sent from controller to agents */
static nvram_list_t nvram_params[] = {
	{WBD_NVRAM_MCHAN_SLAVE, WBD_NVRAM_FLAG_VALINT, WBD_DEF_MCHAN_SLAVE},
	{WBD_NVRAM_TM_SLV_WD_TARGET_BSS, WBD_NVRAM_FLAG_VALINT, WBD_TM_SLV_WD_TARGET_BSS},
	{NVRAM_MAP_NO_MULTIHOP, WBD_NVRAM_FLAG_STR, 0}
};

#define WBD_GET_NVRAMS_COUNT() (sizeof(nvram_params)/sizeof(nvram_params[0]))

/* ------------------------------------ Static Declarations ------------------------------------ */

/* Do Initialize sequences for newly created Master Info for specific band */
static int
wbd_master_init_actions(uint8 bkt_id, wbd_master_info_t *master);
/* get tbss threshold configurations */
static int
wbd_master_get_tbss_config(wbd_tbss_thld_t *thld_cfg, char *thld_nvram,
	wbd_master_info_t *master);
/* Gets Master NVRAM settings */
static int
wbd_master_retrieve_nvram_config(uint8 bkt_id, wbd_master_info_t *master);
/* Master updates WEAK_CLIENT Data */
static int
wbd_master_process_weak_client_cmd_data(wbd_master_info_t *master_info,
	i5_dm_clients_type *i5_assoc_sta, wbd_cmd_weak_client_resp_t *cmdweakclientresp);
/* Processes WEAK_CLIENT request */
static int
wbd_master_process_weak_client_cmd(wbd_master_info_t *master_info,
	i5_dm_clients_type *i5_assoc_sta, wbd_weak_sta_metrics_t *sta_stats,
	wbd_weak_sta_policy_t *metric_policy);
/* Create the timer to remove stale client entry for BSS */
static int
wbd_controller_create_remove_client_timer(wbd_remove_client_arg_t* arg, int timeout);
/* Remove stale client entry form BSS, if required */
static void
wbd_controller_remove_client_timer_cb(bcm_usched_handle *hdl, void *arg);
/* Create AP chan report from topology */
static void wbd_master_create_ap_chan_report(i5_dm_network_topology_type *i5_topology,
	wbd_master_info_t *master_info);
/* -------------------------- Add New Functions above this -------------------------------- */

/* Processes the STEER CLI command */
static void
wbd_master_process_steer_cli_cmd(wbd_com_handle *hndl, int childfd, void *cmddata, void *arg);
/* Get the SLAVELIST CLI command data */
static int
wbd_master_process_bsslist_cli_cmd_data(wbd_info_t *info,
	wbd_cli_send_data_t *clidata,  char **outdataptr);
/* Processes the SLAVELIST CLI command */
static void
wbd_master_process_bsslist_cli_cmd(wbd_com_handle *hndl, int childfd, void *cmddata, void *arg);
/* Get the INFO CLI command data */
static int
wbd_master_process_info_cli_cmd_data(wbd_info_t *info,
	wbd_cli_send_data_t *clidata,  char **outdataptr);
/* Processes the INFO CLI command */
static void
wbd_master_process_info_cli_cmd(wbd_com_handle *hndl, int childfd, void *cmddata, void *arg);
/* Get the CLIENTLIST CLI command data */
static int
wbd_master_process_clientlist_cli_cmd_data(wbd_info_t *info,
	wbd_cli_send_data_t *clidata,  char **outdataptr);
/* Processes the CLIENTLIST CLI command */
static void
wbd_master_process_clientlist_cli_cmd(wbd_com_handle *hndl, int childfd, void *cmddata, void *arg);
/* Get the logs cli command data */
static int
wbd_master_process_logs_cli_cmd_data(wbd_info_t *info,
	wbd_cli_send_data_t *clidata,  char **outdataptr);
/* Process the logs cli command. */
static void
wbd_master_process_logs_cli_cmd(wbd_com_handle *hndl, int childfd,
	void *cmddata, void *arg);
/* Processes the BH OPT command */
static void
wbd_master_process_bh_opt_cli_cmd(wbd_com_handle *hndl, int childfd, void *cmddata, void *arg);
/* Callback for exception from communication handler for master server */
static void
wbd_master_com_server_exception(wbd_com_handle *hndl, void *arg, WBD_COM_STATUS status);
/* Callback for exception from communication handler for CLI */
static void
wbd_master_com_cli_exception(wbd_com_handle *hndl, void *arg, WBD_COM_STATUS status);
/* Register all the commands for master server to communication handle */
static int
wbd_master_register_server_command(wbd_info_t *info);
/* Callback fn to send CONFIG cmd to Slave which has just joined to Master */
static void
wbd_master_send_ap_autoconfig_renew_timer_cb(bcm_usched_handle *hdl, void *arg);
/* Processes BSS capability report message */
static void
wbd_master_process_bss_capability_report_cmd(unsigned char *neighbor_al_mac,
	unsigned char *tlv_data, unsigned short tlv_data_len);
/* Processes BSS metrics report message */
static void
wbd_master_process_bss_metrics_report_cmd(unsigned char *neighbor_al_mac,
	unsigned char *tlv_data, unsigned short tlv_data_len);
/* Processes Steer Response Report message */
static void
wbd_master_process_steer_resp_report_cmd(unsigned char *neighbor_al_mac,
	unsigned char *tlv_data, unsigned short tlv_data_len);
/* Processes ZWDFS message */
static void
wbd_master_process_zwdfs_cmd(unsigned char *neighbor_al_mac,
	unsigned char *tlv_data, unsigned short tlv_data_len);
/* Send 1905 Vendor Specific Zero wait DFS command, from Controller to Agents */
static int
wbd_master_broadcast_vendor_msg_zwdfs(uint8 *src_al_mac, wbd_cmd_vndr_zwdfs_msg_t *msg, uint8 band);
/* Send NVRAM set vendor specific message */
static int
wbd_master_send_vndr_nvram_set_cmd(i5_dm_device_type *i5_device);
/* create common chan info from intersection of all agent's chan info */
static wbd_dfs_chan_info_t*
wbd_master_get_common_chan_info(i5_dm_network_topology_type *i5_topology, uint8 band);
/* send intersection of all agent's same band chan info. Agent along with
 * this chan info and local chanspecs list prepare dfs list
 */
static void
wbd_master_send_dfs_chan_info(wbd_dfs_chan_info_t *dfs_chan_info,
	uint8 band, unsigned char *al_mac, unsigned char *radio_mac);
/* Create intersection of all agent's chan info(with same band), and Broadcast
 * final chan_info to each agent.
 */
static void
wbd_master_process_intf_chan_info(unsigned char *src_al_mac, unsigned char *tlv_data,
	unsigned short tlv_data_len);
#if defined(MULTIAPR2)
/* Send Channel Scan Request Message */
static void
wbd_master_send_chscan_req(unsigned char *al_mac, bool fresh_scan_req);
#endif /* MULTIAPR2 */
/* ------------------------------------ Static Declarations ------------------------------------ */

#ifdef WLHOSTFBT
/* Get MDID from NVRAM, if not present generate and set the mdid */
extern uint16 wbd_get_mdid(char *prefix);
#endif /* WLHOSTFBT */

/* Common algo to compare STA Stats and Thresholds, & identify if STA is Weak or not */
extern int wbd_weak_sta_identification(struct ether_addr *sta_mac,
	wbd_weak_sta_metrics_t *sta_stats, wbd_weak_sta_policy_t *thresholds,
	int *out_fail_cnt, int *out_weak_flag);

/* Do Initialize sequences for newly created Master Info for specific band */
static int
wbd_master_init_actions(uint8 bkt_id, wbd_master_info_t *master)
{
	int ret = WBDE_OK;
	WBD_ENTER();

	/* Gets Master NVRAM settings */
	wbd_master_retrieve_nvram_config(bkt_id, master);

	WBD_INFO("BKTID[%d] Master[%p] Init Sequence Started %s\n",
		master->bkt_id, master, wbderrorstr(ret));
	WBD_EXIT();
	return ret;
}

/* get tbss threshold configurations */
static int
wbd_master_get_tbss_config(wbd_tbss_thld_t *thld_cfg, char *thld_nvram,
	wbd_master_info_t *master)
{
	int ret = WBDE_OK, num = 0;
	char *str;
	wbd_tbss_thld_t threshold_cfg;
	WBD_ENTER();

	/* read threshold from NVRAM */
	str = blanket_nvram_safe_get(thld_nvram);
	if (str) {
		num = sscanf(str, "%d %d %d %d %x %d",
			&threshold_cfg.t_rssi,
			&threshold_cfg.t_hops,
			&threshold_cfg.t_sta_cnt,
			&threshold_cfg.t_uplinkrate,
			&threshold_cfg.flags,
			&threshold_cfg.sof_algos);
		if (num == 6) {
			if ((threshold_cfg.sof_algos < 0) ||
				(threshold_cfg.sof_algos > WBD_MAX_SOF_ALGOS)) {
				threshold_cfg.sof_algos = WBD_SOF_ALGO_BEST_RSSI;
			}
			memcpy(thld_cfg, &threshold_cfg, sizeof(*thld_cfg));
		} else {
			WBD_WARNING("BKTID[%d] %s : %s = %s\n", master->bkt_id,
				wbderrorstr(WBDE_INV_NVVAL), thld_nvram, str);
		}
	}

	WBD_DEBUG("BKTID[%d] %s t_rssi[%d] t_hops[%d] t_sta_cnt[%d] t_uplinkrate[%d] flags[0x%X] "
		"sof_algo[%d]\n", master->bkt_id, thld_nvram, thld_cfg->t_rssi, thld_cfg->t_hops,
		thld_cfg->t_sta_cnt, thld_cfg->t_uplinkrate, thld_cfg->flags, thld_cfg->sof_algos);

	WBD_EXIT();
	return ret;
}

/* get tbss weightage configurations */
static int
wbd_master_get_tbss_weightage_config(wbd_tbss_wght_t *wght_cfg, char *wght_nvram,
	wbd_master_info_t *master)
{
	int ret = WBDE_OK, num = 0;
	char *str;
	wbd_tbss_wght_t weightage_cfg;
	WBD_ENTER();

	/* read weightages from NVRAM */
	str = blanket_nvram_safe_get(wght_nvram);
	if (str) {
		num = sscanf(str, "%d %d %d %d %d %d %x",
			&weightage_cfg.w_rssi,
			&weightage_cfg.w_hops,
			&weightage_cfg.w_sta_cnt,
			&weightage_cfg.w_phyrate,
			&weightage_cfg.w_nss,
			&weightage_cfg.w_tx_rate,
			&weightage_cfg.flags);
		if (num == 7) {
			memcpy(wght_cfg, &weightage_cfg, sizeof(*wght_cfg));
		} else {
			WBD_WARNING("BKTID[%d] %s : %s = %s\n", master->bkt_id,
				wbderrorstr(WBDE_INV_NVVAL), wght_nvram, str);
		}
	}

	WBD_DEBUG("BKTID[%d] %s w_rssi[%d] w_hops[%d] w_sta_cnt[%d] w_phyrate[%d] flags[0x%X]\n",
		master->bkt_id, wght_nvram, wght_cfg->w_rssi, wght_cfg->w_hops, wght_cfg->w_sta_cnt,
		wght_cfg->w_phyrate, wght_cfg->flags);

	WBD_EXIT();
	return ret;
}

/* Gets Master NVRAM settings */
static int
wbd_master_retrieve_nvram_config(uint8 bkt_id, wbd_master_info_t *master)
{
	int ret = WBDE_OK, fbt, fbt_overds, fbt_reassoc_tm;
	char thld_nvram[WBD_MAX_NVRAM_NAME];
	char prefix[IFNAMSIZ];
	char *str;
	int num = 0;
	WBD_ENTER();

	snprintf(prefix, sizeof(prefix), "%s_", master->bkt_name);
	WBD_DEBUG("Retrieve Config for BKTID[%d] BKTNAME[%s]\n", master->bkt_id, master->bkt_name);
	master->tbss_info.nws_adv_thld =
		blanket_get_config_val_int(NULL, WBD_NVRAM_ADV_THLD, WBD_TBSS_ADV_THLD);

	master->tbss_info.tbss_stacnt_thld = blanket_get_config_val_int(NULL,
		WBD_NVRAM_TBSS_STACNT_THLD, WBD_TBSS_MIN_STA_THLD);

	master->tbss_info.tbss_algo = (uint8)blanket_get_config_val_uint(NULL,
		WBD_NVRAM_TBSS_ALGO, 0);
	if (master->tbss_info.tbss_algo >= wbd_get_max_tbss_algo())
		master->tbss_info.tbss_algo = 0;

	master->tbss_info.tbss_algo_bh = (uint8)blanket_get_config_val_uint(NULL,
		WBD_NVRAM_TBSS_ALGO_BH, 0);
	if (master->tbss_info.tbss_algo_bh >= wbd_get_max_tbss_algo())
		master->tbss_info.tbss_algo_bh = 0;

	/* Read Weightage index and prepare weightage config from predefined entries */
	master->tbss_info.tbss_wght = (uint8)blanket_get_config_val_uint(NULL,
		WBD_NVRAM_TBSS_WGHT_IDX, 0);
	if (master->tbss_info.tbss_wght >= wbd_get_max_tbss_wght())
		master->tbss_info.tbss_wght = 0;
	memcpy(&master->tbss_info.wght_cfg, wbd_get_tbss_wght(master),
		sizeof(master->tbss_info.wght_cfg));

	/* Read Backhaul Weightage index and prepare weightage config from predefined entries */
	master->tbss_info.tbss_wght_bh = (uint8)blanket_get_config_val_uint(NULL,
		WBD_NVRAM_TBSS_WGHT_IDX_BH, 0);
	if (master->tbss_info.tbss_wght_bh >= wbd_get_max_tbss_wght_bh())
		master->tbss_info.tbss_wght_bh = 0;
	memcpy(&master->tbss_info.wght_cfg_bh, wbd_get_tbss_wght_bh(master),
		sizeof(master->tbss_info.wght_cfg_bh));

	/* Get NVRAM : ACTION fram intervval for home and off-channel weak STA */
	str = blanket_nvram_prefix_safe_get(NULL, WBD_NVRAM_TBSS_BOUNDARIES);
	if (str) {
		num = sscanf(str, "%d %d %d %d", &master->tbss_info.tbss_min_phyrate,
			&master->tbss_info.tbss_max_phyrate, &master->tbss_info.tbss_min_rssi,
			&master->tbss_info.tbss_max_rssi);
	}
	if (!str || num != 4) {
		/* 2G boundary is default for a blanket */
		master->tbss_info.tbss_min_phyrate = WBD_TBSS_MIN_PHYRATE_BOUNDARY_2G;
		master->tbss_info.tbss_max_phyrate = WBD_TBSS_MAX_PHYRATE_BOUNDARY_2G;
		master->tbss_info.tbss_min_rssi = WBD_TBSS_MIN_RSSI_BOUNDARY_2G;
		master->tbss_info.tbss_max_rssi = WBD_TBSS_MAX_RSSI_BOUNDARY_2G;
	}

	WBD_DEBUG("BKTID[%d] nws_adv_thld[%d] tbss_stacnt_thld[%d] tbss_algo[%d] tbss_algo_bh[%d] "
		"tbss_wght[%d] min_phyrate_boundary[%d] max_phyrate_boundary[%d] "
		"min_rssi_boundary[%d] max_rssi_boundary[%d]\n", master->bkt_id,
		master->tbss_info.nws_adv_thld, master->tbss_info.tbss_stacnt_thld,
		master->tbss_info.tbss_algo, master->tbss_info.tbss_algo_bh,
		master->tbss_info.tbss_wght, master->tbss_info.tbss_min_phyrate,
		master->tbss_info.tbss_max_phyrate, master->tbss_info.tbss_min_rssi,
		master->tbss_info.tbss_max_rssi);

	/* read weightages from NVRAM for fronthaul */
	wbd_master_get_tbss_weightage_config(&master->tbss_info.wght_cfg, WBD_NVRAM_TBSS_WGHT,
		master);

	/* read weightages from NVRAM for backhaul */
	wbd_master_get_tbss_weightage_config(&master->tbss_info.wght_cfg_bh, WBD_NVRAM_TBSS_WGHT_BH,
		master);

	/* Read threshold index and prepare threshold config from predefined entries - 2G */
	master->tbss_info.tbss_thld_2g = (uint8)blanket_get_config_val_uint(NULL,
		WBD_NVRAM_TBSS_THLD_IDX_2G, 0);
	if (master->tbss_info.tbss_thld_2g >= wbd_get_max_tbss_thld_2g())
		master->tbss_info.tbss_thld_2g = 0;
	memcpy(&master->tbss_info.thld_cfg_2g, wbd_get_tbss_thld_2g(master),
		sizeof(master->tbss_info.thld_cfg_2g));

	WBDSTRNCPY(thld_nvram, WBD_NVRAM_TBSS_THLD_2G, sizeof(thld_nvram));
	wbd_master_get_tbss_config(&master->tbss_info.thld_cfg_2g, thld_nvram, master);

	/* Read threshold index and prepare threshold config from predefined entries - 5G */
	master->tbss_info.tbss_thld_5g = (uint8)blanket_get_config_val_uint(NULL,
		WBD_NVRAM_TBSS_THLD_IDX_5G, 0);
	if (master->tbss_info.tbss_thld_5g >= wbd_get_max_tbss_thld_5g())
		master->tbss_info.tbss_thld_5g = 0;
	memcpy(&master->tbss_info.thld_cfg_5g, wbd_get_tbss_thld_5g(master),
		sizeof(master->tbss_info.thld_cfg_5g));

	WBDSTRNCPY(thld_nvram, WBD_NVRAM_TBSS_THLD_5G, sizeof(thld_nvram));
	wbd_master_get_tbss_config(&master->tbss_info.thld_cfg_5g, thld_nvram, master);

	/* Read threshold index and prepare threshold config from predefined entries - Backhaul */
	master->tbss_info.tbss_thld_bh = (uint8)blanket_get_config_val_uint(NULL,
		WBD_NVRAM_TBSS_THLD_IDX_BH, 0);
	if (master->tbss_info.tbss_thld_bh >= wbd_get_max_tbss_thld_bh())
		master->tbss_info.tbss_thld_bh = 0;
	memcpy(&master->tbss_info.thld_cfg_bh, wbd_get_tbss_thld_bh(master),
		sizeof(master->tbss_info.thld_cfg_bh));

	WBDSTRNCPY(thld_nvram, WBD_NVRAM_TBSS_THLD_BH, sizeof(thld_nvram));
	wbd_master_get_tbss_config(&master->tbss_info.thld_cfg_bh, thld_nvram, master);

	/* Set sta based default threshhold */
	master->tbss_info.thld_sta.t_tx_rate = WBD_TBSS_STA_TX_RATE_THLD;
	master->tbss_info.thld_sta.t_tx_failures = WBD_TBSS_STA_TX_FAILURES_THLD;

	/* Master Look for FBT Enable */
	fbt = blanket_get_config_val_int(prefix, WBD_NVRAM_FBT, WBD_FBT_NOT_DEFINED);

	/* If FBT Enable, Master Fetch FBT Configurations */
	if (fbt == WBD_FBT_DEF_FBT_ENABLED) {

		/* Set FBT_ENAB flag, to enable FBT_CONFIG_REQ/FBT_CONFIG_RESP mechanism */
		master->flags |= WBD_FLAGS_MASTER_FBT_ENABLED;

#ifdef WLHOSTFBT
		/* Get MDID for this FBT Network */
		master->fbt_info.mdid = wbd_get_mdid(prefix);
#endif /* WLHOSTFBT */

		/* Get FBT fbt_overds NVRAM. If it is not defined. Get default value */
		fbt_overds = blanket_get_config_val_int(prefix, NVRAM_FBT_OVERDS,
			WBD_FBT_DEF_OVERDS);
		if (fbt_overds) {
			master->fbt_info.ft_cap_policy |= FBT_MDID_CAP_OVERDS;
		}
		/* Get FBT fbt_reassoc_time NVRAM. If it is not defined. Get default value */
		fbt_reassoc_tm = blanket_get_config_val_int(prefix, NVRAM_FBT_REASSOC_TIME,
			WBD_FBT_DEF_REASSOC_TIME);
		master->fbt_info.tie_reassoc_interval = fbt_reassoc_tm;
	}

	WBD_EXIT();
	return ret;
}

/* Called when some STA joins the BSS */
int
wbd_controller_refresh_blanket_on_sta_assoc(struct ether_addr *sta_mac,
	struct ether_addr *parent_bssid, wbd_wl_sta_stats_t *sta_stats)
{
	int ret = WBDE_OK, sta_found_count = 0;
	wbd_remove_client_arg_t *param = NULL;
	WBD_ENTER();

	/* If STA entry is found in Bounce Table && STA Status is STEERING, so this STA has
	 *  been steered from source to this TBSS. So let's increment the Bounce Count here.
	 */
	wbd_ds_increment_bounce_count_of_entry(wbd_get_ginfo()->wbd_master,
		sta_mac, parent_bssid, sta_stats);

	/* If remove_client is 0(not enabled), no need to send the REMOVE_CLIENT_REQ command */
	if (!WBD_RMCLIENT_ENAB((wbd_get_ginfo()->flags))) {
		goto end;
	}

	/* Count how many BSSes has the STA entry in Controller Topology */
	sta_found_count = wbd_ds_find_duplicate_sta_in_controller(sta_mac, NULL);

	WBD_INFO("STA["MACF"] Found in %d BSSes\n", ETHER_TO_MACF(*sta_mac), sta_found_count);
	/* Atleast 2 BSS should have the same client entry to start the timer,
	 * if more than 2 BSSes having the client entry, no need to start the timer
	 * again, as timer is already running
	 */
	if ((sta_found_count > 1) && (sta_found_count <= WBD_MIN_BSS_TO_CREATE_TIMER)) {
		param = (wbd_remove_client_arg_t*)wbd_malloc(sizeof(*param), &ret);
		WBD_ASSERT_MSG("STA["MACF"] BSS["MACF"] Failed to alloc REMOVE_CLIENT_REQ arg\n",
			ETHER_TO_MACF(*sta_mac), ETHER_TO_MACF(*parent_bssid));

		/* Create the timer to send REMOVE_CLIENT_REQ Vendor Msg to BSS */
		eacopy(sta_mac, &(param->sta_mac));
		eacopy(parent_bssid, &(param->parent_bssid));

		wbd_controller_create_remove_client_timer(param,
			wbd_get_ginfo()->max.tm_remove_client);
	}
end:
	WBD_EXIT();
	return ret;
}

/* Master updates WEAK_CLIENT Data */
static int
wbd_master_process_weak_client_cmd_data(wbd_master_info_t *master_info,
	i5_dm_clients_type *i5_assoc_sta, wbd_cmd_weak_client_resp_t *cmdweakclientresp)
{
	int ret = WBDE_OK;
	wbd_blanket_master_t *wbd_master;
	wbd_assoc_sta_item_t *assoc_sta;
	wbd_sta_bounce_detect_t *bounce_sta_entry;
	wbd_bounce_detect_t *bounce_cfg;
	wbd_prb_sta_t *prbsta;
	i5_dm_bss_type *i5_bss;
	uint8 map_flags = IEEE1905_MAP_FLAG_FRONTHAUL;
	unsigned int fh_bss_count, b_bss_count;
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(master_info, WBDE_INV_ARG);
	WBD_ASSERT_ARG(i5_assoc_sta->vndr_data, WBDE_INV_ARG);
	wbd_master = master_info->parent;

	i5_bss = (i5_dm_bss_type*)WBD_I5LL_PARENT(i5_assoc_sta);

	/* Update the bouncing table */
	wbd_ds_update_sta_bounce_table(master_info->parent);

	/* Choose the bounce detect config based on the STA type */
	if (I5_CLIENT_IS_BSTA(i5_assoc_sta)) {
		bounce_cfg = &wbd_master->bounce_cfg_bh;
	} else {
		bounce_cfg = &wbd_master->bounce_cfg;
	}

	/* Fetch the STA entry from bouncing table,
	 * if sta is present and sta state is WBD_BOUNCE_DWELL_STATE send dwell time to slave
	 * which in turn will inform to BSD.
	 */
	bounce_sta_entry = wbd_ds_find_sta_in_bouncing_table(wbd_master,
		(struct ether_addr*)i5_assoc_sta->mac);
	if ((bounce_cfg->cnt == 0) ||
		((bounce_sta_entry != NULL) &&
		(bounce_sta_entry->state == WBD_BOUNCE_DWELL_STATE))) {
		cmdweakclientresp->dwell_time = ((bounce_cfg->cnt == 0) ?
			bounce_cfg->dwell_time :
			(bounce_cfg->dwell_time - bounce_sta_entry->run.dwell_time));
		WBD_INFO("BKTID[%d] Slave["MACDBG"] STA["MACDBG"] is bouncing STA till %d "
			"seconds\n",
			master_info->bkt_id, MAC2STRDBG(i5_bss->BSSID),
			MAC2STRDBG(i5_assoc_sta->mac), cmdweakclientresp->dwell_time);
		ret = WBDE_DS_BOUNCING_STA;
		goto end;
	}

	/* Get Assoc item pointer */
	assoc_sta = i5_assoc_sta->vndr_data;

	if (I5_CLIENT_IS_BSTA(i5_assoc_sta)) {
		map_flags = IEEE1905_MAP_FLAG_BACKHAUL;

		/* If the backhaul optimization is running, then do not honor weak client */
		if (WBD_MINFO_IS_BH_OPT_RUNNING(master_info->flags)) {
			WBD_INFO("BKTID[%d] Slave["MACDBG"] For BHSTA["MACDBG"]. %s\n",
				master_info->bkt_id, MAC2STRDBG(i5_bss->BSSID),
				MAC2STRDBG(i5_assoc_sta->mac), wbderrorstr(WBDE_BH_OPT_RUNNING));
			ret = WBDE_BH_OPT_RUNNING;
			goto end;
		}

		/* If there is not more than one backhaul BSS, cant steer the backhaul STA */
		b_bss_count = i5DmCountNumOfAgentsWithbBSS();
		/* If the device with STA interface has the backhaul BSS, reduce the backhaul BSS
		 * count by 1
		 */
		if (wbd_ds_is_bsta_device_has_bbss(i5_assoc_sta->mac)) {
			WBD_DEBUG("For BHSTA["MACDBG"], the BSTA Device has Backhaul BSS\n",
				MAC2STRDBG(i5_assoc_sta->mac));
			b_bss_count -= 1;
		}
		if (b_bss_count <= 1) {
			WBD_WARNING("BKTID[%d] Slave["MACDBG"] For BHSTA["MACDBG"] Count[%d]. %s\n",
				master_info->bkt_id, MAC2STRDBG(i5_bss->BSSID),
				MAC2STRDBG(i5_assoc_sta->mac), b_bss_count,
				wbderrorstr(WBDE_NO_SLAVE_TO_STEER));
			ret = WBDE_NO_SLAVE_TO_STEER;
			goto end;
		}
		/* Check if the STA can be steered to other backhaul BSS without forming any loop */
		if (!wbd_master_is_bsta_steering_possible(i5_assoc_sta)) {
			WBD_WARNING("BKTID[%d] Slave["MACDBG"] For BHSTA["MACDBG"] steering is not "
				"possible as it can form a loop\n",
				master_info->bkt_id, MAC2STRDBG(i5_bss->BSSID),
				MAC2STRDBG(i5_assoc_sta->mac));
			ret = WBDE_BH_STEER_LOOP;
			goto end;
		}
	} else {
		/* If there is not more than one fronthaul BSS, cant steer the weak client */
		if ((fh_bss_count = wbd_ds_count_fhbss()) <= 1) {
			WBD_WARNING("BKTID[%d] Slave["MACDBG"] For STA["MACDBG"] "
				"FHBSSCount[%d]. %s\n",
				master_info->bkt_id, MAC2STRDBG(i5_bss->BSSID),
				MAC2STRDBG(i5_assoc_sta->mac), fh_bss_count,
				wbderrorstr(WBDE_NO_SLAVE_TO_STEER));
			ret = WBDE_NO_SLAVE_TO_STEER;
			goto end;
		}
	}

	/* If its in ignore list */
	if (assoc_sta->status == WBD_STA_STATUS_IGNORE) {
		WBD_WARNING("BKTID[%d] Slave["MACDBG"] STA["MACDBG"] In Ignore list\n",
			master_info->bkt_id, MAC2STRDBG(i5_bss->BSSID),
			MAC2STRDBG(i5_assoc_sta->mac));
		ret = WBDE_IGNORE_STA;
		goto end;
	}

	/* refresh this STA on probe sta list */
	prbsta = wbd_ds_find_sta_in_probe_sta_table(master_info->parent->parent,
		(struct ether_addr*)i5_assoc_sta->mac, FALSE);
	if (prbsta) {
		prbsta->active = time(NULL);
	} else {
		WBD_WARNING("Warning: probe list has no STA["MACDBG"]\n",
			MAC2STRDBG(i5_assoc_sta->mac));
	}

	/* If the STA is in more than one BSS dont accept weak client. Accept weak client only
	 * After REMOVE_CLIENT_REQ Command is sent and it is removed from the other BSS.
	 * This is to fix the issue where we found that STA was not exists in assoclist of masters.
	 * This happens when we get WEAK_CLIENT from the old AP where the STA is associated. When
	 * we get WEAK_CLIENT, we are removing the STA from assoclist of other BSS's before
	 * adding it to the monitorlist.
	 */
	if (wbd_ds_find_duplicate_sta_in_controller((struct ether_addr*)i5_assoc_sta->mac,
		NULL) > 1) {
		WBD_WARNING("STA["MACDBG"] BSS["MACDBG"] Exists in More than One BSS.\n",
			MAC2STRDBG(i5_assoc_sta->mac), MAC2STRDBG(i5_bss->BSSID));
		ret = WBDE_DS_DUP_STA;
		goto end;
	}

	/* Add STA in all peer Slaves' Monitor STA List based on mapFlags. If the STA is connected
	 * to Fronthaul, add only to Fronthaul BSS
	 */
	ret = wbd_ds_add_sta_in_peer_devices_monitorlist(master_info, i5_assoc_sta, map_flags);
	if ((ret != WBDE_OK) && (ret != WBDE_DS_STA_EXST)) {
		WBD_INFO("BKTID[%d] Slave["MACDBG"] STA["MACDBG"]. Failed to add to peer slaves "
			"monitorlist\n\n", master_info->bkt_id, MAC2STRDBG(i5_bss->BSSID),
			MAC2STRDBG(i5_assoc_sta->mac));
	}

	/* Update STA Status = Weak */
	WBD_DS_UP_ASSOCSTA_STATUS(master_info, assoc_sta, WBD_STA_STATUS_WEAK);

	/* Start Updating Last Reported RSSI, if STA is Weak, to have Hysterisis condition check */
	assoc_sta->last_weak_rssi = assoc_sta->stats.rssi;

end: /* Check Master Pointer before using it below */

	WBD_EXIT();
	return ret;
}

/* Prepares the beacon request fields for the STAs connected to Fronthaul BSS */
static int
wbd_master_prepare_beacon_request(wbd_glist_t *rclass_chan_list,
	i5_dm_clients_type *i5_assoc_sta, ieee1905_beacon_request *bcn_req)
{
	int ret = WBDE_OK;
	uint8 idx = 0, idx_ap_chan_report_len;
	i5_dm_interface_type *i5_ifr;
	i5_dm_bss_type *i5_bss;
	wbd_bcn_req_rclass_list_t *rclass_list;
	wbd_bcn_req_chan_list_t *chan_list;
	dll_t *rclass_item_p, *chan_item_p;
	unsigned char ap_chan_report[WBD_MAX_BUF_256];

	i5_bss = (i5_dm_bss_type*)WBD_I5LL_PARENT(i5_assoc_sta);
	i5_ifr = (i5_dm_interface_type*)WBD_I5LL_PARENT(i5_bss);

	memcpy(bcn_req->sta_mac, i5_assoc_sta->mac, sizeof(bcn_req->sta_mac));
	bcn_req->opclass = i5_ifr->opClass;
	bcn_req->channel = 255;
	memcpy(bcn_req->bssid, &ether_bcast, sizeof(bcn_req->bssid));
	memcpy(&bcn_req->ssid, &i5_bss->ssid, sizeof(bcn_req->ssid));

	/* Prepare the AP channel report */
	memset(ap_chan_report, 0, sizeof(ap_chan_report));
	bcn_req->ap_chan_report_count = rclass_chan_list->count;

	/* Traverse br0 AP channel report */
	foreach_glist_item(rclass_item_p, *rclass_chan_list) {
		rclass_list = (wbd_bcn_req_rclass_list_t*)rclass_item_p;

		idx_ap_chan_report_len = idx; /* Place holder for length of one AP chan report */
		idx++;
		ap_chan_report[idx++] = rclass_list->rclass;
		/* Traverse channel list */
		foreach_glist_item(chan_item_p, rclass_list->chan_list) {
			chan_list = (wbd_bcn_req_chan_list_t*)chan_item_p;
			ap_chan_report[idx++] = chan_list->channel;
		}
		ap_chan_report[idx_ap_chan_report_len] = (idx - 1) - idx_ap_chan_report_len;
	}
	bcn_req->ap_chan_report_len = idx;
	bcn_req->ap_chan_report = (unsigned char*)wbd_malloc(idx, &ret);
	WBD_ASSERT();

	memcpy(bcn_req->ap_chan_report, ap_chan_report, idx);

end:
	WBD_EXIT();
	return ret;
}

/* Cretae the rclass and channel list for AP channel report for backhaul STA */
void
wbd_master_create_backhaul_bcn_req_ap_chan_report(wbd_glist_t *rclass_chan_list)
{
	i5_dm_network_topology_type *i5_topology;
	i5_dm_device_type *i5_iter_device;
	i5_dm_interface_type *i5_iter_ifr;
	i5_dm_bss_type *i5_iter_bss;
	WBD_ENTER();

	i5_topology = ieee1905_get_datamodel();

	/* Update the AP chan report with new channel values of all the backhaul interfaces */
	foreach_i5glist_item(i5_iter_device, i5_dm_device_type, i5_topology->device_list) {
		if (!I5_IS_MULTIAP_AGENT(i5_iter_device->flags)) {
			continue;
		}

		foreach_i5glist_item(i5_iter_ifr, i5_dm_interface_type,
			i5_iter_device->interface_list) {

			bool is_backhaul = FALSE;

			/* Only if there is atleast one backhaul BSS */
			foreach_i5glist_item(i5_iter_bss, i5_dm_bss_type,
				i5_iter_ifr->bss_list) {

				if (I5_IS_BSS_BACKHAUL(i5_iter_bss->mapFlags)) {
					is_backhaul = TRUE;
					break;
				}
			}

			if (is_backhaul) {
				if (i5_iter_ifr->opClass == 0 && i5_iter_ifr->chanspec != 0) {
					blanket_get_global_rclass(i5_iter_ifr->chanspec,
						&i5_iter_ifr->opClass);
					WBD_INFO("Device["MACDBG"] IFR["MACDBG"] rclass was empty "
						"calculated it, rclass[%d] chanspec[0x%x]\n",
						MAC2STRDBG(i5_iter_device->DeviceId),
						MAC2STRDBG(i5_iter_ifr->InterfaceId),
						i5_iter_ifr->opClass, i5_iter_ifr->chanspec);
				}
				wbd_master_update_ap_chan_report(rclass_chan_list, i5_iter_ifr);
			}
		}
	}
}

/* Prepares the beacon request fields for the backhaul STA */
static int
wbd_master_prepare_backhaul_beacon_request(i5_dm_clients_type *i5_assoc_sta,
	ieee1905_beacon_request *bcn_req)
{
	int ret = WBDE_OK;
	wbd_glist_t rclass_chan_list;

	/* For the backhaul STA create the AP channel report and free it after use as it is not
	 * used frequently. No need to store it
	 */
	wbd_ds_glist_init(&rclass_chan_list);
	wbd_master_create_backhaul_bcn_req_ap_chan_report(&rclass_chan_list);

	wbd_master_prepare_beacon_request(&rclass_chan_list, i5_assoc_sta, bcn_req);

	wbd_ds_ap_chan_report_cleanup(&rclass_chan_list);

	WBD_EXIT();
	return ret;
}

/* Check if the device is operating on a particular channel */
static int
wbd_master_is_agent_operating_on_channel(i5_dm_device_type *i5_device, uint8 channel)
{
	i5_dm_interface_type *i5_ifr;

	/* Iterate through wireless interfaces */
	foreach_i5glist_item(i5_ifr, i5_dm_interface_type, i5_device->interface_list) {
		if (!i5DmIsInterfaceWireless(i5_ifr->MediaType)) {
			continue;
		}

		/* Compare the channel */
		if (channel == wf_chspec_ctlchan(i5_ifr->chanspec)) {
			return 1;
		}
	}

	return 0;
}

/* Sends the Associated/Unassociated link metrics and beacon metrics for the STA */
int
wbd_master_send_link_metric_requests(wbd_master_info_t *master_info,
	i5_dm_clients_type *i5_assoc_sta)
{
	int ret = WBDE_OK;
	i5_dm_network_topology_type *i5_topology;
	i5_dm_device_type *i5_device, *i5_iter_device, *i5_self_device;
	i5_dm_interface_type *i5_ifr;
	i5_dm_bss_type *i5_bss;
	ieee1905_beacon_request bcn_req;
	ieee1905_unassoc_sta_link_metric_query uassoc_req;
	unassoc_query_per_chan_rqst *per_chan_rqst = NULL;
	time_t now = time(NULL);
	wbd_beacon_reports_t *wbd_bcn_rpt = NULL;
	wbd_device_item_t *device_vndr;
	wbd_bss_item_t *bss_vndr;
	wbd_info_t *info = master_info->parent->parent;
	WBD_ENTER();

	memset(&bcn_req, 0, sizeof(bcn_req));
	memset(&uassoc_req, 0, sizeof(uassoc_req));

	i5_bss = (i5_dm_bss_type*)WBD_I5LL_PARENT(i5_assoc_sta);
	i5_ifr = (i5_dm_interface_type*)WBD_I5LL_PARENT(i5_bss);
	i5_device = (i5_dm_device_type*)WBD_I5LL_PARENT(i5_ifr);
	WBD_ASSERT_ARG(i5_bss->vndr_data, WBDE_INV_ARG);

	WBD_SAFE_GET_I5_SELF_DEVICE(i5_self_device, &ret);
	bss_vndr = i5_bss->vndr_data;

	/* If the AP metrics query is already sent to this device within TBSS time,
	 * dont send it again
	 */
	if ((now - bss_vndr->apmetrics_timestamp) >= info->max.tm_wd_tbss) {
		/* Send AP Metrics Query message to the agent where STA associated. The agent will
		 * send AP metrics response, associated STA link metrics and STA traffic stats as
		 * in the metric report policy we have included flags which specifies to include
		 * associated sta link metrics and sta traffic stats
		 */
		ieee1905_send_ap_metrics_query(i5_device->DeviceId, i5_bss->BSSID, 1);
		bss_vndr->apmetrics_timestamp = now;
	}

	/* Check whether to send beacon metrics query or not */
	if (!WBD_BKT_DONT_SEND_BCN_QRY(master_info->parent->flags)) {
		/* Check if we already have beacon report from this sta */
		wbd_bcn_rpt = wbd_ds_find_item_fm_beacon_reports(info,
			(struct ether_addr*)&(i5_assoc_sta->mac), &ret);

		if (wbd_bcn_rpt &&
			(!eacmp(&(i5_device->DeviceId), &(wbd_bcn_rpt->neighbor_al_mac))) &&
			((now - wbd_bcn_rpt->timestamp) < WBD_MIN_BCN_METRIC_QUERY_DELAY)) {
			/* Already present reports can be sent */
			WBD_DEBUG("Reports are available for sta["MACDBG"] on device["MACDBG"]"
				"now[%lu]  timestamp[%lu] diff[%lu]\n",
				MAC2STRDBG(i5_assoc_sta->mac), MAC2STRDBG(i5_device->DeviceId),
				now, wbd_bcn_rpt->timestamp, (now - wbd_bcn_rpt->timestamp));
		} else {
			if (wbd_bcn_rpt) {
				wbd_ds_remove_beacon_report(info,
					(struct ether_addr *)&(i5_assoc_sta->mac));
			}
			/* Send Beacon Metrics Query message to the agent where STA associated */
			if (I5_CLIENT_IS_BSTA(i5_assoc_sta)) {
				ret = wbd_master_prepare_backhaul_beacon_request(i5_assoc_sta,
					&bcn_req);
			} else {
				ret = wbd_master_prepare_beacon_request(
					&master_info->ap_chan_report, i5_assoc_sta, &bcn_req);
			}
			if (ret == WBDE_OK) {
				ieee1905_send_beacon_metrics_query(i5_device->DeviceId, &bcn_req);
			}
		}
	}

	/* Send Unassociated STA Link Metrics Query message to all the other agents */
	uassoc_req.opClass = i5_ifr->opClass;
	uassoc_req.chCount = 1;

	uassoc_req.data = (unassoc_query_per_chan_rqst*)wbd_malloc(
		(sizeof(unassoc_query_per_chan_rqst)), &ret);
	WBD_ASSERT();
	per_chan_rqst = uassoc_req.data;

	per_chan_rqst->chan = wf_chspec_ctlchan(i5_ifr->chanspec);
	per_chan_rqst->n_sta = 1;

	per_chan_rqst->mac_list = (unsigned char*)wbd_malloc(ETHER_ADDR_LEN, &ret);
	WBD_ASSERT();
	memcpy(per_chan_rqst->mac_list, i5_assoc_sta->mac, ETHER_ADDR_LEN);

	i5_topology = ieee1905_get_datamodel();
	i5_iter_device = (i5_dm_device_type *)WBD_I5LL_HEAD(i5_topology->device_list);
	/* Traverse Device List to send it to all the device in the network */
	while (i5_iter_device != NULL) {
		uint8 send_unassoc_query = 1;

		/* Send it only to agent */
		if (I5_IS_MULTIAP_AGENT(i5_iter_device->flags) && i5_iter_device->vndr_data) {
			device_vndr = i5_iter_device->vndr_data;

			/* Check if it supports unassociated STA link metrics reporting ot not */
			if (i5_iter_device->BasicCaps & IEEE1905_AP_CAPS_FLAGS_UNASSOC_RPT) {
				/* if the device does not support unassociated STA link metrics on
				 * the channel its BSSs are not working. Check is there any
				 * interface is working on the same channel. If not do not send
				 * unassociated STA link metrics query
				 */
				if (!(i5_iter_device->BasicCaps &
					IEEE1905_AP_CAPS_FLAGS_UNASSOC_RPT_NON_CH)) {
					if (!wbd_master_is_agent_operating_on_channel(
						i5_iter_device, per_chan_rqst->chan)) {
						send_unassoc_query = 0;
						WBD_INFO("Device["MACDBG"] Does not support "
							"Unassociated STA Link Metrics reporting "
							"on channels its BSSs are not currently "
							"operating on CAPS[0x%02x]. "
							"Requested[0x%x]\n",
							MAC2STRDBG(i5_iter_device->DeviceId),
							i5_iter_device->BasicCaps,
							i5_ifr->chanspec);
					}
				}
			} else {
				send_unassoc_query = 0;
				WBD_INFO("Device["MACDBG"] Does not support Unassociated STA Link "
					"Metrics reporting CAPS[0x%02x].\n",
					MAC2STRDBG(i5_iter_device->DeviceId),
					i5_iter_device->BasicCaps);
			}

			if (send_unassoc_query) {
				ieee1905_send_unassoc_sta_link_metric_query(
					i5_iter_device->DeviceId, &uassoc_req);
			}

			/* If the neighbor link metrics query is already sent to this device within
			 * TBSS time, dont send it again
			 */
			if ((now - device_vndr->nbrlinkmetrics_timestamp) >= info->max.tm_wd_tbss) {
				/* Send the neighbor link metrics query requesting the link metrics
				 * to the self device(Controller)
				 */
				ieee1905_send_neighbor_link_metric_query(i5_iter_device->DeviceId,
					0, i5_self_device->DeviceId);
				device_vndr->nbrlinkmetrics_timestamp = now;
			}

			/* If the BSS metrics query is already sent to this device within TBSS time,
			 * dont send it again
			 */
			if ((now - device_vndr->bssmetrics_timestamp) >= info->max.tm_wd_tbss) {
				/* Send BSS Metrics Query */
				wbd_master_send_bss_metrics_query(i5_iter_device->DeviceId);
				device_vndr->bssmetrics_timestamp = now;
			}
		}
		i5_iter_device = WBD_I5LL_NEXT(i5_iter_device);
	}

end:
	/* free allocated data */
	if (bcn_req.ap_chan_report) {
		free(bcn_req.ap_chan_report);
	}

	if (uassoc_req.data) {
		per_chan_rqst = uassoc_req.data;
		if (per_chan_rqst->mac_list) {
			free(per_chan_rqst->mac_list);
		}
		free(uassoc_req.data);
	}

	WBD_EXIT();
	return ret;
}

/* Send 1905 Vendor Specific Weak Client Response command, from Controller to the Agent */
int
wbd_master_send_weak_client_response_cmd(wbd_cmd_weak_client_resp_t *cmdweakclientresp)
{
	int ret = WBDE_OK;
	ieee1905_vendor_data vndr_msg_data;
	WBD_ENTER();

	memset(&vndr_msg_data, 0x00, sizeof(vndr_msg_data));

	/* Allocate Dynamic mem for Vendor data from App */
	vndr_msg_data.vendorSpec_msg =
		(unsigned char *)wbd_malloc(IEEE1905_MAX_VNDR_DATA_BUF, &ret);
	WBD_ASSERT();

	/* Fill vndr_msg_data struct object to send Vendor Message */
	memcpy(vndr_msg_data.neighbor_al_mac,
		&cmdweakclientresp->cmdparam.dst_mac, IEEE1905_MAC_ADDR_LEN);

	WBD_INFO("Send Weak Client Response from Device["MACDBG"] to Device["MACDBG"]\n",
		MAC2STRDBG(ieee1905_get_al_mac()), MAC2STRDBG(vndr_msg_data.neighbor_al_mac));

	/* Encode Vendor Specific TLV for Message : Weak Client Response to send */
	ret = wbd_tlv_encode_weak_client_response((void *)cmdweakclientresp,
		vndr_msg_data.vendorSpec_msg, &vndr_msg_data.vendorSpec_len);
	WBD_ASSERT_MSG("Failed to encode Weak Client Response which needs to be sent "
		"from Device["MACDBG"] to Device["MACDBG"]\n",
		MAC2STRDBG(ieee1905_get_al_mac()), MAC2STRDBG(vndr_msg_data.neighbor_al_mac));

	/* Send Vendor Specific Message : Weak Client Response */
	ieee1905_send_vendor_specific_msg(&vndr_msg_data);

	WBD_CHECK_ERR_MSG(WBDE_DS_1905_ERR, "Failed to send "
		"Weak Client Response from Device["MACDBG"] to Device["MACDBG"], Error : %s\n",
		MAC2STRDBG(ieee1905_get_al_mac()), MAC2STRDBG(vndr_msg_data.neighbor_al_mac),
		wbderrorstr(ret));

end:
	if (vndr_msg_data.vendorSpec_msg) {
		free(vndr_msg_data.vendorSpec_msg);
	}
	WBD_EXIT();
	return ret;
}

/* Processes WEAK_CLIENT request */
static int
wbd_master_process_weak_client_cmd(wbd_master_info_t *master_info, i5_dm_clients_type *i5_assoc_sta,
	wbd_weak_sta_metrics_t *sta_stats, wbd_weak_sta_policy_t *metric_policy)
{
	int ret = WBDE_OK;
	wbd_cmd_weak_client_resp_t cmdweakclientresp;
	i5_dm_bss_type *i5_bss;
	i5_dm_interface_type *i5_ifr;
	i5_dm_device_type *i5_device;
	char logmsg[WBD_LOGS_BUF_128] = {0}, timestamp[WBD_MAX_BUF_32] = {0};
	WBD_ENTER();

	i5_bss = (i5_dm_bss_type*)WBD_I5LL_PARENT(i5_assoc_sta);
	i5_ifr = (i5_dm_interface_type*)WBD_I5LL_PARENT(i5_bss);
	i5_device = (i5_dm_device_type*)WBD_I5LL_PARENT(i5_ifr);

	/* process the WEAK_CLIENT data */
	memset(&cmdweakclientresp, 0x00, sizeof(cmdweakclientresp));
	ret = wbd_master_process_weak_client_cmd_data(master_info, i5_assoc_sta,
		&cmdweakclientresp);
	WBD_ASSERT();

end: /* Check Master Pointer before using it below */

	if ((ret == WBDE_OK) && (master_info)) {
		/* Create and store weak client log. */
		snprintf(logmsg, sizeof(logmsg), CLI_CMD_LOGS_WEAK,
			wbd_get_formated_local_time(timestamp, sizeof(timestamp)),
			MAC2STRDBG(i5_assoc_sta->mac), MAC2STRDBG(i5_bss->BSSID),
			sta_stats->rssi, metric_policy->t_rssi,
			sta_stats->tx_rate, metric_policy->t_tx_rate,
			sta_stats->tx_failures, metric_policy->t_tx_failures);
		wbd_ds_add_logs_in_master(master_info->parent, logmsg);
	}

#ifdef BCM_APPEVENTD
	if (ret == WBDE_OK) {
		/* Send weak client event to appeventd. */
		wbd_appeventd_weak_sta(APP_E_WBD_MASTER_WEAK_CLIENT, " ",
			(struct ether_addr*)i5_assoc_sta->mac, sta_stats->rssi,
			sta_stats->tx_failures, sta_stats->tx_rate);
	}
#endif /* BCM_APPEVENTD */

	/* Send vendor specific response to the agent if the weak client is not successfull */
	if (ret != WBDE_OK) {
		eacopy((struct ether_addr*)&i5_device->DeviceId,
			&cmdweakclientresp.cmdparam.dst_mac);
		eacopy((struct ether_addr*)&i5_assoc_sta->mac, &cmdweakclientresp.sta_mac);
		eacopy((struct ether_addr*)&i5_bss->BSSID, &cmdweakclientresp.BSSID);
		/* Convert wbd error code to weak client response reason codes */
		cmdweakclientresp.error_code = wbd_error_to_wc_resp_reason_code(ret);
		if (wbd_master_send_weak_client_response_cmd(&cmdweakclientresp) != WBDE_OK) {
			WBD_WARNING("BKTID[%d] Slave["MACF"] STA["MACF"] WEAK_CLIENT_RESP : %s\n",
				master_info->bkt_id,
				ETHER_TO_MACF(cmdweakclientresp.BSSID),
				ETHER_TO_MACF(cmdweakclientresp.sta_mac),
				wbderrorstr(WBDE_SEND_RESP_FL));
		}
	}

	/* Now send the link metrics if required */
	if ((ret == WBDE_OK) && (master_info)) {
		ret = wbd_master_send_link_metric_requests(master_info, i5_assoc_sta);
	}

	WBD_EXIT();
	return ret;
}

/* Processes WEAK_CANCEL request */
static void
wbd_master_process_weak_cancel_cmd(wbd_master_info_t *master, i5_dm_clients_type *i5_assoc_sta)
{
	int ret = WBDE_OK;
	wbd_cmd_weak_client_resp_t cmdweakclientresp;
	i5_dm_bss_type *i5_bss;
	i5_dm_interface_type *i5_ifr;
	i5_dm_device_type *i5_device;
	wbd_assoc_sta_item_t *assoc_sta;
	uint8 map_flags = IEEE1905_MAP_FLAG_FRONTHAUL;
	WBD_ENTER();

	WBD_ASSERT_ARG(i5_assoc_sta->vndr_data, WBDE_INV_ARG);

	i5_bss = (i5_dm_bss_type*)WBD_I5LL_PARENT(i5_assoc_sta);
	i5_ifr = (i5_dm_interface_type*)WBD_I5LL_PARENT(i5_bss);
	i5_device = (i5_dm_device_type*)WBD_I5LL_PARENT(i5_ifr);
	assoc_sta = i5_assoc_sta->vndr_data;

	if (I5_CLIENT_IS_BSTA(i5_assoc_sta)) {
		map_flags = IEEE1905_MAP_FLAG_BACKHAUL;
	}

	memset(&cmdweakclientresp, 0x00, sizeof(cmdweakclientresp));

	/* Update STA Status = Normal */
	WBD_DS_UP_ASSOCSTA_STATUS(master, assoc_sta, WBD_STA_STATUS_NORMAL);
	WBD_DEBUG("BKTID[%d] Blanket Weak STA Count : %d\n", master->bkt_id,
		master->weak_client_count);

	/* Remove STA from all peer BSS' Monitor STA List */
	if (wbd_ds_remove_sta_fm_peer_devices_monitorlist(
		(struct ether_addr*)i5_bss->BSSID,
		(struct ether_addr*)i5_assoc_sta->mac, map_flags) != WBDE_OK) {
		WBD_INFO("BSS["MACDBG"] STA["MACDBG"]. Failed to remove STA from "
			"peer slaves monitorlist\n",
			MAC2STRDBG(i5_bss->BSSID), MAC2STRDBG(i5_assoc_sta->mac));
	}

	/* Send the response as failed by default as it helps when the STA is weak in the BSD but
	 * in the master it is not detected as weak for some reason. After BSD sends weak message,
	 * it will not try to send the weak message again unless there is a fail.
	 */
	eacopy((struct ether_addr*)&i5_device->DeviceId,
		&cmdweakclientresp.cmdparam.dst_mac);
	eacopy((struct ether_addr*)&i5_assoc_sta->mac, &cmdweakclientresp.sta_mac);
	eacopy((struct ether_addr*)&i5_bss->BSSID, &cmdweakclientresp.BSSID);
	/* Convert wbd error code to weak cancel response reason codes */
	cmdweakclientresp.error_code = wbd_error_to_wc_resp_reason_code(WBDE_FAIL_XX);
	if (wbd_master_send_weak_client_response_cmd(&cmdweakclientresp) != WBDE_OK) {
		WBD_WARNING("BKTID[%d] Slave["MACF"] STA["MACF"] WEAK_CANCEL_RESP : %s\n",
			master->bkt_id,
			ETHER_TO_MACF(cmdweakclientresp.BSSID),
			ETHER_TO_MACF(cmdweakclientresp.sta_mac),
			wbderrorstr(WBDE_SEND_RESP_FL));
	}

end:
	WBD_EXIT();
}

#ifdef WLHOSTFBT
/* Get FBT_CONFIG_RESP Data from 1905 Device */
static int
wbd_master_get_fbt_config_resp_data(wbd_cmd_fbt_config_t *fbt_config_resp)
{
	int ret = WBDE_OK;
	dll_t *fbt_item_p = NULL;
	wbd_fbt_bss_entry_t *fbt_data = NULL;
	wbd_fbt_bss_entry_t new_fbt_bss;
	wbd_master_info_t *master = NULL;
	wbd_info_t *info = wbd_get_ginfo();
	wbd_cmd_fbt_config_t *ctlr_fbt_config = &(info->wbd_fbt_config);
	WBD_ENTER();

	/* Initialize fbt_config_resp's Total item count in caller  */
	WBD_INFO("Get FBT Config Response Data\n");

	/* Travese FBT Information of all BSSes in this Blanket */
	foreach_glist_item(fbt_item_p, ctlr_fbt_config->entry_list) {

		fbt_data = (wbd_fbt_bss_entry_t *)fbt_item_p;

		/* Find matching Master for this Interface's blanket */
		WBD_DS_GET_MASTER_INFO(info, WBD_BKT_ID_BR0, master, (&ret));
		if (ret != WBDE_OK) {
			ret = WBDE_OK;
			continue;
		}

		/* If FBT is not Enabled on RootAP, Leave */
		if (!WBD_FBT_ENAB(master->flags)) {
			ret = WBDE_DS_FBT_NT_POS;
			continue;
		}

		WBD_INFO("WBD_FBT_ENAB Enabled for  : "
			"Blanket[BR%d] DEVICE["MACDBG"] BRDG["MACDBG"] BSS["MACDBG"] "
			"MDID[%d] FT_CAP[%d] FT_REASSOC[%d]\n",
			fbt_data->blanket_id, MAC2STRDBG(fbt_data->al_mac),
			MAC2STRDBG(fbt_data->bss_br_mac), MAC2STRDBG(fbt_data->bssid),
			master->fbt_info.mdid,
			master->fbt_info.ft_cap_policy,
			master->fbt_info.tie_reassoc_interval);

		/* Prepare a new FBT BSS item for Master's FBT Response List */
		memset(&new_fbt_bss, 0, sizeof(new_fbt_bss));
		memset(&(new_fbt_bss.fbt_info), 0, sizeof(new_fbt_bss.fbt_info));

		new_fbt_bss.blanket_id = fbt_data->blanket_id;
		memcpy(new_fbt_bss.al_mac, fbt_data->al_mac,
			sizeof(new_fbt_bss.al_mac));
		memcpy(new_fbt_bss.bssid, fbt_data->bssid,
			sizeof(new_fbt_bss.bssid));
		memcpy(new_fbt_bss.bss_br_mac, fbt_data->bss_br_mac,
			sizeof(new_fbt_bss.bss_br_mac));

		new_fbt_bss.len_r0kh_id = strlen(fbt_data->r0kh_id);
		memcpy(new_fbt_bss.r0kh_id, fbt_data->r0kh_id,
			sizeof(new_fbt_bss.r0kh_id));
		new_fbt_bss.len_r0kh_key = strlen(fbt_data->r0kh_key);
		memcpy(new_fbt_bss.r0kh_key, fbt_data->r0kh_key,
			sizeof(new_fbt_bss.r0kh_key));

		/* Assign FBT info, while sending FBT_CONFIG_RESP */
		new_fbt_bss.fbt_info.mdid = master->fbt_info.mdid;
		new_fbt_bss.fbt_info.ft_cap_policy = master->fbt_info.ft_cap_policy;
		new_fbt_bss.fbt_info.tie_reassoc_interval =
			master->fbt_info.tie_reassoc_interval;

		WBD_INFO("Adding new entry to FBT_CONFIG_RESP : "
			"Blanket[BR%d] Device["MACDBG"] "
			"BRDG["MACDBG"] BSS["MACDBG"] "
			"R0KH_ID[%s] LEN_R0KH_ID[%d] "
			"R0KH_Key[%s] LEN_R0KH_Key[%d] "
			"MDID[%d] FT_CAP[%d] FT_REASSOC[%d]\n",
			new_fbt_bss.blanket_id,
			MAC2STRDBG(new_fbt_bss.al_mac),
			MAC2STRDBG(new_fbt_bss.bss_br_mac),
			MAC2STRDBG(new_fbt_bss.bssid),
			new_fbt_bss.r0kh_id, new_fbt_bss.len_r0kh_id,
			new_fbt_bss.r0kh_key, new_fbt_bss.len_r0kh_key,
			new_fbt_bss.fbt_info.mdid,
			new_fbt_bss.fbt_info.ft_cap_policy,
			new_fbt_bss.fbt_info.tie_reassoc_interval);

		/* Add a FBT BSS item in a Slave's FBT Response List, Total item count++ */
		wbd_add_item_in_fbt_cmdlist(&new_fbt_bss,
			fbt_config_resp, NULL);
	}

	WBD_EXIT();
	return ret;
}

/* Send 1905 Vendor Specific FBT_CONFIG_RESP command, from Controller to Agents */
static int
wbd_master_broadcast_fbt_config_resp_cmd()
{
	int ret = WBDE_OK;

	ieee1905_vendor_data vndr_msg_data;
	wbd_cmd_fbt_config_t fbt_config_resp; /* FBT Config Response Data */
	i5_dm_network_topology_type *ctlr_topology;
	i5_dm_device_type *iter_dev, *self_device;
	WBD_ENTER();

	memset(&vndr_msg_data, 0, sizeof(vndr_msg_data));

	/* Initialize FBT Config Response Data */
	memset(&fbt_config_resp, 0, sizeof(fbt_config_resp));
	wbd_ds_glist_init(&(fbt_config_resp.entry_list));

	/* Allocate Dynamic mem for Vendor data from App */
	vndr_msg_data.vendorSpec_msg =
		(unsigned char *)wbd_malloc(IEEE1905_MAX_VNDR_DATA_BUF, &ret);
	WBD_ASSERT();

	/* Get FBT Config Response Data from 1905 Device */
	wbd_master_get_fbt_config_resp_data(&fbt_config_resp);

	/* Encode Vendor Specific TLV for Message : FBT_CONFIG_RESP to send */
	wbd_tlv_encode_fbt_config_response((void *)&fbt_config_resp,
		vndr_msg_data.vendorSpec_msg, &vndr_msg_data.vendorSpec_len);

	/* Get Topology of Controller from 1905 lib */
	ctlr_topology = (i5_dm_network_topology_type *)ieee1905_get_datamodel();

	WBD_SAFE_GET_I5_SELF_DEVICE(self_device, &ret);

	/* Loop for all the Devices in Controller, to send FBT_CONFIG_RESP to all Agents */
	foreach_i5glist_item(iter_dev, i5_dm_device_type, ctlr_topology->device_list) {

		/* Loop for, other than self (Controller) device, only for Agent Devices */
		if (memcmp(self_device->DeviceId, iter_dev->DeviceId, MAC_ADDR_LEN) == 0) {
			continue;
		}

		/* Fill Destination AL_MAC */
		memcpy(vndr_msg_data.neighbor_al_mac, iter_dev->DeviceId, IEEE1905_MAC_ADDR_LEN);

		WBD_INFO("Send FBT_CONFIG_RESP from Device["MACDBG"] to Device["MACDBG"]\n",
			MAC2STRDBG(ieee1905_get_al_mac()), MAC2STRDBG(iter_dev->DeviceId));

		/* Send Vendor Specific Message : FBT_CONFIG_RESP */
		ieee1905_send_vendor_specific_msg(&vndr_msg_data);

		WBD_CHECK_ERR_MSG(WBDE_DS_1905_ERR, "Failed to send "
			"FBT_CONFIG_RESP from Device["MACDBG"], Error : %s\n",
			MAC2STRDBG(ieee1905_get_al_mac()), wbderrorstr(ret));
	}

end:
	/* Remove all FBT Config Response data items */
	wbd_ds_glist_cleanup(&(fbt_config_resp.entry_list));

	if (vndr_msg_data.vendorSpec_msg) {
		free(vndr_msg_data.vendorSpec_msg);
	}

	WBD_EXIT();
	return ret;

}

/* Process 1905 Vendor Specific FBT_CONFIG_REQ Message */
static int
wbd_master_process_fbt_config_req_cmd(unsigned char *neighbor_al_mac,
	unsigned char *tlv_data, unsigned short tlv_data_len)
{
	int ret = WBDE_OK;
	wbd_cmd_fbt_config_t fbt_config_req; /* FBT Config Request Data */
	wbd_info_t *info = wbd_get_ginfo();
	wbd_cmd_fbt_config_t *ctlr_fbt_config = &(info->wbd_fbt_config);
	dll_t *fbt_item_p = NULL;
	wbd_fbt_bss_entry_t *fbt_data = NULL;
	char logmsg[WBD_MAX_BUF_128] = {0}, timestamp[WBD_MAX_BUF_32] = {0};

	WBD_ENTER();

	/* Initialize FBT Config Request Data */
	memset(&fbt_config_req, 0, sizeof(fbt_config_req));
	wbd_ds_glist_init(&(fbt_config_req.entry_list));

	/* Decode Vendor Specific TLV for Message : FBT_CONFIG_REQ on receive */
	ret = wbd_tlv_decode_fbt_config_request((void *)&fbt_config_req, tlv_data, tlv_data_len);
	WBD_ASSERT_MSG("Failed to decode FBT Config Request From DEVICE["MACDBG"]\n",
		MAC2STRDBG(neighbor_al_mac));

	/* Travese FBT Config Request List items */
	foreach_glist_item(fbt_item_p, fbt_config_req.entry_list) {

		wbd_fbt_bss_entry_t *matching_fbt_bss = NULL;

		fbt_data = (wbd_fbt_bss_entry_t *)fbt_item_p;
		memcpy(fbt_data->al_mac, neighbor_al_mac, sizeof(fbt_data->al_mac));

		/* Find matching fbt_info for this bssid */
		matching_fbt_bss = wbd_find_fbt_bss_item_for_bssid(fbt_data->bssid,
			ctlr_fbt_config);

		/* If not FOUND, Create Add Received fbt_info for this bssid */
		if (matching_fbt_bss) {
			WBD_INFO("Master Found Matching AL_MAC["MACDBG"] BSSID["MACDBG"]\n",
				MAC2STRDBG(fbt_data->al_mac), MAC2STRDBG(fbt_data->bssid));
		} else {

			WBD_INFO("Master NOT Found Matching AL_MAC["MACDBG"] BSSID["MACDBG"]. "
				"ADD NEW FBT_INFO.\n",
				MAC2STRDBG(fbt_data->al_mac), MAC2STRDBG(fbt_data->bssid));

			/* Add a FBT BSS item in a Slave's FBT Request List */
			wbd_add_item_in_fbt_cmdlist(fbt_data, ctlr_fbt_config, NULL);
		}

		WBD_INFO("From Blanket[BR%d] DEVICE["MACDBG"] BRDG["MACDBG"] BSS["MACDBG"] "
			"Received FBT_CONFIG_REQ : "
			"R0KH_ID[%s] LEN_R0KH_ID[%zu] "
			"R0KH_Key[%s] LEN_R0KH_Key[%zu] "
			"MDID[%d] FT_CAP[%d] FT_REASSOC[%d]\n",
			fbt_data->blanket_id, MAC2STRDBG(fbt_data->al_mac),
			MAC2STRDBG(fbt_data->bss_br_mac), MAC2STRDBG(fbt_data->bssid),
			fbt_data->r0kh_id, strlen(fbt_data->r0kh_id),
			fbt_data->r0kh_key, strlen(fbt_data->r0kh_key),
			fbt_data->fbt_info.mdid,
			fbt_data->fbt_info.ft_cap_policy,
			fbt_data->fbt_info.tie_reassoc_interval);
	}

	/* Send 1905 Vendor Specific FBT_CONFIG_RESP Message to all Agents */
	wbd_master_broadcast_fbt_config_resp_cmd();

end:
	/* Remove all FBT Config Request data items */
	wbd_ds_glist_cleanup(&(fbt_config_req.entry_list));

	/* Create and store MAP Init End log */
	snprintf(logmsg, sizeof(logmsg), CLI_CMD_LOGS_MAP_INIT_END,
		wbd_get_formated_local_time(timestamp, sizeof(timestamp)),
		MAC2STRDBG(ieee1905_get_al_mac()));
	wbd_ds_add_logs_in_master(info->wbd_master, logmsg);

#ifdef BCM_APPEVENTD
	/* Send MAP Init Start event to appeventd. */
	wbd_appeventd_map_init(APP_E_WBD_MASTER_MAP_INIT_END,
		(struct ether_addr*)ieee1905_get_al_mac(), MAP_INIT_END, MAP_APPTYPE_MASTER);
#endif /* BCM_APPEVENTD */

	WBD_EXIT();
	return ret;
}
#endif /* WLHOSTFBT */

/* Process 1905 Vendor Specific Associated STA Link Metrics Vendor TLV */
static int
wbd_master_process_assoc_sta_metric_vndr_tlv(unsigned char *neighbor_al_mac,
	unsigned char *tlv_data, unsigned short tlv_data_len)
{
	int ret = WBDE_OK;
	i5_dm_bss_type *i5_bss;
	wbd_assoc_sta_item_t *assoc_sta;
	wbd_cmd_vndr_assoc_sta_metric_t cmd; /* STA Metric Vndr Data */
	wbd_prb_sta_t *prbsta;
	WBD_ENTER();

	/* Initialize Vendor Specific Associated STA Link Metrics */
	memset(&cmd, 0, sizeof(cmd));

	/* Decode Vendor Specific TLV : Associated STA Link Metrics Vendor Data on receive */
	ret = wbd_tlv_decode_vndr_assoc_sta_metrics((void *)&cmd, tlv_data, tlv_data_len);
	WBD_ASSERT_MSG("Failed to decode Assoc STA Metrics From DEVICE["MACDBG"]\n",
		MAC2STRDBG(neighbor_al_mac));

	WBD_INFO("Data Received VNDR_ASSOC_STA_LINK_METRIC : "
		"SRC_BSSID["MACDBG"] STA_MAC["MACDBG"] "
		"Idle_rate[%d] Tx_failures[%d] STA Cap[0x%02x] Tx_tot_failures[%d]\n",
		MAC2STRDBG(cmd.src_bssid), MAC2STRDBG(cmd.sta_mac),
		cmd.vndr_metrics.idle_rate, cmd.vndr_metrics.tx_failures,
		cmd.vndr_metrics.sta_cap, cmd.vndr_metrics.tx_tot_failures);

	/* Store the data */
	WBD_SAFE_FIND_I5_BSS_IN_DEVICE(neighbor_al_mac, cmd.src_bssid, i5_bss, &ret);

	/* Check if the STA exists or not */
	wbd_ds_find_sta_in_bss_assoclist(i5_bss, (struct ether_addr*)cmd.sta_mac,
		&ret, &assoc_sta);
	WBD_ASSERT_MSG("Device["MACDBG"] BSSID["MACDBG"] STA["MACDBG"]. %s\n",
		MAC2STRDBG(neighbor_al_mac), MAC2STRDBG(i5_bss->BSSID), MAC2STRDBG(cmd.sta_mac),
		wbderrorstr(ret));

	assoc_sta->stats.tx_failures = cmd.vndr_metrics.tx_failures;
	/* tx_failures != 0 indicated readings from BSD. Since BSD provides
	 * total failures as well as failures in last few seconds update the
	 * last active time to current time
	 */
	if (cmd.vndr_metrics.tx_failures != 0) {
		assoc_sta->stats.active = time(NULL);
		assoc_sta->stats.old_tx_tot_failures = cmd.vndr_metrics.tx_tot_failures -
			cmd.vndr_metrics.tx_failures;
	} else {
		assoc_sta->stats.old_tx_tot_failures = assoc_sta->stats.tx_tot_failures;
	}
	assoc_sta->stats.tx_tot_failures = cmd.vndr_metrics.tx_tot_failures;
	assoc_sta->stats.idle_rate = cmd.vndr_metrics.idle_rate;

	/* Update Dual band capability only if it is dual band */
	if (cmd.vndr_metrics.sta_cap & WBD_TLV_ASSOC_STA_CAPS_FLAG_DUAL_BAND) {
		/* Get probe STA entry and update it as it supports dual band */
		prbsta = wbd_ds_find_sta_in_probe_sta_table(wbd_get_ginfo(),
			(struct ether_addr*)cmd.sta_mac, FALSE);
		if (prbsta) {
			prbsta->band = WBD_BAND_LAN_ALL;
		}
	}

end:
	WBD_EXIT();
	return ret;
}

/* Process 1905 Vendor Specific Messages at WBD Application Layer */
int
wbd_master_process_vendor_specific_msg(ieee1905_vendor_data *msg_data)
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

		/* For TLv, Initialize data pointers and counters */
		tlv_data = &msg_data->vendorSpec_msg[pos];

		/* Pointer is at the next TLV */
		i5_tlv_t *ptlv = (i5_tlv_t *)tlv_data;

		/* Get next TLV's data length (Hdr bytes skipping done in fn wbd_tlv_decode_xxx) */
		tlv_data_len = ntohs(ptlv->length);

		WBD_DEBUG("vendorSpec_len[%d] tlv_hdr_len[%d] tlv_data_len[%d] pos[%d]\n",
			msg_data->vendorSpec_len, tlv_hdr_len, tlv_data_len, pos);

		switch (ptlv->type) {

			case WBD_TLV_FBT_CONFIG_REQ_TYPE:
				WBD_INFO("Processing %s vendorSpec_len[%d]\n",
					wbd_tlv_get_type_str(WBD_TLV_FBT_CONFIG_REQ_TYPE),
					msg_data->vendorSpec_len);

#ifdef WLHOSTFBT
				/* Process 1905 Vendor Specific FBT_CONFIG_REQ Message */
				wbd_master_process_fbt_config_req_cmd(
					msg_data->neighbor_al_mac, tlv_data, tlv_data_len);
#endif /* WLHOSTFBT */
				break;

			case WBD_TLV_VNDR_ASSOC_STA_METRICS_TYPE:
				WBD_INFO("Processing %s vendorSpec_len[%d]\n",
					wbd_tlv_get_type_str(WBD_TLV_VNDR_ASSOC_STA_METRICS_TYPE),
					msg_data->vendorSpec_len);

				/* Process 1905 Vendor Specific Assoc STA Metrics Vendor TLV */
				wbd_master_process_assoc_sta_metric_vndr_tlv(
					msg_data->neighbor_al_mac, tlv_data, tlv_data_len);

				break;

			case WBD_TLV_BSS_CAPABILITY_REPORT_TYPE:
				WBD_INFO("Processing %s vendorSpec_len[%d]\n",
					wbd_tlv_get_type_str(ptlv->type),
					msg_data->vendorSpec_len);
				/* Process 1905 Vendor Specific BSS Capability Report Message */
				wbd_master_process_bss_capability_report_cmd(
					msg_data->neighbor_al_mac, tlv_data, tlv_data_len);
				break;

			case WBD_TLV_BSS_METRICS_REPORT_TYPE:
				WBD_INFO("Processing %s vendorSpec_len[%d]\n",
					wbd_tlv_get_type_str(ptlv->type),
					msg_data->vendorSpec_len);
				/* Process 1905 Vendor Specific BSS Metrics Report Message */
				wbd_master_process_bss_metrics_report_cmd(
					msg_data->neighbor_al_mac, tlv_data, tlv_data_len);
				break;

			case WBD_TLV_STEER_RESP_REPORT_TYPE:
				WBD_INFO("Processing %s vendorSpec_len[%d]\n",
					wbd_tlv_get_type_str(ptlv->type),
					msg_data->vendorSpec_len);
				/* Process 1905 Vendor Specific Steer Response Report Message */
				wbd_master_process_steer_resp_report_cmd(
					msg_data->neighbor_al_mac, tlv_data, tlv_data_len);
				break;
			case WBD_TLV_ZWDFS_TYPE:
				WBD_INFO("Processing %s vendorSpec_len[%d]\n",
					wbd_tlv_get_type_str(ptlv->type),
					msg_data->vendorSpec_len);
				/* Process 1905 Vendor Specific Zero wait dfs Message */
				wbd_master_process_zwdfs_cmd(msg_data->neighbor_al_mac,
					tlv_data, tlv_data_len);
				break;
			case WBD_TLV_CHAN_INFO_TYPE:
				WBD_INFO("Processing %s vendorSpec_len[%d]\n",
					wbd_tlv_get_type_str(ptlv->type),
					msg_data->vendorSpec_len);

				/* Process 1905 Vendor Specific chan info and chanspec list */
				wbd_master_process_intf_chan_info(msg_data->neighbor_al_mac,
					tlv_data, tlv_data_len);
				break;
			default:
				WBD_WARNING("Vendor TLV[%s] processing not Supported by Master.\n",
					wbd_tlv_get_type_str(ptlv->type));
				break;
		}
	}

	WBD_DEBUG("vendorSpec_len[%d] tlv_hdr_len[%d] tlv_data_len[%d] pos[%d]\n",
		msg_data->vendorSpec_len, tlv_hdr_len, tlv_data_len, pos);

	WBD_EXIT();
	return ret;
}

#if defined(MULTIAPR2)
/* Send Channel Scan Request Message */
static void
wbd_master_send_chscan_req(unsigned char *al_mac, bool fresh_scan_req)
{
	int ret = WBDE_OK, num_of_radios = 0;
	ieee1905_chscan_req_msg chscan_req;
	ieee1905_per_radio_opclass_list *radio_info = NULL;
	ieee1905_per_opclass_chan_list *opclass_info_1 = NULL, *opclass_info_2 = NULL;

	i5_dm_interface_type *ifr = NULL;
	i5_dm_device_type *dev = NULL;

	memset(&chscan_req, 0, sizeof(chscan_req));

	/* Initialize radio list */
	ieee1905_glist_init(&chscan_req.radio_list);

	WBD_SAFE_FIND_I5_DEVICE(dev, al_mac, &ret);

	/* Insert Channel Scan Request TLV Flags */
	if (fresh_scan_req) {
		chscan_req.chscan_req_msg_flag |= MAP_CHSCAN_REQ_FRESH_SCAN;
	}

	/* Insert Number of radios : upon which channel scans are requested : In the End */

	/* Insert details for each radio */
	foreach_i5glist_item(ifr, i5_dm_interface_type, dev->interface_list) {

		/* Loop for, only for wireless interfaces with valid chanspec & BSSes */
		if (!i5DmIsInterfaceWireless(ifr->MediaType) || !ifr->BSSNumberOfEntries) {
			WBD_INFO("Not a wireless interface "MACF" OR no BSSes\n",
				ETHERP_TO_MACF(ifr->InterfaceId));
			continue;
		}

		/* Allocate per_radio_opclass Info structure */
		radio_info = (ieee1905_per_radio_opclass_list*)
			wbd_malloc(sizeof(*radio_info), &ret);
		WBD_ASSERT();

		/* Initialize opclass list */
		ieee1905_glist_init(&radio_info->opclass_list);

		/* Insert Radio Unique Identifier of a radio of the Multi-AP Agent */
		memcpy(radio_info->radio_mac, ifr->InterfaceId, MAC_ADDR_LEN);

		/* If Fresh Scan is not Required, Number of Operating Classes = 0 */
		if (!fresh_scan_req) {

			/* Insert Number of Operating Classes */
			radio_info->num_of_opclass = 0;
		} else {

			/* Insert Number of Operating Classes */
			radio_info->num_of_opclass = 2;
		}

		/* If Fresh Scan is Required, specify Opclass & Channel Numbers,
		 * If Fresh Scan is not Required, skip this part
		 */
		if (fresh_scan_req) {

			/* Allocate per_opclass_chan Info structure */
			opclass_info_1 = (ieee1905_per_opclass_chan_list *)
				wbd_malloc(sizeof(*opclass_info_1), &ret);
			WBD_ASSERT();

			/* Allocate per_opclass_chan Info structure */
			opclass_info_2 = (ieee1905_per_opclass_chan_list *)
				wbd_malloc(sizeof(*opclass_info_2), &ret);
			WBD_ASSERT();

			if (CHSPEC_IS2G(ifr->chanspec)) {

				/* Insert Operating Class Value 1 */
				opclass_info_1->opclass_val = 81;
				/* Insert Number of Channels specified in the Channel List */
				opclass_info_1->num_of_channels = 3;
				/* Insert Octets of Channels specified in the Channel List */
				opclass_info_1->chan_list[0] = wf_chspec_ctlchan(0x1007);
				/* Insert Octets of Channels specified in the Channel List */
				opclass_info_1->chan_list[1] = wf_chspec_ctlchan(0x1008);
				/* Insert Octets of Channels specified in the Channel List */
				opclass_info_1->chan_list[2] = wf_chspec_ctlchan(0x1009);

				/* Insert Operating Class Value 2 */
				opclass_info_2->opclass_val = 82;
				/* Insert Number of Channels specified in the Channel List */
				opclass_info_2->num_of_channels = 1;
				/* Insert Octets of Channels specified in the Channel List */
				opclass_info_2->chan_list[0] = wf_chspec_ctlchan(0x100e);

				WBD_INFO("Send 2G ChannelScanReq IFR["MACF"] :\n"
				"opclass_1[%d] chan_1[%d] chan_2[%d] chan_3[%d]\n"
				"opclass_2[%d] chan_1[%d]\n",
				ETHERP_TO_MACF(ifr->InterfaceId),
				opclass_info_1->opclass_val, opclass_info_1->chan_list[0],
				opclass_info_1->chan_list[1], opclass_info_1->chan_list[2],
				opclass_info_2->opclass_val, opclass_info_2->chan_list[0]);

			} else {

				/* Insert Operating Class Value 1 */
				opclass_info_1->opclass_val = 115;
				/* Insert Number of Channels specified in the Channel List */
				opclass_info_1->num_of_channels = 2;
				/* Insert Octets of Channels specified in the Channel List */
				opclass_info_1->chan_list[0] = wf_chspec_ctlchan(0xd024);
				/* Insert Octets of Channels specified in the Channel List */
				opclass_info_1->chan_list[1] = wf_chspec_ctlchan(0xd028);

				/* Insert Operating Class Value 2 */
				opclass_info_2->opclass_val = 121;
				/* Insert Number of Channels specified in the Channel List */
				opclass_info_2->num_of_channels = 2;
				/* Insert Octets of Channels specified in the Channel List */
				opclass_info_2->chan_list[0] = wf_chspec_ctlchan(0xd064);
				/* Insert Octets of Channels specified in the Channel List */
				opclass_info_2->chan_list[1] = wf_chspec_ctlchan(0xd068);

				WBD_INFO("Send 5G ChannelScanReq IFR["MACF"] :\n"
				"opclass_1[%d] chan_1[%d] chan_2[%d]\n"
				"opclass_2[%d] chan_1[%d] chan_2[%d]\n",
				ETHERP_TO_MACF(ifr->InterfaceId),
				opclass_info_1->opclass_val, opclass_info_1->chan_list[0],
				opclass_info_1->chan_list[1], opclass_info_2->opclass_val,
				opclass_info_2->chan_list[0], opclass_info_2->chan_list[1]);

			}

			/* Append per_radio_opclass Info structure 1 */
			ieee1905_glist_append(&radio_info->opclass_list, (dll_t*)opclass_info_1);
			/* Append per_radio_opclass Info structure 2 */
			ieee1905_glist_append(&radio_info->opclass_list, (dll_t*)opclass_info_2);

		}

		/* Append per_radio_opclass Info structure */
		ieee1905_glist_append(&chscan_req.radio_list, (dll_t*)radio_info);

		num_of_radios++;
	}

	/* Insert Number of radios : upon which channel scans are requested : Fill now */
	chscan_req.num_of_radios = num_of_radios;

	/* WBD_INFO("Sending Requested { %s } Channel Scan Report Message to ["MACF"]\n",
	 *	fresh_scan_req ? "FRESH" : "STORED", ETHERP_TO_MACF(al_mac));
	 */

	/* Send Channel Scan Request Message to a Multi AP Device  - Avoiding for now
	 * i5MessageChannelScanRequestSend(dev->psock, al_mac, &chscan_req);
	 */

end:
	/* Free the memory allocated for Channel Scan Request Msg structure */
	i5DmChannelScanRequestInfoFree(&chscan_req);

	return;
}
#endif /* MULTIAPR2 */

/* Set AP configured flag */
void wbd_master_set_ap_configured(unsigned char *al_mac, unsigned char *radio_mac, int if_band)
{
	int ret = WBDE_OK;
	i5_dm_interface_type *interface;
	i5_dm_device_type *i5_device;

	WBD_ENTER();

	if (!al_mac || !radio_mac) {
		WBD_ERROR("Invalid Device or Radio address\n");
		goto end;
	}

	interface = wbd_ds_get_i5_interface(al_mac, radio_mac, &ret);
	WBD_ASSERT();

	interface->isConfigured = 1;
	interface->band = if_band;

	/* Send some device speific message. Note: This callback comes for every interface on the
	 * device. Set a flag to indicate its already sent so that next time before sending one
	 * can check it
	 */
	i5_device = (i5_dm_device_type*)WBD_I5LL_PARENT(interface);
	/* Send NVRAM set vendor message if it is not sent */
	wbd_master_send_vndr_nvram_set_cmd(i5_device);

end:
	WBD_EXIT();
}

/* Handle operating channel report */
void wbd_master_process_operating_chan_report(wbd_info_t *info, unsigned char *al_mac,
	ieee1905_operating_chan_report *chan_report)
{
	wbd_master_info_t *master_info;
	i5_dm_network_topology_type *i5_topology;
	i5_dm_device_type *i5_iter_device;
	i5_dm_interface_type *i5_iter_ifr, *i5_ifr;
	i5_dm_device_type *self_device;
	i5_dm_interface_type *self_intf;
	uint8 channel = 0;
	uint bw = 0;
	uint16 chspec = 0;
	int ret = WBDE_OK;
	int agent_if_band, controller_if_band, if_band;
	WBD_ENTER();

	i5_topology = ieee1905_get_datamodel();
	if (!chan_report || !(chan_report->list)) {
		WBD_ERROR(" Invalid chan report, return \n");
		goto end;
	}
	/* following piece of code needs to add at onboarding time to take care of
	 * cross model working i.e. dual band <-> Tri band
	 */
	agent_if_band = controller_if_band  = if_band = BAND_INV;

	WBD_SAFE_GET_I5_IFR(al_mac, chan_report->radio_mac, i5_ifr, &ret);
	agent_if_band = i5_ifr->band;

	if (!WBD_BAND_VALID(agent_if_band)) {
		WBD_ERROR(" Invalid band for agent, return \n");
		return;
	}

	WBD_SAFE_GET_MASTER_INFO(info, WBD_BKT_ID_BR0, master_info, &ret);
	/* support for only one 1 opclass and 1 channel */
	blanket_get_bw_from_rc(chan_report->list->op_class, &bw);
	chspec = wf_channel2chspec(chan_report->list->chan, bw);
	if (chspec == 0) {
		WBD_ERROR("get chspec failed. channel: %d rc: %d bw: %d\n",
			chan_report->list->chan, chan_report->list->op_class, bw);
		goto end;
	}

	i5_ifr->opClass = chan_report->list->op_class;
	i5_ifr->chanspec = chspec;

	/* Create AP chan report */
	wbd_master_create_ap_chan_report(i5_topology, master_info);

	if (WBD_MCHAN_ENAB(info->flags)) {
		WBD_INFO("Multi chan mode is ON, no need to update other agent's chanspec \n");
		goto end;
	}

	if (i5_ifr->MediaSpecificInfo[6] & I5_MEDIA_INFO_ROLE_STA) {
		WBD_INFO("If primary is STA, it follows the associated AP's channel. "
			"So don't update other agent's chanspec\n");
		goto end;
	}

	if (blanket_get_config_val_int(NULL, WBD_NVRAM_ENAB_SINGLE_CHAN_BH, 0) == 0) {
		if (wbd_ds_is_interface_dedicated_backhaul(i5_ifr)) {
			WBD_INFO("Interface "MACF" is dedicated backhaul. No need to update "
				"other agent's chanspec\n", ETHERP_TO_MACF(i5_ifr->InterfaceId));
			goto end;
		}
	}

	WBD_SAFE_GET_I5_SELF_DEVICE(self_device, &ret);

	foreach_i5glist_item(self_intf, i5_dm_interface_type, self_device->interface_list) {

		if ((self_intf->chanspec == 0) || !i5DmIsInterfaceWireless(self_intf->MediaType)) {
			continue;
		}

		/* get controller's interface matching with same band as requested in
		 * channel report, if chanspec of band is different update corresponding
		 * interface's chanspec
		 */
		channel = wf_chspec_ctlchan(self_intf->chanspec);

		if (agent_if_band & (WBD_BAND_FROM_CHANNEL(channel))) {
			/* Support:
			 * dual band controller  - dual band agent
			 * tri band controller - tri band agent.
			 * Get valid interface on which to apply operating channel
			 * report parameters.
			 * Also consider switch from 5G Low to 5G high and vice versa
			 * for following case:
			 * Any agent switch from 5g low/high to high/low channel
			 * Update the chanspec of controller's interface accordingly
			 */
			controller_if_band = agent_if_band;
			WBD_INFO("controller_band[%d], agent_if_band[%d]\n", controller_if_band,
				agent_if_band);
		}

		if (agent_if_band != controller_if_band) {
			WBD_DEBUG("Requested band=%d does not match with Controller's"
				"interface band=%d, look for other interface \n",
				agent_if_band, WBD_BAND_FROM_CHANNEL(channel));

			continue;
		}
		if (chspec == self_intf->chanspec) {
			WBD_INFO("Agent "MACF" send operating channel report for interface "
				MACF" in reposne to channel selection request, Ignore\n",
				ETHERP_TO_MACF(al_mac), ETHERP_TO_MACF(i5_ifr->InterfaceId));
			return;
		} else {
			/* band is same, some agent changed the channel during normal
			 * operation update controller's interface chanspec and inform
			 * all other agents for the new channel with channel selection
			 * request
			 */
			self_intf->chanspec = chspec;
			WBD_INFO("update controller's %s chanspec to %x \n",
				self_intf->ifname, self_intf->chanspec);

			break;
		}
	}

	if (self_intf == NULL) {
		/* No interface on master matching operating chan report credentials, return */
		WBD_ERROR("Error in updating controller chanspec w.r.t operating chan report \n");
		return;
	}

	foreach_i5glist_item(i5_iter_device, i5_dm_device_type, i5_topology->device_list) {
		if ((eacmp(i5_iter_device->DeviceId, al_mac) == 0) ||
			!I5_IS_MULTIAP_AGENT(i5_iter_device->flags)) {
			/* No need to send Channel selection request to
			 * originator of operating channel report message
			 */
			continue;
		}
		foreach_i5glist_item(i5_iter_ifr, i5_dm_interface_type,
			i5_iter_device->interface_list) {

			if ((i5_iter_ifr->chanspec == 0) ||
				!i5DmIsInterfaceWireless(i5_iter_ifr->MediaType) ||
				!i5_iter_ifr->BSSNumberOfEntries) {
				continue;
			}

			if_band = i5_iter_ifr->band;
			if (agent_if_band == if_band) {
				WBD_INFO("channel selection request to"MACF", chan[%d] band[%d]\n",
					ETHERP_TO_MACF(i5_iter_ifr->InterfaceId),
					wf_chspec_ctlchan(i5_iter_ifr->chanspec), if_band);

				/* send channel selection request for this radio mac */
				wbd_master_send_channel_selection_request(info,
					i5_iter_device->DeviceId, i5_iter_ifr->InterfaceId);
			}
		}
	}
end:
	WBD_EXIT();
}

/* Check if channel in regclass is invalid: invalid. 0 = valid, 1 = invalid */
int wbd_master_check_regclass_channel_invalid(uint8 chan,
	uint8 regclass, ieee1905_chan_pref_rc_map_array *cp)
{
	int i, j;
	for (i = 0; i < cp->rc_count; i++) {
		if (regclass != cp->rc_map[i].regclass) {
			continue;
		}

		/* If there is no channels and preference is 0, then its invalid channel */
		if (cp->rc_map[i].count == 0 && cp->rc_map[i].pref == 0) {
			return 1;
		}

		/* search for channel. If the preference is 0 then its invalid channel */
		for (j = 0; j < cp->rc_map[i].count; j++) {
			if (chan == cp->rc_map[i].channel[j]) {
				return ((cp->rc_map[i].pref == 0) ? 1 : 0);
			}
		}
		break;
	}
	return 0;
}

/* Send channel selection request */
/* Algorithm to send channel selection request:
 * Prepare channel prefernce report with:
 * - List of opclass which are not present in agent's radio capability
 * - Update agent's opclass with invalid channels based on controller's
 *   current chanspec and opclass
 * - Prepare list of channels for opclass present in agent's radio capability
 *   but are non preferred at the moment.
 * - Compare prepared channel preference report with controller and agent's
 *   channel preference database.
 *
 * - Final channel preference report should be with the following information:
 *   - List of all valid opclass in global operating class table
 *   - Each operating class object have either zero or non zero list of
 *     non preferred channels
 */
void wbd_master_send_channel_selection_request(wbd_info_t *info, unsigned char *al_mac,
	unsigned char *radio_mac)
{
	int ret = WBDE_OK;
	uint8 rc_first, rc_last;
	uint8 chan_first, chan_last, center_channel, channel = 0;
	ieee1905_chan_pref_rc_map_array chan_pref;
	ieee1905_chan_pref_rc_map *rc_map;
	i5_dm_interface_type *interface, *self_intf = NULL;
	i5_dm_device_type *self_device, *device;
	const i5_dm_rc_chan_map_type *rc_chan_map = i5DmGetRegClassChannelMap();
	int i, j, k;
	ieee1905_radio_caps_type *RadioCaps;
	int rcaps_rc_count;
	int rcidx;
	wbd_cmd_vndr_set_chan_t vndr_set_chan;
	ieee1905_vendor_data vndr_msg;
	int if_band = BAND_INV;
	void *pmsg;
	int dedicated_bh = 0;

	WBD_ENTER();

	memset(&chan_pref, 0, sizeof(chan_pref));
	memset(&vndr_msg, 0, sizeof(vndr_msg));

	if (!al_mac) {
		WBD_ERROR("Invalid Device address\n");
		goto end;
	}

	device = wbd_ds_get_i5_device(al_mac, &ret);
	WBD_ASSERT();

	pmsg = ieee1905_create_channel_selection_request(al_mac);
	if (pmsg == NULL) {
		WBD_ERROR("Channel selection request creation failed\n");
		goto end;
	}

	chan_pref.rc_map = (ieee1905_chan_pref_rc_map *)
		wbd_malloc(sizeof(ieee1905_chan_pref_rc_map) * REGCLASS_MAX_COUNT, NULL);
	if (chan_pref.rc_map == NULL) {
		WBD_ERROR("malloc failed for rc_map\n");
		goto end;
	}

	foreach_i5glist_item(interface, i5_dm_interface_type, device->interface_list) {

		if (!i5DmIsInterfaceWireless(interface->MediaType) ||
				!interface->BSSNumberOfEntries) {
			WBD_INFO("Not a wireless interface "MACF" OR no BSSes\n",
				ETHERP_TO_MACF(interface->InterfaceId));
			continue;
		}
		if (radio_mac && (memcmp(interface->InterfaceId, radio_mac, MAC_ADDR_LEN) != 0)) {
			WBD_INFO("Radio mac specified ["MACF"] current ["MACF"]. Skip it\n",
				ETHERP_TO_MACF(radio_mac), ETHERP_TO_MACF(interface->InterfaceId));
			continue;
		}
		if (!interface->isConfigured) {
			WBD_INFO("Interface ["MACF"] not configured yet. Skip it\n",
				ETHERP_TO_MACF(interface->InterfaceId));
			continue;
		}

		chan_pref.rc_count = 0;
		memset(chan_pref.rc_map, 0, sizeof(ieee1905_chan_pref_rc_map) * REGCLASS_MAX_COUNT);

		RadioCaps = &interface->ApCaps.RadioCaps;
		rcaps_rc_count = RadioCaps->List ? RadioCaps->List[0]: 0;

		/* Get the valid range of channels and regulatory class for the given
		 * band. 160Mhz is handled as special case. So not considered here
		 */
		if_band = ieee1905_get_band_from_radiocaps(RadioCaps);
		WBD_INFO("band from radiocaps: 0x%x\n", if_band);

		if (if_band == BAND_2G) {
			rc_first = REGCLASS_24G_FIRST;
			rc_last = REGCLASS_24G_LAST;
			chan_first = CHANNEL_24G_FIRST;
			chan_last = CHANNEL_24G_LAST;
		} else if (if_band == BAND_5GL) {
			rc_first = REGCLASS_5GL_FIRST;
			rc_last = REGCLASS_5GL_LAST;
			chan_first = CHANNEL_5GL_FIRST;
			chan_last = CHANNEL_5GL_LAST;
		} else if (if_band == BAND_5GH) {
			rc_first = REGCLASS_5GH_FIRST;
			rc_last = REGCLASS_5GH_LAST;
			chan_first = CHANNEL_5GH_FIRST;
			chan_last = CHANNEL_5GH_LAST;
		} else if (if_band == (BAND_5GH | BAND_5GL)) {
			rc_first = REGCLASS_5GL_FIRST;
			rc_last = REGCLASS_5GH_LAST;
			chan_first = CHANNEL_5GL_FIRST;
			chan_last = CHANNEL_5GH_LAST;
		} else {
			WBD_ERROR("Invalid band : %d\n", if_band);
			goto end;
		}
		WBD_INFO("Interface: ["MACF"]Band: [%d], RegClass Range: [%d - %d] "
			"Channel Range: [%d -%d]\n", ETHERP_TO_MACF(interface->InterfaceId),
			if_band, rc_first, rc_last, chan_first, chan_last);

		/* Add all regulatory classes outside current band to the unpreferred list */
		WBD_DEBUG("Regulatory classes outside current band:\n");
		for (i = 0; i < REGCLASS_MAX_COUNT; i++) {
			if (rc_chan_map[i].regclass < rc_first ||
				(rc_chan_map[i].regclass > rc_last &&
				(if_band == BAND_2G ||
				rc_chan_map[i].regclass <= REGCLASS_40MHZ_LAST))) {

				chan_pref.rc_map[chan_pref.rc_count].regclass =
					rc_chan_map[i].regclass;
				chan_pref.rc_count++;
				WBD_DEBUG("[%d]:%d\n", chan_pref.rc_count, rc_chan_map[i].regclass);
			}
		}

		/* If Multi-channel feature is disabled, all agents should operate in
		 * the master channel. So find master channel in the given band
		 */
		if (WBD_MCHAN_ENAB(info->flags)) {
			WBD_INFO("Multi channel enabled\n");
			/* no need to add vendor tlv for multi chan operation */
			goto validate;
		}
		if (blanket_get_config_val_int(NULL, WBD_NVRAM_ENAB_SINGLE_CHAN_BH, 0) == 0) {
			dedicated_bh = wbd_ds_is_interface_dedicated_backhaul(interface);
			if (dedicated_bh) {
				WBD_INFO("Interface ["MACF"] is dedicated Backhaul. Don't force "
					"single channel\n", ETHERP_TO_MACF(interface->InterfaceId));
				goto validate;
			}
		}
		self_device = wbd_ds_get_self_i5_device(&ret);
		WBD_ASSERT();

		for (self_intf = (i5_dm_interface_type *)WBD_I5LL_HEAD(self_device->interface_list);
			self_intf; self_intf = WBD_I5LL_NEXT(self_intf)) {

			if ((self_intf->chanspec == 0) ||
				!i5DmIsInterfaceWireless(self_intf->MediaType)) {
				continue;
			}

			blanket_get_global_rclass(self_intf->chanspec, &self_intf->opClass);
			channel = wf_chspec_ctlchan(self_intf->chanspec);
			center_channel = self_intf->chanspec & WL_CHANSPEC_CHAN_MASK;
			WBD_INFO("ifname: %s, channel: %d rclass: %d chanspec: 0x%x\n",
				self_intf->ifname, channel,
				self_intf->opClass, self_intf->chanspec);
			if (channel >= chan_first && channel <= chan_last)
				break;
		}
		if (!self_intf) {
			WBD_WARNING("No matching master interface for channel range (%d - %d)\n",
				chan_first, chan_last);
			continue;
		}
		WBD_INFO("Master interface [%s] is operating in Regclass [%d] channel[%d]\n",
				self_intf->ifname, self_intf->opClass, channel);

		WBD_DEBUG("Unprefered Regulatory classes OR Unprefered channel(s) "
			"in a valid regclass inside the band:\n");
		for (i = 0; (i < REGCLASS_MAX_COUNT) &&
			(chan_pref.rc_count < REGCLASS_MAX_COUNT); i++) {
			rc_map = &chan_pref.rc_map[chan_pref.rc_count];
			if (rc_chan_map[i].regclass < rc_first ||
				(rc_chan_map[i].regclass > rc_last &&
				(if_band == BAND_2G ||
				rc_chan_map[i].regclass <= REGCLASS_40MHZ_LAST))) {
				continue;
			}

			if (rc_chan_map[i].regclass != self_intf->opClass) {
				rc_map->regclass = rc_chan_map[i].regclass;
				chan_pref.rc_count++;
				WBD_DEBUG("[%d]:%d\n", chan_pref.rc_count, rc_chan_map[i].regclass);
				continue;
			}

			for (j = 0; j < rc_chan_map[i].count; j++) {
				if (rc_chan_map[i].regclass > REGCLASS_40MHZ_LAST) {
					/* use centre channel to compare */
					if (center_channel == rc_chan_map[i].channel[j]) {
						continue;
					}
				} else {
					/* use control channel */
					if (channel == rc_chan_map[i].channel[j]) {
						continue;
					}
				}

				if (rc_map->regclass != rc_chan_map[i].regclass) {
					rc_map->regclass = rc_chan_map[i].regclass;
					chan_pref.rc_count++;
					WBD_DEBUG("[%d]: %d Channels:\n",
						chan_pref.rc_count,
						rc_chan_map[i].regclass);
				}
				rc_map->channel[rc_map->count] =
					rc_chan_map[i].channel[j];
				WBD_DEBUG("%d\n", rc_map->channel[rc_map->count]);
				rc_map->count++;
			}
		}
validate:
		/* Check if at least one valid channel is present in the list,
		 * by comparing it agaist agent's channel preference
		 */
		for (i = 0; i < REGCLASS_MAX_COUNT; i++) {
			int ch_count = 0;
			uint8 rc = 0;
			int invalid = 0;
			uint8 count = 0;
			const uint8 *chan_list = NULL;
			/* Find a regulatory class in Radio capabilities */
			for (j = 0, rcidx = 1; j < rcaps_rc_count && rcidx < RadioCaps->Len; j++) {
				rc = RadioCaps->List[rcidx++];

				rcidx++; /* Max Transmit power */
				ch_count = RadioCaps->List[rcidx++];
				if (rc_chan_map[i].regclass == rc) {
					break;
				} else {
					rcidx += ch_count;
				}
			}
			if (rc_chan_map[i].regclass != rc) {
				WBD_DEBUG("Regulatory Class [%d] not found in Radio Capabilities\n",
					rc_chan_map[i].regclass);
				continue;
			}

			count = rc_chan_map[i].count;
			chan_list = rc_chan_map[i].channel;

			for (j = 0; j < count; j++) {
				int chidx = rcidx;
				invalid = 0;
				for (k = 0; k < ch_count; k++, chidx++) {
					if (RadioCaps->List[chidx] == chan_list[j]) {
						WBD_DEBUG("Channel [%d] is invalid in RadioCaps\n",
							chan_list[j]);
						invalid = 1;
						break;
					}
				}
				if (invalid) {
					WBD_DEBUG("Not valid Channel [%d] in Radio capabilities\n",
						rc_chan_map[i].channel[j]);
					continue;
				}
				/* rc_chan_map[i].channel[j] is valid in Radio capabilities.
				 * check if it is valid in master and local preferences
				 */
				invalid = wbd_master_check_regclass_channel_invalid(chan_list[j],
					rc_chan_map[i].regclass, &chan_pref);
				if (invalid) {
					WBD_DEBUG("regclass[%d] Not valid Channel [%d] "
							"in Master preference\n",
							rc_chan_map[i].regclass, chan_list[j]);
					continue;
				}
				invalid = wbd_master_check_regclass_channel_invalid(chan_list[j],
					rc_chan_map[i].regclass, &interface->ChanPrefs);
				if (invalid == 0) {
					break;
				}
				WBD_DEBUG("Not valid Channel [%d] in Agent preference\n",
					rc_chan_map[i].channel[j]);

			}
			if (invalid) {
				continue;
			}

			WBD_INFO("Regulatory class [%d] channel [%d] is valid at both Master and "
				"Agent\n", rc_chan_map[i].regclass, chan_list[j]);

			ieee1905_insert_channel_selection_request_tlv(pmsg, interface->InterfaceId,
				&chan_pref);

			if (!WBD_MCHAN_ENAB(info->flags) && !dedicated_bh &&
				(self_intf->opClass > REGCLASS_40MHZ_LAST)) {
				/* Prepare vendor message for agent with BW > 40, As MULTI-AP
				 * topology is operating in single channel operation, BRCM agent
				 * use this information only to set the channel and ignore channel
				 * preference report present in channel selection request.
				 * =======
				 * For Multi chan environment, Controller does not send vendor TLV
				 * in channel selection request message.
				 */
				vndr_set_chan.cntrl_chan = channel;
				vndr_set_chan.rclass = self_intf->opClass;
				memcpy(&vndr_set_chan.mac, interface->InterfaceId, ETHER_ADDR_LEN);
				vndr_msg.vendorSpec_msg =
					(unsigned char *)wbd_malloc((sizeof(vndr_set_chan) +
					sizeof(i5_tlv_t)), &ret);
				WBD_ASSERT();

				wbd_tlv_encode_msg(&vndr_set_chan, sizeof(vndr_set_chan),
					WBD_TLV_CHAN_SET_TYPE, vndr_msg.vendorSpec_msg,
					&vndr_msg.vendorSpec_len);

				ieee1905_insert_vendor_message_tlv(pmsg, &vndr_msg);
				free(vndr_msg.vendorSpec_msg);
			}

			break;
		}
	}

	ieee1905_send_message(pmsg);

end:
	if (chan_pref.rc_map) {
		free(chan_pref.rc_map);
	}
	WBD_EXIT();
}

#if defined(MULTIAPR2)
/* Unsuccessful association config policy value fetch from NVRAM "map_unsuccessful_assoc_policy" */
void
wbd_master_fetch_unsuccessful_association_nvram_config(void)
{
	int num = 0;
	int report_flag;
	ieee1905_unsuccessful_assoc_config_t unsuccessful_association;
	char *nvval;

	memset(&unsuccessful_association, 0, sizeof(unsuccessful_association));
	nvval = nvram_safe_get(MAP_NVRAM_UNSUCCESSFUL_ASSOC_POLICY);
	num = sscanf(nvval, "%d %d", &report_flag,
			&unsuccessful_association.max_reporting_rate);
	if (report_flag == 1) {
		unsuccessful_association.report_flag |= MAP_UNSUCCESSFUL_ASSOC_FLAG_REPORT;
	}
	if (num == 2) {
		ieee1905_add_unsuccessful_association_policy(&unsuccessful_association);
	}
	WBD_INFO("NVRAM[%s=%s] REPORT_FLAG[0x%02x] MAX_REPORTING_RATE[%d]\n ",
		MAP_NVRAM_UNSUCCESSFUL_ASSOC_POLICY, nvval, unsuccessful_association.report_flag,
		unsuccessful_association.max_reporting_rate);
}
#endif /* MULTIAPR2 */

/* Add the metric policy for a radio */
void
wbd_master_add_metric_report_policy(wbd_info_t *info,
	unsigned char* radio_mac, int if_band)
{
	int ret = WBDE_OK, chan_util_def = 0;
	wbd_master_info_t *master = NULL;
	ieee1905_ifr_metricrpt metricrpt;
	wbd_weak_sta_policy_t *metric_policy;
	char *nvname;
	WBD_ENTER();

	/* get the master info for the band */
	WBD_SAFE_GET_MASTER_INFO(info, WBD_BKT_ID_BR0, master, &ret);
	if (if_band & WBD_BAND_LAN_2G) {
		metric_policy = &master->metric_policy_2g;
		nvname = WBD_NVRAM_CHAN_UTIL_THLD_2G;
		chan_util_def = WBD_DEF_CHAN_UTIL_THLD_2G;
	} else {
		metric_policy = &master->metric_policy_5g;
		nvname = WBD_NVRAM_CHAN_UTIL_THLD_5G;
		chan_util_def = WBD_DEF_CHAN_UTIL_THLD_5G;
	}

	/* Fill the metric policy for a radio to be added to MAP */
	memset(&metricrpt, 0, sizeof(metricrpt));
	memcpy(metricrpt.mac, radio_mac, sizeof(metricrpt.mac));
	metricrpt.sta_mtrc_rssi_thld = (unsigned char)WBD_RSSI_TO_RCPI(metric_policy->t_rssi);
	metricrpt.sta_mtrc_rssi_hyst = (unsigned char)metric_policy->t_hysterisis;
	metricrpt.ap_mtrc_chan_util =
		(unsigned char)blanket_get_config_val_int(NULL, nvname, chan_util_def);
	/* Include policy to send traffic stats and associated link metrics in the AP metrics
	 * response. Because after the weak client is identified, master will ask for AP metrics
	 * from agent. For which agent will give ap metrics, sta traffic stats and associated
	 * sta link metrics.
	 */
	metricrpt.sta_mtrc_policy_flag |= MAP_STA_MTRC_TRAFFIC_STAT;
	metricrpt.sta_mtrc_policy_flag |= MAP_STA_MTRC_LINK_MTRC;
	ieee1905_add_metric_reporting_policy_for_radio(0, &metricrpt);
#if defined(MULTIAPR2)
	wbd_master_fetch_unsuccessful_association_nvram_config();
#endif /* MULTIAPR2 */
end:
	WBD_EXIT();
}

#if defined(MULTIAPR2)
/* Add the Channel Scan Reporting policy for a radio */
void
wbd_master_add_chscan_report_policy()
{
	uint8 indep_scan = 0;
	ieee1905_chscanrpt_config chscanrpt;
	WBD_ENTER();

	/* Fill the Channel Scan Reporting Policy for a radio to be added to MAP */
	memset(&chscanrpt, 0, sizeof(chscanrpt));

	/* Get NVRAM setting for Channel Scan Reporting Policy TLV's Flags */
	indep_scan = blanket_get_config_val_int(NULL,
		NVRAM_MAP_CHSCAN_INDEP_SCAN, DEF_MAP_CHSCAN_INDEP_SCAN);
	if (indep_scan) {

		/* The Channel Scan Reporting Policy TLV's,  Report Independent Channel Scans
		 * Flag's bit 7 identifies whether a SmartMesh Agent should report the results
		 * of any Independent Channel Scan that it performs to the SmartMesh Controller.
		 * bit 7    1 - Report Independent Channel Scans
		 *          0 - Do not report Independent Channel Scans
		 */
		chscanrpt.chscan_rpt_policy_flag |= MAP_CHSCAN_RPT_POL_INDEP_SCAN;
	}

	ieee1905_add_chscan_reporting_policy_for_radio(&chscanrpt);

	WBD_EXIT();
}
#endif /* MULTIAPR2 */

/* Send Policy Configuration for a radio */
void
wbd_master_send_policy_config(wbd_info_t *info, unsigned char *al_mac,
	unsigned char* radio_mac, int if_band)
{
	WBD_ENTER();

	wbd_master_add_metric_report_policy(info, radio_mac, if_band);
#if defined(MULTIAPR2)
	wbd_master_add_chscan_report_policy();
#endif /* MULTIAPR2 */
	ieee1905_send_policy_config(al_mac);

	WBD_EXIT();
}

/* Get Vendor Specific TLV  for Metric Reporting Policy from Master */
static void
wbd_master_get_vndr_tlv_metric_policy(ieee1905_vendor_data *out_vendor_tlv)
{
	int ret = WBDE_OK, band;
	i5_dm_device_type *dst_device;
	i5_dm_interface_type *iter_ifr;
	wbd_cmd_vndr_metric_policy_config_t policy_config; /* VNDR_METRIC_POLICY Data */
	wbd_metric_policy_ifr_entry_t new_policy_ifr;
	wbd_master_info_t *master = NULL;
	WBD_ENTER();

	WBD_INFO("Get Vendor Specific TLV for Metric Reporting Policy for Device["MACDBG"]\n",
		MAC2STRDBG(out_vendor_tlv->neighbor_al_mac));

	/* Initialize Vendor Metric Policy struct Data */
	memset(&policy_config, 0, sizeof(policy_config));
	wbd_ds_glist_init(&(policy_config.entry_list));

	/* Get the Device from AL_MAC, for which, this Vendor TLV is required, to be filled */
	WBD_SAFE_FIND_I5_DEVICE(dst_device, out_vendor_tlv->neighbor_al_mac, &ret);

	/* Loop for all the Interfaces in this Device */
	foreach_i5glist_item(iter_ifr, i5_dm_interface_type, dst_device->interface_list) {

		wbd_weak_sta_policy_t *metric_policy;

		/* Loop for, only for wireless interfaces with valid chanspec & BSSes */
		if (!i5DmIsInterfaceWireless(iter_ifr->MediaType) ||
			!iter_ifr->BSSNumberOfEntries ||
			!iter_ifr->isConfigured) {
			continue;
		}

		/* Find matching Master for this Interface's Band, to get Policy Config */
		band = WBD_BAND_FROM_1905_BAND(iter_ifr->band);
		WBD_DS_GET_MASTER_INFO(wbd_get_ginfo(), WBD_BKT_ID_BR0, master, (&ret));
		if (ret != WBDE_OK) {
			ret = WBDE_OK;
			continue;
		}
		if (band & WBD_BAND_LAN_2G) {
			metric_policy = &master->metric_policy_2g;
		} else {
			metric_policy = &master->metric_policy_5g;
		}

		WBD_INFO("Adding new entry to VNDR_METRIC_POLICY : BKTID[%d] "
			"BSSNumberOfEntries[%d] RAdio_Configured[%d]\n",
			master->bkt_id, iter_ifr->BSSNumberOfEntries, iter_ifr->isConfigured);

		memset(&new_policy_ifr, 0, sizeof(new_policy_ifr));

		/* Prepare a new Metric Policy item for Vendor Metric Policy List */
		memcpy(new_policy_ifr.ifr_mac, iter_ifr->InterfaceId,
			sizeof(new_policy_ifr.ifr_mac));

		/* Copy Vendor Metric Policy for this Radio, in Vendor Metric Policy List */
		new_policy_ifr.vndr_policy.t_idle_rate = metric_policy->t_idle_rate;
		new_policy_ifr.vndr_policy.t_tx_rate = metric_policy->t_tx_rate;
		new_policy_ifr.vndr_policy.t_tx_failures = metric_policy->t_tx_failures;
		new_policy_ifr.vndr_policy.flags = metric_policy->flags;

		WBD_INFO("Adding new entry to VNDR_METRIC_POLICY for : "
			"Device["MACDBG"] IFR_MAC["MACDBG"] "
			"t_idle_rate[%d] t_tx_rate[%d] t_tx_failures[%d] Flags[0x%X]\n",
			MAC2STRDBG(out_vendor_tlv->neighbor_al_mac),
			MAC2STRDBG(new_policy_ifr.ifr_mac),
			new_policy_ifr.vndr_policy.t_idle_rate,
			new_policy_ifr.vndr_policy.t_tx_rate,
			new_policy_ifr.vndr_policy.t_tx_failures,
			new_policy_ifr.vndr_policy.flags);

		/* Add a Metric Policy item in a Metric Policy List */
		wbd_add_item_in_metric_policylist(&new_policy_ifr, &policy_config, NULL);

		/* Increament Total item count */
		policy_config.num_entries++;
	}

	/* Encode Vendor Specific TLV : Metric Reporting Policy Vendor Data to send */
	wbd_tlv_encode_vndr_metric_policy((void*)&policy_config, out_vendor_tlv->vendorSpec_msg,
		&(out_vendor_tlv->vendorSpec_len));
end:
	/* If Vendor Metric Policy List is filled up */
	if (policy_config.num_entries > 0) {

		/* Remove all Vendor Metric Policy data items */
		wbd_ds_glist_cleanup(&(policy_config.entry_list));
	}
	WBD_EXIT();
}

/* Get Vendor Specific TLV  for Guest SSID */
static void
wbd_master_encode_vndr_tlv_guest_ssid(ieee1905_vendor_data *out_vendor_tlv)
{
	int ret = WBDE_OK;
	dll_t *item_p, *next_p;
	ieee1905_client_bssinfo_type *list;
	unsigned char *pbuf, *pmem;
	i5_tlv_t *ptlv;
	unsigned char count = 0;
	WBD_ENTER();

	WBD_INFO("Get Vendor Specific TLV for Guest SSID\n");

	/* Validate Args */
	WBD_ASSERT_ARG(out_vendor_tlv, WBDE_INV_ARG);
	WBD_ASSERT_ARG(out_vendor_tlv->vendorSpec_msg, WBDE_INV_ARG);

	pmem = out_vendor_tlv->vendorSpec_msg;

	/* bytes skiped for TLV Hdr size, fill the TLV Hdr in the end */
	pbuf = pmem + sizeof(i5_tlv_t);

	/* Update count of SSID at the end */
	pbuf++;

	/* Add all Guest SSIDs */
	for (item_p = dll_head_p(&i5_config.client_bssinfo_list.head);
		!dll_end(&i5_config.client_bssinfo_list.head, item_p);
		item_p = next_p) {

		next_p = dll_next_p(item_p);
		list = (ieee1905_client_bssinfo_type*)item_p;
		if (list->Guest == 1) {
			count++;

			/* insert SSID length */
			*pbuf = list->ssid.SSID_len;
			pbuf++;

			/* insert SSID */
			memcpy(pbuf, list->ssid.SSID, list->ssid.SSID_len);
			pbuf += list->ssid.SSID_len;
		}
	}

	/* Fill the TLV Hdr now */
	ptlv = (i5_tlv_t *)pmem;
	ptlv->type = WBD_TLV_GUEST_SSID_TYPE;
	ptlv->length = htons(pbuf-pmem-sizeof(i5_tlv_t));

	out_vendor_tlv->vendorSpec_len = pbuf - pmem;

	/* Now Update the count of SSID */
	pbuf = pmem + sizeof(i5_tlv_t);
	*pbuf = count;
end:
	WBD_EXIT();
	return;
}

/* Master get Vendor Specific TLV to send for 1905 Message */
void
wbd_master_get_vendor_specific_tlv(i5_msg_types_with_vndr_tlv_t msg_type,
	ieee1905_vendor_data *out_vendor_tlv)
{
	int ret = WBDE_OK;
	WBD_ENTER();

	WBD_ASSERT_ARG(out_vendor_tlv, WBDE_INV_ARG);

	WBD_INFO("Get Vendor Specific TLV requested for Device["MACDBG"]: %s\n",
		MAC2STRDBG(out_vendor_tlv->neighbor_al_mac), wbd_get_1905_msg_str(msg_type));

	switch (msg_type) {

		case i5MsgMultiAPPolicyConfigRequestValue :
			/* Get Vendor TLV for 1905 Message : MultiAP Policy Config Request */
			wbd_master_get_vndr_tlv_metric_policy(out_vendor_tlv);
			break;

		case i5MsgMultiAPGuestSsidValue:
			/* Encode Vendor TLV for 1905 Message : MultiAP Guest SSID */
			wbd_master_encode_vndr_tlv_guest_ssid(out_vendor_tlv);
			break;

		default:
			WBD_WARNING("Invalid option\n");
			break;
	}

end:
	WBD_EXIT();
}

/* Create the timer to remove stale client entry for BSS */
static int
wbd_controller_create_remove_client_timer(wbd_remove_client_arg_t* arg, int timeout)
{
	int ret = WBDE_OK;
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(arg, WBDE_INV_ARG);

	/* Create a timer to send REMOVE_CLIENT_REQ cmd to selected BSS */
	ret = wbd_add_timers(wbd_get_ginfo()->hdl, arg,
		WBD_SEC_MICROSEC(timeout), wbd_controller_remove_client_timer_cb, 0);
	if (ret != WBDE_OK) {
		WBD_WARNING("Interval[%d] Failed to create REMOVE_CLIENT_REQ timer\n",
			timeout);
	}

end:

	WBD_EXIT();
	return ret;
}

/* Remove stale client entry form BSS, if required */
static void
wbd_controller_remove_client_timer_cb(bcm_usched_handle *hdl, void *arg)
{
	int ret = WBDE_OK, bss_count = 0;
	bool recreate = FALSE;
	i5_dm_network_topology_type *i5_topology = NULL;
	i5_dm_device_type *device = NULL;
	i5_dm_interface_type *i5_ifr;
	i5_dm_bss_type *i5_bss;
	i5_dm_clients_type *sta = NULL;
	wbd_assoc_sta_item_t *assoc_sta;
	wbd_cmd_remove_client_t cmd;
	ieee1905_vendor_data vndr_msg_data;
	wbd_remove_client_arg_t* remove_client_arg = NULL;
	struct timeval assoc_time = {0, 0};
	WBD_ENTER();

	memset(&cmd, 0, sizeof(cmd));
	memset(&vndr_msg_data, 0, sizeof(vndr_msg_data));

	/* Validate arg */
	WBD_ASSERT_ARG(arg, WBDE_INV_ARG);

	remove_client_arg = (wbd_remove_client_arg_t*)arg;

	/* Count how many BSSes has the STA entry in Controller Topology.
	 * Also get the assoc_time of STA which is associated to the BSS most recently.
	 */
	bss_count = wbd_ds_find_duplicate_sta_in_controller(&remove_client_arg->sta_mac,
		&assoc_time);

	WBD_INFO("STA["MACF"] BSS["MACF"] bss_count[%d] assoc_time["TIMEVALF"]\n",
		ETHER_TO_MACF(remove_client_arg->sta_mac),
		ETHER_TO_MACF(remove_client_arg->parent_bssid),
		bss_count, TIMEVAL_TO_TIMEVALF(assoc_time));
	/* No need to process further as only single BSS hold the client entry */
	if (bss_count <= 1) {
		goto end;
	}

	/* Copy STA MAC to REMOVE_CLIENT_REQ msg data */
	eacopy(&remove_client_arg->sta_mac, &cmd.stamac);

	/* Copy bssid to REMOVE_CLIENT_REQ msg data */
	eacopy(&remove_client_arg->parent_bssid, &cmd.bssid);

	/* Allocate Dynamic mem for Vendor data from App */
	vndr_msg_data.vendorSpec_msg =
		(unsigned char *)wbd_malloc(IEEE1905_MAX_VNDR_DATA_BUF, &ret);
	WBD_ASSERT();

	/* Encode Vendor Specific TLV for Message : REMOVE_CLIENT_REQ to send */
	wbd_tlv_encode_remove_client_request((void *)&cmd,
		vndr_msg_data.vendorSpec_msg, &vndr_msg_data.vendorSpec_len);

	/* More than one BSS has STA entry in assoclist, so send the REMOVE_CLIENT_REQ msg to
	 * all the BSSes whose assoc time is less than the latest associated BSSes
	 */
	i5_topology = ieee1905_get_datamodel();
	foreach_i5glist_item(device, i5_dm_device_type, i5_topology->device_list) {

		/* Loop only for Agent Devices */
		if (!I5_IS_MULTIAP_AGENT(device->flags)) {
			continue;
		}

		foreach_i5glist_item(i5_ifr, i5_dm_interface_type, device->interface_list) {
			foreach_i5glist_item(i5_bss, i5_dm_bss_type, i5_ifr->bss_list) {
				sta = i5DmFindClientInBSS(i5_bss,
					(unsigned char *)&remove_client_arg->sta_mac);
				if (sta == NULL || sta->vndr_data == NULL) {
					continue;
				}

				/* Get Assoc item pointer */
				assoc_sta = sta->vndr_data;
				WBD_INFO("STA["MACF"] BSS["MACF"] bss_count[%d] "
					"assoc_time["TIMEVALF"] "
					"assoc_sta->assoc_time["TIMEVALF"]\n",
					ETHER_TO_MACF(remove_client_arg->sta_mac),
					ETHER_TO_MACF(remove_client_arg->parent_bssid),
					bss_count, TIMEVAL_TO_TIMEVALF(assoc_time),
					TIMEVAL_TO_TIMEVALF(assoc_sta->assoc_time));

				/* CSTYLED */
				if (timercmp(&assoc_sta->assoc_time, &assoc_time, <)) {

					/* Fill Destination AL_MAC in Vendor data */
					memcpy(vndr_msg_data.neighbor_al_mac,
						device->DeviceId, IEEE1905_MAC_ADDR_LEN);
					WBD_INFO("Send REMOVE_CLIENT_REQ from Device["MACDBG"] "
						"to Device["MACDBG"]\n",
						MAC2STRDBG(ieee1905_get_al_mac()),
						MAC2STRDBG(device->DeviceId));

					/* Send Vendor Specific Message : REMOVE_CLIENT_REQ */
					ret = ieee1905_send_vendor_specific_msg(&vndr_msg_data);

					/* if remove client message send failed. recreate the
					 * timer
					 */
					if (ret != WBDE_OK) {
						recreate = TRUE;
					}
				}
			}
		}
	}

end:
	if (remove_client_arg) {
		if (recreate) {
			/* Stop the timer to create it again with different time */
			wbd_remove_timers(hdl, wbd_controller_remove_client_timer_cb,
				remove_client_arg);

			/* Re-try creating timer to remove stale client entry for BSS */
			wbd_controller_create_remove_client_timer(remove_client_arg,
				wbd_get_ginfo()->max.tm_retry_remove_client);
		} else {
			free(remove_client_arg);
		}
	}
	if (vndr_msg_data.vendorSpec_msg) {
		free(vndr_msg_data.vendorSpec_msg);
	}
	WBD_EXIT();
}

/* Processes the STEER CLI command */
static void
wbd_master_process_steer_cli_cmd(wbd_com_handle *hndl, int childfd, void *cmddata, void *arg)
{
	wbd_cli_send_data_t *clidata = (wbd_cli_send_data_t*)cmddata;
	wbd_info_t *info = (wbd_info_t*)arg;
	char outdata[WBD_MAX_BUF_256] = {0};
	struct ether_addr mac;
	struct ether_addr bssid;
	int ret = WBDE_OK;
	i5_dm_bss_type *i5_bss = NULL, *i5_tbss = NULL;
	i5_dm_interface_type *i5_ifr = NULL;
	i5_dm_device_type *i5_device = NULL;
	i5_dm_clients_type *sta = NULL;
	char logmsg[WBD_MAX_BUF_128] = {0}, timestamp[WBD_MAX_BUF_32] = {0};

	/* Validate Message data */
	WBD_ASSERT_MSGDATA(clidata, "STEER_CLI");

	/* Get & Validate mac */
	WBD_GET_VALID_MAC(clidata->mac, &mac, "STEER_CLI", WBDE_INV_MAC);

	/* Get & Validate tbss's bssid */
	WBD_GET_VALID_MAC(clidata->bssid, &bssid, "STEER_CLI", WBDE_INV_BSSID);

	/* Check if the STA  exists or not */
	sta = wbd_ds_find_sta_in_topology((unsigned char*)&mac, &ret);
	WBD_ASSERT_MSG("STA["MACF"]. %s\n", ETHER_TO_MACF(mac), wbderrorstr(ret));

	i5_bss = (i5_dm_bss_type*)WBD_I5LL_PARENT(sta);
	i5_ifr = (i5_dm_interface_type*)WBD_I5LL_PARENT(i5_bss);
	i5_device = (i5_dm_device_type*)WBD_I5LL_PARENT(i5_ifr);

	i5_tbss = wbd_ds_get_i5_bss_in_topology((unsigned char *)&bssid, &ret);
	WBD_ASSERT_MSG("TBSS["MACF"]. %s\n", ETHER_TO_MACF(mac), wbderrorstr(ret));

	wbd_master_send_steer_req(i5_device->DeviceId, sta->mac, i5_bss->BSSID, i5_tbss);

	/* Create and store steer log. */
	snprintf(logmsg, sizeof(logmsg), CLI_CMD_LOGS_STEER,
		wbd_get_formated_local_time(timestamp, sizeof(timestamp)),
		MAC2STRDBG(sta->mac), MAC2STRDBG(i5_bss->BSSID), MAC2STRDBG(i5_tbss->BSSID));
	wbd_ds_add_logs_in_master(info->wbd_master, logmsg);

end: /* Check Master Pointer before using it below */

	snprintf(outdata, sizeof(outdata), "%s\n", wbderrorstr(ret));

	if (wbd_com_send_cmd(hndl, childfd, outdata, NULL) != WBDE_OK) {
		WBD_WARNING("STEER CLI : %s\n", wbderrorstr(WBDE_SEND_RESP_FL));
	}

	if (clidata)
		free(clidata);

	return;
}

/* -------------------------- Add New Functions above this -------------------------------- */

/* Get the SLAVELIST CLI command data */
static int
wbd_master_process_bsslist_cli_cmd_data(wbd_info_t *info, wbd_cli_send_data_t *clidata,
	char **outdataptr)
{
	int ret = WBDE_OK;
	char *outdata = NULL;
	int count = 0, band_check = 0, bssid_check = 0;
	int outlen = WBD_MAX_BUF_8192, len = 0;
	struct ether_addr bssid;
	i5_dm_network_topology_type *i5_topology;
	i5_dm_device_type *device;
	i5_dm_interface_type *ifr;
	i5_dm_bss_type *bss;

	/* Validate fn args */
	WBD_ASSERT_ARG(info, WBDE_INV_ARG);
	WBD_ASSERT_MSGDATA(clidata, "SLAVELIST_CLI");

	outdata = (char*)wbd_malloc(outlen, &ret);
	WBD_ASSERT();

	/* Check if Band is requested */
	if (WBD_BAND_VALID((clidata->band))) {
		band_check = 1;
	}

	if (strlen(clidata->bssid) > 0) {
		/* Get Validated Non-NULL BSSID */
		WBD_GET_VALID_MAC(clidata->bssid, &bssid, "SLAVELIST_CLI", WBDE_INV_BSSID);
		bssid_check = 1;
	}

	/* Get the info data from i5 topology */
	i5_topology = ieee1905_get_datamodel();
	foreach_i5glist_item(device, i5_dm_device_type, i5_topology->device_list) {
		ret = wbd_snprintf_i5_device(device, CLI_CMD_I5_DEV_FMT, &outdata, &outlen, &len,
			WBD_MAX_BUF_8192);
		foreach_i5glist_item(ifr, i5_dm_interface_type, device->interface_list) {
			/* Skip non-wireless or interfaces with zero bss count */
			if (!i5DmIsInterfaceWireless(ifr->MediaType) ||
				!ifr->BSSNumberOfEntries) {
				continue;
			}
			/* Band specific validation */
			if (band_check && clidata->band == WBD_BAND_FROM_CHANNEL(
				CHSPEC_CHANNEL(ifr->chanspec))) {
				continue;
			}
			foreach_i5glist_item(bss, i5_dm_bss_type, ifr->bss_list) {
				/* bssid check */
				if (bssid_check &&
					memcmp(&bssid, (struct ether_addr*)bss->BSSID,
						MAC_ADDR_LEN)) {
					continue;
				}

				ret = wbd_snprintf_i5_bss(bss, CLI_CMD_I5_BSS_FMT, &outdata,
					&outlen, &len, WBD_MAX_BUF_8192, FALSE, FALSE);
				count++;
			}
		}
	}

	if (!count) {
		snprintf(outdata + strlen(outdata), outlen - strlen(outdata),
			"No entry found \n");
	}

end: /* Check Master Pointer before using it below */

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
wbd_master_process_bsslist_cli_cmd(wbd_com_handle *hndl, int childfd, void *cmddata, void *arg)
{
	int ret = WBDE_OK;
	wbd_cli_send_data_t *clidata = (wbd_cli_send_data_t*)cmddata;
	wbd_info_t *info = (wbd_info_t*)arg;
	char *outdata = NULL;
	BCM_REFERENCE(ret);

	ret = wbd_master_process_bsslist_cli_cmd_data(info, clidata, &outdata);

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
wbd_master_process_info_cli_cmd_data(wbd_info_t *info, wbd_cli_send_data_t *clidata,
	char **outdataptr)
{
	int ret = WBDE_OK, len = 0, outlen = WBD_MAX_BUF_8192;
	char *outdata = NULL;
	int count = 0, band_check = 0, bssid_check = 0;
	struct ether_addr bssid;
	i5_dm_network_topology_type *i5_topology;
	i5_dm_device_type *device;
	i5_dm_interface_type *ifr;
	i5_dm_bss_type *bss;

	/* Validate fn args */
	WBD_ASSERT_ARG(info, WBDE_INV_ARG);
	WBD_ASSERT_MSGDATA(clidata, "INFO_CLI");

	outdata = (char*)wbd_malloc(outlen, &ret);
	WBD_ASSERT();

	/* Check if Band is requested */
	if (WBD_BAND_VALID((clidata->band))) {
		band_check = 1;
	}

	if (strlen(clidata->bssid) > 0) {
		/* Get Validated Non-NULL BSSID */
		WBD_GET_VALID_MAC(clidata->bssid, &bssid, "INFO_CLI", WBDE_INV_BSSID);
		bssid_check = 1;
	}

	/* Get the info data from i5 topology */
	i5_topology = ieee1905_get_datamodel();
	foreach_i5glist_item(device, i5_dm_device_type, i5_topology->device_list) {
		ret = wbd_snprintf_i5_device(device, CLI_CMD_I5_DEV_FMT, &outdata, &outlen, &len,
			WBD_MAX_BUF_8192);
		foreach_i5glist_item(ifr, i5_dm_interface_type, device->interface_list) {
			/* Skip non-wireless or interfaces with zero bss count */
			if (!i5DmIsInterfaceWireless(ifr->MediaType) ||
				!ifr->BSSNumberOfEntries) {
				continue;
			}
			/* Band specific validation */
			if (band_check && clidata->band == WBD_BAND_FROM_CHANNEL(
				CHSPEC_CHANNEL(ifr->chanspec))) {
				continue;
			}
			foreach_i5glist_item(bss, i5_dm_bss_type, ifr->bss_list) {
				/* bssid check */
				if (bssid_check &&
					memcmp(&bssid, (struct ether_addr*)bss->BSSID,
						MAC_ADDR_LEN)) {
					continue;
				}
				ret = wbd_snprintf_i5_bss(bss, CLI_CMD_I5_BSS_FMT, &outdata,
					&outlen, &len, WBD_MAX_BUF_8192, TRUE, TRUE);
				count++;
			}
		}
	}

	if (!count) {
		snprintf(outdata + strlen(outdata), outlen - strlen(outdata),
			"No entry found \n");
	}

end: /* Check Master Pointer before using it below */

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
wbd_master_process_info_cli_cmd(wbd_com_handle *hndl, int childfd, void *cmddata, void *arg)
{
	wbd_cli_send_data_t *clidata = (wbd_cli_send_data_t*)cmddata;
	wbd_info_t *info = (wbd_info_t*)arg;
	char *outdata = NULL;
	int ret = WBDE_OK;

	BCM_REFERENCE(ret);

	if (clidata->flags & WBD_CLI_FLAGS_JSON) {
		outdata = wbd_json_create_cli_info(info, clidata);

	} else {
		ret = wbd_master_process_info_cli_cmd_data(info, clidata, &outdata);
	}

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
wbd_master_process_clientlist_cli_cmd_data(wbd_info_t *info, wbd_cli_send_data_t *clidata,
	char **outdataptr)
{
	int ret = WBDE_OK, len = 0, outlen = WBD_MAX_BUF_8192;
	char *outdata = NULL;
	int count = 0, band_check = 0, bssid_check = 0;
	struct ether_addr bssid;
	i5_dm_network_topology_type *i5_topology;
	i5_dm_device_type *device;
	i5_dm_interface_type *ifr;
	i5_dm_bss_type *bss;

	/* Validate fn args */
	WBD_ASSERT_ARG(info, WBDE_INV_ARG);
	WBD_ASSERT_ARG(outdataptr, WBDE_INV_ARG);
	WBD_ASSERT_MSGDATA(clidata, "CLIENTLIST_CLI");

	outdata = (char*)wbd_malloc(outlen, &ret);
	WBD_ASSERT();

	/* Check if Band is requested */
	if (WBD_BAND_VALID((clidata->band))) {
		band_check = 1;
	}

	if (strlen(clidata->bssid) > 0) {
		/* Get Validated Non-NULL BSSID */
		WBD_GET_VALID_MAC(clidata->bssid, &bssid, "CLIENTLIST_CLI", WBDE_INV_BSSID);
		bssid_check = 1;
	}

	/* Get the info data from i5 topology */
	i5_topology = ieee1905_get_datamodel();
	foreach_i5glist_item(device, i5_dm_device_type, i5_topology->device_list) {
		ret = wbd_snprintf_i5_device(device, CLI_CMD_I5_DEV_FMT, &outdata, &outlen, &len,
			WBD_MAX_BUF_8192);
		foreach_i5glist_item(ifr, i5_dm_interface_type, device->interface_list) {
			/* Skip non-wireless or interfaces with zero bss count */
			if (!i5DmIsInterfaceWireless(ifr->MediaType) ||
				!ifr->BSSNumberOfEntries) {
				continue;
			}
			/* Band specific validation */
			if (band_check && clidata->band == WBD_BAND_FROM_CHANNEL(
				CHSPEC_CHANNEL(ifr->chanspec))) {
				continue;
			}
			foreach_i5glist_item(bss, i5_dm_bss_type, ifr->bss_list) {
				/* bssid check */
				if (bssid_check &&
					memcmp(clidata->bssid, bss->BSSID, MAC_ADDR_LEN)) {
					continue;
				}
				ret = wbd_snprintf_i5_clients(bss, &outdata, &outlen, &len,
					WBD_MAX_BUF_8192, TRUE);
				count++;
			}
		}
	}

	if (!count) {
		snprintf(outdata + strlen(outdata), outlen - strlen(outdata),
			"No entry found \n");
	}

end: /* Check Master Pointer before using it below */

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
wbd_master_process_clientlist_cli_cmd(wbd_com_handle *hndl, int childfd, void *cmddata, void *arg)
{
	wbd_cli_send_data_t *clidata = (wbd_cli_send_data_t*)cmddata;
	wbd_info_t *info = (wbd_info_t*)arg;
	char *outdata = NULL;
	int ret;

	BCM_REFERENCE(ret);
	ret = wbd_master_process_clientlist_cli_cmd_data(info, clidata, &outdata);

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

/* Get the LOGS CLI command data */
static int
wbd_master_process_logs_cli_cmd_data(wbd_info_t *info,
	wbd_cli_send_data_t *clidata, char **outdataptr)
{
	int ret = WBDE_OK;
	char *outdata = NULL;
	int outlen = WBD_MAX_BUF_8192 * 4;

	outdata = (char*)wbd_malloc(outlen, &ret);
	WBD_ASSERT();

	/* Validate arg */
	WBD_ASSERT_ARG(info, WBDE_INV_ARG);

	/* Validate Message data */
	WBD_ASSERT_MSGDATA(clidata, "LOGS_CLI");

	/* Fetch logs from master. */
	(void)wbd_ds_get_logs_from_master(info->wbd_master, outdata, outlen);

end:
	if (ret != WBDE_OK && outdata) {
		snprintf(outdata + strlen(outdata), outlen - strlen(outdata),
			"%s\n", wbderrorstr(ret));
	}
	if (outdataptr) {
		*outdataptr = outdata;
	}
	return ret;
}

static char*
wbd_master_clear_cli_logs(wbd_info_t *info, wbd_cli_send_data_t *clidata)
{
	int ret = WBDE_OK;
	char *outdata = NULL;
	int outlen = WBD_MAX_BUF_64;

	outdata = (char*)wbd_malloc(outlen, &ret);
	WBD_ASSERT();

	/* Validate arg */
	WBD_ASSERT_ARG(info, WBDE_INV_ARG);

	/* Validate Message data */
	WBD_ASSERT_MSGDATA(clidata, "LOGS_CLI");

	/* clear log messages. */
	memset(&(info->wbd_master->master_logs), 0,
		sizeof(info->wbd_master->master_logs));
	info->wbd_master->master_logs.buflen = ARRAYSIZE(info->wbd_master->master_logs.logmsgs);

end:
	if (outdata) {
		snprintf(outdata, outlen, "%s\n", wbderrorstr(ret));
	}

	return outdata;
}

/* Processes the LOGS CLI command */
static void
wbd_master_process_logs_cli_cmd(wbd_com_handle *hndl, int childfd, void *cmddata, void *arg)
{
	wbd_cli_send_data_t *clidata = (wbd_cli_send_data_t*)cmddata;
	wbd_info_t *info = (wbd_info_t*)arg;
	char *outdata = NULL;
	int ret;

	BCM_REFERENCE(ret);

	if (clidata->flags & WBD_CLI_CLEAR_LOGS) {
		outdata = wbd_master_clear_cli_logs(info, clidata);
	} else if (clidata->flags & WBD_CLI_FLAGS_JSON) {
		outdata = wbd_json_create_cli_logs(info, clidata);
	} else {
		ret = wbd_master_process_logs_cli_cmd_data(info, clidata, &outdata);
	}

	if (outdata) {
		if (wbd_com_send_cmd(hndl, childfd, outdata, NULL) != WBDE_OK) {
			WBD_WARNING("LOGS CLI : %s\n", wbderrorstr(WBDE_SEND_RESP_FL));
		}
		free(outdata);
	}

	if (clidata)
		free(clidata);

	return;
}

/* Processes the BH OPT command */
static void
wbd_master_process_bh_opt_cli_cmd(wbd_com_handle *hndl, int childfd, void *cmddata, void *arg)
{
	wbd_cli_send_data_t *clidata = (wbd_cli_send_data_t*)cmddata;
	wbd_info_t *info = (wbd_info_t*)arg;
	int ret = WBDE_OK;
	char *outdata = NULL;
	int outlen = WBD_MAX_BUF_64;

	outdata = (char*)wbd_malloc(outlen, &ret);
	WBD_ASSERT();

	/* Validate arg */
	WBD_ASSERT_ARG(info, WBDE_INV_ARG);

	/* Validate Message data */
	WBD_ASSERT_MSGDATA(clidata, "BH_OPT_CLI");

	if (clidata->disable == 1) {
		info->wbd_master->flags &= ~WBD_FLAGS_BKT_BH_OPT_ENABLED;
		WBD_INFO("Disabling backhaul optimizaion\n");
	} else {
		info->wbd_master->flags |= WBD_FLAGS_BKT_BH_OPT_ENABLED;
		WBD_INFO("Enabling backhaul optimizaion\n");
	}

end:
	if (outdata) {
		snprintf(outdata, outlen, "%s. %s\n", wbderrorstr(ret),
			(WBD_BKT_BH_OPT_ENAB(info->wbd_master->flags) ? "Enabled" : "Disabled"));
		if (wbd_com_send_cmd(hndl, childfd, outdata, NULL) != WBDE_OK) {
			WBD_WARNING("BH_OPT CLI : %s\n", wbderrorstr(WBDE_SEND_RESP_FL));
		}
		free(outdata);
	}
	if (clidata) {
		free(clidata);
	}

	return;
}

/* Gets Master NVRAM settings based on primary prefix */
static int
wbd_master_retrieve_prefix_nvram_config(wbd_master_info_t *master, uint8 band)
{
	int ret = WBDE_OK, num = 0;
	wbd_weak_sta_policy_t wbd_metric;
	wbd_weak_sta_policy_t *metric_policy;
	char *nvval, nvname[WBD_MAX_NVRAM_NAME];
	char logmsg[WBD_MAX_BUF_128] = {0}, timestamp[WBD_MAX_BUF_32] = {0};
	static int map_init_start = 0;
	WBD_ENTER();

	if (band == WBD_BAND_LAN_2G) {
		metric_policy = &master->metric_policy_2g;
		WBDSTRNCPY(nvname, WBD_NVRAM_BKT_WEAK_STA_CFG_2G, sizeof(nvname));
	} else {
		metric_policy = &master->metric_policy_5g;
		WBDSTRNCPY(nvname, WBD_NVRAM_BKT_WEAK_STA_CFG_5G, sizeof(nvname));
	}
	WBD_DEBUG("Retrieve Config for BKTID[%d] nvname[%s]\n",
		master->bkt_id, nvname);

	metric_policy->t_idle_rate = WBD_STA_METRICS_REPORTING_IDLE_RATE_THLD;
	metric_policy->t_rssi = WBD_STA_METRICS_REPORTING_RSSI_THLD;
	metric_policy->t_hysterisis = WBD_STA_METRICS_REPORTING_RSSI_HYSTERISIS_MARGIN;
	metric_policy->t_tx_rate = WBD_STA_METRICS_REPORTING_TX_RATE_THLD;
	metric_policy->t_tx_failures = WBD_STA_METRICS_REPORTING_TX_FAIL_THLD;
	metric_policy->flags = WBD_WEAK_STA_POLICY_FLAG_RSSI;

	memset(&wbd_metric, 0, sizeof(wbd_metric));
	nvval = blanket_nvram_safe_get(nvname);
	num = sscanf(nvval, "%d %d %d %d %d %x", &wbd_metric.t_idle_rate, &wbd_metric.t_rssi,
		&wbd_metric.t_hysterisis, &wbd_metric.t_tx_rate,
		&wbd_metric.t_tx_failures, &wbd_metric.flags);
	if (num == 6) {
		memcpy(metric_policy, &wbd_metric, sizeof(*metric_policy));
	}

	if ((num != 6) && (map_init_start == 0)) {

		/* Create and store MAP Init Start log */
		snprintf(logmsg, sizeof(logmsg), CLI_CMD_LOGS_MAP_INIT_START,
			wbd_get_formated_local_time(timestamp, sizeof(timestamp)),
			MAC2STRDBG(ieee1905_get_al_mac()));
		wbd_ds_add_logs_in_master(wbd_get_ginfo()->wbd_master, logmsg);

#ifdef BCM_APPEVENTD
		/* Send MAP Init Start event to appeventd. */
		wbd_appeventd_map_init(APP_E_WBD_MASTER_MAP_INIT_START,
			(struct ether_addr*)ieee1905_get_al_mac(), MAP_INIT_START, MAP_APPTYPE_MASTER);
#endif /* BCM_APPEVENTD */
		map_init_start++;
	}

	WBD_INFO("BKTID[%d] NVRAM[%s=%s] IDLE_RATE[%d] RSSI[%d] HYSTERISIS[%d] "
		"PHY_RATE[%d] TX_FAILURES[%d] FLAGS[0x%X]\n",
		master->bkt_id, nvname, nvval,
		metric_policy->t_idle_rate, metric_policy->t_rssi,
		metric_policy->t_hysterisis, metric_policy->t_tx_rate,
		metric_policy->t_tx_failures, metric_policy->flags);

	WBD_EXIT();
	return ret;
}

/* Creates the blanket master for the blanket ID */
void
wbd_master_create_master_info(wbd_info_t *info, uint8 bkt_id, char *bkt_name)
{
	int ret = WBDE_OK;
	wbd_master_info_t *master_info = NULL;
	WBD_ENTER();

	master_info = wbd_ds_find_master_in_blanket_master(info->wbd_master, bkt_id, (&ret));

	/* If Master Info NOT FOUND for Band */
	if (!master_info) {

		/* Allocate & Initialize Master for this blanket ID, add to blanket List */
		ret = wbd_ds_create_master_for_blanket_id(info->wbd_master,
			bkt_id, bkt_name, &master_info);
		WBD_ASSERT();

		/* Do Initialize sequences for newly created Master for specific band */
		ret = wbd_master_init_actions(bkt_id, master_info);
		WBD_ASSERT();

		/* Read Master Band specific NVRAMs */
		wbd_master_retrieve_prefix_nvram_config(master_info, WBD_BAND_LAN_2G);
		wbd_master_retrieve_prefix_nvram_config(master_info, WBD_BAND_LAN_5GL);

		/* Read backhual NVRAM config */
		wbd_retrieve_backhaul_nvram_config(&master_info->metric_policy_bh);
	}

end:
	WBD_EXIT();
}

/* Got Associated STA link metric response */
void
wbd_master_process_assoc_sta_metric_resp(wbd_info_t *info, unsigned char *al_mac,
	unsigned char *bssid, unsigned char *sta_mac, ieee1905_sta_link_metric *metric)
{
	int ret = WBDE_OK, band, sta_is_weak = 0;
	i5_dm_interface_type *i5_ifr;
	i5_dm_bss_type *i5_bss;
	i5_dm_clients_type *i5_assoc_sta;
	wbd_master_info_t *master;
	wbd_assoc_sta_item_t *assoc_sta;
	wbd_weak_sta_metrics_t sta_stats;
	wbd_weak_sta_policy_t *metric_policy;
	time_t gap, now = time(NULL);
	WBD_ENTER();

	WBD_SAFE_FIND_I5_BSS_IN_DEVICE(al_mac, bssid, i5_bss, &ret);

	/* Check if the STA exists or not */
	i5_assoc_sta = wbd_ds_find_sta_in_bss_assoclist(i5_bss, (struct ether_addr*)sta_mac, &ret,
		&assoc_sta);
	WBD_ASSERT_MSG("Device["MACDBG"] BSSID["MACDBG"] STA["MACF"]. %s\n", MAC2STRDBG(al_mac),
		MAC2STRDBG(i5_bss->BSSID), ETHERP_TO_MACF(sta_mac), wbderrorstr(ret));

	i5_ifr = (i5_dm_interface_type*)WBD_I5LL_PARENT(i5_bss);
	WBD_SAFE_GET_MASTER_INFO(info, WBD_BKT_ID_BR0, master, (&ret));

	if (I5_IS_BSS_GUEST(i5_bss->mapFlags)) {
		WBD_INFO("Device["MACDBG"] BSSID["MACDBG"] STA["MACF"]. "
			"Sta mtric report not supported on Guest Network\n",
			MAC2STRDBG(al_mac), MAC2STRDBG(i5_bss->BSSID), ETHERP_TO_MACF(sta_mac));
		goto end;
	}

	/* If backhaul STA, use backhaul metric policy */
	if (I5_CLIENT_IS_BSTA(i5_assoc_sta)) {
		metric_policy = &master->metric_policy_bh;
	} else {
		band = WBD_BAND_FROM_1905_BAND(i5_ifr->band);
		if (band & WBD_BAND_LAN_2G) {
			metric_policy = &master->metric_policy_2g;
		} else {
			metric_policy = &master->metric_policy_5g;
		}
	}

	/* Fill STA Stats, which needs to be compared for Weak Client Indentification */
	memset(&sta_stats, 0, sizeof(sta_stats));
	if (metric) {
		sta_stats.rssi = WBD_RCPI_TO_RSSI(metric->rcpi);
		sta_stats.tx_rate = metric->downlink_rate;
		assoc_sta->stats.sta_tm = time(NULL);
	}
	sta_stats.idle_rate = assoc_sta->stats.idle_rate;
	/* Ignore tx failure readings older than 10 seconds */
	gap = now - assoc_sta->stats.active;
	if (assoc_sta->stats.active > 0 && gap < WBD_METRIC_EXPIRY_TIME) {
		if (assoc_sta->stats.tx_tot_failures >= assoc_sta->stats.old_tx_tot_failures) {
			sta_stats.tx_failures = assoc_sta->stats.tx_tot_failures -
				assoc_sta->stats.old_tx_tot_failures;
		} else {
			/* Handle counter roll over */
			sta_stats.tx_failures = (-1) - assoc_sta->stats.old_tx_tot_failures +
				assoc_sta->stats.tx_tot_failures + 1;
		}
	} else {
		sta_stats.tx_failures = 0;
	}
	assoc_sta->stats.active = now;

	sta_stats.last_weak_rssi = assoc_sta->last_weak_rssi ?
		assoc_sta->last_weak_rssi : sta_stats.rssi;

	/* Common algo to compare STA Stats and Thresholds, & identify if STA is Weak or not */
	sta_is_weak = wbd_weak_sta_identification((struct ether_addr*)sta_mac, &sta_stats,
		metric_policy, NULL, NULL);

	WBD_INFO("For %sSTA["MACDBG"] idle_rate[%d] t_idle_rate[%d] rssi[%d] t_rss[%d] "
		"tx_rate[%d] t_tx_rate[%d] tx_failures[%d] t_tx_failures[%d] "
		"sta_status[%d] is_sta_weak[%s]\n",
		I5_CLIENT_IS_BSTA(i5_assoc_sta) ? "BH" : "", MAC2STRDBG(sta_mac),
		sta_stats.idle_rate, metric_policy->t_idle_rate,
		sta_stats.rssi, metric_policy->t_rssi,
		sta_stats.tx_rate, metric_policy->t_tx_rate,
		sta_stats.tx_failures, metric_policy->t_tx_failures,
		assoc_sta->status, sta_is_weak ? "YES" : "NO");

	/* If the associated link metrics is for backhaul STA, and the backhaul optimization is
	 * running, no need to do anything
	 */
	if (I5_CLIENT_IS_BSTA(i5_assoc_sta) && WBD_MINFO_IS_BH_OPT_RUNNING(master->flags)) {
		WBD_DEBUG("Backhaul optimization is running. So, do not process assoc STA metrics "
			"response\n");
		goto end;
	}

	/* If it is weak STA or the backhaul optimization is running */
	if (sta_is_weak) {
		/* If not already weak process the weak client data */
		if (assoc_sta->status != WBD_STA_STATUS_WEAK) {
			ret = wbd_master_process_weak_client_cmd(master, i5_assoc_sta, &sta_stats,
				metric_policy);
			/* If steering a backhaul STA forms a loop, try to do backhaul
			 * optimization before handling weak backhaul STA. This can be possible
			 * only for max_bh_opt_try_on_weak times
			 */
			if (ret == WBDE_BH_STEER_LOOP &&
				(info->wbd_master->max_bh_opt_try_on_weak >
				assoc_sta->bh_opt_count_on_weak)) {
				/* Unset the done flag so that, backhaul optimization
				 * will start for all the backhaul STAs
				 */
				wbd_ds_unset_bh_opt_flags(WBD_FLAGS_ASSOC_ITEM_BH_OPT_DONE);
				wbd_master_start_bh_opt(NULL);
				assoc_sta->bh_opt_count_on_weak++;
			}
		}

		/* If the TBSS timer is not created for this STA. create it */
		if (ret == WBDE_OK && !WBD_ASSOC_ITEM_IS_TBSS_TIMER(assoc_sta->flags)) {
			wbd_master_create_identify_target_bss_timer(master, i5_assoc_sta);
		}
	} else {
		/* If the status is not ignore */
		if (assoc_sta->status != WBD_STA_STATUS_IGNORE) {
			wbd_master_process_weak_cancel_cmd(master, i5_assoc_sta);
		}
	}

end:
	WBD_EXIT();
}

/* Add to monitor list of a BSS if the STA is weak */
static wbd_monitor_sta_item_t*
wbd_master_add_to_monitor_list(i5_dm_bss_type *i5_bss, unsigned char *sta_mac)
{
	int ret = WBDE_OK;
	i5_dm_bss_type *i5_assoc_bss;
	i5_dm_clients_type *i5_sta;
	wbd_assoc_sta_item_t *assoc_sta;
	wbd_monitor_sta_item_t *monitor_sta = NULL;
	uint8 map_flags = IEEE1905_MAP_FLAG_FRONTHAUL;

	/* Find the STA in the topology and check is its weak or not */
	i5_sta = wbd_ds_find_sta_in_topology(sta_mac, &ret);
	if (!i5_sta) {
		goto end;
	}

	assoc_sta = (wbd_assoc_sta_item_t*)i5_sta->vndr_data;
	/* Add to monitor list only if its weak or backhaul managed */
	if (assoc_sta && (assoc_sta->status == WBD_STA_STATUS_WEAK ||
		WBD_ASSOC_ITEM_IS_BH_OPT(assoc_sta->flags))) {
		if (I5_CLIENT_IS_BSTA(i5_sta)) {
			map_flags = IEEE1905_MAP_FLAG_BACKHAUL;
		}

		/* Only use matching BSS(Fronthaul or Backhaul) */
		if (!(map_flags & i5_bss->mapFlags)) {
			goto end;
		}

		i5_assoc_bss = (i5_dm_bss_type*)WBD_I5LL_PARENT(i5_sta);

		/* Don't add if the SSIDs are not matching */
		if ((i5_bss->ssid.SSID_len != i5_assoc_bss->ssid.SSID_len) ||
			memcmp((char*)i5_bss->ssid.SSID, (char*)i5_assoc_bss->ssid.SSID,
			i5_bss->ssid.SSID_len) != 0) {
			goto end;
		}

		ret = wbd_ds_add_sta_in_bss_monitorlist(i5_bss,
			(struct ether_addr*)i5_assoc_bss->BSSID, (struct ether_addr*)i5_sta->mac,
			&monitor_sta);
		if (ret != WBDE_OK) {
			WBD_WARNING("BSS["MACDBG"] STA["MACDBG"] Failed to Add STA to "
				"monitorlist Error : %s\n",
				MAC2STRDBG(i5_bss->BSSID), MAC2STRDBG(i5_sta->mac),
				wbderrorstr(ret));
			goto end;
		}
	} else {
		WBD_WARNING("BSS["MACDBG"] STA["MACDBG"] Is not weak or not backhaul managed. "
			"So Don't add\n",
			MAC2STRDBG(i5_bss->BSSID), MAC2STRDBG(i5_sta->mac));
	}

end:
	return monitor_sta;
}

/* Got UnAssociated STA link metric response */
void
wbd_master_unassoc_sta_metric_resp(unsigned char *al_mac, ieee1905_unassoc_sta_link_metric *metric)
{
	int ret = WBDE_OK, rssi, band;
	i5_dm_device_type *i5_device;
	i5_dm_bss_type *i5_bss;
	ieee1905_unassoc_sta_link_metric_list *sta_item;
	dll_t *unassoc_sta_p;
	wbd_monitor_sta_item_t *monitor_sta = NULL;
	WBD_ENTER();

	WBD_SAFE_FIND_I5_DEVICE(i5_device, al_mac, &ret);

	/* Traverse Unassociated STA link metrics list */
	foreach_glist_item(unassoc_sta_p, metric->sta_list) {

		uint8 map_flags = IEEE1905_MAP_FLAG_FRONTHAUL;
		i5_dm_clients_type *i5_assoc_sta;
		sta_item = (ieee1905_unassoc_sta_link_metric_list*)unassoc_sta_p;

		/* Find the STA in the topology */
		i5_assoc_sta = wbd_ds_find_sta_in_topology(sta_item->mac, &ret);
		if (!i5_assoc_sta) {
			WBD_INFO("Device["MACDBG"] STA["MACDBG"] : %s\n",
				MAC2STRDBG(al_mac), MAC2STRDBG(sta_item->mac), wbderrorstr(ret));
			continue;
		}

		if (I5_CLIENT_IS_BSTA(i5_assoc_sta)) {
			map_flags = IEEE1905_MAP_FLAG_BACKHAUL;
		}

		/* Get the BSS based on band to get the monitor list */
		band = WBD_BAND_FROM_CHANNEL(sta_item->channel);
		WBD_DS_FIND_I5_BSS_IN_DEVICE_FOR_BAND_AND_MAPFLAG(i5_device, band, i5_bss,
			map_flags, &ret);
		if ((ret != WBDE_OK) || !i5_bss) {
			WBD_INFO("Device["MACDBG"] Band[%d]  STA["MACDBG"] : %s\n",
				MAC2STRDBG(al_mac), band, MAC2STRDBG(sta_item->mac),
				wbderrorstr(ret));
			continue;
		}

		/* Check in Monitor list */
		monitor_sta = wbd_ds_find_sta_in_bss_monitorlist(i5_bss,
			(struct ether_addr*)&sta_item->mac, &ret);
		/* If monitor STA item not exists add it */
		if (!monitor_sta) {
			WBD_WARNING("STA["MACDBG"] not found in BSS["MACDBG"]'s Monitor list. "
				"So Add it\n",
				MAC2STRDBG(sta_item->mac),
				MAC2STRDBG(i5_bss->BSSID));
			monitor_sta = wbd_master_add_to_monitor_list(i5_bss, sta_item->mac);
		}
		if (monitor_sta) {
			rssi = WBD_RCPI_TO_RSSI(sta_item->rcpi);
			monitor_sta->rssi = rssi;
			monitor_sta->channel = sta_item->channel;
			monitor_sta->regclass = metric->opClass;
			monitor_sta->monitor_tm = time(NULL);
			WBD_INFO("Device["MACDBG"] BSS["MACDBG"] STA["MACDBG"] OPClass[%x] "
				"Channel[%d] RSSI[%d]\n",
				MAC2STRDBG(al_mac), MAC2STRDBG(i5_bss->BSSID),
				MAC2STRDBG(sta_item->mac), metric->opClass,
				sta_item->channel, rssi);
		}
	}
end:
	WBD_EXIT();
}

/* This function returns the value it receivied is RSSI or RCPI. If it can't
 * predict it returns UNKNOWN. The logic is explained assuming rssi_max is
 * -10dBm and rssi_min is -95dBm
 *  rssi	2's compliment	rcpi
 *  --------------------------------
 *  -10dBm	246		200
 *  -95dBm	161		 30
 *  ------------------------------
 *  From the above table, overlap is between 161 - 200 (2's compliment of
 *  rssi_min and RCPI of rssi_max)
 *  Also, RCPI will never be an odd number, if rssi is not fraction. So odd
 *  values between 161 and 200 can also be safely assumed as RSSI
 */
static int
wbd_master_check_rssi_or_rcpi(int value)
{
	wbd_info_t *info = wbd_get_ginfo();
	uint8 overlap_min = (uint8)info->rssi_min;
	uint8 overlap_max = (uint8)WBD_RSSI_TO_RCPI(info->rssi_max);

	WBD_INFO("RSSI Range [%d - %d] OVERLAP Range [%d - %d] value: %d\n",
		info->rssi_max, info->rssi_min, overlap_min, overlap_max, value);
	if (value < overlap_min) {
		return WBD_VALUE_TYPE_RCPI;
	} else if ((value > overlap_max) || (value & 1)) {
		return WBD_VALUE_TYPE_RSSI;
	}
	return WBD_VALUE_TYPE_UNKNOWN;
}

/* Converts RCPI to RSSI conversion smartly. It considers RCPI received as RCPI
 * only if falls within the safe RCPI range. Otherwise it assumes the value as
 * RSSI itself, and just changes its sign. This is required beacuse most the
 * STAs in market send RSSI itself as RCPI
 */
static int8
wbd_master_conv_rcpi_to_rssi(uint8 *rssi_or_rcpi, uint8 rcpi)
{
	if (WBD_SMART_RCPI_CONV_ENAB((wbd_get_ginfo()->flags))) {
		if (*rssi_or_rcpi == WBD_VALUE_TYPE_UNKNOWN) {
			*rssi_or_rcpi = wbd_master_check_rssi_or_rcpi(rcpi);
		}
		if (*rssi_or_rcpi == WBD_VALUE_TYPE_RCPI) {
			return WBD_RCPI_TO_RSSI(rcpi);
		}
	}
	return (int8)rcpi;
}

/* Update beacon RSSI to monitor STA item */
static void
wbd_master_update_beacon_rssi(wbd_beacon_reports_t  *wbd_bcn_rpt, dot11_rmrep_bcn_t *rmrep_bcn,
	wbd_monitor_sta_item_t *monitor_sta)
{
	wbd_sta_bounce_detect_t *entry;
	if (!monitor_sta) {
		return;
	}

	/* Use bouncing table to store if the beacon report received has RSSI or RCPI */
	entry = wbd_ds_find_sta_in_bouncing_table(wbd_get_ginfo()->wbd_master,
			&monitor_sta->sta_mac);
	if (!entry) {
		WBD_WARNING("Warning: bounce table has no STA["MACF"]\n",
			ETHER_TO_MACF(monitor_sta->sta_mac));
		return;
	}

	monitor_sta->bcn_rpt_rssi = wbd_master_conv_rcpi_to_rssi(
		&entry->rssi_or_rcpi, rmrep_bcn->rcpi);
	WBD_INFO("Monitor STA["MACF"] RCPI recieved: [%d] converted RSSI: [%d] rssi_or_rcpi: %s\n",
		ETHER_TO_MACF(monitor_sta->sta_mac), rmrep_bcn->rcpi, monitor_sta->bcn_rpt_rssi,
		(entry->rssi_or_rcpi == WBD_VALUE_TYPE_RCPI) ? "RCPI" : "RSSI");
	monitor_sta->channel = rmrep_bcn->channel;
	monitor_sta->regclass = rmrep_bcn->reg;
	monitor_sta->bcn_tm = wbd_bcn_rpt->timestamp;
}

/* Got Beacon Metrics metric response */
void
wbd_master_beacon_metric_resp(unsigned char *al_mac, ieee1905_beacon_report *report)
{
	int ret = WBDE_OK, len_read = 0, i;
	uint8 prb_band;
	i5_dm_device_type *i5_device;
	i5_dm_bss_type *i5_bss;
	i5_dm_clients_type *i5_assoc_sta;
	dot11_rm_ie_t *ie;
	dot11_rmrep_bcn_t *rmrep_bcn;
	wbd_assoc_sta_item_t *assoc_sta = NULL;
	wbd_monitor_sta_item_t *monitor_sta = NULL;
	wbd_prb_sta_t *prbsta;
	wbd_beacon_reports_t *wbd_bcn_rpt = NULL;
	WBD_ENTER();

	WBD_SAFE_FIND_I5_DEVICE(i5_device, al_mac, &ret);

	wbd_bcn_rpt = wbd_ds_find_item_fm_beacon_reports(wbd_get_ginfo(),
		(struct ether_addr*)report->sta_mac, &ret);

	if (!wbd_bcn_rpt) {
		WBD_WARNING("No beacon report found for STA["MACDBG"]\n",
			MAC2STRDBG(report->sta_mac));
		goto end;
	}

	for (i = 0; i < report->report_element_count; i++) {
		ie = (dot11_rm_ie_t *)(report->report_element + len_read);
		len_read += (2 + ie->len);
		rmrep_bcn = (dot11_rmrep_bcn_t *)&ie[1];

		/* Update the probe table with band and active timestamp */
		prb_band = WBD_BAND_FROM_CHANNEL(rmrep_bcn->channel);
		/* If its dual band channel, update the probe STA table */
		if (WBD_IS_DUAL_BAND(prb_band)) {
			prbsta = wbd_ds_find_sta_in_probe_sta_table(wbd_get_ginfo(),
				(struct ether_addr*)report->sta_mac, FALSE);
			if (prbsta) {
				prbsta->active = time(NULL);
				prbsta->band |= prb_band;
			} else {
				WBD_WARNING("Warning: probe list has no STA["MACDBG"]\n",
					MAC2STRDBG(report->sta_mac));
			}
		}

		WBD_INFO("From Device["MACDBG"] STA["MACDBG"] BEACON EVENT, regclass: %d, "
			"channel: %d, rcpi: %d, bssid["MACF"]\n",
			MAC2STRDBG(i5_device->DeviceId), MAC2STRDBG(report->sta_mac),
			rmrep_bcn->reg, rmrep_bcn->channel, rmrep_bcn->rcpi,
			ETHER_TO_MACF(rmrep_bcn->bssid));
		i5_bss = wbd_ds_get_i5_bss_in_topology((unsigned char*)&rmrep_bcn->bssid, &ret);
		if (ret != WBDE_OK) {
			WBD_INFO("BEACON EVENT from Device["MACDBG"] for BSS["MACF"]. %s\n",
				MAC2STRDBG(al_mac), ETHER_TO_MACF(rmrep_bcn->bssid),
				wbderrorstr(ret));
			continue;
		}
		if (I5_IS_BSS_GUEST(i5_bss->mapFlags)) {
			WBD_INFO("BEACON EVENT from Device["MACDBG"] for Guest BSS["MACF"]."
				" Not Supported\n",
				MAC2STRDBG(al_mac), ETHER_TO_MACF(rmrep_bcn->bssid));
			continue;
		}

		/* Check in Monitor list */
		monitor_sta = wbd_ds_find_sta_in_bss_monitorlist(i5_bss,
			(struct ether_addr*)&report->sta_mac, &ret);
		if (monitor_sta) {
			wbd_master_update_beacon_rssi(wbd_bcn_rpt, rmrep_bcn, monitor_sta);
			continue;
		}

		/* Find it in assoc list of the BSS. This is for the BSS where the STA is
		 * associated to store the beacon report RSSI
		 */
		assoc_sta = NULL;
		i5_assoc_sta = wbd_ds_find_sta_in_bss_assoclist(i5_bss,
			(struct ether_addr*)report->sta_mac, &ret, &assoc_sta);
		if (i5_assoc_sta && assoc_sta) {
			wbd_sta_bounce_detect_t *entry = wbd_ds_find_sta_in_bouncing_table(
				wbd_get_ginfo()->wbd_master, (struct ether_addr*)report->sta_mac);
			if (!entry) {
				WBD_WARNING("No bouncing table entry for STA["MACDBG"] to check if"
					"it sends RSSI or RCPI\n", MAC2STRDBG(report->sta_mac));
				continue;
			}
			assoc_sta->stats.bcn_rpt_rssi = wbd_master_conv_rcpi_to_rssi(
				&entry->rssi_or_rcpi, rmrep_bcn->rcpi);
			assoc_sta->stats.bcn_tm = wbd_bcn_rpt->timestamp;
			WBD_INFO("Assoc STA["MACDBG"] RCPI recieved: [%d] converted RSSI: [%d]"
				"rssi_or_rcpi: %s\n", MAC2STRDBG(report->sta_mac),
				rmrep_bcn->rcpi, assoc_sta->stats.bcn_rpt_rssi,
				(entry->rssi_or_rcpi == WBD_VALUE_TYPE_RCPI) ? "RCPI" : "RSSI");
		} else {
			WBD_WARNING("STA["MACDBG"] not found in BSS["MACDBG"]'s "
				"Monitor list and assoc list. Add it to monitor list\n",
				MAC2STRDBG(report->sta_mac), MAC2STRDBG(i5_bss->BSSID));
			/* If monitor STA item not exists add it */
			monitor_sta = wbd_master_add_to_monitor_list(i5_bss, report->sta_mac);
			if (monitor_sta) {
				wbd_master_update_beacon_rssi(wbd_bcn_rpt, rmrep_bcn, monitor_sta);
			}
		}
	}

end:
	WBD_EXIT();
}

/* Update the AP channel report */
void
wbd_master_update_ap_chan_report(wbd_glist_t *ap_chan_report, i5_dm_interface_type *i5_ifr)
{
	int ret = WBDE_OK;
	unsigned char channel;
	wbd_bcn_req_rclass_list_t *rclass_list = NULL;
	wbd_bcn_req_chan_list_t *chan_list = NULL;
	WBD_ENTER();

	/* if the rclass is present needs to add channel to the rclass list */
	rclass_list = wbd_ds_find_rclass_in_ap_chan_report(ap_chan_report, i5_ifr->opClass, &ret);
	if (rclass_list == NULL) {
		ret = wbd_ds_add_rclass_in_ap_chan_report(ap_chan_report, i5_ifr->opClass,
			&rclass_list);
		WBD_ASSERT();
	}

	/* If channel present no need to add the channel */
	channel = wf_chspec_ctlchan(i5_ifr->chanspec);
	chan_list = wbd_ds_find_channel_in_rclass_list(rclass_list, channel, &ret);
	if (chan_list == NULL) {
		ret = wbd_ds_add_channel_in_rclass_list(rclass_list, channel, NULL);
	}

end:
	WBD_EXIT();
}

/* Create AP chan report from topology */
static void
wbd_master_create_ap_chan_report(i5_dm_network_topology_type *i5_topology,
	wbd_master_info_t *master_info)
{
	int ret = WBDE_OK;
	i5_dm_device_type *i5_iter_device;
	i5_dm_interface_type *i5_iter_ifr;
	WBD_ENTER();

	WBD_ASSERT_ARG(i5_topology, WBDE_INV_ARG);
	WBD_ASSERT_ARG(master_info, WBDE_INV_ARG);

	/* Clean up the AP channel report */
	wbd_ds_ap_chan_report_cleanup(&master_info->ap_chan_report);

	/* Update the AP chan report with new channel values */
	foreach_i5glist_item(i5_iter_device, i5_dm_device_type, i5_topology->device_list) {
		if (!I5_IS_MULTIAP_AGENT(i5_iter_device->flags)) {
			continue;
		}

		foreach_i5glist_item(i5_iter_ifr, i5_dm_interface_type,
			i5_iter_device->interface_list) {

			if ((i5_iter_ifr->chanspec == 0) ||
				!i5DmIsInterfaceWireless(i5_iter_ifr->MediaType) ||
				!i5_iter_ifr->BSSNumberOfEntries ||
				wbd_ds_is_interface_dedicated_backhaul(i5_iter_ifr)) {
				continue;
			}
			wbd_master_update_ap_chan_report(&master_info->ap_chan_report, i5_iter_ifr);
		}
	}
end:
	WBD_EXIT();
}

/* Callback from IEEE 1905 module to get interface info */
int
wbd_master_get_interface_info_cb(char *ifname, ieee1905_ifr_info *info)
{
	WBD_ENTER();

	/* Check interface (for valid BRCM wl interface) */
	(void)blanket_probe(ifname);

	blanket_get_chanspec(ifname, &info->chanspec);

	WBD_EXIT();
	return WBDE_OK;
}
/* Callback for exception from communication handler for master server */
static void
wbd_master_com_server_exception(wbd_com_handle *hndl, void *arg, WBD_COM_STATUS status)
{
	WBD_ERROR("Exception from Master server\n");
}

/* Callback for exception from communication handler for CLI */
static void
wbd_master_com_cli_exception(wbd_com_handle *hndl, void *arg, WBD_COM_STATUS status)
{
	WBD_ERROR("Exception from CLI server\n");
}

/* Register all the commands for master server to communication handle */
static int
wbd_master_register_server_command(wbd_info_t *info)
{
	/* Now register CLI commands */
	wbd_com_register_cmd(info->com_cli_hdl, WBD_CMD_CLI_VERSION,
		wbd_process_version_cli_cmd, info, wbd_json_parse_cli_cmd);

	wbd_com_register_cmd(info->com_cli_hdl, WBD_CMD_CLI_SLAVELIST,
		wbd_master_process_bsslist_cli_cmd, info, wbd_json_parse_cli_cmd);

	wbd_com_register_cmd(info->com_cli_hdl, WBD_CMD_CLI_INFO,
		wbd_master_process_info_cli_cmd, info, wbd_json_parse_cli_cmd);

	wbd_com_register_cmd(info->com_cli_hdl, WBD_CMD_CLI_CLIENTLIST,
		wbd_master_process_clientlist_cli_cmd, info, wbd_json_parse_cli_cmd);

	wbd_com_register_cmd(info->com_cli_hdl, WBD_CMD_CLI_STEER,
		wbd_master_process_steer_cli_cmd, info, wbd_json_parse_cli_cmd);

	wbd_com_register_cmd(info->com_cli_hdl, WBD_CMD_CLI_LOGS,
		wbd_master_process_logs_cli_cmd, info, wbd_json_parse_cli_cmd);

	wbd_com_register_cmd(info->com_cli_hdl, WBD_CMD_CLI_MSGLEVEL,
		wbd_process_set_msglevel_cli_cmd, info, wbd_json_parse_cli_cmd);

	wbd_com_register_cmd(info->com_cli_hdl, WBD_CMD_CLI_BHOPT,
		wbd_master_process_bh_opt_cli_cmd, info, wbd_json_parse_cli_cmd);

	return WBDE_OK;
}

/* Initialize the communication module for master */
int
wbd_init_master_com_handle(wbd_info_t *info)
{
	/* Initialize communication module for server FD */
	info->com_serv_hdl = wbd_com_init(info->hdl, info->server_fd, 0x0000,
		wbd_json_parse_cmd_name, wbd_master_com_server_exception, info);
	if (!info->com_serv_hdl) {
		WBD_ERROR("Failed to initialize the communication module for master server\n");
		return WBDE_COM_ERROR;
	}

	/* Initialize communication module for CLI */
	info->com_cli_hdl = wbd_com_init(info->hdl, info->cli_server_fd, 0x0000,
		wbd_json_parse_cli_cmd_name, wbd_master_com_cli_exception, info);
	if (!info->com_cli_hdl) {
		WBD_ERROR("Failed to initialize the communication module for CLI server\n");
		return WBDE_COM_ERROR;
	}

	wbd_master_register_server_command(info);

	return WBDE_OK;
}

/* Callback fn to send autoconfig renew message to unconfigured slaves on master restart */
static void
wbd_master_send_ap_autoconfig_renew_timer_cb(bcm_usched_handle *hdl, void *arg)
{
	int ret = WBDE_OK, send_renew = 0;
	i5_dm_network_topology_type *i5_topology;
	i5_dm_device_type *i5_device;
	i5_dm_interface_type *i5_ifr;
	static int restart_count = WBD_RENEW_TIMER_RESTART_COUNT;
	wbd_info_t *info = (wbd_info_t *)arg;

	WBD_ENTER();
	BCM_REFERENCE(ret);

	WBD_ASSERT_ARG(info, WBDE_INV_ARG);

	i5_topology = ieee1905_get_datamodel();

	/* Traverse Device List to send it to all the device in the network */
	foreach_i5glist_item(i5_device, i5_dm_device_type, i5_topology->device_list) {
		if (!(I5_IS_MULTIAP_AGENT(i5_device->flags))) {
			WBD_DEBUG("Device["MACDBG"] not slave. continue...\n",
				MAC2STRDBG(i5_device->DeviceId));
			continue;
		}
		foreach_i5glist_item(i5_ifr, i5_dm_interface_type, i5_device->interface_list) {
			if (i5_ifr->isConfigured == 1) {
				WBD_DEBUG("Device["MACDBG"] Interface["MACDBG"] is already "
					"configured\n", MAC2STRDBG(i5_device->DeviceId),
					MAC2STRDBG(i5_ifr->InterfaceId));
				break;
			}
		}
		if (i5_ifr) {
			continue;
		}

		/* As renew is multicast message, if the interface is not configured for any one
		 * device, just send it by breaking this loop
		 */
		send_renew = 1;
		WBD_INFO("Device["MACDBG"] not configured. Send renew\n",
			MAC2STRDBG(i5_device->DeviceId));
		break;
	}

	if (send_renew) {
		ieee1905_send_ap_autoconfig_renew(NULL);
	}

	restart_count--;
	if (restart_count > 0) {
		wbd_remove_timers(info->hdl, wbd_master_send_ap_autoconfig_renew_timer_cb, arg);
		ret = wbd_add_timers(info->hdl, info,
			WBD_SEC_MICROSEC(info->max.tm_autoconfig_renew),
			wbd_master_send_ap_autoconfig_renew_timer_cb, 0);
		if (ret != WBDE_OK) {
			WBD_WARNING("Interval[%d] Failed to create AP autoconfig renew timer "
				"Error: %d\n", info->max.tm_autoconfig_renew, ret);
		}
	}
end:
	WBD_EXIT();
}

/* Handle master restart; Send renew if the agent didn't AP Auto configuration */
int wbd_master_create_ap_autoconfig_renew_timer(wbd_info_t *info)
{
	int ret = WBDE_OK;
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(info, WBDE_INV_ARG);

	WBD_INFO("Timer created for auto config nenew, Interval [%d]\n",
		info->max.tm_autoconfig_renew);
	ret = wbd_add_timers(info->hdl, info,
		WBD_SEC_MICROSEC(info->max.tm_autoconfig_renew),
		wbd_master_send_ap_autoconfig_renew_timer_cb, 0);
	if (ret != WBDE_OK) {
		WBD_WARNING("Interval[%d] Failed to create AP autoconfig renew timer "
			"Error: %d\n", info->max.tm_autoconfig_renew, ret);
	}

end:
	WBD_EXIT();
	return ret;
}

/* Callback fn to send channel selection request to configured agents */
static void
wbd_master_send_channel_select_cb(bcm_usched_handle *hdl, void *arg)
{
	int ret = WBDE_OK;
	i5_dm_device_type *device = (i5_dm_device_type *)arg;

	WBD_ENTER();
	BCM_REFERENCE(ret);

	WBD_ASSERT_ARG(device, WBDE_INV_ARG);

	WBD_INFO("Sending channel selection request to ["MACF"]\n",
		ETHERP_TO_MACF(device->DeviceId));
	wbd_master_send_channel_selection_request(wbd_get_ginfo(),
		device->DeviceId, NULL);
end:
	WBD_EXIT();
}

/* Callback fn to send channel prefernce query to configured agents */
static void
wbd_master_send_channel_preference_query_cb(bcm_usched_handle *hdl, void *arg)
{
	int ret = WBDE_OK;
	i5_dm_device_type *device = (i5_dm_device_type *)arg;

	WBD_ENTER();
	BCM_REFERENCE(ret);

	WBD_ASSERT_ARG(device, WBDE_INV_ARG);

	WBD_INFO("Sending channel preference query to ["MACF"]\n",
		ETHERP_TO_MACF(device->DeviceId));
	ieee1905_send_channel_preference_query(device->DeviceId);

	/* start the timer to send channel selection request */
	ret = wbd_add_timers(wbd_get_ginfo()->hdl, device,
		WBD_SEC_MICROSEC(WBD_TM_CHAN_SELECT_AFTER_QUERY),
		wbd_master_send_channel_select_cb, 0);
	if (ret != WBDE_OK) {
		WBD_WARNING("Timeout[%d] Failed to create channel select timer "
			"Error: %d\n", WBD_TM_CHAN_SELECT_AFTER_QUERY, ret);
	}
end:
	WBD_EXIT();
}

/* Create channel select timer */
int
wbd_master_create_channel_select_timer(wbd_info_t *info, unsigned char *al_mac)
{
	int ret = WBDE_OK;
	i5_dm_device_type *device;
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(info, WBDE_INV_ARG);
	WBD_ASSERT_ARG(al_mac, WBDE_INV_ARG);

	device = wbd_ds_get_i5_device(al_mac, &ret);
	WBD_ASSERT();

	WBD_INFO("Creating timer for channel select, timeout [%d] AL mac ["MACF"]\n",
		info->max.tm_channel_select, ETHERP_TO_MACF(al_mac));
	ret = wbd_add_timers(info->hdl, device,
		WBD_SEC_MICROSEC(info->max.tm_channel_select),
		wbd_master_send_channel_preference_query_cb, 0);
	if (ret != WBDE_OK) {
		if (ret == WBDE_USCHED_TIMER_EXIST) {
			WBD_INFO("channel select timer already exist\n");
		} else {
			WBD_WARNING("Timeout[%d] Failed to create channel select timer "
				"Error: %d\n", info->max.tm_channel_select, ret);
		}
	}

end:
	WBD_EXIT();
	return ret;
}

#if defined(MULTIAPR2)
/* Callback fn to send OnBoot Channel Scan Request from Controller to Agent */
static void
wbd_master_send_onboot_channel_scan_req_cb(bcm_usched_handle *hdl, void *arg)
{
	int ret = WBDE_OK;
	i5_dm_device_type *device = (i5_dm_device_type *)arg;

	WBD_ENTER();
	BCM_REFERENCE(ret);

	WBD_ASSERT_ARG(device, WBDE_INV_ARG);

	WBD_INFO("Sending OnBoot Channel Scan Request to ["MACF"]\n",
		ETHERP_TO_MACF(device->DeviceId));

	/* Send Channel Scan Request Message */
	wbd_master_send_chscan_req(device->DeviceId, 0);

end:
	WBD_EXIT();
}

/* Create OnBoot Channel Scan Request timer to Get Channel Scan Report from this Agent */
int
wbd_master_create_onboot_channel_scan_req_timer(wbd_info_t *info, unsigned char *al_mac)
{
	int ret = WBDE_OK;
	i5_dm_device_type *device;
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(info, WBDE_INV_ARG);
	WBD_ASSERT_ARG(al_mac, WBDE_INV_ARG);

	device = wbd_ds_get_i5_device(al_mac, &ret);
	WBD_ASSERT();

	WBD_INFO("Master Creating timer for OnBoot Channel Scan Request, timeout[%d] AL["MACF"]\n",
		info->max.tm_onboot_chscan_req, ETHERP_TO_MACF(al_mac));

	/* Add the timer */
	ret = wbd_add_timers(info->hdl, device,
		WBD_SEC_MICROSEC(info->max.tm_onboot_chscan_req),
		wbd_master_send_onboot_channel_scan_req_cb, 0);

	/* If Add Timer not Succeeded */
	if (ret != WBDE_OK) {
		if (ret == WBDE_USCHED_TIMER_EXIST) {
			WBD_INFO("OnBoot Channel Scan Request timer already exist\n");
		} else {
			WBD_WARNING("Timeout[%d] Failed to create OnBoot Channel Scan Request "
				"timer Error: %d\n", info->max.tm_onboot_chscan_req, ret);
		}
	}

end:
	WBD_EXIT();
	return ret;
}
#endif /* MULTIAPR2 */

/* Update the lowest Tx Power of all BSS */
void
wbd_master_update_lowest_tx_pwr(wbd_info_t *info, unsigned char *al_mac,
	ieee1905_operating_chan_report *chan_report)
{
	int ret = WBDE_OK;
	i5_dm_interface_type *i5_ifr;
	WBD_ENTER();

	WBD_SAFE_GET_I5_IFR(al_mac, chan_report->radio_mac, i5_ifr, &ret);
	i5_ifr->TxPowerLimit = chan_report->tx_pwr;

	if (i5_ifr->TxPowerLimit == 0) {
		WBD_WARNING("tx power limit not receivied\n");
	} else if (info->base_txpwr <= 0 || info->base_txpwr > i5_ifr->TxPowerLimit) {
		info->base_txpwr = i5_ifr->TxPowerLimit;
	}

end:
	WBD_EXIT();
}

/* Store beacon reports sent from sta. Only one report from sta should be present in list.
 * These reports can be checked for timestamp before sending any new beacon metric query
 * If the sta is still associated to same device.
*/
void
wbd_master_store_beacon_metric_resp(wbd_info_t *info, unsigned char *al_mac,
	ieee1905_beacon_report *report)
{
	int ret = WBDE_OK;
	wbd_beacon_reports_t *wbd_bcn_rpt = NULL;
	uint8 *tmpbuf = NULL;
	uint16 len = 0;
	time_t now = 0;
	WBD_ENTER();

	/* Skip storing this report if response flags is not success
	 * or report count or report length is 0
	*/
	WBD_INFO("STA["MACDBG"] Device["MACDBG"] Beacon Report details "
		"response[%d] element_count[%d] element_len[%d]\n",
		MAC2STRDBG(report->sta_mac), MAC2STRDBG(al_mac), report->response,
		report->report_element_count, report->report_element_len);

	if ((report->response != IEEE1905_BEACON_REPORT_RESP_FLAG_SUCCESS) ||
		!report->report_element_count || !report->report_element_len) {
		goto end;
	}

	/* Check whether we already have reports recieved from this STA */
	wbd_bcn_rpt = wbd_ds_find_item_fm_beacon_reports(info,
		(struct ether_addr*)&(report->sta_mac), &ret);

	now = time(NULL);

	if (wbd_bcn_rpt && (!eacmp((struct ether_addr*)&al_mac, &(wbd_bcn_rpt->neighbor_al_mac))) &&
		((now - wbd_bcn_rpt->timestamp) < WBD_MIN_BCN_METRIC_QUERY_DELAY)) {
		/* Since we already have beacon report from sta, this might be a case of
		 * multiple beacon responses from agent, update the present beacon report
		*/

		WBD_INFO("Beacon report already present. Append to it \n");

		len = report->report_element_len + wbd_bcn_rpt->report_element_len;
		tmpbuf = (uint8*)wbd_malloc(len, &ret);
		WBD_ASSERT_MSG(" STA["MACDBG"] Beacon report element malloc failed\n",
			MAC2STRDBG(report->sta_mac));

		memcpy(tmpbuf, wbd_bcn_rpt->report_element, wbd_bcn_rpt->report_element_len);
		free(wbd_bcn_rpt->report_element);
		wbd_bcn_rpt->report_element = NULL;

		memcpy(tmpbuf + wbd_bcn_rpt->report_element_len, report->report_element,
				report->report_element_len);

		wbd_bcn_rpt->report_element = tmpbuf;
		wbd_bcn_rpt->report_element_count += report->report_element_count;
		wbd_bcn_rpt->report_element_len = len;

		goto end;
	} else {
		if (wbd_bcn_rpt) {
			/* Remove the old beacon report */
			wbd_ds_remove_beacon_report(info, (struct ether_addr*)&(report->sta_mac));
		}
	}

	/* Add report to glist */
	wbd_bcn_rpt = wbd_ds_add_item_to_beacon_reports(info, (struct ether_addr*)al_mac, now,
		(struct ether_addr*)&report->sta_mac);
	if (!wbd_bcn_rpt) {
		WBD_WARNING("STA["MACDBG"] Beacon report store failed\n",
			MAC2STRDBG(report->sta_mac));
		goto end;
	}

	if (NULL == wbd_ds_find_sta_in_bouncing_table(info->wbd_master,
			(struct ether_addr*)&report->sta_mac)) {
		WBD_INFO("STA["MACDBG"] Not in bouncing table. Adding it since "
			"beacon report received\n", MAC2STRDBG(report->sta_mac));
		/* Sending bssid as NULL to skip steering related initializations */
		wbd_ds_add_sta_to_bounce_table(info->wbd_master,
			(struct ether_addr*)&report->sta_mac, NULL, 0);
	}

	wbd_bcn_rpt->report_element_count = report->report_element_count;
	wbd_bcn_rpt->report_element_len = report->report_element_len;

	wbd_bcn_rpt->report_element = (uint8*)wbd_malloc(report->report_element_len, &ret);
	WBD_ASSERT_MSG(" STA["MACDBG"] Beacon report element malloc failed\n",
		MAC2STRDBG(report->sta_mac));

	memcpy(wbd_bcn_rpt->report_element, report->report_element, report->report_element_len);

end:
	WBD_EXIT();
}

/* Send BSS capability query message */
int
wbd_master_send_bss_capability_query(wbd_info_t *info, unsigned char *al_mac,
	unsigned char* radio_mac)
{
	int ret = WBDE_OK;
	ieee1905_vendor_data vndr_msg_data;
	WBD_ENTER();

	memset(&vndr_msg_data, 0x00, sizeof(vndr_msg_data));

	/* Allocate Dynamic mem for Vendor data from App */
	vndr_msg_data.vendorSpec_msg =
		(unsigned char *)wbd_malloc(IEEE1905_MAX_VNDR_DATA_BUF, &ret);
	WBD_ASSERT();

	/* Fill vndr_msg_data struct object to send Vendor Message */
	memcpy(vndr_msg_data.neighbor_al_mac, al_mac, IEEE1905_MAC_ADDR_LEN);

	WBD_INFO("Send BSS capability query from Device["MACDBG"] to Device["MACDBG"]\n",
		MAC2STRDBG(ieee1905_get_al_mac()), MAC2STRDBG(vndr_msg_data.neighbor_al_mac));

	/* Encode Vendor Specific TLV for Message : BSS capability query to send */
	ret = wbd_tlv_encode_bss_capability_query((void *)radio_mac,
		vndr_msg_data.vendorSpec_msg, &vndr_msg_data.vendorSpec_len);
	WBD_ASSERT_MSG("Failed to encode BSS capability query which needs to be sent "
		"from Device["MACDBG"] to Device["MACDBG"]\n",
		MAC2STRDBG(ieee1905_get_al_mac()), MAC2STRDBG(vndr_msg_data.neighbor_al_mac));

	/* Send Vendor Specific Message : BSS capability query */
	ret = ieee1905_send_vendor_specific_msg(&vndr_msg_data);

	WBD_CHECK_ERR_MSG(WBDE_DS_1905_ERR, "Failed to send "
		"BSS capability query from Device["MACDBG"] to Device["MACDBG"], Error : %s\n",
		MAC2STRDBG(ieee1905_get_al_mac()), MAC2STRDBG(vndr_msg_data.neighbor_al_mac),
		wbderrorstr(ret));

end:
	if (vndr_msg_data.vendorSpec_msg) {
		free(vndr_msg_data.vendorSpec_msg);
	}
	WBD_EXIT();
	return ret;
}

/* Processes BSS capability report message */
static void
wbd_master_process_bss_capability_report_cmd(unsigned char *neighbor_al_mac,
	unsigned char *tlv_data, unsigned short tlv_data_len)
{
	int ret = WBDE_OK;
	i5_dm_device_type *i5_device = NULL;
	WBD_ENTER();

	/* Validate Message data */
	WBD_ASSERT_MSGDATA(tlv_data, "BSS Capability Report");

	WBD_SAFE_FIND_I5_DEVICE(i5_device, neighbor_al_mac, &ret);

	/* Decode Vendor Specific TLV for Message : BSS capability report on receive */
	ret = wbd_tlv_decode_bss_capability_report((void *)i5_device, tlv_data, tlv_data_len);
	if (ret != WBDE_OK) {
		WBD_WARNING("Failed to decode the BSS capability Report TLV\n");
		goto end;
	}

end:
	WBD_EXIT();
}

/* Send BSS metrics query message */
int
wbd_master_send_bss_metrics_query(unsigned char *al_mac)
{
	int ret = WBDE_OK;
	ieee1905_vendor_data vndr_msg_data;
	WBD_ENTER();

	memset(&vndr_msg_data, 0x00, sizeof(vndr_msg_data));

	/* Allocate Dynamic mem for Vendor data from App */
	vndr_msg_data.vendorSpec_msg =
		(unsigned char *)wbd_malloc(IEEE1905_MAX_VNDR_DATA_BUF, &ret);
	WBD_ASSERT();

	/* Fill vndr_msg_data struct object to send Vendor Message */
	memcpy(vndr_msg_data.neighbor_al_mac, al_mac, IEEE1905_MAC_ADDR_LEN);

	WBD_INFO("Send BSS metrics query from Device["MACDBG"] to Device["MACDBG"]\n",
		MAC2STRDBG(ieee1905_get_al_mac()), MAC2STRDBG(vndr_msg_data.neighbor_al_mac));

	/* Encode Vendor Specific TLV for Message : BSS metrics query to send */
	ret = wbd_tlv_encode_bss_metrics_query(NULL,
		vndr_msg_data.vendorSpec_msg, &vndr_msg_data.vendorSpec_len);
	WBD_ASSERT_MSG("Failed to encode BSS metrics query which needs to be sent "
		"from Device["MACDBG"] to Device["MACDBG"]\n",
		MAC2STRDBG(ieee1905_get_al_mac()), MAC2STRDBG(vndr_msg_data.neighbor_al_mac));

	/* Send Vendor Specific Message : BSS metrics query */
	ret = ieee1905_send_vendor_specific_msg(&vndr_msg_data);

	WBD_CHECK_ERR_MSG(WBDE_DS_1905_ERR, "Failed to send "
		"BSS metrics query from Device["MACDBG"] to Device["MACDBG"], Error : %s\n",
		MAC2STRDBG(ieee1905_get_al_mac()), MAC2STRDBG(vndr_msg_data.neighbor_al_mac),
		wbderrorstr(ret));

end:
	if (vndr_msg_data.vendorSpec_msg) {
		free(vndr_msg_data.vendorSpec_msg);
	}
	WBD_EXIT();
	return ret;
}

/* Processes BSS metrics report message */
static void
wbd_master_process_bss_metrics_report_cmd(unsigned char *neighbor_al_mac,
	unsigned char *tlv_data, unsigned short tlv_data_len)
{
	int ret = WBDE_OK;
	i5_dm_device_type *i5_device = NULL;
	wbd_tlv_decode_cmn_data_t cmn_data;
	WBD_ENTER();

	/* Validate Message data */
	WBD_ASSERT_MSGDATA(tlv_data, "BSS metrics Report");

	WBD_SAFE_FIND_I5_DEVICE(i5_device, neighbor_al_mac, &ret);
	cmn_data.info = wbd_get_ginfo();
	cmn_data.data = (void *)i5_device;

	/* Decode Vendor Specific TLV for Message : BSS metrics report on receive */
	ret = wbd_tlv_decode_bss_metrics_report((void *)&cmn_data, tlv_data, tlv_data_len);
	WBD_ASSERT_MSG("Failed to decode the BSS metrics Report TLV From Device["MACDBG"]\n",
		MAC2STRDBG(neighbor_al_mac));

end:
	WBD_EXIT();
}

/* Processes steer response report message */
static void
wbd_master_process_steer_resp_report_cmd(unsigned char *neighbor_al_mac,
	unsigned char *tlv_data, unsigned short tlv_data_len)
{
	int ret = WBDE_OK;
	wbd_cmd_steer_resp_rpt_t steer_resp_rpt;
	char logmsg[WBD_MAX_BUF_128] = {0}, timestamp[WBD_MAX_BUF_32] = {0};

	WBD_ENTER();

	/* Validate Message data */
	WBD_ASSERT_MSGDATA(tlv_data, "Steer Response Report");

	memset(&steer_resp_rpt, 0, sizeof(steer_resp_rpt));

	/* Decode Vendor Specific TLV for Message : Steer Response report on receive */
	ret = wbd_tlv_decode_steer_resp_report(&steer_resp_rpt, tlv_data, tlv_data_len);
	WBD_ASSERT_MSG("Failed to decode the Steer Response Report TLV From Device["MACDBG"]\n",
		MAC2STRDBG(neighbor_al_mac));

	/* Create and store steer resp log */
	snprintf(logmsg, sizeof(logmsg), CLI_CMD_LOGS_STEER_NO_RESP,
		wbd_get_formated_local_time(timestamp, sizeof(timestamp)),
		MAC2STRDBG(steer_resp_rpt.sta_mac));
	wbd_ds_add_logs_in_master(wbd_get_ginfo()->wbd_master, logmsg);
end:
	WBD_EXIT();
}

/* skip zwdfs command for dedicated backhaul, else broadcast to all agents
 * operating in same band with interface originally intiated the request
 */
static void
wbd_master_process_zwdfs_cmd(unsigned char *src_al_mac, unsigned char *tlv_data,
	unsigned short tlv_data_len)
{
	int ret = WBDE_OK;
	wbd_info_t *info = wbd_get_ginfo();
	wbd_cmd_vndr_zwdfs_msg_t zwdfs_msg;
	i5_dm_interface_type *pdmif = NULL;

	WBD_ENTER();

	if (WBD_MCHAN_ENAB(info->flags)) {
		WBD_INFO("Multi chan mode is ON, skip inform ZWDFS msg to other agents \n");
		goto end;
	}

	memset(&zwdfs_msg, 0, sizeof(zwdfs_msg));

	ret = wbd_tlv_decode_zwdfs_msg((void*)&zwdfs_msg, tlv_data, tlv_data_len);
	WBD_ASSERT();

	pdmif = wbd_ds_get_i5_interface(src_al_mac, (uchar*)&zwdfs_msg.mac, &ret);
	WBD_ASSERT();

	WBD_INFO("Rcvd ZWDFS msg for reason[%d] from: SRC_AL_MAC["MACDBG"] SRC interface["MACF"]"
		" cntrl_chan[%d], opclass[%d]\n", zwdfs_msg.reason, MAC2STRDBG(src_al_mac),
		ETHERP_TO_MACF(pdmif->InterfaceId), zwdfs_msg.cntrl_chan, zwdfs_msg.opclass);

	if (wbd_ds_is_interface_dedicated_backhaul(pdmif)) {
		WBD_INFO("Interface ["MACF"] is dedicated Backhaul. Don't force "
			"ZWDFS message \n", ETHERP_TO_MACF(pdmif->InterfaceId));
		goto end;
	}
	ret = wbd_master_broadcast_vendor_msg_zwdfs(src_al_mac, &zwdfs_msg,
		ieee1905_get_band_from_radiocaps(&pdmif->ApCaps.RadioCaps));
	if (ret != WBDE_OK) {
		WBD_ERROR(" error in informing ZWDFS msg to agents \n");
	}
end:
	WBD_EXIT();
}

/* Send 1905 Vendor Specific Zero wait DFS command, from Controller to Agents */
static int
wbd_master_broadcast_vendor_msg_zwdfs(uint8 *src_al_mac, wbd_cmd_vndr_zwdfs_msg_t *msg, uint8 band)
{
	int ret = WBDE_OK;
	ieee1905_vendor_data vndr_msg_data;
	i5_dm_network_topology_type *ctlr_topology;
	wbd_cmd_vndr_zwdfs_msg_t zwdfs_msg;
	i5_dm_device_type *i5_iter_device;
	i5_dm_interface_type *i5_iter_ifr;

	WBD_ENTER();

	memset(&vndr_msg_data, 0, sizeof(vndr_msg_data));

	/* prepare common elements for all agents */
	memset(&zwdfs_msg, 0, sizeof(zwdfs_msg));

	zwdfs_msg.cntrl_chan = msg->cntrl_chan;
	zwdfs_msg.opclass = msg->opclass;
	zwdfs_msg.reason = msg->reason;
	/* Allocate Dynamic mem for Vendor data from App */
	vndr_msg_data.vendorSpec_msg =
		(unsigned char *)wbd_malloc((sizeof(wbd_cmd_vndr_zwdfs_msg_t) + sizeof(i5_tlv_t)),
			&ret);
	WBD_ASSERT();

	/* Get Topology of Controller from 1905 lib */
	ctlr_topology = (i5_dm_network_topology_type *)ieee1905_get_datamodel();

	/* Loop for all the Devices in Controller, to send zero wait dfs msg to all Agents */
	foreach_i5glist_item(i5_iter_device, i5_dm_device_type, ctlr_topology->device_list) {
		if (i5DmDeviceIsSelf(i5_iter_device->DeviceId)) {
			WBD_DEBUG(" self device, ignore continue \n");
			continue;
		}
		if ((eacmp(i5_iter_device->DeviceId, src_al_mac) == 0)) {
			WBD_DEBUG("same device as SRC AL MAC , ignore, continue\n");
			continue;
		}
		foreach_i5glist_item(i5_iter_ifr, i5_dm_interface_type,
			i5_iter_device->interface_list) {

			if ((i5_iter_ifr->chanspec == 0) ||
				!i5DmIsInterfaceWireless(i5_iter_ifr->MediaType)) {
				continue;
			}
			if (band !=
				ieee1905_get_band_from_radiocaps(&i5_iter_ifr->ApCaps.RadioCaps)) {
				WBD_INFO("skip broadcast zwdfs msg for required Mismatch band[%d],"
					" with agent["MACF"] if_band[%d]\n", band,
					ieee1905_get_band_from_radiocaps(
					&i5_iter_ifr->ApCaps.RadioCaps),
					ETHERP_TO_MACF(i5_iter_ifr->InterfaceId));
				continue;
			}
			/* Fill Destination AL_MAC */
			memcpy(vndr_msg_data.neighbor_al_mac, i5_iter_device->DeviceId,
				IEEE1905_MAC_ADDR_LEN);

			WBD_INFO("Send ZWDFS MSG for reason[%d] from Device["MACDBG"] to"
				"Device["MACDBG"] for interface["MACF"] cntrl_chan[%d],"
				" opclass[%d]\n",
				zwdfs_msg.reason, MAC2STRDBG(ieee1905_get_al_mac()),
				MAC2STRDBG(i5_iter_device->DeviceId),
				ETHERP_TO_MACF(i5_iter_ifr->InterfaceId), zwdfs_msg.cntrl_chan,
				zwdfs_msg.opclass);

			memcpy(&zwdfs_msg.mac, i5_iter_ifr->InterfaceId, ETHER_ADDR_LEN);

			wbd_tlv_encode_zwdfs_msg((void *)&zwdfs_msg, vndr_msg_data.vendorSpec_msg,
				&vndr_msg_data.vendorSpec_len);

			WBD_DEBUG("zwdfs msg len[%d]\n", vndr_msg_data.vendorSpec_len);
			/* Send Vendor Specific Message with Zero wait dfs TLV */
			ieee1905_send_vendor_specific_msg(&vndr_msg_data);
		}
	}

end:
	if (vndr_msg_data.vendorSpec_msg) {
		free(vndr_msg_data.vendorSpec_msg);
	}

	WBD_EXIT();
	return ret;

}

/* Send 1905 Vendor Specific backhaul STA mertric policy command, from Controller to Agent */
int
wbd_master_send_backhaul_sta_metric_policy_vndr_cmd(wbd_info_t *info,
	unsigned char *neighbor_al_mac, unsigned char *radio_mac)
{
	int ret = WBDE_OK;
	i5_dm_interface_type *i5_ifr;
	ieee1905_vendor_data vndr_msg_data;
	wbd_master_info_t *master;
	WBD_ENTER();

	memset(&vndr_msg_data, 0, sizeof(vndr_msg_data));

	if (!neighbor_al_mac || !radio_mac) {
		WBD_ERROR("Invalid Device or Radio address\n");
		goto end;
	}

	/* Send the backhaul STA metric policy vendor message only if there is a backhaul BSS */
	i5_ifr = wbd_ds_get_i5_interface(neighbor_al_mac, radio_mac, &ret);
	WBD_ASSERT();

	/* If there is no backhaul BSS on this interface, do not send the backhaul policy */
	if (!I5_IS_BSS_BACKHAUL(i5_ifr->mapFlags)) {
		WBD_DEBUG("In Device["MACDBG"] IFR["MACDBG"] mapFlags[%x], No backhaul BSS\n",
			MAC2STRDBG(neighbor_al_mac), MAC2STRDBG(radio_mac), i5_ifr->mapFlags);
		goto end;
	}

	WBD_SAFE_GET_MASTER_INFO(info, WBD_BKT_ID_BR0, master, (&ret));

	/* Allocate Dynamic mem for Vendor data from App */
	vndr_msg_data.vendorSpec_msg =
		(unsigned char *)wbd_malloc(IEEE1905_MAX_VNDR_DATA_BUF, &ret);
	WBD_ASSERT();

	/* Encode Vendor Specific TLV for Message : backhaul STA mertric policy to send */
	wbd_tlv_encode_backhaul_sta_metric_report_policy((void *)&master->metric_policy_bh,
		vndr_msg_data.vendorSpec_msg, &vndr_msg_data.vendorSpec_len);

	/* Fill Destination AL_MAC */
	memcpy(vndr_msg_data.neighbor_al_mac, neighbor_al_mac, IEEE1905_MAC_ADDR_LEN);

	WBD_INFO("Send backhaul STA mertric policy from Device["MACDBG"] to Device["MACDBG"]\n",
		MAC2STRDBG(ieee1905_get_al_mac()), MAC2STRDBG(neighbor_al_mac));

	/* Send Vendor Specific Message : backhaul STA mertric policy */
	ret = ieee1905_send_vendor_specific_msg(&vndr_msg_data);
	WBD_CHECK_ERR_MSG(WBDE_DS_1905_ERR, "Failed to send backhaul STA mertric policy from "
		"Device["MACDBG"] to Device["MACDBG"], Error : %s\n",
		MAC2STRDBG(ieee1905_get_al_mac()), MAC2STRDBG(vndr_msg_data.neighbor_al_mac),
		wbderrorstr(ret));

end:
	if (vndr_msg_data.vendorSpec_msg) {
		free(vndr_msg_data.vendorSpec_msg);
	}

	WBD_EXIT();
	return ret;
}

/* Create the NVRAM list for device */
static int
wbd_master_create_device_nvrams(wbd_cmd_vndr_nvram_set_t *cmd)
{
	int ret = WBDE_OK, idx = 0;
	WBD_ENTER();

	/* Allocate memory for the NVRAMs without prefix */
	cmd->n_common_nvrams = WBD_GET_NVRAMS_COUNT();
	cmd->common_nvrams = (wbd_cmd_vndr_nvram_t*)
		wbd_malloc((sizeof(wbd_cmd_vndr_nvram_t) * cmd->n_common_nvrams), &ret);
	WBD_ASSERT();

	/* For each NVRAMs */
	for (idx = 0; idx < WBD_GET_NVRAMS_COUNT(); idx++) {
		memcpy(cmd->common_nvrams[idx].name, nvram_params[idx].name,
			strlen(nvram_params[idx].name));
		/* Integer value NVRAMs */
		if (nvram_params[idx].flags & WBD_NVRAM_FLAG_VALINT) {
			snprintf(cmd->common_nvrams[idx].value,
				sizeof(cmd->common_nvrams[idx].value),
				"%d", blanket_get_config_val_int(NULL, nvram_params[idx].name,
				nvram_params[idx].def));
		} else { /* String value NVRAMs */
			snprintf(cmd->common_nvrams[idx].value,
				sizeof(cmd->common_nvrams[idx].value),
				"%s", blanket_nvram_safe_get(nvram_params[idx].name));
		}
	}

end:
	WBD_EXIT();
	return ret;
}

/* Send NVRAM set vendor specific message */
static int
wbd_master_send_vndr_nvram_set_cmd(i5_dm_device_type *i5_device)
{
	int ret = WBDE_OK;
	ieee1905_vendor_data vndr_msg_data;
	wbd_cmd_vndr_nvram_set_t cmd;
	wbd_device_item_t *device_vndr;
	WBD_ENTER();

	memset(&vndr_msg_data, 0, sizeof(vndr_msg_data));
	memset(&cmd, 0, sizeof(cmd));

	device_vndr = (wbd_device_item_t*)i5_device->vndr_data;
	/* If already sent, dont send */
	if (device_vndr->flags & WBD_DEV_FLAG_NVRAM_SET) {
		goto exit;
	}

	/* Allocate Dynamic mem for Vendor data from App */
	vndr_msg_data.vendorSpec_msg =
		(unsigned char *)wbd_malloc(IEEE1905_MAX_VNDR_DATA_BUF, &ret);
	WBD_ASSERT();

	ret = wbd_master_create_device_nvrams(&cmd);
	WBD_ASSERT();

	/* Encode Vendor Specific TLV for Message : NVRAM set to send */
	wbd_tlv_encode_nvram_set((void *)&cmd, vndr_msg_data.vendorSpec_msg,
		&vndr_msg_data.vendorSpec_len);

	/* Fill Destination AL_MAC */
	memcpy(vndr_msg_data.neighbor_al_mac, i5_device->DeviceId, IEEE1905_MAC_ADDR_LEN);

	WBD_INFO("Send NVRAM set vendor message from Device["MACDBG"] to Device["MACDBG"]\n",
		MAC2STRDBG(ieee1905_get_al_mac()), MAC2STRDBG(i5_device->DeviceId));

	/* Send Vendor Specific Message : NVRAM set */
	ret = ieee1905_send_vendor_specific_msg(&vndr_msg_data);
	WBD_CHECK_ERR_MSG(WBDE_DS_1905_ERR, "Failed to send NVRAM set vendor message from "
		"Device["MACDBG"] to Device["MACDBG"], Error : %s\n",
		MAC2STRDBG(ieee1905_get_al_mac()), MAC2STRDBG(vndr_msg_data.neighbor_al_mac),
		wbderrorstr(ret));
	device_vndr->flags |= WBD_DEV_FLAG_NVRAM_SET;

end:
	if (vndr_msg_data.vendorSpec_msg) {
		free(vndr_msg_data.vendorSpec_msg);
	}

	wbd_free_nvram_sets(&cmd);

exit:
	WBD_EXIT();
	return ret;
}

/* send master's dfs chan info i.e. intersection of all agent's chan info */
static void
wbd_master_send_dfs_chan_info(wbd_dfs_chan_info_t *dfs_chan_info,
	uint8 band, unsigned char *al_mac, unsigned char *radio_mac)
{
	ieee1905_vendor_data vndr_msg;
	wbd_cmd_vndr_controller_dfs_chan_info_t chan_info_msg;
	int ret = WBDE_OK;

	WBD_ENTER();

	memset(&vndr_msg, 0, sizeof(vndr_msg));
	memset(&chan_info_msg, 0, sizeof(chan_info_msg));

	vndr_msg.vendorSpec_msg =
		(unsigned char *)wbd_malloc(IEEE1905_MAX_VNDR_DATA_BUF, &ret);
	WBD_ASSERT();
	memcpy(vndr_msg.neighbor_al_mac, al_mac, IEEE1905_MAC_ADDR_LEN);

	memcpy(&chan_info_msg.mac, radio_mac, ETHER_ADDR_LEN);
	chan_info_msg.band = band;
	chan_info_msg.chan_info = dfs_chan_info;

	WBD_DEBUG("send msg to MAC["MACF"] band[%d] count[%d] \n",
		ETHERP_TO_MACF(&chan_info_msg.mac), chan_info_msg.band,
		chan_info_msg.chan_info->count);

	wbd_tlv_encode_dfs_chan_info((void*)&chan_info_msg, vndr_msg.vendorSpec_msg,
		&vndr_msg.vendorSpec_len);

	WBD_DEBUG("CHAN_INFO vendor msg len[%d] MACF["MACF"] band[%d]\n",
		vndr_msg.vendorSpec_len, ETHERP_TO_MACF(&chan_info_msg.mac),
		chan_info_msg.band);

	ieee1905_send_vendor_specific_msg(&vndr_msg);
	free(vndr_msg.vendorSpec_msg);

end:
	WBD_EXIT();
}

/* Verify chan info from valid interface's chan info, Get common dfs chan info
 * and Broadcast to each agent based on band matching.
 *
 * DFS chan info only valid for 2G.
 *
 */
static void
wbd_master_process_intf_chan_info(unsigned char *src_al_mac, unsigned char *tlv_data,
	unsigned short tlv_data_len)
{
	i5_dm_network_topology_type *i5_topology = NULL;
	i5_dm_device_type *i5_iter_device = NULL;
	i5_dm_interface_type *pdmif = NULL;
	i5_dm_interface_type *i5_iter_ifr = NULL;
	i5_dm_bss_type *bss = NULL;
	wbd_info_t *info = NULL;
	wbd_ifr_item_t *ifr_vndr_info = NULL;
	wbd_cmd_vndr_intf_chan_info_t chan_info_msg;
	wbd_dfs_chan_info_t *dfs_chan_info = NULL;
	int ret = WBDE_OK;
	bool fronthaul_bss_configured = FALSE;
	bool free_chan_info = TRUE;

	WBD_ENTER();

	memset(&chan_info_msg, 0, sizeof(chan_info_msg));

	i5_topology = ieee1905_get_datamodel();
	if (!i5_topology) {
		WBD_ERROR("unexpected ... no topology from ieee1905_get_datamodel \n");
		goto end;
	}

	info = wbd_get_ginfo();
	/* DFS channel forced implementation for:
	 * - Topology operating in single channel mode
	 */
	if (WBD_MCHAN_ENAB(info->flags)) {
		WBD_INFO("Multi chan mode is ON, process interface's chan info"
			"only for single chan mode \n");
		goto end;
	}

	ret = wbd_tlv_decode_chan_info((void*)&chan_info_msg, tlv_data,
		tlv_data_len);
	WBD_ASSERT();

	WBD_DEBUG("chan info msg : mac["MACF"] band[%d] chan_info count[%d]\n",
		ETHERP_TO_MACF(&chan_info_msg.mac), chan_info_msg.band,
		chan_info_msg.chan_info->count);

	pdmif = wbd_ds_get_i5_interface(src_al_mac, (uchar*)&chan_info_msg.mac, &ret);
	WBD_ASSERT();

	WBD_DEBUG(" chan info rcvd for interface ["MACF"] \n", ETHERP_TO_MACF(&pdmif->InterfaceId));

	if (pdmif->band == WBD_BAND_LAN_2G) {
		WBD_ERROR("unlikely event..2G agent should not send chan info, exit \n");
		goto end;
	}
	/* dont process if fronthaul bss in not configured on interface */
	foreach_i5glist_item(bss, i5_dm_bss_type, pdmif->bss_list) {
		if (I5_IS_BSS_FRONTHAUL(bss->mapFlags)) {
			fronthaul_bss_configured = TRUE;
			break;
		}
	}
	if (!fronthaul_bss_configured) {
		WBD_DEBUG("No fronthaul interface configured, exit \n");
		goto end;
	}

	ifr_vndr_info = (wbd_ifr_item_t *)pdmif->vndr_data;

	/* wbd_ds_interface_init_cb not registered for controller, only used
	 * by Agents.
	 *
	 * For first chan info from agent's interface, neither vndr_info nor
	 * chan info exist, allocate vndr_info and use chan info from
	 * wbd_tlv_decode_chan_info and save it. Let vndr_info->chan_info
	 * point to chan_info_msg.chan_info.
	 *
	 * Dont free this chan_info_msg.chan_info, logic free this memory
	 * when same agent's interface again send chan info.
	 */
	if (!ifr_vndr_info) {
		ifr_vndr_info = (wbd_ifr_item_t *) wbd_malloc(sizeof(*ifr_vndr_info), &ret);
		WBD_ASSERT_MSG("Failed to allocate memory for ifr_vndr_data\n");
	}

	if (ifr_vndr_info->chan_info) {
		/* release earlier list and save new chan info */
		free(ifr_vndr_info->chan_info);
	}

	ifr_vndr_info->chan_info = chan_info_msg.chan_info;

	pdmif->vndr_data = ifr_vndr_info;
	free_chan_info = FALSE;

	WBD_DEBUG("pdmif band [%d] chan info [%p] \n", pdmif->band, ifr_vndr_info->chan_info);

	dfs_chan_info = wbd_master_get_common_chan_info(i5_topology, pdmif->band);
	if ((dfs_chan_info == NULL) || (dfs_chan_info->count == 0)) {
		WBD_DEBUG(" No common chan info or only one agent's chan info is present, exit\n");
		goto end;
	}

	WBD_DEBUG("send chan info, count[%d] to agent \n", dfs_chan_info->count);

	/* broadcast this dfs forced chan list to every brcm agent */
	foreach_i5glist_item(i5_iter_device, i5_dm_device_type, i5_topology->device_list) {
		if (!I5_IS_MULTIAP_AGENT(i5_iter_device->flags)) {
			continue;
		}
		foreach_i5glist_item(i5_iter_ifr, i5_dm_interface_type,
			i5_iter_device->interface_list) {

			if (!(i5_iter_ifr->isConfigured) ||
				!i5DmIsInterfaceWireless(i5_iter_ifr->MediaType)) {
				continue;
			}
			/* send to matching 5G band interfaces, not valid for 2g band */
			if ((pdmif->band) & (i5_iter_ifr->band)) {
				WBD_INFO("DFS forced channel list to: Device["MACF"] "
					"IFR["MACF"], chan[%d] band[%d]\n",
					ETHERP_TO_MACF(i5_iter_device->DeviceId),
					ETHERP_TO_MACF(i5_iter_ifr->InterfaceId),
					wf_chspec_ctlchan(i5_iter_ifr->chanspec),
					i5_iter_ifr->band);

				wbd_master_send_dfs_chan_info(dfs_chan_info, pdmif->band,
					i5_iter_device->DeviceId, i5_iter_ifr->InterfaceId);
			}
		}
	}
end:
	if (free_chan_info && (chan_info_msg.chan_info)) {
		/* May be chan info invalid or invalid interface
		 * free memory
		 */
		free(chan_info_msg.chan_info);
	}
	if (dfs_chan_info) {
		free(dfs_chan_info);
	}
	WBD_EXIT();
}

/* prepare common chan info from all agent's chan info */
static wbd_dfs_chan_info_t*
wbd_master_get_common_chan_info(i5_dm_network_topology_type *i5_topology, uint8 band)
{
	i5_dm_device_type *i5_iter_device = NULL;
	i5_dm_interface_type *i5_iter_ifr = NULL;
	wbd_ifr_item_t *ifr_vndr_info = NULL;
	wbd_dfs_chan_info_t *dfs_chan_info = NULL;
	uint8 slv_chnl = 0;
	uint8 setbitBuff[256];
	uint8 iter_setbitBuff[256];
	uint16 *pbuf = NULL;
	uint32 i = 0;
	uint32 *ptr_setbitBuff = NULL;
	uint32 *ptr_iter_setbitBuff = NULL;
	int ret = WBDE_OK;
	uint8 n_agent_found = 0;
	uint8 index = 0;

	WBD_ASSERT_ARG(i5_topology, WBDE_INV_ARG);

	pbuf = (uint16*)wbd_malloc(WBD_MAX_BUF_512, &ret);
	WBD_ASSERT();

	ptr_setbitBuff = (uint32*)&setbitBuff;
	ptr_iter_setbitBuff = (uint32*)&iter_setbitBuff;

	memset(&setbitBuff, 0xff, sizeof(setbitBuff));

	/* set bit in iter_setbitBuff corresponding to each channel number in chan info of
	 * each device's agent interface matching with input band. Compare every iteration
	 * bitmap with global bitbuff i.e. setbitBuff.
	 *
	 * pbuf holds the information of each channel bitmap present in channel info of
	 * agent. Every iteration validates new bitmap with existing bitmap present
	 * for the channel. If not same clear the channel bit from iter_setbitBuff.
	 *
	 */
	foreach_i5glist_item(i5_iter_device, i5_dm_device_type, i5_topology->device_list) {
		if (!I5_IS_MULTIAP_AGENT(i5_iter_device->flags)) {
			continue;
		}
		memset(&iter_setbitBuff, 0x00, sizeof(iter_setbitBuff));

		foreach_i5glist_item(i5_iter_ifr, i5_dm_interface_type,
			i5_iter_device->interface_list) {

			if (!i5DmIsInterfaceWireless(i5_iter_ifr->MediaType) ||
				(i5_iter_ifr->band != band) || !(i5_iter_ifr->isConfigured)) {
				continue;
			}
			ifr_vndr_info = (wbd_ifr_item_t *)i5_iter_ifr->vndr_data;

			if (!ifr_vndr_info || !(ifr_vndr_info->chan_info)) {
				WBD_INFO("Interface's vndr, chan info band[%d] not initialized\n",
					i5_iter_ifr->band);
				break; /* look for other device */
			}

			WBD_DEBUG("interface["MACF"] chan count[%d] \n",
				ETHERP_TO_MACF(&i5_iter_ifr->InterfaceId),
				ifr_vndr_info->chan_info->count);

			for (i = 0; i < ifr_vndr_info->chan_info->count; i++) {
				slv_chnl = ifr_vndr_info->chan_info->chinfo[i].channel;
				setbit(&iter_setbitBuff, slv_chnl);

				WBD_DEBUG("set bit in iter_bitbuff for channel[%d] \n", slv_chnl);

				if (n_agent_found == 0) {
					pbuf[slv_chnl] |=
						ifr_vndr_info->chan_info->chinfo[i].bitmap;

					WBD_DEBUG("chan[%d] first agent's bitmap[%x] \n", slv_chnl,
						ifr_vndr_info->chan_info->chinfo[i].bitmap);
				} else {
					/* compare already stored bitmap in pbuf[slv_chnl] with new
					 * bitmap. If different, ignore current channel
					 */
					uint16 bitmap = pbuf[slv_chnl];

					WBD_DEBUG("chan[%d] bitmap[%x] compare with existing"
						"bitmap [%x] \n", slv_chnl, bitmap,
						ifr_vndr_info->chan_info->chinfo[i].bitmap);

					if (bitmap != ifr_vndr_info->chan_info->chinfo[i].bitmap) {
						WBD_DEBUG("clear bit for channel[%d] as bitmap[%x]"
							"is different than previous bitmap\n",
							slv_chnl, bitmap);

						clrbit(&iter_setbitBuff, slv_chnl);
					}
				}
			}
			n_agent_found++;
		}
		for (i = 0; i < (sizeof(setbitBuff)/4); i++) {
			/* save agent iter_setbitBuff chan info in setbitBuff */
			ptr_setbitBuff[i] &= ptr_iter_setbitBuff[i];
		}
	}

	WBD_DEBUG("total agents found[%d] \n", n_agent_found);
	if (n_agent_found <= 1) {
		/* intersection is not possible, return */
		goto end;
	}

	WBD_DEBUG(" create chan info from setbit \n");
	/* use setbitBuff */
	dfs_chan_info = (wbd_dfs_chan_info_t*)wbd_malloc(sizeof(wbd_dfs_chan_info_t),
		&ret);
	WBD_ASSERT();

	for (i = 0; i <= MAX_5G_CONTROL_CHANNEL; i++) {
		if (!isset(&setbitBuff, i)) {
			continue;
		}
		dfs_chan_info->channel[index] = i;
		dfs_chan_info->count++;
		WBD_DEBUG("DFS common chan [%d] \n", dfs_chan_info->channel[index]);
		index++;
	}
	WBD_DEBUG("total chann info count [%d] \n", dfs_chan_info->count);
end:
	if (pbuf) {
		free(pbuf);
	}
	return dfs_chan_info;
}

#if defined(MULTIAPR2)
/* Process tunnel message based on message type */
void wbd_master_process_tunneled_msg(wbd_info_t *info, unsigned char *al_mac,
	ieee1905_tunnel_msg_t *msg)
{
	WBD_ENTER();

	switch (msg->payload_type) {
		case ieee1905_tunnel_msg_payload_assoc_rqst:
		case ieee1905_tunnel_msg_payload_re_assoc_rqst:
		case ieee1905_tunnel_msg_payload_btm_query:
		case ieee1905_tunnel_msg_payload_anqp_rqst:
		case ieee1905_tunnel_msg_payload_wnm_rqst:
		{
			/* TODO: how to use this information ... */
			WBD_INFO("payload type[%d] with tunnel msg from sta["MACF"]"
				"payload_len[%d] \n", msg->payload_type,
				ETHERP_TO_MACF(msg->source_mac), msg->payload_len);
		}
		break;

		default:
		{
			WBD_ERROR("unknown payload type[%d] with tunnel msg from sta["MACF"]"
				" with payload_len[%d] \n", msg->payload_type,
				ETHERP_TO_MACF(msg->source_mac), msg->payload_len);
		}
	}
	WBD_EXIT();
}

/* Processes Channel Scan Request by Controller */
void
wbd_master_process_channel_scan_rpt_cb(unsigned char *src_al_mac, time_t ts_chscan_rpt,
	ieee1905_chscan_report_msg *chscan_rpt)
{
	int ret = WBDE_OK;
	ieee1905_chscan_result_item *emt_p = NULL;
	char ssidbuf[SSID_FMT_BUF_LEN] = "";
	WBD_ENTER();

	/* Validate Message data */
	WBD_ASSERT_MSGDATA(chscan_rpt, "Channel Scan Report");

	/* Extract Number of Results */
	WBD_DEBUG("Received Channel Scan Report: Timestamp = [%lu] Num of Results = [%d] \n",
		(unsigned long)ts_chscan_rpt, chscan_rpt->num_of_results);

	/* Extract details for each radio */
	foreach_iglist_item(emt_p, ieee1905_chscan_result_item, chscan_rpt->chscan_result_list) {

		ieee1905_chscan_result_nbr_item *emt_nbr_p = NULL;

		/* Extract Radio Unique Identifier of a radio of the Multi-AP Agent */
		WBD_DEBUG("IFR["MACF"] ", ETHERP_TO_MACF(emt_p->radio_mac));

		/* Extract Operating Class */
		WBD_DEBUG("OpClass = [%d] ", emt_p->opclass);

		/* Extract Channel */
		WBD_DEBUG("Channel = [%d] ", emt_p->channel);

		/* Extract Scan Status Code */
		WBD_DEBUG("Scan Status Code = [%d] ", emt_p->scan_status_code);

		/* Check for Scan Status Code Success & Following Fields presence */
		if (emt_p->scan_status_code != MAP_CHSCAN_STATUS_SUCCESS) {
			continue;
		}

		/* Extract Timestamp Length */
		WBD_DEBUG("Timestamp Length = [%d] ", emt_p->timestamp_length);

		/* Extract Timestamp */
		if (emt_p->timestamp_length > 0) {
			WBD_DEBUG("Timestamp [%s] ", emt_p->timestamp);
		}

		/* Extract Utilization */
		WBD_DEBUG("Utilization = [%d] ", emt_p->utilization);

		/* Extract Noise */
		WBD_DEBUG("Noise = [%d] ", emt_p->noise);

		/* Extract AggregateScanDuration */
		WBD_DEBUG("AggregateScanDuration = [%d] ", emt_p->aggregate_scan_duration);

		/* Extract Channel Scan Request TLV Flags */
		WBD_DEBUG("ChScanResult_Flags = [0x%X] ", emt_p->chscan_result_flag);

		/* Extract Number Of Neighbors */
		WBD_DEBUG("Number Of Neighbors = [%d] \n", emt_p->num_of_neighbors);

		/* Extract details for each Neighbor */
		foreach_iglist_item(emt_nbr_p, ieee1905_chscan_result_nbr_item,
			emt_p->neighbor_list) {

			/* Extract BSSID indicated by the Neighboring BSS */
			WBD_DEBUG("NBR_BSSID["MACF"] ", ETHERP_TO_MACF(emt_nbr_p->nbr_bssid));

			/* Extract SSID of the Neighboring BSS */
			if (emt_nbr_p->nbr_ssid.SSID_len > 0) {
				wbd_wl_format_ssid(ssidbuf, emt_nbr_p->nbr_ssid.SSID,
					emt_nbr_p->nbr_ssid.SSID_len);
				WBD_DEBUG("NBR_SSID[%s] ", ssidbuf);
				WBD_DEBUG("NBR_SSID_Len = [%d] ", emt_nbr_p->nbr_ssid.SSID_len);
			}

			/* Extract Neighboring RSSI */
			WBD_DEBUG("Neighbor RSSI = [%d] ", WBD_RCPI_TO_RSSI(emt_nbr_p->nbr_rcpi));

			/* Extract ChannelBandwidth */
			if (emt_nbr_p->ch_bw_length > 0) {
				WBD_DEBUG("ch_bw [%s] ch_bw_length [%d] ",
					emt_nbr_p->ch_bw, emt_nbr_p->ch_bw_length);
			}

			/* Extract ChanUtil & StaCnt, if BSSLoad Element Present. Else omitted */
			if (emt_nbr_p->chscan_result_nbr_flag &
				MAP_CHSCAN_RES_NBR_BSSLOAD_PRESENT) {

				WBD_DEBUG("ChannelUtil = [%d] ", emt_nbr_p->channel_utilization);
				WBD_DEBUG("StationCount = [%d] \n", emt_nbr_p->station_count);
			}

			/* Extract Channel Scan  Result TLV Neighbor Flags */
			WBD_DEBUG("ChScanResult_NBR_Flags = [0x%X] \n",
				emt_nbr_p->chscan_result_nbr_flag);
		}
	}

end:
	WBD_EXIT();
}
#endif /* MULTIAPR2 */
