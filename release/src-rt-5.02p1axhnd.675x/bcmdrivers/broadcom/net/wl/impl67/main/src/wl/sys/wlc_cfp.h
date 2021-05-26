/*
 * Cache based flow processing
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id$
 */

#ifndef _WLC_CFP_H_
#define _WLC_CFP_H_

#if !defined(DONGLEBUILD)
#include <linuxver.h>
#endif /* !defined(DONGLEBUILD) */

#if defined(BCM_AWL)
#include <wl_awl.h>
#endif /* BCM_AWL */

#if defined(BCM_PKTFWD)

/* AWL platforms doesn't support D3LUT */
#if !defined(BCM_AWL) && !defined(WL_AWL_RX)
/* ucode doesn't provide "flowid" in RxStatus for corerev < 80.
 * Fetching CFP contexts using D3LUT lookup for TA in D11
 */
#define CFP_REVLT80_UCODE_WAR
#endif /* ! BCM_AWL && ! WL_AWL_RX */

#endif /* BCM_PKTFWD */

/** Not applicable OSH for PKT macros */
#ifndef PKT_OSH_NA
#define PKT_OSH_NA                      (NULL)
#endif // endif

#define WLC_CFP_NOOP                    do { /* noop */ } while (0)

/** debug prints */
#define WLC_CFP_ERROR(args)             printf args
#ifdef CFP_DEBUG
#define WLC_CFP_DEBUG(args)             printf args
#else
#define WLC_CFP_DEBUG(args)             WLC_CFP_NOOP
#endif // endif

/* data APIs */

/** CFP public module states */
struct wlc_cfp_info {
	bool _tcb;	/**< runtime enable check for Transmit Control Block */
	bool _rcb;	/**< runtime enable check for Recieve Control Block */

	dll_t	rcb_pend;   /* pending list of RCBs to be processed */
	dll_t	rcb_done;   /* RX processing done list of RCBS */
};

/** MACROs return the enab value for Transmit and Receive Control Blocks */
#define CFP_TCB_ENAB(cfp) ((cfp)->_tcb)
#define CFP_RCB_ENAB(cfp) ((cfp)->_rcb)

/** Pending RCB list */
#define CFP_RCB_PEND_LIST(cfp)		(&((cfp)->rcb_pend))
#define CFP_RCB_DONE_LIST(cfp)		(&((cfp)->rcb_done))

extern wlc_cfp_info_t* wlc_cfp_attach(wlc_info_t *wlc);
extern void wlc_cfp_detach(wlc_cfp_info_t *cfp_info);

/** SCB CFP state update changes */
extern void wlc_cfp_state_update(wlc_cfp_info_t * cfp_info);
extern void wlc_cfp_scb_state_upd(wlc_cfp_info_t* cfp_info, struct scb *scb);

/** Set TCB/RCB states */

/* Set TCB states */
extern void wlc_cfp_tcb_upd_cache_state(wlc_info_t *wlc, struct scb *scb, bool valid);
extern void wlc_cfp_tcb_upd_ini_state(wlc_info_t *wlc, struct scb *scb, int prio, bool valid);
extern int wlc_cfp_tcb_upd_pause_state(wlc_info_t *wlc, struct scb *scb, bool valid);
extern int wlc_cfp_tcb_cache_invalidate(wlc_info_t *wlc, int16 flowid);
extern bool wlc_cfp_tcb_is_EST(wlc_info_t *wlc, struct scb *scb, uint8 prio, scb_cfp_t **scb_cfp);
extern bool wlc_cfp_tcb_is_ini_valid(wlc_info_t *wlc, struct scb *scb, uint8 prio,
	scb_cfp_t **scb_cfp);

/** Link cfp with bus layer flow */
extern int wlc_scb_cfp_tcb_link(wlc_info_t *wlc, struct scb *scb, uint8** tcb_state);

/* update legacy TX counters */
extern void wlc_cfp_incr_legacy_tx_cnt(wlc_info_t *wlc, struct scb *scb, uint8 tid);

/* Update RCB state */
extern void wlc_cfp_rcb_upd_responder_state(wlc_info_t *wlc, struct scb *scb, int prio, bool valid);
extern int wlc_cfp_rcb_upd_pause_state(wlc_info_t *wlc, struct scb *scb, bool valid);
extern bool wlc_cfp_rcb_is_EST(wlc_info_t *wlc, struct scb *scb, uint8 prio, scb_cfp_t **scb_cfp);

/** Miscellaneous Wireless CFP helper functions for ingress device|bus layer. */

/** Check if CFP enabled for given ID. Fetch wireless tx packet exptime. */
#if defined(DONGLEBUILD)
extern bool wlc_cfp_tx_enabled(int cfp_unit, uint16 cfp_flowid,
	uint32* cfp_exptime);
#else  /* ! DONGLEBUILD */
extern uint32 wlc_cfp_exptime(wlc_info_t *wlc);
extern bool wlc_cfp_tx_enabled(int cfp_unit, uint16 cfp_flowid, uint32 prio);
extern void wlc_cfp_pktlist_free(wlc_info_t *wlc, void *p);
#endif /* ! DONGLEBUILD */

/** Prepare a CFP capable packet. */
extern void wlc_cfp_pkt_prepare(int cfp_unit, uint16 cfp_flowid, uint8 prio,
	void *pkt, uint32 cfp_exptime); /* Settup pkttag, OSH to/frm native, ... */
#if !defined(DONGLEBUILD)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0))
/** Prepare pkttag and wlanext for CFP capable packet. */
extern void wlc_cfp_pkttag_ext_prepare(wlc_info_t * wlc, uint16 flowid, uint8 prio,
	wlc_pkttag_t * pkttag, struct wlan_ext * wlanext);
#endif /* #if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,19,0)) */
#endif /* !defined(DONGLEBUILD) */

/** Wireless CFP capable transmit fastpath entry point */
extern void wlc_cfp_tx_sendup(int cfp_unit, uint16 cfp_flowid, uint8 prio,
	void *pktlist_head, void *pktlist_tail, uint16 pkt_count);

extern int32 wlc_cfp_cq_fifo_cnt(wlc_info_t *wlc, struct scb *scb, uint8 prio);
extern void wlc_cfp_cq_fifo_inc(wlc_info_t *wlc, struct scb *scb, uint8 prio, uint32 cnt);
extern void wlc_cfp_cq_fifo_dec(wlc_info_t *wlc, struct scb *scb, uint8 prio, uint32 cnt);
extern void wlc_cfp_cq_fifo_upd(wlc_info_t *wlc, struct scb *scb, uint8 ac);

/** CFP RX  processing */
extern bool wlc_cfp_bmac_recv(wlc_hw_info_t *wlc_hw, uint fifo, wlc_worklet_info_t *worklet);

/** CFP RX Exported Functions */
/* Handle a given packet if chainable, put on chain, otherwise process list and recv packet */
void wlc_cfp_rxframe(wlc_info_t *wlc, void* p);
/* Sendup Chained CFP packets, and unchained packet p if not NULL. */
extern void wlc_cfp_rx_sendup(wlc_info_t *wlc, void* p);

#if defined(DONGLEBUILD)
extern int wlc_cfp_scb_rx_queue_count(wlc_info_t* wlc, struct scb *scb);
extern void wlc_cfp_rx_hwa_pkt_audit(wlc_info_t* wlc, void* p);
#endif /* DONGLEBUILD */

/* Exported functions to SQS */
extern scb_cfp_t *wlc_scb_cfp_id2ptr(wlc_info_t* wlc, uint16 cfp_flowid);

extern void* wlc_scb_cfp_cubby(wlc_info_t *wlc, struct scb *scb);

#endif  /* _WLC_CFP_H_ */
