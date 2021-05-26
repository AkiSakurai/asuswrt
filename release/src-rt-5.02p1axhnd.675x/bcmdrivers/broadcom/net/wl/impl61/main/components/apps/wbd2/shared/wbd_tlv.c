/*
 * WBD Vendor Message TLV definitions
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
 * $Id: wbd_tlv.c 781083 2019-11-11 07:05:52Z $
 */

#include <ctype.h>
#include <fcntl.h>

#include "ieee1905_tlv.h"

#include "wbd.h"
#include "wbd_shared.h"
#include "wbd_ds.h"
#include "wbd_tlv.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

typedef struct wbd_tlv_types_name {
	wbd_tlv_types_t tlv_type;
	char tlv_name[40];
} wbd_tlv_types_name_t;

static wbd_tlv_types_name_t wbd_tlv_list_type_name[] = {
	{WBD_TLV_FBT_CONFIG_REQ_TYPE, "Brcm FBT Config Request TLV"},
	{WBD_TLV_FBT_CONFIG_RESP_TYPE, "Brcm FBT Config Response TLV"},
	{WBD_TLV_VNDR_ASSOC_STA_METRICS_TYPE, "Brcm Assoc STA Metrics Vndr TLV"},
	{WBD_TLV_VNDR_METRIC_POLICY_TYPE, "Brcm Metric Reporting Policy Vndr TLV"},
	{WBD_TLV_WEAK_CLIENT_RESP_TYPE, "Brcm Weak Client Response TLV"},
	{WBD_TLV_REMOVE_CLIENT_REQ_TYPE, "Brcm Remove Client Request TLV"},
	{WBD_TLV_BSS_CAPABILITY_QUERY_TYPE, "Brcm BSS Capability Query TLV"},
	{WBD_TLV_BSS_CAPABILITY_REPORT_TYPE, "Brcm BSS Capability Report TLV"},
	{WBD_TLV_BSS_METRICS_QUERY_TYPE, "Brcm BSS Metrics Query TLV"},
	{WBD_TLV_BSS_METRICS_REPORT_TYPE, "Brcm BSS Metrics Report TLV"},
	{WBD_TLV_STEER_RESP_REPORT_TYPE, "Brcm Steer Response Report TLV"},
	{WBD_TLV_STEER_REQUEST_TYPE, "Brcm Steer Request TLV"},
	{WBD_TLV_ZWDFS_TYPE, "Brcm Zero wait DFS TLV"},
	{WBD_TLV_BH_STA_METRIC_POLICY_TYPE, "Brcm Backhaul STA Metric Policy TLV"},
	{WBD_TLV_NVRAM_SET_TYPE, "Brcm NVRAM Set TLV"},
	{WBD_TLV_CHAN_SET_TYPE, "Brcm CHAN SET TLV"},
	{WBD_TLV_CHAN_INFO_TYPE, "Brcm CHAN INFO TLV"},
	{WBD_TLV_DFS_CHAN_INFO_TYPE, "Brcm DFS CHAN INFO TLV"},
};

/* Get TLV Type String from TLV Type Enum Value */
char const*
wbd_tlv_get_type_str(int tlv_type)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(wbd_tlv_list_type_name); i++) {

		if (tlv_type == wbd_tlv_list_type_name[i].tlv_type) {

			return (wbd_tlv_list_type_name[i].tlv_name);
		}
	}

	return "Unknown Tlv";
}

typedef struct wbd_msg_types_with_vndr_tlv_name {
	i5_msg_types_with_vndr_tlv_t msg_type;
	char msg_name[60];
} wbd_msg_types_with_vndr_tlv_name_t;

static wbd_msg_types_with_vndr_tlv_name_t wbd_list_msg_vndr_name[] = {
	{i5MsgMultiAPPolicyConfigRequestValue, "1905 MultiAP Policy Config Request"},
	{i5MsgMultiAPGuestSsidValue, "1905 MultiAP Guest SSID"},
};

/* Get Message String from 1905 Message Type which can support Vendor TLV */
char const*
wbd_get_1905_msg_str(int msg_type)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(wbd_list_msg_vndr_name); i++) {

		if (msg_type == wbd_list_msg_vndr_name[i].msg_type) {

			return (wbd_list_msg_vndr_name[i].msg_name);
		}
	}

	return "Unknown 1905 Message";
}

#ifdef WLHOSTFBT
/* Encode Vendor Specific TLV for Message : FBT_CONFIG_REQ to send */
void
wbd_tlv_encode_fbt_config_request(void* data, unsigned char* tlv_data, unsigned int* tlv_data_len)
{
	int ret = WBDE_OK;
	wbd_cmd_fbt_config_t *cmd = (wbd_cmd_fbt_config_t *)data;
	unsigned char *pbuf, *pmem;
	i5_tlv_t *ptlv;
	dll_t *fbt_item_p = NULL;
	wbd_fbt_bss_entry_t *fbt_data = NULL;
	WBD_ENTER();

	/* Validate args */
	WBD_ASSERT_ARG(tlv_data, WBDE_INV_ARG);
	WBD_ASSERT_ARG(tlv_data_len, WBDE_INV_ARG);

	/* Pointing to out data */
	pmem = tlv_data;

	/* bytes skiped for TLV Hdr size, fill the TLV Hdr in the end */
	pbuf = pmem + sizeof(i5_tlv_t);

	/* 1 byte for the num of FBT enabled BSSs included in this TLV */
	*pbuf = (unsigned char)cmd->entry_list.count;
	pbuf++;

	/* Travese FBT Config Request List items */
	foreach_glist_item(fbt_item_p, cmd->entry_list) {

		fbt_data = (wbd_fbt_bss_entry_t *)fbt_item_p;

		/* 6 bytes for BSSID of the BSS */
		memcpy(pbuf, fbt_data->bssid, MAC_ADDR_LEN);
		pbuf += MAC_ADDR_LEN;

		/* 6 bytes for MAC Address of Bridge Address */
		memcpy(pbuf, fbt_data->bss_br_mac, MAC_ADDR_LEN);
		pbuf += MAC_ADDR_LEN;

		/* 1 byte for the size of R0KH_ID length = n */
		*pbuf = (unsigned char)fbt_data->len_r0kh_id;
		pbuf++;

		/* n bytes for the R0KH_ID */
		memcpy(pbuf, fbt_data->r0kh_id, fbt_data->len_r0kh_id);
		pbuf += fbt_data->len_r0kh_id;

		/* 1 byte for the size of R0KH_KEY length = k */
		*pbuf = (unsigned char)fbt_data->len_r0kh_key;
		pbuf++;

		/* k bytes for the R0KH_KEY */
		memcpy(pbuf, fbt_data->r0kh_key, fbt_data->len_r0kh_key);
		pbuf += fbt_data->len_r0kh_key;
	}

	/* Fill the TLV Hdr now */
	ptlv = (i5_tlv_t *)pmem;
	ptlv->type = WBD_TLV_FBT_CONFIG_REQ_TYPE;
	ptlv->length = htons(pbuf-pmem-sizeof(i5_tlv_t));

	/* Update out data length */
	*tlv_data_len = pbuf-pmem;

end:
	WBD_EXIT();
	return;
}

/* Decode Vendor Specific TLV for Message : FBT_CONFIG_REQ on receive */
int
wbd_tlv_decode_fbt_config_request(void* data, unsigned char* tlv_data, unsigned int tlv_data_len)
{
	int ret = WBDE_TLV_ERROR, iter = 0;
	wbd_cmd_fbt_config_t *cmd = (wbd_cmd_fbt_config_t *)data;
	unsigned char *pValue;
	unsigned short pos = 0;
	WBD_ENTER();

	/* Validate args */
	WBD_ASSERT_ARG(tlv_data, WBDE_INV_ARG);

	/* Initialize data pointer */
	pValue = tlv_data;

	/* bytes skiped for TLV Hdr size, fill the TLV Hdr in the end */
	pValue += sizeof(i5_tlv_t);

	if (tlv_data_len < WBD_MIN_LEN_TLV_FBT_CONFIG_REQUEST) {
		WBD_WARNING("tlv_data_len[%d] < Vendor FBT Config Request Min Len[%d]\n",
			tlv_data_len, WBD_MIN_LEN_TLV_FBT_CONFIG_REQUEST);
		goto end;
	}

	/* 1 byte for the num of FBT enabled BSSs included in this TLV */
	cmd->num_entries = (unsigned char)pValue[pos];
	pos++;

	if (cmd->num_entries <= 0) {
		ret = WBDE_OK;
		goto end;
	}

	for (iter = 0; iter < cmd->num_entries; iter++) {

		wbd_fbt_bss_entry_t new_fbt_bss;
		memset(&new_fbt_bss, 0, sizeof(new_fbt_bss));
		memset(&(new_fbt_bss.fbt_info), 0, sizeof(new_fbt_bss.fbt_info));

		if (tlv_data_len < (pos + 6 + 6 + 1)) {
			WBD_WARNING("tlv_data_len[%d] < Vendor FBT Config Request Len[%d]\n",
				tlv_data_len, (pos + 6 + 6 + 1));
			goto end;
		}

		/* 6 bytes for BSSID of the BSS */
		memcpy(new_fbt_bss.bssid, &pValue[pos], MAC_ADDR_LEN);
		pos += MAC_ADDR_LEN;

		/* 6 bytes for MAC Address of Bridge Address */
		memcpy(new_fbt_bss.bss_br_mac, &pValue[pos], MAC_ADDR_LEN);
		pos += MAC_ADDR_LEN;

		/* 1 byte for the size of R0KH_ID length = n */
		new_fbt_bss.len_r0kh_id = (unsigned char)pValue[pos];
		pos++;

		if (tlv_data_len < (pos + new_fbt_bss.len_r0kh_id + 1)) {
			WBD_WARNING("tlv_data_len[%d] < Vendor FBT Config Request Len[%d]\n",
				tlv_data_len, (pos + new_fbt_bss.len_r0kh_id + 1));
			goto end;
		}
		/* n bytes for the R0KH_ID */
		memcpy(new_fbt_bss.r0kh_id, &pValue[pos], new_fbt_bss.len_r0kh_id);
		new_fbt_bss.r0kh_id[new_fbt_bss.len_r0kh_id] = '\0';
		pos += new_fbt_bss.len_r0kh_id;

		/* 1 byte for the size of R0KH_KEY length = k */
		new_fbt_bss.len_r0kh_key = (unsigned char)pValue[pos];
		pos++;

		if (tlv_data_len < (pos + new_fbt_bss.len_r0kh_key)) {
			WBD_WARNING("tlv_data_len[%d] < Vendor FBT Config Request Len[%d]\n",
				tlv_data_len, (pos + new_fbt_bss.len_r0kh_key));
			goto end;
		}
		/* k bytes for the R0KH_KEY */
		memcpy(new_fbt_bss.r0kh_key, &pValue[pos], new_fbt_bss.len_r0kh_key);
		new_fbt_bss.r0kh_key[new_fbt_bss.len_r0kh_key] = '\0';
		pos += new_fbt_bss.len_r0kh_key;

		/* Add a FBT BSS item in a Slave's FBT Request List */
		wbd_add_item_in_fbt_cmdlist(&new_fbt_bss, cmd, NULL);
	}
	ret = WBDE_OK;

end:
	WBD_EXIT();
	return ret;
}

/* Encode Vendor Specific TLV for Message : FBT_CONFIG_RESP to send */
void
wbd_tlv_encode_fbt_config_response(void* data, unsigned char* tlv_data, unsigned int* tlv_data_len)
{
	int ret = WBDE_OK;
	wbd_cmd_fbt_config_t *cmd = (wbd_cmd_fbt_config_t *)data;
	unsigned char *pbuf, *pmem;
	i5_tlv_t *ptlv;
	dll_t *fbt_item_p = NULL;
	wbd_fbt_bss_entry_t *fbt_data = NULL;
	WBD_ENTER();

	/* Validate args */
	WBD_ASSERT_ARG(tlv_data, WBDE_INV_ARG);
	WBD_ASSERT_ARG(tlv_data_len, WBDE_INV_ARG);

	/* Pointing to out data */
	pmem = tlv_data;

	/* bytes skiped for TLV Hdr size, fill the TLV Hdr in the end */
	pbuf = pmem + sizeof(i5_tlv_t);

	/* 1 byte for the num of FBT enabled BSSs included in this TLV */
	*pbuf = (unsigned char)cmd->entry_list.count;
	pbuf++;

	/* Travese FBT Config Response List items */
	foreach_glist_item(fbt_item_p, cmd->entry_list) {

		fbt_data = (wbd_fbt_bss_entry_t *)fbt_item_p;

		/* 2 bytes for MDID */
		pbuf += i5_cpy_host16_to_netbuf(pbuf, fbt_data->fbt_info.mdid);

		/* 1 byte for FT Capability and Policy field */
		*pbuf = (unsigned char)fbt_data->fbt_info.ft_cap_policy;
		pbuf++;

		/* 4 bytes for Reassoc Deadline Interval in Time units (TUs) for FBT enabled BSS */
		pbuf += i5_cpy_host32_to_netbuf(pbuf, fbt_data->fbt_info.tie_reassoc_interval);

		/* 6 bytes for BSSID of the BSS */
		memcpy(pbuf, fbt_data->bssid, MAC_ADDR_LEN);
		pbuf += MAC_ADDR_LEN;

		/* 6 bytes for Bridge MAC of the BSS */
		memcpy(pbuf, fbt_data->bss_br_mac, MAC_ADDR_LEN);
		pbuf += MAC_ADDR_LEN;

		/* 1 byte for the size of R0KH_ID length = n */
		*pbuf = (unsigned char)fbt_data->len_r0kh_id;
		pbuf++;

		/* n bytes for the R0KH_ID */
		memcpy(pbuf, fbt_data->r0kh_id, fbt_data->len_r0kh_id);
		pbuf += fbt_data->len_r0kh_id;

		/* 1 byte for the size of R0KH_KEY length = k */
		*pbuf = (unsigned char)fbt_data->len_r0kh_key;
		pbuf++;

		/* k bytes for the R0KH_KEY */
		memcpy(pbuf, fbt_data->r0kh_key, fbt_data->len_r0kh_key);
		pbuf += fbt_data->len_r0kh_key;
	}

	/* Fill the TLV Hdr now */
	ptlv = (i5_tlv_t *)pmem;
	ptlv->type = WBD_TLV_FBT_CONFIG_RESP_TYPE;
	ptlv->length = htons(pbuf-pmem-sizeof(i5_tlv_t));

	/* Update out data length */
	*tlv_data_len = pbuf-pmem;

end:
	WBD_EXIT();
	return;
}

/* Decode Vendor Specific TLV for Message : FBT_CONFIG_RESP on receive */
int
wbd_tlv_decode_fbt_config_response(void* data, unsigned char* tlv_data, unsigned int tlv_data_len)
{
	int ret = WBDE_TLV_ERROR, iter = 0;
	wbd_cmd_fbt_config_t *cmd = (wbd_cmd_fbt_config_t *)data;
	unsigned char *pValue;
	unsigned short pos = 0;
	WBD_ENTER();

	/* Validate args */
	WBD_ASSERT_ARG(tlv_data, WBDE_INV_ARG);

	/* Initialize data pointer */
	pValue = tlv_data;

	/* bytes skiped for TLV Hdr size, fill the TLV Hdr in the end */
	pValue += sizeof(i5_tlv_t);

	if (tlv_data_len < WBD_MIN_LEN_TLV_FBT_CONFIG_RESP) {
		WBD_WARNING("tlv_data_len[%d] < Vendor FBT Config Response Min Len[%d]\n",
			tlv_data_len, WBD_MIN_LEN_TLV_FBT_CONFIG_RESP);
		goto end;
	}

	/* 1 byte for the num of FBT enabled BSSs included in this TLV */
	cmd->num_entries = (unsigned char)pValue[pos];
	pos++;

	if (cmd->num_entries <= 0) {
		ret = WBDE_OK;
		goto end;
	}

	for (iter = 0; iter < cmd->num_entries; iter++) {

		wbd_fbt_bss_entry_t new_fbt_bss;
		memset(&new_fbt_bss, 0, sizeof(new_fbt_bss));
		memset(&(new_fbt_bss.fbt_info), 0, sizeof(new_fbt_bss.fbt_info));

		if (tlv_data_len < (pos + 2 + 1 + 4 + 6 + 6 + 1)) {
			WBD_WARNING("tlv_data_len[%d] < Vendor FBT Config Response Len[%d]\n",
				tlv_data_len, (pos + 2 + 1 + 4 + 6 + 6 + 1));
			goto end;
		}

		/* 2 bytes for MDID */
		pos += i5_cpy_netbuf_to_host16(&(new_fbt_bss.fbt_info.mdid), &pValue[pos]);

		/* 1 byte for the num of FBT enabled BSSs included in this TLV */
		new_fbt_bss.fbt_info.ft_cap_policy = (unsigned char)pValue[pos];
		pos++;

		/* 4 bytes for Reassoc Deadline Interval in Time units (TUs) for FBT enabled BSS */
		pos += i5_cpy_netbuf_to_host32(&(new_fbt_bss.fbt_info.tie_reassoc_interval),
			&pValue[pos]);

		/* 6 bytes for BSSID of the BSS */
		memcpy(new_fbt_bss.bssid, &pValue[pos], MAC_ADDR_LEN);
		pos += MAC_ADDR_LEN;

		/* 6 bytes for Bridge MAC of the BSS */
		memcpy(new_fbt_bss.bss_br_mac, &pValue[pos], MAC_ADDR_LEN);
		pos += MAC_ADDR_LEN;

		/* 1 byte for the size of R0KH_ID length = n */
		new_fbt_bss.len_r0kh_id = (unsigned char)pValue[pos];
		pos++;

		if (tlv_data_len < (pos + new_fbt_bss.len_r0kh_id + 1)) {
			WBD_WARNING("tlv_data_len[%d] < Vendor FBT Config Response Len[%d]\n",
				tlv_data_len, (pos + new_fbt_bss.len_r0kh_id + 1));
			goto end;
		}
		/* n bytes for the R0KH_ID */
		memcpy(new_fbt_bss.r0kh_id, &pValue[pos], new_fbt_bss.len_r0kh_id);
		new_fbt_bss.r0kh_id[new_fbt_bss.len_r0kh_id] = '\0';
		pos += new_fbt_bss.len_r0kh_id;

		/* 1 byte for the size of R0KH_KEY length = k */
		new_fbt_bss.len_r0kh_key = (unsigned char)pValue[pos];
		pos++;

		if (tlv_data_len < (pos + new_fbt_bss.len_r0kh_key)) {
			WBD_WARNING("tlv_data_len[%d] < Vendor FBT Config Response Len[%d]\n",
				tlv_data_len, (pos + new_fbt_bss.len_r0kh_key));
			goto end;
		}
		/* k bytes for the R0KH_KEY */
		memcpy(new_fbt_bss.r0kh_key, &pValue[pos], new_fbt_bss.len_r0kh_key);
		new_fbt_bss.r0kh_key[new_fbt_bss.len_r0kh_key] = '\0';
		pos += new_fbt_bss.len_r0kh_key;

		/* Add a FBT BSS item in a Slave's FBT Response List */
		wbd_add_item_in_fbt_cmdlist(&new_fbt_bss, cmd, NULL);
	}
	ret = WBDE_OK;

end:
	WBD_EXIT();
	return ret;
}
#endif /* WLHOSTFBT */

/* Encode Vendor Specific TLV : Associated STA Link Metrics Vendor Data to send */
void
wbd_tlv_encode_vndr_assoc_sta_metrics(void* data,
	unsigned char* tlv_data, unsigned int* tlv_data_len)
{
	int ret = WBDE_OK, l_num_bss = 0;
	wbd_cmd_vndr_assoc_sta_metric_t *cmd = (wbd_cmd_vndr_assoc_sta_metric_t *)data;
	unsigned char *pbuf, *pmem;
	i5_tlv_t *ptlv;
	WBD_ENTER();

	/* Validate args */
	WBD_ASSERT_ARG(tlv_data, WBDE_INV_ARG);
	WBD_ASSERT_ARG(tlv_data_len, WBDE_INV_ARG);

	/* Pointing to out data */
	pmem = tlv_data;

	/* bytes skiped for TLV Hdr size, fill the TLV Hdr in the end */
	pbuf = pmem + sizeof(i5_tlv_t);

	/* 6 bytes for MAC address of the associated STA */
	memcpy(pbuf, cmd->sta_mac, MAC_ADDR_LEN);
	pbuf += MAC_ADDR_LEN;

	/* 1 byte for the num of BSSIDs reported for this STA */
	l_num_bss = (cmd->num_bss == 1) ? 1 : 0;
	*pbuf = (unsigned char)l_num_bss;
	pbuf++;

	/* Considering only ONE BSSID is possible to be associated with sta */
	if (l_num_bss != 1) {
		goto encode_hdr;
	}

	/* 6 bytes for BSSID of the BSS for which the STA is associated */
	memcpy(pbuf, cmd->src_bssid, MAC_ADDR_LEN);
	pbuf += MAC_ADDR_LEN;

	/* 4 bytes for data rate to measure STA is idle */
	pbuf += i5_cpy_host32_to_netbuf(pbuf, cmd->vndr_metrics.idle_rate);

	/* 4 bytes for Tx failures */
	pbuf += i5_cpy_host32_to_netbuf(pbuf, cmd->vndr_metrics.tx_failures);

	/* 4 bytes for Tx Total failures */
	pbuf += i5_cpy_host32_to_netbuf(pbuf, cmd->vndr_metrics.tx_tot_failures);

	/* 1 byte for the STA capability */
	*pbuf = (unsigned char)cmd->vndr_metrics.sta_cap;
	pbuf++;

encode_hdr :

	/* Fill the TLV Hdr now */
	ptlv = (i5_tlv_t *)pmem;
	ptlv->type = WBD_TLV_VNDR_ASSOC_STA_METRICS_TYPE;
	ptlv->length = htons(pbuf-pmem-sizeof(i5_tlv_t));

	/* Update out data length */
	*tlv_data_len = pbuf-pmem;

end:
	WBD_EXIT();
	return;
}

/* Decode Vendor Specific TLV : Associated STA Link Metrics Vendor Data on receive */
int
wbd_tlv_decode_vndr_assoc_sta_metrics(void* data,
	unsigned char* tlv_data, unsigned int tlv_data_len)
{
	int ret = WBDE_OK;
	wbd_cmd_vndr_assoc_sta_metric_t *cmd = (wbd_cmd_vndr_assoc_sta_metric_t *)data;
	unsigned char *pValue;
	unsigned short pos = 0;
	WBD_ENTER();

	/* Validate args */
	WBD_ASSERT_ARG(tlv_data, WBDE_INV_ARG);

	/* Initialize data pointer */
	pValue = tlv_data;

	/* bytes skiped for TLV Hdr size, fill the TLV Hdr in the end */
	pValue += sizeof(i5_tlv_t);

	if (tlv_data_len < WBD_MAX_LEN_TLV_ASSOC_STA_METRICS) {
		WBD_WARNING("tlv_data_len[%d] < Vendor Assoc STA Metrics Len[%d]\n",
			tlv_data_len, WBD_MAX_LEN_TLV_ASSOC_STA_METRICS);
		ret = WBDE_TLV_ERROR;
		goto end;
	}

	/* 6 bytes for MAC address of the associated STA */
	memcpy(cmd->sta_mac, &pValue[pos], MAC_ADDR_LEN);
	pos += MAC_ADDR_LEN;

	/* 1 byte for the num of BSSIDs reported for this STA */
	cmd->num_bss = (unsigned char)pValue[pos];
	pos++;

	/* Considering only ONE BSSID is possible to be associated with sta */
	if (cmd->num_bss != 1) {
		goto end;
	}

	/* 6 bytes for BSSID of the BSS for which the STA is associated */
	memcpy(cmd->src_bssid, &pValue[pos], MAC_ADDR_LEN);
	pos += MAC_ADDR_LEN;

	/* 4 bytes for data rate to measure STA is idle */
	pos += i5_cpy_netbuf_to_host32(&(cmd->vndr_metrics.idle_rate), &pValue[pos]);

	/* 4 bytes for Tx failures */
	pos += i5_cpy_netbuf_to_host32(&(cmd->vndr_metrics.tx_failures), &pValue[pos]);

	/* 4 bytes for Tx total failures */
	pos += i5_cpy_netbuf_to_host32(&(cmd->vndr_metrics.tx_tot_failures), &pValue[pos]);

	/* 1 byte for the STA capability */
	cmd->vndr_metrics.sta_cap = (unsigned char)pValue[pos];
	pos++;

end:
	WBD_EXIT();
	return ret;
}

/* Encode Vendor Specific TLV : Metric Reporting Policy Vendor Data to send */
void wbd_tlv_encode_vndr_metric_policy(void* data,
	unsigned char* tlv_data, unsigned int* tlv_data_len)
{
	int ret = WBDE_OK;
	wbd_cmd_vndr_metric_policy_config_t *cmd = (wbd_cmd_vndr_metric_policy_config_t *)data;
	unsigned char *pbuf, *pmem;
	i5_tlv_t *ptlv;
	dll_t *policy_item_p = NULL;
	wbd_metric_policy_ifr_entry_t *policy_data = NULL;
	WBD_ENTER();

	/* Validate args */
	WBD_ASSERT_ARG(tlv_data, WBDE_INV_ARG);
	WBD_ASSERT_ARG(tlv_data_len, WBDE_INV_ARG);

	/* Pointing to out data */
	pmem = tlv_data;

	/* bytes skiped for TLV Hdr size, fill the TLV Hdr in the end */
	pbuf = pmem + sizeof(i5_tlv_t);

	/* 1 byte for the num of radios included in this TLV */
	*pbuf = (unsigned char)cmd->entry_list.count;
	pbuf++;

	/* Travese Metric Reporting Policy Vendor Config List items */
	foreach_glist_item(policy_item_p, cmd->entry_list) {

		policy_data = (wbd_metric_policy_ifr_entry_t *)policy_item_p;

		/* 6 bytes for Radio Interface MAC address */
		memcpy(pbuf, policy_data->ifr_mac, MAC_ADDR_LEN);
		pbuf += MAC_ADDR_LEN;

		/* 4 bytes for data rate to measure STA is idle */
		pbuf += i5_cpy_host32_to_netbuf(pbuf, policy_data->vndr_policy.t_idle_rate);

		/* 4 bytes for Tx Rate in Mbps */
		pbuf += i5_cpy_host32_to_netbuf(pbuf, policy_data->vndr_policy.t_tx_rate);

		/* 4 bytes for Tx Failures */
		pbuf += i5_cpy_host32_to_netbuf(pbuf, policy_data->vndr_policy.t_tx_failures);

		/* 4 bytes for Flags */
		pbuf += i5_cpy_host32_to_netbuf(pbuf, policy_data->vndr_policy.flags);
	}

	/* Fill the TLV Hdr now */
	ptlv = (i5_tlv_t *)pmem;
	ptlv->type = WBD_TLV_VNDR_METRIC_POLICY_TYPE;
	ptlv->length = htons(pbuf-pmem-sizeof(i5_tlv_t));

	/* Update out data length */
	*tlv_data_len = pbuf-pmem;

end:
	WBD_EXIT();
	return;
}

/* Decode Vendor Specific TLV : Metric Reporting Policy Vendor Data on receive */
int wbd_tlv_decode_vndr_metric_policy(void* data,
	unsigned char* tlv_data, unsigned int tlv_data_len)
{
	int ret = WBDE_TLV_ERROR, iter = 0;
	wbd_cmd_vndr_metric_policy_config_t *cmd = (wbd_cmd_vndr_metric_policy_config_t *)data;
	unsigned char *pValue;
	unsigned short pos = 0;
	WBD_ENTER();

	/* Validate args */
	WBD_ASSERT_ARG(tlv_data, WBDE_INV_ARG);

	/* Initialize data pointer */
	pValue = tlv_data;

	/* bytes skiped for TLV Hdr size, fill the TLV Hdr in the end */
	pValue += sizeof(i5_tlv_t);

	if (tlv_data_len < WBD_MIN_LEN_TLV_METRIC_POLICY) {
		WBD_WARNING("tlv_data_len[%d] < Vendor Metric Policy Min Len[%d]\n",
			tlv_data_len, WBD_MIN_LEN_TLV_METRIC_POLICY);
		goto end;
	}

	/* 1 byte for the num of radios included in this TLV */
	cmd->num_entries = (unsigned char)pValue[pos];
	pos++;

	if (cmd->num_entries <= 0) {
		ret = WBDE_OK;
		goto end;
	}

	if (tlv_data_len < (pos + (cmd->num_entries * (WBD_MAX_LEN_TLV_METRIC_POLICY_ITEM)))) {
		WBD_INFO("tlv_data_len[%d] < Vendor Metric Policy Len[%d]\n",
			tlv_data_len,
			(pos + (cmd->num_entries * (WBD_MAX_LEN_TLV_METRIC_POLICY_ITEM))));
		goto end;
	}

	for (iter = 0; iter < cmd->num_entries; iter++) {

		wbd_metric_policy_ifr_entry_t new_policy_ifr;
		memset(&new_policy_ifr, 0, sizeof(new_policy_ifr));
		memset(&(new_policy_ifr.vndr_policy), 0, sizeof(new_policy_ifr.vndr_policy));

		/* 6 bytes for Radio Interface MAC address */
		memcpy(new_policy_ifr.ifr_mac, &pValue[pos], MAC_ADDR_LEN);
		pos += MAC_ADDR_LEN;

		/* 4 bytes for data rate to measure STA is idle */
		pos += i5_cpy_netbuf_to_host32(&(new_policy_ifr.vndr_policy.t_idle_rate),
			&pValue[pos]);

		/* 4 bytes for Tx Rate in Mbps */
		pos += i5_cpy_netbuf_to_host32(&(new_policy_ifr.vndr_policy.t_tx_rate),
			&pValue[pos]);

		/* 4 bytes for Tx Failures */
		pos += i5_cpy_netbuf_to_host32(&(new_policy_ifr.vndr_policy.t_tx_failures),
			&pValue[pos]);

		/* 4 bytes for Flags */
		pos += i5_cpy_netbuf_to_host32(&(new_policy_ifr.vndr_policy.flags),
			&pValue[pos]);

		/* Add a Metric Policy item in a Metric Policy List */
		wbd_add_item_in_metric_policylist(&new_policy_ifr, cmd, NULL);
	}
	ret = WBDE_OK;

end:
	WBD_EXIT();
	return ret;
}

/* Encode Vendor Specific TLV for Message : weak client response to send */
int
wbd_tlv_encode_weak_client_response(void* data,
	unsigned char* tlv_data, unsigned int* tlv_data_len)
{
	int ret = WBDE_OK;
	wbd_cmd_weak_client_resp_t *cmdweakclientresp = (wbd_cmd_weak_client_resp_t*)data;
	unsigned char *pbuf, *pmem;
	i5_tlv_t *ptlv;
	WBD_ENTER();

	/* Validate args */
	WBD_ASSERT_ARG(tlv_data, WBDE_INV_ARG);
	WBD_ASSERT_ARG(tlv_data_len, WBDE_INV_ARG);

	/* Pointing to out data */
	pmem = tlv_data;

	/* bytes skiped for TLV Hdr size, fill the TLV Hdr in the end */
	pbuf = pmem + sizeof(i5_tlv_t);

	/* 6 bytes for MAC address of the STA */
	memcpy(pbuf, &cmdweakclientresp->sta_mac, MAC_ADDR_LEN);
	pbuf += MAC_ADDR_LEN;

	/* 6 bytes for BSSID of the BSS for which the STA is associated */
	memcpy(pbuf, &cmdweakclientresp->BSSID, MAC_ADDR_LEN);
	pbuf += MAC_ADDR_LEN;

	/* 1 bytes for reason code */
	*pbuf = (unsigned char)cmdweakclientresp->error_code;
	pbuf++;

	/* 4 bytes for dwell time */
	*((unsigned int *)pbuf) = htonl(cmdweakclientresp->dwell_time);
	pbuf += 4;

	/* Fill the TLV Hdr now */
	ptlv = (i5_tlv_t *)pmem;
	ptlv->type = WBD_TLV_WEAK_CLIENT_RESP_TYPE;
	ptlv->length = htons(pbuf-pmem-sizeof(i5_tlv_t));

	/* Update out data length */
	*tlv_data_len = pbuf-pmem;

end:
	WBD_EXIT();
	return ret;
}

/* Decode Vendor Specific TLV for Message : weak client response on recieve */
int
wbd_tlv_decode_weak_client_response(void* data,
	unsigned char* tlv_data, unsigned int tlv_data_len)
{
	int ret = WBDE_OK;
	wbd_cmd_weak_client_resp_t *cmdweakclientresp = (wbd_cmd_weak_client_resp_t*)data;
	unsigned char *pValue;
	unsigned char pos = 0;
	WBD_ENTER();

	/* Validate args */
	WBD_ASSERT_ARG(tlv_data, WBDE_INV_ARG);

	/* Initialize data pointer */
	pValue = tlv_data;

	/* bytes skiped for TLV Hdr size, fill the TLV Hdr in the end */
	pValue += sizeof(i5_tlv_t);

	if (tlv_data_len < WBD_MAX_LEN_TLV_WEAK_CLIENT_RESP) {
		WBD_WARNING("tlv_data_len[%d] < Weak Client Resp Len[%d]\n",
			tlv_data_len, WBD_MAX_LEN_TLV_WEAK_CLIENT_RESP);
		ret = WBDE_TLV_ERROR;
		goto end;
	}

	/* 6 bytes for STA MAC of the STA */
	memcpy(&cmdweakclientresp->sta_mac, &pValue[pos], MAC_ADDR_LEN);
	pos += MAC_ADDR_LEN;

	/* 6 bytes for BSSID of the BSS for which the STA is associated */
	memcpy(&cmdweakclientresp->BSSID, &pValue[pos], MAC_ADDR_LEN);
	pos += MAC_ADDR_LEN;

	/* 1 byte for the reason code */
	cmdweakclientresp->error_code = (unsigned char)pValue[pos];
	pos++;

	/* 4 byte for the dwell time */
	cmdweakclientresp->dwell_time = ntohl(*((unsigned int *)&pValue[pos]));
	pos += 4;

end:
	WBD_EXIT();
	return ret;
}

/* Encode Vendor Specific TLV for Message : BSS capability query */
int
wbd_tlv_encode_bss_capability_query(void* data, unsigned char* tlv_data, unsigned int* tlv_data_len)
{
	int ret = WBDE_OK;
	unsigned char *radio_mac = (unsigned char*)data;
	unsigned char *pbuf, *pmem;
	i5_tlv_t *ptlv;
	WBD_ENTER();

	/* Validate args */
	WBD_ASSERT_ARG(tlv_data, WBDE_INV_ARG);
	WBD_ASSERT_ARG(tlv_data_len, WBDE_INV_ARG);

	/* Pointing to out data */
	pmem = tlv_data;

	/* bytes skiped for TLV Hdr size, fill the TLV Hdr in the end */
	pbuf = pmem + sizeof(i5_tlv_t);

	/* 6 bytes for Radio Unique Identifier */
	memcpy(pbuf, radio_mac, MAC_ADDR_LEN);
	pbuf += MAC_ADDR_LEN;

	/* 1 bytes for number of BSS. Which is 0, means ask the details of all the BSS */
	*pbuf = (unsigned char)0;
	pbuf++;

	/* Fill the TLV Hdr now */
	ptlv = (i5_tlv_t *)pmem;
	ptlv->type = WBD_TLV_BSS_CAPABILITY_QUERY_TYPE;
	ptlv->length = htons(pbuf-pmem-sizeof(i5_tlv_t));

	/* Update out data length */
	*tlv_data_len = pbuf-pmem;

end:
	WBD_EXIT();
	return ret;
}

/* Decode Vendor Specific TLV for Message : BSS capability query on recieve */
int
wbd_tlv_decode_bss_capability_query(void* data, unsigned char* tlv_data, unsigned int tlv_data_len)
{
	int ret = WBDE_OK;
	unsigned char *radio_mac = (unsigned char*)data;
	unsigned char *pValue;
	unsigned char pos = 0;
	WBD_ENTER();

	/* Validate args */
	WBD_ASSERT_ARG(tlv_data, WBDE_INV_ARG);

	/* Initialize data pointer */
	pValue = tlv_data;

	/* bytes skiped for TLV Hdr size, fill the TLV Hdr in the end */
	pValue += sizeof(i5_tlv_t);

	if (tlv_data_len < WBD_MAX_LEN_TLV_BSS_CAPABILITY_QUERY) {
		WBD_WARNING("tlv_data_len[%d] < BSS Capability Query Len[%d]\n",
			tlv_data_len, WBD_MAX_LEN_TLV_BSS_CAPABILITY_QUERY);
		ret = WBDE_TLV_ERROR;
		goto end;
	}

	/* 6 bytes for Radio Unique Identifier */
	memcpy(radio_mac, &pValue[pos], MAC_ADDR_LEN);
	pos += MAC_ADDR_LEN;

	/* No need to read 1 byte of BSS count, as it is always be 0 for now. In future,
	 * List of BSSID of a BSS operated by the MAP device can be provided here. So that the
	 * agent will send the response only for those BSSID's
	 */

end:
	WBD_EXIT();
	return ret;
}

/* Encode Vendor Specific TLV for Message : BSS capability Report */
int
wbd_tlv_encode_bss_capability_report(void* data, unsigned char* tlv_data,
	unsigned int* tlv_data_len)
{
	unsigned char bss_count = 0;
	int ret = WBDE_OK;
	i5_dm_interface_type *i5_ifr = (i5_dm_interface_type*)data;
	i5_dm_bss_type *i5_bss;
	wbd_bss_item_t *bss_vndr_data;
	unsigned char *pbuf, *pmem, *pbsscount_mem = NULL;
	i5_tlv_t *ptlv;
	WBD_ENTER();

	/* Validate args */
	WBD_ASSERT_ARG(tlv_data, WBDE_INV_ARG);
	WBD_ASSERT_ARG(tlv_data_len, WBDE_INV_ARG);

	/* Pointing to out data */
	pmem = tlv_data;

	/* bytes skiped for TLV Hdr size, fill the TLV Hdr in the end */
	pbuf = pmem + sizeof(i5_tlv_t);

	/* 6 bytes for Radio Unique Identifier */
	memcpy(pbuf, i5_ifr->InterfaceId, MAC_ADDR_LEN);
	pbuf += MAC_ADDR_LEN;

	/* 1 bytes for number of BSS. Fill later */
	pbsscount_mem = pbuf;
	pbuf++;

	/* Traverse BSS List */
	foreach_i5glist_item(i5_bss, i5_dm_bss_type, i5_ifr->bss_list) {
		unsigned char map_flags = 0;

		bss_vndr_data = (wbd_bss_item_t*)i5_bss->vndr_data;
		if (bss_vndr_data == NULL) {
			continue;
		}

		/* 6 bytes for BSSID */
		memcpy(pbuf, i5_bss->BSSID, MAC_ADDR_LEN);
		pbuf += MAC_ADDR_LEN;

		/* Get MAP flags */
		if (I5_IS_BSS_FRONTHAUL(i5_bss->mapFlags)) {
			map_flags |= IEEE1905_FRONTHAUL_BSS;
		}
		if (I5_IS_BSS_BACKHAUL(i5_bss->mapFlags)) {
			map_flags |= IEEE1905_BACKHAUL_BSS;
		}
		if (I5_IS_BSS_STA(i5_bss->mapFlags)) {
			map_flags |= IEEE1905_BACKHAUL_STA;
		}

		/* 1 bytes for MAP flags */
		*pbuf = (unsigned char)map_flags;
		pbuf++;

		/* 4 bytes for BSSID Information field */
		*((unsigned int *)pbuf) = htonl(bss_vndr_data->bssid_info);
		pbuf += 4;

		/* 1 bytes for PHY Type */
		*pbuf = (unsigned char)bss_vndr_data->phytype;
		pbuf++;

		bss_count++;
	}

	/* Fill the BSS count */
	*pbsscount_mem = (unsigned char)bss_count;

	/* Fill the TLV Hdr now */
	ptlv = (i5_tlv_t *)pmem;
	ptlv->type = WBD_TLV_BSS_CAPABILITY_REPORT_TYPE;
	ptlv->length = htons(pbuf-pmem-sizeof(i5_tlv_t));

	/* Update out data length */
	*tlv_data_len = pbuf-pmem;

end:
	WBD_EXIT();
	return ret;
}

/* Decode Vendor Specific TLV for Message : BSS capability report on recieve */
int
wbd_tlv_decode_bss_capability_report(void* data, unsigned char* tlv_data, unsigned int tlv_data_len)
{
	int ret = WBDE_OK;
	unsigned char idx, bss_count = 0, radio_mac[MAC_ADDR_LEN], bssid[MAC_ADDR_LEN];
	unsigned char *pValue;
	unsigned char pos = 0, map_flags;
	uint32 bssid_info;
	uint8 phytype;
	i5_dm_device_type *i5_device = (i5_dm_device_type*)data;
	i5_dm_interface_type *i5_ifr = NULL;
	i5_dm_bss_type *i5_bss = NULL;
	wbd_bss_item_t *bss_vndr_data;
	WBD_ENTER();

	/* Validate args */
	WBD_ASSERT_ARG(tlv_data, WBDE_INV_ARG);

	/* Initialize data pointer */
	pValue = tlv_data;

	/* bytes skiped for TLV Hdr size, fill the TLV Hdr in the end */
	pValue += sizeof(i5_tlv_t);

	if (tlv_data_len < WBD_MIN_LEN_TLV_BSS_CAPABILITY_REPORT) {
		WBD_WARNING("tlv_data_len[%d] < BSS Capability Report Len[%d]\n",
			tlv_data_len, WBD_MIN_LEN_TLV_BSS_CAPABILITY_REPORT);
		ret = WBDE_TLV_ERROR;
		goto end;
	}

	/* 6 bytes for Radio Unique Identifier */
	memcpy(radio_mac, &pValue[pos], MAC_ADDR_LEN);
	pos += MAC_ADDR_LEN;

	bss_count = (unsigned char)pValue[pos];
	pos++;

	WBD_INFO("BSS Capability Report Radio["MACDBG"] BSS Count[%d]\n",
		MAC2STRDBG(radio_mac), bss_count);
	if (bss_count <= 0) {
		goto end;
	}

	if (tlv_data_len < (pos + (bss_count * (MAC_ADDR_LEN + 6)))) {
		WBD_WARNING("tlv_data_len[%d] != BSS Capability Report Len[%d]\n",
			tlv_data_len, (pos + (bss_count * (MAC_ADDR_LEN + 6))));
		ret = WBDE_TLV_ERROR;
		goto end;
	}

	i5_ifr = wbd_ds_get_i5_ifr_in_device(i5_device, radio_mac, &ret);
	if (i5_ifr == NULL) {
		WBD_WARNING("Failed to find Interface["MACDBG"] in Device["MACDBG"]\n",
			MAC2STRDBG(radio_mac), MAC2STRDBG(i5_device->DeviceId));
		goto end;
	}

	for (idx = 0; idx < bss_count; idx++) {
		/* 6 bytes for BSSID */
		memcpy(bssid, &pValue[pos], MAC_ADDR_LEN);
		pos += MAC_ADDR_LEN;

		/* One byte for map flags */
		map_flags = (unsigned char)pValue[pos];
		pos++;

		/* Get 4 octets of BSSID Information */
		bssid_info = ntohl(*((unsigned int *)&pValue[pos]));
		pos += 4;

		/* One byte for PHY Type */
		phytype = (unsigned char)pValue[pos];
		pos++;

		WBD_INFO("Radio["MACDBG"] BSS Count[%d] idx[%d] BSSID["MACDBG"] MAP[%02x] "
			"BSSIDINFO[0x%x] PHYType[0x%x]\n",
			MAC2STRDBG(radio_mac), bss_count, idx+1, MAC2STRDBG(bssid), map_flags,
			bssid_info, phytype);

		i5_bss = wbd_ds_get_i5_bss_in_ifr(i5_ifr, bssid, &ret);
		if (ret != WBDE_OK) {
			WBD_WARNING("Failed to find BSS["MACDBG"] in Interface["MACDBG"] "
				"Device["MACDBG"]\n",
				MAC2STRDBG(bssid), MAC2STRDBG(i5_ifr->InterfaceId),
				MAC2STRDBG(i5_device->DeviceId));
			continue;
		}

		bss_vndr_data = (wbd_bss_item_t*)i5_bss->vndr_data;
		if (bss_vndr_data == NULL) {
			WBD_WARNING("BSS["MACDBG"] NULL Vendor Data\n", MAC2STRDBG(i5_bss->BSSID));
			continue;
		}

		/* No need to update the mapFlags as we update it from the BSS info table in
		 * IEEE1905 module itslef
		 */

		bss_vndr_data->bssid_info = bssid_info;
		bss_vndr_data->phytype = phytype;
	}
end:
	WBD_EXIT();
	return ret;
}

/* Encode Vendor Specific TLV for Message : REMOVE_CLIENT_REQ to send */
int
wbd_tlv_encode_remove_client_request(void* data,
	unsigned char* tlv_data, unsigned int* tlv_data_len)
{
	int ret = WBDE_OK;
	wbd_cmd_remove_client_t *cmd = (wbd_cmd_remove_client_t*)data;
	unsigned char *pbuf, *pmem;
	i5_tlv_t *ptlv;
	WBD_ENTER();

	/* Validate args */
	WBD_ASSERT_ARG(tlv_data, WBDE_INV_ARG);
	WBD_ASSERT_ARG(tlv_data_len, WBDE_INV_ARG);

	/* Pointing to out data */
	pmem = tlv_data;

	/* bytes skiped for TLV Hdr size, fill the TLV Hdr in the end */
	pbuf = pmem + sizeof(i5_tlv_t);

	/* 6 bytes for MAC address of the STA to be Removed */
	memcpy(pbuf, &(cmd->stamac), MAC_ADDR_LEN);
	pbuf += MAC_ADDR_LEN;

	/* 6 bytes for latest bssid of the STA */
	memcpy(pbuf, &(cmd->bssid), MAC_ADDR_LEN);
	pbuf += MAC_ADDR_LEN;

	/* Fill the TLV Hdr now */
	ptlv = (i5_tlv_t *)pmem;
	ptlv->type = WBD_TLV_REMOVE_CLIENT_REQ_TYPE;
	ptlv->length = htons(pbuf-pmem-sizeof(i5_tlv_t));

	/* Update out data length */
	*tlv_data_len = pbuf-pmem;

end:
	WBD_EXIT();
	return ret;
}

/* Decode Vendor Specific TLV for Message : REMOVE_CLIENT_REQ on recieve */
int
wbd_tlv_decode_remove_client_request(void* data,
	unsigned char* tlv_data, unsigned int tlv_data_len)
{
	int ret = WBDE_OK;
	wbd_cmd_remove_client_t *cmd = (wbd_cmd_remove_client_t*)data;
	unsigned char *pValue;
	unsigned short pos = 0;
	WBD_ENTER();

	/* Validate args */
	WBD_ASSERT_ARG(tlv_data, WBDE_INV_ARG);

	/* Initialize data pointer */
	pValue = tlv_data;

	/* bytes skiped for TLV Hdr size, fill the TLV Hdr in the end */
	pValue += sizeof(i5_tlv_t);

	if (tlv_data_len >= (pos + MAC_ADDR_LEN)) {
		/* 6 bytes for STA MAC of the STA */
		memcpy(&(cmd->stamac), &pValue[pos], MAC_ADDR_LEN);
		pos += MAC_ADDR_LEN;
	} else {
		goto end;
	}

	if (tlv_data_len >= (pos + MAC_ADDR_LEN)) {
		/* 6 bytes for latest bssid of STA */
		memcpy(&(cmd->bssid), &pValue[pos], MAC_ADDR_LEN);
		pos += MAC_ADDR_LEN;
	} else {
		goto end;
	}

end:
	WBD_EXIT();
	return ret;
}

/* Encode Vendor Specific TLV for Message : BSS metrics query */
int
wbd_tlv_encode_bss_metrics_query(void* data, unsigned char* tlv_data, unsigned int* tlv_data_len)
{
	int ret = WBDE_OK;
	unsigned char *pbuf, *pmem;
	i5_tlv_t *ptlv;
	WBD_ENTER();

	/* Validate args */
	WBD_ASSERT_ARG(tlv_data, WBDE_INV_ARG);
	WBD_ASSERT_ARG(tlv_data_len, WBDE_INV_ARG);

	/* Pointing to out data */
	pmem = tlv_data;

	/* bytes skiped for TLV Hdr size, fill the TLV Hdr in the end */
	pbuf = pmem + sizeof(i5_tlv_t);

	/* 1 bytes for number of Radio. Which is 0, means ask the details of all BSS and Radio */
	*pbuf = (unsigned char)0;
	pbuf++;

	/* Fill the TLV Hdr now */
	ptlv = (i5_tlv_t *)pmem;
	ptlv->type = WBD_TLV_BSS_METRICS_QUERY_TYPE;
	ptlv->length = htons(pbuf-pmem-sizeof(i5_tlv_t));

	/* Update out data length */
	*tlv_data_len = pbuf-pmem;

end:
	WBD_EXIT();
	return ret;
}

/* Decode Vendor Specific TLV for Message : BSS metrics query on recieve */
int
wbd_tlv_decode_bss_metrics_query(void* data, unsigned char* tlv_data, unsigned int tlv_data_len)
{
	int ret = WBDE_OK;
	unsigned char *pValue;
	WBD_ENTER();

	/* Validate args */
	WBD_ASSERT_ARG(tlv_data, WBDE_INV_ARG);

	/* Initialize data pointer */
	pValue = tlv_data;

	/* bytes skiped for TLV Hdr size, fill the TLV Hdr in the end */
	pValue += sizeof(i5_tlv_t);

	if (tlv_data_len < WBD_MIN_LEN_TLV_BSS_METRICS_QUERY) {
		WBD_WARNING("tlv_data_len[%d] < BSS metrics Query Len[%d]\n",
			tlv_data_len, WBD_MIN_LEN_TLV_BSS_METRICS_QUERY);
		ret = WBDE_TLV_ERROR;
		goto end;
	}

	/* No need to read 1 byte of Radio count, as it is always be 0 for now. In future,
	 * List of Radios and BSSIDs operated by the MAP device can be provided here. So that the
	 * agent will send the response only for those BSSID's
	 */

end:
	WBD_EXIT();
	return ret;
}

/* Encode Vendor Specific TLV for Message : BSS metrics Report */
int
wbd_tlv_encode_bss_metrics_report(void* data, unsigned char* tlv_data,
	unsigned int* tlv_data_len)
{
	int ret = WBDE_OK;
	i5_dm_device_type *i5_device = (i5_dm_device_type*)data;
	i5_dm_interface_type *i5_ifr;
	i5_dm_bss_type *i5_bss;
	wbd_bss_item_t *bss_vndr_data;
	unsigned char *pbuf, *pmem, *pbsscount_mem;
	i5_tlv_t *ptlv;
	WBD_ENTER();

	/* Validate args */
	WBD_ASSERT_ARG(tlv_data, WBDE_INV_ARG);
	WBD_ASSERT_ARG(tlv_data_len, WBDE_INV_ARG);

	/* Pointing to out data */
	pmem = tlv_data;

	/* bytes skiped for TLV Hdr size, fill the TLV Hdr in the end */
	pbuf = pmem + sizeof(i5_tlv_t);

	/* 1 bytes for number of Radios */
	*pbuf = (unsigned char)i5_device->InterfaceNumberOfEntries;
	pbuf++;

	/* Traverse Interface List */
	foreach_i5glist_item(i5_ifr, i5_dm_interface_type, i5_device->interface_list) {

		unsigned char bss_count = 0;

		/* 6 bytes for Radio Unique Identifier */
		memcpy(pbuf, i5_ifr->InterfaceId, MAC_ADDR_LEN);
		pbuf += MAC_ADDR_LEN;

		/* 1 bytes for number of BSS */
		pbsscount_mem = pbuf;
		pbuf++;

		/* Traverse BSS List */
		foreach_i5glist_item(i5_bss, i5_dm_bss_type, i5_ifr->bss_list) {

			bss_vndr_data = (wbd_bss_item_t*)i5_bss->vndr_data;
			if (bss_vndr_data == NULL) {
				WBD_WARNING("BSS["MACDBG"] NULL Vendor Data\n",
					MAC2STRDBG(i5_bss->BSSID));
				continue;
			}

			/* 6 bytes for BSSID */
			memcpy(pbuf, i5_bss->BSSID, MAC_ADDR_LEN);
			pbuf += MAC_ADDR_LEN;

			/* 4 octets for Average Tx Rate */
			*((unsigned int *)pbuf) = htonl(bss_vndr_data->avg_tx_rate);
			pbuf += 4;

			/* 4 octets for Maximum Tx Rate */
			*((unsigned int *)pbuf) = htonl(bss_vndr_data->max_tx_rate);
			pbuf += 4;

			bss_count++;
		}

		/* Fill the BSS count */
		*pbsscount_mem = (unsigned char)bss_count;
	}

	/* Fill the TLV Hdr now */
	ptlv = (i5_tlv_t *)pmem;
	ptlv->type = WBD_TLV_BSS_METRICS_REPORT_TYPE;
	ptlv->length = htons(pbuf-pmem-sizeof(i5_tlv_t));

	/* Update out data length */
	*tlv_data_len = pbuf-pmem;

end:
	WBD_EXIT();
	return ret;
}

/* Decode Vendor Specific TLV for Message : BSS metrics report on recieve */
int
wbd_tlv_decode_bss_metrics_report(void* data, unsigned char* tlv_data, unsigned int tlv_data_len)
{
	int ret = WBDE_OK;
	unsigned char idx, j, radio_count = 0, bss_count = 0, radio_mac[MAC_ADDR_LEN];
	unsigned char bssid[MAC_ADDR_LEN];
	unsigned char *pValue;
	unsigned char pos = 0;
	uint32 avg_tx_rate, max_tx_rate;
	wbd_tlv_decode_cmn_data_t *cmn_data = (wbd_tlv_decode_cmn_data_t*)data;
	i5_dm_device_type *i5_device;
	i5_dm_interface_type *i5_ifr;
	i5_dm_bss_type *i5_bss;
	wbd_bss_item_t *bss_vndr_data;
	wbd_info_t *info;
	wbd_master_info_t *master_info;
	WBD_ENTER();

	/* Validate args */
	WBD_ASSERT_ARG(tlv_data, WBDE_INV_ARG);

	i5_device = (i5_dm_device_type*)cmn_data->data;
	info = cmn_data->info;
	WBD_SAFE_GET_MASTER_INFO(info, WBD_BKT_ID_BR0, master_info, &ret);

	/* Initialize data pointer */
	pValue = tlv_data;

	/* bytes skiped for TLV Hdr size, fill the TLV Hdr in the end */
	pValue += sizeof(i5_tlv_t);

	if (tlv_data_len < WBD_MIN_LEN_TLV_BSS_METRICS_REPORT) {
		WBD_WARNING("tlv_data_len[%d] < BSS metrics Report Len[%d]\n",
			tlv_data_len, WBD_MIN_LEN_TLV_BSS_METRICS_REPORT);
		ret = WBDE_TLV_ERROR;
		goto end;
	}

	/* 1 octet for Number of Radios reported */
	radio_count = (unsigned char)pValue[pos];
	pos++;
	if (radio_count <= 0) {
		goto end;
	}

	/* For every Radio reported get the Radio MAC and its BSS Metrics from the message */
	for (idx = 0; idx < radio_count; idx++) {

		if (tlv_data_len < (pos + MAC_ADDR_LEN + 1)) {
			WBD_WARNING("tlv_data_len[%d] < BSS Metrics Report Len[%d]\n",
				tlv_data_len, (pos + MAC_ADDR_LEN + 1));
			ret = WBDE_TLV_ERROR;
			goto end;
		}

		/* 6 bytes for Radio Unique Identifier */
		memcpy(radio_mac, &pValue[pos], MAC_ADDR_LEN);
		pos += MAC_ADDR_LEN;

		i5_ifr = wbd_ds_get_i5_ifr_in_device(i5_device, radio_mac, &ret);
		if (i5_ifr == NULL) {
			WBD_WARNING("Failed to find Interface["MACDBG"] in Device["MACDBG"]\n",
				MAC2STRDBG(radio_mac), MAC2STRDBG(i5_device->DeviceId));
			goto end;
		}

		/* 1 octet for Number of BSS */
		bss_count = (unsigned char)pValue[pos];
		pos++;

		WBD_DEBUG("BSS Metrics Report Radio["MACDBG"] BSS Count[%d]\n",
			MAC2STRDBG(radio_mac), bss_count);
		if (bss_count <= 0) {
			continue;
		}

		/* For every BSS reported get the BSSID and its Metrics from the message */
		for (j = 0; j < bss_count; j++) {
			if (tlv_data_len < (pos + MAC_ADDR_LEN + 8)) {
				WBD_WARNING("tlv_data_len[%d] < BSS Metrics Report Len[%d]\n",
					tlv_data_len, (pos + MAC_ADDR_LEN + 8));
				ret = WBDE_TLV_ERROR;
				goto end;
			}

			/* 6 bytes for BSSID */
			memcpy(bssid, &pValue[pos], MAC_ADDR_LEN);
			pos += MAC_ADDR_LEN;

			/* Get 4 octets of Average Tx Rate */
			avg_tx_rate = ntohl(*((unsigned int *)&pValue[pos]));
			pos += 4;

			/* Get 4 octets of Maximum Tx Rate */
			max_tx_rate = ntohl(*((unsigned int *)&pValue[pos]));
			pos += 4;

			i5_bss = wbd_ds_get_i5_bss_in_ifr(i5_ifr, bssid, &ret);
			if (ret != WBDE_OK) {
				WBD_WARNING("Failed to find BSS["MACDBG"] in Interface["MACDBG"] "
					"Device["MACDBG"]\n",
					MAC2STRDBG(bssid), MAC2STRDBG(i5_ifr->InterfaceId),
					MAC2STRDBG(i5_device->DeviceId));
				continue;
			}

			if (i5_bss->vndr_data == NULL) {
				WBD_WARNING("BSS["MACDBG"] NULL Vendor Data\n",
					MAC2STRDBG(i5_bss->BSSID));
				continue;
			}

			/* Store average and maximum tx rate in BSS vendor data */
			bss_vndr_data = (wbd_bss_item_t*)i5_bss->vndr_data;
			bss_vndr_data->avg_tx_rate = avg_tx_rate;
			bss_vndr_data->max_tx_rate = max_tx_rate;
			WBD_INFO("Radio["MACDBG"] BSS Count[%d] BSSID["MACDBG"] "
				"avg_tx_rate[%d] max_tx_rate[%d]]\n",
				MAC2STRDBG(radio_mac), bss_count, MAC2STRDBG(bssid),
				bss_vndr_data->avg_tx_rate, bss_vndr_data->max_tx_rate);

			/* Update maximum of avg_tx_rate */
			if (bss_vndr_data->avg_tx_rate > master_info->max_avg_tx_rate) {
				master_info->max_avg_tx_rate = bss_vndr_data->avg_tx_rate;
			}
		}
	}

end:
	WBD_EXIT();
	return ret;
}

/* Encode Vendor Specific TLV for Message : Steer Response Report */
int
wbd_tlv_encode_steer_resp_report(void* data, unsigned char* tlv_data,
	unsigned int* tlv_data_len)
{
	int ret = WBDE_OK, len = 0;
	wbd_cmd_steer_resp_rpt_t *steer_resp_rpt = (wbd_cmd_steer_resp_rpt_t*)data;
	i5_tlv_t *ptlv;
	WBD_ENTER();

	/* Validate args */
	WBD_ASSERT_ARG(tlv_data, WBDE_INV_ARG);
	WBD_ASSERT_ARG(tlv_data_len, WBDE_INV_ARG);

	/* Pointing to out data */
	ptlv = (i5_tlv_t*)tlv_data;
	ptlv->type = WBD_TLV_STEER_RESP_REPORT_TYPE;
	ptlv->length = htons(WBD_LEN_TLV_STEER_RESP_REPORT);
	len += sizeof(i5_tlv_t);

	/* 6 Bytes for bssid */
	memcpy(&tlv_data[len], steer_resp_rpt->bssid, MAC_ADDR_LEN);
	len += MAC_ADDR_LEN;

	/* 6 Bytes for sta mac */
	memcpy(&tlv_data[len], steer_resp_rpt->sta_mac, MAC_ADDR_LEN);
	len += MAC_ADDR_LEN;

	/* 1 Byte for status code */
	tlv_data[len] =  steer_resp_rpt->status;
	len++;

	/* Update out data length */
	*tlv_data_len = len;

end:
	WBD_EXIT();
	return ret;
}

/* Decode Vendor Specific TLV for Message : Steer Response Report on recieve */
int
wbd_tlv_decode_steer_resp_report(void* data, unsigned char* tlv_data, unsigned int tlv_data_len)
{
	int ret = WBDE_OK, pos = 0;
	unsigned char *pValue;
	wbd_cmd_steer_resp_rpt_t *steer_resp_rpt = (wbd_cmd_steer_resp_rpt_t*)data;

	WBD_ENTER();

	/* Validate args */
	WBD_ASSERT_ARG(tlv_data, WBDE_INV_ARG);

	if (tlv_data_len < WBD_LEN_TLV_STEER_RESP_REPORT) {
		WBD_WARNING("tlv_data_len[%d] < Steer Resp  Report Len[%d]\n",
			tlv_data_len, WBD_LEN_TLV_STEER_RESP_REPORT);
		ret = WBDE_TLV_ERROR;
		goto end;
	}

	/* Initialize data pointer */
	pValue = tlv_data;

	/* bytes skiped for TLV Hdr size, fill the TLV Hdr in the end */
	pValue += sizeof(i5_tlv_t);

	/* 6 bytes for bssid */
	memcpy(steer_resp_rpt->bssid, &pValue[pos], MAC_ADDR_LEN);
	pos += MAC_ADDR_LEN;

	/* 6 bytes for sta mac */
	memcpy(steer_resp_rpt->sta_mac, &pValue[pos], MAC_ADDR_LEN);
	pos += MAC_ADDR_LEN;

	/* 1 bytes for status code */
	steer_resp_rpt->status = (unsigned char)pValue[pos];
	pos++;

end:
	WBD_EXIT();
	return ret;
}

/* Encode Vendor Specific TLV for Message : Vendor Steer Request */
int
wbd_tlv_encode_vendor_steer_request(void* data, unsigned char* tlv_data,
	unsigned int* tlv_data_len)
{
	int ret = WBDE_OK;
	i5_dm_bss_type *i5_bss = (i5_dm_bss_type*)data;
	wbd_bss_item_t *bss_vndr_data;
	unsigned char *pbuf, *pmem;
	i5_tlv_t *ptlv;
	WBD_ENTER();

	/* Validate args */
	WBD_ASSERT_ARG(tlv_data, WBDE_INV_ARG);
	WBD_ASSERT_ARG(tlv_data_len, WBDE_INV_ARG);

	/* Pointing to out data */
	pmem = tlv_data;

	/* bytes skiped for TLV Hdr size, fill the TLV Hdr in the end */
	pbuf = pmem + sizeof(i5_tlv_t);

	bss_vndr_data = (wbd_bss_item_t*)i5_bss->vndr_data;

	/* 1 bytes for number of BSS */
	if (bss_vndr_data) {
		*pbuf = (unsigned char)1;
		pbuf++;
	} else {
		WBD_WARNING("BSS["MACDBG"] NULL Vendor Data\n", MAC2STRDBG(i5_bss->BSSID));
		*pbuf = (unsigned char)0;
		pbuf++;
		goto encode_hdr;
	}

	/* 6 bytes for BSSID */
	memcpy(pbuf, i5_bss->BSSID, MAC_ADDR_LEN);
	pbuf += MAC_ADDR_LEN;

	/* 4 bytes for BSSID Information field */
	*((unsigned int *)pbuf) = htonl(bss_vndr_data->bssid_info);
	pbuf += 4;

	/* 1 bytes for PHY Type */
	*pbuf = (unsigned char)bss_vndr_data->phytype;
	pbuf++;

encode_hdr:
	/* Fill the TLV Hdr now */
	ptlv = (i5_tlv_t *)pmem;
	ptlv->type = WBD_TLV_STEER_REQUEST_TYPE;
	ptlv->length = htons(pbuf-pmem-sizeof(i5_tlv_t));

	/* Update out data length */
	*tlv_data_len = pbuf-pmem;

end:
	WBD_EXIT();
	return ret;
}

/* Decode Vendor Specific TLV for Message : Vendor Steer Request on recieve */
int
wbd_tlv_decode_vendor_steer_request(void* data, unsigned char* tlv_data, unsigned int tlv_data_len)
{
	int ret = WBDE_OK;
	unsigned char idx, bss_count = 0;
	unsigned char *pValue;
	unsigned char pos = 0, bssid[MAC_ADDR_LEN];
	wbd_bss_item_t *bss_vndr_data;
	i5_dm_bss_type *i5_bss;
	uint32 bssid_info;
	uint8 phytype;
	WBD_ENTER();

	/* Validate args */
	WBD_ASSERT_ARG(tlv_data, WBDE_INV_ARG);

	/* Initialize data pointer */
	pValue = tlv_data;

	/* bytes skiped for TLV Hdr size, fill the TLV Hdr in the end */
	pValue += sizeof(i5_tlv_t);

	if (tlv_data_len < WBD_LEN_TLV_STEER_REQUEST) {
		WBD_INFO("tlv_data_len[%d] < Vendor Steer Request Len[%d]\n",
			tlv_data_len, WBD_LEN_TLV_STEER_REQUEST);
		ret = WBDE_TLV_ERROR;
		goto end;
	}

	/* 1 byte of BSS count */
	bss_count = (unsigned char)pValue[pos];
	pos++;

	WBD_INFO("Vendor Steer Request BSS Count[%d]\n", bss_count);
	if (bss_count <= 0) {
		goto end;
	}

	if (tlv_data_len < (pos + (bss_count * (MAC_ADDR_LEN + 5)))) {
		WBD_INFO("tlv_data_len[%d] < Vendor Steer Request Len[%d]\n",
			tlv_data_len, (pos + (bss_count * (MAC_ADDR_LEN + 5))));
		ret = WBDE_TLV_ERROR;
		goto end;
	}

	for (idx = 0; idx < bss_count; idx++) {
		/* 6 bytes for BSSID */
		memcpy(bssid, &pValue[pos], MAC_ADDR_LEN);
		pos += MAC_ADDR_LEN;

		/* Get 4 octets of BSSID Information */
		bssid_info = ntohl(*((unsigned int *)&pValue[pos]));
		pos += 4;

		/* One byte for PHY Type */
		phytype = (unsigned char)pValue[pos];
		pos++;

		WBD_INFO("BSS Count[%d] idx[%d] BSSID["MACDBG"] BSSIDINFO[0x%x] PHYType[0x%x]\n",
			bss_count, idx+1, MAC2STRDBG(bssid), bssid_info, phytype);

		i5_bss = wbd_ds_get_i5_bss_in_topology(bssid, &ret);
		if (i5_bss && i5_bss->vndr_data) {
			bss_vndr_data = (wbd_bss_item_t*)i5_bss->vndr_data;
			bss_vndr_data->bssid_info = bssid_info;
			bss_vndr_data->phytype = phytype;
		}
	}
end:
	WBD_EXIT();
	return ret;
}

/* Generic routine to encode/decode Vendor Specific TLV for Message : */
int
wbd_tlv_encode_msg(void* data, unsigned int data_len, unsigned char tlv_type,
	unsigned char *tlv_data, unsigned int* tlv_data_len)
{
	int ret = WBDE_OK;
	unsigned char *pbuf, *pmem;
	i5_tlv_t *ptlv;
	WBD_ENTER();

	/* Validate args */
	WBD_ASSERT_ARG(tlv_data, WBDE_INV_ARG);
	WBD_ASSERT_ARG(tlv_data_len, WBDE_INV_ARG);

	/* Pointing to out data */
	pmem = tlv_data;

	/* bytes skiped for TLV Hdr size, fill the TLV Hdr in the end */
	pbuf = pmem + sizeof(i5_tlv_t);

	memcpy(pbuf, data, data_len);
	pbuf += data_len;

	/* Fill the TLV Hdr now */
	ptlv = (i5_tlv_t *)pmem;
	ptlv->type = tlv_type;
	ptlv->length = htons(pbuf-pmem-sizeof(i5_tlv_t));

	/* Update out data length */
	*tlv_data_len = pbuf-pmem;
end:
	WBD_EXIT();
	return ret;
}

/* Encode Vendor Specific TLV for Message : Zero wait DFS */
int
wbd_tlv_encode_zwdfs_msg(void* data, unsigned char* tlv_data, unsigned int* tlv_data_len)
{
	int ret = WBDE_OK;
	unsigned char *pbuf, *pmem;
	i5_tlv_t *ptlv;
	wbd_cmd_vndr_zwdfs_msg_t *zwdfs_msg = NULL;
	wbd_cmd_vndr_zwdfs_msg_t *zwdfs_pbuf = NULL;
	WBD_ENTER();

	/* Validate args */
	WBD_ASSERT_ARG(tlv_data, WBDE_INV_ARG);
	WBD_ASSERT_ARG(tlv_data_len, WBDE_INV_ARG);

	zwdfs_msg = (wbd_cmd_vndr_zwdfs_msg_t*)data;
	/* Pointing to out data */
	pmem = tlv_data;

	/* bytes skiped for TLV Hdr size, fill the TLV Hdr in the end */
	pbuf = pmem + sizeof(i5_tlv_t);

	zwdfs_pbuf = (wbd_cmd_vndr_zwdfs_msg_t *)pbuf;
	zwdfs_pbuf->cntrl_chan = zwdfs_msg->cntrl_chan;
	zwdfs_pbuf->opclass = zwdfs_msg->opclass;
	zwdfs_pbuf->reason = htonl(zwdfs_msg->reason);
	memcpy(&zwdfs_pbuf->mac, &zwdfs_msg->mac, ETHER_ADDR_LEN);

	pbuf += sizeof(wbd_cmd_vndr_zwdfs_msg_t);

	/* Fill the TLV Hdr now */
	ptlv = (i5_tlv_t *)pmem;
	ptlv->type = WBD_TLV_ZWDFS_TYPE;
	ptlv->length = htons(pbuf-pmem-sizeof(i5_tlv_t));

	/* Update out data length */
	*tlv_data_len = pbuf-pmem;
end:
	WBD_EXIT();
	return ret;
}

/* Decode Vendor Specific TLV for Message : Zero wait DFS */
int
wbd_tlv_decode_zwdfs_msg(void* data, unsigned char* tlv_data, unsigned int tlv_data_len)
{
	int ret = WBDE_OK;
	uint8 *ptlv = NULL;
	wbd_cmd_vndr_zwdfs_msg_t *zwdfs_msg = NULL;
	wbd_cmd_vndr_zwdfs_msg_t *zwdfs_tlv = NULL;

	WBD_ENTER();

	if (!data || !tlv_data) {
		WBD_ERROR("Invalid arg, return \n");
		ret = WBDE_INV_ARG;
		goto end;
	}
	if (tlv_data_len < sizeof(*zwdfs_msg)) {
		WBD_WARNING("tlv_data_len[%d] < Minimum zwdfs control block[%zu]\n",
			tlv_data_len, sizeof(*zwdfs_msg));
		ret = WBDE_TLV_ERROR;
		goto end;
	}
	zwdfs_msg = (wbd_cmd_vndr_zwdfs_msg_t*)data;
	ptlv = tlv_data;
	ptlv += sizeof(i5_tlv_t);

	zwdfs_tlv = (wbd_cmd_vndr_zwdfs_msg_t*)ptlv;

	zwdfs_msg->cntrl_chan = zwdfs_tlv->cntrl_chan;
	zwdfs_msg->opclass = zwdfs_tlv->opclass;
	zwdfs_msg->reason = ntohl(zwdfs_tlv->reason);
	memcpy(&zwdfs_msg->mac, &zwdfs_tlv->mac, ETHER_ADDR_LEN);

	WBD_EXIT();
end:
	return ret;
}

/* Encode Vendor Specific TLV for Message : Backhaul STA Vendor Metric Reporting Policy Config */
int
wbd_tlv_encode_backhaul_sta_metric_report_policy(void* data, unsigned char* tlv_data,
	unsigned int* tlv_data_len)
{
	int ret = WBDE_OK;
	wbd_weak_sta_policy_t *cmd = (wbd_weak_sta_policy_t*)data;
	unsigned char *pbuf, *pmem;
	i5_tlv_t *ptlv;
	WBD_ENTER();

	/* Validate args */
	WBD_ASSERT_ARG(tlv_data, WBDE_INV_ARG);
	WBD_ASSERT_ARG(tlv_data_len, WBDE_INV_ARG);

	/* Pointing to out data */
	pmem = tlv_data;

	/* bytes skiped for TLV Hdr size, fill the TLV Hdr in the end */
	pbuf = pmem + sizeof(i5_tlv_t);

	/* 1 bytes for STA Metrics Reporting RCPI Threshold */
	*pbuf = (unsigned char)WBD_RSSI_TO_RCPI(cmd->t_rssi);
	pbuf++;

	/* 1 bytes for STA Metrics Reporting RCPI Hysteresis Margin Override */
	*pbuf = (unsigned char)cmd->t_hysterisis;
	pbuf++;

	/* 4 bytes for STA Metrics Reporting Idle rate Threshold */
	*((unsigned int *)pbuf) = htonl(cmd->t_idle_rate);
	pbuf += 4;

	/* 4 bytes for STA Metrics Reporting Tx rate Threshold */
	*((unsigned int *)pbuf) = htonl(cmd->t_tx_rate);
	pbuf += 4;

	/* 4 bytes for STA Metrics Reporting Tx_failures Threshold */
	*((unsigned int *)pbuf) = htonl(cmd->t_tx_failures);
	pbuf += 4;

	/* 1 bytes for Flags which indicates which fields to consider from policy config */
	*pbuf = (unsigned char)cmd->flags;
	pbuf++;

	/* Fill the TLV Hdr now */
	ptlv = (i5_tlv_t *)pmem;
	ptlv->type = WBD_TLV_BH_STA_METRIC_POLICY_TYPE;
	ptlv->length = htons(pbuf-pmem-sizeof(i5_tlv_t));

	/* Update out data length */
	*tlv_data_len = pbuf-pmem;

end:
	WBD_EXIT();
	return ret;
}

/* Decode Vendor Specific TLV for Message : Backhaul STA Vendor Metric Reporting Policy Config */
int
wbd_tlv_decode_backhaul_sta_metric_report_policy(void* data, unsigned char* tlv_data,
	unsigned int tlv_data_len)
{
	int ret = WBDE_OK;
	wbd_weak_sta_policy_t *cmd = (wbd_weak_sta_policy_t*)data;
	unsigned char *pValue;
	unsigned char pos = 0;
	WBD_ENTER();

	/* Validate args */
	WBD_ASSERT_ARG(tlv_data, WBDE_INV_ARG);

	/* Initialize data pointer */
	pValue = tlv_data;

	/* bytes skiped for TLV Hdr size, fill the TLV Hdr in the end */
	pValue += sizeof(i5_tlv_t);

	if (tlv_data_len < WBD_LEN_TLV_BH_STA_METRIC_POLICY) {
		WBD_WARNING("tlv_data_len[%d] < Backhaul STA Metric Policy Len[%d]\n",
			tlv_data_len, WBD_LEN_TLV_BH_STA_METRIC_POLICY);
		ret = WBDE_TLV_ERROR;
		goto end;
	}

	/* 1 bytes for STA Metrics Reporting RCPI Threshold */
	cmd->t_rssi = WBD_RCPI_TO_RSSI((unsigned char)pValue[pos]);
	pos++;

	/* 1 bytes for STA Metrics Reporting RCPI Hysteresis Margin Override */
	cmd->t_hysterisis = (unsigned char)pValue[pos];
	pos++;

	/* 4 bytes for STA Metrics Reporting Idle rate Threshold */
	cmd->t_idle_rate = ntohl(*((unsigned int *)&pValue[pos]));
	pos += 4;

	/* 4 bytes for STA Metrics Reporting Tx rate Threshold */
	cmd->t_tx_rate = ntohl(*((unsigned int *)&pValue[pos]));
	pos += 4;

	/* 4 bytes for STA Metrics Reporting Tx_failures Threshold */
	cmd->t_tx_failures = ntohl(*((unsigned int *)&pValue[pos]));
	pos += 4;

	/* 1 bytes for Flags which indicates which fields to consider from policy config */
	cmd->flags = (unsigned char)pValue[pos];
	pos++;

end:
	WBD_EXIT();
	return ret;
}

/* Encode Vendor Specific TLV for Message : NVRAM Set Vendor TLV */
int wbd_tlv_encode_nvram_set(void* data, unsigned char* tlv_data,
	unsigned int* tlv_data_len)
{
	int ret = WBDE_OK, idx, k;
	wbd_cmd_vndr_nvram_set_t *cmd = (wbd_cmd_vndr_nvram_set_t*)data;
	unsigned char *pbuf, *pmem;
	i5_tlv_t *ptlv;
	WBD_ENTER();

	/* Validate args */
	WBD_ASSERT_ARG(tlv_data, WBDE_INV_ARG);
	WBD_ASSERT_ARG(tlv_data_len, WBDE_INV_ARG);

	/* Pointing to out data */
	pmem = tlv_data;

	/* bytes skiped for TLV Hdr size, fill the TLV Hdr in the end */
	pbuf = pmem + sizeof(i5_tlv_t);

	/* 1 bytes for Number of NVRAMs independent of Radio/BSS */
	*pbuf = (unsigned char)cmd->n_common_nvrams;
	pbuf++;

	/* Fill all the common NVRAMs */
	if (cmd->n_common_nvrams > 0 && cmd->common_nvrams) {
		for (idx = 0; idx < cmd->n_common_nvrams; idx++) {
			/* 1 bytes for NVRAM name length */
			*pbuf = (unsigned char)strlen(cmd->common_nvrams[idx].name);
			pbuf++;

			/* Name of the NVRAM */
			if (strlen(cmd->common_nvrams[idx].name) > 0) {
				memcpy(pbuf, cmd->common_nvrams[idx].name,
					strlen(cmd->common_nvrams[idx].name));
				pbuf += strlen(cmd->common_nvrams[idx].name);
			}

			/* 1 bytes for NVRAM value length */
			*pbuf = (unsigned char)strlen(cmd->common_nvrams[idx].value);
			pbuf++;

			/* Value of the NVRAM */
			if (strlen(cmd->common_nvrams[idx].value) > 0) {
				memcpy(pbuf, cmd->common_nvrams[idx].value,
					strlen(cmd->common_nvrams[idx].value));
				pbuf += strlen(cmd->common_nvrams[idx].value);
			}
		}
	}

	/* 1 bytes for Number of Number of Radio Unique Identifiers */
	*pbuf = (unsigned char)cmd->n_radios;
	pbuf++;

	/* Fill all the Radios */
	if (cmd->n_radios > 0 && cmd->radio_nvrams) {
		for (idx = 0; idx < cmd->n_radios; idx++) {
			/* 6 bytes for Radio Uinque Identifier */
			memcpy(pbuf, &cmd->radio_nvrams[idx].mac,
				sizeof(cmd->radio_nvrams[idx].mac));
			pbuf += sizeof(cmd->radio_nvrams[idx].mac);

			/* 1 bytes for Number of NVRAMs */
			*pbuf = (unsigned char)cmd->radio_nvrams[idx].n_nvrams;
			pbuf++;

			/* Fill all the Radio NVRAMs */
			if (cmd->radio_nvrams[idx].n_nvrams <= 0 ||
				!cmd->radio_nvrams[idx].nvrams) {
				continue;
			}

			for (k = 0; k < cmd->radio_nvrams[idx].n_nvrams; k++) {
				/* 1 bytes for NVRAM name length */
				*pbuf = (unsigned char)
					strlen(cmd->radio_nvrams[idx].nvrams[k].name);
				pbuf++;

				/* Name of the NVRAM */
				if (strlen(cmd->radio_nvrams[idx].nvrams[k].name)) {
					memcpy(pbuf, cmd->radio_nvrams[idx].nvrams[k].name,
						strlen(cmd->radio_nvrams[idx].nvrams[k].name));
					pbuf += strlen(cmd->radio_nvrams[idx].nvrams[k].name);
				}

				/* 1 bytes for NVRAM value length */
				*pbuf = (unsigned char)
					strlen(cmd->radio_nvrams[idx].nvrams[k].value);
				pbuf++;

				/* Value of the NVRAM */
				if (strlen(cmd->radio_nvrams[idx].nvrams[k].value) > 0) {
					memcpy(pbuf, cmd->radio_nvrams[idx].nvrams[k].value,
						strlen(cmd->radio_nvrams[idx].nvrams[k].value));
					pbuf += strlen(cmd->radio_nvrams[idx].nvrams[k].value);
				}
			}
		}
	}

	/* 1 bytes for Number of Number of BSSIDs */
	*pbuf = (unsigned char)cmd->n_bsss;
	pbuf++;

	/* Fill all the BSSs */
	if (cmd->n_bsss > 0 && cmd->bss_nvrams) {
		for (idx = 0; idx < cmd->n_bsss; idx++) {
			/* 6 bytes for BSSID */
			memcpy(pbuf, &cmd->bss_nvrams[idx].mac,
				sizeof(cmd->bss_nvrams[idx].mac));
			pbuf += sizeof(cmd->bss_nvrams[idx].mac);

			/* 1 bytes for Number of NVRAMs */
			*pbuf = (unsigned char)cmd->bss_nvrams[idx].n_nvrams;
			pbuf++;

			/* Fill all the BSS NVRAMs */
			if (cmd->bss_nvrams[idx].n_nvrams <= 0 ||
				!cmd->bss_nvrams[idx].nvrams) {
				continue;
			}

			for (k = 0; k < cmd->bss_nvrams[idx].n_nvrams; k++) {
				/* 1 bytes for NVRAM name length */
				*pbuf = (unsigned char)strlen(cmd->bss_nvrams[idx].nvrams[k].name);
				pbuf++;

				/* Name of the NVRAM */
				if (strlen(cmd->bss_nvrams[idx].nvrams[k].name) > 0) {
					memcpy(pbuf, cmd->bss_nvrams[idx].nvrams[k].name,
						strlen(cmd->bss_nvrams[idx].nvrams[k].name));
					pbuf += strlen(cmd->bss_nvrams[idx].nvrams[k].name);
				}

				/* 1 bytes for NVRAM value length */
				*pbuf = (unsigned char)strlen(cmd->bss_nvrams[idx].nvrams[k].value);
				pbuf++;

				/* Value of the NVRAM */
				if (strlen(cmd->bss_nvrams[idx].nvrams[k].value) > 0) {
					memcpy(pbuf, cmd->bss_nvrams[idx].nvrams[k].value,
						strlen(cmd->bss_nvrams[idx].nvrams[k].value));
					pbuf += strlen(cmd->bss_nvrams[idx].nvrams[k].value);
				}
			}
		}
	}

	/* Fill the TLV Hdr now */
	ptlv = (i5_tlv_t *)pmem;
	ptlv->type = WBD_TLV_NVRAM_SET_TYPE;
	ptlv->length = htons(pbuf-pmem-sizeof(i5_tlv_t));

	/* Update out data length */
	*tlv_data_len = pbuf-pmem;

end:
	WBD_EXIT();
	return ret;
}

/* Decode Vendor Specific TLV for Message : NVRAM Set Vendor TLV */
int wbd_tlv_decode_nvram_set(void* data, unsigned char* tlv_data,
	unsigned int tlv_data_len)
{
	int ret = WBDE_OK, pos = 0, idx, k;
	uint8 name_len, value_len;
	unsigned char *pValue;
	wbd_cmd_vndr_nvram_set_t *cmd = (wbd_cmd_vndr_nvram_set_t*)data;

	WBD_ENTER();

	/* Validate args */
	WBD_ASSERT_ARG(tlv_data, WBDE_INV_ARG);

	if (tlv_data_len < WBD_MIN_LEN_TLV_NVRAM_SET) {
		WBD_WARNING("tlv_data_len[%d] < NVRAM Set Len[%d]\n",
			tlv_data_len, WBD_MIN_LEN_TLV_NVRAM_SET);
		ret = WBDE_TLV_ERROR;
		goto end;
	}

	/* Initialize data pointer */
	pValue = tlv_data;

	/* bytes skiped for TLV Hdr size */
	pValue += sizeof(i5_tlv_t);

	/* 1 bytes for Number of common NVRAMs */
	cmd->n_common_nvrams = (unsigned char)pValue[pos];
	pos++;

	/* Decode common NVRAMs */
	if (cmd->n_common_nvrams > 0) {
		cmd->common_nvrams = (wbd_cmd_vndr_nvram_t*)
			wbd_malloc((sizeof(wbd_cmd_vndr_nvram_t) * cmd->n_common_nvrams), &ret);
		WBD_ASSERT();

		for (idx = 0; idx < cmd->n_common_nvrams; idx++) {
			/* 1 bytes for NVRAM Name length */
			if (tlv_data_len < pos + 1) {
				goto end;
			}
			name_len = (unsigned char)pValue[pos];
			pos++;

			/* name_len bytes NVRAM name */
			if (name_len > 0) {
				if (tlv_data_len < pos + name_len) {
					goto end;
				}
				memcpy(cmd->common_nvrams[idx].name, &pValue[pos], name_len);
				pos += name_len;
			}

			/* 1 bytes for NVRAM Value length */
			if (tlv_data_len < pos + 1) {
				goto end;
			}
			value_len = (unsigned char)pValue[pos];
			pos++;

			/* name_len bytes NVRAM name */
			if (value_len > 0) {
				if (tlv_data_len < pos + value_len) {
					goto end;
				}
				memcpy(cmd->common_nvrams[idx].value, &pValue[pos], value_len);
				pos += value_len;
			}
			WBD_DEBUG("pos[%d] tlv_data_len[%d] name_len[%d] name[%s] value_len[%d] "
				"value[%s]\n", pos, tlv_data_len, name_len,
				cmd->common_nvrams[idx].name, value_len,
				cmd->common_nvrams[idx].value);
		}
	}

	/* 1 bytes for Number of Radios */
	if (tlv_data_len < pos + 1) {
		goto end;
	}
	cmd->n_radios = (unsigned char)pValue[pos];
	pos++;

	/* Decode Primary Prefix NVRAMs */
	if (cmd->n_radios > 0) {
		cmd->radio_nvrams = (wbd_cmd_vndr_prefix_nvram_t*)
			wbd_malloc((sizeof(wbd_cmd_vndr_prefix_nvram_t) * cmd->n_radios), &ret);
		WBD_ASSERT();

		for (idx = 0; idx < cmd->n_radios; idx++) {
			/* 6 bytes for MAC */
			if (tlv_data_len < pos + MAC_ADDR_LEN) {
				goto end;
			}
			memcpy(&cmd->radio_nvrams[idx].mac, &pValue[pos],
				sizeof(cmd->radio_nvrams[idx].mac));
			pos += sizeof(cmd->radio_nvrams[idx].mac);

			/* 1 bytes for Number of Radio NVRAMs */
			if (tlv_data_len < pos + 1) {
				goto end;
			}
			cmd->radio_nvrams[idx].n_nvrams = (unsigned char)pValue[pos];
			pos++;

			if (cmd->radio_nvrams[idx].n_nvrams <= 0) {
				continue;
			}

			cmd->radio_nvrams[idx].nvrams = (wbd_cmd_vndr_nvram_t*)
				wbd_malloc((sizeof(wbd_cmd_vndr_nvram_t) *
				cmd->radio_nvrams[idx].n_nvrams), &ret);
			WBD_ASSERT();

			/* Decode NVRAMs for each radio */
			for (k = 0; k < cmd->radio_nvrams[idx].n_nvrams; k++) {
				/* 1 bytes for NVRAM Name length */
				if (tlv_data_len < pos + 1) {
					goto end;
				}
				name_len = (unsigned char)pValue[pos];
				pos++;

				/* name_len bytes NVRAM name */
				if (name_len > 0) {
					if (tlv_data_len < pos + name_len) {
						goto end;
					}
					memcpy(cmd->radio_nvrams[idx].nvrams[k].name, &pValue[pos],
						name_len);
					pos += name_len;
				}

				/* 1 bytes for NVRAM Value length */
				if (tlv_data_len < pos + 1) {
					goto end;
				}
				value_len = (unsigned char)pValue[pos];
				pos++;

				/* name_len bytes NVRAM name */
				if (value_len > 0) {
					if (tlv_data_len < pos + value_len) {
						goto end;
					}
					memcpy(cmd->radio_nvrams[idx].nvrams[k].value, &pValue[pos],
						value_len);
					pos += value_len;
				}
				WBD_DEBUG("pos[%d] tlv_data_len[%d] MAC["MACDBG"] name_len[%d] "
					"name[%s] value_len[%d] value[%s]\n", pos, tlv_data_len,
					MAC2STRDBG(cmd->radio_nvrams[idx].mac), name_len,
					cmd->radio_nvrams[idx].nvrams[k].name, value_len,
					cmd->radio_nvrams[idx].nvrams[k].value);
			}
		}
	}

	/* 1 bytes for Number of BSSs */
	if (tlv_data_len < pos + 1) {
		goto end;
	}
	cmd->n_bsss = (unsigned char)pValue[pos];
	pos++;

	/* Decode BSS NVRAMs */
	if (cmd->n_bsss > 0) {
		cmd->bss_nvrams = (wbd_cmd_vndr_prefix_nvram_t*)
			wbd_malloc((sizeof(wbd_cmd_vndr_prefix_nvram_t) * cmd->n_bsss), &ret);
		WBD_ASSERT();

		for (idx = 0; idx < cmd->n_bsss; idx++) {
			/* 6 bytes for MAC */
			if (tlv_data_len < pos + MAC_ADDR_LEN) {
				goto end;
			}
			memcpy(&cmd->bss_nvrams[idx].mac, &pValue[pos],
				sizeof(cmd->bss_nvrams[idx].mac));
			pos += sizeof(cmd->bss_nvrams[idx].mac);

			/* 1 bytes for Number of Radio NVRAMs */
			if (tlv_data_len < pos + 1) {
				goto end;
			}
			cmd->bss_nvrams[idx].n_nvrams = (unsigned char)pValue[pos];
			pos++;

			if (cmd->bss_nvrams[idx].n_nvrams <= 0) {
				continue;
			}

			cmd->bss_nvrams[idx].nvrams = (wbd_cmd_vndr_nvram_t*)
				wbd_malloc((sizeof(wbd_cmd_vndr_nvram_t) *
				cmd->bss_nvrams[idx].n_nvrams), &ret);
			WBD_ASSERT();

			/* Decode NVRAMs for each radio */
			for (k = 0; k < cmd->bss_nvrams[idx].n_nvrams; k++) {
				/* 1 bytes for NVRAM Name length */
				if (tlv_data_len < pos + 1) {
					goto end;
				}
				name_len = (unsigned char)pValue[pos];
				pos++;

				/* name_len bytes NVRAM name */
				if (name_len > 0) {
					if (tlv_data_len < pos + name_len) {
						goto end;
					}
					memcpy(cmd->bss_nvrams[idx].nvrams[k].name, &pValue[pos],
						name_len);
					pos += name_len;
				}

				/* 1 bytes for NVRAM Value length */
				if (tlv_data_len < pos + 1) {
					goto end;
				}
				value_len = (unsigned char)pValue[pos];
				pos++;

				/* name_len bytes NVRAM name */
				if (value_len > 0) {
					if (tlv_data_len < pos + value_len) {
						goto end;
					}
					memcpy(cmd->bss_nvrams[idx].nvrams[k].value, &pValue[pos],
						value_len);
					pos += value_len;
				}
				WBD_DEBUG("pos[%d] tlv_data_len[%d] MAC["MACDBG"] name_len[%d] "
					"name[%s] value_len[%d] value[%s]\n", pos, tlv_data_len,
					MAC2STRDBG(cmd->bss_nvrams[idx].mac), name_len,
					cmd->bss_nvrams[idx].nvrams[k].name, value_len,
					cmd->bss_nvrams[idx].nvrams[k].value);
			}
		}
	}
	WBD_DEBUG("pos[%d] tlv_data_len[%d] Decode Successfull\n", pos, tlv_data_len);

end:
	WBD_EXIT();
	return ret;
}

/* Encode Vendor Specific TLV for Message : chan info */
int
wbd_tlv_encode_chan_info(void* data, unsigned char* tlv_data,
	unsigned int* tlv_data_len)
{
	wbd_cmd_vndr_intf_chan_info_t *pinbuf = NULL;
	wbd_cmd_vndr_intf_chan_info_t *poutbuf = NULL;
	wbd_interface_chan_info_t *chan_info = NULL;
	int ret = WBDE_OK;
	unsigned char *pbuf, *pmem;
	i5_tlv_t *ptlv;
	uint32 i = 0;

	WBD_ENTER();

	/* Validate args */
	WBD_ASSERT_ARG(tlv_data, WBDE_INV_ARG);
	WBD_ASSERT_ARG(tlv_data_len, WBDE_INV_ARG);

	pinbuf = (wbd_cmd_vndr_intf_chan_info_t*)data;
	/* Pointing to out data */
	pmem = tlv_data;

	/* bytes skiped for TLV Hdr size, fill the TLV Hdr in the end */
	pbuf = pmem + sizeof(i5_tlv_t);

	poutbuf = (wbd_cmd_vndr_intf_chan_info_t*)pbuf;

	memcpy(&poutbuf->mac, &pinbuf->mac, ETHER_ADDR_LEN);

	pbuf += ETHER_ADDR_LEN;
	poutbuf->band = pinbuf->band;

	pbuf += sizeof(pinbuf->band);

	WBD_DEBUG("Before encoding chan info: pbuf[%p] intf band[%d] MAC["MACF"]\n",
		pbuf, pinbuf->band, ETHERP_TO_MACF(&(poutbuf->mac)));

	chan_info = (wbd_interface_chan_info_t*)pbuf;

	chan_info->count = pinbuf->chan_info->count;

	WBD_DEBUG("chan info count [%d] \n", pinbuf->chan_info->count);

	for (i = 0; i < pinbuf->chan_info->count; i++) {
		chan_info->chinfo[i].channel = pinbuf->chan_info->chinfo[i].channel;
		chan_info->chinfo[i].bitmap = htons(pinbuf->chan_info->chinfo[i].bitmap);

		WBD_DEBUG("Encode Chan info: channel[%d] bitmap[%x] \n",
			pinbuf->chan_info->chinfo[i].channel, pinbuf->chan_info->chinfo[i].bitmap);
	}

	pbuf += sizeof(pinbuf->chan_info->count) +
		(pinbuf->chan_info->count * sizeof(wbd_chan_info_t));

	WBD_DEBUG("after encoding cha info :pbuf[%p] , chan info count[%d] per block size[%zu]\n",
		pbuf, chan_info->count, sizeof(wbd_chan_info_t));

	/* Fill the TLV Hdr now */
	ptlv = (i5_tlv_t *)pmem;
	ptlv->type = WBD_TLV_CHAN_INFO_TYPE;
	ptlv->length = htons(pbuf-pmem-sizeof(i5_tlv_t));

	/* Update out data length */
	*tlv_data_len = pbuf-pmem;
end:
	WBD_EXIT();
	return ret;
}

/* Decode Vendor Specific TLV for Message : chan info and list of supported chanspec */
int
wbd_tlv_decode_chan_info(void* data, unsigned char* tlv_data,
	unsigned short tlv_data_len)
{
	wbd_cmd_vndr_intf_chan_info_t *pmsg = NULL;
	wbd_interface_chan_info_t *pchan_info = NULL;
	wbd_chan_info_t chinfo;
	int ret = WBDE_OK;
	uint32 i = 0;
	uint8 *ptlv = NULL;
	uint8 *pbuf = NULL;
	uint8 chan_info_count = 0;

	WBD_ENTER();

	/* Validate args */
	WBD_ASSERT_ARG(tlv_data, WBDE_INV_ARG);
	WBD_ASSERT_ARG(tlv_data_len, WBDE_INV_ARG);
	memset(&chinfo, 0, sizeof(chinfo));

	pmsg = (wbd_cmd_vndr_intf_chan_info_t*)data;

	ptlv = tlv_data;
	ptlv += sizeof(i5_tlv_t);

	memcpy(&pmsg->mac, ptlv, sizeof(pmsg->mac));
	ptlv += ETHER_ADDR_LEN;

	pmsg->band = *ptlv++;

	chan_info_count = *ptlv++;

	pchan_info = (wbd_interface_chan_info_t *)wbd_malloc(
		sizeof(wbd_interface_chan_info_t), &ret);
	WBD_ASSERT();

	pchan_info->count = chan_info_count;
	WBD_DEBUG("decode: chan info count [%d] MAC["MACF"] \n", pchan_info->count,
		ETHERP_TO_MACF(&pmsg->mac));

	if (pchan_info->count > WBD_MAXCHANNEL) {
		WBD_ERROR("Invalid chan info count[%d], exit\n", pchan_info->count);
		ret = WBDE_TLV_ERROR;
		goto end;
	}

	pbuf = ptlv;
	for (i = 0; i < pchan_info->count; i++) {
		pchan_info->chinfo[i].channel = *pbuf++;
		pchan_info->chinfo[i].bitmap = ntohs(*(uint16*)pbuf);
		pbuf += sizeof(uint16);
	}
	/* debugging purpose */
	for (i = 0; i < pchan_info->count; i++) {
		/* only for debugging ... */
		WBD_DEBUG("Decode chan info: index[%d] channel[%d] bitmap[%x]\n",
			i, pchan_info->chinfo[i].channel, pchan_info->chinfo[i].bitmap);
	}

	ptlv += (pchan_info->count * sizeof(wbd_chan_info_t));

	pmsg->chan_info = pchan_info;

end:
	WBD_EXIT();
	if (ret != WBDE_OK) {
		/* release memory */
		if (pchan_info) {
			free(pchan_info);
			pmsg->chan_info = NULL;
		}
	}
	return ret;
}

/* Encode Vendor Specific DFS CHAN INFO TLV Message, list of common channels for all agents
 * to prepare same dfs channel forced list in single channel topology.
 */
int
wbd_tlv_encode_dfs_chan_info(void* data, unsigned char* tlv_data, unsigned int* tlv_data_len)
{
	wbd_cmd_vndr_controller_dfs_chan_info_t *pinbuf = NULL;
	wbd_cmd_vndr_controller_dfs_chan_info_t *poutbuf = NULL;
	wbd_dfs_chan_info_t *chan_info = NULL;
	int ret = WBDE_OK;
	unsigned char *pbuf, *pmem;
	i5_tlv_t *ptlv;
	uint32 i = 0;

	WBD_ENTER();

	/* Validate args */
	WBD_ASSERT_ARG(tlv_data, WBDE_INV_ARG);
	WBD_ASSERT_ARG(tlv_data_len, WBDE_INV_ARG);

	pinbuf = (wbd_cmd_vndr_controller_dfs_chan_info_t*)data;
	/* Pointing to out data */
	pmem = tlv_data;

	/* bytes skiped for TLV Hdr size, fill the TLV Hdr in the end */
	pbuf = pmem + sizeof(i5_tlv_t);

	poutbuf = (wbd_cmd_vndr_controller_dfs_chan_info_t*)pbuf;

	memcpy(&poutbuf->mac, &pinbuf->mac, ETHER_ADDR_LEN);

	pbuf += ETHER_ADDR_LEN;
	poutbuf->band = pinbuf->band;

	pbuf += sizeof(pinbuf->band);

	WBD_DEBUG("Before encoding chan info: pbuf[%p] intf band[%d] MAC["MACF"]\n",
		pbuf, pinbuf->band, ETHERP_TO_MACF(&(poutbuf->mac)));

	chan_info = (wbd_dfs_chan_info_t*)pbuf;

	chan_info->count = pinbuf->chan_info->count;
	pbuf += sizeof(pinbuf->chan_info->count);

	WBD_DEBUG("Encode dfs chan info, with count [%d] \n", pinbuf->chan_info->count);

	for (i = 0; i < pinbuf->chan_info->count; i++) {
		chan_info->channel[i] = pinbuf->chan_info->channel[i];
		WBD_DEBUG("Encode dfs chan[%d] \n", chan_info->channel[i]);
		pbuf++;
	}

	/* Fill the TLV Hdr now */
	ptlv = (i5_tlv_t *)pmem;
	ptlv->type = WBD_TLV_DFS_CHAN_INFO_TYPE;
	ptlv->length = htons(pbuf-pmem-sizeof(i5_tlv_t));

	WBD_DEBUG("after encoding pbuf[%p] \n", pbuf);
	/* Update out data length */
	*tlv_data_len = pbuf-pmem;
end:
	WBD_EXIT();
	return ret;
}

/* Decode Vendor Specific DFS chan info TLV from controller. */
int
wbd_tlv_decode_dfs_chan_info(void* data, unsigned char* tlv_data, unsigned short tlv_data_len)
{
	wbd_cmd_vndr_controller_dfs_chan_info_t *pmsg = NULL;
	wbd_dfs_chan_info_t *pchan_info = NULL;
	int ret = WBDE_OK;
	uint32 i = 0;
	uint8 *ptlv = NULL;
	uint8 *pbuf = NULL;
	uint8 chan_count = 0;

	WBD_ENTER();

	/* Validate args */
	WBD_ASSERT_ARG(tlv_data, WBDE_INV_ARG);
	WBD_ASSERT_ARG(tlv_data_len, WBDE_INV_ARG);

	pmsg = (wbd_cmd_vndr_controller_dfs_chan_info_t*)data;

	ptlv = tlv_data;
	ptlv += sizeof(i5_tlv_t);

	memcpy(&pmsg->mac, ptlv, sizeof(pmsg->mac));
	ptlv += ETHER_ADDR_LEN;

	pmsg->band = *ptlv++;

	chan_count = *ptlv++;

	if (chan_count == 0) {
		WBD_DEBUG("No channels in dfs chan info, exit \n");
		goto end;
	}

	pchan_info = (wbd_dfs_chan_info_t *)wbd_malloc(sizeof(wbd_dfs_chan_info_t), &ret);
	WBD_ASSERT();

	pchan_info->count = chan_count;
	pbuf = ptlv;

	for (i = 0; i < pchan_info->count; i++) {
		pchan_info->channel[i] = *pbuf++;
	}
	/* debugging purpose */
	WBD_DEBUG("Decode DFS chan info for count[%d] \n", pchan_info->count);
	for (i = 0; i < pchan_info->count; i++) {
		WBD_DEBUG("Decode DFS chan info: channel[%d] \n", pchan_info->channel[i]);
	}

	pmsg->chan_info = pchan_info;
end:
	WBD_EXIT();
	return ret;
}
