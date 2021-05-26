/*
 * Proxd FTM MSCH scheduler support. See twiki FineTimingMeasurement.
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
 * $Id: pdftmmsch.c 777286 2019-07-25 19:43:30Z $
 */

#include "wlc.h"
#include "wlc_sta.h"
#include "pdftmpvt.h"

/*
 * FTM MSCH callback handler.
 */
static int
pdftm_msch_cb(void *handler_ctx, wlc_msch_cb_info_t *cb_info)
{
	int err = BCME_OK;
	pdftm_session_t *sn = (pdftm_session_t *)handler_ctx;
	pdftm_session_state_t *sst;
	pdftm_t *ftm;
	uint64 now;

	ASSERT(FTM_VALID_SESSION(sn));
	ftm = sn->ftm;
	ASSERT(FTM_VALID(ftm));
	BCM_REFERENCE(now);
	sst = sn->ftm_state;
	/*
	 * REQ_START | ON_CHAN
	 * Re-calculate burst start because sometimes state change reports
	 * burst expired.
	 */
	FTM_GET_TSF(ftm, now);
	FTM_LOGSCHED(ftm, (("%s[%d]: wl%d type %d state %d"
		"now %u.%u burst start %u.%u\n",
		__FUNCTION__, __LINE__, FTM_UNIT(ftm), cb_info->type, sn->state,
		FTM_LOG_TSF_ARG(now), FTM_LOG_TSF_ARG(sst->burst_start))));
	/* MPC up on pre-onchan msch event */
	if (cb_info->type & MSCH_CT_PRE_ONCHAN) {
		proxd_power(ftm->wlc, PROXD_PWR_ID_BURST, TRUE);
	}
	else if ((cb_info->type & MSCH_CT_ON_CHAN) ||
			(cb_info->type & MSCH_CT_SLOT_START)) {
		/* Dont start queue if STA and chancpec is same as sta chanspec */
		/* if (!wlc_sta_timeslot_onchannel(sn->bsscfg)) */ {
			wlc_txqueue_start(ftm->wlc, sn->bsscfg, cb_info->chanspec, NULL);
		}
		if (sn->state >= WL_PROXD_SESSION_STATE_DELAY ||
				sn->state <= WL_PROXD_SESSION_STATE_SCHED_WAIT) {
			sn->flags |= FTM_SESSION_ON_CHAN;

		}
	} else if ((cb_info->type & MSCH_CT_SLOT_END) ||
		(cb_info->type & MSCH_CT_REQ_END) ||
		(cb_info->type & MSCH_CT_SLOT_SKIP)) {
		/* Dont start queue if STA and chancpec is same as sta chanspec */
		/* if (!wlc_sta_timeslot_onchannel(sn->bsscfg)) */ {
			wlc_txqueue_end(ftm->wlc, sn->bsscfg, NULL);
		}
		sst->flags &= ~FTM_SESSION_STATE_BURST_REQ;
		sn->flags &= ~FTM_SESSION_ON_CHAN;
		sst->flags |= FTM_SESSION_STATE_MSCH_SCHEDULED;
		if (sn->state <= WL_PROXD_SESSION_STATE_BURST) {
			/*
			 * If burst is still ongoing stop now
			 */
			FTM_LOGSCHED(ftm, (("%s[%d]: wl%d type %d state %d Stopping Burst\n",
				__FUNCTION__, __LINE__, FTM_UNIT(ftm), cb_info->type, sn->state)));
			if (sn->state == WL_PROXD_SESSION_STATE_BURST) {
				/* cancel burst & report the partial result */
				pdburst_cancel(sn->ftm_state->burst, WL_PROXD_E_TIMEOUT);
			} else {
				(void)pdftm_change_session_state(sn, sn->status,
					WL_PROXD_SESSION_STATE_STOPPING);
			}
		}
		if (cb_info->type & MSCH_CT_REQ_END) {
			if (sn->ext_sched->req_hdl) {
				sn->ext_sched->req_hdl = NULL;
			}
		}
	} else {
		FTM_LOGSCHED(ftm, (("%s[%d]: wl%d Unknown event %d\n",
			__FUNCTION__, __LINE__, FTM_UNIT(ftm), cb_info->type)));
	}
	(void)pdftm_wake_sched(ftm, sn);

	return err;
}

/*
 * Called from detach to stop FTM scheduling.
 * Loop through all sessions and delete timers and free them.
 */
int
pdftm_msch_end(pdftm_t *ftm, pdftm_session_t *sn)
{
	int err = BCME_OK;
	int i;
	pdftm_session_state_t *sst;
	pdftm_cmn_t *ftm_cmn;

	ASSERT(FTM_VALID(ftm));
	ftm_cmn = ftm->ftm_cmn;

	if (!(ftm_cmn->flags & FTM_FLAG_SCHED_INITED))
		goto done;

	proxd_power(ftm->wlc, PROXD_PWR_ID_BURST, FALSE);
	/*
	 * Loop through session MSCH sched timers, delete and free them.
	 */
	if (!sn) {
		for (i = 0; i < ftm_cmn->max_sessions; i++) {
			sn = ftm_cmn->sessions[i];
			if (!FTM_VALID_SESSION(sn))
				continue;
			sst = sn->ftm_state;
			if ((sst != NULL) && sn->ext_sched && sn->ext_sched->req_hdl) {
				if (!(sst->flags & FTM_SESSION_STATE_MSCH_SCHEDULED)) {
					/* check for sta on chan as cfg is shared */
					/* if (!wlc_sta_timeslot_onchannel(sn->bsscfg)) */ {
						wlc_txqueue_end(ftm->wlc, sn->bsscfg, NULL);
					}

					wlc_msch_timeslot_unregister(ftm->wlc->msch_info,
						(wlc_msch_req_handle_t **)&sn->ext_sched->req_hdl);
				}
			}
		}
	} else {
		if (!FTM_VALID_SESSION(sn))
			goto done;
		if (sn->ext_sched && sn->ext_sched->req_hdl) {
			sst = sn->ftm_state;
			if (sst != NULL) {
				if (!(sst->flags & FTM_SESSION_STATE_MSCH_SCHEDULED)) {
					/* check for sta on chan as cfg is shared */
					/* if (!wlc_sta_timeslot_onchannel(sn->bsscfg)) */ {
						wlc_txqueue_end(ftm->wlc, sn->bsscfg, NULL);
					}

					wlc_msch_timeslot_unregister(ftm->wlc->msch_info,
						(wlc_msch_req_handle_t **)&sn->ext_sched->req_hdl);
				}
			}
		}
	}

done:
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s[%d]: status %d, time:%llu\n",
		FTM_UNIT(ftm), __FUNCTION__, __LINE__, err, OSL_SYSUPTIME_US())));
	return err;
}

int
pdftm_msch_begin(pdftm_t *ftm, pdftm_session_t *sn)
{
	int err = BCME_OK;
	pdftm_session_state_t *sst;
	int chanspec_cnt = 1;
	chanspec_t chanspec;
	wlc_msch_req_param_t req_param;
	uint64 now;

	ASSERT(FTM_VALID(ftm));
	BCM_REFERENCE(now);

	if (!FTM_VALID_SESSION(sn))
		return WL_PROXD_E_INVALID_SESSION;

	if (!sn->ext_sched) {
		sn->ext_sched = MALLOCZ(FTM_OSH(ftm), sizeof(*sn->ext_sched));
		if (!sn->ext_sched)
			return WL_PROXD_E_ERROR;
	}
	sst = sn->ftm_state;

	if (sst->flags & FTM_SESSION_STATE_BURST_REQ)
		return err;
	/*
	 * Burst type, start time and duration are known.
	 * register with MSCH and wait for a callback.
	 */
	bzero(&req_param, sizeof(req_param));
	req_param.duration = FTM_INTVL2USEC(&sn->config->burst_config->duration);
	if (FTM_SESSION_IS_ASAP(sn)) {
		req_param.req_type = MSCH_RT_DUR_FLEX; /* Start fixed duration flex */
	} else {
		/* Non-ASAP */
		req_param.req_type = MSCH_RT_BOTH_FIXED; /* Start fixed duration fixed */
	}
	req_param.priority = MSCH_RP_CONNECTION;
	FTM_GET_TSF(ftm, now);
	/*
	 * From the time burst_start is computed by ftm_sched_compute_burst_start
	 * to the call wlc_msch_timeslot_register, it takes approx. 80 usecs
	 * To be safe adding 300 us to make sure we are not going to request
	 * time in the past. This will change when MSCH implements future time
	 * slot availability request API.
	 */
	req_param.start_time_h = (uint32)(sst->burst_start >> 32);
	req_param.start_time_l = (uint32)(sst->burst_start) + 300;
	req_param.interval = 0;
	req_param.flags = 0;
	chanspec = sn->config->burst_config->chanspec;

	if (FTM_INFRA_STA(sn->bsscfg)) {
		if (chanspec != wlc_get_home_chanspec(sn->bsscfg)) {
			req_param.req_type = MSCH_RT_BOTH_FIXED;
		}
		else {
			req_param.req_type = MSCH_RT_START_FLEX;
		}
	}
	err = wlc_msch_timeslot_register(ftm->wlc->msch_info, &chanspec,
		chanspec_cnt, pdftm_msch_cb, (void *)sn, &req_param,
		(wlc_msch_req_handle_t **)&sn->ext_sched->req_hdl);
	if (err != BCME_OK) {
		FTM_LOGSCHED(ftm, (("wl%d: %s timeslot request failed %d\n",
			FTM_UNIT(ftm), __FUNCTION__, err)));
	} else {
		wlc_msch_set_chansw_reason(ftm->wlc->msch_info,
			(wlc_msch_req_handle_t *)sn->ext_sched->req_hdl,
			CHANSW_PROXD);
		wlc_user_wake_upd(sn->ftm->wlc, WLC_USER_WAKE_REQ_FTM, TRUE);
		sst->flags |= FTM_SESSION_STATE_BURST_REQ;
	}
	FTM_LOGSCHED(ftm, (("wl%d: %s timeslot requested now %u.%u start %u.%u\n",
		FTM_UNIT(ftm), __FUNCTION__, FTM_LOG_TSF_ARG(now),
		FTM_LOG_TSF_ARG(sst->burst_start))));
	return err;
}
