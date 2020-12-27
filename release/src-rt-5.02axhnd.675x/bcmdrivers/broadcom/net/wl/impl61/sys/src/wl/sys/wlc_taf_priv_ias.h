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
#define TAF_COEFF_IAS_MAX_BITS  12
#define TAF_COEFF_IAS_MAX	(1 << TAF_COEFF_IAS_MAX_BITS) /* coefficient for normalisation */
#define TAF_COEFF_IAS_MAX_CFG	(((1 << TAF_COEFF_IAS_MAX_BITS) * 255) / 256)
#define TAF_SCORE_IAS_MAX	(TAF_SCORE_MAX / TAF_COEFF_IAS_MAX) /* max val prior norm */
#define TAF_COEFF_EBOS_MAX	64
#define TAF_PCOUNT_SCALE_SHIFT	5

#define TAF_TIME_HIGH_MAX	16000
#define TAF_TIME_LOW_MAX	16000
#define TAF_TIME_HIGH_DEFAULT	6000
#define TAF_TIME_LOW_DEFAULT	4000
#define TAF_TIME_ATOS_HIGH_DEFAULT	TAF_TIME_HIGH_DEFAULT
#define TAF_TIME_ATOS_LOW_DEFAULT	TAF_TIME_LOW_DEFAULT
#define TAF_TIME_ATOS2_HIGH_DEFAULT	(TAF_TIME_ATOS_HIGH_DEFAULT + 600)
#define TAF_TIME_ATOS2_LOW_DEFAULT	TAF_TIME_LOW_DEFAULT

#define TAF_PKT_SIZE_DEFAULT (100)

#define NUM_IAS_SCHEDULERS   (LAST_IAS_SCHEDULER - FIRST_IAS_SCHEDULER + 1)
#define IAS_INDEX(type)      ((type) - FIRST_IAS_SCHEDULER)
#define IAS_INDEXM(M)        IAS_INDEX((M)->type)

typedef enum {
	TAF_TRIGGER_NONE,
	TAF_TRIGGER_STARTED,
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
	int32  cumul_units[4];
	uint32 resched_units;
	uint32 high[NUM_IAS_SCHEDULERS];
	uint32 low[NUM_IAS_SCHEDULERS];
	uint8  resched_index;
	uint8  was_reset;
	uint8  star_packet_received;
	int8   resched_index_suppr;
	uint32 sched_start;
	struct {
		uint32 uflow_low;
		uint32 uflow_high;
		uint32 target_high;
		uint32 target_low;
	} data;
	struct {
		uint32 uflow_low;
		uint32 uflow_high;
		uint32 target_high;
		uint32 target_low;
		uint32 average_high;
		uint32 sched_duration;
		uint32 max_duration;
		uint32 prev_release_time;
		uint32 time_delta;
	} debug;
	taf_trigger_cause_t trigger;
} taf_schedule_tid_state_t;

typedef struct {
	uint32 index;
	/* the following is used for tracking average packet size */
	struct {
		uint16  release_pkt_size_mean;
		uint16  release_pcount;
		uint32  release_bytes;
		uint32  release_units;
		uint32  total_scaled_pcount;
		uint32  total_bytes;
		uint32  calc_timestamp;
	} data;
#ifdef TAF_DBG
	struct {
		uint32  emptied;
		uint32  max_did_rel_delta;
		uint32  ready;
		uint32  did_release;
		uint16  release_pcount;
		uint16  release_pkt_size_mean;
		uint32  release_frcount;
		uint32  release_time;
		uint32  did_rel_delta;
		uint32  did_rel_timestamp;
	} debug;
#endif // endif
} taf_ias_sched_scbtid_stats_t;

typedef struct taf_ias_sched_data {
	//uint32 idle_periods;
	bool use_amsdu[TAF_NUM_SCHED_SOURCES];
	taf_sched_handle_t handle[TAF_NUM_SCHED_SOURCES];
	taf_ias_sched_scbtid_stats_t scbtidstats;
} taf_ias_sched_data_t;

typedef struct taf_ias_coeff {
	uint16             coeff1;
	uint16             coeff10;
	uint16             coeff100;
} taf_ias_coeff_t;

typedef struct taf_ias_method_info {
	uint32             high;
	uint32             low;
	taf_ias_coeff_t    coeff;
	uint8*             score_weights;
	uint32             release_limit;
	bool               data_block;
	uint32             opt_aggp_limit;
	taf_traffic_map_t  tid_active;
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
	//uint16             release_limit[NUM_IAS_SCHEDULERS][TAF_MAXPRIO];
	/* ias_ready_to_schedule is here so all EBOS/ATOS etc schedulers can peek status */
	taf_traffic_map_t  ias_ready_to_schedule[NUM_IAS_SCHEDULERS];
	uint32             index;     /* used to separate scheduler periods */
	uint32             cpu_time;
	uint32             cpu_elapsed_time;
#ifdef TAF_DBG
	uint32             data_block_start;
	uint32             data_block_total;
	uint32             data_block_prev_in_transit;
#endif // endif
} taf_ias_group_info_t;

typedef struct {
	taf_method_info_t       method;
	taf_ias_method_info_t   ias;
} taf_ias_container_t;

#define TAF_IAS_GROUP_INFO(_t) \
			(taf_ias_group_info_t*)((_t)->group_context[TAF_SCHEDULER_IAS_METHOD])

#define TAF_IAS_METHOD_GROUP_INFO(_m)   (taf_ias_group_info_t*)TAF_METHOD_GROUP_INFO(_m)

#define TAF_IAS_TID_STATE(scb_taf, tid) (taf_ias_sched_data_t*)(TAF_CUBBY_TIDINFO(scb_taf, tid))

#define taf_ias_TID_STATS(_s_t, _t)  ((TAF_IAS_TID_STATE(_s_t, _t))->scbtidstats)

static INLINE
taf_schedule_tid_state_t* BCMFASTPATH taf_get_tid_state(wlc_taf_info_t* taf_info, int tid)
{
	taf_ias_group_info_t* ias_info = TAF_IAS_GROUP_INFO(taf_info);

#if TAF_ENABLE_SQS_PULL
	TAF_ASSERT(tid == TAF_DEFAULT_UNIFIED_TID);

	return &ias_info->unified_tid_state;
#else
	taf_schedule_tid_state_t* tidstate = NULL;

	TAF_ASSERT(tid >= 0 && tid < TAF_NUM_TIDSTATE);

	if (taf_info->ordering == TAF_ORDER_TID_PARALLEL) {
		/* all access classes are running in parallel, so maintain separated TID
		* context
		*/
		tidstate = &ias_info->tid_state[tid];
	} else {
		/* the scheduling is across access class in some way (either SCB then
		* TID, or else TID then SCB). So the context is unified.
		*/
		tidstate = &ias_info->unified_tid_state;
	}
	return tidstate;
#endif /* TAF_ENABLE_SQS_PULL */
}

static INLINE uint32 taf_units_to_score(taf_method_info_t* method, int tid, uint32 units)
{
	uint8* score_weights = method->ias->score_weights;
	TAF_ASSERT(score_weights && tid >= 0 && tid < NUMPRIO);
	return (units * score_weights[tid]) >> 8;
}

static int  taf_ias_dump(void *handle, struct bcmstrbuf *b);
static bool taf_ias_rate_override(void* handle, ratespec_t rspec, wlcband_t *band);

static bool taf_ias_link_state(void *handle, struct scb* scb, int tid, taf_source_type_t s_idx,
	taf_link_state_t state);
static int
taf_ias_scb_state(void *handle, struct scb* scb, void* update, taf_scb_state_t state);

static bool taf_ias_tx_status(void * handle, taf_scb_cubby_t* scb_taf, int tid, void * p,
	taf_txpkt_state_t status);

static int taf_ias_iovar(void *handle, const char* cmd, wl_taf_define_t* result,
	struct bcmstrbuf* b);

static void taf_ias_sched_state(void *, taf_scb_cubby_t *, int tid, int count,
	taf_source_type_t s_idx, taf_sched_state_t);

#if TAF_ENABLE_SQS_PULL
static void taf_ias_markup_sched_state(void *, taf_scb_cubby_t *, int tid, int count,
	taf_source_type_t s_idx, taf_sched_state_t);
#endif /* TAF_ENABLE_SQS_PULL */

#endif /* _wlc_taf_priv_ias_h_ */
