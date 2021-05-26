/*
 * TXMOD (tx stack) manipulation module interface.
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
 * $Id: wlc_txmod.h 782351 2019-12-17 17:00:04Z $
 */

#ifndef _wlc_txmod_h_
#define _wlc_txmod_h_

#include <typedefs.h>
#include <wlc_types.h>

/* txmod attach/detach */

wlc_txmod_info_t *wlc_txmod_attach(wlc_info_t *wlc);
void wlc_txmod_detach(wlc_txmod_info_t *txmodi);

/* txmod registration */

/* Allowed txpath features */
typedef enum txmod_id {
	TXMOD_START = 0,	/* Node to designate start of the tx path */
	TXMOD_TRANSMIT = 1,	/* Node to designate enqueue to common tx queue */
	TXMOD_TDLS = 2,
	TXMOD_APPS = 3,
	TXMOD_NAR = 4,
	TXMOD_AMSDU = 5,
	TXMOD_AMPDU = 6,
/* !!!Add new txmod ID above and update array 'txmod_pos' with the position of the txmod!!! */
	TXMOD_LAST
} txmod_id_t;

/* Function that does transmit packet feature processing. prec is precedence of the packet */
typedef void (*txmod_tx_fn_t)(void *ctx, struct scb *scb, void *pkt, uint prec);

/* Callback for txmod when it gets deactivated by other txmod */
typedef void (*txmod_deactivate_fn_t)(void *ctx, struct scb *scb);

/* Callback for txmod when it gets activated */
typedef void (*txmod_activate_fn_t)(void *ctx, struct scb *scb);

/* Callback for txmod to return packets held by this txmod */
typedef uint (*txmod_pktcnt_fn_t)(void *ctx);

/* Callback for txmod to flush all packets held for an scb/prec by this txmod */
typedef void (*txmod_pkt_flush_fn_t)(void *ctx, struct scb *scb, uint8 tid);

/* Function vector to make it easy to initialize txmod
 * Note: txmod_info itself is not modified to avoid adding one more level of indirection
 * during transmit of a packet
 */
typedef struct txmod_fns {
	txmod_tx_fn_t		tx_fn;			/* Process the packet */
	txmod_pktcnt_fn_t	pktcnt_fn;		/* Return the packet count */
	txmod_pkt_flush_fn_t	pktflush_fn;		/* Flush packets for scb/prec */
	txmod_deactivate_fn_t	deactivate_notify_fn;	/* Handle the deactivation of the feature */
	txmod_activate_fn_t	activate_notify_fn;	/* Handle the activation of the feature */
} txmod_fns_t;

/* Register a txmod handlers with txmod module */
void wlc_txmod_fn_register(wlc_txmod_info_t *txmodi, txmod_id_t fid, void *ctx, txmod_fns_t fns);

/* Insert/remove a txmod at/from the fixed loction in the scb txpath */
void wlc_txmod_config(wlc_txmod_info_t *txmodi, struct scb *scb, txmod_id_t fid);
void wlc_txmod_unconfig(wlc_txmod_info_t *txmodi, struct scb *scb, txmod_id_t fid);

/* fast txpath access in scb */

/* per scb txpath node */
struct tx_path_node {
	txmod_tx_fn_t next_tx_fn;	/* Next function to be executed */
	void *next_handle;
	uint8 next_fid;			/* Next fid in transmit path */
	bool configured;		/* Whether this feature is configured */
};

/* Given the 'feature', invoke the next stage of transmission in tx path */
#define SCB_TX_NEXT(fid, scb, pkt, prec) \
	scb->tx_path[fid].next_tx_fn(scb->tx_path[fid].next_handle, scb, pkt, prec)

/* utilities */
uint wlc_txmod_txpktcnt(wlc_txmod_info_t *txmodi);
void wlc_txmod_flush_pkts(wlc_txmod_info_t *txmodi, struct scb *scb, uint8 txmod_tid);

/* KEEP THE FOLLOWING FOR NOW. THE REFERENCE TO THESE SHOULD BE ELIMINATED FIRST
 * AND THEN THESE SHOULD BE MOVED BACK TO THE .c FILE SINCE NO ONE SHOULD PEEK INTO
 * THE TXMOD ORDER I.E. ALL A TXMOD SHOULD IS TO PASS THE PACKET TO THE NEXT TXMOD.
 */
/* Is the feature currently in the path to handle transmit. ACTIVE implies CONFIGURED */
#define SCB_TXMOD_ACTIVE(scb, fid) (scb->tx_path[fid].next_tx_fn != NULL)
/* Next feature configured */
#define SCB_TXMOD_NEXT_FID(scb, fid) (scb->tx_path[fid].next_fid)
/* Is the feature configured? */
#define SCB_TXMOD_CONFIGURED(scb, fid) (scb->tx_path[fid].configured)

#define TXMOD_TID_FLUSH_ALL_TID		0x80
#define TXMOD_TID_FLUSH_ALL_SCB		0x40
#define TXMOD_TID_FLUSH_SUPPRESS	0x20
#define TXMOD_TID_MASK			0x0F

#define TXMOD_FLUSH_ALL_TIDS(tid)	(tid & TXMOD_TID_FLUSH_ALL_TID)
#define TXMOD_FLUSH_ALL_SCBS(tid)	(tid & TXMOD_TID_FLUSH_ALL_SCB)
#define TXMOD_FLUSH_SUPPRESS(tid)	(tid & TXMOD_TID_FLUSH_SUPPRESS)
#define TXMOD_TID_GET(tid)		(tid & TXMOD_TID_MASK)
#endif /* _wlc_txmod_h_ */
