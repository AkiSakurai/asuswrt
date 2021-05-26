/*
 * 802.11ax uplink MU scheduler and scheduler statistics module
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id:$
 */

#ifndef _wlc_ulmu_h_
#define _wlc_ulmu_h_

#include <wlc_types.h>

/* ulmu ul policy definition */
#define ULMU_POLICY_DISABLE		0
#define ULMU_POLICY_BASIC		1
#define ULMU_POLICY_MAX			1
#define ULMU_POLICY_AUTO		(-1)

#define ULMU_USRCNT_MAX			32

#define ULMU_OFDMA_TRSSI_MAP			110
#define ULMU_OFDMA_TRSSI_MIN			20  /* -90 dBm */
#define ULMU_OFDMA_TRSSI_MAX			90  /* -20 dBm */
#define ULMU_OFDMA_TRSSI_INIT			80  /* -30 dBm */

#define ULMU_TRIG_FIFO			5

#define ULMU_NF_AGGN		4 /* normalization factor */
#define ULMU_NF_AGGLEN		10 /* normalization factor */
#define ULMU_EMA_ALPHA		4

#define ULMU_RSPEC_INVD			((uint16) -1)

#define ULMU_MOVING_AVG(p, cur, alpha) \
	do {				\
		*p -= (*p) >> alpha;	\
		*p += cur >> alpha;	\
	} while (0);

/* attach/detach */
wlc_ulmu_info_t *wlc_ulmu_attach(wlc_info_t *wlc);
void wlc_ulmu_detach(wlc_ulmu_info_t *ulmu);

/* Driver UL trigger interface */

#define ULMU_PACKET_TRIGGER		0
#define ULMU_TWT_TRIGGER		1

#define ULMU_QOSNULL_LIMIT		6
#define ULMU_TIMEOUT_LIMIT		7
#define ULMU_CB_THRESHOLD		100
#define ULMU_CB_TEST_THRESHOLD	10

#define ULMU_STATUS_INPROGRESS	0
#define ULMU_STATUS_THRESHOLD	1
#define ULMU_STATUS_COMPLETE	2
#define ULMU_STATUS_QOSNULL		10
#define ULMU_STATUS_TIMEOUT		20
#define ULMU_STATUS_TRIGCNT		30
#define	ULMU_STATUS_WATCHDOG	40
#define ULMU_STATUS_NOTADMITTED	50
#define ULMU_STATUS_EVICTED		60
#define ULMU_STATUS_SUPPRESS	70
#define ULMU_STATUS_UNKNOWN		((uint32) (-1))

#define EVICT_CLIENT FALSE
#define ADMIT_CLIENT TRUE

#define ULMU_DRVR_RT_MASK	0x1
#define ULMU_DRVR_RT_SHIFT	0
#define ULMU_DRVR_SP_MASK	0x2
#define ULMU_DRVR_SP_SHIFT	1
#define ULMU_DRVR_SET_RT(a, v)	(a) = (((a) & ~ULMU_DRVR_RT_MASK) | ((v) << ULMU_DRVR_RT_SHIFT))
#define ULMU_DRVR_GET_RT(a)	(((a) & ULMU_DRVR_RT_MASK) >> ULMU_DRVR_RT_SHIFT)
#define ULMU_DRVR_SET_SP(a, v)	(a) = (((a) & ~ULMU_DRVR_SP_MASK) | ((v) << ULMU_DRVR_SP_SHIFT))
#define ULMU_DRVR_GET_SP(a)	(((a) & ULMU_DRVR_SP_MASK) >> ULMU_DRVR_SP_SHIFT)

struct packet_trigger_info;
typedef struct packet_trigger_info packet_trigger_info_t;

typedef int (*ulmu_cb_t) (
			wlc_info_t *wlc,
			struct scb *scb,
			packet_trigger_info_t *ti,
			void *arg,
			uint32 status_code,
			uint32 bytes_consumed,
			uint32 avg_pktsz,
			uint32 duration);

struct packet_trigger_info {
	uint32 trigger_bytes;
	ulmu_cb_t callback_function;
	void *callback_parameter;
	uint32 callback_reporting_threshold;
	uint32 qos_null_threshold;
	uint32 failed_request_threshold;
	uint32 watchdog_timeout;
	bool multi_callback;
	bool post_utxd;
	void *pkt;
};

/* Placeholder for TWT trigger data */
typedef struct twt_trigger_info {
	uint32 trigger_bytes;
} twt_trigger_info_t;

typedef struct wlc_ulmu_trigger_info {
	union {
		packet_trigger_info_t packet_trigger;
		twt_trigger_info_t twt_trigger;
	} trigger_type;
} wlc_ulmu_trigger_info_t;

typedef struct trigger_ctx_entry trigger_ctx_entry_t;

extern int wlc_ulmu_drv_trigger_request(wlc_info_t *wlc, struct scb *scb,
	int trigger_type, wlc_ulmu_trigger_info_t *trigger_info);

#ifdef BCMDBG
/* Return current active UTXD request pointer */
extern trigger_ctx_entry_t *wlc_ulmu_active_utxd_request(
	wlc_info_t *wlc, struct scb *scb);

/* Return current pending UTXD request pointer */
extern trigger_ctx_entry_t *wlc_ulmu_pending_utxd_request(
	wlc_info_t *wlc, struct scb *scb);

/* Return next pending UTXD request pointer */
extern trigger_ctx_entry_t *wlc_ulmu_next_utxd_request(
	trigger_ctx_entry_t *utxd_request);

/* Returns trigger setup info from the UTXD request */
extern packet_trigger_info_t *wlc_ulmu_trigger_info(
	trigger_ctx_entry_t *utxd_request);
#endif /* BCMDBG */

/* UL OFDMA */
extern bool wlc_ulmu_admit_client(wlc_info_t *wlc, scb_t *scb, bool admit);

extern void wlc_ulmu_ul_rspec_upd(wlc_ulmu_info_t *ulmu, scb_t* scb);
extern void wlc_ulmu_ul_nss_upd(wlc_ulmu_info_t *ulmu, scb_t* scb, uint8 tx_nss);

extern int wlc_ulmu_stats_upd(wlc_ulmu_info_t *ulmu, struct scb *scb,
	tx_status_t *txs);
#if defined(BCMDBG) || defined(UL_RU_STATS_DUMP)
extern int wlc_ulmu_rustats_upd(wlc_ulmu_info_t *ulmu, struct scb *scb,
	tx_status_t *txs);
#endif // endif
extern void wlc_ulmu_fburst_set(wlc_ulmu_info_t *ulmu, bool enable);
extern void wlc_ulmu_max_trig_usrs_set(wlc_ulmu_info_t *ulmu, uint16 val);
extern void wlc_ulmu_maxn_uplimit_set(wlc_ulmu_info_t *ulmu, uint16 val);
#if defined(WL11AX) && defined(WL_PSMX)
extern void wlc_ulmu_chanspec_upd(wlc_info_t *wlc);
extern void wlc_ulmu_oper_state_upd(wlc_ulmu_info_t* ulmu, scb_t *scb, uint8 state);
#else
#define wlc_ulmu_chanspec_upd(a) do {} while (0)
#define wlc_ulmu_oper_state_upd(a, b, c) do {} while (0)
#endif /* defined(WL11AX) && defined(WL_PSMX) */
extern int wlc_ulmu_reclaim_utxd(wlc_info_t *wlc, tx_status_t *txs);
extern bool wlc_ulmu_del_usr(wlc_ulmu_info_t *ulmu, scb_t *scb, bool is_bss_up);
extern void wlc_ulmu_scb_reqbytes_decr(wlc_ulmu_info_t *ulmu, scb_t *scb, uint32 cnt);
extern int wlc_ulmu_sw_trig_enable(wlc_info_t *wlc, uint32 enable);
extern void wlc_ulmu_twt_params(wlc_ulmu_info_t *ulmu, bool on);
extern int wlc_ulmu_post_utxd(wlc_ulmu_info_t *ulmu);
extern bool wlc_ulmu_is_ulrt_fixed(wlc_ulmu_info_t *ulmu);
#ifdef WL_ULRT_DRVR
extern bool wlc_ulmu_is_drv_rtctl_en(wlc_ulmu_info_t *ulmu);
extern bool wlc_ulmu_is_drv_spctl_en(wlc_ulmu_info_t *ulmu);
#endif // endif
#ifdef WLTAF_ULMU
#include <wlc_taf.h>
extern void * BCMFASTPATH wlc_ulmu_taf_get_scb_info(void *ulmuh, struct scb* scb);
extern void * BCMFASTPATH wlc_ulmu_taf_get_scb_tid_info(void *scb_h, int tid);
extern uint16 BCMFASTPATH wlc_ulmu_taf_get_pktlen(void *scbh, void *tidh);
extern bool wlc_ulmu_taf_release(void* ulmuh, void* scbh, void* tidh, bool force,
	taf_scheduler_public_t* taf);
extern bool wlc_ulmu_taf_bulk(void* ulmuh, int tid, bool open);
#endif // endif
extern bool wlc_ulmu_admit_ready(wlc_info_t *wlc, struct scb *scb);
extern void wlc_ulmu_alloc_fifo(wlc_info_t *wlc, struct scb *sta_scb);
#endif /* _wlc_ulmu_h_ */
