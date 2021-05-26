/*
 * MU-MIMO transmit module for Broadcom 802.11 Networking Adapter Device Drivers
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
#include <wlc_dbg.h>
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
#include <wlc_fifo.h>
#ifdef WLTAF
#include <wlc_taf.h>
#endif // endif
#include <wlc_he.h>
#include <wlc_musched.h>
#include <wlc_ampdu_cmn.h>
#include <bcmdevs.h>

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
#define MPDU_THRES_SETTLE 48
#define MPDU_THRES_HIGH 800
#define MPDU_THRES_LOW 40
#define MCS_MIN 0
#define MCS_MAX_VHT 9
#define MCS_MAX_HE  11
#define PHYREG_M2V_MSG0 0x109c
#define ADJ_IDX(index) (index + 8)

#define MUTX_TP_MAX 2
#define TP_TO_IDX(mutx_tp) ((mutx_tp == HEMMU) ? 1 : 0)

/* MU-MIMO Transmit Policy */
typedef struct mutx_policy {
	uint32	bw_policy;	/* NONE, 20MHZ, 40MHZ, or 80MHZ */
	uint32	ac_policy;	/* NONE, BE/BK, VI, or VO */
	uint32	effective_ac;	/* Nonzero AC to avoid frequent link changes: BE/BK, VI, or VO */
} mutx_policy_t;

/* iovar table */
enum wlc_mutx_iov {
	IOV_MUTX_PKTENG = 2,
	IOV_MUTX_PKTENG_ADDSTA = 3,
	IOV_MUTX_PKTENG_CLRSTA = 4,
	IOV_MUTX_PKTENG_TX = 5,
	IOV_MUTX_SENDGID = 6,
	IOV_MU_POLICY = 7,
	IOV_MUTX_MUINFO = 8,
	IOV_MU_SNR_CALIB = 9,
	IOV_MU_FEATURES = 10,
	IOV_MUPFMON_MUMCS_THRESH = 11,
	IOV_MUTX_MPDUSZ_MU_ADMIT_THRESH = 12,
	IOV_MUTX_MPDUSZ_DLOFDMA_ADMIT_THRESH = 13,
	IOV_MUTX_LAST
};

/* Max number of times the driver will send a Group ID Mgmt frame to
 * a STA when the STA does not ACK.
 */
#define MU_GID_SEND_LIMIT  3

/* Minimum number of transmit chains for MU TX to be active */
#define MUTX_TXCHAIN_MIN   3

static const bcm_iovar_t mutx_iovars[] = {
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
	{"mu_features", IOV_MU_FEATURES, /* not applicable to HE operation */
	(IOVF_SET_DOWN), 0, IOVT_UINT32, 0
	},
	{"mupfmon_mumcs_thresh", IOV_MUPFMON_MUMCS_THRESH,
	0, 0, IOVT_UINT32, 0
	},
	{"mpdusz_mu_admit_thresh", IOV_MUTX_MPDUSZ_MU_ADMIT_THRESH,
	0, 0, IOVT_BUFFER, 0
	},
	{"mpdusz_dlofdma_admit_thresh", IOV_MUTX_MPDUSZ_DLOFDMA_ADMIT_THRESH,
	0, 0, IOVT_BUFFER, 0
	},
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

/* By default, each MU client is a member of groups 1 - . */
static uint8 dflt_membership_16[MUCLIENT_NUM_16][MU_MEMBERSHIP_SIZE] = {
	{ 0xFE, 0xFF, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00 },
	{ 0xFE, 0xFF, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00 },
	{ 0xFE, 0xFF, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00 },
	{ 0xFE, 0xFF, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00 },
	{ 0xFE, 0xFF, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00 },
	{ 0xFE, 0xFF, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00 },
	{ 0xFE, 0xFF, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00 },
	{ 0xFE, 0xFF, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00 },
	{ 0xFE, 0xFF, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00 },
	{ 0xFE, 0xFF, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00 },
	{ 0xFE, 0xFF, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00 },
	{ 0xFE, 0xFF, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00 },
	{ 0xFE, 0xFF, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00 },
	{ 0xFE, 0xFF, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00 },
	{ 0xFE, 0xFF, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00 },
	{ 0xFE, 0xFF, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00 }
	};
/* The default user position for each of the 16 MU clients. */
static uint8 dflt_position_16[MUCLIENT_NUM_16][MU_POSITION_SIZE] = {
	{ 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
	{ 0x00, 0x6C, 0x15, 0x04, 0x01, 0x00, 0x04, 0x01, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
	{ 0x94, 0x01, 0x08, 0x41, 0x10, 0x00, 0x58, 0x00, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
	{ 0xD4, 0x42, 0x1D, 0x45, 0x11, 0x00, 0x68, 0x01, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
	{ 0x68, 0x95, 0x42, 0x10, 0x04, 0x00, 0x80, 0x16, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
	{ 0x78, 0xA6, 0x57, 0x14, 0x05, 0x00, 0x84, 0x26, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
	{ 0xAC, 0xFB, 0x4A, 0x51, 0x14, 0x00, 0x98, 0x1A, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
	{ 0xFC, 0xFF, 0x5F, 0x55, 0x15, 0x00, 0xA8, 0x2A, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
	{ 0x00, 0x18, 0xA0, 0xAA, 0x6A, 0x55, 0xFD, 0x3F, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
	{ 0x00, 0x6C, 0xF5, 0xAB, 0xAA, 0x65, 0xFD, 0x3F, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
	{ 0x94, 0x01, 0xA8, 0xAA, 0xFF, 0x5A, 0xFD, 0x3F, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
	{ 0xD4, 0x42, 0xFD, 0xAB, 0xFF, 0x6B, 0xFD, 0x3F, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
	{ 0x68, 0x95, 0xA2, 0xFE, 0x6A, 0xBD, 0xFE, 0x3F, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
	{ 0x78, 0xA6, 0xF7, 0xFF, 0xAA, 0xBD, 0xFF, 0x3F, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
	{ 0xAC, 0xFB, 0xAA, 0xFE, 0xFF, 0xFE, 0xFE, 0x3F, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
	{ 0xFC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x3F, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 }
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
	struct mutx_scb_list *next, *prev, *link_p, *link_n, *head;
	void *data;
	int cnt;
} mutx_scb_list_t;

/* State structure for the MU-MIMO module created by
 * wlc_mutx_module_attach().
 */
struct wlc_mutx_info {
	osl_t *osh;              /* OSL handle */
	wlc_info_t *wlc;         /* wlc_info_t handle */
	wlc_pub_t *pub;          /* wlc_pub_t handle */
	int scb_handle;			     /* scb cubby handle to retrieve data from scb */
	bool mutx_pkteng; /* mupkteng -- debug feature */
	/* below are reported from txstatus */
	/* these counters also count for users now switched to SU */
	uint32 tot_cnt; /* MU + SU tx'd */
	uint32 mutx_cnt; /* tx'd as MU */
	uint32 mu_ncons; /* queued as MU and consumed as either MU/SU */
	uint32 mu_nlost; /* queued as MU and lost as either MU/SU */
	uint32 mutx_client_scheduler_dur;
	uint32 mutx_client_scheduler_cnt;
	bool mutx_client_scheduler;
	bool mu_snr_calib;
	bool block_datafifo;
	uint32 mupfmon_mumcs_thresh;
	uint32 mutx_admit_mcs_thresh;
	bcm_notif_h mutx_state_notif_hdl;	/* mutx state notifier handle. */
	bool _mu_tx[MUTX_TP_MAX];
	bool muclient_pfmon;
	bool ac_policy_on;
	bool client_samebw;
	uint8 muclient_nrx;
	int rssi_thresh;
	uint32 mpdusz_mu_admit_thresh[BW_160MHZ + 1];
	uint32 mpdusz_dlofdma_admit_thresh[BW_160MHZ + 1];
	uint32 mu_snd_per_sec;

	/* TRUE if MU transmit is operational. To be operational, the phy must support MU BFR,
	 * MU BFR must be enabled by configuration, and there must be nothing in the system
	 * state that precludes MU TX, such as having too few transmit chains enabled.
	 */
	bool active;
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
	bool mutx_switch_in_progress;
	uint8 txstats_nupd;
	mutx_scb_list_t *on_hold_list;
	mutx_scb_list_t *admit_list; /* mu capable stas currenty admitted */
	mutx_scb_list_t *candidate_list; /* mu capable stas which match all policy. */
	mutx_scb_list_t *alternate_list; /* mu capable stas which only match bw_policy. */
	mutx_scb_list_t *evictee_list;
	mutx_scb_list_t *new_list; /* flush list */
	mutx_scb_list_t *admit_ofdma_list; /*  ofdma capable sts currently admitted */
	mutx_scb_list_t *candidate_ofdma_list; /* ofdma capable stas */
	mutx_scb_list_t *mu_cap_stas_list; /* flush list */
	/* Max number of muclients */
	scb_t *mu_clients[MUCLIENT_NUM];
	int max_muclients;
	/* MU TX policy - AC and BW - different from mu_policy_t */
	mutx_policy_t *mutx_policy;
	bool client_bw160;
};

#if defined(BCMDBG) || defined(DUMP_MUTX)
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
	uint32 mutxcnt_succ[MUTX_MAXMUTXCNT];
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
	uint32 txassu_reason[C_M2SQ_REASON_MAX];
	uint32 snd_fail_cnt;
	bool   snd_epch_prev;
} mutx_stats_t;
#endif // endif

#ifdef WLCNT
typedef struct mutx_pfmon_stats {
	uint32 mu_ncons; /* queued as MU and consumed as either MU/SU */
	uint32 mutx_cnt;   /* tx'd as MU (== SUM of txmu[i]) */
	uint32 gpos_cnt[MUTX_MAXGNUM];
	uint32 mutxcnt_stat[MUTX_MAXMUTXCNT];
	uint32 mutxcnt_succ[MUTX_MAXMUTXCNT];
	/* Total mu frame transmission attempts */
	uint32 txmu[MUTX_MCS_INDEX_NUM];
	/* Total mu frames that end up being txed as SU */
	uint32 txsu[MUTX_MCS_INDEX_NUM];
	/* Total mu frame transmission attempts at primray mcs */
	uint32 txmu_primrate[MUTX_MCS_INDEX_NUM];
	uint32 txassu_reason[C_M2SQ_REASON_MAX];
	uint32 snd_fail_cnt;
	bool   snd_epch_prev;
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
#endif /* WLCNT */

enum {
	MUTX_AC_NONE = 0,
	MUTX_AC_BEBK,
	MUTX_AC_VI,
	MUTX_AC_VO
};

enum {
	MUTX_MUINFO_OPT_NONE = 0,
	MUTX_MUINFO_OPT_SIMPLE = 1,
	MUTX_MUINFO_OPT_DETAIL = 2,
	MUTX_MUINFO_OPT_DBG = 3
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
	MUTX_REASON_DISASSOC_EVICT = 8,
	MUTX_REASON_PFMON_PRIMRATE_SUCC_EVICT = 9,
	MUTX_REASON_PFMON_PRIMRATE_MUMCS_EVICT = 10,
	MUTX_REASON_PFMON_TXSASMU_EVICT = 11,
	MUTX_REASON_PFMON_AVG_MPDU_SZ_EVICT = 12,
	MUTX_REASON_PFMON_AC_VO_ONLY_EVICT = 13,
	MUTX_REASON_MAX = 14
};

enum {
	MUTX_POLICY_EVICT_NONE = 0,
	MUTX_POLICY_EVICT_BW = 1,
	MUTX_POLICY_EVICT_AC = 2,
	MUTX_POLICY_EVICT_PFMON = 3,
	MUTX_POLICY_EVICT_MAX = 4
};

static char* mutx_reason2str[MUTX_REASON_MAX] = {
	"new association",
	"new admit",
	"bw policy mismatch",
	"ac policy mismatch",
	"high traffic",
	"low traffic",
	"blacklist admit",
	"blacklist evict",
	"disassoc_evict",
	"pfmon_primrate_succ_evict",
	"pfmon_primrate_mumcs_evict",
	"pfmon_txasmu_evict",
	"pfmon_avg_mpdu_sz_evict",
	"pfmon_ac_vo_only_evict"
};

#define HEURS_CNTMAX 64
#define HEURS_BOUNDINC(i)		(((i) < ((HEURS_CNTMAX)-1)) ? (i)+1 : HEURS_CNTMAX)
#define HEURS_BOUNDDEC(i)		(((i) > (0)) ? (i)- 1 : 0)

typedef enum {
	HEURS_INIT = 0,
	HEURS_MU_ADMIT,
	HEURS_MU_ADMIT_INC,
	HEURS_MU_PFMON_EVICT,
	HEURS_MU_POLICY_MISMATCH_EVICT,
	HEURS_MU_POLICY_SCORE_EVICT,
	HEURS_SU_DEC
} heurs_reason_t;

typedef struct musta_heurs {
	uint8 counter0;
	uint8 counter1;
	uint8 counter2;
	uint8 counter3;
} musta_heurs_t;

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
#if defined(BCMDBG) || defined(DUMP_MUTX)
	mutx_stats_t *mutx_stats;	/* counters/stats */
#else /* BCMDBG || BCMDBG_DUMP || DUMP_MUTX */
	void *_unused;
#endif // endif
#ifdef WLCNT
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
	wlc_ravg_info_t ravg_info[MUTX_AC_VO+1];
	int rssi;
	uint32 nrx;
	/* the policy evict reason if MU client will be evicted from policy change. */
	uint8 policy_evict;
	/* the reason of MU client admit and eviction */
	uint8 reason;
	uint8 assocd;
	uint8 tp;
	musta_heurs_t musta_heurs;
	uint8 txstats_nupd;
	uint32 m2sq_anygrp;
	uint32 m2sq_snd_fail;
	uint32 m2sq_rr_lmt;
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

#define MUTX_SCB_CUBBY(mu_info, scb)  ((mutx_scb_cubby_t*)(SCB_CUBBY((scb), (mu_info)->scb_handle)))
#define MUTX_SCB(mu_info, scb)        (mutx_scb_t*)(MUTX_SCB_CUBBY((mu_info), scb)->mutx_scb)

/* Basic module infrastructure */

#ifdef WL_MU_FEATURES_AUTO
static int wlc_mutx_switch(wlc_info_t *wlc, bool mutx_feature, bool is_iov);
#endif // endif
static int wlc_mu_doiovar(void *hdl, uint32 actionid,
	void *p, uint plen, void *a, uint alen, uint vsize, wlc_if_t *wlcif);
#if defined(BCMDBG) || defined(DUMP_MUTX)
static int wlc_mutx_dump(void *ctx, bcmstrbuf_t *b);
static int wlc_mutx_dump_clr(void *ctx);
#endif // endif
static void wlc_mutx_watchdog(void *mi);
static int wlc_mutx_up(void *mi);
#if defined(BCMDBG) || defined(BCMDBG_MU)
static int wlc_mutx_sendgid(wlc_mutx_info_t *mu_info);
#endif // endif

#ifdef WL_MU_FEATURES_AUTO
static void wlc_mutx_state_upd(wlc_info_t *wlc, uint8 state);
#endif // endif
static bool wlc_mutx_sta_nrx_check(wlc_mutx_info_t *mu_info, scb_t *scb);
static bool wlc_mutx_sta_ac_check(wlc_mutx_info_t *mu_info, struct scb *scb, uint link_bw);
/* SCB cubby management */
static void wlc_mutx_scb_state_upd(void *ctx, scb_state_upd_data_t *notif_data);
static uint mu_scb_cubby_secsz(void *context, scb_t *scb);
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

static bool mu_sta_mu_ready(wlc_mutx_info_t *mu_info, scb_t *scb, bool strict_check);
static int mu_sta_mu_enable(wlc_mutx_info_t *mu_info, scb_t *scb);
static int mu_sta_mu_disable(wlc_mutx_info_t *mu_info, scb_t *scb);
static bool mu_scb_membership_change_get(wlc_mutx_info_t *mu_info, scb_t *scb);
static void mu_scb_membership_change_set(wlc_mutx_info_t *mu_info, scb_t *scb, bool pending);

static void mu_candidates_select(wlc_mutx_info_t *mu_info, int *vhtmu_cand_cnt,
	int *hemmu_cand_cnt, int *dlofdma_cand_cnt, bool sched);
static void mu_clients_eviction(wlc_mutx_info_t *mu_info, bool sched);
#ifdef WL_PSMX
static void wlc_mutx_clr_mubf(wlc_info_t *wlc);
#endif // endif
static bool wlc_mutx_client_scheduler(wlc_mutx_info_t * mu_info, bool sched);

/* Manage user position, group membership bit arrays */
static uint8 mu_user_pos_get(uint8 *pos_array, uint16 group_id);
static bool mu_any_membership(uint8 *membership);

/* Sending group ID management frames */
static int mu_group_id_msg_send(wlc_info_t *wlc, scb_t *scb,
                                uint8 *membership, uint8 *user_pos);
static void wlc_mutx_gid_complete(wlc_info_t *wlc, uint txstatus, void *arg);
static uint32 wlc_mutx_bw_policy_update(wlc_mutx_info_t *mu_info, bool force);

#ifdef WL_MUPKTENG
static int wlc_mupkteng_addsta(wlc_mutx_info_t *mu_info, struct ether_addr *ea, int client_idx,
		int nrxchain);
static int wlc_mupkteng_clrsta(wlc_mutx_info_t *mu_info, int client_idx);
static int wlc_mupkteng_tx(wlc_mutx_info_t *mu_info, mupkteng_tx_t *mupkteng_tx);
#endif // endif
static void wlc_muclient_clear_pfmon_stats(wlc_mutx_info_t *mu_info, scb_t *scb);
static bool wlc_muclients_pfmon_eval_primrate_succ(wlc_mutx_info_t *mu_info, scb_t *scb);
static bool wlc_muclients_pfmon_eval_tx_as_mu(wlc_mutx_info_t *mu_info, scb_t *scb);
static bool wlc_muclients_pfmon_eval_primrate_mcs(wlc_mutx_info_t *mu_info, scb_t *scb);
static void wlc_muclient_pfmon(wlc_mutx_info_t *mu_info);
static int wlc_mutx_fifo_hold_set(wlc_info_t *wlc, void *fifo_bitmap);
static void wlc_mutx_fifo_hold_clr(wlc_info_t *wlc, void *fifo_bitmap);
#if defined(BCMDBG) || defined(DUMP_MUTX)
static void wlc_muclient_clear_stats(wlc_mutx_info_t *mu_info, mutx_stats_t *mutx_stats);
#endif // endif

static void wlc_musta_upd_heurs(wlc_mutx_info_t *mu_info, scb_t *scb,
	heurs_reason_t reason);
static void wlc_mutx_admit_clients(wlc_mutx_info_t *mu_info);
static int wlc_mutx_sta_mucidx_set(wlc_mutx_info_t *mu_info, scb_t *scb);
#ifdef MAC_AUTOTXV_OFF
static void wlc_mutx_admit_users_reinit(wlc_mutx_info_t *mutx);
#endif // endif

/* APIs to set and get MU group membership */
static void wlc_mutx_membership_clear(wlc_mutx_info_t *mu_info, struct scb *scb);
#ifdef NOT_YET
static int wlc_mutx_membership_get(wlc_mutx_info_t *mu_info, uint16 client_index,
	uint8 *membership, uint8 *position);
#endif // endif
static int wlc_mutx_membership_set(wlc_mutx_info_t *mu_info, uint16 client_index,
                            uint8 *membership, uint8 *position);
static void wlc_mutx_dlofdma_client_set_clear(wlc_mutx_info_t *mu_info);

#define LIST_IS_EMPTY(head) \
	(head == head->next)
#define FOREACH_LIST_POS(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); pos = n, n = pos->next)

#define ITEMS_IN_LIST(head) \
	(head->cnt)
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
mutx_scb_list_move(mutx_scb_list_t *head, mutx_scb_list_t *new)
{
	ASSERT((head != NULL) && (new != NULL));
	if ((head != NULL) && (new != NULL)) {
		if (new->head) {
			new->prev->next = new->next;
			new->next->prev = new->prev;
			new->head->cnt--;
			ASSERT(new->head->cnt >= 0);
		}
		head->prev->next = new;
		new->prev = head->prev;
		head->prev = new;
		new->next = head;
		new->head = head;
		head->cnt++;
	}
}

static void
mutx_scb_list_rem(mutx_scb_list_t *item)
{
	ASSERT((item->head != NULL) && (item != NULL));
	if (item != NULL) {
		item->prev->next = item->next;
		item->next->prev = item->prev;
		item->next = item->prev = item;
		item->head->cnt--;
		ASSERT(item->head->cnt >= 0);
		item->head = NULL;
	}
}

#define MUTX_SCORE_MAX		0xFFFFFFFF
#define MUTX_SCORE_MIN		0x0

#define MUTX_RSSI_MIN		(-82)
#define MUTX_ADMIT_MCS_THRESH 5
/* Threshold for 2x2 chip(AP) */
#define MUTX_ADMIT_MCS_THRESH_2x2 7

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
		if (!mu_scb) {
			ASSERT(0);
			continue;
		}
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
		if (!mu_scb) {
			ASSERT(0);
			continue;
		}

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

#if defined(BCMDBG) || defined(DUMP_MUTX)
static void
wlc_mutx_print_txpermcs(wlc_mutx_info_t *mu_info, uint32 tot_tx, uint32 tot_tx_succ,
	uint8 last_mcs_idx, uint32 *tx, uint32 *tx_succ, bcmstrbuf_t *b,
	bool is_mu, bool is_sgi, uint8 mu_tp)
{
	char name_tot[30], name_per[30];
	uint8 MAX_RATES = (mu_tp == TX_STATUS_MUTP_HEMM) ? MAX_HE_RATES : MAX_VHT_RATES;

	if (!tot_tx)
		return;

	if ((mu_tp != TX_STATUS_MUTP_HEMM) && (mu_tp != TX_STATUS_MUTP_VHTMU)) {
		WL_ERROR(("%s wl%d asserts: mu_tp %d ",
		__FUNCTION__, mu_info->wlc->pub->unit, mu_tp));
		ASSERT(0);
		return;
	}

	if (mu_tp == TX_STATUS_MUTP_HEMM) {
		sprintf(name_tot, "TX HE-%s%s TOT", (is_mu ? "MMU" : "SU"), (is_sgi ? " SGI" : ""));
		sprintf(name_per, "TX HE-%s%s PER", (is_mu ? "MMU" : "SU"), (is_sgi ? " SGI" : ""));
	} else if (mu_tp == TX_STATUS_MUTP_VHTMU) {
		sprintf(name_tot, "TX VHT-%s%s TOT", (is_mu ? "MU" : "SU"), (is_sgi ? " SGI" : ""));
		sprintf(name_per, "TX VHT-%s%s PER", (is_mu ? "MU" : "SU"), (is_sgi ? " SGI" : ""));
	}
	wlc_print_dump_table(b, name_tot, tx,
		name_per, tx_succ,
		AMPDU_MAX_HE, MAX_RATES, TABLE_MCS);

#ifdef NOT_YET
	if (mu_tp == TX_STATUS_MUTP_HEMM) {
		bcm_bprintf(b, "TX HE-%s%s TOT :",
			(is_mu ? "MMU" : "SU"), (is_sgi ? " SGI" : ""));
	} else if (mu_tp == TX_STATUS_MUTP_VHTMU) {
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
}

static void
wlc_mutx_dump_stats(wlc_mutx_info_t *mu_info,
	mutx_stats_t *mutx_stats, uint8 mu_tp, bcmstrbuf_t *b, uint8 option)
{
	uint32 total, tot_txmu = 0, tot_txmu_succ = 0, tot_txmu_sgi = 0, tot_txmu_sgi_succ = 0;
	uint32 tot_txsu = 0, tot_txsu_succ = 0, tot_txassu_reason = 0, tot_txassu = 0;
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
	mu_info->tot_cnt += total;

	tot_txassu += mutx_stats->txassu_reason[C_M2SQ_REASON_NONE];
	for (i = C_M2SQ_NOT_IN_ANYGRP; i < C_M2SQ_REASON_MAX; i++) {
		tot_txassu += mutx_stats->txassu_reason[i];
		tot_txassu_reason += mutx_stats->txassu_reason[i];
	}
	bcm_bprintf(b, "mutx: tot_tx %u tx_as_mu %u (%u%%)"
		" ncons %u nlost %u\n",
		total, tot_txmu, (total ? (tot_txmu * 100 / total) : 0),
		mutx_stats->mu_ncons, mutx_stats->mu_nlost);
	if (option == MUTX_MUINFO_OPT_DBG) {
		if (tot_txassu) {
			bcm_bprintf(b, "mutx: txassu %u  reason_none %u\n"
				" reason %u vasip %u snd_fail %u retry_lmt %u snd_fail_cnt %u\n",
				tot_txassu, mutx_stats->txassu_reason[C_M2SQ_REASON_NONE],
				tot_txassu_reason, mutx_stats->txassu_reason[C_M2SQ_NOT_IN_ANYGRP],
				mutx_stats->txassu_reason[C_M2SQ_SOUNDNG_FAIL],
				mutx_stats->txassu_reason[C_M2SQ_RETRY_LMT_RCHD],
				mutx_stats->snd_fail_cnt);
		}
	}

	wlc_mutx_print_txpermcs(mu_info, tot_txmu, tot_txmu_succ, last_mcs_idx_mu,
		mutx_stats->txmu, mutx_stats->txmu_succ, b, TRUE, FALSE, mu_tp);

	if (option == MUTX_MUINFO_OPT_DBG) {
		wlc_mutx_print_txpermcs(mu_info, tot_txmu_sgi, tot_txmu_sgi_succ, last_mcs_idx_mu,
			mutx_stats->txmu_sgi, mutx_stats->txmu_sgi_succ, b, TRUE, TRUE, mu_tp);

		wlc_mutx_print_txpermcs(mu_info, tot_txsu, tot_txsu_succ, last_mcs_idx_su,
			mutx_stats->txsu, mutx_stats->txsu_succ, b, FALSE, FALSE, mu_tp);
	}
	/* determines highest mutx *index* on which a transmit took place */
	for (i = 0, total = 0, last = 0; i < MUTX_MAXMUTXCNT; i++) {
		total += mutx_stats->mutxcnt_stat[i];
		if (mutx_stats->mutxcnt_stat[i]) {
			last = i;
		}
	}

	if (option == MUTX_MUINFO_OPT_DBG) {
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

	/* determines highest mutx *index* on which a transmit took place */
	for (i = 0, total = 0, last = 0; i < MUTX_MAXMUTXCNT; i++) {
		if (mutx_stats->mutxcnt_succ[i]) {
			last = i;
		}
	}

	bcm_bprintf(b, "mutxcnt_succ :");
	for (i = 0; i <= last; i++) {
		if (mutx_stats->mutxcnt_stat[i]) {
			bcm_bprintf(b, "  %d(%d%%)", mutx_stats->mutxcnt_succ[i],
			(mutx_stats->mutxcnt_succ[i] * 100) / mutx_stats->mutxcnt_stat[i]);
		}
		if ((i % MUTX_MAXMUTXCNT) == 5 && i != last) {
				bcm_bprintf(b, "\n        :");
		}
	}
	bcm_bprintf(b, "\n");

	// XXX: For HEMM, these stats are derived from txs.gbmp which isn't the final gbmp.
	bcm_bprintf(b, "grpsize :");
	total = mutx_stats->gnum[0] + mutx_stats->gnum[1] +
		mutx_stats->gnum[2] + mutx_stats->gnum[3];
	for (i = 0; i < MUTX_MAXGNUM; i++) {
		bcm_bprintf(b, "  %d(%d%%)",
		mutx_stats->gnum[i], (total ? mutx_stats->gnum[i]*100/total : 0));
	}
	bcm_bprintf(b, "\n");

	if (option == MUTX_MUINFO_OPT_DBG) {
		bcm_bprintf(b, "gpos    :");
		for (i = 0; i < MUTX_MAXGNUM; i++) {
			bcm_bprintf(b, "  %d",
			mutx_stats->gpos_cnt[i]);
		}
		bcm_bprintf(b, "\n");
	}
}
#endif // endif

/* Dump MU-MIMO state information. */
static int
wlc_mutx_muinfo(wlc_mutx_info_t *mu_info, bcmstrbuf_t *b, uint8 option)
{
	scb_t *scb;
	mutx_scb_t *mu_scb;
	int i;
	uint16 g;
	uint8 pos;
	bool empty_group;
	mutx_scb_list_t *hold_pos, *next;
	wlc_info_t *wlc = mu_info->wlc;
	mutx_policy_t *mutx_policy;
	int ac, vhtmu_cnt = 0, hemmu_cnt = 0;
	//uint8 max_mmu_usrs = wlc_txcfg_max_clients_get(wlc->txcfg, DLMMU);
	BCM_REFERENCE(empty_group);
	BCM_REFERENCE(pos);
	BCM_REFERENCE(g);
	if (!wlc) {
		bcm_bprintf(b, "wlc pointer is NULL on mu_info\n");
		return BCME_OK;
	}

	if (MU_TX_ENAB(wlc) || HE_DLMU_ENAB(wlc->pub)) {
		bcm_bprintf(b, "MU BFR capable and configured; ");
	}

	if (!mu_info->active) {
		return BCME_OK;
	}
	mutx_policy = mu_info->mutx_policy;
	bcm_bprintf(b, "MU feature is %s, AC policy is %s\n"
		"BW policy = %u AC policy = %u\n",
		mu_info->active ? "ON" : "OFF",
		mu_info->ac_policy_on ? "ON" : "OFF",
		mutx_policy->bw_policy,
		mutx_policy->ac_policy);

	bcm_bprintf(b, "Maximum clients %d\n",
		mu_info->max_muclients);
	if (mu_info->muclient_elect_pending)
		bcm_bprintf(b, "MU client election pending\n");

	if (MU_TX_ENAB(wlc) && option == MUTX_MUINFO_OPT_DBG) {
		/* print group membership */
		bcm_bprintf(b, "Static VHTMU GID table:\n");
		for (g = MU_GROUP_ID_MIN; g < MU_GROUP_ID_MAX; g++) {
			/* 1st iteration to tell whether this is an empty group */
			empty_group = TRUE;
			for (i = 0; i < mu_info->max_muclients; i++) {
				scb = mu_info->mu_clients[i];
				if (!scb) {
					continue;
				}
				mu_scb = MUTX_SCB(mu_info, scb);
				if (mu_scb && isset(mu_scb->membership, g)) {
					if (empty_group) {
						bcm_bprintf(b, "    GID %u:", g);
						empty_group = FALSE;
						break;
					}
				}
			}
			/* 2nd iteration to print the client indices in each position */
			for (pos = 0; pos < 4; pos++) {
				if (!empty_group)
					bcm_bprintf(b, " [");
				for (i = 0; i < mu_info->max_muclients; i++) {
					scb = mu_info->mu_clients[i];
					if (scb && SCB_VHTMU(scb)) {
						mu_scb = MUTX_SCB(mu_info, scb);
						if (mu_scb &&
							isset(mu_scb->membership, g)) {
							if (mu_user_pos_get(
								mu_scb->position,
								g) == pos) {
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

	for (i = 0; i < mu_info->max_muclients; i++) {
		if (!(scb = mu_info->mu_clients[i])) {
			continue;
		}
		if (!(mu_scb = MUTX_SCB(mu_info, scb))) {
			continue;
		}
		if (MU_TX_ENAB(wlc) && (mu_scb->tp == VHTMU)) {
			vhtmu_cnt++;
		}
		if (HE_DLMMU_ENAB(wlc->pub) && (mu_scb->tp == HEMMU)) {
			hemmu_cnt++;
		}
	}

	/* Dump current MU client set */
	bcm_bprintf(b, "MU clients: vhtmu %2d hemmu %2d\n",
		vhtmu_cnt, hemmu_cnt);
	for (i = 0; i < mu_info->max_muclients; i++) {
		if (!(scb = mu_info->mu_clients[i])) {
			continue;
		}
		if (!(mu_scb = MUTX_SCB(mu_info, scb))) {
			continue;
		}

		ac = (mu_info->mutx_policy->ac_policy != MUTX_AC_NONE) ?
			mu_info->mutx_policy->ac_policy :
			mu_info->mutx_policy->effective_ac;
		bcm_bprintf(b, "[%u][%s] "MACF" rssi %d nrx %u "
			"ac %d avg_mpdu_sz %4u [%s]\n",
			i, (mu_scb->tp == VHTMU) ? "vhtmu" : "hemmu",
			ETHER_TO_MACF(scb->ea),
			mu_info->mutx_client_scheduler ? mu_scb->rssi :
			wlc_lq_rssi_get(wlc, SCB_BSSCFG(scb), scb),
			mu_scb->nrx, ac, RAVG_AVG(&mu_scb->ravg_info[ac]),
			mutx_reason2str[mu_scb->reason]);

		/* Client's BSS info */
		bcm_bprintf(b, "BSS: wl%u.%u "MACF"\n", wlc->pub->unit,
			WLC_BSSCFG_IDX(scb->bsscfg),
			ETHER_TO_MACF(scb->bsscfg->cur_etheraddr));
#if defined(BCMDBG) || defined(DUMP_MUTX)
		if (option) {
			if (mu_scb->mutx_stats) {
				wlc_mutx_dump_stats(mu_info, mu_scb->mutx_stats,
					((mu_scb->tp == HEMMU) ?  TX_STATUS_MUTP_HEMM :
						TX_STATUS_MUTP_VHTMU), b, option);
				wlc_muclient_clear_stats(mu_info, mu_scb->mutx_stats);
			}
		}
#endif // endif
	}

	if (option) {
		bcm_bprintf(b, "Total mutx: tot_tx %u txasmu %u (%u%%) ncons %u "
			"nlost %u\n", mu_info->tot_cnt, mu_info->mutx_cnt,
			mu_info->tot_cnt ? (mu_info->mutx_cnt * 100 / mu_info->tot_cnt) : 0,
			mu_info->mu_ncons, mu_info->mu_nlost);

		if (MU_TX_ENAB(wlc) && (option == MUTX_MUINFO_OPT_DBG)) {
			bcm_bprintf(b, "\nMU clients on hold_list:\n");
			FOREACH_LIST_POS(hold_pos, next, mu_info->on_hold_list) {
				mutx_scb_t *mutx_scb = (mutx_scb_t *)hold_pos->data;
				bcm_bprintf(b, ""MACF" hold_dur %u(secs)\n",
					ETHER_TO_MACF(mutx_scb->scb->ea),
					(MU_STA_MAX_HOLD_DUR -  mutx_scb->on_hold_dur));
			}
		}
	}
	mu_info->mu_ncons = 0;
	mu_info->tot_cnt = 0;
	mu_info->mutx_cnt = 0;
	mu_info->mu_nlost = 0;

	bcm_bprintf(b, "\nMU capable but not admitted STAs:\n");
	FOREACH_LIST_POS(hold_pos, next, mu_info->mu_cap_stas_list) {
		mu_scb = (mutx_scb_t *)hold_pos->data;
		scb = mu_scb->scb;
		if (mu_scb->client_index != MU_CLIENT_INDEX_NONE) {
			continue;
		}

		ac = (mu_info->mutx_policy->ac_policy != MUTX_AC_NONE) ?
			mu_info->mutx_policy->ac_policy :
			mu_info->mutx_policy->effective_ac;

		bcm_bprintf(b, ""MACF" [%s] rssi %d nrx %u is_dsmps %d "
			"ac %2u avg_mpdu_sz %4u [%s]\n",
			ETHER_TO_MACF(scb->ea),
			(mu_scb->tp == VHTMU) ? "vhtmu" : "hemmu",
			mu_info->mutx_client_scheduler ? mu_scb->rssi :
			wlc_lq_rssi_get(wlc, SCB_BSSCFG(scb), scb),
			mu_scb->nrx, wlc_stf_is_scb_dynamic_smps(wlc, scb),
			ac, RAVG_AVG(&mu_scb->ravg_info[ac]),
			mutx_reason2str[mu_scb->reason]);
	}
	return BCME_OK;
}

#if defined(BCMDBG) || defined(DUMP_MUTX)
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
	scb_t *scb;
	mutx_scb_t *mu_scb;
	int i;

	if (!mu_info->wlc) {
		return BCME_OK;
	}

	for (i = 0; i < mu_info->max_muclients; i++) {
		scb = mu_info->mu_clients[i];
		if (!scb) {
			continue;
		}

		mu_scb = MUTX_SCB(mu_info, scb);
		if (mu_scb && mu_scb->mutx_stats) {
			wlc_muclient_clear_stats(mu_info, mu_scb->mutx_stats);
		}
	}

	mu_info->mu_ncons = 0;
	mu_info->tot_cnt = 0;
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
	uint8 i;
	scb_t *scb;
	uint32 bw, fifo_max_cnt;
	wlc_pub_t *pub = wlc->pub;

	BCM_REFERENCE(mu_info);
	BCM_REFERENCE(ret_int_ptr);
	BCM_REFERENCE(ret_uint_ptr);
	BCM_REFERENCE(set_bool);

	if (plen >= (int)sizeof(int_val))
		memcpy(&int_val, p, sizeof(int_val));

	switch (actionid) {
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
		policy->samebw = mu_info->client_samebw;
		policy->nrx = mu_info->muclient_nrx;
		policy->max_muclients = mu_info->max_muclients;
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
			wlc_mutx_bw_policy_update(mu_info, FALSE);
		}
		/* pfmon */
		if (policy->pfmon) {
			mu_info->muclient_pfmon = TRUE;
		} else {
			mu_info->muclient_pfmon = FALSE;
		}

		/* samebw */
		if (policy->samebw) {
			mu_info->client_samebw = TRUE;
		} else {
			mu_info->client_samebw = FALSE;
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
		fifo_max_cnt = wlc_fifo_max_per_ac(wlc->fifo, WL_MU_DLMMU);
		/* max_muclients */
		if (policy->max_muclients > fifo_max_cnt) {
			err = BCME_RANGE;
			break;
		}
		if (pub->up) {
			WL_ERROR(("wl%d: max_muclients can't be set with interface up\n",
				pub->unit));
			err = BCME_NOTDOWN;
			break;
		}

		err = wlc_txcfg_max_clients_set(wlc->txcfg, DLMMU, policy->max_muclients);
		if (!err) {
			mu_info->max_muclients = (uint8)policy->max_muclients;
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
			W_REG(wlc->osh, D11_PHY_REG_ADDR(wlc), (uint16)PHYREG_M2V_MSG0);
			W_REG(wlc->osh, D11_PHY_REG_DATA(wlc), (uint16)0x8006);
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
		if (D11REV_LT(pub->corerev, 129) ||
			(int_val & MU_FEATURES_AUTO)) {
			err = BCME_UNSUPPORTED;
			break;
		}

		/* BCM6705 (same chipid as 43692) and 43692 do not support MU */
		if ((CHIPID(si_chipid(wlc->pub->sih)) == BCM43692_CHIP_ID) &&
			(int_val & MU_FEATURES_MUTX)) {
			err = BCME_UNSUPPORTED;
			break;
		}

		/* Only MUTX is supported */
		if ((pub->mu_features & MU_FEATURES_MUTX) ==
			((uint32)int_val & MU_FEATURES_MUTX)) {
			break;
		}
		if (((uint32)int_val & MU_FEATURES_MUTX) != 0) {
			pub->mu_features |=
			WLC_MU_BFR_CAP_PHY(wlc) ? MU_FEATURES_MUTX : 0;
		} else {
			pub->mu_features &= ~MU_FEATURES_MUTX;
		}
		break;
	case IOV_SVAL(IOV_MUPFMON_MUMCS_THRESH):
		if (!(int_val >= 0 && int_val < MUTX_MCS_NUM)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		mu_info->mupfmon_mumcs_thresh = int_val;
		if (!wlc->pub->up) {
			break;
		}
		if (!mu_info->active) {
			break;
		}
		for (i = 0; i < mu_info->max_muclients; i++) {
			scb = mu_info->mu_clients[i];
			if (scb == NULL) {
				continue;
			}
			wlc_muclient_clear_pfmon_stats(mu_info, scb);
		}
		break;
	case IOV_GVAL(IOV_MUPFMON_MUMCS_THRESH):
		*ret_uint_ptr = (uint8)mu_info->mupfmon_mumcs_thresh;
		break;

	case IOV_GVAL(IOV_MUTX_MPDUSZ_MU_ADMIT_THRESH): {
		wl_mutx_mpdusz_admit_thresh_t *admit_thresh = (wl_mutx_mpdusz_admit_thresh_t *)a;
		if (int_val < 1 || int_val > 4) {
			err = BCME_BADARG;
			break;
		}
		bw = int_val;
		admit_thresh->mpdusz = mu_info->mpdusz_mu_admit_thresh[bw];
		break;
	}

	case IOV_SVAL(IOV_MUTX_MPDUSZ_MU_ADMIT_THRESH): {
		wl_mutx_mpdusz_admit_thresh_t *admit_thresh = (wl_mutx_mpdusz_admit_thresh_t *)a;
		if (int_val < 1 || int_val > 4) {
			err = BCME_BADARG;
			break;
		}
		bw = int_val;
		mu_info->mpdusz_mu_admit_thresh[bw] = admit_thresh->mpdusz;
		break;
	}

	case IOV_GVAL(IOV_MUTX_MPDUSZ_DLOFDMA_ADMIT_THRESH): {
		wl_mutx_mpdusz_admit_thresh_t *admit_thresh = (wl_mutx_mpdusz_admit_thresh_t *)a;
		if (int_val < 1 || int_val > 4) {
			err = BCME_BADARG;
			break;
		}
		bw = int_val;
		admit_thresh->mpdusz = mu_info->mpdusz_dlofdma_admit_thresh[bw];
		break;
	}

	case IOV_SVAL(IOV_MUTX_MPDUSZ_DLOFDMA_ADMIT_THRESH): {
		wl_mutx_mpdusz_admit_thresh_t *admit_thresh = (wl_mutx_mpdusz_admit_thresh_t *)a;
		if (int_val < 1 || int_val > 4) {
			err = BCME_BADARG;
			break;
		}
		bw = int_val;
		mu_info->mpdusz_dlofdma_admit_thresh[bw] = admit_thresh->mpdusz;
		break;
	}
	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

/* Update and return the BW policy */
uint32
wlc_mutx_bw_policy_update(wlc_mutx_info_t *mu_info, bool force)
{
	wlc_info_t *wlc = mu_info->wlc;
	scb_t *scb;
	mutx_scb_t *mu_scb;
	int i;
	uint32 num_stas_bw[BW_160MHZ+1];
	uint32 bw, max_bw;
	uint32 metric = 0, tmp;
	uint32 bw_policy_old;
	mutx_scb_list_t *pos, *next;
	mutx_policy_t *mutx_policy;

	mutx_policy = mu_info->mutx_policy;
	bw_policy_old = mutx_policy->bw_policy;
	if (!force &&
		(mutx_policy->ac_policy != 0)) {
		return 0;
	}
	/* Init the counters */
	for (bw = 0; bw <= BW_160MHZ; bw++) {
		num_stas_bw[bw] = 0;
	}

	/* Iterate and count */
	FOREACH_LIST_POS(pos, next, mu_info->mu_cap_stas_list) {
		mu_scb = (mutx_scb_t *)pos->data;
		if ((mu_scb->tp == HEMMU) &&
			!mu_info->_mu_tx[TP_TO_IDX(HEMMU)]) {
			continue;
		}
		scb = mu_scb->scb;
		if (!wlc_mutx_sta_nrx_check(mu_info, scb)) {
			continue;
		}
		bw = wlc_scb_ratesel_get_link_bw(wlc, scb);
		if ((!mu_info->client_bw160) && (bw == BW_160MHZ)) {
			continue;
		} else if ((mu_info->client_bw160) && (bw == BW_160MHZ) &&
			(mu_scb->tp != HEMMU)) {
			continue;
		}
		if (bw >= BW_20MHZ && bw <= BW_160MHZ)
			num_stas_bw[bw]++;
	}

#define BW_FACTOR 20 /* BW*20 */
	/* Decide the BW policy based on the counters */
	max_bw = mu_info->client_bw160 ? BW_160MHZ : BW_80MHZ;
	for (i = BW_20MHZ; i <= max_bw; i++) {
		if (num_stas_bw[i] > 1) {
			tmp = num_stas_bw[i] * (BW_FACTOR << (i - 1));
			if (tmp >= metric) {
				metric = tmp;
				bw = i;
			}
		}
	}
	mutx_policy->bw_policy = bw;

	if (force) {
		/* We just want to get the default bw, do not trigger any policy change */
		return (bw);
	}
	if (mu_info->active && (bw != bw_policy_old)) {
		/* Evict policy mismatch client */
		for (i = 0; i < mu_info->max_muclients; i++) {
			scb = mu_info->mu_clients[i];
			if (scb) {
				mu_scb = MUTX_SCB(mu_info, scb);
				if (wlc_scb_ratesel_get_link_bw(wlc, scb) != bw)
					mu_scb->policy_evict = MUTX_POLICY_EVICT_BW;
			}
		}
		if (bw) {
			/* More than two MU capable STAs have the same BW */
			WL_MUMIMO(("wl%d: Updating BW admission policy to %sMHz\n",
				mu_info->pub->unit,
				(bw == BW_20MHZ) ? "20":
				((bw == BW_40MHZ) ? "40":
				((bw == BW_80MHZ) ? "80" : "160"))));
		} else {
			WL_MUMIMO(("wl%d: No common BW MU STAs\n",
				mu_info->pub->unit));
		}
		mu_info->muclient_elect_pending = TRUE;
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
wlc_mutx_client_scheduler(wlc_mutx_info_t *mu_info, bool sched)
{
	int vhtmu_cand_cnt, hemmu_cand_cnt, dlofdma_cand_cnt;
	bool evictees = FALSE;

	vhtmu_cand_cnt = hemmu_cand_cnt = dlofdma_cand_cnt = 0;
	mu_candidates_select(mu_info, &vhtmu_cand_cnt, &hemmu_cand_cnt,
		&dlofdma_cand_cnt, sched);
	if (sched || vhtmu_cand_cnt || hemmu_cand_cnt ||
		dlofdma_cand_cnt) {
		mu_clients_eviction(mu_info, sched);
		if (((mu_info->active) &&
			!LIST_IS_EMPTY(mu_info->admit_list)) ||
			dlofdma_cand_cnt) {
			wlc_mutx_admit_clients(mu_info);
		}
	}
	if (sched) {
		mu_info->mutx_client_scheduler_cnt = 0;
	}
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
#define MUTX_PKTRATE_THRESH_LOW		50	/* low threshold for admission */
#define MUTX_PKTRATE_ADMIT_THRESH	MUTX_PKTRATE_THRESH_LOW	/* threshold for admission */
#define MUTX_PKTRATE_EVICT_THRESH	MUTX_PKTRATE_THRESH_LOW	/* threshold for eviction */
#define MUTX_MPDUSZ_BW160_MU_ADMIT_THRESH 2800
#define MUTX_MPDUSZ_BW80_MU_ADMIT_THRESH  1400
#define MUTX_MPDUSZ_BW40_MU_ADMIT_THRESH  700
#define MUTX_MPDUSZ_BW20_MU_ADMIT_THRESH  350
#ifdef NOT_YET
#define MUTX_MPDUSZ_DLOFDMA_ADMIT_THRESH 2560
#else
/* Disable DLOFDMA-SU switch across all BW by default */
#define MUTX_MPDUSZ_DLOFDMA_ADMIT_THRESH 0x7FFF
#endif /* NOT YET */
#define MUTX_OFDMA_MCS_THRESH 2
#define MUTX_SU_MCS_THRESH 8
#define MUTX_HEMMU_MUCIDX_BASE		8
static void
wlc_mutx_ac_update(wlc_mutx_info_t *mu_info)
{
	int idx;
	wlc_info_t *wlc = mu_info->wlc;
	mutx_scb_t *mu_scb;
	scb_t *scb;
	int prio, ac, bw, tp_idx;
	ampdu_tx_scb_stats_t ampdu_scb_stats;
	uint32 ac_policy, ac_policy_old;
	uint32 bw_policy, bw_policy_old;
	uint32 ac_policy_scores[BW_160MHZ+1][MUTX_AC_VO+1];
	uint32 num_stas_ac[MUTX_TP_MAX][BW_160MHZ+1][MUTX_AC_VO+1];
	uint32 score, pkts_num;
	mutx_scb_list_t *pos, *next;
	mutx_policy_t *mutx_policy;
	ratespec_t rspec;
	uint32 mcs, avg_mpdu_len;
	musta_heurs_t *musta_heurs;
	bool is_mu_cap;

	if (!mu_info->active) {
		return;
	}

	for (tp_idx = 0; tp_idx < MUTX_TP_MAX; tp_idx++) {
		for (bw = 0; (mu_info->active) && (bw <= BW_160MHZ); bw++) {
			for (ac = MUTX_AC_NONE; ac <= MUTX_AC_VO; ac++) {
				ac_policy_scores[bw][ac] = 0;
			}
			for (ac = MUTX_AC_NONE; ac <= MUTX_AC_VO; ac++) {
				num_stas_ac[tp_idx][bw][ac] = 0;
			}
		}
	}

	FOREACH_LIST_POS(pos, next, mu_info->mu_cap_stas_list) {
		mu_scb = (mutx_scb_t *)pos->data;

		if ((mu_scb->tp == HEMMU) &&
			!mu_info->_mu_tx[TP_TO_IDX(HEMMU)]) {
			continue;
		}
		scb = mu_scb->scb;
		musta_heurs = &mu_scb->musta_heurs;
		is_mu_cap = TRUE;
		if (!wlc_mutx_sta_nrx_check(mu_info, scb)) {
			ASSERT(!mu_in_client_set(mu_info, scb));
			is_mu_cap = FALSE;
		}

		if (mu_info->muclient_pfmon && is_mu_cap &&
			!mu_in_client_set(mu_info, scb)) {
			wlc_musta_upd_heurs(mu_info, scb, HEURS_SU_DEC);
			if (musta_heurs->counter0 > 0) {
				is_mu_cap = FALSE;
			}
			rspec = wlc_scb_ratesel_get_primary(wlc, scb, NULL);
			mcs = wlc_ratespec_mcs(rspec);
			if (is_mu_cap && (mcs < mu_info->mutx_admit_mcs_thresh)) {
				mu_scb->score = 0;
				bzero(&mu_scb->ac_policy_scores[0],
					sizeof(mu_scb->ac_policy_scores));
				is_mu_cap = FALSE;
			}
		}

		if (wlc_ampdu_scbstats_get_and_clr(wlc, scb, &ampdu_scb_stats) == FALSE) {
			continue;
		}
		bw = (int)wlc_scb_ratesel_get_link_bw(wlc, scb);
		if ((!mu_info->client_bw160) && (bw == BW_160MHZ)) {
			is_mu_cap = FALSE;
		} else if ((mu_info->client_bw160) && (bw == BW_160MHZ) &&
			(mu_scb->tp != HEMMU)) {
			is_mu_cap = FALSE;
		}
		mu_scb->rssi = wlc_lq_rssi_get(wlc, SCB_BSSCFG(scb), scb);
		for (prio = 0; prio < NUMPRIO; prio++) {
			pkts_num = ampdu_scb_stats.tx_pkts[prio];

			if (pkts_num == 0)
				continue;

			avg_mpdu_len = ampdu_scb_stats.tx_bytes[prio] / pkts_num;
			if (pkts_num < MUTX_PKTRATE_ADMIT_THRESH)
				continue;

			WL_MUTX((MACF" prio=%d rssi=%d nrx=%u tx_pkts %u tx_bytes %u "
				"pkts_sent %u, bytes_sent %u avg_mpdu_len %u\n",
				ETHER_TO_MACF(scb->ea), prio,
				mu_scb->rssi, mu_scb->nrx,
				ampdu_scb_stats.tx_pkts[prio],
				ampdu_scb_stats.tx_bytes[prio],
				ampdu_scb_stats.tx_pkts_total[prio],
				ampdu_scb_stats.tx_bytes_total[prio],
				avg_mpdu_len));

			ac = prio2mufifo[prio];
			RAVG_ADD(&mu_scb->ravg_info[ac], avg_mpdu_len);
			score = pkts_num/100;
			mu_scb->ac_policy_scores[ac] += score;
			if (is_mu_cap &&
				(pkts_num >= MUTX_PKTRATE_ADMIT_THRESH) &&
				(!mu_info->muclient_pfmon || (mu_scb->tp == VHTMU) ||
				((mu_scb->tp == HEMMU) &&
				(RAVG_AVG(&mu_scb->ravg_info[ac]) >
				mu_info->mpdusz_mu_admit_thresh[bw])))) {
				num_stas_ac[TP_TO_IDX(mu_scb->tp)][bw][ac]++;
				ac_policy_scores[bw][ac] += score;
			}
		}
	}
	score = 0;
	mutx_policy = mu_info->mutx_policy;
	bw_policy_old = mutx_policy->bw_policy;
	ac_policy_old = mutx_policy->effective_ac;
	bw_policy = bw_policy_old;
	ac_policy = MUTX_AC_NONE;
	bw = mu_info->client_bw160 ? BW_160MHZ : BW_80MHZ;
	for (; (mu_info->active) && (bw >= BW_20MHZ); bw--) {
		for (ac = MUTX_AC_BEBK; ac <= MUTX_AC_VO; ac++) {
			WL_MUTX(("wl%d: ac_policy_score[%d][%d] = %u\n",
				mu_info->pub->unit, bw, ac,
				ac_policy_scores[bw][ac]));
			if (((num_stas_ac[TP_TO_IDX(VHTMU)][bw][ac] > 1) ||
				(num_stas_ac[TP_TO_IDX(HEMMU)][bw][ac] > 1)) &&
				ac_policy_scores[bw][ac] > score) {
				bw_policy = bw;
				ac_policy = ac;
				score = ac_policy_scores[bw][ac];
			}
		}
	}

	WL_MUTX(("wl%d: bw_policy=%u ac_policy=%u score=%u\n",
		mu_info->pub->unit, bw_policy, ac_policy, score));
	mutx_policy->bw_policy = bw_policy;
	mutx_policy->ac_policy = ac_policy;

	if (ac_policy) {
		/* To avoid unnecessary re-admissions */
		if (ac_policy_old == MUTX_AC_NONE)
			ac_policy_old = ac_policy;
		mutx_policy->effective_ac = ac_policy;
	}
	if ((mu_info->active) && ((bw_policy != bw_policy_old) ||
		(mutx_policy->effective_ac != ac_policy_old))) {
		bw = mu_info->client_bw160 ? BW_160MHZ : BW_80MHZ;
		for (; bw >= BW_20MHZ; bw--) {
			for (ac = MUTX_AC_BEBK; ac <= MUTX_AC_VO; ac++) {
				if (ac_policy_scores[bw][ac]) {
					WL_MUTX(("wl%d: ac_policy_score[%d][%d] = "
						"%u\n", mu_info->pub->unit, bw, ac,
						ac_policy_scores[bw][ac]));
				}
			}
		}
		WL_ERROR(("wl%d: Updating => bw_policy=%u ac_policy=%u\n",
			mu_info->pub->unit, bw_policy, ac_policy));
		if (mu_info->active) {
			/* Evict policy mismatch client */
			for (idx = 0; idx < mu_info->max_muclients; idx++) {
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
						bw_policy)) {
					mu_scb->policy_evict = MUTX_POLICY_EVICT_AC;
				}
			}
				mu_info->muclient_elect_pending = TRUE;
		}
	}
	if ((mu_info->active) &&
		((mutx_policy->bw_policy == 0) || (mutx_policy->ac_policy == 0))) {
		/* No suitable BW/AC combination, pick up the default bw */
		wlc_mutx_bw_policy_update(mu_info, TRUE);
	}

	if (mu_info->muclient_pfmon) {
		wlc_muclient_pfmon(mu_info);
	}
	return;
}

static void
wlc_ac_policy_scores_update(wlc_mutx_info_t *mu_info)
{
	mutx_scb_t *mu_scb;
	uint32 ac;
	mutx_scb_list_t *pos, *next;
	mutx_policy_t *mutx_policy;
	wlc_info_t *wlc = mu_info->wlc;
	BCM_REFERENCE(wlc);

	if (!mu_info->active ||
		!HE_DLMU_ENAB(wlc->pub)) {
		return;
	}

	mutx_policy = mu_info->mutx_policy;
	ac = mutx_policy->ac_policy;
	FOREACH_LIST_POS(pos, next, mu_info->mu_cap_stas_list) {
		mu_scb = (mutx_scb_t *)pos->data;
		if (ac != MUTX_AC_NONE) {
			mu_scb->score =
				mu_scb->ac_policy_scores[ac];
		} else {
			mu_scb->score = 0;
		}
		mu_scb->time_score++;
	}
}

static void
wlc_ac_policy_scores_reset(wlc_mutx_info_t *mu_info)
{
	mutx_scb_t *mu_scb;
	mutx_scb_list_t *pos, *next;
	wlc_info_t *wlc = mu_info->wlc;
	BCM_REFERENCE(wlc);

	/* Reset ac_policy_scores after scheduler. */
	if (!mu_info->active ||
		!HE_DLMU_ENAB(wlc->pub)) {
		return;
	}

	FOREACH_LIST_POS(pos, next, mu_info->mu_cap_stas_list) {
		mu_scb = (mutx_scb_t *)pos->data;
		bzero(&mu_scb->ac_policy_scores, sizeof(mu_scb->ac_policy_scores));
		RAVG_INIT(&mu_scb->ravg_info[0], 1024, 2);
	}
}

/* retry GID in watchdog reason codes */
#define MUTX_R_GID_DISABLED		0	/* retry GID disabled in watchdog */
#define MUTX_R_GID_NO_BUFFER		1	/* retry GID due to out of buffer */
#define MUTX_R_GID_CALLBACK_FAIL	2	/* retry GID due to callback register fail */
#define MUTX_R_GID_RETRY_CNT		3	/* retry GID due to retry cnt */
#define MUTX_R_GID_BAD_USR_POS		4	/* retry GID due to bad usr pos */
#define MUTX_R_GID_RETRY_WAR		5	/* retry GID due to retry war */

#define MUTX_STATS_MIN_NUPD 3
#define MUTXCNT_SCORE_THRESH 66
#define MUTXCNT_SCORE_NBIN 2

#if defined(BCMDBG) || defined(DUMP_MUTX)
static void
wlc_muclient_clear_stats(wlc_mutx_info_t *mu_info, mutx_stats_t *mutx_stats)
{
	memset(mutx_stats, 0, sizeof(mutx_stats_t));
}
#endif // endif

static void
wlc_muclient_clear_pfmon_stats(wlc_mutx_info_t *mu_info, scb_t *scb)
{
	mutx_scb_t *mu_scb;
	mutx_pfmon_stats_t *mutx_pfmon_stats;
	musta_heurs_t *musta_heurs;
	mu_scb = MUTX_SCB(mu_info, scb);

	WL_MUTX(("%s+\n", __FUNCTION__));
	if (mu_scb == NULL)
		return;

	mutx_pfmon_stats = mu_scb->mutx_pfmon_stats;

	if (mutx_pfmon_stats == NULL)
		return;

	musta_heurs = &mu_scb->musta_heurs;
	memset(mutx_pfmon_stats, 0, sizeof(mutx_pfmon_stats_t));
	mu_scb->txstats_nupd = 0;
	musta_heurs->counter3 = 0;
}

#define MUTX_PRIMRATE_SUCC_THRESH 66
static bool
wlc_muclients_pfmon_eval_primrate_succ(wlc_mutx_info_t *mu_info, scb_t *scb)
{
	mutx_scb_t *mu_scb;
	mutx_pfmon_stats_t *pfmon_stats;
	musta_heurs_t *musta_heurs;
	bool evict = FALSE;
	uint32 txmu_tot, txmu_succ, txmu_score;
	uint8 i, last;

	mu_scb = MUTX_SCB(mu_info, scb);
	musta_heurs = &mu_scb->musta_heurs;
	BCM_REFERENCE(musta_heurs);
	pfmon_stats = mu_scb->mutx_pfmon_stats;

	txmu_tot = pfmon_stats->mutxcnt_stat[0];
	if (txmu_tot) {
		/* #txmu at primary rate */
		txmu_succ = pfmon_stats->mutxcnt_succ[0];
		txmu_score = (txmu_succ * 100) / txmu_tot;
		/* evict client? */
		evict = (txmu_score < MUTX_PRIMRATE_SUCC_THRESH) ? TRUE: FALSE;
		WL_MUTX(("evict_client(%s):pfmon_primrate_succ: "MACF""
			" nsample %u\ntxmu_tot %u txmu_succ %u(%u%%) %s thresh(66%%)\n",
			(evict ? "true" : "false"), ETHER_TO_MACF(scb->ea),
			musta_heurs->counter3, txmu_tot, txmu_succ, txmu_score,
			(evict ? "<" : ">")));

		if (evict) {
			mu_scb->policy_evict = MUTX_POLICY_EVICT_PFMON;
			mu_scb->reason = MUTX_REASON_PFMON_PRIMRATE_SUCC_EVICT;
			/* determines highest*index* on which a transmit took place */
			for (i = 0, last = 0; i < MUTX_MAXMUTXCNT; i++) {
				if (pfmon_stats->mutxcnt_succ[i]) {
					last = i;
				}
			}
			WL_MUTX(("pfmon_stats mutxcnt_tot:  "));
			for (i = 0; i <= last; i++) {
				WL_MUTX(("%u      ", pfmon_stats->mutxcnt_stat[i]));
				if ((i % MUTX_MAXMUTXCNT) == 5 && i != last) {
					WL_ERROR(("\n        :"));
				}
			}
			WL_MUTX(("\n"));
			WL_MUTX(("pfmon_stats mutxcnt_succ: "));
			for (i = 0; i <= last; i++) {
				WL_MUTX(("%u(%u%%) ", pfmon_stats->mutxcnt_succ[i],
				(pfmon_stats->mutxcnt_succ[i] * 100)/pfmon_stats->mutxcnt_stat[i]));
				if ((i % MUTX_MAXMUTXCNT) == 5 && i != last) {
					WL_MUTX(("\n        :"));
				}
			}
			WL_MUTX(("\n"));
		}
	}
	return evict;
}

void
wlc_mutx_sounding_period_upd(wlc_mutx_info_t *mu_info, uint16 period)
{
	if (!mu_info) {
		return;
	}
	mu_info->mu_snd_per_sec = (period ? (1000 / period) : 0);
	WL_MUTX(("%s mu_snd_per_sec %u period %u\n", __FUNCTION__,
	mu_info->mu_snd_per_sec, period));
}

#define MUTX_TXASMU_THRESH 30
#define MUTX_TXASSU_THRESH 30

static char* txassu_reason2str[C_M2SQ_REASON_MAX] = {
	"none",
	"vasip",
	"snd_fail",
	"retry_lmt"
};

static bool
wlc_muclients_pfmon_eval_tx_as_mu(wlc_mutx_info_t *mu_info, scb_t *scb)
{
	mutx_scb_t *mu_scb;
	mutx_pfmon_stats_t *pfmon_stats;
	musta_heurs_t *musta_heurs;
	uint32 tot_tx = 0, tot_txmu = 0, tot_txsu = 0, txmu_score;
	bool evict = FALSE;
	int i, reason = 0;
	uint32 txassu_reason = 0;
	BCM_REFERENCE(reason);
	BCM_REFERENCE(txassu_reason2str);
	mu_scb = MUTX_SCB(mu_info, scb);
	musta_heurs = &mu_scb->musta_heurs;
	BCM_REFERENCE(musta_heurs);
	pfmon_stats = mu_scb->mutx_pfmon_stats;
	for (i = 0; i < MUTX_MCS_INDEX_NUM; i++) {
		tot_txmu += pfmon_stats->txmu[i];
		tot_txsu += pfmon_stats->txsu[i];
	}
	tot_tx = tot_txmu + tot_txsu;
	if (tot_tx) {
		txmu_score = (tot_txmu * 100) / tot_tx;
		evict = (txmu_score < MUTX_TXASMU_THRESH) ? TRUE: FALSE;
		WL_MUTX(("evict_client(%s):pfmon_txasmu: "MACF""
			" nsample %u\n tot_tx %u txasmu %u tot_txsu %u"
			" score(%u%%) %s thresh(%u%%)\n", (evict ? "true" : "false"),
			ETHER_TO_MACF(scb->ea), musta_heurs->counter3, tot_tx,
			tot_txmu, tot_txsu, txmu_score, (evict ? "<" : ">"),
			MUTX_TXASMU_THRESH));
		if (!evict) {
			return FALSE;
		}
		evict = FALSE;
		for (i = C_M2SQ_NOT_IN_ANYGRP; i < C_M2SQ_REASON_MAX; i++) {
				txassu_reason += pfmon_stats->txassu_reason[i];
		}
		if (txassu_reason) {
			/* evict client if snd_fail_cnt or
			 * txassu due to not in any group > 50%
			 */
			if (pfmon_stats->snd_fail_cnt >
				(mu_info->mu_snd_per_sec * musta_heurs->counter3) / 2) {
				evict = TRUE;
				reason = C_M2SQ_SOUNDNG_FAIL;
			} else if (pfmon_stats->txassu_reason[C_M2SQ_NOT_IN_ANYGRP] &&
				(pfmon_stats->txassu_reason[C_M2SQ_NOT_IN_ANYGRP] >
				(txassu_reason / 2))) {
				evict = TRUE;
				reason = C_M2SQ_NOT_IN_ANYGRP;
			}
			WL_MUTX(("evict_client(%s):pfmon_txassu_%s: "MACF""
				" nsample %u\n reason: %u vasip %u snd_fail %u"
				" retry_lmt %u snd_fail_cnt %u\n",
				(evict ? "true" : "false"), txassu_reason2str[reason],
				ETHER_TO_MACF(scb->ea), musta_heurs->counter3, txassu_reason,
				pfmon_stats->txassu_reason[C_M2SQ_NOT_IN_ANYGRP],
				pfmon_stats->txassu_reason[C_M2SQ_SOUNDNG_FAIL],
				pfmon_stats->txassu_reason[C_M2SQ_RETRY_LMT_RCHD],
				pfmon_stats->snd_fail_cnt));
			if (evict) {
				mu_scb->policy_evict = MUTX_POLICY_EVICT_PFMON;
				mu_scb->reason = MUTX_REASON_PFMON_TXSASMU_EVICT;
			}
		}
	}
	return evict;
}

#define MUTX_PRIMRATE_MUMCS_THRESH 4
/* Threshold for 2x2 chip(AP) */
#define MUTX_PRIMRATE_MUMCS_THRESH_2X2 6

static bool
wlc_muclients_pfmon_eval_primrate_mcs(wlc_mutx_info_t *mu_info, scb_t *scb)
{
	mutx_scb_t *mu_scb;
	mutx_pfmon_stats_t *pfmon_stats;
	musta_heurs_t *musta_heurs;
	uint32 txmu_tot = 0, txmu_cnt = 0;
	bool evict = FALSE;
	int i, last, primary_mcs = -1;

	mu_scb = MUTX_SCB(mu_info, scb);
	musta_heurs = &mu_scb->musta_heurs;
	BCM_REFERENCE(musta_heurs);
	pfmon_stats = mu_scb->mutx_pfmon_stats;

	/* determines highest*index* on which a transmit took place */
	for (i = 0, last = 0; i < MUTX_MCS_INDEX_NUM; i++) {
		if (pfmon_stats->txmu_primrate[i]) {
			last = i;
		}
	}
	for (i = 0; i <= last; i++) {
		txmu_tot += pfmon_stats->txmu_primrate[i];
		if (txmu_cnt < pfmon_stats->txmu_primrate[i]) {
			txmu_cnt = pfmon_stats->txmu_primrate[i];
			primary_mcs = i % MUTX_MCS_NUM;
		}
	}

	if (txmu_tot) {
		evict = (primary_mcs < mu_info->mupfmon_mumcs_thresh) ? TRUE: FALSE;
		WL_MUTX(("evict_client(%s):pfmon_primrate_mumcs: "MACF""
			" nsample %u\n primary_mumcs((%d)(%u%%)) %s thresh(%u)\n"
			"txmu_tot %u txmu_cnt %u\n", (evict ? "true" : "false"),
			ETHER_TO_MACF(scb->ea), musta_heurs->counter3, primary_mcs,
			(txmu_cnt * 100)/txmu_tot, (evict ? "<" : ">="),
			mu_info->mupfmon_mumcs_thresh, txmu_tot, txmu_cnt));
		if (evict) {
			mu_scb->policy_evict = MUTX_POLICY_EVICT_PFMON;
			mu_scb->reason = MUTX_REASON_PFMON_PRIMRATE_MUMCS_EVICT;
		}
	}
	return evict;
}

static void
wlc_muclient_pfmon(wlc_mutx_info_t *mu_info)
{
	scb_t *scb;
	mutx_scb_t *mu_scb;
	musta_heurs_t *musta_heurs;
	bool evict = FALSE;
	int i, evict_cnt = 0, ac;
	uint link_bw;

	if (!mu_info->active) {
		return;
	}

	ac = mu_info->mutx_policy->ac_policy;
	for (i = 0; i < mu_info->max_muclients; i++) {
		scb = mu_info->mu_clients[i];
		if (scb == NULL) {
			continue;
		}
		evict = FALSE;
		mu_scb = MUTX_SCB(mu_info, scb);
		musta_heurs = &mu_scb->musta_heurs;
		if ((ac == MUTX_AC_NONE) ||
			(ac != MUTX_AC_NONE &&
				mu_scb->ac_policy_scores[ac] == MUTX_AC_NONE)) {
			wlc_muclient_clear_pfmon_stats(mu_info, scb);
		}

		if (musta_heurs->counter3 >= MUTX_STATS_MIN_NUPD) {
			evict = wlc_muclients_pfmon_eval_primrate_mcs(mu_info, scb);
			if (!evict) {
				evict = wlc_muclients_pfmon_eval_primrate_succ(mu_info, scb);
			}
			if (!evict) {
				wlc_muclients_pfmon_eval_tx_as_mu(mu_info, scb);
			}
			wlc_muclient_clear_pfmon_stats(mu_info, scb);
		}

		if (!evict && (mu_scb->tp == HEMMU)) {
			link_bw = wlc_scb_ratesel_get_link_bw(mu_info->wlc, scb);
			if (ac == MUTX_AC_NONE) {
				ac = mu_info->mutx_policy->effective_ac;
			}
			if ((ac != MUTX_AC_NONE) &&
				(mu_scb->ac_policy_scores[ac] != MUTX_AC_NONE) &&
				RAVG_AVG(&mu_scb->ravg_info[ac]) <=
				mu_info->mpdusz_mu_admit_thresh[link_bw]) {
				mu_scb->policy_evict = MUTX_POLICY_EVICT_PFMON;
				mu_scb->reason = MUTX_REASON_PFMON_AVG_MPDU_SZ_EVICT;
				WL_MUTX(("evict_client(true):pfmon_avg_mpdu_sz[%d]=%u"
					" < threshold(%u)\n", ac,
					RAVG_AVG(&mu_scb->ravg_info[ac]),
					mu_info->mpdusz_mu_admit_thresh[link_bw]));
				evict = TRUE;
			}
		}
		if (evict) {
			evict_cnt++;
		}
	}
	if (evict_cnt) {
		mu_info->muclient_elect_pending = TRUE;
	}
	return;
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

	if ((!(mu_info->_mu_tx[TP_TO_IDX(VHTMU)] ||
		mu_info->_mu_tx[TP_TO_IDX(HEMMU)] ||
		HE_DLMU_ENAB(wlc->pub))) ||
		(wlc->txfifo_detach_pending) ||
		(wlc->excursion_active) ||
	    SCAN_IN_PROGRESS(wlc->scan) ||
		wlc->block_datafifo) {
		/* Defer mutx watchdog until scan completes. */
		WL_MUMIMO(("wl%d: MUTX wd blocked: fifodetach: %d, excur: %d scan: %d\n",
			wlc->pub->unit, wlc->txfifo_detach_pending, wlc->excursion_active,
			SCAN_IN_PROGRESS(wlc->scan)));
		return;
	}

	wlc_mutx_ac_update(mu_info);

	FOREACH_LIST_POS(pos, next, mu_info->on_hold_list) {
		mu_scb = (mutx_scb_t *)pos->data;
		if (--mu_scb->on_hold_dur) {
			continue;
		}
		mutx_scb_list_move(mu_info->mu_cap_stas_list, pos);
		/* If any STA is removed from on_hold_list,
		 * we should trigger muclient_elect_pending to selecting new set.
		 */
		mu_info->muclient_elect_pending = TRUE;
	}

	/* Check if we need to do MU client selection */
	if (mu_info->active &&
		mu_info->muclient_elect_pending) {
		mu_clients_select(mu_info);
		return;
	}

	/* Check if we need to do MU Client scheduler */
	if (mu_info->mutx_client_scheduler) {
		mu_info->mutx_client_scheduler_cnt++;
		if (mu_info->mutx_client_scheduler_cnt >= mu_info->mutx_client_scheduler_dur) {
			wlc_ac_policy_scores_update(mu_info);
			(void)wlc_mutx_client_scheduler(mu_info, TRUE);
			wlc_ac_policy_scores_reset(mu_info);
			if (wlc->block_datafifo) {
				return;
			}
		} else if (mu_info->muclient_pfmon) {
				(void)wlc_mutx_client_scheduler(mu_info, FALSE);
				if (wlc->block_datafifo) {
					return;
				}
			}
		}

	/* Check if we need to send a Group ID management message to a vht mu client */
	for (idx = 0; idx < mu_info->max_muclients; idx++) {
		if (mu_info->mu_clients[idx] == NULL) {
			continue;
		}

		mu_scb = MUTX_SCB(mu_info, mu_info->mu_clients[idx]);
		if (!mu_scb || (mu_scb->tp != VHTMU) ||
			(!mu_scb->membership_change && !mu_scb->gid_retry_war)) {
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
			wlc_fifo_sta_bitmap(wlc->fifo, mu_scb->scb, depart_fifo_bmp);
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
			mutx_scb_list_move(mu_info->on_hold_list, mu_scb->item);
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

	/* Check if system is VHTMU or HEMMU BFR capable, both physically and by current
	 * configuration, and if so, set _mu_tx. Configuration can only change
	 * while radio is down. So checking at up time is sufficient.
	 */
	wlc_mutx_update(wlc);
	mu_info->max_muclients = wlc_txcfg_max_clients_get(wlc->txcfg, DLMMU);
	WL_MUMIMO(("%s max_muclients %d\n", __FUNCTION__, mu_info->max_muclients));
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

static uint
mu_scb_cubby_secsz(void *ctx, scb_t *scb)
{
	uint size = 0;
	if (scb && !SCB_INTERNAL(scb)) {
		size = ALIGN_SIZE(sizeof(mutx_scb_t), sizeof(uint32));
#if defined(BCMDBG) || defined(DUMP_MUTX)
		size += ALIGN_SIZE(sizeof(mutx_stats_t), sizeof(uint32));
#endif // endif
#ifdef WLCNT
		size += ALIGN_SIZE(sizeof(mutx_pfmon_stats_t), sizeof(uint32)) +
			ALIGN_SIZE(sizeof(mutx_snr_calib_stats_t), sizeof(uint32));
#endif	/* WLCNT */
	}
	return size;
}

/* Initialize this module's scb state. */
static int
mu_scb_cubby_init(void *ctx, scb_t *scb)
{
	wlc_mutx_info_t *mu_info = (wlc_mutx_info_t*) ctx;
	wlc_info_t *wlc = mu_info->wlc;
	mutx_scb_cubby_t *cubby = MUTX_SCB_CUBBY(mu_info, scb);
	mutx_scb_t *mu_scb = cubby->mutx_scb;
	uint8 *secptr = wlc_scb_sec_cubby_alloc(wlc, scb, mu_scb_cubby_secsz(ctx, scb));

	cubby->mutx_scb = mu_scb = (mutx_scb_t *)secptr;
	if (mu_scb != NULL) {
		secptr += ALIGN_SIZE(sizeof(mutx_scb_t), sizeof(uint32));
		mu_scb->item = mutx_scb_list_new(mu_info, mu_scb);
		if (mu_scb->item == NULL) {
			WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
				mu_info->pub->unit, __FUNCTION__, MALLOCED(mu_info->osh)));
			return BCME_ERROR;
		}
#if defined(BCMDBG) || defined(DUMP_MUTX)
		mu_scb->mutx_stats = (mutx_stats_t *)secptr;
		secptr += ALIGN_SIZE(sizeof(mutx_stats_t), sizeof(uint32));
#endif // endif
#ifdef WLCNT
		mu_scb->mutx_pfmon_stats = (mutx_pfmon_stats_t*)secptr;
		secptr += ALIGN_SIZE(sizeof(mutx_pfmon_stats_t), sizeof(uint32));
		mu_scb->mutx_snr_calib_stats = (mutx_snr_calib_stats_t*)secptr;
#endif	/* WLCNT */

		/* New STA is not in the MU client set by default */
		mu_scb->client_index = MU_CLIENT_INDEX_NONE;
		mu_scb->retry_gid = MUTX_R_GID_RETRY_CNT;
		mu_scb->scb = scb;
		mu_scb->on_hold_dur = 0;
		mu_scb->score = 0;
		mu_scb->time_score = 0;
		mu_scb->tp = 0;
		mu_scb->mu_link_permit = FALSE;
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

	if (!mu_scb) {
		return;
	}

	if ((mu_scb->tp == VHTMU) || (mu_scb->tp == HEMMU)) {
		ASSERT(mu_scb->item !=  NULL);
		if (mu_in_client_set(mu_info, scb)) {
			mu_sta_mu_disable(mu_info, scb);
			if (mu_scb->tp == VHTMU) {
				/* Cancel any pending wlc_mutx_gid_complete
				 * packet callbacks.
				 */
				wlc_pcb_fn_find(wlc->pcb, wlc_mutx_gid_complete,
					(void*) mu_scb, TRUE);
			}
		} else if ((mu_scb->tp == HEMMU) &&
			SCB_DLOFDMA(scb)) {
			mu_info->muclient_elect_pending = TRUE;
		}
		if (mu_scb->item != NULL) {
			mutx_scb_list_rem(mu_scb->item);
		}
		mu_scb->tp = 0;
	}

	if (mu_scb->item != NULL) {
		mutx_scb_list_del(mu_info, mu_scb->item);
	}
	wlc_scb_sec_cubby_free(wlc, scb, mu_scb);
	cubby->mutx_scb = NULL;
}

#if defined(BCMDBG) || defined(BCMDBG_MU)
static int
wlc_mutx_sendgid(wlc_mutx_info_t *mu_info)
{
	mutx_scb_t *mu_scb;
	int i, success = BCME_OK;

	/* Send a Group ID management message to each MU client */
	for (i = 0; i < mu_info->max_muclients; i++) {
		if (mu_info->mu_clients[i] == NULL) {
			continue;
		}

		mu_scb = MUTX_SCB(mu_info, mu_info->mu_clients[i]);

		if (mu_group_id_msg_send(mu_info->wlc, mu_info->mu_clients[i],
		                         mu_scb->membership,
		                         mu_scb->position) == BCME_OK) {
			WL_MUMIMO(("Mu-MIMO client index: %u GID frame Successful\n", i));
		} else {
			WL_ERROR(("Mu-MIMO client index: %u GID frame Failed\n", i));
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
	uint16 i = BF_SHM_IDX_INV;
	mutx_scb_t *mu_scb = MUTX_SCB(mu_info, scb);

	BCM_REFERENCE(mu_scb);
	ASSERT(mu_scb->client_index == MU_CLIENT_INDEX_NONE);

	if (D11REV_LT(mu_info->pub->corerev, 128)) {
		return wlc_txbf_get_mubfi_idx(wlc->txbf, scb);
	}

	for (i = 0; i < mu_info->max_muclients; i++) {
		if ((mu_info->mu_clients[i] == NULL) ||
			(mu_info->mu_clients[i] == scb)) {
			break;
		}
	}
	return (i < mu_info->max_muclients) ? i : BF_SHM_IDX_INV;
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
	int i;

	if (!mu_info) {
		return BF_SHM_IDX_INV;
	}

	wlc = mu_info->wlc;
	mu_scb = MUTX_SCB(mu_info, scb);

	if (!mu_scb) {
		return BF_SHM_IDX_INV;
	}

	if (SCB_INTERNAL(scb)) {
		return BF_SHM_IDX_INV;
	}

	if (D11REV_LT(mu_info->pub->corerev, 128)) {
		return wlc_txbf_get_mubfi_idx(wlc->txbf, scb);
	}

	if (!(mu_scb->tp == VHTMU || mu_scb->tp == HEMMU)) {
		return BF_SHM_IDX_INV;
	}

	for (i = 0; i < mu_info->max_muclients; i++) {
		if (mu_info->mu_clients[i] == scb) {
			return i;
		}
	}
	return BF_SHM_IDX_INV;
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
	wlc_info_t *wlc = mu_info->wlc;
	mutx_scb_t *mu_scb = MUTX_SCB(mu_info, scb);
	uint16 i;
	int err = BCME_OK, fifo_avail_cnt;
	uint ac = (mu_info->mutx_policy->ac_policy != MUTX_AC_NONE) ?
		mu_info->mutx_policy->ac_policy : mu_info->mutx_policy->effective_ac;
	BCM_REFERENCE(fifo_avail_cnt);

	if (!mu_scb) {
		return BCME_BADARG;
	}

	if (mu_in_client_set(mu_info, scb))
		return BCME_ERROR;

	if (mu_scb->tp == HEMMU) {
		if (SCB_DLOFDMA(scb)) {
			wlc_fifo_free_all(wlc->fifo, scb);
			wlc_scbmusched_set_dlofdma(wlc->musched, scb, FALSE);
		}
		SCB_HEMMU_ENABLE(scb);
		wlc_txbf_tbcap_update(wlc->txbf, scb);
	} else if (SCB_VHT_CAP(scb)) {
		mu_scb->mu_link_permit = TRUE;
		wlc_txbf_link_upd(wlc->txbf, scb);
	}

	if (mu_info->muclient_pfmon) {
		fifo_avail_cnt = wlc_fifo_avail_count(wlc->fifo,
			ac - 1, WL_MU_DLMMU);
		WL_MUTX(("wl%d: STA "MACF" ac %d fifo_avail_cnt %d tx_type %d\n",
			wlc->pub->unit, ETHER_TO_MACF(scb->ea), ac, fifo_avail_cnt,
			mu_scb->tp));
		ASSERT(ac != MUTX_AC_NONE);
		(void)wlc_fifo_alloc(wlc->fifo, scb, WL_MU_DLMMU, (ac - 1));
		ASSERT((wlc_fifo_isMU(wlc->fifo, scb, (ac - 1))));
	}

	i = wlc_mutx_sta_mucidx_set(mu_info, scb);
	if (i == BF_SHM_IDX_INV) {
		WL_MUTX(("wl%d: %s: MU-MIMO STA "MACF" not admitted as admit"
			" list full\n", wlc->pub->unit,
			__FUNCTION__, ETHER_TO_MACF(scb->ea)));
		ASSERT(0);
		return BCME_NORESOURCE;
	}

	ASSERT(i < mu_info->max_muclients);
	mu_info->mu_clients[i] = scb;
	mu_scb->client_index = i;
	mu_scb->time_score = 0;
	wlc_musta_upd_heurs(mu_info, scb, HEURS_MU_ADMIT);
#ifdef MAC_AUTOTXV_OFF
	err = wlc_macreq_upd_bfi(wlc, scb,
		(mu_info->base_mucidx + i), TRUE);
	if (err != BCME_OK && err != BCME_UNSUPPORTED) {
		WL_ERROR(("wl%d: MAC command failed to update bfi"
			"for STA "MACF" i:%u. err:%d\n",
			mu_info->pub->unit, ETHER_TO_MACF(scb->ea), i, err));
		return BCME_ERROR;
	}
#endif // endif

	if (mu_scb->tp == VHTMU) {
		/* Use default group membership. This sends Group ID message to client. */
		if (D11REV_LT(mu_info->pub->corerev, 129)) {
			err = wlc_mutx_membership_set(mu_info, i,
				dflt_membership[i], dflt_position[i]);
		} else if (D11REV_IS(mu_info->pub->corerev, 130)) {
			err = wlc_mutx_membership_set(mu_info, i,
				dflt_membership_2x2[i], dflt_position_2x2[i]);
		} else {
			/* Use default group membership. This sends Group ID message to client. */
			err = wlc_mutx_membership_set(mu_info, i,
				dflt_membership_16[i], dflt_position_16[i]);
		}
	} else {
		ASSERT(mu_scb->tp == HEMMU);
		/* Update the ampdu_mpdu count */
		scb_ampdu_update_config_mu(wlc->ampdu_tx, scb);
#ifdef AP
		wlc_apps_ps_flush(wlc, scb);
#endif // endif
#ifdef WLTAF
		wlc_taf_scb_state_update(wlc->taf_handle, scb, TAF_NO_SOURCE,
			TAF_PARAM(SCB_MU(scb)), TAF_SCBSTATE_MU_DL_HEMIMO);
#endif // endif
	}

	if (err != BCME_OK) {
		/* unable to set membership; clear the list */
		mu_info->mu_clients[i] = NULL;
		mu_scb->client_index = MU_CLIENT_INDEX_NONE;
	} else {
		/* Initialize SNR adustment index in VASIP */
		if (mu_info->mu_snr_calib) {
			W_REG(wlc->osh, D11_PHY_REG_ADDR(wlc), (uint16)PHYREG_M2V_MSG0);
			W_REG(wlc->osh, D11_PHY_REG_DATA(wlc),
					(uint16)(0x8007 | (mu_scb->client_index << 6)));
			/*
			 * bit15:      interrupt bit
			 * bit[14:11]: SNR adjustment index, range [0:15] is equal to [-8:7] dB.
			 * bit[10]:    nss index (0 or 1)
			 * bit[9:6]:   client index;
			 * bit[5:0]:   interrupt type;
			 */
		}
		WL_ERROR(("wl%d: STA "MACF" selected as MU client at index %u tx_type %d.\n",
			mu_info->pub->unit, ETHER_TO_MACF(scb->ea), i, mu_scb->tp));
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
	heurs_reason_t heurs_reason;
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

	WL_ERROR(("wl%d: STA "MACF" removed from MU client set with index %d tx_type %d\n",
		mu_info->pub->unit, ETHER_TO_MACF(scb->ea), mu_scb->client_index,
		mu_scb->tp));

	if (mu_scb->policy_evict) {
		if (mu_scb->policy_evict == MUTX_POLICY_EVICT_PFMON) {
			heurs_reason = HEURS_MU_PFMON_EVICT;
		} else {
			heurs_reason = HEURS_MU_POLICY_MISMATCH_EVICT;
		}
	} else {
			heurs_reason = HEURS_MU_POLICY_SCORE_EVICT;
	}
	wlc_musta_upd_heurs(mu_info, scb, heurs_reason);
	mu_info->mu_clients[mu_scb->client_index] = NULL;
	mu_scb->client_index = MU_CLIENT_INDEX_NONE;
	mu_scb->time_score = 0;
	mu_scb->policy_evict = MUTX_POLICY_EVICT_NONE;
	/* Free all MU FIFOs allocated to this client */
	wlc_fifo_free_all(wlc->fifo, scb);
	if (SCB_MARKED_DEL(scb) || SCB_DEL_IN_PROGRESS(scb)) {
		WL_ERROR(("wl%d: %s skip STA: "MACF" that will be deleted\n", wlc->pub->unit,
			__FUNCTION__, ETHER_TO_MACF(scb->ea)));
		return BCME_ERROR;
	}
	if (!SCB_VHTMU(scb) && !SCB_HEMMU(scb)) {
		WL_MUTX(("wl%d: MU already disabled, skip\n", mu_info->pub->unit));
		return BCME_OK;
	}
	if (mu_scb->tp == HEMMU) {
		SCB_HEMMU_DISABLE(scb);
		wlc_txbf_tbcap_update(wlc->txbf, scb);
	} else {
		/* Inform the ucode/hw of client set change */
		SCB_VHTMU_DISABLE(scb);
		wlc_mutx_membership_clear(mu_info, scb);
		mu_scb->mu_link_permit = FALSE;
		wlc_txbf_link_upd(wlc->txbf, scb);
	}

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
	wlc_taf_scb_state_update(wlc->taf_handle, scb, TAF_NO_SOURCE, TAF_PARAM(SCB_MU(scb)),
		((mu_scb->tp == VHTMU) ?
		TAF_SCBSTATE_MU_DL_VHMIMO : TAF_SCBSTATE_MU_DL_HEMIMO));
#endif // endif
	return BCME_OK;
}

/* Clear the entire MU client set. */
static void
mu_client_set_clear(wlc_mutx_info_t *mu_info)
{
	int i;
	scb_t *scb;
	BCM_REFERENCE(mu_info);
	for (i = 0; i < mu_info->max_muclients; i++) {
		scb = mu_info->mu_clients[i];
		if (!scb) {
			continue;
		}

		if (mu_client_set_remove(mu_info, scb) != BCME_OK) {
			WL_ERROR(("wl%d:Failed to remove STA "MACF" from MU client set.\n",
				mu_info->pub->unit, ETHER_TO_MACF(scb->ea)));
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

	for (i = 0; i < mu_info->max_muclients; i++) {
		if (mu_info->mu_clients[i]) {
			clients++;
		}
	}

	if (clients >= mu_info->max_muclients) {
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

	if (mu_scb == NULL) {
		return;
	}
	if (!mu_info->active)
		return;

	if (mu_client_set_full(mu_info)) {
		return;
	}
	(void)mu_client_set_add(mu_info, scb);
}

/* Returns the number of clients that will be evicted */
static int
mu_client_get_evictee_cnt(wlc_mutx_info_t *mu_info)
{
	scb_t *evictee;
	mutx_scb_t *mu_scb_evict;
	int i, evictee_cnt = 0;

	for (i = 0; i < mu_info->max_muclients; i++) {
		evictee = mu_info->mu_clients[i];
		if (evictee) {
			mu_scb_evict = MUTX_SCB(mu_info, evictee);
			ASSERT(mu_scb_evict != NULL);
			ASSERT(mu_scb_evict->item != NULL);
			if (mu_scb_evict->policy_evict) {
				evictee_cnt++;
			}
		}
	}
	return evictee_cnt;
}
/* Select the MU-capable STAs that are not currently in the
 * MU client set.
 */
static void
mu_candidates_select(wlc_mutx_info_t *mu_info,
	int *vhtmu_cand_cnt,
	int *hemmu_cand_cnt,
	int *dlofdma_cand_cnt,
	bool sched)
{
	scb_t *scb, *candidate;
	mutx_scb_t *mu_scb;
	mutx_scb_list_t *pos, *next;
	int vhtcnt = 0, hecnt = 0, cnt = 0, i, ofdma_cnt = 0;
	int evictee_cnt;
	bool max_limit;
	wlc_info_t *wlc = mu_info->wlc;

	BCM_REFERENCE(wlc);
	BCM_REFERENCE(evictee_cnt);

	if (!mu_info->active && !HE_DLMU_ENAB(wlc->pub)) {
		return;
	}

	max_limit = mu_client_set_full(mu_info);
	evictee_cnt = mu_client_get_evictee_cnt(mu_info);
	FOREACH_LIST_POS(pos, next, mu_info->mu_cap_stas_list) {
		mu_scb = (mutx_scb_t *)pos->data;
		scb = mu_scb->scb;
		if (!mu_info->active || (!mu_scb) ||
			(max_limit && !evictee_cnt &&
			(!sched || mu_scb->score == 0)) ||
			(mu_scb->on_hold_dur) ||
			(mu_scb->client_index != MU_CLIENT_INDEX_NONE)) {
			continue;
		}

		ASSERT(!mu_in_client_set(mu_info, scb));
		if (mu_sta_mu_ready(mu_info, scb, TRUE)) {
			mutx_scb_list_move(mu_info->candidate_list, pos);
			if (mu_scb->tp == VHTMU) {
				vhtcnt++;
			}
			if (mu_scb->tp == HEMMU) {
				hecnt++;
			}
			cnt++;
		} else if (mu_sta_mu_ready(mu_info, scb, FALSE)) {
			mutx_scb_list_move(mu_info->alternate_list, pos);
			if (mu_scb->tp == VHTMU) {
				vhtcnt++;
			}
			if (mu_scb->tp == HEMMU) {
				hecnt++;
			}
			cnt++;
		}
	}

	for (i = 0; i < mu_info->max_muclients; i++) {
		scb = mu_info->mu_clients[i];
		if (scb) {
			mu_scb = MUTX_SCB(mu_info, scb);
			ASSERT(mu_scb != NULL);
			ASSERT(mu_scb->item != NULL);
			/* If client_samebw is FALSE, it means we can have
			 * mixed bw MU Client.
			 * Add policy mismatch MU Client to alternate_list.
			 */
			if (!mu_info->client_samebw) {
				mutx_scb_list_move(mu_info->alternate_list, mu_scb->item);
			}

			if (mu_scb->tp == VHTMU) {
				vhtcnt++;
			}
			if (mu_scb->tp == HEMMU) {
				hecnt++;
			}
			cnt++;
		}
	}

	FOREACH_LIST_POS(pos, next, mu_info->mu_cap_stas_list) {
		mu_scb = (mutx_scb_t *)pos->data;
		candidate = mu_scb->scb;
		if (!SCB_HEMMU(candidate)) {
			ofdma_cnt++;
		}
	}

	*vhtmu_cand_cnt = vhtcnt;
	*dlofdma_cand_cnt = ofdma_cnt;
	*hemmu_cand_cnt = hecnt;
	if (!(*vhtmu_cand_cnt || *hemmu_cand_cnt || *dlofdma_cand_cnt)) {
		mu_info->muclient_elect_pending = FALSE;
	}
}

/* Go through list of HE MMU capable users and see if we need to re-decide on evicting any
 * already admitted hemmu user
 */
void
wlc_mutx_evict_or_admit_muclient(wlc_mutx_info_t *mu_info, scb_t *scb)
{
	mutx_scb_t *mu_scb;

	/* If MU-MIMO transmit disabled locally, no need to process STA. */
	if (!mu_info ||
		!(mu_info->_mu_tx[TP_TO_IDX(VHTMU)] ||
		mu_info->_mu_tx[TP_TO_IDX(HEMMU)])) {
		return;
	}
	mu_scb = MUTX_SCB(mu_info, scb);
	if (!mu_scb || !mu_scb->tp) {
		return;
	}

	if (mu_in_client_set(mu_info, scb) &&
		!mu_sta_mu_ready(mu_info, scb, FALSE)) {
		mu_scb->policy_evict = MUTX_POLICY_EVICT_BW;
		mu_info->muclient_elect_pending = TRUE;
		mu_clients_select(mu_info);
	} else {
		mu_info->muclient_elect_pending = TRUE;
	}
}

static void
wlc_mutx_muclients_reclaim_txfifo(wlc_mutx_info_t *mu_info)
{
	wlc_info_t *wlc = mu_info->wlc;
	mutx_scb_t *mu_scb;
	mutx_scb_list_t *pos, *next;
	scb_t *mu_client, *candidate, *evictee;
	uint ac, i;
	int fifo_avail_cnt, fifo_max_cnt;
	int new_admit_cnt = 0;
	int admitted_cnt = 0;
	int scb_mufifo_cnt = 0;
	int tot_fifo_avail_cnt;
	uint16 evictees_bmp = 0;
	uint16 client_idx = 0;

	BCM_REFERENCE(new_admit_cnt);
	BCM_REFERENCE(fifo_max_cnt);
	BCM_REFERENCE(fifo_avail_cnt);

	if (!mu_info->muclient_pfmon || !mu_info->active ||
		LIST_IS_EMPTY(mu_info->new_list) ||
		!wlc_mu_fifo_count(wlc->fifo, WL_MU_DLMMU)) {
		return;
	}

	ac = (mu_info->mutx_policy->ac_policy != MUTX_AC_NONE) ?
		mu_info->mutx_policy->ac_policy : mu_info->mutx_policy->effective_ac;
		ASSERT(ac != MUTX_AC_NONE);
	new_admit_cnt = ITEMS_IN_LIST(mu_info->new_list);

	for (i = 0; i < mu_info->max_muclients; i++) {
		mu_client = mu_info->mu_clients[i];
		if (mu_client) {
			admitted_cnt++;
			client_idx = wlc_mutx_sta_client_index(mu_info, mu_client);
			evictees_bmp |= (1 << client_idx);
			mu_scb = MUTX_SCB(mu_info, mu_client);
			/* Free all MU FIFOs allocated to this client */
			wlc_fifo_free_all(wlc->fifo, mu_client);
			(void)wlc_fifo_alloc(wlc->fifo, mu_client, WL_MU_DLMMU, (ac - 1));
			ASSERT((wlc_fifo_isMU(wlc->fifo, mu_client, (ac - 1))));
			scb_mufifo_cnt +=
				wlc_scb_mu_fifo_count(wlc->fifo, mu_client);
		}
	}

	FOREACH_LIST_POS(pos, next, mu_info->new_list) {
		mu_scb = (mutx_scb_t *)pos->data;
		candidate = mu_scb->scb;
		if (candidate && SCB_DLOFDMA(candidate)) {
			wlc_fifo_free_all(wlc->fifo, candidate);
		}
	}

	tot_fifo_avail_cnt = wlc_fifo_dlmmu_avail_count(wlc->fifo);
	fifo_max_cnt = wlc_fifo_max_per_ac(wlc->fifo, WL_MU_DLMMU);
	fifo_avail_cnt = MIN(tot_fifo_avail_cnt, new_admit_cnt);
	fifo_avail_cnt = MIN(fifo_avail_cnt, fifo_max_cnt);
	fifo_avail_cnt = MIN(fifo_avail_cnt,
			(wlc_fifo_avail_count(wlc->fifo, ac - 1, WL_MU_DLMMU)));
	WL_MUTX(("%s+ ac_policy %d fifo_max_cnt %d tot_fifo_avail_cnt %d\n"
		"fifo_alloc_cnt %d fifo_avail_cnt %d admitted_cnt %d new_admit_cnt %d\n",
		__FUNCTION__, ac, fifo_max_cnt,
		tot_fifo_avail_cnt, scb_mufifo_cnt, fifo_avail_cnt,
		admitted_cnt, new_admit_cnt));

	FOREACH_LIST_POS(pos, next, mu_info->new_list) {
		mu_scb = (mutx_scb_t *)pos->data;
		candidate = mu_scb->scb;
		if (new_admit_cnt <= fifo_avail_cnt) {
			break;
		}
		mutx_scb_list_move(mu_info->mu_cap_stas_list, pos);
		WL_MUTX(("prune candidate------->"MACF"\n",
			ETHER_TO_MACF(candidate->ea)));
		new_admit_cnt = ITEMS_IN_LIST(mu_info->new_list);
	}
	new_admit_cnt = ITEMS_IN_LIST(mu_info->new_list);

	WL_MUTX(("%s- ac_policy %d fifo_max_cnt %d tot_fifo_avail_cnt %d\n"
		"fifo_alloc_cnt %d fifo_avail_cnt %d admitted_cnt %d new_admit_cnt %d\n",
		__FUNCTION__, ac, fifo_max_cnt,
		tot_fifo_avail_cnt, scb_mufifo_cnt, fifo_avail_cnt,
		admitted_cnt, new_admit_cnt));

	if ((new_admit_cnt + admitted_cnt) >= 2) {
		if (new_admit_cnt) {
			tot_fifo_avail_cnt -= new_admit_cnt;
		}
	} else {
		FOREACH_LIST_POS(pos, next, mu_info->new_list) {
			mu_scb = (mutx_scb_t *)pos->data;
			candidate = mu_scb->scb;
			mutx_scb_list_move(mu_info->mu_cap_stas_list, pos);
			WL_MUTX(("prune candidate------->"MACF" mu_type %d\n",
				ETHER_TO_MACF(candidate->ea), mu_scb->tp));
		}
		for (i = 0; ((evictees_bmp) &&
			(i < mu_info->max_muclients));
			i++, evictees_bmp >>= 1) {
			evictee = mu_info->mu_clients[i];
			if ((evictees_bmp & 1U) && evictee) {
				mu_scb = MUTX_SCB(mu_info, evictee);
				WL_MUTX((" %s evict------->"MACF" mu_type %d\n", __FUNCTION__,
					ETHER_TO_MACF(evictee->ea), mu_scb->tp));
				mu_client_set_remove(mu_info, evictee);
			}
		}
	}
}

static void
wlc_mutx_admit_prune_new_users(wlc_mutx_info_t *mu_info)
{
	wlc_info_t *wlc = mu_info->wlc;
	mutx_scb_t *mu_scb;
	mutx_scb_list_t *pos, *next;
	scb_t *mu_client, *candidate, *evictee;
	uint ac, i, ac_2;
	int admitted_cnt = 0;
	int new_admit_cnt;
	int fifo_avail_cnt, evictee_cnt;
	int fifo_max_cnt;
	int scb_mufifo_cnt = 0;
	int tot_fifo_avail_cnt = 0;
	uint16 evictees_bmp = 0;
	uint16 client_idx = 0;
	BCM_REFERENCE(candidate);
	BCM_REFERENCE(evictee_cnt);
	BCM_REFERENCE(ac);

	if (!mu_info->muclient_pfmon ||
		!mu_info->active ||
		LIST_IS_EMPTY(mu_info->new_list) ||
		!wlc_mu_fifo_count(wlc->fifo, WL_MU_DLMMU)) {
		return;
	}
	ac = (mu_info->mutx_policy->ac_policy != MUTX_AC_NONE) ?
		mu_info->mutx_policy->ac_policy : mu_info->mutx_policy->effective_ac;
	ASSERT(ac != MUTX_AC_NONE);
	new_admit_cnt = ITEMS_IN_LIST(mu_info->new_list);
	evictee_cnt = ITEMS_IN_LIST(mu_info->evictee_list);

	FOREACH_LIST_POS(pos, next, mu_info->evictee_list) {
		mu_scb = (mutx_scb_t *)pos->data;
		evictee = mu_scb->scb;
		if (!evictee) {
			continue;
		}
		scb_mufifo_cnt += wlc_scb_mu_fifo_count(wlc->fifo, evictee);
		tot_fifo_avail_cnt += wlc_scb_mu_fifo_count(wlc->fifo, evictee);
	}

	for (i = 0; i < mu_info->max_muclients; i++) {
		mu_client = mu_info->mu_clients[i];
		if (!mu_client) {
			continue;
		}
		mu_scb = MUTX_SCB(mu_info, mu_client);
		if (mu_scb->policy_evict) {
			continue;
		}
		admitted_cnt++;
		client_idx = wlc_mutx_sta_client_index(mu_info, mu_client);
		evictees_bmp |= (1 << client_idx);
		scb_mufifo_cnt +=
			wlc_scb_mu_fifo_count(wlc->fifo, mu_client);
		tot_fifo_avail_cnt +=
			wlc_scb_mu_fifo_count(wlc->fifo, mu_client);
	}

	FOREACH_LIST_POS(pos, next, mu_info->new_list) {
		mu_scb = (mutx_scb_t *)pos->data;
		candidate = mu_scb->scb;
		if (!candidate || !SCB_DLOFDMA(candidate)) {
			continue;
		}
		for (ac_2 = 0; ac_2 < AC_COUNT; ac_2++) {
			if (wlc_fifo_isMU(wlc->fifo, candidate, ac_2) &&
				(wlc_fifo_user_count(wlc->fifo,
					candidate, ac_2) == 1)) {
				tot_fifo_avail_cnt++;
			}
		}
	}

	tot_fifo_avail_cnt -= admitted_cnt;
	tot_fifo_avail_cnt -= admitted_cnt;
	tot_fifo_avail_cnt += wlc_fifo_dlmmu_avail_count(wlc->fifo);
	fifo_max_cnt = wlc_fifo_max_per_ac(wlc->fifo, WL_MU_DLMMU);
	fifo_avail_cnt = MIN(tot_fifo_avail_cnt, new_admit_cnt);
	fifo_avail_cnt = MIN(fifo_avail_cnt, fifo_max_cnt);
	WL_MUTX(("%s+ ac_policy %d fifo_max_cnt %d tot_fifo_avail_cnt %d\n"
		"fifo_alloc_cnt %d fifo_avail_cnt %d admitted_cnt %d new_admit_cnt %d "
		"evictee_cnt %d\n", __FUNCTION__, ac, fifo_max_cnt,
		tot_fifo_avail_cnt, scb_mufifo_cnt, fifo_avail_cnt,
		admitted_cnt, new_admit_cnt, evictee_cnt));

	if ((fifo_avail_cnt != 0) &&
		((fifo_avail_cnt + admitted_cnt) >= 2)) {
		WL_MUTX(("%s- ac_policy %d fifo_max_cnt %d "
			"tot_fifo_avail_cnt %d\nfifo_alloc_cnt %d fifo_avail_cnt %d "
			"admitted_cnt %d new_admit_cnt %d evictee_cnt %d\n", __FUNCTION__,
			ac, fifo_max_cnt, tot_fifo_avail_cnt,
			scb_mufifo_cnt, fifo_avail_cnt, admitted_cnt,
			new_admit_cnt, evictee_cnt));
			return;
	}
	FOREACH_LIST_POS(pos, next, mu_info->new_list) {
		mu_scb = (mutx_scb_t *)pos->data;
		candidate = mu_scb->scb;
		if (new_admit_cnt <= fifo_avail_cnt) {
			break;
		}
		mutx_scb_list_move(mu_info->mu_cap_stas_list, pos);
		WL_MUTX(("prune candidate------->"MACF" fifo-%d allocation"
			" failure\n", ETHER_TO_MACF(candidate->ea), ac));
			new_admit_cnt = ITEMS_IN_LIST(mu_info->new_list);
	}
	new_admit_cnt = ITEMS_IN_LIST(mu_info->new_list);
	WL_MUTX(("%s- ac_policy %d fifo_max_cnt %d tot_fifo_avail_cnt %d\n"
		"fifo_alloc_cnt %d fifo_avail_cnt %d admitted_cnt %d new_admit_cnt %d "
		"evictee_cnt %d\n", __FUNCTION__, ac, fifo_max_cnt,
		tot_fifo_avail_cnt, scb_mufifo_cnt, fifo_avail_cnt,
		admitted_cnt, new_admit_cnt, evictee_cnt));

	if ((new_admit_cnt + admitted_cnt) >= 2) {

	} else {
		FOREACH_LIST_POS(pos, next, mu_info->new_list) {
			mu_scb = (mutx_scb_t *)pos->data;
			candidate = mu_scb->scb;
			mutx_scb_list_move(mu_info->mu_cap_stas_list, pos);
			WL_MUTX(("prune candidate------->"MACF" mu_type %d\n",
				ETHER_TO_MACF(candidate->ea), mu_scb->tp));
		}
		for (i = 0; ((evictees_bmp) &&
			(i < mu_info->max_muclients));
			i++, evictees_bmp >>= 1) {
			evictee = mu_info->mu_clients[i];
			if ((evictees_bmp & 1U) && evictee) {
				WL_MUTX((" %s evict------->"MACF"\n", __FUNCTION__,
					ETHER_TO_MACF(evictee->ea)));
				mu_scb = MUTX_SCB(mu_info, evictee);
				mutx_scb_list_move(mu_info->evictee_list, mu_scb->item);
			}
		}
	}
}

static void
wlc_mutx_eval_dlofdma_admission(wlc_mutx_info_t *mu_info)
{
	scb_t *scb;
	mutx_scb_t *mu_scb;
	mutx_scb_list_t *pos, *next;
	uint16 dlofdma_admit_cnt = 0, dlofdma_cand_cnt = 0;
	uint16 max_dlofdma_users, min_dlofdma_users;
	uint8 min_bw, max_bw, link_bw;
	wlc_info_t *wlc = mu_info->wlc;
	bool allow_bw160 = FALSE, is_dlofdma_eligible;
	uint32 rspec, mcs, avg_mpdusz_thresh[MUTX_AC_VO+1], ac;
	int tx_type;

	if (!HE_DLMU_ENAB(wlc->pub)) {
		return;
	}

	if (!(mu_info->mutx_client_scheduler ||
			mu_info->muclient_elect_pending)) {
		return;
	}

	wlc_musched_get_min_dlofdma_users(wlc->musched,
		&min_dlofdma_users, &allow_bw160);
	min_bw = BW_20MHZ;
	max_bw = allow_bw160 ? BW_160MHZ : BW_80MHZ;
	max_dlofdma_users = wlc_txcfg_max_clients_get(wlc->txcfg, DLOFDMA);

	FOREACH_LIST_POS(pos, next, mu_info->mu_cap_stas_list) {
		if ((dlofdma_cand_cnt + dlofdma_admit_cnt) == max_dlofdma_users) {
			break;
		}

		mu_scb = (mutx_scb_t *)pos->data;
		scb = mu_scb->scb;
		link_bw = wlc_scb_ratesel_get_link_bw(wlc, scb);
		if (!((link_bw >= min_bw) && (link_bw <= max_bw))) {
			continue;
		}

		if (SCB_HEMMU(scb) || (mu_scb->tp != HEMMU)) {
			continue;
		}
		for (ac = MUTX_AC_BEBK; (mu_info->muclient_pfmon &&
				(ac < (MUTX_AC_VO + 1))); ac++) {
			if (mu_scb->ac_policy_scores[ac] == MUTX_AC_NONE) {
				continue;
			}
			avg_mpdusz_thresh[ac] = mu_info->mpdusz_dlofdma_admit_thresh[link_bw];
			WL_MUTX(("wl%d: "MACF" tx_type %d ac %u score = %u "
				"avg_mpdu_sz %u avg_mpdusz_thresh %d\n",
				mu_info->pub->unit, ETHER_TO_MACF(scb->ea),
				HEMMU, ac, mu_scb->ac_policy_scores[ac],
				RAVG_AVG(&mu_scb->ravg_info[ac]), avg_mpdusz_thresh[ac]));
		}

		rspec = wlc_scb_ratesel_get_primary(wlc, scb, NULL);
		mcs = wlc_ratespec_mcs(rspec);
		WL_MUTX(("isdlofdma ? %s link_bw %d mcs %d\n",
			SCB_DLOFDMA(scb) ? "TRUE" : "FALSE", link_bw, mcs));

		tx_type = DLOFDMA;
		if (mu_info->muclient_pfmon &&
			!CHSPEC_IS160(wlc->chanspec) &&
			(mcs >= MUTX_SU_MCS_THRESH)) {
			if (((mu_scb->ac_policy_scores[MUTX_AC_BEBK] != MUTX_AC_NONE) &&
				(RAVG_AVG(&mu_scb->ravg_info[MUTX_AC_BEBK]) >=
					avg_mpdusz_thresh[MUTX_AC_BEBK])) ||
				((mu_scb->ac_policy_scores[MUTX_AC_VI] != MUTX_AC_NONE) &&
				(RAVG_AVG(&mu_scb->ravg_info[MUTX_AC_VI]) >=
					(avg_mpdusz_thresh[MUTX_AC_VI]))) ||
				((mu_scb->ac_policy_scores[MUTX_AC_VO] != MUTX_AC_NONE) &&
				(RAVG_AVG(&mu_scb->ravg_info[MUTX_AC_VO]) >=
					(avg_mpdusz_thresh[MUTX_AC_VO])))) {
				tx_type = SU;
			}
		}
		is_dlofdma_eligible = wlc_musched_scb_isdlofdma_eligible(wlc->musched, scb);
		if (SCB_DLOFDMA(scb) && ((tx_type != DLOFDMA) ||
			(!is_dlofdma_eligible))) {
			WL_MUTX(("switch from DLOFDMA => SU\n"));
			dlofdma_admit_cnt++;
			mutx_scb_list_move(mu_info->admit_ofdma_list, pos);
		} else if (!SCB_DLOFDMA(scb) && !SCB_IS_UNUSABLE(scb) &&
			is_dlofdma_eligible && (tx_type == DLOFDMA)) {
			WL_MUTX(("switch from SU => DLOFDMA\n"));
			mutx_scb_list_move(mu_info->candidate_ofdma_list, pos);
			dlofdma_cand_cnt++;
		} else {
			if (SCB_DLOFDMA(scb)) {
				dlofdma_admit_cnt++;
			}
			WL_MUTX(("no switch\n"));
		}
		ASSERT((dlofdma_cand_cnt + dlofdma_admit_cnt) <= max_dlofdma_users);
	}

	if ((dlofdma_cand_cnt + dlofdma_admit_cnt) >= min_dlofdma_users) {
		WL_MUMIMO(("%s dlofdma admit list dlofdma_cand_cnt %d "
			"dlofdma_admit_cnt %d min_dlofdma_users %d\n", __FUNCTION__,
			dlofdma_cand_cnt, dlofdma_admit_cnt, min_dlofdma_users));
		FOREACH_LIST_POS(pos, next, mu_info->candidate_ofdma_list) {
			mu_scb = (mutx_scb_t *)pos->data;
			scb = mu_scb->scb;
			WL_MUMIMO((""MACF"\n", ETHER_TO_MACF(scb->ea)));

			/* Exclude dsmps clients if dsmps_war is enabled */
			if (wlc->dsmps_war && wlc_stf_is_scb_dynamic_smps(wlc, scb)) {
				mutx_scb_list_move(mu_info->mu_cap_stas_list, pos);
			} else {
				mutx_scb_list_move(mu_info->admit_ofdma_list, pos);
			}
			dlofdma_cand_cnt--;
		}
		ASSERT(dlofdma_cand_cnt == 0);
	} else {
		if (dlofdma_admit_cnt && (dlofdma_admit_cnt < min_dlofdma_users)) {
			WL_MUMIMO(("%s eviction list dlofdma_admit_cnt(%d) < "
				"min_dlofdma_users(%d)\n",
				__FUNCTION__, dlofdma_admit_cnt, min_dlofdma_users));
			FOREACH_LIST_POS(pos, next, mu_info->admit_ofdma_list) {
				mu_scb = (mutx_scb_t *)pos->data;
				scb = mu_scb->scb;
				WL_MUMIMO((""MACF"\n", ETHER_TO_MACF(scb->ea)));
				if (SCB_DLOFDMA(scb)) {
					dlofdma_admit_cnt--;
				}
			}

			FOREACH_LIST_POS(pos, next, mu_info->mu_cap_stas_list) {
				mu_scb = (mutx_scb_t *)pos->data;
				scb = mu_scb->scb;
				WL_MUMIMO((""MACF"\n", ETHER_TO_MACF(scb->ea)));
				if (SCB_DLOFDMA(scb)) {
					mutx_scb_list_move(mu_info->admit_ofdma_list, pos);
					dlofdma_admit_cnt--;
				}
			}

			ASSERT(dlofdma_admit_cnt == 0);
		}
		if (dlofdma_cand_cnt) {
			WL_MUMIMO(("%s n_dlofdma cand(%d) < min_dlofdma_users(%d)\n",
				__FUNCTION__, dlofdma_cand_cnt, min_dlofdma_users));
			FOREACH_LIST_POS(pos, next, mu_info->candidate_ofdma_list) {
				mu_scb = (mutx_scb_t *)pos->data;
				scb = mu_scb->scb;
				WL_MUMIMO((""MACF"\n", ETHER_TO_MACF(scb->ea)));
				mutx_scb_list_move(mu_info->mu_cap_stas_list, pos);
				dlofdma_cand_cnt--;
			}
			ASSERT(dlofdma_cand_cnt == 0);
		}
	}
	ASSERT(LIST_IS_EMPTY(mu_info->candidate_ofdma_list));
	return;
}

static void
wlc_mutx_admit_dlofdma_clients(wlc_mutx_info_t *mu_info)
{
	wlc_info_t *wlc = mu_info->wlc;
	scb_t *scb;
	mutx_scb_t *mu_scb;
	mutx_scb_list_t *pos, *next;
	bool ofdma_on;

	if (!HE_DLMU_ENAB(wlc->pub)) {
		return;
	}

	WL_MUMIMO(("%s new ofdma admit list\n", __FUNCTION__));
	FOREACH_LIST_POS(pos, next, mu_info->admit_ofdma_list) {
		mu_scb = (mutx_scb_t *)pos->data;
		scb = mu_scb->scb;
		WL_MUMIMO((""MACF"\n", ETHER_TO_MACF(scb->ea)));
		if (SCB_DLOFDMA(scb)) {
			ofdma_on = FALSE;
			/* Release DLOFDMA FIFO to this client */
			wlc_fifo_free_all(wlc->fifo, scb);
		} else {
			ofdma_on = TRUE;
		}
		wlc_scbmusched_set_dlofdma(wlc->musched, scb, ofdma_on);
		wlc_scbmusched_set_dlschpos(wlc->musched, scb, 0);
		/* re-initialize rate info */
		wlc_scb_ratesel_init(wlc, scb);
		mutx_scb_list_move(mu_info->mu_cap_stas_list, pos);
	}

	ASSERT(LIST_IS_EMPTY(mu_info->admit_ofdma_list));
	ASSERT(LIST_IS_EMPTY(mu_info->candidate_ofdma_list));
	return;
}

void
wlc_mutx_upd_min_dlofdma_users(wlc_mutx_info_t *mu_info)
{
	if (!HE_DLMU_ENAB(mu_info->wlc->pub)) {
		return;
	}
	mu_info->muclient_elect_pending = TRUE;
}

static void
wlc_musta_upd_heurs(wlc_mutx_info_t *mu_info, scb_t *scb,
	heurs_reason_t reason)
{
	mutx_scb_t *mu_scb;
	musta_heurs_t *musta_heurs;
	int ac;

	mu_scb = MUTX_SCB(mu_info, scb);
	musta_heurs = &mu_scb->musta_heurs;

	if (!mu_info->muclient_pfmon || !mu_info->active) {
		return;
	}

	switch (reason)
	{
		case HEURS_INIT:
		{
			ASSERT((!mu_in_client_set(mu_info, scb)));
			bzero(musta_heurs, sizeof(musta_heurs_t));
			musta_heurs->counter0 = 2;
			break;
		}
		case HEURS_MU_PFMON_EVICT:
		{
			ASSERT((mu_in_client_set(mu_info, scb)));
			ASSERT((mu_scb->policy_evict == MUTX_POLICY_EVICT_PFMON));
			musta_heurs->counter2 = HEURS_CNTMAX - musta_heurs->counter1;
			musta_heurs->counter0 = ((musta_heurs->counter2 * 1) >> 1) +
				(musta_heurs->counter0 - ((musta_heurs->counter0 * 1) >> 1));
			musta_heurs->counter1 = 0;
			musta_heurs->counter2 = 0;
			musta_heurs->counter3 = 0;
			WL_MUTX(("%s pfmon_evict------->"MACF" counter0 %u counter1 %u"
				" counter2 %u counter3 %u\n", __FUNCTION__,
				ETHER_TO_MACF(scb->ea), musta_heurs->counter0,
				musta_heurs->counter1, musta_heurs->counter2,
				musta_heurs->counter3));
			break;
		}
		case HEURS_MU_POLICY_MISMATCH_EVICT:
		case HEURS_MU_POLICY_SCORE_EVICT:
		{
			ASSERT((mu_in_client_set(mu_info, scb)));
			musta_heurs->counter0 = musta_heurs->counter0 -
				((musta_heurs->counter0 * 1) >> 1);
			musta_heurs->counter1 = 0;
			musta_heurs->counter2 = 0;
			musta_heurs->counter3 = 0;
			WL_MUTX(("%s %s_evict------->"MACF" counter0 %u counter1 %u"
				" counter2 %u counter3 %u\n", __FUNCTION__,
				mu_scb->policy_evict ?  "policy_mismatch" : "evict",
				ETHER_TO_MACF(scb->ea), musta_heurs->counter0,
				musta_heurs->counter1, musta_heurs->counter2,
				musta_heurs->counter3));
			break;
		}

		case HEURS_MU_ADMIT:
		case HEURS_MU_ADMIT_INC:
		{
			ASSERT((mu_in_client_set(mu_info, scb)));
			ASSERT((mu_scb->policy_evict == MUTX_POLICY_EVICT_NONE));
			musta_heurs->counter1 = HEURS_BOUNDINC(musta_heurs->counter1);
			musta_heurs->counter3++;
			ac = mu_info->mutx_policy->ac_policy;
			if ((ac == MUTX_AC_NONE) || (ac != MUTX_AC_NONE &&
				mu_scb->ac_policy_scores[ac] == MUTX_AC_NONE)) {
				wlc_muclient_clear_pfmon_stats(mu_info, scb);
			}
			WL_MUTX(("%s admit------->"MACF" counter0 %u counter1 %u"
				" counter2 %u counter3 %u\n", __FUNCTION__,
				ETHER_TO_MACF(scb->ea), musta_heurs->counter0,
				musta_heurs->counter1, musta_heurs->counter2,
				musta_heurs->counter3));

			break;
		}
		case HEURS_SU_DEC:
		{
			ASSERT((!mu_in_client_set(mu_info, scb)));
			musta_heurs->counter0 = HEURS_BOUNDDEC(musta_heurs->counter0);
			if (musta_heurs->counter0 > 0) {
				bzero(&mu_scb->ac_policy_scores,
					sizeof(mu_scb->ac_policy_scores));
			}
			WL_MUTX(("%s dec------->"MACF" counter0 %u counter1 %u"
				" counter2 %u counter3 %u\n", __FUNCTION__,
				ETHER_TO_MACF(scb->ea), musta_heurs->counter0,
				musta_heurs->counter1, musta_heurs->counter2,
				musta_heurs->counter3));

			break;
		}
		default:
		{
			ASSERT(0);
			break;
		}
	}
}

static void
wlc_mutx_admit_clients(wlc_mutx_info_t *mu_info)
{
	wlc_info_t *wlc = mu_info->wlc;
	scb_t *candidate, *evictee;
	mutx_scb_t *mu_scb;
	mutx_scb_list_t *pos, *next;
	uint16 muclients_bmp = { 0 };
	uint16 evictees_bmp = { 0 };
	uint16 client_idx = 0;
	int i;
#ifdef BCMDBG
	uint8 mu_type = 0;
	WL_MUMIMO(("%s+ TXPKTPENDTOT %d\n", __FUNCTION__, (TXPKTPENDTOT(wlc))));

	WL_MUMIMO(("%s+: mu_admit_list \n", __FUNCTION__));
	FOREACH_LIST_POS(pos, next, mu_info->admit_list) {
			mu_scb = (mutx_scb_t *)pos->data;
			ASSERT(mu_scb != NULL);
			candidate = mu_scb->scb;
			ASSERT(candidate != NULL);
			WL_MUMIMO((""MACF"\n", ETHER_TO_MACF(candidate->ea)));
	}
	WL_MUMIMO(("%s+: mu_cap_stas_list\n", __FUNCTION__));
	FOREACH_LIST_POS(pos, next, mu_info->mu_cap_stas_list) {
		mu_scb = (mutx_scb_t *)pos->data;
		candidate = mu_scb->scb;
		WL_MUMIMO((""MACF" mu_type %d\n", ETHER_TO_MACF(candidate->ea),
			mu_scb->tp));
	}
#endif /* BCMDBG */

	if (mu_info->active && !LIST_IS_EMPTY(mu_info->admit_list)) {
		if (ITEMS_IN_LIST(mu_info->admit_list) < 2) {
			FOREACH_LIST_POS(pos, next, mu_info->admit_list) {
				mu_scb = (mutx_scb_t *)pos->data;
				candidate = mu_scb->scb;
				if (mu_in_client_set(mu_info, candidate) &&
					(mu_scb->policy_evict == MUTX_POLICY_EVICT_NONE)) {
					mu_scb->policy_evict = MUTX_POLICY_EVICT_BW;
				} else {
					mutx_scb_list_move(mu_info->mu_cap_stas_list, pos);
				}
			}
		}
		muclients_bmp = 0;
		client_idx = 0;
		FOREACH_LIST_POS(pos, next, mu_info->admit_list) {
			mu_scb = (mutx_scb_t *)pos->data;
			candidate = mu_scb->scb;
			/* clients in mu clients set */
			if (mu_in_client_set(mu_info, candidate)) {
				if	(mu_scb->policy_evict == MUTX_POLICY_EVICT_NONE) {
					client_idx = wlc_mutx_sta_client_index(mu_info, candidate);
					muclients_bmp |= (1 << client_idx);
					wlc_musta_upd_heurs(mu_info, candidate,
						HEURS_MU_ADMIT_INC);
				}
				mutx_scb_list_move(mu_info->mu_cap_stas_list, pos);
			} else {
				/* new list: new clients which will be admitted */
				mutx_scb_list_move(mu_info->new_list, pos);
			}
		}

		evictees_bmp = ~muclients_bmp & NBITMASK(mu_info->max_muclients);
		/* eviction list: clients which will be evicted */
		for (i = 0; ((evictees_bmp) &&
			(i < mu_info->max_muclients));
			i++, evictees_bmp >>= 1) {
			evictee = mu_info->mu_clients[i];
			if ((evictees_bmp & 1U) && evictee) {
				mu_scb = MUTX_SCB(mu_info, evictee);
				mutx_scb_list_move(mu_info->evictee_list, mu_scb->item);
			}
		}
		if (!wlc->block_datafifo) {
			wlc_mutx_admit_prune_new_users(mu_info);
		}
		if (LIST_IS_EMPTY(mu_info->new_list) &&
			evictees_bmp && muclients_bmp &&
			!(muclients_bmp & (muclients_bmp - 1))) {
			ASSERT(muclients_bmp == (1 << client_idx));
			evictee = mu_info->mu_clients[client_idx];
			mu_scb = MUTX_SCB(mu_info, evictee);
			mutx_scb_list_move(mu_info->evictee_list, mu_scb->item);
		}
	}
	/* block data fifo only if needed */
	if (TXPKTPENDTOT(wlc)) {
		mu_info->block_datafifo = FALSE;
		if (mu_info->active && (!LIST_IS_EMPTY(mu_info->evictee_list) ||
			!LIST_IS_EMPTY(mu_info->new_list))) {
			/* rentry through wlc_mutx_txfifo_complete */
			wlc_block_datafifo(wlc, DATA_BLOCK_MUTX, DATA_BLOCK_MUTX);
			WL_MUMIMO(("wl%d: %s:  block data fifo %d\n", wlc->pub->unit, __FUNCTION__,
				(wlc->block_datafifo & DATA_BLOCK_MUTX) != 0));
			mu_info->block_datafifo = TRUE;
			return;
		}
	}

	/* evict clients from current mu client set */
	FOREACH_LIST_POS(pos, next, mu_info->evictee_list) {
		mu_scb = (mutx_scb_t *)pos->data;
		evictee = mu_scb->scb;
		WL_MUTX(("evict------->"MACF"\n", ETHER_TO_MACF(evictee->ea)));
		mutx_scb_list_move(mu_info->mu_cap_stas_list, pos);
		ASSERT(TXPKTPENDTOT(wlc) == 0);
		mu_client_set_remove(mu_info, evictee);
	}
	wlc_mutx_muclients_reclaim_txfifo(mu_info);
	/* newly admitted clients */
	FOREACH_LIST_POS(pos, next, mu_info->new_list) {
		mu_scb = (mutx_scb_t *)pos->data;
		candidate = mu_scb->scb;
		mutx_scb_list_move(mu_info->mu_cap_stas_list, pos);
		WL_MUTX(("admit------->"MACF"\n", ETHER_TO_MACF(candidate->ea)));
		ASSERT(!mu_in_client_set(mu_info, candidate));
		ASSERT(TXPKTPENDTOT(wlc) == 0);
		mu_sta_mu_enable(mu_info, candidate);
	}

	mu_info->muclient_elect_pending = FALSE;

	/* Evalute dlofdma admission */
	if (HE_DLMU_ENAB(wlc->pub)) {
		if (LIST_IS_EMPTY(mu_info->admit_ofdma_list)) {
			wlc_mutx_eval_dlofdma_admission(mu_info);
		}

		if (!LIST_IS_EMPTY(mu_info->admit_ofdma_list)) {
			if (TXPKTPENDTOT(wlc)) {
				wlc_block_datafifo(wlc, DATA_BLOCK_MUTX, DATA_BLOCK_MUTX);
				mu_info->block_datafifo = TRUE;
				return;
			}
			ASSERT(TXPKTPENDTOT(wlc) == 0);
			wlc_mutx_admit_dlofdma_clients(mu_info);
		}
	}

	if (mu_info->muclient_elect_pending) {
		mu_info->muclient_elect_pending = FALSE;
	}

#ifdef BCMDBG
	WL_MUMIMO(("%s-: mu_client_set\n", __FUNCTION__));
	for (i = 0; i < mu_info->max_muclients; i++) {
		candidate = mu_info->mu_clients[i];
		if (candidate == NULL) {
			continue;
		}
		mu_scb = MUTX_SCB(mu_info, candidate);
		WL_MUMIMO((""MACF" mu_type %d\n", ETHER_TO_MACF(candidate->ea),
				mu_scb->tp));
	}

	WL_MUMIMO(("%s-: mu_cap_stas_list\n", __FUNCTION__));
	FOREACH_LIST_POS(pos, next, mu_info->mu_cap_stas_list) {
		mu_scb = (mutx_scb_t *)pos->data;
		candidate = mu_scb->scb;
		WL_MUMIMO((""MACF" mu_type %d\n", ETHER_TO_MACF(candidate->ea),
			mu_scb->tp));
		if ((mu_scb->tp == HEMMU) &&
			(SCB_HEMMU(candidate) || SCB_DLOFDMA(candidate))) {
			mu_type = SCB_HEMMU(candidate) ? HEMMU : DLOFDMA;
			if (wlc_txbf_tbcap_check(wlc->txbf,
				candidate, mu_type) != BCME_OK) {
				WL_MUMIMO(("wl%d: %s Invalid BFI config\n",
					wlc->pub->unit, __FUNCTION__));
				ASSERT(0);
			}
		}
	}
	WL_MUMIMO(("%s- TXPKTPENDTOT %d\n", __FUNCTION__, (TXPKTPENDTOT(wlc))));
#endif /* BCMDBG */
	return;
}

static void
mu_clients_eviction(wlc_mutx_info_t *mu_info, bool sched)
{
	wlc_info_t *wlc = mu_info->wlc;
	scb_t *candidate, *evictee;
	mutx_scb_t *mu_scb_evict, *mu_scb;
	mutx_scb_list_t *evict_entry, *cand_entry, *alter_entry, *pos, *next;
	uint8 i;
	int nclients = 0, nvhtmu = 0, nhemmu = 0;
	int nvhtmu_cand = 0, nhemmu_cand = 0, mu_type = 0;
	BCM_REFERENCE(wlc);
	BCM_REFERENCE(candidate);

	nclients = 0;
	for (i = 0; i < mu_info->max_muclients; i++) {
		evictee = mu_info->mu_clients[i];
		if (!evictee) {
			continue;
		}
		mu_scb_evict = MUTX_SCB(mu_info, evictee);
		ASSERT(mu_scb_evict != NULL);
		ASSERT(mu_scb_evict->item != NULL);
		if (mu_scb_evict->policy_evict) {
			/* This client will be evicted,
			 * we don't need to add it to evictee_list
			 * for score comparison.
			 */
			if (mu_scb_evict->policy_evict == MUTX_POLICY_EVICT_BW) {
				mu_scb_evict->reason =
					MUTX_REASON_BW_POLICY_MISMATCH;
			} else if (mu_scb_evict->policy_evict ==
				MUTX_POLICY_EVICT_AC) {
				mu_scb_evict->reason =
					MUTX_REASON_AC_POLICY_MISMATCH;
			}
			mutx_scb_list_move(mu_info->admit_list, mu_scb_evict->item);
		} else {
			/* Increase the nclients if policy_evict is FALSE. */
			mutx_scb_list_move(mu_info->evictee_list,
				mu_scb_evict->item);
			nclients++;
			if (mu_scb_evict->tp == VHTMU) {
				nvhtmu++;
			} else if (mu_scb_evict->tp == HEMMU) {
				nhemmu++;
			}
		}
	}

	/* Compare max score candidate with min score evictee, and do eviction. */
	while ((cand_entry = mutx_scb_list_max(mu_info->candidate_list)) != NULL) {
		mu_scb = (mutx_scb_t *)cand_entry->data;
		candidate = mu_scb->scb;
		/* If current MU client set is not full,
		 * add candidate to the MU client set.
		 */
		if (nclients < mu_info->max_muclients) {
			mutx_scb_list_move(mu_info->admit_list, cand_entry);
			mu_scb->reason = MUTX_REASON_NEW_ADMIT;
			nclients++;
			if (mu_scb->tp == VHTMU) {
				nvhtmu_cand++;
			} else if (mu_scb->tp == HEMMU) {
				nhemmu_cand++;
			}

			if (!sched && (nclients == mu_info->max_muclients)) {
				break;
			}
		} else {
			evict_entry = mutx_scb_list_min(mu_info->evictee_list);
			if (evict_entry == NULL) {
				mutx_scb_list_move(mu_info->candidate_list, cand_entry);
				break;
			}
			mu_scb_evict = (mutx_scb_t *)evict_entry->data;
			/* If min score evictee is not less than max score candidate,
			 * then we don't need to do eviction.
			 */
			if (mu_scb_evict->score >= mu_scb->score) {
				mutx_scb_list_move(mu_info->admit_list, evict_entry);
				mutx_scb_list_move(mu_info->candidate_list, cand_entry);
				break;
			}

			if (mu_scb->tp == VHTMU) {
				nvhtmu_cand++;
			} else if (mu_scb->tp == HEMMU) {
				nhemmu_cand++;
			}

			if (mu_scb_evict->tp == VHTMU) {
				nvhtmu--;
			} else if (mu_scb_evict->tp == HEMMU) {
				nhemmu--;
			}

			mutx_scb_list_move(mu_info->admit_list, cand_entry);
			mu_scb->reason = MUTX_REASON_HIGH_TRAFFIC;
			mu_scb_evict->reason = MUTX_REASON_LOW_TRAFFIC;
			evictee = mu_scb_evict->scb;
			if (mu_sta_mu_ready(mu_info, evictee, TRUE)) {
				mutx_scb_list_move(mu_info->candidate_list, evict_entry);
			} else {
				mutx_scb_list_move(mu_info->alternate_list, evict_entry);
			}
			WL_MUMIMO(("wl%d:%s:Exchange evictee "MACF"(%d) "
				"and candidate "MACF"(%d)\n", wlc->pub->unit,
				__FUNCTION__, ETHER_TO_MACF(evictee->ea),
				mu_scb_evict->score, ETHER_TO_MACF(candidate->ea),
				mu_scb->score));
		}
	}

	/* If mu client set is not full, select one from alternate list. */
	while (nclients < mu_info->max_muclients) {
		alter_entry = mutx_scb_list_max(mu_info->alternate_list);
		if (alter_entry == NULL) {
			break;
		}
		mu_scb = (mutx_scb_t *)alter_entry->data;
		if (mu_scb->tp == VHTMU) {
			nvhtmu_cand++;
		} else if (mu_scb->tp == HEMMU) {
			nhemmu_cand++;
		}
		nclients++;
		mutx_scb_list_move(mu_info->admit_list, alter_entry);
	}

	if (((nvhtmu + nvhtmu_cand) == 1) ||
		((nhemmu + nvhtmu_cand) == 1)) {
		FOREACH_LIST_POS(pos, next, mu_info->admit_list) {
			mu_scb = (mutx_scb_t *)pos->data;
			if ((nvhtmu == 0) && (nvhtmu_cand == 1) &&
				(mu_scb->tp == VHTMU)) {
				nvhtmu_cand--;
				nclients--;
				mutx_scb_list_move(mu_info->mu_cap_stas_list, pos);
			}
			if ((nhemmu == 0) && (nhemmu_cand == 1) &&
				(mu_scb->tp == HEMMU)) {
				nhemmu_cand--;
				nclients--;
				mutx_scb_list_move(mu_info->mu_cap_stas_list, pos);
			}
		}

		FOREACH_LIST_POS(pos, next, mu_info->evictee_list) {
			mu_scb = (mutx_scb_t *)pos->data;
			if ((nvhtmu == 1) && (nvhtmu_cand == 0) &&
				(mu_scb->tp == VHTMU)) {
				nvhtmu--;
				mutx_scb_list_move(mu_info->mu_cap_stas_list, pos);
				nclients--;
			}
			if ((nhemmu == 1) && (nhemmu_cand == 0) &&
				(mu_scb->tp == HEMMU)) {
				nhemmu--;
				mutx_scb_list_move(mu_info->mu_cap_stas_list, pos);
				nclients--;
			}
		}
		mu_type = VHTMU;
		if ((nvhtmu == 0) && (nvhtmu_cand == 0)) {
			mu_type = HEMMU;
		}
		/* If mu client set is not full, select one from candidate_list. */
		while ((nclients) && (nclients < mu_info->max_muclients)) {
			/* If mu client set is not full, select one from candidate_list. */
			cand_entry = mutx_scb_list_max(mu_info->candidate_list);
			if (cand_entry == NULL) {
				break;
			}
			mu_scb = (mutx_scb_t *)cand_entry->data;
			if (mu_type == mu_scb->tp) {
				nhemmu++;
				nvhtmu++;
				nclients++;
				mutx_scb_list_move(mu_info->admit_list, cand_entry);
				break;
			}
			mutx_scb_list_move(mu_info->mu_cap_stas_list, cand_entry);
		}

		/* If mu client set is not full, select one from alternate list. */
		while ((nclients) && (nclients < mu_info->max_muclients)) {
			/* If mu client set is not full, select one from alternate list. */
			alter_entry = mutx_scb_list_max(mu_info->alternate_list);
			if (alter_entry == NULL) {
				break;
			}
			mu_scb = (mutx_scb_t *)alter_entry->data;
			if (mu_type == mu_scb->tp) {
				nhemmu++;
				nvhtmu++;
				nclients++;
				mutx_scb_list_move(mu_info->admit_list, alter_entry);
				break;
			}
			mutx_scb_list_move(mu_info->mu_cap_stas_list, alter_entry);
		}
	}

	/* add clients to admit list */
	FOREACH_LIST_POS(pos, next, mu_info->evictee_list) {
		mutx_scb_list_move(mu_info->admit_list, pos);
	}

	FOREACH_LIST_POS(pos, next, mu_info->candidate_list) {
		mutx_scb_list_move(mu_info->mu_cap_stas_list, pos);
	}
	FOREACH_LIST_POS(pos, next, mu_info->alternate_list) {
		mutx_scb_list_move(mu_info->mu_cap_stas_list, pos);
	}
	FOREACH_LIST_POS(pos, next, mu_info->evictee_list) {
		ASSERT(0);
		mutx_scb_list_rem(pos);
		mu_scb = (mutx_scb_t *)pos->data;
		mu_scb->reason = MUTX_REASON_LOW_TRAFFIC;
	}
}

void
wlc_mutx_dlofdma_client_set_clear(wlc_mutx_info_t *mu_info)
{
	wlc_info_t *wlc = mu_info->wlc;
	scb_t *scb;
	mutx_scb_t *mu_scb;
	mutx_scb_list_t *pos, *next;

	if (!HE_DLMU_ENAB(wlc->pub)) {
		return;
	}
	FOREACH_LIST_POS(pos, next, mu_info->mu_cap_stas_list) {
		mu_scb = (mutx_scb_t *)pos->data;
		scb = mu_scb->scb;
		if (SCB_DLOFDMA(scb)) {
			/* Release DLOFDMA FIFO to this client */
			wlc_fifo_free_all(wlc->fifo, scb);
			wlc_scbmusched_set_dlofdma(wlc->musched, scb, FALSE);
			wlc_scbmusched_set_dlschpos(wlc->musched, scb, 0);
			/* re-initialize rate info */
			wlc_scb_ratesel_init(wlc, scb);
		}
	}
}

void
wlc_mutx_evict_all_muclients(wlc_mutx_info_t *mu_info)
{
	wlc_info_t *wlc = mu_info->wlc;
	BCM_REFERENCE(wlc);

	WL_MUMIMO(("wl%d: %s: Force evict all DLMU clients\n", wlc->pub->unit, __FUNCTION__));
	mu_client_set_clear(mu_info);
	if (HE_DLMU_ENAB(wlc->pub)) {
		wlc_mutx_dlofdma_client_set_clear(mu_info);
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
		printf("%s muclient_elect_pending\n", __FUNCTION__);
		mu_info->muclient_elect_pending = TRUE;
	}

	return BCME_OK;
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
	mutx_policy_t *mutx_policy;
	mutx_scb_t *mu_scb;

	if (!mu_info->active) {
		return FALSE;
	}
	mu_scb = MUTX_SCB(mu_info, scb);
	ASSERT((mu_scb->tp == VHTMU) ||
		(mu_scb->tp == HEMMU));

	if ((mu_scb->tp == HEMMU) &&
		!mu_info->_mu_tx[TP_TO_IDX(HEMMU)]) {
		return FALSE;
	}
	if (!wlc_mutx_sta_nrx_check(mu_info, scb)) {
		return FALSE;
	}

	ASSERT((mu_scb->tp == VHTMU) || (mu_scb->tp == HEMMU));
	mutx_policy = mu_info->mutx_policy;
	link_bw = wlc_scb_ratesel_get_link_bw(mu_info->wlc, scb);
	/* Check traffic loads */
	if (mu_info->muclient_pfmon && !wlc_mutx_sta_ac_check(mu_info, scb, link_bw)) {
		WL_MUTX(("%s "MACF" traffic load != ac_policy\n",
			__FUNCTION__, ETHER_TO_MACF(scb->ea)));
		return FALSE;
	}

	/* Check BW policy */
	bw_policy = mutx_policy->bw_policy;
	if (((!mu_info->client_bw160) && (link_bw == BW_160MHZ)) ||
		((mu_info->client_bw160) && (link_bw == BW_160MHZ) &&
		(mu_scb->tp != HEMMU)) ||
		((strict_check || mu_info->client_samebw) &&
		(link_bw != bw_policy))) {
		WL_MUTX(("%s "MACF" link_bw != bw_policy\n",
			__FUNCTION__, ETHER_TO_MACF(scb->ea)));
		return FALSE;
	}

	if (SCB_IS_UNUSABLE(scb) || !SCB_ASSOCIATED(scb) || !BSSCFG_AP(scb->bsscfg)) {
		WL_MUTX(("%s "MACF" !associated\n", __FUNCTION__,
			ETHER_TO_MACF(scb->ea)));
		return FALSE;
	}

#ifdef WL11AX
	if ((mu_scb->tp == HEMMU) && (!wlc_he_get_ulmu_allow(mu_info->wlc->hei, scb) ||
		wlc_twt_scb_active(mu_info->wlc->twti, scb))) {
		WL_MUTX(("%s "MACF" %s\n", __FUNCTION__, ETHER_TO_MACF(scb->ea),
		(wlc_twt_scb_active(mu_info->wlc->twti, scb) ? "twt link active" :
		"ulmu not allowed")));
		return FALSE;
	}

	/* Check dynamic smps and dsmps_war */
	if (mu_info->wlc->dsmps_war && wlc_stf_is_scb_dynamic_smps(mu_info->wlc, scb)) {
		WL_MUTX(("%s "MACF" d-smps enabled\n", __FUNCTION__, ETHER_TO_MACF(scb->ea)));
		return FALSE;
	}
#endif /* WL11AX */

	/* Can come here looking for a STA to replace a STA just removed
	 * from the MU client set. If client was removed because its BSS
	 * is going down, avoid selecting another STA in the same BSS.
	 */
	if (!scb->bsscfg->up) {
		WL_MUTX(("%s "MACF" !up\n", __FUNCTION__, ETHER_TO_MACF(scb->ea)));
		return FALSE;
	}
	return TRUE;
}

static bool
wlc_mutx_sta_nrx_check(wlc_mutx_info_t *mu_info, scb_t *scb)
{
	mutx_scb_t *mu_scb;
	if (!mu_info->active) {
		return FALSE;
	}

	mu_scb = MUTX_SCB(mu_info, scb);
	ASSERT((mu_scb->tp == VHTMU) ||
		(mu_scb->tp == HEMMU));

	if (mu_scb->nrx > mu_info->muclient_nrx) {
		WL_MUTX(("%s "MACF" nrx(%d) != nrx_policy (%d)\n",
			__FUNCTION__, ETHER_TO_MACF(scb->ea), mu_scb->nrx,
			mu_info->muclient_nrx));
		return FALSE;
	}

	if (!wlc_txbf_scb_nsts_check(mu_info->wlc->txbf, scb)) {
		return FALSE;
	}

	return TRUE;
}

bool
wlc_mutx_sta_ac_check(wlc_mutx_info_t *mu_info, scb_t *scb, uint link_bw)
{
	mutx_scb_t *mu_scb;
	mutx_policy_t *mutx_policy;
	uint32 ac;
	musta_heurs_t *musta_heurs;

	if (!mu_info->active ||
		(mu_info->ac_policy_on == FALSE)) {
		return TRUE;
	}

	mu_scb = MUTX_SCB(mu_info, scb);
	if (!mu_scb)
		return FALSE;

	ASSERT((mu_scb->tp == VHTMU) ||
		(mu_scb->tp == HEMMU));

	mutx_policy = mu_info->mutx_policy;
	ac = mutx_policy->ac_policy;
	if ((ac == MUTX_AC_NONE) ||
		(ac != 0 && mu_scb->ac_policy_scores[ac] == 0) ||
		(RAVG_AVG(&mu_scb->ravg_info[ac]) <=
		mu_info->mpdusz_mu_admit_thresh[link_bw])) {
		return FALSE;
	}

	musta_heurs = &mu_scb->musta_heurs;
	if (musta_heurs->counter0 > 0) {
		return FALSE;
	}
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
mu_scb_membership_change_get(wlc_mutx_info_t *mu_info, scb_t *scb)
{
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
mu_scb_membership_change_set(wlc_mutx_info_t *mu_info, scb_t *scb, bool pending)
{
	mutx_scb_t *mu_scb;
	wlc_info_t *wlc = mu_info->wlc;

	mu_scb = MUTX_SCB(mu_info, scb);
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
	wlc_taf_scb_state_update(wlc->taf_handle, scb, TAF_NO_SOURCE, TAF_PARAM(SCB_MU(scb)),
		TAF_SCBSTATE_MU_DL_VHMIMO);
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

#ifdef WL_MU_FEATURES_AUTO
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
	wlc_mutx_update(wlc);

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
#endif /* WL_MU_FEATURES_AUTO */

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

	if (mu_scb_membership_change_get(mu_info, scb))
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

static void
wlc_mutx_scb_state_upd(void *ctx, scb_state_upd_data_t *notif_data)
{
	wlc_mutx_info_t *mu_info = (wlc_mutx_info_t*) ctx;
	wlc_info_t *wlc = mu_info->wlc;
	scb_t *scb;
	mutx_scb_t *mu_scb;
	uint8 oldstate;

	ASSERT(notif_data != NULL);
	scb = notif_data->scb;
	ASSERT(scb != NULL);
	oldstate = notif_data->oldstate;

	/* If this device is not MU tx capable, no need to maintain MU client set.
	 * Radio has to go down to enable MUTX; so clients will reassociate
	 * and AP can relearn them then.
	 */
	if (!(mu_info->_mu_tx[TP_TO_IDX(VHTMU)] || mu_info->_mu_tx[TP_TO_IDX(HEMMU)] ||
		HE_DLMU_ENAB(wlc->pub))) {
		return;
	}

	mu_scb = MUTX_SCB(mu_info, scb);
	if (!mu_scb) {
		return;
	}

	if ((oldstate & ASSOCIATED) && !SCB_ASSOCIATED(scb)) {
		/* STA has disassociated. If STA is in current MU client set,
		 * remove it.
		 */

		if (mu_scb->tp == 0) {
			return;
		}
		ASSERT((mu_scb->tp == VHTMU) || (mu_scb->tp == HEMMU));
		WL_ERROR(("wl%d: STA "MACF" has disassociated tx_type %d\n",
		          wlc->pub->unit, ETHER_TO_MACF(scb->ea), mu_scb->tp));
	} else {

		if (mu_scb->tp) {
			return;
		}

		if (HE_ENAB(wlc->pub) && SCB_HE_CAP(scb)) {
			if (!HE_DLMMU_ENAB(wlc->pub) && !HE_DLMU_ENAB(wlc->pub)) {
				return;
			}
		} else if (!MU_TX_ENAB(wlc) || !SCB_VHT_CAP(scb)) {
			return;
		}

		if (!((SCB_HE_CAP(scb) || SCB_VHT_CAP(scb)) &&
			wlc_txbf_is_mu_bfe(wlc->txbf, scb))) {
			return;
		}

		mu_scb->tp = (SCB_HE_CAP(scb) &&
			(HE_DLMMU_ENAB(wlc->pub) || HE_DLMU_ENAB(wlc->pub))) ?
			HEMMU : VHTMU;
		mutx_scb_list_move(mu_info->mu_cap_stas_list, mu_scb->item);
		if (mu_info->muclient_pfmon) {
			wlc_musta_upd_heurs(mu_info, scb, HEURS_INIT);
		}
		wlc_mutx_bw_policy_update(mu_info, FALSE);
		/* MU capable STA now fully ready */
		WL_ERROR(("wl%d: STA "MACF" has associated tx_type %d\n",
		          wlc->pub->unit, ETHER_TO_MACF(scb->ea), mu_scb->tp));
		if ((mu_scb->tp == HEMMU) || (mu_scb->tp == VHTMU)) {
			if (mu_scb->tp == HEMMU) {
				/* TODO: add bw160 support */
				mu_scb->nrx = HE_MAX_SS_SUPPORTED(scb->rateset.he_bw80_rx_mcs_nss);
			} else {
				mu_scb->nrx = VHT_MCS_SS_SUPPORTED(4, scb->rateset.vht_mcsmap) ? 4 :
				(VHT_MCS_SS_SUPPORTED(3, scb->rateset.vht_mcsmap) ? 3 :
				(VHT_MCS_SS_SUPPORTED(2, scb->rateset.vht_mcsmap) ? 2 : 1));
			}
		}
	}
	if (!mu_info->muclient_pfmon) {
		mu_info->muclient_elect_pending = TRUE;
	}
} /* wlc_mutx_scb_state_upd */

/*
 * Processing operating mode notification for channel bw
 */
void
wlc_mutx_update_scb_oper_mode(wlc_mutx_info_t *mu_info,
	scb_t *scb, uint8 oper_mode)
{
	wlc_info_t *wlc;
	mutx_scb_t *mu_scb;

	/* If MU-MIMO transmit disabled locally, no need to process STA. */
	if (!mu_info ||
		!(mu_info->_mu_tx[TP_TO_IDX(VHTMU)] || mu_info->_mu_tx[TP_TO_IDX(HEMMU)])) {
		return;
	}
	wlc = mu_info->wlc;
	mu_scb = MUTX_SCB(mu_info, scb);
	if (!mu_scb || !mu_scb->tp) {
		return;
	}

	if (!mu_info->active) {
		return;
	}

	if (!wlc_vht_get_scb_opermode_enab(wlc->vhti, scb) ||
		DOT11_OPER_MODE_RXNSS_TYPE(oper_mode)) {
		return;
	}

	if (mu_in_client_set(mu_info, scb) &&
		!mu_sta_mu_ready(mu_info, scb, FALSE)) {
		mu_scb->policy_evict = MUTX_POLICY_EVICT_BW;
		mu_clients_select(mu_info);
		mu_info->muclient_elect_pending = TRUE;
	} else {
		wlc_mutx_bw_policy_update(mu_info, FALSE);
	}

	return;
}

bool
wlc_mutx_is_hedlmmu_enab(wlc_mutx_info_t *mu_info)
{
	return ((mu_info->_mu_tx[TP_TO_IDX(HEMMU)] == TRUE) ? TRUE : FALSE);
}

/* Enable or disable Mu Tx feature */
void
wlc_mutx_update(wlc_info_t *wlc)
{
	wlc_mutx_info_t *mu_info = (wlc_mutx_info_t*) wlc->mutx;
	uint8 tp_idx;
	bool previous[MUTX_TP_MAX];
	wlc->_mu_tx = FALSE;
	/* This checks both phy capability and current configuration */
	for (tp_idx = 0; tp_idx < MUTX_TP_MAX; tp_idx++) {
		previous[tp_idx] = mu_info->_mu_tx[tp_idx];
		mu_info->_mu_tx[tp_idx] = wlc_txbf_mutx_enabled(wlc->txbf, (VHTMU + tp_idx));
	}

	if (mu_info->_mu_tx[TP_TO_IDX(VHTMU)]) {
		wlc->_mu_tx = TRUE;
	}

	if ((mu_info->_mu_tx[TP_TO_IDX(VHTMU)] != previous[TP_TO_IDX(VHTMU)]) ||
		(mu_info->_mu_tx[TP_TO_IDX(HEMMU)] != previous[TP_TO_IDX(HEMMU)])) {
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
		if (D11REV_IS(wlc->pub->corerev, 130)) {
			wlc_mhf(wlc, MXHF1, MXHF1_SUBMVP_DISA, MXHF1_SUBMVP_DISA, WLC_BAND_ALL);
		}
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

void
wlc_mutx_nrx_policy_upd(wlc_info_t *wlc, int mu_supported_Ntx)
{
	wlc_mutx_info_t *mu_info = wlc->mutx;

	if (mu_supported_Ntx != 2) {
		return;
	}

	WL_MUTX(("%s vasip mu_supported_Ntx %d nrx policy = %d\n",
		__FUNCTION__, mu_supported_Ntx, MUCLIENT_NRX_1));

	mu_info->mupfmon_mumcs_thresh = MUTX_PRIMRATE_MUMCS_THRESH_2X2;
	mu_info->mutx_admit_mcs_thresh = MUTX_ADMIT_MCS_THRESH_2x2;
	mu_info->muclient_nrx = MUCLIENT_NRX_1;
}

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
	wlc_info_t *wlc;
	wlc_vasip_info_t *vasip;
	bool mutx_active[MUTX_TP_MAX] = {TRUE, TRUE};
	bool mutx_was_active;
	int txchains;

	if (!mu_info) {
		return;
	}

	wlc = mu_info->wlc;
	vasip = wlc->vasip;

	/* This checks both phy capability and current configuration */
	if (!mu_info->_mu_tx[TP_TO_IDX(VHTMU)]) {
		WL_MUMIMO(("wl%d: VHTMU TX not active. MU BFR not enabled.\n",
			wlc->pub->unit));
		mutx_active[TP_TO_IDX(VHTMU)] = FALSE;
	}

	if (!mu_info->_mu_tx[TP_TO_IDX(HEMMU)]) {
		WL_MUMIMO(("wl%d: HEMMU TX not active. HEMMU BFR not enabled.\n",
			wlc->pub->unit));
		mutx_active[TP_TO_IDX(HEMMU)] = FALSE;
	}

	/* Check number of transmit streams */
	txchains = wlc->stf->op_txstreams;
	if (txchains != vasip->mu_supported_Ntx) {
		WL_MUMIMO(("wl%d: MU TX not active. "
			"Number of tx chains, %d, not equal to VASIP FW supported Ntx, %u.\n",
			 wlc->pub->unit, txchains, vasip->mu_supported_Ntx));
		mutx_active[TP_TO_IDX(VHTMU)] = FALSE;
		mutx_active[TP_TO_IDX(HEMMU)] = FALSE;
	}

	WL_MUTX(("%s vasip mu_supported_Ntx %d\n", __FUNCTION__, vasip->mu_supported_Ntx));
	mutx_was_active = mu_info->active;
	mu_info->active = mutx_active[TP_TO_IDX(VHTMU)] || mutx_active[TP_TO_IDX(HEMMU)];
	if (!mutx_was_active && mu_info->active) {
		/* Bring MU TX up. Schedule MU client election. Don't do election
		 * immediately in case there are other configuration or state changes
		 * that follow immediately after this one. Give system time to settle.
		 */
		WL_MUMIMO(("wl%d: Enabling DLMU and marking elect_pending\n",
			wlc->pub->unit));
			mu_info->muclient_elect_pending = TRUE;
	} else if (mutx_was_active && !mu_info->active) {
		/* Bring MU TX down */
		/* Clear MU client set */
		WL_MUMIMO(("wl%d: Clearing DLMU client set\n",
				wlc->pub->unit));
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

#ifdef NOT_YET
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
#endif /* NOT_YET */

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
	//uint8 max_vht_usrs = wlc_txcfg_max_clients_get(mu_info->wlc->txcfg, VHTMU);
	uint8 max_vht_usrs = mu_info->max_muclients;

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
	WL_ERROR(("wl%d: Sending GID Mgmt frame to STA "MACF".\n",
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
	wlc_mutx_info_t *mu_info = wlc->mutx;
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

	ASSERT(mu_scb->tp == VHTMU);
	if (txstatus & TX_STATUS_ACK_RCV) {
		/* STA is now prepared to receive MU-PPDUs */
		mu_scb_membership_change_set(mu_info, scb, FALSE);
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

#ifdef WL_MU_FEATURES_AUTO
static void
wlc_mutx_state_upd(wlc_info_t *wlc, uint8 state)
{
	mutx_state_upd_data_t data;
	data.state = state;

	bcm_notif_signal(wlc->mutx->mutx_state_notif_hdl, &data);
}
#endif // endif

int
wlc_mutx_state_upd_register(wlc_info_t *wlc, bcm_notif_client_callback fn, void *arg)
{
	wlc_mutx_info_t *mu_info = (wlc_mutx_info_t *)wlc->mutx;
	bcm_notif_h hdl = mu_info->mutx_state_notif_hdl;
#ifdef WL_MU_FEATURES_AUTO
	if (mu_info->mutx_switch_in_progress) {
		return BCME_BUSY;
	}
#endif // endif
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
	vht_cap_ie_t vht_cap = {0};
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
		WL_MUTX(("%s client ea "MACF" already exists @idx %d\n", __FUNCTION__,
		ETHER_TO_MACF(mupkteng_clinfo[client_idx].scb->ea), client_idx));
		return BCME_ERROR;
	}

	scb = wlc_scblookup(wlc, wlc->primary_bsscfg, ea);
	if (scb == NULL) {
		WL_ERROR(("Cannot create scb for ea "MACF"\n", ETHER_TO_MACF(*ea)));
		return BCME_ERROR;
	}
	/* Make STA VHT capable */
	scb->flags2 |= SCB2_VHTCAP;
	wlc_vht_update_scb_state(wlc->vhti, wlc->band->bandtype, scb, &vht_cap, 0, 0);
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
		WL_MUTX(("%s: client "MACF" idx %d queued nframes %d flen %d fifo_idx %d\n",
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
wlc_mu_snr_calib_scb(wlc_mutx_info_t *mu_info, scb_t *scb)
{
	uint32 unacked, txmu_total, txmu_succ_total, txmu_count;
	int per, i, primary_mcs, nss, index, mcs_max;
	uint16 phyreg_data;
	bool interrupt;
	int per_threshold_low[12] =    {15, 15, 10, 10, 10,  5,  5,  5,  5,  5,  5,  5};
	int per_threshold_middle[12] = {30, 30, 25, 20, 25, 20, 10, 10, 15, 10, 10, 10};
	int per_threshold_high[12] =   {50, 50, 40, 40, 40, 40, 35, 25, 25, 25, 20, 20};
	int thpt_ratio[12] =   {1, 2, 3, 4, 6, 8, 9, 10, 12, 13, 15, 17};
	int n_mpdu_thres_settle, n_mpdu_thres_low, n_mpdu_thres_high;
	wlc_info_t *wlc = mu_info->wlc;
	mutx_scb_t	*mutx_scb = MUTX_SCB(mu_info, scb);
	mutx_snr_calib_stats_t *mutx_snr_calib_stats = mutx_scb->mutx_snr_calib_stats;
	uint8 mucidx;
	mcs_max = SCB_HEMMU(scb) ? MCS_MAX_HE : MCS_MAX_VHT;
	mucidx = wlc_mutx_sta_mucidx_get(mu_info, scb);

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

		n_mpdu_thres_settle = MPDU_THRES_SETTLE * thpt_ratio[primary_mcs] * (nss + 1);
		n_mpdu_thres_low = MPDU_THRES_LOW * (primary_mcs + 3);
		n_mpdu_thres_high = MPDU_THRES_HIGH * (primary_mcs + 3);

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
				if ((primary_mcs < mcs_max) || (index < 3)) {
					index ++;
					interrupt = 1;
				}
			}
		}

		if (interrupt == 1) {
			W_REG(wlc->osh, D11_PHY_REG_ADDR(wlc), (uint16)PHYREG_M2V_MSG0);
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
				(mucidx << 6) | 5;
			W_REG(wlc->osh, D11_PHY_REG_DATA(wlc), phyreg_data);

			mutx_snr_calib_stats->index[nss] = index;
			mutx_snr_calib_stats->change[nss] = 1;
			continue;
		}

		if ((txmu_total > (20*MPDU_THRES_HIGH)) || (interrupt == 1)) {
			wlc_mu_snr_calib_reset(mutx_scb, nss);
			mutx_snr_calib_stats->txmu_total[nss] += txmu_total;
			mutx_snr_calib_stats->txmu_total[nss] =
				(mutx_snr_calib_stats->txmu_total[nss] > (MPDU_THRES_HIGH * 512))
				? (MPDU_THRES_HIGH * 512) : mutx_snr_calib_stats->txmu_total[nss];
		}
	}
	return;
}

static void BCMFASTPATH
wlc_mutx_mutxs_update(wlc_mutx_info_t *mu_info, scb_t *scb,
	mutx_pfmon_stats_t *mutx_pfmon_stats, mutx_snr_calib_stats_t *mutx_snr_calib_stats,
	tx_status_t *txs)
{
#if defined(BCMDBG) || defined(DUMP_MUTX)
	mutx_scb_t *mu_scb = NULL;
	mutx_stats_t *mutx_stats = NULL;
#endif // endif

	uint8 gbmp = TX_STATUS64_MU_GBMP(txs->status.s4);
	uint8 gpos = TX_STATUS64_MU_GPOS(txs->status.s4);
	uint8 mutxcnt = TX_STATUS64_MU_TXCNT(txs->status.s4);
	uint8 mupktnum = TX_STATUS40_TXCNT_RT0(txs->status.s3);
	uint8 muacknum = TX_STATUS40_ACKCNT_RT0(txs->status.s3);
	uint8 gid = TX_STATUS64_MU_GID(txs->status.s3);
	ratespec_t rspec = TX_STATUS64_MU_RSPEC(txs->status.s4);
	uint8 rate_index, nss, mcs, gnum = 0;
	bool gid_invalid = ((gid >= VHT_SIGA1_GID_MAX_GID) ||
		((gid == 0) && !SCB_HE_CAP(scb) /* ideally should check VHT MU frame instead */));
	uint8 txs_mutp = TX_STATUS_MUTP_VHTMU; /* mu type, dflt to VHTMUMIMO */

	txs_mutp = TX_STATUS_MUTYP(mu_info->wlc->pub->corerev, txs->status.s5);

	if (((txs_mutp != TX_STATUS_MUTP_VHTMU) && (txs_mutp != TX_STATUS_MUTP_HEMM)) ||
		((gbmp == 0) && (txs_mutp == TX_STATUS_MUTP_HEMM))) {
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

#if defined(BCMDBG) || defined(DUMP_MUTX)
	mu_scb = MUTX_SCB(mu_info, scb);
	if (mu_scb)
		mutx_stats = mu_scb->mutx_stats;
	if (mutx_stats) {
		bool is_sgi = TX_STATUS64_MU_SGI(txs->status.s3);

		mutx_stats->gnum[gnum - 1]++;
		mutx_stats->gpos_cnt[gpos]++;
		mutx_stats->txmu[rate_index] += mupktnum;
		if (is_sgi) {
			mutx_stats->txmu_sgi[rate_index] += mupktnum;
			mutx_stats->txmu_sgi_succ[rate_index] += muacknum;
		}
		mutx_stats->txmu_succ[rate_index] += muacknum;
	}
#endif // endif

	mutx_pfmon_stats->gpos_cnt[gpos]++;
	mutx_pfmon_stats->mutx_cnt += mupktnum;
	mutx_pfmon_stats->txmu[rate_index] += mupktnum;
	if (mutxcnt == 1) {
		mutx_snr_calib_stats->txmu[rate_index] += mupktnum;
		mutx_snr_calib_stats->txmu_succ[rate_index] += muacknum;
		mutx_pfmon_stats->txmu_primrate[rate_index] += mupktnum;
	}

	if (mutxcnt == 0) {
		/* This must be RTS failure to the retry limit */
		ASSERT(mupktnum == 0);
	} else {
#if defined(BCMDBG) || defined(DUMP_MUTX)
		if (mutx_stats) {
			mutx_stats->mutxcnt_stat[mutxcnt - 1] += mupktnum;
			mutx_stats->mutxcnt_succ[mutxcnt - 1] += muacknum;
		}
#endif // endif
		mutx_pfmon_stats->mutxcnt_stat[mutxcnt - 1] += mupktnum;
		mutx_pfmon_stats->mutxcnt_succ[mutxcnt - 1] += muacknum;
	}

#ifdef WLCNT
	if (mu_info->mu_snr_calib) {
		wlc_mu_snr_calib_scb(mu_info, scb);
	}
#endif // endif

	if (txs_mutp == TX_STATUS_MUTP_VHTMU) {
#if defined(BCMDBG) || defined(DUMP_MUTX)
		if (mutx_stats) {
			/* Update group ID count */
			mutx_stats->gid_cnt[gid]++;
			mutx_stats->gid_last_rate[gid] = rate_index;
		}
#endif // endif
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
	mutx_pfmon_stats_t *mutx_pfmon_stats;
	mutx_snr_calib_stats_t *mutx_snr_calib_stats;

	if (mu_info == NULL) {
		return;
	}

	mu_scb = MUTX_SCB(mu_info, scb);
	if (mu_scb == NULL) {
		return;
	}

	mutx_snr_calib_stats = mu_scb->mutx_snr_calib_stats;
	mutx_pfmon_stats = mu_scb->mutx_pfmon_stats;
	if (mutx_pfmon_stats == NULL) {
		return;
	}

	wlc_mutx_mutxs_update(mu_info, scb, mutx_pfmon_stats,
		mutx_snr_calib_stats, txs);
}

void BCMFASTPATH
wlc_mutx_update_txcounters(wlc_mutx_info_t *mu_info, scb_t *scb,
	bool txs_mu, tx_status_t *txs, ratesel_txs_t *rs_txs, uint8 rnum)
{
	mutx_scb_t *mu_scb;
	uint8 nss, mcs, i, txassu_reason;
#if defined(BCMDBG) || defined(DUMP_MUTX)
	mutx_stats_t *mutx_stats;
#endif // endif
	mutx_pfmon_stats_t *mutx_pfmon_stats;
	bool snd_epch_cng;
	mutx_snr_calib_stats_t *mutx_snr_calib_stats;
	uint rate_index;
	if (!mu_info || !scb) {
		return;
	}

	mu_scb = MUTX_SCB(mu_info, scb);
	if (mu_scb == NULL) {
		return;
	}

	mutx_snr_calib_stats = mu_scb->mutx_snr_calib_stats;

	mutx_pfmon_stats = mu_scb->mutx_pfmon_stats;
	if (mutx_pfmon_stats == NULL) {
		return;
	}

	mu_info->mu_ncons += rs_txs->ncons;
	mu_info->mu_nlost += rs_txs->nlost;

#if defined(BCMDBG) || defined(DUMP_MUTX)
	mutx_stats = mu_scb->mutx_stats;
	if (mutx_stats) {
		mutx_stats->mu_ncons += rs_txs->ncons;
		mutx_stats->mu_nlost += rs_txs->nlost;
	}
#endif // endif

	if (txs_mu) {
		wlc_mutx_mutxs_update(mu_info, scb, mutx_pfmon_stats,
				mutx_snr_calib_stats, txs);
	} else {
		/* Txed as SU */
		txassu_reason  = TX_STATUS128_M2SQ(txs->status.s5);
		snd_epch_cng = (TX_STATUS128_SNDFL_EPCH(txs->status.s5)
				!= mutx_pfmon_stats->snd_epch_prev);
		if (snd_epch_cng) {
#if defined(BCMDBG) || defined(DUMP_MUTX)
			mutx_stats->snd_fail_cnt++;
#endif // endif
			mutx_pfmon_stats->snd_fail_cnt++;
			mutx_pfmon_stats->snd_epch_prev =
				TX_STATUS128_SNDFL_EPCH(txs->status.s5);
#if defined(BCMDBG) || defined(DUMP_MUTX)
			mutx_stats->snd_epch_prev =
				TX_STATUS128_SNDFL_EPCH(txs->status.s5);
#endif // endif
		}
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
				rate_index = mcs + ((nss - 1) * MUTX_MCS_NUM);
				ASSERT(rate_index < MUTX_MCS_INDEX_NUM);
				mutx_pfmon_stats->txassu_reason[txassu_reason] +=
						rs_txs->tx_cnt[i];
				mutx_pfmon_stats->txsu[rate_index] += rs_txs->tx_cnt[i];
#if defined(BCMDBG) || defined(DUMP_MUTX)
				/* Update MCS count */
				if (mutx_stats) {
					mutx_stats->txassu_reason[txassu_reason] +=
						rs_txs->tx_cnt[i];
					mutx_stats->txsu[rate_index] += rs_txs->tx_cnt[i];
					mutx_stats->txsu_succ[rate_index] += rs_txs->txsucc_cnt[i];
					/* Update group ID count */
					mutx_stats->gid_cnt[VHT_SIGA1_GID_NOT_TO_AP]++;
					mutx_stats->gid_last_rate[VHT_SIGA1_GID_NOT_TO_AP]
						= rate_index;
				}
#endif // endif

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
				mutx_pfmon_stats->txassu_reason[txassu_reason] +=
						rs_txs->tx_cnt[i];
				mutx_pfmon_stats->txsu[rate_index] += rs_txs->tx_cnt[i];
#if defined(BCMDBG) || defined(DUMP_MUTX)
				if (mutx_stats) {
					mutx_stats->txassu_reason[txassu_reason] +=
						rs_txs->tx_cnt[i];
					mutx_stats->txsu[rate_index] += rs_txs->tx_cnt[i];
					mutx_stats->txsu_succ[rate_index] += rs_txs->txsucc_cnt[i];
				}
#endif // endif
			}
		}
	}
}
#endif /* WLCNT */

void
wlc_mutx_txfifo_complete(wlc_info_t *wlc)
{
	wlc_mutx_info_t *mu_info = wlc->mutx;
	WL_MUMIMO(("wl%d: %s:  %d\n", wlc->pub->unit, __FUNCTION__,
	        (wlc->block_datafifo & DATA_BLOCK_MUTX) != 0));
	/* XXX, "mu_info->block_datafifo" is needed to prevent
	 * wlc_mutx_admit_clients getting called recursively(RB:162583).
	 */
	if (mu_info->block_datafifo) {
		ASSERT(TXPKTPENDTOT(wlc) == 0);
		mu_info->block_datafifo = FALSE;
		wlc_mutx_admit_clients(mu_info);
		wlc_block_datafifo(wlc, DATA_BLOCK_MUTX, 0);
	}
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
	int err, bw;
	scb_cubby_params_t cubby_params;

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
	mu_info->_mu_tx[TP_TO_IDX(VHTMU)] = FALSE;
	mu_info->_mu_tx[TP_TO_IDX(HEMMU)] = FALSE;

	/* Avoid registering callbacks if phy is not MU capable */
	if (!WLC_MU_BFR_CAP_PHY(wlc) ||
		D11REV_LT(mu_info->pub->corerev, 129)) {
		return mu_info;
	}

	/* Default enable scheduler and AC policy for now */
	mu_info->mutx_client_scheduler = TRUE;
	mu_info->mupfmon_mumcs_thresh = MUTX_PRIMRATE_MUMCS_THRESH;
	mu_info->mutx_admit_mcs_thresh = MUTX_ADMIT_MCS_THRESH;

#ifdef WLCNT
	if (D11REV_GE(mu_info->pub->corerev, 129) && D11REV_LE(mu_info->pub->corerev, 132)) {
		mu_info->mu_snr_calib = TRUE;
	} else {
		mu_info->mu_snr_calib = FALSE;
	}
#else /* CNT */
	/* always turn off MUMIMO SNR CALIBRATION without WLCNT */
	mu_info->mu_snr_calib = FALSE;
#endif /* CNT */

	if (D11REV_GE(mu_info->pub->corerev, 129) && D11REV_LE(mu_info->pub->corerev, 132) &&
		(CHIPID(si_chipid(wlc->pub->sih)) != BCM43692_CHIP_ID)) {
		/* only 43684b0/b1, 63178/47622, 6710a0/a1 and 6715a0 for now, default MU on
		 * but 6705 (same chipid as 43692) and 43692 default MU off.
		 */
		mu_info->pub->mu_features = MU_FEATURES_MUTX;
		/* The number of active TX FIFOs may have changed */
		wlc_hw_update_nfifo(wlc->hw);
	}

	mu_info->mutx_client_scheduler_dur = MUCLIENT_SCHEDULER_DUR;

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
	bzero(&cubby_params, sizeof(cubby_params));

	cubby_params.context = mu_info;
	cubby_params.fn_init = mu_scb_cubby_init;
	cubby_params.fn_deinit = mu_scb_cubby_deinit;
	cubby_params.fn_secsz = mu_scb_cubby_secsz;
	cubby_params.fn_dump = mu_scb_cubby_dump;
	mu_info->scb_handle = wlc_scb_cubby_reserve_ext(wlc, sizeof(mutx_scb_cubby_t),
		&cubby_params);
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

#if defined(BCMDBG) || defined(DUMP_MUTX)
	wlc_dump_add_fns(mu_info->pub, "mutx", wlc_mutx_dump, wlc_mutx_dump_clr, mu_info);
#endif // endif

	mu_info->ac_policy_on = TRUE;
	mu_info->client_samebw = FALSE;
	mu_info->muclient_pfmon = TRUE;
	mu_info->rssi_thresh = MUTX_RSSI_MIN;
	mu_info->muclient_nrx = MUCLIENT_NRX_2;

	for (bw = BW_20MHZ; bw <= BW_160MHZ; bw++) {
		mu_info->mpdusz_dlofdma_admit_thresh[bw] = MUTX_MPDUSZ_DLOFDMA_ADMIT_THRESH;
	}
	mu_info->mpdusz_mu_admit_thresh[BW_20MHZ] = MUTX_MPDUSZ_BW20_MU_ADMIT_THRESH;
	mu_info->mpdusz_mu_admit_thresh[BW_40MHZ] = MUTX_MPDUSZ_BW40_MU_ADMIT_THRESH;
	mu_info->mpdusz_mu_admit_thresh[BW_80MHZ] = MUTX_MPDUSZ_BW80_MU_ADMIT_THRESH;
	mu_info->mpdusz_mu_admit_thresh[BW_160MHZ] = MUTX_MPDUSZ_BW160_MU_ADMIT_THRESH;

	mu_info->active = TRUE;
	if (D11REV_GE(mu_info->pub->corerev, 132)) {
		mu_info->client_bw160 = TRUE;
	} else {
		mu_info->client_bw160 = FALSE;
	}
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

	mu_info->new_list = mutx_scb_list_new(mu_info, NULL);
	if (mu_info->new_list == NULL) {
		wlc_mutx_detach(mu_info);
		return NULL;
	}

	mu_info->candidate_ofdma_list = mutx_scb_list_new(mu_info, NULL);
	if (mu_info->candidate_ofdma_list == NULL) {
		wlc_mutx_detach(mu_info);
		return NULL;
	}
	mu_info->admit_ofdma_list = mutx_scb_list_new(mu_info, NULL);
	if (mu_info->admit_ofdma_list == NULL) {
		wlc_mutx_detach(mu_info);
		return NULL;
	}
	mu_info->mu_cap_stas_list = mutx_scb_list_new(mu_info, NULL);
	if (mu_info->mu_cap_stas_list == NULL) {
		wlc_mutx_detach(mu_info);
		return NULL;
	}

	if ((mu_info->mutx_policy = (mutx_policy_t *)
		MALLOCZ(wlc->osh, sizeof(mutx_policy_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes for MU TX policy\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
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

	if (mu_info->mutx_policy != NULL) {
		MFREE(mu_info->osh, mu_info->mutx_policy, sizeof(mutx_policy_t));
		mu_info->mutx_policy = NULL;
	}
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
	if (mu_info->new_list != NULL)
		mutx_scb_list_del(mu_info, mu_info->new_list);
	if (mu_info->mu_cap_stas_list != NULL)
		mutx_scb_list_del(mu_info, mu_info->mu_cap_stas_list);
	if (mu_info->candidate_ofdma_list != NULL)
		mutx_scb_list_del(mu_info, mu_info->candidate_ofdma_list);
	if (mu_info->admit_ofdma_list != NULL)
		mutx_scb_list_del(mu_info, mu_info->admit_ofdma_list);
	wlc_scb_state_upd_unregister(mu_info->wlc, wlc_mutx_scb_state_upd, mu_info);
	wlc_module_unregister(mu_info->pub, "mutx", mu_info);
	MFREE(mu_info->osh, mu_info, sizeof(wlc_mutx_info_t));
}

/* Update HE MU-MIMO list for Dynamic SMPS clients. When the station indicates it's
 * going to Dynamic SMPS mode, we remove the station from HE MU-MIMO list. It is a
 * WAR for a short term solution until we have MU-RTS feature.
 */
void wlc_mutx_update_hemmu_dsmps(wlc_mutx_info_t *mu_info, scb_t *scb)
{
	bool mmu_on, mmu_redo = FALSE;
	wlc_info_t *wlc = mu_info->wlc;
	mutx_scb_t *mu_scb = MUTX_SCB(mu_info, scb);

	if (!mu_scb || (mu_scb->tp != HEMMU)) {
		return;
	}

	mmu_on = mu_sta_mu_ready(mu_info, scb, FALSE);
	if (!mmu_on && SCB_HEMMU(scb)) {
		/* if the client is currently in HE MU-MIMO list and no longer eligible after
		 * mmu status change when receiving mimops in ht action, remove the client from
		 * MU-MIMO list
		 */
		mmu_redo = TRUE;
		mu_client_set_remove(mu_info, scb);
	} else if (mmu_on && !SCB_HEMMU(scb)) {
		/* if the client is not currently in HE MU-MIMO list and becomes eligible after
		 * receiving mimops in ht action, run mutx admit process
		 */
		mmu_redo = TRUE;
	}

	if (mmu_redo) {
		wlc_mutx_evict_or_admit_muclient(wlc->mutx, scb);
	}
}

#endif /* WL_MU_TX */
