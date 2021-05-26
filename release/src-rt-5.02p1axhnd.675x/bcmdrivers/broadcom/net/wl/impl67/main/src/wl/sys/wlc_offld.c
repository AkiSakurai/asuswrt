/*
 * Common (OS-independent) portion of Broadcom 802.11 Networking Device Driver
 * TX and RX status offload module
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
#include <wlc_hw_priv.h>
#if defined(WLC_OFFLOADS_M2M_INTR)
#include <pcicfg.h>
#include <wl_export.h>
#endif /* WLC_OFFLOADS_M2M_INTR */

/* M2M corerev check */
#define STATUS_OFFLOAD_SUPPORTED(rev)		(((rev) == 128) || /* 63178, 47622, 6750 */ \
						 ((rev) == 129) || /* 6710 */ \
						 ((rev) == 131) ||  /* 6715 */ \
						 ((rev) == 132))    /* 6756 */

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

#if defined(WLC_OFFLOADS_M2M_INTR)

#define OFFLD_SWITCHCORE(_sih_, _origidx_, _intr_val_)				\
({										\
	BCM_REFERENCE(_origidx_);						\
	BCM_REFERENCE(_intr_val_);						\
})

#define OFFLD_RESTORECORE(_sih_, _coreid_, _intr_val_)

#else /* ! WLC_OFFLOADS_M2M_INTR */

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

#endif /* ! WLC_OFFLOADS_M2M_INTR */

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

/** bits in m2m_bme_status_regs_t::ctl register */
#define BME_STS_CTL_STATUS_LAZYCOUNT_SHIFT	0 /**< No of Statuses in queue to trigger intr */
#define BME_STS_CTL_STATUS_LAZYCOUNT_NBITS	6
#define BME_STS_CTL_STATUS_LAZYCOUNT_MASK	BCM_MASK(BME_STS_CTL_STATUS_LAZYCOUNT)

#define BME_STS_CTL_STATUS_SIZE_SHIFT		22
#define BME_STS_CTL_STATUS_SIZE_NBITS		8 /**< Size of one status unit in bytes. */
#define BME_STS_CTL_STATUS_SIZE_MASK		BCM_MASK(BME_STS_CTL_STATUS_SIZE)

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

struct wlc_offload {
	wlc_info_t			*wlc;
	si_t				*sih;
	void				*osh;		/**< OS handle */
	volatile dma64regs_t		*xmt;
	volatile dma64regs_t		*rcv;
	volatile m2m_core_regs_t	*m2m_core_regs; /**< M2M Core Registers */
	struct offld_circ_buf_s		txsts;          /**< TxStatus offload support */
	struct offld_circ_buf_s		phyrxsts;       /**< PhyRxStatus offload support */
#if defined(WLC_OFFLOADS_M2M_INTR)
	char				irqname[32];	/**< M2M core interrupt name */
	uint				m2m_si_flag;	/**< M2m core bitmap in oobselouta30 reg */
	uint32				m2m_defintmask; /**< M2M default interrupts */
	uint32				m2m_intcontrol;	/**< M2M active interrupts */
	uint32				m2m_intstatus;	/**< M2M interrupt status */
#endif /* WLC_OFFLOADS_M2M_INTR */
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

	/* Take M2M core out of reset if it's not */
	if (!si_iscoreup(wlc_offl->sih))
		si_core_reset(wlc_offl->sih, 0, 0);

	ASSERT(STATUS_OFFLOAD_SUPPORTED(si_corerev(wlc_offl->sih)));

#if defined(WLC_OFFLOADS_M2M_INTR)

#if !defined(DONGLEBUILD)
#if defined(BCMHWA)
	/**
	 * Both HWA and M2M modules are using sixth 4KB region of PCIE BAR 0 space (0x78) to map
	 * corresponding core register space.
	 * Need two spare "mappable" 4K regions in BAR0 to enable both HWA & M2M interrupts
	 */
#error "Both WLC_OFFLOADS_M2M_INTR && BCMHWA features are enabled"
#endif /* BCMHWA */

	if (BUSTYPE(wlc_offl->sih->bustype) == PCI_BUS) {
		/* Set PCIE register PCIE2_BAR0_CORE2_WIN2 with M2M core register space. */
		si_backplane_pci_config_bar0_core2_win2(wlc_offl->sih,
			si_addrspace(wlc_offl->sih, CORE_SLAVE_PORT_0, CORE_BASE_ADDR_0));

		/* Adjust the M2M regs with offset for PCIE2_BAR0_CORE2_WIN2 */
		base = (volatile char *)(base + PCIE2_BAR0_CORE2_WIN2_OFFSET);

		/* Get M2M core bitmap in DMP wrapper's oob_select_a_in register (oobselouta30).
		 * Used to configure M2M core interrupt in PCIEIntMask (PCI_INT_MASK) register.
		 */
		wlc_offl->m2m_si_flag = si_flag(wlc_offl->sih);
	}
#endif /* ! DONGLEBUILD */

#endif /* WLC_OFFLOADS_M2M_INTR */

	wlc_offl->m2m_core_regs = (volatile m2m_core_regs_t*)base;

	wlc_offl->xmt = (volatile dma64regs_t *)(base + DMA_OFFSET_BME_BASE_CHANNEL3_XMT);
	wlc_offl->rcv = (volatile dma64regs_t *)(base + DMA_OFFSET_BME_BASE_CHANNEL3_RCV);

	OR_REG(wlc_offl->osh, &wlc_offl->xmt->control, BMEREG_CONTROL_EN);
	OR_REG(wlc_offl->osh, &wlc_offl->rcv->control, BMEREG_CONTROL_EN);

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
	struct offld_circ_buf_s *p;

	BCM_REFERENCE(p);

	if (wlc_offl == NULL)
		return;

	if (D11REV_LT(wlc_offl->wlc->pub->corerev, 130)) {
		goto done;
	}

	ASSERT(wlc_offl->m2m_core_regs != NULL);

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
	uint32 v32; /* used in REG read/write */

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

	v32 = R_REG(osh, &p->regs->ctl);

	/* Configure status unit size */
	v32 = BCM_CBF(v32, BME_STS_CTL_STATUS_SIZE);
	v32 |= BCM_SBF(p->element_sz, BME_STS_CTL_STATUS_SIZE);

#if defined(WLC_OFFLOADS_M2M_INTR)
	/* Interrupt on every status written to Status Queue. */
	v32 = BCM_CBF(v32, BME_STS_CTL_STATUS_LAZYCOUNT);
	v32 |= BCM_SBF(0x1, BME_STS_CTL_STATUS_LAZYCOUNT);
#endif /* WLC_OFFLOADS_M2M_INTR */

	W_REG(osh, &p->regs->ctl, v32);

	/* Enable sts function */
	OR_REG(osh, &p->regs->cfg, BME_STS_CFG_MOD_ENBL_MASK);
}

#ifdef LINUXSTA_PS
/**
 * (Re)initializes the BME hardware and software. Enables interrupts. Usually called on a 'wl up'.
 *
 * Prerequisite: firmware has progressed passed the BCMATTACH phase
 *
 * @param wlc_offl     Handle related to one of the BME dma channels in the M2M core.
 */
static void
wlc_offload_m2m_reinit(wlc_offload_t *wlc_offl)
{
	uint saved_core_idx;
	uint intr_val;
	const uint8 burst_len = 2; /* 2^(N+4) => 64 byte burst */
	uint32 control;

	si_switch_core(wlc_offl->sih, M2MDMA_CORE_ID, &saved_core_idx,
			&intr_val);

	/* Take M2M core out of reset if it's not */
	if (!si_iscoreup(wlc_offl->sih))
		si_core_reset(wlc_offl->sih, 0, 0);

#if defined(WLC_OFFLOADS_M2M_INTR)
#if !defined(DONGLEBUILD)
	if (BUSTYPE(wlc_offl->sih->bustype) == PCI_BUS) {
		/* Set PCIE register PCIE2_BAR0_CORE2_WIN2 with M2M core register space. */
		si_backplane_pci_config_bar0_core2_win2(wlc_offl->sih,
			si_addrspace(wlc_offl->sih, CORE_SLAVE_PORT_0, CORE_BASE_ADDR_0));
	}
#endif /* ! DONGLEBUILD */
#endif /* WLC_OFFLOADS_M2M_INTR */

	OR_REG(wlc_offl->osh, &wlc_offl->xmt->control, BMEREG_CONTROL_EN);
	OR_REG(wlc_offl->osh, &wlc_offl->rcv->control, BMEREG_CONTROL_EN);

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
}
#endif /* LINUXSTA_PS */
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
	uint32 d11_src_addr;
#if defined(WLC_OFFLOADS_M2M_INTR)
	uint32 m2m_defintmask = 0;
#endif /* WLC_OFFLOADS_M2M_INTR */

#if defined(LINUXSTA_PS)
	wlc_offload_m2m_reinit(wlc_offl);
#endif // endif

#if defined(WLC_OFFLOADS_RXSTS)
	/* setup circular buffer */
	wlc_offload_set_sts_array(wlc_offl, FALSE, va, STSBUF_MP_N_OBJ, sizeof(d11phyrxsts_t));
#endif /* WLC_OFFLOADS_RXSTS */

	OFFLD_SWITCHCORE(wlc_offl->sih, &saved_core_idx, &intr_val);

	/* disable offload module to reset asic function */
	AND_REG(wlc_offl->osh, &wlc_offl->xmt->control, ~BMEREG_CONTROL_EN);
	AND_REG(wlc_offl->osh, &wlc_offl->rcv->control, ~BMEREG_CONTROL_EN);

#ifdef WLC_OFFLOADS_TXSTS
	if (wlc_offl->txsts.regs != NULL) {
		d11_src_addr = SI_ENUM_BASE_BP(wlc_offl->sih);

		if (D11REV_IS(wlc_offl->wlc->pub->corerev, 132))
			d11_src_addr += D11_TXSTS_MEM_OFFSET_REV132;
		else
			d11_src_addr += D11_TXSTS_MEM_OFFSET;

		/* channel supports tx status offloading */
		wlc_offload_init_ch(wlc_offl, &wlc_offl->txsts, d11_src_addr);

#if defined(WLC_OFFLOADS_M2M_INTR)
		/* Enable M2M Interrupt on update of TxStatus Write Index */
		m2m_defintmask |= BCM_SBF(1, M2M_CORE_INTCONTROL_TXSWRINDUPD_INTMASK);
#endif /* WLC_OFFLOADS_M2M_INTR */
	}
#endif /* WLC_OFFLOADS_TXSTS */

#ifdef WLC_OFFLOADS_RXSTS
	if (wlc_offl->phyrxsts.regs != NULL) {
		d11_src_addr = SI_ENUM_BASE_BP(wlc_offl->sih);

		if (D11REV_IS(wlc_offl->wlc->pub->corerev, 132))
			d11_src_addr += D11_PHYRXSTS_MEM_OFFSET_REV132;
		else
			d11_src_addr += D11_PHYRXSTS_MEM_OFFSET;

		/* channel supports phyrx status offloading */
		wlc_offload_init_ch(wlc_offl, &wlc_offl->phyrxsts, d11_src_addr);

#if defined(WLC_OFFLOADS_M2M_INTR)
		/* Enable M2M Interrupt on update of PhyRxStatus Write Index */
		m2m_defintmask |= BCM_SBF(1, M2M_CORE_INTCONTROL_PHYRXSWRINDUPD_INTMASK);
#endif /* WLC_OFFLOADS_M2M_INTR */
	}
#endif /* WLC_OFFLOADS_RXSTS */

#if defined(WLC_OFFLOADS_M2M_INTR)
	wlc_offl->m2m_defintmask = m2m_defintmask;
	wlc_offload_intrson(wlc_offl);
#endif /* WLC_OFFLOADS_M2M_INTR */

	/* reenable offload module to resume asic function */
	OR_REG(wlc_offl->osh, &wlc_offl->xmt->control, BMEREG_CONTROL_EN);
	OR_REG(wlc_offl->osh, &wlc_offl->rcv->control, BMEREG_CONTROL_EN);

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

	AND_REG(wlc_offl->osh, &wlc_offl->xmt->control, ~BMEREG_CONTROL_EN);
	AND_REG(wlc_offl->osh, &wlc_offl->rcv->control, ~BMEREG_CONTROL_EN);

#if defined(WLC_OFFLOADS_M2M_INTR)
	/* Disable M2M interrupts */
	wlc_offl->m2m_defintmask = 0;
	wlc_offload_intrsoff(wlc_offl);
#endif /* WLC_OFFLOADS_M2M_INTR */

#ifdef WLC_OFFLOADS_TXSTS
	if (wlc_offl->txsts.regs != NULL) { /* channel supports phyrx/tx status offloading */
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

#if defined(WLC_OFFLOADS_M2M_INTR)
	bcm_bprintf(b, "\nM2M/BME interrupt:\n");
	bcm_bprintf(b, "m2m_defintmask:0x%08x\t m2m_intcontrol: 0x%08x\t m2m_intstatus: 0x%08x\n",
		wlc_offl->m2m_defintmask, wlc_offl->m2m_intcontrol, wlc_offl->m2m_intstatus);
#endif /* WLC_OFFLOADS_M2M_INTR */

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

/**
 * Check if new phyrxstatus circular buffer has arrived.
 */
bool
wlc_offload_rxsts_new_ready(wlc_offload_t *wlc_offl)
{
	uint saved_core_idx;
	uint intr_val;
	struct offld_circ_buf_s *pc = &wlc_offl->phyrxsts;

	OFFLD_SWITCHCORE(wlc_offl->sih, &saved_core_idx, &intr_val);

	/* saved current wr_idx for comparing */
	pc->wr_idx = R_REG(wlc_offl->osh, &pc->regs->wr_idx); /* register is updated by hardware */

	OFFLD_RESTORECORE(wlc_offl->sih, saved_core_idx, intr_val);

	/* New phyrxsts ready in MAC FIFO. */
	return (pc->rd_inuse != pc->wr_idx);
}
#endif /* WLC_OFFLOADS_RXSTS */

#if defined(WLC_OFFLOADS_M2M_INTR)

/**
 * ================================================================================================
 * Section: M2M/BME interrupt support
 *
 * Register settings to enable interrupts on TxStatus/PhyRxStatus WR index update.
 * - Set bit 30,31 in M2MCORE IntControl/Status registers to enable Tx/PhyRx Status interrupts.
 * - Set Lazycount (BME_STS_CTL_STATUS_LAZYCOUNT) to 1 in M2M TXS/PHYRXS control registers.
 * - Integrated CHIPS
 *	- Register an IRQ handler for IRQ's belonging to M2M (GET_2x2AX_M2M_IRQV()).
 * - External CHIPS over PCIE.
 *	- Interrupts from all cores are routed to the PCIE core. Set M2M coreidx in PCIEIntMask
 *	  register (PCI_INT_MASK).
 *	- M2M intstatus/mask would be in M2M Core requiring switching of PCIe BAR but interrupt
 *	  context switching is not permitted.
 *	  Using sixth 4KB of PCIE BAR 0 space (PCIE2_BAR0_CORE2_WIN2) to map M2M core address space.
 * - Set Interrupt affinity for M2M interrupts (Get IRQ numbers from /proc/interrupts file)
 *	echo <CPU core> > /proc/irq/<IRQ>/smp_affinity
 *
 * PhyRxStatus WR index update interrupt:
 *	Triggering WLAN Rx Processing (MI_DMAINT) on PhyRxStatus arrival (dummy or valid) as opposed
 *	to a Data/Mgmt packet arrival on RxFIFO0. By triggering RxProcessing on a PhyRxStatus
 *	arrival, we can leverage an entire AMPDUs worth of packets readily available in a DPC run.
 *	MAC (Ucode) will post a dummy PhyRxStatus in case the PhyStatus had not arrived from PHY
 *	to MAC, rather than simply dropping (not posting) with skipped seqnum.
 *
 *	All (Data) packets in an AMPDU would have the same transmitter (AMT Index) and candidate for
 *	CFP bulk upstream processing, allowing long packet trains to flow through the WLAN stack and
 *	and subsequently handoff to the bridging subsystem.
 *
 * TxStatus WR index update interrupt:
 *	Not used and not verified on any platform.
 * ================================================================================================
 */

/** Constructs and return name of M2M IRQ */
char *
wlc_offload_irqname(wlc_info_t *wlc, void *btparam)
{
	wlc_offload_t *wlc_offload = wlc->offl;
	BCM_REFERENCE(btparam);

#if defined(CONFIG_BCM_WLAN_DPDCTL)
	if (btparam != NULL) {
		/* bustype = PCI, even embedded 2x2AX devices have virtual pci underneath */
		snprintf(wlc_offload->irqname, sizeof(wlc_offload->irqname),
			"wlpcie:%s, wlan_%d_m2m", pci_name(btparam), wlc->pub->unit);
	} else
#endif /* CONFIG_BCM_WLAN_DPDCTL */
	{
		snprintf(wlc_offload->irqname, sizeof(wlc_offload->irqname), "wlan_%d_m2m",
			wlc->pub->unit);
	}

	return wlc_offload->irqname;
}

uint
wlc_offload_m2m_si_flag(wlc_offload_t *wlc_offload)
{
	return wlc_offload->m2m_si_flag;
}

/** Reads and clear status of M2M interrupts set in wlc_offload::m2m_intcontrol */
static uint32 BCMFASTPATH
wlc_offload_m2m_intstatus(wlc_offload_t *wlc_offload)
{
	volatile m2m_core_regs_t *m2m_core_regs = wlc_offload->m2m_core_regs;
	uint32 m2m_intstatus;

	/* Detect cardbus removed, in power down(suspend) and in reset */
	if (DEVICEREMOVED(wlc_offload->wlc))
		return -1;

	m2m_intstatus = R_REG(wlc_offload->osh, &m2m_core_regs->intstatus);
	m2m_intstatus &= wlc_offload->m2m_defintmask;

	if (m2m_intstatus) {
		/* Clear interrupt status */
		W_REG(wlc_offload->osh, &m2m_core_regs->intstatus, m2m_intstatus);

#if defined(WLC_OFFLOADS_RXSTS)
		if (BCM_GBF(m2m_intstatus, M2M_CORE_INTCONTROL_PHYRXSWRINDUPD_INTMASK)) {
			wlc_intr_process_rxfifo_interrupts(wlc_offload->wlc);
		}
#endif /* WLC_OFFLOADS_RXSTS */
	}

	return m2m_intstatus;
} /* wlc_offload_m2m_intstatus() */

/**
 * Function:	wlc_offload_isr()
 * Description:	IRQ handler for M2M interrupts.
 *		Interrupt status is copied to software variable wlc_offload:m2m2_intstatus and
 *		all M2M interrupts are disabled.
 *		A DPC will be scheduled and TxStatus/PhyRxStatus packages are extracted in DPC.
 */
bool BCMFASTPATH
wlc_offload_isr(wlc_info_t *wlc, bool *wantdpc)
{
	wlc_offload_t *wlc_offload;
	uint32 m2m_intstatus;

	WL_TRACE(("wl%d: %s: ENTER\n", wlc->pub->unit, __FUNCTION__));

	if (wl_powercycle_inprogress(wlc->wl)) {
		return FALSE;
	}

	wlc_offload = wlc->offl;

	if (wlc_offload->m2m_intcontrol == 0)
		return FALSE;

	/* Read and clear M2M intstatus register */
	m2m_intstatus = wlc_offload_m2m_intstatus(wlc_offload);

	if (m2m_intstatus == 0xffffffff) {
		WL_HEALTH_LOG(wlc, DEADCHIP_ERROR);
		WL_ERROR(("DEVICEREMOVED detected in the ISR code path.\n"));
		/* In rare cases, we may reach this condition as a race condition may occur
		 * between disabling interrupts and clearing the SW macintmask.
		 * Clear M2M int status as there is no valid interrupt for us.
		 */
		wlc_offload->m2m_intstatus = 0;
		/* assume this is our interrupt as before; note: want_dpc is FALSE */
		return TRUE;
	}

	/* It is not for us */
	if (m2m_intstatus == 0) {
		return FALSE;
	}

	/* Save interrupt status bits */
	wlc_offload->m2m_intstatus = m2m_intstatus;

	WL_INFORM(("wl%d: %s: M2M core instatus 0x%x \n", wlc->pub->unit,
		__FUNCTION__, m2m_intstatus));

	/* Turn off interrupts and schedule DPC */
	*wantdpc |= TRUE;

	return TRUE;
} /* wlc_offload_isr() */

void
wlc_offload_intrson(wlc_offload_t *wlc_offload)
{
	volatile m2m_core_regs_t *m2m_core_regs = wlc_offload->m2m_core_regs;

	WL_TRACE(("wl%d: %s: ENTER\n", wlc_offload->wlc->pub->unit, __FUNCTION__));

	wlc_offload->m2m_intcontrol = wlc_offload->m2m_defintmask;
	/* Enable M2M interrupts */
	W_REG(wlc_offload->osh, &m2m_core_regs->intcontrol, wlc_offload->m2m_intcontrol);
} /* wlc_offload_intrson() */

void
wlc_offload_intrsoff(wlc_offload_t *wlc_offload)
{
	volatile m2m_core_regs_t *m2m_core_regs = wlc_offload->m2m_core_regs;

	WL_TRACE(("wl%d: %s: ENTER\n", wlc_offload->wlc->pub->unit, __FUNCTION__));

	/* Disable M2M interrupts */
	W_REG(wlc_offload->osh, &m2m_core_regs->intcontrol, 0);
	wlc_offload->m2m_intcontrol = 0;
} /* wlc_offload_intrsoff() */

void
wlc_offload_intrsrestore(wlc_offload_t *wlc_offload, uint32 macintmask)
{
	WL_TRACE(("wl%d: %s: ENTER\n", wlc_offload->wlc->pub->unit, __FUNCTION__));

	if (macintmask) {
		wlc_offload_intrson(wlc_offload);
	} else {
		wlc_offload_intrsoff(wlc_offload);
	}

} /* wlc_offload_intrsrestore() */

/**
 * Called by DPC when the DPC is rescheduled, updates wlc_offload->m2m_intstatus
 *
 * Prerequisite:
 * - Caller should have acquired a spinlock against isr concurrency, or guarantee that interrupts
 *   have been disabled.
 */
void BCMFASTPATH
wlc_offload_intrsupd(wlc_offload_t *wlc_offload)
{
	uint32 m2m_intstatus;

	ASSERT(wlc_offload->m2m_intcontrol == 0);
	m2m_intstatus = wlc_offload_m2m_intstatus(wlc_offload);

	/* Device is removed */
	if (m2m_intstatus == 0xffffffff) {
		WL_HEALTH_LOG(wlc_offload->wlc, DEADCHIP_ERROR);
		return;
	}

	/* Update interrupt status in software */
	wlc_offload->m2m_intstatus |= m2m_intstatus;
} /* wlc_offload_intrsupd() */

void BCMFASTPATH
wlc_offload_process_m2m_intstatus(wlc_info_t *wlc)
{
	wlc_offload_t *wlc_offload = wlc->offl;
	wlc_hw_info_t *wlc_hw = wlc->hw;
	uint32 m2m_intstatus;

	WL_TRACE(("wl%d: %s: ENTER\n", wlc->pub->unit, __FUNCTION__));

	if (wlc_offload->m2m_intstatus == 0)
		return;

	m2m_intstatus = wlc_offload->m2m_intstatus;

	WL_INFORM(("wl%d: %s: wlc_offload->m2m_intstatus[%x]\n", wlc->pub->unit,
		__FUNCTION__, m2m_intstatus));

#if defined(WLC_OFFLOADS_TXSTS)
	if (BCM_GBF(m2m_intstatus, M2M_CORE_INTCONTROL_TXSWRINDUPD_INTMASK)) {
		/* M2M/BME DMA transferred TxStatus to memory */
		m2m_intstatus = BCM_CBF(m2m_intstatus, M2M_CORE_INTCONTROL_TXSWRINDUPD_INTMASK);
		wlc_hw->macintstatus |= MI_TFS;
	}
#endif /* WLC_OFFLOADS_TXSTS */

#if defined(WLC_OFFLOADS_RXSTS)
	if (BCM_GBF(m2m_intstatus, M2M_CORE_INTCONTROL_PHYRXSWRINDUPD_INTMASK)) {
		/* M2M/BME DMA transferred PhyRxStatus to memory */
		m2m_intstatus = BCM_CBF(m2m_intstatus, M2M_CORE_INTCONTROL_PHYRXSWRINDUPD_INTMASK);
		wlc_hw->macintstatus |= MI_DMAINT;
	}
#endif /* WLC_OFFLOADS_RXSTS */

	ASSERT(m2m_intstatus == 0);
	wlc_offload->m2m_intstatus = m2m_intstatus;

} /* wlc_offload_process_m2m_intstatus() */

#endif /* WLC_OFFLOADS_M2M_INTR */
