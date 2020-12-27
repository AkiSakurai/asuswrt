/*
 * Proxd FTM method implementation - session mgmt. See twiki FineTimingMeasurement.
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
 * $Id: pdftmsn.c 777979 2019-08-19 23:14:13Z $
 */

#include "bcmdevs.h"
#include "pdftmpvt.h"
#include <phy_rxgcrs_api.h>

typedef int (*ftm_session_state_handler_t)(pdftm_t *ftm, pdftm_session_t *sn,
	wl_proxd_status_t status, wl_proxd_session_state_t new_state,
	wl_proxd_event_type_t *event_type);

struct ftm_session_state_info {
	wl_proxd_session_state_t state;
	ftm_session_state_handler_t handler;
};
typedef struct ftm_session_state_info ftm_session_state_info_t;

#define SNSH_DECL(_name) static int ftm_snsh_##_name(pdftm_t *ftm, pdftm_session_t *sn, \
	wl_proxd_status_t status, wl_proxd_session_state_t new_state, \
	wl_proxd_event_type_t *event_type)
SNSH_DECL(none);
SNSH_DECL(created);
SNSH_DECL(configured);
SNSH_DECL(start_wait);
SNSH_DECL(started);
SNSH_DECL(delay);
SNSH_DECL(user_wait);
SNSH_DECL(sched_wait);
SNSH_DECL(burst);
SNSH_DECL(stopping);
SNSH_DECL(ended);
SNSH_DECL(destroying);

#define SNSI_INFO_DECL(_s, _n) {WL_PROXD_SESSION_STATE_##_s, ftm_snsh_##_n}
static ftm_session_state_info_t ftm_session_state_info[] = {
	SNSI_INFO_DECL(NONE, none),
	SNSI_INFO_DECL(CREATED, created),
	SNSI_INFO_DECL(CONFIGURED, configured),
	SNSI_INFO_DECL(START_WAIT, start_wait),
	SNSI_INFO_DECL(STARTED, started),
	SNSI_INFO_DECL(DELAY, delay),
	SNSI_INFO_DECL(USER_WAIT, user_wait),
	SNSI_INFO_DECL(SCHED_WAIT, sched_wait),
	SNSI_INFO_DECL(BURST, burst),
	SNSI_INFO_DECL(STOPPING, stopping),
	SNSI_INFO_DECL(ENDED, ended),
	SNSI_INFO_DECL(DESTROYING, destroying)
};

#define UPDATE_STATUS(_sn, _status) do {\
	if ((_sn)->status == BCME_OK) { \
		(_sn)->status = status; \
	} \
} while (0)

#define SAFE_DECR(_x) do { ASSERT((_x) > 0); if (_x) --(_x); } while (0)

static ftm_session_state_handler_t
ftm_snsi_get_handler(wl_proxd_session_state_t state)
{
	uint i;
	for (i = 0; i < ARRAYSIZE(ftm_session_state_info); ++i) {
		if (ftm_session_state_info[i].state == state) {
			return ftm_session_state_info[i].handler;
		}
	}
	return NULL;
}

static pdftm_session_idx_t
ftm_sn_get_free_idx(pdftm_t *ftm)
{
	int i;
	pdftm_session_idx_t idx;
	pdftm_cmn_t *ftm_cmn = ftm->ftm_cmn;
	/* look for allocated, but invalid sessions */
	idx = FTM_SESSION_INVALID_IDX;
	for (i = 0; i < ftm_cmn->max_sessions; ++i) {
		pdftm_session_t *sn = ftm_cmn->sessions[i];
		if (!sn || FTM_VALID_SESSION(sn))
			continue;
		idx = (pdftm_session_idx_t)i;
		goto done;
	}

	/* look for sessions to be allocated */
	for (i = 0; i < ftm_cmn->max_sessions; ++i) {
		pdftm_session_t *sn = ftm_cmn->sessions[i];
		if (sn)
			continue;
		idx = (pdftm_session_idx_t)i;
		goto done;
	}

done:
	return idx;
}

static int
ftm_snsh_none(pdftm_t *ftm, pdftm_session_t *sn, wl_proxd_status_t status,
	wl_proxd_session_state_t new_state, wl_proxd_event_type_t *event_type)
{
	int err = BCME_OK;

	ASSERT(sn->state == WL_PROXD_SESSION_STATE_NONE);

	*event_type = WL_PROXD_EVENT_NONE;
	if (new_state != WL_PROXD_SESSION_STATE_CREATED) {
		err = WL_PROXD_E_BAD_STATE;
		goto done;
	}

	sn->state = new_state;
	UPDATE_STATUS(sn, status);
	*event_type = WL_PROXD_EVENT_SESSION_CREATE;

done:
	return err;
}

static int
ftm_snsh_created(pdftm_t *ftm, pdftm_session_t *sn, wl_proxd_status_t status,
	wl_proxd_session_state_t new_state, wl_proxd_event_type_t *event_type)
{
	int err = BCME_OK;

	ASSERT(sn->state == WL_PROXD_SESSION_STATE_CREATED);

	*event_type = WL_PROXD_EVENT_NONE;
	switch (new_state) {
	case WL_PROXD_SESSION_STATE_CONFIGURED:
	case WL_PROXD_SESSION_STATE_STOPPING:
		break;
	default:
		err = WL_PROXD_E_BAD_STATE;
		break;
	}

	if (err != BCME_OK)
		goto done;

	sn->state = new_state;
	UPDATE_STATUS(sn, status);

	err = pdftm_sched_session(ftm, sn);

done:
	return err;
}

static int
ftm_snsh_configured(pdftm_t *ftm, pdftm_session_t *sn, wl_proxd_status_t status,
	wl_proxd_session_state_t new_state, wl_proxd_event_type_t *event_type)
{
	int err = BCME_OK;
	pdftm_cmn_t *ftm_cmn = ftm->ftm_cmn;
	ASSERT(sn->state == WL_PROXD_SESSION_STATE_CONFIGURED);

	*event_type = WL_PROXD_EVENT_NONE;
	switch (new_state) {
	case WL_PROXD_SESSION_STATE_START_WAIT:
		*event_type = WL_PROXD_EVENT_START_WAIT;
		++(ftm_cmn->sched->in.start_wait);

		if (sn->flags & FTM_SESSION_EXT_SCHED_WAIT) {
			/* register callback w/ external scheduler */
			err = pdftm_session_register_ext_sched(ftm, sn);
			FTM_LOGSN(ftm, (("wl%d: %s: session idx %d register with "
					"ext sched status %d\n",
					FTM_UNIT(ftm), __FUNCTION__, sn->idx, err)));
		}
		break;
	case WL_PROXD_SESSION_STATE_STARTED:
		*event_type = WL_PROXD_EVENT_SESSION_START;
		++(ftm_cmn->sched->in.started);
		break;
	case WL_PROXD_SESSION_STATE_STOPPING:
		break;
	default:
		err = WL_PROXD_E_BAD_STATE;
		break;
	}

	if (err != BCME_OK)
		goto done;

	UPDATE_STATUS(sn, status);
	sn->state = new_state;
	err = pdftm_sched_session(ftm, sn);

done:
	return err;
}

static int
ftm_snsh_start_wait(pdftm_t *ftm, pdftm_session_t *sn, wl_proxd_status_t status,
	wl_proxd_session_state_t new_state, wl_proxd_event_type_t *event_type)
{
	int err = BCME_OK;
	pdftm_cmn_t *ftm_cmn = ftm->ftm_cmn;
	ASSERT(sn->state == WL_PROXD_SESSION_STATE_START_WAIT);
	*event_type = WL_PROXD_EVENT_NONE;

	if (ftm_cmn->flags & FTM_FLAG_SCHED_ACTIVE) {
		err = WL_PROXD_E_BAD_STATE;
		goto done;
	}

	switch (new_state) {
	case WL_PROXD_SESSION_STATE_STARTED:
		*event_type = WL_PROXD_EVENT_SESSION_START;
		++(ftm_cmn->sched->in.started);
		break;
	case WL_PROXD_SESSION_STATE_STOPPING:
		break;
	default:
		err = WL_PROXD_E_BAD_STATE;
		break;
	}
	if (err != BCME_OK)
		goto done;

	SAFE_DECR(ftm_cmn->sched->in.start_wait);
	UPDATE_STATUS(sn, status);
	sn->state = new_state;
	err = pdftm_sched_session(ftm, sn);
done:
	return err;
}

static int
ftm_snsh_started(pdftm_t *ftm, pdftm_session_t *sn, wl_proxd_status_t status,
	wl_proxd_session_state_t new_state, wl_proxd_event_type_t *event_type)
{
	int err = BCME_OK;
	pdftm_cmn_t *ftm_cmn = ftm->ftm_cmn;
	ASSERT(sn->state == WL_PROXD_SESSION_STATE_STARTED);

	*event_type = WL_PROXD_EVENT_NONE;
	switch (new_state) {
	case WL_PROXD_SESSION_STATE_DELAY:
		++(ftm_cmn->sched->in.delay);

		/* expire delay using user config (initiator), or protocol (target) */
		FTM_GET_TSF(ftm, sn->ftm_state->delay_exp);
		sn->ftm_state->delay_exp += FTM_INTVL2USEC(&sn->config->init_delay);
		/* non-asap needs to account for processing delay */
		if (FTM_SESSION_IS_INITIATOR(sn) && !FTM_SESSION_IS_ASAP(sn)) {
			/* if previous trigger was delayed or early, adjust the next start */
			if (FTM_TSF_SYNC_SESSION(sn)) {
				if (sn->ftm_state->trig_tsf && sn->ftm_state->trig_delta) {
					sn->ftm_state->delay_exp -= sn->ftm_state->trig_delta;
				}
			}
		}

		FTM_LOGSN(ftm, (("wl%d: %s: session idx %d delay exp %u.%u\n",
			FTM_UNIT(ftm), __FUNCTION__, sn->idx,
			FTM_LOG_TSF_ARG(sn->ftm_state->delay_exp))));

		*event_type =  WL_PROXD_EVENT_DELAY;
		break;
	case WL_PROXD_SESSION_STATE_STOPPING:
		break;
	default:
		err = WL_PROXD_E_BAD_STATE;
		break;
	}

	if (err != BCME_OK)
		goto done;

	UPDATE_STATUS(sn, status);
	sn->state = new_state;
	SAFE_DECR(ftm_cmn->sched->in.started);

	err = pdftm_sched_session(ftm, sn);
	if (err != BCME_OK)
		goto done;

done:
	return err;
}

static int
ftm_snsh_delay(pdftm_t *ftm, pdftm_session_t *sn, wl_proxd_status_t status,
	wl_proxd_session_state_t new_state, wl_proxd_event_type_t *event_type)
{
	int err = BCME_OK;
	uint64 cur_tsf;
	pdftm_cmn_t *ftm_cmn = ftm->ftm_cmn;
	pdftm_session_state_t *sst = NULL;

	ASSERT(sn->state == WL_PROXD_SESSION_STATE_DELAY);

	sst = sn->ftm_state;

	*event_type = WL_PROXD_EVENT_NONE;
	FTM_GET_TSF(ftm, cur_tsf);
	switch (new_state) {
	case WL_PROXD_SESSION_STATE_SCHED_WAIT:
		if (!(ftm_cmn->flags & FTM_FLAG_SCHED_ACTIVE)) {
			err = WL_PROXD_E_BAD_STATE;
			break;
		}

		/* delay must expire before burst start + timeout on target. if local
		 * availability is set, the session moves to next available slot
		 */
		if (sn->config->flags & WL_PROXD_SESSION_FLAG_TARGET) {
		   if (sn->ftm_state->flags & FTM_SESSION_STATE_NEED_TRIGGER) {
				if (sst->burst_num < sn->config->num_burst) {
				    FTM_LOGSN(ftm, (("%s: TIMEDOUT-burst_num %d(%d)"
						"Recalculating delay_exp %u:%u\n", __FUNCTION__,
						sst->burst_num, sn->config->num_burst,
						FTM_LOG_TSF_ARG(sst->delay_exp))));
					sst->delay_exp = sn->ftm_state->burst_start +
						FTM_INTVL2USEC(&sn->config->burst_period);
					new_state = WL_PROXD_SESSION_STATE_DELAY;
					sst->flags &= ~FTM_SESSION_STATE_BURST_DONE;
					sst->burst_num++;
				}
				else {
					err = WL_PROXD_E_EXPIRED;
					break;
				}
			}
		}

		if (new_state == WL_PROXD_SESSION_STATE_SCHED_WAIT) {
			++(ftm_cmn->sched->in.sched_wait);
		}
		break;
	case WL_PROXD_SESSION_STATE_STOPPING:
		break;
	default:
		err = WL_PROXD_E_BAD_STATE;
		break;
	}

	if (err != BCME_OK)
		goto done;

	UPDATE_STATUS(sn, status);
	sn->state = new_state;
	if (new_state != WL_PROXD_SESSION_STATE_DELAY) {
		SAFE_DECR(ftm_cmn->sched->in.delay);
	}
	/* setup to timeout a burst, if not scheduled within the period */
	sn->ftm_state->burst_exp = cur_tsf +
		FTM_INTVL2USEC(&sn->config->burst_config->timeout);

	if ((new_state == WL_PROXD_SESSION_STATE_STOPPING) ||
			(new_state == WL_PROXD_SESSION_STATE_DELAY)) {
		err = pdftm_sched_session(ftm, sn);
	}

done:
	return err;
}

static int
ftm_snsh_user_wait(pdftm_t *ftm, pdftm_session_t *sn, wl_proxd_status_t status,
	wl_proxd_session_state_t new_state, wl_proxd_event_type_t *event_type)
{
	int err = BCME_OK;
	pdftm_cmn_t *ftm_cmn = ftm->ftm_cmn;
	ASSERT(sn->state == WL_PROXD_SESSION_STATE_USER_WAIT);

	*event_type = WL_PROXD_EVENT_NONE;
	switch (new_state) {
	case WL_PROXD_SESSION_STATE_DELAY:
		if (ftm_cmn->flags & FTM_FLAG_SCHED_ACTIVE) {
			err = WL_PROXD_E_BAD_STATE;
			break;
		}

		/* delay expires at next burst start with additional burst timeout  on target  */
		sn->ftm_state->delay_exp  = sn->ftm_state->burst_start;
		sn->ftm_state->delay_exp += FTM_INTVL2USEC(&sn->config->burst_period);
		if (sn->config->flags & WL_PROXD_SESSION_FLAG_TARGET) {
			sn->ftm_state->delay_exp +=
				FTM_INTVL2USEC(&sn->config->burst_config->timeout);
			if (!(sn->config->flags & WL_PROXD_SESSION_FLAG_ASAP) ||
					(sn->flags & FTM_SESSION_FTM1_SENT)) {
				sn->ftm_state->flags |= FTM_SESSION_STATE_NEED_TRIGGER;
			}
		}

		FTM_LOGSN(ftm, (("wl%d: %s: session idx %d exp tsf %u.%u\n",
			FTM_UNIT(ftm), __FUNCTION__, sn->idx,
			FTM_LOG_TSF_ARG(sn->ftm_state->delay_exp))));

		*event_type = WL_PROXD_EVENT_DELAY;
		++(ftm_cmn->sched->in.delay);
		break;
	case WL_PROXD_SESSION_STATE_STOPPING:
		break;
	default:
		err = WL_PROXD_E_BAD_STATE;
		break;
	}

	if (err != BCME_OK)
		goto done;

	UPDATE_STATUS(sn, status);
	sn->state = new_state;
	err = pdftm_sched_session(ftm, sn);
done:
	return err;
}

static int
ftm_snsh_sched_wait(pdftm_t *ftm, pdftm_session_t *sn, wl_proxd_status_t status,
	wl_proxd_session_state_t new_state, wl_proxd_event_type_t *event_type)
{
	int err = BCME_OK;
	pdftm_expiration_t cur_tsf;
	pdftm_cmn_t *ftm_cmn = ftm->ftm_cmn;
	ASSERT(sn->state == WL_PROXD_SESSION_STATE_SCHED_WAIT);

	*event_type = WL_PROXD_EVENT_NONE;

	if (new_state != WL_PROXD_SESSION_STATE_BURST &&
		new_state != WL_PROXD_SESSION_STATE_DELAY &&
		new_state != WL_PROXD_SESSION_STATE_STOPPING) {
		err = WL_PROXD_E_BAD_STATE;
		goto done;
	}

	/* create/setup the burst. for multi-burst, the burst is setup when the
	 * trigger is received. expire session/burst if no trigger came.
	 */
	if (new_state == WL_PROXD_SESSION_STATE_BURST) {
		if ((sn->config->flags & WL_PROXD_SESSION_FLAG_INITIATOR) ||
			!((sn->ftm_state->flags & FTM_SESSION_STATE_BURST_DONE))) {
			err = pdftm_setup_burst(ftm, sn, NULL, 0, NULL, 0);
		} else {
			err = WL_PROXD_E_EXPIRED;
		}

		if (err != BCME_OK)
			goto done;
	}

	SAFE_DECR(ftm_cmn->sched->in.sched_wait);
	UPDATE_STATUS(sn, status);
	sn->state = new_state;

	switch (new_state) {
	case WL_PROXD_SESSION_STATE_BURST:
		++(ftm_cmn->sched->in.burst);
		if (!(ftm_cmn->flags & FTM_FLAG_SCHED_ACTIVE)) {
			err = WL_PROXD_E_BAD_STATE;
			break;
		}

		/* check expiration (scheduling anomalies) and bail if expired. */
		FTM_GET_TSF(ftm, cur_tsf);
		if ((cur_tsf > sn->ftm_state->burst_exp) || !sn->ftm_state->burst ||
			(sn->ftm_state->flags & FTM_SESSION_STATE_NEED_TRIGGER)) {
			 err = WL_PROXD_E_EXPIRED;
			FTM_CNT(ftm_cmn, sn, timeouts);
			goto done;
		}

		*event_type = WL_PROXD_EVENT_BURST_START;

		FTM_CNT(ftm->ftm_cmn, sn, burst);
		ftm_cmn->sched->burst_sn = sn;

		err = pdburst_start(sn->ftm_state->burst);
		break;
	case WL_PROXD_SESSION_STATE_DELAY:
		++(ftm_cmn->sched->in.delay);
		FTM_LOGSN(ftm, (("wl%d: %s: session idx %d delay exp %u.%u\n",
			FTM_UNIT(ftm), __FUNCTION__, sn->idx,
			FTM_LOG_TSF_ARG(sn->ftm_state->delay_exp))));
		*event_type =  WL_PROXD_EVENT_DELAY;
		break;
	case WL_PROXD_SESSION_STATE_STOPPING:
		err = pdftm_sched_session(ftm, sn);
		break;
	default:
		ASSERT(FALSE);
		break;
	}

done:
	return err;
}

static int
ftm_snsh_burst(pdftm_t *ftm, pdftm_session_t *sn, wl_proxd_status_t status,
	wl_proxd_session_state_t new_state, wl_proxd_event_type_t *event_type)
{
	int err = BCME_OK;
	pdftm_cmn_t *ftm_cmn = ftm->ftm_cmn;
	wlc_bsscfg_t *bsscfg = sn->bsscfg;
	ASSERT(bsscfg != NULL);

	ASSERT(sn->state == WL_PROXD_SESSION_STATE_BURST);
	BCM_REFERENCE(bsscfg);

	*event_type = WL_PROXD_EVENT_BURST_END;
	FTM_GET_TSF(ftm, sn->ftm_state->burst_end);

	if (!(sn->ftm_state->flags & FTM_SESSION_STATE_BURST_DONE)) {
		/* Remote failed, No burst done called update rtt_status */
		sn->ftm_state->rtt_status = status;
	} else {
		/* Burst is done, set burst duration */
		FTM_STATE_SET_BURST_DURATION(sn->ftm_state);
	}

	/* if measurement has not started, update measure start */
	if (sn->ftm_state->meas_start < sn->ftm_state->burst_start) {
		sn->ftm_state->meas_start =  sn->ftm_state->burst_start;
	}

	switch (new_state) {
	case WL_PROXD_SESSION_STATE_DELAY:
		/* note: burst may be continued */
		++(ftm_cmn->sched->in.delay);
		if (FTM_SESSION_IS_INITIATOR(sn)) {
			if ((sn->ftm_state->burst_num == 1) &&
				(sn->ftm_state->rtt_status == WL_PROXD_E_TIMEOUT) &&
				!(sn->flags & FTM_SESSION_FTM1_RCVD)) {
				/* Initiator timed out on FTM1 for the first burst
				 * cancel the session and send trigger-0 (best effort).
				 */
				FTM_LOGSN(ftm, (("wl%d: %s: stopping session, burst_num=%d,"
					" timed out without rx FTM-1\n",
					FTM_UNIT(ftm), __FUNCTION__, sn->ftm_state->burst_num)));
				err = pdftm_stop_session(sn, WL_PROXD_E_CANCELED);
				goto done;
			}
		}
		/* if burst is done, schedule next burst; delay expires at next burst start
		 * allow burst timeout on target to receive trigger
		 */
		if (sn->ftm_state->flags & FTM_SESSION_STATE_BURST_DONE) {
			sn->ftm_state->delay_exp = sn->ftm_state->burst_start;
			if ((sn->config->flags & WL_PROXD_SESSION_FLAG_MBURST_NODELAY) &&
				FTM_SESSION_IS_INITIATOR(sn)) {
				sn->ftm_state->delay_exp += BURST_DELAY_IMMEDIATE;
			} else {
				sn->ftm_state->delay_exp +=
					FTM_INTVL2USEC(&sn->config->burst_period);
			}
			if (FTM_SESSION_IS_TARGET(sn)) {
				sn->ftm_state->flags |= FTM_SESSION_STATE_NEED_TRIGGER;
			}

			/* if previous trigger was delayed or early, adjust the next start */
			if (FTM_TSF_SYNC_SESSION(sn)) {
				if (FTM_SESSION_IS_INITIATOR(sn) && sn->ftm_state->trig_tsf &&
						sn->ftm_state->trig_delta) {
					sn->ftm_state->delay_exp -= sn->ftm_state->trig_delta;
				}
			}
		} else {
			*event_type = WL_PROXD_EVENT_BURST_RESCHED;
		}

		FTM_LOGSN(ftm, (("wl%d: %s: session idx %d delay exp %u.%u\n",
			FTM_UNIT(ftm), __FUNCTION__, sn->idx,
			FTM_LOG_TSF_ARG(sn->ftm_state->delay_exp))));

		/* keep the burst for next time */
		break;
	case WL_PROXD_SESSION_STATE_STOPPING: /* fall through */
	case WL_PROXD_SESSION_STATE_ENDED:
	case WL_PROXD_SESSION_STATE_USER_WAIT:
		/* destroy burst we are done with */
		if (sn->ftm_state->burst != NULL) {
			pdburst_destroy(&sn->ftm_state->burst);
			ASSERT(sn->ftm_state->burst == NULL);
		}
		break;
	default:
		*event_type = WL_PROXD_EVENT_NONE;
		err = WL_PROXD_E_BAD_STATE;
		break;
	}

#ifdef WL_FTM_MSCH
	if (bsscfg->type != BSSCFG_TYPE_SLOTTED_BSS)
		(void)pdftm_msch_end(ftm, sn);
#endif /* WL_FTM_MSCH */
	if (err != BCME_OK)
		goto done;

	ftm_cmn->sched->burst_sn = NULL;
	SAFE_DECR(ftm_cmn->sched->in.burst);
	UPDATE_STATUS(sn, status);
	sn->state = new_state;

	/* burst ended - wake scheduler */
	err = pdftm_sched_session(ftm, sn);

done:
	return err;
}

static int
ftm_snsh_stopping(pdftm_t *ftm, pdftm_session_t *sn, wl_proxd_status_t status,
	wl_proxd_session_state_t new_state, wl_proxd_event_type_t *event_type)
{
	int err = BCME_OK;
	wlc_bsscfg_t *bsscfg = sn->bsscfg;
	ASSERT(bsscfg != NULL);

	ASSERT(sn->state == WL_PROXD_SESSION_STATE_STOPPING);
	BCM_REFERENCE(bsscfg);

	*event_type = WL_PROXD_EVENT_NONE;
	if (new_state != WL_PROXD_SESSION_STATE_ENDED) {
		err = WL_PROXD_E_BAD_STATE;
		goto done;
	}

	*event_type = WL_PROXD_EVENT_SESSION_END;
	UPDATE_STATUS(sn, status);
	sn->state = new_state;

	/* deregister awdl callback */
	if (bsscfg->type == BSSCFG_TYPE_SLOTTED_BSS) {
		err = BCME_UNSUPPORTED; /* needs NAN support */
		goto done;
	}

	if (sn->tsf_scan_state > FTM_TSF_SCAN_STATE_NONE)
		wlc_scan_abort_ex(ftm->wlc->scan, sn->bsscfg, WLC_E_STATUS_ABORT);

	if (sn->ftm_state && sn->ftm_state->burst) {
		pdburst_destroy(&sn->ftm_state->burst);
		ASSERT(sn->ftm_state->burst == NULL);
	}

	if (sn->flags & FTM_SESSION_DELETE_ON_STOP)
		err = pdftm_sched_session(ftm, sn);
done:
	return err;
}

static int
ftm_snsh_ended(pdftm_t *ftm, pdftm_session_t *sn, wl_proxd_status_t status,
	wl_proxd_session_state_t new_state, wl_proxd_event_type_t *event_type)
{
	int err = BCME_OK;

	ASSERT(sn->state == WL_PROXD_SESSION_STATE_ENDED);
	if (new_state != WL_PROXD_SESSION_STATE_DESTROYING) {
		err = WL_PROXD_E_BAD_STATE;
		goto done;
	}

	*event_type = WL_PROXD_EVENT_SESSION_DESTROY;
	UPDATE_STATUS(sn, status);
	sn->state = new_state;
done:
	return err;
}

static int
ftm_snsh_destroying(pdftm_t *ftm, pdftm_session_t *sn, wl_proxd_status_t status,
	wl_proxd_session_state_t new_state, wl_proxd_event_type_t *event_type)
{
	int err = BCME_OK;

	ASSERT(sn->state == WL_PROXD_SESSION_STATE_DESTROYING);
	*event_type = WL_PROXD_EVENT_NONE;
	if (new_state != WL_PROXD_SESSION_STATE_NONE) {
		err = WL_PROXD_E_BAD_STATE;
		goto done;
	}

	/* if tx is pending, destroy will be done by packet callback */
	if (sn->flags & FTM_SESSION_TX_PENDING) {
		goto done;
	}

	/* skip status, state update */
	pdftm_free_session(&sn);
	ASSERT(sn == NULL);
done:
	return err;
}

/* resolve the bsscfg for the burst */
wlc_bsscfg_t*
pdftm_get_session_tx_bsscfg(const pdftm_session_t *sn)
{
	wlc_bsscfg_t *bsscfg;
	pdftm_t *ftm = sn->ftm;
	bsscfg = sn->bsscfg;
	do {
		/* need a valid bssid */
		if (!ETHER_ISNULLADDR(&bsscfg->BSSID))
			break;

		/* use standalone */
		if (sn->config->flags & WL_PROXD_SESSION_FLAG_INITIATOR)
			bsscfg = pdsvc_get_bsscfg(ftm->pd);

	} while (0);
	ASSERT(bsscfg->wlc == sn->ftm->wlc);
	return bsscfg;
}

int
pdftm_get_session_tsf(const pdftm_session_t *sn, uint64 *session_tsf)
{
	wlc_bsscfg_t *bsscfg;
	int err = BCME_OK;
	uint64 tsf = 0;
	scb_t *scb;
	bool use_tsf_off;
	pdftm_t *ftm = sn->ftm;

	ASSERT(sn != NULL);

	bsscfg = pdftm_get_session_tx_bsscfg(sn);
	if (bsscfg == NULL) {
		err = WL_PROXD_E_NOTSF;
		goto done;
	}

	scb = wlc_scbfindband(ftm->wlc, bsscfg, &sn->config->burst_config->peer_mac,
		CHSPEC_BANDUNIT(sn->config->burst_config->chanspec));

	/* on AP/GO use (valid) TSF for the BSS except if the peer is unassociated. In
	 * that case we use BSS TSF if we are the responder, and need to discover
	 * the time reference to use (if required for non-ASAP) if we are the
	 * initiator
	 *
	 * For STA - use the TSF for the BSS except for unassociated infra case for
	 * which there is a need to discover the TSF to use (if required).
	 * How we can distinguish between unassociated infra sta and IBSS that
	 * is created but no peers.
	 */
	use_tsf_off = ((sn->ftm_state != NULL) &&
		((sn->ftm_state->flags & FTM_SESSION_STATE_USE_TSF_OFF) != 0));
	if (BSSCFG_AP(bsscfg)) {
		if (!bsscfg->up) {
			err = BCME_NOTUP;
		} else if (!scb || !SCB_ASSOCIATED(scb)) {
			if (FTM_SESSION_IS_INITIATOR(sn) && !use_tsf_off)
					err = WL_PROXD_E_NOTSF;
		}
	} else {
		if ((!scb || !BSSCFG_AS_STA(bsscfg)) && !use_tsf_off &&
			!BSSCFG_NAN(bsscfg)) {
			err = WL_PROXD_E_NOTSF;
		}
	}

	FTM_LOGSN(ftm, (("wl%d.%d: %s: err %d, use_tsf_off %d bss %d:%d:%d scb %p:%d\n",
		FTM_UNIT(ftm), WLC_BSSCFG_IDX(bsscfg), __FUNCTION__,
		err, use_tsf_off, BSSCFG_AP(bsscfg), bsscfg->up, BSSCFG_AS_STA(bsscfg),
		OSL_OBFUSCATE_BUF(scb), scb ? SCB_ASSOCIATED(scb) : 0)));

	if (err != BCME_OK)
		goto done;

#ifdef WLMCNX
	if (!use_tsf_off && MCNX_ENAB(FTM_PUB(ftm)) &&
			wlc_mcnx_tbtt_valid(FTM_MCNX(ftm), bsscfg)) {
		uint32 tsf_hi, tsf_lo;
		wlc_mcnx_read_tsf64(FTM_MCNX(ftm), bsscfg, &tsf_hi, &tsf_lo);
		tsf = (uint64)tsf_hi << 32 | (uint64)tsf_lo;
	} else
#endif /* WLMCNX */
	{
		if (use_tsf_off) {
			FTM_GET_TSF(ftm, tsf);
		} else {
			FTM_GET_DEV_TSF(ftm, tsf);
		}
	}

	FTM_LOGSN(ftm, (("wl%d: %s: session idx %d current tsf %u.%u\n",
		FTM_UNIT(ftm), __FUNCTION__, sn->idx, FTM_LOG_TSF_ARG(tsf))));

	if (use_tsf_off)
		tsf += sn->tsf_off;

done:
	if (session_tsf)
		*session_tsf = tsf;
	FTM_LOG_STATUS(ftm, err,
		(("wl%d: %s: status %d session idx %d tsf %u.%u\n",
		FTM_UNIT(ftm), __FUNCTION__, err, sn->idx, FTM_LOG_TSF_ARG(tsf))));
	return err;
}

uint64
pdftm_convert_tsf(const pdftm_session_t *sn, uint64 tsf, bool local)
{
	int err;
	uint64 delta;
	uint64 local_tsf;
	uint64 sn_tsf;
	pdftm_t *ftm = sn->ftm;
	err = pdftm_get_session_tsf(sn, &sn_tsf);
	if (err != BCME_OK)
		goto done;

	FTM_GET_TSF(ftm, local_tsf);
	if (local) {
		delta = tsf - local_tsf;
		tsf = sn_tsf + delta;
	} else {
		delta = tsf - sn_tsf;
		tsf = local_tsf + delta;
	}

done:
	return tsf;
}

static int
ftm_sn_alloc_sid(pdftm_t *ftm, wl_proxd_session_id_t *out_sid)
{
	wl_proxd_session_id_t sid;
	int limit;
	int err;
	pdftm_cmn_t *ftm_cmn = ftm->ftm_cmn;
	ASSERT(out_sid != NULL);

	err = BCME_NORESOURCE;
	for (limit = 0, sid = ftm_cmn->last_sid; limit <= ftm_cmn->max_sessions; ++limit) {
		sid = (sid + 1) | 0x8000;
		if (FTM_SESSION_FOR_SID(ftm, sid))
			continue;
		err = BCME_OK;
		break;
	}

	if (err == BCME_OK) {
		ftm_cmn->last_sid = sid;
		*out_sid = sid;
	}

	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d\n", FTM_UNIT(ftm),
		__FUNCTION__, err)));
	return err;
}

static bool
ftm_session_match(pdftm_session_t *sn,
	wl_proxd_session_id_t *sid, const struct ether_addr *peer_mac,
	wl_proxd_session_flags_t flags, wl_proxd_session_state_t state)
{
	bool match = FALSE;

	do {
		if (!FTM_VALID_SESSION(sn))
			break;

		if (sid != NULL && (*sid) != sn->sid)
			break;

		if (peer_mac != NULL) {
			if (memcmp(peer_mac, &sn->config->burst_config->peer_mac,
					sizeof(*peer_mac)))
				break;
		}

		if ((flags & sn->config->flags) != flags)
			break;

		if ((state != WL_PROXD_SESSION_STATE_NONE) && (state != sn->state))
			break;

		match = TRUE;
	} while (0);

	return match;
}

int
pdftm_session_resolve_ratespec(pdftm_t *ftm, chanspec_t cspec, ratespec_t *rspec)
{
	int err = BCME_OK;
	ratespec_t ratespec = 0;
	uint32 bw;
#ifdef WL11AC
	int bandtype;
#endif /* WL11AC */

	ASSERT(rspec != NULL);

	/* TBD - do we need other device caps to resolve? 80p80 ? 11ad? */
	bw = CHSPEC_BW(cspec);
#ifdef WL11AC
	bandtype = CHSPEC_IS5G(cspec) ? WLC_BAND_5G : WLC_BAND_2G;
#endif /* WL11AC */
	if (VHT_ENAB_BAND(FTM_PUB(ftm), bandtype)) {
		ratespec = VHT_RSPEC(0, FTM_NSS);
		switch (bw) {
		case WL_CHANSPEC_BW_20:
			ratespec |= WL_RSPEC_BW_20MHZ;
			break;
		case WL_CHANSPEC_BW_40:
			ratespec |= WL_RSPEC_BW_40MHZ;
			break;
		case WL_CHANSPEC_BW_80:
			ratespec |= WL_RSPEC_BW_80MHZ;
			break;
		case WL_CHANSPEC_BW_160:
			ratespec |= WL_RSPEC_BW_160MHZ;
			break;
		default:
			err = BCME_BADCHAN;
			break;
		}
	} else if (N_ENAB(FTM_PUB(ftm))) {
		ratespec = HT_RSPEC(0);
		switch (bw) {
		case WL_CHANSPEC_BW_20:
			ratespec |= WL_RSPEC_BW_20MHZ;
			break;
		case WL_CHANSPEC_BW_40:
			ratespec |= WL_RSPEC_BW_40MHZ;
			break;
		default:
			err = BCME_BADOPTION;
			break;
		}
	} else {
		ratespec = LEGACY_RSPEC(WLC_RATE_6M) | WL_RSPEC_BW_20MHZ;
	}

	if (err != BCME_OK)
		goto done;

	*rspec =  ratespec;

done:
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d for chanspec 0x%04x ratespec 0x%08x\n",
		FTM_UNIT(ftm), __FUNCTION__, err, cspec, ratespec)));
	return err;
}

int
ftm_session_validate_chanspec(pdftm_t *ftm, pdftm_session_t *sn)
{
	wlc_bsscfg_t *bsscfg;
	scb_t *scb;
	int err = BCME_OK;
	chanspec_t cspec = 0;
	uint8 ctl_chan = 0;
	uint16 bw = 0;
	chanspec_t a_cspec = 0;
	uint8 a_ctl_chan = 0;
	uint16 a_bw = 0;

	bsscfg = sn->bsscfg;
	if (!bsscfg->up)
		goto done;

	cspec = sn->config->burst_config->chanspec;
	ctl_chan = wf_chspec_ctlchan(cspec);
	bw = CHSPEC_BW(cspec);

	/* if associated, ensure burst chanspec is a valid sub-channel of assoc chanspec */
	scb = wlc_scbfindband(ftm->wlc, bsscfg, &sn->config->burst_config->peer_mac,
		CHSPEC_BANDUNIT(cspec));
	if (!scb || !SCB_ASSOCIATED(scb))
		goto done;

	a_cspec = bsscfg->current_bss->chanspec;
	a_ctl_chan = wf_chspec_ctlchan(a_cspec);
	a_bw = CHSPEC_BW(a_cspec);

	if ((a_ctl_chan != ctl_chan) ||
		((a_bw == WL_CHANSPEC_BW_8080) && (bw == WL_CHANSPEC_BW_160)) || (a_bw < bw)) {
		err = BCME_BADCHAN;
		goto done;
	}

done:
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d cspec 0x%04x a_cspec 0x%04x"
		" chan %d a_chan %d bw 0x%04x a_bw 0x%04x.\n",
		FTM_UNIT(ftm), __FUNCTION__, err, cspec, a_cspec, ctl_chan, a_ctl_chan, bw, a_bw)));
	return err;
}

static int
ftm_session_validate_caps(pdftm_t *ftm, pdftm_session_t *sn)
{
	pdftm_session_config_t *sncfg;
	wlc_bsscfg_t *bsscfg;
	scb_t *scb;
	int err = BCME_OK;

	ASSERT(FTM_VALID_SESSION(sn));

	bsscfg = sn->bsscfg;
	if (!bsscfg->up)
		goto done;

	sncfg = sn->config;
	scb = wlc_scbfindband(ftm->wlc, bsscfg, &sn->config->burst_config->peer_mac,
		CHSPEC_BANDUNIT(sncfg->burst_config->chanspec));
	if (!scb || !SCB_ASSOCIATED(scb))
		goto done;

	/* for local initator, peer is responder capable; for local responder,
	 * peer must be capable of initiating
	 */
	if (sncfg->flags & WL_PROXD_SESSION_FLAG_INITIATOR) {
		if (isclr(bsscfg->ext_cap, DOT11_EXT_CAP_FTM_INITIATOR)) {
			err = WL_PROXD_E_INCAPABLE;
		} else if (!SCB_FTM_RESPONDER(scb)) {
			FTM_LOGSN(ftm, (("wl%d: %s: status %d - forcing one-way\n",
				FTM_UNIT(ftm), __FUNCTION__, WL_PROXD_E_REMOTE_INCAPABLE)));
			sncfg->flags |= WL_PROXD_SESSION_FLAG_ONE_WAY;
		}
	} else {
		if (isclr(bsscfg->ext_cap, DOT11_EXT_CAP_FTM_RESPONDER)) {
			err = WL_PROXD_E_INCAPABLE;
		} else if (!SCB_FTM_INITIATOR(scb)) {
			FTM_LOGSN(ftm, (("wl%d: %s: status %d - ignoring\n",
				FTM_UNIT(ftm), __FUNCTION__, WL_PROXD_E_REMOTE_INCAPABLE)));
		}
	}

	/* use vht ack - if peer is brcm and negotiated rate is vht */
	if (FTM_SESSION_IS_AUTO_VHTACK(sn) && SCB_IS_BRCM(scb) && SCB_VHT_CAP(scb))
		sncfg->flags |= WL_PROXD_SESSION_FLAG_VHTACK;

done:
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d\n", FTM_UNIT(ftm), __FUNCTION__, err)));
	return err;
}

uint16
pdftm_resolve_num_ftm(pdftm_t *ftm, wl_proxd_session_flags_t flags,
	chanspec_t cspec)
{
	uint16 tunep_idx;
	wl_proxd_params_tof_tune_t *tunep;
	uint16 num_ftm;

	tunep_idx = proxd_get_tunep_idx(ftm->wlc, flags, cspec, &tunep);
	if (!tunep)
		num_ftm = FTM_SESSION_DEFAULT_BURST_NUM_FTM;
	else
		num_ftm = tunep->ftm_cnt[tunep_idx];

	return num_ftm;
}

/* public interface */

pdftm_session_t*
pdftm_find_session(pdftm_t *ftm, wl_proxd_session_id_t *sid,
	const struct ether_addr *peer_mac,
	wl_proxd_session_flags_t flags,
	wl_proxd_session_state_t state)
{
	pdftm_session_t *sn = NULL;
	int i = 0;
	pdftm_cmn_t *ftm_cmn = ftm->ftm_cmn;

	/* note: use max_sessions, not num_sessions */
	for (i = ftm_cmn->mru_idx; i < ftm_cmn->max_sessions; ++i) {
		sn = ftm_cmn->sessions[i];
		if (ftm_session_match(sn, sid, peer_mac, flags, state))
			goto done;
	}

	for (i = 0; i < ftm_cmn->mru_idx; ++i) {
		sn = ftm_cmn->sessions[i];
		if (ftm_session_match(sn, sid, peer_mac, flags, state))
			goto done;
	}

	sn = NULL;
done:
	if (sn)
		ftm_cmn->mru_idx = sn->idx;
	return sn;
}

void
pdftm_copy_session_config(pdftm_t *ftm, pdftm_session_config_t *dst,
	const pdftm_session_config_t *src)
{
	pdburst_config_t *bcfg;

	ASSERT(FTM_VALID(ftm));
	ASSERT(dst != NULL && dst->burst_config != NULL);
	if (!src)
		goto done;

	/* save burst config ptr and restore after copy */
	bcfg = dst->burst_config;
	memcpy(dst, src, sizeof(*dst));
	dst->burst_config = bcfg;
	if (!src->burst_config)
		goto done;
	memcpy(bcfg, src->burst_config, sizeof(*bcfg));

done:
	FTM_LOGSN(ftm, (("wl%d: %s: copied %p:%p to %p.%p\n",
		FTM_UNIT(ftm), __FUNCTION__, OSL_OBFUSCATE_BUF(src),
		src ? OSL_OBFUSCATE_BUF(src->burst_config) : NULL,
		OSL_OBFUSCATE_BUF(dst),
		dst ? OSL_OBFUSCATE_BUF(dst->burst_config) : NULL)));
}

pdftm_session_config_t*
pdftm_alloc_session_config(pdftm_t *ftm)
{
	pdftm_session_config_t *sncfg;
	ASSERT(FTM_VALID(ftm));

	sncfg = MALLOCZ(FTM_OSH(ftm), sizeof(*sncfg));
	if (!sncfg)
		goto done;

	sncfg->burst_config = MALLOCZ(FTM_OSH(ftm), sizeof(*sncfg->burst_config));
	if (!sncfg->burst_config)
		pdftm_free_session_config(ftm, &sncfg);

done:
	FTM_LOGSN(ftm, (("wl%d: %s: allocated %p:%p\n",
		FTM_UNIT(ftm), __FUNCTION__, OSL_OBFUSCATE_BUF(sncfg),
		sncfg ? OSL_OBFUSCATE_BUF(sncfg->burst_config) : NULL)));
	return sncfg;
}

void
pdftm_free_session_config(pdftm_t *ftm, pdftm_session_config_t **in_config)
{
	pdftm_session_config_t *sncfg;

	ASSERT(FTM_VALID(ftm));

	if (!in_config || !*in_config)
		goto done;

	sncfg = *in_config;
	*in_config = NULL;

	FTM_LOGSN(ftm, (("wl%d: %s: freeing %p:%p\n",
		FTM_UNIT(ftm), __FUNCTION__, OSL_OBFUSCATE_BUF(sncfg),
		OSL_OBFUSCATE_BUF(sncfg->burst_config))));

	if (sncfg->burst_config)
		MFREE(FTM_OSH(ftm), sncfg->burst_config, sizeof(*sncfg->burst_config));
	 MFREE(FTM_OSH(ftm), sncfg, sizeof(*sncfg));
done:;
}

int
pdftm_alloc_session(pdftm_t *ftm, wlc_bsscfg_t *bsscfg,
	wl_proxd_session_id_t sid, pdftm_session_t **out_sn,
	bool *created)
{
	int err = BCME_OK;
	pdftm_session_t *sn = NULL;
	pdftm_session_idx_t idx;
	pdftm_cmn_t *ftm_cmn = ftm->ftm_cmn;
	ASSERT(out_sn != NULL);
	ASSERT(created != NULL);
	ASSERT(bsscfg != NULL);

	*created = FALSE;
	sn = FTM_SESSION_FOR_SID(ftm, sid);
	if (sn) {
		goto done;
	}

	if (!FTM_BSSCFG_FTM_ENABLED(ftm->ftm_cmn, bsscfg)) {
		err = BCME_DISABLED;
		goto done;
	}

	if (sid == WL_PROXD_SESSION_ID_GLOBAL) {
		err = ftm_sn_alloc_sid(ftm, &sid);
		if (err != BCME_OK) {
			goto done;
		}
	}

	idx = ftm_sn_get_free_idx(ftm);
	if (idx == FTM_SESSION_INVALID_IDX) {
		err = BCME_NORESOURCE;
		goto done;
	}

	/* re-use already allocated (invalid) sn or allocate one */
	if (ftm_cmn->sessions[idx] != NULL) {
		sn = ftm_cmn->sessions[idx];
		/* note: sn not cleared, all fields initialized below */
	} else {
		sn = MALLOCZ(FTM_OSH(ftm), sizeof(pdftm_session_t));
		if (sn) {
			sn->config = pdftm_alloc_session_config(ftm);
			sn->cnt = MALLOCZ(FTM_OSH(ftm), sizeof(*sn->cnt));
			sn->session_records = MALLOCZ(FTM_OSH(ftm),
				sizeof(pdftm_session_record_t) * FTM_SESSION_MAX_RECORDS);
		}

		if (!sn || !sn->config || !sn->cnt || !sn->session_records) {
			err = BCME_NOMEM;
			if (sn && sn->config) {
				pdftm_free_session_config(ftm, &sn->config);
			}
			if (sn && sn->cnt) {
				MFREE(FTM_OSH(ftm), sn->cnt, sizeof(*sn->cnt));
			}
			if (sn && sn->session_records) {
				MFREE(FTM_OSH(ftm), sn->session_records,
					(sizeof(pdftm_session_record_t) * FTM_SESSION_MAX_RECORDS));
			}
			if (sn) {
				MFREE(FTM_OSH(ftm), sn, sizeof(*sn));
			}

			goto done;
		}

		ftm_cmn->sessions[idx] = sn;
		++(ftm_cmn->num_sessions);
	}

	sn->ftm = ftm;
	sn->idx = idx;
	sn->flags = FTM_SESSION_VALID;
	sn->sid = sid;
	sn->bsscfg = bsscfg;
	sn->ftm1_retrycnt = 0;
	sn->scan_retry_attempt = 0;

	/* init config w/ ftm defaults */
	ASSERT(sn->config != NULL && sn->config->burst_config != NULL);
	pdftm_copy_session_config(ftm, sn->config, ftm_cmn->config->session_defaults);

	sn->config->peer_avail = NULL;
	sn->state = WL_PROXD_SESSION_STATE_NONE;
	sn->status = BCME_OK;

	/* Init session state records/history */
	sn->session_rec_idx = 0;
	bzero(sn->session_records, (sizeof(pdftm_session_record_t) * FTM_SESSION_MAX_RECORDS));
	pdftm_record_session_state(sn);

	/* init counters */
	bzero(sn->cnt, sizeof(*sn->cnt));

	/* ftm state is allocated on session start */
	ASSERT(sn->ftm_state == NULL);
	sn->tsf_off = 0;
	sn->tsf_scan_state = FTM_TSF_SCAN_STATE_NONE;

	++(ftm_cmn->cnt->sessions);
	if (ftm_cmn->cnt->max_sessions < ftm_cmn->cnt->sessions) {
		ftm_cmn->cnt->max_sessions = ftm_cmn->cnt->sessions;
	}

	*created  = TRUE;	/* even if re-used */
	err = pdftm_change_session_state(sn, BCME_OK, WL_PROXD_SESSION_STATE_CREATED);
	if (err != BCME_OK) {
		goto done;
	}

done:
	if (err != BCME_OK) {
		pdftm_free_session(&sn);
	}

	*out_sn = sn;

	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d session %p\n",
		FTM_UNIT(ftm), __FUNCTION__, err, OSL_OBFUSCATE_BUF(sn))));
	return err;
}

static int
ftm_session_resolve_bsscfg(pdftm_t *ftm, pdftm_session_t *sn)
{
	int err = BCME_OK;
	const struct ether_addr *peer;
	wlc_bsscfg_t *bsscfg;
	scb_t *scb;

	peer = &sn->config->burst_config->peer_mac;
	bsscfg = sn->bsscfg;
	scb = wlc_scbfind(ftm->wlc, bsscfg, peer);
	if (scb != NULL) {
		bsscfg = SCB_BSSCFG(scb);
	} else {
		if (BSSCFG_AP(bsscfg) && bsscfg->up)
			goto resolved;
		if (BSSCFG_SLOTTED_BSS(bsscfg)) {
			/* taken care by AWDL or NAN */
			goto resolved;
		}
		bsscfg = pdsvc_get_bsscfg(ftm->pd);
	}

resolved:
	ASSERT(bsscfg != NULL);
	FTM_LOGSN(ftm, (("wl%d: %s: session idx %d updating bss idx %d to %d\n",
		FTM_UNIT(ftm), __FUNCTION__, sn->idx,
		WLC_BSSCFG_IDX(sn->bsscfg), WLC_BSSCFG_IDX(bsscfg))));

	sn->bsscfg = bsscfg;

	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d session idx %d\n",
		FTM_UNIT(ftm), __FUNCTION__, err, sn->idx)));
	return err;
}

int
pdftm_validate_session(pdftm_session_t *sn)
{
	int initiator;
	int target;
	pdftm_session_config_t *sncfg;
	int err = BCME_OK;
	pdftm_session_t *other_sn;
	uint32 dur_ms, dur_usec;
	uint32 sep;
	uint8 bexp;
	wl_proxd_params_tof_tune_t *tunep;
	pdftm_t *ftm;

	if (!FTM_VALID_SESSION(sn)) {
		err = WL_PROXD_E_INVALID_SESSION;
		goto done;
	}
	ftm = sn->ftm;
	sncfg = sn->config;

	/* initiator and target are mutually exclusive */
	initiator = (sncfg->flags & WL_PROXD_SESSION_FLAG_INITIATOR) ? 1 : 0;
	target = (sncfg->flags & WL_PROXD_SESSION_FLAG_TARGET) ? 1 : 0;
	if (!(initiator ^ target)) {
		err = BCME_BADOPTION;
		goto done;
	}

	/* all initiator sids must be externally allocated */
	if (initiator && !WL_PROXD_SID_EXT_ALLOC(sn->sid)) {
		err = WL_PROXD_E_INVALID_SID;
		goto done;
	}

	if (ETHER_ISNULLADDR(sncfg->burst_config->peer_mac.octet) ||
		ETHER_ISMULTI(sncfg->burst_config->peer_mac.octet)) {
		err = BCME_BADADDR;
		goto done;
	}

	/* allow only one initiator or target session per peer mac */
	other_sn = pdftm_find_session(ftm, NULL, &sncfg->burst_config->peer_mac,
		initiator ? WL_PROXD_SESSION_FLAG_INITIATOR : WL_PROXD_SESSION_FLAG_TARGET,
		WL_PROXD_SESSION_STATE_NONE);

	if (other_sn != sn) {
		err = WL_PROXD_E_DUP_SESSION;
		goto done;
	}

	(void)ftm_session_resolve_bsscfg(ftm, sn);

	/* note: if peer avail is used, chanspec and ratespec are resolved (again)
	 * during scheduling.
	 */
	if (!sncfg->burst_config->chanspec)
		sncfg->burst_config->chanspec = FTM_SESSION_CHANSPEC(ftm, sn, sn->bsscfg);
	else
		err = ftm_session_validate_chanspec(ftm, sn);

	if (err != BCME_OK)
		goto done;

	/* adjust ftm sep/min delta to be a multiple of 100us (see spec) */
	if (!sncfg->burst_config->ftm_sep.intvl)
		sep = pdftm_resolve_ftm_sep(ftm, sn->config->flags, sncfg->burst_config->chanspec);
	else
		sep = FTM_INTVL2USEC(&sncfg->burst_config->ftm_sep);

	sep = ROUNDUP(sep, 100);
	FTM_INIT_INTVL(&sncfg->burst_config->ftm_sep, sep, WL_PROXD_TMU_MICRO_SEC);

	/* adjust num ftm */
	if (!sncfg->burst_config->num_ftm) {
		sncfg->burst_config->num_ftm =
			pdftm_resolve_num_ftm(ftm, sn->config->flags,
				sncfg->burst_config->chanspec);
	}

	/* resolve burst duration */
	dur_ms = (uint32)FTM_INTVL2MSEC(&sncfg->burst_config->duration);
	if (dur_ms > FTM_PARAMS_BURSTTMO_MAX_MSEC) {
		dur_ms = FTM_PARAMS_BURSTTMO_MAX_MSEC;
		FTM_INIT_INTVL(&sncfg->burst_config->duration, dur_ms,
			WL_PROXD_TMU_MILLI_SEC);
	} else if (!dur_ms) {
		FTM_SESSION_INIT_BURST_DURATION_US(dur_usec,
			sncfg->burst_config->num_ftm, sep,
			sncfg->burst_config->ftm_retries);
		dur_usec = ROUNDUP(dur_usec, 1000);
		dur_ms = FTM_MICRO2MILLI(dur_usec);
		FTM_INIT_INTVL(&sncfg->burst_config->duration, dur_ms,
			WL_PROXD_TMU_MILLI_SEC);
	}

	if (!sncfg->num_burst) {
		err = BCME_BADOPTION;
		goto done;
	}

	/* adjust num_burst to be power of 2 - spec specifies only exponent */
	FTM_PROTO_BEXP(sncfg->num_burst, bexp);
	sncfg->num_burst = 1 << bexp;

	if (!sncfg->burst_config->ratespec) {
		err = pdftm_session_resolve_ratespec(ftm, sncfg->burst_config->chanspec,
			&sncfg->burst_config->ratespec);
	} else {
		err = pdftm_validate_ratespec(ftm, sn);
	}

	if (err != BCME_OK) {
		goto done;
	}

	/* enable bcm specific vht ack - if negotiated rate is vht */
	sncfg->flags &= ~WL_PROXD_SESSION_FLAG_VHTACK;
	tunep = proxd_get_tunep(ftm->wlc, NULL);
	if (tunep && tunep->vhtack) {
		sncfg->flags |= WL_PROXD_SESSION_FLAG_VHTACK;
	}

	/* note: need to do this after chanspec is validated */
	err = ftm_session_validate_caps(ftm, sn);
	if (err != BCME_OK) {
		goto done;
	}

	/* ensure that burst period is sufficient */
	if (FTM_INTVL2MSEC(&sncfg->burst_period) < dur_ms) {
		FTM_INIT_INTVL(&sncfg->burst_period, dur_ms << 1, WL_PROXD_TMU_MILLI_SEC);
	}

	/* validate security - note: needed even if session is not secure */
	err = pdftm_sec_validate_session(ftm, sn);
	if (FTM_SESSION_IS_INITIATOR(sn) && (err == WL_PROXD_E_SEC_NOKEY)) {
		sn->flags |= FTM_SESSION_KEY_WAIT;
		err = BCME_OK;
	}

	if (err != BCME_OK) {
		goto done;
	}

	/* validate session for ext scheduler */
	err = pdftm_sched_validate_session(ftm, sn);
	if (err == WL_PROXD_E_EXT_SCHED) {
		sn->flags |= FTM_SESSION_EXT_SCHED_WAIT;
		err = BCME_OK;
	}

	if (err != BCME_OK) {
		goto done;
	}
done:
	return err;
}

void
pdftm_free_session(pdftm_session_t **in_sn)
{
	pdftm_session_t *sn = NULL;
	pdftm_session_config_t *sncfg;
	pdftm_t *ftm;
	pdftm_cmn_t *ftm_cmn;
	if (!in_sn || !*in_sn)
		goto done;

	sn = *in_sn;
	*in_sn = NULL;
	ftm = sn->ftm;
	ftm_cmn = ftm->ftm_cmn;

	if (sn->ftm1_tx_timer) {
		wlc_hrt_del_timeout(sn->ftm1_tx_timer);
		wlc_hrt_free_timeout(sn->ftm1_tx_timer);
		sn->ftm1_tx_timer = NULL;
	}

#ifdef WL_FTM_MSCH
	(void)pdftm_msch_end(ftm, sn);
#endif /* WL_FTM_MSCH */

	if (sn->ftm_state) {
		pdftm_session_state_t *s = sn->ftm_state;
		if (s->rtt)
			MFREE(FTM_OSH(ftm), s->rtt, s->max_rtt * sizeof(*s->rtt));
		MFREE(FTM_OSH(ftm), s, sizeof(*s));
		sn->ftm_state = NULL;
	}
#ifdef WL_FTM_11K
	if (sn->lci_req) {
		MFREE(FTM_OSH(ftm), sn->lci_req, BCM_TLV_SIZE(sn->lci_req));
		sn->lci_req = NULL;
	}
	if (sn->civic_req) {
		MFREE(FTM_OSH(ftm), sn->civic_req, BCM_TLV_SIZE(sn->civic_req));
		sn->civic_req = NULL;
	}
	if (sn->lci_rep) {
		MFREE(FTM_OSH(ftm), sn->lci_rep, sn->lci_rep_len);
		sn->lci_rep = NULL;
	}
	if (sn->civic_rep) {
		MFREE(FTM_OSH(ftm), sn->civic_rep, sn->civic_rep_len);
		sn->civic_rep = NULL;
	}
#endif /* WL_FTM_11K */
	sncfg = sn->config;
	if (sncfg->peer_avail) {
		MFREE(FTM_OSH(ftm), sncfg->peer_avail,
			WL_PROXD_AVAIL_SIZE(sncfg->peer_avail, sncfg->peer_avail->max_slots));
		sncfg->peer_avail = NULL;
	}

	sn->flags = 0; /* invalidate for re-use */
	SAFE_DECR(ftm_cmn->cnt->sessions);

done:
	if (sn != NULL) {
		FTM_LOGSN(sn->ftm, (("wl%d: %s: cleaning up session idx %d\n", FTM_UNIT(sn->ftm),
			__FUNCTION__, sn ? sn->idx : -1)));
	}
}

void
pdftm_free_sessions(pdftm_t *ftm, wlc_bsscfg_t *bsscfg, bool detach)
{
	int i;
	pdftm_cmn_t *ftm_cmn = ftm->ftm_cmn;
	FTM_LOGSN(ftm, (("wl%d: %s: cleaning up - %sdetach\n",
		FTM_UNIT(ftm), __FUNCTION__, detach ? "" : "not ")));

	if (FTM_BSSCFG_SECURE(wlc_ftm_get_handle(ftm->wlc), bsscfg)) {
		phy_rxgcrs_sel_classifier((phy_info_t *) WLC_PI(ftm->wlc),
			TOF_CLASSIFIER_BPHY_ON_OFDM_ON);
	}

	if (!ftm_cmn->sessions || !ftm_cmn->max_sessions)
		goto done;

	for (i = 0; i < ftm_cmn->max_sessions; ++i) {
		pdftm_session_t *sn = ftm_cmn->sessions[i];
		if (!FTM_VALID_SESSION(sn))
			continue;
		if (bsscfg != NULL && bsscfg != sn->bsscfg)
			continue;

		if (!(sn->state == WL_PROXD_SESSION_STATE_STOPPING ||
			sn->state == WL_PROXD_SESSION_STATE_ENDED ||
			sn->state == WL_PROXD_SESSION_STATE_DESTROYING)) {
			(void)pdftm_change_session_state(sn, BCME_OK,
				WL_PROXD_SESSION_STATE_STOPPING);
		}
	}

	if (!detach)
		goto done;

	/* release all memory */
	for (i = 0; i < ftm_cmn->max_sessions; ++i) {
		pdftm_session_t *sn = ftm_cmn->sessions[i];
		if (!sn) {
			continue;
		}

		pdftm_free_session_config(ftm, &sn->config);
		if (sn->cnt) {
			MFREE(FTM_OSH(ftm), sn->cnt, sizeof(*sn->cnt));
		}

		if (sn->session_records) {
			MFREE(FTM_OSH(ftm), sn->session_records,
				(sizeof(pdftm_session_record_t) * FTM_SESSION_MAX_RECORDS));
			sn->session_records = NULL;
			sn->session_rec_idx = 0;
		}

		ASSERT(sn->ftm_state == NULL);
		MFREE(FTM_OSH(ftm), sn, sizeof(*sn));
		ftm_cmn->sessions[i] = NULL;
		SAFE_DECR(ftm_cmn->num_sessions);
	}

	MFREE(FTM_OSH(ftm), ftm_cmn->sessions, sizeof(pdftm_session_t *) * ftm_cmn->max_sessions);
	ftm_cmn->max_sessions = 0;
done:;
}

int
pdftm_start_session(pdftm_session_t *sn)
{
	int err;
	wl_proxd_session_state_t new_state;
	pdftm_t *ftm = sn->ftm;
	wlc_bsscfg_t *bsscfg = sn->bsscfg;
	ASSERT(bsscfg != NULL);
	(void)bsscfg;
	BCM_REFERENCE(bsscfg);

	err = pdftm_validate_session(sn);
	if (err != BCME_OK)
		goto done;

	if (sn->state > WL_PROXD_SESSION_STATE_CONFIGURED) {
		err = WL_PROXD_E_BAD_STATE;
		goto done;
	}

	/* allocate ftm state - includes space for results */
	ASSERT(sn->ftm_state == NULL);
	sn->ftm_state = MALLOCZ(FTM_OSH(ftm), sizeof(*sn->ftm_state));
	if (!sn->ftm_state) {
		err = BCME_NOMEM;
		goto done;
	}

	sn->ftm_state->max_rtt = sn->config->burst_config->num_ftm;
	if (FTM_SESSION_IS_TARGET(sn)) {
		sn->ftm1_tx_timer = wlc_hrt_alloc_timeout(ftm->wlc->hrti);
		if (!sn->ftm1_tx_timer) {
			err = BCME_NOMEM;
			goto done;
		}
	}

	/* for target irrespective of bss type it is on on_chan by this time
	* for initiator
	* i. legacy bss - on_chan is set in msch call back
	* ii. slotted_bss - on_chan is set in extsched_callback api
	*/
	if (FTM_SESSION_IS_TARGET(sn)) {
		sn->flags |= FTM_SESSION_ON_CHAN;
	}

	/* choose START_WAIT or STARTED depending on session wait mask */
	new_state = (FTM_SESSION_IN_START_WAIT(sn) ?
			WL_PROXD_SESSION_STATE_START_WAIT : WL_PROXD_SESSION_STATE_STARTED);
	err = pdftm_change_session_state(sn, BCME_OK, new_state);

	if (err != BCME_OK)
		goto done;

done:
	if (err != BCME_OK) {
		if (sn->ftm_state) {
			MFREE(FTM_OSH(ftm), sn->ftm_state, sizeof(*sn->ftm_state));
		}
		if (sn->ftm1_tx_timer) {
			wlc_hrt_free_timeout(sn->ftm1_tx_timer);
			sn->ftm1_tx_timer = NULL;
		}
	}
	return err;
}

int
pdftm_stop_session(pdftm_session_t *sn, wl_proxd_status_t status)
{
	int err = BCME_OK;

	ASSERT(sn->ftm != NULL && sn != NULL);
	ASSERT(sn->ftm->ftm_cmn->sessions[sn->idx] == sn);

	if (!FTM_VALID_SESSION(sn)) {
		err = BCME_BADARG;
		goto done;
	}

	err = pdftm_change_session_state(sn, status, WL_PROXD_SESSION_STATE_STOPPING);
	if (err != BCME_OK)
		goto done;

done:
	return err;
}

bool
pdftm_session_is_onchan(pdftm_session_t *sn)
{
	bool onchan = FALSE;

	if (sn->flags & FTM_SESSION_ON_CHAN) {
		onchan = TRUE;
	}
	return onchan;
}

void
pdftm_init_session_defaults(pdftm_t *ftm, pdftm_session_config_t *sncfg)
{
	pdftm_cmn_t *ftm_cmn = ftm->ftm_cmn;
	ASSERT(FTM_VALID(ftm) && sncfg != NULL);

	sncfg->event_mask = ftm_cmn->config->event_mask;
	if (ftm_cmn->config->flags & WL_PROXD_FLAG_RX_AUTO_BURST)
		sncfg->flags |= WL_PROXD_SESSION_FLAG_RX_AUTO_BURST;
	if (ftm_cmn->config->flags & WL_PROXD_FLAG_TX_AUTO_BURST)
		sncfg->flags |= WL_PROXD_SESSION_FLAG_TX_AUTO_BURST;
	if (ftm_cmn->config->flags & WL_PROXD_FLAG_ASAP_CAPABLE)
		sncfg->flags |= WL_PROXD_SESSION_FLAG_ASAP;
	sncfg->flags |= WL_PROXD_SESSION_FLAG_PRE_SCAN;
	sncfg->flags |= WL_PROXD_SESSION_FLAG_AUTO_VHTACK;
	sncfg->flags |= WL_PROXD_SESSION_FLAG_MBURST_NODELAY;
	sncfg->num_burst = FTM_SESSION_DEFAULT_NUM_BURST;

	/* no default here - resolved based on chanspec */
	sncfg->burst_config->num_ftm = 0;

	sncfg->burst_config->ftm_retries = FTM_SESSION_DEFAULT_RETRIES;
	sncfg->burst_config->chanspec = FTM_SESSION_DEFAULT_CHANSPEC;
	sncfg->burst_config->ratespec = FTM_SESSION_DEFAULT_RATESPEC;
	sncfg->burst_config->tx_power = FTM_SESSION_DEFAULT_TX_POWER;
	sncfg->burst_config->ftm_req_retries = FTM_SESSION_DEFAULT_RETRIES;

	/* no default here - either configured or based on chanspec/sn flags */
	FTM_INIT_INTVL(&sncfg->burst_config->ftm_sep, 0, WL_PROXD_TMU_MICRO_SEC);

	FTM_INIT_INTVL(&sncfg->burst_config->timeout,
		FTM_SESSION_DEFAULT_BURST_TIMEOUT_MS, WL_PROXD_TMU_MILLI_SEC);
	FTM_INIT_INTVL(&sncfg->burst_config->duration,
		0, WL_PROXD_TMU_MILLI_SEC);
	FTM_INIT_INTVL(&sncfg->burst_period, FTM_SESSION_DEFAULT_BURST_PERIOD_MS,
		WL_PROXD_TMU_MILLI_SEC);
}

const pdburst_config_t *
pdftm_get_burst_config(pdftm_t *ftm, pdftm_session_t *sn, wl_proxd_session_flags_t *flags)
{
	pdftm_session_config_t *sncfg;
	pdftm_cmn_t *ftm_cmn = ftm->ftm_cmn;
	ASSERT(FTM_VALID(ftm));
	sncfg = !sn ? ftm_cmn->config->session_defaults : sn->config;
	if (sncfg) {
		*flags = sncfg->flags;
		return sncfg->burst_config;
	}
	return NULL;
}

void
pdftm_record_session_state(pdftm_session_t *sn)
{
	pdftm_t *ftm = NULL;
	ASSERT(sn);

	if (sn->session_rec_idx == FTM_SESSION_MAX_RECORDS) {
		sn->session_rec_idx = 0;
	}
	ftm = sn->ftm;
	FTM_GET_TSF(ftm, sn->session_records[sn->session_rec_idx].state_ts);
	sn->session_records[sn->session_rec_idx].status = sn->status;
	sn->session_records[sn->session_rec_idx++].state = sn->state;
}

int
pdftm_change_session_state(pdftm_session_t *sn, wl_proxd_status_t status,
	wl_proxd_session_state_t new_state)
{
	ftm_session_state_handler_t handler;
	int err = BCME_OK;
	wl_proxd_event_type_t event_type = WL_PROXD_EVENT_NONE;
	wl_proxd_session_state_t old_state = WL_PROXD_SESSION_STATE_NONE;
	pdftm_t *ftm = sn->ftm;
	uint8 err_at = 0;

	ASSERT(FTM_VALID(ftm));
	if (!FTM_VALID_SESSION(sn)) {
		err = BCME_BADARG;
		goto done;
	}

	old_state = sn->state;
	if (old_state == new_state) /* ignore no state change */
		goto done;

	handler = ftm_snsi_get_handler(old_state);
	if (!handler) {
		err = BCME_UNSUPPORTED;
		err_at = 1;
		goto done;
	}

	err = handler(ftm, sn, status, new_state, &event_type);
	if (err != BCME_OK) {
		err_at = 2;
		goto done;
	}
	pdftm_record_session_state(sn);

	new_state = sn->state;

	if (new_state == WL_PROXD_SESSION_STATE_NONE) {
		sn = NULL;
	} else if (event_type != WL_PROXD_EVENT_NONE) {
		pdftm_notify_info_t notify_info;
		FTM_INIT_SESSION_NOTIFY_INFO(&notify_info, sn, &sn->config->burst_config->peer_mac,
			event_type, 0, 0, NULL, 0);
		pdftm_notify(ftm, sn->bsscfg, PDFTM_NOTIF_EVENT_TYPE, &notify_info);

		/* when burst ends and the session also ends, indicate session end */
		if (event_type == WL_PROXD_EVENT_BURST_END &&
			sn->state == WL_PROXD_SESSION_STATE_ENDED) {
			event_type = WL_PROXD_EVENT_SESSION_END;
			FTM_INIT_SESSION_NOTIFY_INFO(&notify_info, sn,
				&sn->config->burst_config->peer_mac,
				event_type, 0, 0, NULL, 0);
			pdftm_notify(ftm, sn->bsscfg, PDFTM_NOTIF_EVENT_TYPE, &notify_info);
		}
	}

done:
#if defined(BCMDBG) || defined(FTM_DONGLE_DEBUG)
	{
		uint64 cur_tsf;
		BCM_REFERENCE(cur_tsf);
		FTM_GET_TSF(ftm, cur_tsf);
		FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d from state %d(%s)->%d(%s) "
			"proxd status %d for session idx %d event %d at %u.%u\n",
			FTM_UNIT(ftm), __FUNCTION__, err,
			old_state, pdftm_get_session_state_name(old_state),
			new_state, pdftm_get_session_state_name(new_state),
			status, sn ? sn->idx : -1, event_type, FTM_LOG_TSF_ARG(cur_tsf))));
	}
#endif /* BCMDBG || FTM_DONGLE_DEBUG */

	if (err != BCME_OK) {
		FTM_INFO(("wl%d: pdftm_change_session_state: err %d err_at %d "
			"from state %d->%d proxd status %d for "
			"session idx %d event %d\n", FTM_UNIT(ftm), err, err_at,
			old_state, new_state, status, sn ? sn->idx : -1, event_type));
	}
	return err;
}

int
pdftm_get_burst_params(pdftm_session_t *sn,
	const uint8 *req, uint req_len, const wlc_d11rxhdr_t *req_wrxh,
	ratespec_t req_rspec, pdburst_params_t *bp)
{
	const pdftm_session_config_t *sncfg;

	ASSERT(FTM_VALID_SESSION(sn));
	sncfg = sn->config;

	bzero(bp, sizeof(*bp));
	bp->flags = sncfg->flags;
	bp->bsscfg = pdftm_get_session_tx_bsscfg(sn);
	bp->config = sncfg->burst_config;

	bp->req =  req;
	bp->req_len = req_len;
	bp->req_wrxh = req_wrxh;
	bp->req_rspec = req_rspec;
	bp->start_tsf = sn->ftm_state->burst_start;
	bp->dialog = sn->ftm_state->dialog;
	return BCME_OK;
}

uint16
pdftm_resolve_ftm_sep(pdftm_t *ftm, wl_proxd_session_flags_t flags, chanspec_t cspec)
{
	uint16 sep;

	if (flags & WL_PROXD_SESSION_FLAG_SEQ_EN) {
		sep = FTM_SEP_SEQ_EN;
	} else if (CHSPEC_IS80(cspec)) {
		sep = FTM_SEP_80M;
	} else if (CHSPEC_IS40(cspec)) {
		sep = FTM_SEP_40M;
	} else {
		sep = FTM_SEP_20M;
	}

#if defined(__ARM_ARCH_7M__)
	/*
	 * (phy = 36) && (arm = cm3)
	 * this will cover all of 43012 based chips
	 */
	if (ACREV_IS(ftm->wlc->band->phyrev, 36)) {
		sep += FTM_SEP_CM3;
	}
#endif /* __ARM_ARCH_7M__ */

	return sep;
}

int
pdftm_validate_ratespec(pdftm_t *ftm, pdftm_session_t *sn)
{
	int err = BCME_OK;
	int bandtype;
	chanspec_t cspec;
	ratespec_t rspec;

	ASSERT(FTM_VALID_SESSION(sn));
	cspec = sn->config->burst_config->chanspec;
	rspec = sn->config->burst_config->ratespec;

	bandtype = CHSPEC_IS5G(cspec) ? WLC_BAND_5G : WLC_BAND_2G;
	if (!wlc_valid_rate(ftm->wlc, rspec, bandtype, FALSE))
		err = BCME_BADRATESET;

	return err;
}

#if defined(BCMDBG) || defined(FTM_DONGLE_DEBUG)
#define C(X) case WL_PROXD_SESSION_STATE_ ## X : { name = #X; break; }
const char*
pdftm_get_session_state_name(wl_proxd_session_state_t state)
{
	const char *name = "unknown";

	switch (state) {
	C(NONE); C(CREATED); C(CONFIGURED); C(START_WAIT); C(STARTED); C(DELAY); C(USER_WAIT);
	C(SCHED_WAIT); C(BURST); C(STOPPING); C(ENDED); C(DESTROYING);
	default: break;
	}
	return name;
}
#undef C
#endif /* BCMDBG || FTM_DONGLE_DEBUG */

int
pdftm_get_session_result(pdftm_t *ftm, pdftm_session_t *sn,
	wl_proxd_rtt_result_t *rp, int num_rtt)
{
	int err = BCME_OK;
	const pdftm_session_state_t *sst;
	int i;

	ASSERT(rp != NULL);
	ASSERT(FTM_VALID_SESSION(sn));
	ASSERT(sn->config != NULL);

	sst = sn->ftm_state;
	if (!sst) {
		err = BCME_NOTFOUND;
		goto done;
	}

	rp->version = WL_PROXD_RTT_SAMPLE_VERSION_2;
	rp->length = sizeof(*rp) - OFFSETOF(wl_proxd_rtt_result_t, sid);
	rp->sid = sn->sid;
	rp->flags = sst->result_flags;
	rp->status = sst->rtt_status;
	memcpy(&rp->peer, &sn->config->burst_config->peer_mac, sizeof(rp->peer));
	rp->state = sn->state;
	rp->u.retry_after = sst->retry_after;
	rp->avg_dist = sst->avg_dist;
	rp->sd_rtt = sst->sd_rtt;
	rp->num_valid_rtt = sst->num_valid_rtt;
	rp->num_ftm = (uint8)sn->config->burst_config->num_ftm;
	rp->burst_num = sst->burst_num;
	rp->num_meas = sst->num_meas;

	/* avg_rtt is the first element */
	rp->rtt[0] = sst->avg_rtt;
	rp->num_rtt = MIN(num_rtt, sst->num_rtt);
	for (i = 0; sst->rtt != NULL && i < rp->num_rtt; ++i)
		rp->rtt[i+1] = sst->rtt[i];

done:
	return err;
}

void *
pdftm_session_get_burstp(wlc_info_t *wlc, struct ether_addr *peer, uint8 flag)
{
	pdftm_session_t *sn;
	sn = FTM_SESSION_FOR_PEER(wlc_ftm_get_handle(wlc), peer, flag);

	if (sn && sn->ftm_state) {
		return sn->ftm_state->burst;
	} else {
		return NULL;
	}
}

static void
ftm_session_tsf_scan_complete(void *ctx, int wlc_status, wlc_bsscfg_t *bsscfg)
{
	pdftm_session_t *sn;
	wlc_bss_info_t *bi = NULL;
	uint i;
	pdftm_time_t l_tsf = 0, r_tsf = 0;
	uint32 l_tsf_l, l_tsf_h;
	uint32 r_tsf_l, r_tsf_h;
	uint64 cur_tsf, delta;
	wlc_info_t *wlc;
	pdftm_t *ftm;
	int err = BCME_OK;

	BCM_REFERENCE(bsscfg);
	sn = (pdftm_session_t *)ctx;
	ftm = sn->ftm;

	/* check if Session is still valid..else ignore scan_complete_cb */
	if (!FTM_VALID_SESSION(sn)) {
		err = WL_PROXD_E_EXPIRED;
		goto done;
	}

	switch (sn->tsf_scan_state) {
	case FTM_TSF_SCAN_STATE_ACTIVE_SCAN:
		sn->tsf_scan_state = FTM_TSF_SCAN_STATE_ACTIVE_SCAN_DONE;
		break;
	case FTM_TSF_SCAN_STATE_PASSIVE_SCAN:
		sn->tsf_scan_state = FTM_TSF_SCAN_STATE_PASSIVE_SCAN_DONE;
		break;
	default:
		/*
		 * if wlc_status is success,
		 * tsf scan state should be
		 * active scan or passive scan only
		 */
		ASSERT(wlc_status != WLC_E_STATUS_SUCCESS ||
			(sn->tsf_scan_state == FTM_TSF_SCAN_STATE_ACTIVE_SCAN ||
				sn->tsf_scan_state == FTM_TSF_SCAN_STATE_PASSIVE_SCAN));
		err = WL_PROXD_E_SCANFAIL;
		break;
	}

	if (wlc_status != WLC_E_STATUS_SUCCESS) {
		err = WL_PROXD_E_SCANFAIL;
		goto done;
	}

	if (err != BCME_OK) {
		goto done;
	}

	wlc = ftm->wlc;
	for (i = 0; i < wlc->scan_results->count; ++i) {
		wlc_bss_info_t *cur_bi = wlc->scan_results->ptrs[i];

		if (!cur_bi->bcn_prb) {
			continue;
		}

		if (!memcmp(&sn->config->burst_config->peer_mac,
				&cur_bi->BSSID, sizeof(bi->BSSID))) {
			bi = cur_bi;
			break;
		}
	}

	if (!bi) {
		err = WL_PROXD_E_NOTSF;
		goto done;
	}

	/* recover 64-bit local tsf for the timestamp  in bcn prb */

	/* note: use wlc/dev tsf and not ftm tsf (ref) that may use pmu timer
	 * so, we compute delta to timestamp in the device tsf space and use the
	 * the delta to adjust using the ftm tsf (ref) to that point
	 */
	ASSERT(ftm->wlc->clk);
	FTM_GET_DEV_TSF(ftm, cur_tsf);
	l_tsf = cur_tsf;
	l_tsf_l = l_tsf & 0xffffffff;
	l_tsf_h = l_tsf >> 32;
	if (l_tsf_l < bi->rx_tsf_l)
		--l_tsf_h;
	l_tsf_l = bi->rx_tsf_l;
	l_tsf = ((uint64)l_tsf_h << 32) | (uint64)l_tsf_l;

	delta = cur_tsf - l_tsf;
	FTM_GET_TSF(ftm, cur_tsf); /* note: tsf, could be pmu timer */
	l_tsf = cur_tsf - delta;

	r_tsf_l = ltoh32_ua(&bi->bcn_prb->timestamp[0]);
	r_tsf_h = ltoh32_ua(&bi->bcn_prb->timestamp[1]);
	r_tsf = ((uint64)r_tsf_h << 32) | (uint64)r_tsf_l;

	sn->tsf_off = r_tsf - l_tsf;
	sn->ftm_state->flags |= FTM_SESSION_STATE_USE_TSF_OFF;

	/* enable vht ack if brcm peer - subject to negotiated rate being vht */
	if (FTM_SESSION_IS_AUTO_VHTACK(sn) && (bi->flags & WLC_BSS_BRCM))
		sn->config->flags |= WL_PROXD_SESSION_FLAG_VHTACK;

done:
	(void)pdftm_wake_sched(ftm, sn);
	FTM_LOG_STATUS(ftm, err,
		(("wl%d: %s: wlc status %d err %d session idx %d "
		"scan state %d bi->rx_tsf_l %u tsf_off %u.%u "
		"l_tsf %u.%u r_tsf %u.%u\n",
		FTM_UNIT(ftm), __FUNCTION__, wlc_status, err, sn->idx,
		sn->tsf_scan_state, bi ? bi->rx_tsf_l : 0,
		FTM_LOG_TSF_ARG(sn->tsf_off),
		FTM_LOG_TSF_ARG(l_tsf), FTM_LOG_TSF_ARG(r_tsf))));
}

int pdftm_start_session_tsf_scan(pdftm_session_t *sn)
{
	int err = BCME_OK;
	const pdftm_session_config_t *sncfg;
	wlc_ssid_t ssid;
	pdftm_tsf_scan_state_t scan_st;
	int scan_type = DOT11_SCANTYPE_ACTIVE;
	int num_probes = FTM_SESSION_TSF_SCAN_NUM_PROBES;
	int active_time = 0;
	int passive_time = 0;
	chanspec_t scan_chanspec;
	pdftm_t *ftm;

	ASSERT(FTM_VALID_SESSION(sn));

	ftm = sn->ftm;
	scan_st = sn->tsf_scan_state;
	switch (scan_st) {
	case FTM_TSF_SCAN_STATE_NONE:
		scan_st = FTM_TSF_SCAN_STATE_ACTIVE_SCAN;
		active_time = FTM_SESSION_TSF_SCAN_ACTIVE_TIME_MS;
		break;
	case FTM_TSF_SCAN_STATE_ACTIVE_SCAN_DONE:
		scan_st = FTM_TSF_SCAN_STATE_PASSIVE_SCAN;
		scan_type = DOT11_SCANTYPE_PASSIVE;
		num_probes = 0;
		passive_time = FTM_SESSION_TSF_SCAN_PASSIVE_TIME_MS;
		break;

	case FTM_TSF_SCAN_STATE_ACTIVE_SCAN: /* fall through */
	case FTM_TSF_SCAN_STATE_PASSIVE_SCAN:
		goto done; /* okay, wait for scan complete */
		/* break; */

	case FTM_TSF_SCAN_STATE_PASSIVE_SCAN_DONE: /* fall through */
	default:
		err = WL_PROXD_E_SCANFAIL;
		break;
	}

	if (err != BCME_OK)
		goto done;

	if (ANY_SCAN_IN_PROGRESS(ftm->wlc->scan)) {
		err = BCME_BUSY;
		goto done;
	}
	/*
	*change chanspec to ctl chanspec. observed issues when scan is given
	*on 80 MHz on both core dual band capable RSDB chip
	*/
	scan_chanspec = wf_chspec_ctlchspec(sn->config->burst_config->chanspec);

	sncfg = sn->config;
	ssid.SSID_len = 0;
	err = wlc_scan_request_ex(ftm->wlc, DOT11_BSSTYPE_ANY,
		&sncfg->burst_config->peer_mac, 1, &ssid,
		scan_type, num_probes, active_time, passive_time,
		0 /* home time */,
		&scan_chanspec, 1 /* num chan */,
		0 /* chan start */, TRUE /* save prb rsp */,
		ftm_session_tsf_scan_complete, sn,
		WLC_ACTION_SCAN, WL_SCANFLAGS_SWTCHAN, sn->bsscfg,
		NULL /* act cb */, NULL /* act cb arg */);

	if (err != BCME_OK) {
		if (err == BCME_EPERM)
			err = BCME_BUSY;
		goto done;
	}

	sn->tsf_scan_state = scan_st;

done:
	FTM_LOG_STATUS(ftm, err,
		(("wl%d: %s: status %d, session idx %d scan_st %d\n",
		FTM_UNIT(ftm), __FUNCTION__, err, sn->idx, scan_st)));
	return err;
}

/*
* save rx lci or civic report (included in a FTM meas frame)
* for a session
* Input:
*     tlv_id: specify the tlv-id for LCI or CIVIC
*     lci_civic_rep: point to a lci or civic reported included
*                    in a rx-FTM-meas-frame
*     lci_civic_rep_len: size of 'lci_civic_rep' in bytes
*
*/
int
pdftm_session_set_lci_civic_rpt(pdftm_session_t *sn,
	uint16 tlv_id, uint8 *lci_civic_rep, uint16 lci_civic_rep_len)
{
	uint8		*buffer;
	int		err = BCME_OK;
	pdftm_t *ftm = sn->ftm;
	ASSERT(tlv_id == WL_PROXD_TLV_ID_LCI || tlv_id == WL_PROXD_TLV_ID_CIVIC);

	/* allocate a buffer to store lci or civic report if available */
	buffer = MALLOCZ(FTM_OSH(ftm), lci_civic_rep_len);
	if (!buffer) {
		err = BCME_NOMEM;
		goto done;
	}
	memcpy(buffer, lci_civic_rep, lci_civic_rep_len);

	if (tlv_id == WL_PROXD_TLV_ID_LCI) {
		/* free the previous one if already exists */
		if (sn->lci_rep)
			MFREE(FTM_OSH(ftm), sn->lci_rep, sn->lci_rep_len);

		/* save the current one */
		sn->lci_rep = buffer;
		sn->lci_rep_len = lci_civic_rep_len;
	}
	else { /* tlv_id == WL_PROXD_TLV_ID_CIVIC) */
		 /* free the previous one if already exists */
		 if (sn->civic_rep)
			MFREE(FTM_OSH(ftm), sn->civic_rep, sn->civic_rep_len);

		/* save the current one */
		sn->civic_rep = buffer;
		sn->civic_rep_len = lci_civic_rep_len;
	}

done:
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d, lci/civic-report-len=%d\n",
		FTM_UNIT(ftm), __FUNCTION__, err, lci_civic_rep_len)));

	return err;
}

int
wlc_ftm_stop_session(wlc_ftm_t *ftm, wl_proxd_session_id_t sid, int status)
{
	pdftm_session_t *sn = NULL;
	int err = BCME_OK;

	ASSERT(FTM_VALID(ftm));
	sn = FTM_SESSION_FOR_SID(ftm, sid);
	if (!sn) {
		err = BCME_NOTFOUND;
		goto done;
	}

	err = pdftm_stop_session(sn, status);
done:
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d for session %d\n",
		FTM_UNIT(ftm), __FUNCTION__, err, sid)));
	return err;
}

int
wlc_ftm_delete_session(wlc_ftm_t *ftm, wl_proxd_session_id_t sid)
{
	pdftm_session_t *sn = NULL;
	int err = BCME_OK;

	ASSERT(FTM_VALID(ftm));
	sn = FTM_SESSION_FOR_SID(ftm, sid);
	if (!sn) {
		err = BCME_NOTFOUND;
		goto done;
	}

	sn->flags |= FTM_SESSION_DELETE_ON_STOP;
	if (sn->state != WL_PROXD_SESSION_STATE_ENDED &&
		sn->state != WL_PROXD_SESSION_STATE_DESTROYING) {
		err = pdftm_stop_session(sn, WL_PROXD_E_CANCELED);
		if (err != BCME_OK)
			goto done;
	}

	/* destroy burst if still valid */
	if (sn->ftm_state && sn->ftm_state->burst) {
		pdburst_destroy(&sn->ftm_state->burst);
	}
	err = pdftm_sched_process_session_end(ftm, sn->idx, sn->idx);

done:
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d for session %d\n",
		FTM_UNIT(ftm), __FUNCTION__, err, sid)));
	return err;
}

static int
ftm_session_get_wait_reason(pdftm_session_t *sn)
{
	int ret = WL_PROXD_WAIT_NONE;

	if (sn->state != WL_PROXD_SESSION_STATE_START_WAIT)
		return ret;

	ret = (sn->flags & FTM_SESSION_KEY_WAIT) ? WL_PROXD_WAIT_KEY : 0;
	ret |= ((sn->flags & FTM_SESSION_EXT_SCHED_WAIT) ? WL_PROXD_WAIT_SCHED : 0);
	ret |= ((sn->flags & FTM_SESSION_TSF_WAIT)? WL_PROXD_WAIT_TSF : 0);

	return ret;
}

int
wlc_ftm_get_session_info(wlc_ftm_t *ftm, wl_proxd_session_id_t sid,
	wl_proxd_ftm_session_info_t *sip)
{
	pdftm_session_t *sn = NULL;
	int err = BCME_OK;

	ASSERT(FTM_VALID(ftm));
	sn = FTM_SESSION_FOR_SID(ftm, sid);
	if (!sip || !sn) {
		err = BCME_NOTFOUND;
		goto done;
	}

	sip->sid = sid;
	sip->bss_index = WLC_BSSCFG_IDX(sn->bsscfg);
	sip->pad = 0;
	sip->bssid = sn->bsscfg->BSSID;
	sip->state = sn->state;
	sip->status = sn->status;
	sip->burst_num = sn->ftm_state ? sn->ftm_state->burst_num : 0;
	sip->wait_reason = ftm_session_get_wait_reason(sn);

	if (sn->ftm_state) {
		pdftm_time_t meas_start;
		meas_start = pdftm_convert_tsf(sn, sn->ftm_state->meas_start, TRUE);
		sip->meas_start_lo = (uint32)(meas_start & 0xffffffffULL);
		sip->meas_start_hi = (uint32)(meas_start >> 32);
	} else {
		sip->meas_start_hi = sip->meas_start_lo = 0;
	}

done:
	return err;
}

int
wlc_ftm_get_session_tsf(wlc_ftm_t *ftm, wl_proxd_session_id_t sid, uint64 *tsf)
{
	pdftm_session_t *sn = NULL;
	int err = BCME_OK;

	ASSERT(FTM_VALID(ftm));
	sn = FTM_SESSION_FOR_SID(ftm, sid);
	if (!sn) {
		err = BCME_NOTFOUND;
		goto done;
	}

	err = pdftm_get_session_tsf(sn, tsf);

done:
	return err;
}

/*
 * Stop FTM sessions with bsscfg
 * Input:
 * ftm: FTM handle
 * cfg: bsscfg used to match the session.
 * Return BCME_OK
 */
void
wlc_ftm_stop_sess_inprog(wlc_ftm_t *ftm, wlc_bsscfg_t *cfg)
{
	int i;
	uint8 bss_index;
	pdftm_cmn_t *ftm_cmn = ftm->ftm_cmn;
	ASSERT(FTM_VALID(ftm));
	ASSERT(cfg != NULL);
	bss_index = WLC_BSSCFG_IDX(cfg);
	/*
	 * Loop through all sessions
	 */
	for (i = 0; i < ftm_cmn->max_sessions; i++) {
		pdftm_session_t *sn = ftm_cmn->sessions[i];
		/*
		 * If session is real
		 */
		if (sn) {
			/*
			 * Match bsscfg
			 */
			if (WLC_BSSCFG_IDX(sn->bsscfg) == bss_index) {
				/*
				 * Check state and stop it if started and not ended
				 */
				if ((sn->state > WL_PROXD_SESSION_STATE_CONFIGURED) &&
						(sn->state <= WL_PROXD_SESSION_STATE_BURST)) {
					wlc_ftm_stop_session(ftm, sn->sid, WL_PROXD_E_CANCELED);
				}
			}
		}
	}
	return;
}

int
pdftm_session_trig_tsf_update(pdftm_session_t *sn,
    const dot11_ftm_sync_info_t *tsf_si)
{
	int err = BCME_OK;
	pdftm_session_state_t *sst = NULL;
	pdftm_time_t sn_tsf = 0ULL, r_tsf = 0ULL;
	uint32 r_tsf_lo, r_tsf_hi;
	pdftm_time_t r_tsf_local = 0ULL;
	int32 delta = 0;

	ASSERT(FTM_VALID_SESSION(sn));
	ASSERT(FTM_VALID(sn->ftm));

	if (!tsf_si ||
		FTM_SESSION_IS_TARGET(sn))
		goto done;

	if (!FTM_SESSION_NEED_TSF_SYNC(sn))
		goto done;

	sst = sn->ftm_state;
	sst->flags &= ~FTM_SESSION_STATE_TSF_SYNC;

	FTM_LOGSN(sn->ftm, (("wl%d: %s: ie %d.%d.%d.%02x%02x%02x%02x\n",
		FTM_UNIT(sn->ftm), __FUNCTION__,
		tsf_si->id, tsf_si->len, tsf_si->id_ext,
		tsf_si->tsf_sync_info[0], tsf_si->tsf_sync_info[1],
		tsf_si->tsf_sync_info[2], tsf_si->tsf_sync_info[3])));

	/* compute tsf correspondng to tsf sync info */
	err = pdftm_get_session_tsf(sn, &sn_tsf);
	if (err != BCME_OK)
		goto done;

	r_tsf_lo = ltoh32_ua(&tsf_si->tsf_sync_info[0]);
	r_tsf_hi = (uint32)(sn_tsf >> 32);
	r_tsf = (uint64)r_tsf_hi << 32 | (uint64)r_tsf_lo;
	if (r_tsf < sn_tsf)
		r_tsf +=  (uint64)1 << 32;

	r_tsf_local = pdftm_convert_tsf(sn, r_tsf, FALSE);
	delta = (int32)(r_tsf_local - sst->trig_tsf);

	/* set delta adjustment for next burst */
	sst->trig_delta = delta;

done:
	FTM_LOG_STATUS(sn->ftm, err, (("wl%d: %s: status %d for session sid %d "
		"r_tsf %u.%u r_tsf_local %u.%u delta %d\n",
		FTM_UNIT(sn->ftm), __FUNCTION__, err, sn ? sn->sid : 0,
		FTM_LOG_TSF_ARG(r_tsf), FTM_LOG_TSF_ARG(r_tsf_local), delta)));

	return err;
}

int
wlc_ftm_start_session(wlc_ftm_t *ftm, wl_proxd_session_id_t sid)
{
	int err = BCME_OK;
	pdftm_session_t *sn;
	sn = FTM_SESSION_FOR_SID(ftm, sid);
	if (!sn) {
		err = WL_PROXD_E_INVALID_SESSION;
		goto done;
	}
	err = pdftm_start_session(sn);
	if (err != BCME_OK) {
		(void) pdftm_stop_session(sn, err);
		goto done;
	}
done:
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: start session for sid %d status %d\n",
			FTM_UNIT(ftm), __FUNCTION__, sid, err)));
	return err;
}

int
wlc_ftm_get_session_result(wlc_ftm_t *ftm, wl_proxd_session_id_t sid,
	wl_proxd_rtt_result_t *res, int num_rtt)
{
	int err = BCME_OK;
	pdftm_session_t *sn;
	sn = FTM_SESSION_FOR_SID(ftm, sid);
	if (!sn) {
		err = WL_PROXD_E_INVALID_SESSION;
		goto done;
	}
	err = pdftm_get_session_result(ftm, sn, res, num_rtt);
	if (err != BCME_OK) {
		goto done;
	}
done:
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: result for sid %d status %d\n",
			FTM_UNIT(ftm), __FUNCTION__, sid, err)));
	return err;
}

int
wlc_ftm_get_session_params(wlc_ftm_t *ftm, wl_proxd_session_id_t sid,
	dot11_ftm_params_t *ftm_params)
{
	int err = BCME_OK;
	pdftm_session_t *sn;
	sn = FTM_SESSION_FOR_SID(ftm, sid);
	if (!sn) {
		err = WL_PROXD_E_INVALID_SESSION;
		goto done;
	}
	err = ftm_proto_get_ftm1_params(ftm, sn, BCME_NOTREADY, ftm_params);
	if (err != BCME_OK) {
		goto done;
	}
done:
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: get params for sid %d status %d\n",
			FTM_UNIT(ftm), __FUNCTION__, sid, err)));
	return err;

}

/*
* This Pub API is used to update/setup BSSCFG that is
* to be used for sending out FTM events for the given session sid.
* Example: NAN wants to send out events on primary or
* NDI based on host configurartion
*/

int
wlc_ftm_set_session_event_bsscfg(wlc_ftm_t * ftm, wl_proxd_session_id_t sid,
	wlc_bsscfg_t * event_cfg)
{
	int err = BCME_OK;
	pdftm_session_t *sn;
	sn = FTM_SESSION_FOR_SID(ftm, sid);
	if (!sn) {
		err = WL_PROXD_E_INVALID_SESSION;
		goto done;
	}
	sn->event_bsscfg = event_cfg;
done:
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: set event bsscfg for sid %d status %d\n",
			FTM_UNIT(ftm), __FUNCTION__, sid, err)));
	return err;

}
