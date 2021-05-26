/*
 * wlc_modesw.c -- .
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
 * $Id: wlc_modesw.c 789501 2020-07-30 03:18:50Z $
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_event_utils.h>
#include <bcmwifi_channels.h>
#include <wlc_modesw.h>
#include <wlc_pcb.h>
#include <wlc_scb.h>
#include <wl_export.h>
#include <wlc_bmac.h>
#include <bcmendian.h>
#include <wlc_stf.h>
#include <wlc_ap.h>
#include <wlc_scan.h>
#include <wlc_tx.h>
#include <phy_cache_api.h>
#include <phy_chanmgr_api.h>
#include <phy_calmgr_api.h>
#include <wlc_dfs.h>
#include <wlc_assoc.h>
#ifdef WLMCNX
#include <wlc_mcnx.h>
#endif // endif
#ifdef WLMCHAN
#include <wlc_mchan.h>
#endif // endif
#ifdef WLTDLS
#include <wlc_tdls.h>
#endif // endif
#include <wlc_scb_ratesel.h>
#include <wlc_vht.h>
#include <wlc_pm.h>
#ifdef WLCSA
#include <wlc_csa.h>
#endif // endif
#include <wlc_mutx.h>

#ifdef WL_AIR_IQ
#include <wlc_airiq.h>
#endif // endif
#ifdef WL_ULMU
#include <wlc_ulmu.h>
#endif /* WL_ULMU */
#include <wlc_txbf.h>

enum msw_oper_mode_states {
	MSW_NOT_PENDING = 0,
	MSW_UPGRADE_PENDING = 1,
	MSW_DOWNGRADE_PENDING = 2,
	MSW_UP_PM1_WAIT_ACK = 3,
	MSW_UP_PM0_WAIT_ACK = 4,
	MSW_DN_PM1_WAIT_ACK = 5,
	MSW_DN_PM0_WAIT_ACK = 6,
	MSW_UP_AF_WAIT_ACK = 7,
	MSW_DN_AF_WAIT_ACK = 8,
	MSW_DN_DEFERRED = 9,
	MSW_UP_DEFERRED = 10,
	MSW_DN_PERFORM = 11,
	MSW_UP_PERFORM = 12,
	MSW_WAIT_DRAIN = 13,
	MSW_AP_DN_WAIT_DTIM = 14,
	MSW_AP_MSCH_DN_DEFFERRED = 15, /* 4 new states to handle
					* delay due to MSCH
					*/
	MSW_AP_MSCH_UP_DEFFERRED = 16,
	MSW_AP_MSCH_DN_PERFORM = 17,
	MSW_AP_MSCH_UP_PERFORM = 18,
	MSW_FAILED = 19
};

/* iovar table */
enum {
	IOV_HT_BWSWITCH = 1,
	IOV_DUMP_DYN_BWSW = 2,
	IOV_MODESW_TIME_CALC = 3,
	IOV_OMN_MASTER = 4,
	IOV_MODESW_LAST
};

static const struct {
	uint state;
	char name[40];
} opmode_st_names[] = {
	{MSW_NOT_PENDING, "MSW_NOT_PENDING"},
	{MSW_UPGRADE_PENDING, "MSW_UPGRADE_PENDING"},
	{MSW_DOWNGRADE_PENDING, "MSW_DOWNGRADE_PENDING"},
	{MSW_UP_PM1_WAIT_ACK, "MSW_UP_PM1_WAIT_ACK"},
	{MSW_UP_PM0_WAIT_ACK, "MSW_UP_PM0_WAIT_ACK"},
	{MSW_DN_PM1_WAIT_ACK, "MSW_DN_PM1_WAIT_ACK"},
	{MSW_DN_PM0_WAIT_ACK, "MSW_DN_PM0_WAIT_ACK"},
	{MSW_UP_AF_WAIT_ACK, "MSW_UP_AF_WAIT_ACK"},
	{MSW_DN_AF_WAIT_ACK, "MSW_DN_AF_WAIT_ACK"},
	{MSW_DN_DEFERRED, "MSW_DN_DEFERRED"},
	{MSW_UP_DEFERRED, "MSW_UP_DEFERRED"},
	{MSW_DN_PERFORM, "MSW_DN_PERFORM"},
	{MSW_UP_PERFORM, "MSW_UP_PERFORM"},
	{MSW_WAIT_DRAIN, "MSW_WAIT_DRAIN"},
	{MSW_AP_DN_WAIT_DTIM, "MSW_AP_DN_WAIT_DTIM"},
	{MSW_AP_MSCH_DN_DEFFERRED, "MSW_AP_MSCH_DN_DEFFERRED"},
	{MSW_AP_MSCH_UP_DEFFERRED, "MSW_AP_MSCH_UP_DEFFERRED"},
	{MSW_AP_MSCH_DN_PERFORM, "MSW_AP_MSCH_DN_PERFORM"},
	{MSW_AP_MSCH_UP_PERFORM, "MSW_AP_MSCH_UP_PERFORM"},
	{MSW_FAILED, "MSW_FAILED"}
};

/* Macro Definitions */
#define MODESW_AP_DOWNGRADE_BACKOFF		5

/* Action Frame retry limit init */
#define ACTION_FRAME_RETRY_LIMIT 1

/* After nss/bw upgrade to original capability,  stop publishing OMN's after these many DTIMs  */
#define MODESW_DISABLE_OMN_DTIMS	5

/* IOVAR Specific defines */
#define BWSWITCH_20MHZ	20
#define BWSWITCH_40MHZ	40

#define CTRL_FLAGS_HAS(flags, bit)	((uint32)(flags) & (bit))

/* Extend VHT_ENAB to per-BSS */
#define MODESW_BSS_VHT_ENAB(wlc, cfg) \
	(VHT_ENAB_BAND((wlc)->pub, \
	(wlc)->bandstate[CHSPEC_BANDUNIT((cfg)->current_bss->chanspec)]->bandtype) && \
	!((cfg)->flags & WLC_BSSCFG_VHT_DISABLE))

/* Time Measurement enums */
enum {
	MODESW_TM_START = 0,
	MODESW_TM_PM1 = 1,
	MODESW_TM_PHY_COMPLETE = 2,
	MODESW_TM_PM0 = 3,
	MODESW_TM_ACTIONFRAME_COMPLETE = 4
};

#ifdef WL_MODESW_TIMECAL

#define TIMECAL_LOOP(b, str, val) \
	bcm_bprintf(&b, str); \
	for (val = 0; val < TIME_CALC_LIMT; val++)

/* Time Measurement Macro */
#define TIME_CALC_LIMT 8

/* Time Measurement Global Counters */
uint32 SequenceNum;

/* Increment Counter upto TIME_CALC_LIMT and round around */
#define INCR_CHECK(n, max) \
	if (++n > max) n = 1;

/* Update the Sequeue count in Time Measure Structure and update Sequeue number */
#define SEQ_INCR(n) \
		{n = SequenceNum; SequenceNum++;}
#endif /* WL_MODESW_TIMECAL */

/* Threshold for recovery of mode switch (ms) */
#define MODESW_RECOVERY_THRESHOLD 1500

static int
wlc_modesw_doiovar(void *handle, uint32 actionid,
	void *params, uint plen, void *arg, uint alen, uint vsize, struct wlc_if *wlcif);

static const bcm_iovar_t modesw_iovars[] = {
	{"dummy", IOV_MODESW_LAST,
	(0), 0, IOVT_BOOL, 0
	},
	{"ht_bwswitch", IOV_HT_BWSWITCH, 0, 0, IOVT_UINT32, 0},
#if defined(BCMDBG)
	{"dump_modesw_dyn_bwsw", IOV_DUMP_DYN_BWSW,
	(IOVF_GET_UP), 0, IOVT_BUFFER, WLC_IOCTL_MAXLEN,
	},
#endif // endif
#ifdef WL_MODESW_TIMECAL
	{"modesw_timecal", IOV_MODESW_TIME_CALC,
	(0), 0, IOVT_BUFFER, 0
	},
#endif // endif
	{"omn_master", IOV_OMN_MASTER, 0, 0, IOVT_BUFFER, 0},
	{NULL, 0, 0, 0, 0, 0}
};

typedef struct wlc_modesw_ctx {
	wlc_modesw_info_t *modesw_info;
	uint16		connID;
	void *pkt;
	wlc_bsscfg_t *cfg;
	int status;
	uint8 oper_mode;
	int signal;
	uint8 notif;
} wlc_modesw_cb_ctx_t;

/* Private cubby struct for ModeSW */
typedef struct {
	uint8 oper_mode_new;
	uint8 oper_mode_old;
	uint8 state;
	uint8 opmode_disable_dtim_cnt; /* OMN is disabled after these many DTIMs */
	uint16 action_sendout_counter; /* count of sent out action frames */
	uint16 max_opermode_chanspec;  /* Oper mode channel changespec */
	chanspec_t new_chanspec;
	struct wl_timer *oper_mode_timer;
	wlc_modesw_cb_ctx_t *timer_ctx;
	uint32 ctrl_flags;	/* control some overrides, eg: used for pseudo operation */
	uint8 orig_pmstate;
	uint32 start_time;
	bool is_csa_lock;
	int resume; /* flag to handle minimal bw_switch changes
		     * for other interfaces after primary interface
		     * phy change is already done
		     */
} modesw_bsscfg_cubby_t;

/* SCB Cubby for modesw */
typedef struct {
	wlc_modesw_cb_ctx_t *cb_ctx;
	uint16 modesw_retry_counter;
} modesw_scb_cubby_t;

typedef struct _rsdb_dyn_algo_params {
	/* duration during which the MAX LOAD requirement is observed */
	uint8 modesw_sample_duration;
	/* if the last known load requirement was for primary */
	bool last_load_req_primary;
} rsdb_dyn_algo_params;

/* basic modesw are added in wlc structure having cubby handle */
struct wlc_modesw_info {
	wlc_info_t *wlc;
	int cfgh;   /* bsscfg cubby handle */
	int scbh;		/* scb cubby handle */
	bcm_notif_h modesw_notif_hdl;
	uint8 type_of_switch; /* type of mode switch currently active. R->V or V->R */
	rsdb_dyn_algo_params rsdb_dyn_algo_params; /* Holds parameters for rsdb dyn algo */
	/* determines that modeswitch has just happened, used in case of R -> V */
	uint8 modesw_bw_init_flag;
	bool  modesw_csa_cb_regd;
	bool mutx_cb_regd;
	uint32 event_flags;
	uint16 event_reason;
	uint16 event_trigger;
	wl_omnm_mod_t omnm_mod; /* module that is the current OMN master */
	uint32 omnm_expiry;	/* watchdog tick at which current OMN master lock expires */
#ifdef WL_MODESW_TIMECAL
	bool modesw_timecal_enable;
	uint32 ActDNCnt;
	uint32 PHY_UPCnt;
	uint32 PHY_DNCnt;
	uint32 ActFRM_Cnt;
	modesw_time_calc_t modesw_time[TIME_CALC_LIMT];
#endif /* This should always be at the end of this structure */
};
enum {
	MODESW_TIMER_NULL = 0,
	MODESW_TIMER_BSSCFG_CLONE,
	MODESW_TIMER_AP_BACKOFF_EXP,
	MODESW_TIMER_COMPLETE_DOWNGRADE,
	MODESW_TIMER_EXEC_CALLBACKS
};

/* bsscfg specific info access accessor */
#define MODESW_BSSCFG_CUBBY_LOC(modesw, cfg) \
	((modesw_bsscfg_cubby_t **)BSSCFG_CUBBY((cfg), (modesw)->cfgh))
#define MODESW_BSSCFG_CUBBY(modesw, cfg) (*(MODESW_BSSCFG_CUBBY_LOC(modesw, cfg)))

#define MODESW_SCB_CUBBY_LOC(modesw, scb) ((modesw_scb_cubby_t **)SCB_CUBBY((scb), (modesw)->scbh))
#define MODESW_SCB_CUBBY(modesw, scb) (*(MODESW_SCB_CUBBY_LOC(modesw, scb)))

static uint wlc_modesw_scb_secsz(void *ctx, struct scb *scb);
/* Initiate scb cubby ModeSW context */
static int wlc_modesw_scb_init(void *ctx, struct scb *scb);

static void wlc_modesw_msch_cb_handle(wlc_bsscfg_t *cfg);

/* Remove scb cubby ModeSW context */
static void wlc_modesw_scb_deinit(void *ctx, struct scb *scb);

/* Initiate ModeSW context */
static int wlc_modesw_bss_init(void *ctx, wlc_bsscfg_t *cfg);

/* Remove ModeSW context */
static void wlc_modesw_bss_deinit(void *ctx, wlc_bsscfg_t *cfg);

/* generate WLC_E_MODE_SWITCH event based on arguments passed */
static void
wlc_modesw_send_event(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *cfg,
		uint16 opmode_from, uint16 opmode_to, uint32 event_flags, uint16 event_reason,
		uint8 * event_data, uint16 event_data_len);

static void wlc_modesw_perform_upgrade_downgrade(wlc_modesw_info_t *modesw_info,
	wlc_bsscfg_t *bsscfg);

static void wlc_modesw_oper_mode_timer(void *arg);
void wlc_modesw_bsscfg_move_timer(void *arg);

static int wlc_modesw_change_sta_oper_mode(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *bsscfg,
	uint8 oper_mode, uint8 enabled);

static chanspec_t wlc_modesw_find_downgrade_chanspec(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	uint8 oper_mode_new, uint8 oper_mode_old);

static chanspec_t wlc_modesw_find_upgrade_chanspec(wlc_modesw_info_t *modesw_info,
	wlc_bsscfg_t *cfg, uint8 oper_mode_new, uint8 oper_mode_old);

static bool wlc_modesw_change_ap_oper_mode(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *bsscfg,
	uint8 oper_mode, uint8 enabled);

static void wlc_modesw_ap_upd_bcn_act(wlc_modesw_info_t *modesw_info,
	wlc_bsscfg_t *bsscfg, uint8 state);

static int wlc_modesw_send_action_frame_request(wlc_modesw_info_t *modesw_info,
	wlc_bsscfg_t *bsscfg, const struct ether_addr *ea, uint8 oper_mode_new);

static void wlc_modesw_ap_send_action_frames(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *bsscfg);

static bool wlc_modesw_process_action_frame_status(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	struct scb *scb, bool *cntxt_clear, uint txstatus);

static void wlc_modesw_update_PMstate(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);

static void wlc_modesw_assoc_cxt_cb(void *ctx, bss_assoc_state_data_t *notif_data);
static void wlc_modesw_disassoc_cb(void *ctx, bss_disassoc_notif_data_t *notif_data);

static void wlc_modesw_scb_state_upd_cb(void *ctx, scb_state_upd_data_t *notif_data);
static void wlc_modesw_process_recovery(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *cfg);
static void wlc_modesw_watchdog(void *context);

static void wlc_modesw_update_opermode(wlc_modesw_info_t *modesw,
		chanspec_t chspec, wlc_bsscfg_t *cfg);
static void wlc_modesw_bsscfg_opmode_upd_cb(void *ctx, bsscfg_state_upd_data_t *evt);
static void wlc_modesw_chansw_opmode_upd_cb(void *arg, wlc_chansw_notif_data_t *data);

static void wlc_modesw_dealloc_context(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *cfg);

static int wlc_modesw_opmode_pending(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *cfg);
static void wlc_modesw_clear_phy_chanctx(void *ctx, wlc_bsscfg_t *cfg);

void wlc_modesw_time_measure(wlc_modesw_info_t *modesw_info, uint32 ctrl_flags, uint32 event);

static int wlc_modesw_oper_mode_complete(wlc_modesw_info_t *modesw_info,
	wlc_bsscfg_t* cfg, int status, int signal);

static int wlc_modesw_alloc_context(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *cfg);
static void wlc_modesw_set_pmstate(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t* bsscfg,
	bool state);

static void wlc_modesw_ctrl_hdl_all_cfgs(wlc_modesw_info_t *modesw_info,
		wlc_bsscfg_t *cfg, enum notif_events signal);
#ifdef WLCSA
static void wlc_modesw_csa_cb(void *ctx, wlc_csa_cb_data_t *notif_data);
#endif // endif

static void wlc_modesw_restore_defaults(void *ctx, wlc_bsscfg_t *bsscfg, bool isup);

static void wlc_modesw_notify_cleanup(wlc_modesw_info_t* modesw_info, wlc_bsscfg_t* bsscfg);

static void wlc_modesw_notif_cb_notif(wlc_modesw_info_t *modeswinfo, wlc_bsscfg_t *cfg, int status,
	uint8 oper_mode, uint8 oper_mode_old, int signal);

static int wlc_modesw_omn_master(wlc_modesw_info_t *modesw_info, void *in, int in_len,
	void *out, int out_len);

#if defined(BCMDBG)
static int wlc_modesw_dyn_bwsw_dump(wlc_modesw_info_t *modesw_info, void *input, int buf_len,
	void *output);
#endif // endif

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

/* ModeSW attach Function to Register Module and reserve cubby Structure */
wlc_modesw_info_t *
BCMATTACHFN(wlc_modesw_attach)(wlc_info_t *wlc)
{
	bsscfg_cubby_params_t cubby_params;
	scb_cubby_params_t scb_cubby_params;
	wlc_modesw_info_t *modesw_info;
	bcm_notif_module_t *notif;

	if (!(modesw_info = (wlc_modesw_info_t *)MALLOCZ(wlc->osh, sizeof(wlc_modesw_info_t)))) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}
	modesw_info->modesw_csa_cb_regd = FALSE;
	modesw_info->wlc = wlc;

	/* reserve the bsscfg cubby for any bss specific private data */
	bzero(&cubby_params, sizeof(cubby_params));

	cubby_params.context = modesw_info;
	cubby_params.fn_init = wlc_modesw_bss_init;
	cubby_params.fn_deinit = wlc_modesw_bss_deinit;

	modesw_info->cfgh = wlc_bsscfg_cubby_reserve_ext(wlc, sizeof(modesw_bsscfg_cubby_t *),
	                                                 &cubby_params);

	if (modesw_info->cfgh < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve_ext() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto err;
	}

	/* reserve cubby in the scb container for per-scb private data */
	bzero(&scb_cubby_params, sizeof(scb_cubby_params));

	scb_cubby_params.context = modesw_info;
	scb_cubby_params.fn_init = wlc_modesw_scb_init;
	scb_cubby_params.fn_deinit = wlc_modesw_scb_deinit;
	scb_cubby_params.fn_secsz = wlc_modesw_scb_secsz;
	modesw_info->scbh = wlc_scb_cubby_reserve_ext(wlc, sizeof(modesw_scb_cubby_t *),
		&scb_cubby_params);
	if (modesw_info->scbh < 0) {
		WL_ERROR(("wl%d: %s: wlc_scb_cubby_reserve() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto err;
	}

	/* Init the modesw MAX load requirement duration */
	modesw_info->rsdb_dyn_algo_params.modesw_sample_duration = 0;

	/* register module */
	if (wlc_module_register(wlc->pub, modesw_iovars, "modesw", modesw_info, wlc_modesw_doiovar,
		wlc_modesw_watchdog, NULL, NULL)) {
		WL_ERROR(("wl%d: auth wlc_module_register() failed\n", wlc->pub->unit));
		goto err;
	}

	/* create notification list update. */
	notif = wlc->notif;
	ASSERT(notif != NULL);
	if (bcm_notif_create_list(notif, &modesw_info->modesw_notif_hdl) != BCME_OK) {
		WL_ERROR(("wl%d: %s: modesw_info bcm_notif_create_list() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto err;
	}

	if (wlc_bss_assoc_state_register(wlc, wlc_modesw_assoc_cxt_cb, modesw_info) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bss_assoc_state_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto err;
	}

	if (wlc_bss_disassoc_notif_register(wlc, wlc_modesw_disassoc_cb,
			modesw_info) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bss_disassoc_notif_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto err;
	}

	/* Add client callback to the scb state notification list */
	if ((wlc_scb_state_upd_register(wlc, wlc_modesw_scb_state_upd_cb,
		modesw_info)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: unable to register callback %p\n",
			wlc->pub->unit, __FUNCTION__,
			OSL_OBFUSCATE_BUF(wlc_modesw_scb_state_upd_cb)));
		goto err;
	}

	if ((wlc_bsscfg_state_upd_register(wlc,
			wlc_modesw_bsscfg_opmode_upd_cb, modesw_info)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_state_upd_register failed\n",
		          wlc->pub->unit, __FUNCTION__));
			goto err;
	}

	if ((wlc_chansw_notif_register(wlc,
			wlc_modesw_chansw_opmode_upd_cb, wlc)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_chansw_notif_register failed.",
		          wlc->pub->unit, __FUNCTION__));
		goto err;
	}

	wlc->pub->_modesw = TRUE;

	return modesw_info;
err:
	MODULE_DETACH(modesw_info, wlc_modesw_detach);
	return NULL;
}

/* Detach modesw Module from system */
void
BCMATTACHFN(wlc_modesw_detach)(wlc_modesw_info_t *modesw_info)
{
	wlc_info_t *wlc = NULL;
	if (!modesw_info)
		return;

	wlc = modesw_info->wlc;
	WL_TRACE(("wl%d: wlc_modesw_detach\n", wlc->pub->unit));
	wlc_module_unregister(wlc->pub, "modesw", modesw_info);

	if (modesw_info->modesw_notif_hdl != NULL) {
		WL_MODE_SWITCH(("REMOVING NOTIFICATION LIST\n"));
		bcm_notif_delete_list(&modesw_info->modesw_notif_hdl);
	}

	if (wlc_bss_disassoc_notif_unregister(wlc, wlc_modesw_disassoc_cb,
			modesw_info) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bss_disassoc_notif_unregister() failed\n",
			wlc->pub->unit, __FUNCTION__));
	}

	if (wlc_bss_assoc_state_unregister(wlc, wlc_modesw_assoc_cxt_cb, wlc->modesw) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bss_assoc_state_unregister() failed\n",
			wlc->pub->unit, __FUNCTION__));
	}

	/* Remove client callback for the scb state notification list */
	wlc_scb_state_upd_unregister(wlc, wlc_modesw_scb_state_upd_cb, modesw_info);

	if ((wlc_bsscfg_state_upd_unregister(wlc,
			wlc_modesw_bsscfg_opmode_upd_cb, modesw_info)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_state_upd_unregister failed\n",
		          wlc->pub->unit, __FUNCTION__));
	}

	if ((wlc_chansw_notif_unregister(wlc,
			wlc_modesw_chansw_opmode_upd_cb, wlc)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_chansw_notif_unregister() failed\n",
				wlc->pub->unit, __FUNCTION__));
	}

	MFREE(wlc->osh, modesw_info, sizeof(wlc_modesw_info_t));
}

/* Send event WLC_E_OMN_MASTER */
static void
wlc_modesw_send_omnm_event(wlc_modesw_info_t *mswi)
{
	wlc_info_t *wlc = mswi->wlc;
	wl_event_omnm_t ev;

	ev.ver = (uint16) WLC_E_OMNM_VER;
	ev.len = (uint16) sizeof(ev);
	ev.mod = (uint16) mswi->omnm_mod;
	ev.lock_life = (uint16) ((mswi->omnm_expiry > wlc->pub->now) ?
		(mswi->omnm_expiry - wlc->pub->now) : (0));

	wlc_bss_mac_event(wlc, wlc->primary_bsscfg, WLC_E_OMN_MASTER, NULL, 0,
			0, 0, &ev, ev.len);
	WL_MODE_SWITCH(("wl%d: %s: sending event WLC_E_OMN_MASTER/%d with mod=%d\n",
			wlc->pub->unit, __FUNCTION__, WLC_E_OMN_MASTER, ev.mod));
}

/* Call this from IOV or other modules to set a OMN master before requesting mode-switch.
 * Contract: Participating modules must check return status before issuing mode switch requests.
 * mswi  : mode switch info context
 * mod   : index of module to be set as master
 * flags : see flags in wl_omnm_info_t
 * Returns
 *	BCME_OK		if successful
 *	BCME_BUSY	if other modules are holding locks
 *	BCME_BADARG	if input arguments are wrong
 *	and other negative values to indicate other errors
 */
int
wlc_modesw_omn_master_set(wlc_modesw_info_t *mswi, wl_omnm_mod_t mod, uint16 flags)
{
	wlc_info_t *wlc;
	int ret = BCME_OK;

	if (!mswi) {
		return BCME_BADARG;
	}

	wlc = mswi->wlc;

	WL_MODE_SWITCH(("wl%d: %s: called with mod=%d, flags=0x%04x\n",
			wlc->pub->unit, __FUNCTION__, mod, flags));

	if (mod <= WL_OMNM_MOD_NONE || mod >= WL_OMNM_MOD_LAST) {
		WL_MODE_SWITCH(("wl%d: %s: invalid mod=%d\n",
				wlc->pub->unit, __FUNCTION__, mod));
		return BCME_BADARG;
	}

	if (mswi->omnm_mod != mod && mswi->omnm_expiry > wlc->pub->now) { /* other master locked  */
		if (mswi->omnm_mod < mod) {
			ret = BCME_BUSY;
			WL_MODE_SWITCH(("wl%d: %s: ret=%d better ranking mod has the lock\n",
					wlc->pub->unit, __FUNCTION__, mod));
			return BCME_BUSY; /* better ranking module is holding the lock */
		}
		if (mswi->omnm_mod && mswi->omnm_mod > mod && (flags & WL_OMNM_FLAG_SKIP) != 0) {
			ret = BCME_BUSY; /* skip taking lock from lower rank module as requested */
			WL_MODE_SWITCH(("wl%d: %s: ret=%d skip taking lock for mod %d from lower "
					"rank mod %d as requested by flags 0x%04x\n",
					wlc->pub->unit, __FUNCTION__, ret, mod,
					mswi->omnm_mod, flags));
			return ret;
		}

		/* let current master unlock/abort cleanly before a different module is locked */
		switch (mswi->omnm_mod) {
			case WL_OMNM_MOD_ZDFS:
				/* other modules may not abort this at present */
				break;
			case WL_OMNM_MOD_AIRIQ:
#ifdef WL_AIR_IQ
				if ((ret = wlc_airiq_scan_abort(wlc->airiq, TRUE)) != BCME_OK) {
					WL_MODE_SWITCH(("wl%d: %s: ret=%d failed to abort airiq\n",
							wlc->pub->unit, __FUNCTION__, mod));
					return ret;
				}
#endif /* WL_AIR_IQ */
				break;
			case WL_OMNM_MOD_OBSS_DBS:
				break;
			case WL_OMNM_MOD_DYN160:
				break;
			case WL_OMNM_MOD_BW160:
				break;
			default:
				return BCME_UNSUPPORTED;
		}
	}

	mswi->omnm_mod = mod; /* update current master */
	mswi->omnm_expiry = wlc->pub->now + WL_OMNM_LOCK_SECONDS; /* lock expiry */

	WL_MODE_SWITCH(("wl%d: %s: set mod=%d as master with expiry after %d secs at "
			"tick# %d (where now = %d ticks)\n",
			wlc->pub->unit, __FUNCTION__, mod, WL_OMNM_LOCK_SECONDS,
			mswi->omnm_expiry, wlc->pub->now));

	wlc_modesw_send_omnm_event(mswi);

	return ret;
}

/* Fill OMN master state into structure */
int
wlc_modesw_omn_master_get(wlc_modesw_info_t *mswi, wl_omnm_info_t *omnm_out)
{
	wlc_info_t *wlc;
	const int omnm_sz = sizeof(*omnm_out);

	if (!mswi) {
		return BCME_ERROR;
	}

	wlc = mswi->wlc;

	if (!omnm_out) {
		WL_MODE_SWITCH(("wl%d: %s: NULL pointer omnm_out=%p\n",
				wlc->pub->unit, __FUNCTION__, omnm_out));
		return BCME_ERROR;
	}

	memset(omnm_out, 0, omnm_sz);

	omnm_out->ver = (uint16) WL_OMNM_VER;
	omnm_out->len = (uint16) omnm_sz;
	omnm_out->mod = (uint16) mswi->omnm_mod;
	omnm_out->lock_life = (uint16) ((mswi->omnm_expiry > wlc->pub->now) ?
			(mswi->omnm_expiry - wlc->pub->now) : (0));

	WL_MODE_SWITCH(("wl%d: %s [ver: %d, len: %d, act: %d, flags: %d, mod: %d, lock_life: %d]\n",
			wlc->pub->unit, __FUNCTION__, omnm_out->ver, omnm_out->len, omnm_out->act,
			omnm_out->flags, omnm_out->mod, omnm_out->lock_life));

	return BCME_OK;
}

int
wlc_modesw_omn_master_clear(wlc_modesw_info_t *mswi, wl_omnm_mod_t mod)
{
	wlc_info_t *wlc = mswi->wlc;

	if (mod != mswi->omnm_mod) {
		WL_MODE_SWITCH(("wl%d: %s: requested mod %d is not current master %d\n",
				wlc->pub->unit, __FUNCTION__, mod, mswi->omnm_mod));
		return BCME_BADARG;
	}

	WL_MODE_SWITCH(("wl%d: %s: clearing lock of mod %d\n",
			wlc->pub->unit, __FUNCTION__, mod));
	mswi->omnm_mod = WL_OMNM_MOD_NONE;
	mswi->omnm_expiry = wlc->pub->now;

	return BCME_OK;
}

static int
wlc_modesw_omn_master(wlc_modesw_info_t *mswi, void *in, int in_len,
	void *out, int out_len)
{
	wlc_info_t *wlc = mswi->wlc;
	wl_omnm_info_t *omnm_in = (wl_omnm_info_t *)in, omnm_zero = {0};
	const int omnm_sz = sizeof(*omnm_in);
	int ret = BCME_OK;

	BCM_REFERENCE(wlc); /* used only if logs are compiled in */
	/* in absence of input, treat it as an attempt to GET */
	if (!in || in_len < omnm_sz || (in_len >= omnm_sz &&
			!memcmp(omnm_in, &omnm_zero, sizeof(omnm_zero)))) {
		WL_MODE_SWITCH(("wl%d: %s: no explicit input (in=%p, in_len=%d); going to get\n",
				wlc->pub->unit, __FUNCTION__, in, in_len));
		goto OMNM_GET;
	}

	WL_MODE_SWITCH(("wl%d: %s [ver: %d, len: %d, act: %d, flags: %d, mod: %d, lock_life: %d]\n",
			wlc->pub->unit, __FUNCTION__, omnm_in->ver, omnm_in->len, omnm_in->act,
			omnm_in->flags, omnm_in->mod, omnm_in->lock_life));

	if (omnm_in->ver != WL_OMNM_VER) {
		WL_MODE_SWITCH(("wl%d: %s: version %d unknown; expected %d\n",
				wlc->pub->unit, __FUNCTION__, omnm_in->ver, WL_OMNM_VER));
		return BCME_VERSION;
	}

	if (omnm_in->len < omnm_sz) {
		WL_MODE_SWITCH(("wl%d: %s: len %d < expected %d\n",
				wlc->pub->unit, __FUNCTION__, omnm_in->len, omnm_sz));
		return BCME_BUFTOOSHORT;
	}

	if (omnm_in->act == WL_OMNM_ACT_GET) {
		goto OMNM_GET;
	}

	if (omnm_in->act == WL_OMNM_ACT_CLEAR) {
		(void) wlc_modesw_omn_master_clear(mswi, omnm_in->mod);
		goto OMNM_GET;
	}

	if (omnm_in->act == WL_OMNM_ACT_SET) {
		if ((ret = wlc_modesw_omn_master_set(mswi, omnm_in->mod, omnm_in->flags))
				!= BCME_OK) {
			WL_MODE_SWITCH(("wl%d: %s: failed to set ret=%d\n",
					wlc->pub->unit, __FUNCTION__, ret));
			return ret;
		}
	}

OMNM_GET:
	if (!out || out_len < omnm_sz) {
		WL_MODE_SWITCH(("wl%d: %s: output null or small out:%p, out_len:%d\n",
				wlc->pub->unit, __FUNCTION__, out, out_len));
		return BCME_BUFTOOSHORT;
	}

	return wlc_modesw_omn_master_get(mswi, (wl_omnm_info_t *)out);
}

#if defined(BCMDBG)
static int
wlc_modesw_dyn_bwsw_dump(wlc_modesw_info_t *modesw_info, void *input, int buf_len, void *output)
{
	wlc_info_t *wlc = modesw_info->wlc;
	wlc_bsscfg_t *cfg;
	int idx;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = NULL;
	struct bcmstrbuf b;
	char *buf_ptr;

	buf_ptr = (char *) output;

	bcm_binit(&b, buf_ptr, buf_len);

	bcm_bprintf(&b, "Dump Details :\n");

	FOREACH_AS_BSS(wlc, idx, cfg) {
		pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, cfg);

		bcm_bprintf(&b, "\n\nInterface ID=%d\n", cfg->ID);
		bcm_bprintf(&b, "Modesw State=%d\n", pmodesw_bsscfg->state);
		bcm_bprintf(&b, "New Chanspec=%x\n", pmodesw_bsscfg->new_chanspec);
		bcm_bprintf(&b, "Max Opermode Chanspec =%x\n",
			pmodesw_bsscfg->max_opermode_chanspec);
		bcm_bprintf(&b, "Action Sendout Counter = %d\n",
			pmodesw_bsscfg->action_sendout_counter);
		bcm_bprintf(&b, "Old Oper mode = %x\n", pmodesw_bsscfg->oper_mode_old);
		bcm_bprintf(&b, "New oper mode = %x\n", pmodesw_bsscfg->oper_mode_new);
	}
	return 0;
}
#endif // endif

/* function sets the modesw init flag, which determines whether modesw just happened */
INLINE void
wlc_set_modesw_bw_init(wlc_modesw_info_t *modesw, bool init)
{
	modesw->modesw_bw_init_flag = init;
}

/* function tells whether modesw just happened or not */
INLINE bool
wlc_get_modesw_bw_init(wlc_modesw_info_t *modesw)
{
	return modesw->modesw_bw_init_flag;
}

/* function tells whether the last load requirement was for primary or not */
INLINE bool
wlc_modesw_get_last_load_req_if(wlc_modesw_info_t* modesw)
{
	return modesw->rsdb_dyn_algo_params.last_load_req_primary;
}

/* function sets the last load requirement; TRUE if primary, FALSE if secondary */
INLINE void
wlc_modesw_set_last_load_req_if(wlc_modesw_info_t* modesw, bool setval)
{
	modesw->rsdb_dyn_algo_params.last_load_req_primary = setval;
}

/* function returns the duration during which the MAX load requirement is seen */
INLINE
uint8 wlc_modesw_get_sample_dur(wlc_modesw_info_t* modesw)
{
	return modesw->rsdb_dyn_algo_params.modesw_sample_duration;
}

/* function sets the duration for MAX load requirement */
INLINE
void wlc_modesw_set_sample_dur(wlc_modesw_info_t* modesw, uint8 setval)
{
	modesw->rsdb_dyn_algo_params.modesw_sample_duration = setval;
}

/* function return the type of switch currently active */
INLINE
modesw_type wlc_modesw_get_switch_type(wlc_modesw_info_t *modesw)
{
	modesw_type ret = MODESW_NO_SW_IN_PROGRESS;
	if (modesw) {
		ret = modesw->type_of_switch;
	}
	return ret;
}

/* function sets the type of switch */
INLINE
void wlc_modesw_set_switch_type(wlc_modesw_info_t *modesw, modesw_type type)
{
	if (modesw && (type < MODESW_LIST_END)) {
		modesw->type_of_switch = type;
	}
}

#ifdef WL_MODESW_TIMECAL
static int
wlc_modesw_time_stats(wlc_modesw_info_t *modesw_info, void *input, int buf_len,
void *output)
{
	char *buf_ptr;
	int val;
	struct bcmstrbuf b;
	modesw_time_calc_t *pmodesw_time = modesw_info->modesw_time;
	buf_ptr = (char *)output;
	bcm_binit(&b, buf_ptr, buf_len);

	if (modesw_info->modesw_timecal_enable == 1)
	{
		bcm_bprintf(&b, "\n\t\t\t\t Statistics (Time in microseconds)\n");

		TIMECAL_LOOP(b, "\n Index \t\t\t", val)
			bcm_bprintf(&b, "\t %d", val);

		/* Actual Downgrade */
		TIMECAL_LOOP(b, "\n DN Seq No :\t\t", val)
			bcm_bprintf(&b, "\t %d", pmodesw_time[val].ActDN_SeqCnt);

		TIMECAL_LOOP(b, "\n DN Action + ACK Time:\t", val)
			bcm_bprintf(&b, "\t %d",
				pmodesw_time[val].DN_ActionFrame_time -
				pmodesw_time[val].DN_start_time);

		TIMECAL_LOOP(b, "\n DN PHY Down Time\t", val)
			bcm_bprintf(&b, "\t %d", pmodesw_time[val].DN_PHY_BW_UPDTime -
				pmodesw_time[val].DN_ActionFrame_time);

		TIMECAL_LOOP(b, "\n DN PM Transition Time:\t", val)
			bcm_bprintf(&b, "\t %d", pmodesw_time[val].DN_CompTime -
				pmodesw_time[val].DN_PHY_BW_UPDTime);

		TIMECAL_LOOP(b, "\n DN Total Time:\t\t", val)
			bcm_bprintf(&b, "\t %d", pmodesw_time[val].DN_CompTime -
				pmodesw_time[val].DN_start_time);

		/* PHY  Upgrade */
		TIMECAL_LOOP(b, "\n Pseudo UP Seq No:\t", val)
			bcm_bprintf(&b, "\t %d", pmodesw_time[val].PHY_UP_SeqCnt);

		TIMECAL_LOOP(b, "\n Pseudo UP Process Time:", val)
			bcm_bprintf(&b, "\t %d", pmodesw_time[val].PHY_UP_PM1_time -
				pmodesw_time[val].PHY_UP_start_time);

		TIMECAL_LOOP(b, "\n Pseudo UP PHY Time:\t", val)
			bcm_bprintf(&b, "\t %d", pmodesw_time[val].PHY_UP_BW_UPDTime
				- pmodesw_time[val].PHY_UP_PM1_time);

		TIMECAL_LOOP(b, "\n Pseudo UP PM TransTime:", val)
			bcm_bprintf(&b, "\t %d", pmodesw_time[val].PHY_UP_CompTime
				- pmodesw_time[val].PHY_UP_BW_UPDTime);

		TIMECAL_LOOP(b, "\n Pseudo UP CompleteTime:", val)
			bcm_bprintf(&b, "\t %d", pmodesw_time[val].PHY_UP_CompTime
				- pmodesw_time[val].PHY_UP_start_time);

		/* PHY  Downgrade */
		TIMECAL_LOOP(b, "\n Pseudo DN Seq No:\t", val)
			bcm_bprintf(&b, "\t %d", pmodesw_time[val].PHY_DN_SeqCnt);

		TIMECAL_LOOP(b, "\n Pseudo DN Process Time:", val)
			bcm_bprintf(&b, "\t %d", pmodesw_time[val].PHY_DN_PM1_time -
				pmodesw_time[val].PHY_DN_start_time);

		TIMECAL_LOOP(b, "\n Pseudo DN PHY Time:\t", val)
			bcm_bprintf(&b, "\t %d", pmodesw_time[val].PHY_DN_BW_UPDTime
				- pmodesw_time[val].PHY_DN_PM1_time);

		TIMECAL_LOOP(b, "\n Pseudo DN PM TransTime:", val)
			bcm_bprintf(&b, "\t %d", pmodesw_time[val].PHY_DN_CompTime
				- pmodesw_time[val].PHY_DN_BW_UPDTime);

		TIMECAL_LOOP(b, "\n Pseudo DN CompleteTime:", val)
			bcm_bprintf(&b, "\t %d", pmodesw_time[val].PHY_DN_CompTime
				- pmodesw_time[val].PHY_DN_start_time);
		TIMECAL_LOOP(b, "\n Normal Upgrade Seq No:\t", val)
			bcm_bprintf(&b, "\t %d", pmodesw_time[val].ACTFrame_SeqCnt);

		TIMECAL_LOOP(b, "\n Normal UP CompleteTime:", val)
			bcm_bprintf(&b, "\t %d", pmodesw_time[val].ACTFrame_complete -
				pmodesw_time[val].ACTFrame_start);
		bcm_bprintf(&b, "\n");
	}
	else
	{
		bcm_bprintf(&b, "\n Modesw timecal Disabled \n");
		bcm_bprintf(&b, " No Statistics \n");
	}
	return 0;
}
#endif /* WL_MODESW_TIMECAL */

/* Iovar functionality if any should be added in this function */
static int
wlc_modesw_doiovar(void *handle, uint32 actionid,
	void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif)
{
	int32 *ret_int_ptr;
	wlc_modesw_info_t *modesw = (wlc_modesw_info_t *)handle;
	int32 int_val = 0;
	int err = BCME_OK;
	bool bool_val;
	wlc_info_t *wlc = modesw->wlc;
	wlc_bsscfg_t *bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)a;
	BCM_REFERENCE(ret_int_ptr);
	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	bool_val = (int_val != 0) ? TRUE : FALSE;
	BCM_REFERENCE(bool_val);

	switch (actionid) {
		case IOV_SVAL(IOV_HT_BWSWITCH) :
			if (WLC_MODESW_ENAB(wlc->pub)) {
				uint8 bw;

				if (wlc_modesw_is_req_valid(modesw, bsscfg) != TRUE) {
					err = BCME_BUSY;
					break;
				}
				if (int_val == BWSWITCH_20MHZ)
					bw = DOT11_OPER_MODE_20MHZ;

				else if (int_val == BWSWITCH_40MHZ)
					bw = DOT11_OPER_MODE_40MHZ;

				else
				break;

				err = wlc_modesw_handle_oper_mode_notif_request(modesw,
					bsscfg, bw, TRUE, 0);
			} else
				err = BCME_UNSUPPORTED;
			break;
		case IOV_GVAL(IOV_HT_BWSWITCH) :
			wlc_sync_txfifo_all(wlc, bsscfg->wlcif->qi, SYNCFIFO);
			break;
#if defined(BCMDBG)
		case IOV_GVAL(IOV_DUMP_DYN_BWSW) :
			err = wlc_modesw_dyn_bwsw_dump(modesw, p, alen, a);
			break;
#endif // endif
#ifdef WL_MODESW_TIMECAL
		case IOV_SVAL(IOV_MODESW_TIME_CALC) :
			modesw->modesw_timecal_enable = bool_val;
			break;
		case IOV_GVAL(IOV_MODESW_TIME_CALC) :
		{
			err = wlc_modesw_time_stats(modesw, p, alen, a);
			break;
		}
#endif /* WL_MODESW_TIMECAL */
		case IOV_SVAL(IOV_OMN_MASTER) :
		case IOV_GVAL(IOV_OMN_MASTER) :
		{
			err = wlc_modesw_omn_master(modesw, p, plen, a, alen);
			break;
		}
		default:
			err = BCME_UNSUPPORTED;
	}

	return err;
}

/* Allocate modesw context ,
 * and return the status back
 */
int
wlc_modesw_bss_init(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_modesw_info_t *modesw_info = (wlc_modesw_info_t *)ctx;
	wlc_info_t *wlc = modesw_info->wlc;
	modesw_bsscfg_cubby_t **pmodesw_bsscfg = MODESW_BSSCFG_CUBBY_LOC(modesw_info, cfg);
	modesw_bsscfg_cubby_t *modesw = NULL;

	if (!BSSCFG_INFRA_STA_OR_AP(cfg)) {
		*pmodesw_bsscfg = NULL;
		return BCME_OK;
	}
	/* allocate memory and point bsscfg cubby to it */
	if ((modesw = MALLOCZ(wlc->osh, sizeof(modesw_bsscfg_cubby_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return BCME_NOMEM;
	}

	*pmodesw_bsscfg = modesw;

	return BCME_OK;
}

/* Toss ModeSW context */
static void
wlc_modesw_bss_deinit(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_modesw_info_t *modesw_info = (wlc_modesw_info_t *)ctx;
	wlc_info_t *wlc = modesw_info->wlc;
	modesw_bsscfg_cubby_t **pmodesw_bsscfg = MODESW_BSSCFG_CUBBY_LOC(modesw_info, cfg);
	modesw_bsscfg_cubby_t *modesw, *ipmode_cfg;
	wlc_bsscfg_t* icfg;
	int idx;

	/* free the Cubby reserve allocated memory  */
	modesw = *pmodesw_bsscfg;
	if (modesw) {
		/* timer and Context Free */
		wlc_modesw_dealloc_context(modesw_info, cfg);

		MFREE(wlc->osh, modesw, sizeof(modesw_bsscfg_cubby_t));
		*pmodesw_bsscfg = NULL;
	}

	/* Check if this cfg is the only cfg
	* pending for opmode change ?
	*/
	FOREACH_AS_BSS(wlc, idx, icfg) {
		ipmode_cfg = MODESW_BSSCFG_CUBBY(modesw_info, icfg);
		if (icfg != cfg &&
		    (ipmode_cfg->state != MSW_NOT_PENDING)) {
			WL_MODE_SWITCH(("wl%d: Opmode change pending for cfg = %d..\n",
				WLCWLUNIT(wlc), icfg->_idx));
			return;
		}
	}
	wlc_user_wake_upd(wlc, WLC_USER_WAKE_REQ_VHT, FALSE);
	return;
}

uint
wlc_modesw_scb_secsz(void *ctx, struct scb *scb)
{
	uint size = 0;

	if (scb && !SCB_INTERNAL(scb)) {
		size = sizeof(modesw_scb_cubby_t);
	}
	return size;
}

/* Allocate modesw context , and return the status back
 */
int
wlc_modesw_scb_init(void *ctx, struct scb *scb)
{
	wlc_modesw_info_t *modeswinfo = (wlc_modesw_info_t *)ctx;
	wlc_info_t *wlc = modeswinfo->wlc;
	modesw_scb_cubby_t **pmodesw_scb = MODESW_SCB_CUBBY_LOC(modeswinfo, scb);

	*pmodesw_scb = wlc_scb_sec_cubby_alloc(wlc, scb, wlc_modesw_scb_secsz(ctx, scb));

	return BCME_OK;
}

static void
wlc_modesw_scb_deinit(void *ctx, struct scb *scb)
{
	wlc_modesw_info_t *modeswinfo = (wlc_modesw_info_t *)ctx;
	wlc_info_t *wlc = modeswinfo->wlc;
	modesw_scb_cubby_t **pmodesw_scb = MODESW_SCB_CUBBY_LOC(modeswinfo, scb);
	modesw_scb_cubby_t *modesw_scb = *pmodesw_scb;

	if (modesw_scb != NULL) {
		if (modesw_scb->cb_ctx != NULL) {
			MFREE(wlc->osh, modesw_scb->cb_ctx,
				sizeof(wlc_modesw_cb_ctx_t));
			modesw_scb->cb_ctx = NULL;
		}
		wlc_scb_sec_cubby_free(wlc, scb, modesw_scb);
	}
	*pmodesw_scb = NULL;
}

static void
wlc_modesw_process_ap_backoff_expiry(wlc_modesw_info_t *modesw, wlc_bsscfg_t *bsscfg)
{
	wlc_info_t *wlc = (wlc_info_t *)modesw->wlc;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = NULL;

	WL_MODE_SWITCH(("%s wl%d: AP downgrade timer expired...Perform the downgrade of AP.\n",
		__FUNCTION__, WLCWLUNIT(wlc)));

	pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(wlc->modesw, bsscfg);

#ifdef WLMCHAN
	if (MCHAN_ACTIVE(wlc->pub) && (!wlc_ap_count(wlc->ap, FALSE))) {
		if (pmodesw_bsscfg->state == MSW_DN_PERFORM) {
			if (pmodesw_bsscfg->action_sendout_counter == 0) {
				WL_MODE_SWITCH(("wl%d: All acks received.... starting downgrade\n",
					WLCWLUNIT(wlc)));
				wlc_modesw_perform_upgrade_downgrade(wlc->modesw, bsscfg);
				wlc_modesw_change_state(wlc->modesw, bsscfg, MSW_NOT_PENDING);
				WL_MODE_SWITCH(("\nwl%d: Sending signal = MODESW_DN_AP_COMPLETE\n",
					WLCWLUNIT(wlc)));
				wlc_modesw_oper_mode_complete(wlc->modesw, bsscfg,
					BCME_OK, MODESW_DN_AP_COMPLETE);
			}
		}
	} else
#endif /* WLMCHAN */
	{
		if (pmodesw_bsscfg->action_sendout_counter == 0) {
			WL_MODE_SWITCH(("wl%d: All acks received.... starting downgrade\n",
				WLCWLUNIT(wlc)));
			wlc_modesw_time_measure(wlc->modesw, pmodesw_bsscfg->ctrl_flags,
				MODESW_TM_PM1);
		} else {
			WL_MODE_SWITCH(("wl%d: Acks pending, but downgrading as timer expired\n",
				WLCWLUNIT(wlc)));
		}
		/* For prot obss we drain beforehand */
		if (CTRL_FLAGS_HAS(pmodesw_bsscfg->ctrl_flags,
			MODESW_CTRL_AP_ACT_FRAMES)) {
			wlc_modesw_perform_upgrade_downgrade(wlc->modesw, bsscfg);
		} else {
			/* Non prot obss cases we drain at this point */
			if (TXPKTPENDTOT(wlc) == 0) {
				wlc_modesw_change_state(wlc->modesw, bsscfg,
					MSW_DN_PERFORM);

				wlc_modesw_perform_upgrade_downgrade(wlc->modesw, bsscfg);
			} else {
				wlc_block_datafifo(wlc, DATA_BLOCK_TXCHAIN, DATA_BLOCK_TXCHAIN);
				wlc_modesw_change_state(wlc->modesw, bsscfg,
					MSW_WAIT_DRAIN);
			}
		}
	}
}

static void
wlc_modesw_complete_downgrade_callback(wlc_modesw_info_t *modesw, wlc_bsscfg_t *bsscfg)
{
	wlc_info_t *wlc = (wlc_info_t *)modesw->wlc;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = NULL;

	WL_MODE_SWITCH(("%s wl%d: AP downgrade timer expired...Perform the downgrade of AP.\n",
		__FUNCTION__, WLCWLUNIT(wlc)));

	pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(wlc->modesw, bsscfg);
	if ((BSSCFG_STA(bsscfg) || BSSCFG_PSTA(bsscfg)) &&
		(pmodesw_bsscfg->state == MSW_WAIT_DRAIN)) {
		wlc_modesw_change_state(modesw, bsscfg, MSW_DN_PERFORM);
		wlc_modesw_resume_opmode_change(modesw, bsscfg);
	}
	else if (BSSCFG_AP(bsscfg) &&
		(pmodesw_bsscfg->state == MSW_WAIT_DRAIN)) {
		uint8 new_rxnss, old_rxnss;
		new_rxnss = DOT11_OPER_MODE_RXNSS(pmodesw_bsscfg->oper_mode_new);
		old_rxnss = DOT11_OPER_MODE_RXNSS(pmodesw_bsscfg->oper_mode_old);
		if (CTRL_FLAGS_HAS(pmodesw_bsscfg->ctrl_flags,
			MODESW_CTRL_AP_ACT_FRAMES) && (new_rxnss == old_rxnss)) {
			/* For prot obss cases, we transit from wait_drain
			 * to downgrade_pending after drain
			 */
			wlc_modesw_change_state(modesw, bsscfg,
				MSW_DOWNGRADE_PENDING);
			wlc_modesw_change_ap_oper_mode(modesw, bsscfg,
				pmodesw_bsscfg->oper_mode_new, TRUE);
		} else {
			wlc_modesw_change_state(modesw, bsscfg,
				MSW_DN_PERFORM);
			/* Call actual HW change */
			wlc_modesw_perform_upgrade_downgrade(modesw, bsscfg);
		}
	}
}

/* These functions register/unregister a callback that wlc_modesw_notif_cb_notif may invoke. */
int
wlc_modesw_notif_cb_register(wlc_modesw_info_t *modeswinfo, wlc_modesw_notif_cb_fn_t cb,
            void *arg)
{
	bcm_notif_h hdl = modeswinfo->modesw_notif_hdl;
	return bcm_notif_add_interest(hdl, (bcm_notif_client_callback)cb, arg);
}

int
wlc_modesw_notif_cb_unregister(wlc_modesw_info_t *modeswinfo, wlc_modesw_notif_cb_fn_t cb,
            void *arg)
{
	bcm_notif_h hdl = modeswinfo->modesw_notif_hdl;
	return bcm_notif_remove_interest(hdl, (bcm_notif_client_callback)cb, arg);
}

static void
wlc_modesw_notif_cb_notif(wlc_modesw_info_t *modeswinfo, wlc_bsscfg_t *cfg, int status,
	uint8 oper_mode, uint8 oper_mode_old, int signal)
{
	wlc_modesw_notif_cb_data_t notif_data;
	bcm_notif_h hdl = modeswinfo->modesw_notif_hdl;

	notif_data.cfg = cfg;
	notif_data.opmode = oper_mode;
	notif_data.opmode_old = oper_mode_old;
	notif_data.status = status;
	notif_data.signal = signal;
	bcm_notif_signal(hdl, &notif_data);
	return;
}

void
wlc_modesw_ap_bwchange_notif(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg;
	if (cfg) {
		wlc = cfg->wlc;
	} else {
		/* The request has been cancelled, ignore the Clbk */
		return;
	}
	pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(wlc->modesw, cfg);
	if (pmodesw_bsscfg == NULL) {
		ASSERT(0);
	}
	wlc_modesw_time_measure(wlc->modesw, pmodesw_bsscfg->ctrl_flags,
		MODESW_TM_PHY_COMPLETE);
	if (pmodesw_bsscfg->state == MSW_AP_MSCH_DN_PERFORM) {
		wlc_modesw_change_state(wlc->modesw, cfg, MSW_DN_PERFORM);
		wlc_modesw_notif_cb_notif(wlc->modesw, cfg, BCME_OK,
			pmodesw_bsscfg->oper_mode_new, pmodesw_bsscfg->oper_mode_old,
			MODESW_PHY_DN_COMPLETE);
	} else if (pmodesw_bsscfg->state == MSW_AP_MSCH_UP_PERFORM) {
		wlc_modesw_change_state(wlc->modesw, cfg, MSW_UP_PERFORM);
		if (!CTRL_FLAGS_HAS(pmodesw_bsscfg->ctrl_flags,
				MODESW_CTRL_HANDLE_ALL_CFGS)) {
			wlc_modesw_notif_cb_notif(wlc->modesw, cfg, BCME_OK,
				pmodesw_bsscfg->oper_mode_new,
				pmodesw_bsscfg->oper_mode_old,
				MODESW_PHY_UP_COMPLETE);
		}
	}
	wlc_scb_ratesel_init_bss(wlc, cfg);
	if (PHYMODE(wlc) != PHYMODE_BGDFS) {
		wlc_phy_cal_perical(WLC_PI(wlc), PHY_PERICAL_PHYMODE_SWITCH);
	}
	WL_MODE_SWITCH(("cfg:%d %s msch_cb_handle called\n",
		cfg->_idx, __FUNCTION__));
	wlc_modesw_msch_cb_handle(cfg);
}

void
wlc_modesw_msch_cb_handle(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg;
	modesw_bsscfg_cubby_t *ipmode_cfg;
	wlc_bsscfg_t* icfg;
	uint8 new_rxnss, old_rxnss;
	uint8 new_bw, old_bw, new160, old160;
	int idx;
	if (cfg) {
		wlc = cfg->wlc;
	} else {
		/* XXX: not sure the purpose of here, but keep the original
		 * The request has been cancelled, ignore the Clbk
		 */
		return;
	}
	pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(wlc->modesw, cfg);
	if (pmodesw_bsscfg == NULL) {
		ASSERT(0);
	}
	new_rxnss = DOT11_OPER_MODE_RXNSS(pmodesw_bsscfg->oper_mode_new);
	old_rxnss = DOT11_OPER_MODE_RXNSS(pmodesw_bsscfg->oper_mode_old);
	new_bw = DOT11_OPER_MODE_CHANNEL_WIDTH(pmodesw_bsscfg->oper_mode_new);
	old_bw = DOT11_OPER_MODE_CHANNEL_WIDTH(pmodesw_bsscfg->oper_mode_old);
	new160 = DOT11_OPER_MODE_160_8080(pmodesw_bsscfg->oper_mode_new);
	old160 = DOT11_OPER_MODE_160_8080(pmodesw_bsscfg->oper_mode_old);
	WL_MODE_SWITCH(("cfg:%d afterUpdBW:nc:%x,wlccs:%x,bsscs:%x\n",
		cfg->_idx,
		pmodesw_bsscfg->new_chanspec, wlc->chanspec,
		cfg->current_bss->chanspec));
	/* update oper_mode */
	cfg->oper_mode = wlc_modesw_derive_opermode(wlc->modesw,
		cfg->current_bss->chanspec, cfg,
		wlc->stf->op_rxstreams);
	/* open blocked chains as channel change is done */
	if (wlc->block_datafifo & DATA_BLOCK_TXCHAIN) {
		WLC_BLOCK_DATAFIFO_CLEAR(wlc, DATA_BLOCK_TXCHAIN);
	}
	if (!wlc_modesw_is_downgrade(wlc, cfg, pmodesw_bsscfg->oper_mode_old,
			pmodesw_bsscfg->oper_mode_new)) {
		/* BW upgrade */
		if (pmodesw_bsscfg->resume) {
			if (pmodesw_bsscfg->state == MSW_UP_PERFORM) {
				wlc_modesw_change_state(wlc->modesw,
					cfg, MSW_UP_PM0_WAIT_ACK);
			}
			else {
				wlc_modesw_change_state(wlc->modesw,
					cfg, MSW_DN_PM0_WAIT_ACK);
			}
#ifdef STA
			wlc_modesw_set_pmstate(wlc->modesw, cfg, FALSE);
#endif // endif
			pmodesw_bsscfg->resume = FALSE;
			return;
		}
		wlc_modesw_notif_cb_notif(wlc->modesw, cfg,
			BCME_OK, pmodesw_bsscfg->oper_mode_new,
			pmodesw_bsscfg->oper_mode_old, MODESW_PHY_UP_COMPLETE);

		if (!CTRL_FLAGS_HAS(pmodesw_bsscfg->ctrl_flags,
				(MODESW_CTRL_DN_SILENT_DNGRADE |
				MODESW_CTRL_UP_SILENT_UPGRADE))) {
			wlc_modesw_ap_upd_bcn_act(wlc->modesw, cfg, pmodesw_bsscfg->state);
		}
		wlc_modesw_time_measure(wlc->modesw, pmodesw_bsscfg->ctrl_flags,
			MODESW_TM_PM0);

		if ((old_rxnss != new_rxnss) || (new_bw != old_bw) || (new160 != old160)) {
			if (old_rxnss != new_rxnss) {
				WL_MODE_SWITCH(("cfg:%d RXNSS change\n", cfg->_idx));
			} else if (new_bw != old_bw) {
				WL_MODE_SWITCH(("cfg:%d BW_SWITCH\n", cfg->_idx));
			} else if (new160 != old160) {
				WL_MODE_SWITCH(("cfg:%d DYN_160\n", cfg->_idx));
			}
			WL_MODE_SWITCH(("cfg:%d nc:%x,wlccs:%x,bsscs:%x\n",
				cfg->_idx,
				pmodesw_bsscfg->new_chanspec, wlc->chanspec,
				cfg->current_bss->chanspec));
			wlc_modesw_oper_mode_complete(wlc->modesw, cfg,
				BCME_OK, MODESW_UP_AP_COMPLETE);
		}
#ifdef WL_AIR_IQ
		/* oper_mode_enable must be explicitly disabled since 2.4GHz
		 * oper mode will be 0 after upgade case.
		 */
		if (CHSPEC_IS2G(pmodesw_bsscfg->new_chanspec)) {
			cfg->oper_mode_enabled = FALSE;
		}
#endif /* WL_AIR_IQ */
		if (pmodesw_bsscfg->state == MSW_UPGRADE_PENDING) {
			wlc_modesw_change_state(wlc->modesw, cfg, MSW_NOT_PENDING);
		}
	} else {
		/* BW downgrade */
		if (pmodesw_bsscfg->resume) {
			if (pmodesw_bsscfg->state == MSW_UP_PERFORM) {
				wlc_modesw_change_state(wlc->modesw,
					cfg, MSW_UP_PM0_WAIT_ACK);
			}
			else {
				wlc_modesw_change_state(wlc->modesw,
					cfg, MSW_DN_PM0_WAIT_ACK);
			}
#ifdef STA
			wlc_modesw_set_pmstate(wlc->modesw, cfg, FALSE);
#endif // endif
			pmodesw_bsscfg->resume = FALSE;
			return;
		}
		if (CTRL_FLAGS_HAS(pmodesw_bsscfg->ctrl_flags, (MODESW_CTRL_DN_SILENT_DNGRADE))) {
			wlc_modesw_notif_cb_notif(wlc->modesw, cfg, BCME_OK,
				cfg->oper_mode, pmodesw_bsscfg->oper_mode_old,
				MODESW_DN_AP_COMPLETE);
			wlc_modesw_change_state(wlc->modesw, cfg, MSW_NOT_PENDING);
			wlc_modesw_time_measure(wlc->modesw, pmodesw_bsscfg->ctrl_flags,
				MODESW_TM_PM0);
			if (pmodesw_bsscfg->state == MSW_UPGRADE_PENDING) {
				wlc_modesw_change_state(wlc->modesw, cfg, MSW_NOT_PENDING);
			}
		} else {
			/* Update beacons and probe responses to reflect the update in bandwidth
			*/
			if (!CTRL_FLAGS_HAS(pmodesw_bsscfg->ctrl_flags,
					(MODESW_CTRL_DN_SILENT_DNGRADE |
					MODESW_CTRL_UP_SILENT_UPGRADE))) {
				wlc_bss_update_beacon(wlc, cfg);
				wlc_bss_update_probe_resp(wlc, cfg, TRUE);
			}

			wlc_modesw_change_state(wlc->modesw, cfg, MSW_NOT_PENDING);
			WL_MODE_SWITCH(("\nwl%d: Sending signal = MODESW_DN_AP_COMPLETE\n",
				WLCWLUNIT(wlc)));
			wlc_modesw_notif_cb_notif(wlc->modesw, cfg, BCME_OK,
				cfg->oper_mode, pmodesw_bsscfg->oper_mode_old,
				MODESW_DN_AP_COMPLETE);
			WL_MODE_SWITCH(("cfg:%d  MODESW downgrade nc:%x,wlccs:%x,bsscs:%x\n",
					cfg->_idx,
					pmodesw_bsscfg->new_chanspec, wlc->chanspec,
					cfg->current_bss->chanspec));
			if (wlc_modesw_oper_mode_complete(wlc->modesw, cfg,
					BCME_OK, MODESW_DN_AP_COMPLETE) == BCME_NOTREADY) {
				WL_MODE_SWITCH(("wl%d: Resume others (cfg %d)\n",
					WLCWLUNIT(wlc), cfg->_idx));
				FOREACH_AS_BSS(wlc, idx, icfg) {
					ipmode_cfg = MODESW_BSSCFG_CUBBY(
						wlc->modesw, icfg);
					if ((icfg != cfg) && (ipmode_cfg->state ==
							MSW_DN_DEFERRED)) {
						ipmode_cfg->state = MSW_DN_PERFORM;
						wlc_modesw_resume_opmode_change(
							wlc->modesw, icfg);
					}
				}
				if (CTRL_FLAGS_HAS(pmodesw_bsscfg->ctrl_flags,
						MODESW_CTRL_HANDLE_ALL_CFGS)) {
					/* This flag signifies that a per bsscfg
					 * callback is needed. eg:for Dynamic
					 * BWSW we need a callback for every
					 * bsscfg to indicate that PHY UP is
					 * done.
					 */
					wlc_modesw_notif_cb_notif(wlc->modesw, cfg,
						BCME_OK, cfg->oper_mode,
						pmodesw_bsscfg->oper_mode_old,
						MODESW_DN_AP_COMPLETE);
				}
			}

			wlc_modesw_time_measure(wlc->modesw, pmodesw_bsscfg->ctrl_flags,
				MODESW_TM_PM0);
		}
	}
}

/* generate WLC_E_MODE_SWITCH event based on arguments passed */
static void
wlc_modesw_send_event(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *cfg,
		uint16 opmode_from, uint16 opmode_to, uint32 event_flags, uint16 event_reason,
		uint8 * event_data, uint16 event_data_len)
{
	wlc_info_t *wlc = modesw_info->wlc;
	wl_event_mode_switch_t *pevent;
	uint16 event_len = sizeof(*pevent) + event_data_len;
	BCM_REFERENCE(cfg);

	if (!(pevent = (wl_event_mode_switch_t *) MALLOCZ(wlc->osh, event_len))) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return;
	}
	pevent->version = WL_EVENT_MODESW_VER_1;
	pevent->length = event_len;
	pevent->opmode_from = opmode_from;
	pevent->opmode_to = opmode_to;
	pevent->flags = event_flags;
	pevent->reason = event_reason;
	pevent->data_offset = sizeof(*pevent);

	if (event_data_len > 0 && event_data != NULL) {
		memcpy(pevent->data, event_data, event_data_len);
	}

	wlc_bss_mac_event(wlc, wlc->primary_bsscfg, WLC_E_MODE_SWITCH, NULL, 0,
			0, 0, (void *)pevent, WL_E_MODESW_SIZE(pevent));

	WL_MODE_SWITCH(("wl%d: WLC_E_MODE_SWITCH ver:%u, len:%u, op_from:0x%03X, op_to:0x%03X, "
			"flags:0x%08X, reason:%u, data_offset:%u, data_len:%u\n", WLCWLUNIT(wlc),
			pevent->version, pevent->length, pevent->opmode_from, pevent->opmode_to,
			pevent->flags, pevent->reason, pevent->data_offset, event_data_len));

	MFREE(wlc->osh, pevent, event_len);
}

/* TODO:6GHZ: does wlc_modesw_is_connection_he() have to be introduced ? */

bool
wlc_modesw_is_connection_vht(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	struct scb *scb = NULL;
	if ((BSSCFG_IBSS(bsscfg)) || (BSSCFG_AP(bsscfg))) {
		/* AP or IBSS case */
		return (MODESW_BSS_VHT_ENAB(wlc, bsscfg)) ? TRUE : FALSE;
	} else {
		/* non ibss case */
		scb = wlc_scbfindband(wlc, bsscfg, &bsscfg->BSSID,
			CHSPEC_BANDUNIT(bsscfg->current_bss->chanspec));
		if (scb && SCB_VHT_CAP(scb))
			return TRUE;
		return FALSE;
	}
}

bool
wlc_modesw_is_connection_ht(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	struct scb *scb = NULL;
	if (BSSCFG_IBSS(bsscfg) || (BSSCFG_AP(bsscfg))) {
		/* AP or IBSS case */
		return (BSS_N_ENAB(wlc, bsscfg)) ? TRUE : FALSE;
	} else {
		/* non ibss case */
		scb = wlc_scbfindband(wlc, bsscfg, &bsscfg->BSSID,
			CHSPEC_BANDUNIT(bsscfg->current_bss->chanspec));
		if (scb && SCB_HT_CAP(scb))
			return TRUE;
		if (scb && SCB_VHT_CAP(scb))
			return TRUE;
		return FALSE;
	}
}

/* Interface Function to set Opermode Channel Spec Variable */
void
wlc_modesw_set_max_chanspec(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *cfg,
	chanspec_t chanspec)
{
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, cfg);

	pmodesw_bsscfg->max_opermode_chanspec = chanspec;
}

bool
wlc_modesw_is_req_valid(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *cfg)
{
	bool valid_req = FALSE;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, cfg);
	if (pmodesw_bsscfg->state == MSW_NOT_PENDING)
		valid_req = TRUE;
	/* return  the TRUE/ FALSE */
	else
		valid_req = FALSE;

	return valid_req;
}

/* This function is to check if we can disable OMN. Once oper_mode is enabled,
 * we can disable it if we already operating in MAX NSS and BW.
 *
 * Returns TRUE if OMN can be disbaled.
 */
static bool
wlc_modesw_opmode_be_disabled(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(wlc->modesw, cfg);
	chanspec_t max_oper_chspec = pmodesw_bsscfg->max_opermode_chanspec;
	chanspec_t cur_chspec = cfg->current_bss->chanspec;
	uint8 max_nss, cur_nss = DOT11_OPER_MODE_RXNSS(cfg->oper_mode);
	uint16 cap_mcsmap, bw_cap = wlc->band->bw_cap;
	bool ch160_halfnss = (CHSPEC_IS160(max_oper_chspec) || WL_BW_CAP_160MHZ(bw_cap)) &&
			WLC_PHY_160_HALF_NSS(wlc);

	cap_mcsmap = wlc_vht_get_rx_mcsmap(wlc->vhti);
	max_nss = VHT_MAX_SS_SUPPORTED(cap_mcsmap);

	/* Unless operating at 160Mhz with Half NSS, disable OMN */
	if (!ch160_halfnss && (max_nss == cur_nss) &&
		CHSPEC_BW(max_oper_chspec) == CHSPEC_BW(cur_chspec) &&
		(pmodesw_bsscfg->state == MSW_NOT_PENDING)) {
#ifdef BCMDBG
		WL_MODE_SWITCH(("wl%d: max_oper_chspec 0x%x cur_chspec 0x%x cap_mcsmap 0x%x "
				"cur_nss %d max nss %d ch160_halfnss %d\n", WLCWLUNIT(wlc),
				max_oper_chspec, cur_chspec, cap_mcsmap, cur_nss, max_nss,
				ch160_halfnss));
#endif /* BCMDBG */
		return TRUE;
	}
	return FALSE;
}

/* Handles the TBTT interrupt for AP. Required to process
 * the AP downgrade procedure, which is waiting for DTIM
 * beacon to go out before perform actual downgrade of link
 */
void
wlc_modesw_bss_tbtt(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = modesw_info->wlc;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, cfg);
	BCM_REFERENCE(wlc);
	if (!BSSCFG_AP(cfg)) {
		return;
	}
	if (cfg->oper_mode_enabled && pmodesw_bsscfg &&
		(pmodesw_bsscfg->state == MSW_AP_DN_WAIT_DTIM) &&
		!CTRL_FLAGS_HAS(pmodesw_bsscfg->ctrl_flags, MODESW_CTRL_DN_SILENT_DNGRADE))
	{
		/* sanity checks */
		if (wlc_modesw_alloc_context(modesw_info, cfg) != BCME_OK) {
			goto fail;
		}

		/* AP downgrade is pending */
		/* If it is the TBTT for DTIM then start the non-periodic timer */
#ifdef AP
		if (!wlc_ap_getdtim_count(wlc, cfg)) {
			ASSERT(pmodesw_bsscfg->oper_mode_timer != NULL);
#ifdef WLMCHAN
			if (MCHAN_ACTIVE(wlc->pub) && (!wlc_ap_count(wlc->ap, FALSE))) {
				wlc_modesw_change_state(modesw_info, cfg, MSW_DN_PERFORM);
			} else
#endif /* WLMCHAN */
			{
				WL_MODE_SWITCH(("TBTT Timer set....\n"));
				WL_MODE_SWITCH(("DTIM Tbtt..Start timer for 5 msec \n"));
				ASSERT(pmodesw_bsscfg->timer_ctx->notif == 0);
				pmodesw_bsscfg->timer_ctx->notif = MODESW_TIMER_AP_BACKOFF_EXP;
				if (pmodesw_bsscfg->oper_mode_timer != NULL) {
					wl_add_timer(wlc->wl, pmodesw_bsscfg->oper_mode_timer,
						MODESW_AP_DOWNGRADE_BACKOFF, FALSE);
				}
			}
		}
#endif /* AP */
		return;
	}
	/* This is to stop publishing OMN's after a few DTIMs on upgrade. After BGDFS NSS upgrade,
	 * OMN's will be published only for few DTIM's.
	 */
	if (cfg->oper_mode_enabled && wlc_modesw_opmode_be_disabled(wlc, cfg)) {
		uint32 val;

		wlc_bmac_copyfrom_objmem(wlc->hw, (S_DOT11_DTIMCOUNT << 2),
				&val, sizeof(val), OBJADDR_SCR_SEL);
		if (!val) {
			if (!(pmodesw_bsscfg->opmode_disable_dtim_cnt--)) {
				cfg->oper_mode_enabled = FALSE;
				pmodesw_bsscfg->opmode_disable_dtim_cnt = MODESW_DISABLE_OMN_DTIMS;
				wlc_bss_update_beacon(wlc, cfg);
				wlc_bss_update_probe_resp(wlc, cfg, TRUE);
				WL_MODE_SWITCH(("wl%d: OMN disabled after %d DTIMs\n",
						WLCWLUNIT(wlc),	MODESW_DISABLE_OMN_DTIMS));
			}
		}
	}
	return;
fail:
	/* Downgrade can not be completed. Change the mode switch state
	 * so that a next attempt could be tried by upper layer
	 */
	wlc_modesw_change_state(modesw_info, cfg, MSW_NOT_PENDING);
	return;
}

/* This function changes the state of the cfg to new_state */
void
wlc_modesw_change_state(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *cfg, int8 new_state)
{
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = NULL;
	pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, cfg);
	if (!pmodesw_bsscfg)
		return;
	if (pmodesw_bsscfg->state != new_state) {
		WL_MODE_SWITCH(("wl%d: cfg = %d MODESW STATE Transition: %s -> %s\n",
			WLCWLUNIT(modesw_info->wlc), cfg->_idx,
			opmode_st_names[pmodesw_bsscfg->state].name,
			opmode_st_names[new_state].name));
		pmodesw_bsscfg->state = new_state;
		/* Take new timestamp for new state transition. */
		pmodesw_bsscfg->start_time = OSL_SYSUPTIME();
	}
	return;
}

/* This function allocates the opmode context which is retained till
 * the opmode is disabled
 */
static int
wlc_modesw_alloc_context(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = modesw_info->wlc;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, cfg);
	if (pmodesw_bsscfg->timer_ctx == NULL) {
		/* Modesw context is not initialized */
		if (!(pmodesw_bsscfg->timer_ctx =
			(wlc_modesw_cb_ctx_t *)MALLOCZ(wlc->osh,
			sizeof(wlc_modesw_cb_ctx_t)))) {
			WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			return BCME_ERROR;
		}
		pmodesw_bsscfg->timer_ctx->connID = cfg->ID;
		pmodesw_bsscfg->timer_ctx->modesw_info = modesw_info;
		pmodesw_bsscfg->timer_ctx->cfg = cfg;

		WL_MODE_SWITCH(("Initializing timer ...alloc mem\n"));
		/* Initialize the timer */
		pmodesw_bsscfg->oper_mode_timer = wl_init_timer(wlc->wl,
			wlc_modesw_oper_mode_timer, pmodesw_bsscfg->timer_ctx,
			"opermode_dgrade_timer");

		if (pmodesw_bsscfg->oper_mode_timer == NULL)
		{
			WL_ERROR(("Timer alloc failed\n"));
			MFREE(wlc->osh, pmodesw_bsscfg->timer_ctx,
					sizeof(wlc_modesw_cb_ctx_t));
			return BCME_ERROR;
		}
	}
#ifdef WLCSA
	if (modesw_info->modesw_csa_cb_regd == FALSE) {
		if (wlc->csa) {
			if (wlc_csa_cb_register(wlc->csa,
				wlc_modesw_csa_cb, modesw_info) != BCME_OK) {
				WL_ERROR(("wl%d: %s csa notif callbk failed, but continuing\n",
					wlc->pub->unit, __FUNCTION__));
				ASSERT(0);
			} else {
				modesw_info->modesw_csa_cb_regd = TRUE;
			}
		}
	}
#endif /* WLCSA */
	return BCME_OK;
}

/* This function de-allocates the opmode context when the opmode
 * is disabled
 */
static void
wlc_modesw_dealloc_context(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = modesw_info->wlc;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, cfg);
	if (pmodesw_bsscfg != NULL) {
		if (pmodesw_bsscfg->oper_mode_timer != NULL) {
			wl_del_timer(wlc->wl, pmodesw_bsscfg->oper_mode_timer);
			wl_free_timer(wlc->wl, pmodesw_bsscfg->oper_mode_timer);
			pmodesw_bsscfg->oper_mode_timer = NULL;
		}
		if (pmodesw_bsscfg->timer_ctx != NULL) {
			MFREE(wlc->osh, pmodesw_bsscfg->timer_ctx,
				sizeof(wlc_modesw_cb_ctx_t));
			pmodesw_bsscfg->timer_ctx = NULL;
		}
	}
#ifdef WLCSA
	if (modesw_info->modesw_csa_cb_regd == TRUE) {
		if (wlc->csa) {
			if (wlc_csa_cb_unregister(wlc->csa,
				wlc_modesw_csa_cb, modesw_info) != BCME_OK) {
				WL_ERROR(("wl%d: %s: wlc_csa_notif_cb_unregister() failed\n",
					wlc->pub->unit, __FUNCTION__));
			} else {
				modesw_info->modesw_csa_cb_regd = FALSE;
			}
		}
	}
#endif /* WLCSA */

}

static void
wlc_modesw_perform_upgrade_downgrade(wlc_modesw_info_t *modesw_info,
	wlc_bsscfg_t * bsscfg)
{
	wlc_info_t *wlc = modesw_info->wlc;
	uint8 rxnss, old_rxnss, chains = 0, i;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = NULL;
	bool nss_change;
	int err = 0;

	ASSERT(modesw_info);
	ASSERT(bsscfg);
	pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, bsscfg);
	ASSERT(pmodesw_bsscfg);
	ASSERT((pmodesw_bsscfg->state != MSW_NOT_PENDING));
	rxnss = DOT11_OPER_MODE_RXNSS(pmodesw_bsscfg->oper_mode_new);
	old_rxnss = DOT11_OPER_MODE_RXNSS(pmodesw_bsscfg->oper_mode_old);
	nss_change = (rxnss != old_rxnss) ? TRUE:FALSE;

	if (CTRL_FLAGS_HAS(pmodesw_bsscfg->ctrl_flags,
		MODESW_CTRL_RSDB_MOVE)) {
		WL_MODE_SWITCH(("wl%d: Invalid case, Upgrade/Downgrade for Dual MAC RSDB\n",
			WLCWLUNIT(wlc)));
		ASSERT(0);
		return;
	}

	WL_MODE_SWITCH(("wl%d: cfg-%d: new_chspec %x old_chspec %x wlc_chspec %x"
		" new_rxnss %x old_rxnss %x\n",
		WLCWLUNIT(wlc),
		bsscfg->_idx,
		pmodesw_bsscfg->new_chanspec,
		bsscfg->current_bss->chanspec,
		wlc->chanspec,
		rxnss,
		wlc->stf->op_rxstreams));

	/* Invalidate the ctx before cals are triggered */
	wlc_phy_invalidate_chanctx(WLC_PI(wlc), bsscfg->current_bss->chanspec);

	if (pmodesw_bsscfg->state == MSW_UP_PERFORM) {
		wlc_modesw_notif_cb_notif(modesw_info, bsscfg, BCME_OK,
				pmodesw_bsscfg->oper_mode_new, pmodesw_bsscfg->oper_mode_old,
				MODESW_PHY_UP_START);
	}

	/* Perform the NSS change if there is any change required in no of Rx streams */
	/* Perform the NSS change if there is any change required in no of Rx streams */
	if (nss_change) {
		/* Construct the Rx/Tx chains based on Rx streams */
		for (i = 0; i < rxnss; i++) {
			chains |= (1 << i);
		}
		/* Perform the change of Tx chain first, as it waits for
		 * all the Tx packets to flush out. Change of Rx chain
		 * involves sending out the MIMOOPS action frame, which
		 * can cause postponement of the Tx chain changes
		 * due to pending action frame in Tx FIFO.
		 */
		err = wlc_stf_set_optxrxstreams(wlc, rxnss);
		if (err != BCME_OK) {
			WL_ERROR(("wl%d: %d error in setting Op RxTxchains %d\n",
				WLCWLUNIT(wlc), err, rxnss));
			return;
		}
		/* update HT mcs rates based on number of chains */
		/* we need to take care of wlc_rate_init for every scb here */
		wlc_scb_ratesel_init_all(wlc);
		wlc_modesw_time_measure(wlc->modesw, pmodesw_bsscfg->ctrl_flags,
			MODESW_TM_PHY_COMPLETE);
		if (pmodesw_bsscfg->state == MSW_DN_PERFORM) {
			wlc_modesw_notif_cb_notif(modesw_info, bsscfg, BCME_OK,
				pmodesw_bsscfg->oper_mode_new,
				pmodesw_bsscfg->oper_mode_old,
				MODESW_PHY_DN_COMPLETE);
		}

		if (!CTRL_FLAGS_HAS(pmodesw_bsscfg->ctrl_flags,
				MODESW_CTRL_HANDLE_ALL_CFGS)) {
			if (pmodesw_bsscfg->state == MSW_UP_PERFORM) {
				wlc_modesw_notif_cb_notif(modesw_info, bsscfg, BCME_OK,
					pmodesw_bsscfg->oper_mode_new,
					pmodesw_bsscfg->oper_mode_old,
					MODESW_PHY_UP_COMPLETE);
			}
		}
		wlc_scb_ratesel_init_bss(wlc, bsscfg);
		if (PHYMODE(wlc) != PHYMODE_BGDFS) {
			wlc_phy_cal_perical(WLC_PI(wlc), PHY_PERICAL_PHYMODE_SWITCH);
		}
		WL_MODE_SWITCH(("cfg:%d %s msch_cb_handle called\n",
			bsscfg->_idx, __FUNCTION__));
		wlc_modesw_msch_cb_handle(bsscfg);
	} else if (BSSCFG_AP(bsscfg)) {
		/* Perform the bandwidth switch if the current chanspec
		 * is not matching with requested chanspec in the operating
		 * mode notification
		 */
		int idx;
		wlc_bsscfg_t* icfg;
		modesw_bsscfg_cubby_t *ipmode_cfg = NULL;
		bsscfg->flags2 |= WLC_BSSCFG_FL2_MODESW_BWSW;
		wlc_ap_timeslot_unregister(bsscfg);
		bsscfg->target_bss->chanspec = pmodesw_bsscfg->new_chanspec;
		bsscfg->flags2 &= ~WLC_BSSCFG_FL2_MODESW_BWSW;

		if (pmodesw_bsscfg->state == MSW_AP_DN_WAIT_DTIM) {
			wlc_modesw_change_state(modesw_info, bsscfg, MSW_AP_MSCH_DN_DEFFERRED);
			FOREACH_AS_BSS(wlc, idx, icfg) {
				if (bsscfg != icfg) {
					ipmode_cfg = MODESW_BSSCFG_CUBBY(modesw_info, icfg);
					if (BSSCFG_AP(icfg)) {
						if (ipmode_cfg->state != MSW_AP_MSCH_DN_DEFFERRED) {
							return;
						}
					}
				}
			}
		} else if (pmodesw_bsscfg->state == MSW_UP_PERFORM) {
			wlc_modesw_change_state(modesw_info, bsscfg, MSW_AP_MSCH_UP_DEFFERRED);
			FOREACH_AS_BSS(wlc, idx, icfg) {
				if (bsscfg != icfg) {
					ipmode_cfg = MODESW_BSSCFG_CUBBY(modesw_info, icfg);
					if (BSSCFG_AP(icfg)) {
						if (ipmode_cfg->state != MSW_AP_MSCH_UP_DEFFERRED) {
							return;
						}
					}
				}
			}
		}
		/* states synchronised  across different bsscfgs */
		WL_MODE_SWITCH(("wl%d.%d %s: Registering all cfg's with MSCH for new chanspec\n",
			WLCWLUNIT(wlc), WLC_BSSCFG_IDX(bsscfg), __FUNCTION__));
		FOREACH_AS_BSS(wlc, idx, icfg) {
			if (BSSCFG_AP(icfg)) {
				if (pmodesw_bsscfg->state == MSW_AP_MSCH_DN_DEFFERRED) {
					wlc_modesw_change_state(modesw_info, icfg,
						MSW_AP_MSCH_DN_PERFORM);
				} else if (pmodesw_bsscfg->state == MSW_AP_MSCH_UP_DEFFERRED) {
					wlc_modesw_change_state(modesw_info, icfg,
						MSW_AP_MSCH_UP_PERFORM);
				}
				wlc_ap_timeslot_register(icfg);
			}
		}
	} else {
		/* handles non-NSS and non-AP BW changes --like STA etc */
		/* For STA, perform the bandwidth switch if the current chanspec
		 * is not matching with requested chanspec in the operating
		 * mode notification
		 */
		bsscfg->flags2 |= WLC_BSSCFG_FL2_MODESW_BWSW;
		wlc_update_bandwidth(wlc, bsscfg, pmodesw_bsscfg->new_chanspec);
		bsscfg->flags2 &= ~WLC_BSSCFG_FL2_MODESW_BWSW;
		wlc_modesw_time_measure(wlc->modesw, pmodesw_bsscfg->ctrl_flags,
			MODESW_TM_PHY_COMPLETE);
		if (pmodesw_bsscfg->state == MSW_DN_PERFORM) {
			wlc_modesw_notif_cb_notif(modesw_info, bsscfg, BCME_OK,
				pmodesw_bsscfg->oper_mode_new,
				pmodesw_bsscfg->oper_mode_old,
				MODESW_PHY_DN_COMPLETE);
		}

		if (!CTRL_FLAGS_HAS(pmodesw_bsscfg->ctrl_flags,
				MODESW_CTRL_HANDLE_ALL_CFGS)) {
			if (pmodesw_bsscfg->state == MSW_UP_PERFORM) {
				wlc_modesw_notif_cb_notif(modesw_info, bsscfg, BCME_OK,
					pmodesw_bsscfg->oper_mode_new,
					pmodesw_bsscfg->oper_mode_old,
					MODESW_PHY_UP_COMPLETE);
			}
		}
		wlc_scb_ratesel_init_bss(wlc, bsscfg);
		WL_MODE_SWITCH(("cfg:%d %s msch_cb_handle called\n",
			bsscfg->_idx, __FUNCTION__));
		wlc_modesw_msch_cb_handle(bsscfg);
	}
}

/* Updates beacon and probe response with New bandwidth
* Sents action frames only during normal Upgrade
*/
static void
wlc_modesw_ap_upd_bcn_act(wlc_modesw_info_t *modesw_info,
wlc_bsscfg_t *bsscfg, uint8 state)
{
	wlc_info_t *wlc = modesw_info->wlc;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, bsscfg);
	ASSERT(BSSCFG_AP(bsscfg) || BSSCFG_IBSS(bsscfg));

	if (CTRL_FLAGS_HAS(pmodesw_bsscfg->ctrl_flags, MODESW_CTRL_AP_ACT_FRAMES)) {
		wlc_modesw_ap_send_action_frames(wlc->modesw, bsscfg);
	} else if (pmodesw_bsscfg->state == MSW_DOWNGRADE_PENDING) {
		wlc_modesw_change_state(modesw_info, bsscfg, MSW_AP_DN_WAIT_DTIM);
	}

	wlc_bss_update_beacon(wlc, bsscfg);
	wlc_bss_update_probe_resp(wlc, bsscfg, TRUE);
}

/* This function resumes the STA/AP downgrade for each bsscfg.
 * This is required if the downgrade was postponed due to pending
 * packets in the Tx FIFO
 */
void
wlc_modesw_bsscfg_complete_downgrade(wlc_modesw_info_t *modesw_info)
{
	wlc_info_t *wlc = modesw_info->wlc;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg;
	wlc_bsscfg_t *icfg;
	uint8 i = 0;
	FOREACH_AS_BSS(wlc, i, icfg) {
		pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, icfg);
		if (pmodesw_bsscfg == NULL)
			return;
		if (pmodesw_bsscfg->state != MSW_NOT_PENDING) {
			/* DOWNGRADE timer is already assigned and running
			 * for this cfg. So proceed with next cfg.
			 */
			if (pmodesw_bsscfg->timer_ctx->notif
				== MODESW_TIMER_COMPLETE_DOWNGRADE) {
				continue;
			}
			ASSERT(pmodesw_bsscfg->timer_ctx->notif == 0);
			pmodesw_bsscfg->timer_ctx->notif = MODESW_TIMER_COMPLETE_DOWNGRADE;

			/* block the data fifo till the downgrade is complete */
			wlc_block_datafifo(wlc, DATA_BLOCK_TXCHAIN, DATA_BLOCK_TXCHAIN);

			wl_add_timer(wlc->wl, pmodesw_bsscfg->oper_mode_timer, 0, FALSE);
		}
	}
}

/* In MBSS case, during BW downgrade, this function is called by the Nth CFG */
/* This also invokes the registerd callbacks via timer */
/* For BW upgrade, each CFG will individually call this function and also */
/* invoke the callback directly rather than via timer. */
static int
wlc_modesw_oper_mode_complete(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t* cfg, int status,
	int signal)
{
	wlc_info_t *wlc = modesw_info->wlc;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg, *ipmode_cfg;
	wlc_bsscfg_t* icfg;
	int idx;

	pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, cfg);

	/* Stop the expiry timer */
	WL_MODE_SWITCH(("wl%d: Called opmode complete for cfg = %d status = %d signal = %d\n",
		WLCWLUNIT(wlc), cfg->_idx, status, signal));

	if (pmodesw_bsscfg->oper_mode_timer != NULL) {
		wl_del_timer(wlc->wl, pmodesw_bsscfg->oper_mode_timer);
	}

	/* Derive oper_mode only during bw downgrade */
	if (signal == MODESW_DN_AP_COMPLETE) {
		if (cfg->associated || cfg->up) {
			cfg->oper_mode = wlc_modesw_derive_opermode(modesw_info,
					cfg->current_bss->chanspec, cfg, wlc->stf->op_rxstreams);
		}
	}

	if (status) {
		WL_MODE_SWITCH(("wl%d:%d Opmode change failed..\n",
			WLCWLUNIT(wlc), cfg->_idx));
	}

	/* Dont bother about other cfg's in MCHAN active mode */
#ifdef WLMCHAN
	if (MCHAN_ACTIVE(wlc->pub) && (!wlc_ap_count(wlc->ap, FALSE))) {
		if (status) {
			wlc_modesw_change_state(modesw_info, cfg, MSW_FAILED);
		}
		else {
			wlc_modesw_change_state(modesw_info, cfg, MSW_NOT_PENDING);
		}
		wlc_user_wake_upd(wlc, WLC_USER_WAKE_REQ_VHT, FALSE);
		/* Call the registered callbacks */
		wlc_modesw_notif_cb_notif(modesw_info, cfg, status, cfg->oper_mode,
			pmodesw_bsscfg->oper_mode_old, signal);
		return BCME_OK;
	}
#endif /* WLMCHAN */
	WL_MODE_SWITCH(("wl%d: Opmode change complete for cfg %d...Check Others\n",
		WLCWLUNIT(wlc), cfg->_idx));

	FOREACH_AS_BSS(wlc, idx, icfg) {
		ipmode_cfg = MODESW_BSSCFG_CUBBY(modesw_info, icfg);
		if ((ipmode_cfg->state != MSW_NOT_PENDING) &&
			(ipmode_cfg->state != MSW_FAILED) &&
			(icfg != cfg)) {
			WL_MODE_SWITCH(("wl%d: Opmode change pending for cfg = %d..\n",
				WLCWLUNIT(wlc), icfg->_idx));
			if (status) {
				wlc_modesw_change_state(modesw_info, cfg, MSW_FAILED);
			}
			else {
				wlc_modesw_change_state(modesw_info, cfg, MSW_NOT_PENDING);
			}
			pmodesw_bsscfg->start_time = 0;
			return BCME_NOTREADY;
		}
	}
	WL_MODE_SWITCH(("wl%d: ALL cfgs are DONE (says cfg %d)\n",
		WLCWLUNIT(wlc), cfg->_idx));

	WL_MODE_SWITCH(("wl%d: Complete opmode change for cfg %d\n",
		WLCWLUNIT(wlc), cfg->_idx));
	wlc_user_wake_upd(wlc, WLC_USER_WAKE_REQ_VHT, FALSE);
	/* Update the modesw structure */
	pmodesw_bsscfg->timer_ctx->status = status;
	pmodesw_bsscfg->timer_ctx->oper_mode = cfg->oper_mode;
	pmodesw_bsscfg->timer_ctx->signal = signal;
	/* Unblock data fifo */
	if (wlc->block_datafifo & DATA_BLOCK_TXCHAIN) {
		WLC_BLOCK_DATAFIFO_CLEAR(wlc, DATA_BLOCK_TXCHAIN);
	}

	if (status) {
		wlc_modesw_change_state(wlc->modesw, cfg, MSW_FAILED);
	}
	else {
		wlc_modesw_change_state(wlc->modesw, cfg, MSW_NOT_PENDING);
	}
#ifdef STA
	/* restore PM mode */
	if (!BSSCFG_AP(cfg) && !BSSCFG_IBSS(cfg)) {
		bool pm = cfg->pm->PM;
		cfg->pm->PM = 0;
		wlc_set_pm_mode(wlc, pm, cfg);
	}
#endif /* STA */
	if (CTRL_FLAGS_HAS(pmodesw_bsscfg->ctrl_flags,
			MODESW_CTRL_HANDLE_ALL_CFGS)) {
		FOREACH_AS_BSS(wlc, idx, icfg) {
			/* Call the registered callbacks */
			wlc_modesw_ctrl_hdl_all_cfgs(modesw_info, icfg,
					signal);
		}
	} else {
		wlc_modesw_notif_cb_notif(modesw_info, cfg, status,
			cfg->oper_mode, pmodesw_bsscfg->oper_mode_old, signal);

	}
	return BCME_OK;
}

/* Handle the different states of the operating mode processing. */
void
wlc_modesw_pm_pending_complete(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg, *ipmode_cfg;
	wlc_bsscfg_t *icfg;
	int idx;
	if (!cfg || !modesw_info)
		return;

	wlc =  modesw_info->wlc;
	pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, cfg);

	/* Make sure that we have oper mode change pending for this cfg.
	* It may happen that we clear the oper mode context for this cfg
	* due to timeout.
	*/
	if (!pmodesw_bsscfg || (pmodesw_bsscfg->state == MSW_NOT_PENDING) ||
		(pmodesw_bsscfg->state == MSW_FAILED)) {
		return;
	}

	switch (pmodesw_bsscfg->state)
	{
		case MSW_UP_PM1_WAIT_ACK:
			{
				WL_MODE_SWITCH(("wl%d: Got the NULL ack for PM 1 for cfg = %d."
					"Start actualupgrade\n", WLCWLUNIT(wlc), cfg->_idx));
				wlc_modesw_time_measure(wlc->modesw, pmodesw_bsscfg->ctrl_flags,
					MODESW_TM_PM1);
				wlc_modesw_change_state(modesw_info, cfg, MSW_UP_PERFORM);
				wlc_modesw_resume_opmode_change(modesw_info, cfg);
				wlc_modesw_time_measure(modesw_info, pmodesw_bsscfg->ctrl_flags,
					MODESW_TM_PHY_COMPLETE);
			}
			break;
		case MSW_UP_PM0_WAIT_ACK:
			{
				uint8 opermode_bw;
				WL_MODE_SWITCH(("wl%d: Got NULL ack for PM 0 for cfg = %d.."
					"Send action frame\n", WLCWLUNIT(wlc), cfg->_idx));
				opermode_bw = pmodesw_bsscfg->oper_mode_new;

				/* if pseudo state indicates pending, dont send action frame */
				if (CTRL_FLAGS_HAS(pmodesw_bsscfg->ctrl_flags,
						MODESW_CTRL_UP_SILENT_UPGRADE)) {
					/* donot send action frame., and stop the SM here */
					WL_MODE_SWITCH(("MODESW_CTRL_UP_SILENT_UPGRADE\n"));
					wlc_modesw_change_state(modesw_info, cfg, MSW_NOT_PENDING);
					wlc_user_wake_upd(wlc, WLC_USER_WAKE_REQ_VHT, FALSE);

					if (wlc_modesw_oper_mode_complete(wlc->modesw, cfg,
							BCME_OK, MODESW_PHY_UP_COMPLETE) ==
							BCME_NOTREADY) {
						WL_MODE_SWITCH(("wl%d: Resume others (cfg %d)\n",
							WLCWLUNIT(wlc), cfg->_idx));
						FOREACH_AS_BSS(wlc, idx, icfg) {
							ipmode_cfg = MODESW_BSSCFG_CUBBY(
								wlc->modesw, icfg);
							if ((icfg != cfg) &&
								(ipmode_cfg->state ==
									MSW_UP_DEFERRED)) {
								ipmode_cfg->state = MSW_UP_PERFORM;
								wlc_modesw_resume_opmode_change(
									wlc->modesw, icfg);
							}
						}
					}
				} else {
					WL_MODE_SWITCH(("UP:send ACTION!\n"));
					wlc_modesw_ctrl_hdl_all_cfgs(modesw_info, cfg,
						MODESW_PHY_UP_COMPLETE);
					wlc_modesw_send_action_frame_request(modesw_info, cfg,
						&cfg->BSSID, opermode_bw);
				}
				wlc_modesw_time_measure(modesw_info, pmodesw_bsscfg->ctrl_flags,
					MODESW_TM_PM0);
			}
			break;
		case MSW_DN_PM1_WAIT_ACK:
			{
				WL_MODE_SWITCH(("wl%d: Got the NULL ack for PM 1 for cfg = %d."
					"Start actual downG\n",
					WLCWLUNIT(wlc), cfg->_idx));
				wlc_modesw_time_measure(modesw_info, pmodesw_bsscfg->ctrl_flags,
					MODESW_TM_PM1);
				/* Dont drain pkts for MCHAN active. TBD for mchan */
				if (TXPKTPENDTOT(wlc) == 0) {
					wlc_modesw_change_state(modesw_info, cfg, MSW_DN_PERFORM);
					wlc_modesw_resume_opmode_change(modesw_info,
						cfg);
				}
				else {
					WLC_BLOCK_DATAFIFO_SET(wlc, DATA_BLOCK_TXCHAIN);
					wlc_modesw_change_state(modesw_info, cfg, MSW_WAIT_DRAIN);
				}
				wlc_modesw_time_measure(modesw_info, pmodesw_bsscfg->ctrl_flags,
					MODESW_TM_PHY_COMPLETE);
			}
			break;
		case MSW_DN_PM0_WAIT_ACK:
			{
				WL_MODE_SWITCH(("wl%d: Got NULL ack for PM 0 for cfg = %d..\n",
					WLCWLUNIT(wlc), cfg->_idx));
				/* Start the Tx queue */
				WLC_BLOCK_DATAFIFO_CLEAR(wlc, DATA_BLOCK_TXCHAIN);
				if (wlc_modesw_oper_mode_complete(modesw_info, cfg,
						BCME_OK, MODESW_DN_STA_COMPLETE) ==
						BCME_NOTREADY) {
					WL_MODE_SWITCH(("wl%d: Resume others (cfg %d)\n",
						WLCWLUNIT(wlc), cfg->_idx));
					FOREACH_AS_BSS(wlc, idx, icfg) {
						ipmode_cfg = MODESW_BSSCFG_CUBBY(modesw_info, icfg);
						if ((icfg != cfg) &&
								(ipmode_cfg->state ==
								MSW_DN_DEFERRED)) {
							ipmode_cfg->state = MSW_DN_PERFORM;
							wlc_modesw_resume_opmode_change(modesw_info,
								icfg);
						}
					}
				}
				wlc_modesw_time_measure(modesw_info, pmodesw_bsscfg->ctrl_flags,
					MODESW_TM_PM0);
			}
			break;
		default:
			break;
	}
}

/* Timer handler for
 * AP downgrade
 * BSSCFG clone
 * Opermode complete
 */
void
wlc_modesw_oper_mode_timer(void *arg)
{
	wlc_modesw_cb_ctx_t *ctx = (wlc_modesw_cb_ctx_t *)arg;
	wlc_info_t *wlc = (wlc_info_t *)ctx->modesw_info->wlc;
	wlc_bsscfg_t *bsscfg = wlc_bsscfg_find_by_ID(wlc, ctx->connID);
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = NULL;
	uint8 notif = ctx->notif;

	/* in case bsscfg is freed before this callback is invoked */
	if (bsscfg == NULL) {
		WL_ERROR(("wl%d: %s: unable to find bsscfg by ID %p\n",
		          wlc->pub->unit, __FUNCTION__, OSL_OBFUSCATE_BUF(arg)));
		return;
	}

	pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(wlc->modesw, bsscfg);

	/* Reset notif state */
	ctx->notif = MODESW_TIMER_NULL;
	switch (notif) {
		case MODESW_TIMER_EXEC_CALLBACKS:
		{
#ifdef STA
			bool PM = bsscfg->pm->PM;
#endif // endif
			/* Unblock data fifo */
			if (wlc->block_datafifo & DATA_BLOCK_TXCHAIN) {
				WLC_BLOCK_DATAFIFO_CLEAR(wlc, DATA_BLOCK_TXCHAIN);
			}

			if (ctx->status) {
				wlc_modesw_change_state(wlc->modesw, ctx->cfg, MSW_FAILED);
			}
			else {
				wlc_modesw_change_state(wlc->modesw, ctx->cfg, MSW_NOT_PENDING);
			}
#ifdef STA
			/* restore PM mode */
			if (!BSSCFG_AP(bsscfg) && !BSSCFG_IBSS(bsscfg)) {
				bsscfg->pm->PM = 0;
				wlc_set_pm_mode(wlc, PM, bsscfg);
			}
#endif // endif
			if (CTRL_FLAGS_HAS(pmodesw_bsscfg->ctrl_flags,
				MODESW_CTRL_HANDLE_ALL_CFGS)) {
				int idx;
				wlc_bsscfg_t *icfg;
				FOREACH_AS_BSS(wlc, idx, icfg) {
					/* Call the registered callbacks */
					wlc_modesw_ctrl_hdl_all_cfgs(ctx->modesw_info, icfg,
						ctx->signal);
				}
			} else {
				wlc_modesw_notif_cb_notif(ctx->modesw_info, ctx->cfg, ctx->status,
					ctx->oper_mode, pmodesw_bsscfg->oper_mode_old, ctx->signal);

			}
			break;
		}
		case MODESW_TIMER_AP_BACKOFF_EXP:
			wlc_modesw_process_ap_backoff_expiry(wlc->modesw, bsscfg);
			break;
		case MODESW_TIMER_COMPLETE_DOWNGRADE:
			wlc_modesw_complete_downgrade_callback(wlc->modesw, bsscfg);
			break;
		default:
			WL_MODE_SWITCH(("Unknown scheduler for %s \n", __FUNCTION__));
			ASSERT(0);
			break;
	}
	return;
}

/* This function is responsible to understand the bandwidth
 * of the given operating mode and return the corresponding
 * bandwidth using chanpsec values. This is used to compare
 * the bandwidth of the operating mode and the chanspec
 */
uint16 wlc_modesw_get_bw_from_opermode(uint8 oper_mode, chanspec_t chspec)
{
	uint16 opermode_bw_chspec_map[] = {
		WL_CHANSPEC_BW_20,
		WL_CHANSPEC_BW_40,
		WL_CHANSPEC_BW_80
	};
	uint16 bw = DOT11_OPER_MODE_CHANNEL_WIDTH(oper_mode);
	uint8 bw160_8080 = DOT11_OPER_MODE_160_8080(oper_mode);
	bw = opermode_bw_chspec_map[bw];
	/* Based on VHT Op IE derived chanspec, differentiate between 80+80 and 160 */
	if (bw == WL_CHANSPEC_BW_80 && bw160_8080) {
		bw = CHSPEC_BW(chspec);
	}
	return bw;
}

static int
wlc_modesw_opmode_pending(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *cfg)
{
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = NULL;
	uint8 state;
	pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, cfg);
	state = pmodesw_bsscfg->state;

	if ((state != MSW_DN_PERFORM) &&
	    (state !=	MSW_UP_PERFORM) &&
	    (state != MSW_DN_DEFERRED) &&
	    (state != MSW_UP_DEFERRED) &&
	    (state != MSW_UP_PM0_WAIT_ACK) &&
	    (state != MSW_DN_PM0_WAIT_ACK) &&
	    (state != MSW_UP_AF_WAIT_ACK) &&
	    (state !=	MSW_NOT_PENDING)) {
			return BCME_NOTREADY;
		}
	return BCME_OK;
}

/* This function handles the pending downgrade after the Tx packets are sent out */
void
wlc_modesw_resume_opmode_change(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *bsscfg)
{
	wlc_info_t *wlc = modesw_info->wlc;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = NULL;
#if defined(BCMDBG)
	modesw_bsscfg_cubby_t *ipmode_cfg = NULL;
#endif // endif
	int idx;
	wlc_bsscfg_t* icfg;
	pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, bsscfg);

	if (!pmodesw_bsscfg || (pmodesw_bsscfg->state == MSW_NOT_PENDING)) {
		return;
	}
	WL_MODE_SWITCH(("wl%d: Resuming opmode for cfg = %d with state = %s\n",
		WLCWLUNIT(wlc), bsscfg->_idx, opmode_st_names[pmodesw_bsscfg->state].name));

	/* Check if other cfg's are done before any HW change */
	FOREACH_AS_BSS(wlc, idx, icfg) {
#if defined(BCMDBG)
		ipmode_cfg = MODESW_BSSCFG_CUBBY(modesw_info, icfg);
#endif // endif
		if ((icfg != bsscfg) && wlc_modesw_opmode_pending(modesw_info, icfg)) {
#if defined(BCMDBG)
			WL_MODE_SWITCH(("wl%d: Opmode pending for cfg = %d"
				" with state = %s..Deferring cfg %d\n",
				WLCWLUNIT(wlc), icfg->_idx,
				opmode_st_names[ipmode_cfg->state].name,
				bsscfg->_idx));
#endif // endif
			if (pmodesw_bsscfg->state == MSW_DN_PERFORM) {
				wlc_modesw_change_state(modesw_info, bsscfg, MSW_DN_DEFERRED);
			}
			else if (pmodesw_bsscfg->state == MSW_UP_PERFORM) {
				wlc_modesw_change_state(modesw_info, bsscfg, MSW_UP_DEFERRED);
			}
			return;
		}
	}
	/* BSscfg move if required */
	if (CTRL_FLAGS_HAS(pmodesw_bsscfg->ctrl_flags,
			MODESW_CTRL_RSDB_MOVE)) {
		WL_MODE_SWITCH(("Starting bsscfg Move \n"));

		/* Set the caller context */
		ASSERT(pmodesw_bsscfg->timer_ctx->notif == 0);
		pmodesw_bsscfg->timer_ctx->notif = MODESW_TIMER_BSSCFG_CLONE;

		wl_add_timer(wlc->wl, pmodesw_bsscfg->oper_mode_timer, 0, FALSE);

		/* Skip actual HW changes for dual mac rsdb */
		return;
	}
	/* Call actual HW change */
	pmodesw_bsscfg->resume = TRUE;
	wlc_modesw_perform_upgrade_downgrade(modesw_info, bsscfg);
	return;
}

/* Returns TRUE if the new operating mode settings are lower than
 * the existing operating mode.
 */
bool
wlc_modesw_is_downgrade(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	uint8 oper_mode_old, uint8 oper_mode_new)
{
	uint8 old_bw, new_bw, old_nss, new_nss, old160, new160;
	old_bw = DOT11_OPER_MODE_CHANNEL_WIDTH(oper_mode_old);
	new_bw = DOT11_OPER_MODE_CHANNEL_WIDTH(oper_mode_new);
	old_nss = DOT11_OPER_MODE_RXNSS(oper_mode_old);
	new_nss = DOT11_OPER_MODE_RXNSS(oper_mode_new);
	old160 = DOT11_OPER_MODE_160_8080(oper_mode_old);
	new160 = DOT11_OPER_MODE_160_8080(oper_mode_new);

	if (new_nss < old_nss)
		return TRUE;
	if (new_bw < old_bw)
		return TRUE;
	if (new160 < old160)
		return TRUE;
	return FALSE;
}

/* Handles STA Upgrade and Downgrade process
 */
static int
wlc_modesw_change_sta_oper_mode(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *bsscfg,
	uint8 oper_mode, uint8 enabled)
{
	wlc_info_t * wlc = modesw_info->wlc;
	struct scb *scb = NULL;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, bsscfg);
	ASSERT(bsscfg->associated);

	if (oper_mode == bsscfg->oper_mode)
	{
		WL_MODE_SWITCH(("No change in mode required. Returning...\n"));
		return BCME_OK;
	}

	if (wlc_modesw_is_connection_vht(wlc, bsscfg))
	{
		WL_MODE_SWITCH(("VHT case\n"));
		scb = wlc_scbfindband(wlc, bsscfg, &bsscfg->BSSID,
			CHSPEC_BANDUNIT(bsscfg->current_bss->chanspec));

		bsscfg->oper_mode_enabled = enabled;

		if (oper_mode == bsscfg->oper_mode)
		{
			WL_MODE_SWITCH(("No change in mode required. Returning...\n"));
			return BCME_OK;
		}
	}
	pmodesw_bsscfg->start_time = OSL_SYSUPTIME();
	pmodesw_bsscfg->oper_mode_new = oper_mode;
	pmodesw_bsscfg->oper_mode_old = bsscfg->oper_mode;
	pmodesw_bsscfg->orig_pmstate = bsscfg->pm->PMenabled;

	if (enabled) {
		wlc_user_wake_upd(wlc, WLC_USER_WAKE_REQ_VHT, TRUE);
		/* Downgrade */
		if (wlc_modesw_is_downgrade(wlc, bsscfg, bsscfg->oper_mode, oper_mode))
		{
			wlc_modesw_change_state(modesw_info, bsscfg, MSW_DOWNGRADE_PENDING);
			pmodesw_bsscfg->new_chanspec =
				wlc_modesw_find_downgrade_chanspec(wlc,
				bsscfg, oper_mode, bsscfg->oper_mode);
			if (MCHAN_ACTIVE(wlc->pub) && (!wlc_ap_count(wlc->ap, FALSE)))
				return BCME_OK;
			if (!CTRL_FLAGS_HAS(pmodesw_bsscfg->ctrl_flags,
				MODESW_CTRL_DN_SILENT_DNGRADE)) {
				WL_MODE_SWITCH(("modesw:DN:sendingAction\n"));
				/* send action frame */
				wlc_modesw_send_action_frame_request(wlc->modesw, bsscfg,
					&bsscfg->BSSID, oper_mode);
			} else {
				/* eg: case: obss pseudo upgrade is done for stats collection,
				* but based on that we see that obss is still active. So this
				* path will come on downgrade. So, downgrade without
				* sending action frame.
				*/
				WL_MODE_SWITCH(("modesw:DN:silentDN\n"));

#ifdef WLMCHAN
				if (MCHAN_ACTIVE(wlc->pub) && (!wlc_ap_count(wlc->ap, FALSE))) {
					wlc_modesw_change_state(modesw_info,
						bsscfg, MSW_DN_PERFORM);
					return BCME_OK;
				}
#endif /* WLMCHAN */
				wlc_modesw_change_state(modesw_info, bsscfg, MSW_DN_PM1_WAIT_ACK);
				wlc_modesw_set_pmstate(modesw_info, bsscfg, TRUE);
			}
		}
		/* Upgrade */
		else {
			uint8 new_rxnss, old_rxnss;

			wlc_modesw_change_state(modesw_info, bsscfg, MSW_UPGRADE_PENDING);
			pmodesw_bsscfg->new_chanspec = wlc_modesw_find_upgrade_chanspec(modesw_info,
				bsscfg, oper_mode, bsscfg->oper_mode);
			new_rxnss = DOT11_OPER_MODE_RXNSS(pmodesw_bsscfg->oper_mode_new);
			old_rxnss = DOT11_OPER_MODE_RXNSS(pmodesw_bsscfg->oper_mode_old);
			if (pmodesw_bsscfg->new_chanspec == bsscfg->current_bss->chanspec &&
					(new_rxnss == old_rxnss)) {
				WL_MODE_SWITCH(("===========> No upgrade is possible \n"));
				wlc_modesw_change_state(modesw_info, bsscfg, MSW_NOT_PENDING);
				wlc_modesw_set_pmstate(modesw_info, bsscfg, FALSE);
				wlc_user_wake_upd(wlc, WLC_USER_WAKE_REQ_VHT, FALSE);
				/* Upgrade failed. inform functions of this failure */
				return BCME_BADOPTION;
			}

#ifdef WLMCHAN
			if (MCHAN_ACTIVE(wlc->pub) && (!wlc_ap_count(wlc->ap, FALSE))) {
				wlc_modesw_change_state(modesw_info, bsscfg, MSW_UP_PERFORM);
				return BCME_OK;
			}
#endif /* WLMCHAN */
			WL_MODE_SWITCH((" Upgrading Started ...Setting PM state to TRUE\n"));
			wlc_modesw_change_state(modesw_info, bsscfg, MSW_UP_PM1_WAIT_ACK);
			wlc_modesw_set_pmstate(modesw_info, bsscfg, TRUE);
		}
	}
	else {
		if (wlc_modesw_is_connection_vht(wlc, bsscfg)) {
			scb->flags3 &= ~(SCB3_OPER_MODE_NOTIF);
			WL_MODE_SWITCH(("Op mode notif disable request enable = %d\n", enabled));
		}
		bsscfg->oper_mode = oper_mode;
	}
	return BCME_OK;
}

/* Returns the new chanpsec value which could be used to downgrade
 * the bsscfg.
 */
chanspec_t
wlc_modesw_find_downgrade_chanspec(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint8 oper_mode_new,
	uint8 oper_mode_old)
{
	chanspec_t ret_chspec = 0, curr_chspec;
	uint8 new_bw, old_bw, new160, old160;
	curr_chspec = cfg->current_bss->chanspec;
	new_bw = DOT11_OPER_MODE_CHANNEL_WIDTH(oper_mode_new);
	old_bw = DOT11_OPER_MODE_CHANNEL_WIDTH(oper_mode_old);
	new160 = DOT11_OPER_MODE_160_8080(oper_mode_new);
	old160 = DOT11_OPER_MODE_160_8080(oper_mode_old);

	if (old_bw <= new_bw && new160 == old160)
		return curr_chspec;

	if (new160 == DOT11_OPER_MODE_1608080MHZ) {
		ASSERT(FALSE);
	}
	else if (new_bw == DOT11_OPER_MODE_80MHZ)
		ret_chspec = wf_chspec_primary80_chspec(curr_chspec);
	else if (new_bw == DOT11_OPER_MODE_40MHZ)
		ret_chspec = wf_chspec_primary40_chspec(curr_chspec);
	else if (new_bw == DOT11_OPER_MODE_20MHZ)
		ret_chspec = wf_chspec_ctlchspec(curr_chspec);
	return ret_chspec;
}

/* Returns the new chanpsec value which could be used to upgrade
 * the bsscfg.
 */
chanspec_t
wlc_modesw_find_upgrade_chanspec(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *cfg,
uint8 oper_mode_new, uint8 oper_mode_old)
{
	chanspec_t curr_chspec, def_chspec;
	uint8 new_bw, old_bw, def_bw, def_opermode, isnew_bw160;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, cfg);

	def_chspec = pmodesw_bsscfg->max_opermode_chanspec;
	def_opermode = wlc_modesw_derive_opermode(modesw_info, def_chspec, cfg,
		modesw_info->wlc->stf->op_rxstreams);

	new_bw = DOT11_OPER_MODE_CHANNEL_WIDTH(oper_mode_new);
	old_bw = DOT11_OPER_MODE_CHANNEL_WIDTH(oper_mode_old);
	def_bw = DOT11_OPER_MODE_CHANNEL_WIDTH(def_opermode);
	curr_chspec = cfg->current_bss->chanspec;
	isnew_bw160 = DOT11_OPER_MODE_160_8080(oper_mode_new);

	WL_MODE_SWITCH(("wl%d: Current bw = %x chspec = %x new_bw = %x orig_bw = %x "
			"def_chspec 0x%x new160 %d\n", WLCWLUNIT(modesw_info->wlc), old_bw,
			curr_chspec, new_bw, def_bw, def_chspec, isnew_bw160));

	if (!isnew_bw160) {
		/* Compare if the channel is different in max_opermode_chanspec
		 * which is required to avoid an upgrade to a different channel
		 */
		if ((old_bw >= new_bw) || (old_bw >= def_bw)) {
			return curr_chspec;
		}

		if (new_bw == DOT11_OPER_MODE_80MHZ) {
			if (CHSPEC_IS8080(def_chspec) || CHSPEC_IS160(def_chspec))
				return wf_chspec_primary80_chspec(def_chspec);
		} else if (new_bw == DOT11_OPER_MODE_40MHZ) {
			if (CHSPEC_IS8080(def_chspec) || CHSPEC_IS160(def_chspec))
				return wf_chspec_primary40_chspec(
						wf_chspec_primary80_chspec(def_chspec));
			else if (CHSPEC_IS80(def_chspec)) {
				return wf_chspec_primary40_chspec(def_chspec);
			}
		} else if (new_bw == DOT11_OPER_MODE_20MHZ) {
			ASSERT(FALSE);
		}
	}
	return def_chspec;

}

/* Function to override the current chspec for normal dngrade */
void
wlc_modesw_dynbw_tx_bw_override(wlc_modesw_info_t *modesw_info,
	wlc_bsscfg_t *bsscfg, uint32 *rspec_bw)
{
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = NULL;
	chanspec_t override_chspec;

	ASSERT(bsscfg != NULL);
	pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, bsscfg);
	ASSERT(pmodesw_bsscfg != NULL);

	if (pmodesw_bsscfg->state != MSW_DOWNGRADE_PENDING) {
		return;
	}
	override_chspec = wlc_modesw_find_downgrade_chanspec(modesw_info->wlc, bsscfg,
		pmodesw_bsscfg->oper_mode_new, pmodesw_bsscfg->oper_mode_old);
	*rspec_bw = chspec_to_rspec(CHSPEC_BW(override_chspec));
	WL_MODE_SWITCH((" Sending out at overriden rspec bw = %x\n", *rspec_bw));
}

/* Sents Action frame Request to all the STA's Connected to AP
 */
void
wlc_modesw_ap_send_action_frames(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *bsscfg)
{
	wlc_info_t *wlc = modesw_info->wlc;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = NULL;
	struct scb_iter scbiter;
	struct scb *scb;

	pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, bsscfg);
	pmodesw_bsscfg->action_sendout_counter = 0;
	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
		/* Sent Action frame only for Associated STA's */
		if (scb->state & ASSOCIATED)
		{
			pmodesw_bsscfg->action_sendout_counter++;
			WL_MODE_SWITCH(("Sending out frame no = %d \n",
				pmodesw_bsscfg->action_sendout_counter));
			wlc_modesw_send_action_frame_request(modesw_info, scb->bsscfg,
				&scb->ea, pmodesw_bsscfg->oper_mode_new);
		}
	}
	if ((pmodesw_bsscfg->state == MSW_DOWNGRADE_PENDING) &&
		(pmodesw_bsscfg->action_sendout_counter == 0)) {
		wlc_modesw_change_state(modesw_info, bsscfg, MSW_AP_DN_WAIT_DTIM);
		}
}

/* Handles AP Upgrade and Downgrade process
 */
static bool
wlc_modesw_change_ap_oper_mode(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *bsscfg,
	uint8 oper_mode, uint8 enabled)
{
	wlc_info_t * wlc = modesw_info->wlc;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = NULL;
	uint32 ctrl_flags;
	uint8 new_rxnss, old_rxnss;

	pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, bsscfg);

	if (bsscfg->oper_mode == oper_mode &&
		bsscfg->oper_mode_enabled == enabled)
		return FALSE;

	bsscfg->oper_mode_enabled = enabled;

	if (!enabled)
	{
		return TRUE;
	}
	pmodesw_bsscfg->oper_mode_old = bsscfg->oper_mode;
	pmodesw_bsscfg->oper_mode_new = oper_mode;
	new_rxnss = DOT11_OPER_MODE_RXNSS(pmodesw_bsscfg->oper_mode_new);
	old_rxnss = DOT11_OPER_MODE_RXNSS(pmodesw_bsscfg->oper_mode_old);

	/* Upgrade */
	if (!wlc_modesw_is_downgrade(wlc, bsscfg, bsscfg->oper_mode, oper_mode)) {
		WL_MODE_SWITCH(("wl%d: AP upgrade \n", WLCWLUNIT(wlc)));
		pmodesw_bsscfg->new_chanspec = wlc_modesw_find_upgrade_chanspec(modesw_info,
			bsscfg, oper_mode, bsscfg->oper_mode);

		WL_MODE_SWITCH(("wl%d: Got the new chanspec as %x bw = %x\n",
			WLCWLUNIT(wlc),
			pmodesw_bsscfg->new_chanspec,
			CHSPEC_BW(pmodesw_bsscfg->new_chanspec)));

		wlc_modesw_change_state(modesw_info, bsscfg, MSW_UPGRADE_PENDING);
		/* Update current oper mode to reflect upgraded bw */
		bsscfg->oper_mode = oper_mode;
		ctrl_flags = pmodesw_bsscfg->ctrl_flags;
		wlc_modesw_time_measure(wlc->modesw, pmodesw_bsscfg->ctrl_flags,
			MODESW_TM_PM1);
		if (!CTRL_FLAGS_HAS(ctrl_flags, (MODESW_CTRL_UP_SILENT_UPGRADE))) {
			/* Transition to this state for non-obss dyn bw cases */
			wlc_modesw_change_state(modesw_info, bsscfg, MSW_UP_PERFORM);
		}
#ifdef WLMCHAN
		if (MCHAN_ACTIVE(wlc->pub) && (!wlc_ap_count(wlc->ap, FALSE))) {
			return TRUE;
		}
#endif /* WLMCHAN */
		wlc_modesw_perform_upgrade_downgrade(modesw_info, bsscfg);
	}
	/* Downgrade */
	else {
		WL_MODE_SWITCH(("wl%d: AP Downgrade Case \n", WLCWLUNIT(wlc)));
		pmodesw_bsscfg->new_chanspec = wlc_modesw_find_downgrade_chanspec(wlc, bsscfg,
			oper_mode, bsscfg->oper_mode);
		if (!pmodesw_bsscfg->new_chanspec)
			return FALSE;
		WL_MODE_SWITCH(("wl%d: Got the new chanspec as %x bw = %x\n",
			WLCWLUNIT(wlc),
			pmodesw_bsscfg->new_chanspec,
			CHSPEC_BW(pmodesw_bsscfg->new_chanspec)));
		if (pmodesw_bsscfg->state == MSW_DOWNGRADE_PENDING) {
			/*
			 * For OBSS_DBS we wait for packets to drain and comeback here
			 * due to drain complete or after 2 secs due to Watchdog.
			 * In either case we UNBLOCK TX and just go ahead with BW_switch.
			 */
			WL_MODE_SWITCH(("wl%d:Entering %s again\n", WLCWLUNIT(wlc), __FUNCTION__));
			if (TXPKTPENDTOT(wlc) != 0) {
				WL_MODE_SWITCH(("wl%d:%s Drain not successful, Interference HIGH\n",
					WLCWLUNIT(wlc), __FUNCTION__));
			}
			if (wlc->block_datafifo & DATA_BLOCK_TXCHAIN) {
				/* Unblock and measure time at drain end */
				WLC_BLOCK_DATAFIFO_CLEAR(wlc, DATA_BLOCK_TXCHAIN);
			}
		} else {
			wlc_modesw_change_state(modesw_info, bsscfg, MSW_DOWNGRADE_PENDING);

			if (CTRL_FLAGS_HAS(pmodesw_bsscfg->ctrl_flags,
					MODESW_CTRL_AP_ACT_FRAMES) &&
					(new_rxnss == old_rxnss)) {
				/* For prot obss we do the drain before sending out action frames */
				if (TXPKTPENDTOT(wlc) != 0) {
					WLC_BLOCK_DATAFIFO_SET(wlc, DATA_BLOCK_TXCHAIN);
					wlc_modesw_change_state(wlc->modesw,
						bsscfg, MSW_WAIT_DRAIN);
					return TRUE;
				}
				if (wlc->block_datafifo & DATA_BLOCK_TXCHAIN) {
					/* Unblock and measure time at drain end */
					WLC_BLOCK_DATAFIFO_CLEAR(wlc, DATA_BLOCK_TXCHAIN);
				}
			}
		}
		/* Update current oper mode to reflect downgraded bw */
		bsscfg->oper_mode = oper_mode;
		if (!CTRL_FLAGS_HAS(pmodesw_bsscfg->ctrl_flags,
				MODESW_CTRL_DN_SILENT_DNGRADE)) {
			WL_MODE_SWITCH(("wl%d: Proper downgrade to %x\n",
				WLCWLUNIT(wlc), pmodesw_bsscfg->new_chanspec));
			wlc_modesw_ap_upd_bcn_act(modesw_info, bsscfg, pmodesw_bsscfg->state);
		}
		else {
			WL_MODE_SWITCH(("wl%d: Pseudo downgrade to %x\n",
				WLCWLUNIT(wlc), pmodesw_bsscfg->new_chanspec));
			wlc_modesw_time_measure(wlc->modesw, pmodesw_bsscfg->ctrl_flags,
				MODESW_TM_PM1);
#ifdef WLMCHAN
			if (MCHAN_ACTIVE(wlc->pub) && (!wlc_ap_count(wlc->ap, FALSE))) {
				wlc_modesw_change_state(modesw_info, bsscfg, MSW_AP_DN_WAIT_DTIM);
				return TRUE;
			}
#endif /* WLMCHAN */
			wlc_modesw_perform_upgrade_downgrade(modesw_info, bsscfg);
		}
	}

	return TRUE;
}

/* Handles AP and STA case bandwidth Separately
 */
int
wlc_modesw_handle_oper_mode_notif_request(wlc_modesw_info_t *modesw_info,
	wlc_bsscfg_t *bsscfg, uint8 oper_mode, uint8 enabled, uint32 ctrl_flags)
{
	wlc_info_t *wlc = modesw_info->wlc;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg;
	uint8 old_bw, new_bw, old_nss, new_nss, old160, new160;
	int err = BCME_OK;

	if (!bsscfg)
		return BCME_NOMEM;

	pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, bsscfg);

	if ((wlc_modesw_is_connection_vht(wlc, bsscfg)) &&
		(!MODESW_BSS_VHT_ENAB(wlc, bsscfg))) {
		return BCME_UNSUPPORTED;
	}

	if (SCAN_IN_PROGRESS(wlc->scan)) {
		/* Abort any SCAN in progress */
		WL_MODE_SWITCH(("wl%d.%d: MODESW Aborting the SCAN\n", WLCWLUNIT(wlc),
			WLC_BSSCFG_IDX(bsscfg)));
		wlc_scan_abort(wlc->scan, WLC_E_STATUS_ABORT);
	}

	if (!bsscfg->oper_mode_enabled) {
		bsscfg->oper_mode = wlc_modesw_derive_opermode(wlc->modesw,
			bsscfg->current_bss->chanspec, bsscfg,
			wlc->stf->op_rxstreams);
	}

	old_bw = DOT11_OPER_MODE_CHANNEL_WIDTH(bsscfg->oper_mode);
	new_bw = DOT11_OPER_MODE_CHANNEL_WIDTH(oper_mode);
	old_nss = DOT11_OPER_MODE_RXNSS(bsscfg->oper_mode);
	new_nss = DOT11_OPER_MODE_RXNSS(oper_mode);
	old160 = DOT11_OPER_MODE_160_8080(bsscfg->oper_mode);
	new160 = DOT11_OPER_MODE_160_8080(oper_mode);

	if (MODE_SWITCH_IN_PROGRESS(wlc->modesw)) {
		WL_MODE_SWITCH(("wl%d: bsscfg->oper_mode_enabled re-entry %d\n",
			WLCWLUNIT(wlc), bsscfg->oper_mode_enabled));
	}

	if (CTRL_FLAGS_HAS(ctrl_flags, MODESW_CTRL_SEND_EVENT)) {
		uint8 *data = NULL;
		uint32 data_size = 0;

		wlc_modesw_send_event(modesw_info, bsscfg,
				((bsscfg->oper_mode_enabled ? 0x100 : 0) | bsscfg->oper_mode),
				oper_mode, modesw_info->event_flags, modesw_info->event_reason,
				data, data_size);
	}

	/* For now we only support either an upgrade of  Nss/BW, or a downgrade
	 * of Nss/BW, not a combination of up/downgrade. Code currently handles
	 * an upgrade by changing the operation mode immediately and waiting for
	 * annoucing the capability, and handles a downgrade by announcing the
	 * lower capability, waiting for peers to act on the notification, then changing
	 * the operating mode. Handling a split up/downgrade would involve more
	 * code to change part of our operational mode immediatly, and part after
	 * a delay. Some hardware may not be able to support the intermmediate
	 * mode. The split up/downgrade is not an important use case, so avoiding
	 * the code complication.
	 */

	/* Skip handling of a combined upgrade/downgrade of Nss and BW */
	if (((new_bw < old_bw) && (new_nss > old_nss)) ||
			((new_bw > old_bw) && (new_nss < old_nss)) ||
			((new_nss != old_nss) && (old160 != new160)) ||
			(new_bw > DOT11_OPER_MODE_80MHZ)) {
		WL_MODE_SWITCH(("wl%d: Rejecting new bw: %d, old bw: %d; new nss: %d, old nss: %d "
				"old160: %d new160: %d\n", WLCWLUNIT(wlc), new_bw, old_bw,
				new_nss, old_nss, old160, new160));
		return BCME_BADARG;
	}

	if ((pmodesw_bsscfg->state != MSW_NOT_PENDING) &&
		(pmodesw_bsscfg->state != MSW_FAILED))
		return BCME_NOTREADY;

	WL_MODE_SWITCH(("wl%d: Processing new bw: %d, old bw: %d; new nss: %d, old nss: %d "
			"old160: %d new160: %d\n", WLCWLUNIT(wlc), new_bw, old_bw, new_nss,
			old_nss, old160, new160));

	pmodesw_bsscfg->ctrl_flags = ctrl_flags;

	if (BSSCFG_AP(bsscfg) || BSSCFG_IBSS(bsscfg)) {
		/* Initialize the AP operating mode if UP */
		WL_MODE_SWITCH((" AP CASE \n"));
		if ((enabled && !bsscfg->oper_mode_enabled && bsscfg->up) ||
				(enabled && pmodesw_bsscfg->timer_ctx == NULL))
		{
			pmodesw_bsscfg->max_opermode_chanspec =
				bsscfg->current_bss->chanspec;
			bsscfg->oper_mode_enabled = enabled;
			/* Initialize the oper_mode */
			bsscfg->oper_mode =
				wlc_modesw_derive_opermode(modesw_info,
				bsscfg->current_bss->chanspec, bsscfg, wlc->stf->op_rxstreams);
			if (wlc_modesw_alloc_context(modesw_info, bsscfg) != BCME_OK) {
				return BCME_NOMEM;
			}
		}
		if (bsscfg->up) {
			wlc_modesw_change_ap_oper_mode(modesw_info, bsscfg, oper_mode,
				enabled);
		}
		else {
			bsscfg->oper_mode = oper_mode;
			bsscfg->oper_mode_enabled = enabled;
		}
	}

	if (BSSCFG_INFRA_STA(bsscfg) || BSSCFG_PSTA(bsscfg)) {
		if (WLC_BSS_ASSOC_NOT_ROAM(bsscfg)) {
			struct scb *scb = NULL;

			scb = wlc_scbfindband(wlc, bsscfg, &bsscfg->BSSID,
				CHSPEC_BANDUNIT(bsscfg->current_bss->chanspec));
			BCM_REFERENCE(scb);
			/* for VHT AP, if oper mode is not supported, throw error
			* and continue with other means of mode switch (MIMOPS, CH WIDTH etc)
			*/
			if (wlc_modesw_is_connection_vht(wlc, bsscfg) &&
				!(SCB_OPER_MODE_NOTIF_CAP(scb))) {
				WL_ERROR(("wl%d: No capability for opermode switch in VHT AP"
					"....Unexpected..\n", WLCWLUNIT(wlc)));
			}

			if (enabled && !bsscfg->oper_mode_enabled) {
				bsscfg->oper_mode_enabled = enabled;
				bsscfg->oper_mode =
					wlc_modesw_derive_opermode(wlc->modesw,
					bsscfg->current_bss->chanspec, bsscfg,
					wlc->stf->op_rxstreams);
			}
			err = wlc_modesw_change_sta_oper_mode(modesw_info, bsscfg,
				oper_mode, enabled);
		}
		else {
			bsscfg->oper_mode = oper_mode;
			bsscfg->oper_mode_enabled = enabled;
		}
	}

	if (!enabled) {
		bsscfg->oper_mode = (uint8)FALSE;
		return TRUE;
	}
	return err;
}

/* pass chanspec to get respective oper mode notification bandwidth (DOT11_OPER_MODE_*) */
uint8
wlc_modesw_get_oper_bw(chanspec_t chanspec)
{
	uint16 ch_bw = CHSPEC_BW(chanspec);

	switch (ch_bw) {
		case WL_CHANSPEC_BW_160:
			return DOT11_OPER_MODE_80MHZ;
		case WL_CHANSPEC_BW_8080:
			return DOT11_OPER_MODE_80MHZ;
		case WL_CHANSPEC_BW_80:
			return DOT11_OPER_MODE_80MHZ;
		case WL_CHANSPEC_BW_40:
			return DOT11_OPER_MODE_40MHZ;
		case WL_CHANSPEC_BW_20:
			return DOT11_OPER_MODE_20MHZ;
		default: /* other unhandled cases */
			ASSERT(FALSE);
	}

	return DOT11_OPER_MODE_20MHZ;
}

/* Prepares the operating mode field based upon the
 * bandwidth specified in the given chanspec and rx streams
 * configured for the WLC.
 */
uint8
wlc_modesw_derive_opermode(wlc_modesw_info_t *modesw_info, chanspec_t chanspec,
	wlc_bsscfg_t *bsscfg, uint8 rxstreams)
{
	wlc_info_t *wlc = modesw_info->wlc;
	uint8 bw = DOT11_OPER_MODE_20MHZ, rxnss = 0, rxnss_type, oper_mode, bw160_8080 = 0;

	bw = wlc_modesw_get_oper_bw(chanspec);

	if (CHSPEC_IS8080(chanspec) || CHSPEC_IS160(chanspec)) {
		bw160_8080 = TRUE;
	}

	if (wlc_modesw_is_connection_vht(wlc, bsscfg)) {
		rxnss =  MIN(rxstreams, VHT_CAP_MCS_MAP_NSS_MAX);
		rxnss_type = FALSE; /* Currently only type 0 is supported */
		oper_mode = DOT11_D8_OPER_MODE(rxnss_type, rxnss, FALSE, bw160_8080, bw);
#if defined(WL_AIR_IQ)
	} else if (wlc_airiq_phymode_3p1(wlc)) {
		/* Air-IQ 3+1 mode unrestricted by VHT configuration, or
		 * 2.4 GHz band limitations
		 */
		rxnss =  MIN(rxstreams, VHT_CAP_MCS_MAP_NSS_MAX);
		rxnss_type = FALSE; /* Currently only type 0 is supported */
		oper_mode = DOT11_D8_OPER_MODE(rxnss_type, rxnss, FALSE, bw160_8080, bw);
#endif /* WL_AIR_IQ */
	} else {
		oper_mode = bw;
	}

	return oper_mode;
}

/* This function will check the txstatus value for the Action frame request.
* If ACK not Received,Call Action Frame Request again and Return FALSE -> no cb_ctx will happen
* If retry limit  exceeded,Send disassoc Reuest and return TRUE so that we can clear the scb cb_ctx
* If ACK Received properly,return TRUE so that we can clear the scb cb_ctx
*/
static bool
wlc_modesw_process_action_frame_status(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb,
bool *cntxt_clear, uint txstatus)
{
	modesw_scb_cubby_t *pmodesw_scb;
	wlc_modesw_cb_ctx_t *ctx;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg, *ipmode_cfg;
	wlc_bsscfg_t *icfg;
	int idx;

	pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(wlc->modesw, bsscfg);

	ASSERT(scb != NULL);
	pmodesw_scb = MODESW_SCB_CUBBY(wlc->modesw, scb);
	ASSERT(pmodesw_scb != NULL);
	ctx = pmodesw_scb->cb_ctx;
	ASSERT(ctx != NULL);

	if (BSSCFG_AP(bsscfg)) {
		WL_MODE_SWITCH(("Value of sendout counter = %d\n",
			pmodesw_bsscfg->action_sendout_counter));
		pmodesw_bsscfg->action_sendout_counter--;
	}

	if (txstatus & TX_STATUS_ACK_RCV) {
		WL_ERROR(("wl%d: cfg = %d Action Frame Ack received \n",
			WLCWLUNIT(wlc), bsscfg->_idx));
		 /* Reset the Retry counter if ACK is received properly */
		 pmodesw_scb->modesw_retry_counter = 0;
	}
	else if ((txstatus & TX_STATUS_MASK) == TX_STATUS_NO_ACK)
	{
		WL_ERROR(("ACK NOT RECEIVED... retrying....%d\n",
			pmodesw_scb->modesw_retry_counter+1));
		/* Clearing  the cb_cxt */
		MFREE(wlc->osh, ctx, sizeof(wlc_modesw_cb_ctx_t));
		pmodesw_scb->cb_ctx = NULL;
		*cntxt_clear = FALSE;
		if (pmodesw_scb->modesw_retry_counter <=
			ACTION_FRAME_RETRY_LIMIT) {
			pmodesw_scb->modesw_retry_counter++;
			/* Increment the sendout counter only for AP case */
			if (BSSCFG_AP(bsscfg))
			pmodesw_bsscfg->action_sendout_counter++;
			/* Send Action frame again */
			wlc_modesw_send_action_frame_request(wlc->modesw,
			scb->bsscfg, &scb->ea,
			pmodesw_bsscfg->oper_mode_new);
			return FALSE;
		}
		else {
			/* Retry limit Exceeded */
			pmodesw_scb->modesw_retry_counter = 0;
			/* Disassoc the non-acking recipient
			* For STA and PSTAs, they'll roam and reassoc back
			*/
			if (CTRL_FLAGS_HAS(pmodesw_bsscfg->ctrl_flags,
					MODESW_CTRL_NO_ACK_DISASSOC)) {
				wlc_senddisassoc(wlc, bsscfg, scb, &scb->ea,
					&bsscfg->BSSID, &bsscfg->cur_etheraddr,
					DOT11_RC_INACTIVITY);
			}
			if ((BSSCFG_STA(bsscfg)) &&
				(wlc_modesw_oper_mode_complete(wlc->modesw, bsscfg, BCME_ERROR,
				MODESW_ACTION_FAILURE) == BCME_NOTREADY)) {
				WL_MODE_SWITCH(("wl%d: Resume others (cfg %d)\n",
					WLCWLUNIT(wlc), bsscfg->_idx));
				FOREACH_AS_BSS(wlc, idx, icfg) {
					ipmode_cfg = MODESW_BSSCFG_CUBBY(wlc->modesw, icfg);
					if ((icfg != bsscfg) &&
						(ipmode_cfg->state ==
						MSW_DN_DEFERRED)) {
						ipmode_cfg->state = MSW_DN_PERFORM;
						wlc_modesw_resume_opmode_change(wlc->modesw, icfg);
					}
				}
				wlc_modesw_ctrl_hdl_all_cfgs(wlc->modesw, bsscfg,
					MODESW_ACTION_FAILURE);
			}
		}
	}
	return TRUE;
}

/* This function is used when we are in partial state
* a) action frame sent for UP/DN grade and we received callback, but scb becomes null
* b) based on action frame complete status. That is "ACK is successfully received or
* Retry Limit exceeded"
*/
static void
wlc_modesw_update_PMstate(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	modesw_bsscfg_cubby_t *pmodesw_bsscfg, *ipmode_cfg;
	wlc_bsscfg_t *icfg;
	int idx;
	pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(wlc->modesw, bsscfg);
	if (BSSCFG_STA(bsscfg)) {
	/* Perform the downgrade and report success */
		if (pmodesw_bsscfg->state == MSW_DN_AF_WAIT_ACK) {
#ifdef WLMCHAN
			if (MCHAN_ACTIVE(wlc->pub) && (!wlc_ap_count(wlc->ap, FALSE))) {
				wlc_modesw_change_state(wlc->modesw, bsscfg, MSW_DN_PERFORM);
				return;
			}
#endif /* WLMCHAN */
			wlc_modesw_change_state(wlc->modesw, bsscfg, MSW_DN_PM1_WAIT_ACK);
			WL_MODE_SWITCH(("Setting PM to TRUE for downgrade \n"));
			wlc_modesw_set_pmstate(wlc->modesw, bsscfg, TRUE);
		}
		else if (pmodesw_bsscfg->state == MSW_UP_AF_WAIT_ACK)
		{
			WL_MODE_SWITCH(("modesw:Upgraded: opermode: %x, chanspec: %x\n",
				bsscfg->oper_mode,
				bsscfg->current_bss->chanspec));
			wlc_modesw_change_state(wlc->modesw, bsscfg, MSW_NOT_PENDING);
			if (wlc_modesw_oper_mode_complete(wlc->modesw, bsscfg,
				BCME_OK, MODESW_UP_STA_COMPLETE) == BCME_NOTREADY) {
				WL_MODE_SWITCH(("wl%d: Resume others (cfg %d)\n",
					WLCWLUNIT(wlc), bsscfg->_idx));
				FOREACH_AS_BSS(wlc, idx, icfg) {
					ipmode_cfg = MODESW_BSSCFG_CUBBY(wlc->modesw, icfg);
					if ((icfg != bsscfg) &&
						(ipmode_cfg->state ==
						MSW_UP_DEFERRED)) {
						ipmode_cfg->state = MSW_UP_PERFORM;
						wlc_modesw_resume_opmode_change(wlc->modesw, icfg);
					}
				}
			}
			wlc_modesw_time_measure(wlc->modesw, pmodesw_bsscfg->ctrl_flags,
				MODESW_TM_ACTIONFRAME_COMPLETE);
		}
	}
	else if (BSSCFG_AP(bsscfg)) {
		if (pmodesw_bsscfg->state == MSW_DN_AF_WAIT_ACK &&
			pmodesw_bsscfg->action_sendout_counter == 0)
		{
			/* Set state as DEFERRED for downgrade case and
			* process in tbtt using this state
			*/
			wlc_modesw_change_state(wlc->modesw, bsscfg, MSW_AP_DN_WAIT_DTIM);
		}
	}
	return;
}

/* Receives the Frame Acknowledgement and Retries till ACK received Properly
 * and Updates PM States Properly
 */

static void
wlc_modesw_send_action_frame_complete(wlc_info_t *wlc, uint txstatus, void *arg)
{
	wlc_modesw_cb_ctx_t *ctx;
	wlc_bsscfg_t *bsscfg;
	struct scb *scb = NULL;
	bool cntxt_clear = TRUE;
	modesw_scb_cubby_t *pmodesw_scb = NULL;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = NULL;

	pmodesw_scb = (modesw_scb_cubby_t*)arg;
	ctx = pmodesw_scb->cb_ctx;

	if (ctx == NULL)
			return;
	WL_MODE_SWITCH(("wlc_modesw_send_action_frame_complete called \n"));
	bsscfg = wlc_bsscfg_find_by_ID(wlc, ctx->connID);

	/* in case bsscfg is freed before this callback is invoked */
	if (bsscfg == NULL) {
		WL_ERROR(("wl%d: %s: unable to find bsscfg by ID %p\n",
		          wlc->pub->unit, __FUNCTION__, OSL_OBFUSCATE_BUF(arg)));
		wlc_user_wake_upd(wlc, WLC_USER_WAKE_REQ_VHT, FALSE);
		/* Sync up the SW and MAC wake states */
		return;
	}

	pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(wlc->modesw, bsscfg);
	scb = WLPKTTAGSCBGET(ctx->pkt);

	/* Make sure that we have oper mode change pending for this cfg.
	* It may happen that we clear the oper mode context for this cfg
	* due to timeout.
	*/
	if (!pmodesw_bsscfg || ((pmodesw_bsscfg->state != MSW_DN_AF_WAIT_ACK) &&
		(pmodesw_bsscfg->state != MSW_UP_AF_WAIT_ACK))) {
		return;
	}

	/* make sure the scb still exists */
	if (scb == NULL) {
		WL_ERROR(("wl%d: %s: unable to find scb from the pkt %p\n",
		          wlc->pub->unit, __FUNCTION__, OSL_OBFUSCATE_BUF(ctx->pkt)));
		wlc_user_wake_upd(wlc, WLC_USER_WAKE_REQ_VHT, FALSE);
		wlc_modesw_update_PMstate(wlc, bsscfg);
		return;
	}
	if (scb->mark & (SCB_DEL_IN_PROG | SCB_MARK_TO_DEL)) {
		WL_MODE_SWITCH(("wl%d: cfg %d scb->mark(%x) DEL_IN_PROG | MARK_TO_DEL set\n",
			WLCWLUNIT(wlc), bsscfg->_idx,
			(scb->mark & (SCB_DEL_IN_PROG | SCB_MARK_TO_DEL))));
		return;
	}
	/* Retry Action frame sending upto RETRY LIMIT if ACK not received for STA and AP case */
	if (wlc_modesw_process_action_frame_status(wlc, bsscfg, scb, &cntxt_clear,
		txstatus) == TRUE) {
		/* Upgrade or Downgrade the Bandwidth only for STA case -
		* This will be called only if ACK is successfully received or Retry Limit exceeded
		*/
		wlc_modesw_update_PMstate(wlc, bsscfg);
	}
	/* For Upgrade Case : After all ACKs are received - set state as  MODESW_UP_AP_COMPLETE */
	if (pmodesw_bsscfg->action_sendout_counter == 0 && (BSSCFG_AP(bsscfg)) &&
		pmodesw_bsscfg->state == MSW_UP_AF_WAIT_ACK) {
		wlc_modesw_oper_mode_complete(wlc->modesw, bsscfg, BCME_OK, MODESW_UP_AP_COMPLETE);
	}

	if (cntxt_clear) {
		MFREE(wlc->osh, ctx, sizeof(wlc_modesw_cb_ctx_t));
		pmodesw_scb->cb_ctx = NULL;
	}
}

/* Return the current state of the modesw module for given bsscfg */
bool
wlc_modesw_opmode_ie_reqd(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *bsscfg)
{
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, bsscfg);
	wlc_info_t *wlc = modesw_info->wlc;
	uint8 ret = FALSE, narrow_nss = FALSE;

	ASSERT(bsscfg != NULL);

	narrow_nss = (((uint)(DOT11_OPER_MODE_RXNSS(bsscfg->oper_mode)))
		< WLC_BITSCNT(wlc->stf->hw_rxchain_cap));

	if ((bsscfg->oper_mode_enabled) &&
		(narrow_nss || (pmodesw_bsscfg->state == MSW_UP_PERFORM))) {
		ret = TRUE;
	}
	return ret;
}

/* Prepare and send the operating mode notification action frame
 * Registers a callback to handle the acknowledgement
 */
static int
wlc_modesw_send_action_frame_request(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *bsscfg,
	const struct ether_addr *ea, uint8 oper_mode_new) {
	struct scb *scb = NULL;
	void *p;
	uint8 *pbody;
	uint body_len;
	struct dot11_action_vht_oper_mode *ahdr;
	struct dot11_action_ht_ch_width *ht_hdr;
	struct dot11_action_ht_mimops *ht_mimops_hdr;
	wlc_info_t *wlc = modesw_info->wlc;
	modesw_scb_cubby_t *pmodesw_scb = NULL;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, bsscfg);
	wlc_pkttag_t* pkttag;

	ASSERT(bsscfg != NULL);

	scb = wlc_scbfindband(wlc, bsscfg, ea,
		CHSPEC_BANDUNIT(bsscfg->current_bss->chanspec));

	/* If STA disassoc during this time, update PM state */
	if (scb == NULL) {
		WL_ERROR(("Inside %s scb not found \n", __FUNCTION__));
		wlc_modesw_update_PMstate(wlc, bsscfg);
		return BCME_OK;
	}

	if (scb->mark & (SCB_DEL_IN_PROG | SCB_MARK_TO_DEL)) {
		WL_MODE_SWITCH(("wl%d: cfg %d scb->mark(%x) DEL_IN_PROG | MARK_TO_DEL set\n",
			WLCWLUNIT(wlc), bsscfg->_idx,
			(scb->mark & (SCB_DEL_IN_PROG | SCB_MARK_TO_DEL))));
		return BCME_OK;
	}
	/* Legacy Peer */
	if (!wlc_modesw_is_connection_vht(wlc, bsscfg) &&
	    !wlc_modesw_is_connection_ht(wlc, bsscfg)) {
		/* Legacy Peer. Skip action frame stage */
		pmodesw_bsscfg->oper_mode_new = oper_mode_new;
		if (wlc_modesw_is_downgrade(wlc, bsscfg,
		    pmodesw_bsscfg->oper_mode_old, oper_mode_new)) {
			wlc_modesw_change_state(modesw_info, bsscfg, MSW_DN_AF_WAIT_ACK);
		} else {
			wlc_modesw_change_state(modesw_info, bsscfg, MSW_UP_AF_WAIT_ACK);
		}
		wlc_modesw_update_PMstate(wlc, bsscfg);
		return BCME_OK;
	}

	pmodesw_scb = MODESW_SCB_CUBBY(wlc->modesw, scb);
	if (wlc_modesw_is_connection_vht(wlc, bsscfg))
		body_len = sizeof(struct dot11_action_vht_oper_mode);
	else
		body_len = sizeof(struct dot11_action_ht_ch_width);

	p = wlc_frame_get_action(wlc, ea, &bsscfg->cur_etheraddr,
		&bsscfg->BSSID, body_len, &pbody, DOT11_ACTION_NOTIFICATION);
	if (p == NULL)
	{
		WL_ERROR(("Unable to allocate the mgmt frame \n"));
		return BCME_NOMEM;
	}

	/* Avoid NULL frame for this pkt in PM2 */
	pkttag = WLPKTTAG(p);
	pkttag->flags3 |= WLF3_NO_PMCHANGE;

	if ((pmodesw_scb->cb_ctx = ((wlc_modesw_cb_ctx_t *)MALLOCZ(modesw_info->wlc->osh,
		sizeof(wlc_modesw_cb_ctx_t)))) == NULL) {
		WL_ERROR(("Inside %s....cant allocate context \n", __FUNCTION__));
		PKTFREE(wlc->osh, p, TRUE);
		return BCME_NOMEM;
	}

	if (SCB_OPER_MODE_NOTIF_CAP(scb) &&
		wlc_modesw_is_connection_vht(wlc, bsscfg)) {
		WL_MODE_SWITCH(("wl%d: cfg %d Send Oper Mode Action Frame..\n", WLCWLUNIT(wlc),
			bsscfg->_idx));
		ahdr = (struct dot11_action_vht_oper_mode *)pbody;
		ahdr->category = DOT11_ACTION_CAT_VHT;
		ahdr->action = DOT11_VHT_ACTION_OPER_MODE_NOTIF;
		ahdr->mode = oper_mode_new;
	} else if (wlc_modesw_is_connection_vht(wlc, bsscfg) ||
			wlc_modesw_is_connection_ht(wlc, bsscfg)) {
		if (DOT11_OPER_MODE_RXNSS(pmodesw_bsscfg->oper_mode_old)
			!= DOT11_OPER_MODE_RXNSS(pmodesw_bsscfg->oper_mode_new)) {
			WL_MODE_SWITCH(("wl%d: cfg %d Send MIMO PS Action Frame..\n",
				WLCWLUNIT(wlc), bsscfg->_idx));
			/* NSS change needs MIMOPS action frame */
			ht_mimops_hdr = (struct dot11_action_ht_mimops *)pbody;
			ht_mimops_hdr->category = DOT11_ACTION_CAT_HT;
			ht_mimops_hdr->action = DOT11_ACTION_ID_HT_MIMO_PS;
			if (wlc_modesw_is_downgrade(wlc, bsscfg, bsscfg->oper_mode,
				oper_mode_new)) {
				ht_mimops_hdr->control = SM_PWRSAVE_ENABLE;
			} else {
				ht_mimops_hdr->control = 0;
			}
		} else {
			ht_hdr = (struct dot11_action_ht_ch_width *) pbody;
			ht_hdr->category = DOT11_ACTION_CAT_HT;
			ht_hdr->action = DOT11_ACTION_ID_HT_CH_WIDTH;
			ht_hdr->ch_width = DOT11_OPER_MODE_CHANNEL_WIDTH(oper_mode_new);
		}
	}
	pmodesw_bsscfg->oper_mode_new = oper_mode_new;
	pmodesw_scb->cb_ctx->pkt = p;
	pmodesw_scb->cb_ctx->connID = bsscfg->ID;

	wlc_sendmgmt(wlc, p, bsscfg->wlcif->qi, scb);
	WL_MODE_SWITCH(("Registering the action frame callback in state %s for cfg %d\n",
		opmode_st_names[pmodesw_bsscfg->state].name, bsscfg->_idx));
	if (wlc_modesw_is_downgrade(wlc, bsscfg, pmodesw_bsscfg->oper_mode_old,
		oper_mode_new)) {
		wlc_modesw_change_state(modesw_info, bsscfg, MSW_DN_AF_WAIT_ACK);
	} else {
		wlc_modesw_change_state(modesw_info, bsscfg, MSW_UP_AF_WAIT_ACK);
	}
	wlc_pcb_fn_register(wlc->pcb,
		wlc_modesw_send_action_frame_complete, (void*)pmodesw_scb, p);
	return BCME_OK;
}

/* Function finds the New Bandwidth depending on the Switch Type
*	bw is an IN/OUT parameter
*/
static int
wlc_modesw_get_next_bw(uint8 *bw, uint8 switch_type, uint8 max_bw)
{
	uint8 new_bw = 0;

	if (switch_type == BW_SWITCH_TYPE_DNGRADE) {
		if (*bw == DOT11_OPER_MODE_20MHZ)
			return BCME_ERROR;

		new_bw = *bw - 1;
	} else if (switch_type == BW_SWITCH_TYPE_UPGRADE) {
		new_bw = *bw + 1;
	}
	WL_MODE_SWITCH(("%s:cur:%d,max:%d,new:%d\n", __FUNCTION__, *bw, max_bw, new_bw));
	if (new_bw > max_bw)
		return BCME_ERROR;

	*bw = new_bw;

	return BCME_OK;
}

/* This function mainly identifies whether the connection is VHT or HT
*   and calls the Notify Request function to switch the Bandwidth
*/

int
wlc_modesw_bw_switch(wlc_modesw_info_t *modesw_info,
	chanspec_t chanspec, uint8 switch_type, wlc_bsscfg_t *cfg, uint32 ctrl_flags)
{
	int iov_oper_mode = 0;
	uint8 oper_mode = 0;
	uint8 max_bw = 0, bw = 0, bw160_8080 = 0;
	int err = BCME_OK;
	bool enabled = TRUE;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg =	MODESW_BSSCFG_CUBBY(modesw_info, cfg);
	if (wlc_modesw_is_req_valid(modesw_info, cfg) != TRUE) {
		return BCME_BUSY;
	}
	pmodesw_bsscfg->ctrl_flags = ctrl_flags;

	wlc_modesw_time_measure(modesw_info, ctrl_flags, MODESW_TM_START);
	/* send ONLY action frame now */
	if (CTRL_FLAGS_HAS(ctrl_flags, MODESW_CTRL_UP_ACTION_FRAMES_ONLY)) {
		wlc_modesw_change_state(modesw_info, cfg, MSW_UPGRADE_PENDING);
		if (BSSCFG_STA(cfg)) {
			return wlc_modesw_send_action_frame_request(modesw_info, cfg,
				&cfg->BSSID, pmodesw_bsscfg->oper_mode_new);
		}
		else {
			wlc_modesw_ap_upd_bcn_act(modesw_info, cfg,
				pmodesw_bsscfg->state);
			if (pmodesw_bsscfg->action_sendout_counter == 0) {
				wlc_modesw_time_measure(modesw_info, pmodesw_bsscfg->ctrl_flags,
					MODESW_TM_ACTIONFRAME_COMPLETE);
				wlc_modesw_oper_mode_complete(modesw_info, cfg, BCME_OK,
					MODESW_UP_AP_COMPLETE);
			}
			return BCME_OK;
		}
	}
	iov_oper_mode = cfg->oper_mode & 0xff;
	if (wlc_modesw_is_connection_vht(modesw_info->wlc, cfg)) {
		if (WL_BW_CAP_160MHZ(modesw_info->wlc->band->bw_cap)) {
			max_bw = WL_BW_160MHZ;
		} else {
			max_bw = WL_BW_80MHZ;
		}
	} else {
		max_bw = WL_BW_40MHZ;
	}
	if (!cfg->oper_mode_enabled) {
		/* if not enabled, based on current bw, set values. TBD:
		* nss not taken care
		*/
		oper_mode = wlc_modesw_derive_opermode(modesw_info,
			chanspec, cfg, modesw_info->wlc->stf->op_rxstreams);
	} else {
		oper_mode = cfg->oper_mode;
	}
	bw = DOT11_OPER_MODE_CHANNEL_WIDTH(oper_mode);
	bw160_8080 = DOT11_OPER_MODE_160_8080(oper_mode);
	if (bw160_8080) {
		/* Bandwidth conversion from
		 * IEEE 802.11 REVmc D8.0/802.11-2016 of spec draft
		 */
		bw = WL_BW_160MHZ;
		bw160_8080 = 0;
	}
	if ((err = wlc_modesw_get_next_bw(&bw, switch_type, max_bw)) ==
		BCME_OK) {
		if (wlc_modesw_is_connection_vht(modesw_info->wlc, cfg)) {
			if (bw == WL_BW_160MHZ) {
				/* Bandwidth conversion to
				 * IEEE 802.11 REVmc D8.0/802.11-2016 of spec draft
				 */
				bw = WL_BW_80MHZ;
				bw160_8080 = DOT11_OPER_MODE_1608080MHZ;
			}
			iov_oper_mode = DOT11_D8_OPER_MODE(0, DOT11_OPER_MODE_RXNSS(oper_mode),
					0, bw160_8080, bw);
			WL_MODE_SWITCH(("%s:conVHT:%x\n", __FUNCTION__, iov_oper_mode));
		} else {
			iov_oper_mode = bw;
			WL_MODE_SWITCH(("%s:conHT: %x\n", __FUNCTION__, iov_oper_mode));
		}
		if (wlc_modesw_is_req_valid(modesw_info, cfg) != TRUE)
			return BCME_BUSY;
		err = wlc_modesw_handle_oper_mode_notif_request(modesw_info, cfg,
			(uint8)iov_oper_mode, enabled, ctrl_flags);
	} else {
		WL_MODE_SWITCH(("wl%d: %s: error\n", WLCWLUNIT(modesw_info->wlc),  __FUNCTION__));
	}
	return err;
}

/* Function restores all modesw variables and states to
* default values
*/
static void
wlc_modesw_restore_defaults(void *ctx, wlc_bsscfg_t *bsscfg, bool isup)
{
	wlc_modesw_info_t *modesw_info = (wlc_modesw_info_t *)ctx;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, bsscfg);
	if (!pmodesw_bsscfg)
		return;
	WL_MODE_SWITCH(("wl%d: Restore defaults for cfg %d up = %d\n",
			WLCWLUNIT(modesw_info->wlc), bsscfg->_idx, isup));
	if ((pmodesw_bsscfg->state != MSW_NOT_PENDING) &&
		(pmodesw_bsscfg->state != MSW_FAILED) &&
		(!isup)) {
		WL_MODE_SWITCH(("wl%d: Notify cleanup for cfg %d state = %d\n",
			WLCWLUNIT(modesw_info->wlc), bsscfg->_idx, pmodesw_bsscfg->state));
		wlc_modesw_notify_cleanup(modesw_info, bsscfg);
	}
	pmodesw_bsscfg->state = MSW_NOT_PENDING;
	pmodesw_bsscfg->oper_mode_new = 0;
	pmodesw_bsscfg->oper_mode_old = 0;

}

/* Function to reset the oper mode
 */
static void
wlc_modesw_reset_opmode(void *ctx, wlc_bsscfg_t *bsscfg)
{
	wlc_modesw_info_t *modesw_info = (wlc_modesw_info_t *)ctx;
	wlc_info_t *wlc = modesw_info->wlc;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, bsscfg);

	if (WLC_PHY_160_FULL_NSS(wlc) || !WL_BW_CAP_160MHZ(wlc->band->bw_cap)) {
		bsscfg->oper_mode_enabled = FALSE;
	}

	bsscfg->oper_mode = wlc_modesw_derive_opermode(modesw_info, wlc->default_bss->chanspec,
			bsscfg, wlc->stf->op_rxstreams);

	pmodesw_bsscfg->opmode_disable_dtim_cnt = MODESW_DISABLE_OMN_DTIMS;

	WL_MODE_SWITCH(("wl%d: %s oper_mode 0x%x op_mode_enabled %d max_opmode_chspec 0x%x\n",
			WLCWLUNIT(wlc), __FUNCTION__, bsscfg->oper_mode, bsscfg->oper_mode_enabled,
			pmodesw_bsscfg->max_opermode_chanspec));

}

/* Callback from assoc. This Function will free the timer context and reset
* the bsscfg variables when STA association state gets updated.
*/
static void
wlc_modesw_assoc_cxt_cb(void *ctx, bss_assoc_state_data_t *notif_data)
{
	wlc_modesw_info_t *modesw_info = NULL;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = NULL;
	ASSERT(notif_data->cfg != NULL);
	ASSERT(ctx != NULL);
	modesw_info = (wlc_modesw_info_t *)ctx;

	WL_MODE_SWITCH(("%s:Got Callback from Assoc. Clearing Context\n",
		__FUNCTION__));

	if (!notif_data->cfg)
		return;

	pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, notif_data->cfg);
	if (CTRL_FLAGS_HAS(pmodesw_bsscfg->ctrl_flags,
			MODESW_CTRL_RSDB_MOVE)) {
		if (((pmodesw_bsscfg->state == MSW_UP_PERFORM) ||
			(pmodesw_bsscfg->state == MSW_DN_PERFORM)) &&
			(notif_data->state == AS_ASSOC_VERIFY)) {
			/* Assoc recreate done after the bsscfg clone */
			/* Send PM-0 and open up data path */
			WL_MODE_SWITCH(("Assoc recreate Done \n"));

			if (pmodesw_bsscfg->state == MSW_UP_PERFORM) {
				wlc_modesw_change_state(modesw_info,
					notif_data->cfg, MSW_UP_PM0_WAIT_ACK);
			} else {
				wlc_modesw_change_state(modesw_info,
					notif_data->cfg, MSW_DN_PM0_WAIT_ACK);
			}
#ifdef STA
			WL_MODE_SWITCH(("Send PM -0 indication \n"));
			wlc_set_pmstate(notif_data->cfg, FALSE);
#endif // endif
			return;
		}
	}
}

static void
wlc_modesw_disassoc_cb(void *ctx, bss_disassoc_notif_data_t *notif_data)
{
	ASSERT(notif_data->cfg != NULL);
	ASSERT(ctx != NULL);

	if (notif_data->type != DAN_TYPE_LOCAL_DISASSOC)
		return;

	wlc_modesw_clear_phy_chanctx(ctx, notif_data->cfg);
}

/* Notify cleanup of mode switching for this CFG */
static void
wlc_modesw_notify_cleanup(wlc_modesw_info_t* modesw_info, wlc_bsscfg_t* bsscfg)
{
	wlc_info_t *wlc = modesw_info->wlc;
	wlc_bsscfg_t *icfg = NULL;
	modesw_bsscfg_cubby_t *ipmode_cfg = NULL;
	int idx = 0;
	if (wlc_modesw_oper_mode_complete(modesw_info, bsscfg,
		BCME_OK, MODESW_CLEANUP) == BCME_NOTREADY) {
		WL_MODE_SWITCH(("wl%d: Resume others (cfg %d)\n",
		WLCWLUNIT(wlc), bsscfg->_idx));
		FOREACH_AS_BSS(wlc, idx, icfg) {
			ipmode_cfg = MODESW_BSSCFG_CUBBY(modesw_info, icfg);
			if ((icfg != bsscfg) &&
				(ipmode_cfg->state ==
				MSW_DN_DEFERRED)) {
				ipmode_cfg->state = MSW_DN_PERFORM;
				if (BSSCFG_STA(icfg)) {
					wlc_modesw_resume_opmode_change(modesw_info, icfg);
				}
			}
		}
	}
}

/* Callback from CSA. This Function will reset
* the bsscfg variables on channel change.
*/
#ifdef WLCSA
static void
wlc_modesw_csa_cb(void *ctx, wlc_csa_cb_data_t *notif_data)
{
	wlc_modesw_info_t *modesw_info = (wlc_modesw_info_t *)ctx;
	wlc_bsscfg_t *bsscfg = notif_data->cfg;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, bsscfg);
	ASSERT(notif_data->cfg != NULL);
	ASSERT(ctx != NULL);

	if ((notif_data->signal == CSA_CHANNEL_CHANGE_START_INFRA) &&
		(!pmodesw_bsscfg->is_csa_lock)) {
		WL_MODE_SWITCH(("%s:Got Callback from CSA. resetting modesw\n",
			__FUNCTION__));
		pmodesw_bsscfg->is_csa_lock = TRUE;
		wlc_modesw_restore_defaults(ctx, bsscfg, FALSE);
		wlc_modesw_reset_opmode(ctx, bsscfg);

	} else if (notif_data->signal == CSA_CHANNEL_CHANGE_END) {
		pmodesw_bsscfg->is_csa_lock = FALSE;
	}
}
#endif /* WLCSA */

/* bsscfg_updown cb is deprecated and replaced with bsscfg_state_upd cb.
 * The registered cb is invoked for change in the mentioned bsscfg
 * states: ENABLE/DISABLE/UP/DOWN. This cb is also invoked when PHY
 * chanspec change is caused(Ex: oper_mode BW down/upgrade).For wl down/up,
 * MODESW states are restored to default and oper_mode is reset.
 */
static void
wlc_modesw_bsscfg_opmode_upd_cb(void *ctx, bsscfg_state_upd_data_t *evt)
{
	wlc_modesw_info_t *modesw_info = NULL;
	wlc_bsscfg_t *cfg = evt->cfg;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = NULL;
	bool old_en, old_up, new_en, new_up;

	ASSERT(cfg != NULL);
	ASSERT(ctx != NULL);

	modesw_info = (wlc_modesw_info_t *)ctx;
	pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, cfg);

	old_en = evt->old_enable;
	old_up = evt->old_up;
	new_en = cfg->enable;
	new_up = cfg->up;

	WL_MODE_SWITCH(("wl%d.%d: %s: old_enable %d, old_up %d, new_enable %d, new_up %d\n",
			WLCWLUNIT(modesw_info->wlc), WLC_BSSCFG_IDX(cfg), __FUNCTION__,
			old_en, old_up, new_en, new_up));

	if (CTRL_FLAGS_HAS(pmodesw_bsscfg->ctrl_flags,
			MODESW_CTRL_RSDB_MOVE))
		return;

	/*  bsscfg is disabled */
	if (new_en == FALSE) {
		WL_MODE_SWITCH(("wl%d.%d.%s: bsscfg disabled.return\n",
				WLCWLUNIT(modesw_info->wlc), WLC_BSSCFG_IDX(cfg), __FUNCTION__));
		return;
	}

	/* oper_mode BW down/upgrade does not change bsscfg states but effects PHY chspec
	 * change which invokes this cb. Since, bsscfg states reamin same, do nothing and return.
	 */
	if (old_en == new_en && old_up == new_up) {
		WL_MODE_SWITCH(("wl%d.%d.%s: No change in bsscfg state.return\n",
				WLCWLUNIT(modesw_info->wlc), WLC_BSSCFG_IDX(cfg), __FUNCTION__));
		return;
	}

	/* Clear phy_chanctxt when bsscfg is brought down (wl down) */
	if (old_up == TRUE && new_up == FALSE) {
		WL_MODE_SWITCH(("wl%d.%d: %s: clbk from bsscfg_state_upd.intf down.modesw reset\n",
				WLCWLUNIT(modesw_info->wlc), WLC_BSSCFG_IDX(cfg), __FUNCTION__));
		/* Clear PHY context during wl down */
		wlc_modesw_clear_phy_chanctx(ctx, cfg);
	}

	/* Restore MODESW states and reset oper_mode for wl down/up */
	wlc_modesw_restore_defaults(ctx, evt->cfg, evt->cfg->up);
	wlc_modesw_reset_opmode(ctx, evt->cfg);
}

/* Callback to update oper_mode on channel switch */
static void
wlc_modesw_update_opermode(wlc_modesw_info_t *modesw_info, chanspec_t chspec,
	wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = (wlc_info_t *)modesw_info->wlc;
	wlc_stf_info_t *stf = wlc->stf;
	uint16 bwcap = modesw_info->wlc->band->bw_cap;

	cfg->oper_mode = wlc_modesw_derive_opermode(modesw_info, chspec, cfg, stf->op_rxstreams);

	if (BSSCFG_AP(cfg)) {
		if ((CHSPEC_IS160(chspec) || WL_BW_CAP_160MHZ(bwcap)) &&
				WLC_PHY_160_HALF_NSS(wlc)) {
			cfg->oper_mode_enabled = TRUE;
		}
	} else {
		/* For STA oper_mode is NOT Enabled */
		cfg->oper_mode_enabled = FALSE;
	}
}

/* Callback to update oper_mode on channel switch */
/* For BW downgrade, update the oper_mode value for all */
/* the CFG's.On BW upgrade, update oper_mode of only the */
/* first up CFG(in MBSS case) or Primary CFG.During BW up */
/* each CFG will update its oper_mode individually. */
static void
wlc_modesw_chansw_opmode_upd_cb(void *arg, wlc_chansw_notif_data_t *data)
{
	wlc_info_t *wlc = (wlc_info_t *)arg;
	chanspec_t new_chspec = data->new_chanspec;
	chanspec_t old_chspec = data->old_chanspec;
	wlc_bsscfg_t *icfg;
	int idx;

	/* Opermode not needed when scan in progress */
	if (SCAN_IN_PROGRESS(wlc->scan)) {
		return;
	}

	/* On BW change, update the home_chanspec as done in wlc_update_bandwidth */
	wlc->home_chanspec = new_chspec;
#ifdef WL_BEAMFORMING
	if (TXBF_ENAB(wlc->pub)) {
		wlc_txbf_chanspec_upd(wlc->txbf);
	}
#endif // endif
#ifdef WL_ULMU
	wlc_ulmu_chanspec_upd(wlc);
#endif /* WL_ULMU */
	WL_MODE_SWITCH(("wl%d: %s, old_cs %x, new_cs %x home_chanspec 0x%x\n",
			WLCWLUNIT(wlc), __FUNCTION__, old_chspec, new_chspec,
			wlc->home_chanspec));

	if (CHSPEC_BW_LT(new_chspec, CHSPEC_BW(old_chspec))) { /* BW downgrade */
		WL_MODE_SWITCH(("BW Downgrade\n"));
		/* For BW downgrade, update opmode for all BSS, based on chanspec and streams */
		FOREACH_BSS(wlc, idx, icfg) {
			wlc_modesw_update_opermode(wlc->modesw, new_chspec, icfg);
		}
	} else {
		WL_MODE_SWITCH(("BW Upgrade\n"));
		FOR_ALL_UP_BSS(wlc, idx, icfg) {
			break; // break at the first UP cfg
		}
		if (icfg == NULL) {
			icfg = wlc->primary_bsscfg;
		}
		/* For BW upgrade on chansw, update opmode of only the first UP cfg */
		wlc_modesw_update_opermode(wlc->modesw, new_chspec, icfg);
	}
}

/* Function to Clear PHY context created during bandwidth switching */
static void
wlc_modesw_clear_phy_chanctx(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_modesw_info_t *modesw_info = (wlc_modesw_info_t *)ctx;
	wlc_info_t *wlc = modesw_info->wlc;
	int idx;
	wlc_bsscfg_t *bsscfg;
	chanspec_t chanspec_160 = 0, chanspec_80 = 0, chanspec_40 = 0, chanspec_20 = 0;
	chanspec_t ctl_ch;
	chanspec_t chspec_band;
	bool cspec160 = FALSE, cspec80 = FALSE, cspec40 = FALSE, cspec20 = FALSE;

	WL_MODE_SWITCH(("%s: Clearing PHY Contexts chanspec[%x] cfg[%p]\n",
		__FUNCTION__, cfg->current_bss->chanspec, OSL_OBFUSCATE_BUF(cfg)));

	/* Get Control channel using the current bsscfg chanspec */
	ctl_ch = wf_chspec_ctlchan(cfg->current_bss->chanspec);
	chspec_band = CHSPEC_BAND(cfg->current_bss->chanspec);

	if (chspec_band != WL_CHANSPEC_BAND_2G) {
		/* Get 160 Mhz Chanspec using the Control channel */
		chanspec_160 = wf_channel2chspec(ctl_ch, WL_CHANSPEC_BW_160, chspec_band);

		/* Get 80 Mhz Chanspec using the Control channel */
		chanspec_80 = wf_channel2chspec(ctl_ch, WL_CHANSPEC_BW_80, chspec_band);
	}

	/* Get 40 Mhz Chanspec using the Control channel */
	chanspec_40 = wf_channel2chspec(ctl_ch, WL_CHANSPEC_BW_40, chspec_band);

	/* Get 20 Mhz Chanspec  using the Control channel */
	chanspec_20 = wf_channel2chspec(ctl_ch, WL_CHANSPEC_BW_20, chspec_band);

	WL_MODE_SWITCH(("%s: chanspec_160 [%x] chanspec_80 [%x] chanspec_40[%x] chanspec_20 [%x]\n",
		__FUNCTION__, chanspec_160, chanspec_80, chanspec_40, chanspec_20));

	/* Check whether same chanspec is used by any other interface or not.
	* If so,Dont clear that chanspec phy context as it can be used by other interface
	*/
	FOREACH_BSS(wlc, idx, bsscfg) {

		WL_MODE_SWITCH(("%s: idx [%d] bsscfg [%p] <===> cfg[%p]\n",
			__FUNCTION__, idx, OSL_OBFUSCATE_BUF(bsscfg), OSL_OBFUSCATE_BUF(cfg)));

		/* Clear chanspec for not Associated STA and AP down cases as well */
		if (!MODESW_BSS_ACTIVE(wlc, bsscfg))
			continue;

		/* Skip the bsscfg that is being sent */
		if (bsscfg == cfg)
			continue;

		if (bsscfg->current_bss->chanspec == chanspec_160)
			cspec160 = TRUE;

		if (bsscfg->current_bss->chanspec == chanspec_80)
			cspec80 = TRUE;

		if (bsscfg->current_bss->chanspec == chanspec_40)
			cspec40 = TRUE;

		if (bsscfg->current_bss->chanspec == chanspec_20)
			cspec20 = TRUE;
	}

	WL_MODE_SWITCH(("%s: cspec160 [%d] cspec80 [%d] cspec40 [%d] cspec20 [%d]\n",
		__FUNCTION__, cspec160, cspec80, cspec40, cspec20));

	/* Clear all the PHY contexts  */
	if ((!cspec160) && (chanspec_160))
		phy_chanmgr_destroy_ctx(WLC_PI(wlc), chanspec_160);

	if ((!cspec80) && (chanspec_80))
		phy_chanmgr_destroy_ctx(WLC_PI(wlc), chanspec_80);

	if ((!cspec40) && (chanspec_40))
		phy_chanmgr_destroy_ctx(WLC_PI(wlc), chanspec_40);

	if ((!cspec20) && (chanspec_20))
		phy_chanmgr_destroy_ctx(WLC_PI(wlc), chanspec_20);
}

/* Function to Measure Time taken for each step for Actual Downgrade,
*  Silent Upgrade , Silent downgrade
*/
void
wlc_modesw_time_measure(wlc_modesw_info_t *modesw_info, uint32 ctrl_flags, uint32 event)
{
#ifdef WL_MODESW_TIMECAL
	uint32 value;
	modesw_time_calc_t *pmodtime = modesw_info->modesw_time;

	/* Get the current Time value */
	wlc_read_tsf(modesw_info->wlc, &value, NULL);

	WL_MODE_SWITCH((" wlc_modesw_time_measure ctrl_flags [0x%x] event[%d] ",
		ctrl_flags, event));

	if (!ctrl_flags) /* Normal Downgrade case */
	{
		switch (event)
		{
			case MODESW_TM_START:
				INCR_CHECK(modesw_info->ActDNCnt, TIME_CALC_LIMT);
				SEQ_INCR(pmodtime[modesw_info->ActDNCnt - 1].ActDN_SeqCnt);
				pmodtime[modesw_info->ActDNCnt - 1].DN_start_time = value;
				break;
			case MODESW_TM_PM1:
				pmodtime[modesw_info->ActDNCnt - 1].DN_ActionFrame_time
					= value;
				break;
			case MODESW_TM_PHY_COMPLETE:
				pmodtime[modesw_info->ActDNCnt - 1].DN_PHY_BW_UPDTime
					= value;
				break;
			case MODESW_TM_PM0:
				pmodtime[modesw_info->ActDNCnt - 1].DN_CompTime
					= value;
				break;
			default:
				break;
		}
	}
	/* Pseudo upgrade case */
	else if (CTRL_FLAGS_HAS(ctrl_flags, MODESW_CTRL_UP_SILENT_UPGRADE))
	{
		switch (event)
		{
			case MODESW_TM_START:
				INCR_CHECK(modesw_info->PHY_UPCnt, TIME_CALC_LIMT);
				SEQ_INCR(pmodtime[modesw_info->PHY_UPCnt-1].PHY_UP_SeqCnt);
				pmodtime[modesw_info->PHY_UPCnt - 1].PHY_UP_start_time
					= value;
				break;
			case MODESW_TM_PM1:
				pmodtime[modesw_info->PHY_UPCnt - 1].PHY_UP_PM1_time
					= value;
				break;
			case MODESW_TM_PHY_COMPLETE:
				pmodtime[modesw_info->PHY_UPCnt - 1].PHY_UP_BW_UPDTime
					= value;
				break;
			case MODESW_TM_PM0:
				pmodtime[modesw_info->PHY_UPCnt - 1].PHY_UP_CompTime
					= value;
				break;
			default:
				break;
		}
	}
	/* Silent Downgrade case */
	else if (CTRL_FLAGS_HAS(ctrl_flags, MODESW_CTRL_DN_SILENT_DNGRADE))
	{
		switch (event)
		{
			case MODESW_TM_START:
				INCR_CHECK(modesw_info->PHY_DNCnt, TIME_CALC_LIMT);
				SEQ_INCR(pmodtime[modesw_info->PHY_DNCnt-1].PHY_DN_SeqCnt);
				pmodtime[modesw_info->PHY_DNCnt - 1].PHY_DN_start_time
					= value;
				break;
			case MODESW_TM_PM1:
				pmodtime[modesw_info->PHY_DNCnt - 1].PHY_DN_PM1_time
					= value;
				break;
			case MODESW_TM_PHY_COMPLETE:
				pmodtime[modesw_info->PHY_DNCnt - 1].PHY_DN_BW_UPDTime
					= value;
				break;
			case MODESW_TM_PM0:
				pmodtime[modesw_info->PHY_DNCnt - 1].PHY_DN_CompTime
					= value;
				break;
			default:
				break;
		}
	}
	/* Normal upgrade case */
	else if (CTRL_FLAGS_HAS(ctrl_flags, MODESW_CTRL_UP_ACTION_FRAMES_ONLY))
	{
		switch (event)
		{
			case MODESW_TM_START:
				INCR_CHECK(modesw_info->ActFRM_Cnt, TIME_CALC_LIMT);
				SEQ_INCR(pmodtime[modesw_info->ActFRM_Cnt-1].ACTFrame_SeqCnt);
				pmodtime[modesw_info->ActFRM_Cnt - 1].ACTFrame_start
					= value;
				break;
			case MODESW_TM_ACTIONFRAME_COMPLETE:
				pmodtime[modesw_info->ActFRM_Cnt - 1].ACTFrame_complete
					= value;
				break;
			default:
				break;
		}
	}
#endif /* WL_MODESW_TIMECAL */
}

void
wlc_modesw_clear_context(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = NULL;
#ifdef STA
		bool PM = cfg->pm->PM;
#endif // endif
	wlc = modesw_info->wlc;
	pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, cfg);

	if (!pmodesw_bsscfg)
		return;

	if (pmodesw_bsscfg->state == MSW_NOT_PENDING) {
		return;
	}
	if (BSSCFG_STA(cfg)) {
#ifdef STA
		if (!BSSCFG_AP(cfg)) {
			cfg->pm->PM = 0;
			wlc_set_pm_mode(wlc, PM, cfg);
		}
#endif // endif
		wlc_user_wake_upd(wlc, WLC_USER_WAKE_REQ_VHT, FALSE);
	}

	if (wlc->block_datafifo & DATA_BLOCK_TXCHAIN) {
		WLC_BLOCK_DATAFIFO_CLEAR(wlc, DATA_BLOCK_TXCHAIN);
	}
	pmodesw_bsscfg->start_time = 0;
	pmodesw_bsscfg->state = MSW_NOT_PENDING;
	return;
}

static void wlc_modesw_ctrl_hdl_all_cfgs(wlc_modesw_info_t *modesw_info,
		wlc_bsscfg_t *cfg, enum notif_events signal)
{
	modesw_bsscfg_cubby_t *pmodesw_bsscfg;
	pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, cfg);

	if (pmodesw_bsscfg == NULL)
		return;

	if (CTRL_FLAGS_HAS(pmodesw_bsscfg->ctrl_flags,
			MODESW_CTRL_HANDLE_ALL_CFGS)) {
		/* This flag signifies that a per bsscfg
		* callback is needed. eg:for Dynamic
		* BWSW we need a callback for every
		* bsscfg to indicate that PHY UP is
		* done.
		*/
		wlc_modesw_notif_cb_notif(modesw_info, cfg, BCME_OK,
			cfg->oper_mode, pmodesw_bsscfg->oper_mode_old, signal);
	}
}

bool
wlc_modesw_pending_check(wlc_modesw_info_t *modesw_info)
{
	wlc_info_t *wlc = modesw_info->wlc;
	uint8 idx;
	modesw_bsscfg_cubby_t *ipmode_cfg;
	wlc_bsscfg_t* icfg;

	FOREACH_AS_BSS(wlc, idx, icfg) {
		ipmode_cfg = MODESW_BSSCFG_CUBBY(modesw_info, icfg);
		/* FAILED for someone else ?? */
		if (ipmode_cfg->state == MSW_FAILED) {
			WL_MODE_SWITCH(("wl%d: Opmode change failed for cfg = %d..\n",
				WLCWLUNIT(wlc), icfg->_idx));
		}
		/* Pending for someone else ?? */
		else if ((ipmode_cfg->state != MSW_NOT_PENDING)) {
			WL_MODE_SWITCH(("wl%d: Opmode change pending for cfg = %d..\n",
				WLCWLUNIT(wlc), icfg->_idx));
			return FALSE;
		}
	}
	return TRUE;
}

bool
wlc_modesw_in_progress(wlc_modesw_info_t *modesw_info)
{
	wlc_info_t *wlc;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = NULL;
	wlc_bsscfg_t *icfg;
	int idx;
	uint8 msw_pend = FALSE;
	if (!modesw_info)
		return msw_pend;
	wlc = modesw_info->wlc;
	for (idx = 0; idx < WLC_MAXBSSCFG; idx++) {
		icfg = wlc->bsscfg[idx];
		if (!icfg) {
			continue;
		}
		pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(wlc->modesw, icfg);
		if (pmodesw_bsscfg && pmodesw_bsscfg->state != MSW_NOT_PENDING &&
			pmodesw_bsscfg->state != MSW_FAILED)
			msw_pend = TRUE;
	}
	return msw_pend;
}

#ifdef WLMCHAN
bool
wlc_modesw_mchan_hw_switch_complete(wlc_info_t * wlc, wlc_bsscfg_t * bsscfg)
{
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = NULL;
	pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(wlc->modesw, bsscfg);
	if (pmodesw_bsscfg && ((pmodesw_bsscfg->state == MSW_NOT_PENDING) ||
		(pmodesw_bsscfg->state == MSW_FAILED)))
		return TRUE;
	else
		return FALSE;
}

bool
wlc_modesw_mchan_hw_switch_pending(wlc_info_t * wlc, wlc_bsscfg_t * bsscfg,
	bool hw_only)
{
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = NULL;
	bool ret = FALSE;
	pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(wlc->modesw, bsscfg);
	if (pmodesw_bsscfg)
	{
		if (!hw_only) {
			if ((pmodesw_bsscfg->state == MSW_DN_PERFORM) ||
				(pmodesw_bsscfg->state == MSW_UP_PERFORM) ||
				(pmodesw_bsscfg->state == MSW_DOWNGRADE_PENDING))
				ret = TRUE;
		} else {
			if ((pmodesw_bsscfg->state == MSW_DN_PERFORM) ||
				(pmodesw_bsscfg->state == MSW_UP_PERFORM))
				ret = TRUE;
		}
	}
	return ret;
}

void
wlc_modesw_mchan_switch_process(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t * bsscfg)
{
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = NULL;
	wlc_info_t * wlc;
	wlc = modesw_info->wlc;
	if (!bsscfg) {
		return;
	}
	pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(wlc->modesw, bsscfg);
	if (!pmodesw_bsscfg) {
		return;
	}

	if (BSSCFG_STA(bsscfg)) {
		switch (pmodesw_bsscfg->state)
		{
			case MSW_DOWNGRADE_PENDING:
				WL_MODE_SWITCH(("modesw:DN:sendingAction cfg-%d\n", bsscfg->_idx));
				/* send action frame */
				wlc_modesw_send_action_frame_request(wlc->modesw, bsscfg,
					&bsscfg->BSSID, pmodesw_bsscfg->oper_mode_new);
			break;
			case MSW_DN_PERFORM:
				wlc_modesw_perform_upgrade_downgrade(modesw_info, bsscfg);
				bsscfg->oper_mode = wlc_modesw_derive_opermode(wlc->modesw,
					bsscfg->current_bss->chanspec, bsscfg,
					wlc->stf->op_rxstreams);
				wlc_modesw_oper_mode_complete(modesw_info, bsscfg,
				BCME_OK, MODESW_DN_STA_COMPLETE);
			break;
			case MSW_UP_PERFORM:
				wlc_modesw_perform_upgrade_downgrade(modesw_info, bsscfg);
				bsscfg->oper_mode = wlc_modesw_derive_opermode(wlc->modesw,
					bsscfg->current_bss->chanspec, bsscfg,
					wlc->stf->op_rxstreams);
				wlc_modesw_send_action_frame_request(modesw_info, bsscfg,
					&bsscfg->BSSID, pmodesw_bsscfg->oper_mode_new);
			break;
		}
	}
	else {
		switch (pmodesw_bsscfg->state)
		{
			case MSW_DN_PERFORM:
				if (pmodesw_bsscfg->action_sendout_counter == 0) {
					WL_MODE_SWITCH(("All acks received. starting downgrade\n"));
					wlc_modesw_perform_upgrade_downgrade(wlc->modesw, bsscfg);
					wlc_modesw_change_state(wlc->modesw, bsscfg,
						MSW_NOT_PENDING);
					WL_MODE_SWITCH(("\n Send Sig = MODESW_DN_AP_COMPLETE\n"));
					wlc_modesw_oper_mode_complete(wlc->modesw, bsscfg,
						BCME_OK, MODESW_DN_AP_COMPLETE);
				}
			break;
			case MSW_UP_PERFORM:
				wlc_modesw_perform_upgrade_downgrade(wlc->modesw, bsscfg);
				/* Update the oper_mode of the cfg */
				bsscfg->oper_mode = wlc_modesw_derive_opermode(modesw_info,
					bsscfg->current_bss->chanspec, bsscfg,
					wlc->stf->op_rxstreams);

				if (!CTRL_FLAGS_HAS(pmodesw_bsscfg->ctrl_flags,
					(MODESW_CTRL_DN_SILENT_DNGRADE |
					MODESW_CTRL_UP_SILENT_UPGRADE))) {
					wlc_modesw_ap_upd_bcn_act(wlc->modesw, bsscfg,
						pmodesw_bsscfg->state);
				}
				wlc_modesw_oper_mode_complete(wlc->modesw, bsscfg, BCME_OK,
					MODESW_UP_AP_COMPLETE);
			break;
		}
	}
}
#endif /* WLMCHAN */

static void
wlc_modesw_set_pmstate(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t* bsscfg, bool state)
{
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = NULL;
	wlc_info_t * wlc;
	wlc = modesw_info->wlc;
	pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(wlc->modesw, bsscfg);

	/* Update PM mode accordingly to reflect new PM state */
	if (state) {
		mboolset(bsscfg->pm->PMblocked, WLC_PM_BLOCK_MODESW);
		if (bsscfg->pm->PMenabled != state) {
			/* Set the PM state if we are not already
			* in the same state.
			*/
			wlc_set_pmstate(bsscfg, state);
		} else {
			/* If we are already in PM1 mode then both the above calls
			* are skipped. So fake the PM ack reception for moving the
			* state machine forward.
			*/
			wlc_modesw_pm_pending_complete(modesw_info, bsscfg);
		}
	}
	else {
		mboolclr(bsscfg->pm->PMblocked, WLC_PM_BLOCK_MODESW);
		/* Restore PM State only if we changed it during mode switch */
		if (pmodesw_bsscfg->orig_pmstate != bsscfg->pm->PMenabled) {
			wlc_set_pmstate(bsscfg, pmodesw_bsscfg->orig_pmstate);
		} else {
		/* If we did not change PM mode and PM state then both the above calls
		* are skipped. So fake the PM ack reception for moving the
		* state machine forward.
		*/
			wlc_modesw_pm_pending_complete(modesw_info, bsscfg);
		}
	}
	return;
}

/* To keep track of STAs which get associated or disassociated */
static void
wlc_modesw_scb_state_upd_cb(void *ctx, scb_state_upd_data_t *notif_data)
{
	wlc_modesw_info_t *modesw_info = (wlc_modesw_info_t*)ctx;
	wlc_bsscfg_t *bsscfg;
	struct scb *scb;
	uint8 oldstate = notif_data->oldstate;

	scb = notif_data->scb;
	bsscfg = scb->bsscfg;

	/* Check if the state transited SCB is internal.
	 * In that case we have to do nothing, just return back
	 */
	if (SCB_INTERNAL(scb))
		return;

	/* Check to see if we are a Soft-AP/GO and the state transited SCB
	 * is a non BRCM as well as a legacy STA
	 */
	if (BSSCFG_STA(bsscfg)) {
		/* Decrement the count if state transition is from
		 * associated -> unassociated
		 */
		if (((oldstate & ASSOCIATED) && !SCB_ASSOCIATED(scb)) ||
			((oldstate & AUTHENTICATED) && !SCB_AUTHENTICATED(scb))) {
			WL_MODE_SWITCH(("wl%d: cfg %d Deauth Done. Clear MODESW Context",
				WLCWLUNIT(bsscfg->wlc), bsscfg->_idx));
			wlc_modesw_restore_defaults(modesw_info, bsscfg, FALSE);
		}
	}
}

/* Recover the mode switch module whenever pkt response
* is delayed.
*/
static void
wlc_modesw_process_recovery(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg;
	uint32 current_time = 0;

	if (!cfg || !modesw_info)
		return;

	wlc =  modesw_info->wlc;
	pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, cfg);

	/* Make sure that we have oper mode change pending for this cfg.
	* It may happen that we clear the oper mode context for this cfg.
	*/
	if (!pmodesw_bsscfg || (pmodesw_bsscfg->state == MSW_NOT_PENDING) ||
		(pmodesw_bsscfg->state == MSW_FAILED) || (!pmodesw_bsscfg->start_time)) {
		return;
	}

	current_time = OSL_SYSUPTIME();

	if ((current_time - pmodesw_bsscfg->start_time) > MODESW_RECOVERY_THRESHOLD) {
		WL_ERROR(("wl%d: cfg %d Needs Recovery state = %s time = %d msec\n", WLCWLUNIT(wlc),
			cfg->_idx, opmode_st_names[pmodesw_bsscfg->state].name,
			(current_time - pmodesw_bsscfg->start_time)));

		switch (pmodesw_bsscfg->state)
		{
			case MSW_DN_AF_WAIT_ACK:
				wlc_modesw_update_PMstate(wlc, cfg);
			break;
			case MSW_UP_AF_WAIT_ACK:
				wlc_modesw_update_PMstate(wlc, cfg);
				WL_MODE_SWITCH(("wl%d: send_count = %d, IS_AP = %d, state = %s\n",
					wlc->pub->unit,
					pmodesw_bsscfg->action_sendout_counter,
					BSSCFG_AP(cfg),
					opmode_st_names[pmodesw_bsscfg->state].name));
				if (BSSCFG_AP(cfg)) {
					pmodesw_bsscfg->action_sendout_counter = 0;
					wlc_modesw_oper_mode_complete(wlc->modesw, cfg,
						BCME_OK, MODESW_UP_AP_COMPLETE);
				}
			break;
			case MSW_DN_PM0_WAIT_ACK:
			case MSW_DN_PM1_WAIT_ACK:
			case MSW_UP_PM0_WAIT_ACK:
			case MSW_UP_PM1_WAIT_ACK:
				wlc_modesw_pm_pending_complete(modesw_info, cfg);
			break;
			case MSW_WAIT_DRAIN:
				wlc_modesw_bsscfg_complete_downgrade(wlc->modesw);
			break;
		}
		/* Take new timestamp for recovery. */
		pmodesw_bsscfg->start_time = OSL_SYSUPTIME();
	}
	return;
}

static void
wlc_modesw_watchdog(void *context)
{
	wlc_info_t *wlc;
	uint8 idx;
	wlc_bsscfg_t *icfg;
	wlc_modesw_info_t *modesw_info = (wlc_modesw_info_t *)context;
	wlc = modesw_info->wlc;

	FOREACH_AS_BSS(wlc, idx, icfg) {
		wlc_modesw_process_recovery(modesw_info, icfg);
	}
	return;
}
