/*
 * TXMOD (tx stack) manipulation module.
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
 * $Id: wlc_txmod.c 783348 2020-01-24 10:25:17Z $
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wlc_txmod.h>
#include <wlc_dump.h>

/* structure to store registered functions for a txmod */
typedef struct txmod_info {
	txmod_fns_t fns;
	void *ctx;		/* Opaque handle to be passed */
} txmod_info_t;

/* wlc txmod module states */
struct wlc_txmod_info {
	wlc_info_t *wlc;
	int scbh;
	txmod_id_t txmod_last;	/* txmod registry capacity */
	txmod_info_t *txmod;	/* txmod registry (allocated along with this structure) */
};

/* access to scb cubby */
#define SCB_TXPATH_INFO(txmodi, scb) (tx_path_node_t *)SCB_CUBBY(scb, (txmodi)->scbh)

/* local declaration */
#if defined(BCMDBG)
static int wlc_txmod_dump(void *ctx, struct bcmstrbuf *b);
#endif // endif

static int wlc_txmod_scb_init(void *ctx, struct scb *scb);
/* Dump the active txpath for the current SCB */
#if defined(BCMDBG)
static void wlc_txmod_scb_dump(void *ctx, struct scb *scb, struct bcmstrbuf *b);
#endif // endif

static const uint8 *wlc_txmod_get_pos(void);

/* sub-alloc sizes, only reference TXMOD_LAST at attach time */
#define TXMOD_REGSZ	TXMOD_LAST * sizeof(txmod_info_t)
#define TXPATH_REGSZ	TXMOD_LAST * sizeof(tx_path_node_t)

/* attach/detach functions */
wlc_txmod_info_t *
BCMATTACHFN(wlc_txmod_attach)(wlc_info_t *wlc)
{
	wlc_txmod_info_t *txmodi;
	scb_cubby_params_t txmod_scb_cubby_params;

	/* allocate state info plus the txmod registry */
	if ((txmodi = MALLOCZ(wlc->osh, sizeof(*txmodi) + TXMOD_REGSZ)) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}

	txmodi->wlc = wlc;
	/* reserve cubby in the scb container for per-scb private data */
	bzero(&txmod_scb_cubby_params, sizeof(txmod_scb_cubby_params));
	txmod_scb_cubby_params.context = txmodi;
	txmod_scb_cubby_params.fn_init = wlc_txmod_scb_init;
#if defined(BCMDBG)
	txmod_scb_cubby_params.fn_dump = wlc_txmod_scb_dump;
#endif // endif
	txmod_scb_cubby_params.fn_deinit = NULL;

	/* reserve the scb cubby for per scb txpath */
	txmodi->scbh = wlc_scb_cubby_reserve_ext(wlc, TXPATH_REGSZ,
			&txmod_scb_cubby_params);
	if (txmodi->scbh < 0) {
		WL_ERROR(("wl%d: %s: wlc_scb_cubby_reserve failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* txmod registry */
	txmodi->txmod = (txmod_info_t *)&txmodi[1];
	txmodi->txmod_last = TXMOD_LAST;

#if defined(BCMDBG)
	/* debug dump */
	wlc_dump_register(wlc->pub, "txmod", wlc_txmod_dump, txmodi);
#endif // endif

	return txmodi;

fail:
	MODULE_DETACH(txmodi, wlc_txmod_detach);
	return NULL;
}

void
BCMATTACHFN(wlc_txmod_detach)(wlc_txmod_info_t *txmodi)
{
	wlc_info_t *wlc;

	if (txmodi == NULL)
		return;

	wlc = txmodi->wlc;
	BCM_REFERENCE(wlc);

	MFREE(wlc->osh, txmodi, sizeof(*txmodi) + TXMOD_REGSZ);
}

/* per scb txpath */
static int
wlc_txmod_scb_init(void *ctx, struct scb *scb)
{
	wlc_txmod_info_t *txmodi = (wlc_txmod_info_t *)ctx;
	tx_path_node_t *txpath = SCB_TXPATH_INFO(txmodi, scb);

	/* for faster txpath access */
	scb->tx_path = txpath;
	return BCME_OK;
}

/** return number of packets txmodule has currently; 0 if no pktcnt function */
static uint
wlc_txmod_get_pkts_pending(wlc_txmod_info_t *txmodi, txmod_id_t fid)
{
	if (txmodi->txmod[fid].fns.pktcnt_fn) {
		return txmodi->txmod[fid].fns.pktcnt_fn(txmodi->txmod[fid].ctx);
	}

	return 0;
}

/** Return total transmit packets held by the txmods */
uint
wlc_txmod_txpktcnt(wlc_txmod_info_t *txmodi)
{
	uint pktcnt = 0;
	int i;

	/* Call any pktcnt handlers of registered modules only */
	for (i = TXMOD_START; i < (int)txmodi->txmod_last; i++) {
		pktcnt += wlc_txmod_get_pkts_pending(txmodi, i);
	}

	return pktcnt;
}

/** flush packets for scb/tid in all txmodules */
void
wlc_txmod_flush_pkts(wlc_txmod_info_t *txmodi, struct scb *scb, uint8 txmod_tid)
{
	int fid;
	uint8 tid;
	txmod_info_t mod_info;

	/* Call any flush handlers of registered modules only */
	for (fid = TXMOD_START; fid < (int)txmodi->txmod_last; fid++) {
		mod_info = txmodi->txmod[fid];
		if (mod_info.fns.pktflush_fn) {
			if (TXMOD_FLUSH_ALL_TIDS(txmod_tid)) {
				for (tid = 0; tid < NUMPRIO; tid++) {
					mod_info.fns.pktflush_fn(mod_info.ctx, scb, tid);
				}
			} else {
				tid = TXMOD_TID_GET(txmod_tid);
				mod_info.fns.pktflush_fn(mod_info.ctx, scb, tid);
			}
		}
	}

	return;
}

/* Dump the active txpath for the current SCB */
#if defined(BCMDBG)
static const char *txmod_names[TXMOD_LAST] = {
	"Start",
	"Transmit",
	"TDLS",
	"APPS",
	"NAR",
	"A-MSDU",
	"A-MPDU",
};

/* debug dump */
static int
wlc_txmod_dump(void *ctx, struct bcmstrbuf *b)
{
	wlc_txmod_info_t *txmodi = ctx;
	txmod_id_t txmod;
	const uint8 *txmod_position = wlc_txmod_get_pos();

	for (txmod = TXMOD_START; txmod < txmodi->txmod_last; txmod++) {
		bcm_bprintf(b, "fid %d:\tname %s\t\tpos %d\t"
		           "tx %p cnt %p deact %p act %p ctx %p\n",
		            txmod, txmod_names[txmod], txmod_position[txmod],
		            txmodi->txmod[txmod].fns.tx_fn,
		            txmodi->txmod[txmod].fns.pktcnt_fn,
		            txmodi->txmod[txmod].fns.deactivate_notify_fn,
		            txmodi->txmod[txmod].fns.activate_notify_fn,
		            txmodi->txmod[txmod].ctx);
	}

	return BCME_OK;
}

static void
wlc_txmod_scb_dump(void *ctx, struct scb *scb, struct bcmstrbuf *b)
{
	txmod_id_t next_fid;

	bcm_bprintf(b, "     Tx Path: ");
	for (next_fid = TXMOD_START;
	     next_fid != TXMOD_TRANSMIT;
	     next_fid = scb->tx_path[next_fid].next_fid) {
		bcm_bprintf(b, " %s ->", txmod_names[next_fid]);
	}
	bcm_bprintf(b, " %s\n", txmod_names[next_fid]);
}
#endif // endif

/* Helper macro for txpath in scb */
/* A feature in Tx path goes through following states:
 * Unregisterd -> Registered [Global state]
 * Registerd -> Configured -> Active -> Configured [Per-scb state]
 */

/* Set the next feature of given feature */
#define SCB_TXMOD_SET(scb, fid, _next_fid) { \
	scb->tx_path[fid].next_tx_fn = txmodi->txmod[_next_fid].fns.tx_fn; \
	scb->tx_path[fid].next_handle = txmodi->txmod[_next_fid].ctx; \
	scb->tx_path[fid].next_fid = _next_fid; \
}

/* Numeric value designating this feature's position in tx_path */
static const uint8 txmod_pos[TXMOD_LAST] = {
	0,	/* TXMOD_START */		/* First */
	6,	/* TXMOD_TRANSMIT */		/* Last */
	1,	/* TXMOD_TDLS */
	5,	/* TXMOD_APPS */
	2,	/* TXMOD_NAR */
	3,	/* TXMOD_AMSDU */
	4,	/* TXMOD_AMPDU */
};

static const uint8 *
wlc_txmod_get_pos(void)
{
	return txmod_pos;
}

/**
 * Add a feature to the path. It should not be already on the path and should be configured
 * Does not take care of evicting anybody
 */
static void
wlc_txmod_activate(wlc_txmod_info_t *txmodi, struct scb *scb, txmod_id_t fid)
{
	const uint8 *txmod_position = wlc_txmod_get_pos();
	uint curr_mod_position;
	txmod_id_t prev, next;
	txmod_info_t curr_mod_info = txmodi->txmod[fid];

	ASSERT(SCB_TXMOD_CONFIGURED(scb, fid));
	ASSERT(!SCB_TXMOD_ACTIVE(scb, fid));

	curr_mod_position = txmod_position[fid];

	prev = TXMOD_START;

	while ((next = scb->tx_path[prev].next_fid) != 0 &&
	       txmod_position[next] < curr_mod_position)
		prev = next;

	/* next == TXMOD_START indicate this is the first addition to the path
	 * it must be TXMOD_TRANSMIT as it's the one that puts the packet in
	 * txq. If this changes, then assert will need to be removed.
	 */
	ASSERT(next != TXMOD_START || fid == TXMOD_TRANSMIT);
	ASSERT(txmod_position[next] != curr_mod_position);

	SCB_TXMOD_SET(scb, prev, fid);
	SCB_TXMOD_SET(scb, fid, next);

	/* invoke any activate notify functions now that it's in the path */
	if (curr_mod_info.fns.activate_notify_fn)
		curr_mod_info.fns.activate_notify_fn(curr_mod_info.ctx, scb);
}

/**
 * Remove a fid from the path. It should be already on the path
 * Does not take care of replacing it with any other feature.
 */
static void
wlc_txmod_deactivate(wlc_txmod_info_t *txmodi, struct scb *scb, txmod_id_t fid)
{
	txmod_id_t prev, next;
	txmod_info_t curr_mod_info = txmodi->txmod[fid];

	/* If not active, do nothing */
	if (!SCB_TXMOD_ACTIVE(scb, fid))
		return;

	/* if deactivate notify function is present, call it */
	if (curr_mod_info.fns.deactivate_notify_fn)
		curr_mod_info.fns.deactivate_notify_fn(curr_mod_info.ctx, scb);

	prev = TXMOD_START;

	while ((next = scb->tx_path[prev].next_fid) != fid)
		prev = next;

	SCB_TXMOD_SET(scb, prev, scb->tx_path[fid].next_fid);
	scb->tx_path[fid].next_tx_fn = NULL;
}

/* Register the function to handle this feature */
void
BCMATTACHFN(wlc_txmod_fn_register)(wlc_txmod_info_t *txmodi, txmod_id_t feature_id,
	void *ctx, txmod_fns_t fns)
{
	ASSERT(feature_id < txmodi->txmod_last);
	/* tx_fn can't be NULL */
	ASSERT(fns.tx_fn != NULL);

	txmodi->txmod[feature_id].fns = fns;
	txmodi->txmod[feature_id].ctx = ctx;
}

/* Add the fid to handle packets for this SCB, if allowed */
void
wlc_txmod_config(wlc_txmod_info_t *txmodi, struct scb *scb, txmod_id_t fid)
{
	ASSERT(fid < txmodi->txmod_last);

	/* Don't do anything if not yet registered or
	 * already configured
	 */
	if ((txmodi->txmod[fid].fns.tx_fn == NULL) ||
	    (SCB_TXMOD_CONFIGURED(scb, fid)))
		return;

	/* Indicate that the feature is configured */
	scb->tx_path[fid].configured = TRUE;

	ASSERT(!SCB_TXMOD_ACTIVE(scb, fid));

	wlc_txmod_activate(txmodi, scb, fid);
}

/* Remove the feature to handle packets for this SCB.
 * If just configured but not in path, just marked unconfigured
 * If in path, feature is removed and, if applicable, replaced by any other feature
 */
void
wlc_txmod_unconfig(wlc_txmod_info_t *txmodi, struct scb *scb, txmod_id_t fid)
{
	ASSERT(fid < txmodi->txmod_last);
	ASSERT(fid != TXMOD_TRANSMIT);

	if (!SCB_TXMOD_CONFIGURED(scb, fid))
		return;

	scb->tx_path[fid].configured = FALSE;

	/* Nothing to do if not active */
	if (!SCB_TXMOD_ACTIVE(scb, fid))
		return;

	wlc_txmod_deactivate(txmodi, scb, fid);
}
