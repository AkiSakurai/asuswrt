/*
 * WBD Shared code between master and slave
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
 * $Id: wbd_shared.h 781159 2019-11-13 10:20:54Z $
 */

#ifndef _WBD_SHARED_H_
#define _WBD_SHARED_H_

#define WBD_MAX_BUF_64			64
#define WBD_MAX_REASON_LEN		256	/* Maximum length of reason */

#define MAX_VERSION_STR		10

#define CLI_CMD_I5_BSS_FMT \
"    BSSID: "MACDBG", SSID: %s, %s, %s\n"
#define CLI_CMD_I5_DEV_FMT "Device "MACDBG"    %s%s%s\n"
#define CLI_CMD_I5_ASSOCLIST_BSSID_FMT "    BSS: "MACDBG" Total STAs: %d\n"
#define CLI_CMD_I5_ASSOCLIST_DTL_FMT "        STA[%d]: "MACDBG" [%s] %s\n"

#define CLI_CMD_INFO_SLAVES_FMT \
"Slave %d: "MACF"\n\
\tBand(GHz)\t\t: %d\n\
\tChannel\t\t\t: %d\n\
\tBandWidth(MHz)\t\t: %d\n\
\tSSID\t\t\t: %s\n\
\tRSSI(dBm)\t\t: %d\n\
\tTx Rate(Mbps)\t\t: %d\n\
\tActive Since(Sec)\t: %d\n\
\tActive Before(Sec)\t: %d\n"

#define CLI_CMD_INFO_SLAVES_INFO "\nTotal Slaves in %dG %s Blanket : %d\n\n"
#define CLI_CMD_INFO_ASOCLIST_INFO "\tTotal Associated STAs\t: %d\n\n"
#define CLI_CMD_INFO_MONLIST_INFO \
"\tTotal Monitored STAs\t: %2d     MAC                      RSSI\n"

#define CLI_CMD_CLILST_CLIENTS_INFO_MTR "\nTotal STAs in %dG %s Blanket : %d\n\n"
#define CLI_CMD_CLILST_CLIENTS_FMT \
"STA %d: "MACF"\n\
\tBSSID\t\t\t: "MACF"\n\
\tRSSI\t\t\t: %d\n\
\tTx Rate(Mbps)\t\t: %0.2f\n\
\tTx Failures\t\t: %d\n\
\tStatus\t\t\t: %s\n"

#define CLI_CMD_CLILST_MONITOR_INFO \
"        Monitored STA[%d]:  "MACF"   Rssi: %d\n"
#define CLI_CMD_CLILST_MONITOR_FMT \
"\t\t         "MACF"        %d\n"

/* Cli logs message format. */
#define CLI_CMD_LOGS_ASSOC "%s Assoc: by "MACDBG" on "MACDBG"\n"
#define CLI_CMD_LOGS_DISASSOC "%s Disassoc: by "MACDBG" on "MACDBG"\n"
#define CLI_CMD_LOGS_WEAK "%s Weak STA: "MACDBG" on "MACDBG" RSSI=%d/%d TxRate=%d/%d TxFail=%d/%d\n"
#define CLI_CMD_LOGS_STEER "%s Steer: "MACDBG" from "MACDBG" to "MACDBG"\n"
#define CLI_CMD_LOGS_WEAK_CANCEL "%s Weak cancel: STA "MACF" on "MACF"\n"
#define CLI_CMD_LOGS_STEER_RESP "%s For STA "MACDBG" Steer response: %s[%d]\n"
#define CLI_CMD_LOGS_STEER_RESP_TBSS "%s For STA "MACDBG" Steer response: %s[%d] TBSS "MACDBG"\n"
#define CLI_CMD_LOGS_STEER_NO_RESP "%s Steer: No response form STA "MACDBG"\n"
#define CLI_CMD_LOGS_MAP_INIT_START "%s MAP Init Start: DeviceID "MACDBG"\n"
#define CLI_CMD_LOGS_MAP_INIT_END "%s MAP Init End: DeviceID "MACDBG"\n"
#define CLI_CMD_LOGS_FOUND_TBSS_ALGO_SOF "%s For WEEK_STA "MACDBG" Found Target TBSS "MACDBG" \
	by using SOF algorithm \n"
#define CLI_CMD_LOGS_FOUND_TBSS_ALGO_BEST_RSSI "%s For WEEK_STA "MACDBG" Found Target TBSS "MACDBG"\
	RSSI:[%d] by using Best RSSI algorithm\n"
#define CLI_CMD_LOGS_FOUND_TBSS_ALGO_NWS "%s WBD_TBSS: For WEEK STA["MACDBG"] on Node["MACDBG"]: by\
	using NWS algorithm : weight_flags = 0x%x, rssi = %d, n_rssi = %d, n_hops = %d, \
	tot_sta = %d, assoc_cnt = %d, n_sta_cnt = %d, phyrate = %d, n_phyrate = %d, bss_nss = %d, \
	max_nss = %d, n_nss = %d, slave_txrate = %d, max_txrate = %d, n_tx_rate = %d, score = %d\n"

/* CLI format flags String/JSON default is string */
#define WBD_CLI_FLAGS_STRING	0x01
#define WBD_CLI_FLAGS_JSON	0x02
#define WBD_CLI_CLEAR_LOGS	0x04	/* To clear master logs */

/* WiFi-Blanket related disassoc reason code(from REMOVE_CLIENT_REQ). If disassoc is from driver,
 * the driver will send its own reason codes
 */
#define WBD_DISASSOC_REASON_LOCAL	1001	/* Disassoc from WBD app */

/* Minimum delay between consecutive beacon requests (in ms) */
#define WBD_MIN_BCN_REQ_DELAY		400

/* Minimum delay between beacon metric query (in seconds) */
#define WBD_MIN_BCN_METRIC_QUERY_DELAY	16

/* Multiap oui length and offset */
#define MIN_MAP_IE_LEN				4 /* OUI[3] + OUI_TYEP[1] */
#define MAP_ATTR_VAL_OFFESET			2
#define MAP_IE_TYPE				0x1B

#define WBD_CHAN_INFO_BITMAP_MASK		0x0000FFFF	/* remove minute info */

/* Type of the command used for Master and Slave communication */
typedef enum wbd_cmd_type {
	WBD_CMD_WEAK_CLIENT_BSD_REQ = 0,
	WBD_CMD_WEAK_CANCEL_BSD_REQ,
	WBD_CMD_BLOCK_CLIENT_RESP,
	WBD_CMD_STA_STATUS_BSD_REQ,
	WBD_CMD_BLOCK_CLIENT_BSD_REQ
} wbd_cmd_type_t;

typedef enum wbd_cli_cmd_type {
	WBD_CMD_CLI_VERSION = 0,
	WBD_CMD_CLI_SLAVELIST,
	WBD_CMD_CLI_STEER,
	WBD_CMD_CLI_INFO,
	WBD_CMD_CLI_CLIENTLIST,
	WBD_CMD_CLI_MONITORLIST,
	WBD_CMD_CLI_MONITORADD,
	WBD_CMD_CLI_MONITORDEL,
	WBD_CMD_CLI_WEAK_CLIENT,
	WBD_CMD_CLI_WEAK_CANCEL,
	WBD_CMD_CLI_LOGS,
	WBD_CMD_CLI_MSGLEVEL,
	WBD_CMD_CLI_BHOPT
} wbd_cli_cmd_type_t;

/* Common parameters for all the commands */
typedef struct wbd_cmd_param {
	wbd_cmd_type_t cmdtype;			/* Type of the command */
	struct ether_addr src_mac;		/* MAC address of the source slave */
	int band;				/* Band of the msg sender */
	struct ether_addr dst_mac;		/* MAC address of the destination slave */
} wbd_cmd_param_t;

/* To hold the list of mac addresses */
typedef struct wbd_maclist {
	uint count;
	struct ether_addr mac[1]; /* List of mac addresses of the STA */
} wbd_maclist_t;

/* For command Get STA details */
typedef struct wbd_cmd_get_details {
	wbd_cmd_param_t cmdparam;
	wbd_maclist_t *maclist; /* list of mac address of the STA */
} wbd_cmd_get_details_t;

/* Information of the CLI command */
typedef struct wbd_cli_send_data {
	wbd_cli_cmd_type_t cmd;
	wbd_cli_cmd_type_t sub_cmd;
	char mac[ETHER_ADDR_LEN * 3];
	char bssid[ETHER_ADDR_LEN * 3];
	int band;
	int priority;
	chanspec_t chanspec;
	int flags;
	int msglevel;
	int disable;
} wbd_cli_send_data_t;

typedef struct wbd_cmd_common_resp {
	wbd_cmd_param_t cmdparam;
	int error_code;
} wbd_cmd_common_resp_t;

typedef struct wbd_cmd_weak_client_resp {
	wbd_cmd_param_t cmdparam;
	wbd_wc_resp_reason_code_t error_code;	/* Error code */
	struct ether_addr sta_mac;	/* MAC address of the STA */
	struct ether_addr BSSID;	/* BSSID of the BSS where STA is associated STA */
	int dwell_time;			/* no. of seconds the STA should be blocked from steering */
} wbd_cmd_weak_client_resp_t;

/* For command WEAK_CLIENT_BSD */
typedef struct wbd_cmd_weak_client_bsd {
	wbd_cmd_param_t cmdparam;
	struct ether_addr mac;	/* MAC address of the STA */
	int32 rssi;
	uint32 tx_rate;
	uint32 tx_failures;
	int32 reported_rssi;
	uint32 datarate;
	uint32 rx_tot_pkts;
	uint32 rx_tot_bytes;
	uint32 tx_tot_failures;
} wbd_cmd_weak_client_bsd_t;

/* For command WEAK_CANCEL_BSD */
typedef struct wbd_cmd_weak_cancel_bsd {
	wbd_cmd_param_t cmdparam;
	struct ether_addr mac;	/* MAC address of the STA */
	int32 rssi;
	uint32 tx_rate;
	uint32 tx_failures;
	int32 reported_rssi;
	uint32 datarate;
	uint32 rx_tot_pkts;
	uint32 rx_tot_bytes;
	uint32 tx_tot_failures;
} wbd_cmd_weak_cancel_bsd_t;

/* For command STA_STATUS_BSD */
typedef struct wbd_cmd_sta_status_bsd {
	wbd_cmd_param_t cmdparam;
	struct ether_addr sta_mac;	/* MAC address of the STA */
} wbd_cmd_sta_status_bsd_t;

/* For command Block_Client */
typedef struct wbd_cmd_block_client {
	wbd_cmd_param_t cmdparam;
	struct ether_addr mac;		/* MAC address of STA to be block */
	struct ether_addr tbss_mac;	/* MAC address of the target bss. */
} wbd_cmd_block_client_t;

/* For command REMOVE_CLIENT_REQ */
typedef struct wbd_cmd_remove_client {
	wbd_cmd_param_t cmdparam;
	struct ether_addr stamac;	/* MAC address of the associated STA */
	struct ether_addr bssid;	/* lastest BSSId of client  */
} wbd_cmd_remove_client_t;

/* For command STEER RESP */
typedef struct wbd_cmd_steer_resp_rpt {
	unsigned char bssid[MAC_ADDR_LEN];	/* BSSID of the BSS */
	unsigned char sta_mac[MAC_ADDR_LEN];	/* STA mac address */
	unsigned char status;			/* Status code */
} wbd_cmd_steer_resp_rpt_t;

/* For commands ASSOC_STA_METRIC_VNDR, actual STA_METRIC_VNDR data */
typedef struct wbd_assoc_sta_metric_vndr {
	unsigned int idle_rate;		/* data rate to measure STA is idle */
	unsigned int tx_failures;	/* tx failures */
	unsigned int tx_tot_failures;	/* tx total failures */
	unsigned char sta_cap;		/* STA Capability */
} wbd_assoc_sta_metric_vndr_t;

/* For commands VNDR_ASSOC_STA_METRICS */
typedef struct wbd_cmd_vndr_assoc_sta_metric {
	unsigned char sta_mac[MAC_ADDR_LEN];	/* MAC address of the associated STA */
	unsigned char num_bss;			/* Number of BSSIDs reported for this STA */
	unsigned char src_bssid[MAC_ADDR_LEN];	/* BSSID of BSS for which the STA is associated */

	wbd_assoc_sta_metric_vndr_t vndr_metrics; /* STA_METRIC_VNDR data */

} wbd_cmd_vndr_assoc_sta_metric_t;

/* Metric report policy configuration for a band */
typedef struct wbd_vndr_metric_rpt_policy {
	unsigned int t_idle_rate;	/* data rate threshold to measure STA is idle */
	unsigned int t_tx_rate;		/* Tx rate threshold in Mbps */
	unsigned int t_tx_failures;	/* threshold for tx retry */
	unsigned int flags;		/* WBD_WC_XX, Flags indicates which params to consider */
} wbd_vndr_metric_rpt_policy_t;

/* For command VNDR_METRIC_POLICY, IFR List entry */
typedef struct wbd_metric_policy_ifr_entry {
	dll_t node;	/* self referencial (next,prev) pointers of type dll_t */

	unsigned char ifr_mac[MAC_ADDR_LEN];	/* MAC of the Interface */
	wbd_vndr_metric_rpt_policy_t vndr_policy; /* Vendor Metric Reporting Policy Config */

} wbd_metric_policy_ifr_entry_t;

/* For command VNDR_METRIC_POLICY */
typedef struct wbd_cmd_vndr_metric_policy_config {
	int num_entries;	/* Number of wbd_metric_policy_ifr_entry_t type objects */
	wbd_glist_t entry_list; /* List of wbd_metric_policy_ifr_entry_t type objects */
} wbd_cmd_vndr_metric_policy_config_t;

/* Common input for TLV decode function which has info pointer */
typedef struct wbd_tlv_decode_cmn_data {
	wbd_info_t *info;
	void *data;
} wbd_tlv_decode_cmn_data_t;

/* zero wait dfs vendor message */
typedef struct wbd_cmd_vndr_zwdfs_msg {
	struct ether_addr mac;	/* issue on interface */
	wl_chan_change_reason_t reason;	/* CSA or DFS_AP_MOVE */
	uint8 cntrl_chan;	/* control channel */
	uint8 opclass;		/* regulatory class index from global regulatory class table */
} wbd_cmd_vndr_zwdfs_msg_t;

/* send channel selection request with vendor specific message */
typedef struct wbd_cmd_vndr_set_chan {
	struct ether_addr mac;
	uint8 cntrl_chan; /* use when append_vndr_tlv_to_chan_select_rqst is set */
	uint8 rclass; /* use when append_vndr_tlv_to_chan_select_rqst is set */
} wbd_cmd_vndr_set_chan_t;

/* NVRAM name and value pair */
typedef struct wbd_cmd_vndr_nvram {
	char name[WBD_MAX_BUF_128];			/* Name of the NVRAM */
	char value[WBD_MAX_BUF_256];			/* Value of the NVRAM */
} wbd_cmd_vndr_nvram_t;

/* All the NVRAMs with prefix */
typedef struct wbd_cmd_vndr_prefix_nvram {
	uint8 mac[MAC_ADDR_LEN];			/* MAC address of interface/BSS */
	uint8 n_nvrams;					/* Number interface/BSS NVRAMs */
	wbd_cmd_vndr_nvram_t *nvrams;			/* List of NVRAMs */
} wbd_cmd_vndr_prefix_nvram_t;

/* Common input for NVRAM set vendor TLV */
typedef struct wbd_cmd_vndr_nvram_set {
	uint8 n_common_nvrams;				/* Number of NVRAMs without prefix */
	wbd_cmd_vndr_nvram_t *common_nvrams;		/* List of NVRAMs withotut prefix */
	uint8 n_radios;					/* Number of Radios */
	wbd_cmd_vndr_prefix_nvram_t *radio_nvrams;	/* List of Radios and NVRAMs */
	uint8 n_bsss;					/* Number of BSSs */
	wbd_cmd_vndr_prefix_nvram_t *bss_nvrams;	/* List of BSSs and NVRAMs */
} wbd_cmd_vndr_nvram_set_t;

/* interface specific chan info */
typedef struct wbd_cmd_vndr_intf_chan_info {
	struct ether_addr mac;	/* interface mac */
	uint8 band;
	wbd_interface_chan_info_t *chan_info; /* list of channels with bitmap */
} wbd_cmd_vndr_intf_chan_info_t;

/* 5g specific Controller's chan info for all agent's interface in same band
 * running in single chan mode
 */
typedef struct wbd_cmd_vndr_controller_5g_chan_info {
	struct ether_addr mac;	/* interface mac */
	uint8 band;
	wbd_dfs_chan_info_t *chan_info;
} wbd_cmd_vndr_controller_dfs_chan_info_t;

/* Gets the command ID from the command name */
extern wbd_cmd_type_t wbd_get_command_id(const char *cmd);

/* Gets the command name from the command ID */
extern const char* wbd_get_command_name(const wbd_cmd_type_t id);

/* Gets the CLI command ID from the command name */
extern wbd_cli_cmd_type_t wbd_get_cli_command_id(const char *cmd);

/* Gets the CLI command name from the command ID */
extern const char* wbd_get_cli_command_name(const wbd_cli_cmd_type_t id);

/* Try to open the server FD's till it succeeds */
extern int wbd_try_open_server_fd(int portno, int* error);

/* Try to open the Event FD's till it succeeds */
extern int wbd_try_open_event_fd(int portno, int* error);

/* Parse common cli arguments for mastre and slave apps */
extern void wbd_parse_cli_args(int argc, char *argv[]);

/* Generic memory Allocation function for WBD app */
extern void* wbd_malloc(unsigned int len, int *error);

/* Allocate & Initialize the info structure */
extern wbd_info_t* wbd_info_init(int *error);

/* Cleanup all the sockets and allocated memory */
extern void wbd_info_cleanup(wbd_info_t *info);

/* Main loop which keeps on checking for the timers and fd's */
extern void wbd_run(bcm_usched_handle *hdl);

/* Add FD to micro scheduler */
extern int wbd_add_fd_to_schedule(bcm_usched_handle *hdl,
	int fd, void *arg, bcm_usched_fdscbfn *cbfn);

/* Add timers to micro scheduler */
extern int wbd_add_timers(bcm_usched_handle *hdl,
	void *arg, unsigned long long timeout, bcm_usched_timerscbfn *cbfn, int repeat_flag);

/* Remove timers fm micro scheduler */
extern int wbd_remove_timers(bcm_usched_handle *hdl, bcm_usched_timerscbfn *cbfn, void *arg);

/* Gets common config settings for mastre and slave */
extern int wbd_retrieve_common_config(wbd_info_t *info);

/* Gets list of mac list from NVRAM and store it in glist */
extern int wbd_retrieve_maclist_from_nvram(const char *nvramval, wbd_glist_t *list);

/* To get the digit of Bridge in which Interface belongs */
extern int wbd_get_ifname_bridge(char* ifname, int *out_bridge);

/* Gets WBD Band Enumeration from ifname's Chanspec & Bridge Type */
extern int wbd_identify_wbd_band_type(int bridge_dgt, chanspec_t ifr_chanspec, int *out_band);

/* Processes the Version CLI command */
extern void wbd_process_version_cli_cmd(wbd_com_handle *hndl, int childfd,
	void *cmddata, void *arg);

/* Processes the Set Message Level command */
extern void wbd_process_set_msglevel_cli_cmd(wbd_com_handle *hndl, int childfd,
	void *cmddata, void *arg);

/* Formats the SSID */
extern int wbd_wl_format_ssid(char* ssid_buf, uint8* ssid, int ssid_len);

/* strcat function with relloc buffer functionality in-built */
extern int wbd_strcat_with_realloc_buffer(char **outdata, int* outlen,
	int *totlen, int len_extend, char *tmpline);

/* Get output buffer for Monitored STA Item */
extern int wbd_snprintf_monitor_sta_info(wbd_monitor_sta_item_t *sta_item, char* format,
	char **outdata, int* outlen, int *totlen, int len_extend, int index);

/* Get output buffer for I5 Device */
int wbd_snprintf_i5_device(i5_dm_device_type *ifr, char *format, char **outdata, int *outlen,
	int *totlen, int len_extend);

/* Get output buffer for I5 BSS */
int wbd_snprintf_i5_bss(i5_dm_bss_type *bss, char *format, char **outdata, int *outlen,
	int *totlen, int len_extend, bool include_clients, bool include_monitored_stas);

/* Get output buffer for I5 Clients Item */
int wbd_snprintf_i5_clients(i5_dm_bss_type *bss, char **outdata, int *outlen, int *totlen,
	int len_entended, bool is_bssid_req);

/* Get Bridge MAC */
extern int wbd_get_bridge_mac(struct ether_addr* addr);

/* Concatinate list by removing duplicates */
extern void wbd_concat_list(char *list1, char *list2, char *out_list, int out_sz);

/* Function to display tty output. */
extern void wbd_tty_hdlr(char *file);

/* Checks whether FBT is enabled or not from NVRAMs */
extern int wbd_is_fbt_nvram_enabled(char *prefix);

#ifdef WLHOSTFBT
/* Add a FBT BSS item in a Slave's FBT Request List */
extern int wbd_add_item_in_fbt_cmdlist(wbd_fbt_bss_entry_t *in_fbt_bss,
	wbd_cmd_fbt_config_t *fbt_config, wbd_fbt_bss_entry_t **out_fbt_bss);
/* Find matching fbt_info for this bss */
extern wbd_fbt_bss_entry_t* wbd_find_fbt_bss_item_for_bssid(unsigned char *bssid,
	wbd_cmd_fbt_config_t *fbt_config);
#endif /* WLHOSTFBT */

/* Get IEEE1905_MAP_FLAG_XXX flag value for a interface */
unsigned char wbd_get_map_flags(char *prefix);

/* Add a Metric Policy item in a Metric Policy List */
extern int wbd_add_item_in_metric_policylist(wbd_metric_policy_ifr_entry_t *in_policy_ifr,
	wbd_cmd_vndr_metric_policy_config_t *policy_config,
	wbd_metric_policy_ifr_entry_t **out_policy_ifr);

/* Find matching Metric Policy for this radio */
extern wbd_metric_policy_ifr_entry_t *
wbd_find_vndr_metric_policy_for_radio(unsigned char *in_ifr_mac,
	wbd_cmd_vndr_metric_policy_config_t *policy_config);

/* Convert WBD error code to weak client response reason code */
wbd_wc_resp_reason_code_t wbd_error_to_wc_resp_reason_code(int error_code);

/* Convert weak client response reason code to WBD error code */
int wbd_wc_resp_reason_code_to_error(wbd_wc_resp_reason_code_t error_code);

/** @brief Initialize the IEEE1905 module.
 *
 * @param info			Application Handle
 * @param supServiceFlag	Flag to indicate supported service(Controller and/or Agent)
 *              of type BLANKET_FLAG_SUPP_XXX
 * @param isRegistrar		1 If it is registrar
 * @param cbs			Callbacks to be registered
 *
 * @return			status of the call. 0 Success. Non Zero Failure
 */
int ieee1905_module_init(wbd_info_t *info, unsigned int supServiceFlag, int isRegistrar,
	ieee1905_call_bks_t *cbs);

/** @brief De-Initialize the blanket.
 *
 * @return		Void.
 */
void blanket_deinit();

/** @brief Start MultiAP Messaging.
 *
 * @return		Void.
 */
void blanket_start_multiap_messaging();

/* Gets NVRAM settings for backhaul */
extern int wbd_retrieve_backhaul_nvram_config(wbd_weak_sta_policy_t *metric_policy);

/* Free memory allocated for NVRAM set vendor specific command */
extern void wbd_free_nvram_sets(wbd_cmd_vndr_nvram_set_t *cmd_nvram_set);

/* check for overlap between the passed channel arguments */
extern bool wbd_check_chanspec_for_overlap(chanspec_t cur_chspec, chanspec_t candi_chspec);

/* Check whether the current channel from chan info is good channel to consider or not
 */
extern bool wbd_check_chan_good_candidate(int sub_channel, wbd_dfs_chan_info_t* chan_info);

/* debug: print hex bytes of message */
extern void prhex(const char *msg, const uchar *buf, uint nbytes);
#endif /* _WBD_SHARED_H_ */
