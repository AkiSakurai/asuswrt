/*
 * Propagate txstatus (also flow control) interface.
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
 * $Id: wlc_wlfc.h 774418 2019-04-24 07:52:25Z $
 */

#ifndef __wlc_wlfc_h__
#define __wlc_wlfc_h__

#include <typedefs.h>
#include <wlc_types.h>
#include <wlfc_proto.h>

/* from wl_export.h */
void wlfc_process_txstatus(wlc_wlfc_info_t *wlfc, uint8 status, void *pkt,
	void *txs, bool hold);
int wlfc_push_credit_data(wlc_wlfc_info_t *wlfc, void *pkt);
int wlfc_push_signal_data(wlc_wlfc_info_t *wflc, uint8 *data, uint8 len, bool hold);
int wlfc_sendup_ctl_info_now(wlc_wlfc_info_t *wlfc);
uint32 wlfc_query_mode(wlc_wlfc_info_t *wlfc);
void wlfc_enable_cred_borrow(wlc_info_t *wlc, uint32 bEnable);
/* called from wl_rte.c */
void wlc_send_credit_map(wlc_info_t *wlc);
int wlc_send_txstatus(wlc_info_t *wlc, void *pkt);
int wlc_sendup_txstatus(wlc_info_t *wlc, void **pkt);
#ifdef PROP_TXSTATUS_DEBUG
void wlc_wlfc_info_dump(wlc_info_t *wlc);
#endif // endif

/* new interfaces */
void wlc_wlfc_psmode_request(wlc_wlfc_info_t *wlfc, struct scb *scb,
	uint8 credit, uint8 ac_bitmap, wlfc_ctl_type_t request_type);
int wlc_wlfc_mac_status_upd(wlc_wlfc_info_t *wlfc, struct scb *scb,
	uint8 open_close);
bool wlc_wlfc_suppr_status_query(wlc_info_t *wlc, struct scb *scb);

/* from wlc_p2p.h */
int wlc_wlfc_interface_state_update(wlc_info_t *wlc, wlc_bsscfg_t *cfg, wlfc_ctl_type_t state);
void wlc_wlfc_flush_pkts_to_host(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
void wlc_wlfc_flush_queue(wlc_info_t *wlc, struct pktq *q);

/* from wlc.h */
void wlc_txfifo_suppress(wlc_info_t *wlc, struct scb *scb);
void wlc_process_wlhdr_txstatus(wlc_info_t *wlc, uint8 status, void *pkt, bool hold);
bool wlc_suppress_sync_fsm(wlc_info_t *wlc, struct scb *scb, void *pkt, bool suppress);
uint8 wlc_txstatus_interpret(tx_status_macinfo_t *txstatus,  int8 acked);
int16 wlc_link_txflow_scb(wlc_info_t *wlc, struct wlc_if *wlcif, uint16 flowid, uint8 op,
	uint8 * sa, uint8 *da, uint8 tid);
#ifdef WLCFP
int wlc_bus_cfp_link_update(wlc_info_t *wlc, struct wlc_if *wlcif,
	uint16 ringid, uint8 tid, uint8* da, uint8 op,
	uint8** tcb_state, uint16* cfp_flowid);
#endif // endif
#ifdef WLSQS
void wlc_wlfc_sqs_stride_resume(wlc_info_t *wlc, struct scb *scb, uint8 prio);
#endif // endif
/* new for wlc_apps.c */
void wlc_wlfc_scb_ps_on(wlc_info_t *wlc, struct scb *scb);
void wlc_wlfc_scb_ps_off(wlc_info_t *wlc, struct scb *scb);
void wlc_wlfc_scb_psq_resp(wlc_info_t *wlc, struct scb *scb);
bool wlc_wlfc_scb_psq_chk(wlc_info_t *wlc, struct scb *scb, uint16 fc, struct pktq *pktq);
uint wlc_wlfc_scb_ps_count(wlc_info_t *wlc, struct scb *scb, bool bss_ps,
	struct pktq *pktq, bool force_request);

/* from wlc_mchan.h */
int wlc_wlfc_mchan_interface_state_update(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	wlfc_ctl_type_t open_close, bool force_open);

void wlc_wlfc_dotxstatus(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct scb *scb,
	void *pkt, tx_status_t *txs, bool pps_retry);

/* attach/detach */
wlc_wlfc_info_t *wlc_wlfc_attach(wlc_info_t *wlc);
void wlc_wlfc_detach(wlc_wlfc_info_t *wlfc);

#ifdef WLATF_DONGLE
/* Max number of samples in running buffer:
 * Keep max samples number a power of two, thus
 * all the division would be a single-cycle
 * bit-shifting.
 */
#define RAVG_EXP_PKT 2
#define RAVG_EXP_WGT 2

typedef struct _ravg {
	uint32 sum;
	uint8 exp;
} wlc_ravg_info_t;

#define RAVG_EXP(_obj_) ((_obj_)->exp)
#define RAVG_SUM(_obj_) ((_obj_)->sum)
#define RAVG_AVG(_obj_) ((_obj_)->sum >> (_obj_)->exp)

/* Basic running average algorithm:
 * Keep a running buffer of the last N values, and a running SUM of all the
 * values in the buffer. Each time a new sample comes in, subtract the oldest
 * value in the buffer from SUM, replace it with the new sample, add the new
 * sample to SUM, and output SUM/N.
 */
#define RAVG_ADD(_obj_, _sample_) \
{ \
	uint8 _exp_ = RAVG_EXP((_obj_)); \
	uint32 _sum_ = RAVG_SUM((_obj_)); \
	RAVG_SUM((_obj_)) = ((_sum_ << _exp_) - _sum_) >> _exp_; \
	RAVG_SUM((_obj_)) += (_sample_); \
}

/* Initializing running buffer with value (_sample_) */
#define RAVG_INIT(_obj_, _sample_, _exp_) \
{ \
	RAVG_SUM((_obj_)) = (_sample_) << (_exp_); \
	RAVG_EXP((_obj_)) = (_exp_); \
}
#define PRIOMAP(_wlc_) ((_wlc_)->pciedev_prio_map)

#if defined(FLOW_PRIO_MAP_AC)
/* XXX: FLOW_PRIO_MAP_AC would be defined for PCIE FD router builds.
 * XXX: Router supports only 4 flow-rings per STA, therefore average
 * XXX: buffers would be 4.
 */
#define FLOWRING_PER_SCB_MAX AC_COUNT
#define RAVG_PRIO2FLR(_map_, _prio_) WME_PRIO2AC((_prio_))
#else  /* !FLOW_PRIO_MAP_AC */
/* XXX: Supports maximum up to 8 flow-rings, therefore average
 * XXX: buffers would be maximum 8.
 */
#define FLOWRING_PER_SCB_MAX NUMPRIO
/* XXX: Following the currently used prio mapping
 * XXX: configuration scheme in pcie bus layer.
 */
#define RAVG_PRIO2FLR(_map_, _prio_) \
	((((_map_) == PCIEDEV_AC_PRIO_MAP) ? WME_PRIO2AC((_prio_)) : (_prio_)))
#endif  /* FLOW_PRIO_MAP_AC */

#define TXPKTLEN_RAVG(_atfd_, _ac_) (&(_atfd_)->flr_txpktlen_ravg[(_ac_)])
#define WEIGHT_RAVG(_atfd_, _ac_) (&(_atfd_)->flr_weigth_ravg[(_ac_)])

typedef struct wlc_atfd {
		/* Running average info for tx packet len per flow ring */
		wlc_ravg_info_t flr_txpktlen_ravg[FLOWRING_PER_SCB_MAX];
		/* Running average info for weight per flow ring */
		wlc_ravg_info_t flr_weigth_ravg[FLOWRING_PER_SCB_MAX];
} wlc_atfd_t;

extern wlc_atfd_t *wlfc_get_atfd(wlc_info_t *wlc, struct scb *scb);
extern void wlc_scb_upd_all_flr_weight(wlc_info_t *wlc, struct scb *scb);
extern void wlc_ravg_add_weight(wlc_atfd_t *atfd, int fl, ratespec_t rspec);
#endif /* WLATF_DONGLE */

#if defined(BCMPCIEDEV) && defined(BCMLFRAG) && defined(PROP_TXSTATUS)
extern uint32 wlc_suppress_recent_for_fragmentation(wlc_info_t *wlc, void *sdu, uint nfrags);
#endif // endif

#endif /* __wlc_wlfc_h__ */
