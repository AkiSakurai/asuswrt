/*
 * WBD Blanket utility for Slave (Linux)
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
 * $Id: wbd_blanket_utility.c 781736 2019-11-27 09:57:31Z $
 */

#include <errno.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>

#include <typedefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <shutils.h>
#include <wlutils.h>
#include <wlioctl.h>
#include <bcmendian.h>
#include <common_utils.h>
#include <wlif_utils.h>

#include "wbd.h"
#include "blanket.h"
#include "wbd_shared.h"
#include "wbd_tlv.h"
#include "wbd_blanket_utility.h"

/* Get STA link metrics and STA traffic stats */
int wbd_slave_process_get_assoc_sta_metric(char *ifname, unsigned char *bssid,
	unsigned char *sta_mac, ieee1905_sta_link_metric *metric,
	ieee1905_sta_traffic_stats *traffic_stats, ieee1905_vendor_data *out_vndr_tlv)
{
	int ret = WBDE_OK, rssi;
	sta_info_t sta_info;
	wbd_cmd_vndr_assoc_sta_metric_t cmd; /* STA Metric Vndr Data */
	wbd_prb_sta_t *prbsta;
	iov_bs_data_counters_t ctr;
	i5_dm_bss_type *i5_bss;
	i5_dm_clients_type *i5_sta = NULL;
	wbd_assoc_sta_item_t* sta = NULL;
	uint32 rx_rate = 0;
	time_t gap, now = time(NULL);
	wbd_info_t *info = wbd_get_ginfo();
	WBD_ENTER();

	WBD_SAFE_GET_I5_SELF_BSS(bssid, i5_bss, &ret);

	/* Retrive the sta from slave's assoclist */
	i5_sta = wbd_ds_find_sta_in_bss_assoclist(i5_bss, (struct ether_addr*)sta_mac, &ret, &sta);
	if (!i5_sta && !sta) {
		WBD_WARNING("BSS["MACF"] STA["MACF"] Failed to find "
			"STA in assoclist. Error : %s\n",
			MAC2STRDBG(bssid), MAC2STRDBG(sta_mac), wbderrorstr(ret));
		goto end;
	}

	if (!traffic_stats || !metric) {
		WBD_ERROR("traffic stas or metric cannot be NULL\n");
		goto end;
	}

	gap = now - sta->stats.active;

	ret = blanket_get_rssi(ifname, (struct ether_addr*)sta_mac, &rssi);
	WBD_ASSERT();
	metric->rcpi = (unsigned char)(WBD_RSSI_TO_RCPI(rssi));

	/* get other stats from STA_INFO iovar */
	ret = blanket_get_sta_info(ifname, (struct ether_addr*)sta_mac, &sta_info);
	WBD_ASSERT();

	metric->uplink_rate = (unsigned int)(sta_info.rx_rate / 1000);
	metric->downlink_rate = (unsigned int)(sta_info.tx_rate / 1000);

	/* Fill Traffic stats */
	traffic_stats->bytes_sent = (unsigned int)(sta_info.tx_tot_bytes);
	traffic_stats->bytes_recv = (unsigned int)(sta_info.rx_tot_bytes);
	traffic_stats->packets_sent = (unsigned int)(sta_info.tx_tot_pkts);
	traffic_stats->packets_recv = (unsigned int)(sta_info.rx_tot_pkts);
	traffic_stats->tx_packet_err = (unsigned int)(sta_info.tx_failures);
	traffic_stats->rx_packet_err = (unsigned int)(sta_info.rx_decrypt_failures);
	traffic_stats->retransmission_count = (unsigned int)(sta_info.tx_pkts_retried);

	WBD_INFO("Ifname[%s] BSS["MACDBG"] STA["MACDBG"] RCPI[%d]  Downlink[%d] Uplink[%d] "
		"BytesSent[%d] BytesRecv[%d] PacketsSent[%d] PacketsRecv[%d] "
		"TxPacketErrors[%d] RxPacketErrors[%d] RetransmissionCount[%d]\n",
		ifname, MAC2STRDBG(bssid), MAC2STRDBG(sta_mac), metric->rcpi,
		metric->downlink_rate, metric->uplink_rate, traffic_stats->bytes_sent,
		traffic_stats->bytes_recv, traffic_stats->packets_sent,
		traffic_stats->packets_recv,
		traffic_stats->tx_packet_err, traffic_stats->rx_packet_err,
		traffic_stats->retransmission_count);

	if (out_vndr_tlv) {
		/* Initialize Vendor Specific Associated STA Link Metrics */
		memset(&cmd, 0, sizeof(cmd));

		/* Fill Vendor Specific Associated STA Link Metrics */
		cmd.num_bss = 1;
		memcpy(cmd.src_bssid, bssid, MAC_ADDR_LEN);
		memcpy(cmd.sta_mac, sta_mac, MAC_ADDR_LEN);

		/* Get the tx failures only if it is not got from the driver within TBSS time */
		if (sta->stats.active > 0 && gap < info->max.tm_wd_tbss) {
			cmd.vndr_metrics.tx_failures = sta->stats.tx_failures;
			cmd.vndr_metrics.tx_tot_failures = sta->stats.tx_tot_failures;
			WBD_DEBUG("active[%d] gap[%d] Old Txfailures[%d] TxTotFailures[%d]\n",
				(uint32)sta->stats.active, (uint32)gap, sta->stats.tx_failures,
				(uint32)sta->stats.tx_tot_failures);
		} else {
			cmd.vndr_metrics.tx_failures = 0;
			cmd.vndr_metrics.tx_tot_failures = (uint32)sta_info.tx_failures;
		}

		/* Get the bs_data only if it is not got from the driver within TBSS time */
		if (sta->stats.active > 0 && gap < info->max.tm_wd_tbss) {
			/* Use the old data */
			cmd.vndr_metrics.idle_rate = (uint32)(sta->stats.idle_rate);
			WBD_DEBUG("active[%d] gap[%d] Old DataRate[%d]\n",
				(uint32)sta->stats.active, (uint32)gap, sta->stats.idle_rate);
		} else {
			memset(&ctr, 0, sizeof(ctr));
			if (blanket_get_bs_data_counters(ifname, (struct ether_addr*)sta_mac, &ctr)
				== WBDE_OK) {

				/* Calculate Data rate */
				sta->stats.idle_rate = (ctr.time_delta) ?
					((ctr.throughput * 8000) / ctr.time_delta) : 0;

				if (sta_info.rx_tot_pkts > sta->stats.rx_tot_pkts) {
					/* rx_rate shall be aggregated into datarate calculation */
					if (sta_info.rx_tot_bytes > sta->stats.rx_tot_bytes) {
						gap = now - sta->stats.active;
						if (gap > 0) {
							rx_rate = (((sta_info.rx_tot_bytes -
								sta->stats.rx_tot_bytes) * 8) /
								(gap * 1000));
							sta->stats.idle_rate += rx_rate;
						}
					}
				}
				WBD_DEBUG("BSS["MACDBG"] STA["MACDBG"] datarate[%d]\n",
					MAC2STRDBG(bssid), MAC2STRDBG(sta_mac),
					sta->stats.idle_rate);

				sta->stats.rx_tot_pkts = (uint32)sta_info.rx_tot_pkts;
				sta->stats.rx_tot_bytes = (uint64)sta_info.rx_tot_bytes;
				cmd.vndr_metrics.idle_rate = (uint32)(sta->stats.idle_rate);
			}
		}

		/* Get Dual band supported flag */
		prbsta = wbd_ds_find_sta_in_probe_sta_table(wbd_get_ginfo(),
			(struct ether_addr*)sta_mac, FALSE);
		if (prbsta) {
			if (WBD_IS_DUAL_BAND(prbsta->band)) {
				cmd.vndr_metrics.sta_cap |= WBD_TLV_ASSOC_STA_CAPS_FLAG_DUAL_BAND;
			}
		}

		/* Encode Vendor Specific TLV : Associated STA Link Metrics Vendor Data to send */
		wbd_tlv_encode_vndr_assoc_sta_metrics((void*)&cmd, out_vndr_tlv->vendorSpec_msg,
			&(out_vndr_tlv->vendorSpec_len));

		/* Update the active timestamp */
		if (gap >= info->max.tm_wd_tbss) {
			sta->stats.active = now;
		}
	}
end:
	WBD_EXIT();
	return ret;
}

/* Update BSS capability for all BSS in a interface */
void
wbd_slave_update_bss_capability(i5_dm_interface_type *i5_ifr)
{
	int ret = WBDE_OK;
	wl_bss_info_t *bss_info;
	i5_dm_bss_type *i5_bss;
	wbd_bss_item_t *bss_vndr_data;
	uint8 phytype;
	uint32 bssid_info;
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(i5_ifr, WBDE_INV_ARG);

	/* Traverse BSS List */
	foreach_i5glist_item(i5_bss, i5_dm_bss_type, i5_ifr->bss_list) {

		bss_vndr_data = (wbd_bss_item_t*)i5_bss->vndr_data;
		if (bss_vndr_data == NULL) {
			WBD_WARNING("BSS["MACDBG"] Vendor Data NULL\n", MAC2STRDBG(i5_bss->BSSID));
			continue;
		}

		bssid_info = 0;
		phytype = 0;

		/* Get BSS Info */
		ret = blanket_get_bss_info(i5_bss->ifname, &bss_info);
		if (ret != WBDE_OK) {
			continue;
		}

		/* Get the BSSID Information */
		if (blanket_get_bssid_info_field(i5_bss->ifname, bss_info, &bssid_info)
			== WBDE_OK) {

			bss_vndr_data->bssid_info = bssid_info;
			if (WBD_FBT_ENAB(bss_vndr_data->flags)) {
				bss_vndr_data->bssid_info |= DOT11_NGBR_BI_MOBILITY;
			}
		}

		if (blanket_get_phy_type(i5_bss->ifname, bss_info, &phytype) == WBDE_OK) {
			bss_vndr_data->phytype = phytype;
		}

		WBD_INFO("BSS["MACDBG"] bssid_info[0x%x] phytype[0x%x]\n",
			MAC2STRDBG(i5_bss->BSSID), bssid_info, phytype);
	}

end:
	WBD_EXIT();
}

/* STA has Associated or Disassociated on this device */
int
blanket_sta_assoc_disassoc(struct ether_addr *bssid, struct ether_addr *mac, int isAssoc,
	unsigned short time_elapsed, unsigned char notify, unsigned char *assoc_frame,
	unsigned int assoc_frame_len)
{
	return (ieee1905_sta_assoc_disassoc((unsigned char*)bssid, (unsigned char*)mac, isAssoc,
		time_elapsed, notify, assoc_frame, assoc_frame_len));
}

/* Add All STA's Currently Associated with the BSS */
int
blanket_add_all_associated_stas(char *ifname, struct ether_addr *bssid)
{
	int ret = 0;
	int iter_sta;
	unsigned short time_elapsed = 0;
	sta_info_t sta_info;
	uint8 *assoclist = NULL;
	int assoclistlen = 0;
	struct maclist* list = NULL;
	WBD_ENTER();

	/* Get Max Assoclist len */
	ret = blanket_get_max_assoc(ifname, &assoclistlen);
	WBD_ASSERT();

	assoclistlen = assoclistlen * ETHER_ADDR_LEN + sizeof(uint32);

	assoclist = (uint8 *)wbd_malloc(assoclistlen, &ret);
	WBD_ASSERT();

	list = (struct maclist *)assoclist;

	/* Fill assoc sta list if any */
	ret =  blanket_get_assoclist(ifname, list, assoclistlen);
	WBD_ASSERT();

	for (iter_sta = 0; iter_sta < list->count; iter_sta++) {
		/* Get the time elapsed since the assoc */
		ret = blanket_get_sta_info(ifname, &list->ea[iter_sta], &sta_info);
		if (ret != 0) {
			time_elapsed = 0;
		} else {
			time_elapsed = (unsigned short)sta_info.in;
		}

		ret = ieee1905_sta_assoc_disassoc((unsigned char*)bssid,
			(unsigned char*)&list->ea[iter_sta], 1, time_elapsed, 0,
			NULL, 0);
		if (ret != 0) {
			WBD_WARNING("Ifname[%s] BSSID["MACF"] STA["MACF"] Failed to Add STA to "
				"MultiAP Error : %d\n",
				ifname, ETHERP_TO_MACF(bssid), ETHER_TO_MACF(list->ea[iter_sta]),
				ret);
		}
	}

end:
	if (assoclist) {
		free(assoclist);
	}

	WBD_EXIT();
	return ret;
}

/* Get Backhaul Associated STA metrics */
int
wbd_slave_get_backhaul_assoc_sta_metric(char *ifname, i5_dm_clients_type *i5_sta)
{
	int ret = WBDE_OK, rssi;
	sta_info_t sta_info;
	iov_bs_data_counters_t ctr;
	wbd_assoc_sta_item_t* sta = NULL;
	uint32 rx_rate = 0;
	time_t gap, now = time(NULL);
	wbd_info_t *info = wbd_get_ginfo();
	i5_dm_bss_type *i5_bss;
	WBD_ENTER();

	WBD_ASSERT_ARG(i5_sta, WBDE_INV_ARG);
	i5_bss = (i5_dm_bss_type*)WBD_I5LL_PARENT(i5_sta);
	sta = (wbd_assoc_sta_item_t*)i5_sta->vndr_data;
	WBD_ASSERT_ARG(sta, WBDE_INV_ARG);

	gap = now - sta->stats.active;

	ret = blanket_get_rssi(ifname, (struct ether_addr*)i5_sta->mac, &rssi);
	WBD_ASSERT();
	sta->stats.rssi = rssi;

	/* get other stats from STA_INFO iovar */
	ret = blanket_get_sta_info(ifname, (struct ether_addr*)i5_sta->mac, &sta_info);
	WBD_ASSERT();

	sta->stats.tx_rate = sta_info.tx_rate / 1000;
	sta->stats.old_tx_tot_failures = sta->stats.tx_tot_failures;
	sta->stats.tx_tot_failures = (uint32)sta_info.tx_failures;
	sta->stats.tx_failures = sta->stats.tx_tot_failures - sta->stats.old_tx_tot_failures;

	memset(&ctr, 0, sizeof(ctr));
	if (blanket_get_bs_data_counters(ifname, (struct ether_addr*)i5_sta->mac, &ctr)
		== WBDE_OK) {

		/* Calculate Data rate */
		sta->stats.idle_rate = (ctr.time_delta) ?
			((ctr.throughput * 8000) / ctr.time_delta) : 0;

		if (sta_info.rx_tot_pkts > sta->stats.rx_tot_pkts) {
			/* rx_rate shall be aggregated into datarate calculation */
			if (sta_info.rx_tot_bytes > sta->stats.rx_tot_bytes) {
				gap = now - sta->stats.active;
				if (gap > 0) {
					rx_rate = (((sta_info.rx_tot_bytes -
						sta->stats.rx_tot_bytes) * 8) /
						(gap * 1000));
					sta->stats.idle_rate += rx_rate;
				}
			}
		}

		sta->stats.rx_tot_pkts = (uint32)sta_info.rx_tot_pkts;
		sta->stats.rx_tot_bytes = (uint64)sta_info.rx_tot_bytes;
	}

	WBD_INFO("Ifname[%s] BSS["MACDBG"] STA["MACDBG"] RSSI[%d]  TxRate[%d] "
		"OldTxFailures[%d] TxTotFailures[%d] TxFailures[%d] IdleRate[%d] "
		"RxTotPkts[%d] RxTotBytes[%d]\n",
		ifname, MAC2STRDBG(i5_bss->BSSID), MAC2STRDBG(i5_sta->mac),
		sta->stats.rssi, (int)sta->stats.tx_rate, sta->stats.old_tx_tot_failures,
		sta->stats.tx_tot_failures, sta->stats.tx_failures, sta->stats.idle_rate,
		sta->stats.rx_tot_pkts, (int)sta->stats.rx_tot_bytes);

	/* Update the active timestamp */
	if (gap >= info->max.tm_wd_bh_weakclient) {
		sta->stats.active = now;
	}
end:
	WBD_EXIT();
	return ret;
}

/* Block or unblock backhaul STA assocaition. Incase of block, disassociate all backhaul STAs on
 * the self device
 */
void
wbd_slave_block_unblock_backhaul_sta_assoc(int unblock)
{
	int ret, macmode, mac_filter;
	i5_dm_device_type *i5_device = i5DmGetSelfDevice();
	i5_dm_interface_type *iter_ifr;
	i5_dm_bss_type *iter_bss;
	i5_dm_clients_type *iter_sta;
	wbd_bss_item_t *bss_vndr_data;

	/* Loop for all the Interfaces in Device */
	foreach_i5glist_item(iter_ifr, i5_dm_interface_type, i5_device->interface_list) {

		/* Loop for all the BSSs in Interface */
		foreach_i5glist_item(iter_bss, i5_dm_bss_type, iter_ifr->bss_list) {

			bss_vndr_data = (wbd_bss_item_t*)iter_bss->vndr_data;

			/* Only use Backhaul BSS */
			if (!I5_IS_BSS_BACKHAUL(iter_bss->mapFlags)) {
				continue;
			}

			if (unblock) {
				macmode = htod32(WLC_MACMODE_DISABLED);
				mac_filter = 0;
			} else {
				macmode = htod32(WLC_MACMODE_ALLOW);
				mac_filter = 1;
			}

			ret = wl_ioctl(iter_bss->ifname, WLC_SET_MACMODE, &macmode, sizeof(macmode));
			WBD_INFO("Ifname[%s] %s BSS["MACDBG"] MACMODE[%d] Flags[%x] ret[%d]\n",
				iter_bss->ifname, unblock ? "Unblocking" : "Blocking",
				MAC2STRDBG(iter_bss->BSSID), macmode, bss_vndr_data->flags, ret);

			ret = wl_iovar_setint(iter_bss->ifname, "probresp_mac_filter", mac_filter);
			WBD_INFO("Ifname[%s] %s BSS["MACDBG"] probresp_mac_filter[%d] Flags[%x] "
				"ret[%d]\n", iter_bss->ifname, unblock ? "UnBlocking" : "Blocking",
				MAC2STRDBG(iter_bss->BSSID), mac_filter, bss_vndr_data->flags, ret);

			/* Deauthenticate all STAs if we are blocking the bh BSS */
			if (!unblock) {
				/* Loop for all the backhaul Clients in BSS */
				foreach_i5glist_item(iter_sta, i5_dm_clients_type,
					iter_bss->client_list) {
					/* Deauth the STA from current BSS */
					blanket_deauth_sta(iter_bss->ifname,
						(struct ether_addr*)iter_sta->mac, 0);
					WBD_INFO("Ifname[%s] BSS["MACDBG"] Disassociate "
						"STA["MACDBG"]\n", iter_bss->ifname,
						MAC2STRDBG(iter_bss->BSSID),
						MAC2STRDBG(iter_sta->mac));
				}
			}
		}
	}
}
