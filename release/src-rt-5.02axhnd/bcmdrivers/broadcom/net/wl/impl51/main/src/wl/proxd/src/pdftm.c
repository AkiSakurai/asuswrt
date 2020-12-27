/*
 * Proxd FTM method implementation. See twiki FineTimingMeasurement.
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
 * $Id: pdftm.c 777286 2019-07-25 19:43:30Z $
 */

#include "pdftmpvt.h"

static int wlc_ftm_parse_ext_cap_ie(void *ctx, wlc_iem_parse_data_t *data);

static void
ftm_scb_state_upd_cb(void *ctx, scb_state_upd_data_t *notif_data)
{
	pdftm_t *ftm =  (pdftm_t *) ctx;
	pdftm_session_t *sn = NULL;
	struct scb *scb;
	wl_proxd_session_id_t sid;
	int err = BCME_OK;

	BCM_REFERENCE(err);
	ASSERT(notif_data != NULL);

	scb = notif_data->scb;
	ASSERT(scb != NULL);

	sn = FTM_SESSION_FOR_PEER(ftm, &scb->ea, WL_PROXD_SESSION_FLAG_INITIATOR);
	if (!sn)
		goto done;

	sid = sn->sid;
	if (!FTM_SESSION_SECURE(sn)) {
		ASSERT(!(sn->flags & FTM_SESSION_KEY_WAIT));
		goto done;
	}

	if (((sn->state > WL_PROXD_SESSION_STATE_START_WAIT) &&
		(sn->state < WL_PROXD_SESSION_STATE_STOPPING)) &&
		!(SCB_AUTHORIZED(scb))) {
		(void)wlc_ftm_stop_session(ftm, sid, WL_PROXD_E_SEC_NOKEY);
		err = WL_PROXD_E_SEC_POLICY;
		goto done;
	}

	if (sn->state != WL_PROXD_SESSION_STATE_START_WAIT) {
		goto done;
	}

	if (!SCB_AUTHORIZED(scb) || !(sn->flags & FTM_SESSION_KEY_WAIT)) {
		goto done;
	}

	sn->flags &= ~FTM_SESSION_KEY_WAIT;
	err = pdftm_sched_session(ftm, sn);

done:
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: done with status %d\n",
		FTM_UNIT(ftm), __FUNCTION__, err)));
}

static void
pdftm_chansw_cb(void *ctx, wlc_chansw_notif_data_t *data)
{
	pdftm_t *ftm = ctx;
	pdftm_cmn_t *ftm_cmn = ftm->ftm_cmn;
	int i;

	/* Check if any session is in burststate */
	if (!ftm_cmn->sched->in.burst) {
		return;
	}
	for (i = 0; i < ftm_cmn->max_sessions; ++i) {
		pdftm_session_t *sn = ftm_cmn->sessions[i];
		if (!FTM_VALID_SESSION(sn)) {
			continue;
		}
		/* check session is in burst state and is on the same WLC as
		* of change/set chanspec
		*/
		if ((sn->state == WL_PROXD_SESSION_STATE_BURST) &&
			(sn->ftm == ftm) && sn->ftm_state &&
			sn->ftm_state->burst) {
			FTM_INFO(("Stopping session sid %d due to CHANSW - oldchanspec %x"
				" new chanspec %x\n", sn->sid, data->old_chanspec,
				data->new_chanspec));
			sn->flags &= ~FTM_SESSION_ON_CHAN;
			if (FTM_SESSION_IS_INITIATOR(sn)) {
				pdburst_cancel(sn->ftm_state->burst, WL_PROXD_E_NOAVAIL);
			} else {
				sn->status = WL_PROXD_E_NOAVAIL;
				(void)pdftm_change_session_state(sn, BCME_OK,
					WL_PROXD_SESSION_STATE_STOPPING);
			}
		}
	}
}

pdftm_t *
BCMATTACHFN(pdftm_attach)(wlc_info_t *wlc, pdsvc_t *pd)
{
	pdftm_t *ftm;
	pdftm_cmn_t *ftm_cmn = NULL;
	int err = BCME_OK;

	ASSERT(wlc != NULL && pd != NULL);

	ftm = MALLOCZ(wlc->osh, sizeof(pdftm_t));
	if (!ftm) {
		err = BCME_NOMEM;
		goto fail;
	}

	ftm->magic = FTM_MAGIC;
	ftm->wlc = wlc;
	ftm->pd = pd;
	ftm_cmn = ftm->ftm_cmn = (pdftm_cmn_t*) obj_registry_get(wlc->objr, OBJR_FTM_CMN_INFO);
	if (ftm_cmn == NULL) {
		ftm_cmn = ftm->ftm_cmn = MALLOCZ(wlc->osh, sizeof(pdftm_cmn_t));
		if (ftm_cmn == NULL) {
			WL_ERROR(("\n Malloc error pdftm_attach\n"));
			ASSERT(0);
			goto fail;
		}
		obj_registry_set(wlc->objr, OBJR_FTM_CMN_INFO, ftm_cmn);

		ftm_cmn->caps |= WL_PROXD_FTM_CAP_FTM1;

		ftm_cmn->config = MALLOCZ(wlc->osh, sizeof(pdftm_config_t));
		if (!ftm_cmn->config) {
			err = BCME_NOMEM;
			goto fail;
		}

		ftm_cmn->enabled_bss_len = FTM_BSSCFG_NUM_OPTIONS *
			HOWMANY(WLC_MAXBSSCFG, NBBY);
		ftm_cmn->enabled_bss = MALLOCZ(wlc->osh, ftm_cmn->enabled_bss_len);
		if (!ftm_cmn->enabled_bss) {
			err = BCME_NOMEM;
			goto fail;
		}

		ftm_cmn->config->session_defaults = pdftm_alloc_session_config(ftm);
		if (!ftm_cmn->config->session_defaults) {
			err = BCME_NOMEM;
			goto fail;
		}

		/* initialize config defaults - rx disabled, but auto burst enabled */
		ftm_cmn->config->flags = WL_PROXD_FLAG_RX_AUTO_BURST | WL_PROXD_FLAG_TX_AUTO_BURST |
			WL_PROXD_FLAG_ASAP_CAPABLE;
		ftm_cmn->config->event_mask = WL_PROXD_EVENT_MASK_ALL;
		ftm_cmn->config->event_mask &=
			~WL_PROXD_EVENT_MASK_EVENT(WL_PROXD_EVENT_FTM_FRAME);
		ftm_cmn->config->event_mask &=	~WL_PROXD_EVENT_MASK_EVENT(WL_PROXD_EVENT_DELAY);
		ftm_cmn->config->rx_max_burst = FTM_SESSION_RX_MAX_BURST;
		ftm_cmn->config->max_rctx_sids = FTM_MAX_RCTX_SIDS;

#ifdef BCMDBG
		ftm_cmn->config->debug = WL_PROXD_DEBUG_ALL;
#endif /* BCMDBG */

		pdftm_init_session_defaults(ftm, ftm_cmn->config->session_defaults);

		ftm_cmn->max_sessions = WL_PROXD_MAX_SESSIONS;
		ftm_cmn->sessions = MALLOCZ(wlc->osh,
			sizeof(pdftm_session_t *) * ftm_cmn->max_sessions);
		if (!ftm_cmn->sessions) {
			err = BCME_NOMEM;
			goto fail;
		}

		ftm_cmn->cnt = MALLOCZ(wlc->osh, sizeof(*ftm_cmn->cnt));
		if (!ftm_cmn->cnt) {
			err = BCME_NOMEM;
			goto fail;
		}

		ftm_cmn->sched = MALLOCZ(wlc->osh, sizeof(*ftm_cmn->sched));
		if (!ftm_cmn->sched) {
			err = BCME_NOMEM;
			goto fail;
		}
		/* init timer to fix the WLC core for sched->timer,
		*else it will be dangling between cores
		*/
		ftm_cmn->sched->timer_wl = FTM_WL(ftm);
		ftm_cmn->sched->timer = wl_init_timer(FTM_WL(ftm),
			pdftm_sched, ftm, FTM_SCHED_NAME);
		ftm_cmn->flags |= FTM_FLAG_SCHED_INITED;

		ftm_cmn->last_sid = WL_PROXD_SID_EXT_MAX + 1;
		err = bcm_notif_create_list(wlc->notif, &ftm_cmn->h_notif);
		if (err != BCME_OK) {
			goto fail;
		}

		ftm_cmn->dig = MALLOCZ(wlc->osh, sizeof(*ftm_cmn->dig));
		if (!ftm_cmn->dig) {
			err = BCME_NOMEM;
			goto fail;
		}
#ifdef WLSLOTTED_BSS
		if (SLOTTED_BSS_SUPPORT(wlc->pub)) {
			err = wlc_slotted_bss_st_notif_register(wlc->sbi,
				(void *)pdftm_ext_sched_cb, NULL);
			/* ext sched registration fails */
			if (err != BCME_OK) {
				WL_ERROR(("wl%d: %s nan register failed with error %d\n",
				wlc->pub->unit, __FUNCTION__, err));
				goto fail;
			}
		}
#endif /* WLSLOTTED_BSS */

	}
#ifdef WLRSDB
	ftm_cmn->ftm[wlc->pub->unit] = ftm;
#endif // endif

	/* parse */
	/* assocreq/assocresp/reassocreq/reassocresp/bcn */
	{
		uint16 ext_cap_parse_fstbmp =
				FT2BMP(FC_ASSOC_REQ) |
				FT2BMP(FC_ASSOC_RESP) |
				FT2BMP(FC_REASSOC_REQ) |
				FT2BMP(FC_REASSOC_RESP) |
				FT2BMP(FC_BEACON) |
				0;
		if ((err = wlc_iem_add_parse_fn_mft(wlc->iemi,
			ext_cap_parse_fstbmp, DOT11_MNG_EXT_CAP_ID,
				wlc_ftm_parse_ext_cap_ie, ftm)) != BCME_OK) {
			WL_ERROR(("wl%d: %s wlc_iem_add_parse_fn failed, err %d,"
				"ext cap ie in assocreq\n",
				wlc->pub->unit, __FUNCTION__, err));
			goto fail;
		}

	}
	ftm->scbh = wlc_scb_cubby_reserve(wlc, sizeof(ftm_scb_t *),
		pdftm_scb_init, pdftm_scb_deinit, PDFTM_SCB_DUMP, ftm);
	if (ftm->scbh < 0) {
		err = BCME_NORESOURCE;
		goto fail;
	}

	/* Add client callback to the scb state notification list */
	err = wlc_scb_state_upd_register(wlc, ftm_scb_state_upd_cb, ftm);
	if (err != BCME_OK) {
		goto fail;
	}
	/* channel switch notif registration */
	if ((err = wlc_chansw_notif_register(wlc, pdftm_chansw_cb, ftm)) != BCME_OK) {
		WL_ERROR(("wl%d: pdftm_attach: wlc_chansw_notif_register"
			" failed. err=%d\n",
			wlc->pub->unit, err));
		goto fail;
	}
	(void)obj_registry_ref(wlc->objr, OBJR_FTM_CMN_INFO);
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: done with status %d\n",
		wlc->pub->unit, __FUNCTION__, err)));

	/* Enable event_log tags */
#ifdef EVENT_LOG_COMPILE
#if defined(BCMDBG_ERR)
	event_log_tag_start(EVENT_LOG_TAG_PROXD_ERROR, EVENT_LOG_SET_ERROR, EVENT_LOG_TAG_FLAG_LOG);
#endif /* BCMDBG_ERR */
#endif /* EVENT_LOG_COMPILE */
	return ftm;
fail:

	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: done with status %d\n",
		wlc->pub->unit, __FUNCTION__, err)));
	MODULE_DETACH(ftm, pdftm_detach);
	return NULL;
}

void
BCMATTACHFN(pdftm_detach)(pdftm_t *in_ftm)
{
	pdftm_t *ftm;
	pdftm_cmn_t *ftm_cmn;
	if (!in_ftm)
		goto done;

	ftm = in_ftm;
	ASSERT(FTM_VALID(ftm));
	ftm_cmn = ftm->ftm_cmn;
	FTM_LOG(ftm, (("wl%d: %s: cleaning up\n", FTM_UNIT(ftm), __FUNCTION__)));
	if (obj_registry_unref(ftm->wlc->objr, OBJR_FTM_CMN_INFO) == 0) {
		if (ftm_cmn->dig)
			MFREE(FTM_OSH(ftm), ftm_cmn->dig, sizeof(*ftm_cmn->dig));

		if (ftm_cmn->cnt)
			MFREE(FTM_OSH(ftm), ftm_cmn->cnt, sizeof(*ftm_cmn->cnt));

#ifdef WL_FTM_RANGE
		if (ftm_cmn->rctx) {
			/* destory the ranging context, delete sessions if configured */
			wlc_ftm_ranging_destroy(&ftm_cmn->rctx);
		}
		ASSERT(ftm_cmn->rctx == NULL);
#endif /* WL_FTM_RANGE */

		(void)pdftm_end_sched(ftm);
		pdftm_free_sessions(ftm, NULL, TRUE);

		if (ftm_cmn->sched)
			MFREE(FTM_OSH(ftm), ftm_cmn->sched, sizeof(*ftm_cmn->sched));

		if (ftm_cmn->h_notif != NULL)
			bcm_notif_delete_list(&ftm_cmn->h_notif);

		if (ftm_cmn->enabled_bss)
			 MFREE(FTM_OSH(ftm), ftm_cmn->enabled_bss, ftm_cmn->enabled_bss_len);

		if (ftm_cmn->config) {
			pdftm_free_session_config(ftm, &ftm_cmn->config->session_defaults);
		MFREE(FTM_OSH(ftm), ftm_cmn->config, sizeof(*ftm_cmn->config));
		}
		MFREE(FTM_OSH(ftm), ftm_cmn, sizeof(*ftm_cmn));
		obj_registry_set(ftm->wlc->objr, OBJR_FTM_CMN_INFO, NULL);
	}

	wlc_chansw_notif_unregister(ftm->wlc, pdftm_chansw_cb, ftm);

	if (ftm) {
		ftm->magic = 0xdefaced1;
		MFREE(FTM_OSH(ftm), ftm, sizeof(*ftm));
	}

done:;
}

bool
wlc_ftm_is_enabled(wlc_ftm_t *ftm, wlc_bsscfg_t *bsscfg)
{
	ASSERT(FTM_VALID(ftm));
	return ftm && bsscfg && FTM_BSSCFG_FTM_ENABLED(ftm->ftm_cmn, bsscfg);
}

static int
wlc_ftm_parse_ext_cap_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	struct scb *scb = wlc_iem_parse_get_assoc_bcn_scb(data);
	if (scb != NULL) {
		if (data->ie != NULL) {
			dot11_extcap_ie_t *extcap_ie_tlv = (dot11_extcap_ie_t *)data->ie;
			dot11_extcap_t *cap = (dot11_extcap_t*)extcap_ie_tlv->cap;

			/* extract FTM initiator and responder caps */
			if ((extcap_ie_tlv->len >= CEIL(DOT11_EXT_CAP_FTM_RESPONDER, 8)) &&
			    (isset(cap->extcap, DOT11_EXT_CAP_FTM_RESPONDER)))
				scb->flags3 |= SCB3_FTM_RESPONDER;

			if ((extcap_ie_tlv->len >= CEIL(DOT11_EXT_CAP_FTM_INITIATOR, 8)) &&
			    (isset(cap->extcap, DOT11_EXT_CAP_FTM_INITIATOR)))
				scb->flags3 |= SCB3_FTM_INITIATOR;
		}
	}

	return BCME_OK;
}

wlc_info_t*
wlc_ftm_get_wlc(wlc_ftm_t *ftm)
{
	ASSERT(FTM_VALID(ftm));
	return ftm ? ftm->wlc : NULL;
}

int
pdftm_scb_init(void *ctx, scb_t *scb)
{
	pdftm_t *ftm = (pdftm_t *)ctx;
	ASSERT(FTM_VALID(ftm));
	FTM_SCB(ftm, scb) = NULL; /* lazy allocation */
	return BCME_OK;
}

void
pdftm_scb_deinit(void *ctx, scb_t *scb)
{
	pdftm_t *ftm = (pdftm_t *)ctx;
	ftm_scb_t *ftm_scb;

	ASSERT(FTM_VALID(ftm));

	ftm_scb = FTM_SCB(ftm, scb);
	if (!ftm_scb)
		goto done;

	FTM_SCB(ftm, scb) = NULL; /* clear cubby */
	MFREE(FTM_OSH(ftm), ftm_scb, ftm_scb->len);

done:;
}

ftm_scb_t*
pdftm_scb_alloc(pdftm_t *ftm, scb_t *scb, bool *created)
{
	ftm_scb_t *ftm_scb;
	int ftm_scb_len;

	ASSERT(FTM_VALID(ftm));

	ftm_scb = FTM_SCB(ftm, scb);
	if (ftm_scb) {
		if (created)
			*created = FALSE;
		goto done;
	}

	ftm_scb_len = sizeof(ftm_scb_t) + FTM_TPK_MAX_LEN;
	ftm_scb = MALLOCZ(FTM_OSH(ftm), ftm_scb_len);
	if (!ftm_scb)
		goto done;

	ftm_scb->len = ftm_scb_len;
	ftm_scb->tpk_max = FTM_TPK_MAX_LEN;
	ftm_scb->tpk_len = 0;	/* no tpk yet */
	ftm_scb->tpk = (uint8 *)(ftm_scb + 1); /* point to trailing tpk */

	FTM_SCB(ftm, scb) = ftm_scb;
	if (created)
		*created = TRUE;

done:
	return ftm_scb;
}

#if defined(WL_PROXDETECT) || defined(WL_FTM)
bool
wlc_ftm_bsscfg_is_secure(const wlc_bsscfg_t *bsscfg)
{
	wlc_ftm_t *ftm;
	ASSERT(bsscfg != NULL);

	ftm = wlc_ftm_get_handle(bsscfg->wlc);

	return ftm && bsscfg && FTM_BSSCFG_SECURE(ftm, bsscfg);
}
#endif // endif
#ifdef WLSLOTTED_BSS
int pdftm_ext_sched_cb(void *ctx, slotted_bss_notif_data_t *cb_data)
{
	pdftm_session_t *sn;
	pdftm_t *ftm;
	ASSERT(cb_data != NULL);
	uint16 sid = *((uint16*)(cb_data->peer_ctx));
	int err = BCME_OK;

	ftm = wlc_ftm_get_handle(cb_data->bsscfg->wlc);
	sn = FTM_SESSION_FOR_SID(ftm, sid);
	if (!sn) {
		err = BCME_NOTFOUND;
		goto done;
	}

	ASSERT(FTM_VALID_SESSION(sn));

	if ((cb_data->state == SB_RNG_START)) {
		/* update sn bsscfg and wlc(ftm) as per chanspec */
		sn->bsscfg = cb_data->bsscfg;
		sn->ftm = ftm;
		err = pdftm_ext_sched_session_ready(sn);
		if (err != BCME_OK) {
			goto done;
		}
		err = pdftm_extsched_timeslot_available(sn, cb_data->duration,
				cb_data->chanspec, &cb_data->peer_mac_addr);
	}
	else if ((cb_data->state == SB_RNG_STOP)) {
		err = pdftm_extsched_timeslot_unavailable(sn);
	}
	else {
		FTM_LOGSCHED(ftm, (("wl%d: %s: unhandled state %d for peer " MACF "\n",
			FTM_UNIT(ftm), __FUNCTION__, cb_data->state,
			ETHERP_TO_MACF(&cb_data->peer_mac_addr))));
	}

done:
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d\n",
		FTM_UNIT(ftm), __FUNCTION__, err)));
	return err;
}
#endif /* WLSLOTTED_BSS */
