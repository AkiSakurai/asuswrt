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
 * $Id: wbd_tlv.h 781083 2019-11-11 07:05:52Z $
 */

#ifndef _WBD_TLV_H_
#define _WBD_TLV_H_

/* Length of the TLVs */
#define WBD_MIN_LEN_TLV_FBT_CONFIG_REQUEST		1
#define WBD_MIN_LEN_TLV_FBT_CONFIG_RESP			1
#define WBD_MAX_LEN_TLV_ASSOC_STA_METRICS		26
#define WBD_MIN_LEN_TLV_METRIC_POLICY			1
#define WBD_MAX_LEN_TLV_METRIC_POLICY_ITEM		22
#define WBD_MAX_LEN_TLV_WEAK_CLIENT_RESP		17
#define WBD_MAX_LEN_TLV_BSS_CAPABILITY_QUERY		7
#define WBD_MIN_LEN_TLV_BSS_CAPABILITY_REPORT		7
#define WBD_MIN_LEN_TLV_BSS_METRICS_QUERY		1
#define WBD_MIN_LEN_TLV_BSS_METRICS_REPORT		1
#define WBD_LEN_TLV_STEER_RESP_REPORT			13
#define WBD_LEN_TLV_STEER_REQUEST			1
#define WBD_LEN_TLV_BH_STA_METRIC_POLICY		15
#define WBD_MIN_LEN_TLV_NVRAM_SET			3

/* Modify wbd_tlv_list_type_name when modifying this */
typedef enum wbd_tlv_types {
	WBD_TLV_FBT_CONFIG_REQ_TYPE			= 1,
	WBD_TLV_FBT_CONFIG_RESP_TYPE			= 2,
	WBD_TLV_VNDR_ASSOC_STA_METRICS_TYPE		= 3,
	WBD_TLV_VNDR_METRIC_POLICY_TYPE			= 4,
	WBD_TLV_WEAK_CLIENT_RESP_TYPE			= 5,
	WBD_TLV_REMOVE_CLIENT_REQ_TYPE			= 6,
	WBD_TLV_BSS_CAPABILITY_QUERY_TYPE		= 7,
	WBD_TLV_BSS_CAPABILITY_REPORT_TYPE		= 8,
	WBD_TLV_BSS_METRICS_QUERY_TYPE			= 9,
	WBD_TLV_BSS_METRICS_REPORT_TYPE			= 10,
	WBD_TLV_STEER_RESP_REPORT_TYPE			= 11,
	WBD_TLV_STEER_REQUEST_TYPE			= 12,
	WBD_TLV_ZWDFS_TYPE				= 13,
	WBD_TLV_BH_STA_METRIC_POLICY_TYPE		= 14,
	WBD_TLV_NVRAM_SET_TYPE				= 15,
	WBD_TLV_CHAN_SET_TYPE				= 16,
	WBD_TLV_GUEST_SSID_TYPE				= 17,
	WBD_TLV_CHAN_INFO_TYPE				= 18,
	WBD_TLV_DFS_CHAN_INFO_TYPE			= 19,
} wbd_tlv_types_t;

/* Associated STA Link Metrics Capability Flags */
/* STA Supports Dual Band
 * 0: Not supported
 * 1 : Supported
 */
#define WBD_TLV_ASSOC_STA_CAPS_FLAG_DUAL_BAND		0x80

/* Get TLV Type String from TLV Type Enum Value */
extern char const* wbd_tlv_get_type_str(int tlv_type);

/* Get Message String from 1905 Message Type which can support Vendor TLV */
extern char const* wbd_get_1905_msg_str(int msg_type);

#ifdef WLHOSTFBT
/* Encode Vendor Specific TLV for Message : FBT_CONFIG_REQ to send */
extern void wbd_tlv_encode_fbt_config_request(void* data,
	unsigned char* tlv_data, unsigned int* tlv_data_len);

/* Decode Vendor Specific TLV for Message : FBT_CONFIG_REQ on receive */
extern int wbd_tlv_decode_fbt_config_request(void* data,
	unsigned char* tlv_data, unsigned int tlv_data_len);

/* Encode Vendor Specific TLV for Message : FBT_CONFIG_RESP to send */
extern void wbd_tlv_encode_fbt_config_response(void* data,
	unsigned char* tlv_data, unsigned int* tlv_data_len);

/* Decode Vendor Specific TLV for Message : FBT_CONFIG_RESP on receive */
extern int wbd_tlv_decode_fbt_config_response(void* data,
	unsigned char* tlv_data, unsigned int tlv_data_len);

#endif /* WLHOSTFBT */

/* Encode Vendor Specific TLV : Associated STA Link Metrics Vendor Data to send */
extern void wbd_tlv_encode_vndr_assoc_sta_metrics(void* data,
	unsigned char* tlv_data, unsigned int* tlv_data_len);

/* Decode Vendor Specific TLV : Associated STA Link Metrics Vendor Data on receive */
extern int wbd_tlv_decode_vndr_assoc_sta_metrics(void* data,
	unsigned char* tlv_data, unsigned int tlv_data_len);

/* Encode Vendor Specific TLV : Metric Reporting Policy Vendor Data to send */
extern void wbd_tlv_encode_vndr_metric_policy(void* data,
	unsigned char* tlv_data, unsigned int* tlv_data_len);

/* Decode Vendor Specific TLV : Metric Reporting Policy Vendor Data on receive */
extern int wbd_tlv_decode_vndr_metric_policy(void* data,
	unsigned char* tlv_data, unsigned int tlv_data_len);

/* Encode Vendor Specific TLV for Message : weak client response */
extern int wbd_tlv_encode_weak_client_response(void* data, unsigned char* tlv_data,
	unsigned int* tlv_data_len);

/* Decode Vendor Specific TLV for Message : weak client response on recieve */
extern int wbd_tlv_decode_weak_client_response(void* data, unsigned char* tlv_data,
	unsigned int tlv_data_len);

/* Encode Vendor Specific TLV for Message : BSS capability query */
extern int wbd_tlv_encode_bss_capability_query(void* data, unsigned char* tlv_data,
	unsigned int* tlv_data_len);

/* Decode Vendor Specific TLV for Message : BSS capability query on recieve */
extern int wbd_tlv_decode_bss_capability_query(void* data, unsigned char* tlv_data,
	unsigned int tlv_data_len);

/* Encode Vendor Specific TLV for Message : BSS capability Report */
extern int wbd_tlv_encode_bss_capability_report(void* data, unsigned char* tlv_data,
	unsigned int* tlv_data_len);

/* Decode Vendor Specific TLV for Message : BSS capability report on recieve */
extern int wbd_tlv_decode_bss_capability_report(void* data, unsigned char* tlv_data,
	unsigned int tlv_data_len);

/* Encode Vendor Specific TLV for Message : REMOVE_CLIENT_REQ to send */
extern int wbd_tlv_encode_remove_client_request(void* data, unsigned char* tlv_data,
	unsigned int* tlv_data_len);

/* Decode Vendor Specific TLV for Message : REMOVE_CLIENT_REQ on recieve */
extern int wbd_tlv_decode_remove_client_request(void* data, unsigned char* tlv_data,
	unsigned int tlv_data_len);

/* Encode Vendor Specific TLV for Message : BSS metrics query */
extern int wbd_tlv_encode_bss_metrics_query(void* data, unsigned char* tlv_data,
	unsigned int* tlv_data_len);

/* Decode Vendor Specific TLV for Message : BSS metrics query on recieve */
extern int wbd_tlv_decode_bss_metrics_query(void* data, unsigned char* tlv_data,
	unsigned int tlv_data_len);

/* Encode Vendor Specific TLV for Message : BSS metrics Report */
extern int wbd_tlv_encode_bss_metrics_report(void* data, unsigned char* tlv_data,
	unsigned int* tlv_data_len);

/* Decode Vendor Specific TLV for Message : BSS metrics report on recieve */
extern int wbd_tlv_decode_bss_metrics_report(void* data, unsigned char* tlv_data,
	unsigned int tlv_data_len);

/* Encode Vendor Specific TLV for Message : Steer resp report */
int wbd_tlv_encode_steer_resp_report(void* data, unsigned char* tlv_data,
	unsigned int* tlv_data_len);

/* Decode Vendor Specific TLV for Message : Steer resp report on recieve */
int wbd_tlv_decode_steer_resp_report(void* data, unsigned char* tlv_data,
	unsigned int tlv_data_len);

/* Encode Vendor Specific TLV for Message : Vendor Steer Request */
int wbd_tlv_encode_vendor_steer_request(void* data, unsigned char* tlv_data,
	unsigned int* tlv_data_len);

/* Decode Vendor Specific TLV for Message : Vendor Steer Request on recieve */
int wbd_tlv_decode_vendor_steer_request(void* data, unsigned char* tlv_data,
	unsigned int tlv_data_len);

/* Encode Vendor Specific TLV for Message : Zero wait DFS */
int wbd_tlv_encode_zwdfs_msg(void* data, unsigned char* tlv_data, unsigned int* tlv_data_len);

/* Decode Vendor Specific TLV for Message : Zero wait DFS */
int wbd_tlv_decode_zwdfs_msg(void* data, unsigned char* tlv_data, unsigned int tlv_data_len);

/* Encode Vendor Specific TLV for Message : Backhaul STA Vendor Metric Reporting Policy Config */
int wbd_tlv_encode_backhaul_sta_metric_report_policy(void* data, unsigned char* tlv_data,
	unsigned int* tlv_data_len);

/* Decode Vendor Specific TLV for Message : Backhaul STA Vendor Metric Reporting Policy Config */
int wbd_tlv_decode_backhaul_sta_metric_report_policy(void* data, unsigned char* tlv_data,
	unsigned int tlv_data_len);

/* Encode Vendor Specific TLV for Message : NVRAM Set Vendor TLV */
int wbd_tlv_encode_nvram_set(void* data, unsigned char* tlv_data,
	unsigned int* tlv_data_len);

/* Decode Vendor Specific TLV for Message : NVRAM Set Vendor TLV */
int wbd_tlv_decode_nvram_set(void* data, unsigned char* tlv_data,
	unsigned int tlv_data_len);

/* Generic routine to create vendor message with specific TLV */
int wbd_tlv_encode_msg(void* data, unsigned int data_len, unsigned char tlv_type,
	unsigned char* tlv_data, unsigned int* tlv_data_len);

/* Encode Vendor Specific TLV for Message : chan info */
int wbd_tlv_encode_chan_info(void* data, unsigned char* tlv_data, unsigned int* tlv_data_len);

/* Decode Vendor Specific TLV for Message : chan info */
int wbd_tlv_decode_chan_info(void* data, unsigned char* tlv_data, unsigned short tlv_data_len);

/* Encode Vendor Specific DFS CHAN INFO TLV Message, list of common channels for all agents
 * to prepare same dfs channel forced list in single channel topology.
 */
int wbd_tlv_encode_dfs_chan_info(void* data, unsigned char* tlv_data, unsigned int* tlv_data_len);

/* Decode Vendor Specific DFS chan info TLV from controller. */
int wbd_tlv_decode_dfs_chan_info(void* data, unsigned char* tlv_data, unsigned short tlv_data_len);
#endif /* _WBD_TLV_H_ */
