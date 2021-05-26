/*
 * Proxd FTM method support. See twiki FineTimingMeasurement.
 * This header is private/internal to proxd FTM method
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
 * $Id: pdftmpvt.h 787663 2020-06-08 15:59:47Z $
 */

#ifndef _pdftmpvt_h_
#define _pdftmpvt_h_

#include <wlc_cfg.h>

#include <typedefs.h>
#ifdef WL_RANGE_SEQ
#include <bcmstdlib_s.h>
#endif // endif
#include <bcmendian.h>
#include <bcmutils.h>
#include <bcmtlv.h>
#include <802.11.h>
#include <bcmevent.h>
#include <wlioctl.h>
#include <bcmwifi_channels.h>
#include <bcm_notif_pub.h>
#ifdef WL_RANGE_SEQ
#include <bcmwpa.h>
#endif // endif

#include <osl.h>
#include <wl_dbg.h>
#include <siutils.h>

#include <wlc_pub.h>
#include <wlc_rate.h>
#include <wlc_key.h>
#include <wlc_channel.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wlc_msch.h>
#include <wl_export.h>
#ifdef WLMCNX
#include <wlc_mcnx.h>
#endif /* WLMCNX */
#include <wlc_scan_utils.h>
#include <wlc_hrt.h>
#include <wlc_ftm.h>
#include <wlc_bmac.h>

#include <wlc_ie_mgmt.h>
#include <wlc_ie_mgmt_ft.h>
#include <wlc_ie_helper.h>
#include <wlc_chanctxt.h>

#include "pdsvc.h"
#include "pdburst.h"
#include "pdftm.h"
#include <wlc_pdsvc.h>
#include <wlc_pdmthd.h>
#include <phy_tof_api.h>
#ifdef WLTDLS
#include <wlc_tdls.h>
#endif // endif

#define	PHY_CH_EST_SMPL_SZ	512

#define HOWMANY(x, y) ((((x) + (y) - 1)) / (y))

#define FTM_CNT_SN_INCR(_sn, _ctr, _incr) (((_sn)->cnt->_ctr) += (_incr))
#define FTM_CNT_INCR(_ftm, _sn, _ctr, _incr)  do {\
	(_ftm)->cnt->_ctr += (_incr); \
	if (_sn) {\
		FTM_CNT_SN_INCR(_sn, _ctr, _incr); \
	}\
} while (0)

struct pdftm_ext_sched {
	void *req_hdl;
};
typedef struct pdftm_ext_sched pdftm_ext_sched_t;

struct pdftm_session_config {
	wl_proxd_session_flags_t	flags;
	wl_proxd_event_mask_t		event_mask;
	pdburst_config_t		*burst_config;
#if defined(WLC_PROXD_ROM_COMPAT) || defined(WLC_PROXD_ROM_COMPAT1)
	uint8				pad[44];
#endif /* WLC_PROXD_ROM_COMPAT || WLC_PROXD_ROM_COMPAT1 */
	uint16				num_burst;
	wl_proxd_intvl_t		init_delay;
	wl_proxd_intvl_t		burst_period;	/* between bursts */
	wl_proxd_avail_t		*peer_avail;
};
typedef struct pdftm_session_config pdftm_session_config_t;

struct pdftm_config {
	wl_proxd_flags_t		flags;
	wl_proxd_event_mask_t		event_mask;
	wl_proxd_debug_mask_t		debug;
	pdftm_session_config_t		*session_defaults;
	uint16				rx_max_burst;	/* limit bursts per session */
	uint16				max_rctx_sids;
	wl_proxd_avail_t		*local_avail;	/* local availability */
	struct ether_addr		dev_addr;
	wl_proxd_flags_t		scratch_flags;
};
typedef struct pdftm_config pdftm_config_t;

enum {
	FTM_SESSION_NONE		= 0x0000,
	FTM_SESSION_VALID		= 0x0001,
	FTM_SESSION_DELETE_ON_STOP	= 0x0002,
	FTM_SESSION_FTM1_SENT		= 0x0004,
	FTM_SESSION_FTM1_RCVD		= 0x0008,
	FTM_SESSION_PARAM_OVERRIDE	= 0x0010,	/* note: this can not move to ftm state */
	FTM_SESSION_LCI_REQ_RCVD	= 0x0020,
	FTM_SESSION_CIVIC_REQ_RCVD	= 0x0040,
	FTM_SESSION_RSVD80		= 0x0080,	/* reserved */
	FTM_SESSION_TX_PENDING		= 0x0100,	/* callback active */
	FTM_SESSION_TSPT1NS		= 0x0200,	/* 0.1ns timestamps */
	FTM_SESSION_KEY_WAIT		= 0x0400,	/* key wait */
	FTM_SESSION_EXT_SCHED_WAIT	= 0x0800,	/* external sched wait */
	FTM_SESSION_TSF_WAIT		= 0x1000,	/* tsf wait */
	FTM_SESSION_ON_CHAN		= 0x2000,	/* on channel time slot */
};
typedef int16 pdftm_session_flags_t;

#define FTM_VALID_SESSION(_sn) ((_sn) != NULL && ((_sn)->flags & FTM_SESSION_VALID) != 0)

#ifdef WL_FTM_TSF_SYNC
#define FTM_TSF_SYNC_SESSION(_sn) (\
	((_sn)->ftm->config->flags & WL_PROXD_FLAG_NO_TSF_SYNC) == 0)
#else
#define FTM_TSF_SYNC_SESSION(_sn) FALSE
#endif /* WL_FTM_TSF_SYNC */

typedef uint8 pdftm_session_idx_t;

typedef uint64 pdftm_time_t; /* local tsf */
typedef pdftm_time_t pdftm_expiration_t;

enum {
	FTM_SESSION_STATE_NONE			= 0x00,
	FTM_SESSION_STATE_BURST_DONE		= 0x01,
	FTM_SESSION_STATE_USE_TSF_OFF		= 0x02,
	FTM_SESSION_STATE_NEED_TRIGGER		= 0x04,
	FTM_SESSION_STATE_UPD_MEAS_START	= 0x08,
	FTM_SESSION_STATE_BURST_REQ		= 0x10,
	/* 0x20 is reserved */
	FTM_SESSION_STATE_MSCH_SCHEDULED	= 0x40,
	FTM_SESSION_STATE_TSF_SYNC		= 0x80
};
typedef int8 pdftm_session_state_flags_t;

struct pdftm_session_state {
	pdburst_t			*burst;
	pdftm_expiration_t		delay_exp;
	pdftm_expiration_t		burst_exp;
	pdftm_time_t			burst_start;	/* actual or sched */
	pdftm_time_t			burst_end;	/* actual or sched */
	uint8				dialog;		/* id for meas frame */
	pdftm_session_state_flags_t	flags;
	wl_proxd_status_t		rtt_status;
	wl_proxd_result_flags_t		result_flags;
	uint16				burst_num;
	wl_proxd_rtt_sample_t		avg_rtt;
	uint32				avg_dist;	/* 1/256m units */
	uint16				sd_rtt;		/* standard deviation */
	uint16				num_valid_rtt;	/* valid rtt count */
	uint16				num_rtt;
	uint16				max_rtt;
	wl_proxd_intvl_t		retry_after;
	wl_proxd_rtt_sample_t		*rtt;		/* rtt[max_rtt] */

	pdftm_time_t			meas_start;	/* for cur/prev burst */

	/* local tsf at trigger was sent/received */
	pdftm_time_t			trig_tsf;
	/* tsf sync - adjustment for next burst */
	int32				trig_delta;
	uint16				num_meas;
};
typedef struct pdftm_session_state pdftm_session_state_t;

/* scan state;  odd numbers are for setup, and even (next) for done */
enum {
	FTM_TSF_SCAN_STATE_NONE			= 0,
	FTM_TSF_SCAN_STATE_ACTIVE_SCAN		= 1,
	FTM_TSF_SCAN_STATE_ACTIVE_SCAN_DONE	= 2,
	FTM_TSF_SCAN_STATE_PASSIVE_SCAN		= 3,
	FTM_TSF_SCAN_STATE_PASSIVE_SCAN_DONE	= 4
};
typedef int8 pdftm_tsf_scan_state_t;

enum {
	TARGET_STATS_MF_BUF_RCVD		= (1 << 0),
	TARGET_STATS_MF_BUF_BITFLIPS_VALID	= (1 << 1),
	TARGET_STATS_MF_BUF_SNR_VALID		= (1 << 2),
	TARGET_STATS_MEAS_INFO_RCVD		= (1 << 3),
	TARGET_STATS_MF_STATS_BUF_RCVD		= (1 << 4),
	TARGET_STATS_ALL			= 0xffff
};
typedef uint16 target_stats_status_t;

struct target_side_data {
	wl_proxd_bitflips_t	bit_flips;
	wl_proxd_snr_t		snr;
	wl_proxd_phy_error_t	target_phy_error;
	target_stats_status_t	status;	/* bit mask, see above target_stats_status_t */
};
typedef struct target_side_data target_side_data_t;

typedef struct pdftm_session_record {
	wl_proxd_session_state_t	state;
	wl_proxd_status_t		status;
	pdftm_time_t			state_ts;
} pdftm_session_record_t;

#define FTM_SESSION_MAX_RECORDS		10u

struct pdftm_session {
	pdftm_t				*ftm;
	wlc_bsscfg_t			*bsscfg;
	pdftm_session_idx_t		idx;
	uint8				ftm1_retrycnt;
	pdftm_session_flags_t		flags;
	wl_proxd_session_id_t		sid;
	pdftm_session_config_t		*config;
	wl_proxd_session_state_t	state;
	wl_proxd_status_t		status;
	wl_proxd_counters_t		*cnt;
	pdftm_session_state_t		*ftm_state;
	dot11_rmreq_ftm_lci_t		*lci_req;
	dot11_rmreq_ftm_civic_t		*civic_req;
	pdftm_ext_sched_t		*ext_sched;
	uint8				*lci_rep;	/* lci-report */
	uint16				lci_rep_len;
	uint8				*civic_rep;	/* civic-report */
	uint16				civic_rep_len;
	pdftm_time_t			tsf_off;	/* tsf = local + tsf_off  */
	pdftm_tsf_scan_state_t		tsf_scan_state;
	wlc_hrt_to_t			*ftm1_tx_timer; /* FTM1 tx timer */
	target_side_data_t		target_stats;
	wlc_bsscfg_t			*event_bsscfg; /* bsscfg to be used for FTM events */
	uint8				scan_retry_attempt;
	int				session_rec_idx; /* current rec index */
	pdftm_session_record_t		*session_records; /* session state records/history */
};

#define FTM_SESSION_INVALID_IDX 0xff

enum {
	FTM_FLAG_NONE		= 0x0000,
	FTM_FLAG_SCHED_INITED	= 0x0001,
	FTM_FLAG_SCHED_ACTIVE	= 0x0002,
	FTM_FLAG_ALL		= 0xffff
};
typedef int16 pdftm_flags_t;

struct pdftm_sched {
	struct {
		uint32 started;
		uint32 delay;
		uint32 sched_wait;
		uint32 burst;
		uint32 start_wait;
	} in;
	struct wl_timer *timer;
	pdftm_session_t *burst_sn;
	struct wl_info	*timer_wl;
};
typedef struct pdftm_sched pdftm_sched_t;

typedef struct pdftm_iov_tlv_digest pdftm_iov_tlv_digest_t;

#define FTM_DIG_INIT(_ftm, _sn, _dig) do {\
	_dig = (_ftm)->dig; \
	bzero((_dig), sizeof(*(_dig))); \
	(_dig)->ftm = _ftm; \
	(_dig)->session = _sn; \
} while (0)

#define FTM_DIG_RESET(_ftm, _sn, _dig) do { \
	bzero((_dig), sizeof(*(_dig))); \
	(_dig)->ftm = _ftm; \
	(_dig)->session = _sn; \
} while (0)

struct pdftm {
	uint32			magic;
	wlc_info_t		*wlc;
	pdsvc_t			*pd;
	int			scbh;		/* scb cubby handle */
	pdftm_flags_t		flags;
	pdftm_config_t		*config;
	wl_proxd_ftm_caps_t	caps;
	uint8			*enabled_bss;	/* ftm enabled bss [tx.rx]+ */
	uint8			enabled_bss_len;
	uint16			max_sessions;
	pdftm_session_t		**sessions;	/* array of sessions of len max_sessions * */
	uint16			num_sessions;	/* allocated - see cnt for valid  */
	wl_proxd_counters_t	*cnt;		/* counters */
	pdftm_session_idx_t	mru_idx;	/* lookup support  - most recently used idx */
	pdftm_sched_t		*sched;		/* scheduling support */
	uint16			last_sid;	/* allocation support */
	bcm_notif_h		h_notif;
	wlc_ftm_ranging_ctx_t	*rctx;		/* ranging context  - singleton */
	pdftm_time_t		last_tsf;
	uint32			last_pmu;
	pdftm_iov_tlv_digest_t	*dig;		/* iov support */
};
/* packing support  - digest of supported tlvs and pack/unpack state */

/* tlv info for pack/unpack, no unpack callback - its requires more validation, and
 * not necessary to unpack all as a given context may need to look at only few tlvs
 */

typedef struct pdftm_iov_tlv_dig_info pdftm_iov_tlv_dig_info_t;

typedef void (*pdftm_iov_tlv_packer_t)(const pdftm_iov_tlv_digest_t *dig,
	uint8 **buf, const uint8 *data, int len);
typedef uint16 (*pdftm_iov_tlv_get_len_t)(const pdftm_iov_tlv_digest_t *dig,
    const pdftm_iov_tlv_dig_info_t *tlv_info);

/* tlv info for pack/unpack, no unpack callback - its requires more validation, and
 * not necessary to unpack all as a given context may need to look at only few tlvs
 */
struct pdftm_iov_tlv_dig_info {
	uint16			id;		/* tlv id */
	uint16			len;
	int			dig_off;	/* digest offset */
	pdftm_iov_tlv_get_len_t	get_len;
	pdftm_iov_tlv_packer_t	pack;
};

/* note: because of ROM invalidation and parsing constraints, do not
 * extend the pdftm_iov_tlv_config_digest strucutre. Instead add
 * fields at the end of pdftm_iov_tlv_digest with config_ prefix.
 */
struct pdftm_iov_tlv_config_digest {
	uint32				*flags;
	chanspec_t			*chanspec;
	uint32				*tx_power;
	ratespec_t			*ratespec;
	wl_proxd_intvl_t		*burst_duration;
	wl_proxd_intvl_t		*burst_period;
	wl_proxd_intvl_t		*burst_ftm_sep;
	wl_proxd_intvl_t		*burst_timeout;
	uint16				*burst_num_ftm;
	uint16				*num_burst;
	uint8				*ftm_retries;
	wl_proxd_intvl_t		*init_delay;
	wl_proxd_event_mask_t		*event_mask;
	wl_proxd_flags_t		*flags_mask;
	struct ether_addr		*peer_mac;
	uint32				*session_flags;
	uint32				*session_flags_mask;
	uint32				*debug_mask;
	uint16				*rx_max_burst;	/* limit bursts */
	wl_proxd_avail_t		*avail;		/* local/peer availability */
	wl_proxd_session_id_list_t	*sid_list;
	wl_proxd_ranging_flags_t	*ranging_flags;
	wl_proxd_ranging_flags_t	*ranging_flags_mask;
	uint8				*dev_addr;
	uint8				*ftm_req_retries;
	wl_proxd_tpk_t			*tpk_ftm;
};
typedef struct pdftm_iov_tlv_config_digest pdftm_iov_tlv_config_digest_t;

/* note: because of ROM invalidation and parsing constraints, do not
 * extend the pdftm_iov_tlv_out_digest strucutre. Instead add
 * fields at the end of pdftm_iov_tlv_digest with out_ prefix.
 */
struct pdftm_iov_tlv_out_digest {
	const wl_proxd_ftm_info_t		*info;
	const wl_proxd_ftm_session_info_t	*session_info;
	const wl_proxd_ftm_session_status_t	*session_status;
	const wl_proxd_counters_t		*counters;
	const wl_proxd_rtt_result_t		*rtt_result;
	const uint8				*ftm_req;
	uint16					ftm_req_len;
	const wl_proxd_ranging_info_t		*ranging_info;
	const uint8				*lci_rep;
	uint16					lci_rep_len;
	const uint8				*civic_rep;
	uint16					civic_rep_len;
};
typedef struct pdftm_iov_tlv_out_digest pdftm_iov_tlv_out_digest_t;

struct pdftm_iov_tlv_digest {
	pdftm_t				*ftm;
	pdftm_session_t			*session;
	uint8				*bss_index;
	struct ether_addr		*bssid;

	/* response context */
	uint16				num_tlvs;
	const uint16			*tlv_ids;	/* in (for is_tlv_supported) and out */
	uint16				next_tlv_idx;
	const pdftm_iov_tlv_dig_info_t	*cur_tlv_info;

	/* tlv data - e.g. config, ftm state */
	struct {
		pdftm_iov_tlv_config_digest_t	config;	/* in and out */
		pdftm_iov_tlv_out_digest_t	out;
	} tlv_data;
	/* tbd lci req, lci, civic loc req, civic loc, avail map */
	/* add additional fields at end */
	uint8				*req_tlvs_end;	/* end of request buffer */
	wl_proxd_params_tof_tune_t	*tune;
	struct ether_addr		*cur_ether_addr; /* source address for Tx */
};

/* ranging support */
struct wlc_ftm_ranging_ctx {
	pdftm_t				*ftm;
	uint16				max_sids;
	uint16				num_done;	/* sessions finished */
	wl_proxd_status_t		status;		/* ranging status */
	wl_proxd_ranging_state_t	state;
	wl_proxd_ranging_flags_t	flags;
	uint16				num_sids;
	wlc_ftm_ranging_cb_t		cb;
	void				*cb_ctx;
	wl_proxd_session_id_t		*sids;
	wl_proxd_session_id_t		sid_min;	/* allocation support */
	wl_proxd_session_id_t		sid_max;
};

/* logging support */
#define FTM_INFO(_args) WL_NONE(_args)
#define FTM_TRACE(_args) WL_TRACE(_args)
#define FTM_ERR(_args) WL_ERROR(_args)
#define FTM_LOG_TSF_ARG(_tsf64) (uint32)((_tsf64) >> 32), (uint32)((_tsf64) & 0xffffffff)

#ifdef BCMDBG
#define FTM_DEBUG_OPT(_ftm, _opt) ((_ftm)->config->debug & (_opt))
#define FTM_LOG_OPT(_ftm, _opt, _args) do {\
	if (FTM_DEBUG_OPT(_ftm, _opt)) WL_APSTA _args; } while (0)

#define FTM_LOG(_ftm, _args) FTM_LOG_OPT(_ftm, WL_PROXD_DEBUG_LOG, _args)
#define FTM_LOGIOV(_ftm, _args) FTM_LOG_OPT(_ftm, WL_PROXD_DEBUG_IOV, _args)
#define FTM_LOGEVT(_ftm, _args) FTM_LOG_OPT(_ftm, WL_PROXD_DEBUG_EVENT, _args)
#define FTM_LOGSN(_ftm, _args) FTM_LOG_OPT(_ftm, WL_PROXD_DEBUG_SESSION, _args)
#define FTM_LOGPROTO(_ftm, _args) FTM_LOG_OPT(_ftm, WL_PROXD_DEBUG_PROTO, _args)
#define FTM_LOGSCHED(_ftm, _args) FTM_LOG_OPT(_ftm, WL_PROXD_DEBUG_SCHED, _args)
#define FTM_LOGRANGE(_ftm, _args) FTM_LOG_OPT(_ftm, WL_PROXD_DEBUG_RANGING, _args)
#define FTM_LOGPKT(_ftm, _args) FTM_LOG_OPT(_ftm, WL_PROXD_DEBUG_PKT, _args)
#define FTM_LOGSEC(_ftm, _args) FTM_LOG_OPT(_ftm, WL_PROXD_DEBUG_SEC, _args)

#define FTM_LOG_STATUS(_ftm, _err, _args) do {\
	if ((_err) != BCME_OK) {\
		WL_ERROR _args; } else { FTM_LOG(_ftm, _args); } \
	} while (0);

#define FTM_LOGDECL(stmt) stmt

#else

#if !defined(BCMROMBUILD)
/* #define FTM_DONGLE_DEBUG */
#endif /* !BCMROMBUILD */

#ifdef FTM_DONGLE_DEBUG
#define FTM_PRINTF(_args) printf _args
#else
#define FTM_PRINTF(_args)
#endif /* FTM_DONGLE_DEBUG */

#define FTM_LOG(_ftm, _args) FTM_PRINTF _args
#define FTM_LOGIOV(_ftm, _args) FTM_PRINTF _args
#define FTM_LOGEVT(_ftm, _args) FTM_PRINTF _args
#define FTM_LOGSN(_ftm, _args) FTM_PRINTF _args
#define FTM_LOGPROTO(_ftm, _args) FTM_PRINTF _args
#define FTM_LOGSCHED(_ftm, _args) FTM_PRINTF _args
#define FTM_LOG_STATUS(_ftm, _err, _args) FTM_PRINTF _args
#define FTM_LOGRANGE(_ftm, _args) FTM_PRINTF _args
#define FTM_LOGPKT(_ftm, _args) FTM_PRINTF  _args
#define FTM_LOGSEC(_ftm, _args) FTM_PRINTF  _args

#ifdef FTM_DONGLE_DEBUG
#define FTM_LOGDECL(stmt) stmt
#else
#define FTM_LOGDECL(stmt)
#endif /* FTM_DONGLE_DEBUG */

#endif /* BCMDBG */

#if defined(BCMDBG) || defined(FTM_DONGLE_DEBUG)
const char* pdftm_get_session_state_name(wl_proxd_session_state_t state);
#define FTM_DBG_GET_TSF(_ftm, _tsf) FTM_GET_TSF(_ftm, _tsf)
#else
#define FTM_DBG_GET_TSF(_ftm, _tsf)
#endif /* BCMDBG || FTM_DONGLE_DEBUG */

#define FTM_MAGIC 0x46544D00
#define FTM_OSH(_ftm) ((_ftm)->wlc->osh)
#define FTM_UNIT(_ftm) WLCWLUNIT((_ftm)->wlc)
#define FTM_PUB(_ftm) ((_ftm)->wlc->pub)
#define FTM_MCNX(_ftm) ((_ftm)->wlc->mcnx)
#define FTM_BAND(_ftm) (_ftm)->wlc->band
#define FTM_BANDTYPE(_ftm) FTM_BAND(_ftm)->bandtype
#define FTM_BANDUNIT(_ftm) FTM_BAND(_ftm)->bandunit
#define FTM_WL(_ftm) ((_ftm)->wlc->wl)
#define FTM_CMI(_ftm) (_ftm)->wlc->cmi

/* per bsscfg options (flags) */
enum {
	FTM_BSSCFG_OPTION_TX = 0,
	FTM_BSSCFG_OPTION_RX = 1,
	FTM_BSSCFG_OPTION_SECURE = 2
	/* additional options here */
};

#define FTM_BSSCFG_NUM_OPTIONS 3

/* option encoded in a block of bits that repeat for each bsscfg */
#define FTM_BSSCFG_OPTION_BIT(_bsscfg, _opt) (\
	(WLC_BSSCFG_IDX(_bsscfg) * FTM_BSSCFG_NUM_OPTIONS) + (_opt))

#define FTM_BSSCFG_FTM_ENABLED(_ftm, _bsscfg) isset((_ftm)->enabled_bss, \
	FTM_BSSCFG_OPTION_BIT(_bsscfg, FTM_BSSCFG_OPTION_TX))

#define FTM_BSSCFG_FTM_RX_ENABLED(_ftm, _bsscfg) isset((_ftm)->enabled_bss, \
	FTM_BSSCFG_OPTION_BIT(_bsscfg, FTM_BSSCFG_OPTION_RX))

#define FTM_WLC_UP(_ftm) FTM_PUB(_ftm)->up
#define FTM_WLC_HW_UP(_ftm) FTM_PUB(_ftm)->hw_up
#define FTM_BSSCFG(_ftm) (_ftm)->wlc->primary_bsscfg

#define FTM_RX_ENABLED(_flags) (((_flags) & WL_PROXD_FLAG_RX_ENABLED) != 0)
#define FTM_TX_LCI_ENABLED(_flags) (((_flags) & WL_PROXD_FLAG_TX_LCI) != 0)
#define FTM_TX_CIV_LOC_ENABLED(_flags) (((_flags) & WL_PROXD_FLAG_TX_CIVIC) != 0)
#define FTM_AVAIL_PUBLISH(_flags) (((_flags) & WL_PROXD_FLAG_AVAIL_PUBLISH) != 0)
#define FTM_AVAIL_SCHEDULE(_flags) (((_flags) & WL_PROXD_FLAG_AVAIL_SCHEDULE) != 0)

#ifdef WL_FTM_SECURITY
#define FTM_SECURE(_flags) (((_flags) & WL_PROXD_FLAG_SECURE) != 0)
#define FTM_SESSION_SECURE(_sn) (\
	((_sn)->config->flags & WL_PROXD_SESSION_FLAG_SECURE) != 0)
#define FTM_BSSCFG_SECURE(_ftm, _bsscfg) isset((_ftm)->enabled_bss, \
	FTM_BSSCFG_OPTION_BIT(_bsscfg, FTM_BSSCFG_OPTION_SECURE))
#else
#define FTM_SECURE(_flags) FALSE
#define FTM_SESSION_SECURE(_sn) FALSE
#define FTM_BSSCFG_SECURE(_ftm, _bsscfg) FALSE
#endif /* WL_FTM_SECURITY */

#ifdef BCMDBG
#define FTM_VALID(_ftm) ((_ftm) != NULL && (_ftm)->magic == FTM_MAGIC &&\
	(_ftm)->wlc != NULL && (_ftm)->pd != NULL)
#define FTM_VALID_RCTX(_rctx) ((_rctx != NULL) && FTM_VALID((_rctx)->ftm))
#else
#define FTM_VALID(_ftm) TRUE
#define FTM_VALID_RCTX(_rctx) TRUE
#endif // endif

#define FTM_UPDATE_FLAGS(_dst_flags, _src_flags, _mask) do {\
	_dst_flags &= ~(_mask); \
	_dst_flags |= (_src_flags) & (_mask); \
} while (0);

#define FTM_SESSION_IS_INITIATOR(_sn) (\
	((_sn)->config->flags & WL_PROXD_SESSION_FLAG_INITIATOR) != 0)

#define FTM_SESSION_IS_TARGET(_sn) (\
	((_sn)->config->flags & WL_PROXD_SESSION_FLAG_TARGET) != 0)

#define FTM_SESSION_START_WAIT_MASK (FTM_SESSION_KEY_WAIT | FTM_SESSION_EXT_SCHED_WAIT |\
		FTM_SESSION_TSF_WAIT)

#define FTM_SESSION_IN_START_WAIT(_sn) (\
	((_sn)->flags & FTM_SESSION_START_WAIT_MASK) != 0)

#define FTM_SESSION_IS_ASAP(_sn) (\
	((_sn)->config->flags & WL_PROXD_SESSION_FLAG_ASAP) != 0)

#define FTM_SESSION_IS_AUTO_VHTACK(_sn) (\
	((_sn)->config->flags & WL_PROXD_SESSION_FLAG_AUTO_VHTACK) != 0)

#define FTM_SESSION_IS_VHTACK(_sn) (\
	((_sn)->config->flags & WL_PROXD_SESSION_FLAG_VHTACK) != 0)

#define FTM_SESSION_IS_RTT_DETAIL(_sn) (\
	((_sn)->config->flags & WL_PROXD_SESSION_FLAG_RTT_DETAIL) != 0)

#define FTM_SESSION_INITIATOR_NOPREF(_sn, _flag) (\
	(((_sn)->config->flags & WL_PROXD_SESSION_FLAG_INITIATOR) != 0) &&\
	(((_sn)->config->flags & (_flag)) != 0))

#define FTM_SESSION_SEQ_EN(_sn) (\
	((_sn)->config->flags & WL_PROXD_SESSION_FLAG_SEQ_EN) != 0)

#define FTM_SESSION_PRE_SCAN_ENABLED(_flags) (((_flags) & WL_PROXD_SESSION_FLAG_PRE_SCAN) != 0)

#define FTM_SESSION_NEED_TSF_SYNC(_sn) ((_sn) && (_sn)->ftm_state && \
	((_sn)->ftm_state->flags & FTM_SESSION_STATE_TSF_SYNC))

#define FTM_SESSION_SEND_TSF_SYNC(_sn) ((_sn) && (_sn)->ftm_state && \
	((_sn)->ftm_state->flags & FTM_SESSION_STATE_TSF_SYNC))

#define FTM_SCHED_NAME "ftm-sched"

#define FTM_GET_DEV_TSF(_ftm, _tsf64) {\
	uint32 _hi = 0, _lo = 0; \
	if ((_ftm)->wlc->clk) \
		wlc_read_tsf((_ftm)->wlc, &_lo, &_hi); \
	(_tsf64) = (uint64)_hi << 32 | _lo; \
}

#define FTM_GET_TSF(_ftm, _tsf64) { (_tsf64) =  pdftm_get_pmu_tsf(_ftm); }
#ifdef WL_FTM_MSCH
#define FTM_BURST_ALLOWED(_flags)	(((_flags) & FTM_SESSION_ON_CHAN) != 0)
#else
#define FTM_BURST_ALLOWED(_flags)	TRUE
#endif /* WL_FTM_MSCH */

/* given an expiration and current tsf, check whether interval has expired */
#define FTM_INTVL_EXPIRED(_exp_tsf, _cur_tsf, _intvl) (((_exp_tsf) < (_cur_tsf)) || \
	(((_exp_tsf) - (_cur_tsf)) < FTM_INTVL2USEC(_intvl)))

#ifdef MCHAN
#define FTM_SESSION_CHANSPEC(_ftm, _sn, _rx_bsscfg) (\
	((_rx_bsscfg)->associated && (_rx_bsscfg)->current_bss != NULL) ?
		(_rx_bsscfg)->current_bss->chanspec : (_sn)->bsscfg->chanspec))
#else
#define FTM_SESSION_CHANSPEC(ftm, _sn, _rx_bsscfg) (\
	((_rx_bsscfg)->associated && (_rx_bsscfg)->current_bss != NULL) ? \
		(_rx_bsscfg)->current_bss->chanspec : ftm->wlc->home_chanspec)
#endif /* MCHAN */

/* limit bursts per session, althouth protocol allows upto 2^15 */
#define FTM_NUM_BURST_NO_PREFERENCE			(1 << 15)
#define FTM_MAX_NUM_BURST_EXP				14
#define FTM_SESSION_RX_MAX_BURST			1024
#define FTM_SESSION_RX_DEFAULT_NUM_BURST	2
#define FTM_SESSION_RX_DEFAULT_BURST_TIMEOUT_USEC 128000
#define FTM_SESSION_NON_ASAP_BURST_DELAY_US 50000

#define FTM_SESSION_DELAYED(_sn) ((_sn)->ftm_state->burst_start > \
		(_sn)->ftm_state->delay_exp)

#define FTM_SESSION_TSPT1NS(_sn) (((_sn)->flags & FTM_SESSION_TSPT1NS) != 0)

/* event size - data len includes all tlvs */
#define FTM_EVENT_ALLOC_SIZE(_data_len, _num_tlvs) (OFFSETOF(wl_proxd_event_t, tlvs) + \
	BCM_XTLV_SIZE_FOR_TLVS((_data_len), (_num_tlvs), FTM_XTLV_OPTIONS))

#define FTM_PERCENT(_val, _percent) pdftm_div64((_val) * (_percent), 100)

#define FTM_MAX_TIME 0xffffffffffffffffULL
#define FTM_SCHED_OVERLAP(_s1, _e1, _s2, _e2) (((_s2) >= (_s1) && (_s2) < (_e1)) || \
	((_s1) >= (_s2) && (_s1) < (_e2)))

#define FTM_SCHED_DELAY_EXP_ADJ_US 2000
#define FTM_GETEXP(_x, _e) do {\
	uint _i; \
	uint64 _x2 = (_x); \
	for (_i = 0, (_e) = 0; _i < (sizeof(_x) * NBBY); ++_i, (_x2) >>= 1) {\
		if ((_x2) & 0x1) (_e) = _i; \
	}\
} while (0)

#define FTM_PROTO_BEXP(_val, _exp) FTM_GETEXP(_val, _exp)

#define FTM_GETEXP_ROUNDUP(_val, _nbits, _res) { \
	int _shift = (_nbits); \
	while ((_shift > 0) && (((1 << (_shift)) & (_val)) == 0)) { \
		(_shift)--; \
	} \
	_res =  _shift + 1; \
	if ((1U << _shift)  == (_val)) { \
		_res = (_res) - 1; \
	} \
}

#define FTM_PROTO_BURSTTMO(_val, _exp) do {\
	uint32 _val2; \
	_val2 = (uint32)(_val) / 250; \
	FTM_GETEXP_ROUNDUP(_val2, 9, _exp); \
} while (0)

/* resolution is in pico per 11mc D4.3, but we also support 0.1ns */
#define FTM_PICOINHZ(_sn) (FTM_SESSION_TSPT1NS(_sn) ? 100 : 1)

#define FTM_PICO2HZ(_sn, _pico) ((FTM_PICOINHZ(_sn) > 1) ? \
	pdftm_div64(_pico, FTM_PICOINHZ(_sn)) : _pico)

#define FTM_HZ2PICO(_sn, _pico) ((_pico) * FTM_PICOINHZ(_sn))

#define FTM_PROTO_PICO2TS(_sn, _ts, _in_val) do {\
	uint64 _val; \
	_val = FTM_PICO2HZ(_sn, _in_val); \
	(_ts)[0] = (_val) & 0xff; (_ts)[1] = (_val) >> 8 & 0xff; \
	(_ts)[2] = (_val) >> 16 & 0xff; (_ts)[3] = (_val) >> 24 & 0xff; \
	(_ts)[4] = (_val) >> 32 & 0xff; (_ts)[5] = (_val) >> 40 & 0xff; \
} while (0)

#define  FTM_PROTO_TS2PICO(_sn, _ts, _pico) do {\
	(_pico) = (_ts)[5]; \
	(_pico) = (_pico) << 8 | (_ts)[4]; \
	(_pico) = (_pico) << 8 | (_ts)[3]; \
	(_pico) = (_pico) << 8 | (_ts)[2]; \
	(_pico) = (_pico) << 8 | (_ts)[1]; \
	(_pico) = (_pico) << 8 | (_ts)[0]; \
	_pico = FTM_HZ2PICO(_sn, _pico); \
} while (0);

#define FTM_PROTO_PICO2TSERR(_sn, _tserr, _in_val) {\
	uint16 _val; \
	_val = (uint16)FTM_PICO2HZ(_sn, _in_val); \
	DOT11_FTM_ERR_SET_MAX_ERR(_tserr, _val); \
}

#define FTM_PROTO_TSERR2PICO(_sn, _tserr, _pico) do {\
	(_pico) = (_tserr)[1]; \
	(_pico) = (_pico) << 8 | (_tserr)[0]; \
	_pico = FTM_HZ2PICO(_sn, _pico); \
} while (0)

#define FTM_SESSION_DEFAULT_NUM_BURST 1
#define FTM_SESSION_DEFAULT_RETRIES PROXD_DEFAULT_RETRY_CNT
#define FTM_SESSION_DEFAULT_CHANSPEC 0	/* no default, invalid */
#define FTM_SESSION_DEFAULT_RATESPEC 0  /* no default, invalid */
#define FTM_SESSION_DEFAULT_TX_POWER PROXD_DEFAULT_TX_POWER
#define FTM_SESSION_DEFAULT_BURST_TIMEOUT_MS 20 /* vs spec 10ms */
#define FTM_SESSION_DEFAULT_BURST_DURATION_MS 64
#define FTM_SESSION_DEFAULT_BURST_NUM_FTM 5
#define FTM_SESSION_DEFAULT_BURST_PERIOD_MS 5000

/* Default FTM separation in micro seconds */
#define FTM_SEP_SEQ_EN	3600u
#define FTM_SEP_CM3	1400u
#ifdef TOF_DEFAULT_USE_MULTICORE
#define FTM_SEP_80M	3100u
#define FTM_SEP_40M	1700u
#define FTM_SEP_20M	500u
#else
#define FTM_SEP_80M	1300u
#define FTM_SEP_40M	700u
#define FTM_SEP_20M	200u
#endif /* TOF_DEFAULT_SINGLE_CORE_EN */

#define FTM_SESSION_FOR_SID(_ftm, _sid) pdftm_find_session(_ftm, \
	&(_sid), NULL, WL_PROXD_SESSION_FLAG_NONE, WL_PROXD_SESSION_STATE_NONE)
#define FTM_SESSION_FOR_PEER(_ftm, _peer_mac, _flags) pdftm_find_session(_ftm, \
	NULL, _peer_mac, (_flags), WL_PROXD_SESSION_STATE_NONE)

#define FTM_SET_DIST(_pd_dist, _pd_distu, _ftm_dist) do {\
	switch (_pd_distu) {\
	case PD_DIST_1BY16M: _ftm_dist = (_pd_dist) << 4; break; \
	case PD_DIST_1BY256M: _ftm_dist = (_pd_dist); break; \
	case PD_DIST_UNKNOWN: _ftm_dist = -1; break; \
	case PD_DIST_CM: _ftm_dist  = ((_pd_dist) << 8) / 100; break; \
	default: ASSERT(FALSE); break; \
	}\
} while (0)

#define FTM_INIT_NOTIFY_INFO(_ni, _sn) do {\
	bzero(_ni, sizeof(*(_ni))); \
	(_ni)->sn = (_sn); \
} while (0)

#define FTM_INIT_SESSION_NOTIFY_INFO(_ni, _sn, _addr, _evt, _chan, _rspec, _data, _len) do {\
	FTM_INIT_NOTIFY_INFO(_ni, _sn); \
	(_ni)->addr = (_addr); \
	(_ni)->event_type = (_evt); (_ni)->chanspec = (_chan); \
	(_ni)->ratespec = (_rspec); (_ni)->data = (_data); (_ni)->data_len = (_len); \
} while (0)

#define FTM_INIT_RANGING_NOTIFY_INFO(_ni, _evt, _rctx) { \
	FTM_INIT_NOTIFY_INFO(_ni, NULL); \
	(_ni)->event_type = (_evt); \
	(_ni)->rctx = (_rctx); \
}

/* per spec - single chain only */
#define FTM_NSS 1

#define FTM_XTLV_OPTIONS BCM_XTLV_OPTION_ALIGN32

#define FTM_CNT(_ftm, _sn, _ctr)  do {\
	++((_ftm)->cnt->_ctr); \
	if (_sn) ++((_sn)->cnt->_ctr); \
} while (0)

#define FTM_CNT_NOSN(_ftm, _ctr) (++((_ftm)->cnt->_ctr))
#define FTM_CNT_SN(_sn, _ctr) (++((_sn)->cnt->_ctr))

/* maximum channel switch time in micro-seconds */
#define FTM_MAX_CHANSW_US 4000

/* don't reschedule if backoff requested by target unless this long  */
#define FTM_SESSION_RESCHED_BACKOFF_USEC FTM_MAX_CHANSW_US

/* helpers to check if override from responder allowed - allow for now */
#define FTM_INFRA_STA(_bsscfg) (BSSCFG_STA(_bsscfg) &&\
	(_bsscfg)->BSS && (_bsscfg)->associated)
#define FTM_INITIATOR_ALLOW_CIOVRD(_ftm, _bsscfg) (TRUE || FTM_INFRA_STA(_bsscfg))
#define FTM_INITIATOR_ALLOW_DUROVRD(_ftm, _bsscfg, _asap) (\
	TRUE || (FTM_INFRA_STA(_bsscfg) && (_asap) != 0))
#define FTM_INITIATOR_ALLOW_BPEROVRD(_ftm, _bsscfg) \
	 (TRUE || FTM_INFRA_STA(_bsscfg))
/* allow number of bursts to only decrease. except no change for single bursts */
#define FTM_INITIATOR_ALLOW_NBURSTOVRD(_ftm, _bsscfg, _nburst, _sn_nburst) (\
	((_sn_nburst) > 1) &&\
	((_nburst) > 0) && ((_sn_nburst) > (_nburst)))

#define FTM_MAX_RCTX_SIDS 16

#ifdef BCMDBG
#define FTM_RCTX_VALID(_rctx) ((_rctx) != NULL && FTM_VALID((_rctx)->ftm))
#else
#define FTM_RCTX_VALID(_rctx) TRUE
#endif /* BCMDBG */

/* for availability */
#define FTM_AVAIL_SLOTS_INC				8u

#define FTM_SESSION_TSF_SCAN_ACTIVE_TIME_MS		10u
#define FTM_SESSION_TSF_SCAN_PASSIVE_TIME_MS		200u
#define FTM_SESSION_TSF_SCAN_NUM_PROBES			2u
#define FTM_SESSION_TSF_SCAN_RETRY_MS			100u
#define FTM_SESSION_MSCH_BURST_ALLOWED_RETRY_MS		1u
#define FTM_SESSION_TSF_SCAN_RETRY_MAX_ATTEMPT		3u

#define FTM_SESSION_SCAN_RETRIES_DONE(attempt) ((attempt) > \
		FTM_SESSION_TSF_SCAN_RETRY_MAX_ATTEMPT)

#define FTM_STATE_SET_BURST_DURATION(_st) FTM_INIT_INTVL(&(_st)->retry_after, \
				(_st)->burst_end - (_st)->burst_start, WL_PROXD_TMU_MICRO_SEC)

#define FTM_MIN_BURST_DURATION_US 8000

/* compute burst duration based on ftm sep and num ftm in burst.
 * 11mc - Fine timing measurement negotiation - 10.24.6.3 11mc4.x
 * 100 micro approx for TFTM + SIFS + ACK
 */
#define FTM_SESSION_INIT_BURST_DURATION_US(_dur, _num_ftm, _sep, _retries) { \
	_dur = MAX(((((_num_ftm) * (_retries + 1)) - 1) * MAX((_sep), 300) + 100), \
		 FTM_MIN_BURST_DURATION_US); \
}

#define FTM_BODY_LEN_AFTER_TLV(_body, _body_len, _tlv, _tlv_size) (\
	((const uint8 *)(_body) + (_body_len)) - ((const uint8 *)tlv + (_tlv_size)))

#define FTM_PKT_DUMP_BUFSZ 512

enum {
	FTM_PARAM_OVRD_NONE		= 0x00,
	FTM_PARAM_OVRD_ASAP		= 0x01,
	FTM_PARAM_OVRD_CHANSPEC		= 0x02,
	FTM_PARAM_OVRD_FTM_SEP		= 0x04,
	FTM_PARAM_OVRD_BURST_DURATION	= 0x08,
	FTM_PARAM_OVRD_BURST_PERIOD	= 0x10,
	FTM_PARAM_OVRD_NUM_FTM		= 0x20,
	FTM_PARAM_OVRD_NUM_BURST	= 0x40
};

#define FTM_TSF_64KTU (1 << (FTM_PARAMS_PARTIAL_TSF_SHIFT + \
	FTM_PARAMS_PARTIAL_TSF_BIT_LEN))

typedef int32 ftm_param_ovrd_t;

/* cubby types */

struct ftm_scb {
	uint16	len;
	uint8	tpk_max;
	uint8	tpk_len;
	uint8	*tpk;

	/* add additional fields above
	 * additional data follows
		tpk
	 */
};
typedef struct ftm_scb ftm_scb_t;

#define FTM_SCB_TPK_VALID(_ftm_scb) (\
	(_ftm_scb) != NULL && (_ftm_scb)->tpk_len != 0 && \
	(_ftm_scb)->tpk != NULL)

/* buffer used to compose tpk data for hashing */
#define FTM_TPK_BUF_LEN 128

#define TOF_PRINTF(x)   printf x

/* prototypes */

extern const pdburst_callbacks_t pdftm_burst_callbacks;

/* session */

void pdftm_init_session_defaults(pdftm_t *ftm, pdftm_session_config_t *sncfg);

/* invalidate session and release memory (detach) for all sessions */
void pdftm_free_sessions(pdftm_t *ftm, wlc_bsscfg_t *bsscfg, bool detach);

/* find or allocate a session */
int pdftm_alloc_session(pdftm_t *ftm, wlc_bsscfg_t *bsscfg, wl_proxd_session_id_t sid,
	pdftm_session_t **sn, bool *created);

/* release a session, mark it invalid, memory may not be freed */
void pdftm_free_session(pdftm_session_t **sn);

/* find an existing session */
pdftm_session_t* pdftm_find_session(pdftm_t *ftm, wl_proxd_session_id_t *sid,
    const struct ether_addr *peer_mac, wl_proxd_session_flags_t flags,
    wl_proxd_session_state_t state);

/* check session parameters for consistency/correctness, and fix up to defaults */
int pdftm_validate_session(pdftm_session_t *sn);

/* start a session */
int pdftm_start_session(pdftm_session_t *sn);

/* stop a session */
int pdftm_stop_session(pdftm_session_t *sn, wl_proxd_status_t status);

/* check if session is on chan */
bool pdftm_session_is_onchan(pdftm_session_t *sn);

/* resolve tx bsscfg for session */
wlc_bsscfg_t* pdftm_get_session_tx_bsscfg(const pdftm_session_t *sn);

/* resolve tsf for session */
int pdftm_get_session_tsf(const pdftm_session_t *sn, uint64 *tsf);

/* resolve the ratespec for a session */
int pdftm_session_resolve_ratespec(pdftm_t *ftm,
	chanspec_t cspec, ratespec_t *rspec);

/* resolve the number of ftms per burst */
uint16 pdftm_resolve_num_ftm(pdftm_t *ftm, wl_proxd_session_flags_t flags,
	chanspec_t cspec);

/* convert tsf to/from local/session; local arg refers to input tsf */
uint64 pdftm_convert_tsf(const pdftm_session_t *sn, uint64 tsf, bool local);

/* state record */
void pdftm_record_session_state(pdftm_session_t *sn);

/* state update */
int pdftm_change_session_state(pdftm_session_t *sn,
	wl_proxd_status_t status, wl_proxd_session_state_t new_state);

/* get burst parameters */
int pdftm_get_burst_params(pdftm_session_t *sn,
	const uint8 *req, uint req_len, const wlc_d11rxhdr_t *req_wrxh,
	ratespec_t req_rspec, pdburst_params_t *params);

/* transmit ftm frame */
int pdftm_tx_ftm(pdftm_t *ftm, pdftm_session_t *sn,
	const pdburst_tsinfo_t *tsi, int req_err);

/* packing support */
int pdftm_iov_pack(pdftm_t *ftm, int rsp_max, wl_proxd_tlv_t *rsp_tlvs,
    const uint16 tlv_ids[], int num_tlvs, pdftm_iov_tlv_digest_t *dig, int *rsp_len);

/* initialize and return the ftm module iov digest */
pdftm_iov_tlv_digest_t*  pdftm_iov_dig_init(pdftm_t *ftm, pdftm_session_t *sn);

/* reset a given iov digest */
void pdftm_iov_dig_reset(pdftm_t *ftm, pdftm_session_t *sn,
	pdftm_iov_tlv_digest_t *dig);

/* scheduling */
int pdftm_init_sched(pdftm_t *ftm);
void pdftm_sched(void *arg);
int pdftm_wake_sched(pdftm_t *ftm, pdftm_session_t *sn);
int pdftm_end_sched(pdftm_t *ftm);
int pdftm_sched_process_session_end(pdftm_t *ftm, pdftm_session_idx_t start,
	pdftm_session_idx_t end);
int pdftm_sched_session(pdftm_t *ftm, pdftm_session_t *sn);
int vs_prep_mf_buf(uint8 *body, uint body_max, uint len, pdburst_session_info_t *bsi,
	pdftm_session_t *sn, pdburst_frame_type_t type, uint *prev_ie_len,
	uint *vs_ie_len);

/* utils */
uint64 pdftm_div64(uint64 val, uint16 div);
bcm_tlv_t* pdftm_tlvdup(pdftm_t *ftm, const bcm_tlv_t *tlv);
uint64 pdftm_get_pmu_tsf(pdftm_t *ftm);
/* burst support */
int pdftm_setup_burst(pdftm_t *ftm, pdftm_session_t *sn, const uint8 *req, uint req_len,
	const wlc_d11rxhdr_t *req_wrxh, ratespec_t req_rspec);

/* notify protocol */
void pdftm_proto_notify(pdftm_t *ftm, pdftm_session_t *sn, wl_proxd_event_type_t event_type);

/* determine ftm sep in microseconds for a session */
uint16 pdftm_resolve_ftm_sep(pdftm_t *ftm, wl_proxd_session_flags_t flags,
	chanspec_t chanspec);

/* validate a ratespec for a session. chanspec/ratespec must be resolved before
 * calling this function
 */
int pdftm_validate_ratespec(pdftm_t *ftm, pdftm_session_t *sn);

/* determine if proxd needs to be enabled */
bool pdftm_need_proxd(pdftm_t *ftm, uint flag);

/* get session result. space for num_rtt samples must be allocated */
int pdftm_get_session_result(pdftm_t *ftm, pdftm_session_t *sn,
	wl_proxd_rtt_result_t *rp, int num_rtt);

/* get length of a ranging event */
int pdftm_ranging_get_event_len(wlc_ftm_ranging_ctx_t *rctx,
	wl_proxd_event_type_t event_type);

/* pack ranging tlvs into tlvs */
int pdftm_ranging_pack_tlvs(wlc_ftm_ranging_ctx_t *rctx,
    wl_proxd_event_type_t event_type, int max_tlvs_len,
	uint8* tlvs, int *tlvs_len);

/* handle availability update */
int pdftm_sched_avail_update(pdftm_t *ftm);
int pdftm_session_avail_update(pdftm_session_t *sn);

/* allowed to rx based on avail ? */
bool pdftm_sched_rx_allowed(pdftm_t *ftm, pdftm_session_t *sn,
	wlc_bsscfg_t *bsscfg, chanspec_t rx_cspec);

/* pack ranging session ids */
void pdftm_ranging_pack_sessions(wlc_ftm_ranging_ctx_t *rctx,
	uint8 **buf, int buf_len);

/* save rx lci/civic-report */
int pdftm_session_set_lci_civic_rpt(pdftm_session_t *sn,
	uint16 tlv_id, uint8 *lci_civic_rep, uint16 lci_civic_rep_len);

#if defined(BCMDBG) || defined(FTM_DONGLE_DEBUG)
/* dump a session */
void pdftm_dump_session(const pdftm_t *ftm, const pdftm_session_t *sn,
	struct bcmstrbuf *b);
void pdftm_dump_session_config(wl_proxd_session_id_t sid,
    const pdftm_session_config_t *sncfg, struct bcmstrbuf *b);
void pdftm_dump_ranging_ctx(const wlc_ftm_ranging_ctx_t *rctx, struct bcmstrbuf *b);
void pdftm_dump_pkt_body(pdftm_t *ftm, const uint8 *body, uint body_len,
	struct bcmstrbuf *b);
#endif /* BCMDBG || FTM_DONGLE_DEBUG */

int pdftm_start_session_tsf_scan(pdftm_session_t *sn);

pdftm_session_config_t* pdftm_alloc_session_config(pdftm_t *ftm);
void pdftm_free_session_config(pdftm_t *ftm, pdftm_session_config_t **sncfg);
void pdftm_copy_session_config(pdftm_t *ftm, pdftm_session_config_t *dst,
    const pdftm_session_config_t *src);
int pdftm_msch_begin(pdftm_t *ftm, pdftm_session_t *sn);
int pdftm_msch_end(pdftm_t *ftm, pdftm_session_t *sn);

int pdftm_session_trig_tsf_update(pdftm_session_t *sn,
	const dot11_ftm_sync_info_t *tsf_si);

void pdftm_bsscfg_clear_options(pdftm_t *ftm,
	wlc_bsscfg_t *bsscfg);

#ifdef WL_RANGE_SEQ
#define FTM_DEFAULT_TARGET_BITFLIPS     0xFFFF
#define FTM_DEFAULT_TARGET_SNR          0xFFFF
extern wl_proxd_phy_error_t
ftm_vs_tgt_snr_bitfips(pdburst_method_ctx_t ctx, uint16 snr_thresh,
	uint16 bitflip_thresh, wl_proxd_snr_t *tof_target_snr,
	wl_proxd_bitflips_t *tof_target_bitflips);

extern void
ftm_vs_reset_target_side_data(pdburst_method_ctx_t ctx);
#endif /* WL_RANGE_SEQ */

/* vendor support */
int pdftm_vs_get_frame_len(void *ctx, pdburst_frame_type_t type,
	const pdburst_session_info_t *bsi);
int pdftm_vs_prep_tx(void *ctx, pdburst_frame_type_t type,
	uint8 *body, uint body_max, uint *body_len, pdburst_session_info_t *bsi);
int pdftm_vs_rx(pdburst_method_ctx_t ctx, pdburst_frame_type_t type,
	const uint8 *body, uint body_len, pdburst_session_info_t *bsi);

#ifdef WL_RANGE_SEQ
extern int pdftm_vs_info_update(pdftm_session_t *sn, ftm_vs_info_t vsinfo);
extern int pdftm_process_vs(pdftm_t *ftm, pdftm_session_t *sn,
		uint8 meas_type, const uint8 **body, int * body_len);
extern bcm_tlv_t *pdftm_find_vs(pdftm_t *ftm, pdftm_session_t *sn,
		uint8 meas_type, uint8 *body, uint * body_len);
extern int pdftm_get_vs_len(pdftm_t * ftm, pdftm_session_t *sn, uint8 *body, int body_len);
extern int pdftm_vs_len(pdburst_method_ctx_t ctx, pdburst_frame_type_t type);
extern void pdftm_update_session_cfg_flags(pdburst_method_ctx_t ctx, bool seq_en);
extern bool pdftm_get_session_cfg_flag(pdburst_method_ctx_t ctx, bool seq_en);
#endif /* WL_RANGE_SEQ */

#define FTM_VENDOR_IE_HDR_LEN 7
#ifdef WL_RANGE_SEQ
#define VS_IE_ID                0xDD
#define REQ_TLV_LEN     sizeof(ftm_vs_seq_params_t) + sizeof(ftm_vs_req_params_t) + \
		FTM_VENDOR_IE_HDR_LEN + (2 * TLV_HDR_LEN)

#define MEAS_TLV_LEN sizeof(ftm_vs_sec_params_t) + sizeof(ftm_vs_meas_info_t) + \
	sizeof(ftm_vs_timing_params_t) + (3 * TLV_HDR_LEN) + FTM_VENDOR_IE_HDR_LEN
#else
#define MEAS_TLV_LEN sizeof(ftm_vs_sec_params_t) + sizeof(ftm_vs_meas_info_t) + \
	(2 * TLV_HDR_LEN) + FTM_VENDOR_IE_HDR_LEN
#endif /* WL_RANGE_SEQ */
#define MEAS_INFO_IE_TLV_LEN sizeof(ftm_vs_meas_info_t) + (1 * TLV_HDR_LEN) + FTM_VENDOR_IE_HDR_LEN

/* FTM vendor OUI and type - Can be customized here */
#define FTM_VS_OUI BRCM_PROP_OUI
#define FTM_VS_IE_TYPE BRCM_FTM_IE_TYPE
#define FTM_VS_PARAMS_VERSION BCM_FTM_VS_PARAMS_VERSION

/* cubby support */
int pdftm_scb_init(void *ctx, scb_t *scb);
void pdftm_scb_deinit(void *ctx, scb_t *scb);
void  pdftm_scb_dump(void *ctx, scb_t *scb, struct bcmstrbuf *b);

#ifdef BCMDBG
#define PDFTM_SCB_DUMP pdftm_scb_dump
#else
#define PDFTM_SCB_DUMP NULL
#endif /* BCMDBG */

#define FTM_SCB_PTR(ftm, scb) (ftm_scb_t **)SCB_CUBBY(scb, (ftm)->scbh)
#define FTM_SCB(ftm, scb) (*FTM_SCB_PTR(ftm, scb))

ftm_scb_t* pdftm_scb_alloc(pdftm_t *ftm, scb_t *scb, bool *created);

/* security */
int pdftm_sec_check_rx_policy(pdftm_t *ftm, pdftm_session_t *sn /* optional */,
	const dot11_management_header_t *hdr);
int pdftm_sec_validate_session(pdftm_t *ftm, pdftm_session_t *sn);
void pdftm_calc_ri_rr(const uint8 *tpk, uint32 tpk_len, const uint8* rand_ri_rr,
	uint32 rand_ri_rr_len, uint8 *output, unsigned int output_len);

int pdftm_session_register_ext_sched(pdftm_t *ftm, pdftm_session_t *sn);
int pdftm_extsched_timeslot_available(pdftm_session_t *sn, uint64 duration_ms, chanspec_t chanspec,
	const struct ether_addr *peer);
int pdftm_extsched_timeslot_unavailable(pdftm_session_t *sn);
int pdftm_ext_sched_session_ready(pdftm_session_t *sn);

/* ext sched validation */
int pdftm_sched_validate_session(pdftm_t *ftm, pdftm_session_t *sn);
int ftm_session_validate_chanspec(pdftm_t *ftm, pdftm_session_t *sn);
/* get ftm params for nan ranging */
int ftm_proto_get_ftm1_params(pdftm_t * ftm, pdftm_session_t * sn, int req_err,
	dot11_ftm_params_t * params);

void * pdftm_session_get_burstp(wlc_info_t *wlc, struct ether_addr *peer, uint8 flag);

#define FTM_DEFAULT_TARGET_BITFLIPS	0xFFFF
#define FTM_DEFAULT_TARGET_SNR		0xFFFF
extern wl_proxd_phy_error_t
ftm_vs_tgt_snr_bitfips(pdburst_method_ctx_t ctx, uint16 snr_thresh,
	uint16 bitflip_thresh, wl_proxd_snr_t *tof_target_snr,
	wl_proxd_bitflips_t *tof_target_bitflips);
extern void
ftm_vs_reset_target_side_data(pdburst_method_ctx_t ctx);

#define TIMING_TLV_START_DELTA_MAX 330 /* 330 us */
#define TIMING_TLV_START_SEQ_TIME 273  /* 273 us */
#define TIMING_TLV_DELTA_TIME_TX2RX 40  /* 40 us */
#define TIMING_TLV_START_SEQ_TIME_MIN 270  /* 270 us */
#define TIMING_TLV_START_SEQ_TIME_MAX 300  /* 300 us */
#define TIMING_TLV_DELTA_TIME_TX2RX_MIN 20  /* 20 us */
#define TIMING_TLV_DELTA_TIME_TX2RX_MAX 60  /* 60 us */

#endif /* _pdftmpvt_h_ */
