/*
 * wlc_taf_ias.c
 *
 * This file implements the WL driver infrastructure for the TAF module.
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
 * $Id$
 *
 */
/*
 * Include files.
 */

#include <wlc_pub.h>
#include <wlc_taf.h>
#include <wlc_taf_cmn.h>

#ifdef WLTAF_IAS
#include <wlc.h>
#include <wlc_dump.h>

#include <wlc_ampdu.h>
#if TAF_ENABLE_NAR
#include <wlc_nar.h>
#endif // endif
#if defined(AP) && defined(TAF_DBG)
#include <wlc_apps.h>
#endif // endif

#include <hnd_pktq.h>

#include <wlc_taf_priv_ias.h>

/* this is used for unified TID scheduling */
static const uint8 taf_tid_service_order[TAF_NUM_TIDSTATE] = {
	PRIO_8021D_NC, PRIO_8021D_VO, PRIO_8021D_VI,
	PRIO_8021D_CL, PRIO_8021D_EE, PRIO_8021D_BE,
	PRIO_8021D_BK, PRIO_8021D_NONE
};

/* scoring weights according AC (via tid), normalised to 256;
 * lower score gives higher air access ratio
 */
/* XXX currently this is only working between different SCB, it has no effect relative to
 * flows of different priority to the same SCB
 */
static uint8 taf_tid_score_weights[NUMPRIO] = {
	 80,	/* 0	AC_BE	Best Effort */
	 80,	/* 1	AC_BK	Background */
	 80,	/* 2	AC_BK	Background */
	 80,	/* 3	AC_BE	Best Effort */
	 80,	/* 4	AC_VI	Video */
	 80,	/* 5	AC_VI	Video */
	 80,	/* 6	AC_VO	Voice */
	 80	/* 7	AC_VO	Voice */
};

static INLINE void BCMFASTPATH
taf_update_item_stat(taf_method_info_t *method, taf_scb_cubby_t *scb_taf)
{

#ifdef TAF_DBG
#endif /* TAF_DBG */
}

static INLINE void BCMFASTPATH taf_update_item(taf_method_info_t *method, taf_scb_cubby_t* scb_taf)
{
	wlc_taf_info_t *taf_info = method->taf_info;
	struct scb *scb = scb_taf->scb;
	taf_scheduler_scb_stats_t* scb_stats = &scb_taf->info.scb_stats;

	/* Assume rate info is the same throughout all scheduling interval. */
	taf_rate_to_taf_units(taf_info, scb,  &scb_stats->ias.data.rspec,
		&scb_stats->ias.data.byte_rate, &scb_stats->ias.data.pkt_rate);

}

static void taf_ias_set_coeff(uint32 coeff, taf_ias_coeff_t* decay_coeff)
{
	uint32 index = 1;
	uint32 value = coeff << 6;

	decay_coeff->coeff1 = coeff;

	while (index < 10) {
		index++;
		value *= coeff;
		value >>= TAF_COEFF_IAS_MAX_BITS;
	}
	coeff = (value + (1 << 5)) >> 6;
	decay_coeff->coeff10 = coeff;

	while (index < 100) {
		index += 10;
		value *= coeff;
		value >>= TAF_COEFF_IAS_MAX_BITS;
	}
	coeff = (value + (1 << 5)) >> 6;
	decay_coeff->coeff100 = coeff;
}

static INLINE uint32 BCMFASTPATH
taf_ias_decay_score(uint32 initial, taf_ias_coeff_t* decay_coeff, uint32 elapsed, int32* correct)
{
	/* The decay coeff, is how much fractional reduction to occur per 2 milliseconds
	 * of elapsed time. This is an exponential model.
	 */
	uint32 value = initial;
	uint32 counter = elapsed >> 11; /* (1 << 11) is 2048 */

	if (correct) {
		*correct = (int32)(elapsed) - (int32)(counter << 11);
	}
	if (counter > 1000) {
		/* if it is a long time (2 second or more), just reset the score */
		value = 0;
	} else {
		while (value && (counter >= 100)) {
			value = value * decay_coeff->coeff100;
			value >>= TAF_COEFF_IAS_MAX_BITS; /* normalise coeff */
			counter -= 100;
		}
		while (value && (counter >= 10)) {
			value = value * decay_coeff->coeff10;
			value >>= TAF_COEFF_IAS_MAX_BITS; /* normalise coeff */
			counter -= 10;
		}
		while (value && counter--) {
			value = value * decay_coeff->coeff1;
			value >>= TAF_COEFF_IAS_MAX_BITS; /* normalise coeff */
		}
	}
	return value;
}

static INLINE void BCMFASTPATH taf_update_list(taf_method_info_t *method, uint32 now_time)
{
	taf_list_t *item = method->list;

	method->counter++;

	while (item) {
		int32  time_correction = 0;
		uint32 elapsed;
		uint32 score;
		taf_scb_cubby_t* scb_taf = item->scb_taf;

		scb_taf->timestamp = now_time;

		switch (method->type) {
			case TAF_EBOS:
				break;

			case TAF_PSEUDO_RR:
			case TAF_ATOS:
			case TAF_ATOS2:
				elapsed = now_time - scb_taf->info.scb_stats.ias.data.timestamp;
				score = taf_ias_decay_score(scb_taf->score, &method->ias->coeff,
					elapsed, &time_correction);
				if (score != scb_taf->score) {
					WL_TAFM(method, MACF" score %u --> %u (elapsed %u)\n",
						TAF_ETHERC(scb_taf), scb_taf->score, score,
						elapsed);
					scb_taf->score = score;
					scb_taf->info.scb_stats.ias.data.timestamp =
						now_time - time_correction;
				}
				break;
			default:
				TAF_ASSERT(0);
				break;
		}
		taf_update_item(method, scb_taf);
		taf_update_item_stat(method, scb_taf);

		item = item->next;
	}
}

static INLINE void BCMFASTPATH taf_prepare_list(taf_method_info_t *method, uint32 now_time)
{
	if (method->ordering == TAF_LIST_DO_NOT_SCORE) {
		return;
	}
	taf_update_list(method, now_time);

	if (method->ordering == TAF_LIST_SCORE_MINIMUM ||
			method->ordering == TAF_LIST_SCORE_MAXIMUM) {
		wlc_taf_sort_list(&method->list, method->ordering);
	}

}

static bool BCMFASTPATH
taf_ias_send_source(taf_method_info_t *method, taf_scb_cubby_t *scb_taf,
		int tid, taf_source_type_t s_idx, taf_schedule_tid_state_t* tid_state,
		taf_release_context_t *context)
{
	wlc_taf_info_t* taf_info;
	uint32 actual_release = 0;
#if TAF_ENABLE_SQS_PULL
	uint32 virtual_release = 0;
#else
#ifdef TAF_DBG
	const uint32 virtual_release = 0;
#endif /* TAF_DBG */
#endif /* TAF_ENABLE_SQS_PULL */
	uint32 released_units = 0;
	uint32 released_bytes = 0;
	uint32 time_limit_units;
	taf_ias_sched_data_t* ias_sched = TAF_IAS_TID_STATE(scb_taf, tid);
	taf_ias_sched_scbtid_stats_t* scbtidstats;
	bool force;

	TAF_ASSERT(ias_sched);

#ifdef TAF_DBG
	BCM_REFERENCE(virtual_release);
#endif // endif
	if (!ias_sched->handle[s_idx].used) {
		return TRUE;
	}

	taf_info = method->taf_info;
	time_limit_units = TAF_MICROSEC_TO_UNITS(tid_state->high[IAS_INDEXM(method)]);
	scbtidstats = &ias_sched->scbtidstats;
	force = (scb_taf->force & (1 << tid)) ? TRUE : FALSE;
	context->public.ias.was_emptied = FALSE;
	context->public.ias.is_ps_mode = scb_taf->info.ps_mode;

#if TAF_ENABLE_SQS_PULL
	if (TAF_SOURCE_IS_VIRTUAL(s_idx)) {
		TAF_ASSERT(scb_taf->info.pkt_pull_request[tid] == 0);
		context->public.ias.estimated_pkt_size_mean =
			scbtidstats->data.release_pkt_size_mean ?
			scbtidstats->data.release_pkt_size_mean : TAF_PKT_SIZE_DEFAULT;
		context->public.ias.traffic_count_available =
			scb_taf->info.traffic_count[s_idx][tid];

		/* margin here is used to ask SQS for extra packets beyond nominal */
		context->public.ias.margin = method->ias->margin;
	}
#endif /* TAF_ENABLE_SQS_PULL */

	/* aggregation optimisation */
	if (TAF_SOURCE_IS_AMPDU(s_idx) && !scb_taf->info.ps_mode) {
		 /* hold_for_aggs = AMSDU optimise */
		bool hold_for_aggs = (tid_state->trigger == TAF_TRIGGER_IMMEDIATE);
		/* hold_for_aggp = AMPDU optimise */
		bool hold_for_aggp;
		uint32 more_traffic = 0;
		taf_method_info_t* scb_method = scb_taf->method;
		uint32 wme_ac = SCB_WME(scb_taf->scb) ? WME_PRIO2AC(tid) : AC_BE;
#ifdef WLCNTSCB
		int32 in_flight = SCB_PKTS_INFLT_FIFOCNT_VAL(scb_taf->scb, tid);
#else
		const int32 in_flight = 0;
#endif /* WLCNTSCB */

		/* This controls aggregation optimisation for real packet release - release
		 * function can use to optimised (hold back) packets for aggregation.
		 * Aggregation optmisation is only done in case of immediate triggering;
		 * if normal IAS TX window is working, do not do this normally.
		 */

		if ((scb_method->type == TAF_EBOS) || (wme_ac == AC_VO)) {
			/* traffic is prioritised, do not hold back for aggregation */
			hold_for_aggs = FALSE;
		}

		/* check traffic in-transit, if 0 do not hold for aggregation as left over
		* aggregation packets might never get sent without follow-up
		* new traffic which is not guaranteed to come
		*/
		hold_for_aggs = hold_for_aggs && (in_flight > 0);

		/* if already plenty of in flight traffic, do not hold AMPDU as AQM can be fed */
		hold_for_aggp = hold_for_aggs && (in_flight < scb_taf->info.max_pdu);

#if TAF_ENABLE_SQS_PULL
		/* Is there is more traffic on the way very soon...? Aggregation can be held as
		 * forthcoming traffic will add to packets already available. Leftover aggregation
		 * packets will ripple forward into the new arriving data where this decision will
		 * be made again next time.
		 */
		more_traffic = (taf_info->op_state == TAF_SCHEDULE_VIRTUAL_PACKETS) ?
			scb_taf->info.traffic_count[TAF_SOURCE_HOST_SQS][tid] :
			scb_taf->info.pkt_pull_request[tid];
#endif /* TAF_ENABLE_SQS_PULL */

		hold_for_aggs = hold_for_aggs || (more_traffic > 0);
		hold_for_aggp = hold_for_aggp || (more_traffic > 0);

		//WL_TAFM(method, "hold_for_agg %u/%u (trig %d, in-flight %u, more traffic %u, "
		//	"wme %u, maxpdu %u)\n", hold_for_aggs, hold_for_aggp, tid_state->trigger,
		//	in_flight, more_traffic, wme_ac, scb_taf->info.max_pdu);

		/* flag to enable aggregation optimisation or not */
		context->public.ias.opt_aggs = hold_for_aggs;
		context->public.ias.opt_aggp = hold_for_aggp;

		/* pdu number threshold for AMPDU holding */
		context->public.ias.opt_aggp_limit =
			scb_method->ias->opt_aggp_limit < scb_taf->info.max_pdu ?
			scb_method->ias->opt_aggp_limit : scb_taf->info.max_pdu;
	} else {
		context->public.ias.opt_aggs = FALSE;
		context->public.ias.opt_aggp = FALSE;
	}
#ifdef TAF_DBG
	context->public.complete = TAF_REL_COMPLETE_TIME_LIMIT;
#endif // endif

	/* for accounting, real or virtual both get counted here */
	while ((tid_state->total.released_units + released_units) < time_limit_units) {

		bool result;

		context->public.ias.total.released_units = tid_state->total.released_units +
				released_units;

		context->public.mode = TAF_SOURCE_IS_REAL(s_idx) ?
#ifdef WLCFP
			TAF_RELEASE_MODE_REAL_FAST :
#else
			TAF_RELEASE_MODE_REAL :
#endif /* WLCFP */
			TAF_RELEASE_MODE_VIRTUAL;

#ifdef TAF_DBG
		context->public.complete = TAF_REL_COMPLETE_NULL;
#endif // endif

		result = taf_info->funcs[s_idx].release_fn(*taf_info->source_handle_p[s_idx],
			ias_sched->handle[s_idx].scbh, ias_sched->handle[s_idx].tidh, force,
			&context->public);

#if TAF_ENABLE_SQS_PULL
		if (TAF_SOURCE_IS_VIRTUAL(s_idx)) {
			TAF_ASSERT(context->public.ias.actual.release == 0);

			virtual_release += context->public.ias.virtual.release;
			context->public.ias.virtual.release = 0;

			released_units += context->public.ias.virtual.released_units;
			context->public.ias.virtual.released_units = 0;
		} else
#endif /* TAF_ENABLE_SQS_PULL */
		{
			actual_release += context->public.ias.actual.release;
			context->public.ias.actual.release = 0;

			released_units += context->public.ias.actual.released_units;
			context->public.ias.actual.released_units = 0;

			released_bytes += context->public.ias.actual.released_bytes;
			context->public.ias.actual.released_bytes = 0;
		}

		if (context->public.ias.was_emptied) {
			break;
		}
#if TAF_ENABLE_SQS_PULL
		if ((TAF_SOURCE_IS_REAL(s_idx) && context->public.ias.actual.release == 0) ||
			(TAF_SOURCE_IS_VIRTUAL(s_idx) &&
			context->public.ias.virtual.release == 0) ||
			!result)
#else
		if (context->public.ias.actual.release == 0 || !result)
#endif /* TAF_ENABLE_SQS_PULL */
		{
			break;
		}

		if (force && released_units >= TAF_MICROSEC_TO_UNITS(taf_info->force_time)) {
			scb_taf->force &= ~(1 << tid);
			break;
		}
	}

	if (released_units) {
		WL_TAFM(method, "%s: total rel_units %u, rel_units %u, "
			"%u real / %u virtual sent, stop reason: %s\n", TAF_SOURCE_NAME(s_idx),
			tid_state->total.released_units, released_units,
			actual_release, virtual_release,
			taf_rel_complete_text[context->public.complete]);
	} else {
		WL_TAFM(method, "%s: no release (%u/%u) reason: %s\n", TAF_SOURCE_NAME(s_idx),
			tid_state->total.released_units, time_limit_units,
			taf_rel_complete_text[context->public.complete]);
	}

	/* the following is statistics across the TID state which is global across all SCB using
	 * that TID
	 */
	tid_state->total.released_units += released_units;

#if TAF_ENABLE_SQS_PULL
	if (virtual_release) {
		TAF_ASSERT(TAF_SOURCE_IS_VIRTUAL(s_idx));
		context->virtual_release += virtual_release;
		scb_taf->info.pkt_pull_request[tid] = virtual_release;
		scb_taf->info.pkt_pull_map |= (1 << tid);
		taf_info->total_pull_requests++;
	}
#endif /* TAF_ENABLE_SQS_PULL */
	if (actual_release)
	{
#if TAF_ENABLE_SQS_PULL
		TAF_ASSERT(TAF_SOURCE_IS_REAL(s_idx));
#endif /* TAF_ENABLE_SQS_PULL */

		tid_state->real.released_units += released_units;

		/* the following statistics are per SCB,TID link */
		scbtidstats->data.release_pcount += actual_release;
		scbtidstats->data.release_bytes += released_bytes;
		scbtidstats->data.release_units += released_units;

		/* this is global across the whole scheduling interval */
		context->actual_release += actual_release;
#ifdef TAF_DBG
		scbtidstats->debug.did_release++;
#endif /* TAF_DBG */
	}
	return context->public.ias.was_emptied;
}

static INLINE void BCMFASTPATH
taf_ias_mean_pktlen(taf_ias_sched_scbtid_stats_t* scbtidstats, uint32 timestamp,
	taf_ias_coeff_t* decay)
{
#if TAF_ENABLE_SQS_PULL || defined(TAF_DBG)
	/*
	* This calculates the average packet size from what has been released. It is going to be
	* used under SQS, to forward predict how many virtual packets to be converted.
	*/
	if (scbtidstats->data.release_pcount) {
		uint32 mean;
		uint32 coeff = 0;
		uint32 elapsed = timestamp - scbtidstats->data.calc_timestamp;
		int32 time_correction = 0;

		coeff = taf_ias_decay_score(TAF_COEFF_IAS_MAX, decay, elapsed, &time_correction);

		if (coeff < TAF_COEFF_IAS_MAX) {
			uint64 calc;
			scbtidstats->data.calc_timestamp = timestamp - time_correction;

			/* may overflow 32 bit int, use 64 bit */
			calc = ((uint64)scbtidstats->data.total_scaled_pcount * (uint64)coeff);
			calc += (TAF_COEFF_IAS_MAX >> 1);
			calc >>= TAF_COEFF_IAS_MAX_BITS;
			scbtidstats->data.total_scaled_pcount = calc;

			/* likely to overflow 32 bit int, use 64 bit */
			calc = ((uint64)scbtidstats->data.total_bytes * (uint64)coeff);
			calc >>= TAF_COEFF_IAS_MAX_BITS;
			scbtidstats->data.total_bytes = calc;
		}

		/*
		* pcount is scaled here, as at low data rates, the discrete low packet count
		* loses precision when doing exponential decay; the scaling gives a
		* notional fractional pkt count to help the accuracy
		*/
		scbtidstats->data.total_scaled_pcount += (scbtidstats->data.release_pcount
				<< TAF_PCOUNT_SCALE_SHIFT);
		scbtidstats->data.total_bytes += scbtidstats->data.release_bytes;

		/*
		* XXX avoid 64 bit divide, use standard 32 bit, so TAF_PCOUNT_SCALE_SHIFT
		* can't be too big
		*/

		TAF_ASSERT(scbtidstats->data.total_bytes < (1 << (32 - TAF_PCOUNT_SCALE_SHIFT)));

		mean = scbtidstats->data.total_bytes << TAF_PCOUNT_SCALE_SHIFT;

		TAF_ASSERT(scbtidstats->data.total_scaled_pcount);

		mean /= (scbtidstats->data.total_scaled_pcount);

		scbtidstats->data.release_pkt_size_mean = mean;
#ifdef TAF_DBG
		scbtidstats->debug.release_pkt_size_mean = mean;
#endif // endif
	};
#endif /* TAF_ENABLE_SQS_PULL || TAF_DBG */

}

static INLINE void BCMFASTPATH
taf_ias_sched_update(taf_method_info_t *method, taf_scb_cubby_t *scb_taf,
		int tid, taf_schedule_tid_state_t *tid_state, taf_release_context_t *context)

{
	taf_ias_sched_data_t* ias_sched = TAF_IAS_TID_STATE(scb_taf, tid);
	taf_ias_sched_scbtid_stats_t* scbtidstats = &ias_sched->scbtidstats;
	uint32 new_score;
#ifdef TAF_DBG
	uint32 rel_delta = 0;
#endif // endif
	/* clear the force status */
	scb_taf->force &= ~(1 << tid);

#if TAF_ENABLE_SQS_PULL
	if (scb_taf->info.pkt_pull_map == 0) {
		/* reset release limit tracking after pull cycle has completed */
		scb_taf->info.released_units_limit = 0;
	}
#endif /* TAF_ENABLE_SQS_PULL */

	if (!scbtidstats->data.release_pcount) {
		return;
	}

#if TAF_ENABLE_SQS_PULL
	if (scb_taf->info.released_units_limit > 0) {
		if (scb_taf->info.released_units_limit > scbtidstats->data.release_units) {
			scb_taf->info.released_units_limit -= scbtidstats->data.release_units;
		} else {
			/* minus -1 to flag release limit already reached */
			scb_taf->info.released_units_limit = -1;
		}
	}
#endif /* TAF_ENABLE_SQS_PULL */

	/* These following very few lines of code, encompass the entire difference between
	* EBOS, PRR and ATOS. The concept expressed here is critical to the essential behaviour,
	* and nothing much else is different in any way.
	* In every other respect, EBOS, PRR and ATOS are identical. ATOS2 behaviour happens just
	* as a consequence of coming after ATOS.
	*/

	/* NOTE: this score is global across all tid */
	switch (method->type) {
		case TAF_EBOS:
			break;

		case TAF_PSEUDO_RR:
			/* this is packet number scoring (or could use released_bytes instead) */
			new_score = scb_taf->score +
				taf_units_to_score(method, tid,
				scbtidstats->data.release_pcount << 6);
			scb_taf->score = MIN(new_score, TAF_SCORE_IAS_MAX);
			break;

		case TAF_ATOS:
		case TAF_ATOS2:
			/* this is scheduled airtime scoring */
			new_score = scb_taf->score +
				taf_units_to_score(method, tid, scbtidstats->data.release_units);
			scb_taf->score = MIN(new_score, TAF_SCORE_IAS_MAX);
			break;

		default:
			TAF_ASSERT(0);
	}
	taf_ias_mean_pktlen(scbtidstats, taf_timestamp(TAF_WLCM(method)), &method->ias->coeff);

#ifdef AP
	if (scb_taf->info.traffic_available == 0) {
		//WL_TAFM(method, "TO DO traffic_available == 0\n");
		//wlc_apps_pvb_update(TAF_WLCM(method), scb_taf->scb);
	}
#endif /* AP */
	//WL_TAFM(method, "traffic %s available 0x%x\n",
	//	scb_taf->info.traffic_available & (1 << tid) ? "still" : "no more",
	//	scb_taf->info.traffic_available);

#ifdef TAF_DBG

	if (method->taf_info->scheduler_index != scbtidstats->index) {
		rel_delta = scb_taf->timestamp - scbtidstats->debug.did_rel_timestamp;

		if (rel_delta > scbtidstats->debug.max_did_rel_delta) {
			scbtidstats->debug.max_did_rel_delta = rel_delta;
		}
		scbtidstats->debug.did_rel_delta += rel_delta;
		scbtidstats->debug.did_rel_timestamp = scb_taf->timestamp;

		scbtidstats->debug.release_frcount ++;
		scbtidstats->index = method->taf_info->scheduler_index;
	}
	scbtidstats->debug.release_pcount += scbtidstats->data.release_pcount;
	scbtidstats->debug.release_time += TAF_UNITS_TO_MICROSEC(scbtidstats->data.release_units);

	if (context->public.ias.was_emptied) {
		scbtidstats->debug.emptied++;
	}
	WL_TAFM(method, MACF" tid %u rate %d, rel %d pkts %d us, "
		"total %u us, prev rel %u\n",
		TAF_ETHERC(scb_taf), tid, RSPEC2KBPS(scb_taf->info.scb_stats.ias.data.rspec),
		scbtidstats->data.release_pcount,
		TAF_UNITS_TO_MICROSEC(scbtidstats->data.release_units),
		TAF_UNITS_TO_MICROSEC(tid_state->total.released_units), rel_delta);

#endif /* TAF_DBG */

	/* reset before next scheduler invocation */
	scbtidstats->data.release_pcount = 0;
	scbtidstats->data.release_bytes = 0;
	scbtidstats->data.release_units = 0;
}

static INLINE bool BCMFASTPATH
taf_ias_est_release_time(taf_scb_cubby_t *scb_taf, int tid, int len)
{
	taf_ias_sched_data_t* ias_sched = TAF_IAS_TID_STATE(scb_taf, tid);
	taf_ias_sched_scbtid_stats_t* scbtidstats = &ias_sched->scbtidstats;

	uint16 est_pkt_size = scbtidstats->data.release_pkt_size_mean ?
		scbtidstats->data.release_pkt_size_mean : TAF_PKT_SIZE_DEFAULT;

	return len * TAF_PKTBYTES_TO_UNITS(est_pkt_size,
		scb_taf->info.scb_stats.ias.data.pkt_rate,
		scb_taf->info.scb_stats.ias.data.byte_rate);
}

static INLINE bool BCMFASTPATH
taf_ias_sched_send(taf_method_info_t *method, taf_scb_cubby_t *scb_taf, int tid,
	taf_schedule_tid_state_t *tid_state, taf_release_context_t *context,
	taf_schedule_state_t op_state)
{
	wlc_taf_info_t* taf_info = method->taf_info;
	uint32 time_limit_units = TAF_MICROSEC_TO_UNITS(tid_state->high[IAS_INDEXM(method)]);
#if TAF_ENABLE_SQS_PULL
	const taf_source_type_t start_source = TAF_FIRST_REAL_SOURCE;
	const taf_source_type_t end_source = (op_state == TAF_SCHEDULE_VIRTUAL_PACKETS) ?
		TAF_NUM_SCHED_SOURCES : TAF_NUM_REAL_SOURCES;
	uint16 v_avail = scb_taf->info.traffic_count[TAF_SOURCE_HOST_SQS][tid];
	uint32 v_prior_released = 0;
	uint32 pre_release_skip = 0;
#else
	const taf_source_type_t start_source = TAF_FIRST_REAL_SOURCE;
	const taf_source_type_t end_source = TAF_NUM_REAL_SOURCES;
#endif /* TAF_ENABLE_SQS_PULL */
	const bool force = (scb_taf->force & (1 << tid)) ? (taf_info->force_time > 0) : FALSE;
	taf_source_type_t s_idx;

	context->public.how = TAF_RELEASE_LIKE_IAS;

#if TAF_ENABLE_SQS_PULL
	context->public.ias.virtual.released_units = 0;
	context->public.ias.virtual.release = 0;
#endif /* TAF_ENABLE_SQS_PULL */
	context->public.ias.actual.released_units = 0;
	context->public.ias.actual.released_bytes = 0;
	context->public.ias.actual.release = 0;

	context->public.ias.byte_rate = scb_taf->info.scb_stats.ias.data.byte_rate;
	context->public.ias.pkt_rate = scb_taf->info.scb_stats.ias.data.pkt_rate;

#if TAF_ENABLE_SQS_PULL
	/* init the scb tracking for release limiting */
	if ((op_state == TAF_SCHEDULE_VIRTUAL_PACKETS) &&
		(scb_taf->info.released_units_limit == 0)) {

		scb_taf->info.released_units_limit = force ?
			TAF_MICROSEC_TO_UNITS(taf_info->force_time) :

			/* access method ptr via scb_taf for SQS_PULL */
			TAF_MICROSEC_TO_UNITS(scb_taf->method->ias->release_limit);
	}
	if (scb_taf->info.released_units_limit >= 0) {
		context->public.ias.released_units_limit = scb_taf->info.released_units_limit;
	} else {
		/* limit already reached (set to -1 in taf_ias_sched_update) */
		WL_TAFM(method, MACF" release limit (%uus) reached\n", TAF_ETHERC(scb_taf),
			scb_taf->method->ias->release_limit);
		/* have to return TRUE here (as if emptied) so IAS continues with next station */
		return TRUE;
	}
#else
	if (force) {
		context->public.ias.released_units_limit =
			TAF_MICROSEC_TO_UNITS(taf_info->force_time);
	} else {
		context->public.ias.released_units_limit =
			TAF_MICROSEC_TO_UNITS(method->ias->release_limit);
	}
#endif /* TAF_ENABLE_SQS_PULL */

	context->public.ias.time_limit_units = time_limit_units;

	for (s_idx = start_source; s_idx < end_source; s_idx++) {

		if (scb_taf->info.scb_stats.ias.data.use[s_idx]) {
			bool emptied;
			uint16 pending = scb_taf->info.traffic_count[s_idx][tid];
#if TAF_ENABLE_SQS_PULL
			if (op_state == TAF_SCHEDULE_VIRTUAL_PACKETS) {
				uint16 pre_rel_limit = TAF_SOURCE_IS_REAL(s_idx) ?
					method->ias->pre_rel_limit[s_idx] : 0;

				if (pending == 0) {
					continue;
				}

				if (!scb_taf->info.ps_mode && !force && pre_rel_limit &&
					(pending < pre_rel_limit) && (v_avail > 0) &&
					(TXPKTPENDTOT(TAF_WLCT(taf_info)) > 0) &&
					taf_ias_est_release_time(scb_taf, tid, pending) <
					(time_limit_units / 2)) {

					v_prior_released = context->virtual_release;

					WL_TAFM(method, "skip real pre-rel to "MACF" tid %u %s "
						"only %u pending (thresh %u), v_avail %u, ps %u\n",
						TAF_ETHERC(scb_taf), tid, TAF_SOURCE_NAME(s_idx),
						pending, pre_rel_limit,
						v_avail, scb_taf->info.ps_mode);

					pre_release_skip++;
					continue;

				} else if (pre_rel_limit) {
					WL_TAFM(method, "do real pkt pre-rel to "MACF" tid %u %s "
						"%u pending (thresh %u), v_avail %u, ps %u "
						"force %u in-transit %u\n",
						TAF_ETHERC(scb_taf), tid, TAF_SOURCE_NAME(s_idx),
						pending, pre_rel_limit, v_avail,
						scb_taf->info.ps_mode, force,
						TXPKTPENDTOT(TAF_WLCT(taf_info)));
				}
			}
#else
			if (pending == 0) {
				WL_TAFM(method, MACF" tid %u %s pending 0\n",
					TAF_ETHERC(scb_taf), tid, TAF_SOURCE_NAME(s_idx));
				continue;
			}
#endif /* TAF_ENABLE_SQS_PULL */

			emptied = taf_ias_send_source(method, scb_taf, tid, s_idx, tid_state,
				context);
#if TAF_ENABLE_SQS_PULL
			if (TAF_SOURCE_IS_VIRTUAL(s_idx) && (pre_release_skip > 0) &&
					(context->virtual_release - v_prior_released == 0)) {
#ifdef TAF_DBG
				uint32 in_transit = TXPKTPENDTOT(TAF_WLCT(taf_info));

				WL_TAFM(method, "pre-release was skipped but no virtual "
					"packets finally possible! (in transit %u)\n", in_transit);

				/* if packets are in transit we are guaranteed to be triggered
				 * later to have another chance to send these;
				 * else otherwise we risk a stall so trap this
				 */
				TAF_ASSERT(in_transit > 0);
#endif /* TAF_DBG */
			}
#endif /* TAF_ENABLE_SQS_PULL */

			if (emptied) {
				scb_taf->info.traffic_map[s_idx] &= ~(1 << tid);
			} else {
				scb_taf->info.traffic_map[s_idx] |= (1 << tid);
			}
		}
	}
	scb_taf->info.traffic_available = 0;
	for (s_idx = TAF_FIRST_REAL_SOURCE; s_idx < TAF_NUM_SCHED_SOURCES; s_idx++) {
		scb_taf->info.traffic_available |= scb_taf->info.traffic_map[s_idx];
	}
	/* return whether SCB, TID was emptied (TRUE) or not */
	return (scb_taf->info.traffic_available & (1 << tid)) == 0;
}

static bool BCMFASTPATH
taf_ias_schedule_scb(taf_method_info_t *method, taf_schedule_tid_state_t *tid_state,
	int tid, taf_release_context_t *context, taf_schedule_state_t op_state)
{
	wlc_taf_info_t* taf_info = method->taf_info;
	uint32 high_units = TAF_MICROSEC_TO_UNITS(tid_state->high[IAS_INDEXM(method)]);
	taf_list_t *list;
#if TAF_ENABLE_SQS_PULL
	const taf_source_type_t start_source = TAF_FIRST_REAL_SOURCE;
	const taf_source_type_t end_source = (op_state == TAF_SCHEDULE_VIRTUAL_PACKETS) ?
		TAF_NUM_SCHED_SOURCES : TAF_NUM_REAL_SOURCES;
#else
	const taf_source_type_t start_source = TAF_FIRST_REAL_SOURCE;
	const taf_source_type_t end_source = TAF_NUM_REAL_SOURCES;
#endif /* TAF_ENABLE_SQS_PULL */

	method->ias->tid_active &= ~(1 << tid);

	for (list = method->list; list; list = list->next) {
		taf_scb_cubby_t *scb_taf = list->scb_taf;
		struct scb *scb = scb_taf->scb;
		taf_ias_sched_data_t* ias_sched = TAF_IAS_TID_STATE(scb_taf, tid);
		bool emptied;
		bool traffic_active = FALSE;
		taf_source_type_t s_idx;

		if (!ias_sched) {
			scb_taf->force &= ~(1 << tid);
			WL_TAFM(method, "skip "MACF" tid %u, no context\n",
				TAF_ETHERS(scb), tid);
			continue;
		}

		if (SCB_MARKED_DEL(scb) || SCB_DEL_IN_PROGRESS(scb)) {
			scb_taf->force &= ~(1 << tid);
			WL_TAFM(method, "skip "MACF", will be deleted\n", TAF_ETHERS(scb));
			continue;
		}

		method->ias->tid_active |= (1 << tid);

#if !TAF_ENABLE_SQS_PULL
		if (scb_taf->info.ps_mode) {
			/* cannot force so clear the force status */
			scb_taf->force &= ~(1 << tid);
#ifdef TAF_DBG
			WL_TAFM(method, "skip "MACF" tid %u ps %u psq\n",
				TAF_ETHERS(scb), tid, wlc_apps_psq_len(taf_info->wlc, scb));

			for (s_idx = start_source; s_idx < end_source; s_idx ++) {
				uint16 (*pktqlen_fn) (void *, void *) =
					taf_info->funcs[s_idx].pktqlen_fn;
				taf_sched_handle_t* handle = &ias_sched->handle[s_idx];

				if (handle->used) {
					uint16 pktqlen;
					if ((pktqlen = pktqlen_fn(handle->scbh, handle->tidh))) {

						WL_TAFM(method, "PS tid %u qlen %s: %u\n",
							tid, TAF_SOURCE_NAME(s_idx), pktqlen);
					}
				}
			}
			scb_taf->info.scb_stats.ias.debug.skip_ps++;
#endif /* TAF_DBG */
			continue;
		}
		TAF_ASSERT(!SCB_PS(scb));
#endif /* !TAF_ENABLE_SQS_PULL */

		if (scb_taf->info.data_block_mode) {
			/* cannot force so clear the force status */
			scb_taf->force &= ~(1 << tid);
			WL_TAFM(method, "skip "MACF" data block\n",
				TAF_ETHERS(scb));
#ifdef TAF_DBG
			scb_taf->info.scb_stats.ias.debug.skip_data_block++;
#endif /* TAF_DBG */
			continue;
		}

		/* this loops over AMPDU (and NAR when implemented) */
		for (s_idx = start_source; s_idx < end_source; s_idx ++) {
			uint16 (*pktqlen_fn) (void *, void *) = taf_info->funcs[s_idx].pktqlen_fn;
			taf_sched_handle_t* handle = &ias_sched->handle[s_idx];

			scb_taf->info.traffic_map[s_idx] &= ~(1 << tid);

			if (handle->used) {
				if ((scb_taf->info.traffic_count[s_idx][tid] =
					pktqlen_fn(handle->scbh, handle->tidh))) {

					traffic_active = TRUE;
					scb_taf->info.traffic_map[s_idx] |= (1 << tid);

					WL_TAFM(method, MACF" tid %u qlen %s: %u\n",
						TAF_ETHERS(scb), tid,
						TAF_SOURCE_NAME(s_idx),
						scb_taf->info.traffic_count[s_idx][tid]);
				}
			} else {
				scb_taf->info.traffic_count[s_idx][tid] = 0;
			}
		}

		if (!traffic_active) {
			/* no traffic - clear the force status */
			scb_taf->force &= ~(1 << tid);
			continue;
		}
#ifdef TAF_DBG
		ias_sched->scbtidstats.debug.ready++;
#endif /* TAF_DBG */

		emptied = taf_ias_sched_send(method, scb_taf, tid, tid_state, context, op_state);

		taf_ias_sched_update(method, scb_taf, tid, tid_state, context);

		if (!emptied) {
			return TRUE;
		}
		if (tid_state->total.released_units >= high_units) {
			return TRUE;
		}
	}
	return FALSE;
}

static bool BCMFASTPATH
taf_ias_schedule_all_scb(taf_method_info_t *method, taf_schedule_tid_state_t *tid_state,
	int tid, taf_release_context_t *context)
{
	taf_schedule_state_t op_state = context->op_state;

	return taf_ias_schedule_scb(method, tid_state, tid, context, op_state);
}

static void taf_ias_time_settings_sync(taf_method_info_t* method)
{
	wlc_taf_info_t* taf_info = method->taf_info;
	taf_ias_group_info_t* ias_info = TAF_IAS_GROUP_INFO(taf_info);

#if TAF_ENABLE_SQS_PULL
	TAF_ASSERT(TAF_UNITED_ORDER(taf_info));
#else
	int tid;

	for (tid = 0; tid < TAF_NUM_TIDSTATE; tid++) {
		ias_info->tid_state[tid].high[IAS_INDEXM(method)] = method->ias->high;
		ias_info->tid_state[tid].low[IAS_INDEXM(method)] = method->ias->low;
	}
#endif /* TAF_ENABLE_SQS_PULL */

	WL_TAFM(method, "high = %us, low = %uus\n", method->ias->high, method->ias->low);

	ias_info->unified_tid_state.high[IAS_INDEXM(method)] = method->ias->high;
	ias_info->unified_tid_state.low[IAS_INDEXM(method)] = method->ias->low;
}

static INLINE bool BCMFASTPATH
taf_ias_schedule_all_tid(taf_method_info_t* method, taf_schedule_tid_state_t *tid_state,
	int tid_index_start, int tid_index_end, taf_release_context_t* context)
{
	bool finished = FALSE;
	int tid_index = tid_index_start;

	do {
		int tid = taf_tid_service_order[tid_index];

		if (method->ias->tid_active & (1 << tid)) {
			finished = taf_ias_schedule_all_scb(method, tid_state, tid, context);
		}
		if (!finished) {
			tid_index++;
		}
	} while (!finished && tid_index <= tid_index_end);

	return finished;
}

static INLINE BCMFASTPATH bool
taf_ias_completed(taf_method_info_t *method, taf_release_context_t *context,
	taf_schedule_tid_state_t *tid_state, bool finished, uint32 now_time,
	taf_schedule_state_t op_state)
{
#if TAF_ENABLE_SQS_PULL
	bool virtual_scheduling = (op_state == TAF_SCHEDULE_VIRTUAL_PACKETS &&
		method->type != TAF_VIRTUAL_MARKUP);
	bool v2r_pending = (virtual_scheduling && (context->virtual_release > 0));
#endif /* TAF_ENABLE_SQS_PULL */

	if (!finished) {
		finished = (method->type == LAST_IAS_SCHEDULER);
	}

#if TAF_ENABLE_SQS_PULL
	//WL_TAFM(method, "finished = %u, virtual_scheduling = %u, v2r_pending = %u, "
	//	"virtual release %u\n",
	//	finished, virtual_scheduling, v2r_pending, context->virtual_release);

	if (virtual_scheduling) {
		if (finished) {
			WL_TAFT(method->taf_info, "vsched phase exit %uus real, %uus "
				"virtual (%u rpkts / %u vpkts)%s pulls %u\n",
				TAF_UNITS_TO_MICROSEC(tid_state->real.released_units),
				TAF_UNITS_TO_MICROSEC(tid_state->total.released_units -
					tid_state->real.released_units),
				context->actual_release, context->virtual_release,
				v2r_pending ? ", TAF_VIRTUAL_MARKUP" : "",
				method->taf_info->total_pull_requests);

			tid_state->total.released_units = tid_state->real.released_units;

			if (v2r_pending) {
				taf_method_info_t *vmarkup = taf_get_method_info(method->taf_info,
					TAF_VIRTUAL_MARKUP);
				*vmarkup->ready_to_schedule = ~0;
			}
		}
		if (v2r_pending) {
			return finished;
		}
	} else if (method->type == TAF_VIRTUAL_MARKUP && finished) {
		TAF_ASSERT(!v2r_pending);
		*method->ready_to_schedule = 0;
	}
#endif /* TAF_ENABLE_SQS_PULL */

	TAF_ASSERT(tid_state->total.released_units == tid_state->real.released_units);

	if (finished && (tid_state->real.released_units > 0)) {
		wlc_taf_info_t* taf_info = method->taf_info;
		taf_ias_group_info_t* ias_info = TAF_IAS_GROUP_INFO(taf_info);
		uint32 low_units = TAF_MICROSEC_TO_UNITS(tid_state->low[IAS_INDEXM(method)]);
		uint32 high_units = TAF_MICROSEC_TO_UNITS(tid_state->high[IAS_INDEXM(method)]);
		uint32 duration;

		if (tid_state->real.released_units < high_units) {
			tid_state->data.uflow_high++;
			tid_state->debug.uflow_high++;
		} else {
			tid_state->data.target_high++;
			tid_state->debug.target_high++;
		}

		tid_state->debug.average_high += tid_state->real.released_units;
		tid_state->debug.prev_release_time = now_time;

		tid_state->resched_index = context->public.ias.index;

		if (tid_state->real.released_units > low_units) {
			tid_state->resched_units = tid_state->real.released_units - low_units;
			tid_state->data.target_low++;
			tid_state->debug.target_low++;
		}
		else {
			tid_state->resched_units = 0;
			tid_state->data.uflow_low++;
			tid_state->debug.uflow_low++;
		}

		duration = (now_time - tid_state->sched_start);
		tid_state->debug.sched_duration += duration;

		if (duration > tid_state->debug.max_duration) {
			tid_state->debug.max_duration = duration;
		}

		tid_state->sched_start = 0;

		if (tid_state->resched_units > 0) {
			bool suppressed_star_packet = FALSE;
#if !TAF_ENABLE_SQS_PULL
			taf_method_info_t *scheduler = method;

#else
			taf_method_info_t *scheduler = taf_get_method_info(method->taf_info,
				TAF_SCHEDULER_START);
#endif /* !TAF_ENABLE_SQS_PULL */

			if ((tid_state->resched_index_suppr ==
					tid_state->resched_index) &&
				(tid_state->cumul_units[tid_state->resched_index] >=
					tid_state->resched_units)) {

				suppressed_star_packet = TRUE;
			}

			if (suppressed_star_packet) {
				WL_TAFT(taf_info, "final exit %uus scheduled trigger "
					"suppressed (prev rel is %u, "
					"in transit %u), sched dur %uus, r_t_s 0x%x\n\n",
					TAF_UNITS_TO_MICROSEC(tid_state->real.released_units),
					tid_state->debug.time_delta,
					TXPKTPENDTOT(TAF_WLCT(taf_info)),
					duration, *scheduler->ready_to_schedule);
				tid_state->trigger = TAF_TRIGGER_NONE;
			} else {
				if (TAF_UNITED_ORDER(taf_info)) {
					*scheduler->ready_to_schedule = 0;
				} else {
					*scheduler->ready_to_schedule &= ~(1 << context->tid);
				}

				WL_TAFT(taf_info, "final exit %uus scheduled "
					"trig %u:%uus (prev rel is %u, in transit "
					"%u), sched dur %uus, r_t_s 0x%x\n\n",
					TAF_UNITS_TO_MICROSEC(tid_state->real.released_units),
					tid_state->resched_index,
					TAF_UNITS_TO_MICROSEC(tid_state->resched_units),
					tid_state->debug.time_delta,
					TXPKTPENDTOT(TAF_WLCT(taf_info)),
					duration, *scheduler->ready_to_schedule);
				tid_state->trigger = TAF_TRIGGER_STAR_THRESHOLD;
			}

			/* mask index with '3' because the pkt tag only has 2 bits */
			ias_info->index = (ias_info->index + 1) & 3;
		} else {
			WL_TAFT(taf_info, "final exit %uus scheduled, immed "
				"retrig (%u) (prev rel is %u, in transit %u), "
				"sched dur %uus, r_t_s 0x%x\n\n",
				TAF_UNITS_TO_MICROSEC(tid_state->real.released_units),
				tid_state->resched_index,
				tid_state->debug.time_delta, TXPKTPENDTOT(TAF_WLCT(taf_info)),
				duration, *method->ready_to_schedule);
			tid_state->trigger = TAF_TRIGGER_IMMEDIATE;
		}

		tid_state->real.released_units = 0;
		tid_state->total.released_units = 0;
		taf_info->release_count ++;
	}
	if (finished) {
		tid_state->star_packet_received = FALSE;
	}
	return finished;
}

#if TAF_ENABLE_SQS_PULL
/* this is to PUSH all the virtual packets previously PULLED which have become real;
 * this is a 'hidden' IAS scheduler dedicated to v2r completion processing
 */
static bool taf_ias_markup(wlc_taf_info_t *taf_info, taf_release_context_t *context,
	void *scheduler_context)
{
	taf_method_info_t *method = scheduler_context;
	int tid = context->tid;
	bool v2r_event = (context->tid == TAF_SQS_V2R_COMPLETE_TID);
	uint32 now_time = taf_timestamp(TAF_WLCT(taf_info));
	uint32 prev_time;
	taf_schedule_tid_state_t *tid_state;
	taf_ias_group_info_t* ias_info;
	taf_scb_cubby_t* scb_taf = v2r_event ? NULL : *SCB_TAF_CUBBY_PTR(taf_info, context->scb);

	if ((taf_info->op_state != TAF_MARKUP_REAL_PACKETS) ||
			(context->op_state != TAF_MARKUP_REAL_PACKETS)) {
		WL_TAFM(method, "NOT IN MARKUP PHASE\n");
		TAF_ASSERT(0);
		return FALSE;
	}

	if (!v2r_event && scb_taf &&
			(((scb_taf->info.pkt_pull_map & (1 << tid)) == 0) ||
			(scb_taf->info.pkt_pull_dequeue == 0))) {

		WL_TAFM(method, MACF" tid %u is not set for markup (0x%u) or no new dequeued "
			"packets (%u) %s\n",
			TAF_ETHERC(scb_taf), tid, scb_taf->info.pkt_pull_map,
			scb_taf->info.pkt_pull_dequeue,
			scb_taf->info.ps_mode ? "in PS mode":"- exit");

		if (!scb_taf->info.ps_mode) {
			return TRUE;
		}
	}

	tid_state = taf_get_tid_state(taf_info, TAF_TID(tid));
	ias_info = TAF_IAS_GROUP_INFO(taf_info);
	context->public.ias.index = ias_info->index;

	if (!v2r_event && scb_taf) {
		taf_ias_sched_send(method, scb_taf, tid, tid_state, context,
			TAF_SCHEDULE_REAL_PACKETS);

		if ((scb_taf->info.pkt_pull_request[tid] == 0) &&
			(scb_taf->info.pkt_pull_map & (1 << tid))) {

			scb_taf->info.pkt_pull_map &= ~(1 << tid);

			if (taf_info->total_pull_requests > 0) {
				taf_info->total_pull_requests--;
			} else {
				WL_TAFM(method, "total_pull_requests 0!\n");
			}
		}
		scb_taf->info.pkt_pull_dequeue = 0;

		if (!scb_taf->info.ps_mode) {
			taf_ias_sched_update(scb_taf->method, scb_taf, tid, tid_state, context);
		}
		WL_TAFM(method, "%u pulls outstanding (actual release %u)\n",
			taf_info->total_pull_requests, context->actual_release);
	} else {
		WL_TAFM(method, "async v2r completion\n");
	}

	prev_time = now_time;
	now_time = taf_timestamp(TAF_WLCT(taf_info));
	ias_info->cpu_time += (now_time - prev_time);

	if (wlc_taf_marked_up(taf_info)) {
		tid_state->debug.time_delta = now_time - tid_state->debug.prev_release_time;

		return taf_ias_completed(method, context, tid_state, TRUE, now_time,
			TAF_SCHEDULE_REAL_PACKETS);
	}
	return TRUE;
}
#endif /* TAF_ENABLE_SQS_PULL */

static bool BCMFASTPATH
taf_ias_schedule(wlc_taf_info_t *taf_info, taf_release_context_t *context,
	void *scheduler_context)
{
	taf_method_info_t *method = scheduler_context;
	bool finished = FALSE;
	taf_ias_group_info_t* ias_info;
	taf_schedule_tid_state_t *tid_state;
	uint32 now_time;
	uint32 prev_time;

#if TAF_ENABLE_SQS_PULL
	/* packet markup is handle only by taf_ias_markup */
	TAF_ASSERT(taf_info->op_state != TAF_MARKUP_REAL_PACKETS);
	TAF_ASSERT(context->op_state != TAF_MARKUP_REAL_PACKETS);

	if (context->op_state == TAF_SCHEDULE_VIRTUAL_PACKETS) {
		TAF_ASSERT(taf_info->op_state == TAF_SCHEDULE_VIRTUAL_PACKETS);
	}
#endif /* TAF_ENABLE_SQS_PULL */

	if (!method->ias->tid_active && method->type != LAST_IAS_SCHEDULER) {
		return FALSE;
	}

	now_time = taf_timestamp(TAF_WLCT(taf_info));
	ias_info = TAF_IAS_GROUP_INFO(taf_info);

	if (method->ias->data_block) {
#ifdef TAF_DBG
		if (ias_info->data_block_start == 0) {
			ias_info->data_block_start = now_time;
			ias_info->data_block_prev_in_transit = TXPKTPENDTOT(TAF_WLCT(taf_info));
			WL_TAFM(method, "exit due to data_block (START)\n");
		}
		if (TXPKTPENDTOT(TAF_WLCT(taf_info)) != ias_info->data_block_prev_in_transit) {
			ias_info->data_block_prev_in_transit = TXPKTPENDTOT(TAF_WLCT(taf_info));
			WL_TAFM(method, "exit due to data_block (%uus)\n",
				(now_time - ias_info->data_block_start));
		}
#endif /* TAF_DBG */
		return TRUE;
	}
#ifdef TAF_DBG
	else if (ias_info->data_block_start) {
		ias_info->data_block_total += now_time - ias_info->data_block_start;
		ias_info->data_block_start = 0;
	}
#endif /* TAF_DBG */

	if (context->op_state == TAF_SCHEDULE_REAL_PACKETS) {
		TAF_ASSERT(context->tid >= 0 && context->tid < TAF_NUM_TIDSTATE);
	}

	tid_state = taf_get_tid_state(taf_info, TAF_TID(context->tid));
	context->public.how = TAF_RELEASE_LIKE_IAS;
	context->public.ias.index = ias_info->index;
	tid_state->cumul_units[ias_info->index] = 0;
	tid_state->was_reset = 0;

	if (tid_state->sched_start == 0) {
		tid_state->sched_start = now_time;
	}
	taf_prepare_list(method, now_time);

	if (TAF_UNITED_ORDER(taf_info)) {
		finished = taf_ias_schedule_all_tid(method, tid_state, 0, NUMPRIO - 1,
			context);
	} else if (method->ias->tid_active & (1 << context->tid)) {
		finished = taf_ias_schedule_all_scb(method, tid_state, context->tid, context);
	}

	prev_time = now_time;
	now_time = taf_timestamp(TAF_WLCT(taf_info));
	tid_state->debug.time_delta = now_time - tid_state->debug.prev_release_time;
	ias_info->cpu_time += (now_time - prev_time);

	return taf_ias_completed(method, context, tid_state, finished, now_time,
		taf_info->op_state);
}

int BCMATTACHFN(wlc_taf_ias_method_detach)(void* context)
{
	taf_ias_container_t* container = context;
	taf_method_info_t* method = container ? &container->method : NULL;

	TAF_ASSERT((void*)method == (void*)container);

	if (container) {
		wlc_taf_info_t* taf_info = method->taf_info;

		TAF_ASSERT(taf_info->group_use_count[method->group] > 0);
		taf_info->group_use_count[method->group]--;

		if (taf_info->group_use_count[method->group] == 0) {
			taf_ias_group_info_t* ias_info = TAF_IAS_GROUP_INFO(taf_info);

			if (ias_info) {
				MFREE(TAF_WLCT(taf_info)->pub->osh, ias_info, sizeof(*ias_info));
			}
			taf_info->group_context[method->group] = NULL;
		}
		MFREE(TAF_WLCT(taf_info)->pub->osh, container, sizeof(*container));
	} else {
		TAF_ASSERT(0);
	}
	return BCME_OK;
}

static int taf_ias_watchdog(void* handle)
{
	taf_method_info_t* method = handle;
	taf_list_t* list = method->list;
	uint32 count = 0;

	while (list) {
		count++;

		/* for current purposes, we can stop here */
		break;
		list = list->next;
	}
	if (count == 0 && method->ias->tid_active) {
		method->ias->tid_active = 0;
		WL_TAFM(method, "no links active\n");
	}
	return BCME_OK;
}

static const taf_scheduler_fn_t taf_ias_funcs = {
	taf_ias_schedule,       /* scheduler_fn    */
	taf_ias_watchdog,       /* watchdog_fn     */
	taf_ias_dump,           /* dump_fn         */
	taf_ias_link_state,     /* linkstate_fn    */
	taf_ias_scb_state,      /* scbstate_fn     */
	NULL,                   /* bssstate_fn     */
	taf_ias_rate_override,  /* rateoverride_fn */
	taf_ias_iovar,          /* iovar_fn        */
	taf_ias_tx_status,      /* txstat_fn       */
	taf_ias_sched_state     /* schedstate_fn   */
};

#if TAF_ENABLE_SQS_PULL
static const taf_scheduler_fn_t taf_ias_markup_funcs = {
	taf_ias_markup,         /* scheduler_fn    */
	NULL,                   /* watchdog_fn     */
	NULL,                   /* dump_fn         */
	NULL,                   /* linkstate_fn    */
	NULL,                   /* scbstate_fn     */
	NULL,                   /* bssstate_fn     */
	NULL,                   /* rateoverride_fn */
	taf_ias_iovar,          /* iovar_fn        */
	NULL,                   /* txstat_fn       */
	taf_ias_markup_sched_state /* schedstate_fn */
};
#endif /* TAF_ENABLE_SQS_PULL */

static const char* taf_ias_name[NUM_IAS_SCHEDULERS] = {
#if TAF_ENABLE_SQS_PULL
	"markup",
#endif /* TAF_ENABLE_SQS_PULL */
	"ebos", "prr", "atos", "atos2"};

#if defined(TAF_DBG)
static const char* taf_ias_dump_name[NUM_IAS_SCHEDULERS] = {
#if TAF_ENABLE_SQS_PULL
	"taf_markup",
#endif /* TAF_ENABLE_SQS_PULL */
	"taf_ebos", "taf_prr", "taf_atos", "taf_atos2"};
#endif /* TAF_DBG */

void* BCMATTACHFN(wlc_taf_ias_method_attach)(wlc_taf_info_t *taf_info, taf_scheduler_kind type)
{
	taf_ias_container_t* container;
	taf_method_info_t* method;
	taf_ias_group_info_t* ias_info = NULL;
	taf_source_type_t source;

	TAF_ASSERT(TAF_TYPE_IS_IAS(type));

	container = (taf_ias_container_t*) MALLOCZ(TAF_WLCT(taf_info)->pub->osh,
		sizeof(*container));

	if (container == NULL) {
		goto exitfail;
	}
	method = &container->method;
	TAF_ASSERT((void*)method == (void*)container);

	method->ias = &container->ias;

	method->taf_info = taf_info;
	method->type = type;
	method->ordering = TAF_LIST_SCORE_MINIMUM;
	method->scheme = TAF_ORDERED_PULL_SCHEDULING;

	method->group = TAF_SCHEDULER_IAS_METHOD;
#if TAF_ENABLE_SQS_PULL
	if (type == TAF_VIRTUAL_MARKUP) {
		method->funcs = taf_ias_markup_funcs;
	} else
#endif /* TAF_ENABLE_SQS_PULL */
	{
		method->funcs = taf_ias_funcs;
	}
	method->name = taf_ias_name[IAS_INDEX(type)];

	method->score_init = 0;

#ifdef TAF_DBG
	if (method->funcs.dump_fn) {
		method->dump_name = taf_ias_dump_name[IAS_INDEX(type)];
		wlc_dump_register(TAF_WLCT(taf_info)->pub, method->dump_name, method->funcs.dump_fn,
			method);
	}
#endif /* TAF_DBG */

	/*
	 * done once only (first time) even though this function might be called several times
	 * for each scheduler sub-type (EBOS, PRR, ATOS, ATOS2....)
	 */
	TAF_ASSERT(taf_info->group_use_count[TAF_SCHEDULER_IAS_METHOD] == IAS_INDEX(type));

	if (IAS_INDEX(type) == 0) {
		ias_info = (taf_ias_group_info_t*)MALLOCZ(TAF_WLCT(taf_info)->pub->osh,
			sizeof(*ias_info));
		if (ias_info == NULL) {
			goto exitfail;
		}
		taf_info->group_context[TAF_SCHEDULER_IAS_METHOD] = ias_info;
	} else {
		ias_info = TAF_IAS_GROUP_INFO(taf_info);
		TAF_ASSERT(ias_info);
	}
	taf_info->group_use_count[TAF_SCHEDULER_IAS_METHOD] ++;

	taf_ias_set_coeff(TAF_COEFF_IAS_DEF, &method->ias->coeff);

#if TAF_ENABLE_SQS_PULL
#if TAF_ENABLE_NAR
	method->ias->pre_rel_limit[TAF_SOURCE_NAR] = 1;
#endif /* TAF_ENABLE_NAR */
#if TAF_ENABLE_AMPDU
	method->ias->pre_rel_limit[TAF_SOURCE_AMPDU] = 16;
#endif /* TAF_ENABLE_AMPDU */
	method->ias->pre_rel_limit[TAF_SOURCE_HOST_SQS] = 0;
	/* with SQS, margin is the amount of extra pkts to request beyond estimate */
	method->ias->margin = 4;
#endif /* TAF_ENABLE_SQS_PULL */

	method->ias->release_limit = 0;

	switch (type) {
		case TAF_EBOS :
			method->ordering = TAF_LIST_SCORE_FIXED_INIT_MINIMUM;
			method->score_init = 1;
			method->ias->high = TAF_TIME_HIGH_DEFAULT;
			method->ias->low = TAF_TIME_LOW_DEFAULT;
			method->ias->opt_aggp_limit = 0;
			method->ias->score_weights = NULL;
			break;

		case TAF_PSEUDO_RR :
			method->ias->high = TAF_TIME_HIGH_DEFAULT;
			method->ias->low = TAF_TIME_LOW_DEFAULT;
			method->ias->opt_aggp_limit = 63;
			method->ias->score_weights = taf_tid_score_weights;
			break;
#if TAF_ENABLE_SQS_PULL
		case TAF_VIRTUAL_MARKUP:
			/* fall through */
#endif /* TAF_ENABLE_SQS_PULL */
		case TAF_ATOS:
			method->ias->high = TAF_TIME_ATOS_HIGH_DEFAULT;
			method->ias->low = TAF_TIME_ATOS_LOW_DEFAULT;
			method->ias->opt_aggp_limit = 63;
			method->ias->score_weights = taf_tid_score_weights;
			break;

		case TAF_ATOS2:
			method->ias->high = TAF_TIME_ATOS2_HIGH_DEFAULT;
			method->ias->low = TAF_TIME_ATOS2_LOW_DEFAULT;
			method->ias->opt_aggp_limit = 31;
			method->ias->score_weights = taf_tid_score_weights;
			break;
		default:
			MFREE(TAF_WLCT(taf_info)->pub->osh, method, sizeof(*method));
			TAF_ASSERT(0);
			WL_ERROR(("wl%u %s: unknown taf scheduler type %u\n",
				WLCWLUNIT(TAF_WLCT(taf_info)), __FUNCTION__, type));
			return NULL;
	}
	taf_ias_time_settings_sync(method);

	if (method->type == taf_info->default_scheduler) {
		taf_info->default_score = method->score_init;
	}
#if TAF_ENABLE_SQS_PULL
	if (type == TAF_VIRTUAL_MARKUP) {
		method->ready_to_schedule = &ias_info->ias_ready_to_schedule[IAS_INDEX(type)];
		*method->ready_to_schedule = 0;
	} else
#endif /* TAF_ENABLE_SQS_PULL */
	{
		method->ready_to_schedule =
			&ias_info->ias_ready_to_schedule[IAS_INDEX(LAST_IAS_SCHEDULER)];
		*method->ready_to_schedule = ~0;
	}

	/* this loops over AMPDU (and NAR when implented) */
	for (source = 0; source < TAF_NUM_SCHED_SOURCES; source ++) {
		TAF_ASSERT(taf_info->source_handle_p[source]);
		TAF_ASSERT(taf_info->funcs[source].scb_h_fn);
		TAF_ASSERT(taf_info->funcs[source].tid_h_fn);
		TAF_ASSERT(taf_info->funcs[source].pktqlen_fn);
	}
	return container;
exitfail:
	if (container) {
		MFREE(TAF_WLCT(taf_info)->pub->osh, container, sizeof(*container));
	}
	WL_ERROR(("wl%u %s: memory alloc fail\n", WLCWLUNIT(TAF_WLCT(taf_info)), __FUNCTION__));
	return NULL;
}

int taf_ias_dump(void *handle, struct bcmstrbuf *b)
{
	taf_method_info_t* method = handle;
	wlc_taf_info_t *taf_info = method->taf_info;
	taf_ias_group_info_t* ias_info = TAF_IAS_GROUP_INFO(taf_info);
	uint32 prev_time = ias_info->cpu_elapsed_time;
	uint32 cpu_norm = 0;
	int tid_index = 0;
	taf_traffic_map_t tidmap = 0;
#ifdef TAF_DBG
	taf_list_t* list = method->list;
	const uint32 max_time = 9999999;
	int idx = 0;
#endif /* TAF_DBG */

	if (!taf_info->enabled) {
		bcm_bprintf(b, "taf must be enabled first\n");
		return BCME_OK;
	}
	TAF_ASSERT(TAF_TYPE_IS_IAS(method->type));

	ias_info->cpu_elapsed_time = taf_timestamp(TAF_WLCT(taf_info));

	if (prev_time != 0) {
		uint32 elapsed = ias_info->cpu_elapsed_time - prev_time;
		elapsed /= 10000;
		if (elapsed) {
			cpu_norm = (ias_info->cpu_time + (elapsed >> 1)) / elapsed;
		}
	}
	bcm_bprintf(b, "%s count %d, high = %uus, low = %uus, cpu time = %uus, "
		    "cpu_norm = %u/10000, r_t_s 0x%x\n",
		    TAF_SCHED_NAME(method), method->counter, method->ias->high, method->ias->low,
		    ias_info->cpu_time, cpu_norm, *method->ready_to_schedule);

	ias_info->cpu_time = 0;
	method->counter = 0;

#ifdef TAF_DBG
	/* Dump all SCBs in our special order. */
	bcm_bprintf(b, "Stations list (scheduling as %s)\n",
		wlc_taf_ordering_name[taf_info->ordering]);
	bcm_bprintf(b, "Idx %-17s %3s %8s %8s %8s %8s %8s %8s %8s %8s "
			"%8s %8s %8s\n", "Mac Address", "TID", "Ready", "TimeRel", "RelFrCnt",
			"RelPCnt", "AveFrSze", "AvePkSze", "AveDelay", "MaxDelay", "Emptied",
			"SkipPS", "SkipDB");

	while (list) {
		taf_scb_cubby_t *scb_taf = list->scb_taf;
		taf_scheduler_scb_stats_t* scb_stats = &scb_taf->info.scb_stats;
		taf_traffic_map_t scb_tidmap = 0;

		TAF_ASSERT(scb_taf->method == method);

		for (tid_index = 0; tid_index < TAF_NUM_TIDSTATE; tid_index++) {
			uint32 tid = taf_tid_service_order[tid_index];
			taf_ias_sched_data_t* ias_sched = TAF_IAS_TID_STATE(scb_taf, tid);
			taf_ias_sched_scbtid_stats_t* tstats;
			uint32 rel_frc;
			uint32 ave_delay;
			uint32 max_delay;

			if (!ias_sched) {
				continue;
			}
			tstats = &ias_sched->scbtidstats;

			if (!tstats->debug.ready) {
				continue;
			}

			rel_frc = tstats->debug.release_frcount ?
				tstats->debug.release_frcount : 1;
			ave_delay = tstats->debug.did_rel_delta/rel_frc;
			max_delay = tstats->debug.max_did_rel_delta;

			if (!scb_tidmap) {
				if (tidmap) {
					bcm_bprintf(b, "\n");
				}
				bcm_bprintf(b, "%3d "MACF" %3u ", idx,
						TAF_ETHERC(scb_taf), tid);
			}
			else {
				bcm_bprintf(b, "                      %3u ", tid);
			}
			scb_tidmap |= (1 << tid);

			if (ave_delay > max_time) {
				ave_delay = max_time;
			}
			if (max_delay > max_time) {
				max_delay = max_time;
			}

			bcm_bprintf(b, "%8u %8u %8u %8u %8u %8u %8u %8u %8u %8u %8u\n",
				tstats->debug.ready,
				tstats->debug.release_time/1000,
				tstats->debug.release_frcount,
				tstats->debug.release_pcount,
				(tstats->debug.release_pcount +
				(rel_frc/2))/ rel_frc,
				tstats->debug.release_pkt_size_mean,
				ave_delay, max_delay,
				tstats->debug.emptied,
#if TAF_ENABLE_SQS_PULL
				0,
#else
				scb_stats->ias.debug.skip_ps,
#endif // endif
				scb_stats->ias.debug.skip_data_block);

			memset(&tstats->debug, 0, sizeof(tstats->debug));
		}
		tidmap |= scb_tidmap;
		list = list->next;
		idx++;
	}
#endif /* TAF_DBG */
	bcm_bprintf(b, "---\n");
	for (tid_index = 0; tid_index < TAF_NUM_TIDSTATE; tid_index++) {
		taf_schedule_tid_state_t* tidstate = NULL;
		uint32 total_count;
		uint32 ave_high = 0;
		uint32 ave_dur = 0;

		if (TAF_PARALLEL_ORDER(taf_info)) {
			int tid = taf_tid_service_order[tid_index];

			if ((tidmap & (1 << tid)) == 0) {
				continue;
			}
			tidstate = taf_get_tid_state(taf_info, tid);

			total_count = tidstate->debug.target_high + tidstate->debug.uflow_high;
			if (total_count > 0) {
				ave_high = tidstate->debug.average_high / total_count;
				ave_dur = tidstate->debug.sched_duration / total_count;
			}

			bcm_bprintf(b, "tid %u  target high %6u / uflow high %6u,  ave_high %6uus; "
				    "target low %6u / uflow low %6u\nave duration "
				    "%5u, max duration %5u\n",
				    taf_tid_service_order[tid_index],
				    tidstate->debug.target_high, tidstate->debug.uflow_high,
				    TAF_UNITS_TO_MICROSEC(ave_high),
				    tidstate->debug.target_low, tidstate->debug.uflow_low,
				    ave_dur, tidstate->debug.max_duration);
		}
		if (TAF_UNITED_ORDER(taf_info)) {
			tidstate = taf_get_tid_state(taf_info, TAF_TID(TAF_DEFAULT_UNIFIED_TID));

			total_count = tidstate->debug.target_high + tidstate->debug.uflow_high;
			if (total_count > 0) {
				ave_high = tidstate->debug.average_high / total_count;
				ave_dur = tidstate->debug.sched_duration / total_count;
			}

			bcm_bprintf(b, "unified target high %6u / uflow high %6u,  ave_high %6uus; "
				    "target low %6u / uflow low %6u\nave sched "
				    "duration %5u, max sched duration %5u\n",
				    tidstate->debug.target_high, tidstate->debug.uflow_high,
				    TAF_UNITS_TO_MICROSEC(ave_high),
				    tidstate->debug.target_low, tidstate->debug.uflow_low,
				    ave_dur, tidstate->debug.max_duration);

			tid_index = TAF_NUM_TIDSTATE; /* break the loop */
		}
		TAF_ASSERT(tidstate);
		memset(&tidstate->debug, 0, sizeof(tidstate->debug));
	}
#ifdef TAF_DBG
	for (list = method->list; list; list = list->next) {
		taf_scheduler_scb_stats_t* scb_stats =  &list->scb_taf->info.scb_stats;
		memset(&scb_stats->ias.debug, 0, sizeof(scb_stats->ias.debug));
	}
#endif /* TAF_DBG */
	return BCME_OK;
}

static void taf_ias_tid_reset(taf_method_info_t* method, int tid)
{
	taf_schedule_tid_state_t* tidstate = taf_get_tid_state(method->taf_info, TAF_TID(tid));
#ifdef TAF_DBG
	uint32 prev_rel_units = tidstate->real.released_units;
#endif /* TAF_DBG */
	taf_ias_group_info_t* ias_info = TAF_IAS_GROUP_INFO(method->taf_info);
	if (tidstate->was_reset) {
		return;
	}
	tidstate->trigger = TAF_TRIGGER_NONE;
	tidstate->real.released_units = 0;
	tidstate->total.released_units = 0;
	tidstate->star_packet_received = FALSE;
	tidstate->resched_index_suppr = -1;

	tidstate->cumul_units[0] = 0;
	tidstate->cumul_units[1] = 0;
	tidstate->cumul_units[2] = 0;
	tidstate->cumul_units[3] = 0;

	tidstate->resched_units = 0;
	tidstate->resched_index = ias_info->index;
	tidstate->high[IAS_INDEXM(method)] = method->ias->high;
	tidstate->low[IAS_INDEXM(method)] = method->ias->low;
	memset(&tidstate->data, 0, sizeof(tidstate->data));
	memset(&tidstate->debug, 0, sizeof(tidstate->debug));

	tidstate->was_reset ++;

	if (TAF_PARALLEL_ORDER(method->taf_info)) {
		*method->ready_to_schedule |= (1 << tid);
	} else {
		*method->ready_to_schedule = ~(0);
	}

	WL_TAFM(method, "tid %u reset (%u) r_t_s 0x%x\n", tid, prev_rel_units,
		*method->ready_to_schedule);
}

static INLINE void taf_upd_ts2_scb_flag(taf_scb_cubby_t *scb_taf)
{
	struct scb* scb = scb_taf->scb;

	scb->flags3 &= ~(SCB3_TS_MASK);

	switch (scb_taf->method->type) {
		case TAF_EBOS:
			scb->flags3 |= SCB3_TS_EBOS;
			break;
		case TAF_ATOS:
			scb->flags3 |= SCB3_TS_ATOS;
			break;
		case TAF_ATOS2:
			scb->flags3 |= SCB3_TS_ATOS2;
			break;
		default:
			break;
	}
}

static int taf_ias_sched_reset(taf_method_info_t* method, taf_scb_cubby_t *scb_taf, int tid,
	taf_source_type_t s_idx)
{
	wlc_taf_info_t* taf_info = method->taf_info;
	taf_ias_sched_data_t *ias_sched = TAF_IAS_TID_STATE(scb_taf, tid);

	TAF_ASSERT(ias_sched);

	if (!ias_sched) {
		return BCME_ERROR;
	}

	if (ias_sched->handle[s_idx].used) {
		void* src_h = *(taf_info->source_handle_p[s_idx]);
		TAF_ASSERT(src_h);

		WL_TAFM(method, "reset %s for "MACF" tid %u\n", TAF_SOURCE_NAME(s_idx),
			TAF_ETHERC(scb_taf), tid);

		ias_sched->handle[s_idx].scbh =
			taf_info->funcs[s_idx].scb_h_fn(src_h, scb_taf->scb);
		ias_sched->handle[s_idx].tidh =
			taf_info->funcs[s_idx].tid_h_fn(ias_sched->handle[s_idx].scbh, tid);

		if (TAF_PARALLEL_ORDER(taf_info)) {
			*method->ready_to_schedule |= (1 << tid);
		} else {
			*method->ready_to_schedule = ~(0);
		}
		method->ias->tid_active |= (1 << tid);
		scb_taf->info.traffic_map[s_idx] |= (1 << tid);

		ias_sched->scbtidstats.data.calc_timestamp = scb_taf->timestamp;
		/* initial bias in packet averaging initialisation */
		ias_sched->scbtidstats.data.release_pkt_size_mean = TAF_PKT_SIZE_DEFAULT;
		ias_sched->scbtidstats.data.total_scaled_pcount =
			(4 << TAF_PCOUNT_SCALE_SHIFT);

		WL_TAFM(method, "ias_sched for "MACF" tid %u source %s reset (%u,%u) "
			"r_t_s 0x%x\n",  TAF_ETHERC(scb_taf), tid,
			TAF_SOURCE_NAME(s_idx), ias_sched->handle[s_idx].used,
			ias_sched->use_amsdu[s_idx], *method->ready_to_schedule);
	} else {
		ias_sched->handle[s_idx].scbh = NULL;
		ias_sched->handle[s_idx].tidh = NULL;

		WL_TAFM(method, "ias_sched for "MACF" tid %u source %s unused r_t_s \n"
			"0x%x\n", TAF_ETHERC(scb_taf), tid,
			TAF_SOURCE_NAME(s_idx), *method->ready_to_schedule);
	}
	return BCME_OK;
}

static int taf_ias_tid_remove(taf_method_info_t* method, taf_scb_cubby_t *scb_taf, int tid)
{
	wlc_info_t *wlc = TAF_WLCM(method);
	taf_ias_sched_data_t *ias_sched = TAF_IAS_TID_STATE(scb_taf, tid);

	TAF_ASSERT(ias_sched);
	if (ias_sched) {
		MFREE(wlc->pub->osh, ias_sched, sizeof(*ias_sched));
		TAF_CUBBY_TIDINFO(scb_taf, tid) = NULL;
		WL_TAFM(method, "ias_sched for "MACF" tid %u destroyed\n",
			TAF_ETHERC(scb_taf), tid);
	}
	return BCME_OK;
}

static int taf_ias_sched_init(taf_method_info_t* method, taf_scb_cubby_t *scb_taf, int tid)
{
	wlc_info_t *wlc = TAF_WLCM(method);
	taf_ias_sched_data_t *ias_sched;

	TAF_ASSERT(TAF_CUBBY_TIDINFO(scb_taf, tid) == NULL);

	ias_sched = (taf_ias_sched_data_t*) MALLOCZ(wlc->pub->osh, sizeof(*ias_sched));
	TAF_ASSERT(ias_sched);

	if (!ias_sched) {
		return BCME_NOMEM;
	}
	TAF_CUBBY_TIDINFO(scb_taf, tid) = ias_sched;

	WL_TAFM(method, "ias_sched for "MACF" tid %u created\n", TAF_ETHERC(scb_taf), tid);

	return BCME_OK;
}

bool taf_ias_link_state(void *handle, struct scb* scb, int tid, taf_source_type_t s_idx,
	taf_link_state_t state)
{
	taf_method_info_t* method = handle;
	taf_scb_cubby_t* scb_taf = *SCB_TAF_CUBBY_PTR(method->taf_info, scb);
	int status = BCME_OK;
	taf_ias_sched_data_t* ias_sched;

	TAF_ASSERT(scb_taf || (state == TAF_LINKSTATE_CLEAN) || (state == TAF_LINKSTATE_REMOVE));

	if (!scb_taf) {
		return BCME_NOTFOUND;
	}
	ias_sched = TAF_IAS_TID_STATE(scb_taf, tid);

	switch (state) {
		case TAF_LINKSTATE_INIT:
			if (!ias_sched) {
				status = taf_ias_sched_init(method, scb_taf, tid);
			}
			scb_taf->info.linkstate[s_idx][tid] = state;
			break;

		case TAF_LINKSTATE_ACTIVE:
			if (scb_taf->info.linkstate[s_idx][tid] == TAF_LINKSTATE_NONE) {
				if (!ias_sched) {
					status = taf_ias_sched_init(method, scb_taf, tid);
					if (status == BCME_OK) {
						ias_sched = TAF_IAS_TID_STATE(scb_taf, tid);
					}
				}
			}
			if (ias_sched) {
				ias_sched->handle[s_idx].used = TRUE;
				status = taf_ias_sched_reset(method, scb_taf, tid, s_idx);
				WL_TAFM(method, "ias_sched for "MACF" tid %u %s active\n",
					TAF_ETHERS(scb), tid, TAF_SOURCE_NAME(s_idx));
				scb_taf->info.linkstate[s_idx][tid] = state;
			}
			break;

		case TAF_LINKSTATE_REMOVE:
			scb_taf->info.linkstate[s_idx][tid] = TAF_LINKSTATE_NONE;
			if (ias_sched) {
				taf_source_type_t _idx;
				bool allfree = TRUE;

				ias_sched->handle[s_idx].used = FALSE;
				for (_idx = 0; _idx < TAF_NUM_SCHED_SOURCES; _idx++) {

					if (scb_taf->info.linkstate[_idx][tid] !=
							TAF_LINKSTATE_NONE) {
						allfree = FALSE;
					}
				}
				WL_TAFM(method, "%s for %s "MACF" tid %u%s\n",
					taf_link_states_text[state], TAF_SOURCE_NAME(s_idx),
					TAF_ETHERS(scb), tid, allfree ? " all free" : "");

				if (allfree) {
					status = taf_ias_tid_remove(method, scb_taf, tid);
				}
			}
			break;

		case TAF_LINKSTATE_NONE:
			scb_taf->info.linkstate[s_idx][tid] = TAF_LINKSTATE_NONE;
			break;

		case TAF_LINKSTATE_HARD_RESET:
			if (ias_sched) {
				taf_ias_sched_reset(method, scb_taf, tid, s_idx);
				taf_ias_tid_reset(method, tid);
			}
			break;

		case TAF_LINKSTATE_CLEAN:
		case TAF_LINKSTATE_SOFT_RESET:
			if (ias_sched) {
				taf_ias_sched_reset(method, scb_taf, tid, s_idx);
			}
			break;
		case TAF_LINKSTATE_AMSDU_AGG:
		case TAF_LINKSTATE_NOT_ACTIVE:
			WL_TAFM(method, MACF" tid %u, %s TO DO - %s\n", TAF_ETHERS(scb), tid,
				TAF_SOURCE_NAME(s_idx), taf_link_states_text[state]);

			break;

		default:
			status = BCME_ERROR;
			TAF_ASSERT(0);
	}
	/*
	 * return TRUE if we handled it
	 */
	if (status == BCME_OK) {
		return TRUE;
	}
	return FALSE;
}

static int taf_ias_set_source(taf_method_info_t* method, taf_scb_cubby_t* scb_taf,
	taf_source_type_t s_idx, bool set)
{
	if (scb_taf) {
		scb_taf->info.scb_stats.ias.data.use[s_idx] = set;
		if (!set) {
			scb_taf->info.traffic_map[s_idx] = 0;
		}
#if TAF_ENABLE_AMPDU
		if (TAF_SOURCE_IS_AMPDU(s_idx) && set) {
			wlc_taf_info_t* taf_info = method->taf_info;
			void ** source_h = taf_info->source_handle_p[s_idx];
			void * scb_h = taf_info->funcs[s_idx].scb_h_fn(*source_h, scb_taf->scb);
			scb_taf->info.max_pdu = wlc_ampdu_get_taf_max_pdu(scb_h);
		} else {
			scb_taf->info.max_pdu = 0;
		}
#endif // endif
		return BCME_OK;
	}
	return BCME_ERROR;
}

static INLINE uint32 taf_ias_traffic_wait(taf_method_info_t* method, struct scb* scb)
{
	taf_scb_cubby_t* scb_taf = *SCB_TAF_CUBBY_PTR(method->taf_info, scb);
	uint32 pending = 0;
	int tid;
	taf_source_type_t s_idx;

	for (s_idx = 0; s_idx < TAF_NUM_SCHED_SOURCES; s_idx ++) {
		for (tid = 0; tid < TAF_NUMPRIO(s_idx); tid++) {
			taf_ias_sched_data_t* ias_sched = TAF_IAS_TID_STATE(scb_taf, tid);
			uint16 (*pktqlen_fn) (void *, void *) =
				method->taf_info->funcs[s_idx].pktqlen_fn;

			scb_taf->info.traffic_map[s_idx] &= ~(1 << tid);

			if (ias_sched && ias_sched->handle[s_idx].used && pktqlen_fn) {
				uint16 pktq_len;
				pktq_len = pktqlen_fn(ias_sched->handle[s_idx].scbh,
					ias_sched->handle[s_idx].tidh);
				scb_taf->info.traffic_count[s_idx][tid] = pktq_len;
				pending += pktq_len;
				if (pktq_len) {
					scb_taf->info.traffic_map[s_idx] |= (1 << tid);
					scb_taf->info.traffic_available |= (1 << tid);
				}
			}
		}
	}
	WL_TAFM(method, MACF" - %u\n", TAF_ETHERS(scb), pending);
	return pending;
}
static INLINE void taf_ias_scb_ps_mode(taf_method_info_t* method, taf_scb_cubby_t* scb_taf)
{
#if TAF_ENABLE_SQS_PULL
	int tid;
#ifdef TAF_DBG
	char debug[48];
	char * taf_debug = debug;

	if (!scb_taf) {
		return;
	}

	if (scb_taf->info.pkt_pull_map) {
		snprintf(debug, sizeof(debug), ": %u,%u,%u,%u,%u,%u,%u,%u",
			scb_taf->info.pkt_pull_request[0],
			scb_taf->info.pkt_pull_request[1],
			scb_taf->info.pkt_pull_request[2],
			scb_taf->info.pkt_pull_request[3],
			scb_taf->info.pkt_pull_request[4],
			scb_taf->info.pkt_pull_request[5],
			scb_taf->info.pkt_pull_request[6],
			scb_taf->info.pkt_pull_request[7]);
	} else {
		taf_debug = "";
	}

	WL_TAFT(method->taf_info, MACF" PS mode O%s (pkt_pull_map 0x%02x%s)\n",
		TAF_ETHERC(scb_taf), scb_taf->info.ps_mode ? "N" : "FF",
		scb_taf->info.pkt_pull_map, taf_debug);
#endif /* TAF_DBG */
	if (scb_taf->info.ps_mode == 0 && scb_taf->info.pkt_pull_map) {
		for (tid = 0; tid < NUMPRIO; tid ++) {
			if (scb_taf->info.pkt_pull_request[tid]) {
				TAF_ASSERT(scb_taf->info.pkt_pull_map & (1 << tid));
				scb_taf->info.pkt_pull_request[tid] = 0;
			}
		}
		scb_taf->info.pkt_pull_map = 0;
	}
#else
	WL_TAFM(method, MACF" PS mode O%s\n", TAF_ETHERC(scb_taf),
		scb_taf->info.ps_mode ? "N" : "FF");
#endif /* TAF_ENABLE_SQS_PULL */

}

int
taf_ias_scb_state(void *handle, struct scb* scb, void* data, taf_scb_state_t state)
{
	taf_method_info_t* method = handle;
	taf_scb_cubby_t* scb_taf = *SCB_TAF_CUBBY_PTR(method->taf_info, scb);
	int status = BCME_OK;
	taf_source_type_t s_idx;
	int tid;
	bool active;

	switch (state) {

		case TAF_SCBSTATE_INIT:
			WL_TAFM(method, MACF" SCB INIT\n", TAF_ETHERS(scb));
			taf_upd_ts2_scb_flag(scb_taf);
			scb_taf->timestamp = taf_timestamp(TAF_WLCM(method));
			break;

		case TAF_SCBSTATE_EXIT:
			WL_TAFM(method, MACF" SCB EXIT\n", TAF_ETHERS(scb));
			break;

		case TAF_SCBSTATE_RESET:
			if (scb_taf) {
				for (tid = 0; tid < TAF_MAXPRIO; tid++) {
#if TAF_ENABLE_SQS_PULL
					scb_taf->info.pkt_pull_request[tid] = 0;
#endif /* TAF_ENABLE_SQS_PULL */
					for (s_idx = TAF_FIRST_REAL_SOURCE;
						s_idx < TAF_NUM_SCHED_SOURCES; s_idx++) {

						scb_taf->info.traffic_map[s_idx] = 0;
						scb_taf->info.traffic_count[s_idx][tid] = 0;
					}
				}
				scb_taf->info.traffic_available = 0;
				scb_taf->info.ps_mode = SCB_PS(scb);
				scb_taf->timestamp = taf_timestamp(TAF_WLCM(method));
				scb_taf->force = FALSE;
				scb_taf->score = method->score_init;
#if TAF_ENABLE_SQS_PULL
				scb_taf->info.pkt_pull_map = 0;
				scb_taf->info.released_units_limit = 0;
				scb_taf->info.pkt_pull_dequeue = 0;
#endif /* TAF_ENABLE_SQS_PULL */
#ifdef TAF_DBG
				memset(&scb_taf->info.scb_stats.ias.debug, 0,
				       sizeof(scb_taf->info.scb_stats.ias.debug));
#endif // endif
				WL_TAFM(method, MACF" SCB RESET\n", TAF_ETHERS(scb));
			}
			break;

		case TAF_SCBSTATE_SOURCE_ENABLE:
		case TAF_SCBSTATE_SOURCE_DISABLE:
			s_idx = wlc_taf_get_source_index(data);
			if (s_idx != TAF_SOURCE_UNDEFINED) {
				active = (state != TAF_SCBSTATE_SOURCE_DISABLE) ? TRUE : FALSE;
				status = taf_ias_set_source(method, scb_taf, s_idx, active);
				WL_TAFM(method, MACF" %s %s\n", TAF_ETHERS(scb),
					TAF_SOURCE_NAME(s_idx), active ? "ON" : "OFF");
			} else {
				WL_TAFM(method, MACF" %s - TO DO - %s\n", TAF_ETHERS(scb),
					(const char*)data,
					(state != TAF_SCBSTATE_SOURCE_DISABLE) ? "ON" : "OFF");
			}
			break;

		case TAF_SCBSTATE_SOURCE_UPDATE:
			s_idx = wlc_taf_get_source_index(data);
			active = scb_taf->info.scb_stats.ias.data.use[s_idx];
			status = taf_ias_set_source(method, scb_taf, s_idx, active);
			WL_TAFM(method, MACF" %s update\n", TAF_ETHERS(scb),
				TAF_SOURCE_NAME(s_idx));
			break;

		case TAF_SCBSTATE_POWER_SAVE:
			taf_ias_scb_ps_mode(method, scb_taf);
			break;

		case TAF_SCBSTATE_NONE:
			break;

		case TAF_SCBSTATE_GET_TRAFFIC_ACTIVE:
			/* this is a GET query, with "update" being a uint32 pointer to return
			 * pending traffic; typically called from wlc_apps for pvb update
			 */
			TAF_ASSERT(data);
			* (uint32 *) data = taf_ias_traffic_wait(method, scb);
			break;

		case TAF_SCBSTATE_UPDATE_BSSCFG:
		case TAF_SCBSTATE_DWDS:
		case TAF_SCBSTATE_WDS:
		case TAF_SCBSTATE_OFF_CHANNEL:
		case TAF_SCBSTATE_DATA_BLOCK_OTHER:
		case TAF_SCBSTATE_MU_MIMO:
			WL_TAFM(method, MACF" TO DO - %s (%u)\n", TAF_ETHERS(scb),
				taf_scb_states_text[state], state);

			break;
		default:
			TAF_ASSERT(0);
	}
	return status;
}

/* Wrapper function to enable/disable rate overide for the entire driver,
 * called by rate override code
 */
static bool taf_ias_rate_override(void* handle, ratespec_t rspec, wlcband_t *band)
{
	taf_method_info_t* method = handle;
	taf_list_t* list = method->list;
	wlc_info_t* wlc = TAF_WLCM(method);

	TAF_ASSERT(TAF_TYPE_IS_IAS(method->type));

	while (list) {
		taf_scb_cubby_t *scb_taf = list->scb_taf;
		struct scb* scb = scb_taf->scb;

		if (band->bandtype == wlc_scbband(wlc, scb)->bandtype) {
			WL_TAFM(method, MACF" TO DO\n", TAF_ETHERS(scb));
		}
		list = list->next;
	}
	return BCME_OK;
}

static INLINE bool
taf_ias_handle_star(taf_method_info_t* method, taf_scb_cubby_t* scb_taf, int tid, uint16 pkttag,
	uint8 index)
{
	wlc_taf_info_t* taf_info = method->taf_info;
	taf_schedule_tid_state_t* tid_state = taf_get_tid_state(taf_info, TAF_TID(tid));

	if (index != tid_state->resched_index) {
		//WL_TAFM(method, "ignoring tag=%d:%d (wait %u)\n", index,
		//	TAF_PKTTAG_TO_MICROSEC(pkttag), tid_state->resched_index);
		return TRUE;
	}

	tid_state->cumul_units[tid_state->resched_index] += TAF_PKTTAG_TO_UNITS(pkttag);

	if (tid_state->cumul_units[tid_state->resched_index] >= tid_state->resched_units) {
		taf_ias_group_info_t* ias_info = TAF_IAS_GROUP_INFO(taf_info);

		if (tid_state->star_packet_received) {
			return TRUE;
		}
		tid_state->star_packet_received = TRUE;

		if (TAF_PARALLEL_ORDER(taf_info)) {
			*method->ready_to_schedule |= (1 << tid);
		} else {
			*method->ready_to_schedule = ~(0);
		}

		WL_TAFM(method, MACF" cumulative %d/%d  reschedule=%d, complete\n\n",
			TAF_ETHERC(scb_taf), index,
			TAF_UNITS_TO_MICROSEC(tid_state->cumul_units[tid_state->resched_index]),
			TAF_UNITS_TO_MICROSEC(tid_state->resched_units));

		if (tid_state->resched_index_suppr >= 0) {
			WL_TAFT(taf_info, "clear last suppressed tag %u:%u\n",
				tid_state->resched_index_suppr,
				TAF_UNITS_TO_MICROSEC(tid_state->cumul_units[tid_state->
				resched_index_suppr]));
			tid_state->cumul_units[tid_state->resched_index_suppr] = -1;
			tid_state->resched_index_suppr = -1;
		}

		tid_state->resched_index = ias_info->index;
	} else {
		//WL_TAFT(taf_info, "wait tag=%d - %d/%d (%d)\n",
		//	(pkttag), index,
		//	(tid_state->cumul_units[tid_state->resched_index]),
		//	(tid_state->resched_units));
	}

	return TRUE;
}

static INLINE bool taf_ias_pkt_lost(taf_method_info_t* method, taf_scb_cubby_t* scb_taf,
	int tid, void * p, taf_txpkt_state_t status)
{
	taf_schedule_tid_state_t* tid_state = taf_get_tid_state(method->taf_info, TAF_TID(tid));
	uint16 pkttag_index = WLPKTTAG(p)->pktinfo.taf.ias.index;
	uint16 pkttag_units = WLPKTTAG(p)->pktinfo.taf.ias.units;

	tid_state->resched_index_suppr = pkttag_index;

	WL_TAFT(method->taf_info, "%u - %u/%u (%u) %s\n", TAF_PKTTAG_TO_MICROSEC(pkttag_units),
		pkttag_index, TAF_UNITS_TO_MICROSEC(tid_state->cumul_units[pkttag_index]),
		tid_state->resched_index, taf_txpkt_status_text[status]);

	switch (method->type) {
		case TAF_EBOS:
			break;

		case TAF_PSEUDO_RR:
			if (scb_taf->score > 0) {
				scb_taf->score--;
			}
			break;

		case TAF_ATOS:
		case TAF_ATOS2:
		{
			uint32 taf_pkt_time_units = TAF_PKTTAG_TO_UNITS(pkttag_units);
			uint32 taf_pkt_time_score = taf_units_to_score(method, tid,
				taf_pkt_time_units);

			if (scb_taf->score > taf_pkt_time_score) {
				scb_taf->score -= taf_pkt_time_score;
			} else {
				scb_taf->score = 0;
			}
		}
			break;

		default:
			TAF_ASSERT(0);
	}
	return taf_ias_handle_star(method, scb_taf, tid, pkttag_units, pkttag_index);
}

bool taf_ias_tx_status(void * handle, taf_scb_cubby_t* scb_taf, int tid, void * p,
	taf_txpkt_state_t status)
{
	taf_method_info_t* method = handle;
	bool ret = FALSE;
#ifdef TAF_DEBUG_VERBOSE
	static bool dump = FALSE;
#endif // endif

	switch (status) {

		case TAF_TXPKT_STATUS_UPDATE_RETRY_COUNT:
		case TAF_TXPKT_STATUS_UPDATE_PACKET_COUNT:
		case TAF_TXPKT_STATUS_NONE:
		case TAF_TXPKT_STATUS_UPDATE_RATE:
		case TAF_TXPKT_STATUS_SUPPRESSED_QUEUED:
#if !TAF_ENABLE_SQS_PULL
		case TAF_TXPKT_STATUS_SUPPRESSED:
#endif // endif
			ret = TRUE;
			break;

		case TAF_TXPKT_STATUS_PKTFREE_DROP:
			WL_TAFM(method, MACF" tid %d TAF_TXPKT_STATUS_PKTFREE_DROP tag %u/%u:%u\n",
				TAF_ETHERC(scb_taf), tid, WLPKTTAG(p)->pktinfo.taf.ias.tagged,
					WLPKTTAG(p)->pktinfo.taf.ias.index,
					WLPKTTAG(p)->pktinfo.taf.ias.units);
#ifdef TAF_DEBUG_VERBOSE
			if (!dump) {
				taf_memtrace_dump();
				dump = TRUE;
			}
#endif // endif
		case TAF_TXPKT_STATUS_SUPPRESSED_FREE:
#if TAF_ENABLE_SQS_PULL
		case TAF_TXPKT_STATUS_SUPPRESSED:
#endif // endif
			if (WLPKTTAG(p)->pktinfo.taf.ias.tagged == TAF_TAGGED &&
					WLPKTTAG(p)->pktinfo.taf.ias.units <= TAF_PKTTAG_RESERVED) {

				ret = taf_ias_pkt_lost(method, scb_taf, tid, p, status);
				WLPKTTAG(p)->pktinfo.taf.ias.units = TAF_PKTTAG_PROCESSED;
			}
			break;

		case TAF_TXPKT_STATUS_REGMPDU:
		case TAF_TXPKT_STATUS_PKTFREE:
			if (WLPKTTAG(p)->pktinfo.taf.ias.tagged == TAF_TAGGED &&
					WLPKTTAG(p)->pktinfo.taf.ias.units <= TAF_PKTTAG_RESERVED) {

				ret = taf_ias_handle_star(method, scb_taf, tid,
					WLPKTTAG(p)->pktinfo.taf.ias.units,
					WLPKTTAG(p)->pktinfo.taf.ias.index);
				WLPKTTAG(p)->pktinfo.taf.ias.units = TAF_PKTTAG_PROCESSED;
#ifdef TAF_DEBUG_VERBOSE
				dump = FALSE;
#endif // endif
			}
			break;
		default:
			break;
	}
	if (!ret && WLPKTTAG(p)->pktinfo.taf.ias.tagged == TAF_TAGGED &&
			WLPKTTAG(p)->pktinfo.taf.ias.units == TAF_PKTTAG_PROCESSED) {
		ret = TRUE;
	}
	return ret;
}

#if TAF_ENABLE_SQS_PULL
static void taf_ias_markup_sched_state(void * handle, taf_scb_cubby_t * scb_taf, int tid, int count,
	taf_source_type_t s_idx, taf_sched_state_t state)
{
	taf_method_info_t* method = handle;

	switch (state) {
		case TAF_SCHED_STATE_RESET:
			WL_TAFM(method, "RESET\n");
			*method->ready_to_schedule = 0;
			break;

		default:
			break;
	}
}
#endif /* TAF_ENABLE_SQS_PULL */

static void taf_ias_sched_state(void * handle, taf_scb_cubby_t * scb_taf, int tid, int count,
	taf_source_type_t s_idx, taf_sched_state_t state)
{
	taf_method_info_t* method = handle;

	switch (state) {
		case TAF_SCHED_STATE_DATA_BLOCK_FIFO:
			method->ias->data_block = count ? TRUE : FALSE;
			break;

		case TAF_SCHED_STATE_REWIND:
			WL_TAFM(method, MACF" tid %u rewind %u pkts\n", TAF_ETHERC(scb_taf), tid,
				count);
			break;

		case TAF_SCHED_STATE_RESET:
			WL_TAFM(method, "RESET\n");
			*method->ready_to_schedule = ~0;
			break;

		default:
			TAF_ASSERT(0);
	}
}

int
taf_ias_iovar(void *handle, const char* cmd, wl_taf_define_t* result, struct bcmstrbuf* b)
{
	taf_method_info_t* method = handle;
	int status = BCME_UNSUPPORTED;
	const char* local_cmd = cmd;

	if (!TAF_TYPE_IS_IAS(method->type)) {
		return BCME_UNSUPPORTED;
	}
	if (!cmd) {
		return BCME_UNSUPPORTED;
	}
	WL_TAFM(method, "%s\n", cmd);

	if (!strcmp(cmd, "coeff") && (method->type != TAF_EBOS)) {
		int err;
		uint32 coeff = method->ias->coeff.coeff1;
		err = wlc_taf_param(&local_cmd, &coeff, 0, TAF_COEFF_IAS_MAX_CFG, b);
		taf_ias_set_coeff(coeff, &method->ias->coeff);
		result->misc = method->ias->coeff.coeff1;
		return err;
	}
	if (!strcmp(cmd, "low")) {
		int err = wlc_taf_param(&local_cmd, &method->ias->low, 0, method->ias->high, b);
		if (err == TAF_IOVAR_OK_SET) {
			taf_ias_time_settings_sync(method);
		}
		result->misc = method->ias->low;
		return err;
	}
	if (!strcmp(cmd, "high")) {
		int err = wlc_taf_param(&local_cmd, &method->ias->high, method->ias->low,
			TAF_WINDOW_MAX, b);
		if (err == TAF_IOVAR_OK_SET) {
			taf_ias_time_settings_sync(method);
		}
		result->misc = method->ias->high;
		return err;
	}
	if (!strcmp(cmd, "aggp")) {
		int err = wlc_taf_param(&local_cmd, &method->ias->opt_aggp_limit, 0, 63, b);
		result->misc = method->ias->opt_aggp_limit;
		return err;
	}
	if (!strcmp(cmd, "tid_weight")) {
		int err = BCME_BADARG;

		if (method->ias->score_weights == NULL) {
			return err;
		}
		cmd += strlen(cmd) + 1;

		if (*cmd) {
			int tid = bcm_strtoul(cmd, NULL, 0);

			if (tid >= 0 && tid < NUMPRIO) {
				uint32 weight = method->ias->score_weights[tid];

				err = wlc_taf_param(&cmd, &weight, 0, BCM_UINT8_MAX, b);
				if (err == TAF_IOVAR_OK_SET) {
					method->ias->score_weights[tid] = weight;
				}
				result->misc = method->ias->score_weights[tid];
			}
		} else {
			int tid;
			const char* ac[AC_COUNT] = {"BE", "BK", "VI", "VO"};
			bcm_bprintf(b, "tid: (AC) weight(0-255)\n");

			for (tid = 0; tid < NUMPRIO; tid++) {
				bcm_bprintf(b, "  %d: (%s)    %3u\n", tid, ac[WME_PRIO2AC(tid)],
					method->ias->score_weights[tid]);
			}
			err = BCME_OK;
		}
		return err;
	}
	if (!strcmp(cmd, "rel_limit")) {
		int err = wlc_taf_param(&local_cmd, &method->ias->release_limit, 0,
			method->ias->high, b);
		result->misc = method->ias->release_limit;
		return err;
	}
#if TAF_ENABLE_SQS_PULL
	if (!strcmp(cmd, "margin")) {
		int err = wlc_taf_param(&local_cmd, &method->ias->margin, 0, 255, b);
		result->misc = method->ias->margin;
		return err;
	}
#endif /* TAF_ENABLE_SQS_PULL */
	return status;
}

#endif /* WLTAF_IAS */
