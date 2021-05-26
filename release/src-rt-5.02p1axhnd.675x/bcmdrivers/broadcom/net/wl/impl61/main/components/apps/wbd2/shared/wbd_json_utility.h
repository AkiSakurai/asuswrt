/*
 * WBD JSON format creation and parsing
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
 * $Id: wbd_json_utility.h 779283 2019-09-24 08:30:26Z $
 */

#ifndef _WBD_JSON_UTILITY_H_
#define _WBD_JSON_UTILITY_H_

#include <json.h>
#include "wbd.h"
#include "wbd_shared.h"

#define JSON_TAG_CMD			"Cmd"
#define JSON_TAG_BAND			"Band"
#define JSON_TAG_DATA			"Data"
#define JSON_TAG_BSSID			"BSSID"
#define JSON_TAG_SSID			"SSID"
#define JSON_TAG_MAC			"MAC"
#ifdef PLC_WBD
#define JSON_TAG_PLC_MAC		"PLCMAC"
#endif /* PLC_WBD */
#define JSON_TAG_ERRORCODE		"Error"
#define JSON_TAG_REASON			"Reason"
#define JSON_TAG_RSSI			"RSSI"
#define JSON_TAG_CLI_SUBCMD		"SubCmd"
#define JSON_TAG_INTERVAL		"Interval"
#define JSON_TAG_REPETITION		"Repetition"
#define JSON_TAG_PRIORITY		"Priority"
#define JSON_TAG_MACLIST		"MACList"
#define JSON_TAG_CLI_STA_PRIORITY	"StaPrio"
#define JSON_TAG_CHANSPEC		"Chanspec"
#define JSON_TAG_RCLASS			"RClass"
#define JSON_TAG_TX_FAILURES		"TxFailures"
#define JSON_TAG_TX_TOT_FAILURES	"TxTotFailures"
#define JSON_TAG_TX_RATE		"TxRate"
#define JSON_TAG_STATYPE		"STAType"
#define JSON_TAG_HOPCOUNT		"HopCount"
#define JSON_TAG_NVRAM			"Nvram"
#define JSON_TAG_DWELL			"Dwell"
#define JSON_TAG_SLAVE_TYPE		"SlaveType"
#define JSON_TAG_BRIDGE_TYPE		"BridgeType"
#define JSON_TAG_FLAGS			"Flags"
#define JSON_TAG_DFS_CHAN_LIST		"Dfschans"
#define JSON_TAG_TWOG			"TwoG"
#define JSON_TAG_FIVEG_LOW		"FiveGLow"
#define JSON_TAG_FIVEG_HIGH		"FiveGHigh"
#define JSON_TAG_STATUS			"Status"
#define JSON_TAG_UPLINK_MAC		"UplinkMAC"
#define JSON_TAG_CHANNEL		"Channel"
#define JSON_TAG_BITMAP			"Bitmap"
#define JSON_TAG_CHAN_INFO_LIST		"ChanInfoList"
#define JSON_TAG_CHANSPEC_LIST		"Chanspecs"
#define JSON_TAG_NSS			"Nss"
#define JSON_TAG_AVG_TX_RATE		"AvgTxRate"
#define JSON_TAG_CAPABILITY_LIST	"CapabilityList"
#define JSON_TAG_PREFIX			"Prefix"
#define JSON_TAG_IFNAME			"Ifname"
#define JSON_TAG_BRIDGE_ADDR		"BridgeAddr"
#define JSON_TAG_MASTER_LOGS		"MasterLogs"
#define JSON_TAG_TAF			"Taf"
#define JSON_TAG_TAF_STA		"StaTaf"
#define JSON_TAG_TAF_BSS		"BssTaf"
#define JSON_TAG_TAF_DEF		"defTaf"
#define JSON_TAG_TX_POWER		"TxPower"
#define JSON_TAG_BSSID_INFO		"BssidInfo"
#define JSON_TAG_PHYTYPE		"PhyType"
#define JSON_TAG_SRC_MAC		"SrcMAC"
#define JSON_TAG_DST_MAC		"DstMAC"
#define JSON_TAG_MAP_FLAGS		"MapFlags"
#define JSON_TAG_REP_RSSI		"ReportedRSSI"
#define JSON_TAG_DATA_RATE		"DataRate"
#define JSON_TAG_RX_TOT_PKTS		"RxTotPkts"
#define JSON_TAG_RX_TOT_BYTES		"RxTotBytes"
#define JSON_TAG_MSGLEVEL		"MsgLevel"
#define JSON_TAG_DISABLE		"Disable"
/* ------------------------------ HEALPER FUNCTIONS ------------------------------------- */

/* Get the command ID from the json data */
extern wbd_com_cmd_type_t wbd_json_parse_cmd_name(const void *data);

/* Get the CLI command ID from the json data */
extern wbd_com_cmd_type_t wbd_json_parse_cli_cmd_name(const void *data);

/* ------------------------------ HEALPER FUNCTIONS ------------------------------------- */

/* ------------------------------- CREATE FUNCTIONS ------------------------------------- */

/* Create CLI command request and get the JSON data in string format */
extern void* wbd_json_create_cli_cmd(void *data);

/* Creates common JSON response with message */
extern char* wbd_json_create_common_resp(wbd_cmd_param_t *cmdparam, int error_code);

/* Returns cli sata in json format for UI */
extern char* wbd_json_create_cli_info(wbd_info_t *info, wbd_cli_send_data_t *clidata);

/* Create cli logs in json format for UI */
extern char* wbd_json_create_cli_logs(wbd_info_t *info, wbd_cli_send_data_t *clidata);

/* ------------------------------- CREATE FUNCTIONS ------------------------------------- */

/* ------------------------------ PARSING FUNCTIONS ------------------------------------- */

/* Parse CLI command and fill the structure */
extern void* wbd_json_parse_cli_cmd(void *data);

/* Parse common JSON response and fill the structure */
extern void* wbd_json_parse_common_resp(void *data);

/* Parse WEAK_CLIENT_BSD command and fill the structure */
extern void* wbd_json_parse_weak_client_bsd_cmd(void *data);

/* Parse WEAK_CANCEL_BSD command and fill the structure */
extern void* wbd_json_parse_weak_cancel_bsd_cmd(void *data);

/* Parse STA_STATUS_BSD command and fill the structure */
extern void* wbd_json_parse_sta_status_bsd_cmd(void *data);

/* Parse BLOCK_CLIENT_BSD command and fill the structure */
extern void* wbd_json_parse_blk_client_bsd_cmd(void *data);
/* ------------------------------ PARSING FUNCTIONS ------------------------------------- */
#endif /* _WBD_JSON_UTILITY_H_ */
