/*
 * Common (OS-independent) portion of Broadcom 802.11 Networking Device Driver
 * TX and RX status offload module
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
 * $Id: wlc_offld.c 999999 2017-01-04 16:02:31Z $
 */

/**
 * @file
 * @brief
 * Source file for WLC_OFFLD module. This file contains the functionality to initialize and run
 * the HW supported TX and RX status offloads in D11 corerevs >= 130.
 */

/**
 * XXX For more information, see:
 * Confluence:[M2MDMA+programming+guide+for+TX+status+and+PHY+RX+status]
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <wlioctl.h>
#include <bcmdevs.h>
#include <d11.h>
#include <wlc.h>
#include <wlc_pub.h>
#include <wlc_types.h>
#include <wl_dbg.h>
#include <sbhnddma.h>
#include <bcmutils.h>
#include <hndsoc.h>
#include <m2mdma_core.h>	/* for m2m_core_regs_t */
#include <wlc_types.h>
#include <wlc_offld.h>
#include <wlc_bmac.h> // for wlc_txs_pkg16_t
#include <wlc_dump.h>
#ifdef BCM_SECURE_DMA
#include <wlc_hw_priv.h>
#endif /* BCM_SECURE_DMA */

/* M2M corerev check */
#define STATUS_OFFLOAD_SUPPORTED(rev)		(((rev) == 128) || /* 63178, 47622, 6750 */ \
						 ((rev) == 129) || /* 6710 */ \
						  ((rev) == 131))  /* 6715 */

#define	PCI64ADDR_HIGH				0x80000000	/* address[63] */

#define DMA_OFFSET_BME_BASE_CHANNEL3_XMT	0x2C0
#define DMA_OFFSET_BME_BASE_CHANNEL3_RCV	0x2E0

#define M2M_OFFSET_BME_TXSTS_REGS		0x800
#define M2M_OFFSET_BME_PHYRXSTS_REGS		0x900

/**
 * The BME has the same registers as a DMA engine (using the same address space). So the
 * struct dma64regs_t from sbhnddma.h is reused. The bits of some of the fields are
 * defined here.
 */
#define BMEREG_CONTROL_EN			(1 << 0)
#define BMEREG_CONTROL_BURSTLEN_SHIFT		18
#define BMEREG_CONTROL_BURSTLEN_MASK		0x7

#define BMEREG_RCVCONTROL_WCPD_SHIFT		(1 << 28)

#define M2M_INTCONTROL_TXS_WRIND_UPD_MASK	(1<<30)
#define M2M_INTCONTROL_PHYRXS_WRIND_UPD_MASK	(1<<31)

#define M2M_INTCONTROL_MASK \
	(M2M_INTCONTROL_TXS_WRIND_UPD_MASK)

#define M2M_INTCTL_TXS (1<<30)

/* For txstatus memory/phyrxtatus memory inside the d11 core for d11 corerev >= 130,
 * where the d11 memory address offset for tx/phyrx status is defined in the below.
 * Need to add backplane address with this offset for the absolute physical address,
 * from the BME's perspective.
 */
#define D11_TXSTS_MEM_OFFSET			0x008d8000
#define D11_TXSTS_MEM_OFFSET_REV132		0x008f0000
#define D11_PHYRXSTS_MEM_OFFSET			0x008d9000
#define D11_PHYRXSTS_MEM_OFFSET_REV132		0x008f1000

#if defined(__ARM_ARCH_7A__)
#define datasynchronizationbarrier() __asm__ __volatile__ ("dsb")
#endif /* __ARM_ARCH_7A__ */

#define OFFLD_SWITCHCORE(_sih_, _origidx_, _intr_val_)				\
({										\
	*_origidx_ = 0;								\
	*_intr_val_ = 0;							\
	if (BUSTYPE(_sih_->bustype) == PCI_BUS) {				\
		si_switch_core(_sih_, M2MDMA_CORE_ID, _origidx_, _intr_val_);	\
	}									\
})

#define OFFLD_RESTORECORE(_sih_, _coreid_, _intr_val_)		\
	if (BUSTYPE(_sih_->bustype) == PCI_BUS)			\
		si_restore_core(_sih_, _coreid_, _intr_val_);

#define OFFLD_CIRC_BUF_NEXT_IDX(idx, pc)		MODINC_POW2((idx), pc->n_elements)
#define OFFLD_CIRC_BUF_INUSE(bitmap, idx)		isset((bitmap), (idx))
#define OFFLD_CIRC_BUF_SET_INUSE(bitmap, idx)	setbit((bitmap), (idx))
#define OFFLD_CIRC_BUF_CLR_INUSE(bitmap, idx)	clrbit((bitmap), (idx))

#define CBUF_N_TXSTS_PACKAGE_ENTRIES	1024 /**< 1024 elements in BME ring buffer */

#define D11_HWA_MACIF_CTL_RXPHYSTS_MAC_HWA_IF_EN (1 << 8)
#define D11_HWA_MACIF_CTL_RXPHYSTS_METHOD (1 << 10) /** 1 = through RXE_PHYSTS_DATAL/H */
#define D11_HWA_MACIF_CTL_RXPHYSTS_COUNT (1 << 12)

#define D11_HWA_MACIF_CTL_VAL (D11_HWA_MACIF_CTL_RXPHYSTS_MAC_HWA_IF_EN | \
			       D11_HWA_MACIF_CTL_RXPHYSTS_METHOD | \
			       D11_HWA_MACIF_CTL_RXPHYSTS_COUNT)

/** SB side: BME registers for phyrxsts / txstatus acceleration */
typedef struct m2m_bme_status_regs {
	uint32 cfg;		/* 0x0 */
	uint32 ctl;	/* 0x4 */
	uint32 sts;	/* 0x8 */
	uint32 debug;	/*  0xC */
	uint32 da_base_l;	/* 0x10 */
	uint32 da_base_h;	/* 0x14 */
	uint32 size;	/* 0x18 */
	uint32 wr_idx;	/* 0x1C */
	uint32 rd_idx;	/* 0x20 */
	uint32 dma_template;	/* 0x24 */
	uint32 sa_base_l;	/* 0x28 */
	uint32 sa_base_h;	/* 0x2C */
} m2m_bme_status_regs_t;

/** bits in m2m_bme_status_regs_t::cfg register */
#define BME_STS_CFG_MOD_ENBL_NBITS 1
#define BME_STS_CFG_MOD_ENBL_SHIFT 0
#define BME_STS_CFG_MOD_ENBL_MASK BCM_MASK(BME_STS_CFG_MOD_ENBL)
#define BME_STS_CFG_MOD_CH_NBITS 3
#define BME_STS_CFG_MOD_CH_SHIFT 3
#define BME_STS_CFG_MOD_CH_MASK BCM_MASK(BME_STS_CFG_MOD_CH)
#define BME_STS_CFG_EXTRARD_NBITS 1
#define BME_STS_CFG_EXTRARD_SHIFT 4
#define BME_STS_CFG_EXTRARD_MASK BCM_MASK(BME_STS_CFG_EXTRARD)
#define BME_STS_CFG_STOPCOND_NBITS 2
#define BME_STS_CFG_STOPCOND_SHIFT 2
#define BME_STS_CFG_STOPCOND_MASK  BCM_MASK(BME_STS_CFG_STOPCOND)

/** bits in m2m_bme_status_regs_t::dma_template register */
#define BME_DMATMPL_NOTPCIEDP_SHIFT   0 /**< NotPcie filled in DMA descr used for destination */
#define BME_DMATMPL_NOTPCIEDP_NBITS   1
#define BME_DMATMPL_NOTPCIEDP_MASK     BCM_MASK(BME_DMATMPL_NOTPCIEDP)

#define BME_DMATMPL_COHERENTDP_SHIFT  1 /**< Coherent filled in DMA descr used for destination */
#define BME_DMATMPL_COHERENTDP_NBITS  1
#define BME_DMATMPL_COHERENTDP_MASK   BCM_MASK(BME_DMATMPL_COHERENTDP)

#define BME_DMATMPL_ADDREXTDP_SHIFT   2 /**< AddrExt filled in DMA descr used for destination */
#define BME_DMATMPL_ADDREXTDP_NBITS   2
#define BME_DMATMPL_ADDREXTDP_MASK    BCM_MASK(BME_DMATMPL_ADDREXTDP)

#define BME_DMATMPL_NOTPCIESP_SHIFT   4 /**< NotPCIe filled in DMA descr used for source */
#define BME_DMATMPL_NOTPCIESP_NBITS   1
#define BME_DMATMPL_NOTPCIESP_MASK    BCM_MASK(BME_DMATMPL_NOTPCIESP)

#define BME_DMATMPL_COHERENTSP_SHIFT  5 /**< Coherent filled in DMA descr used for source */
#define BME_DMATMPL_COHERENTSP_NBITS  1
#define BME_DMATMPL_COHERENTSP_MASK   BCM_MASK(BME_DMATMPL_COHERENTSP)

#define BME_DMATMPL_PERDESCWC_SHIFT   7 /**< PerDescWC filled in DMA descr used for destination */
#define BME_DMATMPL_PERDESCWC_NBITS   1
#define BME_DMATMPL_PERDESCWC_MASK    BCM_MASK(BME_DMATMPL_PERDESCWC)

#define BME_STS_CTL_STATUS_SIZE_NBITS 8
#define BME_STS_CTL_STATUS_SIZE_SHIFT 22
#define BME_STS_CTL_STATUS_SIZE_MASK  BCM_MASK(BME_STS_CTL_STATUS_SIZE)

static void wlc_offload_set_sts_array(wlc_offload_t *wlc_offl, bool txsts, uint8 *va,
	int n_buf_els, int element_sz);

/** 63178 / 47622 support tx/phyrx status offloading in BME channel #1 */
struct offld_circ_buf_s {
	uint8	*va;        /**< virtual start address of ring buffer */
	volatile m2m_bme_status_regs_t *regs; /**< virtual address of txs or phyrxsts registers */
	dmaaddr_t pa;       /**< physical start address of ring buffer */
	uint	element_sz; /**< size of one circular buffer element in [bytes] */
	uint	n_elements;
	uint	buf_sz;     /**< size of the circular buffer in [bytes] */
	uint    rd_inuse;   /**< last idx that buffer posted to wl */
	uint    wr_idx;     /**< software stored write index from regiser */
	uint	rd_idx;     /**< last idx that buffer processed completed */
	uint8	*inproc_bitmap;	/**< in process sts buffer bitmap */
	bool	coherent;	/**< TRUE if system is coherent */
};

struct wlc_offload_s {
	wlc_info_t		*wlc;
	si_t			*sih;
	void			*osh;		    /**< os handle */
	volatile dma64regs_t	*xmt;
	volatile dma64regs_t	*rcv;
	volatile m2m_core_regs_t	*m2m_core_regs;   /**< m2m core registers */
	uint32		        m2m_intmask;        /**< cause reducing reg rds is more efficient */
	struct offld_circ_buf_s   txsts;              /**< txstatus offload support */
	struct offld_circ_buf_s   phyrxsts;           /**< phy rx status offload support */
};

wlc_offload_t *
BCMATTACHFN(wlc_offload_attach)(wlc_info_t *wlc)
{
	wlc_offload_t *wlc_offl;
	uint saved_core_idx;
	uint intr_val;
	volatile char *base;
	const uint8 burst_len = 2; /* 2^(N+4) => 64 byte burst */
	uint32 control;

	/* Allocate private info structure */
	if ((wlc_offl = MALLOCZ(wlc->osh, sizeof(*wlc_offl))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return (NULL);
	}
	wlc_offl->wlc = wlc;
	wlc_offl->sih = wlc->pub->sih;
	wlc_offl->osh = wlc->osh;

	if (D11REV_LT(wlc->pub->corerev, 130)) {
		WL_INFORM(("wl%d: %s: Not enabling TX/RX status offload support\n",
			wlc->pub->unit, __FUNCTION__));
		return wlc_offl;
	}

	base = (volatile char *)si_switch_core(wlc_offl->sih, M2MDMA_CORE_ID, &saved_core_idx,
		&intr_val);
	ASSERT(base);
	wlc_offl->m2m_core_regs = (volatile m2m_core_regs_t*)base;

	/* Take M2M core out of reset if it's not */
	if (!si_iscoreup(wlc_offl->sih))
		si_core_reset(wlc_offl->sih, 0, 0);

	wlc_offl->xmt = (volatile dma64regs_t *)(base + DMA_OFFSET_BME_BASE_CHANNEL3_XMT);
	wlc_offl->rcv = (volatile dma64regs_t *)(base + DMA_OFFSET_BME_BASE_CHANNEL3_RCV);

	OR_REG(wlc_offl->osh, &wlc_offl->xmt->control, BMEREG_CONTROL_EN);
	OR_REG(wlc_offl->osh, &wlc_offl->rcv->control, BMEREG_CONTROL_EN);

	ASSERT(STATUS_OFFLOAD_SUPPORTED(si_corerev(wlc_offl->sih)));
#ifdef WLC_OFFLOADS_TXSTS
	wlc_offl->txsts.regs =
		(volatile m2m_bme_status_regs_t *)(base + M2M_OFFSET_BME_TXSTS_REGS);
#endif /* WLC_OFFLOADS_TXSTS */
#ifdef WLC_OFFLOADS_RXSTS
	wlc_offl->phyrxsts.regs =
		(volatile m2m_bme_status_regs_t *)(base + M2M_OFFSET_BME_PHYRXSTS_REGS);
#endif /* WLC_OFFLOADS_RXSTS */

#ifdef WLC_OFFLOADS_TXSTS
	wlc_offload_set_sts_array(wlc_offl, TRUE, NULL, CBUF_N_TXSTS_PACKAGE_ENTRIES,
		sizeof(wlc_txs_pkg16_t));
#endif /* WLC_OFFLOADS_TXSTS */

	/* Configure xmit burstlen */
	control = R_REG(wlc_offl->osh, &wlc_offl->xmt->control);
	control &= ~(BMEREG_CONTROL_BURSTLEN_MASK << BMEREG_CONTROL_BURSTLEN_SHIFT);
	control |= (burst_len << BMEREG_CONTROL_BURSTLEN_SHIFT);
	W_REG(wlc_offl->osh, &wlc_offl->xmt->control, control);
	/* Configure receive burstlen */
	control = R_REG(wlc_offl->osh, &wlc_offl->rcv->control);
	control &= ~(BMEREG_CONTROL_BURSTLEN_MASK << BMEREG_CONTROL_BURSTLEN_SHIFT);
	control |= (burst_len << BMEREG_CONTROL_BURSTLEN_SHIFT);
	W_REG(wlc_offl->osh, &wlc_offl->rcv->control, control);

	si_restore_core(wlc_offl->sih, saved_core_idx, intr_val);

#if defined(BCMDBG)
	wlc_dump_add_fns(wlc->pub, "offld", wlc_offload_wl_dump, NULL, wlc_offl);
#endif // endif

	return wlc_offl;
} /* wlc_offload_attach */

void
BCMATTACHFN(wlc_offload_detach)(wlc_offload_t *wlc_offl)
{
	uint saved_core_idx;
	uint intr_val;
	struct offld_circ_buf_s *p;

	BCM_REFERENCE(p);

	if ((wlc_offl == NULL) || D11REV_LT(wlc_offl->wlc->pub->corerev, 130)) {
		goto done;
	}
	ASSERT(wlc_offl->m2m_core_regs != NULL);

	OFFLD_SWITCHCORE(wlc_offl->sih, &saved_core_idx, &intr_val);

	AND_REG(wlc_offl->osh, &wlc_offl->xmt->control, ~BMEREG_CONTROL_EN);
	AND_REG(wlc_offl->osh, &wlc_offl->rcv->control, ~BMEREG_CONTROL_EN);

	OFFLD_RESTORECORE(wlc_offl->sih, saved_core_idx, intr_val);

#if defined(WLC_OFFLOADS_TXSTS)
	p = &wlc_offl->txsts;
	if (p->buf_sz > 0) {
		DMA_UNMAP(wlc_offl->osh, p->pa, p->buf_sz, DMA_RX, NULL, NULL);
		if (p->inproc_bitmap) {
			MFREE(wlc_offl->osh, p->inproc_bitmap, CEIL(p->n_elements, NBBY));
		}
		p->inproc_bitmap = NULL;
	}
#endif /* WLC_OFFLOADS_TXSTS */
#if defined(WLC_OFFLOADS_RXSTS)
	p = &wlc_offl->phyrxsts;
	if (p->buf_sz > 0) {
		DMA_UNMAP(wlc_offl->osh, p->pa, p->buf_sz, DMA_RX, NULL, NULL);
		if (p->inproc_bitmap) {
			MFREE(wlc_offl->osh, p->inproc_bitmap, CEIL(p->n_elements, NBBY));
		}
		p->inproc_bitmap = NULL;
	}
#endif /* WLC_OFFLOADS_RXSTS */

done:
	MFREE(wlc_offl->osh, (void *)wlc_offl, sizeof(*wlc_offl));
}

/**
 * (Re)initializes a specific BME tx/phyrx status offload channel. Usually called on a 'wl up'.
 * @param[in] d11_src_addr   Backplane address pointing to a d11 core internal memory
 */
static void
wlc_offload_init_ch(wlc_offload_t *wlc_offl, struct offld_circ_buf_s *p, uint32 d11_src_addr)
{
	osl_t *osh = (osl_t *)wlc_offl->osh;

	/* Disable STS config to reset logic */
	AND_REG(osh, &p->regs->cfg, ~BME_STS_CFG_MOD_ENBL_MASK);

	/* For source address setting */
	W_REG(osh, &p->regs->sa_base_l, d11_src_addr);
	W_REG(osh, &p->regs->sa_base_h, 0);

	ASSERT(sizeof(PHYSADDRLO(p->pa)) > sizeof(uint32) ?
			(PHYSADDRLO(p->pa) >> 32) == 0 : TRUE);

	if ((BUSTYPE(wlc_offl->sih->bustype) == PCI_BUS) &&
		(BCM6710_CHIP(wlc_offl->sih->chip) || BCM6715_CHIP(wlc_offl->sih->chip))) {
		uint32 dma_template = BME_DMATMPL_NOTPCIESP_MASK | BME_DMATMPL_PERDESCWC_MASK;

		/* XXX: For destination address setting, the da_base_l/h is from BME's perspective
		 * in NIC-400; remap the host memory address to PCIE large memory window
		 */

		W_REG(osh, &p->regs->da_base_l, (uint32)PHYSADDRLO(p->pa));
		W_REG(osh, &p->regs->da_base_h, PHYSADDRHI(p->pa) | PCI64ADDR_HIGH);

		/* XXX: Note PCIE large memory window is used for the mapping of host memory,
		 * PCIE bus address bit[63] is from dma_template register bit[2]
		 */
		if (PHYSADDRHI(p->pa) & PCI64ADDR_HIGH) {
			dma_template |= (1 << BME_DMATMPL_ADDREXTDP_SHIFT);
		}
		W_REG(osh, &p->regs->dma_template, dma_template);

		/* XXX: Enable WCPD bit in RcvCtrl register as WCPD per descriptor is set
		 * in dma_template register, as suggested by HW designer, for consistency
		 */
		OR_REG(wlc_offl->osh, &wlc_offl->rcv->control, BMEREG_RCVCONTROL_WCPD_SHIFT);
	} else {
		/* For destination address setting */
		W_REG(osh, &p->regs->da_base_l, (uint32)PHYSADDRLO(p->pa));
		W_REG(osh, &p->regs->da_base_h, PHYSADDRHI(p->pa));

		/* source is is AXI backplane (so 'not PCIe'),
		 * destination is coherent 63178 ARM mem
		 */
		W_REG(osh, &p->regs->dma_template,
			BME_DMATMPL_COHERENTDP_MASK | BME_DMATMPL_NOTPCIESP_MASK);

		/* Marked as coherent support device */
		p->coherent = TRUE;
	}

	W_REG(osh, &p->regs->size, p->n_elements);

	/* BME buffer elements between rd_idx, and rd_inuse:
	 * with STSBUF_DATA(sts) attached to match PHY seq number with Rx frame
	 */
	p->rd_idx = 0;
	W_REG(osh, &p->regs->rd_idx, p->rd_idx);

	/* BME buffer elements between rd_inuse and wr_idx:
	 * PHYRxStatus has been posted by H/W, not attach to STSBUF_DATA(sts)
	 */
	p->rd_inuse = 0;

	/* BME buffer elements between wr_idx and rd_idx:
	 * available buffer elements for H/W to post PHYRxStatus
	 */
	p->wr_idx = 0;
	W_REG(osh, &p->regs->wr_idx, p->wr_idx);

	if (p->inproc_bitmap != NULL) {
		bzero(p->inproc_bitmap, CEIL(p->n_elements, NBBY));
	}

	AND_REG(osh, &p->regs->ctl, ~BME_STS_CTL_STATUS_SIZE_MASK);
	OR_REG(osh, &p->regs->ctl, p->element_sz << BME_STS_CTL_STATUS_SIZE_SHIFT);

	/* Enable sts function */
	OR_REG(osh, &p->regs->cfg, BME_STS_CFG_MOD_ENBL_MASK);
}

/**
 * (Re)initializes the BME hardware and software. Enables interrupts. Usually called on a 'wl up'.
 *
 * Prerequisite: firmware has progressed passed the BCMATTACH phase
 *
 * @param wlc_offl     Handle related to one of the BME dma channels in the M2M core.
 */
void
wlc_offload_init(wlc_offload_t *wlc_offl, uint8 *va)
{
	uint saved_core_idx;
	uint intr_val;

#if defined(WLC_OFFLOADS_RXSTS)
	/* setup circular buffer */
	wlc_offload_set_sts_array(wlc_offl, FALSE, va, STSBUF_MP_N_OBJ, sizeof(d11phystshdr_t));
#endif /* WLC_OFFLOADS_RXSTS */

	OFFLD_SWITCHCORE(wlc_offl->sih, &saved_core_idx, &intr_val);
#ifdef WLC_OFFLOADS_TXSTS
	if (wlc_offl->txsts.regs != NULL) {
		/* channel supports tx status offloading */
		wlc_offload_init_ch(wlc_offl, &wlc_offl->txsts,
		 SI_ENUM_BASE_BP(wlc_offl->sih) +
		  (D11REV_IS(wlc_offl->wlc->pub->corerev, 132) ?
		   D11_TXSTS_MEM_OFFSET_REV132 : D11_TXSTS_MEM_OFFSET));
		OR_REG(wlc_offl->osh, &wlc_offl->m2m_core_regs->intcontrol, M2M_INTCONTROL_MASK);
	}
#endif /* WLC_OFFLOADS_TXSTS */
#ifdef WLC_OFFLOADS_RXSTS
	if (wlc_offl->phyrxsts.regs != NULL) {
		/* channel supports phyrx status offloading */
		wlc_offload_init_ch(wlc_offl, &wlc_offl->phyrxsts,
		 SI_ENUM_BASE_BP(wlc_offl->sih) +
		  (D11REV_IS(wlc_offl->wlc->pub->corerev, 132) ?
		   D11_PHYRXSTS_MEM_OFFSET_REV132 : D11_PHYRXSTS_MEM_OFFSET));
	}
#endif /* WLC_OFFLOADS_RXSTS */
	OFFLD_RESTORECORE(wlc_offl->sih, saved_core_idx, intr_val);

	W_REG(wlc_offl->osh, D11_HWA_MACIF_CTL(wlc_offl->wlc), (uint16)D11_HWA_MACIF_CTL_VAL);
}

/**
 * Disables a specific BME channel. Usually called on a 'wl down'.
 */
static void
wlc_offload_deinit_ch(osl_t *osh, struct offld_circ_buf_s *p)
{
	AND_REG(osh, &p->regs->cfg, ~BME_STS_CFG_MOD_ENBL_MASK);
}

/**
 * Disables interrupts. Usually called on a 'wl down'.
 *
 * Prerequisite: firmware has progressed passed the BCMATTACH phase
 */
void
wlc_offload_deinit(wlc_offload_t *wlc_offl)
{
	uint saved_core_idx;
	uint intr_val;

	OFFLD_SWITCHCORE(wlc_offl->sih, &saved_core_idx, &intr_val);
#ifdef WLC_OFFLOADS_TXSTS
	if (wlc_offl->txsts.regs != NULL) { /* channel supports phyrx/tx status offloading */
		AND_REG(wlc_offl->osh, &wlc_offl->m2m_core_regs->intcontrol, ~M2M_INTCONTROL_MASK);
		wlc_offload_deinit_ch(wlc_offl->osh, &wlc_offl->txsts);
	}
#endif /* WLC_OFFLOADS_TXSTS */
#ifdef WLC_OFFLOADS_RXSTS
	if (wlc_offl->phyrxsts.regs != NULL) {
		wlc_offload_deinit_ch(wlc_offl->osh, &wlc_offl->phyrxsts);
	}
#endif /* WLC_OFFLOADS_RXSTS */
	OFFLD_RESTORECORE(wlc_offl->sih, saved_core_idx, intr_val);
}

#ifdef WLC_OFFLOADS_TXSTS
/**
 * Returns a pointer to one rx or one txstatus in the circular buffer that was written by the d11
 * core but not yet consumed by software.
 *
 * @param[in] caller_buf   Caller allocated buffer, large enough to contain phyrx or tx status
 * @param[in] want_txs     TRUE for tx status, FALSE for phyrx status
 * @return                 Pointer to caller_buf if succeeded
 */
static void * BCMFASTPATH
wlc_offload_get_status(wlc_offload_t *wlc_offl, void *caller_buf, bool want_txs)
{
	uint saved_core_idx;
	uint intr_val;
	uint wr_idx;            /**< hardware updates write index */
	void *ret = NULL;       /**< return value */
	struct offld_circ_buf_s *pc = want_txs ? &wlc_offl->txsts : &wlc_offl->phyrxsts;

	ASSERT(pc->regs != NULL); /* BME channel does not support txs/phyrx status offloading */
	OFFLD_SWITCHCORE(wlc_offl->sih, &saved_core_idx, &intr_val);

	wr_idx = R_REG(wlc_offl->osh, &pc->regs->wr_idx); /* register is updated by hardware */
	if (wr_idx != pc->rd_idx) {
		memcpy(caller_buf, pc->va + pc->rd_idx * pc->element_sz, pc->element_sz);
		ret = caller_buf;
		if (++pc->rd_idx == pc->n_elements) {
			pc->rd_idx = 0;
		}
		/* inform hw that it may advance */
		W_REG(wlc_offl->osh, &pc->regs->rd_idx, pc->rd_idx);
	}

	OFFLD_RESTORECORE(wlc_offl->sih, saved_core_idx, intr_val);

	return ret;
} /* wlc_offload_get_status */

/**
 * Returns a pointer to one txstatus in the circular buffer that was written by the d11 core but
 * not yet consumed by software.
 *
 * @param[in] caller_buf   Caller allocated buffer, large enough to contain phyrx or tx status
 * @return                 Pointer to caller_buf if succeeded
 */
void * BCMFASTPATH
wlc_offload_get_txstatus(wlc_offload_t *wlc_offl, void *caller_buf)
{
	return wlc_offload_get_status(wlc_offl, caller_buf, TRUE);
} /* wlc_offload_get_txstatus */

/**
 * Read and optionally clear m2m intstatus register.
 * This routine must not be preempted by other WLAN code.
 *
 * @param[in] in_isr
 * @Return:  0xFFFFFFFF if DEVICEREMOVED, 0 if the interrupt is not for us, or we are in some
 *           special case, device interrupt status bits otherwise.
 */
uint32 BCMFASTPATH
wlc_offload_m2m_intstatus(wlc_offload_t *wlc_offl, bool in_isr)
{
	volatile m2m_core_regs_t *regs = wlc_offl->m2m_core_regs;
	uint32 active_irqs;

#ifdef LINUX_VERSION_CODE
	ASSERT((in_irq() && irqs_disabled()) || in_softirq());
#endif /* LINUX_VERSION_CODE */

	active_irqs = R_REG(wlc_offl->osh, &regs->intstatus);

	if (in_isr) {
		active_irqs &= wlc_offl->m2m_intmask;
		if (active_irqs == 0) /* it is not for us */
			return 0;

		if (active_irqs == 0xffffffff) {
			return 0xffffffff;
		}

		// de-assert interrupt so isr will not be re-invoked, dpc will be scheduled
		wlc_offload_set_txs_intmask(wlc_offl, FALSE);
	} else { /* called from DPC */
		/* clears irqs */
		W_REG(wlc_offl->osh, &regs->intstatus, active_irqs);
	}

	return active_irqs;
} /* wlc_offload_m2m_intstatus */

void BCMFASTPATH
wlc_offload_set_txs_intmask(wlc_offload_t *wlc_offl, bool enable)
{
	volatile m2m_core_regs_t *regs = wlc_offl->m2m_core_regs;

	wlc_offl->m2m_intmask = enable ? M2M_INTCTL_TXS : 0;
	W_REG(wlc_offl->osh, &regs->intcontrol, wlc_offl->m2m_intmask);
}

#ifdef BCMQT
bool BCMFASTPATH
wlc_offload_is_m2m_irq(wlc_offload_t *wlc_offl)
{
	volatile m2m_core_regs_t *regs = wlc_offl->m2m_core_regs;
	uint32 intstatus = R_REG(wlc_offl->osh, &regs->intstatus);

	return ((wlc_offl->m2m_intmask & intstatus) != 0);
}
#endif /* BCMQT */

#endif /* WLC_OFFLOADS_TXSTS */

#if defined(BCMDBG)
static void
wlc_offload_dump_sts(struct offld_circ_buf_s *p, struct bcmstrbuf *b, char *funcstr)
{
	int i;

	/* dump hw register */
	bcm_bprintf(b, "%s\n\tRegister Settings\n", funcstr);
	bcm_bprintf(b, "\t\tcfg: 0x%08x\tctl: 0x%08x\tsts: 0x%08x\tdebug: 0x%08x\n",
		p->regs->cfg, p->regs->ctl, p->regs->sts, p->regs->debug);
	bcm_bprintf(b, "\t\tqueue size: %d\twr_idx: %d\trd_idx: %d\tdma_tmpl: 0x%08x\n",
		p->regs->size, p->regs->wr_idx, p->regs->rd_idx, p->regs->dma_template);
	bcm_bprintf(b, "\t\tda: 0x%08x_%08x\tsa: 0x%08x_%08x\n",
		p->regs->da_base_h, p->regs->da_base_l, p->regs->sa_base_h, p->regs->sa_base_l);

	/* dump sw control blocks */
	bcm_bprintf(b, "\tSW Controls Block\n", funcstr);
	bcm_bprintf(b, "\t\telement size: %d\tcount: %d\ttotal buf size: %d\n",
		p->element_sz, p->n_elements, p->buf_sz);
	bcm_bprintf(b, "\t\twr_idx: %d\trd_inuse: %d\trd_idx: %d\n",
		p->wr_idx, p->rd_inuse, p->rd_idx);

	bcm_bprintf(b, "\t\tcircular buffer bitmap: 0x%02x", p->inproc_bitmap[0]);
	for (i = 1; i < CEIL(p->n_elements, NBBY); i++) {
		bcm_bprintf(b, "_%02x", p->inproc_bitmap[i]);
	}
	bcm_bprintf(b, "\n");
}

int
wlc_offload_wl_dump(void *ctx, struct bcmstrbuf *b)
{
	wlc_offload_t *wlc_offl = (wlc_offload_t *)ctx;
	struct offld_circ_buf_s *p;
	uint saved_core_idx;
	uint intr_val;

	OFFLD_SWITCHCORE(wlc_offl->sih, &saved_core_idx, &intr_val);

#ifdef WLC_OFFLOADS_TXSTS
	p = &wlc_offl->txsts;
	if (p->va != NULL && p->buf_sz != 0) {
		char *txstr = "wlc_offload TxSts Circular Buffer Control";
		wlc_offload_dump_sts(p, b, txstr);
	}
#endif /* WLC_OFFLOADS_TXSTS */
#ifdef WLC_OFFLOADS_RXSTS
	p = &wlc_offl->phyrxsts;
	if (p->va != NULL && p->buf_sz != 0) {
		char *rxstr = "wlc_offload PhyRxSts Circular Buffer Control";
		wlc_offload_dump_sts(p, b, rxstr);
	}
#endif /* WLC_OFFLOADS_RXSTS */

	OFFLD_RESTORECORE(wlc_offl->sih, saved_core_idx, intr_val);

	return BCME_OK;
}
#endif // endif

/**
 * Setup buffer pointer of txsts or phyrxsts to bme offload channel. Buffer allocated in wl module
 * and assigned to bme channel for retrieving status. With this, wl module can use the status
 * without need to copy it out from bme buffer
 *
 * @param[in]	txsts		determine which ststus buffer to set. True is txsts, False is rxsts
 *
 * @param[in]	n_buf_els	Number of elements in one BME circular buffer.
 *
 * @param[in]	element_sz	size of each element.
 */
static void
wlc_offload_set_sts_array(wlc_offload_t *wlc_offl, bool txsts, uint8 *va, int n_buf_els,
	int element_sz)
{
	struct offld_circ_buf_s *p;
#ifdef BCM_SECURE_DMA
	wlc_hw_info_t	*hw = wlc_offl->wlc->hw;
#endif /* BCM_SECURE_DMA */
	ASSERT(wlc_offl);
	ASSERT(va);

	if (txsts)
		p = &wlc_offl->txsts;
	else
		p = &wlc_offl->phyrxsts;

	p->element_sz = element_sz;
	p->n_elements = n_buf_els;
	p->buf_sz = element_sz * n_buf_els;
	p->va = va;
	/* map circular buffer for device use only */
#if !defined(BCM_SECURE_DMA)
	p->pa = DMA_MAP(wlc_offl->osh, p->va, p->buf_sz, DMA_RX, NULL, NULL);
#else
	p->pa = hw->pap;
#endif // endif
	if (p->inproc_bitmap == NULL) {
		p->inproc_bitmap = MALLOCZ(wlc_offl->osh, CEIL(p->n_elements, NBBY));
	}
	ASSERT(p->inproc_bitmap);
}

#ifdef WLC_OFFLOADS_RXSTS
void * BCMFASTPATH
wlc_offload_get_rxstatus(wlc_offload_t *wlc_offl, int16 *bmebufidx)
{
	uint saved_core_idx;
	uint intr_val;
	void *ret_addr = NULL;
	struct offld_circ_buf_s *pc = &wlc_offl->phyrxsts;

	OFFLD_SWITCHCORE(wlc_offl->sih, &saved_core_idx, &intr_val);

	/* saved current wr_idx for comparing */
	pc->wr_idx = R_REG(wlc_offl->osh, &pc->regs->wr_idx); /* register is updated by hardware */

	OFFLD_RESTORECORE(wlc_offl->sih, saved_core_idx, intr_val);

	/* New phyrxsts ready in MAC FIFO. Retrieve one. */
	if (pc->rd_inuse != pc->wr_idx) {
		/* Sync entry if no DMA coherent supported. */
		if (!pc->coherent) {
			dmaaddr_t sync_addr;
			uint offset = pc->rd_inuse * pc->element_sz;

			PHYSADDRHISET(sync_addr, PHYSADDRHI(pc->pa));
			PHYSADDRLOSET(sync_addr, PHYSADDRLO(pc->pa) + offset);

			/* Invalid cache for one entry size */
			DMA_SYNC(wlc_offl->osh, sync_addr, pc->element_sz, DMA_RX);
		}

		/* save circular buffer index */
		*bmebufidx = pc->rd_inuse;
		ret_addr = pc->va + (pc->rd_inuse * pc->element_sz);

		/* marked the buffer idx is in use */
		OFFLD_CIRC_BUF_SET_INUSE(pc->inproc_bitmap, pc->rd_inuse);

		/* advance rd_inuse to next one */
		pc->rd_inuse = OFFLD_CIRC_BUF_NEXT_IDX(pc->rd_inuse, pc);
	}

	return ret_addr;
}

/**
 * Notify BME channel rxststus offload function to update rd_idx to hardware register. With this
 * bme can keep every posted Phy RxStatus content was not overridden before it processed.
 *
 * @param[in]	bmebufidx	Phy RxStatus circular buffer index to be freed
 */

void BCMFASTPATH
wlc_offload_release_rxstatus(void *ctx, int16 bmebufidx)
{
	uint saved_core_idx;
	uint intr_val;
	wlc_offload_t *wlc_offl = (wlc_offload_t *)ctx;
	struct offld_circ_buf_s *pc = &wlc_offl->phyrxsts;
	uint16 old_rd_idx = pc->rd_idx;

	OFFLD_CIRC_BUF_CLR_INUSE(pc->inproc_bitmap, bmebufidx);

	/* Advance rd_idx until meet an index which is in use */
	while (!OFFLD_CIRC_BUF_INUSE(pc->inproc_bitmap, pc->rd_idx) && pc->rd_idx != pc->rd_inuse) {
		pc->rd_idx = OFFLD_CIRC_BUF_NEXT_IDX(pc->rd_idx, pc);
	}

	if (old_rd_idx != pc->rd_idx) {
		OFFLD_SWITCHCORE(wlc_offl->sih, &saved_core_idx, &intr_val);

		/* inform hw that it may advance */
		W_REG(wlc_offl->osh, &pc->regs->rd_idx, pc->rd_idx);

		OFFLD_RESTORECORE(wlc_offl->sih, saved_core_idx, intr_val);
	}
}
#endif /* WLC_OFFLOADS_RXSTS */
