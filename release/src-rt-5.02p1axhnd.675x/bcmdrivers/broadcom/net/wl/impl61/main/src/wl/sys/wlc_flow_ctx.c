/*
 * wlc_flow_ctx.c
 *
 * Common TX/RX Flow Context Table management module
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
 * $Id$
 *
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <bcmutils.h>
#include <osl.h>
#include <wlioctl.h>
#include <wlc_pub.h>
#include <wlc.h>

#include "wlc_flow_ctx.h"

#define MAX_FLOWS_NONAP	8		/* for both TX and RX */
#define MAX_FLOWS_AP	MAXSCB

/* private module info */
struct flow_ctx_info {
	wlc_info_t *wlc;	/* Handle to wlc context */
	wlc_pub_t *pub;		/* Handle to public context */
	osl_t *osh;		/* OSL handle */
	uint8 num_entries;	/* the total number of entries in the table */
	flow_ctx_t *flow_tbl;	/* Pointer to Flow Context Table */
};

#if defined(BCMDBG)
#include <wlc_dump.h>

static int
wlc_flow_ctx_module_dump(void *ctx, struct bcmstrbuf *b)
{
	wlc_flow_ctx_info_t *flow_tbli = (wlc_flow_ctx_info_t *)ctx;
	flow_ctx_t *flow_tbl;
	uint8 i;

	ASSERT(flow_tbli != NULL);
	ASSERT(b != NULL);

	if (b == NULL || flow_tbli == NULL) {
		return BCME_BADARG;
	}

	flow_tbl = flow_tbli->flow_tbl;

	bcm_bprintf(b, "\nflowID\tscb\tbsscfg\trefcnt\n");

	for (i = 0; i < flow_tbli->num_entries; ++i) {
		bcm_bprintf(b, "%d", i + 1);
		if (flow_tbl[i].scb == NULL) {
			bcm_bprintf(b, "\tNULL");
		} else {
			bcm_bprintf(b, "\t%p", flow_tbl[i].scb);
		}
		if (flow_tbl[i].bsscfg == NULL) {
			bcm_bprintf(b, "\tNULL");
		} else {
			bcm_bprintf(b, "\t%p", flow_tbl[i].bsscfg);
		}
		bcm_bprintf(b, "\t%d\n", flow_tbl[i].ref_cnt);
	}

	return BCME_OK;
}
#endif // endif

wlc_flow_ctx_info_t *
BCMATTACHFN(wlc_flow_ctx_attach)(wlc_info_t *wlc)
{
	wlc_flow_ctx_info_t *flow_tbli;

	if ((flow_tbli = MALLOCZ(wlc->osh, sizeof(*flow_tbli))) == NULL) {
		WL_ERROR(("wl%d: %s: MALLOC failed, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->pub->osh)));
		goto fail;
	}

	flow_tbli->wlc = wlc;
	flow_tbli->pub = wlc->pub;
	flow_tbli->osh = wlc->osh;

	flow_tbli->num_entries = (AP_ENAB(wlc->pub)) ? MAX_FLOWS_AP : MAX_FLOWS_NONAP;

	flow_tbli->flow_tbl = MALLOCZ(wlc->osh,
	                              sizeof(*(flow_tbli->flow_tbl)) * flow_tbli->num_entries);
	if (flow_tbli->flow_tbl == NULL) {
		WL_ERROR(("wl%d: %s: MALLOC failed, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->pub->osh)));
		goto fail;
	}

#if defined(BCMDBG)
	wlc_dump_register(flow_tbli->pub, "flow_ctx", wlc_flow_ctx_module_dump, (void *)flow_tbli);
#endif // endif

	return flow_tbli;

fail:
	MODULE_DETACH(flow_tbli, wlc_flow_ctx_detach);

	return NULL;
}

void
BCMATTACHFN(wlc_flow_ctx_detach)(wlc_flow_ctx_info_t *flow_tbli)
{
	if (flow_tbli == NULL)
		return;

	if (flow_tbli->flow_tbl != NULL) {
		MFREE(flow_tbli->osh, flow_tbli->flow_tbl,
		      sizeof(*(flow_tbli->flow_tbl)) * flow_tbli->num_entries);
	}

	MFREE(flow_tbli->osh, flow_tbli, sizeof(*flow_tbli));
}

static flow_ctx_t *
wlc_flow_ctx_find(const wlc_flow_ctx_info_t *flow_tbli, uint16 flowID)
{
	ASSERT(flow_tbli != NULL);
	ASSERT(flow_tbli->num_entries != 0);
	ASSERT(flowID != 0);

	return (flowID <= flow_tbli->num_entries) ? &flow_tbli->flow_tbl[flowID - 1] : NULL;
}

/*
 * wlc_flow_ctx_add_context()
 *
 * Inputs:
 *	flow_tbli: flow context table module handle (i.e. returned from wlc_flow_ctx_attach)
 *	ctx      : user allocated structure to copy to the flow context table
 * Output:
 *	flowID set by this module, or BCME_NORESOURCE if table is full
 */
int
wlc_flow_ctx_add_context(wlc_flow_ctx_info_t *flow_tbli, const flow_ctx_t *ctx)
{
	flow_ctx_t *flow_tbl;
	uint8 i;

	ASSERT(flow_tbli != NULL);
	ASSERT(flow_tbli->num_entries != 0);
	ASSERT(ctx != NULL);

	flow_tbl = flow_tbli->flow_tbl;

	/* scan table for first empty slot */
	for (i = 0; flow_tbl[i].scb != NULL && i < flow_tbli->num_entries; ++i);

	/* already full table? */
	if (i == flow_tbli->num_entries) {
		WL_INFORM(("wl%d: Flow table full\n", flow_tbli->pub->unit));

		return BCME_NORESOURCE;
	}

	/* set the flow context */
	flow_tbl[i].ref_cnt = 0;
	flow_tbl[i].scb = ctx->scb;
	flow_tbl[i].bsscfg = ctx->bsscfg;

	WL_INFORM(("wl%d: Added FlowID %d to idx %d\n", flow_tbli->pub->unit, i + 1, i));

	return i + 1;	/* flowID = array index + 1 */
}

/*
 * wlc_flow_ctx_add_packet_refs()
 *
 * Inputs:
 *	flow_tbli: flow context table module handle (i.e. returned from wlc_flow_ctx_attach)
 *	flowID     : the ID of the flow to add packet references to in the flow context table
 *	num_packets: the number of packets that refer to the given flowID
 * Output:
 *	BCME_OK or flowID BCME_NOTFOUND
 */
int
wlc_flow_ctx_add_packet_refs(wlc_flow_ctx_info_t *flow_tbli, uint16 flowID, uint8 num_packets)
{
	flow_ctx_t *flow_tbl_entry = wlc_flow_ctx_find(flow_tbli, flowID);

	if (flow_tbl_entry == NULL) {
		return BCME_NOTFOUND;
	}

	ASSERT(flow_tbl_entry->ref_cnt < 0xFFFFU - num_packets);	/* leak check */
	flow_tbl_entry->ref_cnt += num_packets;

	return BCME_OK;
}

/*
 * wlc_flow_ctx_remove_packet_refs()
 *
 * Inputs:
 *	flow_tbli: flow context table module handle (i.e. returned from wlc_flow_ctx_attach)
 *	flowID     : the ID of the flow to remove packet references from the flow context table
 *	num_packets: the number of packets that refer to the given flowID
 * Output:
 *	BCME_OK or flowID BCME_NOTFOUND
 */
int
wlc_flow_ctx_remove_packet_refs(wlc_flow_ctx_info_t *flow_tbli, uint16 flowID, uint8 num_packets)
{
	flow_ctx_t *flow_tbl_entry = wlc_flow_ctx_find(flow_tbli, flowID);

	if (flow_tbl_entry == NULL) {
		return BCME_NOTFOUND;
	}

	if (flow_tbl_entry->ref_cnt <= num_packets) {
		flow_tbl_entry->ref_cnt = 0;
	} else {
		flow_tbl_entry->ref_cnt -= num_packets;
	}

	return BCME_OK;
}

/*
 * wlc_flow_ctx_del_context()
 *
 * Inputs:
 *	flow_tbli: flow context table module handle (i.e. returned from wlc_flow_ctx_attach)
 *	flowID   : the ID of the flow that we no longer use
 * Output:
 *	BCME_OK or flowID BCME_NOTFOUND or BCME_NOTREADY if there is still a reference to it
 */
int
wlc_flow_ctx_del_context(wlc_flow_ctx_info_t *flow_tbli, uint16 flowID)
{
	flow_ctx_t *flow_tbl_entry = wlc_flow_ctx_find(flow_tbli, flowID);

	if (flow_tbl_entry == NULL) {
		return BCME_NOTFOUND;
	}

	if (flow_tbl_entry->ref_cnt != 0) {
		return BCME_NOTREADY;	/* context is still referenced by one or more packets */
	}

	flow_tbl_entry->scb = NULL;
	flow_tbl_entry->bsscfg = NULL;

	return BCME_OK;
}

/*
 * wlc_flow_ctx_update()
 *
 * Inputs:
 *	flow_tbli: flow context table module handle (i.e. returned from wlc_flow_ctx_attach)
 *	flowID   : the ID of the flow to update
 *	ctx      : user allocated structure to copy to the flow context table, based on flowID
 * Output:
 *	BCME_OK or flowID BCME_NOTFOUND
 */
int
wlc_flow_ctx_update(const wlc_flow_ctx_info_t *flow_tbli, uint16 flowID, const flow_ctx_t *ctx)
{
	flow_ctx_t *flow_tbl_entry;

	ASSERT(ctx != NULL);

	flow_tbl_entry = wlc_flow_ctx_find(flow_tbli, flowID);

	if (flow_tbl_entry == NULL) {
		return BCME_NOTFOUND;
	}

	flow_tbl_entry->scb = ctx->scb;
	flow_tbl_entry->bsscfg = ctx->bsscfg;

	/*
	 * Note: scb/bsscfg has now been replaced for a particular flowID.  We need to inform
	 * the packet classification module that the old flow no longer exists
	 */

	return BCME_OK;
}

/*
 * wlc_flow_ctx_lookup()
 *
 * Inputs:
 *	flow_tbli: flow context table module handle (i.e. returned from wlc_flow_ctx_attach)
 *	flowID   : the ID of the flow to find
 * Output:
 *	NULL, or pointer to found entry
 */
const flow_ctx_t *
wlc_flow_ctx_lookup(const wlc_flow_ctx_info_t *flow_tbli, uint16 flowID)
{
	const flow_ctx_t *flow_tbl_entry = wlc_flow_ctx_find(flow_tbli, flowID);

	if (flow_tbl_entry == NULL) {
		return NULL;
	}

	WL_INFORM(("wl%d: FlowID %d found at idx %d\n", flow_tbli->pub->unit, flowID,
	          (int)(flow_tbl_entry - flow_tbli->flow_tbl)));

	return flow_tbl_entry;
}
