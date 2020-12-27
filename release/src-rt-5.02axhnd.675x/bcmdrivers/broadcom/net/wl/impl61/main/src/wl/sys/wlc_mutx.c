/*
 * MU-MIMO transmit module for Broadcom 802.11 Networking Adapter Device Drivers
 *
 * Copyright 2019 Broadcom
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
 * $Id: wlc_mutx.c $
 */

/* This module manages multi-user MIMO transmit configuration and state.
 *
 * Terminology:
 *  MU group - As defined by 802.11ac, one of 62 MU-MIMO groups, numbered from 1 to 62,
 *  that identifies the set of STAs included in an MU-PPDU and the position of each in
 *  the VHT-SIG-A NSTS field. A STA can be a member of multiple groups. A group ID can
 *  be overloaded, such that the same group ID identifies different combinations of STAs;
 *  however, a sta that appears in multiple overloaded groups must be in the same user
 *  position in each group with the same group ID.
 *
 *  User position - An integer from 0 to 3 that specifies which of the 4 NSTS (#space time streams)
 *  fields in VHT-SIG-A applies to a given user for a given group ID.
 *
 *  MU Capable STA - A currently associated VHT STA that has advertised MU beamformee capability.
 *
 *  MU Client Set - A set of N MU-capable STAs currently selected to receive MU transmission
 *  from this device. N is a fixed number determined by hardware/memory limitations.
 *
 *  Client index - An integer that uniquely identifies a STA that is a member of the current
 *  MU client set. The client index is used as an index into the set of MU tx FIFOs and
 *  as an index into the blocks of shared memory that communicate MU user configuration and
 *  state between the driver and microcode.
 *
 * This module selects the set of MU client STAs from among all MU-capable STAs. The
 * maximum number of MU clients is limited by hardware. The initial implementation
 * simply selects the first N MU-capable STAs that associate to the AP. These STAs
 * remain MUC clients as long as they remain associated and MU-capable. If a MU client
 * disassociates or loses its MU capability, then another MU capable STA is chosen
 * to replace it. The module is written to support a more intelligent selection of
 * MU clients, but development of an algorithm to do MU client selection is deferred
 * to a future release.
 *
 * This module provides APIs to get and set a STA's MU group membership status and
 * user position for an MU-MIMO group ID. Currently, the group membership and
 * user positions are static. The static values are coded into this module. The
 * APIs support a potential future capability for the hardware to dynamically
 * make group membership assignments and communicate those assignments back to the
 * driver.
 *
 * This module includes functions that construct and send Group ID Management frames
 * to MU clients to inform the client of its group membership and user position
 * within each group.
 *
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <osl.h>
#include <wlc_types.h>
#include <siutils.h>
#include <bcmwifi_channels.h>
#include <wlioctl.h>
#include <wlc_pub.h>
#include <wlc.h>
#include <wl_dbg.h>
#include <wlc_pcb.h>
#include <wlc_lq.h>
#include <wlc_scb.h>
#include <wlc_txbf.h>
#include <wlc_vht.h>
#include <wlc_txc.h>
#include <wlc_mutx.h>
#include <wlc_bmac.h>
#include <wlc_tx.h>
#include <wlc_ampdu.h>
#include <wl_export.h>
#include <wlc_hrt.h>
#include <wlc_scb_ratesel.h>
#include <wlc_dump.h>
#include <wlc_stf.h>
#include <wlc_test.h>
#include <phy_api.h>
#include <wlc_apps.h>
#include <wlc_ratelinkmem.h>
#include <wlc_scan.h>
#include <wlc_vasip.h>
#include <wlc_macreq.h>
#include <wlc_txcfg.h>
#ifdef WLTAF
#include <wlc_taf.h>
#endif // endif
#include <wlc_he.h>
#include <wlc_musched.h>

#ifdef WL_MU_TX

/* Maximum VHT MCS index. MCS 0 - 11, up to 4 streams */
#define MUTX_MCS_NUM  12
#define MUTX_NSS_NUM   4
#define MUTX_MCS_INDEX_NUM  (MUTX_MCS_NUM*MUTX_NSS_NUM)
#define MUTX_MAXGNUM	4	/* The max # of users in one group */
#define MUTX_MAXMUTXCNT	8	/* The max # of mutxcnt including retries until SU switch */

#define MUCLIENT_NRX_1 1
#define MUCLIENT_NRX_2 2
#define MUCLIENT_NRX_MAX 2

/* Few Thresholds for MU sounding report SNR calibration */
#define MPDU_THRES_SETTLE 128
#define MPDU_THRES_HIGH 6400
#define MPDU_THRES_LOW 320
#define MCS_MIN 0
#define MCS_MAX 9
#define PHYREG_M2V_MSG0 0x109c
#define ADJ_IDX(index) (index + 8)

/* iovar table */
enum wlc_mutx_iov {
	IOV_MUTX_UT = 1,
	IOV_MUTX_PKTENG = 2,
	IOV_MUTX_PKTENG_ADDSTA = 3,
	IOV_MUTX_PKTENG_CLRSTA = 4,
	IOV_MUTX_PKTENG_TX = 5,
	IOV_MUTX_SENDGID = 6,
	IOV_MU_POLICY = 7,
	IOV_MUTX_MUINFO = 8,
	IOV_MU_SNR_CALIB = 9,
	IOV_MU_FEATURES = 10,
	IOV_HEMU_RSSI_THRESH = 11,
	IOV_MUTX_LAST
};

/* Max number of times the driver will send a Group ID Mgmt frame to
 * a STA when the STA does not ACK.
 */
#define MU_GID_SEND_LIMIT  3

/* Minimum number of transmit chains for MU TX to be active */
#define MUTX_TXCHAIN_MIN   3

static const bcm_iovar_t mutx_iovars[] = {
	{"mutx_ut", IOV_MUTX_UT, 0, 0, IOVT_UINT32, 0},
#ifdef WL_MUPKTENG
	{"mutx_pkteng", IOV_MUTX_PKTENG, 0, 0, IOVT_BOOL, 0},
	{"mupkteng_addsta", IOV_MUTX_PKTENG_ADDSTA, 0, 0, IOVT_BUFFER, 0},
	{"mupkteng_clrsta", IOV_MUTX_PKTENG_CLRSTA, 0, 0, IOVT_BUFFER, 0},
	{"mupkteng_tx", IOV_MUTX_PKTENG_TX, 0, 0, IOVT_BUFFER, 0},
#endif // endif
#if defined(BCMDBG) || defined(BCMDBG_MU)
	{"mutx_sendgid", IOV_MUTX_SENDGID, 0, 0, IOVT_BOOL, 0},
#endif // endif
	{"mu_snr_calib", IOV_MU_SNR_CALIB, 0, 0, IOVT_BOOL, 0},
	{"mu_policy", IOV_MU_POLICY, 0, 0, IOVT_BUFFER, 0},
	{"muinfo", IOV_MUTX_MUINFO,
	(IOVF_SET_UP), 0, IOVT_BUFFER, 0
	},
	{"mu_features", IOV_MU_FEATURES,
	(0), 0, IOVT_UINT32, 0
	},
#if defined(BCMDBG) || defined(BCMDBG_MU)
	{"hemu_rssi_thresh", IOV_HEMU_RSSI_THRESH,
	(IOVF_SET_DOWN), 0, IOVT_INT32, 0
	},
#endif // endif
	{NULL, 0, 0, 0, 0, 0 }
};

/* By default, each MU client is a member of groups 1 - 9. */
static uint8 dflt_membership[MUCLIENT_NUM][MU_MEMBERSHIP_SIZE] = {
	{ 0xFE, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0xFE, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0xFE, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0xFE, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0xFE, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0xFE, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0xFE, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0xFE, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
	};

/* for 2x2 chips we only use groups 1 - 3 */
static uint8 dflt_membership_2x2[MUCLIENT_NUM][MU_MEMBERSHIP_SIZE] = {
	{ 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
	};

/* The default user position for each of the 8 MU clients. */
static uint8 dflt_position[MUCLIENT_NUM][MU_POSITION_SIZE] = {
	{ 0x00, 0x18, 0x00, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
	{ 0x00, 0x6C, 0x05, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
	{ 0x94, 0x01, 0x08, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
	{ 0xD4, 0x42, 0x0D, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
	{ 0x68, 0x95, 0x02, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
	{ 0x78, 0xA6, 0x07, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
	{ 0xAC, 0xFB, 0x0A, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
	{ 0xFC, 0xFF, 0x0F, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 }
};

/* The default user position for each of the 8 MU clients for 2x2 AP. */
static uint8 dflt_position_2x2[MUCLIENT_NUM][MU_POSITION_SIZE] = {
	{ 0x00, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
	{ 0x04, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
	{ 0x10, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
	{ 0x14, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
	{ 0x40, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
	{ 0x44, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
	{ 0x50, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
	{ 0x54, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 }
};

typedef struct mutx_scb_list {
	struct mutx_scb_list *next, *prev, *link_p, *link_n;
	void *data;
	bool blcklst;
} mutx_scb_list_t;

/* State structure for the MU-MIMO module created by
 * wlc_mutx_module_attach().
 */
struct wlc_mutx_info {
	osl_t *osh;              /* OSL handle */
	wlc_info_t *wlc;         /* wlc_info_t handle */
	wlc_pub_t *pub;          /* wlc_pub_t handle */

	int scb_handle;			     /* scb cubby handle to retrieve data from scb */

	/* TRUE if MU transmit is operational. To be operational, the phy must support MU BFR,
	 * MU BFR must be enabled by configuration, and there must be nothing in the system
	 * state that precludes MU TX, such as having too few transmit chains enabled.
	 */
	bool mutx_active;

	/* TRUE if the current set of MU clients is considered stale and a new
	 * set should be selected.
	 */
	bool muclient_elect_pending;

	/* Current set of MU clients. The index to this array is the client_index
	 * member of mutx_scb_t. If fewer than MUCLIENT_NUM associated clients
	 * are MU capable, then the array is not full. Non-empty elements may follow
	 * empty elements. This happens when a MU client is removed (disassociates
	 * or loses MU capability).
	 */
	scb_t *mu_clients[MUCLIENT_NUM];

#ifdef WLCNT
	/* Count of memory allocation failures for scb cubbies */
	uint16 mutx_scb_alloc_fail;
#endif // endif
	bool mutx_pkteng; /* mupkteng -- debug feature */
	/* below are reported from txstatus */
	/* these counters also count for users now switched to SU */
	uint32 mutx_cnt; /* tx'd as MU */
	uint32 mu_ncons; /* queued as MU and consumed as either MU/SU */
	uint32 mu_nlost; /* queued as MU and lost as either MU/SU */
	mutx_scb_list_t *on_hold_list;
	mutx_scb_list_t *admit_list; /* mu capable stas currenty admitted */
	mutx_scb_list_t *candidate_list; /* mu capable stas which match all policy. */
	mutx_scb_list_t *alternate_list; /* mu capable stas which only match bw_policy. */
	mutx_scb_list_t *evictee_list;
	uint32 mutx_client_scheduler_dur;
	uint32 mutx_client_scheduler_cnt;
	bool mutx_client_scheduler;
	bool muclient_pfmon;
	bool mu_snr_calib;
	bool pfmon_gpos;
	bool mutx_client_samebw;
	bool ac_policy_on;
	uint8 txstats_nupd;
	mutx_scb_list_t *blcklst_bkt;

	bcm_notif_h mutx_state_notif_hdl;	/* mutx state notifier handle. */
	bool mutx_switch_in_progress;
	uint8 muclient_nrx;
	uint8 num_hemmu_usrs[BW_MAXMHZ];
	uint8 min_hemmu_usrs;
	int hemu_rssi_thresh;
};

#ifdef WLCNT
typedef struct mutx_stats {

	/* Number of data frames sent for each group ID. Counts both SU and MU GIDs. */
	uint32 gid_cnt[MIMO_GROUP_NUM];

	/* Last sent rate for each group ID. The rate value is represented as
	 * 2 bits of NSS (actual NSS - 1) followed by 4 bits of MCS.
	 */
	uint8 gid_last_rate[MIMO_GROUP_NUM];

	uint32 mu_ncons; /* queued as MU and consumed as either MU/SU */
	uint32 mu_nlost; /* queued as MU and lost as either MU/SU */
	uint32 gnum[MUTX_MAXGNUM];
	uint32 gpos_cnt[MUTX_MAXGNUM];
	uint32 mutxcnt_stat[MUTX_MAXMUTXCNT];
	/* Below indexed by mcs + ((nss - 1) * mcs_num) */
	/* Total mu frame transmission attempts */
	uint32 txmu[MUTX_MCS_INDEX_NUM];
	/* Total mu frame transmissions acked */
	uint32 txmu_succ[MUTX_MCS_INDEX_NUM];
	/* Total mu frames that end up being txed as SU */
	uint32 txsu[MUTX_MCS_INDEX_NUM];
	/* Total mu frames that end up being txed as SU and acked */
	uint32 txsu_succ[MUTX_MCS_INDEX_NUM];
	/* Total mu frames Tx'ed with sgi on */
	uint32 txmu_sgi[MUTX_MCS_INDEX_NUM];
	/* Total mu frame transmissions acked with sgi on */
	uint32 txmu_sgi_succ[MUTX_MCS_INDEX_NUM];
} mutx_stats_t;

typedef struct mutx_pfmon_stats {
	uint32 mu_ncons; /* queued as MU and consumed as either MU/SU */
	uint32 mutx_cnt;   /* tx'd as MU (== SUM of txmu[i]) */
	uint32 gpos_cnt[MUTX_MAXGNUM];
	uint32 mutxcnt_stat[MUTX_MAXMUTXCNT];
	/* Total mu frame transmission attempts */
	uint32 txmu[MUTX_MCS_INDEX_NUM];
} mutx_pfmon_stats_t;

typedef struct mutx_snr_calib_stats {
	int index[2];
	/* Total mu frame transmission attempts */
	uint32 txmu[MUTX_MCS_NUM * 2];
	uint32 txmu_succ[MUTX_MCS_NUM * 2];
	uint32 txmu_total[2];
	uint8 count[2][17];
	bool change[2];
} mutx_snr_calib_stats_t;
#endif   /* WLCNT */

enum {
	MUTX_AC_NONE = 0,
	MUTX_AC_BEBK,
	MUTX_AC_VI,
	MUTX_AC_VO
};

enum {
	MUTX_MUINFO_OPT_NONE = 0,
	MUTX_MUINFO_OPT_SIMPLE = 1,
	MUTX_MUINFO_OPT_DETAIL = 2
};

enum {
	MUTX_REASON_NEW_ASSOC = 0,
	MUTX_REASON_NEW_ADMIT = 1,
	MUTX_REASON_BW_POLICY_MISMATCH = 2,
	MUTX_REASON_AC_POLICY_MISMATCH = 3,
	MUTX_REASON_HIGH_TRAFFIC = 4,
	MUTX_REASON_LOW_TRAFFIC = 5,
	MUTX_REASON_BLACKLIST_ADMIT = 6,
	MUTX_REASON_BLACKLIST_EVICT = 7,
	MUTX_REASON_MAX = 8
};

enum {
	MUTX_POLICY_EVICT_NONE = 0,
	MUTX_POLICY_EVICT_BW = 1,
	MUTX_POLICY_EVICT_AC = 2
};

static char* mutx_reason2str[MUTX_REASON_MAX] = {
	"new association",
	"new admit",
	"bw policy mismatch",
	"ac policy mismatch",
	"high traffic",
	"low traffic",
	"blacklist admit",
	"blacklist evict"
};

/* MU-MIMO tx scb cubby structure. */
typedef struct mutx_scb {
	/* If this STA is in the MU clients set, then client_index is the index
	 * for this STA in the mu_clients[] array above. If this STA is not a
	 * MU client, then client_index is set to MU_CLIENT_INDEX_NONE.
	 */
	uint16 client_index;

	/* Bit mask indicating STA's membership in each MU-MIMO group. Indexed by
	 * group ID. A 1 at index N indicates the STA is a member of group N. Lowest
	 * order bit indicates membership status in group ID 0. Groups 0 and 63 are
	 * reserved for SU.
	 */
	uint8 membership[MU_MEMBERSHIP_SIZE];

	/* Bit string indicating STA's user position within each MU-MIMO group.
	 * A user position is represented by two bits (positions 0 - 3), under the
	 * assumption that no more than 4 MPDUs are included in an MU-PPDU. A
	 * position value is only valid if the corresponding membership flag is set.
	 */
	uint8 position[MU_POSITION_SIZE];

	/* TRUE if the STA's MU group membership or user position within a
	 * group has changed since the last time we successfully sent
	 * a Group ID message to this user.
	 */
	bool membership_change;

	/* Number of times the current Group ID mgmt frame has been sent
	 * to this STA. Used to limit driver-initiated retries.
	 */
	uint8 gid_send_cnt;

	/* Some STAs ack the GID frame but do not behave like having received the GID frames.
	 * Resend the GID management frame as a WAR.
	 */
	uint8 gid_retry_war;

	/* Receive data frame statistics */
#ifdef WLCNT
	mutx_stats_t *mutx_stats;	/* counters/stats */
	mutx_pfmon_stats_t *mutx_pfmon_stats;
	mutx_snr_calib_stats_t *mutx_snr_calib_stats;
#endif /* WLCNT */
	uint8 retry_gid;
	mutx_scb_list_t *item;
#define MU_STA_MAX_HOLD_DUR 60
	uint32 on_hold_dur;
	scb_t *scb;

	/* Weight for selecting the MU candidate to MU client */
	uint32 score;
	uint32 time_score;
	uint32 mutxcnt;
	uint32 mutxcnt_total;

	/* TRUE if this STA is permitted to initialize mx_bfiblk_idx. */
	bool mu_link_permit;

	uint32 ac_policy_scores[MUTX_AC_VO+1];
	int rssi;
	uint32 nrx;
	/* grps the candidate is part of */
	bool blcklst;

	/* the policy evict reason if MU client will be evicted from policy change. */
	uint8 policy_evict;

	/* the reason of MU client admit and eviction */
	uint8 reason;
	uint8 assocd;
} mutx_scb_t;

#define GID_RETRY_DELAY 3

/* MU TX scb cubby is just a pointer to a dynamically allocated structure. Of course,
 * this memory allocation can fail. So generally speaking, the return value of MUTX_SCB
 * must be checked for NULL. One exception: An scb in the MU client set is guaranteed
 * to have anon-NULL mutx_scb.
 */
typedef struct mutx_scb_cubby {
	mutx_scb_t *mutx_scb;
} mutx_scb_cubby_t;

/* The following table lists the aqmfifo allocated to each MU client */
static uint8  mutx_aqmfifo[MUCLIENT_NUM][AC_COUNT] = {
	/* bk, be, vi, vo */
	/* original designed fifo allocation */
	{ 8,  8,  9, 10},
	{11, 11, 12, 13},
	{14, 14, 15, 16},
	{17, 17, 18, 19},
	{20, 20, 21, 22},
	{23, 23, 24, 25},
	{26, 26, 27, 28},
	{29, 29, 30, 31}
};

static uint8  mutx_aqmfifo_70q[MUCLIENT_NUM_16][AC_COUNT] = {
	/* bk, be, vi, vo */
	{ 6,  7,  8,  9},
	{10, 11, 12, 13},
	{14, 15, 16, 17},
	{18, 19, 20, 21},
	{22, 23, 24, 25},
	{26, 27, 28, 29},
	{30, 31, 32, 33},
	{34, 35, 36, 37},
	{38, 39, 40, 41},
	{42, 43, 44, 45},
	{46, 47, 48, 49},
	{50, 51, 52, 53},
	{54, 55, 56, 57},
	{58, 59, 60, 61},
	{62, 63, 64, 65},
	{66, 67, 68, 69}
};

#define MUTX_SCB_CUBBY(mu_info, scb)  ((mutx_scb_cubby_t*)(SCB_CUBBY((scb), (mu_info)->scb_handle)))
#define MUTX_SCB(mu_info, scb)        (mutx_scb_t*)(MUTX_SCB_CUBBY(mu_info, scb)->mutx_scb)

/* Basic module infrastructure */
static int wlc_mutx_switch(wlc_info_t *wlc, bool mutx_feature, bool is_iov);
static int wlc_mu_doiovar(void *hdl, uint32 actionid,
	void *p, uint plen, void *a, uint alen, uint vsize, wlc_if_t *wlcif);
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU) || defined(BCMDBG_MU) \
	|| defined(DUMP_MUTX)
static int wlc_mutx_dump(void *ctx, bcmstrbuf_t *b);
static int wlc_mutx_dump_clr(void *ctx);
#endif // endif
static void wlc_mutx_watchdog(void *mi);
static int wlc_mutx_up(void *mi);
#if defined(BCMDBG) || defined(BCMDBG_MU)
static int wlc_mutx_sendgid(wlc_mutx_info_t *mu_info);
#endif // endif
static void wlc_mutx_state_upd(wlc_info_t *wlc, uint8 state);

/* SCB cubby management */
static void wlc_mutx_scb_state_upd(void *ctx, scb_state_upd_data_t *notif_data);
static mutx_scb_t *mu_scb_cubby_alloc(wlc_mutx_info_t *mu_info, scb_t *scb);
static int mu_scb_cubby_init(void *context, scb_t *scb);
static void mu_scb_cubby_deinit(void *context, scb_t *scb);
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU) || defined(BCMDBG_MU)
static void mu_scb_cubby_dump(void *ctx, scb_t *scb, bcmstrbuf_t *b);
#else
#define mu_scb_cubby_dump NULL
#endif // endif

/* MU client set management */
static int mu_client_set_add(wlc_mutx_info_t *mu_info, scb_t *scb);
static int mu_client_set_remove(wlc_mutx_info_t *mu_info, scb_t *scb);
static void mu_client_set_clear(wlc_mutx_info_t *mu_info);
static bool mu_client_set_full(wlc_mutx_info_t *mu_info);
static void mu_client_set_new_sta(wlc_mutx_info_t *mu_info, scb_t *scb);
static bool mu_in_client_set(wlc_mutx_info_t *mu_info, scb_t *scb);
static int mu_clients_select(wlc_mutx_info_t *mu_info);

static bool mu_sta_mu_capable(wlc_mutx_info_t *mu_info, scb_t *scb);
static bool mu_sta_mu_ready(wlc_mutx_info_t *mu_info, scb_t *scb, bool strict_check);
static int mu_sta_mu_enable(wlc_mutx_info_t *mu_info, scb_t *scb);
static int mu_sta_mu_disable(wlc_mutx_info_t *mu_info, scb_t *scb);
static bool mu_scb_membership_change_get(wlc_info_t *wlc, scb_t *scb);
static void mu_scb_membership_change_set(wlc_info_t *wlc, scb_t *scb, bool pending);

static uint8 mu_candidates_select(wlc_mutx_info_t *mu_info);
static void mu_clients_eviction(wlc_mutx_info_t *mu_info);
#ifdef WL_PSMX
static void wlc_mutx_clr_mubf(wlc_info_t *wlc);
#endif // endif
static bool wlc_mutx_client_scheduler(wlc_mutx_info_t * mu_info, bool pfmon);

/* Manage user position, group membership bit arrays */
#ifdef BCMDBG
static void mu_user_pos_set(uint8 *pos_array, uint16 group_id, uint8 user_pos);
#endif // endif
static uint8 mu_user_pos_get(uint8 *pos_array, uint16 group_id);
static bool mu_any_membership(uint8 *membership);

/* Sending group ID management frames */
static int mu_group_id_msg_send(wlc_info_t *wlc, scb_t *scb,
                                uint8 *membership, uint8 *user_pos);
static void wlc_mutx_gid_complete(wlc_info_t *wlc, uint txstatus, void *arg);

#ifdef BCMDBG
static void mutx_ut(wlc_mutx_info_t *mu_info, int32 val);
#endif // endif
#ifdef WL_MUPKTENG
static int wlc_mupkteng_addsta(wlc_mutx_info_t *mu_info, struct ether_addr *ea, int client_idx,
		int nrxchain);
static int wlc_mupkteng_clrsta(wlc_mutx_info_t *mu_info, int client_idx);
static int wlc_mupkteng_tx(wlc_mutx_info_t *mu_info, mupkteng_tx_t *mupkteng_tx);
#endif // endif
static void wlc_muclient_clear_pfmon_stats(wlc_mutx_info_t *mu_info, scb_t *scb);
static void wlc_muclients_clear_pfmon_stats(wlc_mutx_info_t *mu_info);
static uint wlc_muclients_eval_mutxcnt_stats(wlc_mutx_info_t *mu_info);
static uint wlc_muclients_eval_gpos_stats(wlc_mutx_info_t *mu_info);
static void wlc_muclient_pfmon(wlc_mutx_info_t *mu_info);
static int wlc_mutx_fifo_hold_set(wlc_info_t *wlc, void *fifo_bitmap);
static void wlc_mutx_fifo_hold_clr(wlc_info_t *wlc, void *fifo_bitmap);
static void wlc_mutx_client_clr_blcklst_state(wlc_mutx_info_t *mu_info, scb_t *scb);
static void wlc_mutx_clients_clr_blcklst_state(wlc_mutx_info_t *mu_info);

static mutx_scb_list_t *mutx_blcklst_new(wlc_mutx_info_t *mu_info);
static void mutx_blcklst_del(wlc_mutx_info_t *mu_info, mutx_scb_list_t *bkt);
static void wlc_muclient_clear_stats(wlc_mutx_info_t *mu_info, mutx_stats_t *mutx_stats);
static void mutx_blcklst_dump(wlc_mutx_info_t *mu_info, bcmstrbuf_t *b);
static bool mutx_blcklst_match_item(mutx_scb_list_t *item, mutx_scb_list_t *bkt);
static void mutx_blcklst_insert_item(wlc_mutx_info_t *mu_info, mutx_scb_list_t *item,
		mutx_scb_list_t *bkt);
static void mutx_blcklst_del_item(wlc_mutx_info_t *mu_info, mutx_scb_list_t *item);
static  mutx_scb_list_t *mutx_blcklst_match(wlc_mutx_info_t *mu_info, mutx_scb_list_t *head,
	mutx_scb_list_t **blcklst_bkt);
static void wlc_mutx_blcklst_match(wlc_mutx_info_t *mu_info);
static bool wlc_mutx_admit_muclients(wlc_mutx_info_t *mu_info);
static int wlc_mutx_sta_mucidx_set(wlc_mutx_info_t *mu_info, scb_t *scb);
#ifdef MAC_AUTOTXV_OFF
static void wlc_mutx_admit_users_reinit(wlc_mutx_info_t *mutx);
#endif // endif
static void wlc_mutx_he_scb_state_upd(wlc_mutx_info_t *mu_info, scb_state_upd_data_t *notif_data);
static void wlc_mutx_he_admit_clients(wlc_mutx_info_t *mu_info);
static bool wlc_mutx_scb_ishemmu_eligible(wlc_mutx_info_t *mu_info, scb_t* scb);
static void wlc_mutx_hemmu_min_usrs_eval(wlc_mutx_info_t *mu_info, scb_t *scb, bool assocd,
	int8 fbw, int8 bw);

#define LIST_IS_EMPTY(head) \
	(head == head->next)
#define FOREACH_LIST_POS(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); pos = n, n = pos->next)
#define FOREACH_BKT_POS(pos, n, head) \
	for (pos = (head)->link_n, n = pos->link_n; pos != (head); pos = n, n = pos->link_n)

static mutx_scb_list_t*
mutx_scb_list_new(wlc_mutx_info_t *mi, void *data)
{
	mutx_scb_list_t* new;
	new = MALLOCZ(mi->osh, sizeof(*new));
	if (new) {
		new->data = data;
		new->next = new->prev = new;
		new->link_n = new->link_p = new;
	}
	return new;
}

static void
mutx_scb_list_del(wlc_mutx_info_t *mi, mutx_scb_list_t *item)
{
	ASSERT(item != NULL);
	if (item != NULL) {
		item->prev->next = item->next;
		item->next->prev = item->prev;
		item->data = NULL;
		MFREE(mi->osh, item, sizeof(*item));
	}
}

static void
mutx_scb_list_add_tail(mutx_scb_list_t *head, mutx_scb_list_t *new)
{
	ASSERT((head != NULL) && (new != NULL));
	if ((head != NULL) && (new != NULL)) {
		head->prev->next = new;
		new->prev = head->prev;
		head->prev = new;
		new->next = head;
	}
}

static void
mutx_scb_list_rem(mutx_scb_list_t *item)
{
	ASSERT(item != NULL);
	if (item != NULL) {
		item->prev->next = item->next;
		item->next->prev = item->prev;
		item->next = item->prev = item;
	}
}

static mutx_scb_list_t*
mutx_bkt_new(wlc_mutx_info_t *mi, void *data)
{
	mutx_scb_list_t* new;
	new = MALLOCZ(mi->osh, sizeof(*new));
	if (new) {
		new->data = data;
		new->next = new->prev = new;
		new->link_n = new->link_p = new;
	}
	return new;
}

static void
mutx_bkt_del(wlc_mutx_info_t *mi,  mutx_scb_list_t  *item)
{
	ASSERT(item != NULL);
	if (item != NULL) {
		item->link_p->link_n = item->link_n;
		item->link_n->link_p = item->link_p;
		item->data = NULL;
		MFREE(mi->osh, item, sizeof(*item));
	}
}

static void
mutx_bkt_add_tail(mutx_scb_list_t *head, mutx_scb_list_t *new)
{
	ASSERT((head != NULL) && (new != NULL));
	if ((head != NULL) && (new != NULL)) {
		head->link_p->link_n = new;
		new->link_p = head->link_p;
		head->link_p = new;
		new->link_n = head;
	}
}

#define MUTX_SCORE_MAX		0xFFFFFFFF
#define MUTX_SCORE_MIN		0x0

#define MUTX_RSSI_MIN		(-70)

static mutx_scb_list_t*
mutx_scb_list_max(mutx_scb_list_t *head)
{
	uint32 max = MUTX_SCORE_MIN;
	uint32 time = 0;
	int rssi = MUTX_RSSI_MIN;
	mutx_scb_list_t *result = NULL;
	mutx_scb_list_t *item, *next;

	FOREACH_LIST_POS(item, next, head) {
		mutx_scb_t *mu_scb = (mutx_scb_t *)item->data;
		if (!mu_scb)
			continue;
		if ((!result) || (mu_scb->score > max) ||
			((mu_scb->score == max) && (mu_scb->rssi > rssi)) ||
			((mu_scb->score == max) && (mu_scb->rssi == rssi) &&
			(mu_scb->time_score > time))) {
			max = mu_scb->score;
			rssi = mu_scb->rssi;
			time = mu_scb->time_score;
			result = item;
		}
	}

	return result;
}

static mutx_scb_list_t*
mutx_scb_list_min(mutx_scb_list_t *head)
{
	uint32 min = MUTX_SCORE_MAX;
	uint32 time = 0;
	int rssi = 0;
	mutx_scb_list_t *result = NULL;
	mutx_scb_list_t *item, *next;

	FOREACH_LIST_POS(item, next, head) {
		mutx_scb_t *mu_scb = (mutx_scb_t *)item->data;
		if (!mu_scb)
			continue;

		if ((!result) || (mu_scb->score < min) ||
			((mu_scb->score == min) && (mu_scb->rssi < rssi)) ||
			((mu_scb->score == min) && (mu_scb->rssi == rssi) &&
			(mu_scb->time_score > time))) {
			min = mu_scb->score;
			rssi = mu_scb->rssi;
			time = mu_scb->time_score;
			result = item;
		}
	}

	return result;
}

static  mutx_scb_list_t *
mutx_blcklst_new(wlc_mutx_info_t *mu_info)
{
	mutx_scb_list_t *head, *bkt;

	bkt = mu_info->blcklst_bkt;

	head = mutx_bkt_new(mu_info, NULL);
	if (head == NULL) {
		return NULL;
	}

	mutx_bkt_add_tail(bkt, head);
	return head;
}

static void
mutx_blcklst_del(wlc_mutx_info_t *mu_info, mutx_scb_list_t *bkt)
{
	mutx_scb_list_t *pos, *next;

	FOREACH_LIST_POS(pos, next, bkt) {
		mutx_scb_list_del(mu_info, pos);
	}

	mutx_bkt_del(mu_info, bkt);

	return;
}

static void
mutx_blcklst_dump(wlc_mutx_info_t *mu_info, bcmstrbuf_t *b)
{
	mutx_scb_list_t *head, *bkt;
	mutx_scb_list_t *pos, *next;
	mutx_scb_list_t *link_pos, *link_n;
	mutx_scb_t *mutx_scb;
	scb_t *scb;
	int i = 0;

	bkt = mu_info->blcklst_bkt;
	FOREACH_BKT_POS(link_pos, link_n, bkt) {
			head = link_pos;
		bcm_bprintf(b, "blcklst items in grp%d\n", i);
		i++;
		FOREACH_LIST_POS(pos, next, head) {
			mutx_scb = (mutx_scb_t *)pos->data;
			if (mutx_scb == NULL)
				return;
			scb = mutx_scb->scb;
			bcm_bprintf(b, ""MACF" \n", ETHER_TO_MACF(scb->ea));
		}
	}
	return;
}

static bool
mutx_blcklst_match_item(mutx_scb_list_t *item, mutx_scb_list_t *head)
{
	mutx_scb_t *mutx_scb_item = (mutx_scb_t *)item->data;
	mutx_scb_t *mutx_scb = NULL;
	mutx_scb_list_t *pos, *next;

	if (mutx_scb_item == NULL)
		return FALSE;

	if (head == NULL)
		return FALSE;

	FOREACH_LIST_POS(pos, next, head) {
		mutx_scb = (mutx_scb_t *)pos->data;
		if (mutx_scb == NULL)
			return FALSE;
		if ((void *)mutx_scb_item == (void *)mutx_scb) {
			return TRUE;
		}
	}

	return FALSE;
}

static void
mutx_blcklst_insert_item(wlc_mutx_info_t *mu_info, mutx_scb_list_t *item, mutx_scb_list_t *bkt)
{
	mutx_scb_t *mutx_scb_item = (mutx_scb_t *)item->data;
	mutx_scb_list_t *bkt_item;

	ASSERT(bkt != NULL);
	bkt_item = mutx_scb_list_new(mu_info, item->data);
	if (bkt_item == NULL) {
		return;
	}
	if (mutx_scb_item->blcklst)
		bkt_item->blcklst = TRUE;

	mutx_scb_list_add_tail(bkt, bkt_item);
	return;
}

static void
mutx_blcklst_del_item(wlc_mutx_info_t *mu_info, mutx_scb_list_t *item)
{
	mutx_scb_t *mutx_scb, *mutx_scb_item;
	mutx_scb_list_t *bkt, *link_pos, *link_n;
	mutx_scb_list_t *head, *pos, *next;

	if (item == NULL)
		return;

	mutx_scb_item = (mutx_scb_t *)item->data;
	if (mutx_scb_item == NULL)
		return;

	bkt = mu_info->blcklst_bkt;

	if (bkt == NULL)
		return;

	FOREACH_BKT_POS(link_pos, link_n, bkt) {
		head = link_pos;
		FOREACH_LIST_POS(pos, next, head) {
			mutx_scb = (mutx_scb_t *)pos->data;
			ASSERT(mutx_scb != NULL);
			if ((void *)mutx_scb_item == (void *)mutx_scb) {
				mutx_blcklst_del(mu_info, link_pos);
				break;
			}
		}
	}
	return;
}

static  mutx_scb_list_t *mutx_blcklst_match(wlc_mutx_info_t *mu_info, mutx_scb_list_t *head,
		mutx_scb_list_t **blcklst_bkt)
{
	mutx_scb_list_t *pos, *next, *blcklst_pos = NULL;
	mutx_scb_list_t *bkt, *link_pos, *link_n;
	uint nitems = 0, nitems_lst;
	bool match = FALSE;

	FOREACH_LIST_POS(pos, next, head) {
		nitems++;
	}

	*blcklst_bkt = NULL;
	bkt = mu_info->blcklst_bkt;
	FOREACH_BKT_POS(link_pos, link_n, bkt) {
		nitems_lst = 0;
		FOREACH_LIST_POS(pos, next, link_pos) {
			nitems_lst++;
		}

		if (nitems_lst != nitems)
			continue;

		FOREACH_LIST_POS(pos, next, link_pos) {
			if (!mutx_blcklst_match_item(pos, head)) {
				match = FALSE;
				break;
			}
			if (pos->blcklst) {
				blcklst_pos = pos;
			}
			match = TRUE;
		}

		if (match) {
			ASSERT((blcklst_pos != NULL));
			FOREACH_LIST_POS(pos, next, head) {
				if (pos->data == blcklst_pos->data) {
					*blcklst_bkt = link_pos;
					return pos;
				}
			}
		}

	}

	return NULL;
}

#ifdef BCMDBG
/* Set the user position for a given group within a position array owned by a STA. */
static void
mu_user_pos_set(uint8 *pos_array, uint16 group_id, uint8 user_pos)
{
	/* Index of first (low) bit in user position field */
	uint16 bit_index = group_id * MU_POSITION_BIT_LEN;

	pos_array[(bit_index) / NBBY] = (user_pos << (bit_index % NBBY));
}
#endif  /* BCMDBG */

static uint8
mu_user_pos_get(uint8 *pos_array, uint16 group_id)
{
	uint8 user_pos;
	uint8 pos_mask;

	/* Index of first (low) bit in user position field */
	uint16 bit_index = group_id * MU_POSITION_BIT_LEN;
	pos_mask = 0x3 << (bit_index % NBBY);

	user_pos = (pos_array[(bit_index) / NBBY] & pos_mask) >> (bit_index % NBBY);
	return user_pos;
}

/* Return TRUE if the membership array indicates membership in at least one group. */
static bool
mu_any_membership(uint8 *membership)
{
	uint32 *mw = (uint32*) membership;  /* start of one 4-byte word of membership array */
	int i;
	int num_ints = MU_MEMBERSHIP_SIZE / sizeof(uint32);

	if (!membership)
		return FALSE;

	for (i = 0; i < num_ints; i++) {
		if (*mw != 0) {
			return TRUE;
		}
		mw++;
	}
	return FALSE;
}

/* Module Attach/Detach */

/*
 * Create the MU-MIMO module infrastructure for the wl driver.
 * wlc_module_register() is called to register the module's
 * handlers. The dump function is also registered. Handlers are only
 * registered if the phy is MU BFR capable and if MU TX is not disabled
 * in the build.
 *
 * _mu_tx is always FALSE upon return. Because _mu_tx reflects not only
 * phy capability, but also configuration, have to wait until the up callback, if
 * registered, to set _mu_tx to TRUE.
 *
 * Returns
 *     A wlc_mutx_info_t structure, or NULL in case of failure.
 */
wlc_mutx_info_t*
BCMATTACHFN(wlc_mutx_attach)(wlc_info_t *wlc)
{
	wlc_mutx_info_t *mu_info;
	int err;

	/* allocate the main state structure */
	mu_info = MALLOCZ(wlc->osh, sizeof(wlc_mutx_info_t));
	if (mu_info == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));

		return NULL;
	}

	mu_info->wlc = wlc;
	mu_info->pub = wlc->pub;
	mu_info->osh = wlc->osh;
	wlc->_mu_tx = FALSE;

	/* Avoid registering callbacks if phy is not MU capable */
	if (!WLC_MU_BFR_CAP_PHY(wlc)) {
		return mu_info;
	}

	/* Default enable scheduler and AC policy for now */
	mu_info->mutx_client_scheduler = TRUE;
	mu_info->ac_policy_on = TRUE;
	/* Enforce same BW by default before BW-aware ucode gets in */
	mu_info->mutx_client_samebw = FALSE;

	mu_info->pfmon_gpos = FALSE;
#ifndef WLCNT
	/* always turn off MUMIMO SNR CALIBRATION without WLCNT */
	mu_info->mu_snr_calib = FALSE;
#else
	if (D11REV_IS(mu_info->pub->corerev, 129))
		mu_info->mu_snr_calib = TRUE;
	else
		mu_info->mu_snr_calib = FALSE;
#endif // endif

	mu_info->hemu_rssi_thresh = MUTX_RSSI_MIN;
	if (D11REV_IS(mu_info->pub->corerev, 129)) {
		/* only 43684b0/b1 for now, default MU on */
		mu_info->pub->mu_features = MU_FEATURES_MUTX;
		/* The number of active TX FIFOs may have changed */
		wlc_hw_update_nfifo(wlc->hw);
	} else if (D11REV_GE(mu_info->pub->corerev, 65)) {
		/* Default let the drivers switch SU/MU automatically */
		mu_info->pub->mu_features = MU_FEATURES_AUTO;
	}

	if (D11REV_IS(mu_info->pub->corerev, 130) || D11REV_IS(mu_info->pub->corerev, 131)) {
		mu_info->pub->mu_features = MU_FEATURES_OFF;
	}

	mu_info->muclient_nrx = MUCLIENT_NRX_2;
	mu_info->mutx_client_scheduler_dur = MUCLIENT_SCHEDULER_DUR;
	mu_info->min_hemmu_usrs = 2;

	/* Phy is MU TX capable. And build has not prohibited MU TX. So
	 * register callbacks and allow MU TX to be dynamically enabled.
	 */
	err = wlc_module_register(mu_info->pub, mutx_iovars, "mutx", mu_info,
	                          wlc_mu_doiovar, wlc_mutx_watchdog, wlc_mutx_up, NULL);

	if (err != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed with error %d (%s).\n",
		          wlc->pub->unit, __FUNCTION__, err, bcmerrorstr(err)));

		/* use detach as a common failure deallocation */
		wlc_mutx_detach(mu_info);
		return NULL;
	}

	/* reserve scb cubby space for STA-specific data. No init function is
	 * registered because cubby memory is allocated on demand.
	 */
	mu_info->scb_handle = wlc_scb_cubby_reserve(wlc, sizeof(mutx_scb_cubby_t),
	                                            mu_scb_cubby_init,
	                                            mu_scb_cubby_deinit,
	                                            mu_scb_cubby_dump,
	                                            mu_info);
	if (mu_info->scb_handle < 0) {
		WL_ERROR(("wl%d: %s: Failed to reserve scb cubby space.\n",
		         wlc->pub->unit, __FUNCTION__));

		wlc_mutx_detach(mu_info);
		return NULL;
	}

	/* Add client callback to the scb state notification list */
	if ((err = wlc_scb_state_upd_register(wlc, wlc_mutx_scb_state_upd, mu_info)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: unable to register callback %p\n",
		          wlc->pub->unit, __FUNCTION__, wlc_mutx_scb_state_upd));

		wlc_mutx_detach(mu_info);
		return NULL;
	}

#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU) || defined(BCMDBG_MU) \
	|| defined(DUMP_MUTX)
	wlc_dump_add_fns(mu_info->pub, "mutx", wlc_mutx_dump, wlc_mutx_dump_clr, mu_info);
#endif // endif
	mu_info->on_hold_list = mutx_scb_list_new(mu_info, NULL);
	if (mu_info->on_hold_list == NULL) {
		wlc_mutx_detach(mu_info);
		return NULL;
	}

	mu_info->admit_list = mutx_scb_list_new(mu_info, NULL);
	if (mu_info->admit_list == NULL) {
		wlc_mutx_detach(mu_info);
		return NULL;
	}

	mu_info->candidate_list = mutx_scb_list_new(mu_info, NULL);
	if (mu_info->candidate_list == NULL) {
		wlc_mutx_detach(mu_info);
		return NULL;
	}

	mu_info->alternate_list = mutx_scb_list_new(mu_info, NULL);
	if (mu_info->alternate_list == NULL) {
		wlc_mutx_detach(mu_info);
		return NULL;
	}

	mu_info->evictee_list = mutx_scb_list_new(mu_info, NULL);
	if (mu_info->evictee_list == NULL) {
		wlc_mutx_detach(mu_info);
		return NULL;
	}

	mu_info->blcklst_bkt = mutx_bkt_new(mu_info, NULL);
	if (mu_info->blcklst_bkt == NULL) {
		wlc_mutx_detach(mu_info);
		return NULL;
	}

	/* create notification list for mutx state change. */
	if (bcm_notif_create_list(wlc->notif, &mu_info->mutx_state_notif_hdl) != BCME_OK) {
		WL_ERROR(("wl%d: %s: mutx bcm_notif_create_list() failed\n",
			wlc->pub->unit, __FUNCTION__));
		wlc_mutx_detach(mu_info);
		return NULL;
	}

	if ((wlc->mutx_policy = (wlc_mutx_policy_t *)
		MALLOCZ(wlc->osh, sizeof(wlc_mutx_policy_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes for MU TX policy\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		wlc_mutx_detach(mu_info);
		return NULL;
	}
	return mu_info;
}

/* Free all resources associated with the MU-MIMO module
 * infrastructure. This is done at the cleanup stage when
 * freeing the driver.
 *
 * mu_info    MU-MIMO module state structure
 */
void
BCMATTACHFN(wlc_mutx_detach)(wlc_mutx_info_t *mu_info)
{
	if (mu_info == NULL) {
		return;
	}

	if (mu_info->mutx_state_notif_hdl != NULL)
		bcm_notif_delete_list(&mu_info->mutx_state_notif_hdl);

	if (mu_info->on_hold_list != NULL)
		mutx_scb_list_del(mu_info, mu_info->on_hold_list);
	if (mu_info->candidate_list != NULL)
		mutx_scb_list_del(mu_info, mu_info->candidate_list);
	if (mu_info->admit_list != NULL)
		mutx_scb_list_del(mu_info, mu_info->admit_list);
	if (mu_info->alternate_list != NULL)
		mutx_scb_list_del(mu_info, mu_info->alternate_list);
	if (mu_info->evictee_list != NULL)
		mutx_scb_list_del(mu_info, mu_info->evictee_list);
	if (mu_info->blcklst_bkt != NULL)
		mutx_bkt_del(mu_info, mu_info->blcklst_bkt);

	if (mu_info->wlc->mutx_policy != NULL) {
		MFREE(mu_info->wlc->osh, mu_info->wlc->mutx_policy, sizeof(wlc_mutx_policy_t));
		mu_info->wlc->mutx_policy = NULL;
	}

	wlc_scb_state_upd_unregister(mu_info->wlc, wlc_mutx_scb_state_upd, mu_info);

	wlc_module_unregister(mu_info->pub, "mutx", mu_info);

	MFREE(mu_info->osh, mu_info, sizeof(wlc_mutx_info_t));
}

#ifdef WLCNT
static void
wlc_mutx_print_txpermcs(wlc_mutx_info_t *mu_info, uint32 tot_tx, uint32 tot_tx_succ,
	uint8 last_mcs_idx, uint32 *tx, uint32 *tx_succ, bcmstrbuf_t *b,
	bool is_mu, bool is_sgi, uint8 mu_type)
{
	uint8 i;
	uint8 MAX_RATES = (mu_type == TX_STATUS_MUTP_HEMM) ? MAX_HE_RATES : MAX_VHT_RATES;
	if ((mu_type != TX_STATUS_MUTP_HEMM) && (mu_type != TX_STATUS_MUTP_VHTMU)) {
		WL_ERROR(("%s wl%d asserts: mu_type %d ",
		__FUNCTION__, mu_info->wlc->pub->unit, mu_type));
		ASSERT(0);
		return;
	}

	if (mu_type == TX_STATUS_MUTP_HEMM) {
		bcm_bprintf(b, "TX HE-%s%s TOT :",
			(is_mu ? "MMU" : "SU"), (is_sgi ? " SGI" : ""));
	} else if (mu_type == TX_STATUS_MUTP_VHTMU) {
		bcm_bprintf(b, "TX VHT-%s%s TOT :",
			(is_mu ? "MU" : "SU"), (is_sgi ? " SGI" : ""));
	}
	if (tot_tx) {
		for (i = 0; i <= last_mcs_idx; i++) {
			bcm_bprintf(b, "  %d(%d%%)", tx[i],
				(tx[i] * 100) / tot_tx);
			if (((i % MAX_RATES) == (MAX_RATES - 1)) &&
				(i != last_mcs_idx)) {
				bcm_bprintf(b, "\n        :");
			}

		}
	}
	bcm_bprintf(b, "\n");
#ifdef NOT_YET
	if (mu_type == TX_STATUS_MUTP_HEMM) {
		bcm_bprintf(b, "TX HE-%s%s TOT :",
			(is_mu ? "MMU" : "SU"), (is_sgi ? " SGI" : ""));
	} else if (mu_type == TX_STATUS_MUTP_VHTMU) {
		bcm_bprintf(b, "TX VHT-%s%s TOT :",
			(is_mu ? "MU" : "SU"), (is_sgi ? " SGI" : ""));
	}
	if (tot_tx_succ) {
		for (i = 0; i <= last_mcs_idx; i++) {
			bcm_bprintf(b, "  %d(%d%%)", tx_succ[i],
				(tx_succ[i] * 100) / tot_tx_succ);
			if (((i % MAX_RATES) == (MAX_RATES - 1)) &&
				(i != last_mcs_idx)) {
				bcm_bprintf(b, "\n	  :");
			}
		}
	}
	bcm_bprintf(b, "\n");
#else
	BCM_REFERENCE(tot_tx_succ);
#endif /* NOT_YET */
	if (mu_type == TX_STATUS_MUTP_HEMM) {
		bcm_bprintf(b, "TX HE-%s%s PER :",
			(is_mu ? "MMU" : "SU"), (is_sgi ? " SGI" : ""));
	} else if (mu_type == TX_STATUS_MUTP_VHTMU) {
		bcm_bprintf(b, "TX VHT-%s%s PER :",
			(is_mu ? "MU" : "SU"), (is_sgi ? " SGI" : ""));
	}
	if (tot_tx) {
		for (i = 0; i <= last_mcs_idx; i++) {
			int unacked = 0, per = 0;
			unacked = tx[i] - tx_succ[i];
			if (unacked < 0) {
				unacked = 0;
			}
			if ((unacked > 0) && (tx[i])) {
				per = (unacked * 100) / tx[i];
			}
			bcm_bprintf(b, "  %d(%d%%)", unacked, per);
			if (((i % MAX_RATES) == (MAX_RATES - 1)) &&
				(i != last_mcs_idx)) {
				bcm_bprintf(b, "\n        :");
			}
		}
	}
	bcm_bprintf(b, "\n");
}

static void
wlc_mutx_dump_stats(wlc_mutx_info_t *mu_info,
	mutx_stats_t *mutx_stats, uint8 mu_type, bcmstrbuf_t *b, uint8 option)
{
	uint32 total, tot_txmu = 0, tot_txmu_succ = 0, tot_txmu_sgi = 0, tot_txmu_sgi_succ = 0;
	uint32 tot_txsu = 0, tot_txsu_succ = 0;
	uint8 i, last, last_mcs_idx_mu = 0, last_mcs_idx_su = 0;

	/* determines highest MCS array *index* on which a transmit took place */
	for (i = 0; i < MUTX_MCS_INDEX_NUM; i++) {
		tot_txmu += mutx_stats->txmu[i];
		tot_txmu_succ += mutx_stats->txmu_succ[i];
		tot_txsu += mutx_stats->txsu[i];
		tot_txsu_succ += mutx_stats->txsu_succ[i];
		tot_txmu_sgi += mutx_stats->txmu_sgi[i];
		tot_txmu_sgi_succ += mutx_stats->txmu_sgi_succ[i];
		if (mutx_stats->txmu[i]) {
			last_mcs_idx_mu = i;
		}
		if (mutx_stats->txsu[i]) {
			last_mcs_idx_su = i;
		}
	}
	total = tot_txmu + tot_txsu;

	bcm_bprintf(b, "mutx: tot_tx %u tx_as_mu %u (%u%%)"
		" ncons %u nlost %u\n",
		total, tot_txmu, (total ? (tot_txmu * 100 / total) : 0),
		mutx_stats->mu_ncons, mutx_stats->mu_nlost);

	wlc_mutx_print_txpermcs(mu_info, tot_txmu, tot_txmu_succ, last_mcs_idx_mu,
		mutx_stats->txmu, mutx_stats->txmu_succ, b, TRUE, FALSE, mu_type);

	wlc_mutx_print_txpermcs(mu_info, tot_txmu_sgi, tot_txmu_sgi_succ, last_mcs_idx_mu,
		mutx_stats->txmu_sgi, mutx_stats->txmu_sgi_succ, b, TRUE, TRUE, mu_type);

	wlc_mutx_print_txpermcs(mu_info, tot_txsu, tot_txsu_succ, last_mcs_idx_su,
		mutx_stats->txsu, mutx_stats->txsu_succ, b, FALSE, FALSE, mu_type);

	/* determines highest mutx *index* on which a transmit took place */
	for (i = 0, total = 0, last = 0; i < MUTX_MAXMUTXCNT; i++) {
		total += mutx_stats->mutxcnt_stat[i];
		if (mutx_stats->mutxcnt_stat[i]) {
			last = i;
		}
	}

	if (option == MUTX_MUINFO_OPT_DETAIL) {
		bcm_bprintf(b, "mutxcnt :");
		if (total) {
			for (i = 0; i <= last; i++) {
				bcm_bprintf(b, "  %d(%d%%)", mutx_stats->mutxcnt_stat[i],
					(mutx_stats->mutxcnt_stat[i] * 100) / total);
				if ((i % MUTX_MAXMUTXCNT) == 5 && i != last) {
					bcm_bprintf(b, "\n        :");
				}
			}
		}
		bcm_bprintf(b, "\n");
	}

	// XXX: For HEMM, these stats are derived from txs.gbmp which isn't the final gbmp.
	bcm_bprintf(b, "grpsize :");
	total = mutx_stats->gnum[0] + mutx_stats->gnum[1] +
		mutx_stats->gnum[2] + mutx_stats->gnum[3];
	for (i = 0; i < MUTX_MAXGNUM; i++) {
		bcm_bprintf(b, "  %d(%d%%)",
		mutx_stats->gnum[i], (total ? mutx_stats->gnum[i]*100/total : 0));
	}
	bcm_bprintf(b, "\n");

	if (option == MUTX_MUINFO_OPT_DETAIL) {
		bcm_bprintf(b, "gpos    :");
		for (i = 0; i < MUTX_MAXGNUM; i++) {
			bcm_bprintf(b, "  %d",
			mutx_stats->gpos_cnt[i]);
		}
		bcm_bprintf(b, "\n");
	}
}
#endif /* WLCNT */

/* Dump MU-MIMO state information. */
static int
wlc_mutx_muinfo(wlc_mutx_info_t *mu_info, bcmstrbuf_t *b, uint8 option)
{
	scb_t *scb;
	scb_iter_t scbiter;
	mutx_scb_t *mu_scb;
	int i;
	uint16 g;
	uint8 pos;
	bool empty_group;
	mutx_scb_list_t *hold_pos, *next;
	wlc_info_t *wlc = mu_info->wlc;
	uint8 max_mmu_usrs = wlc_txcfg_max_mmu_clients_get(wlc->txcfg);

	if (!wlc) {
		bcm_bprintf(b, "wlc pointer is NULL on mu_info\n");
		return BCME_OK;
	}

	if (MU_TX_ENAB(wlc))
		bcm_bprintf(b, "MU BFR capable and configured; ");

	bcm_bprintf(b, "MU feature is %s, AC policy is %s\n"
		"BW policy = %u AC policy = %u\n",
		mu_info->mutx_active ? "ON" : "OFF",
		mu_info->ac_policy_on ? "ON" : "OFF",
		wlc->mutx_policy->bw_policy,
		wlc->mutx_policy->ac_policy);

	bcm_bprintf(b, "Maximum clients VHTMU %d HEMMU %d\n",
		wlc_txcfg_max_clients_get(wlc->txcfg, VHTMU),
		wlc_txcfg_max_clients_get(wlc->txcfg, HEMMU));

	if (mu_info->muclient_elect_pending)
		bcm_bprintf(b, "MU client election pending\n");

	if (option == MUTX_MUINFO_OPT_DETAIL) {
		/* print group membership */
		bcm_bprintf(b, "Static VHTMU GID table:\n");
		for (g = MU_GROUP_ID_MIN; g < MU_GROUP_ID_MAX; g++) {
			/* 1st iteration to tell whether this is an empty group */
			empty_group = TRUE;
			for (i = 0; i < MUCLIENT_NUM; i++) {
				scb = mu_info->mu_clients[i];
				if (scb && SCB_VHTMU(scb)) {
					mu_scb = MUTX_SCB(mu_info, scb);
					if (mu_scb && isset(mu_scb->membership, g)) {
						if (empty_group) {
							bcm_bprintf(b, "    GID %u:", g);
							empty_group = FALSE;
							break;
						}
					}
				}
			}
			/* 2nd iteration to print the client indices in each position */
			for (pos = 0; pos < 4; pos++) {
				if (!empty_group)
					bcm_bprintf(b, " [");
				for (i = 0; i < MUCLIENT_NUM; i++) {
					scb = mu_info->mu_clients[i];
					if (scb && SCB_VHTMU(scb)) {
						mu_scb = MUTX_SCB(mu_info, scb);
						if (mu_scb && isset(mu_scb->membership, g)) {
							if (mu_user_pos_get(mu_scb->position, g) ==
								pos) {
								bcm_bprintf(b, " %u", i);
							}
						}
					}
				}
				if (!empty_group)
					bcm_bprintf(b, " ]");
			}
			if (!empty_group)
				bcm_bprintf(b, "\n");
		}
	} else if (option == MUTX_MUINFO_OPT_SIMPLE) {
		bcm_bprintf(b, "GID joined: [9]\n");
	}

	/* Dump current MU client set */
	bcm_bprintf(b, "MU clients:\n");
	for (i = 0; i < max_mmu_usrs; i++) {
		if (!(scb = mu_info->mu_clients[i])) {
			continue;
		}
		if (!(mu_scb = MUTX_SCB(mu_info, scb))) {
			continue;
		}

		bcm_bprintf(b, "[%u] "MACF" rssi %d nrx %u [%s]\n",
			i, ETHER_TO_MACF(scb->ea),
			mu_info->mutx_client_scheduler ? mu_scb->rssi :
			wlc_lq_rssi_get(wlc, SCB_BSSCFG(scb), scb),
			mu_scb->nrx, mutx_reason2str[mu_scb->reason]);
		/* Client's BSS info */
		bcm_bprintf(b, "BSS: wl%u.%u "MACF"\n", wlc->pub->unit,
			WLC_BSSCFG_IDX(scb->bsscfg),
			ETHER_TO_MACF(scb->bsscfg->cur_etheraddr));
#ifdef WLCNT
		if (option) {
			if (mu_scb->mutx_stats) {
				wlc_mutx_dump_stats(mu_info, mu_scb->mutx_stats,
					SCB_HE_CAP(scb) ?
					TX_STATUS_MUTP_HEMM : TX_STATUS_MUTP_VHTMU, b, option);
				wlc_muclient_clear_stats(mu_info, mu_scb->mutx_stats);
			}
		}
#endif /* WLCNT */
	}

	if (option) {
		bcm_bprintf(b, "Total mutx: tx_as_mu %u ncons %u nlost %u\n",
			mu_info->mutx_cnt, mu_info->mu_ncons, mu_info->mu_nlost);

		bcm_bprintf(b, "\nMU clients on hold_list:\n");
		FOREACH_LIST_POS(hold_pos, next, mu_info->on_hold_list) {
			mutx_scb_t *mutx_scb = (mutx_scb_t *)hold_pos->data;
			bcm_bprintf(b, ""MACF" hold_dur %u(secs)\n",
					ETHER_TO_MACF(mutx_scb->scb->ea),
					(MU_STA_MAX_HOLD_DUR -  mutx_scb->on_hold_dur));
		}
		mutx_blcklst_dump(mu_info, b);
	}
	mu_info->mu_ncons = 0;
	mu_info->mutx_cnt = 0;
	mu_info->mu_nlost = 0;

	bcm_bprintf(b, "\nMU capable STAs:\n");
	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (mu_sta_mu_capable(mu_info, scb) && SCB_MU(scb)) {
			if (!(mu_scb = MUTX_SCB(mu_info, scb))) {
				continue;
			}

			bcm_bprintf(b, ""MACF" rssi %d nrx %u [%s]\n", ETHER_TO_MACF(scb->ea),
				mu_info->mutx_client_scheduler ? mu_scb->rssi :
				wlc_lq_rssi_get(wlc, SCB_BSSCFG(scb), scb),
				mu_scb->nrx, mutx_reason2str[mu_scb->reason]);
		}
	}

	return BCME_OK;
}

#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU) || defined(BCMDBG_MU) \
	|| defined(DUMP_MUTX)
/* Dump MU-MIMO state information. */
static int
wlc_mutx_dump(void *ctx, bcmstrbuf_t *b)
{
	wlc_mutx_info_t *mu_info = ctx;
	return wlc_mutx_muinfo(mu_info, b, MUTX_MUINFO_OPT_DETAIL);
}

static int
wlc_mutx_dump_clr(void *ctx)
{
	wlc_mutx_info_t *mu_info = ctx;
#ifdef WLCNT
	scb_t *scb;
	mutx_scb_t *mu_scb;
	int i;
	uint8 max_mmu_usrs = wlc_txcfg_max_mmu_clients_get(mu_info->wlc->txcfg);

	if (!mu_info->wlc) {
		return BCME_OK;
	}

	for (i = 0; i < max_mmu_usrs; i++) {
		scb = mu_info->mu_clients[i];
		if (!scb) {
			continue;
		}

		mu_scb = MUTX_SCB(mu_info, scb);
		if (mu_scb && mu_scb->mutx_stats) {
			wlc_muclient_clear_stats(mu_info, mu_scb->mutx_stats);
		}
	}
#endif /* WLCNT */

	mu_info->mu_ncons = 0;
	mu_info->mutx_cnt = 0;
	mu_info->mu_nlost = 0;

	return BCME_OK;
}
#endif // endif

/* IOVar handler for the MU-MIMO infrastructure module */
static int
wlc_mu_doiovar(void *hdl, uint32 actionid,
	void *p, uint plen, void *a, uint alen, uint vsize, wlc_if_t *wlcif)
{
	wlc_mutx_info_t *mu_info = (wlc_mutx_info_t*) hdl;
	wlc_info_t *wlc = mu_info->wlc;
	int32 int_val = 0;
	int err = 0;
	int32 *ret_int_ptr = (int32 *)a;
	uint32 *ret_uint_ptr = (uint32 *)a;
	bool set_bool;
	wlc_pub_t *pub = wlc->pub;

	BCM_REFERENCE(mu_info);
	BCM_REFERENCE(ret_int_ptr);
	BCM_REFERENCE(ret_uint_ptr);
	BCM_REFERENCE(set_bool);

	if (plen >= (int)sizeof(int_val))
		memcpy(&int_val, p, sizeof(int_val));

	switch (actionid) {
#ifdef BCMDBG
	case IOV_SVAL(IOV_MUTX_UT):
		mutx_ut(mu_info, int_val);
		break;
#endif /* BCMDBG */
#ifdef WL_MUPKTENG
	case IOV_SVAL(IOV_MUTX_PKTENG):
		set_bool = int_val? TRUE : FALSE;
		if (mu_info->mutx_pkteng != set_bool) {
			mu_info->mutx_pkteng = set_bool;
		}
		break;

	case IOV_GVAL(IOV_MUTX_PKTENG):
		*ret_uint_ptr = (uint8)mu_info->mutx_pkteng;
		break;

	case IOV_SVAL(IOV_MUTX_PKTENG_ADDSTA): {
		mupkteng_sta_t sta;
		bcopy(p, &sta, sizeof(mupkteng_sta_t));
		err = wlc_mupkteng_addsta(mu_info, &sta.ea, sta.idx, sta.nrxchain);
		break;
	}

	case IOV_SVAL(IOV_MUTX_PKTENG_CLRSTA): {
		mupkteng_sta_t sta;
		bcopy(p, &sta, sizeof(mupkteng_sta_t));
		err = wlc_mupkteng_clrsta(mu_info, sta.idx);
		break;
	}
	case IOV_SVAL(IOV_MUTX_PKTENG_TX): {
		mupkteng_tx_t mupkteng_tx;
		bcopy(p, &mupkteng_tx, sizeof(mupkteng_tx_t));
		err = wlc_mupkteng_tx(mu_info, &mupkteng_tx);
		break;
	}
#endif /* WL_MUPKTENG */
#if defined(BCMDBG) || defined(BCMDBG_MU)
	case IOV_GVAL(IOV_MUTX_SENDGID):
		*ret_uint_ptr = wlc_mutx_sendgid(mu_info);
		break;

#endif /* BCMDBG */
	case IOV_GVAL(IOV_MU_POLICY): {
		mu_policy_t *policy = (mu_policy_t *)a;
		policy->version = WL_MU_POLICY_PARAMS_VERSION;
		policy->length = sizeof(mu_policy_t);
		policy->sched_timer = mu_info->mutx_client_scheduler ?
			mu_info->mutx_client_scheduler_dur : 0;
		policy->pfmon = mu_info->muclient_pfmon;
		policy->pfmon_gpos = mu_info->pfmon_gpos;
		policy->samebw = mu_info->mutx_client_samebw;
		policy->nrx = mu_info->muclient_nrx;
		policy->max_muclients = wlc_txcfg_max_clients_get(wlc->txcfg, VHTMU);
		break;
	}

	case IOV_SVAL(IOV_MU_POLICY): {
		mu_policy_t *policy = (mu_policy_t *)a;

		if (policy->version != WL_MU_POLICY_PARAMS_VERSION) {
			err = BCME_BADARG;
			break;
		}
		/* sched_timer */
		if (policy->sched_timer) {
			mu_info->mutx_client_scheduler = TRUE;
			mu_info->mutx_client_scheduler_dur = policy->sched_timer;
			mu_info->ac_policy_on = TRUE;
			/* Early starting the 1st time scheduler */
			mu_info->mutx_client_scheduler_cnt = (policy->sched_timer > 3) ?
				(policy->sched_timer - 3) : policy->sched_timer;
		} else {
			mu_info->mutx_client_scheduler = FALSE;
			mu_info->mutx_client_scheduler_dur = MUCLIENT_SCHEDULER_DUR;
			mu_info->ac_policy_on = FALSE;
			wlc_mutx_clients_clr_blcklst_state(mu_info);
			wlc_mutx_bw_policy_update(mu_info, FALSE);
		}
		/* pfmon */
		if (policy->pfmon) {
			mu_info->muclient_pfmon = TRUE;
		} else {
			mu_info->muclient_pfmon = FALSE;
			wlc_mutx_clients_clr_blcklst_state(mu_info);
		}
#if defined(BCMDBG) || defined(BCMDBG_MU)
		/* pfmon_gpos */
		if (policy->pfmon_gpos) {
			mu_info->pfmon_gpos = TRUE;
		} else {
			mu_info->pfmon_gpos = FALSE;
			wlc_mutx_clients_clr_blcklst_state(mu_info);
		}
#endif // endif
		/* samebw */
		if (policy->samebw) {
			mu_info->mutx_client_samebw = TRUE;
		} else {
			mu_info->mutx_client_samebw = FALSE;
		}
		/* nrx */
		if (policy->nrx != mu_info->muclient_nrx) {
			if (pub->up) {
				WL_ERROR(("wl%d: nrx can't be set with interface up\n",
					pub->unit));
				err = BCME_NOTDOWN;
				break;
			}
			if (policy->nrx < MUCLIENT_NRX_1 || policy->nrx > MUCLIENT_NRX_MAX) {
				err = BCME_RANGE;
				break;
			}
			mu_info->muclient_nrx = (uint8)policy->nrx;
		}
		/* max_muclients */
		if (policy->max_muclients != wlc_txcfg_max_clients_get(wlc->txcfg, VHTMU)) {

			if (pub->up) {
				WL_ERROR(("wl%d: max_muclients can't be set with interface up\n",
					pub->unit));
				err = BCME_NOTDOWN;
				break;
			}

			err = wlc_txcfg_max_clients_set(wlc->txcfg, VHTMU, policy->max_muclients);
		}
		break;
	}

	case IOV_GVAL(IOV_MUTX_MUINFO): {
		bcmstrbuf_t b;
		uint8 option = *((uint8 *)p);

	        bcm_binit(&b, (char *)a, alen);

		if (option) {
#if defined(BCMDBG) || defined(BCMDBG_MU)
			option = MUTX_MUINFO_OPT_DETAIL;
#else
			option = MUTX_MUINFO_OPT_SIMPLE;
#endif // endif
		}

		err = wlc_mutx_muinfo(mu_info, &b, option);
		break;
	}

	case IOV_GVAL(IOV_MU_SNR_CALIB):
		*ret_uint_ptr = mu_info->mu_snr_calib;
		break;

	case IOV_SVAL(IOV_MU_SNR_CALIB):
		if (int_val) {
			mu_info->mu_snr_calib = TRUE;
		} else {
			mu_info->mu_snr_calib = FALSE;
			W_REG(wlc->osh, D11_PHY_REG_ADDR(wlc), PHYREG_M2V_MSG0);
			W_REG(wlc->osh, D11_PHY_REG_DATA(wlc), 0x8006);
			/*
			 * bit15:      interrupt bit
			 * bit[5:0]:   interrupt type;
			 */
		}
		break;

	case IOV_GVAL(IOV_MU_FEATURES):
		*ret_uint_ptr = pub->mu_features;
		break;

	case IOV_SVAL(IOV_MU_FEATURES):
		/* Handle the auto bit */
		pub->mu_features &= ~MU_FEATURES_AUTO;
		pub->mu_features |= ((uint32)int_val & MU_FEATURES_AUTO);

		if (D11REV_GE(pub->corerev, 128) &&
		  (int_val & MU_FEATURES_AUTO)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		/* Currently only MUTX is supported */
		if ((pub->mu_features & MU_FEATURES_MUTX) ==
			((uint32)int_val & MU_FEATURES_MUTX)) {
			break;
		}

		if (HE_DLMU_ENAB(pub) && pub->up) {
			err = BCME_NOTDOWN;
			break;
		}
#ifdef WL_MU_TX
		err = wlc_mutx_switch(wlc, (((uint32)int_val & MU_FEATURES_MUTX) != 0), TRUE);
#endif /* WL_MU_TX */
		break;

#if defined(BCMDBG) || defined(BCMDBG_MU)
	case IOV_GVAL(IOV_HEMU_RSSI_THRESH):
		*ret_int_ptr = mu_info->hemu_rssi_thresh;
		break;

	case IOV_SVAL(IOV_HEMU_RSSI_THRESH):
		mu_info->hemu_rssi_thresh = int_val;
		break;
#endif /* BCMDBG */
	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

static void
wlc_mutx_client_clr_blcklst_state(wlc_mutx_info_t *mu_info, scb_t *scb)
{
	mutx_scb_t *mu_scb;

	mu_scb = MUTX_SCB(mu_info, scb);
	if (!mu_scb) {
		return;
	}

	if (mu_scb->item != NULL)
		mutx_blcklst_del_item(mu_info, mu_scb->item);
}

static void
wlc_mutx_clients_clr_blcklst_state(wlc_mutx_info_t *mu_info)
{
	wlc_info_t *wlc = mu_info->wlc;
	scb_t *scb;
	scb_iter_t scbiter;
	uint16 vht_flags;

	/* Iterate over SCBs */
	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (!SCB_ASSOCIATED(scb)) {
			continue;
		}
		if (!SCB_VHT_CAP(scb)) {
			/* STA not VHT capable. Can't be MU capable. */
			continue;
		}
		vht_flags = wlc_vht_get_scb_flags(wlc->vhti, scb);
		if ((vht_flags & SCB_MU_BEAMFORMEE) == 0) {
			continue;
		}

		wlc_mutx_client_clr_blcklst_state(mu_info, scb);
	}
}

/* Update and return the BW policy */
uint32
wlc_mutx_bw_policy_update(wlc_mutx_info_t *mu_info, bool force)
{
	wlc_info_t *wlc = mu_info->wlc;
	scb_t *scb;
	scb_iter_t scbiter;
	uint16 vht_flags;
	uint32 num_stas_bw[BW_80MHZ+1];
	uint32 bw;
	uint32 metric = 0, tmp;
	uint32 bw_policy_old = wlc->mutx_policy->bw_policy;

	if (!force && mu_info->ac_policy_on)
		return 0;

	/* Init the counters */
	for (bw = 0; bw <= BW_80MHZ; bw++)
		num_stas_bw[bw] = 0;

	/* Iterate and count */
	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (!SCB_ASSOCIATED(scb)) {
			continue;
		}
		if (!SCB_VHT_CAP(scb)) {
			/* STA not VHT capable. Can't be MU capable. */
			continue;
		}
		vht_flags = wlc_vht_get_scb_flags(wlc->vhti, scb);
		if ((vht_flags & SCB_MU_BEAMFORMEE) == 0) {
			continue;
		}
		bw = wlc_scb_ratesel_get_link_bw(wlc, scb);
		if (bw >= BW_20MHZ && bw <= BW_80MHZ)
			num_stas_bw[bw]++;
	}

	/* Decide the BW policy based on the counters */
	bw = 0;
	if (num_stas_bw[BW_80MHZ] > 1) {
		bw = BW_80MHZ;
		metric = num_stas_bw[BW_80MHZ] * 80;
	}
	if (num_stas_bw[BW_40MHZ] > 1) {
		tmp = num_stas_bw[BW_40MHZ] * 40;
		if (tmp > metric) {
			bw = BW_40MHZ;
			metric = tmp;
		}
	}
	if (num_stas_bw[BW_20MHZ] > 1) {
		tmp = num_stas_bw[BW_20MHZ] * 20;
		if (tmp > metric) {
			bw = BW_20MHZ;
			metric = tmp;
		}
	}
	wlc->mutx_policy->bw_policy = bw;

	if (force) {
		/* We just want to get the default bw, do not trigger any policy change */
		return (bw);
	}

	if (mu_info->mutx_active && (bw != bw_policy_old)) {
		mutx_scb_t *mu_scb;
		uint8 i;

		/* Evict policy mismatch client */
		for (i = 0; i < MUCLIENT_NUM; i++) {
			scb = mu_info->mu_clients[i];
			if (scb) {
				mu_scb = MUTX_SCB(mu_info, scb);
				if (wlc_scb_ratesel_get_link_bw(wlc, scb) != bw)
					mu_scb->policy_evict = MUTX_POLICY_EVICT_BW;
			}
		}
		if (bw) {
			WL_ERROR(("wl%d: Updating BW admission policy to %sMHz\n",
				mu_info->pub->unit,
				(bw == BW_20MHZ)?"20":
				(bw == BW_40MHZ)?"40":"80"));
			/* More than two MU capable STAs have the same BW */
			mu_info->muclient_elect_pending = TRUE;
		} else
			WL_MUMIMO(("wl%d: No common BW for MU STAs\n", mu_info->pub->unit));
	}

	return (bw);
}

#ifdef WL_PSMX
static void
wlc_mutx_clr_mubf(wlc_info_t *wlc)
{
	osl_t *osh;

	osh = wlc->osh;

	/* set macx command: CLR_MUBF and wait for it to be ack'd by ucode */
	/* For rev128 macx command: add/del MTXV used instead */
	if (D11REV_GE(wlc->pub->corerev, 65) && D11REV_LT(wlc->pub->corerev, 128)) {
		OR_REG(osh, D11_MACCOMMAND_psmx(wlc), MCMDX_CLR_MUBF);
		SPINWAIT((R_REG(osh, D11_MACCOMMAND_psmx(wlc)) & MCMDX_CLR_MUBF), 10000);
	}
}
#endif /* WL_PSMX */

static bool
wlc_mutx_client_scheduler(wlc_mutx_info_t *mu_info, bool pfmon)
{
	uint8 cand_cnt;
	bool evictees = FALSE;

	cand_cnt = mu_candidates_select(mu_info);
	if (pfmon || cand_cnt) {
		mu_clients_eviction(mu_info);

		evictees = wlc_mutx_admit_muclients(mu_info);
	}

	mu_info->mutx_client_scheduler_cnt = 0;

	return evictees;
}

uint8 prio2mufifo[NUMPRIO] = {
	MUTX_AC_BEBK,	/* 0	BE	AC_BE */
	MUTX_AC_BEBK,	/* 1	BK	AC_BK */
	MUTX_AC_BEBK,	/* 2	--	AC_BK */
	MUTX_AC_BEBK,	/* 3	EE	AC_BE */
	MUTX_AC_VI,	/* 4	CL	AC_VI */
	MUTX_AC_VI,	/* 5	VI	AC_VI */
	MUTX_AC_VO,	/* 6	VO	AC_VO */
	MUTX_AC_VO	/* 7	NC	AC_VO */
};

#define MUTX_PKTRATE_THRESH_HIGH	12500	/* high threshold for admission */
#define MUTX_PKTRATE_THRESH_MID		2500	/* mid-range threshold for admission */
#define MUTX_PKTRATE_THRESH_LOW		500	/* low threshold for admission */
#define MUTX_PKTRATE_ADMIT_THRESH	MUTX_PKTRATE_THRESH_LOW	/* threshold for admission */
#define MUTX_PKTRATE_EVICT_THRESH	100	/* threshold for eviction */

static int
wlc_mutx_ac_update(wlc_mutx_info_t *mu_info)
{
	int idx;
	wlc_info_t *wlc = mu_info->wlc;
	mutx_scb_t *mu_scb;
	scb_t *scb;
	scb_iter_t scbiter;
	int prio, ac, bw;
	ampdu_tx_scb_stats_t ampdu_scb_stats;
	uint32 ac_policy, ac_policy_old;
	uint32 bw_policy, bw_policy_old;
	uint32 ac_policy_scores[BW_80MHZ+1][MUTX_AC_VO+1];
	uint32 num_stas_ac[BW_80MHZ+1][MUTX_AC_VO+1];
	uint32 score, pkts_num;
	uint16 vht_flags;

	if (mu_info->ac_policy_on == FALSE)
		return 0;

	for (bw = 0; bw <= BW_80MHZ; bw++) {
		for (ac = MUTX_AC_NONE; ac <= MUTX_AC_VO; ac++) {
			ac_policy_scores[bw][ac] = 0;
			num_stas_ac[bw][ac] = 0;
		}
	}

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (!SCB_ASSOCIATED(scb))
			continue;
		if (!SCB_VHT_CAP(scb)) {
			/* STA not VHT capable. Can't be MU capable. */
			continue;
		}
		vht_flags = wlc_vht_get_scb_flags(wlc->vhti, scb);
		if ((vht_flags & SCB_MU_BEAMFORMEE) == 0) {
			continue;
		}
		if (wlc_ampdu_scbstats_get_and_clr(wlc, scb, &ampdu_scb_stats) == FALSE)
			continue;

		mu_scb = MUTX_SCB(mu_info, scb);
		if (!mu_scb)
			continue;
		bw = (int)wlc_scb_ratesel_get_link_bw(wlc, scb);
		mu_scb->rssi = wlc_lq_rssi_get(wlc, SCB_BSSCFG(scb), scb);
		for (prio = 0; prio < NUMPRIO; prio++) {
			pkts_num = ampdu_scb_stats.tx_pkts_total[prio];
			if (pkts_num == 0)
				continue;
			if (pkts_num < MUTX_PKTRATE_ADMIT_THRESH)
				continue;

			WL_INFORM((MACF" prio=%d rssi=%d nrx=%u tx_pkts %u tx_bytes %u"
				" pkts_sent %u, bytes_sent %u\n",
				ETHER_TO_MACF(scb->ea), prio,
				mu_scb->rssi, mu_scb->nrx,
				ampdu_scb_stats.tx_pkts[prio],
				ampdu_scb_stats.tx_bytes[prio],
				ampdu_scb_stats.tx_pkts_total[prio],
				ampdu_scb_stats.tx_bytes_total[prio]));

			ac = prio2mufifo[prio];
			score = pkts_num/100;
			num_stas_ac[bw][ac]++;
			ac_policy_scores[bw][ac] += score;
			mu_scb->ac_policy_scores[ac] += score;
		}
	}

	score = 0;
	bw_policy_old = wlc->mutx_policy->bw_policy;
	ac_policy_old = wlc->mutx_policy->effective_ac;
	bw_policy = bw_policy_old;
	ac_policy = MUTX_AC_NONE;
	for (bw = BW_80MHZ; bw >= BW_20MHZ; bw--) {
		for (ac = MUTX_AC_BEBK; ac <= MUTX_AC_VO; ac++) {
			WL_INFORM(("wl%d: ac_policy_score[%d][%d] = %u\n",
				mu_info->pub->unit,
				bw, ac, ac_policy_scores[bw][ac]));
			if (num_stas_ac[bw][ac] > 1 &&
				ac_policy_scores[bw][ac] > score) {
				bw_policy = bw;
				ac_policy = ac;
				score = ac_policy_scores[bw][ac];
			}
		}
	}

	WL_INFORM(("wl%d: bw_policy=%u ac_policy=%u score=%u\n",
		mu_info->pub->unit,
		bw_policy, ac_policy, score));

	wlc->mutx_policy->bw_policy = bw_policy;
	wlc->mutx_policy->ac_policy = ac_policy;
	if (ac_policy) {
		/* To avoid unnecessary re-admissions */
		if (ac_policy_old == MUTX_AC_NONE)
			ac_policy_old = ac_policy;
		wlc->mutx_policy->effective_ac = ac_policy;
	}
	if ((bw_policy != bw_policy_old) ||
		(wlc->mutx_policy->effective_ac != ac_policy_old)) {
		for (bw = BW_80MHZ; bw >= BW_20MHZ; bw--) {
			for (ac = MUTX_AC_BEBK; ac <= MUTX_AC_VO; ac++) {
				if (ac_policy_scores[bw][ac]) {
					WL_ERROR(("wl%d: ac_policy_score[%d][%d] = %u\n",
						mu_info->pub->unit,
						bw, ac, ac_policy_scores[bw][ac]));
				}
			}
		}
		WL_ERROR(("wl%d: Updating bw_policy=%u ac_policy=%u\n",
			mu_info->pub->unit,
			bw_policy, ac_policy));
		if (mu_info->mutx_active) {
			/* Evict policy mismatch client */
			for (idx = 0; idx < MUCLIENT_NUM; idx++) {
				scb = mu_info->mu_clients[idx];
				if (!scb) {
					continue;
				}

				mu_scb = MUTX_SCB(mu_info, scb);
				if (!mu_scb) {
					continue;
				}

				if ((mu_scb->ac_policy_scores[ac_policy] == 0) ||
					(wlc_scb_ratesel_get_link_bw(wlc, scb) !=
						bw_policy))
					mu_scb->policy_evict = MUTX_POLICY_EVICT_AC;
			}

			mu_info->muclient_elect_pending = TRUE;
		}
	}

	if (wlc->mutx_policy->bw_policy == 0 || wlc->mutx_policy->ac_policy == 0) {
		/* No suitable BW/AC combination, pick up the default bw */
		wlc_mutx_bw_policy_update(mu_info, TRUE);
	}

	return (wlc->mutx_policy->ac_policy);
}

static void
wlc_ac_policy_scores_update(wlc_mutx_info_t *mu_info)
{
	wlc_info_t *wlc = mu_info->wlc;
	scb_t *scb;
	scb_iter_t scbiter;
	mutx_scb_t *mu_scb;
	uint32 ac;
	ac = wlc->mutx_policy->ac_policy;

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (mu_sta_mu_capable(mu_info, scb)) {
			mu_scb = MUTX_SCB(mu_info, scb);
			if (!mu_scb)
				continue;

			if (ac != MUTX_AC_NONE) {
				mu_scb->score =
					mu_scb->ac_policy_scores[ac];
			} else {
				mu_scb->score = 0;
			}
			mu_scb->time_score++;
		}
	}
}

static void
wlc_ac_policy_scores_reset(wlc_mutx_info_t *mu_info)
{
	wlc_info_t *wlc = mu_info->wlc;
	scb_t *scb;
	scb_iter_t scbiter;
	mutx_scb_t *mu_scb;
	uint32 ac;

	ac = wlc->mutx_policy->ac_policy;
	/* Reset ac_policy_scores after scheduler. */
	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (mu_sta_mu_capable(mu_info, scb)) {
			mu_scb = MUTX_SCB(mu_info, scb);
			if (!mu_scb)
				continue;
			if (ac != MUTX_AC_NONE) {
				mu_scb->ac_policy_scores[ac] = 0;
			}
		}
	}
}

/* retry GID in watchdog reason codes */
#define MUTX_R_GID_DISABLED		0	/* retry GID disabled in watchdog */
#define MUTX_R_GID_NO_BUFFER		1	/* retry GID due to out of buffer */
#define MUTX_R_GID_CALLBACK_FAIL	2	/* retry GID due to callback register fail */
#define MUTX_R_GID_RETRY_CNT		3	/* retry GID due to retry cnt */
#define MUTX_R_GID_BAD_USR_POS		4	/* retry GID due to bad usr pos */
#define MUTX_R_GID_RETRY_WAR		5	/* retry GID due to retry war */

#define MUTX_STATS_MIN_NUPD 5
#define MUTXCNT_SCORE_THRESH 66
#define MUTXCNT_SCORE_NBIN 2

static void
wlc_muclient_clear_stats(wlc_mutx_info_t *mu_info, mutx_stats_t *mutx_stats)
{
	memset(mutx_stats, 0, sizeof(mutx_stats_t));
}

static void
wlc_muclient_clear_pfmon_stats(wlc_mutx_info_t *mu_info, scb_t *scb)
{
	mutx_scb_t *mu_scb;
	mutx_pfmon_stats_t *mutx_pfmon_stats;
	mu_scb = MUTX_SCB(mu_info, scb);

	if (mu_scb == NULL)
		return;

	mutx_pfmon_stats = mu_scb->mutx_pfmon_stats;

	if (mutx_pfmon_stats == NULL)
		return;

	memset(mutx_pfmon_stats, 0, sizeof(mutx_pfmon_stats_t));
}

static uint
wlc_muclients_eval_mutxcnt_stats(wlc_mutx_info_t *mu_info)
{
	scb_t *scb;
	mutx_scb_t *mu_scb;
	mutx_pfmon_stats_t *mutx_stats;
	bool evict = FALSE;
	uint32 mutxcnt, total, mutxcnt_score;
	int i, j;
	uint evict_cnt = 0;
	mutx_scb_list_t *bkt;

	for (i = 0; i < MUCLIENT_NUM_4; i++) {
		scb = mu_info->mu_clients[i];
		if (scb == NULL)
			continue;
		mu_scb = MUTX_SCB(mu_info, scb);
		mutx_stats = mu_scb->mutx_pfmon_stats;

		/* mutxcnt from bin 0 - MUTXCNT_SCORE_NBIN */
		for (j = 0, mutxcnt = 0; j < MUTXCNT_SCORE_NBIN; j++) {
			mutxcnt += mutx_stats->mutxcnt_stat[j];
		}

		/* total # of mu vht tx */
		for (j = 0, total = 0; j < MUTX_MCS_INDEX_NUM; j++) {
			total += mutx_stats->txmu[j];
		}

		if (total) {
			mutxcnt_score = (mutxcnt * 100) / total;
		} else
			mutxcnt_score = 0;

		/* evict client? */
		evict = (total && (mutxcnt_score < MUTXCNT_SCORE_THRESH)) ? TRUE: FALSE;
		mu_scb->blcklst = FALSE;
		if (evict) {
			mu_scb->blcklst = TRUE;
			evict_cnt++;
			WL_MUMIMO(("%s: evict_client "MACF": mutxcnt %u%%\n", __FUNCTION__,
				ETHER_TO_MACF(scb->ea), mutxcnt_score));
		}
	}

	if (evict_cnt) {
		bkt = mutx_blcklst_new(mu_info);
		if (bkt != NULL) {
			for (i = 0; i < MUCLIENT_NUM_4; i++) {
				scb = mu_info->mu_clients[i];
				if (scb == NULL)
					continue;
				mu_scb = MUTX_SCB(mu_info, scb);
				mutx_blcklst_insert_item(mu_info, mu_scb->item, bkt);
				mu_scb->blcklst = FALSE;
			}
			wlc_ac_policy_scores_update(mu_info);
			(void) wlc_mutx_client_scheduler(mu_info, TRUE);
			wlc_ac_policy_scores_reset(mu_info);
		}
	}

	return evict_cnt;
}

static uint
wlc_muclients_eval_gpos_stats(wlc_mutx_info_t *mu_info)
{
	scb_t *scb;
	mutx_scb_t *mu_scb;
	mutx_pfmon_stats_t *mutx_stats;
	bool evict = FALSE;
	int i;
	uint evict_cnt = 0;
	uint8 gbmp;
	mutx_scb_list_t *bkt;

	/* check if each client is in vasip recommended grp */
	for (gbmp = 0, i = 0; i < MUCLIENT_NUM_4; i++) {
		scb = mu_info->mu_clients[i];
		if (scb == NULL)
			continue;
		mu_scb = MUTX_SCB(mu_info, scb);
		mutx_stats = mu_scb->mutx_pfmon_stats;
		if (mutx_stats->mu_ncons < MUTX_PKTRATE_EVICT_THRESH)
			continue;

		if (mutx_stats->mutx_cnt &&
			mutx_stats->gpos_cnt[i]) {
			gbmp |= (1 << i);
		}
	}

	if (!gbmp) {
		return 0;
	}

	for (i = 0;  i < MUCLIENT_NUM_4; i++) {
		scb = mu_info->mu_clients[i];
		if (scb == NULL)
			continue;
		mu_scb = MUTX_SCB(mu_info, scb);
		mutx_stats = mu_scb->mutx_pfmon_stats;
		/* evict client if it is being left out in vasip recommended grp */
		evict =  ((mutx_stats->mu_ncons > MUTX_PKTRATE_EVICT_THRESH) &&
				(gbmp & 1) == 0) ? TRUE: FALSE;
		gbmp >>= 1;
		mu_scb->blcklst = FALSE;
		if (evict) {
			mu_scb = MUTX_SCB(mu_info, scb);
			evict_cnt++;
			mu_scb->blcklst = TRUE;
			WL_MUMIMO(("%s: evict_client "MACF":\n", __FUNCTION__,
				ETHER_TO_MACF(scb->ea)));
		}
	}

	if (evict_cnt) {
		bkt = mutx_blcklst_new(mu_info);
		if (bkt != NULL) {
			for (i = 0; i < MUCLIENT_NUM_4; i++) {
				scb = mu_info->mu_clients[i];
				if (scb == NULL)
					continue;
				mu_scb = MUTX_SCB(mu_info, scb);
				mutx_blcklst_insert_item(mu_info, mu_scb->item, bkt);
				mu_scb->blcklst = FALSE;
			}
			wlc_ac_policy_scores_update(mu_info);
			(void)wlc_mutx_client_scheduler(mu_info, TRUE);
			wlc_ac_policy_scores_reset(mu_info);
		}
	}

	return evict_cnt;
}

static void
wlc_muclients_clear_pfmon_stats(wlc_mutx_info_t *mu_info)
{
	scb_t *scb;
	int i;

	for (i = 0; i < MUCLIENT_NUM; i++) {
		scb = mu_info->mu_clients[i];
		if (scb == NULL)
			continue;
		wlc_muclient_clear_pfmon_stats(mu_info, scb);
	}

	mu_info->txstats_nupd = 0;
}

static void
wlc_muclient_pfmon(wlc_mutx_info_t *mu_info)
{
	uint evict_cnt;

	if (!(mu_info->txstats_nupd)) {
		wlc_muclients_clear_pfmon_stats(mu_info);
	}

	if (mu_info->txstats_nupd > MUTX_STATS_MIN_NUPD) {
		evict_cnt = wlc_muclients_eval_mutxcnt_stats(mu_info);
		if (!evict_cnt && mu_info->pfmon_gpos)
			wlc_muclients_eval_gpos_stats(mu_info);
		wlc_muclients_clear_pfmon_stats(mu_info);
		return;
	}

	mu_info->txstats_nupd++;
}

static int
wlc_mutx_fifo_hold_set(wlc_info_t *wlc, void *fifo_bitmap)
{
	uint i;
	uint8 flush_fbmp[TX_FIFO_BITMAP_SIZE_MAX] = { 0 };
	bool need_flush = FALSE;
	ASSERT(fifo_bitmap != NULL);

	if ((wlc->txfifo_detach_pending) || (wlc->excursion_active)) {
		return BCME_NOTREADY;
	}

	for (i = 0; i < WLC_HW_NFIFO_INUSE(wlc); i++) {
		if (!isset(fifo_bitmap, i)) {
			continue;
		}
		if (TXPKTPENDGET(wlc, i) > 0) {
			/* flush only fifos with pending pkts */
			setbit(flush_fbmp, i);
			need_flush = TRUE;
		}

		/* Do not allow any new packets to flow to the fifo from the active_queue
		 * while we are synchronizing the fifo for the active_queue.
		 * The wlc_sync_txfifos() process started below will trigger tx status
		 * processing that could trigger new pkt enqueues to the fifo.
		 * The 'hold' call will flow control the fifo.
		 */
		txq_hw_hold_set(wlc->active_queue->low_txq, i);
	}

	/* flush the fifos and process txstatus from packets that
	 * were sent before the flush
	 */
	if (need_flush) {
		wlc_sync_txfifos(wlc, wlc->active_queue, flush_fbmp, FLUSHFIFO);
	}

	return BCME_OK;
}

static void
wlc_mutx_fifo_hold_clr(wlc_info_t *wlc, void *fifo_bitmap)
{
	uint i;

	ASSERT(fifo_bitmap != NULL);

	for (i = 0; i < WLC_HW_NFIFO_INUSE(wlc); i++) {
		if (!isset(fifo_bitmap, i)) {
			continue;
		}

		/* Clear the hw fifo hold since we are done with the MU Client scheduler. */
		txq_hw_hold_clr(wlc->active_queue->low_txq, i);
	}
}

/* Function registered as this module's watchdog callback. Called once
 * per second. Checks if one of the current MU clients has not yet
 * ack'd a Group ID Mgmt frame, and in that case, sends one to the
 * MU client, if the number of retries has not reached the limit.
 * Also, if MU client selection is pending, select a new set of MU
 * clients.
 */
static void
wlc_mutx_watchdog(void *mi)
{
	wlc_mutx_info_t *mu_info = (wlc_mutx_info_t*) mi;
	mutx_scb_t *mu_scb;
	wlc_info_t *wlc = mu_info->wlc;
	mutx_scb_list_t *pos, *next;
	int idx;
	uint16 depart_idx_bmp = 0;
	uint8 depart_fifo_bmp[TX_FIFO_BITMAP_SIZE_MAX] = { 0 };

	if ((wlc->txfifo_detach_pending) || (wlc->excursion_active) ||
	    SCAN_IN_PROGRESS(wlc->scan)) {
		/* Defer mutx watchdog until scan completes. */
		WL_MUMIMO(("wl%d: MUTX wd blocked: fifodetach: %d, excur: %d scan: %d\n",
			wlc->pub->unit, wlc->txfifo_detach_pending, wlc->excursion_active,
			SCAN_IN_PROGRESS(wlc->scan)));
		return;
	}

	wlc_mutx_ac_update(mu_info);
	/* If MU-MIMO tx is disabled, no-op */
	if (!mu_info->mutx_active) {
		/* Check if we need to switch from SU to MU */
		if (wlc_txbf_mu_cap_stas(wlc->txbf) >= 2) {
			if (TXPKTPENDTOT(wlc)) {
				wlc_block_datafifo(wlc, DATA_BLOCK_MUTX, DATA_BLOCK_MUTX);
			} else {
				wlc_mutx_switch(wlc, TRUE, FALSE);
			}
		}
		return;
	}

	/* Check if we need to switch from MU to SU and we're set to allow it */
	if (wlc_txbf_mu_cap_stas(wlc->txbf) < 2) {
		if (TXPKTPENDTOT(wlc)) {
			wlc_block_datafifo(wlc, DATA_BLOCK_MUTX, DATA_BLOCK_MUTX);
		} else {
			wlc_mutx_switch(wlc, FALSE, FALSE);
		}
		return;
	}
	FOREACH_LIST_POS(pos, next, mu_info->on_hold_list) {
		mu_scb = (mutx_scb_t *)pos->data;
		if (--mu_scb->on_hold_dur) {
			continue;
		}
		mutx_scb_list_rem(pos);
		/* If any STA is removed from on_hold_list,
		 * we should trigger muclient_elect_pending to selecting new set.
		 */
		mu_info->muclient_elect_pending = TRUE;
	}

	/* Check if we need to do MU client selection */
	if (mu_info->muclient_elect_pending) {
		mu_clients_select(mu_info);
	}

	/* Check if we need to do MU Client scheduler */
	if (mu_info->mutx_client_scheduler) {
		mu_info->mutx_client_scheduler_cnt++;
		if (mu_info->mutx_client_scheduler_cnt >= mu_info->mutx_client_scheduler_dur) {
			wlc_ac_policy_scores_update(mu_info);
			(void)wlc_mutx_client_scheduler(mu_info, FALSE);
			wlc_ac_policy_scores_reset(mu_info);
		} else if (mu_info->muclient_pfmon) {
			wlc_muclient_pfmon(mu_info);
		}
	}

	/* Check if we need to send a Group ID management message to a client */
	for (idx = 0; idx < MUCLIENT_NUM; idx++) {
		if (mu_info->mu_clients[idx] == NULL) {
			continue;
		}

		mu_scb = MUTX_SCB(mu_info, mu_info->mu_clients[idx]);
		if (!mu_scb || (!mu_scb->membership_change && !mu_scb->gid_retry_war)) {
			continue;
		}

		if (mu_scb->gid_retry_war && mu_scb->retry_gid == MUTX_R_GID_RETRY_WAR) {
			mu_scb->gid_retry_war--;
			if (!mu_scb->membership_change && mu_scb->gid_retry_war)
				continue;
		}

		if (mu_scb->gid_send_cnt < MU_GID_SEND_LIMIT) {
			if (mu_group_id_msg_send(mu_info->wlc, mu_info->mu_clients[idx],
			                         mu_scb->membership,
			                         mu_scb->position) == BCME_OK) {
				mu_scb->gid_send_cnt++;
			}
		}
		else if (mu_scb->retry_gid != MUTX_R_GID_DISABLED) {
			/* Exhausted retries for GID management. Remove STA from
			 * MU client set and select a replacement. This STA could
			 * be chosen again if another spot opens (i.e., we are not
			 * permanently locking it out).
			 *
			 * Implement a hold down time (e.g., 60 sec)
			 * during which STA cannot be selected as a MU client, to avoid
			 * continually thrashing the MU client set and continually
			 * sending GID messages that go unacked? Might happen if multiple
			 * MU-capable STAs go away w/o disassociating. Not sure if that's
			 * likely enough to add the extra complexity. I'd guess not since
			 * AP only sends 1 GID msg per second to each such MU client, and
			 * scb will eventually time out.
			 */
			/* Downgrade to SU */
			depart_idx_bmp |= (1 << idx);
			wlc_mutx_sta_fifo_bitmap(wlc->mutx, mu_scb->scb, depart_fifo_bmp);
		}
	}

	if (depart_idx_bmp) {
		scb_t *scb;
		uint8 su_fifo_bitmsk = 0xff;
		BCM_REFERENCE(su_fifo_bitmsk);

		/* If any client is departing the mu set, flush the high fifos
		 * the sta has been using before the sta starts using low fifos
		 * in order to avoid OOO tx.
		 */
		WL_ERROR(("wl%d: %s: departing fifo[0] 0x%x, idx 0x%x\n",
			mu_info->pub->unit, __FUNCTION__, depart_fifo_bmp[0], depart_idx_bmp));
		if (D11REV_GE(mu_info->pub->corerev, 128)) {
			su_fifo_bitmsk = 0x3f;
		}
		ASSERT((depart_fifo_bmp[0] & su_fifo_bitmsk) == 0); /* should be only high fifos */

#ifdef WL_PSMX
		/* set mu_sounding timer to be 0 */
		wlc_txbf_mutimer_update(wlc->txbf, TRUE);
		wlc_mutx_clr_mubf(wlc);
#endif // endif
		/* do txfifo_sync which suspend_mac and suspend/flush aqm_txfifo */
		if (wlc_mutx_fifo_hold_set(wlc, depart_fifo_bmp) != BCME_OK) {
			WL_ERROR(("%s: wlc_tx_fifo_hold_set fail!\n", __FUNCTION__));
			ASSERT(0);
		}

		for (idx = 0; depart_idx_bmp; idx++, depart_idx_bmp >>= 1) {
			if (!(depart_idx_bmp & 0x1)) {
				continue;
			}
			scb = mu_info->mu_clients[idx];
			if (scb == NULL) {
				/* if during FIFO flush we either succesfully completed a disassoc/
				 * deauth or deinited an SCB, it may be gone from mu_clients already
				 */
				continue;
			}
			mu_scb = MUTX_SCB(mu_info, scb);
			mutx_scb_list_add_tail(mu_info->on_hold_list, mu_scb->item);
			mu_scb->on_hold_dur = MU_STA_MAX_HOLD_DUR;
			mu_sta_mu_disable(mu_info, scb);
			wlc_txbf_link_upd(wlc->txbf, scb);
		}
		wlc_mutx_fifo_hold_clr(wlc, depart_fifo_bmp);
#ifdef WL_PSMX
		/* re-enable mu_sounding timer */
		wlc_txbf_mutimer_update(wlc->txbf, FALSE);
#endif // endif
	}
	return;
}

/* Registered callback when radio comes up. */
static int
wlc_mutx_up(void *mi)
{
	wlc_mutx_info_t *mu_info = (wlc_mutx_info_t*) mi;
	wlc_info_t *wlc = mu_info->wlc;

	/* Check if system is MU BFR capable, both physically and by current
	 * configuration, and if so, set _mu_tx. Configuration can only change
	 * while radio is down. So checking at up time is sufficient.
	 */
	wlc_mutx_update(wlc, wlc_txbf_mutx_enabled(wlc->txbf));

#ifdef WL_PSMX
	wlc_mutx_hostflags_update(wlc);
#endif /* WL_PSMX */

	/* Make sure we start with empty MU client set */
	// XXX: after big hammer this call is clearing scb membership
	// mu_client_set_clear(mu_info);

#ifdef MAC_AUTOTXV_OFF
	/* re-init all the already admitted vht-mu users info maintained in shm */
	wlc_mutx_admit_users_reinit(mu_info);
#endif // endif

	/* Determine whether MU TX can be active */
	wlc_mutx_active_update(mu_info);

	return BCME_OK;
}

/* Allocate memory for an scb cubby. Point real cubby to allocated memory.
 * Initialize client index.
 * Inputs
 *   mu_info - MU TX state
 *   scb     - scb whose cubby is allocated
 *
 * Returns
 *   MU SCB if allocation successful
 *   NULL otherwise
 */
static mutx_scb_t *
mu_scb_cubby_alloc(wlc_mutx_info_t *mu_info, scb_t *scb)
{
	mutx_scb_t *mu_scb;
	mutx_scb_cubby_t *cubby = MUTX_SCB_CUBBY(mu_info, scb);

	mu_scb = MALLOCZ(mu_info->osh, sizeof(mutx_scb_t));
	if (!mu_scb) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			mu_info->pub->unit, __FUNCTION__, MALLOCED(mu_info->osh)));
		WLCNTINCR(mu_info->mutx_scb_alloc_fail);
		return NULL;
	}
#ifdef WLCNT
	if ((mu_scb->mutx_stats = MALLOCZ(mu_info->osh, sizeof(mutx_stats_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			mu_info->pub->unit, __FUNCTION__, MALLOCED(mu_info->osh)));

		MFREE(mu_info->osh, mu_scb, sizeof(mutx_scb_t));
		return NULL;
	}
	if ((mu_scb->mutx_pfmon_stats = MALLOCZ(mu_info->osh,
		sizeof(mutx_pfmon_stats_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			mu_info->pub->unit, __FUNCTION__, MALLOCED(mu_info->osh)));
		MFREE(mu_info->osh, mu_scb->mutx_stats, sizeof(mutx_stats_t));
		MFREE(mu_info->osh, mu_scb, sizeof(mutx_scb_t));
		return NULL;
	}
	if ((mu_scb->mutx_snr_calib_stats
				= MALLOCZ(mu_info->osh, sizeof(mutx_snr_calib_stats_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			mu_info->pub->unit, __FUNCTION__, MALLOCED(mu_info->osh)));

		MFREE(mu_info->osh, mu_scb->mutx_pfmon_stats, sizeof(mutx_pfmon_stats_t));
		MFREE(mu_info->osh, mu_scb->mutx_stats, sizeof(mutx_stats_t));
		MFREE(mu_info->osh, mu_scb, sizeof(mutx_scb_t));
		return NULL;
	}
#endif	/* WLCNT */

	mu_scb->item = mutx_scb_list_new(mu_info, mu_scb);
	if (mu_scb->item == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			mu_info->pub->unit, __FUNCTION__, MALLOCED(mu_info->osh)));
#ifdef WLCNT
		MFREE(mu_info->osh, mu_scb->mutx_snr_calib_stats, sizeof(mutx_snr_calib_stats_t));
		MFREE(mu_info->osh, mu_scb->mutx_pfmon_stats, sizeof(mutx_pfmon_stats_t));
		MFREE(mu_info->osh, mu_scb->mutx_stats, sizeof(mutx_stats_t));
#endif // endif
		MFREE(mu_info->osh, mu_scb, sizeof(mutx_scb_t));
		return NULL;
	}

	cubby->mutx_scb = mu_scb;
	/* New STA is not in the MU client set by default */
	mu_scb->client_index = MU_CLIENT_INDEX_NONE;
	mu_scb->retry_gid = MUTX_R_GID_RETRY_CNT;
	mu_scb->scb = scb;
	mu_scb->on_hold_dur = 0;
	mu_scb->score = 0;
	mu_scb->time_score = 0;
	mu_scb->blcklst = FALSE;
	mu_scb->mu_link_permit = FALSE;
	return mu_scb;
}

/* Iinitialize this module's scb state. */
static int
mu_scb_cubby_init(void *ctx, scb_t *scb)
{
	wlc_mutx_info_t *mu_info = (wlc_mutx_info_t*) ctx;

	if (!SCB_INTERNAL(scb)) {
		if (mu_scb_cubby_alloc(mu_info, scb) == NULL) {
			return BCME_ERROR;
		}
	}
	return BCME_OK;
}

/* Deinitialize this module's scb state. Remove the STA from the MU client set.
 * Free the module's state structure.
 */
static void
mu_scb_cubby_deinit(void *ctx, scb_t *scb)
{
	wlc_mutx_info_t *mu_info = (wlc_mutx_info_t*) ctx;
	wlc_info_t *wlc = mu_info->wlc;
	mutx_scb_cubby_t *cubby = MUTX_SCB_CUBBY(mu_info, scb);
	mutx_scb_t *mu_scb = cubby->mutx_scb;

	if (!mu_scb)
		return;

	if (D11REV_GE(wlc->pub->corerev, 129) && HE_MMU_ENAB(wlc->pub) &&
		SCB_HE_CAP(scb)) {
		mu_scb->assocd = FALSE;
		wlc_mutx_he_eval_and_admit_clients(mu_info, scb, mu_scb->assocd);
	} else {
		mu_sta_mu_disable(mu_info, scb);

		/* Cancel any pending wlc_mutx_gid_complete packet callbacks. */
		wlc_pcb_fn_find(wlc->pcb, wlc_mutx_gid_complete, (void*) mu_scb, TRUE);
	}

	if (mu_scb->item != NULL) {
		mutx_blcklst_del_item(mu_info, mu_scb->item);
		mutx_scb_list_del(mu_info, mu_scb->item);
	}
#ifdef WLCNT
	if (mu_scb->mutx_snr_calib_stats != NULL) {
		MFREE(mu_info->osh, mu_scb->mutx_snr_calib_stats, sizeof(mutx_snr_calib_stats_t));
	}
	if (mu_scb->mutx_pfmon_stats != NULL) {
		MFREE(mu_info->osh, mu_scb->mutx_pfmon_stats, sizeof(mutx_pfmon_stats_t));
	}
	if (mu_scb->mutx_stats != NULL) {
		MFREE(mu_info->osh, mu_scb->mutx_stats, sizeof(mutx_stats_t));
	}
#endif // endif
	MFREE(mu_info->osh, mu_scb, sizeof(mutx_scb_t));
	cubby->mutx_scb = NULL;
}

#if defined(BCMDBG) || defined(BCMDBG_MU)
static int
wlc_mutx_sendgid(wlc_mutx_info_t *mu_info)
{
	mutx_scb_t *mu_scb;
	int i, success = BCME_OK;

	/* Send a Group ID management message to each MU client */
	for (i = 0; i < MUCLIENT_NUM; i++) {
		if (mu_info->mu_clients[i] == NULL) {
			continue;
		}

		mu_scb = MUTX_SCB(mu_info, mu_info->mu_clients[i]);

		if (mu_group_id_msg_send(mu_info->wlc, mu_info->mu_clients[i],
		                         mu_scb->membership,
		                         mu_scb->position) == BCME_OK) {
			WL_MUMIMO(("Mu-MIMO client index: %u GID frame Successful\n", i));
		} else {
			WL_MUMIMO(("Mu-MIMO client index: %u GID frame Failed\n", i));
			success = BCME_ERROR;
		}
	}
	return success;

}
#endif /* BCMDBG || BCMDBG_MU */

#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU) || defined(BCMDBG_MU)
/* Dump MU-MIMO cubby data for a given STA. */
static void
mu_scb_cubby_dump(void *ctx, scb_t *scb, bcmstrbuf_t *b)
{
	uint16 i;
	wlc_mutx_info_t *mu_info = (wlc_mutx_info_t*) ctx;
	mutx_scb_t *mu_scb = MUTX_SCB(mu_info, scb);

	if (mu_scb == NULL) {
		return;
	}

	bcm_bprintf(b, "     MU-MIMO client index: %u\n", mu_scb->client_index);
	if (SCB_VHTMU(scb) && mu_any_membership(mu_scb->membership)) {
		bcm_bprintf(b, "%15s %15s\n", "Group ID", "User Position");
		for (i = MU_GROUP_ID_MIN; i <= MU_GROUP_ID_MAX; i++) {
			if (isset(mu_scb->membership, i)) {
				bcm_bprintf(b, "%15u %15u\n", i,
				            mu_user_pos_get(mu_scb->position, i));
			}
		}
	}
}
#endif // endif

static int
wlc_mutx_sta_mucidx_set(wlc_mutx_info_t *mu_info, scb_t *scb)
{
	wlc_info_t *wlc = mu_info->wlc;
	mutx_scb_t *mu_scb = MUTX_SCB(mu_info, scb);
	uint16 i = BF_SHM_IDX_INV, prev_idx = 0, reason_code = 0;
	uint16 max_vhtmu_usrs, max_mmu_usrs;
	uint16 start = 0, end;

	BCM_REFERENCE(reason_code);
	BCM_REFERENCE(prev_idx);

	if (D11REV_LT(mu_info->pub->corerev, 128)) {
		return wlc_txbf_get_mubfi_idx(wlc->txbf, scb);
	}

	if (!mu_scb) {
		reason_code = 1;
		i = BF_SHM_IDX_INV;
		goto done;
	}

	max_vhtmu_usrs = wlc_txcfg_max_clients_get(wlc->txcfg, VHTMU);
	max_mmu_usrs = wlc_txcfg_max_mmu_clients_get(wlc->txcfg);
	end = max_vhtmu_usrs;
	if (SCB_HE_CAP(scb)) {
		start = max_vhtmu_usrs;
		end = max_mmu_usrs;
	}

	prev_idx = mu_scb->client_index;
	ASSERT(mu_scb->client_index == MU_CLIENT_INDEX_NONE);
	for (i = start; i < end; i++) {
		if ((mu_info->mu_clients[i] == NULL) ||
			mu_info->mu_clients[i] == scb) {
			break;
		}
	}

	if (i == end) {
		reason_code = 3;
		i = BF_SHM_IDX_INV; /* keep return value same as previous */
	}

done:
	WL_MUTX(("wl%d: STA "MACF" fifo index %02x -> %02x %d\n",
		mu_info->pub->unit, ETHER_TO_MACF(scb->ea),
		prev_idx, i, reason_code));
	return i;
}

/**
 * Get the mu client index assigned to a MMU (VHT/HE) admitted STA
 *
 * This is also used to assign fifo index to a STA
 *
 * @param muched	handle to musched context
 * @param scb		scb pointer
 * @return		index (0-15), if assigned
 *			-1 if unassigned
 */
int
wlc_mutx_sta_mucidx_get(wlc_mutx_info_t *mu_info, scb_t *scb)
{
	wlc_info_t *wlc;
	mutx_scb_t *mu_scb;
	uint16 i = BF_SHM_IDX_INV;
	uint16 max_vhtmu_usrs, max_mmu_usrs;
	uint16 start = 0, end;

	if (!mu_info) {
		return BF_SHM_IDX_INV;
	}

	wlc = mu_info->wlc;
	mu_scb = MUTX_SCB(mu_info, scb);
	if (D11REV_LT(mu_info->pub->corerev, 128)) {
		return wlc_txbf_get_mubfi_idx(wlc->txbf, scb);
	}

	if (SCB_INTERNAL(scb)) {
		return BF_SHM_IDX_INV;
	}

	if (!mu_scb) {
		return BF_SHM_IDX_INV;
	}

	max_vhtmu_usrs = wlc_txcfg_max_clients_get(wlc->txcfg, VHTMU);
	max_mmu_usrs = wlc_txcfg_max_mmu_clients_get(wlc->txcfg);
	end = max_vhtmu_usrs;
	if (SCB_HE_CAP(scb)) {
		start = max_vhtmu_usrs;
		end = max_mmu_usrs;
	}

	for (i = start; i < end; i++) {
		if (mu_info->mu_clients[i] == scb) {
			break;
		}
	}

	if (i == end) {
		return BF_SHM_IDX_INV; /* keep return value same as previous */
	}

	return i;
}

/* Add a STA to the current MU client set.
 * Returns
 *   BCME_OK if STA is added to MU client set
 *   BCME_ERROR if STA already a member of MU client set
 *   BCME_NORESOURCE if MU client set is already full
 *   BCME_BADARG if scb cubby is NULL. All MU clients must have a non-NULL cubby
 */
static int
mu_client_set_add(wlc_mutx_info_t *mu_info, scb_t *scb)
{
#ifdef MAC_AUTOTXV_OFF
	wlc_info_t *wlc = mu_info->wlc;
#endif // endif
	mutx_scb_t *mu_scb = MUTX_SCB(mu_info, scb);
	uint16 i;
	int err;

	if (!mu_scb) {
		return BCME_BADARG;
	}

	if (mu_in_client_set(mu_info, scb))
		return BCME_ERROR;

	i = wlc_mutx_sta_mucidx_set(mu_info, scb);
	if (i == BF_SHM_IDX_INV) {
		return BCME_NORESOURCE;
	}

	ASSERT(i < MUCLIENT_NUM);

	mu_info->mu_clients[i] = scb;
	mu_scb->client_index = i;
	mu_scb->time_score = 0;

#ifdef MAC_AUTOTXV_OFF
	err = wlc_macreq_upd_bfi(wlc, scb, i, TRUE);
	if (err != BCME_OK && err != BCME_UNSUPPORTED) {
		WL_ERROR(("wl%d: MAC command failed to update bfi"
			"for STA "MACF" i:%u. err:%d\n",
			mu_info->pub->unit, ETHER_TO_MACF(scb->ea), i, err));
		return BCME_ERROR;
	}
#endif // endif

	/* Use default group membership. This sends Group ID message to client. */
	if (D11REV_IS(mu_info->pub->corerev, 130)) {
		err = wlc_mutx_membership_set(mu_info, i,
				dflt_membership_2x2[i], dflt_position_2x2[i]);
	} else {
		err = wlc_mutx_membership_set(mu_info, i, dflt_membership[i], dflt_position[i]);
	}

	if (err != BCME_OK) {
		/* unable to set membership; clear the list */
		mu_info->mu_clients[i] = NULL;
		mu_scb->client_index = MU_CLIENT_INDEX_NONE;
	} else {
		WL_ERROR(("wl%d: STA "MACF" selected as MU client at index %u.\n",
			mu_info->pub->unit, ETHER_TO_MACF(scb->ea), i));
	}
	return err;
}

/* Remove STA from current MU client set.
 * Returns
 *   BCME_OK if STA is removed from MU client set
 *   BCME_NOTFOUND if STA is not found
 */
static int
mu_client_set_remove(wlc_mutx_info_t *mu_info, scb_t *scb)
{
	mutx_scb_t *mu_scb = MUTX_SCB(mu_info, scb);
	wlc_info_t *wlc = mu_info->wlc;
#ifdef MAC_AUTOTXV_OFF
	int err;
#endif // endif

	if (!mu_scb) {
		return BCME_NOTFOUND;
	}

	ASSERT(mu_info->mu_clients[mu_scb->client_index] == scb);

#ifdef MAC_AUTOTXV_OFF
	err = wlc_macreq_upd_bfi(wlc, scb, mu_scb->client_index, FALSE);
	if (err != BCME_OK && err != BCME_UNSUPPORTED) {
		WL_ERROR(("wl%d: MAC command failed to update bfi"
			"for STA "MACF" i:%u\n",
			mu_info->pub->unit, ETHER_TO_MACF(scb->ea), mu_scb->client_index));
	}
#endif // endif

	wlc_tx_fifo_scb_flush(wlc, scb);
	mu_info->mu_clients[mu_scb->client_index] = NULL;
	mu_scb->client_index = MU_CLIENT_INDEX_NONE;
	mu_scb->time_score = 0;
	mu_scb->policy_evict = MUTX_POLICY_EVICT_NONE;

	WL_ERROR(("wl%d: STA "MACF" removed from MU client set with index %d.\n",
		mu_info->pub->unit, ETHER_TO_MACF(scb->ea),
		mu_scb->client_index));

	if (!SCB_VHTMU(scb)) {
		WL_MUTX(("wl%d: MU already disabled, skip\n", mu_info->pub->unit));
		return BCME_OK;
	}
	/* Inform the ucode/hw of client set change */
	SCB_VHTMU_DISABLE(scb);

	wlc_mutx_membership_clear(mu_info, scb);

	/* Update the ampdu_mpdu number */
	scb_ampdu_update_config_mu(wlc->ampdu_tx, scb);
	if (WLC_TXC_ENAB(wlc)) {
		wlc_txc_inv(wlc->txc, scb);
	}
	if (RATELINKMEM_ENAB(wlc->pub)) {
		wlc_ratelinkmem_update_link_entry(wlc, scb);
	}

#ifdef AP
	wlc_apps_ps_flush(wlc, scb);
#endif // endif
#ifdef WLTAF
	wlc_taf_scb_state_update(wlc->taf_handle, scb, TAF_PARAM(SCB_MU(scb)),
		TAF_SCBSTATE_MU_MIMO);
#endif // endif
	return BCME_OK;
}

/* Clear the entire MU client set. */
static void
mu_client_set_clear(wlc_mutx_info_t *mu_info)
{
	int i;
	scb_t *scb;
	uint16 max_vhtmu_usrs = wlc_txcfg_max_clients_get(mu_info->wlc->txcfg, VHTMU);

	for (i = 0; i < max_vhtmu_usrs; i++) {
		scb = mu_info->mu_clients[i];
		if (scb) {
			if (mu_client_set_remove(mu_info, scb) != BCME_OK) {
				WL_ERROR(("wl%d:Failed to remove STA "MACF" from MU client set.\n",
					mu_info->pub->unit, ETHER_TO_MACF(scb->ea)));
			}
		}
	}
}

/* Report whether a given STA is one of the current MU clients. */
static bool
mu_in_client_set(wlc_mutx_info_t *mu_info, scb_t *scb)
{
	return (wlc_mutx_sta_client_index(mu_info, scb) != MU_CLIENT_INDEX_NONE);
}

/* Returns TRUE if the current MU client set is full, or reach to max_muclients limitation. */
static bool
mu_client_set_full(wlc_mutx_info_t *mu_info)
{
	int i, clients = 0;
	uint16 max_vhtmu_usrs = wlc_txcfg_max_clients_get(mu_info->wlc->txcfg, VHTMU);

	for (i = 0; i < max_vhtmu_usrs; i++) {
		if (mu_info->mu_clients[i])
			clients++;
	}

	if (clients >= max_vhtmu_usrs) {
		return TRUE;
	}

	return FALSE;
}

/* Check if the addition of a new MU-capable STA should cause a change
 * to the MU client set. For now, if the MU client set is not full,
 * and MU TX is active, the STA is added to the MU client set. In the future,
 * this could be one trigger for the MU client selection algorithm to run.
 */
static void
mu_client_set_new_sta(wlc_mutx_info_t *mu_info, scb_t *scb)
{
	mutx_scb_t *mu_scb = MUTX_SCB(mu_info, scb);

	if (!mu_info->mutx_active)
		return;

	if (mu_client_set_full(mu_info)) {
		return;
	}

	if (mu_scb == NULL) {
		return;
	}

	/* Candidate set has room to add new STA. */
	(void)mu_client_set_add(mu_info, scb);
}

/* Select the MU-capable STAs that are not currently in the
 * MU client set.
 */
static uint8
mu_candidates_select(wlc_mutx_info_t *mu_info)
{
	wlc_info_t *wlc = mu_info->wlc;
	scb_t *scb;
	scb_iter_t scbiter;
	mutx_scb_t *mu_scb;
	uint8 cnt = 0, i;
	bool max_limit = mu_client_set_full(mu_info);

	if (!mu_info->mutx_active)
		return 0;

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		mu_scb = MUTX_SCB(mu_info, scb);
		if ((!mu_scb) || (max_limit && (!mu_info->mutx_client_scheduler ||
			mu_scb->score == 0)) || (mu_scb->on_hold_dur) ||
			(mu_scb->client_index != MU_CLIENT_INDEX_NONE))
			continue;

		if (mu_sta_mu_ready(mu_info, scb, TRUE)) {
			mutx_scb_list_add_tail(mu_info->candidate_list,
				mu_scb->item);
			cnt++;
		} else if (mu_sta_mu_ready(mu_info, scb, FALSE)) {
			mutx_scb_list_add_tail(mu_info->alternate_list,
				mu_scb->item);
			cnt++;
		}
	}

	/* If mutx_client_samebw is 0, it means we can have mixed bw MU Client.
	 * Add policy mismatch MU Client to alternate_list.
	 */
	if (!mu_info->mutx_client_samebw) {
		for (i = 0; i < MUCLIENT_NUM; i++) {
			scb = mu_info->mu_clients[i];
			if (scb) {
				mu_scb = MUTX_SCB(mu_info, scb);
				ASSERT(mu_scb != NULL);
				ASSERT(mu_scb->item != NULL);
				if (mu_scb->policy_evict) {
					mutx_scb_list_add_tail(mu_info->alternate_list,
						mu_scb->item);
					cnt++;
				}
			}
		}
	}

	return cnt;
}

static void
wlc_mutx_blcklst_match(wlc_mutx_info_t *mu_info)
{
	scb_t *candidate;
	mutx_scb_t *mu_scb;
	mutx_scb_list_t *cand_entry, *alt_entry, *pos, *next, *blcklst_bkt = NULL;
	bool alt_list = FALSE;
#ifdef NOT_YET
	mutx_scb_list_t *start, *head, *blcklst_pos;
#endif // endif
	BCM_REFERENCE(next);
	BCM_REFERENCE(mu_scb);
	BCM_REFERENCE(candidate);

	if (LIST_IS_EMPTY(mu_info->admit_list))
		return;

#ifdef BCMDBG
	WL_MUMIMO(("admit list before blcklst match\n"));
	FOREACH_LIST_POS(pos, next, mu_info->admit_list) {
		mu_scb = (mutx_scb_t *)pos->data;
		ASSERT(mu_scb != NULL);
		candidate = mu_scb->scb;
		ASSERT(candidate != NULL);
		WL_MUMIMO((""MACF"\n", ETHER_TO_MACF(candidate->ea)));
	}
#endif // endif

	/* admit_list in blcklst? */
	while ((pos = mutx_blcklst_match(mu_info, mu_info->admit_list, &blcklst_bkt))) {
		/* select new cand from max list */
		cand_entry = mutx_scb_list_max(mu_info->candidate_list);
		if (cand_entry == NULL) {
			alt_list = TRUE;
			break;
		}
		mutx_scb_list_rem(pos);
		mu_scb = (mutx_scb_t *)pos->data;
		mu_scb->reason = MUTX_REASON_BLACKLIST_EVICT;
		mutx_scb_list_rem(cand_entry);
		mu_scb = (mutx_scb_t *)cand_entry->data;
		mu_scb->reason = MUTX_REASON_BLACKLIST_ADMIT;
		mutx_scb_list_add_tail(mu_info->admit_list, cand_entry);
	}

	/* mu client set not full, check alternate_list */
	while (alt_list && (pos = mutx_blcklst_match(mu_info, mu_info->admit_list, &blcklst_bkt))) {
		/* select new cand from alternate_list */
		alt_entry = mutx_scb_list_max(mu_info->alternate_list);
		if (alt_entry == NULL) {
			break;
		}
		ASSERT(pos);
		mutx_scb_list_rem(pos);
		mu_scb = (mutx_scb_t *)pos->data;
		mu_scb->reason = MUTX_REASON_BLACKLIST_EVICT;
		mutx_scb_list_rem(alt_entry);
		mu_scb = (mutx_scb_t *)alt_entry->data;
		mu_scb->reason = MUTX_REASON_BLACKLIST_ADMIT;
		mutx_scb_list_add_tail(mu_info->admit_list, alt_entry);
	}

#ifdef NOT_YET
	/* final mu client set won't be full */
	while ((blcklst_pos = mutx_blcklst_match(mu_info, mu_info->admit_list, &blcklst_bkt))) {
		head = mu_info->admit_list;
		start = head->next;
		FOREACH_LIST_POS(pos, next, mu_info->admit_list) {
			mutx_scb_list_rem(pos);
			if (!(blcklst_pos = mutx_blcklst_match(mu_info, mu_info->admit_list,
				&blcklst_bkt))) {
				break;
			}

			mutx_scb_list_add_tail(mu_info->admit_list, pos);
			if (start == head->next) {
				mutx_scb_list_rem(start);
				break;
			}
		}
	}
#endif /* NOT_YET */

	/* Check if selected group about to be admitted is also in black listed groups */
	/* It happens when there are no alternate set to choose */
	/* Delete the group from the black list */
	if (blcklst_bkt != NULL)
		mutx_blcklst_del(mu_info, blcklst_bkt);
#ifdef BCMDBG
	WL_MUMIMO(("admit lst after blcklst match\n"));
	FOREACH_LIST_POS(pos, next, mu_info->admit_list) {
		mu_scb = (mutx_scb_t *)pos->data;
		candidate = mu_scb->scb;
		WL_MUMIMO((""MACF"\n", ETHER_TO_MACF(candidate->ea)));
	}
#endif // endif
}

static bool
wlc_mutx_admit_muclients(wlc_mutx_info_t *mu_info)
{
	wlc_info_t *wlc = mu_info->wlc;
	scb_iter_t scbiter;
	scb_t *candidate, *evictee;
	mutx_scb_t *mu_scb_evict, *mu_scb;
	mutx_scb_list_t *pos, *next;
	uint8 fifo_bitmap[TX_FIFO_BITMAP_SIZE_MAX] = { 0 };
	uint8 i, ac, muclients_bmp = 0, evictees_bmp, su_free_slots;
	uint8 new_muclients = 0;
	bool evictees = FALSE;
	bool need_flush = FALSE;
	uint16 max_vhtmu_usrs = wlc_txcfg_max_clients_get(wlc->txcfg, VHTMU);

#ifdef BCMDBG
	WL_MUMIMO(("%s: mu admit set\n", __FUNCTION__));
	FOREACH_LIST_POS(pos, next, mu_info->admit_list) {
		mu_scb = (mutx_scb_t *)pos->data;
		candidate = mu_scb->scb;
		WL_MUMIMO((""MACF"\n", ETHER_TO_MACF(candidate->ea)));
	}
#endif // endif

	/* clients already in mu clients set */
	FOREACH_LIST_POS(pos, next, mu_info->admit_list) {
		mu_scb = (mutx_scb_t *)pos->data;
		candidate = mu_scb->scb;
		if (mu_in_client_set(mu_info, candidate)) {
			muclients_bmp |= (1U << wlc_mutx_sta_client_index(mu_info, candidate));
			mutx_scb_list_rem(pos);
		} else {
			new_muclients++;
		}
	}

	if (new_muclients) {
		/* There are MU STAs which will be MU Clients, we should flush SU fifos. */
		setbit(fifo_bitmap, TX_AC_BK_FIFO);
		setbit(fifo_bitmap, TX_AC_BE_FIFO);
		setbit(fifo_bitmap, TX_AC_VI_FIFO);
		setbit(fifo_bitmap, TX_AC_VO_FIFO);
		need_flush = TRUE;
		WL_MUMIMO(("%s: There are %d new mu clients in admit_list\n",
			__FUNCTION__, new_muclients));
	}

	/* Get tx fifo bitmap from evictees */
	evictees_bmp = ~muclients_bmp & NBITMASK(max_vhtmu_usrs);
	for (i = 0; (evictees_bmp) && (i < max_vhtmu_usrs); i++, evictees_bmp >>= 1) {
		if (evictees_bmp & 1U) {
			if (mu_info->mu_clients[i]) {
				for (ac = 0; ac < AC_COUNT; ac++) {
					ASSERT(!D11REV_IS(mu_info->pub->corerev, 130) ||
						i < MUCLIENT_NUM_8);
					if (D11REV_GE(mu_info->pub->corerev, 128)) {
						setbit(fifo_bitmap, mutx_aqmfifo_70q[i][ac]);
					}
					else {
						setbit(fifo_bitmap, mutx_aqmfifo[i][ac]);
					}
					need_flush = TRUE;
				}
			}
		}
	}

	/* If there are evictess, then do specific fifo flush for them. */
	if (need_flush) {
#ifdef WL_PSMX
		/* set mu_sounding timer to be 0 */
		wlc_txbf_mutimer_update(wlc->txbf, TRUE);

		wlc_mutx_clr_mubf(wlc);
#endif // endif
		/* do txfifo_sync which suspend_mac and suspend/flush aqm_txfifo */
		if (wlc_mutx_fifo_hold_set(wlc, fifo_bitmap) != BCME_OK) {
			WL_ERROR(("%s: wlc_tx_fifo_hold_set fail!\n", __FUNCTION__));
			FOREACH_LIST_POS(pos, next, mu_info->admit_list) {
				mutx_scb_list_rem(pos);
			}
			goto done;
		}
		evictees = TRUE;
	}

	/* evict clients from mu clients set not in admit list */
	evictees_bmp = ~muclients_bmp & NBITMASK(max_vhtmu_usrs);
	for (i = 0; (evictees_bmp) && (i < max_vhtmu_usrs); i++, evictees_bmp >>= 1) {
		if (evictees_bmp & 1U) {
			evictee = mu_info->mu_clients[i];
			if (evictee) {
				mu_scb_evict = MUTX_SCB(mu_info, evictee);
				if (mu_client_set_remove(mu_info, evictee) == BCME_OK) {
					/* update txbf link to SU for evictee */
					mu_scb_evict->mu_link_permit = FALSE;
					wlc_txbf_link_upd(wlc->txbf, evictee);
				}
			}
		}
	}

	/* admit new clients not in current mu clients set */
	FOREACH_LIST_POS(pos, next, mu_info->admit_list) {
		mutx_scb_list_rem(pos);
		mu_scb = (mutx_scb_t *)pos->data;
		candidate = mu_scb->scb;
		ASSERT(!mu_in_client_set(mu_info, candidate));
		mu_scb->mu_link_permit = TRUE;
		wlc_txbf_link_upd(wlc->txbf, candidate);
		mu_sta_mu_enable(mu_info, candidate);
	}

	/* If there is free slot for SU Beamfoming link, try to fill it for evictee. */
	su_free_slots = wlc_txbf_get_free_su_bfr_links(wlc->txbf);
	FOREACHSCB(wlc->scbstate, &scbiter, evictee) {
		if (su_free_slots == 0)
			break;

		if ((wlc_mutx_sta_mucidx_get(mu_info, evictee) == BF_SHM_IDX_INV) &&
			(wlc_txbf_get_bfi_idx(wlc->txbf, evictee) == BF_SHM_IDX_INV)) {
			wlc_txbf_link_upd(wlc->txbf, evictee);
			su_free_slots--;
		}
	}

done:
	if (need_flush) {
		wlc_mutx_fifo_hold_clr(wlc, fifo_bitmap);
#ifdef WL_PSMX
		/* re-enable mu_sounding timer */
		wlc_txbf_mutimer_update(wlc->txbf, FALSE);
#endif // endif
	}

#ifdef BCMDBG
	WL_MUMIMO(("%s: mu client set\n", __FUNCTION__));
	for (i = 0; i < max_vhtmu_usrs; i++) {
		candidate = mu_info->mu_clients[i];
		if (candidate == NULL)
			continue;
		mu_scb = MUTX_SCB(mu_info, candidate);
		WL_MUMIMO((""MACF"\n",  ETHER_TO_MACF(candidate->ea)));
	}
#endif // endif

	return evictees;
}

static void
mu_clients_eviction(wlc_mutx_info_t *mu_info)
{
	wlc_info_t *wlc = mu_info->wlc;
	scb_t *candidate, *evictee;
	mutx_scb_t *mu_scb_evict, *mu_scb;
	mutx_scb_list_t *evict_entry, *cand_entry, *alter_entry, *pos, *next;
	uint8 i;
	int nclients = 0;
	uint16 max_vhtmu_usrs = wlc_txcfg_max_clients_get(wlc->txcfg, VHTMU);

	BCM_REFERENCE(wlc);
	BCM_REFERENCE(candidate);
	for (i = 0; i < max_vhtmu_usrs; i++) {
		evictee = mu_info->mu_clients[i];
		if (evictee) {
			mu_scb_evict = MUTX_SCB(mu_info, evictee);
			ASSERT(mu_scb_evict != NULL);
			ASSERT(mu_scb_evict->item != NULL);
			if (mu_scb_evict->policy_evict) {
				/* This client will be evicted,
				 * we don't need to add it to evictee_list for score comparison.
				 */
				if (mu_scb_evict->policy_evict == MUTX_POLICY_EVICT_BW) {
					mu_scb_evict->reason = MUTX_REASON_BW_POLICY_MISMATCH;
				} else if (mu_scb_evict->policy_evict == MUTX_POLICY_EVICT_AC) {
					mu_scb_evict->reason = MUTX_REASON_AC_POLICY_MISMATCH;
				} else {
					ASSERT(0);
				}
				mu_scb_evict->policy_evict = MUTX_POLICY_EVICT_NONE;
			} else {
				/* Increase the nclients if policy_evict is FALSE. */
				mutx_scb_list_add_tail(mu_info->evictee_list, mu_scb_evict->item);
				nclients++;
			}
		}
	}

	/* Compare max score candidate with min score evictee, and do eviction. */
	while ((cand_entry = mutx_scb_list_max(mu_info->candidate_list)) != NULL) {
		mutx_scb_list_rem(cand_entry);
		mu_scb = (mutx_scb_t *)cand_entry->data;
		candidate = mu_scb->scb;

		/* If current MU client set is not full, add candidate to the MU client set. */
		if (nclients < max_vhtmu_usrs) {
			mutx_scb_list_add_tail(mu_info->admit_list, cand_entry);
			mu_scb->reason = MUTX_REASON_NEW_ADMIT;
			nclients++;
		} else {
			evict_entry = mutx_scb_list_min(mu_info->evictee_list);
			if (evict_entry == NULL) {
				break;
			}

			mutx_scb_list_rem(evict_entry);
			mu_scb_evict = (mutx_scb_t *)evict_entry->data;

			/* If min score evictee is not less than max score candidate,
			 * then we don't need to do eviction.
			 */
			if (mu_scb_evict->score >= mu_scb->score) {
				mutx_scb_list_add_tail(mu_info->admit_list, evict_entry);
				mutx_scb_list_add_tail(mu_info->candidate_list, cand_entry);
				break;
			}

			mutx_scb_list_add_tail(mu_info->admit_list, cand_entry);
			mu_scb->reason = MUTX_REASON_HIGH_TRAFFIC;
			mu_scb_evict->reason = MUTX_REASON_LOW_TRAFFIC;
			evictee = mu_scb_evict->scb;
			if (mu_sta_mu_ready(mu_info, evictee, TRUE)) {
				mutx_scb_list_add_tail(mu_info->candidate_list, evict_entry);
			} else {
				mutx_scb_list_add_tail(mu_info->alternate_list, evict_entry);
			}
			WL_ERROR(("wl%d:%s:Exchange evictee "MACF"(%d) and candidate "MACF"(%d)\n",
				wlc->pub->unit, __FUNCTION__, ETHER_TO_MACF(evictee->ea),
				mu_scb_evict->score, ETHER_TO_MACF(candidate->ea), mu_scb->score));
		}
	}

	/* If mu client set is not full, select one from alternate list. */
	while (nclients < max_vhtmu_usrs) {
		alter_entry = mutx_scb_list_max(mu_info->alternate_list);
		if (alter_entry == NULL) {
			break;
		}
		mutx_scb_list_rem(alter_entry);
		mutx_scb_list_add_tail(mu_info->admit_list, alter_entry);
		nclients++;
	}

	/* add clients to admit list */
	FOREACH_LIST_POS(pos, next, mu_info->evictee_list) {
		mutx_scb_list_rem(pos);
		ASSERT(pos != NULL);
		mutx_scb_list_add_tail(mu_info->admit_list, pos);
	}

	if (mu_info->muclient_pfmon)
		wlc_mutx_blcklst_match(mu_info);

	/* Clear linking list for next scheduler. */
	FOREACH_LIST_POS(pos, next, mu_info->candidate_list) {
		mutx_scb_list_rem(pos);
	}
	FOREACH_LIST_POS(pos, next, mu_info->alternate_list) {
		mutx_scb_list_rem(pos);
	}

	FOREACH_LIST_POS(pos, next, mu_info->evictee_list) {
		mutx_scb_list_rem(pos);
		mu_scb = (mutx_scb_t *)pos->data;
		mu_scb->reason = MUTX_REASON_LOW_TRAFFIC;
	}
}

/* For now, the first 8 MU-capable STAs are designated candidates. They are
 * added to the MU client set when they associate. A more sophisticated
 * MU client selection algorithm could be inserted here.
 */
static int
mu_clients_select(wlc_mutx_info_t *mu_info)
{
	(void)wlc_mutx_client_scheduler(mu_info, FALSE);

	mu_info->muclient_elect_pending = FALSE;
	return BCME_OK;
}

/* Take all actions required when a new MU-capable STA is available.
 * Inputs:
 *   mu_info - MU TX state
 *   scb     - the new MU-capable STA
 * Returns:
 *   BCME_OK if successful
 *   BCME_NOMEM if memory allocation for the scb cubby failed
 */
static int
mu_sta_mu_enable(wlc_mutx_info_t *mu_info, scb_t *scb)
{
	mutx_scb_t *mu_scb = MUTX_SCB(mu_info, scb);

	if (!mu_scb) {
		return BCME_ERROR;
	}

	if (!mu_in_client_set(mu_info, scb)) {
		mu_client_set_new_sta(mu_info, scb);
	}

	return BCME_OK;
}

/* Take actions required when a STA disassociates or is no longer is MU-capable.
 * If the STA is a MU client, remove the STA from the MU client set.
 * If another MU-capable STA is ready, add it to the MU client set.
 */
static int
mu_sta_mu_disable(wlc_mutx_info_t *mu_info, scb_t *scb)
{
	/* Remove scb from candidate set if currently a member */
	if (!mu_in_client_set(mu_info, scb)) {
		return BCME_ERROR;
	}
	/* Removing the MU client and trigger muclient_elect_pending for selecting new set. */
	if (mu_client_set_remove(mu_info, scb) == BCME_OK) {
		mu_info->muclient_elect_pending = TRUE;
	}

	return BCME_OK;
}

/* Report whether a given STA is MU capable.
 * STA must be VHT capable and advertise MU BEAMFORMEE capability.
 * Internal scbs are not considered MU capable.
 * Returns TRUE if STA is MU capable.
 */
static bool
mu_sta_mu_capable(wlc_mutx_info_t *mu_info, scb_t *scb)
{
	wlc_info_t *wlc = mu_info->wlc;
	uint16 vht_flags, link_bw;

	if (SCB_INTERNAL(scb)) {
		return FALSE;
	}

	if (!SCB_VHT_CAP(scb)) {
		/* STA not VHT capable. Can't be MU capable. */
		return FALSE;
	}

	if (SCB_HE_CAP(scb)) {
		/* STA is capable of HE, do not admit it as VHT MU client.
		 * we will not use either VHT SU rate, nor VHT MU rate
		 */
		return FALSE;
	}

	vht_flags = wlc_vht_get_scb_flags(wlc->vhti, scb);
	if ((vht_flags & SCB_MU_BEAMFORMEE) == 0) {
		return FALSE;
	}

	link_bw = wlc_scb_ratesel_get_link_bw(wlc, scb);
	if (link_bw == BW_160MHZ) {
		return FALSE;
	}

	return TRUE;
}

/* Report whether a given STA is ready to be an MU receiver.
 * STA must MU capable.
 * STA must also be associated to a BSS where this device is acting as an AP.
 * Returns TRUE if STA is MU capable.
 */
static bool
mu_sta_mu_ready(wlc_mutx_info_t *mu_info, scb_t *scb, bool strict_check)
{
	uint32 bw_policy;
	uint8 link_bw;
	if (!mu_sta_mu_capable(mu_info, scb)) {
		return FALSE;
	}

	/* Check BW policy */
	bw_policy = mu_info->wlc->mutx_policy->bw_policy;
	link_bw = wlc_scb_ratesel_get_link_bw(mu_info->wlc, scb);
	if ((strict_check || mu_info->mutx_client_samebw) && link_bw != bw_policy) {
		return FALSE;
	}

	/* Check traffic loads */
	if (strict_check && !wlc_mutx_sta_ac_check(mu_info, scb))
		return FALSE;

	if (!SCB_ASSOCIATED(scb) || !BSSCFG_AP(scb->bsscfg)) {
		return FALSE;
	}

	if (SCB_HE_CAP(scb)) {
		/* STA is capable of HE, do not admit it as VHT MU client.
		 * we will not use either VHT SU rate, nor VHT MU rate
		 */
		return FALSE;
	}

	/* Can come here looking for a STA to replace a STA just removed
	 * from the MU client set. If client was removed because its BSS
	 * is going down, avoid selecting another STA in the same BSS.
	 */
	if (!scb->bsscfg->up) {
		return FALSE;
	}

	return TRUE;
}

bool
wlc_mutx_sta_ac_check(wlc_mutx_info_t *mu_info, scb_t *scb)
{
	mutx_scb_t *mu_scb = MUTX_SCB(mu_info, scb);
	uint32 ac;

	if (mu_info->ac_policy_on == FALSE) {
		/* AC policy is disabled, return TRUE anyway. */
		return TRUE;
	}

	if (!mu_scb)
		return FALSE;
	ac = mu_info->wlc->mutx_policy->ac_policy;
	if (ac != 0 && mu_scb->ac_policy_scores[ac] == 0)
		return FALSE;

	return TRUE;
}

bool
wlc_mutx_sta_on_hold(wlc_mutx_info_t *mu_info, scb_t *scb)
{
	mutx_scb_t *mu_scb = MUTX_SCB(mu_info, scb);
	if (mu_scb && mu_scb->on_hold_dur)
		return TRUE;
	return FALSE;
}

bool
wlc_mutx_sta_mu_link_permit(wlc_mutx_info_t *mu_info, scb_t *scb)
{
	mutx_scb_t *mu_scb = MUTX_SCB(mu_info, scb);
	if (mu_scb && mu_scb->mu_link_permit)
		return TRUE;
	return FALSE;
}

/* Return TRUE if a given STA's group membership has changed, but the STA
 * has not yet acknowledged the Group ID Mgmt frame that informed it of
 * the change.
 * Return FALSE if the STA has acknowledged receipt of its current
 * membership status.
 */
static bool
mu_scb_membership_change_get(wlc_info_t *wlc, scb_t *scb)
{
	wlc_mutx_info_t *mu_info = (wlc_mutx_info_t*) wlc->mutx;
	mutx_scb_t *mu_scb = MUTX_SCB(mu_info, scb);

	if (mu_scb)
		return mu_scb->membership_change;
	else
		return FALSE;
}

/* Update the scb state that indicates whether this AP is waiting for the scb
 * to acknowledge receipt of a Group ID Mgmt frame.
 */
static void
mu_scb_membership_change_set(wlc_info_t *wlc, scb_t *scb, bool pending)
{
	wlc_mutx_info_t *mu_info = (wlc_mutx_info_t*) wlc->mutx;
	mutx_scb_t *mu_scb = MUTX_SCB(mu_info, scb);

	if (!mu_scb)
		return;

	mu_scb->membership_change = pending;

	/* is NONE then we came here from clear/remove */
	if (!mu_any_membership(mu_scb->membership)) {
		return;
	}

	if (SCB_VHTMU(scb)) {
		return;
	}

	SCB_VHTMU_ENABLE(scb);

	/* Update the ampdu_mpdu number */
	scb_ampdu_update_config_mu(wlc->ampdu_tx, scb);
	if (WLC_TXC_ENAB(mu_info->wlc)) {
		wlc_txc_inv(wlc->txc, scb);
	}
	if (RATELINKMEM_ENAB(wlc->pub)) {
		wlc_ratelinkmem_update_link_entry(wlc, scb);
	}

#ifdef AP
	wlc_apps_ps_flush(wlc, scb);
#endif // endif
#ifdef WLTAF
	wlc_taf_scb_state_update(wlc->taf_handle, scb, TAF_PARAM(SCB_MU(scb)),
		TAF_SCBSTATE_MU_MIMO);
#endif // endif
}

/* Get the MU client index for a given STA.
 * Returns the client index (0 to 7) if STA is one of the current MU clients.
 * If STA is not in the current MU client set, returns MU_CLIENT_INDEX_NONE.
 * Caller may wish to call wlc_mutx_is_muclient() to verify that STA is a
 * fully-qualified MU client.
 */
uint16
wlc_mutx_sta_client_index(wlc_mutx_info_t *mu_info, scb_t *scb)
{
	mutx_scb_t *mu_scb;

	if (!mu_info->mutx_active)
		return MU_CLIENT_INDEX_NONE;

	if (SCB_INTERNAL(scb)) {
		return MU_CLIENT_INDEX_NONE;
	}

	mu_scb = MUTX_SCB(mu_info, scb);
	if (mu_scb)
		return mu_scb->client_index;
	else
		return MU_CLIENT_INDEX_NONE;
}

#ifdef WL_MUPKTENG
uint8
wlc_mutx_pkteng_on(wlc_mutx_info_t *mu_info)
{
	return mu_info->mutx_pkteng;
}
#endif // endif

static void
wlc_mu_reinit(wlc_info_t *wlc)
{
	uint32 gptime = 0;

	WLCNTINCR(wlc->pub->_cnt->reinit);

	/* cache gptime out count */
	if (wlc->pub->up)
		gptime = wlc_hrt_gptimer_get(wlc);

	wl_init(wlc->wl);

	/* restore gptime out count after init (gptimer is reset due to wlc_reset */
	if (gptime)
		wlc_hrt_gptimer_set(wlc, gptime);
}

static int
wlc_mutx_switch(wlc_info_t *wlc, bool mutx_feature, bool is_iov)
{
	int err = 0;
	wlc_mutx_info_t *mu_info = wlc->mutx;

	if (D11REV_LT(wlc->pub->corerev, 64))
		return BCME_UNSUPPORTED;

	/* If we have a forced setting and the switch request does not come from an IOVAR,
	 * then we just ignore the request.
	 */
	if (!(wlc->pub->mu_features & MU_FEATURES_AUTO) && !is_iov)
		return BCME_EPERM;

	WL_MUMIMO(("wl%d: %s: switching to %s-TX\n",
		wlc->pub->unit, __FUNCTION__, mutx_feature? "MU":"SU"));

	if (mutx_feature) {
		/* Before switch to MU, notify other module. */
		wlc_mutx_state_upd(wlc, MUTX_ON);
		if (WLC_PHY_AS_80P80(wlc, wlc->chanspec)) {
			WL_MUMIMO(("wl%d: %s: chanspec(0x%x) is 160Mhz, can not switch to MU\n",
				wlc->pub->unit, __FUNCTION__, wlc->chanspec));
			return BCME_NOTREADY;
		}
		wlc->pub->mu_features |= WLC_MU_BFR_CAP_PHY(wlc) ? MU_FEATURES_MUTX : 0;
	} else {
		wlc->pub->mu_features &= ~MU_FEATURES_MUTX;
	}

	mu_info->mutx_switch_in_progress = TRUE;

	wlc_sync_txfifo_all(wlc, wlc->active_queue, SYNCFIFO);

#if defined(BCM_DMA_CT) && !defined(BCM_DMA_CT_DISABLED)
	if (D11REV_LT(wlc->pub->corerev, 128)) {
		wlc_bmac_ctdma_update(wlc->hw, mutx_feature);
	}
#endif // endif
	/* Control mutx feature based on Mu txbf capability. */
	wlc_mutx_update(wlc, wlc_txbf_mutx_enabled(wlc->txbf));

	if (wlc->pub->up && wlc->state != WLC_STATE_GOING_DOWN) {
#ifdef WL_PSMX
		wlc_mutx_hostflags_update(wlc);
#endif /* WL_PSMX */
		wlc_mutx_active_update(wlc->mutx);
#ifdef WL_PSMX
		wlc_txbf_mutimer_update(wlc->txbf, FALSE);
#endif /* WL_PSMX */
#ifdef WL_BEAMFORMING
		if (wlc->pub->associated && TXBF_ENAB(wlc->pub)) {
			wlc_bsscfg_t *bsscfg;
			scb_t *scb;
			scb_iter_t scbiter;
			int i;

			wlc_txbf_bfi_init(wlc->txbf);
			FOREACH_BSS(wlc, i, bsscfg) {
				FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
					if ((wlc_mutx_sta_mucidx_get(mu_info, scb)
						!= BF_SHM_IDX_INV) ||
						(wlc_txbf_get_bfi_idx(wlc->txbf, scb)
						!= BF_SHM_IDX_INV &&
						(D11REV_LT(wlc->pub->corerev, 128)))) {
						wlc_txbf_delete_link(wlc->txbf, scb);
					}
				}
			}

			/* wlc_txbf_delete_link deletes all the SU links too,
			 * restore If there is free slot for SU Beamfoming link
			 */
			FOREACHSCB(wlc->scbstate, &scbiter, scb) {
				if ((wlc_mutx_sta_mucidx_get(mu_info, scb) == BF_SHM_IDX_INV) &&
					(wlc_txbf_get_bfi_idx(wlc->txbf, scb) == BF_SHM_IDX_INV)) {
					wlc_txbf_link_upd(wlc->txbf, scb);
				}
			}
		}
#endif /* WL_BEAMFORMING */
		if (D11REV_LT(wlc->pub->corerev, 128)) {
			/* no MU ucode re-download and therefore no reinit required in rev128+ */
			wlc_mu_reinit(wlc);
		}
	}

	mu_info->mutx_switch_in_progress = FALSE;

	/* After switch to SU, notify other module. */
	if (!mutx_feature)
		wlc_mutx_state_upd(wlc, MUTX_OFF);

	return (err);
}

bool
wlc_mutx_switch_in_progress(wlc_info_t *wlc)
{
	wlc_mutx_info_t *mu_info = (wlc_mutx_info_t*) wlc->mutx;

	return mu_info->mutx_switch_in_progress;
}

void BCMFASTPATH
wlc_mutx_sta_txfifo(wlc_mutx_info_t *mu_info, scb_t *scb, uint *pfifo)
{
	mutx_scb_t *mu_scb;
	uint16 mu_features = mu_info->pub->mu_features;

	if (((mu_features & (MU_FEATURES_MUTX)) && !mu_info->mutx_active) ||
		((!(mu_features & MU_FEATURES_MUTX)) &&
		!HE_MMU_ENAB(mu_info->pub))) {
		return;
	}

	mu_scb = MUTX_SCB(mu_info, scb);
	if (!mu_scb || mu_scb->client_index == MU_CLIENT_INDEX_NONE) {
		return;
	}

	ASSERT(*pfifo < AC_COUNT);
	ASSERT(!D11REV_IS(mu_info->pub->corerev, 130) ||
		mu_scb->client_index < MUCLIENT_NUM_8);
	if (D11REV_GE(mu_info->pub->corerev, 128)) {
		*pfifo = mutx_aqmfifo_70q[mu_scb->client_index][*pfifo];
	}
	else {
		*pfifo = mutx_aqmfifo[mu_scb->client_index][*pfifo];
	}
}

bool
wlc_mutx_txfifo_ac_matching(uint fifo, uint ac)
{
	int i;

	ASSERT(ac < AC_COUNT);
	if (ac >= AC_COUNT)
		return FALSE;

	for (i = 0; i < MUCLIENT_NUM; i++) {
		if (mutx_aqmfifo[i][ac] == fifo)
			return TRUE;
	}

	return FALSE;
}

void
wlc_mutx_sta_fifo_bitmap(wlc_mutx_info_t *mu_info, scb_t *scb, void *fifo_bitmap)
{
	int ac;
	mutx_scb_t *mu_scb;
	uint16 mu_features = mu_info->pub->mu_features;

	ASSERT(fifo_bitmap != NULL);

	if (D11REV_LT(mu_info->pub->corerev, 128) && !mu_info->mutx_active) {
		return;
	} else if (((mu_features & MU_FEATURES_MUTX) && !mu_info->mutx_active) ||
		((!(mu_features & MU_FEATURES_MUTX)) && !HE_MMU_ENAB(mu_info->pub))) {
		return;
	}

	mu_scb = MUTX_SCB(mu_info, scb);
	if (!mu_scb || mu_scb->client_index == MU_CLIENT_INDEX_NONE)
		return;

	ASSERT(!D11REV_IS(mu_info->pub->corerev, 130) || mu_scb->client_index < MUCLIENT_NUM_8);

	for (ac = 0; ac < AC_COUNT; ac++) {
		if (D11REV_GE(mu_info->pub->corerev, 128)) {
			setbit(fifo_bitmap, mutx_aqmfifo_70q[mu_scb->client_index][ac]);
		}
		else {
			setbit(fifo_bitmap, mutx_aqmfifo[mu_scb->client_index][ac]);
		}
	}
}

uint8
wlc_mutx_get_muclient_nrx(wlc_mutx_info_t *mu_info)
{
	return mu_info->muclient_nrx;
}

/* Report whether a given STA is one of the current MU clients. For this API,
 * a STA is only considered a fully qualified MU client if this AP has received
 * acknowledgement of the most recent Group ID Mgmt frame sent to the STA.
 */
bool
wlc_mutx_is_muclient(wlc_mutx_info_t *mu_info, scb_t *scb)
{
	if (SCB_INTERNAL(scb)) {
		return FALSE;
	}

	if (mu_scb_membership_change_get(mu_info->wlc, scb))
		return FALSE;

	return (wlc_mutx_sta_client_index(mu_info, scb) != MU_CLIENT_INDEX_NONE);
}

/* Report whether a given STA is in an MU group
 * Inputs:
 *   mu_info  - MU TX state
 *   scb      - STA in question
 *   group_id - Either a specific MU group ID, 1 - 62, or can use 0 as
 *              a wildcard, in which case report whether STA is a member
 *              of any MU group
 * Returns:
 *   TRUE if STA is a group member
 *   FALSE otherwise
 */
bool
wlc_mutx_is_group_member(wlc_mutx_info_t *mu_info, scb_t *scb, uint16 group_id)
{
	mutx_scb_t *mu_scb = MUTX_SCB(mu_info, scb);

	if (mu_scb) {
		if (group_id == MU_GROUP_ID_ANY) {
			return mu_any_membership(mu_scb->membership);
		} else {
			return isset(mu_scb->membership, group_id);
		}
	}
	return FALSE;
}

/* Callback function invoked when a STA's association state changes.
 * Inputs:
 *   ctx -        mutx state structure
 *   notif_data - information describing the state change
 */
static void
wlc_mutx_scb_state_upd(void *ctx, scb_state_upd_data_t *notif_data)
{
	wlc_mutx_info_t *mu_info = (wlc_mutx_info_t*) ctx;
	wlc_info_t *wlc = mu_info->wlc;
	scb_t *scb;
	uint8 oldstate;
	ASSERT(notif_data != NULL);

	scb = notif_data->scb;
	ASSERT(scb != NULL);
	oldstate = notif_data->oldstate;

	wlc_mutx_bw_policy_update(mu_info, FALSE);

	if (D11REV_GE(wlc->pub->corerev, 129) && HE_MMU_ENAB(wlc->pub) &&
		SCB_HE_CAP(scb)) {
		wlc_mutx_he_scb_state_upd(mu_info, notif_data);
		return;
	}

	/* If this device is not MU tx capable, no need to maintain MU client set.
	 * Radio has to go down to enable MUTX; so clients will reassociate
	 * and AP can relearn them then.
	 */
	if (!MU_TX_ENAB(wlc))
		return;

	if ((oldstate & ASSOCIATED) && !SCB_ASSOCIATED(scb)) {
		/* STA has disassociated. If STA is in current MU client set,
		 * remove it.
		 */
		WL_MUMIMO(("wl%d: STA "MACF" has disassociated.\n",
		          wlc->pub->unit, ETHER_TO_MACF(scb->ea)));

		/* Check if the the driver can pick next mu candidate */
		if (mu_in_client_set(mu_info, scb)) {
			mu_info->muclient_elect_pending = TRUE;
		}
		mu_sta_mu_disable(mu_info, scb);
	} else if (mu_sta_mu_ready(mu_info, scb, FALSE)) {
		/* MU capable STA now fully ready */
		WL_MUMIMO(("wl%d: MU capable STA "MACF" has associated.\n",
		          wlc->pub->unit, ETHER_TO_MACF(scb->ea)));
		if (!mu_client_set_full(mu_info))
			mu_info->muclient_elect_pending = TRUE;
	}
}

/**
 * turn on/off HE MU-MIMO for a scb
 *
 * Once the decision is made by admission control to admit/evict a STA,
 * this API is invoked to enable/disable STA as HE-MUMIMO. Checks whether the admit list is full
 * and finds an un-occupied fifo index and assigns to sta
 *
 * @param muched	handle to mutx_info context
 * @param scb		scb pointer
 * @param enable	TRUE, to admit user. FALSE to evict
 * @return		none
 */
void
wlc_mutx_set_hemmu(wlc_mutx_info_t *mu_info, scb_t* scb, bool enable)
{
	mutx_scb_t *mu_scb;
	wlc_info_t *wlc;
	uint16 i;
#ifdef MAC_AUTOTXV_OFF
	int err = 0;
#endif // endif
	bool upd_lmem = TRUE;

	wlc = mu_info->wlc;
	BCM_REFERENCE(wlc);

	if (SCB_INTERNAL(scb)) {
		return;
	}

	if ((mu_scb = MUTX_SCB(mu_info, scb)) == NULL) {
		WL_ERROR(("wl%d: %s: Fail to get mutx scb cubby STA "MACF"\n",
			wlc->pub->unit, __FUNCTION__,
			ETHER_TO_MACF(scb->ea)));
		return;
	}

	if (enable) {
		i = wlc_mutx_sta_mucidx_get(mu_info, scb);
		if (i != BF_SHM_IDX_INV) {
			ASSERT(scb == mu_info->mu_clients[i]);
			if (SCB_HEMMU(scb)) {
				upd_lmem = FALSE;
			}
		} else if ((i = wlc_mutx_sta_mucidx_set(mu_info, scb)) == BF_SHM_IDX_INV) {
			WL_MUTX(("wl%d: %s: HE MU-MIMO STA "MACF" not admitted as admit"
				" list full\n", wlc->pub->unit,
				__FUNCTION__, ETHER_TO_MACF(scb->ea)));
			return;
		}

		SCB_HEMMU_ENABLE(scb);
		mu_info->mu_clients[i] = scb;
		mu_scb->client_index = i;
		wlc_txbf_tbcap_update(wlc->txbf, scb);
#ifdef MAC_AUTOTXV_OFF
		err = wlc_macreq_upd_bfi(wlc, scb, i, TRUE);
		if (err != BCME_OK) {
			WL_ERROR(("wl%d: MAC command failed to update bfi"
				"for STA "MACF" i:%u. err:%d\n",
				mu_info->pub->unit, ETHER_TO_MACF(scb->ea), i, err));
			return;
		}
#endif // endif
	} else {
#ifdef MAC_AUTOTXV_OFF
		err = wlc_macreq_upd_bfi(wlc, scb, mu_scb->client_index, FALSE);
		if (err != BCME_OK) {
			WL_ERROR(("wl%d: MAC command failed to update bfi"
			"for STA "MACF" i:%u\n",
			mu_info->pub->unit, ETHER_TO_MACF(scb->ea), mu_scb->client_index));
		}
#endif // endif
		if (!SCB_HEMMU(scb)) {
			upd_lmem = FALSE;
		}

		mu_info->mu_clients[mu_scb->client_index] = NULL;
		mu_scb->client_index = MU_CLIENT_INDEX_NONE;
		SCB_HEMMU_DISABLE(scb);
		/* set tbcap in bfi config */
		wlc_txbf_tbcap_update(wlc->txbf, scb);
	}

	WL_MUTX(("wl%d: %s: %s HE MU-MIMO STA "MACF" upd:%d\n", wlc->pub->unit,
		__FUNCTION__, enable ? "Enable" : "Disable", ETHER_TO_MACF(scb->ea), upd_lmem));

	if (!upd_lmem) {
		return;
	}

	/* Update the ampdu_mpdu count */
	scb_ampdu_update_config_mu(wlc->ampdu_tx, scb);
	if (WLC_TXC_ENAB(wlc)) {
		wlc_txc_inv(wlc->txc, scb);
	}
	if (RATELINKMEM_ENAB(wlc->pub)) {
		wlc_ratelinkmem_update_link_entry(wlc, scb);
	}
}

/**
 *  * Check STA's capability to be admitted as HEMMU
 *
 * @param mu_info	handle to mu_info context
 * @param scb		scb pointer
 * @return		TRUE, if eligible. FALSE, otherwise.
 */
static bool
wlc_mutx_scb_ishemmu_eligible(wlc_mutx_info_t *mu_info, scb_t* scb)
{
	bool ret = FALSE;
	mutx_scb_t *mu_scb = MUTX_SCB(mu_info, scb);
	wlc_info_t *wlc = mu_info->wlc;

	if (mu_scb && SCB_HE_CAP(scb) && HE_MMU_ENAB(wlc->pub) &&
		BSSCFG_AP(SCB_BSSCFG(scb)) &&
		wlc_txbf_is_hemmu_enab(wlc->txbf, scb) &&
		wlc_he_get_ulmu_allow(wlc->hei, scb) &&
		!wlc_twt_scb_active(wlc->twti, scb)) {
		ret = TRUE;
	}
	return ret;
}

/* To check if the he-mmu is operational for a given BW, This is to hold allowing
 * the first user within a given BW as DLOFDMA, before admitting as HEMMU
 * Returns
 * true: 1) If there are atleast two HEMMU users in given bw or
 *       2) no of eligible (may not be admitted) hemmu users reached max
 *       3) If not 1) or 2), See if there are already min # of dlofdma users
 * false: otherwise
 */
bool
wlc_mutx_is_hemmu_enab(wlc_mutx_info_t *mu_info, uint8 bw)
{
	uint8 num_hemmu_usrs = 0, num_usrs_allbw = 0, link_bw, max_hemmu_usrs;
	scb_t *scb;
	scb_iter_t scbiter;
	wlc_info_t *wlc;
	uint16 num_dlofdma_usrs = 0;

	if (!mu_info) {
		return TRUE;
	}

	wlc = mu_info->wlc;
	max_hemmu_usrs = wlc_txcfg_max_clients_get(wlc->txcfg, HEMMU);

	if (!max_hemmu_usrs) {
		return TRUE;
	}

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		bool mmu_on;

		mmu_on = wlc_mutx_scb_ishemmu_eligible(mu_info, scb);
		link_bw = wlc_scb_ratesel_get_link_bw(wlc, scb);

		if (mmu_on) {
			num_usrs_allbw++;
		}

		if (SCB_DLOFDMA(scb)) {
			continue;
		}

		if (bw != link_bw) {
			continue;
		}

		if (!mmu_on) {
			if (wlc_musched_scb_isdlofdma_eligible(wlc->musched, scb)) {
				num_dlofdma_usrs++;
			}

			continue;
		}

		num_hemmu_usrs++;
		if ((num_hemmu_usrs >= mu_info->min_hemmu_usrs) ||
			num_usrs_allbw == max_hemmu_usrs) {
			return TRUE;
		}
	}

	if (num_dlofdma_usrs >= 2)
		return TRUE;

	return FALSE;

}

/* Go through each supported BW and evaluate if the eligibility criteria is met for all
 * associated users per BW
 * Called from admission control logic triggered by STA's assoc/disassoc (wlc_mutx_he_scb_state_upd)
 * If there is any sta's omibw params got changed (wlc_mutx_hemmu_omibw_upd)
 */
static void
wlc_mutx_hemmu_min_usrs_eval(wlc_mutx_info_t *mu_info, scb_t *scb_in, bool assocd,
	int8 from_bw, int8 to_bw)
{
	wlc_info_t *wlc = mu_info->wlc;
	scb_t *scb;
	scb_iter_t scbiter;
	uint8 bw, link_bw, bw0;

	/* loop through all bw's and see if we have min of 2 hemmu users per bw */
	for (bw = BW_20MHZ; bw <= BW_MAXMHZ; bw++) {
		bw0 = (bw - 1);
		mu_info->num_hemmu_usrs[bw0] = 0;

		/* skip radio's unsupported bandwidth */
		if (!((wlc->band->bw_cap) & (1<<(bw0)))) {
			continue;
		}

		FOREACHSCB(wlc->scbstate, &scbiter, scb) {
			bool mmu_on;
			mutx_scb_t *mu_scb = MUTX_SCB(mu_info, scb);

			mmu_on = wlc_mutx_scb_ishemmu_eligible(mu_info, scb);
			link_bw = wlc_scb_ratesel_get_link_bw(wlc, scb);

			if (!mmu_on) {
				continue;
			}
			/* skip ineligible or already admitted as dlofdma */
			if (SCB_DLOFDMA(scb)) {
				continue;
			}

			if (scb_in && scb_in == scb) {
				if (from_bw != AUTO) {
					ASSERT(from_bw == link_bw);

					if (bw == from_bw) {
						mu_info->num_hemmu_usrs[bw0]--;
					} else if (bw == to_bw) {
						mu_info->num_hemmu_usrs[bw0]++;
					}
					continue;
				} else if (!assocd) {
					continue;
				}
			}

			if (bw != link_bw) {
				continue;
			}

			mu_scb->rssi = wlc_lq_rssi_get(wlc, SCB_BSSCFG(scb), scb);
			if (mu_scb->rssi < mu_info->hemu_rssi_thresh) {
				/* skip if sta rssi > threshold  */
				WL_MUTX(("wl%d: %s: skip hemu STA "MACF" rssi(%d) > thresh(%d)\n",
					wlc->pub->unit, __FUNCTION__, ETHER_TO_MACF(scb->ea),
					mu_scb->rssi, mu_info->hemu_rssi_thresh));
				continue;
			}

			mu_info->num_hemmu_usrs[bw0]++;

			if (from_bw == AUTO &&
				mu_info->num_hemmu_usrs[bw0] >= mu_info->min_hemmu_usrs) {
				break;
			}
		}
	}
}

/* Go through list of HE MMU capable users and see if we need to re-decide on evicting any
 * already admitted hemmu user
 */
void
wlc_mutx_he_eval_and_admit_clients(wlc_mutx_info_t *mu_info, scb_t *scb_in, bool assocd)
{
	if (!mu_info) {
		return;
	}

	wlc_mutx_hemmu_min_usrs_eval(mu_info, scb_in, assocd, AUTO, AUTO);

	wlc_mutx_he_admit_clients(mu_info);
}

/* Admit/evict a user based on admission criteria (#users > min_users) */
static void
wlc_mutx_he_admit_clients(wlc_mutx_info_t *mu_info)
{
	wlc_info_t *wlc = mu_info->wlc;
	uint8 bw, link_bw, bw0;
	scb_iter_t scbiter;
	scb_t *scb;
	bool onoff;

	for (bw = BW_20MHZ; bw <= BW_MAXMHZ; bw++) {
		bw0 = (bw - 1);
		/* skip radio's unsupported bandwidth */
		if (!((wlc->band->bw_cap) & (1 << bw0))) {
			continue;
		}

		onoff = (mu_info->num_hemmu_usrs[bw0] >= mu_info->min_hemmu_usrs) ? TRUE : FALSE;
		FOREACHSCB(wlc->scbstate, &scbiter, scb) {
			bool mmu_on, scb_onoff;
			mutx_scb_t *mu_scb = MUTX_SCB(mu_info, scb);

			if (mu_scb == NULL) {
				continue;
			}
			mmu_on = wlc_mutx_scb_ishemmu_eligible(mu_info, scb);
			link_bw = wlc_scb_ratesel_get_link_bw(wlc, scb);

			scb_onoff = (onoff && mu_scb->assocd);
			mu_scb->rssi = wlc_lq_rssi_get(wlc, SCB_BSSCFG(scb), scb);
			if (!mmu_on || (scb_onoff == SCB_HEMMU(scb)) ||
			    (bw != link_bw)) {
				/* skip ineligible or already set SCB or bw mismatch */
				continue;
			}

			if (scb_onoff && (mu_scb->rssi < mu_info->hemu_rssi_thresh)) {
				/* skip if sta rssi > threshold  */
				WL_MUTX(("wl%d: %s: skip hemu STA "MACF" rssi(%d) > thresh(%d)\n",
					wlc->pub->unit, __FUNCTION__, ETHER_TO_MACF(scb->ea),
					mu_scb->rssi, mu_info->hemu_rssi_thresh));
				continue;
			}

			/* switch from hemmu to dlofdma is already handled in ucode */
			if (mmu_on) {
				/* disable/enable dlmu explicitly */
				wlc_mutx_set_hemmu(mu_info, scb, scb_onoff);
			} else {
				if (scb_onoff) {
					WL_MUTX(("wl%d: %s: Fail to enable HEMMU STA "MACF" cap"
						" %x dlmmu %x\n", wlc->pub->unit, __FUNCTION__,
						ETHER_TO_MACF(scb->ea), SCB_HE_CAP(scb),
						HE_MMU_ENAB(wlc->pub)));
				}
				wlc_mutx_set_hemmu(mu_info, scb, FALSE);
			}
		}
	}
}

/*  if the bw bucket that the sta is in has to disable hemmu, we need to know what other STA
 *  that has it disabled so we end up with same logic as wlc_mutx_he_admit_clients
 *  should be independent of the sta already admitted i.e, SCB_HEMMU == true or not
 */
void
wlc_mutx_hemmu_omibw_upd(wlc_mutx_info_t *mu_info, scb_t *omi_scb, int8 from_bw, int8 to_bw)
{
	/* skip if there is no bandwidth change */
	if (from_bw != AUTO && from_bw == to_bw) {
		return;
	}

	if (!mu_info) {
		return;
	}

	/* Evaluate tot # of users per BW, considering the STA's BW change */
	wlc_mutx_hemmu_min_usrs_eval(mu_info, omi_scb, TRUE, from_bw, to_bw);

	/* Admit or evict the user */
	wlc_mutx_he_admit_clients(mu_info);
}

static void
wlc_mutx_he_scb_state_upd(wlc_mutx_info_t *mu_info, scb_state_upd_data_t *notif_data)
{
	scb_t *scb;
	uint8 oldstate;
	wlc_bsscfg_t *bsscfg;
	mutx_scb_t *mu_scb;

	scb = notif_data->scb;
	oldstate = notif_data->oldstate;
	bsscfg = scb->bsscfg;
	mu_scb = MUTX_SCB(mu_info, scb);

	if (!BSSCFG_AP(bsscfg) ||
#ifndef WL11AX
		/* force to return if WL11AX compilation flag is off */
		TRUE ||
#endif /* WL11AX */
		SCB_INTERNAL(scb) || !mu_scb || SCB_DLOFDMA(scb)) {
		return;
	}

	/* only HE MMU capable SCBs come here. */
	if ((!WSEC_ENABLED(bsscfg->wsec) && !(oldstate & ASSOCIATED) &&
		SCB_AUTHENTICATED(scb) && SCB_ASSOCIATED(scb)) ||
		(WSEC_ENABLED(bsscfg->wsec) && SCB_AUTHENTICATED(scb) &&
		SCB_ASSOCIATED(scb) && SCB_AUTHORIZED(scb) && !(oldstate & AUTHORIZED))) {
		/* open security: not assoc -> assoc
		 * security: assoc -> authorized
		 */

		/* pass */
		mu_scb->assocd = TRUE;
	} else if ((oldstate & ASSOCIATED) && !SCB_ASSOCIATED(scb)) {
		/* pass */
		mu_scb->assocd = FALSE;
	} else {
		return;
	}

	/* Current policy to enable he mu-mimo: for AP: he features = 0x1f;
	 * for STA: 1) HE cap; 2) Associated; 3) not admitted as dl-ofdma
	 * 4) tot HE STAs perbw > 1 5) ulmu enable
	 */
	wlc_mutx_he_eval_and_admit_clients(mu_info, scb, mu_scb->assocd);
}

/* Respond to a change in VHT capabilities for a given client. The client's
 * new capabilities must already be stored in the driver's internal data
 * structures when this function is called. If STA is capable of
 * acting as an MU-MIMO receiver, consider adding the STA to the set of
 * MU clients.
 *
 * This function is only called when MU-MIMO transmit is enabled on this device.
 * A call to this function is triggered by the following events:
 *
 *   - VHT is globally disabled on this device
 *   - This device is an AP that just received a frame which
 *     includes an updated set of VHT capabilities for the client
 *
 * Inputs:
 *      mu_info - MU-MIMO state
 *      scb     - STA whose VHT capabilities have just been received
 */
void
wlc_mutx_update_vht_cap(wlc_mutx_info_t *mu_info, scb_t *scb)
{
	mutx_scb_t *mu_scb;

	/* MUTX cubby is not reserved/allocated if phy is not MU capable */
	if (!WLC_MU_BFR_CAP_PHY(mu_info->wlc)) {
		return;
	}
	mu_scb = MUTX_SCB(mu_info, scb);
	if (mu_scb) {
		mu_scb->nrx = VHT_MCS_SS_SUPPORTED(4, scb->rateset.vht_mcsmap) ? 4 :
			(VHT_MCS_SS_SUPPORTED(3, scb->rateset.vht_mcsmap) ? 3 :
			(VHT_MCS_SS_SUPPORTED(2, scb->rateset.vht_mcsmap) ? 2 : 1));
	}

	/* If MU-MIMO transmit disabled locally, no need to process STA. */
	if (!MU_TX_ENAB(mu_info->wlc)) {
		return;
	}

	WL_MUMIMO(("wl%d: MU-MIMO module notified of VHT capabilities update for STA "MACF".\n",
	         mu_info->pub->unit, ETHER_TO_MACF(scb->ea)));

	if (mu_sta_mu_ready(mu_info, scb, FALSE)) {
		mu_info->muclient_elect_pending = TRUE;
	}
}

/* Enable or disable Mu Tx feature */
void
wlc_mutx_update(wlc_info_t *wlc, bool enable)
{
	bool previous = wlc->_mu_tx;

	/* Mu tx feature depends on CutThru DMA support, but mu_features would override
	 * the CTDMA setting.
	 */
	if ((wlc->pub->mu_features & MU_FEATURES_MUTX) != 0) {
		wlc->_mu_tx = enable;
	} else {
		/* Force disable mutx feature. */
		wlc->_mu_tx = FALSE;
	}

	if (wlc->_mu_tx != previous) {
		/* The number of active TX FIFOs may have changed */
		wlc_hw_update_nfifo(wlc->hw);
	}
}

#ifdef WL_PSMX
void
wlc_mutx_hostflags_update(wlc_info_t *wlc)
{
	uint16 mhf_mask, mhf_val = 0, mxhf_mask, mxhf_val = 0;

	mxhf_mask = (MXHF0_CHKFID | MXHF0_DYNSND | MXHF0_MUAGGOPT | MXHF0_MUTXUFWAR);
	mhf_mask = MHF2_PRELD_GE64;

	if (D11REV_GE(wlc->pub->corerev, 128)) {
		return;
	}

	if (MU_TX_ENAB(wlc)) {
		/* When MUTX is enabled, which implies CTDMA needs to be on,
		 * we will enable preloading for 4365c0.
		 */
		if (D11REV_IS(wlc->pub->corerev, 65)) {
			mhf_val = MHF2_PRELD_GE64;
		}
		mxhf_val = (MXHF0_CHKFID | MXHF0_DYNSND | MXHF0_MUAGGOPT | MXHF0_MUTXUFWAR);
	} else {
		/* When MUTX is disabled, preloading is enabled if CTDMA is on.
		 */
		if (wlc->_dma_ct_flags & DMA_CT_PRECONFIGURED) {
			if (D11REV_IS(wlc->pub->corerev, 65)) {
				mhf_val = MHF2_PRELD_GE64;
			}
		}
	}
	wlc_mhf(wlc, MHF2, mhf_mask, mhf_val, WLC_BAND_ALL);

	wlc_mhf(wlc, MXHF0, mxhf_mask, mxhf_val, WLC_BAND_ALL);
}
#endif /* WL_PSMX */

/* Evaluate whether current configuration and system state allow MU TX to
 * be active. If there is a change in status, take required actions.
 * MU TX can be active if:
 *  - the phy supports MU BFR
 *  - MU BFR is enabled by configuration
 *  - System state preconditions:
 *    -- The number of transmit chains meets a minimum requirement
 *    -- XXX What about 3x3 + 1x1? Do we need to add a call when entering/exiting this mode?
 */
void
wlc_mutx_active_update(wlc_mutx_info_t *mu_info)
{
	wlc_info_t *wlc = mu_info->wlc;
	wlc_vasip_info_t *vasip = wlc->vasip;
	bool mutx_active = TRUE;
	bool mutx_was_active = mu_info->mutx_active;
	int txchains;

	/* This checks both phy capability and current configuration */
	if (!MU_TX_ENAB(wlc)) {
		WL_MUMIMO(("wl%d: MU TX not active. MU BFR not enabled.\n",
			wlc->pub->unit));
		mutx_active = FALSE;
	}

	/* Check number of transmit streams */
	txchains = WLC_BITSCNT(wlc->stf->txchain);
	if (txchains != vasip->mu_supported_Ntx) {
		WL_MUMIMO(("wl%d: MU TX not active. "
			   "Number of tx chains, %d, not equal to VASIP FW supported Ntx, %u.\n",
			   wlc->pub->unit, txchains, vasip->mu_supported_Ntx));
		mutx_active = FALSE;
	}

	mu_info->mutx_active = mutx_active;

	if (!mutx_was_active && mutx_active) {
		/* Bring MU TX up. Schedule MU client election. Don't do election
		 * immediately in case there are other configuration or state changes
		 * that follow immediately after this one. Give system time to settle.
		 */
		mu_info->muclient_elect_pending = TRUE;
	} else if (mutx_was_active && !mutx_active) {
		/* Bring MU TX down */
		/* Clear MU client set */
		mu_client_set_clear(mu_info);
	}
}

/* Clear MU group ID membership for a client with a given client index. */
void
wlc_mutx_membership_clear(wlc_mutx_info_t *mu_info, scb_t *scb)
{
	mutx_scb_t *mu_scb;

	mu_scb = MUTX_SCB(mu_info, scb);
	if (mu_scb) {
		memset(mu_scb->membership, 0, MU_MEMBERSHIP_SIZE);
		memset(mu_scb->position, 0, MU_POSITION_SIZE);
	}

	/* Tell STA it no longer has any MU group membership */
	mu_group_id_msg_send(mu_info->wlc, scb, NULL, NULL);
}

/* Get the current membership and user positions for the STA at a given client index.
 * Could be used to get current state, modify membership, and call wlc_mutx_membership_set()
 * with modified state.
 *
 * Inputs:
 *   mu_info -    MU TX state
 *   client_index - Index of a STA in the MU client set.
 *   membership - (out) Group membership bit mask. 1 indicates client is a member of the
 *                corresponding group. Lowest order bit indicates membership
 *                status in group ID 0.
 *   position -   (out) User position array for this STA. Two bits per group. Same order
 *                as membership array (user pos for group 0 first).
 * Returns:
 *   BCME_OK if membership and position arrays are set
 *   BCME_RANGE if client_index is out of range
 *   BCME_ERROR if scb does not have a mutx cubby
 *   BCME_BADARG if membership or position array is NULL, or if there is no MU
 *               client at the given client index
 */
int
wlc_mutx_membership_get(wlc_mutx_info_t *mu_info, uint16 client_index,
	uint8 *membership, uint8 *position)
{
	mutx_scb_t *mu_scb;
	uint8 max_vht_usrs = wlc_txcfg_max_clients_get(mu_info->wlc->txcfg, VHTMU);

	if (client_index >= max_vht_usrs) {
		WL_ERROR(("wl%d: %s: MU client index %u out of range.\n",
			mu_info->pub->unit, __FUNCTION__, client_index));
		return BCME_RANGE;
	}

	if (mu_info->mu_clients[client_index] == NULL) {
		return BCME_BADARG;
	}

	mu_scb = MUTX_SCB(mu_info, mu_info->mu_clients[client_index]);
	if (mu_scb == NULL) {
		return BCME_ERROR;
	}

	if (!membership || !position) {
		return BCME_BADARG;
	}

	memcpy(membership, mu_scb->membership, MU_MEMBERSHIP_SIZE);
	memcpy(position, mu_scb->position, MU_POSITION_SIZE);

	return BCME_OK;
}

/* Set the MU group membership and user position for all MU groups for a STA with
 * a given client index.
 *
 * Inputs:
 *   mu_info - MU TX state
 *   client_index - Index of a STA in the MU client set.
 *   membership - Group membership bit mask. 1 indicates client is a member of the
 *                corresponding group. Lowest order bit indicates membership
 *                status in group ID 0.
 *   position - User position array for this STA. Two bits per group. Same order
 *              as membership array (user pos for group 0 first).
 *
 * Returns:
 *   BCME_OK if membership set
 *   BCME_RANGE if client_index is out of range
 *   BCME_BADARG if there is no MU client at the given client index
 *   BCME_ERROR if another error occurs
 */
int
wlc_mutx_membership_set(wlc_mutx_info_t *mu_info, uint16 client_index,
	uint8 *membership, uint8 *position)
{
	mutx_scb_t *mu_scb;
	uint8 max_vht_usrs = wlc_txcfg_max_clients_get(mu_info->wlc->txcfg, VHTMU);

	if (client_index >= max_vht_usrs) {
		WL_ERROR(("wl%d: %s: MU client index %u out of range.\n",
			mu_info->pub->unit, __FUNCTION__, client_index));
		return BCME_RANGE;
	}

	if (mu_info->mu_clients[client_index] == NULL) {
		return BCME_BADARG;
	}

	if (!membership || !position) {
		return BCME_BADARG;
	}

	mu_scb = MUTX_SCB(mu_info, mu_info->mu_clients[client_index]);
	if (mu_scb == NULL) {
		mu_scb = mu_scb_cubby_alloc(mu_info, mu_info->mu_clients[client_index]);
	}
	if (mu_scb == NULL) {
		return BCME_ERROR;
	}

	/* Test if this client's group membership has changed */
	if (memcmp(mu_scb->membership, membership, MU_MEMBERSHIP_SIZE) ||
	    memcmp(mu_scb->position, position, MU_POSITION_SIZE)) {
		mu_scb->membership_change = TRUE;
	}

	memcpy(mu_scb->membership, membership, MU_MEMBERSHIP_SIZE);
	memcpy(mu_scb->position, position, MU_POSITION_SIZE);

	if (mu_scb->membership_change) {
		/* Tell STA new group membership and user position data */
		mu_scb->gid_send_cnt = 0;
		mu_scb->gid_retry_war = GID_RETRY_DELAY;
		if (mu_group_id_msg_send(mu_info->wlc, mu_info->mu_clients[client_index],
		                         mu_scb->membership, mu_scb->position) == BCME_OK) {
			mu_scb->gid_send_cnt++;
		}
	}

	return BCME_OK;
}

/* Build and queue a Group ID Management frame to a given STA.
 * Inputs:
 *      wlc - radio
 *      scb - STA to receive the GID frame
 *      membership - membership array to put in the message. If NULL, send a GID message
 *                   indicating that the STA is not a member of any MU group
 *      user_pos   - user position array to put in the message. May be NULL if membership
 *                   is NULL.
 * Returns BCME_OK     if message sent and send status callback registered
 *         BCME_BADARG if called with an invalid argument
 *         BCME_NOMEM  if memory allocation failure
 *         BCME_ERROR  if send status callback registration failed
 */
static int
mu_group_id_msg_send(wlc_info_t *wlc, scb_t *scb, uint8 *membership, uint8 *user_pos)
{
	void *pkt;
	uint8 *pbody;
	uint body_len;             /* length in bytes of the body of the group ID frame */
	struct dot11_action_group_id *gid_msg;
	wlc_bsscfg_t *bsscfg;      /* BSS config for BSS to which STA is associated */
	struct ether_addr *da;     /* RA for group ID frame */
	int ret;
	mutx_scb_t *mu_scb;
	wlc_mutx_info_t *mu_info = (wlc_mutx_info_t*) wlc->mutx;

	mu_scb = MUTX_SCB(mu_info, scb);
	ASSERT(mu_scb != NULL);
	if (!mu_scb) {
		return BCME_ERROR;
	}

	if (mu_scb->retry_gid == MUTX_R_GID_DISABLED) {
		return BCME_ERROR;
	}

	if (!user_pos && membership) {
		WL_ERROR(("wl%d: User position must be specified if membership is specified.\n",
		          wlc->pub->unit));
		/* Will retx at next tick */
		mu_scb->retry_gid = MUTX_R_GID_RETRY_CNT;
		return BCME_BADARG;
	}

	/* Allocate frame */
	WL_MUMIMO(("wl%d: Sending GID Mgmt frame to STA "MACF".\n",
		wlc->pub->unit, ETHER_TO_MACF(scb->ea)));

	bsscfg = scb->bsscfg;
	da = &scb->ea;
	body_len = sizeof(struct dot11_action_group_id);
	pkt = wlc_frame_get_action(wlc, da, &bsscfg->cur_etheraddr, &bsscfg->BSSID,
	                         body_len, &pbody, DOT11_ACTION_CAT_VHT);
	if (pkt == NULL) {
		WL_ERROR(("wl%d: No memory to allocate Group ID frame to "MACF".\n",
			wlc->pub->unit, ETHERP_TO_MACF(da)));
		mu_scb->retry_gid = MUTX_R_GID_NO_BUFFER;
		return BCME_NOMEM;
	}

	PKTSETPRIO(pkt, PRIO_8021D_VO);
	/* Write body of frame */
	gid_msg = (struct dot11_action_group_id*) pbody;
	gid_msg->category = DOT11_ACTION_CAT_VHT;
	gid_msg->action = DOT11_VHT_ACTION_GID_MGMT;

	if (membership) {
		memcpy(gid_msg->membership_status, membership, DOT11_ACTION_GID_MEMBERSHIP_LEN);
		memcpy(gid_msg->user_position, user_pos, DOT11_ACTION_GID_USER_POS_LEN);
	} else {
		memset(gid_msg->membership_status, 0, DOT11_ACTION_GID_MEMBERSHIP_LEN);
		memset(gid_msg->user_position, 0, DOT11_ACTION_GID_USER_POS_LEN);
	}

	/* Register callback to be invoked when tx status is processed for this frame */
	ret = wlc_pcb_fn_register(wlc->pcb, wlc_mutx_gid_complete, (void*)mu_scb, pkt);
	if (ret != 0) {
		/* We won't get status for tx of this frame. So we will retransmit,
		 * even if it gets ACK'd. Should be a rare case, and retx isn't harmful.
		 */
		WL_MUMIMO(("wl%d: Failed to register for status of Group ID Mgmt "
			"frame to STA "MACF".\n", wlc->pub->unit, ETHER_TO_MACF(scb->ea)));
		mu_scb->retry_gid = MUTX_R_GID_CALLBACK_FAIL;
		PKTFREE(wlc->osh, pkt, TRUE);
		return BCME_ERROR;
	}

	/* Call wlc_sendctl() instead of wlc_sendmgmt() to use "enq_only" */
	ret = wlc_sendctl(wlc, pkt, bsscfg->wlcif->qi, scb, WLC_LOWEST_SCB_RSPEC(scb), TRUE);
	if (!ret) {
		/* Cancel any pending wlc_mutx_gid_complete packet callbacks. */
		wlc_pcb_fn_find(wlc->pcb, wlc_mutx_gid_complete, (void*) mu_scb, TRUE);
		mu_scb->retry_gid = MUTX_R_GID_NO_BUFFER;
		return BCME_ERROR;
	}

	mu_scb->retry_gid = MUTX_R_GID_DISABLED;
	return BCME_OK;
}

/* Callback invoked when tx status is processed for a group ID mgmt frame.
 * If the STA ack'd the frame, then we know it is prepared to receive
 * MU-PPDUs from this AP. If not ACK'd, we will retransmit.
 */
static void
wlc_mutx_gid_complete(wlc_info_t *wlc, uint txstatus, void *arg)
{
	mutx_scb_t *mu_scb = (mutx_scb_t *) arg;
	scb_t *scb;

	if (!mu_scb) {
		WL_ERROR(("wl%d: %s: mu_scb is NULL !!!\n", wlc->pub->unit, __FUNCTION__));
		return;
	}

	if ((scb = mu_scb->scb) == NULL) {
		WL_ERROR(("wl%d: %s: No SCB was found.\n", wlc->pub->unit, __FUNCTION__));
		return;
	}

	if (ETHER_ISMULTI(&scb->ea)) {
		WL_ERROR(("wl%d: Group ID message complete callback with mcast "MACF".\n",
		          wlc->pub->unit, ETHER_TO_MACF(scb->ea)));
		return;
	}

	if (txstatus & TX_STATUS_ACK_RCV) {
		/* STA is now prepared to receive MU-PPDUs */
		mu_scb_membership_change_set(wlc, scb, FALSE);
		WL_MUMIMO(("wl%d: STA "MACF" acknowledged MU-MIMO Group ID message.\n",
		        wlc->pub->unit, ETHER_TO_MACF(scb->ea)));
		/* May retx at GID_RETRY_DELAY tick */
		mu_scb->retry_gid = MUTX_R_GID_RETRY_WAR;
	} else {
		WL_MUMIMO(("wl%d: STA "MACF" failed to acknowledge MU-MIMO Group ID message.\n",
		        wlc->pub->unit, ETHER_TO_MACF(scb->ea)));
		/* Will retx at next tick */
		mu_scb->retry_gid = MUTX_R_GID_RETRY_CNT;
	}
}

static void
wlc_mutx_state_upd(wlc_info_t *wlc, uint8 state)
{
	mutx_state_upd_data_t data;
	data.state = state;

	bcm_notif_signal(wlc->mutx->mutx_state_notif_hdl, &data);
}

int
wlc_mutx_state_upd_register(wlc_info_t *wlc, bcm_notif_client_callback fn, void *arg)
{
	wlc_mutx_info_t *mu_info = (wlc_mutx_info_t *)wlc->mutx;
	bcm_notif_h hdl = mu_info->mutx_state_notif_hdl;

	if (mu_info->mutx_switch_in_progress)
		return BCME_BUSY;

	if (hdl == NULL) {
		WL_ERROR(("wl%d: %s: mutx->mutx_state_notif_hdl is NULL\n",
				WLCWLUNIT(wlc), __FUNCTION__));
		return BCME_ERROR;
	}
	return bcm_notif_add_interest(hdl, fn, arg);
}

int
wlc_mutx_state_upd_unregister(wlc_info_t *wlc, bcm_notif_client_callback fn, void *arg)
{
	wlc_mutx_info_t *mu_info = (wlc_mutx_info_t *)wlc->mutx;
	bcm_notif_h hdl = mu_info->mutx_state_notif_hdl;

	if (mu_info->mutx_switch_in_progress)
		return BCME_OK;

	if (hdl == NULL) {
		WL_ERROR(("wl%d: %s: mutx->mutx_state_notif_hdl is NULL\n",
				WLCWLUNIT(wlc), __FUNCTION__));
		return BCME_ERROR;
	}
	return bcm_notif_remove_interest(hdl, fn, arg);
}

#ifdef BCMDBG
/* unit test code */

#define MU_UT_STA_NUM 14
static int
mu_ut_client_set(wlc_mutx_info_t *mu_info)
{
	wlc_info_t *wlc = mu_info->wlc;
	struct ether_addr ea = {{ 0x00, 0x21, 0x02, 0x03, 0x04, 0x00 }};
	scb_t *scb_list[MU_UT_STA_NUM];
	mutx_scb_t *mu_scb;
	int err = BCME_OK;
	uint8 i;

	/* Create some scbs to play with */
	for (i = 0; i < MU_UT_STA_NUM; i++) {
		ea.octet[5] = i;
		scb_list[i] = wlc_scblookup(wlc, wlc->cfg, &ea);
		mu_scb = mu_scb_cubby_alloc(mu_info, scb_list[i]);
		if (mu_scb == NULL) {
			WL_ERROR(("\nCS. Failed to allocate scb cubby at index %d", i));
			err = BCME_NOMEM;
		}
	}

	/* Verify client index initialized to none */
	if (wlc_mutx_sta_client_index(mu_info, scb_list[0]) != MU_CLIENT_INDEX_NONE) {
		WL_ERROR(("\nCS0. Client index not initialized to NONE."));
		err = BCME_ERROR;
	}

	/* Add STA to MU client list */
	if (mu_client_set_add(mu_info, scb_list[0]) != BCME_OK) {
		WL_ERROR(("\nCS1. Failed to add STA 0 to MU client set"));
		err = BCME_ERROR;
	}

	/* Verify MU client added */
	if (!mu_in_client_set(mu_info, scb_list[0])) {
		WL_ERROR(("\nCS2. Failed to find STA in MU client set"));
		err = BCME_ERROR;
	}

	/* Test external API. Need to ensure no change pending. */
	if (mu_scb_membership_change_get(mu_info->wlc, scb_list[0])) {
		mu_scb_membership_change_set(mu_info->wlc, scb_list[0], FALSE);
	}
	if (!wlc_mutx_is_muclient(mu_info, scb_list[0])) {
		WL_ERROR(("\nCS2.5. STA not reported as MU client."));
		err = BCME_ERROR;
	}

	/* Cand set shouldn't be full yet */
	if (mu_client_set_full(mu_info)) {
		WL_ERROR(("\nCS3. Candidate set reported full after adding just 1 STA."));
		err = BCME_ERROR;
	}

	/* Client index should be set */
	if (wlc_mutx_sta_client_index(mu_info, scb_list[0]) == MU_CLIENT_INDEX_NONE) {
		WL_ERROR(("\nCS4. Client index of MU client set to NONE."));
		err = BCME_ERROR;
	}

	/* Remove MU client */
	if (mu_client_set_remove(mu_info, scb_list[0]) != BCME_OK) {
		WL_ERROR(("\nCS5. Failed to remove STA from MU client set."));
		err = BCME_ERROR;
	}

	/* Verify MU client removed */
	if (mu_in_client_set(mu_info, scb_list[0])) {
		WL_ERROR(("\nCS6. Failed to remove STA from MU client set"));
		err = BCME_ERROR;
	}

	/* Client index should be NONE again */
	if (wlc_mutx_sta_client_index(mu_info, scb_list[0]) != MU_CLIENT_INDEX_NONE) {
		WL_ERROR(("\nCS7. Client index of STA removed from MU client set not NONE."));
		err = BCME_ERROR;
	}

	/* Add a bunch of MU clients */
	for (i = 0; i < MUCLIENT_NUM; i++) {
		if (mu_client_set_add(mu_info, scb_list[i]) != BCME_OK) {
			WL_ERROR(("\nCS8. Failed to add STA %d to MU client set", i));
			err = BCME_ERROR;
		}
	}

	/* Next add should fail. Candidate set is full. */
	if (mu_client_set_add(mu_info, scb_list[MUCLIENT_NUM]) != BCME_NORESOURCE) {
		WL_ERROR(("\nCS9. Added MU client to set that should have been full."));
		err = BCME_ERROR;
	}

	/* Cand set should be full */
	if (!mu_client_set_full(mu_info)) {
		WL_ERROR(("\nCS10. Candidate set not reported full."));
		err = BCME_ERROR;
	}

	for (i = 0; i < MUCLIENT_NUM; i++) {
		if (!mu_in_client_set(mu_info, scb_list[i])) {
			WL_ERROR(("\nCS11. Failed to find STA %d MU client set", i));
			err = BCME_ERROR;
		}
	}

	/* Remove MU client from middle */
	if (mu_client_set_remove(mu_info, scb_list[5]) != BCME_OK) {
		WL_ERROR(("\nCS12. Failed to remove MU client."));
		err = BCME_ERROR;
	}

	/* Cand set should not be full */
	if (mu_client_set_full(mu_info)) {
		WL_ERROR(("\nCS13. Candidate set reported full after remove."));
		err = BCME_ERROR;
	}

	/* Should be able to add one */
	if (mu_client_set_add(mu_info, scb_list[MUCLIENT_NUM]) != BCME_OK) {
		WL_ERROR(("\nCS14. Failed to fill space in MU client set."));
		err = BCME_ERROR;
	}

	if (!mu_in_client_set(mu_info, scb_list[MUCLIENT_NUM])) {
		WL_ERROR(("\nCS15. Failed to find STA %d MU client set", i));
		err = BCME_ERROR;
	}

	/* Clear MU client set */
	mu_client_set_clear(mu_info);

	/* No STA should be in MU client set */
	for (i = 0; i < MU_UT_STA_NUM; i++) {
		if (mu_in_client_set(mu_info, scb_list[i])) {
			WL_ERROR(("\nCS16. STA %d in MU client set after clear", i));
			err = BCME_ERROR;
		}
	}

	/* Clean up */
	for (i = 0; i < MU_UT_STA_NUM; i++) {
		wlc_scbfree(wlc, scb_list[i]);
	}
	return err;
}

/* Unit test of user pos utilities */
static int
mu_ut_user_pos(wlc_mutx_info_t *mu_info)
{
	wlc_info_t *wlc = mu_info->wlc;
	struct ether_addr ea = {{ 0x00, 0x11, 0x02, 0x03, 0x04, 0x00 }};
	scb_t *scb;
	mutx_scb_t *mu_scb;
	uint16 group_id = 5;
	uint8 user_pos;
	uint8 user_pos_ret;
	int err = BCME_OK;

	/* Add 1 MU-capable STA */
	ea.octet[5] = 1;
	scb = wlc_scblookup(wlc, wlc->cfg, &ea);
	if (mu_sta_mu_enable(mu_info, scb) != BCME_OK) {
		WL_MUMIMO(("\nUP-1. Failed to enable MU on STA"));
	}

	mu_scb = MUTX_SCB(mu_info, scb);

	for (group_id = MU_GROUP_ID_MIN; group_id <= MU_GROUP_ID_MAX; group_id++) {
		for (user_pos = 0; user_pos <= 3; user_pos++) {
			/* Verify we get what we set */
			memset(mu_scb->position, 0, MU_POSITION_SIZE);
			mu_user_pos_set(mu_scb->position, group_id, user_pos);
			user_pos_ret = mu_user_pos_get(mu_scb->position, group_id);
			if (user_pos_ret != user_pos) {
			WL_ERROR(("\nUP2. Wrote user pos %u. Got %u.", user_pos, user_pos_ret));
				err = BCME_ERROR;
			}
		}
	}

	wlc_scbfree(wlc, scb);

	return err;
}

static int
mu_ut_l2(wlc_mutx_info_t *mu_info)
{
	wlc_info_t *wlc = mu_info->wlc;
	struct ether_addr ea = {{ 0x00, 0x31, 0x02, 0x03, 0x04, 0x00 }};
	scb_t *scb_list[MU_UT_STA_NUM];
	int err = BCME_OK;
	uint8 i;
	vht_cap_ie_t vht_cap;
	scb_t *scb;

	memset(&vht_cap, 0, sizeof(vht_cap));
	vht_cap.vht_cap_info |= (VHT_CAP_INFO_SU_BEAMFMEE | VHT_CAP_INFO_MU_BEAMFMEE);

	/* Create some scbs. Fill MU client list. */
	for (i = 0; i < MU_UT_STA_NUM; i++) {
		ea.octet[5] = i;
		scb = scb_list[i] = wlc_scblookup(wlc, wlc->cfg, &ea);

		scb->flags2 |= SCB2_VHTCAP;
		wlc_vht_update_scb_state(wlc->vhti, wlc->band->bandtype,
			scb, 0, &vht_cap, 0, 0);
		wlc_scb_setstatebit(wlc, scb, AUTHENTICATED);
		wlc_scb_setstatebit(wlc, scb, ASSOCIATED);

		if (mu_sta_mu_enable(mu_info, scb_list[i]) != BCME_OK) {
			WL_ERROR(("\nL2-1. Failed to enable MU on STA %d", i));
		}
	}

	/* First N STAs should be on MU client list */
	for (i = 0; i < MUCLIENT_NUM; i++) {
		if (!mu_in_client_set(mu_info, scb_list[i])) {
			WL_ERROR(("\nL2-2. Failed to find STA %d in MU client set", i));
			err = BCME_ERROR;
		}
	}

	/* Disable MU on one STA */
	if (mu_sta_mu_disable(mu_info, scb_list[3]) != BCME_OK) {
		WL_ERROR(("\nL2-3. Failed to disable MU on a STA."));
		err = BCME_ERROR;
	}

	/* STA should have been removed from MU client set */
	if (mu_in_client_set(mu_info, scb_list[3])) {
		WL_ERROR(("\nL2-5. Failed to remove STA 3 from MU client set"));
		err = BCME_ERROR;
	}

	/* Another STA should have been added to MU client set */
	for (i = MUCLIENT_NUM; i < MU_UT_STA_NUM; i++) {
		if (mu_in_client_set(mu_info, scb_list[i])) {
			break;
		}
	}
	if (i == MU_UT_STA_NUM) {
		WL_ERROR(("\nL2-6. Failed to add STA to fill MU client set"));
		err = BCME_ERROR;
	}

	/* Disable MU on all STAs to empty STA list and MU client set */
	for (i = 0; i < MU_UT_STA_NUM; i++) {
		wlc_scbfree(wlc, scb_list[i]);
	}
	return err;
}

/* Test membership get and set routines */
static int
mu_ut_membership(wlc_mutx_info_t *mu_info)
{
	wlc_info_t *wlc = mu_info->wlc;
	struct ether_addr ea = {{ 0x00, 0x31, 0x02, 0x03, 0x04, 0x00 }};
	scb_t *scb;
	int err = BCME_OK;
	vht_cap_ie_t vht_cap;
	uint8 m[MU_MEMBERSHIP_SIZE];
	uint8 ups[MU_POSITION_SIZE];
	const uint16 group_id = 60;    /* STA will be added to this group */
	const uint16 not_in_gid = 40;  /* STA not in this group */
	const uint8 up1 = 2;            /* STA added to group_id at this up */
	const uint8 up2 = 1;           /* and then changed to this up */
	const uint8 client_index = 0;    /* Only STA at this index */
	int rc;

	memset(&vht_cap, 0, sizeof(vht_cap));
	vht_cap.vht_cap_info |= (VHT_CAP_INFO_SU_BEAMFMEE | VHT_CAP_INFO_MU_BEAMFMEE);

	/* Create one scb. Add to MU client list. */
	scb = wlc_scblookup(wlc, wlc->cfg, &ea);

	scb->flags2 |= SCB2_VHTCAP;
	wlc_vht_update_scb_state(wlc->vhti, wlc->band->bandtype, scb, 0, &vht_cap, 0, 0);
	wlc_scb_setstatebit(wlc, scb, AUTHENTICATED);
	wlc_scb_setstatebit(wlc, scb, ASSOCIATED);

	if (mu_sta_mu_enable(mu_info, scb) != BCME_OK) {
		WL_ERROR(("\nM-1. Failed to enable MU on STA"));
	}

	/* STA should be on MU client list */
	if (!mu_in_client_set(mu_info, scb)) {
		WL_ERROR(("\nM-2. Failed to find STA in MU client set"));
		err = BCME_ERROR;
	}

	/* Get current membership */
	if (wlc_mutx_membership_get(mu_info, 0, m, ups) != BCME_OK) {
		WL_ERROR(("\nM-2. Failed to get current membership"));
		err = BCME_ERROR;
	}

	/* Add membership in group 60 at user postion 2 */
	setbit(m, group_id);
	mu_user_pos_set(ups, group_id, up1);
	if (wlc_mutx_membership_set(mu_info, client_index, m, ups) != BCME_OK) {
		WL_ERROR(("\nM-3. Failed to set new membership in group %u", group_id));
		err = BCME_ERROR;
	}

	/* Verify membership set correctly */
	if (!wlc_mutx_is_group_member(mu_info, scb, MU_GROUP_ID_ANY)) {
		WL_ERROR(("\nM-5. STA should be a group member"));
		err = BCME_ERROR;
	}

	if (!wlc_mutx_is_group_member(mu_info, scb, group_id)) {
		WL_ERROR(("\nM-6. STA should be a member of group %u", group_id));
		err = BCME_ERROR;
	}

	if (wlc_mutx_is_group_member(mu_info, scb, not_in_gid)) {
		WL_ERROR(("\nM-7. STA should not be a member of group %u", not_in_gid));
		err = BCME_ERROR;
	}

	/* Now change just the user position */
	if (wlc_mutx_membership_get(mu_info, client_index, m, ups) != BCME_OK) {
		WL_ERROR(("\nM-8. Failed to get current membership"));
		err = BCME_ERROR;
	}
	mu_user_pos_set(ups, group_id, up2);
	if (wlc_mutx_membership_set(mu_info, client_index, m, ups) != BCME_OK) {
		WL_ERROR(("\nM-9. Failed to set new user position %u in group %u", up2, group_id));
		err = BCME_ERROR;
	}
	if (wlc_mutx_membership_get(mu_info, client_index, m, ups) != BCME_OK) {
		WL_ERROR(("\nM-10. Failed to get current membership"));
		err = BCME_ERROR;
	}
	if (mu_user_pos_get(ups, group_id) != up2) {
		WL_ERROR(("\nM-11. Got up %u. Expected %u", mu_user_pos_get(ups, group_id), up2));
		err = BCME_ERROR;
	}

	/* Remove STA from group */
	clrbit(m, group_id);
	mu_user_pos_set(ups, group_id, 0);
	if (wlc_mutx_membership_set(mu_info, client_index, m, ups) != BCME_OK) {
		WL_ERROR(("\nM-12. Failed to set new membership in group %u", group_id));
		err = BCME_ERROR;
	}

	/* Verify membership set correctly */
	if (wlc_mutx_is_group_member(mu_info, scb, group_id)) {
		WL_ERROR(("\nM-13. STA not removed from MU group %u.", group_id));
		err = BCME_ERROR;
	}

	/* Negative test */
	rc = wlc_mutx_membership_set(mu_info, 1, m, ups);
	if (rc != BCME_BADARG) {
		WL_ERROR(("\nM-14. Unexpected return code %d", rc));
		err = BCME_ERROR;
	}
	rc = wlc_mutx_membership_set(mu_info, client_index, NULL, ups);
	if (rc != BCME_BADARG) {
		WL_ERROR(("\nM-15. Unexpected return code %d", rc));
		err = BCME_ERROR;
	}
	rc = wlc_mutx_membership_set(mu_info, client_index, m, NULL);
	if (rc != BCME_BADARG) {
		WL_ERROR(("\nM-16. Unexpected return code %d", rc));
		err = BCME_ERROR;
	}
	rc = wlc_mutx_membership_set(mu_info, MUCLIENT_NUM, m, ups);
	if (rc != BCME_RANGE) {
		WL_ERROR(("\nM-17. Unexpected return code %d", rc));
		err = BCME_ERROR;
	}

	/* Clear all membership */
	wlc_mutx_membership_clear(mu_info, scb);
	if (wlc_mutx_is_group_member(mu_info, scb, MU_GROUP_ID_ANY)) {
		WL_ERROR(("\nM-18. STA still an MU group member"));
		err = BCME_ERROR;
	}

	wlc_scbfree(wlc, scb);
	return err;
}

/* MU-MIMO transmit unit test */
static void
mutx_ut(wlc_mutx_info_t *mu_info, int32 val)
{
	wlc_info_t *wlc = mu_info->wlc;
	bool success = TRUE;
	const int NUM_MU_STA = 2;
	uint8 i;
	scb_t *scb;
	struct ether_addr ea = {{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x00 }};
	vht_cap_ie_t vht_cap;

	BCM_REFERENCE(wlc);

	WL_MUMIMO(("\nRunning MU-MIMO unit test..."));

	if (!MU_TX_ENAB(wlc)) {
		WL_MUMIMO(("\nAP not MUTX capable"));
		return;
	}

	if (val == 1) {
		/* Run automated unit test */

		/* Test MU client set routines */
		if (mu_ut_client_set(mu_info) == BCME_OK) {
			WL_MUMIMO(("\nCandidate set code passed"));
		} else {
			WL_MUMIMO(("\nCandidate set code failed"));
			success = FALSE;
		}

		/* Test user position utilities */
		if (mu_ut_user_pos(mu_info) == BCME_OK) {
			WL_MUMIMO(("\nUser pos utilities passed unit test"));
		} else {
			WL_MUMIMO(("\nUser pos utilities failed unit test"));
			success = FALSE;
		}

		/* Test next level up */
		if (mu_ut_l2(mu_info) == BCME_OK) {
			WL_MUMIMO(("\nNext level APIs passed unit test"));
		} else {
			WL_MUMIMO(("\nNext level APIs failed unit test"));
			success = FALSE;
		}

		/* Test membership get and set */
		if (mu_ut_membership(mu_info) == BCME_OK) {
			WL_MUMIMO(("\nMembership get and set passed unit test"));
		} else {
			WL_MUMIMO(("\nMembership get and set failed unit test"));
			success = FALSE;
		}

		if (success)
			WL_MUMIMO(("\nMU TX unit test passed\n"));
		else
			WL_MUMIMO(("\nMU TX unit test failed\n"));
	} else if (val == 2) {
		/* Create a couple MU STAs. Configure VHT cap for STAs */
		memset(&vht_cap, 0, sizeof(vht_cap));
		vht_cap.vht_cap_info |= (VHT_CAP_INFO_SU_BEAMFMEE | VHT_CAP_INFO_MU_BEAMFMEE);

		/* Associate a few MU-capable STAs. */
		for (i = 0; i < NUM_MU_STA; i++) {
			ea.octet[5] = i;
			scb = wlc_scblookup(wlc, wlc->cfg, &ea);
			WL_MUMIMO(("\nCreated scb with ea "MACF"\n", ETHER_TO_MACF(scb->ea)));
			/* Make STA VHT capable */
			scb->flags2 |= SCB2_VHTCAP;
			wlc_vht_update_scb_state(wlc->vhti, wlc->band->bandtype,
			                         scb, 0, &vht_cap, 0, 0);
			wlc_scb_setstatebit(wlc, scb, AUTHENTICATED);
			wlc_scb_setstatebit(wlc, scb, ASSOCIATED);
		}
	} else {
		WL_MUMIMO(("\nUnsupported option\n"));
	}
}
#endif  /* BCMDBG */

#ifdef WL_MUPKTENG
struct mupkteng_clinfo
{
	int idx;
	int aid;
	int fifo;
	scb_t *scb;

} mupkteng_clinfo[] = {
	{0, 1, 8, NULL},
	{1, 2, 11, NULL},
	{2, 3, 14, NULL},
	{3, 4, 17, NULL},
	{4, 5, 20, NULL},
	{5, 6, 23, NULL},
	{6, 7, 26, NULL},
	{7, 8, 29, NULL}
	};

static int
wlc_mupkteng_addsta(wlc_mutx_info_t *mu_info, struct ether_addr *ea, int client_idx, int nrxchain)
{
	scb_t *scb;
	vht_cap_ie_t vht_cap;
	wlc_info_t *wlc =  mu_info->wlc;
	int err = BCME_OK;
	if (!wlc_mutx_pkteng_on(mu_info)) {
		WL_ERROR(("%s: mutx_pkteng is not set\n", __FUNCTION__));
		return BCME_ERROR;
	}

	WL_MUMIMO(("%s ea "MACF" idx %d nrxchain %d\n", __FUNCTION__, ETHER_TO_MACF(*ea),
		client_idx, nrxchain));

	wlc = mu_info->wlc;

	if ((client_idx < 0) || (client_idx > 7))
		return BCME_ERROR;

	if (ETHER_ISNULLADDR(ea) || ETHER_ISMULTI(ea))
		 return BCME_ERROR;

	if (nrxchain == 0)
		 return BCME_ERROR;

	if (mupkteng_clinfo[client_idx].scb != NULL) {
		WL_ERROR(("%s client ea "MACF" already exists @idx %d\n", __FUNCTION__,
		ETHER_TO_MACF(mupkteng_clinfo[client_idx].scb->ea), client_idx));
		return BCME_ERROR;
	}

	scb = wlc_scblookup(wlc, wlc->cfg, ea);
	if (scb == NULL) {
		WL_ERROR(("Cannot create scb for ea "MACF"\n", ETHER_TO_MACF(*ea)));
		return BCME_ERROR;
	}
	/* Make STA VHT capable */
	scb->flags2 |= SCB2_VHTCAP;
	wlc_vht_update_scb_state(wlc->vhti, wlc->band->bandtype,
	                         scb, 0, &vht_cap, 0, 0);
	wlc_scb_setstatebit(wlc, scb, AUTHENTICATED);
	wlc_scb_setstatebit(wlc, scb, ASSOCIATED);
	SCB_VHTMU_ENABLE(scb);
	scb->aid = (uint16)mupkteng_clinfo[client_idx].aid;
	err = wlc_txbf_mupkteng_addsta(wlc->txbf, scb, (uint8)client_idx, (uint8)nrxchain);

	if (err != BCME_OK) {
		wlc_scbfree(wlc, scb);
		return BCME_ERROR;
	}

	mupkteng_clinfo[client_idx].scb = scb;
	return BCME_OK;
}

static int
wlc_mupkteng_clrsta(wlc_mutx_info_t *mu_info, int client_idx)
{
	wlc_info_t *wlc =  mu_info->wlc;
	scb_t *scb;

	scb = mupkteng_clinfo[client_idx].scb;
	if (scb == NULL) {
		return BCME_OK;
	}
	wlc_txbf_mupkteng_clrsta(wlc->txbf, scb);
	wlc_scbfree(wlc, scb);
	mupkteng_clinfo[client_idx].scb = NULL;
	return BCME_OK;
}

static int
wlc_mupkteng_tx(wlc_mutx_info_t *mu_info, mupkteng_tx_t *mupkteng_tx)
{
	struct ether_addr *sa;
	scb_t *scb;
	void *pkt = NULL;
	int fifo_idx = -1;
	wlc_info_t *wlc = mu_info->wlc;
	ratespec_t rate_override;
	int i, j, nframes, nclients, flen, ntx;
	wl_pkteng_t pkteng;
	uint16 seq;
	int err = BCME_OK;

	sa = &wlc->pub->cur_etheraddr;

	nclients = mupkteng_tx->nclients;
	pkteng.flags = WL_MUPKTENG_PER_TX_START;
	pkteng.delay = 60;
	pkteng.nframes = ntx = mupkteng_tx->ntx;

	WL_MUMIMO(("%s sa "MACF" total clients %d ntx %d\n", __FUNCTION__,
		ETHER_TO_MACF(wlc->pub->cur_etheraddr), nclients, ntx));

	if ((nclients == 0) || (nclients > 8)) {
		return BCME_ERROR;
	}

	for (i = 0; i < nclients; i++) {
		scb = mupkteng_clinfo[mupkteng_tx->client[i].idx].scb;
		if (scb == NULL) {
			WL_ERROR(("%s scb is NULL @client_idx %d\n", __FUNCTION__, i));
			return BCME_ERROR;
		}
		WL_MUMIMO(("client "MACF": idx %d rspec %d  nframes %d flen %d \n",
			ETHER_TO_MACF(scb->ea), mupkteng_tx->client[i].idx,
			mupkteng_tx->client[i].rspec, mupkteng_tx->client[i].nframes,
			mupkteng_tx->client[i].flen));
	}
	rate_override = wlc->band->rspec_override;

	err = wlc_bmac_pkteng(wlc->hw, &pkteng, NULL, 0);
	if ((err != BCME_OK)) {
		/* restore Mute State after pkteng is done */
		if (wlc_quiet_chanspec(wlc->cmi, wlc->chanspec))
			wlc_mute(wlc, ON, 0);
		return BCME_ERROR;
	}

	/* Unmute the channel for pkteng if quiet */
	if (wlc_quiet_chanspec(wlc->cmi, wlc->chanspec))
		wlc_mute(wlc, OFF, 0);
#ifdef WL_PSMX
	if (PSMX_ENAB(wlc->pub))
		wlc_bmac_suspend_macx_and_wait(wlc->hw);
#endif // endif
	nclients = mupkteng_tx->nclients;
	for (i = 0; i < nclients; i++) {
		fifo_idx =  mupkteng_clinfo[mupkteng_tx->client[i].idx].fifo;
		nframes = mupkteng_tx->client[i].nframes;
		flen =  mupkteng_tx->client[i].flen;
		scb = mupkteng_clinfo[mupkteng_tx->client[i].idx].scb;

		if (scb == NULL) {
			WL_ERROR(("%s: scb @client idx %d is NULL\n", __FUNCTION__, i));
			pkteng.flags = WL_MUPKTENG_PER_TX_STOP;
			wlc_bmac_pkteng(wlc->hw, &pkteng, NULL, 0);
			/* restore Mute State after pkteng is done */
			if (wlc_quiet_chanspec(wlc->cmi, wlc->chanspec))
				wlc_mute(wlc, ON, 0);
			return BCME_ERROR;
		}
		for (j = 0; j < nframes; j++) {
			seq = j + 1;
			pkt = wlc_mutx_testframe(wlc, scb, sa, rate_override, fifo_idx,
				flen, seq);
			if (pkt == NULL) {
				WL_ERROR(("%s: failed to allocate mutx testframe\n", __FUNCTION__));
				pkteng.flags = WL_MUPKTENG_PER_TX_STOP;
				wlc_bmac_pkteng(wlc->hw, &pkteng, NULL, 0);
				/* restore Mute State after pkteng is done */
				if (wlc_quiet_chanspec(wlc->cmi, wlc->chanspec))
					wlc_mute(wlc, ON, 0);
				return BCME_ERROR;
			}
			wlc_bmac_txfifo(wlc->hw, fifo_idx, pkt, TRUE, INVALIDFID, 1);
		}
		WL_INFORM(("%s: client "MACF" idx %d queued nframes %d flen %d fifo_idx %d\n",
			__FUNCTION__, ETHER_TO_MACF(scb->ea), i, nframes, flen, fifo_idx));
	}

	OSL_DELAY(5000);
#ifdef WL_PSMX
	if (PSMX_ENAB(wlc->pub))
		wlc_bmac_enable_macx(wlc->hw);
#endif // endif
	return BCME_OK;
}
#endif /* WL_MUPKTENG */

#ifdef WLCNT
static void
wlc_mu_snr_calib_reset(mutx_scb_t *mutx_scb, int nss)
{
	int i;
	mutx_snr_calib_stats_t *mutx_snr_calib_stats = mutx_scb->mutx_snr_calib_stats;
	for (i = 0; i < MUTX_MCS_NUM; i++) {
		mutx_snr_calib_stats->txmu_succ[i + nss * MUTX_MCS_NUM] = 0;
		mutx_snr_calib_stats->txmu[i + nss * MUTX_MCS_NUM] = 0;
	}
}

static void
wlc_mu_snr_calib_scb(wlc_info_t *wlc, mutx_scb_t *mutx_scb)
{
	mutx_snr_calib_stats_t *mutx_snr_calib_stats = mutx_scb->mutx_snr_calib_stats;
	uint32 unacked, txmu_total, txmu_succ_total, txmu_count;
	int per, i, primary_mcs, phyreg_data, nss, index;
	bool interrupt;
	int per_threshold_low[10] =    {15, 15, 10, 10, 10, 5,  5,  5,  5,  5};
	int per_threshold_middle[10] = {30, 30, 25, 20, 25, 20, 10, 10, 15, 10};
	int per_threshold_high[10] =   {50, 50, 40, 40, 40, 40, 35, 25, 25, 25};
	int n_mpdu_thres_settle, n_mpdu_thres_low, n_mpdu_thres_high;

	for (nss = 0; nss < 2; nss++) {
		interrupt = 0;
		txmu_total = 0;
		txmu_succ_total = 0;
		txmu_count = 0;
		primary_mcs = 0;

		for (i = 0; i < MUTX_MCS_NUM; i++) {
			txmu_total += mutx_snr_calib_stats->txmu[i + nss * MUTX_MCS_NUM];
			txmu_succ_total += mutx_snr_calib_stats->txmu_succ[i + nss * MUTX_MCS_NUM];

			if (txmu_count < mutx_snr_calib_stats->txmu[i + nss * MUTX_MCS_NUM]) {
				txmu_count = mutx_snr_calib_stats->txmu[i + nss * MUTX_MCS_NUM];
				primary_mcs = i;
			}
		}

		n_mpdu_thres_settle = MPDU_THRES_SETTLE * (primary_mcs + 1);
		n_mpdu_thres_low = MPDU_THRES_LOW +  (MPDU_THRES_LOW/10) * (primary_mcs + 1);
		n_mpdu_thres_high = MPDU_THRES_HIGH +  (MPDU_THRES_HIGH/10) * (primary_mcs + 1);

		if (mutx_snr_calib_stats->change[nss] == 1) {
			if (txmu_total > n_mpdu_thres_settle) {
				mutx_snr_calib_stats->change[nss] = 0;
				mutx_snr_calib_stats->txmu_total[nss] += txmu_total;
				wlc_mu_snr_calib_reset(mutx_scb, nss);
			}
			continue;
		}

		txmu_succ_total = (txmu_succ_total > txmu_total) ? txmu_total : txmu_succ_total;
		unacked = txmu_total - txmu_succ_total;
		per = (txmu_total == 0) ? 0 : (unacked * 100) / txmu_total;
		index = mutx_snr_calib_stats->index[nss];

		if ((per > per_threshold_middle[primary_mcs]) &&
				(txmu_total > n_mpdu_thres_low) && (primary_mcs > MCS_MIN)) {
			if (per > per_threshold_high[primary_mcs]) {
				mutx_snr_calib_stats->count[nss][ADJ_IDX(index)] ++;
				mutx_snr_calib_stats->count[nss][ADJ_IDX(index)] =
					(mutx_snr_calib_stats->count[nss][ADJ_IDX(index)] > 8)
					? 8 : mutx_snr_calib_stats->count[nss][ADJ_IDX(index)];
				mutx_snr_calib_stats->txmu_total[nss] = 0;
			}
			index --;
			interrupt = 1;
		}

		if ((per < per_threshold_middle[primary_mcs]) &&
				(txmu_total > n_mpdu_thres_high)) {
			mutx_snr_calib_stats->count[nss][ADJ_IDX(index)] = 0;
			if ((per < per_threshold_low[primary_mcs]) &&
				(mutx_snr_calib_stats->txmu_total[nss] > (n_mpdu_thres_high *
				(mutx_snr_calib_stats->count[nss][ADJ_IDX(index) + 1] + 1)))) {
				if ((primary_mcs < MCS_MAX) || (index < 3)) {
					index ++;
					interrupt = 1;
				}
			}
		}

		if (interrupt == 1) {
			W_REG(wlc->osh, D11_PHY_REG_ADDR(wlc), PHYREG_M2V_MSG0);
			/*
			 * bit15:      interrupt bit
			 * bit[14:11]: SNR adjustment index, range [0:15] is equal to [-8:7] dB.
			 * bit[10]:    nss index (0 or 1)
			 * bit[9:6]:   client index;
			 * bit[5:0]:   interrupt type;
			 */
			index  = (index < -8) ? -8 : index;
			index  = (index > 7) ? 7 : index;

			phyreg_data =  0x8000 | ((ADJ_IDX(index)) << 11) | (nss << 10) |
				(mutx_scb->client_index << 6) | 5;
			W_REG(wlc->osh, D11_PHY_REG_DATA(wlc), phyreg_data);

			mutx_snr_calib_stats->index[nss] = index;
			mutx_snr_calib_stats->change[nss] = 1;
			continue;
		}

		if ((txmu_total > (2*MPDU_THRES_HIGH)) || (interrupt == 1)) {
			wlc_mu_snr_calib_reset(mutx_scb, nss);
			mutx_snr_calib_stats->txmu_total[nss] += txmu_total;
			mutx_snr_calib_stats->txmu_total[nss] =
				(mutx_snr_calib_stats->txmu_total[nss] > (MPDU_THRES_HIGH * 128))
				? (MPDU_THRES_HIGH * 128) : mutx_snr_calib_stats->txmu_total[nss];
		}
	}
	return;
}

static void BCMFASTPATH
wlc_mutx_mutxs_update(wlc_mutx_info_t *mu_info, scb_t *scb, mutx_stats_t *mutx_stats,
	mutx_pfmon_stats_t *mutx_pfmon_stats, mutx_snr_calib_stats_t *mutx_snr_calib_stats,
	tx_status_t *txs)
{
	wlc_info_t *wlc = mu_info->wlc;
	mutx_scb_t *mu_scb;
	uint8 gbmp = TX_STATUS64_MU_GBMP(txs->status.s4);
	uint8 gpos = TX_STATUS64_MU_GPOS(txs->status.s4);
	uint8 mutxcnt = TX_STATUS64_MU_TXCNT(txs->status.s4);
	uint8 mupktnum = TX_STATUS40_TXCNT_RT0(txs->status.s3);
	uint8 muacknum = TX_STATUS40_ACKCNT_RT0(txs->status.s3);
	uint8 gid = TX_STATUS64_MU_GID(txs->status.s3);
	ratespec_t rspec = TX_STATUS64_MU_RSPEC(txs->status.s4);
	uint8 rate_index, nss, mcs, gnum = 0;
	bool is_sgi = TX_STATUS64_MU_SGI(txs->status.s3);
	bool gid_invalid = ((gid >= VHT_SIGA1_GID_MAX_GID) ||
		((gid == 0) && !SCB_HE_CAP(scb) /* ideally should check VHT MU frame instead */));
	uint8 txs_mutype = TX_STATUS_MUTP_VHTMU; /* mu type, dflt to VHTMUMIMO */

	txs_mutype = TX_STATUS_MUTYP(mu_info->wlc->pub->corerev, txs->status.s5);

	if (((txs_mutype != TX_STATUS_MUTP_VHTMU) && (txs_mutype != TX_STATUS_MUTP_HEMM)) ||
		((gbmp == 0) && (txs_mutype == TX_STATUS_MUTP_HEMM))) {
		// XXX: Not the final GBMP nor guarantees something is wrong.
		return;
	}

	rspec += (1 << TX_STATUS64_MU_NSS_SHIFT);	/* Calculate actual NSS */
	mcs = rspec & WL_RSPEC_VHT_MCS_MASK;
	nss = (rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT;

	if (gbmp == 0 || mutxcnt > MUTX_MAXMUTXCNT || gid_invalid || mcs > WLC_MAX_VHT_MCS ||
		nss == 0 || nss > VHT_CAP_MCS_MAP_NSS_MAX) {
		WL_ERROR(("%s wl%d asserts: gbmp = 0x%x mutxcnt = %u "
			"gid = %u mcs = %u nss = %u\n",
			__FUNCTION__, mu_info->wlc->pub->unit,
			gbmp, mutxcnt, gid, mcs, nss));
		WL_ERROR(("wl%d raw txstatus %04X %04X | "
		"%04X %04X | "
		"%08X %08X %08X | %08X %08X | %08X\n",
		mu_info->wlc->pub->unit,
		txs->status.raw_bits, txs->frameid,
		txs->sequence, txs->phyerr,
		txs->status.s3, txs->status.s4,
		txs->status.s5,
		txs->status.ack_map1, txs->status.ack_map2,
		txs->status.s8));
		ASSERT(0);
		return;
	}

	/* gnum must be valid number, as gbmp is read from 4bits field
	 * and (gbmp == 0) is already filtered out above.
	 */
	for (; gbmp; gbmp >>= 1) {
		if ((gbmp & 0x1)) {
			gnum++;
		}
	}

	/* Update MCS count */
	rate_index = mcs + ((nss - 1) * MUTX_MCS_NUM);
	ASSERT(rate_index < MUTX_MCS_INDEX_NUM);

	mu_info->mutx_cnt += mupktnum;

	mutx_stats->gnum[gnum - 1]++;
	mutx_stats->gpos_cnt[gpos]++;
	mutx_stats->txmu[rate_index] += mupktnum;
	if (is_sgi) {
		mutx_stats->txmu_sgi[rate_index] += mupktnum;
		mutx_stats->txmu_sgi_succ[rate_index] += muacknum;
	}
	mutx_stats->txmu_succ[rate_index] += muacknum;

	mutx_pfmon_stats->gpos_cnt[gpos]++;
	mutx_pfmon_stats->mutx_cnt += mupktnum;
	mutx_pfmon_stats->txmu[rate_index] += mupktnum;
	if (mutxcnt == 1) {
		mutx_snr_calib_stats->txmu[rate_index] += mupktnum;
		mutx_snr_calib_stats->txmu_succ[rate_index] += muacknum;
	}

	if (mutxcnt == 0) {
		/* This must be RTS failure to the retry limit */
		ASSERT(mupktnum == 0);
	} else {
		mutx_stats->mutxcnt_stat[mutxcnt - 1] += mupktnum;
		mutx_pfmon_stats->mutxcnt_stat[mutxcnt - 1] += mupktnum;
	}

	if (mu_info->mu_snr_calib) {
		mu_scb = MUTX_SCB(mu_info, scb);
		wlc_mu_snr_calib_scb(wlc, mu_scb);
	}

	if (txs_mutype == TX_STATUS_MUTP_VHTMU) {
		/* Update group ID count */
		mutx_stats->gid_cnt[gid]++;
		mutx_stats->gid_last_rate[gid] = rate_index;
		/* Trace MU frame receipt */
		WL_INFORM(("wl%d: Sent MU frame to "MACF" with group ID %u, mcs %ux%u.\n",
			mu_info->pub->unit, ETHER_TO_MACF(scb->ea), gid, mcs, nss));
	} else {
		/* HEMM */
		WL_INFORM(("wl%d: Sent MU frame to "MACF" with mcs %ux%u.\n",
			mu_info->pub->unit, ETHER_TO_MACF(scb->ea), mcs, nss));
	}
}

void BCMFASTPATH
wlc_mutx_upd_interm_counters(wlc_mutx_info_t *mu_info, scb_t *scb,
	tx_status_t *txs)
{
	mutx_scb_t *mu_scb;
	mutx_stats_t *mutx_stats;
	mutx_pfmon_stats_t *mutx_pfmon_stats;
	mutx_snr_calib_stats_t *mutx_snr_calib_stats;

	if (mu_info == NULL) {
		return;
	}

	mu_scb = MUTX_SCB(mu_info, scb);
	if (mu_scb == NULL) {
		return;
	}

	mutx_stats = mu_scb->mutx_stats;
	mutx_snr_calib_stats = mu_scb->mutx_snr_calib_stats;
	if (mutx_stats == NULL) {
		return;
	}

	mutx_pfmon_stats = mu_scb->mutx_pfmon_stats;
	if (mutx_pfmon_stats == NULL) {
		return;
	}

	wlc_mutx_mutxs_update(mu_info, scb, mutx_stats, mutx_pfmon_stats,
			mutx_snr_calib_stats, txs);
}

void BCMFASTPATH
wlc_mutx_update_txcounters(wlc_mutx_info_t *mu_info, scb_t *scb,
	bool txs_mu, tx_status_t *txs, ratesel_txs_t *rs_txs, uint8 rnum)
{
	mutx_scb_t *mu_scb;
	uint8 nss, mcs, i;
	mutx_stats_t *mutx_stats;
	mutx_pfmon_stats_t *mutx_pfmon_stats;
	mutx_snr_calib_stats_t *mutx_snr_calib_stats;
	if (!mu_info || !scb) {
		return;
	}

	mu_scb = MUTX_SCB(mu_info, scb);
	if (mu_scb == NULL) {
		return;
	}

	mutx_stats = mu_scb->mutx_stats;
	mutx_snr_calib_stats = mu_scb->mutx_snr_calib_stats;
	if (mutx_stats == NULL) {
		return;
	}

	mutx_pfmon_stats = mu_scb->mutx_pfmon_stats;
	if (mutx_pfmon_stats == NULL) {
		return;
	}

	mu_info->mu_ncons += rs_txs->ncons;
	mu_info->mu_nlost += rs_txs->nlost;

	mutx_stats->mu_ncons += rs_txs->ncons;
	mutx_stats->mu_nlost += rs_txs->nlost;

	mutx_pfmon_stats->mu_ncons += rs_txs->ncons;

	if (txs_mu) {
		wlc_mutx_mutxs_update(mu_info, scb, mutx_stats, mutx_pfmon_stats,
				mutx_snr_calib_stats, txs);
	} else {
		uint rate_index;
		/* Txed as SU */
		for (i = 0; i < rnum; i++) {
			ratespec_t rspec = rs_txs->txrspec[i];

			if (RSPEC_ISVHT(rspec)) {
				mcs = rspec & WL_RSPEC_VHT_MCS_MASK;
				nss = (rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT;
				if (mcs > WLC_MAX_VHT_MCS || nss == 0 || nss > MUTX_NSS_NUM) {
					WL_ERROR(("%s wl%d asserts: mcs = %d nss = %d\n",
						__FUNCTION__, mu_info->wlc->pub->unit, mcs, nss));
					ASSERT(0);
					continue;
				}

				/* Update MCS count */
				rate_index = mcs + ((nss - 1) * MUTX_MCS_NUM);
				ASSERT(rate_index < MUTX_MCS_INDEX_NUM);
				mutx_stats->txsu[rate_index] += rs_txs->tx_cnt[i];
				mutx_stats->txsu_succ[rate_index] += rs_txs->txsucc_cnt[i];

				/* Update group ID count */
				mutx_stats->gid_cnt[VHT_SIGA1_GID_NOT_TO_AP]++;
				mutx_stats->gid_last_rate[VHT_SIGA1_GID_NOT_TO_AP] = rate_index;
			} else if (RSPEC_ISHE(rspec)) {
				mcs = rspec & WL_RSPEC_HE_MCS_MASK;
				nss = (rspec & WL_RSPEC_HE_NSS_MASK) >> WL_RSPEC_HE_NSS_SHIFT;
				if (mcs > WLC_MAX_HE_MCS || nss == 0 || nss > MUTX_NSS_NUM) {
					WL_ERROR(("%s wl%d asserts: mcs = %d nss = %d\n",
						__FUNCTION__, mu_info->wlc->pub->unit, mcs, nss));
					ASSERT(0);
					continue;
				}

				rate_index = mcs + ((nss - 1) * MUTX_MCS_NUM);
				ASSERT(rate_index < MUTX_MCS_INDEX_NUM);
				mutx_stats->txsu[rate_index] += rs_txs->tx_cnt[i];
				mutx_stats->txsu_succ[rate_index] += rs_txs->txsucc_cnt[i];
			}
		}
	}
}
#endif   /* WLCNT */

void
wlc_mutx_txfifo_complete(wlc_info_t *wlc)
{
	wlc_mutx_info_t *mu_info = wlc->mutx;

	/* Prevent loop from wlc_sync_txfifo_all */
	if (mu_info->mutx_switch_in_progress)
		return;

	if (!mu_info->mutx_active) {
		/* Check if we need to switch from SU to MU */
		if (wlc_txbf_mu_cap_stas(wlc->txbf) >= 2) {
			wlc_mutx_switch(wlc, TRUE, FALSE);
		}
	} else {
		/* Check if we need to switch from MU to SU */
		if (wlc_txbf_mu_cap_stas(wlc->txbf) < 2) {
			wlc_mutx_switch(wlc, FALSE, FALSE);
		}
	}
}

#ifdef MAC_AUTOTXV_OFF
static void wlc_mutx_admit_users_reinit(wlc_mutx_info_t *mutx)
{
	wlc_info_t *wlc = mutx->wlc;
	scb_t *scb;
	int i, err;
	uint8 mucidx /* a.k.a fifogrp idx */;
	uint8 max_mmu_usrs = wlc_txcfg_max_mmu_clients_get(wlc->txcfg);

	if (D11REV_LT(mutx->pub->corerev, 128)) {
		return;
	}

	for (i = 0; i < max_mmu_usrs; i++) {
		scb = mutx->mu_clients[i];
		if (!scb) {
			continue;
		}

		mucidx = wlc_mutx_sta_mucidx_get(mutx, scb);
		if (mucidx == BF_SHM_IDX_INV) {
			continue;
		}

		wlc_txbf_tbcap_update(wlc->txbf, scb);
		err = wlc_macreq_upd_bfi(wlc, scb, mucidx, TRUE);
		if (err != BCME_OK && err != BCME_UNSUPPORTED) {
			WL_ERROR(("wl%d: MAC command failed to update bfi for"
				"STA "MACF" i:%u.\n",
				mutx->pub->unit, ETHER_TO_MACF(scb->ea), i));
			continue;
		}

		WL_INFORM(("wl%d: %s: sta "MACF" updated with fifo_idx:%d\n",
			wlc->pub->unit, __FUNCTION__, ETHER_TO_MACF(scb->ea), mucidx));
	}
}
#endif /* MAC_AUTOTXV_OFF */
#endif /* WL_MU_TX */
