/*
 * WBD Steering Policy
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
 * $Id: wbd_master_control.c 782703 2020-01-02 06:23:34Z $
 */

#define TYPEDEF_FLOAT_T
#include <math.h>

#include "wbd_master_control.h"
#include "wbd_master_com_hdlr.h"
#ifdef BCM_APPEVENTD
#include "wbd_appeventd.h"
#endif /* BCM_APPEVENTD */
#include "wbd_tlv.h"

#define WBD_MASK_BIT7_TO_BIT5	0xE0
#define WBD_MASK_BIT7_TO_BIT6	0xC0

/* Is beacon report rssi recent enough to consider for tbss calculation? */
#define BEACON_RSSI_RECENT(bcn_tm, info) ((time(NULL)) <= \
	(bcn_tm + (info)->max.tm_bcn_rssi))

/* Information for the CLI command for vendor specific message */
typedef struct wbd_get_score_param {
	int rssi;
	int n_rssi;
	int n_hops;
	int total_sta_cnt;
	int assoc_cnt;
	int n_sta_cnt;
	int phy_rate;
	int n_phyrate;
	int bss_nss;
	int n_nss;
	int avg_tx_rate;
	int n_tx_rate;
	int node_score;
} wbd_get_score_param_t;

static wbd_tbss_wght_t predefined_tbss_wght[] = {
	/* w_rssi w_hops w_sta_cnt w_phyrate w_nss w_tx_rate flags */
	/* 0 : All weightage */
	{7, 8, 4, 6, 4, 4, 0x3F},
	/* 1 : High RSSI */
	{1, 0, 0, 0, 0, 0, 0x01},
	/* End */
	{0, 0, 0, 0, 0, 0, 0x0}
};

/* Predefined TBSS weightage for backhaul */
static wbd_tbss_wght_t predefined_tbss_wght_bh[] = {
	/* w_rssi w_hops w_sta_cnt w_phyrate w_nss w_tx_rate flags */
	/* 0 : All weightage except sta count */
	{7, 8, 0, 6, 4, 4, 0x3B},
	/* 1 : All weightage */
	{7, 8, 4, 6, 4, 4, 0x3F},
	/* End */
	{0, 0, 0, 0, 0, 0, 0x0}
};

static wbd_tbss_thld_t predefined_tbss_thld_2g[] = {
	/* t_rssi t_hops t_sta_cnt t_uplinkrate flags sof_algos */
	/* 0 : All Threshold and WBD_SOF_ALGO_BEST_NWS */
	{-62, 1, 10, 36, 0x7F, 0},
	/* 1 : All Threshold and WBD_SOF_ALGO_BEST_RSSI */
	{-62, 1, 10, 36, 0x7F, 1},
	/* End */
	{0, 0, 0, 0, 0, 0}
};

static wbd_tbss_thld_t predefined_tbss_thld_5g[] = {
	/* t_rssi t_hops t_sta_cnt t_uplinkrate flags sof_algos */
	/* 0 : All Threshold and WBD_SOF_ALGO_BEST_NWS */
	{-65, 1, 10, 36, 0x7F, 0},
	/* 1 : All Threshold and WBD_SOF_ALGO_BEST_RSSI */
	{-65, 1, 10, 36, 0x7F, 1},
	/* End */
	{0, 0, 0, 0, 0, 0}
};

/* Predfined TBSS threshold for backhaul */
static wbd_tbss_thld_t predefined_tbss_thld_bh[] = {
	/* t_rssi t_hops t_sta_cnt t_uplinkrate flags sof_algos */
	/* 0 : All Threshold and WBD_SOF_ALGO_BEST_NWS */
	{-70, 1, 10, 36, 0x7F, 0},
	/* 1 : All Threshold and WBD_SOF_ALGO_BEST_RSSI */
	{-70, 1, 10, 36, 0x7F, 1},
	/* End */
	{0, 0, 0, 0, 0, 0}
};

typedef i5_dm_bss_type* (*wbd_tbss_algo_t)(wbd_master_info_t *master_info, i5_dm_clients_type *sta,
	bool is_dual_band);

/* Selecting BSS for steering a STA based on normalized wightage and score(NWS) */
static i5_dm_bss_type*
wbd_tbss_algo_nws(wbd_master_info_t *master_info, i5_dm_clients_type *sta, bool is_dual_band);

/* Selecting BSS for steering a STA based on threshold values. Survival of Fittest(SoF) algo */
static i5_dm_bss_type*
wbd_tbss_algo_sof(wbd_master_info_t *master_info, i5_dm_clients_type *sta, bool is_dual_band);

static wbd_tbss_algo_t predefined_tbss_algo[] = {
	wbd_tbss_algo_sof,
	wbd_tbss_algo_nws,
	NULL
};

#define WBD_MAX_WEIGHTAGE \
	(sizeof(predefined_tbss_wght)/sizeof(wbd_tbss_wght_t)-1)
#define WBD_MAX_WEIGHTAGE_BH \
	(sizeof(predefined_tbss_wght_bh)/sizeof(wbd_tbss_wght_t)-1)
#define WBD_MAX_THRESHOLD_2G \
	(sizeof(predefined_tbss_thld_2g)/sizeof(wbd_tbss_thld_t)-1)
#define WBD_MAX_THRESHOLD_5G \
	(sizeof(predefined_tbss_thld_5g)/sizeof(wbd_tbss_thld_t)-1)
#define WBD_MAX_THRESHOLD_BH \
	(sizeof(predefined_tbss_thld_bh)/sizeof(wbd_tbss_thld_t)-1)
#define WBD_MAX_TARGET_BSS_ALGO	\
	(sizeof(predefined_tbss_algo)/sizeof(wbd_tbss_algo_t)-1)

/* Storing slave pointer and its score for NWS algo */
typedef struct wbd_tbss_score_item_t {
	int score;
	i5_dm_bss_type *i5_bss;
} wbd_tbss_score_item_t;

/* Storing score of all the slaves in NWS algo */
typedef struct wbd_tbss_scores_list {
	int size;
	int count;
	wbd_tbss_score_item_t score_item[1];
} wbd_tbss_scores_list_t;

/* Arguments to be passed to the backhaul optimization timer callback */
typedef struct wbd_bh_opt_timer_args {
	unsigned char sta_mac[MAC_ADDR_LEN];	/* If the timer called on backhaul STA join */
} wbd_bh_opt_timer_args_t;

/* Get the number of steering policies defined */
int
wbd_get_max_tbss_wght()
{
	return WBD_MAX_WEIGHTAGE;
}

/* Get the number of backhaul steering weightage policies defined */
int
wbd_get_max_tbss_wght_bh()
{
	return WBD_MAX_WEIGHTAGE_BH;
}

/* Get the number of threshold policies defined for 2G */
int
wbd_get_max_tbss_thld_2g()
{
	return WBD_MAX_THRESHOLD_2G;
}

/* Get the number of threshold policies defined for 5G */
int
wbd_get_max_tbss_thld_5g()
{
	return WBD_MAX_THRESHOLD_5G;
}

/* Get the number of threshold policies defined for backhaul */
int
wbd_get_max_tbss_thld_bh()
{
	return WBD_MAX_THRESHOLD_BH;
}

/* Get the finding target BSS algorithms */
int
wbd_get_max_tbss_algo()
{
	return WBD_MAX_TARGET_BSS_ALGO;
}

/* Get the weightage policy based on index */
wbd_tbss_wght_t*
wbd_get_tbss_wght(wbd_master_info_t *master_info)
{
	return &predefined_tbss_wght[master_info->tbss_info.tbss_wght];
}

/* Get the weightage policy based on index for backhaul */
wbd_tbss_wght_t*
wbd_get_tbss_wght_bh(wbd_master_info_t *master_info)
{
	return &predefined_tbss_wght_bh[master_info->tbss_info.tbss_wght_bh];
}

/* Get the threshold policy based on index for 2G */
wbd_tbss_thld_t*
wbd_get_tbss_thld_2g(wbd_master_info_t *master_info)
{
	return &predefined_tbss_thld_2g[master_info->tbss_info.tbss_thld_2g];
}

/* Get the threshold policy based on index for 5G */
wbd_tbss_thld_t*
wbd_get_tbss_thld_5g(wbd_master_info_t *master_info)
{
	return &predefined_tbss_thld_5g[master_info->tbss_info.tbss_thld_5g];
}

/* Get the threshold policy based on index for backhaul */
wbd_tbss_thld_t*
wbd_get_tbss_thld_bh(wbd_master_info_t *master_info)
{
	return &predefined_tbss_thld_bh[master_info->tbss_info.tbss_thld_bh];
}

/* get NSS of a interface  from radio caps */
static int
wbd_get_nss_from_radio_caps(i5_dm_interface_type *i5_ifr)
{
	int cur_nss = 0;

	if (i5_ifr->ApCaps.VHTCaps.Valid) {
		switch (i5_ifr->ApCaps.VHTCaps.CapsEx & WBD_MASK_BIT7_TO_BIT5) {
			case IEEE1905_AP_VHTCAP_TX_NSS_1:
				cur_nss = 1;
				break;
			case IEEE1905_AP_VHTCAP_TX_NSS_2:
				cur_nss = 2;
				break;
			case IEEE1905_AP_VHTCAP_TX_NSS_3:
				cur_nss = 3;
				break;
			case IEEE1905_AP_VHTCAP_TX_NSS_4:
				cur_nss = 4;
				break;
			case IEEE1905_AP_VHTCAP_TX_NSS_8:
				cur_nss = 8;
				break;
		}
	} else {
		switch (i5_ifr->ApCaps.HTCaps & WBD_MASK_BIT7_TO_BIT6) {
			case IEEE1905_AP_HTCAP_TX_NSS_1:
				cur_nss = 1;
				break;
			case IEEE1905_AP_HTCAP_TX_NSS_2:
				cur_nss = 2;
				break;
			case IEEE1905_AP_HTCAP_TX_NSS_3:
				cur_nss = 3;
				break;
			case IEEE1905_AP_HTCAP_TX_NSS_4:
				cur_nss = 4;
				break;
		}
	}

	return cur_nss;
}

/* get max NSS in all the interfaces of all devices in the topology */
static int
wbd_get_max_nss_in_topology()
{
	int max_nss = 0, cur_nss = 0;
	i5_dm_network_topology_type *i5_topology;
	i5_dm_device_type *i5_device;
	i5_dm_interface_type *i5_ifr;

	i5_topology = ieee1905_get_datamodel();
	/* For each device and interface */
	foreach_i5glist_item(i5_device, i5_dm_device_type, i5_topology->device_list) {

		/* Loop for only for Agent Devices */
		if (!I5_IS_MULTIAP_AGENT(i5_device->flags)) {
			continue;
		}

		foreach_i5glist_item(i5_ifr, i5_dm_interface_type, i5_device->interface_list) {
			/* Only for wireless interfaces get NSS */
			if (!i5DmIsInterfaceWireless(i5_ifr->MediaType)) {
				continue;
			}

			cur_nss = wbd_get_nss_from_radio_caps(i5_ifr);
			if (cur_nss > max_nss) {
				max_nss = cur_nss;
			}
		}
	}

	return max_nss;
}

/* Find the monitored STA in any of the BSS with valid RSSI in this device */
static wbd_monitor_sta_item_t*
wbd_tbss_get_monitor_sta_on_device(i5_dm_device_type *i5_device, unsigned char *mac,
	i5_dm_bss_type **i5_parent_bss, uint8 map_flags)
{
	int ret = WBDE_OK;
	i5_dm_interface_type *iter_ifr;
	i5_dm_bss_type *iter_bss;
	wbd_monitor_sta_item_t *monitor_sta = NULL;

	/* Loop for all the Interfaces in Device */
	foreach_i5glist_item(iter_ifr, i5_dm_interface_type, i5_device->interface_list) {

		/* Loop for all the BSSs in Interface */
		foreach_i5glist_item(iter_bss, i5_dm_bss_type, iter_ifr->bss_list) {

			if (I5_IS_BSS_GUEST(iter_bss->mapFlags)) {
				WBD_DEBUG("Skip Guest BSS\n");
				continue;
			}
			/* Only use matching BSS(Fronthaul or Backhaul) */
			if (!(map_flags & iter_bss->mapFlags)) {
				continue;
			}

			monitor_sta = wbd_ds_find_sta_in_bss_monitorlist(iter_bss,
				(struct ether_addr*)mac, &ret);
			if (monitor_sta) {
				/* Return if the RSSI is valid */
				if (WBD_RSSI_VALID(monitor_sta->rssi)) {
					if (i5_parent_bss) {
						*i5_parent_bss = iter_bss;
					}
					return monitor_sta;
				}
			}
		}
	}

	return NULL;
}

/* Find the valid beacon RSSI in any of the BSS in this device */
static wbd_monitor_sta_item_t*
wbd_tbss_get_beacon_rssi_on_device(i5_dm_device_type *i5_device, unsigned char *mac,
	i5_dm_bss_type **i5_parent_bss, uint8 map_flags)
{
	int ret = WBDE_OK;
	i5_dm_interface_type *iter_ifr;
	i5_dm_bss_type *iter_bss;
	wbd_monitor_sta_item_t *monitor_sta = NULL;

	/* Loop for all the Interfaces in Device */
	foreach_i5glist_item(iter_ifr, i5_dm_interface_type, i5_device->interface_list) {

		/* Loop for all the BSSs in Interface */
		foreach_i5glist_item(iter_bss, i5_dm_bss_type, iter_ifr->bss_list) {

			if (I5_IS_BSS_GUEST(iter_bss->mapFlags)) {
				WBD_DEBUG("Skip Guest BSS\n");
				continue;
			}

			/* Only use fronthaul BSS */
			if (!(map_flags & iter_bss->mapFlags)) {
				continue;
			}

			monitor_sta = wbd_ds_find_sta_in_bss_monitorlist(iter_bss,
				(struct ether_addr*)mac, &ret);
			if (monitor_sta) {
				/* Return if the RSSI is valid */
				if (WBD_RSSI_VALID(monitor_sta->bcn_rpt_rssi)) {
					if (i5_parent_bss) {
						*i5_parent_bss = iter_bss;
					}
					return monitor_sta;
				}
			}
		}
	}

	return NULL;
}

/* get the estimated RSSI from the interface where the actual RSSI is calculated */
static int
wbd_tbss_get_estimated_rssi(int rssi, int rssi_bcn, i5_dm_bss_type *cur_bss,
	i5_dm_bss_type *parent_bss, int adjust)
{
	int ret = WBDE_OK, est_rssi = 0, rssi_change = 0, base_txpwr = wbd_get_ginfo()->base_txpwr;
	i5_dm_interface_type *cur_ifr, *parent_ifr;

	WBD_ASSERT_ARG(cur_bss, WBDE_INV_ARG);
	WBD_ASSERT_ARG(parent_bss, WBDE_INV_ARG);

	/* Interface where the RSSI may not calculated */
	cur_ifr = (i5_dm_interface_type*)WBD_I5LL_PARENT(cur_bss);
	/* Interface where the RSSI may calculated */
	parent_ifr = (i5_dm_interface_type*)WBD_I5LL_PARENT(parent_bss);

	/* Check for validity of RSSI bcoz, sta_monitor may not have found the RSSI */
	if (!WBD_BKT_DONT_USE_BCN_RPT(wbd_get_ginfo()->wbd_master->flags) &&
		WBD_RSSI_VALID(rssi_bcn)) {
		est_rssi = rssi_bcn;
		/* Now adjust the RSSI based on the parent devices Tx Power Limit */
		est_rssi += cur_ifr->TxPowerLimit;
		est_rssi -= parent_ifr->TxPowerLimit;
	} else if (WBD_RSSI_VALID(rssi)) {
		if (adjust) {
			/* Add diff of Slave's Target Max Tx Power & Master's Lowest Tx Power.
			 * Not using it for SoF Algo because its checking rssi threshold.
			 * But used for NWS algo, to give advantage for high tx power radios
			 */
			est_rssi = rssi_change = rssi + (cur_ifr->TxPowerLimit - base_txpwr);
		} else {
			est_rssi = rssi;
		}
	} else {
		return 0;
	}

	/* Now apply Free space path loss correction since the radios are in different band */
	if (WBD_BAND_TYPE_LAN_2G(cur_ifr->band) && WBD_BAND_TYPE_LAN_5G(parent_ifr->band)) {
		est_rssi += wbd_get_ginfo()->crossband_rssi_est;
	} else if (WBD_BAND_TYPE_LAN_2G(parent_ifr->band) && WBD_BAND_TYPE_LAN_5G(cur_ifr->band)) {
		est_rssi -= wbd_get_ginfo()->crossband_rssi_est;
	}

	WBD_DEBUG("CUR_BSS["MACDBG"] PARENT_BSS["MACDBG"] RSSI[%d] RSSI_CHANGE[%d] RSSI_BCN[%d] "
		"EST_RSSI[%d] CUR_TXPWR[%d] PARENT_TXPWR[%d] BASE_TXPWR[%d]\n",
		MAC2STRDBG(cur_bss->BSSID), MAC2STRDBG(parent_bss->BSSID), rssi, rssi_change,
		rssi_bcn, est_rssi, cur_ifr->TxPowerLimit, parent_ifr->TxPowerLimit, base_txpwr);

end:
	return est_rssi;
}

/* get the estimated RSSI from the interface where the actual RSSI is calculated
 * based on monitor and beacon rssi
*/
static int
wbd_tbss_get_estimated_sta_rssi(wbd_monitor_sta_item_t *sta, int monitor_rssi, int bcn_rssi,
	time_t bcn_tm, i5_dm_bss_type *curr_bss, i5_dm_bss_type *monitor_bss,
	i5_dm_bss_type *bcn_bss, int adjust)
{
	int est_rssi = 0, est_rssi_bcn = 0, final_rssi;
	i5_dm_bss_type *bss = NULL;
	wbd_info_t *info = wbd_get_ginfo();

	if (!WBD_BKT_DONT_USE_BCN_RPT(info->wbd_master->flags) &&
		WBD_RSSI_VALID(sta->bcn_rpt_rssi) &&
		BEACON_RSSI_RECENT(sta->bcn_tm, info)) {
		final_rssi = sta->bcn_rpt_rssi;
		goto end;
	} else if (!WBD_BKT_DONT_USE_BCN_RPT(info->wbd_master->flags) &&
		BEACON_RSSI_RECENT(bcn_tm, info)) {
		/* Beacon rssi available for a different bss in the same device.
		 * Estimate the rssi of current bss based on that value.
		 */
		est_rssi_bcn = bcn_rssi;
		bss = bcn_bss;
	} else if (WBD_RSSI_VALID(sta->rssi)) {
		/* stamon rssi is present consider it */
		est_rssi = sta->rssi;
		bss = curr_bss;
	} else {
		/* Stamon rssi available for a different bss in the same device.
		 * Estimate the rssi of current bss based on that value.
		 */
		est_rssi = monitor_rssi;
		bss = monitor_bss;
	}
	final_rssi = wbd_tbss_get_estimated_rssi(est_rssi, est_rssi_bcn, curr_bss, bss, adjust);

end:
	WBD_DEBUG("STA: RSSI[%d] BCN_RSSI[%d] STA_BCN_TM[%lu]\n",
		sta->rssi, sta->bcn_rpt_rssi, (unsigned long)sta->bcn_tm);
	WBD_DEBUG("ESTIMATE: MONITOR_RSSI[%d] RSSI_BCN[%d] BEACON_TM[%lu] FINAL RSSI[%d]\n",
		monitor_rssi, bcn_rssi,	(unsigned long)bcn_tm, final_rssi);

	return final_rssi;
}

/* Select the best TBSS, if all the scores are same */
static i5_dm_bss_type*
wbd_master_find_target_slave_for_same_scores(wbd_tbss_scores_list_t *tbss_scores,
	i5_dm_bss_type *parent_bss, int current_adv_score, int max_score)
{
	i5_dm_device_type *i5_device;
	i5_dm_interface_type *i5_ifr;
	i5_dm_bss_type *target_bss = NULL, *ethernet_bss = NULL;
	wbd_tbss_score_item_t *iter_item = NULL;
	int idx;
	WBD_ENTER();

	/* iterate through each score to find out best possible TBSS */
	for (idx = 0; idx < tbss_scores->count; idx++) {
		iter_item = &tbss_scores->score_item[idx];
		i5_ifr = (i5_dm_interface_type*)WBD_I5LL_PARENT(iter_item->i5_bss);
		i5_device = (i5_dm_device_type*)WBD_I5LL_PARENT(i5_ifr);

		/* skip parent node, score is less than max_score or advantage score */
		if ((parent_bss == iter_item->i5_bss) || (iter_item->score < max_score) ||
			(iter_item->score < current_adv_score)) {
			WBD_DEBUG("Skipping node. parent_bss["MACDBG"] bss["MACDBG"] "
				"iter_item->score[%d] max_score[%d] current_adv_score[%d]\n",
				MAC2STRDBG(parent_bss->BSSID),
				MAC2STRDBG(iter_item->i5_bss->BSSID),
				iter_item->score, max_score, current_adv_score);
			continue;
		}
		/* Better to select the master bss as TBSS */
		if (I5_IS_CTRLAGENT(i5_device->flags)) {
			target_bss = iter_item->i5_bss;
			WBD_TBSS("Select master BSS["MACDBG"] Type[%s], score[%d]\n",
				MAC2STRDBG(iter_item->i5_bss->BSSID),
				"Master", iter_item->score);
			goto end;
		} else if (!I5_IS_DWDS(i5_device->flags)) {
			WBD_TBSS("Considering Ethernet BSS["MACDBG"] Type[%s], score[%d]\n",
				MAC2STRDBG(iter_item->i5_bss->BSSID),
				"Ethernet",
				iter_item->score);
			ethernet_bss = iter_item->i5_bss;
		} else {
			target_bss = iter_item->i5_bss;
		}
	}
	if (ethernet_bss)
		target_bss = ethernet_bss;

end:
	if (target_bss) {
		WBD_DEBUG("Selected TBSS ["MACDBG"]\n", MAC2STRDBG(target_bss->BSSID));
	}
	WBD_EXIT();
	return target_bss;
}

/* Update the phyrate to controller and normalized hop values for a device */
static void
wbd_master_update_device_tbss_value(i5_dm_device_type *i5_self_device, i5_dm_device_type *i5_device,
	wbd_device_item_t *device_vndr_data)
{
	uint8 eth_updated = 0, dedicated_updated = 0;
	uint16 min_phyrate = i5_device->macThroughPutCapacity;
	float depreciation = 1.0;
	i5_dm_device_type *i5_parent_device = i5_device;

	/* For all ethernet connection add 0.1 only once
	 * For each dedicated DWDS add 0.1 for first hop and 1 for each other hops
	 * For each non dedicated DWDS add 1
	 */
	/* TODO : For PLC and MOCA interfaces also can be changed here */
	while (i5_parent_device && (i5_parent_device != i5_self_device)) {
		if (I5_IS_DWDS(i5_parent_device->flags)) {
			if (I5_IS_DEDICATED_BK(i5_parent_device->flags)) {
				if (dedicated_updated) {
					depreciation += 1.0;
				} else {
					depreciation += 0.1;
					dedicated_updated = 1;
				}
			} else {
				depreciation += 1.0;
			}
		} else {
			if (!eth_updated) {
				depreciation += 0.1;
				eth_updated = 1;
			}
		}

		/* Update phyrate */
		if (min_phyrate > i5_parent_device->macThroughPutCapacity) {
			min_phyrate = i5_parent_device->macThroughPutCapacity;
		}
		i5_parent_device = i5_parent_device->parentDevice;
	}

	if (i5_parent_device) {
		device_vndr_data->normalized_hops = (uint8)((float)100/(float)depreciation);
		device_vndr_data->min_phyrate = min_phyrate;
		WBD_DEBUG("Device["MACDBG"] Controller["MACDBG"] n_hops[%d] Phyrate[%d]\n",
			MAC2STRDBG(i5_device->DeviceId), MAC2STRDBG(i5_self_device->DeviceId),
			device_vndr_data->normalized_hops, device_vndr_data->min_phyrate);
	} else {
		WBD_WARNING("Device["MACDBG"] Controller["MACDBG"] Parent Device is NULL\n",
			MAC2STRDBG(i5_device->DeviceId), MAC2STRDBG(i5_self_device->DeviceId));
	}
}

/* Update the phyrate to controller and normalized hop values for all the devices */
static void
wbd_master_update_all_devices_tbss_values()
{
	i5_dm_network_topology_type *i5_topology;
	i5_dm_device_type *i5_device;
	wbd_device_item_t *device_vndr_data; /* 1905 device item vendor data */
	WBD_ENTER();

	if ((i5_topology = ieee1905_get_datamodel()) == NULL) {
		goto end;
	}

	foreach_i5glist_item(i5_device, i5_dm_device_type, i5_topology->device_list) {

		device_vndr_data = (wbd_device_item_t*)i5_device->vndr_data;

		/* Loop for only for Agent Devices */
		if (!I5_IS_MULTIAP_AGENT(i5_device->flags) || !device_vndr_data) {
			continue;
		}

		if (I5_IS_CTRLAGENT(i5_device->flags)) {
			/* For controller agent phyrate is as it is and n_hops is 100
			 * TODO : For PLC and MOCA interfaces also can be changed here
			 */
			device_vndr_data->normalized_hops = 100;
			device_vndr_data->min_phyrate = i5_device->macThroughPutCapacity;
			WBD_DEBUG("Device["MACDBG"] Controller["MACDBG"] n_hops[%d] Phyrate[%d]\n",
				MAC2STRDBG(i5_device->DeviceId),
				MAC2STRDBG(i5_topology->selfDevice->DeviceId),
				100, device_vndr_data->min_phyrate);
			continue;
		}

		wbd_master_update_device_tbss_value(i5_topology->selfDevice,
			i5_device, device_vndr_data);
	}

end:
	WBD_EXIT();
}

/* Algorithm to get score of a node for a give STA */
static uint
wbd_master_get_score(wbd_master_info_t *master_info, i5_dm_bss_type *i5_bss,
	wbd_wl_sta_stats_t *stats, uint32 check_rule_mask, struct ether_addr *mac,
	wbd_get_score_param_t *tbss_param, wbd_tbss_wght_t *wght_cfg)
{
	int rssi = 0, n_rssi = 0;
	int n_hops = 0;
	int total_sta_cnt = 0, assoc_cnt = 0, n_sta_cnt = 0;
	int phyrate = 0, n_phyrate = 0;
	int n_nss = 0, bss_nss = 0;
	int n_tx_rate = 0;
	int node_score = 0;
	i5_dm_interface_type *i5_ifr;
	i5_dm_device_type *i5_device;
	wbd_bss_item_t *bss_item = (wbd_bss_item_t*)i5_bss->vndr_data;
	wbd_device_item_t *device_vndr_data;

	WBD_ENTER();

	if (bss_item == NULL) {
		WBD_WARNING("BSS["MACDBG"] NULL Vendor Data\n", MAC2STRDBG(i5_bss->BSSID));
		goto scoring;
	}

	i5_ifr = (i5_dm_interface_type*)WBD_I5LL_PARENT(i5_bss);
	i5_device = (i5_dm_device_type*)WBD_I5LL_PARENT(i5_ifr);
	device_vndr_data = (wbd_device_item_t*)i5_device->vndr_data;

	if (wght_cfg->flags & WBD_WGHT_FLAG_RSSI) {
		int max_rssi = master_info->tbss_info.tbss_max_rssi;
		int min_rssi = master_info->tbss_info.tbss_min_rssi;

		/* Check for validity of RSSI bcoz, sta_monitor may not have found the RSSI */
		if (WBD_RSSI_VALID(stats->rssi)) {
			rssi = stats->rssi;
		} else {
			WBD_DEBUG("For STA["MACF"] on BSS["MACDBG"] invalid rssi(%d)\n",
				ETHERP_TO_MACF(mac), MAC2STRDBG(i5_bss->BSSID),
				stats->rssi);
			goto scoring;
		}
		/* Find the normalized RSSI on a scale of 0 to 100.
		 * Below is a standard algorithm used for showing wireless signal strength by
		 * applications
		 */
		if (check_rule_mask == WBD_WGHT_FLAG_RSSI) {
			/* If only RSSI rule is defined. Then consider whole RSSI */
			n_rssi = 100 + rssi;
			if (n_rssi < 0)
				n_rssi = 0;
		} else if (rssi <= min_rssi) {
			n_rssi = 0;
		} else if (rssi >= max_rssi) {
			n_rssi = 100;
		} else {
			n_rssi = (rssi - min_rssi) * 100 / (max_rssi - min_rssi);
		}
	}

	if (wght_cfg->flags & WBD_WGHT_FLAG_HOPS) {
		n_hops = device_vndr_data->normalized_hops;
	}

	if (wght_cfg->flags & WBD_WGHT_FLAG_STACNT) {
		/* Find the normalized number of STAs associated to the AP
		 * This is the percentage of STAs this AP is holding out of all STAs in the blanket
		 */
		total_sta_cnt = master_info->blanket_client_count;
		assoc_cnt = i5_bss->ClientsNumberOfEntries;
		if (assoc_cnt <= master_info->tbss_info.tbss_stacnt_thld)
			n_sta_cnt = 100;
		else
			n_sta_cnt = (total_sta_cnt - assoc_cnt) * 100 / total_sta_cnt;
	}

	if (wght_cfg->flags & WBD_WGHT_FLAG_UPLINKRT) {
		/* Find the normalized phy rate between the repeater and root AP
		 * Find the AP with the best phyrate.
		 * Find how good this node is compared to the Ap with the best phyrate
		 */
		int max_phyrate = master_info->tbss_info.tbss_max_phyrate;
		int min_phyrate = master_info->tbss_info.tbss_min_phyrate;

		phyrate = device_vndr_data->min_phyrate;

		if (check_rule_mask == WBD_WGHT_FLAG_UPLINKRT) {
			/* If only phyrate rule is defined. Then consider whole phyrate */
			n_phyrate = phyrate;
		} else if (phyrate >= max_phyrate) {
			n_phyrate = 100;
		} else if (phyrate <= min_phyrate) {
			n_phyrate = 0;
		} else {
			n_phyrate = (phyrate - min_phyrate) * 100 / (max_phyrate - min_phyrate);
		}
	}

	if ((wght_cfg->flags & WBD_WGHT_FLAG_NSS) &&
		(master_info->tbss_info.max_nss)) {
			/* Find the normalized number of NSS of slave to serve the clients */
			bss_nss = wbd_get_nss_from_radio_caps(i5_ifr);
			n_nss = ((bss_nss * 100) / (master_info->tbss_info.max_nss));
	}

	if ((wght_cfg->flags & WBD_WGHT_FLAG_TX_RATE) &&
		(master_info->max_avg_tx_rate)) {
			/* Find the normalized number of txrate of BSS to serve the clients */
			n_tx_rate = ((bss_item->avg_tx_rate * 100)/
				(master_info->max_avg_tx_rate));
	}

scoring:
	/* Calculate the score for the node */
	node_score = ((wght_cfg->w_rssi * n_rssi)+
		(wght_cfg->w_hops * n_hops)+
		(wght_cfg->w_sta_cnt * n_sta_cnt)+
		(wght_cfg->w_phyrate * n_phyrate)+
		(wght_cfg->w_nss * n_nss)+
		(wght_cfg->w_tx_rate * n_tx_rate));
	WBD_TBSS("For STA["MACF"] on Node["MACDBG"]: weight_flags=0x%x, rssi=%d, "
		"n_rssi=%d, n_hops=%d, tot_sta=%d, assoc_cnt=%d, n_sta_cnt=%d, "
		"phyrate=%d, n_phyrate=%d, bss_nss=%d, max_nss=%d, n_nss=%d, slave_txrate=%d, "
		"max_txrate=%d, n_tx_rate=%d, score=%d\n", ETHERP_TO_MACF(mac),
		MAC2STRDBG(i5_bss->BSSID), wght_cfg->flags,
		rssi, n_rssi, n_hops, total_sta_cnt, assoc_cnt, n_sta_cnt, phyrate,
		n_phyrate, bss_nss, master_info->tbss_info.max_nss,
		n_nss, bss_item->avg_tx_rate, master_info->max_avg_tx_rate,
		n_tx_rate, node_score);
	if (node_score > tbss_param->node_score) {
		tbss_param->rssi = rssi;
		tbss_param->n_rssi = n_rssi;
		tbss_param->n_hops = n_hops;
		tbss_param->total_sta_cnt = total_sta_cnt;
		tbss_param->assoc_cnt = assoc_cnt;
		tbss_param->n_sta_cnt = n_sta_cnt;
		tbss_param->phy_rate = phyrate;
		tbss_param->n_phyrate = n_phyrate;
		tbss_param->bss_nss = bss_nss;
		tbss_param->n_nss = n_nss;
		tbss_param->avg_tx_rate = bss_item->avg_tx_rate;
		tbss_param->n_tx_rate = n_tx_rate;
		tbss_param->node_score = node_score;
	}

	WBD_EXIT();

	return node_score;
}

/* Selecting BSS for steering a STA based on normalized wightage and score(NWS) */
i5_dm_bss_type*
wbd_tbss_algo_nws(wbd_master_info_t *master_info, i5_dm_clients_type *weak_sta, bool is_dual_band)
{
	uint8 cur_band, parent_band, band_steer, map_flags = IEEE1905_MAP_FLAG_FRONTHAUL;
	i5_dm_network_topology_type *i5_topology;
	i5_dm_device_type *i5_device, *parent_device;
	i5_dm_interface_type *i5_ifr, *parent_ifr;
	i5_dm_bss_type *i5_bss, *target_bss = (i5_dm_bss_type*)WBD_I5LL_PARENT(weak_sta);
	i5_dm_bss_type *parent_bss;
	wbd_assoc_sta_item_t *assoc_sta;
	wbd_monitor_sta_item_t *sta = NULL, *bcn_sta = NULL;
	wbd_wl_sta_stats_t stats;
	wbd_tbss_scores_list_t *tbss_scores = NULL;
	int ret = WBDE_OK, bss_count;
	int max_score = 0, current_score = 0;
	int adv_thld = master_info->tbss_info.nws_adv_thld;
	uint32 check_rule_mask = 0;
	int buflen, i = 0;
	char logmsg[WBD_MAX_BUF_512] = {0}, timestamp[WBD_MAX_BUF_32] = {0};
	wbd_get_score_param_t tbss_param;
	wbd_tbss_wght_t *wght_cfg = NULL;

	WBD_ENTER();

	if (weak_sta->vndr_data == NULL) {
		WBD_WARNING("STA["MACDBG"] NULL Vendor Data\n", MAC2STRDBG(weak_sta->mac));
		goto end;
	}

	/* Now allocate the structure to hold all scores */
	bss_count = master_info->blanket_bss_count;
	buflen = (sizeof(wbd_tbss_scores_list_t) +
		(sizeof(wbd_tbss_score_item_t) * (bss_count - 1)));
	tbss_scores = (wbd_tbss_scores_list_t*)wbd_malloc(buflen, &ret);
	WBD_ASSERT_MSG("STA["MACDBG"] Failed to allocate memory for tbss_scores\n",
		MAC2STRDBG(weak_sta->mac));
	tbss_scores->size = bss_count;
	/* Reset the nss */
	master_info->tbss_info.max_nss = wbd_get_max_nss_in_topology();

	i5_topology = ieee1905_get_datamodel();

	parent_bss = (i5_dm_bss_type*)WBD_I5LL_PARENT(weak_sta);
	parent_ifr = (i5_dm_interface_type*)WBD_I5LL_PARENT(parent_bss);
	parent_device = (i5_dm_device_type*)WBD_I5LL_PARENT(parent_ifr);

	/* Get the band of the STA */
	parent_band = WBD_BAND_FROM_CHANNEL(wf_chspec_ctlchan(parent_ifr->chanspec));

	/* If 0 dont do band steering within the same device of the associated STA */
	band_steer = (WBD_BAND_STEER_ENAB((master_info->parent->parent->flags))) ? 1 : 0;

	assoc_sta = (wbd_assoc_sta_item_t*)weak_sta->vndr_data;

	/* Update the MAP flag for backhaul STA and choose the weightage configuration based on
	 * STA type
	 */
	if (I5_CLIENT_IS_BSTA(weak_sta)) {
		map_flags = IEEE1905_MAP_FLAG_BACKHAUL;
		wght_cfg = &master_info->tbss_info.wght_cfg_bh;
	} else {
		wght_cfg = &master_info->tbss_info.wght_cfg;
	}

	/* Get which rules are selected */
	check_rule_mask = ((wght_cfg->flags) & (WBD_WGHT_FLAG_ALL));

	WBD_TBSS("START NWS bss_count[%d] Max_NSS[%d] check_rule_mask[%x]\n", bss_count,
		master_info->tbss_info.max_nss, check_rule_mask);

	/* Reset the target bsss parameter */
	memset(&tbss_param, 0, sizeof(tbss_param));

	/* Traverse BSS List to check the best BSS to steer */
	foreach_i5glist_item(i5_device, i5_dm_device_type, i5_topology->device_list) {

		int est_rssi = 0, est_rssi_bcn = 0;
		time_t bcn_tm = 0;
		i5_dm_bss_type *parent_monitor_bss = NULL, *bcn_monitor_bss = NULL;

		/* Loop for only for Agent Devices */
		if (!I5_IS_MULTIAP_AGENT(i5_device->flags)) {
			continue;
		}

		/* If the STA is backhaul STA, dont consider if the STA MAC address is part of the
		 * interface list in this device
		 */
		if (I5_CLIENT_IS_BSTA(weak_sta)) {
			if (wbd_ds_get_i5_ifr_in_device(i5_device, weak_sta->mac, &ret)) {
				WBD_DEBUG("Device["MACDBG"] STA["MACDBG"] is backhaul STA and its "
					"from same device so, skip\n",
					MAC2STRDBG(i5_device->DeviceId), MAC2STRDBG(weak_sta->mac));
				continue;
			}
		}

		/* If the device is parent device, then the rssi will be from associated sta */
		if (i5_device == parent_device) {
			est_rssi = WBD_RCPI_TO_RSSI(weak_sta->link_metric.rcpi);
			if (!WBD_BKT_DONT_USE_BCN_RPT(master_info->parent->flags) &&
				BEACON_RSSI_RECENT(assoc_sta->stats.bcn_tm, wbd_get_ginfo())) {
				bcn_tm = assoc_sta->stats.bcn_tm;
				est_rssi_bcn = assoc_sta->stats.bcn_rpt_rssi;
			}
			parent_monitor_bss = parent_bss;
			bcn_monitor_bss = parent_bss;
		} else {
			sta = wbd_tbss_get_monitor_sta_on_device(i5_device, weak_sta->mac,
				&parent_monitor_bss, map_flags);
			bcn_sta = wbd_tbss_get_beacon_rssi_on_device(i5_device, weak_sta->mac,
				&bcn_monitor_bss, map_flags);
			if (sta || bcn_sta) {
				if (sta) {
					est_rssi = sta->rssi;
				}
				if (bcn_sta) {
					bcn_tm = bcn_sta->bcn_tm;
					est_rssi_bcn = bcn_sta->bcn_rpt_rssi;
				}
			} else {
				WBD_WARNING("STA["MACDBG"] invalid Beacon or monitor rssi in "
					"Monitorlist on Device["MACDBG"]\n",
					MAC2STRDBG(weak_sta->mac), MAC2STRDBG(i5_device->DeviceId));
				continue;
			}
		}

		foreach_i5glist_item(i5_ifr, i5_dm_interface_type, i5_device->interface_list) {

			if (!i5DmIsInterfaceWireless(i5_ifr->MediaType) ||
				!i5_ifr->BSSNumberOfEntries) {
				continue;
			}

			/* Get the current band of the interface */
			cur_band = WBD_BAND_FROM_CHANNEL(wf_chspec_ctlchan(i5_ifr->chanspec));
			/* If the STA doesnt support Dual band and band of the STA is not the
			 * band of interface, then skip it
			 */
			if (!is_dual_band && cur_band != parent_band) {
				WBD_DEBUG("STA["MACDBG"] is not Dual Band so. "
					"skip IFR["MACDBG"] in Device["MACDBG"]\n",
					MAC2STRDBG(weak_sta->mac), MAC2STRDBG(i5_ifr->InterfaceId),
					MAC2STRDBG(i5_device->DeviceId));
				continue;
			}

			/* Dont do steering between bands on the same device where the STA
			 * is associated if the band_steer is disabled
			 */
			if (!band_steer && (i5_device == parent_device) &&
				(cur_band != parent_band)) {
				WBD_INFO("Device["MACDBG"] ParentDevice["MACDBG"] "
					"cur_band[%d] parent_band[%d] "
					"No band steering of STA["MACDBG"]\n",
					MAC2STRDBG(i5_device->DeviceId),
					MAC2STRDBG(parent_device->DeviceId),
					cur_band, parent_band,
					MAC2STRDBG(weak_sta->mac));
				continue;
			}

			foreach_i5glist_item(i5_bss, i5_dm_bss_type, i5_ifr->bss_list) {
				int score = 0;

				if (I5_IS_BSS_GUEST(i5_bss->mapFlags)) {
					continue;
				}
				/* Only use matching BSS(Fronthaul or Backhaul) */
				if (!(map_flags & i5_bss->mapFlags)) {
					continue;
				}
#if defined(MULTIAPR2)
				if (i5_bss->assoc_allowance_status == 0x00) {
					WBD_DEBUG("no more assoc req on bss["MACF"] \n",
						ETHERP_TO_MACF(i5_bss->BSSID));
					/* no more association request on this bss, skip */
					continue;
				}
#endif /* MULTIAPR2 */
				memset(&stats, 0, sizeof(stats));

				/* For slave to which this STA is associated */
				if (i5_bss == parent_bss) {
					if (assoc_sta->fail_cnt != 0)
						continue;
					/* Get the score for the STA in which its associated and
					 * store it as current score
					 */
					stats.rssi = wbd_tbss_get_estimated_rssi(est_rssi,
						est_rssi_bcn, i5_bss, parent_bss, 1);
					score = wbd_master_get_score(master_info, i5_bss,
						&stats, check_rule_mask,
						(struct ether_addr*)&weak_sta->mac, &tbss_param,
						wght_cfg);
					current_score = assoc_sta->score = score;
				} else {
					sta = wbd_ds_find_sta_in_bss_monitorlist(i5_bss,
						(struct ether_addr*)weak_sta->mac, &ret);
					if (sta) {
						/* If the fail count is not 0, its not eligible */
						if (sta->fail_cnt != 0)
							continue;

						/* Get the score for the STA monitored
						 * by this bss.
						 */
						memset(&stats, 0, sizeof(stats));
						stats.rssi = wbd_tbss_get_estimated_sta_rssi(
							sta, est_rssi, est_rssi_bcn,
							bcn_tm, i5_bss, parent_monitor_bss,
							bcn_monitor_bss, 1);
						score = wbd_master_get_score(master_info, i5_bss,
							&stats, check_rule_mask,
							&sta->sta_mac, &tbss_param,
							wght_cfg);
						sta->score = score;
					} else {
						WBD_WARNING("STA["MACDBG"] Not Found in "
							"Monitorlist of Slave["MACDBG"]\n",
							MAC2STRDBG(weak_sta->mac),
							MAC2STRDBG(i5_bss->BSSID));
						continue;
					}
				}
				/* Store the score and bss pointer for finding the best TBSS
				 * incase of more than 1 bss having same scores. Before adding
				 * check the size of the memory allocated.
				 */
				if (tbss_scores->size > tbss_scores->count) {
					tbss_scores->score_item[tbss_scores->count].score = score;
					tbss_scores->score_item[tbss_scores->count].i5_bss = i5_bss;
					tbss_scores->count++;
				}

				/* Find the best AP and keep it's score. Also note the score of
				 * the current AP
				 */
				if (score > max_score) {
					target_bss = i5_bss;
					max_score = score;
				}
			}
		}
	}

	/* Now check for target_bss.
	 * 1. If the target_bss is not the BSS to which the STA is associated and the score is
	 * greater than the threshold
	 * 2. If the scores are same for two or more bss find the best among.
	 */
	if ((target_bss != parent_bss) &&
		(max_score >= (current_score + adv_thld))) {
		goto end;
	} else if (tbss_scores->count > 1) {
		/* It will come here if there are nodes with same scores */
		target_bss = wbd_master_find_target_slave_for_same_scores(tbss_scores,
			parent_bss, (current_score + adv_thld), max_score);
	} else {
		target_bss = NULL;
	}

end:
	if (tbss_scores)
		free(tbss_scores);
	if (target_bss) {
		WBD_TBSS("For STA["MACDBG"] found Target BSS ["MACDBG"] score[%d]\n",
			MAC2STRDBG(weak_sta->mac),
			MAC2STRDBG(target_bss->BSSID), max_score);
		/* Create and store  score for the node in tbss log. */
		snprintf(logmsg, sizeof(logmsg), CLI_CMD_LOGS_FOUND_TBSS_ALGO_NWS,
			wbd_get_formated_local_time(timestamp, sizeof(timestamp)),
			MAC2STRDBG(weak_sta->mac), MAC2STRDBG(target_bss->BSSID),
			wght_cfg ? wght_cfg->flags : 0, tbss_param.rssi, tbss_param.n_rssi,
			tbss_param.n_hops, tbss_param.total_sta_cnt, tbss_param.assoc_cnt,
			tbss_param.n_sta_cnt, tbss_param.phy_rate, tbss_param.n_phyrate,
			tbss_param.bss_nss, master_info->tbss_info.max_nss, tbss_param.n_nss,
			tbss_param.avg_tx_rate, master_info->max_avg_tx_rate,
			tbss_param.n_tx_rate, tbss_param.node_score);
		for (i = 0; i <= strlen(logmsg); i += (WBD_MAX_BUF_128 - 1)) {
			wbd_ds_add_logs_in_master(master_info->parent, logmsg + i);
		}
	}

	WBD_EXIT();
	return target_bss;
}

/* Get the fail count for the STA in a slave. The fail count will be incremented if its
 * less than the threshold level
 */
static int
wbd_master_get_threshold_fail_count(wbd_master_info_t *master_info, i5_dm_bss_type *i5_bss,
	wbd_wl_sta_stats_t *stats, struct ether_addr *mac, uint8 is_bsta)
{
	int fail_cnt = 0, rssi = 0;
	int sta_cnt = 0, sta_tx_rate = 0;
	int phyrate = 0;
	int txrate = 0;
	i5_dm_interface_type *i5_ifr;
	i5_dm_device_type *i5_device;
	wbd_bss_item_t *bss_item = (wbd_bss_item_t*)i5_bss->vndr_data;
	wbd_tbss_thld_t *thld_cfg = NULL;
	wbd_device_item_t *device_vndr_data;
	uint32 tx_failures = 0;
	int band;

	UNUSED_PARAMETER(phyrate);

	WBD_ENTER();

	if (bss_item == NULL) {
		WBD_WARNING("BSS["MACDBG"] NULL Vendor Data\n", MAC2STRDBG(i5_bss->BSSID));
		fail_cnt = 1;
		goto end;
	}

	i5_ifr = (i5_dm_interface_type*)WBD_I5LL_PARENT(i5_bss);
	i5_device = (i5_dm_device_type*)WBD_I5LL_PARENT(i5_ifr);
	device_vndr_data = (wbd_device_item_t*)i5_device->vndr_data;

	if (is_bsta) {
		thld_cfg = &master_info->tbss_info.thld_cfg_bh;
	} else {
		band = WBD_BAND_FROM_1905_BAND(i5_ifr->band);
		if (band & WBD_BAND_LAN_2G) {
			thld_cfg = &master_info->tbss_info.thld_cfg_2g;
		} else {
			thld_cfg = &master_info->tbss_info.thld_cfg_5g;
		}
	}

	if (thld_cfg->flags & WBD_THLD_FLAG_RSSI) {
		/* Check for validity of RSSI bcoz, sta_monitor may not have found the RSSI */
		if (WBD_RSSI_VALID(stats->rssi)) {
			rssi = stats->rssi;
		} else {
			fail_cnt++;
			WBD_DEBUG("Device["MACDBG"]. RSSI Invalid. Falling out.\n",
				MAC2STRDBG(i5_device->DeviceId));
			goto end;
		}

		if (rssi < thld_cfg->t_rssi)
			fail_cnt++;
	}

	if (thld_cfg->flags & WBD_THLD_FLAG_STACNT) {
		sta_cnt = i5_bss->ClientsNumberOfEntries;
		/* If already the maximum STAs are connected, don't choose this as TBSS */
		if (sta_cnt >= thld_cfg->t_sta_cnt)
			fail_cnt++;
	}

	if (thld_cfg->flags & WBD_THLD_FLAG_UPLINKRT) {
		phyrate = device_vndr_data->min_phyrate;
		if (phyrate < thld_cfg->t_uplinkrate)
			fail_cnt++;
	}

	if (thld_cfg->flags & WBD_THLD_FLAG_MAX_TXRATE) {
		/* slave's max tx_rate can change dynamically, gets update in STA_REPORT
		 * from slave at every STA_REPORT interval, It may be possible we are
		 * here before STA_REPORT from any slave in response to Subscribe
		 * command. In case of Invalid tx_rate i.e. Zero, better to exit and
		 * retry in next timer callback
		 */
		txrate = bss_item->max_tx_rate;
		if (txrate < master_info->tbss_info.thld_sta.t_tx_rate)
			fail_cnt++;
	}

	/* TxRate and TxFailures comparison's are only applicable to associated slave,
	 * for other slave, these comparison's will be phased out.
	 */
	if (thld_cfg->flags & WBD_THLD_FLAG_STA_TXRT) {
		sta_tx_rate = (stats->tx_rate < WBD_TBSS_MAX_TXRATE_BOUNDARY) ? stats->tx_rate : 0;
		/* Compare TxRate */
		if ((sta_tx_rate > WBD_TBSS_MIN_TXRATE_BOUNDARY) &&
			(sta_tx_rate < master_info->tbss_info.thld_sta.t_tx_rate)) {
			fail_cnt++;
		}
	}

	if (thld_cfg->flags & WBD_THLD_FLAG_STA_TXFAIL) {
		time_t gap, now = time(NULL);

		/* Ignore readings older than 10 seconds */
		gap = now - stats->active;
		if (stats->active > 0 && gap < WBD_METRIC_EXPIRY_TIME) {
			if (stats->tx_tot_failures >= stats->old_tx_tot_failures) {
				tx_failures = stats->tx_tot_failures -
					stats->old_tx_tot_failures;
			} else {
				/* Handle counter roll over */
				tx_failures = (-1) - stats->old_tx_tot_failures +
					stats->tx_tot_failures + 1;
			}
		}

		/* Compare TxFailures count. */
		if (tx_failures >
			master_info->tbss_info.thld_sta.t_tx_failures) {
			fail_cnt++;
		}
	}

end:
	WBD_TBSS("BKTNAME[%s] For STA["MACF"] on Slave["MACDBG"]. (value/threshold/enable) "
		"rssi (%d/%d/%d) sta_cnt (%d/%d/%d) uplink (%d/%d/%d) txrate (%d/%d/%d) "
		"sta_txrate (%d/%d/%d) sta_txfail (%d/%d/%d) cfgflags=0x%X Fail count = %d\n",
		master_info->bkt_name, ETHERP_TO_MACF(mac), MAC2STRDBG(i5_bss->BSSID),
		rssi, thld_cfg->t_rssi, (thld_cfg->flags & WBD_THLD_FLAG_RSSI),
		sta_cnt, thld_cfg->t_sta_cnt, (thld_cfg->flags & WBD_THLD_FLAG_STACNT),
		phyrate, thld_cfg->t_uplinkrate, (thld_cfg->flags & WBD_THLD_FLAG_UPLINKRT),
		txrate, master_info->tbss_info.thld_sta.t_tx_rate,
		(thld_cfg->flags & WBD_THLD_FLAG_MAX_TXRATE),
		sta_tx_rate, master_info->tbss_info.thld_sta.t_tx_rate,
		(thld_cfg->flags & WBD_THLD_FLAG_STA_TXRT),
		tx_failures, master_info->tbss_info.thld_sta.t_tx_failures,
		(thld_cfg->flags & WBD_THLD_FLAG_STA_TXFAIL),
		thld_cfg->flags, fail_cnt);

	WBD_EXIT();

	return fail_cnt;
}

/* Find the node with best RSSI in bss list of all devices whose fail_cnt is 0 */
i5_dm_bss_type*
wbd_master_find_target_slave_with_best_rssi(wbd_master_info_t *master_info,
	i5_dm_clients_type *weak_sta, bool is_dual_band)
{
	uint8 cur_band, parent_band, band_steer, map_flags = IEEE1905_MAP_FLAG_FRONTHAUL;
	i5_dm_network_topology_type *i5_topology;
	i5_dm_device_type *i5_device, *parent_device;
	i5_dm_interface_type *i5_ifr, *parent_ifr;
	i5_dm_bss_type *i5_bss, *target_bss = NULL, *parent_bss;
	wbd_assoc_sta_item_t *assoc_sta;
	wbd_monitor_sta_item_t *sta = NULL, *bcn_sta = NULL;
	int best_rssi = -125, rssi, ret = WBDE_OK;
	char logmsg[WBD_MAX_BUF_128] = {0}, timestamp[WBD_MAX_BUF_32] = {0};

	WBD_ENTER();

	if (weak_sta->vndr_data == NULL) {
		WBD_WARNING("STA["MACDBG"] NULL Vendor Data\n", MAC2STRDBG(weak_sta->mac));
		goto end;
	}

	parent_bss = (i5_dm_bss_type*)WBD_I5LL_PARENT(weak_sta);
	parent_ifr = (i5_dm_interface_type*)WBD_I5LL_PARENT(parent_bss);
	parent_device = (i5_dm_device_type*)WBD_I5LL_PARENT(parent_ifr);

	/* Get the band of the STA */
	parent_band = WBD_BAND_FROM_CHANNEL(wf_chspec_ctlchan(parent_ifr->chanspec));

	/* If 0 dont do band steering within the same device of the associated STA */
	band_steer = (WBD_BAND_STEER_ENAB((master_info->parent->parent->flags))) ? 1 : 0;

	assoc_sta = (wbd_assoc_sta_item_t*)weak_sta->vndr_data;
	/* Initialize the RSSI with the current slave */
	if (assoc_sta && WBD_RSSI_VALID(assoc_sta->stats.bcn_rpt_rssi)) {
		best_rssi = assoc_sta->stats.bcn_rpt_rssi;
	} else {
		best_rssi = WBD_RCPI_TO_RSSI(weak_sta->link_metric.rcpi);
	}

	if (I5_CLIENT_IS_BSTA(weak_sta)) {
		map_flags = IEEE1905_MAP_FLAG_BACKHAUL;
	}

	i5_topology = ieee1905_get_datamodel();
	/* Find tbss with best rssi. */
	foreach_i5glist_item(i5_device, i5_dm_device_type, i5_topology->device_list) {

		int est_rssi = 0, est_rssi_bcn = 0;
		time_t bcn_tm = 0;
		i5_dm_bss_type *parent_monitor_bss = NULL, *bcn_monitor_bss = NULL;

		/* Loop for only for Agent Devices */
		if (!I5_IS_MULTIAP_AGENT(i5_device->flags)) {
			continue;
		}

		/* If the STA is backhaul STA, dont consider if the STA MAC address is part of the
		 * interface list in this device
		 */
		if (I5_CLIENT_IS_BSTA(weak_sta)) {
			if (wbd_ds_get_i5_ifr_in_device(i5_device, weak_sta->mac, &ret)) {
				WBD_DEBUG("Device["MACDBG"] STA["MACDBG"] is backhaul STA and its "
					"from same device so, skip\n",
					MAC2STRDBG(i5_device->DeviceId), MAC2STRDBG(weak_sta->mac));
				continue;
			}
		}

		/* We have got the RSSI of the associated STA and as we are not steering the
		 * STA between bands on the same device where the STA is associated
		 * (if band_steer is disabled), we can skip if the device is parent device
		 */
		if (!band_steer && (i5_device == parent_device)) {
			WBD_INFO("Device["MACDBG"] ParentDevice["MACDBG"] "
				"No band steering of STA["MACDBG"]\n",
				MAC2STRDBG(i5_device->DeviceId),
				MAC2STRDBG(parent_device->DeviceId),
				MAC2STRDBG(weak_sta->mac));
			continue;
		}

		/* Get monitored RSSI from the other device */
		sta = wbd_tbss_get_monitor_sta_on_device(i5_device, weak_sta->mac,
			&parent_monitor_bss, map_flags);
		bcn_sta = wbd_tbss_get_beacon_rssi_on_device(i5_device, weak_sta->mac,
			&bcn_monitor_bss, map_flags);
		if (sta || bcn_sta) {
			if (sta) {
				est_rssi = sta->rssi;
			}
			if (bcn_sta) {
				bcn_tm = bcn_sta->bcn_tm;
				est_rssi_bcn = bcn_sta->bcn_rpt_rssi;
			}
		} else {
			WBD_WARNING("STA["MACDBG"] invalid Beacon or monitor rssi in "
				"Monitorlist on Device["MACDBG"]\n",
				MAC2STRDBG(weak_sta->mac), MAC2STRDBG(i5_device->DeviceId));
			continue;
		}

		foreach_i5glist_item(i5_ifr, i5_dm_interface_type, i5_device->interface_list) {

			if (!i5DmIsInterfaceWireless(i5_ifr->MediaType) ||
				!i5_ifr->BSSNumberOfEntries) {
				continue;
			}

			/* Get the current band of the interface */
			cur_band = WBD_BAND_FROM_CHANNEL(wf_chspec_ctlchan(i5_ifr->chanspec));
			/* If the STA doesnt support Dual band and band of the STA is not the
			 * band of interface, then skip it
			 */
			if (!is_dual_band && cur_band != parent_band) {
				WBD_DEBUG("STA["MACDBG"] is not Dual Band so. "
					"skip IFR["MACDBG"] in Device["MACDBG"]\n",
					MAC2STRDBG(weak_sta->mac), MAC2STRDBG(i5_ifr->InterfaceId),
					MAC2STRDBG(i5_device->DeviceId));
				continue;
			}

			foreach_i5glist_item(i5_bss, i5_dm_bss_type, i5_ifr->bss_list) {

				if (I5_IS_BSS_GUEST(i5_bss->mapFlags)) {
					continue;
				}
				/* Only use fronthaul BSS */
				if (!(map_flags & i5_bss->mapFlags)) {
					continue;
				}

#if defined(MULTIAPR2)
				if (i5_bss->assoc_allowance_status == 0x00) {
					WBD_DEBUG("no more assoc req on bss["MACF"] \n",
						ETHERP_TO_MACF(i5_bss->BSSID));
					/* no more association request on this bss, skip */
					continue;
				}
#endif /* MULTIAPR2 */
				sta = wbd_ds_find_sta_in_bss_monitorlist(i5_bss,
					(struct ether_addr*)weak_sta->mac, &ret);
				if (sta) {
					rssi = wbd_tbss_get_estimated_sta_rssi(
						sta, est_rssi, est_rssi_bcn,
						bcn_tm, i5_bss, parent_monitor_bss,
						bcn_monitor_bss, 1);

					/* Consider the STA only if its fail count is 0 */
					if ((sta->fail_cnt == 0) && (rssi > best_rssi)) {
						best_rssi = rssi;
						target_bss = i5_bss;
					}
				} else {
					WBD_WARNING("STA["MACDBG"] Not Found in Monitorlist of "
						"BSS["MACDBG"]\n",
						MAC2STRDBG(weak_sta->mac),
						MAC2STRDBG(i5_bss->BSSID));
				}
			}
		}
	}

end:
	if (target_bss) {
		/* Create and store find tbss log. */
		snprintf(logmsg, sizeof(logmsg), CLI_CMD_LOGS_FOUND_TBSS_ALGO_BEST_RSSI,
			wbd_get_formated_local_time(timestamp, sizeof(timestamp)),
			MAC2STRDBG(weak_sta->mac), MAC2STRDBG(target_bss->BSSID), best_rssi);
		wbd_ds_add_logs_in_master(master_info->parent, logmsg);
	}
	WBD_EXIT();
	return target_bss;
}

/* Selecting BSS for steering a STA based on threshold values. Survival of Fittest(SoF) algo */
i5_dm_bss_type*
wbd_tbss_algo_sof(wbd_master_info_t *master_info, i5_dm_clients_type *weak_sta, bool is_dual_band)
{
	uint8 cur_band, parent_band, band_steer, map_flags = IEEE1905_MAP_FLAG_FRONTHAUL;
	uint8 is_bsta = 0;
	int ret = WBDE_NO_SLAVE_TO_STEER;
	i5_dm_network_topology_type *i5_topology;
	i5_dm_device_type *device, *parent_device;
	i5_dm_interface_type *ifr, *parent_ifr;
	i5_dm_bss_type *bss, *parent_bss;
	wbd_assoc_sta_item_t *assoc_sta;
	wbd_monitor_sta_item_t *monitor_sta = NULL, *bcn_sta = NULL;
	wbd_wl_sta_stats_t stats;
	i5_dm_bss_type *tmp_good_bss = NULL, *target_bss = NULL;
	int node_count = 0;
	wbd_tbss_thld_t *thld_cfg;
	WBD_ENTER();
	char logmsg[WBD_MAX_BUF_128] = {0}, timestamp[WBD_MAX_BUF_32] = {0};

	if (weak_sta->vndr_data == NULL) {
		WBD_WARNING("STA["MACDBG"] NULL Vendor Data\n", MAC2STRDBG(weak_sta->mac));
		goto end;
	}

	i5_topology = ieee1905_get_datamodel();

	parent_bss = (i5_dm_bss_type*)WBD_I5LL_PARENT(weak_sta);
	parent_ifr = (i5_dm_interface_type*)WBD_I5LL_PARENT(parent_bss);
	parent_device = (i5_dm_device_type*)WBD_I5LL_PARENT(parent_ifr);

	/* Get the band of the STA */
	parent_band = WBD_BAND_FROM_CHANNEL(wf_chspec_ctlchan(parent_ifr->chanspec));

	/* If 0 dont do band steering within the same device of the associated STA */
	band_steer = (WBD_BAND_STEER_ENAB((master_info->parent->parent->flags))) ? 1 : 0;

	assoc_sta = (wbd_assoc_sta_item_t*)weak_sta->vndr_data;
	is_bsta = I5_CLIENT_IS_BSTA(weak_sta) ? 1 : 0;
	if (I5_CLIENT_IS_BSTA(weak_sta)) {
		map_flags = IEEE1905_MAP_FLAG_BACKHAUL;
	}

	/* Find tbss with best rssi. */
	foreach_i5glist_item(device, i5_dm_device_type, i5_topology->device_list) {

		int est_rssi = 0, est_rssi_bcn = 0;
		time_t bcn_tm = 0;
		i5_dm_bss_type *parent_monitor_bss = NULL, *bcn_monitor_bss = NULL;

		/* Loop for only for Agent Devices */
		if (!I5_IS_MULTIAP_AGENT(device->flags)) {
			continue;
		}

		/* If the STA is backhaul STA, dont consider if the STA MAC address is part of the
		 * interface list in this device
		 */
		if (I5_CLIENT_IS_BSTA(weak_sta)) {
			if (wbd_ds_get_i5_ifr_in_device(device, weak_sta->mac, &ret)) {
				WBD_DEBUG("Device["MACDBG"] STA["MACDBG"] is backhaul STA and its "
					"from same device so, skip\n",
					MAC2STRDBG(device->DeviceId), MAC2STRDBG(weak_sta->mac));
				continue;
			}
		}

		/* If the device is parent device, then the rssi will be from associated sta */
		if (device == parent_device) {
			est_rssi = WBD_RCPI_TO_RSSI(weak_sta->link_metric.rcpi);
			if (!WBD_BKT_DONT_USE_BCN_RPT(master_info->parent->flags) &&
				BEACON_RSSI_RECENT(assoc_sta->stats.bcn_tm, wbd_get_ginfo())) {
				bcn_tm = assoc_sta->stats.bcn_tm;
				est_rssi_bcn = assoc_sta->stats.bcn_rpt_rssi;
			}
			parent_monitor_bss = parent_bss;
			bcn_monitor_bss = parent_bss;
		} else {
			monitor_sta = wbd_tbss_get_monitor_sta_on_device(device, weak_sta->mac,
				&parent_monitor_bss, map_flags);
			bcn_sta = wbd_tbss_get_beacon_rssi_on_device(device, weak_sta->mac,
				&bcn_monitor_bss, map_flags);
			if (monitor_sta || bcn_sta) {
				if (monitor_sta) {
					est_rssi = monitor_sta->rssi;
				}
				if (bcn_sta) {
					bcn_tm = bcn_sta->bcn_tm;
					est_rssi_bcn = bcn_sta->bcn_rpt_rssi;
				}
			} else {
				WBD_WARNING("STA["MACDBG"] invalid Beacon or monitor rssi in "
					"Monitorlist on Device["MACDBG"]\n",
					MAC2STRDBG(weak_sta->mac), MAC2STRDBG(device->DeviceId));
				continue;
			}
		}

		foreach_i5glist_item(ifr, i5_dm_interface_type, device->interface_list) {

			if (!i5DmIsInterfaceWireless(ifr->MediaType) ||
				!ifr->BSSNumberOfEntries) {
				continue;
			}

			/* Get the current band of the interface */
			cur_band = WBD_BAND_FROM_CHANNEL(wf_chspec_ctlchan(ifr->chanspec));
			/* If the STA doesnt support Dual band and band of the STA is not the
			 * band of interface, then skip it
			 */
			if (!is_dual_band && cur_band != parent_band) {
				WBD_DEBUG("STA["MACDBG"] is not Dual Band so. "
					"skip IFR["MACDBG"] in Device["MACDBG"]\n",
					MAC2STRDBG(weak_sta->mac), MAC2STRDBG(ifr->InterfaceId),
					MAC2STRDBG(device->DeviceId));
				continue;
			}

			/* Dont do steering between bands on the same device where the STA
			 * is associated if the band_steer is disabled
			 */
			if (!band_steer && (device == parent_device) &&
				(cur_band != parent_band)) {
				WBD_INFO("Device["MACDBG"] ParentDevice["MACDBG"] "
					"cur_band[%d] parent_band[%d] "
					"No band steering of STA["MACDBG"]\n",
					MAC2STRDBG(device->DeviceId),
					MAC2STRDBG(parent_device->DeviceId),
					cur_band, parent_band,
					MAC2STRDBG(weak_sta->mac));
				continue;
			}

			foreach_i5glist_item(bss, i5_dm_bss_type, ifr->bss_list) {
				int fail_cnt = 1;

				if (I5_IS_BSS_GUEST(bss->mapFlags)) {
					continue;
				}
				/* Only use matching BSS(Fronthaul or Backhaul) */
				if (!(map_flags & bss->mapFlags)) {
					continue;
				}

#if defined(MULTIAPR2)
				if (bss->assoc_allowance_status == 0x00) {
					WBD_DEBUG("no more assoc req on bss["MACF"] \n",
						ETHERP_TO_MACF(bss->BSSID));
					/* no more association request on this bss, skip */
					continue;
				}
#endif /* MULTIAPR2 */
				memset(&stats, 0, sizeof(stats));

				/* For BSS to which this STA is associated */
				if (bss == parent_bss) {
					stats.rssi = wbd_tbss_get_estimated_rssi(est_rssi,
						est_rssi_bcn, bss, parent_bss, 0);
					stats.tx_rate = weak_sta->link_metric.downlink_rate;
					stats.tx_tot_failures = assoc_sta->stats.tx_tot_failures;
					stats.old_tx_tot_failures =
						assoc_sta->stats.old_tx_tot_failures;
					stats.active = assoc_sta->stats.active;
					fail_cnt = wbd_master_get_threshold_fail_count(master_info,
						bss, &stats, (struct ether_addr*)weak_sta->mac,
						is_bsta);
					assoc_sta->fail_cnt = fail_cnt;
				} else {
					monitor_sta = wbd_ds_find_sta_in_bss_monitorlist(bss,
						(struct ether_addr*)weak_sta->mac, &ret);
					if (monitor_sta) {
						stats.rssi = wbd_tbss_get_estimated_sta_rssi(
							monitor_sta, est_rssi, est_rssi_bcn,
							bcn_tm, bss,
							parent_monitor_bss, bcn_monitor_bss, 0);

						fail_cnt = wbd_master_get_threshold_fail_count(
							master_info, bss, &stats,
							(struct ether_addr*)weak_sta->mac, is_bsta);
						monitor_sta->fail_cnt = fail_cnt;
						WBD_DEBUG("For STA["MACDBG"] in BSS["MACDBG"] "
							"RSSI [%d] fail count [%d]\n",
							MAC2STRDBG(weak_sta->mac),
							MAC2STRDBG(bss->BSSID),
							stats.rssi, fail_cnt);
					} else {
						WBD_WARNING("STA["MACDBG"] Not Found in "
							"Monitorlist in BSS["MACDBG"]\n",
							MAC2STRDBG(weak_sta->mac),
							MAC2STRDBG(bss->BSSID));
					}
				}
				/* If this node has not failed any threshold, this is a potential
				 * candidate for target BSS
				 */
				if (fail_cnt == 0) {
					node_count++;
					tmp_good_bss = bss;
				}
			}
		}
	}

	/* If there are no BSS found with fail_cnt zero */
	if (node_count == 0) {
		WBD_TBSS("All BSS failed to pass threshold check for STA["MACDBG"]\n",
			MAC2STRDBG(weak_sta->mac));
		goto end;
	}

	/* If there is only one node with fail_cnt zero */
	if (node_count == 1) {
		/* If its the BSS in which the weak STA is associated skip it */
		if (parent_bss == tmp_good_bss) {
			WBD_TBSS("For STA["MACDBG"], Only current BSS["MACDBG"] passed all "
				"thresholds. Not Steering\n",
				MAC2STRDBG(weak_sta->mac),
				MAC2STRDBG(tmp_good_bss->BSSID));
			goto end;
		}
		WBD_TBSS("For STA["MACDBG"] only one BSS["MACDBG"] found eligible\n",
			MAC2STRDBG(weak_sta->mac),
			MAC2STRDBG(tmp_good_bss->BSSID));
		target_bss = tmp_good_bss;
		goto end;
	}

	/* If there are more than one bss with fail_cnt is 0, then decide the target BSS
	 * based on the following algorithms which is NVRAM controlled
	 */

	/* 1: the slave with better RSSI
	 * 2: and next one is the NWS algorithm
	 */
	if (parent_band == WBD_BAND_LAN_2G) {
		thld_cfg = &master_info->tbss_info.thld_cfg_2g;
	} else {
		thld_cfg = &master_info->tbss_info.thld_cfg_5g;
	}
	if (thld_cfg->sof_algos == WBD_SOF_ALGO_BEST_RSSI) {
		target_bss = wbd_master_find_target_slave_with_best_rssi(master_info,
			weak_sta, is_dual_band);
		goto end;
	} else if (thld_cfg->sof_algos == WBD_SOF_ALGO_BEST_NWS) {
		target_bss = wbd_tbss_algo_nws(master_info, weak_sta, is_dual_band);
		goto end;
	}

end:
	if (target_bss) {
		/* Create and store find tbss log. */
		snprintf(logmsg, sizeof(logmsg), CLI_CMD_LOGS_FOUND_TBSS_ALGO_SOF,
			wbd_get_formated_local_time(timestamp, sizeof(timestamp)),
			MAC2STRDBG(weak_sta->mac), MAC2STRDBG(target_bss->BSSID));
		wbd_ds_add_logs_in_master(master_info->parent, logmsg);
	}
	WBD_EXIT();

	return target_bss;
}

/* Finds the target BSS in all the slaves */
static i5_dm_bss_type*
wbd_master_find_target_bss(wbd_master_info_t *master_info, i5_dm_clients_type *weak_sta,
	bool is_dual_band)
{
	uint8 tbss_algo;

	/* Update the values required for TBSS calculation */
	wbd_master_update_all_devices_tbss_values();

	/* Choose the TBSS alogorithm to use based on the STA type */
	if (I5_CLIENT_IS_BSTA(weak_sta)) {
		tbss_algo = master_info->tbss_info.tbss_algo_bh;
	} else {
		tbss_algo = master_info->tbss_info.tbss_algo;
	}

	WBD_INFO("BKTNAME[%s] For STA["MACDBG"] DualBand[%d] Finding Target BSS Using Algo[%d]\n",
		master_info->bkt_name, MAC2STRDBG(weak_sta->mac), is_dual_band, tbss_algo);

	return (predefined_tbss_algo[tbss_algo])(master_info, weak_sta, is_dual_band);
}

/* Send steer req msg */
void
wbd_master_send_steer_req(unsigned char *al_mac, unsigned char *sta_mac, unsigned char *bssid,
	i5_dm_bss_type *tbss)
{
	int ret = WBDE_OK;
	ieee1905_steer_req steer_req;
	ieee1905_sta_list *sta_info;
	ieee1905_bss_list *bss_info;
	i5_dm_interface_type *ifr = NULL;
	i5_dm_device_type *dev = NULL;
	ieee1905_vendor_data vndr_msg_data;

	WBD_SAFE_FIND_I5_DEVICE(dev, al_mac, &ret);

	memset(&steer_req, 0, sizeof(steer_req));

	memcpy(&steer_req.neighbor_al_mac, al_mac, MAC_ADDR_LEN);
	memcpy(&steer_req.source_bssid, bssid, MAC_ADDR_LEN);

	/* Fill vndr_msg_data struct object to send Vendor Message */
	memset(&vndr_msg_data, 0x00, sizeof(vndr_msg_data));
	memcpy(vndr_msg_data.neighbor_al_mac, al_mac, MAC_ADDR_LEN);

	/* Allocate Dynamic mem for Vendor data from App */
	vndr_msg_data.vendorSpec_msg =
		(unsigned char *)wbd_malloc(IEEE1905_MAX_VNDR_DATA_BUF, &ret);
	WBD_ASSERT();

	/* Initialize sta and bss list */
	ieee1905_glist_init(&steer_req.sta_list);
	ieee1905_glist_init(&steer_req.bss_list);

#ifdef MULTIAPR2
	steer_req.profile = map_steer_req_profile2;
#endif /* MULTIAPR2 */
	steer_req.request_flags = IEEE1905_STEER_FLAGS_MANDATE |
		IEEE1905_STEER_FLAGS_BTM_ABRIDGED;

	sta_info = (ieee1905_sta_list*)wbd_malloc(sizeof(*sta_info), &ret);
	WBD_ASSERT();
	memcpy(sta_info->mac, sta_mac, MAC_ADDR_LEN);
	ieee1905_glist_append(&steer_req.sta_list, (dll_t*)sta_info);

	bss_info = (ieee1905_bss_list*)wbd_malloc(sizeof(*bss_info), &ret);
	WBD_ASSERT();
	memcpy(bss_info->bssid, tbss->BSSID, MAC_ADDR_LEN);
	ifr = (i5_dm_interface_type*)WBD_I5LL_PARENT(tbss);
	bss_info->target_op_class = ifr->opClass;
	bss_info->target_channel = wf_chspec_ctlchan(ifr->chanspec);
	bss_info->reason_code	= MBO_BTM_REASONCODE_FLAG_LOW_RSSI;
	ieee1905_glist_append(&steer_req.bss_list, (dll_t*)bss_info);

	WBD_TBSS("Sending the steer req for sta["MACDBG"] "
		"at bss["MACDBG"] for target bss["MACDBG"] opclass [%d] chan [%d]\n",
		MAC2STRDBG(sta_mac), MAC2STRDBG(bssid), MAC2STRDBG(tbss->BSSID),
		bss_info->target_op_class, bss_info->target_channel);

	/* Encode Vendor Specific TLV for Message : Steer request vendor TLV to send */
	ret = wbd_tlv_encode_vendor_steer_request((void *)tbss,
		vndr_msg_data.vendorSpec_msg, &vndr_msg_data.vendorSpec_len);
	if (ret != WBDE_OK) {
		WBD_WARNING("Failed to encode Steer request vendor TLV which needs to be sent "
			"from Device["MACDBG"] to Device["MACDBG"]\n",
			MAC2STRDBG(ieee1905_get_al_mac()),
			MAC2STRDBG(vndr_msg_data.neighbor_al_mac));
	}

	i5MessageClientSteeringRequestSend(dev->psock, al_mac, &steer_req, &vndr_msg_data);

end:
	i5DmSteerRequestInfoFree(&steer_req);
	if (vndr_msg_data.vendorSpec_msg) {
		free(vndr_msg_data.vendorSpec_msg);
	}
	return;
}

/* Send backhaul Steer request */
static int
wbd_master_send_backhaul_steer(i5_dm_clients_type *sta, i5_dm_bss_type *tbss)
{
	i5_dm_interface_type *tifr, *ifr;
	i5_dm_device_type *device;
	ieee1905_backhaul_steer_msg bh_steer_req;

	tifr = (i5_dm_interface_type*)WBD_I5LL_PARENT(tbss);

	/* Find the interface in the network matching backhaul STA MAC. Send backhaul STEER request
	 * to that device where the interface exists
	 */
	if ((ifr = i5DmFindInterfaceFromNetwork(sta->mac)) == NULL) {
		WBD_WARNING("Backhaul STA MAC["MACDBG"] Not found as Interface in Network\n",
			MAC2STRDBG(sta->mac));
		return WBDE_DS_UN_IFR;
	}
	device = (i5_dm_device_type*)WBD_I5LL_PARENT(ifr);

	memset(&bh_steer_req, 0, sizeof(bh_steer_req));
	memcpy(&bh_steer_req.bh_sta_mac, &sta->mac, sizeof(bh_steer_req.bh_sta_mac));
	memcpy(&bh_steer_req.trgt_bssid, &tbss->BSSID, sizeof(bh_steer_req.trgt_bssid));
	bh_steer_req.opclass = tifr->opClass;
	bh_steer_req.channel = wf_chspec_ctlchan(tifr->chanspec);

	WBD_INFO("Send Backhaul STEER to["MACDBG"] for STA["MACDBG"] TargetBSS["MACDBG"] "
		"Opclass[%d] Chanspec[0x%x]\n",
		MAC2STRDBG(device->DeviceId), MAC2STRDBG(sta->mac), MAC2STRDBG(tbss->BSSID),
		tifr->opClass, tifr->chanspec);
	return (ieee1905_send_backhaul_steering_request(device->DeviceId, &bh_steer_req));
}

/* Callback fn called fm scheduler library to identify the target BSS for steerable STAs */
static void
wbd_master_identify_target_bss_timer_cb(bcm_usched_handle *hdl, void *arg)
{
	bool is_dual_band = FALSE;
	uint8 parent_band, map_flags = IEEE1905_MAP_FLAG_FRONTHAUL, is_backhaul = 0;
	uint8 bounce_sta_flag = 0x00;
	int ret = WBDE_OK, free_timer = 1;
	wbd_tbss_timer_args_t *args = (wbd_tbss_timer_args_t*)arg;
	wbd_master_info_t *master_info = NULL;
	i5_dm_clients_type *sta = NULL;
	i5_dm_bss_type *tbss = NULL, *sbss = NULL;
	i5_dm_interface_type *parent_ifr;
	wbd_assoc_sta_item_t *assoc_sta = NULL;
	char logmsg[WBD_MAX_BUF_128] = {0}, timestamp[WBD_MAX_BUF_32] = {0};
	wbd_monitor_sta_item_t *sta_item = NULL;
	wbd_prb_sta_t *prbsta;
	WBD_ENTER();

	WBD_ASSERT_ARG(args, WBDE_INV_ARG);
	WBD_ASSERT_ARG(args->master_info, WBDE_INV_ARG);
	is_backhaul = args->is_backhaul;

	BCM_REFERENCE(sta_item);

	master_info = args->master_info;

	WBD_SAFE_FIND_I5_BSS_IN_DEVICE(args->al_mac, args->bssid, sbss, &ret);

	sta = wbd_ds_find_sta_in_bss_assoclist(sbss, (struct ether_addr*)args->sta_mac,
		&ret, &assoc_sta);
	WBD_ASSERT_MSG("In device["MACDBG"] BSSID["MACDBG"] STA["MACDBG"]. %s\n",
		MAC2STRDBG(args->al_mac), MAC2STRDBG(args->bssid),
		MAC2STRDBG(args->sta_mac), wbderrorstr(ret));

	/* If STA is no more weak or backhaul optimization is not running for this STA then
	 * remove the timer.
	 */
	if (assoc_sta && ((assoc_sta->status != WBD_STA_STATUS_WEAK) &&
		(!WBD_ASSOC_ITEM_IS_BH_OPT(assoc_sta->flags)))) {
		WBD_DEBUG("Not weak or not backhaul managed flag[%x] status[%d] IsBackhaul[%d]\n",
			assoc_sta->flags, assoc_sta->status, args->is_backhaul);
		goto end;
	}

	/* For backhaul STA is_dual_band should be FALSE so that it wont steer across the bands */
	if (I5_CLIENT_IS_BSTA(sta)) {
		map_flags = IEEE1905_MAP_FLAG_BACKHAUL;
		if (!WBD_BKT_BH_OPT_ENAB(master_info->parent->flags)) {
			wbd_ds_remove_sta_fm_peer_devices_monitorlist(
				(struct ether_addr*)&sbss->BSSID,
				(struct ether_addr*)&sta->mac, map_flags);
			WBD_INFO("Backhaul Optimization is disabled. Flags[0x%08x]. Skipping "
				"backhaul optimization\n", master_info->parent->flags);
			goto end;
		}
	} else {
		parent_ifr = (i5_dm_interface_type*)WBD_I5LL_PARENT(sbss);

		/* Get the band of the STA. If the STA is in 5G assume that it supports dual band */
		parent_band = WBD_BAND_FROM_CHANNEL(wf_chspec_ctlchan(parent_ifr->chanspec));
		if (WBD_IS_DUAL_BAND(parent_band)) {
			is_dual_band = TRUE;
		} else {
			/* If the band is not dual. Get the band supported from probe STA list */
			prbsta = wbd_ds_find_sta_in_probe_sta_table(master_info->parent->parent,
				(struct ether_addr*)sta->mac, FALSE);
			if (prbsta) {
				if (WBD_IS_DUAL_BAND(prbsta->band)) {
					is_dual_band = TRUE;
				}
			} else {
				WBD_WARNING("Warning: probe list has no STA["MACDBG"]\n",
					MAC2STRDBG(sta->mac));
			}
		}
	}

	tbss = wbd_master_find_target_bss(master_info, sta, is_dual_band);
	/* if no valid target BSS, or source & target BSS same, go for next try */
	if (tbss == NULL || tbss == sbss) {
		/* If the backhaul optimization is running for this STA, then increment the counter */
		if (WBD_ASSOC_ITEM_IS_BH_OPT(assoc_sta->flags)) {
			assoc_sta->bh_opt_count++;
			WBD_TBSS("For BHSTA["MACDBG"]. There Is no tbss in backhaul optimization. "
				"TotalTry[%d] MaxTry[%d]\n",
				MAC2STRDBG(sta->mac), assoc_sta->bh_opt_count,
				master_info->parent->max_bh_opt_try);
			/* If the backhaul optimization retry exceeds the limit, stop for that STA */
			if (assoc_sta->bh_opt_count >= master_info->parent->max_bh_opt_try) {
				/* Just go to end, and in the end it will take care of starting for
				 * other STA
				 */
				wbd_ds_remove_sta_fm_peer_devices_monitorlist(
					(struct ether_addr*)&sbss->BSSID,
					(struct ether_addr*)&sta->mac, map_flags);
				goto end;
			}
		}
		WBD_TBSS("There Is no tbss sending link metric req again \n");
		wbd_master_send_link_metric_requests(master_info, sta);
		/* Dont remove the timer as TBSS is not found. Try again */
		free_timer = 0;
	} else {
		if (I5_CLIENT_IS_BSTA(sta)) {
			/* Initialze bounce STA flag as backhaul STA */
			bounce_sta_flag |= WBD_STA_BOUNCE_DETECT_FLAGS_BH;

			if (wbd_master_send_backhaul_steer(sta, tbss) != WBDE_OK) {
				WBD_INFO("Failed to send backhaul steer. Sending link metric "
					"req again\n");
				wbd_master_send_link_metric_requests(master_info, sta);
				/* Dont remove the timer as TBSS is not found. Try again */
				free_timer = 0;
				goto end;
			}
		} else {
			wbd_master_send_steer_req(args->al_mac, args->sta_mac, args->bssid, tbss);
		}
#ifdef BCM_APPEVENTD
		/* Fetch the monitored sta item from dst slave's monitor list */
		sta_item = wbd_ds_find_sta_in_bss_monitorlist(tbss,
			(struct ether_addr*)args->sta_mac, &ret);
		/* Send steer start event to appeventd. */
		wbd_appeventd_steer_start(APP_E_WBD_MASTER_STEER_START, " ",
			(struct ether_addr*)args->sta_mac, (struct ether_addr*)tbss->BSSID,
			assoc_sta->stats.rssi, sta_item ? sta_item->rssi : 0);
#endif /* BCM_APPEVENTD */
		/* Remove STA from all BSS' Monitor STA List */
		if (wbd_ds_remove_sta_fm_peer_devices_monitorlist((struct ether_addr*)&sbss->BSSID,
				(struct ether_addr*)&sta->mac, map_flags) != WBDE_OK) {
			WBD_INFO("BKTID[%d] STA["MACDBG"]. Failed to remove STA "
				"from peer slaves monitorlist\n", master_info->bkt_id,
				MAC2STRDBG(sta->mac));
		}

		/* If successfully sent STEER cmd, Update STA's Status = Steering */
		WBD_DS_UP_ASSOCSTA_STATUS(master_info, assoc_sta, WBD_STA_STATUS_WEAK_STEERING);

		/* After steering, add the STA to the bouncing table. If the STA is already exists
		 * in the bouncing table, the enrty will be updated. Do not update the Bounce Count
		 * right now, instead, Update the STA Status to STEERING, and when this STA
		 * associates to TBSS then increament the count.
		 */
		wbd_ds_add_sta_to_bounce_table(master_info->parent, (struct ether_addr*)&sta->mac,
			(struct ether_addr*)&args->bssid, bounce_sta_flag);

		/* Create and store steer log. */
		snprintf(logmsg, sizeof(logmsg), CLI_CMD_LOGS_STEER,
			wbd_get_formated_local_time(timestamp, sizeof(timestamp)),
			MAC2STRDBG(sta->mac), MAC2STRDBG(args->bssid), MAC2STRDBG(tbss->BSSID));
		wbd_ds_add_logs_in_master(master_info->parent, logmsg);
	}

end:
	/* free_timer is set, remove the timer */
	if (free_timer) {
		bcm_usched_remove_timer(hdl, wbd_master_identify_target_bss_timer_cb, args);
		if (args != NULL) {
			free(args);
			args = NULL;
		}

		/* Update the flag that, TBSS timer is removed */
		if (assoc_sta) {
			assoc_sta->flags &= ~WBD_FLAGS_ASSOC_ITEM_TBSS;
		}
	}

	if (is_backhaul) {
		/* free_timer is 1 means, backhaul STA steering for the current STA is done */
		if (free_timer && master_info) {
			/* If the backhaul optimization is running, set the flag as done, then start
			 * with another STA
			 */
			if (WBD_MINFO_IS_BH_OPT_RUNNING(master_info->flags)) {
				WBD_TBSS("Backhaul STA optimization is running mflag %x aflag %x\n",
					master_info->flags, assoc_sta ? assoc_sta->flags : 0x00);
				if (assoc_sta) {
					assoc_sta->flags &= ~WBD_FLAGS_ASSOC_ITEM_BH_OPT;
					assoc_sta->flags |= WBD_FLAGS_ASSOC_ITEM_BH_OPT_DONE;
				}
				master_info->flags &= ~WBD_FLAGS_MASTER_BH_OPT_RUNNING;
				wbd_master_start_bh_opt(NULL);
			} else if (WBD_MINFO_IS_BH_OPT_PENDING(master_info->flags)) {
				/* If the backhaul optimization pending is set, then restart the
				 * backhaul optimization
				 */
				WBD_TBSS("Restart the backhaul optimization as backhaul sta "
					"steering is done mflag %x\n", master_info->flags);
				master_info->flags &= ~WBD_FLAGS_MASTER_BH_OPT_PENDING;
				wbd_ds_unset_bh_opt_flags(WBD_FLAGS_ASSOC_ITEM_BH_OPT_DONE);
				wbd_master_start_bh_opt(NULL);
			}
		}
	}

	WBD_EXIT();
}

/* Create the timer to identify the target BSS for weak STA */
int
wbd_master_create_identify_target_bss_timer(wbd_master_info_t *master_info,
	i5_dm_clients_type *i5_sta)
{
	int ret = WBDE_OK, timeout;
	wbd_tbss_timer_args_t *args = NULL;
	i5_dm_bss_type *i5_bss;
	i5_dm_interface_type *i5_ifr;
	i5_dm_device_type *i5_device;
	wbd_assoc_sta_item_t *assoc_sta;

	WBD_ENTER();

	args = (wbd_tbss_timer_args_t *)wbd_malloc(sizeof(*args), &ret);
	WBD_ASSERT_MSG("Malloc falied for tbss timer creation\n");

	i5_bss = (i5_dm_bss_type*)WBD_I5LL_PARENT(i5_sta);
	i5_ifr = (i5_dm_interface_type*)WBD_I5LL_PARENT(i5_bss);
	i5_device = (i5_dm_device_type*)WBD_I5LL_PARENT(i5_ifr);
	assoc_sta = (wbd_assoc_sta_item_t*)i5_sta->vndr_data;

	memset(args, 0, sizeof(*args));
	args->master_info = master_info;
	memcpy(args->al_mac, i5_device->DeviceId, MAC_ADDR_LEN);
	memcpy(args->bssid, i5_bss->BSSID, MAC_ADDR_LEN);
	memcpy(args->sta_mac, i5_sta->mac, MAC_ADDR_LEN);

	/* Get the timeout for TBSS calculation timer based on STA type(Backhaul or Fronthaul) */
	if (I5_CLIENT_IS_BSTA(i5_sta)) {
		/* If the backhaul optimization is running, take the optimization TBSS timeout.
		 * Else, take the backhaul STA TBSS timeout
		 */
		if (assoc_sta && WBD_ASSOC_ITEM_IS_BH_OPT(assoc_sta->flags)) {
			timeout = master_info->parent->parent->max.tm_wd_bh_opt_tbss;
		} else {
			timeout = master_info->parent->parent->max.tm_wd_bh_tbss;
		}
		args->is_backhaul = 1;
	} else {
		timeout = master_info->parent->parent->max.tm_wd_tbss;
	}

	ret = wbd_add_timers(master_info->parent->parent->hdl, args,
		WBD_SEC_MICROSEC(timeout),
		wbd_master_identify_target_bss_timer_cb, 1);

	if (ret != WBDE_OK) {
		WBD_WARNING("BKTNAME[%s] Interval[%d] Failed to create identify TBSS timer\n",
			master_info->bkt_name, timeout);
		goto end;
	}

	if (assoc_sta) {
		assoc_sta->flags |= WBD_FLAGS_ASSOC_ITEM_TBSS;
	}

end:
	WBD_EXIT();

	return ret;
}

/* Is TBSS timer created for any of the backhaul STAs */
int
wbd_master_is_tbss_created_for_bsta()
{
	i5_dm_network_topology_type *i5_topology;
	i5_dm_device_type *i5_device;
	i5_dm_interface_type *i5_ifr;
	i5_dm_bss_type *i5_bss;
	i5_dm_clients_type *i5_sta;
	wbd_assoc_sta_item_t *assoc_sta;

	i5_topology = ieee1905_get_datamodel();

	foreach_i5glist_item(i5_device, i5_dm_device_type, i5_topology->device_list) {
		/* Loop for only for Agent Devices */
		if (!I5_IS_MULTIAP_AGENT(i5_device->flags)) {
			continue;
		}

		foreach_i5glist_item(i5_ifr, i5_dm_interface_type, i5_device->interface_list) {
			foreach_i5glist_item(i5_bss, i5_dm_bss_type, i5_ifr->bss_list) {
				/* If not backhaul BSS continue */
				if (!I5_IS_BSS_BACKHAUL(i5_bss->mapFlags)) {
					continue;
				}

				foreach_i5glist_item(i5_sta, i5_dm_clients_type,
					i5_bss->client_list) {

					/* If not backhaul STA continue */
					if (!I5_CLIENT_IS_BSTA(i5_sta)) {
						continue;
					}

					assoc_sta = (wbd_assoc_sta_item_t*)i5_sta->vndr_data;
					if (assoc_sta &&
						WBD_ASSOC_ITEM_IS_TBSS_TIMER(assoc_sta->flags)) {
						WBD_DEBUG("TBSS timer already created for bSTA "
							MACDBG" on device "MACDBG". Interface:"
							MACDBG" BSS:"MACDBG"\n",
							MAC2STRDBG(i5_sta->mac),
							MAC2STRDBG(i5_device->DeviceId),
							MAC2STRDBG(i5_ifr->InterfaceId),
							MAC2STRDBG(i5_bss->BSSID));
						return 1;
					}
				}
			}
		}
	}

	return 0;
}

/* Check if the backhaul STA steering is possible or not to any of the backhaul BSS in the network
 * by checking whether it can form a loop if we steer it to the BSS or not
 */
int
wbd_master_is_bsta_steering_possible(i5_dm_clients_type *i5_sta)
{
	int ret = WBDE_OK, is_possible = 0;
	i5_dm_network_topology_type *i5_topology;
	i5_dm_device_type *i5_parent_device, *i5_device, *i5_self_device, *i5_sta_ifr_device;
	i5_dm_interface_type *i5_self_ifr, *i5_sta_ifr;
	i5_dm_bss_type *i5_self_bss;
	WBD_ENTER();

	WBD_ASSERT_ARG(i5_sta, WBDE_INV_ARG);

	if ((i5_topology = ieee1905_get_datamodel()) == NULL) {
		goto end;
	}

	i5_self_bss = (i5_dm_bss_type*)WBD_I5LL_PARENT(i5_sta);
	i5_self_ifr = (i5_dm_interface_type*)WBD_I5LL_PARENT(i5_self_bss);
	i5_self_device = (i5_dm_device_type*)WBD_I5LL_PARENT(i5_self_ifr);

	/* Find the interface in the network matching backhaul STA MAC */
	if ((i5_sta_ifr = i5DmFindInterfaceFromNetwork(i5_sta->mac)) == NULL) {
		WBD_WARNING("bSTA "MACDBG" is not an interface in network. Will not steer him\n",
			MAC2STRDBG(i5_sta->mac));
		goto end;
	}
	i5_sta_ifr_device = (i5_dm_device_type*)WBD_I5LL_PARENT(i5_sta_ifr);
	WBD_INFO("Checking if bSTA "MACDBG" from device "MACDBG" can be steered to any other "
		"potential TBSS. It is currently associated to BSS "MACDBG" on device "MACDBG"\n",
		MAC2STRDBG(i5_sta->mac), MAC2STRDBG(i5_sta_ifr_device->DeviceId),
		MAC2STRDBG(i5_self_bss->BSSID), MAC2STRDBG(i5_self_device->DeviceId));

	foreach_i5glist_item(i5_device, i5_dm_device_type, i5_topology->device_list) {

		i5_parent_device = i5_device;

		WBD_INFO("Checking if bSTA "MACDBG" can be steered to device "MACDBG"\n",
			MAC2STRDBG(i5_sta->mac), MAC2STRDBG(i5_device->DeviceId));
		/* Loop for only for Agent Devices with backhaul BSS also skip the self device */
		if (!I5_IS_MULTIAP_AGENT(i5_device->flags) || !I5_HAS_BH_BSS(i5_device->flags) ||
			i5_self_device == i5_device) {
			WBD_DEBUG("Device "MACDBG" is %san agent. It %sbackhaul BSS. bSTA "MACDBG
				" is %sassociated to it. Hence not a candidate for TBSS. Skipping\n"
				, MAC2STRDBG(i5_device->DeviceId),
				I5_IS_MULTIAP_AGENT(i5_device->flags) ? "" : "not ",
				I5_HAS_BH_BSS(i5_device->flags) ? "has " : "does not have ",
				MAC2STRDBG(i5_sta->mac),
				(i5_self_device == i5_device) ? "" : "not ");
			continue;
		}

		/* If the STA is backhaul STA, dont consider if the STA MAC address is part of the
		 * interface list in this device
		 */
		if (i5_sta_ifr_device == i5_device) {
			WBD_DEBUG("bSTA "MACDBG" resides in device "MACDBG". Not a candidate for "
				"TBSS. Skipping\n", MAC2STRDBG(i5_sta->mac),
				MAC2STRDBG(i5_device->DeviceId));
			continue;
		}

		/* Now loop through parent devices of each device till we reach
		 * controller (means parent device pointer is NULL). Skip the device where sta is
		 * associated also skip the device where the STA interface is present. If we hit
		 * the device where the STA is interface is present, then there will be loop
		 */
		while (i5_parent_device && (i5_parent_device != i5_self_device) &&
			(i5_parent_device != i5_sta_ifr_device)) {
			WBD_DEBUG("Checking if device "MACDBG" is connected to controller without "
				"a hop via "MACDBG". Next parent is "MACDBG"\n",
				MAC2STRDBG(i5_device->DeviceId),
				MAC2STRDBG(i5_sta_ifr_device->DeviceId),
				MAC2STRDBG(i5_parent_device->DeviceId));
			i5_parent_device = i5_parent_device->parentDevice;
		}

		if (i5_parent_device == NULL) {
			is_possible = 1;
			WBD_INFO("Device "MACDBG" is a possible TBSS candidate for bSTA "MACDBG"\n",
				MAC2STRDBG(i5_device->DeviceId), MAC2STRDBG(i5_sta->mac));
			break;
		} else {
			WBD_INFO("Device "MACDBG" is not a possible TBSS candidate for bSTA "
				MACDBG". One of its parent is "MACDBG" and it will cause loop\n",
				MAC2STRDBG(i5_device->DeviceId), MAC2STRDBG(i5_sta->mac),
				MAC2STRDBG(i5_parent_device->DeviceId));
		}
	}

end:
	WBD_EXIT();
	return is_possible;
}

/* Start the backhaul optimization on backhaul STA join */
void
wbd_master_start_bh_opt(unsigned char *sta_mac)
{
	int ret = WBDE_OK, b_bss_count, bsta_count;
	wbd_master_info_t *master_info;
	i5_dm_clients_type *i5_sta = NULL;
	wbd_assoc_sta_item_t *assoc_sta = NULL;
	wbd_sta_bounce_detect_t *bounce_sta_entry;
	wbd_blanket_master_t *wbd_master;
	uint32 dwell_time;
	bool is_bh_opt_in_progress = 0;
	WBD_ENTER();

	WBD_SAFE_GET_MASTER_INFO(wbd_get_ginfo(), WBD_BKT_ID_BR0, master_info, &ret);
	wbd_master = master_info->parent;

	blanket_nvram_prefix_set(NULL, WBD_NVRAM_BH_OPT_COMPLETE, "0");

	if (!WBD_BKT_BH_OPT_ENAB(wbd_master->flags)) {
		WBD_INFO("Backhaul Optimization is disabled. Flags[0x%08x]. Skipping backhaul "
			"optimization\n", wbd_get_ginfo()->wbd_master->flags);
		goto end;
	}

	/* If the TBSS timer is created for any of the backhaul STAs, set the flag such that,
	 * start the backhaul optimization once the backhaul steering is done
	 */
	if (wbd_master_is_tbss_created_for_bsta()) {
		WBD_INFO("Backhaul steering in progress. Skipping backhaul optiization\n");
		is_bh_opt_in_progress = 1;
		master_info->flags |= WBD_FLAGS_MASTER_BH_OPT_PENDING;
		goto end;
	}

	if ((bsta_count = wbd_ds_count_bstas()) <= 0) {
		WBD_INFO("Number of bSTAs=%d. Skipping backhaul optimization\n", bsta_count);
		goto end;
	}

	WBD_INFO("Start backhaul optimization\n");
	/* Update the bouncing table */
	wbd_ds_update_sta_bounce_table(wbd_master);

	if (sta_mac && (!(ETHER_ISNULLADDR((struct ether_addr*)sta_mac)))) {
		i5_sta = wbd_ds_find_sta_in_topology(sta_mac, &ret);
	}

find_next_sta:
	/* If the STA MAC is present start the backhaul optimization for the specified backhaul STA.
	 * Else get the STA for which the backhaul optimization is not done
	 */
	if (!i5_sta) {
		i5_sta = wbd_ds_get_bh_opt_pending_sta();
	}

	assoc_sta = i5_sta ? (wbd_assoc_sta_item_t*)i5_sta->vndr_data : NULL;
	if (!assoc_sta) {
		WBD_INFO("No STA found for backhaul optimization\n");
		goto end;
	}

	WBD_DEBUG("Checking backhaul optimization for bSTA "MACDBG"\n", MAC2STRDBG(i5_sta->mac));
	/* Check if the STA can be steered to other backhaul BSS without forming any loop */
	if (!wbd_master_is_bsta_steering_possible(i5_sta)) {
		WBD_INFO("Steering bSTA "MACDBG" may form a loop. Skip it\n",
			MAC2STRDBG(i5_sta->mac));
		assoc_sta->flags |= WBD_FLAGS_ASSOC_ITEM_BH_OPT_DONE;
		i5_sta = NULL;
		goto find_next_sta;
	}

	/* Fetch the STA entry from bouncing table,
	 * if sta is present and sta state is WBD_BOUNCE_DWELL_STATE, skip this STA
	 */
	bounce_sta_entry = wbd_ds_find_sta_in_bouncing_table(wbd_master,
		(struct ether_addr*)i5_sta->mac);
	if ((wbd_master->bounce_cfg_bh.cnt == 0) ||
		((bounce_sta_entry != NULL) &&
		(bounce_sta_entry->state == WBD_BOUNCE_DWELL_STATE))) {
		/* Calculate remaining dwell preiod. If the bounce count configuration is 0 then
		 * make the dwell time as default dwell time. Else, get the remaining dwell period
		 * by taking the difference of dwell time configured and the time period the STA
		 * is in dwell state
		 */
		dwell_time = ((wbd_master->bounce_cfg_bh.cnt == 0) ?
			wbd_master->bounce_cfg_bh.dwell_time :
			(wbd_master->bounce_cfg_bh.dwell_time - bounce_sta_entry->run.dwell_time));
		WBD_INFO("BKTID[%d] bSTA "MACDBG" in bouncing state for another %d seconds. Skip "
			"it\n", master_info->bkt_id, MAC2STRDBG(i5_sta->mac), dwell_time);
		assoc_sta->flags |= WBD_FLAGS_ASSOC_ITEM_BH_OPT_DONE;
		i5_sta = NULL;
		goto find_next_sta;
	}

	b_bss_count = i5DmCountNumOfAgentsWithbBSS();

	/* If the device with STA interface has the backhaul BSS, reduce the backhaul BSS
	 * count by 1
	 */
	if (wbd_ds_is_bsta_device_has_bbss(i5_sta->mac)) {
		b_bss_count -= 1;
	}

	if (b_bss_count <= 1) {
		WBD_INFO("Number of backhaul BSS where bSTA "MACDBG" can be connected is %d. Cannot"
			" do further optimization. Skip it\n", MAC2STRDBG(i5_sta->mac), b_bss_count);
		goto end;
	}

	WBD_INFO("Number of backhaul BSS where bSTA "MACDBG" can be connected is %d. Checking if "
		"bSTA should be steered\n", MAC2STRDBG(i5_sta->mac), b_bss_count);
	is_bh_opt_in_progress = 1;
	assoc_sta->bh_opt_count = 0;
	assoc_sta->flags |= WBD_FLAGS_ASSOC_ITEM_BH_OPT;
	master_info->flags |= WBD_FLAGS_MASTER_BH_OPT_RUNNING;
	wbd_master_send_link_metric_requests(master_info, i5_sta);

	/* If the TBSS timer is not created for this STA. create it */
	if (!WBD_ASSOC_ITEM_IS_TBSS_TIMER(assoc_sta->flags)) {
		wbd_master_create_identify_target_bss_timer(master_info, i5_sta);
	}

end:
	if (!is_bh_opt_in_progress) {
		WBD_INFO("Backhaul optimization complete!\n");
		blanket_nvram_prefix_set(NULL, WBD_NVRAM_BH_OPT_COMPLETE, "1");
	}
	WBD_EXIT();
}

/* Backhaul optimization timeout */
static void
wbd_master_start_bh_opt_timeout(bcm_usched_handle *hdl, void *arg)
{
	int ret = WBDE_OK;
	wbd_master_info_t *master_info;
	wbd_bh_opt_timer_args_t *bh_opt_arg = (wbd_bh_opt_timer_args_t*)arg;
	WBD_ENTER();

	WBD_ASSERT_ARG(bh_opt_arg, WBDE_INV_ARG);
	WBD_SAFE_GET_MASTER_INFO(wbd_get_ginfo(), WBD_BKT_ID_BR0, master_info, &ret);
	master_info->flags &= ~WBD_FLAGS_MASTER_BH_OPT_TIMER;
	wbd_master_start_bh_opt(bh_opt_arg->sta_mac);

end:
	if (bh_opt_arg) {
		free(bh_opt_arg);
	}
	WBD_EXIT();
}

/* Create the backhaul optimization timer. Check if we should start all the backhaul optimization
 * in the network. This can be called if the device is joined the network or any of the backhaul
 * STA is joined the network
 */
void
wbd_master_create_bh_opt_timer(i5_dm_clients_type *i5_sta)
{
	int ret = WBDE_OK;
	wbd_master_info_t *master_info;
	wbd_bh_opt_timer_args_t *args;
	WBD_ENTER();

	if (!WBD_BKT_BH_OPT_ENAB(wbd_get_ginfo()->wbd_master->flags)) {
		WBD_INFO("Backhaul Optimization is disabled. Flags[0x%08x]. Not creating timer\n",
			wbd_get_ginfo()->wbd_master->flags);
		blanket_nvram_prefix_set(NULL, WBD_NVRAM_BH_OPT_COMPLETE, "1");
		goto end;
	}
	blanket_nvram_prefix_set(NULL, WBD_NVRAM_BH_OPT_COMPLETE, "0");

	WBD_SAFE_GET_MASTER_INFO(wbd_get_ginfo(), WBD_BKT_ID_BR0, master_info, &ret);

	if (WBD_MINFO_IS_BH_OPT_TIMER(master_info->flags) ||
		WBD_MINFO_IS_BH_OPT_RUNNING(master_info->flags) ||
		WBD_MINFO_IS_BH_OPT_PENDING(master_info->flags)) {
		WBD_INFO("Backhaul Optimization timer is already%s%s%s. Not creating timer\n",
			WBD_MINFO_IS_BH_OPT_TIMER(master_info->flags) ? " created" : "",
			WBD_MINFO_IS_BH_OPT_RUNNING(master_info->flags) ? " running" : "",
			WBD_MINFO_IS_BH_OPT_PENDING(master_info->flags) ? " marked as pending" : "");
		goto end;
	}

	/* If the new STA or backhaul BSS joins, Unset the done flag so that, backhaul optimization
	 * will start for all the backhaul STAs
	 */
	wbd_ds_unset_bh_opt_flags(WBD_FLAGS_ASSOC_ITEM_BH_OPT_DONE);

	args = (wbd_bh_opt_timer_args_t*)wbd_malloc(sizeof(*args), &ret);
	WBD_ASSERT();

	if (i5_sta) {
		memcpy(args->sta_mac, i5_sta->mac, sizeof(args->sta_mac));
	}

	WBD_DEBUG("%s: Creating backhaul optimization timer. Callback after %d sec\n",
		master_info->bkt_name, WBD_TM_BH_OPT);
	ret = wbd_add_timers(master_info->parent->parent->hdl, args,
		WBD_SEC_MICROSEC(WBD_TM_BH_OPT),
		wbd_master_start_bh_opt_timeout, 0);

	if (ret != WBDE_OK) {
		WBD_WARNING("BKTNAME[%s] Interval[%d] Failed to create Backhaul Optimization timer\n",
			master_info->bkt_name, WBD_TM_BH_OPT);
		blanket_nvram_prefix_set(NULL, WBD_NVRAM_BH_OPT_COMPLETE, "1");
	} else {
		master_info->flags |= WBD_FLAGS_MASTER_BH_OPT_TIMER;
	}

end:
	WBD_EXIT();
	return;
}
