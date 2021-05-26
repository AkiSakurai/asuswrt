/*
 * HWA library routines that are not specific to PCIE or MAC facing blocks.
 *
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
#include <bcmutils.h>
#include <siutils.h>
#include <bcmdevs.h>
#ifdef HWA_BUS_BUILD
#include <rte_cfg.h>
#include <pciedev.h>
#endif // endif
#include <bcmmsgbuf.h>
#include <hwa_lib.h>
#include <hndsoc.h>
#ifdef HNDBME
#include <hndbme.h>
#endif // endif
#include <bcmhme.h>
#include <wlc.h>
#include <wlc_dump.h>
#ifndef DONGLEBUILD
#include <pcicfg.h>
#include <wl_export.h>
#endif /* !DONGLEBUILD */

/*
 * -----------------------------------------------------------------------------
 * Section: hwa_ring_t
 * -----------------------------------------------------------------------------
 *
 * Uses the bcm_ring abstraction to implement a HWA S2H or H2S ring interface.
 * Extends the bcm_ring library with the capability to fetch and update WR or RD
 * indices from HWA registers.Addresses of these WR and RD registers are saved
 * in the hwa_ring_t abstraction.
 *
 * -----------------------------------------------------------------------------
 */
void // Initialize a HWA circular ring
hwa_ring_init(hwa_ring_t *ring, const char *name,
	uint8 block_id, uint8 ring_dir, uint8 ring_num,
	uint16 depth, void *memory, hwa_reg_addr_t reg_wr, hwa_reg_addr_t reg_rd)
{
	HWA_TRACE(("%s %s <%p>[%x::%u:%u] memory[%p:%u] wr[%p] rd[%p]\n",
		name, __FUNCTION__, ring, block_id, ring_dir, ring_num,
		memory, depth, reg_wr, reg_rd));

	HWA_RING_ASSERT(ring != HWA_RING_NULL);
	HWA_RING_ASSERT((ring_dir == HWA_RING_S2H) || (ring_dir == HWA_RING_H2S));
	HWA_RING_ASSERT(depth != 0);
	HWA_RING_ASSERT(memory != (void*)NULL);

	bcm_ring_init(HWA_RING_STATE(ring));

	ring->reg_wr = reg_wr; // "address" of HWA WR register
	ring->reg_rd = reg_rd; // "address" of HWA RD register
	ring->memory = memory; // ring memory
	ring->depth  = depth;  // number of elements in the ring

	// Construct a unique ring identifier - used for debug only
	ring->id.u16 = HWA_RING_ID(block_id, ring_dir, ring_num);
	snprintf(ring->name, HWA_RING_NAME_SIZE, "%s", name);

	// Reset HW registers and SW state
	if (ring_dir == HWA_RING_S2H)
		HWA_WR_REG16_ADDR(ring->name, ring->reg_wr, 0U);
	else
		HWA_WR_REG16_ADDR(ring->name, ring->reg_rd, 0U);
	HWA_RING_STATE(ring)->write = 0;
	HWA_RING_STATE(ring)->read = 0;

} // hwa_ring_init

void // Finish using a ring, Reset HW registers to prevent accidental use
hwa_ring_fini(hwa_ring_t *ring)
{
	if (ring == NULL)
		return;

	// Reset HW registers and SW state
	HWA_WR_REG_ADDR(ring->name, ring->reg_wr, 0U);
	HWA_WR_REG_ADDR(ring->name, ring->reg_rd, 0U);
	memset(ring, 0, sizeof(*ring));

} // hwa_ring_fini

void // Debug dump a hwa_ring_t
hwa_ring_dump(hwa_ring_t *ring, struct bcmstrbuf *b, const char *prefix)
{
	if (ring == HWA_RING_NULL || ring->memory == NULL)
		return;

	HWA_BPRINT(b, "%-13s ring<%s,%p>[%x::%u:%u] memory<%p:%u>"
		" wr[%p::%4u][%4u] rd[%p:%4u][%4u]\n", prefix,
		ring->name, ring,
		ring->id.block_id, ring->id.ring_dir, ring->id.ring_num,
		ring->memory, ring->depth,
		ring->reg_wr, HWA_RD_REG_ADDR(ring->name, ring->reg_wr),
		HWA_RING_STATE(ring)->write,
		ring->reg_rd, HWA_RD_REG_ADDR(ring->name, ring->reg_rd),
		HWA_RING_STATE(ring)->read);

} // hwa_ring_dump

#ifdef HWA_DPC_BUILD
/*
 * -----------------------------------------------------------------------------
 * Section: HWA Internal DMA Engines
 * -----------------------------------------------------------------------------
 */
void
BCMATTACHFN(hwa_dma_attach)(hwa_dev_t *dev)
{
	uint32 v32, tx32, rx32, i;
	hwa_regs_t *regs;

	HWA_FTRACE(HWAde);

	HWA_ASSERT(dev != (hwa_dev_t*)NULL);

	regs = dev->regs;

	v32 = HWA_RD_REG_NAME(HWAde, regs, dma, corecontrol);
	dev->dma.arbitration = BCM_GBF(v32, HWA_DMA_CORECONTROL_ARBTYPE);
	HWA_TRACE(("%s HWA[%d] DMA arbitration<%s>\n",
		HWAde, dev->unit, (dev->dma.arbitration == 0) ? "RR" : "FXD"));

	v32 = HWA_RD_REG_NAME(HWAde, regs, dma, corecapabilities);
	dev->dma.channels_max =
		BCM_GBF(v32, HWA_DMA_CORECAPABILITIES_CHANNELCNT);
	dev->dma.burst_length_max =
		BCM_GBF(v32, HWA_DMA_CORECAPABILITIES_MAXBURSTLEN);
	dev->dma.outstanding_rds_max =
		BCM_GBF(v32, HWA_DMA_CORECAPABILITIES_MAXRDOUTSTANDING);

	/* HWA_DMA_CHANNEL1 burst length is adjustable */
	dev->dma.channels = HWA_DMA_CHANNELS_MAX;
	dev->dma.burst_length[HWA_DMA_CHANNEL0].tx = DMA_BL_64;
	dev->dma.burst_length[HWA_DMA_CHANNEL0].rx = DMA_BL_64;
	dev->dma.burst_length[HWA_DMA_CHANNEL1].tx = DMA_BL_64;
	dev->dma.burst_length[HWA_DMA_CHANNEL1].rx = DMA_BL_64;
	dev->dma.outstanding_rds = DMA_MR_1;
	HWA_BUS_EXPR({
		if (HWAREV_GE(dev->corerev, 130)) {
			dev->dma.burst_length[HWA_DMA_CHANNEL1].tx =
				pcie_h2ddma_tx_get_burstlen(dev->pciedev);
			dev->dma.burst_length[HWA_DMA_CHANNEL1].rx =
				pcie_h2ddma_rx_get_burstlen(dev->pciedev);
			/* HWA_DMA_CHANNEL0 has same as HWA_DMA_CHANNEL1 */
			if (HWAREV_GE(dev->corerev, 131)) {
				dev->dma.burst_length[HWA_DMA_CHANNEL0].tx =
					dev->dma.burst_length[HWA_DMA_CHANNEL1].tx;
				dev->dma.burst_length[HWA_DMA_CHANNEL0].rx =
					dev->dma.burst_length[HWA_DMA_CHANNEL1].rx;
			}
		}
	});
	if (HWAREV_GE(dev->corerev, 130))
		dev->dma.outstanding_rds = DMA_MR_2;

	/* channelCnt contains one less than the number of DMA channel
	 * pairs supported by this device
	 */
	HWA_ASSERT(dev->dma.channels <= (dev->dma.channels_max + 1));

	/* rev131 can support higher burst length at CH0 */
	if (HWAREV_LE(dev->corerev, 130)) {
		HWA_ASSERT(dev->dma.burst_length[HWA_DMA_CHANNEL0].tx <= dev->dma.burst_length_max);
		HWA_ASSERT(dev->dma.burst_length[HWA_DMA_CHANNEL0].rx <= dev->dma.burst_length_max);
	}
	HWA_ASSERT(dev->dma.outstanding_rds <= dev->dma.outstanding_rds_max);

	HWA_ERROR(("%s HWA[%d] channels[%u:%u] burstlen[%u:%u:%u:%u:%u] outst_rds[%u:%u]\n",
		HWAde, dev->unit, dev->dma.channels, dev->dma.channels_max,
		dev->dma.burst_length[HWA_DMA_CHANNEL0].tx,
		dev->dma.burst_length[HWA_DMA_CHANNEL0].rx,
		dev->dma.burst_length[HWA_DMA_CHANNEL1].tx,
		dev->dma.burst_length[HWA_DMA_CHANNEL1].rx,
		dev->dma.burst_length_max,
		dev->dma.outstanding_rds, dev->dma.outstanding_rds_max));

	/*
	 * HWA DMA engines are not programmed by SW, and no INTs are expected
	 * other than errors. Likewise the HWA DMA engines are not truly descriptor
	 * rings with base address etc. HWA cores directly program these DMA engines
	 * bypassing the prefetch logic and wait upon DMA dones.
	 *
	 * Per channel, descErr, dataErr, descProtoErr and rcvDescUf? may be enabled
	 * for HWA debug. SW may enable this and monitor interrupt status for DBG.
	 */
	v32 = 0;
	HWA_WR_REG_NAME(HWAde, regs, dma, intcontrol, v32);

	/*
	 * SW has no control on which DMA channel is used for transfers to/from
	 * host DDR (i.e over PCIE) and for transfers to/from Device SysMem.
	 * As such, settings like: Coherency, BurstAlignEn, AddrExt on XmtCtrl and
	 * RcvCtrl are not useful.
	 *
	 * Likewise, DMA engines' descriptor prefetch does not apply.
	 */
	for (i = 0; i < dev->dma.channels; i++) {

		tx32 = (0U
			// | D64_XC_XE // "XmtEn" will be done in hwa_dma_enable()
			// | D64_XC_SE "SuspEn"
			// | D64_XC_LE "LoopbackEn"
			// | D64_XC_FL "Flush"
			// | D64_XC_BE // "BurstAlignEn"
			| BCM_SBF(dev->dma.outstanding_rds, D64_XC_MR)
			// | D64_XC_CS "ChannelSwitchEn"
			// | D64_XC_PD "PtyChkDisable"
			// | D64_XC_SA "SelectActive"
			// | D64_XC_AE "AddrExt" ... NIC Mode impacts???
			| BCM_SBF(dev->dma.burst_length[i].tx, D64_XC_BL)
			| BCM_SBF(DMA_PC_0, D64_XC_PC) // "PrefetchCtl" = 0
			| BCM_SBF(DMA_PT_1, D64_XC_PT) // "PrefetchThresh" = 0
			| BCM_SBIT(D64_XC_CO) // Coherent
			| 0U);
		HWA_WR_REG_NAME(HWAde, regs, dma, channels[i].tx.control, tx32);

		// XXX: D64_RC_WC is not working in Veloce, disable it.
		// Enabling WaitForComplete is only required for specififc DMA transfers
		// to Host DDR over PCIE, to avoid readback traffic.
		rx32 = (0U
			// | D64_RC_RE // "RcvEn" will be done in hwa_dma_enable()
			| BCM_SBF(0, D64_RC_RO) // "RcvOffset" = 0
			// | D64_RC_FM "FIFOMode"
			// | D64_RC_SH "SepRxHdrDescrEn"
			// | D64_RC_OC "OverflowCont"
			// | D64_RC_PD "PtyChkDisable"
			// | D64_RC_WC // "WaitForComplete" ... bad, forces a readback FIXME!!!
			HWA_PKTPGR_EXPR(| BCM_SBIT(D64_RC_WAITCMP)) // WaitForComplete
			// | D64_RC_SA "SelectActive"
			// | D64_RC_GE "GlomEn"
			// | D64_RC_AE "AddrExt" ... NIC Mode impacts???
			| BCM_SBF(dev->dma.burst_length[i].rx, D64_RC_BL)
			| BCM_SBF(DMA_PC_0, D64_RC_PC) // "PrefetchCtl" = 0
			| BCM_SBF(DMA_PT_1, D64_RC_PT) // "PrefetchThresh" = 0
			| BCM_SBIT(D64_RC_CO) // Coherent
			HWA_PKTPGR_EXPR(| BCM_SBIT(D64_RC_WCPD)) // WaitForComplete per Descriptor
			| 0U);
		HWA_WR_REG_NAME(HWAde, regs, dma, channels[i].rx.control, rx32);

	} // for dma.channels

	/*
	 * XXX Ignore dma::powercontrol::poweronrequest register
	 * v32 = BCM_SBIT(HWA_DMA_POWERCONTROL_POWERONREQUEST);
	 * HWA_WR_REG_NAME(HWAde, regs, dma, powercontrol, v32);
	 */

	// DMA channels not yet enabled, see hwa_dma_enable()
	dev->dma.enabled = FALSE;

} // hwa_dma_attach

void // Enable HWA DMA channels
hwa_dma_enable(hwa_dma_t *dma)
{
	hwa_dev_t *dev;
	hwa_regs_t *regs;
	uint32 i, v32, tx32, rx32;

	HWA_FTRACE(HWAde);

	dev = HWA_DEV(dma);
	regs = dev->regs;

	for (i = 0; i < dma->channels; i++) {
		v32 = HWA_RD_REG_NAME(HWAde, regs, dma, channels[i].tx.control);
		tx32 = v32 | D64_XC_XE;
		HWA_WR_REG_NAME(HWAde, regs, dma, channels[i].tx.control, tx32);

		v32 = HWA_RD_REG_NAME(HWAde, regs, dma, channels[i].rx.control);
		rx32 = v32 | D64_RC_RE;
		HWA_WR_REG_NAME(HWAde, regs, dma, channels[i].rx.control, rx32);
	}

	dma->enabled = TRUE;

} // hwa_dma_enable

void // Adjust HWA DMA Rx burst length
hwa_dma_rx_burstlen_adjust(hwa_dma_t *dma)
{
	hwa_dev_t *dev;
	hwa_regs_t *regs;
	uint32 rx32;
	uint8 rx_bl;

	/* Keep original burst length setting */
	if (HWAREV_LT(dev->corerev, 130))
		return;

	HWA_FTRACE(HWAde);

	dev = HWA_DEV(dma);
	regs = dev->regs;

	/* XXX: set rxburstlen to minimum of MPS configured during
	* enumeration and the burst length supported by chip.
	*/
	rx_bl = dma->burst_length[HWA_DMA_CHANNEL1].rx;
	rx_bl = MIN(rx_bl, DMA_BL_128);
	dma->burst_length[HWA_DMA_CHANNEL1].rx = rx_bl;

	rx32 = HWA_RD_REG_NAME(HWAde, regs, dma, channels[HWA_DMA_CHANNEL1].rx.control);
	rx32 = BCM_CBF(rx32, D64_RC_BL);
	rx32 |= BCM_SBF(rx_bl, D64_RC_BL);
	HWA_WR_REG_NAME(HWAde, regs, dma, channels[HWA_DMA_CHANNEL1].rx.control, rx32);

	HWA_ERROR(("%s adjust channels[1] rx burstlen[%u]\n", HWAde, rx_bl));

	/* HWA_DMA_CHANNEL0 has same as HWA_DMA_CHANNEL1 */
	if (HWAREV_GE(dev->corerev, 131)) {
		dma->burst_length[HWA_DMA_CHANNEL0].rx = rx_bl;

		rx32 = HWA_RD_REG_NAME(HWAde, regs, dma, channels[HWA_DMA_CHANNEL0].rx.control);
		rx32 = BCM_CBF(rx32, D64_RC_BL);
		rx32 |= BCM_SBF(rx_bl, D64_RC_BL);
		HWA_WR_REG_NAME(HWAde, regs, dma, channels[HWA_DMA_CHANNEL0].rx.control, rx32);

		HWA_ERROR(("%s adjust channels[0] rx burstlen[%u]\n", HWAde, rx_bl));
	}

} // hwa_dma_rx_burstlen_adjust

hwa_dma_status_t // Get the HWA DMA Xmt and Rcv status for a given channel
hwa_dma_channel_status(hwa_dma_t *dma, uint32 channel)
{
	uint32 v32;
	hwa_dev_t *dev;
	hwa_regs_t *regs;
	hwa_dma_status_t status;

	HWA_FTRACE(HWAde);

	HWA_ASSERT(dma != (hwa_dma_t*)NULL);
	HWA_ASSERT(channel < dma->channels);

	dev = HWA_DEV(dma);
	regs = dev->regs;

	status.u32 = 0U;
	v32 = HWA_RD_REG_NAME(HWAde, regs, dma, channels[channel].tx.status0);
	status.xmt_state = BCM_GBF(v32, HWA_DMA_XMTSTATUS0_XMTSTATE);
	v32 = HWA_RD_REG_NAME(HWAde, regs, dma, channels[channel].tx.status1);
	status.xmt_error = BCM_GBF(v32, HWA_DMA_XMTSTATUS1_XMTERR);
	v32 = HWA_RD_REG_NAME(HWAde, regs, dma, channels[channel].rx.status0);
	status.rcv_state = BCM_GBF(v32, HWA_DMA_RCVSTATUS0_RCVSTATE);
	v32 = HWA_RD_REG_NAME(HWAde, regs, dma, channels[channel].rx.status1);
	status.rcv_error = BCM_GBF(v32, HWA_DMA_RCVSTATUS1_XMTERR);

	return status;
} // hwa_dma_channel_status

#if defined(BCMDBG) || defined(HWA_DUMP)
void // Dump the dma channels state
hwa_dma_dump(hwa_dma_t *dma, struct bcmstrbuf *b, bool verbose, bool dump_regs)
{
	if (dma == (hwa_dma_t*)NULL)
		return;

	HWA_BPRINT(b, "%s channels[%u:%u] burstlen[%u:%u:%u:%u:%u] outst_rds[%u:%u] %s\n",
		HWAde, dma->channels, dma->channels_max,
		dma->burst_length[HWA_DMA_CHANNEL0].tx,
		dma->burst_length[HWA_DMA_CHANNEL0].rx,
		dma->burst_length[HWA_DMA_CHANNEL1].tx,
		dma->burst_length[HWA_DMA_CHANNEL1].rx,
		dma->burst_length_max,
		dma->outstanding_rds, dma->outstanding_rds_max,
		dma->enabled ? "ENABLED" : "DISABLED");
#if defined(WLTEST) || defined(HWA_DUMP)
	if (dump_regs)
		hwa_dma_regs_dump(dma, b);
#endif // endif
} // hwa_dma_dump

#if defined(WLTEST) || defined(HWA_DUMP)
void // Dump the dma block registers
hwa_dma_regs_dump(hwa_dma_t *dma, struct bcmstrbuf *b)
{
	uint32 i;
	hwa_dev_t *dev;
	hwa_regs_t *regs;

	if (dma == (hwa_dma_t*)NULL)
		return;

	dev = HWA_DEV(dma);
	regs = dev->regs;

	HWA_BPRINT(b, "%s HWA[%d] registers[%p] offset[0x%04x]\n",
		HWAde, dev->unit, &regs->dma, (uint32)OFFSETOF(hwa_regs_t, dma));
	HWA_BPR_REG(b, dma, corecontrol);
	HWA_BPR_REG(b, dma, corecapabilities);
	HWA_BPR_REG(b, dma, intcontrol);
	HWA_BPR_REG(b, dma, intstatus);
	for (i = 0; i < dma->channels; i++) {
		HWA_BPR_REG(b, dma, chint[i].status);
		HWA_BPR_REG(b, dma, chint[i].mask);
	}
	for (i = 0; i < dma->channels; i++) {
		HWA_BPR_REG(b, dma, chintrcvlazy[i]);
	}
	HWA_BPR_REG(b, dma, clockctlstatus);
	HWA_BPR_REG(b, dma, workaround);
	HWA_BPR_REG(b, dma, powercontrol);
	HWA_BPR_REG(b, dma, gpioselect);
	HWA_BPR_REG(b, dma, gpiooutout);
	HWA_BPR_REG(b, dma, gpiooe);
	for (i = 0; i < dma->channels; i++) {
		HWA_BPR_REG(b, dma, channels[i].tx.control);
		HWA_BPR_REG(b, dma, channels[i].tx.ptr);
		HWA_BPR_REG(b, dma, channels[i].tx.addrlow);
		HWA_BPR_REG(b, dma, channels[i].tx.addrhigh);
		HWA_BPR_REG(b, dma, channels[i].tx.status0);
		HWA_BPR_REG(b, dma, channels[i].tx.status1);
		HWA_BPR_REG(b, dma, channels[i].rx.control);
		HWA_BPR_REG(b, dma, channels[i].rx.ptr);
		HWA_BPR_REG(b, dma, channels[i].rx.addrlow);
		HWA_BPR_REG(b, dma, channels[i].rx.addrhigh);
		HWA_BPR_REG(b, dma, channels[i].rx.status0);
		HWA_BPR_REG(b, dma, channels[i].rx.status1);
	}

} // hwa_dma_regs_dump

#endif // endif
#endif /* BCMDBG */
#endif /* HWA_DPC_BUILD */

// PKTPGR use HDBM and DDBM configured in hwa_pktpgr.c
#if !defined(HWA_PKTPGR_BUILD)

/*
 * -----------------------------------------------------------------------------
 * Section: HWA Buffer Manager, used by TxBM and RxBM
 * -----------------------------------------------------------------------------
 */
void // Configure Buffer Manager
hwa_bm_config(hwa_dev_t *dev, hwa_bm_t *bm,
	const char *name, hwa_bm_instance_t instance,
	uint16 pkt_total, uint16 pkt_size,
	uint32 pkt_base_loaddr, uint32 pkt_base_hiaddr, void *memory)
{
	HWA_ASSERT(dev != (hwa_dev_t*)NULL);
	HWA_ASSERT(bm != (hwa_bm_t*)NULL);
	HWA_ASSERT((pkt_total != 0) && (pkt_size != 0) && (pkt_base_loaddr != 0U));
	HWA_ASSERT(memory != (void*)NULL);

	bm->dev = dev;

	HWA_TRACE(("%s %s config size<%u> total<%u> base32<0x%08x,0x%08x>\n",
		HWAbm, name, pkt_size, pkt_total, pkt_base_loaddr, pkt_base_hiaddr));

	bm->pkt_total       = pkt_total;
	bm->pkt_size        = pkt_size;
	bm->pkt_base.loaddr = pkt_base_loaddr;
	bm->pkt_base.hiaddr = pkt_base_hiaddr;
	bm->memory          = memory;
	bm->instance        = instance;
	snprintf(bm->name, HWA_BM_NAME_SIZE - 1, "%s", name);

	if (bm->instance == HWA_TX_BM) {
		bm->pkt_max     = HWA_TXPATH_PKTS_MAX;
		bm->avail_sw    = bm->pkt_max;

		/* XXX, 16 is suggested number from designer,
		 * It also can be a tolance cycle for txfree ring
		 * CRBCAHWA-529: Fixed >= rev130
		 */
		if (HWAREV_IS(dev->corerev, 129)) {
			HWA_ASSERT(bm->avail_sw > 16);
			bm->avail_sw    -= 16;
		}
	} else {
		bm->pkt_max     = HWA_RXPATH_PKTS_MAX;
	}

	if (bm->instance == HWA_TX_BM) {
		/* Initial lbuf */
		HWA_TXPOST_EXPR(hwa_txpost_bm_lb_init(dev, memory, pkt_total,
			pkt_size, HWA_TXPOST_PKT_BYTES));
	}

} // hwa_bm_config

void // Enable or Disable a Buffer Manager
hwa_bm_init(hwa_bm_t *bm, bool enable)
{
	uint32 v32;
	hwa_regs_t *regs;

	HWA_FTRACE(HWAbm);

	HWA_ASSERT(bm != (hwa_bm_t*)NULL);
	HWA_ASSERT(bm->pkt_base.loaddr != 0U);

	regs = bm->dev->regs;

	v32 = // BM pkt pool sizing
		BCM_SBF(bm->pkt_total, HWA_BM_BUFFER_CONFIG_NUMBUFS) |
		BCM_SBF(bm->pkt_size, HWA_BM_BUFFER_CONFIG_BUFSIZE);

	if (bm->instance == HWA_TX_BM) {
		HWA_WR_REG_NAME(bm->name, regs, tx_bm, buffer_config, v32);
		v32 = bm->pkt_base.loaddr;
		HWA_WR_REG_NAME(bm->name, regs, tx_bm, pool_start_addr_lo, v32);
		v32 = 0U;
		HWA_WR_REG_NAME(bm->name, regs, tx_bm, pool_start_addr_hi, v32);
	} else {
		HWA_WR_REG_NAME(bm->name, regs, rx_bm, buffer_config, v32);
		v32 = bm->pkt_base.loaddr;
		HWA_WR_REG_NAME(bm->name, regs, rx_bm, pool_start_addr_lo, v32);
		v32 = bm->pkt_base.hiaddr;
		HWA_WR_REG_NAME(bm->name, regs, rx_bm, pool_start_addr_hi, v32);
	}

	v32 = (enable) ? BCM_SBIT(HWA_BM_BM_CTRL_ENABLE) : 0;
	if (bm->instance == HWA_TX_BM)
		HWA_WR_REG_NAME(bm->name, regs, tx_bm, bm_ctrl, v32);
	else
		HWA_WR_REG_NAME(bm->name, regs, rx_bm, bm_ctrl, v32);

	bm->enabled = enable;

	HWA_TRACE(("%s %s state %s\n",
		HWAbm, bm->name, (enable)? "ENABLED" : "DISABLED"));

} // hwa_bm_init

int // Allocate a SW Pkt buffer from the BM
hwa_bm_alloc(hwa_bm_t *bm, dma64addr_t *buf_addr)
{
	int index;
	uint32 v32, status;
	hwa_regs_t *regs;

	HWA_ASSERT(bm != (hwa_bm_t*)NULL);
	HWA_ASSERT(buf_addr != NULL);

	HWA_TRACE(("%s %s alloc\n", HWAbm, bm->name));

	regs = bm->dev->regs;

	if (bm->instance == HWA_TX_BM) {
		if (bm->avail_sw == 0) {
			HWA_TRACE(("%s: No SW pkt buffer\n", __FUNCTION__));
			bm->avail_sw_low++;
			return HWA_FAILURE;
		}
		v32 = HWA_RD_REG_NAME(bm->name, regs, tx_bm, alloc_index);
	}
	else {
		v32 = HWA_RD_REG_NAME(bm->name, regs, rx_bm, alloc_index);
	}
	status = BCM_GBF(v32, HWA_BM_ALLOC_INDEX_ALLOCSTATUS);

	if (status == HWA_BM_SUCCESS_SW) {
		if (bm->instance == HWA_TX_BM) {
			buf_addr->loaddr =
			  HWA_RD_REG_NAME(bm->name, regs, tx_bm, alloc_addr.loaddr);
			buf_addr->hiaddr =
			  HWA_RD_REG_NAME(bm->name, regs, tx_bm, alloc_addr.hiaddr);
			bm->avail_sw--;

			HWA_TXPOST_EXPR({
				void *buf;
				buf = HWA_UINT2PTR(void, buf_addr->loaddr);
				PKTTAGCLR(HWAPKT2LFRAG(buf));
			});
		} else {
			buf_addr->loaddr =
			  HWA_RD_REG_NAME(bm->name, regs, rx_bm, alloc_addr.loaddr);
			buf_addr->hiaddr =
			  HWA_RD_REG_NAME(bm->name, regs, rx_bm, alloc_addr.hiaddr);
		}

		index = BCM_GBF(v32, HWA_BM_ALLOC_INDEX_ALLOCINDEX);

		HWA_ASSERT(index < bm->pkt_max);
		HWA_ASSERT(index ==
			hwa_bm_ptr2idx(bm, (void*)(uintptr)buf_addr->loaddr));

		HWA_STATS_EXPR(bm->allocs++);
		HWA_TRACE(("%s %s alloc buf<%u><0x%08x, 0x%08x>\n",
			HWAbm, bm->name, index, buf_addr->loaddr, buf_addr->hiaddr));

		return index;
	}

	HWA_STATS_EXPR(bm->fails++);
	HWA_WARN(("%s %s alloc failure\n", HWAbm, bm->name));

	return HWA_FAILURE;

} // hwa_bm_alloc

int // Free a previously allocated Tx BM SW Pkt buffer by its index
hwa_bm_free(hwa_bm_t *bm, uint16 buf_idx)
{
	uint32 v32, status, loopcnt;
	hwa_regs_t *regs;

	HWA_ASSERT(bm != (hwa_bm_t*)NULL);
	HWA_ASSERT(buf_idx < bm->pkt_max);

	HWA_TRACE(("%s %s free buf_idx<%u>\n", HWAbm, bm->name, buf_idx));

	regs = bm->dev->regs;

	v32 = BCM_SBF(buf_idx, HWA_BM_DEALLOC_INDEX_DEALLOCINDEX);
	if (bm->instance == HWA_TX_BM)
		HWA_WR_REG_NAME(bm->name, regs, tx_bm, dealloc_index, v32);
	else
		HWA_WR_REG_NAME(bm->name, regs, rx_bm, dealloc_index, v32);

	for (loopcnt = 0; loopcnt < HWA_BM_LOOPCNT; loopcnt++) {
		if (loopcnt > bm->loopcnt_hwm)
			bm->loopcnt_hwm = loopcnt;

		if (bm->instance == HWA_TX_BM)
			v32 =
			    HWA_RD_REG_NAME(bm->name, regs, tx_bm, dealloc_status);
		else
			v32 =
			    HWA_RD_REG_NAME(bm->name, regs, rx_bm, dealloc_status);
		status = BCM_GBF(v32, HWA_BM_DEALLOC_STATUS_DEALLOCSTATUS);

		// WAR for HWA2.x
		if (status == HWA_BM_SUCCESS_SW) {
			HWA_STATS_EXPR(bm->frees++);
			bm->avail_sw++;
			return HWA_SUCCESS;
		}

		if (status & HWA_BM_DONEBIT) {
			HWA_ASSERT(status == HWA_BM_SUCCESS);

			HWA_STATS_EXPR(bm->frees++);
			bm->avail_sw++;
			return HWA_SUCCESS;
		}
	}

	HWA_STATS_EXPR(bm->fails++);
	HWA_WARN(("%s %s free failure\n", HWAbm, bm->name));

	HWA_ASSERT(0); // Failure in deletion of a HWA managed buffer. SW|HWA bug

	return HWA_FAILURE;

} // hwa_bm_free

uint32 // Get the runtime count of available buffers in Buffer Manager
hwa_bm_avail(hwa_bm_t *bm)
{
	uint32 v32, avail_cnt;
	hwa_regs_t *regs;

	HWA_ASSERT(bm != (hwa_bm_t*)NULL);

	regs = bm->dev->regs;

	if (bm->instance == HWA_TX_BM)
		v32 = HWA_RD_REG_NAME(bm->name, regs, tx_bm, bm_ctrl);
	else
		v32 = HWA_RD_REG_NAME(bm->name, regs, rx_bm, bm_ctrl);
	avail_cnt = BCM_GBF(v32, HWA_BM_BM_CTRL_AVAILCNT);

	HWA_TRACE(("%s %s avail<%u>\n", HWAbm, bm->name, avail_cnt));

	return avail_cnt;

} // hwa_bm_avail

#if defined(BCMDBG) || defined(HWA_DUMP)
void // Dump HWA Buffer Manager software state
hwa_bm_dump(hwa_bm_t *bm, struct bcmstrbuf *b, bool verbose, bool dump_regs)
{
	HWA_ASSERT(bm != (hwa_bm_t*)NULL);

	HWA_BPRINT(b, "%s dump num<%u> size<%u> base<0x%08x,0x%08x>\n", bm->name,
		bm->pkt_total, bm->pkt_size, bm->pkt_base.loaddr, bm->pkt_base.hiaddr);
	HWA_STATS_EXPR(
		HWA_BPRINT(b, "+ alloc<%u> frees<%u> fails<%u> avail_sw_low<%u>\n",
			bm->allocs, bm->frees, bm->fails, bm->avail_sw_low));
	HWA_STATS_EXPR(bm->avail_sw_low = 0);
#if defined(WLTEST) || defined(HWA_DUMP)
	if (dump_regs)
		hwa_bm_regs_dump(bm, b);
#endif // endif
} // hwa_bm_dump

#if defined(WLTEST) || defined(HWA_DUMP)
void // Dump HWA Buffer Manager registers
hwa_bm_regs_dump(hwa_bm_t *bm, struct bcmstrbuf *b)
{
	hwa_dev_t *dev;
	hwa_regs_t *regs;

	if ((bm == (hwa_bm_t*)NULL) || (bm->dev == (hwa_dev_t*)NULL) ||
	        (bm->dev->regs == (hwa_regs_t*)NULL))
		return;

	dev = bm->dev;
	regs = dev->regs;

	// Skip: following registers as reading has side effect.
	// alloc_index, alloc_addrlo, alloc_addrhi, dealloc_index*, dealloc_status

	if (bm->instance == HWA_TX_BM) {
		HWA_BPRINT(b, "%s %s registers[%p] offset[0x%04x]\n", HWAbm, bm->name,
			&regs->tx_bm, (uint32)OFFSETOF(hwa_regs_t, tx_bm));
		HWA_BPR_REG(b, tx_bm, buffer_config);
		HWA_BPR_REG(b, tx_bm, pool_start_addr_lo);
		HWA_BPR_REG(b, tx_bm, pool_start_addr_hi);
		HWA_BPR_REG(b, tx_bm, bm_ctrl);
	} else {
		HWA_BPRINT(b, "%s %s registers[%p] offset[0x%04x]\n", HWAbm, bm->name,
			&regs->rx_bm, (uint32)OFFSETOF(hwa_regs_t, rx_bm));
		HWA_BPR_REG(b, rx_bm, buffer_config);
		HWA_BPR_REG(b, rx_bm, pool_start_addr_lo);
		HWA_BPR_REG(b, rx_bm, pool_start_addr_hi);
		HWA_BPR_REG(b, rx_bm, bm_ctrl);
	}

} // hwa_bm_regs_dump

#endif // endif
#endif /* BCMDBG */

#endif /* !HWA_PKTPGR_BUILD */

/*
 * -----------------------------------------------------------------------------
 * Section: Statistics Register Set Management
 * -----------------------------------------------------------------------------
 */
#define HWA_STATS_BUSY_WAIT_BUSY_BURNLOOP  1024 // burnloop until DMA finishes
static void _hwa_stats_busy_wait(hwa_dev_t *dev);

static void
_hwa_stats_busy_wait(hwa_dev_t *dev)
{
	uint32 v32;
	uint32 is_busy;
	uint32 loop_count = HWA_STATS_BUSY_WAIT_BUSY_BURNLOOP;

	/* Add loop count in case we try to get stats when HWA DMA is dead alreay. */
	v32 = HWA_RD_REG_NAME(HWA00, dev->regs, common, statscontrolreg);
	is_busy = BCM_GBF(v32, HWA_COMMON_STATSCONTROLREG_STARTBUSY);

	while (is_busy && loop_count--) {
		OSL_DELAY(1);
		v32 = HWA_RD_REG_NAME(HWA00, dev->regs, common, statscontrolreg);
		is_busy = BCM_GBF(v32, HWA_COMMON_STATSCONTROLREG_STARTBUSY);
	}

	if (is_busy && loop_count == 0) {
		HWA_ERROR(("%s():HWA[%d] stats is not ready, HWA DMA could be error!\n",
			__FUNCTION__, dev->unit));
	}
} // _hwa_stats_busy_wait

void
hwa_stats_clear(hwa_dev_t *dev, hwa_stats_set_index_t set_idx)
{
	uint32 v32;

	HWA_ASSERT(dev != (hwa_dev_t*)NULL);
	HWA_ASSERT(set_idx < HWA_STATS_SET_INDEX_MAX);
	HWA_FTRACE(HWA00);

	// FIXME: By placing dma template in this reg we need to repeatedly set them
	v32 =
		BCM_SBF(set_idx, HWA_COMMON_STATSCONTROLREG_STATSSETIDX) |
		BCM_SBF(1, HWA_COMMON_STATSCONTROLREG_CLEARSTATSSET) |
		BCM_SBF(0, HWA_COMMON_STATSCONTROLREG_COPYSTATSSET) |
		BCM_SBF(0, HWA_COMMON_STATSCONTROLREG_NUMSETS) |
#ifdef BCMPCIEDEV
		BCM_SBF(1, HWA_COMMON_STATSCONTROLREG_DMAPCIEDESCTEMPLATENOTPCIE) |
#else
		BCM_SBF(0, HWA_COMMON_STATSCONTROLREG_DMAPCIEDESCTEMPLATENOTPCIE) |
#endif // endif
		BCM_SBF(dev->host_coherency,
			HWA_COMMON_STATSCONTROLREG_DMAPCIEDESCTEMPLATECOHERENT) |
		BCM_SBF(0, HWA_COMMON_STATSCONTROLREG_DMAPCIEDESCTEMPLATEADDREXT) |
		BCM_SBF(1, HWA_COMMON_STATSCONTROLREG_DMAHWADESCTEMPLATENOTPCIE);
	HWA_WR_REG_NAME(HWA00, dev->regs, common, statscontrolreg, v32);

	_hwa_stats_busy_wait(dev);

} // hwa_stats_clear

void // Fetch HWA statistics and invoke the callback handler on completion
hwa_stats_copy(hwa_dev_t *dev, hwa_stats_set_index_t set_idx,
	uint32 loaddr, uint32 hiaddr, uint32 num_sets, uint8 clear_on_copy,
	hwa_stats_cb_t hwa_stats_cb, uintptr arg1, uint32 arg2)
{
	uint32 v32;

	HWA_ASSERT(dev != (hwa_dev_t*)NULL);
	HWA_ASSERT(set_idx < HWA_STATS_SET_INDEX_MAX);
	HWA_FTRACE(HWA00);

	HWA_WR_REG_NAME(HWA00, dev->regs, common, statdonglreaddressreg, loaddr);
	HWA_WR_REG_NAME(HWA00, dev->regs, common, statdonglreaddresshireg, hiaddr);

	// FIXME: Why do we have txpost wi related DMAPCIEDESCTEMPLATE attributes
	//        in the Common::StatsControlReg?
	//        By placing dma template in this reg we need to repeatedly set them
	v32 =
		BCM_SBF(set_idx, HWA_COMMON_STATSCONTROLREG_STATSSETIDX) |
		BCM_SBF(clear_on_copy, HWA_COMMON_STATSCONTROLREG_CLEARSTATSSET) |
		BCM_SBF(1, HWA_COMMON_STATSCONTROLREG_COPYSTATSSET) |
		BCM_SBF(num_sets, HWA_COMMON_STATSCONTROLREG_NUMSETS) |
#ifdef BCMPCIEDEV
		BCM_SBF(1, HWA_COMMON_STATSCONTROLREG_DMAPCIEDESCTEMPLATENOTPCIE) |
#else
		BCM_SBF(0, HWA_COMMON_STATSCONTROLREG_DMAPCIEDESCTEMPLATENOTPCIE) |
#endif // endif
		BCM_SBF(dev->host_coherency,
			HWA_COMMON_STATSCONTROLREG_DMAPCIEDESCTEMPLATECOHERENT) |
		BCM_SBF(0, HWA_COMMON_STATSCONTROLREG_DMAPCIEDESCTEMPLATEADDREXT) |
		BCM_SBF(1, HWA_COMMON_STATSCONTROLREG_DMAHWADESCTEMPLATENOTPCIE);
	HWA_WR_REG_NAME(HWA00, dev->regs, common, statscontrolreg, v32);

	_hwa_stats_busy_wait(dev); // loop until DMA done ... synchronous

	(*hwa_stats_cb)(dev, arg1, arg2);

} // hwa_stats_copy

/*
 * -----------------------------------------------------------------------------
 * Section: Upstream callback handlers
 * -----------------------------------------------------------------------------
 */
// Default HWA noop handler
#if !defined(HWA_PKTPGR_BUILD)
static int
hwa_callback_noop(void *context,
	uintptr arg1, uintptr arg2, uint32 arg3, uint32 arg4)
{
	HWA_TRACE(("%s(arg1<0x%llx> arg2<0x%llx> arg3<0x%08x> arg4<0x%08x>)\n",
		__FUNCTION__, HWA_PTR2UINT(arg1), HWA_PTR2UINT(arg2), arg3, arg4));
	return HWA_FAILURE;

} // hwa_callback_noop
#else /* HWA_PKTPGR_BUILD */
static int
hwa_callback_noop(void *context, hwa_dev_t *dev, /* const */ hwa_pp_cmd_t *pp_cmd)
{
	HWA_TRACE(("%s(dev<%p>,cmd<%p)\n", __FUNCTION__, dev, pp_cmd));
	return HWA_FAILURE;
} // hwa_callback_noop
#endif /* HWA_PKTPGR_BUILD */

void // Register an upper layer upstream callback handler
hwa_register(hwa_dev_t *dev,
	hwa_callback_t cb, void *cb_ctx, hwa_callback_fn_t cb_fn)
{
	dev->handlers[cb].context  = cb_ctx;
	dev->handlers[cb].callback = cb_fn;
} // hwa_register

/*
 * -----------------------------------------------------------------------------
 * Section: HWA Device carrying SW driver state of all HWA blocks.
 * -----------------------------------------------------------------------------
 */
#ifdef DONGLEBUILD
// Global instance of HWA device
hwa_dev_t hwa_dev_g = { .coreid = 0, .corerev = 0 };
hwa_dev_t *hwa_dev = &hwa_dev_g;
#endif /* DONGLEBUILD */

struct hwa_dev * // HWA attach, uses global hwa_dev
BCMATTACHFN(hwa_attach)(void *wlc, uint device, osl_t *osh,
	volatile void *regs, si_t *sih, uint unit, uint bustype)
{
	int i;
	uint32 v32;
	char *val;
	hwa_dev_t *hwa_dev_local = NULL;

	HWA_TRACE(("\n\n+++++ HWA[%d] ATTACH PHASE BEGIN +++++\n\n", unit));
	HWA_TRACE(("%s HWA[%d]:  attach device<%x> regs<%p> bustype<%u>\n",
		HWA00, unit, device, regs, bustype));

#ifdef DONGLEBUILD
	// Use global hwa_dev
	if (hwa_dev->coreid != 0)
		return hwa_dev;

	hwa_dev_local = hwa_dev;

	memset(hwa_dev_local, 0, sizeof(hwa_dev_t));
#else /* !DONGLEBUILD */
	if ((hwa_dev_local = MALLOCZ(osh, sizeof(hwa_dev_t))) == NULL) {
		HWA_ERROR(("HWA[%d]: %s out of mem, malloced %d bytes \n",
			unit, __FUNCTION__, MALLOCED(osh)));
		goto fail_attach;
	}
#endif /* !DONGLEBUILD */

	HWA_ASSERT(regs != NULL); // pointer to HWA registers

	// Attach to backplane
	hwa_dev_local->sih = sih;
	if (!hwa_dev_local->sih) {
		goto fail_attach;
	}

	// Bring back to PCIe core
	si_setcore(hwa_dev_local->sih, HWA_CORE_ID, 0);

	if (!si_iscoreup(hwa_dev_local->sih)) {
		si_core_reset(hwa_dev_local->sih, 0, 0);
	}

	// FIXME: Need an assert to ensure that HWA core is up

	// Setup the HWA platform contexts and regs
	hwa_dev_local->wlc = wlc;
	hwa_dev_local->osh = osh;
	hwa_dev_local->regs = regs;
	hwa_dev_local->device = device;
	hwa_dev_local->bustype = bustype;

	// Setup the HWA core id and revision
	hwa_dev_local->coreid = si_coreid(hwa_dev_local->sih);
	hwa_dev_local->corerev = si_corerev(hwa_dev_local->sih);

#ifdef BCMQT // WIP: for 6715B0 Veloce
	if (HWAREV_IS(HWA_REVISION_ID, 132)) {
		HWA_PRINT("%s HWA corerev is %d force to 132 for Veloce test\n",
			__FUNCTION__, hwa_dev_local->corerev);
		hwa_dev_local->corerev = 132;
	}
#endif /* BCMQT */

	// Setup the SW driver mode as NIC or Full Dongle
	hwa_dev_local->driver_mode = HWA_DRIVER_MODE;

#ifndef DONGLEBUILD
	// Setup si_flag which is bit definition used for PCIEIntMask
	hwa_dev_local->si_flag = si_flag(hwa_dev_local->sih);
#endif /* !DONGLEBUILD */

	// For B0 verification.
	// Default values
#ifdef RXCPL4
	hwa_dev_local->rxcpl_inuse = 0;
#endif // endif
	hwa_dev_local->txfifo_apb = 0;
	hwa_dev_local->txstat_apb = 1;
	hwa_dev_local->txfifo_hwupdnext = 0;

	// 0: use legacy HWA dma, 1: use dma tx/rx. HWAREV_GE(131)
	hwa_dev_local->dmasel = 0;
	HWA_PKTPGR_EXPR({
		// XXX, dmasel has 4K boundary issue in A0 [rev 131]. Don't use all.
		// A0-TX: needs partial dmasel to WAR BCAFWUCODE-6451
		// A0-RX: needs partial dmasel for Rx throughput turning.
		if (HWAREV_GE(hwa_dev_local->corerev, 131))
			hwa_dev_local->dmasel = 1;  // force enable for PP
	});

	// 0: Read AQM descriptor back, 1: use sotptr and numdec from mac. HWAREV_GE(131)
	hwa_dev_local->txd_cal_mode = 0;
	HWA_PKTPGR_EXPR(hwa_dev_local->txd_cal_mode = 1); // force enable for PP

	// 0: use dongle memory, 1: use axi memory(HWA2.2)
	HWA_PKTPGR_EXPR(hwa_dev_local->d11b_axi = 1); // force enable for PP
	HWA_PKTPGR_EXPR({
		// d11b_recycle_war is enabled by default for A0
		if (HWAREV_IS(hwa_dev_local->corerev, 131))
			hwa_dev_local->d11b_recycle_war = TRUE;
	});

	// The is specific for HWA2.2 when one MPDU has more than one TxLfrag(HWA2.2)
	HWA_PKTPGR_EXPR(hwa_dev_local->flexible_txlfrag = 1); // force enable for PP

	// RXProcess
	HWA_PKTPGR_EXPR(hwa_dev_local->rx_pkt_req_max = 4);
	HWA_PKTPGR_EXPR(hwa_dev_local->rx_pkt_count_max = 256);

	// Update from nvram if any.
#ifdef RXCPL4
	if ((val = getvar(NULL, "rxcpl_inuse")) != NULL)
		hwa_dev_local->rxcpl_inuse = bcm_strtoul(val, NULL, 0);
	HWA_ASSERT(hwa_dev_local->rxcpl_inuse < 4);
#endif // endif
	if ((val = getvar(NULL, "txfifo_apb")) != NULL)
		hwa_dev_local->txfifo_apb = bcm_strtoul(val, NULL, 0);
	if ((val = getvar(NULL, "txstat_apb")) != NULL)
		hwa_dev_local->txstat_apb = bcm_strtoul(val, NULL, 0);
	if ((val = getvar(NULL, "txfifo_hwupdnext")) != NULL)
		hwa_dev_local->txfifo_hwupdnext = bcm_strtoul(val, NULL, 0);
	// Must to use HW update next for PP
	HWA_PKTPGR_EXPR(hwa_dev_local->txfifo_hwupdnext = 1);

#if defined(BCMHME)
	// MAC DMA FIFO description table location.
	hwa_dev_local->hme_macifs_modes = HWA_HME_MACIFS_NONE;
	if (HWAREV_GE(hwa_dev_local->corerev, 131)) {
		HWA_RXFILL_EXPR(hwa_dev_local->hme_macifs_modes |= HWA_HME_MACIFS_RX);
		HWA_TXFIFO_EXPR(hwa_dev_local->hme_macifs_modes |= HWA_HME_MACIFS_TX);
	}
	if ((val = getvar(NULL, "hme_macifs_modes")) != NULL)
		hwa_dev_local->hme_macifs_modes = bcm_strtoul(val, NULL, 0);
	// Must to use HME MACIFS offload memory for PP
	HWA_PKTPGR_EXPR(hwa_dev_local->hme_macifs_modes = HWA_HME_MACIFS_RX_TX);
#endif /* BCMHME */

	HWA_PRINT("%s "
#ifdef RXCPL4
		"rxcpl_inuse<%u> "
#endif // endif
		"txfifo_apb<%u> txstat_apb<%u> "
		"txfifo_hwupdnext<%u> txfree<%s>\n",
		HWA00,
#ifdef RXCPL4
		hwa_dev_local->rxcpl_inuse,
#endif // endif
		hwa_dev_local->txfifo_apb, hwa_dev_local->txstat_apb,
		hwa_dev_local->txfifo_hwupdnext,
#ifdef HWA_TXPOST_FREEIDXTX
		"ring");
#else
		"reg");
#endif // endif
	HWA_PRINT("hme_macifs_modes<%d> "
#if defined(HWA_PKTPGR_BUILD)
		"d11b_axi<%d> flexible_txlfrag<%d> "
#endif // endif
		"dmasel<%d> txd_cal_mode<%d>\n",
		hwa_dev_local->hme_macifs_modes,
#if defined(HWA_PKTPGR_BUILD)
		hwa_dev_local->d11b_axi, hwa_dev_local->flexible_txlfrag,
#endif // endif
		hwa_dev_local->dmasel, hwa_dev_local->txd_cal_mode);

	// Fetch the pcie_ipc object
	HWA_BUS_EXPR(hwa_dev_local->pcie_ipc = hnd_get_pcie_ipc());
	// Fetch the pciedev object
	HWA_BUS_EXPR(hwa_dev_local->pciedev = pciedev_get_handle());
	HWA_BUS_EXPR(hwa_dev_local->dngl = pciedev_dngl(hwa_dev_local->pciedev));

	// Global object is safe for use
	hwa_dev_local->self = hwa_dev_local;

	HWA_TRACE(("%s driver mode<%s> hwa_dev<%p> size<%d> core id<%x> rev<%u>"
		" device<%x> bustype<%u>\n",
		HWA00, (hwa_dev_local->driver_mode == HWA_FD_MODE) ? "FD" : "NIC", hwa_dev_local,
		(int)sizeof(hwa_dev_t), hwa_dev_local->coreid, hwa_dev_local->corerev,
		hwa_dev_local->device, hwa_dev_local->bustype));

	HWA_ASSERT(hwa_dev_local->coreid  == HWA_CORE_ID); // hnd_soc.h
	HWA_ASSERT(hwa_dev_local->corerev >= 129); // 43684A0, A1 Depleted

	v32 = HWA_RD_REG_NAME(HWA00, hwa_dev_local->regs, top, hwahwcap2);
	BCM_REFERENCE(v32);
	HWA_ASSERT(BCM_GBF(v32, HWA_TOP_HWAHWCAP2_HWABLKSPRESENT) ==
		HWA_BLKS_PRESENT);

	// Attach HWA DMA engines/channels
	HWA_DPC_EXPR(hwa_dma_attach(hwa_dev_local));

	for (i = 0; i < HWA_CALLBACK_MAX; i++) {
		hwa_register(hwa_dev_local, i, NULL, hwa_callback_noop);
	}

	// Init axi memory base address
	hwa_init_axi_addr(hwa_dev_local);

	// Allocate all HWA blocks state
	HWA_RXPATH_EXPR(hwa_rxpath_attach(hwa_dev_local)); // HWA1a HWA1b
	HWA_RXDATA_EXPR(hwa_rxdata_attach(hwa_dev_local)); // HWA2a

	HWA_TXPOST_EXPR(hwa_txpost_attach(hwa_dev_local)); // HWA3a
	HWA_TXFIFO_EXPR(hwa_txfifo_attach(hwa_dev_local)); // HWA3b
	HWA_TXSTAT_EXPR(hwa_txstat_attach(hwa_dev_local)); // HWA4a

	HWA_CPLENG_EXPR(hwa_cpleng_attach(hwa_dev_local)); // HWA2b HWA4b

	HWA_PKTPGR_EXPR(hwa_pktpgr_attach(hwa_dev_local)); // HWApp

	HWA_TRACE(("\n\n----- HWA ATTACH PHASE DONE -----\n\n"));

	((wlc_info_t *)wlc)->pub->hwarev = hwa_dev_local->corerev;
	((wlc_info_t *)wlc)->pub->_hwa = TRUE;

#if defined(BCMDBG)
	wlc_dump_register(((wlc_info_t *)wlc)->pub, "hwa", (dump_fn_t)hwa_wl_dump,
		(void *)hwa_dev_local);
#endif // endif

#ifdef DONGLEBUILD
	return HWA_DEVP(TRUE);
#else
	return hwa_dev_local;
#endif // endif

fail_attach:

	hwa_detach(hwa_dev_local);
	HWA_WARN(("HWA[%d] Attach failure\n", unit));
	HWA_ASSERT(0);

	return HWA_DEV_NULL;

} // hwa_attach

void
BCMATTACHFN(hwa_detach)(struct hwa_dev *dev)
{
	if (dev == HWA_DEV_NULL) {
		return;
	}

	HWA_TRACE(("\n\n+++++ HWA[%d] DETACH PHASE BEGIN +++++\n\n", dev->unit));

	HWA_RXPATH_EXPR(hwa_rxpath_detach(&dev->rxpath)); // HWA1a HWA1b
	HWA_RXDATA_EXPR(hwa_rxdata_detach(&dev->rxdata)); // HWA2a

	HWA_TXPOST_EXPR(hwa_txpost_detach(&dev->txpost)); // HWA3a
	HWA_TXFIFO_EXPR(hwa_txfifo_detach(&dev->txfifo)); // HWA3b
	HWA_TXSTAT_EXPR(hwa_txstat_detach(&dev->txstat)); // HWA4a

	HWA_CPLENG_EXPR(hwa_cpleng_detach(&dev->cpleng)); // HWA2b HWA4b

	HWA_PKTPGR_EXPR(hwa_pktpgr_detach(&dev->pktpgr)); // HWApp

	hwa_osl_detach(dev);

#ifndef DONGLEBUILD
	/* Free up the HWA dev memory */
	MFREE(dev->osh, dev, sizeof(hwa_dev_t));
	dev = NULL;
#endif /* !DONGLEBUILD */

	((wlc_info_t *)dev->wlc)->pub->_hwa = FALSE;

	HWA_TRACE(("\n\n----- HWA[%d] DETACH PHASE DONE -----\n\n", dev->unit));

} // hwa_detach

uint8
hwa_hme_macifs_modes(struct hwa_dev *dev)
{
#ifdef BCMHME
	if (hme_cap(HME_USER_MACIFS)) {
		return dev->hme_macifs_modes;
	}
#endif // endif

	return HWA_HME_MACIFS_NONE;
}

void // HWA CONFIG PHASE
hwa_config(struct hwa_dev *dev)
{
	uint32 v32, splithdr, sw_pkt32_cap;
	hwa_regs_t *regs;

	HWA_TRACE(("\n\n+++++ HWA[%d] CONFIG PHASE BEGIN +++++\n\n", dev->unit));
	HWA_FTRACE(HWA00);

	HWA_ASSERT(dev != (hwa_dev_t*)NULL);
	regs = dev->regs;

	/* Legacy HWA DMA : DMA src to HWA-DMA internal then to dest.
	 * DMA TX/RX SEL: DMA src to dest directly.
	 */
	if (HWAREV_GE(dev->corerev, 131)) {
		if (dev->dmasel) {
			// dma tx
			if (HWAREV_IS(dev->corerev, 131)) {
				// XXX, 6715A0-PP uses partial dmasel to WAR BCAFWUCODE-6451
				// bit0 and bit1.
				v32 = 0U
				| BCM_SBIT(HWA_COMMON_DMATXSEL_FETCH_SWPKTC_WDMATX)
				| BCM_SBIT(HWA_COMMON_DMATXSEL_FETCH_SWPKT_WDMATX)
				| 0U;

				// XXX, To improve Rx throughput.
				// Pager rings have aligned at 4K, saft to use dmatxsel.
				// bit3 to bit9
				v32 |= 0U
				| BCM_SBIT(HWA_COMMON_DMATXSEL_FETCH_PAGEIN_REQCMD_WDMATX)
				| BCM_SBIT(HWA_COMMON_DMATXSEL_FETCH_PAGEOUT_REQCMD_WDMATX)
				| BCM_SBIT(HWA_COMMON_DMATXSEL_FETCH_PAGEMGR_ALLOC_REQCMD_WDMATX)
				| BCM_SBIT(HWA_COMMON_DMATXSEL_FETCH_PAGEMGR_FREE_REQCMD_WDMATX)
				| BCM_SBIT(HWA_COMMON_DMATXSEL_FETCH_PAGEMGR_FREE_RPH_REQCMD_WDMATX)
				| BCM_SBIT(HWA_COMMON_DMATXSEL_FETCH_FREEP_PACKET_WDMATX)
				| BCM_SBIT(HWA_COMMON_DMATXSEL_FETCH_PHPL_PACKET_WDMATX)
				| 0U;
			} else {
				v32 = 0U
				| BCM_SBIT(HWA_COMMON_DMATXSEL_FETCH_SWPKTC_WDMATX)
				| BCM_SBIT(HWA_COMMON_DMATXSEL_FETCH_SWPKT_WDMATX)
				| BCM_SBIT(HWA_COMMON_DMATXSEL_FETCH_SCMDQ_WDMATX)
				| BCM_SBIT(HWA_COMMON_DMATXSEL_FETCH_PAGEIN_REQCMD_WDMATX)
				| BCM_SBIT(HWA_COMMON_DMATXSEL_FETCH_PAGEOUT_REQCMD_WDMATX)
				| BCM_SBIT(HWA_COMMON_DMATXSEL_FETCH_PAGEMGR_ALLOC_REQCMD_WDMATX)
				| BCM_SBIT(HWA_COMMON_DMATXSEL_FETCH_PAGEMGR_FREE_REQCMD_WDMATX)
				| BCM_SBIT(HWA_COMMON_DMATXSEL_FETCH_PAGEMGR_FREE_RPH_REQCMD_WDMATX)
				| BCM_SBIT(HWA_COMMON_DMATXSEL_FETCH_FREEP_PACKET_WDMATX)
				| BCM_SBIT(HWA_COMMON_DMATXSEL_FETCH_PHPL_PACKET_WDMATX)
				| BCM_SBIT(HWA_COMMON_DMATXSEL_FETCH_FRC_WITEM_WDMATX)
				| BCM_SBIT(HWA_COMMON_DMATXSEL_FETCH_FRC_FMTTRANS_WDMATX)
				| BCM_SBIT(HWA_COMMON_DMATXSEL_FETCH_WI_MOVE_WDMATX)
				| BCM_SBIT(HWA_COMMON_DMATXSEL_FETCH_CED_MOVE_WDMATX)
				| BCM_SBIT(HWA_COMMON_DMATXSEL_FETCH_RXWI_MOVE_WDMATX)
				| 0U;
			}
			HWA_WR_REG_NAME(HWApp, regs, common, dmatxsel, v32);

			// dma rx
			if (HWAREV_IS(dev->corerev, 131)) {
				// XXX, 6715A0-PP uses partial dmasel to WAR BCAFWUCODE-6451
				// bit4 and bit5.
				v32 = 0U
				| BCM_SBIT(HWA_COMMON_DMARXSEL_PAGEIN_L2D_WDMARX)
				| BCM_SBIT(HWA_COMMON_DMARXSEL_PAGEOUT_L2H_WDMARX)
				| 0U;

				// XXX, To improve Rx throughput
				// bit2, 6, 9, 10, 12, 13, 14
				v32 |= 0U
				| BCM_SBIT(HWA_COMMON_DMARXSEL_TXP_SWPKT_WDMARX)
				| BCM_SBIT(HWA_COMMON_DMARXSEL_UPDATE_PAGEIN_RSPCMD_WDMARX)
				| BCM_SBIT(HWA_COMMON_DMARXSEL_UPDATE_APKT_PACKET_WDMARX)
				| BCM_SBIT(HWA_COMMON_DMARXSEL_UPDATE_RXAPKT_PACKET_WDMARX)
				| BCM_SBIT(HWA_COMMON_DMARXSEL_MOVE_D0MGR_WDMARX)
				| BCM_SBIT(HWA_COMMON_DMARXSEL_MOVE_D1MGR_WDMARX)
				| BCM_SBIT(HWA_COMMON_DMARXSEL_MOVE_D11B_WDMARX)
				| 0U;
			} else {
				v32 = 0U
				| BCM_SBIT(HWA_COMMON_DMARXSEL_TXD_MAC_WDMARX)
				| BCM_SBIT(HWA_COMMON_DMARXSEL_TXS_MOVE_WDMARX)
				| BCM_SBIT(HWA_COMMON_DMARXSEL_TXP_SWPKT_WDMARX)
				| BCM_SBIT(HWA_COMMON_DMARXSEL_TXP_PKTCHQ_WDMARX)
				| BCM_SBIT(HWA_COMMON_DMARXSEL_PAGEIN_L2D_WDMARX)
				| BCM_SBIT(HWA_COMMON_DMARXSEL_PAGEOUT_L2H_WDMARX)
				| BCM_SBIT(HWA_COMMON_DMARXSEL_UPDATE_PAGEIN_RSPCMD_WDMARX)
				| BCM_SBIT(HWA_COMMON_DMARXSEL_UPDATE_PAGEOUT_RSPCMD_WDMARX)
				| BCM_SBIT(HWA_COMMON_DMARXSEL_UPDATE_PAGEMGR_ALLOC_RSPCMD_WDMARX)
				| BCM_SBIT(HWA_COMMON_DMARXSEL_UPDATE_APKT_PACKET_WDMARX)
				| BCM_SBIT(HWA_COMMON_DMARXSEL_UPDATE_RXAPKT_PACKET_WDMARX)
				| BCM_SBIT(HWA_COMMON_DMARXSEL_UPDATE_STAT_ENG_WDMARX)
				| BCM_SBIT(HWA_COMMON_DMARXSEL_MOVE_D0MGR_WDMARX)
				| BCM_SBIT(HWA_COMMON_DMARXSEL_MOVE_D1MGR_WDMARX)
				| BCM_SBIT(HWA_COMMON_DMARXSEL_MOVE_D11B_WDMARX)
				| 0U;
			}
			HWA_WR_REG_NAME(HWApp, regs, common, dmarxsel, v32);

			// The default burstlen is 512B, must set to 256B.
			v32 = HWA_RD_REG_NAME(HWA00, regs, common, dmatx0ctrl);
			v32 = BCM_CBF(v32, HWA_COMMON_DMATX0CTRL_DMATX0BL);
			v32 |= BCM_SBF(DMA_BL_256, HWA_COMMON_DMATX0CTRL_DMATX0BL);
			HWA_WR_REG_NAME(HWA00, regs, common, dmatx0ctrl, v32);

			v32 = HWA_RD_REG_NAME(HWA00, regs, common, dmatx1ctrl);
			v32 = BCM_CBF(v32, HWA_COMMON_DMATX1CTRL_DMATX1BL);
			v32 |= BCM_SBF(DMA_BL_256, HWA_COMMON_DMATX1CTRL_DMATX1BL);
			HWA_WR_REG_NAME(HWA00, regs, common, dmatx1ctrl, v32);
		} else {
			// legacy HWA DMA
			v32 = 0U;
			HWA_WR_REG_NAME(HWApp, regs, common, dmatxsel, v32);
			HWA_WR_REG_NAME(HWApp, regs, common, dmarxsel, v32);
		}
	}

#if !defined(BCMPCIEDEV) /* NIC mode SW driver */

	HWA_ASSERT(dev->driver_mode == HWA_NIC_MODE);

	dev->macif_placement = HWA_MACIF_IN_HOSTMEM; // Currently MACIf(s) in DDR

	// Check ACP coherence flag
	if (osl_is_flag_set(dev->osh, OSL_ACP_COHERENCE)) {
		dev->macif_coherency = HWA_HW_COHERENCY;
		dev->host_coherency  = HWA_HW_COHERENCY;
	} else {
		dev->macif_coherency = HWA_SW_COHERENCY;
		dev->host_coherency  = HWA_SW_COHERENCY;
	}

	dev->host_addressing = HWA_64BIT_ADDRESSING;
	dev->host_physaddrhi = HWA_INVALID_HIADDR;

	splithdr = 0U; // NIC mode uses contiguous packet
	sw_pkt32_cap = dev->host_addressing;

#else  /* defined BCMPCIEDEV: FullDongle mode SW driver */

#if defined(BCMHME) && (defined(HWA_TXFIFO_BUILD) || defined(HWA_RXFILL_BUILD))
	// Update HMExtension info to WL.
	if (HWA_HME_MACIFS_EN(dev)) {
		int freesz;
		haddr64_t hme64;
		dma64addr_t dma64;

		// Get all HME MACIFS memory
		freesz = hme_free(HME_USER_MACIFS);
		hme64 = hme_get(HME_USER_MACIFS, freesz);
		PHYSADDR64HISET(dma64, HADDR64_HI(hme64));
		PHYSADDR64LOSET(dma64, HADDR64_LO(hme64));
		if (hwa_wl_hme_macifs_upd(dev, dma64, freesz) != BCME_OK) {
			HWA_ERROR(("%s HWA WL HME update failure\n", HWA00));
			HWA_ASSERT(0);
			return;
		}
	}
#endif /* BCMHME && (HWA_TXFIFO_BUILD || HWA_RXFILL_BUILD) */

	HWA_ERROR(("%s Host advertized capabilities<0x%08x>\n", HWA00, dev->pcie_ipc->hcap1));

	HWA_ASSERT(dev->driver_mode == HWA_FD_MODE);

	// Ensure Host has advertised capability in pcie_ipc structure
	HWA_ASSERT(dev->pcie_ipc != (struct pcie_ipc *)NULL);

	dev->macif_placement = HWA_MACIF_IN_DNGLMEM; // Always in device memory
	dev->macif_coherency = HWA_HW_COHERENCY;	 // Always HW coherent

	// Determine Host coherency model using host advertised capability
	dev->host_coherency =
		(dev->pcie_ipc->hcap1 & PCIE_IPC_HCAP1_HW_COHERENCY) ?
		HWA_HW_COHERENCY : HWA_SW_COHERENCY;
	// Note: Dongle CPU complex is always assumed to be HW coherent.

	if (dev->pcie_ipc->hcap1 & PCIE_IPC_HCAP1_ADDR64) {
		dev->host_addressing = HWA_64BIT_ADDRESSING;
		dev->host_physaddrhi = HWA_INVALID_HIADDR;
	} else {
		dev->host_addressing = HWA_32BIT_ADDRESSING;
		dev->host_physaddrhi = // Use fixed HI address provided by Host
			HWA_HOSTADDR64_HI32(dev->pcie_ipc->host_physaddrhi);
	}

	dev->pcie_ipc_rings = // fetch the ring info passed via the pcie_ipc
		HWA_UINT2PTR(pcie_ipc_rings_t, dev->pcie_ipc->rings_daddr32);
	HWA_ASSERT(dev->pcie_ipc_rings != (pcie_ipc_rings_t*)NULL);

	// FIXME: SW driver validated for Aggregated WI(RxPost, RxCpl and TxCpl)
	// FIXME: TxPost will continue to use non-aggregated but compact format!!!
	HWA_ASSERT((dev->pcie_ipc->hcap1 & PCIE_IPC_HCAP1_ACWI) != 0);
	dev->wi_aggr_cnt = HWA_PCIEIPC_WI_AGGR_CNT; // WI format aggregation cnt
	HWA_TRACE(("%s wi_aggr_cnt<%u>\n", HWA00, dev->wi_aggr_cnt));

	// FIXME: Not clear why host_2byte_index is part of common::intr_control
	// FIXME: This register is for generating SW doorbell using [addr, val]
	// FIXME: use_val and host_2byte_index will conflict!!!
#ifdef PCIE_DMAINDEX16 /* dmaindex16 firmware target build */
	HWA_ASSERT((dev->pcie_ipc->flags & PCIE_IPC_FLAGS_DMA_INDEX) != 0);
	HWA_ASSERT((dev->pcie_ipc->flags & PCIE_IPC_FLAGS_2BYTE_INDICES) != 0);
	v32 = HWA_RD_REG_NAME(HWA00, regs, common, intr_control);
	v32 |= BCM_SBIT(HWA_COMMON_INTR_CONTROL_HOST_2BYTE_INDEX) |
		BCM_SBF(dev->host_coherency, HWA_COMMON_INTR_CONTROL_TEMPLATE_COHERENT);
	HWA_WR_REG_NAME(HWA00, regs, common, intr_control, v32);
#else
	HWA_WARN(("%s PCIE_DMAINDEX16 not defined\n", HWA00));
	HWA_ASSERT(0);
#endif /* PCIE_DMAINDEX16 */

	splithdr = 1U; // PCIE FD uses split header and data, later in host memory.
#if defined(HWA_PKTPGR_BUILD)
	sw_pkt32_cap = 0U;  // Must be 64bit mode in PKTPGR
#else
	sw_pkt32_cap = splithdr;
#endif // endif

	if (dev->pcie_ipc->hcap1 & PCIE_IPC_HCAP1_LIMIT_BL) {
		HWA_DPC_EXPR(hwa_dma_rx_burstlen_adjust(&dev->dma));
	}
#endif /* defined BCMPCIEDEV: FullDongle mode SW driver */

	HWA_TRACE(("%s config mode<%s> macif<%s:%s> coh<%s> haddr<%s> hi32<0x%08x>\n",
		HWA00,
		(dev->driver_mode == HWA_FD_MODE) ? "FD" : "NIC",
		(dev->macif_placement == HWA_MACIF_IN_DNGLMEM) ? "DNGL" : "HOST",
		(dev->macif_coherency == HWA_HW_COHERENCY) ? "HW" : "SW",
		(dev->host_coherency == HWA_HW_COHERENCY) ? "HW" : "SW",
		(dev->host_addressing == HWA_64BIT_ADDRESSING) ? "64b" : "32b",
		dev->host_physaddrhi));

	// Configure the min threshold beyond which stalls are counted
	v32 = BCM_SBF(HWA_STATISTICS_MIN_BUSY,
	              HWA_COMMON_STATMINBUSYTIMEREG_STATSMINBUSYTIME);
	HWA_WR_REG_NAME(HWA00, regs, common, statminbusytimereg, v32);

	/*
	 * Configure whether 32bit or 64bit addresses will be specified
	 * The below complexity is intentionally introduced to significantly reduce
	 * the latency for HWA3b to promptly flush from Overflow queues to TxFIFO.
	 * This operation is deadline driven and address compression to reduce the
	 * size of a SW Tx packet (3b) is required. Memory savings is secondary.
	 *
	 *      -------------------------------------------------------------
	 *      |    capabilities    |      |  HWA3b SW Pkt Address fields  |
	 *      |sw_pkt32 | data_buf | MODE | pkt_next | hdr_buf | data_buf |
	 *      |---------|----------|------|----------|---------|----------|
	 *      |  0~     |     1*   |  FD* |    32b~  |   64b   |   64b    |
	 *      |  1      |     1*   |  FD* |    32b   |   32b   |   64b    |
	 *      |  0~     |     0    |  NIC |    64b~  |   64b   |    -     |
	 *      |  1      |     0    |  NIC |    32b   |   32b   |    -     |
	 *      -------------------------------------------------------------
	 *
	 * FD Mode:
	 * - data_buf_cap is always 1, implying a 64b data_buf_addr resides in host.
	 * - pkt_next is always 32b as dongle always uses 32bit addressing.
	 * - sw_pkt32_cap controls only hdr_buf_addr
	 *
	 * NIC Mode:
	 * - data_buf_cap is always 0 and SW Pkt does not have a data_buf_addr field
	 * - sw_pkt32_cap setting controls size of pkt_next and hdr_buf_addr
	 *
	 * When 32bit host addressing is enabled, a fixed hiaddr will be used to
	 * configure various descriptors.
	 */
	v32 = (0U
		HWA_PKTPGR_EXPR(
		    | BCM_SBIT(HWA_COMMON_HWA2HWCAP_HWPPSUPPORT))
		| BCM_SBF(splithdr, HWA_COMMON_HWA2HWCAP_DATA_BUF_CAP)
		| BCM_SBF(dev->host_addressing,
		          HWA_COMMON_HWA2HWCAP_PCIE_IPC_PKT_ADDR32_CAP)
		| BCM_SBF(sw_pkt32_cap, HWA_COMMON_HWA2HWCAP_SWPKT_ADDR32_CAP));
	HWA_WR_REG_NAME(HWA00, regs, common, hwa2hwcap, v32);

	// In NIC mode, setting pcieipc pkt high32 has no effect.

	// For TxFifo
	if (splithdr)
		v32 = 0;
	else
		v32 = dev->host_physaddrhi;
	HWA_WR_REG_NAME(HWA00, regs, common, hwa2swpkt_high32_pa, v32);

	// For RxFill
	v32 = dev->host_physaddrhi;
	HWA_WR_REG_NAME(HWA00, regs, common, hwa2pciepc_pkt_high32_pa, v32);

	HWA_RXPOST_EXPR(hwa_rxpost_preinit(&dev->rxpost));
	HWA_RXFILL_EXPR(hwa_rxfill_preinit(&dev->rxfill));

	// PKTPGR use HDBM and DDBM configured in hwa_pktpgr.c
#if !defined(HWA_PKTPGR_BUILD)
	// For TxBM
	HWA_TXPOST_EXPR({
		void *memory;
		uint32 mem_sz;
		uint32 txbuf_bytes;

		//XXX, for now it's 44 + 96 = 140, need to reduce the to X.
		txbuf_bytes = HWA_TXPOST_PKT_BYTES + LBUFFRAGSZ;
		mem_sz = txbuf_bytes * HWA_TXPATH_PKTS_MAX;
		if ((memory = MALLOCZ(dev->osh, mem_sz)) == NULL) {
			HWA_ERROR(("%s txbm malloc size<%u> failure\n", HWA00, mem_sz));
			HWA_ASSERT(memory != (void*)NULL);
			return;
		}
		HWA_TRACE(("%s txbm +memory[%p,%u]\n", HWA00, memory, mem_sz));
		hwa_bm_config(dev, &dev->tx_bm, "BM TxPATH", HWA_TX_BM,
			HWA_TXPATH_PKTS_MAX, txbuf_bytes,
			HWA_PTR2UINT(memory), HWA_PTR2HIADDR(memory), memory);
	});

#endif /* !HWA_PKTPGR_BUILD */

	// Set pager HBM and enable pager
	HWA_PKTPGR_EXPR(hwa_pktpgr_preinit(&dev->pktpgr));

	HWA_TRACE(("\n\n----- HWA[%d] CONFIG PHASE DONE -----\n\n", dev->unit));

} // hwa_config

void // HWA INIT PHASE
hwa_init(hwa_dev_t *dev)
{
	uint32 v32;

	HWA_TRACE(("\n\n+++++ HWA[%d] INIT PHASE BEGIN +++++\n\n", dev->unit));
	HWA_FTRACE(HWA00);

	HWA_ASSERT(dev != (hwa_dev_t*)NULL);
	HWA_ASSERT(dev->regs != (hwa_regs_t*)NULL);

	v32 = 0U; // Disable module clk gating

	HWA_WR_REG_NAME(HWA00, dev->regs, common, module_clkgating_enable, v32);

	// XXX Default module_clkext must be 3 or above CRWLHWA-299
	v32 = HWA_RD_REG_NAME(HWA00, dev->regs, common, module_clkext);
	HWA_ASSERT(v32 >= 3);

	// Enable clk to all blocks - not using hwa_module_request()
	v32 = 0U
		HWA_RXPATH_EXPR(
			| BCM_SBF(1, HWA_COMMON_MODULE_CLK_ENABLE_BLOCKRXCORE0_CLKENABLE)
			| BCM_SBF(1, HWA_COMMON_MODULE_CLK_ENABLE_BLOCKRXBM_CLKENABLE))
		HWA_CPLENG_EXPR(
			| BCM_SBF(1, HWA_COMMON_MODULE_CLK_ENABLE_BLOCKCPL_CLKENABLE))
		HWA_TXSTAT_EXPR(
			| BCM_SBF(1, HWA_COMMON_MODULE_CLK_ENABLE_BLOCKTXSTS_CLKENABLE))
		HWA_TXPOST_EXPR(
			| BCM_SBF(1, HWA_COMMON_MODULE_CLK_ENABLE_BLOCK3A_CLKENABLE))
		HWA_TXFIFO_EXPR(
			| BCM_SBF(1, HWA_COMMON_MODULE_CLK_ENABLE_BLOCK3B_CLKENABLE))
		HWA_PKTPGR_EXPR(
			| BCM_SBF(1, HWA_COMMON_MODULE_CLK_ENABLE_PAGER_CLKENABLE))
		| BCM_SBF(1, HWA_COMMON_MODULE_CLK_ENABLE_BLOCKSTATISTICS_CLKENABLE);
	// XXX, CRBCAHWA-581: WAR for rev129
	if (HWAREV_IS(dev->corerev, 129)) {
		// Set block3B_clkEnable to enalbe 4A TxS internal memory region for DMA.
		HWA_TXSTAT_EXPR(v32 |= BCM_SBF(1, HWA_COMMON_MODULE_CLK_ENABLE_BLOCK3B_CLKENABLE));
		// XXX, Set block3B_clkEnable to make 3A only mode work.
		HWA_TXPOST_EXPR(v32 |= BCM_SBF(1, HWA_COMMON_MODULE_CLK_ENABLE_BLOCK3B_CLKENABLE));
	}
	HWA_RXPATH_EXPR(
		if (HWA_RX_CORES > 1)
			v32 |= BCM_SBIT(HWA_COMMON_MODULE_CLK_ENABLE_BLOCKRXCORE1_CLKENABLE));
	HWA_TXSTAT_EXPR(
		if (HWA_TX_CORES > 1)
			v32 |= BCM_SBIT(HWA_COMMON_MODULE_CLK_ENABLE_BLOCKTXSTS1_CLKENABLE));
	HWA_WR_REG_NAME(HWA00, dev->regs, common, module_clk_enable, v32);

	// PKTPGR use HDBM and DDBM configured in hwa_pktpgr.c
#if !defined(HWA_PKTPGR_BUILD)
	// Buffer Managers enabled now
	HWA_RXFILL_EXPR(hwa_bm_init(&dev->rx_bm, TRUE));
	HWA_TXPOST_EXPR(hwa_bm_init(&dev->tx_bm, TRUE));
#endif /* !HWA_PKTPGR_BUILD */

	// Initialize all blocks
	HWA_RXPOST_EXPR(hwa_rxpost_init(&dev->rxpost));
	HWA_RXFILL_EXPR(hwa_rxfill_init(&dev->rxfill));
	HWA_RXDATA_EXPR(hwa_rxdata_init(&dev->rxdata));

	HWA_TXPOST_EXPR(hwa_txpost_init(&dev->txpost));
	HWA_TXFIFO_EXPR(hwa_txfifo_init(&dev->txfifo));
	HWA_TXSTAT_EXPR(hwa_txstat_init(&dev->txstat));

	HWA_CPLENG_EXPR(hwa_cpleng_init(&dev->cpleng));

	HWA_PKTPGR_EXPR(hwa_pktpgr_init(&dev->pktpgr));

	dev->inited = TRUE;

	HWA_TRACE(("\n\n----- HWA[%d] INIT PHASE DONE -----\n\n", dev->unit));

} // hwa_init

void // HWA DEINIT PHASE
hwa_deinit(hwa_dev_t *dev)
{

	HWA_TRACE(("\n\n+++++ HWA DEINIT PHASE BEGIN +++++\n\n"));
	HWA_FTRACE(HWA00);

	HWA_ASSERT(dev != (hwa_dev_t*)NULL);

	// DeInitialize all blocks
	HWA_RXDATA_EXPR(hwa_rxdata_deinit(&dev->rxdata));
	HWA_RXFILL_EXPR(hwa_rxfill_deinit(&dev->rxfill));
	HWA_RXPOST_EXPR(hwa_rxpost_deinit(&dev->rxpost));

	/* Make sure all rxstatus are updated */
	HWA_RXPOST_EXPR(hwa_rxpath_flush_rxcomplete(dev));
	NO_HWA_PKTPGR_EXPR(HWA_RXPOST_EXPR(hwa_wlc_mac_event(
		dev, WLC_E_HWA_RX_STOP)));
	HWA_TXSTAT_EXPR(hwa_txstat_deinit(&dev->txstat));
	HWA_TXFIFO_EXPR(hwa_txfifo_deinit(&dev->txfifo));
	HWA_TXPOST_EXPR(hwa_txpost_deinit(&dev->txpost));

	HWA_CPLENG_EXPR(hwa_cpleng_deinit(&dev->cpleng));

	HWA_PKTPGR_EXPR(hwa_pktpgr_deinit(&dev->pktpgr));

	HWA_TRACE(("\n\n----- HWA[%d] DEINIT PHASE DONE -----\n\n", dev->unit));

}

void
hwa_set_reinit(struct hwa_dev *dev)
{
	if (dev == HWA_DEV_NULL) {
		return;
	}

	dev->reinit = TRUE;
}

void
hwa_reinit(hwa_dev_t *dev)
{
	uint32 v32;

	HWA_FTRACE(HWA00);

	HWA_ASSERT(dev != (hwa_dev_t*)NULL);

	hwa_deinit(dev);

	hwa_init(dev);

	// DMA channels are enabled now
	HWA_DPC_EXPR(hwa_dma_enable(&dev->dma));

	// Enable all blocks except 1a and 1b
	v32 = (0U
		HWA_CPLENG_EXPR(
			| BCM_SBIT(HWA_COMMON_MODULE_ENABLE_BLOCKCPL_ENABLE))
		HWA_TXPOST_EXPR(
			| BCM_SBIT(HWA_COMMON_MODULE_ENABLE_BLOCK3A_ENABLE))
		HWA_TXFIFO_EXPR(
			| BCM_SBIT(HWA_COMMON_MODULE_ENABLE_BLOCK3B_ENABLE))
		HWA_TXSTAT_EXPR(
			| BCM_SBIT(HWA_COMMON_MODULE_ENABLE_BLOCKTXSTS_ENABLE))
		HWA_PKTPGR_EXPR(
			| BCM_SBIT(HWA_COMMON_MODULE_ENABLE_PAGER_ENABLE))
		| BCM_SBIT(HWA_COMMON_MODULE_ENABLE_BLOCKSTATISTICS_ENABLE));
	HWA_TXSTAT_EXPR(
		if (HWA_TX_CORES > 1)
			v32 |= BCM_SBIT(HWA_COMMON_MODULE_ENABLE_BLOCKTXSTS1_ENABLE));
	HWA_WR_REG_NAME(HWA00, dev->regs, common, module_enable, v32);

	HWA_RXPOST_EXPR(hwa_wlc_mac_event(dev, WLC_E_HWA_RX_REINIT));

	dev->reinit = FALSE;

}

void // Enable 1a 1b blocks.
hwa_rx_enable(hwa_dev_t *dev)
{
	HWA_RXPATH_EXPR({
		uint32 v32;

		HWA_FTRACE(HWA00);

		HWA_ASSERT(dev != (hwa_dev_t*)NULL);

		// Enable 1a 1b
		v32 = HWA_RD_REG_NAME(HWA00, dev->regs, common, module_enable);
		v32 |= (BCM_SBIT(HWA_COMMON_MODULE_ENABLE_BLOCKRXCORE0_ENABLE) |
			BCM_SBIT(HWA_COMMON_MODULE_ENABLE_BLOCKRXBM_ENABLE));
		if (HWA_RX_CORES > 1)
			v32 |= BCM_SBIT(HWA_COMMON_MODULE_ENABLE_BLOCKRXCORE1_ENABLE);
		HWA_WR_REG_NAME(HWA00, dev->regs, common, module_enable, v32);

		// Do SW rxlbufpool_fill after Host enable Rx module.
		HWA_PKTPGR_EXPR({
			uint32 is_empty;
			uint32 loop_count = HWA_LOOPCNT;

			// Wait for rxpf_empty_for_mac is 0 otherwise first time request
			// will failed with 0 packet count resp when dmasel is enabled.
			v32 = HWA_RD_REG_NAME(HWA1x, dev->regs, rx_core[0], rxfill_status0);
			is_empty = BCM_GBIT(v32, HWA_RX_RXFILL_STATUS0_RXPF_EMPTY_FOR_MAC);
			while (is_empty && loop_count--) {
				OSL_DELAY(2);
				v32 = HWA_RD_REG_NAME(HWA1x, dev->regs, rx_core[0], rxfill_status0);
				is_empty = BCM_GBIT(v32, HWA_RX_RXFILL_STATUS0_RXPF_EMPTY_FOR_MAC);
			}
			hwa_rxpath_rxlbufpool_fill(dev, 0);
		});
	});
} // hwa_rx_enable

void // Enable all blocks. DMA engines and BMs are enabled in INIT Phase
hwa_enable(hwa_dev_t *dev)
{
	uint32 v32;

	HWA_TRACE(("\n\n+++++ HWA[%d] ENABLE PHASE BEGIN +++++\n\n", dev->unit));
	HWA_FTRACE(HWA00);

	HWA_ASSERT(dev != (hwa_dev_t*)NULL);

	// DMA channels are enabled now
	HWA_DPC_EXPR(hwa_dma_enable(&dev->dma));

	// Enable all blocks except 1a 1b - not using hwa_module_request()
	v32 = (0U
		// RXDATA FHR and PktComparison are enabled in hwa_rxdata_init()
		HWA_CPLENG_EXPR(
			| BCM_SBIT(HWA_COMMON_MODULE_ENABLE_BLOCKCPL_ENABLE))
		HWA_TXPOST_EXPR(
			| BCM_SBIT(HWA_COMMON_MODULE_ENABLE_BLOCK3A_ENABLE))
		HWA_TXFIFO_EXPR(
			| BCM_SBIT(HWA_COMMON_MODULE_ENABLE_BLOCK3B_ENABLE))
		HWA_TXSTAT_EXPR(
			| BCM_SBIT(HWA_COMMON_MODULE_ENABLE_BLOCKTXSTS_ENABLE))
		HWA_PKTPGR_EXPR(
			| BCM_SBIT(HWA_COMMON_MODULE_ENABLE_PAGER_ENABLE))
		| BCM_SBIT(HWA_COMMON_MODULE_ENABLE_BLOCKSTATISTICS_ENABLE));
	HWA_TXSTAT_EXPR(
		if (HWA_TX_CORES > 1)
			v32 |= BCM_SBIT(HWA_COMMON_MODULE_ENABLE_BLOCKTXSTS1_ENABLE));
	HWA_WR_REG_NAME(HWA00, dev->regs, common, module_enable, v32);

	HWA_TRACE(("\n\n----- HWA[%d] ENABLE PHASE DONE -----\n\n", dev->unit));
} // hwa_enable

void // Disable all blocks. DMA engines.
hwa_disable(hwa_dev_t *dev)
{
	uint32 v32;

	HWA_TRACE(("\n\n+++++ HWA[%d] DISABLE PHASE BEGIN +++++\n\n", dev->unit));
	HWA_FTRACE(HWA00);

	HWA_ASSERT(dev != (hwa_dev_t*)NULL);

	// Disable all blocks except 3a, and cpl engine and pktpgr,
	// except 3b as well in packet pager mode.
	v32 = HWA_RD_REG_NAME(HWA00, dev->regs, common, module_enable);
	v32 &= (HWA_COMMON_MODULE_ENABLE_BLOCKCPL_ENABLE_MASK |
		HWA_PKTPGR_EXPR(HWA_COMMON_MODULE_ENABLE_PAGER_ENABLE_MASK |
		HWA_COMMON_MODULE_ENABLE_BLOCK3B_ENABLE_MASK |)
		HWA_COMMON_MODULE_ENABLE_BLOCK3A_ENABLE_MASK);
	HWA_WR_REG_NAME(HWA00, dev->regs, common, module_enable, v32);

	HWA_TRACE(("\n\n----- HWA[%d] DISABLE PHASE DONE -----\n\n", dev->unit));
}

void // Initialize HWA axi memory base address
hwa_init_axi_addr(hwa_dev_t *dev)
{
	uint32 cur_core_idx;
	uint32 axi_mem_addr = 0U;
#ifndef DONGLEBUILD
	uchar *bar_va;
	uint32 bar_size;
	wlc_info_t *wlc = (wlc_info_t *)dev->wlc;
#endif /* !DONGLEBUILD */

	cur_core_idx = si_coreidx(dev->sih); // save current core

	if (si_setcore(dev->sih, dev->coreid, /* unit = */ 0) != NULL) {

		axi_mem_addr = // Get the HWA axi memory base
			si_addrspace(dev->sih, CORE_SLAVE_PORT_1, CORE_BASE_ADDR_0);

		HWA_ASSERT(axi_mem_addr == HWA_AXI_BASE_ADDR);

	} else {
		HWA_ERROR(("HWA[%d]:%s: Failed to find HWA core<0x%08x> axi memory base\n",
			dev->unit, __FUNCTION__, dev->coreid));
		ASSERT(0);
	}

	(void) si_setcoreidx(dev->sih, cur_core_idx); // restore current core

	dev->axi_mem_base = axi_mem_addr;

#ifndef DONGLEBUILD
	// Using BAR1 to map HWA AXI Memory Region
	bar_size = wl_pcie_bar1(wlc->wl, &bar_va);

	ASSERT(bar_va != NULL && bar_size != 0);

	dev->bar1_va = (uint32 *)bar_va;

	// Current BAR1 default value is 8MB
	ASSERT(bar_size == 0x800000);
	dev->bar1_mask = bar_size - 1;

	// Only NIC HWA 4A use BAR1 for now.
	ASSERT(OSL_PCI_READ_CONFIG(dev->osh, PCI_BAR1_WIN, sizeof(uint32)) == 0);
	OSL_PCI_WRITE_CONFIG(dev->osh, PCI_BAR1_WIN, sizeof(uint32),
		axi_mem_addr & ~(dev->bar1_mask));
#endif /* !DONGLEBUILD */

	HWA_TRACE(("%s:HWA[%d]: axi memory base addr<0x%08x>\n",
		__FUNCTION__, dev->unit, (uint32)dev->axi_mem_base));

} // hwa_axi_addr

hwa_mem_addr_t // Fetch AXI address of an HWA object given its offset
hwa_axi_addr(hwa_dev_t *dev, const uint32 hwa_mem_offset)
{
	hwa_mem_addr_t axi_mem_addr;

#ifdef DONGLEBUILD
	axi_mem_addr = dev->axi_mem_base + hwa_mem_offset;
#else /* !DONGLEBUILD */
	axi_mem_addr = (hwa_mem_addr_t)dev->bar1_va;
	axi_mem_addr += (dev->axi_mem_base + hwa_mem_offset) & dev->bar1_mask;
#endif /* !DONGLEBUILD */

	HWA_TRACE(("%s:HWA[%d]: hwa_mem_offset<0x%x> addr<0x%p>\n",
		__FUNCTION__, dev->unit, hwa_mem_offset,
		(void *)(axi_mem_addr)));

	return axi_mem_addr;

} // hwa_axi_addr

// Errors reported within hwa::common::intstatus
#define HWA_INTSTATUS_ERRORS \
	(HWA_COMMON_INTSTATUS_ERROR_INT_MASK | \
	HWA_COMMON_INTSTATUS_TXSQUEUEFULL_INT_CORE0_MASK | \
	HWA_COMMON_INTSTATUS_TXSQUEUEFULL_INT_CORE1_MASK | \
	HWA_COMMON_INTSTATUS_CE2REG_INVALID_IND_UPDATE_MASK)

// Error reported within DMA Engine
#define HWA_DMA_ERRORS \
	(HWA_DMA_CH0INTSTATUS_DESCERR_MASK | \
	 HWA_DMA_CH0INTSTATUS_DATAERR_MASK | \
	 HWA_DMA_CH0INTSTATUS_DESCPROTOERR_MASK | \
	 HWA_DMA_CH0INTSTATUS_RCVDESCUF_MASK)

#define HWA_CHK_ERROR(blk, regname) \
({ \
	uint32 u32_val = HWA_RD_REG_NAME(HWA00, regs, blk, regname); \
	if (u32_val) HWA_WARN(("%s %s %s<0x%08x\n", HWA00, #blk, #regname, u32_val)); \
})

void // Lookup various error registers and dump. May be invoked in a poll timer
hwa_error(hwa_dev_t *dev)
{
	uint32 v32;
	hwa_regs_t *regs;

	if (dev == (hwa_dev_t*)NULL)
		return;

	regs = dev->regs;

	v32 = HWA_RD_REG_NAME(HWA00, regs, common, intstatus);
	if (v32 & HWA_INTSTATUS_ERRORS)
		HWA_WARN(("%s:HWA[%d] common intstatus<0x%08x>\n", HWA00, dev->unit, v32));
	HWA_CHK_ERROR(common, errorstatusreg);
	HWA_CHK_ERROR(common, directaxierrorstatus);

	HWA_DPC_EXPR({
		uint32 i;
		for (i = 0; i < dev->dma.channels; i++) {
			HWA_CHK_ERROR(dma, chint[i].status);

			v32 = HWA_RD_REG_NAME(HWAde, regs, dma, channels[i].tx.status1);
			if (BCM_GBF(v32, HWA_DMA_XMTSTATUS1_XMTERR))
				HWA_WARN(("%s dma xmt""status1<0x%08x>\n", HWA00, v32));
			v32 = HWA_RD_REG_NAME(HWAde, regs, dma, channels[i].rx.status1);
			if (BCM_GBF(v32, HWA_DMA_RCVSTATUS1_XMTERR)) // typo "XMT" ? RCVERR ...
				HWA_WARN(("%s dma rcv::status1<0x%08x>\n", HWA00, v32));
		}
	});

} // hwa_error

// XXX Invoke everytime MAC driver performs a MAC clkctlstatus register update
uint32 // HWA clock control
hwa_clkctl_request(hwa_dev_t *dev, hwa_clkctl_cmd_t cmd, bool enable)
{
	uint32 top32; // dma32;
	hwa_regs_t *regs;

	// Setup locals
	regs = dev->regs;

	top32 = HWA_RD_REG_NAME(HWA00, regs, top, clkctlstatus);
	// dma32 = HWA_RD_REG_NAME(HWA00, regs, dma, clockctlstatus);

	if (enable == TRUE) {
		// enable bit in clkctlstatus
		top32 |= (1 << cmd); // dma32 |= (1 << cmd);
	} else {
		if (cmd <= HWA_CLK_REQUEST_MAX) {
			// disable bit in clkctlstatus
			top32 &= ~(1 << cmd); // dma32 &= ~(1 << cmd);
		} else { // READ request of clk availability
			top32 = ((top32 & (1 << cmd)) >> cmd); // fetch current clkstatus
			goto done;
		}
	}

	HWA_WR_REG_NAME(HWA00, regs, top, clkctlstatus, top32);
	/*
	 * HWA::dma::clockctlstatus duplicated in HWA::top::clkctlstatus
	 * HWA::dma::clockctlstatus registers need not be programmed
	 *
	 * if (cmd < HWA_HWA_CLK_REQUEST)
	 *    HWA_WR_REG_NAME(HWA00, regs, dma, clockctlstatus, dma32);
	 */

done:
	HWA_TRACE(("%s clkctl cmd<%u> enable<%u> reg<0x%08x>\n",
		HWA00, cmd, enable, top32));

	return top32;

} // hwa_clkctl_request

// XXX Invoke everytime MAC driver performs a MAC powercontrol register update
uint32 // HWA power control
hwa_pwrctl_request(hwa_dev_t *dev, hwa_pwrctl_cmd_t cmd, uint32 arg_v32)
{
	uint32 v32;
	hwa_regs_t *regs;

	// Setup locals
	regs = dev->regs;

	v32 = HWA_RD_REG_NAME(HWA00, regs, top, powercontrol);
	switch (cmd) {
		case HWA_MEM_CLK_GATING:
			if (arg_v32)
				v32 |= BCM_SBF(arg_v32, HWA_TOP_POWERCONTROL_ENABLE_MEM_CLK_GATING);
			else
				v32 = BCM_CBF(v32, HWA_TOP_POWERCONTROL_ENABLE_MEM_CLK_GATING);
			break;
		case HWA_POWER_KEEP_ON:
			if (arg_v32)
				v32 |= BCM_SBF(arg_v32, HWA_TOP_POWERCONTROL_POWERCTL);
			else
				v32 = BCM_CBF(v32, HWA_TOP_POWERCONTROL_POWERCTL);
			break;
		default:
			break;
	}

	HWA_WR_REG_NAME(HWA00, regs, top, powercontrol, v32);

	HWA_TRACE(("%s pwrctl cmd<%u> arg_v32<%u> reg<0x%08x>\n",
			HWA00, cmd, arg_v32, v32));

	return v32;

} // hwa_pwrctl_request

uint32 // Control a HWA module
hwa_module_request(hwa_dev_t *dev, hwa_module_block_t blk,
	hwa_module_cmd_t cmd, bool enable)
{
	uint32 v32;
	hwa_regs_t *regs;

	// Audit parameters and pre-conditions
	HWA_AUDIT_DEV(dev);

	// Setup locals
	regs = dev->regs;

	switch (cmd) {
		case HWA_MODULE_CLK_ENABLE:
			v32 = HWA_RD_REG_NAME(HWA00, regs, common, module_clk_enable);
			break;
		case HWA_MODULE_CLK_GATING:
			v32 = HWA_RD_REG_NAME(HWA00, regs, common, module_clkgating_enable);
			break;
		case HWA_MODULE_RESET:
			v32 = HWA_RD_REG_NAME(HWA00, regs, common, module_reset);
			break;
		case HWA_MODULE_CLK_AVAIL:
			v32 = HWA_RD_REG_NAME(HWA00, regs, common, module_clkavail);
			v32 = ((v32 & (1 << blk)) >> blk);
			goto done;
		case HWA_MODULE_ENABLE:
			v32 = HWA_RD_REG_NAME(HWA00, regs, common, module_enable);
			break;
		case HWA_MODULE_IDLE:
			v32 = HWA_RD_REG_NAME(HWA00, regs, common, module_idle);
			v32 = ((v32 & (1 << blk)) >> blk);
			goto done;
		default:
			HWA_WARN(("%s: HWA[%d] module blk<%u> cmd<%u> enable<%u> failure\n",
				HWA00, dev->unit, blk, cmd, enable));
			return -1;
	}

	if (enable == TRUE) {
		v32 |= (1 << blk); // enable bit for requested module block
	} else {
		v32 &= ~(1 << blk); // disable bit for requested module block
	}

	switch (cmd) {
		case HWA_MODULE_CLK_ENABLE:
			HWA_WR_REG_NAME(HWA00, regs, common, module_clk_enable, v32);
			break;
		case HWA_MODULE_CLK_GATING:
			HWA_WR_REG_NAME(HWA00, regs, common, module_clkgating_enable, v32);
			break;
		case HWA_MODULE_RESET:
			HWA_WR_REG_NAME(HWA00, regs, common, module_reset, v32); break;
		case HWA_MODULE_ENABLE:
			HWA_WR_REG_NAME(HWA00, regs, common, module_enable, v32); break;
		default:
			return -1;
	}

done:
	HWA_TRACE(("%s: HWA[%d]:  module blk<%u> cmd<%u> enable<%u> reg<0x%08x>\n",
		HWA00, dev->unit, blk, cmd, enable, v32));

	return v32;

} // hwa_module_request

/**
 * HWA2.0 only provides a single Completion Ring interrupt. Runner requires
 * a unique TxCpl<addr,val> and RxCpl<addr,val> mechanism to allow TxCompletions
 * and RxCompletions to be redirected to explicit Runner Cores on which explicit
 * Runner HW threads will be kick started.
 *
 * For RxPost RD index update and TxPost RD index update, presently, dongle does
 * not issue a doorbell interrupt. DHD uses Lazy RD fetch for RxPost and TxPost
 * production. If dongle needs to explicitly deliver an interrupt on RD updates
 * for RxPost or TxPost, then the legact PCIE doorbell FN0 register's address
 * and a dummy value (0xdeadbeef) may be registered.
 */
void // Register a software doorbell
hwa_sw_doorbell_request(struct hwa_dev *dev, hwa_sw_doorbell_t request,
	uint32 index, dma64addr_t haddr64, uint32 value)
{
	uint32 v32;
	hwa_regs_t *regs;

	HWA_TRACE(("%s sw doorbell<%u> haddr64<0x%08x,0x%08x> value<0x%08x>\n",
		HWA00, request, haddr64.hiaddr, haddr64.loaddr, value));

	// Audit parameters and pre-conditions
	HWA_AUDIT_DEV(dev);

	// Setup locals
	regs = dev->regs;

	v32 = HWA_RD_REG_NAME(HWA00, regs, common, intr_control);

	switch (request) {
		case HWA_TX_DOORBELL:
			HWA_WR_REG_NAME(HWA00, regs, common, txintraddrlo, haddr64.loaddr);
			HWA_WR_REG_NAME(HWA00, regs, common, txintraddrhi, haddr64.hiaddr);
			HWA_WR_REG_NAME(HWA00, regs, common, txintrval, value);
			v32 |= (0U
				| BCM_SBIT(HWA_COMMON_INTR_CONTROL_TXHOSTINTR_EN)
				| BCM_SBIT(HWA_COMMON_INTR_CONTROL_USEVAL_TX)
				| 0U);
			break;

		case HWA_RX_DOORBELL:
			HWA_WR_REG_NAME(HWA00, regs, common, rxintraddrlo, haddr64.loaddr);
			HWA_WR_REG_NAME(HWA00, regs, common, rxintraddrhi, haddr64.hiaddr);
			HWA_WR_REG_NAME(HWA00, regs, common, rxintrval, value);
			v32 |= (0U
				| BCM_SBIT(HWA_COMMON_INTR_CONTROL_RXHOSTINTR_EN)
				| BCM_SBIT(HWA_COMMON_INTR_CONTROL_USEVAL_RX)
				| 0U);
			break;

		case HWA_TXCPL_DOORBELL:
			HWA_WR_REG_NAME(HWA00, regs, common, cplintr_tx_addrlo, haddr64.loaddr);
			HWA_WR_REG_NAME(HWA00, regs, common, cplintr_tx_addrhi, haddr64.hiaddr);
			HWA_WR_REG_NAME(HWA00, regs, common, cplintr_tx_val, value);
			v32 |= (0U
				| BCM_SBIT(HWA_COMMON_INTR_CONTROL_CPLHOSTINTR_EN)
				| BCM_SBIT(HWA_COMMON_INTR_CONTROL_USEVAL_CPLTX)
				| 0U);

			break;

		case HWA_RXCPL_DOORBELL:
			HWA_ASSERT(index < 4);
			HWA_WR_REG_NAME(HWA00, regs, common, cplintr_rx[index].addrlo,
				haddr64.loaddr);
			HWA_WR_REG_NAME(HWA00, regs, common, cplintr_rx[index].addrhi,
				haddr64.hiaddr);
			HWA_WR_REG_NAME(HWA00, regs, common, cplintr_rx[index].val, value);
			v32 |= (0U
				| BCM_SBIT(HWA_COMMON_INTR_CONTROL_CPLHOSTINTR_EN)
				| BCM_SBF((1 << index), HWA_COMMON_INTR_CONTROL_USEVAL_CPLRX)
				| 0U);
			break;

		default: HWA_WARN(("%s doorbell<%u> failure\n", HWA00, request));
			HWA_ASSERT(0);
	}

	v32 |= BCM_SBF(dev->host_coherency, HWA_COMMON_INTR_CONTROL_TEMPLATE_COHERENT);
	HWA_WR_REG_NAME(HWA00, regs, common, intr_control, v32);

} // hwa_sw_doorbell_request

void
hwa_print_pooluse(struct hwa_dev *dev)
{
	if (dev == (hwa_dev_t*)NULL)
		return;

#if defined(HWA_TXPOST_BUILD) || defined(HWA_RXFILL_BUILD)
	NO_HWA_PKTPGR_EXPR({
		HWA_TXPOST_EXPR(hwa_bm_t *txbm = &dev->tx_bm);
		HWA_RXFILL_EXPR(hwa_bm_t *rxbm = &dev->rx_bm);
		HWA_TXPOST_EXPR(uint32 txbm_mem_sz = (txbm->pkt_size * txbm->pkt_total));
		HWA_RXFILL_EXPR(uint32 rxbm_mem_sz = (rxbm->pkt_size * rxbm->pkt_total));
		HWA_PRINT("\tIn use");
		HWA_TXPOST_EXPR(HWA_PRINT(" TxBM %d(%d/%d)(%dK)",
			txbm->pkt_total, hwa_bm_avail(txbm),
			txbm->avail_sw, KB(txbm_mem_sz)));
		HWA_RXFILL_EXPR(HWA_PRINT(" RxBM %d(%d)(%dK)",
			rxbm->pkt_total, hwa_bm_avail(rxbm),
			KB(rxbm_mem_sz)));
		HWA_PRINT("\n");
	});
#endif // endif

	HWA_PKTPGR_EXPR(hwa_pktpgr_print_pooluse(dev));
}

#if defined(BCMDBG) || defined(HWA_DUMP)

void // Dump all HWA blocks
hwa_dump(struct hwa_dev *dev, struct bcmstrbuf *b, uint32 block_bitmap,
	bool verbose, bool dump_regs, bool dump_txfifo_shadow, uint8 *fifo_bitmap)
{
	if (dev == (hwa_dev_t*)NULL)
		return;

	HWA_BPRINT(b, "HWA[%d][%s] driver<%d> dev<%p> regs<%p> "
		"intmask<0x%08x> defintmask<0x%08x> intstatus<0x%08x> "
		"core id<%u> rev<%u>\n",
		dev->unit, HWA_FAMILY(dev->corerev), HWA_DRIVER_VERSION,
		dev, dev->regs, dev->intmask, dev->defintmask, dev->intstatus,
		dev->coreid, dev->corerev);
	HWA_BPRINT(b, "HWA [ ");
	HWA_RXPOST_EXPR(HWA_BPRINT(b, "1A ")); HWA_RXFILL_EXPR(HWA_BPRINT(b, "1B "));
	HWA_RXDATA_EXPR(HWA_BPRINT(b, "2A ")); HWA_RXCPLE_EXPR(HWA_BPRINT(b, "2B "));
	HWA_TXPOST_EXPR(HWA_BPRINT(b, "3A ")); HWA_TXFIFO_EXPR(HWA_BPRINT(b, "3B "));
	HWA_TXSTAT_EXPR(HWA_BPRINT(b, "4A ")); HWA_TXCPLE_EXPR(HWA_BPRINT(b, "4B "));
	HWA_PKTPGR_EXPR(HWA_BPRINT(b, "PP "));
	HWA_BPRINT(b, "] blocks are enabled\n");

	HWA_BUS_EXPR(
		HWA_BPRINT(b, "+ osh<%p> sih<%p> pcie_sh<%p> pcie_ipc_rings<%p> wi_aggr<%u>\n",
			dev->osh, dev->sih, dev->pcie_ipc,
			dev->pcie_ipc_rings, dev->wi_aggr_cnt));

	HWA_BPRINT(b, "+ driver_mode<%s> macif<%s, %s> host<%s, %s, 0x%08x>\n",
		(dev->driver_mode == HWA_FD_MODE) ? "FD" : "NIC",
		(dev->macif_placement == HWA_MACIF_IN_DNGLMEM) ? "DNGL" : "HOST",
		(dev->macif_coherency == HWA_HW_COHERENCY) ? "HW" : "SW",
		(dev->host_coherency == HWA_HW_COHERENCY) ? "HW" : "SW",
		(dev->host_addressing == HWA_64BIT_ADDRESSING) ? "64b" : "32b",
		dev->host_physaddrhi);

	if (verbose) {
		int cb;
		for (cb = 0; cb < HWA_CALLBACK_MAX; cb++) {
			HWA_BPRINT(b, "+ hwa_handler %u <%p, %p>\n",
				cb, dev->handlers[cb].context, dev->handlers[cb].callback);
		}
	}

#if defined(WLTEST) || defined(HWA_DUMP)
	if (dump_regs) {
		hwa_regs_dump(dev, b,
			(block_bitmap & (HWA_DUMP_TOP|HWA_DUMP_CMN))); // top and common only
	}
#endif // endif
	if (block_bitmap & HWA_DUMP_PP) {
		HWA_PKTPGR_EXPR(hwa_pktpgr_dump(&dev->pktpgr, b, verbose, dump_regs,
			fifo_bitmap)); // HWApp
	}

	if (block_bitmap & HWA_DUMP_DMA) {
		HWA_DPC_EXPR(hwa_dma_dump(&dev->dma, b, verbose, dump_regs)); // dump DMA channels
	}
	// PKTPGR use HDBM and DDBM configured in hwa_pktpgr.c
#if !defined(HWA_PKTPGR_BUILD)
	if (block_bitmap & (HWA_DUMP_1A|HWA_DUMP_1B)) {
		HWA_RXFILL_EXPR(hwa_bm_dump(&dev->rx_bm, b, verbose, dump_regs)); // HWA rx_bm
	}
	if (block_bitmap & (HWA_DUMP_3A|HWA_DUMP_3B)) {
		HWA_TXPOST_EXPR(hwa_bm_dump(&dev->tx_bm, b, verbose, dump_regs)); // HWA tx_bm
	}
#endif // endif
	if (block_bitmap & HWA_DUMP_1A) {
		HWA_RXPOST_EXPR(hwa_rxpost_dump(&dev->rxpost, b, verbose)); // HWA1a
	}
	if (block_bitmap & HWA_DUMP_1B) {
		HWA_RXFILL_EXPR(hwa_rxfill_dump(&dev->rxfill, b, verbose)); // HWA1b
	}
	if (block_bitmap & (HWA_DUMP_1A|HWA_DUMP_1B)) {
		HWA_RXPATH_EXPR(hwa_rxpath_dump(&dev->rxpath, b, verbose, dump_regs)); // HWA1x
	}
	if (block_bitmap & HWA_DUMP_2A) {
		HWA_RXDATA_EXPR(hwa_rxdata_dump(&dev->rxdata, b, verbose)); // HWA2a
	}
	if (block_bitmap & HWA_DUMP_3A) {
		HWA_TXPOST_EXPR(hwa_txpost_dump(&dev->txpost, b, verbose, dump_regs)); // HWA3a
	}
	if (block_bitmap & HWA_DUMP_3B) {
		HWA_TXFIFO_EXPR(hwa_txfifo_dump(&dev->txfifo, b, verbose, dump_regs,
			dump_txfifo_shadow, fifo_bitmap)); // HWA3b
	}
	if (block_bitmap & HWA_DUMP_4A) {
		HWA_TXSTAT_EXPR(hwa_txstat_dump(&dev->txstat, b, verbose, dump_regs)); // HWA4a
	}
	if (block_bitmap & (HWA_DUMP_2B|HWA_DUMP_4B)) {
		HWA_CPLENG_EXPR(hwa_cpleng_dump(&dev->cpleng, b, verbose, dump_regs)); // HWAce
	}

} // hwa_dump

#if defined(WLTEST) || defined(HWA_DUMP)

void // Dump all HWA block registers or just top and common blocks
hwa_regs_dump(hwa_dev_t *dev, struct bcmstrbuf *b, uint32 block_bitmap)
{
	hwa_regs_t *regs;

	if (dev == (hwa_dev_t*)NULL)
		return;

	regs = dev->regs;

	HWA_BPRINT(b, "HWA[%d][%s] rev<%d> driver<%d> dev<%p> registers<%p>\n",
		dev->unit, HWA_FAMILY(dev->corerev), dev->corerev, HWA_DRIVER_VERSION, dev, regs);

	if (block_bitmap & HWA_DUMP_TOP) {
		// TOP block
		hwa_top_regs_t *top = &regs->top;
		HWA_BPRINT(b, "HWA Top: registers[%p] offset[0x%04x]\n",
			top, (uint32)OFFSETOF(hwa_regs_t, top));
		HWA_BPR_REG(b, top, debug_fifobase);
		HWA_BPR_REG(b, top, debug_fifodata_lo);
		HWA_BPR_REG(b, top, debug_fifodata_hi);
		HWA_BPR_REG(b, top, clkctlstatus);
		HWA_BPR_REG(b, top, workaround);
		HWA_BPR_REG(b, top, powercontrol);
		HWA_BPR_REG(b, top, hwahwcap1);
		HWA_BPR_REG(b, top, hwahwcap2);
	}

	if (block_bitmap & HWA_DUMP_CMN) {
		// COMMON block
		hwa_common_regs_t *common = &regs->common;
		HWA_BPRINT(b, "%s registers[%p] offset[0x%04x]\n",
			HWA00, common, (uint32)OFFSETOF(hwa_regs_t, common));
		HWA_BPR_REG(b, common, rxpost_wridx_r0);
		HWA_BPR_REG(b, common, rxpost_wridx_r1);

		if (HWAREV_GE(dev->corerev, 131)) {
			HWA_BPR_REG(b, common, dma_desc_chk_ctrl);
			HWA_BPR_REG(b, common, dma_desc_chk_th);
			HWA_BPR_REG(b, common, dma_desc_chk_sts0);
			HWA_BPR_REG(b, common, dma_desc_chk_sts1);
			HWA_BPR_REG(b, common, dma_desc_chk_sts2);
		}

		HWA_BPR_REG(b, common, intstatus);
		HWA_BPR_REG(b, common, intmask);
		HWA_BPR_REG(b, common, statscontrolreg);
		HWA_BPR_REG(b, common, statdonglreaddressreg);
		HWA_BPR_REG(b, common, statminbusytimereg);
		HWA_BPR_REG(b, common, errorstatusreg);
		HWA_BPR_REG(b, common, directaxierrorstatus);
		HWA_BPR_REG(b, common, statdonglreaddresshireg);
		HWA_BPR_REG(b, common, txintraddr_lo);
		HWA_BPR_REG(b, common, txintraddr_hi);
		HWA_BPR_REG(b, common, rxintraddr_lo);
		HWA_BPR_REG(b, common, rxintraddr_hi);
		HWA_BPR_REG(b, common, cplintr_tx_addr_lo);
		HWA_BPR_REG(b, common, cplintr_tx_addr_hi);
		HWA_BPR_REG(b, common, cplintr_tx_val);
		HWA_BPR_REG(b, common, intr_control);
		HWA_BPR_REG(b, common, module_clk_enable);
		HWA_BPR_REG(b, common, module_clkgating_enable);
		HWA_BPR_REG(b, common, module_reset);
		HWA_BPR_REG(b, common, module_clkavail);
		HWA_BPR_REG(b, common, module_clkext);
		HWA_BPR_REG(b, common, module_enable);
		HWA_BPR_REG(b, common, module_idle);
		if (HWAREV_GE(dev->corerev, 131)) {
			HWA_BPR_REG(b, common, dmatxsel);
			HWA_BPR_REG(b, common, dmarxsel);
		}
		HWA_BPR_REG(b, common, gpiomuxcfg);
		HWA_BPR_REG(b, common, gpioout);
		HWA_BPR_REG(b, common, gpiooe);
		HWA_BPR_REG(b, common, mac_base_addr_core0);
		HWA_BPR_REG(b, common, mac_base_addr_core1);
		HWA_BPR_REG(b, common, mac_frmtxstatus);
		HWA_BPR_REG(b, common, mac_dma_ptr);
		HWA_BPR_REG(b, common, mac_ind_xmtptr);
		HWA_BPR_REG(b, common, mac_ind_qsel);
		HWA_BPR_REG(b, common, hwa2hwcap);
		HWA_BPR_REG(b, common, hwa2swpkt_high32_pa);
		HWA_BPR_REG(b, common, hwa2pciepc_pkt_high32_pa);
		HWA_BPR_REG(b, common, cplintr_rx[0].addr_lo);
		HWA_BPR_REG(b, common, cplintr_rx[0].addr_hi);
		HWA_BPR_REG(b, common, cplintr_rx[0].val);
		HWA_BPR_REG(b, common, cplintr_rx[1].addr_lo);
		HWA_BPR_REG(b, common, cplintr_rx[1].addr_hi);
		HWA_BPR_REG(b, common, cplintr_rx[1].val);
		HWA_BPR_REG(b, common, cplintr_rx[2].addr_lo);
		HWA_BPR_REG(b, common, cplintr_rx[2].addr_hi);
		HWA_BPR_REG(b, common, cplintr_rx[2].val);
		HWA_BPR_REG(b, common, cplintr_rx[3].addr_lo);
		HWA_BPR_REG(b, common, cplintr_rx[3].addr_hi);
		HWA_BPR_REG(b, common, cplintr_rx[3].val);
		if (HWAREV_GE(dev->corerev, 131)) {
#ifdef HWA_PKTPGR_BUILD
			HWA_BPR_REG(b, common, pageintstatus);
			HWA_BPR_REG(b, common, pageintmask);
#endif /* HWA_PKTPGR_BUILD */
			HWA_BPR_REG(b, common, datacaptureaddrlo);
			HWA_BPR_REG(b, common, datacaptureaddrhi);
			HWA_BPR_REG(b, common, datacapturecfg);
			HWA_BPR_REG(b, common, datacapturectrl);
			HWA_BPR_REG(b, common, datacapturewridx);
			HWA_BPR_REG(b, common, dcdataloading);
			HWA_BPR_REG(b, common, dcmaskbit);
			HWA_BPR_REG(b, common, dctriggersig);
			HWA_BPR_REG(b, common, dmatx0ctrl);
			HWA_BPR_REG(b, common, dmatx1ctrl);
			HWA_BPR_REG(b, common, dmarx0ctrl);
			HWA_BPR_REG(b, common, dmarx1ctrl);
		}
	}

	// DMA
	if (block_bitmap & HWA_DUMP_DMA) {
		HWA_DPC_EXPR(hwa_dma_regs_dump(&dev->dma, b));
	}

	// PKTPGR use HDBM and DDBM configured in hwa_pktpgr.c
#if !defined(HWA_PKTPGR_BUILD)
	// RXBM, TXBM
	if (block_bitmap & (HWA_DUMP_1A | HWA_DUMP_1B)) {
		HWA_RXFILL_EXPR(hwa_bm_regs_dump(&dev->rx_bm, b));
	}
	if (block_bitmap & (HWA_DUMP_3A | HWA_DUMP_3B)) {
		HWA_TXPOST_EXPR(hwa_bm_regs_dump(&dev->tx_bm, b));
	}
#endif // endif

	// RXPATH(RXPOST, RXFIFO)
	if (block_bitmap & (HWA_DUMP_1A|HWA_DUMP_1B)) {
		HWA_RXPATH_EXPR(hwa_rxpath_regs_dump(&dev->rxpath, b));
	}

	// TXPOST, TXFIFO and TXSTAT
	if (block_bitmap & HWA_DUMP_3A) {
		HWA_TXPOST_EXPR(hwa_txpost_regs_dump(&dev->txpost, b));
	}
	if (block_bitmap & HWA_DUMP_3B) {
		HWA_TXFIFO_EXPR(hwa_txfifo_regs_dump(&dev->txfifo, b));
	}
	if (block_bitmap & HWA_DUMP_4A) {
		HWA_TXSTAT_EXPR(hwa_txstat_regs_dump(&dev->txstat, b));
	}

	// TxCPLE and RxCPLE
	if (block_bitmap & (HWA_DUMP_2B|HWA_DUMP_4B)) {
		HWA_CPLENG_EXPR(hwa_cpleng_regs_dump(&dev->cpleng, b));
	}

	// PKTPGR
	if (block_bitmap & HWA_DUMP_PP) {
		HWA_PKTPGR_EXPR(hwa_pktpgr_regs_dump(&dev->pktpgr, b));
	}
} // hwa_regs_dump

void // Read a single register in HWA given its offset in HWA regs
hwa_reg_read(hwa_dev_t *dev, uint32 reg_offset)
{
	uint32 v32;
	uintptr reg_base;

	if (dev == (hwa_dev_t*)NULL)
		return;

	reg_base = HWA_PTR2UINT(dev->regs);

	v32 = HWA_RD_REG((uintptr)reg_base + reg_offset);

	HWA_PRINT("HWA[%d] REGISTER[0x%p] offset<0x%04x> v32[0x%08x, %10u]\n",
		dev->unit, (void*)(reg_base + reg_offset), reg_offset, v32, v32);

} // hwa_reg_read

#endif // endif

#endif // endif

#if defined(WLTEST) || defined(HWA_DUMP) || defined(BCMDBG_ERR)
static int
hwa_dbg_get_regaddr(hwa_regs_t *regs, char *type, uintptr *reg_addr)
{
	*reg_addr = 0;

	if (!strcmp(type, "top")) {
		// From regs@(offset 0) instead of top@(offset 0x104)
		*reg_addr = HWA_PTR2UINT(regs);
	} else if (!strcmp(type, "cmn")) {
		*reg_addr = HWA_PTR2UINT(&regs->common);
	} else if (!strcmp(type, "dma")) {
		*reg_addr = HWA_PTR2UINT(&regs->dma);
	} else if (!strcmp(type, "rx")) {
		HWA_RXPATH_EXPR(*reg_addr = HWA_PTR2UINT(&regs->rx_core[0]));  //core 0
	} else if (!strcmp(type, "rxbm")) {
		HWA_RXPATH_EXPR(*reg_addr = HWA_PTR2UINT(&regs->rx_bm));
	} else if (!strcmp(type, "tx")) {
		HWA_TXPOST_EXPR(*reg_addr = HWA_PTR2UINT(&regs->tx));
	} else if (!strcmp(type, "txbm")) {
		HWA_TXPOST_EXPR(*reg_addr = HWA_PTR2UINT(&regs->tx_bm));
	} else if (!strcmp(type, "txdma")) {
		HWA_TXFIFO_EXPR(*reg_addr = HWA_PTR2UINT(&regs->txdma));
	} else if (!strcmp(type, "txs")) {
		HWA_TXSTAT_EXPR(*reg_addr = HWA_PTR2UINT(&regs->tx_status[0])); //core 0
	} else if (!strcmp(type, "cpl")) {
		HWA_CPLENG_EXPR(*reg_addr = HWA_PTR2UINT(&regs->cpl));
	} else if (!strcmp(type, "pp")) {
		HWA_PKTPGR_EXPR(*reg_addr = HWA_PTR2UINT(&regs->pager));
	} else {
		return BCME_BADARG;
	}

	if (*reg_addr == 0)
		return BCME_BADARG;

	return BCME_OK;
}

int
hwa_dbg_regread(struct hwa_dev *dev, char *type, uint32 reg_offset, int32 *ret_int_ptr)
{
	int ret = BCME_OK;
	uintptr reg_addr;

	if (dev == (hwa_dev_t*)NULL)
		return BCME_ERROR;

	ret = hwa_dbg_get_regaddr(dev->regs, type, &reg_addr);
	if (ret != BCME_OK)
		return ret;

	*ret_int_ptr = HWA_RD_REG((uintptr)reg_addr + reg_offset);

	return ret;
}

#if defined(WLTEST)
int
hwa_dbg_regwrite(struct hwa_dev *dev, char *type, uint32 reg_offset, uint32 val)
{
	int ret = BCME_OK;
	uintptr reg_addr;

	if (dev == (hwa_dev_t*)NULL)
		return BCME_ERROR;

	ret = hwa_dbg_get_regaddr(dev->regs, type, &reg_addr);
	if (ret != BCME_OK)
		return ret;

	HWA_WR_REG(((uintptr)reg_addr + reg_offset), val);

	return BCME_OK;
}
#endif // endif
#endif // endif

#ifdef HWA_DPC_BUILD
/* ========= Common ISR/DPC handle functions ========= */
void BCMFASTPATH
hwa_intrson(hwa_dev_t *dev)
{
	uint32 v32;

	HWA_FTRACE(HWA00);

	HWA_ASSERT(dev != (hwa_dev_t*)NULL);
	HWA_ASSERT(dev->regs != (hwa_regs_t*)NULL);

	dev->intmask = dev->defintmask;
	v32 = HWA_RD_REG_NAME(HWA00, dev->regs, common, intmask);
	v32 |= dev->intmask;
	HWA_WR_REG_NAME(HWA00, dev->regs, common, intmask, v32);
}

void BCMFASTPATH
hwa_intrsoff(hwa_dev_t *dev)
{
	uint32 v32;

	HWA_FTRACE(HWA00);

	HWA_ASSERT(dev != (hwa_dev_t*)NULL);
	HWA_ASSERT(dev->regs != (hwa_regs_t*)NULL);

	v32 = HWA_RD_REG_NAME(HWA00, dev->regs, common, intmask);
	v32 &= ~dev->intmask;
	HWA_WR_REG_NAME(HWA00, dev->regs, common, intmask, v32);

	dev->intmask = 0;
}

void BCMFASTPATH
hwa_intrsupd(hwa_dev_t *dev)
{
	uint32 intstatus;

	HWA_FTRACE(HWA00);

	HWA_ASSERT(dev != (hwa_dev_t*)NULL);

	/* read and clear intstatus */
	intstatus = hwa_intstatus(dev);

	/* update interrupt status in software */
	dev->intstatus |= intstatus;
}

uint32 BCMFASTPATH
hwa_intstatus(hwa_dev_t *dev)
{
	uint32 v32;

	HWA_FTRACE(HWA00);

	HWA_ASSERT(dev != (hwa_dev_t*)NULL);
	HWA_ASSERT(dev->regs != (hwa_regs_t*)NULL);

	v32 = HWA_RD_REG_NAME(HWA00, dev->regs, common, intstatus);
	v32 &= dev->defintmask;
	if (v32) {
		HWA_WR_REG_NAME(HWA00, dev->regs, common, intstatus, v32);
		(void)HWA_RD_REG_NAME(HWA00, dev->regs, common, intstatus); /* sync readback */
	}

	return v32;
}

bool
hwa_dispatch(hwa_dev_t *dev)
{
	uint32 intstatus;

	HWA_FTRACE(HWA00);

	HWA_ASSERT(dev != (hwa_dev_t*)NULL);

	/* read and clear intstatus */
	intstatus = hwa_intstatus(dev);

	/* assign interrupt status in software */
	dev->intstatus = intstatus;

	return intstatus ? TRUE : FALSE;
}

bool
hwa_txdma_worklet(hwa_dev_t *dev, uint32 intstatus)
{
	return FALSE;
}

/* Run DPC,
 * Return TRUE : need re-schedule
 * Return FALSE: no re-schedule
 */
bool
hwa_worklet(hwa_dev_t *dev)
{
	uint32 intstatus;

	HWA_FTRACE(HWA00);

	HWA_ASSERT(dev != (hwa_dev_t*)NULL);

	intstatus = dev->intstatus;
	dev->intstatus = 0;

	if (intstatus == 0)
		return FALSE;  // no re-schedule

	// PktPager
	HWA_PKTPGR_EXPR({
	if (intstatus & HWA_COMMON_INTSTATUS_PACKET_PAGER_INTR_MASK) {
		uint32 v32;
		uint32 pageintstatus;

		// page_intrsoff
		v32 = HWA_RD_REG_NAME(HWApp, dev->regs, common, pageintmask);
		v32 &= ~dev->pageintmask;
		HWA_WR_REG_NAME(HWApp, dev->regs, common, pageintmask, v32);

		// Get page_intstatus
		pageintstatus = dev->pageintstatus; // old intstatus
		dev->pageintstatus = 0;
		pageintstatus |= HWA_RD_REG_NAME(HWApp, dev->regs, common, pageintstatus);
		pageintstatus &= dev->pageintmask;
		if (pageintstatus) {
			// ACK common::pageintstatus.
			HWA_WR_REG_NAME(HWApp, dev->regs, common, pageintstatus,
				pageintstatus);
			(void)HWA_RD_REG_NAME(HWApp, dev->regs, common,
				pageintstatus); /* sync readback */

			// PAGEIN response
			// HWA_PKTPGR_PAGEIN_RXPROCESS
			// HWA_PKTPGR_PAGEIN_TXSTATUS
			// HWA_PKTPGR_PAGEIN_TXPOST
			if (pageintstatus & HWA_COMMON_PAGEIN_INT_MASK) {
				// XXX, PageIn ErrInts are not handled yet!
				hwa_pktpgr_response(dev, &dev->pktpgr.pagein_rsp_ring,
					HWA_PKTPGR_PAGEIN_CALLBACK);
			}

			// PAGEOUT response
			//  HWA_PKTPGR_PAGEOUT_PKTLIST
			//  HWA_PKTPGR_PAGEOUT_LOCAL
			if (pageintstatus & HWA_COMMON_PAGEOUT_INT_MASK) {
				// XXX, PageOut ErrInts are not handled yet!
				hwa_pktpgr_response(dev, &dev->pktpgr.pageout_rsp_ring,
					HWA_PKTPGR_PAGEOUT_CALLBACK);
			}

			// PAGEMGR response
			//  HWA_PKTPGR_PAGEMGR_ALLOC_RX
			//  HWA_PKTPGR_PAGEMGR_ALLOC_RX_RPH
			//  HWA_PKTPGR_PAGEMGR_ALLOC_TX
			//  HWA_PKTPGR_PAGEMGR_PUSH
			//  HWA_PKTPGR_PAGEMGR_PULL
			//  HWA_PKTPGR_PAGEMGR_PUSH_PKTTAG
			//  HWA_PKTPGR_PAGEMGR_PULL_KPFL_LINK
			if (pageintstatus & HWA_COMMON_PAGEMGR_INT_MASK) {
				// XXX, PageMgr ErrInts are not handled yet!
				hwa_pktpgr_response(dev, &dev->pktpgr.pagemgr_rsp_ring,
					HWA_PKTPGR_PAGEMGR_CALLBACK);
			}

			// For debug notification
			if (pageintstatus & HWA_COMMON_PAGERBM_INT_MASK) {
				HWA_PRINT("%s%s\n",
				(pageintstatus & HWA_COMMON_PAGEINTSTATUS_PPHDBMTH1INT_MASK) ?
					"HDBM-TH1 " : "",
				(pageintstatus & HWA_COMMON_PAGEINTSTATUS_PPDDBMTH1INT_MASK) ?
					"DDBM-TH1 " : "");
			}
		}

		// page_intrson
		v32 = HWA_RD_REG_NAME(HWApp, dev->regs, common, pageintmask);
		v32 |= dev->pageintmask;
		HWA_WR_REG_NAME(HWApp, dev->regs, common, pageintmask, v32);
	}
	});

#if defined(HWA_TXPOST_BUILD) && !defined(HWA_PKTPGR_BUILD)
	// HWA3a TxPOST.  TX enter point
	if (intstatus & HWA_COMMON_INTSTATUS_TXPKTCHN_INT_MASK) {
		(void)hwa_txpost_pktchain_process(dev);
	}
#endif // endif

	if ((!dev->up) || (dev->reinit)) {
		goto done;
	}

#ifdef HWA_RXFILL_BUILD
	// HWA1b RxFILL.
	// RX enter point triggered by d11bdest wr index is updated in memory.
	if (intstatus & HWA_COMMON_INTSTATUS_D11BDEST0_INT_MASK) {
#if defined(HWA_PKTPGR_BUILD)
		if (dev->pktpgr.pgi_rxpkt_req == 0) {
			(void)hwa_rxfill_pagein_rx_req(dev, 0, HWA_D11BDEST_RXPKT_READY_INVALID,
				HWA_PROCESS_BOUND); // core 0
		}
#else
		(void)hwa_rxfill_rxbuffer_process(dev, 0, HWA_PROCESS_BOUND); // core 0
#endif // endif
	}
#endif /* HWA_RXFILL_BUILD */

#ifdef HWA_TXSTAT_BUILD
	// HWA4a TxSTAT.
	// TX status enter point triggered by txs entries are written in memory.
	if (intstatus & HWA_COMMON_INTSTATUS_TXSWRINDUPD_INT_CORE0_MASK) {
		(void)hwa_txstat_process(dev, 0, HWA_PROCESS_BOUND); // core 0
	}
#endif // endif
done:
	/* In each worklet functions they may update the dev->intstatus to request re-schedule */
	return (dev->intstatus) ? TRUE : FALSE;
}
#endif /* HWA_DPC_BUILD */
