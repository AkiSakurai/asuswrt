/*
 * 802.11ah/11ax Target Wake Time protocol and d11 h/w manipulation.
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_twt.c 791028 2020-09-14 09:23:08Z $
 */

#ifdef WLTWT

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <bcm_math.h>
#include <wlioctl.h>
#include <802.11.h>
#include <802.11ah.h>
#include <wlc_types.h>
#include <wlc_pub.h>
#include <wlc_ratelinkmem.h>
#include <wlc_twt.h>
#include <wlc.h>
#include <phy_hecap_api.h>
#include <wlc_bsscfg.h>
#include <wlc_scb.h>
#include <wlc_iocv_cmd.h>
#include <wlc_mcnx.h>
#include <wlc_ie_mgmt.h>
#include <wlc_ie_mgmt_types.h>
#include <wlc_pcb.h>
#include <wl_export.h>
#include <wlc_musched.h>
#include <wlc_bmac.h>
#include <wlc_apps.h>
#include <wlc_iocv.h>
#include <wlc_test.h>
#include <wlc_hrt.h>
#include <wlc_fifo.h>
#include <wlc_hw_priv.h>
#ifdef WLCFP
#include <wlc_cfp.h>
#endif /* WLCFP */
#include <wlc_dump.h>
#include <wlc_mutx.h>
#include <wlc_ulmu.h>

/* Next debug mode is needed to test situation where client is not in PS while entering TWT.
 * This is possible behavior, but not done the WCC client. As a result it is hard to test this
 * path. Setting next define result in timer which will internally leave PM (and possibly fill
 * queues) just before TWT becomes active, allowing the testing of forced PM to enter TWT
 */
/* #define DEBUG_TWT_PS */

#ifdef TESTBED_AP_11AX
#define WLC_TWT_USE_4MS_SCHEDID
#endif /* TESTBED_AP_11AX */

#define WLC_TWT_MAX_BTWT_CLIENTS	16		/* Max clients supported in one BTWT (AP) */
#define WLC_TWT_MAX_BTWT		8		/* Max broadcast TWTs currently supported */
#define WLC_TWT_MAX_ITWT_SCB		1		/* Max individual TWTs per SCB supported */
#define WLC_TWT_MAX_ITWT_TOTAL		16		/* Max total individual TWTs supported */

#define WLC_TWT_FIRST_SP_AHEAD_NOPM	50000		/* Safety margin for TWT in future NO PM */
#define WLC_TWT_FIRST_SP_AHEAD_PM	500000		/* Safety margin for TWT in future in PM */
#define WLC_TWT_AF_LIFETIME_SAFETY	5000		/* usec */

#define WLC_TWT_TSF_MAX			0xffffffff	/* TSF (u32) MAX value */

#define WLC_TWT_BID_OFFSET		8		/* Used for mapping bid to fid */
#define WLC_TWT_FID2BID(fid)	((fid) - WLC_TWT_BID_OFFSET)
#define WLC_TWT_BID2FID(bid)	((bid) + WLC_TWT_BID_OFFSET)
#define WLC_TWT_FID_IS_BID(fid)	(((fid) >= WLC_TWT_BID_OFFSET) ? TRUE : FALSE)

/* BSS desc state values, flags, INACTIVE is defined as value to make comparison easy */
#define WLC_TWT_STATE_INACTIVE		0		/* Resource not allocated, keep 0 !! */
#define WLC_TWT_STATE_RESERVED		(1 << 0)	/* Resource allocated, not active */
#define WLC_TWT_STATE_ACTIVE		(1 << 1)	/* Resource allocated, and active */

#define WLC_TWT_TOP_IE_SZ		(sizeof(twt_ie_top_t))
#define WLC_TWT_BCAST_IE_SZ		(sizeof(twt_ie_bcast_t))
#define WLC_TWT_INDV_IE_SZ		(sizeof(twt_ie_indv_t))

#define WLC_TWT_BTWT_SHIFT		10		/* Shift count for TWT field in BCAST IE */
#define WLC_TWT_BTWT_MASK		0xFFFF
#define WLC_TWT_BTWT_TSF_MASK		(WLC_TWT_BTWT_MASK << WLC_TWT_BTWT_SHIFT)

#define WLC_TWT_TEARDOWN_COUNT		10		/* Noof beacons to schedule teardown */
#define WLC_TWT_TEARDOWN_TRY		4		/* Noof tries to send teardown request */

/* Action Frame Pending/Awaiting statemachine types */
#define WLC_TWT_AF_STATE_NONE		0	/* No AF outstanding */
#define WLC_TWT_AF_PEND_TEARDOWN	1	/* Waiting for Teardown AF Ack */
#define WLC_TWT_AF_PEND_SETUP_ITWT_REQ	2	/* Waiting for iTWT Setup Request AF Ack */
#define WLC_TWT_AF_AWAIT_SETUP_ITWT_RSP	3	/* Waiting for iTWT Setup Response */
#define WLC_TWT_AF_PEND_SETUP_ITWT_RSP	4	/* Waiting for iTWT Setup Response AF Ack */
#define WLC_TWT_AF_PEND_SETUP_BTWT_REQ	5	/* Waiting for bTWT Setup Request AF Ack */
#define WLC_TWT_AF_AWAIT_SETUP_BTWT_RSP	6	/* Waiting for iTWT Setup Response */
#define WLC_TWT_AF_PEND_SETUP_BTWT_RSP	7	/* Waiting for bTWT Setup Response AF Ack */

#define WLC_TWT_AF_TIMEOUT		20	/* TBTT, wait max 20 beacons for response */

/* Ucode interface for TWT uses scheduleid. This scheduled ID is a 4bit timestamp which
 * represents the TSF in blocks of 8ms (1 << 13). For this some defines are created here
 */
#ifdef WLC_TWT_USE_4MS_SCHEDID
#define WLC_SCHEDID_TSF_SHIFT		12
#else /* If 4MS schedid size is not set then use the default 8 msec */
#define WLC_SCHEDID_TSF_SHIFT		13
#endif /* WLC_TWT_USE_4MS_SCHEDID */
#define WLC_TSFL_SCHEDID_MASK_INV	((1 << WLC_SCHEDID_TSF_SHIFT) - 1)
#define WLC_TSFL_SCHEDID_MASK		(~WLC_TSFL_SCHEDID_MASK_INV)
#define WLC_TSFL_TO_SCHEDID(tsfl)	(uint16)((tsfl) >> WLC_SCHEDID_TSF_SHIFT)
#define WLC_SCHEDID_TO_TSFL(schedid)	((schedid) << WLC_SCHEDID_TSF_SHIFT)

#define WLC_PRETWT_TIME			2048	/* Ucode time of interrupt ahead of schedule */

#define WLC_TWT_MAX_RLM_USR		16

/* Ratelinkmem structure */
typedef struct wlc_twt_rlm {
	uint16	schedid;
	uint16	usr_count;
	uint16  usr_info[WLC_TWT_MAX_RLM_USR];
} wlc_twt_rlm_t;

#define TSB_USR_TRIG_SHIFT	0
#define TSB_USR_ANCD_SHIFT	1
#define TSB_USR_SPDUR_MASK	0xf0
#define TSB_USR_SPDUR_SHIFT	4
#define TSB_USR_IDX_MASK	0xff00
#define TSB_USR_IDX_SHIFT	8

#define TSB_USR_SPDUR(x) ((x & TSB_USR_SPDUR_MASK) >> TSB_USR_SPDUR_SHIFT)
#define TSB_USR_IDX(x) ((x & TSB_USR_IDX_MASK) >> TSB_USR_IDX_SHIFT)

/* TWT Information related defines: */
#define TWT_INFO_ALL_FLOW_ID		0xff
#define TWT_INFO_SIZE_INFO		1
#define TWT_INFO_NEXTTWT_SIZE_1		1
#define TWT_INFO_NEXTTWT_SIZE_1_BITS	32
#define TWT_INFO_NEXTTWT_SIZE_2		2
#define TWT_INFO_NEXTTWT_SIZE_2_BITS	48
#define TWT_INFO_NEXTTWT_SIZE_3		3
#define TWT_INFO_NEXTTWT_SIZE_3_BITS	64

#define WLC_TWT_MAX_WAIT4PSMWD		100000	/* usec */

/**
 * There are two types of TWT schedules. Bcast and Individual.  Bcast schedules are maintained
 * in the bsscfg cubby for both AP and STA, sharing the storage. Individual TWT schedules are
 * stored in the scb specific config.
 */
/** Descriptor for storing Bcast TWT info specific to each BSSCFG */
typedef struct twt_bcast_desc {
	uint8		state;			/* flags WLC_TWT_STATE_... */
	uint8		teardown_count;		/* noof beacons to program teardown bcast ie */
	wl_twt_sdesc_t	desc;			/* Bcast TWT parameters */
	uint32		twt_l;			/* TSF low 32 bit for next SP after beacon */
	uint32		twt_h;			/* TSF high 32 bit for next SP after beacon */
} twt_bcast_desc_t;

/** per BSS TWT specific info */
typedef struct twt_bss {
	/* STA */
	uint8		*btwt_ie;		/* Dynamically allocated last recvd IE */
	uint8		btwt_ie_len;		/* Length of the last received IE */
	/* STA/AP */
	uint8		bid_count;		/* Number of desriptors in use */
	/* An AP can send up to WL_TWT_ID_BCAST_MAX schedules. We have to store them all if we
	 * are a STA. So even if we limit the support at AP side regarding number of schedules
	 * we still need to be able to store all the information as STA. Now theoretically we
	 * can make storage a bit smaller because the IE carrying the bcast information can only
	 * be 256 long and each bcast twt schedule is 9 bytes so only 28 max, but then we cant
	 * do bcast id mapping straight in array. That would require a lot of lookup code. Can do
	 * it later to save a couple of bytes, but question is if extra code would defeat
	 * that optimisation
	 */
	twt_bcast_desc_t bcast[WL_TWT_ID_BCAST_MAX + 1];
} twt_bss_t;

typedef struct twt_indv_desc {
	uint8		state;			/* flags WLC_TWT_STATE_... */
	bool		paused;			/* TWT information received */
	bool		resuming;		/* TWT information indicating resume received */
	uint32		resume_twt_l;		/* TSF at which resume should take part low 32 */
	uint32		resume_twt_h;		/* TSF at which resume should take part high 32 */
	wl_twt_sdesc_t	desc;			/* Individual TWT parameters */
	uint32		twt_l;			/* TSF low 32 bit for next SP */
	uint32		twt_h;			/* TSF high 32 bit for next SP */
	uint32		wake_interval;		/* Calculated interval of SP */
	uint16		wake_duration;		/* Calculated duration of SP (in schedid) */
	uint8		trigger;		/* Calculated trigger value (1 | 0) */
	bool		tx_closed;		/* Used for announced traffic */
	wlc_hrt_to_t	*tx_close_timer;	/* Used for announced traffic */
	scb_t		*scb;			/* Timer needs to be able to map back to SCB */
	uint8		teardown_retry_cnt;	/* How many times to retry teardown */
	uint32		cnt_sp;			/* Counter; in/out of SP */
	uint32		cnt_rx_trig;		/* Counter; rx trigggers (for announced) */
	uint32		cnt_paused;		/* Counter; TWT link paused */
	uint32		cnt_tx_pkt;		/* Counter; Tx packets scheduled for this link */
	uint32		cnt_sp_missed;		/* Counter; Missed SP completely (late timer int) */
	uint32		cnt_sp_missed_by1;	/* Counter; Missed SP by 1 (late timer int) */
	uint32		cnt_pretwt_late_sp_closed; /* Counter; Missed SP by late pre TWT */
#if defined(BCMDBG) || defined(DUMP_TWT)
	uint32		cnt_pretwt;
	int32		cnt_avg_pretwt;
	int32		cnt_min_pretwt;
	int32		cnt_max_pretwt;
	uint32		cnt_tx_open_tsf_l;
	uint32		cnt_tx_open_tsf_h;
	uint32		cnt_tx_latency;
	int32		cnt_avg_tx_latency;
	int32		cnt_min_tx_latency;
	int32		cnt_max_tx_latency;
#endif // endif
} twt_indv_desc_t;

/** per SCB TWT specific info */
typedef struct twt_scb {
	uint8		cap;		/* Capabilities supported by remote, as set in IE */
	uint8		af_state;	/* Action Frame pending/awaited, WLC_TWT_AF_PEND/AWAIT_.. */
	uint8		af_teardown;	/* Teardown AF information */
	uint8		af_fid;		/* Flow ID for which AF was sent */
	uint8		af_timeout;	/* AF state protection counter */
	uint8		af_dialog_token; /* dialog token in use for AF series */
	uint8		af_retry_count;	/* how many times to retry AF */
	bool		apps_prepared;	/* Has APPS been prepared for TWT. */
	twt_indv_desc_t	indv[WLC_TWT_MAX_ITWT_SCB];
	uint8		indv_active_cnt; /* Active Individual TWT connections */
	uint8		indv_paused_cnt; /* Paused Individual TWT connections */
	uint16		rlm_id;		/* Ratelink mem index */
	struct twt_scb	*next;		/* Linked list of active twt_scb's */
	uint16		cur_sp_start;	/* current sched ID of SP start to program to txd */
	bool		sp_started;	/* Has the first SP for this link been put in RLM */
} twt_scb_t;

/**
 * TWT module info
 * Holds the usual stuff. Generic/Global is the dialog_token. This is unique token used for
 * Action Frames (AF). This token is globally maintained and for each new sequence the value of
 * dialog_token should be used as the token and the value should be increased (auto wrap on 255)
 */
struct wlc_twt_info {
	wlc_info_t	*wlc;
	int		bssh;
	int		scbh;

	uint8		dialog_token;		/* Global dialog token, increase on usage */
	int		itwt_count;		/* Total number of active iTWT connections */
	int		itwt_trig_count;	/* Total number of active trigger-enabled iTWTs */
	bool		rlm_scheduler_active;
	int		rlm_prog_idx;		/* Next index of RLM to program next schedule */
	int		rlm_read_idx;		/* Next index of RLM to process interrupt for */
	wlc_hrt_to_t	*rlm_scheduler_timer;	/* Delayed scheduler timer */
	twt_scb_t	*scb_twt_list;
	wlc_twt_rlm_t	twtschedblk[AMT_IDX_TWT_RSVD_SIZE];
#ifdef DEBUG_TWT_PS
	wlc_hrt_to_t	*test_timer;
#endif /* DEBUG_TWT_PS */
	uint32		cnt_engine_started;	/* Counter; UCODE engine started */
	uint32		cnt_sp_missed_almost;	/* Counter; Missed SP almost, late by 1 schedid */
	uint32		cnt_pretwt_late;	/* Counter; intr handler is very delayed */
};

/* macros to access bsscfg cubby & data */
#define TWT_BSS_CUBBY(twti, cfg)	(twt_bss_t **)BSSCFG_CUBBY(cfg, (twti)->bssh)
#define TWT_BSS(twti, cfg)		(((twti)->bssh) ? *TWT_BSS_CUBBY(twti, cfg) : NULL)

/* macros to access scb cubby & data */
#define TWT_SCB_CUBBY(twti, scb)	(twt_scb_t **)SCB_CUBBY(scb, (twti)->scbh)
#define TWT_SCB(twti, scb)		(((twti)->scbh) ? *TWT_SCB_CUBBY(twti, scb) : NULL)

/* ======== local function declarations ======== */

/* wlc module */
static int wlc_twt_doiovar(void *ctx, uint32 actionid, void *params, uint plen,
	void *arg, uint alen, uint vsize, struct wlc_if *wlcif);
#if defined(BCMDBG) || defined(DUMP_TWT)
static int wlc_twt_dump(void *ctx, struct bcmstrbuf *b);
static int wlc_twt_dump_clr(void *ctx);
#endif // endif
static int wlc_twt_wlc_up(void *ctx);

/* bsscfg cubby */
static int wlc_twt_bss_init(void *ctx, wlc_bsscfg_t *cfg);
static void wlc_twt_bss_deinit(void *ctx, wlc_bsscfg_t *cfg);
#if defined(BCMDBG) || defined(DUMP_TWT)
static void wlc_twt_bss_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b);
#else
#define wlc_twt_bss_dump NULL
#endif // endif

/* scb cubby */
static uint wlc_twt_scb_secsz(void *ctx, scb_t *scb);
static int wlc_twt_scb_init(void *ctx, scb_t *scb);
static void wlc_twt_scb_deinit(void *ctx, scb_t *scb);
static void wlc_twt_scb_dump(void *ctx, scb_t *scb, struct bcmstrbuf *b);
static void wlc_twt_scb_state_upd_cb(void *ctx, scb_state_upd_data_t *notif_data);

/* Beacone IE mgmt hooks */
#ifdef AP
static uint wlc_twt_calc_bcast_ie_len(void *ctx, wlc_iem_calc_data_t *data);
static int wlc_twt_write_bcast_ie(void *ctx, wlc_iem_build_data_t *data);
#endif /* AP */
#ifdef STA
static int wlc_twt_parse_btwt_ie(void *ctx, wlc_iem_parse_data_t *data);
#endif /* STA */
static void wlc_twt_get_tsf(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint32 *tsf_l, uint32 *tsf_h);
static void wlc_twt_ie_parse_single_bcast_ie(twt_ie_bcast_t *bcast_ie, wl_twt_sdesc_t *desc);
static int wlc_twt_ap_btwt_config(wlc_twt_info_t *twti, wlc_bsscfg_t *cfg, wl_twt_sdesc_t *desc);
static int wlc_twt_ap_btwt_teardown(wlc_twt_info_t *twti, wlc_bsscfg_t *cfg, uint8 bid);
static int wlc_twt_bid_first_free(twt_bss_t *bss_twt, uint8 *bid);
static uint wlc_twt_ie_add_single_bcast_ie(uint8 *body, twt_bcast_desc_t *bcast,
	uint8 setup_command, uint16 twt);
static void wlc_twt_af_txcomplete(wlc_info_t *wlc, void *pkt, uint txstatus);
static twt_indv_desc_t *wlc_twt_find_fid(twt_scb_t *scb_twt, uint8 fid);
static int wlc_twt_teardown_send(wlc_twt_info_t *twti, wlc_bsscfg_t *cfg, scb_t *scb,
	bool bcast, uint8 id);
static int wlc_twt_teardown(wlc_twt_info_t *twti, scb_t *scb, uint8 af_teardown, bool enter_pm);
static int wlc_twt_teardown_retry(wlc_twt_info_t *twti, scb_t *scb, uint8 af_teardown);
static int wlc_twt_sta_itwt_req_send(wlc_twt_info_t *twti, scb_t *scb, wl_twt_sdesc_t *setup);
static int wlc_twt_sta_btwt_req_send(wlc_twt_info_t *twti, scb_t *scb, wl_twt_sdesc_t *setup);
static void wlc_twt_scb_timeout_cnt(wlc_info_t *wlc, scb_t *scb);
static int wlc_twt_information(wlc_twt_info_t *twti, scb_t *scb, uint8 flow_id, bool resume,
	uint16 fc, uint32 twt_l, uint32 twt_h);
static int wlc_twt_teardown_btwt(wlc_twt_info_t *twti, scb_t *scb, uint8 bid, bool enter_pm);

static int wlc_twt_teardown_itwt(wlc_twt_info_t *twti, scb_t *scb, twt_indv_desc_t *indv,
	bool enter_pm);
static int wlc_twt_itwt_start(wlc_twt_info_t *twti, scb_t *scb, twt_indv_desc_t *indv);
static int wlc_twt_itwt_stop(wlc_twt_info_t *twti, scb_t *scb, twt_indv_desc_t *indv,
	bool enter_pm);
static void wlc_twt_prepare_for_first_sp(wlc_twt_info_t *twti, twt_indv_desc_t *indv);
static void wlc_twt_sp_resume(wlc_twt_info_t *twti, twt_indv_desc_t *indv);
static void wlc_twt_sp_pause(wlc_twt_info_t *twti, scb_t *scb, twt_indv_desc_t *indv, uint16 fc);
static void wlc_twt_itwt_end_sp(void *arg);

#ifdef DEBUG_TWT_PS
static void wlc_twt_test_trigger(void *arg);
#endif /* DEBUG_TWT_PS */

enum {
	IOV_TWT		= 0,
	IOV_TWT_PREINTR	= 1,
	IOV_TWT_PRESTRT	= 2,
	IOV_TWT_PRESTOP	= 3,
	IOV_LAST
};

static const bcm_iovar_t twt_iovars[] = {
	{ "twt", IOV_TWT, (0), 0, IOVT_BUFFER, 0 },
	{ "twt_preintr", IOV_TWT_PREINTR, (0), 0, IOVT_BUFFER, 0 },
	{ "twt_prestrt", IOV_TWT_PRESTRT, (0), 0, IOVT_BUFFER, 0 },
	{ "twt_prestop", IOV_TWT_PRESTOP, (0), 0, IOVT_BUFFER, 0 },
	{ NULL, 0, 0, 0, 0, 0 }
};

/*  TWT cmds  */
static int wlc_twt_cmd_enab(void *ctx, uint8 *params, uint16 plen, uint8 *result,
	uint16 *rlen, bool set, wlc_if_t *wlcif);
static int wlc_twt_cmd_setup(void *ctx, uint8 *params, uint16 plen, uint8 *result,
	uint16 *rlen, bool set, wlc_if_t *wlcif);
static int wlc_twt_cmd_teardown(void *ctx, uint8 *params, uint16 plen, uint8 *result,
	uint16 *rlen, bool set, wlc_if_t *wlcif);
static int wlc_twt_cmd_list(void *ctx, uint8 *params, uint16 plen, uint8 *result,
	uint16 *rlen, bool set, wlc_if_t *wlcif);

static const wlc_iov_cmd_t twt_cmds[] = {
	{ WL_TWT_CMD_ENAB, IOVF_SET_DOWN, IOVT_UINT8, wlc_twt_cmd_enab },
	{ WL_TWT_CMD_SETUP, IOVF_SET_UP, IOVT_BUFFER, wlc_twt_cmd_setup },
	{ WL_TWT_CMD_TEARDOWN, IOVF_SET_UP, IOVT_BUFFER, wlc_twt_cmd_teardown },
	{ WL_TWT_CMD_LIST, IOVF_SET_UP, IOVT_BUFFER, wlc_twt_cmd_list },
};

/* Module specific functions: attach and detach. */
wlc_twt_info_t *
BCMATTACHFN(wlc_twt_attach)(wlc_info_t *wlc)
{
	wlc_twt_info_t *twti;
	uint16 build_twtfstbmp = FT2BMP(FC_BEACON);
	scb_cubby_params_t cubby_params;

	/* allocate private module info */
	if ((twti = MALLOCZ(wlc->osh, sizeof(*twti))) == NULL) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n", wlc->pub->unit,
			__FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}
	twti->wlc = wlc;

	/* TWT is only supported when HW supports HE, check HW capabilities */
	if (!WLC_HE_CAP_PHY(wlc)) {
		WL_INFORM(("wl%d: %s: TWT functionality disabled \n", wlc->pub->unit,
			__FUNCTION__));
		return twti;
	}

	/* register module up/down, watchdog, and iovar callbacks */
	if (wlc_module_register(wlc->pub, twt_iovars, "twt", twti, wlc_twt_doiovar,
			NULL, wlc_twt_wlc_up, NULL) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register failed\n", wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

#if defined(BCMDBG) || defined(DUMP_TWT)
	/* debug dump */
	wlc_dump_add_fns(wlc->pub, "twt", wlc_twt_dump, wlc_twt_dump_clr, twti);
#endif // endif

	/* reserve some space in bsscfg for private data */
	twti->bssh = wlc_bsscfg_cubby_reserve(wlc, sizeof(twt_bss_t *),
		wlc_twt_bss_init, wlc_twt_bss_deinit, wlc_twt_bss_dump, twti);
	if (twti->bssh < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve failed\n", wlc->pub->unit,
			__FUNCTION__));
		goto fail;
	}

	/* reserve some space in scb for private data */
	bzero(&cubby_params, sizeof(cubby_params));

	cubby_params.context = twti;
	cubby_params.fn_init = wlc_twt_scb_init;
	cubby_params.fn_deinit = wlc_twt_scb_deinit;
	cubby_params.fn_secsz = wlc_twt_scb_secsz;
	cubby_params.fn_dump = wlc_twt_scb_dump;
	twti->scbh = wlc_scb_cubby_reserve_ext(wlc, sizeof(twt_scb_t *),
		&cubby_params);
	if (twti->scbh < 0) {
		WL_ERROR(("wl%d: %s: wlc_scb_cubby_reserve_ext failed %d\n", wlc->pub->unit,
			__FUNCTION__, twti->scbh));
		goto fail;
	}

#ifdef AP
	/* Beacon */
	if (wlc_iem_add_build_fn_mft(wlc->iemi, build_twtfstbmp, DOT11_MNG_TWT_ID,
			wlc_twt_calc_bcast_ie_len, wlc_twt_write_bcast_ie, twti) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_build_fn failed, twt bcast ie\n", wlc->pub->unit,
			__FUNCTION__));
		goto fail;
	}
	/* Add client callback to the scb state notification list */
	if (wlc_scb_state_upd_register(wlc, wlc_twt_scb_state_upd_cb, wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s: unable to register callback\n", wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#endif /* AP */

#ifdef STA
	/* Beacon */
	if (wlc_iem_add_parse_fn_mft(wlc->iemi, build_twtfstbmp, DOT11_MNG_TWT_ID,
			wlc_twt_parse_btwt_ie, twti) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_parse_fn failed, twt bcast ie\n", wlc->pub->unit,
			__FUNCTION__));
		goto fail;
	}
#endif /* STA */

	/* Register packet class callback, used for Action Frames */
	if (wlc_pcb_fn_set(wlc->pcb, 0, WLF2_PCB1_TWT_AF, wlc_twt_af_txcomplete) != BCME_OK) {
		goto fail;
	}

	wlc->pub->_twt = TRUE;

	return twti;
fail:
	wlc_twt_detach(twti);
	return NULL;
}

void
BCMATTACHFN(wlc_twt_detach)(wlc_twt_info_t *twti)
{
	wlc_info_t *wlc;

	if (twti == NULL)
		return;

	wlc = twti->wlc;

	if (twti->rlm_scheduler_timer) {
		wlc_hrt_free_timeout(twti->rlm_scheduler_timer);
	}

	wlc_scb_state_upd_unregister(wlc, wlc_twt_scb_state_upd_cb, wlc);
	wlc_module_unregister(wlc->pub, "twt", twti);

	MFREE(wlc->osh, twti, sizeof(*twti));
}

#if defined(BCMDBG) || defined(DUMP_TWT)
/* debug dump */
static int
wlc_twt_dump(void *ctx, struct bcmstrbuf *b)
{
	wlc_twt_info_t *twti = (wlc_twt_info_t *)ctx;
	wlc_info_t *wlc = twti->wlc;
	wlc_bsscfg_t *bsscfg;
	twt_scb_t *scb_twt;
	twt_indv_desc_t *indv;
	uint32 tsf_h, tsf_l;
	int i;

	bcm_bprintf(b, "TWT Enab: %d\n", TWT_ENAB(wlc->pub));

	if (!TWT_ENAB(wlc->pub)) {
		return BCME_OK;
	}
	bcm_bprintf(b, "UCODE engine enabled: %d, start count: %d\n",
		twti->rlm_scheduler_active, twti->cnt_engine_started);
	bcm_bprintf(b, "Almost missed SP scheduleding=%d (interrupt delayed by 1 schedid)\n",
		twti->cnt_sp_missed_almost);
	bcm_bprintf(b, "Pre TWT interrupt very delayed count=%d\n",
		twti->cnt_pretwt_late);
	if (wlc->clk) {
		wlc_twt_get_tsf(wlc, wlc->primary_bsscfg, &tsf_l, &tsf_h);
	} else {
		tsf_h = 0;
		tsf_l = 0;
	}
	bcm_bprintf(b, "TSF=%08x:%08x (hex)\n", tsf_h, tsf_l);

	if (twti->itwt_count) {
		bcm_bprintf(b, "\nNumber of active individual TWT sessions: %d\n",
			twti->itwt_count);
		scb_twt = twti->scb_twt_list;
		ASSERT(scb_twt);
		while (scb_twt) {
			for (i = 0; i < ARRAYSIZE(scb_twt->indv); i++) {
				if (scb_twt->indv[i].state != WLC_TWT_STATE_ACTIVE) {
					continue;
				}
				indv = &scb_twt->indv[i];
				bcm_bprintf(b, "------------------------\n");
				bcm_bprintf(b, "Individual TWT for SCB %p, AID %d, MAC " MACF
					" RLM Id %d\n",
					indv->scb, SCB_AID(indv->scb),
					ETHERP_TO_MACF(&indv->scb->ea), scb_twt->rlm_id);
				bcm_bprintf(b, "TWT=%08x:%08x (hex), next schedid=%x\n",
					indv->twt_h, indv->twt_l, WLC_TSFL_TO_SCHEDID(indv->twt_l));
				bcm_bprintf(b, "ID=%d Interval=%d/%d (m/e) =%d (usec) Duration=%d "
					"(usec) Unannounced=%d Trigger=%d\n",
					indv->desc.id, indv->desc.wake_interval_mantissa,
					indv->desc.wake_interval_exponent,
					indv->desc.wake_interval_mantissa *
					(1 << indv->desc.wake_interval_exponent),
					256 * indv->desc.wake_duration,
					(indv->desc.flow_flags & WL_TWT_FLOW_FLAG_UNANNOUNCED) ?
					1 : 0,
					(indv->desc.flow_flags & WL_TWT_FLOW_FLAG_TRIGGER) ? 1 : 0);
				bcm_bprintf(b, "Count SP=%d Rx trig=%d Paused=%d Tx pkt=%d\n",
					indv->cnt_sp, indv->cnt_rx_trig, indv->cnt_paused,
					indv->cnt_tx_pkt);
				bcm_bprintf(b, "      SP missed=%d missed by 1=%d "
					"(late interrupt)\n",
					indv->cnt_sp_missed, indv->cnt_sp_missed_by1);
				bcm_bprintf(b, "      SP missed by late pre TWT interrupt count=%d"
					"\n", indv->cnt_pretwt_late_sp_closed);
				if (indv->cnt_pretwt) {
					if (indv->desc.flow_flags & WL_TWT_FLOW_FLAG_UNANNOUNCED) {
						bcm_bprintf(b, "      PreTWT avg(last 16)=%d min/"
							"max=%d/%d count=%d\n",
							(indv->cnt_pretwt > 16) ?
							indv->cnt_avg_pretwt / 16 :
							indv->cnt_avg_pretwt /
								(int32)indv->cnt_pretwt,
							indv->cnt_min_pretwt, indv->cnt_max_pretwt,
							indv->cnt_pretwt);
					} else {
						bcm_bprintf(b, "      Rx avg(last 16)=%d min/"
							"max=%d/%d count=%d\n",
							(indv->cnt_pretwt > 16) ?
							indv->cnt_avg_pretwt / 16 :
							indv->cnt_avg_pretwt /
								(int32)indv->cnt_pretwt,
							indv->cnt_min_pretwt, indv->cnt_max_pretwt,
							indv->cnt_pretwt);
					}
				}
				if (indv->cnt_tx_latency) {
					bcm_bprintf(b, "      Tx latency(last 16)=%d min/"
						"max=%d/%d count=%d\n",
						(indv->cnt_tx_latency > 16) ?
						indv->cnt_avg_tx_latency / 16 :
						indv->cnt_avg_tx_latency /
							(int32)indv->cnt_tx_latency,
						indv->cnt_min_tx_latency, indv->cnt_max_tx_latency,
						indv->cnt_tx_latency);
				}
			}
			scb_twt = scb_twt->next;
		}
	}

	FOREACH_BSS(wlc, i, bsscfg) {
		if (BSSCFG_AP(bsscfg)) {
			bcm_bprintf(b, "------------------------\n");
			bcm_bprintf(b, "Broadcast TWT for Bsscfg %p:\n", bsscfg);
			wlc_twt_bss_dump(twti, bsscfg, b);
		}
	}

	return BCME_OK;
}

static int
wlc_twt_dump_clr(void *ctx)
{
	wlc_twt_info_t *twti = (wlc_twt_info_t *)ctx;
	twt_scb_t *scb_twt;
	twt_indv_desc_t *indv;
	int i;

	if (twti->itwt_count) {
		scb_twt = twti->scb_twt_list;
		ASSERT(scb_twt);
		while (scb_twt) {
			for (i = 0; i < ARRAYSIZE(scb_twt->indv); i++) {
				if (scb_twt->indv[i].state != WLC_TWT_STATE_ACTIVE) {
					continue;
				}
				indv = &scb_twt->indv[i];
				indv->cnt_avg_pretwt = 0;
				indv->cnt_min_pretwt = 0;
				indv->cnt_max_pretwt = 0;
				indv->cnt_pretwt = 0;
				indv->cnt_sp = 0;
				indv->cnt_tx_latency = 0;
				indv->cnt_avg_tx_latency = 0;
				indv->cnt_min_tx_latency = 0;
				indv->cnt_max_tx_latency = 0;
				indv->cnt_rx_trig = 0;
				indv->cnt_paused = 0;
				indv->cnt_tx_pkt = 0;
				indv->cnt_sp_missed = 0;
				indv->cnt_sp_missed_by1 = 0;
				indv->cnt_pretwt_late_sp_closed = 0;
			}
			scb_twt = scb_twt->next;
		}
	}
	return BCME_OK;
}

#endif // endif

void
wlc_twt_dump_schedblk(wlc_info_t *wlc)
{
	wlc_twt_info_t *twti = wlc->twti;
	int i, j;

	if (!TWT_ENAB(wlc->pub)) {
		return;
	}
	for (j = 0; j < ARRAYSIZE(twti->twtschedblk); j++) {
		WL_PRINT(("%d) usr_count = %d, schedid = %d\n", j,
			twti->twtschedblk[j].usr_count,
			twti->twtschedblk[j].schedid));
		for (i = 0; i < twti->twtschedblk[j].usr_count; i++) {
			WL_PRINT(("	[%d].id = %d dur = %d\n", i,
				(twti->twtschedblk[j].usr_info[i] & TSB_USR_IDX_MASK)
				>> TSB_USR_IDX_SHIFT,
				((twti->twtschedblk[j].usr_info[i] & TSB_USR_SPDUR_MASK)
				>> TSB_USR_SPDUR_SHIFT) + 1));
		}
	}
}

/**
 * This function returns TRUE if all requirements are met to turn on TWT, otherwise FALSE
 * is returned. To be used at initial phase to determine if TWT is on or not and by enab
 * cmd to see if TWT can be enabled.
 */
static bool
wlc_twt_allowed(wlc_info_t *wlc)
{
	/* Currently HW which supports 11ax and which has 11ax enabled is the only type of devices
	 * for which we should return TRUE. TWT without 11ax is not supported.
	 */
	return (HE_ENAB(wlc->pub) ? TRUE : FALSE);
}

/**
 * Disable TWT. Set enab to false and remove any configuration if necessary, should only be
 * called when down. This function is to be called by external modules if there is something
 * changed which would require TWT to be disabled (e.g. 11ax enab is FALSE).
 */
void
wlc_twt_disable(wlc_info_t *wlc)
{
	ASSERT(!wlc->pub->up);
	/* Set enab to false */
	wlc->pub->_twt = FALSE;
}

/**
 * Is this device/connection TWT Requestor capable. return TRUE if so. This is the function
 * to be used by 11ax module to determine capability of the device.
 */
bool
wlc_twt_req_cap(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	/* TWT requester functionality is currently only supported for STA mode */
	if (TWT_ENAB(wlc->pub) && BSSCFG_STA(cfg)) {
		return TRUE;
	}
	return FALSE;
}

/**
 * Is this device/connection TWT Responder capable. return TRUE if so. This is the function
 * to be used by 11ax module to determine capability of the device.
 */
bool
wlc_twt_resp_cap(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	/* TWT responder functionality is currently only supported for AP mode */
	if (TWT_ENAB(wlc->pub) && BSSCFG_AP(cfg)) {
		return TRUE;
	}
	return FALSE;
}

/**
 * Is this device/connection Broadcast TWT capable. return TRUE if so. This is the function
 * to be used by 11ax module to determine capability of the device.
 */
bool
wlc_twt_bcast_cap(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	/* return not supported for the moment. Once implemented this can be turned on */
	return (FALSE);
}

/* ======== bsscfg module hooks ======== */

static int
wlc_twt_bss_init(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_twt_info_t *twti = (wlc_twt_info_t *)ctx;
	wlc_info_t *wlc = twti->wlc;
	twt_bss_t **pbss_twt = TWT_BSS_CUBBY(twti, cfg);
	twt_bss_t *bss_twt;

	if ((bss_twt = MALLOCZ(wlc->osh, sizeof(*bss_twt))) == NULL) {
		WL_ERROR(("wl%d: %s: mem alloc failed, allocated %d bytes\n", wlc->pub->unit,
			__FUNCTION__, MALLOCED(wlc->osh)));
		return BCME_NOMEM;
	}

	*pbss_twt = bss_twt;

	return BCME_OK;
}

static void
wlc_twt_bss_deinit(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_twt_info_t *twti = (wlc_twt_info_t *)ctx;
	wlc_info_t *wlc = twti->wlc;
	twt_bss_t **pbss_twt = TWT_BSS_CUBBY(twti, cfg);
	twt_bss_t *bss_twt = TWT_BSS(twti, cfg);

	/* sanity check */
	if (bss_twt == NULL) {
		WL_ERROR(("wl%d: %s: BSS cubby is NULL!\n", wlc->pub->unit, __FUNCTION__));
		return;
	}

	if (bss_twt->btwt_ie) {
		MFREE(wlc->osh, bss_twt->btwt_ie, bss_twt->btwt_ie_len);
	}

	MFREE(wlc->osh, bss_twt, sizeof(*bss_twt));
	*pbss_twt = NULL;
}

#if defined(BCMDBG) || defined(DUMP_TWT)
static void
wlc_twt_bss_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b)
{
	wlc_twt_info_t *twti = (wlc_twt_info_t *)ctx;
	twt_bcast_desc_t *bcast;
	twt_bss_t *bss_twt = TWT_BSS(twti, cfg);
	uint8 i;

	if (bss_twt == NULL) {
		bcm_bprintf(b, "    BSS cubby is NULL!\n");
		return;
	}
	/* Dump the list of bcast TWT configurations */
	for (i = 0; i < WLC_TWT_MAX_BTWT; i++) {
		bcast = &bss_twt->bcast[i];
		if (bcast->state != WLC_TWT_STATE_INACTIVE) {
			bcm_bprintf(b, "    Bcast ID %u>\n", bcast->desc.id);
			bcm_bprintf(b, "    flow flags: 0x%x\n",
				bcast->desc.flow_flags);
			bcm_bprintf(b, "    nominal min wake duration: 0x%x\n",
				bcast->desc.wake_duration);
			bcm_bprintf(b, "    wake interval: 0x%x 0x%0x\n\n\n",
				bcast->desc.wake_interval_mantissa,
				bcast->desc.wake_interval_exponent);
		}
	}
}
#endif // endif

/* ======== scb module hooks ======== */
static uint
wlc_twt_scb_secsz(void *ctx, scb_t *scb)
{
	uint size = 0;
	if (scb && !SCB_INTERNAL(scb)) {
		size = sizeof(twt_scb_t);
	}
	return size;
}

static int
wlc_twt_scb_init(void *ctx, scb_t *scb)
{
	wlc_twt_info_t *twti = (wlc_twt_info_t *)ctx;
	wlc_info_t *wlc = twti->wlc;
	twt_scb_t **pscb_twt = TWT_SCB_CUBBY(twti, scb);

	*pscb_twt = wlc_scb_sec_cubby_alloc(wlc, scb, wlc_twt_scb_secsz(ctx, scb));

	return BCME_OK;
}

static void
wlc_twt_scb_deinit(void *ctx, scb_t *scb)
{
	wlc_twt_info_t *twti = (wlc_twt_info_t *)ctx;
	wlc_info_t *wlc = twti->wlc;
	twt_scb_t **pscb_twt = TWT_SCB_CUBBY(twti, scb);
	twt_scb_t *scb_twt = TWT_SCB(twti, scb);
	uint8 i;

	/* sanity check */
	if (scb_twt == NULL) {
		return;
	}

	for (i = 0; i < ARRAYSIZE(scb_twt->indv); i++) {
		if (scb_twt->indv[i].state == WLC_TWT_STATE_ACTIVE) {
			wlc_twt_teardown_itwt(twti, scb, &scb_twt->indv[i], FALSE);
		}
	}

	wlc_scb_sec_cubby_free(wlc, scb, scb_twt);
	*pscb_twt = NULL;
}

static void
wlc_twt_scb_dump(void *ctx, scb_t *scb, struct bcmstrbuf *b)
{
	wlc_twt_info_t *twti = (wlc_twt_info_t *)ctx;
	twt_scb_t *scb_twt = TWT_SCB(twti, scb);

	if (scb_twt == NULL)
		return;

	BCM_REFERENCE(b);
}

/**
 * TWT Capability set/get APIs, set is called by HE module and therefor exported, get is only
 * used internally to determine if remote side has the necessary capabilities set.
 */
void
wlc_twt_scb_set_cap(wlc_twt_info_t *twti, scb_t *scb, uint8 cap_idx, bool set)
{
	twt_scb_t *scb_twt = TWT_SCB(twti, scb);
	if (scb_twt == NULL) {
		WL_ERROR(("wl%d: %s: SCB cubby is NULL\n", twti->wlc->pub->unit, __FUNCTION__));
		return;
	}

	if (set) {
		setbit((uint8 *)&scb_twt->cap, cap_idx);
	} else {
		clrbit((uint8 *)&scb_twt->cap, cap_idx);
	}
}

static void wlc_twt_scb_state_upd_cb(void *ctx, scb_state_upd_data_t *notif_data)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;
	wlc_twt_info_t *twti = wlc->twti;
	struct scb *scb;
	uint8 oldstate;
	twt_scb_t *scb_twt;
	uint8 i;

	ASSERT(notif_data != NULL);

	scb = notif_data->scb;
	ASSERT(scb != NULL);
	oldstate = notif_data->oldstate;

	if (BSSCFG_AP(scb->bsscfg) && (oldstate & ASSOCIATED) && !SCB_ASSOCIATED(scb)) {
		WL_TWT(("%s: TWT update needed as SCB is disassociated\n", __FUNCTION__));

		if ((scb_twt = TWT_SCB(twti, scb)) == NULL) {
			return;
		}
		for (i = 0; i < ARRAYSIZE(scb_twt->indv); i++) {
			if (scb_twt->indv[i].state != WLC_TWT_STATE_INACTIVE) {
				wlc_twt_teardown_itwt(twti, scb, &scb_twt->indv[i], FALSE);
			}
		}
	}
}

/**
 * Retrieve remote or local (based on bsscfg) TSF. If we ever want to support this in STA mode with
 * P2P  also enabled then we need to use mcnx version, but for now we are good by using main TSF
 * as it will be synced to beacon in case of STA mode,
 */
static void
wlc_twt_get_tsf(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint32 *tsf_l, uint32 *tsf_h)
{
	wlc_read_tsf(wlc, tsf_l, tsf_h);
}

/* ======== iovar dispatch ======== */

static int
wlc_twt_ucode_param_get(wlc_info_t *wlc, uint16 offset, uint16 *val)
{
	if (!TWT_ENAB(wlc->pub)) {
		return BCME_UNSUPPORTED;
	}
	if (!wlc->clk) {
		return BCME_NOCLK;
	}
	*val = wlc_read_shm(wlc, offset);
#if defined(WL_PSMX)
	if (offset == M_TWT_PRESTRT(wlc) && *val != wlc_read_shmx(wlc, MX_TWT_PRESTRT(wlc))) {
		return BCME_ERROR;
	}
#endif // endif

	return BCME_OK;
}

static int
wlc_twt_ucode_param_set(wlc_info_t *wlc, uint16 offset, uint16 val)
{
	if (!TWT_ENAB(wlc->pub)) {
		return BCME_UNSUPPORTED;
	}
	if (!wlc->clk) {
		return BCME_NOCLK;
	}
	wlc_write_shm(wlc, offset, (uint16)val);
#if defined(WL_PSMX)
	if (offset == M_TWT_PRESTRT(wlc)) {
		wlc_write_shmx(wlc, MX_TWT_PRESTRT(wlc), val);
	}
#endif // endif
	return BCME_OK;
}

static int
wlc_twt_doiovar(void *ctx, uint32 actionid, void *params, uint plen,
	void *arg, uint alen, uint vsize, struct wlc_if *wlcif)
{
	wlc_twt_info_t *twti = ctx;
	wlc_info_t *wlc = twti->wlc;
	int32 int_val = 0;
	int32 *ret_int_ptr = (int32 *)arg;
	uint16 tmp_val = -1;
	int err = BCME_OK;

	BCM_REFERENCE(vsize);

	if (plen >= (int)sizeof(int_val)) {
		bcopy(params, &int_val, sizeof(int_val));
	}

	switch (actionid) {
	case IOV_GVAL(IOV_TWT):
		err = wlc_iocv_iov_cmd_proc(wlc, ctx, twt_cmds, ARRAYSIZE(twt_cmds),
			FALSE, params, plen, arg, alen, wlcif);
		break;

	case IOV_SVAL(IOV_TWT):
		err = wlc_iocv_iov_cmd_proc(wlc, ctx, twt_cmds, ARRAYSIZE(twt_cmds),
			TRUE, params, plen, arg, alen, wlcif);
		break;

	case IOV_GVAL(IOV_TWT_PREINTR):
		err = wlc_twt_ucode_param_get(wlc, M_PRETWT_US(wlc), &tmp_val);
		*ret_int_ptr = (int32)tmp_val;
		break;

	case IOV_SVAL(IOV_TWT_PREINTR):
		err = wlc_twt_ucode_param_set(wlc, M_PRETWT_US(wlc), (uint16)int_val);
		break;

	case IOV_GVAL(IOV_TWT_PRESTRT):
		err = wlc_twt_ucode_param_get(wlc, M_TWT_PRESTRT(wlc), &tmp_val);
		*ret_int_ptr = (int32)tmp_val;
		break;

	case IOV_SVAL(IOV_TWT_PRESTRT):
		err = wlc_twt_ucode_param_set(wlc, M_TWT_PRESTRT(wlc), (uint16)int_val);
		break;

	case IOV_GVAL(IOV_TWT_PRESTOP):
		err = wlc_twt_ucode_param_get(wlc, M_TWT_PRESTOP(wlc), &tmp_val);
		*ret_int_ptr = (int32)tmp_val;
		break;

	case IOV_SVAL(IOV_TWT_PRESTOP):
		err = wlc_twt_ucode_param_set(wlc, M_TWT_PRESTOP(wlc), (uint16)int_val);
		break;

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

static int
wlc_twt_cmd_enab(void *ctx, uint8 *params, uint16 plen, uint8 *result,
	uint16 *rlen, bool set, wlc_if_t *wlcif)
{
	wlc_twt_info_t *twti = ctx;
	wlc_info_t *wlc = twti->wlc;

	if (set) {
		if (!wlc_twt_allowed(wlc)) {
			return BCME_UNSUPPORTED;
		}
		if (!(*params)) {
			wlc_twt_disable(wlc);
		} else {
			wlc->pub->_twt = TRUE;
		}
	}
	else {
		*result = wlc->pub->_twt;
		*rlen = sizeof(*result);
	}

	return BCME_OK;
}

/**
 * Setup command to setup TWT schedule. There are a couple of different types of setup to
 * extract. If device is AP then BCAST schedule will define bcast schedule of the beacon IEs.
 * In all other situations it will result in twt setup. In case of non-AP device the setup may
 * result in setup of individual twt or join of bcast twt.
 */
static int
wlc_twt_cmd_setup(void *ctx, uint8 *params, uint16 plen, uint8 *result,
	uint16 *rlen, bool set, wlc_if_t *wlcif)
{
	wlc_twt_info_t *twti = ctx;
	wlc_info_t *wlc = twti->wlc;
	wl_twt_setup_t *setup = (wl_twt_setup_t *)params;
	wlc_bsscfg_t *cfg;
	scb_t *scb;
	int err = BCME_OK;

	if (!TWT_ENAB(wlc->pub) || (!set)) {
		err = BCME_UNSUPPORTED;
		goto fail;
	}

	if (setup->version != WL_TWT_SETUP_VER) {
		err = BCME_VERSION;
		goto fail;
	}

	/* lookup the bsscfg */
	cfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(cfg != NULL);

	/* If we are AP and this is bcast setup then we deal with that now. It means we should
	 * set up a new or update an existing bcast schedule and get the beacons updated with
	 * this information. No setup request is to be sent.
	 */
	if (BSSCFG_AP(cfg) && (setup->desc.flow_flags & WL_TWT_FLOW_FLAG_BROADCAST)) {
		/* Add check for capability enabled? TBD */

		return wlc_twt_ap_btwt_config(twti, cfg, &setup->desc);
	}

	if (BSSCFG_STA(cfg)) {
		if (wlc_twt_req_cap(wlc, cfg)) {
			/* Find SCB */
			scb = wlc_scbfind(wlc, cfg, &cfg->BSSID);

			if (setup->desc.flow_flags & WL_TWT_FLOW_FLAG_BROADCAST) {
				return wlc_twt_sta_btwt_req_send(twti, scb, &setup->desc);
			} else {
				return wlc_twt_sta_itwt_req_send(twti, scb, &setup->desc);
			}
		} else {
			WL_TWT(("%s Capability for TWT request not set\n", __FUNCTION__));

		}
	}

	/* No support for indv join in case of AP */
	err = BCME_UNSUPPORTED;

fail:
	if (err != BCME_OK) {
		WL_ERROR(("wl%d: %s: err=%d\n", wlc->pub->unit, __FUNCTION__, err));
	}
	return err;
}

/**
 * Teardown command to teardown either bcast or individual link
 */
static int
wlc_twt_cmd_teardown(void *ctx, uint8 *params, uint16 plen, uint8 *result, uint16 *rlen,
	bool set, wlc_if_t *wlcif)
{
	wlc_twt_info_t *twti = ctx;
	wlc_info_t *wlc = twti->wlc;
	twt_scb_t *scb_twt;
	twt_indv_desc_t *indv;
	wl_twt_teardown_t *teardown = (wl_twt_teardown_t *)params;
	wlc_bsscfg_t *cfg;
	scb_t *scb;
	struct ether_addr *addr;
	int err = BCME_OK;
	bool bcast;

	if (!TWT_ENAB(wlc->pub) || (!set)) {
		err = BCME_UNSUPPORTED;
		goto fail;
	}

	if (teardown->version != WL_TWT_TEARDOWN_VER) {
		err = BCME_VERSION;
		goto fail;
	}

	/* lookup the bsscfg */
	cfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(cfg != NULL);

	bcast = (teardown->flow_flags & WL_TWT_FLOW_FLAG_BROADCAST) ? TRUE : FALSE;
	if (BSSCFG_AP(cfg) && (bcast)) {
		return wlc_twt_ap_btwt_teardown(twti, cfg, teardown->id);
	}

	/* If we are AP, find the SCB, ignore the user supplied peer address for infra STA
	 * and use BSSID instead.
	 */
	addr = (BSSCFG_STA(cfg) && cfg->BSS) ? &cfg->BSSID : &teardown->peer;

	/* lookup the scb */
	scb = wlc_scbfind(wlc, cfg, addr);
	if (scb) {
		scb_twt = TWT_SCB(twti, scb);
		if (scb_twt == NULL) {
			err = BCME_UNSUPPORTED;
			goto fail;
		}
		indv = wlc_twt_find_fid(scb_twt, bcast ?
			WLC_TWT_BID2FID(teardown->id) : teardown->id);
		if (!indv) {
			WL_TWT(("%s No indv: Unexpected Flow ID in teardown request=%d\n",
				__FUNCTION__, teardown->id));
			err = BCME_BADARG;
			goto fail;
		}
		indv->teardown_retry_cnt = WLC_TWT_TEARDOWN_TRY;
		return wlc_twt_teardown_send(twti, cfg, scb, bcast, teardown->id);
	}

	err = BCME_BADADDR;
fail:
	if (err != BCME_OK) {
		WL_TWT(("wl%d: %s: err=%d\n", wlc->pub->unit, __FUNCTION__, err));
	}
	return err;
}

/**
 * list the twt link info. On AP it will report the configured bcast links and the individual
 * of the sta for which (if so) a mac address is provided. On non-AP this will report all bcast
 * and individual links. A flag in the info will tell if a bcast twt has been "joined"
 */
static int
wlc_twt_cmd_list(void *ctx, uint8 *params, uint16 plen, uint8 *result, uint16 *rlen,
	bool set, wlc_if_t *wlcif)
{
	wlc_twt_info_t *twti = ctx;
	wlc_info_t *wlc = twti->wlc;
	wl_twt_list_t *list_input = (wl_twt_list_t *)params;
	wl_twt_list_t *list;
	wlc_bsscfg_t *cfg;
	scb_t *scb;
	struct ether_addr *addr;
	int err = BCME_OK;
	twt_bss_t *bss_twt;
	twt_scb_t *scb_twt;
	uint16 size;
	wl_twt_sdesc_t *desc;
	uint count;
	uint i;

	size = sizeof(*list);
	list = (wl_twt_list_t *)result;
	list->version = WL_TWT_INFO_VER;

	if (!TWT_ENAB(wlc->pub) || (set)) {
		err = BCME_UNSUPPORTED;
		goto done;
	}

	if (list_input->version != WL_TWT_INFO_VER) {
		err = BCME_VERSION;
		goto done;
	}

	/* lookup the bsscfg */
	cfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(cfg != NULL);

	bss_twt = TWT_BSS(twti, cfg);
	if (bss_twt == NULL) {
		err = BCME_UNSUPPORTED;
		goto done;
	}

	/* First the bcast twt info */
	count = 0;
	for (i = WL_TWT_ID_BCAST_MIN; i < ARRAYSIZE(bss_twt->bcast); i++) {
		if (bss_twt->bcast[i].state != WLC_TWT_STATE_INACTIVE) {
			desc = &list->desc[count];
			*desc = bss_twt->bcast[i].desc;
			count++;
			size += sizeof(*desc);
		}
	}
	list->bcast_count = count;

	/* Now the individual twt info */
	list->indv_count = 0;

	/* ignore the user supplied peer address for infra STA and use BSSID instead */
	addr = (BSSCFG_STA(cfg) && cfg->BSS) ? &cfg->BSSID : &list_input->peer;

	/* To protect for assert in scb_find, check for multi ether address */
	if (ETHER_ISMULTI(addr)) {
		goto done;
	}
	/* lookup the scb */
	scb = wlc_scbfind(wlc, cfg, addr);

	/* Find the scb cubby, bail out if NULL */
	if ((scb == NULL) || ((scb_twt = TWT_SCB(twti, scb)) == NULL)) {
		goto done;
	}

	count = 0;
	for (i = 0; i < ARRAYSIZE(scb_twt->indv); i++) {
		if (scb_twt->indv[i].state != WLC_TWT_STATE_INACTIVE) {
			desc = &list->desc[count];
			*desc = scb_twt->indv[i].desc;
			count++;
			size += sizeof(*desc);
		}
	}
	list->indv_count = count;

done:
	list->length = size - sizeof(list->version) - sizeof(list->length);
	*rlen = size;

	if (err != BCME_OK) {
		WL_ERROR(("wl%d: %s: err=%d\n", wlc->pub->unit, __FUNCTION__, err));
	}
	return err;
}

/**
 * Adjust TWT parameters to be used for TWT configuration. Align them with schedid (8msec), so the
 * they can be used with TWT scheduler.
 */
static void
wlc_twt_bcast_adjust_twt_params(wlc_twt_info_t *twti, twt_bcast_desc_t *bcast)
{
	uint32 wake_interval;
	uint32 wake_duration;

	/* schedid is a time unit, see definition of WLC_SCHEDID_TSF_SHIFT, all timings are to
	 * be remapped to this timing.
	 */

	/* Wak duration is in unit of 256 usec (shift over 8) */
	wake_duration = ((uint32)bcast->desc.wake_duration << 8);
	if (wake_duration & WLC_TSFL_SCHEDID_MASK_INV) {
		wake_duration &= WLC_TSFL_SCHEDID_MASK;
		wake_duration += (1 << WLC_SCHEDID_TSF_SHIFT);
		/* Check for exceeding max wake_duration of 255 */
		if (wake_duration > (255 << 8)) {
			wake_duration -= (1 << WLC_SCHEDID_TSF_SHIFT);
		}
	}
	bcast->desc.wake_duration = wake_duration >> 8;

	wake_interval = bcast->desc.wake_interval_mantissa *
		(1 << bcast->desc.wake_interval_exponent);

	if (wake_interval & WLC_TSFL_SCHEDID_MASK_INV) {
		wake_interval &= WLC_TSFL_SCHEDID_MASK;
		if (wake_interval == 0) {
			wake_interval = (1 << WLC_SCHEDID_TSF_SHIFT);
		}
		wake_interval += (1 << WLC_SCHEDID_TSF_SHIFT);
	}
	bcast->desc.wake_interval_exponent = WLC_SCHEDID_TSF_SHIFT;
	bcast->desc.wake_interval_mantissa = wake_interval >> WLC_SCHEDID_TSF_SHIFT;

	/* It is possible that wake_duration has become wake_interval. Increase wake_interval
	 * in that case
	 */
	if (wake_duration == wake_interval) {
		wake_interval += (1 << WLC_SCHEDID_TSF_SHIFT);
	}

	/* For AP the Persistence exponent/mantissa is not really supported. It indicates
	 * a lifetime for the bcast twt link. At this point only the method "the value 255 in the
	 * Broadcast Persistence Mantissa subfield indicates that the Broadcast TWT SPs are
	 * present until explicitly terminated". Override the mantissa with 255 here.
	 */
	bcast->desc.persistence_mantissa = 255;
}

/**
 * Configure a broadcast TWT schedule in AP Beacons. The configuration can be new or an update.
 * If an id is gigen (not 0) then try to find it. If existing then update... (not supported yet)
 */
static int
wlc_twt_ap_btwt_config(wlc_twt_info_t *twti, wlc_bsscfg_t *cfg, wl_twt_sdesc_t *desc)
{
	wlc_info_t *wlc = twti->wlc;
	int err = BCME_OK;
	twt_bss_t *bss_twt;
	twt_bcast_desc_t *bcast;
	uint8 bid;
	uint32 tsf_h, tsf_l;
	uint64 next_beacon;
	uint32 bcnint_us;

	bss_twt = TWT_BSS(twti, cfg);
	if (bss_twt == NULL) {
		err = BCME_UNSUPPORTED;
		goto fail;
	}

	/* Validate the bid. It can be 0. In that case the bid gets determined here and the
	 * schedule is considered new. Otherwise check if it exists
	 */
	bid = desc->id;
	if (bid > WL_TWT_ID_BCAST_MAX) {
		err = BCME_BADARG;
		goto fail;
	}

	/* If no explicit bid used in request then get first available free bid for this
	 * bcast TWT, if bid configured, verify it is available.
	 */
	if (bid == WL_TWT_ID_BCAST_AUTO) {
		if ((err = wlc_twt_bid_first_free(bss_twt, &desc->id)) != BCME_OK) {
			goto fail;
		}
		bid = desc->id;
	}

	bcast = &bss_twt->bcast[bid];
	/* Two possibilities to get here, either it is an update of existing bid or a new bid
	 * updating a bid is complex and will be built later. For now return unsupported
	 */
	if (bcast->state != WLC_TWT_STATE_INACTIVE) {
		err = BCME_UNSUPPORTED;
		goto fail;
	}

	/* This is the new bcast info schedule situation. Lets store the necessary information
	 * and make sure the beacon generation gets triggered.
	 */
	memcpy(&bcast->desc, desc, sizeof(bcast->desc));
	bcast->state |= WLC_TWT_STATE_RESERVED;
	bss_twt->bid_count++;

	wlc_twt_bcast_adjust_twt_params(twti, bcast);

	/* Determine the first SP. Once this is calculated it will be updated regularly on TBTT */
	wlc_twt_get_tsf(wlc, cfg, &tsf_l, &tsf_h);
	/* Use TSF to find the next beacon moment */
	bcnint_us = cfg->current_bss->beacon_period * DOT11_TU_TO_US;
	next_beacon = math_div_64(tsf_h, tsf_l, bcnint_us);
	next_beacon++;
	next_beacon *= bcnint_us;
	/* Add some time after beacon */
	next_beacon += WLC_SCHEDID_TO_TSFL(bid + 1);

	bcast->twt_l = next_beacon & WLC_TWT_TSF_MAX;
	bcast->twt_h = next_beacon >> 32;

	/* regenerate beacon */
	if (cfg->up) {
		wlc_bss_update_beacon(wlc, cfg);
	}
fail:
	if (err != BCME_OK) {
		WL_ERROR(("wl%d: %s: err=%d\n", wlc->pub->unit, __FUNCTION__, err));
	}
	return err;
}

/**
 * Teardown a broadcast TWT schedule in AP (Beacons).
 */
static int
wlc_twt_ap_btwt_teardown(wlc_twt_info_t *twti, wlc_bsscfg_t *cfg, uint8 bid)
{
	/* Initial version is only to support BCAST TWT schedule removal and in not so
	 * nice way: Just set state to inactive. Fix in future to handle nicely.
	 */
	int err = BCME_OK;
	twt_bss_t *bss_twt;
	twt_bcast_desc_t *bcast;

	bss_twt = TWT_BSS(twti, cfg);
	if (bss_twt == NULL) {
		err = BCME_UNSUPPORTED;
		goto fail;
	}

	if (bid > WL_TWT_ID_BCAST_MAX) {
		err = BCME_BADARG;
		goto fail;
	}
	bcast = &bss_twt->bcast[bid];

	/* Initial approach, just clear the state and lower the count */
	if (bcast->state != WLC_TWT_STATE_INACTIVE) {
		bcast->teardown_count = WLC_TWT_TEARDOWN_COUNT;
	} else {
		err = BCME_BADARG;
	}

fail:
	if (err != BCME_OK) {
		WL_ERROR(("wl%d: %s: err=%d\n", twti->wlc->pub->unit, __FUNCTION__, err));
	}
	return err;
}

static int
wlc_twt_bid_first_free(twt_bss_t *bss_twt, uint8 *bid)
{
	uint i;

	for (i = WL_TWT_ID_BCAST_MIN; i < ARRAYSIZE(bss_twt->bcast); i++) {
		if (bss_twt->bcast[i].state == WLC_TWT_STATE_INACTIVE) {
			*bid = i;
			return BCME_OK;
		}
	}

	return BCME_NORESOURCE;
}

/* ======== IE mgmt hooks ======== */

#ifdef AP
/**
 * Broadcast TWT IE is the IE which gets added to the beacon. It is the only IE which gets
 * addded and it can get dynamically updated. bcast configurations can be added and removed in
 * a "running" system.
 */

/**
 * return the length of the broadcast TWT IE, 0 if no bcast TWT configurations are active
 */
static uint
wlc_twt_calc_bcast_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	wlc_twt_info_t *twti = ctx;
	wlc_bsscfg_t *cfg = data->cfg;
	twt_bss_t *bss_twt = TWT_BSS(twti, cfg);
	uint len;

	if (bss_twt == NULL) {
		return 0;
	}

	if (!BSSCFG_AP(cfg)) {
		return 0;
	}

	/* bid_count identifies how many descriptors are taken. */
	if (bss_twt->bid_count == 0) {
		return 0;
	}

	/* See 802.11ax for definition 9.4.2.200, Figure Figure 9-589av and Figure 9-589av2 */
	len = sizeof(twt_ie_top_t);
	len += (bss_twt->bid_count * WLC_TWT_BCAST_IE_SZ);

	return len;
}

/**
 * write the broadcast TWT IE into the frame
 */
static int
wlc_twt_write_bcast_ie(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_twt_info_t *twti = ctx;
	wlc_bsscfg_t *cfg = data->cfg;
	twt_bss_t *bss_twt = TWT_BSS(twti, cfg);
	twt_bcast_desc_t *bcast;
	twt_ie_top_t *twt_top;
	twt_ie_bcast_t *last_bcast_ie = NULL;
	uint8 *body;
	uint i;
	uint16 twt;

	if (bss_twt == NULL) {
		return BCME_OK;
	}

	if (bss_twt->bid_count == 0) {
		return BCME_OK;
	}

	/* First build the body of the IE and determine total length along the way. After that
	 * create the header and with the len field
	 */
	body = data->buf + sizeof(twt_ie_top_t);
	for (i = WL_TWT_ID_BCAST_MIN; i < ARRAYSIZE(bss_twt->bcast); i++) {
		bcast = &bss_twt->bcast[i];
		if (bcast->state != WLC_TWT_STATE_INACTIVE) {
			last_bcast_ie = (twt_ie_bcast_t *)body;
			twt = (bcast->twt_l >> WLC_TWT_BTWT_SHIFT) & WLC_TWT_BTWT_MASK;
			body += wlc_twt_ie_add_single_bcast_ie(body, bcast,
				TWT_SETUP_CMD_ACCEPT_TWT, twt);
			if (bcast->teardown_count) {
				/* TWT calculation: beacon count determines how many beacons
				 * this is to be scheduled yet. Once count reaches 0 and TBTT
				 * handler called then schedule will be removed. Use that to
				 * get correct TWT here !!
				 */
				last_bcast_ie = (twt_ie_bcast_t *)body;
				body += wlc_twt_ie_add_single_bcast_ie(body, bcast,
					TWT_SETUP_CMD_REJECT_TWT, twt);
			}
		}
	}
	/* Set Last Broadcast Parameter Set in the last bcast IE */
	if (last_bcast_ie) {
		setbit(&last_bcast_ie->request_type, TWT_REQ_TYPE_IMPL_LAST_IDX);
	}

	twt_top = (twt_ie_top_t *)data->buf;
	twt_top->id = DOT11_MNG_TWT_ID;
	twt_top->len = (uint8)(body - data->buf) - TLV_HDR_LEN;
	twt_top->ctrl = TWT_CTRL_NEGO_BCAST_BEACON << TWT_CTRL_NEGOTIATION_SHIFT;

	return BCME_OK;
}
#endif /* AP */

#ifdef STA
/**
 * parse the broadcast TWT IE in the received beacon
 */
static int
wlc_twt_parse_btwt_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_twt_info_t *twti = ctx;
	wlc_info_t *wlc = twti->wlc;
	wlc_bsscfg_t *cfg = data->cfg;
	twt_bss_t *bss_twt = TWT_BSS(twti, cfg);
	twt_ie_top_t *twt_top;
	int err = BCME_OK;
	uint8 to_remove[WL_TWT_ID_BCAST_MAX + 1];
	twt_ie_bcast_t *bcast_ie;
	uint8 *body;
	int16 body_len;
	uint8 *ie_param;
	uint8 setup_command;
	uint8 bid;
	twt_bcast_desc_t *bcast;
	int i;
	bool done;

	if (bss_twt == NULL) {
		return BCME_OK;
	}

	if (data->ie == NULL) {
		/* Check if we have any bcast TWT from this AP stored, if so possible teardown
		 * any related links and cleanup the stored IE
		 */

		if (bss_twt->btwt_ie) {
			/* TODO, cleanup any TWT links, brute force method as cleaning up
			 * this way by AP is not really the way to do it, but deal with it
			 */

			MFREE(wlc->osh, bss_twt->btwt_ie, bss_twt->btwt_ie_len);
			bss_twt->btwt_ie =  NULL;
			bss_twt->btwt_ie_len = 0;
		}
		return BCME_OK;
	}

	body = (uint8 *)data->ie;
	twt_top = (twt_ie_top_t *)body;
	body_len = TLV_HDR_LEN + twt_top->len;

	/* Do we already have info on the bcast TWT IEs stored, if so did they change? */
	if (bss_twt->btwt_ie) {
		if ((bss_twt->btwt_ie_len != body_len) ||
			memcmp(body, bss_twt->btwt_ie, body_len)) {
			/* The IE has been updated. Remove the old one. */
			MFREE(wlc->osh, bss_twt->btwt_ie, bss_twt->btwt_ie_len);
			bss_twt->btwt_ie =  NULL;
			bss_twt->btwt_ie_len = 0;
		} else {
			return BCME_OK;
		}
	}

	/* Store the IE now, even if we find an error then we want to store it now. If error
	 * then we wont have to parse it again. It will result in same error
	 */
	if ((bss_twt->btwt_ie = MALLOCZ(wlc->osh, body_len)) == NULL) {
		err = BCME_NOMEM;
		goto fail;
	}
	memcpy(bss_twt->btwt_ie, data->ie, body_len);
	bss_twt->btwt_ie_len = body_len;

	/* parse control field and validate negotiation field for BROADCAST bit */
	if (((twt_top->ctrl & TWT_CTRL_NEGOTIATION_MASK) >> TWT_CTRL_NEGOTIATION_SHIFT) !=
			TWT_CTRL_NEGO_BCAST_BEACON) {
		err = BCME_BADARG;
		goto fail;
	}

	/* Start parsing all the Broadcast TWT announcements, see Table 27-6. There is
	 * however one "incorrect" form of removal of TWT we want to handle as well. An AP is
	 * supposed to clearly advertise the removal of the TWT. This can be done by using
	 * persistence or by using reject but it can also just remove the twt schedule, The
	 * latter is not allowed, but if handled then persistence handling is not explicitly
	 * needed, as it will result in the same, except that we as client might be a bit
	 * slow in detecting the removal. But it isnt our choice anyway, so it doesnt matter.
	 */

	/* Start with marking all known schedules as to be removed. Clear the removal once info
	 * has been found, (or explicitly set it if reject setup command is found)
	 */
	memset(&to_remove[0], 0, sizeof(to_remove));
	for (i = WL_TWT_ID_BCAST_MIN; i < ARRAYSIZE(bss_twt->bcast); i++) {
		if (bss_twt->bcast[i].state != WLC_TWT_STATE_INACTIVE) {
			to_remove[i] = 1;
		}
	}

	body += sizeof(twt_ie_top_t);
	body_len -= sizeof(twt_ie_top_t);

	done = FALSE;
	while ((body_len >= WLC_TWT_BCAST_IE_SZ) && (done == FALSE)) {
		bcast_ie = (twt_ie_bcast_t *)body;

		/* Get setup command from IE. If accept, see if we have it locally stored, if
		 * not then store it, otherwise ignore. Changes will go through Alternate command.
		 * If Alternate check if update is imminent, if so then update. If Reject TWT is
		 * configured then just ignore it for now. Could uptimize for that, but we just
		 * detect removal of. No explicit handling of that here.
		 */
		ie_param = (uint8 *)&bcast_ie->request_type;
		setup_command = getbits(ie_param, sizeof(bcast_ie->request_type),
			TWT_REQ_TYPE_SETUP_CMD_IDX, TWT_REQ_TYPE_SETUP_CMD_FSZ);

		ie_param = (uint8 *)&bcast_ie->bcast_info;
		bid = getbits(ie_param, sizeof(bcast_ie->bcast_info),
			TWT_BCAST_INFO_ID_IDX, TWT_BCAST_INFO_ID_FSZ);
		bcast = &bss_twt->bcast[bid];
		if (setup_command == TWT_SETUP_CMD_ACCEPT_TWT) {
			if (bcast->state == WLC_TWT_STATE_INACTIVE) {
				wlc_twt_ie_parse_single_bcast_ie(bcast_ie, &bcast->desc);
				bcast->state |= WLC_TWT_STATE_RESERVED;
			} else {
				to_remove[bid] = 0;
			}
		}
		/* TODO: add handling of changed TWT schedule and to be deleted schedule.
		 * Implement upon adding this to AP!
		 */
		done = isset(&bcast_ie->request_type, TWT_REQ_TYPE_IMPL_LAST_IDX);
		body += WLC_TWT_BCAST_IE_SZ;
		body_len -= WLC_TWT_BCAST_IE_SZ;
	}
	/* Verify body_len versus done bit. body_len should be 0 and done TRUE. Ignore error
	 * but do report it.
	 */
	if (!done || body_len) {
		WL_ERROR(("wl%d: %s: Beacon parsing error: body_len left=%d, implicit last=%d\n",
			wlc->pub->unit, __FUNCTION__, body_len, done));
	}

	/* Remove all schedules which have to_remove set */
	for (i = WL_TWT_ID_BCAST_MIN; i < ARRAYSIZE(bss_twt->bcast); i++) {
		if (to_remove[i]) {
			/* Send teardown request? */
			bss_twt->bcast[i].state = WLC_TWT_STATE_INACTIVE;
		}
	}

fail:
	if (err != BCME_OK) {
		WL_ERROR(("wl%d: %s: err=%d\n", wlc->pub->unit, __FUNCTION__, err));
	}
	return err;
}
#endif /* STA */

/**
 * Parse single Broadcast TWT IE based on info in bcast_ie and store the parsed information in
 * bcast.
 */
static void
wlc_twt_ie_parse_single_bcast_ie(twt_ie_bcast_t *bcast_ie, wl_twt_sdesc_t *desc)
{
	uint8 *ie_param;

	memset(desc, 0, sizeof(*desc));

	/* Request type */
	ie_param = (uint8 *)&bcast_ie->request_type;
	desc->flow_flags = WL_TWT_FLOW_FLAG_BROADCAST;
	if (isset(ie_param, TWT_REQ_TYPE_TRIGGER_IDX)) {
		desc->flow_flags |= WL_TWT_FLOW_FLAG_TRIGGER;
	}
	if (isset(ie_param, TWT_REQ_TYPE_FLOW_TYPE_IDX)) {
		desc->flow_flags |= WL_TWT_FLOW_FLAG_UNANNOUNCED;
	}

	/* Target Waket Time is not parsed here */

	/* Nominal Minimum TWT Wake Druation */
	desc->wake_duration = bcast_ie->wake_duration;

	/* TWT Wake Interval Mantissa */
	desc->wake_interval_mantissa = ltoh16(bcast_ie->wake_interval_mantissa);

	/* Broadcast TWT Info */
	ie_param = (uint8 *)&bcast_ie->bcast_info;
	desc->persistence_exponent = getbits(ie_param, sizeof(bcast_ie->bcast_info),
		TWT_BCAST_INFO_PERS_EXP_IDX, TWT_BCAST_INFO_PERS_EXP_FSZ);
	desc->persistence_mantissa = getbits(ie_param, sizeof(bcast_ie->bcast_info),
		TWT_BCAST_INFO_PERS_MAN_IDX, TWT_BCAST_INFO_PERS_MAN_FSZ);
	/* Also store bid, even though we likely already know it */
	desc->id = getbits(ie_param, sizeof(bcast_ie->bcast_info),
		TWT_BCAST_INFO_ID_IDX, TWT_BCAST_INFO_ID_FSZ);
}

/**
 * Add single Broadcast TWT IE based on info in 'bcast'. assumption is that the IE is
 * cleared/zero-ed before being called. twt is the Target Wake Time to be filled into the struct.
 */
static uint
wlc_twt_ie_add_single_bcast_ie(uint8 *body, twt_bcast_desc_t *bcast,
	uint8 setup_command, uint16 twt)
{
	twt_ie_bcast_t *bcast_ie;
	uint8 *ie_param;

	/* Rquest Type */
	bcast_ie = (twt_ie_bcast_t *)body;
	ie_param = (uint8 *)&bcast_ie->request_type;

	setbits(ie_param, sizeof(bcast_ie->request_type), TWT_REQ_TYPE_SETUP_CMD_IDX,
		TWT_REQ_TYPE_SETUP_CMD_FSZ, setup_command);
	if (bcast->desc.flow_flags & WL_TWT_FLOW_FLAG_TRIGGER) {
		setbit(ie_param, TWT_REQ_TYPE_TRIGGER_IDX);
	}
	if (bcast->desc.flow_flags & WL_TWT_FLOW_FLAG_UNANNOUNCED) {
		setbit(ie_param, TWT_REQ_TYPE_FLOW_TYPE_IDX);
	}

	/* Keep Broadcast TWT Recommendation field to 0, see 9-262k1 table */

	setbits(ie_param, sizeof(bcast_ie->request_type), TWT_REQ_TYPE_WAKE_EXP_IDX,
		TWT_REQ_TYPE_WAKE_EXP_FSZ, bcast->desc.wake_interval_exponent);

	if (bcast->desc.flow_flags & WL_TWT_FLOW_FLAG_PROTECTION) {
		setbit(ie_param, TWT_REQ_TYPE_PROTECTION_IDX);
	}

	/* Target Waket Time */
	bcast_ie->twt = htol16(twt);

	/* Nominal Minimum TWT Wake Druation */
	bcast_ie->wake_duration = bcast->desc.wake_duration;

	/* TWT Wake Interval Mantissa */
	bcast_ie->wake_interval_mantissa = htol16(bcast->desc.wake_interval_mantissa);

	/* Broadcast TWT Info */
	ie_param = (uint8 *)&bcast_ie->bcast_info;
	setbits(ie_param, sizeof(bcast_ie->bcast_info), TWT_BCAST_INFO_PERS_EXP_IDX,
		TWT_BCAST_INFO_PERS_EXP_FSZ, bcast->desc.persistence_exponent);
	setbits(ie_param, sizeof(bcast_ie->bcast_info), TWT_BCAST_INFO_ID_IDX,
		TWT_BCAST_INFO_ID_FSZ, bcast->desc.id);
	setbits(ie_param, sizeof(bcast_ie->bcast_info), TWT_BCAST_INFO_PERS_MAN_IDX,
		TWT_BCAST_INFO_PERS_MAN_FSZ, bcast->desc.persistence_mantissa);

	return WLC_TWT_BCAST_IE_SZ;
}

/**
 * TBTT update handler. Called for every bsscfg, To be called from wlc_bss_tbtt (wlc.c). Called
 * for both STA and AP. AP is used to handle teardown requests to update beacons if necessary. For
 * AP and STA there is a per SCB check to see if timeouts happened on AF handling.
 */

void
wlc_twt_tbtt(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	twt_bss_t *bss_twt;
	uint i;
	bool beacon_update;
	scb_t *scb;
	scb_iter_t scbiter;
	twt_bcast_desc_t *bcast;
	uint32 wake_interval;
	uint32 tsf_h, tsf_l;

	bss_twt = TWT_BSS(wlc->twti, cfg);
	if (bss_twt == NULL) {
		goto check_scb;
	}

	if (bss_twt->bid_count == 0) {
		goto check_scb;
	}

	beacon_update = FALSE;
	for (i = WL_TWT_ID_BCAST_MIN; i < ARRAYSIZE(bss_twt->bcast); i++) {
		bcast = &bss_twt->bcast[i];
		if (bcast->teardown_count) {
			bcast->teardown_count--;
			if (bcast->teardown_count == 0) {
				bcast->state = WLC_TWT_STATE_INACTIVE;
				bss_twt->bid_count--;
			}
			beacon_update = TRUE;
		}
		if (bcast->state != WLC_TWT_STATE_INACTIVE) {
			wlc_twt_get_tsf(wlc, cfg, &tsf_l, &tsf_h);
			wlc_uint64_add(&tsf_h, &tsf_l, 0, WLC_SCHEDID_TO_TSFL(1));
			wake_interval = bcast->desc.wake_interval_mantissa *
				(1 << bcast->desc.wake_interval_exponent);
			while (((tsf_h - bcast->twt_h) == 1) ||
			       ((tsf_h == bcast->twt_h) && (tsf_l > bcast->twt_l))) {
				wlc_uint64_add(&bcast->twt_h, &bcast->twt_l, 0, wake_interval);
				beacon_update = TRUE;
			}

		}
	}
	if (beacon_update) {
		wlc_bss_update_beacon(wlc, cfg);
	}

check_scb:
	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {
		wlc_twt_scb_timeout_cnt(wlc, scb);
	}

}

/**
 * The individual TWT setup is complete. All parameters are stored in indv, and AF sequence has
 * completed. This function can be called in AP and in non-AP mode. The counterpart is the function
 * wlc_twt_teardown_itwt
 */
static int
wlc_twt_setup_itwt_complete(wlc_twt_info_t *twti, scb_t *scb, twt_indv_desc_t *indv)
{
	twt_scb_t *scb_twt = TWT_SCB(twti, scb);
	twt_scb_t *last;

	indv->state = WLC_TWT_STATE_ACTIVE;
	scb_twt->indv_active_cnt++;
	twti->itwt_count++;

	/* Is this the first iTWT for this scb then add it to the active list */
	if (scb_twt->indv_active_cnt == 1) {
		scb_twt->next = NULL;
		if (twti->scb_twt_list) {
			last = twti->scb_twt_list;
			while (last->next) {
				last = last->next;
			}
			last->next = scb_twt;
		} else {
			twti->scb_twt_list = scb_twt;
		}
		WL_TWT(("%s added scb_twt %p to list, twti->scb_twt_list = %p\n",
			__FUNCTION__, scb_twt, twti->scb_twt_list));
		scb_twt->cur_sp_start = WLC_TSFL_TO_SCHEDID(indv->twt_l);
	}

	WL_TWT(("%s id=%d iTWT count = %d\n", __FUNCTION__, indv->desc.id, twti->itwt_count));

	return wlc_twt_itwt_start(twti, scb, indv);
}

/**
 * Individual twt teardown. This function can be called when an AF is received with teardown
 * command or when the sending of the AF twt teardsown is complete. Remove the itwt schedule
 */
static int
wlc_twt_teardown_itwt(wlc_twt_info_t *twti, scb_t *scb, twt_indv_desc_t *indv, bool enter_pm)
{
	twt_scb_t *scb_twt = TWT_SCB(twti, scb);
	twt_scb_t *prev;

	if (indv->state == WLC_TWT_STATE_ACTIVE) {
		scb_twt->indv_active_cnt--;
		/* If this is the last iTWT for this SCB then remove it from the list */
		if (scb_twt->indv_active_cnt == 0) {
			ASSERT(twti->scb_twt_list);
			if (twti->scb_twt_list == scb_twt) {
				twti->scb_twt_list = scb_twt->next;
			} else {
				prev = twti->scb_twt_list;
				while (prev->next != scb_twt) {
					ASSERT(prev->next);
					prev = prev->next;
				}
				prev->next = scb_twt->next;
			}
			WL_TWT(("%s removed scb_twt %p (idx %d) from list,"
				" twti->scb_twt_list = %p\n",
				__FUNCTION__, scb_twt, scb_twt->rlm_id, twti->scb_twt_list));
		}
		ASSERT(twti->itwt_count);
		twti->itwt_count--;
		if (indv->paused) {
			indv->paused = FALSE;
			scb_twt->indv_paused_cnt--;
		}
		twti->itwt_trig_count -= indv->trigger;

		if (indv->trigger && twti->itwt_trig_count == 0) {
			/* last trigger-enabled itwt should set original ul params */
			wlc_ulmu_twt_params(twti->wlc->ulmu, FALSE);
		}
	}
	indv->state = WLC_TWT_STATE_INACTIVE;

	WL_TWT(("%s id=%d, iTWT count = %d trig count = %d\n", __FUNCTION__,
		indv->desc.id, twti->itwt_count, twti->itwt_trig_count));

	return wlc_twt_itwt_stop(twti, scb, indv, enter_pm);
}

/**
 * Is there an individual TWT active on this SCB. This is used by external modules. TWT
 * impacts PS handling and with that suppression handling. Currently we report true when
 * active_cnt is not 0. However active gets set once the setup is completed, but that is not
 * really correct moment. It needs to be done when the first SP is about to enter? Need carefull
 * "change" handling.
 */
bool
wlc_twt_scb_active(wlc_twt_info_t *twti, scb_t *scb)
{
	twt_scb_t *scb_twt;

	if (twti == NULL) {
		return FALSE;
	}
	scb_twt = TWT_SCB(twti, scb);
	if (scb_twt == NULL) {
		return FALSE;
	}
	if (scb_twt->indv_active_cnt == scb_twt->indv_paused_cnt) {
		return FALSE;
	}

	return scb_twt->sp_started;
}

/**
 * Return SCB-specific TWT info for sta_info reporting using WL_STA_TWT_ flags from wlioctl_defs.h.
 */
uint8
wlc_twt_scb_info(wlc_twt_info_t *twti, scb_t *scb)
{
	twt_scb_t *scb_twt;
	uint8 twt_info;

	if (twti == NULL) {
		return 0;
	}
	scb_twt = TWT_SCB(twti, scb);
	if (scb_twt == NULL) {
		return 0;
	}
	twt_info = isset((uint8 *)&scb_twt->cap, WLC_TWT_CAP_TWT_REQ_SUPPORT) ? WL_STA_TWT_CAP : 0;
	if (scb_twt->indv_active_cnt > 0) {
		twt_info |= WL_STA_TWT_SCHEDULED;
	}
	if (scb_twt->indv_paused_cnt < scb_twt->indv_active_cnt) {
		twt_info |= WL_STA_TWT_ACTIVE;
	}
	return twt_info;
}

/**
 * find the fid in the individual TWT configurations. Return NULL if not found
 */
static twt_indv_desc_t *
wlc_twt_find_fid(twt_scb_t *scb_twt, uint8 fid)
{
	uint8 i;

	for (i = 0; i < ARRAYSIZE(scb_twt->indv); i++) {
		if ((scb_twt->indv[i].state != WLC_TWT_STATE_INACTIVE) &&
		    (scb_twt->indv[i].desc.id == fid)) {
			return &scb_twt->indv[i];
		}
	}

	return NULL;
}

/**
 * Parse a twt setup IE and 'translate' it into local settings.
 */
static void
wlc_twt_parse_twt_setup_ie(twt_ie_indv_t *twt_ie, twt_indv_desc_t *indv)
{
	uint8 *ie_param;

	/* Request type */
	ie_param = (uint8 *)&twt_ie->request_type;

	indv->desc.flow_flags = 0;
	if (isclr(ie_param, TWT_REQ_TYPE_REQUEST_IDX)) {
		WL_TWT(("%s invalid IE, TWT request bit not set!\n", __FUNCTION__));
	}
	if (isset(ie_param,  TWT_REQ_TYPE_TRIGGER_IDX)) {
		indv->desc.flow_flags |= WL_TWT_FLOW_FLAG_TRIGGER;
	}
	if (isclr(ie_param, TWT_REQ_TYPE_IMPL_LAST_IDX)) {
		WL_TWT(("%s invalid IE, Implicit bit not set!\n", __FUNCTION__));
	}
	if (isset(ie_param,  TWT_REQ_TYPE_FLOW_TYPE_IDX)) {
		indv->desc.flow_flags |= WL_TWT_FLOW_FLAG_UNANNOUNCED;
	}
	indv->desc.id = getbits(ie_param, sizeof(twt_ie->request_type),
		TWT_REQ_TYPE_FLOW_ID_IDX, TWT_REQ_TYPE_FLOW_ID_FSZ);
	indv->desc.wake_interval_exponent = getbits(ie_param, sizeof(twt_ie->request_type),
		TWT_REQ_TYPE_WAKE_EXP_IDX, TWT_REQ_TYPE_WAKE_EXP_FSZ);
	if (isset(ie_param,  TWT_REQ_TYPE_PROTECTION_IDX)) {
		indv->desc.flow_flags |= WL_TWT_FLOW_FLAG_PROTECTION;
	}

	/* Target Wake Time */
	indv->twt_l = ltoh32_ua(&twt_ie->twt[0]);
	indv->twt_h = ltoh32_ua(&twt_ie->twt[4]);

	/* Nominal Minimum TWT Wake Druation */
	indv->desc.wake_duration = twt_ie->wake_duration;

	/* TWT Wake Interval Mantissa */
	indv->desc.wake_interval_mantissa = ltoh16(twt_ie->wake_interval_mantissa);

	indv->desc.channel = twt_ie->channel;
	if (indv->desc.channel) {
		WL_TWT(("%s invalid IE, TWT Channel should be 0!\n", __FUNCTION__));
	}
}

/**
 * Build the basic AF for sending an Individual or Broadcast TWT setup request or response
 */
static void*
wlc_twt_build_af(scb_t *scb, uint8 dialog_token, uint8 **body, bool individual)
{
	wlc_bsscfg_t *cfg;
	void *pkt;
	uint body_len;
	twt_ie_top_t *twt_top;

	cfg = SCB_BSSCFG(scb);

	/* Allocate the packet */
	body_len = S1G_AF_TWT_SETUP_TWT_IE_OFF + WLC_TWT_TOP_IE_SZ;
	body_len += individual ? WLC_TWT_INDV_IE_SZ : WLC_TWT_BCAST_IE_SZ;
	if ((pkt = wlc_frame_get_action(cfg->wlc, &scb->ea, &cfg->cur_etheraddr,
		&cfg->BSSID, body_len, body, DOT11_ACTION_CAT_S1G)) == NULL) {
		return NULL;
	}
	/* AF header */
	(*body)[S1G_AF_CAT_OFF] = DOT11_ACTION_CAT_S1G;
	(*body)[S1G_AF_ACT_OFF] = S1G_ACTION_TWT_SETUP;
	(*body)[S1G_AF_TWT_SETUP_TOKEN_OFF] = dialog_token;

	*body += S1G_AF_TWT_SETUP_TWT_IE_OFF;
	/* Fill up AF top part of IE */
	twt_top = (twt_ie_top_t *)*body;
	twt_top->id = DOT11_MNG_TWT_ID;
	twt_top->len = sizeof(twt_top->ctrl);
	twt_top->len += individual ? WLC_TWT_INDV_IE_SZ : WLC_TWT_BCAST_IE_SZ;
	twt_top->ctrl = (individual ? TWT_CTRL_NEGO_INDV_REQ_RSP : TWT_CTRL_NEGO_BCAST_MGMT) <<
		TWT_CTRL_NEGOTIATION_SHIFT;
	*body += WLC_TWT_TOP_IE_SZ;

	return pkt;
}

/**
 * Create an individual twt setup IE and 'translate' local settings into IE.
 */
static void
wlc_twt_itwt_build_setup_ie(twt_ie_indv_t *twt_ie, twt_indv_desc_t *indv, uint8 setup_command)
{
	uint8 *ie_param;

	/* Request type */
	ie_param = (uint8 *)&twt_ie->request_type;

	/* The following requires an update if we ever want to support requester from AP or
	 * responder from STA, for now, features dont allow that.
	 */
	if ((setup_command == TWT_SETUP_CMD_REQUEST_TWT) ||
	    (setup_command == TWT_SETUP_CMD_SUGGEST_TWT) ||
	    (setup_command == TWT_SETUP_CMD_DEMAND_TWT)) {
		setbit(ie_param, TWT_REQ_TYPE_REQUEST_IDX);
	}
	setbits(ie_param, sizeof(twt_ie->request_type), TWT_REQ_TYPE_SETUP_CMD_IDX,
		TWT_REQ_TYPE_SETUP_CMD_FSZ, setup_command);
	if (indv->desc.flow_flags & WL_TWT_FLOW_FLAG_TRIGGER) {
		setbit(ie_param, TWT_REQ_TYPE_TRIGGER_IDX);
	}
	setbit(ie_param, TWT_REQ_TYPE_IMPL_LAST_IDX);
	if (indv->desc.flow_flags & WL_TWT_FLOW_FLAG_UNANNOUNCED) {
		setbit(ie_param, TWT_REQ_TYPE_FLOW_TYPE_IDX);
	}
	setbits(ie_param, sizeof(twt_ie->request_type), TWT_REQ_TYPE_FLOW_ID_IDX,
		TWT_REQ_TYPE_FLOW_ID_FSZ, indv->desc.id);
	setbits(ie_param, sizeof(twt_ie->request_type), TWT_REQ_TYPE_WAKE_EXP_IDX,
		TWT_REQ_TYPE_WAKE_EXP_FSZ, indv->desc.wake_interval_exponent);
	if (indv->desc.flow_flags & WL_TWT_FLOW_FLAG_PROTECTION) {
		setbit(ie_param, TWT_REQ_TYPE_PROTECTION_IDX);
	}

	/* Store TWT */
	store32_ua(&twt_ie->twt[0], htol32(indv->twt_l));
	store32_ua(&twt_ie->twt[4], htol32(indv->twt_h));

	/* Nominal Minimum TWT Wake Druation */
	twt_ie->wake_duration = indv->desc.wake_duration;

	/* TWT Wake Interval Mantissa */
	twt_ie->wake_interval_mantissa = htol16(indv->desc.wake_interval_mantissa);
}

/**
 * Send Reject Action Frame. The reject frame is made of the TWT IE which caused this reject.
 */
static int
wlc_twt_sta_itwt_reject_send(wlc_twt_info_t *twti, scb_t *scb, twt_ie_indv_t *twt_ie,
	uint8 dialog_token)
{
	int err = BCME_OK;
	void *pkt = NULL;
	uint8 *body;

	pkt = wlc_twt_build_af(scb, dialog_token, &body, TRUE);
	if (!pkt) {
		err = BCME_NOMEM;
		goto fail;
	}

	memcpy(body, twt_ie, WLC_TWT_INDV_IE_SZ);
	twt_ie = (twt_ie_indv_t *)body;

	setbits((uint8 *)&twt_ie->request_type, sizeof(twt_ie->request_type),
		TWT_REQ_TYPE_SETUP_CMD_IDX, TWT_REQ_TYPE_SETUP_CMD_FSZ, TWT_SETUP_CMD_REJECT_TWT);

	if (WL_TWT_ON()) {
		prhex("iTWT Reject", body - S1G_AF_TWT_SETUP_TWT_IE_OFF - WLC_TWT_TOP_IE_SZ,
			S1G_AF_TWT_SETUP_TWT_IE_OFF + WLC_TWT_TOP_IE_SZ + WLC_TWT_INDV_IE_SZ);
	}
	if (!wlc_sendmgmt(twti->wlc, pkt, twti->wlc->active_queue, scb)) {
		err = BCME_BUSY;
	}

fail:
	return err;
}

/**
 * Adjust TWT parameters to be used for TWT configuration. This function is to be called when
 * request has been accepeted, but the parameters need adjustment (or not) to work with our
 * TWT sheduler. Also dealing with WFA specific setups is handled here. WFA used commands:
 * SetupCommand	Trigger	Unannounced	Interval (m/e/us)	Duration (256us/us)
 * 0(request)	1	0		512/10/524288		255/65280
 * 1(suggest)	1	1		32768/0/32768		64/16384
 * 0(request)	1	0		512/10/524288		255/65280
 * 1(suggest)	1	1		32768/0/32768		64/16384
 * 1(suggest)	1	0		512/10/524288		255/65280
 * 0 or 1	1	1		32768/0/32768		64/16384
 * 0 or 1	1	1		32768/1/65536		40/10240
 * ----------------------- WFA 6E: ----------------------------------------
 * 0 or 1	1	1		51200/1/102400		40/10240
 */
static void
wlc_twt_indv_adjust_twt_params(wlc_twt_info_t *twti, scb_t *scb, twt_indv_desc_t *indv)
{
	uint32 wake_interval;
	uint32 wake_duration;

	/* schedid is a time unit, see definition of WLC_SCHEDID_TSF_SHIFT, all timings are to
	 * be remapped to this timing, except for WFA links. Those will once accepted and when
	 * not meeting the requirement perfectly be change to usable values, which will still be
	 * suitable to meet the WFA requirement.
	 */

	/* Wake duration is in unit of 256 usec (shift over 8) */
	wake_duration = ((uint32)indv->desc.wake_duration << 8);

	/* Duration: Increase to next schedid, unless value 255 (max) or 40 */
	if ((indv->desc.wake_duration != 255) && ((indv->desc.wake_duration != 40) ||
#ifdef WLC_TWT_USE_4MS_SCHEDID
		((indv->desc.wake_interval_mantissa != 32768) &&
		(indv->desc.wake_interval_mantissa != 51200)) ||
#else
		(indv->desc.wake_interval_mantissa != 32768) ||
#endif /* WLC_TWT_USE_4MS_SCHEDID */
		(indv->desc.wake_interval_exponent != 1))) {
		if (wake_duration & WLC_TSFL_SCHEDID_MASK_INV) {
			wake_duration &= WLC_TSFL_SCHEDID_MASK;
			wake_duration += (1 << WLC_SCHEDID_TSF_SHIFT);
			/* Check for exceeding max wake_duration of 255 */
			if (wake_duration > (255 << 8)) {
				wake_duration -= (1 << WLC_SCHEDID_TSF_SHIFT);
			}
		}
		indv->desc.wake_duration = wake_duration >> 8;
	}
	/* All WFA wake intervals are fine. So only the non WFA requests need updating to
	 * acceptable values. However that is slightly complicated. The request comes in
	 * mantissa exponent. Translating that to usec is easy, adjusting is also easy,
	 * translating back to mantissa exponent is not.
	 */
	wake_interval = indv->desc.wake_interval_mantissa *
		(1 << indv->desc.wake_interval_exponent);

	if (wake_interval & WLC_TSFL_SCHEDID_MASK_INV) {
		wake_interval &= WLC_TSFL_SCHEDID_MASK;
		if (wake_interval == 0) {
			wake_interval = (1 << WLC_SCHEDID_TSF_SHIFT);
		}
		wake_interval += (1 << WLC_SCHEDID_TSF_SHIFT);
	}

	/* It is possible that wake_duration has become wake_interval. Increase wake_interval
	 * in that case
	 */
	if (wake_duration == wake_interval) {
		wake_interval += (1 << WLC_SCHEDID_TSF_SHIFT);
	}
	/* Avoid overwriting WFA values, as it is not accepted to use different mantissa
	 * exponent, even thought the interval is the same
	 */
	if (!(((indv->desc.wake_interval_mantissa == 32768) &&
	      ((indv->desc.wake_interval_exponent == 0) ||
	       (indv->desc.wake_interval_exponent == 1))) ||
	     ((indv->desc.wake_interval_mantissa == 512) &&
#ifdef WLC_TWT_USE_4MS_SCHEDID
	      (indv->desc.wake_interval_exponent == 10)) ||
	     ((indv->desc.wake_interval_mantissa == 51200) &&
	      (indv->desc.wake_interval_exponent == 1)))) {
#else
	      (indv->desc.wake_interval_exponent == 10)))) {
#endif /* WLC_TWT_USE_4MS_SCHEDID */
		indv->desc.wake_interval_exponent = WLC_SCHEDID_TSF_SHIFT;
		indv->desc.wake_interval_mantissa = wake_interval >> WLC_SCHEDID_TSF_SHIFT;
	}
}

/**
 * Action Frame handler for the action_id TWT SETUP where setup command is reqeust, suggest or
 * demand. The negotiation type was determined to be individual.
 */
static int
wlc_twt_indv_af_setup_req_recv(wlc_twt_info_t *twti, scb_t *scb, twt_ie_indv_t *twt_ie,
	uint8 setup_command, uint8 dialog_token, bool in_pm)
{
	wlc_info_t *wlc = twti->wlc;
	twt_scb_t *scb_twt;
	int err = BCME_OK;
	twt_indv_desc_t *indv = NULL;
	uint8 flow_id;
	uint8 i;
	void *pkt;
	uint8 *body;
	uint8 check_flags;
	uint8 response;
	uint32 wake_interval;
	uint32 wake_duration;
	uint32 noof_bits;
	uint16 test;
	uint32 tsf_l, tsf_h;
	uint32 tmp_l, tmp_h;
	uint32 min_delta;

	/* Find the scb cubby, bail out if NULL */
	if ((scb == NULL) || ((scb_twt = TWT_SCB(twti, scb)) == NULL)) {
		err = BCME_UNSUPPORTED;
		goto fail;
	}

	if (!SCB_HE_CAP(scb)) {
		err = BCME_UNSUPPORTED;
		goto fail;
	}

	/* The main difference on the different setup commands is how to deal with twt
	 * parameter. In case of demand we have to obey, other commands allow a sugesstion to be
	 * made. Complexity should 'grow' here on how to handle, but for now we accept it all.
	 */

	/* AF statemachine free? If not then bail out */
	if (scb_twt->af_state != WLC_TWT_AF_STATE_NONE) {
		err = BCME_NORESOURCE;
		goto fail;
	}

	/* Validate the flow id to be free. If not free then bail out. */
	flow_id = getbits((uint8 *)&twt_ie->request_type, sizeof(twt_ie->request_type),
		TWT_REQ_TYPE_FLOW_ID_IDX, TWT_REQ_TYPE_FLOW_ID_FSZ);
	for (i = 0; i < ARRAYSIZE(scb_twt->indv); i++) {
		if (scb_twt->indv[i].state != WLC_TWT_STATE_INACTIVE) {
			if (scb_twt->indv[i].desc.id == flow_id) {
				/* Already in use */
				err = BCME_BADARG;
				goto fail;
			}
		} else {
			if (indv == NULL) {
				indv = &scb_twt->indv[i];
			}
		}
	}
	if (indv == NULL) {
		/* No more iTWT descriptors available for this sta */
		err = BCME_NORESOURCE;
		goto fail;
	}
	memset(indv, 0, sizeof(*indv));

	/* Are we at the max of total number iTWT supported? */
	if (twti->itwt_count >= WLC_TWT_MAX_ITWT_TOTAL) {
		err = BCME_NORESOURCE;
		goto fail;
	}

	/* Decode the twt_ie into wl twt setup descriptor */
	wlc_twt_parse_twt_setup_ie(twt_ie, indv);

	WL_TWT(("%s Setup id=%d, Trigger = %s, Unannounced = %s, Protection = %s\n",
		__FUNCTION__, flow_id,
		(indv->desc.flow_flags & WL_TWT_FLOW_FLAG_TRIGGER) ? "YES" : "NO",
		(indv->desc.flow_flags & WL_TWT_FLOW_FLAG_UNANNOUNCED) ? "YES" : "NO",
		(indv->desc.flow_flags & WL_TWT_FLOW_FLAG_PROTECTION) ? "YES" : "NO"));
	WL_TWT(("%s twt = %08x:%08x (hex), Duration = %d, Interval (m/e) = %d:%d\n",
		__FUNCTION__, indv->twt_h, indv->twt_l, indv->desc.wake_duration,
		indv->desc.wake_interval_mantissa, indv->desc.wake_interval_exponent));
	/* Validate the wake interval. Troubled thing is that it can be a huge number. The
	 * formula is mantissa * 2 ^ exponent. Mantissa is 16bit and exponent is 5 bit. So the
	 * max value is 16bit + 31 bit = 47 bit large. That is useless. currently an interval more
	 * than max uint32 makes is not supported. But we have to make sure that in the request it
	 * wasn't requested.
	 */
	test = indv->desc.wake_interval_mantissa;
	for (noof_bits = 0; test; test >>= 1) {
		noof_bits++;
	}
	noof_bits += indv->desc.wake_interval_exponent;

	if ((noof_bits > 31) || (!indv->desc.wake_interval_mantissa)) {
		WL_TWT(("%s m=%d, e=%d, no of bits = %d\n", __FUNCTION__,
			indv->desc.wake_interval_mantissa, indv->desc.wake_interval_exponent,
			noof_bits));
		err = BCME_BADARG;
		goto fail;
	}

	/* TWT setup request check: if one or more iTWT exists for this SCB then they should
	 * have the same announced/unannounced and trigger/non-trigger setting. Mixing is
	 * not supported since windows might slide over eachother, making it impossible to
	 * handle.
	 */
	check_flags = WL_TWT_FLOW_FLAG_UNANNOUNCED | WL_TWT_FLOW_FLAG_TRIGGER;
	for (i = 0; i < ARRAYSIZE(scb_twt->indv); i++) {
		if ((scb_twt->indv[i].state == WLC_TWT_STATE_ACTIVE) &&
		    ((scb_twt->indv[i].desc.flow_flags & check_flags) !=
		     (indv->desc.flow_flags & check_flags))) {
			WL_TWT(("%s Unsupported mixed announced modes\n", __FUNCTION__));
			err = BCME_UNSUPPORTED;
			goto fail;
		}
	}

	response = TWT_SETUP_CMD_ACCEPT_TWT;

	/* The iTWT is to be accepted. When the command is demand then we follow the Target Wake
	 * Time. In case of Request or Suggest, the TWT is determined now.
	 */
	if ((setup_command == TWT_SETUP_CMD_REQUEST_TWT) ||
	    (setup_command == TWT_SETUP_CMD_SUGGEST_TWT)) {
		wlc_twt_indv_adjust_twt_params(twti, scb, indv);
		WL_TWT(("%s Accepting twt, start TSF = %08x:%08x (hex)\n",
			__FUNCTION__, indv->twt_h, indv->twt_l));
	} else {
		/* TWT_SETUP_CMD_DEMAND_TWT: Demand can only be accepted if all parameters are
		 * alligned with schedid, so lets verify those. In the rare occassion that this
		 * is true and the schedule is far enough in the future (TWT field) then the
		 * command can be accepted, otherwise use alternate to suggest different schedule.
		 * Check for future TWT is ignored, it is correct in start and "any" value is
		 * accepted here.
		 */
		WL_TWT(("%s setup_command TWT_SETUP_CMD_DEMAND_TWT recvd\n", __FUNCTION__));

		wake_interval = indv->desc.wake_interval_mantissa *
			(1 << indv->desc.wake_interval_exponent);
		wake_duration = ((uint32)indv->desc.wake_duration << 8);
		if ((wake_interval & WLC_TSFL_SCHEDID_MASK_INV) ||
		    (wake_duration & WLC_TSFL_SCHEDID_MASK_INV) ||
		    (indv->twt_l & WLC_TSFL_SCHEDID_MASK_INV)) {
			response = TWT_SETUP_CMD_ALTER_TWT;
			WL_TWT(("%s replying with ALTERNATE\n", __FUNCTION__));
			wlc_twt_indv_adjust_twt_params(twti, scb, indv);
		}
	}

	/* Allocate AF and fill header */
	pkt = wlc_twt_build_af(scb, dialog_token, &body, TRUE);
	if (!pkt) {
		err = BCME_NOMEM;
		goto fail;
	}

	/* Intel client wants TWT which is close (or at least alligned) to their suggested TWT,
	 * which is possible. Some requirements though, we need some minimal time ahead
	 * in future, because the TWT has to be in future and it is uncleare when frame is
	 * transmitted, that is why the PM mode needs to be known. Set lifetime of frame
	 * matching the TWT and on timeout of frames use retry with larger value
	 */
	wlc_twt_get_tsf(wlc, SCB_BSSCFG(scb), &tsf_l, &tsf_h);
	if (in_pm) {
		min_delta = WLC_TWT_FIRST_SP_AHEAD_PM;
	} else {
		min_delta = WLC_TWT_FIRST_SP_AHEAD_NOPM;
	}
	wlc_uint64_add(&tsf_h, &tsf_l, 0, min_delta + WLC_TSFL_SCHEDID_MASK_INV);
	tsf_l &= WLC_TSFL_SCHEDID_MASK;
	/* Now the minimum moment where we would like to have the SP starting is known. Compare
	 * this with suggested TWT. Problem is: TWT value can be kinda bogus. So see if requested
	 * TWT is reasonable close to locally calculated TWT
	 */
	wake_interval = indv->desc.wake_interval_mantissa *
		(1 << indv->desc.wake_interval_exponent);
	tmp_l = indv->twt_l;
	tmp_h = indv->twt_h;
	wlc_uint64_sub(&tmp_h, &tmp_l, tsf_h, tsf_l);
	while (tmp_h == WLC_TWT_TSF_MAX) {
		wlc_uint64_add(&indv->twt_h, &indv->twt_l, 0, wake_interval);
		wlc_uint64_add(&tmp_h, &tmp_l, 0, wake_interval);
	}
	if (tmp_h != 0) {
		/* Too far away, ignore the request, set twt to calculated tsf */
		indv->twt_h = tsf_h;
		indv->twt_l = tsf_l;
	} else {
		/* Check if suggested TWT is reasonably close to future */
		while ((tmp_l > WLC_TWT_FIRST_SP_AHEAD_PM) && (tmp_l > wake_interval + min_delta)) {
			wlc_uint64_sub(&indv->twt_h, &indv->twt_l, 0, wake_interval);
			wlc_uint64_sub(&tmp_h, &tmp_l, 0, wake_interval);
		}
		indv->twt_l &= WLC_TSFL_SCHEDID_MASK;
	}
	/* set frametimeout little bit lower (5 msec) then first calculated SP, make sure lifetime
	 * of packet is earlier then SP and do retry of packet in that case, but avoid getting
	 * TWT in use which is in past (though the scheduler will handle it).
	 */
	wlc_twt_get_tsf(twti->wlc, SCB_BSSCFG(scb), &tsf_l, &tsf_h);
	tmp_l = indv->twt_l;
	tmp_h = indv->twt_h;
	wlc_uint64_sub(&tmp_h, &tmp_l, tsf_h, tsf_l);
	ASSERT(tmp_h == 0);
	tmp_l -= WLC_TWT_AF_LIFETIME_SAFETY;
	wlc_lifetime_set(wlc, pkt, tmp_l);

	/* Fill up AF indvidual part of IE */
	wlc_twt_itwt_build_setup_ie((twt_ie_indv_t *)body, indv, response);
	WLF2_PCB1_REG(pkt, WLF2_PCB1_TWT_AF);

	if (WL_TWT_ON()) {
		prhex("iTWT Accept/Alternate",
			body - S1G_AF_TWT_SETUP_TWT_IE_OFF - WLC_TWT_TOP_IE_SZ,
			S1G_AF_TWT_SETUP_TWT_IE_OFF + WLC_TWT_TOP_IE_SZ + WLC_TWT_INDV_IE_SZ);
	}
	/* commit for transmission */
	if (!wlc_sendmgmt(wlc, pkt, wlc->active_queue, scb)) {
		err = BCME_BUSY;
		/* wlc_sendmgmt() also owns/frees the pkt when returns false, no need to free it */
		goto fail;
	}

	if (response == TWT_SETUP_CMD_ACCEPT_TWT) {
		/* The request has been accepted. Mark the entry as in use */
		indv->state = WLC_TWT_STATE_RESERVED;

		/* Set the wait for ACK, once acked schedule becomes active */
		scb_twt->af_state = WLC_TWT_AF_PEND_SETUP_ITWT_RSP;
		scb_twt->af_fid = indv->desc.id;
		scb_twt->af_retry_count = 1;	/* Only one retry */
		scb_twt->af_dialog_token = dialog_token;
	}

fail:
	if (err != BCME_OK) {
		/* On failure we reply with reject. For this the original twt_ie is used */
		wlc_twt_sta_itwt_reject_send(twti, scb, twt_ie, dialog_token);

		WL_TWT(("wl%d: %s: error=%d\n", wlc->pub->unit, __FUNCTION__, err));
	}
	return err;
}

/**
 * Retry AF setup response. First try failed, retry with updated TWT.
 */
static void
wlc_twt_indv_retry_setup_resp(wlc_twt_info_t *twti, scb_t *scb, twt_indv_desc_t *indv)
{
	wlc_info_t *wlc = twti->wlc;
	twt_scb_t *scb_twt;
	int err = BCME_OK;
	void *pkt;
	uint8 *body;
	uint32 wake_interval;
	uint32 tsf_l, tsf_h;

	/* Find the scb cubby, bail out if NULL */
	if ((scb == NULL) || ((scb_twt = TWT_SCB(twti, scb)) == NULL)) {
		err = BCME_UNSUPPORTED;
		goto fail;
	}

	/* Allocate AF and fill header */
	pkt = wlc_twt_build_af(scb, scb_twt->af_dialog_token, &body, TRUE);
	if (!pkt) {
		err = BCME_NOMEM;
		goto fail;
	}

	/* Update TWT */
	wlc_twt_get_tsf(wlc, SCB_BSSCFG(scb), &tsf_l, &tsf_h);
	wlc_uint64_sub(&tsf_h, &tsf_l, indv->twt_h, indv->twt_l);

	wake_interval = indv->desc.wake_interval_mantissa *
		(1 << indv->desc.wake_interval_exponent);

	while ((tsf_h == WLC_TWT_TSF_MAX) || (tsf_l < WLC_TWT_FIRST_SP_AHEAD_PM)) {
		wlc_uint64_add(&indv->twt_h, &indv->twt_l, 0, wake_interval);
		wlc_uint64_add(&tsf_h, &tsf_l, 0, wake_interval);
	}

	tsf_l -= WLC_TWT_AF_LIFETIME_SAFETY;
	wlc_lifetime_set(wlc, pkt, tsf_l);

	/* Fill up AF indvidual part of IE */
	wlc_twt_itwt_build_setup_ie((twt_ie_indv_t *)body, indv, TWT_SETUP_CMD_ACCEPT_TWT);
	WLF2_PCB1_REG(pkt, WLF2_PCB1_TWT_AF);

	if (WL_TWT_ON()) {
		prhex("iTWT Retry Accept",
			body - S1G_AF_TWT_SETUP_TWT_IE_OFF - WLC_TWT_TOP_IE_SZ,
			S1G_AF_TWT_SETUP_TWT_IE_OFF + WLC_TWT_TOP_IE_SZ + WLC_TWT_INDV_IE_SZ);
	}

	/* commit for transmission */
	if (!wlc_sendmgmt(wlc, pkt, wlc->active_queue, scb)) {
		err = BCME_BUSY;
		/* wlc_sendmgmt() also owns/frees the pkt when returns false, no need to free it */
		goto fail;
	}

	scb_twt->af_state = WLC_TWT_AF_PEND_SETUP_ITWT_RSP;

fail:
	if (err != BCME_OK) {
		WL_ERROR(("%s: Failed to create retry, err=%d\n", __FUNCTION__, err));
		wlc_twt_teardown_itwt(twti, scb, indv, FALSE);
	}
}

/**
 * Action Frame handler for the action_id TWT SETUP where setup command is accept. If a setup
 * is ongoing then finalize it now.
 */
static int
wtc_twt_indv_af_setup_accept_recv(wlc_twt_info_t *twti, scb_t *scb, twt_ie_indv_t *twt_ie,
	uint8 dialog_token)
{
	twt_scb_t *scb_twt;
	twt_indv_desc_t *indv;
	int err = BCME_OK;
	uint8 flow_id;

	/* Find the scb cubby, bail out if NULL */
	if ((scb == NULL) || ((scb_twt = TWT_SCB(twti, scb)) == NULL)) {
		err = BCME_UNSUPPORTED;
		goto fail;
	}

	if (!SCB_HE_CAP(scb)) {
		err = BCME_UNSUPPORTED;
		goto fail;
	}

	flow_id = getbits((uint8 *)&twt_ie->request_type, sizeof(twt_ie->request_type),
		TWT_REQ_TYPE_FLOW_ID_IDX, TWT_REQ_TYPE_FLOW_ID_FSZ);

	if (flow_id != scb_twt->af_fid) {
		WL_TWT(("%s Unexpected Flow ID recv=%d, expected=%d\n",
			__FUNCTION__, flow_id, scb_twt->af_fid));
		/* TBD - goto fail? */
	}

	indv = wlc_twt_find_fid(scb_twt, flow_id);
	if (!indv) {
		WL_TWT(("%s No indv: Unexpected Flow ID recv=%d, expected=%d\n",
			__FUNCTION__, flow_id, scb_twt->af_fid));
		err = BCME_BADARG;
		goto fail;
	}

	/* Decode the twt_ie into wl twt setup descriptor */
	wlc_twt_parse_twt_setup_ie(twt_ie, indv);

	/* Validate settings - TBD */
	WL_TWT(("%s Accept id=%d, Trigger = %s, Unannounced = %s, Protection = %s\n",
		__FUNCTION__, flow_id,
		(indv->desc.flow_flags & WL_TWT_FLOW_FLAG_TRIGGER) ? "YES" : "NO",
		(indv->desc.flow_flags & WL_TWT_FLOW_FLAG_UNANNOUNCED) ? "YES" : "NO",
		(indv->desc.flow_flags & WL_TWT_FLOW_FLAG_PROTECTION) ? "YES" : "NO"));
	WL_TWT(("%s twt = %08x:%08x (hex), Duration = %d, Interval (m/e) = %d:%d\n",
		__FUNCTION__, indv->twt_h, indv->twt_l, indv->desc.wake_duration,
		indv->desc.wake_interval_mantissa, indv->desc.wake_interval_exponent));

	/* Clear states, setup complete */
	scb_twt->af_state = WLC_TWT_AF_STATE_NONE;
	scb_twt->af_timeout = 0;

	wlc_twt_setup_itwt_complete(twti, scb, indv);

fail:
	if (err != BCME_OK) {
		WL_TWT(("wl%d: %s: error=%d\n", twti->wlc->pub->unit, __FUNCTION__, err));
	}
	return err;
}

/**
 * Action Frame handler for the action_id TWT SETUP where setup command is reject. If a setup
 * is ongoing then abort it now. No validateion of dialog token or fid, just error print if
 * incorrect. There should be no reject unless setup is ongoing.
 */
static int
wlc_twt_indv_af_setup_reject_recv(wlc_twt_info_t *twti, scb_t *scb, twt_ie_indv_t *twt_ie,
	uint8 dialog_token)
{
	twt_scb_t *scb_twt;
	twt_indv_desc_t *indv;
	int err = BCME_OK;
	uint8 flow_id;

	/* Find the scb cubby, bail out if NULL */
	if ((scb == NULL) || ((scb_twt = TWT_SCB(twti, scb)) == NULL)) {
		err = BCME_UNSUPPORTED;
		goto fail;
	}

	if (!SCB_HE_CAP(scb)) {
		err = BCME_UNSUPPORTED;
		goto fail;
	}

	/* There is a possibity we get the response before we handle the ACK */
	if ((scb_twt->af_state == WLC_TWT_AF_PEND_SETUP_ITWT_REQ) ||
	    (scb_twt->af_state == WLC_TWT_AF_AWAIT_SETUP_ITWT_RSP)) {
		scb_twt->af_timeout = 0;
		scb_twt->af_state = WLC_TWT_AF_STATE_NONE;
		if (dialog_token != scb_twt->af_dialog_token) {
			WL_TWT(("%s Unexpected Dialog Token recv=%d, expected=%d\n",
				__FUNCTION__, dialog_token, scb_twt->af_dialog_token));
		}
		flow_id = getbits((uint8 *)&twt_ie->request_type, sizeof(twt_ie->request_type),
			TWT_REQ_TYPE_FLOW_ID_IDX, TWT_REQ_TYPE_FLOW_ID_FSZ);
		if (flow_id != scb_twt->af_fid) {
			WL_TWT(("%s Unexpected Flow ID recv=%d, expected=%d\n",
				__FUNCTION__, flow_id, scb_twt->af_fid));
		}
		WL_TWT(("%s id=%d\n", __FUNCTION__, flow_id));

		indv = wlc_twt_find_fid(scb_twt, flow_id);
		if (!indv) {
			WL_TWT(("%s No indv: Unexpected Flow ID received=%d\n",
				__FUNCTION__, flow_id));
		} else {
			wlc_twt_teardown_itwt(twti, scb, indv, FALSE);
		}
	} else {
		WL_TWT(("%s Unhandled Reject AF, state=%d, ea=" MACF "\n",
			__FUNCTION__, scb_twt->af_state, ETHERP_TO_MACF(&scb->ea)));
	}

fail:
	if (err != BCME_OK) {
		WL_TWT(("wl%d: %s: error=%d\n", twti->wlc->pub->unit, __FUNCTION__, err));
	}
	return err;
}

/**
 * Individual Action Frame handler for the action_id TWT SETUP. First part of body has been parsed,
 * and it was determined to be individual TWT SETUP command.
 */
static int
wlc_twt_indv_af_setup_recv(wlc_twt_info_t *twti, scb_t *scb, uint8 *body, uint body_len,
	uint8 dialog_token, uint16 fc)
{
	uint8 setup_command;
	twt_ie_indv_t *twt_ie;
	int err = BCME_OK;

	if (body_len < WLC_TWT_INDV_IE_SZ) {
		err = BCME_BUFTOOSHORT;
		goto fail;
	}
	/* Validate 'TWT request' field of the Request type - TBD */
	/* Validate 'Implicit' field of the Request type to be 1 - TBD */
	twt_ie = (twt_ie_indv_t *)body;
	setup_command = getbits((uint8 *)&twt_ie->request_type, sizeof(twt_ie->request_type),
		TWT_REQ_TYPE_SETUP_CMD_IDX, TWT_REQ_TYPE_SETUP_CMD_FSZ);
	WL_TWT(("%s AID %d Setup received, command %d\n", __FUNCTION__, SCB_AID(scb),
		setup_command));

	/* Process Request Type field. Dispatch based upon TWT Setup Command: */
	switch (setup_command) {
	case TWT_SETUP_CMD_REQUEST_TWT:
	case TWT_SETUP_CMD_SUGGEST_TWT:
	case TWT_SETUP_CMD_DEMAND_TWT:
		err = wlc_twt_indv_af_setup_req_recv(twti, scb, twt_ie, setup_command,
			dialog_token, fc & FC_PM ? TRUE : FALSE);
		break;
	case TWT_SETUP_CMD_ACCEPT_TWT:
		err = wtc_twt_indv_af_setup_accept_recv(twti, scb, twt_ie, dialog_token);
		break;
	case TWT_SETUP_CMD_REJECT_TWT:
		err = wlc_twt_indv_af_setup_reject_recv(twti, scb, twt_ie, dialog_token);
		break;
	case TWT_SETUP_CMD_GRPING_TWT:
	case TWT_SETUP_CMD_ALTER_TWT:
	case TWT_SETUP_CMD_DICTATE_TWT:
	default:
		WL_TWT(("wl%d: %s: Unsupported setup command %d\n", twti->wlc->pub->unit,
			__FUNCTION__, setup_command));
		err = BCME_UNSUPPORTED;
		break;
	}

fail:
	return err;
}

/**
 * Send Reject Action Frame. The reject frame is made of the TWT IE which caused this reject.
 */
static int
wlc_twt_sta_bcast_reject_send(wlc_twt_info_t *twti, scb_t *scb, twt_ie_bcast_t *twt_ie,
	uint8 dialog_token)
{
	int err = BCME_OK;
	void *pkt = NULL;
	uint8 *body;

	pkt = wlc_twt_build_af(scb, dialog_token, &body, FALSE);
	if (!pkt) {
		err = BCME_NOMEM;
		goto fail;
	}

	memcpy(body, twt_ie, WLC_TWT_BCAST_IE_SZ);
	twt_ie = (twt_ie_bcast_t *)body;

	setbits((uint8 *)&twt_ie->request_type, sizeof(twt_ie->request_type),
		TWT_REQ_TYPE_SETUP_CMD_IDX, TWT_REQ_TYPE_SETUP_CMD_FSZ, TWT_SETUP_CMD_REJECT_TWT);

	if (WL_TWT_ON()) {
		prhex("bTWT Reject", body - S1G_AF_TWT_SETUP_TWT_IE_OFF - WLC_TWT_TOP_IE_SZ,
			S1G_AF_TWT_SETUP_TWT_IE_OFF + WLC_TWT_TOP_IE_SZ + WLC_TWT_BCAST_IE_SZ);
	}
	if (!wlc_sendmgmt(twti->wlc, pkt, twti->wlc->active_queue, scb)) {
		err = BCME_BUSY;
	}

fail:
	return err;
}

/**
 * Action Frame handler for the action_id TWT SETUP where setup command is reqeust, suggest or
 * demand. The negotiation type was determined to be broadcast.
 */
static int
wlc_twt_bcast_af_setup_req_recv(wlc_twt_info_t *twti, scb_t *scb, twt_ie_bcast_t *twt_ie,
	uint8 setup_command, uint8 dialog_token)
{
	wlc_info_t *wlc = twti->wlc;
	twt_scb_t *scb_twt;
	twt_bss_t *bss_twt;
	twt_indv_desc_t *indv = NULL;
	int err = BCME_OK;
	uint8 bid;
	uint8 i;
	void *pkt;
	uint8 *body;
	twt_ie_bcast_t *bcast;
	uint32 twt_l, twt_h;
	uint32 tsf_l, tsf_h;
	uint16 twt;
	uint32 wake_interval;

	/* Find the scb cubby, bail out if NULL */
	if ((scb == NULL) || ((scb_twt = TWT_SCB(twti, scb)) == NULL)) {
		err = BCME_UNSUPPORTED;
		goto fail;
	}

	if (!SCB_HE_CAP(scb)) {
		err = BCME_UNSUPPORTED;
		goto fail;
	}

	bss_twt = TWT_BSS(twti, scb->bsscfg);
	if (bss_twt == NULL) {
		err = BCME_UNSUPPORTED;
		goto fail;
	}

	/* For now do not handle Suggest TWT or Demand TWT. It would require to create a new
	 * BTWT scheduler, but that is not something to be done for the moment. Host should
	 * configure BTWT schedules. NOTE: we could accept suggest if it could be mapped to
	 * existing schedule, as standard allows for sending back different params !
	 */
	if ((setup_command == TWT_SETUP_CMD_SUGGEST_TWT) ||
		(setup_command == TWT_SETUP_CMD_DEMAND_TWT)) {

		WL_TWT(("%s BTWT Suggest or Demand currently not supported\n", __FUNCTION__));
		err = BCME_UNSUPPORTED;
		goto fail;
	}

	/* AF statemachine free? If not then bail out */
	if (scb_twt->af_state != WLC_TWT_AF_STATE_NONE) {
		err = BCME_NORESOURCE;
		goto fail;
	}

	/* Does the bid exist? */
	bid = getbits((uint8 *)&twt_ie->bcast_info, sizeof(twt_ie->bcast_info),
		TWT_BCAST_INFO_ID_IDX, TWT_BCAST_INFO_ID_FSZ);
	if (bss_twt->bcast[bid].state == WLC_TWT_STATE_INACTIVE) {
		WL_TWT(("%s BTWT CMD_REQUEST bid %d not configured\n", __FUNCTION__, bid));
		err = BCME_NORESOURCE;
		goto fail;
	}

	/* Schedule exists. A lot of verification could be made, but why? client wants to join
	 * bcast schedule, lets just accept it. Only requirement left is that there is a iTWT
	 * schedule available, as that is what is being used to do the scheduling.
	 */
	for (i = 0; i < ARRAYSIZE(scb_twt->indv); i++) {
		if (scb_twt->indv[i].state != WLC_TWT_STATE_INACTIVE) {
			if (scb_twt->indv[i].desc.id == WLC_TWT_BID2FID(bid)) {
				/* Already in use */
				err = BCME_BADARG;
				goto fail;
			}
		} else {
			if (indv == NULL) {
				indv = &scb_twt->indv[i];
			}
		}
	}
	if (indv == NULL) {
		/* No more iTWT descriptors available for this sta */
		err = BCME_NORESOURCE;
		goto fail;
	}
	memset(indv, 0, sizeof(*indv));

	/* Are we at the max of total number iTWT supported? */
	if (twti->itwt_count >= WLC_TWT_MAX_ITWT_TOTAL) {
		err = BCME_NORESOURCE;
		goto fail;
	}

	/* Find timestamp where first SP can be scheduled */
	wlc_twt_get_tsf(wlc, SCB_BSSCFG(scb), &tsf_l, &tsf_h);
	wlc_uint64_add(&tsf_h, &tsf_l, 0, WLC_TWT_FIRST_SP_AHEAD_PM);
	twt_h = bss_twt->bcast[bid].twt_h;
	twt_l = bss_twt->bcast[bid].twt_l;
	wake_interval = bss_twt->bcast[bid].desc.wake_interval_mantissa *
		(1 << bss_twt->bcast[bid].desc.wake_interval_exponent);
	while (((tsf_h - twt_h) == 1) || ((tsf_h == twt_h) && (tsf_l > twt_l))) {
		wlc_uint64_add(&twt_h, &twt_l, 0, wake_interval);
	}

	/* Copy the broadcast schedule information to the individual configuration */
	indv->desc = bss_twt->bcast[bid].desc;
	indv->desc.id = WLC_TWT_BID2FID(bid);

	indv->twt_l = twt_l;
	indv->twt_h = twt_h;

	/* Allocate AF and fill header */
	pkt = wlc_twt_build_af(scb, dialog_token, &body, FALSE);
	if (!pkt) {
		err = BCME_NOMEM;
		goto fail;
	}

	twt = (twt_l >> WLC_TWT_BTWT_SHIFT) & WLC_TWT_BTWT_MASK;
	wlc_twt_ie_add_single_bcast_ie(body, &bss_twt->bcast[bid], TWT_SETUP_CMD_ACCEPT_TWT, twt);
	bcast = (twt_ie_bcast_t *)body;
	setbit(&bcast->request_type, TWT_REQ_TYPE_IMPL_LAST_IDX);
	WLF2_PCB1_REG(pkt, WLF2_PCB1_TWT_AF);

	if (WL_TWT_ON()) {
		prhex("bTWT Accept/Alternate",
			body - S1G_AF_TWT_SETUP_TWT_IE_OFF - WLC_TWT_TOP_IE_SZ,
			S1G_AF_TWT_SETUP_TWT_IE_OFF + WLC_TWT_TOP_IE_SZ + WLC_TWT_BCAST_IE_SZ);
	}
	/* commit for transmission */
	if (!wlc_sendmgmt(wlc, pkt, wlc->active_queue, scb)) {
		err = BCME_BUSY;
		/* wlc_sendmgmt() also owns/frees the pkt when returns false, no need to free it */
		goto fail;
	}

	/* All went well, set state to reserved */
	indv->state = WLC_TWT_STATE_RESERVED;

	/* Set the wait for ACK, once acked schedule becomes active */
	scb_twt->af_state = WLC_TWT_AF_PEND_SETUP_BTWT_RSP;
	scb_twt->af_fid = indv->desc.id;

fail:
	if (err != BCME_OK) {
		/* On failure we reply with reject. For this the original twt_ie is used */
		wlc_twt_sta_bcast_reject_send(twti, scb, twt_ie, dialog_token);

		WL_TWT(("wl%d: %s: error=%d\n", wlc->pub->unit, __FUNCTION__, err));
	}
	return err;
}

/**
 * Action Frame handler for the action_id TWT SETUP where setup command is accept. If a setup
 * is ongoing then finalize it now.
 */
static int
wtc_twt_bast_af_setup_accept_recv(wlc_twt_info_t *twti, scb_t *scb, twt_ie_bcast_t *twt_ie,
	uint8 dialog_token)
{
	twt_scb_t *scb_twt;
	twt_indv_desc_t *indv;
	int err = BCME_OK;
	uint8 bid;
	uint32 twt;

	/* Find the scb cubby, bail out if NULL */
	if ((scb == NULL) || ((scb_twt = TWT_SCB(twti, scb)) == NULL)) {
		err = BCME_UNSUPPORTED;
		goto fail;
	}

	bid = getbits((uint8 *)&twt_ie->bcast_info, sizeof(twt_ie->bcast_info),
		TWT_BCAST_INFO_ID_IDX, TWT_BCAST_INFO_ID_FSZ);

	if (bid != WLC_TWT_FID2BID(scb_twt->af_fid)) {
		WL_TWT(("%s Unexpected Broadcast ID recv=%d, expected=%d\n",
			__FUNCTION__, bid, WLC_TWT_FID2BID(scb_twt->af_fid)));
		/* Continue, the indv wont be found most likely */
	}

	indv = wlc_twt_find_fid(scb_twt, WLC_TWT_BID2FID(bid));
	if (indv == NULL) {
		err = BCME_BADARG;
		goto fail;
	}

	if (dialog_token != scb_twt->af_dialog_token) {
		WL_TWT(("%s Unexpected Dialog Token recv=%d, expected=%d\n",
			__FUNCTION__, dialog_token, scb_twt->af_dialog_token));
	}

	/* Parse the basics of the TWT IE */
	wlc_twt_ie_parse_single_bcast_ie(twt_ie, &indv->desc);
	/* Need to map back the bid to fid !! */
	indv->desc.id = WLC_TWT_BID2FID(indv->desc.id);

	/* Now parse the TWT and adjust to current TSF */
	twt = ltoh16(twt_ie->twt);
	twt <<= WLC_TWT_BTWT_SHIFT;
	wlc_twt_get_tsf(twti->wlc, SCB_BSSCFG(scb), &indv->twt_l, &indv->twt_h);
	if (twt < (indv->twt_l & WLC_TWT_BTWT_TSF_MASK)) {
		indv->twt_l += (1 << (WLC_TWT_BTWT_SHIFT + 16));
	}
	indv->twt_l &= ~WLC_TWT_BTWT_TSF_MASK;
	indv->twt_l |= twt;

	WL_TWT(("%s Recv Broadcast ACCEPT, setting up iTWT bid=%d\n",
		__FUNCTION__, bid));
	WL_TWT(("%s Trigger = %s, Unannounced = %s, Protection = %s\n",
		__FUNCTION__,
		(indv->desc.flow_flags & WL_TWT_FLOW_FLAG_TRIGGER) ? "YES" : "NO",
		(indv->desc.flow_flags & WL_TWT_FLOW_FLAG_UNANNOUNCED) ? "YES" : "NO",
		(indv->desc.flow_flags & WL_TWT_FLOW_FLAG_PROTECTION) ? "YES" : "NO"));
	WL_TWT(("%s twt = %08x:%08x (hex), Duration = %d, Interval (m/e) = %d:%d\n",
		__FUNCTION__, indv->twt_h, indv->twt_l, indv->desc.wake_duration,
		indv->desc.wake_interval_mantissa, indv->desc.wake_interval_exponent));

	/* Clear statemachine */
	scb_twt->af_timeout = 0;
	scb_twt->af_state = WLC_TWT_AF_STATE_NONE;

	wlc_twt_setup_itwt_complete(twti, scb, indv);

fail:
	return err;
}

/**
 * Broadcast Action Frame handler for the action_id TWT SETUP, with setup command equal to 3
 * (reject), which is to be treated as teardwon request.
 */
static int
wlc_twt_bcast_af_setup_reject_recv(wlc_twt_info_t *twti, scb_t *scb, twt_ie_bcast_t *twt_ie,
	uint8 dialog_token, bool enter_pm)
{
	twt_scb_t *scb_twt;
	int err = BCME_OK;
	uint8 bid;

	/* Find the scb cubby, bail out if NULL */
	if ((scb == NULL) || ((scb_twt = TWT_SCB(twti, scb)) == NULL)) {
		err = BCME_UNSUPPORTED;
		goto fail;
	}

	bid = getbits((uint8 *)&twt_ie->bcast_info, sizeof(twt_ie->bcast_info),
		TWT_BCAST_INFO_ID_IDX, TWT_BCAST_INFO_ID_FSZ);
	WL_TWT(("%s bid=%d\n", __FUNCTION__, bid));

	/* There is a possibity we get the response before we handle the ACK */
	if ((scb_twt->af_state == WLC_TWT_AF_PEND_SETUP_BTWT_REQ) ||
	    (scb_twt->af_state == WLC_TWT_AF_AWAIT_SETUP_BTWT_RSP)) {
		scb_twt->af_timeout = 0;
		scb_twt->af_state = WLC_TWT_AF_STATE_NONE;
		if (dialog_token != scb_twt->af_dialog_token) {
			WL_TWT(("%s Unexpected Dialog Token recv=%d, expected=%d\n",
				__FUNCTION__, dialog_token, scb_twt->af_dialog_token));
		}
		if (bid != WLC_TWT_FID2BID(scb_twt->af_fid)) {
			WL_TWT(("%s Unexpected Flow ID recv=%d, expected=%d\n",
				__FUNCTION__, bid, WLC_TWT_FID2BID(scb_twt->af_fid)));
		}
	}

	err = wlc_twt_teardown_btwt(twti, scb, bid, enter_pm);

fail:
	return err;
}

/**
 * Broadcast Action Frame handler for the action_id TWT SETUP. First part of body has been parsed,
 * and it was determined to be broadcast TWT SETUP command.
 */
static int
wlc_twt_bcast_af_setup_recv(wlc_twt_info_t *twti, scb_t *scb, uint8 *body, uint body_len,
	uint8 dialog_token, uint16 fc)
{
	uint8 setup_command;
	twt_ie_bcast_t *twt_ie;
	int err = BCME_OK;

	if (body_len < WLC_TWT_BCAST_IE_SZ) {
		err = BCME_BUFTOOSHORT;
		goto fail;
	}
	twt_ie = (twt_ie_bcast_t *)body;
	setup_command = getbits((uint8 *)&twt_ie->request_type, sizeof(twt_ie->request_type),
		TWT_REQ_TYPE_SETUP_CMD_IDX, TWT_REQ_TYPE_SETUP_CMD_FSZ);
	WL_TWT(("%s AID %d Setup received, command %d\n", __FUNCTION__, SCB_AID(scb),
		setup_command));

	/* Process Request Type field. Dispatch based upon TWT Setup Command: */
	switch (setup_command) {
	case TWT_SETUP_CMD_REQUEST_TWT:
	case TWT_SETUP_CMD_SUGGEST_TWT:
	case TWT_SETUP_CMD_DEMAND_TWT:
		err = wlc_twt_bcast_af_setup_req_recv(twti, scb, twt_ie, setup_command,
			dialog_token);
		break;
	case TWT_SETUP_CMD_REJECT_TWT:
		err = wlc_twt_bcast_af_setup_reject_recv(twti, scb, twt_ie, dialog_token,
			fc & FC_PM ? TRUE : FALSE);
		break;
	case TWT_SETUP_CMD_ACCEPT_TWT:
		err = wtc_twt_bast_af_setup_accept_recv(twti, scb, twt_ie, dialog_token);
		break;
	case TWT_SETUP_CMD_GRPING_TWT:
	case TWT_SETUP_CMD_ALTER_TWT:
	case TWT_SETUP_CMD_DICTATE_TWT:
	default:
		WL_TWT(("wl%d: %s: Unsupported setup command %d\n", twti->wlc->pub->unit,
			__FUNCTION__, setup_command));
		err = BCME_UNSUPPORTED;
		break;
	}

fail:
	return err;
}

/**
 * Action Frame handler for the action_id TWT SETUP. The setup frame is a dialog_token followed
 * by a TWT element. dialog_token is for matching request/response transaction.
 */
static int
wlc_twt_af_setup_recv(wlc_twt_info_t *twti, scb_t *scb, uint8 *body, uint body_len, uint16 fc)
{
	twt_ie_top_t *twt_top;
	uint8 dialog_token;
	uint8 negotiation;
	int err = BCME_OK;

	/* First len check, TWT element is variabel length, check for basic twt hdr IE */
	if (body_len <= S1G_AF_TWT_SETUP_TWT_IE_OFF + WLC_TWT_TOP_IE_SZ) {
		err = BCME_BUFTOOSHORT;
		WL_TWT(("wl%d: %s: buffer to short %d expected at least %d\n",
			twti->wlc->pub->unit, __FUNCTION__, body_len,
			(int)(S1G_AF_TWT_SETUP_TWT_IE_OFF + WLC_TWT_TOP_IE_SZ)));
		goto fail;
	}

	dialog_token = body[S1G_AF_TWT_SETUP_TOKEN_OFF];

	body += S1G_AF_TWT_SETUP_TWT_IE_OFF;
	twt_top = (twt_ie_top_t *)body;

	/* At this point the only acceptable negotiation types are INDV_REQ_RSP and BCAST_MGMT */
	negotiation = (twt_top->ctrl & TWT_CTRL_NEGOTIATION_MASK) >> TWT_CTRL_NEGOTIATION_SHIFT;

	body_len -= S1G_AF_TWT_SETUP_TWT_IE_OFF;
	body_len -= WLC_TWT_TOP_IE_SZ;
	body += WLC_TWT_TOP_IE_SZ;

	if (negotiation == TWT_CTRL_NEGO_INDV_REQ_RSP) {
		err = wlc_twt_indv_af_setup_recv(twti, scb, body, body_len, dialog_token, fc);
	} else if (negotiation == TWT_CTRL_NEGO_BCAST_MGMT) {
		err = wlc_twt_bcast_af_setup_recv(twti, scb, body, body_len, dialog_token, fc);
	} else {
		err = BCME_BADARG;
		WL_TWT(("wl%d: %s: negotiation type %d not supported\n", twti->wlc->pub->unit,
			__FUNCTION__, negotiation));
	}

fail:
	return err;
}

/**
 * Broadcast twt teardown. This function can be called when an AF is received with teardown
 * command or when the sending of the AF twt teardown is complete. If we are AP then remove
 * the registration of the station to this schedule, if we are non-AP then remove the complete
 * schedule.
 */
static int
wlc_twt_teardown_btwt(wlc_twt_info_t *twti, scb_t *scb, uint8 bid, bool enter_pm)
{
	twt_scb_t *scb_twt;
	twt_indv_desc_t *indv;
	int err = BCME_OK;

	BCM_REFERENCE(bid);

	/* Find the scb cubby, bail out if NULL */
	if ((scb == NULL) || ((scb_twt = TWT_SCB(twti, scb)) == NULL)) {
		err = BCME_UNSUPPORTED;
		goto fail;
	}

	indv = wlc_twt_find_fid(scb_twt, WLC_TWT_BID2FID(bid));

	if (indv == NULL) {
		WL_TWT(("%s No indv: Unexpected Broadcast ID %d in TWT AF\n",
			__FUNCTION__, bid));
		err = BCME_BADARG;
		goto fail;
	}

	err = wlc_twt_teardown_itwt(twti, scb, indv, enter_pm);

fail:
	return err;
}

/**
 * Action Frame handler for the action_id TWT TEARDOWN
 */
static int
wlc_twt_af_teardown_recv(wlc_twt_info_t *twti, scb_t *scb, uint8 *body, uint body_len, uint16 fc)
{
	int err = BCME_OK;

	if (body_len < S1G_AF_TWT_TEARDOWN_FLOW_OFF) {
		err = BCME_BUFTOOSHORT;
		goto fail;
	}

	err = wlc_twt_teardown(twti, scb, body[S1G_AF_TWT_TEARDOWN_FLOW_OFF],
		fc & FC_PM ? TRUE : FALSE);

fail:
	return err;
}

/**
 * Action Frame handler for the action_id TWT INFORMATION:
 *
 * FC is the D11 FC as used by remote to transmit this frame, in case of suspend it determines
 * the PM mode of the suspended TWT link.
 */
static int
wlc_twt_af_info_recv(wlc_twt_info_t *twti, scb_t *scb, uint8 *body, uint body_len, uint16 fc)
{
	wlc_info_t *wlc = twti->wlc;
	twt_scb_t *scb_twt;
	twt_indv_desc_t *indv;
	int err = BCME_OK;
	uint8 twt_info;
	uint8 flow_id;
	uint32 tsf_h, tsf_l;
	uint32 twt_h, twt_l;
	bool resume;
	uint8 next_twt_size;

	BCM_REFERENCE(resume); /* TO BE REMOVED */

	/* First len check, TWT element is variabel length, but minimal 1 byte */
	if (body_len < S1G_AF_TWT_INFO_OFF) {
		err = BCME_BUFTOOSHORT;
		goto fail;
	}

	/* Find the scb cubby, bail out if NULL */
	if ((scb == NULL) || ((scb_twt = TWT_SCB(twti, scb)) == NULL)) {
		err = BCME_UNSUPPORTED;
		goto fail;
	}

	if (!SCB_HE_CAP(scb)) {
		err = BCME_UNSUPPORTED;
		goto fail;
	}

	twt_info = body[S1G_AF_TWT_INFO_OFF];

	/* Validate res_req and next_twt_rw to be 0 ! */
	if (twt_info & TWT_INFO_RESP_REQ) {
		err = BCME_BADARG;
		WL_TWT(("wl%d: %s: Response Requested should have been 0\n", twti->wlc->pub->unit,
			__FUNCTION__));
		goto fail;
	}
	if (twt_info & TWT_INFO_NEXT_TWT_REQ) {
		err = BCME_BADARG;
		WL_TWT(("wl%d: %s: Next TWT Request should have been 0\n", twti->wlc->pub->unit,
			__FUNCTION__));
		goto fail;
	}

	/* Determine flowid for which info was received */
	if (twt_info & TWT_INFO_ALL_TWT) {
		flow_id = TWT_INFO_ALL_FLOW_ID;
		indv = NULL;
	} else {
		flow_id = twt_info & TWT_INFO_FLOW_ID_MASK;
		/* find the the info */
		indv = wlc_twt_find_fid(scb_twt, flow_id);
		if (!indv) {
			WL_TWT(("%s No indv: Unexpected Flow ID in TWT Information=%d\n",
				__FUNCTION__, flow_id));
			err = BCME_BADARG;
			goto fail;
		}
	}

	wlc_twt_get_tsf(wlc, wlc->primary_bsscfg, &tsf_l, &tsf_h);
	WL_TWT(("%s TSF=%08x:%08x\n", __FUNCTION__, tsf_h, tsf_l));

	resume = FALSE;
	next_twt_size = (twt_info & TWT_INFO_NEXT_TWT_SIZE_MASK) >> TWT_INFO_NEXT_TWT_SIZE_SHIFT;
	if (next_twt_size) {
		resume = TRUE;
		/* The is to be a next TWT, validate the length of message accordingly */
		/* 1: 32bit, 2: 48 bit, 3: 64. => noof_bits = 16 * size * 16 */
		if (next_twt_size == TWT_INFO_NEXTTWT_SIZE_1) {
			if (body_len < S1G_AF_TWT_INFO_OFF + TWT_INFO_SIZE_INFO +
				(TWT_INFO_NEXTTWT_SIZE_1_BITS / 8)) {
				err = BCME_BUFTOOSHORT;
				WL_TWT(("wl%d: %s: buffer too short (1) %d\n",
					twti->wlc->pub->unit, __FUNCTION__, body_len));
				goto fail;
			}
			twt_l = ltoh32_ua(&body[S1G_AF_TWT_INFO_OFF + TWT_INFO_SIZE_INFO]);
			WL_TWT(("%s SIZE1 resume at twt=%08x:%08x\n", __FUNCTION__, 0, twt_l));
			twt_h = tsf_h;
			if (twt_l < tsf_l) {
				if ((tsf_l - twt_l) > 0x7fffffff) {
					/* Wrap around assumption */
					twt_h++;
				} else {
					/* Problem. specified TWT is in past. If this applies to
					 * all TWT then TWT inidicates minimum time, so just set
					 * it to TSF, otherwise use flow info wake interval to
					 * correct.
					 */
					if (flow_id == TWT_INFO_ALL_FLOW_ID) {
						twt_l = tsf_l;
					} else {
						wlc_uint64_add(&twt_h, &twt_l, 0,
							indv->wake_interval);

					}
				}
			}
		}
		if (next_twt_size == TWT_INFO_NEXTTWT_SIZE_2) {
			if (body_len < S1G_AF_TWT_INFO_OFF + TWT_INFO_SIZE_INFO +
				(TWT_INFO_NEXTTWT_SIZE_2_BITS / 8)) {
				err = BCME_BUFTOOSHORT;
				WL_TWT(("wl%d: %s: buffer too short (2) %d\n",
					twti->wlc->pub->unit, __FUNCTION__, body_len));
				goto fail;
			}
			twt_l = ltoh32_ua(&body[S1G_AF_TWT_INFO_OFF + TWT_INFO_SIZE_INFO]);
			twt_h = ltoh16_ua(&body[S1G_AF_TWT_INFO_OFF + TWT_INFO_SIZE_INFO + 4]);
			WL_TWT(("%s SIZE2 resume at twt=%08x:%08x\n", __FUNCTION__, twt_h, twt_l));
			/* This is the most tricky one. There are two situations where twt can
			 * be assumed in passed. That is if the twt_h is 1 smaller then tsf_h or
			 * when twt_h == tsf_h and twt_l < tsf_l.
			 */
			if (twt_h == (tsf_h & 0xffff)) {
				twt_h = tsf_h;		/* "add" higher bits */
				/* now check twt_l, can still be time in past */
				if (twt_l < tsf_l) {
					if (flow_id == TWT_INFO_ALL_FLOW_ID) {
						twt_l = tsf_l;
						twt_h = tsf_h;
					} else {
						while ((twt_l < tsf_l) && (twt_h == tsf_h)) {
							wlc_uint64_add(&twt_h, &twt_l, 0,
								indv->wake_interval);
						}
					}
				}
			} else if (twt_h == ((tsf_h - 1) & 0xffff)) {
				/* Assume time in past */
				if (flow_id == TWT_INFO_ALL_FLOW_ID) {
					twt_l = tsf_l;
					twt_h = tsf_h;
				} else {
					while ((twt_h == ((tsf_h - 1) & 0xffff)) ||
					       (twt_l < tsf_l)) {
						wlc_uint64_add(&twt_h, &twt_l, 0,
							indv->wake_interval);
					}
				}
			} else {
				twt_h += (tsf_h & 0xffff0000);
			}

		}
		if (next_twt_size == TWT_INFO_NEXTTWT_SIZE_3) {
			if (body_len < S1G_AF_TWT_INFO_OFF + TWT_INFO_SIZE_INFO +
				(TWT_INFO_NEXTTWT_SIZE_3_BITS / 8)) {
				err = BCME_BUFTOOSHORT;
				WL_TWT(("wl%d: %s: buffer too short (3) %d\n",
					twti->wlc->pub->unit, __FUNCTION__, body_len));
				goto fail;
			}
			twt_l = ltoh32_ua(&body[S1G_AF_TWT_INFO_OFF + TWT_INFO_SIZE_INFO]);
			twt_h = ltoh32_ua(&body[S1G_AF_TWT_INFO_OFF + TWT_INFO_SIZE_INFO + 4]);
			WL_TWT(("%s SIZE3 resume at twt=%08x:%08x\n", __FUNCTION__, twt_h, twt_l));
			if (((twt_l < tsf_l) && (twt_h == tsf_h)) || (twt_h == (tsf_h - 1))) {
				if (flow_id == TWT_INFO_ALL_FLOW_ID) {
					twt_l = tsf_l;
					twt_h = tsf_h;
				} else {
					while ((twt_h == (tsf_h - 1)) ||
					       ((twt_l < tsf_l) && (twt_h == tsf_h))) {
						wlc_uint64_add(&twt_h, &twt_l, 0,
							indv->wake_interval);
					}
				}
			}
		}
	}

	err = wlc_twt_information(twti, scb, flow_id, resume, fc, twt_l, twt_h);

fail:
	return err;
}

/**
 * Process action frame. When a action frame of type DOT11_ACTION_CAT_S1G is received then this
 * function is to be called.
 */
int
wlc_twt_actframe_proc(wlc_twt_info_t *twti, uint action_id, struct dot11_management_header *hdr,
	scb_t *scb, uint8 *body, uint body_len)
{
	int err = BCME_OK;

	WL_TWT(("wl%d: %s Processing AF with action id %d, AID %d\n", twti->wlc->pub->unit,
		__FUNCTION__, action_id, SCB_AID(scb)));
	if (WL_TWT_ON()) {
		prhex("AF", body, body_len);
	}

	if (!TWT_ENAB(twti->wlc->pub)) {
		return BCME_UNSUPPORTED;
	}

	switch (action_id) {
	case S1G_ACTION_TWT_SETUP:
		err = wlc_twt_af_setup_recv(twti, scb, body, body_len, ltoh16(hdr->fc));
		break;

	case S1G_ACTION_TWT_TEARDOWN:
		err = wlc_twt_af_teardown_recv(twti, scb, body, body_len, ltoh16(hdr->fc));
		break;

	case S1G_ACTION_TWT_INFO:
		err = wlc_twt_af_info_recv(twti, scb, body, body_len, ltoh16(hdr->fc));
		break;

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	if (err != BCME_OK) {
		WL_TWT(("wl%d: %s: TWT AF %d error=%d, AID %d\n", twti->wlc->pub->unit,
			__FUNCTION__, action_id, err, SCB_AID(scb)));
	}
	return err;
}

/**
 * Action Frame tx complete callback - used to complete some of the actions. Set the packet
 * callback function WLF2_PCB1_TWT_AF and this function will be called.
 */
static void
wlc_twt_af_txcomplete(wlc_info_t *wlc, void *pkt, uint txstatus)
{
	wlc_twt_info_t *twti = wlc->twti;
	scb_t *scb = WLPKTTAGSCBGET(pkt);
	twt_scb_t *scb_twt;
	twt_indv_desc_t *indv;
	bool no_ack;
	uint8 af_state;

	/* Find the scb cubby, bail out if NULL */
	if ((scb == NULL) || ((scb_twt = TWT_SCB(twti, scb)) == NULL)) {
		return;
	}

	/* Check suppression status. how to handle? - TBD */
	if ((txstatus & TX_STATUS_SUPR_MASK) >> TX_STATUS_SUPR_SHIFT) {
		WL_ERROR(("%s: txstatus 0x%x, suppress_mask 0x%x \n", __FUNCTION__, txstatus,
			((txstatus & TX_STATUS_SUPR_MASK) >> TX_STATUS_SUPR_SHIFT)));
		return;
	}

	/* Did the remote side acknowledge frame? */
	no_ack = ((txstatus & TX_STATUS_MASK) == TX_STATUS_NO_ACK);
	af_state = scb_twt->af_state;
	scb_twt->af_state = WLC_TWT_AF_STATE_NONE;
	scb_twt->af_timeout = 0;
	switch (af_state) {
	case WLC_TWT_AF_PEND_TEARDOWN:
		if (no_ack) {
			WL_ERROR(("%s: Teardown failed (0x%02x), no ACK (txstatus=0x%x)\n",
				__FUNCTION__, scb_twt->af_teardown, txstatus));
			wlc_twt_teardown_retry(twti, scb, scb_twt->af_teardown);
		} else {
			wlc_twt_teardown(twti, scb, scb_twt->af_teardown,
				txstatus & TX_STATUS_PMINDCTD ? TRUE : FALSE);
		}
		break;
	case WLC_TWT_AF_PEND_SETUP_BTWT_REQ: /* bTWT uses the iTWT to do the atual SP */
	case WLC_TWT_AF_PEND_SETUP_ITWT_REQ:
		if (no_ack) {
			WL_ERROR(("%s: Setup failed, no ACK (txstatus=0x%x)\n",
				__FUNCTION__, txstatus));
			indv = wlc_twt_find_fid(scb_twt, scb_twt->af_fid);
			if (!indv) {
				WL_ERROR(("%s No indv: Unexpected Flow ID stored=%d\n",
					__FUNCTION__, scb_twt->af_fid));
			} else {
				wlc_twt_teardown_itwt(twti, scb, indv, FALSE);
			}
		} else {
			scb_twt->af_state = af_state == WLC_TWT_AF_PEND_SETUP_ITWT_REQ ?
				WLC_TWT_AF_AWAIT_SETUP_ITWT_RSP : WLC_TWT_AF_AWAIT_SETUP_BTWT_RSP;
			scb_twt->af_timeout = WLC_TWT_AF_TIMEOUT;
		}
		break;
	case WLC_TWT_AF_PEND_SETUP_BTWT_RSP: /* bTWT uses the iTWT to do the atual SP */
	case WLC_TWT_AF_PEND_SETUP_ITWT_RSP:
		indv = wlc_twt_find_fid(scb_twt, scb_twt->af_fid);
		if (!indv) {
			WL_ERROR(("%s No indv: Unexpected Flow ID stored=%d\n",
				__FUNCTION__, scb_twt->af_fid));
		} else {
			if (no_ack) {
				if ((af_state == WLC_TWT_AF_PEND_SETUP_ITWT_RSP) &&
					(scb_twt->af_retry_count)) {
					scb_twt->af_retry_count--;
					wlc_twt_indv_retry_setup_resp(twti, scb, indv);
				} else {
					WL_ERROR(("%s: Accept failed, no ACK (txstatus=0x%x)\n",
						__FUNCTION__, txstatus));
					wlc_twt_teardown_itwt(twti, scb, indv, FALSE);
				}
			} else {
				/* iTWT setup complete */
				wlc_twt_setup_itwt_complete(twti, scb, indv);
			}
		}
		break;
	default:
		WL_ERROR(("%s: AF txcomplete, for unknown action %d (txstatus=0x%x)\n",
			__FUNCTION__, af_state, txstatus));
		break;
	}
}

/**
 * Teardown the TWT schedule for the link in the scb described by af_teardown. The af_teardown
 * is to be in the format as used in the Teardown Action Frame, telling bcast/indv and id.
 */
static int
wlc_twt_teardown(wlc_twt_info_t *twti, scb_t *scb, uint8 af_teardown, bool enter_pm)
{
	twt_scb_t *scb_twt;
	twt_indv_desc_t *indv;
	int err = BCME_UNSUPPORTED;
	uint8 negotiation;
	uint8 id;
	uint8 i;

	negotiation = (af_teardown & TWT_TEARDOWN_NEGO_TYPE_MASK) >> TWT_TEARDOWN_NEGO_TYPE_SHIFT;

	if (negotiation == TWT_CTRL_NEGO_INDV_REQ_RSP) {
		/* Find the scb cubby, bail out if NULL */
		if ((scb == NULL) || ((scb_twt = TWT_SCB(twti, scb)) == NULL)) {
			return BCME_UNSUPPORTED;
		}
		if (af_teardown & TWT_TEARDOWN_ALL_TWT) {
			for (i = 0; i < ARRAYSIZE(scb_twt->indv); i++) {
				if (scb_twt->indv[i].state == WLC_TWT_STATE_ACTIVE) {
					err = wlc_twt_teardown_itwt(twti, scb, &scb_twt->indv[i],
						enter_pm);
					if (err) {
						WL_ERROR(("wl%d: %s Error %d, continue\n",
							twti->wlc->pub->unit, __FUNCTION__, err));
					}
				}
			}
		} else {
			id = (af_teardown & TWT_TEARDOWN_ID_INDV_MASK) >> TWT_TEARDOWN_ID_SHIFT;
			indv = wlc_twt_find_fid(scb_twt, id);
			if (!indv) {
				WL_ERROR(("wl%d: %s No indv: Unexpected Flow ID %d\n",
					twti->wlc->pub->unit, __FUNCTION__, id));
				err = BCME_BADARG;
			} else {
				err = wlc_twt_teardown_itwt(twti, scb, indv, enter_pm);
			}
		}
	}
	if (negotiation == TWT_CTRL_NEGO_BCAST_MGMT) {
		id = (af_teardown & TWT_TEARDOWN_ID_BCAST_MASK) >> TWT_TEARDOWN_ID_SHIFT;
		err = wlc_twt_teardown_btwt(twti, scb, id, enter_pm);
	}

	return err;
}

/**
 * Teardown the TWT schedule failed, check if retry is needed. The af_teardown
 * is to be in the format as used in the Teardown Action Frame, telling bcast/indv and id.
 */
static int
wlc_twt_teardown_retry(wlc_twt_info_t *twti, scb_t *scb, uint8 af_teardown)
{
	twt_scb_t *scb_twt;
	twt_indv_desc_t *indv;
	int err = BCME_UNSUPPORTED;
	uint8 negotiation;
	uint8 id;

	negotiation = (af_teardown & TWT_TEARDOWN_NEGO_TYPE_MASK) >> TWT_TEARDOWN_NEGO_TYPE_SHIFT;

	if (negotiation == TWT_CTRL_NEGO_INDV_REQ_RSP) {
		id = (af_teardown & TWT_TEARDOWN_ID_INDV_MASK) >> TWT_TEARDOWN_ID_SHIFT;
		/* Find the scb cubby, bail out if NULL */
		if ((scb == NULL) || ((scb_twt = TWT_SCB(twti, scb)) == NULL)) {
			return BCME_UNSUPPORTED;
		}
		indv = wlc_twt_find_fid(scb_twt, id);
		if (!indv) {
			WL_ERROR(("%s No indv: Unexpected Flow ID in teardown request=%d\n",
				__FUNCTION__, id));
			err = BCME_BADARG;
		} else if (indv->teardown_retry_cnt) {
			indv->teardown_retry_cnt--;
			WL_TWT(("%s Retrying teardown, count=%d\n", __FUNCTION__,
				indv->teardown_retry_cnt));
			err = wlc_twt_teardown_send(twti, SCB_BSSCFG(scb), scb, FALSE, id);
		}
	}
	if (negotiation == TWT_CTRL_NEGO_BCAST_MGMT) {
		WL_TWT(("%s No indv: Unexpected Request type in teardown request=%d\n",
			__FUNCTION__, negotiation));
		err = BCME_BADARG;
	}

	return err;
}

/**
 * Send a teardown Action Frame. Validate the request for teardown, then generate an Action Frame.
 * Once the action action frame has been transmittd, remove the TWT schedule.
 */
static int
wlc_twt_teardown_send(wlc_twt_info_t *twti, wlc_bsscfg_t *cfg, scb_t *scb, bool bcast,
	uint8 id)
{
	wlc_info_t *wlc = twti->wlc;
	twt_scb_t *scb_twt;
	int err = BCME_OK;
	void *pkt = NULL;
	uint body_len;
	uint8 *body;

	/* Find the scb cubby, bail out if NULL */
	if ((scb == NULL) || ((scb_twt = TWT_SCB(twti, scb)) == NULL)) {
		err = BCME_UNSUPPORTED;
		goto fail;
	}

	/* First check if there is a TWT schedule active which can be removed - TBD */
	WL_TWT(("%s bcast=%d, id=%d\n", __FUNCTION__, bcast, id));

	/* See if there is no AF pending. if so then we cannot continue. Parallel
	 * requests are not supported
	 */
	if (scb_twt->af_state != WLC_TWT_AF_STATE_NONE) {
		err = BCME_BUSY;
		goto fail;
	}

	/* Allocate the packet */
	body_len = S1G_AF_TWT_TEARDOWN_FLOW_OFF + sizeof(uint8);
	if ((pkt = wlc_frame_get_action(wlc, &scb->ea, &cfg->cur_etheraddr,
		&cfg->BSSID, body_len, &body, DOT11_ACTION_CAT_S1G)) == NULL) {
		err = BCME_NOMEM;
		goto fail;
	}

	/* fill up the packet */
	body[S1G_AF_CAT_OFF] = DOT11_ACTION_CAT_S1G;
	body[S1G_AF_ACT_OFF] = S1G_ACTION_TWT_TEARDOWN;
	if (bcast) {
		body[S1G_AF_TWT_TEARDOWN_FLOW_OFF] =
			(TWT_CTRL_NEGO_BCAST_MGMT << TWT_TEARDOWN_NEGO_TYPE_SHIFT) |
			(id << TWT_TEARDOWN_ID_SHIFT);
	} else {
		body[S1G_AF_TWT_TEARDOWN_FLOW_OFF] =
			(TWT_CTRL_NEGO_INDV_REQ_RSP << TWT_TEARDOWN_NEGO_TYPE_SHIFT) |
			(id << TWT_TEARDOWN_ID_SHIFT);
	}

	/* Enable packet callback => wlc_twt_af_txcomplete */
	WLF2_PCB1_REG(pkt, WLF2_PCB1_TWT_AF);
	scb_twt->af_state = WLC_TWT_AF_PEND_TEARDOWN;
	scb_twt->af_teardown = body[S1G_AF_TWT_TEARDOWN_FLOW_OFF];

	/* commit for transmission */
	if (!wlc_sendmgmt(wlc, pkt, wlc->active_queue, scb)) {
		err = BCME_BUSY;
		/* wlc_sendmgmt() frees the pkt when returns false */
		pkt = NULL;
		goto fail;
	}

fail:
	if (err != BCME_OK) {
		if (pkt != NULL) {
			PKTFREE(wlc->osh, pkt, TRUE);
		}
		WL_ERROR(("wl%d: %s: error=%d\n", wlc->pub->unit, __FUNCTION__, err));
	}
	return err;
}

/**
 * Process twt information. All parameters should have been checked and be valid. This means fid
 * is valid, twt is in future (or TSF). Only part which will be checked here is the state
 * validation. See if link is not already in suspend and vice versa. If flowid is
 * TWT_INFO_ALL_FLOW_ID then all links will be put in suspend (or resumed). Current version
 * does not suppport broadcast TWT.
 */
static int
wlc_twt_information(wlc_twt_info_t *twti, scb_t *scb, uint8 flow_id, bool resume, uint16 fc,
	uint32 twt_l, uint32 twt_h)
{
	twt_scb_t *scb_twt;
	twt_indv_desc_t *indv;
	int err = BCME_OK;
	uint8 start, stop;
	uint8 i;

	/* Find the scb cubby, bail out if NULL */
	if ((scb == NULL) || ((scb_twt = TWT_SCB(twti, scb)) == NULL)) {
		goto fail;
	}

	WL_TWT(("%s Enter, AID %d flow_id=0x%02x, resume=%d, twt=%08x:%08x\n", __FUNCTION__,
		SCB_AID(scb), flow_id, resume, resume ? twt_h : 0, resume ? twt_l : 0));

	if (flow_id == TWT_INFO_ALL_FLOW_ID) {
		start = 0;
		stop = ARRAYSIZE(scb_twt->indv);
	} else {
		start = flow_id;
		stop = flow_id + 1;
	}
	for (i = start; i < stop; i++) {
		indv = &scb_twt->indv[i];
		if (indv->state == WLC_TWT_STATE_ACTIVE) {
			if (resume) {
				if (!indv->paused) {
					WL_TWT(("%s Flow ID %d implicit pause\n", __FUNCTION__, i));
					wlc_twt_sp_pause(twti, scb, indv, fc);
				}
				indv->resuming = TRUE;
				indv->resume_twt_l = twt_l;
				indv->resume_twt_h = twt_h;
			} else {
				indv->resuming = FALSE;
				if (indv->paused) {
					if (flow_id != TWT_INFO_ALL_FLOW_ID) {
						WL_TWT(("%s Flow ID %d already paused\n",
							__FUNCTION__, i));
						err = BCME_BADARG;
						goto fail;
					}
				} else {
					wlc_twt_sp_pause(twti, scb, indv, fc);
				}
			}
		}
	}

fail:
	return err;
}

/**
 * Take first available flow id if any available.
 */
static int
wlc_twti_fid_alloc(wlc_twt_info_t *twti, scb_t *scb, uint8 *flow_id, twt_indv_desc_t **indv)
{
	twt_scb_t *scb_twt = TWT_SCB(twti, scb);
	uint8 fid_bmp;
	uint8 i;
	uint8 count;

	fid_bmp = 0;
	count = 0;
	*indv = NULL;
	for (i = 0; i < ARRAYSIZE(scb_twt->indv); i++) {
		if (scb_twt->indv[i].state != WLC_TWT_STATE_INACTIVE) {
			count++;
			fid_bmp |= (1 << scb_twt->indv[i].desc.id);
		} else if (*indv == NULL) {
			*indv = &scb_twt->indv[i];
		}
	}
	if (count == ARRAYSIZE(scb_twt->indv)) {
		return BCME_NORESOURCE;
	}
	for (i = 0; i < ARRAYSIZE(scb_twt->indv); i++) {
		if ((fid_bmp & (1 << i)) == 0) {
			*flow_id = i;
			break;
		}
	}

	return BCME_OK;
}

/**
 * Send a setup Action Frame. Request comes from host to start a ITWT. Validate the request for
 * setup, then generate an Action Frame. Once the action action frame has been transmitted, wait for
 * possible response. Sending of AF needs timeout (within AF itself, can come after response!).
 * Response waiting needs also timeout. Blocking is required till completion. So keep af_state.
 */
static int
wlc_twt_sta_itwt_req_send(wlc_twt_info_t *twti, scb_t *scb, wl_twt_sdesc_t *setup)
{
	wlc_info_t *wlc = twti->wlc;
	wlc_bsscfg_t *cfg;
	twt_scb_t *scb_twt;
	int err = BCME_OK;
	void *pkt = NULL;
	uint8 *body;
	twt_ie_indv_t *twt_ie;
	twt_indv_desc_t *indv = NULL;
	uint8 setup_command;

	/* Find the scb cubby, bail out if NULL */
	if ((scb == NULL) || ((scb_twt = TWT_SCB(twti, scb)) == NULL)) {
		err = BCME_UNSUPPORTED;
		goto fail;
	}
	cfg = SCB_BSSCFG(scb);

	/* See if there is no AF pending. if so then we cannot continue. Parallel
	 * requests are not supported
	 */
	if (scb_twt->af_state != WLC_TWT_AF_STATE_NONE) {
		err = BCME_BUSY;
		goto fail;
	}

	/* First see if we have a flow id available to setup this request */
	err = wlc_twti_fid_alloc(twti, scb, &setup->id, &indv);
	if (err) {
		goto fail;
	}
	indv->state = WLC_TWT_STATE_RESERVED;

	/* Validate the configuration - TBD */

	/* Start building the AF */
	scb_twt->af_state = twti->dialog_token++;
	pkt = wlc_twt_build_af(scb, scb_twt->af_dialog_token, &body, TRUE);
	if (!pkt) {
		err = BCME_NOMEM;
		goto fail;
	}
	/* Fill up AF indvidual part of IE */
	twt_ie = (twt_ie_indv_t *)body;

	memcpy(&indv->desc, setup, sizeof(indv->desc));

	if (setup->setup_command == WL_TWT_SETUP_CMD_DEMAND) {
		setup_command = TWT_SETUP_CMD_DEMAND_TWT;
	} else if (setup->setup_command == WL_TWT_SETUP_CMD_SUGGEST) {
		setup_command = TWT_SETUP_CMD_SUGGEST_TWT;
	} else {
		setup_command = TWT_SETUP_CMD_REQUEST_TWT;
	}

	/* Type can be Request/Suggest or Demand, reguest: TWT time is value 0 */
	if (setup_command == TWT_SETUP_CMD_REQUEST_TWT) {
		indv->twt_l = 0;
		indv->twt_h = 0;
	} else {
		wlc_twt_get_tsf(wlc, cfg, &indv->twt_l, &indv->twt_h);
		/* Add some fixed value which is far enough in the future to make sure
		 * the negotiation can complete.
		 */
		wlc_uint64_add(&indv->twt_h, &indv->twt_l, 0, WLC_TWT_FIRST_SP_AHEAD_PM);
	}

	wlc_twt_itwt_build_setup_ie(twt_ie, indv, setup_command);
	WLF2_PCB1_REG(pkt, WLF2_PCB1_TWT_AF);

	if (WL_TWT_ON()) {
		prhex("iTWT Request", body - S1G_AF_TWT_SETUP_TWT_IE_OFF - WLC_TWT_TOP_IE_SZ,
			S1G_AF_TWT_SETUP_TWT_IE_OFF + WLC_TWT_TOP_IE_SZ + WLC_TWT_INDV_IE_SZ);
	}
	/* commit for transmission */
	if (!wlc_sendmgmt(wlc, pkt, wlc->active_queue, scb)) {
		err = BCME_BUSY;
		/* wlc_sendmgmt() also owns/frees the pkt when returns false, no need to free it */
		goto fail;
	}

	/* Configure wait for response af_state */
	scb_twt->af_state = WLC_TWT_AF_PEND_SETUP_ITWT_REQ;
	scb_twt->af_fid = setup->id;

fail:
	if (err != BCME_OK) {
		if (indv != NULL) {
			indv->state = WLC_TWT_STATE_INACTIVE;
		}
		WL_ERROR(("wl%d: %s: error=%d\n", wlc->pub->unit, __FUNCTION__, err));
	}
	return err;
}

/**
 * Send a setup Action Frame. Request comes from host to join a BTWT. Validate the request for
 * setup, then generate an Action Frame. Once the action action frame has been transmitted, wait for
 * possible response. Sending of AF needs timeout (within AF itself, can come after response!).
 * Response waiting needs also timeout. Blocking is required till completion. So keep af_state.
 */
static int
wlc_twt_sta_btwt_req_send(wlc_twt_info_t *twti, scb_t *scb, wl_twt_sdesc_t *setup)
{
	wlc_info_t *wlc = twti->wlc;
	wlc_bsscfg_t *cfg;
	twt_scb_t *scb_twt;
	twt_bss_t *bss_twt;
	int err = BCME_OK;
	uint8 bid;
	twt_indv_desc_t *indv = NULL;
	twt_ie_bcast_t *bcast;
	uint8 i;
	void *pkt;
	uint8 *body;

	/* Find the scb cubby, bail out if NULL */
	if ((scb == NULL) || ((scb_twt = TWT_SCB(twti, scb)) == NULL)) {
		err = BCME_UNSUPPORTED;
		goto fail;
	}

	cfg = SCB_BSSCFG(scb);

	/* See if there is no AF pending. if so then we cannot continue. Parallel
	 * requests are not supported
	 */
	if (scb_twt->af_state != WLC_TWT_AF_STATE_NONE) {
		err = BCME_BUSY;
		goto fail;
	}

	cfg = SCB_BSSCFG(scb);
	bss_twt = TWT_BSS(twti, cfg);
	if (bss_twt == NULL) {
		err = BCME_UNSUPPORTED;
		goto fail;
	}
	bid = setup->id;
	if (bss_twt->bcast[bid].state == WLC_TWT_STATE_INACTIVE) {
		err = BCME_BADARG;
		goto fail;
	}

	/* See if there is a iTWT schedule available, as that is what is being used to do the
	 * scheduling.
	 */
	for (i = 0; i < ARRAYSIZE(scb_twt->indv); i++) {
		if (scb_twt->indv[i].state != WLC_TWT_STATE_INACTIVE) {
			if (scb_twt->indv[i].desc.id == WLC_TWT_BID2FID(bid)) {
				/* Already in use */
				err = BCME_BADARG;
				goto fail;
			}
		} else {
			if (indv == NULL) {
				indv = &scb_twt->indv[i];
			}
		}
	}
	if (indv == NULL) {
		/* No more iTWT descriptors available for this sta */
		err = BCME_NORESOURCE;
		goto fail;
	}
	memset(indv, 0, sizeof(*indv));

	/* Allocate AF and fill header */
	scb_twt->af_state = twti->dialog_token++;
	pkt = wlc_twt_build_af(scb, scb_twt->af_dialog_token, &body, FALSE);
	if (!pkt) {
		err = BCME_NOMEM;
		goto fail;
	}

	/* Use TWT 0, to set real valid value the TWT would have to be tracked continously, lets
	 * hope that no AP requires a real value. As this would make common sense
	 */
	wlc_twt_ie_add_single_bcast_ie(body, &bss_twt->bcast[bid], TWT_SETUP_CMD_REQUEST_TWT, 0);

	bcast = (twt_ie_bcast_t *)body;
	setbit(&bcast->request_type, TWT_REQ_TYPE_IMPL_LAST_IDX);
	WLF2_PCB1_REG(pkt, WLF2_PCB1_TWT_AF);

	if (WL_TWT_ON()) {
		prhex("bTWT Request",
			body - S1G_AF_TWT_SETUP_TWT_IE_OFF - WLC_TWT_TOP_IE_SZ,
			S1G_AF_TWT_SETUP_TWT_IE_OFF + WLC_TWT_TOP_IE_SZ + WLC_TWT_BCAST_IE_SZ);
	}

	/* commit for transmission */
	if (!wlc_sendmgmt(wlc, pkt, wlc->active_queue, scb)) {
		err = BCME_BUSY;
		/* wlc_sendmgmt() also owns/frees the pkt when returns false, no need to free it */
		goto fail;
	}

	/* Copy the broadcast schedule information to the individual configuration */
	indv->desc = bss_twt->bcast[bid].desc;
	indv->desc.id = WLC_TWT_BID2FID(bid);
	indv->state = WLC_TWT_STATE_RESERVED;

	/* Configure wait for response af_state */
	scb_twt->af_state = WLC_TWT_AF_PEND_SETUP_BTWT_REQ;
	scb_twt->af_fid = indv->desc.id;

fail:
	if (err != BCME_OK) {
		WL_ERROR(("wl%d: %s: error=%d\n", wlc->pub->unit, __FUNCTION__, err));
	}
	return err;
}

/**
 * Timeout counter handler for AF statemachine. To be called for each scb per TBTT. If a state
 * is pending and protected by timeout then it is being dealt with from this function. To be used
 * to wait for example on AF response frames.
 */
static void
wlc_twt_scb_timeout_cnt(wlc_info_t *wlc, scb_t *scb)
{
	twt_scb_t *scb_twt;
	twt_indv_desc_t *indv;

	/* Find the scb cubby, bail out if NULL */
	if ((scb_twt = TWT_SCB(wlc->twti, scb)) == NULL) {
		return;
	}

	if (scb_twt->af_timeout) {
		scb_twt->af_timeout--;
		if (scb_twt->af_timeout == 0) {
			/* Expired. Handle the different states for which we got here */
			WL_ERROR(("%s: AF State change timeout. ea=" MACF " state=%d\n",
				__FUNCTION__, ETHERP_TO_MACF(&scb->ea), scb_twt->af_state));
			switch (scb_twt->af_state) {
			case WLC_TWT_AF_AWAIT_SETUP_BTWT_RSP: /* fall through */
			case WLC_TWT_AF_AWAIT_SETUP_ITWT_RSP:
				scb_twt->af_state = WLC_TWT_AF_STATE_NONE;
				indv = wlc_twt_find_fid(scb_twt, scb_twt->af_fid);
				if (!indv) {
					WL_ERROR(("%s No indv: Unexpected Flow ID stored=%d\n",
						__FUNCTION__, scb_twt->af_fid));
				} else {
					wlc_twt_teardown_itwt(wlc->twti, scb, indv, FALSE);
				}
				break;
			default:
				WL_ERROR(("Unahandled state=%d !\n", scb_twt->af_state));
				ASSERT(0);
				scb_twt->af_state = WLC_TWT_AF_STATE_NONE;
				break;
			}
		}
	}
}

/* TWT scheduler (RLM block programming) in short TWTS defines: */

/* WLC_TWT_FIRST_SP_MIN_TIME defines the miniumum time ahead in time for next
 * SP to start the scheduling. If the time till the next SP is lower then this then wait for
 * next SP to start the scheduling for this SP. This is used during start of SP. It is set to 3
 * (schedid) because the scheduler programs ahead up to two blocks and we want to avoid that
 * initial start of SP falls in block which is already programmed
 */
#ifdef WLC_TWT_USE_4MS_SCHEDID
#define WLC_TWT_FIRST_SP_MIN_TIME	WLC_SCHEDID_TO_TSFL(6)
#else
#define WLC_TWT_FIRST_SP_MIN_TIME	WLC_SCHEDID_TO_TSFL(3)
#endif /* WLC_TWT_USE_4MS_SCHEDID */

/* WLC_TWTS_MAX_PRESCHEDULES is used to counter interrupt latency when programming a TWT
 * RLM schedule. The number of preschedules should be lower then the max number available/
 * reserved which is currently 4. Concept is to get timer interrupt just when new schedid is
 * active and program x further. x defines the max latency the timer interrupt can handle. If the
 * schedid period decreases (initial value choosen was 8 msec) then then this define can be
 * increased to counter that. So if for example WLC_TWTS_MAX_PRESCHEDULES is set to 2 then
 * at time x it is supposed to program for x + 2, but if x is delayed due to interrupt latency
 * then scheduler can deal with 1 delay in schedid. If more delay then SP will be jumped and not
 * programmed.
 */
#ifdef WLC_TWT_USE_4MS_SCHEDID
#define WLC_TWTS_MAX_PRESCHEDULES	3
#else
#define WLC_TWTS_MAX_PRESCHEDULES	2
#endif /* WLC_TWT_USE_4MS_SCHEDID */

/* WLC_TWTS_TIMER_MARGIN defines the time the timer should expire ahead of first SP for next
 * schedule. Example if SP starts at time X then timer should should expire
 * X - WLC_TWTS_TIMER_MARGIN so the RLM schedule can be prepared. The sheduler programs two
 * blocks. Keep that as the timer margin. But if schedid gets smaller (then 8 ms) then this
 * method should be reconsidered. There is an unpredictable latency to expect !
 */
#define WLC_TWTS_TIMER_MARGIN		WLC_SCHEDID_TO_TSFL(WLC_TWTS_MAX_PRESCHEDULES + 1)

/* There is an error in HRT. It is roughly timeout / 200, correct calculated timeout */
#define WLC_TWT_HRT_ERROR_CORRECTION	200

/* Approach of this scheduler: it can be called anytime, but the purpose is that it gets called
 * just before a schedule should be programmed in RLM, though it may also be so that this is not
 * needed. This scheduler will also find the timer to be programmed for the next schedule.
 * Concept: The scheduler when called will take timestamp S. If an SP is to start (be scheduled)
 * within the next 2 schedid (which is currently defined as 8 ms) then the SP will be be put into
 * corresponding rlm prep block. So basically the scheuler can build up to two blocks to be passed
 * on to RLM at the same time. This is done to allow for smaller schedid in the future, but still
 * be safe to deal with
 * Note 1: initial version, we dont always need a timer for next schedule, we can lift along on the
 * pretwt timer. But that is an optimization to avoid unnecessary timer interrupts.
 * Note 2: Initial version uses simple linked list of active twt_scbs. Per SCB we coult have
 * multiple schedules active ! The list is not sorted, just a quick lookup list so not all SCBs
 * have to be checked
 */
static void
wlc_twt_itwt_rlm_scheduler_update_trigger(void *arg)
{
	wlc_twt_info_t *twti = (wlc_twt_info_t *)arg;
	wlc_info_t *wlc = twti->wlc;
	twt_scb_t *scb_twt;
	twt_indv_desc_t *indv;
	uint32 tsf_h, tsf_l;
	uint32 delta_h, delta_l;
	wlc_twt_rlm_t *twt_rlm;
	wlc_twt_rlm_t twt_rlm_arr[WLC_TWTS_MAX_PRESCHEDULES];
	uint8 i, idx;
	bool programmed;
	uint32 new_timeout;

	wlc_twt_get_tsf(wlc, wlc->primary_bsscfg, &tsf_l, &tsf_h);

	scb_twt = twti->scb_twt_list;
	ASSERT(scb_twt);

	memset(&twt_rlm_arr[0], 0, sizeof(twt_rlm_arr));
	/* Initial (max) timeout. Dont make timer too long since there might be an error */
	new_timeout = (1 << 20); /* 1 second */
	while (scb_twt) {
		for (i = 0; i < ARRAYSIZE(scb_twt->indv); i++) {
			if (scb_twt->indv[i].state != WLC_TWT_STATE_ACTIVE) {
				continue;
			}
			indv = &scb_twt->indv[i];
			delta_h = indv->twt_h;
			delta_l = indv->twt_l;
			wlc_uint64_sub(&delta_h, &delta_l, tsf_h, tsf_l);
			if (delta_h != 0) {
				if (delta_h != WLC_TWT_TSF_MAX) {
					/* This is not to expect */
					WL_ERROR(("%s TSF (%08x:%08x) too far in future\n",
						__FUNCTION__, delta_h, delta_l));
					continue;
				}
				indv->cnt_sp_missed++;
				/* Keep adding interval till we delta_h is 0 */
				while (delta_h) {
					wlc_uint64_add(&delta_h, &delta_l, 0,
						indv->wake_interval);
					/* for assertion check in the next if body */
					wlc_uint64_add(&indv->twt_h, &indv->twt_l, 0,
						indv->wake_interval);
				}
			}
			/* if schedid of delta_l is 0 then we are too late!! It would mean
			 * that it should already have been programmed. So skip the SP
			 * for that destination. It should not occur...
			 */
			if (WLC_TSFL_TO_SCHEDID(delta_l - 1) == 0) {
				indv->cnt_sp_missed_by1++;
				wlc_uint64_add(&delta_h, &delta_l, 0, indv->wake_interval);
				/* for assertion check in the next if body */
				wlc_uint64_add(&indv->twt_h, &indv->twt_l, 0, indv->wake_interval);
			}
			if (WLC_TSFL_TO_SCHEDID(delta_l - 1) <= WLC_TWTS_MAX_PRESCHEDULES) {
				if (indv->paused) {
					if (indv->resuming) {
						wlc_twt_sp_resume(twti, indv);
					}
					if (indv->paused) {
						goto skip_rlm_programming;
					}
				}
				indv->cnt_sp++;
				/* put it into RLM schedule */
				idx = WLC_TSFL_TO_SCHEDID(delta_l - 1) - 1;
				twt_rlm = &twt_rlm_arr[idx];
				twt_rlm->schedid = WLC_TSFL_TO_SCHEDID(tsf_l) + idx + 2;
				if (twt_rlm->schedid != WLC_TSFL_TO_SCHEDID(indv->twt_l)) {
					WL_TWT(("%s: idx tsf 0x%x schedid 0x%x != 0x%x\n",
						__FUNCTION__, tsf_l, twt_rlm->schedid,
						WLC_TSFL_TO_SCHEDID(indv->twt_l)));
					ASSERT(0);
				}
				if (!scb_twt->apps_prepared) {
					scb_twt->cur_sp_start = twt_rlm->schedid;
					wlc_twt_prepare_for_first_sp(twti, indv);
				}
				twt_rlm->usr_info[twt_rlm->usr_count] =
					(scb_twt->rlm_id << TSB_USR_IDX_SHIFT) |
					((indv->wake_duration - 1) << TSB_USR_SPDUR_SHIFT) |
					(indv->trigger << TSB_USR_TRIG_SHIFT);
				if (!(indv->desc.flow_flags &
					WL_TWT_FLOW_FLAG_UNANNOUNCED)) {
					/* announced! */
					twt_rlm->usr_info[twt_rlm->usr_count] |=
						(1 << TSB_USR_ANCD_SHIFT);
				}
				twt_rlm->usr_count++;

skip_rlm_programming:
				/* Make sure SP gets updated to next interval */
				wlc_uint64_add(&delta_h, &delta_l, 0, indv->wake_interval);
			}

			/* Suitable for next timer ? */
			if (delta_l <= WLC_TWTS_TIMER_MARGIN) {
				/* Too short, use immediate callback */
				new_timeout = 1;

			} else if ((delta_l - WLC_TWTS_TIMER_MARGIN) < new_timeout) {
				new_timeout = delta_l - WLC_TWTS_TIMER_MARGIN;
			}
			/* Update the indiv twt SP start time */
			wlc_uint64_add(&delta_h, &delta_l, tsf_h, tsf_l);
			ASSERT((delta_l & WLC_TSFL_SCHEDID_MASK_INV) == 0);
			indv->twt_h = delta_h;
			indv->twt_l = delta_l;
		}
		scb_twt = scb_twt->next;
	}

	programmed = FALSE;
	for (i = 0; i < ARRAYSIZE(twt_rlm_arr); i++) {
		if (twt_rlm_arr[i].usr_count) {
			uint16 usrnum;
			if (i == 0) {
				twti->cnt_sp_missed_almost++;
			}
			usrnum = (wlc_ratelinkmem_lmem_read_word(wlc,
				(AMT_IDX_TWT_RSVD_START + twti->rlm_prog_idx), 0) >> 16);
			if (usrnum != 0) {
				/* There is a possibility that the current error is unrelated to
				 * TWT, but a result of generic halt of ucode. Asserting would lead
				 * to wrong assumption that it is TWT related. Check PSMWDS_PC to
				 * determine this.
				 */
				if (wlc->clk) {
					SPINWAIT(((wlc_read_shm(wlc, M_PSMWDS_PC(wlc)) == 0) &&
						(wlc_read_shmx(wlc, MX_PSMWDS_PC(wlc)) == 0)),
						WLC_TWT_MAX_WAIT4PSMWD);
					if (wlc_read_shm(wlc, M_PSMWDS_PC(wlc)) ||
						wlc_read_shmx(wlc, MX_PSMWDS_PC(wlc))) {
						/* It is generic PSM WD. Do a hard return, skipping
						 * the programming and stopping the timer.
						 */
						WL_ERROR(("%s: ABORTING, PSM WD about to trigger\n",
							__FUNCTION__));
						return;
					}
				}

				/* We are very close to SP start with no twtschedblk available */
				WL_ERROR(("%s: cur tsf 0x%x SP strt 0x%x\n", __FUNCTION__,
					tsf_l, twt_rlm_arr[i].schedid));
				ASSERT(0);
			}
			wlc_ratelinkmem_update_link_twtschedblk(wlc, twti->rlm_prog_idx,
				(uint8 *)&twt_rlm_arr[i], sizeof(twt_rlm_arr[i]));
			memcpy(&twti->twtschedblk[twti->rlm_prog_idx],
				&twt_rlm_arr[i], sizeof(twt_rlm_arr[i]));

			WL_TRACE(("%s: idx [%d %d] tsf 0x%x schedid 0x%x usrcnt %d\n", __FUNCTION__,
				twti->rlm_prog_idx, twti->rlm_read_idx, tsf_l,
				twt_rlm_arr[i].schedid, twt_rlm_arr[i].usr_count));

			twti->rlm_prog_idx++;
			if (twti->rlm_prog_idx == AMT_IDX_TWT_RSVD_SIZE) {
				twti->rlm_prog_idx = 0;
			}
			programmed = TRUE;
		}
	}
	new_timeout -= (new_timeout / WLC_TWT_HRT_ERROR_CORRECTION);
	wlc_hrt_add_timeout(twti->rlm_scheduler_timer,
		new_timeout, wlc_twt_itwt_rlm_scheduler_update_trigger, twti);

	if ((!twti->rlm_scheduler_active) && (programmed)) {
		twti->rlm_scheduler_active = TRUE;
		twti->cnt_engine_started++;
		wlc_write_shm(wlc, M_TWTCMD(wlc), 1);
		W_REG(wlc->osh, D11_MACCOMMAND(wlc->hw),
			(R_REG(wlc->osh, D11_MACCOMMAND(wlc->hw)) | MCMD_TWT));
		WL_TWT(("TWT Ucode engine started...\n"));
	}

#ifdef DEBUG_TWT_PS
	if (!twti->rlm_scheduler_active) {
		if (!twti->test_timer) {
			twti->test_timer = wlc_hrt_alloc_timeout(wlc->hrti);
		}
		if (new_timeout > 7000) {
			wlc_hrt_add_timeout(twti->test_timer,
				new_timeout - 2000, wlc_twt_test_trigger, twti);
		}
	}
#endif /* DEBUG_TWT_PS */
}

#ifdef DEBUG_TWT_PS
static void
wlc_twt_test_trigger(void *arg)
{
	wlc_twt_info_t *twti = (wlc_twt_info_t *)arg;
	wlc_info_t *wlc = twti->wlc;
	twt_scb_t *scb_twt;
	scb_t *scb;

	scb_twt = twti->scb_twt_list;

	scb = scb_twt->indv[0].scb;

	WL_TWT(("%s Enter  !!!!! forcing exit PS !!!\n", __FUNCTION__));

	wlc_apps_ps_requester(wlc, scb, 0, PS_SWITCH_PMQ_ENTRY);
	wlc_apps_process_ps_switch(wlc, scb, 0);

}
#endif /* DEBUG_TWT_PS */

static void
wlc_twt_itwt_end_sp(void *arg)
{
	twt_indv_desc_t *indv = (twt_indv_desc_t *)arg;
	wlc_info_t *wlc = indv->scb->bsscfg->wlc;
	scb_t *scb = indv->scb;

	if (indv->state != WLC_TWT_STATE_ACTIVE) {
		return;
	}
	wlc_apps_twt_sp_enter_ps(wlc, indv->scb);
	indv->tx_closed = TRUE;

	/* if there is nothing left on LO/HW queue then release fifos now, otherwise have
	 * APPS do this once suppress is complete
	 */
	if (wlc_scb_tot_inflt_fifocnt(wlc, scb) == 0) {
		wlc_twt_ps_suppress_done(wlc->twti, scb);
	}
}

void
wlc_twt_ps_suppress_done(wlc_twt_info_t *twti, scb_t *scb)
{
	wlc_fifo_free_all(twti->wlc->fifo, scb);
}

/*
 * this can be a syncrhonous and asynchronous call as a result from the call to wlc_apps_twt_enter.
 * APPS had put the datapath in a "blocking" mode where TWT SP can start and open the SP when
 * ready. This function will make sure the necessary datapath modules will get updated such that
 * data will will go out using TWT.
 */
void
wlc_twt_apps_ready_for_twt(wlc_twt_info_t *twti, scb_t *scb)
{
	wlc_info_t *wlc = twti->wlc;
	twt_scb_t *scb_twt;

	WL_TWT(("%s Enter AID %d\n", __FUNCTION__, SCB_AID(scb)));

	scb_twt = TWT_SCB(twti, scb);
	scb_twt->sp_started = TRUE;

	/* Release the assigned fifos for this SCB, so dedicated TWT Fifos can be used */
	wlc_fifo_free_all(twti->wlc->fifo, scb);

	/* The first iTWT for this scb should trigger linkmem update, so PM gets ignored */
	ASSERT(RATELINKMEM_ENAB(wlc->pub));
	wlc_ratelinkmem_update_link_entry(wlc, scb);

#ifdef WLCFP
	/* Trigger CFP path update check. TWT would have blocked it */
	wlc_cfp_scb_state_upd(wlc->cfp, scb);
#endif /* WLCFP */

	/* Trigger MUTX to re-evaluate admission */
	wlc_mutx_evict_or_admit_muclient(wlc->mutx, scb);

	/* Make sure that triggering starts if the TWT link is trigger based. */
	wlc_ulmu_admit_ready(wlc, scb);
}

/*
 * this function is to be called once the first SP for a certain SCB is about to start its first
 * SP, when this is called the datapath gets drained, and upon drain complete the function
 * wlc_twt_apps_ready_for_twt
 */
static void
wlc_twt_prepare_for_first_sp(wlc_twt_info_t *twti, twt_indv_desc_t *indv)
{
	twt_scb_t *scb_twt;

	WL_TWT(("%s Enter AID %d\n", __FUNCTION__, SCB_AID(indv->scb)));

	scb_twt = TWT_SCB(twti, indv->scb);
	scb_twt->apps_prepared = TRUE;
	scb_twt->sp_started = FALSE;

	indv->tx_closed = TRUE;

	wlc_apps_twt_enter(twti->wlc, indv->scb);
}

/*
 * Resume routine to support TWT information.
 */
static void
wlc_twt_sp_resume(wlc_twt_info_t *twti, twt_indv_desc_t *indv)
{
	scb_t *scb = indv->scb;
	twt_scb_t *scb_twt = TWT_SCB(twti,  scb);
	uint32 twt_l;
	uint32 twt_h;

	twt_l = indv->resume_twt_l;
	twt_h = indv->resume_twt_h;

	wlc_uint64_sub(&twt_h, &twt_l, indv->twt_h, indv->twt_l);
	wlc_uint64_sub(&twt_h, &twt_l, 0, 1);
	if (twt_h == WLC_TWT_TSF_MAX) {
		WL_TWT(("%s Resuming AID %d\n", __FUNCTION__, SCB_AID(scb)));

		indv->paused = FALSE;
		indv->resuming = FALSE;
		ASSERT(scb_twt->indv_paused_cnt);
		scb_twt->indv_paused_cnt--;

		if ((scb_twt->indv_active_cnt - 1) == scb_twt->indv_paused_cnt) {
			wlc_twt_prepare_for_first_sp(twti, indv);
		}
	}
}

/*
 * Suspend routine to support TWT information.
 */
static void
wlc_twt_sp_pause(wlc_twt_info_t *twti, scb_t *scb, twt_indv_desc_t *indv, uint16 fc)
{
	wlc_info_t *wlc = twti->wlc;
	twt_scb_t *scb_twt = TWT_SCB(twti, scb);

	WL_TWT(("%s Enter AID %d\n", __FUNCTION__, SCB_AID(scb)));

	indv->paused = TRUE;
	indv->cnt_paused++;
	scb_twt->indv_paused_cnt++;

	if (scb_twt->indv_active_cnt == scb_twt->indv_paused_cnt) {
		wlc_fifo_free_all(wlc->fifo, scb);
		wlc_apps_twt_exit(wlc, scb, fc & FC_PM ? TRUE : FALSE);
		wlc_ratelinkmem_update_link_entry(wlc, scb);
		if (!indv->tx_closed) {
			wlc_hrt_del_timeout(indv->tx_close_timer);
			indv->tx_closed = TRUE;
		}
	}
}

static int
wlc_twt_itwt_start(wlc_twt_info_t *twti, scb_t *scb, twt_indv_desc_t *indv)
{
	wlc_info_t *wlc = twti->wlc;
	twt_scb_t *scb_twt;
	uint32 tsf_h, tsf_l;
	uint32 delta_h, delta_l;
	uint32 wake_interval;
	uint32 wake_duration;
	uint8 i;

	WL_TWT(("%s Enter, AID %d MAC " MACF "\n", __FUNCTION__, SCB_AID(scb),
		ETHERP_TO_MACF(&scb->ea)));

	wlc_twt_get_tsf(wlc, SCB_BSSCFG(scb), &tsf_l, &tsf_h);
	WL_TWT(("%s twt = %08x:%08x (hex), TSF = %08x:%08x (hex)\n",
		__FUNCTION__, indv->twt_h, indv->twt_l, tsf_h, tsf_l));

	WL_TWT(("%s current schedule ID = %d\n",
		__FUNCTION__, WLC_TSFL_TO_SCHEDID(tsf_l)));

	if (BSSCFG_STA(SCB_BSSCFG(scb))) {
		WL_TWT(("%s STA support not implemented, ignoring the START\n", __FUNCTION__));
		return BCME_OK;
	}

	/*
	 * A new iTWT got added. What we need to do is to figure out if we need to start the
	 * TWT RLM scheduler 'machine' or if it is already running and we can lift along. This
	 * all depends on if the scheduler is active and how far in the future the next event
	 * is programmed.
	 */

	wake_interval = indv->desc.wake_interval_mantissa *
		(1 << indv->desc.wake_interval_exponent);
	wake_duration = ((uint32)indv->desc.wake_duration << 8); /* conversion from 256us unit */
	indv->trigger = (indv->desc.flow_flags & WL_TWT_FLOW_FLAG_TRIGGER) ? 1 : 0;
	twti->itwt_trig_count += indv->trigger;

	if (indv->trigger && twti->itwt_trig_count == 1) {
		/* first trigger mode itwt should set twt ul params */
		wlc_ulmu_twt_params(wlc->ulmu, TRUE);
	}

	WL_TWT(("%s SP interval is %d (usec), wake_duration is %d (usec),"
		" trigger=%d trig count=%d\n",
		__FUNCTION__, wake_interval, wake_duration, indv->trigger, twti->itwt_trig_count));

	/* XXX Important: If logging on BUPC then tsf is "late" by 10 msec by now. Fix timing by
	 * rereading TSF here and make sure no more logging till we have started scheduler
	 */
	wlc_twt_get_tsf(wlc, SCB_BSSCFG(scb), &tsf_l, &tsf_h);

	indv->wake_interval = WLC_SCHEDID_TO_TSFL(WLC_TSFL_TO_SCHEDID(wake_interval));
	indv->wake_duration = WLC_TSFL_TO_SCHEDID(wake_duration);

	/* Adjust interval and duration in case of WFA test, see wlc_twt_indv_adjust_twt_params */
	if (wake_duration & WLC_TSFL_SCHEDID_MASK_INV) {
		wake_duration &= WLC_TSFL_SCHEDID_MASK;
		if (indv->desc.wake_duration == 255) {
			/* This is acceptable. The SP will become 256 usec too long, but the
			 * the WFA tests accept a (rather high) percentage of traffic outside
			 * the window.
			 */
			wake_duration += (1 << WLC_SCHEDID_TSF_SHIFT);
		}
	}

	scb_twt = TWT_SCB(twti, scb);
	scb_twt->rlm_id = wlc_ratelinkmem_get_scb_link_index(wlc, scb);

	delta_h = indv->twt_h;
	delta_l = indv->twt_l;
	wlc_uint64_sub(&delta_h, &delta_l, tsf_h, tsf_l);
	/* At this point we should have a delta for the next schedule. Since tsf is in usec it
	 * means tsf_h should be 0 now, since interval of 0xffffffff usec is 4294 secs. We dont
	 * expect that (should test on that during setup request handling !). So if the tsf_h has
	 * a value it should be 0xffffffff. In that case the TWT of the request was wrong (as it
	 * was supposed to be in future (see spec), but we can adjust (by adding interval). If it
	 * is not 0xffffffff or 0 then something is wrong and we should abort. No use to program
	 * some timer for more then 4294 seconds in the future.
	 */
	if (delta_h != 0) {
		WL_ERROR(("%s Delta TSF (%08x:%08x) too far in future or in past\n", __FUNCTION__,
			delta_h, delta_l));
		if (delta_h != WLC_TWT_TSF_MAX) {
			WL_ERROR(("%s TSF not adjustable, aborting\n", __FUNCTION__));
			return BCME_ERROR;
		} else {
			/* Keep adding interval till we delta_h is 0 */
			while (delta_h) {
				wlc_uint64_add(&delta_h, &delta_l, 0, wake_interval);
			}
		}
	}
	/* if delta_l is very close (less then 2 schedid blocks) then add another interval. */
	if (delta_l < WLC_TWT_FIRST_SP_MIN_TIME) {
		wlc_uint64_add(&delta_h, &delta_l, 0, wake_interval);
	}

	if (scb_twt->indv_active_cnt == 1) {
		scb_twt->sp_started = FALSE;
		scb_twt->apps_prepared = FALSE;
		wlc_fifo_free_all(wlc->fifo, scb);
	}

	/* Start by tracking the SP timing in the indv storage, this will be the first time
	 * this SP will be active
	 */
	wlc_uint64_add(&tsf_h, &tsf_l, 0, delta_l);
	indv->twt_h = tsf_h;
	indv->twt_l = tsf_l;

	indv->scb = scb;

	ASSERT(indv->tx_close_timer == NULL);
	/* Allocate the necessary timer resources for traffic */
	indv->tx_close_timer = wlc_hrt_alloc_timeout(wlc->hrti);
	if (!indv->tx_close_timer) {
		WL_ERROR(("%s Failed to allocate tx_close traffic timer\n", __FUNCTION__));
		return BCME_NORESOURCE;
	}

	/* At this point we know when the SP for this new setup is starting. The SP is set to be
	 * ahead in time. Now just call the scheduler to see if it needs an update for this new
	 * schedule.
	 */
	if (!twti->rlm_scheduler_timer) {
		/* Allocate the necessary timer resources for scheduler */
		twti->rlm_scheduler_timer = wlc_hrt_alloc_timeout(wlc->hrti);
		if (!twti->rlm_scheduler_timer) {
			WL_ERROR(("%s Failed to rlm scheduler trigger timer\n", __FUNCTION__));
			if (indv->tx_close_timer) {
				wlc_hrt_free_timeout(indv->tx_close_timer);
				indv->tx_close_timer = NULL;
			}
			return BCME_NORESOURCE;
		}
		/* Clear RLM before ucode TWT gets enabled */
		memset(twti->twtschedblk, 0, sizeof(twti->twtschedblk));
		for (i = 0; i < AMT_IDX_TWT_RSVD_SIZE; i++) {
			wlc_ratelinkmem_update_link_twtschedblk(wlc, i,
				(uint8 *)&twti->twtschedblk[i], sizeof(twti->twtschedblk[i]));
		}
	}

	wlc_twt_itwt_rlm_scheduler_update_trigger(twti);

	WL_TWT(("%s Next SP is in delta %08x (hex), delta schedid=%d\n", __FUNCTION__, delta_l,
		WLC_TSFL_TO_SCHEDID(delta_l)));

	return BCME_OK;
}

static int
wlc_twt_itwt_stop(wlc_twt_info_t *twti, scb_t *scb, twt_indv_desc_t *indv, bool enter_pm)
{
	wlc_info_t *wlc = twti->wlc;
	twt_scb_t *scb_twt;

	if (BSSCFG_STA(SCB_BSSCFG(scb))) {
		WL_TWT(("%s STA support not implemented, ignoring the STOP\n", __FUNCTION__));
		return BCME_OK;
	}

	if (indv->tx_close_timer) {
		wlc_hrt_free_timeout(indv->tx_close_timer);
		indv->tx_close_timer = NULL;
	}

	/* If this is the last Inidividual TWT to get teardown for this SCB, then make sure
	 * APPS is informed.
	 */
	scb_twt = TWT_SCB(twti, scb);
	if (scb_twt->indv_active_cnt == 0) {
		wlc_fifo_free_all(wlc->fifo, scb);
		wlc_apps_twt_exit(wlc, scb, enter_pm);
		/* The last iTWT removals for this scb should trigger linkmem update */
		ASSERT(RATELINKMEM_ENAB(wlc->pub));
		wlc_ratelinkmem_update_link_entry(wlc, scb);
	}

	/* Deactivate the scheduler */
	if ((twti->rlm_scheduler_active) && (twti->itwt_count == 0)) {
		wlc_write_shm(wlc, M_TWTCMD(wlc), 0);
		W_REG(wlc->osh, D11_MACCOMMAND(wlc->hw),
			(R_REG(wlc->osh, D11_MACCOMMAND(wlc->hw)) | MCMD_TWT));
		twti->rlm_scheduler_active = FALSE;
		/* Reset the rlm programming index */
		twti->rlm_prog_idx = 0;
		twti->rlm_read_idx = 0;
		WL_TWT(("%s Stopping ucode TWT engine\n", __FUNCTION__));
	}
	if ((twti->rlm_scheduler_timer) && (twti->itwt_count == 0)) {
		wlc_hrt_free_timeout(twti->rlm_scheduler_timer);
		twti->rlm_scheduler_timer = NULL;
	}

	return BCME_OK;
}

/* This function checks if SP can be opened for Tx. This is based of TSF and TWT of indv. This
 * function can be called from two places depending on the mode (announced). If unannounced then
 * this function gets called from pretwt interrupt. This interrupt is generated 2048 usec ahead of
 * actual scheduling, take this time in account when validating the TSF being inside SP.
 * returns TRUE if SP was released for data.
 */
static bool
wlc_twt_check_for_release_sp(wlc_twt_info_t *twti, twt_indv_desc_t *indv,
	uint32 tsf_h, uint32 tsf_l, bool pretwt_int)
{
	wlc_twt_rlm_t *twt_rlm;
	int i, usr_idx;
	uint16 sp_find, sp_start, sp_offset, sp_dur;
	uint16 usr_info;
	twt_scb_t *scb_twt;
	uint32 timeout;

	if (!indv->tx_closed) {
		return FALSE;
	}

	/* XXX: Adjust the time SP schedid we search for by WLC_PRETWT_TIME. This solves a couple
	 * of issues. If it is pretwt then interrupt can be WLC_PRETWT_TIME earlier, but we
	 * should open for that SP. It also generates a safety margin at the end of the SP. If the
	 * SP is almost at its end: < WLC_PRETWT_TIME, then by adding WLC_PRETWT_TIME it wont be
	 * found anymore, which is good since there is no use to open up tx if the SP is almost
	 * at its end.
	 */
	sp_find = WLC_TSFL_TO_SCHEDID(tsf_l + WLC_PRETWT_TIME);

	scb_twt = TWT_SCB(twti, indv->scb);

	/* Find out if the TSF with USR rlm_id can be found in twtschedblk (as is programmed
	 * towards ucode) within valid schedid. The TSF can be mapped back to schedid. Every
	 * schedule in RLM has a startschedid with duration schedid per rlm_id. The SW can only
	 * use schedid (and open tx) with schedid which is actually programmed in HW
	 */
	for (i = 0; i < ARRAYSIZE(twti->twtschedblk); i++) {
		twt_rlm = &twti->twtschedblk[i];
		sp_start = twt_rlm->schedid;
		for (usr_idx = 0; usr_idx < twt_rlm->usr_count; usr_idx++) {
			usr_info = twt_rlm->usr_info[usr_idx];
			if (((usr_info & TSB_USR_IDX_MASK) >> TSB_USR_IDX_SHIFT) != scb_twt->rlm_id)
				continue;
			sp_offset = sp_find - sp_start;
			sp_dur = ((usr_info & TSB_USR_SPDUR_MASK) >> TSB_USR_SPDUR_SHIFT) + 1;
			if (sp_offset < sp_dur) {
				/* Valid: sp_find is withing range of sp_start and sp_start +
				 * duration
				 */
				scb_twt->cur_sp_start = sp_start;
				indv->tx_closed = FALSE;
				/* Determine time till end of window to be set for close timer */
				timeout = WLC_SCHEDID_TO_TSFL(sp_dur - sp_offset);
				timeout -= ((tsf_l + WLC_PRETWT_TIME) & WLC_TSFL_SCHEDID_MASK_INV);
				timeout += WLC_PRETWT_TIME;
				goto found;
			}
		}
	}
	return FALSE;
found:

#if defined(BCMDBG) || defined(DUMP_TWT)
	/* Statistics; Calculate pre twt interrupt time or Rx packet time (vs SP start time), track
	 * min/avg/max. Cleared by dump routine.
	 */
	{
		int32 pretwt;
		uint32 tmp_tsf_l;
		uint32 twt_h, twt_l;

		indv->cnt_tx_open_tsf_l = tsf_l;
		indv->cnt_tx_open_tsf_h = tsf_h;

		twt_l = tsf_l;
		twt_h = tsf_h;
		wlc_uint64_add(&twt_h, &twt_l, 0, WLC_PRETWT_TIME);
		twt_l &= WLC_TSFL_SCHEDID_MASK;
		if (sp_offset) {
			wlc_uint64_sub(&twt_h, &twt_l, 0, WLC_SCHEDID_TO_TSFL(sp_offset));
		}

		tmp_tsf_l = tsf_l;
		wlc_uint64_sub(&tsf_h, &tmp_tsf_l, twt_h, twt_l);

		pretwt = (int32)tmp_tsf_l;
		if (pretwt == 0) {
			pretwt = 1;
		}

		indv->cnt_pretwt++;
		/* Average over last 16 */
		if (indv->cnt_pretwt > 16) {
			indv->cnt_avg_pretwt *= 15;
			indv->cnt_avg_pretwt /= 16;
		}
		indv->cnt_avg_pretwt += pretwt;
		if ((pretwt < indv->cnt_min_pretwt) ||
			(indv->cnt_min_pretwt == 0)) {
			indv->cnt_min_pretwt = pretwt;
		}
		if ((pretwt > indv->cnt_max_pretwt) ||
			(indv->cnt_max_pretwt == 0)) {
			indv->cnt_max_pretwt = pretwt;
		}
	}
#endif // endif

	wlc_apps_twt_sp_release_ps(twti->wlc, indv->scb);
	wlc_hrt_add_timeout(indv->tx_close_timer, (uint)timeout, wlc_twt_itwt_end_sp, indv);

	return TRUE;
}

void
wlc_twt_intr(wlc_twt_info_t *twti)
{
	wlc_info_t *wlc = twti->wlc;
	twt_indv_desc_t *indv;
	twt_scb_t *scb_twt;
	uint32 tsf_h, tsf_l;
	uint16 schedid, usr_count, schedid_read, usr_info;
	uint8 i, j;
	uint8 tsbidx = wlc_read_shm(wlc, M_TWTINT_DATA(wlc));

	wlc_twt_get_tsf(wlc, wlc->primary_bsscfg, &tsf_l, &tsf_h);

	if (!twti->scb_twt_list) {
		WL_ERROR(("%s Pre TWT interrupt without active schedules (%08x:%08x)\n",
			__FUNCTION__, tsf_h, tsf_l));
		return;
	}

	while (twti->rlm_read_idx != MODINC_POW2(tsbidx, AMT_IDX_TWT_RSVD_SIZE)) {
		schedid = twti->twtschedblk[twti->rlm_read_idx].schedid;
		usr_count = twti->twtschedblk[twti->rlm_read_idx].usr_count;
		/* read for sanity check */
		schedid_read = (wlc_ratelinkmem_lmem_read_word(wlc,
				(AMT_IDX_TWT_RSVD_START + twti->rlm_read_idx), 0) & 0xffff);

		WL_TRACE(("%s: idx [%d %d] tsf 0x%x schedid 0x%x usrcnt %d\n", __FUNCTION__,
			twti->rlm_prog_idx, twti->rlm_read_idx, tsf_l, schedid, usr_count));

		if (twti->rlm_read_idx != tsbidx) {
			twti->cnt_pretwt_late++;
		}

		if (schedid != schedid_read) {
			WL_ERROR(("%s: Unexpected schedid!"
				" cur_tsf 0x%x exp schedid 0x%x read schedid 0x%x\n",
				__FUNCTION__, tsf_l, schedid, schedid_read));
			ASSERT(0);
		}

		for (i = 0; i < usr_count; i++) {
			usr_info = twti->twtschedblk[twti->rlm_read_idx].usr_info[i];

			for (scb_twt = twti->scb_twt_list; scb_twt &&
				scb_twt->rlm_id != TSB_USR_IDX(usr_info); scb_twt = scb_twt->next);

			if (!scb_twt) {
				/* could be removed by teardown before interrupt */
				WL_TWT(("%s scb_twt not found for idx %d\n",
					__FUNCTION__, TSB_USR_IDX(usr_info)));
				continue;
			}
			for (j = 0; j < ARRAYSIZE(scb_twt->indv); j++) {
				indv = &scb_twt->indv[j];
				if (indv->state == WLC_TWT_STATE_ACTIVE) {
					break;
				}
			}
			ASSERT(j < ARRAYSIZE(scb_twt->indv));

			if (indv->desc.flow_flags & WL_TWT_FLOW_FLAG_UNANNOUNCED) {
				if (!wlc_twt_check_for_release_sp(twti, indv, tsf_h, tsf_l, TRUE)) {
					indv->cnt_pretwt_late_sp_closed++;
				}
			}
		}
		twti->rlm_read_idx = MODINC_POW2(twti->rlm_read_idx, AMT_IDX_TWT_RSVD_SIZE);
	}
}

bool
wlc_twt_scb_get_schedid(wlc_twt_info_t *twti, scb_t *scb, uint16 *schedid)
{
	twt_scb_t *scb_twt;

	if (twti == NULL) {
		return FALSE;
	}
	scb_twt = TWT_SCB(twti, scb);
	if ((scb_twt == NULL) || (scb_twt->indv_active_cnt == scb_twt->indv_paused_cnt) ||
	    (!scb_twt->sp_started))  {
		return FALSE;
	}
	*schedid = scb_twt->cur_sp_start;
	scb_twt->indv[0].cnt_tx_pkt++;

#if defined(BCMDBG) || defined(DUMP_TWT)
	/* Statistics; Track time from pre twt interrupt or Rx packet (depending on mode Announced)(
	 * till the first tx packet. This function is called just before the tx packet is to be put
	 * on lo queue so it gives good information on packet latency of system.
	 */
	{
		wlc_info_t *wlc = twti->wlc;
		twt_indv_desc_t *indv;
		uint32 tsf_h;
		uint32 tsf_l;

		indv = &scb_twt->indv[0];
		if ((indv->cnt_tx_open_tsf_h) || (indv->cnt_tx_open_tsf_l)) {

			wlc_twt_get_tsf(wlc, wlc->primary_bsscfg, &tsf_l, &tsf_h);
			wlc_uint64_sub(&tsf_h, &tsf_l, indv->cnt_tx_open_tsf_h,
				indv->cnt_tx_open_tsf_l);
			if (tsf_l == 0) {
				tsf_l = 1;
			}

			indv->cnt_tx_latency++;
			/* Average over last 16 */
			if (indv->cnt_tx_latency > 16) {
				indv->cnt_avg_tx_latency *= 15;
				indv->cnt_avg_tx_latency /= 16;
			}
			indv->cnt_avg_tx_latency += tsf_l;
			if ((tsf_l < indv->cnt_min_tx_latency) ||
				(indv->cnt_min_tx_latency == 0)) {
				indv->cnt_min_tx_latency = tsf_l;
			}
			if ((tsf_l > indv->cnt_max_tx_latency) ||
				(indv->cnt_max_tx_latency == 0)) {
				indv->cnt_max_tx_latency = tsf_l;
			}

			indv->cnt_tx_open_tsf_l = 0;
			indv->cnt_tx_open_tsf_h = 0;
		}
	}
#endif // endif

	return TRUE;
}

/* The function wlc_twt_rx_pkt_trigger is called for every rx packets. TWT can use it to open up
 * individual TWT links which use the ananounced method, meaning data cannot be sent till rx packet
 * had arrived.
 */
void
wlc_twt_rx_pkt_trigger(wlc_twt_info_t *twti, scb_t *scb)
{
	/* NOTE: this cannot work simple: the rx could result in opening up tx path, however
	 * it requires that tx is always closed at end of SP. This is difficult to deal with
	 * as there is no HW/ucode support to deal with this. This means software timer to do
	 * software controlled TWT_PS. This needs to be put into place before we can deal with
	 * announce DL traffic. Build later
	 */
	wlc_info_t *wlc = twti->wlc;
	twt_scb_t *scb_twt;
	twt_indv_desc_t *indv;
	uint32 tsf_h, tsf_l;
	uint8 i;

	if (twti == NULL) {
		return;
	}
	scb_twt = TWT_SCB(twti, scb);
	if (scb_twt == NULL) {
		return;
	}
	if (scb_twt->indv_active_cnt == scb_twt->indv_paused_cnt) {
		return;
	}

	wlc_twt_get_tsf(wlc, wlc->primary_bsscfg, &tsf_l, &tsf_h);

	for (i = 0; i < ARRAYSIZE(scb_twt->indv); i++) {
		indv = &scb_twt->indv[i];
		if (indv->state != WLC_TWT_STATE_ACTIVE) {
			continue;
		}
		/* Again this only works for one individual schedule. A lot of
		 * redesign is needed to make this work with overlapping multiple
		 * schedules on single SCB..
		 */
		if (indv->desc.flow_flags & WL_TWT_FLOW_FLAG_UNANNOUNCED) {
			continue;
		}
		if (wlc_twt_check_for_release_sp(twti, indv, tsf_h, tsf_l, FALSE)) {
			indv->cnt_rx_trig++;
		}
	}
}

/* The function wlc_twt_fill_link_entry is called from BFI module because the linkmem field
 * of BFIConfig1 is shared between BFI and TWT. First BFI fills the field then TWT can update.
 * Once this update happens, the PM bit in rx packetss will be ignored and no longer result in
 * PMQ entries.
 */
void
wlc_twt_fill_link_entry(wlc_twt_info_t *twti, scb_t *scb, d11linkmem_entry_t *link_entry)
{
	twt_scb_t *scb_twt = TWT_SCB(twti, scb);

	if ((scb_twt == NULL) || (scb_twt->indv_active_cnt == scb_twt->indv_paused_cnt)) {
		return;
	}
	link_entry->BFIConfig1 |= (1 << C_LTX_TWTACT_NBIT); /* indicate this is TWT user */
}

bool
wlc_twt_scb_is_trig_enab(wlc_twt_info_t *twti, scb_t *scb)
{
	twt_scb_t *scb_twt;
	twt_indv_desc_t *indv;
	uint8 i;
	bool trigger_enab = FALSE;

	if (twti == NULL) {
		return trigger_enab;
	}
	scb_twt = TWT_SCB(twti, scb);
	if (scb_twt == NULL) {
		return trigger_enab;
	}

	for (i = 0; i < ARRAYSIZE(scb_twt->indv); i++) {
		indv = &scb_twt->indv[i];
		if (indv->state == WLC_TWT_STATE_ACTIVE && indv->trigger) {
			trigger_enab = TRUE;
		}
	}

	return trigger_enab;
}

/* wlc up/init callback, this function is registered to deal with reinit/big hammer handling.
 * When this function is called while the TWT is active, so there are one or more TWT schedules
 * active then it is safe to assume this happened due to reinit of driver. When this happesn
 * ucode will be reset and te RLM scheduler should reinitialize. Since the TWT scheduler timer
 * will continue like normal the safest method to deal with this is clear the RLM and reinit
 * the state such that ucode will get re-initialized on the next schedule.
 */
static int
wlc_twt_wlc_up(void *ctx)
{
	wlc_twt_info_t *twti = ctx;
	wlc_info_t *wlc = twti->wlc;
	twt_scb_t *scb_twt;
	twt_indv_desc_t *indv;
	uint8 i;

	WL_TWT(("%s Enter\n", __FUNCTION__));

	if (twti->rlm_scheduler_active) {
		twti->rlm_scheduler_active = FALSE;
		/* Reset the rlm programming index */
		twti->rlm_prog_idx = 0;
		twti->rlm_read_idx = 0;
		/* Clear RLM before ucode TWT gets enabled */
		memset(twti->twtschedblk, 0, sizeof(twti->twtschedblk));
		for (i = 0; i < AMT_IDX_TWT_RSVD_SIZE; i++) {
			wlc_ratelinkmem_update_link_twtschedblk(wlc, i,
				(uint8 *)&twti->twtschedblk[i], sizeof(twti->twtschedblk[i]));
		}

		/* Reinit all TWT links */
		scb_twt = twti->scb_twt_list;
		while (scb_twt) {
			for (i = 0; i < ARRAYSIZE(scb_twt->indv); i++) {
				if (scb_twt->indv[i].state != WLC_TWT_STATE_ACTIVE) {
					continue;
				}
				indv = &scb_twt->indv[i];
				wlc_apps_twt_exit(wlc, indv->scb, TRUE);
				if (!indv->tx_closed) {
					wlc_hrt_del_timeout(indv->tx_close_timer);
					indv->tx_closed = TRUE;
				}
			}
			scb_twt->apps_prepared = FALSE;
			scb_twt = scb_twt->next;
		}
	}

	return BCME_OK;
}

#endif /* WLTWT */
