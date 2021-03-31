/*
 * wlc_flow_ctx.h
 *
 * Common interface for TX/RX Flow Context Table module
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
#ifndef _wlc_flow_ctx_h
#define _wlc_flow_ctx_h

#include <wlc_types.h>

typedef struct {
	uint16 ref_cnt;
	const scb_t *scb;
	const wlc_bsscfg_t *bsscfg;
} flow_ctx_t;

extern wlc_flow_ctx_info_t * wlc_flow_ctx_attach(wlc_info_t *wlc);
extern void wlc_flow_ctx_detach(wlc_flow_ctx_info_t *flow_tbli);

/*
 * Add a TX/RX context to flow table (returning flowID).
 *
 * Inputs:
 *	flow_tbli: flow context table module handle (i.e. returned from wlc_flow_ctx_attach)
 *	ctx      : user allocated structure to copy to the flow table
 * Output:
 *	flowID or BCME_NORESOURCE if table is full
 */
extern int wlc_flow_ctx_add_context(wlc_flow_ctx_info_t *flow_tbli, const flow_ctx_t *ctx);

/*
 * Delete a TX/RX context from flow table (given flowID).
 * Note: the flow will not be deleted if there is one or more packet reference to it
 *
 * Inputs:
 *	flow_tbli: flow context table module handle (i.e. returned from wlc_flow_ctx_attach)
 *	flowID   : the ID of the flow to delete from the flow table
 * Output:
 *	BCME_OK or flowID BCME_NOTFOUND or BCME_NOTREADY if there is still a reference to it
 */
extern int wlc_flow_ctx_del_context(wlc_flow_ctx_info_t *flow_tbli, uint16 flowID);

/*
 * Add packets to an existing TX/RX flow context
 *
 * Inputs:
 *	flow_tbli  : flow context table module handle (i.e. returned from wlc_flow_ctx_attach)
 *	flowID     : the ID of the flow to add packet references to in the flow context table
 *	num_packets: the number of packets that refer to the given flowID
 * Output:
 *	BCME_OK or flowID BCME_NOTFOUND
 */
extern int wlc_flow_ctx_add_packet_refs(wlc_flow_ctx_info_t *flow_tbli, uint16 flowID,
                                        uint8 num_packets);

/*
 * Remove packets of an existing TX/RX flow context
 *
 * Inputs:
 *	flow_tbli  : flow context table module handle (i.e. returned from wlc_flow_ctx_attach)
 *	flowID     : the ID of the flow to remove packet references from the flow context table
 *	num_packets: the number of packets that refer to the given flowID
 * Output:
 *	BCME_OK or flowID BCME_NOTFOUND
 */
extern int wlc_flow_ctx_remove_packet_refs(wlc_flow_ctx_info_t *flow_tbli, uint16 flowID,
                                           uint8 num_packets);

/*
 * Update a TX/RX context from flow table (given flowID).
 * Note: the user will need to inform the packet classification module about this flow change
 *
 * Inputs:
 *	flow_tbli: flow context table module handle (i.e. returned from wlc_flow_ctx_attach)
 *	flowID   : the ID of the flow to update
 *	ctx      : user allocated structure to copy to the flow table, based on flow->flowID
 * Output:
 *	BCME_OK or flow->flowID BCME_NOTFOUND
 */
extern int wlc_flow_ctx_update(const wlc_flow_ctx_info_t *flow_tbli, uint16 flowID,
                               const flow_ctx_t *ctx);

/*
 * Lookup a TX/RX context to flow table (given flowID).
 *
 * Inputs:
 *	flow_tbli: flow context table module handle (i.e. returned from wlc_flow_ctx_attach)
 *	flowID   : the ID of the flow to find
 * Output:
 *	NULL, or pointer to found entry
 */
extern const flow_ctx_t *wlc_flow_ctx_lookup(const wlc_flow_ctx_info_t *flow_tbli, uint16 flowID);

#endif /* _wlc_flow_ctx_h */
