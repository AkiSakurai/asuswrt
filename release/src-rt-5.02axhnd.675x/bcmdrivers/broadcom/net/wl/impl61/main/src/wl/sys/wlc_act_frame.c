/**
 * @file
 * @brief
 * Action frame functions
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
 * $Id: wlc_act_frame.c 776430 2019-06-28 06:33:22Z $
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <osl.h>
#include <wlioctl.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_tx.h>
#include <wlc_msch.h>
#include <wlc_pcb.h>
#include <wlc_act_frame.h>
#include <wlc_quiet.h>
#include <wlc_scb.h>
#include <wlc_event.h>
#include <wlc_event_utils.h>
#include <wlc_scan.h>
#include <wlc_scan_utils.h>
#include <wlc_chanctxt.h>

#ifdef WLRSDB
#include <wlc_rsdb.h>
#ifdef WLMCNX
#include <wlc_mcnx.h>
#endif /* WLMCNX */
#endif /* WLRSDB */

#ifdef WL_PROXDETECT
#include <wlc_ftm.h>
#endif // endif

#ifdef BCMDBG
#define ACTFRAME_MESSAGE(x)	printf x
#else /* BCMDBG */
#define ACTFRAME_MESSAGE(x)
#endif /* BCMDBG */

/* iovar table */
enum {
	IOV_ACTION_FRAME = 0,	/* Wifi protocol action frame */
	IOV_AF = 1,
	IOV_AF_ABORT = 2
};

static const bcm_iovar_t actframe_iovars[] = {
	{"wifiaction", IOV_ACTION_FRAME,
	(0), 0, IOVT_BUFFER, WL_WIFI_ACTION_FRAME_SIZE
	},
	{"actframe", IOV_AF,
	(0), 0, IOVT_BUFFER, OFFSETOF(wl_af_params_t, action_frame) +
	OFFSETOF(wl_action_frame_t, data)
	},
	{"actframe_abort", IOV_AF_ABORT, (0), 0, IOVT_BOOL, 0},
	{NULL, 0, 0, 0, 0, 0}
};

struct wlc_act_frame_info {
	wlc_info_t *wlc;		/* pointer to main wlc structure */
	int32 cfg_cubby_hdl;		/* BSSCFG cubby offset */
};

typedef struct act_frame_cubby {
	int32			af_dwell_time;
	void			*action_frame;		/* action frame for off channel */
	wlc_msch_req_handle_t	*req_msch_actframe_hdl;	/* hdl to msch request */
	wlc_actframe_callback	cb_func;		/* callback function */
	void			*cb_ctxt;		/* callback context to be passed back */
	wlc_act_frame_tx_cb_fn_t af_tx_cb;		/* optional cb func for specfic needs */
	void			*arg;			/* cb context for the optional cb func */
	void			*wlc_handler;		/* wlc's MSCH used to send act frame. */
	wlc_msch_req_param_t	req;
	bool			is_onchan_af;		/* check on chan AF */
	bool			scanInUse;
	bool			af_queued;
} act_frame_cubby_t;

#define WLC_AF_DEFAULT_DWELL_TIME 20

#define ACTION_FRAME_IN_PROGRESS(act_frame_cubby)	((act_frame_cubby)->action_frame != NULL)

/* bsscfg states access macros */
#define BSSCFG_ACT_FRAME_CUBBY_LOC(act_frame_info, cfg) \
		((act_frame_cubby_t **)BSSCFG_CUBBY((cfg), (act_frame_info)->cfg_cubby_hdl))

#define BSSCFG_ACT_FRAME_CUBBY(act_frame_info, cfg) \
		(*BSSCFG_ACT_FRAME_CUBBY_LOC(act_frame_info, cfg))

static int wlc_act_frame_init(void *ctx, wlc_bsscfg_t *cfg);
static void wlc_act_frame_deinit(void *ctx, wlc_bsscfg_t *cfg);

static int wlc_actframe_send_action_frame_cb(void* handler_ctxt, wlc_msch_cb_info_t *cb_info);
static void *wlc_prepare_action_frame(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	const struct ether_addr *bssid, void *action_frame);
static void wlc_actionframetx_complete(wlc_info_t *wlc, void *pkt, uint txstatus);

static int wlc_act_frame_wlc_down(void *ctx);
static int wlc_act_frame_doiovar(void *ctx, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size,
	struct wlc_if *wlcif);
static int wlc_msch_af_send(void* handler_ctxt, wlc_msch_cb_info_t *cb_info);
static bool wlc_act_frame_isonchan(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
static void wlc_af_send(wlc_info_t *wlc, void *arg, uint *dwell);
static void wlc_af_off_channel_callback(void * arg, int status, wlc_bsscfg_t *cfg);
static int wlc_act_frame_txaf_scan(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	act_frame_cubby_t *actframe_cubby, wl_af_params_t *af, bool *do_event);
/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

wlc_act_frame_info_t *
BCMATTACHFN(wlc_act_frame_attach)(wlc_info_t *wlc)
{
	wlc_act_frame_info_t *act_frame_info;

	act_frame_info = MALLOCZ(wlc->osh, sizeof(wlc_act_frame_info_t));

	if (!act_frame_info) {
		WL_ERROR(("%s: Alloc failure for act_frame_info\n", __FUNCTION__));
		return NULL;
	}

	/* reserve cubby in the bsscfg container for private data */
	if ((act_frame_info->cfg_cubby_hdl =
			wlc_bsscfg_cubby_reserve(wlc, sizeof(act_frame_cubby_t *),
			wlc_act_frame_init, wlc_act_frame_deinit, NULL,
			(void *)act_frame_info)) < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* register packet class callback */
	if (wlc_pcb_fn_set(wlc->pcb, 0, WLF2_PCB1_AF, wlc_actionframetx_complete) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_pcb_fn_set(af) failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* action frame ioctls & iovars */
	if (wlc_module_register(wlc->pub, actframe_iovars, "actframe", act_frame_info,
		wlc_act_frame_doiovar, NULL, NULL, wlc_act_frame_wlc_down) != BCME_OK) {

		WL_ERROR(("wl%d: actframe wlc_module_register() failed\n",
		          wlc->pub->unit));
		goto fail;
	}

	act_frame_info->wlc = wlc;
	return act_frame_info;

fail:
	if (act_frame_info) {
		MFREE(act_frame_info->wlc->osh, act_frame_info, sizeof(wlc_act_frame_info_t));
	}
	return NULL;
}

void
BCMATTACHFN(wlc_act_frame_detach)(wlc_act_frame_info_t *act_frame_info)
{
	ASSERT(act_frame_info);
	wlc_module_unregister(act_frame_info->wlc->pub, "actframe", act_frame_info);
	MFREE(act_frame_info->wlc->osh, act_frame_info, sizeof(wlc_act_frame_info_t));
}

/* bsscfg cubby */
static int
wlc_act_frame_init(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_act_frame_info_t *act_frame_info = (wlc_act_frame_info_t *)ctx;
	wlc_info_t *wlc;
	act_frame_cubby_t **act_frame_loc;
	act_frame_cubby_t *act_frame_cubby_info;

	wlc = cfg->wlc;

	/* allocate cubby info */
	if ((act_frame_cubby_info = MALLOCZ(wlc->osh, sizeof(act_frame_cubby_t))) == NULL) {
		WL_ERROR(("wl%d: %s: MALLOC failed, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return BCME_NOMEM;
	}

	act_frame_loc = BSSCFG_ACT_FRAME_CUBBY_LOC(act_frame_info, cfg);
	*act_frame_loc = act_frame_cubby_info;

	return BCME_OK;
}

static void
wlc_act_frame_deinit(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_act_frame_info_t *act_frame_info = (wlc_act_frame_info_t *)ctx;
	wlc_info_t *wlc;
	act_frame_cubby_t *act_frame_cubby;

	wlc = act_frame_info->wlc;
	act_frame_cubby = BSSCFG_ACT_FRAME_CUBBY(act_frame_info, cfg);

	if (act_frame_cubby == NULL) {
		return;
	}

	/* Abort action frame slot, if registered */
	(void)wlc_actframe_abort_action_frame(cfg);

	MFREE(wlc->osh, act_frame_cubby, sizeof(act_frame_cubby_t));
}

static int
wlc_act_frame_wlc_down(void *ctx)
{
	wlc_act_frame_info_t *act_frame_info = (wlc_act_frame_info_t *)ctx;
	wlc_info_t *wlc = act_frame_info->wlc;
	int idx;
	wlc_bsscfg_t *cfg;

	/* Ensure all actiona frame transactions are aborted before wlc going down. */
	FOREACH_BSS(wlc, idx, cfg)	{
		wlc_actframe_abort_action_frame(cfg);
	}
	return BCME_OK;
}

/* iovar dispatch */
static int
wlc_act_frame_doiovar(void *ctx, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif)
{
	wlc_act_frame_info_t *act_frame_info = (wlc_act_frame_info_t *)ctx;
	wlc_info_t *wlc;
	act_frame_cubby_t *actframe_cubby;

	int err = BCME_OK;
	wlc_bsscfg_t *bsscfg;

	ASSERT(act_frame_info != NULL);
	wlc = act_frame_info->wlc;
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	actframe_cubby = BSSCFG_ACT_FRAME_CUBBY(act_frame_info, bsscfg);
	ASSERT(actframe_cubby != NULL);

	/* Do the actual parameter implementation */
	switch (actionid) {

	case IOV_SVAL(IOV_ACTION_FRAME):
		if (wlc->pub->up) {
			/* Send frame to remote client/server */
			err = wlc_send_action_frame(wlc, bsscfg, NULL, arg);
		} else {
			err = BCME_NOTUP;
		}
		break;
	case IOV_SVAL(IOV_AF): {
		bool af_continue = FALSE;
		bool do_event = TRUE;
		wl_af_params_t *af = NULL;
		wlc_msch_req_param_t *req = NULL;

		af = MALLOC(wlc->osh, WL_WIFI_AF_PARAMS_SIZE);
		if (!af) {
			WL_ERROR(("wl%d: %s: af malloc failed\n", wlc->pub->unit, __FUNCTION__));
			err = BCME_NOMEM;
			break;
		}
		memcpy(af, arg, len);

		do {
#ifdef WLRSDB
			/* get the corresponding wlc */
			if (RSDB_ENAB(wlc->pub) &&
				WLC_RSDB_DUAL_MAC_MODE(WLC_RSDB_CURR_MODE(wlc))) {
				if (af->channel != 0) {
					wlc_info_t * wlc_2g, *wlc_5g;
					wlc_rsdb_get_wlcs(wlc, &wlc_2g, &wlc_5g);

					/* Note - The below code overwrite the wlc.
					 * Any wlc related code should be added
					 * after #endif WLRSDB.
					 */
					if (CHSPEC_IS2G(CH20MHZ_CHSPEC(af->channel))) {
						wlc = wlc_2g;
					} else {
						wlc = wlc_5g;
					}
				}
			}
#endif /* WLRSDB */
			ASSERT(wlc != NULL);
			/* use scan to send AF for quiet chanspec */
			if (wlc_quiet_chanspec(wlc->cmi, CH20MHZ_CHSPEC(af->channel))) {
				wlc_act_frame_txaf_scan(wlc,
					bsscfg, actframe_cubby, af, &do_event);
				break;
			}

			/* piggyback on current MSCH channel */
			if (af->channel == wf_chspec_ctlchan(WLC_BAND_PI_RADIO_CHANSPEC) ||
				af->channel == 0) {
				af_continue = TRUE;
			}

			if (WLFMC_ENAB(wlc->pub) && af->channel == 0) {
				af_continue = TRUE;
			}

			if (af->dwell_time < 0) {
				af->dwell_time = WLC_AF_DEFAULT_DWELL_TIME;
			}

			if (!af_continue) {
				actframe_cubby->is_onchan_af = FALSE;

				if (af->channel == 0) {
					af->channel =
						wf_chspec_ctlchan(WLC_BAND_PI_RADIO_CHANSPEC);
				}
				/* Don`t process another action frame request
				 * till previous one is completed
				 */
				if (wlc_af_inprogress(wlc, bsscfg)) {
					WL_ERROR(("Action frame is in progress\n"));
					err = BCME_BUSY;
					break;
				}

				actframe_cubby->scanInUse = FALSE;
				err = wlc_send_action_frame_off_channel(wlc, bsscfg,
					CH20MHZ_CHSPEC(af->channel), CHANSW_IOVAR,
					af->dwell_time, &af->BSSID, &af->action_frame, NULL, NULL);
				do_event = FALSE;
				break;
			} else {
				actframe_cubby->is_onchan_af = TRUE;
			}

			if (!wlc->pub->up) {
				err = BCME_NOTUP;
				break;
			}

			/* If current channel is a 'quiet' channel then reject */
			if (BSS_QUIET_STATE(wlc->quiet, bsscfg) & SILENCE) {
				WL_ERROR(("BSS in Quiet Channel\n"));
				err = BCME_NOTREADY;
				break;
			}

			/* Send frame to remote client/server */
			err = wlc_send_action_frame(wlc, bsscfg, &af->BSSID, &af->action_frame);

			/* update AF dwell time for MSCH based AF piggyback */
			if (!err && wlc_af_inprogress(wlc, bsscfg) && !actframe_cubby->scanInUse) {
				req = &actframe_cubby->req;
				req->duration = MS_TO_USEC(af->dwell_time);
				if (wlc_msch_timeslot_update(wlc->msch_info,
					actframe_cubby->req_msch_actframe_hdl,
					req, MSCH_UPDATE_END_TIME) != BCME_OK) {
					break;
				}
			}
			do_event = FALSE;
		} while (FALSE);

		if (do_event) {
			/* Send back the Tx status to the host */
			wlc_bss_mac_event(wlc, bsscfg, WLC_E_ACTION_FRAME_COMPLETE,
				NULL, WLC_E_STATUS_NO_ACK, 0, 0,
				&af->action_frame.packetId, sizeof(af->action_frame.packetId));
		}
		MFREE(wlc->osh, af, WL_WIFI_AF_PARAMS_SIZE);
		break;
	}
	case IOV_SVAL(IOV_AF_ABORT): {
		int32 int_val = 0;
		bool bool_val;
		if (p_len >= (int)sizeof(int_val))
			memcpy(&int_val, params, sizeof(int_val));
		bool_val = (int_val != 0) ? TRUE : FALSE;
		if (bool_val) {
			err = wlc_actframe_abort_action_frame(bsscfg);
		}
		break;
	}
	default:
		WL_ERROR(("wl%d: %s: Command unsupported", wlc->pub->unit, __FUNCTION__));
		err = BCME_UNSUPPORTED;
		break;
	}
	return err;
}

static void
wlc_actframe_action_frame_msch_slot_end(wlc_info_t *wlc, wlc_bsscfg_t *cfg, int status)
{
	uint32 packetId;
	act_frame_cubby_t *actframe_cubby;
	ASSERT(cfg);
	actframe_cubby = BSSCFG_ACT_FRAME_CUBBY(cfg->wlc->wlc_act_frame_info, cfg);
	if (actframe_cubby->action_frame) {
		packetId = WLPKTTAG(actframe_cubby->action_frame)->shared.packetid;
		wlc_bss_mac_event(wlc, cfg, WLC_E_ACTION_FRAME_COMPLETE, NULL,
			status, 0, 0, &packetId, sizeof(packetId));
		/* Free any un-queued frames. Any queued frames will be freed as part of
		 * txcomplete processing
		 */
		if (!actframe_cubby->af_queued) {
			PKTFREE(wlc->osh, actframe_cubby->action_frame, TRUE);
		}
		actframe_cubby->af_queued = FALSE;
		actframe_cubby->action_frame = NULL;
	}
	wlc_bss_mac_event(wlc, cfg, WLC_E_ACTION_FRAME_OFF_CHAN_COMPLETE,
		NULL, 0, 0, 0, 0, 0);
#if defined(WLRSDB) && defined(WLMCNX)
	if (RSDB_ENAB(wlc->pub) && MCNX_ENAB(wlc->pub) && wlc != cfg->wlc) {
		wlc_mcnx_alt_ra_unset(wlc->mcnx, cfg);
	}
#endif /* defined(WLRSDB) && defined(WLMCNX) */
	actframe_cubby->wlc_handler = NULL;

	wlc_txqueue_end(wlc, NULL, NULL);

#ifdef STA
	wlc_mpc_off_req_set(wlc, MPC_OFF_REQ_TX_ACTION_FRAME, 0);
#endif /* STA */

	/* Allow uCode/d11 to sleep */
	wlc_user_wake_upd(wlc, WLC_USER_WAKE_REQ_AF, FALSE);
}

int
wlc_actframe_abort_action_frame(wlc_bsscfg_t *cfg)
{
	act_frame_cubby_t *actframe_cubby;
	wlc_info_t *wlc;
	int err = BCME_OK;
	int status = WLC_E_STATUS_NO_ACK;
	ASSERT(cfg);
	actframe_cubby = BSSCFG_ACT_FRAME_CUBBY(cfg->wlc->wlc_act_frame_info, cfg);
	wlc = (wlc_info_t*)actframe_cubby->wlc_handler;

	/* Internal wlc and msch request handles are NULL only when slot end has already been
	 * given by MSCH
	 */
	if (!actframe_cubby->req_msch_actframe_hdl && !wlc) {
		return err;
	}
	ASSERT(actframe_cubby->req_msch_actframe_hdl);
	ASSERT(wlc);

	if (ACTION_FRAME_IN_PROGRESS(actframe_cubby)) {
		status = WLC_E_STATUS_ABORT;
	}

	if (wlc_msch_timeslot_unregister(wlc->msch_info,
		&actframe_cubby->req_msch_actframe_hdl) != BCME_OK) {
		WL_ERROR(("wl%d: %s: MSCH timeslot unregister failed\n",
			wlc->pub->unit, __FUNCTION__));
		err = BCME_ERROR;
		return err;
	}
	actframe_cubby->req_msch_actframe_hdl = NULL;
	wlc_actframe_action_frame_msch_slot_end(wlc, cfg, status);
	return err;
}

/**
 * Called back by the multichannel scheduler (msch). This is used to send off-channel action frames,
 * in which case they're pre-association (e.g. GON, Public action, etc.). There's no BSS here.
 */
static int
wlc_actframe_send_action_frame_cb(void* handler_ctxt, wlc_msch_cb_info_t *cb_info)
{
	wlc_bsscfg_t *cfg = (wlc_bsscfg_t *)handler_ctxt;
	act_frame_cubby_t *actframe_cubby;
	wlc_info_t *wlc;
	ASSERT(cfg);
	actframe_cubby = BSSCFG_ACT_FRAME_CUBBY(cfg->wlc->wlc_act_frame_info, cfg);

	wlc = (wlc_info_t*)actframe_cubby->wlc_handler;
	ASSERT(wlc);

	if (cb_info->type & MSCH_CT_ON_CHAN) {
		wlc_txqueue_start(wlc, NULL, cb_info->chanspec, NULL);
	}

	if (cb_info->type & MSCH_CT_SLOT_START) {

		if (!ACTION_FRAME_IN_PROGRESS(actframe_cubby)) {
			WL_ERROR(("wl%d: Action frame in not progress\n", wlc->pub->unit));
			goto fail;
		}

		/* Keep uCode/d11 to awake */
		wlc_user_wake_upd(wlc, WLC_USER_WAKE_REQ_AF, TRUE);

#ifdef WLP2P
		if (BSS_P2P_ENAB(wlc, cfg)) {
			/* Set time of expiry over dwell time(ms) */
			wlc_lifetime_set(wlc, actframe_cubby->action_frame,
				(actframe_cubby->af_dwell_time*2)*1000);
		}
#endif // endif
#if defined(WLRSDB) && defined(WLMCNX)
		/* If we are using wlc which is not hosting the bsscfg, then program the AMT
		 * to receive response action frame with da as bsscfg->cur_etheraddr
		 */
		if (RSDB_ENAB(wlc->pub) && MCNX_ENAB(wlc->pub) && wlc != cfg->wlc) {
			wlc_mcnx_alt_ra_set(wlc->mcnx, cfg);
		}
#endif /* defined(WLRSDB) && defined(WLMCNX) */

		if (actframe_cubby->af_tx_cb) {
			actframe_cubby->af_tx_cb(wlc, actframe_cubby->arg,
				actframe_cubby->action_frame);
		} else {
			if (wlc_tx_action_frame_now(wlc, cfg, actframe_cubby->action_frame,
				NULL) != BCME_OK) {
				WL_ERROR(("wl%d: Could not send out action frame\n",
					wlc->pub->unit));
				actframe_cubby->action_frame = NULL;
				goto fail;
			}
			/* Mark that frame has been queued. This is inturn useful while freeing
			 * frame while aborting AF timeslot
			 */
			actframe_cubby->af_queued = TRUE;
		}
	}

	if (cb_info->type &  MSCH_CT_SLOT_END) {
		ASSERT(cb_info->type & MSCH_CT_REQ_END);
		/* Resetting local reference of msch handle here. This is done to ensure that
		 * IOV_AF_ABORT returns cleanly without any processing
		 */
		actframe_cubby->req_msch_actframe_hdl = NULL;
		/* If we are using wlc which is not hosting the bsscfg, then program the AMT
		 * to receive response action frame with da as bsscfg->cur_etheraddr
		 */
		wlc_actframe_action_frame_msch_slot_end(wlc, cfg, WLC_E_STATUS_NO_ACK);
	}

	return BCME_OK;

fail:
	if (actframe_cubby->action_frame) {
		PKTFREE(wlc->osh, actframe_cubby->action_frame, TRUE);
		actframe_cubby->action_frame = NULL;
	}
	wlc_txqueue_end(wlc, NULL, NULL);
	if (actframe_cubby->req_msch_actframe_hdl) {
		if (wlc_msch_timeslot_unregister(wlc->msch_info,
			&actframe_cubby->req_msch_actframe_hdl) != BCME_OK) {
			WL_ERROR(("wl%d: %s: MSCH timeslot unregister failed\n",
				wlc->pub->unit, __FUNCTION__));
		}
		actframe_cubby->req_msch_actframe_hdl = NULL;
	}
	actframe_cubby->wlc_handler = NULL;

#ifdef STA
	/* Bringup down the core once done */
	wlc_mpc_off_req_set(wlc, MPC_OFF_REQ_TX_ACTION_FRAME, 0);
#endif /* STA */
	/* Allow uCode/d11 to sleep */
	wlc_user_wake_upd(wlc, WLC_USER_WAKE_REQ_AF, FALSE);

	return BCME_ERROR;
}

static void *
wlc_prepare_action_frame(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
                         const struct ether_addr *bssid, void *action_frame)
{
	void *pkt;
	uint8* pbody;
	uint16 body_len;
	struct ether_addr da;
	uint32 packetId;
	wlc_pkttag_t *pkttag;
	uint8 mfp;
	struct scb *scb;

	memcpy(&packetId, (uint8*)action_frame +
		OFFSETOF(wl_action_frame_t, packetId), sizeof(uint32));

	memcpy(&body_len, (uint8*)action_frame + OFFSETOF(wl_action_frame_t, len),
		sizeof(body_len));

	memcpy(&da, (uint8*)action_frame + OFFSETOF(wl_action_frame_t, da), ETHER_ADDR_LEN);

	/* set a3 to current bss if a2 in the same bss or
	   wildcard bssid if not
	*/

	if (!ETHER_ISMULTI(&da) &&
			((scb = wlc_scbfind(wlc, cfg, (const struct ether_addr *)&da)) != NULL) &&
			(SCB_ASSOCIATED(scb) || BSSCFG_SLOTTED_BSS(cfg))) {
		mfp = SCB_MFP(scb);
		bssid = &cfg->BSSID;
	} else {
		mfp = 0;
		bssid = &ether_bcast;
	}

	pkt = NULL;
	if (body_len) {
		uint8 *body;

		body = (uint8*)action_frame + OFFSETOF(wl_action_frame_t, data) +
		        DOT11_ACTION_CAT_OFF;

		/* set as dual protected if appropriate */
		wlc_set_protected_dual_publicaction(body, mfp, cfg);

		/*  get action frame */
		if ((pkt = wlc_frame_get_action(wlc, &da, &cfg->cur_etheraddr,
		                                bssid, body_len, &pbody, body[0])) == NULL) {
			return NULL;
		}

		pkttag = WLPKTTAG(pkt);
		pkttag->shared.packetid = packetId;
		WLPKTTAGBSSCFGSET(pkt, cfg->_idx);

		memcpy(pbody, (uint8*)action_frame + OFFSETOF(wl_action_frame_t, data), body_len);
	}

	return pkt;
}

/*
Per specs, some public action frames must be protected while mfp is active.
transform public action from/to protected dual pubiblic action frame
based on action type and mfp state.
 */
void
wlc_set_protected_dual_publicaction(uint8 *action_frame, uint8 mfp, wlc_bsscfg_t *bsscfg)
{
	uint8 *cat;
	uint8 act;

	cat = action_frame + DOT11_ACTION_CAT_OFF;
	act =  cat[1];

	/* potential PDPA that we support are channel switch and GAS frames.
	   There are more in the specs that would need to be added if supported in the future.
	*/
	if (*cat == DOT11_ACTION_CAT_PUBLIC || *cat == DOT11_ACTION_CAT_PDPA) {

		if (mfp && wlc_is_protected_dual_publicaction(act, bsscfg))
		{

			*cat  = DOT11_ACTION_CAT_PDPA;
		}
		else {
			*cat = DOT11_ACTION_CAT_PUBLIC;
		}
	}
}

int
wlc_tx_action_frame_now(wlc_info_t *wlc, wlc_bsscfg_t *cfg, void *pkt, struct scb *scb)
{
	ratespec_t rate_override = 0;
	struct dot11_management_header *hdr;
	hdr = (struct dot11_management_header *)PKTDATA(wlc->osh, pkt);

	if (!ETHER_ISMULTI(&hdr->da) && !scb) {
		scb = wlc_scbfind(wlc, cfg, &hdr->da);
		if (scb && !SCB_ASSOCIATED(scb))
			scb = NULL;
	}
	if (BSS_P2P_ENAB(wlc, cfg)) {
		rate_override = OFDM_RSPEC(WLC_RATE_6M);
	} else {
		rate_override = WLC_LOWEST_BAND_RSPEC(wlc->band);
	}

	/* register packet callback */
	WLF2_PCB1_REG(pkt, WLF2_PCB1_AF);

	if (!wlc_queue_80211_frag(wlc, pkt, wlc->active_queue, scb, cfg,
		FALSE, NULL, rate_override)) {
		WL_ERROR(("%s, wlc_queue_80211_frag failed", __FUNCTION__));
		return BCME_ERROR;
	}

	return BCME_OK;
} /* wlc_tx_action_frame_now */

int
wlc_send_action_frame_off_channel(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, chanspec_t chanspec,
	int reason, int32 dwell_time, struct ether_addr *bssid, wl_action_frame_t *action_frame,
	wlc_act_frame_tx_cb_fn_t cb, void *arg)
{
	chanspec_t chanspec_list[1];
	int err = BCME_OK;
	wlc_msch_req_param_t req_param;
	act_frame_cubby_t *act_frame_cubby;

	act_frame_cubby = BSSCFG_ACT_FRAME_CUBBY(wlc->wlc_act_frame_info, bsscfg);

	ASSERT(act_frame_cubby->wlc_handler == NULL);
	ASSERT(act_frame_cubby->req_msch_actframe_hdl == NULL);
	act_frame_cubby->action_frame = wlc_prepare_action_frame(wlc, bsscfg, bssid, action_frame);

	if (!act_frame_cubby->action_frame) {
		ACTFRAME_MESSAGE(("%s, action_frame is NULL\n", __FUNCTION__));
		wlc_bss_mac_event(wlc, bsscfg, WLC_E_ACTION_FRAME_COMPLETE,
			NULL, WLC_E_STATUS_NO_ACK, 0, 0,
			&action_frame->packetId, sizeof(action_frame->packetId));
		return BCME_NOMEM;
	}

#ifdef STA
	/* Bringup the core for the duration of off channel Action Frame Event */
	wlc_mpc_off_req_set(wlc, MPC_OFF_REQ_TX_ACTION_FRAME, MPC_OFF_REQ_TX_ACTION_FRAME);
#endif /* STA */

	act_frame_cubby->af_tx_cb = cb;
	act_frame_cubby->arg = arg;
	act_frame_cubby->wlc_handler = wlc;
	act_frame_cubby->af_queued = FALSE;

	chanspec_list[0] = chanspec;

	bzero(&req_param, sizeof(req_param));
	req_param.req_type = MSCH_RT_START_FLEX;
	req_param.duration = MS_TO_USEC(dwell_time);
	req_param.priority = MSCH_DEFAULT_PRIO;

	act_frame_cubby->af_dwell_time = dwell_time;

	act_frame_cubby->scanInUse = FALSE;
	if ((err = wlc_msch_timeslot_register(wlc->msch_info,
		&chanspec_list[0], 1, wlc_actframe_send_action_frame_cb, bsscfg, &req_param,
		&act_frame_cubby->req_msch_actframe_hdl)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: MSCH timeslot register failed, err %d\n",
			wlc->pub->unit, __FUNCTION__, err));

		if (act_frame_cubby->action_frame) {
			PKTFREE(wlc->osh, act_frame_cubby->action_frame, TRUE);
			act_frame_cubby->action_frame = NULL;
		}
	} else {
		wlc_msch_set_chansw_reason(wlc->msch_info, act_frame_cubby->req_msch_actframe_hdl,
			reason);
	}
	return err;
}

int
wlc_send_action_frame(wlc_info_t *wlc, wlc_bsscfg_t *cfg, const struct ether_addr *bssid,
	void *action_frame)
{
	void *pkt = wlc_prepare_action_frame(wlc, cfg, bssid, action_frame);

	if (!pkt) {
		return BCME_NOMEM;
	}

	return wlc_tx_action_frame_now(wlc, cfg, pkt, NULL);
}

static void
wlc_af_send(wlc_info_t *wlc, void *arg, uint *dwell)
{
	wlc_bsscfg_t *bsscfg = (wlc_bsscfg_t *)arg;
	act_frame_cubby_t *act_frame_cubby;

	BCM_REFERENCE(dwell);
	if (!ANY_SCAN_IN_PROGRESS(wlc->scan))
		return;

	act_frame_cubby = BSSCFG_ACT_FRAME_CUBBY(wlc->wlc_act_frame_info, bsscfg);

	if (!act_frame_cubby->action_frame) {
		ACTFRAME_MESSAGE(("%s, action_frame is NULL\n", __FUNCTION__));
		ASSERT(0);
		return;
	}
	wlc_tx_action_frame_now(wlc, bsscfg, act_frame_cubby->action_frame, NULL);
	act_frame_cubby->af_queued = TRUE;
}

static void
wlc_actionframetx_complete(wlc_info_t *wlc, void *pkt, uint txstatus)
{
	int err;
	int idx;
	wlc_bsscfg_t *cfg;
	uint status = WLC_E_STATUS_SUCCESS;
	uint32 packetId;
	bool af_evt_sent = FALSE;
	act_frame_cubby_t *actframe_cubby;

	if (!(txstatus & TX_STATUS_ACK_RCV)) {
		status = WLC_E_STATUS_NO_ACK;
	}
	packetId = WLPKTTAG(pkt)->shared.packetid;

	idx = WLPKTTAGBSSCFGGET(pkt);
	if ((cfg = wlc_bsscfg_find(wlc, idx, &err)) == NULL) {
		WL_ERROR(("wl%d: %s, wlc_bsscfg_find failed\n",
			wlc->pub->unit, __FUNCTION__));
		return;
	}

	/* Check packet header is valid, if not set hdrSize as zero */
	if ((WLPKTTAG(pkt)->flags & WLF_TXHDR) == 0) {
		WL_ERROR(("wl%d.%d: %s: pkt header not fully formed.\n",
			WLCWLUNIT(wlc), idx, __FUNCTION__));
	}

	actframe_cubby = BSSCFG_ACT_FRAME_CUBBY(wlc->wlc_act_frame_info, cfg);
	if (actframe_cubby->action_frame) {
		/* Don`t process action frame further if:
		 * - Slot End has already been processed or
		 * - Action frame txcomplete is from previous slots action frame
		 */
		if ((!actframe_cubby->scanInUse && !actframe_cubby->req_msch_actframe_hdl) ||
			(actframe_cubby->action_frame != pkt)) {
			WL_ERROR(("%s, Not processing action frame, msch-hdl=%p, pkt=%p, "
				"actframe=%p\n", __FUNCTION__,
				actframe_cubby->req_msch_actframe_hdl, pkt,
				actframe_cubby->action_frame));
			return;
		}
	}

	if (actframe_cubby->action_frame || wlc_act_frame_isonchan(wlc, cfg)) {
		/* Send back the Tx status to the host */
		if (!af_evt_sent) {
			wlc_bss_mac_event(wlc, cfg, WLC_E_ACTION_FRAME_COMPLETE, NULL, status,
				0, 0, &packetId, sizeof(packetId));
		}
		if (actframe_cubby->action_frame) {
			actframe_cubby->action_frame = NULL;
			actframe_cubby->af_queued = FALSE;
		}
	}

}

/** Called back by the multichannel scheduler (msch) */
static int
wlc_msch_af_send(void* handler_ctxt, wlc_msch_cb_info_t *cb_info)
{
	wlc_bsscfg_t *bsscfg = (wlc_bsscfg_t *)handler_ctxt;
	act_frame_cubby_t *actframe_cubby;
	ASSERT(bsscfg);

	actframe_cubby = BSSCFG_ACT_FRAME_CUBBY(bsscfg->wlc->wlc_act_frame_info, bsscfg);

	if (cb_info->type & MSCH_CT_SLOT_START) {
		if (actframe_cubby->cb_func) {
			actframe_cubby->cb_func(bsscfg->wlc,
				actframe_cubby->cb_ctxt, (uint *)(&actframe_cubby->af_dwell_time));
		}
	}
	if (cb_info->type &  MSCH_CT_SLOT_END) {
		ASSERT(cb_info->type & MSCH_CT_REQ_END);
	}
	return BCME_OK;
}

int
wlc_msch_actionframe(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint32 channel, int reason,
	uint32 dwell_time, struct ether_addr *bssid, wlc_actframe_callback cb_func, void *arg)
{
	chanspec_t chanspec_list[1];
	int err = BCME_OK;
	wlc_msch_req_param_t req_param;
	act_frame_cubby_t *act_frame_cubby;
	act_frame_cubby = BSSCFG_ACT_FRAME_CUBBY(wlc->wlc_act_frame_info, bsscfg);

	chanspec_list[0] = (channel == 0)? WLC_BAND_PI_RADIO_CHANSPEC :
		CH20MHZ_CHSPEC(channel);

	bzero(&req_param, sizeof(req_param));
	req_param.req_type = MSCH_RT_START_FLEX;
	req_param.duration = dwell_time;
	req_param.priority = MSCH_DEFAULT_PRIO;

	act_frame_cubby->af_dwell_time = dwell_time;
	act_frame_cubby->cb_func = cb_func;
	act_frame_cubby->cb_ctxt = arg;
	if ((err = wlc_msch_timeslot_register(wlc->msch_info,
		&chanspec_list[0], 1, wlc_msch_af_send, bsscfg, &req_param,
		&act_frame_cubby->req_msch_actframe_hdl)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: MSCH timeslot register failed, err %d\n",
			wlc->pub->unit, __FUNCTION__, err));
	} else {
		wlc_msch_set_chansw_reason(wlc->msch_info, act_frame_cubby->req_msch_actframe_hdl,
			reason);
	}
	return err;
}

/*
 * See the table in the 802.11 spec called
 * Public Action field values defined for Protected Dual of Public Action frames
 */
bool
wlc_is_protected_dual_publicaction(uint8 act, wlc_bsscfg_t *bsscfg)
{
	bool is_protected = FALSE;
	BCM_REFERENCE(bsscfg);
	/* potential PDPA that we support are channel switch and GAS frames.
	 * There are more in the specs that would need to be added
	 * if supported in the future.
	*/
	if ((act == DOT11_PUB_ACTION_CHANNEL_SWITCH ||
	     /* GAS frame */
	     (act >= GAS_REQUEST_ACTION_FRAME &&
	      act <= GAS_COMEBACK_RESPONSE_ACTION_FRAME)))
	{
		is_protected = TRUE;
	}
#if defined(WL_PROXDETECT) && defined(WL_FTM)
	/* secure ranging check */
	else if (act == DOT11_PUB_ACTION_FTM_REQ ||
			act == DOT11_PUB_ACTION_FTM) {
		is_protected = WLC_BSSCFG_SECURE_FTM(bsscfg);
	}
#endif // endif

	return is_protected;
}

/*
test if the frame is a valid public action. return
valid : 1
invalid : -1
not a public action : 0
*/
int
wlc_is_publicaction(wlc_info_t * wlc, struct dot11_header *hdr, int len,
                    struct scb *scb, wlc_bsscfg_t *bsscfg)
{
	uint8 cat;
	uint8 act;
	BCM_REFERENCE(wlc);
	if (len < (DOT11_MGMT_HDR_LEN + DOT11_ACTION_HDR_LEN))
		return 0;

	cat = *((uint8 *)hdr + DOT11_MGMT_HDR_LEN);	/* peek for category field */
	act = *((uint8 *)hdr + DOT11_MGMT_HDR_LEN + 1);	/* peek for action field */

	/* both PUBLIC and PDPA are OK */
	if (cat == DOT11_ACTION_CAT_PUBLIC || cat == DOT11_ACTION_CAT_PDPA) {

		if (scb != NULL) {
			/* By spec, if the transmitter is in our BSS, a3 must be the current bssid
			   otherwise, it must be wildcard. See "Public Action frame addressing"
			   However, enforcing the wildcard literally
			   could create backward compatibility issues.
			   Here we will just verify that, if in same BSS, the a3 and bssid match.
			*/
			if (SCB_ASSOCIATED(scb) && eacmp(&bsscfg->BSSID, &hdr->a3) != 0) {
				WL_ERROR(("wl%d: %s: A3 does not match BSSID\n",
					wlc->pub->unit, __FUNCTION__));
				return -1;
			}

			if (SCB_MFP(scb)) {
				if (cat == DOT11_ACTION_CAT_PUBLIC) {
					/* fail if MFP and pub action and needing protection */
					if (wlc_is_protected_dual_publicaction(act, bsscfg)) {
						WL_ERROR(("wl%d: %s: rx frame has category %d,  "
							"pub action field %d; should have cat %d\n",
							wlc->pub->unit, __FUNCTION__, cat,
							act, DOT11_ACTION_CAT_PDPA));
						return -1;
					}
				} else { /* pdpa */
					/* fail if MFP and PDPA but not needing protection */
					if (!wlc_is_protected_dual_publicaction(act, bsscfg)) {
						WL_ERROR(("wl%d: %s: rx frame has category %d, "
							"pub action field %d; should have cat %d\n",
							wlc->pub->unit, __FUNCTION__, cat,
							act, DOT11_ACTION_CAT_PUBLIC));
						return -1;
					}
				}
			} else { /* !mfp */
				/* fail if not MFP and PDPA */
				if (cat == DOT11_ACTION_CAT_PDPA) {
					WL_ERROR(("wl%d: %s: rx frame has category %d, "
						"pub action field %d; should have cat %d\n",
						wlc->pub->unit, __FUNCTION__, cat,
						act, DOT11_ACTION_CAT_PUBLIC));
					return -1;
				}
			}
		}
		return 1;
	}
#ifdef WL_PROXDETECT
	if (PROXD_ENAB(wlc->pub) &&
	    (cat == DOT11_ACTION_CAT_UWNM || cat == DOT11_ACTION_CAT_WNM)) {
	            return 1;
	    }
#endif // endif
	return 0;
}

/* AF tx succeeds but dwell time is still valid */
bool
wlc_af_dwelltime_inprogress(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	act_frame_cubby_t *act_frame_cubby = BSSCFG_ACT_FRAME_CUBBY(wlc->wlc_act_frame_info, cfg);
	ASSERT(act_frame_cubby != NULL);
	return ((!ACTION_FRAME_IN_PROGRESS(act_frame_cubby)) &&
		act_frame_cubby->req_msch_actframe_hdl);
}

bool
wlc_af_inprogress(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	act_frame_cubby_t *act_frame_cubby = BSSCFG_ACT_FRAME_CUBBY(wlc->wlc_act_frame_info, cfg);
	ASSERT(act_frame_cubby != NULL);
	return (ACTION_FRAME_IN_PROGRESS(act_frame_cubby) ||
		act_frame_cubby->req_msch_actframe_hdl);
}

static bool
wlc_act_frame_isonchan(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	act_frame_cubby_t *act_frame_cubby = BSSCFG_ACT_FRAME_CUBBY(wlc->wlc_act_frame_info, cfg);
	ASSERT(act_frame_cubby != NULL);
	return act_frame_cubby->is_onchan_af;
}

static int
wlc_act_frame_txaf_scan(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, act_frame_cubby_t *actframe_cubby,
	wl_af_params_t *af, bool *do_event)
{
	/* TODO: AF piggyback for sending AF via scan */
	chanspec_t chanspec_list = CH20MHZ_CHSPEC(af->channel);
	wlc_ssid_t ssid;
	int err = BCME_OK;

	if (ANY_SCAN_IN_PROGRESS(wlc->scan)) {
		return BCME_BUSY;
	}

	ssid.SSID_len = 0;
	actframe_cubby->scanInUse = TRUE;
	actframe_cubby->action_frame =
		wlc_prepare_action_frame(wlc, bsscfg, &af->BSSID, &af->action_frame);

	if (!actframe_cubby->action_frame) {
		return BCME_NOMEM;
	}
	actframe_cubby->af_queued = FALSE;

	if (bsscfg->wlc != wlc) {
		/* Set the right cfg index if we are using other wlc  */
		WLPKTTAGBSSCFGSET(actframe_cubby->action_frame, wlc->cfg->_idx);
	}

	err = wlc_scan_request_ex(wlc, DOT11_BSSTYPE_ANY, &af->BSSID,
			1, &ssid, -1, 1, af->dwell_time, af->dwell_time,
			-1, &chanspec_list, 1, 0, FALSE, wlc_af_off_channel_callback, wlc,
			WLC_ACTION_SCAN,
			0, bsscfg, wlc_af_send, bsscfg);

	if (err != BCME_OK) {
		WL_ERROR(("%s(): wlc_scan_request_ex() failed,"
			"err = %d\n", __FUNCTION__, err));
	}

	*do_event = FALSE;
	return err;
}

static void
wlc_af_off_channel_callback(void * arg, int status, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = (wlc_info_t *)arg;
	act_frame_cubby_t *actframe_cubby = BSSCFG_ACT_FRAME_CUBBY(wlc->wlc_act_frame_info, cfg);
	/* if AF is aborted */
	if (actframe_cubby->action_frame) {
		wlc_actionframetx_complete(wlc, actframe_cubby->action_frame, TX_STATUS_NO_ACK);
		actframe_cubby->action_frame = NULL;
		actframe_cubby->af_queued = FALSE;
	}
#ifdef WLRSDB
	/* If we used wlc which is not hosting the cfg, to transmit action frame,
	* unset the already programmed rcmta.
	*/
	if (RSDB_ENAB(wlc->pub) && wlc != cfg->wlc) {
#ifdef WLMCNX
		if (MCNX_ENAB(wlc->pub) && (cfg != wlc_bsscfg_primary(cfg->wlc)))
			wlc_mcnx_ra_unset(wlc->mcnx, cfg);
#endif // endif
	}
#endif /* WLRSDB */

	actframe_cubby->scanInUse = FALSE;
	/* Send back the channel switch indication to the host */
	wlc_bss_mac_event(wlc, cfg, WLC_E_ACTION_FRAME_OFF_CHAN_COMPLETE, NULL, status,
	                  0, 0, 0, 0);
}
