/*
 * WBD Weak Client identification Policy
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
 * $Id: wbd_slave_control.c 618311 2016-02-10 15:06:06Z $
 */

#define TYPEDEF_FLOAT_T
#include <math.h>

#include <bcmparams.h>
#include "wbd_slave_control.h"
#include "wbd_json_utility.h"
#include "wbd_slave_com_hdlr.h"
#include "wbd_wl_utility.h"
#include "wbd_rc_shared.h"
#ifdef BCM_APPEVENTD
#include "wbd_appeventd.h"
#endif /* BCM_APPEVENTD */
#include "wbd_tlv.h"
#include "wbd_blanket_utility.h"

/* Use a possible max data rate to filter bogus phyrate reading */
#define WBD_BK_MAX_DATA_RATE  6934
#define WBD_BK_MIN_PHYRATE	6

static wbd_wc_thld_t predefined_wbd_wc_thld[] = {
	/* rssi tx_rate flags */
	/* 0 : Only RSSI */
	{-65, 0.0, 0x1},
	/* 1 : All Threshold */
	{-65, 0.1, 0x3},
	/* End */
	{0, 0.0, 0}
};

typedef bool (*wbd_wc_algo_t)(wbd_slave_item_t *slave_item, i5_dm_clients_type *i5_sta);

/* Deciding if Client is weak or not based on BSD - dummy */
static bool bsd_wc_algo(wbd_slave_item_t *slave_item, i5_dm_clients_type *i5_sta) {return TRUE;}
/* Deciding if Client is weak or not based on threshold values. Policy Survival of Fittest(SoF) */
static bool wbd_wc_algo_sof(wbd_slave_item_t *slave_item, i5_dm_clients_type *i5_sta);

static wbd_wc_algo_t predefined_wbd_wc_algo[] = {
	bsd_wc_algo,
	wbd_wc_algo_sof,
	NULL
};

#define WBD_MAX_WEAK_CLIENT_THRESHOLD \
	(sizeof(predefined_wbd_wc_thld)/sizeof(wbd_wc_thld_t)-1)
#define WBD_MAX_WEAK_CLIENT_ALGO	\
	(sizeof(predefined_wbd_wc_algo)/sizeof(wbd_wc_algo_t)-1)

/* Create the timer to retry the STEER again */
extern int wbd_slave_create_steer_retry_timer(wbd_slave_item_t *slave, i5_dm_clients_type *sta,
	ieee1905_steer_req *steer_req, ieee1905_bss_list *bss_info, int btm_supported);

/* Get the number of threshold policies defined */
int
wbd_get_max_wc_thld()
{
	return WBD_MAX_WEAK_CLIENT_THRESHOLD;
}

/* Get the finding weak client algorithms */
int
wbd_get_max_wc_algo()
{
	return WBD_MAX_WEAK_CLIENT_ALGO;
}

/* Get the threshold policy based on index */
wbd_wc_thld_t*
wbd_get_wc_thld(wbd_slave_item_t *slave_item)
{
	return &predefined_wbd_wc_thld[slave_item->wc_info.wc_thld];
}

static int
wbd_slave_get_threshold_fail_count(wbd_slave_item_t *slave_item, i5_dm_clients_type *i5_sta)
{
	int fail_cnt = 0, rssi;
	float tx_rate;
	wbd_assoc_sta_item_t *sta = (wbd_assoc_sta_item_t*)i5_sta->vndr_data;

	WBD_ENTER();

	if (sta == NULL) {
		WBD_WARNING("STA["MACDBG"] NULL Vendor Data\n", MAC2STRDBG(i5_sta->mac));
		fail_cnt = -1;
		goto end;
	}

	if (slave_item->wc_info.wc_thld_cfg.flags & WBD_WC_THLD_FLAG_RSSI) {
		rssi = sta->stats.rssi;
		if (rssi < slave_item->wc_info.wc_thld_cfg.t_rssi) {
			fail_cnt++;
		}
		WBD_DEBUG("RSSI[%d], Threshold[%d]\n", rssi,
			slave_item->wc_info.wc_thld_cfg.t_rssi);
	}

	if (slave_item->wc_info.wc_thld_cfg.flags & WBD_WC_THLD_FLAG_TXRT) {
		tx_rate = sta->stats.tx_rate;
		if (tx_rate < slave_item->wc_info.wc_thld_cfg.tx_rate) {
			fail_cnt++;
		}
		WBD_DEBUG("TXRate[%.2f], Threshold[%.2f]\n", tx_rate,
			slave_item->wc_info.wc_thld_cfg.tx_rate);
	}

	WBD_DEBUG("Band[%d] Slave["MACF"] STA["MACDBG"] flags[0x%X], Fail Count[%d]\n",
		slave_item->band, ETHER_TO_MACF(slave_item->wbd_ifr.mac),
		MAC2STRDBG(i5_sta->mac), slave_item->wc_info.wc_thld_cfg.flags, fail_cnt);

end:
	WBD_EXIT();
	return fail_cnt;
}

/* Deciding if Client is weak or not based on threshold values. Policy Survival of Fittest(SoF) */
static bool
wbd_wc_algo_sof(wbd_slave_item_t *slave_item, i5_dm_clients_type *sta)
{
	int fail_cnt;
	bool status = FALSE;
	WBD_ENTER();

	fail_cnt = wbd_slave_get_threshold_fail_count(slave_item, sta);

	/* If this STA has failed any threshold, then STA is weak */
	if (fail_cnt != 0) {
		status = TRUE;
		goto end;
	}

end:
	WBD_EXIT();
	return status;
}

/* Checks the STA is weak or not */
static bool
wbd_is_client_weak(wbd_slave_item_t *slave_item, i5_dm_clients_type *sta)
{
	return (predefined_wbd_wc_algo[slave_item->wc_info.wc_algo])(slave_item, sta);
}

/* Checks for the Weak STAs in the assoclist */
static int
wbd_slave_find_weak_client(wbd_slave_item_t *slave_item, int *found)
{
	int ret = WBDE_OK;
	wbd_assoc_sta_item_t* sta_item = NULL;
	i5_dm_bss_type *i5_bss;
	i5_dm_clients_type *i5_sta;
	WBD_ENTER();

	WBD_DS_GET_I5_SELF_BSS((unsigned char*)&slave_item->wbd_ifr.bssid, i5_bss, &ret);

	/* Travese STA items associated with this BSS */
	foreach_i5glist_item(i5_sta, i5_dm_clients_type, i5_bss->client_list) {

		sta_item = (wbd_assoc_sta_item_t*)i5_sta->vndr_data;
		if (sta_item == NULL) {
			WBD_WARNING("STA["MACDBG"] NULL Vendor Data\n", MAC2STRDBG(i5_sta->mac));
			continue;
		}

		/* If the STA is marked as ignore or marked as steered no need to check */
		if ((sta_item->status == WBD_STA_STATUS_IGNORE) ||
			(sta_item->status == WBD_STA_STATUS_WEAK_STEERING)) {
			continue;
		}

		if (wbd_is_client_weak(slave_item, i5_sta) == TRUE) {
			/* If already WEAK_CLIENT dont send command again */
			if (sta_item->status == WBD_STA_STATUS_WEAK) {
				continue;
			}

			WBD_STEER("Band[%d] In slave["MACF"] Found weak STA["MACDBG"]\n",
				slave_item->band, ETHER_TO_MACF(slave_item->wbd_ifr.bssid),
				MAC2STRDBG(i5_sta->mac));
			/* Send WEAK_CLIENT command and Update STA Status = Weak */

		} else {
			/* Check if its already weak client */
			if (sta_item->status == WBD_STA_STATUS_WEAK) {
				WBD_STEER("Band[%d] In slave["MACF"] Weak STA["MACDBG"] became "
					"strong\n", slave_item->band,
					ETHER_TO_MACF(slave_item->wbd_ifr.bssid),
					MAC2STRDBG(i5_sta->mac));
				/* Send WEAK_CANCEL command and Update STA Status = Normal */
			}
		}
	}

	WBD_EXIT();
	return ret;
}

/* Callback function called fm scheduler lib to send WEAK_CLIENT cmd to Master, if required */
static void
wbd_slave_identify_weak_client_timer_cb(bcm_usched_handle *hdl, void *arg)
{
	int ret = WBDE_OK, found;
	wbd_slave_item_t *slave_item = (wbd_slave_item_t*)arg;
	i5_dm_bss_type *self_bss;
	WBD_ENTER();
	BCM_REFERENCE(ret);

	WBD_DS_GET_I5_SELF_BSS((unsigned char*)&slave_item->wbd_ifr.bssid, self_bss, &ret);

	wbd_slave_update_assoclist_fm_wl(self_bss, NULL, FALSE);

	ret = wbd_slave_find_weak_client(slave_item, &found);

	WBD_EXIT();
}

/* Create the timer to check for weak clients & send WEAK_CLIENT command to Master, if any */
int
wbd_slave_create_identify_weak_client_timer(wbd_slave_item_t *slave_item)
{
	int ret = WBDE_OK;
	WBD_ENTER();

	/* if Selecting Weak Client based on BSD app, don't start/stop WBD Watchdog timer */
	if (slave_item->wc_info.wc_algo == 0)
		goto end;

	ret = wbd_add_timers(slave_item->parent->parent->hdl, slave_item,
		WBD_SEC_MICROSEC(slave_item->parent->parent->max.tm_wd_weakclient),
		wbd_slave_identify_weak_client_timer_cb, 1);

	if (ret != WBDE_OK) {
		WBD_WARNING("Band[%d] Slave["MACF"] Interval[%d] Failed to create weak_client ide"
			"ntify timer\n", slave_item->band, ETHER_TO_MACF(slave_item->wbd_ifr.mac),
			slave_item->parent->parent->max.tm_wd_weakclient);
	}

end:
	WBD_EXIT();
	return ret;
}

/* Remove timer to check for weak clients */
int
wbd_slave_remove_identify_weak_client_timer(wbd_slave_item_t *slave_item)
{
	int ret = WBDE_OK;
	WBD_ENTER();

	/* if Selecting Weak Client based on BSD app, don't start/stop WBD Watchdog timer */
	if (slave_item->wc_info.wc_algo == 0)
		goto end;

	/* Remove WEAK_CLIENT timer */
	ret = wbd_remove_timers(slave_item->parent->parent->hdl,
		wbd_slave_identify_weak_client_timer_cb, slave_item);

end:
	WBD_EXIT();
	return ret;
}

/* Passes the association control request to the ieee1905 */
void
wbd_slave_send_assoc_control(int blk_time, unsigned char *source_bssid,
	unsigned char *trgt_bssid, struct ether_addr *sta_mac)
{
	ieee1905_client_assoc_cntrl_info assoc_cntrl;
	WBD_ENTER();

	/* Block only if the time period is provided */
	if (blk_time <= 0) {
		goto end;
	}

	/* send the client association control messages */
	memset(&assoc_cntrl, 0, sizeof(assoc_cntrl));
	memcpy(assoc_cntrl.source_bssid, source_bssid, sizeof(assoc_cntrl.source_bssid));
	memcpy(assoc_cntrl.trgt_bssid, trgt_bssid, sizeof(assoc_cntrl.trgt_bssid));
	assoc_cntrl.unblock = 0; /* Means block */
	assoc_cntrl.time_period = blk_time;
	memcpy(assoc_cntrl.sta_mac, sta_mac, sizeof(assoc_cntrl.sta_mac));

	ieee1905_send_client_association_control(&assoc_cntrl);

end:
	WBD_EXIT();
}

/* Do legacy steering */
static void
wbd_do_legacy_sta_steer(wbd_slave_item_t *slave, ieee1905_steer_req *steer_req,
	ieee1905_sta_list *sta_info, ieee1905_bss_list *bss_info)
{
	int blk_time = slave->parent->parent->max.tm_blk_sta;
	struct ether_addr sta_mac;
	WBD_ENTER();

	memcpy(&sta_mac, sta_info->mac, sizeof(sta_mac));

	wbd_slave_send_assoc_control(blk_time, steer_req->source_bssid, bss_info->bssid, &sta_mac);

	/* Block only if the time period is provided */
	if (blk_time > 0) {
		/* Block the STA on source BSSID */
		wl_wlif_block_mac(slave->wlif_hdl, sta_mac, blk_time);
	}

	/* Deauth the STA from current BSS */
	blanket_deauth_sta(slave->wbd_ifr.ifr.ifr_name, &sta_mac, 0);

	WBD_EXIT();
}

/* Send Steer Response Report message */
static void
wbd_slave_send_steer_resp_report_cmd(unsigned char *neighbor_al_mac,
	wbd_cmd_steer_resp_rpt_t *steer_resp_rpt)
{
	int ret = WBDE_OK;
	ieee1905_vendor_data vndr_msg_data;

	WBD_ENTER();
	/* Fill vndr_msg_data struct object to send Vendor Message */
	memset(&vndr_msg_data, 0x00, sizeof(vndr_msg_data));
	memcpy(vndr_msg_data.neighbor_al_mac, neighbor_al_mac, IEEE1905_MAC_ADDR_LEN);

	/* Allocate Dynamic mem for Vendor data from App */
	vndr_msg_data.vendorSpec_msg =
		(unsigned char *)wbd_malloc(IEEE1905_MAX_VNDR_DATA_BUF, &ret);
	WBD_ASSERT();

	WBD_INFO("Send Steer Response Report from Device["MACDBG"] to Device["MACDBG"]\n",
		MAC2STRDBG(ieee1905_get_al_mac()), MAC2STRDBG(vndr_msg_data.neighbor_al_mac));

	/* Encode Vendor Specific TLV for Message : Steer Response Report to send */
	ret = wbd_tlv_encode_steer_resp_report(steer_resp_rpt, vndr_msg_data.vendorSpec_msg,
		&vndr_msg_data.vendorSpec_len);
	WBD_ASSERT_MSG("Failed to encode Steer Response Report which needs to be sent "
		"from Device["MACDBG"] to Device["MACDBG"]\n",
		MAC2STRDBG(ieee1905_get_al_mac()), MAC2STRDBG(vndr_msg_data.neighbor_al_mac));

	/* Send Vendor Specific Message : Steer Response report */
	ret = ieee1905_send_vendor_specific_msg(&vndr_msg_data);
	WBD_CHECK_ERR_MSG(WBDE_DS_1905_ERR, "Failed to send "
		"Steer Response Report from Device["MACDBG"] to Device["MACDBG"], Error : %s\n",
		MAC2STRDBG(ieee1905_get_al_mac()), MAC2STRDBG(vndr_msg_data.neighbor_al_mac),
		wbderrorstr(ret));
end:
	if (vndr_msg_data.vendorSpec_msg) {
		free(vndr_msg_data.vendorSpec_msg);
	}

	WBD_EXIT();
}

/* Do Mandate and Opportunity steer */
void
wbd_do_map_steer(wbd_slave_item_t *slave, ieee1905_steer_req *steer_req,
	ieee1905_sta_list *sta_info, ieee1905_bss_list *bss_info, int btm_supported,
	uint8 is_retry)
{
	uint bw  = 0;
	int steer_resp = 0;
	wl_wlif_bss_trans_data_t ioctl_data;
	wl_wlif_bss_trans_resp_t out_resp;
	ieee1905_btm_report btm_report;
	i5_dm_clients_type *sta = NULL;
	i5_dm_device_type *i5_self_device = NULL;
	wbd_assoc_sta_item_t *assoc_sta = NULL;
	i5_dm_bss_type *i5_tbss;
	wbd_bss_item_t *bss_vndr_data = NULL;
	int ret = WBDE_OK;
	uint16 bcn_prd = 0;
	WBD_ENTER();

	WBD_SAFE_GET_I5_SELF_DEVICE(i5_self_device, &ret);

	sta = wbd_ds_find_sta_in_device(i5_self_device,	sta_info->mac, &ret);
	WBD_ASSERT_MSG("STA["MACDBG"]. %s\n", MAC2STRDBG(sta_info->mac), wbderrorstr(ret));
	WBD_ASSERT_ARG(sta->vndr_data, WBDE_INV_ARG);

#ifdef BCM_APPEVENTD
	/* Send steer start event to appeventd. */
	{
		int rssi = WBD_RCPI_TO_RSSI(sta->link_metric.rcpi);
		wbd_appeventd_steer_start(APP_E_WBD_SLAVE_STEER_START, slave->wbd_ifr.ifr.ifr_name,
			(struct ether_addr*)&sta_info->mac, (struct ether_addr*)&bss_info->bssid,
			rssi, 0);
	}
#endif /* BCM_APPEVENTD */

	assoc_sta = (wbd_assoc_sta_item_t*)sta->vndr_data;
	assoc_sta->status = WBD_STA_STATUS_WEAK_STEERING;

	/* If the BTM not supported, do legacy steering */
	if (!btm_supported) {
		wbd_do_legacy_sta_steer(slave, steer_req, sta_info, bss_info);
		goto end;
	}

	i5_tbss = wbd_ds_get_i5_bss_in_topology(bss_info->bssid, &ret);
	if (i5_tbss) {
		bss_vndr_data = (wbd_bss_item_t*)i5_tbss->vndr_data;
	}

	/* Fill the bss-trans data. */
	memset(&ioctl_data, 0, sizeof(ioctl_data));
	ioctl_data.rclass = bss_info->target_op_class;

	/* IF target bssid is wildcard bssid then ignored validate Rclass value and channel */
	if (!i5DmIsMacWildCard(bss_info->bssid)) {
		ret = blanket_get_bw_from_rc(bss_info->target_op_class, &bw);
		if (ret != WBDE_OK) {
			WBD_ERROR("Invalid Rclass[0x%x] exit \n", bss_info->target_op_class);
			goto end;
		}
		ioctl_data.chanspec = wf_channel2chspec(bss_info->target_channel, bw);

		if (!blanket_is_global_rclass_supported(slave->wbd_ifr.ifr.ifr_name,
			(struct ether_addr*)&(sta_info->mac))) {
			blanket_get_rclass(slave->wbd_ifr.ifr.ifr_name, ioctl_data.chanspec,
				&(ioctl_data.rclass));
		}
		/* Block the STAs in all the BSS except current and traget */
		wbd_slave_send_assoc_control(slave->parent->parent->max.tm_blk_sta,
			steer_req->source_bssid, bss_info->bssid,
			(struct ether_addr*)sta_info->mac);
	}

	memcpy(&(ioctl_data.bssid), &bss_info->bssid, sizeof(ioctl_data.bssid));
	memcpy(&(ioctl_data.addr), &sta_info->mac, sizeof(ioctl_data.addr));
	ioctl_data.timeout = slave->parent->parent->max.tm_blk_sta;
	ioctl_data.bssid_info = bss_vndr_data ? bss_vndr_data->bssid_info : 0;
	ioctl_data.phytype = bss_vndr_data ? bss_vndr_data->phytype : 0;
	ioctl_data.reason = bss_info->reason_code;
	if (IEEE1905_IS_STEER_MANDATE(steer_req->request_flags)) {
		if (steer_req->request_flags & IEEE1905_STEER_FLAGS_BTM_ABRIDGED) {
			ioctl_data.flags |= WLIFU_BSS_TRANS_FLAGS_BTM_ABRIDGED;
		}
		if (steer_req->request_flags & IEEE1905_STEER_FLAGS_DISASSOC_IMNT) {
			ioctl_data.flags |= WLIFU_BSS_TRANS_FLAGS_DISASSOC_IMNT;
		}
		/* Disassoc timer from controller is in Time units and in BTM request it should be
		 * in TBTT
		 */
		blanket_get_beacon_period(slave->wbd_ifr.ifr.ifr_name, &bcn_prd);
		/* If the beacon period is 0, set it to 100 */
		if (bcn_prd == 0) {
			bcn_prd = 100;
			WBD_WARNING("In Slave["MACF"] Beacon period returned 0. Set it to default 100\n",
				ETHER_TO_MACF(slave->wbd_ifr.bssid));
		}
		ioctl_data.disassoc_timer = (uint16)(steer_req->dissassociation_timer / bcn_prd);
	} else {
		ioctl_data.flags |= WLIFU_BSS_TRANS_FLAGS_BTM_ABRIDGED;
		ioctl_data.disassoc_timer = 0x0000;
	}

	/* Steer the client */
	memset(&out_resp, 0, sizeof(out_resp));
	steer_resp = wl_wlif_do_bss_trans(slave->wlif_hdl, &ioctl_data,
		slave->parent->parent->event_steer_fd, &out_resp);
	WBD_STEER("Band[%d] In slave["MACF"] Steer STA["MACF"] Steer_Resp[%d] and "
		"response_valid[%d] Status[%d] BSSID["MACF"]\n", slave->band,
		ETHER_TO_MACF(slave->wbd_ifr.bssid), ETHER_TO_MACF(ioctl_data.addr),
		steer_resp, out_resp.response_valid, out_resp.status,
		ETHER_TO_MACF(out_resp.trgt_bssid));

	/* Update steer fail count */
	if (steer_resp == WLIFU_BSS_TRANS_RESP_ACCEPT) {
		assoc_sta->steer_fail_count = 0;
	} else {
		assoc_sta->steer_fail_count++;

		/* If steer response is not accept and steer retry is required
		 * create the timer to retry the STEER process
		 */
		if (slave->parent->parent->steer_retry_config.retry_count != 0) {
			/* Create Timer for retrying the steer */
			wbd_slave_create_steer_retry_timer(slave, sta, steer_req,
				bss_info, btm_supported);
		}
	}

#ifdef BCM_APPEVENTD
	/* Send steer response to appeventd */
	wbd_appeventd_steer_resp(APP_E_WBD_SLAVE_STEER_RESP, slave->wbd_ifr.ifr.ifr_name,
		(struct ether_addr*)&sta_info->mac, steer_resp);
#endif /* BCM_APPEVENTD */

	/* If the response is valid and its Mandate steer send BTM report to controller */
	if (!is_retry && IEEE1905_IS_STEER_MANDATE(steer_req->request_flags)) {
		if (out_resp.response_valid) {
			memset(&btm_report, 0, sizeof(btm_report));

			memcpy(&btm_report.neighbor_al_mac, steer_req->neighbor_al_mac,
				sizeof(btm_report.neighbor_al_mac));
			btm_report.request_flags = steer_req->request_flags;
			memcpy(&btm_report.source_bssid, &steer_req->source_bssid,
				sizeof(btm_report.source_bssid));
			memcpy(&btm_report.sta_mac, &sta_info->mac,
				sizeof(btm_report.sta_mac));
			btm_report.status = out_resp.status;
			memcpy(&btm_report.trgt_bssid, &out_resp.trgt_bssid,
				sizeof(btm_report.trgt_bssid));
			ieee1905_send_btm_report(&btm_report);
		} else {
			wbd_cmd_steer_resp_rpt_t steer_resp_rpt;
			memset(&steer_resp_rpt, 0, sizeof(steer_resp_rpt));
			memcpy(&steer_resp_rpt.bssid, &steer_req->source_bssid,
				sizeof(steer_resp_rpt.bssid));
			memcpy(&steer_resp_rpt.sta_mac, &sta_info->mac,
				sizeof(steer_resp_rpt.sta_mac));
			wbd_slave_send_steer_resp_report_cmd(steer_req->neighbor_al_mac,
				&steer_resp_rpt);
		}
	}

end:
	WBD_EXIT();
}

/* Get the BSS for the STA to steer */
static ieee1905_bss_list*
wbd_slave_map_get_bss_from_sta_index(ieee1905_steer_req *steer_req, uint8 sta_index)
{
	uint8 bss_index = 1;
	dll_t *item_p = NULL;
	ieee1905_bss_list *bssInfo = (ieee1905_bss_list*)dll_head_p(&steer_req->bss_list.head);

	if (steer_req->bss_list.count == 1) {
		return bssInfo;
	}

	/* Empty for loop to traverse through */
	for (item_p = dll_head_p(&steer_req->bss_list.head);
		!dll_end(&steer_req->bss_list.head, item_p) && bss_index < sta_index;
		item_p = dll_next_p(item_p), bss_index++) {

		bssInfo = (ieee1905_bss_list*)item_p;
	}

	if (item_p != NULL) {
		return bssInfo;
	}

	return NULL;
}

/* Check all the steering condition */
static int
wbd_slave_map_check_steering_condition(wbd_slave_item_t *slave, ieee1905_steer_req *steer_req,
	ieee1905_sta_list *sta_info, int *btm_supported)
{
	if (IEEE1905_IS_STEER_OPPORTUNITY(steer_req->request_flags) ||
		IEEE1905_IS_STEER_RSSI(steer_req->request_flags)) {
		/* In steering opportunity or RSSI bases, STAs mac address should not be in
		 * Local Steering Disallowed STA List
		 */
		if (ieee1905_is_sta_in_local_steering_disallowed_list(sta_info->mac)) {
			WBD_STEER("Band[%d] In slave["MACF"] STA["MACDBG"] request_flags[%d] "
				"in Local Steering Disallowed STA List\n",
				slave->band, ETHER_TO_MACF(slave->wbd_ifr.bssid),
				MAC2STRDBG(sta_info->mac), steer_req->request_flags);
			return 0;
		}
	}

	if (btm_supported) {
		/* In BTM Supported STAs mac address should not be in
		 * BTM Steering Disallowed STA list. If there do legacy based steering
		 */
		if (ieee1905_is_sta_in_BTM_steering_disallowed_list(sta_info->mac)) {
			WBD_STEER("Band[%d] In slave["MACF"] STA["MACDBG"] btm_supported[%d] "
				" in BTM steering Disallowed list\n",
				slave->band, ETHER_TO_MACF(slave->wbd_ifr.bssid),
				MAC2STRDBG(sta_info->mac), *btm_supported);
			*btm_supported = 0;
			return 1;
		}
	}

	/* All conditions met. Ready to steer */
	return 1;
}
/* Callback from IEEE 1905 module when the agent gets STEER request */
void
wbd_slave_process_map_steer_request_cb(wbd_info_t *info, ieee1905_steer_req *steer_req,
	ieee1905_vendor_data *in_vndr_tlv)
{
	uint8 sta_index = 1;
	int ret = WBDE_OK;
	wbd_slave_item_t *slave = NULL;
	ieee1905_sta_list *staInfo;
	ieee1905_bss_list *bssInfo = NULL;
	dll_t *item_p, *next_p;
	int btm_supported = 1;
	WBD_ENTER();

	WBD_INFO("Callback from IEEE1905 Source BSSID["MACDBG"] Request Flag[0x%X] "
		"Opportunity[%d] DissassocTimer[%d]\n", MAC2STRDBG(steer_req->source_bssid),
		steer_req->request_flags, steer_req->opportunity_window,
		steer_req->dissassociation_timer);

	/* Choose Slave Info based on mac */
	WBD_SAFE_GET_SLAVE_ITEM(info, (struct ether_addr*)&steer_req->source_bssid, WBD_CMP_BSSID,
		slave, (&ret));

	/* If vendor TLV decode it */
	if (in_vndr_tlv) {
		wbd_tlv_decode_vendor_steer_request(NULL, in_vndr_tlv->vendorSpec_msg,
			in_vndr_tlv->vendorSpec_len);
	}

	for (item_p = dll_head_p(&steer_req->sta_list.head);
		!dll_end(&steer_req->sta_list.head, item_p);
		item_p = next_p, sta_index++) {

		next_p = dll_next_p(item_p);
		staInfo = (ieee1905_sta_list*)item_p;

		if (WBD_WNM_CHECK_ENAB(info->flags)) {
			btm_supported = blanket_is_bss_trans_supported(slave->wbd_ifr.ifr.ifr_name,
				(struct ether_addr*)staInfo->mac);
		}

		WBD_INFO("Band[%d] In slave["MACF"] Steer STA["MACDBG"] btm_supported[%d]\n",
			slave->band, ETHER_TO_MACF(slave->wbd_ifr.bssid),
			MAC2STRDBG(staInfo->mac), btm_supported);
		/* If all the steering conditions not met dont steer */
		if (!wbd_slave_map_check_steering_condition(slave, steer_req, staInfo,
			&btm_supported)) {
			WBD_INFO("Band[%d] In slave["MACF"] Steer STA["MACDBG"] "
				"Conditions not met\n",
				slave->band, ETHER_TO_MACF(slave->wbd_ifr.bssid),
				MAC2STRDBG(staInfo->mac));
			continue;
		}
		bssInfo = wbd_slave_map_get_bss_from_sta_index(steer_req, sta_index);
		wbd_do_map_steer(slave, steer_req, staInfo, bssInfo, btm_supported, 0);
	}

	/* In case of steer opportunity send the steer completed */
	if (IEEE1905_IS_STEER_OPPORTUNITY(steer_req->request_flags)) {
		WBD_INFO("Band[%d] In slave["MACF"] Send Steering Completed Message To["MACDBG"]\n",
			slave->band, ETHER_TO_MACF(slave->wbd_ifr.bssid),
			MAC2STRDBG(steer_req->neighbor_al_mac));
		ieee1905_send_steering_completed_message(steer_req);
	}

end:
	if (ret != 0) {
		WBD_WARNING("Source BSSID["MACDBG"] Request Flag[0x%X] "
			"Opportunity[%d] DissassocTimer[%d]. Steering Request Failed[%s]\n",
			MAC2STRDBG(steer_req->source_bssid), steer_req->request_flags,
			steer_req->opportunity_window, steer_req->dissassociation_timer,
			wbderrorstr(ret));
	}
	WBD_EXIT();
}

#if defined(MULTIAPR2)
/* Prepare STEER request structure for the BTM query from the STA and send STEER */
void
wbd_slave_send_bss_trans_for_btm_query(struct ether_addr *sta_mac)
{
	int ret = WBDE_OK;
	ieee1905_steer_req steer_req;
	ieee1905_sta_list *sta_info;
	ieee1905_bss_list *bss_info;
	i5_dm_interface_type *pdmif;
	i5_dm_bss_type *pdmbss;
	i5_dm_clients_type *pdmclient;
	WBD_ENTER();

	memset(&steer_req, 0, sizeof(steer_req));

	/* Initialize sta and bss list */
	ieee1905_glist_init(&steer_req.sta_list);
	ieee1905_glist_init(&steer_req.bss_list);
	steer_req.request_flags |= IEEE1905_STEER_FLAGS_MANDATE;

	/* Add STA info */
	sta_info = (ieee1905_sta_list*)wbd_malloc(sizeof(*sta_info), &ret);
	WBD_ASSERT();

	memcpy(sta_info->mac, sta_mac, MAC_ADDR_LEN);
	ieee1905_glist_append(&steer_req.sta_list, (dll_t*)sta_info);

	/* Add Local BSS */
	bss_info = (ieee1905_bss_list*)wbd_malloc(sizeof(*bss_info), &ret);
	WBD_ASSERT();

	/* Get the BSS for the STA by finding the STA in current device and getting its parent */
	pdmclient = i5DmFindClientInDevice(i5DmGetSelfDevice(), (unsigned char*)sta_mac);
	if (pdmclient == NULL) {
		WBD_ERROR("STA["MACF"] Not found\n", ETHERP_TO_MACF(sta_mac));
		goto end;
	}
	pdmbss = I5LL_PARENT(pdmclient);
	pdmif = I5LL_PARENT(pdmbss);
	memcpy(bss_info->bssid, pdmbss->BSSID, MAC_ADDR_LEN);
	memcpy(steer_req.source_bssid, pdmbss->BSSID, MAC_ADDR_LEN);
	bss_info->target_op_class = pdmif->opClass;
	bss_info->target_channel = wf_chspec_ctlchan(pdmif->chanspec);
	bss_info->reason_code = 9; /* Transitioning to a premium AP */
	ieee1905_glist_append(&steer_req.bss_list, (dll_t*)bss_info);

	wbd_slave_process_map_steer_request_cb(wbd_get_ginfo(), &steer_req, NULL);

end:
	i5DmSteerRequestInfoFree(&steer_req);
	WBD_EXIT();
}
#endif /* MULTIAPR2 */

/* Callback from IEEE 1905 module when the agent gets block/unblock STA request */
void
wbd_slave_process_map_block_unblock_sta_request_cb(wbd_info_t *info,
	ieee1905_block_unblock_sta *block_unblock_sta)
{
	int ret = WBDE_OK;
	wbd_slave_item_t *slave = NULL;
	ieee1905_sta_list *staInfo;
	dll_t *item_p, *next_p;
	struct ether_addr sta;
	WBD_ENTER();

	WBD_INFO("Callback from IEEE1905 for blocking STAs on BSSID["MACDBG"]\n",
		MAC2STRDBG(block_unblock_sta->source_bssid));

	/* Choose Slave Info based on mac */
	WBD_SAFE_GET_SLAVE_ITEM(info, (struct ether_addr*)&block_unblock_sta->source_bssid,
		WBD_CMP_BSSID, slave, (&ret));

	for (item_p = dll_head_p(&block_unblock_sta->sta_list.head);
		!dll_end(&block_unblock_sta->sta_list.head, item_p);
		item_p = next_p) {

		next_p = dll_next_p(item_p);
		staInfo = (ieee1905_sta_list*)item_p;

		memcpy(&sta, staInfo->mac, sizeof(sta));
		if (block_unblock_sta->unblock) {
			/* UnBlock the STA on source BSSID */
			WBD_INFO("Unblocking STA["MACDBG"] on BSSID["MACDBG"]\n",
				MAC2STRDBG(staInfo->mac),
				MAC2STRDBG(block_unblock_sta->source_bssid));
			wl_wlif_unblock_mac(slave->wlif_hdl, sta, 0);
		} else {
			/* Block the STA on source BSSID */
			WBD_INFO("Blocking STA["MACDBG"] on BSSID["MACDBG"]\n",
				MAC2STRDBG(staInfo->mac),
				MAC2STRDBG(block_unblock_sta->source_bssid));
			wl_wlif_block_mac(slave->wlif_hdl, sta,
				block_unblock_sta->time_period);
		}
	}

end:
	WBD_EXIT();
}

/* Channel validity check aganist provided preference */
int wbd_slave_check_channel_validity(ieee1905_chan_pref_rc_map *rc_map, unsigned char rc_count,
	uint8 rclass, uint8 channel, uint8 min_pref)
{
	int i, j;
	int ret = TRUE;

	WBD_ENTER();
	if (!rc_map)
		goto end;

	WBD_INFO("Input: Regulatory Class:%d Channel:%d\n", rclass, channel);
	for (i = 0; i < rc_count; i++) {
		if (rc_map[i].regclass != rclass)
			continue;
		for (j = 0; j < rc_map[i].count; j++) {
			if (rc_map[i].channel[j] == channel) {
				break;
			}
		}
		if (rc_map[i].count == 0 || j < rc_map[i].count) {
			WBD_INFO("Preference Required:%d Current:%d\n",
				min_pref, rc_map[i].pref);
			if (rc_map[i].pref < min_pref) {
				ret = FALSE;
			}
			break;
		}
	}
end:
	WBD_EXIT();
	return ret;
}

/* Callback from IEEE 1905 module for channel preference report */
void wbd_slave_prepare_local_channel_preference(i5_dm_interface_type *i5_intf,
	ieee1905_chan_pref_rc_map_array *cp)
{
	int ret = WBDE_OK;
	char *buf;
	int i;
	ieee1905_radio_caps_type *RadioCaps = &i5_intf->ApCaps.RadioCaps;
	int rcaps_rc_count = RadioCaps->List ? RadioCaps->List[0] : 0, rc_allocated;
	const i5_dm_rc_chan_map_type *rc_chan_map = i5DmGetRegClassChannelMap();
	uint8 *ptr = RadioCaps->List;
	uint16 bytes_to_rd, bytes_rd;

	/* skip first byte, as it contain number of opclass */
	ptr++;

	WBD_ENTER();

	cp->rc_count = 0;
	rc_allocated = WBD_MAX_CHAN_PREF_RC_COUNT;
	buf = wbd_malloc((sizeof(ieee1905_chan_pref_rc_map) * rc_allocated), &ret);
	WBD_ASSERT();
	cp->rc_map = (ieee1905_chan_pref_rc_map *)buf;

	WBD_INFO("Prepare Local channel preference for ["MACF"]\n",
		ETHERP_TO_MACF(i5_intf->InterfaceId));

	bytes_to_rd = RadioCaps->Len;
	bytes_rd = 0;
	/* add rclass if chan info for supported channels becomes non prefered. */
	for (i = 0; (i < rcaps_rc_count) && (bytes_rd < bytes_to_rd); i++) {
		uint16 channel;
		uint16 chan_cnt;
		uint16 rclass_cnt;
		uint16 chan_bitmap[11] = {0x00};
		uint32 chanspec;
		uint bw;
		radio_cap_sub_info_t *radio_sub_info = NULL;
		ieee1905_chan_pref_rc_map *rc_map = NULL;

		radio_sub_info = (radio_cap_sub_info_t *)ptr;

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
			uint bitmap = 0x00;
			/* few channels in radio capabilities marked as non operable, exclude them
			 * while prreparing channel preference for corresponding operating class
			 */
			if (isset(chan_bitmap, rc_chan_map[rclass_cnt].channel[chan_cnt])) {
				/* skip this channel */
				continue;
			}

			blanket_get_bw_from_rc(radio_sub_info->regclass, &bw);
			if (radio_sub_info->regclass <= REGCLASS_40MHZ_LAST) {
				/* arguments are: control channel and bw */
				chanspec =
					wf_channel2chspec(
					rc_chan_map[rclass_cnt].channel[chan_cnt], bw);
			} else {
				/* arguments are centre channel and bandwidth */
				chanspec =
					CHBW_CHSPEC(
					rc_chan_map[rclass_cnt].channel[chan_cnt], bw);
			}

			FOREACH_20_SB(chanspec, channel) {
				uint bitmap_sb = 0x00;
				ret = blanket_get_chan_info(i5_intf->ifname, channel, &bitmap_sb);
				if (ret < 0) {
					WBD_ERROR("chan_info failed for chan %d\n",
						rc_chan_map[rclass_cnt].channel[chan_cnt]);
					break;
				}
				if (!(bitmap_sb & WL_CHAN_VALID_HW) ||
					!(bitmap_sb & WL_CHAN_VALID_SW)) {
					ret = -1;
					break;
				}
				if (!(bitmap_sb & (WL_CHAN_RADAR | WL_CHAN_RESTRICTED |
					WL_CHAN_PASSIVE | WL_CHAN_INACTIVE))) {
					/* Highest preference channel */
					continue;
				}
				bitmap |= bitmap_sb;
			}
			/* get preference and reason */
			if (bitmap & WL_CHAN_RADAR) {
				/* If memory is not enough for adding regulatory class entry,
				 * reallocate it. Check for already allocated count against the
				 * regulatory classes count already stored in the preference list
				 */
				if (cp->rc_count >= rc_allocated) {
					char *tmp;

					WBD_WARNING("Memory is not enough for channel preference "
						"report, reallocate. Count[%d] Allocated[%d]\n",
						cp->rc_count, rc_allocated);
					rc_allocated += 10; /* 10 more regulatory class for now */
					tmp = (char*)realloc(buf,
						(sizeof(ieee1905_chan_pref_rc_map) * rc_allocated));
					if (tmp == NULL) {
						WBD_ERROR("%s of %d\n",
							wbderrorstr(WBDE_REALLOC_FL), rc_allocated);
						goto end;
					}
					buf = tmp;
					cp->rc_map = (ieee1905_chan_pref_rc_map *)buf;
				}
				rc_map = &cp->rc_map[cp->rc_count];
				/* update channnel and count in case prefererence for any channel
				 * changed due to radar
				 */
				rc_map->channel[rc_map->count] =
					rc_chan_map[rclass_cnt].channel[chan_cnt];
				rc_map->count++;
				if (bitmap & (WL_CHAN_INACTIVE | WL_CHAN_RESTRICTED)) {
					rc_map->pref = IEEE1905_CHAN_PREF_NON_OP;
					rc_map->reason = (bitmap & WL_CHAN_INACTIVE) ?
						I5_REASON_RADAR : I5_REASON_UNSPECFIED;
				} else {
					rc_map->pref = IEEE1905_CHAN_PREF_14;
					rc_map->reason = (bitmap & WL_CHAN_PASSIVE) ?
						I5_REASON_DFS_PASSIVE : I5_REASON_DFS_CAC_COMPLETE;
				}

				rc_map->regclass = radio_sub_info->regclass;
				cp->rc_count++;
			}
		}
		ptr += sizeof(radio_cap_sub_info_t) + radio_sub_info->n_chan;
		bytes_rd += sizeof(radio_cap_sub_info_t) + radio_sub_info->n_chan;
	}
end:
	WBD_EXIT();
}

/* Callback from IEEE 1905 module when the agent gets Channel Selection request */
void
wbd_slave_process_chan_selection_request_cb(wbd_info_t *info, unsigned char *al_mac,
	unsigned char *interface_mac, ieee1905_chan_pref_rc_map_array *cp,
	unsigned char rclass_local_count, ieee1905_chan_pref_rc_map *local_chan_pref)
{
	wbd_slave_item_t *slave = NULL;
	wl_uint32_list_t *chlist = NULL;
	i5_dm_interface_type *pdmif;
	ieee1905_radio_caps_type *RadioCaps;
	int buflen;
	int ret = WBDE_OK;
	int i, j, k;
	int is_valid = 0;
	int min_pref = IEEE1905_CHAN_PREF_15;
	int rcidx;
	int rcaps_rc_count;
	uint8 channel;

	WBD_ENTER();

	if (!cp || !al_mac || !interface_mac) {
		WBD_ERROR("NULL data channel selection request OR NULL device/pdmif\n");
		goto end;
	}
	WBD_SAFE_GET_SLAVE_ITEM(info, (struct ether_addr*)interface_mac, WBD_CMP_BSSID,
		slave, (&ret));
	pdmif = wbd_ds_get_i5_interface(al_mac, interface_mac, &ret);
	WBD_ASSERT();

	WBD_INFO("Received channel selection request for interface ["MACF"]\n",
		ETHERP_TO_MACF(interface_mac));
	RadioCaps = &pdmif->ApCaps.RadioCaps;
	rcaps_rc_count = RadioCaps->List ? RadioCaps->List[0]: 0;

	blanket_get_global_rclass(pdmif->chanspec, &pdmif->opClass);
	if (CHSPEC_IS80(pdmif->chanspec) || CHSPEC_IS160(pdmif->chanspec)) {
		channel = CHSPEC_CHANNEL(pdmif->chanspec);
	} else {
		channel = wf_chspec_ctlchan(pdmif->chanspec);
	}
	is_valid = wbd_slave_check_channel_validity(cp->rc_map, cp->rc_count,
		pdmif->opClass, channel, IEEE1905_CHAN_PREF_1);
	if (is_valid) {
		WBD_INFO("Current channel is valid\n");
		goto end;
	}

	/* Get list of chanspecs */
	buflen = (MAXCHANNEL + 1) * sizeof(uint32);

	chlist =  (wl_uint32_list_t *)wbd_malloc(buflen, &ret);
	WBD_ASSERT_MSG(" Failed to allocate memory for chanspecs\n");

	ret = blanket_get_chanspecs_list(pdmif->ifname, chlist, buflen);
	if ((ret != WBDE_OK) || !chlist->count) {
		WBD_ERROR("Getting chanspecs failed\n");
		goto end;
	}
setchannel:
	/* Starting with last chanspec so that higher bandwidth chanspec is considered first */
	for (i = chlist->count - 1; i >= 0; i--) {
		uint8 rclass;
		int invalid = 1;

		if (CHSPEC_IS80(pdmif->chanspec) || CHSPEC_IS160(pdmif->chanspec)) {
			channel = CHSPEC_CHANNEL(chlist->element[i]);
		} else {
			channel = wf_chspec_ctlchan(chlist->element[i]);
		}

		if (blanket_get_global_rclass(chlist->element[i], &rclass) != WBDE_OK) {
			WBD_INFO("Failed to get rclass for chanspec: 0x%x\n", chlist->element[i]);
			continue;
		}
		if (rclass < 81 || rclass > 130) {
			WBD_INFO("Invalid regclass: %d for chanspec: 0x%x\n",
				rclass, chlist->element[i]);
			continue;
		}

		for (j = 0, rcidx = 1; j < rcaps_rc_count && rcidx < RadioCaps->Len; j++) {
			int ch_count;
			uint8 rc = RadioCaps->List[rcidx++];

			rcidx++; /* Max Transmit power */
			ch_count = RadioCaps->List[rcidx++];

			if (rc != rclass) {
				rcidx += ch_count;
				continue;
			}

			invalid  = 0;
			for (k = 0; k < ch_count && rcidx < RadioCaps->Len; k++, rcidx++) {
				if (RadioCaps->List[rcidx] == channel) {
					invalid = 1;
					break;
				}
			}
			break;
		}
		if (invalid) {
			continue;
		}
		WBD_INFO("regclass: %d chanspec[%d]: 0x%x\n", rclass, i, chlist->element[i]);

		/* check channel validity in local preference list */
		is_valid = wbd_slave_check_channel_validity(local_chan_pref,
			rclass_local_count, rclass, channel, min_pref);
		if (!is_valid)
			continue;

		/* check channel validity in master preference list */
		is_valid = wbd_slave_check_channel_validity(cp->rc_map,
			cp->rc_count, rclass, channel, min_pref);

		if (is_valid) {
			WBD_INFO("Found valid channel. (Regulatory Class: %d, Chanspec: 0x%x)\n",
				rclass, chlist->element[i]);
			/* Set chanspec through ACSD deamon running on ACCESS POINT */
			wbd_slave_set_chanspec_through_acsd(slave,
				chlist->element[i], WBD_REASON_OTHER, pdmif);
			break;
		}
	}
	if (i < 0) {
		/* Reduce Preference and try again */
		min_pref--;
		if (min_pref >= 0) {
			goto setchannel;
		} else {
			WBD_ERROR("No valid channels found. skipping channel switch\n");
		}
	}
end:
	if (chlist)
		free(chlist);
	WBD_EXIT();
}

/* Callback from IEEE 1905 module when the agent gets set tx power limit */
extern void wbd_slave_process_set_tx_power_limit_cb(wbd_info_t *info,
	char *ifname, unsigned char tx_power_limit)
{
	int pwr_percent, max_pwr, cur_pwr, old_pwr_percent;

	WBD_ENTER();

	if (!ifname) {
		WBD_ERROR("NULL ifname - Set tx power limit\n");
		goto end;
	}

	/* Get the Maximum Tx Power of Slave */
	blanket_get_txpwr_target_max(ifname, &cur_pwr);

	blanket_get_pwr_percent(ifname, &old_pwr_percent);

	max_pwr = cur_pwr * 100 / old_pwr_percent;

	if (tx_power_limit >= max_pwr) {
		pwr_percent = 100;
	} else {
		pwr_percent = tx_power_limit * 100 / max_pwr;
	}
	WBD_INFO("ifname: %s cur pwr: %d max pwr: %d old_pwr_percent : %d pwr_percent : %d "
		"tx_power_limit:%d\n",
		ifname, cur_pwr, max_pwr, old_pwr_percent, pwr_percent, tx_power_limit);

	/* Set only if there is any change */
	if (old_pwr_percent != pwr_percent) {
		blanket_set_pwr_percent(ifname, pwr_percent);
	}
end:
	WBD_EXIT();
}

/* Callback from IEEE 1905 module to get the backhual link metric */
int wbd_slave_process_get_backhaul_link_metric_cb(wbd_info_t *info, char *ifname,
	unsigned char *interface_mac, ieee1905_backhaul_link_metric *metric)
{
	int ret = WBDE_OK;
	sta_info_t sta_info;
	int rssi = 0, rate;
	char is_dwds = 0;
	blanket_counters_t counters;
	char prefix[IFNAMSIZ] = {0};
	unsigned short link_available = 0;

	/* Get prefix of the interface from Driver */
	blanket_get_interface_prefix(ifname, prefix, sizeof(prefix));

	if (strcmp(blanket_nvram_prefix_safe_get(prefix, "mode"), "sta")) {
		is_dwds = 1;
	}

	/* If it is DWDS AP */
	if (is_dwds) {
		ret = blanket_get_rssi(ifname, (struct ether_addr*)interface_mac, &rssi);
		WBD_ASSERT();

		/* get other stats from STA_INFO iovar */
		ret = blanket_get_sta_info(ifname, (struct ether_addr*)interface_mac, &sta_info);
		WBD_ASSERT();

		metric->txPacketErrors = (unsigned int)sta_info.tx_failures;
		metric->transmittedPackets = (unsigned int)sta_info.tx_tot_pkts;
		metric->receivedPackets = (unsigned int)sta_info.rx_tot_pkts;

		metric->macThroughPutCapacity = (unsigned short)(sta_info.tx_rate / 1000);
	} else {
		/* Its a DWDS STA */
		ret = blanket_get_rssi(ifname, NULL, &rssi);
		memset(&counters, 0, sizeof(counters));
		ret = blanket_get_counters(ifname, &counters);
		WBD_ASSERT();

		metric->txPacketErrors = (unsigned int)counters.txerror;
		metric->transmittedPackets = (unsigned int)counters.txframe;
		metric->receivedPackets = (unsigned int)counters.rxframe;
		metric->rxPacketErrors = (unsigned int)counters.rxerror;

		/* get rate */
		blanket_get_rate(ifname, &rate);
		metric->macThroughPutCapacity = (unsigned short)(rate);
	}

	metric->rcpi = (unsigned char)WBD_RSSI_TO_RCPI(rssi);
	wbd_wl_get_link_availability(ifname, &link_available);
	metric->linkAvailability = (link_available < 100) ? link_available : 100;

	WBD_INFO("Ifname[%s] MAC["MACDBG"] DWDS %s txPacketErrors %d transmittedPackets %d "
		"receivedPackets %d rxPacketErrors %d rssi %d macThroughPutCapacity %d\n",
		ifname, MAC2STRDBG(interface_mac), (is_dwds ? "AP" : "STA"), metric->txPacketErrors,
		metric->transmittedPackets, metric->receivedPackets, metric->rxPacketErrors,
		metric->rcpi, metric->macThroughPutCapacity);

end:
	return ret;
}

void
wbd_slave_process_bh_steer_request_cb(wbd_info_t *info, char *ifname,
	ieee1905_backhaul_steer_msg *bh_steer_req)
{
	int ret = WBDE_OK;
	wl_join_params_t *join_params = NULL;
	uint join_params_size;
	uint bw = 0;
	char prefix[IFNAMSIZ] = {0};
	wbd_bh_steer_check_arg_t *param_cb = NULL;
	i5_dm_interface_type *i5_ifr;
	wbd_ifr_item_t *ifr_vndr_data;
	WBD_ENTER();

	if (!bh_steer_req) {
		WBD_ERROR("NULL backhaul steer request received\n");
		goto end;
	}

	if ((i5_ifr = wbd_ds_get_self_i5_ifr_from_ifname(ifname, NULL)) == NULL) {
		WBD_WARNING("Self Interface not found for ifname[%s]\n", ifname);
		goto end;
	}

	WBD_ASSERT_ARG(i5_ifr->vndr_data, WBDE_INV_ARG);
	ifr_vndr_data = (wbd_ifr_item_t*)i5_ifr->vndr_data;

	/* Get prefix of the interface from Driver */
	blanket_get_interface_prefix(ifname, prefix, sizeof(prefix));

	if (strcmp(blanket_nvram_prefix_safe_get(prefix, NVRAM_MODE), "sta")) {
		WBD_ERROR("Not a STA inteface\n");
		goto free_arg;
	}

	if (blanket_get_config_val_uint(prefix, NVRAM_MAP, 0) != IEEE1905_MAP_FLAG_STA) {
		WBD_ERROR("Interface not mapped as STA\n");
		goto free_arg;
	}

	WBD_INFO("Received callback: Backhaul STA["MACDBG"] is moving to BSSID["MACDBG"]\n",
		MAC2STRDBG(bh_steer_req->bh_sta_mac), MAC2STRDBG(bh_steer_req->trgt_bssid));

	if (WBD_IS_HAPD_ENABLED(info->flags)) {
		char cmd[WBD_MAX_BUF_128] = {0};
		uint start_factor, band, frequency;

		snprintf(cmd, sizeof(cmd), "wpa_cli -p %s/%swpa_supplicant set_network 0 "
			"bssid "MACDBG"",
			WBD_HAPD_SUPP_CTRL_DIR, prefix, MAC2STRDBG(bh_steer_req->trgt_bssid));
		WBD_INFO("%s\n", cmd);
		system(cmd);

		snprintf(cmd, sizeof(cmd), "wpa_cli -p %s/%swpa_supplicant save_config",
			WBD_HAPD_SUPP_CTRL_DIR, prefix);
		WBD_INFO("%s\n", cmd);
		system(cmd);

		/* If the AP scan is disabled enable it */
		if (ifr_vndr_data->flags & WBD_FLAGS_IFR_AP_SCAN_DISABLED) {
			snprintf(cmd, sizeof(cmd), "wpa_cli -p %s/%swpa_supplicant -i%s ap_scan 1",
				WBD_HAPD_SUPP_CTRL_DIR, prefix, ifname);
			WBD_INFO("%s\n", cmd);
			system(cmd);
			ifr_vndr_data->flags &= ~WBD_FLAGS_IFR_AP_SCAN_DISABLED;
		}

		/* For scan on particualr channel, for wpa supplicant we should pass frequency.
		 * For getting the frequency we should pass control channela and start_factor.
		 * start_factor depends on band. So get the band and from band get start_factor
		 */
		band = WBD_BAND_FROM_CHANNEL(bh_steer_req->channel);
		if (WBD_IS_BAND_5G(band)) {
			start_factor = WF_CHAN_FACTOR_5_G;
		} else {
			start_factor = WF_CHAN_FACTOR_2_4_G;
		}
		frequency = wf_channel2mhz(bh_steer_req->channel, start_factor);

		snprintf(cmd, sizeof(cmd), "wpa_cli -p %s/%swpa_supplicant -i%s scan freq=%u "
			"bssid="MACDBG"",
			WBD_HAPD_SUPP_CTRL_DIR, prefix, ifname,
			frequency, MAC2STRDBG(bh_steer_req->trgt_bssid));
		WBD_INFO("%s\n", cmd);
		system(cmd);
	} else {
		/* allocate the storage */
		join_params_size = WL_JOIN_PARAMS_FIXED_SIZE + sizeof(chanspec_t);
		if ((join_params = malloc(join_params_size)) == NULL) {
			WBD_ERROR("Error allocating %d bytes for assoc params\n", join_params_size);
			goto free_arg;
		}

		/* Join SSID with assoc params */
		memset(join_params, 0, join_params_size);
		blanket_get_ssid(ifname, &join_params->ssid);
		memcpy(&join_params->params.bssid, &bh_steer_req->trgt_bssid, ETHER_ADDR_LEN);
		join_params->params.bssid_cnt = 0;
		join_params->params.chanspec_num = 1;
		blanket_get_bw_from_rc(bh_steer_req->opclass, &bw);
		join_params->params.chanspec_list[0] = wf_channel2chspec(bh_steer_req->channel, bw);
		WBD_INFO("Joining SSID: %s BSSID:["MACF"] chanspec: 0x%x\n", join_params->ssid.SSID,
			ETHER_TO_MACF(join_params->params.bssid),
			join_params->params.chanspec_list[0]);
		blanket_join_ssid(ifname, join_params, join_params_size);
		free(join_params);
	}
	if ((param_cb = malloc(sizeof(*param_cb))) == NULL) {
		WBD_ERROR("Error allocating bytes for callback params\n");
		goto free_arg;
	}
	param_cb->bh_steer_msg = bh_steer_req;
	WBDSTRNCPY(param_cb->ifname, ifname, sizeof(param_cb->ifname));
	WBDSTRNCPY(param_cb->prefix, prefix, sizeof(param_cb->prefix));
	param_cb->check_cnt = 9;
	if (wbd_add_timers(info->hdl, param_cb, WBD_SEC_MICROSEC(1),
		wbd_slave_check_bh_join_timer_cb, 1) == WBDE_OK) {
		goto end;
	} else {
		WBD_WARNING("Failed to create timer for checking if backhaul STA joined "
			"to target bssid["MACF"]\n", ETHER_TO_MACF(join_params->params.bssid));
		free(param_cb);
	}
free_arg:
	free(bh_steer_req);
end:
	WBD_EXIT();
}

/* Remove ifnames from the list */
static int wbd_slave_remove_ifname_from_list(char *vif_name, char *nvname)
{
	char interface_list[NVRAM_MAX_VALUE_LEN] = {0}, *str = NULL;
	int interface_list_size = sizeof(interface_list), ret;

	/* Remove from lan_ifnames */
	str = blanket_nvram_safe_get(nvname);
	WBDSTRNCPY(interface_list, str, interface_list_size);
	if (find_in_list(interface_list, vif_name)) {
		WBD_INFO("%s is in NVRAM %s(%s)\n", vif_name, nvname, str);
		ret = remove_from_list(vif_name, interface_list, interface_list_size);
		if (ret == 0) {
			WBD_INFO("Removed %s and new list is %s. Set %s\n", vif_name,
				interface_list, nvname);
			nvram_set(nvname, interface_list);
			return 0;
		}
	}

	return -1;
}

/* ADD the ifname to the list */
static int wbd_slave_add_ifname_to_list(char *vif_name, char *nvname)
{
	char interface_list[NVRAM_MAX_VALUE_LEN] = {0}, *str = NULL;
	int interface_list_size = sizeof(interface_list), ret = -1;

	/* Add interface to wlX_vifs, if not already */
	str = blanket_nvram_safe_get(nvname);
	WBDSTRNCPY(interface_list, str, interface_list_size);
	if (find_in_list(interface_list, vif_name)) {
		WBD_INFO("%s is already in NVRAM %s(%s)\n", vif_name, nvname, str);
		return ret;
	}
	ret = add_to_list(vif_name, interface_list, interface_list_size);
	if (ret == 0) {
		WBD_INFO("%s is added and new lits is %s. Set %s\n", vif_name, interface_list,
			nvname);
		nvram_set(nvname, interface_list);
		ret = 0;
	}

	return ret;
}

/* Erase the NVRAMs from the BSS */
static int wbd_slave_erase_bss(char *vif_name, int is_primary)
{
	char prefix[IFNAMSIZ] = {0};
	int ret = -1;

	snprintf(prefix, sizeof(prefix), "%s_", vif_name);

	/* If STA interface dont erase */
	if (strcmp(blanket_nvram_prefix_safe_get(prefix, "mode"), "sta") == 0) {
		return ret;
	}
	if (blanket_get_config_val_int(prefix, NVRAM_BSS_ENABLED, 0) != 0) {
		ret = 0;
		blanket_nvram_prefix_set(prefix, NVRAM_BSS_ENABLED, "0");
	}

	if (blanket_get_config_val_int(prefix, NVRAM_RADIO, 0) != 0) {
		ret = 0;
		blanket_nvram_prefix_set(prefix, NVRAM_RADIO, "0");
	}

	blanket_nvram_prefix_set(prefix, NVRAM_CLOSED, "0");

	/* On primary dont remove from br0_ifnames and lan_ifnames */
	if (!is_primary) {
		/* Remove from lan_ifnames */
		if (wbd_slave_remove_ifname_from_list(vif_name, NVRAM_LAN_IFNAMES) == 0) {
			ret = 0;
		}
		/* Remove from br0_ifnames */
		if (wbd_slave_remove_ifname_from_list(vif_name, NVRAM_BR0_IFNAMES) == 0) {
			ret = 0;
		}
		/* Remove from lan1_ifnames */
		if (wbd_slave_remove_ifname_from_list(vif_name, NVRAM_LAN1_IFNAMES) == 0) {
			ret = 0;
		}
		/* Remove from br1_ifnames */
		if (wbd_slave_remove_ifname_from_list(vif_name, NVRAM_BR1_IFNAMES) == 0) {
			ret = 0;
		}
	}
	/* Remove from wbd_ifnames */
	if (wbd_slave_remove_ifname_from_list(vif_name, WBD_NVRAM_IFNAMES) == 0) {
		ret = 0;
	}

	return ret;
}

/* Erase the NVRAMs from all the BSS on this interface */
static int wbd_slave_erase_all_bss(char *ifname)
{
	int unit, idx, ret = -1;
	char prefix[IFNAMSIZ] = {0};
	char lan_ifnames[NVRAM_MAX_VALUE_LEN] = {0};
	char lan1_ifnames[NVRAM_MAX_VALUE_LEN] = {0};
	char tmpvif[IFNAMSIZ] = {0};

	/* Get NVRAM value from "lan_ifnames" and "lan1_ifnames" and save it */
	WBDSTRNCPY(lan_ifnames, blanket_nvram_safe_get(NVRAM_LAN_IFNAMES), sizeof(lan_ifnames) - 1);
	WBDSTRNCPY(lan1_ifnames, blanket_nvram_safe_get(NVRAM_LAN1_IFNAMES), sizeof(lan1_ifnames) - 1);

	osifname_to_nvifname(ifname, tmpvif, sizeof(tmpvif));
	ret = wbd_slave_erase_bss(tmpvif, 1);

	/* Get Prefix from OS specific interface name */
	blanket_get_interface_prefix(ifname, prefix, sizeof(prefix));

	/* Get Unit of Primary Interface */
	unit = blanket_get_config_val_int(prefix, NVRAM_UNIT, 0);

	/* Erase all virtual BSS */
	for (idx = 1; idx < WBD_MAX_POSSIBLE_VIRTUAL_BSS; idx++) {
		snprintf(tmpvif, sizeof(tmpvif), "wl%d.%d", unit, idx);
		if (find_in_list(lan_ifnames, tmpvif) ||
			find_in_list(lan1_ifnames, tmpvif)) {
			if (wbd_slave_erase_bss(tmpvif, 0) == 0) {
				ret = 0;
			}
		} else {
			break;
		}
	}

	return ret;
}

/* Convert 1905 Auth Type to akm NVRAM value */
static void
wbd_slave_1905AuthTypeToAKM(char *prefix, unsigned short AuthType, char *akm, uint8 akm_len)
{
	int fbt = 0, map = 0, psk2ft = 0;

	memset(akm, 0, akm_len);

	/* No need to handle IEEE1905_AUTH_OPEN as the akm is already empty */
	if (AuthType & IEEE1905_AUTH_WPA) {
		add_to_list("wpa", akm, akm_len);
	}
	if (AuthType & IEEE1905_AUTH_WPA2) {
		add_to_list("wpa2", akm, akm_len);
	}
	if (AuthType & IEEE1905_AUTH_WPAPSK) {
		add_to_list("psk", akm, akm_len);
	}
	if (AuthType & IEEE1905_AUTH_WPA2PSK) {
		add_to_list("psk2", akm, akm_len);

		/* Read MAP NVRAM, and Check if Fronthaul BSS */
		map = blanket_get_config_val_int(prefix, NVRAM_MAP, 0);

		/* Check If FBT is enabled on this BSS */
		fbt = blanket_get_config_val_int(prefix, WBD_NVRAM_FBT,
				WBD_FBT_NOT_DEFINED);

		/* Condition to convert AKM from "psk2" to "psk2 psk2ft" */
		psk2ft = ((I5_IS_BSS_FRONTHAUL(map)) && (fbt == WBD_FBT_DEF_FBT_ENABLED));

		if (psk2ft) {
			add_to_list("psk2ft", akm, akm_len);
		}
	}
	if (AuthType & IEEE1905_AUTH_SAE) {
		add_to_list("sae", akm, akm_len);
	}
}

/* Convert 1905 Auth Type to auth NVRAM value */
static void
wbd_slave_1905AuthTypeToauth(unsigned short AuthType, char *auth, uint8 auth_len)
{
	memset(auth, 0, auth_len);

	if (AuthType == IEEE1905_AUTH_SHARED) {
		snprintf(auth, auth_len, "1");
	} else {
		snprintf(auth, auth_len, "0");
	}
}

/* Convert 1905 EncryptType to Crypto NVRAM value */
static void wbd_slave_1905EncrToCrypto(unsigned short EncryptType, char *crypto, uint8 crypto_len)
{
	memset(crypto, 0, crypto_len);

	if (EncryptType == IEEE1905_ENCR_TKIP) {
		snprintf(crypto, crypto_len, "tkip");
	} else if (EncryptType == IEEE1905_ENCR_AES) {
		snprintf(crypto, crypto_len, "aes");
	} else if (EncryptType == (IEEE1905_ENCR_TKIP | IEEE1905_ENCR_AES)) {
		snprintf(crypto, crypto_len, "tkip+aes");
	}
}

/* Convert 1905 EncryptType to WEP NVRAM name */
static void wbd_slave_1905EncrToWEP(unsigned short EncryptType, char *wep, uint8 wep_len)
{
	memset(wep, 0, wep_len);

	/* WEP Enabled */
	if (EncryptType == IEEE1905_ENCR_WEP) {
		snprintf(wep, wep_len, "enabled");
	} else {
		snprintf(wep, wep_len, "disabled");
	}
}

/* Make dedicated backhaul bss hidden */
static int
wbd_slave_make_bh_bss_hidden(char *prefix, ieee1905_client_bssinfo_type *bss)
{
	int ret = -1;

	/* For dedicated backhaul interface n/w type should be closed.
	 * Which makes sure AP does not advertise its ssid in beacon and
	 * Only directed prob resp will be having the actual ssid.
	 */
	if (bss->BackHaulBSS == 1 && bss->FrontHaulBSS != 1) {
		if (blanket_get_config_val_int(NULL, NVRAM_MAP_BH_OPEN, 0) == 0 &&
			blanket_get_config_val_int(prefix, NVRAM_CLOSED, 0) == 0) {
			blanket_nvram_prefix_set(prefix, NVRAM_CLOSED, "1");
			ret = 0;
		} else if (blanket_get_config_val_int(NULL, NVRAM_MAP_BH_OPEN, 0) == 1 &&
			blanket_get_config_val_int(prefix, NVRAM_CLOSED, 0) == 1) {
			blanket_nvram_prefix_set(prefix, NVRAM_CLOSED, "0");
			ret = 0;
		}
	}

	return ret;
}

/* Aplly the default settings needed like ssid, key etc... */
static void wbd_slave_apply_default_ap_nvrams(char *prefix, ieee1905_client_bssinfo_type *bss)
{
	char akm[WBD_MAX_BUF_32], crypto[WBD_MAX_BUF_16], strmap[WBD_MAX_BUF_16];
	char wep[WBD_MAX_BUF_16], auth[WBD_MAX_BUF_16];
	uint8 map = 0;

	blanket_nvram_prefix_set(prefix, NVRAM_WPS_MODE, "enabled");
	blanket_nvram_prefix_set(prefix, NVRAM_MODE, "ap");
	blanket_nvram_prefix_set(prefix, NVRAM_RADIO, "1");
	blanket_nvram_prefix_set(prefix, NVRAM_BSS_ENABLED, "1");
	blanket_nvram_prefix_set(prefix, NVRAM_SSID, (const char*)bss->ssid.SSID);
	blanket_nvram_prefix_set(prefix, NVRAM_WPA_PSK, (const char*)bss->NetworkKey.key);
	blanket_nvram_prefix_set(NULL, NVRAM_LAN_WPS_OOB, "disabled");

	wbd_slave_1905AuthTypeToAKM(prefix, bss->AuthType, akm, sizeof(akm));
	blanket_nvram_prefix_set(prefix, NVRAM_AKM, akm);

	wbd_slave_1905AuthTypeToauth(bss->AuthType, auth, sizeof(auth));
	blanket_nvram_prefix_set(prefix, NVRAM_AUTH, auth);

	wbd_slave_1905EncrToCrypto(bss->EncryptType, crypto, sizeof(crypto));
	blanket_nvram_prefix_set(prefix, NVRAM_CRYPTO, crypto);

	wbd_slave_1905EncrToWEP(bss->EncryptType, wep, sizeof(wep));
	blanket_nvram_prefix_set(prefix, NVRAM_WEP, wep);

	if (bss->BackHaulBSS == 1 && bss->FrontHaulBSS == 1) {
		map = 3;
	} else if (bss->BackHaulBSS == 1) {
		map = 2;
	} else if (bss->FrontHaulBSS == 1) {
		map = 1;
	}
	snprintf(strmap, sizeof(strmap), "%d", map);
	blanket_nvram_prefix_set(prefix, NVRAM_MAP, strmap);

	/* Make dedicated backhaul hidden  */
	(void)wbd_slave_make_bh_bss_hidden(prefix, bss);

	WBD_INFO("prefix[%s] ssid[%s] key[%s] AuthType[%x] akm[%s] EncryptType[%x] crypto[%s] "
		"map[%s]\n",
		prefix, bss->ssid.SSID, bss->NetworkKey.key, bss->AuthType, akm,
		bss->EncryptType, crypto, strmap);
}

/* Return TRUE if the settings are already there */
static int wbd_slave_same_bss_setting(char *ifname, char *prefix, ieee1905_client_bssinfo_type *bss)
{
	int ret = 0;
	char tmp[NVRAM_MAX_VALUE_LEN];
	char akm[WBD_MAX_BUF_32] = {0}, crypto[WBD_MAX_BUF_16] = {0}, strmap[WBD_MAX_BUF_16] = {0};
	char wep[WBD_MAX_BUF_16], auth[WBD_MAX_BUF_16];
	uint8 map = 0;

	if (!nvram_match(strcat_r(prefix, NVRAM_SSID, tmp), (char*)bss->ssid.SSID)) {
		WBD_INFO("%s not matching %s != %s\n", NVRAM_SSID,
			blanket_nvram_prefix_safe_get(prefix, NVRAM_SSID), bss->ssid.SSID);
		goto no_match;
	}
	if (!nvram_match(strcat_r(prefix, NVRAM_WPA_PSK, tmp), (const char*)bss->NetworkKey.key)) {
		WBD_INFO("%s not matching %s != %s\n", NVRAM_WPA_PSK,
			blanket_nvram_prefix_safe_get(prefix, NVRAM_WPA_PSK), bss->NetworkKey.key);
		goto no_match;
	}

	wbd_slave_1905AuthTypeToAKM(prefix, bss->AuthType, akm, sizeof(akm));
	if (!nvram_match(strcat_r(prefix, NVRAM_AKM, tmp), akm)) {
		WBD_INFO("%s not matching %s != %s\n", NVRAM_AKM,
			blanket_nvram_prefix_safe_get(prefix, NVRAM_AKM), akm);
		goto no_match;
	}

	wbd_slave_1905AuthTypeToauth(bss->AuthType, auth, sizeof(auth));
	if (!nvram_match(strcat_r(prefix, NVRAM_AUTH, tmp), auth)) {
		WBD_INFO("%s not matching %s != %s\n", NVRAM_AUTH,
			blanket_nvram_prefix_safe_get(prefix, NVRAM_AUTH), auth);
		goto no_match;
	}

	wbd_slave_1905EncrToCrypto(bss->EncryptType, crypto, sizeof(crypto));
	if (!nvram_match(strcat_r(prefix, NVRAM_CRYPTO, tmp), crypto)) {
		WBD_INFO("%s not matching %s != %s\n", NVRAM_CRYPTO,
			blanket_nvram_prefix_safe_get(prefix, NVRAM_CRYPTO), crypto);
		goto no_match;
	}

	wbd_slave_1905EncrToWEP(bss->EncryptType, wep, sizeof(wep));
	if (!nvram_match(strcat_r(prefix, NVRAM_WEP, tmp), wep)) {
		WBD_INFO("%s not matching %s != %s\n", NVRAM_WEP,
			blanket_nvram_prefix_safe_get(prefix, NVRAM_WEP), wep);
		goto no_match;
	}

	if (bss->BackHaulBSS == 1 && bss->FrontHaulBSS == 1) {
		map = 3;
	} else if (bss->BackHaulBSS == 1) {
		map = 2;
	} else if (bss->FrontHaulBSS == 1) {
		map = 1;
	}
	snprintf(strmap, sizeof(strmap), "%d", map);
	if (!nvram_match(strcat_r(prefix, NVRAM_MAP, tmp), strmap)) {
		WBD_INFO("%s not matching %s != %s\n", NVRAM_MAP,
			blanket_nvram_prefix_safe_get(prefix, NVRAM_MAP), strmap);
		goto no_match;
	}

	if (!(blanket_get_config_val_int(prefix, NVRAM_BSS_ENABLED, 0))) {
		WBD_INFO("%s not matching %s != %d\n", NVRAM_BSS_ENABLED,
			blanket_nvram_prefix_safe_get(prefix, NVRAM_BSS_ENABLED), 1);
		goto no_match;
	}

	if (!(blanket_get_config_val_int(prefix, NVRAM_RADIO, 0))) {
		WBD_INFO("%s not matching %s != %d\n", NVRAM_RADIO,
			blanket_nvram_prefix_safe_get(prefix, NVRAM_RADIO), 1);
		goto no_match;
	}

	if (bss->Guest == 0 && !find_in_list(blanket_nvram_safe_get(NVRAM_LAN_IFNAMES), ifname)) {
		WBD_INFO("%s not in %s(%s)\n", ifname, NVRAM_LAN_IFNAMES,
			blanket_nvram_safe_get(NVRAM_LAN_IFNAMES));
		goto no_match;
	}

	if (bss->Guest == 1 && !find_in_list(blanket_nvram_safe_get(NVRAM_LAN1_IFNAMES), ifname)) {
		WBD_INFO("%s not in %s(%s)\n", ifname, NVRAM_LAN1_IFNAMES,
			blanket_nvram_safe_get(NVRAM_LAN1_IFNAMES));
		goto no_match;
	}

	/* All the required settings are matching */
	ret = 1;

no_match:
	WBD_INFO("prefix[%s] ssid[%s] key[%s] AuthType[%x] akm[%s] EncryptType[%x] crypto[%s] "
		"map[%s] isGuest[%d] ret[%d]. %s\n",
		prefix, bss->ssid.SSID, bss->NetworkKey.key, bss->AuthType, akm,
		bss->EncryptType, crypto, strmap, bss->Guest, ret, ret ? "Matching" : "No Match");

	return ret;
}

int
wbd_slave_move_ifname_to_list(char *ifname, char *nvname_from, char *nvname_to)
{
	int ret = -1;

	if (wbd_slave_remove_ifname_from_list(ifname, nvname_from) == 0) {
		ret = 0;
	}

	if (wbd_slave_add_ifname_to_list(ifname, nvname_to) == 0) {
		ret = 0;
	}

	return ret;
}

/* Update the local BSS map flags from the BSS info recieved on M2 from controller */
static void wbd_slave_update_mapflags_from_bss_info(i5_dm_interface_type *i5_ifr,
	ieee1905_client_bssinfo_type *bss)
{
	i5_dm_bss_type *i5_bss;
	unsigned char map_flags = 0;

	/* Get MAP Flags from BSS info */
	if (bss->BackHaulBSS == 1) {
		map_flags |= IEEE1905_MAP_FLAG_BACKHAUL;
	}
	if (bss->FrontHaulBSS == 1) {
		map_flags |= IEEE1905_MAP_FLAG_FRONTHAUL;
	}
	if (bss->Guest == 1) {
		map_flags |= IEEE1905_MAP_FLAG_GUEST;
	}

	/* Search the BSS from interface list based on the SSID */
	foreach_i5glist_item(i5_bss, i5_dm_bss_type, i5_ifr->bss_list) {
		if ((bss->ssid.SSID_len != i5_bss->ssid.SSID_len) ||
			memcmp((char*)bss->ssid.SSID, (char*)i5_bss->ssid.SSID,
			bss->ssid.SSID_len) != 0) {
			continue;
		}

		i5_bss->mapFlags |= map_flags;
	}
}

/* This creates the BSS on the interface */
int wbd_slave_create_bss_on_ifr(wbd_info_t *info, char *ifname,
	ieee1905_glist_t *bssinfo_list, ieee1905_policy_config *policy, unsigned char policy_type)
{
	ieee1905_client_bssinfo_type *prim_bss = NULL, *iter_bss = NULL;
	char prefix[IFNAMSIZ] = {0}, tmpvif[NVRAM_MAX_VALUE_LEN], vif_prefix[IFNAMSIZ] = {0};
	char tmp[NVRAM_MAX_VALUE_LEN], lan_ifnames[NVRAM_MAX_VALUE_LEN] = {0};
	char lan1_ifnames[NVRAM_MAX_VALUE_LEN] = {0};
	int idx, unit, ret = -1, iter_bss_match;
	i5_dm_interface_type *i5_ifr;
	WBD_ENTER();

	if (!bssinfo_list) {
		goto end;
	}

	/* Get Prefix from OS specific interface name */
	blanket_get_interface_prefix(ifname, prefix, sizeof(prefix));

	/* Get Unit of Primary Interface */
	unit = blanket_get_config_val_int(prefix, NVRAM_UNIT, 0);

	/* Get First BSS Info in BSS Info List */
	prim_bss = (ieee1905_client_bssinfo_type *)dll_head_p(&(bssinfo_list->head));

	/* If Teardown erase all the NVRAMs */
	if (prim_bss->TearDown) {
		WBD_INFO("ifname[%s] Teardown. So Erase all BSS\n", ifname);
		ret = wbd_slave_erase_all_bss(ifname);
		goto end;
	}

	/* Find the self interface from ifname */
	if ((i5_ifr = wbd_ds_get_self_i5_ifr_from_ifname(ifname, NULL)) == NULL) {
		WBD_WARNING("Self Interface not found for ifname[%s]\n", ifname);
		goto end;
	}

	WBD_DEBUG("ifname[%s] IFR["MACDBG"] Number of BSS in M2[%d] Number of BSS configured in "
		"the radio[%d]\n",
		ifname, MAC2STRDBG(i5_ifr->InterfaceId), bssinfo_list->count,
		i5_ifr->BSSNumberOfEntries);

	/* If the number of BSS recieved in M2 is not equal to the number of BSS already configured,
	 * erase all the BSS and reconfigure it
	 */
	if (bssinfo_list->count != i5_ifr->BSSNumberOfEntries) {
		WBD_INFO("ifname[%s] IFR["MACDBG"] Number of BSS in M2[%d] != Number of BSS "
			"configured in the radio[%d]. Erase all the BSS and reconfigure\n",
			ifname, MAC2STRDBG(i5_ifr->InterfaceId), bssinfo_list->count,
			i5_ifr->BSSNumberOfEntries);
		goto erase;
	}

	/* Check if Primary BSS & all Virtual BSS settings are already applied or not */
	for (iter_bss = ((ieee1905_client_bssinfo_type *)dll_head_p(&((*bssinfo_list).head)));
		! dll_end(&((*bssinfo_list).head), (dll_t*)iter_bss);
		iter_bss = ((ieee1905_client_bssinfo_type *)dll_next_p((dll_t*)iter_bss))) {

		iter_bss_match = 0;

		if (iter_bss == prim_bss) {

			/* Check if the settings is already applied or not */
			if (wbd_slave_same_bss_setting(ifname, prefix, iter_bss)) {
				if (wbd_slave_add_ifname_to_list(ifname, WBD_NVRAM_IFNAMES) == 0 ||
					wbd_slave_make_bh_bss_hidden(prefix, iter_bss) == 0) {
					ret = 0;
				}
				WBD_INFO("ifname[%s] Prefix[%s] Same setting ret[%d]\n", ifname, prefix, ret);
				iter_bss_match = 1;
				continue;
			}
		}

		/* Check in all the virtual interfaces */
		for (idx = 1; idx < WBD_MAX_POSSIBLE_VIRTUAL_BSS; idx++) {
			snprintf(tmpvif, sizeof(tmpvif), "wl%d.%d", unit, idx);
			snprintf(vif_prefix, sizeof(vif_prefix), "wl%d.%d_", unit, idx);
			if (wbd_slave_same_bss_setting(tmpvif, vif_prefix, iter_bss)) {
				if (wbd_slave_add_ifname_to_list(tmpvif, WBD_NVRAM_IFNAMES) == 0 ||
					wbd_slave_make_bh_bss_hidden(vif_prefix, iter_bss) == 0) {
					ret = 0;
				}
				WBD_INFO("ifname[%s] vif[%s] Prefix[%s] Same setting ret[%d]\n", ifname,
					tmpvif, vif_prefix, ret);
				iter_bss_match = 1;
				break;
			}
		}

		/* If Current BSS settings is not already applied on any AP BSS, ERASE */
		if (!iter_bss_match) {
			goto erase;
		} else {
			/* Update the map flags based on the BSS info recieved on M2. This is done
			 * beacuse we do not update the NVRAM "map" with guest bit flag. So, the BSS
			 * in interface list now contains only the fronthaul and backhaul details.
			 * We can get guest bit flag from BSS info which we got in M2.
			 */
			wbd_slave_update_mapflags_from_bss_info(i5_ifr, iter_bss);
		}
	}

	/* If Primary BSS & all Virtual BSS settings are already applied, nothing to be done */
	WBD_INFO("ifname[%s] Prefix[%s] All BSS has same settings ret[%d]\n", ifname, prefix, ret);
	goto end;

erase :
	/* Erase all the BSS on this interface */
	ret = wbd_slave_erase_all_bss(ifname);

	/* Apply Settings for Primary BSS & all Virtual BSS */
	for (iter_bss = ((ieee1905_client_bssinfo_type *)dll_head_p(&((*bssinfo_list).head)));
		! dll_end(&((*bssinfo_list).head), (dll_t*)iter_bss);
		iter_bss = ((ieee1905_client_bssinfo_type *)dll_next_p((dll_t*)iter_bss))) {

		iter_bss_match = 0;

		/* First interface apply settings on primary interface if its not STA */
		if (iter_bss == prim_bss) {
			if (atoi(blanket_nvram_safe_get(NVRAM_MAP_ONBOARDED)) == 0) {
				blanket_nvram_prefix_unset(prefix, NVRAM_MAP);
				blanket_nvram_prefix_set(prefix, NVRAM_MODE, "ap");
			}
			/* Check if, Primary Interface Mode is STA */
			if (nvram_match(strcat_r(prefix, NVRAM_MODE, tmp), "sta") != 1) {
				wbd_slave_apply_default_ap_nvrams(prefix, iter_bss);
				wbd_slave_add_ifname_to_list(ifname, WBD_NVRAM_IFNAMES);
				if (iter_bss->Guest == 1) {
					wbd_slave_move_ifname_to_list(ifname,
						NVRAM_LAN_IFNAMES, NVRAM_LAN1_IFNAMES);
				}
				WBD_INFO("ifname[%s] Prefix[%s] Applied Settings\n", ifname, prefix);
				iter_bss_match = 1;
				ret = 0;
				continue;
			}
		}

		/* Get NVRAM value from "lan_ifnames" and "lan1_ifnames" save it */
		WBDSTRNCPY(lan_ifnames, blanket_nvram_safe_get(NVRAM_LAN_IFNAMES), sizeof(lan_ifnames) - 1);
		WBDSTRNCPY(lan1_ifnames, blanket_nvram_safe_get(NVRAM_LAN1_IFNAMES), sizeof(lan1_ifnames) - 1);

		/* Check the vifs which is free */
		for (idx = 1; idx < WBD_MAX_POSSIBLE_VIRTUAL_BSS; idx++) {
			snprintf(tmpvif, sizeof(tmpvif), "wl%d.%d", unit, idx);
			if ((!find_in_list(lan_ifnames, tmpvif)) &&
				(!find_in_list(lan1_ifnames, tmpvif))) {
				wbd_slave_erase_bss(tmpvif, 0);
				wbd_create_vif(unit, idx);
				if (iter_bss->Guest == 1) {
					WBD_INFO("tmpvif %s is GUEST\n", tmpvif);
					wbd_slave_move_ifname_to_list(tmpvif,
						NVRAM_LAN_IFNAMES, NVRAM_LAN1_IFNAMES);
				}
				snprintf(vif_prefix, sizeof(vif_prefix), "%s_", tmpvif);
				wbd_slave_apply_default_ap_nvrams(vif_prefix, iter_bss);
				wbd_slave_add_ifname_to_list(tmpvif, WBD_NVRAM_IFNAMES);
				ret = 0;
				WBD_INFO("ifname[%s] Prefix[%s] Applied Settings\n", ifname, vif_prefix);
				iter_bss_match = 1;
				break;
			}
		}

		/* If Current BSS settings is not applied on any AP BSS */
		if (!iter_bss_match) {
			WBD_INFO("ifname[%s] subunit[%d]. Failed to apply settings\n", ifname, idx);
		}
	}

end:

#if defined(MULTIAPR2)
	/* If the M2 has both 802.1Q default settings and traffic separation policy, apply it */
	if (policy_type & (MAP_POLICY_TYPE_FLAG_TS_8021QSET | MAP_POLICY_TYPE_FLAG_TS_POLICY)) {
		if (wbd_slave_process_traffic_separation_policy(policy, policy_type) == 0) {
			ret = 0;
		}
	}
#endif /* MULTIAPR2 */

	if (!bssinfo_list) {
		if (i5DmIsM2ReceivedByAllWirelessInterfaces()) {
			blanket_nvram_prefix_set(NULL, NVRAM_MAP_ONBOARDED, "1");
			blanket_nvram_prefix_unset(NULL, NVRAM_WPS_ON_STA);
			WBD_INFO("M2 message is received by all the interfaces hence"
				" setting map onboarded to 1 and unsetting nvram wps_on_sta\n");
		}
		WBD_INFO("Creating rc restart timer\n");
		wbd_slave_create_rc_restart_timer(info);
		info->flags |= WBD_INFO_FLAGS_RC_RESTART;
	}
	WBD_EXIT();
	return ret;
}

/* Check if the backhaul STA is weak or not */
static int
wbd_slave_is_backhaul_sta_weak(wbd_info_t *info, i5_dm_clients_type *i5_sta, int *out_weak_flag)
{
	int isweak = FALSE, fail_cnt = 0;
	uint32 adj_mcs_phyrate = 0;
	wbd_weak_sta_metrics_t sta_stats;
	wbd_weak_sta_policy_t *policy;
	wbd_assoc_sta_item_t* sta;
	WBD_ENTER();

	policy = &info->wbd_slave->metric_policy_bh;
	sta = (wbd_assoc_sta_item_t*)i5_sta->vndr_data;

	/* Using tx_rate directly because, if the tx_rate drops drastically,
	 * the mcs_phyrate is not getting updated.
	 * Adjust MCS phyrate by filtering bogus tx_rate
	 */
	adj_mcs_phyrate = ((sta->stats.tx_rate < WBD_BK_MAX_DATA_RATE) &&
		(sta->stats.tx_rate > WBD_BK_MIN_PHYRATE)) ? sta->stats.tx_rate : 0;

	WBD_INFO("STA["MACDBG"] RSSI[%d] ReportedRSSI[%d] tx_rate[%d] adj_mcs_phyrate[%d] "
		"datarate[%d] tx_failures[%d] PolicyFlag[0x%x] t_IdleRate[%d] t_RSSI[%d] "
		"t_hysterisis[%d] t_txrate[%d] t_txfailure[%d]\n",
		MAC2STRDBG(i5_sta->mac), sta->stats.rssi, sta->last_weak_rssi,
		(int)sta->stats.tx_rate, adj_mcs_phyrate, sta->stats.idle_rate,
		sta->stats.tx_failures, policy->flags, policy->t_idle_rate, policy->t_rssi,
		policy->t_hysterisis, policy->t_tx_rate, policy->t_tx_failures);

	/* Fill STA Stats, which needs to be compared for Weak Client Indentification */
	memset(&sta_stats, 0, sizeof(sta_stats));
	sta_stats.idle_rate = sta->stats.idle_rate;
	sta_stats.rssi = sta->stats.rssi;
	sta_stats.last_weak_rssi = sta->last_weak_rssi;
	sta_stats.tx_rate = adj_mcs_phyrate;
	sta_stats.tx_failures = sta->stats.tx_failures;

	/* Common algo to compare STA Stats and Thresholds, & identify if STA is Weak or not */
	isweak = wbd_weak_sta_identification((struct ether_addr*)&i5_sta->mac, &sta_stats,
		&info->wbd_slave->metric_policy_bh, &fail_cnt, out_weak_flag);

	if (isweak) {
		WBD_INFO("Found Weak STA["MACDBG"] fail_cnt=%d sta_rule_met=0x%X\n",
			MAC2STRDBG(i5_sta->mac), fail_cnt,
			out_weak_flag ? *out_weak_flag : 0);
	}

	WBD_EXIT();
	return isweak;
}

/* Process backhaul STA */
static void
wbd_slave_check_backhaul_weak_sta(wbd_info_t *info, i5_dm_clients_type *i5_sta)
{
	int ret = WBDE_OK, weak_flag;
	wbd_assoc_sta_item_t* sta;
	i5_dm_device_type *i5_controller_device = NULL;
	WBD_ENTER();

	sta = (wbd_assoc_sta_item_t*)i5_sta->vndr_data;
	WBD_SAFE_GET_I5_CTLR_DEVICE(i5_controller_device, &ret);

	if (wbd_slave_is_backhaul_sta_weak(info, i5_sta, &weak_flag) == TRUE) {
		/* If its already weak, don't inform to Controller */
		if (sta->status != WBD_STA_STATUS_WEAK) {
			ret = ieee1905_send_assoc_sta_link_metric(i5_controller_device->DeviceId,
				i5_sta->mac);
			if (ret != 0) {
				sta->status = WBD_STA_STATUS_WEAK;
			}
		}
	} else {
		/* Not weak. So check if its already weak */
		if (sta->status == WBD_STA_STATUS_WEAK) {
			sta->status = WBD_STA_STATUS_NORMAL;
		}
	}

end:
	WBD_EXIT();
	return;
}

/* Bakchual STA weak client watch dog timer callback */
static void
wbd_slave_backhaul_sta_weak_client_watchdog_timer_cb(bcm_usched_handle *hdl, void *arg)
{
	int ret = WBDE_OK, is_processed = 0;
	wbd_info_t *info = (wbd_info_t*)arg;
	i5_dm_device_type *i5_self_device;
	i5_dm_interface_type *i5_iter_ifr;
	i5_dm_bss_type *i5_iter_bss;
	i5_dm_clients_type *i5_iter_sta;
	wbd_assoc_sta_item_t* sta;
	WBD_ENTER();

	WBD_DEBUG("Backhaul sta weak client watchdog timer\n");

	i5_self_device = i5DmGetSelfDevice();

	/* Validate arg */
	WBD_ASSERT_ARG(info, WBDE_INV_ARG);
	WBD_ASSERT_ARG(i5_self_device, WBDE_INV_ARG);

	/* For each backhaul STA */
	foreach_i5glist_item(i5_iter_ifr, i5_dm_interface_type, i5_self_device->interface_list) {

		foreach_i5glist_item(i5_iter_bss, i5_dm_bss_type, i5_iter_ifr->bss_list) {

			/* Only check in backhaul BSS */
			if (!(i5_iter_bss->mapFlags & IEEE1905_MAP_FLAG_BACKHAUL)) {
				continue;
			}

			foreach_i5glist_item(i5_iter_sta, i5_dm_clients_type,
				i5_iter_bss->client_list) {

				sta = (wbd_assoc_sta_item_t*)i5_iter_sta->vndr_data;
				if (!sta) {
					continue;
				}

				if (!I5_CLIENT_IS_BSTA(i5_iter_sta)) {
					WBD_INFO("BSS["MACDBG"] STA["MACDBG"] is Not Backhaul "
						"STA\n", MAC2STRDBG(i5_iter_bss->BSSID),
						MAC2STRDBG(i5_iter_sta->mac));
					continue;
				}

				/* If the STA needs to be ignored. Dont process */
				if (sta->status == WBD_STA_STATUS_IGNORE) {
					WBD_INFO("BSS["MACDBG"] STA["MACDBG"] Ignored\n",
						MAC2STRDBG(i5_iter_bss->BSSID),
						MAC2STRDBG(i5_iter_sta->mac));
					continue;
				}

				/* Update processed flag. If none of the backhaul STAs are processed
				 * no need of watchdog timer as there are no STAs or all the STAs
				 * needs to be ignored
				 */
				is_processed = 1;

				/* Skip the bouncing STA */
				if (sta->error_code == WBDE_DS_BOUNCING_STA) {
					time_t now = time(NULL);
					time_t gap;

					gap = now - sta->dwell_start;
					/* Check if the STA still in dwell state */
					if (sta->dwell_time > gap) {
						WBD_INFO("Still in dwell state. BSS["MACDBG"] "
							"BHSTA["MACDBG"] error_code[%d] dwell[%lu] "
							"dwell_start[%lu] gap[%lu]\n",
							MAC2STRDBG(i5_iter_bss->BSSID),
							MAC2STRDBG(i5_iter_sta->mac),
							sta->error_code,
							(unsigned long)sta->dwell_time,
							(unsigned long)sta->dwell_start,
							(unsigned long)gap);
						continue;
					}
					/* Dwell state is expired. so, remove the dwell state */
					sta->error_code = WBDE_OK;
					sta->dwell_start = 0;
					sta->dwell_time = 0;
				}

				/* Update the stats for this backhaul STA */
				wbd_slave_get_backhaul_assoc_sta_metric(i5_iter_bss->ifname,
					i5_iter_sta);
				wbd_slave_check_backhaul_weak_sta(info, i5_iter_sta);
			}
		}
	}

	if (!is_processed) {
		WBD_INFO("Backhaul STA weak client watchdog timer can be removed\n");
		wbd_slave_remove_backhaul_sta_weak_client_watchdog_timer(info);
	}

end:
	WBD_EXIT();
}

/* Create the bakchual STA weak client watch dog timer */
void
wbd_slave_create_backhaul_sta_weak_client_watchdog_timer(wbd_info_t *info)
{
	int ret = WBDE_OK;
	WBD_ENTER();

	if (info->wbd_slave->flags & WBD_BKT_SLV_FLAGS_BH_WD_WEAK_CLIENT) {
		WBD_INFO("Backhaul STA weak client watchdog timer already created\n");
		goto end;
	}

	ret = wbd_add_timers(info->hdl, info, WBD_SEC_MICROSEC(info->max.tm_wd_bh_weakclient),
		wbd_slave_backhaul_sta_weak_client_watchdog_timer_cb, 1);
	if (ret != WBDE_OK) {
		WBD_WARNING("Interval[%d] Failed to create backhaul STA weak client "
			"watchdog timer\n", info->max.tm_wd_bh_weakclient);
		goto end;
	}

	info->wbd_slave->flags |= WBD_BKT_SLV_FLAGS_BH_WD_WEAK_CLIENT;
end:
	WBD_EXIT();
}

/* Remove the bakchual STA weak client watch dog timer */
void
wbd_slave_remove_backhaul_sta_weak_client_watchdog_timer(wbd_info_t *info)
{
	WBD_ENTER();
	if (info->wbd_slave->flags & WBD_BKT_SLV_FLAGS_BH_WD_WEAK_CLIENT) {
		WBD_INFO("Backhaul STA weak client watchdog timer exists. Remove it\n");
		wbd_remove_timers(info->hdl, wbd_slave_backhaul_sta_weak_client_watchdog_timer_cb,
			info);
		info->wbd_slave->flags &= ~WBD_BKT_SLV_FLAGS_BH_WD_WEAK_CLIENT;
	}
	WBD_EXIT();
}
