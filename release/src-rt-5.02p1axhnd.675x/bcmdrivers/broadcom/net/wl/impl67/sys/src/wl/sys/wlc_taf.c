/*
 *	<<Broadcom-WL-IPTag/Proprietary:>>
 *
 *	wlc_taf.c
*
*	This file implements the WL driver infrastructure for the TAF module.
*
*      Copyright 2020 Broadcom
*
*      This program is the proprietary software of Broadcom and/or
*      its licensors, and may only be used, duplicated, modified or distributed
*      pursuant to the terms and conditions of a separate, written license
*      agreement executed between you and Broadcom (an "Authorized License").
*      Except as set forth in an Authorized License, Broadcom grants no license
*      (express or implied), right to use, or waiver of any kind with respect to
*      the Software, and Broadcom expressly reserves all rights in and to the
*      Software and all intellectual property rights therein.  IF YOU HAVE NO
*      AUTHORIZED LICENSE, THEN YOU HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY
*      WAY, AND SHOULD IMMEDIATELY NOTIFY BROADCOM AND DISCONTINUE ALL USE OF
*      THE SOFTWARE.
*
*      Except as expressly set forth in the Authorized License,
*
*      1. This program, including its structure, sequence and organization,
*      constitutes the valuable trade secrets of Broadcom, and you shall use
*      all reasonable efforts to protect the confidentiality thereof, and to
*      use this information only in connection with your use of Broadcom
*      integrated circuit products.
*
*      2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED
*      "AS IS" AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES,
*      REPRESENTATIONS OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR
*      OTHERWISE, WITH RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY
*      DISCLAIMS ANY AND ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY,
*      NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES,
*      ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
*      CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING
*      OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
*
*      3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL
*      BROADCOM OR ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL,
*      SPECIAL, INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR
*      IN ANY WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN
*      IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii)
*      ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF
*      OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS SHALL APPLY
*      NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
*
*	$Id$
*/
/*
 * Include files.
 */

#include <wlc_taf_cmn.h>
#include <wlc_dump.h>
#include <wl_export.h>
#include <wlc_tx.h>

#ifdef BCMDBG
#ifdef DONGLEBUILD
/*
 * used to temporarily avoid assert during attach otherwise DHD does not start and we don't see
 * the message
 */
bool taf_attach_complete = FALSE;
#endif /* DONGLEBUILD */
const char* taf_assert_fail = "TAF ASSERT fail";
#endif /* BCMDBG */

#ifdef HOSTDPI
const uint8 wlc_taf_prec2prio[WLC_PREC_COUNT] = {
	0,	/* 0 */		0,
	1,	/* 2 */		1,
	2,	/* 4 */		2,
	3,	/* 6 */		3,
	4,	/* 8 */		4,
	5,	/* 10 */	5,
	6,	/* 12 */	6,
	7,	/* 14 */	7
 };

#else /* HOSTDPI */
const uint8 wlc_taf_prec2prio[WLC_PREC_COUNT] = {
	2,	/* 0 */		2,
	1,	/* 2 */		1,
	0,	/* 4 */		0,
	3,	/* 6 */		3,
	4,	/* 8 */		4,
	5,	/* 10 */	5,
	6,	/* 12 */	6,
	7,	/* 14 */	7
};
#endif /* HOSTDPI */

static INLINE bool taf_link_state_method_update(taf_method_info_t* method, struct scb* scb, int tid,
	taf_source_type_t s_idx, taf_link_state_t state);

static INLINE int taf_scb_state_method_update(taf_method_info_t* method, struct scb* scb,
	taf_source_type_t s_idx, void* update, taf_scb_state_t state);

static bool taf_is_bypass(wlc_taf_info_t *taf_info);

static taf_method_info_t* taf_get_method_by_name(wlc_taf_info_t* taf_info, const char* cmd);

static void taf_list_demote_item(taf_method_info_t* method, uint32 prio);
static void taf_move_list_item(taf_scb_cubby_t* scb_taf, taf_method_info_t* dest_method);

static const taf_scheduler_def_t taf_null_scheduler =
{
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

#define TAF_NULL_SCHEDULER      &taf_null_scheduler

const taf_scheduler_def_t * taf_scheduler_definitions[NUM_TAF_SCHEDULERS] = {
#if TAF_ENABLE_SQS_PULL && defined(WLTAF_IAS)
	/* TAF_VIRTUAL_MARKUP, */
	&taf_ias_scheduler,
#endif // endif
#ifdef WLTAF_IAS
	/* TAF_EBOS, */
	&taf_ias_scheduler,

	/* TAF_PSEUDO_RR, */
	TAF_NULL_SCHEDULER,

	/* TAF_ATOS, */
	&taf_ias_scheduler,

	/* TAF_ATOS2, */
	&taf_ias_scheduler,
#endif /* WLTAF_IAS */
};

const char taf_undefined_string[] = "(not supported)";

/* this maps d11_rev128_txs_mutype_enum (+1) to TAF taf_tech_type_t */
static const taf_tech_type_t tech_idx_map[] = {
	TAF_TECH_DL_SU,
#if TAF_ENABLE_MU_TX
	TAF_TECH_DL_VHMUMIMO, /* TX_STATUS_MUTP_VHTMU */
	TAF_TECH_DL_HEMUMIMO, /* TX_STATUS_MUTP_HEMM */
	TAF_TECH_DL_OFDMA     /* TX_STATUS_MUTP_HEOM */
#endif // endif
};

static const int8 tech_idx_rmap[TAF_NUM_TECH_TYPES] = {
#if TAF_ENABLE_MU_TX
	TAF_REL_TYPE_HEMUMIMO,  /* TAF_TECH_DL_HEMUMIMO */
	TAF_REL_TYPE_VHMUMIMO,  /* TAF_TECH_DL_VHMUMIMO */
	TAF_REL_TYPE_OFDMA,     /* TAF_TECH_DL_OFDMA */
#if TAF_ENABLE_UL
	TAF_REL_TYPE_ULOFDMA,   /* TAF_TECH_UL_OFDMA */
#endif /* TAF_ENABLE_UL */
#endif /* TAF_ENABLE_MU_TX */
	TAF_REL_TYPE_SU         /* TAF_TECH_DL_SU */
};

/* converts d11_rev128_txs_mutype_enum to a TAF taf_tech_type_t; assumes negative number is SU */
taf_tech_type_t taf_mutype_to_tech(int idx)
{
	TAF_ASSERT(idx <= TX_STATUS_MUTP_HEOM);

	if (idx < 0 || ++idx >= ARRAYSIZE(tech_idx_map)) {
		return TAF_TECH_DL_SU;
	}
	return tech_idx_map[idx];
}

/* converts TAF taf_tech_type_t to d11_rev128_txs_mutype_enum */
int8 taf_tech_to_mutype(taf_tech_type_t tech)
{
	if (tech < TAF_NUM_TECH_TYPES) {
		return tech_idx_rmap[tech];
	}
	return TAF_REL_TYPE_NOT_ASSIGNED;
}

#if TAF_ENABLE_UL
const char* taf_type_name[TAF_NUM_LIST_TYPES] = {" [DL]", " [UL]"};
#else
const char* taf_type_name[TAF_NUM_LIST_TYPES] = { "" };
#endif // endif

const char* taf_tx_sources[TAF_NUM_SCHED_SOURCES + 1] = {
#if TAF_ENABLE_UL
	"ul",
#endif // endif
#if TAF_ENABLE_NAR
	"nar",
#endif // endif
#if TAF_ENABLE_AMPDU
	"ampdu",
#endif // endif
#if TAF_ENABLE_SQS_PULL
	"sqs_host",
#endif // endif
	taf_undefined_string
};

#if (defined(TAF_DBG) || defined(BCMDBG)) && defined(TAF_DEBUG_VERBOSE)
static char* taf_trace_buf = NULL;
static uint32 taf_trace_buf_len = 0;
static uint32 taf_trace_output_len = 0;
static int taf_trace_index = 0;

uint32
taf_dbg_idx(wlc_info_t* wlc)
{
	if (wlc && wlc->taf_handle) {
		return wlc->taf_handle->scheduler_index;
	}
	return 0;
}

#ifdef DONGLEBUILD
#include <bcmstdlib.h>
#endif // endif

#ifndef PRINTF_BUFLEN
#define PRINTF_BUFLEN 256
#endif /* PRINTF_BUFLEN */

static void
tputc(char c)
{
	static uint16 count = 0;
#ifdef DONGLEBUILD
	putc(c);
#else
	printf("%c", c);
#endif // endif
	/* this is for (usually fatal) debug output, try to avoid overloading serial console by
	 * inserting some delay into output
	 */
	if (c == '\n' || ++count >= PRINTF_BUFLEN) {
		count = 0;
		OSL_DELAY(100);
	}
}

void
wlc_taf_mem_log(wlc_info_t* wlc, const char* fn, const char* format, ...)
{
	wlc_taf_info_t* taf_info = wlc->taf_handle;
	char* dst;
	int remain;
	int local_len;
	int local_len_part;

	if (taf_trace_buf == NULL || taf_trace_buf_len == 0) {
		return;
	}

	dst = taf_trace_buf + taf_trace_index;
	remain = taf_trace_buf_len - taf_trace_index;

	TAF_ASSERT(remain >= PRINTF_BUFLEN);

	local_len = snprintf(dst, remain, "%x,%s: ", taf_info->scheduler_index, fn);

	local_len = MIN(local_len + 1, remain);
	remain -= local_len;

	if (remain > 0) {
		va_list ap;

		/* adjust backwards to overwrite terminating zero in previous write */
		--local_len;
		++remain;

		va_start(ap, format);
		local_len_part = vsnprintf(dst + local_len, remain, format, ap);
		va_end(ap);

		local_len += MIN(local_len_part + 1, remain);
	}

	if (local_len + taf_trace_index > taf_trace_buf_len - PRINTF_BUFLEN) {
		taf_trace_output_len = local_len + taf_trace_index - 1;
		taf_trace_index = 0;
	} else {
		taf_trace_index += (local_len - 1);
	}
}

void
taf_memtrace_dump(wlc_taf_info_t* taf_info)
{
	const uint32 buf_len = taf_trace_output_len;
	const int unit = (taf_info != NULL) ? WLCWLUNIT(TAF_WLCT(taf_info)) : -1;

	if (taf_trace_buf && ((taf_trace_index > 0) || taf_trace_buf[0] != 0)) {
		int index = taf_trace_index;

		if (taf_info != NULL) {
			static uint32 taf_memtrace_prev_dump = 0;
			uint32 now_time = taf_timestamp(TAF_WLCT(taf_info));

			/* help to prevent repetitive debug output (100ms min delay) */
			if (taf_memtrace_prev_dump != 0 &&
				(now_time - taf_memtrace_prev_dump) < 100000) {

				return;
			}
			taf_memtrace_prev_dump = now_time;
		}

		OSL_DELAY(500);
		WL_PRINT(("\n========================wl%d: TAF TRACE DUMP=======================\n",
			unit));
		OSL_DELAY(200);

		taf_trace_buf[index++] = 0;
		if (index == buf_len) {
			index = 0;
		}
		if (taf_trace_buf[index] == 0) {
			index = 0;
		}
		while (taf_trace_buf[index]) {
			tputc(taf_trace_buf[index++]);
			if (index == buf_len) {
				index = 0;
			}
		}
		OSL_DELAY(500);
		WL_PRINT(("=================================================================\n\n"));
		OSL_DELAY(500);
	}
}
#else /* (TAF_DBG ||  BCMDBG) && TAF_DEBUG_VERBOSE */
#ifdef BCMDBG
void
taf_memtrace_dump(wlc_taf_info_t* taf_info)
{
	/* do nothing */
}
#endif /* BCMDBG */
#endif /* (TAF_DBG ||  BCMDBG) && TAF_DEBUG_VERBOSE */

#ifdef TAF_DBG
const char* taf_rel_complete_text[NUM_TAF_REL_COMPLETE_TYPES] = {
	"null",
	"nothing to send",
	"nothing waiting aggregation",
	"partial",
	"src emptied",
	"ps mode src emptied",
	"output full",
	"output blocked",
	"no buffer",
	"time limit",
	"release limit",
	"error",
	"ps mode blocked",
	"src limited",
	"fulfilled"
};

const char* taf_link_states_text[NUM_TAF_LINKSTATE_TYPES] = {
	"none",
	"init",
	"activated",
	"de-activated",
	"hard reset",
	"soft reset",
	"removed",
	"amsdu"
};

const char* taf_scb_states_text[NUM_TAF_SCBSTATE_TYPES] = {
	"none",
	"init",
	"exit",
	"reset",
	"source enable",
	"source disable",
	"wds",
	"dwds",
	"source update",
	"bsscfg update",
	"power save",
	"off channel",
	"data block other",
	"update dl mu-mimo",
	"update ul mu-mimo",
	"update dl mu-ofdma",
	"update ul mu-ofdma",
	"twt sp enter",
	"twt sp exit",
	"check traffic active"
};

const char* taf_txpkt_status_text[TAF_NUM_TXPKT_STATUS_TYPES] = {
	"none",
	"regmpdu",
	"pktfree",
	"pktfree_reset",
	"pktfree_drop",
	"update retries",
	"update packets",
	"update rate",
	"suppressed",
	"ul suppressed",
	"suppressed free",
	"ps queued",
	"requeued",
	"ul trigger complete"
};

const char* taf_sched_state_text[TAF_NUM_SCHED_STATE_TYPES] = {
	"none",
	"data-block",
	"rewind",
	"reset"
};
#endif /* TAF_DBG */

#if TAF_ENABLE_MU_TX
const char* taf_mutech_text[TAF_NUM_TECH_TYPES] = {
	"DL-HE MIMO",
	"DL-VH MIMO",
	"DL-OFDMA",
#if TAF_ENABLE_UL
	"UL-OFDMA",
#endif /* TAF_ENABLE_UL */
	"SU"
};
#endif /* TAF_ENABLE_MU_TX */

/*
 * Module iovar handling.
 */
enum {
	IOV_TAF_DEFINE    /* universal configuration */
};

static const bcm_iovar_t taf_iovars[] = {
	{"taf", IOV_TAF_DEFINE, 0, 0, IOVT_BUFFER, sizeof(wl_taf_define_t)},
	{NULL, 0, 0, 0, 0, 0}
};

taf_source_type_t wlc_taf_get_source_index(taf_source_index_public_t source)
{
	const uint8 pub_source_map[TAF_NUM_PUBLIC_SOURCES] = {
		TAF_SOURCE_UNDEFINED,	/* TAF_NO_SOURCE */

#if TAF_ENABLE_UL
		TAF_SOURCE_UL,		/* TAF_UL */
#else
		TAF_SOURCE_UNDEFINED,	/* TAF_UL not available */
#endif // endif

#if TAF_ENABLE_NAR
		TAF_SOURCE_NAR,		/* TAF_NAR */
#else
		TAF_SOURCE_UNDEFINED,	/* TAF_NAR not available */
#endif // endif

#if TAF_ENABLE_AMPDU
		TAF_SOURCE_AMPDU,	/* TAF_AMPDU */
#else
		TAF_SOURCE_UNDEFINED,	/* TAF_AMPDU not available */
#endif // endif

#if TAF_ENABLE_SQS_PULL
		TAF_SOURCE_HOST_SQS,	/* TAF_SQSHOST */
#else
		TAF_SOURCE_UNDEFINED,	/* TAF_SQSHOST not available */
#endif // endif
	};

	if (source >= TAF_NUM_PUBLIC_SOURCES || source < TAF_NO_SOURCE) {
		return TAF_SOURCE_UNDEFINED;
	}
	return (taf_source_type_t)pub_source_map[source];
}

uint32
wlc_taf_uladmit_count(wlc_taf_info_t* taf_info, bool ps_exclude)
{
	uint32 count = 0;
#if TAF_ENABLE_UL
	taf_scb_cubby_t *scb_taf = taf_info->head;

	while (scb_taf) {
		taf_list_t* item = taf_list_find(scb_taf, TAF_TYPE_UL);

		if (item != NULL && (!ps_exclude || !scb_taf->info.ps_mode)) {
			++count;
		}
		scb_taf = scb_taf->next;
	}
	WL_TAFT3(taf_info, "count %u\n", count);
#endif // endif
	return count;
}

static int
taf_bypass(wlc_taf_info_t * taf_info, bool set)
{
	bool prev_bypass = taf_info->bypass;
	uint32 ulcount;

	if (set && wlc_taf_scheduler_blocked(taf_info)) {
		return BCME_BUSY;
	}

	if (set && taf_ul_enabled(taf_info) &&
			((ulcount = wlc_taf_uladmit_count(taf_info, FALSE)) > 0)) {
		WL_TAFT2(taf_info, "unable to bypass due to %u ul stations\n", ulcount);
		return BCME_DISABLED;
	}
	taf_info->bypass = set;
	WL_TAFT1(taf_info, "is %u\n", set);

#if TAF_ENABLE_SQS_PULL
	/* Configure BUS side scheduler based on TAF state */
	wl_bus_taf_scheduler_config(TAF_WLCT(taf_info)->wl, taf_info->enabled && !taf_info->bypass);
#endif /* TAF_ENABLE_SQS_PULL */

	if (prev_bypass && !set) {
		wlc_taf_reset_scheduling(taf_info, ALLPRIO, TRUE);
	}
	return BCME_OK;
}

static int
taf_set_cubby_method(taf_scb_cubby_t *scb_taf, taf_method_info_t *dst_method, uint32 prio)
{
	taf_list_scoring_order_t ordering = dst_method->ordering;

	if (ordering != TAF_LIST_DO_NOT_SCORE) {
		if (ordering == TAF_LIST_SCORE_FIXED_INIT_MINIMUM) {
			/* demote existing entry at this priority (if it exists) */
			taf_list_demote_item(dst_method, TAF_DLPRIO(prio));
#if TAF_ENABLE_UL
			/* demote existing entry for UL prio (if it exists) */
			taf_list_demote_item(dst_method, TAF_ULPRIO(prio));
#endif // endif
		} else if (ordering == TAF_LIST_SCORE_FIXED_INIT_MAXIMUM) {
			TAF_ASSERT(0);
		}
		scb_taf->score[TAF_TYPE_DL] = TAF_DLPRIO(prio);
#if TAF_ENABLE_UL
		scb_taf->score[TAF_TYPE_UL] = TAF_ULPRIO(prio);
#endif // endif
	}

	taf_move_list_item(scb_taf, dst_method);

	if (ordering != TAF_LIST_DO_NOT_SCORE) {
		wlc_taf_sort_list(taf_get_list_head_ptr(dst_method), ordering,
			taf_timestamp(TAF_WLCM(dst_method)));
	}
#if TAF_ENABLE_SQS_PULL
	taf_scb_state_method_update(dst_method, scb_taf->scb, TAF_SOURCE_HOST_SQS, NULL,
		TAF_SCBSTATE_SOURCE_ENABLE);
#endif // endif
	return BCME_OK;
}

static uint32
taf_total_traffic_pending(wlc_taf_info_t* taf_info)
{
	uint32 total_packets = 0;
	taf_scb_cubby_t *scb_taf = taf_info->head;

	while (scb_taf) {
		total_packets += wlc_taf_traffic_active(taf_info, scb_taf->scb);
		scb_taf = scb_taf->next;
	}
	return total_packets;
}

static int
taf_dump_list(taf_method_info_t* method, struct bcmstrbuf* b)
{
	taf_list_t* iter;
	uint32 list_index = 0;

	TAF_ASSERT(method);

	iter = taf_get_list_head(method);

	if (iter) {
		bcm_bprintf(b, "assigned %s entries:\n", TAF_SCHED_NAME(method));
	}
	else {
		bcm_bprintf(b, "no assigned entries for %s\n", TAF_SCHED_NAME(method));
	}

	while (iter) {
		bcm_bprintf(b, "%3d:  "MACF"%s", ++list_index, TAF_ETHERC(iter->scb_taf),
			TAF_TYPE(iter->type));

		if ((method->ordering == TAF_LIST_SCORE_FIXED_INIT_MINIMUM) ||
				(method->ordering == TAF_LIST_SCORE_FIXED_INIT_MAXIMUM)) {
			bcm_bprintf(b, " : %u\n", TAF_PRIO(iter->scb_taf->score[iter->type]));
		}
		else {
			bcm_bprintf(b, "\n");
		}
		iter = iter->next;
	}
	return BCME_OK;
}

int
wlc_taf_param(const char** cmd, uint32* param, uint32 min, uint32 max, struct bcmstrbuf* b)
{
	*cmd += strlen(*cmd) + 1;

	if (**cmd) {
		/* an extra parameter was supplied, treat it as a 'set' operation */
		uint32 value;

		value = bcm_strtoul(*cmd, NULL, 0);
		if (value > max || value < min) {
			return BCME_RANGE;
		}
		*param = value;
		if (b) {
			bcm_bprintf(b, "%c", '\0');
		}
		return TAF_IOVAR_OK_SET;
	}
	else if (b) {
		/* no more parameter was given, treat as a 'get' operation */
		if (*param > 16) {
			bcm_bprintf(b, "%u (0x%x)\n", *param, *param);
		}
		else {
			bcm_bprintf(b, "%u\n", *param);
		}
		return TAF_IOVAR_OK_GET;
	}
	/* should never get here if buffer (b) is OK */
	return BCME_BUFTOOSHORT;
}

static int
taf_method_iovar(wlc_taf_info_t* taf_info, taf_scheduler_kind type, const char* cmd,
	wl_taf_define_t* result, struct bcmstrbuf* b, taf_scb_cubby_t* scb_taf)
{
	taf_method_info_t* method = taf_get_method_info(taf_info, type);
	int err = BCME_UNSUPPORTED;

	if (method && method->funcs.iovar_fn) {
		err = method->funcs.iovar_fn(method, scb_taf, *cmd ? cmd : NULL, result, b);
	}
	return err;
}

#ifdef TAF_DEBUG_VERBOSE
static int
taf_set_trace_buf(wlc_taf_info_t* taf_info, uint32 len)
{
	wlc_info_t* wlc = TAF_WLCT(taf_info);

	if ((taf_trace_buf != NULL) && (taf_trace_buf_len > 0)) {
		MFREE(wlc->pub->osh, taf_trace_buf,
		      taf_trace_buf_len);
		taf_trace_buf = NULL;
		taf_trace_buf_len = 0;
		taf_trace_output_len = 0;
	}
	if (len > 0) {
		taf_trace_buf_len = len * 1024;
		taf_trace_buf = MALLOCZ(wlc->pub->osh, taf_trace_buf_len);
		if (taf_trace_buf == 0) {
			taf_trace_buf_len = 0;
			return BCME_NOMEM;
		}
		taf_trace_output_len = taf_trace_buf_len;
	}
	taf_trace_index = 0;

	return BCME_OK;
}
#endif /* TAF_DEBUG_VERBOSE */

static int
taf_do_enable(wlc_taf_info_t* taf_info, bool enable)
{
	wlc_info_t* wlc = TAF_WLCT(taf_info);

	TAF_ASSERT(!wlc->pub->up);
	/* check some config */
	TAF_ASSERT(TAF_NUM_TECH_TYPES < 8);
	TAF_ASSERT(NUM_TAF_SCHEDULER_METHOD_GROUPS != 0);
	TAF_ASSERT(NUM_TAF_SCHEDULERS != 0);

	taf_info->enabled = enable;

#if TAF_ENABLE_UL
	/* disable TAF UL scheduling if TAF feature is disabled */
	if (!taf_info->enabled) {
		taf_info->ul_enabled = FALSE;
		/* disable ucode for ul scheduling */
		wlc_ulmu_sw_trig_enable(wlc, FALSE);
	}
#endif // endif

#if TAF_ENABLE_SQS_PULL
	/* Configure BUS side scheduler based on TAF state */
	wl_bus_taf_scheduler_config(wlc->wl, taf_info->enabled && !taf_info->bypass);
#endif // endif

	WL_PRINT(("wl%u %s: TAF is %sabled\n", WLCWLUNIT(wlc), __FUNCTION__,
		taf_info->enabled ? "en" : "dis"));
	return BCME_OK;
}

/* called during up transition */
static void
taf_init(wlc_taf_info_t* taf_info)
{
	BCM_REFERENCE(taf_info);

#if TAF_ENABLE_SQS_PULL
	/* Configure BUS side scheduler (perhaps again) based on TAF state */
	wl_bus_taf_scheduler_config(TAF_WLCT(taf_info)->wl, taf_info->enabled && !taf_info->bypass);
#endif // endif
#ifdef TAF_DEBUG_VERBOSE
	/* enable this next line for debug of stalls and crashes */
	//taf_set_trace_buf(taf_info, 16);
#endif // endif

#if TAF_ENABLE_UL
	/* config ucode for ul scheduling */
	wlc_ulmu_sw_trig_enable(TAF_WLCT(taf_info), taf_ul_enabled(taf_info));
#endif // endif
}

static void
taf_define_tech_out(struct bcmstrbuf* b, uint32 mask)
{
	taf_tech_type_t idx;

	bcm_bprintf(b, "idx:   tech type\n");
#if TAF_ENABLE_MU_TX
	for (idx = 0; idx < TAF_NUM_TECH_TYPES; idx++) {
		if (mask & (1 << idx)) {
			bcm_bprintf(b, " %2u:   %s\n", idx, taf_mutech_text[idx]);
		}
	}
#else
	idx = TAF_TECH_DL_SU;
	if (mask & (1 << idx)) {
		bcm_bprintf(b, " %2u:   %s\n", idx, "SU");
	}
#endif /* TAF_ENABLE_MU_TX */
}

static int
taf_define(wlc_taf_info_t* taf_info, const struct ether_addr* ea,
	wl_taf_define_t* result, wl_taf_define_t* input, struct bcmstrbuf* b, bool set)
{

	taf_scheduler_kind type;
	taf_method_info_t* method;
	const struct ether_addr undef_ea = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
	wlc_info_t*        wlc = TAF_WLCT(taf_info);
	char               local_input_cache[128];
	const char*        cmd = local_input_cache;
	int                copyi = 0;

	while ((input->text[copyi] || input->text[copyi + 1]) &&
			(copyi < (ARRAYSIZE(local_input_cache) - 2))) {
		local_input_cache[copyi] = input->text[copyi];
		copyi++;
	}
	local_input_cache[copyi++] = 0;
	local_input_cache[copyi++] = 0;

	/* if undef_ea is set, process as text arguments */
	if (eacmp(ea, &undef_ea) == 0) {
		result->ea = undef_ea;

		/* no argument given */
		if (cmd[0] == 0) {
			bcm_bprintf(b, "taf is %sabled (%u), use 'taf enable 0|1' to change\n",
				taf_info->enabled ? "en" : "dis", taf_info->enabled);
			return BCME_OK;
		}

#ifdef TAF_DEBUG_VERBOSE
		if (!strcmp(cmd, "trace_buf")) {
			uint32 len = taf_trace_buf_len / 1024;
			int err = wlc_taf_param(&cmd, &len, 0, 32, b);
			if (err == TAF_IOVAR_OK_SET) {
				return taf_set_trace_buf(taf_info, len);
			}
			return err;
		}
#endif /* TAF_DEBUG_VERBOSE */
		if (!strcmp(cmd, "enable")) {
			uint32 state = taf_info->enabled ? 1 : 0;
			int err = wlc_taf_param(&cmd, &state, 0, 1, b);

			/* try to interpret whether text param is 'set' or 'get' */
			if (!set && (err == TAF_IOVAR_OK_SET) && *cmd &&
					(state != (taf_info->enabled ? 1 : 0))) {
				set = TRUE;
			}
			/* was not in down state and there was attempt to change it */
			if (set && wlc->pub->up) {
				return BCME_NOTDOWN;
			}
			if (set) {
				err = taf_do_enable(taf_info, state);
			}
			result->misc = taf_info->enabled;
			return err;
		}
		if (!taf_info->enabled) {
			return BCME_NOTREADY;
		}
#if TAF_ENABLE_UL
		if (!strcmp(cmd, "ul")) {
			uint32 state = taf_info->ul_enabled ? 1 : 0;
			int err = wlc_taf_param(&cmd, &state, 0, 1, b);

			/* try to interpret whether text param is 'set' or 'get' */
			if (!set && (err == TAF_IOVAR_OK_SET) && *cmd &&
					(state != (taf_info->ul_enabled ? 1 : 0))) {
				set = TRUE;
			}
			/* was not in down state and there was attempt to change it */
			if (set) {
				if (wlc->pub->up) {
					return BCME_NOTDOWN;
				} else {
					err = wlc_ulmu_sw_trig_enable(wlc, state);
					if (err == BCME_OK) {
						taf_info->ul_enabled = state;
					}
				}
			}
			result->misc = taf_info->ul_enabled;
			return err;
		}
#endif /* TAF_ENABLE_UL */
		if (!strcmp(cmd, "order")) {
			result->misc = TAF_ORDER_TID_SCB;
			bcm_bprintf(b, "taf order is %d: TID order then SCB (unalterable)\n",
				result->misc);
			cmd += strlen(cmd) + 1;
			if (cmd[0]) {
				/* no longer support changing this setting */
				return BCME_UNSUPPORTED;
			}
			return BCME_OK;
		}

		if (!strcmp(cmd, "bypass")) {
			uint32 state = taf_info->bypass;
			int err = wlc_taf_param(&cmd, &state, FALSE, TRUE, b);

			if (err == TAF_IOVAR_OK_SET) {
				err = taf_bypass(taf_info, state);
			}
			result->misc = taf_info->bypass;
			return err;
		}
		if (!strcmp(cmd, "ratesel")) {
			int err = wlc_taf_param(&cmd, &taf_info->use_sampled_rate_sel, 0,
				(1 << TAF_NUM_TECH_TYPES) - 1, b);

			result->misc = taf_info->use_sampled_rate_sel;

			taf_define_tech_out(b, taf_info->use_sampled_rate_sel);
			return err;
		}
#if TAF_ENABLE_MU_BOOST
		if (!strcmp(cmd, "mu_boost")) {
			uint32 boost = taf_info->mu_boost;
			int err = wlc_taf_param(&cmd, &boost, TAF_MUBOOST_OFF,
				TAF_NUM_MUBOOST_CONFIGS - 1, b);

			if (err == TAF_IOVAR_OK_SET) {
				taf_info->mu_boost = boost;
			}
			WL_TAFT1(taf_info, "mu_boost %u\n", taf_info->mu_boost);
			result->misc = taf_info->mu_boost;
			return err;
		}
#endif /* TAF_ENABLE_MU_BOOST */
		if (!strcmp(cmd, "bulk_commit")) {
			uint32 bulk_commit = taf_info->bulk_commit;
			int err = wlc_taf_param(&cmd, &bulk_commit, FALSE, TRUE, b);

			if (err == TAF_IOVAR_OK_SET) {
				if (taf_info->bulk_commit && !bulk_commit) {
					taf_close_all_sources(taf_info, ALLPRIO);
				}
				taf_info->bulk_commit = bulk_commit;
			}
			WL_TAFT1(taf_info, "bulk_commit %u\n", taf_info->bulk_commit);
			result->misc = taf_info->bulk_commit;
			return err;
		}
		if (!strcmp(cmd, "super")) {
			uint32 super = taf_info->super;
			int err = wlc_taf_param(&cmd, &super, 0, (1 << TAF_NUM_TECH_TYPES) - 1, b);

			if (err == TAF_IOVAR_OK_SET) {
				taf_info->super = super;
			}
			taf_define_tech_out(b, taf_info->super);
			WL_TAFT1(taf_info, "super %u\n", taf_info->super);
			result->misc = taf_info->super;
			return err;
		}
		if (!strcmp(cmd, "tech")) {
			taf_define_tech_out(b, ~0);
			return BCME_OK;
		}
#ifdef TAF_DBG
		if (!strcmp(cmd, "dbgflags")) {
			int err = wlc_taf_param(&cmd, &taf_info->dbgflags, 0, (uint32)(-1), b);
			result->misc = taf_info->dbgflags;
			return err;
		}
#endif // endif
#if TAF_ENABLE_MU_TX
		if (!strcmp(cmd, "mu")) {
			uint32 mu = taf_info->mu;
			int err = wlc_taf_param(&cmd, &mu, 0,
				(TAF_NUM_MU_TECH_TYPES > 0) ?
				(1 << TAF_NUM_MU_TECH_TYPES) - 1 : 1, b);

			if (err == TAF_IOVAR_OK_SET) {
				taf_info->mu = mu;
			}
			result->misc = taf_info->mu;
			taf_define_tech_out(b, taf_info->mu);
			WL_TAFT1(taf_info, "mu %u\n", taf_info->mu);
			return err;
		}
#endif /* TAF_ENABLE_MU_TX */
#ifdef WLTAF_IAS
		/* all of these are here for backwards compatibility with previous version where
		 * EBOS/ATOS/ATOS2 where inside TAF and had direct level iovar control
		 */
		if (!strcmp(cmd, "high") || !strcmp(cmd, "low")) {
			taf_scheduler_kind etype;
			int eresult;
			for (etype = FIRST_IAS_SCHEDULER; etype <= LAST_IAS_SCHEDULER; etype ++) {
				eresult = taf_method_iovar(taf_info, etype, cmd, result, b, NULL);
				if (eresult == BCME_UNSUPPORTED) {
					continue;
				}
				if (eresult == TAF_IOVAR_OK_GET || eresult < BCME_OK) {
					return eresult;
				}
			}
			return eresult;
		}
		/* all of these are here for backwards compatibility with previous version TAF */
		if (!strcmp(cmd, "fallback") || !strcmp(cmd, "adapt") ||
				!strcmp(cmd, "high_max") || !strcmp(cmd, "low_max")) {
			return taf_method_iovar(taf_info, TAF_SCHEDULER_START, cmd, result, b,
				NULL);
		}
		/* all of these are here for backwards compatibility with previous version TAF */
		if (!strcmp(cmd, "atos_high") || !strcmp(cmd, "atos_low")) {
			return taf_method_iovar(taf_info, TAF_ATOS, cmd + sizeof("atos"),
				result, b, NULL);
		}
		/* all of these are here for backwards compatibility with previous version TAF */
		if (!strcmp(cmd, "atos2_high") || !strcmp(cmd, "atos2_low")) {
			return taf_method_iovar(taf_info, TAF_ATOS2, cmd + sizeof("atos2"),
				result, b, NULL);
		}
#endif /* WLTAF_IAS */
		if (!strcmp(cmd, "force")) {
			int err = wlc_taf_param(&cmd, &taf_info->force_time, 0,
				TAF_WINDOW_MAX, b);
			result->misc = taf_info->force_time;
			return err;
		}
		if (!strcmp(cmd, "default")) {
			cmd += strlen(cmd) + 1;

			if (*cmd) {
				if ((method = taf_get_method_by_name(taf_info, cmd))) {
					taf_info->default_scheduler = method->type;
					return BCME_OK;
				}
				return BCME_BADARG;
			}
			result->misc = taf_info->default_scheduler;
			method = taf_get_method_info(taf_info, taf_info->default_scheduler);
			bcm_bprintf(b, "%u: %s\n", result->misc, TAF_SCHED_NAME(method));
			return BCME_OK;
		}
		if (!strcmp(cmd, "wd_stall")) {
			uint32 wd_stall_limit = taf_info->watchdog_data_stall_limit;
			int err = wlc_taf_param(&cmd, &wd_stall_limit, 0, BCM_UINT16_MAX, b);
			if (err == TAF_IOVAR_OK_SET) {
				taf_info->watchdog_data_stall_limit = wd_stall_limit;
			}
			result->misc = taf_info->watchdog_data_stall_limit;
			return err;
		}
		if (!strcmp(cmd, "list")) {
			for (type = TAF_SCHEDULER_START; type < TAF_SCHED_LAST_INDEX; type++) {
				if ((method = taf_get_method_info(taf_info, type))) {
					bcm_bprintf(b, "%u: %s%s\n", type, TAF_SCHED_NAME(method),
						type == taf_info->default_scheduler ?
						" (default)" : "");
				}

			}
			return BCME_OK;
		}
		if (!strcmp(cmd, "reset")) {
			result->misc = taf_total_traffic_pending(taf_info);
#ifdef TAF_DEBUG_VERBOSE
			cmd += strlen(cmd) + 1;
			if (*cmd && !strcmp(cmd, "logdump")) {
				taf_memtrace_dump(taf_info);
			}
#endif // endif
			WL_ERROR(("wl%u %s: reset hard (total traffic %u)\n",
				WLCWLUNIT(TAF_WLCT(taf_info)), __FUNCTION__, result->misc));
			wlc_taf_reset_scheduling(taf_info, ALLPRIO, TRUE);
			return BCME_OK;
		}
#ifdef TAF_DEBUG_VERBOSE
		if (!strcmp(cmd, "logdump")) {
			taf_memtrace_dump(taf_info);
			return BCME_OK;
		}
#endif // endif
		if ((method = taf_get_method_by_name(taf_info, cmd))) {
			int err;
			bool valid_sched = method->type >= TAF_SCHEDULER_START &&
				method->type < TAF_SCHED_LAST_INDEX;

			cmd += strlen(cmd) + 1;

			if (*cmd) {
				if (valid_sched && !strcmp(cmd, "list")) {
					if (!wlc->pub->up) {
						return BCME_NOTUP;
					}
					return taf_dump_list(method, b);
				}
				if (valid_sched && !strcmp(cmd, "default")) {
					taf_info->default_scheduler = method->type;
					taf_info->default_score = method->score_init;
					return BCME_OK;
				}
				if (!strcmp(cmd, "dump")) {
					if (!wlc->pub->up) {
						return BCME_NOTUP;
					}
					if (method->funcs.dump_fn) {
						return method->funcs.dump_fn(method, b);
					}
				}
			}
			err = taf_method_iovar(taf_info, method->type, cmd, result, b, NULL);
			if (*cmd || (err != BCME_UNSUPPORTED)) {
				return err;
			}
			if (valid_sched && !*cmd) {
				return taf_dump_list(method, b);
			}
			return BCME_UNSUPPORTED;
		}
		return BCME_UNSUPPORTED;
	}

	/* at this point, a MAC address was supplied so it is not a text command. */

	/* find entry by MAC address operation */
	for (type = TAF_SCHEDULER_START; type < TAF_SCHED_LAST_INDEX; type++) {
		taf_list_t* found;
		taf_method_info_t* dst = NULL;
		taf_scb_cubby_t* scb_taf;
		bool fixed_scoring = FALSE;

		method = taf_get_method_info(taf_info, type);
		if (!method) {
			continue;
		}
		found = taf_list_ea_find(taf_get_list_head(method), ea);

		if (!found) {
			continue;
		}
		scb_taf = found->scb_taf;

		if (scb_taf && !strcmp(cmd, "reset")) {
			int tid;
			taf_source_type_t s_idx;

			WL_TAFM1(scb_taf->method, MACF" iovar reset\n", CONST_ETHERP_TO_MACF(ea));

			for (s_idx = 0; s_idx < TAF_NUM_SCHED_SOURCES; s_idx++) {
				for (tid = 0; tid < TAF_NUMPRIO(s_idx); tid++) {
					taf_link_state_method_update(scb_taf->method, scb_taf->scb,
						tid, s_idx, TAF_LINKSTATE_HARD_RESET);
				}
			}
			taf_scb_state_method_update(scb_taf->method, scb_taf->scb,
				TAF_SOURCE_UNDEFINED, NULL, TAF_SCBSTATE_RESET);
			return BCME_OK;
		}
		if (!set && cmd[0] == 0) {
			/* this is 'get' */
			result->ea = *ea;
			result->sch = type;
			result->prio = TAF_PRIO(found->scb_taf->score[found->type]);
			result->misc = 0;
			bcm_bprintf(b, "%s", TAF_SCHED_NAME(method));
			return BCME_OK;
		}

		/* this is a 'set' */

		/* was a valid (numeric) scheduler given? */

		if (input->sch != (uint32)(~0)) {
			dst = taf_get_method_info(taf_info, input->sch);

		} else if (input->text[0]) {
			dst = taf_get_method_by_name(taf_info, (const char*)input->text);
		}

		if (dst && (dst->type < TAF_SCHEDULER_START || dst->type >= TAF_SCHED_LAST_INDEX)) {
			return BCME_UNSUPPORTED;
		}

		if (!dst) {
			if (scb_taf && scb_taf->method && cmd[0] != 0) {
				return taf_method_iovar(taf_info, scb_taf->method->type, cmd,
					result, b, scb_taf);
			}
			return BCME_NOTFOUND;
		}

		fixed_scoring = dst->ordering == TAF_LIST_SCORE_FIXED_INIT_MINIMUM ||
			dst->ordering == TAF_LIST_SCORE_FIXED_INIT_MAXIMUM;

		/* check priority correctly configured */
		if ((fixed_scoring && !input->prio) || (!fixed_scoring && input->prio)) {
			return BCME_BADARG;
		}
		if (fixed_scoring && input->prio > TAF_PRIO(TAF_SCORE_MAX)) {
			return BCME_RANGE;
		}

		if (dst->type != scb_taf->method->type ||
			(fixed_scoring && (input->prio != TAF_PRIO(scb_taf->score[found->type])))) {

			uint32 new_score = 0;
			int res;

			if (wlc_taf_scheduler_blocked(taf_info)) {
				return BCME_BUSY;
			}

			if (fixed_scoring) {
				new_score = input->prio;

			} else if (dst->ordering == TAF_LIST_SCORE_MINIMUM) {
				new_score = TAF_SCORE_MIN;

			} else if (dst->ordering == TAF_LIST_SCORE_MAXIMUM) {
				new_score = TAF_SCORE_MAX;

			} else {
				TAF_ASSERT(dst->ordering == TAF_LIST_DO_NOT_SCORE);
			}
			res = taf_set_cubby_method(scb_taf, dst, new_score);

			if (res != BCME_OK) {
				return res;
			}
		} else {
			WL_TAFT2(taf_info, MACF" is unchanged (%s prio %u)\n",
				CONST_ETHERP_TO_MACF(ea), TAF_SCHED_NAME(dst),
				fixed_scoring ? scb_taf->score[found->type] : 0);
		}
		return BCME_OK;
	}
	return BCME_NOTFOUND;
}

static int
taf_doiovar(void *handle, uint32 actionid, void *params, uint plen, void *arg, uint alen,
	uint vsize, struct wlc_if *wlcif)
{
	wlc_taf_info_t *taf_info = handle;
	int status = BCME_OK;

	if (D11REV_LT(TAF_WLCT(taf_info)->pub->corerev, 40)) {
		/* only support on latest chipsets */
		return BCME_UNSUPPORTED;
	}
#if !TAF_VALID_CONFIG
	return BCME_UNSUPPORTED;
#endif // endif

	switch (actionid) {
		/* there is overlap in Set and Get for this iovar */
		case IOV_GVAL(IOV_TAF_DEFINE):
		case IOV_SVAL(IOV_TAF_DEFINE):
		{
			wl_taf_define_t* taf_def_return = (wl_taf_define_t*) arg;
			wl_taf_define_t* taf_def_input = (wl_taf_define_t*) params;
			const struct ether_addr ea = taf_def_input->ea;
			int result;

			struct bcmstrbuf b;
			int32  avail_len;

			/* only version 1 is currently supported */
			if (taf_def_input->version != 1) {
				WL_ERROR(("taf iovar version incorrect (%u/%u)\n",
					taf_def_input->version, 1));
				return BCME_VERSION;
			}

			avail_len = alen - OFFSETOF(wl_taf_define_t, text);
			avail_len = (avail_len > 0 ? avail_len : 0);
			b.origsize = b.size = avail_len;
			b.origbuf = b.buf = (char *) &taf_def_return->text[0];

			result = taf_define(taf_info, &ea, taf_def_return,
				taf_def_input, &b, actionid == IOV_SVAL(IOV_TAF_DEFINE));
			if (result == TAF_IOVAR_OK_SET || result == TAF_IOVAR_OK_GET) {
				result = BCME_OK;
			}
			return result;
		}
		break;

		default:
			status = BCME_UNSUPPORTED;
			break;

	}
	return status;
}

void BCMFASTPATH
wlc_taf_list_append(taf_list_t** head, taf_list_t* item)
{
	if (head && *head == NULL) {
		*head = item;
		item->prev = NULL;
	} else {
		taf_list_t* iter = head ? *head : NULL;

		while (iter && iter->next) {
			iter = iter->next;
		}
		if (iter) {
			iter->next = item;
			item->prev = iter;
		}
	}
}

void BCMFASTPATH
wlc_taf_list_insert(taf_list_t* parent, taf_list_t* list_start)
{
	taf_list_t* below;
	taf_list_t* list_end = list_start;

	if (!list_start || !parent) {
		return;
	}
	below = parent->next;

	while (list_end->next) {
		list_end = list_end->next;
	}

	list_end->next = below;
	list_start->prev = parent;
	parent->next = list_start;

	if (below) {
		below->prev = list_end;
	}
}

void BCMFASTPATH
wlc_taf_list_remove(taf_list_t** head, taf_list_t* item)
{
	taf_list_t*  above;
	taf_list_t*  below;

	if (!item) {
		return;
	}
	above = item->prev;
	below = item->next;

	if (head && item == *head) {
		*head = below;
	}

	if (above) {
		above->next = below;
	}
	if (below) {
		below->prev = above;
	}
	item->next = NULL;
	item->prev = NULL;
}

static INLINE taf_list_t* BCMFASTPATH taf_list_compare(taf_list_t *itemA, uint32 scoreA,
	taf_list_t *itemB, uint32 scoreB, uint32 now_time)
{
	if (scoreA < scoreB) {
		return itemA;
	} else if (scoreA > scoreB) {
		return itemB;
	} else {
		uint32 itemA_elapsed = now_time - itemA->scb_taf->timestamp[itemA->type];
		uint32 itemB_elapsed = now_time - itemB->scb_taf->timestamp[itemB->type];

		if (itemA_elapsed > itemB_elapsed) {
			return itemA;
		} else if (itemA_elapsed < itemB_elapsed) {
			return itemB;
		} else {
			/* totally equal! */
			itemA->scb_taf->timestamp[itemA->type] += 1;
		}
	}
	return itemA;
}

taf_list_t* BCMFASTPATH wlc_taf_merge_list(taf_list_t** headA, taf_list_t** headB,
	taf_list_scoring_order_t ordering, uint32 now_time)
{
	taf_list_t* local_list_head = NULL;
	taf_list_t* local_list_end = NULL;
	taf_list_t* listA;
	taf_list_t* listB;
	uint32 scoreA;
	uint32 scoreB;

	TAF_ASSERT(ordering == TAF_LIST_SCORE_MINIMUM ||
		ordering == TAF_LIST_SCORE_FIXED_INIT_MINIMUM);

	if (!(headA && headB)) {
		TAF_ASSERT(0);
		return NULL;
	}

	listA = *headA;
	listB = *headB;

	if (listA && listB) {
		scoreA = listA->scb_taf->score[listA->type];
		scoreB = listB->scb_taf->score[listB->type];
	} else {
		scoreA = scoreB = 0;
	}

	while (listA && listB) {
		taf_list_t* item;

		item = taf_list_compare(listA, scoreA, listB, scoreB, now_time);
		if (item == listA) {
			listA = item->next;
			if (listA) {
				scoreA = listA->scb_taf->score[listA->type];
			}
		} else {
			listB = item->next;
			if (listB) {
				scoreB = listB->scb_taf->score[listB->type];
			}
		}

		if (local_list_end != NULL) {
			local_list_end->next = item;
		}
		item->prev = local_list_end;
		local_list_end = item;

		if (local_list_head == NULL) {
			local_list_head = item;
		}
	}

	if (listA) {
		if (local_list_end) {
			local_list_end->next = listA;
			listA->prev = local_list_end;
		} else {
			listA->prev = NULL;
			local_list_head = listA;
		}
	} else if (listB) {
		if (local_list_end) {
			local_list_end->next = listB;
			listB->prev = local_list_end;
		} else {
			listB->prev = NULL;
			local_list_head = listB;
		}
	}

	*headA = NULL;
	*headB = NULL;

	return local_list_head;
}

static INLINE void taf_list_split(taf_list_t* source, taf_list_t** listA, taf_list_t** listB)
{
	taf_list_t* front = source;
	taf_list_t* back = source->next;

	while (back) {
		back = back->next;
		if (back) {
			back = back->next;
			front = front->next;
		}
	}
	*listA = source;
	*listB = front->next;
	front->next = NULL;
}

static void taf_list_demote_item(taf_method_info_t* method, uint32 prio)
{
	taf_list_t* found;
	taf_list_t* head = taf_get_list_head(method);

	/* only support minimum scoring currently */
	TAF_ASSERT(method->ordering == TAF_LIST_SCORE_MINIMUM ||
		method->ordering == TAF_LIST_SCORE_FIXED_INIT_MINIMUM);

	WL_TAFM2(method, "checking to demote item prio %u\n", prio);

	do {
		taf_list_t* iter = head;
		uint32 highest = 0;
		found = NULL;

		while (iter) {
			taf_list_type_t type = iter->type;
			taf_scb_cubby_t* scb_taf = iter->scb_taf;

			TAF_ASSERT(scb_taf);

			if (scb_taf->score[type] > highest) {
				highest = scb_taf->score[type];
			}
			if (scb_taf->score[type] == prio) {
				found = iter;
			}
			iter = iter->next;
		}
		if (found) {
			found->scb_taf->score[found->type] = highest + 1;
			WL_TAFM1(method, "demoted item "MACF"%s with prio %u to prio %u\n",
				TAF_ETHERC(found->scb_taf), TAF_TYPE(found->type), prio,
				found->scb_taf->score[found->type]);
		}
	} while (found);
}

void
wlc_taf_list_delete(taf_scb_cubby_t *scb_taf, taf_list_type_t type)
{
	taf_list_t* item = taf_list_find(scb_taf, type);

	if (item) {
		taf_method_info_t* method = scb_taf->method;

		item->scb_taf = NULL;
		wlc_taf_list_remove(taf_get_list_head_ptr(method), item);
	}
}

void BCMFASTPATH
wlc_taf_sort_list(taf_list_t ** source_head, taf_list_scoring_order_t ordering,
	uint32 now_time)
{
	if (*source_head && (*source_head)->next) {
		taf_list_t* listA;
		taf_list_t* listB;

		taf_list_split(*source_head, &listA, &listB);

		wlc_taf_sort_list(&listA, ordering, now_time);
		wlc_taf_sort_list(&listB, ordering, now_time);

		*source_head = wlc_taf_merge_list(&listA, &listB, ordering, now_time);
	}
}

static taf_list_t*
taf_list_new(taf_scb_cubby_t *scb_taf, taf_list_type_t type)
{
	taf_list_t* list = NULL;

	if (scb_taf) {
		list = TAF_LIST(scb_taf, type);

		TAF_ASSERT(list->scb_taf == NULL && list->next == NULL && list->prev == NULL);

		list->scb_taf = scb_taf;
		list->type = type;
	}
	return list;
}

static void
taf_move_list_item(taf_scb_cubby_t* scb_taf, taf_method_info_t* dest_method)
{
	taf_list_t*  item;
	taf_method_info_t* source_method = scb_taf->method;
	taf_scheduler_method_group source_group = source_method ?
		source_method->group : TAF_GROUP_UNDEFINED;
	taf_scheduler_method_group dest_group = dest_method->group;
	taf_link_state_t prev_state[TAF_NUM_SCHED_SOURCES][TAF_MAXPRIO];
	int tid;
	taf_source_type_t s_idx;
	taf_list_type_t type;

	if (source_method == dest_method) {
		return;
	}

	for (type = TAF_TYPE_DL; type < TAF_NUM_LIST_TYPES; type++) {
		if (source_method) {
			item = taf_list_find(scb_taf, type);

			if (item == NULL) {
				WL_TAFT2(dest_method->taf_info, "item "MACF"%s NOT found\n",
					TAF_ETHERC(scb_taf), TAF_TYPE(type));
				continue;
			}
			WL_TAFT2(dest_method->taf_info, "item "MACF"%s found\n",
				TAF_ETHERC(scb_taf), TAF_TYPE(type));
		} else {
			item = taf_list_new(scb_taf, type);
			WL_TAFT2(dest_method->taf_info, "item "MACF"%s NEW\n",
				TAF_ETHERC(scb_taf), TAF_TYPE(type));
		}

		WL_TAFT1(dest_method->taf_info,
			"removing "MACF"%s from %s and add to %s prio %d\n",
		        TAF_ETHERC(scb_taf), TAF_TYPE(type), TAF_SCHED_NAME(source_method),
		        TAF_SCHED_NAME(dest_method), scb_taf->score[type]);

		if ((type == TAF_TYPE_DL) && source_method && (source_group != dest_group) &&
			(source_group != TAF_GROUP_UNDEFINED)) {

			for (s_idx = 0; s_idx < TAF_NUM_SCHED_SOURCES; s_idx++) {
				for (tid = 0; tid < TAF_NUMPRIO(s_idx); tid++) {

					prev_state[s_idx][tid] =
						scb_taf->info.linkstate[s_idx][tid];
					taf_link_state_method_update(source_method,
						scb_taf->scb, tid, s_idx, TAF_LINKSTATE_REMOVE);
					TAF_ASSERT(scb_taf->info.linkstate[s_idx][tid] ==
						TAF_LINKSTATE_NONE);
				}
			}
		}
		if (source_method) {
			wlc_taf_list_remove(taf_get_list_head_ptr(source_method), item);
		}
		wlc_taf_list_append(taf_get_list_head_ptr(dest_method), item);

		if (type == TAF_TYPE_DL) {
			scb_taf->method = dest_method;
			for (s_idx = 0; s_idx < TAF_NUM_SCHED_SOURCES; s_idx++) {
				for (tid = 0; tid < TAF_NUMPRIO(s_idx); tid++) {
					taf_link_state_t  newstate;
					if ((source_group) != dest_group &&
						(source_group != TAF_GROUP_UNDEFINED)) {
						newstate = prev_state[s_idx][tid];
					} else {
						newstate = TAF_LINKSTATE_SOFT_RESET;
					}
					taf_link_state_method_update(dest_method, scb_taf->scb,
						tid, s_idx, newstate);
				}
			}
			taf_scb_state_method_update(dest_method, scb_taf->scb,
				TAF_SOURCE_UNDEFINED, NULL, TAF_SCBSTATE_RESET);
		}
		if (!source_method) {
			break;
		}
	}
}

/*
 * Watchdog timer. Called approximatively once a second.
 */
static void
taf_watchdog(void *handle)
{
	wlc_taf_info_t *taf_info = handle;
	taf_scheduler_kind type;
	bool reset = FALSE;
	bool reset_hard = FALSE;
	uint32 total_traffic = 0;

	BCM_REFERENCE(reset_hard);
	BCM_REFERENCE(reset);

	if (taf_info == NULL || !taf_info->enabled || taf_info->bypass) {
		return;
	}

	/* check for deferred reset, before anything else */
	if (taf_info->deferred_reset != TAF_DEFER_RESET_NONE) {
		bool hard_reset = taf_info->deferred_reset == TAF_DEFER_RESET_HARD;

		WL_TAFT1(taf_info, "deferred %s reset request (total traffic %u)\n",
			hard_reset ? "hard" : "soft", taf_total_traffic_pending(taf_info));

		wlc_taf_reset_scheduling(taf_info, ALLPRIO, hard_reset);
		if (hard_reset) {
			return;
		}
	}

	if (taf_info->release_count == 0) {
		taf_info->watchdog_data_stall++;
		WL_TAFT3(taf_info, "count (%u)\n", taf_info->watchdog_data_stall);
	} else {
		taf_info->watchdog_data_stall = 0;
	}

	if ((taf_info->watchdog_data_stall > 1) && !wlc_taf_scheduler_blocked(taf_info)) {

		wlc_taf_schedule(taf_info, TAF_DEFAULT_UNIFIED_TID, NULL, FALSE);

		if (taf_info->release_count > 0) {
#ifdef TAF_DEBUG_VERBOSE
			taf_memtrace_dump(taf_info);
#endif // endif
			WL_ERROR(("wl%u %s: scheduler resumed\n", WLCWLUNIT(TAF_WLCT(taf_info)),
				__FUNCTION__));
			taf_info->watchdog_data_stall = 0;
		}
	}

	if (taf_info->watchdog_data_stall_limit &&
			taf_info->watchdog_data_stall >= taf_info->watchdog_data_stall_limit) {
		reset = TRUE;
		reset_hard = (total_traffic = taf_total_traffic_pending(taf_info)) > 0;
		taf_info->watchdog_data_stall = 0;
	}

	if (reset) {
		if (reset_hard) {
			WL_ERROR(("wl%u %s: do hard reset scheduling (total traffic %u) due to "
				"potential stall (wd limit %u)\n",
				WLCWLUNIT(TAF_WLCT(taf_info)), __FUNCTION__, total_traffic,
				taf_info->watchdog_data_stall_limit));
#ifdef TAF_DEBUG_VERBOSE
			taf_memtrace_dump(taf_info);
#endif // endif
		}
		wlc_taf_reset_scheduling(taf_info, ALLPRIO, reset_hard);
	}
	/* process watchdog for scheduler methods */
	for (type = TAF_SCHED_FIRST_INDEX; type < TAF_SCHED_LAST_INDEX; type++) {
		taf_method_info_t* method = taf_get_method_info(taf_info, type);

		if (method && method->funcs.watchdog_fn) {
			method->funcs.watchdog_fn(method);
		}
	}
	taf_info->release_count = 0;
	return;
}

static taf_method_info_t*
taf_get_method_by_name(wlc_taf_info_t* taf_info, const char* cmd)
{
	taf_scheduler_kind type;

	while (*cmd == ' ') {
		cmd++;
	}
	for (type = TAF_SCHED_FIRST_INDEX; type < TAF_SCHED_LAST_INDEX; type++) {
		taf_method_info_t* method = taf_get_method_info(taf_info, type);

		if (!method) {
			continue;
		}
		if (!strcmp(cmd, TAF_SCHED_NAME(method)) ||
			(cmd[0] >= '0' && cmd[0] <= '9' && bcm_strtoul(cmd, NULL, 0) == type)) {
			return method;
		}
	}
	return NULL;
}

bool BCMFASTPATH
wlc_taf_enabled(wlc_taf_info_t* taf_info)
{
	return taf_enabled(taf_info);
}

bool BCMFASTPATH
wlc_taf_in_use(wlc_taf_info_t* taf_info)
{
	return taf_in_use(taf_info);
}

bool BCMFASTPATH
wlc_taf_ul_enabled(wlc_taf_info_t* taf_info)
{
	return taf_ul_enabled(taf_info);
}

bool BCMFASTPATH
wlc_taf_scheduler_running(wlc_taf_info_t* taf_info)
{
	return taf_scheduler_running(taf_info);
}

bool BCMFASTPATH
wlc_taf_nar_in_use(wlc_taf_info_t* taf_info, bool * enabled)
{
#if TAF_ENABLE_NAR
	bool taf_enabled = FALSE;
	bool taf_bypass = FALSE;

	if (taf_info) {
		taf_enabled = taf_info->enabled;
		taf_bypass = taf_info->bypass;
	}
	if (enabled) {
		*enabled = taf_enabled;
	}
	return taf_enabled && !taf_bypass;
#else
	if (enabled) {
		*enabled = FALSE;
	}
	return FALSE;
#endif /* TAF_ENABLE_NAR */
}

bool BCMFASTPATH
wlc_taf_scheduler_blocked(wlc_taf_info_t* taf_info)
{
	bool blocked = taf_scheduler_running(taf_info);

#if TAF_ENABLE_SQS_PULL
	blocked = blocked || (taf_info->op_state != TAF_SCHEDULE_VIRTUAL_PACKETS);
#endif /* TAF_ENABLE_SQS_PULL */

	return blocked;
}

static INLINE uint32 BCMFASTPATH
taf_set_force(wlc_taf_info_t* taf_info, struct scb* scb, int tid)
{
	taf_scb_cubby_t* scb_taf = *SCB_TAF_CUBBY_PTR(taf_info, scb);

	if (taf_info->force_time > 0) {
		scb_taf->force |= (1 << tid);

		WL_TAFM4(scb_taf->method, "setting force option to "MACF" tid %u\n",
			TAF_ETHERC(scb_taf), tid);
	}

	return scb_taf->force;
}

uint16
wlc_taf_traffic_active(wlc_taf_info_t* taf_info, struct scb* scb)
{
	uint32 traffic_active = 0;

	wlc_taf_scb_state_update(taf_info, scb, TAF_NO_SOURCE, &traffic_active,
		TAF_SCBSTATE_GET_TRAFFIC_ACTIVE);

	return (uint16)traffic_active;
}

static INLINE bool BCMFASTPATH
taf_schedule(taf_method_info_t* method, taf_release_context_t * context)
{
	wlc_taf_info_t* taf_info = method->taf_info;
	bool finished = FALSE;
	bool  (*scheduler_fn) (wlc_taf_info_t *, taf_release_context_t *, void *);

	scheduler_fn = method->funcs.scheduler_fn;

	if (!scheduler_fn) {
		return FALSE;
	}

	finished = scheduler_fn(taf_info, context, method);
	return finished;
}

#ifdef WLTAF_PUSH_SCHED_AVAILABLE
static INLINE bool
taf_push_schedule(wlc_taf_info_t *taf_info, bool force, taf_release_context_t* context)
{
	bool did_schedule = FALSE;

	struct scb * scb = context->scb;
	int tid = context->tid;
	taf_scb_cubby_t* scb_taf = scb ? *SCB_TAF_CUBBY_PTR(taf_info, scb) : NULL;
	taf_method_info_t* method;

	if (!scb_taf) {
		return FALSE;
	}
	TAF_ASSERT(tid >= 0 && tid < NUMPRIO;
	method = scb_taf->method;
	TAF_ASSERT(method);

	if (force) {
		*method->ready_to_schedule |= (1 << tid);
		WL_TAFM2(method, "force r_t_s 0x%x\n", *method->ready_to_schedule);
	}
	if (method->scheme == TAF_SINGLE_PUSH_SCHEDULING) {
		/* this is packet push scheduling */

		if (*method->ready_to_schedule & (1 << tid)) {
			if (taf_schedule(method, context)) {
				did_schedule = TRUE;
			}
		}
	}
	return did_schedule;
}
#endif /* WLTAF_PUSH_SCHED_AVAILABLE */

bool BCMFASTPATH
wlc_taf_schedule(wlc_taf_info_t *taf_info, int tid, struct scb *scb, bool force)
{
	bool finished = FALSE;
	taf_release_context_t context = {0};
	taf_method_info_t* method;
	taf_scheduler_kind type;
	bool did_schedule = FALSE;
	wlc_info_t* wlc = TAF_WLCT(taf_info);
#ifdef TAF_DBG
	char* trig_msg = NULL;
#endif /* TAF_DBG */

	if (taf_is_bypass(taf_info)) {
		return FALSE;
	}

	/* check for deferred reset, before TAF scheduler actually starts */
	if (taf_info->deferred_reset != TAF_DEFER_RESET_NONE) {
		bool hard_reset = taf_info->deferred_reset == TAF_DEFER_RESET_HARD;

		WL_TAFT1(taf_info, "deferred %s reset request (total traffic %u)\n",
			hard_reset ? "hard" : "soft", taf_total_traffic_pending(taf_info));

		wlc_taf_reset_scheduling(taf_info, ALLPRIO, hard_reset);
	}

	if (!wlc->pub->up) {
		WL_INFORM(("wl%u %s: not up\n", WLCWLUNIT(wlc), __FUNCTION__));
		return FALSE;
	}

	if (taf_info->scheduler_inhibit) {
		taf_info->scheduler_defer = TRUE;

		if (taf_info->op_state != TAF_MARKUP_REAL_PACKETS) {
			return TRUE;
		}
	}

#if TAF_ENABLE_SQS_PULL
	/* for normal trigger cases, handle exceptions */
	if ((tid >= 0) && (tid < TAF_MAXPRIO)) {
		if ((scb == NULL) && (taf_info->op_state == TAF_MARKUP_REAL_PACKETS)) {
			WL_TAFT2(taf_info, "skipping NULL scb tid %u in markup phase\n", tid);
			return TRUE;
		}
	}

#ifdef TAF_DBG
	if (taf_info->sqs_state_fault > taf_info->sqs_state_fault_prev) {
		taf_info->sqs_state_fault_prev = taf_info->sqs_state_fault;
		WL_ERROR(("wl%u %s: sqs_state_fault count %u\n",
			WLCWLUNIT(wlc), __FUNCTION__, taf_info->sqs_state_fault));
	}
#endif /* TAF_DBG */
#endif /* TAF_ENABLE_SQS_PULL */
	TAF_ASSERT(taf_info->scheduler_nest == 0);

	if (taf_info->scheduler_nest != 0) {
		return TRUE;
	}
	taf_info->scheduler_nest ++;

	if (force && scb && tid >= 0 && tid < TAF_MAXPRIO) {
		/* set the force flag
		*/
		taf_set_force(taf_info, scb, tid);
	}

	context.now_time = taf_timestamp(wlc);

#if TAF_ENABLE_SQS_PULL
	if (tid == TAF_SQS_TRIGGER_TID && taf_info->op_state != TAF_SCHEDULE_VIRTUAL_PACKETS) {
#ifdef TAF_DBG
		uint32 pull_complete_elapsed = taf_info->virtual_complete_time ?
			context.now_time - taf_info->virtual_complete_time : 0;

		if (pull_complete_elapsed > 50000) {
			WL_ERROR(("wl%u %s: pkt pull duration %u\n", WLCWLUNIT(wlc),
				__FUNCTION__, pull_complete_elapsed));
			taf_info->virtual_complete_time = context.now_time;
		}
#endif /* TAF_DBG */
		goto taf_schedule_end;
	}
#else
	TAF_ASSERT(taf_info->op_state == TAF_SCHEDULE_REAL_PACKETS);
#endif /* TAF_ENABLE_SQS_PULL */

	context.tid = tid;
	context.scb = scb;

	if ((tid >= 0) && (did_schedule = taf_push_schedule(taf_info, force, &context))) {
		goto taf_schedule_end;
	} /* else PULL schedule ..... */

#if TAF_ENABLE_SQS_PULL
	switch (tid) {
		case TAF_SQS_V2R_COMPLETE_TID:
			context.op_state = TAF_MARKUP_REAL_PACKETS;
#ifdef TAF_DBG
			trig_msg = "SQS v2r complete";
#endif /* TAF_DBG */
			tid = 0;
			break;
		case TAF_SQS_TRIGGER_TID:
			context.op_state = TAF_SCHEDULE_VIRTUAL_PACKETS;
#ifdef TAF_DBG
			trig_msg = "\n\nSQS";
#endif /* TAF_DBG */
			tid = 0;
			break;

		case ALLPRIO:
			tid = 0;
			/* fall through */
		default:
			context.op_state = taf_info->op_state;
#ifdef TAF_DBG
			if (context.op_state == TAF_MARKUP_REAL_PACKETS) {
				trig_msg = NULL;
			} else {
				trig_msg =  "normal";
			}
#endif /* TAF_DBG */
			break;
	}
#else
#ifdef TAF_DBG
	trig_msg =  "normal";
#endif /* TAF_DBG */
	if (tid == ALLPRIO) {
		tid = 0;
	}
	context.tid = tid;
	context.op_state = TAF_SCHEDULE_REAL_PACKETS;
#endif /* TAF_ENABLE_SQS_PULL */

	/* everything from here is ordered scheduling (packet pull scheduling) */
	for (type = TAF_SCHED_FIRST_INDEX; type < TAF_SCHED_LAST_INDEX; type++) {
		method = taf_get_method_info(taf_info, type);

		if (!method || method->scheme == TAF_SINGLE_PUSH_SCHEDULING) {
			continue;
		}
		if (*method->ready_to_schedule & (1 << tid)) {
#ifdef TAF_DBG
			if (trig_msg) {
				WL_TAFT1(taf_info, "%s (tid %d, in transit %u)\n", trig_msg, tid,
					TXPKTPENDTOT(wlc));
				trig_msg = NULL;
			}
#endif /* TAF_DBG */
			finished = taf_schedule(method, &context);
			did_schedule = TRUE;
		}
		if (finished) {
			break;
		}
	}

taf_schedule_end:
#if TAF_ENABLE_SQS_PULL
	if (context.virtual_release) {
		taf_info->op_state = TAF_MARKUP_REAL_PACKETS;
#ifdef TAF_DBG
		taf_info->virtual_complete_time = context.now_time;
#endif // endif
		wlc_sqs_eops_rqst();
		taf_info->eops_rqst++;
	}
	if (did_schedule && taf_info->op_state == TAF_SCHEDULE_VIRTUAL_PACKETS) {
		taf_info->scheduler_index ++;
	}
#else
	if (did_schedule) {
		taf_info->scheduler_index ++;
	}
#endif /* TAF_ENABLE_SQS_PULL */

	/* check elapsed time since last scheduler for stall debugging */
	if (did_schedule) {
		taf_info->last_scheduler_run = context.now_time;
	}
	else {
		uint32 scheduler_elapsed = taf_info->last_scheduler_run ?
			context.now_time - taf_info->last_scheduler_run : 0;

		/* STALL recovery */
		if (scheduler_elapsed > 500000) {
			if (taf_info->watchdog_data_stall < taf_info->watchdog_data_stall_limit) {

				WL_INFORM(("wl%u %s: no TAF scheduler ran since %u ms\n",
					WLCWLUNIT(wlc), __FUNCTION__, scheduler_elapsed / 1000));
				taf_info->last_scheduler_run = context.now_time;
			}
		}
	}
	if (context.status == TAF_CYCLE_LEGACY) {
		TAF_ASSERT(tid >= 0 && tid < NUMPRIO && scb != NULL);
		WL_TAFT2(taf_info, MACF" tid %u revert to fallback default data path\n",
			TAF_ETHERS(scb), tid);
	}

	/* Drain the TXQs after the scheduler run */
	if (did_schedule && taf_info->scheduler_inhibit == 0 && wlc->active_queue != NULL &&
		WLC_TXQ_OCCUPIED(wlc)) {
		wlc_send_q(wlc, wlc->active_queue);
	}

	TAF_ASSERT(taf_info->scheduler_nest == 1);

	taf_info->scheduler_nest --;

	return (context.status != TAF_CYCLE_LEGACY);
}
/*
 * SCB cubby functions, called when an SCB is created or destroyed.
 */
static uint
taf_scbcubby_secsz(void *handle, struct scb *scb)
{
	wlc_taf_info_t *taf_info = handle;

	uint size = 0;
	if (taf_info->enabled && !SCB_INTERNAL(scb)) {
		size = sizeof(taf_scb_cubby_t);
	}
	return size;
}
static int
taf_scbcubby_init(void *handle, struct scb *scb)
{
	wlc_taf_info_t *taf_info = handle;
	wlc_info_t *wlc = TAF_WLCT(taf_info);
	taf_scb_cubby_t *scb_taf = NULL;
	taf_method_info_t *method;

	/* Init function always called after SCB reset */
	TAF_ASSERT(*SCB_TAF_CUBBY_PTR(taf_info, scb) == NULL);

	scb_taf = wlc_scb_sec_cubby_alloc(wlc, scb, taf_scbcubby_secsz(handle, scb));
	*SCB_TAF_CUBBY_PTR(taf_info, scb) = scb_taf;

	if (scb_taf != NULL) {
		WL_TAFT2(taf_info, "TAF cby "MACF" allocated\n", TAF_ETHERS(scb));
		scb_taf->scb = scb;
		scb_taf->method = NULL;

		method = taf_get_method_info(taf_info, taf_info->default_scheduler);

		TAF_ASSERT(method);

		/* Just put the cubby at the begining of the list as bsscfg is unknown */
		scb_taf->next = taf_info->head;
		taf_info->head = scb_taf;

		taf_set_cubby_method(scb_taf, method, taf_info->default_score);
		wlc_taf_scb_state_update(taf_info, scb, TAF_NO_SOURCE, NULL, TAF_SCBSTATE_INIT);
	}

	return BCME_OK;
}

/*
 * The SCB cubby is saved to be reused later if the station re-associate, or for another STA.
 */
static void
taf_scbcubby_exit(void *handle, struct scb *scb)
{
	wlc_taf_info_t *taf_info = handle;
	taf_scb_cubby_t* scb_taf = *SCB_TAF_CUBBY_PTR(taf_info, scb);

	/* If we do have a cubby, clean up. */
	if (scb_taf) {
		taf_scb_cubby_t *prev, *curr;
		taf_source_type_t s_idx;

		WL_TAFT2(taf_info, MACF" cubby exit\n", TAF_ETHERC(scb_taf));
#ifdef TAF_PKTQ_LOG
		wlc_taf_dpstats_free(TAF_WLCT(taf_info), scb);
#endif // endif
		for (s_idx = 0; s_idx < TAF_NUM_SCHED_SOURCES; s_idx++) {
			int tid;
			for (tid = 0; tid < TAF_NUMPRIO(s_idx); tid++) {
				taf_link_state_method_update(scb_taf->method, scb, tid, s_idx,
					TAF_LINKSTATE_REMOVE);
			}
		}
		wlc_taf_scb_state_update(taf_info, scb, TAF_NO_SOURCE, NULL, TAF_SCBSTATE_EXIT);

		wlc_taf_list_delete(scb_taf, TAF_TYPE_DL);
#if TAF_ENABLE_UL
		wlc_taf_list_delete(scb_taf, TAF_TYPE_UL);
#endif // endif

		/* Trick to modify head seamlessly */
		prev = (taf_scb_cubby_t *) &taf_info->head;

		for (curr = taf_info->head; curr; prev = curr, curr = curr->next) {
			if (scb_taf == curr) {
				/* Found! remove it from the head list */
				prev->next = curr->next;
				break;
			}
		}
		TAF_ASSERT(curr);

		wlc_scb_sec_cubby_free(TAF_WLCT(taf_info), scb, scb_taf);
		*SCB_TAF_CUBBY_PTR(taf_info, scb) = NULL;
	}
}

/* do not call multiple times for every tid - rather use tid=ALLPRIO to reset all tid */
bool BCMFASTPATH
wlc_taf_reset_scheduling(wlc_taf_info_t *taf_info, int tid, bool hardreset)
{
	taf_scb_cubby_t *scb_taf;
	bool taf_running;

	if (!taf_info || !taf_info->enabled) {
		return FALSE;
	}
	taf_running = taf_scheduler_running(taf_info);

	WL_TAFT1(taf_info, "tid %d %s%s\n", tid, hardreset ? "hard" : "soft",
	       taf_running ? ", TAF running":"");

	if (taf_running && taf_info->deferred_reset != TAF_DEFER_RESET_HARD) {
		/* TAF is running, defer reset */
		taf_info->deferred_reset = hardreset ? TAF_DEFER_RESET_HARD : TAF_DEFER_RESET_SOFT;
		return FALSE;
	}
	scb_taf = taf_info->head;
	taf_info->deferred_reset = TAF_DEFER_RESET_NONE;

	taf_info->scheduler_defer = FALSE;

	if (hardreset) {
		taf_info->scheduler_inhibit = 0;
	}

	while (scb_taf) {
		taf_source_type_t s_idx;
		int ltid = (tid == ALLPRIO) ? 0 : tid;

		do {
			for (s_idx = 0; s_idx < TAF_NUM_SCHED_SOURCES; s_idx++) {
				taf_link_state_method_update(scb_taf->method, scb_taf->scb, ltid,
					s_idx, hardreset ?
					TAF_LINKSTATE_HARD_RESET : TAF_LINKSTATE_SOFT_RESET);
			}
		} while ((tid == ALLPRIO) && (++ltid < TAF_MAXPRIO));

		if (hardreset) {
			taf_scb_state_method_update(scb_taf->method, scb_taf->scb,
				TAF_SOURCE_UNDEFINED, NULL, TAF_SCBSTATE_RESET);
		}
		scb_taf = scb_taf->next;
	}
	if (hardreset) {
		WL_ERROR(("wl%u %s: hard reset\n", WLCWLUNIT(TAF_WLCT(taf_info)), __FUNCTION__));
#if TAF_ENABLE_SQS_PULL
		WL_ERROR(("wl%u %s: total pull requests %u, eops request %u, op state %d\n",
			WLCWLUNIT(TAF_WLCT(taf_info)), __FUNCTION__,
			taf_info->total_pull_requests, taf_info->eops_rqst, taf_info->op_state));
#endif /* TAF_ENABLE_SQS_PULL */

		wlc_taf_sched_state(taf_info, NULL, tid, 0, TAF_NO_SOURCE, TAF_SCHED_STATE_RESET);
		taf_info->watchdog_data_stall = 0;

		if (!taf_scheduler_running(taf_info)) {
			wlc_taf_schedule(taf_info, TAF_DEFAULT_UNIFIED_TID, NULL, FALSE);
		}
	}
	return TRUE;
}

#ifdef TAF_DBG
static int
taf_dump(void *handle, struct bcmstrbuf *b)
{
	wlc_taf_info_t *taf_info = handle;
	taf_scheduler_kind type = TAF_SCHEDULER_START;
	char *args = TAF_WLCT(taf_info)->dump_args;
	bool quiet = FALSE;

	if (taf_info->enabled && args != NULL) {
		taf_method_info_t* method = NULL;
		char* p = bcmstrtok(&args, " ", 0);

#ifdef TAF_DEBUG_VERBOSE
		if (p && p[0] == '-' && p[1] == 'q' && p[2] == 0) {
			quiet = TRUE;
		} else
#endif // endif
		if (p && p[0] == '-' && (method = taf_get_method_by_name(taf_info, p+1)) &&
			method->funcs.dump_fn) {
			return method->funcs.dump_fn(method, b);
		}
		if (!quiet && method == NULL) {
			return BCME_BADARG;
		}
	}

	if (!quiet) {
		bcm_bprintf(b, "taf is %sabled%s\n", taf_info->enabled ? "en" : "dis",
			taf_info->bypass ? " BUT BYPASSED (not in use)" : "");
#if TAF_ENABLE_UL
		bcm_bprintf(b, "taf UL is %sabled\n", taf_info->ul_enabled ? "en" : "dis");
#endif // endif
	}

	if (!taf_info->enabled) {
		return BCME_OK;
	}

	while (!quiet && type < TAF_SCHED_LAST_INDEX) {
		taf_method_info_t* method = taf_get_method_info(taf_info, type);

		if (!method) {
			type++;
			continue;
		}
		bcm_bprintf(b, "\nmethod '%s' is available (scheduler index %u)",
			TAF_SCHED_NAME(method), type);

		if (taf_info->default_scheduler == type) {
			bcm_bprintf(b, " which is default for new stations");
		}
		if (method->funcs.dump_fn) {
			bcm_bprintf(b, "; for detailed info, please do 'dump %s'",
				TAF_DUMP_NAME(method));
		}
		bcm_bprintf(b, "\n");
		taf_dump_list(method, b);
		type++;
	}
#ifdef TAF_DEBUG_VERBOSE
	if (taf_trace_buf && b->size > PRINTF_BUFLEN)
	{
		const uint32 buf_len = taf_trace_output_len;
		int index = taf_trace_index;
		char end;

		taf_trace_buf[index++] = 0;

		/* handle limited length ioctl buffer for dump */
		if (buf_len > (b->size - 4)) {
			bcm_bprintf(b, "...");
			index += (buf_len - (b->size - 1));
		}
		if (index >= buf_len) {
			index -= buf_len;
		}
		if (taf_trace_buf[index] == 0) {
			index = 0;
		}
		end = taf_trace_buf[buf_len - 1];

		if (end) {
			taf_trace_buf[buf_len - 1] = 0;
			bcm_bprintf(b, "%s%c", taf_trace_buf + index, end);
			taf_trace_buf[buf_len - 1] = end;
		}

		bcm_bprintf(b, "%s", taf_trace_buf);
		taf_trace_index = 0;
		memset(taf_trace_buf, 0, taf_trace_buf_len);
	}
#endif /* TAF_DEBUG_VERBOSE */
	return BCME_OK;
}
#endif /* TAF_DBG */

/*
 * taf_up() - interface coming up.
 */
static int
taf_up(void *handle)
{
	wlc_taf_info_t *taf_info = handle;
	taf_scheduler_kind type;

	taf_init(taf_info);

	for (type = TAF_SCHED_FIRST_INDEX; type < TAF_SCHED_LAST_INDEX; type++) {
		int index = TAF_SCHED_INDEX(type);

		if (taf_scheduler_definitions[index]->up_fn) {
			void* sch_context = taf_get_method_info(taf_info, type);

			if (sch_context) {
				taf_scheduler_definitions[index]->up_fn(sch_context);
			}
		}
	}

	return BCME_OK;
}

/*
 * taf_down() - interface going down.
 */
static int
taf_down(void *handle)
{
	wlc_taf_info_t *taf_info = handle;
	taf_scheduler_kind type;
	int callbacks = 0;

	for (type = TAF_SCHED_FIRST_INDEX; type < TAF_SCHED_LAST_INDEX; type++) {
		int index = TAF_SCHED_INDEX(type);

		if (taf_scheduler_definitions[index]->down_fn) {
			void* sch_context = taf_get_method_info(taf_info, type);

			if (sch_context) {
				callbacks += taf_scheduler_definitions[index]->down_fn(sch_context);
			}
		}
	}

	return callbacks;
}

/*
 * wlc_taf_attach() - attach function, called from wlc_attach_module().
 *
 * Allocate and initialise our context structure, register the module, register the txmod.
 *
 */
wlc_taf_info_t *
BCMATTACHFN(wlc_taf_attach)(wlc_info_t * wlc)
{
	wlc_taf_info_t *taf_info;
	int status;
	taf_scheduler_kind type;
	scb_cubby_params_t cubby_params;

	WL_TAFW(wlc, "\n");

	/* Allocate and initialise our main structure. */
	taf_info = MALLOCZ(wlc->pub->osh, sizeof(*taf_info));
	if (!taf_info) {
		return NULL;
	}

	/* Save backlink to wlc */
	TAF_WLCT(taf_info) = wlc;

	status = wlc_module_register(wlc->pub, taf_iovars, "taf", taf_info,
	taf_doiovar, taf_watchdog,  taf_up, taf_down);

	if (status != BCME_OK) {
		MFREE(TAF_WLCT(taf_info)->pub->osh, taf_info, sizeof(*taf_info));
		return NULL;
	}

	taf_info->enabled = FALSE;

#if !TAF_VALID_CONFIG
	/* not a supported build configuration, proceed no more */
	return taf_info;
#endif // endif

	if (D11REV_LT(wlc->pub->corerev, 40)) {
		/* only support on latest chipsets, proceed no more */
		return taf_info;
	}

#if TAF_ENABLE_SQS_PULL
	taf_info->op_state = TAF_SCHEDULE_VIRTUAL_PACKETS;
#else
	taf_info->op_state = TAF_SCHEDULE_REAL_PACKETS;
#endif /* TAF_ENABLE_SQS_PULL */

	taf_info->watchdog_data_stall_limit = TAF_WD_STALL_RESET_LIMIT;

#ifdef TAF_DBG
	wlc_dump_register(wlc->pub, "taf", taf_dump, taf_info);
#endif /* TAF_DBG */

	/*
	* set up an scb cubby - this returns an offset, or -1 on failure.
	*/
	bzero(&cubby_params, sizeof(cubby_params));

	cubby_params.context = taf_info;
	cubby_params.fn_init = taf_scbcubby_init;
	cubby_params.fn_deinit = taf_scbcubby_exit;
	cubby_params.fn_secsz = taf_scbcubby_secsz;
	taf_info->scb_handle = wlc_scb_cubby_reserve_ext(wlc, sizeof(taf_scb_cubby_t *),
		&cubby_params);
	if (taf_info->scb_handle < 0) {
		goto exitfail;
	}

	taf_info->default_scheduler = TAF_DEFAULT_SCHEDULER;
	taf_info->default_score = TAF_SCORE_MIN;

	/* the default amount of traffic (in microsecs) to send when forced,
	 * this helps to prevent a link from stalling completely
	 */
	taf_info->force_time = TAF_TIME_FORCE_DEFAULT;

#if TAF_ENABLE_SQS_PULL
	/* this is enabled later so default FALSE */
	wl_bus_taf_scheduler_config(wlc->wl, FALSE);
#endif /* TAF_ENABLE_SQS_PULL */

#if TAF_ENABLE_AMPDU
	taf_info->source_handle_p[TAF_SOURCE_AMPDU] = (void*)&wlc->ampdu_tx;
	taf_info->funcs[TAF_SOURCE_AMPDU].scb_h_fn = wlc_ampdu_get_taf_scb_info;
	taf_info->funcs[TAF_SOURCE_AMPDU].tid_h_fn = wlc_ampdu_get_taf_scb_tid_info;
	taf_info->funcs[TAF_SOURCE_AMPDU].pktqlen_fn = wlc_ampdu_get_taf_scb_tid_pktlen;
	taf_info->funcs[TAF_SOURCE_AMPDU].release_fn = wlc_ampdu_taf_release;
	taf_info->funcs[TAF_SOURCE_AMPDU].open_close_fn = NULL;
#endif /* TAF_ENABLE_AMPDU */
#if TAF_ENABLE_NAR
	taf_info->source_handle_p[TAF_SOURCE_NAR] = (void*)&wlc->nar_handle;
	taf_info->funcs[TAF_SOURCE_NAR].scb_h_fn = wlc_nar_get_taf_scb_info;
	taf_info->funcs[TAF_SOURCE_NAR].tid_h_fn = wlc_nar_get_taf_scb_prec_info;
	taf_info->funcs[TAF_SOURCE_NAR].pktqlen_fn = wlc_nar_get_taf_scb_prec_pktlen;
	taf_info->funcs[TAF_SOURCE_NAR].release_fn = wlc_nar_taf_release;
	taf_info->funcs[TAF_SOURCE_NAR].open_close_fn = NULL;
#endif /* TAF_ENABLE_NAR */
#if TAF_ENABLE_SQS_PULL
	taf_info->source_handle_p[TAF_SOURCE_HOST_SQS] = wlc_sqs_taf_get_handle(wlc);
	taf_info->funcs[TAF_SOURCE_HOST_SQS].scb_h_fn = wlc_sqs_taf_get_scb_info;
	taf_info->funcs[TAF_SOURCE_HOST_SQS].tid_h_fn = wlc_sqs_taf_get_scb_tid_info;
	taf_info->funcs[TAF_SOURCE_HOST_SQS].pktqlen_fn = wlc_sqs_taf_get_scb_tid_pkts;
	taf_info->funcs[TAF_SOURCE_HOST_SQS].release_fn = wlc_sqs_taf_release;
	taf_info->funcs[TAF_SOURCE_HOST_SQS].open_close_fn = NULL;
#endif /* TAF_ENABLE_SQS_PULL */
#if TAF_ENABLE_UL
	taf_info->ul_enabled = FALSE; /* disabled by default */
	taf_info->source_handle_p[TAF_SOURCE_UL] = (void*)&wlc->ulmu;
	taf_info->funcs[TAF_SOURCE_UL].scb_h_fn = wlc_ulmu_taf_get_scb_info;
	taf_info->funcs[TAF_SOURCE_UL].tid_h_fn = wlc_ulmu_taf_get_scb_tid_info;
	taf_info->funcs[TAF_SOURCE_UL].pktqlen_fn = wlc_ulmu_taf_get_pktlen;
	taf_info->funcs[TAF_SOURCE_UL].release_fn = wlc_ulmu_taf_release;
	taf_info->funcs[TAF_SOURCE_UL].open_close_fn = wlc_ulmu_taf_bulk;
#endif /* TAF_ENABLE_UL */

	taf_info->bulk_commit = TRUE;
	taf_info->super = 0;

#if TAF_ENABLE_MU_TX
	/* enable super-scheduling by default for MU */
	taf_info->super |= TAF_TECH_DL_MU_MASK;

	taf_info->use_sampled_rate_sel = TAF_TECH_DL_VHMUMIMO_MASK | TAF_TECH_DL_HEMUMIMO_MASK;

	taf_info->mu = TAF_TECH_DL_OFDMA_MASK | TAF_TECH_DL_VHMUMIMO_MASK |
		TAF_TECH_DL_HEMUMIMO_MASK;

	taf_info->mu_g_enable_mask[TAF_TECH_DL_VHMUMIMO] = ~0;
	taf_info->mu_g_enable_mask[TAF_TECH_DL_HEMUMIMO] = ~0;
	taf_info->mu_g_enable_mask[TAF_TECH_DL_OFDMA] = ~0;

#if TAF_ENABLE_MU_BOOST
	taf_info->mu_boost = TAF_MUBOOST_OFF;
#endif // endif

#if TAF_ENABLE_UL
	taf_info->super |= TAF_TECH_UL_OFDMA_MASK;
	taf_info->mu |= TAF_TECH_UL_OFDMA_MASK;
	taf_info->mu_g_enable_mask[TAF_TECH_UL_OFDMA] = ~0;
#endif /* TAF_ENABLE_UL */
#else
	taf_info->use_sampled_rate_sel = 0;
#endif /* TAF_ENABLE_MU_TX */

	for (type = TAF_SCHED_FIRST_INDEX; type < TAF_SCHED_LAST_INDEX; type++) {
		taf_method_info_t* method = NULL;
		int index = TAF_SCHED_INDEX(type);

		if (taf_scheduler_definitions[index]->attach_fn) {
			method = taf_scheduler_definitions[index]->attach_fn(taf_info, type);
		}

		if (((taf_info->scheduler_context[index] = method) == NULL) &&
			(taf_scheduler_definitions[index] != TAF_NULL_SCHEDULER)) {
			WL_ERROR(("wl%u %s: failed to attach (%d)\n", WLCWLUNIT(wlc), __FUNCTION__,
				type));
			goto exitfail;
		}
	}
#if defined(DONGLEBUILD) || defined(PKTC)
	/* enable by default */
	if (taf_do_enable(taf_info, TRUE) != BCME_OK) {
		goto exitfail;
	}
#endif /* DONGLEBUILD || PKTC */

#if defined(TAF_DEBUG_VERBOSE) && defined(DONGLEBUILD)
	taf_attach_complete = TRUE;
#endif // endif
	/* All is fine, return handle */
	return taf_info;
exitfail:
	wlc_taf_detach(taf_info);
	return NULL;
}

/*
 * wlc_taf_detach() - wlc module detach function, called from wlc_detach_module().
 *
 */
int
BCMATTACHFN(wlc_taf_detach) (wlc_taf_info_t *taf_info)
{
#if defined(TAF_DEBUG_VERBOSE) && defined(DONGLEBUILD)
	taf_attach_complete = FALSE;
#endif // endif

	if (taf_info) {
		taf_scheduler_kind type;

		for (type = TAF_SCHED_FIRST_INDEX; type < TAF_SCHED_LAST_INDEX; type++) {
			int index = TAF_SCHED_INDEX(type);

			if (taf_scheduler_definitions[index]->detach_fn) {
				void* sch_context = taf_get_method_info(taf_info, type);

				if (sch_context) {
					taf_scheduler_definitions[index]->detach_fn(sch_context);
				}
			}
			taf_info->scheduler_context[index] = NULL;
		}
		wlc_module_unregister(TAF_WLCT(taf_info)->pub, "taf", taf_info);
		MFREE(TAF_WLCT(taf_info)->pub->osh, taf_info, sizeof(*taf_info));
	}
	return BCME_OK;
}

static INLINE bool
taf_link_state_method_update(taf_method_info_t* method, struct scb* scb, int tid,
	taf_source_type_t s_idx, taf_link_state_t state)
{
	bool finished = FALSE;

	TAF_ASSERT(method->funcs.linkstate_fn);
	finished = method->funcs.linkstate_fn(method, scb, tid, s_idx, state);

	return finished;
}

void
wlc_taf_link_state(wlc_taf_info_t* taf_info, struct scb* scb, int tid,
	taf_source_index_public_t source, taf_link_state_t state)
{
	taf_scb_cubby_t* scb_taf;

	if (!taf_info || !taf_info->enabled) {
		return;
	}
	scb_taf = *SCB_TAF_CUBBY_PTR(taf_info, scb);

	if (scb_taf) {
		taf_source_type_t s_idx = wlc_taf_get_source_index(source);

		TAF_ASSERT(tid >= 0 && tid < TAF_MAXPRIO);
		if (s_idx != TAF_SOURCE_UNDEFINED) {
			taf_link_state_method_update(scb_taf->method, scb, tid, s_idx,
				state);
		}
	}
}

static INLINE int
taf_scb_state_method_update(taf_method_info_t* method, struct scb* scb, taf_source_type_t s_idx,
	void* update, taf_scb_state_t state)
{
	int ret = BCME_ERROR;

	if (method->funcs.scbstate_fn) {
		ret = method->funcs.scbstate_fn(method, scb, s_idx, update, state);
	}
	return ret;
}

static INLINE int
taf_scb_power_save(wlc_taf_info_t* taf_info, taf_scb_cubby_t* scb_taf, bool on)
{
	int result = BCME_OK;
	taf_method_info_t* owner = scb_taf->method;

	scb_taf->info.ps_mode = on;

	/* inform owner method SCB about power save */
	result = taf_scb_state_method_update(owner, scb_taf->scb, TAF_SOURCE_UNDEFINED,
		TAF_PARAM(on), TAF_SCBSTATE_POWER_SAVE);

	if (result != BCME_OK) {
		return result;
	}

	if (on) {
		/* nothing to do */
	}

	if (!on && !wlc_taf_scheduler_blocked(taf_info)) {
		/* this is PS off - need to kick the scheduler */

		WL_TAFT1(taf_info, MACF" PS mode OFF - kick scheduler\n", TAF_ETHERC(scb_taf));
		wlc_taf_schedule(taf_info, TAF_DEFAULT_UNIFIED_TID, scb_taf->scb, FALSE);
	}
	return result;

}

int
wlc_taf_scb_state_update(wlc_taf_info_t* taf_info, struct scb* scb,
	taf_source_index_public_t source, void* update, taf_scb_state_t state)
{
	taf_scb_cubby_t* scb_taf;

	if (!taf_info || !taf_info->enabled) {
		return BCME_NOTREADY;
	}
	scb_taf = *SCB_TAF_CUBBY_PTR(taf_info, scb);

	if (scb_taf) {
		int result;
		/* handle common high level scb state change */
		switch (state) {
			case TAF_SCBSTATE_POWER_SAVE:
				result = taf_scb_power_save(taf_info, scb_taf,
					update ? TRUE : FALSE);
				break;
			case TAF_SCBSTATE_TWT_SP_ENTER:
				result = taf_scb_power_save(taf_info, scb_taf, FALSE);
				break;
			case TAF_SCBSTATE_TWT_SP_EXIT:
				result = taf_scb_power_save(taf_info, scb_taf, TRUE);
				break;
			default:
				/* then pass to the method.... */
				result = taf_scb_state_method_update(scb_taf->method, scb,
					wlc_taf_get_source_index(source), update, state);
				break;
		}
		return result;
	}
	return BCME_NOTFOUND;
}

#if TAF_ENABLE_UL
void
wlc_taf_handle_ul_transition(taf_scb_cubby_t* scb_taf, bool ul_enable)
{
	TAF_ASSERT(!taf_scheduler_running(scb_taf->method->taf_info));

	if (ul_enable) {
		taf_method_info_t* method = scb_taf->method;
		/* add list entry */
		taf_list_t* item = taf_list_new(scb_taf, TAF_TYPE_UL);
		taf_list_scoring_order_t ordering = method->ordering;

		WL_TAFM1(method, MACF" adding list entry for UL\n", TAF_ETHERC(scb_taf));

		wlc_taf_list_append(taf_get_list_head_ptr(method), item);

		/* cannot be in bypass mode when UL has been admitted */
		taf_bypass(method->taf_info, FALSE);

		if (ordering != TAF_LIST_DO_NOT_SCORE) {
			if (ordering == TAF_LIST_SCORE_FIXED_INIT_MINIMUM) {
				uint32 prio = TAF_PRIO(scb_taf->score[TAF_TYPE_DL]);

				/* demote existing entry for UL prio (if it exists) */
				taf_list_demote_item(method, TAF_ULPRIO(prio));
				scb_taf->score[TAF_TYPE_UL] = TAF_ULPRIO(prio);

			} else if (ordering == TAF_LIST_SCORE_MINIMUM) {
				/* pre-init score for UL to be half of current DL score */
				scb_taf->score[TAF_TYPE_UL] = scb_taf->score[TAF_TYPE_DL] >> 1;
			} else {
				TAF_ASSERT(0);
			}
			wlc_taf_sort_list(taf_get_list_head_ptr(method), ordering,
				taf_timestamp(TAF_WLCM(method)));
		}
	} else {
		WL_TAFM1(scb_taf->method, MACF" removing list entry for UL\n", TAF_ETHERC(scb_taf));
		/* remove list entry */
		wlc_taf_list_delete(scb_taf, TAF_TYPE_UL);
	}
}
#endif /* TAF_ENABLE_UL */

static INLINE void
taf_method_rate_override(wlc_taf_info_t* taf_info, taf_scheduler_kind type,
	ratespec_t rspec, wlcband_t *band)
{
	taf_method_info_t* method = taf_get_method_info(taf_info, type);

	if (method && method->funcs.rateoverride_fn) {
		method->funcs.rateoverride_fn(method, rspec, band);
	}
}

void
wlc_taf_rate_override(wlc_taf_info_t* taf_info, ratespec_t rspec, wlcband_t *band)
{
	taf_scheduler_kind type;

	if (!taf_info || !taf_info->enabled) {
		return;
	}
	for (type = TAF_SCHEDULER_START; type < TAF_SCHED_LAST_INDEX; type++) {
		taf_method_rate_override(taf_info, type, rspec, band);
	}
}

bool
wlc_taf_txpkt_status(wlc_taf_info_t* taf_info, struct scb* scb, int tid, void* p,
	taf_txpkt_state_t status)
{
	taf_scb_cubby_t* scb_taf;
	bool ret = FALSE;
	taf_method_info_t* method;

	if (!taf_info || !taf_info->enabled || taf_info->bypass) {
		return FALSE;
	}
	TAF_ASSERT(tid < TAF_MAXPRIO);

	if (scb == NULL) {
		if (status != TAF_TXPKT_STATUS_TRIGGER_COMPLETE) {
			scb = WLPKTTAGSCBGET(p);
		} else {
			/* in case of UL, scb should not be NULL */
			TAF_ASSERT(0);
		}
	}
	if ((scb == NULL) || SCB_INTERNAL(scb)) {
		WL_TAFT4(taf_info, "scb is null\n");
		return FALSE;
	}
	if ((scb_taf = *SCB_TAF_CUBBY_PTR(taf_info, scb)) == NULL) {
		WL_TAFT4(taf_info, MACF" has no TAF cubby\n", TAF_ETHERS(scb));
		return FALSE;
	}
	method = scb_taf->method;

	ret = method->funcs.txstat_fn(method, scb_taf, tid, p, status);

	if (!ret && status == TAF_TXPKT_STATUS_PKTFREE_RESET) {
		WL_TAFT1(taf_info, MACF" tid %u %s\n", TAF_ETHERS(scb), tid,
			taf_txpkt_status_text[status]);
		ret = wlc_taf_reset_scheduling(taf_info, tid, FALSE);
	}
	if (!ret) {
		WL_TAFM4(method, MACF" unhandled, %s\n", TAF_ETHERS(scb),
			taf_txpkt_status_text[status]);
	}
	return ret;
}

static INLINE void
taf_method_bss_state_update(wlc_taf_info_t* taf_info, taf_scheduler_kind type, wlc_bsscfg_t *bsscfg,
	void* update, taf_bss_state_t state)
{
	taf_method_info_t* method = taf_get_method_info(taf_info, type);

	if (method && method->funcs.bssstate_fn) {
		method->funcs.bssstate_fn(method, bsscfg, update, state);
	}
}

void
wlc_taf_bss_state_update(wlc_taf_info_t* taf_info, wlc_bsscfg_t *bsscfg, void* update,
	taf_bss_state_t state)
{
	taf_scheduler_kind type;

	if (!taf_info || !taf_info->enabled) {
		return;
	}

	for (type = TAF_SCHEDULER_START; type < TAF_SCHED_LAST_INDEX; type++) {
		taf_method_bss_state_update(taf_info, type, bsscfg, update, state);
	}
}

#if TAF_ENABLE_SQS_PULL
static INLINE void
taf_sqs_link_init(wlc_taf_info_t* taf_info, taf_scb_cubby_t* scb_taf, int tid)
{
	if (scb_taf->info.linkstate[TAF_SOURCE_HOST_SQS][tid] == TAF_LINKSTATE_NONE &&
		TAF_WLCT(taf_info)->pub->up) {

		struct scb * scb = scb_taf->scb;

		/* XXX this is a workaround in case AMPDU ini (which is required by SQS)
		* does not exist, it has to be created on demand
		*/
		if (SCB_AMPDU(scb)) {
			void* handle = wlc_ampdu_taf_sqs_link_init(TAF_WLCT(taf_info), scb, tid);
			BCM_REFERENCE(handle);

			WL_TAFT1(taf_info, MACF" tid %u %s\n", TAF_ETHERS(scb), tid,
				handle ? " AMPDU":"");

		}
		taf_link_state_method_update(scb_taf->method, scb, tid,
			TAF_SOURCE_HOST_SQS, TAF_LINKSTATE_ACTIVE);
	}
}
#endif /* TAF_ENABLE_SQS_PULL */

void
wlc_taf_pkts_enqueue(wlc_taf_info_t* taf_info, struct scb* scb, int tid,
	taf_source_index_public_t source, int pkts)
{
#if TAF_ENABLE_SQS_PULL
	taf_scb_cubby_t* scb_taf;

	if (source == TAF_SQSHOST && taf_info->enabled &&
		(scb_taf = *SCB_TAF_CUBBY_PTR(taf_info, scb))) {

		TAF_ASSERT(tid >= 0 && tid < TAF_MAXPRIO);

		taf_sqs_link_init(taf_info, scb_taf, tid);

		scb_taf->info.traffic.map[TAF_SOURCE_HOST_SQS] |= (1 << tid);
	}
#endif /* TAF_ENABLE_SQS_PULL */
}

void BCMFASTPATH
taf_close_all_sources(wlc_taf_info_t* taf_info, int in_tid)
{
	int tid = (in_tid == ALLPRIO) ? 0 : in_tid;

	do {
		taf_source_type_t s_idx;

		for (s_idx = TAF_FIRST_REAL_SOURCE; s_idx < TAF_NUM_SCHED_SOURCES; s_idx++) {
			if (taf_info->opened[s_idx] & (1 << tid)) {
				void * arg = *taf_info->source_handle_p[s_idx];

				taf_info->funcs[s_idx].open_close_fn(arg, tid, FALSE);
				taf_info->opened[s_idx] &= ~(1 << tid);

				WL_TAFT3(taf_info, "closed %s tid %u\n",
					TAF_SOURCE_NAME(s_idx), tid);
			}
		}
	} while ((in_tid == ALLPRIO) && (++tid < NUMPRIO));
}

void BCMFASTPATH
taf_open_all_sources(wlc_taf_info_t* taf_info, int in_tid)
{
	int tid;

	TAF_ASSERT(taf_info->bulk_commit);
	tid = (in_tid == ALLPRIO) ? 0 : in_tid;

	do {
		taf_source_type_t s_idx;

		/* open scheduling window for each source with this TID */
		for (s_idx = TAF_FIRST_REAL_SOURCE; s_idx < TAF_NUM_SCHED_SOURCES; s_idx++) {
			if (!taf_info->funcs[s_idx].open_close_fn) {
				continue;
			}
			if ((taf_info->opened[s_idx] & (1 << tid)) == 0) {
				void * arg = *taf_info->source_handle_p[s_idx];

				if (taf_info->funcs[s_idx].open_close_fn(arg, tid, TRUE)) {

					WL_TAFT3(taf_info, "opened %s tid %u\n",
						TAF_SOURCE_NAME(s_idx), tid);
					taf_info->opened[s_idx] |= (1 << tid);
				}
			}
		}
	} while ((in_tid == ALLPRIO) && (++tid < NUMPRIO));
}

static INLINE void
taf_sched_state_method_update(taf_method_info_t* method, taf_scb_cubby_t* scb_taf, int tid,
	int count, taf_source_type_t s_idx, taf_link_state_t state)
{
	TAF_ASSERT(method->funcs.schedstate_fn);
	method->funcs.schedstate_fn(method, scb_taf, tid, count, s_idx, state);
}

void
wlc_taf_sched_state(wlc_taf_info_t* taf_info, struct scb* scb, int tid, int count,
	taf_source_index_public_t source, taf_sched_state_t state)
{
	taf_scb_cubby_t* scb_taf;
	taf_source_type_t s_idx;

	if (!taf_info || !taf_info->enabled) {
		return;
	}

	scb_taf = scb ? *SCB_TAF_CUBBY_PTR(taf_info, scb) : NULL;
	s_idx = wlc_taf_get_source_index(source);

	if (scb_taf) {
		taf_sched_state_method_update(scb_taf->method, scb_taf, tid, count, s_idx, state);
	} else {
		taf_scheduler_kind type;

		for (type = TAF_SCHED_FIRST_INDEX; type < TAF_SCHED_LAST_INDEX; type++) {
			taf_method_info_t* method = taf_get_method_info(taf_info, type);

			if (!method) {
				continue;
			}
			taf_sched_state_method_update(method, NULL, tid, count, s_idx, state);
		}
	}

	switch (state) {
		case TAF_SCHED_STATE_RESET:
			taf_info->release_count = 0;
			taf_info->last_scheduler_run = 0;
			taf_info->super_active = FALSE;
#if TAF_ENABLE_SQS_PULL
			taf_info->op_state = TAF_SCHEDULE_VIRTUAL_PACKETS;
			taf_info->total_pull_requests = 0;
			taf_info->eops_rqst = 0;
			WL_TAFT1(taf_info, "reset to virtual phase\n");
#ifdef TAF_DBG
			taf_info->virtual_complete_time = 0;
#endif /* TAF_DBG */
#endif /* TAF_ENABLE_SQS_PULL */
			taf_close_all_sources(taf_info, ALLPRIO);
			break;
		case TAF_SCHED_STATE_DATA_BLOCK_FIFO:
			if ((count == 0) && !wlc_taf_scheduler_blocked(taf_info)) {
				wlc_taf_schedule(taf_info, TAF_DEFAULT_UNIFIED_TID, NULL, FALSE);
			}
			break;
#if TAF_ENABLE_MU_TX
		case TAF_SCHED_STATE_MU_DL_VHMIMO:
			if (count != 0) {
				taf_info->mu_g_enable_mask[TAF_TECH_DL_VHMUMIMO] |= 1 << tid;
			} else {
				taf_info->mu_g_enable_mask[TAF_TECH_DL_VHMUMIMO] &= ~(1 << tid);
			}
			break;
		case TAF_SCHED_STATE_MU_DL_HEMIMO:
			if (count != 0) {
				taf_info->mu_g_enable_mask[TAF_TECH_DL_HEMUMIMO] |= 1 << tid;
			} else {
				taf_info->mu_g_enable_mask[TAF_TECH_DL_HEMUMIMO] &= ~(1 << tid);
			}
			break;
		case TAF_SCHED_STATE_MU_DL_OFDMA:
			if (count != 0) {
				taf_info->mu_g_enable_mask[TAF_TECH_DL_OFDMA] |= 1 << tid;
			} else {
				taf_info->mu_g_enable_mask[TAF_TECH_DL_OFDMA] &= ~(1 << tid);
			}
			break;
#endif /* TAF_ENABLE_MU_TX */
		default:
			break;
	}
}

#if TAF_ENABLE_SQS_PULL
void
wlc_taf_reset_sqs(wlc_taf_info_t *taf_info)
{
	/* less heavy than a full scheduler reset */
	taf_scb_cubby_t *scb_taf = taf_info->head;

	while (scb_taf) {
		taf_scb_state_method_update(scb_taf->method, scb_taf->scb, TAF_SOURCE_UNDEFINED,
			NULL, TAF_SCBSTATE_RESET);

		scb_taf = scb_taf->next;
	}
}

bool
wlc_taf_marked_up(wlc_taf_info_t *taf_info)
{
	if (taf_info->eops_rqst == 0) {
		taf_info->op_state = TAF_SCHEDULE_VIRTUAL_PACKETS;

		WL_TAFT3(taf_info, "TAF_SCHEDULE_VIRTUAL_PACKETS (%u pull req%s)\n",
			taf_info->total_pull_requests,
			taf_info->total_pull_requests > 0 ? ", will be set to 0" : "");

		if (taf_info->total_pull_requests > 0) {
			WL_ERROR(("wl%u %s: total_pull_requests (%d) should be 0\n",
				WLCWLUNIT(TAF_WLCT(taf_info)), __FUNCTION__,
				taf_info->total_pull_requests));
			taf_info->total_pull_requests = 0;
			taf_info->sqs_state_fault++;
#ifdef TAF_DEBUG_VERBOSE
			taf_memtrace_dump(taf_info);
#endif // endif
			wlc_taf_reset_sqs(taf_info);
		}
		else if (taf_info->sqs_state_fault) {
			WL_ERROR(("wl%u %s: clearing sqs_state_fault (%u)\n",
				WLCWLUNIT(TAF_WLCT(taf_info)), __FUNCTION__,
				taf_info->sqs_state_fault));
			taf_info->sqs_state_fault = 0;
#ifdef TAF_DBG
			taf_info->sqs_state_fault_prev = 0;
#endif /* TAF_DBG */
		}
		return TRUE;
	}
	return FALSE;
}
#endif /* TAF_ENABLE_SQS_PULL */

void
wlc_taf_pkts_dequeue(wlc_taf_info_t* taf_info, struct scb* scb, int tid, int pkts)
{
#if TAF_ENABLE_SQS_PULL
	taf_scb_cubby_t* scb_taf;

	if (!scb) {
		WL_TAFT3(taf_info, "scb null\n");
		return;
	}

	scb_taf = *SCB_TAF_CUBBY_PTR(taf_info, scb);

	if (scb_taf == NULL) {
		WL_TAFT4(taf_info, MACF" has no TAF cubby\n", TAF_ETHERS(scb));
		return;
	}

	if (taf_info->op_state != TAF_MARKUP_REAL_PACKETS) {
		/* XXX NAR only traffic traffic does not pass via sqs virtual pull
		* (it is pushed direct) so it causes unexpected dequeue; hence guard this sqs fault
		* condition by checking that this is AMPDU.
		*/
		if ((scb_taf->info.linkstate[TAF_SOURCE_AMPDU][tid] != TAF_LINKSTATE_NONE)) {

			taf_info->sqs_state_fault++;
			WL_ERROR(("wl%u %s: unexpected op_state (%d) "MACF" tid %u, pkts %u, "
				"pull %u, ps %u\n", WLCWLUNIT(TAF_WLCT(taf_info)), __FUNCTION__,
				taf_info->op_state, TAF_ETHERS(scb), tid,
				pkts, scb_taf->info.pkt_pull_request[tid], scb_taf->info.ps_mode));
#ifdef TAF_DEBUG_VERBOSE
			taf_memtrace_dump(taf_info);
#endif // endif
		}
		return;
	}

	scb_taf->info.pkt_pull_dequeue += pkts;

	if (scb_taf->info.pkt_pull_request[tid] >= pkts) {
		scb_taf->info.pkt_pull_request[tid] -= pkts;

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
#ifdef TAF_DEBUG_VERBOSE
					taf_memtrace_dump(taf_info);
#endif // endif
			}
		}
	} else {
		uint16 prev_pull = scb_taf->info.pkt_pull_request[tid];

		scb_taf->info.pkt_pull_request[tid] = 0;

		WL_ERROR(("wl%u %s: "MACF" tid %u, pull MISCOUNT: prev = %u, dequeue = %u\n",
			WLCWLUNIT(TAF_WLCT(taf_info)), __FUNCTION__,
			TAF_ETHERS(scb), tid, prev_pull, pkts));

	}
#endif /* TAF_ENABLE_SQS_PULL */
}

void
wlc_taf_v2r_complete(wlc_taf_info_t* taf_info)
{
#if TAF_ENABLE_SQS_PULL
	if (taf_info->op_state != TAF_MARKUP_REAL_PACKETS) {
		WL_ERROR(("wl%u %s: unexpected op_state (%d)\n", WLCWLUNIT(TAF_WLCT(taf_info)),
			__FUNCTION__, taf_info->op_state));
	}

	if (taf_info->eops_rqst > 0) {
		taf_info->eops_rqst--;
	} else {
		WL_ERROR(("wl%u %s: eops_rqst 0\n", WLCWLUNIT(TAF_WLCT(taf_info)), __FUNCTION__));
	}

	if (taf_info->eops_rqst == 0 && taf_info->op_state == TAF_MARKUP_REAL_PACKETS) {
		wlc_taf_schedule(taf_info, TAF_SQS_V2R_COMPLETE_TID, NULL, FALSE);
	}
#endif /* TAF_ENABLE_SQS_PULL */
}

void
wlc_taf_set_dlofdma_maxn(wlc_info_t *wlc, uint8 (*maxn)[D11_REV128_BW_SZ])
{
#if TAF_ENABLE_MU_TX
	wlc_taf_info_t *taf_info = wlc->taf_handle;

	if (!taf_info || !maxn) {
		TAF_ASSERT(taf_info && maxn);
		return;
	}

	STATIC_ASSERT(sizeof(taf_info->dlofdma_maxn) == sizeof(*maxn));
	memcpy(taf_info->dlofdma_maxn, maxn, sizeof(taf_info->dlofdma_maxn));

#ifdef TAF_DBG
	{
		int bw = 0;

		BCM_REFERENCE(bw);
		for (bw = 0; bw < D11_REV128_BW_SZ; bw++) {
			WL_TAFT1(taf_info, "maxn[%d]=%d\n", bw, taf_info->dlofdma_maxn[bw]);
		}
		WL_TAFT2(taf_info, "DLOFDMA maxn updated.\n");
	}
#endif /* TAF_DBG */
#endif /* TAF_ENABLE_MU_TX */
}

void
wlc_taf_set_ulofdma_maxn(wlc_info_t *wlc, uint8 (*maxn)[D11_REV128_BW_SZ])
{
#if TAF_ENABLE_MU_TX && TAF_ENABLE_UL
	wlc_taf_info_t *taf_info = wlc->taf_handle;

	if (!taf_info || !maxn) {
		TAF_ASSERT(taf_info && maxn);
		return;
	}

	STATIC_ASSERT(sizeof(taf_info->ulofdma_maxn) == sizeof(*maxn));
	memcpy(taf_info->ulofdma_maxn, maxn, sizeof(taf_info->ulofdma_maxn));

#ifdef TAF_DBG
	{
		int bw = 0;

		BCM_REFERENCE(bw);
		for (bw = 0; bw < D11_REV128_BW_SZ; bw++) {
			WL_TAFT1(taf_info, "maxn[%d]=%d\n", bw, taf_info->ulofdma_maxn[bw]);
		}
		WL_TAFT2(taf_info, "ULOFDMA maxn updated.\n");
	}
#endif /* TAF_DBG */
#endif /* TAF_ENABLE_MU_TX && TAF_ENABLE_UL */
}

void
wlc_taf_inhibit(wlc_info_t *wlc, bool inhibit)
{
	wlc_taf_info_t* taf_info = wlc->taf_handle;

	if (inhibit) {

		/* check for too many nested inhibits - not strictly a problem but if this
		 * limit is hit we might want to check why
		 */
		if (taf_info->scheduler_inhibit > 2) {
			WL_ERROR(("wl%u %s: high nested taf inhibit request %u\n",
				WLCWLUNIT(wlc), __FUNCTION__, taf_info->scheduler_inhibit));
		}
		TAF_ASSERT(taf_info->scheduler_inhibit < 8);

		if (taf_info->scheduler_inhibit < BCM_UINT8_MAX) {
			++taf_info->scheduler_inhibit;
		}

	} else {

		if (taf_info->scheduler_inhibit > 0) {
			--taf_info->scheduler_inhibit;
		} else {
			WL_ERROR(("wl%u %s: mismatched inhibit\n", WLCWLUNIT(wlc), __FUNCTION__));
		}

		/* if inhibit count is now zero, and scheduler was attempted (defer state is set),
		 * now it is fine to call TAF scheduler
		 */
		if (taf_info->scheduler_inhibit == 0 && taf_info->scheduler_defer) {

			TAF_ASSERT(!wlc_taf_scheduler_blocked(taf_info));
			taf_info->scheduler_defer = FALSE;
			wlc_taf_schedule(taf_info, TAF_DEFAULT_UNIFIED_TID, NULL, FALSE);
		}
	}
}

#ifdef TAF_PKTQ_LOG
int
wlc_taf_dpstats_dump(wlc_info_t* wlc, struct scb* scb, wl_iov_pktq_log_t* iov_pktq, uint8 index,
	bool clear, uint32 timelo, uint32 timehi, uint32 prec_mask, uint32 flags,
	const char** label)
{
	wlc_taf_info_t* taf_info = wlc->taf_handle;
	taf_scb_cubby_t* scb_taf = *SCB_TAF_CUBBY_PTR(taf_info, scb);
	taf_method_info_t* method = scb_taf ? scb_taf->method : NULL;

	if (iov_pktq->version != 6) {
		return BCME_VERSION;
	}

	if (method && method->funcs.dpstats_log_fn) {
		mac_log_counters_v06_t* mac_log = &iov_pktq->pktq_log.v06.counters[index];
		uint32 mask;

		if (label) {
			*label = TAF_SCHED_NAME(method);
		}

		mask = method->funcs.dpstats_log_fn(method, scb_taf, mac_log, clear, timelo, timehi,
			prec_mask);

		iov_pktq->pktq_log.v06.num_prec[index] = (mask & 0xFF) ? NUMPRIO : 0;
		iov_pktq->pktq_log.v06.counter_info[index] = mask | (flags & ~0xFFFF);
		return BCME_OK;
	}
	return BCME_UNSUPPORTED;
}

void
wlc_taf_dpstats_free(wlc_info_t* wlc, struct scb* scb)
{
	wlc_taf_info_t* taf_info = wlc->taf_handle;
	taf_scb_cubby_t* scb_taf = *SCB_TAF_CUBBY_PTR(taf_info, scb);
	taf_method_info_t* method = scb_taf ? scb_taf->method : NULL;

	if (method && method->funcs.dpstats_log_fn) {
		method->funcs.dpstats_log_fn(method, scb_taf, NULL, 0, 0, 0, 0);
	}
}
#else
int
wlc_taf_dpstats_dump(wlc_info_t* wlc, struct scb* scb, wl_iov_pktq_log_t* iov_pktq, uint8 index,
	bool clear, uint32 timelo, uint32 timehi, uint32 prec_mask, uint32 flags,
	const char** label)
{
	return BCME_UNSUPPORTED;
}

void
wlc_taf_dpstats_free(wlc_info_t* wlc, struct scb* scb)
{
}
#endif /* TAF_PKTQ_LOG */
