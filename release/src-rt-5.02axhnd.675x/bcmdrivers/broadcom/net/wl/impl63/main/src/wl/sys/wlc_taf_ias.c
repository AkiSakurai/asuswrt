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

#include <wlc_taf_cmn.h>

#ifdef WLTAF_IAS
#include <wlc_taf_priv_ias.h>

#include <wlc_stf.h>

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

#if TAF_ENABLE_MU_TX
static uint16 taf_mu_pair[TAF_MAX_MU] = {
	0,
	90,  /* 2 */
	90,  /* 3 */
	90,  /* 4 */
	90,  /* 5 */
	90,  /* 6 */
	90,  /* 7 */
	90   /* 8 */
};
static uint16 taf_mu_mimo_rel_limit[TAF_MAX_MU_MIMO] = {
	0,
	TAF_TIME_HIGH_DEFAULT, /* 2 */
	4000, /* 3 */
	4000  /* 4 */
};
#endif /* TAF_ENABLE_MU_TX */

static INLINE uint32 BCMFASTPATH
taf_ias_est_release_time(taf_scb_cubby_t *scb_taf, taf_ias_sched_data_t* ias_sched, int len,
	taf_list_type_t type)
{
	uint16 est_pkt_size;
	uint32 pktbytes;
#if TAF_ENABLE_UL
	taf_rspec_index_t rindex = (type == TAF_TYPE_DL) ? TAF_RSPEC_SU_DL : TAF_RSPEC_UL;
#else
	taf_rspec_index_t rindex = TAF_RSPEC_SU_DL;
#endif /* TAF_ENABLE_UL */

	if (ias_sched == NULL) {
		TAF_ASSERT(0);
		return 0;
	}
	est_pkt_size = ias_sched->scbtidstats.data.release_pkt_size_mean ?
		ias_sched->scbtidstats.data.release_pkt_size_mean : TAF_PKT_SIZE_DEFAULT;

	pktbytes = TAF_PKTBYTES_TO_UNITS(est_pkt_size,
		scb_taf->info.scb_stats.ias.data.pkt_rate[rindex],
		scb_taf->info.scb_stats.ias.data.byte_rate[rindex]);

	return pktbytes * len;
}

static INLINE void BCMFASTPATH
taf_update_item_stat(taf_method_info_t *method, taf_scb_cubby_t *scb_taf, int sched_tid)
{
	wlc_taf_info_t* taf_info = method->taf_info;
	taf_source_type_t s_idx;
	int tid;
	int tid_index = 0;
	taf_list_type_t type = TAF_TYPE_DL;
	taf_schedule_tid_state_t* tid_state = taf_get_tid_state(taf_info,
			TAF_TID(sched_tid, taf_info));
	taf_ias_group_info_t* ias_info = TAF_IAS_GROUP_INFO(taf_info);
#if TAF_ENABLE_MU_TX
	taf_ias_mu_t* mu = &method->ias->mu;
#endif // endif

	if (sched_tid == ALLPRIO) {
		memset(&scb_taf->info.traffic, 0, sizeof(scb_taf->info.traffic));
	} else {
#if TAF_ENABLE_SQS_PULL
		TAF_ASSERT(sched_tid == ALLPRIO);
#else
		TAF_ASSERT(TAF_PARALLEL_ORDER(taf_info));
		for (s_idx = TAF_FIRST_REAL_SOURCE; s_idx < TAF_NUM_SCHED_SOURCES; s_idx ++) {
			scb_taf->info.traffic.map[s_idx] &= ~(1 << sched_tid);
			scb_taf->info.traffic.count[s_idx][sched_tid] = 0;
		}
		scb_taf->info.traffic.available[TAF_SRC_TO_TYPE(s_idx)] &= ~(1 << sched_tid);
		scb_taf->info.traffic.est_units[sched_tid] = 0;
#endif /* TAF_ENABLE_SQS_PULL */
	}

	do {
		taf_ias_sched_data_t* ias_sched;
		uint32 est_release_total = 0;

		tid = (sched_tid == ALLPRIO) ? tid_service_order[tid_index] : sched_tid;

		if ((ias_sched = TAF_IAS_TID_STATE(scb_taf, tid)) == NULL) {
			continue;
		}

#if TAF_ENABLE_MU_TX
		scb_taf->info.mu_type[tid] = TAF_TECH_UNASSIGNED;
#endif // endif

		ias_info->ncount_flow +=
			ias_sched->scbtidstats.data.total_scaled_ncount >> TAF_NCOUNT_SCALE_SHIFT;

		/* this loops over AMPDU, NAR (and SQS if used) */
		for (s_idx = TAF_FIRST_REAL_SOURCE; s_idx < TAF_NUM_SCHED_SOURCES; s_idx ++) {
			taf_sched_handle_t* handle = &ias_sched->handle[s_idx];

			if (!handle->used) {
				continue;
			}

			type = TAF_SRC_TO_TYPE(s_idx);

			if ((scb_taf->info.traffic.count[s_idx][tid] =
				taf_info->funcs[s_idx].pktqlen_fn(handle->scbh, handle->tidh))) {

				uint32 est_release;

				scb_taf->info.traffic.map[s_idx] |= (1 << tid);
				scb_taf->info.traffic.available[type] |= (1 << tid);

				est_release = taf_ias_est_release_time(scb_taf,
					ias_sched, scb_taf->info.traffic.count[s_idx][tid], type);

				est_release_total += est_release;

				WL_TAFM(method, MACF" tid %u qlen %8s: %4u, est %5u, rel_score %4u "
					"(in flight %3u)\n", TAF_ETHERC(scb_taf), tid,
					TAF_SOURCE_NAME(s_idx),
					scb_taf->info.traffic.count[s_idx][tid],
					est_release,
					scb_taf->info.scb_stats.ias.data.relative_score[type],
					SCB_PKTS_INFLT_FIFOCNT_VAL(scb_taf->scb, tid));
			}
		}
		if ((scb_taf->info.traffic.available[TAF_TYPE_DL] & (1 << tid)) ||
#if TAF_ENABLE_UL
			(scb_taf->info.traffic.available[TAF_TYPE_UL] & (1 << tid))) {
#else
			TRUE) {
#endif // endif
#if TAF_ENABLE_MU_TX
			taf_tech_type_t tech;

			for (tech = 0; tech < TAF_NUM_MU_TECH_TYPES; tech++) {
				if (!(taf_info->mu & (1 << tech))) {
					continue;
				}
				if (scb_taf->info.mu_tech[tech].enable &
						taf_info->mu_g_enable_mask[tech] & (1 << tid)) {
					mu->est_release_units_tech[tech][tid] +=
						est_release_total;
					mu->num_tech_users[tech][tid] ++;
#if TAF_LOGL3
					WL_TAFM(method, MACF" has %u traffic for %s (U %u)\n",
						TAF_ETHERC(scb_taf), est_release_total,
						taf_mutech_text[tech],
						mu->num_tech_users[tech][tid]);
#endif // endif
				}
			}
#endif /* TAF_ENABLE_MU_TX */
			tid_state->est_release_units += est_release_total;
			scb_taf->info.traffic.est_units[tid] = est_release_total;
			TAF_DPSTATS_LOG_ADD(&ias_sched->scbtidstats, ready, 1);
		} else {
			scb_taf->force &= ~(1 << tid);
		}
	} while ((sched_tid == ALLPRIO) && ++tid_index < TAF_MAXPRIO);

	method->ias->sched_active |= scb_taf->info.traffic.available[TAF_TYPE_DL];
#if TAF_ENABLE_UL
	method->ias->sched_active |= scb_taf->info.traffic.available[TAF_TYPE_UL];
#endif // endif
}

static INLINE void BCMFASTPATH taf_update_item(taf_method_info_t *method, taf_scb_cubby_t* scb_taf)
{
	wlc_taf_info_t *taf_info = method->taf_info;

	/* Assume rate info is the same throughout all scheduling interval. */
	taf_rate_to_taf_units(taf_info, scb_taf,
#if TAF_ENABLE_MU_TX
		scb_taf->info.mu_enable_mask | TAF_TECH_DL_SU_MASK);
#else
		TAF_TECH_DL_SU_MASK);
#endif // endif

	if (method->ias->total_score) { /* avoid divide by 0 */
		scb_taf->info.scb_stats.ias.data.relative_score[TAF_TYPE_DL] =
			(scb_taf->score[TAF_TYPE_DL] << TAF_COEFF_IAS_MAX_BITS)
			/ method->ias->total_score;
#if TAF_ENABLE_UL
		if (scb_taf->info.mu_enable_mask & TAF_TECH_UL_OFDMA_MASK) {
			scb_taf->info.scb_stats.ias.data.relative_score[TAF_TYPE_UL] =
				(scb_taf->score[TAF_TYPE_UL] << TAF_COEFF_IAS_MAX_BITS)
				/ method->ias->total_score;
		}
#endif // endif
	} else {
		scb_taf->info.scb_stats.ias.data.relative_score[TAF_TYPE_DL] = 0;
#if TAF_ENABLE_UL
		scb_taf->info.scb_stats.ias.data.relative_score[TAF_TYPE_UL] = 0;
#endif // endif
	}

	memset(scb_taf->info.released_units_limit, 0, sizeof(scb_taf->info.released_units_limit));
#if TAF_ENABLE_SQS_PULL
	scb_taf->info.pkt_pull_dequeue = 0;
#endif // endif
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
	/* The decay coeff, is how much fractional reduction to occur per time interval
	 * of elapsed time. This is an exponential model.
	 */
	uint32 value = initial;
	uint32 counter = elapsed >> decay_coeff->time_shift;

	if (correct) {
		*correct = (int32)(elapsed) - (int32)(counter << decay_coeff->time_shift);
	}
	if (counter > 1000) {
		/* if it is a long time, just reset the score */
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
		while (value && counter) {
			value = value * decay_coeff->coeff1;
			value >>= TAF_COEFF_IAS_MAX_BITS; /* normalise coeff */
			counter--;
		}
	}
	return value;
}

static INLINE void BCMFASTPATH taf_ias_pre_update_list(taf_method_info_t *method, uint32 now_time)
{
	taf_list_t *item;
	uint32 total_score = 0;

	for (item = method->list; item; item = item->next) {
		int32  time_correction = 0;
		uint32 elapsed;
		uint32 score;
		taf_scb_cubby_t* scb_taf = item->scb_taf;

		if (SCB_MARKED_DEL(scb_taf->scb) || SCB_DEL_IN_PROGRESS(scb_taf->scb)) {
			scb_taf->force = 0;
			/* max score means least priority for IAS */
			scb_taf->score[item->type] = TAF_SCORE_MAX;
			WL_TAFM(method, MACF" will be deleted\n", TAF_ETHERC(scb_taf));
			continue;
		}

		switch (method->type) {
			case TAF_EBOS:
				break;

			case TAF_PSEUDO_RR:
			case TAF_ATOS:
			case TAF_ATOS2:
				elapsed = now_time -
					scb_taf->info.scb_stats.ias.data.dcaytimestamp[item->type];
				score = taf_ias_decay_score(scb_taf->score[item->type],
					&method->ias->coeff, elapsed, &time_correction);
				if (score != scb_taf->score[item->type]) {
					scb_taf->score[item->type] = score;
					scb_taf->info.scb_stats.ias.data.dcaytimestamp[item->type] =
						now_time - time_correction;
				}
#if TAF_LOGL4
				WL_TAFM(method, MACF" type %d score %d\n",
					TAF_ETHERC(scb_taf), item->type,
					scb_taf->score[item->type]);
#endif // endif
				break;
			default:
				TAF_ASSERT(0);
				break;
		}
		total_score += scb_taf->score[item->type];
	}
	method->ias->total_score = total_score;
}

static INLINE void BCMFASTPATH taf_ias_post_update_list(taf_method_info_t *method, int tid)
{
	taf_list_t *item;

#if TAF_ENABLE_MU_TX
	memset(&method->ias->mu, 0, sizeof(method->ias->mu));
#endif // endif
	for (item = method->list; item; item = item->next) {
		taf_scb_cubby_t* scb_taf = item->scb_taf;

		if (SCB_MARKED_DEL(scb_taf->scb) ||
			SCB_DEL_IN_PROGRESS(scb_taf->scb) || (item->type != TAF_TYPE_DL)) {
			continue;
		}
		taf_update_item(method, scb_taf);
		taf_update_item_stat(method, scb_taf, tid);
	}
}

#if TAF_ENABLE_MU_TX
static INLINE bool BCMFASTPATH taf_ias_dl_ofdma_suitable(taf_scb_cubby_t *scb_taf, int tid,
	taf_tech_type_t tech_idx)
{
	if (scb_taf->info.ps_mode) {
		return FALSE;
	}
	if (scb_taf->info.mu_type[tid] < TAF_TECH_UNASSIGNED) {
		return FALSE;
	}
	if (scb_taf->info.traffic.est_units[tid] == 0) {
		return FALSE;
	}
	return (scb_taf->info.mu_tech[tech_idx].enable & (1 << tid)) ? TRUE : FALSE;
}

static INLINE bool BCMFASTPATH taf_ias_dl_ofdma(taf_method_info_t* method, taf_tech_type_t tech_idx,
	taf_list_t *list, taf_schedule_tid_state_t *tid_state, int tid,
	taf_release_context_t *context)
{
	wlc_taf_info_t* taf_info = method->taf_info;
	taf_scb_cubby_t *items[TAF_MAX_MU_OFDMA];
	taf_scb_cubby_t *scb_taf = list->scb_taf;
	taf_scb_cubby_t *scb_taf_next = NULL;
	taf_ias_mu_t* mu = &method->ias->mu;
	uint32 inter_item[TAF_MAX_MU_OFDMA];
	uint32 est_inter_total = 0;
	uint32 mu_user_count = 0;
	uint32 min_mu_traffc;
	uint32 time_limit_units;
	uint32 index;
	int bw_idx = scb_taf->info.scb_stats.global.data.bw_idx;
	const taf_list_type_t type = list->type;

	TAF_ASSERT(tech_idx == TAF_TECH_DL_OFDMA);
	TAF_ASSERT(taf_info->dlofdma_maxn[bw_idx] > 0);

	time_limit_units = TAF_MICROSEC_TO_UNITS(tid_state->high[IAS_INDEXM(method)]);
	if (time_limit_units <= tid_state->total.released_units) {
		return FALSE;
	}
	time_limit_units -= tid_state->total.released_units;

	if (TAF_OFDMA_OPT(method, LOW_TRAFFIC) &&
			time_limit_units > mu->est_release_units_tech[tech_idx][tid]) {
		WL_TAFM(method, "total available traffic for %s cannot fill remaining window %u\n",
			taf_mutech_text[tech_idx], time_limit_units);
		mu->est_release_units_tech[tech_idx][tid] = 0;
		return FALSE;
	}

	items[mu_user_count] = scb_taf;
	inter_item[mu_user_count] = 0;
	mu_user_count++;

	WL_TAFM(method, "this %s item is "MACF" having %u units tid %u and score %u, "
		"rel score %u, bw_idx %u (opt %u)\n",
		taf_mutech_text[tech_idx], TAF_ETHERC(scb_taf),
		scb_taf->info.traffic.est_units[tid], tid, scb_taf->score[type],
		scb_taf->info.scb_stats.ias.data.relative_score[type], bw_idx,
		method->ias->mu_ofdma_opt);

	min_mu_traffc = scb_taf->info.traffic.est_units[tid];

	while (list) {
		uint32 rel_diff;
		uint32 fraction;

		inter_item[mu_user_count] = 0;

		for (list = list->next; list; list = list->next) {
			scb_taf_next = list->scb_taf;

			if ((type == TAF_TYPE_DL) &&
				taf_ias_dl_ofdma_suitable(scb_taf_next, tid, tech_idx)) {
				break;
			}
			if (scb_taf_next->info.mu_type[tid] != TAF_TECH_DONT_ASSIGN) {
				inter_item[mu_user_count] +=
					scb_taf_next->info.traffic.est_units[tid];
			}
		}

		if (!list || !scb_taf_next) {
			WL_TAFM(method, "no next %u %s item found\n", mu_user_count + 1,
				taf_mutech_text[tech_idx]);
			break;
		}

		if (inter_item[mu_user_count] > 0 && !TAF_OFDMA_OPT(method, SWEEP)) {
			WL_TAFM(method, "intermediate traffic found\n");
			break;
		}

		WL_TAFM(method, "next %s item is "MACF" having %u units tid %u and score "
			"%u, rel_score %u (inter traffic %u)\n",
			taf_mutech_text[tech_idx], TAF_ETHERC(scb_taf_next),
			scb_taf_next->info.traffic.est_units[tid], tid,
			scb_taf_next->score[TAF_TYPE_DL],
			scb_taf_next->info.scb_stats.ias.data.relative_score[TAF_TYPE_DL],
			inter_item[mu_user_count]);

		rel_diff = scb_taf_next->info.scb_stats.ias.data.relative_score[type] -
			scb_taf->info.scb_stats.ias.data.relative_score[type];

		if (TAF_OFDMA_OPT(method, PAIR) &&
			rel_diff > method->ias->mu_pair[mu_user_count]) {

			WL_TAFM(method, "relative score diff %u "MACF" is too large (limit %u)\n",
				rel_diff, TAF_ETHERC(scb_taf_next),
				method->ias->mu_pair[mu_user_count]);
			break;
		}

		fraction = (mu_user_count + 1) * scb_taf_next->info.traffic.est_units[tid];

		if (TAF_OFDMA_OPT(method, TOO_SMALL) &&
			(fraction < time_limit_units) && (fraction < min_mu_traffc)) {

			WL_TAFM(method, "traffic available is too small (%u/%u/%u)\n",
				fraction, time_limit_units, min_mu_traffc);
			break;
		}
		if (TAF_OFDMA_OPT(method, TOO_LARGE) &&
			(((mu_user_count + 1) * min_mu_traffc) < time_limit_units) &&
			(scb_taf_next->info.traffic.est_units[tid] > time_limit_units)) {

			WL_TAFM(method, "traffic available too large for pairing "
				"(%u/%u)\n",  time_limit_units, min_mu_traffc);
			break;
		}

		items[mu_user_count] = scb_taf_next;
		est_inter_total += inter_item[mu_user_count];

		if (scb_taf_next->info.traffic.est_units[tid] < min_mu_traffc) {
			min_mu_traffc = scb_taf_next->info.traffic.est_units[tid];
		}
		bw_idx = MAX(bw_idx, scb_taf_next->info.scb_stats.global.data.bw_idx);
		TAF_ASSERT(bw_idx <= D11_REV128_BW_160MHZ);
		mu_user_count++;

		if ((mu_user_count >= TAF_MAX_MU_OFDMA) ||
			(mu_user_count >= taf_info->dlofdma_maxn[bw_idx])) {
			WL_TAFM(method, "ending due to mu_user_count of %u\n", mu_user_count);
			break;
		}
	}

	if (mu_user_count < 2) {
		WL_TAFM(method, "only %u %s user possible\n", mu_user_count,
			taf_mutech_text[tech_idx]);
		return FALSE;
	}

	if (est_inter_total == 0) {
		time_limit_units = time_limit_units / mu_user_count;

		time_limit_units = MIN(time_limit_units, min_mu_traffc);

		if (method->ias->release_limit > 0) {
			uint32 global_rel_limit = TAF_MICROSEC_TO_UNITS(method->ias->release_limit);
			time_limit_units = MIN(time_limit_units, global_rel_limit);
		}

		WL_TAFM(method, "final %s possible: %u users, %u traffic possible per user, "
			"bw_idx %u - maxN %u\n",
			taf_mutech_text[tech_idx], mu_user_count, time_limit_units, bw_idx,
			taf_info->dlofdma_maxn[bw_idx]);

	} else {
		WL_TAFM(method, "intermediate traffic count %u so skip %s release this frame\n",
			est_inter_total, taf_mutech_text[tech_idx]);
		tech_idx = TAF_TECH_DONT_ASSIGN;
		time_limit_units = 0;
	}

	for (index = 0; index < mu_user_count; index++) {

		scb_taf_next = items[index];

		scb_taf_next->info.mu_type[tid] = tech_idx;
		scb_taf_next->info.released_units_limit[tid] = time_limit_units;

		if (tech_idx < TAF_NUM_TECH_TYPES) {
			WL_TAFM(method, "set rel limit %u to "MACF" tid %u %s\n",
				scb_taf_next->info.released_units_limit[tid],
				TAF_ETHERC(scb_taf_next), tid,
				taf_mutech_text[tech_idx]);
		}
	}

	return TRUE;
}

#if TAF_ENABLE_UL
static INLINE bool BCMFASTPATH taf_ias_ul_ofdma(taf_method_info_t* method, taf_tech_type_t tech_idx,
	taf_list_t *list, taf_schedule_tid_state_t *tid_state, int tid,
	taf_release_context_t *context)
{
	taf_scb_cubby_t *items[TAF_MAX_MU];
	taf_scb_cubby_t *scb_taf = list->scb_taf;
	taf_scb_cubby_t *scb_taf_next = NULL;
	taf_ias_mu_t* mu = &method->ias->mu;
	uint32 mu_user_count = 0;
	uint32 min_mu_traffc;
	uint32 time_limit_units;
	uint32 index;
	uint32 est_inter_total = 0;
	const taf_list_type_t type = list->type;

	TAF_ASSERT(tech_idx == TAF_TECH_UL_OFDMA);

	time_limit_units = TAF_MICROSEC_TO_UNITS(tid_state->high[IAS_INDEXM(method)]);
	if (time_limit_units <= tid_state->total.released_units) {
		return FALSE;
	}
	time_limit_units -= tid_state->total.released_units;

	if (time_limit_units > mu->est_release_units_tech[tech_idx][tid]) {
		WL_TAFM(method, "total available traffic for %s cannot fill remaining window %u\n",
			taf_mutech_text[tech_idx], time_limit_units);
		return FALSE;
	}

	items[mu_user_count] = scb_taf;
	mu_user_count++;

	WL_TAFM(method, "this %s item is "MACF" having %u units tid %u and score %u, "
		"rel score %u;\n", taf_mutech_text[tech_idx], TAF_ETHERC(scb_taf),
		scb_taf->info.traffic.est_units[tid], tid, scb_taf->score[type],
		scb_taf->info.scb_stats.ias.data.relative_score[type]);

	min_mu_traffc = scb_taf->info.traffic.est_units[tid];

	while (list) {
		uint32 rel_diff;

		for (list = list->next; list; list = list->next) {
			scb_taf_next = list->scb_taf;

			if ((type == TAF_TYPE_UL) &&
				taf_ias_dl_ofdma_suitable(scb_taf_next, tid, tech_idx)) {
				break;
			}

			est_inter_total += scb_taf_next->info.traffic.est_units[tid];
		}
		if (est_inter_total) {
			WL_TAFM(method, "non adjacent queued traffic (%u)\n", est_inter_total);
			break;
		}

		if (list && scb_taf_next) {
			uint32 fraction;

			WL_TAFM(method, "next %s item is "MACF" having %u units tid %u and score "
				"%u, rel_score %u; inter traffic is %u units\n",
				taf_mutech_text[tech_idx], TAF_ETHERC(scb_taf_next),
				scb_taf_next->info.traffic.est_units[tid], tid,
				scb_taf_next->score[type],
				scb_taf_next->info.scb_stats.ias.data.relative_score[type],
				est_inter_total);

			fraction = (mu_user_count + 1) * scb_taf_next->info.traffic.est_units[tid];

			if ((fraction < time_limit_units) && (fraction < min_mu_traffc)) {
				WL_TAFM(method, "traffic available is too small (%u/%u/%u)\n",
					fraction, time_limit_units, min_mu_traffc);
				break;
			}
			if ((((mu_user_count + 1) * min_mu_traffc) < time_limit_units) &&
				(scb_taf_next->info.traffic.est_units[tid] > time_limit_units)) {
				WL_TAFM(method, "traffic available too large for pairing "
					"(%u/%u)\n",  time_limit_units, min_mu_traffc);
				break;
			}
		} else {
			WL_TAFM(method, "no next %u %s item found\n", mu_user_count + 1,
				taf_mutech_text[tech_idx]);
			break;
		}

		rel_diff = scb_taf_next->info.scb_stats.ias.data.relative_score[type] -
			scb_taf->info.scb_stats.ias.data.relative_score[type];

		if (rel_diff <= method->ias->mu_pair[mu_user_count]) {

			items[mu_user_count] = scb_taf_next;
			mu_user_count++;

			if (scb_taf_next->info.traffic.est_units[tid] < min_mu_traffc) {
				min_mu_traffc = scb_taf_next->info.traffic.est_units[tid];
			}
		} else {
			WL_TAFM(method, "relative score diff %u is too large (limit %u)\n",
				rel_diff, method->ias->mu_pair[mu_user_count]);
			break;
		}
		if (mu_user_count >= TAF_MAX_MU) {
			break;
		}
	}

	if (mu_user_count < 2) {
		WL_TAFM(method, "only %u %s user possible\n", mu_user_count,
			taf_mutech_text[tech_idx]);
		return FALSE;
	}

	/* slightly scale down time here (reduce by ~3%) to allow some space for
	 * over-filling due to finite packet sizes/phy rates
	 */
	time_limit_units *= 31;
	time_limit_units >>= 5;

	time_limit_units = time_limit_units / mu_user_count;

	time_limit_units = MIN(time_limit_units, min_mu_traffc);

	if (method->ias->release_limit > 0) {
		uint32 global_rel_limit = TAF_MICROSEC_TO_UNITS(method->ias->release_limit);
		time_limit_units = MIN(time_limit_units, global_rel_limit);
	}

	WL_TAFM(method, "final %s possible: %u users, %u traffic possible per user\n",
		taf_mutech_text[tech_idx], mu_user_count, time_limit_units);

	for (index = 0; index < mu_user_count; index++) {

		scb_taf_next = items[index];

		scb_taf_next->info.mu_type[tid] = tech_idx;
		scb_taf_next->info.released_units_limit[tid] = time_limit_units;

		WL_TAFM(method, "set rel limit %u to "MACF" %s\n",
			scb_taf_next->info.released_units_limit[tid], TAF_ETHERC(scb_taf_next),
			taf_mutech_text[tech_idx]);
	}

	return TRUE;
}
#endif /* TAF_ENABLE_UL */

static INLINE bool BCMFASTPATH taf_ias_dl_mimo_suitable(taf_scb_cubby_t *scb_taf, uint8 bw,
	uint32 streams_max, int tid, taf_tech_type_t tech_idx)
{
	uint32 streams;
	if (scb_taf->info.ps_mode) {
		return FALSE;
	}
	if (scb_taf->info.mu_type[tid] < TAF_TECH_UNASSIGNED) {
		return FALSE;
	}
	if (scb_taf->info.traffic.est_units[tid] == 0) {
		return FALSE;
	}
	if (taf_bw(scb_taf) != bw) {
		return FALSE;
	}

	if ((streams = taf_nss(scb_taf)) >= streams_max || (streams_max == 4 && streams == 3)) {
		return FALSE;
	}

	return (scb_taf->info.mu_tech[tech_idx].enable & (1 << tid)) ? TRUE : FALSE;
}

static INLINE bool BCMFASTPATH taf_ias_dl_mimo(taf_method_info_t* method, taf_tech_type_t tech_idx,
	taf_list_t *list, taf_schedule_tid_state_t *tid_state, int tid,
	taf_release_context_t *context)
{
	taf_scb_cubby_t *items[TAF_MAX_MU_MIMO];
	taf_scb_cubby_t *scb_taf = list->scb_taf;
	taf_scb_cubby_t *scb_taf_next = NULL;
	uint32 inter_item[TAF_MAX_MU_MIMO];
	uint32 est_inter_total = 0;
	uint32 mu_user_count = 0;
	uint32 time_limit_units;
	uint32 streams = 0;
	uint32 streams_max = TAF_WLCM(method)->stf->op_txstreams;
	uint32 min_mu_traffc;
	uint32 max_mu_traffc;
	uint32 user_rel_limit;
	uint8  bw;
	uint32 index;
	uint32 orig_release;
	uint32 total_mu_traffic;
	uint32 release_len;

	TAF_ASSERT(tech_idx == TAF_TECH_DL_HEMUMIMO || tech_idx == TAF_TECH_DL_VHMUMIMO);
	TAF_ASSERT(method->ias->mu_mimo_rel_limit);

	time_limit_units = TAF_MICROSEC_TO_UNITS(tid_state->high[IAS_INDEXM(method)]);
	if (time_limit_units <= tid_state->total.released_units) {
		return FALSE;
	}
	time_limit_units -= tid_state->total.released_units;

	if ((streams = taf_nss(scb_taf)) >= streams_max || (streams_max == 4 && streams == 3)) {
		return FALSE;
	}

	WL_TAFM(method, "this %s item is "MACF" having %6u units tid %u score %5u, "
		"rel score %4u, mu_nss %u, max_nss %u  (opt %u)\n",
		taf_mutech_text[tech_idx], TAF_ETHERC(scb_taf),
		scb_taf->info.traffic.est_units[tid], tid, scb_taf->score[TAF_TYPE_DL],
		scb_taf->info.scb_stats.ias.data.relative_score[TAF_TYPE_DL], taf_nss_mu(scb_taf),
		streams, method->ias->mu_mimo_opt);

	streams = 1;
	bw = taf_bw(scb_taf);
	items[mu_user_count] = scb_taf;
	inter_item[mu_user_count] = 0;

	mu_user_count++;
	orig_release = scb_taf->info.traffic.est_units[tid];
	min_mu_traffc = orig_release;
	max_mu_traffc = orig_release;
	total_mu_traffic = orig_release;

	while (list) {
#ifdef TAF_DBG
		uint32 streams_next;
#endif // endif
		inter_item[mu_user_count] = 0;

		for (list = list->next; list; list = list->next) {
			scb_taf_next = list->scb_taf;

			if ((list->type == TAF_TYPE_DL) &&
				taf_ias_dl_mimo_suitable(scb_taf_next, bw, streams_max,
					tid, tech_idx)) {
#ifdef TAF_DBG
				streams_next = taf_nss(scb_taf_next);
#endif // endif
				break;
			}
			if (scb_taf_next->info.mu_type[tid] != TAF_TECH_DONT_ASSIGN) {
				inter_item[mu_user_count] +=
					scb_taf_next->info.traffic.est_units[tid];
			}
		}

		if (!list || !scb_taf_next) {
			WL_TAFM(method, "no next %u %s item found\n", mu_user_count + 1,
				taf_mutech_text[tech_idx]);
			break;
		}

		if (inter_item[mu_user_count] > 0 && !TAF_MIMO_OPT(method, SWEEP)) {
			WL_TAFM(method, "intermediate traffic found\n");
			break;
		}

		if (TAF_MIMO_OPT(method, PAIR)) {
			uint32 rel_diff =
				scb_taf_next->info.scb_stats.ias.data.relative_score[TAF_TYPE_DL] -
				scb_taf->info.scb_stats.ias.data.relative_score[TAF_TYPE_DL];

			if (rel_diff > method->ias->mu_pair[mu_user_count]) {
				WL_TAFM(method, "relative score diff %u "MACF" is too large "
					"(limit %u)\n", rel_diff, TAF_ETHERC(scb_taf_next),
					method->ias->mu_pair[mu_user_count]);
				break;
			}
		}

		items[mu_user_count] = scb_taf_next;
		est_inter_total += inter_item[mu_user_count];

		++streams;

		total_mu_traffic += scb_taf_next->info.traffic.est_units[tid];

		max_mu_traffc = MAX(max_mu_traffc, scb_taf_next->info.traffic.est_units[tid]);
		min_mu_traffc = MIN(min_mu_traffc, scb_taf_next->info.traffic.est_units[tid]);

		WL_TAFM(method, "next %s item is "MACF" having %6u units tid %u score "
			"%5u, rel_score %4u, mu_nss %u, max_nss %u  (inter traffic %u)\n",
			taf_mutech_text[tech_idx], TAF_ETHERC(scb_taf_next),
			scb_taf_next->info.traffic.est_units[tid], tid,
			scb_taf_next->score[TAF_TYPE_DL],
			scb_taf_next->info.scb_stats.ias.data.relative_score[TAF_TYPE_DL],
			taf_nss_mu(scb_taf_next), streams_next, inter_item[mu_user_count]);

		mu_user_count++;

		if (mu_user_count >= TAF_MAX_MU_MIMO) {
			WL_TAFM(method, "ending %s due to mu_user_count of %u\n",
				taf_mutech_text[tech_idx], mu_user_count);
			break;
		}
	}

	if (mu_user_count == 1) {
		WL_TAFM(method, "only 1 %s user possible\n", taf_mutech_text[tech_idx]);
		return FALSE;
	}

	if (TAF_MIMO_OPT(method, MEAN)) {
		release_len = total_mu_traffic / mu_user_count;

	} else if (TAF_MIMO_OPT(method, MIN)) {
		release_len = min_mu_traffc;

	} else if (TAF_MIMO_OPT(method, MEDIAN)) {
		release_len = (min_mu_traffc + max_mu_traffc) / 2;

	} else {
		release_len = max_mu_traffc;
	}

	if (est_inter_total == 0) {
		user_rel_limit =
			TAF_MICROSEC_TO_UNITS(method->ias->mu_mimo_rel_limit[mu_user_count - 1]);

		if (user_rel_limit && release_len > user_rel_limit) {
			WL_TAFM(method, "mu-mimo release %u limiting to %uus (%u)\n", mu_user_count,
				TAF_UNITS_TO_MICROSEC(user_rel_limit), user_rel_limit);

			release_len = user_rel_limit;
		}

		if (release_len > time_limit_units) {
			WL_TAFM(method, "mu-mimo release limiting to remaining window %uus (%u)\n",
				TAF_UNITS_TO_MICROSEC(time_limit_units), time_limit_units);
			release_len = time_limit_units;
		}

		if (method->ias->release_limit > 0 && release_len > method->ias->release_limit) {
			uint32 global_rel_limit = TAF_MICROSEC_TO_UNITS(method->ias->release_limit);

			WL_TAFM(method, "mu-mimo release limiting to global limit %uus (%u)\n",
				method->ias->release_limit, global_rel_limit);
			release_len = global_rel_limit;
		}

		WL_TAFM(method, "final %s possible: %u users (%u streams), %u/%u min/max traffic, "
			"%u possible per user\n", taf_mutech_text[tech_idx], mu_user_count, streams,
			min_mu_traffc, max_mu_traffc, release_len);
	} else {
		WL_TAFM(method, "intermediate traffic count %u so sweep this %s release\n",
			est_inter_total, taf_mutech_text[tech_idx]);
		tech_idx = TAF_TECH_DONT_ASSIGN;
		release_len = 0;
	}

	total_mu_traffic = 0;

	for (index = 0; index < mu_user_count; index++) {

		scb_taf_next = items[index];
		scb_taf_next->info.mu_type[tid] = tech_idx;

		if (tech_idx < TAF_NUM_TECH_TYPES) {
			scb_taf_next->info.released_units_limit[tid] = MIN(release_len,
				scb_taf_next->info.traffic.est_units[tid]);

			total_mu_traffic += scb_taf_next->info.released_units_limit[tid];

			WL_TAFM(method, "set rel limit %5u to "MACF" tid %u %s\n",
				scb_taf_next->info.released_units_limit[tid],
				TAF_ETHERC(scb_taf_next), tid,
				taf_mutech_text[tech_idx]);
		}
	}
	if (total_mu_traffic > time_limit_units) {
		tid_state->extend.units += total_mu_traffic - time_limit_units;
		WL_TAFM(method, "%s extend units %u\n", taf_mutech_text[tech_idx],
			total_mu_traffic - time_limit_units);
	}

	return TRUE;
}
#endif /* TAF_ENABLE_MU_TX */

static INLINE void BCMFASTPATH taf_ias_analyse_mu(taf_method_info_t* method,
	taf_list_t *list, taf_schedule_tid_state_t *tid_state, int tid,
	taf_release_context_t *context)
{
#if TAF_ENABLE_MU_TX
	taf_tech_type_t tech_idx;
	taf_scb_cubby_t *scb_taf = list->scb_taf;
	taf_ias_mu_t* mu = &method->ias->mu;

	TAF_ASSERT(tid >= 0 && tid < NUMPRIO);

	if (scb_taf->info.ps_mode) {
		return;
	}
	if (!scb_taf->info.scb_stats.ias.data.use[TAF_SOURCE_AMPDU] ||
			scb_taf->info.linkstate[TAF_SOURCE_AMPDU][tid] != TAF_LINKSTATE_ACTIVE) {
		return;
	}
	if (scb_taf->info.mu_type[tid] < TAF_TECH_UNASSIGNED) {
#if TAF_LOGL3
		WL_TAFM(method, MACF" tid %u is already allocated for %s (%u units)\n",
			TAF_ETHERC(scb_taf), tid, taf_mutech_text[scb_taf->info.mu_type[tid]],
			scb_taf->info.released_units_limit);
#endif // endif
		return;
	}

	for (tech_idx = 0; scb_taf->info.mu_type[tid] == TAF_TECH_UNASSIGNED &&
			tech_idx < TAF_NUM_MU_TECH_TYPES;
			tech_idx++) {
		wlc_taf_info_t* taf_info = method->taf_info;

		if (!(taf_info->mu & (1 << tech_idx))) {
			continue;
		}
		if (!(scb_taf->info.mu_tech[tech_idx].enable &
			taf_info->mu_g_enable_mask[tech_idx] & (1 << tid))) {
			continue;
		}
		if (mu->num_tech_users[tech_idx][tid] < 2) {
			continue;
		}
		switch (tech_idx) {
			case TAF_TECH_DL_HEMUMIMO:
			case TAF_TECH_DL_VHMUMIMO:
				taf_ias_dl_mimo(method, tech_idx, list, tid_state, tid, context);
				continue;
			case TAF_TECH_DL_OFDMA:
				taf_ias_dl_ofdma(method, tech_idx, list, tid_state, tid, context);
				continue;
#if TAF_ENABLE_UL
			case TAF_TECH_UL_OFDMA:
				taf_ias_ul_ofdma(method, tech_idx, list, tid_state, tid, context);
				continue;
#endif /* TAF_ENABLE_UL */
			default:
				TAF_ASSERT(0);
		}
		break;
	}
	if (scb_taf->info.mu_type[tid] == TAF_TECH_UNASSIGNED) {
		WL_TAFM(method, MACF" tid %u to be scheduled as SU\n", TAF_ETHERC(scb_taf), tid);
		scb_taf->info.mu_type[tid] = TAF_TECH_DL_SU;
	}
#else
	BCM_REFERENCE(method);
	BCM_REFERENCE(list);
	BCM_REFERENCE(tid_state);
	BCM_REFERENCE(tid);
#endif /* TAF_ENABLE_MU_TX */
}

static INLINE void BCMFASTPATH taf_ias_prepare_list(taf_method_info_t *method, uint32 now_time,
	int tid)
{
	taf_ias_pre_update_list(method, now_time);

	if (method->ordering == TAF_LIST_SCORE_MINIMUM ||
			method->ordering == TAF_LIST_SCORE_MAXIMUM) {
		wlc_taf_sort_list(&method->list, method->ordering, now_time);
	}
	taf_ias_post_update_list(method, tid);
}

static INLINE void BCMFASTPATH
taf_ias_optimise_agg(taf_scb_cubby_t *scb_taf, int tid, taf_source_type_t s_idx,
	taf_schedule_tid_state_t* tid_state, taf_schedule_state_t op_state,
	taf_release_context_t *context)
{
#if TAF_ENABLE_AMPDU
	/* aggregation optimisation */
	if (TAF_SOURCE_IS_AMPDU(s_idx) && !scb_taf->info.ps_mode) {
		/* hold_for_aggs = AMSDU optimise */
		bool hold_for_aggs = (tid_state->trigger == TAF_TRIGGER_IMMEDIATE);
		/* hold_for_aggp = AMPDU optimise */
		bool hold_for_aggp;
		uint32 more_traffic = 0;
		taf_method_info_t* scb_method = scb_taf->method;
		uint32 wme_ac = SCB_WME(scb_taf->scb) ? WME_PRIO2AC(tid) : AC_BE;
		int32 in_flight = SCB_PKTS_INFLT_FIFOCNT_VAL(scb_taf->scb, tid);
#if TAF_ENABLE_MU_TX
		bool mu_agg_hold = TRUE;

		if (scb_taf->info.mu_type[tid] == TAF_TECH_DL_OFDMA &&
			!TAF_OFDMA_OPT(scb_method, AGG_HOLD)) {

			mu_agg_hold = FALSE;

		} else if (scb_taf->info.mu_type[tid] <= TAF_TECH_DL_VHMUMIMO &&
			!TAF_MIMO_OPT(scb_method, AGG_HOLD)) {

			mu_agg_hold = FALSE;
		}
#else
		const bool mu_agg_hold = TRUE;
#endif // endif

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
		hold_for_aggs = hold_for_aggs && (in_flight > 0) && mu_agg_hold;

		/* if already plenty of in flight traffic, do not hold AMPDU as AQM can be fed */
		hold_for_aggp = hold_for_aggs && (in_flight < scb_taf->info.max_pdu) && mu_agg_hold;

#if TAF_ENABLE_SQS_PULL
		/* Is there is more traffic on the way very soon...? Aggregation can be held as
		 * forthcoming traffic will add to packets already available. Leftover aggregation
		 * packets will ripple forward into the new arriving data where this decision will
		 * be made again next time.
		 */
		more_traffic = (op_state == TAF_SCHEDULE_VIRTUAL_PACKETS) ?
			scb_taf->info.traffic.count[TAF_SOURCE_HOST_SQS][tid] :
			scb_taf->info.pkt_pull_request[tid];
#endif /* TAF_ENABLE_SQS_PULL */

		hold_for_aggs = hold_for_aggs || (more_traffic > 0);
		hold_for_aggp = hold_for_aggp || (more_traffic > 0);

		/* flag to enable aggregation optimisation or not */
		context->public.ias.opt_aggs = hold_for_aggs;
		context->public.ias.opt_aggp = hold_for_aggp;

		/* pdu number threshold for AMPDU holding */
		context->public.ias.opt_aggp_limit =
			scb_method->ias->opt_aggp_limit < scb_taf->info.max_pdu ?
			scb_method->ias->opt_aggp_limit : scb_taf->info.max_pdu;
	} else
#endif /* TAF_ENABLE_AMPDU */
	{
		context->public.ias.opt_aggs = FALSE;
		context->public.ias.opt_aggp = FALSE;
	}
}

static bool BCMFASTPATH
taf_ias_send_source(taf_method_info_t *method, taf_scb_cubby_t *scb_taf,
		int tid, taf_source_type_t s_idx, taf_schedule_tid_state_t* tid_state,
		taf_release_context_t *context, taf_list_type_t type)
{
	wlc_taf_info_t* taf_info;
	uint32 actual_release = 0;
#if TAF_ENABLE_SQS_PULL
	uint32 virtual_release = 0;
	uint32 pending_release = 0;
#else
#ifdef TAF_DBG
	const uint32 virtual_release = 0;
	const uint32 pending_release = 0;
#endif /* TAF_DBG */
#endif /* TAF_ENABLE_SQS_PULL */
	uint32 extend_units = 0;
	uint32 released_units = 0;
	uint32 released_bytes = 0;
	uint32 time_limit_units;
	taf_ias_sched_data_t* ias_sched = TAF_IAS_TID_STATE(scb_taf, tid);
	taf_ias_sched_scbtid_stats_t* scbtidstats;
	bool force;
	uint32 pre_qlen = 0;
	uint32 post_qlen = 0;
	uint32 release_n = 0;

	TAF_ASSERT(ias_sched);

#ifdef TAF_DBG
	BCM_REFERENCE(virtual_release);
	BCM_REFERENCE(pending_release);
#endif // endif
	if (!ias_sched->handle[s_idx].used) {
		return TRUE;
	}

	taf_info = method->taf_info;
	time_limit_units = tid_state->extend.units +
		TAF_MICROSEC_TO_UNITS(tid_state->high[IAS_INDEXM(method)]);
	scbtidstats = &ias_sched->scbtidstats;
	force = (scb_taf->force & (1 << tid)) ? TRUE : FALSE;
	context->public.ias.was_emptied = FALSE;
	context->public.ias.is_ps_mode = scb_taf->info.ps_mode;

#if TAF_ENABLE_SQS_PULL
	if (TAF_SOURCE_IS_VIRTUAL(s_idx)) {
		if (scb_taf->info.pkt_pull_request[tid]) {
			/* left-over unfulfilled request */
			WL_ERROR(("wl%u %s: (%x) "MACF" tid %u pkt_pull_rqst %u map 0x%x "
				"(sqs fault %u)\n",
				WLCWLUNIT(TAF_WLCT(taf_info)), __FUNCTION__,
				taf_info->scheduler_index,
				TAF_ETHERC(scb_taf),
				tid, scb_taf->info.pkt_pull_request[tid],
				scb_taf->info.pkt_pull_map,
				taf_info->sqs_state_fault));
			scb_taf->info.pkt_pull_request[tid] = 0;
			scb_taf->info.pkt_pull_map &= ~(1 << tid);
		}
		context->public.ias.estimated_pkt_size_mean =
			scbtidstats->data.release_pkt_size_mean ?
			scbtidstats->data.release_pkt_size_mean : TAF_PKT_SIZE_DEFAULT;
		context->public.ias.traffic_count_available =
			scb_taf->info.traffic.count[s_idx][tid];

		/* margin here is used to ask SQS for extra packets beyond nominal */
		context->public.ias.margin = method->ias->margin;
	}
#endif /* TAF_ENABLE_SQS_PULL */
	if (TAF_SOURCE_IS_REAL(s_idx)) {
		/* Record exact level of queue before; this is required because AMSDU
		* in AMPDU is variable according to various configs and situations, and it
		* is necessary to measure this again after release to see what actually went
		* for accurate packet size estimation
		*/
		pre_qlen = taf_info->funcs[s_idx].pktqlen_fn(ias_sched->handle[s_idx].scbh,
			ias_sched->handle[s_idx].tidh);
	}

	if (type == TAF_TYPE_DL) {
		/* aggregation optimisation */
		taf_ias_optimise_agg(scb_taf, tid, s_idx, tid_state, taf_info->op_state, context);
	}

	context->public.complete = TAF_REL_COMPLETE_TIME_LIMIT;

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

		context->public.complete = TAF_REL_COMPLETE_NULL;

		result = taf_info->funcs[s_idx].release_fn(*taf_info->source_handle_p[s_idx],
			ias_sched->handle[s_idx].scbh, ias_sched->handle[s_idx].tidh, force,
			&context->public);

#if TAF_ENABLE_SQS_PULL
		if (TAF_SOURCE_IS_VIRTUAL(s_idx)) {
			TAF_ASSERT(context->public.ias.actual.release == 0);

			virtual_release += context->public.ias.virtual.release;
			context->public.ias.virtual.release = 0;

			pending_release += context->public.ias.pending.release;
			context->public.ias.pending.release = 0;

			released_units += context->public.ias.virtual.released_units;
			context->public.ias.virtual.released_units = 0;

			released_units += context->public.ias.pending.released_units;
			context->public.ias.pending.released_units = 0;
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

		extend_units += context->public.ias.extend.units;
		context->public.ias.extend.units = 0;

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
		if (TAF_SOURCE_IS_REAL(s_idx) && (pre_qlen > 0)) {
			post_qlen = taf_info->funcs[s_idx].pktqlen_fn(ias_sched->handle[s_idx].scbh,
				ias_sched->handle[s_idx].tidh);

			release_n = pre_qlen - post_qlen;
		}

#if TAF_ENABLE_SQS_PULL
		if (method->type != TAF_VIRTUAL_MARKUP)
#endif // endif
		{
			WL_TAFM(method, MACF" tid %u %s: total units %u, rel_units %u, "
				"%u real (%u n) / %u virt / %u pend sent, stop : %s%s\n",
				TAF_ETHERC(scb_taf), tid, TAF_SOURCE_NAME(s_idx),
				tid_state->total.released_units, released_units,
				actual_release, release_n, virtual_release, pending_release,
				taf_rel_complete_text[context->public.complete],
				context->public.ias.was_emptied ? " E":"");
		}
	} else {
		WL_TAFM(method, MACF" %s: no release (%u/%u) reason: %s\n", TAF_ETHERC(scb_taf),
			TAF_SOURCE_NAME(s_idx), tid_state->total.released_units, time_limit_units,
			taf_rel_complete_text[context->public.complete]);
	}

	/* the following is statistics across the TID state which is global across all SCB using
	 * that TID
	 */
	tid_state->total.released_units += released_units;
	tid_state->extend.units += extend_units;

#if TAF_ENABLE_SQS_PULL
	if (virtual_release) {
		TAF_ASSERT(TAF_SOURCE_IS_VIRTUAL(s_idx));
		context->virtual_release += virtual_release;
		scb_taf->info.pkt_pull_request[tid] = virtual_release;
		scb_taf->info.pkt_pull_map |= (1 << tid);
		taf_info->total_pull_requests++;
#if TAF_LOGL1
		WL_TAFT(taf_info, "total_pull_requests %u\n", taf_info->total_pull_requests);
#endif // endif
	}
	context->pending_release += pending_release;
#endif /* TAF_ENABLE_SQS_PULL */

	if (actual_release) {
#if TAF_ENABLE_SQS_PULL
		TAF_ASSERT(TAF_SOURCE_IS_REAL(s_idx));
#endif /* TAF_ENABLE_SQS_PULL */

		tid_state->real.released_units += released_units;
		tid_state->total_units_in_transit += released_units;

		/* the following statistics are per SCB,TID link */
		scbtidstats->data.release_pcount += actual_release;
		scbtidstats->data.release_units += released_units;
		if (type == TAF_TYPE_DL) {
			scbtidstats->data.release_bytes += released_bytes;
			scbtidstats->data.release_ncount += release_n;
		}

		/* this is global across the whole scheduling interval */
		context->actual_release += actual_release;
		context->actual_release_n += release_n;

	} else if (TAF_SOURCE_IS_AMPDU(s_idx) &&
		context->public.complete == TAF_REL_COMPLETE_NOTHING_AGG &&
		taf_info->op_state != TAF_SCHEDULE_VIRTUAL_PACKETS) {

		TAF_DPSTATS_LOG_ADD(scbtidstats, held_z, 1);
	}
	switch (context->public.complete) {
		case TAF_REL_COMPLETE_RESTRICTED:
			TAF_DPSTATS_LOG_ADD(scbtidstats, restricted, 1);
			/* fall through */
		case TAF_REL_COMPLETE_FULL:
		case TAF_REL_COMPLETE_BLOCKED:
		case TAF_REL_COMPLETE_NO_BUF:
		case TAF_REL_COMPLETE_ERR:
			TAF_DPSTATS_LOG_ADD(scbtidstats, error, 1);
			context->status = TAF_CYCLE_FAILURE;
			break;

		case TAF_REL_COMPLETE_REL_LIMIT:
			return TRUE;
			break;
		default:
			break;
	}

	return context->public.ias.was_emptied;
}

static INLINE void BCMFASTPATH
taf_ias_mean_pktlen(taf_ias_sched_scbtid_stats_t* scbtidstats, uint32 timestamp,
	taf_ias_coeff_t* decay)
{
	/*
	* This calculates the average packet size from what has been released. It is going to be
	* used under SQS, to forward predict how many virtual packets to be converted.
	*/

	if (scbtidstats->data.release_ncount) {
		uint32 mean;
		uint32 coeff = 0;
		uint32 elapsed = timestamp - scbtidstats->data.calc_timestamp;
		int32 time_correction = 0;

		coeff = taf_ias_decay_score(TAF_COEFF_IAS_MAX, decay, elapsed, &time_correction);

		if (coeff < TAF_COEFF_IAS_MAX) {
			uint64 calc;
			scbtidstats->data.calc_timestamp = timestamp - time_correction;

			/* may overflow 32 bit int, use 64 bit */
			calc = ((uint64)scbtidstats->data.total_scaled_ncount *
				(uint64)coeff);
			calc += (TAF_COEFF_IAS_MAX >> 1);
			calc >>= TAF_COEFF_IAS_MAX_BITS;
			scbtidstats->data.total_scaled_ncount = calc;

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
		scbtidstats->data.total_scaled_ncount += (scbtidstats->data.release_ncount
				<< TAF_NCOUNT_SCALE_SHIFT);
		scbtidstats->data.total_bytes += scbtidstats->data.release_bytes;

		/*
		* XXX avoid 64 bit divide, use standard 32 bit, so TAF_PCOUNT_SCALE_SHIFT
		* can't be too big
		*/

		TAF_ASSERT(scbtidstats->data.total_bytes < (1 << (32 - TAF_NCOUNT_SCALE_SHIFT)));

		mean = scbtidstats->data.total_bytes << TAF_NCOUNT_SCALE_SHIFT;

		TAF_ASSERT(scbtidstats->data.total_scaled_ncount);

		mean /= (scbtidstats->data.total_scaled_ncount);

		scbtidstats->data.release_pkt_size_mean = mean;
		TAF_DPSTATS_LOG_SET(scbtidstats, release_pkt_size_mean, mean);
	};
}

static INLINE void BCMFASTPATH
taf_ias_sched_update(taf_method_info_t *method, taf_scb_cubby_t *scb_taf,
		int tid, taf_schedule_tid_state_t *tid_state,
		taf_release_context_t *context, taf_list_type_t type)

{
	taf_ias_sched_data_t* ias_sched = TAF_IAS_TID_STATE(scb_taf, tid);
	taf_ias_sched_scbtid_stats_t* scbtidstats = ias_sched ? &ias_sched->scbtidstats : NULL;
	uint32 new_score;
	uint32 rel_units;

	/* clear the force status */
	scb_taf->force &= ~(1 << tid);

#if TAF_ENABLE_SQS_PULL
	if (scb_taf->info.pkt_pull_map == 0) {
		/* reset release limit tracking after pull cycle has completed */
		scb_taf->info.released_units_limit[tid] = 0;
	}
#endif /* TAF_ENABLE_SQS_PULL */

	TAF_ASSERT(ias_sched && scbtidstats);

	if (!scbtidstats->data.release_pcount) {
		return;
	}

	if (method->taf_info->scheduler_index != scbtidstats->index) {
		TAF_DPSTATS_LOG_ADD(scbtidstats, release_frcount, 1);
		scbtidstats->index = method->taf_info->scheduler_index;
		/* record the release time */
		scb_taf->timestamp = taf_timestamp(TAF_WLCM(method));
	}

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
			new_score = taf_units_to_score(method, scb_taf, tid,
				scbtidstats->data.release_pcount << 6);
			scb_taf->score[type] = MIN(scb_taf->score[type] + new_score,
				TAF_SCORE_IAS_MAX);
			break;

		case TAF_ATOS:
		case TAF_ATOS2:
			/* this is scheduled airtime scoring */

			rel_units = scbtidstats->data.release_units;

			new_score = taf_units_to_score(method, scb_taf, tid, rel_units);

			scb_taf->score[type] = MIN(scb_taf->score[type] + new_score,
				TAF_SCORE_IAS_MAX);
			break;

		default:
			TAF_ASSERT(0);
	}

	if (type == TAF_TYPE_DL) {
		taf_ias_mean_pktlen(scbtidstats, scb_taf->timestamp, &method->ias->coeff);
	}

	TAF_DPSTATS_LOG_ADD(scbtidstats, release_pcount, scbtidstats->data.release_pcount);
	TAF_DPSTATS_LOG_ADD(scbtidstats, release_ncount, scbtidstats->data.release_ncount);
	TAF_DPSTATS_LOG_ADD(scbtidstats, release_time,
		(uint64)TAF_UNITS_TO_MICROSEC(scbtidstats->data.release_units));
	TAF_DPSTATS_LOG_ADD(scbtidstats, release_bytes, (uint64)scbtidstats->data.release_bytes);

	if (context->public.ias.was_emptied) {
		TAF_DPSTATS_LOG_ADD(scbtidstats, emptied, 1);
	}
#if TAF_LOGL2
	WL_TAFM(method, MACF" tid %u rate %u, rel %u pkts (%u npkts) %u us, "
		"total %u us\n",
		TAF_ETHERC(scb_taf), tid,
#if TAF_ENABLE_MU_TX
		RSPEC2KBPS(scb_taf->info.scb_stats.global.data.rspec
		[scb_taf->info.scb_stats.ias.data.ridx_used[type]]),
#else
		RSPEC2KBPS(scb_taf->info.scb_stats.global.data.rspec[TAF_RSPEC_SU_DL]),
#endif // endif
		scbtidstats->data.release_pcount, scbtidstats->data.release_ncount,
		TAF_UNITS_TO_MICROSEC(scbtidstats->data.release_units),
		TAF_UNITS_TO_MICROSEC(tid_state->total.released_units));
#endif /* TAF_LOGL2 */
	/* reset before next scheduler invocation */
	scbtidstats->data.release_pcount = 0;
	scbtidstats->data.release_bytes = 0;
	scbtidstats->data.release_units = 0;
	scbtidstats->data.release_ncount = 0;
}

static INLINE void taf_ias_sched_clear(taf_scb_cubby_t *scb_taf, taf_release_context_t *context,
	taf_rspec_index_t rindex)
{
	context->public.how = TAF_RELEASE_LIKE_IAS;

#if TAF_ENABLE_SQS_PULL
	context->public.ias.virtual.released_units = 0;
	context->public.ias.virtual.release = 0;
#endif /* TAF_ENABLE_SQS_PULL */
	context->public.ias.actual.released_units = 0;
	context->public.ias.actual.released_bytes = 0;
	context->public.ias.actual.release = 0;

	context->public.ias.byte_rate = scb_taf->info.scb_stats.ias.data.byte_rate[rindex];
	context->public.ias.pkt_rate = scb_taf->info.scb_stats.ias.data.pkt_rate[rindex];
}

static INLINE bool BCMFASTPATH
taf_ias_sched_send(taf_method_info_t *method, taf_scb_cubby_t *scb_taf, int tid,
	taf_schedule_tid_state_t *tid_state, taf_release_context_t *context,
	taf_schedule_state_t op_state, taf_list_type_t type)
{
	wlc_taf_info_t* taf_info = method->taf_info;
	uint32 time_limit_units = tid_state->extend.units +
		TAF_MICROSEC_TO_UNITS(tid_state->high[IAS_INDEXM(method)]);
#if TAF_ENABLE_SQS_PULL
	const taf_source_type_t start_source = TAF_FIRST_REAL_SOURCE;
	const taf_source_type_t end_source = (op_state == TAF_SCHEDULE_VIRTUAL_PACKETS) ?
		TAF_NUM_SCHED_SOURCES : TAF_NUM_REAL_SOURCES;
	uint16 v_avail = scb_taf->info.traffic.count[TAF_SOURCE_HOST_SQS][tid];
	uint32 v_prior_released = 0;
	uint32 real_prior_released = 0;
	uint32 pre_release_skip = 0;
#else /* TAF_ENABLE_SQS_PULL */
	const taf_source_type_t start_source = TAF_FIRST_REAL_SOURCE;
	const taf_source_type_t end_source = TAF_NUM_REAL_SOURCES;
#endif /* TAF_ENABLE_SQS_PULL */
	const bool force = (scb_taf->force & (1 << tid)) ? (taf_info->force_time > 0) : FALSE;
	taf_source_type_t s_idx;
	taf_rspec_index_t rindex;

#if TAF_ENABLE_MU_TX
	switch (scb_taf->info.mu_type[tid]) {
		case TAF_TECH_DL_HEMUMIMO:
			context->public.type = TAF_REL_TYPE_HEMUMIMO; break;
		case TAF_TECH_DL_VHMUMIMO:
			context->public.type = TAF_REL_TYPE_VHMUMIMO; break;
		case TAF_TECH_DL_OFDMA:
			context->public.type = TAF_REL_TYPE_OFDMA;    break;
#if TAF_ENABLE_UL
		case TAF_TECH_UL_OFDMA:
			context->public.type = TAF_REL_TYPE_ULOFDMA;  break;
#endif /* TAF_ENABLE_UL */
		default:
			context->public.type = TAF_REL_TYPE_SU;       break;
	}

#if TAF_ENABLE_UL
	if (type == TAF_TYPE_UL) {
		rindex = TAF_RSPEC_UL;
	} else
	/* fall through */
#endif // endif
	if (taf_info->use_sampled_rate_sel & (1 << scb_taf->info.mu_type[tid])) {
		rindex = (scb_taf->info.mu_type[tid] == TAF_TECH_DL_SU) ?
			TAF_RSPEC_SU_DL_INSTANT : TAF_RSPEC_MU_DL_INSTANT;
	} else {
		rindex = TAF_RSPEC_SU_DL;
	}
	scb_taf->info.scb_stats.ias.data.ridx_used[type] = rindex;
#else
	rindex = TAF_RSPEC_SU_DL;
#endif /* TAF_ENABLE_MU_TX */

	context->public.release_type_present |= (1 << context->public.type);

	taf_ias_sched_clear(scb_taf, context, rindex);

#if TAF_ENABLE_SQS_PULL
	/* init the scb tracking for release limiting */
	if ((op_state == TAF_SCHEDULE_VIRTUAL_PACKETS) &&
		(scb_taf->info.released_units_limit[tid] == 0)) {

		scb_taf->info.released_units_limit[tid] = force ?
			TAF_MICROSEC_TO_UNITS(taf_info->force_time) :

			/* access method ptr via scb_taf for SQS_PULL */
			TAF_MICROSEC_TO_UNITS(scb_taf->method->ias->release_limit);
	}
	if (scb_taf->info.released_units_limit[tid] >= 0) {
		context->public.ias.released_units_limit = scb_taf->info.released_units_limit[tid];
	} else {
#if TAF_LOGL2
		/* limit already reached (set to -1 in taf_ias_sched_update) */
		WL_TAFM(method, MACF" release limit reached\n", TAF_ETHERC(scb_taf));
#endif // endif
		/* have to return TRUE here (as if emptied) so IAS continues with next station */
		return TRUE;
	}
#else
	if (force) {
		context->public.ias.released_units_limit =
			TAF_MICROSEC_TO_UNITS(taf_info->force_time);
	} else {
		if (scb_taf->info.released_units_limit >= 0) {
			context->public.ias.released_units_limit =
				scb_taf->info.released_units_limit[tid];
		} else {
			context->public.ias.released_units_limit =
				TAF_MICROSEC_TO_UNITS(method->ias->release_limit);
		}
	}
#endif /* TAF_ENABLE_SQS_PULL */

	context->public.ias.time_limit_units = time_limit_units;

	for (s_idx = start_source; s_idx < end_source; s_idx++) {
		bool emptied;
		uint16 pending;

		TAF_ASSERT(s_idx >= 0);

		if (!(scb_taf->info.scb_stats.ias.data.use[s_idx])) {
			continue;
		}
		if (scb_taf->info.linkstate[s_idx][tid] != TAF_LINKSTATE_ACTIVE) {
			continue;
		}
		if (!taf_src_type_match(s_idx, type)) {
			continue;
		}
#if TAF_ENABLE_SQS_PULL
		if (op_state == TAF_SCHEDULE_VIRTUAL_PACKETS) {
#if TAF_ENABLE_MU_TX
			const bool is_mu = scb_taf->info.mu_type[tid] < TAF_NUM_MU_TECH_TYPES;
#else
			const bool is_mu = FALSE;
#endif /* TAF_ENABLE_MU_TX */

			if ((pending = scb_taf->info.traffic.count[s_idx][tid]) == 0) {
				continue;
			}

			/* multi-stage if to make it less complex to handle real packets
			 * already available
			 */
			if (TAF_SOURCE_IS_VIRTUAL(s_idx)) {
				v_prior_released = context->virtual_release;
				/* do release */
			} else if (TAF_SOURCE_IS_UL(s_idx)) {
				/* do release */
			} else if (TAF_SOURCE_IS_NAR(s_idx)) {
				/* do release */
			} else if (scb_taf->info.ps_mode || force || (v_avail == 0) ||
				((pending >= method->ias->pre_rel_limit[s_idx]) && !is_mu)) {
				/* do release */
			} else if (SCB_PKTS_INFLT_FIFOCNT_VAL(scb_taf->scb, tid) == 0 && !is_mu) {
				/* do release */
			} else {
				int32 remain_limit_units = time_limit_units -
					tid_state->total.released_units;
				uint32 est_rel =
					taf_ias_est_release_time(scb_taf,
					TAF_IAS_TID_STATE(scb_taf, tid), pending, type);

				if (context->public.ias.released_units_limit > 0) {
					remain_limit_units = MIN(remain_limit_units,
						context->public.ias.released_units_limit);
				}

				if (est_rel < remain_limit_units) {
					/* skip this release */
					pre_release_skip++;
#if TAF_LOGL4
					WL_TAFM(method, "skip real vphase rel to "MACF" tid %u %s "
						"only %u pending (thresh %u), v_avail %u, ps %u,"
						"est_rel %u, remain limit %u, mu %u\n",
						TAF_ETHERC(scb_taf), tid, TAF_SOURCE_NAME(s_idx),
						pending, method->ias->pre_rel_limit[s_idx], v_avail,
						scb_taf->info.ps_mode, est_rel, remain_limit_units,
						is_mu);
#endif // endif

					/* For IAS purpose, consider this is emptied */
					scb_taf->info.traffic.map[s_idx] &= ~(1 << tid);
					continue;
				}
			}

			if (TAF_SOURCE_IS_REAL(s_idx)) {
				if (real_prior_released == 0) {
					real_prior_released = context->actual_release;
				}
#if TAF_LOGL2
				WL_TAFM(method, "try real vphase rel to "MACF" tid %u %s "
					"%u pending, v_avail %u, ps %u "
					"force %u tx-in-transit %u, mu %u\n",
					TAF_ETHERC(scb_taf), tid, TAF_SOURCE_NAME(s_idx),
					pending, v_avail, scb_taf->info.ps_mode, force,
					SCB_PKTS_INFLT_FIFOCNT_VAL(scb_taf->scb, tid), is_mu);
#endif // endif
			}
		}
#else
		if ((pending = scb_taf->info.traffic.count[s_idx][tid]) == 0) {
			WL_TAFM(method, MACF" tid %u %s pending 0\n",
				TAF_ETHERC(scb_taf), tid, TAF_SOURCE_NAME(s_idx));
			continue;
		}
#endif /* TAF_ENABLE_SQS_PULL */

		emptied = taf_ias_send_source(method, scb_taf, tid,
			s_idx, tid_state, context, type);
#if TAF_ENABLE_SQS_PULL
		if (TAF_SOURCE_IS_VIRTUAL(s_idx) && (v_avail > 0) &&
				(context->virtual_release - v_prior_released == 0) &&
				(context->actual_release - real_prior_released == 0) &&
				(scb_taf->info.traffic.count[TAF_SOURCE_AMPDU][tid] != 0) &&
				(SCB_PKTS_INFLT_FIFOCNT_VAL(scb_taf->scb, tid) == 0)) {

				/* no virtual traffic released & no real traffic was pre-released */
				WL_TAFM(method, MACF" tid %u pre-release not completed and without "
					"virtual rel (virtual avail %u, ampdu avail %u); "
					"send ampdu once again....\n",
					TAF_ETHERC(scb_taf), tid, v_avail,
					scb_taf->info.traffic.count[TAF_SOURCE_AMPDU][tid]);

				scb_taf->info.traffic.count[s_idx][tid] = 0;
				scb_taf->info.traffic.map[s_idx] &= ~(1 << tid);
				v_avail = 0;
				/* go around again... NB: loop counter s_idx will be ++ */
				s_idx = start_source - 1;
				continue;
			}
#endif /* TAF_ENABLE_SQS_PULL */

		if (emptied) {
			scb_taf->info.traffic.map[s_idx] &= ~(1 << tid);
		}

		if (scb_taf->info.released_units_limit[tid] > 0 && TAF_SOURCE_IS_REAL(s_idx)) {
			taf_ias_sched_data_t* ias_sched = TAF_IAS_TID_STATE(scb_taf, tid);
			taf_ias_sched_scbtid_stats_t* scbtidstats = &ias_sched->scbtidstats;

			if (scb_taf->info.released_units_limit[tid] >
					scbtidstats->data.release_units) {
				scb_taf->info.released_units_limit[tid] -=
					scbtidstats->data.release_units;
			} else {
				/* minus -1 to flag release limit already reached */
				scb_taf->info.released_units_limit[tid] = -1;
				TAF_DPSTATS_LOG_ADD(scbtidstats, limited, 1);
				WL_TAFM(method, MACF" tid %u release limit reached\n",
					TAF_ETHERC(scb_taf), tid);
				return TRUE;
			}
		}
	}
	scb_taf->info.traffic.available[type] = 0;
	for (s_idx = TAF_FIRST_REAL_SOURCE; s_idx < TAF_NUM_SCHED_SOURCES; s_idx++) {
		if (!taf_src_type_match(s_idx, type)) {
			continue;
		}
		scb_taf->info.traffic.available[TAF_SRC_TO_TYPE(s_idx)] |=
			scb_taf->info.traffic.map[s_idx];
	}
	/* return whether SCB, TID was emptied (TRUE) across all sources (or not) */
	return (scb_taf->info.traffic.available[type] & (1 << tid)) == 0;
}

static bool BCMFASTPATH
taf_ias_schedule_scb(taf_method_info_t *method, taf_schedule_tid_state_t *tid_state,
	int tid, taf_release_context_t *context, taf_schedule_state_t op_state)
{
	uint32 high_units = TAF_MICROSEC_TO_UNITS(tid_state->high[IAS_INDEXM(method)]);
	uint32 low_units = TAF_MICROSEC_TO_UNITS(tid_state->low[IAS_INDEXM(method)]);
	taf_list_t *list;

#if TAF_LOGL2
	WL_TAFM(method, "total est units %u (total in transit %u)\n",
		tid_state->est_release_units, tid_state->total_units_in_transit);
#endif // endif

	for (list = method->list; list; list = list->next) {
		taf_scb_cubby_t *scb_taf = list->scb_taf;
		taf_ias_sched_data_t* ias_sched;
		bool emptied;

		if (!(scb_taf->info.traffic.available[list->type] & (1 << tid))) {
			scb_taf->force &= ~(1 << tid);
#if TAF_LOGL3
			WL_TAFM(method, "skip "MACF" tid %u, no traffic\n",
				TAF_ETHERC(scb_taf), tid);
#endif // endif
			continue;
		}

		if (!(ias_sched = TAF_IAS_TID_STATE(scb_taf, tid))) {
			scb_taf->force &= ~(1 << tid);
#if TAF_LOGL3
			WL_TAFM(method, "skip "MACF" tid %u, no context\n",
				TAF_ETHERC(scb_taf), tid);
#endif // endif
			continue;
		}

		if (scb_taf->info.ps_mode) {
			TAF_DPSTATS_LOG_ADD(&ias_sched->scbtidstats, pwr_save, 1);

			/* cannot force so clear the force status */
			scb_taf->force &= ~(1 << tid);
		}
		if (TAF_OPT(method, EXIT_EARLY) &&
#if TAF_ENABLE_MU_TX
			scb_taf->info.mu_type[tid] == TAF_TECH_UNASSIGNED &&
#endif // endif
			tid_state->total.released_units >= (low_units + tid_state->extend.units) &&
			(scb_taf->info.traffic.est_units[tid] >
			(high_units + tid_state->extend.units - tid_state->total.released_units))) {
			WL_TAFM(method, MACF" tid %u has %u traffic, space remaining %u, "
				"exit cycle now\n", TAF_ETHERC(scb_taf), tid,
				scb_taf->info.traffic.est_units[tid],
				(high_units + tid_state->extend.units -
				tid_state->total.released_units));
			return TRUE;
		}

#if TAF_ENABLE_MU_TX
		if ((method->type == TAF_ATOS || method->type == TAF_ATOS2)) {
			taf_ias_analyse_mu(method, list, tid_state, tid, context);

			if (scb_taf->info.mu_type[tid] == TAF_TECH_DONT_ASSIGN) {
				WL_TAFM(method, "skipping "MACF" tid % due to MU optimisation\n",
					TAF_ETHERC(scb_taf), tid);
				continue;
			}
		}
#endif /* TAF_ENABLE_MU_TX */

		emptied = taf_ias_sched_send(method, scb_taf, tid, tid_state,
			context, op_state, list->type);

		taf_ias_sched_update(method, scb_taf, tid, tid_state, context, list->type);

		if (!emptied) {
			return TRUE;
		}
		if (tid_state->total.released_units >= (high_units + tid_state->extend.units)) {
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
		ias_info->tid_state[tid].immed = ias_info->unified_tid_state.immed;
	}
#endif /* TAF_ENABLE_SQS_PULL */

	WL_TAFM(method, "high = %us, low = %uus, immed = %u counts\n",
		method->ias->high, method->ias->low, ias_info->unified_tid_state.immed);

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
		int tid = tid_service_order[tid_index];

		if (method->ias->sched_active & (1 << tid)) {
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
	wlc_taf_info_t* taf_info = method->taf_info;
	taf_ias_group_info_t* ias_info = TAF_IAS_GROUP_INFO(taf_info);
#if TAF_ENABLE_SQS_PULL
	bool virtual_scheduling = (op_state == TAF_SCHEDULE_VIRTUAL_PACKETS &&
		method->type != TAF_VIRTUAL_MARKUP);
	bool v2r_pending = (virtual_scheduling && (context->virtual_release > 0));
#endif /* TAF_ENABLE_SQS_PULL */

	if (!finished) {
		finished = (method->type == LAST_IAS_SCHEDULER);
	}

#if TAF_ENABLE_SQS_PULL
	if (virtual_scheduling) {
		if (finished) {
			WL_TAFT(taf_info, "vsched phase exit %uus real, %uus "
				"virtual (%u rpkts / %u vpkts / %u ppkts)%s pulls %u\n",
				TAF_UNITS_TO_MICROSEC(tid_state->real.released_units),
				TAF_UNITS_TO_MICROSEC(tid_state->total.released_units -
					tid_state->real.released_units),
				context->actual_release, context->virtual_release,
				context->pending_release,
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

	if (tid_state->total.released_units != tid_state->real.released_units) {
#if TAF_LOGL3
		WL_TAFM(method, "total.released_units %u, real.released_units %u, op_state %d, "
			"finished %u, actual release %u (%u n), virtual release %u, pending "
			"release %u\n",
			tid_state->total.released_units, tid_state->real.released_units,
			op_state, finished, context->actual_release, context->actual_release_n,
			context->virtual_release, context->pending_release);
#endif // endif
	}

	if (finished && (tid_state->real.released_units > 0)) {
		uint32 low_units = TAF_MICROSEC_TO_UNITS(tid_state->low[IAS_INDEXM(method)]);
		uint32 high_units = TAF_MICROSEC_TO_UNITS(tid_state->high[IAS_INDEXM(method)]);
		uint32 duration;
		uint8 cnext = tid_state->cycle_next;
		uint8 cnow = tid_state->cycle_now;
#if !TAF_ENABLE_SQS_PULL
		taf_method_info_t *scheduler = method;
#else
		taf_method_info_t *scheduler = taf_get_method_info(method->taf_info,
			TAF_SCHEDULER_START);
#endif /* !TAF_ENABLE_SQS_PULL */

		if (tid_state->extend.units) {
			if (tid_state->real.released_units > high_units) {
				high_units = MIN(tid_state->real.released_units,
					high_units + tid_state->extend.units);
			}
			if (tid_state->real.released_units > low_units) {
				low_units += high_units -
					TAF_MICROSEC_TO_UNITS(tid_state->high[IAS_INDEXM(method)]);
			}
		}
#if TAF_LOGL3
		WL_TAFM(method, "high %uus, low %uus, immed %uus, extend %uus\n",
			TAF_UNITS_TO_MICROSEC(high_units),
			TAF_UNITS_TO_MICROSEC(low_units),
			tid_state->immed,
			TAF_UNITS_TO_MICROSEC(tid_state->extend.units));
#endif // endif

		WL_TAFT(taf_info, "tuit %u\n", tid_state->total_units_in_transit);

		if (tid_state->real.released_units < high_units) {
			tid_state->data.uflow_high++;
			tid_state->debug.uflow_high++;
		} else {
			tid_state->data.target_high++;
			tid_state->debug.target_high++;
		}

		tid_state->debug.average_high += tid_state->real.released_units;

		if (TAF_OPT(method, TOOMUCH) && (low_units > 0) &&
				(tid_state->real.released_units < low_units) &&
				(tid_state->trigger == TAF_TRIGGER_IMMEDIATE) &&
				(tid_state->total_units_in_transit > high_units) &&
				(cnext == 0) && (cnow == 0)) {

			/* scheduler triggering runaway prevention */
			WL_TAFT(taf_info, "too much immediate trig traffic in transit "
				"%uus, %s barrier (%u)\n",
				TAF_UNITS_TO_MICROSEC(tid_state->total_units_in_transit),
				tid_state->barrier_req == 0 ? "request" : "insert",
				tid_state->barrier_req);

			if (tid_state->barrier_req == 0) {
				tid_state->barrier_req = 1;
			} else {
				low_units = tid_state->real.released_units - 1;
				tid_state->data.uflow_low++;
				tid_state->debug.uflow_low++;
			}
		}

		if (tid_state->real.released_units > low_units) {
			tid_state->resched_units[cnext] =
				tid_state->real.released_units - low_units;
			if (tid_state->barrier_req == 0) {
				tid_state->data.target_low++;
				tid_state->debug.target_low++;
			} else {
				tid_state->barrier_req = 0;
			}
		} else {
			/* TX window underflow */
			tid_state->data.uflow_low++;
			tid_state->debug.uflow_low++;

			if (cnext == 1) {
				WL_TAFT(taf_info, "cycle_next is 1 and low underflow, wait for "
					"all traffic to clear\n");
				/* this will wait to clear out entire data path to allow
				 * super-scheduling to resume again- prevent run-away
				 */
				tid_state->cycle_next = cnext = 0;
				tid_state->resched_units[0] = tid_state->real.released_units;
				tid_state->barrier_req = 0;
				context->status = TAF_CYCLE_FAILURE;
			} else if (cnow == 1) {
				WL_TAFT(taf_info, "cycle_now is 1 and low underflow\n");
				tid_state->resched_units[0] = 1;
			} else if (ias_info->ncount_flow < tid_state->immed) {
				/* infrequent, small volume data */
				WL_TAFT(taf_info, "star pkt instead of immed trigger (%u, %u)\n",
					ias_info->ncount_flow, tid_state->immed);
				tid_state->resched_units[0] =  TAF_OPT(method, IMMED_LOOSE) ?
					tid_state->real.released_units - 1 : 1;
				tid_state->debug.immed_star++;
				tid_state->data.immed_star++;
			} else {
				/* immediate re-trigger mode */
				tid_state->resched_units[0] = 0;
				tid_state->debug.immed_trig++;
				tid_state->data.immed_trig++;
			}
		}

		tid_state->resched_index[cnext]  = context->public.ias.index;
		duration = (now_time - tid_state->sched_start);
		tid_state->debug.sched_duration += duration;

		if (duration > tid_state->debug.max_duration) {
			tid_state->debug.max_duration = duration;
		}

		tid_state->sched_start = 0;

		if (tid_state->resched_units[cnext] > 0) {
			bool star_pkt_seen;

			star_pkt_seen = tid_state->cumul_units[tid_state->resched_index[cnext]] >=
				tid_state->resched_units[cnext];

			if (taf_info->super && cnext && tid_state->cycle_ready == 0) {
				tid_state->cycle_ready = 1;
			}

			if (star_pkt_seen) {
				WL_TAFT(taf_info, "final exit %uus scheduled, trigger %u:%u "
					"already seen (prev rel is %u, in transit %u), sched dur "
					"%uus, r_t_s 0x%x\n\n",
					TAF_UNITS_TO_MICROSEC(tid_state->real.released_units),
					tid_state->resched_index[cnext],
					tid_state->resched_units[cnext],
					tid_state->data.time_delta,
					TXPKTPENDTOT(TAF_WLCT(taf_info)),
					duration, *scheduler->ready_to_schedule);
				tid_state->trigger = TAF_TRIGGER_NONE;
			} else {
				uint32 est_traffic = tid_state->est_release_units;
				bool extra_cycle = FALSE;

				if (est_traffic > tid_state->real.released_units) {
					est_traffic -= tid_state->real.released_units;
				} else {
					est_traffic = 0;
				}

				if (taf_info->super && cnext == 0 && cnow == 0 &&
						est_traffic > high_units &&
						tid_state->real.released_units >= low_units &&
						context->status == TAF_CYCLE_INCOMPLETE) {
					WL_TAFT(taf_info, "%u units traffic available for "
						"super-scheduling\n", est_traffic);
					/* insert an extra cycle */
					extra_cycle = TRUE;
					tid_state->cycle_ready = 0;
				}

				if (TAF_UNITED_ORDER(taf_info)) {
					*scheduler->ready_to_schedule = extra_cycle ? ~0 : 0;
				} else if (!extra_cycle) {
					*scheduler->ready_to_schedule &= ~(1 << context->tid);
				} else {
					*scheduler->ready_to_schedule |= (1 << context->tid);
				}

				WL_TAFT(taf_info, "final exit %uus scheduled "
					"%s trig %u:%uus (prev rel is %u, in transit "
					"%u), sched dur %uus, r_t_s 0x%x%s\n\n",
					TAF_UNITS_TO_MICROSEC(tid_state->real.released_units),
					cnext ? "next" : "now",
					tid_state->resched_index[cnext],
					TAF_UNITS_TO_MICROSEC(tid_state->resched_units[cnext]),
					tid_state->data.time_delta,
					TXPKTPENDTOT(TAF_WLCT(taf_info)),
					duration, *scheduler->ready_to_schedule,
					extra_cycle ? " RESCHED" : "");

				tid_state->trigger = TAF_TRIGGER_STAR_THRESHOLD;

				if (extra_cycle) {
					tid_state->cycle_next = cnext = 1;
				} else if (taf_info->super && cnext == 1 && cnow == 1) {
					tid_state->cycle_now = cnow = 0;
				}
			}
		} else {
			WL_TAFT(taf_info, "final exit %uus scheduled, immed "
				"retrig (%u) (prev rel is %u, in transit %u), "
				"sched dur %uus, r_t_s 0x%x\n\n",
				TAF_UNITS_TO_MICROSEC(tid_state->real.released_units),
				tid_state->resched_index[cnext],
				tid_state->data.time_delta, TXPKTPENDTOT(TAF_WLCT(taf_info)),
				duration, *scheduler->ready_to_schedule);
			tid_state->trigger = TAF_TRIGGER_IMMEDIATE;
		}
		tid_state->total_sched_units[context->public.ias.index] =
			tid_state->real.released_units;

		taf_info->release_count ++;

		/* mask index because the pkt tag only has few bits */
		ias_info->tag_star_index =
			(ias_info->tag_star_index + 1) & TAF_MAX_PKT_INDEX;

	} else if (finished) {
		WL_TAFT(taf_info, "final exit nothing scheduled, immed "
			"retrig (%u) (prev rel is %u, in transit %u)\n\n",
			tid_state->resched_index[tid_state->cycle_next],
			tid_state->data.time_delta, TXPKTPENDTOT(TAF_WLCT(taf_info)));
		tid_state->trigger = TAF_TRIGGER_IMMEDIATE;
	}

	if (finished) {
		tid_state->real.released_units = 0;
		tid_state->total.released_units = 0;
		tid_state->extend.units = 0;
		tid_state->est_release_units = 0;
		tid_state->waiting_schedule = 0;

#ifdef TAF_DBG
		ias_info->ncount_flow_last = ias_info->ncount_flow;
#endif // endif
		ias_info->ncount_flow = 0;

		if (taf_info->super) {
			WL_TAFT(taf_info, "super sched: wait %u/%u, cycle now %u; cycle next "
				"%u (%u/%u), ready %u, wait sched %u\n\n",
				tid_state->resched_index[0],
				TAF_UNITS_TO_MICROSEC(tid_state->resched_units[0]),
				tid_state->cycle_now, tid_state->cycle_next,
				tid_state->resched_index[1],
				TAF_UNITS_TO_MICROSEC(tid_state->resched_units[1]),
				tid_state->cycle_ready,
				tid_state->waiting_schedule);
		}
		tid_state->star_packet_received = FALSE;

		context->status = TAF_CYCLE_COMPLETE;
	}
	return finished;
}

static bool taf_ias_data_block(taf_method_info_t *method, uint32 now_time)
{
#ifdef TAF_DBG
	wlc_taf_info_t *taf_info = method->taf_info;
	taf_ias_group_info_t* ias_info = TAF_IAS_GROUP_INFO(taf_info);
#endif /* TAF_DBG */

	if (method->ias->data_block) {
#ifdef TAF_DBG
		if (ias_info->data_block_start == 0) {
			ias_info->data_block_start = now_time;
			ias_info->data_block_prev_in_transit =
				TXPKTPENDTOT(TAF_WLCT(taf_info));
			WL_TAFM(method, "exit due to data_block (START)\n");
		}
		if (TXPKTPENDTOT(TAF_WLCT(taf_info)) !=
				ias_info->data_block_prev_in_transit) {
			ias_info->data_block_prev_in_transit =
				TXPKTPENDTOT(TAF_WLCT(taf_info));
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
	return FALSE;
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
		WL_ERROR(("wl%u %s: NOT IN MARKUP PHASE\n",
			WLCWLUNIT(TAF_WLCT(taf_info)), __FUNCTION__));
		TAF_ASSERT(0);
		return FALSE;
	}

	if (!v2r_event && scb_taf &&
			(((scb_taf->info.pkt_pull_map & (1 << tid)) == 0) ||
			(scb_taf->info.pkt_pull_dequeue == 0))) {
#if TAF_LOGL3
		WL_TAFM(method, MACF" tid %u is not set for markup (0x%u) or no new dequeued "
			"packets (%u) %s\n",
			TAF_ETHERC(scb_taf), tid, scb_taf->info.pkt_pull_map,
			scb_taf->info.pkt_pull_dequeue,
			scb_taf->info.ps_mode ? "in PS mode":"- exit");
#endif // endif
		if (!scb_taf->info.ps_mode) {
			return TRUE;
		}
	}

	if (!v2r_event && !(scb_taf->info.tid_enabled & (1 << tid))) {
		/* this is an assertfail level problem */
		WL_ERROR(("wl%u %s: tid %u not enabled (0x%x/0x%x/%u/%u)\n",
			WLCWLUNIT(TAF_WLCT(taf_info)), __FUNCTION__,
			tid, scb_taf->info.tid_enabled,
			scb_taf->info.pkt_pull_map,
			scb_taf->info.pkt_pull_dequeue,
			scb_taf->info.ps_mode));
		TAF_ASSERT(v2r_event || (scb_taf->info.tid_enabled & (1 << tid)));
		return TRUE;
	}
	TAF_ASSERT(TAF_IAS_TID_STATE(scb_taf, tid));

	tid_state = taf_get_tid_state(taf_info, TAF_TID(tid, taf_info));
	ias_info = TAF_IAS_GROUP_INFO(taf_info);
	context->public.ias.index = ias_info->tag_star_index;

	if (!v2r_event && scb_taf) {
		if (!taf_ias_data_block(method, now_time)) {
			taf_ias_sched_send(method, scb_taf, tid, tid_state, context,
				TAF_SCHEDULE_REAL_PACKETS, TAF_TYPE_DL);
		}

		if ((scb_taf->info.pkt_pull_request[tid] == 0) &&
			(scb_taf->info.pkt_pull_map & (1 << tid))) {

			scb_taf->info.pkt_pull_map &= ~(1 << tid);

			if (taf_info->total_pull_requests > 0) {
				taf_info->total_pull_requests--;
			} else {
				WL_ERROR(("wl%u %s: tid %u total_pull_requests overflow 0! "
					"(0x%x/0x%x/%u/%u)\n",
					WLCWLUNIT(TAF_WLCT(taf_info)), __FUNCTION__,
					tid, scb_taf->info.tid_enabled,
					scb_taf->info.pkt_pull_map,
					scb_taf->info.pkt_pull_dequeue,
					scb_taf->info.ps_mode));
			}
		}
		scb_taf->info.pkt_pull_dequeue = 0;

		if (!scb_taf->info.ps_mode) {
			taf_ias_sched_update(scb_taf->method, scb_taf, tid,
				tid_state, context, TAF_TYPE_DL);
		}
#if TAF_LOGL1
		WL_TAFT(taf_info, "total_pull_requests %u\n", taf_info->total_pull_requests);
#endif // endif
	} else if (v2r_event) {
		WL_TAFM(method, "async v2r completion\n");
	}

	prev_time = now_time;
	now_time = taf_timestamp(TAF_WLCT(taf_info));
	ias_info->cpu_time += (now_time - prev_time);

	if (wlc_taf_marked_up(taf_info)) {
		tid_state->data.time_delta = now_time - tid_state->data.prev_release_time;
		tid_state->data.prev_release_time = now_time;

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

	if (context->op_state == TAF_SCHEDULE_REAL_PACKETS) {
		TAF_ASSERT(context->tid >= 0 && context->tid < TAF_NUM_TIDSTATE);
	}

	if (!method->ias->tid_active && method->type != LAST_IAS_SCHEDULER) {
		return FALSE;
	}

	now_time = taf_timestamp(TAF_WLCT(taf_info));
	ias_info = TAF_IAS_GROUP_INFO(taf_info);

	if (taf_ias_data_block(method, now_time)) {
		return TRUE;
	}

	tid_state = taf_get_tid_state(taf_info, TAF_TID(context->tid, taf_info));
	context->public.how = TAF_RELEASE_LIKE_IAS;
	context->public.ias.index = ias_info->tag_star_index;
	tid_state->cumul_units[ias_info->tag_star_index] = 0;
	tid_state->was_reset = 0;

	if (tid_state->sched_start == 0) {
		tid_state->sched_start = now_time;
	}

	if (TAF_UNITED_ORDER(taf_info)) {
		method->ias->sched_active = 0;
	} else {
		method->ias->sched_active &= ~(1 << context->tid);
	}

	method->counter++;

	taf_ias_prepare_list(method, now_time, TAF_TID(context->tid, taf_info));

	if (TAF_UNITED_ORDER(taf_info) && method->ias->sched_active) {
		finished = taf_ias_schedule_all_tid(method, tid_state, 0, NUMPRIO - 1,
			context);
	} else if (method->ias->sched_active & (1 << context->tid)) {
		finished = taf_ias_schedule_all_scb(method, tid_state, context->tid, context);
	}

	prev_time = now_time;
	now_time = taf_timestamp(TAF_WLCT(taf_info));
#if !TAF_ENABLE_SQS_PULL
	tid_state->data.time_delta = now_time - tid_state->data.prev_release_time;
	tid_state->data.prev_release_time = now_time;
#endif // endif
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
	wlc_taf_info_t* taf_info = method->taf_info;
	taf_list_t* list = method->list;
	taf_traffic_map_t enabled = 0;
	int tid = 0;
#ifdef TAF_DEBUG_VERBOSE
	bool do_dump = FALSE;
#endif // endif

	if (list == NULL && method->ias->tid_active) {
		method->ias->tid_active = 0;
		WL_TAFM(method, "no links active\n");

		return BCME_OK;
	}
	if (TXPKTPENDTOT(TAF_WLCM(method))) {
		/* if not idle, just exit this housekeeping now */
		return BCME_OK;
	}

#if TAF_ENABLE_UL
	if (ULTRIGPENDTOT(TAF_WLCM(method))) {
		/* if not idle, just exit this housekeeping now */
		return BCME_OK;
	}
#endif /* TAF_ENABLE_UL */

	do {
		taf_schedule_tid_state_t* tid_state = taf_get_tid_state(taf_info,
			TAF_TID(tid, taf_info));

		/* total packets pending is zero (as watchdog exits earlier if not) */
		if (tid_state->total_units_in_transit > 0) {
			WL_ERROR(("wl%u %s: (%x) %u units in transit (expect 0)\n",
				WLCWLUNIT(TAF_WLCT(taf_info)), __FUNCTION__,
				taf_info->scheduler_index,
				tid_state->total_units_in_transit));
#ifdef TAF_DEBUG_VERBOSE
			do_dump  = TRUE;
#endif // endif
			taf_ias_tid_reset(method, tid);
		}

	} while (TAF_PARALLEL_ORDER(taf_info) && ++tid < NUMPRIO);

#ifdef TAF_DEBUG_VERBOSE
	if (do_dump) {
		taf_memtrace_dump(taf_info);
	}
#endif // endif

	while (list) {
		taf_scb_cubby_t *scb_taf;

		scb_taf = list->scb_taf;
		enabled |= scb_taf->info.tid_enabled;

		list = list->next;
	}
	if (method->ias->tid_active != enabled) {
		WL_TAFM(method, "current enable 0x%x, checked enable 0x%x\n",
			method->ias->tid_active, enabled);
		method->ias->tid_active = enabled;
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
	taf_ias_sched_state,    /* schedstate_fn   */
#ifdef TAF_PKTQ_LOG
	taf_ias_dpstats_dump,   /* dpstats_log_fn  */
#endif // endif
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
	taf_iasm_sched_state,   /* schedstate_fn */
#ifdef TAF_PKTQ_LOG
	NULL,                    /* dpstats_log_fn  */
#endif // endif
};
#endif /* TAF_ENABLE_SQS_PULL */

#define TAFNAME  "taf -"

static const char* taf_ias_name[NUM_IAS_SCHEDULERS] = {
#if TAF_ENABLE_SQS_PULL
	TAFNAME"markup",
#endif /* TAF_ENABLE_SQS_PULL */
	TAFNAME"ebos", TAFNAME"prr", TAFNAME"atos", TAFNAME"atos2"};

void* BCMATTACHFN(wlc_taf_ias_method_attach)(wlc_taf_info_t *taf_info, taf_scheduler_kind type)
{
	taf_ias_container_t* container;
	taf_method_info_t* method;
	taf_ias_group_info_t* ias_info = NULL;
	taf_source_type_t source;
	taf_ias_method_info_t* cfg_ias;

	TAF_ASSERT(TAF_TYPE_IS_IAS(type));

	container = (taf_ias_container_t*) MALLOCZ(TAF_WLCT(taf_info)->pub->osh,
		sizeof(*container));

	if (container == NULL) {
		goto exitfail;
	}
	method = &container->method;
	TAF_ASSERT((void*)method == (void*)container);

	cfg_ias = method->ias = &container->ias;

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
	method->name = taf_ias_name[IAS_INDEX(type)] + sizeof(TAFNAME) - 1;

	method->score_init = 0;

#ifdef TAF_DBG
	if (method->funcs.dump_fn) {
		method->dump_name = taf_ias_name[IAS_INDEX(type)];
	}
#endif // endif

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
		ias_info->unified_tid_state.immed = 12000;
	} else {
		ias_info = TAF_IAS_GROUP_INFO(taf_info);
		TAF_ASSERT(ias_info);
	}
	taf_info->group_use_count[TAF_SCHEDULER_IAS_METHOD] ++;

	cfg_ias->coeff.time_shift = TAF_COEFF_IAS_FACTOR;
	taf_ias_set_coeff(TAF_COEFF_IAS_DEF, &cfg_ias->coeff);

#if TAF_ENABLE_SQS_PULL
#if TAF_ENABLE_NAR
	cfg_ias->pre_rel_limit[TAF_SOURCE_NAR] = 1;
#endif /* TAF_ENABLE_NAR */
#if TAF_ENABLE_AMPDU
	cfg_ias->pre_rel_limit[TAF_SOURCE_AMPDU] = 32;
#endif /* TAF_ENABLE_AMPDU */
#if TAF_ENABLE_UL
	cfg_ias->pre_rel_limit[TAF_SOURCE_UL] = 65535;
#endif // endif
	cfg_ias->pre_rel_limit[TAF_SOURCE_HOST_SQS] = 0;
	/* with SQS, margin is the amount of extra pkts to request beyond estimate */
	cfg_ias->margin = 16;
#endif /* TAF_ENABLE_SQS_PULL */

	cfg_ias->release_limit = 0;
	cfg_ias->barrier = 2;

	switch (type) {
		case TAF_EBOS :
			method->ordering = TAF_LIST_SCORE_FIXED_INIT_MINIMUM;
			method->score_init = 1;
			cfg_ias->high = TAF_TIME_HIGH_DEFAULT;
			cfg_ias->low = TAF_TIME_LOW_DEFAULT;
			cfg_ias->opt_aggp_limit = 0;
			cfg_ias->score_weights = NULL;
			cfg_ias->barrier = 1;
#if TAF_ENABLE_MU_TX
			cfg_ias->mu_pair = NULL;
			cfg_ias->mu_mimo_rel_limit = NULL;
			cfg_ias->mu_mimo_opt = 0;
			cfg_ias->mu_ofdma_opt = 0;
#endif /* TAF_ENABLE_MU_TX */
			cfg_ias->options = 0;
			break;

		case TAF_PSEUDO_RR :
			cfg_ias->high = TAF_TIME_HIGH_DEFAULT;
			cfg_ias->low = TAF_TIME_LOW_DEFAULT;
			cfg_ias->opt_aggp_limit = 63;
			cfg_ias->score_weights = taf_tid_score_weights;
#if TAF_ENABLE_MU_TX
			cfg_ias->mu_pair = NULL;
			cfg_ias->mu_mimo_rel_limit = NULL;
			cfg_ias->mu_mimo_opt =
				(1 << TAF_OPT_MIMO_MEAN) |
				(1 << TAF_OPT_MIMO_SWEEP);

			cfg_ias->mu_ofdma_opt =
				(1 << TAF_OPT_OFDMA_TOO_SMALL) |
				(1 << TAF_OPT_OFDMA_TOO_LARGE) |
				(1 << TAF_OPT_OFDMA_PAIR) |
				(1 << TAF_OPT_OFDMA_SWEEP);
#endif /* TAF_ENABLE_MU_TX */
			cfg_ias->options = (1 << TAF_OPT_EXIT_EARLY);
			break;
#if TAF_ENABLE_SQS_PULL
		case TAF_VIRTUAL_MARKUP:
			/* fall through */
#endif /* TAF_ENABLE_SQS_PULL */
		case TAF_ATOS:
			cfg_ias->high = TAF_TIME_ATOS_HIGH_DEFAULT;
			cfg_ias->low = TAF_TIME_ATOS_LOW_DEFAULT;
			cfg_ias->opt_aggp_limit = 63;
			cfg_ias->score_weights = taf_tid_score_weights;
#if TAF_ENABLE_MU_TX
			cfg_ias->mu_pair = taf_mu_pair;
			cfg_ias->mu_mimo_rel_limit = taf_mu_mimo_rel_limit;
			cfg_ias->mu_mimo_opt =
				(1 << TAF_OPT_MIMO_MEAN) |
				(1 << TAF_OPT_MIMO_SWEEP);

			cfg_ias->mu_ofdma_opt =
				(1 << TAF_OPT_OFDMA_TOO_SMALL) |
				(1 << TAF_OPT_OFDMA_TOO_LARGE) |
				(1 << TAF_OPT_OFDMA_PAIR) |
				(1 << TAF_OPT_OFDMA_SWEEP);
#endif /* TAF_ENABLE_MU_TX */
			cfg_ias->options = (1 << TAF_OPT_EXIT_EARLY);
			break;

		case TAF_ATOS2:
			cfg_ias->high = TAF_TIME_ATOS2_HIGH_DEFAULT;
			cfg_ias->low = TAF_TIME_ATOS2_LOW_DEFAULT;
			cfg_ias->opt_aggp_limit = 31;
			cfg_ias->score_weights = taf_tid_score_weights;
#if TAF_ENABLE_MU_TX
			cfg_ias->mu_pair = taf_mu_pair;
			cfg_ias->mu_mimo_rel_limit = taf_mu_mimo_rel_limit;
			cfg_ias->mu_mimo_opt =
				(1 << TAF_OPT_MIMO_MEAN) |
				(1 << TAF_OPT_MIMO_SWEEP);

			cfg_ias->mu_ofdma_opt =
				(1 << TAF_OPT_OFDMA_TOO_SMALL) |
				(1 << TAF_OPT_OFDMA_TOO_LARGE) |
				(1 << TAF_OPT_OFDMA_PAIR) |
				(1 << TAF_OPT_OFDMA_SWEEP);
#endif /* TAF_ENABLE_MU_TX */
			cfg_ias->options = (1 << TAF_OPT_EXIT_EARLY);
			break;
		default:
			MFREE(TAF_WLCT(taf_info)->pub->osh, method, sizeof(*method));
			WL_ERROR(("wl%u %s: unknown taf scheduler type %u\n",
				WLCWLUNIT(TAF_WLCT(taf_info)), __FUNCTION__, type));
			TAF_ASSERT(0);
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

#if TAF_ENABLE_SQS_PULL
static bool taf_ias_sqs_capable(taf_scb_cubby_t *scb_taf, int tid)
{
	taf_method_info_t* method;
	wlc_taf_info_t* taf_info;
	taf_ias_sched_data_t *ias_sched;
	const taf_source_type_t s_idx = TAF_SOURCE_HOST_SQS;

	TAF_ASSERT(scb_taf && tid >= 0 && tid < NUMPRIO);

	method = scb_taf->method;
	taf_info = method->taf_info;

	ias_sched = TAF_IAS_TID_STATE(scb_taf, tid);

	if (ias_sched && ias_sched->handle[s_idx].used) {
		return wlc_sqs_ampdu_capable(TAF_WLCT(taf_info),
			ias_sched->handle[s_idx].scbh, tid);
	}
	return wlc_sqs_ampdu_capable(TAF_WLCT(taf_info),
		taf_info->funcs[s_idx].scb_h_fn(*(taf_info->source_handle_p[s_idx]),
		scb_taf->scb), tid);
}
#endif /* TAF_ENABLE_SQS_PULL */

int taf_ias_dump(void *handle, struct bcmstrbuf *b)
{
	taf_method_info_t* method = handle;
	wlc_taf_info_t *taf_info = method->taf_info;
	taf_ias_group_info_t* ias_info = TAF_IAS_GROUP_INFO(taf_info);
	uint32 prev_time = ias_info->cpu_elapsed_time;
	uint32 cpu_norm = 0;
	int tid_index = 0;
	taf_list_t*  list = method->list;

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

	bcm_bprintf(b, "ncount flow %u\n", ias_info->ncount_flow_last);

	method->counter = 0;

	for (tid_index = 0; tid_index < TAF_NUM_TIDSTATE; tid_index++) {
		taf_schedule_tid_state_t* tidstate = NULL;
		uint32 total_count;
		uint32 ave_high = 0;
		uint32 ave_dur = 0;

#if !TAF_ENABLE_SQS_PULL
		if (TAF_PARALLEL_ORDER(taf_info)) {
			int tid = tid_service_order[tid_index];

			tidstate = taf_get_tid_state(taf_info, tid);

			total_count = tidstate->debug.target_high + tidstate->debug.uflow_high;
			if (total_count > 0) {
				ave_high = tidstate->debug.average_high / total_count;
				ave_dur = tidstate->debug.sched_duration / total_count;
			}

			bcm_bprintf(b,
				"tid %u\n"
				"target high %6u, uflow high %6u, ave_high %6uus\n"
				"target low  %6u, uflow low  %6u\n"
				"immed trigg %6u, immed star %6u\n"
				"ave sched duration %5u, max sched duration %5u\n",
				tid_service_order[tid_index],
				tidstate->debug.target_high, tidstate->debug.uflow_high,
				TAF_UNITS_TO_MICROSEC(ave_high),
				tidstate->debug.target_low, tidstate->debug.uflow_low,
				tidstate->debug.immed_trig, tidstate->debug.immed_star,
				ave_dur, tidstate->debug.max_duration);
		}
#endif /* !TAF_ENABLE_SQS_PULL */
		if (TAF_UNITED_ORDER(taf_info)) {
			tidstate = taf_get_tid_state(taf_info,
				TAF_TID(TAF_DEFAULT_UNIFIED_TID, taf_info));

			total_count = tidstate->debug.target_high + tidstate->debug.uflow_high;
			if (total_count > 0) {
				ave_high = tidstate->debug.average_high / total_count;
				ave_dur = tidstate->debug.sched_duration / total_count;
			}

			bcm_bprintf(b,
				"unified\n"
				"target high %6u, uflow high %6u, ave_high %6uus\n"
				"target low  %6u, uflow low  %6u\n"
				"immed trigg %6u, immed star %6u\n"
				"ave sched duration %5u, max sched duration %5u\n",
				tidstate->debug.target_high, tidstate->debug.uflow_high,
				TAF_UNITS_TO_MICROSEC(ave_high),
				tidstate->debug.target_low, tidstate->debug.uflow_low,
				tidstate->debug.immed_trig, tidstate->debug.immed_star,
				ave_dur, tidstate->debug.max_duration);

			tid_index = TAF_NUM_TIDSTATE; /* break the loop */
		}
		TAF_ASSERT(tidstate);
		memset(&tidstate->debug, 0, sizeof(tidstate->debug));
	}
	while (list) {
		taf_scb_cubby_t *scb_taf = list->scb_taf;
		taf_source_type_t s_idx;

		bcm_bprintf(b, "\n"MACF"%s score %4u %s\n", TAF_ETHERC(scb_taf),
			TAF_TYPE(list->type),
			scb_taf->info.scb_stats.ias.data.relative_score[list->type],
			scb_taf->info.ps_mode ? " PS":"");

		for (s_idx = 0; s_idx < TAF_NUM_SCHED_SOURCES; s_idx ++) {
			bool enabled = scb_taf->info.scb_stats.ias.data.use[s_idx];
			int tid;

			if (!taf_src_type_match(s_idx, list->type)) {
				continue;
			}

			bcm_bprintf(b, "%s %sabled\n", TAF_SOURCE_NAME(s_idx),
				enabled ? "en" : "dis");

			if (!enabled) {
				continue;
			}
			for (tid = 0; tid < NUMPRIO; tid++) {
				taf_link_state_t st = scb_taf->info.linkstate[s_idx][tid];
				char * msg = "";
#ifdef TAF_DBG
				const char * st_txt = taf_link_states_text[st];
#else
				const char * st_txt = "";
#endif /* TAF_DBG */

				if (st == TAF_LINKSTATE_NONE) {
					continue;
				}
#if TAF_ENABLE_SQS_PULL
				if (TAF_SOURCE_IS_SQS(s_idx)) {
					msg = taf_ias_sqs_capable(scb_taf, tid) ?
					      "sqs cap" : "sqs incap";
				}
#endif /* TAF_ENABLE_SQS_PULL */
				if (TAF_SOURCE_IS_AMPDU(s_idx)) {
					if (!SCB_AMPDU(scb_taf->scb)) {
						msg = "ampdu incap";
					}
				}
				if (TAF_SOURCE_IS_UL(s_idx)) {
					bcm_bprintf(b, "%u: [%u] %s (/%u) %s\n", tid, st, st_txt,
						scb_taf->info.traffic.count[s_idx][tid], msg);
				} else {
					bcm_bprintf(b, "%u: [%u] %s (%u/%u) %s\n", tid, st, st_txt,
						SCB_PKTS_INFLT_FIFOCNT_VAL(scb_taf->scb, tid),
						scb_taf->info.traffic.count[s_idx][tid], msg);
				}
			}
		}
		list = list->next;
	}
	return BCME_OK;
}

static void taf_ias_tid_reset(taf_method_info_t* method, int tid)
{
	taf_schedule_tid_state_t* tidstate = taf_get_tid_state(method->taf_info,
		TAF_TID(tid, method->taf_info));
#ifdef TAF_DBG
	uint32 prev_rel_units = tidstate->real.released_units;
#endif /* TAF_DBG */
	taf_ias_group_info_t* ias_info = TAF_IAS_GROUP_INFO(method->taf_info);
	int index;
	if (tidstate->was_reset) {
		return;
	}
	tidstate->trigger = TAF_TRIGGER_NONE;
	tidstate->real.released_units = 0;
	tidstate->total.released_units = 0;
	tidstate->star_packet_received = FALSE;

	for (index = 0; index <= TAF_MAX_PKT_INDEX; index ++) {
		tidstate->cumul_units[index] = 0;
		tidstate->total_sched_units[index] = 0;
	}

	tidstate->total_units_in_transit = 0;
	tidstate->debug.prev_index = TAF_MAX_PKT_INDEX + 1;
	tidstate->prev_resched_index = -1;
	tidstate->resched_units[0] = 0;
	tidstate->resched_units[1] = 0;
	tidstate->resched_index[0] = ias_info->tag_star_index;
	tidstate->cycle_now = 0;
	tidstate->cycle_next = 0;
	tidstate->cycle_ready = 0;

	tidstate->high[IAS_INDEXM(method)] = method->ias->high;
	tidstate->low[IAS_INDEXM(method)] = method->ias->low;

	memset(&tidstate->data, 0, sizeof(tidstate->data));
	memset(&tidstate->debug, 0, sizeof(tidstate->debug));
	memset(&tidstate->real, 0, sizeof(tidstate->real));
	memset(&tidstate->total, 0, sizeof(tidstate->total));
	memset(&tidstate->extend, 0, sizeof(tidstate->extend));

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
		scb_taf->info.traffic.map[s_idx] |= (1 << tid);

		ias_sched->scbtidstats.data.calc_timestamp = scb_taf->timestamp;
		/* initial bias in packet averaging initialisation */
		ias_sched->scbtidstats.data.release_pkt_size_mean = TAF_PKT_SIZE_DEFAULT;
		ias_sched->scbtidstats.data.total_scaled_ncount =
			(4 << TAF_NCOUNT_SCALE_SHIFT);

		WL_TAFM(method, "ias_sched for "MACF" tid %u source %s reset (%u) "
			"r_t_s 0x%x\n",  TAF_ETHERC(scb_taf), tid,
			TAF_SOURCE_NAME(s_idx), ias_sched->handle[s_idx].used,
			*method->ready_to_schedule);
	} else {
		ias_sched->handle[s_idx].scbh = NULL;
		ias_sched->handle[s_idx].tidh = NULL;

		WL_TAFM(method, "ias_sched for "MACF" tid %u source %s unused r_t_s "
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
		scb_taf->info.tid_enabled &= ~(1 << tid);
		WL_TAFM(method, "ias_sched for "MACF" tid %u destroyed (in flight count %u)\n",
			TAF_ETHERC(scb_taf), tid, SCB_PKTS_INFLT_FIFOCNT_VAL(scb_taf->scb, tid));
	}
#if TAF_ENABLE_SQS_PULL
	if (scb_taf->info.pkt_pull_request[tid] > 0) {
		WL_TAFM(method, "pkt pull req for "MACF" tid %u was %u, total pull count %u, "
			"reset\n", TAF_ETHERC(scb_taf), tid, scb_taf->info.pkt_pull_request[tid],
			method->taf_info->total_pull_requests);

		--method->taf_info->total_pull_requests;
		WL_TAFT(method->taf_info, "total_pull_requests %u\n",
			method->taf_info->total_pull_requests);
		scb_taf->info.pkt_pull_request[tid] = 0;
		scb_taf->info.pkt_pull_map &= ~(1 << tid);
	}
#endif /* TAF_ENABLE_SQS_PULL */
	return BCME_OK;
}

static int taf_ias_sched_init(taf_method_info_t* method, taf_scb_cubby_t *scb_taf, int tid)
{
	wlc_info_t *wlc = TAF_WLCM(method);
	taf_ias_sched_data_t *ias_sched;

	TAF_ASSERT(TAF_CUBBY_TIDINFO(scb_taf, tid) == NULL);

	ias_sched = (taf_ias_sched_data_t*) MALLOCZ(wlc->pub->osh, sizeof(*ias_sched));

	if (!ias_sched) {
		return BCME_NOMEM;
	}
	TAF_CUBBY_TIDINFO(scb_taf, tid) = ias_sched;
	scb_taf->info.tid_enabled |=  (1 << tid);

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

	TAF_ASSERT(scb_taf || (state == TAF_LINKSTATE_NOT_ACTIVE) ||
		(state == TAF_LINKSTATE_REMOVE));

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

		case TAF_LINKSTATE_NOT_ACTIVE:
			if (ias_sched) {
				ias_sched->handle[s_idx].used = FALSE;
				status = taf_ias_sched_reset(method, scb_taf, tid, s_idx);
			}
			scb_taf->info.linkstate[s_idx][tid]  = state;
			WL_TAFM(method, "ias_sched for "MACF" tid %u %s now inactive\n",
				TAF_ETHERS(scb), tid, TAF_SOURCE_NAME(s_idx));
			break;

		case TAF_LINKSTATE_NONE:
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
						break;
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

		case TAF_LINKSTATE_HARD_RESET:
			if (ias_sched) {
				taf_ias_sched_reset(method, scb_taf, tid, s_idx);
				taf_ias_tid_reset(method, tid);
			}
			break;

		case TAF_LINKSTATE_SOFT_RESET:
			if (ias_sched) {
				taf_ias_sched_reset(method, scb_taf, tid, s_idx);
			}
			break;
		case TAF_LINKSTATE_AMSDU_AGG:
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
			scb_taf->info.traffic.map[s_idx] = 0;
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

			scb_taf->info.traffic.map[s_idx] &= ~(1 << tid);

			if (ias_sched && ias_sched->handle[s_idx].used && pktqlen_fn) {
				uint16 pktq_len;
				pktq_len = pktqlen_fn(ias_sched->handle[s_idx].scbh,
					ias_sched->handle[s_idx].tidh);
				scb_taf->info.traffic.count[s_idx][tid] = pktq_len;
				pending += pktq_len;
				if (pktq_len) {
					scb_taf->info.traffic.map[s_idx] |= (1 << tid);
					scb_taf->info.traffic.available[TAF_SRC_TO_TYPE(s_idx)] |=
						(1 << tid);
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
	uint32 elapsed = 0;

	if (!scb_taf) {
		return;
	}

	if (scb_taf->info.ps_mode) {
		scb_taf->info.scb_stats.ias.debug.ps_enter = taf_timestamp(TAF_WLCM(method));
	} else {
		elapsed = taf_timestamp(TAF_WLCM(method)) -
			scb_taf->info.scb_stats.ias.debug.ps_enter;
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

	WL_TAFT(method->taf_info, MACF" PS mode O%s (pkt_pull_map 0x%02x%s) elapsed %u\n",
		TAF_ETHERC(scb_taf), scb_taf->info.ps_mode ? "N" : "FF",
		scb_taf->info.pkt_pull_map, taf_debug, elapsed);
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
				taf_list_scoring_order_t order = method->ordering;

				for (tid = 0; tid < TAF_MAXPRIO; tid++) {
#if TAF_ENABLE_SQS_PULL
					scb_taf->info.pkt_pull_request[tid] = 0;
#endif /* TAF_ENABLE_SQS_PULL */
					for (s_idx = TAF_FIRST_REAL_SOURCE;
						s_idx < TAF_NUM_SCHED_SOURCES; s_idx++) {

						scb_taf->info.traffic.map[s_idx] = 0;
						scb_taf->info.traffic.count[s_idx][tid] = 0;
					}
				}
				scb_taf->info.traffic.available[TAF_TYPE_DL] = 0;
#if TAF_ENABLE_UL
				scb_taf->info.traffic.available[TAF_TYPE_UL] = 0;
#endif // endif
				scb_taf->info.ps_mode = SCB_PS(scb);
				scb_taf->timestamp = taf_timestamp(TAF_WLCM(method));
				scb_taf->force = FALSE;

				if (order == TAF_LIST_SCORE_MINIMUM ||
					order == TAF_LIST_SCORE_MAXIMUM) {
					scb_taf->score[TAF_TYPE_DL] = method->score_init;
#if TAF_ENABLE_UL
					scb_taf->score[TAF_TYPE_UL] = method->score_init;
#endif // endif
				}
#if TAF_ENABLE_SQS_PULL
				scb_taf->info.pkt_pull_map = 0;
				memset(scb_taf->info.released_units_limit, 0,
				       sizeof(scb_taf->info.released_units_limit));
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

		case TAF_SCBSTATE_MU_DL_VHMIMO:
		case TAF_SCBSTATE_MU_DL_HEMIMO:
		case TAF_SCBSTATE_MU_DL_OFDMA:
		case TAF_SCBSTATE_MU_UL_OFDMA:
#if TAF_ENABLE_MU_TX
			if (scb_taf) {
				taf_tech_type_t idx;
				wlc_taf_info_t* taf_info = method->taf_info;

				switch (state) {
					case TAF_SCBSTATE_MU_DL_VHMIMO:
						idx = TAF_TECH_DL_VHMUMIMO;
						break;
					case TAF_SCBSTATE_MU_DL_HEMIMO:
						idx = TAF_TECH_DL_HEMUMIMO;
						break;
					case TAF_SCBSTATE_MU_DL_OFDMA:
						idx = TAF_TECH_DL_OFDMA;
						break;
#if TAF_ENABLE_UL
					case TAF_SCBSTATE_MU_UL_OFDMA:
						TAF_ASSERT(method->taf_info->ul_enabled);
						idx = TAF_TECH_UL_OFDMA;
						wlc_taf_handle_ul_transition(scb_taf,
							data ? TRUE : FALSE);
						break;
#endif /* TAF_ENABLE_UL */
					default:
						return BCME_ERROR;
				}

				/* XXX for now assume all TID are active/not active together.
				 * This should be move to "linkstate" handler once external
				 * system is controlling this at TID level and not SCB level
				 */
				scb_taf->info.mu_tech[idx].enable =
					(taf_traffic_map_t) ((data) ? ~0 : 0);

				if (scb_taf->info.mu_tech[idx].enable) {
					scb_taf->info.mu_enable_mask |= (1 << idx);
				} else {
					scb_taf->info.mu_enable_mask &= ~(1 << idx);
				}

				if (scb_taf->info.mu_tech[idx].enable &&
					taf_info->tech_handle_p[idx] != NULL &&
					taf_info->tech_fn[idx].scb_h_fn != NULL) {
					void * th = *taf_info->tech_handle_p[idx];

					scb_taf->info.mu_tech[idx].scb_h =
						taf_info->tech_fn[idx].scb_h_fn(th, scb);
				} else {
					scb_taf->info.mu_tech[idx].scb_h = NULL;
				}
				WL_TAFM(method, MACF" set %s to %sable\n", TAF_ETHERS(scb),
					taf_mutech_text[idx],
					scb_taf->info.mu_tech[idx].enable ? "en" : "dis");
			}
#endif /* TAF_ENABLE_MU_TX */
			break;

		case TAF_SCBSTATE_UPDATE_BSSCFG:
		case TAF_SCBSTATE_DWDS:
		case TAF_SCBSTATE_WDS:
		case TAF_SCBSTATE_OFF_CHANNEL:
		case TAF_SCBSTATE_DATA_BLOCK_OTHER:
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
taf_ias_handle_star(taf_method_info_t* method, taf_scb_cubby_t* scb_taf, int tid, uint32 pkttag,
	uint8 index, taf_list_type_t type)
{
	wlc_taf_info_t* taf_info = method->taf_info;
	taf_schedule_tid_state_t* tid_state = taf_get_tid_state(taf_info,
		TAF_TID(tid, taf_info));
	bool prev_complete = TRUE;
	int8 last_sched;
	uint32 pkttag_units;

#if TAF_LOGL4
	if (index != tid_state->debug.prev_index) {
		WL_TAFT(taf_info, "recv index %u (wait %u)\n", index, tid_state->resched_index[0]);
		tid_state->debug.prev_index = index;

	}
#endif // endif

	if (type == TAF_TYPE_DL) {
		tid_state->cumul_units[index] += (pkttag_units = TAF_PKTTAG_TO_UNITS(pkttag));
	}
#if TAF_ENABLE_UL
	else {
		tid_state->cumul_units[index] += (pkttag_units = pkttag);
	}
#endif /* TAF_ENABLE_UL */

	if (tid_state->total_units_in_transit >= pkttag_units) {
		tid_state->total_units_in_transit -= pkttag_units;
	} else {
		WL_ERROR(("wl%u %s: (%x) overflow (%u) %u/%u/%u/%u\n",
			WLCWLUNIT(TAF_WLCT(taf_info)), __FUNCTION__, taf_info->scheduler_index,
			index, pkttag_units, tid_state->cumul_units[index],
			tid_state->total_sched_units[index], tid_state->total_units_in_transit));
#ifdef TAF_DEBUG_VERBOSE
		taf_memtrace_dump(taf_info);
#endif // endif
		tid_state->total_units_in_transit  = 0;
	}

	if (method->ias->barrier && (last_sched = tid_state->prev_resched_index) >= 0) {
		last_sched -= (method->ias->barrier - 1);
		if (last_sched < 0) {
			last_sched += TAF_MAX_PKT_INDEX + 1;
		}
		if (tid_state->cumul_units[last_sched] < tid_state->total_sched_units[last_sched]) {
			prev_complete = FALSE;
		}
	}

#ifdef TAF_DEBUG_VERBOSE
	if (tid_state->cumul_units[tid_state->resched_index[0]] >= tid_state->resched_units[0]) {

		if (!prev_complete && tid_state->debug.barrier_start == 0) {
			tid_state->debug.barrier_start = taf_timestamp(TAF_WLCT(taf_info));
		}
		if (tid_state->waiting_schedule && tid_state->debug.wait_start == 0) {
			tid_state->debug.wait_start = taf_timestamp(TAF_WLCT(taf_info));
		}
		if (prev_complete && tid_state->debug.barrier_start != 0) {
			uint32 now_time = taf_timestamp(TAF_WLCT(taf_info));
			uint32 elapsed = now_time - tid_state->debug.barrier_start;

			tid_state->debug.barrier_start = 0;

			WL_TAFT(taf_info, "barrier wait time %u us\n", elapsed);
		}
		if (!tid_state->waiting_schedule && tid_state->debug.wait_start != 0) {
			uint32 now_time = taf_timestamp(TAF_WLCT(taf_info));
			uint32 elapsed = now_time - tid_state->debug.wait_start;

			tid_state->debug.wait_start = 0;

			WL_TAFT(taf_info, "supersched wait time %u us\n", elapsed);
		}
	}
#endif /* TAF_DEBUG_VERBOSE */

	if (tid_state->cumul_units[tid_state->resched_index[0]] >= tid_state->resched_units[0] &&
		prev_complete && !tid_state->waiting_schedule) {

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

		WL_TAFT(taf_info, "cumulative %d/%d  reschedule=%d, complete\n\n",
			tid_state->resched_index[0],
			TAF_UNITS_TO_MICROSEC(tid_state->cumul_units[tid_state->resched_index[0]]),
			TAF_UNITS_TO_MICROSEC(tid_state->resched_units[0]));

		tid_state->prev_resched_index = tid_state->resched_index[0];

		if (tid_state->cycle_next == 1 && tid_state->cycle_now == 0) {
			if (tid_state->cycle_ready) {
				tid_state->resched_index[0]  = tid_state->resched_index[1];
				tid_state->resched_units[0]  = tid_state->resched_units[1];
				tid_state->star_packet_received = FALSE;
				tid_state->cycle_now = 1;
				tid_state->waiting_schedule = 1;
			} else  {
				WL_TAFT(taf_info, "super sched not ready\n");
			}
		} else if (tid_state->cycle_next == 0 && tid_state->cycle_now == 1) {
			tid_state->cycle_now = 0;
		} else {
			tid_state->resched_index[0] = ias_info->tag_star_index;
			tid_state->cycle_now = 0;
			tid_state->cycle_next = 0;
		}
		if (taf_info->super) {
			WL_TAFT(taf_info, "super sched: wait %u/%u, cycle now %u; cycle next "
				"%u, prev index %u, ready %u, wait sched %u\n\n",
				tid_state->resched_index[0],
				TAF_UNITS_TO_MICROSEC(tid_state->resched_units[0]),
				tid_state->cycle_now, tid_state->cycle_next,
				tid_state->prev_resched_index, tid_state->cycle_ready,
				tid_state->waiting_schedule);
		}
	}

	return TRUE;
}

static INLINE bool taf_ias_pkt_lost(taf_method_info_t* method, taf_scb_cubby_t* scb_taf,
	int tid, void * p, taf_txpkt_state_t status)
{
	taf_schedule_tid_state_t* tid_state = taf_get_tid_state(method->taf_info,
		TAF_TID(tid, method->taf_info));
	uint16 pkttag_index = WLPKTTAG(p)->pktinfo.taf.ias.index;
	uint16 pkttag_units = WLPKTTAG(p)->pktinfo.taf.ias.units;
	uint32 taf_pkt_time_units = TAF_PKTTAG_TO_UNITS(pkttag_units);

	BCM_REFERENCE(tid_state);
	WL_TAFT(method->taf_info, MACF"/%u, %u - %u/%u (%u) %s (tuit %u)\n", TAF_ETHERC(scb_taf),
		tid, TAF_PKTTAG_TO_MICROSEC(pkttag_units),
		pkttag_index, TAF_UNITS_TO_MICROSEC(tid_state->cumul_units[pkttag_index]),
		tid_state->resched_index[0], taf_txpkt_status_text[status],
		tid_state->total_units_in_transit);

	switch (method->type) {
		case TAF_EBOS:
			break;

		case TAF_PSEUDO_RR:
			if (scb_taf->score[TAF_TYPE_DL] > 0) {
				scb_taf->score[TAF_TYPE_DL]--;
			}
			break;

		case TAF_ATOS:
		case TAF_ATOS2:
		{
			uint32 taf_pkt_time_score = taf_units_to_score(method, scb_taf, tid,
				taf_pkt_time_units);

			if (scb_taf->score[TAF_TYPE_DL] > taf_pkt_time_score) {
				scb_taf->score[TAF_TYPE_DL] -= taf_pkt_time_score;
			} else {
				scb_taf->score[TAF_TYPE_DL] = 0;
			}
		}
			break;

		default:
			TAF_ASSERT(0);
	}
	return taf_ias_handle_star(method, scb_taf, tid, pkttag_units, pkttag_index, TAF_TYPE_DL);
}

bool taf_ias_tx_status(void * handle, taf_scb_cubby_t* scb_taf, int tid, void * p,
	taf_txpkt_state_t status)
{
	taf_method_info_t* method = handle;
	bool ret = FALSE;
#if TAF_ENABLE_UL
	taf_list_type_t type = (status == TAF_TXPKT_STATUS_TRIGGER_COMPLETE) ?
		TAF_TYPE_UL : TAF_TYPE_DL;
#else
	taf_list_type_t type = TAF_TYPE_DL;
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
			/* fall through */
		case TAF_TXPKT_STATUS_SUPPRESSED_FREE:
#if TAF_ENABLE_SQS_PULL
		case TAF_TXPKT_STATUS_SUPPRESSED:
#endif // endif
			if (TAF_IS_TAGGED(WLPKTTAG(p)) &&
					WLPKTTAG(p)->pktinfo.taf.ias.units <= TAF_PKTTAG_RESERVED) {

				ret = taf_ias_pkt_lost(method, scb_taf, tid, p, status);
				WLPKTTAG(p)->pktinfo.taf.ias.units = TAF_PKTTAG_PROCESSED;
			}
			break;

		case TAF_TXPKT_STATUS_REGMPDU:
		case TAF_TXPKT_STATUS_PKTFREE:
			if (TAF_IS_TAGGED(WLPKTTAG(p)) &&
					WLPKTTAG(p)->pktinfo.taf.ias.units <= TAF_PKTTAG_RESERVED) {

				ret = taf_ias_handle_star(method, scb_taf, tid,
					WLPKTTAG(p)->pktinfo.taf.ias.units,
					WLPKTTAG(p)->pktinfo.taf.ias.index, TAF_TYPE_DL);
				WLPKTTAG(p)->pktinfo.taf.ias.units = TAF_PKTTAG_PROCESSED;
			}
			break;
#if TAF_ENABLE_UL
		case TAF_TXPKT_STATUS_TRIGGER_COMPLETE:
			if (TAF_IS_TAGGED(WLULPKTTAG(p))) {
				WL_TAFT(method->taf_info, MACF" processing\n",
					TAF_ETHERC(scb_taf));
				ret = taf_ias_handle_star(method, scb_taf, tid,
					WLULPKTTAG(p)->units,
					WLULPKTTAG(p)->index, TAF_TYPE_UL);
				WLULPKTTAG(p)->units = TAF_PKTTAG_PROCESSED;
			}
			break;
#endif /* TAF_ENABLE_UL */
		default:
			break;
	}
	if (!ret &&
		(((type == TAF_TYPE_DL) && TAF_IS_TAGGED(WLPKTTAG(p)) &&
			(WLPKTTAG(p)->pktinfo.taf.ias.units == TAF_PKTTAG_PROCESSED)) ||
#if TAF_ENABLE_UL
			((type == TAF_TYPE_UL) && TAF_IS_TAGGED(WLULPKTTAG(p)) &&
			(WLULPKTTAG(p)->units == TAF_PKTTAG_PROCESSED)))) {
#else
			FALSE)) {
#endif /* TAF_ENABLE_UL */
#ifdef TAF_LOGL2
		WL_TAFT(method->taf_info, MACF" %s tid %u, pkt already processed\n",
			TAF_ETHERC(scb_taf), TAF_TYPE(type), tid);
#endif // endif
		ret = TRUE;
	}
	return ret;
}

#if TAF_ENABLE_SQS_PULL
static void taf_iasm_sched_state(void * handle, taf_scb_cubby_t * scb_taf, int tid, int count,
	taf_source_type_t s_idx, taf_sched_state_t state)
{
	taf_method_info_t* method = handle;

	switch (state) {
		case TAF_SCHED_STATE_DATA_BLOCK_FIFO:
			WL_TAFM(method, "data block %u\n", method->ias->data_block);
			method->ias->data_block = count ? TRUE : FALSE;
			break;

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
			WL_TAFM(method, "data block %u\n", method->ias->data_block);
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
			break;
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
		if (err == TAF_IOVAR_OK_SET) {
			taf_ias_set_coeff(coeff, &method->ias->coeff);
		}
		result->misc = method->ias->coeff.coeff1;
		return err;
	}
	if (!strcmp(cmd, "coeff_factor") && (method->type != TAF_EBOS)) {
		int err;
		uint32 factor = method->ias->coeff.time_shift;
		err = wlc_taf_param(&local_cmd, &factor, 6, 18, b);
		if (err == TAF_IOVAR_OK_SET) {
			method->ias->coeff.time_shift = factor;
		}
		result->misc = method->ias->coeff.time_shift;
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
	if (!strcmp(cmd, "immed")) {
		taf_ias_group_info_t* ias_info = TAF_IAS_GROUP_INFO(method->taf_info);
		uint32 immed = ias_info->unified_tid_state.immed;
		int err = wlc_taf_param(&local_cmd, &immed, 0, 65535, b);
		if (err == TAF_IOVAR_OK_SET) {
			ias_info->unified_tid_state.immed = immed;
			taf_ias_time_settings_sync(method);
		}
		result->misc = immed;
		return err;
	}
#if TAF_ENABLE_MU_TX
	if (!strcmp(cmd, "mu_pair")) {
		int err = BCME_BADARG;

		if (method->ias->mu_pair == NULL) {
			return err;
		}
		cmd += strlen(cmd) + 1;

		if (*cmd) {
			int idx = bcm_strtoul(cmd, NULL, 0);

			if (idx >= 2 && idx <= TAF_MAX_MU) {
				uint32 score = method->ias->mu_pair[idx - 1];

				err = wlc_taf_param(&cmd, &score, 0,
					(1 << TAF_COEFF_IAS_MAX_BITS) -1, b);
				if (err == TAF_IOVAR_OK_SET) {
					method->ias->mu_pair[idx - 1] = score;
				}
				result->misc = method->ias->mu_pair[idx - 1];
			}
		} else {
			int idx;
			bcm_bprintf(b, "idx: pair(0-4095)\n");

			for (idx = 1; idx < TAF_MAX_MU; idx++) {
				bcm_bprintf(b, " %2u:   %4u\n", idx + 1, method->ias->mu_pair[idx]);
			}
			err = BCME_OK;
		}
		return err;
	}
	if (!strcmp(cmd, "mu_mimo_limit")) {
		int err = BCME_BADARG;

		if (method->ias->mu_mimo_rel_limit == NULL) {
			return err;
		}
		cmd += strlen(cmd) + 1;

		if (*cmd) {
			int idx = bcm_strtoul(cmd, NULL, 0);

			if (idx >= 2 && idx <= TAF_MAX_MU_MIMO) {
				uint32 score = method->ias->mu_mimo_rel_limit[idx - 1];

				err = wlc_taf_param(&cmd, &score, 0, TAF_MICROSEC_MAX, b);
				if (err == TAF_IOVAR_OK_SET) {
					method->ias->mu_mimo_rel_limit[idx - 1] = score;
				}
				result->misc = method->ias->mu_mimo_rel_limit[idx - 1];
			}
		} else {
			int idx;
			bcm_bprintf(b, "idx: mimo rel limit us (0-%u)\n", TAF_MICROSEC_MAX);

			for (idx = 1; idx < TAF_MAX_MU_MIMO; idx++) {
				bcm_bprintf(b, " %2u:   %4u\n", idx + 1,
					method->ias->mu_mimo_rel_limit[idx]);
			}
			err = BCME_OK;
		}
		return err;
	}
	if (!strcmp(cmd, "mu_mimo_opt")) {
		int err = wlc_taf_param(&local_cmd, &method->ias->mu_mimo_opt, 0, (uint32)(-1), b);

		result->misc = method->ias->mu_mimo_opt;
		return err;
	}
	if (!strcmp(cmd, "mu_ofdma_opt")) {
		int err = wlc_taf_param(&local_cmd, &method->ias->mu_ofdma_opt, 0, (uint32)(-1), b);

		result->misc = method->ias->mu_ofdma_opt;
		return err;
	}
#endif /* TAF_ENABLE_MU_TX */
	if (!strcmp(cmd, "opt")) {
		int err = wlc_taf_param(&local_cmd, &method->ias->options, 0, (uint32)(-1), b);

		result->misc = method->ias->options;
		return err;
	}
	if (!strcmp(cmd, "aggp")) {
		int err = wlc_taf_param(&local_cmd, &method->ias->opt_aggp_limit, 0, 63, b);
		result->misc = method->ias->opt_aggp_limit;
		return err;
	}
	if (!strcmp(cmd, "barrier")) {
		int err = wlc_taf_param(&local_cmd, &method->ias->barrier, 0,
			(TAF_MAX_PKT_INDEX - 1), b);
		result->misc = method->ias->barrier;
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

#ifdef TAF_PKTQ_LOG
static bool taf_ias_dpstats_counters(taf_scb_cubby_t* scb_taf,
	mac_log_counters_v06_t* mac_log, int tid, bool clear)
{
	taf_log_counters_v06_t*  taflog;
	taf_ias_sched_data_t* ias_sched = scb_taf ? TAF_IAS_TID_STATE(scb_taf, tid) : NULL;
	taf_ias_dpstats_counters_t* counters = ias_sched ?
		ias_sched->scbtidstats.dpstats_log : NULL;

	if (mac_log == NULL || (counters == NULL && ias_sched == NULL)) {
		return FALSE;
	}
	if (counters == NULL) {
		counters = (taf_ias_dpstats_counters_t*)MALLOCZ(TAF_WLCM(scb_taf->method)->pub->osh,
			sizeof(*counters));

		ias_sched->scbtidstats.dpstats_log = counters;
		return FALSE;
	}

	taflog = &mac_log->taf[tid];

	taflog->ias.emptied               = counters->emptied;
	taflog->ias.pwr_save              = counters->pwr_save;
	taflog->ias.release_pcount        = counters->release_pcount;
	taflog->ias.release_ncount        = counters->release_ncount;
	taflog->ias.ready                 = counters->ready;
	taflog->ias.release_pkt_size_mean = counters->release_pkt_size_mean;
	taflog->ias.release_frcount       = counters->release_frcount;
	taflog->ias.restricted            = counters->restricted;
	taflog->ias.limited               = counters->limited;
	taflog->ias.held_z                = counters->held_z;
	taflog->ias.release_time          = counters->release_time;
	taflog->ias.release_bytes         = counters->release_bytes;

	if (clear) {
		memset(counters, 0, sizeof(*counters));
	}
	return TRUE;
}

static uint32 taf_ias_dpstats_dump(void* handle, taf_scb_cubby_t* scb_taf,
	mac_log_counters_v06_t* mac_log, bool clear, uint32 timelo, uint32 timehi, uint32 mask)
{
	taf_method_info_t* method = handle;
	int tid;

	/* free stats memory ? */
	if ((mac_log == NULL) && (scb_taf != NULL)) {
		for (tid = 0; tid < NUMPRIO; tid++) {
			taf_ias_sched_data_t* ias_sched = TAF_IAS_TID_STATE(scb_taf, tid);
			taf_ias_dpstats_counters_t* counters = ias_sched ?
				ias_sched->scbtidstats.dpstats_log : NULL;

			if (counters) {
				MFREE(TAF_WLCM(method)->pub->osh, counters,
					sizeof(*counters));
				ias_sched->scbtidstats.dpstats_log  = NULL;
			}
		}
		return 0;
	};

	/* this is the 'auto' setting */
	if (mask & PKTQ_LOG_AUTO) {
		if (mask & PKTQ_LOG_DEF_PREC) {
			/* if was default (all TID) and AUTO mode is set,
			 * we can remove the "all TID"
			 */
			mask &= ~0xFFFF;
		} else if (mask & 0xFFFF) {
			/* this was not default, there is actual specified
			 * mask, so use it rather than automatic mode
			 */
			mask &= ~PKTQ_LOG_AUTO;
		}
	}
	mask |= PKTQ_LOG_DEVELOPMENT_VERSION;

	for (tid = 0; tid < NUMPRIO; tid++) {
		if (!(mask & (1 << tid)) && !(mask & PKTQ_LOG_AUTO)) {
			continue;
		}
		if (taf_ias_dpstats_counters(scb_taf, mac_log, tid, clear)) {
			mask |= (1 << tid);
		} else {
			mask &= ~(1 << tid);
		}
	}
	return mask;
}
#endif /* TAF_PKTQ_LOG */
#endif /* WLTAF_IAS */
