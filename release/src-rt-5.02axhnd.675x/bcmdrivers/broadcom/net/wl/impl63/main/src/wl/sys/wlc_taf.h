/*
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * wlc_taf.h
 *
 * This module contains the external definitions for the taf transmit module.
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
 * $Id$
 *
 */
#if !defined(__wlc_taf_h__)
#define __wlc_taf_h__

#ifdef WLTAF

#include <d11.h>
#include <wlc_pub.h>

/*
 * Module attach and detach functions. This is the tip of the iceberg, visible from the outside.
 * All the rest is hidden under the surface.
 */
extern wlc_taf_info_t *wlc_taf_attach(wlc_info_t *);
extern int wlc_taf_detach(wlc_taf_info_t *);

#ifdef BCMDBG
extern uint32 taf_dbg_idx(wlc_info_t* wlc);

#ifdef TAF_DEBUG_VERBOSE
extern void wlc_taf_mem_log(wlc_info_t* wlc, const char* func, const char* format, ...);
#define WL_TAFLOG(_w, _f, ...)          wlc_taf_mem_log(_w, __FUNCTION__, _f, ##__VA_ARGS__)
#else
#define WL_TAFLOG(w, format, ...)	do {} while (0)
#endif /* TAF_DEBUG_VERBOSE */

#define WL_TAFF(w, format, ...)		do {\
						if (wl_msg_level & WL_TAF_VAL) { \
							WL_PRINT(("wl%d: %21s: "format, \
								WLCWLUNIT(((wlc_info_t*)(w))), \
								__FUNCTION__, ##__VA_ARGS__)); \
						} else { \
							WL_TAFLOG(w, format, ##__VA_ARGS__); \
						} \
					} while (0)

#define _taf_stringify(x...)    #x
#define taf_stringify(x...)     _taf_stringify(x)

extern const char* taf_assert_fail;
extern void taf_memtrace_dump(struct wlc_taf_info *);

#ifdef DONGLEBUILD
extern bool taf_attach_complete;
#else
#define taf_attach_complete     TRUE
#endif // endif

#define TAF_ASSERT(exp)         if (!(exp)) { \
					taf_memtrace_dump(NULL); \
					WL_PRINT(("%s '"taf_stringify(exp)"' %s\n", \
						taf_assert_fail, __FUNCTION__)); \
					OSL_DELAY(400000); \
				} \
				if (taf_attach_complete) { ASSERT(exp); }

#else
#define WL_TAFF(w, format, ...)			do {} while (0)
#define TAF_ASSERT(exp)				ASSERT(exp)
#endif /* BCMDBG */

#define TAF_MAX_PKT_INDEX            7
#define TAF_PKT_SIZE_DEFAULT         (1024)  /* used in case there is no previous packet known */

#define TAF_PKTTAG_NUM_BITS			13
#define TAF_PKTTAG_MAX				((1 << TAF_PKTTAG_NUM_BITS) - 1)

#define TAF_PKTTAG_RESERVED			(TAF_PKTTAG_MAX - 4)
#define TAF_PKTTAG_DEFAULT			(TAF_PKTTAG_RESERVED + 1)
#define TAF_PKTTAG_PS				(TAF_PKTTAG_RESERVED + 2)
#define TAF_PKTTAG_PROCESSED			(TAF_PKTTAG_RESERVED + 3)

#define TAF_WINDOW_MAX				(65535)
#define TAF_MICROSEC_MAX			(TAF_PKTTAG_TO_MICROSEC(TAF_PKTTAG_RESERVED))

#define TAF_UNITS_TO_MICROSEC(a)		(((a) + 4) >> 3)
#define TAF_MICROSEC_TO_UNITS(a)		((a) << 3)

#define TAF_MICROSEC_TO_PKTTAG(a)		(((a) * 3) / 2)
#define TAF_PKTTAG_TO_MICROSEC(a)		(((a) * 2) / 3)

#define TAF_PKTTAG_TO_UNITS(a)			(TAF_PKTTAG_TO_MICROSEC(TAF_MICROSEC_TO_UNITS(a)))

static INLINE uint32 taf_units_to_pkttag(uint32 a, uint32* e)
{
	uint32 value;

	if (e) {
		a += *e;
	}
	value = TAF_UNITS_TO_MICROSEC(TAF_MICROSEC_TO_PKTTAG(a));

	if (value > TAF_PKTTAG_RESERVED) {
		value = TAF_PKTTAG_RESERVED;
	}

	if (e) {
		*e = a - TAF_PKTTAG_TO_UNITS(value);
	}
	return value;
}
#define TAF_UNITS_TO_PKTTAG(a, e)		taf_units_to_pkttag((a), (e))

static INLINE uint32 taf_ul_units_to_pkttag(uint32 a, uint32* e)
{
	uint32 value;

	if (e) {
		a += *e;
	}
	value = TAF_UNITS_TO_MICROSEC(TAF_MICROSEC_TO_PKTTAG(a));

	if (e) {
		*e = a - TAF_PKTTAG_TO_UNITS(value);
	}
	return value;
}
#define TAF_UL_UNITS_TO_PKTTAG(a, e)		taf_ul_units_to_pkttag((a), (e))

#define TAF_PKTBYTES_COEFF			16384
#define TAF_PKTBYTES_TO_TIME(len, p, b) \
	(((p) + ((len) * (b)) + (TAF_PKTBYTES_COEFF / 2)) / TAF_PKTBYTES_COEFF)
#define TAF_PKTBYTES_TO_UNITS(len, p, b) \
	(TAF_PKTBYTES_TO_TIME(len, TAF_MICROSEC_TO_UNITS(p), TAF_MICROSEC_TO_UNITS(b)))

typedef enum {
	TAF_RELEASE_LIKE_IAS = 0x1A5,
	TAF_RELEASE_LIKE_DEFAULT = 0xDEF,
#ifdef WLATF
	TAF_RELEASE_LIKE_ATF = 0xA7F
#endif // endif
} taf_release_like_t;

typedef enum {
	TAF_RELEASE_MODE_REAL,
#ifdef WLCFP
	TAF_RELEASE_MODE_REAL_FAST,
#endif // endif
	TAF_RELEASE_MODE_VIRTUAL
} taf_release_mode_t;

typedef enum {
	TAF_REL_COMPLETE_NULL,
	TAF_REL_COMPLETE_NOTHING,
	TAF_REL_COMPLETE_NOTHING_AGG,
	TAF_REL_COMPLETE_EMPTIED,
	TAF_REL_COMPLETE_EMPTIED_PS,
	TAF_REL_COMPLETE_FULL,
	TAF_REL_COMPLETE_BLOCKED,
	TAF_REL_COMPLETE_NO_BUF,
	TAF_REL_COMPLETE_TIME_LIMIT,
	TAF_REL_COMPLETE_REL_LIMIT,
	TAF_REL_COMPLETE_ERR,
	TAF_REL_COMPLETE_PS,
	TAF_REL_COMPLETE_RESTRICTED,
	TAF_REL_COMPLETE_FULFILLED,
	NUM_TAF_REL_COMPLETE_TYPES
} taf_release_complete_t;

typedef struct taf_ias_public {
	uint8   was_emptied;
	uint8   is_ps_mode;
	uint8   index;
	uint8   margin;
	uint8   opt_aggs;
	uint8   opt_aggp;
	uint16  opt_aggp_limit;
	uint16  npkts_for_release;
	uint32  adjust;
	uint32  time_limit_units;
	uint32  released_units_limit;
	uint32  byte_rate;
	uint32  pkt_rate;
#ifdef WLSQS
	uint16  estimated_pkt_size_mean;
	uint16  traffic_count_available;
	struct {
		uint32 release;
		uint32 released_units;
	} virtual;
	struct {
		uint32 release;
		uint32 released_units;
	} pending;
#endif /* WLSQS */
	struct {
		uint32 units;
	} extend;
	struct {
		uint32 release;
		uint32 npkts;
		uint32 released_units;
		uint32 released_bytes;
	} actual;
	struct {
		uint32 release;
		uint32 released_units;
		uint32 released_bytes;
	} total;

} taf_ias_public_t;

typedef struct taf_def_public {
	bool    was_emptied;
	bool    is_ps_mode;
	struct {
		uint32 release;
		uint32 release_limit;
		uint32 released_bytes;
	} actual;
} taf_def_public_t;

typedef enum {
	TAF_REL_TYPE_SU,
	TAF_REL_TYPE_VHMUMIMO,
	TAF_REL_TYPE_HEMUMIMO,
	TAF_REL_TYPE_OFDMA,
	TAF_REL_TYPE_ULOFDMA
} taf_release_type_t;

typedef struct  taf_scheduler_public {
	taf_release_like_t  how;
	taf_release_mode_t  mode;
	taf_release_type_t  type;
	uint16              release_type_present;
	taf_release_complete_t complete;
	union {
		taf_ias_public_t  ias;
		taf_def_public_t  def;
	};
} taf_scheduler_public_t;

#define TAF_AMPDU                 "ampdu"
#define TAF_NAR                   "nar"
#define TAF_SQSHOST               "sqs_host"
#define TAF_UL                    "ul"
#define TAF_SQS_TRIGGER_TID       (-2)
#define TAF_SQS_V2R_COMPLETE_TID  (-3)

#define TAF_SET_TAG(tag)      (tag)->flags3 |= WLF3_TAF_TAGGED
#define TAF_IS_TAGGED(tag)    (((tag)->flags3 & WLF3_TAF_TAGGED) ? TRUE : FALSE)

#define TAF_PARAM(p)	(void*)(size_t)(p)

typedef enum {
	TAF_LINKSTATE_NONE,
	TAF_LINKSTATE_INIT,
	TAF_LINKSTATE_ACTIVE,
	TAF_LINKSTATE_NOT_ACTIVE,
	TAF_LINKSTATE_HARD_RESET,
	TAF_LINKSTATE_SOFT_RESET,
	TAF_LINKSTATE_REMOVE,
	TAF_LINKSTATE_AMSDU_AGG,

	NUM_TAF_LINKSTATE_TYPES
} taf_link_state_t;

typedef enum {
	TAF_BSS_STATE_NONE,
	TAF_BSS_STATE_AMPDU_AGGREGATE_OVR,
	TAF_BSS_STATE_AMPDU_AGGREGATE_TID,

	NUM_TAF_BSS_STATE_TYPES
} taf_bss_state_t;

typedef enum {
	TAF_SCBSTATE_NONE,
	TAF_SCBSTATE_INIT,
	TAF_SCBSTATE_EXIT,
	TAF_SCBSTATE_RESET,
	TAF_SCBSTATE_SOURCE_ENABLE,
	TAF_SCBSTATE_SOURCE_DISABLE,
	TAF_SCBSTATE_WDS,
	TAF_SCBSTATE_DWDS,
	TAF_SCBSTATE_SOURCE_UPDATE,
	TAF_SCBSTATE_UPDATE_BSSCFG,
	TAF_SCBSTATE_POWER_SAVE,
	TAF_SCBSTATE_OFF_CHANNEL,
	TAF_SCBSTATE_DATA_BLOCK_OTHER,
	TAF_SCBSTATE_MU_DL_VHMIMO,
	TAF_SCBSTATE_MU_DL_HEMIMO,
	TAF_SCBSTATE_MU_DL_OFDMA,
	TAF_SCBSTATE_MU_UL_OFDMA,
	TAF_SCBSTATE_TWT_SP_ENTER,
	TAF_SCBSTATE_TWT_SP_EXIT,

	TAF_SCBSTATE_GET_TRAFFIC_ACTIVE,

	NUM_TAF_SCBSTATE_TYPES
} taf_scb_state_t;

typedef enum {
	TAF_TXPKT_STATUS_NONE,
	TAF_TXPKT_STATUS_REGMPDU,
	TAF_TXPKT_STATUS_PKTFREE,
	TAF_TXPKT_STATUS_PKTFREE_RESET,
	TAF_TXPKT_STATUS_PKTFREE_DROP,
	TAF_TXPKT_STATUS_UPDATE_RETRY_COUNT,
	TAF_TXPKT_STATUS_UPDATE_PACKET_COUNT,
	TAF_TXPKT_STATUS_UPDATE_RATE,
	TAF_TXPKT_STATUS_SUPPRESSED,
	TAF_TXPKT_STATUS_SUPPRESSED_FREE,
	TAF_TXPKT_STATUS_SUPPRESSED_QUEUED,
	TAF_TXPKT_STATUS_TRIGGER_COMPLETE, /* ULMU trigger completion */
	TAF_NUM_TXPKT_STATUS_TYPES
} taf_txpkt_state_t;

typedef enum {
	TAF_SCHED_STATE_NONE,
	TAF_SCHED_STATE_DATA_BLOCK_FIFO,
	TAF_SCHED_STATE_REWIND,
	TAF_SCHED_STATE_RESET,
	TAF_SCHED_STATE_MU_DL_VHMIMO,
	TAF_SCHED_STATE_MU_DL_HEMIMO,
	TAF_SCHED_STATE_MU_DL_OFDMA,
	TAF_SCHED_STATE_MU_UL_OFDMA,
	TAF_NUM_SCHED_STATE_TYPES
} taf_sched_state_t;

extern const uint8 wlc_taf_prec2prio[WLC_PREC_COUNT];

#define TAF_PREC(prec)       wlc_taf_prec2prio[(prec)]

extern bool wlc_taf_enabled(wlc_taf_info_t* taf_info);
extern bool wlc_taf_in_use(wlc_taf_info_t* taf_info);
extern bool wlc_taf_ul_enabled(wlc_taf_info_t* taf_info);
extern bool wlc_taf_nar_in_use(wlc_taf_info_t* taf_info, bool * enabled);
#ifdef WLSQS
extern bool wlc_taf_scheduler_blocked(wlc_taf_info_t* taf_info);
#else
#define wlc_taf_scheduler_blocked(t)   FALSE
#endif // endif

extern void wlc_taf_pkts_enqueue(wlc_taf_info_t* taf_info, struct scb* scb, int tid,
	const void* data, int pkts);
extern void wlc_taf_pkts_dequeue(wlc_taf_info_t* taf_info, struct scb* scb, int tid, int pkts);

extern uint16 wlc_taf_traffic_active(wlc_taf_info_t* taf_info, struct scb* scb);

extern void wlc_taf_bss_state_update(wlc_taf_info_t* taf_info, wlc_bsscfg_t *bsscfg, void* update,
	taf_bss_state_t state);

extern void wlc_taf_link_state(wlc_taf_info_t* taf_info, struct scb* scb, int tid,
	const void* data, taf_link_state_t state);

extern int wlc_taf_scb_state_update(wlc_taf_info_t* taf_info, struct scb* scb, void* update,
	taf_scb_state_t state);

extern bool wlc_taf_txpkt_status(wlc_taf_info_t* taf_info, struct scb* scb, int tid, void* p,
	taf_txpkt_state_t status);

extern void wlc_taf_rate_override(wlc_taf_info_t* taf_info, ratespec_t rspec, wlcband_t *band);

extern void wlc_taf_sched_state(wlc_taf_info_t* taf_info, struct scb* scb, int tid, int count,
	const void* data, taf_sched_state_t state);

extern bool wlc_taf_schedule(wlc_taf_info_t* taf_info,  int tid,  struct scb* scb, bool force);

extern void wlc_taf_v2r_complete(wlc_taf_info_t* taf_info);

extern bool wlc_taf_reset_scheduling(wlc_taf_info_t* taf_info, int tid, bool hardreset);

#ifdef PKTQ_LOG
extern int wlc_taf_dpstats_dump(wlc_info_t* wlc, struct scb* scb, wl_iov_pktq_log_t* iov_pktq,
	uint8 index, bool clear, uint32 timelo, uint32 timehi, uint32 prec_mask, uint32 flags,
        const char** label);
extern void wlc_taf_dpstats_free(wlc_info_t* wlc, struct scb* scb);
#endif /* PKTQ_LOG */

extern void wlc_taf_set_dlofdma_maxn(wlc_info_t *wlc, uint8 (*maxn)[D11_REV128_BW_SZ]);
#endif /* WLTAF */

#endif /* __wlc_taf_h__ */
