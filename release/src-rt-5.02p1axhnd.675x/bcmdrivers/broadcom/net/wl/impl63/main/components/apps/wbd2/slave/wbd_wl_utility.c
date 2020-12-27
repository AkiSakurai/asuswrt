/*
 * WBD WL utility for both Master and Slave (Linux)
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
 * $Id: wbd_wl_utility.c 778661 2019-09-06 12:28:01Z $
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

#include "blanket.h"

#include "wbd.h"
#include "wbd_shared.h"
#include "wbd_ds.h"
#include "wbd_wl_utility.h"

/* -------------------------------------- Utility Macros -------------------------------------- */
#define WBD_DOT11_ACTION_CAT_INVALID		125

#define WBD_IOVAR_ASSERT(iovar) \
	if (ret < 0) { \
		WBD_WARNING("%s : Failed to %s, WL error : %d\n", ifname, (iovar), ret); \
		ret = WBDE_WL_ERROR; \
		goto end; \
	}
/* -------------------------------------- Utility Macros -------------------------------------- */

/* Fetch Interface Info for AP */
int
wbd_wl_fill_interface_info(wbd_wl_interface_t *ifr_info)
{
	int ret = WBDE_OK;
	wl_bss_info_t *bss_info;
#if defined(MULTIAPR2)
	int bsscfg_idx;
#endif /* MULTIAPR2 */
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(ifr_info->ifr.ifr_name, WBDE_INV_ARG);

	/* Check interface (for valid BRCM wl interface) */
	(void)blanket_probe(ifr_info->ifr.ifr_name);

	/* Get Bridge from interface name */
	ret = wbd_get_ifname_bridge(ifr_info->ifr.ifr_name, &ifr_info->bridge_type);
	WBD_ASSERT();

	/* Get prefix of the interface from Driver */
	blanket_get_interface_prefix(ifr_info->ifr.ifr_name,
		ifr_info->prefix, sizeof(ifr_info->prefix));

	/* Get MAC */
	ret = blanket_get_bss_mac(ifr_info->ifr.ifr_name, &ifr_info->mac);
	WBD_ASSERT();

	/* Get BSSID */
	ret = blanket_try_get_bssid(ifr_info->ifr.ifr_name, WBD_MAX_GET_BSSID_TRY,
		WBD_GET_BSSID_USECOND_GAP, &ifr_info->bssid);
	WBD_ASSERT();

	/* Get BSS Info */
	ret = blanket_get_bss_info(ifr_info->ifr.ifr_name, &bss_info);
	WBD_ASSERT();

	/* Fill bssid_info fields for bss transition */
	ret = blanket_get_bssid_info_field(ifr_info->ifr.ifr_name, bss_info, &ifr_info->bssid_info);

	/* Get phytype */
	ret = blanket_get_phy_type(ifr_info->ifr.ifr_name, bss_info, &ifr_info->phytype);
	WBD_ASSERT();

	/* Parse BSS Info ---------------------------------- START */
	/* Get Chanspec */
	memcpy(&ifr_info->chanspec, &bss_info->chanspec, sizeof(bss_info->chanspec));

	/* Get BSSID */
	memcpy(&ifr_info->bssid, &bss_info->BSSID, sizeof(ifr_info->bssid));

	/* Get SSID */
	memcpy(ifr_info->blanket_ssid.SSID, bss_info->SSID, bss_info->SSID_len);
	ifr_info->blanket_ssid.SSID_len = bss_info->SSID_len;

	/* Get RSSI */
	ifr_info->RSSI = (int16)(dtoh16(bss_info->RSSI));
	/* Parse BSS Info ---------------------------------- END */

	/* Get rClass */
	ret = blanket_get_global_rclass(ifr_info->chanspec, (uint8 *)&ifr_info->rclass);
	WBD_ASSERT();

	/* Initialize Slave's IP Address */
	snprintf(ifr_info->slave_ip, sizeof(ifr_info->slave_ip), "%s", WBD_LOOPBACK_IP);

	/* get the primary prefix info */
	ret = blanket_get_radio_prefix(ifr_info->ifr.ifr_name,
		ifr_info->primary_prefix, sizeof(ifr_info->primary_prefix));
	WBD_ASSERT();

	/* Get the primary radio interface MAC address */
	blanket_get_radio_mac(ifr_info->primary_prefix, &ifr_info->radio_mac);

	/* Get max_nss */
	blanket_get_max_nss(ifr_info->ifr.ifr_name, bss_info, &ifr_info->max_nss);

	/* Get bridge address */
	wbd_get_bridge_mac(&ifr_info->br_addr);

	/* Get apsta mode */
	blanket_get_apsta_mode(ifr_info->ifr.ifr_name, &ifr_info->apsta);

	/* Get the max no of assoclist */
	(void)blanket_get_max_assoc(ifr_info->ifr.ifr_name, &ifr_info->maxassoc);

#if defined(MULTIAPR2)
	/* Do not send BTM request from firmware for the BTM Query from a STA. Let application
	 * handle it if the flag is defined
	 */
	if (wbd_get_ginfo()->wbd_slave->flags & WBD_BKT_SLV_FLAGS_WNM_NO_BTQ_RESP) {
		/* Get the interface subunit */
		if (get_ifname_unit(ifr_info->ifr.ifr_name, NULL, &bsscfg_idx) != 0) {
			WBD_ERROR("ifname(%s): unable to parse unit.subunit\n",
				ifr_info->ifr.ifr_name);
		} else {
			if (bsscfg_idx == -1)
				bsscfg_idx = 0;
			wl_bssiovar_setint(ifr_info->ifr.ifr_name, "wnm_no_btq_resp",
				bsscfg_idx, 1);
		}
	}
#endif /* MULTIAPR2 */

	/* Slave's status changed to Enabled, as for this band ifname is present in wbd_ifnames */
	ifr_info->enabled = 1;
	WBD_DEBUG("ifname[%s] Bridge[%d] Prefix[%s] PrimaryPrifix[%s] Chanspec[0x%x] MAC["MACF"] "
		"BSSID["MACF"] SSID[%s] RSSI[%d] RClass[0x%x] SlaveIP[%s] Enabled[%d] "
		"Max Nss[%d] BridgeMAC["MACF"] BSSIDInfo[0x%x] RadioMAC["MACF"]\n",
		ifr_info->ifr.ifr_name, ifr_info->bridge_type,
		ifr_info->prefix, ifr_info->primary_prefix,
		ifr_info->chanspec, ETHER_TO_MACF(ifr_info->mac), ETHER_TO_MACF(ifr_info->bssid),
		ifr_info->blanket_ssid.SSID, ifr_info->RSSI, ifr_info->rclass, ifr_info->slave_ip,
		ifr_info->enabled, ifr_info->max_nss, ETHER_TO_MACF(ifr_info->br_addr),
		ifr_info->bssid_info, ETHER_TO_MACF(ifr_info->radio_mac));

end:
	/* If any Error reading interface info, Change Slave's status to Disabled */
	if (ret != WBDE_OK) {
		ifr_info->enabled = 0;
		WBD_DEBUG("ifname[%s] Failed to read interface info. Enabled[%d]\n",
			ifr_info->ifr.ifr_name, ifr_info->enabled);
	}
	WBD_EXIT();
	return ret;
}

bcm_tlv_t *
wbd_wl_find_ie(uint8 id, void *tlvs, int tlvs_len, const char *voui, uint8 *type, int type_len)
{
	bcm_tlv_t *ie;
	uint8 ie_len;

	ie = (bcm_tlv_t*)tlvs;

	/* make sure we are looking at a valid IE */
	if (ie == NULL || !bcm_valid_tlv(ie, tlvs_len)) {
		return NULL;
	}

	/* Walk through the IEs looking for an OUI match */
	do {
		ie_len = ie->len;
		if ((ie->id == id) && (ie_len >= (DOT11_OUI_LEN + type_len)) &&
			!bcmp(ie->data, voui, DOT11_OUI_LEN)) {
			/* compare optional type */
			if (type_len == 0 || !bcmp(&ie->data[DOT11_OUI_LEN], type, type_len)) {
				return (ie);		/* a match */
			}
		}
	} while ((ie = bcm_next_tlv(ie, &tlvs_len)) != NULL);

	return NULL;
}

/* Send assoc decision */
int
wbd_wl_send_assoc_decision(char *ifname, uint32 info_flags, uint8 *data, int len,
	struct ether_addr *sta_mac, uint32 evt_type)
{
	int total_len = 0, ret = WBDE_OK;
	assoc_decision_t *dc = NULL;
	uchar *buf = NULL;
	multiap_ie_t *map_ie = NULL;
	uint8 ie_type = MAP_IE_TYPE;
	WBD_ENTER();

	wl_endian_probe(ifname);

	WBD_DEBUG("%s : Recieved Event %s for "MACF"\n", ifname,
		((evt_type == WLC_E_PRE_ASSOC_IND)?"WLC_E_PRE_ASSOC_IND":"WLC_E_PRE_REASSOC_IND"),
		ETHERP_TO_MACF(sta_mac));
	if (evt_type == WLC_E_PRE_ASSOC_IND) {
		data += DOT11_ASSOC_REQ_FIXED_LEN;
		len -= DOT11_ASSOC_REQ_FIXED_LEN;
	} else if (evt_type == WLC_E_PRE_REASSOC_IND) {
		data +=  DOT11_REASSOC_REQ_FIXED_LEN;
		len -= DOT11_REASSOC_REQ_FIXED_LEN;
	}

	total_len = sizeof(assoc_decision_t) + WLC_DC_DWDS_DATA_LENGTH - 1;
	buf = (uchar*)wbd_malloc(total_len, &ret);
	WBD_ASSERT();

	dc = (assoc_decision_t *) buf;
	bcopy(sta_mac, &dc->da, ETHER_ADDR_LEN);

	/* Find multiap IE with WFA_OUI */
	map_ie = (multiap_ie_t*)wbd_wl_find_ie(DOT11_MNG_VS_ID, data, len, WFA_OUI, &ie_type, 1);
	if ((map_ie) && (!WBD_UAP_ENAB(info_flags)) && (map_ie->len > MIN_MAP_IE_LEN) &&
		(map_ie->attr[MAP_ATTR_VAL_OFFESET] & IEEE1905_BACKHAUL_STA)) {
		/* Found multiap backhaul sta, Assoc reject */
		WBD_DEBUG("%s : BRCM PSR addr("MACF")\n", ifname, ETHERP_TO_MACF(sta_mac));
		dc->assoc_approved = FALSE;
		dc->reject_reason = DOT11_SC_ASSOC_FAIL;
	} else {
		dc->assoc_approved = TRUE;
		dc->reject_reason = 0;
	}
	WBD_DEBUG("%s: addr("MACF") association %s \n", ifname, ETHERP_TO_MACF(sta_mac),
		dc->assoc_approved ? "ACCEPT" : "REJECT");
	ret = wl_iovar_set(ifname, "assoc_decision", dc, total_len);
	WBD_IOVAR_ASSERT("assoc_decision");

end:
	if (buf)
		free(buf);

	WBD_EXIT();
	return ret;
}

/* Send Action frame to STA, measure rssi from response */
int
wbd_wl_actframe_to_sta(char* ifname, struct ether_addr* ea)
{
	int ret = WBDE_OK;
	wl_action_frame_t * action_frame;
	wl_af_params_t * af_params;
	char smbuf[WBD_MAX_BUF_8192];
	uint namelen;
	uint iolen;
	dot11_rmreq_t *rmreq;
	dot11_rmreq_stat_t *sreq;
	WBD_ENTER();

	wl_endian_probe(ifname);
	af_params = wbd_malloc(WL_WIFI_AF_PARAMS_SIZE, &ret);
	WBD_ASSERT();

	af_params->channel = 0;
	af_params->dwell_time = htod32(-1);
	action_frame = &af_params->action_frame;
	/* Add the packet Id */
	action_frame->packetId = htod32((uint32)(uintptr)action_frame);

	memcpy(&action_frame->da, ea, sizeof(action_frame->da));
	/* set default BSSID */
	memcpy(&af_params->BSSID, ea, sizeof(af_params->BSSID));

	action_frame->len = htod16(DOT11_RMREQ_LEN + DOT11_RMREQ_STAT_LEN);
	rmreq = (dot11_rmreq_t *)&action_frame->data[0];
	/* Use Invalid Category Value to get back the action frame's response
	 * this category is not defined in proto/802.11.h
	 */
	rmreq->category = WBD_DOT11_ACTION_CAT_INVALID;
	rmreq->action = DOT11_RM_ACTION_RM_REQ;
	rmreq->token = 0x01; /* is there any other way than hardcoded */
	rmreq->reps = 0x0000;

	sreq = (dot11_rmreq_stat_t *)&rmreq->data[0];
	sreq->id = DOT11_MNG_MEASURE_REQUEST_ID;
	sreq->len = DOT11_RMREQ_STAT_LEN - TLV_HDR_LEN;
	sreq->token = 0x01;
	sreq->mode = 0;
	sreq->type = DOT11_MEASURE_TYPE_STAT;
	sreq->interval = htod16(0x0080);
	sreq->duration = htod16(0x0000);
	sreq->group_id = 0x00;
	memcpy(&sreq->peer, ea, sizeof(sreq->peer));

	namelen = strlen("actframe") + 1;	 /* length of iovar name plus null */
	iolen = namelen + WL_WIFI_AF_PARAMS_SIZE;

	memcpy(smbuf, "actframe", namelen);	/* copy iovar name including null */
	memcpy((int8*)smbuf + namelen, af_params, WL_WIFI_AF_PARAMS_SIZE);

	ret = wl_ioctl(ifname, WLC_SET_VAR, smbuf, iolen);
	WBD_IOVAR_ASSERT("actframe");
end:
	if (af_params)
		free(af_params);
	WBD_EXIT();
	return ret;
}

/* Get dfs pref chan list from driver */
int
wbd_wl_get_dfs_forced_chspec(char* ifname, wl_dfs_forced_t *smbuf, int size)
{
	wl_dfs_forced_t inp;
	int ret = 0, idx = 0;
	WBD_ENTER();

	memset(&inp, 0, sizeof(inp));

	wl_endian_probe(ifname);
	inp.version = htod16(DFS_PREFCHANLIST_VER);
	ret = wl_iovar_getbuf(ifname, "dfs_channel_forced", &inp, sizeof(inp),
		smbuf, size);
	WBD_IOVAR_ASSERT("get dfs_channel_forced");

	inp.chspec_list.num = dtoh32(inp.chspec_list.num);
	inp.version = dtoh16(inp.version);
	inp.chspec = dtoh16(inp.chspec);
	for (idx = 0; idx < inp.chspec_list.num; idx++) {
		inp.chspec_list.list[idx] = dtoh16(inp.chspec_list.list[idx]);
	}
end:
	WBD_EXIT();
	return ret;
}

/* Set dfs pref chan list */
int
wbd_wl_set_dfs_forced_chspec(char* ifname, wl_dfs_forced_t* dfs_frcd, int ioctl_size)
{
	int ret = 0, idx = 0;
	WBD_ENTER();

	wl_endian_probe(ifname);

	for (idx = 0; idx < dfs_frcd->chspec_list.num; idx++) {
		dfs_frcd->chspec_list.list[idx] = htod16(dfs_frcd->chspec_list.list[idx]);
	}
	dfs_frcd->chspec_list.num = htod32(dfs_frcd->chspec_list.num);
	dfs_frcd->chspec = htod16(dfs_frcd->chspec);
	dfs_frcd->version = htod16(dfs_frcd->version);
	ret = wl_iovar_set(ifname, "dfs_channel_forced", dfs_frcd, ioctl_size);
	WBD_IOVAR_ASSERT("set dfs_channel_forced");
end:
	WBD_EXIT();
	return ret;
}

/* Sends beacon request to assoc STA */
int
wbd_wl_send_beacon_request(char *ifname, wlc_ssid_t *ssid,
	struct ether_addr *bssid, struct ether_addr *sta_mac,
	uint8 channel, int *delay)
{
	int ret = WBDE_OK;
	char *dur_str;
	uint8 dur_data[5];
	bcnreq_t *bcnreq;
	blanket_bcnreq_t bcnreq_extn;
	WBD_ENTER();

	if (channel == 0) {
		goto end;
	}

	memset(&bcnreq_extn, 0, sizeof(bcnreq_extn));
	bcnreq = &bcnreq_extn.bcnreq;
	/* Initialize Beacon Request Frame data */
	bcnreq->bcn_mode = DOT11_RMREQ_BCN_ACTIVE;
	bcnreq->dur = 0x0001;
	dur_str = "0001";
	bcnreq->channel = channel;
	eacopy(sta_mac, &bcnreq->da);
	bcnreq->random_int = 0x0000;
	memcpy(bcnreq->ssid.SSID, ssid->SSID, ssid->SSID_len);
	bcnreq->ssid.SSID_len = ssid->SSID_len;
	bcnreq->reps = 0x0000;
	eacopy(bssid, &bcnreq_extn.src_bssid);
	eacopy(&ether_null, &bcnreq_extn.target_bssid);
	bcnreq_extn.opclass = 0;

	/* Sends beacon request to assocociated STA */
	blanket_send_beacon_request(ifname, &bcnreq_extn, 0, 0, NULL);
	WBD_IOVAR_ASSERT("actframe");

	/* Convert measurement duration from string to binary */
	get_hex_data((uchar *)dur_str, &dur_data[0], strlen(dur_str) / 2);

	/* delay between beacon request = min delay + measurement duration */
	*delay = WBD_MIN_BCN_REQ_DELAY + ntoh16(dur_data[1] | dur_data[0] << 8);

end:
	WBD_EXIT();
	return ret;
}

int wbd_wl_get_link_availability(char *ifname, unsigned short *link_available)
{
	int ret = WBDE_OK, buflen = WBD_MAX_BUF_512;
	int available = 0;
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
	/* Available link is sum of inbss, txop, goodtx and badtx */
	available = list->stats[0].ccastats[1] + list->stats[0].ccastats[6] +
		list->stats[0].ccastats[7] + list->stats[0].ccastats[8];
	*link_available = (unsigned short)available;
	WBD_INFO("Ifname[%s] link_available[%d]\n", ifname, *link_available);
end:
	if (data_buf) {
		free(data_buf);
	}
	WBD_EXIT();
	return ret;
}
