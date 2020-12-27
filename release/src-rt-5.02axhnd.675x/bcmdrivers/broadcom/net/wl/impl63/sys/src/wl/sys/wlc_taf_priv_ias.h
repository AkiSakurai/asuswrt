/*
 * wlc_taf_ias.h
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
#ifndef _wlc_taf_priv_ias_h_
#define _wlc_taf_priv_ias_h_

#define TAF_COEFF_IAS_DEF	4032
#define TAF_COEFF_IAS_FACTOR	12 /* (1<<12) is 4096, approx 4ms */
#define TAF_COEFF_IAS_MAX_BITS  12
#define TAF_COEFF_IAS_MAX	(1 << TAF_COEFF_IAS_MAX_BITS) /* coefficient for normalisation */
#define TAF_COEFF_IAS_MAX_CFG	(((1 << TAF_COEFF_IAS_MAX_BITS) * 255) / 256)
#define TAF_SCORE_IAS_MAX	(TAF_SCORE_MAX / TAF_COEFF_IAS_MAX) /* max val prior norm */
#define TAF_COEFF_EBOS_MAX	64
#define TAF_NCOUNT_SCALE_SHIFT	5

#define TAF_TIME_HIGH_MAX	16000
#define TAF_TIME_LOW_MAX	16000
#define TAF_TIME_HIGH_DEFAULT	TAF_MICROSEC_MAX
#define TAF_TIME_LOW_DEFAULT	(TAF_TIME_HIGH_DEFAULT - 1500)
#define TAF_TIME_ATOS_HIGH_DEFAULT	TAF_TIME_HIGH_DEFAULT
#define TAF_TIME_ATOS_LOW_DEFAULT	TAF_TIME_LOW_DEFAULT
#define TAF_TIME_ATOS2_HIGH_DEFAULT	(TAF_TIME_ATOS_HIGH_DEFAULT + 400)
#define TAF_TIME_ATOS2_LOW_DEFAULT	TAF_TIME_LOW_DEFAULT

#define TAF_OPT_IMMED_LOOSE        0
#define TAF_OPT_TOOMUCH            1
#define TAF_OPT_EXIT_EARLY         2
#define TAF_OPT(m, f)              (((m)->ias->options & (1 << TAF_OPT_##f)) != 0)

#if TAF_ENABLE_MU_TX
#define TAF_OPT_MIMO_MEAN          0
#define TAF_OPT_MIMO_MIN           1
#define TAF_OPT_MIMO_MEDIAN        2
#define TAF_OPT_MIMO_MAX           3
#define TAF_OPT_MIMO_PAIR          4
#define TAF_OPT_MIMO_SWEEP         5
#define TAF_OPT_MIMO_AGG_HOLD      6
#define TAF_MIMO_OPT(m, f)         (((m)->ias->mu_mimo_opt & (1 << TAF_OPT_MIMO_##f)) != 0)

#define TAF_OPT_OFDMA_PAIR         4
#define TAF_OPT_OFDMA_SWEEP        5
#define TAF_OPT_OFDMA_AGG_HOLD     6
#define TAF_OPT_OFDMA_LOW_TRAFFIC  7
#define TAF_OPT_OFDMA_TOO_SMALL    8
#define TAF_OPT_OFDMA_TOO_LARGE    9
#define TAF_OFDMA_OPT(m, f)        (((m)->ias->mu_ofdma_opt & (1 << TAF_OPT_OFDMA_##f)) != 0)
#endif /* TAF_ENABLE_MU_TX */

#define NUM_IAS_SCHEDULERS         (LAST_IAS_SCHEDULER - FIRST_IAS_SCHEDULER + 1)
#define IAS_INDEX(type)            ((type) - FIRST_IAS_SCHEDULER)
#define IAS_INDEXM(M)              IAS_INDEX((M)->type)

typedef enum {
	TAF_TRIGGER_NONE,
	TAF_TRIGGER_IMMEDIATE,
	TAF_TRIGGER_STAR_THRESHOLD
} taf_trigger_cause_t;

#if TAF_ENABLE_MU_TX
typedef struct {
	uint32 est_release_units_tech[TAF_NUM_MU_TECH_TYPES][TAF_MAXPRIO];
	uint8  num_tech_users[TAF_NUM_MU_TECH_TYPES][TAF_MAXPRIO];
} taf_ias_mu_t;
#endif // endif

typedef struct {
	struct {
		uint32 released_units;
		uint32 released_bytes;
	} real;
	struct {
		uint32 released_units;
	} total;
	struct {
		uint32 units;
	} extend;
	uint32 total_units_in_transit;
	uint32 cumul_units[TAF_MAX_PKT_INDEX + 1];
	uint32 total_sched_units[TAF_MAX_PKT_INDEX + 1];
	uint32 resched_units[2];
	uint16 high[NUM_IAS_SCHEDULERS];
	uint16 low[NUM_IAS_SCHEDULERS];
	uint16 immed;
	int8   prev_resched_index;
	uint8  resched_index[2];
	uint8  cycle_now;
	uint8  cycle_next;
	uint8  cycle_ready;
	uint8  was_reset;
	uint8  star_packet_received;
	uint8  barrier_req;
	uint8  waiting_schedule;
	uint32 sched_start;
	struct {
		uint32 uflow_low;
		uint32 uflow_high;
		uint32 target_high;
		uint32 target_low;
		uint32 immed_trig;
		uint32 immed_star;
		uint32 prev_release_time;
		uint32 time_delta;
	} data;
	struct {
		uint8  prev_index;
		uint32 uflow_low;
		uint32 uflow_high;
		uint32 target_high;
		uint32 target_low;
		uint32 immed_trig;
		uint32 immed_star;
		uint32 average_high;
		uint32 sched_duration;
		uint32 max_duration;
#ifdef TAF_DEBUG_VERBOSE
		uint32 barrier_start;
		uint32 wait_start;
#endif // endif
	} debug;
	taf_trigger_cause_t trigger;
	uint32 est_release_units;
} taf_schedule_tid_state_t;

#define TAF_IAS_RESCHED_UNITS(ts)  ((ts)->resched_units[(ts)->cycle_next])
#define TAF_IAS_RESCHED_IDX(ts)    ((ts)->resched_index[(ts)->cycle_next])

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
	uint32 index;
	/* the following is used for tracking average packet size */
	struct {
		uint16  release_pkt_size_mean;
		uint16  release_pcount;
		uint32  release_ncount;
		uint32  release_bytes;
		uint32  release_units;
		uint32  total_scaled_ncount;
		uint32  total_bytes;
		uint32  calc_timestamp;
	} data;
#ifdef TAF_PKTQ_LOG
	taf_ias_dpstats_counters_t* dpstats_log;
#endif /* TAF_PKTQ_LOG */
} taf_ias_sched_scbtid_stats_t;

typedef struct taf_ias_sched_data {
	taf_sched_handle_t handle[TAF_NUM_SCHED_SOURCES];
	taf_ias_sched_scbtid_stats_t scbtidstats;
} taf_ias_sched_data_t;

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
	taf_ias_mu_t       mu;
	uint32             mu_mimo_opt;
	uint32             mu_ofdma_opt;
#endif /* TAF_ENABLE_MU_TX */
#if TAF_ENABLE_SQS_PULL
	uint16             pre_rel_limit[TAF_NUM_SCHED_SOURCES];
	uint32             margin;
#endif /* TAF_ENABLE_SQS_PULL */
} taf_ias_method_info_t;

/* taf_ias_info is GLOBAL shared across all different EBOS/PRR/ATOS/ATOS2 */
typedef struct taf_ias_group_info {
#if !TAF_ENABLE_SQS_PULL
	taf_schedule_tid_state_t tid_state[TAF_MAXPRIO];
#endif // endif
	taf_schedule_tid_state_t unified_tid_state;
	/* ias_ready_to_schedule is here so all EBOS/ATOS etc schedulers can peek status */
	taf_traffic_map_t  ias_ready_to_schedule[NUM_IAS_SCHEDULERS];
	uint32             tag_star_index;     /* used to separate scheduler periods */
	uint32             cpu_time;
	uint32             cpu_elapsed_time;
#ifdef TAF_DBG
	uint32             data_block_start;
	uint32             data_block_total;
	uint32             data_block_prev_in_transit;
#endif // endif
	uint32             ncount_flow_last;
	uint32             ncount_flow;
} taf_ias_group_info_t;

typedef struct {
	taf_method_info_t       method;
	taf_ias_method_info_t   ias;
} taf_ias_container_t;

#define TAF_IAS_GROUP_INFO(_t) \
			(taf_ias_group_info_t*)((_t)->group_context[TAF_SCHEDULER_IAS_METHOD])

#define TAF_IAS_METHOD_GROUP_INFO(_m)   (taf_ias_group_info_t*)TAF_METHOD_GROUP_INFO(_m)

#define TAF_IAS_TID_STATE(scb_taf, tid) (taf_ias_sched_data_t*)(TAF_CUBBY_TIDINFO(scb_taf, tid))

#define TAF_IAS_TID_STATS(_s_t, _t)  ((TAF_IAS_TID_STATE(_s_t, _t))->scbtidstats)

static INLINE
taf_schedule_tid_state_t* BCMFASTPATH taf_get_tid_state(wlc_taf_info_t* taf_info, int tid)
{
	taf_ias_group_info_t* ias_info = TAF_IAS_GROUP_INFO(taf_info);

#if TAF_ENABLE_SQS_PULL
	TAF_ASSERT(tid == TAF_DEFAULT_UNIFIED_TID);

	return &ias_info->unified_tid_state;
#else
	taf_schedule_tid_state_t* tidstate = NULL;

	if (taf_info->ordering == TAF_ORDER_TID_PARALLEL) {
		TAF_ASSERT(tid >= 0 && tid < TAF_NUM_TIDSTATE);
		/* all access classes are running in parallel, so maintain separated TID
		* context
		*/
		tidstate = &ias_info->tid_state[tid];
	} else {
		TAF_ASSERT(tid == TAF_DEFAULT_UNIFIED_TID);
		/* the scheduling is across access class in some way (either SCB then
		* TID, or else TID then SCB). So the context is unified.
		*/
		tidstate = &ias_info->unified_tid_state;
	}
	return tidstate;
#endif /* TAF_ENABLE_SQS_PULL */
}

static INLINE uint32 taf_units_to_score(taf_method_info_t* method, taf_scb_cubby_t *scb_taf,
	int tid, uint32 units)
{
	uint8* score_weights = method->ias->score_weights;
	uint32 score = (units * score_weights[tid]) >> 8;

#ifdef WLATF_PERC
	if (scb_taf && scb_taf->scb->sched_staperc) {
		/* If ATM percentages are applied, normalize the scores here */
		return score / scb_taf->scb->sched_staperc;
	}
#endif // endif
	return score;
}

static int  taf_ias_dump(void *handle, struct bcmstrbuf *b);
static bool taf_ias_rate_override(void* handle, ratespec_t rspec, wlcband_t *band);

static bool taf_ias_link_state(void *handle, struct scb* scb, int tid, taf_source_type_t s_idx,
	taf_link_state_t state);
static int taf_ias_scb_state(void *handle, struct scb* scb, void* update, taf_scb_state_t state);

static bool taf_ias_tx_status(void * handle, taf_scb_cubby_t* scb_taf, int tid, void * p,
	taf_txpkt_state_t status);

static int taf_ias_iovar(void *handle, const char* cmd, wl_taf_define_t* result,
	struct bcmstrbuf* b);

static void taf_ias_sched_state(void *, taf_scb_cubby_t *, int tid, int count,
	taf_source_type_t s_idx, taf_sched_state_t);

static void taf_ias_tid_reset(taf_method_info_t* method, int tid);

#ifdef TAF_PKTQ_LOG
static uint32 taf_ias_dpstats_dump(void * handle, taf_scb_cubby_t* scb_taf,
	mac_log_counters_v06_t* mac_log, bool clear, uint32 timelo, uint32 timehi,
	uint32 prec_mask);
#endif // endif

#if TAF_ENABLE_SQS_PULL
static void taf_iasm_sched_state(void *, taf_scb_cubby_t *, int tid, int count,
	taf_source_type_t s_idx, taf_sched_state_t);
#endif /* TAF_ENABLE_SQS_PULL */

#endif /* _wlc_taf_priv_ias_h_ */
