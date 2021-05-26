/*
 * wlc_taf_oriv_ias.h
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
#ifndef _wlc_taf_priv_ias_h_
#define _wlc_taf_priv_ias_h_

#if TAF_ENABLE_TIMER
#include <wl_export.h>
#endif // endif

#define TAF_COEFF_IAS_DEF	4032
#define TAF_COEFF_IAS_FACTOR	12 /* (1<<12) is 4096, approx 4ms */
#define TAF_COEFF_IAS_MAX_BITS  12
#define TAF_COEFF_IAS_MAX	(1 << TAF_COEFF_IAS_MAX_BITS) /* coefficient for normalisation */
#define TAF_COEFF_IAS_MAX_CFG	(((1 << TAF_COEFF_IAS_MAX_BITS) * 255) / 256)
#define TAF_SCORE_IAS_MAX	(TAF_SCORE_MAX / TAF_COEFF_IAS_MAX) /* max val prior norm */
#define TAF_PKTCOUNT_SCALE_SHIFT	5

#define TAF_TIME_HIGH_MAX	16000
#define TAF_TIME_LOW_MAX	16000
#define TAF_TIME_HIGH_DEFAULT	TAF_MICROSEC_MAX
#define TAF_TIME_LOW_DEFAULT	(TAF_TIME_HIGH_DEFAULT - 1500)
#define TAF_TIME_ATOS_HIGH_DEFAULT	TAF_TIME_HIGH_DEFAULT
#define TAF_TIME_ATOS_LOW_DEFAULT	TAF_TIME_LOW_DEFAULT
#define TAF_TIME_ATOS2_HIGH_DEFAULT	(TAF_TIME_ATOS_HIGH_DEFAULT + 400)
#define TAF_TIME_ATOS2_LOW_DEFAULT	TAF_TIME_LOW_DEFAULT

/* to save computational effort, assume fix durations - packet overhead times in us */
#define TAF_NAR_OVERHEAD           330 /* biased a little high for adjusted 'fairness' */
#define TAF_AMPDU_NO_RTS_OVERHEAD  180
#define TAF_AMPDU_RTS_OVERHEAD     260
#define TAF_RTS_OVERHEAD           (TAF_AMPDU_RTS_OVERHEAD - TAF_AMPDU_NO_RTS_OVERHEAD)

#define TAF_IAS_RETRIG_TIME_NORM   505
#define TAF_IAS_RETRIG_TIME_UL     15

#define TAF_MU_PAIR_BREAK_FRAC16   2

#define TAF_OPT_EXIT_EARLY         0
#define TAF_OPT_PKT_NAR_OVERHEAD   5
#define TAF_OPT_PKT_AMPDU_OVERHEAD 6

#define TAF_OPT(m, f)              ((ias_cfg(m).options & (1 << TAF_OPT_##f)) != 0)

#define TAF_OPT_IAS_IMMED_LOOSE    0
#define TAF_OPT_IAS_TOOMUCH        1
#define TAF_OPT_IAS_TOOMUCH_CUMUL  2
#define TAF_OPT_IAS_TOOMUCH_LOOSE  3
#define TAF_OPT_IAS_SFAIL_LOOSE    4
#define TAF_OPT_IAS_AUTOBW20_SUPER 5
#define TAF_OPT_IAS_AUTOBW40_SUPER 6
#define TAF_OPT_IAS_RETRIGGER      8
#define TAF_IAS_OPT(i, f)          (((i)->options & (1 << TAF_OPT_IAS_##f)) != 0)

#if TAF_ENABLE_MU_TX
#define TAF_OPT_MIMO_MEAN          0
#define TAF_OPT_MIMO_MIN           1
#define TAF_OPT_MIMO_MEDIAN        2
#define TAF_OPT_MIMO_MAX           3
#define TAF_OPT_MIMO_PAIR          4
#define TAF_OPT_MIMO_SWEEP         5
#define TAF_OPT_MIMO_AGG_HOLD      6
#define TAF_OPT_MIMO_INT_EXTEND    7
#define TAF_OPT_MIMO_PAIR_MAXMU    8
#define TAF_OPT_MIMO_MAX_STREAMS   9
#define TAF_MIMO_OPT(m, f)         ((ias_cfg(m).mu_mimo_opt & (1 << TAF_OPT_MIMO_##f)) != 0)

#define TAF_OPT_OFDMA_PAIR         4
#define TAF_OPT_OFDMA_SWEEP        5
#define TAF_OPT_OFDMA_AGG_HOLD     6
#define TAF_OPT_OFDMA_INT_EXTEND   7
#define TAF_OPT_OFDMA_TOO_SMALL    8
#define TAF_OPT_OFDMA_TOO_LARGE    9
#define TAF_OPT_OFDMA_MAXN_REL    10

#define TAF_OFDMA_OPT(m, f)        ((ias_cfg(m).mu_ofdma_opt & (1 << TAF_OPT_OFDMA_##f)) != 0)
#endif /* TAF_ENABLE_MU_TX */

#define NUM_IAS_SCHEDULERS         (LAST_IAS_SCHEDULER - FIRST_IAS_SCHEDULER + 1)
#define IAS_INDEX(type)            ((type) - FIRST_IAS_SCHEDULER)
#define IAS_INDEXM(M)              IAS_INDEX((M)->type)

#define TAF_STATE_HIGHER_PEND      0

#if TAF_ENABLE_UL
#define TAF_PKT_TAG_INDEX(p, type) \
	((type == TAF_TYPE_DL) ? WLPKTTAG(p)->pktinfo.taf.ias.index : WLULPKTTAG(p)->index)
#define TAF_PKT_TAG_UNITS(p, type) \
	((type == TAF_TYPE_DL) ? WLPKTTAG(p)->pktinfo.taf.ias.units : WLULPKTTAG(p)->units)
#else
#define TAF_PKT_TAG_INDEX(p, type)	(WLPKTTAG(p)->pktinfo.taf.ias.index)
#define TAF_PKT_TAG_UNITS(p, type)	(WLPKTTAG(p)->pktinfo.taf.ias.units)
#endif /* TAF_ENABLE_UL */

typedef enum {
	TAF_TRIGGER_NONE,
	TAF_TRIGGER_IMMEDIATE,
	TAF_TRIGGER_STAR_THRESHOLD
} taf_trigger_cause_t;

typedef struct {
	struct {
		uint32 released_units;
		uint32 released_bytes;
	} real;
	struct {
		uint32 released_units;
	} total;
	struct {
		uint32 window_dur_units;
		uint32 rounding_units;
	} extend;
	uint32 total_units_in_transit;
	uint32 cumul_units[TAF_MAX_NUM_WINDOWS];
	uint32 total_sched_units[TAF_MAX_NUM_WINDOWS];
	uint32 resched_units[2];
	uint16 high[NUM_IAS_SCHEDULERS];
	uint16 low[NUM_IAS_SCHEDULERS];
	uint16 release_type_present;
	uint16 bw_type_present;
	uint8  resched_index[2];
	int8   prev_resched_index;
	uint8  cycle_now;
	uint8  cycle_next;
	uint8  cycle_ready;
	uint8  was_reset;
	uint8  star_packet_received;
	uint8  barrier_req;
	uint8  waiting_schedule;
	taf_trigger_cause_t trigger;
	uint32 est_release_units;
	uint32 super_sched_last;
} taf_ias_uni_state_t;

#define TAF_IAS_RESCHED_UNITS(ts)  ((ts)->resched_units[(ts)->cycle_next])
#define TAF_IAS_RESCHED_IDX(ts)    ((ts)->resched_index[(ts)->cycle_next])
#define TAF_IAS_EXTEND(ts)         ((ts)->extend.window_dur_units + (ts)->extend.rounding_units)

#ifdef TAF_PKTQ_LOG
typedef struct {
	uint32  time_lo;
	uint32  time_hi;
	uint32  emptied;
	uint32  max_did_rel_delta;
	uint32  forced;
	uint32  ready;
	uint32  pwr_save;
	uint32  did_release;
	uint32  restricted;
	uint32  limited;
	uint32  held_z;
	uint32  release_pcount;
	uint32  release_ncount;
	uint32  release_pkt_size_mean;
	uint32  release_frcount;
	uint64  release_time;
	uint64  release_bytes;
	uint32  did_rel_delta;
	uint32  did_rel_timestamp;
} taf_ias_dpstats_counters_t;

#define TAF_DPSTATS_LOG_ADD(stats, var, val)	do { if ((stats)->dpstats_log) { \
							(stats)->dpstats_log->var += (val); \
						} } while (0)

#define TAF_DPSTATS_LOG_SET(stats, var, val)	do { if ((stats)->dpstats_log) { \
							(stats)->dpstats_log->var = (val); \
						} } while (0)

#else
#define TAF_DPSTATS_LOG_ADD(stats, var, val)	do {} while (0)
#define TAF_DPSTATS_LOG_SET(stats, var, val)	do {} while (0)
#endif /* TAF_PKTQ_LOG */

typedef struct {
	uint32 index[TAF_NUM_LIST_TYPES];
	/* the following is used for tracking average packet size */
	struct {
		uint16  release_pkt_size_mean;
		uint16  release_pcount;
		uint16  release_ncount;
		uint32  release_bytes;
		uint32  release_units;
		uint32  total_scaled_ncount;
		uint32  total_scaled_pcount;
		uint32  total_bytes;
		uint32  calc_timestamp;
	} data;
#ifdef TAF_PKTQ_LOG
	taf_ias_dpstats_counters_t* dpstats_log;
#endif /* TAF_PKTQ_LOG */
} taf_ias_sched_rel_stats_t;

#include <packed_section_start.h>
typedef struct BWL_PRE_PACKED_STRUCT taf_ias_sched_data {
	taf_sched_handle_t handle[TAF_NUM_SCHED_SOURCES];
	taf_ias_sched_rel_stats_t rel_stats;
	int32              released_units_limit[TAF_NUM_LIST_TYPES];
	uint8              used;
	uint8              aggsf;
} BWL_POST_PACKED_STRUCT taf_ias_sched_data_t;
#include <packed_section_end.h>

#define TAF_IAS_STATIC_CONTEXT     TAF_STATIC_CONTEXT

#if TAF_IAS_STATIC_CONTEXT
/* size check */
TAF_COMPILE_ASSERT(taf_ias_sched_data_t, sizeof(taf_ias_sched_data_t) <= TAF_STATIC_CONTEXT_SIZE);
#endif // endif

typedef struct taf_ias_coeff {
	uint8              time_shift;
	uint16             coeff1;
	uint16             coeff10;
	uint16             coeff100;
} taf_ias_coeff_t;

typedef struct taf_ias_method_info {
	uint32             high;
	uint32             low;
	uint32             total_score;
	taf_ias_coeff_t    coeff;
	uint8*             score_weights;
	uint32             release_limit;
	uint32             barrier;
	uint32             opt_aggp_limit;
	bool               data_block;
	taf_traffic_map_t  tid_active;
	taf_traffic_map_t  sched_active;
	uint32             options;
#if TAF_ENABLE_MU_TX
	uint16*            mu_pair;
	uint16*            mu_mimo_rel_limit;
	uint32             mu_mimo_opt;
	uint32             mu_ofdma_opt;
#endif /* TAF_ENABLE_MU_TX */
#if TAF_ENABLE_MU_BOOST
	uint32             mu_boost_limit;
	uint32             mu_boost_compensate;
#endif /* TAF_ENABLE_MU_BOOST */
#if TAF_ENABLE_SQS_PULL
	uint16             pre_rel_limit[TAF_NUM_SCHED_SOURCES];
	uint32             margin;
#endif /* TAF_ENABLE_SQS_PULL */
} taf_ias_method_info_t;

/* taf_ias_info is GLOBAL shared across all different EBOS/PRR/ATOS/ATOS2 */
typedef struct taf_ias_group_info {
	taf_ias_uni_state_t unified_state;
	/* ias_ready_to_schedule is here so all EBOS/ATOS etc schedulers can peek status */
	taf_traffic_map_t  ias_ready_to_schedule[NUM_IAS_SCHEDULERS];
#if TAF_ENABLE_TIMER
	struct wl_timer *  agg_hold_timer;
	uint32             agg_hold_start_time;
	uint32             agg_hold_threshold;
	bool               is_agg_hold_timer_running;
	bool               agg_hold_exit_pending;
	bool               agg_hold_expired;
	bool               agg_hold_prevent;
	bool               stall_prevent_timer;
	uint8              agg_hold;
#endif /* TAF_ENABLE_TIMER */
	uint8              prev_tag_star_index; /* used to separate scheduler periods */
	uint8              tag_star_index;     /* used to separate scheduler periods */
	uint32             immed;
	uint32             options;
	uint32             now_time;
	uint32             cpu_time;
	uint32             cpu_elapsed_time;
#ifdef TAF_DBG
	uint32             data_block_start;
	uint32             data_block_total;
	uint32             data_block_prev_in_transit;
#endif // endif
	uint32             ncount_flow_last;
	uint32             ncount_flow;
	uint32             sched_start;
	struct {
		uint32 uflow_low;
		uint32 uflow_high;
		uint32 target_high;
		uint32 target_low;
		uint32 immed_trig;
		uint32 cumulative_immed_trig;
		uint32 immed_star;
		uint32 prev_release_time;
		uint32 time_delta;
		uint32 super_sched;
		uint32 super_sched_collapse;
	} data;
	struct {
		uint32 uflow_low;
		uint32 uflow_high;
		uint32 target_high;
		uint32 target_low;
		uint32 nothing_released;
		uint32 immed_trig;
		uint32 immed_star;
		uint32 max_cumulative_immed_trig;
		uint32 average_high;
		uint32 sched_duration;
		uint32 max_duration;
		uint32 super_sched;
		uint32 super_sched_collapse;
		uint32 barrier_req;
		uint32 traffic_stat_overflow;
#if TAF_ENABLE_TIMER
		uint32 agg_hold_count;
		uint32 agg_hold_max_time;
		uint32 agg_hold_total_time;
#endif // endif
#ifdef TAF_DEBUG_VERBOSE
		uint32 barrier_start;
		uint32 wait_start;
#endif // endif
	} debug;
} taf_ias_group_info_t;

typedef struct {
	taf_method_info_t       method;
	taf_ias_method_info_t   ias;
} taf_ias_container_t;

#define TAF_IAS_GROUP_INFO(_t) \
			((taf_ias_group_info_t*)((_t)->group_context[TAF_SCHEDULER_IAS_METHOD]))

#define TAF_IAS_METHOD_GROUP_INFO(_m)   (taf_ias_group_info_t*)TAF_METHOD_GROUP_INFO(_m)

#define TAF_IAS_TID_STATE(scb_taf, tid) (taf_ias_sched_data_t*)(TAF_CUBBY_TIDINFO(scb_taf, tid))

#define TAF_IAS_TID_STATS(_s_t, _t)  &((TAF_IAS_TID_STATE(_s_t, _t))->rel_stats)

#define TAF_IAS_NOWTIME(_m)             *((_m)->now_time_p)

#define TAF_IAS_NOWTIME_SYNC(_m, _ts)   do { \
						*((_m)->now_time_p) = (_ts); \
					} while (0)

#define taf_get_uni_state(_taf)         &(TAF_IAS_GROUP_INFO(_taf)->unified_state)

#define ias_cfg(method)                 ((taf_ias_container_t*)method)->ias

static void* BCMATTACHFN(taf_ias_method_attach)(wlc_taf_info_t *taf_info, taf_scheduler_kind type);
static int   BCMATTACHFN(taf_ias_method_detach)(void* context);

static void taf_ias_up(void* context);
static int  taf_ias_down(void* context);
static taf_list_t ** taf_ias_get_list_head_ptr(void * context);

static int  taf_ias_dump(void *handle, struct bcmstrbuf *b);
static bool taf_ias_rate_override(void* handle, ratespec_t rspec, wlcband_t *band);

static bool taf_ias_link_state(void *handle, struct scb* scb, int tid, taf_source_type_t s_idx,
	taf_link_state_t state);
static int taf_ias_scb_state(void *handle, struct scb* scb, taf_source_type_t s_idx,
	void* update, taf_scb_state_t state);

static bool taf_ias_tx_status(void * handle, taf_scb_cubby_t* scb_taf, int tid, void * p,
	taf_txpkt_state_t status);

static int taf_ias_iovar(void *handle, taf_scb_cubby_t * scb_taf, const char* cmd,
	wl_taf_define_t* result, struct bcmstrbuf* b);

static void taf_ias_sched_state(void *, taf_scb_cubby_t *, int tid, int count,
	taf_source_type_t s_idx, taf_sched_state_t);

static void taf_ias_unified_reset(taf_method_info_t* method);

#ifdef TAF_PKTQ_LOG
static uint32 taf_ias_dpstats_dump(void * handle, taf_scb_cubby_t* scb_taf,
	mac_log_counters_v06_t* mac_log, bool clear, uint32 timelo, uint32 timehi,
	uint32 prec_mask);
#endif // endif

#if TAF_ENABLE_SQS_PULL
static void taf_iasm_sched_state(void *, taf_scb_cubby_t *, int tid, int count,
	taf_source_type_t s_idx, taf_sched_state_t);
#endif /* TAF_ENABLE_SQS_PULL */

static INLINE uint8 taf_ias_achvd_aggsf(taf_scb_cubby_t * scb_taf, int tid)
{
	taf_ias_sched_rel_stats_t* scbtidstats = TAF_IAS_TID_STATS(scb_taf, tid);
	uint32 ppkts = scbtidstats->data.total_scaled_pcount;

	if (ppkts > (20 << TAF_PKTCOUNT_SCALE_SHIFT)) {
		uint32 aggsf = (scbtidstats->data.total_scaled_ncount + (ppkts >> 1)) / ppkts;

		return MAX(aggsf, 1);
	}
	return 1;
}

static INLINE uint8 BCMFASTPATH taf_ias_aggsf(taf_scb_cubby_t *scb_taf, int tid)
{
	uint8 achieved = taf_ias_achvd_aggsf(scb_taf, tid);
	uint8 limit = taf_aggsf(scb_taf, tid);
	return MIN(achieved, limit);
}

static INLINE uint32 BCMFASTPATH taf_ias_mpdus(taf_scb_cubby_t *scb_taf, int tid, uint32 len)
{
	uint8 aggsf = taf_ias_aggsf(scb_taf, tid);

	if (aggsf > 0) {
		return len / aggsf;
	}
	return 0;
}
static INLINE bool taf_ias_is_nar_traffic(taf_scb_cubby_t * scb_taf, int tid,
	taf_source_type_t s_idx)
{
#if TAF_ENABLE_NAR
	bool is_nar = TAF_SOURCE_IS_NAR(s_idx);

	if (!is_nar && !TAF_SOURCE_IS_UL(s_idx) &&
		scb_taf->info.linkstate[TAF_SOURCE_AMPDU][tid] != TAF_LINKSTATE_ACTIVE &&
		scb_taf->info.scb_stats.ias.data.use[TAF_SOURCE_NAR]) {

		is_nar = (scb_taf->info.linkstate[TAF_SOURCE_NAR][tid] == TAF_LINKSTATE_ACTIVE);
	}
	return is_nar;
#else
	return FALSE;
#endif /* TAF_ENABLE_NAR */
}

#endif /* _wlc_taf_priv_ias_h_ */
