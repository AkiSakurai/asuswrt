/*
 * WBD (WiFi Blanket Daemon) shared include file
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
 * $Id: wbd.h 782272 2019-12-16 08:41:14Z $
 */

#ifndef _WBD_H_
#define _WBD_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>

#include <shutils.h>
#include <bcmnvram.h>
#include <bcmendian.h>
#include <ethernet.h>
#include <wlioctl.h>
#include <security_ipc.h>

#include "wbd_error.h"
#include "bcm_usched.h"
#include "bcm_stamon.h"
#include "wbd_com.h"
#include "wbd_ds.h"

/* MulitAP Modes */
#define MAP_MODE_FLAG_DISABLED		0x0000	/* Disabled */
#define MAP_MODE_FLAG_CONTROLLER	0x0001	/* Controller */
#define MAP_MODE_FLAG_AGENT		0x0002	/* Agent */

#define MAP_IS_DISABLED(mode)	((mode) <= MAP_MODE_FLAG_DISABLED)	/* Is Disabled */
#define MAP_IS_CONTROLLER(mode)	((mode) & MAP_MODE_FLAG_CONTROLLER)	/* Is Controller */
#define MAP_IS_AGENT(mode)	((mode) & MAP_MODE_FLAG_AGENT)		/* Is Agent */

/* Macros for NSS value */
#define WBD_NSS_2		2
#define WBD_NSS_3		3
#define WBD_NSS_4		4
#define WBD_NSS_8		8

/* Macros to handle endianess between dongle and host. */
/* Global variable to indicate endianess, set by wl_endian_probe on wireless interface */
extern bool gg_swap;
#define htod64(i) (gg_swap ? bcmswap64(i) : (uint64)(i))
#define htod32(i) (gg_swap ? bcmswap32(i) : (uint32)(i))
#define htod16(i) (gg_swap ? bcmswap16(i) : (uint16)(i))
#define dtoh64(i) (gg_swap ? bcmswap64(i) : (uint64)(i))
#define dtoh32(i) (gg_swap ? bcmswap32(i) : (uint32)(i))
#define dtoh16(i) (gg_swap ? bcmswap16(i) : (uint16)(i))
#define htodchanspec(i) (gg_swap ? htod16(i) : i)
#define dtohchanspec(i) (gg_swap ? dtoh16(i) : i)
#define htodenum(i) (gg_swap ? ((sizeof(i) == 4) ? htod32(i) :\
	((sizeof(i) == 2) ? htod16(i) : i)) : i)
#define dtohenum(i) (gg_swap ? ((sizeof(i) == 4) ? dtoh32(i) :\
	((sizeof(i) == 2) ? dtoh16(i) : i)) : i)

/* ---------------------------------- Constant Declarations ------------------------------------ */
/* Hostapd and wpa_supplicant control dir path */
#define WBD_HAPD_SUPP_CTRL_DIR		"/var/run"

/* Wi-Fi Blanket tty file */
#define WBD_MASTER_FILE_TTY            "/tmp/wbd_master_tty.info"
#define WBD_SLAVE_FILE_TTY             "/tmp/wbd_slave_tty.info"

/* Wi-Fi Blanket Application Process info */
#define WBD_MASTER_PROCESS		"wbd_master"
#define WBD_MASTER_PROCESS_NAME1	"/usr/sbin/wbd_master"
#define WBD_MASTER_PROCESS_NAME2	"/bin/wbd_master"
#define WBD_SLAVE_PROCESS		"wbd_slave"
#define WBD_SLAVE_PROCESS_NAME1		"/usr/sbin/wbd_slave"
#define WBD_SLAVE_PROCESS_NAME2		"/bin/wbd_slave"

/* Wi-Fi Blanket Application Version */
#define WBD_VERSION			2

/* WiFi Blanket Bit flags for WBD_INFO structure. */
#define WBD_INFO_FLAGS_RC_RESTART	0x0001	/* Flag to Trigger rc restart */
#define WBD_INFO_FLAGS_IS_RC_RESTART_TM	0x0002	/* Is rc restart timer created */
#define WBD_INFO_FLAGS_STA_WNM_CHECK	0x0004	/* check wnm cap of sta while steering */
#define WBD_INFO_FLAGS_MCHAN_SLAVE	0x0008	/* Multi channel supported */
#define WBD_INFO_FLAGS_SMART_CONV_RCPI	0x0010	/* beacon report - smart rcpi to rssi converion? */
#define WBD_INFO_FLAGS_BAND_STEER	0x0020	/* Local Band steering is allowed or not */
#define WBD_INFO_FLAGS_ALL_AP_CFGRED_TM	0x0040	/* Is all_ap_configured timer created */
#define WBD_INFO_FLAGS_REMOVE_CLIENT	0x0080	/* remove_client required ? */
#define WBD_INFO_FLAGS_UAP		0x0100	/* Upstream AP */
#define WBD_INFO_FLAGS_PER_CHAN_BCN_REQ	0x0200	/* Beacon request should be sent on each channel */
#define WBD_INFO_FLAGS_IEEE1905_INIT	0x0400	/* IEEE1905 Module initialized or not */
#define WBD_INFO_FLAGS_CLOSING_APP	0x0800	/* Closing the application */
#define WBD_INFO_FLAGS_IS_HAPD_ENABLED	0x1000	/* Whether hostapd is enabled or not */

/* WiFi Blanket Bit flags for wbd_blanket_slave */
#define WBD_BKT_SLV_FLAGS_BH_WD_WEAK_CLIENT	0x0001	/* Flag to check if the backhaul STA weak
							 * client watchdog created or not
							 */
#define WBD_BKT_SLV_FLAGS_FBT_REQ_SENT		0x0008	/* FBT request sent to controller */

/* Check whether WNM STA cap check is enabled or not */
#define WBD_WNM_CHECK_ENAB(flags)	((flags) & (WBD_INFO_FLAGS_STA_WNM_CHECK))
/* Check whether Multi channel is enabled or not */
#define WBD_MCHAN_ENAB(flags)		((flags) & (WBD_INFO_FLAGS_MCHAN_SLAVE))
/* Check whether smart rcpi to rssi converion is enabled or not */
#define WBD_SMART_RCPI_CONV_ENAB(flags)	((flags) & (WBD_INFO_FLAGS_SMART_CONV_RCPI))
/* Check whether Local Band steering is enabled or not */
#define WBD_BAND_STEER_ENAB(flags)	((flags) & (WBD_INFO_FLAGS_BAND_STEER))
/* Check whether remove_client is enabled or not */
#define WBD_RMCLIENT_ENAB(flags)	((flags) & (WBD_INFO_FLAGS_REMOVE_CLIENT))
/* Check whether the device is upstream AP or not */
#define WBD_UAP_ENAB(flags)		((flags) & (WBD_INFO_FLAGS_UAP))
/* Check if beacon request should be sent per channel */
#define WBD_PER_CHAN_BCN_REQ(flags)	((flags) & (WBD_INFO_FLAGS_PER_CHAN_BCN_REQ))
/* Check whether IEEE1905 module is initialized or not */
#define WBD_IEEE1905_INIT(flags)	((flags) & (WBD_INFO_FLAGS_IEEE1905_INIT))
/* Check whether application is marked to be closed */
#define WBD_CLOSING_APP(flags)		((flags) & (WBD_INFO_FLAGS_CLOSING_APP))
/* Check whether hostapd is enabled or not */
#define WBD_IS_HAPD_ENABLED(flags)	((flags) & (WBD_INFO_FLAGS_IS_HAPD_ENABLED))

/* Wi-Fi Blanket Timeouts - Default Values */
#define WBD_TM_SOCKET			10	/* Timeout for Socket's read and write */
#define WBD_TM_SLV_BLOCK_STA		10	/* Timeout for Slave to Unblock a Steering STA */
#define WBD_TM_SLV_WD_WEAK_CLIENT	5	/* Timeout for Slave to run Weak Client Watchdog */
#define WBD_TM_SLV_WD_TARGET_BSS	4	/* Timeout for Slave to run Target BSS Watchdog */
#define WBD_TM_SLV_WD_BCN_RSSI		10	/* Timeout for RSSI received in beacon report */
#define WBD_TM_SLV_OFFCHAN_AF_PERIOD	1100	/* Time period to send actframe to offchannel
						 * weak STA (in ms)
						 */
#define WBD_TM_SLV_OFFCHAN_AF_INTERVAL	10	/* Off-channel actframe interval (in ms) */
#define WBD_TM_SLV_BCN_REQ_INTERVAL	16000	/* Send beacon request to weak STA (in ms) */
#define WBD_TM_MTR_RM_CLIENT_INTERVAL	1	/* Timeout to Remove stale client entry */
#define WBD_TM_MTR_RTRY_RM_CLIENT	30	/* Timeout to Retry the Remove stale client entry */
#define WBD_TM_RC_RESTART		5	/* Timeout for Both to do rc restart gracefully */
#define WBD_TM_STEER_RETRY_INTRVL	4	/* Interval for retrying the STEER */
#define WBD_TM_MAP_MONITOR_READ		3	/* Interval for reading monitored RSSI */
#define WBD_TM_MAP_SEND_BCN_REPORT	1000	/* Timeout for sending beacon report (in ms) */
#define WBD_TM_AUTOCONFIG_RENEW		6	/* Timeout for sending AP autoconfiguration Renew */
#define WBD_TM_UPDATE_CHANSPEC		10	/* Timeout for slave to update chanspec */
#define WBD_TM_CHANNEL_SELECT		10	/* Timeout for sending channel preference query */
#define WBD_TM_PROBE			3600	/* Timeout to remove probe STA entry */
#define WBD_TM_BH_WD_WEAK_CLIENT	30	/* Timeout to run backhaul STA weak client
						 * watchdog
						 */
#define WBD_TM_SLV_WD_BH_TARGET_BSS	30	/* Timeout to run Target BSS Watchdog for Backhaul
						 * STA
						 */
#define WBD_TM_SLV_WD_BH_OPT_TARGET_BSS	20	/* Timeout to run Target BSS Watchdog for Backhaul
						 * Optimization
						 */
#define WBD_TM_BH_OPT			10	/* Tiemout to run the backhaul optimization */

/* Wi-Fi Blanket default NVRAM values */
#define WBD_DEF_STA_WNM_CHECK		0	/* Default for WNM STA cap check */
#define WBD_DEF_MCHAN_SLAVE		0	/* Default for Multi channel */
#define WBD_DEF_SMART_CONV_RCPI		1	/* Default for rcpi to rssi smart converion */
/* To estimate rssi of a radio based another radio's rssi in a different band,
 * we should consider the factors that affect signal strength and that are
 * frequency dependent. One such factor is Free space path loss (FSPL). FSPL is
 * the loss in signal strength that occurs when an electromagnetic wave travels
 * over a line of sight path in free space.
 *              FSPL(dB) = 20 log(d) + 20 log (f) + 32.44
 * Where d is distance and f is frequency.
 * Since distance is same for both radio's under consideration, difference in
 * signal strength = 20 log (f1) - 20 log (f2). f1 and f2 are the frequencies
 * in which the radios are operating. So as per the equation, channel in the 2G
 * band has better signal strength which varies in the range of 6.3 and 7.7dB.
 * For ease of calculation, assuming it as 7.
 *  XXX: FSPL does not hold for most terrestrial situations because
 *  of other effects from the ground, objects in the path and the like
 */
#define WBD_DEF_CROSSBAND_RSSI_EST	7
#define WBD_DEF_RSSI_MAX		-10	/* Default maximum value for rssi */
#define WBD_DEF_RSSI_MIN		-95	/* Default minimum value for rssi */
#define WBD_DEF_BAND_STEER		1	/* Default for Local Band steering */
#define WBD_DEF_REMOVE_CLIENT		1	/* Default for remove client */
#define WBD_STEER_RETRY_COUNT		5	/* Default STEER Retry limit */
#define WBD_DEF_CHAN_UTIL_THLD_2G	0	/* Default Channel Utilization Threshold for 2G */
#define WBD_DEF_CHAN_UTIL_THLD_5G	0	/* Default Channel Utilization Threshold for 5G */
#define WBD_DEF_UAP			0	/* Default Upstream AP */
#define WBD_DEF_PER_CHAN_BCN_REQ	1	/* Default for per chan bcn request */
#define WBD_BH_OPT_MAX_TRY		4	/* Number of TBSS retries for BH STA */
#define WBD_DEF_BH_OPT			1	/* Default for backhaul optimization. 1-Eanbled */
#define WBD_DEF_AP_CONFIG_SEARCH_THLD	5	/* Number of ap auto config search threshold */
#define WBD_BH_OPT_MAX_TRY_ON_WEAK	2	/* Default number of backhaul optimization retries
						 * allowed upon weak STA processing failure due to
						 * formation of loop
						 */
#define WBD_DEF_PRIM_VLAN_ID		10	/* Default Primary VLAN ID */
#define WBD_DEF_SEC_VLAN_ID		20	/* Default Secondary VLAN ID */
#define WBD_DEF_VLAN_ETHER_TYPE		0x8100	/* Default VLAN ether type */

/* FBT NVRAMs default values */
#define WBD_FBT_NOT_DEFINED		-1
#define WBD_FBT_DEF_FBT_DISABLED	0
#define WBD_FBT_DEF_FBT_ENABLED		1
#define WBD_FBT_DEF_OVERDS		0
#define WBD_FBT_DEF_REASSOC_TIME	1000
#define WBD_FBT_DEF_AP			1

/* buffer length needed for wl_format_ssid */
/* 32 SSID chars, max of 4 chars for each SSID char "\xFF", plus NULL */
#define SSID_FMT_BUF_LEN		(4*32+1) /* Length for SSID format string */
/* Wi-Fi Blanket Buffer Lengths */
#define WBD_MAX_BUF_16			16
#define WBD_MAX_BUF_32			32
#define WBD_MAX_BUF_64			64
#define WBD_MAX_BUF_128			128
#define WBD_MAX_BUF_256			256
#define WBD_MAX_BUF_512			512
#define WBD_MAX_BUF_1024		1024
#define WBD_MAX_BUF_2048		2048
#define WBD_MAX_BUF_4096		4096
#define WBD_MAX_BUF_8192		8192
#define WBD_MAX_CMD_LEN			50	/* Maximum length of command */
#define WBD_MAX_IP_LEN			16	/* Length of IP address */
#define MAX_STATUS_STR_LEN		50	/* Maximum status length in the JSON */
#define WBD_MAX_NVRAM_NAME		128	/* Maximum length of Name of the NVRAM */
#define WBD_MAX_PROCESS_NAME		30	/* Maximum length of process NAME */
#define WBD_MAX_BSS_SUPPORTED		3	/* Maximum BSS Supported per radio */
#define WBD_MAX_POSSIBLE_VIRTUAL_BSS	8	/* Maximum Number possible of virtual BSS */
#define WBD_MAX_GET_BSSID_TRY		500	/* Number of times get BSSID to be tried */
#define WBD_GET_BSSID_USECOND_GAP	10	/* Milliseconds gap between each get BSSID */
#define WBD_MAX_ACSD_SET_CHSPEC_DELAY	1000	/* Maximum delay in acsd to set chanspec (ms) */
#define WBD_GET_CHSPEC_GAP		10	/* Milliseconds gap between each get chanspec */
#define WBD_STR_MAC_LEN			17	/* String representation of MAC address length */
#define WBD_MAX_CHAN_PREF_RC_COUNT	40	/* Maximum regulatory class count for individual
						 * center channels
						 */

/* Wi-Fi Blanket Sleep Intervals for time taking task failures */
#define WBD_SLEEP_SERVER_FAIL		5 /* Interval between each server creation if fails */
#define WBD_SLEEP_GATEWAYIP_FAIL	5 /* Interval between each get gateway IP fail */
#define WBD_SLEEP_GETIFR_FAIL		1 /* Interval between each get Interface Info fail */

/* Retry count for time taking task failures */
#define WBD_N_RETRIES			10

#define WBD_CMP_BSSID			1
#define WBD_CMP_MAC			2
#ifdef PLC_WBD
#define WBD_CMP_PLC_MAC			3
#endif /* PLC_WBD */

/* Default RSSI for ETH Slaves, as no need to calculate the RSSI for Eth and PLC Slaves */
#define WBD_DEFAULT_ETH_RSSI		-20
/* Default TX_RATE for ethernet slaves, is currently 1 Gbps(1000 Mbps) */
#define WBD_DEFAULT_ETH_TX_RATE		1000

/* Loopback IP address */
#define WBD_LOOPBACK_IP			"127.0.0.1"

/* Default Beacon request Sub Elements */
#define DEF_BCN_REQ_SUB_ELE		"01020000020100"
/* 05(Category: Radio Measurement) 00 (Action: Radio Request) 00(Token) 0000(No. of Repetions)
 * 26(Member ID: measure req) 17(Len) 00(Token) 00(Measurement Request Mode)
 * 05(Measurement Type: Beacon Request) 00(Operating Class) 00(chan Number) 0000(interval)
 * 0001(Duration) 01(Measuremnt Mode: Active) ffffffffffff(BSSID: Wildcard)
 * 01020000(Beacon reporting subelement) 020100(Reporting detail subelement)
 */

/* Weak client response reason code for vendor specific message */
typedef enum wbd_wc_resp_reason_code {
	WBD_WC_RESP_REASON_SUCCESS	= 0x00,
	WBD_WC_RESP_REASON_UNSPECIFIED	= 0x01,
	WBD_WC_RESP_REASON_IGNORE_STA	= 0x02,
	WBD_WC_RESP_REASON_BOUNCINGSTA	= 0x03
} wbd_wc_resp_reason_code_t;

/* NVRAM variable names */
#define WBD_NVRAM_MSGLVL		"wbd_msglevel"
#define WBD_NVRAM_IFNAMES		"wbd_ifnames"
#define WBD_NVRAM_IGNR_MACLST		"wbd_ignr_maclst"
#define WBD_NVRAM_BOUNCE_DETECT		"wbd_bounce_detect"
#define WBD_NVRAM_BOUNCE_DETECT_BH	"wbd_bounce_detect_bh"
#define WBD_NVRAM_REMOVE_CLIENT		"wbd_remove_client"
#define WBD_NVRAM_MASTER_IP		"wbd_master_ip"
#define WBD_NVRAM_FBT			"wbd_fbt"
#define WBD_NVRAM_MULTIAP_MODE		"multiap_mode"
#define WBD_NVRAM_MULTIAP_MSG_MODULE	"multiap_msglevel_module"
#define WBD_NVRAM_MULTIAP_MSGLEVEL	"multiap_msglevel"
#define WBD_NVRAM_MASTER_FLAGS		"wbd_master_flags"
#define WBD_NVRAM_UAP			"map_uap"
#define NVRAM_MAP_NO_MULTIHOP		"map_no_multihop"
#define NVRAM_MAP_ONBOARDED		"map_onboarded"
#define NVRAM_MAP_AGENT_CONFIGURED	"map_agent_configured"
#define NVRAM_BKT_MSGLEVEL		WBD_NVRAM_MSGLVL
#define WBD_NVRAM_BH_OPT_TRY		"wbd_bh_opt_try"
#define WBD_NVRAM_BH_OPT		"wbd_bh_opt"
#define WBD_NVRAM_BH_OPT_COMPLETE	"wbd_bh_opt_complete"
#define WBD_NVRAM_AP_CONFIG_SEARCH_THLD	"wbd_ap_config_search_thld"
#define WBD_NVRAM_BH_OPT_TRY_ON_WEAK	"wbd_bh_opt_try_on_weak"
#define WBD_NVRAM_PRIM_VLAN_ID		"wbd_prim_vlan_id"
#define WBD_NVRAM_SEC_VLAN_ID		"wbd_sec_vlan_id"
#define WBD_NVRAM_VLAN_ETHER_TYPE	"wbd_vlan_ether_type"

/* General NVRAMs */
#define NVRAM_SSID			"ssid"
#define NVRAM_AKM			"akm"
#define NVRAM_HWADDR			"hwaddr"
#define NVRAM_WPA_PSK			"wpa_psk"
#define NVRAM_CRYPTO			"crypto"
#define NVRAM_WEP			"wep"
#define NVRAM_AUTH			"auth"
#define NVRAM_MFP			"mfp"
#define NVRAM_BW_CAP			"bw_cap"
#define NVRAM_REG_MODE			"reg_mode"
#define NVRAM_ACS_FCS_MODE		"acs_fcs_mode"
#define NVRAM_BR0_IFNAMES		"br0_ifnames"
#define NVRAM_BR1_IFNAMES		"br1_ifnames"
#define NVRAM_BSS_ENABLED		"bss_enabled"
#define NVRAM_IFNAME			"ifname"
#define NVRAM_RADIO			"radio"
#define NVRAM_LAN_IFNAMES		"lan_ifnames"
#define NVRAM_LAN1_IFNAMES		"lan1_ifnames"
#define NVRAM_LAN_HWADDR		"lan_hwaddr"
#define NVRAM_UNIT			"unit"
#define NVRAM_WPS_MODE			"wps_mode"
#define NVRAM_MODE			"mode"
#define NVRAM_AKM			"akm"
#define NVRAM_CRYPTO			"crypto"
#define NVRAM_MAP			"map"
#define NVRAM_MAP_MAX_BSS		"map_max_bss"
#define NVRAM_BCN_REQ_SUB_ELE		"bcn_req_sub_ele"
#define NVRAM_TAF_ENABLE		"taf_enable"
#define NVRAM_CLOSED			"closed"	/* For making a bss hidden */
#define NVRAM_MAP_BH_OPEN		"map_bh_open"	/* Nvram for open bh bss def is hidden */
#define NVRAM_MAP_STA_WNM_CHECK		"map_sta_wnm_check" /* check wnm cap of sta */
#define NVRAM_MAP_PER_CHAN_BCN_REQ	"map_per_chan_bcn_req"	/* Send beacon request per chan */
#define NVRAM_BSSID			"bssid"
#define NVRAM_LAN_WPS_OOB		"lan_wps_oob"
#define NVRAM_WPS_ON_STA		"wps_on_sta"
#define NVRAM_HAPD_ENABLED		"hapd_enable"

/* NVRAMs for Target BSS Identification Configuration */
#define WBD_NVRAM_TBSS_WGHT_IDX		"wbd_tbss_wght_idx"
#define WBD_NVRAM_TBSS_WGHT_IDX_BH	"wbd_tbss_wght_idx_bh"
#define WBD_NVRAM_TBSS_WGHT		"wbd_tbss_wght"
#define WBD_NVRAM_TBSS_WGHT_BH		"wbd_tbss_wght_bh"
#define WBD_NVRAM_TBSS_THLD_IDX_2G	"wbd_tbss_thld_idx_2g"
#define WBD_NVRAM_TBSS_THLD_IDX_5G	"wbd_tbss_thld_idx_5g"
#define WBD_NVRAM_TBSS_THLD_IDX_BH	"wbd_tbss_thld_idx_bh"
#define WBD_NVRAM_TBSS_THLD_2G		"wbd_tbss_thld_2g"
#define WBD_NVRAM_TBSS_THLD_5G		"wbd_tbss_thld_5g"
#define WBD_NVRAM_TBSS_THLD_BH		"wbd_tbss_thld_bh"
#define WBD_NVRAM_TBSS_ALGO		"wbd_tbss_algo"
#define WBD_NVRAM_TBSS_ALGO_BH		"wbd_tbss_algo_bh"
#define WBD_NVRAM_ADV_THLD		"wbd_adv_thld"
#define WBD_NVRAM_TBSS_STACNT_THLD	"wbd_tbss_stacnt_thld"
#define WBD_NVRAM_TBSS_BOUNDARIES	"wbd_tbss_boundaries"

/* NVRAMs for Weak Client Identification Configuration */
#define WBD_NVRAM_WC_ALGO		"wbd_wc_algo"
#define WBD_NVRAM_WC_THLD_IDX		"wbd_wc_thld_idx"
#define WBD_NVRAM_WC_THLD		"wbd_wc_thld"
#define WBD_NVRAM_WEAK_STA_CFG		"wbd_weak_sta_cfg" /* Also, for Metric Reporting Policy */
#define WBD_NVRAM_BKT_WEAK_STA_CFG_2G	"wbd_bkt_weak_sta_cfg_2g"
#define WBD_NVRAM_BKT_WEAK_STA_CFG_5G	"wbd_bkt_weak_sta_cfg_5g"
#define WBD_NVRAM_WEAK_STA_CFG_BH	"wbd_weak_sta_cfg_bh"
#define WBD_NVRAM_WEAK_STA_POLICY	"wbd_weak_sta_policy"

/* NVRAMs for Timeouts */
#define WBD_NVRAM_TM_BLK_STA			"wbd_tm_blk_sta"
#define WBD_NVRAM_TM_SLV_WD_WEAK_CLIENT		"wbd_tm_wd_weakclient"
#define WBD_NVRAM_TM_SLV_WD_TARGET_BSS		"wbd_tm_wd_tbss"
#define WBD_NVRAM_TM_SLV_WD_BCN_RSSI		"wbd_tm_bcn_rssi"
#define WBD_NVRAM_TM_SLV_ACT_FRAME_INTERVAL	"wbd_tm_actframe"
#define WBD_NVRAM_TM_MTR_REMOVE_CLIENT_INTERVAL	"wbd_tm_remove_client"
#define WBD_NVRAM_TM_MTR_RTRY_REMOVE_CLIENT	"wbd_tm_retry_remove_client"
#define WBD_NVRAM_TM_RC_RESTART			"wbd_tm_rc_restart"
#define WBD_NVRAM_TM_MAP_MONITOR_READ		"wbd_tm_map_monitor_read"
#define WBD_NVRAM_TM_MAP_SEND_BCN_REPORT	"wbd_tm_map_send_bcn_report"
#define WBD_NVRAM_TM_AUTOCONFIG_RENEW		"wbd_tm_autoconfig_renew"
#define WBD_NVRAM_TM_UPDATE_CHANSPEC		"wbd_tm_update_chanspec"
#define WBD_NVRAM_TM_CHANNEL_SELECT		"wbd_tm_channel_select"
#define WBD_NVRAM_TM_PROBE			"wbd_tm_probe"
#define WBD_NVRAM_TM_BH_WD_WEAK_CLIENT		"wbd_tm_wd_bh_weakclient"
#define WBD_NVRAM_TM_SLV_WD_BH_TARGET_BSS	"wbd_tm_wd_bh_tbss"
#define WBD_NVRAM_TM_SLV_WD_BH_OPT_TARGET_BSS	"wbd_tm_wd_bh_opt_tbss"

#define WBD_NVRAM_MCHAN_SLAVE			"wbd_mchan"
#define WBD_NVRAM_SMART_CONV_RCPI		"wbd_smart_conv_rcpi"
#define WBD_NVRAM_VALID_RSSI_RANGE		"wbd_valid_rssi_range"
#define WBD_NVRAM_CROSSBAND_RSSI_EST		"wbd_crossband_rssi_est"
#define WBD_NVRAM_STEER_RETRY_CONFIG		"wbd_steer_retry_config"
#define WBD_NVRAM_BAND_STEER			"wbd_band_steer"
#define WBD_NVRAM_ENAB_SINGLE_CHAN_BH		"wbd_enab_single_chan_bh"
#define WBD_NVRAM_CHAN_UTIL_THLD_2G		"wbd_chan_util_thld_2g"
#define WBD_NVRAM_CHAN_UTIL_THLD_5G		"wbd_chan_util_thld_5g"

/* PLC related NVRAMs */
#define WBD_NVRAM_PLCIF		"plcif"
#define WBD_NVRAM_PLCMAC	"plcmac"

/* FBT related NVRAMs */
#define NVRAM_FBT_APS		"fbt_aps"
#define NVRAM_FBT_MDID		"fbt_mdid"
#define NVRAM_FBT_ADDR		"addr"
#define NVRAM_FBT_R1KH_ID	"r1kh_id"
#define NVRAM_FBT_R0KH_ID	"r0kh_id"
#define NVRAM_FBT_R0KH_ID_LEN	"r0kh_id_len"
#define NVRAM_FBT_R0KH_KEY	"r0kh_key"
#define NVRAM_FBT_R1KH_KEY	"r1kh_key"
#define NVRAM_FBT_BR_ADDR	"br_addr"
#define NVRAM_FBT_ENABLED	"fbt"
#define NVRAM_FBT_OVERDS	"fbtoverds"
#define NVRAM_FBT_REASSOC_TIME	"fbt_reassoc_time"
#define NVRAM_FBT_AP		"fbt_ap"

/* BSS info NVVRAMs */
#define NVRAM_MAP_BSS_NAMES	"map_bss_names"
#define NVRAM_BAND_FLAG		"band_flag"

/* Wi-Fi Blanket Bridge Types of Interfaces */
#define	WBD_BRIDGE_INVALID		-1	/* -1 Bridge Type is Invalid */
#define	WBD_BRIDGE_LAN			0	/*  0 Bridge Type is LAN */
#define	WBD_BRIDGE_GUEST		1	/*  1 Bridge Type is GUEST */
#define WBD_BRIDGE_VALID(bridge) ((((bridge) < WBD_BRIDGE_LAN) || \
					((bridge) > WBD_BRIDGE_GUEST)) ? (0) : (1))

/* Wi-Fi Blanket Band Types */
#define	WBD_BAND_LAN_INVALID	0x00	/* 0 - auto-select */
#define	WBD_BAND_LAN_2G		0x01	/* 1 - 2.4 Ghz */
#define	WBD_BAND_LAN_5GL	0x02	/* 2 - 5 Ghz LOW */
#define	WBD_BAND_LAN_5GH	0x04	/* 4 - 5 Ghz HIGH */
#define	WBD_BAND_LAN_ALL	(WBD_BAND_LAN_2G | WBD_BAND_LAN_5GL | WBD_BAND_LAN_5GH)

/* Local datetime format specifier. */
#define WBD_DATE_TIME_FORMAT	"%Y:%m:%d %X"

/* WBD steer response values. */
#define WBD_STEER_RESPONSE_ACCEPT		"Accept"
#define WBD_STEER_RESPONSE_REJECT		"Reject"
#define WBD_STEER_RESPONSE_UNKNOWN		"Unknown"

/* --- Wi-Fi Blanket Band Related Macros --- */

/* Print Wi-Fi Blanket Band Digit */
#define WBD_BAND_DIGIT(band) (((band) & WBD_BAND_LAN_2G) ? (2) : (5))

/* Print Wi-Fi Blanket Band Type Detail */
#define WBD_BAND_DTAIL(band) ((band) & WBD_BAND_LAN_2G ? "" : \
				((band) & WBD_BAND_LAN_5GL ? "LOW" : \
				((band) & WBD_BAND_LAN_5GH ? "HIGH" : "INVALID")))

/* Check if Wi-Fi Blanket Band Type is LAN_2G */
#define WBD_BAND_TYPE_LAN_2G(band) (((band) & WBD_BAND_LAN_2G) ? (1) : (0))

/* Check if Wi-Fi Blanket Band Type is LAN_5G */
#define WBD_BAND_TYPE_LAN_5G(band) ((((band) & WBD_BAND_LAN_5GL) || \
					((band) & WBD_BAND_LAN_5GH)) ? (1) : (0))

/* Validate Wi-Fi Blanket Band Digit */
#define WBD_BAND_VALID(band) ((band) & (WBD_BAND_LAN_ALL))

/* Get Neighboring Band for given Wi-Fi Blanket Band Digit */
#define WBD_BAND_NEIGHBOR(band) ((band) == WBD_BAND_LAN_2G ? WBD_BAND_LAN_INVALID : \
				((band) == WBD_BAND_LAN_5GL ? WBD_BAND_LAN_5GH : \
				((band) == WBD_BAND_LAN_5GH ? WBD_BAND_LAN_5GL : \
				WBD_BAND_LAN_INVALID)))

/* Print Band Name */
#define WBD_BAND_NAME(b) ((b & WBD_BAND_LAN_2G) ? "2.4G" : ((b & WBD_BAND_LAN_5GL) ? "5GL" :\
		((b & WBD_BAND_LAN_5GH) ? "5GH" : "Invalid")))

/* Band from channel */
#define WBD_BAND_FROM_CHANNEL(ch)	(((ch) <= 14) ? WBD_BAND_LAN_2G :\
		((ch) < 100) ? WBD_BAND_LAN_5GL : WBD_BAND_LAN_5GH)

/* Get WBD Band from 1905 Band */
#define WBD_BAND_FROM_1905_BAND(p1905_band) (p1905_band)

/* Is 5G check */
#define WBD_IS_BAND_5G(band)	(((band) & WBD_BAND_LAN_5GL) || ((band) & WBD_BAND_LAN_5GH))
/* Is Dual band. If it supports 5G then its dual band */
#define WBD_IS_DUAL_BAND(band)	(WBD_IS_BAND_5G((band)))

/* Use for direct output of Timeval structure in printf etc */
#define TIMEVALF			"%ld sec %ld usec"
#define TIMEVAL_TO_TIMEVALF(tmval)	(long)(tmval).tv_sec, (long)(tmval).tv_usec
/* ---------------------------------- Constant Declarations ------------------------------------ */

/* ------------------------------- Wi-Fi Blanket Utility Macros -------------------------------- */
#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (sizeof(a) / sizeof(a[0]))
#endif /* ARRAYSIZE */

#define WBDSTRNCPY(dst, src, len)	 \
	do { \
		strncpy(dst, src, len -1); \
		dst[len - 1] = '\0'; \
	} while (0)

#define WBD_SEC_MICROSEC(x) ((unsigned long long)(x) * 1000 * 1000)
#define WBD_MSEC_USEC(x) ((x) * 1000)

enum {
	WBD_VALUE_TYPE_UNKNOWN  = 0,
	WBD_VALUE_TYPE_RSSI	= 1,
	WBD_VALUE_TYPE_RCPI	= 2
};

#define WBD_RSSI_VALID(x)	((x) < (0) && (x) > (-125))

#define WBD_RCPI_TO_RSSI(x)	(((x) / 2) - 110)
#define WBD_RSSI_TO_RCPI(x)	(2 * ((x) + 110))

/* Compare MAC leaving out last 4 bits, to check if both MAC belongs to same MBSS or not */
#define WBD_EACMP_MBSS(a, b) ((((uint8 *)(a))[0] ^ ((uint8 *)(b))[0]) | \
				(((uint8 *)(a))[1] ^ ((uint8 *)(b))[1]) | \
				(((uint8 *)(a))[2] ^ ((uint8 *)(b))[2]) | \
				(((uint8 *)(a))[3] ^ ((uint8 *)(b))[3]) | \
				(((uint8 *)(a))[4] ^ ((uint8 *)(b))[4]) | \
				((((uint8 *)(a))[5] & 0xF0) ^ (((uint8 *)(b))[5] & 0xF0)))
/* ------------------------------- Wi-Fi Blanket Utility Macros -------------------------------- */

/* -------------------------- Wi-Fi Blanket Debug Print Macros ------------------------ */
extern unsigned int wbd_msglevel;
extern char g_wbd_process_name[WBD_MAX_PROCESS_NAME];

#define WBD_DEBUG_ERROR		0x0001
#define WBD_DEBUG_WARNING	0x0002
#define WBD_DEBUG_INFO		0x0004
#define WBD_DEBUG_DETAIL	0x0008
#define WBD_DEBUG_JSON		0x0020
#define WBD_DEBUG_TRACE		0X0080
#define WBD_DEBUG_TBSS		0x0100
#define WBD_DEBUG_BOUNCE	0x0200
#define WBD_DEBUG_STEER		0x0400
#define WBD_DEBUG_DS		0x0800
#define WBD_DEBUG_PROBE		0x1000
#define WBD_DEBUG_DEFAULT	WBD_DEBUG_ERROR

#define WBD_PRINT(prefix, fmt, arg...) \
	printf(prefix"WBD-%s >> (%lu) %s: "fmt, g_wbd_process_name, (unsigned long)time(NULL), \
	__FUNCTION__, ##arg)

#define WBD_PRINT_IF(msglevel, fmt, arg...) \
	if (wbd_msglevel & msglevel) \
		printf(fmt, ##arg)

#define WBD_ERROR(fmt, arg...) \
	if (wbd_msglevel & WBD_DEBUG_ERROR) \
		WBD_PRINT("Err: ", fmt, ##arg)

#define WBD_WARNING(fmt, arg...) \
	if (wbd_msglevel & WBD_DEBUG_WARNING) \
		WBD_PRINT("Warn: ", fmt, ##arg)

#define WBD_INFO(fmt, arg...) \
	if (wbd_msglevel & WBD_DEBUG_INFO) \
		WBD_PRINT("Info: ", fmt, ##arg)

#define WBD_DEBUG(fmt, arg...) \
	if (wbd_msglevel & WBD_DEBUG_DETAIL) \
		WBD_PRINT("Dbg: ", fmt, ##arg)

#define WBD_JSON(fmt, arg...) \
	if (wbd_msglevel & WBD_DEBUG_JSON) \
		WBD_PRINT("Json: ", fmt, ##arg)

#define WBD_TRACE(fmt, arg...) \
	if (wbd_msglevel & WBD_DEBUG_TRACE) \
		WBD_PRINT("Trace: ", fmt, ##arg)

#define WBD_TBSS(fmt, arg...) \
	if ((wbd_msglevel & WBD_DEBUG_TBSS) || (wbd_msglevel & WBD_DEBUG_INFO))\
		WBD_PRINT("TBSS: ", fmt, ##arg)

#define WBD_BOUNCE(fmt, arg...) \
	if (wbd_msglevel & WBD_DEBUG_BOUNCE) \
		WBD_PRINT("Bounce: ", fmt, ##arg)

#define WBD_STEER(fmt, arg...) \
	if (wbd_msglevel & WBD_DEBUG_STEER) \
		WBD_PRINT("Steer: ", fmt, ##arg)

#define WBD_DS(fmt, arg...) \
	if (wbd_msglevel & WBD_DEBUG_DS) \
		WBD_PRINT("DS: ", fmt, ##arg)

#define WBD_PROBE(fmt, arg...) \
	if (wbd_msglevel & WBD_DEBUG_PROBE) \
		WBD_PRINT("Probe: ", fmt, ##arg)

#define WBD_ENTER()	WBD_TRACE("Enter...\n")
#define WBD_EXIT()	WBD_TRACE("Exit...\n")

#define WBD_WL_DUMP_ENAB	(wbd_msglevel & WBD_DEBUG_DETAIL)

#define BCM_REFERENCE(data)	((void)(data))

/* -------------------------- Wi-Fi Blanket Debug Print Macros ------------------------ */

/* -------------------------------------------------------------------------------- */
/* ------- ASSERT /CHECK Macros, to avoid frequent checks, gives try/catch type utility ------- */

#define WBD_ASSERT_NULLMAC(mac_ptr, str) \
		do { \
			if (ETHER_ISNULLADDR(mac_ptr)) { \
				ret = WBDE_NULL_MAC; \
				WBD_WARNING("%s : %s\n", str, wbderrorstr(ret)); \
				goto end; \
			} \
		} while (0)

#define WBD_GET_VALID_MAC(mac_str, mac_ptr, str, ERR) \
		do { \
			if (strlen(mac_str) > 0) { \
				if (!wbd_ether_atoe(mac_str, mac_ptr)) { \
					ret = ERR; \
					WBD_WARNING("%s : %s\n", str, wbderrorstr(ret)); \
					goto end; \
				} \
				WBD_ASSERT_NULLMAC(mac_ptr, str); \
			} else { \
				ret = ERR; \
				WBD_WARNING("%s : %s\n", str, wbderrorstr(ret)); \
				goto end; \
			} \
		} while (0)

#define WBD_ASSERT_ARG(arg, ERR) \
		do { \
			if (!arg) { \
				ret = ERR; \
				WBD_WARNING("%s\n", wbderrorstr(ret)); \
				goto end; \
			} \
		} while (0)

#define WBD_ASSERT_MSGDATA(cmd, str) \
		do { \
			if (!cmd) { \
				ret = WBDE_NULL_MSGDATA; \
				WBD_WARNING("%s : %s\n", str, wbderrorstr(ret)); \
				goto end; \
			} \
		} while (0)

#define WBD_ASSERT() \
		do { \
			if (ret != WBDE_OK) { \
				goto end; \
			} \
		} while (0)

#define WBD_ASSERT_MSG(fmt, arg...) \
		do { \
			if (ret != WBDE_OK) { \
				WBD_WARNING(fmt, ##arg); \
				goto end; \
			} \
		} while (0)

#define WBD_ASSERT_ERR(error) \
		do { \
			if (ret != WBDE_OK) { \
				ret = (error); \
				goto end; \
			} \
		} while (0)

#define WBD_ASSERT_ERR_MSG(error, fmt, arg...) \
		do { \
			if (ret != WBDE_OK) { \
				WBD_WARNING(fmt, ##arg); \
				ret = (error); \
				goto end; \
			} \
		} while (0)

#define WBD_CHECK_MSG(fmt, arg...) \
		do { \
			if (ret != WBDE_OK) { \
				WBD_DEBUG(fmt, ##arg); \
			} \
		} while (0)

#define WBD_CHECK_ERR(error) \
		do { \
			if (ret != WBDE_OK) { \
				ret = (error); \
			} \
		} while (0)

#define WBD_CHECK_ERR_MSG(error, fmt, arg...) \
		do { \
			if (ret != WBDE_OK) { \
				WBD_DEBUG(fmt, ##arg); \
				ret = (error); \
			} \
		} while (0)

/* ------- ASSERT /CHECK Macros, to avoid frequent checks, gives try/catch type utility -------- */
/* --------------------------------------------------------------------------------- */

/* --------------------------------- Structure Definitions ---------------------------------- */

/* Wi-Fi Blanket Common Application Configurable Timeouts */
typedef struct wbd_timeouts {
	int tm_blk_sta;			/* Timeout for Slave to Unblock a Steering STA */
	int tm_wd_weakclient;		/* Timeout for Slave to run Weak Client Watchdog */
	int tm_wd_tbss;			/* Timeout for Slave to run Target BSS Watchdog */
	int tm_bcn_rssi;		/* Timeout for RSSI received in beacon report */
	int offchan_af_period;		/* Time period to send actframe to offchannel weak STA */
	int offchan_af_interval;	/* Time interval between each offchannel actframe */
	int tm_bcn_req_frame;		/* Timeout to send beacon request to associated weak sta */
	int tm_remove_client;		/* Timeout to remove stale client from BSS */
	int tm_retry_remove_client;	/* Timeout to remove stale client if the first try failed */
	int tm_rc_restart;		/* Timeout for Both to do rc restart gracefully */
	int tm_map_monitor_read;	/* Timeout to read monitored RSSI for MAP */
	int tm_map_send_bcn_report;	/* Timeout to Send beacon report for MAP (in ms) */
	int tm_autoconfig_renew;	/* Timeout to Send AP autoconfiguration renew */
	int tm_update_chspec;		/* Timeout for checking chanspec change */
	int tm_channel_select;		/* Timeout to Send channel preference query */
	int tm_probe;			/* Timeout to remove probe STA entry */
	int tm_wd_bh_weakclient;	/* Timeout to run backhaul STA weak client watchdog */
	int tm_wd_bh_tbss;		/* Timeout to run Target BSS Watchdog for Backhaul STA */
	int tm_wd_bh_opt_tbss;		/* Timeout to run Target BSS Watchdog for Backhaul
					 * optimization
					 */
} wbd_timeouts_t;

#ifdef PLC_WBD
/* Local PLC device information */
typedef struct wbd_plc_info {
	struct ether_addr plc_mac;	/* PLC device MAC Address */
	char ifname[IFNAMSIZ];		/* PLC interface */
	int enabled;			/* Enabled: 1; Disabled 0 */
} wbd_plc_info_t;
#endif /* PLC_WBD */

/* Main Wi-Fi Blanket Common Application structure to store module info */
struct wbd_info {
	int version;			/* version of this structure */
	int map_mode;			/* MultiAP Mode (Controller/Agent/Disabled) */

	int event_fd;			/* socket FD to receive events from Driver */
	int event_steer_fd;		/* Special socket FD for BSS transition response */
	int server_fd;			/* Socket FD of Server in Master or Slave */
	int cli_server_fd;		/* Socket FD of (CLI) Command Line Interface Server */

	char server_ip[WBD_MAX_IP_LEN];	/* IP address of the Master or Slave Server */

	uint32 flags;			/* Flags of type WBD_INFO_FLAGS_XXXX */

	wbd_timeouts_t max;		/* Handle to Configurable Timeouts */
	wbd_steer_retry_config_t steer_retry_config;	/* STEER Retry Config */
	uint8 crossband_rssi_est;	/* Real Free space path loss between 2G and 5G */
	int base_txpwr;			/* keeps lowest Tx Power of all Slaves.
					 * TODO : Move it to proper place once the band condition
					 * is removed from the master_info. Dont use it from
					 * master_info for now
					 */
	int8 rssi_max;
	int8 rssi_min;
#ifdef PLC_WBD
	wbd_plc_info_t plc_info;	/* Local PLC information */
#endif /* PLC_WBD */
	bcm_usched_handle *hdl;		/* Handle to Micro Scheduler Module */
	wbd_com_handle *com_serv_hdl;	/* Handle to the communication Module for Server */
	wbd_com_handle *com_cli_hdl;	/* Handle to the communication Module for CLI */
	wbd_com_handle *com_eventd_hdl;	/* Handle to the communication Module for EVENT FD */
	wbd_glist_t beacon_reports;	/* List of beacon reports of type wbd_beacon_reports_t */
	wbd_prb_sta_t *prb_sta[WBD_PROBE_HASH_SIZE];	/* Probe STA hash table */
	union {
		wbd_blanket_master_t *wbd_master; /* Master Application Specific Data Structure */
		wbd_blanket_slave_t *wbd_slave; /* Slave Application Specific Data Structure */
	};
};

/* --------------------------------- Structure Definitions ---------------------------------- */

/* ----------------------------- wbd_utils.c function declarations ----------------------------- */

/* Get Application module info */
wbd_info_t* wbd_get_ginfo();

/* Removes all occurence of the character from a string */
void wbd_remove_char(char *str, char let);

/* Get the gateway IP address */
int wbd_get_gatewayip(char *gatewayip, socklen_t size);

/* Convert Ethernet address string representation to binary data */
int wbd_ether_atoe(const char *a, struct ether_addr *n);

/* Convert binary data to Ethernet address string representation */
char * wbd_ether_etoa(const unsigned char *e, char *a);

/* Returns current local time in YYYY:MM:DD HH:MM:SS format. */
char* wbd_get_formated_local_time(char *buf, int size);

/* Returns Hexdump in ASCII formatted output */
void wbd_hexdump_ascii(const char *title, const unsigned char *buf, unsigned int len);

/* ----------------------------- wbd_utils.c function declarations ----------------------------- */

#endif /* _WBD_H_ */
