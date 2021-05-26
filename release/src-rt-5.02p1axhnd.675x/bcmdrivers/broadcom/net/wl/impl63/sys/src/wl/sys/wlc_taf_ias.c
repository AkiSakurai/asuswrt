/*
 * wlc_taf_ias.c
 *
 * This file implements the WL driver infrastructure for the TAF module.
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

const taf_scheduler_def_t taf_ias_scheduler =
{
	taf_ias_method_attach,
	taf_ias_method_detach,
	taf_ias_up,
	taf_ias_down
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

#if TAF_ENABLE_MU_TX
static uint16 taf_mu_pair[TAF_MAX_MU] = {
	0,
	360,  /* 2 */
	360,  /* 3 */
	360,  /* 4 */
	360,  /* 5 */
	360,  /* 6 */
	360,  /* 7 */
	360   /* 8 */
};
static uint16 taf_mu_mimo_rel_limit[TAF_MAX_MU_MIMO] = {
	0,
	4400, /* 2 */
	4400, /* 3 */
	4400, /* 4 */
	/* avoid flooding FIFO too much as MU users go > 4 by reducing release amount per user */
	(4400 * 4) / 5,  /* 5 */
	(4400 * 4) / 6,  /* 6 */
	(4400 * 4) / 7,  /* 7 */
	(4400 * 4) / 8,  /* 8 */
};
#endif /* TAF_ENABLE_MU_TX */

static taf_rspec_index_t
taf_ias_get_rindex(taf_scb_cubby_t *scb_taf, taf_list_type_t type, taf_tech_type_t tech)
{
	uint8 ratespec_mask = scb_taf->info.scb_stats.global.rdata.ratespec_mask;

#if TAF_ENABLE_UL
	if (type == TAF_TYPE_UL) {
		if (ratespec_mask & (1 << TAF_RSPEC_UL)) {
			return TAF_RSPEC_UL;
		}
		goto error;
	}
#endif /* TAF_ENABLE_UL */

#if TAF_ENABLE_MU_TX
	if (!scb_taf->info.ps_mode && TAF_TECH_MASK_IS_MUMIMO(1 << tech)) {
#if TAF_ENABLE_MU_BOOST
		if (ratespec_mask & (1 << TAF_RSPEC_MU_DL_BOOST)) {
			return TAF_RSPEC_MU_DL_BOOST;
		}
#endif // endif
		if (ratespec_mask & (1 << TAF_RSPEC_MU_DL_INSTANT)) {
			return TAF_RSPEC_MU_DL_INSTANT;
		}
		goto error;
	}
	if (!scb_taf->info.ps_mode && tech == TAF_TECH_DL_OFDMA) {
	}
#endif /* TAF_ENABLE_MU_TX */

	if (ratespec_mask & (1 << TAF_RSPEC_SU_DL_INSTANT)) {
		return TAF_RSPEC_SU_DL_INSTANT;
	}

	if (ratespec_mask & (1 << TAF_RSPEC_SU_DL)) {
		return TAF_RSPEC_SU_DL;
	}
error:
	WL_ERROR(("wl%u %s: "MACF"%s tech %d, ratemask 0x%x, tech_en mask 0x%x\n",
		WLCWLUNIT(TAF_WLCC(scb_taf)), __FUNCTION__, TAF_ETHERC(scb_taf), TAF_TYPE(type),
		tech, ratespec_mask, scb_taf->info.tech_enable_mask));
	TAF_ASSERT(0);
	return TAF_RSPEC_SU_DL;
}

static INLINE uint32 BCMFASTPATH
taf_ias_est_release_time(taf_scb_cubby_t *scb_taf, taf_ias_sched_data_t* ias_sched,
	int len, taf_rspec_index_t rindex, taf_list_type_t type)
{
	uint16 est_pkt_size;
	uint32 pktbytes;

	if (ias_sched == NULL) {
		TAF_ASSERT(0);
		return 0;
	}

	if (type == TAF_TYPE_DL &&
		ias_sched->rel_stats.data.release_pkt_size_mean > 0) {
		est_pkt_size = ias_sched->rel_stats.data.release_pkt_size_mean;
	} else {
		est_pkt_size = (type == TAF_TYPE_DL) ?
			TAF_PKT_SIZE_DEFAULT_DL : TAF_PKT_SIZE_DEFAULT_UL;
	}

	pktbytes = TAF_PKTBYTES_TO_UNITS(est_pkt_size,
		scb_taf->info.scb_stats.ias.data.pkt_rate[rindex],
		scb_taf->info.scb_stats.ias.data.byte_rate[rindex]);

	return pktbytes * len;
}

static INLINE uint32 BCMFASTPATH
taf_ias_get_new_traffic_estimate(taf_scb_cubby_t *scb_taf, int tid, taf_rspec_index_t rindex,
	taf_list_type_t type)
{
	uint32 est_release_total = 0;
	uint32 total_pkt_count = 0;
	taf_source_type_t s_idx;

#if TAF_ENABLE_UL
	/* UL not handled here yet */
	TAF_ASSERT(type != TAF_TYPE_UL);
#endif /* TAF_ENABLE_UL */

	for (s_idx = TAF_FIRST_REAL_SOURCE; s_idx < TAF_NUM_SCHED_SOURCES; s_idx ++) {
		if (type != TAF_SRC_TO_TYPE(s_idx)) {
			continue;
		}
		total_pkt_count += scb_taf->info.traffic.count[s_idx][tid];
	}
	if (total_pkt_count > 0) {
		est_release_total =
			taf_ias_est_release_time(scb_taf, TAF_IAS_TID_STATE(scb_taf, tid),
				total_pkt_count, rindex, type);
	}

	return est_release_total;
}

static INLINE void BCMFASTPATH
taf_ias_upd_item_stat(taf_method_info_t *method, taf_scb_cubby_t *scb_taf)
{
	wlc_taf_info_t* taf_info = method->taf_info;
	taf_ias_uni_state_t* uni_state = taf_get_uni_state(taf_info);
	taf_ias_group_info_t* ias_info = TAF_IAS_GROUP_INFO(taf_info);
	int tid_index;

	for (tid_index = 0; tid_index < TAF_MAXPRIO; tid_index++) {
		taf_ias_sched_data_t* ias_sched;
		taf_source_type_t s_idx;
		uint32 total_pkt_count[TAF_NUM_LIST_TYPES] = {0};
		int tid = tid_service_order[tid_index];

		if ((ias_sched = TAF_IAS_TID_STATE(scb_taf, tid)) == NULL) {
			continue;
		}

		ias_sched->released_units_limit[TAF_TYPE_DL] = 0;
#if TAF_ENABLE_UL
		ias_sched->released_units_limit[TAF_TYPE_UL] = 0;
#endif // endif

		ias_info->ncount_flow +=
			ias_sched->rel_stats.data.total_scaled_ncount >> TAF_PKTCOUNT_SCALE_SHIFT;

		/* this loops over AMPDU, NAR (and SQS, UL if used) */
		for (s_idx = TAF_FIRST_REAL_SOURCE; s_idx < TAF_NUM_SCHED_SOURCES; s_idx ++) {
			taf_sched_handle_t* handle = &ias_sched->handle[s_idx];

			if (!(ias_sched->used & (1 << s_idx))) {
				continue;
			}

			if ((scb_taf->info.traffic.count[s_idx][tid] =
				taf_info->funcs[s_idx].pktqlen_fn(handle->scbh, handle->tidh))) {

				taf_list_type_t type = TAF_SRC_TO_TYPE(s_idx);

				scb_taf->info.traffic.map[s_idx] |= (1 << tid);
				total_pkt_count[type] += scb_taf->info.traffic.count[s_idx][tid];

				WL_TAFM1(method, MACF" tid %u qlen %8s: %4u%s\n",
					TAF_ETHERC(scb_taf), tid, TAF_SOURCE_NAME(s_idx),
					scb_taf->info.traffic.count[s_idx][tid],
				        scb_taf->info.ps_mode ? " PS":"");
			}
		}
#if TAF_ENABLE_UL
		if (total_pkt_count[TAF_TYPE_UL] > 0) {
			scb_taf->info.traffic.est_units[TAF_TYPE_UL][tid] =
				taf_ias_est_release_time(scb_taf, ias_sched,
				total_pkt_count[TAF_TYPE_UL], TAF_RSPEC_UL, TAF_TYPE_UL);

			uni_state->est_release_units +=
				scb_taf->info.traffic.est_units[TAF_TYPE_UL][tid];

			scb_taf->info.traffic.available[TAF_TYPE_UL] |= (1 << tid);

			WL_TAFM1(method, MACF"%s estimated UL %5u units, rel_score %4u%s\n",
				TAF_ETHERC(scb_taf), TAF_TYPE(TAF_TYPE_UL),
				scb_taf->info.traffic.est_units[TAF_TYPE_UL][tid],
				scb_taf->info.scb_stats.ias.data.relative_score[TAF_TYPE_UL],
				scb_taf->info.ps_mode ? " PS":"");
		}
#endif /* TAF_ENABLE_UL */

		if (total_pkt_count[TAF_TYPE_DL] > 0) {
			taf_rspec_index_t rindex =
				scb_taf->info.scb_stats.ias.data.ridx_used[TAF_TYPE_DL];

			if ((scb_taf->info.scb_stats.global.rdata.ratespec_mask &
					(1 << rindex)) == 0) {
				TAF_ASSERT(rindex != TAF_RSPEC_SU_DL);
				rindex = TAF_RSPEC_SU_DL;
				scb_taf->info.scb_stats.ias.data.ridx_used[TAF_TYPE_DL] =
					TAF_RSPEC_SU_DL;
			}

			ias_sched->aggsf = taf_ias_aggsf(scb_taf, tid);

			if (ias_sched->aggsf == 0) {
				ias_sched->aggsf = 1;
			}

			scb_taf->info.traffic.est_units[TAF_TYPE_DL][tid] =
				taf_ias_est_release_time(scb_taf, ias_sched,
				total_pkt_count[TAF_TYPE_DL], rindex, TAF_TYPE_DL);

			uni_state->est_release_units +=
				scb_taf->info.traffic.est_units[TAF_TYPE_DL][tid];

			scb_taf->info.traffic.available[TAF_TYPE_DL] |= (1 << tid);

			WL_TAFM2(method, MACF"%s estimated %sU %5u units, pkt size %u, aggsf %u, "
				"rel_score %4u (in flight %3u)%s\n",
				TAF_ETHERC(scb_taf), TAF_TYPE(TAF_TYPE_DL),
				(rindex == TAF_RSPEC_SU_DL || rindex == TAF_RSPEC_SU_DL_INSTANT) ?
				"S" : "M", scb_taf->info.traffic.est_units[TAF_TYPE_DL][tid],
				ias_sched->rel_stats.data.release_pkt_size_mean,
				ias_sched->aggsf,
				scb_taf->info.scb_stats.ias.data.relative_score[TAF_TYPE_DL],
				SCB_PKTS_INFLT_FIFOCNT_VAL(scb_taf->scb, tid),
				scb_taf->info.ps_mode ? " PS":"");
		}

		if ((scb_taf->info.traffic.available[TAF_TYPE_DL] & (1 << tid)) ||
#if TAF_ENABLE_UL
			(scb_taf->info.traffic.available[TAF_TYPE_UL] & (1 << tid))) {
#else
			FALSE) {
#endif // endif
			TAF_DPSTATS_LOG_ADD(&ias_sched->rel_stats, ready, 1);
		} else {
			scb_taf->force &= ~(1 << tid);
		}
	}

	method->ias->sched_active |= scb_taf->info.traffic.available[TAF_TYPE_DL];
#if TAF_ENABLE_UL
	method->ias->sched_active |= scb_taf->info.traffic.available[TAF_TYPE_UL];
#endif // endif
}

static INLINE uint32 taf_ias_units_to_score(taf_method_info_t* method, taf_scb_cubby_t *scb_taf,
		int tid, taf_tech_type_t tech, uint32 units)
{
	uint8* score_weights = method->ias->score_weights;
	uint32 score = ((units * score_weights[tid]) + 128) >> 8;

#if TAF_ENABLE_MU_BOOST
	taf_scheduler_scb_stats_t* scb_stats = &scb_taf->info.scb_stats;
	wlc_taf_info_t* taf_info = method->taf_info;

	if (method->ordering == TAF_LIST_SCORE_MINIMUM && TAF_TECH_MASK_IS_MUMIMO(1 << tech)) {

		if ((taf_info->mu_boost == TAF_MUBOOST_FACTOR ||
				taf_info->mu_boost == TAF_MUBOOST_RATE_FACTOR) &&
			(scb_stats->global.rdata.mu_clients_count > 0) &&
			(scb_stats->global.rdata.mu_groups_count > 0)) {

			uint32 boost = (scb_stats->global.rdata.mu_groups_count << 10) /
				scb_stats->global.rdata.mu_clients_count;

			if (method->ias->mu_boost_compensate > 0) {
				boost = (boost * method->ias->mu_boost_compensate) >> 10;
			}

			if (boost < method->ias->mu_boost_limit) {
				boost = method->ias->mu_boost_limit;
			}

			score = (score * boost) >> 10;
		} else if (taf_info->mu_boost == TAF_MUBOOST_SU_NSS_FAIR &&
			scb_stats->global.rdata.mu_nss < scb_stats->global.rdata.max_nss) {

			/* station is NSS 2 capable but is doing NSS 1 in MU */
			if (scb_stats->global.rdata.mu_nss == 1 &&
				scb_stats->global.rdata.max_nss == 2) {
					score = score >> 1;
			} else {
				WL_ERROR(("wl%u %s: "MACF" mu nss fair %u/%u unhandled\n",
					WLCWLUNIT(TAF_WLCM(method)), __FUNCTION__,
					TAF_ETHERC(scb_taf), scb_stats->global.rdata.mu_nss,
					scb_stats->global.rdata.max_nss));
				TAF_ASSERT(0);
			}
		}
		WL_TAFM4(method, MACF" boost: score orig %u, clients %u, group %u, boost %u, "
			"limit %u, actual %u, score final %u\n",
			TAF_ETHERC(scb_taf), units * score_weights[tid] >> 8,
			scb_stats->global.rdata.mu_clients_count,
			scb_stats->global.rdata.mu_groups_count,
			((scb_stats->global.rdata.mu_groups_count << 8) /
			scb_stats->global.rdata.mu_clients_count),
			method->ias->mu_boost_limit, boost, score);
	}
#endif /* TAF_ENABLE_MU_BOOST */

#if defined(WLATF) && defined(WLATF_PERC)
	if (scb_taf->scb->sched_staperc) {
		/* If ATM percentages are applied, normalize the scores here */
		score = score / scb_taf->scb->sched_staperc;
	}
#endif // endif
	if (score == 0 && units > 0) {
		score = 1;
	}

	return score;
}

static INLINE ratespec_t BCMFASTPATH
taf_get_rate(wlc_taf_info_t *taf_info, taf_scb_cubby_t* scb_taf, taf_rspec_index_t rindex)
{
	wlc_info_t* wlc = TAF_WLCT(taf_info);
	struct scb * scb = scb_taf->scb;
	ratespec_t result = 0;
#if TAF_ENABLE_MU_BOOST
	uint8 mu_clients_count = 1;
	uint8 mu_groups_count = 1;
#endif // endif
	switch (rindex) {
		case TAF_RSPEC_SU_DL:
			result = wlc_scb_ratesel_get_primary(wlc, scb, NULL);
			TAF_ASSERT(result != 0);
			break;
		case TAF_RSPEC_SU_DL_INSTANT:
#if TAF_ENABLE_MU_TX
		case TAF_RSPEC_MU_DL_INSTANT:
#endif // endif
			result = wlc_scb_ratesel_get_opstats(wlc->wrsi, scb, AC_BE,
				RTSTAT_GET_TXS | ((rindex == TAF_RSPEC_SU_DL_INSTANT ?
				RTSTAT_GET_TYPE_SU : RTSTAT_GET_TYPE_MU) << RTSTAT_GET_TYPE_SHIFT));

			if (result == 0) {
				WL_TAFT4(taf_info, MACF" rspec is NULL (type %u)\n",
					TAF_ETHERS(scb), rindex);
			}
			break;

#if TAF_ENABLE_MU_BOOST
		case TAF_RSPEC_MU_DL_BOOST:
			result =  wlc_scb_get_sum_nss_mu_rate(wlc->wrsi, scb, AC_BE,
				&mu_clients_count, &mu_groups_count);

			if (result != 0) {
				scb_taf->info.scb_stats.global.rdata.mu_clients_count =
					mu_clients_count;
				scb_taf->info.scb_stats.global.rdata.mu_groups_count =
					mu_groups_count;
			} else {
				WL_TAFT4(taf_info, MACF" rspec is NULL (type %u)\n",
					TAF_ETHERS(scb), rindex);

			}
			break;
#endif /* TAF_ENABLE_MU_BOOST */

#if TAF_ENABLE_UL
		case TAF_RSPEC_UL:
			result = wlc_scb_ratesel_get_ulrt_rspec(wlc->wrsi, scb, 0);
			if (result == ULMU_RSPEC_INVD) {
				result = wlc_scb_ratesel_get_primary(wlc, scb, NULL);
				TAF_ASSERT(result != 0);
				break;
			}
			result |= HE_GI_TO_RSPEC(WL_RSPEC_HE_2x_LTF_GI_1_6us);
			result |= wlc_scb_ratesel_get_link_bw(wlc, scb) << WL_RSPEC_BW_SHIFT;
			break;
#endif /* TAF_ENABLE_UL */
		default:
			TAF_ASSERT(0);
	}

	WL_TAFT4(taf_info, MACF" rspec is 0x%8x (type %u)\n", TAF_ETHERS(scb), result, rindex);

	return result;
}

static INLINE void taf_ias_pkt_overhead(wlc_taf_info_t *taf_info, taf_scb_cubby_t* scb_taf,
	taf_scheduler_scb_stats_t* scb_stats, taf_rspec_index_t rindex, ratespec_t rspec)
{
	scb_stats->ias.data.pkt_rate[rindex] = (scb_stats->ias.data.byte_rate[rindex] *
		wlc_airtime_dot11hdrsize(scb_taf->scb->wsec));

#if TAF_ENABLE_NAR
	if (rindex == TAF_RSPEC_SU_DL && scb_taf->info.scb_stats.ias.data.use[TAF_SOURCE_NAR]) {

		scb_stats->ias.data.nar_byte_rate = scb_stats->ias.data.byte_rate[rindex];

		if (TAF_OPT(scb_taf->method, PKT_NAR_OVERHEAD)) {
			scb_stats->ias.data.nar_pkt_rate =
				TAF_MICROSEC_TO_UNITS(TAF_NAR_OVERHEAD) << TAF_PKTBYTES_COEFF_BITS;

			WL_TAFT2(taf_info, MACF" %s overhead %uus\n", TAF_ETHERC(scb_taf),
				TAF_SOURCE_NAME(TAF_SOURCE_NAR), TAF_NAR_OVERHEAD);
		} else {
			scb_stats->ias.data.nar_pkt_rate = scb_stats->ias.data.pkt_rate[rindex];
		}
	}
#endif /* TAF_ENABLE_NAR */
	if (TAF_OPT(scb_taf->method, PKT_AMPDU_OVERHEAD) && rindex == TAF_RSPEC_SU_DL &&
		scb_stats->ias.data.use[TAF_SOURCE_AMPDU]) {

		WL_TAFT2(taf_info, MACF" %s with rts %uus, no rts %uus, rts %uus\n",
			TAF_ETHERC(scb_taf), TAF_SOURCE_NAME(TAF_SOURCE_AMPDU),
			TAF_AMPDU_RTS_OVERHEAD,
			TAF_AMPDU_NO_RTS_OVERHEAD,
			TAF_RTS_OVERHEAD);

		scb_stats->ias.data.overhead_with_rts =
				TAF_MICROSEC_TO_UNITS(TAF_AMPDU_RTS_OVERHEAD);

		scb_stats->ias.data.overhead_without_rts =
				TAF_MICROSEC_TO_UNITS(TAF_AMPDU_NO_RTS_OVERHEAD);

		scb_stats->ias.data.overhead_rts =
				TAF_MICROSEC_TO_UNITS(TAF_RTS_OVERHEAD);
	}
}

static INLINE void BCMFASTPATH
taf_ias_rate_to_units(wlc_taf_info_t *taf_info, taf_scb_cubby_t* scb_taf, uint32 tech)
{
	taf_rspec_index_t rindex;
	ratespec_t rspec;
	taf_scheduler_scb_stats_t* scb_stats = &scb_taf->info.scb_stats;
	uint32 rate_sel = taf_info->use_sampled_rate_sel;
#if TAF_ENABLE_MU_BOOST
	uint8 boost = taf_info->mu_boost;
	bool boost_rate = (boost == TAF_MUBOOST_RATE || boost == TAF_MUBOOST_RATE_FACTOR);
	bool boost_factor = (boost == TAF_MUBOOST_FACTOR || boost == TAF_MUBOOST_RATE_FACTOR);
#endif // endif

#if TAF_ENABLE_MU_TX
	rate_sel &= (taf_info->mu | TAF_TECH_DL_SU_MASK);
#endif // endif
	rate_sel |= TAF_TECH_UL_OFDMA_MASK;
	tech &= rate_sel;

	scb_stats->global.rdata.ratespec_mask = 0;

	for (rindex = TAF_RSPEC_SU_DL; rindex < NUM_TAF_RSPECS; rindex++) {
		if (rindex == TAF_RSPEC_SU_DL_INSTANT && ((tech & TAF_TECH_DL_SU_MASK) == 0)) {
			continue;
		}

		if (TAF_RIDX_IS_UL(rindex) && !TAF_TECH_MASK_IS_UL(tech)) {
			continue;
		}

		if (TAF_RIDX_IS_MUMIMO(rindex) && !TAF_TECH_MASK_IS_MUMIMO(tech)) {
			continue;
		}
#if TAF_ENABLE_MU_BOOST
		if (TAF_RIDX_IS_BOOST(rindex) && !boost_rate && !boost_factor) {
			continue;
		}
		if (rindex == TAF_RSPEC_MU_DL_INSTANT && boost_rate) {
			continue;
		}
#endif // endif
		rspec = taf_get_rate(taf_info, scb_taf, rindex);

		if (rspec == 0) {
			WL_TAFT2(taf_info, "ridx %u rspec is 0, use SU\n", rindex);
			TAF_ASSERT(rindex > TAF_RSPEC_SU_DL);
			rspec = scb_stats->global.rdata.rspec[TAF_RSPEC_SU_DL];
		}
#if TAF_ENABLE_MU_BOOST
		if (TAF_RIDX_IS_BOOST(rindex) && !boost_rate) {
			continue;
		}
#endif // endif
		scb_stats->global.rdata.ratespec_mask |= (1 << rindex);

		if (scb_stats->global.rdata.rspec[rindex] != rspec) {
			uint32 byte_rate;

			WL_TAFT3(taf_info, MACF" updating rspec; %d:0x%8x\n",
				TAF_ETHERC(scb_taf), rindex, rspec);

			if (rindex == TAF_RSPEC_SU_DL) {
				uint32 max_bw = wlc_ratespec_bw(rspec);
#if TAF_ENABLE_MU_TX
				int idx = D11_REV128_BW_20MHZ;

				switch (max_bw) {
					case 20: break;
					case 40: idx = D11_REV128_BW_40MHZ; break;
					case 80: idx = D11_REV128_BW_80MHZ; break;
					case 160: idx = D11_REV128_BW_160MHZ; break;
					default:
						WL_ERROR(("wl%u %s: "MACF" invalid bw %u (0x%x)\n",
							WLCWLUNIT(TAF_WLCT(taf_info)), __FUNCTION__,
							TAF_ETHERC(scb_taf), max_bw, rspec));
						TAF_ASSERT(0);
				}
				scb_stats->global.rdata.bw_idx = idx;
#endif // endif
				scb_stats->global.rdata.max_bw = max_bw;
				scb_stats->global.rdata.max_nss = wlc_ratespec_nss(rspec);
#ifdef TAF_DBG
				scb_stats->global.rdata.mcs = wlc_ratespec_mcs(rspec);
				scb_stats->global.rdata.encode =
					(rspec & WL_RSPEC_ENCODING_MASK) >> WL_RSPEC_ENCODING_SHIFT;
#endif // endif
			}

#if TAF_ENABLE_MU_TX
			if (TAF_RIDX_IS_MUMIMO(rindex)) {
#ifdef TAF_DBG
				uint8 prev_nss = scb_stats->global.rdata.mu_nss;

				scb_stats->global.rdata.mu_mcs = wlc_ratespec_mcs(rspec);
				scb_stats->global.rdata.mu_encode =
					(rspec & WL_RSPEC_ENCODING_MASK) >> WL_RSPEC_ENCODING_SHIFT;

				if (prev_nss && prev_nss != scb_stats->global.rdata.mu_nss) {
					WL_TAFT2(taf_info, MACF" (%u) NSS switch ! %u --> %u\n",
						TAF_ETHERC(scb_taf), rindex,
						prev_nss, scb_stats->global.rdata.mu_nss);
				}
#endif // endif
				scb_stats->global.rdata.mu_nss = wlc_ratespec_nss(rspec);
			}
#endif /* TAF_ENABLE_MU_TX */
			scb_stats->global.rdata.rspec[rindex] = rspec;

			byte_rate =  wlc_airtime_payload_time_us(0, rspec,
				TAF_MICROSEC_TO_UNITS(TAF_PKTBYTES_COEFF));

			scb_stats->ias.data.byte_rate[rindex] = byte_rate;

			if (scb_stats->ias.data.byte_rate[rindex] == 0) {
				WL_ERROR(("wl%u %s: "MACF" null rate (type %u, rspec 0x%x)\n",
					WLCWLUNIT(TAF_WLCT(taf_info)), __FUNCTION__,
					TAF_ETHERC(scb_taf), rindex, rspec));
				TAF_ASSERT(scb_stats->ias.data.byte_rate[rindex]);
			}
			taf_ias_pkt_overhead(taf_info, scb_taf, scb_stats, rindex, rspec);
		}
	}
}

static void taf_ias_flush_rspec(taf_scb_cubby_t *scb_taf)
{
	taf_scheduler_scb_stats_t* scb_stats = &scb_taf->info.scb_stats;
	wlc_taf_info_t* taf_info = scb_taf->method->taf_info;

	memset(&scb_stats->global.rdata, 0, sizeof(scb_stats->global.rdata));

	if (scb_taf->info.tid_enabled != 0 && wlc_taf_scheduler_blocked(taf_info)) {
		/* normally rate info is re-filled at start of scheduler cycle;
		 * in case rate info is flushed mid-cycle, it needs to be specifically
		 * refreshed now as it may be required for use
		 */
		taf_ias_rate_to_units(taf_info, scb_taf,
			scb_taf->info.tech_enable_mask | TAF_TECH_DL_SU_MASK);
	}
}

static void taf_ias_clean_all_rspecs(taf_method_info_t *method)
{
	taf_list_t* list = method->list;

	while (list) {
		taf_scb_cubby_t *scb_taf = list->scb_taf;

		taf_ias_flush_rspec(scb_taf);
		list = list->next;
	}
}

static INLINE void BCMFASTPATH taf_ias_upd_item(taf_method_info_t *method, taf_scb_cubby_t* scb_taf)
{
	wlc_taf_info_t *taf_info = method->taf_info;
	taf_list_type_t type;

	/* Assume rate info is the same throughout all scheduling interval. */
	taf_ias_rate_to_units(taf_info, scb_taf,
		scb_taf->info.tech_enable_mask | TAF_TECH_DL_SU_MASK);

	for (type = TAF_TYPE_DL; type < TAF_NUM_LIST_TYPES; type++) {
		if (method->ias->total_score) { /* avoid divide by 0 */
			scb_taf->info.scb_stats.ias.data.relative_score[type] =
				(scb_taf->score[type] << TAF_COEFF_IAS_MAX_BITS) /
				method->ias->total_score;
		} else {
			scb_taf->info.scb_stats.ias.data.relative_score[type] = 0;
		}
	}

	memset(&scb_taf->info.traffic, 0, sizeof(scb_taf->info.traffic));
	memset(scb_taf->info.tech_type, TAF_TECH_UNASSIGNED, sizeof(scb_taf->info.tech_type));
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

static INLINE void BCMFASTPATH taf_ias_pre_update_list(taf_method_info_t *method)
{
	taf_list_t *item;
	uint32 total_score = 0;
	uint32 now_time = TAF_IAS_NOWTIME(method);

	for (item = method->list; item; item = item->next) {
		int32  time_correction = 0;
		uint32 elapsed;
		uint32 score;
		taf_scb_cubby_t* scb_taf = item->scb_taf;

		if (SCB_MARKED_DEL(scb_taf->scb) || SCB_DEL_IN_PROGRESS(scb_taf->scb)) {
			scb_taf->force = 0;
			/* max score means least priority for IAS */
			scb_taf->score[item->type] = TAF_SCORE_MAX;
			WL_TAFM1(method, MACF"%s will be deleted\n", TAF_ETHERC(scb_taf),
				TAF_TYPE(item->type));
			continue;
		}
		if (scb_taf->info.tid_enabled == 0) {
			WL_TAFM4(method, MACF"%s not active\n", TAF_ETHERC(scb_taf),
				TAF_TYPE(item->type));
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
				WL_TAFM4(method, MACF" type %d score %d\n",
					TAF_ETHERC(scb_taf), item->type,
					scb_taf->score[item->type]);
				break;
			default:
				TAF_ASSERT(0);
				break;
		}
		total_score += scb_taf->score[item->type];
	}
	method->ias->total_score = total_score;
}

static INLINE void BCMFASTPATH taf_ias_post_update_list(taf_method_info_t *method)
{
	taf_list_t *item;

	for (item = method->list; item; item = item->next) {
		taf_scb_cubby_t* scb_taf = item->scb_taf;

		if ((scb_taf->info.tid_enabled == 0) || SCB_MARKED_DEL(scb_taf->scb) ||
			SCB_DEL_IN_PROGRESS(scb_taf->scb) || (item->type != TAF_TYPE_DL)) {
			continue;
		}
		taf_ias_upd_item(method, scb_taf);
		taf_ias_upd_item_stat(method, scb_taf);
	}
}

#if TAF_ENABLE_TIMER
static void taf_ias_agghold_stat(wlc_taf_info_t * taf_info, taf_ias_group_info_t* ias_info,
	uint32 time)
{
	uint32 elapsed = 0;

	if  (ias_info->agg_hold_start_time != 0) {

		elapsed = time - ias_info->agg_hold_start_time;
		++ias_info->debug.agg_hold_count;
		ias_info->agg_hold_start_time = 0;

		WL_TAFT2(taf_info, "%s ended, %uus elapsed\n",
			ias_info->stall_prevent_timer ? "retrigger" : "aggregation hold",
			elapsed);
	} else {
		TAF_ASSERT(0);
	}

	if (!ias_info->stall_prevent_timer && elapsed > ias_info->debug.agg_hold_max_time) {
		ias_info->debug.agg_hold_max_time = elapsed;
		WL_TAFT4(taf_info, "new max hold %u\n", ias_info->debug.agg_hold_max_time);
	}
	if (!ias_info->stall_prevent_timer) {
		ias_info->debug.agg_hold_total_time += (uint64)elapsed;
	}
}

static void taf_ias_aggh_tmr_exp(void *arg)
{
	wlc_taf_info_t * taf_info = (wlc_taf_info_t *) arg;
	taf_ias_group_info_t* ias_info = TAF_IAS_GROUP_INFO(taf_info);

	TAF_ASSERT(ias_info);

	ias_info->is_agg_hold_timer_running = FALSE;
	ias_info->agg_hold_expired = TRUE;
#if !TAF_ENABLE_SQS_PULL
	ias_info->agg_hold_exit_pending = FALSE;
#endif // endif
	ias_info->now_time = taf_timestamp(TAF_WLCT(taf_info));

	WL_TAFT2(taf_info, "%s timer expired\n",
		ias_info->stall_prevent_timer ? "retrigger" : "aggregation hold");
	taf_ias_agghold_stat(taf_info, ias_info, ias_info->now_time);

	ias_info->stall_prevent_timer = FALSE;

	if (!wlc_taf_scheduler_blocked(taf_info)) {
#if TAF_ENABLE_SQS_PULL
		ias_info->agg_hold_exit_pending = FALSE;
#endif // endif
		wlc_taf_schedule(taf_info, TAF_DEFAULT_UNIFIED_TID, NULL, FALSE);
	}
}

static int taf_ias_agghold_exp(taf_ias_group_info_t* ias_info)
{
	return ias_info->agg_hold_expired;
}

static int taf_ias_agghold_start(wlc_taf_info_t * taf_info, int ms, uint32 time)
{
	taf_ias_group_info_t* ias_info = TAF_IAS_GROUP_INFO(taf_info);

	if (ias_info->agg_hold_prevent) {
		return BCME_NOTREADY;
	}
	if (ias_info->is_agg_hold_timer_running) {
		return BCME_BUSY;
	}
	TAF_ASSERT(ias_info->agg_hold_start_time == 0);
	wl_add_timer(TAF_WLCT(taf_info)->wl, ias_info->agg_hold_timer, ms, FALSE);

	ias_info->is_agg_hold_timer_running = TRUE;
	ias_info->agg_hold_exit_pending = TRUE;
	ias_info->agg_hold_start_time = time;
	ias_info->stall_prevent_timer = FALSE;

	WL_TAFT2(taf_info, "agg hold timer started %ums\n", ms);
	return BCME_OK;
}

static bool taf_ias_agghold_stop(wlc_taf_info_t * taf_info, taf_ias_group_info_t* ias_info,
	uint32 time)
{
	if (ias_info->is_agg_hold_timer_running) {
		if (wl_del_timer(TAF_WLCT(taf_info)->wl, ias_info->agg_hold_timer)) {
			taf_ias_agghold_stat(taf_info, ias_info, time);

			ias_info->is_agg_hold_timer_running = FALSE;
		}
	}
	ias_info->agg_hold_exit_pending = FALSE;
	WL_TAFT3(taf_info, "%u\n", ias_info->is_agg_hold_timer_running);

	ias_info->stall_prevent_timer = FALSE;

	return !ias_info->is_agg_hold_timer_running;
}

static INLINE bool taf_ias_agghold_is_active(taf_ias_group_info_t* ias_info)
{
	TAF_ASSERT(!ias_info->agg_hold_expired || !ias_info->is_agg_hold_timer_running);
	return ias_info->is_agg_hold_timer_running || ias_info->agg_hold_exit_pending;
}

static INLINE bool taf_ias_agghold_was_active(taf_ias_group_info_t* ias_info)
{
	return ias_info->agg_hold_expired;
}

static INLINE void taf_ias_agghold_clear(taf_ias_group_info_t* ias_info)
{
	ias_info->agg_hold_expired = FALSE;
	ias_info->agg_hold_prevent = FALSE;
	ias_info->stall_prevent_timer = FALSE;
}

static INLINE void taf_ias_agghold_prevent(taf_ias_group_info_t* ias_info)
{
	ias_info->agg_hold_prevent = TRUE;
}

static INLINE bool taf_ias_agghold_reset(taf_method_info_t * method)
{
	taf_ias_group_info_t* ias_info = TAF_IAS_GROUP_INFO(method->taf_info);
	taf_ias_agghold_clear(ias_info);
	return taf_ias_agghold_stop(method->taf_info, ias_info, NULL);
}

static int taf_ias_retrig_start(wlc_taf_info_t * taf_info, uint32 time)
{
	taf_ias_group_info_t* ias_info = TAF_IAS_GROUP_INFO(taf_info);
	uint32 ms;

	if (ias_info->is_agg_hold_timer_running) {
		return BCME_BUSY;
	}

	TAF_ASSERT(!ias_info->agg_hold_prevent);
	TAF_ASSERT(ias_info->agg_hold_start_time == 0);
	TAF_ASSERT(!ias_info->agg_hold_exit_pending);

	if (wlc_taf_uladmit_count(taf_info, TRUE) == 0) {
		ms = TAF_IAS_RETRIG_TIME_NORM;
	} else {
		ms = TAF_IAS_RETRIG_TIME_UL;
	}

	wl_add_timer(TAF_WLCT(taf_info)->wl, ias_info->agg_hold_timer, ms, FALSE);

	ias_info->is_agg_hold_timer_running = TRUE;
	ias_info->agg_hold_start_time = time;
	ias_info->stall_prevent_timer = TRUE;

	WL_TAFT2(taf_info, "retrigger timer started %ums\n", ms);
	return BCME_OK;
}

static INLINE bool taf_ias_retrig_is_active(taf_ias_group_info_t* ias_info)
{
	TAF_ASSERT(!ias_info->agg_hold_expired || !ias_info->is_agg_hold_timer_running);
	return ias_info->is_agg_hold_timer_running && ias_info->stall_prevent_timer;
}

static bool taf_ias_retrig_stop(wlc_taf_info_t * taf_info, taf_ias_group_info_t* ias_info,
	uint32 time)
{
	return taf_ias_agghold_stop(taf_info, ias_info, time);
}
#endif /* TAF_ENABLE_TIMER */

static void taf_ias_up(void * context)
{
	taf_method_info_t *method = (taf_method_info_t *) context;

	BCM_REFERENCE(method);
}

static int taf_ias_down(void * context)
{
	taf_method_info_t *method = (taf_method_info_t *) context;
	int callbacks = 0;

#if TAF_ENABLE_TIMER
	if (!taf_ias_agghold_reset(method)) {
		callbacks++;
	}
#else
	BCM_REFERENCE(method);
#endif /* TAF_ENABLE_TIMER */
	return callbacks;
}

#if TAF_ENABLE_MU_TX
static INLINE bool taf_ias_break_mu_pair(taf_method_info_t* method, uint32 inter_count,
uint32 inter_extend, uint32 min_mu_traffc)
{
	if (inter_count > ((min_mu_traffc * TAF_MU_PAIR_BREAK_FRAC16) >> 4)) {
		WL_TAFM2(method, "too much intermediate traffic found (%u/%u)\n",
			inter_extend, min_mu_traffc / 8);
		return TRUE;
	}
	return FALSE;
}

static INLINE uint32 taf_ias_tech_alloc(taf_method_info_t* method, taf_scb_cubby_t **scb_taf_p,
	uint8 mu_user_count, uint32 rel_limit_units, taf_tech_type_t tech_idx, int tid,
	taf_list_type_t type)
{
	int index;
	uint32 total_mu_traffic = 0;

	for (index = 0; index < mu_user_count; index++) {
		taf_scb_cubby_t * scb_taf = *scb_taf_p++;

		scb_taf->info.tech_type[type][tid] = tech_idx;

		if (tech_idx < TAF_NUM_TECH_TYPES) {
			taf_ias_sched_data_t* ias_sched = TAF_IAS_TID_STATE(scb_taf, tid);

			ias_sched->released_units_limit[type] = rel_limit_units;
			total_mu_traffic += rel_limit_units;

			WL_TAFM2(method, "set rel limit %u to "MACF"%s tid %u %s\n",
				ias_sched->released_units_limit[type], TAF_ETHERC(scb_taf),
				TAF_TYPE(type), tid, TAF_TECH_NAME(tech_idx));
		}
	}
	return total_mu_traffic;
}

static INLINE bool BCMFASTPATH taf_ias_dl_ofdma_suitable(taf_scb_cubby_t *scb_taf, int tid,
	taf_tech_type_t tech_idx)
{
	if (scb_taf->info.ps_mode) {
		return FALSE;
	}
	if ((scb_taf->info.mu_tech_en[tech_idx] & (1 << tid)) == 0) {
		return FALSE;
	}
	if (SCB_MARKED_DEL(scb_taf->scb) || SCB_DEL_IN_PROGRESS(scb_taf->scb)) {
		return FALSE;
	}
	if ((scb_taf->info.tid_enabled & (1 << tid)) == 0) {
		return FALSE;
	}
	if (!scb_taf->info.scb_stats.ias.data.use[TAF_SOURCE_AMPDU] ||
		scb_taf->info.linkstate[TAF_SOURCE_AMPDU][tid] != TAF_LINKSTATE_ACTIVE) {
		return FALSE;
	}
	if (scb_taf->info.tech_type[TAF_TYPE_DL][tid] < TAF_TECH_UNASSIGNED) {
		return FALSE;
	}
	if (scb_taf->info.traffic.est_units[TAF_TYPE_DL][tid] == 0) {
		return FALSE;
	}
	if (RSPEC_ISCCK(scb_taf->info.scb_stats.global.rdata.rspec[TAF_RSPEC_SU_DL])) {
		return FALSE;
	}
	return TRUE;
}

static INLINE bool BCMFASTPATH taf_ias_dl_ofdma(taf_method_info_t* method, taf_tech_type_t tech_idx,
	taf_list_t *list, taf_ias_uni_state_t *uni_state, int tid)
{
	wlc_taf_info_t* taf_info = method->taf_info;
	taf_scb_cubby_t *items[TAF_MAX_MU_OFDMA];
	taf_scb_cubby_t *scb_taf = list->scb_taf;
	taf_scb_cubby_t *scb_taf_next = NULL;
	uint32 inter_item[TAF_MAX_MU_OFDMA];
	uint32 est_inter_total = 0;
	uint32 inter_extend = 0;
#if TAF_ENABLE_UL
	uint32 inter_count = 0;
#endif // endif
	uint32 mu_user_count = 0;
	uint32 min_mu_traffc;
	uint32 time_limit_units;
	uint32 user_time_limit_units;
	uint32 total_mu_traffic = 0;
	int bw_idx = scb_taf->info.scb_stats.global.rdata.bw_idx;

	if (list->type != TAF_TYPE_DL) {
		return FALSE;
	}

	TAF_ASSERT(tech_idx == TAF_TECH_DL_OFDMA);
	TAF_ASSERT(taf_info->dlofdma_maxn[bw_idx] > 0);

	time_limit_units = TAF_MICROSEC_TO_UNITS(uni_state->high[IAS_INDEXM(method)]);
	if (time_limit_units <= uni_state->total.released_units) {
		return FALSE;
	}
	time_limit_units -= uni_state->total.released_units;

	if (!taf_ias_dl_ofdma_suitable(scb_taf, tid, tech_idx)) {
		return FALSE;
	}

	items[mu_user_count] = scb_taf;
	inter_item[mu_user_count] = 0;
	mu_user_count++;

	WL_TAFM2(method, "this %s item is "MACF"%s having %u units tid %u and score %u, "
		"rel score %u, bw_idx %u (opt %u)\n",
		TAF_TECH_NAME(tech_idx), TAF_ETHERC(scb_taf), TAF_TYPE(TAF_TYPE_DL),
		scb_taf->info.traffic.est_units[TAF_TYPE_DL][tid], tid, scb_taf->score[TAF_TYPE_DL],
		scb_taf->info.scb_stats.ias.data.relative_score[TAF_TYPE_DL], bw_idx,
		method->ias->mu_ofdma_opt);

	min_mu_traffc = scb_taf->info.traffic.est_units[TAF_TYPE_DL][tid];

	while (list) {
		uint32 fraction;

		inter_item[mu_user_count] = 0;

		for (list = list->next; list; list = list->next) {
			scb_taf_next = list->scb_taf;

			if ((list->type == TAF_TYPE_DL) &&
				taf_ias_dl_ofdma_suitable(scb_taf_next, tid, tech_idx)) {
				break;
			}
			if (scb_taf_next->info.tech_type[list->type][tid] != TAF_TECH_DONT_ASSIGN) {
				uint32 est = scb_taf_next->info.traffic.est_units[list->type][tid];

				if (est > 0 && (list->type == TAF_TYPE_DL ||
						!TAF_OFDMA_OPT(method, INT_EXTEND))) {
					inter_item[mu_user_count] += est;
					WL_TAFM3(method, "increasing inter traffic %u to %u from "
						MACF"%s tid %u est units %u\n", mu_user_count,
						inter_item[mu_user_count], TAF_ETHERC(scb_taf_next),
						TAF_TYPE(list->type), tid, est);
				}
#if TAF_ENABLE_UL
				else if (est > 0) {
					inter_count += est;
					WL_TAFM3(method, "increasing inter count to %u from "
						MACF"%s tid %u est units %u\n",
						inter_count, TAF_ETHERC(scb_taf_next),
						TAF_TYPE(list->type), tid, est);
				}
#endif // endif
			}
		}

		if (!list || !scb_taf_next) {
			WL_TAFM3(method, "no next %u %s item found\n", mu_user_count + 1,
				TAF_TECH_NAME(tech_idx));
			break;
		}

		if (inter_item[mu_user_count] > 0 && !TAF_OFDMA_OPT(method, SWEEP)) {
			WL_TAFM2(method, "intermediate traffic found\n");
			break;
		}
#if TAF_ENABLE_UL
		if (taf_ias_break_mu_pair(method, inter_count, inter_extend, min_mu_traffc)) {
			break;
		}
		inter_extend = inter_count;
#endif // endif
		WL_TAFM2(method, "next %s item is "MACF"%s having %u units tid %u and score "
			"%u, rel_score %u (inter traffic %u)\n",
			TAF_TECH_NAME(tech_idx), TAF_ETHERC(scb_taf_next),  TAF_TYPE(list->type),
			scb_taf_next->info.traffic.est_units[TAF_TYPE_DL][tid], tid,
			scb_taf_next->score[TAF_TYPE_DL],
			scb_taf_next->info.scb_stats.ias.data.relative_score[TAF_TYPE_DL],
			inter_item[mu_user_count]);

		if (TAF_OFDMA_OPT(method, PAIR)) {
			uint32 pro_limit = (scb_taf->score[TAF_TYPE_DL] *
				method->ias->mu_pair[mu_user_count]) >> TAF_COEFF_IAS_MAX_BITS;

			uint32 diff = scb_taf_next->score[TAF_TYPE_DL] -
				scb_taf->score[TAF_TYPE_DL];

			if (diff > pro_limit) {
				WL_TAFM2(method, "score diff %u "MACF"%s is too large "
					"(limit %u/%u)\n", diff,
					TAF_ETHERC(scb_taf_next), TAF_TYPE(list->type),
					pro_limit, method->ias->mu_pair[mu_user_count]);
				break;
			}
		}

		fraction = (mu_user_count + 1) *
			scb_taf_next->info.traffic.est_units[TAF_TYPE_DL][tid];

		if (TAF_OFDMA_OPT(method, TOO_SMALL) &&
			(fraction < time_limit_units) && (fraction < min_mu_traffc)) {

			WL_TAFM2(method, "traffic available is too small (%u/%u/%u)\n",
				fraction, time_limit_units, min_mu_traffc);
			break;
		}
		if (TAF_OFDMA_OPT(method, TOO_LARGE) &&
			(((mu_user_count + 1) * min_mu_traffc) < time_limit_units) &&
			(scb_taf_next->info.traffic.est_units[TAF_TYPE_DL][tid] >
			time_limit_units)) {

			WL_TAFM2(method, "traffic available too large for pairing "
				"(%u/%u)\n",  time_limit_units, min_mu_traffc);
			break;
		}

		items[mu_user_count] = scb_taf_next;
		est_inter_total += inter_item[mu_user_count];

		if (scb_taf_next->info.traffic.est_units[TAF_TYPE_DL][tid] < min_mu_traffc) {
			min_mu_traffc = scb_taf_next->info.traffic.est_units[TAF_TYPE_DL][tid];
		}
		bw_idx = MAX(bw_idx, scb_taf_next->info.scb_stats.global.rdata.bw_idx);
		TAF_ASSERT(bw_idx <= D11_REV128_BW_160MHZ);
		mu_user_count++;

		if (mu_user_count >= TAF_MAX_MU_OFDMA) {
			WL_TAFM2(method, "ending due to mu_user_count limit (%u)\n", mu_user_count);
			break;
		}
		if (TAF_OFDMA_OPT(method, MAXN_REL) &&
			(mu_user_count >= taf_info->dlofdma_maxn[bw_idx])) {
			WL_TAFM2(method, "ending due to maxN mu_user_count (%u)\n", mu_user_count);
			break;
		}
	}

	if (mu_user_count < 2) {
		WL_TAFM2(method, "only %u %s user possible\n", mu_user_count,
			TAF_TECH_NAME(tech_idx));
		return FALSE;
	}

	if (est_inter_total == 0) {
		uint32 user_base_count = MIN(mu_user_count, taf_info->dlofdma_maxn[bw_idx]);

		user_time_limit_units = time_limit_units / user_base_count;

		user_time_limit_units = MIN(user_time_limit_units, min_mu_traffc);

		if (method->ias->release_limit > 0) {
			uint32 global_rel_limit = TAF_MICROSEC_TO_UNITS(method->ias->release_limit);
			user_time_limit_units = MIN(user_time_limit_units, global_rel_limit);
		}

		WL_TAFM1(method, "final %s possible: %u users, %u traffic possible per user, "
			"bw_idx %u - maxN %u\n",
			TAF_TECH_NAME(tech_idx), mu_user_count, user_time_limit_units, bw_idx,
			taf_info->dlofdma_maxn[bw_idx]);

	} else {
		WL_TAFM1(method, "intermediate traffic count %u so skip %s release this frame\n",
			est_inter_total, TAF_TECH_NAME(tech_idx));
		tech_idx = TAF_TECH_DONT_ASSIGN;
		user_time_limit_units = 0;
	}

	total_mu_traffic = taf_ias_tech_alloc(method, items, mu_user_count, user_time_limit_units,
		tech_idx, tid, TAF_TYPE_DL);

	if (total_mu_traffic + inter_extend > time_limit_units) {
		uni_state->extend.window_dur_units +=
			total_mu_traffic + inter_extend - time_limit_units;
#if TAF_ENABLE_UL
		if (inter_extend > 0) {
			WL_TAFM2(method, "extend release due to non DL intermediate traffic (%u)\n",
				inter_extend);
		}
#endif // endif
		WL_TAFM2(method, "%s extend units %u\n", TAF_TECH_NAME(tech_idx),
			total_mu_traffic + inter_extend - time_limit_units);
	}
	return TRUE;
}

#if TAF_ENABLE_UL
static INLINE bool BCMFASTPATH taf_ias_ul_ofdma_suitable(taf_scb_cubby_t *scb_taf, int tid,
	taf_tech_type_t tech_idx)
{
	if (scb_taf->info.ps_mode) {
		return FALSE;
	}
	if (SCB_MARKED_DEL(scb_taf->scb) || SCB_DEL_IN_PROGRESS(scb_taf->scb)) {
		return FALSE;
	}
	if ((scb_taf->info.tid_enabled & (1 << tid)) == 0) {
		return FALSE;
	}
	if (!scb_taf->info.scb_stats.ias.data.use[TAF_SOURCE_UL] ||
		scb_taf->info.linkstate[TAF_SOURCE_UL][tid] != TAF_LINKSTATE_ACTIVE) {
		return FALSE;
	}
	if (scb_taf->info.tech_type[TAF_TYPE_UL][tid] < TAF_TECH_UNASSIGNED) {
		return FALSE;
	}
	if (scb_taf->info.traffic.est_units[TAF_TYPE_UL][tid] == 0) {
		return FALSE;
	}
	return (scb_taf->info.mu_tech_en[tech_idx] & (1 << tid)) ? TRUE : FALSE;
}

static INLINE bool BCMFASTPATH taf_ias_ul_ofdma(taf_method_info_t* method, taf_tech_type_t tech_idx,
	taf_list_t *list, taf_ias_uni_state_t *uni_state, int tid)
{
	wlc_taf_info_t* taf_info = method->taf_info;
	taf_scb_cubby_t *items[TAF_MAX_MU_OFDMA];
	taf_scb_cubby_t *scb_taf = list->scb_taf;
	taf_scb_cubby_t *scb_taf_next = NULL;
	uint32 inter_item[TAF_MAX_MU_OFDMA];
	uint32 est_inter_total = 0;
	uint32 inter_extend = 0;
	uint32 inter_count = 0;
	uint32 mu_user_count = 0;
	uint32 min_mu_traffc;
	uint32 time_limit_units;
	uint32 user_time_limit_units;
	uint32 total_mu_traffic = 0;
	int bw_idx = scb_taf->info.scb_stats.global.rdata.bw_idx;

	if (list->type != TAF_TYPE_UL) {
		return FALSE;
	}

	TAF_ASSERT(tech_idx == TAF_TECH_UL_OFDMA);
	TAF_ASSERT(taf_info->ulofdma_maxn[bw_idx] > 0);

	time_limit_units = TAF_MICROSEC_TO_UNITS(uni_state->high[IAS_INDEXM(method)]);
	if (time_limit_units <= uni_state->total.released_units) {
		return FALSE;
	}
	time_limit_units -= uni_state->total.released_units;

	if (!taf_ias_ul_ofdma_suitable(scb_taf, tid, tech_idx)) {
		return FALSE;
	}

	items[mu_user_count] = scb_taf;
	inter_item[mu_user_count] = 0;
	mu_user_count++;

	WL_TAFM2(method, "this %s item is "MACF"%s having %u units tid %u and score %u, "
		"rel score %u, bw_idx %u (opt %u)\n",
		TAF_TECH_NAME(tech_idx), TAF_ETHERC(scb_taf), TAF_TYPE(TAF_TYPE_UL),
		scb_taf->info.traffic.est_units[TAF_TYPE_UL][tid], tid, scb_taf->score[TAF_TYPE_UL],
		scb_taf->info.scb_stats.ias.data.relative_score[TAF_TYPE_UL], bw_idx,
		method->ias->mu_ofdma_opt);

	min_mu_traffc = scb_taf->info.traffic.est_units[TAF_TYPE_UL][tid];

	while (list) {
		uint32 fraction;

		inter_item[mu_user_count] = 0;

		for (list = list->next; list; list = list->next) {
			scb_taf_next = list->scb_taf;

			if ((list->type == TAF_TYPE_UL) &&
				taf_ias_ul_ofdma_suitable(scb_taf_next, tid, tech_idx)) {
				break;
			}
			if (scb_taf_next->info.tech_type[list->type][tid] != TAF_TECH_DONT_ASSIGN) {
				uint32 est = scb_taf_next->info.traffic.est_units[list->type][tid];

				if (est > 0 && (list->type == TAF_TYPE_UL ||
					!TAF_OFDMA_OPT(method, INT_EXTEND))) {

					inter_item[mu_user_count] += est;
					WL_TAFM3(method, "increasing inter traffic %u to %u from "
						MACF"%s tid %u est units %u\n", mu_user_count,
						inter_item[mu_user_count], TAF_ETHERC(scb_taf_next),
						TAF_TYPE(list->type), tid, est);
				} else if (est > 0) {
					inter_count += est;
					WL_TAFM3(method, "increasing inter count to %u from "
						MACF"%s tid %u est units %u\n",
						inter_count, TAF_ETHERC(scb_taf_next),
						TAF_TYPE(list->type), tid, est);
				}
			}
		}

		if (!list || !scb_taf_next) {
			WL_TAFM3(method, "no next %u %s item found\n", mu_user_count + 1,
				TAF_TECH_NAME(tech_idx));
			break;
		}

		if (inter_item[mu_user_count] > 0 && !TAF_OFDMA_OPT(method, SWEEP)) {
			WL_TAFM2(method, "intermediate traffic found\n");
			break;
		}
		if (taf_ias_break_mu_pair(method, inter_count, inter_extend, min_mu_traffc)) {
			break;
		}
		inter_extend = inter_count;

		WL_TAFM2(method, "next %s item is "MACF"%s having %u units tid %u and score "
			"%u, rel_score %u (inter traffic %u)\n",
			TAF_TECH_NAME(tech_idx), TAF_ETHERC(scb_taf_next), TAF_TYPE(list->type),
			scb_taf_next->info.traffic.est_units[TAF_TYPE_UL][tid], tid,
			scb_taf_next->score[TAF_TYPE_UL],
			scb_taf_next->info.scb_stats.ias.data.relative_score[TAF_TYPE_UL],
			inter_item[mu_user_count]);

		if (TAF_OFDMA_OPT(method, PAIR)) {
			uint32 pro_limit = (scb_taf->score[TAF_TYPE_UL] *
				method->ias->mu_pair[mu_user_count]) >> TAF_COEFF_IAS_MAX_BITS;

			uint32 diff = scb_taf_next->score[TAF_TYPE_UL] -
				scb_taf->score[TAF_TYPE_UL];

			if (diff > pro_limit) {
				WL_TAFM2(method, "score diff %u "MACF"%s is too large "
					"(limit %u/%u)\n", diff,
					TAF_ETHERC(scb_taf_next), TAF_TYPE(list->type),
					pro_limit, method->ias->mu_pair[mu_user_count]);
				break;
			}
		}

		fraction = (mu_user_count + 1) *
			scb_taf_next->info.traffic.est_units[TAF_TYPE_UL][tid];

		if (TAF_OFDMA_OPT(method, TOO_SMALL) &&
			(fraction < time_limit_units) && (fraction < min_mu_traffc)) {

			WL_TAFM2(method, "traffic available is too small (%u/%u/%u)\n",
				fraction, time_limit_units, min_mu_traffc);
			break;
		}
		if (TAF_OFDMA_OPT(method, TOO_LARGE) &&
			(((mu_user_count + 1) * min_mu_traffc) < time_limit_units) &&
			(scb_taf_next->info.traffic.est_units[TAF_TYPE_UL][tid] >
			time_limit_units)) {

			WL_TAFM2(method, "traffic available too large for pairing "
				"(%u/%u)\n",  time_limit_units, min_mu_traffc);
			break;
		}

		items[mu_user_count] = scb_taf_next;
		est_inter_total += inter_item[mu_user_count];

		if (scb_taf_next->info.traffic.est_units[TAF_TYPE_UL][tid] < min_mu_traffc) {
			min_mu_traffc = scb_taf_next->info.traffic.est_units[TAF_TYPE_UL][tid];
		}
		bw_idx = MAX(bw_idx, scb_taf_next->info.scb_stats.global.rdata.bw_idx);
		TAF_ASSERT(bw_idx <= D11_REV128_BW_160MHZ);
		mu_user_count++;

		if (mu_user_count >= TAF_MAX_MU_OFDMA) {
			WL_TAFM2(method, "ending due to mu_user_count limit (%u)\n", mu_user_count);
			break;
		}
		if (TAF_OFDMA_OPT(method, MAXN_REL) &&
				(mu_user_count >= taf_info->ulofdma_maxn[bw_idx])) {
			WL_TAFM2(method, "ending due to maxN mu_user_count (%u)\n", mu_user_count);
			break;
		}
	}

	if (mu_user_count < 2) {
		WL_TAFM2(method, "only %u %s user possible\n", mu_user_count,
			TAF_TECH_NAME(tech_idx));
		return FALSE;
	}

	if (est_inter_total == 0) {
		uint32 user_base_count = MIN(mu_user_count, taf_info->ulofdma_maxn[bw_idx]);

		user_time_limit_units = time_limit_units / user_base_count;

		user_time_limit_units = MIN(user_time_limit_units, min_mu_traffc);

		if (method->ias->release_limit > 0) {
			uint32 global_rel_limit = TAF_MICROSEC_TO_UNITS(method->ias->release_limit);
			user_time_limit_units = MIN(user_time_limit_units, global_rel_limit);
		}

		WL_TAFM1(method, "final %s possible: %u users, %u traffic possible per user, "
			"bw_idx %u - maxN %u\n",
			TAF_TECH_NAME(tech_idx), mu_user_count, user_time_limit_units, bw_idx,
			taf_info->ulofdma_maxn[bw_idx]);

	} else {
		WL_TAFM1(method, "intermediate traffic count %u so skip %s release this frame\n",
			est_inter_total, TAF_TECH_NAME(tech_idx));
		tech_idx = TAF_TECH_DONT_ASSIGN;
		user_time_limit_units = 0;
	}

	total_mu_traffic = taf_ias_tech_alloc(method, items, mu_user_count, user_time_limit_units,
			tech_idx, tid, TAF_TYPE_UL);

	if (total_mu_traffic + inter_extend > time_limit_units) {
		uni_state->extend.window_dur_units += total_mu_traffic + inter_extend -
			time_limit_units;

		if (inter_extend > 0) {
			WL_TAFM2(method, "extend release due to non UL intermediate traffic (%u)\n",
				inter_extend);
		}
		WL_TAFM2(method, "%s extend units %u\n", TAF_TECH_NAME(tech_idx),
			total_mu_traffic + inter_extend - time_limit_units);
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
	if (SCB_MARKED_DEL(scb_taf->scb) || SCB_DEL_IN_PROGRESS(scb_taf->scb)) {
		return FALSE;
	}
	if ((scb_taf->info.tid_enabled & (1 << tid)) == 0) {
		return FALSE;
	}
	if (!scb_taf->info.scb_stats.ias.data.use[TAF_SOURCE_AMPDU] ||
		scb_taf->info.linkstate[TAF_SOURCE_AMPDU][tid] != TAF_LINKSTATE_ACTIVE) {
		return FALSE;
	}
	if (scb_taf->info.tech_type[TAF_TYPE_DL][tid] < TAF_TECH_UNASSIGNED) {
		return FALSE;
	}
	if (scb_taf->info.traffic.est_units[TAF_TYPE_DL][tid] == 0) {
		return FALSE;
	}
	if (taf_bw(scb_taf) != bw) {
		return FALSE;
	}
	if ((streams = taf_nss(scb_taf)) >= streams_max || (streams_max == 4 && streams == 3)) {
		return FALSE;
	}

	return (scb_taf->info.mu_tech_en[tech_idx] & (1 << tid)) ? TRUE : FALSE;
}

static INLINE uint32 taf_ias_check_upd_rate(taf_scb_cubby_t *scb_taf, taf_tech_type_t tech_idx,
	int tid, taf_ias_uni_state_t *uni_state, taf_list_type_t type)
{
	taf_rspec_index_t r_idx = taf_ias_get_rindex(scb_taf, type, tech_idx);

	if (r_idx != scb_taf->info.scb_stats.ias.data.ridx_used[type] &&
		scb_taf->info.scb_stats.global.rdata.ratespec_mask & (1 << r_idx)) {

		if (uni_state->est_release_units > scb_taf->info.traffic.est_units[type][tid]) {
			uni_state->est_release_units  -= scb_taf->info.traffic.est_units[type][tid];
		} else {
			uni_state->est_release_units = 0;
		}
		scb_taf->info.traffic.est_units[type][tid] =
			taf_ias_get_new_traffic_estimate(scb_taf, tid, r_idx, type);

		uni_state->est_release_units += scb_taf->info.traffic.est_units[type][tid];
		scb_taf->info.scb_stats.ias.data.ridx_used[type] = r_idx;
	}
	return scb_taf->info.traffic.est_units[type][tid];
}

static INLINE bool BCMFASTPATH taf_ias_dl_mimo(taf_method_info_t* method, taf_tech_type_t tech_idx,
	taf_list_t *list, taf_ias_uni_state_t *uni_state, int tid)
{
	taf_scb_cubby_t *items[TAF_MAX_MU_MIMO];
	uint32 traffic_estimate[TAF_MAX_MU_MIMO];
	taf_scb_cubby_t *scb_taf = list->scb_taf;
	taf_scb_cubby_t *scb_taf_next = NULL;
	uint32 inter_item[TAF_MAX_MU_MIMO];
	uint32 est_inter_total = 0;
	uint32 inter_extend = 0;
#if TAF_ENABLE_UL
	uint32 inter_count = 0;
#endif // endif
	uint32 mu_user_count = 1;
	uint32 time_limit_units;
	uint32 streams = 0;
	uint32 streams_max = TAF_WLCM(method)->stf->op_txstreams;
	uint32 min_mu_traffc;
	uint32 max_mu_traffc;
	uint32 user_rel_limit;
	uint8  bw;
	uint32 orig_release;
	uint32 total_mu_traffic;
	uint32 release_len;

	if (list->type != TAF_TYPE_DL) {
		return FALSE;
	}

	TAF_ASSERT(tech_idx == TAF_TECH_DL_HEMUMIMO || tech_idx == TAF_TECH_DL_VHMUMIMO);
	TAF_ASSERT(method->ias->mu_mimo_rel_limit);

	time_limit_units = TAF_MICROSEC_TO_UNITS(uni_state->high[IAS_INDEXM(method)]);
	if (time_limit_units <= uni_state->total.released_units) {
		return FALSE;
	}
	time_limit_units -= uni_state->total.released_units;

	bw = taf_bw(scb_taf);

	if (!taf_ias_dl_mimo_suitable(scb_taf, bw, streams_max, tid, tech_idx)) {
		return FALSE;
	}

	/* need to re-evaluate traffic according to MU rate info */
	traffic_estimate[0] =
		taf_ias_check_upd_rate(scb_taf, tech_idx, tid, uni_state, TAF_TYPE_DL);

	WL_TAFM2(method, "this %s item is "MACF"%s having MU %6u units tid %u, "
		"rel score %4u, mu rate %ux%2u(%u), su rate %ux%2u(%u)  (opt %u)\n",
		taf_mutech_text[tech_idx], TAF_ETHERC(scb_taf), TAF_TYPE(TAF_TYPE_DL),
		traffic_estimate[0], tid,
		scb_taf->info.scb_stats.ias.data.relative_score[TAF_TYPE_DL], taf_nss_mu(scb_taf),
		taf_mcs_mu(scb_taf), taf_encode_mu(scb_taf), taf_nss(scb_taf), taf_mcs(scb_taf),
		taf_encode(scb_taf), method->ias->mu_mimo_opt);

	streams = 1;
	items[0] = scb_taf;
	inter_item[0] = 0;
	orig_release = traffic_estimate[0];

	min_mu_traffc = orig_release;
	max_mu_traffc = orig_release;
	total_mu_traffic = orig_release;

	while (list) {
		inter_item[mu_user_count] = 0;

		for (list = list->next; list; list = list->next) {
			scb_taf_next = list->scb_taf;

			if ((list->type == TAF_TYPE_DL) &&
				taf_ias_dl_mimo_suitable(scb_taf_next, bw, streams_max,
					tid, tech_idx)) {
				break;
			}
			if (scb_taf_next->info.tech_type[list->type][tid] != TAF_TECH_DONT_ASSIGN) {
				uint32 est = scb_taf_next->info.traffic.est_units[list->type][tid];

				if (est > 0 && (list->type == TAF_TYPE_DL ||
						!TAF_MIMO_OPT(method, INT_EXTEND))) {
					inter_item[mu_user_count] += est;
					WL_TAFM3(method, "increasing inter traffic %u to %u from "
						MACF"%s tid %u est units %u\n", mu_user_count,
						inter_item[mu_user_count], TAF_ETHERC(scb_taf_next),
						TAF_TYPE(list->type), tid, est);
				}
#if TAF_ENABLE_UL
				else if (est > 0) {
					inter_count += est;
					WL_TAFM3(method, "increasing inter count to %u from "
						MACF"%s tid %u est units %u\n",
						inter_count, TAF_ETHERC(scb_taf_next),
						TAF_TYPE(list->type), tid, est);
				}
#endif // endif

			}
		}

		if (!list || !scb_taf_next) {
			WL_TAFM3(method, "no next %u %s item found\n", mu_user_count + 1,
				TAF_TECH_NAME(tech_idx));
			break;
		}

		if (inter_item[mu_user_count] > 0 && !TAF_MIMO_OPT(method, SWEEP)) {
			WL_TAFM2(method, "intermediate traffic found\n");
			break;
		}
#if TAF_ENABLE_UL
		if (taf_ias_break_mu_pair(method, inter_count, inter_extend, min_mu_traffc)) {
			break;
		}
		inter_extend = inter_count;
#endif // endif
		if (TAF_MIMO_OPT(method, PAIR) ||
			((mu_user_count >= streams_max) && TAF_MIMO_OPT(method, PAIR_MAXMU))) {

			uint32 pro_limit = (scb_taf->score[TAF_TYPE_DL] *
				method->ias->mu_pair[mu_user_count]) >> TAF_COEFF_IAS_MAX_BITS;

			uint32 diff = scb_taf_next->score[TAF_TYPE_DL] -
				scb_taf->score[TAF_TYPE_DL];

			if (diff > pro_limit) {
				WL_TAFM2(method, "score diff %u "MACF"%s is too large "
					"(limit %u/%u)\n", diff,
					TAF_ETHERC(scb_taf_next), TAF_TYPE(list->type),
					pro_limit, method->ias->mu_pair[mu_user_count]);
				break;
			}
		}

		items[mu_user_count] = scb_taf_next;
		est_inter_total += inter_item[mu_user_count];

		traffic_estimate[mu_user_count] =
			taf_ias_check_upd_rate(scb_taf_next, tech_idx, tid, uni_state, TAF_TYPE_DL);

		++streams;

		total_mu_traffic += traffic_estimate[mu_user_count];

		max_mu_traffc = MAX(max_mu_traffc, traffic_estimate[mu_user_count]);
		min_mu_traffc = MIN(min_mu_traffc, traffic_estimate[mu_user_count]);

		WL_TAFM2(method, "next %s item is "MACF"%s having MU %6u units tid %u, "
			"rel_score %4u, mu rate %ux%2u(%u), su rate %ux%2u(%u)  "
			"(itraffic %u)\n",
			TAF_TECH_NAME(tech_idx), TAF_ETHERC(scb_taf_next), TAF_TYPE(list->type),
			traffic_estimate[mu_user_count], tid,
			scb_taf_next->info.scb_stats.ias.data.relative_score[TAF_TYPE_DL],
			taf_nss_mu(scb_taf_next), taf_mcs_mu(scb_taf_next), taf_encode_mu(scb_taf),
			taf_nss(scb_taf_next), taf_mcs(scb_taf_next), taf_encode(scb_taf),
			inter_item[mu_user_count]);

		mu_user_count++;

		if (mu_user_count >= TAF_MAX_MU_MIMO ||
			((mu_user_count >= streams_max) && TAF_MIMO_OPT(method, MAX_STREAMS))) {

			WL_TAFM2(method, "ending %s due to mu_user_count of %u\n",
				TAF_TECH_NAME(tech_idx), mu_user_count);
			break;
		}
	}

	if (mu_user_count == 1) {
		WL_TAFM2(method, "only 1 %s user possible\n", TAF_TECH_NAME(tech_idx));
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
			WL_TAFM2(method, "mu-mimo release %u limiting to %uus (%u)\n",
				mu_user_count, TAF_UNITS_TO_MICROSEC(user_rel_limit),
				user_rel_limit);

			release_len = user_rel_limit;
		}

		if (release_len > time_limit_units) {
			WL_TAFM2(method, "mu-mimo release limiting to remaining window %uus (%u)\n",
				TAF_UNITS_TO_MICROSEC(time_limit_units), time_limit_units);
			release_len = time_limit_units;
		}

		if (method->ias->release_limit > 0 && release_len > method->ias->release_limit) {
			uint32 global_rel_limit = TAF_MICROSEC_TO_UNITS(method->ias->release_limit);

			WL_TAFM2(method, "mu-mimo release limiting to global limit %uus (%u)\n",
				method->ias->release_limit, global_rel_limit);
			release_len = global_rel_limit;
		}

		WL_TAFM1(method, "final %s possible: %u users, %u/%u min/max "
			"traffic, %u possible per user\n", TAF_TECH_NAME(tech_idx), mu_user_count,
			min_mu_traffc, max_mu_traffc, release_len);
	} else {
		WL_TAFM1(method, "intermediate traffic count %u so sweep this %s release\n",
			est_inter_total, TAF_TECH_NAME(tech_idx));
		tech_idx = TAF_TECH_DONT_ASSIGN;
		release_len = 0;
	}

	total_mu_traffic = taf_ias_tech_alloc(method, items, mu_user_count, release_len,
		tech_idx, tid, TAF_TYPE_DL);

	if (total_mu_traffic + inter_extend > time_limit_units) {
		uni_state->extend.window_dur_units += total_mu_traffic + inter_extend -
			time_limit_units;
#if TAF_ENABLE_UL
		if (inter_extend > 0) {
			WL_TAFM2(method, "extend release due to non DL intermediate traffic (%u)\n",
				inter_extend);
		}
#endif // endif
		WL_TAFM2(method, "%s extend units %u\n", TAF_TECH_NAME(tech_idx),
			total_mu_traffic + inter_extend - time_limit_units);
	}

	return TRUE;
}
#endif /* TAF_ENABLE_MU_TX */

static INLINE void BCMFASTPATH taf_ias_analyse_mu(taf_method_info_t* method,
	taf_list_t *list, taf_ias_uni_state_t *uni_state, int tid)
{
#if TAF_ENABLE_MU_TX
	taf_tech_type_t tech_idx;
	taf_scb_cubby_t *scb_taf = list->scb_taf;

	TAF_ASSERT(tid >= 0 && tid < NUMPRIO);

	if (scb_taf->info.ps_mode) {
		return;
	}
	if (scb_taf->info.tech_type[list->type][tid] < TAF_TECH_UNASSIGNED) {
		WL_TAFM4(method, MACF"%s tid %u is already allocated for %s (%d units)\n",
			TAF_ETHERC(scb_taf), TAF_TYPE(list->type), tid,
			TAF_TECH_NAME(scb_taf->info.tech_type[list->type][tid]),
			scb_taf->info.released_units_limit[list->type][tid]);
		return;
	}

	for (tech_idx = 0; scb_taf->info.tech_type[list->type][tid] == TAF_TECH_UNASSIGNED &&
			tech_idx < TAF_NUM_MU_TECH_TYPES;
			tech_idx++) {
		wlc_taf_info_t* taf_info = method->taf_info;

		if (!(taf_info->mu & (1 << tech_idx))) {
			continue;
		}
		if (!(scb_taf->info.mu_tech_en[tech_idx] &
			taf_info->mu_g_enable_mask[tech_idx] & (1 << tid))) {
			continue;
		}
		switch (tech_idx) {
			case TAF_TECH_DL_HEMUMIMO:
			case TAF_TECH_DL_VHMUMIMO:
				taf_ias_dl_mimo(method, tech_idx, list, uni_state, tid);
				continue;
			case TAF_TECH_DL_OFDMA:
				taf_ias_dl_ofdma(method, tech_idx, list, uni_state, tid);
				continue;
#if TAF_ENABLE_UL
			case TAF_TECH_UL_OFDMA:
				taf_ias_ul_ofdma(method, tech_idx, list, uni_state, tid);
				continue;
#endif /* TAF_ENABLE_UL */
			default:
				TAF_ASSERT(0);
		}
		break;
	}
#else
	BCM_REFERENCE(method);
	BCM_REFERENCE(list);
	BCM_REFERENCE(uni_state);
	BCM_REFERENCE(tid);
#endif /* TAF_ENABLE_MU_TX */
}

static INLINE void BCMFASTPATH taf_ias_prepare_list(taf_method_info_t *method)
{
	taf_ias_pre_update_list(method);

	if (method->ordering == TAF_LIST_SCORE_MINIMUM ||
			method->ordering == TAF_LIST_SCORE_MAXIMUM) {
		wlc_taf_sort_list(&method->list, method->ordering, TAF_IAS_NOWTIME(method));
	}
	taf_ias_post_update_list(method);
}

static INLINE void BCMFASTPATH
taf_ias_optimise_agg(taf_scb_cubby_t *scb_taf, int tid, taf_ias_uni_state_t* uni_state,
	taf_schedule_state_t op_state, taf_release_context_t *context)
{
	/* aggregation optimisation */

	/* hold_for_aggs = AMSDU optimise */
	bool hold_for_aggs = (uni_state->trigger == TAF_TRIGGER_IMMEDIATE);
	/* hold_for_aggp = AMPDU optimise */
	bool hold_for_aggp;
	uint32 more_traffic = 0;
	taf_method_info_t* scb_method = scb_taf->method;
	uint32 wme_ac = SCB_WME(scb_taf->scb) ? WME_PRIO2AC(tid) : AC_BE;
	int32 in_flight = SCB_PKTS_INFLT_FIFOCNT_VAL(scb_taf->scb, tid);
#if TAF_ENABLE_MU_TX
	bool mu_agg_hold = TRUE;

	if (scb_taf->info.tech_type[TAF_TYPE_DL][tid] == TAF_TECH_DL_OFDMA &&
		!TAF_OFDMA_OPT(scb_method, AGG_HOLD)) {

		mu_agg_hold = FALSE;

	} else if (scb_taf->info.tech_type[TAF_TYPE_DL][tid] <= TAF_TECH_DL_VHMUMIMO &&
		!TAF_MIMO_OPT(scb_method, AGG_HOLD)) {

		mu_agg_hold = FALSE;
	}
#else
	const bool mu_agg_hold = TRUE;
#endif /* TAF_ENABLE_MU_TX */

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
}

static bool BCMFASTPATH
taf_ias_send_source(taf_method_info_t *method, taf_scb_cubby_t *scb_taf,
	int tid, taf_source_type_t s_idx, taf_ias_uni_state_t* uni_state,
	taf_ias_sched_data_t* ias_sched, taf_release_context_t *context, taf_list_type_t type)
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
	taf_ias_sched_rel_stats_t* rel_stats;
	bool force;
	uint32 pre_qlen = 0;
	uint32 post_qlen = 0;
	uint32 release_n = 0;

	TAF_ASSERT(ias_sched);

#ifdef TAF_DBG
	BCM_REFERENCE(virtual_release);
	BCM_REFERENCE(pending_release);
#endif // endif
	if (!(ias_sched->used & (1 << s_idx))) {
		return TRUE;
	}

	taf_info = method->taf_info;
	time_limit_units = TAF_IAS_EXTEND(uni_state) +
		TAF_MICROSEC_TO_UNITS(uni_state->high[IAS_INDEXM(method)]);
	rel_stats = &ias_sched->rel_stats;
	force = (scb_taf->force & (1 << tid)) ? TRUE : FALSE;
	context->public.ias.was_emptied = FALSE;
	context->public.ias.is_ps_mode = scb_taf->info.ps_mode;
	context->public.ias.traffic_count_available = scb_taf->info.traffic.count[s_idx][tid];

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
		/* margin here is used to ask SQS for extra packets beyond nominal */
		context->public.ias.margin = method->ias->margin;
	}
#endif /* TAF_ENABLE_SQS_PULL */
	if (TAF_SOURCE_IS_REAL(s_idx) && !TAF_SOURCE_IS_UL(s_idx)) {
		/* Record exact level of queue before; this is required because AMSDU
		* in AMPDU is variable according to various configs and situations, and it
		* is necessary to measure this again after release to see what actually went
		* for accurate packet size estimation
		*/
		pre_qlen = taf_info->funcs[s_idx].pktqlen_fn(ias_sched->handle[s_idx].scbh,
			ias_sched->handle[s_idx].tidh);
	}

	context->public.complete = TAF_REL_COMPLETE_TIME_LIMIT;
	context->public.ias.time_limit_units = time_limit_units;

	if (type == TAF_TYPE_DL && !scb_taf->info.ps_mode) {
		if (TAF_SOURCE_IS_AMPDU(s_idx)) {
			/* aggregation optimisation */
			taf_ias_optimise_agg(scb_taf, tid, uni_state, taf_info->op_state, context);
		}
	} else {
		context->public.ias.opt_aggs = 0;
		context->public.ias.opt_aggp = FALSE;
	}

	/* for accounting, real or virtual both get counted here */
	while ((uni_state->total.released_units + released_units) < time_limit_units) {

		bool result;

		context->public.ias.total.released_units = uni_state->total.released_units +
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
		if (TAF_SOURCE_IS_REAL(s_idx) && !TAF_SOURCE_IS_UL(s_idx) && (pre_qlen > 0)) {
			post_qlen = taf_info->funcs[s_idx].pktqlen_fn(ias_sched->handle[s_idx].scbh,
				ias_sched->handle[s_idx].tidh);

			release_n = pre_qlen - post_qlen;
		}

#if TAF_ENABLE_SQS_PULL && !TAF_LOGL4
		if (method->type != TAF_VIRTUAL_MARKUP)
#endif // endif
		{
			WL_TAFM1(method, MACF"%s tid %u %s: tot_units %6u, rel %5u, ext %3u, "
				"%3u real (%3un) / %3u virt / %2u pend, ridx %u, %u:%s%s\n",
				TAF_ETHERC(scb_taf), TAF_TYPE(type), tid,
				TAF_SOURCE_NAME(s_idx), uni_state->total.released_units,
				released_units, extend_units,
				actual_release, release_n, virtual_release, pending_release,
#if TAF_ENABLE_MU_TX
				scb_taf->info.scb_stats.ias.data.ridx_used[type],
#else
				0,
#endif // endif
				context->public.complete,
#ifdef TAF_DBG
				taf_rel_complete_text[context->public.complete],
#else
				"",
#endif // endif
				context->public.ias.was_emptied ? " E":"");
		}
	} else {
#if TAF_ENABLE_SQS_PULL && !TAF_LOGL3
		if (method->type != TAF_VIRTUAL_MARKUP ||
			context->public.complete >= TAF_REL_COMPLETE_ERR)
#endif // endif
		{
			WL_TAFM1(method, MACF"%s %s: no release (%u/%u) reason %u:%s\n",
			TAF_ETHERC(scb_taf), TAF_TYPE(type), TAF_SOURCE_NAME(s_idx),
			uni_state->total.released_units, time_limit_units,
			context->public.complete,
#ifdef TAF_DBG
			taf_rel_complete_text[context->public.complete]);
#else
			"");
#endif // endif
		}
	}

	/* the following is statistics across the TID state which is global across all SCB using
	 * that TID
	 */
	uni_state->total.released_units += released_units;
	uni_state->extend.rounding_units += extend_units;

#if TAF_ENABLE_SQS_PULL
	if (virtual_release) {
		TAF_ASSERT(TAF_SOURCE_IS_VIRTUAL(s_idx));
		context->virtual_release += virtual_release;
		scb_taf->info.pkt_pull_request[tid] = virtual_release;
		scb_taf->info.pkt_pull_map |= (1 << tid);
		taf_info->total_pull_requests++;

		WL_TAFT3(taf_info, "total_pull_requests %u\n", taf_info->total_pull_requests);
	}
	context->pending_release += pending_release;
#endif /* TAF_ENABLE_SQS_PULL */

	if (actual_release) {
#if TAF_ENABLE_SQS_PULL
		TAF_ASSERT(TAF_SOURCE_IS_REAL(s_idx));
#endif /* TAF_ENABLE_SQS_PULL */

		uni_state->real.released_units += released_units;
		uni_state->total_units_in_transit += released_units;

		/* the following statistics are per SCB,TID link */
		rel_stats->data.release_pcount += actual_release;
		rel_stats->data.release_units += released_units;
		if (type == TAF_TYPE_DL) {
			rel_stats->data.release_bytes += released_bytes;
			rel_stats->data.release_ncount += release_n;
		}

		/* this is global across the whole scheduling interval */
		context->actual_release += actual_release;
		context->actual_release_n += release_n;

	} else if (TAF_SOURCE_IS_AMPDU(s_idx) &&
		context->public.complete == TAF_REL_COMPLETE_NOTHING_AGG &&
		taf_info->op_state != TAF_SCHEDULE_VIRTUAL_PACKETS) {

		TAF_DPSTATS_LOG_ADD(rel_stats, held_z, 1);
	}
	switch (context->public.complete) {
		case TAF_REL_COMPLETE_RESTRICTED:
			TAF_DPSTATS_LOG_ADD(rel_stats, restricted, 1);
			/* fall through */
		case TAF_REL_COMPLETE_FULL:
		case TAF_REL_COMPLETE_BLOCKED:
		case TAF_REL_COMPLETE_NO_BUF:
		case TAF_REL_COMPLETE_ERR:
			TAF_DPSTATS_LOG_ADD(rel_stats, error, 1);
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

static void taf_ias_decay_stats(taf_ias_sched_rel_stats_t* rel_stats, taf_ias_coeff_t* decay,
	uint32 now_time)
{
	uint32 elapsed = now_time - rel_stats->data.calc_timestamp;
	int32 time_correction = 0;
	uint32 coeff = taf_ias_decay_score(TAF_COEFF_IAS_MAX, decay, elapsed, &time_correction);

	if (coeff < TAF_COEFF_IAS_MAX) {
		uint64 calc;
		rel_stats->data.calc_timestamp = now_time - time_correction;

		/* may overflow 32 bit int, use 64 bit */
		calc = ((uint64)rel_stats->data.total_scaled_ncount * (uint64)coeff);
		calc += (TAF_COEFF_IAS_MAX >> 1);
		calc >>= TAF_COEFF_IAS_MAX_BITS;
		rel_stats->data.total_scaled_ncount = calc;

		/* may overflow 32 bit int, use 64 bit */
		calc = ((uint64)rel_stats->data.total_scaled_pcount * (uint64)coeff);
		calc += (TAF_COEFF_IAS_MAX >> 1);
		calc >>= TAF_COEFF_IAS_MAX_BITS;
		rel_stats->data.total_scaled_pcount = calc;

		/* likely to overflow 32 bit int, use 64 bit */
		calc = ((uint64)rel_stats->data.total_bytes * (uint64)coeff);
		calc >>= TAF_COEFF_IAS_MAX_BITS;
		rel_stats->data.total_bytes = calc;
	}
}

static INLINE void BCMFASTPATH
taf_ias_mean_pktlen(taf_scb_cubby_t *scb_taf, taf_ias_sched_rel_stats_t* rel_stats,
	uint32 timestamp, taf_ias_coeff_t* decay)
{
	/*
	* This calculates the average packet size from what has been released. It is going to be
	* used under SQS, to forward predict how many virtual packets to be converted.
	*/
	uint32 mean;
	uint32 ncount = rel_stats->data.release_ncount;
	uint32 pcount = rel_stats->data.release_pcount;
	uint32 release_bytes = rel_stats->data.release_bytes;

	if (ncount == 0) {
		return;
	}

	taf_ias_decay_stats(rel_stats, decay, timestamp);

	/*
	* packet counts are scaled here, as at low data rates, the discrete low packet count
	* loses precision when doing exponential decay; the scaling gives a
	* notional fractional pkt count to help the accuracy
	*/
	rel_stats->data.total_scaled_ncount += (ncount << TAF_PKTCOUNT_SCALE_SHIFT);
	rel_stats->data.total_scaled_pcount += (pcount << TAF_PKTCOUNT_SCALE_SHIFT);
	rel_stats->data.total_bytes += release_bytes;

	/*
	* XXX avoid 64 bit divide, use standard 32 bit, so TAF_PKTCOUNT_SCALE_SHIFT
	* can't be too big
	*/

	TAF_ASSERT(rel_stats->data.total_bytes < (1 << (32 - TAF_PKTCOUNT_SCALE_SHIFT)));

	mean = rel_stats->data.total_bytes << TAF_PKTCOUNT_SCALE_SHIFT;

	TAF_ASSERT(rel_stats->data.total_scaled_ncount);

	mean += (rel_stats->data.total_scaled_ncount >> 1);
	mean /= (rel_stats->data.total_scaled_ncount);

	rel_stats->data.release_pkt_size_mean = mean;
	TAF_DPSTATS_LOG_SET(rel_stats, release_pkt_size_mean, mean);
}

static INLINE void BCMFASTPATH
taf_ias_sched_update(taf_method_info_t *method, taf_scb_cubby_t *scb_taf,
		int tid, taf_ias_uni_state_t *uni_state, taf_list_type_t type)

{
	taf_ias_sched_data_t* ias_sched = TAF_IAS_TID_STATE(scb_taf, tid);
	taf_ias_sched_rel_stats_t* rel_stats = &ias_sched->rel_stats;
	uint32 new_score;

	/* clear the force status */
	scb_taf->force &= ~(1 << tid);

#if TAF_ENABLE_SQS_PULL
	if ((type == TAF_TYPE_DL) && scb_taf->info.pkt_pull_map == 0) {
		/* reset release limit tracking after pull cycle has completed */
		ias_sched->released_units_limit[TAF_TYPE_DL] = 0;
	}
#endif /* TAF_ENABLE_SQS_PULL */

	if (!rel_stats->data.release_pcount) {
		return;
	}

	if (method->taf_info->scheduler_index != rel_stats->index[type]) {
		TAF_DPSTATS_LOG_ADD(rel_stats, release_frcount, 1);
		rel_stats->index[type] = method->taf_info->scheduler_index;
		/* record the release time */
		scb_taf->timestamp[type] = TAF_IAS_NOWTIME(method);
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
			new_score = 0;
			break;

		case TAF_PSEUDO_RR:
			/* this is packet number scoring (or could use released_bytes instead) */
			new_score = taf_ias_units_to_score(method, scb_taf, tid,
				scb_taf->info.tech_type[type][tid],
				rel_stats->data.release_pcount << 6);
			break;

		case TAF_ATOS:
		case TAF_ATOS2:
			/* this is scheduled airtime scoring */
			new_score = taf_ias_units_to_score(method, scb_taf, tid,
				scb_taf->info.tech_type[type][tid],
				rel_stats->data.release_units);
			break;

		default:
			new_score = 0;
			TAF_ASSERT(0);
	}

	scb_taf->score[type] = MIN(scb_taf->score[type] + new_score, TAF_SCORE_IAS_MAX);

	if (type == TAF_TYPE_DL) {
		taf_ias_mean_pktlen(scb_taf, rel_stats, scb_taf->timestamp[TAF_TYPE_DL],
			&method->ias->coeff);
	}

	TAF_DPSTATS_LOG_ADD(rel_stats, release_pcount, rel_stats->data.release_pcount);
	TAF_DPSTATS_LOG_ADD(rel_stats, release_ncount, rel_stats->data.release_ncount);
	TAF_DPSTATS_LOG_ADD(rel_stats, release_time,
		(uint64)TAF_UNITS_TO_MICROSEC(rel_stats->data.release_units));
	TAF_DPSTATS_LOG_ADD(rel_stats, release_bytes, (uint64)rel_stats->data.release_bytes);

	WL_TAFM2(method, MACF"%s tid %u rate %7u, rel %3u pkts (%3u npkts) %4u us, "
		"total %5u us\n",
		TAF_ETHERC(scb_taf), TAF_TYPE(type), tid,
#if TAF_ENABLE_MU_TX
		RSPEC2KBPS(scb_taf->info.scb_stats.global.rdata.rspec
		[scb_taf->info.scb_stats.ias.data.ridx_used[type]]),
#else
		RSPEC2KBPS(scb_taf->info.scb_stats.global.rdata.rspec[TAF_RSPEC_SU_DL]),
#endif // endif
		rel_stats->data.release_pcount, rel_stats->data.release_ncount,
		TAF_UNITS_TO_MICROSEC(rel_stats->data.release_units),
		TAF_UNITS_TO_MICROSEC(uni_state->total.released_units));

	/* reset before next scheduler invocation */
	rel_stats->data.release_pcount = 0;
	rel_stats->data.release_bytes = 0;
	rel_stats->data.release_units = 0;
	rel_stats->data.release_ncount = 0;
}

static INLINE void taf_ias_sched_rate(taf_scb_cubby_t *scb_taf, int tid,
	taf_rspec_index_t rindex, taf_source_type_t s_idx, taf_ias_sched_data_t* ias_sched,
	taf_release_context_t *context)
{
	taf_scheduler_scb_stats_t * scb_stats = &scb_taf->info.scb_stats;

#if TAF_ENABLE_NAR
	if (taf_ias_is_nar_traffic(scb_taf, tid, s_idx)) {
		context->public.ias.pkt_rate = scb_stats->ias.data.nar_pkt_rate;
		context->public.ias.byte_rate = scb_stats->ias.data.nar_byte_rate;
	} else
#endif /* TAF_ENABLE_NAR */
#if TAF_ENABLE_UL
	if (TAF_SOURCE_IS_UL(s_idx)) {
		context->public.ias.byte_rate = scb_stats->ias.data.byte_rate[rindex];
		context->public.ias.pkt_rate = 0;
	} else
#endif /* TAF_ENABLE_UL */
	{
		uint32 pkt_rate = scb_stats->ias.data.pkt_rate[rindex];

		if (TAF_OPT(scb_taf->method, PKT_AMPDU_OVERHEAD) && scb_taf->info.max_pdu >= 1) {
			pkt_rate += (scb_stats->ias.data.overhead_without_rts <<
				TAF_PKTBYTES_COEFF_BITS) / scb_taf->info.max_pdu;
		}
		context->public.ias.byte_rate = scb_stats->ias.data.byte_rate[rindex];
		context->public.ias.pkt_rate = pkt_rate;
	}
#if TAF_ENABLE_SQS_PULL
	context->public.ias.aggsf = ias_sched->aggsf;
#endif // endif

	WL_TAFM3(scb_taf->method, MACF" %s aggsf %u, byte_rate %u, "
		"pkt_rate %u\n", TAF_ETHERC(scb_taf), TAF_SOURCE_NAME(s_idx),
		context->public.ias.aggsf, context->public.ias.byte_rate,
		context->public.ias.pkt_rate);
}

static INLINE void taf_ias_sched_clear(taf_release_context_t *context)
{
	context->public.how = TAF_RELEASE_LIKE_IAS;

#if TAF_ENABLE_SQS_PULL
	context->public.ias.virtual.released_units = 0;
	context->public.ias.virtual.release = 0;
#endif /* TAF_ENABLE_SQS_PULL */
	context->public.ias.actual.released_units = 0;
	context->public.ias.actual.released_bytes = 0;
	context->public.ias.actual.release = 0;

	context->public.ias.timestamp = context->now_time;
}

static INLINE bool BCMFASTPATH
taf_ias_tracking_end(taf_method_info_t *method, taf_scb_cubby_t *scb_taf, taf_list_type_t type,
	taf_ias_sched_data_t* ias_sched)
{
	bool limited = FALSE;
#ifdef TAF_LOGL2
	char * msg = "";

	BCM_REFERENCE(msg);
#endif // endif

	if (!limited && ias_sched->released_units_limit[type] < 0) {
#ifdef TAF_LOGL2
		msg = "release";
#endif // endif
		limited = TRUE;
	}

	if (limited) {
		WL_TAFM2(method, MACF"%s %s limit reached\n",
			TAF_ETHERC(scb_taf), TAF_TYPE(type), msg);
	}

	return limited;
}

static INLINE void BCMFASTPATH
taf_ias_init_tracking(taf_method_info_t *method, taf_scb_cubby_t *scb_taf,
	taf_ias_sched_data_t* ias_sched, taf_schedule_state_t op_state, bool force,
	taf_list_type_t type, taf_release_context_t *context)
{
	wlc_taf_info_t* taf_info = method->taf_info;
#if TAF_ENABLE_SQS_PULL
	const bool init_tracking = (op_state == TAF_SCHEDULE_VIRTUAL_PACKETS);
#else
	const bool init_tracking = TRUE;
#endif // endif

	/* init the scb tracking for release limiting */
	if (init_tracking) {
		if (ias_sched->released_units_limit[type] == 0) {

			if (force && type == TAF_TYPE_DL) {
				ias_sched->released_units_limit[type] =
					TAF_MICROSEC_TO_UNITS(taf_info->force_time);
			} else {
				ias_sched->released_units_limit[type] =
					TAF_MICROSEC_TO_UNITS(method->ias->release_limit);
			}
		}
	}
	context->public.ias.released_units_limit = ias_sched->released_units_limit[type];

	WL_TAFM3(method, MACF"%s tracking init %u\n", TAF_ETHERC(scb_taf), TAF_TYPE(type),
		context->public.ias.released_units_limit);
}

static INLINE void
taf_ias_upd_tracking(taf_method_info_t *method, taf_scb_cubby_t *scb_taf,
	taf_ias_sched_data_t* ias_sched, taf_ias_sched_rel_stats_t* rel_stats, taf_list_type_t type)
{
	if (ias_sched->released_units_limit[type] > 0) {
		if (ias_sched->released_units_limit[type] > rel_stats->data.release_units) {
			ias_sched->released_units_limit[type] -=
				rel_stats->data.release_units;
		} else {
			/* minus -1 to flag release limit already reached */
			ias_sched->released_units_limit[type] = -1;
		}
	}
}

static INLINE bool BCMFASTPATH
taf_ias_sched_send(taf_method_info_t *method, taf_scb_cubby_t *scb_taf, int tid,
	taf_ias_uni_state_t *uni_state, taf_ias_sched_data_t* ias_sched,
	taf_release_context_t *context, taf_schedule_state_t op_state, taf_list_type_t type)
{
	wlc_taf_info_t* taf_info = method->taf_info;
#if TAF_ENABLE_SQS_PULL
	const taf_source_type_t start_source = TAF_FIRST_REAL_SOURCE;
	const taf_source_type_t end_source = (op_state == TAF_SCHEDULE_VIRTUAL_PACKETS) ?
		TAF_NUM_SCHED_SOURCES : TAF_NUM_REAL_SOURCES;
	uint16 v_avail = scb_taf->info.traffic.count[TAF_SOURCE_HOST_SQS][tid];
	uint32 v_prior_released = 0;
	uint32 real_prior_released = 0;
#else /* TAF_ENABLE_SQS_PULL */
	const taf_source_type_t start_source = TAF_FIRST_REAL_SOURCE;
	const taf_source_type_t end_source = TAF_NUM_REAL_SOURCES;
#endif /* TAF_ENABLE_SQS_PULL */
#if TAF_ENABLE_TIMER
	const bool agg_hold = taf_ias_agghold_is_active(TAF_IAS_GROUP_INFO(taf_info));
#else
	const bool agg_hold = FALSE;
#endif /* TAF_ENABLE_TIMER */
	const bool force = (scb_taf->force & (1 << tid)) ? (taf_info->force_time > 0) : FALSE;
	taf_source_type_t s_idx;
	taf_rspec_index_t rindex;
	taf_ias_sched_rel_stats_t* rel_stats = &ias_sched->rel_stats;

	if (taf_ias_tracking_end(method, scb_taf,  type, ias_sched)) {
		/* have to return TRUE here (as if emptied) so IAS continues with next station */
		return TRUE;
	}
	taf_ias_sched_clear(context);

	rindex = taf_ias_get_rindex(scb_taf, type, scb_taf->info.tech_type[type][tid]);

	context->public.type = taf_tech_to_mutype(scb_taf->info.tech_type[type][tid]);
	uni_state->release_type_present |= (1 << scb_taf->info.tech_type[type][tid]);
	uni_state->bw_type_present |= (1 << scb_taf->info.scb_stats.global.rdata.bw_idx);

	scb_taf->info.scb_stats.ias.data.ridx_used[type] = rindex;

	context->public.ias.estimated_pkt_size_mean = rel_stats->data.release_pkt_size_mean ?
			rel_stats->data.release_pkt_size_mean :
			(type == TAF_TYPE_DL) ? TAF_PKT_SIZE_DEFAULT_DL : TAF_PKT_SIZE_DEFAULT_UL;

	if (!agg_hold) {
		taf_ias_init_tracking(method, scb_taf, ias_sched, op_state, force, type, context);
	}

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
			const bool is_mu = scb_taf->info.tech_type[type][tid] <
				TAF_NUM_MU_TECH_TYPES;

			if ((pending = scb_taf->info.traffic.count[s_idx][tid]) == 0) {
				continue;
			}

			/* multi-stage if to make it less complex to handle real packets
			 * already available during virtual phase
			 */
			if (TAF_SOURCE_IS_SQS(s_idx)) {
				v_prior_released = context->virtual_release;
				/* do release */
			} else if (agg_hold) {
				/* For IAS purpose, consider this is emptied */
				scb_taf->info.traffic.map[s_idx] &= ~(1 << tid);
				continue;
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
				WL_TAFM3(method, "skip real vphase rel to "MACF" tid %u %s "
					"only %u pending (thresh %u), v_avail %u, ps %u, mu %u\n",
					TAF_ETHERC(scb_taf), tid, TAF_SOURCE_NAME(s_idx),
					pending, MIN(method->ias->pre_rel_limit[s_idx],
					scb_taf->info.max_pdu), v_avail,
					scb_taf->info.ps_mode, is_mu);

				/* For IAS purpose, consider this is emptied */
				scb_taf->info.traffic.map[s_idx] &= ~(1 << tid);
				continue;
			}

			if (TAF_SOURCE_IS_REAL(s_idx)) {
				if (real_prior_released == 0) {
					real_prior_released = context->actual_release;
				}
				WL_TAFM2(method, "try real vphase rel to "MACF"%s tid %u %s "
					"%u pending, v_avail %u, ps %u "
					"force %u tx-in-transit %u, mu %u\n",
					TAF_ETHERC(scb_taf), TAF_TYPE(type), tid,
					TAF_SOURCE_NAME(s_idx), pending, v_avail,
					scb_taf->info.ps_mode, force,
					SCB_PKTS_INFLT_FIFOCNT_VAL(scb_taf->scb, tid), is_mu);
			}
		}
#else
		if ((pending = scb_taf->info.traffic.count[s_idx][tid]) == 0) {
			WL_TAFM2(method, MACF"%s tid %u %s pending 0\n",
				TAF_ETHERC(scb_taf), TAF_TYPE(type), tid, TAF_SOURCE_NAME(s_idx));
			continue;
		}
#endif /* TAF_ENABLE_SQS_PULL */

		taf_ias_sched_rate(scb_taf, tid, rindex, s_idx, ias_sched, context);

		emptied = taf_ias_send_source(method, scb_taf, tid, s_idx, uni_state, ias_sched,
			context, type);

#if TAF_ENABLE_SQS_PULL
		/* handle the case that no virtual packets can be pulled and we skipped existing
		 * AMPDU packets available; if so, go back to release the AMPDU packets
		 */
		if (TAF_SOURCE_IS_SQS(s_idx) && (v_avail > 0) &&
			(context->virtual_release - v_prior_released == 0) &&
			(context->actual_release - real_prior_released == 0) &&
			(scb_taf->info.traffic.count[TAF_SOURCE_AMPDU][tid] != 0) &&
			!scb_taf->info.ps_mode && !agg_hold &&
			(SCB_PKTS_INFLT_FIFOCNT_VAL(scb_taf->scb, tid) == 0)) {

				/* no virtual traffic released & no real traffic was pre-released */
				WL_TAFM1(method, MACF"%s tid %u pre-release not completed and "
					"without virtual rel (virtual avail %u, ampdu avail %u); "
					"send ampdu once again....\n",
					TAF_ETHERC(scb_taf), TAF_TYPE(type), tid, v_avail,
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

		if (TAF_SOURCE_IS_REAL(s_idx)) {
			taf_ias_upd_tracking(method, scb_taf, ias_sched, rel_stats, type);

			if (taf_ias_tracking_end(method, scb_taf, type, ias_sched)) {
				/* release tracking completed */
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
taf_ias_sched_scb(taf_method_info_t *method, taf_ias_uni_state_t *uni_state,
	int tid, taf_release_context_t *context, taf_schedule_state_t op_state)
{
	uint32 highunits = TAF_MICROSEC_TO_UNITS(uni_state->high[IAS_INDEXM(method)]);
	uint32 lowunits = TAF_MICROSEC_TO_UNITS(uni_state->low[IAS_INDEXM(method)]);
	taf_list_t *list;
#if TAF_ENABLE_SQS_PULL && TAF_ENABLE_TIMER
	const bool agg_hold = taf_ias_agghold_is_active(TAF_IAS_GROUP_INFO(method->taf_info));
#else
	const bool agg_hold = FALSE;
#endif // endif

	WL_TAFM2(method, "total est units %u (total in transit %u)\n",
		uni_state->est_release_units, uni_state->total_units_in_transit);

	for (list = method->list; list; list = list->next) {
		taf_scb_cubby_t *scb_taf = list->scb_taf;
		taf_ias_sched_data_t* ias_sched;
		bool emptied;

		if (SCB_MARKED_DEL(scb_taf->scb) || SCB_DEL_IN_PROGRESS(scb_taf->scb)) {
			WL_TAFM4(method, MACF"%s tid %u, deletion\n",
				TAF_ETHERC(scb_taf), TAF_TYPE(list->type), tid);
			continue;
		}

		if (!(scb_taf->info.traffic.available[list->type] & (1 << tid))) {
			scb_taf->force &= ~(1 << tid);

			WL_TAFM4(method, MACF"%s tid %u, no traffic\n",
				TAF_ETHERC(scb_taf), TAF_TYPE(list->type), tid);
			continue;
		}

		if (!(ias_sched = TAF_IAS_TID_STATE(scb_taf, tid))) {
			scb_taf->force &= ~(1 << tid);

			WL_TAFM4(method, MACF"%s tid %u, no context\n",
				TAF_ETHERC(scb_taf), TAF_TYPE(list->type), tid);
			continue;
		}

		if (scb_taf->info.ps_mode) {
			TAF_DPSTATS_LOG_ADD(&ias_sched->rel_stats, pwr_save, 1);

			WL_TAFM4(method, MACF"%s tid %u, power save\n",
				TAF_ETHERC(scb_taf), TAF_TYPE(list->type), tid);

			/* cannot force so clear the force status */
			scb_taf->force &= ~(1 << tid);
		}
		if (!agg_hold && TAF_OPT(method, EXIT_EARLY) &&
#if TAF_ENABLE_MU_TX
			scb_taf->info.tech_type[list->type][tid] == TAF_TECH_UNASSIGNED &&
#endif // endif
			uni_state->total.released_units > (lowunits + TAF_IAS_EXTEND(uni_state))) {

			uint32 traffic_next = scb_taf->info.traffic.est_units[list->type][tid];

			if (traffic_next > (highunits + TAF_IAS_EXTEND(uni_state) -
				uni_state->total.released_units)) {

				WL_TAFM1(method, MACF"%s tid %u has %u traffic, space remaining %u,"
					" exit cycle now\n", TAF_ETHERC(scb_taf),
					TAF_TYPE(list->type), tid, traffic_next,
					(highunits + TAF_IAS_EXTEND(uni_state) -
					uni_state->total.released_units));
				return TRUE;
			}
		}

#if TAF_ENABLE_MU_TX
		if (!agg_hold && (method->type == TAF_ATOS || method->type == TAF_ATOS2)) {
			taf_ias_analyse_mu(method, list, uni_state, tid);

			if (scb_taf->info.tech_type[list->type][tid] == TAF_TECH_DONT_ASSIGN) {
				WL_TAFM1(method,
					"skipping "MACF"%s tid %u due to MU optimisation\n",
					TAF_ETHERC(scb_taf), TAF_TYPE(list->type), tid);
				continue;
			}
		}
#endif /* TAF_ENABLE_MU_TX */
		if (scb_taf->info.tech_type[list->type][tid] == TAF_TECH_UNASSIGNED) {
			if (list->type == TAF_TYPE_DL) {
				WL_TAFM3(method, MACF"%s tid %u to be released like SU\n",
					TAF_ETHERC(scb_taf), TAF_TYPE(TAF_TYPE_DL), tid);
				scb_taf->info.tech_type[TAF_TYPE_DL][tid] = TAF_TECH_DL_SU;
			}
#if TAF_ENABLE_UL
			if (list->type == TAF_TYPE_UL) {
				WL_TAFM3(method, MACF"%s tid %u to be released like 1-MU\n",
					TAF_ETHERC(scb_taf), TAF_TYPE(TAF_TYPE_UL), tid);
				scb_taf->info.tech_type[TAF_TYPE_UL][tid] = TAF_TECH_UL_OFDMA;
			}
#endif // endif
		}

		emptied = taf_ias_sched_send(method, scb_taf, tid, uni_state, ias_sched,
			context, op_state, list->type);

		if (!agg_hold) {
			taf_ias_sched_update(method, scb_taf, tid, uni_state, list->type);
		}

		if (!emptied) {
			WL_TAFM3(method, MACF"%s tid %u not emptied\n",
				TAF_ETHERC(scb_taf), TAF_TYPE(list->type), tid);
			return TRUE;
		}
		if (uni_state->total.released_units >= (highunits + TAF_IAS_EXTEND(uni_state))) {
			WL_TAFM3(method, "release window completed (%u, %u)\n",
				uni_state->total.released_units,
				(highunits + TAF_IAS_EXTEND(uni_state)));
			return TRUE;
		}
	}
	return FALSE;
}

static bool BCMFASTPATH
taf_ias_sched_all_scb(taf_method_info_t *method, taf_ias_uni_state_t *uni_state,
	int tid, taf_release_context_t *context)
{
	taf_schedule_state_t op_state = context->op_state;
	wlc_taf_info_t* taf_info = method->taf_info;

	if (taf_info->bulk_commit) {
		taf_open_all_sources(taf_info, tid);
	}

	return taf_ias_sched_scb(method, uni_state, tid, context, op_state);
}

static void taf_ias_time_settings_sync(taf_method_info_t* method)
{
	wlc_taf_info_t* taf_info = method->taf_info;
	taf_ias_uni_state_t* uni_state = taf_get_uni_state(taf_info);

	WL_TAFM1(method, "high = %us, low = %uus\n", method->ias->high, method->ias->low);

	uni_state->high[IAS_INDEXM(method)] = method->ias->high;
	uni_state->low[IAS_INDEXM(method)] = method->ias->low;
}

static INLINE bool BCMFASTPATH
taf_ias_schedule_all_tid(taf_method_info_t* method, taf_ias_uni_state_t *uni_state,
	int tid_index_start, int tid_index_end, taf_release_context_t* context)
{
	bool finished = FALSE;
	int tid_index = tid_index_start;

	do {
		int tid = tid_service_order[tid_index];

		if (method->ias->sched_active & (1 << tid)) {
			finished = taf_ias_sched_all_scb(method, uni_state, tid, context);
		}
		if (!finished) {
			tid_index++;
		}
	} while (!finished && tid_index <= tid_index_end);

	return finished;
}

static INLINE BCMFASTPATH bool
taf_ias_super_resched(wlc_taf_info_t* taf_info, taf_ias_uni_state_t *uni_state,
	uint32 low_units, uint32 high_units, taf_release_context_t *context)
{
	bool extra_cycle = FALSE;
#if TAF_ENABLE_MU_TX
	bool super = (taf_info->super & uni_state->release_type_present) != 0;
#else
	bool super = taf_info->super;
#endif // endif

	if (!super &&
		((uni_state->bw_type_present &
		((1 << D11_REV128_BW_160MHZ) | (1 << D11_REV128_BW_80MHZ))) == 0)) {

		taf_ias_group_info_t* ias_info = TAF_IAS_GROUP_INFO(taf_info);

		if ((uni_state->bw_type_present & (1 << D11_REV128_BW_40MHZ)) &&
			TAF_IAS_OPT(ias_info, AUTOBW40_SUPER)) {

			super = TRUE;
		} else if ((uni_state->bw_type_present & (1 << D11_REV128_BW_20MHZ)) &&
			TAF_IAS_OPT(ias_info, AUTOBW20_SUPER)) {

			super = TRUE;
		}
	}

	if (taf_info->super_active) {
		if (super) {
			uni_state->super_sched_last = context->now_time;

		} else if (uni_state->cycle_next == 0 && uni_state->cycle_now == 0) {
			taf_info->super_active = FALSE;
			uni_state->super_sched_last = 0;

		} else if (uni_state->super_sched_last != 0) {
			uint32 elapsed = context->now_time - uni_state->super_sched_last;
			uint32 limit = TAF_UNITS_TO_MICROSEC(high_units) * TAF_MAX_NUM_WINDOWS;

			if (elapsed > limit) {
				taf_info->super_active = FALSE;
				uni_state->super_sched_last = 0;
				WL_TAFT2(taf_info, "super scheduling to end due to %uus elapsed "
					"with ineligible traffic\n", elapsed);
			}
		} else {
			taf_info->super_active = FALSE;
		}
	}
	if (super && uni_state->cycle_next == 0 && uni_state->cycle_now == 0 &&
		uni_state->real.released_units >= low_units && uni_state->barrier_req == 0 &&
		context->status == TAF_CYCLE_INCOMPLETE) {

		uint32 est_traffic = uni_state->est_release_units;

		if (est_traffic > uni_state->real.released_units) {
			est_traffic -= uni_state->real.released_units;
		} else {
			est_traffic = 0;
		}

		if (est_traffic > high_units) {
#if TAF_ENABLE_MU_TX
			WL_TAFT2(taf_info, "do super reschedule (0x%x, 0x%x, %u)\n",
				taf_info->super, uni_state->release_type_present, est_traffic);
#else
			WL_TAFT2(taf_info, "do super reschedule (%u)\n", est_traffic);
#endif // endif
			/* insert an extra cycle */
			extra_cycle = TRUE;
			taf_info->super_active = TRUE;
			uni_state->super_sched_last = context->now_time;
			uni_state->cycle_ready = 0;
		}
	}
	return extra_cycle;
}

static INLINE BCMFASTPATH bool
taf_ias_completed(taf_method_info_t *method, taf_release_context_t *context,
	taf_ias_uni_state_t *uni_state, bool finished, taf_schedule_state_t op_state)
{
	wlc_taf_info_t* taf_info = method->taf_info;
	taf_ias_group_info_t* ias_info = TAF_IAS_GROUP_INFO(taf_info);
	uint32 duration;
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
			if (v2r_pending) {
				taf_method_info_t *vmarkup = taf_get_method_info(method->taf_info,
					TAF_VIRTUAL_MARKUP);
				*vmarkup->ready_to_schedule = ~0;

				WL_TAFT1(taf_info, "vsched phase exit %uus real, %uus "
					"virtual (%u rpkts / %u vpkts / %u ppkts), "
					"TAF_VIRTUAL_MARKUP pulls %u\n",
					TAF_UNITS_TO_MICROSEC(uni_state->real.released_units),
					TAF_UNITS_TO_MICROSEC(uni_state->total.released_units -
						uni_state->real.released_units),
					context->actual_release, context->virtual_release,
					context->pending_release,
					method->taf_info->total_pull_requests);
			}
			uni_state->total.released_units = uni_state->real.released_units;
			uni_state->extend.rounding_units = 0;
		}
		if (v2r_pending) {
			return finished;
		}
	} else if (method->type == TAF_VIRTUAL_MARKUP && finished) {
		TAF_ASSERT(!v2r_pending);
		*method->ready_to_schedule = 0;
	}
#endif /* TAF_ENABLE_SQS_PULL */

#if TAF_ENABLE_TIMER
	if (finished) {
		if (taf_ias_agghold_is_active(ias_info) && taf_ias_agghold_exp(ias_info)) {

			TAF_ASSERT(uni_state->real.released_units == 0);

			taf_ias_agghold_stop(method->taf_info, ias_info, TAF_IAS_NOWTIME(method));
#if TAF_ENABLE_SQS_PULL
			uni_state->total.released_units = 0;
			uni_state->extend.window_dur_units = 0;
			uni_state->extend.rounding_units = 0;
			finished = FALSE;
			context->op_state = taf_info->op_state;

			WL_TAFM1(method, "scheduling following aggregation timeout\n");
#endif /* TAF_ENABLE_SQS_PULL */
		} else {
			taf_ias_agghold_clear(ias_info);
		}
	}
#endif /* TAF_ENABLE_TIMER */

	if (finished) {
		duration = ias_info->sched_start != 0 ?
			(TAF_IAS_NOWTIME(method) - ias_info->sched_start) : 0;

		ias_info->debug.sched_duration += duration;

		if (duration > ias_info->debug.max_duration) {
			ias_info->debug.max_duration = duration;
		}
		ias_info->sched_start = 0;

		if (taf_info->bulk_commit) {
			taf_close_all_sources(taf_info, ALLPRIO);
		}
	}

	if (finished && (uni_state->real.released_units > 0)) {
		uint32 low_units = TAF_MICROSEC_TO_UNITS(uni_state->low[IAS_INDEXM(method)]);
		uint32 high_units = TAF_MICROSEC_TO_UNITS(uni_state->high[IAS_INDEXM(method)]);
		uint8 cnext = uni_state->cycle_next;
		uint8 cnow = uni_state->cycle_now;
#if !TAF_ENABLE_SQS_PULL
		taf_method_info_t *scheduler = method;
#else
		taf_method_info_t *scheduler = taf_get_method_info(method->taf_info,
			TAF_SCHEDULER_START);
#endif /* !TAF_ENABLE_SQS_PULL */

		if (TAF_IAS_EXTEND(uni_state) > 0) {
			if (uni_state->real.released_units > high_units) {
				high_units = MIN(uni_state->real.released_units,
					high_units + TAF_IAS_EXTEND(uni_state));
			}
			if (uni_state->real.released_units > low_units) {
				low_units += high_units -
					TAF_MICROSEC_TO_UNITS(uni_state->high[IAS_INDEXM(method)]);
			}
		}

		WL_TAFT3(taf_info, "high %uus, low %uus, extend %uus\n",
			TAF_UNITS_TO_MICROSEC(high_units),
			TAF_UNITS_TO_MICROSEC(low_units),
			TAF_UNITS_TO_MICROSEC(TAF_IAS_EXTEND(uni_state)));

		WL_TAFT2(taf_info, "tuit %u\n", uni_state->total_units_in_transit);

		if (uni_state->real.released_units < high_units) {
			ias_info->data.uflow_high++;
			ias_info->debug.uflow_high++;
		} else {
			ias_info->data.target_high++;
			ias_info->debug.target_high++;
		}

		ias_info->debug.average_high += uni_state->real.released_units;

		ias_info->data.time_delta =
			TAF_IAS_NOWTIME(method) - ias_info->data.prev_release_time;
		ias_info->data.prev_release_time = TAF_IAS_NOWTIME(method);

		if (TAF_IAS_OPT(ias_info, TOOMUCH) && (low_units > 0) &&
				(uni_state->real.released_units < low_units) &&
				(uni_state->trigger == TAF_TRIGGER_IMMEDIATE) &&
				(uni_state->total_units_in_transit > (low_units + high_units)) &&
				(cnext == 0) && (cnow == 0)) {

			/* scheduler triggering runaway prevention */
			WL_TAFT1(taf_info, "too much immediate trig traffic in transit "
				"%uus, %s barrier (%u)\n",
				TAF_UNITS_TO_MICROSEC(uni_state->total_units_in_transit),
				uni_state->barrier_req == 0 ? "request" : "insert",
				uni_state->barrier_req);

			if (uni_state->barrier_req == 0) {
				uni_state->barrier_req = 1;
			} else {
				low_units = TAF_IAS_OPT(ias_info, TOOMUCH_LOOSE) ?
					uni_state->real.released_units - 1 : 0;
				ias_info->data.uflow_low++;
				ias_info->debug.uflow_low++;
				memset(uni_state->total_sched_units, 0,
				       sizeof(uni_state->total_sched_units));
			}
		} else {
			uni_state->barrier_req = 0;
		}

		if (uni_state->real.released_units > low_units) {
			uni_state->resched_units[cnext] =
				uni_state->real.released_units - low_units;
			if (uni_state->barrier_req == 0) {
				ias_info->data.target_low++;
				ias_info->debug.target_low++;
			} else {
				uni_state->barrier_req = 0;
			}
			if (cnext == 1) {
				ias_info->data.super_sched++;
				ias_info->debug.super_sched++;
			}
		} else {
			/* TX window underflow */
			ias_info->data.uflow_low++;
			ias_info->debug.uflow_low++;

			if (cnext == 1) {
				WL_TAFT1(taf_info, "cycle_next is 1 and low underflow, wait for "
					"all traffic to clear\n");
				/* this will wait to clear out entire data path to allow
				 * super-scheduling to resume again- prevent run-away
				 */
				uni_state->cycle_next = cnext = 0;
				uni_state->resched_units[0] = TAF_IAS_OPT(ias_info, SFAIL_LOOSE) ?
					1 : uni_state->real.released_units;
				uni_state->barrier_req = 0;
				ias_info->data.super_sched_collapse++;
				ias_info->debug.super_sched_collapse++;
				ias_info->debug.immed_star++;
				ias_info->data.immed_star++;
				context->status = TAF_CYCLE_FAILURE;
			} else if (cnow == 1) {
				WL_TAFT1(taf_info, "cycle_now is 1 and low underflow\n");
				uni_state->resched_units[0] = 1;
			} else if (ias_info->ncount_flow < ias_info->immed) {
				/* infrequent, small volume data */
				uni_state->resched_units[0] = TAF_IAS_OPT(ias_info, IMMED_LOOSE) ?
					1 : uni_state->real.released_units;
				WL_TAFT2(taf_info, "star pkt instead of immed trigger "
					"(%u, %u, %u)\n", ias_info->ncount_flow, ias_info->immed,
					uni_state->resched_units[0]);
				ias_info->debug.immed_star++;
				ias_info->data.immed_star++;
			} else {
				/* immediate re-trigger mode */
				uni_state->resched_units[0] = 0;
				ias_info->debug.immed_trig++;
				ias_info->data.immed_trig++;
			}
		}

		uni_state->resched_index[cnext] = context->public.ias.index;

		if (uni_state->resched_units[cnext] > 0) {
			bool star_pkt_seen;

			star_pkt_seen = uni_state->cumul_units[uni_state->resched_index[cnext]] >=
				uni_state->resched_units[cnext];

			if (taf_info->super_active && cnext && uni_state->cycle_ready == 0) {
				uni_state->cycle_ready = 1;
			}

			if (star_pkt_seen) {
				WL_TAFT1(taf_info, "final exit %uus released, trigger %u:%u "
					"already seen (prev rel is %u, in transit %u), sched dur "
					"%uus, r_t_s 0x%x\n\n",
					TAF_UNITS_TO_MICROSEC(uni_state->real.released_units),
					uni_state->resched_index[cnext],
					uni_state->resched_units[cnext],
					ias_info->data.time_delta,
					TXPKTPENDTOT(TAF_WLCT(taf_info)),
					duration, *scheduler->ready_to_schedule);
				uni_state->trigger = TAF_TRIGGER_NONE;
			} else {
				bool extra_cycle =
					taf_ias_super_resched(taf_info, uni_state, low_units,
					high_units, context);

				*scheduler->ready_to_schedule = extra_cycle ? ~0 : 0;

				WL_TAFT1(taf_info, "final exit %uus released "
					"%s trig %u:%uus (prev rel is %u, in transit "
					"%u), sched dur %uus, r_t_s 0x%x%s\n\n",
					TAF_UNITS_TO_MICROSEC(uni_state->real.released_units),
					cnext ? "next" : "now",
					uni_state->resched_index[cnext],
					TAF_UNITS_TO_MICROSEC(uni_state->resched_units[cnext]),
					ias_info->data.time_delta,
					TXPKTPENDTOT(TAF_WLCT(taf_info)),
					duration, *scheduler->ready_to_schedule,
					extra_cycle ? " RESCHED" : "");

				uni_state->trigger = TAF_TRIGGER_STAR_THRESHOLD;

				if (extra_cycle) {
					uni_state->cycle_next = cnext = 1;
				} else if (taf_info->super_active && cnext == 1 && cnow == 1) {
					uni_state->cycle_now = cnow = 0;
				}
			}
			/* mask index because the pkt tag only has few bits */
			ias_info->tag_star_index =
				(ias_info->tag_star_index + 1) & TAF_MAX_PKT_INDEX;
		} else {
			WL_TAFT1(taf_info, "final exit %uus released, immed "
				"retrig (%u) (prev rel is %u, in transit %u), "
				"sched dur %uus, r_t_s 0x%x\n\n",
				TAF_UNITS_TO_MICROSEC(uni_state->real.released_units),
				uni_state->resched_index[cnext],
				ias_info->data.time_delta, TXPKTPENDTOT(TAF_WLCT(taf_info)),
				duration, *scheduler->ready_to_schedule);
			uni_state->trigger = TAF_TRIGGER_IMMEDIATE;

			if (uni_state->barrier_req == 1) {
				/* mask index because the pkt tag only has few bits */
				ias_info->tag_star_index =
					(ias_info->tag_star_index + 1) & TAF_MAX_PKT_INDEX;

				WL_TAFT1(taf_info, "increment index to %u due to barrier req\n",
					ias_info->tag_star_index);
				++ias_info->debug.barrier_req;
			}
		}
		uni_state->total_sched_units[context->public.ias.index] =
			uni_state->real.released_units;

		taf_info->release_count ++;
	} else if (finished) {
#if TAF_ENABLE_TIMER
		if (uni_state->total_units_in_transit == 0 && TAF_IAS_OPT(ias_info, RETRIGGER) &&
			taf_ias_retrig_start(taf_info, TAF_IAS_NOWTIME(method)) == BCME_OK) {

			WL_TAFT1(taf_info, "final exit nothing released, start retrigger timer; "
				"retrig (%u) (in transit %u)\n\n",
				uni_state->resched_index[uni_state->cycle_next],
				TXPKTPENDTOT(TAF_WLCT(taf_info)));
		} else
#endif // endif
		{
			WL_TAFT1(taf_info, "final exit nothing released, immed "
				"retrig (%u) (in transit %u)\n\n",
				uni_state->resched_index[uni_state->cycle_next],
				TXPKTPENDTOT(TAF_WLCT(taf_info)));
		}
		uni_state->trigger = TAF_TRIGGER_IMMEDIATE;
	}

	if (finished) {
		uni_state->real.released_units = 0;
		uni_state->total.released_units = 0;
		uni_state->extend.window_dur_units = 0;
		uni_state->extend.rounding_units = 0;
		uni_state->est_release_units = 0;
		uni_state->waiting_schedule = 0;
		uni_state->release_type_present = 0;
		uni_state->bw_type_present = 0;

		ias_info->ncount_flow_last = ias_info->ncount_flow;
		ias_info->ncount_flow = 0;

		if (taf_info->super_active) {
			WL_TAFT2(taf_info, "super sched: wait %u/%u, cycle now %u; cycle next "
				"%u (%u/%u), ready %u, wait sched %u\n\n",
				uni_state->resched_index[0],
				TAF_UNITS_TO_MICROSEC(uni_state->resched_units[0]),
				uni_state->cycle_now, uni_state->cycle_next,
				uni_state->resched_index[1],
				TAF_UNITS_TO_MICROSEC(uni_state->resched_units[1]),
				uni_state->cycle_ready,
				uni_state->waiting_schedule);
		}
		uni_state->star_packet_received = FALSE;

		context->status = TAF_CYCLE_COMPLETE;
	}
	return finished;
}

static bool taf_ias_data_block(taf_method_info_t *method)
{
#ifdef TAF_DBG
	wlc_taf_info_t *taf_info = method->taf_info;
	taf_ias_group_info_t* ias_info = TAF_IAS_GROUP_INFO(taf_info);
#endif /* TAF_DBG */

	if (method->ias->data_block) {
#ifdef TAF_DBG
		if (ias_info->data_block_start == 0) {
			ias_info->data_block_start = TAF_IAS_NOWTIME(method);
			ias_info->data_block_prev_in_transit =
				TXPKTPENDTOT(TAF_WLCT(taf_info));
			WL_TAFM1(method, "exit due to data_block (START)\n");
		}
		if (TXPKTPENDTOT(TAF_WLCT(taf_info)) !=
				ias_info->data_block_prev_in_transit) {
			ias_info->data_block_prev_in_transit =
				TXPKTPENDTOT(TAF_WLCT(taf_info));
			WL_TAFM1(method, "exit due to data_block (%uus)\n",
				(TAF_IAS_NOWTIME(method) - ias_info->data_block_start));
		}
#endif /* TAF_DBG */
		return TRUE;
	}
#ifdef TAF_DBG
	else if (ias_info->data_block_start) {
		ias_info->data_block_total += TAF_IAS_NOWTIME(method) - ias_info->data_block_start;
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
	uint32 prev_time;
	taf_ias_uni_state_t *uni_state;
	taf_ias_group_info_t* ias_info;
	taf_scb_cubby_t* scb_taf = v2r_event ? NULL : *SCB_TAF_CUBBY_PTR(taf_info, context->scb);

	prev_time = context->now_time;
	TAF_IAS_NOWTIME_SYNC(method, prev_time);

	if ((taf_info->op_state != TAF_MARKUP_REAL_PACKETS) ||
			(context->op_state != TAF_MARKUP_REAL_PACKETS)) {
		WL_ERROR(("wl%u %s: NOT IN MARKUP PHASE\n",
			WLCWLUNIT(TAF_WLCT(taf_info)), __FUNCTION__));
		TAF_ASSERT(0);
		return FALSE;
	}

	if (!v2r_event && scb_taf && (scb_taf->info.pkt_pull_dequeue == 0)) {

		WL_TAFM4(method, MACF" tid %u got no new dequeued "
			"packets (%u) %s\n",
			TAF_ETHERC(scb_taf), tid,
			scb_taf->info.pkt_pull_dequeue,
			scb_taf->info.ps_mode ? "in PS mode":"- exit");
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

	uni_state = taf_get_uni_state(taf_info);
	ias_info = TAF_IAS_GROUP_INFO(taf_info);
	context->public.ias.index = ias_info->tag_star_index;

	if (!v2r_event && scb_taf) {
#if TAF_ENABLE_TIMER
		const bool agg_hold = taf_ias_agghold_is_active(ias_info);
#else
		const bool agg_hold = FALSE;
#endif // endif

		if (!taf_ias_data_block(method) && !agg_hold) {
			if (scb_taf->info.ps_mode) {
				/* unsolicited ps poll */
				scb_taf->info.tech_type[TAF_TYPE_DL][tid] = TAF_TECH_DL_SU;
			}
			taf_ias_sched_send(method, scb_taf, tid, uni_state,
				TAF_IAS_TID_STATE(scb_taf, tid), context,
				TAF_SCHEDULE_REAL_PACKETS, TAF_TYPE_DL);
		}
		scb_taf->info.pkt_pull_dequeue = 0;

		if (!scb_taf->info.ps_mode && !agg_hold) {
			taf_ias_sched_update(scb_taf->method, scb_taf, tid, uni_state, TAF_TYPE_DL);
		}

		WL_TAFT3(taf_info, "total_pull_requests %u\n", taf_info->total_pull_requests);
	} else if (v2r_event) {
		WL_TAFM3(method, "v2r completion\n");
	}

	context->now_time = taf_timestamp(TAF_WLCT(taf_info));
	TAF_IAS_NOWTIME_SYNC(method, context->now_time);

	ias_info->cpu_time += (context->now_time - prev_time);

	if (wlc_taf_marked_up(taf_info)) {
		return taf_ias_completed(method, context, uni_state, TRUE,
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
	taf_ias_uni_state_t *uni_state;
	uint32 prev_time;
	uint32 prev_est_units;

	prev_time = context->now_time;
	TAF_IAS_NOWTIME_SYNC(method, prev_time);

#if TAF_ENABLE_SQS_PULL
	/* packet markup is handle only by taf_ias_markup */
	TAF_ASSERT(taf_info->op_state != TAF_MARKUP_REAL_PACKETS);
	TAF_ASSERT(context->op_state != TAF_MARKUP_REAL_PACKETS);

	if (context->op_state == TAF_SCHEDULE_VIRTUAL_PACKETS) {
		TAF_ASSERT(taf_info->op_state == TAF_SCHEDULE_VIRTUAL_PACKETS);
	}
#endif /* TAF_ENABLE_SQS_PULL */

	if (context->op_state == TAF_SCHEDULE_REAL_PACKETS) {
		TAF_ASSERT(context->tid >= 0 && context->tid < TAF_MAXPRIO);
	}

	if (!method->ias->tid_active && method->type != LAST_IAS_SCHEDULER) {
		return FALSE;
	}

	ias_info = TAF_IAS_GROUP_INFO(taf_info);

#if TAF_ENABLE_TIMER
	if (taf_ias_retrig_is_active(ias_info)) {

		taf_ias_retrig_stop(taf_info, ias_info, context->now_time);
		WL_TAFM2(method, "retriggered without timer\n");
	}
#endif // endif

	if (taf_ias_data_block(method)) {
		return TRUE;
	}

	uni_state = taf_get_uni_state(taf_info);
	uni_state->was_reset = 0;

	if (ias_info->sched_start == 0) {
		ias_info->sched_start = context->now_time;
	}

	method->ias->sched_active = 0;
	method->counter++;

	if (uni_state->total.released_units > 0) {
		context->state[TAF_SCHED_INDEX(method->type)] |= (1 << TAF_STATE_HIGHER_PEND);
	}

	prev_est_units = uni_state->est_release_units;
	taf_ias_prepare_list(method);

	if (uni_state->est_release_units > prev_est_units) {

		uint32 new_traffic = uni_state->est_release_units - prev_est_units;

		WL_TAFM2(method, "total est units %u (total in transit %u, total released %u)\n",
			new_traffic, uni_state->total_units_in_transit,
			uni_state->real.released_units);
#if TAF_ENABLE_TIMER
		if ((uni_state->total.released_units == 0) && (ias_info->agg_hold > 0)) {
			if (new_traffic < ias_info->agg_hold_threshold) {
				if (!taf_ias_agghold_was_active(ias_info) &&
						!taf_ias_agghold_is_active(ias_info)) {

					taf_ias_agghold_start(taf_info, ias_info->agg_hold,
						context->now_time);
				}
#if !TAF_ENABLE_SQS_PULL
				if (taf_ias_agghold_is_active(ias_info)) {
					method->ias->sched_active = 0;
					WL_TAFM2(method, "aggregation hold active\n");
				}
#endif /* !TAF_ENABLE_SQS_PULL */
			} else {
				if (taf_ias_agghold_is_active(ias_info)) {
					WL_TAFM1(method, "enough traffic to end agg_hold\n");
					taf_ias_agghold_stop(taf_info, ias_info, context->now_time);
					taf_ias_agghold_clear(ias_info);
				}
			}
		}
#else
		BCM_REFERENCE(new_traffic);
#endif /* TAF_ENABLE_TIMER */
	}

	if ((ias_info->tag_star_index == ias_info->prev_tag_star_index) &&
		(uni_state->trigger == TAF_TRIGGER_IMMEDIATE) &&
		(uni_state->real.released_units == 0) &&
		(uni_state->est_release_units >
			TAF_MICROSEC_TO_UNITS(uni_state->low[IAS_INDEXM(method)]))) {

		memset(uni_state->total_sched_units, 0,  sizeof(uni_state->total_sched_units));
		ias_info->tag_star_index = (ias_info->tag_star_index + 1) & TAF_MAX_PKT_INDEX;
		WL_TAFM1(method, "incremented tag index now %u (%u, %u)\n",
			ias_info->tag_star_index, uni_state->est_release_units,
			TAF_MICROSEC_TO_UNITS(uni_state->low[IAS_INDEXM(method)]));
	}

	context->public.how = TAF_RELEASE_LIKE_IAS;
	context->public.ias.index = ias_info->tag_star_index;
	uni_state->cumul_units[ias_info->tag_star_index] = 0;

	if (method->ias->sched_active) {
		finished = taf_ias_schedule_all_tid(method, uni_state, 0, NUMPRIO - 1,
			context);
	}

	context->now_time = taf_timestamp(TAF_WLCT(taf_info));
	TAF_IAS_NOWTIME_SYNC(method, context->now_time);

	ias_info->cpu_time += (context->now_time - prev_time);

	return taf_ias_completed(method, context, uni_state, finished, taf_info->op_state);
}

static int BCMATTACHFN(taf_ias_method_detach)(void* context)
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
#if TAF_ENABLE_TIMER
				if (ias_info->agg_hold_timer) {
					wl_free_timer(TAF_WLCT(taf_info)->wl,
						ias_info->agg_hold_timer);
				}
#endif // endif
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
#ifdef TAF_DEBUG_VERBOSE
	bool do_dump = FALSE;
#endif // endif
	wlc_info_t *wlc = TAF_WLCM(method);
	taf_ias_uni_state_t* uni_state;

#if TAF_ENABLE_SQS_PULL
	TAF_ASSERT(method->type != TAF_VIRTUAL_MARKUP);
#endif // endif
	if (list == NULL) {
		method->ias->tid_active = 0;
		WL_TAFM4(method, "no links active\n");
		return BCME_OK;
	}

	while (list) {
		taf_scb_cubby_t *scb_taf;

		scb_taf = list->scb_taf;
		enabled |= scb_taf->info.tid_enabled;

		list = list->next;
	}
	if (method->ias->tid_active != enabled) {
		WL_TAFM1(method, "current enable 0x%x, checked enable 0x%x\n",
			method->ias->tid_active, enabled);
		method->ias->tid_active = enabled;
	}

	if (TXPKTPENDTOT(wlc)) {
		/* if not idle, just exit this housekeeping now */
		return BCME_OK;
	}

	/* Check for active transmission from AMPDU/NAR paths */
#ifdef WLNAR
	if (wlc_nar_tx_in_tansit(wlc->nar_handle)) {
		return BCME_OK;
	}
#endif // endif

#ifdef WLAMPDU
	if (wlc_ampdu_tx_intransit_get(wlc)) {
		return BCME_OK;
	}
#endif // endif

#if TAF_ENABLE_UL
	if (ULTRIGPENDTOT(wlc)) {
		/* if not idle, just exit this housekeeping now */
		return BCME_OK;
	}
#endif /* TAF_ENABLE_UL */

	uni_state = taf_get_uni_state(taf_info);

	/* total packets pending is zero (as watchdog exits earlier if not) */
	if (uni_state->total_units_in_transit > 0) {
		WL_ERROR(("wl%u %s: (%x) %u units in transit (expect 0)\n",
			WLCWLUNIT(wlc), __FUNCTION__,
			taf_info->scheduler_index,
			uni_state->total_units_in_transit));
#ifdef TAF_DEBUG_VERBOSE
		do_dump  = TRUE;
#endif // endif
		taf_ias_unified_reset(method);
	}

	if (*method->ready_to_schedule == 0) {
		WL_ERROR(("wl%u %s: (%x) (%s) not ready to schedule (%x)\n",
			WLCWLUNIT(wlc), __FUNCTION__,
			taf_info->scheduler_index, TAF_SCHED_NAME(method),
			method->ias->tid_active));
#ifdef TAF_DEBUG_VERBOSE
		do_dump  = TRUE;
#endif // endif
	}

#ifdef TAF_DEBUG_VERBOSE
	if (do_dump) {
		taf_memtrace_dump(taf_info);
	}
#endif // endif

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
	taf_ias_dpstats_dump,   /* dpstats_log_fn   */
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

static void* BCMATTACHFN(taf_ias_method_attach)(wlc_taf_info_t *taf_info, taf_scheduler_kind type)
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
		ias_info->immed = 24000;
		ias_info->now_time = taf_timestamp(TAF_WLCT(taf_info));
		ias_info->options =
			(1 << TAF_OPT_IAS_TOOMUCH) |
			(1 << TAF_OPT_IAS_TOOMUCH_LOOSE) |
			(1 << TAF_OPT_IAS_SFAIL_LOOSE) |
			(1 << TAF_OPT_IAS_IMMED_LOOSE) |
			(1 << TAF_OPT_IAS_AUTOBW20_SUPER);
#if TAF_ENABLE_TIMER
		ias_info->options |= (1 << TAF_OPT_IAS_RETRIGGER);
		ias_info->agg_hold_timer =
			wl_init_timer(TAF_WLCT(taf_info)->wl, taf_ias_aggh_tmr_exp,
				taf_info, "TAF ias_agg_hold");
		if (ias_info->agg_hold_timer == NULL) {
			WL_ERROR(("wl%d: wl_init_timer for TAF ias_agg_hold failed\n",
				TAF_WLCT(taf_info)->pub->unit));
			goto exitfail;
		}
#endif // endif
	} else {
		ias_info = TAF_IAS_GROUP_INFO(taf_info);
		TAF_ASSERT(ias_info);
	}
	taf_info->group_use_count[TAF_SCHEDULER_IAS_METHOD] ++;
	method->now_time_p = &ias_info->now_time;

	cfg_ias->coeff.time_shift = TAF_COEFF_IAS_FACTOR;
	taf_ias_set_coeff(TAF_COEFF_IAS_DEF, &cfg_ias->coeff);

#if TAF_ENABLE_SQS_PULL
#if TAF_ENABLE_NAR
	cfg_ias->pre_rel_limit[TAF_SOURCE_NAR] = 1;
#endif /* TAF_ENABLE_NAR */

	cfg_ias->pre_rel_limit[TAF_SOURCE_AMPDU] = 32;

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
#if TAF_ENABLE_MU_BOOST
			cfg_ias->mu_boost_limit = 1024;
			cfg_ias->mu_boost_compensate = 1024;
#endif /* TAF_ENABLE_MU_BOOST */
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
				(1 << TAF_OPT_MIMO_SWEEP) |
				(1 << TAF_OPT_MIMO_PAIR_MAXMU) |
				(1 << TAF_OPT_MIMO_INT_EXTEND);

			cfg_ias->mu_ofdma_opt =
				(1 << TAF_OPT_OFDMA_TOO_SMALL) |
				(1 << TAF_OPT_OFDMA_TOO_LARGE) |
				(1 << TAF_OPT_OFDMA_PAIR) |
				(1 << TAF_OPT_OFDMA_SWEEP) |
				(1 << TAF_OPT_OFDMA_INT_EXTEND);
#endif /* TAF_ENABLE_MU_TX */
#if TAF_ENABLE_MU_BOOST
			cfg_ias->mu_boost_limit = 1024;
			cfg_ias->mu_boost_compensate = 1024;
#endif /* TAF_ENABLE_MU_BOOST */
			cfg_ias->options =
				(1 << TAF_OPT_EXIT_EARLY) |
				(1 << TAF_OPT_PKT_NAR_OVERHEAD) |
				(1 << TAF_OPT_PKT_AMPDU_OVERHEAD);
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
				(1 << TAF_OPT_MIMO_SWEEP) |
				(1 << TAF_OPT_MIMO_PAIR_MAXMU) |
				(1 << TAF_OPT_MIMO_INT_EXTEND);

			cfg_ias->mu_ofdma_opt =
				(1 << TAF_OPT_OFDMA_TOO_SMALL) |
				(1 << TAF_OPT_OFDMA_TOO_LARGE) |
				(1 << TAF_OPT_OFDMA_PAIR) |
				(1 << TAF_OPT_OFDMA_SWEEP) |
				(1 << TAF_OPT_OFDMA_INT_EXTEND);
#endif /* TAF_ENABLE_MU_TX */
#if TAF_ENABLE_MU_BOOST
			cfg_ias->mu_boost_limit = 256;
			cfg_ias->mu_boost_compensate = 1024;
#endif /* TAF_ENABLE_MU_BOOST */
			cfg_ias->options =
				(1 << TAF_OPT_EXIT_EARLY) |
				(1 << TAF_OPT_PKT_NAR_OVERHEAD) |
				(1 << TAF_OPT_PKT_AMPDU_OVERHEAD);
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
				(1 << TAF_OPT_MIMO_SWEEP) |
				(1 << TAF_OPT_MIMO_PAIR_MAXMU) |
				(1 << TAF_OPT_MIMO_INT_EXTEND);

			cfg_ias->mu_ofdma_opt =
				(1 << TAF_OPT_OFDMA_TOO_SMALL) |
				(1 << TAF_OPT_OFDMA_TOO_LARGE) |
				(1 << TAF_OPT_OFDMA_PAIR) |
				(1 << TAF_OPT_OFDMA_SWEEP) |
				(1 << TAF_OPT_OFDMA_INT_EXTEND);
#endif /* TAF_ENABLE_MU_TX */
#if TAF_ENABLE_MU_BOOST
			cfg_ias->mu_boost_limit = 256;
			cfg_ias->mu_boost_compensate = 1024;
#endif /* TAF_ENABLE_MU_BOOST */
			cfg_ias->options =
				(1 << TAF_OPT_EXIT_EARLY) |
				(1 << TAF_OPT_PKT_NAR_OVERHEAD) |
				(1 << TAF_OPT_PKT_AMPDU_OVERHEAD);
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

#if TAF_ENABLE_SQS_PULL && defined(TAF_DBG)
static INLINE bool taf_ias_sqs_capable(taf_scb_cubby_t *scb_taf, int tid)
{
	wlc_taf_info_t* taf_info;
	const taf_source_type_t s_idx = TAF_SOURCE_HOST_SQS;

	TAF_ASSERT(scb_taf && tid >= 0 && tid < NUMPRIO);

	taf_info = scb_taf->method->taf_info;

	return wlc_sqs_ampdu_capable(TAF_WLCT(taf_info),
		taf_info->funcs[s_idx].scb_h_fn(*(taf_info->source_handle_p[s_idx]),
		scb_taf->scb), tid);
}
#endif /* TAF_ENABLE_SQS_PULL && TAF_DBG */

int taf_ias_dump(void *handle, struct bcmstrbuf *b)
{
	taf_method_info_t* method = handle;
	wlc_taf_info_t *taf_info = method->taf_info;
	taf_ias_group_info_t* ias_info = TAF_IAS_GROUP_INFO(taf_info);
	uint32 prev_time = ias_info->cpu_elapsed_time;
	uint32 cpu_norm = 0;
	taf_list_t*  list = method->list;
	taf_ias_uni_state_t* uni_state = taf_get_uni_state(taf_info);
	uint32 total_count;
	uint32 ave_high = 0;
	uint32 ave_dur = 0;
#if TAF_ENABLE_TIMER
	uint32 ave_agg_hold = 0;
#endif // endif

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

#if TAF_ENABLE_TIMER
	if (ias_info->debug.agg_hold_count > 0) {
		uint64 total_hold = ias_info->debug.agg_hold_total_time;
		uint32 count = ias_info->debug.agg_hold_count;

		while (total_hold > (uint64)0xFFFFFFFF) {
			total_hold >>= 1;
			count >>= 1;
		}
		if (count > 0) {
			ave_agg_hold = (uint32)total_hold;
			ave_agg_hold /= count;
		}
	}
#endif // endif
	total_count = ias_info->debug.target_high + ias_info->debug.uflow_high;

	if (total_count > 0) {
		ave_high = ias_info->debug.average_high / total_count;
		ave_dur = ias_info->debug.sched_duration / total_count;
	}

	bcm_bprintf(b,
		"unified\n"
		"target high  %7u, uflow high  %7u,    ave_high    %7uus\n"
		"target low   %7u, uflow low   %7u\n"
		"immed trigg  %7u, immed star  %7u\n"
		"super_sched  %7u, sschedfail  %7u\n"
		"barrier req  %7u, tuit        %7u\n"
#if TAF_ENABLE_TIMER
		"agg hold     %7u, max hold    %7uus,  ave hold    %7uus\n"
#endif // endif
		"ave sched duration %5uus, max sched duration %5uus\n",
		ias_info->debug.target_high, ias_info->debug.uflow_high,
		TAF_UNITS_TO_MICROSEC(ave_high),
		ias_info->debug.target_low, ias_info->debug.uflow_low,
		ias_info->debug.immed_trig, ias_info->debug.immed_star,
		ias_info->debug.super_sched, ias_info->debug.super_sched_collapse,
		ias_info->debug.barrier_req, uni_state->total_units_in_transit,
#if TAF_ENABLE_TIMER
		ias_info->debug.agg_hold_count, ias_info->debug.agg_hold_max_time,
		ave_agg_hold,
#endif // endif
		ave_dur, ias_info->debug.max_duration);

	memset(&ias_info->debug, 0, sizeof(ias_info->debug));

	while (list) {
		taf_scb_cubby_t *scb_taf = list->scb_taf;
#ifdef TAF_DBG
#if TAF_LOGL3
		taf_source_type_t s_idx;
#endif // endif
		taf_scheduler_scb_stats_t* scb_stats = &scb_taf->info.scb_stats;
		taf_rspec_index_t rindex;
#endif /* TAF_DBG */

		bcm_bprintf(b, "\n---\n"MACF"%s score %4u %s\n", TAF_ETHERC(scb_taf),
			TAF_TYPE(list->type),
			scb_taf->info.scb_stats.ias.data.relative_score[list->type],
			scb_taf->info.ps_mode ? " PS":"");

#ifdef TAF_DBG
		for (rindex = TAF_RSPEC_SU_DL; rindex < NUM_TAF_RSPECS; rindex++) {
			ratespec_t rspec = scb_stats->global.rdata.rspec[rindex];
			bool sgi = FALSE;

			if (rspec == 0 ||
				((scb_stats->global.rdata.ratespec_mask & (1 << rindex)) == 0)) {
				continue;
			}
#if TAF_ENABLE_UL
			if (list->type == TAF_TYPE_UL && rindex != TAF_RSPEC_UL) {
				continue;
			}
			if (list->type == TAF_TYPE_DL && rindex == TAF_RSPEC_UL) {
				continue;
			}
#endif // endif
			switch ((rspec & WL_RSPEC_ENCODING_MASK) >> WL_RSPEC_ENCODING_SHIFT) {
				case WL_RSPEC_ENCODE_HE >> WL_RSPEC_ENCODING_SHIFT:
					sgi = HE_IS_GI_0_8us(RSPEC_HE_LTF_GI(rspec));
					break;
				case WL_RSPEC_ENCODE_VHT >> WL_RSPEC_ENCODING_SHIFT:
				case WL_RSPEC_ENCODE_HT >> WL_RSPEC_ENCODING_SHIFT:
					sgi = RSPEC_ISSGI(rspec);
				default:
					break;
			}
#if TAF_ENABLE_MU_BOOST
			if (TAF_RIDX_IS_MUMIMO(rindex) &&
				(taf_info->mu_boost == TAF_MUBOOST_FACTOR ||
				taf_info->mu_boost == TAF_MUBOOST_RATE_FACTOR)) {

				bcm_bprintf(b, "ridx %u: rspec 0x%08x, nss %u, bw %3u, mcs %2u, "
					"phy %u, sgi %u, rate %7u, boost:mu clients %u "
					"groups %u\n",
					rindex, rspec, wlc_ratespec_nss(rspec),
					wlc_ratespec_bw(rspec), wlc_ratespec_mcs(rspec),
					(rspec & WL_RSPEC_ENCODING_MASK) >> WL_RSPEC_ENCODING_SHIFT,
					sgi, RSPEC2KBPS(rspec),
					scb_taf->info.scb_stats.global.rdata.mu_clients_count,
					scb_taf->info.scb_stats.global.rdata.mu_groups_count);
			} else
#endif /* TAF_ENABLE_MU_BOOST */
			{
				bcm_bprintf(b, "ridx %u: rspec 0x%08x, nss %u, bw %3u, mcs %2u, "
					"phy %u, sgi %u, rate %7u\n",
					rindex, rspec, wlc_ratespec_nss(rspec),
					wlc_ratespec_bw(rspec), wlc_ratespec_mcs(rspec),
					(rspec & WL_RSPEC_ENCODING_MASK) >> WL_RSPEC_ENCODING_SHIFT,
					sgi, RSPEC2KBPS(rspec));
			}
		}
		bcm_bprintf(b, "\n");

#if TAF_LOGL3
		for (s_idx = 0; s_idx < TAF_NUM_SCHED_SOURCES; s_idx ++) {
			bool enabled = scb_taf->info.scb_stats.ias.data.use[s_idx];

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
				taf_ias_sched_data_t *ias_sched = TAF_IAS_TID_STATE(scb_taf, tid);

				const char * st_txt = taf_link_states_text[st];

				if (st == TAF_LINKSTATE_NONE || !ias_sched) {
					continue;
				}
#if TAF_ENABLE_SQS_PULL
				if (TAF_SOURCE_IS_SQS(s_idx)) {
					msg = taf_ias_sqs_capable(scb_taf, tid) ?
					      "sqs cap" : "sqs incap";
				}
#endif /* TAF_ENABLE_SQS_PULL */
				if (TAF_SOURCE_IS_AMPDU(s_idx)) {
					uint32 flow;
					if (!SCB_AMPDU(scb_taf->scb)) {
						msg = "ampdu incap";
					}
					flow = ias_sched->rel_stats.data.total_scaled_ncount;

					bcm_bprintf(b, "%u: [%u] %s (%u/%u/%u) %s\n", tid, st,
						st_txt, flow >> TAF_NCOUNT_SCALE_SHIFT,
						SCB_PKTS_INFLT_FIFOCNT_VAL(scb_taf->scb, tid),
						scb_taf->info.traffic.count[s_idx][tid], msg);
				} else {
					bcm_bprintf(b, "%u: [%u] %s (%u) %s\n", tid, st, st_txt,
						scb_taf->info.traffic.count[s_idx][tid], msg);
				}
			}
		}
#endif /* TAF_LOGL3 */
#endif /* TAF_DBG */
		list = list->next;
	}
	return BCME_OK;
}

static void taf_ias_unified_reset(taf_method_info_t* method)
{
	taf_ias_group_info_t* ias_info = TAF_IAS_GROUP_INFO(method->taf_info);
	int index;
	taf_ias_uni_state_t* uni_state;

#if TAF_ENABLE_TIMER
	taf_ias_agghold_reset(method);
#endif // endif
	uni_state = taf_get_uni_state(method->taf_info);

	if (uni_state->was_reset) {
		return;
	}

	WL_TAFM1(method, "\n");

	uni_state->trigger = TAF_TRIGGER_NONE;
	uni_state->real.released_units = 0;
	uni_state->total.released_units = 0;
	uni_state->star_packet_received = FALSE;
	uni_state->super_sched_last = 0;

	for (index = 0; index < TAF_MAX_NUM_WINDOWS; index ++) {
		uni_state->cumul_units[index] = 0;
		uni_state->total_sched_units[index] = 0;
	}

	uni_state->total_units_in_transit = 0;
	uni_state->prev_resched_index = -1;
	uni_state->resched_units[0] = 0;
	uni_state->resched_units[1] = 0;
	uni_state->resched_index[0] = ias_info->tag_star_index;
	uni_state->cycle_now = 0;
	uni_state->cycle_next = 0;
	uni_state->cycle_ready = 0;
	uni_state->barrier_req = 0;

	uni_state->high[IAS_INDEXM(method)] = method->ias->high;
	uni_state->low[IAS_INDEXM(method)] = method->ias->low;

	memset(&ias_info->data, 0, sizeof(ias_info->data));
	memset(&ias_info->debug, 0, sizeof(ias_info->debug));
	memset(&uni_state->real, 0, sizeof(uni_state->real));
	memset(&uni_state->total, 0, sizeof(uni_state->total));
	memset(&uni_state->extend, 0, sizeof(uni_state->extend));

	uni_state->was_reset ++;

	*method->ready_to_schedule = ~(0);

	WL_TAFM1(method, "r_t_s 0x%x\n", *method->ready_to_schedule);
}

static INLINE void taf_ias_upd_ts2_scb_flag(taf_scb_cubby_t *scb_taf)
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

	ias_sched->released_units_limit[TAF_TYPE_DL] = 0;
#if TAF_ENABLE_UL
	ias_sched->released_units_limit[TAF_TYPE_UL] = 0;
#endif // endif

	if (ias_sched->used & (1 << s_idx)) {
		void* src_h = *(taf_info->source_handle_p[s_idx]);
		TAF_ASSERT(src_h);

		WL_TAFM1(method, "reset %s for "MACF"%s tid %u\n", TAF_SOURCE_NAME(s_idx),
			TAF_ETHERC(scb_taf), TAF_TYPE(TAF_SRC_TO_TYPE(s_idx)), tid);

		ias_sched->handle[s_idx].scbh =
			taf_info->funcs[s_idx].scb_h_fn(src_h, scb_taf->scb);
		ias_sched->handle[s_idx].tidh =
			taf_info->funcs[s_idx].tid_h_fn(ias_sched->handle[s_idx].scbh, tid);

		if (!wlc_taf_scheduler_blocked(taf_info)) {
			*method->ready_to_schedule = ~(0);
		}
		method->ias->tid_active |= (1 << tid);
		scb_taf->info.traffic.map[s_idx] |= (1 << tid);

		if (!TAF_SOURCE_IS_UL(s_idx)) {
			ias_sched->rel_stats.data.calc_timestamp =
				scb_taf->timestamp[TAF_TYPE_DL];
			/* initial bias in packet averaging initialisation */
			ias_sched->rel_stats.data.release_pkt_size_mean = TAF_PKT_SIZE_DEFAULT_DL;
			ias_sched->rel_stats.data.total_scaled_ncount =
				(4 << TAF_PKTCOUNT_SCALE_SHIFT);
		}

		taf_ias_flush_rspec(scb_taf);

		WL_TAFM1(method, "ias_sched for "MACF"%s tid %u source %s reset (%u) "
			"r_t_s 0x%x\n",  TAF_ETHERC(scb_taf), TAF_TYPE(TAF_SRC_TO_TYPE(s_idx)), tid,
			TAF_SOURCE_NAME(s_idx), ias_sched->used & (1 << s_idx) ? 1 : 0,
			*method->ready_to_schedule);
	} else {
		ias_sched->handle[s_idx].scbh = NULL;
		ias_sched->handle[s_idx].tidh = NULL;

		WL_TAFM1(method, "ias_sched for "MACF"%s tid %u source %s unused r_t_s "
			"0x%x\n", TAF_ETHERC(scb_taf), TAF_TYPE(TAF_SRC_TO_TYPE(s_idx)), tid,
			TAF_SOURCE_NAME(s_idx), *method->ready_to_schedule);
	}
	return BCME_OK;
}

static int taf_ias_tid_remove(taf_method_info_t* method, taf_scb_cubby_t *scb_taf, int tid)
{
	taf_ias_sched_data_t *ias_sched = TAF_IAS_TID_STATE(scb_taf, tid);

	TAF_ASSERT(ias_sched);
	if (ias_sched) {
#if TAF_IAS_STATIC_CONTEXT
		WL_TAFM2(method, "ias_sched for "MACF" tid %u reset NULL (in flight count %u)\n",
			TAF_ETHERC(scb_taf), tid, SCB_PKTS_INFLT_FIFOCNT_VAL(scb_taf->scb, tid));
#else
		MFREE(TAF_WLCM(method)->pub->osh, ias_sched, sizeof(*ias_sched));
		WL_TAFM2(method, "ias_sched for "MACF" tid %u destroyed (in flight count %u)\n",
			TAF_ETHERC(scb_taf), tid, SCB_PKTS_INFLT_FIFOCNT_VAL(scb_taf->scb, tid));
#endif // endif
		TAF_CUBBY_TIDINFO(scb_taf, tid) = NULL;
	}
	scb_taf->info.tid_enabled &= ~(1 << tid);
	scb_taf->info.traffic.est_units[TAF_TYPE_DL][tid] = 0;
	scb_taf->info.traffic.available[TAF_TYPE_DL] &= ~(1 << tid);
#if TAF_ENABLE_UL
	scb_taf->info.traffic.est_units[TAF_TYPE_UL][tid] = 0;
	scb_taf->info.traffic.available[TAF_TYPE_UL] &= ~(1 << tid);
#endif // endif
#if TAF_ENABLE_SQS_PULL
	if (scb_taf->info.pkt_pull_request[tid] > 0) {
		WL_TAFM1(method, "pkt pull req for "MACF" tid %u was %u, total pull count %u, "
			"reset\n", TAF_ETHERC(scb_taf), tid, scb_taf->info.pkt_pull_request[tid],
			method->taf_info->total_pull_requests);

		--method->taf_info->total_pull_requests;
		WL_TAFT1(method->taf_info, "total_pull_requests %u\n",
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

#if TAF_IAS_STATIC_CONTEXT
	BCM_REFERENCE(wlc);

	ias_sched = (taf_ias_sched_data_t*) &scb_taf->info.static_context[tid][0];

	WL_TAFM2(method, "ias_sched for "MACF" tid %u static assigned\n", TAF_ETHERC(scb_taf), tid);
#else
	ias_sched = (taf_ias_sched_data_t*) MALLOCZ(wlc->pub->osh, sizeof(*ias_sched));

	if (!ias_sched) {
		WL_ERROR(("wl%u %s: malloc fail (%u)\n", WLCWLUNIT(wlc), __FUNCTION__,
			(uint32)sizeof(*ias_sched)));
		return BCME_NOMEM;
	}
		WL_TAFM2(method, "ias_sched for "MACF" tid %u created\n", TAF_ETHERC(scb_taf), tid);
#endif // endif

	TAF_CUBBY_TIDINFO(scb_taf, tid) = ias_sched;

	scb_taf->info.tid_enabled |= (1 << tid);

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
				if (status == BCME_OK) {
					ias_sched = TAF_IAS_TID_STATE(scb_taf, tid);
				}
			}
			if (ias_sched) {
				scb_taf->info.linkstate[s_idx][tid] = state;
			}
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
				ias_sched->used |= (1 << s_idx);
				status = taf_ias_sched_reset(method, scb_taf, tid, s_idx);
				WL_TAFM2(method, "ias_sched for "MACF" tid %u %s active\n",
					TAF_ETHERS(scb), tid, TAF_SOURCE_NAME(s_idx));
				scb_taf->info.linkstate[s_idx][tid] = state;
			}
			break;

		case TAF_LINKSTATE_NOT_ACTIVE:
			if (ias_sched) {
				ias_sched->used &= ~(1 << s_idx);
				status = taf_ias_sched_reset(method, scb_taf, tid, s_idx);
			}
			scb_taf->info.linkstate[s_idx][tid]  = state;
			WL_TAFM2(method, "ias_sched for "MACF" tid %u %s now inactive\n",
				TAF_ETHERS(scb), tid, TAF_SOURCE_NAME(s_idx));
			break;

		case TAF_LINKSTATE_NONE:
		case TAF_LINKSTATE_REMOVE:
			scb_taf->info.linkstate[s_idx][tid] = TAF_LINKSTATE_NONE;
			scb_taf->info.traffic.count[s_idx][tid] = 0;
			scb_taf->info.traffic.map[s_idx] &= ~(1 << tid);

			if (ias_sched) {
				taf_source_type_t _idx;
				bool allfree = TRUE;

				ias_sched->used &= ~(1 << s_idx);
				for (_idx = 0; _idx < TAF_NUM_SCHED_SOURCES; _idx++) {

					if (scb_taf->info.linkstate[_idx][tid] !=
							TAF_LINKSTATE_NONE) {
						allfree = FALSE;
						break;
					}
				}
				WL_TAFM2(method, "%s for %s "MACF" tid %u%s\n",
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
				taf_ias_unified_reset(method);
			}
			break;

		case TAF_LINKSTATE_SOFT_RESET:
			if (ias_sched) {
				taf_ias_sched_reset(method, scb_taf, tid, s_idx);
			}
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
		if (set && !scb_taf->info.scb_stats.ias.data.use[s_idx]) {
			/* flush rspec in order to calculate rate info again */
			taf_ias_flush_rspec(scb_taf);
		}
		scb_taf->info.scb_stats.ias.data.use[s_idx] = set;
		if (!set) {
			scb_taf->info.traffic.map[s_idx] = 0;
		}

		if (TAF_SOURCE_IS_AMPDU(s_idx) && set) {
			wlc_taf_info_t* taf_info = method->taf_info;
			void ** source_h = taf_info->source_handle_p[s_idx];
			void * scb_h = taf_info->funcs[s_idx].scb_h_fn(*source_h, scb_taf->scb);
			scb_taf->info.max_pdu = wlc_ampdu_get_taf_max_pdu(scb_h);
		} else {
			scb_taf->info.max_pdu = 0;
		}
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

			if (ias_sched && (ias_sched->used & (1 << s_idx)) && pktqlen_fn) {
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
	WL_TAFM1(method, MACF" - %u\n", TAF_ETHERS(scb), pending);
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

	WL_TAFT1(method->taf_info, MACF" PS mode O%s (pkt_pull_map 0x%02x%s) elapsed %u\n",
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
	WL_TAFM1(method, MACF" PS mode O%s\n", TAF_ETHERC(scb_taf),
		scb_taf->info.ps_mode ? "N" : "FF");
#endif /* TAF_ENABLE_SQS_PULL */
#if TAF_ENABLE_TIMER
	if (!scb_taf->info.ps_mode) {
		WL_TAFM2(method, "prevent agg hold due to power save exit\n");
		taf_ias_agghold_prevent(TAF_IAS_GROUP_INFO(method->taf_info));
	}
#endif // endif
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
			WL_TAFM2(method, MACF" SCB INIT\n", TAF_ETHERS(scb));
			taf_ias_upd_ts2_scb_flag(scb_taf);
			scb_taf->timestamp[TAF_TYPE_DL] = taf_timestamp(TAF_WLCM(method));
#if TAF_ENABLE_UL
			scb_taf->timestamp[TAF_TYPE_UL] = scb_taf->timestamp[TAF_TYPE_DL];
#endif // endif
			break;

		case TAF_SCBSTATE_EXIT:
			WL_TAFM2(method, MACF" SCB EXIT\n", TAF_ETHERS(scb));
			break;

		case TAF_SCBSTATE_RESET:
			if (scb_taf) {
				taf_list_scoring_order_t order = method->ordering;

				taf_ias_flush_rspec(scb_taf);

				for (tid = 0; tid < TAF_MAXPRIO; tid++) {
					taf_ias_sched_data_t* ias_sched =
						TAF_IAS_TID_STATE(scb_taf, tid);
#if TAF_ENABLE_SQS_PULL
					scb_taf->info.pkt_pull_request[tid] = 0;
#endif /* TAF_ENABLE_SQS_PULL */
					for (s_idx = TAF_FIRST_REAL_SOURCE;
						s_idx < TAF_NUM_SCHED_SOURCES; s_idx++) {

						scb_taf->info.traffic.map[s_idx] = 0;
						scb_taf->info.traffic.count[s_idx][tid] = 0;
					}
					if (ias_sched) {
						ias_sched->released_units_limit[TAF_TYPE_DL] = 0;
#if TAF_ENABLE_UL
						ias_sched->released_units_limit[TAF_TYPE_UL] = 0;
#endif // endif
					}
				}
				scb_taf->info.traffic.available[TAF_TYPE_DL] = 0;
				scb_taf->timestamp[TAF_TYPE_DL] = TAF_IAS_NOWTIME(method);
#if TAF_ENABLE_UL
				scb_taf->info.traffic.available[TAF_TYPE_UL] = 0;
				scb_taf->timestamp[TAF_TYPE_UL] = scb_taf->timestamp[TAF_TYPE_DL];
#endif // endif
				scb_taf->info.ps_mode = SCB_PS(scb);
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
				scb_taf->info.pkt_pull_dequeue = 0;
#endif /* TAF_ENABLE_SQS_PULL */
#ifdef TAF_DBG
				memset(&scb_taf->info.scb_stats.ias.debug, 0,
				       sizeof(scb_taf->info.scb_stats.ias.debug));
#endif // endif
				WL_TAFM1(method, MACF" SCB RESET\n", TAF_ETHERS(scb));
			}
			break;

		case TAF_SCBSTATE_SOURCE_ENABLE:
		case TAF_SCBSTATE_SOURCE_DISABLE:
			s_idx = wlc_taf_get_source_index(data);
			if (s_idx != TAF_SOURCE_UNDEFINED) {
				active = (state != TAF_SCBSTATE_SOURCE_DISABLE) ? TRUE : FALSE;
				status = taf_ias_set_source(method, scb_taf, s_idx, active);
				WL_TAFM1(method, MACF" %s %s\n", TAF_ETHERS(scb),
					TAF_SOURCE_NAME(s_idx), active ? "ON" : "OFF");
			}
			break;

		case TAF_SCBSTATE_SOURCE_UPDATE:
			s_idx = wlc_taf_get_source_index(data);
			active = scb_taf->info.scb_stats.ias.data.use[s_idx];
			status = taf_ias_set_source(method, scb_taf, s_idx, active);
			WL_TAFM2(method, MACF" %s update\n", TAF_ETHERS(scb),
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
						TAF_ASSERT(taf_ul_enabled(method->taf_info));
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
				scb_taf->info.mu_tech_en[idx] =
					(taf_traffic_map_t) ((data) ? ~0 : 0);

				if (scb_taf->info.mu_tech_en[idx]) {
					scb_taf->info.tech_enable_mask |= (1 << idx);
				} else {
					scb_taf->info.tech_enable_mask &= ~(1 << idx);
				}

				WL_TAFM1(method, MACF" set %s to %sable\n", TAF_ETHERS(scb),
					TAF_TECH_NAME(idx),
					scb_taf->info.mu_tech_en[idx] ? "en" : "dis");
			}
#endif /* TAF_ENABLE_MU_TX */
			break;

		case TAF_SCBSTATE_UPDATE_BSSCFG:
		case TAF_SCBSTATE_DWDS:
		case TAF_SCBSTATE_WDS:
		case TAF_SCBSTATE_OFF_CHANNEL:
		case TAF_SCBSTATE_DATA_BLOCK_OTHER:
			WL_TAFM4(method, MACF" TO DO - %s (%u)\n", TAF_ETHERS(scb),
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

	WL_TAFM3(method, "0x%x\n", rspec);

	while (list) {
		taf_scb_cubby_t *scb_taf = list->scb_taf;

		taf_ias_flush_rspec(scb_taf);
		list = list->next;
	}
	return BCME_OK;
}

static INLINE bool
taf_ias_handle_star(taf_method_info_t* method, taf_scb_cubby_t* scb_taf, int tid, uint32 pkttag,
	uint8 index, taf_list_type_t type)
{
	wlc_taf_info_t* taf_info = method->taf_info;
	taf_ias_uni_state_t* uni_state = taf_get_uni_state(taf_info);
	taf_ias_group_info_t* ias_info = TAF_IAS_GROUP_INFO(method->taf_info);
	bool prev_complete = TRUE;
	int8 last_sched = -1;
	uint32 pkttag_units;
	uint32 now_time = 0;

	if (type == TAF_TYPE_DL) {
		uni_state->cumul_units[index] += (pkttag_units = TAF_PKTTAG_TO_UNITS(pkttag));
	}
#if TAF_ENABLE_UL
	else {
		uni_state->cumul_units[index] += (pkttag_units = pkttag);
	}
#endif /* TAF_ENABLE_UL */

	if (uni_state->total_units_in_transit >= pkttag_units) {
		uni_state->total_units_in_transit -= pkttag_units;
	} else {
		WL_ERROR(("wl%u %s: (%x) overflow (%u) %u/%u/%u/%u\n",
			WLCWLUNIT(TAF_WLCT(taf_info)), __FUNCTION__, taf_info->scheduler_index,
			index, pkttag_units, uni_state->cumul_units[index],
			uni_state->total_sched_units[index], uni_state->total_units_in_transit));
#ifdef TAF_DEBUG_VERBOSE
		taf_memtrace_dump(taf_info);
#endif // endif
		uni_state->total_units_in_transit  = 0;
	}

	if (method->ias->barrier && (last_sched = uni_state->prev_resched_index) >= 0) {
		last_sched -= (method->ias->barrier - 1);
		if (last_sched < 0) {
			last_sched += TAF_MAX_NUM_WINDOWS;
		}
		if (uni_state->cumul_units[last_sched] < uni_state->total_sched_units[last_sched]) {
			prev_complete = FALSE;
		}
	}

#ifdef TAF_DEBUG_VERBOSE
	if (uni_state->cumul_units[uni_state->resched_index[0]] >= uni_state->resched_units[0]) {

		if (uni_state->waiting_schedule) {
			if (ias_info->debug.wait_start == 0) {
				WL_TAFT2(taf_info, "waiting for super scheduler\n");
				ias_info->debug.wait_start =
					taf_nowtime(TAF_WLCT(taf_info), &now_time);
			}
		} else {
			if (ias_info->debug.wait_start != 0) {
				uint32 elapsed = taf_nowtime(TAF_WLCT(taf_info), &now_time) -
					ias_info->debug.wait_start;

				ias_info->debug.wait_start = 0;
				WL_TAFT2(taf_info, "supersched wait time %u us\n", elapsed);
			}
			if (!prev_complete && ias_info->debug.barrier_start == 0) {
				ias_info->debug.barrier_start =
					taf_nowtime(TAF_WLCT(taf_info), &now_time);
				WL_TAFT2(taf_info, "waiting barrier (prev window %d)\n",
					last_sched);
			}
			if (prev_complete && ias_info->debug.barrier_start != 0) {
				uint32 elapsed = taf_nowtime(TAF_WLCT(taf_info), &now_time) -
					ias_info->debug.barrier_start;

				ias_info->debug.barrier_start = 0;

				WL_TAFT2(taf_info, "barrier total elapsed (waiting window %d) "
					"%u us\n", last_sched, elapsed);
			}
		}
	}
#endif /* TAF_DEBUG_VERBOSE */

	if (uni_state->cumul_units[uni_state->resched_index[0]] >= uni_state->resched_units[0] &&
		prev_complete && !uni_state->waiting_schedule) {

		if (uni_state->star_packet_received) {
			return TRUE;
		}
		uni_state->star_packet_received = TRUE;

		*method->ready_to_schedule = ~(0);

		WL_TAFT1(taf_info, "cumulative %d/%d  reschedule=%d, complete\n\n",
			uni_state->resched_index[0],
			TAF_UNITS_TO_MICROSEC(uni_state->cumul_units[uni_state->resched_index[0]]),
			TAF_UNITS_TO_MICROSEC(uni_state->resched_units[0]));

		uni_state->prev_resched_index = uni_state->resched_index[0];

		if (uni_state->cycle_next == 1 && uni_state->cycle_now == 0) {
			if (uni_state->cycle_ready) {
				uni_state->resched_index[0]  = uni_state->resched_index[1];
				uni_state->resched_units[0]  = uni_state->resched_units[1];
				uni_state->star_packet_received = FALSE;
				uni_state->cycle_now = 1;
				uni_state->waiting_schedule = 1;
			} else  {
				WL_TAFT2(taf_info, "super sched not ready\n");
			}
		} else if (uni_state->cycle_next == 0 && uni_state->cycle_now == 1) {
			uni_state->cycle_now = 0;
		} else {
			uni_state->resched_index[0] = ias_info->tag_star_index;
			uni_state->cycle_now = 0;
			uni_state->cycle_next = 0;
		}
		if (taf_info->super_active) {
			WL_TAFT1(taf_info, "super sched: wait %u/%u, cycle now %u; cycle next "
				"%u, prev index %u, ready %u, wait sched %u\n\n",
				uni_state->resched_index[0],
				TAF_UNITS_TO_MICROSEC(uni_state->resched_units[0]),
				uni_state->cycle_now, uni_state->cycle_next,
				uni_state->prev_resched_index, uni_state->cycle_ready,
				uni_state->waiting_schedule);
		}
	}

	if (now_time != 0) {
		TAF_IAS_NOWTIME_SYNC(method, now_time);
	}

	return TRUE;
}

static INLINE bool taf_ias_pkt_lost(taf_method_info_t* method, taf_scb_cubby_t* scb_taf,
	int tid, void * p, taf_txpkt_state_t status, taf_list_type_t type)
{
	taf_ias_uni_state_t* uni_state = taf_get_uni_state(method->taf_info);
	uint16 pkttag_index = TAF_PKT_TAG_INDEX(p, type);
	uint16 pkttag_units = TAF_PKT_TAG_UNITS(p, type);
	uint32 taf_pkt_time_units = TAF_PKTTAG_TO_UNITS(pkttag_units);

	BCM_REFERENCE(uni_state);
	WL_TAFT3(method->taf_info, MACF"/%u, %u - %u/%u (%u) %s (tuit %u)\n", TAF_ETHERC(scb_taf),
		tid, (uint32)TAF_PKTTAG_TO_MICROSEC(pkttag_units),
		pkttag_index, TAF_UNITS_TO_MICROSEC(uni_state->cumul_units[pkttag_index]),
		uni_state->resched_index[0], taf_txpkt_status_text[status],
		uni_state->total_units_in_transit);

	switch (method->type) {
		case TAF_EBOS:
			break;

		case TAF_PSEUDO_RR:
			if (scb_taf->score[type] > 0) {
				scb_taf->score[type]--;
			}
			break;

		case TAF_ATOS:
		case TAF_ATOS2:
		{
			uint32 taf_pkt_time_score = taf_ias_units_to_score(method, scb_taf, tid,
				scb_taf->info.tech_type[type][tid], taf_pkt_time_units);

			if (scb_taf->score[type] > taf_pkt_time_score) {
				scb_taf->score[type] -= taf_pkt_time_score;
			} else {
				scb_taf->score[type] = 0;
			}
		}
			break;

		default:
			TAF_ASSERT(0);
	}
	return taf_ias_handle_star(method, scb_taf, tid, pkttag_units, pkttag_index, type);
}

bool taf_ias_tx_status(void * handle, taf_scb_cubby_t* scb_taf, int tid, void * p,
	taf_txpkt_state_t status)
{
	taf_method_info_t* method = handle;
	bool ret = FALSE;
	taf_list_type_t type = TAF_TYPE_DL;

	switch (status) {

		case TAF_TXPKT_STATUS_UPDATE_RETRY_COUNT:
		case TAF_TXPKT_STATUS_UPDATE_PACKET_COUNT:
		case TAF_TXPKT_STATUS_NONE:
		case TAF_TXPKT_STATUS_UPDATE_RATE:
		case TAF_TXPKT_STATUS_SUPPRESSED_QUEUED:
			ret = TRUE;
			break;

		case TAF_TXPKT_STATUS_PKTFREE_DROP:
			/* fall through */
		case TAF_TXPKT_STATUS_SUPPRESSED_FREE:
		case TAF_TXPKT_STATUS_SUPPRESSED:
			if (TAF_IS_TAGGED(WLPKTTAG(p)) &&
					TAF_PKT_TAG_UNITS(p, TAF_TYPE_DL) <= TAF_PKTTAG_RESERVED) {

				ret = taf_ias_pkt_lost(method, scb_taf, tid, p,
					status, TAF_TYPE_DL);
				WLPKTTAG(p)->pktinfo.taf.ias.units = TAF_PKTTAG_PROCESSED;
			}
			break;

#if TAF_ENABLE_UL
			case TAF_TXPKT_STATUS_UL_SUPPRESSED:
				if (TAF_IS_TAGGED(WLULPKTTAG(p))) {

					ret = taf_ias_pkt_lost(method, scb_taf, tid,
						p, status, TAF_TYPE_UL);
					WLULPKTTAG(p)->units = TAF_PKTTAG_PROCESSED;
				}
				break;
#endif /* TAF_ENABLE_UL */

		case TAF_TXPKT_STATUS_REGMPDU:
		case TAF_TXPKT_STATUS_PKTFREE:
			if (TAF_IS_TAGGED(WLPKTTAG(p)) &&
					TAF_PKT_TAG_UNITS(p, TAF_TYPE_DL) <= TAF_PKTTAG_RESERVED) {

				ret = taf_ias_handle_star(method, scb_taf, tid,
					TAF_PKT_TAG_UNITS(p, TAF_TYPE_DL),
					TAF_PKT_TAG_INDEX(p, TAF_TYPE_DL), TAF_TYPE_DL);
				WLPKTTAG(p)->pktinfo.taf.ias.units = TAF_PKTTAG_PROCESSED;
			}
			break;
#if TAF_ENABLE_UL
		case TAF_TXPKT_STATUS_TRIGGER_COMPLETE:
			type = TAF_TYPE_UL;
			if (TAF_IS_TAGGED(WLULPKTTAG(p))) {

				WL_TAFT3(method->taf_info, MACF"%s\n",
					TAF_ETHERC(scb_taf), TAF_TYPE(TAF_TYPE_UL));
				ret = taf_ias_handle_star(method, scb_taf, tid,
					TAF_PKT_TAG_UNITS(p, TAF_TYPE_UL),
					TAF_PKT_TAG_INDEX(p, TAF_TYPE_UL), TAF_TYPE_UL);
				WLULPKTTAG(p)->units = TAF_PKTTAG_PROCESSED;
			}
			break;
#endif /* TAF_ENABLE_UL */
		default:
			break;
	}
	if (!ret &&
		(((type == TAF_TYPE_DL) && TAF_IS_TAGGED(WLPKTTAG(p)) &&
			(TAF_PKT_TAG_UNITS(p, TAF_TYPE_DL) == TAF_PKTTAG_PROCESSED)) ||
#if TAF_ENABLE_UL
			((type == TAF_TYPE_UL) && TAF_IS_TAGGED(WLULPKTTAG(p)) &&
			(TAF_PKT_TAG_UNITS(p, TAF_TYPE_UL) == TAF_PKTTAG_PROCESSED)))) {
#else
			FALSE)) {
#endif /* TAF_ENABLE_UL */
		WL_TAFT3(method->taf_info, MACF"%s tid %u, pkt already processed\n",
			TAF_ETHERC(scb_taf), TAF_TYPE(type), tid);
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
			method->ias->data_block = count ? TRUE : FALSE;
			WL_TAFM1(method, "data block %u\n", method->ias->data_block);
			break;

		case TAF_SCHED_STATE_RESET:
			WL_TAFM1(method, "RESET\n");
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
			WL_TAFM1(method, "data block %u\n", method->ias->data_block);
			break;

		case TAF_SCHED_STATE_REWIND:
			if (scb_taf) {
				WL_TAFM2(method, MACF" tid %u rewind %u pkts\n",
					TAF_ETHERC(scb_taf), tid, count);
			}
			break;

		case TAF_SCHED_STATE_RESET:
			WL_TAFM1(method, "RESET\n");
			taf_ias_clean_all_rspecs(method);
			*method->ready_to_schedule = ~0;
			taf_ias_unified_reset(method);
			break;

		default:
			break;
	}
}

int
taf_ias_iovar(void *handle, taf_scb_cubby_t * scb_taf, const char* cmd, wl_taf_define_t* result,
	struct bcmstrbuf* b)
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
	if (scb_taf) {
		WL_TAFM1(method, MACF" %s\n", TAF_ETHERC(scb_taf), cmd);
	} else {
		WL_TAFM1(method, "%s\n", cmd);
	}

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
#if TAF_ENABLE_TIMER
	if (!strcmp(cmd, "agg_hold")) {
		taf_ias_group_info_t* ias_info = TAF_IAS_GROUP_INFO(method->taf_info);
		uint32 agg_hold = ias_info->agg_hold;
		int err = wlc_taf_param(&local_cmd, &agg_hold, 0, 15, b);
		if (err == TAF_IOVAR_OK_SET) {
			ias_info->agg_hold = agg_hold;
		}
		result->misc = agg_hold;
		return err;
	}
	if (!strcmp(cmd, "agg_hold_threshold")) {
		taf_ias_group_info_t* ias_info = TAF_IAS_GROUP_INFO(method->taf_info);
		int err = wlc_taf_param(&local_cmd, &ias_info->agg_hold_threshold, 0,
			method->ias->high >= 800 ?
			TAF_MICROSEC_TO_UNITS(method->ias->high - 800) : 0, b);
		result->misc = ias_info->agg_hold_threshold;
		return err;
	}
#endif /* TAF_ENABLE_TIMER */
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
		uint32 immed = ias_info->immed;
		int err = wlc_taf_param(&local_cmd, &immed, 0, 0xFFFFF, b);
		if (err == TAF_IOVAR_OK_SET) {
			ias_info->immed = immed;
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
#if TAF_ENABLE_MU_BOOST
	if (!strcmp(cmd, "mu_boost_limit")) {
		uint32 boost_limit = method->ias->mu_boost_limit ?
			(1024 * 1024) / method->ias->mu_boost_limit : 1024;

		int err = wlc_taf_param(&local_cmd, &boost_limit, 1024, 32 * 1024, b);

		TAF_ASSERT(boost_limit > 0);
		result->misc = boost_limit;
		method->ias->mu_boost_limit = (1024 * 1024) / boost_limit;
		bcm_bprintf(b, "reciprocal boost limit %u\n", method->ias->mu_boost_limit);
		return err;
	}
	if (!strcmp(cmd, "mu_boost_compensate")) {
		int err = wlc_taf_param(&local_cmd, &method->ias->mu_boost_compensate, 64, 4096, b);

		result->misc = method->ias->mu_boost_compensate;
		return err;
	}
#endif /* TAF_ENABLE_MU_BOOST */
	if (!strcmp(cmd, "opt")) {
		int err = wlc_taf_param(&local_cmd, &method->ias->options, 0, (uint32)(-1), b);

		result->misc = method->ias->options;
		return err;
	}
	if (!strcmp(cmd, "ias_opt")) {
		taf_ias_group_info_t* ias_info = TAF_IAS_GROUP_INFO(method->taf_info);
		int err = wlc_taf_param(&local_cmd, &ias_info->options, 0, (uint32)(-1), b);

		result->misc = ias_info->options;
		return err;
	}
	if (!strcmp(cmd, "aggp")) {
		int err = wlc_taf_param(&local_cmd, &method->ias->opt_aggp_limit, 0, 63, b);
		result->misc = method->ias->opt_aggp_limit;
		return err;
	}
	if (!strcmp(cmd, "barrier")) {
		int err = wlc_taf_param(&local_cmd, &method->ias->barrier, 0,
			(TAF_MAX_NUM_WINDOWS - 2), b);
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

	if (!strcmp(cmd, "reset")) {
		taf_ias_unified_reset(method);
		return BCME_OK;
	}
	return status;
}

#ifdef TAF_PKTQ_LOG
static bool taf_ias_dpstats_counters(taf_scb_cubby_t* scb_taf,
	mac_log_counters_v06_t* mac_log, int tid, bool clear)
{
	taf_log_counters_v06_t*  taflog;
	taf_ias_sched_data_t* ias_sched = scb_taf ? TAF_IAS_TID_STATE(scb_taf, tid) : NULL;
	taf_ias_dpstats_counters_t* counters = ias_sched ?
		ias_sched->rel_stats.dpstats_log : NULL;

	if (mac_log == NULL || (counters == NULL && ias_sched == NULL)) {
		return FALSE;
	}
	if (counters == NULL) {
		counters = (taf_ias_dpstats_counters_t*)MALLOCZ(TAF_WLCM(scb_taf->method)->pub->osh,
			sizeof(*counters));

		ias_sched->rel_stats.dpstats_log = counters;
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
				ias_sched->rel_stats.dpstats_log : NULL;

			if (counters) {
				MFREE(TAF_WLCM(method)->pub->osh, counters,
					sizeof(*counters));
				ias_sched->rel_stats.dpstats_log  = NULL;
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
