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
 * $Id: wbd_blanket_utility.c 791063 2020-09-15 07:32:51Z $
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

	metric->uplink_rate = (unsigned int)(dtoh32(sta_info.rx_rate) / 1000);
	metric->downlink_rate = (unsigned int)(dtoh32(sta_info.tx_rate) / 1000);
#if defined(MULTIAPR2)
	metric->last_data_uplink_rate = (unsigned int)(dtoh32(sta_info.rx_rate) / 1000);
	metric->last_data_downlink_rate = (unsigned int)(dtoh32(sta_info.tx_rate) / 1000);
	metric->utilization_recv = (unsigned int)(dtoh32(sta_info.in) * 1000);
	metric->utilization_tx = (unsigned int)(dtoh32(sta_info.in) * 1000);
#endif /* MULTIAPR2 */

	/* Fill Traffic stats */
	traffic_stats->bytes_sent = (uint64)dtoh64(sta_info.tx_tot_bytes);
	traffic_stats->bytes_recv = (uint64)dtoh64(sta_info.rx_tot_bytes);
	traffic_stats->packets_sent = (unsigned int)dtoh32(sta_info.tx_tot_pkts);
	traffic_stats->packets_recv = (unsigned int)dtoh32(sta_info.rx_tot_pkts);
	traffic_stats->tx_packet_err = (unsigned int)dtoh32(sta_info.tx_failures);
	traffic_stats->rx_packet_err = (unsigned int)dtoh32(sta_info.rx_decrypt_failures);
	traffic_stats->retransmission_count = (unsigned int)dtoh32(sta_info.tx_pkts_retried);

	WBD_INFO("Ifname[%s] BSS["MACDBG"] STA["MACDBG"] RCPI[%d (%d)]  Downlink[%u] Uplink[%u] "
		"BytesSent[%llu] BytesRecv[%llu] PacketsSent[%u] PacketsRecv[%u] "
		"TxPacketErrors[%u] RxPacketErrors[%u] RetransmissionCount[%u]\n",
		ifname, MAC2STRDBG(bssid), MAC2STRDBG(sta_mac), metric->rcpi,
		WBD_RCPI_TO_RSSI(metric->rcpi), metric->downlink_rate, metric->uplink_rate,
		traffic_stats->bytes_sent, traffic_stats->bytes_recv, traffic_stats->packets_sent,
		traffic_stats->packets_recv, traffic_stats->tx_packet_err,
		traffic_stats->rx_packet_err, traffic_stats->retransmission_count);

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
			cmd.vndr_metrics.tx_tot_failures = (uint32)dtoh32(sta_info.tx_failures);
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

				if (dtoh32(sta_info.rx_tot_pkts) > sta->stats.rx_tot_pkts) {
					/* rx_rate shall be aggregated into datarate calculation */
					if (dtoh64(sta_info.rx_tot_bytes) >
						sta->stats.rx_tot_bytes) {
						gap = now - sta->stats.active;
						if (gap > 0) {
							rx_rate = (((dtoh64(sta_info.rx_tot_bytes) -
								sta->stats.rx_tot_bytes) * 8) /
								(gap * 1000));
							sta->stats.idle_rate += rx_rate;
						}
					}
				}
				WBD_DEBUG("BSS["MACDBG"] STA["MACDBG"] datarate[%d]\n",
					MAC2STRDBG(bssid), MAC2STRDBG(sta_mac),
					sta->stats.idle_rate);

				sta->stats.rx_tot_pkts = (uint32)dtoh32(sta_info.rx_tot_pkts);
				sta->stats.rx_tot_bytes = (uint64)dtoh64(sta_info.rx_tot_bytes);
				cmd.vndr_metrics.idle_rate = (uint32)(sta->stats.idle_rate);
			}
		}

		/* Get Dual band supported flag */
		if (WBD_IS_DUAL_BAND(sta->band)) {
			cmd.vndr_metrics.sta_cap |= WBD_TLV_ASSOC_STA_CAPS_FLAG_DUAL_BAND;
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
	unsigned int assoc_frame_len, uint16 reason)
{
	return (ieee1905_sta_assoc_disassoc((unsigned char*)bssid, (unsigned char*)mac, isAssoc,
		time_elapsed, notify, assoc_frame, assoc_frame_len, reason));
}

#if defined(MULTIAPR2)
/* STA assoc Failed connection */
int
blanket_sta_assoc_failed_connection(struct ether_addr *mac, uint16 status)
{
	return ieee1905_sta_assoc_failed_connection((unsigned char *)mac, status);
}
#endif /* MULTIAPR2 */

/* Add All STA's Currently Associated with the BSS */
int
blanket_add_all_associated_stas(char *ifname, struct ether_addr *bssid)
{
	int ret = 0;
	int iter_sta;
	unsigned short time_elapsed = 0;
	sta_info_t sta_info;
	struct maclist* list = NULL;
	WBD_ENTER();

	list = (struct maclist*)wbd_malloc(WLC_IOCTL_MAXLEN, &ret);
	WBD_ASSERT();

	/* Fill assoc sta list if any */
	ret =  blanket_get_assoclist(ifname, list, WLC_IOCTL_MAXLEN);
	WBD_ASSERT();

	for (iter_sta = 0; iter_sta < list->count; iter_sta++) {
		/* Get the time elapsed since the assoc */
		ret = blanket_get_sta_info(ifname, &list->ea[iter_sta], &sta_info);
		if (ret != 0) {
			time_elapsed = 0;
		} else {
			time_elapsed = (unsigned short)dtoh32(sta_info.in);
		}

		ret = ieee1905_sta_assoc_disassoc((unsigned char*)bssid,
			(unsigned char*)&list->ea[iter_sta], 1, time_elapsed, 0,
			NULL, 0, 0);
		if (ret != 0) {
			WBD_WARNING("Ifname[%s] BSSID["MACF"] STA["MACF"] Failed to Add STA to "
				"MultiAP Error : %d\n",
				ifname, ETHERP_TO_MACF(bssid), ETHER_TO_MACF(list->ea[iter_sta]),
				ret);
		}
	}

end:
	if (list) {
		free(list);
	}

	WBD_EXIT();
	return ret;
}

/* Check if a STA is in assoclist of the BSS or not. Add if present */
int
blanket_check_and_add_sta_from_assoclist(char *ifname, struct ether_addr *bssid,
	struct ether_addr *sta_mac)
{
	int ret = 0;
	int iter_sta;
	unsigned short time_elapsed = 0;
	sta_info_t sta_info;
	struct maclist* list = NULL;
	i5_dm_bss_type *i5_bss;
	WBD_ENTER();

	/* Choose BSS based on BSSID */
	WBD_DS_GET_I5_SELF_BSS((unsigned char*)bssid, i5_bss, &ret);
	WBD_ASSERT_MSG("BSS["MACF"] %s\n", ETHERP_TO_MACF(bssid), wbderrorstr(ret));

	/* If STA already found do not raise event */
	if (wbd_ds_find_sta_in_bss_assoclist(i5_bss, sta_mac, &ret, NULL)) {
		WBD_INFO("%s: Entry for associated STA "MACF" is already present in BSS "MACF". "
			"Skip adding\n", ifname, ETHERP_TO_MACF(sta_mac), ETHERP_TO_MACF(bssid));
		goto end;
	}

	list = (struct maclist*)wbd_malloc(WLC_IOCTL_MAXLEN, &ret);
	WBD_ASSERT();

	/* Fill assoc sta list if any */
	ret =  blanket_get_assoclist(ifname, list, WLC_IOCTL_MAXLEN);
	WBD_ASSERT();

	for (iter_sta = 0; iter_sta < list->count; iter_sta++) {
		if (eacmp(sta_mac, &list->ea[iter_sta]) != 0) {
			continue;
		}

		WBD_WARNING("%s: Entry for associated STA "MACF" missing in BSS "MACF". Adding it "
			"by raising an ASSOC event.\n",
			ifname, ETHERP_TO_MACF(sta_mac), ETHERP_TO_MACF(bssid));

		/* Get the time elapsed since the assoc */
		ret = blanket_get_sta_info(ifname, &list->ea[iter_sta], &sta_info);
		if (ret != 0) {
			time_elapsed = 0;
		} else {
			time_elapsed = (unsigned short)dtoh32(sta_info.in);
		}

		ret = ieee1905_sta_assoc_disassoc((unsigned char*)bssid,
			(unsigned char*)&list->ea[iter_sta], 1, time_elapsed, 0,
			NULL, 0, 0);
		if (ret != 0) {
			WBD_WARNING("Ifname[%s] BSSID["MACF"] STA["MACF"] Failed to Add STA to "
				"MultiAP Error : %d\n",
				ifname, ETHERP_TO_MACF(bssid), ETHER_TO_MACF(list->ea[iter_sta]),
				ret);
		}
		break;
	}

end:
	if (list) {
		free(list);
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

	sta->stats.tx_rate = dtoh32(sta_info.tx_rate) / 1000;
	sta->stats.old_tx_tot_failures = sta->stats.tx_tot_failures;
	sta->stats.tx_tot_failures = (uint32)dtoh32(sta_info.tx_failures);
	sta->stats.tx_failures = sta->stats.tx_tot_failures - sta->stats.old_tx_tot_failures;

	memset(&ctr, 0, sizeof(ctr));
	if (blanket_get_bs_data_counters(ifname, (struct ether_addr*)i5_sta->mac, &ctr)
		== WBDE_OK) {

		/* Calculate Data rate */
		sta->stats.idle_rate = (ctr.time_delta) ?
			((ctr.throughput * 8000) / ctr.time_delta) : 0;

		if (dtoh32(sta_info.rx_tot_pkts) > sta->stats.rx_tot_pkts) {
			/* rx_rate shall be aggregated into datarate calculation */
			if (dtoh64(sta_info.rx_tot_bytes) > sta->stats.rx_tot_bytes) {
				gap = now - sta->stats.active;
				if (gap > 0) {
					rx_rate = (((dtoh64(sta_info.rx_tot_bytes) -
						sta->stats.rx_tot_bytes) * 8) /
						(gap * 1000));
					sta->stats.idle_rate += rx_rate;
				}
			}
		}

		sta->stats.rx_tot_pkts = (uint32)dtoh32(sta_info.rx_tot_pkts);
		sta->stats.rx_tot_bytes = (uint64)dtoh64(sta_info.rx_tot_bytes);
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
	char maclist_buf[WLC_IOCTL_MAXLEN], prefix[IFNAMSIZ];
	char var[80], tmp[100], *next, *maclist_str = "";
	struct ether_addr *ea;
	maclist_t *maclist = NULL;
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
				blanket_get_interface_prefix(iter_bss->ifname,
					prefix, sizeof(prefix));

				/* Get MACMODE from NVRAM */
				(void)strcat_r(prefix, NVRAM_MACMODE, tmp);
				if (nvram_match(tmp, "deny")) {
					macmode = WLC_MACMODE_DENY;
				} else if (nvram_match(tmp, "allow")) {
					macmode = WLC_MACMODE_ALLOW;
				} else {
					macmode = WLC_MACMODE_DISABLED;
				}

				/* Get MACLIST from NVRAM */
				blanket_get_config_val_str(prefix,
					NVRAM_MACLIST, "", &maclist_str);

				/* Get MAC_FILTER from NVRAM */
				mac_filter = blanket_get_config_val_int(prefix,
					NVRAM_PROBRESP_MF, 0);

			} else {
				macmode = WLC_MACMODE_ALLOW;
				mac_filter = 1;
			}

			macmode = htod32(macmode);
			ret = wl_ioctl(iter_bss->ifname, WLC_SET_MACMODE,
				&macmode, sizeof(macmode));
			WBD_INFO("Ifname[%s] %s BSS["MACDBG"] MACMODE[%d] Flags[%x] ret[%d]\n",
				iter_bss->ifname, unblock ? "Unblocking" : "Blocking",
				MAC2STRDBG(iter_bss->BSSID), macmode, bss_vndr_data->flags, ret);

			memset(maclist_buf, 0, WLC_IOCTL_MAXLEN);
			maclist = (maclist_t *)maclist_buf;
			maclist->count = 0;
			ea = maclist->ea;
			foreach(var, maclist_str, next) {
				if (((char *)((&ea[1])->octet)) >
					((char *)(&maclist_buf[sizeof(maclist_buf)]))) {
					break;
				}
				if (ether_atoe(var, ea->octet)) {
					maclist->count++;
					ea++;
					WBD_INFO("Ifname[%s] %s BSS["MACDBG"] "
						"MAC[%d]=[%s]\n",
						iter_bss->ifname,
						unblock ? "Unblocking" : "Blocking",
						MAC2STRDBG(iter_bss->BSSID),
						(maclist->count - 1), var);
				}
			}
			maclist->count = htod32(maclist->count);
			ret = wl_ioctl(iter_bss->ifname, WLC_SET_MACLIST,
				maclist, (ETHER_ADDR_LEN * maclist->count + sizeof(uint32)));
			WBD_INFO("Ifname[%s] %s BSS["MACDBG"] MACLIST_CNT[%d] Flags[%x] ret[%d]\n",
				iter_bss->ifname, unblock ? "Unblocking" : "Blocking",
				MAC2STRDBG(iter_bss->BSSID), maclist->count,
				bss_vndr_data->flags, ret);

			ret = wl_iovar_setint(iter_bss->ifname, "probresp_mac_filter", mac_filter);
			WBD_INFO("Ifname[%s] %s BSS["MACDBG"] probresp_mac_filter[%d] Flags[%x] "
				"ret[%d]\n", iter_bss->ifname, unblock ? "UnBlocking" : "Blocking",
				MAC2STRDBG(iter_bss->BSSID), mac_filter, bss_vndr_data->flags, ret);

			/* Disassociate all STAs if we are blocking the bh BSS */
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

/* We need to disable or enable the roaming. If the i5_ifr is provided and the operation is disable
 * the roaming, then disable in all the other STA interfaces except i5_ifr
 */
void
wbd_ieee1905_set_bh_sta_params(t_i5_bh_sta_cmd cmd, i5_dm_interface_type *i5_ifr)
{
	int ret = WBDE_OK;
	i5_dm_device_type *i5_self_device;
	i5_dm_interface_type *i5_iter_ifr;
	wbd_ifr_item_t *ifr_vndr_data;
	char tmp[256];
	WBD_ENTER();

	if ((cmd == IEEE1905_BH_STA_ROAM_ENAB_VAP_FOLLOW) &&
		(i5_iter_ifr = wbd_slave_is_any_bsta_associated(FALSE, TRUE))) {
		WBD_INFO("Command for VAP to follow but STA["MACF"] is associated\n\n",
			ETHERP_TO_MACF(i5_iter_ifr->InterfaceId));
		goto end;
	}

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
				if (i5_iter_ifr == i5_ifr) {
					break;
				}
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
				i5_iter_ifr->roam_enabled = FALSE;
				break;
			case IEEE1905_BH_STA_ROAM_ENAB_VAP_FOLLOW:
				/* If the interface is provided, enable only on that interface */
				if ((i5_ifr != NULL) && (i5_iter_ifr != i5_ifr)) {
					break;
				}
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
				i5_iter_ifr->roam_enabled = TRUE;
				break;
			default:
				WBD_ERROR("Unknown backhaul STA configuration cmd: %d\n", cmd);
				break;
		}
	}
end:
	WBD_EXIT();
}

/* Get the interface metric like channel utilization and noise ect.. */
int
wbd_blanket_get_interface_metric(char *ifname, ieee1905_interface_metric *metric)
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
	metric->chan_util = (unsigned char)((busy * 255) / 100);
#if defined(MULTIAPR2)
	metric->noise = (unsigned char)(WBD_RSSI_TO_RCPI(list->stats[0].bgnoise));
	metric->transmit = (unsigned char)((list->stats[0].ccastats[0] * 255) / 100);
	metric->receive_self = (unsigned char)((list->stats[0].ccastats[1] * 255) / 100);
	metric->receive_other = (unsigned char)((list->stats[0].ccastats[2] * 255) / 100);
	WBD_DEBUG("Ifname[%s] Busy[%d] chan_util[%d] Noise[%d/%d] Transmit[%d/%d] "
		"ReceiveSelf[%d/%d] ReceiveOther[%d/%d]\n", ifname, busy, metric->chan_util,
		metric->noise, list->stats[0].bgnoise, metric->transmit, list->stats[0].ccastats[0],
		metric->receive_self, list->stats[0].ccastats[1], metric->receive_other,
		list->stats[0].ccastats[2]);
#endif /* MULTIAPR2 */

end:
	if (data_buf) {
		free(data_buf);
	}
	WBD_EXIT();
	return ret;
}

#if defined(MULTIAPR2)
/* Get backhaul STA profile of the connected device from sta_info. The ifanme can be WDS ifname
 * in case of upstream AP or backhaul STA interface name
 */
unsigned char
wbd_blanket_get_bh_sta_profile(char *ifname)
{
	int ret = WBDE_OK;
	char wl_ifname[IFNAMSIZ] = {0};
	unsigned char profile = 0, mac[MAC_ADDR_LEN] = {0};
	sta_info_t sta_info;
	WBD_ENTER();

	/* If it's WDS interface, then get the MAC address of the backhaul STA connected to
	 * this backhaul AP
	 */
	if (!strncmp(ifname, "wds", strlen("wds"))) {
		wl_wlif_wds_ap_ifname(ifname, wl_ifname);

		blanket_get_bh_sta_mac_from_wds(wl_ifname, ifname, mac);
		WBD_DEBUG("ifname[%s] wlname[%s] MAC["MACDBG"]\n", ifname, wl_ifname,
			MAC2STRDBG(mac));

		/* Profile information is in map_flags of sta_info. Get it */
		ret = blanket_get_sta_info(wl_ifname, (struct ether_addr*)mac, &sta_info);
	} else {
		/* If it is backhaul STA then get the BSSID */
		blanket_get_bssid(ifname, (struct ether_addr*)mac);

		/* Profile information is in map_flags of sta_info. Get it */
		ret = blanket_get_sta_info(ifname, (struct ether_addr*)mac, &sta_info);
	}
	WBD_ASSERT();

	if (sta_info.map_flags & WL_MAP_STA_PROFILE2) {
		WBD_INFO("ifname[%s] wlname[%s] MAC["MACDBG"] its profile2\n",
			ifname, wl_ifname, MAC2STRDBG(mac));
		profile = ieee1905_map_profile2;
	} else {
		WBD_INFO("ifname[%s] wlname[%s] MAC["MACDBG"] map_flags[%d]\n",
			ifname, wl_ifname, MAC2STRDBG(mac), sta_info.map_flags);
	}

end:
	WBD_EXIT();
	return profile;
}

/* set or unset association disallowed attribute in beacon */
int
wbd_blanket_mbo_assoc_disallowed(char *ifname, unsigned char reason)
{
	int ret = WBDE_OK;
	WBD_ENTER();

	ret = blanket_mbo_assoc_disallowed(ifname, reason);
	WBD_ASSERT();

end:
	WBD_EXIT();
	return ret;
}

/* Set DFS channel clear, as indicated by the controller */
void
wbd_blanket_set_dfs_chan_clear(char *ifname, ieee1905_chan_pref_rc_map *chan_pref)
{
	chanspec_t chanspec;
	int i;
	uint bw = 0;

	if (chan_pref->pref == 0 || chan_pref->reason != I5_REASON_CHAN_CLEAR_IND) {
		WBD_ERROR("Channel Prefernce[%d] or reason[%d] not valid\n",
			chan_pref->pref, chan_pref->reason);
		return;
	}

	WBD_DEBUG("[%s]:Set DFS channel clear for regclass [%d] Number of channels [%d]\n",
		ifname, chan_pref->regclass, chan_pref->count);

	blanket_get_bw_from_rc(chan_pref->regclass, &bw);
	for (i = 0; i < chan_pref->count; i++) {
		if (chan_pref->regclass <= REGCLASS_40MHZ_LAST) {
			chanspec = wf_channel2chspec(chan_pref->channel[i], bw,
				WL_CHANNEL_2G5G_BAND(chan_pref->channel[i]));
		} else {
			chanspec = CHBW_CHSPEC(bw, chan_pref->channel[i]);
		}
		WBD_DEBUG("Clearing for channel [%d] chanspec [0x%x]\n",
			chan_pref->channel[i], chanspec);
		blanket_set_chan_info(ifname, chanspec, 0);
	}
}

#endif /* MULTIAPR2 */

/* Update the mapflags of all the wireless interfaces */
static void
wbd_slave_update_map_flags_of_all_ifr(i5_dm_device_type *i5_device)
{
	i5_dm_interface_type *i5_iter_ifr = NULL;

	/* update the MAP Flags of all the wireless interfaces */
	foreach_i5glist_item(i5_iter_ifr, i5_dm_interface_type, i5_device->interface_list) {

		/* Skip non wireless interfaces */
		if (!i5DmIsInterfaceWireless(i5_iter_ifr->MediaType)) {
			continue;
		}

		i5_iter_ifr->mapFlags = wbd_get_map_flags(i5_iter_ifr->prefix);
		WBD_DEBUG("ifname[%s] mapflags[%x]\n", i5_iter_ifr->ifname, i5_iter_ifr->mapFlags);
	}
}

/* Check if any of the STA interface is connected to upstream AP */
i5_dm_interface_type*
wbd_slave_is_any_bsta_associated(bool update_map_flags, bool get_bssid)
{
	int ret = WBDE_OK;
	i5_dm_device_type *i5_self_dev = NULL;
	i5_dm_interface_type *i5_iter_ifr = NULL;
	struct ether_addr bssid;

	WBD_SAFE_GET_I5_SELF_DEVICE(i5_self_dev, &ret);

	/* first update the MAP Flags of all the wireless interfaces if not updated. This is
	 * because, when we call this function before start messaging, these flags are not updated.
	 */
	if (update_map_flags) {
		wbd_slave_update_map_flags_of_all_ifr(i5_self_dev);
	}

	/* Go through each interface */
	foreach_i5glist_item(i5_iter_ifr, i5_dm_interface_type, i5_self_dev->interface_list) {

		/* Only for STA interface */
		if (!I5_IS_BSS_STA(i5_iter_ifr->mapFlags)) {
			continue;
		}

		if (get_bssid) {
			/* If the BSSID is not NULL for a interface, then it is connected to
			 * upstrem AP
			 */
			if (blanket_get_bssid(i5_iter_ifr->ifname, (struct ether_addr*)&bssid)
				== WBDE_OK) {
				if (!(ETHER_ISNULLADDR((struct ether_addr*)&bssid))) {
					WBD_INFO("ifname[%s] is associated\n", i5_iter_ifr->ifname);
					return i5_iter_ifr;
				}
			}
		} else {
			if (!ETHER_ISNULLADDR(i5_iter_ifr->bssid)) {
				WBD_INFO("Yes %s is associated\n", i5_iter_ifr->ifname);
				return i5_iter_ifr;
			}
		}
	}

end:
	return NULL;
}

/* Set all backhaul STA credentials by extracting it from backhaul BSS.
 * This is helpful if the onboarding was over the Ethernet
 */
void wbd_set_bh_sta_cred_from_bh_bss(ieee1905_client_bssinfo_type *bss)
{
	wlif_wps_nw_creds_t creds;

	if (!bss) {
		return;
	}

	memset(&creds, 0, sizeof(creds));
	memcpy(creds.ssid, bss->ssid.SSID, bss->ssid.SSID_len);
	memcpy(creds.nw_key, bss->NetworkKey.key, bss->NetworkKey.key_len);

	if (bss->AuthType & IEEE1905_AUTH_WPAPSK) {
		creds.akm |= WLIF_WPA_AKM_PSK;
	}
	if ((bss->AuthType & IEEE1905_AUTH_WPA2PSK) ||
		(bss->AuthType & IEEE1905_AUTH_SAE)) {
		creds.akm |= WLIF_WPA_AKM_PSK2;
	}

	if (bss->EncryptType == IEEE1905_ENCR_TKIP) {
		creds.encr |= WLIF_WPA_ENCR_TKIP;
	} else if (bss->EncryptType == IEEE1905_ENCR_AES) {
		creds.encr |= WLIF_WPA_ENCR_AES;
	}
	wl_wlif_map_configure_backhaul_sta_interface(NULL, &creds);
}

/* Set roaming state. If RSSI based roaming, set roam trigger. state values allowed
 * 0 which is all roaming allowed(WLC_ROAM_STATE_ENABLED)
 * 1 Roaming disabled(WLC_ROAM_STATE_DISABLED)
 * 2 low RSSI roaming(WLC_ROAM_STATE_LOW_RSSI)
 */
void
wbd_slave_set_bh_sta_roam_state(int state)
{
	int ret = WBDE_OK, roam_delta, rssi_thld;
	i5_dm_device_type *i5_self_dev = NULL;
	i5_dm_interface_type *i5_iter_ifr = NULL;
	char prefix[IFNAMSIZ], *nvram_val, tmp[NVRAM_MAX_PARAM_LEN];

	/* Only if the WPA supplicant is taking care of STA connection */
	if (!WBD_IS_HAPD_ENABLED(wbd_get_ginfo()->flags)) {
		goto end;
	}

	WBD_SAFE_GET_I5_SELF_DEVICE(i5_self_dev, &ret);

	/* Go through each interface */
	foreach_i5glist_item(i5_iter_ifr, i5_dm_interface_type, i5_self_dev->interface_list) {

		/* Only for STA interface */
		if (!I5_IS_BSS_STA(i5_iter_ifr->mapFlags)) {
			continue;
		}

		/* Get Prefix from OS specific interface name */
		blanket_get_interface_prefix(i5_iter_ifr->ifname, prefix, sizeof(prefix));
		WBD_DEBUG("ifname %s prefix %s is backhaul STA and state to set is %d\n",
			i5_iter_ifr->ifname, prefix, state);

		/* state 2 means low RSSI roaming, set the roam trigger and roam delta also */
		if (state == 2) {
			/* Get the roam trigger RSSI threshold */
			rssi_thld = WBD_DEF_BH_STA_ROAM_TRIGGER;
			if ((nvram_val = nvram_get(strcat_r(prefix, NVRAM_ROAM_TRIGGER, tmp)))) {
				rssi_thld = atoi(nvram_val);
			}
			WBD_INFO("ifname %s prefix %s is backhaul STA. roam.val %d\n",
				i5_iter_ifr->ifname, prefix, rssi_thld);

			/* zero means do not set roam trigger and do not enable roaming */
			if (rssi_thld >= 0) {
				continue;
			}

			/* Enable firmware roaming only for low RSSI(WLC_ROAM_STATE_LOW_RSSI) */
			blanket_set_roam_off(i5_iter_ifr->ifname, state);

			/* Set roam trigger */
			blanket_set_roam_trigger(i5_iter_ifr->ifname, rssi_thld);

			/* Set roam delta if the NVRAM is defined */
			if ((nvram_val = nvram_get(strcat_r(prefix, NVRAM_ROAM_DELTA, tmp)))) {
				roam_delta = atoi(nvram_val);
				blanket_set_roam_delta(i5_iter_ifr->ifname, roam_delta);
				WBD_INFO("ifname %s prefix %s is backhaul STA. "
					"set roam delta %d\n", i5_iter_ifr->ifname, prefix,
					roam_delta);
			}
		} else {
			/* Set firmware roaming state */
			blanket_set_roam_off(i5_iter_ifr->ifname, state);
			WBD_INFO("ifname %s prefix %s is backhaul STA. Set roam state to %d\n",
				i5_iter_ifr->ifname, prefix, state);
		}
	}

end:
	return;
}

/* Disconnect all the backhaul STA interfaces except i5_ifr */
void
wbd_slave_disconnect_all_bstas(i5_dm_interface_type *i5_ifr)
{
	int ret = WBDE_OK;
	i5_dm_device_type *i5_self_device;
	i5_dm_interface_type *i5_iter_ifr;
	wbd_ifr_item_t *ifr_vndr_data;
	char tmp[WBD_MAX_SUP_CMD_LEN];
	WBD_ENTER();

	WBD_SAFE_GET_I5_SELF_DEVICE(i5_self_device, &ret);

	foreach_i5glist_item(i5_iter_ifr, i5_dm_interface_type, i5_self_device->interface_list) {
		if (!I5_IS_BSS_STA(i5_iter_ifr->mapFlags)) {
			continue;
		}
		if (i5_iter_ifr == i5_ifr) {
			continue;
		}
		WBD_ASSERT_ARG(i5_iter_ifr->vndr_data, WBDE_INV_ARG);
		ifr_vndr_data = (wbd_ifr_item_t*)i5_iter_ifr->vndr_data;

		WBD_INFO("ifname : %s Disconnect backhaul STA("MACF")\n",
			i5_iter_ifr->ifname, ETHERP_TO_MACF(i5_iter_ifr->InterfaceId));
		blanket_set_keep_ap_up(i5_iter_ifr->ifname, 1); /* VAP UP */

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

		blanket_deauth_sta(i5_iter_ifr->ifname, NULL, 0);
	}
end:
	WBD_EXIT();
}
