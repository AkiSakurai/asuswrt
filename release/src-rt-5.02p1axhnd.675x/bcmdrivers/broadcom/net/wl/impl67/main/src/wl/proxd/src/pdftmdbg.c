/*
 * Proxd FTM method implementation - debug support. See twiki FineTimingMeasurement.
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
 * $Id: pdftmdbg.c 787304 2020-05-26 09:21:52Z $
 */

#include "pdftmpvt.h"

#if defined(BCMDBG) || defined(FTM_DONGLE_DEBUG)

static const char* ftm_tmu_str[] = {"TU", "s", "ms", "mu", "ns", "ps"};

static void
pdftm_dump_rtt_sample(const wl_proxd_rtt_sample_t *sample, struct bcmstrbuf *b)
{
	bcm_bprintf(b, "rtt %d: %d%s flags: 0x%02x rssi: %d ratespec 0x%08x\n", sample->id,
		sample->rtt.intvl, ftm_tmu_str[sample->rtt.tmu], sample->flags, sample->rssi,
		sample->ratespec);
}

static void
pdftm_dump_ftm_state(wl_proxd_session_id_t sid, const pdftm_session_state_t *state,
	struct bcmstrbuf *b)
{
	int i;

	bcm_bprintf(b, "ftm_state[%d]\n", sid);
	bcm_bprintf(b, "\tdelay: %u.%u\n", FTM_LOG_TSF_ARG(state->delay_exp));
	bcm_bprintf(b, "\tburst exp: %u.%u\n", FTM_LOG_TSF_ARG(state->burst_exp));
	bcm_bprintf(b, "\tburst start: %u.%u\n", FTM_LOG_TSF_ARG(state->burst_start));
	bcm_bprintf(b, "\tburst end: %u.%u\n", FTM_LOG_TSF_ARG(state->burst_end));
	bcm_bprintf(b, "\tdialog: 0x%d\n", state->dialog);
	bcm_bprintf(b, "\tflags: 0x%04x\n", state->flags);
	bcm_bprintf(b, "\trtt status: %d\n", state->rtt_status);
	bcm_bprintf(b, "\tresult flags: 0x%04x\n", state->result_flags);
	bcm_bprintf(b, "\tburst num: %d\n", state->burst_num);
	bcm_bprintf(b, "\tavg rtt: ");
	pdftm_dump_rtt_sample(&state->avg_rtt, b);
	bcm_bprintf(b, "\n");
	bcm_bprintf(b, "\tavg dist: %d (1/256m)\n", state->avg_dist);
	bcm_bprintf(b, "\tsd rtt: %d\n", state->sd_rtt);
	bcm_bprintf(b, "\tnum rtt: avail %d, valid %d max %d\n", state->num_rtt,
		state->num_valid_rtt, state->max_rtt);
	if (state->num_rtt && state->rtt) {
		bcm_bprintf(b, "\tbegin samples\n");
		for (i = 0; i < state->num_rtt; ++i) {
			bcm_bprintf(b, "\t\t");
			pdftm_dump_rtt_sample(&state->rtt[i], b);
			bcm_bprintf(b, "\n");
		}
	}

	bcm_bprintf(b, "\tmeas start: %u.%u\n", FTM_LOG_TSF_ARG(state->meas_start));
	bcm_bprintf(b, "\ttrig tsf : %u.%u\n", FTM_LOG_TSF_ARG(state->trig_tsf));
	bcm_bprintf(b, "\ttrig delta : %d\n", state->trig_delta);
	if (state->burst)
		pdburst_dump(state->burst, b);

	bcm_bprintf(b, "\n");
}

static void
pdftm_dump_counters(wl_proxd_session_id_t sid,
	const wl_proxd_counters_t *cnt, struct bcmstrbuf *b)
{
	bcm_bprintf(b, "counters[%d]\n", sid);
	bcm_bprintf(b, "\ttx: %d\n", cnt->tx);
	bcm_bprintf(b, "\trx: %d\n", cnt->rx);
	bcm_bprintf(b, "\tburst: %d\n", cnt->burst);
	bcm_bprintf(b, "\tsessions: %d\n", cnt->sessions);
	bcm_bprintf(b, "\tmax_sessions: %d\n", cnt->max_sessions);
	bcm_bprintf(b, "\tsched_fail: %d\n", cnt->sched_fail);
	bcm_bprintf(b, "\ttimeouts: %d\n", cnt->timeouts);
	bcm_bprintf(b, "\tprotoerr: %d\n", cnt->protoerr);
	bcm_bprintf(b, "\tnoack: %d\n", cnt->noack);
	bcm_bprintf(b, "\ttxfail: %d\n", cnt->txfail);
	bcm_bprintf(b, "\tlci_req_tx: %d\n", cnt->lci_req_tx);
	bcm_bprintf(b, "\tlci_req_rx: %d\n", cnt->lci_req_rx);
	bcm_bprintf(b, "\tlci_rep_tx: %d\n", cnt->lci_rep_tx);
	bcm_bprintf(b, "\tlci_rep_rx: %d\n", cnt->lci_rep_rx);
	bcm_bprintf(b, "\tcivic_req_tx: %d\n", cnt->civic_req_tx);
	bcm_bprintf(b, "\tcivic_req_rx: %d\n", cnt->civic_req_rx);
	bcm_bprintf(b, "\tcivic_rep_tx: %d\n", cnt->civic_rep_tx);
	bcm_bprintf(b, "\tcivic_rep_rx: %d\n", cnt->civic_rep_rx);
	bcm_bprintf(b, "\trctx: %d\n", cnt->rctx);
	bcm_bprintf(b, "\trctx_done: %d\n", cnt->rctx_done);
	bcm_bprintf(b, "\tpublish_err: %d\n", cnt->publish_err);
	bcm_bprintf(b, "\ton_chan: %d\n", cnt->on_chan);
	bcm_bprintf(b, "\toff_chan: %d\n", cnt->off_chan);
	bcm_bprintf(b, "\n");
}

static void
pdftm_dump_burst_config(const pdburst_config_t *bcfg, struct bcmstrbuf *b)
{
	bcm_bprintf(b, "\tburst config\n");
	bcm_bprintf(b, "\t\tpeer addr " MACF "\n", ETHER_TO_MACF(bcfg->peer_mac));
	bcm_bprintf(b, "\t\tchanspec : 0x%08x\n", bcfg->chanspec);
	bcm_bprintf(b, "\t\ttx power : %d\n", bcfg->tx_power);
	bcm_bprintf(b, "\t\tratespec : 0x%08x\n", bcfg->ratespec);

	bcm_bprintf(b, "\t\tduration : %d%s\n", bcfg->duration.intvl,
		ftm_tmu_str[bcfg->duration.tmu]);
	bcm_bprintf(b, "\t\tftm sep : %d%s\n", bcfg->ftm_sep.intvl,
		ftm_tmu_str[bcfg->ftm_sep.tmu]);
	bcm_bprintf(b, "\t\ttimeout: %d%s\n", bcfg->timeout.intvl,
		ftm_tmu_str[bcfg->timeout.tmu]);

	bcm_bprintf(b, "\t\tnum ftm : %d\n", bcfg->num_ftm);
	bcm_bprintf(b, "\t\tftm retries: %d\n", bcfg->ftm_retries);
	bcm_bprintf(b, "\t\tftm req retries: %d\n", bcfg->ftm_req_retries);
	bcm_bprintf(b, "\n");
}

/* dump local or peer (wl) availability */
static void
pdftm_dump_avail(const wl_proxd_avail_t *avail, wl_proxd_session_id_t sid,
	struct bcmstrbuf *b)
{
	int i;
	const wl_proxd_time_slot_t	*slot;

	bcm_bprintf(b, "\t%s availability:\n",
		sid == WL_PROXD_SESSION_ID_GLOBAL ? "local" : "peer");
	if (avail == (wl_proxd_avail_t *) NULL) {
		bcm_bprintf(b, "\t\tnot available\n");
		return;
	}

	bcm_bprintf(b, "\t\tflags: 0x%04x time_ref: 0x%4x num_slots: %d "
		"max_slots: %d repeat: %d%s\n",
		avail->flags, avail->time_ref, avail->num_slots, avail->max_slots,
		avail->repeat.intvl, ftm_tmu_str[avail->repeat.tmu]);

	slot = WL_PROXD_AVAIL_TIMESLOTS(avail);
	for (i = 0; i < avail->num_slots; ++i) {
		bcm_bprintf(b, "\t\tslots[%d]: start:%d%s duration:%d%s chanspec:0x%08x\n",
			i,
			slot->start.intvl, ftm_tmu_str[slot->start.tmu],
			slot->duration.intvl, ftm_tmu_str[slot->duration.tmu],
			slot->chanspec);
		slot++;
	}
}

void
pdftm_dump_session_config(wl_proxd_session_id_t sid,
	const pdftm_session_config_t *sncfg, struct bcmstrbuf *b)
{
	bcm_bprintf(b, "session config[%d]\n", sid);
	bcm_bprintf(b, "\tflags: 0x%08x\n", sncfg->flags);
	bcm_bprintf(b, "\tevent mask: 0x%08x\n", sncfg->event_mask);

	pdftm_dump_burst_config(sncfg->burst_config, b);

	bcm_bprintf(b, "\tnum burst : %d\n", sncfg->num_burst);
	bcm_bprintf(b, "\tinit delay : %d%s\n", sncfg->init_delay.intvl,
		ftm_tmu_str[sncfg->init_delay.tmu]);
	bcm_bprintf(b, "\tburst period : %d%s\n", sncfg->burst_period.intvl,
		ftm_tmu_str[sncfg->burst_period.tmu]);
	pdftm_dump_avail(sncfg->peer_avail, sid, b);
	bcm_bprintf(b, "\n");
}

static void
pdftm_dump_config(const pdftm_config_t *config, struct bcmstrbuf *b)
{
	bcm_bprintf(b, "\tflags: 0x%08x\n", config->flags);
	bcm_bprintf(b, "\tevent mask: 0x%08x\n", config->event_mask);
	bcm_bprintf(b, "\tdebug mask: 0x%08x\n", config->debug);
	bcm_bprintf(b, "\trx_max_burst: %d\n", config->rx_max_burst);
	pdftm_dump_avail(config->local_avail, WL_PROXD_SESSION_ID_GLOBAL, b);
	bcm_bprintf(b, "\tsession defaults:\n");
	pdftm_dump_session_config(WL_PROXD_SESSION_ID_GLOBAL, config->session_defaults, b);
	bcm_bprintf(b, "\t\tdev_addr " MACF "\n", ETHER_TO_MACF(config->dev_addr));
	bcm_bprintf(b, "\n");
}

static void
pdftm_dump_sched(const pdftm_sched_t *sched, struct bcmstrbuf *b)
{
	bcm_bprintf(b, "sched\n");
	bcm_bprintf(b, "\tstart_wait: %d\n", sched->in.start_wait);
	bcm_bprintf(b, "\tstarted: %d\n", sched->in.started);
	bcm_bprintf(b, "\tdelay: %d\n", sched->in.delay);
	bcm_bprintf(b, "\tsched_wait: %d\n", sched->in.sched_wait);
	bcm_bprintf(b, "\tburst: %d\n", sched->in.burst);
	if (sched->burst_sn) {
		bcm_bprintf(b, "\tburst session[%p] idx: %d\n", OSL_OBFUSCATE_BUF(sched->burst_sn),
			sched->burst_sn->idx);
	}
	bcm_bprintf(b, "\n");
}

static void
pdftm_dump_lci_req(const dot11_rmreq_ftm_lci_t *lci_req, struct bcmstrbuf *b)
{
	if (!lci_req)
		return;

	bcm_bprintf(b, "lci_req\n");
	bcm_bprintf(b, "\tid: %d\n", lci_req->id);
	bcm_bprintf(b, "\tlen: %d\n", lci_req->len);
	bcm_bprintf(b, "\ttoken: %d\n", lci_req->token);
	bcm_bprintf(b, "\tmode: %d\n", lci_req->mode);
	bcm_bprintf(b, "\tsubject: %d\n", lci_req->subj);
	bcm_bprintf(b, "\n");
}

static void
pdftm_dump_civic_req(const dot11_rmreq_ftm_civic_t *civic_req, struct bcmstrbuf *b)
{
	if (!civic_req)
		return;

	bcm_bprintf(b, "civic_req\n");
	bcm_bprintf(b, "\tid: %d\n", civic_req->id);
	bcm_bprintf(b, "\tlen: %d\n", civic_req->len);
	bcm_bprintf(b, "\ttoken: %d\n", civic_req->token);
	bcm_bprintf(b, "\tmode: %d\n", civic_req->mode);
	bcm_bprintf(b, "\tsubject: %d\n", civic_req->subj);
	bcm_bprintf(b, "\tcivloc_type: %d\n", civic_req->civloc_type);
	bcm_bprintf(b, "\tsiu: %d\n", civic_req->siu);
	bcm_bprintf(b, "\tsi: %d\n", civic_req->si);
	bcm_bprintf(b, "\n");
}

void
pdftm_dump_session(const pdftm_t *ftm, const pdftm_session_t *sn, struct bcmstrbuf *b)
{
	if (!sn)
		return;

	bcm_bprintf(b, "session[%d] flags 0x%08x\n", sn->idx, sn->flags);
	if (!FTM_VALID_SESSION(sn))
		return;

	bcm_bprintf(b, "\tsid: %d\n", sn->sid);
	bcm_bprintf(b, "\tbss index: %d\n", WLC_BSSCFG_IDX(sn->bsscfg));

	pdftm_dump_session_config(sn->sid, sn->config, b);

	bcm_bprintf(b, "\tstate: %d\n", sn->state);
	bcm_bprintf(b, "\tstatus: %d\n", sn->status);
	pdftm_dump_counters(sn->sid, sn->cnt, b);
	if (sn->state > WL_PROXD_SESSION_STATE_CONFIGURED && sn->ftm_state != NULL)
		pdftm_dump_ftm_state(sn->sid, sn->ftm_state, b);
	pdftm_dump_lci_req(sn->lci_req, b);
	pdftm_dump_civic_req(sn->civic_req, b);
	bcm_bprintf(b, "\ttsf off: %u.%u\n", FTM_LOG_TSF_ARG(sn->tsf_off));
	bcm_bprintf(b, "\ttsf scan state: %d\n", sn->tsf_scan_state);
	bcm_bprintf(b, "\n");
}

void
pdftm_dump(const pdftm_t *ftm, struct bcmstrbuf *b)
{
	int i;
	ASSERT(FTM_VALID(ftm));

	bcm_bprintf(b, "proxd ftm begin\n");
	bcm_bprintf(b, "\tmagic: 0x%08x\n",	ftm->magic);

	pdftm_dump_config(ftm->config, b);
	bcm_bprintf(b, "\tcaps: 0x%04x\n",	ftm->caps);

	bcm_bprintf(b, "\tenable bss: ");
	for (i = ftm->enabled_bss_len - 1; i >= 0; i--) {
		bcm_bprintf(b, "%02x", ftm->enabled_bss[i]);
	}
	bcm_bprintf(b, "\n");

	bcm_bprintf(b, "\tmax sessions: %d\n",	ftm->max_sessions);
	bcm_bprintf(b, "\tnum sessions: %d\n",	ftm->num_sessions);

	bcm_bprintf(b, "\tactive sessions: %d\n",
		wlc_ftm_num_sessions_inprog((const wlc_ftm_t *)ftm));
	for (i = 0; i < ftm->max_sessions; ++i) {
		pdftm_dump_session(ftm, ftm->sessions[i], b);
	}

	pdftm_dump_counters(WL_PROXD_SESSION_ID_GLOBAL, ftm->cnt, b);

	bcm_bprintf(b, "\tmru idx %d\n", ftm->mru_idx);
	pdftm_dump_sched(ftm->sched, b);
	bcm_bprintf(b, "\tlast sid %d\n", ftm->last_sid);
#ifdef WL_FTM_RANGE
	pdftm_dump_ranging_ctx(ftm->rctx, b);
#endif /* WL_FTM_RANGE */
	bcm_bprintf(b, "\tlast tsf: %u.%u\n", FTM_LOG_TSF_ARG(ftm->last_tsf));
	bcm_bprintf(b, "\tlast pmu: %u\n", ftm->last_pmu);
	bcm_bprintf(b, "proxd ftm end\n");
}

#ifdef WL_FTM_RANGE
void
pdftm_dump_ranging_ctx(const wlc_ftm_ranging_ctx_t *rctx, struct bcmstrbuf *b)
{
	int	i;
	if (!rctx)
		return;

	bcm_bprintf(b, "rctx %p begin\n", OSL_OBFUSCATE_BUF(rctx));
	bcm_bprintf(b, "\tmax_sids: %d\n",  rctx->max_sids);
	bcm_bprintf(b, "\tnum_done: %d\n",  rctx->num_done);
	bcm_bprintf(b, "\tstatus: %d\n",  rctx->status);
	bcm_bprintf(b, "\tstate: %d\n",  rctx->state);
	bcm_bprintf(b, "\tflags: 0x%04x\n", rctx->flags);
	bcm_bprintf(b, "\tnum_sids: %d\n",  rctx->num_sids);

	bcm_bprintf(b, "\tsids: ");
	for (i = 0; i < rctx->num_sids; ++i)
		bcm_bprintf(b, "%d ",  rctx->sids[i]);
	bcm_bprintf(b, "\n");

	bcm_bprintf(b, "rctx end\n");
}
#endif /* WL_FTM_RANGE */

void
pdftm_dump_pkt_body(pdftm_t *ftm, const uint8 *body, uint body_len,
	struct bcmstrbuf *b)
{
	uint8 action;
	const dot11_ftm_params_t *params = NULL;
	const bcm_tlv_t *tlv;
	const dot11_rm_ie_t *meas_req = NULL;
	uint tlv_size;

	action = body[DOT11_ACTION_ACT_OFF];
	if (action == DOT11_PUB_ACTION_FTM_REQ) {
		if (body_len < sizeof(dot11_ftm_req_t)) {
			bcm_bprintf(b, "Req Pkt: too short - length %d\n", body_len);
			return;
		}
		bcm_bprintf(b, "Req Pkt: Trigger %d\n", body[DOT11_ACTION_ACT_OFF+1]);
		body += sizeof(dot11_ftm_req_t);
		body_len -= sizeof(dot11_ftm_req_t);
		if (body_len <= 0)
			return;
	} else if (action == DOT11_PUB_ACTION_FTM) {
		if (body_len < sizeof(dot11_ftm_t)) {
			bcm_bprintf(b, "Measure Pkt: too short - length %d\n", body_len);
			return;
		}
		bcm_bprintf(b, "Measure Pkt: dialog %d followup %d\n", body[2], body[3]);
		bcm_bprintf(b, "TOD 0x%02x%02x%02x%02x%02x%02x Err 0x%02x%02x TOA "
			"0x%02x%02x%02x%02x%02x%02x Err 0x%02x%02x\n",
			body[4], body[5], body[6], body[7], body[8], body[9], body[16], body[17],
			body[10], body[11], body[12], body[13], body[14], body[15],
			body[18], body[19]);
		body += sizeof(dot11_ftm_t);
		body_len -= sizeof(dot11_ftm_t);
		if (body_len <= 0)
			return;
	} else {
		bcm_bprintf(b, "unknown pkt action %d length %d\n", action, body_len);
		return;
	}

	tlv = bcm_find_tlv((const uint8 *)body, body_len, DOT11_MNG_MEASURE_REQUEST_ID);
	if (tlv) {
		tlv_size = TLV_HDR_LEN + tlv->len;
		/* length check needed */
		meas_req = (const dot11_rm_ie_t *)tlv;
		if (meas_req->type == DOT11_MEASURE_TYPE_LCI) {
			bcm_bprintf(b, "LCI\n");
			pdftm_dump_lci_req((const dot11_rmreq_ftm_lci_t *)tlv, b);
		} else if (meas_req->type == DOT11_MEASURE_TYPE_CIVICLOC) {
			bcm_bprintf(b, "CIVIC\n");
			pdftm_dump_civic_req((const dot11_rmreq_ftm_civic_t *)tlv, b);
		} else {
			bcm_bprintf(b, "Unknown type %d\n", meas_req->type);
			return;
		}

		body_len = FTM_BODY_LEN_AFTER_TLV(body, body_len, tlv, tlv_size);
		body = (const uint8 *)tlv + tlv_size;
	}

	tlv = bcm_find_tlv((const uint8 *)body, body_len, DOT11_MNG_MEASURE_REQUEST_ID);
	if (tlv) {
		tlv_size = TLV_HDR_LEN + tlv->len;
		/* length check needed */
		meas_req = (const dot11_rm_ie_t *)tlv;
		if (meas_req->type == DOT11_MEASURE_TYPE_LCI) {
			bcm_bprintf(b, "LCI\n");
			pdftm_dump_lci_req((const dot11_rmreq_ftm_lci_t *)tlv, b);
		} else if (meas_req->type == DOT11_MEASURE_TYPE_CIVICLOC) {
			bcm_bprintf(b, "CIVIC\n");
			pdftm_dump_civic_req((const dot11_rmreq_ftm_civic_t *)tlv, b);
		} else {
			bcm_bprintf(b, "Unknown type %d\n", meas_req->type);
			return;
		}

		body_len = FTM_BODY_LEN_AFTER_TLV(body, body_len, tlv, tlv_size);
		body = (const uint8 *)tlv + tlv_size;
	}

	/* parse tlvs of interest - ftm param */
	tlv = bcm_find_tlv((const uint8 *)body, body_len, DOT11_MNG_FTM_PARAMS_ID);
	if (tlv) {
		tlv_size = TLV_HDR_LEN + tlv->len;
		params = (const dot11_ftm_params_t *)tlv;
		if (tlv_size < sizeof(*params)) {
			bcm_bprintf(b, "Params  too short: size %d\n", tlv_size);
			prhex("ftm params", body, body_len);
			return;
		}

		bcm_bprintf(b, "Params[0]: 0x%02X Status %d Value %d\n", params->info[0],
			FTM_PARAMS_STATUS(params), FTM_PARAMS_VALUE(params));
		bcm_bprintf(b, "Params[1]: 0x%02X BurstExpNum %d BurstDur %d\n",
			params->info[1],
			FTM_PARAMS_NBURSTEXP(params), FTM_PARAMS_BURSTTMO(params));
		bcm_bprintf(b, "Params[2]: 0x%02X MinDelta %dus\n", params->info[2],
			FTM_PARAMS_MINDELTA_USEC(params));
		bcm_bprintf(b, "Params[3,4]: 0x%02X%02X PartialTSF %d\n", params->info[3],
			params->info[4], FTM_PARAMS_PARTIAL_TSF(params));
		bcm_bprintf(b, "Params[5]: 0x%02X ASAP Capable %d ASAP %d Num-FTM %d\n",
			params->info[5], FTM_PARAMS_FTM1(params), FTM_PARAMS_ASAP(params),
			FTM_PARAMS_FTMS_PER_BURST(params));

		/* flush out */
		FTM_LOGPKT(ftm, (("%s\n", b->origbuf)));
		bcm_binit(b, b->origbuf, b->origsize);
		bzero(b->origbuf, b->origsize);

		bcm_bprintf(b, "Params[6]: 0x%02X FormatBW %d\n", params->info[6],
			FTM_PARAMS_CHAN_INFO(params));
		bcm_bprintf(b, "Params[7,8]: 0x%02X%02X BurstPeriod %dms\n",
			params->info[7], params->info[8], FTM_PARAMS_BURST_PERIOD_MS(params));

		body_len = FTM_BODY_LEN_AFTER_TLV(body, body_len, tlv, tlv_size);
		body = (const uint8 *)tlv + tlv_size;
	}
}

void pdftm_scb_dump(void *ctx, scb_t *scb, struct bcmstrbuf *b)
{
	pdftm_t *ftm = (pdftm_t *)ctx;
	ftm_scb_t *ftm_scb = FTM_SCB(ftm, scb);

	bcm_bprintf(b, "ftm info:\n");

	if (!ftm_scb)
		goto done;

	bcm_bprintf(b, "\tlen: %d tpk_max: %d tpk_len: %d\n",
		ftm_scb->len, ftm_scb->tpk_max, ftm_scb->tpk_len);
	if (ftm_scb->tpk) {
		int i;
		for (i = 0; i < ftm_scb->tpk_max; ++i) {
			bcm_bprintf(b, "%02x", ftm_scb->tpk[i]);
		}
	}

done:;
}

#endif // endif
