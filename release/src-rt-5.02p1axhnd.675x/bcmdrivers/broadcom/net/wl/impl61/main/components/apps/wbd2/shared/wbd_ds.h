/*
 * WBD Data structures for Master and Slave Info
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
 * $Id: wbd_ds.h 782695 2020-01-02 04:56:38Z $
 */

#ifndef _WBD_DS_H_
#define _WBD_DS_H_

#include <net/if.h>
#include <bcm_steering.h>
#include "blanket.h"
#include "ieee1905.h"
#include "ieee1905_datamodel_priv.h"
#include "wbd_rc_shared.h"

/* ----------------------------------- Forward Declarations ------------------------------------ */
/* FD : Main Wi-Fi Blanket Common Application structure */
typedef struct wbd_info wbd_info_t;
/* FD : Information of each Slave in Blanket */
struct wbd_slave_item;
typedef struct wbd_slave_item wbd_slave_item_t;
/* FD : Information of Slave in Blanket */
struct wbd_blanket_slave;
typedef struct wbd_blanket_slave wbd_blanket_slave_t;
/* FD : Information of Master in Blanket */
struct wbd_master_info;
typedef struct wbd_master_info wbd_master_info_t;
/* FD : Information of Master in Blanket */
struct wbd_blanket_master;
typedef struct wbd_blanket_master wbd_blanket_master_t;
/* ----------------------------------- Forward Declarations ------------------------------------ */

/* ---------------------------------- Constant Declarations ------------------------------------ */
/* Blanket IDs */
#define WBD_BKT_ID_BR0			1

#define WBD_MAX_BKT_NAME_LEN		20

/* Length of IP address */
#define WBD_MAX_IP_LEN			16
/* Set chanspec to Invalid for Multi channel support */
#define WBD_INVALID_CHANSPEC		-1

/* Update Flags to update Slave Item */
#define WBD_UPDATE_SLAVE_RSSI		0x0001
#define WBD_UPDATE_SLAVE_TX_RATE	0x0002

/* Bit mask for finding weak client based on threshold */
#define WBD_WC_THLD_FLAG_RSSI		0x0001
#define WBD_WC_THLD_FLAG_TXRT		0x0002

/* Defining default advantage threshold for the weightage score */
#define WBD_TBSS_ADV_THLD		5

/* Minimum number of associated STAs in a slave below which the normalized score is 100 */
#define WBD_TBSS_MIN_STA_THLD		5

/* Maximum phyrate/RSSI boundary above which the normalized value is 100 */
#define WBD_TBSS_MAX_PHYRATE_BOUNDARY_2G	100
#define WBD_TBSS_MAX_PHYRATE_BOUNDARY_5G	300
#define WBD_TBSS_MAX_RSSI_BOUNDARY_2G		(-45)
#define WBD_TBSS_MAX_RSSI_BOUNDARY_5G		(-45)

/* Minimum phyrate boundary below which the normalized value is 0 */
#define WBD_TBSS_MIN_PHYRATE_BOUNDARY_2G	36
#define WBD_TBSS_MIN_PHYRATE_BOUNDARY_5G	36
#define WBD_TBSS_MIN_RSSI_BOUNDARY_2G		(-85)
#define WBD_TBSS_MIN_RSSI_BOUNDARY_5G		(-85)

/* Bit mask for finding target BSS based on weightage */
#define WBD_WGHT_FLAG_RSSI		0x0001
#define WBD_WGHT_FLAG_HOPS		0x0002
#define WBD_WGHT_FLAG_STACNT		0x0004
#define WBD_WGHT_FLAG_UPLINKRT		0x0008
#define WBD_WGHT_FLAG_NSS		0x0010
#define WBD_WGHT_FLAG_TX_RATE		0x0020
#define WBD_WGHT_FLAG_ALL		(WBD_WGHT_FLAG_RSSI | WBD_WGHT_FLAG_HOPS | \
					WBD_WGHT_FLAG_STACNT | WBD_WGHT_FLAG_UPLINKRT | \
					WBD_WGHT_FLAG_NSS | WBD_WGHT_FLAG_TX_RATE)

/* Bit mask for finding target BSS based on threshold */
#define WBD_THLD_FLAG_RSSI		0x0001
#define WBD_THLD_FLAG_HOPS		0x0002
#define WBD_THLD_FLAG_STACNT		0x0004
#define WBD_THLD_FLAG_UPLINKRT		0x0008
#define WBD_THLD_FLAG_STA_TXRT		0x0010
#define WBD_THLD_FLAG_STA_TXFAIL	0x0020
#define WBD_THLD_FLAG_MAX_TXRATE	0x0040

/* Target BSS identification Algorithms */
#define WBD_SOF_ALGO_BEST_NWS		0	/* Use Best NWS algorithm in SOF */
#define WBD_SOF_ALGO_BEST_RSSI		1	/* Use Best RSSI algorithm in SOF */
#define WBD_MAX_SOF_ALGOS		WBD_SOF_ALGO_BEST_RSSI

/* Metric Reporting Policy */
#define WBD_STA_METRICS_REPORTING_RSSI_THLD			-70
#define WBD_STA_METRICS_REPORTING_RSSI_HYSTERISIS_MARGIN	3
#define WBD_STA_METRICS_REPORTING_IDLE_RATE_THLD		1000
#define WBD_STA_METRICS_REPORTING_TX_RATE_THLD			50
#define WBD_STA_METRICS_REPORTING_TX_FAIL_THLD			20

/* Metric Reporting Policy of Backhaul */
#define WBD_BH_METRICS_REPORTING_RSSI_THLD			-75
#define WBD_BH_METRICS_REPORTING_RSSI_HYSTERISIS_MARGIN		3
#define WBD_BH_METRICS_REPORTING_IDLE_RATE_THLD			1000
#define WBD_BH_METRICS_REPORTING_TX_RATE_THLD			50
#define WBD_BH_METRICS_REPORTING_TX_FAIL_THLD			200
#define WBD_BH_METRICS_REPORTING_FLAGS				(WBD_WEAK_STA_POLICY_FLAG_RSSI | \
								WBD_WEAK_STA_POLICY_FLAG_TX_RATE | \
								WBD_WEAK_STA_POLICY_FLAG_TX_FAIL)

/* BSD bounce detection defaults */
/* 60 seconds detection window, consecutive count 2, and dwell time 180 seconds.
 * It means if the STA is steered BSD_BOUNCE_DETECT_CNT times withing
 * BSD_BOUNCE_DETECT_WIN seconds, then block that STA from steering for
 * another BSD_BOUNCE_DETECT_DWELL seconds
 */
#define BSD_BOUNCE_DETECT_WIN	60
#define BSD_BOUNCE_DETECT_CNT	2
#define BSD_BOUNCE_DETECT_DWELL	180

/* Bounce detection defaults for backhaul STA */
#define WBD_BOUNCE_DETECT_WIN_BH	120
#define WBD_BOUNCE_DETECT_CNT_BH	4	/* Count is 4 by keeping in mind of multiple rc
						 * restarts in repeaters. So, during rc restarts,
						 * it should not enter into bouncing state
						 */
#define WBD_BOUNCE_DETECT_DWELL_BH	360

#define WBD_BOUNCE_HASH_SIZE	32
#define WBD_BOUNCE_MAC_HASH(ea)	(((ea).octet[4]+(ea).octet[5])% WBD_BOUNCE_HASH_SIZE)
#define WBD_MAXCHANNEL		30

/* Probe STA defaults */
#define WBD_PROBE_HASH_SIZE	32
#define WBD_PROBE_MAC_HASH(ea)	(((ea).octet[4]+(ea).octet[5])% WBD_PROBE_HASH_SIZE)

/* Default sta cfg for txrate and txfail.
 * These values should match with "bsd_wbd_weak_sta_policy_t" structure's
 * phyrate and tx_failures values in bsd/bsd_wbd.c
 */
#define WBD_TBSS_STA_TX_RATE_THLD	50	/* Default phyrate */
#define WBD_TBSS_STA_TX_FAILURES_THLD	20	/* Default tx_failure */

/* Minimum and maximum txrate boundry to filter bogus txrate reading */
#define WBD_TBSS_MAX_TXRATE_BOUNDARY	6934
#define WBD_TBSS_MIN_TXRATE_BOUNDARY	6

/* WBD Compare Results */
#define WBD_CMP_RET_EQUAL	0	/* Param 1 is Equal to Param 2 */
#define WBD_CMP_RET_LESS	1	/* Param 1 is Less than Param 2 */
#define WBD_CMP_RET_GREATER	2	/* Param 1 is Greater than Param 2 */

/* WBD logs buffer size. */
#define WBD_LOGS_BUF_128		128
#define WBD_LOGS_BUF_256		256

/* FBT constants */
#define WBD_FBT_R0KH_ID_LEN		48
#define WBD_FBT_KH_KEY_LEN		17
#define WBD_MAX_AP_NAME			16
#define WBD_FBT_NVRAMS_PER_SLAVE	7

/* wbd_assoc_item_t flags bit mask values */
#define WBD_FLAGS_ASSOC_ITEM_TBSS		0x00000001	/* TBSS timer created */
#define WBD_FLAGS_ASSOC_ITEM_BH_OPT		0x00000002	/* Backhaul Optimization Running */
#define WBD_FLAGS_ASSOC_ITEM_BH_OPT_DONE	0x00000004	/* Backhaul Optimization Done */

/* Helper macro for assoc item bit flags of type WBD_FLAGS_ASSOC_ITEM_XXX */
#define WBD_ASSOC_ITEM_IS_TBSS_TIMER(flags)	((flags) & WBD_FLAGS_ASSOC_ITEM_TBSS)
#define WBD_ASSOC_ITEM_IS_BH_OPT(flags)		((flags) & WBD_FLAGS_ASSOC_ITEM_BH_OPT)
#define WBD_ASSOC_ITEM_IS_BH_OPT_DONE(flags)	((flags) & WBD_FLAGS_ASSOC_ITEM_BH_OPT_DONE)

/* IEEE1905 Interface vendor item bit flags */
#define WBD_FLAGS_IFR_AP_SCAN_DISABLED	0x0001	/* WPA Supplicant AP Scan Disabled */

/* Slave Info Bit flags */
#define WBD_FLAGS_BSS_FBT_ENABLED	0x0001	/* FBT enabled */

/* Bounce Detect STA structure Bit flags */
#define WBD_STA_BOUNCE_DETECT_FLAGS_BH	0x01	/* STA is backhaul STA */

/* ---------------------------------- Constant Declarations ------------------------------------ */

/* ------------------------------- Structure/Enum Definitions ---------------------------------- */
/* Define a Generic List */
typedef struct wbd_glist {
	uint count;			/* Count of list of objects */
	dll_t head;			/* Head Node of list of objects */
} wbd_glist_t;

/* Traverse each item of a Generic List */
#define foreach_glist_item(item, list) \
		for ((item) = dll_head_p(&((list).head)); \
			! dll_end(&((list).head), (item)); \
			(item) = dll_next_p((item)))

/* Traverse each item of a Generic List, Check for additional condition */
#define foreach_glist_item_ex(item, list, condition) \
		for ((item) = dll_head_p(&((list).head)); \
			((!dll_end(&((list).head), item))&& ((condition))); \
			(item) = dll_next_p((item)))

/* Traverse each item of a Generic List, with keep track of next node */
#define foreach_safe_glist_item(item, list, next) \
		for ((item) = dll_head_p(&((list).head)); \
			!dll_end(&((list).head), (item)); \
			(item) = (next))

/* Traverse each item of a Generic List */
#define foreach_i5glist_item(item, type, list) \
		for ((item) = ((type*)i5ll_head_p((list))); \
			! i5ll_end((item)); \
			(item) = ((type*)i5ll_next_p((item))))

/* Traverse each item of a Generic List, Check for additional condition */
#define foreach_i5glist_item_ex(item, type, list, condition) \
		for ((item) = ((type*)i5ll_head_p((list))); \
			(! i5ll_end((item)) && ((condition))); \
			(item) = ((type*)i5ll_next_p((item))))

/* Traverse each item of a Generic List, with keep track of next node */
#define foreach_safe_i5glist_item(item, type, list, next) \
		for ((item) = ((type*)i5ll_head_p((list))); \
			! i5ll_end((item)); \
			(item) = (next))

/*  ------------------------ Weak Client Identification Info  ------------------------ */
/* Threshold parameters for finding weak client based on SoF algorithm */
typedef struct wbd_wc_thld {
	int t_rssi;			/* Threshold for RSSI */
	float tx_rate;			/* Threshold for Tx Rate */
	int flags;			/* WBD_WC_XX, Flags indicates which params to consider */
} wbd_wc_thld_t;

/* Information of Weak Client identification */
typedef struct wbd_wc_ident_info {
	int wc_algo;			/* Weak Client deciding WBD algorithm index */
	int wc_thld;			/* Weak Client deciding threshold index */
	wbd_wc_thld_t wc_thld_cfg;	/* Threshold config for deciding Weak Client */
} wbd_wc_ident_info_t;
/*  ------------------------ Weak Client Identification Info  ------------------------ */

/*  ------------------------ Target BSS Identification Info  ------------------------ */
/* Weightage parameters for finding target parameters based on NWS algorithm */
typedef struct wbd_tbss_wght {
	int w_rssi;			/* Weight for RSSI */
	int w_hops;			/* Weight for Number of hops */
	int w_sta_cnt;			/* Weight for number of associated STAs */
	int w_phyrate;			/* Weight for PHY rate */
	int w_nss;			/* Weigth for NSS */
	int w_tx_rate;			/* Weight for Tx rate of Slave to serve STAs */
	int flags;			/* WBD_WGHT_XX, Flags indicates which params to consider */
} wbd_tbss_wght_t;

/* Threshold parameters for finding target parameters based on SoF algorithm */
typedef struct wbd_tbss_thld {
	int t_rssi;			/* Threshold for RSSI */
	int t_hops;			/* Threshold for Number of Hops */
	int t_sta_cnt;			/* Threshold for number of Associated STAs */
	int t_uplinkrate;		/* Threshold for Uplink rate */
	int flags;			/* WBD_THLD_XX, Flags indicates which params to consider */
	int sof_algos;			/* WBD_SOF_ALGO_XX Algorithm to use, if Candidate BSS > 1 */
} wbd_tbss_thld_t;

/* Threshold parameters for finding target bss based on sta config */
typedef struct wbd_tbss_sta_thld {
	int t_tx_rate;			/* Threshold for Tx Rate */
	int t_tx_failures;		/* Threshold for Tx Failures */
} wbd_tbss_sta_thld_t;

/* Information of Target BSS identification */
typedef struct wbd_tbss_ident_info {
	int nws_adv_thld;		/* Advantage threshold value for NWS algo */
	uint8 tbss_wght;		/* Target BSS finding weightage index */
	uint8 tbss_wght_bh;		/* Target BSS finding weightage index for backhaul */
	uint8 tbss_thld_2g;		/* 2G Target BSS finding threshold index */
	uint8 tbss_thld_5g;		/* 5G Target BSS finding threshold index */
	uint8 tbss_thld_bh;		/* Backhaul Target BSS finding threshold index */
	uint8 tbss_algo;		/* Target BSS finding algorithm index */
	uint8 tbss_algo_bh;		/* Target BSS finding algorithm index for backhaul STA */
	int tbss_stacnt_thld;		/* Threshold for minimum STA count in SoF */
	int tbss_min_phyrate;		/* Phyrate below this is normalized to 0 */
	int tbss_max_phyrate;		/* Phyrate above this is normalized to 100 */
	int tbss_min_rssi;		/* RSSI below this is normalized to 0  */
	int tbss_max_rssi;		/* RSSI above this is normalized to 100 */
	int max_nss;			/* Max NSS to serve */
	wbd_tbss_wght_t wght_cfg;	/* Weightage config for finding target BSS */
	wbd_tbss_wght_t wght_cfg_bh;	/* Backhaul Weightage config for finding target BSS */
	wbd_tbss_thld_t thld_cfg_2g;	/* 2G Threshold config for finding target BSS */
	wbd_tbss_thld_t thld_cfg_5g;	/* 5G Threshold config for finding target BSS */
	wbd_tbss_thld_t thld_cfg_bh;	/* Backhaul Threshold config for finding target BSS */
	wbd_tbss_sta_thld_t thld_sta;	/* Threshold config of sta params for finding target BSS */
} wbd_tbss_ident_info_t;
/*  ------------------------ Target BSS Identification Info  ------------------------ */

/* Status of the STA */
typedef enum {
	WBD_STA_STATUS_INVALID = -1,	/* Invalid status */
	WBD_STA_STATUS_NORMAL = 0,	/* Working Normally. No need to steer */
	WBD_STA_STATUS_IGNORE,		/* Ignore the STA from steering */
	WBD_STA_STATUS_WEAK,		/* Weak STA, try to steer */
	WBD_STA_STATUS_WEAK_STEERING,	/* Weak STA and steer command is sent */
	WBD_STA_STATUS_BOUNCING		/* Weak STA is a bouncing STA */
} wbd_sta_status_t;

/* Triggers which can be used for further action */
typedef enum {
	WBD_TRIGR_NORMAL_CLIENT_DELETED = 0x0001, /* Normal STA got deleted */
	WBD_TRIGR_IGNORE_CLIENT_DELETED = 0x0002, /* Ignore STA got deleted */
	WBD_TRIGR_WEAK_CLIENT_DELETED = 0x0004 /* Weak STA got deleted */
} wbd_trigger_t;

/* WBD chan change reasons being shared across master and slave,
 * this enum is supposed to work in sync with wl_ap_chan_changed_event_t
 * enum defined in bcmevent.h,
 * =========  Add new entries at end ================
 */
typedef enum {
	WBD_REASON_OTHER = -2, /* chan change observed during keep alive interval */
	WBD_REASON_RADAR,
	WBD_REASON_CSA, /* CSA chan change event occured */
	WBD_REASON_DFS_AP_MOVE_START, /* 3+1 trigger */
	WBD_REASON_DFS_AP_MOVE_RADAR_FOUND, /* Radar found on scan core in 3+1 */
	WBD_REASON_DFS_AP_MOVE_ABORTED, /* 3+1 BGDFS aborted */
	WBD_REASON_DFS_AP_MOVE_SUCCESS, /* 3+1 BGDFS success, chan change */
	WBD_REASON_DFS_AP_MOVE_STUNT /* 3+1 BGDFS, dont change channel */
} wbd_chan_change_reason_t;

/* Config for retrying the steer in case of BSS transition is not accepted by STA */
typedef struct wbd_steer_retry_config {
	int tm_gap;		/* Timeout for next STEER. For Every other steer timeout doubles */
	int retry_count;	/* How Many times the STEER has to be retried in case of failure.
				 * -1 for Infinite, 0 For no repeat etc...
				 */
} wbd_steer_retry_config_t;

/* Information of each Mac in maclist */
typedef struct wbd_mac_item {
	dll_t node;			/* self referencial (next,prev) pointers of type dll_t */
	struct ether_addr mac;		/* MAC address */
} wbd_mac_item_t;

/* Type of the slave
 * TODO : Needs to add entry for PLC and MOCA interfaces
 */
typedef enum {
	WBD_SLAVE_TYPE_UNDEFINED = -1,
	WBD_SLAVE_TYPE_MASTER = 0,
	WBD_SLAVE_TYPE_ETHERNET,
	WBD_SLAVE_TYPE_DWDS,
	WBD_SLAVE_TYPE_PLC
} wbd_slave_type_t;

/* Different states of for detecting ping pong of steering */
typedef enum {
	WBD_BOUNCE_INIT = 0,
	WBD_BOUNCE_WINDOW_STATE,
	WBD_BOUNCE_DWELL_STATE,
	WBD_BOUNCE_CLEAN_STATE
} wbd_bounce_state_t;

/* Current values of the bouncing STAs */
typedef struct wbd_bounce_detect {
	uint32 window;		/* window time in seconds */
	uint32 cnt;		/* counts */
	uint32 dwell_time;	/* dwell time in seconds */
} wbd_bounce_detect_t;

/* Entry of the bouncing details of each STA */
typedef struct wbd_sta_bounce_detect {
	uint8 flags;			/* Flags of type WBD_STA_BOUNCE_DETECT_FLAGS_XXX */
	time_t timestamp;		/* timestamp of first steering time */
	struct ether_addr sta_mac;	/* MAC address of STA */
	struct ether_addr src_mac;	/* MAC address of associated slave */
	wbd_bounce_detect_t run;	/* sta bounce detect running counts */
	wbd_bounce_state_t state;	/* Current state of the STA */
	wbd_sta_status_t sta_status;	/* Status of the STA (normal, weak, steering etc...) */
	uint8 rssi_or_rcpi;		/* Store if the STA sends RSSI or RCPI in beacon report */

	struct wbd_sta_bounce_detect *next;
} wbd_sta_bounce_detect_t;

/* Entry of the probe STA details */
typedef struct wbd_prb_sta {
	struct ether_addr sta_mac;	/* MAC address of the STA */
	time_t active;			/* activity timestamp */
	uint8 band;			/* Band info of type WBD_BAND_LAN_XXX */
	struct wbd_prb_sta *next;
} wbd_prb_sta_t;

/* FBT info */
typedef struct wbd_fbt_info {
	unsigned short mdid;			/* MDID of the blanket */
	unsigned char ft_cap_policy;		/* 9.4.2.47 FBT Capab & Policy, eg FBT over DS */
	unsigned int tie_reassoc_interval;	/* 9.4.2.49 Reassociation deadline interval */
} wbd_fbt_info_t;

/* Information of STA to extract from WL */
typedef struct wbd_wl_sta_stats {
	int rssi;
	time_t sta_tm;		/* Timestamp when assoc sta rssi updated */

	float tx_rate;		/* unit Mbps */
	uint32 tx_failures;	/* tx retry - recent value */
	uint32 tx_tot_failures;	/* tx retry - total value */
	uint32 old_tx_tot_failures;	/* tx retry - total value (previous) */
	uint32 idle_rate;	/* data rate to measure STA is idle */
	uint32 rx_tot_pkts;	/* # of data packets recvd */
	uint64 rx_tot_bytes;	/* bytes recvd */

	/* beacon report parameters */
	int8 bcn_rpt_rssi;	/* signal strength */
	time_t bcn_tm;		/* Timestamp when sta beacon report rssi updated */
	time_t active;		/* activity timestamp */
} wbd_wl_sta_stats_t;

/* Information of each STA in Blanket */
typedef struct wbd_assoc_sta_item {
	dll_t node;			/* self referencial (next,prev) pointers of type dll_t */

	int is_offchan_actframe_tm;	/* Is off-channel ACT_FRAME timer created */
	uint32 flags;			/* FLAGS of type WBD_FLAGS_ASSOC_ITEM_XXX */

	int steer_fail_count;		/* Number of times the STEER has failed continuously */
	uint32 steer_retry_timeout;	/* Previous timeout for retrying the steer */
	int score;			/* Score of STA in a slave for finding target BSS */
	int fail_cnt;			/* Fail count of STA in a slave in NWS algo */
	int error_code;			/* Error code if any */
	uint32 dwell_time;		/* Remaining dwell time */
	time_t dwell_start;		/* Timestamp at which dwell time started */
	struct timeval assoc_time;	/* Last associated time, same as steered time */
	int32 last_weak_rssi;		/* RSSI reported earlier, only used in Controller */
	uint8 bh_opt_count;		/* Number of times the TBSS tried in backhaul Optimization */
	uint8 bh_opt_count_on_weak;	/* Number of times the backhaul optimization tried after
					 * weak STA processing failure due to formation of loop
					 */
	wbd_sta_status_t status;	/* Status of the STA (normal, weak etc...) */
	wbd_wl_sta_stats_t stats;	/* current sta stats */
} wbd_assoc_sta_item_t;

/* Information of each STA in Blanket */
typedef struct wbd_monitor_sta_item {
	dll_t node;			/* self referencial (next,prev) pointers of type dll_t */
	time_t monitor_tm;		/* Timestamp when unassoc sta rssi updated */
	int16 rssi;			/* STA Receive Signal Strength (in dBm) */
	int score;			/* Score of STA in a slave for finding target BSS */
	int fail_cnt;			/* Fail count of STA in a slave in NWS algo */
	struct ether_addr sta_mac;	/* STA MAC Address */
	struct ether_addr slave_bssid;	/* Slave BSSID, it is associated with */

	/* beacon report parameters */
	uint8 regclass;		/* Operating class */
	uint8 channel;		/* channel */
	int8 bcn_rpt_rssi;	/* signal strength */
	time_t bcn_tm;		/* Timestamp when sta beacon report rssi updated */
} wbd_monitor_sta_item_t;

#ifdef PLC_WBD
/* Information of each PLC remote node (it could be a standalone PLC device
 * not even participating in the blanket
 */
typedef struct wbd_plc_sta_item {
	dll_t node;		/* self referencial (next,prev) pointers of type dll_t */
	struct ether_addr mac;	/* MAC address of the PLC remote node */
	float tx_rate;		/* unit Mbps */
	float rx_rate;		/* unit Mbps */
} wbd_plc_sta_item_t;
#endif /* PLC_WBD */

/* Information to store slave's chan_info values */
typedef struct wbd_chan_info {
	uint8 channel;	/* Channel number */
	uint16 bitmap;	/* Channel parameters i.e. radar sensitive or not */
} __attribute__ ((__packed__)) wbd_chan_info_t;

typedef struct wbd_interface_chan_info {
	uint8 count;
	wbd_chan_info_t chinfo[WBD_MAXCHANNEL];
} __attribute__ ((__packed__)) wbd_interface_chan_info_t;

typedef struct wbd_dfs_chan_info {
	uint8 count;	/* count of 5g common control channels in channel array */
	uint8 channel[WBD_MAXCHANNEL];
} __attribute__ ((__packed__)) wbd_dfs_chan_info_t;

typedef struct wbd_taf_list {
	int count;	/* list count */
	char **pStr;	/* Array of string */
} wbd_taf_list_t;

typedef struct wbd_taf_params {
	wbd_taf_list_t	sta_list;	/* Traffic scheduler list of sta configurations */
	wbd_taf_list_t	bss_list;	/* list of bss configurations */
	char		*pdef;		/* default configurations */
} wbd_taf_params_t;

/* Information for Slave's WL Interface */
typedef struct wbd_wl_interface {
	int enabled;			/* 1 If the interface is enabled */
	int bridge_type;		/* Slave's Bridge Type LAN/GUEST */
	chanspec_t chanspec;		/* Slave's chanspec */
	uint8 rclass;			/* Slave's Regulatory Class */
	struct ether_addr radio_mac;	/* Radio Interface MAC Address */
	struct ether_addr mac;		/* Slave's MAC Address */
	struct ether_addr bssid;	/* Slave's BSSID */
	struct ether_addr br_addr;	/* Slave's Bridge Address */
	wlc_ssid_t blanket_ssid;	/* Slave's SSID */
#ifdef PLC_WBD
	struct ether_addr plc_mac;	/* Slave's PLC MAC Address */
#endif /* PLC_WBD */
	int16 RSSI;			/* Slave's RSSI (in dBm) wrt Master */
	float uplink_rate;		/* Slave's rx_rate (in Mbps) wrt Master */
	struct ifreq ifr;		/* Slave's WL driver adapter */
	char prefix[IFNAMSIZ];		/* Slave's instance of interface */
	char slave_ip[WBD_MAX_IP_LEN];	/* Slave's IP Address of WL Interface */
	char primary_prefix[IFNAMSIZ];	/* primary interface prefix */
	wbd_interface_chan_info_t wbd_chan_info;	/* interface specific chan_info */
	int avg_tx_rate;		/* avergae tx_rate w.r.t associated clients */
	int max_tx_rate;		/* Max possible tx_rate */
	int max_nss;			/* Max number of NSS in bss_info */
	int txpwr_target_max;		/* Slave's Max Tx Power */
	uint32 bssid_info;		/* BSSID info to be passed for steering */
	uint8 phytype;			/* Slave's phytype */
	uchar apsta;			/* interface configured with DWDS/PSR mode */
	int maxassoc;			/* MAX Assoclist for this interface */
} wbd_wl_interface_t;

/* Information of each Slave in Blanket */
struct wbd_slave_item {
	dll_t node;			/* self referencial (next,prev) pointers of type dll_t */

	int band;			/* Band, this Slave Item is responsible in Blanket for */
	bcm_stamon_handle *stamon_hdl;	/* Handle to the stamon module */

	wbd_slave_type_t slave_type;	/* Slave type, master, ethernet or dwds */

	uint32 flags;			/* Bit flags */

	uint8	dfs_event[1];		/* Flags indicate wl dfs events to be ignored or not */
#ifdef PLC_WBD
	wbd_glist_t plc_sta_list;	/* List of plc asso type objects */
#endif /* PLC_WBD */

	wbd_wl_interface_t wbd_ifr;	/* Slave's WL Interface Info */

	wbd_wc_ident_info_t wc_info;	/* Weak Client Identification info */

	wbd_blanket_slave_t *parent;	/* keeps track of its parent */
	wl_wlif_hdl *wlif_hdl;		/* Handle to wlif lib module. */
	wbd_taf_params_t *taf_info;	/* list of sta with prio and type to share across
					 * Repeaters, This List will be same across all
					 * Repeaters, will be set by Root AP
					 */
};

/* Device specific flags */
#define WBD_DEV_FLAG_NVRAM_SET 0x0001	/* NVRAM set vendor message sent to device */

/* WBD Vendor Specific Information of each device on the topology */
typedef struct wbd_device_item {
	uint32 flags;				/* Flags of type WBD_DEV_FLAG_XXX */
	time_t nbrlinkmetrics_timestamp;	/* neighbor link metrics query timestamp */
	time_t bssmetrics_timestamp;		/* Vendor specific BSS metrics query timestamp */
	uint8 normalized_hops;			/* Normalized hop value for TBSS 0 to 100 */
	uint16 min_phyrate;			/* Minimum phyrate from self to controller
						 * in each hop
						 */
} wbd_device_item_t;

/* WBD Vendor Specific Information of each interface on this Device */
typedef struct wbd_ifr_item {
	uint16 flags;				/* Bit flags of type WBD_FLAGS_IFR_XXX */
	unsigned char chan_util_thld;		/* Channel utilization threshold value from
						 * metric policy
						 */
	unsigned char chan_util_reported;	/* Last reported channel utilization */

	wbd_interface_chan_info_t *chan_info;	/* interface specific chan_info */
} wbd_ifr_item_t;

/* WBD Vendor Specific Information of each BSS on this Device */
typedef struct wbd_bss_item {

	wbd_fbt_info_t fbt_info;	/* FBT info sent by Controller */

	char r0kh_id[WBD_FBT_R0KH_ID_LEN];	/* R0KH_ID of this BSS */
	char r0kh_key[WBD_FBT_KH_KEY_LEN];	/* R0KH_KEY of this BSS */

	unsigned char br_addr[MAC_ADDR_LEN];	/* Bridge MAC address of this BSS */

	uint32 flags;			/* Bit flags of type WBD_FLAGS_BSS_XXX */
	uint32 bssid_info;		/* BSSID info to be passed for steering */
	uint8 phytype;			/* BSS's phytype */
	wbd_glist_t monitor_sta_list;	/* List of wbd_monitor_sta_item_t type objects */

	uint32 avg_tx_rate;		/* Average tx_rate w.r.t associated clients */
	uint32 max_tx_rate;		/* Max possible tx_rate */

	time_t apmetrics_timestamp;	/* AP metrics query timestamp */

} wbd_bss_item_t;

/* Information of Slave in Blanket */
struct wbd_blanket_slave {
	uint32 flags;			/* Bit flags of type WBD_BKT_SLV_FLAGS_XXX */
	wbd_glist_t br0_slave_list;	/* List of wbd_slave_item_t type objects in br0 (LAN) */
	wbd_weak_sta_policy_t metric_policy_bh;	/* Metric report policy configuration of backhaul */
	uint32 n_ap_auto_config_search;		/* Number of AP auto config search sent */
	uint8 n_ap_auto_config_search_thld;	/* MAX threshold of ap auto configuration search
						 * after which slave can block backhaul BSS
						 */
	wbd_info_t *parent;		/* keeps track of its parent */
};

/* Information of Master in Blanket */
struct wbd_master_info {
	dll_t node;			/* self referencial (next,prev) pointers of type dll_t */

	uint8 bkt_id;			/* ID of the blanket, 0 is br0 */
	char bkt_name[WBD_MAX_BKT_NAME_LEN]; /* Name of the blanket */
	uint32 flags;			/* Bit flags of type WBD_FLAGS_MASTER_XXX */
	int blanket_client_count;	/* Number of Total Clients in Blanket */
	unsigned char blanket_bss_count;	/* Number of BSS in the blanket */
	int weak_client_count;		/* Number of Weak Clients in Blanket */
	uint32 max_avg_tx_rate;		/* Maximum of avg_tx_rate of all the BSS in the topology */
	void* dfs_chan_list;		/* Broadcast dfs_chan_list to every slave */
	wbd_glist_t slave_list;		/* List of wbd_slave_item_t type objects */
	wbd_slave_item_t* pm_slave;	/* Pointer to Slave running in Master mode */
	wbd_tbss_ident_info_t tbss_info; /* Target BSS Identification info */

	wbd_interface_chan_info_t chan_info;	/* Good channels to use in future */

	wbd_fbt_info_t fbt_info;	/* FBT related info */

	wbd_weak_sta_policy_t metric_policy_2g;	/* Metric report policy configuration for 2G band */
	wbd_weak_sta_policy_t metric_policy_5g;	/* Metric report policy configuration for 5G band */
	wbd_weak_sta_policy_t metric_policy_bh;	/* Metric report policy configuration of backhaul */

	wbd_glist_t ap_chan_report;	/* AP channel report for this blanket of type
					 * wbd_bcn_req_rclass_list_t. This is to send it in the
					 * Beacon Metrics Query
					 */

	wbd_blanket_master_t *parent;	/* keeps track of its parent */
};

/* Log details for masters in blanket. */
typedef struct wbd_logs_info {
	int index;			/* Read and write index. */
	int buflen;			/* Buffer len. */
	char logmsgs[WBD_LOGS_BUF_256][WBD_LOGS_BUF_128]; /* Buffer for storing log messages. */
} wbd_logs_info_t;

/* Information of Master in Blanket */
struct wbd_blanket_master {
	wbd_glist_t blanket_list;	/* List of wbd_master_info_t type objects */

	uint8 max_bh_opt_try;		/* Number of TBSS retries for STA in backhaul
					 * optimization
					 */
	uint8 max_bh_opt_try_on_weak;	/* Number of backhaul optimization retries allowed upon
					 * weak STA processing failure due to formation of loop
					 */
	uint32 flags;			/* Blanket Master flags of type WBD_FLAGS_BKT_XXX */
	/* Hash table for the bouncing STAs. All the STAs which are steered */
	wbd_sta_bounce_detect_t *sta_bounce_table[WBD_BOUNCE_HASH_SIZE];
	wbd_bounce_detect_t bounce_cfg;	/* STA bounce detect config */
	wbd_bounce_detect_t bounce_cfg_bh;	/* STA bounce detect config */
	wbd_glist_t ignore_maclist;	/* List of wbd_mac_item_t obj, ignored fm steering */
	wbd_logs_info_t master_logs;	/* stores logs information. */
	wbd_info_t *parent;		/* keeps track of its parent */
};

/* list of all beacon reports */
typedef struct wbd_beacon_reports {
	dll_t node;				/* self referencial (next,prev) pointers of
						 * type dll_t
						 */
	time_t timestamp;			/* Timestamp when this report updated */
	struct ether_addr neighbor_al_mac;	/* AL MAC of neighbor from where request came */
	struct ether_addr sta_mac;		/* MAC address of the STA */
	uint8 report_element_count;		/* Number of measurement report element TLV */
	uint16 report_element_len;		/* Length of all measurement report elements */
	uint8 *report_element;			/* All the measurement report elements */
} wbd_beacon_reports_t;

/* Timer args for Tbss finding routine */
typedef struct wbd_tbss_timer_args {
	wbd_master_info_t *master_info;
	unsigned char al_mac[MAC_ADDR_LEN];
	unsigned char bssid[MAC_ADDR_LEN];
	unsigned char sta_mac[MAC_ADDR_LEN];
	uint8 is_backhaul;	/* Is STA backhaul */
} wbd_tbss_timer_args_t;

/* ------------------------------- Structure/Enum Definitions ---------------------------------- */

/* ------------------------------ Macros to Update Structure ------------------------------- */

/* Update STA's Status to NORMAL/IGNORE/WEAK/STEERED/BOUNCING, update Weak Client Count */
#define WBD_DS_UP_ASSOCSTA_STATUS(info, sta, value) \
	do { \
		if (((value) == WBD_STA_STATUS_WEAK) && \
				((sta)->status != WBD_STA_STATUS_WEAK)) \
			((info)->weak_client_count)++; \
		else if (((value) != WBD_STA_STATUS_WEAK) && \
				((sta)->status == WBD_STA_STATUS_WEAK)) \
			((info)->weak_client_count)--; \
		(sta)->status = (value); \
	} while (0)

/* Revive Me, update Slave's last_active timestamp */
#define WBD_DS_UPDATE_LAST_ACTIVE_TIME(slave_item)	 \
		do { \
			(slave_item)->last_active_ts = time(NULL); \
			(slave_item)->marked_inactive = FALSE; \
		} while (0)

/* ------------------------------ Macros to Update Structure ------------------------------- */

/* --------------------------------- Macros to Get Values ---------------------------------- */

/* Traverse list of blankets to find a Master */
extern wbd_master_info_t* wbd_ds_find_master_in_blanket_master(wbd_blanket_master_t *wbd_slave,
	uint8 bkt_id, int* error);

/* Traverse Master Application Info's br0 Master Info List to find Master, with band valid check */
#define WBD_DS_GET_MASTER_INFO(info, bkt_id, master, find_ret) \
		do { \
			(master) = wbd_ds_find_master_in_blanket_master(((info)->wbd_master), \
				(bkt_id), (find_ret)); \
		} while (0)

/* Traverse Master Application Info's br0 Master Info List to find a Master + Init new Master */
#define WBD_SAFE_GET_MASTER_INFO(info, bkt_id, master, find_ret) \
		do { \
			WBD_DS_GET_MASTER_INFO((info), (bkt_id), (master), (find_ret)); \
			if ((*(find_ret)) == WBDE_DS_UNKWN_MSTR) { \
				ret = WBDE_DS_UNKWN_MSTR; \
				goto end; \
			} \
		} while (0)

/* Traverse Slave Application Info's br0 Slave Info List to find a Slave */
#define WBD_DS_GET_SLAVE_ITEM(info, mac, cmp, slave, find_ret) \
		do { \
			(slave) = wbd_ds_find_slave_addr_in_blanket_slave(((info)->wbd_slave), \
				(mac), (cmp), (find_ret)); \
		} while (0)

/* Traverse Slave Application Info's br0 Slave Info List to find a Slave, with Error check */
#define WBD_SAFE_GET_SLAVE_ITEM(info, mac, cmp, slave, find_ret) \
		do { \
			WBD_DS_GET_SLAVE_ITEM((info), (mac), (cmp), (slave), (find_ret)); \
			if ((*(find_ret)) == WBDE_DS_UNKWN_SLV) { \
				ret = WBDE_DS_UNKWN_SLV; \
				goto end; \
			} \
		} while (0)

/* Get Total Weak Client Count For a Master/Slave info */
#define WBD_DS_GET_WEAK_CLIENT_COUNT(info) ((info)->weak_client_count)

/* Send any command from Slave, if Slave is joined to Master */
#define WBD_ASSERT_SLAVE_JOINED(slave) \
	do { \
		if ((!(slave)) || (!((slave)->wbd_ifr.enabled)) || (!((slave)->join_status))) { \
			WBD_WARNING("%s\n", wbderrorstr(WBDE_AGENT_NT_JOINED)); \
			ret = WBDE_AGENT_NT_JOINED; \
			goto end; \
		} \
	} while (0)

/* Helper Macro to get the self device, with error check and error handling */
#define WBD_SAFE_GET_I5_SELF_DEVICE(device, find_ret) \
		do { \
			(device) = wbd_ds_get_self_i5_device((find_ret)); \
			if ((*(find_ret)) == WBDE_DS_UN_DEV) { \
				ret = WBDE_DS_UN_DEV; \
				goto end; \
			} \
		} while (0)

/* Helper Macro to get a device, matching a Device AL_MAC, with error check and error handling */
#define WBD_SAFE_FIND_I5_DEVICE(device, device_al_mac, find_ret) \
		do { \
			(device) = wbd_ds_get_i5_device((device_al_mac), (find_ret)); \
			if ((*(find_ret)) == WBDE_DS_UN_DEV) { \
				ret = WBDE_DS_UN_DEV; \
				WBD_WARNING("Device["MACDBG"] : %s\n", MAC2STRDBG(device_al_mac), \
					wbderrorstr(ret)); \
				goto end; \
			} \
		} while (0)

/* Helper Macro to get the controller device, with error check and error handling */
#define WBD_SAFE_GET_I5_CTLR_DEVICE(device, find_ret) \
		do { \
			(device) = wbd_ds_get_controller_i5_device((find_ret)); \
			if ((*(find_ret)) == WBDE_AGENT_NT_JOINED) { \
				WBD_WARNING("%s\n", wbderrorstr(WBDE_AGENT_NT_JOINED)); \
				ret = WBDE_AGENT_NT_JOINED; \
				goto end; \
			} \
		} while (0)

/* Traverse I5's Interface List to find a Interface of the self device */
#define WBD_DS_GET_I5_SELF_IFR(mac, i5_ifr, find_ret) \
		do { \
			(i5_ifr) = wbd_ds_get_self_i5_interface((mac), (find_ret)); \
		} while (0)

/* Traverse I5's Interface List to find a Interface of the self device, with Error check */
#define WBD_SAFE_GET_I5_SELF_IFR(mac, i5_ifr, find_ret) \
		do { \
			(i5_ifr) = wbd_ds_get_self_i5_interface((mac), (find_ret)); \
			if ((*(find_ret)) != WBDE_OK) { \
				ret = (*(find_ret)); \
				goto end; \
			} \
		} while (0)

/* Traverse I5's Interface List to find a Interface of the device, with Error check */
#define WBD_SAFE_GET_I5_IFR(device_id, mac, i5_ifr, find_ret) \
		do { \
			(i5_ifr) = wbd_ds_get_i5_interface((device_id), (mac), (find_ret)); \
			if ((*(find_ret)) != WBDE_OK) { \
				ret = (*(find_ret)); \
				goto end; \
			} \
		} while (0)

/* Traverse I5's BSS List to find a BSS of the self device, with Error check */
#define WBD_DS_GET_I5_SELF_BSS(bssid, i5_bss, find_ret) \
		do { \
			(i5_bss) = wbd_ds_get_self_i5_bss((bssid), (find_ret)); \
		} while (0)

/* Traverse I5's BSS List to find a BSS of the self device, with Error check */
#define WBD_SAFE_GET_I5_SELF_BSS(bssid, i5_bss, find_ret) \
		do { \
			(i5_bss) = wbd_ds_get_self_i5_bss((bssid), (find_ret)); \
			if ((*(find_ret)) != WBDE_OK) { \
				ret = (*(find_ret)); \
				goto end; \
			} \
		} while (0)

/* Traverse I5's Device to find a BSS in any Interface, with Error check */
#define WBD_SAFE_FIND_I5_BSS_IN_DEVICE(device_al_mac, bssid, i5_bss, find_ret) \
		do { \
			(i5_bss) = wbd_ds_get_i5_bss((device_al_mac), \
					(bssid), (find_ret)); \
			if (((*(find_ret)) != WBDE_OK) || !(i5_bss)) { \
				ret = (*(find_ret)); \
				WBD_WARNING("Device["MACDBG"] BSSID["MACDBG"] %s\n", \
					MAC2STRDBG(device_al_mac), MAC2STRDBG(bssid), \
					wbderrorstr(ret)); \
				goto end; \
			} \
		} while (0)

/* Traverse I5's Device to find a BSS in any Interface based on band and MAPFLags(Fronthaul or
 * Backhaul), without Error check
 */
#define WBD_DS_FIND_I5_BSS_IN_DEVICE_FOR_BAND_AND_MAPFLAG(i5_device, band, i5_bss, map, find_ret) \
		do { \
			(i5_bss) = wbd_ds_get_i5_bss_in_device_for_band_and_mapflag((i5_device), \
					(band), (map), (find_ret)); \
		} while (0)

/* Traverse I5's Interfaces to find a BSS, with Error check */
#define WBD_SAFE_FIND_I5_BSS_IN_IFR(i5_ifr, bssid, i5_bss, find_ret) \
		do { \
			(i5_bss) = wbd_ds_get_i5_bss_in_ifr((i5_ifr), \
					(bssid), (find_ret)); \
			if (((*(find_ret)) != WBDE_OK) || !(i5_bss)) { \
				ret = (*(find_ret)); \
				WBD_WARNING("BSSID["MACDBG"] %s\n", \
					MAC2STRDBG(bssid), \
					wbderrorstr(ret)); \
				goto end; \
			} \
		} while (0)

/* --------------------------------- Macros to Get Values ---------------------------------- */

/* ----------------------------- Extern Function Declaration -------------------------------- */
/* Initialize generic list */
extern void wbd_ds_glist_init(wbd_glist_t *list);
/* Append a node to generic list */
extern void wbd_ds_glist_append(wbd_glist_t *list, dll_t *new_obj);
/* Delete a node from generic list */
extern void wbd_ds_glist_delete(wbd_glist_t *list, dll_t *obj);
/* Delete all the node from generic list */
extern int wbd_ds_glist_cleanup(wbd_glist_t *list);

/* Allocate & Initialize Blanket Master structure object */
extern int wbd_ds_blanket_master_init(wbd_info_t *info);
/* Free & Cleanup Blanket Master structure object from heap */
extern int wbd_ds_blanket_master_cleanup(wbd_info_t *info);

/* Allocate & Initialize Master Info structure object */
extern wbd_master_info_t* wbd_ds_master_info_init(uint8 bkt_id, char *bkt_name, int *error);
/* Create & Add a new Master Info to Blanket Master for specific blanket ID */
extern int wbd_ds_create_master_for_blanket_id(wbd_blanket_master_t *wbd_master, uint8 bkt_id,
	char *bkt_name, wbd_master_info_t **out_master);
/* Free Master Info structure object from heap */
extern int wbd_ds_master_info_cleanup(wbd_master_info_t *master_info, bool free_master_mem);

/* Allocate & Initialize Blanket Slave structure object */
extern int wbd_ds_blanket_slave_init(wbd_info_t *info);
/* Free & Cleanup Blanket Slave structure object from heap */
extern int wbd_ds_blanket_slave_cleanup(wbd_info_t *info);

/* Allocate & Initialize Slave Item structure object */
extern wbd_slave_item_t* wbd_ds_slave_item_init(int band, int *error);
/* Create & Add a new Slave item to Blanket Slave for specific band */
extern int wbd_ds_create_slave_for_band(wbd_blanket_slave_t *wbd_slave, int band,
	wbd_slave_item_t **out_slave);
#ifdef PLC_WBD
/* Remove all PLC STA items on this Slave */
extern int wbd_ds_slave_item_cleanup_plc_sta_list(wbd_slave_item_t *slave_item);
#endif /* PLC_WBD */
/* Free Slave Item structure object from heap */
extern int wbd_ds_slave_item_cleanup(wbd_slave_item_t *slave_item, bool free_slave_mem);

/* Initialize stamon module for Slave's interface */
extern int wbd_ds_slave_stamon_init(wbd_slave_item_t *slave_item);
/* Cleanup stamon module for Slave's interface */
extern void wbd_ds_slave_stamon_cleanup(wbd_slave_item_t *slave_item);

/* Free taf' list */
extern int wbd_ds_slave_free_taf_list(wbd_taf_params_t* taf);

/* Cleanup STA bounce table */
extern void wbd_ds_cleanup_sta_bounce_table(wbd_blanket_master_t *wbd_master);
/* Cleanup all Beacon reports */
extern void wbd_ds_cleanup_beacon_reports(wbd_info_t *info);
/* Cleanup probe STA table */
void wbd_ds_cleanup_prb_sta_table(wbd_info_t *info);

/* Traverse Blanket Slave's br0 list to find a Slave */
extern wbd_slave_item_t* wbd_ds_find_slave_addr_in_blanket_slave(wbd_blanket_slave_t *wbd_slave,
	struct ether_addr *find_slave_addr, int cmp_bssid, int* error);
/* Traverse Master Info's Slave List to find a Slave */
extern wbd_slave_item_t* wbd_ds_find_slave_in_master(wbd_master_info_t *master_info,
	struct ether_addr *find_slave_addr, int cmd_bssid, int *error);
/* Traverse BSS's Monitor STA List to find an Monitored STA */
extern wbd_monitor_sta_item_t* wbd_ds_find_sta_in_bss_monitorlist(i5_dm_bss_type *i5_bss,
	struct ether_addr *find_sta_mac, int* error);
/* Traverse Maclist to find MAC address exists or not */
extern wbd_mac_item_t* wbd_ds_find_mac_in_maclist(wbd_glist_t *list, struct ether_addr *find_mac,
	int* error);
/* Find the STA MAC entry in Master's STA Bounce Table */
extern wbd_sta_bounce_detect_t* wbd_ds_find_sta_in_bouncing_table(wbd_blanket_master_t *wbd_master,
	struct ether_addr *addr);
/* Count how many BSSes has the STA entry in Controller Topology.
 * Also get the assoc_time of STA which is associated to the BSS most recently.
 */
extern int wbd_ds_find_duplicate_sta_in_controller(struct ether_addr *sta_mac,
	struct timeval *out_assoc_time);
/* Traverse Beacon reports list and find the beacon request based on sta_mac */
extern wbd_beacon_reports_t* wbd_ds_find_item_fm_beacon_reports(wbd_info_t *info,
	struct ether_addr *sta_mac, int *error);
/* Find the STA MAC entry in Master's Probe STA Table */
extern wbd_prb_sta_t *wbd_ds_find_sta_in_probe_sta_table(wbd_info_t *info,
	struct ether_addr *addr, bool enable);

/* Removes a Slave item from Blanket Master */
extern int wbd_ds_remove_slave_fm_blanket_master(wbd_blanket_master_t *wbd_master,
	struct ether_addr *slave_bssid);
/* Removes a Slave item from Master Info */
extern int wbd_ds_remove_slave(wbd_master_info_t *master_info,
	struct ether_addr *slave_bssid, wbd_trigger_t *out_trigger);

/* Add a STA item in parent BSS's Monitor STA List */
extern int wbd_ds_add_sta_in_bss_monitorlist(i5_dm_bss_type *i5_bss,
	struct ether_addr *bssid, struct ether_addr *new_sta_mac,
	wbd_monitor_sta_item_t **out_sta_item);
/* Add a STA item in all peer Devices' Monitor STA List. map_flags will tell whether to add it to
 * Fronthaul BSS or Bakhaul BSS
 */
extern int wbd_ds_add_sta_in_peer_devices_monitorlist(wbd_master_info_t *master_info,
	i5_dm_clients_type *i5_assoc_sta, uint8 map_flags);
/* Add a STA item in parent Slave's Assoc STA List and all peer Slaves' Monitor STA List */
extern int wbd_ds_add_sta_in_controller(wbd_blanket_master_t *wbd_master,
	i5_dm_clients_type *i5_assoc_sta);
/* Add a STA to monitorlist of BSS from the assoclist of peer BSS */
extern int wbd_ds_add_monitorlist_fm_peer_devices_assoclist(i5_dm_bss_type *i5_bss);
/* Add a MAC address to maclist */
extern int wbd_ds_add_mac_to_maclist(wbd_glist_t *list, struct ether_addr *mac,
	wbd_mac_item_t **out_mac_item);
/* Add STA to the bouncing table */
extern int wbd_ds_add_sta_to_bounce_table(wbd_blanket_master_t *wbd_master, struct ether_addr *addr,
	struct ether_addr *src_addr, uint8 flags);
/* Add an item to beacon reports list */
extern wbd_beacon_reports_t* wbd_ds_add_item_to_beacon_reports(wbd_info_t *info,
	struct ether_addr *neighbor_al_mac, time_t timestamp, struct ether_addr *sta_mac);
/* add probe STA item */
extern void wbd_ds_add_item_to_probe_sta_table(wbd_info_t *info,
	struct ether_addr *addr, uint8 band);

/* Remove a STA item from parent Slave's Monitor STA List */
extern int wbd_ds_remove_sta_fm_bss_monitorlist(i5_dm_bss_type *i5_bss,
	struct ether_addr *new_sta_mac);

/* Remove a STA item from all peer BSS' Monitor STA List */
extern int wbd_ds_remove_sta_fm_peer_devices_monitorlist(struct ether_addr * parent_slave_bssid,
	struct ether_addr *sta_mac, uint8 mapFlags);
/* Remove beacon request */
extern int wbd_ds_remove_beacon_report(wbd_info_t *info, struct ether_addr *sta_mac);

/* Update slave in master's slavelist */
extern void wbd_ds_update_slave_item(wbd_slave_item_t *dst, wbd_slave_item_t *src, int updateflag);
/* Update the bouncing table */
extern void wbd_ds_update_sta_bounce_table(wbd_blanket_master_t *wbd_master);
/* Increment the Bounce Count of a STA entry in Bounce Table */
extern int wbd_ds_increment_bounce_count_of_entry(wbd_blanket_master_t *wbd_master,
	struct ether_addr *addr, struct ether_addr *dst_mac, wbd_wl_sta_stats_t *sta_stats);

/* Add logs info in the blanket master. */
extern void wbd_ds_add_logs_in_master(wbd_blanket_master_t *blanket_master_info, char *logmsg);
/* Fetch logs info from the blanket master. */
extern int wbd_ds_get_logs_from_master(wbd_blanket_master_t *blanket_master_info,
	char *logs, int len);
/* ----------------------------- Extern Function Declaration -------------------------------- */

/* Get the parent of the i5 list */
#define WBD_I5LL_PARENT(x)	((x)->ll.parent)

/* Get the Next item from the i5 list */
#define WBD_I5LL_NEXT(x)	((x)->ll.next)

#define WBD_I5LL_HEAD(x)	((x).ll.next)

/* List of channels in the operating class for AP channel report in beacon request */
typedef struct wbd_bcn_req_chan_list {
	dll_t node;		/* self referencial (next,prev) pointers of type dll_t */
	unsigned char channel;	/* channel */
} wbd_bcn_req_chan_list_t;

/* List of operating classes and channels in it for the AP channel report in beacon request */
typedef struct wbd_bcn_req_rclass_list {
	dll_t node;		/* self referencial (next,prev) pointers of type dll_t */
	unsigned char rclass;	/* Operating class of the channels */
	wbd_glist_t chan_list;	/* List of channels of type wbd_bcn_req_chan_list_t */
} wbd_bcn_req_rclass_list_t;

/* Helper function to check if device investigated is self_device or not  */
bool wbd_ds_is_i5_device_self(unsigned char *device_id);

/* Helper function to get the controller device */
i5_dm_device_type* wbd_ds_get_controller_i5_device(int *error);

/* Helper function to get the device from al_mac */
i5_dm_device_type *wbd_ds_get_i5_device(unsigned char *device_id, int *error);

/* Helper function to get the self device */
i5_dm_device_type *wbd_ds_get_self_i5_device(int *error);

/* Helper Function to find interface using AL mac and interface mac */
i5_dm_interface_type *wbd_ds_get_i5_interface(unsigned char *device_id,
	unsigned char *if_mac, int *error);

/* Helper Function to find interface in the self device */
i5_dm_interface_type *wbd_ds_get_self_i5_interface(unsigned char *mac, int *error);

/* Helper Function to find interface using i5_device and interface mac */
i5_dm_interface_type* wbd_ds_get_i5_ifr_in_device(i5_dm_device_type *i5_device,
	unsigned char *if_mac, int *error);

/* Helper Function to find self interface from ifname */
i5_dm_interface_type *wbd_ds_get_self_i5_ifr_from_ifname(char *ifname, int *error);

/* Helper function to find BSS using al_mac and bssid */
i5_dm_bss_type* wbd_ds_get_i5_bss(unsigned char *device_id, unsigned char *bssid, int *error);

/* Helper function to find BSS in the self device */
i5_dm_bss_type *wbd_ds_get_self_i5_bss(unsigned char *bssid, int *error);

/* Find BSS using Device and BSSID */
i5_dm_bss_type* wbd_ds_get_i5_bss_in_device(i5_dm_device_type *i5_device, unsigned char *bssid,
	int *error);

/* Find BSS using Device and band */
i5_dm_bss_type* wbd_ds_get_i5_bss_in_device_for_band_and_mapflag(i5_dm_device_type *i5_device,
	int band, uint8 map, int *error);

/* Find BSS from complete topology */
i5_dm_bss_type* wbd_ds_get_i5_bss_in_topology(unsigned char *bssid, int *error);

/* Find BSS using i5_dm_interface_type and BSSID */
i5_dm_bss_type* wbd_ds_get_i5_bss_in_ifr(i5_dm_interface_type *i5_ifr,
	unsigned char *bssid, int *error);

/* Traverse IEEE1905 BSS's Assoc STA List to find an Associated STA */
i5_dm_clients_type *wbd_ds_find_sta_in_bss_assoclist(i5_dm_bss_type *i5_bss,
	struct ether_addr *find_sta_mac, int *error, wbd_assoc_sta_item_t **assoc_sta);

/* Find the sta in the i5 typology */
i5_dm_clients_type* wbd_ds_find_sta_in_topology(unsigned char *sta_mac, int *err);

/* Find the sta in the i5 Device */
i5_dm_clients_type* wbd_ds_find_sta_in_device(i5_dm_device_type *i5_device,
	unsigned char *sta_mac, int *err);

/* This Init the device on the topology */
int wbd_ds_device_init(i5_dm_device_type *i5_device);

/* This Deinit the device on the topology */
int wbd_ds_device_deinit(i5_dm_device_type *i5_device);

/* This Init the interface on the device */
int wbd_ds_interface_init(i5_dm_interface_type *i5_ifr);

/* This Deinit the interface on the device */
int wbd_ds_interface_deinit(i5_dm_interface_type *i5_ifr);

/* This Init the BSS on the interface */
extern int wbd_ds_bss_init(i5_dm_bss_type *i5_bss);

/* This Deinit the BSS on the interface */
extern int wbd_ds_bss_deinit(i5_dm_bss_type *i5_bss);

/* This Init the STA on the BSS */
extern void wbd_ds_sta_init(wbd_info_t *info, i5_dm_clients_type *i5_assoc_sta);

/* This Deinit the STA on the BSS */
extern void wbd_ds_sta_deinit(i5_dm_clients_type *i5_assoc_sta);

/* Check if FBT is possible on this 1905 Device or not */
extern int wbd_ds_is_fbt_possible_on_agent();

/* Traverse rclass list to find the rclass */
extern wbd_bcn_req_rclass_list_t* wbd_ds_find_rclass_in_ap_chan_report(
	wbd_glist_t *ap_chan_report, unsigned char rclass, int* error);

/* Traverse channel list of rclass to find the channel */
extern wbd_bcn_req_chan_list_t* wbd_ds_find_channel_in_rclass_list(
	wbd_bcn_req_rclass_list_t *rclass_list, unsigned char channel, int* error);

/* Add an rclass in ap channel report */
extern int wbd_ds_add_rclass_in_ap_chan_report(wbd_glist_t *ap_chan_report,
	unsigned char rclass, wbd_bcn_req_rclass_list_t** out_rclass_list);

/* Add an channel in rclass list */
extern int wbd_ds_add_channel_in_rclass_list(wbd_bcn_req_rclass_list_t *rclass_lsit,
	unsigned char channel, wbd_bcn_req_chan_list_t** out_channel_list);

/* Free & Cleanup ap channel report from heap */
extern int wbd_ds_ap_chan_report_cleanup(wbd_glist_t *ap_chan_report);

/* timeout probe STA list */
extern void wbd_ds_timeout_prbsta(bcm_usched_handle *hdl, void *arg);

/* Check if the given interface is dedicated backhaul */
extern int wbd_ds_is_interface_dedicated_backhaul(i5_dm_interface_type *interface);

/* Count the number of Fronthaul BSSs in the network */
extern int wbd_ds_count_fhbss();

/* Count the number of backhaul STAs in the network */
extern int wbd_ds_count_bstas();

/* Does the backhaul STA device has the backhaul BSS in it */
extern int wbd_ds_is_bsta_device_has_bbss(unsigned char *sta_mac);

/* Get STA for which the backhaul optimization is pending */
extern i5_dm_clients_type *wbd_ds_get_bh_opt_pending_sta();

/* Get backhaul optimization STA based on the assoc item flags */
extern i5_dm_clients_type *wbd_ds_get_bh_opt_sta(uint32 flags);

/* Unset the backhaul optimization flags of all the backhaul STAs */
extern void wbd_ds_unset_bh_opt_flags(uint32 flags);
#endif /* _WBD_DS_H_ */
