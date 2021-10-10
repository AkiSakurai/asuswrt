/*
 * Proxd FTM method scheduler. See twiki FineTimingMeasurement.
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
 * $Id: pdftmsched.c 779785 2019-10-07 16:45:33Z $
 */

#include "pdftmpvt.h"

int
pdftm_sched_validate_session(pdftm_t *ftm, pdftm_session_t *sn)
{
	int err = BCME_OK;

	wlc_bsscfg_t *bsscfg;
	wl_proxd_session_flags_t flags;

	bsscfg = sn->bsscfg;
	ASSERT(bsscfg != NULL);
	BCM_REFERENCE(bsscfg);

	flags = sn->config->flags;

	/* turn WL_PROXD_E_EXT_SCHED on initiator for any non-infra BSS type */
	if (bsscfg->type != BSSCFG_TYPE_SLOTTED_BSS) {
		goto done;
	} else {
		/* turn off tx auto burst option for ext sched sessions */
		flags &= ~WL_PROXD_SESSION_FLAG_TX_AUTO_BURST;
		err = WL_PROXD_E_EXT_SCHED;
		goto done;
	}
done:
	return err;
}

/* check if burst for session 1 overlaps with burst 'burst_num' of session 2 */
static int
ftm_sched_check_overlap(pdftm_t *ftm, const pdftm_session_t *sn1,
	const pdftm_session_t *sn2, int burst_num, uint64 *start1)
{
	const pdftm_session_state_t *sst1;
	const pdftm_session_state_t *sst2;
	uint64 start2, end2;
	uint64 end1;
	bool overlap;

	sst1 = sn1->ftm_state;
	*start1 = sst1->burst_start;
	end1 = (*start1) + FTM_INTVL2USEC(&sn1->config->burst_config->duration);

	sst2 = sn2->ftm_state;
	start2 = sst2->burst_start + (burst_num * FTM_INTVL2USEC(&sn2->config->burst_period));
	end2 = start2 + FTM_INTVL2USEC(&sn2->config->burst_config->duration);

	overlap = FTM_SCHED_OVERLAP(*start1, end1, start2, end2);
	if (overlap)
		*start1 = end2;

	return overlap ? WL_PROXD_E_SCHED_FAIL : BCME_OK;
}

static int
ftm_sched_compute_burst_start(pdftm_t *ftm, pdftm_session_t *sn)
{
	int err = BCME_OK;
	pdftm_session_state_t *sst;
	int i;
	pdftm_time_t burst_start = 0;
	pdftm_cmn_t *ftm_cmn = ftm->ftm_cmn;
	uint64 min_delay_us;
#ifdef WL_FTM_MSCH
	uint64 msch_exp = 0, now = 0;
#endif /* WL_FTM_MSCH */
	ASSERT(ftm_cmn->sched->burst_sn == NULL || ftm_cmn->sched->in.burst != 0);
	ASSERT(!ftm_cmn->sched->in.burst || ftm_cmn->sched->burst_sn != NULL);

	/* no support for computation for other states e.g sched wait */
	ASSERT(sn->state == WL_PROXD_SESSION_STATE_DELAY);

	sst = sn->ftm_state;

	/* burst starts after delay allowing some time for initial exchange */
	/* add delay for potential initial FTM1 retries (non-asap target) */
	if (FTM_SESSION_IS_TARGET(sn) && (!(sn->flags & FTM_SESSION_FTM1_SENT)) &&
		!FTM_SESSION_IS_ASAP(sn)) {
		min_delay_us = 3*MAX(FTM_INTVL2USEC(&sn->config->burst_config->ftm_sep), 3000)+500;
		sst->delay_exp += min_delay_us;
		FTM_LOGSCHED(ftm, (("wl%d: %s: status %d session idx %d "
			"delay_exp %u.%u was increased by min_delay_us %u.%u, \n",
			FTM_UNIT(ftm), __FUNCTION__, err, sn->idx,
			FTM_LOG_TSF_ARG(sst->delay_exp), FTM_LOG_TSF_ARG(min_delay_us))));
	}
	BCM_REFERENCE(min_delay_us);
#ifdef WL_FTM_MSCH
	if (!BSSCFG_SLOTTED_BSS(sn->bsscfg)) {
		now = msch_current_time(ftm->wlc->msch_info);
		msch_exp = wlc_msch_query_timeslot(ftm->wlc->msch_info, 0, 0);
		if (sst->delay_exp > msch_exp) {
			burst_start = sst->delay_exp;
		} else {
			burst_start = sst->delay_exp + (msch_exp - now);
		}
		/* Try to wake up early on target side if possible */
		if (FTM_SESSION_IS_TARGET(sn)) {
			if (burst_start > msch_exp + FTM_SCHED_DELAY_EXP_ADJ_US) {
				burst_start = burst_start - FTM_SCHED_DELAY_EXP_ADJ_US;
			}
		}
	}
	else {
		burst_start = sst->delay_exp;
	}
#else
	burst_start = sst->delay_exp;
#endif /* WL_FTM_MSCH */
	sst->burst_start = burst_start;
	sst->burst_end = sst->burst_start +
		FTM_INTVL2USEC(&sn->config->burst_config->duration);

	if (!(ftm_cmn->sched->in.burst + ftm_cmn->sched->in.sched_wait
		+ ftm_cmn->sched->in.delay)) {
		goto done;
	}

	/* check that burst does not overlap with next set of burst(s),
	 * and reschedule if necessary.
	 */
	for (i = 0; i < ftm_cmn->max_sessions; ++i) {
		uint64 start;
		pdftm_session_t *sn2 = ftm_cmn->sessions[i];

		if (!FTM_VALID_SESSION(sn2) || (sn == sn2))
			continue;

		if (sn2->state != WL_PROXD_SESSION_STATE_DELAY &&
			sn2->state != WL_PROXD_SESSION_STATE_SCHED_WAIT &&
			sn2->state != WL_PROXD_SESSION_STATE_BURST) {
			continue;
		}

		/* move session start forward if necessary */
		err = ftm_sched_check_overlap(ftm, sn, sn2, 0, &start);
		if (err != BCME_OK) {
			ASSERT(err == WL_PROXD_E_SCHED_FAIL);
			sst->burst_start = start;
			sst->burst_end = start +
				FTM_INTVL2USEC(&sn->config->burst_config->duration);
		}
	}

	/* check for overlap for next period; do not reschedule */
	err = BCME_OK;
	for (i = 0; i < ftm_cmn->max_sessions; ++i) {
		uint64 start;
		pdftm_session_t *sn2 = ftm_cmn->sessions[i];
		int next_err;

		if (!FTM_VALID_SESSION(sn2) || (sn == sn2))
			continue;

		if (sn2->state != WL_PROXD_SESSION_STATE_DELAY &&
			sn2->state != WL_PROXD_SESSION_STATE_SCHED_WAIT &&
			sn2->state != WL_PROXD_SESSION_STATE_BURST) {
			continue;
		}

		/* if this is the last burst, no need to check next period */
		if (sn2->ftm_state->burst_num >= sn2->config->num_burst)
			continue;

		next_err = ftm_sched_check_overlap(ftm, sn, sn2, 1, &start);
		if (next_err != BCME_OK) {
			sst->burst_start = start;
			sst->burst_end = start +
				FTM_INTVL2USEC(&sn->config->burst_config->duration);
			err = next_err;
		}
	}

done:
	sst->burst_exp = sst->burst_start +
		FTM_INTVL2USEC(&sn->config->burst_config->timeout);

	FTM_LOGSCHED(ftm, (("wl%d: %s: status %d session idx %d "
		"start %u.%u end %u.%u\n", FTM_UNIT(ftm), __FUNCTION__, err, sn->idx,
		FTM_LOG_TSF_ARG(sst->burst_start), FTM_LOG_TSF_ARG(sst->burst_end))));
	return err;
}

static int
ftm_sched_get_min_exp(pdftm_t *ftm, wl_proxd_session_state_t state,
	uint32 *out_exp_ms, pdftm_session_t **out_sn)
{
	int err;
	int i;
	pdftm_session_t *sn;
	uint32 min_exp;
	pdftm_expiration_t cur_tsf;
	pdftm_cmn_t *ftm_cmn = ftm->ftm_cmn;
	*out_sn = NULL;

	min_exp = ~0;
	FTM_GET_TSF(ftm, cur_tsf);

	FTM_LOGSCHED(ftm, (("wl%d: %s: cur tsf %u.%u\n",
		FTM_UNIT(ftm), __FUNCTION__, FTM_LOG_TSF_ARG(cur_tsf))));

	for (i = 0; i < ftm_cmn->max_sessions; ++i) {
		uint64 exp;
		uint32 exp_off;
		sn  = ftm_cmn->sessions[i];
		if (!FTM_VALID_SESSION(sn))
			continue;

		if (state == WL_PROXD_SESSION_STATE_DELAY) {
			if (sn->state != WL_PROXD_SESSION_STATE_DELAY)
				continue;
			exp = sn->ftm_state->delay_exp;
			/* to allow for millisecond delay for timer resolution
			 * wake up a bit earlier on the target, but a bit
			 * later on the initiator. So triggers would not be sent
			 * early, but wake a bit early not to miss triggers.
			 */
			if (FTM_SESSION_IS_INITIATOR(sn)) {
				exp += FTM_SCHED_DELAY_EXP_ADJ_US;
			} else {
				exp -= FTM_SCHED_DELAY_EXP_ADJ_US;
			}
		} else if (state == WL_PROXD_SESSION_STATE_SCHED_WAIT) {
			if (sn->state != WL_PROXD_SESSION_STATE_SCHED_WAIT)
				continue;

			if (!FTM_BURST_ALLOWED(sn->flags))
				continue;

			exp = sn->ftm_state->burst_start;
		} else {
			ASSERT(FALSE);
			continue;
		}

		FTM_LOGSCHED(ftm, (("wl%d: %s: session idx %d exp tsf %u.%u\n",
			FTM_UNIT(ftm), __FUNCTION__, sn->idx, FTM_LOG_TSF_ARG(exp))));

		if (exp <= cur_tsf) { /* already expired */
			*out_sn = sn;
			min_exp = 0;
			break;
		}

		exp_off = (uint32)(exp - cur_tsf);
		if (FTM_SESSION_IS_INITIATOR(sn)) {
			exp_off = ROUNDUP(exp_off, 1000);
		}
		exp = FTM_MICRO2MILLI(exp_off);
		if (exp >= (uint64)min_exp)
			continue;
		min_exp = (uint32)exp;
		*out_sn = sn;
	}

	if (out_exp_ms)
		*out_exp_ms = min_exp;

	if (*out_sn)
		err = BCME_OK;
	else
		err = BCME_NOTFOUND;

	FTM_LOGSCHED(ftm, (("wl%d: %s: status %d minimum delay %dms for session with idx %d\n",
		FTM_UNIT(ftm), __FUNCTION__, err, min_exp, (*out_sn) ? (*out_sn)->idx : -1)));
	return err;
}
#ifdef WLRSDB
/**
*For stanalone RTT update the WLC and BSSCFG to be used for the session .
*For AWDL-proxd and NAN-proxd, assuming RTT will run on the corresponsding WLC and BSSCFG
*/
static void
ftm_sched_set_session_ftm(pdftm_session_t *sn)
{
	pdftm_t *ftm = sn->ftm;
	pdftm_cmn_t *ftm_cmn = ftm->ftm_cmn;
	wlc_info_t *from_wlc = ftm->wlc;
	wlc_info_t *to_wlc = NULL;
	uint bit_pos;
	to_wlc = wlc_rsdb_find_wlc_for_chanspec(ftm->wlc, NULL,
		sn->config->burst_config->chanspec, NULL, 0);
	if (to_wlc != from_wlc) {
		if (wlc_bsscfg_primary(from_wlc) != sn->bsscfg) {
			OSL_SYS_HALT();
		}
		sn->ftm = ftm_cmn->ftm[to_wlc->pub->unit];
		/*
		*XXX: Hack:For standalone RTT bsscfg will be set to one of the corresponding
		*WLC bsscfgs on which session is configured.
		*When WLC is changed we have to change sn->bsscfg also.
		*For now using primary bsscfg of new WLC
		*/
		sn->bsscfg = wlc_bsscfg_primary(sn->ftm->wlc);
		bit_pos = FTM_BSSCFG_OPTION_BIT(sn->bsscfg, FTM_BSSCFG_OPTION_TX);
		setbit(ftm_cmn->enabled_bss, bit_pos);
		bit_pos = FTM_BSSCFG_OPTION_BIT(sn->bsscfg, FTM_BSSCFG_OPTION_RX);
		setbit(ftm_cmn->enabled_bss, bit_pos);
	}
}
#endif /* WLRSDB */

static void
ftm_sched_add_timer(pdftm_sched_t *sched, uint32 min_delay_ms)
{
	wl_add_timer(sched->timer_wl, sched->timer, min_delay_ms, FALSE);
}
static void
ftm_sched_del_timer(pdftm_sched_t *sched)
{
	wl_del_timer(sched->timer_wl, sched->timer);
}
void
pdftm_sched(void *arg)
{
	pdftm_t *ftm = (pdftm_t *)arg;
	pdftm_sched_t *sched;
	pdftm_session_t *sn = NULL;
	uint32 min_delay_ms;
	int err = BCME_OK;
	int i;
	wl_proxd_session_state_t new_state;
	bool retry_scan = FALSE;
	pdftm_cmn_t *ftm_cmn = ftm->ftm_cmn;
	ASSERT(FTM_VALID(ftm));
	ASSERT(ftm_cmn->flags & FTM_FLAG_SCHED_INITED);
	ASSERT(!(ftm_cmn->flags & FTM_FLAG_SCHED_ACTIVE));

	ftm_cmn->flags |= FTM_FLAG_SCHED_ACTIVE;
	FTM_LOG(ftm, (("wl%d: %s: active\n", FTM_UNIT(ftm), __FUNCTION__)));

	sched = ftm_cmn->sched;
	BCM_REFERENCE(sched);

	ASSERT(sched->timer);

	/* process start and stop - former moves to delay and latter moves to end */
	for (i = 0; i < ftm_cmn->max_sessions; ++i) {
		sn = ftm_cmn->sessions[i];

		if (!FTM_VALID_SESSION(sn))
			continue;

		new_state = sn->state;
		switch (sn->state) {
		case WL_PROXD_SESSION_STATE_STOPPING:
			new_state = WL_PROXD_SESSION_STATE_ENDED;
			break;

		case WL_PROXD_SESSION_STATE_STARTED:
#ifdef WLRSDB
			if (RSDB_ENAB(ftm->wlc->pub)) {
				ftm_sched_set_session_ftm(sn);
			}
#endif /* WLRSDB */

#ifdef WLTDLS
			if (BSSCFG_AS_STA(sn->bsscfg)&& BSSCFG_INFRA_STA(sn->bsscfg) &&
				TDLS_ENAB(ftm->wlc->pub) && wlc_tdls_is_active(sn->ftm->wlc)) {
				/* Block FTM if TDLS active on Home channel and FTM is OF_CHAN */
				if (sn->config->burst_config->chanspec !=
					sn->bsscfg->current_bss->chanspec) {
					sn->status = WL_PROXD_E_POLICY;
					new_state = WL_PROXD_SESSION_STATE_STOPPING;
					break;
				}
			}
#endif /* WLTDLS */

			new_state = WL_PROXD_SESSION_STATE_DELAY;

			/* initiate a scan for time ref if necessary - since ASAP
			 * can be overridden by target do this scan now if we don't
			 * have a session tsf
			 */
			if (!FTM_SESSION_IS_INITIATOR(sn))
				break;

			/* XXX need more permanent fix/analysis. If
			 * a session is in burst, defer scan on initiator
			 * FIXME
			 */
			if (sched->in.burst) {
				new_state = WL_PROXD_SESSION_STATE_STARTED;
				break;
			}

			err = pdftm_get_session_tsf(sn, NULL);
			if (err == BCME_OK)
				break;

			/* keep in started state until tsf resolves
			* okay to fail for asap.
			* for asap=1, if pre-scan flag is disabled, do not scan.
			*/
			if (!FTM_SESSION_IS_ASAP(sn) ||
				FTM_SESSION_PRE_SCAN_ENABLED(sn->config->flags)) {
				new_state = WL_PROXD_SESSION_STATE_STARTED;
				err = pdftm_start_session_tsf_scan(sn);
				if (err != BCME_OK) {
					if (FTM_SESSION_IS_ASAP(sn) &&
						(err == WL_PROXD_E_SCANFAIL)) {
						err = BCME_OK;
						new_state = WL_PROXD_SESSION_STATE_DELAY;
					}
					else if (err != BCME_BUSY ||
							FTM_SESSION_SCAN_RETRIES_DONE
							(sn->scan_retry_attempt)) {
						sn->status = err;
						new_state = WL_PROXD_SESSION_STATE_STOPPING;
					}
					else {
						retry_scan = TRUE;
						sn->scan_retry_attempt++;
					}
				} else {
				}
			}

			break;
		}

		if (sn->state != new_state) {
			(void)pdftm_change_session_state(sn, sn->status, new_state);
		}

		/* process stopping state so the session is reclaimed */
		if (sn->state == WL_PROXD_SESSION_STATE_STOPPING) {
			(void)pdftm_change_session_state(sn, sn->status,
				WL_PROXD_SESSION_STATE_ENDED);
		}
	}

	/* process end - reclaim */
	pdftm_sched_process_session_end(ftm, 0, ftm_cmn->max_sessions - 1);

	/* if a burst is in progress, scheduler runs via completion callback */
	if (sched->in.burst)
		goto done;

	ASSERT(sched->burst_sn == NULL);

	/* if a session is in delay and could be moved to sched wait, move it */
	min_delay_ms = ~0;
	if (sched->in.delay) {
		for (;;) {
			err = ftm_sched_get_min_exp(ftm, WL_PROXD_SESSION_STATE_DELAY,
				&min_delay_ms, &sn);
			ASSERT(err == BCME_OK || err == BCME_NOTFOUND);
			if ((err != BCME_OK) || (min_delay_ms != 0))
				break;

			new_state = WL_PROXD_SESSION_STATE_SCHED_WAIT;
			if (!(sn->config->flags & WL_PROXD_SESSION_FLAG_ASAP) &&
				FTM_SESSION_IS_INITIATOR(sn)) {
				err = pdftm_get_session_tsf(sn, NULL);
				if (err != BCME_OK)
					new_state = WL_PROXD_SESSION_STATE_STOPPING;
			}

			/* note: after this point re-run the scheduler to reclaim session
			 * that may have stopped.
			 */
			err = pdftm_change_session_state(sn, err, new_state);
			if (err != BCME_OK) {
				(void)pdftm_change_session_state(sn, BCME_OK,
					WL_PROXD_SESSION_STATE_STOPPING);
				break;
			}

			if (new_state != WL_PROXD_SESSION_STATE_SCHED_WAIT)
				break;
		}
	}
	/* select the session w/ earliest burst and start burst */
	if (sched->in.sched_wait) {
		do {
			err = ftm_sched_get_min_exp(ftm,
				WL_PROXD_SESSION_STATE_SCHED_WAIT, NULL, &sn);
			if (sn != NULL) {
				if (sn->flags & FTM_SESSION_ON_CHAN) {
					err = pdftm_change_session_state(sn, BCME_OK,
						WL_PROXD_SESSION_STATE_BURST);
					if (err == BCME_OK) {
						ASSERT(sn->state == WL_PROXD_SESSION_STATE_BURST);
						goto done;
					} else if (err == WL_PROXD_E_DEFERRED) {
						err = pdftm_change_session_state(sn, BCME_OK,
							WL_PROXD_SESSION_STATE_DELAY);
					} else {
						(void) pdftm_change_session_state(sn, err,
							WL_PROXD_SESSION_STATE_STOPPING);
					}
				}
			}
		} while (sn != NULL);
	}
	if (retry_scan)
		min_delay_ms = MIN(min_delay_ms, FTM_SESSION_TSF_SCAN_RETRY_MS);

	FTM_LOGSCHED(ftm, (("wl%d: %s: rescheduling with delay %ums for session idx %d\n",
		FTM_UNIT(ftm), __FUNCTION__, min_delay_ms, sn ? sn->idx : -1)));
	ftm_sched_add_timer(sched, min_delay_ms);

done:
	ftm->ftm_cmn->flags &= ~FTM_FLAG_SCHED_ACTIVE;
	FTM_LOG(ftm, (("wl%d: %s: inactive\n", FTM_UNIT(ftm), __FUNCTION__)));
}

/* external interface */
int
pdftm_init_sched(pdftm_t *ftm)
{
	int err = BCME_OK;
	pdftm_cmn_t *ftm_cmn = ftm->ftm_cmn;
	if  (ftm_cmn->flags & FTM_FLAG_SCHED_ACTIVE)
		goto done;

	if (!ftm_cmn->sched->timer) {
		ftm_cmn->sched->timer = wl_init_timer(FTM_WL(ftm), pdftm_sched,
				ftm, FTM_SCHED_NAME);
		if (!ftm_cmn->sched->timer) {
			err = BCME_NORESOURCE;
			goto done;
		}
	}

	ftm->ftm_cmn->flags |= FTM_FLAG_SCHED_INITED;
done:
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d\n", FTM_UNIT(ftm), __FUNCTION__, err)));
	return err;
}

int
pdftm_wake_sched(pdftm_t *ftm, pdftm_session_t *sn)
{
	int err = BCME_OK;
	pdftm_cmn_t *ftm_cmn = ftm->ftm_cmn;

	BCM_REFERENCE(sn);
	if (ftm->ftm_cmn->flags & FTM_FLAG_SCHED_ACTIVE)
		goto done;

	ftm_sched_del_timer(ftm_cmn->sched);
	ftm_sched_add_timer(ftm_cmn->sched, 0);

done:
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d for session idx %d\n",
		FTM_UNIT(ftm), __FUNCTION__, err, sn ? sn->idx : -1)));
	return err;
}

int
pdftm_end_sched(pdftm_t *ftm)
{
	int err = BCME_OK;
	pdftm_sched_t *sched;
	pdftm_cmn_t *ftm_cmn = ftm->ftm_cmn;
	if (!(ftm_cmn->flags & ftm_cmn->flags & FTM_FLAG_SCHED_INITED))
		goto done;
	sched = ftm_cmn->sched;

	ASSERT(!(ftm_cmn->flags & FTM_FLAG_SCHED_ACTIVE));
	wl_del_timer(FTM_WL(ftm), sched->timer);
	wl_free_timer(FTM_WL(ftm), sched->timer);

	ftm_cmn->flags &= ~FTM_FLAG_SCHED_INITED;
	ftm_cmn->sched->timer = NULL;
#ifdef WL_FTM_MSCH
	err = pdftm_msch_end(ftm, NULL);
#endif // endif

done:
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d\n", FTM_UNIT(ftm), __FUNCTION__, err)));
	return err;
}

int
pdftm_setup_burst(pdftm_t *ftm, pdftm_session_t *sn,
	const uint8 *req, uint req_len, const wlc_d11rxhdr_t *req_wrxh, ratespec_t req_rspec)
{
	int err = BCME_OK;
	pdburst_t *burst;
	pdburst_params_t burst_params;
	pdftm_session_state_t *sst;

	ASSERT(sn->state == WL_PROXD_SESSION_STATE_SCHED_WAIT ||
		sn->state == WL_PROXD_SESSION_STATE_DELAY ||
		sn->state == WL_PROXD_SESSION_STATE_BURST ||
		sn->state == WL_PROXD_SESSION_STATE_START_WAIT ||
		sn->state == WL_PROXD_SESSION_STATE_STARTED);

	/* create (if needed) , and init burst. on success update session state */

	sst = sn->ftm_state;
	if (sst->burst == NULL) {
		burst = pdburst_create(ftm->wlc, sn, &pdftm_burst_callbacks);
		if (!burst) {
			err = BCME_NORESOURCE;
			goto done;
		}
		/* pretend previous burst was done */
		sst->flags |= FTM_SESSION_STATE_BURST_DONE;
	} else {
		burst = sst->burst;
	}

	/* if this burst has not completed - i.e. restarting or rescheduling
	 * record this as the same burst
	 */
	if (sst->flags & FTM_SESSION_STATE_BURST_DONE) {
		sst->flags &= ~FTM_SESSION_STATE_BURST_DONE;
		++(sst->burst_num);
	}

	err = pdftm_get_burst_params(sn, req, req_len, req_wrxh, req_rspec, &burst_params);
	if (err != BCME_OK)
		goto done;

	err = pdburst_init(burst, &burst_params);
	if (err != BCME_OK)
		goto done;

	sst->burst = burst;
	sst->rtt_status = WL_PROXD_E_NOTSTARTED;
	sst->result_flags = 0;
	bzero(&sst->avg_rtt, sizeof(sst->avg_rtt));
	sst->avg_dist = 0;
	sst->num_rtt = 0;	/* note: max_rtt must not change */
	if (sst->rtt)
		bzero(sst->rtt, sst->max_rtt * sizeof(*sst->rtt));

done:
	FTM_LOG_STATUS(ftm, err,
		(("wl%d: %s: status %d burst_num %d num_burst %d\n",
		FTM_UNIT(ftm), __FUNCTION__, err, sst->burst_num, sn->config->num_burst)));
	if ((err != BCME_OK) && (sst->burst_num >= sn->config->num_burst))  {
		if (burst != NULL)
			pdburst_destroy(&burst);
		sst->burst = NULL;
		ASSERT(burst == NULL);

		(void)pdftm_change_session_state(sn, err, WL_PROXD_SESSION_STATE_STOPPING);
		FTM_CNT(ftm->ftm_cmn, sn, sched_fail);
	}

	return err;
}

int
pdftm_sched_session(pdftm_t *ftm, pdftm_session_t *sn)
{
	int err = BCME_OK;
	wlc_bsscfg_t *bsscfg = sn->bsscfg;

	ASSERT(ftm->ftm_cmn->flags & FTM_FLAG_SCHED_INITED);
	ASSERT(bsscfg != NULL);
	BCM_REFERENCE(bsscfg);

	switch (sn->state) {
	case WL_PROXD_SESSION_STATE_DELAY:
		err = ftm_sched_compute_burst_start(ftm, sn);

		/* now that burst start is known, see if we are overriding what
		 * the initiator requested.
		 */
		if (err == BCME_OK)  {
#ifdef WL_FTM_MSCH
			if (bsscfg->type != BSSCFG_TYPE_SLOTTED_BSS) {
				err = pdftm_msch_begin(ftm, sn);
				if (err != BCME_OK) {
					FTM_LOGSCHED(ftm, (("wl%d: %s[%d]: sched slot %d "
						"for session %p\n",
						FTM_UNIT(ftm), __FUNCTION__, __LINE__, err,
						OSL_OBFUSCATE_BUF(sn))));
				}
			}
#endif // endif
			if (FTM_SESSION_IS_TARGET(sn)) {
				pdftm_expiration_t delay;
				delay = sn->ftm_state->burst_start - sn->ftm_state->delay_exp;
				if (delay > FTM_INTVL2USEC(&sn->config->burst_config->timeout))
					sn->flags |= FTM_SESSION_PARAM_OVERRIDE;
			}

			/* we need to delay until the  scheduled burst start. delay
			 * is also adjusted for non-asap to account for processing delay
			 */
			if (FTM_SESSION_IS_INITIATOR(sn)) {
				sn->ftm_state->delay_exp = sn->ftm_state->burst_start;
			}

			/* add processing delay for non-asap session */
			if (FTM_SESSION_IS_INITIATOR(sn) && !FTM_SESSION_IS_ASAP(sn) &&
					!(sn->flags & FTM_SESSION_FTM1_SENT)) {
				sn->ftm_state->burst_start += FTM_SESSION_NON_ASAP_BURST_DELAY_US;
			}

			/* Adjust timeout for trigger - note: after burst start is computed */
			if (sn->ftm_state->flags & FTM_SESSION_STATE_NEED_TRIGGER) {
				ASSERT(FTM_SESSION_IS_TARGET(sn));
				sn->ftm_state->delay_exp +=
					FTM_INTVL2USEC(&sn->config->burst_config->timeout);
			}
			FTM_LOGSCHED(ftm, (("wl%d: %s: session idx %d "
				"delay exp %u.%u burst_start %u.%u\n",
				FTM_UNIT(ftm), __FUNCTION__, sn->idx,
				FTM_LOG_TSF_ARG(sn->ftm_state->delay_exp),
				FTM_LOG_TSF_ARG(sn->ftm_state->burst_start))));
		}
		break;
	default:
		break;
	}

	/* note: wake sched anyway */
	(void)pdftm_wake_sched(ftm, sn);
	if (err != BCME_OK)
		goto done;

done:
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d session idx %d\n",
		FTM_UNIT(ftm), __FUNCTION__, err, sn->idx)));
	return err;
}

int
pdftm_sched_process_session_end(pdftm_t *ftm, pdftm_session_idx_t start,
	pdftm_session_idx_t end)
{
	int i;
	int err = BCME_OK;
	wlc_bsscfg_t *bsscfg;
	pdftm_cmn_t *ftm_cmn = ftm->ftm_cmn;
	for (i = start; i <= end; ++i) {
		pdftm_session_t *sn = ftm_cmn->sessions[i];
		if (!FTM_VALID_SESSION(sn))
			continue;
		ftm = sn->ftm;
		bsscfg = sn->bsscfg;
		BCM_REFERENCE(bsscfg);
		if (sn->state == WL_PROXD_SESSION_STATE_ENDED) {
#ifdef WL_FTM_MSCH
			if (bsscfg->type != BSSCFG_TYPE_SLOTTED_BSS)
				(void)pdftm_msch_end(ftm, sn);
#endif /* WL_FTM_MSCH */
			if (sn->flags & FTM_SESSION_DELETE_ON_STOP) {
				err = pdftm_change_session_state(sn, sn->status,
					WL_PROXD_SESSION_STATE_DESTROYING);
			}
		}
		if (sn->state == WL_PROXD_SESSION_STATE_DESTROYING)
			err = pdftm_change_session_state(sn, sn->status,
					WL_PROXD_SESSION_STATE_NONE);
	}
	return err;
}

int pdftm_ext_sched_session_ready(pdftm_session_t *sn)
{
	pdftm_t *ftm = sn->ftm;
	int err = BCME_OK;

	BCM_REFERENCE(ftm);
	/* log error if state is user_wait */
	if (sn->state == WL_PROXD_SESSION_STATE_USER_WAIT) {
		err = BCME_UNSUPPORTED;
		goto done;
	}

	if (!(sn->state == WL_PROXD_SESSION_STATE_START_WAIT)) {
		err = BCME_NOTREADY;
		goto done;
	}

	/* session security is not ready */
	if (sn->flags & FTM_SESSION_KEY_WAIT) {
		err = WL_PROXD_E_SEC_NOKEY;
		goto done;
	}

	/* session is not waiting for external scheduler */
	if (!(sn->flags & FTM_SESSION_EXT_SCHED_WAIT)) {
		err = WL_PROXD_E_BAD_STATE;
		goto done;
	}
done:
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d\n", FTM_UNIT(ftm), __FUNCTION__, err)));
	return err;
}

int pdftm_extsched_timeslot_available(pdftm_session_t *sn, uint64 duration_ms, chanspec_t chanspec,
		const struct ether_addr *peer)
{
	pdftm_t *ftm;
	uint64 burst_duration;
	int err = BCME_OK;

	ASSERT(FTM_VALID_SESSION(sn));
	ftm = (pdftm_t *) sn->ftm;

	burst_duration = FTM_INTVL2MSEC(&sn->config->burst_config->duration);
	/* check if the provided duration is sufficient for the burst on initiator ..
	* best effort on responder with the available sched duration
	*/
	if (FTM_SESSION_IS_INITIATOR(sn) && duration_ms < burst_duration) {
		err = WL_PROXD_E_NOAVAIL;
		goto done;
	} else {
		FTM_LOG(ftm, (("wl%d: %s: ext sched duration %u.%u\n",
			FTM_UNIT(ftm), __FUNCTION__, FTM_LOG_TSF_ARG(duration_ms))));

		/* set chanspec to the one provided */
		sn->config->burst_config->chanspec = chanspec;

		/* update duration and timeout..use avail duration */
		FTM_INIT_INTVL(&sn->config->burst_config->timeout, duration_ms,
				WL_PROXD_TMU_MILLI_SEC);

		if (duration_ms > FTM_PARAMS_BURSTTMO_MAX_MSEC) {
			duration_ms = FTM_PARAMS_BURSTTMO_MAX_MSEC;
		}

		FTM_INIT_INTVL(&sn->config->burst_config->duration, duration_ms,
				WL_PROXD_TMU_MILLI_SEC);

		err = ftm_session_validate_chanspec(ftm, sn);
		if (err != BCME_OK) {
			sn->state = WL_PROXD_SESSION_STATE_STOPPING;
			err = pdftm_sched_session(ftm, sn);
			goto done;
		}

		err = pdftm_validate_ratespec(ftm, sn);
		if (err != BCME_OK) {
			sn->state = WL_PROXD_SESSION_STATE_STOPPING;
			err = pdftm_sched_session(ftm, sn);
			goto done;
		}

		/* track the on-chan state in session flags */
		sn->flags |= FTM_SESSION_ON_CHAN;

		/* clear ext sched wait state */
		sn->flags &= ~FTM_SESSION_EXT_SCHED_WAIT;

		/* notify - start_ranging_event */
		err = pdftm_change_session_state(sn, BCME_OK, WL_PROXD_SESSION_STATE_STARTED);
		if (err != BCME_OK)
			goto done;
	}
done:
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d\n", FTM_UNIT(ftm), __FUNCTION__, err)));
	if (err) {
		FTM_INFO(("wl%d: pdftm_extsched_timeslot_available: status %d burst_dur %u.%u"
			" avail_dur %u.%u\n", FTM_UNIT(ftm), err,
			FTM_LOG_TSF_ARG(burst_duration), FTM_LOG_TSF_ARG(duration_ms)));
	}
	return err;
}

int pdftm_extsched_timeslot_unavailable(pdftm_session_t *sn)
{
	pdftm_t *ftm;
	int err = BCME_OK;
	uint64 cur_tsf;

	ftm = (pdftm_t *) sn->ftm;
	/* clear on-chan state */
	sn->flags &= ~FTM_SESSION_ON_CHAN;

	if (!(sn->state == WL_PROXD_SESSION_STATE_BURST)) {
		/* change state to user_wait on off_chan */
		FTM_GET_TSF(ftm, cur_tsf);
		if (sn->ftm_state) {
			sn->ftm_state->burst_exp = cur_tsf - 1;
		}
		(void)pdftm_change_session_state(sn, BCME_OK,
		WL_PROXD_SESSION_STATE_USER_WAIT);
	}
	else {
		/* support single burst only */
		err = pdburst_suspend(sn->ftm_state->burst);
		if (err != BCME_OK)
			goto done;
	}
done:
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d\n", FTM_UNIT(ftm), __FUNCTION__, err)));
	return err;

}

int
pdftm_session_register_ext_sched(pdftm_t *ftm, pdftm_session_t *sn)
{
	wlc_bsscfg_t *bsscfg;
	pdftm_session_config_t *sncfg;
	struct ether_addr *peer_mac;
	int err = BCME_OK;

	BCM_REFERENCE(peer_mac);

	bsscfg = sn->bsscfg;
	ASSERT(bsscfg != NULL);
	BCM_REFERENCE(bsscfg);
	sncfg = sn->config;
	if (bsscfg->type != BSSCFG_TYPE_SLOTTED_BSS) {
		return BCME_UNSUPPORTED;
	}

	if (!(FTM_SESSION_IS_ASAP(sn)) && !(sncfg->num_burst == 1)) {
		return BCME_UNSUPPORTED;
	}

	/* obtain a handle to the registration */
	peer_mac = &sncfg->burst_config->peer_mac;

	return err;
}
