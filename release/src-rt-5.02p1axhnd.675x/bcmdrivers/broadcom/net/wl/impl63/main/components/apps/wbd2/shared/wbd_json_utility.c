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
 * $Id: wbd_json_utility.c 779112 2019-09-19 10:14:55Z $
 */

#include "wbd_json_utility.h"
#include "wbd_ds.h"
#include "wbd_com.h"

/* buffer length needed for wl_format_ssid
 * 32 SSID chars, max of 4 chars for each SSID char "\xFF", plus NULL
 */
#define SSID_FMT_BUF_LEN (4*32+1)	/* Length for SSID format string */

#define WBD_MAX_CHANSPECBUF_LEN	5

#define JSON_SAFE_TOCKENER_PARSE(object_main, data, ERR, ret_val) \
		do { \
			object_main = json_tokener_parse(data); \
			if (object_main == NULL) { \
				WBD_WARNING("Main %s\n", wbderrorstr(ERR)); \
				return ret_val; \
			} \
		} while (0)

#define JSON_SAFE_GET_DATAOBJ(object_main, object_data, TAG, ERR) \
		do { \
			object_data = json_object_object_get(object_main, TAG); \
			if (object_data == NULL) { \
				WBD_WARNING("For Tag %s : Data %s\n", TAG, wbderrorstr(ERR)); \
				ret = ERR; \
				goto end; \
			} \
		} while (0)

/* ------------------------------ HEALPER FUNCTIONS ------------------------------------- */

/* Gets the int Value for the given tag name */
int
wbd_json_get_intval_fm_tag(json_object *object, char *tag)
{
	int ret = WBDE_OK;
	json_object *object_tag;
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(object, WBDE_JSON_NULL_OBJ);

	/* Get Data object */
	JSON_SAFE_GET_DATAOBJ(object, object_tag, tag, WBDE_JSON_NULL_OBJ);

	ret = json_object_get_int(object_tag);

end:
	WBD_EXIT();
	return ret;
}

/* Gets the double Value for the given tag name */
double
wbd_json_get_doubleval_fm_tag(json_object *object, char *tag)
{
	int ret = WBDE_OK;
	double val = 0.0;
	json_object *object_tag;
	WBD_ENTER();
	BCM_REFERENCE(ret);

	/* Validate arg */
	WBD_ASSERT_ARG(object, WBDE_JSON_NULL_OBJ);

	/* Get Data object */
	JSON_SAFE_GET_DATAOBJ(object, object_tag, tag, WBDE_JSON_NULL_OBJ);

	val = json_object_get_double(object_tag);

end:
	WBD_EXIT();
	return val;
}
/* Gets the String Value for the given tag name */
int
wbd_json_get_stringval_fm_tag(json_object *object, char *tag, char *buf, int buflen)
{
	int ret = WBDE_OK;
	const char *tmpval;
	json_object *object_tag;
	WBD_ENTER();

	memset(buf, 0, buflen);

	/* Validate arg */
	WBD_ASSERT_ARG(object, WBDE_JSON_NULL_OBJ);

	/* Get Data object */
	JSON_SAFE_GET_DATAOBJ(object, object_tag, tag, WBDE_JSON_NULL_OBJ);

	tmpval = json_object_get_string(object_tag);
	if (tmpval)
		snprintf(buf, buflen, "%s", tmpval);

	if (strlen(buf) <= 0) {
		WBD_JSON("%s for Tag[%s]\n", wbderrorstr(WBDE_JSON_INV_TAG_VAL), tag);
		ret = WBDE_JSON_INV_TAG_VAL;
		goto end;
	}

end:
	WBD_EXIT();
	return ret;
}

/* Gets the JSON string from JSON object and retruns the strrdup string */
static char*
wbd_json_get_jsonstring_fm_object(json_object **object, bool free_object)
{
	char* data = NULL;
	const char* strdata = NULL;
	WBD_ENTER();

	strdata = json_object_to_json_string(*object);

	if (strdata) {
		data = strdup(strdata);
		WBD_JSON("Data : %s\n", data);
	}
	if (free_object)
		json_object_put(*object);

	WBD_EXIT();
	return data;
}

/* Converts ether_addr structure to MAC string and adds it to object */
static void
wbd_json_add_mac_to_object(json_object **object, char* tag, struct ether_addr *mac)
{
	char tmpmac[ETHER_ADDR_LEN * 3] = {0};
	WBD_ENTER();

	snprintf(tmpmac, (ETHER_ADDR_LEN * 3), MACF, ETHERP_TO_MACF(mac));
	json_object_object_add(*object, tag, json_object_new_string(tmpmac));

	WBD_EXIT();
}

/* Get the ether_addr structure obj from JSON object & TAG */
static int
wbd_json_get_mac_fm_object(json_object *object, char *tag, struct ether_addr *mac)
{
	int ret = WBDE_OK;
	char tmpmac[ETHER_ADDR_LEN * 3] = {0};
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(object, WBDE_JSON_NULL_OBJ);

	/* Get the MAC string from the JSON object */
	ret = wbd_json_get_stringval_fm_tag(object, tag, tmpmac, sizeof(tmpmac));
	WBD_ASSERT();

	/* Get & Validate MAC */
	WBD_GET_VALID_MAC(tmpmac, mac, "", WBDE_JSON_INV_TAG_VAL);

end:
	WBD_EXIT();
	return ret;
}

/* Parses the common header for command name and MAC address */
static int
wbd_json_parse_cmd_header(json_object *object, wbd_cmd_param_t *param)
{
	int ret = WBDE_OK;
	char tmpname[WBD_MAX_CMD_LEN] = {0};
	WBD_ENTER();

	/* Get the command name from the JSON object */
	ret = wbd_json_get_stringval_fm_tag(object, JSON_TAG_CMD, tmpname, sizeof(tmpname));
	WBD_ASSERT();

	param->cmdtype = wbd_get_command_id(tmpname);

	/* Get the src MAC address from the JSON object */
	wbd_json_get_mac_fm_object(object, JSON_TAG_SRC_MAC, &param->src_mac);

	/* Get the dst MAC address from the JSON object */
	wbd_json_get_mac_fm_object(object, JSON_TAG_DST_MAC, &param->dst_mac);

	/* Get valid Band from the JSON object */
	param->band = wbd_json_get_intval_fm_tag(object, JSON_TAG_BAND);
	if (!WBD_BAND_VALID((param->band))) {
		ret = WBDE_JSON_INV_TAG_VAL;
	}
end:
	WBD_EXIT();
	return ret;
}

/* Get the command ID from the json data */
wbd_com_cmd_type_t
wbd_json_parse_cmd_name(const void *data)
{
	int ret = WBDE_OK;
	json_object *object_main;
	int cmdid = -1;
	char tmpname[WBD_MAX_CMD_LEN] = {0};
	WBD_ENTER();

	/* Load Main object fm string data */
	JSON_SAFE_TOCKENER_PARSE(object_main, data, WBDE_JSON_NULL_OBJ, cmdid);

	/* Get the command name from the JSON object */
	ret = wbd_json_get_stringval_fm_tag(object_main, JSON_TAG_CMD, tmpname, sizeof(tmpname));
	WBD_ASSERT();

	cmdid = wbd_get_command_id(tmpname);

end:
	json_object_put(object_main);
	WBD_EXIT();
	return cmdid;
}

/* Get the CLI command ID from the json data */
wbd_com_cmd_type_t
wbd_json_parse_cli_cmd_name(const void *data)
{
	int ret = WBDE_OK;
	json_object *object_main;
	int cmdid = -1;
	char tmpname[WBD_MAX_CMD_LEN] = {0};
	WBD_ENTER();

	/* Load Main object fm string data */
	JSON_SAFE_TOCKENER_PARSE(object_main, data, WBDE_JSON_NULL_OBJ, cmdid);

	/* Get the command name from the JSON object */
	ret = wbd_json_get_stringval_fm_tag(object_main, JSON_TAG_CMD, tmpname, sizeof(tmpname));
	WBD_ASSERT();

	cmdid = wbd_get_cli_command_id(tmpname);

end:
	json_object_put(object_main);
	WBD_EXIT();
	return cmdid;
}

/* ------------------------------ HEALPER FUNCTIONS ------------------------------------- */

/* ------------------------------- CREATE FUNCTIONS ------------------------------------- */

/* Create CLI command request and get the JSON data in string format */
void*
wbd_json_create_cli_cmd(void *data)
{
	json_object *object_main;
	wbd_cli_send_data_t *cmdcli = (wbd_cli_send_data_t*)data;
	WBD_ENTER();

	object_main = json_object_new_object();

	json_object_object_add(object_main, JSON_TAG_CMD,
		json_object_new_string(wbd_get_cli_command_name(cmdcli->cmd)));

	json_object_object_add(object_main, JSON_TAG_CLI_SUBCMD,
		json_object_new_string(wbd_get_cli_command_name(cmdcli->sub_cmd)));

	json_object_object_add(object_main, JSON_TAG_MAC, json_object_new_string(cmdcli->mac));

	json_object_object_add(object_main, JSON_TAG_BSSID, json_object_new_string(cmdcli->bssid));

	json_object_object_add(object_main, JSON_TAG_BAND, json_object_new_int(cmdcli->band));

	json_object_object_add(object_main, JSON_TAG_CLI_STA_PRIORITY,
		json_object_new_int(cmdcli->priority));

	json_object_object_add(object_main, JSON_TAG_FLAGS, json_object_new_int(cmdcli->flags));

	json_object_object_add(object_main, JSON_TAG_MSGLEVEL,
		json_object_new_int(cmdcli->msglevel));

	json_object_object_add(object_main, JSON_TAG_DISABLE, json_object_new_int(cmdcli->disable));

	WBD_EXIT();
	return wbd_json_get_jsonstring_fm_object(&object_main, TRUE);
}

/* Creates common JSON response with message */
char*
wbd_json_create_common_resp(wbd_cmd_param_t *cmdparam, int error_code)
{
	json_object *object_main;
	WBD_ENTER();

	object_main = json_object_new_object();

	json_object_object_add(object_main, JSON_TAG_CMD,
		json_object_new_string(wbd_get_command_name(cmdparam->cmdtype)));
	wbd_json_add_mac_to_object(&object_main, JSON_TAG_SRC_MAC, &cmdparam->src_mac);
	wbd_json_add_mac_to_object(&object_main, JSON_TAG_DST_MAC, &cmdparam->dst_mac);
	json_object_object_add(object_main, JSON_TAG_BAND, json_object_new_int(cmdparam->band));
	json_object_object_add(object_main, JSON_TAG_ERRORCODE, json_object_new_int(error_code));

	WBD_EXIT();
	return wbd_json_get_jsonstring_fm_object(&object_main, TRUE);
}

/* Crete JSON object from master info */
char*
wbd_json_create_cli_info(wbd_info_t *info, wbd_cli_send_data_t *clidata)
{
	int band;
	char *outdata = NULL, *ptr;
	wbd_assoc_sta_item_t *assoc_sta = NULL;
	json_object *bssobj, *staobj, *dataobj, *mainobj;
	json_object *arr_stalist, *arr_2G, *arr_5GL, *arr_5GH;
	i5_dm_network_topology_type *i5_topology = NULL;
	i5_dm_device_type *device = NULL;
	i5_dm_interface_type *ifr = NULL;
	i5_dm_bss_type *bss = NULL;
	i5_dm_clients_type *client = NULL;

	WBD_ENTER();

	/* json objects */
	arr_2G = json_object_new_array();
	arr_5GL = json_object_new_array();
	arr_5GH = json_object_new_array();
	dataobj = json_object_new_object();
	mainobj = json_object_new_object();

	/* Get the info data from i5 topology */
	i5_topology = ieee1905_get_datamodel();
	foreach_i5glist_item(device, i5_dm_device_type, i5_topology->device_list) {
		foreach_i5glist_item(ifr, i5_dm_interface_type, device->interface_list) {
			int wbd_band, bw = 0;
			char chan_info[WBD_MAX_BUF_32] = {0};
			/* Skip non-wireless or interfaces with zero bss count */
			if (!i5DmIsInterfaceWireless(ifr->MediaType) ||
				!ifr->BSSNumberOfEntries) {
				continue;
			}
			wbd_band = WBD_BAND_FROM_CHANNEL(CHSPEC_CHANNEL(ifr->chanspec));
			band = WBD_BAND_DIGIT(wbd_band);

			if (CHSPEC_IS20(ifr->chanspec)) {
				bw = 20;
			} else if (CHSPEC_IS40(ifr->chanspec)) {
				bw = 40;
			} else if (CHSPEC_IS80(ifr->chanspec)) {
				bw = 80;
			} else if (CHSPEC_IS160(ifr->chanspec)) {
				bw = 160;
			} else {
				bw = 10;
				continue;	/* Invalid Chanspec just continue */
			}
			snprintf(chan_info, sizeof(chan_info), "%d/%d",
				wf_chspec_ctlchan(ifr->chanspec), bw);

			foreach_i5glist_item(bss, i5_dm_bss_type, ifr->bss_list) {
				char map[WBD_MAX_BUF_32] = {0};
				/* Fill bss(band/bssid/stalist) json object */
				bssobj = json_object_new_object();

				json_object_object_add(bssobj, JSON_TAG_BAND,
					json_object_new_int(band));
				wbd_json_add_mac_to_object(&bssobj, JSON_TAG_BSSID,
					(struct ether_addr*)&bss->BSSID);
				json_object_object_add(bssobj, JSON_TAG_SLAVE_TYPE,
					json_object_new_string("Bss"));
				json_object_object_add(bssobj, JSON_TAG_CHANNEL,
					json_object_new_string(chan_info));
				if (bss->mapFlags > 0) {
					char *ptr = " ";
					if (I5_IS_BSS_FRONTHAUL(bss->mapFlags)) {
						ptr = "FH";
					} else if (I5_IS_BSS_BACKHAUL(bss->mapFlags)) {
						ptr = "BH";
					}
					snprintf(map, sizeof(map), "%s", ptr);
				}
				json_object_object_add(bssobj, JSON_TAG_MAP_FLAGS,
					json_object_new_string(map));
				arr_stalist = json_object_new_array();
				foreach_i5glist_item(client, i5_dm_clients_type,
					bss->client_list) {
					int rssi = 0;
					/* Fill sta(mac/rssi/status) object */
					staobj = json_object_new_object();

					wbd_json_add_mac_to_object(&staobj, JSON_TAG_MAC,
						(struct ether_addr*)&client->mac);
					if (client->link_metric.rcpi != 0) {
						rssi = WBD_RCPI_TO_RSSI(client->link_metric.rcpi);
					}
					json_object_object_add(staobj, JSON_TAG_RSSI,
						json_object_new_int(rssi));
					assoc_sta = (wbd_assoc_sta_item_t*)client->vndr_data;
					if (assoc_sta) {
						ptr = (assoc_sta->status != WBD_STA_STATUS_WEAK) ?
							" " : "Weak";
					} else {
						ptr = " ";
					}
					json_object_object_add(staobj, JSON_TAG_STATUS,
						json_object_new_string(ptr));

					json_object_array_add(arr_stalist, staobj);
				}
				/* Add sta list to bssobj */
				json_object_object_add(bssobj, JSON_TAG_MACLIST, arr_stalist);
				/* Add bssobj to the band specific list */
				json_object_array_add(((wbd_band & WBD_BAND_LAN_2G) ? arr_2G :
					((wbd_band & WBD_BAND_LAN_5GL) ? arr_5GL :
					arr_5GH)), bssobj);
			}
		}
	}

	/* Add both band slave objects to data object */
	json_object_object_add(dataobj, JSON_TAG_FIVEG_LOW, arr_5GL);
	json_object_object_add(dataobj, JSON_TAG_FIVEG_HIGH, arr_5GH);
	json_object_object_add(dataobj, JSON_TAG_TWOG, arr_2G);

	/* Add both cmd tag and data object to main  object */
	json_object_object_add(mainobj, JSON_TAG_CMD, json_object_new_string("info"));
	json_object_object_add(mainobj, JSON_TAG_DATA, dataobj);

	/* Get the string from main json object */
	outdata = wbd_json_get_jsonstring_fm_object(&mainobj, TRUE);

	WBD_EXIT();

	return outdata;
}

/* Creates master logs in JSON format to display in UI. */
char*
wbd_json_create_cli_logs(wbd_info_t *info, wbd_cli_send_data_t *clidata)
{
	char *outdata = NULL, *logsdata = NULL;
	int logslen = WBD_MAX_BUF_8192 * 4;
	int ret = WBDE_OK;
	json_object *mainobj, *dataobj;

	WBD_ENTER();

	BCM_REFERENCE(ret);

	/* Validate info arg */
	WBD_ASSERT_ARG(info, WBDE_INV_ARG);

	logsdata = (char*)wbd_malloc(logslen, &ret);
	WBD_ASSERT();

	/* Fetch the logs from the master. */
	(void)wbd_ds_get_logs_from_master(info->wbd_master, logsdata, logslen);

	/* Create data obj and add master logs to it. */
	dataobj = json_object_new_object();
	json_object_object_add(dataobj, JSON_TAG_MASTER_LOGS, json_object_new_string(logsdata));

	/* Create main obj and add data obj to it. */
	mainobj = json_object_new_object();
	json_object_object_add(mainobj, JSON_TAG_CMD, json_object_new_string("logs"));
	json_object_object_add(mainobj, JSON_TAG_DATA, dataobj);

	/* Create json string from main obj. */
	outdata = wbd_json_get_jsonstring_fm_object(&mainobj, TRUE);

	free(logsdata);
	logsdata = NULL;

end:
	WBD_EXIT();

	return outdata;
}

/* ------------------------------- CREATE FUNCTIONS ------------------------------------- */

/* ------------------------------ PARSING FUNCTIONS ------------------------------------- */

/* Parse CLI command and fill the structure */
void*
wbd_json_parse_cli_cmd(void *data)
{
	int ret = WBDE_OK;
	json_object *object_main;
	wbd_cli_send_data_t *clidata;
	char tmpname[WBD_MAX_CMD_LEN] = {0};
	WBD_ENTER();

	/* Load Main object fm string data */
	JSON_SAFE_TOCKENER_PARSE(object_main, data, WBDE_JSON_NULL_OBJ, NULL);

	clidata = (wbd_cli_send_data_t*)wbd_malloc(sizeof(*clidata), &ret);
	WBD_ASSERT();

	/* Get the command name from the JSON object */
	ret = wbd_json_get_stringval_fm_tag(object_main, JSON_TAG_CMD, tmpname, sizeof(tmpname));
	WBD_ASSERT();

	clidata->cmd = wbd_get_cli_command_id(tmpname);

	/* Get the sub command name from the JSON object */
	ret = wbd_json_get_stringval_fm_tag(object_main, JSON_TAG_CLI_SUBCMD,
		tmpname, sizeof(tmpname));
	WBD_ASSERT();

	clidata->sub_cmd = wbd_get_cli_command_id(tmpname);

	/* Get the MAC address from the JSON object */
	wbd_json_get_stringval_fm_tag(object_main, JSON_TAG_MAC, clidata->mac,
		sizeof(clidata->mac));

	/* Get the BSSID address from the JSON object */
	wbd_json_get_stringval_fm_tag(object_main, JSON_TAG_BSSID, clidata->bssid,
		sizeof(clidata->bssid));

	/* Get the Band from the JSON object */
	clidata->band = wbd_json_get_intval_fm_tag(object_main, JSON_TAG_BAND);

	clidata->priority = wbd_json_get_intval_fm_tag(object_main, JSON_TAG_CLI_STA_PRIORITY);
	clidata->chanspec = wbd_json_get_intval_fm_tag(object_main, JSON_TAG_CHANSPEC);
	clidata->flags |= wbd_json_get_intval_fm_tag(object_main, JSON_TAG_FLAGS);
	clidata->msglevel = wbd_json_get_intval_fm_tag(object_main, JSON_TAG_MSGLEVEL);
	clidata->disable = wbd_json_get_intval_fm_tag(object_main, JSON_TAG_DISABLE);
end:
	json_object_put(object_main);

	if (ret != WBDE_OK) {
		free(clidata);
		clidata = NULL;
	}

	WBD_EXIT();
	return (void*)clidata;
}

/* Parse common JSON response and fill the structure */
void*
wbd_json_parse_common_resp(void *data)
{
	int ret = WBDE_OK;
	json_object *object_main;
	wbd_cmd_common_resp_t *cmdresp;
	WBD_ENTER();

	/* Load Main object fm string data */
	JSON_SAFE_TOCKENER_PARSE(object_main, data, WBDE_JSON_NULL_OBJ, NULL);

	cmdresp = (wbd_cmd_common_resp_t*)wbd_malloc(sizeof(*cmdresp), &ret);
	WBD_ASSERT();

	/* Get command name and MAC address from header */
	wbd_json_parse_cmd_header(object_main, &cmdresp->cmdparam);

	/* Status JSON object */
	cmdresp->error_code = wbd_json_get_intval_fm_tag(object_main, JSON_TAG_ERRORCODE);

	WBD_JSON("Command %s from MAC["MACF"]. Error Code[%d]\n",
		wbd_get_command_name(cmdresp->cmdparam.cmdtype),
		ETHER_TO_MACF(cmdresp->cmdparam.src_mac), cmdresp->error_code);

end:
	json_object_put(object_main);
	WBD_EXIT();
	return (void*)cmdresp;
}

/* Parse WEAK_CLIENT_BSD command and fill the structure */
void*
wbd_json_parse_weak_client_bsd_cmd(void *data)
{
	int ret = WBDE_OK;
	json_object *object_main, *object_data;
	wbd_cmd_weak_client_bsd_t *cmdweakclient = NULL;
	WBD_ENTER();

	/* Load Main object fm string data */
	JSON_SAFE_TOCKENER_PARSE(object_main, data, WBDE_JSON_NULL_OBJ, NULL);

	/* Get Data object */
	JSON_SAFE_GET_DATAOBJ(object_main, object_data, JSON_TAG_DATA, WBDE_JSON_NULL_OBJ);

	/* Allocate WEAK_CLIENT_BSD structure */
	cmdweakclient = (wbd_cmd_weak_client_bsd_t*)wbd_malloc(sizeof(*cmdweakclient), &ret);
	WBD_ASSERT();

	/* Get command name and MAC address from header */
	wbd_json_parse_cmd_header(object_main, &cmdweakclient->cmdparam);

	wbd_json_get_mac_fm_object(object_data, JSON_TAG_MAC, &cmdweakclient->mac);

	/* RSSI JSON object */
	cmdweakclient->rssi = wbd_json_get_intval_fm_tag(object_data, JSON_TAG_RSSI);
	cmdweakclient->tx_rate = wbd_json_get_intval_fm_tag(object_data, JSON_TAG_TX_RATE);
	cmdweakclient->tx_failures = wbd_json_get_intval_fm_tag(object_data, JSON_TAG_TX_FAILURES);
	cmdweakclient->reported_rssi = wbd_json_get_intval_fm_tag(object_data, JSON_TAG_REP_RSSI);
	cmdweakclient->datarate = wbd_json_get_intval_fm_tag(object_data, JSON_TAG_DATA_RATE);
	cmdweakclient->rx_tot_pkts = wbd_json_get_intval_fm_tag(object_data, JSON_TAG_RX_TOT_PKTS);
	cmdweakclient->rx_tot_bytes = wbd_json_get_intval_fm_tag(object_data,
		JSON_TAG_RX_TOT_BYTES);
	cmdweakclient->tx_tot_failures = wbd_json_get_intval_fm_tag(object_data,
		JSON_TAG_TX_TOT_FAILURES);

	WBD_JSON("Command %s From["MACF"] STA MAC["MACF"] RSSI[%d] TxRate[%d] TxFailures[%d] "
		"ReportedRSSI[%d] DataRate[%d] RxTotPkts[%d] RxTotBytes[%d] TxTotFailures[%d]\n",
		wbd_get_command_name(cmdweakclient->cmdparam.cmdtype),
		ETHER_TO_MACF(cmdweakclient->cmdparam.src_mac), ETHER_TO_MACF(cmdweakclient->mac),
		cmdweakclient->rssi, cmdweakclient->tx_rate, cmdweakclient->tx_failures,
		cmdweakclient->reported_rssi, cmdweakclient->datarate, cmdweakclient->rx_tot_pkts,
		cmdweakclient->rx_tot_bytes, cmdweakclient->tx_tot_failures);
end:
	json_object_put(object_main);
	WBD_EXIT();
	return (void*)cmdweakclient;
}

/* Parse WEAK_CANCEL_BSD command and fill the structure */
void*
wbd_json_parse_weak_cancel_bsd_cmd(void *data)
{
	int ret = WBDE_OK;
	json_object *object_main, *object_data;
	wbd_cmd_weak_cancel_bsd_t *cmdweakcancel = NULL;
	WBD_ENTER();

	/* Load Main object fm string data */
	JSON_SAFE_TOCKENER_PARSE(object_main, data, WBDE_JSON_NULL_OBJ, NULL);

	/* Get Data object */
	JSON_SAFE_GET_DATAOBJ(object_main, object_data, JSON_TAG_DATA, WBDE_JSON_NULL_OBJ);

	/* Allocate WEAK_CANCEL_BSD structure */
	cmdweakcancel = (wbd_cmd_weak_cancel_bsd_t*)wbd_malloc(sizeof(*cmdweakcancel), &ret);
	WBD_ASSERT();

	/* Get command name and MAC address from header */
	wbd_json_parse_cmd_header(object_main, &cmdweakcancel->cmdparam);

	wbd_json_get_mac_fm_object(object_data, JSON_TAG_MAC, &cmdweakcancel->mac);
	cmdweakcancel->rssi = wbd_json_get_intval_fm_tag(object_data, JSON_TAG_RSSI);
	cmdweakcancel->tx_rate = wbd_json_get_intval_fm_tag(object_data, JSON_TAG_TX_RATE);
	cmdweakcancel->tx_failures = wbd_json_get_intval_fm_tag(object_data, JSON_TAG_TX_FAILURES);
	cmdweakcancel->reported_rssi = wbd_json_get_intval_fm_tag(object_data, JSON_TAG_REP_RSSI);
	cmdweakcancel->datarate = wbd_json_get_intval_fm_tag(object_data, JSON_TAG_DATA_RATE);
	cmdweakcancel->rx_tot_pkts = wbd_json_get_intval_fm_tag(object_data, JSON_TAG_RX_TOT_PKTS);
	cmdweakcancel->rx_tot_bytes = wbd_json_get_intval_fm_tag(object_data,
		JSON_TAG_RX_TOT_BYTES);
	cmdweakcancel->tx_tot_failures = wbd_json_get_intval_fm_tag(object_data,
		JSON_TAG_TX_TOT_FAILURES);

	WBD_JSON("Command %s From["MACF"] STA MAC["MACF"] RSSI[%d] TxRate[%d] TxFailures[%d] "
		"ReportedRSSI[%d] DataRate[%d] RxTotPkts[%d] RxTotBytes[%d] TxTotFailures[%d]\n",
		wbd_get_command_name(cmdweakcancel->cmdparam.cmdtype),
		ETHER_TO_MACF(cmdweakcancel->cmdparam.src_mac), ETHER_TO_MACF(cmdweakcancel->mac),
		cmdweakcancel->rssi, cmdweakcancel->tx_rate, cmdweakcancel->tx_failures,
		cmdweakcancel->reported_rssi, cmdweakcancel->datarate, cmdweakcancel->rx_tot_pkts,
		cmdweakcancel->rx_tot_bytes, cmdweakcancel->tx_tot_failures);
end:
	json_object_put(object_main);
	WBD_EXIT();
	return (void*)cmdweakcancel;
}

/* Parse STA_STATUS_BSD command and fill the structure */
void*
wbd_json_parse_sta_status_bsd_cmd(void *data)
{
	int ret = WBDE_OK;
	json_object *object_main, *object_data;
	wbd_cmd_sta_status_bsd_t *cmdstastatus = NULL;
	WBD_ENTER();

	/* Load Main object fm string data */
	JSON_SAFE_TOCKENER_PARSE(object_main, data, WBDE_JSON_NULL_OBJ, NULL);

	/* Get Data object */
	JSON_SAFE_GET_DATAOBJ(object_main, object_data, JSON_TAG_DATA, WBDE_JSON_NULL_OBJ);

	/* Allocate STA_STATUS_BSD structure */
	cmdstastatus = (wbd_cmd_sta_status_bsd_t*)wbd_malloc(sizeof(*cmdstastatus), &ret);
	WBD_ASSERT();

	/* Get command name and MAC address from header */
	wbd_json_parse_cmd_header(object_main, &cmdstastatus->cmdparam);

	wbd_json_get_mac_fm_object(object_data, JSON_TAG_MAC, &cmdstastatus->sta_mac);

	WBD_JSON("Command %s From["MACF"] STA MAC["MACF"]\n",
		wbd_get_command_name(cmdstastatus->cmdparam.cmdtype),
		ETHER_TO_MACF(cmdstastatus->cmdparam.src_mac),
		ETHER_TO_MACF(cmdstastatus->sta_mac));
end:
	json_object_put(object_main);
	WBD_EXIT();
	return (void*)cmdstastatus;
}

/* Parse BLOCK_CLIENT_BSD command and fill the structure */
void*
wbd_json_parse_blk_client_bsd_cmd(void *data)
{
	int ret = WBDE_OK;
	json_object *object_main, *object_data;
	wbd_cmd_block_client_t *cmdblkclient = NULL;
	WBD_ENTER();

	/* Load Main object fm string data */
	JSON_SAFE_TOCKENER_PARSE(object_main, data, WBDE_JSON_NULL_OBJ, NULL);

	/* Get Data object */
	JSON_SAFE_GET_DATAOBJ(object_main, object_data, JSON_TAG_DATA, WBDE_JSON_NULL_OBJ);

	/* Allocate BLOCK_CLIENT_BSD structure */
	cmdblkclient = (wbd_cmd_block_client_t*)wbd_malloc(sizeof(*cmdblkclient), &ret);
	WBD_ASSERT();

	/* Get command name, sta and tbss MAC address from header */
	wbd_json_parse_cmd_header(object_main, &cmdblkclient->cmdparam);

	wbd_json_get_mac_fm_object(object_data, JSON_TAG_MAC, &cmdblkclient->mac);

	wbd_json_get_mac_fm_object(object_data, JSON_TAG_BSSID, &cmdblkclient->tbss_mac);

	WBD_JSON("Command %s From["MACF"] STA MAC["MACF"] TBSS["MACF"]\n",
		wbd_get_command_name(cmdblkclient->cmdparam.cmdtype),
		ETHER_TO_MACF(cmdblkclient->cmdparam.src_mac), ETHER_TO_MACF(cmdblkclient->mac),
		ETHER_TO_MACF(cmdblkclient->tbss_mac));
end:
	json_object_put(object_main);
	WBD_EXIT();
	return (void*)cmdblkclient;
}
/* ------------------------------ PARSING FUNCTIONS ------------------------------------- */
