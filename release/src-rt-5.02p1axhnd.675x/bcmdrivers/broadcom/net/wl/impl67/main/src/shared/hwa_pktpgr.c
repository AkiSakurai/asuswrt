/*
 * HWA Packet Pager Library routines
 * - response ring handlers
 * - attach, config, preinit, init, status and debug dump
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
 * $Id$
 *
 * vim: set ts=4 noet sw=4 tw=80:
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 */

#include <typedefs.h>
#include <osl.h>
#include <bcmendian.h>
#include <bcmutils.h>
#include <bcmpcie.h>
#include <bcmmsgbuf.h>
#include <hwa_lib.h>
#include <bcmhme.h>
#include <wlc_cfg.h>

#if defined(HWA_PKTPGR_BUILD)

/**
 * ---------------------------------------------------------------------------+
 * XXX Miscellaneous Packet Pager related registers|fields in other blocks:
 *
 * Block : Registers with Packet Pager related fields.
 * ----- : -------------------------------------------------------------------+
 * common: module_clk_enable, module_clkgating_enable, module_enable,
 *         module_reset, module_clkavail, module_idle
 *         dmatxsel, dmarxsel, hwa2hwcap, pageinstatus, pageinmask
 * rx    : rxpsrc_ring_hwa2cfg,
 *         d11bdest_threshold_l1l0, d11bdest_threshold_l2,
 *         hwa_rx_d11bdest_ring_wrindex_dir,
 *         pagein_status, recycle_status
 * txdma : pp_pagein_cfg, pp_pageout_cfg, pp_pagein_sts, pp_pageout_sts
 *
 * ---------------------------------------------------------------------------+
 */

/**
 * +--------------------------------------------------------------------------+
 *  Section: Handlers invoked to process Packet Pager Responses
 *  Parameter "hwa_pp_cmd_t *" is treated as a const pointer.
 * +--------------------------------------------------------------------------+
 */
// Forward declarations of all response ring handlers
#define HWA_PKTPGR_HANDLER_DECLARE(resp) \
static int hwa_pktpgr_ ## resp(void *ctx, hwa_dev_t *dev, hwa_pp_cmd_t *pp_cmd)

	                                            // PAGEIN response handlers
HWA_PKTPGR_HANDLER_DECLARE(pagein_rxprocess);   //  HWA_PKTPGR_PAGEIN_RXPROCESS
HWA_PKTPGR_HANDLER_DECLARE(pagein_txstatus);    //  HWA_PKTPGR_PAGEIN_TXSTATUS
HWA_PKTPGR_HANDLER_DECLARE(pagein_txpost);      //  HWA_PKTPGR_PAGEIN_TXPOST
	                                            // PAGEOUT response handlers
HWA_PKTPGR_HANDLER_DECLARE(pageout_pktlist);    //  HWA_PKTPGR_PAGEOUT_PKTLIST
HWA_PKTPGR_HANDLER_DECLARE(pageout_local);      //  HWA_PKTPGR_PAGEOUT_LOCAL
	                                            // PAGEMGR response handlers
HWA_PKTPGR_HANDLER_DECLARE(pagemgr_alloc_rx);   //  HWA_PKTPGR_PAGEMGR_ALLOC_RX
HWA_PKTPGR_HANDLER_DECLARE(pagemgr_alloc_rx_rph); //  HWA_PKTPGR_PAGEMGR_ALLOC_RX_RPH
HWA_PKTPGR_HANDLER_DECLARE(pagemgr_alloc_tx);   //  HWA_PKTPGR_PAGEMGR_ALLOC_TX
HWA_PKTPGR_HANDLER_DECLARE(pagemgr_push);       //  HWA_PKTPGR_PAGEMGR_PUSH
HWA_PKTPGR_HANDLER_DECLARE(pagemgr_pull);       //  HWA_PKTPGR_PAGEMGR_PULL

// NOTE: pktlist_head and pktlist_tail address space are in HDBM.
// In PUSH req the packet in DDBM will be freed by HW.
static void
hwa_pktpgr_push_rsp(hwa_dev_t *dev, uint8 pktpgr_trans_id,
	uint8 tagged, int pkt_count, uint16 pkt_mapid,
	hwa_pp_lbuf_t *pktlist_head, hwa_pp_lbuf_t *pktlist_tail)
{
	// Audit pre-conditions
	HWA_AUDIT_DEV(dev);

#ifdef PSPL_TX_TEST
	if (tagged == HWA_PP_CMD_NOT_TAGGED) {
		hwa_pp_pagemgr_req_pull_t req;
		uint32 elem_ix;
		uint16 fifo_idx;
		uint16 mpdu_count;
		hwa_pktpgr_t *pktpgr;
		hwa_pp_pspl_req_info_t *pspl_req_info;
		int trans_id;

		// Setup local
		pktpgr = &dev->pktpgr;

		// TEST: TxPost-->Push-->Pull-->TxFIFO
		HWA_ASSERT(pktpgr->pspl_test_en);

		// NOTE: pktlist_head and pktlist_tail aress space are in HDBM.
		HWA_PP_DBG(HWA_PP_DBG_MGR_TX, "  <<PAGEMGR::RSP PUSH : pkt %3u "
			"list[%p(%d) .. %p] <==PUSH-RSP(%d)==\n\n", pkt_count,
			pktlist_head, pkt_mapid, pktlist_tail, pktpgr_trans_id);

		// Get pspl_req_info from PUSH req.
		elem_ix = bcm_ring_cons(&pktpgr->pspl_state, pktpgr->pspl_depth);
		HWA_ASSERT(elem_ix != BCM_RING_EMPTY);
		pspl_req_info = BCM_RING_ELEM(pktpgr->pspl_table, elem_ix,
			sizeof(hwa_pp_pspl_req_info_t));
		fifo_idx = pspl_req_info->fifo_idx;
		HWA_ASSERT(pkt_mapid == pspl_req_info->pkt_mapid);
		mpdu_count = pspl_req_info->mpdu_count;
		HWA_ASSERT(pkt_count == pspl_req_info->pkt_count);

		// PULL Request
		req.trans        = HWA_PP_PAGEMGR_PULL;
		req.pkt_count    = pkt_count;
		req.pkt_mapid    = pkt_mapid;
		req.tagged       = HWA_PP_CMD_NOT_TAGGED;

		// NOTE: pktlist_head and pktlist_tail aress space are in HDBM, cannot dump them.
		trans_id = hwa_pktpgr_request(dev, hwa_pktpgr_pagemgr_req_ring, &req);
		if (trans_id == HWA_FAILURE) {
			HWA_ERROR((">>PAGEMGR::REQ PULL_REQ failure: pkts<%d> "
				"list[%p .. %p] pkt_mapid %3u", pkt_count,
				pktlist_head, pktlist_tail, pkt_mapid));
			HWA_ASSERT(0);
		}

		// Qeue fifo_idx and pkt_mapid for PULL rsp.
		elem_ix = bcm_ring_prod(&pktpgr->pspl_state, pktpgr->pspl_depth);
		pspl_req_info = BCM_RING_ELEM(pktpgr->pspl_table, elem_ix,
			sizeof(hwa_pp_pspl_req_info_t));
		pspl_req_info->fifo_idx = fifo_idx;
		pspl_req_info->pkt_mapid = pkt_mapid;
		pspl_req_info->mpdu_count = mpdu_count;
		pspl_req_info->pkt_count = pkt_count;

		HWA_PP_DBG(HWA_PP_DBG_MGR_TX, "  >>PAGEMGR_PULL::REQ pkts %3u/%3u fifo %3u "
			"pkt_mapid %3u ==PULL(%d)==>\n\n", mpdu_count, pkt_count, fifo_idx,
			pkt_mapid, pktpgr_trans_id);
	} else
#endif /* PSPL_TX_TEST */
	{
		// Special tagged request process
		HWA_ASSERT(tagged == HWA_PP_CMD_TAGGED);

		HWA_PP_DBG(HWA_PP_DBG_MGR_TX, "  <<PAGEMGR::RSP PUSH(tagged) : pkt %3u "
			"list[%p(%d) .. %p] <==PUSH-RSP(%d)==\n\n",
			pkt_count, pktlist_head, pkt_mapid, pktlist_tail, pktpgr_trans_id);
	}
}

static void
hwa_pktpgr_pull_rsp(hwa_dev_t *dev, uint8 pktpgr_trans_id, uint8 tagged,
	int pkt_count, hwa_pp_lbuf_t *pktlist_head, hwa_pp_lbuf_t *pktlist_tail)
{
	hwa_pktpgr_t *pktpgr;

	// Audit pre-conditions
	HWA_AUDIT_DEV(dev);

	// Setup local
	pktpgr = &dev->pktpgr;

	if (pkt_count == 0) {
		HWA_ERROR(("%s: pkt_count is 0\n", __FUNCTION__));
		return;
	}

#ifdef PSPL_TX_TEST
	if (tagged == HWA_PP_CMD_NOT_TAGGED) {
		uint32 elem_ix;
		uint16 fifo_idx;
		uint16 mpdu_count;
		hwa_pp_pspl_req_info_t *pspl_req_info;

		// TEST: TxPost-->Push-->Pull-->TxFIFO
		HWA_ASSERT(pktpgr->pspl_test_en);

		HWA_PP_DBG(HWA_PP_DBG_MGR_TX, "  <<PAGEMGR::RSP PULL : pkt %3u "
			"list[%p(%d) .. %p(%d)] <==PULL-RSP(%d)==\n\n",
			pkt_count, pktlist_head, PKTMAPID(pktlist_head),
			pktlist_tail, PKTMAPID(pktlist_tail), pktpgr_trans_id);

		// Get pspl_req_info from PULL req.
		elem_ix = bcm_ring_cons(&pktpgr->pspl_state, pktpgr->pspl_depth);
		HWA_ASSERT(elem_ix != BCM_RING_EMPTY);
		pspl_req_info = BCM_RING_ELEM(pktpgr->pspl_table, elem_ix,
			sizeof(hwa_pp_pspl_req_info_t));
		fifo_idx = pspl_req_info->fifo_idx;
		mpdu_count = pspl_req_info->mpdu_count;
		HWA_ASSERT(pkt_count == pspl_req_info->pkt_count);
		HWA_ASSERT(pspl_req_info->pkt_mapid == PKTMAPID(pktlist_head));

		// Tx it.
		hwa_txfifo_pull_pktchain_xmit_request(dev, pktpgr_trans_id, fifo_idx,
			mpdu_count, pkt_count, pktlist_head, pktlist_tail);
	} else
#endif /* PSPL_TX_TEST */
	{
		struct spktq temp_q;

		// Special tagged request process
		HWA_ASSERT(tagged == HWA_PP_CMD_TAGGED);

		// Init the temporary q
		spktq_init_list(&temp_q, PKTQ_LEN_MAX, pktlist_head, pktlist_tail, pkt_count);
		// Add to tag_pull_q
		spktq_enq_list(&pktpgr->tag_pull_q, &temp_q);
		// Deinit the temporary queue
		spktq_deinit(&temp_q);
	}
}

int // HWA_PKTPGR_PAGEIN_RXPROCESS handler
hwa_pktpgr_pagein_rxprocess(void *ctx, hwa_dev_t *dev, hwa_pp_cmd_t *pp_cmd)
{
	hwa_pp_pagein_rsp_rxprocess_t *rxprocess = &pp_cmd->pagein_rsp_rxprocess;

	HWA_FTRACE(HWApp);
	HWA_ASSERT(rxprocess->trans == HWA_PP_PAGEIN_RXPROCESS);

	HWA_ASSERT(dev->pktpgr.pgi_rxpkt_req > 0);
	dev->pktpgr.pgi_rxpkt_req--;

	HWA_ASSERT(dev->pktpgr.pgi_rxpkt_in_trans >= rxprocess->pkt_count);
	dev->pktpgr.pgi_rxpkt_in_trans -= rxprocess->pkt_count;

	// process pktlist
	hwa_rxfill_pagein_rx_rsp(dev,
		rxprocess->trans_id, rxprocess->pkt_count,
		(hwa_pp_lbuf_t*)rxprocess->pktlist_head,
		(hwa_pp_lbuf_t*)rxprocess->pktlist_tail);

	return rxprocess->trans_id;

}   // hwa_pktpgr_pagein_rxprocess()

int // HWA_PKTPGR_PAGEIN_TXSTATUS handler
hwa_pktpgr_pagein_txstatus(void *ctx, hwa_dev_t *dev, hwa_pp_cmd_t *pp_cmd)
{
	hwa_pp_pagein_rsp_txstatus_t *txstatus = &pp_cmd->pagein_rsp_txstatus;

	HWA_FTRACE(HWApp);
	HWA_ASSERT(txstatus->trans == HWA_PP_PAGEIN_TXSTATUS);
	HWA_ASSERT(txstatus->fifo_idx < HWA_TX_FIFOS_MAX);

	// Assume one pagein TxS req map to one pagein TxS rsp.
	HWA_ASSERT(dev->pktpgr.pgi_txs_req[txstatus->fifo_idx] > 0);
	HWA_ASSERT(dev->pktpgr.pgi_txs_req_tot > 0);
	dev->pktpgr.pgi_txs_req[txstatus->fifo_idx]--;
	dev->pktpgr.pgi_txs_req_tot--;

	// process pktlist
	hwa_txstat_pagein_rsp(dev, txstatus->trans_id,
		txstatus->tagged, txstatus->fifo_idx,
		txstatus->mpdu_count, txstatus->pkt_count,
		(hwa_pp_lbuf_t*)txstatus->pktlist_head,
		(hwa_pp_lbuf_t*)txstatus->pktlist_tail);

	return txstatus->trans_id;

}   // hwa_pktpgr_pagein_txstatus()

int // HWA_PKTPGR_PAGEIN_TXPOST handler
hwa_pktpgr_pagein_txpost(void *ctx, hwa_dev_t *dev, hwa_pp_cmd_t *pp_cmd)
{
	hwa_pp_pagein_rsp_txpost_t *txpost = &pp_cmd->pagein_rsp_txpost;

	HWA_FTRACE(HWApp);
	HWA_ASSERT((txpost->trans == HWA_PP_PAGEIN_TXPOST_WITEMS) ||
	           (txpost->trans == HWA_PP_PAGEIN_TXPOST_WITEMS_FRC));

	// process pktlist
	hwa_txpost_pagein_rsp(dev, txpost->trans_id,
		txpost->pkt_count, txpost->oct_count,
		(hwa_pp_lbuf_t*)txpost->pktlist_head,
		(hwa_pp_lbuf_t*)txpost->pktlist_tail);

	return txpost->trans_id;

}   // hwa_pktpgr_pagein_txpost()

int // HWA_PKTPGR_PAGEOUT_PKTLIST handler
hwa_pktpgr_pageout_pktlist(void *ctx, hwa_dev_t *dev, hwa_pp_cmd_t *pp_cmd)
{
	hwa_pp_pageout_rsp_pktlist_t *pktlist = &pp_cmd->pageout_rsp_pktlist;

	HWA_FTRACE(HWApp);
	HWA_ASSERT(pktlist->trans == HWA_PP_PAGEOUT_PKTLIST_WR);

	hwa_txfifo_pageout_pktlist_rsp(dev, pktlist->trans_id, pktlist->fifo_idx,
		pktlist->mpdu_count, pktlist->total_txdma_desc,
		(hwa_pp_lbuf_t*)pktlist->pkt_local);

	return pktlist->trans_id;

}   // hwa_pktpgr_pageout_pktlist()

int // HWA_PKTPGR_PAGEOUT_LOCAL handler
hwa_pktpgr_pageout_local(void *ctx, hwa_dev_t *dev, hwa_pp_cmd_t *pp_cmd)
{
	hwa_pp_pageout_rsp_pktlist_t *pktlocal = &pp_cmd->pageout_rsp_pktlist;

	HWA_FTRACE(HWApp);
	HWA_ASSERT(pktlocal->trans == HWA_PP_PAGEOUT_PKTLOCAL);

	HWA_ASSERT(dev->pktpgr.pgo_local_req > 0);
	dev->pktpgr.pgo_local_req--;

	// free locally allocated TxLfrag
	hwa_txfifo_pageout_local_rsp(dev, pktlocal->trans_id, pktlocal->fifo_idx,
		pktlocal->mpdu_count, pktlocal->total_txdma_desc,
		(hwa_pp_lbuf_t*)pktlocal->pkt_local);

	return pktlocal->trans_id;

}   // hwa_pktpgr_pageout_local()

int // HWA_PKTPGR_PAGEMGR_ALLOC_RX handler
hwa_pktpgr_pagemgr_alloc_rx(void *ctx, hwa_dev_t *dev, hwa_pp_cmd_t *pp_cmd)
{
	hwa_pp_pagemgr_rsp_alloc_t *alloc_rx = &pp_cmd->pagemgr_rsp_alloc;

	HWA_FTRACE(HWApp);
	HWA_ASSERT(alloc_rx->trans == HWA_PP_PAGEMGR_ALLOC_RX);

	HWA_ERROR(("WIP: <<PAGEMGR::RSP HWA_PP_PAGEMGR_ALLOC_RX\n\n"));

	// XXX --- use allocated rxlfrags

	return alloc_rx->trans_id;

}   // hwa_pktpgr_pagemgr_alloc_rx()

// XXX NOTE: We only need RPH.  Rx packet is useless.
int // HWA_PKTPGR_PAGEMGR_ALLOC_RX_RPH handler
hwa_pktpgr_pagemgr_alloc_rx_rph(void *ctx, hwa_dev_t *dev, hwa_pp_cmd_t *pp_cmd)
{
	hwa_pp_pagemgr_rsp_alloc_t *alloc_rph = &pp_cmd->pagemgr_rsp_alloc;

	HWA_FTRACE(HWApp);
	HWA_ASSERT(alloc_rph->trans == HWA_PP_PAGEMGR_ALLOC_RX_RPH);

	// XXX --- use allocated rxpost host info
	hwa_rxpath_pagemgr_alloc_rx_rph_rsp(dev,
		alloc_rph->trans_id, alloc_rph->pkt_count,
		(hwa_pp_lbuf_t*)alloc_rph->pktlist_head,
		(hwa_pp_lbuf_t*)alloc_rph->pktlist_tail);

	return alloc_rph->trans_id;

}   // hwa_pktpgr_pagemgr_alloc_rx_rph()

int // HWA_PKTPGR_PAGEMGR_ALLOC_TX handler
hwa_pktpgr_pagemgr_alloc_tx(void *ctx, hwa_dev_t *dev, hwa_pp_cmd_t *pp_cmd)
{
	hwa_pp_pagemgr_rsp_alloc_t *alloc_tx = &pp_cmd->pagemgr_rsp_alloc;

	HWA_FTRACE(HWApp);
	HWA_ASSERT(alloc_tx->trans == HWA_PP_PAGEMGR_ALLOC_TX);

	// process pktlist
	hwa_txpost_pagemgr_alloc_tx_rsp(dev,
		alloc_tx->trans_id, alloc_tx->pkt_count,
		(hwa_pp_lbuf_t*)alloc_tx->pktlist_head,
		(hwa_pp_lbuf_t*)alloc_tx->pktlist_tail);

	return alloc_tx->trans_id;

}   // hwa_pktpgr_pagemgr_alloc_tx()

int // HWA_PKTPGR_PAGEMGR_PUSH handler
hwa_pktpgr_pagemgr_push(void *ctx, hwa_dev_t *dev, hwa_pp_cmd_t *pp_cmd)
{
	hwa_pp_pagemgr_rsp_push_t *push = &pp_cmd->pagemgr_rsp_push;

	HWA_FTRACE(HWApp);
	HWA_ASSERT((push->trans == HWA_PP_PAGEMGR_PUSH) ||
	           (push->trans == HWA_PP_PAGEMGR_PUSH_PKTTAG));

	if (push->tagged == HWA_PP_CMD_TAGGED) {
		dev->pktpgr.tag_push_req--;
	}

	// NOTE: pktlist_head and pktlist_tail aress space are in HDBM.
	hwa_pktpgr_push_rsp(dev, push->trans_id, push->tagged, push->pkt_count,
		push->pkt_mapid, (hwa_pp_lbuf_t*)push->pktlist_head,
		(hwa_pp_lbuf_t*)push->pktlist_tail);

	return push->trans_id;

}   // hwa_pktpgr_pagemgr_push()

int // HWA_PKTPGR_PAGEMGR_PULL handler
hwa_pktpgr_pagemgr_pull(void *ctx, hwa_dev_t *dev, hwa_pp_cmd_t *pp_cmd)
{
	hwa_pp_pagemgr_rsp_pull_t *pull = &pp_cmd->pagemgr_rsp_pull;

	HWA_FTRACE(HWApp);
	HWA_ASSERT((pull->trans == HWA_PP_PAGEMGR_PULL) ||
	           (pull->trans == HWA_PP_PAGEMGR_PULL_KPFL_LINK));

	if (pull->tagged == HWA_PP_CMD_TAGGED) {
		dev->pktpgr.tag_pull_req--;
	}

	// process pull
	hwa_pktpgr_pull_rsp(dev, pull->trans_id,
		pull->tagged, pull->pkt_count,
		(hwa_pp_lbuf_t*)pull->pktlist_head,
		(hwa_pp_lbuf_t*)pull->pktlist_tail);

	return pull->trans_id;

}   // hwa_pktpgr_pagemgr_pull()

static INLINE int // Ring base, free a buffer to HDBM or DDBM base on its pkt_mapid/buf_idx
hwa_pktpgr_dbm_free_fast(hwa_dev_t *dev, hwa_dbm_instance_t dbm_instance, uint16 buf_idx)
{
	int trans_id, req_loop;
	hwa_pp_freedbm_req_t req;

	HWA_FTRACE(HWApp);

	// Audit pre-conditions
	HWA_AUDIT_DEV(dev);
	HWA_ASSERT(buf_idx < dev->pktpgr.hostpktpool_max);

	HWA_TRACE(("%s DBM<%d> free buf_idx<%u>\n", HWApp, bm_instance, buf_idx));

	req.trans        = (dbm_instance == HWA_HDBM) ?
		HWA_PP_PAGEMGR_FREE_HDBM : HWA_PP_PAGEMGR_FREE_DDBM;
	req.pkt_count    = 1;
	// buf_idx is pkt_mapid for HDBM. buf_idx is dbm index for DDBM
	req.pkt_mapid[0] = buf_idx;

	req_loop = HWA_LOOPCNT;

req_again:
	trans_id = hwa_pktpgr_request(dev, hwa_pktpgr_freepkt_req_ring, &req);
	if (trans_id == HWA_FAILURE) {
		if (--req_loop > 0) {
			OSL_DELAY(10);
			goto req_again;
		}
		HWA_ERROR((">>PAGEMGR::REQ FREE_%s failure: buf_idx<%u>\n",
			(dbm_instance == HWA_HDBM) ? "HDBM" : "DDBM", buf_idx));
		HWA_ASSERT(0);
		return HWA_FAILURE;
	}

	HWA_PP_DBG(HWA_PP_DBG_MGR_TX, "  >>PAGEMGR::REQ FREE_%s      : buf_idx<%u> "
		"==FREE_%s-REQ(%d), no rsp ==>\n\n", (dbm_instance == HWA_HDBM) ? "HDBM" : "DDBM",
		buf_idx, (dbm_instance == HWA_HDBM) ? "HDBM" : "DDBM", trans_id);

	return HWA_SUCCESS;
} // hwa_pktpgr_dbm_free

static INLINE int // Register base, free a buffer to HDBM or DDBM base on its buf_idx
_hwa_pktpgr_dbm_free(hwa_dev_t *dev, hwa_dbm_instance_t dbm_instance, uint16 buf_idx)
{
	uint32 u32, status, loopcnt;
	hwa_pager_regs_t *pager_regs;

	HWA_FTRACE(HWApp);

	// Audit pre-conditions
	HWA_AUDIT_DEV(dev);

	// Setup locals
	pager_regs = &dev->regs->pager;

	HWA_TRACE(("%s DBM<%d> free buf_idx<%u>\n", HWApp, bm_instance, buf_idx));

	if (dbm_instance == HWA_HDBM) {
		HWA_ASSERT(buf_idx < dev->pktpgr.hostpktpool_max);
		u32 = BCM_SBF(buf_idx, HWA_PAGER_PP_HOSTPKTPOOL_DEALLOC_INDEX_PPHDDEALLOCIDX);
		HWA_WR_REG_ADDR(HWApp, &pager_regs->hostpktpool.dealloc_index, u32);
	} else { // DDBM
		HWA_ASSERT(buf_idx < dev->pktpgr.dnglpktpool_max);
		u32 = BCM_SBF(buf_idx, HWA_PAGER_PP_DNGLPKTPOOL_DEALLOC_INDEX_PPDDDEALLOCIDX);
		HWA_WR_REG_ADDR(HWApp, &pager_regs->dnglpktpool.dealloc_index, u32);
	}

	for (loopcnt = 0; loopcnt < HWA_BM_LOOPCNT; loopcnt++) {
		if (dbm_instance == HWA_HDBM) {
			u32 = HWA_RD_REG_ADDR(HWApp, &pager_regs->hostpktpool.dealloc_status);
			status = BCM_GBF(u32,
				HWA_PAGER_PP_HOSTPKTPOOL_DEALLOC_STATUS_PPHDDEALLOCSTS);
		} else {
			u32 = HWA_RD_REG_ADDR(HWApp, &pager_regs->dnglpktpool.dealloc_status);
			status = BCM_GBF(u32,
				HWA_PAGER_PP_DNGLPKTPOOL_DEALLOC_STATUS_PPDDDEALLOCSTS);
		}

		// WAR for HWA2.x
		if (status == HWA_BM_SUCCESS_SW) {
			return HWA_SUCCESS;
		}

		if (status & HWA_BM_DONEBIT) {
			HWA_ASSERT(status == HWA_BM_SUCCESS);
			return HWA_SUCCESS;
		}
	}

	HWA_WARN(("%s DBM<%d> free failure\n", HWApp, dbm_instance));

	HWA_ASSERT(0);

	return HWA_FAILURE;

} // hwa_pktpgr_dbm_free

int // Free a buffer to HDBM or DDBM base on its pkt_mapid/buf_idx
hwa_pktpgr_dbm_free(hwa_dev_t *dev, hwa_dbm_instance_t dbm_instance, uint16 buf_idx)
{
	int ret = HWA_SUCCESS;

	// Audit pre-conditions
	HWA_AUDIT_DEV(dev);

	if (HWAREV_IS(dev->corerev, 131)) {
		ret = _hwa_pktpgr_dbm_free(dev, dbm_instance, buf_idx);
	} else {
		// buf_idx is pkt_mapid for HDBM. buf_idx is dbm index for DDBM
		ret = hwa_pktpgr_dbm_free_fast(dev, dbm_instance, buf_idx);
	}

	return ret;
}

static uint16 // Get available count of HDBM or DDBM
hwa_pktpgr_dbm_availcnt(hwa_dev_t *dev, hwa_dbm_instance_t dbm_instance)
{
	uint16 availcnt;
	uint32 u32;
	hwa_pager_regs_t *pager_regs;

	HWA_FTRACE(HWApp);

	// Audit pre-conditions
	HWA_AUDIT_DEV(dev);

	// Setup locals
	pager_regs = &dev->regs->pager;
	availcnt = 0;

	if (dbm_instance == HWA_DDBM) {
		u32 = HWA_RD_REG_ADDR(HWApp, &pager_regs->dnglpktpool.ctrl);
		availcnt = BCM_GBF(u32, HWA_PAGER_PP_DNGLPKTPOOL_CTRL_PPDDAVAILCNT);
	} else { // HDBM
		u32 = HWA_RD_REG_ADDR(HWApp, &pager_regs->hostpktpool.ctrl);
		availcnt = BCM_GBF(u32, HWA_PAGER_PP_HOSTPKTPOOL_CTRL_PPHDAVAILCNT);
	}

	return availcnt;
}

void
hwa_pktpgr_print_pooluse(hwa_dev_t *dev)
{
	uint32 mem_sz;
	hwa_pktpgr_t *pktpgr = &dev->pktpgr;

	mem_sz = sizeof(hwa_pp_lbuf_t) * pktpgr->dnglpktpool_max;

	HWA_PRINT("\tIn use HDBM %d(%d) DDBM %d(%d)(%dK)\n",
		pktpgr->hostpktpool_max, hwa_pktpgr_dbm_availcnt(dev, HWA_HDBM),
		pktpgr->dnglpktpool_max, hwa_pktpgr_dbm_availcnt(dev, HWA_DDBM),
		KB(mem_sz));
}

/**
 * +--------------------------------------------------------------------------+
 *  Generic Packet Pager consumer loop. Each element of the Response ring is
 *  processed by invoking the handler identified by the transaction command type
 *
 *  All elements from RD to WR index are processed, unbounded. Each response
 *  element is holding onto packets in dongle's packet storage, and needs to be
 *  promptly processed to completion.
 *  Bounding algorithms should be enforced during requests.
 *
 *  There are no response rings corresponding to Free pkt/rph/d11b requests.
 * +--------------------------------------------------------------------------+
 */
void
hwa_pktpgr_response(hwa_dev_t *dev, hwa_ring_t *rsp_ring,
	hwa_callback_t callback)
{
	uint8          trans_cmd;
	uint32         elem_ix;         // location of next element to read
	hwa_pp_cmd_t  *rsp_cmd;         // pointer to command in h2s response ring
	hwa_handler_t *rsp_handler;     // response handler

	HWA_FTRACE(HWApp);
	HWA_AUDIT_DEV(dev);

	hwa_ring_cons_get(rsp_ring);    // fetch response ring's WR index once

	while ((elem_ix = hwa_ring_cons_upd(rsp_ring)) != BCM_RING_EMPTY) {
		rsp_cmd     = HWA_RING_ELEM(hwa_pp_cmd_t, rsp_ring, elem_ix);
		trans_cmd   = rsp_cmd->u8[HWA_PP_CMD_TRANS];
		rsp_handler = &dev->handlers[callback + trans_cmd];

		(*rsp_handler->callback)(rsp_handler->context, dev, rsp_cmd);
	}

	hwa_ring_cons_put(rsp_ring); // commit RD index now

	if (!hwa_ring_is_empty(rsp_ring)) {
		/* need re-schdeule */
		dev->pageintstatus |= HWA_COMMON_PAGEIN_INT_MASK;
		dev->intstatus |= HWA_COMMON_INTSTATUS_PACKET_PAGER_INTR_MASK;
	}

	return;

}    // hwa_pktpgr_response()

/**
 * +--------------------------------------------------------------------------+
 *  Generic API to post a Request command into one of the S2H interfaces
 *  to the Packet Pager. Caller must compose/marshall/pack a Packet Pager
 *  command, and pass to this requst API. This API copies the 16B command into
 *  a slot in the request interface. If the request ring is full a HWA_FAILURE
 *  is returned, otherwise a transaction id is returned.
 *
 *  Transaction ID is a 8bit incrementing unsigned char. As a request ring may
 *  be deeper than 256 elements, a transaction Id does not necessarily identify
 *  a unique RD index in the ring.
 * +--------------------------------------------------------------------------+
 */

int // HWApp: Post a request command
hwa_pktpgr_request(hwa_dev_t *dev, hwa_pktpgr_ring_t pp_ring, void *pp_cmd)
{
	uint8         trans_id;         // per s2h_ring incrementing transaction id
	hwa_ring_t   *req_ring;         // sw to hw request ring
	hwa_pp_cmd_t *req_cmd;          // location in s2h ring to place the cmd
	hwa_pktpgr_t *pktpgr;           // pktpgr local state

	HWA_FTRACE(HWApp);
	HWA_ASSERT(dev != (struct hwa_dev *)NULL);
	HWA_ASSERT(pp_ring < hwa_pktpgr_req_ring_max);

	pktpgr = &dev->pktpgr;
	req_ring = pktpgr->req_ring[pp_ring];

	if (hwa_ring_is_full(req_ring))
		goto failure;

	trans_id = pktpgr->trans_id[pp_ring]++; // increment transaction id
	req_cmd  = HWA_RING_PROD_ELEM(hwa_pp_cmd_t, req_ring);

	*req_cmd = *((hwa_pp_cmd_t*)pp_cmd);    // copy 16 byte command into ring
	// Clear invalid field for new request.
	req_cmd->u8[HWA_PP_CMD_TRANS] = BCM_CBIT(req_cmd->u8[HWA_PP_CMD_TRANS],
		HWA_PP_CMD_INVALID);

	// Take one bit from trans_id for special purpose in pagein req.
	// XXX, hwa_pktpgr_freerph_req_ring, hwa_pktpgr_pagemgr_req_ring
	// trans_id must keep 8 bits because of Rx reclaim and map_pkts_cb.
	if (pp_ring == hwa_pktpgr_pagein_req_ring ||
		pp_ring == hwa_pktpgr_pagemgr_req_ring) {
		pktpgr->trans_id[pp_ring] &= HWA_PP_CMD_TRANS_ID_MASK;
		req_cmd->pagein_req.trans_id = trans_id; // overwrite trans_id
		// tagged bit must be handled by caller.
	} else {
		req_cmd->u8[HWA_PP_CMD_TRANS_ID] = trans_id; // overwrite trans_id
	}

	hwa_ring_prod_upd(req_ring, 1, TRUE);	// update/commit WR

	return trans_id;

failure:
	HWA_ERROR(("%s req ring %u pp_cmd type 0x%x failure\n", HWApp,
		pp_ring, ((hwa_pp_cmd_t*)pp_cmd)->u8[HWA_PP_CMD_TRANS]));

	return HWA_FAILURE;

}   // hwa_pktpgr_request()

// +--------------------------------------------------------------------------+

hwa_ring_t * // Initialize SW and HW ring contexts.
BCMATTACHFN(hwa_pktpgr_ring_init)(hwa_ring_t *hwa_ring,
	const char *ring_name, uint8 ring_dir, uint8 ring_num,
	uint16 depth, void *memory,
	hwa_regs_t *regs, hwa_pp_ring_t *hwa_pp_ring)
{
	uint32 v32;
	hwa_ring_init(hwa_ring, ring_name, HWA_PKTPGR_ID, ring_dir, ring_num,
		depth, memory, &hwa_pp_ring->wr_index, &hwa_pp_ring->rd_index);

	v32 = HWA_PTR2UINT(memory);
	HWA_WR_REG_ADDR(ring_name, &hwa_pp_ring->addr, v32);

	HWA_ASSERT(depth == HWA_PKTPGR_INTERFACE_DEPTH);
	v32 = BCM_SBF(depth, HWA_PAGER_PP_RING_CFG_DEPTH);
	if (ring_dir == HWA_RING_S2H) {
		v32 |= BCM_SBF(HWA_PKTPGR_REQ_WAITTIME,
		               HWA_PAGER_PP_REQ_RING_CFG_WAITTIME);
		if (ring_num == HWA_PKTPGR_PAGEIN_S2H_RINGNUM) {
			v32 |= BCM_SBF(HWA_PKTPGR_RX_BESTEFFORT,
			               HWA_PAGER_PP_PAGEIN_REQ_RING_CFG_PPINRXPROCESSBE);
		}
	}
	HWA_WR_REG_ADDR(ring_name, &hwa_pp_ring->cfg, v32);

	v32 = BCM_SBF(HWA_PKTPGR_INTRAGGR_COUNT,
	              HWA_PAGER_PP_RING_LAZYINT_CFG_AGGRCOUNT)
	    | BCM_SBF(HWA_PKTPGR_INTRAGGR_TMOUT,
	              HWA_PAGER_PP_RING_LAZYINT_CFG_AGGRTIMER);
	HWA_WR_REG_ADDR(ring_name, &hwa_pp_ring->lazyint_cfg, v32);

	return hwa_ring;

}   // hwa_pktpgr_ring_init()

hwa_pktpgr_t * // HWApp: Allocate all required memory for PktPgr
BCMATTACHFN(hwa_pktpgr_attach)(hwa_dev_t *dev)
{
	int ring;
	uint32 v32, mem_sz, depth;
	hwa_regs_t *regs;
	hwa_pager_regs_t *pager_regs;
	hwa_pktpgr_t *pktpgr;
	void *memory[hwa_pktpgr_ring_max], *ring_memory;
#ifdef PSPL_TX_TEST
	uint32 pspl_table_sz;
	void *pspl_table = NULL;
#endif // endif

	HWA_FTRACE(HWApp);

	// Audit pre-conditions
	HWA_AUDIT_DEV(dev);

	// Setup locals
	regs = dev->regs;
	pager_regs = &regs->pager;
	pktpgr = &dev->pktpgr;

	// Verify PP block structures
	HWA_ASSERT(HWA_PP_LBUF_SZ ==
	           (HWA_PP_PKTCONTEXT_BYTES + HWA_PP_PKTDATABUF_BYTES));
	HWA_ASSERT(sizeof(hwa_pp_cmd_t) == HWA_PP_COMMAND_BYTES);

	// Align dnglpktpool_mem to 256B
	mem_sz = sizeof(hwa_pp_lbuf_t) * HWA_DNGL_PKTS_MAX;
	pktpgr->dnglpktpool_mem = MALLOC_ALIGN(dev->osh, mem_sz, 8);
	if (pktpgr->dnglpktpool_mem == NULL) {
		HWA_ERROR(("%s lbuf pool items<%u> mem_size<%u> failure\n",
			HWApp, HWA_DNGL_PKTS_MAX, mem_sz));
		HWA_ASSERT(pktpgr->dnglpktpool_mem != ((hwa_pp_lbuf_t *)NULL));
		goto lbuf_pool_failure;
	}
	ASSERT(ISALIGNED(pktpgr->dnglpktpool_mem, 256));

	pktpgr->dnglpktpool_max = HWA_DNGL_PKTS_MAX;

	// Allocate and initialize the S2H Request and H2S Response interfaces
	// XXX, Be careful if you want to adjust some rings depth. The ring index
	// here to allocate memory is not the same in hwa_pktpgr_ring_init.
	depth = HWA_PKTPGR_INTERFACE_DEPTH;
	mem_sz = depth * sizeof(hwa_pp_cmd_t);
	for (ring = 0; ring < hwa_pktpgr_ring_max; ++ring) {
		// 4KB alignment can reduce transaction times.
		if ((memory[ring] = MALLOC_ALIGN(dev->osh, mem_sz, 12)) == NULL) {
			HWA_ERROR(("%s ring %u size<%u> failure\n", HWApp, ring, mem_sz));
			HWA_ASSERT(memory[ring] != (void*)NULL);
			goto ring_failure;
		}
		ASSERT(ISALIGNED(memory[ring], 4096));
	}

#ifdef PSPL_TX_TEST
	// Allocate and initialize the PUSH/PULL request info ring
	pspl_table_sz = HWA_PKTPGR_PSPL_DEPTH * sizeof(hwa_pp_pspl_req_info_t);
	if ((pspl_table = MALLOCZ(dev->osh, pspl_table_sz)) == NULL) {
		HWA_ERROR(("%s pspl ring malloc size<%u> failure\n",
			HWApp, pspl_table_sz));
		HWA_ASSERT(pspl_table != (void*)NULL);
		goto ring_failure;
	}
#endif /* PSPL_TX_TEST */

	// Initial tag_pull_q
	spktq_init(&pktpgr->tag_pull_q, PKTQ_LEN_MAX);

	// HWA_COMMON_HWA2HWCAP_HWPPSUPPORT must be set before
	// Dongle packet pool configuration.
	// Enable PP support in HWA 2.2. see also hwa_config
	v32 = HWA_RD_REG_NAME(HWApp, dev->regs, common, hwa2hwcap);
	v32 |= BCM_SBIT(HWA_COMMON_HWA2HWCAP_HWPPSUPPORT);
	HWA_WR_REG_NAME(HWApp, dev->regs, common, hwa2hwcap, v32);

	// Setup Rx Block with RxLfrag databuffer offset in words and to
	// use ext/internal D11Bdest and default Rxblock to PAGER mode
	v32 = HWA_RD_REG_NAME(HWApp, regs, rx_core[0], rxpsrc_ring_hwa2cfg);
	if (HWAREV_GE(dev->corerev, 132)) {
		// If pp_alloc_freerph is set, the alloc_rph request may be fulfilled by taking
		// the rph from free_rph request.
		v32 |= BCM_SBIT(HWA_RX_RXPSRC_RING_HWA2CFG_PP_ALLOC_FREERPH);
	}
	v32 |= BCM_SBF(dev->d11b_axi, HWA_RX_RXPSRC_RING_HWA2CFG_PP_INT_D11B)
	    | BCM_SBIT(HWA_RX_RXPSRC_RING_HWA2CFG_PP_PAGER_MODE);
	HWA_WR_REG_NAME(HWApp, regs, rx_core[0], rxpsrc_ring_hwa2cfg, v32);

	// NOTE: These rx_core registers setting will be reset in rxpath, hwa_pcie.c
	// We need to reconfig it in hwa_pktpgr_init
	v32 = HWA_RD_REG_NAME(HWApp, regs, rx_core[0], pagein_status);
	// Lbuf Context includes 4 FragInfo's Haddr64. In RxPath, only one fraginfo
	// is needed, and the memory of the remaining three are repurposed for
	// databuffer, allowing larger a 152 Byte databuffer.
	v32 = BCM_CBF_REV(v32, HWA_RX_PAGEIN_STATUS_PP_RXLFRAG_DATA_BUF_LEN, dev->corerev)
		| BCM_SBF_REV(HWA_PP_RXLFRAG_DATABUF_LEN_WORDS,
			HWA_RX_PAGEIN_STATUS_PP_RXLFRAG_DATA_BUF_LEN, dev->corerev);
	// PP_RXLFRAG_DATA_BUF_OFFSET is used for HWA to DMA data from MAC.
	// rx::rxfill_ctrl1::d11b_offset is used to program MAC descriptor by HWA
	v32 = BCM_CBF_REV(v32, HWA_RX_PAGEIN_STATUS_PP_RXLFRAG_DATA_BUF_OFFSET, dev->corerev)
		| BCM_SBF_REV(HWA_PP_RXLFRAG_DATABUF_OFFSET_WORDS,
			HWA_RX_PAGEIN_STATUS_PP_RXLFRAG_DATA_BUF_OFFSET, dev->corerev);
	// DMA template. Dongle page and Host page.
	// XXX, revisit ADDREXT_H when having high address in 64bits host.
	v32 = BCM_CBIT(v32, HWA_RX_PAGEIN_STATUS_TEMPLATE_ADDREXT_H);
	v32 = BCM_CBIT(v32, HWA_RX_PAGEIN_STATUS_TEMPLATE_NOTPCIE_H);
	v32 = BCM_CBIT(v32, HWA_RX_PAGEIN_STATUS_TEMPLATE_COHERENT_H);
	v32 = BCM_CBIT(v32, HWA_RX_PAGEIN_STATUS_TEMPLATE_ADDREXT_D);
	v32 = v32
		| BCM_SBIT(HWA_RX_PAGEIN_STATUS_TEMPLATE_NOTPCIE_D)
		| BCM_SBIT(HWA_RX_PAGEIN_STATUS_TEMPLATE_COHERENT_D)
		| 0U;
	HWA_WR_REG_NAME(HWApp, regs, rx_core[0], pagein_status, v32);

	v32 = BCM_SBF(HWA_PKTPGR_D11BDEST_TH0,
	              HWA_RX_D11BDEST_THRESHOLD_L1L0_PP_D11THRESHOLD_L0)
	    | BCM_SBF(HWA_PKTPGR_D11BDEST_TH1,
	              HWA_RX_D11BDEST_THRESHOLD_L1L0_PP_D11THRESHOLD_L1);
	HWA_WR_REG_NAME(HWApp, regs, rx_core[0], d11bdest_threshold_l1l0, v32);
	v32 = BCM_SBF(HWA_PKTPGR_D11BDEST_TH2,
	              HWA_RX_D11BDEST_THRESHOLD_L2_PP_D11THRESHOLD_L2);
	HWA_WR_REG_NAME(HWApp, regs, rx_core[0], d11bdest_threshold_l2, v32);

	// Turn on common::module_enable bit packet pager: see hwa_enable()

	// Describe Lbuf::Context and Lbuf::Databuffer size
	v32 = BCM_SBF(HWA_PP_PKTCONTEXT_BYTES,
	              HWA_PAGER_PP_PKTCTX_SIZE_PPPKTCTXSIZE);
	HWA_WR_REG_NAME(HWApp, regs, pager, pp_pktctx_size, v32);
	v32 = BCM_SBF(HWA_PP_PKTDATABUF_BYTES,
	              HWA_PAGER_PP_PKTBUF_SIZE_PPPKTBUFSIZE);
	HWA_WR_REG_NAME(HWApp, regs, pager, pp_pktbuf_size, v32);

	// Configure the DMA Descriptor template
	v32 = 0U
	    // All interface's ring memory is coherent and NotPCIe
	    | BCM_SBIT(HWA_PAGER_PP_DMA_DESCR_TEMPLATE_PPINREQNOTPCIE)
		| BCM_SBIT(HWA_PAGER_PP_DMA_DESCR_TEMPLATE_PPINREQCOHERENT)
		| BCM_SBIT(HWA_PAGER_PP_DMA_DESCR_TEMPLATE_PPINRSPNOTPCIE)
		| BCM_SBIT(HWA_PAGER_PP_DMA_DESCR_TEMPLATE_PPINRSPCOHERENT)
		| BCM_SBIT(HWA_PAGER_PP_DMA_DESCR_TEMPLATE_PPOUTREQNOTPCIE)
		| BCM_SBIT(HWA_PAGER_PP_DMA_DESCR_TEMPLATE_PPOUTREQCOHERENT)
		| BCM_SBIT(HWA_PAGER_PP_DMA_DESCR_TEMPLATE_PPOUTRSPNOTPCIE)
		| BCM_SBIT(HWA_PAGER_PP_DMA_DESCR_TEMPLATE_PPOUTRSPCOHERENT)
		| BCM_SBIT(HWA_PAGER_PP_DMA_DESCR_TEMPLATE_PPALLOCREQNOTPCIE)
		| BCM_SBIT(HWA_PAGER_PP_DMA_DESCR_TEMPLATE_PPALLOCREQCOHERENT)
		| BCM_SBIT(HWA_PAGER_PP_DMA_DESCR_TEMPLATE_PPALLOCRSPNOTPCIE)
		| BCM_SBIT(HWA_PAGER_PP_DMA_DESCR_TEMPLATE_PPALLOCRSPCOHERENT)
		| BCM_SBIT(HWA_PAGER_PP_DMA_DESCR_TEMPLATE_PPFREEREQNOTPCIE)
		| BCM_SBIT(HWA_PAGER_PP_DMA_DESCR_TEMPLATE_PPFREEREQCOHERENT)
		| BCM_SBIT(HWA_PAGER_PP_DMA_DESCR_TEMPLATE_PPFREERPHREQNOTPCIE)
		| BCM_SBIT(HWA_PAGER_PP_DMA_DESCR_TEMPLATE_PPFREERPHREQCOHERENT)
		// PPAPKTNOTPCIE: bit for PAGER to DMA data to donglepool address
		| BCM_SBIT(HWA_PAGER_PP_DMA_DESCR_TEMPLATE_PPAPKTNOTPCIE)
		| BCM_SBIT(HWA_PAGER_PP_DMA_DESCR_TEMPLATE_PPAPKTCOHERENT)
		// PPRXAPKTNOTPCIE: bit for PAGER to DMA data to donglepool address
		| BCM_SBIT(HWA_PAGER_PP_DMA_DESCR_TEMPLATE_PPRXAPKTNOTPCIE)
		| BCM_SBIT(HWA_PAGER_PP_DMA_DESCR_TEMPLATE_PPRXAPKTCOHERENT)
		// PPPUSHSANOTPCIE: bit for PAGER to DMA data from SA:Dongle to dest address
		| BCM_SBIT(HWA_PAGER_PP_DMA_DESCR_TEMPLATE_PPPUSHSANOTPCIE)
		| BCM_SBIT(HWA_PAGER_PP_DMA_DESCR_TEMPLATE_PPPUSHSACOHERENT)
		// PPPUSHDANOTPCIE: bit for PAGER to DMA data from src to DA:Host address
		//| BCM_CBIT(HWA_PAGER_PP_DMA_DESCR_TEMPLATE_PPPUSHDANOTPCIE)
		//| BCM_CBIT(HWA_PAGER_PP_DMA_DESCR_TEMPLATE_PPPUSHDACOHERENT)
		// XXX clear it for now
		//| BCM_CBIT(HWA_PAGER_PP_DMA_DESCR_TEMPLATE_PPPUSHDAWCPDESC)
		// PPPULLSANOTPCIE: bit for PAGER to DMA data from SA:Host to dest address
		//| BCM_CBIT(HWA_PAGER_PP_DMA_DESCR_TEMPLATE_PPPULLSANOTPCIE)
		//| BCM_CBIT(HWA_PAGER_PP_DMA_DESCR_TEMPLATE_PPPULLSACOHERENT)
		// PPPULLDANOTPCIE: bit for PAGER to DMA data from src to DA:Dongle address
		| BCM_SBIT(HWA_PAGER_PP_DMA_DESCR_TEMPLATE_PPPULLDANOTPCIE)
		| BCM_SBIT(HWA_PAGER_PP_DMA_DESCR_TEMPLATE_PPPULLDACOHERENT)
		// XXX clear it for now
		//| BCM_CBIT(HWA_PAGER_PP_DMA_DESCR_TEMPLATE_PPPULLDAWCPDESC)
		| 0U;
	HWA_WR_REG_NAME(HWApp, regs, pager, pp_dma_descr_template, v32);

	// HDBM/DDBM threshold
	// XXX, CRBCAHWA-652, PageIn stuck forever when we hit it in A0.
	v32 = BCM_SBF((HWA_PKTPGR_DNGLPKTPOOL_TH1 / 2),
			HWA_PAGER_PP_PAGEIN_REQ_DDBMTH_PAGEIN_REQ_DDBMTH)
		| BCM_SBF((HWA_PKTPGR_HOSTPKTPOOL_TH1 / 2),
			HWA_PAGER_PP_PAGEIN_REQ_DDBMTH_PAGEIN_REQ_HDBMTH);
	HWA_WR_REG_NAME(HWApp, regs, pager, pp_pagein_req_ddbmth, v32);

	// WDMA is for "dest" SysMem, RDMA is for "src" DDR
	v32 = HWA_RD_REG_NAME(HWApp, regs, txdma, pp_pagein_cfg);
	v32 = BCM_CBIT(v32, HWA_TXDMA_PP_PAGEIN_CFG_PP_PAGEIN_RDMA_COHERENT);
	v32 = BCM_CBIT(v32, HWA_TXDMA_PP_PAGEIN_CFG_PP_PAGEIN_RDMA_NOTPCIE);
	// Not sure if we can turn it on
	v32 = BCM_CBIT(v32, HWA_TXDMA_PP_PAGEIN_CFG_PP_PAGEIN_WDMA_WCPDESC);
	v32 = v32
	    | BCM_SBIT(HWA_TXDMA_PP_PAGEIN_CFG_PP_PAGEIN_WDMA_COHERENT)
	    | BCM_SBIT(HWA_TXDMA_PP_PAGEIN_CFG_PP_PAGEIN_WDMA_NOTPCIE)
	    | 0U;
	HWA_WR_REG_NAME(HWApp, regs, txdma, pp_pagein_cfg, v32);

	// WDMA is for "dest" DDR, RDMA is for "src" SysMEM
	v32 = HWA_RD_REG_NAME(HWApp, regs, txdma, pp_pageout_cfg);
	//Not sure if we can turn it on
	v32 = BCM_CBIT(v32, HWA_TXDMA_PP_PAGEOUT_CFG_PP_PAGEOUT_WDMA_WCPDESC);
	v32 = BCM_CBIT(v32, HWA_TXDMA_PP_PAGEOUT_CFG_PP_PAGEOUT_WDMA_COHERENT);
	v32 = BCM_CBIT(v32, HWA_TXDMA_PP_PAGEOUT_CFG_PP_PAGEOUT_WDMA_NOTPCIE);
	v32 = v32
	    | BCM_SBIT(HWA_TXDMA_PP_PAGEOUT_CFG_PP_PAGEOUT_RDMA_COHERENT)
	    | BCM_SBIT(HWA_TXDMA_PP_PAGEOUT_CFG_PP_PAGEOUT_RDMA_NOTPCIE)
	    | 0U;
	v32 = BCM_CBF(v32, HWA_TXDMA_PP_PAGEOUT_CFG_PP_HDBM_AVAILCNT)
		| BCM_SBF(HWA_PKTPGR_HDBM_AVAILCNT,
			HWA_TXDMA_PP_PAGEOUT_CFG_PP_HDBM_AVAILCNT);
	v32 = BCM_CBF(v32, HWA_TXDMA_PP_PAGEOUT_CFG_PP_MACFIFO_EMPTY_CNT)
		| BCM_SBF(HWA_PKTPGR_MACFIFO_EMPTY_CNT,
			HWA_TXDMA_PP_PAGEOUT_CFG_PP_MACFIFO_EMPTY_CNT);
	v32 = BCM_CBF(v32, HWA_TXDMA_PP_PAGEOUT_CFG_PP_MACAQM_EMPTY_CNT)
		| BCM_SBF(HWA_PKTPGR_MACAQM_EMPTY_CNT,
			HWA_TXDMA_PP_PAGEOUT_CFG_PP_MACAQM_EMPTY_CNT);
	HWA_WR_REG_NAME(HWApp, regs, txdma, pp_pageout_cfg, v32);

	// Configure Dongle Packet Pool
	v32 = HWA_PTR2UINT(pktpgr->dnglpktpool_mem);
	HWA_WR_REG_ADDR(HWApp, &pager_regs->dnglpktpool.addr.lo, v32);
	HWA_WR_REG_ADDR(HWApp, &pager_regs->dnglpktpool.addr.hi, 0U);
	HWA_WR_REG_ADDR(HWApp, &pager_regs->dnglpktpool.size,
		pktpgr->dnglpktpool_max);
	v32 = (0U
		| BCM_SBF(HWA_PKTPGR_DNGLPKTPOOL_TH1,
		          HWA_PAGER_PP_DNGLPKTPOOL_INTR_TH_PPDDTH1)
		| BCM_SBF(HWA_PKTPGR_DNGLPKTPOOL_TH2,
		          HWA_PAGER_PP_DNGLPKTPOOL_INTR_TH_PPDDTH2));
	HWA_WR_REG_ADDR(HWApp, &pager_regs->dnglpktpool.intr_th, v32);
	v32 = BCM_SBIT(HWA_PAGER_PP_DNGLPKTPOOL_CTRL_PPDDENABLE);
	HWA_WR_REG_ADDR(HWApp, &pager_regs->dnglpktpool.ctrl, v32);

	// Host Packet Pool configured in hwa_config - after dhd::dongle handshake

	// Attach all Packet Pager Interfaces
	ring = 0;

	ring_memory = memory[ring++]; // pagein_req_ring + PPInRxProcessBE
	HWA_ERROR(("%s pagein_req +memory[%p,%u]\n", HWApp, ring_memory, mem_sz));
	pktpgr->req_ring[hwa_pktpgr_pagein_req_ring] =
		hwa_pktpgr_ring_init(&pktpgr->pagein_req_ring, "PI>",
			HWA_RING_S2H, HWA_PKTPGR_PAGEIN_S2H_RINGNUM,
			depth, ring_memory, regs, &pager_regs->pagein_req_ring);
	pktpgr->req_ring_reg[hwa_pktpgr_pagein_req_ring] = &pager_regs->pagein_req_ring;

	ring_memory = memory[ring++]; // pagein_rsp_ring
	HWA_ERROR(("%s pagein_rsp +memory[%p,%u]\n", HWApp, ring_memory, mem_sz));
	pktpgr->rsp_ring[hwa_pktpgr_pagein_rsp_ring] =
		hwa_pktpgr_ring_init(&pktpgr->pagein_rsp_ring, "PI<",
			HWA_RING_H2S, HWA_PKTPGR_PAGEIN_H2S_RINGNUM,
			depth, ring_memory, regs, &pager_regs->pagein_rsp_ring);

	ring_memory = memory[ring++]; // pageout_req_ring
	HWA_ERROR(("%s pageout_req +memory[%p,%u]\n", HWApp, ring_memory, mem_sz));
	pktpgr->req_ring[hwa_pktpgr_pageout_req_ring] =
		hwa_pktpgr_ring_init(&pktpgr->pageout_req_ring, "PO>",
			HWA_RING_S2H, HWA_PKTPGR_PAGEOUT_S2H_RINGNUM,
			depth, ring_memory, regs, &pager_regs->pageout_req_ring);
	pktpgr->req_ring_reg[hwa_pktpgr_pageout_req_ring] = &pager_regs->pageout_req_ring;

	ring_memory = memory[ring++]; // pageout_rsp_ring
	HWA_ERROR(("%s pageout_rsp +memory[%p,%u]\n", HWApp, ring_memory, mem_sz));
	pktpgr->rsp_ring[hwa_pktpgr_pageout_rsp_ring] =
		hwa_pktpgr_ring_init(&pktpgr->pageout_rsp_ring, "PO<",
			HWA_RING_H2S, HWA_PKTPGR_PAGEOUT_H2S_RINGNUM,
			depth, ring_memory, regs, &pager_regs->pageout_rsp_ring);

	ring_memory = memory[ring++]; // pagemgr_req_ring
	HWA_ERROR(("%s pagemgr_req +memory[%p,%u]\n", HWApp, ring_memory, mem_sz));
	pktpgr->req_ring[hwa_pktpgr_pagemgr_req_ring] =
		hwa_pktpgr_ring_init(&pktpgr->pagemgr_req_ring, "PM>",
			HWA_RING_S2H, HWA_PKTPGR_PAGEMGR_S2H_RINGNUM,
			depth, ring_memory, regs, &pager_regs->pagemgr_req_ring);
	pktpgr->req_ring_reg[hwa_pktpgr_pagemgr_req_ring] = &pager_regs->pagemgr_req_ring;

	ring_memory = memory[ring++]; // pagemgr_rsp_ring
	HWA_ERROR(("%s pagemgr_rsp +memory[%p,%u]\n", HWApp, ring_memory, mem_sz));
	pktpgr->rsp_ring[hwa_pktpgr_pagemgr_rsp_ring] =
		hwa_pktpgr_ring_init(&pktpgr->pagemgr_rsp_ring, "PM<",
			HWA_RING_H2S, HWA_PKTPGR_PAGEMGR_H2S_RINGNUM,
			depth, ring_memory, regs, &pager_regs->pagemgr_rsp_ring);

	ring_memory = memory[ring++]; // freepkt_req_ring
	HWA_ERROR(("%s freepkt_req +memory[%p,%u]\n", HWApp, ring_memory, mem_sz));
	pktpgr->req_ring[hwa_pktpgr_freepkt_req_ring] =
		hwa_pktpgr_ring_init(&pktpgr->freepkt_req_ring, "PKT",
			HWA_RING_S2H, HWA_PKTPGR_FREEPKT_S2H_RINGNUM,
			depth, ring_memory, regs, &pager_regs->freepkt_req_ring);
	pktpgr->req_ring_reg[hwa_pktpgr_freepkt_req_ring] = &pager_regs->freepkt_req_ring;

	ring_memory = memory[ring++]; // freerph_req_ring
	HWA_ERROR(("%s freerph_req +memory[%p,%u]\n", HWApp, ring_memory, mem_sz));
	pktpgr->req_ring[hwa_pktpgr_freerph_req_ring] =
		hwa_pktpgr_ring_init(&pktpgr->freerph_req_ring, "RPH",
			HWA_RING_S2H, HWA_PKTPGR_FREERPH_S2H_RINGNUM,
			depth, ring_memory, regs, &pager_regs->freerph_req_ring);
	pktpgr->req_ring_reg[hwa_pktpgr_freerph_req_ring] = &pager_regs->freerph_req_ring;

	HWA_ASSERT(ring == hwa_pktpgr_ring_max); // done all rings

#ifdef PSPL_TX_TEST
	// Initial SW PUSH/PULL circular ring
	HWA_ERROR(("%s pspl req info ring +memory[%p,%u]\n", HWApp, pspl_table, pspl_table_sz));
	bcm_ring_init(&pktpgr->pspl_state);
	pktpgr->pspl_table = pspl_table;
	pktpgr->pspl_depth = HWA_PKTPGR_PSPL_DEPTH;
#endif /* PSPL_TX_TEST */

	// Register process item callback for the PKTPGR H2S interfaces.
	hwa_register(dev, HWA_PKTPGR_PAGEIN_RXPROCESS, dev,
	             (hwa_callback_fn_t) hwa_pktpgr_pagein_rxprocess);
	hwa_register(dev, HWA_PKTPGR_PAGEIN_TXSTATUS, dev,
	             (hwa_callback_fn_t) hwa_pktpgr_pagein_txstatus);
	hwa_register(dev, HWA_PKTPGR_PAGEIN_TXPOST, dev,
	             (hwa_callback_fn_t) hwa_pktpgr_pagein_txpost);
	hwa_register(dev, HWA_PKTPGR_PAGEIN_TXPOST_FRC, dev,
	             (hwa_callback_fn_t) hwa_pktpgr_pagein_txpost);
	hwa_register(dev, HWA_PKTPGR_PAGEOUT_PKTLIST, dev,
	             (hwa_callback_fn_t) hwa_pktpgr_pageout_pktlist);
	hwa_register(dev, HWA_PKTPGR_PAGEOUT_LOCAL, dev,
	             (hwa_callback_fn_t) hwa_pktpgr_pageout_local);
	hwa_register(dev, HWA_PKTPGR_PAGEMGR_ALLOC_RX, dev,
	             (hwa_callback_fn_t) hwa_pktpgr_pagemgr_alloc_rx);
	hwa_register(dev, HWA_PKTPGR_PAGEMGR_ALLOC_RX_RPH, dev,
	             (hwa_callback_fn_t) hwa_pktpgr_pagemgr_alloc_rx_rph);
	hwa_register(dev, HWA_PKTPGR_PAGEMGR_ALLOC_TX, dev,
	             (hwa_callback_fn_t) hwa_pktpgr_pagemgr_alloc_tx);
	hwa_register(dev, HWA_PKTPGR_PAGEMGR_PUSH, dev,
	             (hwa_callback_fn_t) hwa_pktpgr_pagemgr_push);
	hwa_register(dev, HWA_PKTPGR_PAGEMGR_PULL, dev,
	             (hwa_callback_fn_t) hwa_pktpgr_pagemgr_pull);
	hwa_register(dev, HWA_PKTPGR_PAGEMGR_PUSH_PKTTAG, dev,
	             (hwa_callback_fn_t) hwa_pktpgr_pagemgr_push);
	hwa_register(dev, HWA_PKTPGR_PAGEMGR_PULL_KPFL_LINK, dev,
	             (hwa_callback_fn_t) hwa_pktpgr_pagemgr_pull);

	return pktpgr;

ring_failure:
	for (ring = 0; ring < hwa_pktpgr_ring_max; ++ring) {
		if (memory[ring] == NULL)
			break;
		MFREE(dev->osh, memory[ring], mem_sz);
	}

#ifdef PSPL_TX_TEST
	if (pspl_table)
		MFREE(dev->osh, pspl_table, pspl_table_sz);
#endif // endif

lbuf_pool_failure:
	hwa_pktpgr_detach(pktpgr);
	HWA_WARN(("%s attach failure\n", HWApp));

	return (hwa_pktpgr_t*) NULL;

}   // hwa_pktpgr_attach()

void // HWApp: Cleanup/Free resources used by PktPgr block
BCMATTACHFN(hwa_pktpgr_detach)(hwa_pktpgr_t *pktpgr)
{
	hwa_dev_t *dev;

	HWA_FTRACE(HWApp);

	if (pktpgr == (hwa_pktpgr_t*)NULL)
		return;

	// Audit pre-conditions
	dev = HWA_DEV(pktpgr);

	if (pktpgr->dnglpktpool_mem != (hwa_pp_lbuf_t*)NULL) {
		void * memory = (void*)pktpgr->dnglpktpool_mem;
		uint32 mem_sz = pktpgr->dnglpktpool_max * HWA_PP_LBUF_SZ;
		HWA_TRACE(("%s lbuf pool items<%u> mem_size<%u> free\n",
			HWApp, HWA_DNGL_PKTS_MAX, mem_sz));
		MFREE(dev->osh, memory, mem_sz);
		pktpgr->dnglpktpool_mem = (hwa_pp_lbuf_t*)NULL;
		pktpgr->dnglpktpool_max = 0;
	}

#ifdef PSPL_TX_TEST
	if (pktpgr->pspl_table) {
		MFREE(dev->osh, pktpgr->pspl_table,
			HWA_PKTPGR_PSPL_DEPTH * sizeof(hwa_pp_pspl_req_info_t));
		pktpgr->pspl_table = NULL;
	}
#endif // endif
}   // hwa_pktpgr_detach()

void // HWApp: Init PktPgr block AFTER DHD handshake pcie_ipc initialized.
hwa_pktpgr_preinit(hwa_pktpgr_t *pktpgr)
{
	uint32 v32;
	hwa_dev_t *dev;
	hwa_pager_regs_t *pager_regs;

	HWA_FTRACE(HWApp);

	// Audit pre-conditions
	dev = HWA_DEV(pktpgr);
	HWA_ASSERT(dev->pcie_ipc != (pcie_ipc_t*)NULL);

	pager_regs = &dev->regs->pager; // Setup locals

	// Allocate Host Packet Pool
	pktpgr->hostpktpool_haddr64 = hme_get(HME_USER_PKTPGR,
		(HWA_HOST_PKTS_MAX * HWA_PP_LBUF_SZ));
	if (HADDR64_IS_ZERO(pktpgr->hostpktpool_haddr64)) {
		HWA_ERROR(("Allocate Host Packet Pool size %d failed\n",
			(HWA_HOST_PKTS_MAX * HWA_PP_LBUF_SZ)));
		HWA_ASSERT(0);
	}
	pktpgr->hostpktpool_max = HWA_HOST_PKTS_MAX;

	v32 = HADDR64_LO(pktpgr->hostpktpool_haddr64);
	HWA_WR_REG_ADDR(HWApp, &pager_regs->hostpktpool.addr.lo, v32);
	v32 = HWA_HOSTADDR64_HI32(HADDR64_HI(pktpgr->hostpktpool_haddr64));
	HWA_WR_REG_ADDR(HWApp, &pager_regs->hostpktpool.addr.hi, v32);
	HWA_WR_REG_ADDR(HWApp, &pager_regs->hostpktpool.size,
		pktpgr->hostpktpool_max);
	v32 = (0U
		| BCM_SBF(HWA_PKTPGR_HOSTPKTPOOL_TH1,
		          HWA_PAGER_PP_HOSTPKTPOOL_INTR_TH_PPHDTH1)
		| BCM_SBF(HWA_PKTPGR_HOSTPKTPOOL_TH2,
		          HWA_PAGER_PP_HOSTPKTPOOL_INTR_TH_PPHDTH2));
	HWA_WR_REG_ADDR(HWApp, &pager_regs->hostpktpool.intr_th, v32);
	v32 = BCM_SBIT(HWA_PAGER_PP_HOSTPKTPOOL_CTRL_PPHDENABLE);
	HWA_WR_REG_ADDR(HWApp, &pager_regs->hostpktpool.ctrl, v32);

	// Enable Packet Pager
	v32 = BCM_SBIT(HWA_PAGER_PP_PAGER_CFG_PAGER_EN);
	HWA_WR_REG_NAME(HWApp, dev->regs, pager, pp_pager_cfg, v32);

}   // hwa_pktpgr_preinit()

void
hwa_pktpgr_init(hwa_pktpgr_t *pktpgr)
{
	uint32 v32;
	uint32 d11b_offset, d11b_length;
	hwa_regs_t *regs;
	hwa_dev_t *dev;

	HWA_FTRACE(HWApp);

	// Audit pre-conditions
	dev = HWA_DEV(pktpgr);

	// Setup locals
	regs = dev->regs;

	// d11b_length is used to program the MAC descriptor.
	d11b_offset = dev->rxfill.config.wrxh_offset +
		dev->rxfill.config.d11_offset;
	d11b_length = HWA_PP_RXLFRAG_DATABUF_LEN - d11b_offset;
	// d11b_offset is start from base address of rxlfrag.
	d11b_offset += HWA_PP_RXLFRAG_DATABUF_OFFSET;
	HWA_ASSERT((ROUNDUP(d11b_length, 4) == d11b_length));
	HWA_ASSERT((ROUNDUP(d11b_offset, 4) == d11b_offset));

	// NOTE: These rx_core registers setting will be reset in rxpath, hwa_pcie.c
	// We need to reconfig it in hwa_pktpgr_init
	v32 = HWA_RD_REG_NAME(HWApp, regs, rx_core[0], pagein_status);
	// Lbuf Context includes 4 FragInfo's Haddr64. In RxPath, only one fraginfo
	// is needed, and the memory of the remaining three are repurposed for
	// databuffer, allowing larger a 152 Byte databuffer.
	// Set the length same as RXFILL_CTRL1.
	v32 = BCM_CBF_REV(v32, HWA_RX_PAGEIN_STATUS_PP_RXLFRAG_DATA_BUF_LEN, dev->corerev)
		| BCM_SBF_REV((d11b_length / NBU32),
			HWA_RX_PAGEIN_STATUS_PP_RXLFRAG_DATA_BUF_LEN, dev->corerev);
	// PP_RXLFRAG_DATA_BUF_OFFSET is used for HWA to DMA data from MAC.
	// rx::rxfill_ctrl1::d11b_offset is used to program MAC descriptor by HWA
	// Set the offset same as RXFILL_CTRL1.
	v32 = BCM_CBF_REV(v32, HWA_RX_PAGEIN_STATUS_PP_RXLFRAG_DATA_BUF_OFFSET, dev->corerev)
		| BCM_SBF_REV((d11b_offset / NBU32),
			HWA_RX_PAGEIN_STATUS_PP_RXLFRAG_DATA_BUF_OFFSET, dev->corerev);
	// DMA template. Dongle page and Host page.
	// XXX, revisit ADDREXT_H when having high address in 64bits host.
	v32 = BCM_CBIT(v32, HWA_RX_PAGEIN_STATUS_TEMPLATE_ADDREXT_H);
	v32 = BCM_CBIT(v32, HWA_RX_PAGEIN_STATUS_TEMPLATE_NOTPCIE_H);
	v32 = BCM_CBIT(v32, HWA_RX_PAGEIN_STATUS_TEMPLATE_COHERENT_H);
	v32 = BCM_CBIT(v32, HWA_RX_PAGEIN_STATUS_TEMPLATE_ADDREXT_D);
	v32 = v32
		| BCM_SBIT(HWA_RX_PAGEIN_STATUS_TEMPLATE_NOTPCIE_D)
		| BCM_SBIT(HWA_RX_PAGEIN_STATUS_TEMPLATE_COHERENT_D)
		| 0U;
	HWA_WR_REG_NAME(HWApp, regs, rx_core[0], pagein_status, v32);

	v32 = BCM_SBF(HWA_PKTPGR_D11BDEST_TH0,
	              HWA_RX_D11BDEST_THRESHOLD_L1L0_PP_D11THRESHOLD_L0)
	    | BCM_SBF(HWA_PKTPGR_D11BDEST_TH1,
	              HWA_RX_D11BDEST_THRESHOLD_L1L0_PP_D11THRESHOLD_L1);
	HWA_WR_REG_NAME(HWApp, regs, rx_core[0], d11bdest_threshold_l1l0, v32);
	v32 = BCM_SBF(HWA_PKTPGR_D11BDEST_TH2,
	              HWA_RX_D11BDEST_THRESHOLD_L2_PP_D11THRESHOLD_L2);
	HWA_WR_REG_NAME(HWApp, regs, rx_core[0], d11bdest_threshold_l2, v32);

	// Clear PageOut req ring stop bit which may set in hwa_txfifo_disable_prep
	v32 = HWA_RD_REG_ADDR(HWApp, &regs->pager.pageout_req_ring.cfg);
	v32 = BCM_CBIT(v32, HWA_PAGER_PP_PAGEOUT_REQ_RING_CFG_PPOUTREQSTOP);
	HWA_WR_REG_ADDR(HWApp, &regs->pager.pageout_req_ring.cfg, v32);
	HWA_ERROR(("%s: Start PageOut Req\n", __FUNCTION__));

	// pktpgr::pagein_intstatus are read only for
	// second level error handling.  The mask setting is
	// merged in HWA_PKTPGR_INT_MASK
	// NOTE: We don't turn on ERROR bits in common::pageintstatus

	// First level IntStatus: common::pageintstatus
	dev->pageintmask = HWA_PKTPGR_INT_MASK;
	HWA_WR_REG_NAME(HWApp, dev->regs, common, pageintmask, dev->pageintmask);

	// Top level IntStatus: common::intstatus
	dev->defintmask |= HWA_COMMON_INTSTATUS_PACKET_PAGER_INTR_MASK;

}   // hwa_pktpgr_init()

void // HWApp: Deinit PktPgr block
hwa_pktpgr_deinit(hwa_pktpgr_t *pktpgr)
{
}   // hwa_pktpgr_deinit()

/**
 * Single debug interface to read all registers carrying packet pager "status"
 * Uses a wrapper _hwa_pktpgr_ring_status() to dump S2H and H2S interface.
 */
static void _hwa_pktpgr_ring_status(const char *ring_name,
                                    hwa_pp_ring_t *hwa_pp_ring_regs);
static void
_hwa_pktpgr_ring_status(const char *ring_name, hwa_pp_ring_t *hwa_pp_ring_regs)
{
	uint32 v32, wr_index, rd_index;
	wr_index = HWA_RD_REG_ADDR(HWApp, &hwa_pp_ring_regs->wr_index);
	rd_index = HWA_RD_REG_ADDR(HWApp, &hwa_pp_ring_regs->rd_index);
	if (wr_index != rd_index)
		HWA_PRINT("\t %s [wr,rd] = [%u, %u]\n", ring_name, wr_index, rd_index);
	v32 = HWA_RD_REG_ADDR(HWApp, &hwa_pp_ring_regs->debug);
	if (v32) HWA_PRINT("\t %s debug<0x%08x>\n", ring_name, v32);

}   // _hwa_pktpgr_ring_status()

void // HWApp: Query various interfaces and module for status and errors
hwa_pktpgr_status(hwa_dev_t *dev)
{
	uint32 v32;
	hwa_regs_t *regs;
	hwa_pager_regs_t *pager_regs;

	HWA_AUDIT_DEV(dev);
	regs = dev->regs;
	pager_regs = &regs->pager;

	// Ring debug status or processing stalled
	HWA_PRINT("%s Ring Status\n", HWApp);
	_hwa_pktpgr_ring_status("pagein_req ", &pager_regs->pagein_req_ring);
	_hwa_pktpgr_ring_status("pagein_rsp ", &pager_regs->pagein_rsp_ring);
	_hwa_pktpgr_ring_status("pageout_req", &pager_regs->pageout_req_ring);
	_hwa_pktpgr_ring_status("pageout_rsp", &pager_regs->pageout_rsp_ring);
	_hwa_pktpgr_ring_status("pagemgr_req", &pager_regs->pagemgr_req_ring);
	_hwa_pktpgr_ring_status("pagemgr_rsp", &pager_regs->pagemgr_rsp_ring);
	_hwa_pktpgr_ring_status("freepkt_req", &pager_regs->freepkt_req_ring);
	_hwa_pktpgr_ring_status("freerph_req", &pager_regs->freerph_req_ring);

	// Errors reported via Instatus
	HWA_PRINT("%s Ring Errors\n", HWApp);
	v32 = HWA_RD_REG_ADDR(HWApp, &pager_regs->pagein_intstatus)
	    & HWA_PAGER_PAGEIN_ERRORS_MASK;
	if (v32) HWA_PRINT("\t pagein_errors<0x%08x>\n", v32);
	v32 = HWA_RD_REG_ADDR(HWApp, &pager_regs->pageout_intstatus)
	    & HWA_PAGER_PAGEOUT_ERRORS_MASK;
	if (v32) HWA_PRINT("\t pageout_errors<0x%08x>\n", v32);
	v32 = HWA_RD_REG_ADDR(HWApp, &pager_regs->pagemgr_intstatus)
	    & HWA_PAGER_PAGEMGR_ERRORS_MASK;
	if (v32) HWA_PRINT("\t pagemgr_errors<0x%08x>\n", v32);
	v32 = HWA_RD_REG_ADDR(HWApp, &pager_regs->pagerbm_intstatus)
		& HWA_PAGER_PAGERBM_ERRORS_MASK;
	if (v32) HWA_PRINT("\t pagerbm_errors<0x%08x>\n", v32);

	HWA_PRINT("%s Transaction Id\n", HWApp);
	v32 = HWA_RD_REG_NAME(HWApp, regs, pager, rx_alloc_transaction_id);
	HWA_PRINT("\t rx_alloc<0x%08x>\n", v32);
	v32 = HWA_RD_REG_NAME(HWApp, regs, pager, rx_free_transaction_id);
	HWA_PRINT("\t rx_free<0x%08x>\n", v32);
	v32 = HWA_RD_REG_NAME(HWApp, regs, pager, tx_alloc_transaction_id);
	HWA_PRINT("\t tx_alloc<0x%08x>\n", v32);
	v32 = HWA_RD_REG_NAME(HWApp, regs, pager, tx_free_transaction_id);
	HWA_PRINT("\t tx_free<0x%08x>\n", v32);

	HWA_PRINT("%s Module Debug\n", HWApp);
	v32 = HWA_RD_REG_NAME(HWApp, regs, pager, pp_apkt_sts_dbg);
	if (v32) HWA_PRINT("\t alloc<0x%08x>\n", v32);
	v32 = HWA_RD_REG_NAME(HWApp, regs, pager, pp_rx_apkt_sts_dbg);
	if (v32) HWA_PRINT("\t rx_alloc<0x%08x>\n", v32);
	v32 = HWA_RD_REG_NAME(HWApp, regs, pager, pp_fpkt_sts_dbg);
	if (v32) HWA_PRINT("\t free<0x%08x>\n", v32);
	v32 = HWA_RD_REG_NAME(HWApp, regs, pager, pp_tb_sts_dbg);
	if (v32) HWA_PRINT("\t table<0x%08x>\n", v32);
	v32 = HWA_RD_REG_NAME(HWApp, regs, pager, pp_push_sts_dbg);
	if (v32) HWA_PRINT("\t push<0x%08x>\n", v32);
	v32 = HWA_RD_REG_NAME(HWApp, regs, pager, pp_pull_sts_dbg);
	if (v32) HWA_PRINT("\t pull<0x%08x>\n", v32);

	HWA_TXFIFO_EXPR({
		HWA_PRINT("%s Tx Status\n", HWApp);
		v32 = HWA_RD_REG_NAME(HWApp, regs, txdma, pp_pageout_sts);
		if (v32) HWA_PRINT("\t pp_pageout_sts<0x%08x>\n", v32);
		v32 = HWA_RD_REG_NAME(HWApp, regs, txdma, pp_pagein_sts);
		if (v32) HWA_PRINT("\t pp_pagein_sts<0x%08x>\n", v32);
	});

	HWA_RXFILL_EXPR({
		HWA_PRINT("%s Rx Status\n", HWApp);
		v32 = HWA_RD_REG_NAME(HWApp, regs, rx_core[0], pagein_status);
		if (v32) HWA_PRINT("\t pagein_status<0x%08x>\n", v32);
		v32 = HWA_RD_REG_NAME(HWApp, regs, rx_core[0], recycle_status);
		if (v32) HWA_PRINT("\t recycle_status<0x%08x>\n", v32);
		v32 = HWA_RD_REG_NAME(HWApp, regs, rx_core[0], recycle_cfg);
		if (v32) HWA_PRINT("\t recycle_cfg<0x%08x>\n", v32);
	})

}   // hwa_pktpgr_status()

// HWA Packet Pager Debug Support
#if defined(BCMDBG) || defined(HWA_DUMP)
void // Dump all SW and HWA Packet Pager state
hwa_pktpgr_dump(hwa_pktpgr_t *pktpgr, struct bcmstrbuf *b,
                bool verbose, bool dump_regs, uint8 *fifo_bitmap)
{
	hwa_dev_t *dev;

	HWA_BPRINT(b, "pktpgr dump\n");

	if (pktpgr == (hwa_pktpgr_t*)NULL)
		return;

	// Setup local
	dev = HWA_DEV(pktpgr);

	// Rings
	hwa_ring_dump(&pktpgr->pagein_req_ring, b, "+ pagein_req");
	hwa_ring_dump(&pktpgr->pagein_rsp_ring, b, "+ pagein_rsp");
	hwa_ring_dump(&pktpgr->pageout_req_ring, b, "+ pageout_req");
	hwa_ring_dump(&pktpgr->pageout_rsp_ring, b, "+ pageout_rsp");
	hwa_ring_dump(&pktpgr->pagemgr_req_ring, b, "+ pagemgr_req");
	hwa_ring_dump(&pktpgr->pagemgr_rsp_ring, b, "+ pagemgr_rsp");
	hwa_ring_dump(&pktpgr->freepkt_req_ring, b, "+ freepkt_req");
	hwa_ring_dump(&pktpgr->freerph_req_ring, b, "+ freerph_req");

	// Table of Host and Dongle Packet Pool context
	HWA_BPRINT(b, "+ HDBM<0x%x_%x> max<%u> avail<%u>\n",
		HADDR64_HI(pktpgr->hostpktpool_haddr64),
		HADDR64_LO(pktpgr->hostpktpool_haddr64),
		pktpgr->hostpktpool_max,
		hwa_pktpgr_dbm_availcnt(dev, HWA_HDBM));
	HWA_BPRINT(b, "+ DDBM<0x%p> max<%u> avail<%u>\n",
		pktpgr->dnglpktpool_mem, pktpgr->dnglpktpool_max,
		hwa_pktpgr_dbm_availcnt(dev, HWA_DDBM));

#if defined(WLTEST) || defined(HWA_DUMP)
	if (dump_regs)
		hwa_pktpgr_regs_dump(pktpgr, b);
#endif // endif

	if (verbose == TRUE) {
		int idx, count;
		uint32 u32;
		hwa_pp_rxlbuf_pool_t *lbufpool;

		// SW maintain variables.
		lbufpool = &pktpgr->rxlbufpool;
		HWA_BPRINT(b, "+ rxlbufpool: head<0x%p> tail<0x%p> avail<%u> "
			"n_pkts<%u> trans_id<%u>\n", lbufpool->pkt_head,
			lbufpool->pkt_tail, lbufpool->avail, lbufpool->n_pkts,
			lbufpool->trans_id);
		lbufpool = &pktpgr->txlbufpool;
		HWA_BPRINT(b, "+ txlbufpool: head<0x%p> tail<0x%p> avail<%u> "
			"n_pkts<%u> trans_id<%u>\n", lbufpool->pkt_head,
			lbufpool->pkt_tail, lbufpool->avail, lbufpool->n_pkts,
			lbufpool->trans_id);
		HWA_BPRINT(b, "+ tag_pull: head<0x%p> tail<0x%p> req pending<%u> count<%u>\n",
			spktq_peek_head(&pktpgr->tag_pull_q), spktq_peek_tail(&pktpgr->tag_pull_q),
			pktpgr->tag_pull_req, spktq_n_pkts(&pktpgr->tag_pull_q));
		HWA_BPRINT(b, "+ tag_push: req pending<%u>\n", pktpgr->tag_push_req);

		u32 = HWA_RD_REG_NAME(HWA1b, dev->regs, rx_core[0], d11bdest_ring_wrindex_dir);
		HWA_BPRINT(b, "+ rxpkt: ready<%u> req pending<%d> in_trans<%d>\n",
			BCM_GBF(u32, HWA_RX_D11BDEST_RING_WRINDEX_DIR_OCCUPIED_AFTER_MAC),
			pktpgr->pgi_rxpkt_req, pktpgr->pgi_rxpkt_in_trans);
		HWA_BPRINT(b, "+ localpkt: pgo req pending<%d>\n", pktpgr->pgo_local_req);
		HWA_BPRINT(b, "+ pgi_txs: req_tot pending<%d>\n", pktpgr->pgi_txs_req_tot);
		count = 0;
		if (fifo_bitmap) {
			for (idx = 0; idx < HWA_TX_FIFOS_MAX; idx++) {
				if (isset(fifo_bitmap, idx) &&
					isset(dev->txfifo.fifo_enab, idx) &&
					pktpgr->pgi_txs_req[idx] != 0) {
					if (count == 0)
						HWA_BPRINT(b, "         :");
					count++;
					HWA_BPRINT(b, " req%-2d<%d>", idx,
						pktpgr->pgi_txs_req[idx]);
					if ((count % 8) == 0)
						HWA_BPRINT(b, "\n         :");
				}
			}
		}
		HWA_BPRINT(b, "\n");
	}
}   // hwa_pktpgr_dump()

#if defined(WLTEST) || defined(HWA_DUMP)
// Dump HWA Packet Pager registers
void
hwa_pktpgr_regs_dump(hwa_pktpgr_t *pktpgr, struct bcmstrbuf *b)
{
	hwa_dev_t *dev;
	hwa_regs_t *regs;

	if (pktpgr == (hwa_pktpgr_t*)NULL)
		return;

	dev = HWA_DEV(pktpgr);
	regs = dev->regs;

#define HWA_BPR_PP_RING(b, SNAME) \
	({ \
		HWA_BPR_REG(b, pager, SNAME.addr); \
		HWA_BPR_REG(b, pager, SNAME.wr_index); \
		HWA_BPR_REG(b, pager, SNAME.rd_index); \
		HWA_BPR_REG(b, pager, SNAME.cfg); \
		HWA_BPR_REG(b, pager, SNAME.lazyint_cfg); \
		HWA_BPR_REG(b, pager, SNAME.debug); \
	})

// Skip: following registers as reading has side effect.
// alloc_index, dealloc_index, dealloc_status
#define HWA_BPR_PP_PKTPOOL(b, SNAME) \
	({ \
		HWA_BPR_REG(b, pager, SNAME.addr.lo); \
		HWA_BPR_REG(b, pager, SNAME.addr.hi); \
		HWA_BPR_REG(b, pager, SNAME.ctrl); \
		HWA_BPR_REG(b, pager, SNAME.size); \
		HWA_BPR_REG(b, pager, SNAME.intr_th); \
	})

#define HWA_BPR_PKT_INUSE(b, SNAME) \
	({ \
		HWA_BPR_REG(b, pager, SNAME.all); \
		HWA_BPR_REG(b, pager, SNAME.tx); \
		HWA_BPR_REG(b, pager, SNAME.rx); \
	})

	HWA_BPRINT(b, "%s registers[%p] offset[0x%04x]\n",
		HWApp, &regs->pager, (uint32)OFFSETOF(hwa_regs_t, pager));
	HWA_BPR_REG(b, pager, pp_pager_cfg);
	HWA_BPR_REG(b, pager, pp_pktctx_size);
	HWA_BPR_REG(b, pager, pp_pktbuf_size);
	HWA_BPR_PP_RING(b,    pagein_req_ring);
	HWA_BPR_PP_RING(b,    pagein_rsp_ring);
	HWA_BPR_REG(b, pager, pagein_intstatus);
	HWA_BPR_PP_RING(b,    pageout_req_ring);
	HWA_BPR_PP_RING(b,    pageout_rsp_ring);
	HWA_BPR_REG(b, pager, pageout_intstatus);
	HWA_BPR_PP_RING(b,    pagemgr_req_ring);
	HWA_BPR_PP_RING(b,    pagemgr_rsp_ring);
	HWA_BPR_REG(b, pager, pagemgr_intstatus);
	HWA_BPR_PP_RING(b,    freepkt_req_ring);
	HWA_BPR_PP_RING(b,    freerph_req_ring);
	HWA_BPR_REG(b, pager, rx_alloc_transaction_id);
	HWA_BPR_REG(b, pager, rx_free_transaction_id);
	HWA_BPR_REG(b, pager, tx_alloc_transaction_id);
	HWA_BPR_REG(b, pager, tx_free_transaction_id);
	HWA_BPR_PP_PKTPOOL(b, hostpktpool);
	HWA_BPR_PP_PKTPOOL(b, dnglpktpool);
	HWA_BPR_REG(b, pager, pagerbm_intstatus);
	HWA_BPR_REG(b, pager, pp_dma_descr_template);
	HWA_BPR_REG(b, pager, pp_pagein_req_ddbmth);
	HWA_BPR_REG(b, pager, pp_dma_descr_template_2);
	HWA_BPR_REG(b, pager, pp_apkt_cfg);
	HWA_BPR_REG(b, pager, pp_rx_apkt_cfg);
	HWA_BPR_REG(b, pager, pp_fpkt_cfg);
	HWA_BPR_REG(b, pager, pp_phpl_cfg);
	HWA_BPR_REG(b, pager, pp_apkt_sts_dbg);
	HWA_BPR_REG(b, pager, pp_rx_apkt_sts_dbg);
	HWA_BPR_REG(b, pager, pp_fpkt_sts_dbg);
	HWA_BPR_REG(b, pager, pp_tb_sts_dbg);
	HWA_BPR_REG(b, pager, pp_push_sts_dbg);
	HWA_BPR_REG(b, pager, pp_pull_sts_dbg);
	HWA_BPR_PKT_INUSE(b, hostpkt_cnt);
	HWA_BPR_PKT_INUSE(b, dngltpkt_cnt);
	HWA_BPR_PKT_INUSE(b, hostpkt_hwm);
	HWA_BPR_PKT_INUSE(b, dngltpkt_hwm);
	HWA_BPR_REG(b, pager, pp_pagein_req_stats);
	HWA_BPR_REG(b, pager, pp_pageout_req_stats);
	HWA_BPR_REG(b, pager, pp_pagealloc_req_stats);
	HWA_BPR_REG(b, pager, pp_pagefree_req_stats);
	HWA_BPR_REG(b, pager, pp_pagefreerph_req_stats);
}   // hwa_pktpgr_regs_dump()

#endif // endif
#endif /* BCMDBG */

// PUSH packet from DDBM to HDBM, HW will free pkt in DDBM.
static void // Push request
_hwa_pktpgr_push_req(hwa_dev_t *dev, uint32 fifo_idx, uint8 tagged,
	void *pktlist_head, void *pktlist_tail, uint16 pkt_count,
	uint16 mpdu_count, hwa_pp_pagemgr_cmd_t pagemgr_cmd)
{
	hwa_pp_pagemgr_req_push_t req;
	hwa_pktpgr_t *pktpgr;
	int trans_id, req_loop;

	HWA_FTRACE(HWApp);
	HWA_ASSERT(dev != (struct hwa_dev *)NULL);
	HWA_ASSERT(pkt_count != 0);
	HWA_ASSERT((pagemgr_cmd == HWA_PP_PAGEMGR_PUSH) ||
	           (pagemgr_cmd == HWA_PP_PAGEMGR_PUSH_PKTTAG));

	pktpgr = &dev->pktpgr;

#ifdef PSPL_TX_TEST
	if (tagged == HWA_PP_CMD_NOT_TAGGED &&
		bcm_ring_is_full(&pktpgr->pspl_state, pktpgr->pspl_depth)) {
		HWA_ERROR((">>PAGEMGR::REQ PUSH_REQ failure: Ring full\n"));
		HWA_ASSERT(0);
	}
#endif /* PSPL_TX_TEST */

	// PUSH Request
	req.trans        = pagemgr_cmd;
	req.pkt_count    = pkt_count;
	req.pktlist_head = (uint32)pktlist_head;
	req.pktlist_tail = (uint32)pktlist_tail;
	req.tagged       = tagged;

	req_loop = HWA_LOOPCNT;

req_again:
	trans_id = hwa_pktpgr_request(dev, hwa_pktpgr_pagemgr_req_ring, &req);
	if (trans_id == HWA_FAILURE) {
		if (--req_loop > 0) {
			OSL_DELAY(10);
			goto req_again;
		}
		HWA_ERROR((">>PAGEMGR::REQ PUSH_REQ failure: pkts<%d> list[%p(%d) .. %p(%d)]\n",
			pkt_count, pktlist_head, PKTMAPID(pktlist_head), pktlist_tail,
			PKTMAPID(pktlist_tail)));
		HWA_ASSERT(0);
		return;
	}

#ifdef PSPL_TX_TEST
	if (tagged == HWA_PP_CMD_NOT_TAGGED) {
		hwa_pp_pspl_req_info_t *pspl_req_info;
		int elem_ix;

		// TEST: TxPost-->Push-->Pull-->TxFIFO
		HWA_ASSERT(pktpgr->pspl_test_en);
		HWA_ASSERT(pagemgr_cmd == HWA_PP_PAGEMGR_PUSH);

		// Qeue fifo_idx and pkt_mapid for PUSH rsp.
		// XXX, Do we support > 8-in-1 AMSDU PUSH?
		elem_ix = bcm_ring_prod(&pktpgr->pspl_state, pktpgr->pspl_depth);
		pspl_req_info = BCM_RING_ELEM(pktpgr->pspl_table, elem_ix,
			sizeof(hwa_pp_pspl_req_info_t));
		pspl_req_info->fifo_idx = fifo_idx;
		pspl_req_info->pkt_mapid = PKTMAPID(pktlist_head);
		pspl_req_info->mpdu_count = mpdu_count;
		pspl_req_info->pkt_count = pkt_count;

		HWA_PP_DBG(HWA_PP_DBG_MGR_TX, "  >>PAGEMGR_PUSH::REQ pkts %3u/%3u "
			"list[0x%p(%d) .. 0x%p(%d)] fifo %3u ==PUSH-REQ(%d)==>\n\n",
			pkt_count, mpdu_count, pktlist_head,
			PKTMAPID(pktlist_head), pktlist_tail, PKTMAPID(pktlist_tail),
			fifo_idx, trans_id);
	} else
#endif /* PSPL_TX_TEST */
	{
		HWA_ASSERT(tagged == HWA_PP_CMD_TAGGED);

		pktpgr->tag_push_req++;

		HWA_PP_DBG(HWA_PP_DBG_MGR_TX, "  >>PAGEMGR::REQ%sPUSH(tagged) : pkts %3u "
			"list[0x%p(%d) .. 0x%p(%d)] fifo %3u ==PUSH-REQ(%d)==>\n\n",
			(pagemgr_cmd == HWA_PP_PAGEMGR_PUSH_PKTTAG) ? " PKTTAG " : " ",
			pkt_count, pktlist_head, PKTMAPID(pktlist_head),
			pktlist_tail, PKTMAPID(pktlist_tail), fifo_idx, trans_id);
	}
}   // _hwa_pktpgr_push_req()

// PUSH packet from DDBM to HDBM, HW will free pkt in DDBM.
void // Push request
hwa_pktpgr_push_req(hwa_dev_t *dev, uint32 fifo_idx, uint8 tagged,
	void *pktlist_head, void *pktlist_tail, uint16 pkt_count,
	uint16 mpdu_count)
{
	_hwa_pktpgr_push_req(dev, fifo_idx, tagged, pktlist_head, pktlist_tail,
		pkt_count, mpdu_count, HWA_PP_PAGEMGR_PUSH);
}

// PUSH pkttag part of packet from DDBM to HDBM, HW will free pkt in DDBM.
void // PUSH pkttag part
hwa_pktpgr_push_pkttag_req(hwa_dev_t *dev, uint32 fifo_idx, uint8 tagged,
	void *pktlist_head, void *pktlist_tail, uint16 pkt_count,
	uint16 mpdu_count)
{
	_hwa_pktpgr_push_req(dev, fifo_idx, tagged, pktlist_head, pktlist_tail,
		pkt_count, mpdu_count, HWA_PP_PAGEMGR_PUSH_PKTTAG);
}

uint8 // HWApp: Get trans_id from specific pktpgr ring
hwa_pktpgr_get_trans_id(hwa_dev_t *dev, hwa_pktpgr_ring_t pp_ring)
{
	hwa_pktpgr_t *pktpgr;

	HWA_FTRACE(HWApp);
	HWA_ASSERT(dev != (struct hwa_dev *)NULL);
	HWA_ASSERT(pp_ring < hwa_pktpgr_req_ring_max);

	pktpgr = &dev->pktpgr;

	HWA_ERROR(("%s, trans_id %d\n", __FUNCTION__, pktpgr->trans_id[pp_ring]));

	return pktpgr->trans_id[pp_ring];
}   // hwa_pktpgr_get_trans_id()

void // HWApp: Update trans_id to specific pktpgr ring
hwa_pktpgr_update_trans_id(hwa_dev_t *dev, hwa_pktpgr_ring_t pp_ring, uint8 trans_id)
{
	hwa_pktpgr_t *pktpgr;

	HWA_FTRACE(HWApp);
	HWA_ASSERT(dev != (struct hwa_dev *)NULL);
	HWA_ASSERT(pp_ring < hwa_pktpgr_req_ring_max);

	pktpgr = &dev->pktpgr;
	pktpgr->trans_id[pp_ring] = trans_id;

	HWA_ERROR(("%s, trans_id %d\n", __FUNCTION__, pktpgr->trans_id[pp_ring]));
}

void // HWApp: Update wr_index to specific pktpgr ring
hwa_pktpgr_update_ring_wr_index(hwa_dev_t *dev, hwa_pktpgr_ring_t pp_ring, uint16 wr_index)
{
	hwa_pktpgr_t *pktpgr;
	hwa_ring_t   *req_ring;

	HWA_ERROR(("  WIP: %s, wr_index <%d>\n", __FUNCTION__, wr_index));

	HWA_ASSERT(dev != (struct hwa_dev *)NULL);
	HWA_ASSERT(pp_ring < hwa_pktpgr_req_ring_max);

	pktpgr = &dev->pktpgr;
	req_ring = pktpgr->req_ring[pp_ring];

	hwa_ring_prod_replace(req_ring, wr_index, TRUE);
}

// Wait for number of "countes" empty slots is available for specific pktpgr ring
bool
hwa_pktpgr_req_ring_wait_for_avail(hwa_dev_t *dev, hwa_pktpgr_ring_t pp_ring,
	uint32 counts, bool wait)
{
	hwa_pktpgr_t *pktpgr;
	hwa_ring_t   *req_ring;
	uint32 loop_count;
	int avail;

	HWA_ASSERT(dev != (struct hwa_dev *)NULL);
	HWA_ASSERT(pp_ring < hwa_pktpgr_req_ring_max);

	pktpgr = &dev->pktpgr;
	req_ring = pktpgr->req_ring[pp_ring];
	HWA_ASSERT(counts < req_ring->depth);
	loop_count = HWA_FSM_IDLE_POLLLOOP;

	// Flushes WR index to HWA
	hwa_ring_prod_put(req_ring);
	// Update HW RD index from HWA
	hwa_ring_prod_get(req_ring);
	// Available empty slots
	avail = hwa_ring_prod_avail(req_ring);

	HWA_ERROR(("Does \"%s\" ring have %d empty slots?", req_ring->name, counts));

	// Enough empty slots.
	if (avail >= counts) {
		HWA_ERROR((" [Y..%d]\n", avail));
		return TRUE;
	}

	// No,
	if (!wait) {
		HWA_ERROR((" [N..%d]\n", avail));
		return FALSE;
	}

	// No but wait until requested pp ring can have number of counts available
	HWA_ERROR(("  Wait...\n"));
	while (avail < counts) {
		HWA_TRACE(("%s HWA consuming pp ring<%d>\n", __FUNCTION__, pp_ring));
		OSL_DELAY(1);
		if (--loop_count == 0) {
			HWA_ERROR(("%s Cannot consume pp ring<%d> for avail counts<%d>\n",
				__FUNCTION__, pp_ring, counts));
			break;
		}
		// Update HW RD
		hwa_ring_prod_get(req_ring);
		avail = hwa_ring_prod_avail(req_ring);
	}

	HWA_ERROR(("  [%s] I have %d empty slots\n", loop_count ? "Y" : "N", avail));

	return (loop_count != 0);
}

static void
_hwa_pktpgr_freepkt(hwa_dev_t *dev, hwa_pp_lbuf_t *pktlist_head,
	hwa_pp_lbuf_t *pktlist_tail, int pkt_count, hwa_pp_pagemgr_cmd_t pagemgr_cmd,
	const char *pagemgr_cmd_str)
{
	int trans_id, req_loop;
	hwa_pp_freepkt_req_t req;

	HWA_AUDIT_DEV(dev);
	HWA_ASSERT(pkt_count != 0);
	HWA_ASSERT(pagemgr_cmd == HWA_PP_PAGEMGR_FREE_RX ||
		pagemgr_cmd == HWA_PP_PAGEMGR_FREE_TX ||
		pagemgr_cmd == HWA_PP_PAGEMGR_FREE_DDBM_LINK);
	HWA_ASSERT(pagemgr_cmd_str);

	if (pkt_count == 0) {
		HWA_ERROR(("%s: pkt_count is 0 from %s request\n",
			__FUNCTION__, pagemgr_cmd_str));
		return;
	}

	// Audit pktlist_head address
	HWA_ASSERT(((uint32)HWA_TABLE_INDX(hwa_pp_lbuf_t,
		dev->pktpgr.dnglpktpool_mem, pktlist_head)) <
		dev->pktpgr.dnglpktpool_max);
	HWA_ASSERT(!(((uintptr)pktlist_head - HWA_PTR2UINT(
		dev->pktpgr.dnglpktpool_mem)) % sizeof(hwa_pp_lbuf_t)));

	// Construct a request
	req.trans        = pagemgr_cmd;
	req.pkt_count    = pkt_count;
	req.pktlist_head = (uint32)pktlist_head;
	req.pktlist_tail = (uint32)pktlist_tail;

	req_loop = HWA_LOOPCNT;

req_again:
	trans_id = hwa_pktpgr_request(dev, hwa_pktpgr_freepkt_req_ring, &req);
	if (trans_id == HWA_FAILURE) {
		if (--req_loop > 0) {
			OSL_DELAY(10);
			goto req_again;
		}
		HWA_ERROR((">>PAGEMGR::REQ %s failure: pkts<%d> list[%p(%d) .. %p(%d)]",
			pagemgr_cmd_str, pkt_count, pktlist_head, PKTMAPID(pktlist_head),
			pktlist_tail, PKTMAPID(pktlist_tail)));
		HWA_ASSERT(0);
		return;
	}

	HWA_PP_DBG(HWA_PP_DBG_MGR, "  >>PAGEMGR::REQ %s      : pkts<%d> "
		"list[%p(%d) .. %p(%d)] ==%s-REQ(%d), no rsp ==>\n\n",
		pagemgr_cmd_str, pkt_count, pktlist_head, PKTMAPID(pktlist_head),
		pktlist_tail, PKTMAPID(pktlist_tail), pagemgr_cmd_str, trans_id);
} // _hwa_pktpgr_freepkt

// Free RxLfrag. No RSP
void
hwa_pktpgr_free_rx(struct hwa_dev *dev, hwa_pp_lbuf_t *pktlist_head,
	hwa_pp_lbuf_t * pktlist_tail, int pkt_count)
{
	_hwa_pktpgr_freepkt(dev, pktlist_head, pktlist_tail, pkt_count,
		HWA_PP_PAGEMGR_FREE_RX, "FREE_RX");
}

// Free RxPostHostInfo. No RSP
void
hwa_pktpgr_free_rph(struct hwa_dev *dev, uint32 host_pktid,
	uint16 host_datalen, dma64addr_t data_buf_haddr64)
{
	int trans_id, req_loop;
	hwa_pp_freerph_req_t req;

	HWA_AUDIT_DEV(dev);

	req.trans            = HWA_PP_PAGEMGR_FREE_RPH;
	req.host_datalen     = host_datalen;
	req.host_pktid       = host_pktid;
	req.data_buf_haddr64 = data_buf_haddr64;

	req_loop = HWA_LOOPCNT;

req_again:
	trans_id = hwa_pktpgr_request(dev, hwa_pktpgr_freerph_req_ring, &req);
	if (trans_id == HWA_FAILURE) {
		if (--req_loop > 0) {
			OSL_DELAY(10);
			goto req_again;
		}
		HWA_ERROR((">>PAGEMGR::REQ FREE_RPH failure : pktid<%u> "
			"haddr<0x%08x:0x%08x> len<%u>\n",
			host_pktid, data_buf_haddr64.hiaddr,
			data_buf_haddr64.loaddr, host_datalen));
		HWA_ASSERT(0);
		return;
	}

	HWA_PP_DBG(HWA_PP_DBG_MGR_RX, "  >>PAGEMGR::REQ FREE_RPH     : pktid<%u> "
		"haddr<0x%08x:0x%08x> len<%u> ==FREERPH-REQ(%d), no rsp ==>\n\n",
		host_pktid, data_buf_haddr64.hiaddr,
		data_buf_haddr64.loaddr, host_datalen, trans_id);
}   // hwa_pktpgr_free_rph()

// Free TxLfrag. No RSP
void
hwa_pktpgr_free_tx(struct hwa_dev *dev, hwa_pp_lbuf_t *pktlist_head,
	hwa_pp_lbuf_t * pktlist_tail, int pkt_count)
{
	_hwa_pktpgr_freepkt(dev, pktlist_head, pktlist_tail, pkt_count,
		HWA_PP_PAGEMGR_FREE_TX, "FREE_TX");
}

// Free DDBM in pktlist. No RSP
void
hwa_pktpgr_free_ddbm_pkt(hwa_dev_t *dev, hwa_pp_lbuf_t *pktlist_head,
	hwa_pp_lbuf_t * pktlist_tail, int pkt_count)
{
	_hwa_pktpgr_freepkt(dev, pktlist_head, pktlist_tail, pkt_count,
		HWA_PP_PAGEMGR_FREE_DDBM_LINK, "FREE_DDBM");
}

// ###############
// ## RX: hwa_pcie.c ##
// ###############

void
hwa_rxpath_rxlbufpool_fill(hwa_dev_t *dev, uint16 force_cnt)
{
	int trans_id;
	hwa_pktpgr_t *pktpgr;
	hwa_pp_pagemgr_req_alloc_t req;
	hwa_pp_rxlbuf_pool_t *rxlbufpool;

	HWA_FTRACE(HWApp);
	HWA_AUDIT_DEV(dev);

	// Setup locals
	pktpgr = &dev->pktpgr;
	rxlbufpool = &pktpgr->rxlbufpool;

	//HWA_ASSERT(rxlbufpool->n_pkts <= HWA_PP_LBUF_POOL_LEN_MAX);
	if (rxlbufpool->n_pkts >= HWA_PP_LBUF_POOL_LEN_MAX &&
		force_cnt == 0)
		return;

	// Refill rxlbufpool
	req.trans      = HWA_PP_PAGEMGR_ALLOC_RX_RPH;
	req.tagged     = HWA_PP_CMD_NOT_TAGGED;
	if (force_cnt)
		req.pkt_count  = force_cnt;
	else
		req.pkt_count  = (HWA_PP_LBUF_POOL_LEN_MAX - rxlbufpool->n_pkts);

	trans_id = hwa_pktpgr_request(dev, hwa_pktpgr_pagemgr_req_ring, &req);
	if (trans_id != HWA_FAILURE) {
			rxlbufpool->n_pkts += req.pkt_count;
			rxlbufpool->trans_id = trans_id;
	}

	HWA_PP_DBG(HWA_PP_DBG_MGR_RX, "  >>PAGEMGR::REQ ALLOC_RX_RPH     : "
		"pkt_count<%u> ==ALLOCRXRPH-REQ(%d)==>\n\n", req.pkt_count, trans_id);
}

// PAGEMGR_ALLOC_RX_RPH give us both PP_LBUF and RPH.
// Free RxLfrag which we don't use it.
int
hwa_rph_allocate(uint32 *bufid, uint16 *len, dma64addr_t *haddr64, bool pre_req)
{
	hwa_dev_t *dev;
	hwa_pktpgr_t *pktpgr;
	hwa_pp_lbuf_t *rxlbuf;
	hwa_pp_rxlbuf_pool_t *rxlbufpool;

	HWA_FTRACE(HWApp);
	dev = HWA_DEVP(TRUE);

	// Setup locals
	pktpgr = &dev->pktpgr;
	rxlbufpool = &pktpgr->rxlbufpool;

	// Check pre_req case.
	if (pre_req) {
		if (rxlbufpool->avail == 0) {
			hwa_rxpath_rxlbufpool_fill(dev, 0);
			return HWA_FAILURE;
		}

		return HWA_SUCCESS;
	}

	// Take one rxlbuf;
	ASSERT(rxlbufpool->pkt_head != NULL);
	rxlbuf = rxlbufpool->pkt_head;
	rxlbufpool->pkt_head = (hwa_pp_lbuf_t *)PKTLINK(rxlbuf);

	if (rxlbufpool->pkt_head == NULL) {
		rxlbufpool->pkt_tail = NULL;
	}

	// Terminate next
	HWAPKTSETLINK(rxlbuf, NULL);

	// Copy out the RPH
	*bufid = RPH_HOSTPKTID(rxlbuf);
	*len = RPH_HOSTDATALEN(rxlbuf);
	*haddr64 = RPH_HOSTADDR64(rxlbuf);
	if (dev->host_addressing & HWA_32BIT_ADDRESSING) {
		// 32bit host
		HWA_ASSERT(HADDR64_HI(*haddr64) == dev->host_physaddrhi);
	}

	HWA_TRACE(("%s rph alloc pktid<%u> len <%u> haddr<0x%08x:0x%08x>\n",
		HWA1x, *bufid, *len, HADDR64_HI(*haddr64), HADDR64_LO(*haddr64)));

	// Free RxLfrag which we don't use it.
	hwa_pktpgr_free_rx(dev, rxlbuf, rxlbuf, 1);

	rxlbufpool->avail--;
	rxlbufpool->n_pkts--;

	// Refill rxlbufpool
	hwa_rxpath_rxlbufpool_fill(dev, 0);

	return HWA_SUCCESS;
}

void
hwa_rxpath_pagemgr_alloc_rx_rph_rsp(hwa_dev_t *dev, uint8 pktpgr_trans_id, int pkt_count,
	hwa_pp_lbuf_t * pktlist_head, hwa_pp_lbuf_t * pktlist_tail)
{
	uint16 pkt = 0;
	hwa_pktpgr_t *pktpgr;
	hwa_pp_rxlbuf_pool_t *rxlbufpool;
	hwa_pp_lbuf_t *rxlbuf = pktlist_head;

	HWA_FTRACE(HWApp);
	HWA_AUDIT_DEV(dev);

	// Setup locals
	pktpgr = &dev->pktpgr;
	rxlbufpool = &pktpgr->rxlbufpool;

	HWA_PP_DBG(HWA_PP_DBG_MGR_RX, "  <<PAGEMGR::RSP PAGEMGR_ALLOC_RX_RPH : pkts %3u "
		"list[%p(%d) .. %p(%d)] id %u <==ALLOCRXRPH-RSP(%d)==\n\n",
		pkt_count, pktlist_head, PKTMAPID(pktlist_head),
		pktlist_tail, PKTMAPID(pktlist_tail), pktpgr_trans_id, pktpgr_trans_id);

	if (pkt_count == 0) {
		HWA_ERROR(("%s: pkt_count is 0\n", __FUNCTION__));
		// Sync avail and n_pkts at last rsp.
		HWA_ASSERT(rxlbufpool->avail < rxlbufpool->n_pkts);
		if (rxlbufpool->trans_id == pktpgr_trans_id &&
			rxlbufpool->n_pkts != rxlbufpool->avail) {
			rxlbufpool->n_pkts = rxlbufpool->avail;
		}
		return;
	}

	// Sanity check.
	HWA_ASSERT(pktlist_tail->context.control.link == 0U);
	for (pkt = 0; pkt < pkt_count - 1; ++pkt) {
		HWA_ASSERT(RPH_HOSTPKTID(rxlbuf) != 0xFFFF);
		rxlbuf = (hwa_pp_lbuf_t*)rxlbuf->context.control.link;
	}
	HWA_ASSERT(rxlbuf == pktlist_tail);
	HWA_ASSERT(RPH_HOSTPKTID(rxlbuf) != 0xFFFF);

	// Add to rxlbufpool
	if (rxlbufpool->pkt_head == NULL) {
		rxlbufpool->pkt_head = pktlist_head;
		rxlbufpool->pkt_tail = pktlist_tail;
	} else {
		PKTSETLINK(rxlbufpool->pkt_tail, pktlist_head);
		rxlbufpool->pkt_tail = pktlist_tail;
	}
	rxlbufpool->avail += pkt_count;

	// Sync avail and n_pkts at last rsp.
	HWA_ASSERT(rxlbufpool->avail <= rxlbufpool->n_pkts);
	if (rxlbufpool->trans_id == pktpgr_trans_id &&
		rxlbufpool->n_pkts != rxlbufpool->avail) {
		rxlbufpool->n_pkts = rxlbufpool->avail;
	}

	return;
}

// ###############
// ## TX: hwa_pcie.c ##
// ###############
void
hwa_txpost_txlbufpool_fill(hwa_dev_t *dev)
{
	int trans_id;
	hwa_pktpgr_t *pktpgr;
	hwa_pp_pagemgr_req_alloc_t req;
	hwa_pp_txlbuf_pool_t *txlbufpool;

	HWA_FTRACE(HWApp);
	HWA_AUDIT_DEV(dev);

	// Setup locals
	pktpgr = &dev->pktpgr;
	txlbufpool = &pktpgr->txlbufpool;

	HWA_ASSERT(txlbufpool->n_pkts <= HWA_PP_LBUF_POOL_LEN_MAX);
	if (txlbufpool->n_pkts >= HWA_PP_LBUF_POOL_LEN_MAX)
		return;

	// Refill txlbufpool
	req.trans      = HWA_PP_PAGEMGR_ALLOC_TX;
	req.pkt_count  = (HWA_PP_LBUF_POOL_LEN_MAX - txlbufpool->n_pkts);
	req.tagged     = HWA_PP_CMD_NOT_TAGGED;

	trans_id = hwa_pktpgr_request(dev, hwa_pktpgr_pagemgr_req_ring, &req);
	if (trans_id != HWA_FAILURE) {
			txlbufpool->n_pkts += req.pkt_count;
			txlbufpool->trans_id = trans_id;
	}

	HWA_PP_DBG(HWA_PP_DBG_MGR_TX, "  >>PAGEMGR::REQ ALLOC_TX     : "
		"pkt_count<%u> ==ALLOCTX-REQ(%d)==>\n\n", req.pkt_count, trans_id);
}

void * // Handle SW allocate tx buffer from TxBM. Return txpost buffer || NULL
hwa_txpost_txbuffer_get(struct hwa_dev *dev)
{
	uint16 pkt_mapid;
	hwa_pktpgr_t *pktpgr;
	hwa_pp_lbuf_t *txlbuf;
	hwa_pp_txlbuf_pool_t *txlbufpool;

	HWA_FTRACE(HWApp);
	HWA_AUDIT_DEV(dev);

	// Setup locals
	pktpgr = &dev->pktpgr;
	txlbufpool = &pktpgr->txlbufpool;

	if (txlbufpool->avail == 0) {
		hwa_txpost_txlbufpool_fill(dev);
		return NULL;
	}

	ASSERT(txlbufpool->pkt_head != NULL);
	txlbuf = txlbufpool->pkt_head;
	txlbufpool->pkt_head = (hwa_pp_lbuf_t *)PKTLINK(txlbuf);

	if (txlbufpool->pkt_head == NULL) {
		txlbufpool->pkt_tail = NULL;
	}

	// Clear context 128B except pkt_mapid.
	pkt_mapid = PKTMAPID(txlbuf);
	bzero(txlbuf, LBUFSZ);
	PKTMAPID(txlbuf) = pkt_mapid;
	// Clear repurpose latest 7 words of databuffer
	bzero(&txlbuf->txreset, HWA_PP_PKTDATABUF_TXFRAG_RESV_BYTES);

	/* Mark packet as TXFRAG and HWAPKT */
	PKTSETTXFRAG(dev->osh, txlbuf);
	PKTSETHWAPKT(dev->osh, txlbuf);
	PKTRESETHWA3BPKT(dev->osh, txlbuf);

	/* Set data point */
	PKTSETTXBUF(dev->osh, txlbuf, PKTPPBUFFERP(txlbuf), HWA_PP_PKTDATABUF_TXFRAG_MAX_BYTES);

	txlbufpool->avail--;
	txlbufpool->n_pkts--;

	// Refill txlbufpool
	hwa_txpost_txlbufpool_fill(dev);

	return txlbuf;
}

void // PAGEMGR ALLOC TX RESP
hwa_txpost_pagemgr_alloc_tx_rsp(hwa_dev_t *dev, uint8 pktpgr_trans_id, int pkt_count,
	hwa_pp_lbuf_t * pktlist_head, hwa_pp_lbuf_t * pktlist_tail)
{
	uint16 pkt = 0;
	hwa_pktpgr_t *pktpgr;
	hwa_pp_txlbuf_pool_t *txlbufpool;
	hwa_pp_lbuf_t *txlbuf;

	HWA_FTRACE(HWApp);
	HWA_AUDIT_DEV(dev);

	// Setup locals
	pktpgr = &dev->pktpgr;
	txlbufpool = &pktpgr->txlbufpool;
	txlbuf = pktlist_head;

	HWA_PP_DBG(HWA_PP_DBG_MGR_TX, "  <<PAGEMGR::RSP PAGEMGR_ALLOC_TX : pkts %3u "
		"list[%p(%d) .. %p(%d)] id %u <==ALLOCTX-RSP(%d)==\n\n",
		pkt_count, pktlist_head, PKTMAPID(pktlist_head),
		pktlist_tail, PKTMAPID(pktlist_tail), pktpgr_trans_id, pktpgr_trans_id);

	if (pkt_count == 0) {
		HWA_ERROR(("%s: pkt_count is 0\n", __FUNCTION__));
		// Sync avail and n_pkts at last rsp.
		HWA_ASSERT(txlbufpool->avail < txlbufpool->n_pkts);
		if (txlbufpool->trans_id == pktpgr_trans_id &&
			txlbufpool->n_pkts != txlbufpool->avail) {
			txlbufpool->n_pkts = txlbufpool->avail;
		}

		return;
	}

	HWA_ASSERT(pktlist_tail->context.control.link == 0U);
	for (pkt = 0; pkt < pkt_count - 1; ++pkt) {
		// Only control::pkt_mapid and control::link have values, others are garbage
		// NOTE: Clear some fields of txlbuf in hwa_txpost_txbuffer_get
		HWA_TRACE(("pkt_mapid<%d> link 0x%p\n",
			txlbuf->context.control.pkt_mapid,
			txlbuf->context.control.link));

		HWA_PKT_DUMP_EXPR(hwa_txpost_dump_pkt(txlbuf, NULL, "alloc_tx", pkt, TRUE));

		txlbuf = (hwa_pp_lbuf_t*)txlbuf->context.control.link;
	}
	HWA_ASSERT(txlbuf == pktlist_tail);
	HWA_TRACE(("pkt_mapid<%d> link 0x%p\n",
		txlbuf->context.control.pkt_mapid,
		txlbuf->context.control.link));

	HWA_PKT_DUMP_EXPR(hwa_txpost_dump_pkt(txlbuf, NULL, "alloc_tx", pkt, TRUE));

	// add to txlbufpool
	if (txlbufpool->pkt_head == NULL) {
		txlbufpool->pkt_head = pktlist_head;
		txlbufpool->pkt_tail = pktlist_tail;
	} else {
		PKTSETLINK(txlbufpool->pkt_tail, pktlist_head);
		txlbufpool->pkt_tail = pktlist_tail;
	}
	txlbufpool->avail += pkt_count;

	// Sync avail and n_pkts at last rsp.
	HWA_ASSERT(txlbufpool->avail <= txlbufpool->n_pkts);
	if (txlbufpool->trans_id == pktpgr_trans_id &&
		txlbufpool->n_pkts != txlbufpool->avail) {
		txlbufpool->n_pkts = txlbufpool->avail;
	}

	return;

} // hwa_txpost_pagein_rsp

// Flag to enable HW capability of one MPDU has more than one TxLfrag.
// Mandatory for more thean 4-in-1 AMSDU.
uint8
hwa_pktpgr_multi_txlfrag(struct hwa_dev *dev)
{
	return dev->flexible_txlfrag;
}

void // Free HME LCLPKT
hwa_pktpgr_hmedata_free(struct hwa_dev *dev, void *pkt)
{
#if defined(BCMHME)
	haddr64_t haddr64;

	// Put HME LCLPK back
	HADDR64_HI_SET(haddr64, PKTHME_HI(dev->osh, pkt));
	HADDR64_LO_SET(haddr64, PKTHME_LO(dev->osh, pkt));
	hme_put(HME_USER_LCLPKT, PKTHMELEN(dev->osh, pkt), haddr64);
#endif /* BCMHME */
}

// Wait for all request have responsed.
static void
hwa_pktpgr_req_wait_to_finish(hwa_dev_t *dev, int *req_cnt_p,
	hwa_ring_t *rsp_ring, const char *who, hwa_callback_t callback)
{
	int req_cnt, req_cnt_orig;
	uint32 loop = HWA_FSM_IDLE_POLLLOOP;

	HWA_FTRACE(HWApp);
	HWA_ASSERT(dev);

	// Polling until all request are processed.
	req_cnt_orig = req_cnt = *req_cnt_p;
	BCM_REFERENCE(req_cnt_orig);

	do {
		// NOTE: here we may process mixed tran type
		hwa_pktpgr_response(dev, rsp_ring, callback);

		// Wait a moment if no progress.
		if (req_cnt == *req_cnt_p) {
			OSL_DELAY(1);
			loop--;
		}
		req_cnt = *req_cnt_p;
	} while (*req_cnt_p > 0 && loop);

	HWA_ERROR(("  HWA[%d] %s polling \"%s\" ring, pending<%u/%u>, try<%d>. %s\n",
		dev->unit, who, rsp_ring->name, req_cnt_orig, *req_cnt_p,
		(HWA_FSM_IDLE_POLLLOOP - loop), loop ? "Done" : "Timeout"));

}

// Wait for all PageIn TxS have responsed.
void
hwa_pktpgr_txstatus_wait_to_finish(hwa_dev_t *dev, int fifo_idx)
{
	hwa_pktpgr_t *pktpgr;

	HWA_FTRACE(HWApp);
	HWA_AUDIT_DEV(dev);

	// XXX May need optimization
	// The PageIn Req service three type of req [TxS, TxPost and Rx],
	// how can the TxS pagein be serviced at high priority in "wl reinit" condition?
	// For example in hwa_txstat_reclaim(reinit == TRUE), we need to process
	// all PageIn TxS Rsp so we use hwa_pktpgr_txstatus_wait_to_finish to polling.
	// But if PageIn Req Q has a lot of TxPost/Rx reqs before TxS then we may try
	// to swap TxPost, Rx PageIn Req to the end [hint: use invalid bit].
	// If the req in Q is few then we don't need to swap it.

	// Setup locals
	pktpgr = &dev->pktpgr;

	if (pktpgr->pgi_txs_req_tot == 0)
		return;

	if ((fifo_idx >= 0) && (pktpgr->pgi_txs_req[fifo_idx] == 0))
		return;

	// Poll until all PageIn TxS are processed.
	hwa_pktpgr_req_wait_to_finish(dev, (fifo_idx >= 0) ?
		&pktpgr->pgi_txs_req[fifo_idx] : &pktpgr->pgi_txs_req_tot,
		&dev->pktpgr.pagein_rsp_ring, "TxStatus", HWA_PKTPGR_PAGEIN_CALLBACK);

}

// Wait for tagged PUSH have responsed.
static void
hwa_pktpgr_tag_push_wait_to_finish(hwa_dev_t *dev)
{
	hwa_pktpgr_t *pktpgr;

	HWA_FTRACE(HWApp);
	HWA_AUDIT_DEV(dev);

	// Setup locals
	pktpgr = &dev->pktpgr;

	if (pktpgr->tag_push_req == 0)
		return;

	// Poll until tagged PUSH are processed.
	hwa_pktpgr_req_wait_to_finish(dev, &pktpgr->tag_push_req,
		&dev->pktpgr.pagemgr_rsp_ring, "Push(Tag)", HWA_PKTPGR_PAGEMGR_CALLBACK);
}

// Wait for all PULL have responsed.
static void
hwa_pktpgr_tag_pull_wait_to_finish(hwa_dev_t *dev)
{
	hwa_pktpgr_t *pktpgr;

	HWA_FTRACE(HWApp);
	HWA_AUDIT_DEV(dev);

	// Setup locals
	pktpgr = &dev->pktpgr;

	HWA_ASSERT(pktpgr->tag_pull_req >= 0);
	if (pktpgr->tag_pull_req == 0)
		return;

	// Poll until tagged PULL are processed.
	hwa_pktpgr_req_wait_to_finish(dev, &pktpgr->tag_pull_req,
		&dev->pktpgr.pagemgr_rsp_ring, "Pull(Tag)", HWA_PKTPGR_PAGEMGR_CALLBACK);

}

// Wait for all PageOut local response.
void
hwa_pktpgr_pageout_local_rsp_wait_to_finish(hwa_dev_t *dev)
{
	hwa_pktpgr_t *pktpgr;

	HWA_FTRACE(HWApp);
	HWA_AUDIT_DEV(dev);

	// Setup locals
	pktpgr = &dev->pktpgr;

	if (pktpgr->pgo_local_req == 0)
		return;

	// Poll until tagged PULL are processed.
	hwa_pktpgr_req_wait_to_finish(dev, &pktpgr->pgo_local_req,
		&dev->pktpgr.pageout_rsp_ring, "Tx(Local)", HWA_PKTPGR_PAGEOUT_CALLBACK);
}

// Wait for all PageIn RxProcess have responsed.
void
hwa_pktpgr_rxprocess_wait_to_finish(hwa_dev_t *dev)
{
	hwa_pktpgr_t *pktpgr;

	HWA_FTRACE(HWApp);
	HWA_AUDIT_DEV(dev);

	// Setup locals
	pktpgr = &dev->pktpgr;

	if (pktpgr->pgi_rxpkt_req == 0)
		return;

	// Poll until tagged PUSH are processed.
	hwa_pktpgr_req_wait_to_finish(dev, &pktpgr->pgi_rxpkt_req,
		&dev->pktpgr.pagein_rsp_ring, "RxProcess", HWA_PKTPGR_PAGEIN_CALLBACK);
}

// Pagein packets from HW shadow
void
hwa_pktpgr_txfifo_shadow_reclaim(struct hwa_dev *dev, uint32 fifo_idx)
{
	int mpdu_count, pktpgr_trans_id, req_loop;
	hwa_txfifo_ovflwqctx_t ovflwq;
	hwa_pp_pagein_req_txstatus_t req;
	uint16 ppddavailcnt;

	// Audit pre-conditions
	HWA_AUDIT_DEV(dev);

	// Make sure all previous pagein TxS req are responsed before reclaim it.
	hwa_pktpgr_txstatus_wait_to_finish(dev, fifo_idx);

	// Get packets count from HWA OvfQ.
	mpdu_count = hwa_txfifo_get_ovfq(dev, fifo_idx, &ovflwq);
	if (mpdu_count <= 0)
		return;

	// Get DDBM available count
	ppddavailcnt = hwa_pktpgr_dbm_availcnt(dev, HWA_DDBM);
	HWA_ERROR(("HW shadow%d has %d packets, ddbmavil<%d>\n", fifo_idx,
		mpdu_count, ppddavailcnt));
	if (mpdu_count > ppddavailcnt) {
		// XXX, need SW enhancement.
		// Reference hwa_pktpgr_map_pkts for low dnglpktspool case.
		// Need to work with caller to PageIn-Process-PageIn piece by piece.
		HWA_ERROR(("%s Out of DDBM resource, req<%d> ddbm<%d>\n",
			HWApp, mpdu_count, ppddavailcnt));
		HWA_ASSERT(0);
	}

	// Issue pagein request
	req.trans        = HWA_PP_PAGEIN_TXSTATUS;
	req.fifo_idx     = fifo_idx;
	req.mpdu_count   = mpdu_count;
	req.tagged       = HWA_PP_CMD_TAGGED; // Just Pagein packets in HW shadow

	req_loop = HWA_LOOPCNT;

req_again:
	pktpgr_trans_id = hwa_pktpgr_request(dev, hwa_pktpgr_pagein_req_ring, &req);
	if (pktpgr_trans_id == HWA_FAILURE) {
		if (--req_loop > 0) {
			OSL_DELAY(10);
			goto req_again;
		}
		HWA_ERROR(("%s PAGEIN::REQ TXSTATUS [shadow] failure mpdu<%u> fifo<%u>,"
			" no resource\n", HWA4a, req.mpdu_count, req.fifo_idx));
		HWA_ASSERT(0);
		return;
	}

	dev->pktpgr.pgi_txs_req[req.fifo_idx]++;
	dev->pktpgr.pgi_txs_req_tot++;

	HWA_PP_DBG(HWA_PP_DBG_4A, "  >>PAGEIN::REQ TXSTATUS(tagged): mpdu %3u fifo %3u "
		"pgi_txs_req %d/%d ==TXS-REQ(%d)==>\n\n", req.mpdu_count, req.fifo_idx,
		dev->pktpgr.pgi_txs_req[req.fifo_idx], dev->pktpgr.pgi_txs_req_tot,
		pktpgr_trans_id);

	// Wait for all packets in HW shadow pagein to SW shadow.
	hwa_pktpgr_txstatus_wait_to_finish(dev, req.fifo_idx);
}

static void
hwa_pktpgr_req_ring_stop(hwa_dev_t *dev, const char *who, hwa_pktpgr_ring_t pp_ring)
{
	hwa_pktpgr_t *pktpgr;
	hwa_pp_ring_t *req_ring_reg;
	uint32 u32, state, loop_count;

	HWA_FTRACE(HWApp);
	HWA_ASSERT(dev != (struct hwa_dev *)NULL);
	HWA_ASSERT(pp_ring < hwa_pktpgr_req_ring_max);

	// Audit pre-conditions
	HWA_AUDIT_DEV(dev);

	// Setup locals
	pktpgr = &dev->pktpgr;
	req_ring_reg = pktpgr->req_ring_reg[pp_ring];

	// Stop req ring
	u32 = HWA_RD_REG_ADDR(HWApp, &req_ring_reg->cfg);
	u32 |= BCM_SBIT(HWA_PAGER_PP_REQ_RING_CFG_STOP);
	HWA_WR_REG_ADDR(HWApp, &req_ring_reg->cfg, u32);
	loop_count = 0;
	do {
		if (loop_count)
			OSL_DELAY(1);
		u32 = HWA_RD_REG_ADDR(HWApp, &req_ring_reg->debug);
		state = BCM_GBF(u32, HWA_PAGER_PP_REQ_RING_DEBUG_FSM);
		HWA_TRACE(("%s: Polling %s DEBUG::STATE <%d>\n", __FUNCTION__, who, state));
	} while (state != 0 && ++loop_count != HWA_FSM_IDLE_POLLLOOP);
	if (loop_count == HWA_FSM_IDLE_POLLLOOP) {
		HWA_ERROR(("%s: %s is not idle <%d>\n", __FUNCTION__, who, state));
	}
	HWA_ERROR(("%s: Stop \"%s\" ring\n", __FUNCTION__, who));
}

static void
hwa_pktpgr_req_ring_start(hwa_dev_t *dev, const char *who, hwa_pktpgr_ring_t pp_ring)
{
	hwa_pktpgr_t *pktpgr;
	hwa_pp_ring_t *req_ring_reg;
	uint32 u32;

	HWA_FTRACE(HWApp);
	HWA_ASSERT(dev != (struct hwa_dev *)NULL);
	HWA_ASSERT(pp_ring < hwa_pktpgr_req_ring_max);

	// Audit pre-conditions
	HWA_AUDIT_DEV(dev);

	// Setup locals
	pktpgr = &dev->pktpgr;
	req_ring_reg = pktpgr->req_ring_reg[pp_ring];

	// Clear req ring stop bit
	u32 = HWA_RD_REG_ADDR(HWApp, &req_ring_reg->cfg);
	u32 = BCM_CBIT(u32, HWA_PAGER_PP_REQ_RING_CFG_STOP);
	HWA_WR_REG_ADDR(HWApp, &req_ring_reg->cfg, u32);
	HWA_ERROR(("%s: Start \"%s\" ring\n", __FUNCTION__, who));
}

static uint32
hwa_pktpgr_req_ring_is_stop(hwa_dev_t *dev, hwa_pktpgr_ring_t pp_ring)
{
	hwa_pktpgr_t *pktpgr;
	hwa_pp_ring_t *req_ring_reg;
	uint32 u32;

	HWA_FTRACE(HWApp);
	HWA_ASSERT(dev != (struct hwa_dev *)NULL);
	HWA_ASSERT(pp_ring < hwa_pktpgr_req_ring_max);

	// Audit pre-conditions
	HWA_AUDIT_DEV(dev);

	// Setup locals
	pktpgr = &dev->pktpgr;
	req_ring_reg = pktpgr->req_ring_reg[pp_ring];

	// Check start req ring
	u32 = HWA_RD_REG_ADDR(HWApp, &req_ring_reg->cfg);
	return BCM_GBIT(u32, HWA_PAGER_PP_REQ_RING_CFG_STOP);
}

// Remove specific packets in PageOut Req Q to SW shadow.
void
hwa_pktpgr_pageout_ring_shadow_reclaim(struct hwa_dev *dev, uint32 fifo_idx)
{
	int wr_idx, rd_idx;
	uint16 depth, mpdu, mpdu_count;
	hwa_ring_t *req_ring;
	hwa_pp_cmd_t *req_cmd;
	hwa_pp_pageout_req_pktlist_t *pktlist;
	hwa_pp_pageout_req_pktlocal_t *pktlocal;
	hwa_pp_lbuf_t *txlbuf, *pktlist_head, *pktlist_tail;

	HWA_FTRACE(HWApp);
	HWA_ASSERT(dev != (struct hwa_dev *)NULL);

	// Make sure PageOutReq ring is stopped.
	if (!hwa_pktpgr_req_ring_is_stop(dev, hwa_pktpgr_pageout_req_ring)) {
		HWA_ERROR(("PageOutReq ring is not stop!\n"));
		HWA_ASSERT(0);
	}

	// Setup local
	req_ring = &dev->pktpgr.pageout_req_ring;

	// Is empty? hwa_ring_is_cons_all will sync read.
	if (hwa_ring_is_cons_all(req_ring))
		return;

	wr_idx = HWA_RING_STATE(req_ring)->write;
	rd_idx = HWA_RING_STATE(req_ring)->read;
	depth = req_ring->depth;

	HWA_ERROR(("Walk \"PO>\" ring to remove fifo%d packets to SW shadow\n", fifo_idx));

	while (rd_idx != wr_idx) {
		// Fetch pageout req cmd
		req_cmd = HWA_RING_ELEM(hwa_pp_cmd_t, req_ring, rd_idx);
		pktlist = &req_cmd->pageout_req_pktlist;
		pktlocal = &req_cmd->pageout_req_pktlocal;

		if (pktlist->fifo_idx == fifo_idx &&
			pktlist->invalid == HWA_PP_CMD_NOT_INVALID) {
			HWA_ASSERT((pktlist->trans == HWA_PP_PAGEOUT_PKTLIST_NR) ||
				(pktlist->trans == HWA_PP_PAGEOUT_PKTLIST_WR) ||
				(pktlist->trans == HWA_PP_PAGEOUT_PKTLOCAL));

			if (pktlist->trans == HWA_PP_PAGEOUT_PKTLIST_NR ||
				pktlist->trans == HWA_PP_PAGEOUT_PKTLIST_WR) {
				pktlist_head = (hwa_pp_lbuf_t *)pktlist->pktlist_head;
				pktlist_tail = (hwa_pp_lbuf_t *)pktlist->pktlist_tail;
				mpdu_count = pktlist->mpdu_count;
				txlbuf = pktlist_head;

				// XXX, do we need to do fixup? review it.
				for (mpdu = 0; mpdu < mpdu_count - 1; ++mpdu) {
					hwa_txstat_pagein_rsp_fixup(dev, txlbuf, mpdu_count);
					txlbuf = (hwa_pp_lbuf_t *)PKTLINK(txlbuf);
				}

				hwa_txstat_pagein_rsp_fixup(dev, txlbuf, mpdu_count);

				HWA_ERROR(("  Remove %d packets to SW shadow%d\n",
					mpdu_count, fifo_idx));

				// Link the pktlist to shadow list.
				hwa_txfifo_shadow_enq(dev, fifo_idx, mpdu_count,
					pktlist->pkt_count, pktlist_head, pktlist_tail);
			} else {
				txlbuf = (hwa_pp_lbuf_t *)pktlocal->pkt_local;
				hwa_txstat_pagein_rsp_fixup_local(dev, txlbuf,
					fifo_idx, 1);
				HWA_ERROR(("  Remove 1 local packet to SW shadow%d\n", fifo_idx));
			}

			// Mark invalid Pageout Req.
			pktlist->invalid = HWA_PP_CMD_INVALID;
		}

		// Next.
		rd_idx = (rd_idx + 1) % depth;
	}
}

// Invalid request
void
hwa_pktpgr_req_ring_invalid_req(hwa_dev_t *dev, hwa_pktpgr_ring_t pp_ring,
	uint8 cmdtype, const char *cmdstr)
{
	uint8 trans;
	uint16 depth;
	int wr_idx, rd_idx;
	hwa_ring_t *req_ring;
	hwa_pp_cmd_t *req_cmd;

	HWA_FTRACE(HWApp);
	HWA_ASSERT(dev != (struct hwa_dev *)NULL);
	HWA_ASSERT(pp_ring < hwa_pktpgr_req_ring_max);

	// Setup local
	req_ring = dev->pktpgr.req_ring[pp_ring];

	// Is empty? hwa_ring_is_cons_all will sync read.
	if (hwa_ring_is_cons_all(req_ring))
		return;

	// Stop req ring
	hwa_pktpgr_req_ring_stop(dev, req_ring->name, pp_ring);

	// sync read again.
	hwa_ring_prod_get(req_ring);

	wr_idx = HWA_RING_STATE(req_ring)->write;
	rd_idx = HWA_RING_STATE(req_ring)->read;
	depth = req_ring->depth;

	HWA_ERROR(("Cancel \"%s\" req in \"%s\" ring.\n", cmdstr, req_ring->name));

	while (rd_idx != wr_idx) {
		// Fetch req cmd
		req_cmd = HWA_RING_ELEM(hwa_pp_cmd_t, req_ring, rd_idx);
		trans = req_cmd->u8[HWA_PP_CMD_TRANS] & HWA_PP_CMD_TRANS_MASK;
		if (trans == cmdtype) {
			req_cmd->u8[HWA_PP_CMD_TRANS] |= BCM_SBIT(HWA_PP_CMD_INVALID);
			HWA_ERROR(("  Invalid + @ rd_idx<%d>\n", rd_idx));

			// Reduce one req
			if (cmdtype == HWA_PP_PAGEIN_RXPROCESS) {
				hwa_pp_pagein_req_rxprocess_t *req;

				req = (hwa_pp_pagein_req_rxprocess_t *)req_cmd;
				HWA_ASSERT(dev->pktpgr.pgi_rxpkt_req > 0);
				dev->pktpgr.pgi_rxpkt_req--;
				HWA_ASSERT(dev->pktpgr.pgi_rxpkt_in_trans >= req->pkt_count);
				dev->pktpgr.pgi_rxpkt_in_trans -= req->pkt_count;
			}
		}

		// Next.
		rd_idx = (rd_idx + 1) % depth;
	}

	hwa_pktpgr_req_ring_start(dev, req_ring->name, pp_ring);
}

// XXX, Need HW enhancement: Can HW do it for SW?
// It's really bad to handle hwa_txfifo_map_pkts by pull-map-bme/push.
// Think about this case, HW shadow has a lot of mpdu per FIFO and DDBM
// is low then the pull-map-push will take log time.
// 1. Stop PageOutReq, PageInReq rings in hwa_pktpgr_map_pkts_prep()
// 2. Shadow map, both SW and HW in hwa_txfifo_map_pkts() per FIFO.
// 3. PageInRsp, PageOutReq map in hwa_pktpgr_ring_map_pkts()
// 4. Start PageOutReq, PageInReq rings in hwa_pktpgr_map_pkts_done()
void
hwa_pktpgr_map_pkts_prep(struct hwa_dev *dev)
{
	// Stop PageOutReq and PageInReq rings because we are going
	// to pull-map-push HW shadow [OvfQ]

	// Stop PageOutReq
	hwa_pktpgr_req_ring_stop(dev, "PageOutReq", hwa_pktpgr_pageout_req_ring);

	// Stop PageInReq
	hwa_pktpgr_req_ring_stop(dev, "PageInReq", hwa_pktpgr_pagein_req_ring);
}

// Start PageOutReq and PageInReq rings
void
hwa_pktpgr_map_pkts_done(struct hwa_dev *dev)
{
	// Clear PageInReq ring stop bit
	hwa_pktpgr_req_ring_start(dev, "PageInReq", hwa_pktpgr_pagein_req_ring);

	// Clear PageOutReq ring stop bit
	hwa_pktpgr_req_ring_start(dev, "PageOutReq", hwa_pktpgr_pageout_req_ring);
}

// Return count of processed mpdu
static int
hwa_pktpgr_tag_pull_process(hwa_dev_t *dev, int mpdu_cnt, uint32 fifo_idx,
	void *cb, void *ctx, uint16 *pkt_mapid, bool last)
{
	uint32 map_result;
	hwa_pktpgr_t *pktpgr;
	hwa_pp_lbuf_t *txlbuf;
	struct spktq *tag_pull_q;
	struct spktq dirty_q, clean_q;
	map_pkts_cb_fn map_pkts_cb;
	int processed;
	//int idx = 0;

	// Audit pre-conditions
	HWA_AUDIT_DEV(dev);

	// Setup local
	map_pkts_cb = (map_pkts_cb_fn)cb;
	pktpgr = &dev->pktpgr;
	tag_pull_q = &pktpgr->tag_pull_q;
	if (HWAREV_GE(dev->corerev, 132)) {
		// Init the temporary q
		spktq_init(&dirty_q, PKTQ_LEN_MAX);
		spktq_init(&clean_q, PKTQ_LEN_MAX);
	}

	// Check.
	HWA_ASSERT(spktq_n_pkts(tag_pull_q) == mpdu_cnt);
	if (spktq_empty(tag_pull_q)) {
		HWA_ERROR(("Why, tag_pull_q is empty\n"));
		return 0;
	}

	HWA_ERROR(("  Processing <%d> pulled packets, head<%p>\n",
		spktq_n_pkts(tag_pull_q), spktq_peek_head(tag_pull_q)));

	// Map packets in tag_pull_q
	// Bypass tail if last is FALSE, because I need to use tail
	// pkt_mapid as next PULL key.
	*pkt_mapid = PKTMAPID(spktq_peek_tail(tag_pull_q));
	txlbuf = spktq_deq(tag_pull_q);
	while (txlbuf) {
		//HWA_ERROR(("  [mpdu-%d]: txlbuf<0x%p> pkt_mapid<%d>\n",
		//	idx++, txlbuf, PKTMAPID(txlbuf)));

		// Do map cb
		map_result = map_pkts_cb(ctx, (void*)txlbuf);

		// Free, not handle, assert it.
		HWA_ASSERT(!(map_result & MAP_PKTS_CB_FREE));

		if (map_result & MAP_PKTS_CB_DIRTY) {
			// Dirty, push back
			//HWA_ERROR(("  Map result is dirty, push pkttag pkt_mapid<%d>\n",
			//	PKTMAPID(txlbuf)));

			if (HWAREV_IS(dev->corerev, 131)) {
				uint32 loaddr;
				haddr64_t haddr64;

				// For 6715A0, I cannot use PUSH to update the change because
				// HW PUSH will clear the link pointer of tail packet in the
				// PUSH req. It breaks the HW shadow list. CRBCAHWA-640

				// 6175A0 SW WAR: assume map_pkts_cb only touch PKTTAG.
				// Use BME to sync PKTTAG for dirty packet.
				// HDBM base
				haddr64 = pktpgr->hostpktpool_haddr64;
				loaddr = HADDR64_LO(haddr64);
				// Plus offset of txlbuf PKTTAG
				loaddr += PKTMAPID(txlbuf) * sizeof(hwa_pp_lbuf_t);
				loaddr += HWA_PP_PKTTAG_BYTES;
				HADDR64_LO_SET(haddr64, loaddr);
				if (hme_d2h_xfer(PKTTAG(txlbuf), &haddr64, HWA_PP_PKTTAG_BYTES,
					TRUE, HME_USER_NOOP) < 0) {
					HWA_ERROR(("ERROR: hme_d2h_xfer failed\n"));
					HWA_ASSERT(0);
				}
				// Free to DDBM by DDBM buffer index.
				// Note: pkt_mapid is not equal to DDBM buffer index.
				// Both HWA_PP_PAGEMGR_FREE_DDBM and
				// HWA_PP_PAGEMGR_FREE_DDBM_LINK are verified.
				hwa_pktpgr_dbm_free(dev, HWA_DDBM, HWA_TABLE_INDX(hwa_pp_lbuf_t,
					pktpgr->dnglpktpool_mem, txlbuf));
			} else {
				// 6715B0-RC3, add PUSH_PKTTAG command.
				spktq_enq(&dirty_q, txlbuf);
			}
		}
		else {
			// Clean, just free to DDBM
			//HWA_ERROR(("  Map result is clean, free to DDBM pkt_mapid<%d>\n",
			//	PKTMAPID(txlbuf)));

			// Free to DDBM by DDBM buffer index.
			// Note: pkt_mapid is not equal to DDBM buffer index.
			// Both HWA_PP_PAGEMGR_FREE_DDBM and
			// HWA_PP_PAGEMGR_FREE_DDBM_LINK are verified.
			if (HWAREV_GE(dev->corerev, 132)) {
				spktq_enq(&clean_q, txlbuf);
			} else {
				hwa_pktpgr_dbm_free(dev, HWA_DDBM, HWA_TABLE_INDX(hwa_pp_lbuf_t,
					pktpgr->dnglpktpool_mem, txlbuf));
			}
		}

		// next
		txlbuf = spktq_deq(tag_pull_q);

		// A0: Because the next pull(if any) needs pkt_mapid as a key, the
		// current last packet can be a key for next pull.
		// Ignore current last packet process.
		// B0: Althrough we can use the same logic as above but let use
		// HWA_PP_PAGEMGR_PULL_KPFL_LINK to verify the design.
		// The PKTLINK of tail txlbuf is HDBM lo_addr space.
		// Use hwa_pktpgr_hdbm_ptr2idx to get next pkt_mapid for next pull if any.
		// B0: Since I use spktq to store PULLed packets, so I cannot use
		// HWA_PP_PAGEMGR_PULL_KPFL_LINK
		if (!last && spktq_empty(tag_pull_q)) {
			// Clean, just free the tail to DDBM.
			//HWA_ERROR(("  Not last call, use tail pkt_mapid<%d> for next PULL."
			//	" Bypass it.\n", PKTMAPID(txlbuf)));
			// Free to DDBM
			if (HWAREV_GE(dev->corerev, 132)) {
				spktq_enq(&clean_q, txlbuf);
			} else {
				hwa_pktpgr_dbm_free(dev, HWA_DDBM, HWA_TABLE_INDX(hwa_pp_lbuf_t,
					pktpgr->dnglpktpool_mem, txlbuf));
			}
			break;
		}
	}

	HWA_ASSERT(spktq_empty(tag_pull_q));

	processed = last ? mpdu_cnt : (mpdu_cnt-1);

	if (HWAREV_GE(dev->corerev, 132)) {
		uint16 n_pkts;

		// Push dirty pkttag
		n_pkts = spktq_n_pkts(&dirty_q);
		if (n_pkts) {
			hwa_pktpgr_push_pkttag_req(dev, fifo_idx,
				HWA_PP_CMD_TAGGED, spktq_peek_head(&dirty_q),
				spktq_peek_tail(&dirty_q), n_pkts, n_pkts);
		}

		// Free clean
		n_pkts = spktq_n_pkts(&clean_q);
		if (n_pkts) {
			hwa_pktpgr_free_ddbm_pkt(dev, spktq_peek_head(&clean_q),
				spktq_peek_tail(&clean_q), n_pkts);
		}

		// Deinit the temporary q
		spktq_deinit(&dirty_q);
		spktq_deinit(&clean_q);
	}

	return processed;
}

// HW shadow map
// PULL-MAP-PUSH OvfQ
// Need to consider the DDBM availability
#define PULL_MPDU_CNT_MAX 64
void
hwa_pktpgr_map_pkts(hwa_dev_t *dev, uint32 fifo_idx, void *cb, void *ctx)
{
	hwa_pktpgr_t *pktpgr;
	hwa_txfifo_ovflwqctx_t ovflwq;
	hwa_pp_pagemgr_req_pull_t req;
	int processed, trans_id, ovfq_mpdu_cnt, pull_mpdu_cnt, req_loop;
	uint16 pkt_mapid, pull_mpdu_cnt_max, ppddavailcnt;

	// Audit pre-conditions
	HWA_AUDIT_DEV(dev);
	HWA_ASSERT(fifo_idx < HWA_TX_FIFOS);

	// Setup local
	pktpgr = &dev->pktpgr;

	// Get OvfQ
	ovfq_mpdu_cnt = hwa_txfifo_get_ovfq(dev, fifo_idx, &ovflwq);
	if (ovfq_mpdu_cnt <= 0)
		return;

	HWA_ERROR(("%d MPDU in HW shadow.\n", ovfq_mpdu_cnt));

	// Get first pkt_mapid, for HDBM buffer index is equal to pkt_mapid
	pkt_mapid = hwa_pktpgr_hdbm_ptr2idx(pktpgr, PHYSADDR64LO(ovflwq.pktq_head));

	// Get DDBM available count
	ppddavailcnt = hwa_pktpgr_dbm_availcnt(dev, HWA_DDBM);
	if (ppddavailcnt <= 1) {
		HWA_ERROR(("Too bad: ppddavailcnt<%d>, force to 2.\n", ppddavailcnt));
		ppddavailcnt = 2; // 2 at least
	}

	// decide pull_mpdu_cnt
	if (ppddavailcnt > PULL_MPDU_CNT_MAX)
		pull_mpdu_cnt_max = PULL_MPDU_CNT_MAX;
	else
		pull_mpdu_cnt_max = ppddavailcnt;
	HWA_ASSERT(pull_mpdu_cnt_max);

	pull_mpdu_cnt = MIN(ovfq_mpdu_cnt, pull_mpdu_cnt_max);
	while (ovfq_mpdu_cnt) {
		// Request tagged PULL
		// B0: Since I use spktq to store PULLed packets, so
		// I cannot use HWA_PP_PAGEMGR_PULL_KPFL_LINK
		req.trans    = HWA_PP_PAGEMGR_PULL;
		req.pkt_count    = pull_mpdu_cnt; // mpdu_count base actually.
		req.pkt_mapid    = pkt_mapid;
		req.tagged       = HWA_PP_CMD_TAGGED;

		req_loop = HWA_LOOPCNT;

req_again:
		trans_id = hwa_pktpgr_request(dev, hwa_pktpgr_pagemgr_req_ring, &req);
		if (trans_id == HWA_FAILURE) {
			if (--req_loop > 0) {
				OSL_DELAY(10);
				goto req_again;
			}
			HWA_ERROR(("Tagged PULL_REQ failure: pkts<%d> pkt_mapid %3u",
				pull_mpdu_cnt, pkt_mapid));
			HWA_ASSERT(0);
			return;
		}
		HWA_ERROR(("PULL HW shadow pkts<%d> pkt_mapid %3u\n", pull_mpdu_cnt, pkt_mapid));

		dev->pktpgr.tag_pull_req++;

		// Wait for tag PULL resp.
		hwa_pktpgr_tag_pull_wait_to_finish(dev);

		// Process tag PULL resp, PUSH dirty else free to DDBM
		processed = hwa_pktpgr_tag_pull_process(dev,
			pull_mpdu_cnt, fifo_idx, cb, ctx, &pkt_mapid,
			(pull_mpdu_cnt == ovfq_mpdu_cnt));
		HWA_ERROR(("Processed <%d> HW shadow packets on fifo_idx<%d>\n",
			processed, fifo_idx));

		// Next PULL
		ovfq_mpdu_cnt -= processed;

		// Decide new pull_mpdu_cnt
		ppddavailcnt = hwa_pktpgr_dbm_availcnt(dev, HWA_DDBM);
		if (ppddavailcnt <= 1) {
			HWA_ERROR(("Too bad: ppddavailcnt<%d>, force to 2 for new pull.\n",
				ppddavailcnt));
			ppddavailcnt = 2; // 2 at least
		}
		if (ppddavailcnt > PULL_MPDU_CNT_MAX)
			pull_mpdu_cnt_max = PULL_MPDU_CNT_MAX;
		else
			pull_mpdu_cnt_max = ppddavailcnt;
		HWA_ASSERT(pull_mpdu_cnt_max);
		pull_mpdu_cnt = MIN(ovfq_mpdu_cnt, pull_mpdu_cnt_max);
	}

	// Wait for PUSH complete before return.
	hwa_pktpgr_tag_push_wait_to_finish(dev);
}

static void
hwa_pktpgr_pageout_req_ring_map_pkts(hwa_dev_t *dev, void *cb, void *ctx)
{
	uint16 depth;
	int i, wr_idx, rd_idx;
	hwa_ring_t *req_ring;
	hwa_pp_cmd_t *req_cmd;
	hwa_pp_lbuf_t *txlbuf;
	hwa_pp_pageout_req_pktlist_t *pktlist;
	hwa_pp_pageout_req_pktlocal_t *pktlocal;
	map_pkts_cb_fn map_pkts_cb = (map_pkts_cb_fn)cb;

	HWA_FTRACE(HWApp);
	HWA_ASSERT(dev != (struct hwa_dev *)NULL);

	// Setup local
	req_ring = &dev->pktpgr.pageout_req_ring;

	// Is empty ? hwa_ring_is_cons_all will sync HW read.
	if (hwa_ring_is_cons_all(req_ring))
		return;

	wr_idx = HWA_RING_STATE(req_ring)->write;
	rd_idx = HWA_RING_STATE(req_ring)->read;
	depth = req_ring->depth;

	HWA_ERROR(("Walk \"PO>\" ring for packet mapping. R:W[%d:%d]\n", rd_idx, wr_idx));

	while (rd_idx != wr_idx) {
		// Fetch req cmd
		req_cmd = HWA_RING_ELEM(hwa_pp_cmd_t, req_ring, rd_idx);
		pktlist = &req_cmd->pageout_req_pktlist;
		pktlocal = &req_cmd->pageout_req_pktlocal;
		// HWA_PP_PAGEOUT_PKTLOCAL ??
		if (pktlist->trans != HWA_PP_PAGEOUT_PKTLIST_NR &&
			pktlist->trans != HWA_PP_PAGEOUT_PKTLIST_WR &&
			pktlist->trans != HWA_PP_PAGEOUT_PKTLOCAL)
			goto next;

		if (pktlist->trans == HWA_PP_PAGEOUT_PKTLIST_NR ||
			pktlist->trans == HWA_PP_PAGEOUT_PKTLIST_WR) {
			// Sanity check
			HWA_ASSERT(pktlist->pkt_count);
			HWA_ASSERT(pktlist->mpdu_count);
			HWA_ASSERT(PKTLINK(pktlist->pktlist_tail) == 0U);
			HWA_ASSERT(pktlist->pktlist_head != 0U);

			// Walk through list.
			txlbuf = (hwa_pp_lbuf_t *)pktlist->pktlist_head;

			HWA_ERROR(("%s: Found one pktlist<0x%p> pkt_mapid<%d> count<%d>\n",
				__FUNCTION__, txlbuf, PKTMAPID(txlbuf), pktlist->mpdu_count));
			for (i = 0; i < pktlist->mpdu_count; i++) {
				HWA_TRACE(("  mapped txlbuf<0x%p> pkt_mapid<%d>\n",
					txlbuf, PKTMAPID(txlbuf)));
				// Do map cb
				(void)map_pkts_cb(ctx, (void*)txlbuf);
				// Next one
				txlbuf = (hwa_pp_lbuf_t *)PKTLINK(txlbuf);
			}
		} else {
			// Sanity check
			HWA_ASSERT(pktlocal->pkt_local != 0U);
			// Walk through list.
			txlbuf = (hwa_pp_lbuf_t *)pktlocal->pkt_local;
			HWA_TRACE(("  mapped local txlbuf<0x%p> pkt_mapid<%d>\n",
				txlbuf, PKTMAPID(txlbuf)));
			(void)map_pkts_cb(ctx, (void*)txlbuf);
		}
next:
		// Next.
		rd_idx = (rd_idx + 1) % depth;
	}
}

static void
hwa_pktpgr_pagein_rsp_ring_map_pkts(hwa_dev_t *dev, void *cb, void *ctx)
{
	uint16 depth;
	int i, wr_idx, rd_idx;
	hwa_ring_t *rsp_ring;
	hwa_pp_cmd_t *rsp_cmd;
	hwa_pp_lbuf_t *txlbuf;
	hwa_pp_pagein_rsp_txstatus_t *txstatus;
	map_pkts_cb_fn map_pkts_cb = (map_pkts_cb_fn)cb;

	HWA_FTRACE(HWApp);
	HWA_ASSERT(dev != (struct hwa_dev *)NULL);

	// Setup local
	rsp_ring = &dev->pktpgr.pagein_rsp_ring;

	// Is empty?
	hwa_ring_cons_get(rsp_ring); // Update H2S ring WR
	if (hwa_ring_is_empty(rsp_ring))
		return;

	wr_idx = HWA_RING_STATE(rsp_ring)->write;
	rd_idx = HWA_RING_STATE(rsp_ring)->read;
	depth = rsp_ring->depth;

	HWA_ERROR(("Walk \"PI<\" ring for packet mapping\n"));

	while (rd_idx != wr_idx) {
		// Fetch rsp cmd
		rsp_cmd = HWA_RING_ELEM(hwa_pp_cmd_t, rsp_ring, rd_idx);
		txstatus = &rsp_cmd->pagein_rsp_txstatus;
		if (txstatus->trans != HWA_PP_PAGEIN_TXSTATUS) {
			HWA_TRACE(("%s: goto next trans<%d>\n", __FUNCTION__, txstatus->trans));
			goto next;
		}

		if (txstatus->pkt_count == 0 || txstatus->mpdu_count == 0) {
			HWA_ERROR(("%s: pkt_count<%d> mpdu_count<%d>\n", __FUNCTION__,
				txstatus->pkt_count, txstatus->mpdu_count));
			goto next;
		}

		// Sanity check
		HWA_ASSERT(PKTLINK(txstatus->pktlist_tail) == 0U);
		HWA_ASSERT(txstatus->pktlist_head != 0U);

		// Walk through list.
		txlbuf = (hwa_pp_lbuf_t *)txstatus->pktlist_head;

		HWA_ERROR(("%s: Found one pktlist<0x%p> pkt_mapid<%d> count<%d>\n",
			__FUNCTION__, txlbuf, PKTMAPID(txlbuf), txstatus->mpdu_count));
		for (i = 0; i < txstatus->mpdu_count; i++) {
			HWA_TRACE(("%s: mapped txlbuf<0x%p> pkt_mapid<%d>\n",
				__FUNCTION__, txlbuf, PKTMAPID(txlbuf)));
			if (PKTISMGMTTXPKT(dev->osh, txlbuf))
				(void)map_pkts_cb(ctx, (void*)PKTLOCAL(dev->osh, txlbuf));
			else
				(void)map_pkts_cb(ctx, (void*)txlbuf);
			txlbuf = (hwa_pp_lbuf_t *)PKTLINK(txlbuf);
		}
next:
		// Next.
		rd_idx = (rd_idx + 1) % depth;
	}
}

void
hwa_pktpgr_ring_map_pkts(struct hwa_dev *dev, void *cb, void *ctx)
{
	// Audit pre-conditions
	HWA_AUDIT_DEV(dev);

	// 1. PageInRsp map
	hwa_pktpgr_pagein_rsp_ring_map_pkts(dev, cb, ctx);

	// 2. PageOutReq map
	hwa_pktpgr_pageout_req_ring_map_pkts(dev, cb, ctx);
}

#endif /* HWA_PKTPGR_BUILD */
