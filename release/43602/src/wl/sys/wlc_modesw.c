/*
 * wlc_modesw.c -- .
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_modesw.c 449355 2014-02-06 21:41:18Z  $
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
#include "bcmwifi_channels.h"
#include <wlc_modesw.h>
#include <wlc_pcb.h>
#include "wlc_scb.h"
#include <wl_export.h>
#include <wlc_bmac.h>
#include <bcmendian.h>

enum oper_mode_states {
	MSW_NOT_PENDING = 0,
	MSW_UPGRADE_PENDING,
	MSW_DOWNGRADE_PENDING
};

enum PM_states {
	PM_IDLE = 0,
	DN_PM_1,
	DN_PM_0,
	UP_PM_1,
	UP_PM_0
};

/* iovar table */
enum {
	IOV_HT_BWSWITCH = 1,
	IOV_DUMP_DYN_BWSW = 2,
	IOV_MODESW_TIME_CALC = 3,
	IOV_MODESW_LAST
};

/* Macro Definitions */
#define MODESW_AP_DOWNGRADE_BACKOFF	50

/* Action Frame retry limit init */
#define ACTION_FRAME_RETRY_LIMIT 1

/* IOVAR Specific defines */
#define BWSWITCH_20MHZ	20
#define BWSWITCH_40MHZ	40


#define CTRL_FLAGS_HAS(flags, bit)	((uint32)flags & bit)

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

static int
wlc_modesw_doiovar(void *handle, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint plen, void *arg, int alen, int vsize, struct wlc_if *wlcif);

static const bcm_iovar_t modesw_iovars[] = {
	{"dummy", IOV_MODESW_LAST,
	(0), IOVT_BOOL, 0
	},
	{"ht_bwswitch", IOV_HT_BWSWITCH, 0, IOVT_UINT32, 0},
#if defined(BCMDBG) || defined(BCMDBG_DUMP)
	{"dump_modesw_dyn_bwsw", IOV_DUMP_DYN_BWSW,
	(IOVF_GET_UP), IOVT_BUFFER, WLC_IOCTL_MAXLEN,
	},
#endif
#ifdef WL_MODESW_TIMECAL
	{"modesw_timecal", IOV_MODESW_TIME_CALC,
	(0), IOVT_BUFFER, 0
	},
#endif
	{NULL, 0, 0, 0, 0}
};

typedef struct wlc_modesw_ctx {
	wlc_info_t *wlc;
	uint16		connID;
	void *pkt;
} wlc_modesw_cb_ctx_t;

/* Private cubby struct for ModeSW */
typedef struct {
	uint8 oper_mode_new;
	uint8 oper_mode_old;
	uint8 state;
	uint8 PM_state;
	uint16 action_sendout_counter; /* count of sent out action frames */
	uint16 max_opermode_chanspec;  /* Oper mode channel changespec */
	chanspec_t new_chanspec;
	struct wl_timer *oper_mode_timer;
	wlc_modesw_cb_ctx_t *timer_ctx;
	uint32 ctrl_flags;	/* control some overrides, eg: used for pseudo operation */
} modesw_bsscfg_cubby_t;

/* SCB Cubby for modesw */
typedef struct {
	uint16 modesw_retry_counter;
	wlc_modesw_cb_ctx_t *cb_ctx;
} modesw_scb_cubby_t;

/* basic modesw are added in wlc structure having cubby handle */
struct wlc_modesw_info {
	wlc_info_t *wlc;
	int cfgh;   /* bsscfg cubby handle */
	int scbh;		/* scb cubby handle */
	bcm_notif_h modesw_notif_hdl;
#ifdef WL_MODESW_TIMECAL
	bool modesw_timecal_enable;
	uint32 ActDNCnt;
	uint32 PHY_UPCnt;
	uint32 PHY_DNCnt;
	uint32 ActFRM_Cnt;
	modesw_time_calc_t modesw_time[TIME_CALC_LIMT];
#endif /* This should always be at the end of this structure */
};

/* bsscfg specific info access accessor */
#define MODESW_BSSCFG_CUBBY_LOC(modesw, cfg) \
	((modesw_bsscfg_cubby_t **)BSSCFG_CUBBY((cfg), (modesw)->cfgh))
#define MODESW_BSSCFG_CUBBY(modesw, cfg) (*(MODESW_BSSCFG_CUBBY_LOC(modesw, cfg)))

#define MODESW_SCB_CUBBY_LOC(modesw, scb) ((modesw_scb_cubby_t **)SCB_CUBBY((scb), (modesw)->scbh))
#define MODESW_SCB_CUBBY(modesw, scb) (*(MODESW_SCB_CUBBY_LOC(modesw, scb)))

/* Initiate scb cubby ModeSW context */
int wlc_modesw_scb_init(void *ctx, struct scb *scb);

/* Remove scb cubby ModeSW context */
static void wlc_modesw_scb_deinit(void *ctx, struct scb *scb);

/* Initiate ModeSW context */
int wlc_modesw_bss_init(void *ctx, wlc_bsscfg_t *cfg);

/* Remove ModeSW context */
static void wlc_modesw_bss_deinit(void *ctx, wlc_bsscfg_t *cfg);

void wlc_modesw_perform_sta_upgrade_downgrade(wlc_modesw_info_t *modesw_info,
	wlc_bsscfg_t *bsscfg);

void wlc_modesw_perform_ap_upgrade_downgrade(wlc_modesw_info_t *modesw_info,
	wlc_bsscfg_t *bsscfg, chanspec_t chanspec);

void wlc_modesw_oper_mode_timer(void *arg);


int wlc_modesw_change_sta_oper_mode(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *bsscfg,
	uint8 oper_mode, uint8 enabled);

chanspec_t wlc_modesw_find_downgrade_chanspec(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	uint8 oper_mode_new, uint8 oper_mode_old);

chanspec_t wlc_modesw_find_upgrade_chanspec(wlc_modesw_info_t *modesw_info,
	wlc_bsscfg_t *cfg, uint8 oper_mode_new, uint8 oper_mode_old);

bool wlc_modesw_change_ap_oper_mode(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *bsscfg,
	uint8 oper_mode, uint8 enabled);

void
wlc_modesw_ap_upd_bcn_act(wlc_modesw_info_t *modesw_info,
wlc_bsscfg_t *bsscfg, uint8 state);

static int
wlc_modesw_send_action_frame_request(wlc_modesw_info_t *modesw_info,
	wlc_bsscfg_t *bsscfg, const struct ether_addr *ea, uint8 oper_mode_new);

void
wlc_modesw_ap_send_action_frames(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *bsscfg);

static bool
wlc_modesw_process_action_frame_status(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb,
bool *cntxt_clear, uint txstatus);

static void
wlc_modesw_update_PMstate(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);

static void
wlc_modesw_assoc_cxt_cb(void *ctx, bss_assoc_state_data_t *notif_data);

void
wlc_modesw_time_measure(wlc_modesw_info_t *modesw_info, uint32 ctrl_flags, uint32 event);

/* ModeSW attach Function to Register Module and reserve cubby Structure */
wlc_modesw_info_t *
BCMATTACHFN(wlc_modesw_attach)(wlc_info_t *wlc)
{
	wlc_modesw_info_t *modesw_info;
	bcm_notif_module_t *notif;

	if (!(modesw_info = (wlc_modesw_info_t *)MALLOCZ(wlc->osh, sizeof(wlc_modesw_info_t)))) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}

	modesw_info->wlc = wlc;

	/* reserve cubby in the bsscfg container for per-bsscfg private data */
	if ((modesw_info->cfgh = wlc_bsscfg_cubby_reserve(wlc, sizeof(modesw_bsscfg_cubby_t *),
		wlc_modesw_bss_init, wlc_modesw_bss_deinit, NULL, (void *)modesw_info)) < 0) {
			WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve() failed\n",
				wlc->pub->unit, __FUNCTION__));
			goto err;
	}

	/* reserve cubby in the scb container for per-scb private data */
	if ((modesw_info->scbh = wlc_scb_cubby_reserve(wlc, sizeof(modesw_scb_cubby_t *),
		wlc_modesw_scb_init, wlc_modesw_scb_deinit, NULL, (void *)modesw_info)) < 0) {
			WL_ERROR(("wl%d: %s: wlc_scb_cubby_reserve() failed\n",
				wlc->pub->unit, __FUNCTION__));
			goto err;
	}

	/* register module */
	if (wlc_module_register(wlc->pub, modesw_iovars, "modesw", modesw_info, wlc_modesw_doiovar,
	                        NULL, NULL, NULL)) {
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
	wlc->pub->_modesw = TRUE;
	return modesw_info;
err:
	wlc_modesw_detach(modesw_info);
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

	if (wlc_bss_assoc_state_unregister(wlc, wlc_modesw_assoc_cxt_cb, wlc->modesw) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bss_assoc_state_unregister() failed\n",
			wlc->pub->unit, __FUNCTION__));
	}
	MFREE(wlc->osh, modesw_info, sizeof(wlc_modesw_info_t));
}
#if defined(BCMDBG) || defined(BCMDBG_DUMP)
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
		bcm_bprintf(&b, "Modesw PM state=%d\n", pmodesw_bsscfg->PM_state);
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
#endif 

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
wlc_modesw_doiovar(void *handle, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif)
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
#if defined(BCMDBG) || defined(BCMDBG_DUMP)
		case IOV_GVAL(IOV_DUMP_DYN_BWSW) :
			err = wlc_modesw_dyn_bwsw_dump(modesw, p, alen, a);
			break;
#endif
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
	modesw_bsscfg_cubby_t *modesw;

	/* free the Cubby reserve allocated memory  */
	modesw = *pmodesw_bsscfg;
	if (modesw) {
		/* timer and Context Free */
		if (modesw->oper_mode_timer != NULL) {
			wl_del_timer(wlc->wl, modesw->oper_mode_timer);
			wl_free_timer(wlc->wl, modesw->oper_mode_timer);
			modesw->oper_mode_timer = NULL;
		}
		if (modesw->timer_ctx != NULL) {
			MFREE(wlc->osh, modesw->timer_ctx,
			sizeof(wlc_modesw_cb_ctx_t));
			modesw->timer_ctx = NULL;
		}
		MFREE(wlc->osh, modesw, sizeof(modesw_bsscfg_cubby_t));
		*pmodesw_bsscfg = NULL;
	}
}

/* Allocate modesw context , and return the status back
 */
int
wlc_modesw_scb_init(void *ctx, struct scb *scb)
{
	wlc_modesw_info_t *modeswinfo = (wlc_modesw_info_t *)ctx;
	wlc_info_t *wlc = modeswinfo->wlc;
	modesw_scb_cubby_t **pmodesw_scb = MODESW_SCB_CUBBY_LOC(modeswinfo, scb);
	modesw_scb_cubby_t *modesw_scb;

	/* allocate memory and point bsscfg cubby to it */
	if ((modesw_scb = MALLOCZ(wlc->osh, sizeof(modesw_scb_cubby_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return BCME_NOMEM;
	}
	*pmodesw_scb = modesw_scb;
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
		MFREE(wlc->osh, modesw_scb, sizeof(modesw_scb_cubby_t));
	}
	*pmodesw_scb = NULL;
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
	uint8 oper_mode, int signal)
{
	wlc_modesw_notif_cb_data_t notif_data;
	bcm_notif_h hdl = modeswinfo->modesw_notif_hdl;

	notif_data.cfg = cfg;
	notif_data.opmode = oper_mode;
	notif_data.status = status;
	notif_data.signal = signal;
	bcm_notif_signal(hdl, &notif_data);
	return;
}

bool
wlc_modesw_is_connection_vht(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	struct scb *scb = NULL;
	if (BSSCFG_STA(bsscfg)) {
		scb = wlc_scbfindband(wlc, bsscfg, &bsscfg->BSSID,
			CHSPEC_WLCBANDUNIT(bsscfg->current_bss->chanspec));
		if (scb && SCB_VHT_CAP(scb))
			return TRUE;
		return FALSE;
	}
	return (BSS_VHT_ENAB(wlc, bsscfg)) ? TRUE : FALSE;
}

/* Interface Function to set Opermode Channel Spec Variable */
int
wlc_modesw_set_max_chanspec(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *cfg,
	chanspec_t chanspec)
{
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, cfg);

	pmodesw_bsscfg->max_opermode_chanspec = chanspec;
	return BCME_OK;
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

/* Handles the TBTT interrupt for AP. Required to process
 * the AP downgrade procedure, which is waiting for DTIM
 * beacon to go out before perform actual downgrade of link
 */
void wlc_modesw_bss_tbtt(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = modesw_info->wlc;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, cfg);
	if (cfg->oper_mode_enabled && pmodesw_bsscfg &&
		pmodesw_bsscfg->state == MSW_DOWNGRADE_PENDING &&
		!CTRL_FLAGS_HAS(pmodesw_bsscfg->ctrl_flags, MODESW_CTRL_DN_SILENT_DNGRADE))
	{
		uint32 val;
		/* sanity checks */
		if (pmodesw_bsscfg->timer_ctx == NULL) {
			/* Modesw context is not initialized */
			if (!(pmodesw_bsscfg->timer_ctx =
				(wlc_modesw_cb_ctx_t *)MALLOCZ(wlc->osh,
				sizeof(wlc_modesw_cb_ctx_t)))) {
				WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
					wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
				goto fail;
			}
			pmodesw_bsscfg->timer_ctx->connID = cfg->ID;
			pmodesw_bsscfg->timer_ctx->wlc = wlc;

			WL_MODE_SWITCH(("Initializing timer ...alloc mem\n"));
			/* Initialize the timer */
			pmodesw_bsscfg->oper_mode_timer =
			wl_init_timer(wlc->wl, wlc_modesw_oper_mode_timer,
			pmodesw_bsscfg->timer_ctx,
			"opermode_dgrade_timer");

			if (pmodesw_bsscfg->oper_mode_timer == NULL)
			{
				WL_ERROR(("Timer alloc failed\n"));
				MFREE(wlc->osh, pmodesw_bsscfg->timer_ctx,
					sizeof(wlc_modesw_cb_ctx_t));
				goto fail;
			}
		}
		/* AP downgrade is pending */
		/* If it is the TBTT for DTIM then start the non-periodic timer */
		wlc_bmac_copyfrom_objmem(wlc->hw, (S_DOT11_DTIMCOUNT << 2),
		&val, sizeof(val), OBJADDR_SCR_SEL);
			if (!val) {
			WL_MODE_SWITCH(("DTIM Tbtt..Start timer for 50 msec \n"));
			ASSERT(pmodesw_bsscfg->oper_mode_timer != NULL);
			WL_MODE_SWITCH(("TBTT Timer set....\n"));
			wl_add_timer(wlc->wl, pmodesw_bsscfg->oper_mode_timer,
				MODESW_AP_DOWNGRADE_BACKOFF, FALSE);
		}
	}
	return;
fail:
	/* Downgrade can not be completed. Change the mode switch state
	 * so that a next attempt could be tried by upper layer
	 */
	pmodesw_bsscfg->state = MSW_NOT_PENDING;
	return;
}

void
wlc_modesw_perform_sta_upgrade_downgrade(wlc_modesw_info_t *modesw_info,
wlc_bsscfg_t * bsscfg)
{
	wlc_info_t *wlc = modesw_info->wlc;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = NULL;
	ASSERT(BSSCFG_STA(bsscfg));
	pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, bsscfg);
	bsscfg->flags2 |= WLC_BSSCFG_FL2_MODESW_BWSW;
	wlc_update_bandwidth(wlc, bsscfg, pmodesw_bsscfg->new_chanspec);
	bsscfg->flags2 &= ~WLC_BSSCFG_FL2_MODESW_BWSW;
	WL_MODE_SWITCH(("afterUpdBW:nc:%x,wlccs:%x,bsscs:%x\n",
		pmodesw_bsscfg->new_chanspec,
		wlc->chanspec, bsscfg->current_bss->chanspec));
}

/* Updates beacon and probe response with New bandwidth
* Sents action frames only during normal Upgrade
*/
void
wlc_modesw_ap_upd_bcn_act(wlc_modesw_info_t *modesw_info,
wlc_bsscfg_t *bsscfg, uint8 state)
{
	wlc_info_t *wlc = modesw_info->wlc;
	ASSERT(BSSCFG_AP(bsscfg));

	if (state == MSW_UPGRADE_PENDING) {
		wlc_modesw_ap_send_action_frames(wlc->modesw, bsscfg);
		wlc_modesw_notif_cb_notif(modesw_info, bsscfg,
			BCME_OK, bsscfg->oper_mode, MODESW_UP_AP_COMPLETE);
	}
	wlc_bss_update_beacon(wlc, bsscfg);
	wlc_bss_update_probe_resp(wlc, bsscfg, TRUE);
}

/* Updates Bandwidth of AP depending on the chanspec sent to this function.
 *  Only update bandwidth for Silent Downgrade Process.
 */
void
wlc_modesw_perform_ap_upgrade_downgrade(wlc_modesw_info_t *modesw_info,
wlc_bsscfg_t *bsscfg, chanspec_t chanspec)
{
	wlc_info_t *wlc = modesw_info->wlc;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = NULL;
	uint32 ctrl_flags;
	ASSERT(BSSCFG_AP(bsscfg));
	pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, bsscfg);
	ctrl_flags = pmodesw_bsscfg->ctrl_flags;
	bsscfg->flags2 |= WLC_BSSCFG_FL2_MODESW_BWSW;
	wlc_update_bandwidth(wlc, bsscfg, chanspec);
	bsscfg->flags2 &= ~WLC_BSSCFG_FL2_MODESW_BWSW;
	wlc_modesw_time_measure(wlc->modesw, pmodesw_bsscfg->ctrl_flags,
		MODESW_TM_PHY_COMPLETE);
	if (pmodesw_bsscfg->state == MSW_UPGRADE_PENDING)
		wlc_modesw_notif_cb_notif(modesw_info, bsscfg,
			BCME_OK, bsscfg->oper_mode, MODESW_PHY_UP_COMPLETE);

	if (!CTRL_FLAGS_HAS(ctrl_flags, (MODESW_CTRL_DN_SILENT_DNGRADE |
		MODESW_CTRL_UP_SILENT_UPGRADE)))
		wlc_modesw_ap_upd_bcn_act(modesw_info, bsscfg, pmodesw_bsscfg->state);

	bsscfg->oper_mode = wlc_modesw_derive_opermode(wlc->modesw, chanspec, bsscfg);
}

/* Handle the different states of the operating mode processing. */
void wlc_modesw_pm_pending_complete(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = modesw_info->wlc;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg;

	if (!cfg)
		return;

	pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, cfg);
	if (pmodesw_bsscfg == NULL)
		return;

	switch (pmodesw_bsscfg->PM_state)
	{
		case PM_IDLE:
			break;
		case UP_PM_1:
			{
				WL_MODE_SWITCH(("Got the NULL ack for PM 1.Start actualupgrade\n"));
				wlc_modesw_time_measure(wlc->modesw, pmodesw_bsscfg->ctrl_flags,
					MODESW_TM_PM1);
				wlc_modesw_perform_sta_upgrade_downgrade(wlc->modesw, cfg);
				/* update opermode otherwise we will get into problem in
				* recv_beacon. TBD: check if this is fine, as this is done in
				* wlc_modesw_send_action_frame_complete normally.
				*/
				cfg->oper_mode = wlc_modesw_derive_opermode(wlc->modesw,
					cfg->current_bss->chanspec, cfg);
				wlc_set_pmstate(cfg, FALSE);
				pmodesw_bsscfg->PM_state = UP_PM_0;
				wlc_modesw_time_measure(modesw_info, pmodesw_bsscfg->ctrl_flags,
					MODESW_TM_PHY_COMPLETE);
			}
			break;

		case UP_PM_0:
			{
				uint8 opermode_bw;
				WL_MODE_SWITCH(("Got NULL ack for PM 0..Send action frame\n"));
				opermode_bw = pmodesw_bsscfg->oper_mode_new;
				pmodesw_bsscfg->PM_state = PM_IDLE;

				wlc_modesw_notif_cb_notif(wlc->modesw, cfg, BCME_OK,
					cfg->oper_mode, MODESW_PHY_UP_COMPLETE);
				/* if pseudo state indicates pending, dont send action frame */
				if (CTRL_FLAGS_HAS(pmodesw_bsscfg->ctrl_flags,
					MODESW_CTRL_UP_SILENT_UPGRADE)) {
					/* donot send action frame., and stop the SM here */
					WL_MODE_SWITCH(("MODESW_CTRL_UP_SILENT_UPGRADE\n"));
					pmodesw_bsscfg->state = MSW_NOT_PENDING;
					wlc_user_wake_upd(wlc, WLC_USER_WAKE_REQ_VHT, FALSE);
				} else {
					WL_MODE_SWITCH(("UP:send ACTION!\n"));
					wlc_modesw_send_action_frame_request(wlc->modesw, cfg,
						&cfg->BSSID, opermode_bw);
				}
				wlc_modesw_time_measure(modesw_info, pmodesw_bsscfg->ctrl_flags,
					MODESW_TM_PM0);
			}
			break;
		case DN_PM_1:
			{
				WL_MODE_SWITCH(("Got the NULL ack for PM 1.Start actual downG\n"));
				wlc_modesw_time_measure(modesw_info, pmodesw_bsscfg->ctrl_flags,
					MODESW_TM_PM1);
				wlc_modesw_perform_sta_upgrade_downgrade(wlc->modesw, cfg);
				wlc_set_pmstate(cfg, FALSE);
				pmodesw_bsscfg->PM_state = DN_PM_0;
				cfg->oper_mode = wlc_modesw_derive_opermode(wlc->modesw,
					cfg->current_bss->chanspec, cfg);
				wlc_modesw_time_measure(modesw_info, pmodesw_bsscfg->ctrl_flags,
					MODESW_TM_PHY_COMPLETE);
			}
			break;
		case DN_PM_0:
			{
				WL_MODE_SWITCH(("Got NULL ack for PM 0..\n"));
				pmodesw_bsscfg->state = MSW_NOT_PENDING;
				wlc_user_wake_upd(wlc, WLC_USER_WAKE_REQ_VHT, FALSE);
				WL_MODE_SWITCH(("modesw:Downgraded oper mode: %x chanspec:%x\n",
					cfg->oper_mode, cfg->current_bss->chanspec));
				pmodesw_bsscfg->PM_state = PM_IDLE;
				wlc_modesw_notif_cb_notif(modesw_info, cfg, BCME_OK,
					cfg->oper_mode, MODESW_DN_STA_COMPLETE);
				wlc_modesw_time_measure(modesw_info, pmodesw_bsscfg->ctrl_flags,
					MODESW_TM_PM0);
			}
			break;
		default:
			ASSERT(0);
			break;
	}
}

/* Timer handler for AP downgrade */
void wlc_modesw_oper_mode_timer(void *arg)
{
	wlc_modesw_cb_ctx_t *ctx = (wlc_modesw_cb_ctx_t *)arg;
	wlc_info_t *wlc = (wlc_info_t *)ctx->wlc;
	wlc_bsscfg_t *bsscfg = wlc_bsscfg_find_by_ID(wlc, ctx->connID);
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = NULL;
	/* in case bsscfg is freed before this callback is invoked */
	if (bsscfg == NULL) {
		WL_ERROR(("wl%d: %s: unable to find bsscfg by ID %p\n",
		          wlc->pub->unit, __FUNCTION__, arg));
		return;
	}
	WL_MODE_SWITCH(("AP downgrade timer expired...Perform the downgrade of AP.\n"));

	pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(wlc->modesw, bsscfg);

	WL_MODE_SWITCH(("Yet to be receive Ack count = %d\n",
		pmodesw_bsscfg->action_sendout_counter));
	if (pmodesw_bsscfg->action_sendout_counter == 0) {
		WL_MODE_SWITCH(("All acks received.... starting downgrade\n"));
		wlc_modesw_time_measure(wlc->modesw, pmodesw_bsscfg->ctrl_flags,
			MODESW_TM_PM1);
		wlc_modesw_perform_ap_upgrade_downgrade(wlc->modesw, bsscfg,
			pmodesw_bsscfg->new_chanspec);
		pmodesw_bsscfg->state = MSW_NOT_PENDING;
		WL_MODE_SWITCH(("\n Sending signal = MODESW_DN_AP_COMPLETE\n"));
		wlc_modesw_notif_cb_notif(wlc->modesw, bsscfg,
			BCME_OK, bsscfg->oper_mode, MODESW_DN_AP_COMPLETE);
		wlc_modesw_time_measure(wlc->modesw, pmodesw_bsscfg->ctrl_flags,
			MODESW_TM_PM0);
	}
	return;
}

/* This function is responsible to understand the bandwidth
 * of the given operating mode and return the corresponding
 * bandwidth using chanpsec values. This is used to compare
 * the bandwidth of the operating mode and the chanspec
 */
uint16 wlc_modesw_get_bw_from_opermode(uint8 oper_mode)
{
	uint16 opermode_bw_chspec_map[] = {
		WL_CHANSPEC_BW_20,
		WL_CHANSPEC_BW_40,
		WL_CHANSPEC_BW_80,
		WL_CHANSPEC_BW_8080,
	};
	uint32 bw = DOT11_OPER_MODE_CHANNEL_WIDTH(oper_mode);
	return opermode_bw_chspec_map[bw];
}

/* Returns TRUE if the new operating mode settings are lower than
 * the existing operating mode.
 */
bool wlc_modesw_is_downgrade(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	uint8 oper_mode_old, uint8 oper_mode_new)
{
	uint8 old_bw, new_bw, old_nss, new_nss;
	old_bw = DOT11_OPER_MODE_CHANNEL_WIDTH(oper_mode_old);
	new_bw = DOT11_OPER_MODE_CHANNEL_WIDTH(oper_mode_new);

	if (wlc_modesw_is_connection_vht(wlc, bsscfg)) {
		old_nss = DOT11_OPER_MODE_RXNSS(oper_mode_old);
		new_nss = DOT11_OPER_MODE_RXNSS(oper_mode_new);
		if (new_nss < old_nss)
			return TRUE;
	}

	if (new_bw < old_bw)
		return TRUE;
	return FALSE;
}

/* Handles STA Upgrade and Downgrade process
 */
int wlc_modesw_change_sta_oper_mode(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *bsscfg,
	uint8 oper_mode, uint8 enabled)
{
	wlc_info_t * wlc = modesw_info->wlc;
	struct scb *scb = NULL;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, bsscfg);
	ASSERT(bsscfg->associated);

	if (wlc_modesw_is_connection_vht(wlc, bsscfg))
	{
		WL_MODE_SWITCH(("VHT case\n"));
		scb = wlc_scbfindband(wlc, bsscfg, &bsscfg->BSSID,
			CHSPEC_WLCBANDUNIT(bsscfg->current_bss->chanspec));

		bsscfg->oper_mode_enabled = enabled;

		if (oper_mode == bsscfg->oper_mode)
		{
			WL_MODE_SWITCH(("No change in mode required. Returning...\n"));
			return BCME_OK;
		}

		if (enabled)
			scb->flags3 |= SCB3_OPER_MODE_NOTIF;
	}
	pmodesw_bsscfg->oper_mode_new = oper_mode;
	pmodesw_bsscfg->PM_state = PM_IDLE;

	if (enabled) {

		wlc_user_wake_upd(wlc, WLC_USER_WAKE_REQ_VHT, TRUE);
		/* Downgrade */
		if (wlc_modesw_is_downgrade(wlc, bsscfg, bsscfg->oper_mode, oper_mode))
		{
			pmodesw_bsscfg->state = MSW_DOWNGRADE_PENDING;
			pmodesw_bsscfg->new_chanspec =
				wlc_modesw_find_downgrade_chanspec(wlc,
				bsscfg, oper_mode, bsscfg->oper_mode);
			if (!CTRL_FLAGS_HAS(pmodesw_bsscfg->ctrl_flags,
				MODESW_CTRL_DN_SILENT_DNGRADE)) {
				WL_MODE_SWITCH(("modesw:DN:sendingAction\n"));
				/* send action frame */

				if (pmodesw_bsscfg->new_chanspec == bsscfg->current_bss->chanspec)
				{
					bsscfg->oper_mode = wlc_modesw_derive_opermode(wlc->modesw,
						bsscfg->current_bss->chanspec, bsscfg);
					pmodesw_bsscfg->PM_state = PM_IDLE;
					pmodesw_bsscfg->state = MSW_NOT_PENDING;
					wlc_user_wake_upd(wlc, WLC_USER_WAKE_REQ_VHT, FALSE);
					return BCME_OK;
				}
				wlc_modesw_send_action_frame_request(wlc->modesw, bsscfg,
					&bsscfg->BSSID, oper_mode);
			} else {
				/* eg: case: obss pseudo upgrade is done for stats collection,
				* but based on that we see that obss is still active. So this
				* path will come on downgrade. So, downgrade without
				* sending action frame.
				*/
				WL_MODE_SWITCH(("modesw:DN:silentDN\n"));
				pmodesw_bsscfg->PM_state = DN_PM_1;

				wlc_set_pmstate(bsscfg, TRUE);
			}
		}
		/* Upgrade */
		else {
			pmodesw_bsscfg->state = MSW_UPGRADE_PENDING;
			pmodesw_bsscfg->new_chanspec = wlc_modesw_find_upgrade_chanspec(modesw_info,
				bsscfg, oper_mode, bsscfg->oper_mode);
			if (pmodesw_bsscfg->new_chanspec == bsscfg->current_bss->chanspec)
			{
				WL_MODE_SWITCH(("No upgrade is possible \n"));
				pmodesw_bsscfg->PM_state = PM_IDLE;
				pmodesw_bsscfg->state = MSW_NOT_PENDING;
				wlc_user_wake_upd(wlc, WLC_USER_WAKE_REQ_VHT, FALSE);
				/* Upgrade failed. inform functions of this failure */
				return BCME_BADOPTION;
			}
			WL_MODE_SWITCH((" Upgrading Started ...Setting PM state to TRUE\n"));
			wlc_set_pmstate(bsscfg, TRUE);

			pmodesw_bsscfg->PM_state = UP_PM_1;
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
	uint8 new_bw, old_bw;
	curr_chspec = cfg->current_bss->chanspec;
	new_bw = DOT11_OPER_MODE_CHANNEL_WIDTH(oper_mode_new);
	old_bw = DOT11_OPER_MODE_CHANNEL_WIDTH(oper_mode_old);

	if (old_bw <= new_bw)
		return curr_chspec;

	if (new_bw == DOT11_OPER_MODE_8080MHZ) {
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
	uint8 new_bw, old_bw, def_bw, def_opermode;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, cfg);

	def_chspec = pmodesw_bsscfg->max_opermode_chanspec;
	def_opermode = wlc_modesw_derive_opermode(modesw_info, def_chspec, cfg);

	new_bw = DOT11_OPER_MODE_CHANNEL_WIDTH(oper_mode_new);
	old_bw = DOT11_OPER_MODE_CHANNEL_WIDTH(oper_mode_old);
	def_bw = DOT11_OPER_MODE_CHANNEL_WIDTH(def_opermode);
	curr_chspec = cfg->current_bss->chanspec;

	WL_MODE_SWITCH(("Current bw = %x chspec = %x new_bw = %x orig_bw = %x\n", old_bw,
		curr_chspec, new_bw, def_bw));

	/* Compare if the channel is different in max_opermode_chanspec
	 * which is required to avoid an upgrade to a different channel
	 */
	if ((old_bw >= new_bw) ||
		(old_bw >= def_bw))
			return curr_chspec;

	if (new_bw == DOT11_OPER_MODE_80MHZ) {
		if (CHSPEC_IS8080(def_chspec))
			return wf_chspec_primary80_chspec(def_chspec);
	}
	else if (new_bw == DOT11_OPER_MODE_40MHZ) {
		if (CHSPEC_IS8080(def_chspec))
			return wf_chspec_primary40_chspec(wf_chspec_primary80_chspec(def_chspec));
		else if (CHSPEC_IS80(def_chspec)) {
			return wf_chspec_primary40_chspec(def_chspec);
		}
	}
	else if (new_bw == DOT11_OPER_MODE_20MHZ) {
		ASSERT(FALSE);
	}
	return def_chspec;

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
		pmodesw_bsscfg->action_sendout_counter++;
		WL_MODE_SWITCH(("Sending out frame no = %d\n",
			pmodesw_bsscfg->action_sendout_counter));
		wlc_modesw_send_action_frame_request(modesw_info, scb->bsscfg,
			&scb->ea, pmodesw_bsscfg->oper_mode_new);
	}
}

/* Handles AP Upgrade and Downgrade process
 */
bool
wlc_modesw_change_ap_oper_mode(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *bsscfg,
	uint8 oper_mode, uint8 enabled)
{
	wlc_info_t * wlc = modesw_info->wlc;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = NULL;
	pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, bsscfg);

	if (bsscfg->oper_mode == oper_mode &&
		bsscfg->oper_mode_enabled == enabled)
		return FALSE;

	bsscfg->oper_mode_enabled = enabled;

	if (!enabled)
	{
		return TRUE;
	}
	/* Upgrade */
	if (!wlc_modesw_is_downgrade(wlc, bsscfg, bsscfg->oper_mode, oper_mode)) {
		WL_MODE_SWITCH(("AP upgrade \n"));
		pmodesw_bsscfg->new_chanspec = wlc_modesw_find_upgrade_chanspec(modesw_info,
			bsscfg, oper_mode, bsscfg->oper_mode);

		WL_MODE_SWITCH(("Got the new chanspec as %x bw = %x\n",
			pmodesw_bsscfg->new_chanspec,
			CHSPEC_BW(pmodesw_bsscfg->new_chanspec)));

		pmodesw_bsscfg->state = MSW_UPGRADE_PENDING;

		pmodesw_bsscfg->oper_mode_old = bsscfg->oper_mode;
		pmodesw_bsscfg->oper_mode_new = oper_mode;
		bsscfg->oper_mode = oper_mode;
		wlc_modesw_time_measure(wlc->modesw, pmodesw_bsscfg->ctrl_flags,
			MODESW_TM_PM1);
		wlc_modesw_perform_ap_upgrade_downgrade(modesw_info, bsscfg,
			pmodesw_bsscfg->new_chanspec);
		wlc_modesw_time_measure(wlc->modesw, pmodesw_bsscfg->ctrl_flags,
			MODESW_TM_PM0);
	}
	/* Downgrade */
	else {
		WL_MODE_SWITCH(("AP Downgrade Case \n"));
		pmodesw_bsscfg->new_chanspec = wlc_modesw_find_downgrade_chanspec(wlc, bsscfg,
			oper_mode, bsscfg->oper_mode);
		if (!pmodesw_bsscfg->new_chanspec)
			return FALSE;
		WL_MODE_SWITCH(("Got the new chanspec as %x bw = %x\n",
			pmodesw_bsscfg->new_chanspec,
			CHSPEC_BW(pmodesw_bsscfg->new_chanspec)));
		pmodesw_bsscfg->state = MSW_DOWNGRADE_PENDING;

		pmodesw_bsscfg->oper_mode_new = oper_mode;
		pmodesw_bsscfg->oper_mode_old = bsscfg->oper_mode;
		bsscfg->oper_mode = oper_mode;
		if (!CTRL_FLAGS_HAS(pmodesw_bsscfg->ctrl_flags,
			MODESW_CTRL_DN_SILENT_DNGRADE)) {
			WL_MODE_SWITCH(("Normal AP Downgrade to %x\n",
				pmodesw_bsscfg->new_chanspec));
			wlc_modesw_ap_send_action_frames(modesw_info, bsscfg);
			wlc_bss_update_beacon(wlc, bsscfg);
			wlc_bss_update_probe_resp(wlc, bsscfg, TRUE);
		}

		else {
			WL_MODE_SWITCH(("Pseudo downgrade to %x\n",
				pmodesw_bsscfg->new_chanspec));
			wlc_modesw_time_measure(wlc->modesw, pmodesw_bsscfg->ctrl_flags,
				MODESW_TM_PM1);
			wlc_modesw_perform_ap_upgrade_downgrade(modesw_info, bsscfg,
				pmodesw_bsscfg->new_chanspec);
			wlc_modesw_notif_cb_notif(modesw_info, bsscfg, BCME_OK,
				bsscfg->oper_mode, MODESW_DN_AP_COMPLETE);
			pmodesw_bsscfg->state = MSW_NOT_PENDING;
			wlc_modesw_time_measure(wlc->modesw, pmodesw_bsscfg->ctrl_flags,
				MODESW_TM_PM0);
		}
	}
	if (pmodesw_bsscfg->state == MSW_UPGRADE_PENDING) {
		pmodesw_bsscfg->state = MSW_NOT_PENDING;
	}

	return TRUE;
}

/* Handles AP and STA case bandwidth Separately
 */
int wlc_modesw_handle_oper_mode_notif_request(wlc_modesw_info_t *modesw_info,
	wlc_bsscfg_t *bsscfg, uint8 oper_mode, uint8 enabled, uint32 ctrl_flags)
{
	wlc_info_t *wlc = modesw_info->wlc;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg;
	int err = BCME_OK;

	if (!bsscfg)
		return BCME_NOMEM;

	pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, bsscfg);
	if ((wlc_modesw_is_connection_vht(wlc, bsscfg)) &&
		(!BSS_VHT_ENAB(wlc, bsscfg)))
		return BCME_UNSUPPORTED;


	ASSERT(pmodesw_bsscfg != NULL);

	if (BSSCFG_AP(bsscfg)) {
		/* Initialize the AP operating mode if UP */
		WL_MODE_SWITCH((" AP CASE \n"));
		if (enabled && !bsscfg->oper_mode_enabled && bsscfg->up)
		{
			pmodesw_bsscfg->max_opermode_chanspec =
				bsscfg->current_bss->chanspec;
			bsscfg->oper_mode_enabled = enabled;
			/* Initialize the oper_mode */
			bsscfg->oper_mode =
				wlc_modesw_derive_opermode(wlc->modesw,
				bsscfg->current_bss->chanspec, bsscfg);
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

	if (BSSCFG_STA(bsscfg)) {
		if (WLC_BSS_CONNECTED(bsscfg)) {
			struct scb *scb = NULL;

			scb = wlc_scbfindband(wlc, bsscfg, &bsscfg->BSSID,
				CHSPEC_WLCBANDUNIT(bsscfg->current_bss->chanspec));
			BCM_REFERENCE(scb);
			/* for VHT AP, if oper mode is not supported, return error */
			if (wlc_modesw_is_connection_vht(wlc, bsscfg)&&
				!(SCB_OPER_MODE_NOTIF_CAP(scb))) {
				WL_ERROR(("No capability for opermode switch\n"));
				return BCME_EPERM;
			}

			if (enabled && !bsscfg->oper_mode_enabled) {
				bsscfg->oper_mode_enabled = enabled;
				bsscfg->oper_mode =
					wlc_modesw_derive_opermode(wlc->modesw,
					bsscfg->current_bss->chanspec, bsscfg);
			}
			err = wlc_modesw_change_sta_oper_mode(modesw_info, bsscfg,
				oper_mode, enabled);
		}
		else {
			bsscfg->oper_mode = oper_mode;
			bsscfg->oper_mode_enabled = enabled;
		}
	}
	return err;
}

/* Prepares the operating mode field based upon the
 * bandwidth specified in the given chanspec and rx streams
 * configured for the WLC.
 */
uint8 wlc_modesw_derive_opermode(wlc_modesw_info_t *modesw_info, chanspec_t chanspec,
	wlc_bsscfg_t *bsscfg)
{
	wlc_info_t *wlc = modesw_info->wlc;
	uint8 bw = DOT11_OPER_MODE_20MHZ, rxnss, rxnss_type;
	/* Initialize the oper_mode */
	if (CHSPEC_IS8080(chanspec))
		bw = DOT11_OPER_MODE_8080MHZ;
	else if (CHSPEC_IS80(chanspec))
		bw = DOT11_OPER_MODE_80MHZ;
	else if (CHSPEC_IS40(chanspec))
		bw = DOT11_OPER_MODE_40MHZ;
	else if (CHSPEC_IS20(chanspec))
		bw = DOT11_OPER_MODE_20MHZ;
	else {
		ASSERT(FALSE);
	}
	if (wlc_modesw_is_connection_vht(wlc, bsscfg)) {
		rxnss =  MIN(wlc->stf->rxstreams, VHT_CAP_MCS_MAP_NSS_MAX);
		rxnss_type = FALSE; /* Currently only type 0 is supported */
		return DOT11_OPER_MODE(rxnss_type, rxnss, bw);
	} else
		return bw;
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
	modesw_scb_cubby_t *pmodesw_scb = MODESW_SCB_CUBBY(wlc->modesw, scb);
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(wlc->modesw, bsscfg);
	wlc_modesw_cb_ctx_t *ctx = pmodesw_scb->cb_ctx;

	ASSERT(pmodesw_scb != NULL);
	ASSERT(ctx != NULL);

	if (BSSCFG_AP(bsscfg)) {
		WL_MODE_SWITCH(("Value of sendout counter = %d\n",
			pmodesw_bsscfg->action_sendout_counter));
		pmodesw_bsscfg->action_sendout_counter--;
	}

	if (txstatus & TX_STATUS_ACK_RCV) {
	     WL_MODE_SWITCH(("Action Frame Ack received \n"));
		 /* Reset the Retry counter if ACK is received properly */
		 pmodesw_scb->modesw_retry_counter = 0;
	}
	else if ((txstatus & TX_STATUS_MASK) == TX_STATUS_NO_ACK)
	{
		WL_MODE_SWITCH(("ACK NOT RECEIVED... retrying....%d\n",
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
			/* Retry limit Exceeded,Disassoc and  clear the context */
			pmodesw_scb->modesw_retry_counter = 0;
			if (BSSCFG_STA(bsscfg)) {
				wlc_modesw_notif_cb_notif(wlc->modesw, bsscfg,
					BCME_ERROR, bsscfg->oper_mode, MODESW_ACTION_FAILURE);
			}
			wlc_senddisassoc(wlc, bsscfg, scb, &scb->ea,
				&bsscfg->BSSID, &bsscfg->cur_etheraddr,
				DOT11_RC_INACTIVITY);
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
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(wlc->modesw, bsscfg);

	if (BSSCFG_STA(bsscfg)) {
	/* Perform the downgrade and report success */
		if (pmodesw_bsscfg->state == MSW_DOWNGRADE_PENDING) {
			WL_MODE_SWITCH(("Setting PM to TRUE for downgrade \n"));
			pmodesw_bsscfg->PM_state = DN_PM_1;
			wlc_set_pmstate(bsscfg, TRUE);
		}
		else if (pmodesw_bsscfg->state == MSW_UPGRADE_PENDING)
		{
			bsscfg->oper_mode = wlc_modesw_derive_opermode(wlc->modesw,
				bsscfg->current_bss->chanspec, bsscfg);
			WL_MODE_SWITCH(("modesw:Upgraded: opermode: %x, chanspec: %x\n",
				bsscfg->oper_mode,
				bsscfg->current_bss->chanspec));
			pmodesw_bsscfg->state = MSW_NOT_PENDING;
			/* TBD. for psuedo upgrade shuld we reset this? */
			wlc_user_wake_upd(wlc, WLC_USER_WAKE_REQ_VHT, FALSE);

			wlc_modesw_notif_cb_notif(wlc->modesw, bsscfg, BCME_OK,
				bsscfg->oper_mode, MODESW_UP_STA_COMPLETE);
			wlc_modesw_time_measure(wlc->modesw, pmodesw_bsscfg->ctrl_flags,
				MODESW_TM_ACTIONFRAME_COMPLETE);
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

	pmodesw_scb = (modesw_scb_cubby_t*)arg;
	ctx = pmodesw_scb->cb_ctx;

	if (ctx == NULL)
			return;
	WL_MODE_SWITCH(("wlc_modesw_send_action_frame_complete called \n"));
	bsscfg = wlc_bsscfg_find_by_ID(wlc, ctx->connID);
	/* in case bsscfg is freed before this callback is invoked */
	if (bsscfg == NULL) {
		WL_ERROR(("wl%d: %s: unable to find bsscfg by ID %p\n",
		          wlc->pub->unit, __FUNCTION__, arg));
		wlc_user_wake_upd(wlc, WLC_USER_WAKE_REQ_VHT, FALSE);
		/* Sync up the SW and MAC wake states */
		return;
	}

	scb = WLPKTTAGSCBGET(ctx->pkt);

	/* make sure the scb still exists */
	if (scb == NULL) {
		WL_ERROR(("wl%d: %s: unable to find scb from the pkt %p\n",
		          wlc->pub->unit, __FUNCTION__, ctx->pkt));
		wlc_modesw_update_PMstate(wlc, bsscfg);
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

	if (cntxt_clear) {
		MFREE(wlc->osh, ctx, sizeof(wlc_modesw_cb_ctx_t));
		pmodesw_scb->cb_ctx = NULL;
	}
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
	wlc_info_t *wlc = modesw_info->wlc;
	modesw_scb_cubby_t *pmodesw_scb = NULL;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, bsscfg);
	ASSERT(bsscfg != NULL);

	scb = wlc_scbfindband(wlc, bsscfg, ea,
		CHSPEC_WLCBANDUNIT(bsscfg->current_bss->chanspec));

	/* If STA disassoc during this time, update PM state */
	if (scb == NULL) {
		WL_ERROR(("Inside %s scb not found \n", __FUNCTION__));
		wlc_modesw_update_PMstate(wlc, bsscfg);
		return BCME_OK;
	}

	pmodesw_scb = MODESW_SCB_CUBBY(wlc->modesw, scb);
	if (wlc_modesw_is_connection_vht(wlc, bsscfg))
		body_len = sizeof(struct dot11_action_vht_oper_mode);
	else
		body_len = sizeof(struct dot11_action_ht_ch_width);

	p = wlc_frame_get_mgmt(wlc, FC_ACTION, ea, &bsscfg->cur_etheraddr,
		&bsscfg->BSSID, body_len, &pbody);
	if (p == NULL)
	{
		WL_ERROR(("Unable to allocate the mgmt frame \n"));
		return BCME_NOMEM;
	}

	if ((pmodesw_scb->cb_ctx = ((wlc_modesw_cb_ctx_t *)MALLOCZ(modesw_info->wlc->osh,
		sizeof(wlc_modesw_cb_ctx_t)))) == NULL) {
		WL_ERROR(("Inside %s....cant allocate context \n", __FUNCTION__));
		return BCME_NOMEM;
	}

	if (wlc_modesw_is_connection_vht(wlc, bsscfg)) {
		ahdr = (struct dot11_action_vht_oper_mode *)pbody;
		ahdr->category = DOT11_ACTION_CAT_VHT;
		ahdr->action = DOT11_VHT_ACTION_OPER_MODE_NOTIF;
		ahdr->mode = oper_mode_new;
	} else {
		ht_hdr = (struct dot11_action_ht_ch_width *) pbody;
		ht_hdr->category = DOT11_ACTION_CAT_HT;
		ht_hdr->action = DOT11_ACTION_ID_HT_CH_WIDTH;

		ht_hdr->ch_width = oper_mode_new;
	}
	pmodesw_bsscfg->oper_mode_new = oper_mode_new;
	pmodesw_scb->cb_ctx->pkt = p;
	pmodesw_scb->cb_ctx->connID = bsscfg->ID;

	wlc_sendmgmt(wlc, p, bsscfg->wlcif->qi, scb);
	WL_MODE_SWITCH(("Registering the action frame callback %d\n",
		pmodesw_bsscfg->PM_state));
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
	uint8 max_bw = 0, bw = 0;
	int err = BCME_OK;
	bool enabled = TRUE;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg =
		MODESW_BSSCFG_CUBBY(modesw_info, cfg);

	pmodesw_bsscfg->ctrl_flags = ctrl_flags;

	wlc_modesw_time_measure(modesw_info, ctrl_flags, MODESW_TM_START);
	/* send ONLY action frame now */
	if (CTRL_FLAGS_HAS(ctrl_flags, MODESW_CTRL_UP_ACTION_FRAMES_ONLY)) {
		pmodesw_bsscfg->state = MSW_UPGRADE_PENDING;
		if (BSSCFG_STA(cfg))
			return wlc_modesw_send_action_frame_request(modesw_info, cfg,
				&cfg->BSSID, pmodesw_bsscfg->oper_mode_new);
		else {
			wlc_modesw_ap_upd_bcn_act(modesw_info, cfg,
				pmodesw_bsscfg->state);
			pmodesw_bsscfg->state = MSW_NOT_PENDING;
			wlc_modesw_time_measure(modesw_info, pmodesw_bsscfg->ctrl_flags,
				MODESW_TM_ACTIONFRAME_COMPLETE);
			return BCME_OK;
		}
	}

	WL_MODE_SWITCH(("%s:cs:%x\n", __FUNCTION__, chanspec));

	iov_oper_mode = cfg->oper_mode & 0xff;
	if (wlc_modesw_is_connection_vht(modesw_info->wlc, cfg))
		max_bw = DOT11_OPER_MODE_160MHZ;
	else
		max_bw = DOT11_OPER_MODE_40MHZ;
	if (!cfg->oper_mode_enabled) {
		/* if not enabled, based on current bw, set values. TBD:
		* nss not taken care
		*/
		oper_mode = wlc_modesw_derive_opermode(modesw_info,
			chanspec, cfg);
	} else {
		oper_mode = cfg->oper_mode;
	}
	bw = DOT11_OPER_MODE_CHANNEL_WIDTH(oper_mode);
	if ((err = wlc_modesw_get_next_bw(&bw, switch_type, max_bw)) ==
		BCME_OK) {
		if (wlc_modesw_is_connection_vht(modesw_info->wlc, cfg)) {
			iov_oper_mode = ((DOT11_OPER_MODE_RXNSS(oper_mode)-1) << 4)
				| bw;
			WL_MODE_SWITCH(("%s:conVHT:%x\n", __FUNCTION__, iov_oper_mode));
		}
		else {
				iov_oper_mode = bw;
				WL_MODE_SWITCH(("%s:conHT: %x\n", __FUNCTION__, iov_oper_mode));
		}
		if (wlc_modesw_is_req_valid(modesw_info, cfg) != TRUE)
			return BCME_BUSY;
		err = wlc_modesw_handle_oper_mode_notif_request(modesw_info, cfg,
			(uint8)iov_oper_mode, enabled, ctrl_flags);
	} else {
	WL_MODE_SWITCH(("%s: error\n", __FUNCTION__));
	}

	return err;
}

/* Callback from assoc. This Function will free the timer context and reset
* the bsscfg variables when STA association state gets updated.
*/
static void
wlc_modesw_assoc_cxt_cb(void *ctx, bss_assoc_state_data_t *notif_data)
{
	wlc_modesw_info_t *modesw_info = (wlc_modesw_info_t *)ctx;
	wlc_bsscfg_t *bsscfg = notif_data->cfg;
	wlc_info_t *wlc = modesw_info->wlc;
	modesw_bsscfg_cubby_t *pmodesw_bsscfg;

	ASSERT(notif_data->cfg != NULL);
	ASSERT(ctx != NULL);
	WL_MODE_SWITCH(("%s:Got Callback from Assoc. Clearing Context\n",
		__FUNCTION__));

	if (!bsscfg)
		return;

	pmodesw_bsscfg = MODESW_BSSCFG_CUBBY(modesw_info, bsscfg);
	if (pmodesw_bsscfg->timer_ctx != NULL) {
		MFREE(wlc->osh, pmodesw_bsscfg->timer_ctx,
		sizeof(wlc_modesw_cb_ctx_t));
		pmodesw_bsscfg->timer_ctx = NULL;
	}
	pmodesw_bsscfg->PM_state = PM_IDLE;
	pmodesw_bsscfg->state = MSW_NOT_PENDING;
	pmodesw_bsscfg->oper_mode_new = 0;
	pmodesw_bsscfg->oper_mode_old = 0;

	if (notif_data->state == AS_IDLE)
	{
		wlc_modesw_clear_phy_chanctx(wlc, bsscfg);
	}
}

/* Function to Clear PHY context created during bandwidth switching */
void
wlc_modesw_clear_phy_chanctx(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	int idx;
	wlc_bsscfg_t *bsscfg;
	chanspec_t chanspec_80, chanspec_40, chanspec_20;
	chanspec_t ctl_ch;
	wlc_phy_t *ppi = wlc->band->pi;
	bool cspec80 = FALSE, cspec40 = FALSE, cspec20 = FALSE;

	WL_MODE_SWITCH(("%s: Clearing PHY Contexts cfg->current_bss->chanspec [%x]\n",
		__FUNCTION__, cfg->current_bss->chanspec));

	/* Get Control channel using the current bsscfg chanspec */
	ctl_ch = wf_chspec_ctlchan(cfg->current_bss->chanspec);

	/* Get 80 Mhz Chanspec  using the Control channel */
	chanspec_80 = wf_channel2chspec(ctl_ch, WL_CHANSPEC_BW_80);

	/* Get 40 Mhz Chanspec  using the Control channel */
	chanspec_40 = wf_channel2chspec(ctl_ch, WL_CHANSPEC_BW_40);

	/* Get 20 Mhz Chanspec  using the Control channel */
	chanspec_20 = wf_channel2chspec(ctl_ch, WL_CHANSPEC_BW_20);
	WL_MODE_SWITCH(("%s: chanspec_80 [%x]chanspec_40[%x] chanspec_20 [%x]\n",
		__FUNCTION__, chanspec_80, chanspec_40, chanspec_20));

	/* Check whether same chanspec is used by any other interface or not.
	* If so,Dont clear that chanspec phy context as it can be used by other interface
	*/
	FOREACH_BSS(wlc, idx, bsscfg) {

		WL_MODE_SWITCH(("%s: idx [%d] bsscfg [%p] <===> cfg[%p]\n",
			__FUNCTION__, idx, bsscfg, cfg));

		if (bsscfg == cfg)
			continue;

		if (bsscfg->current_bss->chanspec == chanspec_80)
			cspec80 = TRUE;

		if (bsscfg->current_bss->chanspec == chanspec_40)
			cspec40 = TRUE;

		if (bsscfg->current_bss->chanspec == chanspec_20)
			cspec20 = TRUE;
	}

	WL_MODE_SWITCH(("%s: cspec80 [%d] cspec40[%d] cspec20[%d]\n",
		__FUNCTION__, cspec80, cspec40, cspec20));

	/* Clear all the PHY contexts  */
	if (!cspec80)
		wlc_phy_destroy_chanctx(ppi, chanspec_80);

	if (!cspec40)
		wlc_phy_destroy_chanctx(ppi, chanspec_40);

	if (!cspec20)
		wlc_phy_destroy_chanctx(ppi, chanspec_20);
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
