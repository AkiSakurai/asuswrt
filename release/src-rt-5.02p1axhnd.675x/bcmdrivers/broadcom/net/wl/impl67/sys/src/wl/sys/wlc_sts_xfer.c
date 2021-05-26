/*
 * Common (OS-independent) portion of Broadcom 802.11 Networking Device Driver
 * Tx andi PhyRx status transfer module
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
 * $Id:$
 */

/**
 * @file
 * @brief
 * Source file for STS_XFER module. This file contains the functionality to initialize and run
 * the HW supported TX and RX status transfers in D11 corerevs >= 129.
 */

/**
 * XXX For more information, see:
 * Confluence:[M2MDMA+programming+guide+for+TX+status+and+PHY+RX+status]
 * Confluence:[RxStatus]
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmdevs.h>
#include <bcm_ring.h>
#include <wl_export.h>
#include <d11.h>
#include <hndd11.h>
#include <hnddma.h>
#include <m2mdma_core.h>	/* for m2m_core_regs_t, m2m_eng_regs_t */
#include <wlc.h>
#include <wlc_types.h>
#include <pcicfg.h>
#include <wlc_sts_xfer.h>
#include <wlc_rx.h>
#include <wlc_pub.h>
#include <wlc_hw_priv.h>
#include <wlc_bmac.h>
#if defined(BCMDBG)
#include <wlc_dump.h>
#endif // endif

#if defined(DONGLEBUILD)
#error "STS_XFER is not supported in Full Dongle"
#endif /* DONGLEBUILD */

/*
 * ------------------------------------------------------------------------------------------------
 *  STS_XFER module provides the library of API's to transfer the PhyRx Status
 *  for MAC rev >= 129 and Tx Status for MAC rev >= 130.
 *  Details are provided in and Tx and PhyRx Status sections.
 * ------------------------------------------------------------------------------------------------
 */

/**
 * ------------------------------------------------------------------------------------------------
 * M2M Status Transfer Register specification
 *
 * For MAC revs >= 130, M2M DMA (BME) channel #3 is re-purposed by MAC to transfer Tx and PhyRx
 * Status from MAC.
 *
 * USE register sepcifications from sbhnddma.h and m2mdma_core.h.
 * Only new M2M Status registers or re-purposed" dma64regs may be listed here, using
 * naming convention from sbhnddma.h and m2mdma_core.h.
 *
 * m2m_status_eng_regs_t
 * ------------------------------------------------------------------------------------------------
 */

/** All offsets are with respect to m2m_core_regs_t */
#define M2M_STATUS_ENG_TXS_REGS_OFFSET		0x800
#define M2M_STATUS_ENG_PHYRXS_REGS_OFFSET	0x900

/* M2M Engine channel #3 is used to tranfer Tx & PhyRx Status */
#define M2M_STATUS_ENG_M2M_CHANNEL		3

/** m2m_status_eng_regs_t::cfg register */
#define M2M_STATUS_ENG_CFG_MODULEEN_SHIFT	0
#define M2M_STATUS_ENG_CFG_MODULEEN_NBITS	1
#define M2M_STATUS_ENG_CFG_MODULEEN_MASK	BCM_MASK(M2M_STATUS_ENG_CFG_MODULEEN)

/** m2m_status_eng_regs_t::ctl register */
/**< No of Statuses in queue to trigger intr */
#define M2M_STATUS_ENG_CTL_LAZYCOUNT_SHIFT	0
#define M2M_STATUS_ENG_CTL_LAZYCOUNT_NBITS	6
#define M2M_STATUS_ENG_CTL_LAZYCOUNT_MASK	BCM_MASK(M2M_STATUS_ENG_CTL_LAZYCOUNT)

/**< Size of one status unit in bytes. */
#define M2M_STATUS_ENG_CTL_MACSTATUSSIZE_SHIFT	22
#define M2M_STATUS_ENG_CTL_MACSTATUSSIZE_NBITS	8
#define M2M_STATUS_ENG_CTL_MACSTATUSSIZE_MASK	BCM_MASK(M2M_STATUS_ENG_CTL_MACSTATUSSIZE)

/** m2m_status_eng_regs_t::size register */
#define M2M_STATUS_ENG_SIZE_QUEUE_SIZE_SHIFT	0
#define M2M_STATUS_ENG_SIZE_QUEUE_SIZE_NBITS	16
#define M2M_STATUS_ENG_SIZE_QUEUE_SIZE_MASK	BCM_MASK(M2M_STATUS_ENG_SIZE_QUEUE_SIZE)

/** m2m_status_eng_regs_t::wridx register */
#define M2M_STATUS_ENG_WRIDX_QUEUE_WRIDX_SHIFT	0
#define M2M_STATUS_ENG_WRIDX_QUEUE_WRIDX_NBITS	16
#define M2M_STATUS_ENG_WRIDX_QUEUE_WRIDX_MASK	BCM_MASK(M2M_STATUS_ENG_WRIDX_QUEUE_WRIDX)

/** m2m_status_eng_regs_t::rdidx register */
#define M2M_STATUS_ENG_RDIDX_QUEUE_RDIDX_SHIFT	0
#define M2M_STATUS_ENG_RDIDX_QUEUE_RDIDX_NBITS	16
#define M2M_STATUS_ENG_RDIDX_QUEUE_RDIDX_MASK	BCM_MASK(M2M_STATUS_ENG_RDIDX_QUEUE_RDIDX)

/** m2m_status_eng_regs_t::dma_template register */
#define M2M_STATUS_ENG_DMA_TEMPLATE_DMATEMPLATENOTPCIEDP_SHIFT		0
#define M2M_STATUS_ENG_DMA_TEMPLATE_DMATEMPLATENOTPCIEDP_NBITS		1
#define M2M_STATUS_ENG_DMA_TEMPLATE_DMATEMPLATENOTPCIEDP_MASK		\
	BCM_MASK(M2M_STATUS_ENG_DMA_TEMPLATE_DMATEMPLATENOTPCIEDP)

#define M2M_STATUS_ENG_DMA_TEMPLATE_DMATEMPLATECOHERENTDP_SHIFT		1
#define M2M_STATUS_ENG_DMA_TEMPLATE_DMATEMPLATECOHERENTDP_NBITS		1
#define M2M_STATUS_ENG_DMA_TEMPLATE_DMATEMPLATECOHERENTDP_MASK		\
	BCM_MASK(M2M_STATUS_ENG_DMA_TEMPLATE_DMATEMPLATECOHERENTDP)

#define M2M_STATUS_ENG_DMA_TEMPLATE_DMATEMPLATEADDREXTDP_SHIFT		2
#define M2M_STATUS_ENG_DMA_TEMPLATE_DMATEMPLATEADDREXTDP_NBITS		2
#define M2M_STATUS_ENG_DMA_TEMPLATE_DMATEMPLATEADDREXTDP_MASK		\
	BCM_MASK(M2M_STATUS_ENG_DMA_TEMPLATE_DMATEMPLATEADDREXTDP)

#define M2M_STATUS_ENG_DMA_TEMPLATE_DMATEMPLATENOTPCIESP_SHIFT		4
#define M2M_STATUS_ENG_DMA_TEMPLATE_DMATEMPLATENOTPCIESP_NBITS		1
#define M2M_STATUS_ENG_DMA_TEMPLATE_DMATEMPLATENOTPCIESP_MASK		\
	BCM_MASK(M2M_STATUS_ENG_DMA_TEMPLATE_DMATEMPLATENOTPCIESP)

#define M2M_STATUS_ENG_DMA_TEMPLATE_DMATEMPLATECOHERENTSP_SHIFT		5
#define M2M_STATUS_ENG_DMA_TEMPLATE_DMATEMPLATECOHERENTSP_NBITS		1
#define M2M_STATUS_ENG_DMA_TEMPLATE_DMATEMPLATECOHERENTSP_MASK		\
	BCM_MASK(M2M_STATUS_ENG_DMA_TEMPLATE_DMATEMPLATECOHERENTSP)

#define M2M_STATUS_ENG_DMA_TEMPLATE_DMATEMPLATEPERDESCWCDP_SHIFT	7
#define M2M_STATUS_ENG_DMA_TEMPLATE_DMATEMPLATEPERDESCWCDP_NBITS	1
#define M2M_STATUS_ENG_DMA_TEMPLATE_DMATEMPLATEPERDESCWCDP_MASK		\
	BCM_MASK(M2M_STATUS_ENG_DMA_TEMPLATE_DMATEMPLATEPERDESCWCDP)

/** HWA MACIF CLTL register */

/* Req-Ack based MAC_HWA interface */
#define D11_HWA_MACIF_CTL_PHYRXSTS_MAC_HWA_IF_EN_SHIFT			8
#define D11_HWA_MACIF_CTL_PHYRXSTS_MAC_HWA_IF_EN_NBITS			1
#define D11_HWA_MACIF_CTL_PHYRXSTS_MAC_HWA_IF_EN_MASK			\
	BCM_MASK(D11_HWA_MACIF_CTL_PHYRXSTS_MAC_HWA_IF_EN)

/* PhyRx Status transfer method
 * 0 - Only from IHR RXE_PHYSTS_DATA
 * 1 - through SHM DMA and RXE_PHYSTS_DATA_L/H
 */
#define D11_HWA_MACIF_CTL_PHYRXSTS_METHOD_SHIFT				10
#define D11_HWA_MACIF_CTL_PHYRXSTS_METHOD_NBITS				1
#define D11_HWA_MACIF_CTL_PHYRXSTS_METHOD_MASK				\
	BCM_MASK(D11_HWA_MACIF_CTL_PHYRXSTS_METHOD)

/* Count the packages written under single PhyRx Status */
#define D11_HWA_MACIF_CTL_PHYRXSTS_PACKAGE_COUNT_SHIFT			12
#define D11_HWA_MACIF_CTL_PHYRXSTS_PACKAGE_COUNT_NBITS			3
#define D11_HWA_MACIF_CTL_PHYRXSTS_PACKAGE_COUNT_MASK			\
	BCM_MASK(D11_HWA_MACIF_CTL_PHYRXSTS_PACKAGE_COUNT)

/** For corerev >= 130, Tx and PhyRx Status Memory region offsets */
/* rev130 & rev131 */
#define D11_MAC_TXSTS_MEM_OFFSET					0x008d8000
#define D11_MAC_PHYRXSTS_MEM_OFFSET					0x008d9000
/* rev132 */
#define D11_MAC_TXSTS_MEM_OFFSET_REV132					0x008f0000
#define D11_MAC_PHYRXSTS_MEM_OFFSET_REV132				0x008f1000

#if defined(STS_XFER_M2M_INTR)

#define STS_XFER_SWITCHCORE(_sih_, _origidx_, _intr_val_)			\
({										\
	BCM_REFERENCE(_origidx_);						\
	BCM_REFERENCE(_intr_val_);						\
})

#define STS_XFER_RESTORECORE(_sih_, _coreid_, _intr_val_)

#else /* ! STS_XFER_M2M_INTR */

#define STS_XFER_SWITCHCORE(_sih_, _origidx_, _intr_val_)			\
({										\
	*(_origidx_) = 0;							\
	*(_intr_val_) = 0;							\
	 if (BUSTYPE(_sih_->bustype) == PCI_BUS) {				\
		si_switch_core(_sih_, M2MDMA_CORE_ID, _origidx_, _intr_val_);	\
	}									\
})

#define STS_XFER_RESTORECORE(_sih_, _coreid_, _intr_val_)			\
	if (BUSTYPE(_sih_->bustype) == PCI_BUS)					\
		si_restore_core(_sih_, _coreid_, _intr_val_);

#endif /* ! STS_XFER_M2M_INTR */

#if defined(WLCNT)
/** Status Transfer statistics */
#define STS_XFER_STATS
#endif /* WLCNT */

/**
 * ------------------------------------------------------------------------------------------------
 * Section: sts_xfer_ring_t
 *
 * sts_xfer_ring_t abstracts a SW to/from DMA interface implemented as a circular
 * ring. It uses a producer consumer paradigm with read and write indexes as
 * implemented in bcm_ring.h.
 *
 * Producer updates WR index and fetches RD index from consumer context.
 * Consumer updates RD index and fetches WR index from producer context.
 *
 * Each sts_xfer_ring, maintains a local bcm_ring state and is initialized with the locations
 * of the HW RD and WR registers, the ring memory base, depth, element size, ring size and
 * host coherency
 *
 * ------------------------------------------------------------------------------------------------
 */

#define STS_XFER_RING_NULL			((sts_xfer_ring_t *) NULL)
#define STS_XFER_RING_NAME_SIZE			16
#define STS_XFER_RING_STATE(sts_xfer_ring)	(&((sts_xfer_ring)->state))

/** sts_xfer_ring object to implement an interface between DMA and SW */
typedef struct sts_xfer_ring		/* Producer/Consumer circular ring */
{
	bcm_ring_t      state;		/* SW context: read and write state */
	void            *memory;	/* memory for ring */
	uint16          depth;          /* ring depth: num elements in ring */
	uint16		elem_size;	/* size of one status element */
	uint32		ring_size;	/* ring size */
	dmaaddr_t	memory_pa;	/* Physical address of Ring; Used in rev >= 130 (M2M) */
	char            name[STS_XFER_RING_NAME_SIZE];
} sts_xfer_ring_t;

/** Locate an element of specified type in a sts_xfer_ring at a given index. */
#define STS_XFER_RING_ELEM(type, sts_xfer_ring, index)		\
	((type *)((sts_xfer_ring)->memory)) + (index)

/**
 * ------------------------------------------------------------------------------------------------
 * Section: M2M Satus transfer interrupts
 * ------------------------------------------------------------------------------------------------
 */
#if defined(STS_XFER_M2M_INTR)

typedef struct sts_xfer_m2m_intr
{
	uint32	defintmask;	/**< M2M default interrupts */
	uint32	intcontrol;	/**< M2M active interrupts */
	uint32	intstatus;	/**< M2M interrupt status */
	char	irqname[32];	/**< M2M core interrupt name */
} sts_xfer_m2m_intr_t;

#endif /* STS_XFER_M2M_INTR */

/**
 * ------------------------------------------------------------------------------------------------
 * Section: Tx Status Name space: Declarations and definitions
 * ------------------------------------------------------------------------------------------------
 */
/* TODO: */

/**
 * ------------------------------------------------------------------------------------------------
 * Section: PhyRx Status Name space: Declarations and definitions
 * ------------------------------------------------------------------------------------------------
 */

#define PHYRXS_D11PHYRXSTS_SEQNUM(d11phyrxsts)		((((d11phyrxsts)->seq & 0xf000) >> 4) |	\
							((d11phyrxsts)->seq & 0xff))
#define PHYRXS_D11PHYRXSTS_LEN_INVALID(d11phyrxsts) \
	((d11phyrxsts)->PhyRxStatusLen == (uint16)(-1))

#define PHYRXS_SEQNUM_MAX				4096	/* 12 bit sequence number */

/* PhyRx Status seqnum is encoded into wlc_pkttag_t::phyrxs_seqid to fetch PhyRx Status ring
 * element and release in PKTFREE.
 * using (phyrxs_seqnum + 1) as phyrxs_seqid to avoid using default value '0'
 */
#define PHYRXS_PKTTAG_SEQID_2_SEQNUM(_seqid)		((_seqid) - 1)
#define PHYRXS_SEQNUM_2_PKTTAG_SEQID(_seqnum)		((_seqnum) + 1)

/** Rx pending list to hold packets if PhyRx Status is not received */
#if defined(BCMSPLITRX)
#define RX_LIST_PEND_MAX		2
#else /* ! BCMSPLITRX */
#define RX_LIST_PEND_MAX		1
#endif /* ! BCMSPLITRX */

#define RX_LIST_PEND(phyrxs, idx)	(&((sts_xfer_phyrxs_t *) phyrxs)->rx_list_pend[(idx)])

#if defined(STS_XFER_STATS)

/** PhyRx Status Statistics */
#define PHYRX_STATS(phyrxs)		(&(phyrxs)->stats)

typedef struct phyrxs_stats		/** PhyRx Status Transfer statistics */
{
	uint32 recv;			/** Total Received */
	uint32 cons;			/** Consumed by WLAN driver */
	uint32 invalid;			/** Received with Invalid Seq number */
	uint32 miss;			/** PhyRx Statuses with Missed seqnumber */
	uint32 release;			/** PhyRx Status buffers release by WLAN */
} phyrxs_stats_t;
#endif  /* STS_XFER_STATS */

/** PhyRx Status private structure */
typedef struct sts_xfer_phyrxs
{
	volatile m2m_status_eng_regs_t *phyrxs_regs;	/* M2M PhyRx Status Engine regs */
	sts_xfer_ring_t		ring;			/* Circular ring */
	uint16			cons_seqnum;		/* Last consumed seqnum */
	uint16			cur_seqnum;		/* Current operative sequm - NOT USED */
	rx_list_t	rx_list_pend[RX_LIST_PEND_MAX];	/* Pending Rx packets of last DPC */
#if defined(STS_XFER_STATS)
	phyrxs_stats_t		stats;			/* PhyRx Status Statistics */
#endif  /* STS_XFER_STATS */
	uint8			*mem_unaligned;		/* Status buffers unaligned memory */
	uint32			mem_size;		/* Unaligned memory size */
} sts_xfer_phyrxs_t;

/**
 * ------------------------------------------------------------------------------------------------
 * Section: STS_XFER Module Name space: Declarations and definitions
 * ------------------------------------------------------------------------------------------------
 */

/* M2M corerev(si) check */
#define STS_XFER_M2M_SUPPORTED(m2m_corerev)	(((m2m_corerev) == 128) ||	/* 63178, 47622 */ \
						 ((m2m_corerev) == 129) ||	/* 6710 */	\
						 ((m2m_corerev) == 131))	/* 6715 */

#define STS_XFER_PCI64ADDR_HIGH			0x80000000	/* PCI64ADDR_HIGH */
#define STS_XFER_M2M_BURST_LEN			DMA_BL_64	/* 64 byte burst */

/** STS_XFER module private structure */
typedef struct wlc_sts_xfer {
	wlc_sts_xfer_info_t	info;			/* Public exported structure */
	wlc_info_t		*wlc;
	si_t			*sih;
	sts_xfer_phyrxs_t	phyrxs;			/* PhyRx Status transfer handle */
	volatile m2m_core_regs_t * m2m_core_regs;	/* M2M DMA core register space */
#if defined(STS_XFER_M2M_INTR)
	sts_xfer_m2m_intr_t	m2m_intr;		/* M2M core interrupts handle */
	uint			m2m_core_si_flag;
#endif /* STS_XFER_M2M_INTR */
} wlc_sts_xfer_t;

#define WLC_STS_XFER_SIZE			(sizeof(wlc_sts_xfer_t))

/** Status transfer module public structures */
#define WLC_STS_XFER_INFO(sts_xfer)		(&(((wlc_sts_xfer_t *)(sts_xfer))->info))

/** Fetch STS_XFER module pointer from pointer to module's public structure. */
#define WLC_STS_XFER(sts_xfer_info) \
	({ \
		wlc_sts_xfer_t *sts_xfer_module = \
		    (wlc_sts_xfer_t *) CONTAINEROF((sts_xfer_info), wlc_sts_xfer_t, info); \
		sts_xfer_module; \
	})

/** Fetch PHYRXS module pointer */
#define WLC_STS_XFER_PHYRXS(sts_xfer)		(&(((wlc_sts_xfer_t *)(sts_xfer))->phyrxs))

/** Fetch M2M interrupt module pointer */
#define WLC_STS_XFER_M2M_INTR(sts_xfer)		(&(((wlc_sts_xfer_t *)(sts_xfer))->m2m_intr))

/** STS_XFER Module statistics */
#if defined(STS_XFER_STATS)
#define STS_XFER_STATS_INCR(_stats, element)		WLCNTINCR((_stats)->element)
#else /* ! STS_XFER_STATS */
#define STS_XFER_STATS_INCR(_stats, element)
#endif /* ! STS_XFER_STATS */

/** STS_XFER M2M Engine initialize and deinitialize */
static void wlc_sts_xfer_m2m_eng_init(wlc_sts_xfer_t *sts_xfer);
static void wlc_sts_xfer_m2m_eng_deinit(wlc_sts_xfer_t *sts_xfer);

/** STS_XFER module init/de-init handlers */
static int wlc_sts_xfer_init(void *ctx);
static int wlc_sts_xfer_deinit(void *ctx);

#if defined(BCMDBG)
/** STS_XFER module dump handlers */
static int wlc_sts_xfer_dump(void *ctx, struct bcmstrbuf *b);
static int wlc_sts_xfer_dump_clr(void *ctx);
#endif // endif

/** STS_XFER module attach handler */
wlc_sts_xfer_info_t *
BCMATTACHFN(wlc_sts_xfer_attach)(wlc_info_t *wlc)
{
	wlc_sts_xfer_info_t	*sts_xfer_info;
	wlc_sts_xfer_t		*sts_xfer;

	WL_TRACE(("wl%d: %s: ENTER \n", wlc->pub->unit, __FUNCTION__));

	sts_xfer_info = NULL;

	/* Allocate STS_XFER module */
	if ((sts_xfer = MALLOCZ(wlc->osh, WLC_STS_XFER_SIZE)) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}

	sts_xfer_info = WLC_STS_XFER_INFO(sts_xfer);
	sts_xfer->wlc = wlc;
	sts_xfer->sih = wlc->pub->sih;
	sts_xfer_info->unit = wlc->pub->unit;

	wlc->pub->_sts_xfer_txs = FALSE;
	wlc->pub->_sts_xfer_phyrxs = STS_XFER_PHYRXS_OFF;

	if (D11REV_LT(wlc->pub->corerev, 129)) {
		WL_ERROR(("wl%d: %s: Not enabling Tx PhyRx Status tranfer support\n",
			wlc->pub->unit, __FUNCTION__));
		return sts_xfer_info;
	}

	{ /* Tx Status: Allocate Memory and intialize ADT's */
		/* TODO: STS_XFER_TXS_ENAB() */
	}

	{ /* PhyRx Status: Allocate Memory and intialize ADT's */
		sts_xfer_phyrxs_t	*phyrxs;
		sts_xfer_ring_t		*phyrxs_ring;
		wlc_tunables_t		*tunables;
		uint16			elem_size, ring_depth;
		uint16			alignment_req = D11PHYRXSTS_GE129_ALIGN_BYTES;

		phyrxs = WLC_STS_XFER_PHYRXS(sts_xfer);
		phyrxs_ring = &(phyrxs->ring);
		tunables = wlc->pub->tunables;

		ring_depth = tunables->nrxd_sts;
		elem_size = tunables->rxbufsz_sts;

		/* PhyRx Status buffers should always be aligned to 8bytes */
		STATIC_ASSERT((sizeof(d11phyrxsts_t) % D11PHYRXSTS_GE129_ALIGN_BYTES) == 0);
		ASSERT(ALIGN_SIZE(elem_size, alignment_req) == elem_size);
		ASSERT(elem_size == sizeof(d11phyrxsts_t));

		phyrxs->mem_size = elem_size * ring_depth;

		/* Allocate memory for PhyRx Status Ring */
#if defined(BCM_SECURE_DMA)
		/* TODO: PhyRx Status transfer over RxFIFO-3 is not supoorted in STB platforms.
		 * PhyRx Status buffers are allocated from SEC DMA mapped region.
		 * Export phyrxs_ring->memory_pa to hnddma module and avoid performing
		 * SECURE_DMA_MAP/SECURE_DMA_UNMAP on PhyRx Status buffers.
		 */
		ASSERT(D11REV_IS(wlc->pub->corerev, 129) == FALSE);
		phyrxs->mem_unaligned = SECURE_DMA_MAP_STS_PHYRX(wlc->osh, phyrxs->mem_size,
			alignment_req, NULL, &phyrxs_ring->memory_pa, NULL);

		ASSERT(phyrxs_ring->memory != NULL);
		ASSERT(ISALIGNED(PHYSADDRLO(phyrxs_ring->memory_pa), alignment_req));

#else /* ! BCM_SECURE_DMA */
		phyrxs->mem_size += alignment_req;
		phyrxs->mem_unaligned = MALLOC(wlc->osh, phyrxs->mem_size);
		if (phyrxs->mem_unaligned == NULL) {
			WL_ERROR(("wl%d: %s: out of mem for PhyRx ring, malloced %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			goto fail;
		}
#endif /* ! BCM_SECURE_DMA */

		/* PhyRx Status ring configuration */
		phyrxs_ring->memory = ALIGN_ADDR(phyrxs->mem_unaligned, alignment_req);
		phyrxs_ring->depth = ring_depth;
		phyrxs_ring->elem_size = elem_size;
		phyrxs_ring->ring_size = elem_size * ring_depth;

		snprintf(phyrxs_ring->name, STS_XFER_RING_NAME_SIZE, "wl%d_phyrxs", wlc->pub->unit);
		bcm_ring_init(STS_XFER_RING_STATE(phyrxs_ring));

		if (D11REV_IS(wlc->pub->corerev, 129)) {
			WL_INFORM(("%d: %s: PhyRx Status is transferred over Rx FIFO-3\n",
				wlc->pub->unit, __FUNCTION__));

			/* For rev129, PhyRx Status is transferred over RxFIFO-3 */
			wlc->pub->_sts_xfer_phyrxs = STS_XFER_PHYRXS_FIFO;

			/* Set PhyRx Status ring memory for STS_FIFO dma */
			ASSERT((wlc->hw->di[STS_FIFO]) != NULL);
			dma_sts_set_phyrxsts_memory(wlc->hw->di[STS_FIFO],
				(void *)phyrxs_ring->memory);

		} else if (D11REV_GE(wlc->pub->corerev, 130)) {
			WL_INFORM(("%d: %s: PhyRx Status is transferred M2M DMA Ch#3\n",
				wlc->pub->unit, __FUNCTION__));

			/* For rev >= 130, PhyRx Status is transferred using M2M (BME ch#3) */
			wlc->pub->_sts_xfer_phyrxs = STS_XFER_PHYRXS_M2M;

#if !defined(BCM_SECURE_DMA)
			/* Map ring memory for device use only */
			phyrxs_ring->memory_pa = DMA_MAP(wlc->osh, phyrxs_ring->memory,
				phyrxs_ring->ring_size, DMA_RX, NULL, NULL);
			ASSERT(ISALIGNED(PHYSADDRLO(phyrxs_ring->memory_pa), alignment_req));
#endif /* ! BCM_SECURE_DMA */
		}
	}

	if (D11REV_GE(wlc->pub->corerev, 130)) {
		/* Prepare M2M Status Engine */
		wlc_sts_xfer_m2m_eng_init(sts_xfer);
	}

	/* Register STS_XFER module up/down, watchdog, and iovar callbacks */
	if (wlc_module_register(wlc->pub, NULL, "sts_xfer", sts_xfer, NULL, NULL,
		wlc_sts_xfer_init, wlc_sts_xfer_deinit) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

#if defined(BCMDBG)
	wlc_dump_add_fns(wlc->pub, "sts_xfer", wlc_sts_xfer_dump, wlc_sts_xfer_dump_clr,
		(void *)sts_xfer_info);
#endif // endif

	WL_INFORM(("wl%d: %s: STS_XFER module %p STS_XFER info %p \n",  wlc->pub->unit,
		__FUNCTION__, sts_xfer, sts_xfer_info));
	return sts_xfer_info; /* return the STS_XFER module public pointer */

fail:
	MODULE_DETACH(sts_xfer_info, wlc_sts_xfer_detach);
	return NULL;
} /* wlc_sts_xfer_attach() */

/** Status transfer module detach handler */
void
BCMATTACHFN(wlc_sts_xfer_detach)(wlc_sts_xfer_info_t *sts_xfer_info)
{
	wlc_info_t		*wlc;
	wlc_sts_xfer_t		*sts_xfer;

	if (sts_xfer_info == NULL)
		return;

	sts_xfer = WLC_STS_XFER(sts_xfer_info);
	wlc = sts_xfer->wlc;

	WL_TRACE(("wl%d: %s: ENTER\n", wlc->pub->unit, __FUNCTION__));

	if (D11REV_LT(wlc->pub->corerev, 129)) {
		goto done;
	}

	/* Deinitialize M2M Engine */
	if (D11REV_GE(wlc->pub->corerev, 130)) {
		wlc_sts_xfer_m2m_eng_deinit(sts_xfer);
	}

	{ /* Destruct Tx Status Ring */
		/* TODO: STS_XFER_TXS_ENAB() */
	}

	{ /* Destruct PhyRx Status Ring */
		sts_xfer_phyrxs_t	*phyrxs;

		phyrxs = WLC_STS_XFER_PHYRXS(sts_xfer);

		if (phyrxs->mem_unaligned != NULL) {
#if defined(BCM_SECURE_DMA)
			SECURE_DMA_UNMAP_STS_PHYRX(wlc->osh, phyrxs->mem_unaligned,
				phyrxs->mem_size, phyrxs->ring.memory_pa, NULL);
#else /* ! BCM_SECURE_DMA */
			MFREE(wlc->osh, phyrxs->mem_unaligned, phyrxs->mem_size);
#endif /* ! BCM_SECURE_DMA */
		}
	}

	/* Unregister the module */
	wlc_module_unregister(wlc->pub, "sts_xfer", sts_xfer);

	wlc->pub->_sts_xfer_txs = FALSE;
	wlc->pub->_sts_xfer_phyrxs = STS_XFER_PHYRXS_OFF;

done:
	/* Free up the STS_XFER module memory */
	MFREE(wlc->osh, sts_xfer, WLC_STS_XFER_SIZE);
} /* wlc_sts_xfer_detach() */

/**
 * ------------------------------------------------------------------------------------------------
 * wlc_sts_xfer_m2m_eng_init: Initialize M2M Status Transfer Engine (BME CH#3)
 * - Enable receive and Transmit channels
 * - Get M2M Status transfer engine register space
 *
 * In NIC builds, for Chips over PCIe  mapping sixth 4KB region of PCIE BAR 0 space (0x78) to
 * M2M core register space.
 * ------------------------------------------------------------------------------------------------
 */
static void
BCMATTACHFN(wlc_sts_xfer_m2m_eng_init)(wlc_sts_xfer_t *sts_xfer)
{
	si_t				*sih;
	volatile char			*base_addr;	/* M2M core register base */
	volatile m2m_eng_regs_t		*m2m_eng_regs;	/* Transmit/Receive DMA registers */
	uint32				saved_core_id;
	uint32				saved_intr_val;
	uint32				v32; /* used in REG read/write */

	sih = sts_xfer->sih;

	base_addr = (volatile char *)
		si_switch_core(sih, M2MDMA_CORE_ID, &saved_core_id, &saved_intr_val);
	ASSERT(base_addr != NULL);

	/* Take M2M core out of reset if it's not */
	if (!si_iscoreup(sih))
		si_core_reset(sih, 0, 0);

	ASSERT(STS_XFER_M2M_SUPPORTED(si_corerev(sih)));

#if !defined(DONGLEBUILD) && defined(STS_XFER_M2M_INTR)
	/* For Chips over PCIe, mapping sixth 4KB region of PCIE BAR 0 space (0x78) to
	 * M2M core register space.
	 */
	if (BUSTYPE(sih->bustype) == PCI_BUS) {
		/* Set PCIE register PCIE2_BAR0_CORE2_WIN2 with M2M core register space. */
		si_backplane_pci_config_bar0_core2_win2(sih,
			si_addrspace(sih, CORE_SLAVE_PORT_0, CORE_BASE_ADDR_0));

		/* Adjust the M2M regs with offset for PCIE2_BAR0_CORE2_WIN2 */
		base_addr = base_addr + PCIE2_BAR0_CORE2_WIN2_OFFSET;

		/* Get M2M core bitmap in DMP wrapper's oob_select_a_in register (oobselouta30).
		 * Used to configure M2M core interrupt in PCIEIntMask (PCI_INT_MASK) register.
		 */
		sts_xfer->m2m_core_si_flag = si_flag(sih);
	}
#endif /* ! DONGLEBUILD && STS_XFER_M2M_INTR */

	sts_xfer->m2m_core_regs = (volatile m2m_core_regs_t *) base_addr;

	/* Offset address of M2M Status Transfer engine ch#3
	 * Transmit DMA Processor	- 0x2C0
	 * Receive DMA processor	- 0x2E0
	 */
	m2m_eng_regs = &sts_xfer->m2m_core_regs->eng_regs[M2M_STATUS_ENG_M2M_CHANNEL];

	/* XXX Some fields of XmtCtrl and RcvCtrl apply to descr processor.
	 * Use read-modify-write, to update Enable and BurstLen.
	 */
	/* Enable the transmit channel of this engine */
	v32 = R_REG(sts_xfer->wlc->osh, &m2m_eng_regs->tx.control);
	v32 = BCM_CBF(v32, D64_XC_BL);
	v32 |= (D64_XC_XE					/* Transmit Enable */
		| BCM_SBF(STS_XFER_M2M_BURST_LEN, D64_XC_BL));	/* Transmit Burst Length */
	W_REG(sts_xfer->wlc->osh, &m2m_eng_regs->tx.control, v32);

	/* Enable the receive channel of this engine */
	v32 = R_REG(sts_xfer->wlc->osh, &m2m_eng_regs->rx.control);
	v32 = BCM_CBF(v32, D64_RC_BL);
	v32 |= (D64_RC_RE					/* Receive Enable */
		| BCM_SBF(STS_XFER_M2M_BURST_LEN, D64_RC_BL));	/* Recieve Burst Length */
	W_REG(sts_xfer->wlc->osh, &m2m_eng_regs->rx.control, v32);

	/* Set M2M CORE PhyRx Status registers - Offset 0x900 */
	ASSERT(OFFSETOF(m2m_core_regs_t, phyrxs_regs) == M2M_STATUS_ENG_PHYRXS_REGS_OFFSET);
	sts_xfer->phyrxs.phyrxs_regs = &sts_xfer->m2m_core_regs->phyrxs_regs;

	/* TODO: STS_XFER_TXS_ENAB() - Set M2M TX Status registers - Offset 0x800 */
	ASSERT(OFFSETOF(m2m_core_regs_t, txs_regs) == M2M_STATUS_ENG_TXS_REGS_OFFSET);

	si_restore_core(sih, saved_core_id, saved_intr_val);

} /* wlc_sts_xfer_m2m_eng_init() */

/** De-initialize M2M Status Transfer engine ch#3 */
static void
BCMATTACHFN(wlc_sts_xfer_m2m_eng_deinit)(wlc_sts_xfer_t *sts_xfer)
{
	m2m_eng_regs_t	*m2m_eng_regs;
	uint32		saved_core_id;
	uint32		saved_intr_val;

	ASSERT(sts_xfer->m2m_core_regs != NULL);
	m2m_eng_regs = &sts_xfer->m2m_core_regs->eng_regs[M2M_STATUS_ENG_M2M_CHANNEL];

	STS_XFER_SWITCHCORE(sts_xfer->sih, &saved_core_id, &saved_intr_val);

	/* Disable the transmit and receive channels of M2M Status Engine */
	AND_REG(sts_xfer->wlc->osh, &m2m_eng_regs->tx.control, ~D64_XC_XE);
	AND_REG(sts_xfer->wlc->osh, &m2m_eng_regs->rx.control, ~D64_RC_RE);

	STS_XFER_RESTORECORE(sts_xfer->sih, saved_core_id, saved_intr_val);

} /* wlc_sts_xfer_m2m_eng_deinit() */

/**
 * ------------------------------------------------------------------------------------------------
 *  __sts_xfer_m2m_status_eng_init: Initialize Status (Tx/PhyRx) specific Transfer Engine
 *  - Enable Status engine
 *  - Configure M2M engine source (MAC status memory) and destination (ring memory) address space
 *  - In NIC builds, enable M2M interrupts
 * ------------------------------------------------------------------------------------------------
 */
static INLINE void
BCMINITFN(__sts_xfer_m2m_status_eng_init)(wlc_sts_xfer_t *sts_xfer, sts_xfer_ring_t *ring,
	m2m_status_eng_regs_t *status_eng_regs, uint32 status_src_addr)
{
	volatile m2m_eng_regs_t *m2m_eng_regs;
	void	*osh;
	uint32	dma_template;
	uint32	v32; /* used in REG read/write */

	osh = sts_xfer->wlc->osh;
	m2m_eng_regs = &sts_xfer->m2m_core_regs->eng_regs[M2M_STATUS_ENG_M2M_CHANNEL];

	/* Disable M2M Status engine */
	AND_REG(osh, &status_eng_regs->cfg, ~M2M_STATUS_ENG_CFG_MODULEEN_MASK);

	/* Map M2M Engine src address to MAC Status Memory region */
	W_REG(osh, &status_eng_regs->sa_base_l, status_src_addr);
	W_REG(osh, &status_eng_regs->sa_base_h, 0);

	ASSERT((sizeof(PHYSADDRLO(ring->memory_pa)) > sizeof(uint32)) ?
		((PHYSADDRLO(ring->memory_pa) >> 32) == 0) : TRUE);

	ASSERT((PHYSADDRHI(ring->memory_pa) & STS_XFER_PCI64ADDR_HIGH) == 0);

	dma_template = (M2M_STATUS_ENG_DMA_TEMPLATE_DMATEMPLATENOTPCIESP_MASK
			| M2M_STATUS_ENG_DMA_TEMPLATE_DMATEMPLATEPERDESCWCDP_MASK
			| M2M_STATUS_ENG_DMA_TEMPLATE_DMATEMPLATECOHERENTDP_MASK);

	if (BUSTYPE(sts_xfer->sih->bustype) == PCI_BUS) {
		/* Map M2M Engine destination address to status ring memory */
		/* XXX: For destination address setting, the da_base_l/h is from BME's perspective
		 * in NIC-400; remap the host memory address to PCIE large memory window
		 */

		W_REG(osh, &status_eng_regs->da_base_l, (uint32)PHYSADDRLO(ring->memory_pa));
		W_REG(osh, &status_eng_regs->da_base_h,
				PHYSADDRHI(ring->memory_pa) | STS_XFER_PCI64ADDR_HIGH);
	} else {
		/* Map M2M Engine destination address to status ring memory */
		W_REG(osh, &status_eng_regs->da_base_l, (uint32)PHYSADDRLO(ring->memory_pa));
		W_REG(osh, &status_eng_regs->da_base_h, PHYSADDRHI(ring->memory_pa));

		dma_template |= M2M_STATUS_ENG_DMA_TEMPLATE_DMATEMPLATENOTPCIEDP_MASK;
	}

	/* Set DMA_TEMPLATE setting */
	W_REG(osh, &status_eng_regs->dma_template, dma_template);

	/* XXX: Enable WCPD bit in RcvCtrl register as WCPD per descriptor is set
	 * in dma_template register, as suggested by HW designer, for consistency
	 */
	v32 = R_REG(osh, &m2m_eng_regs->rx.control);
	v32 |= D64_RC_WCPD;
	W_REG(osh, &m2m_eng_regs->rx.control, v32);

	/* Reset Write Index */
	v32 = BCM_SBF(0, M2M_STATUS_ENG_WRIDX_QUEUE_WRIDX);
	W_REG(osh, &status_eng_regs->wridx, v32);
	/* Reset Read Index */
	v32 = BCM_SBF(0, M2M_STATUS_ENG_RDIDX_QUEUE_RDIDX);
	W_REG(osh, &status_eng_regs->rdidx, v32);

	/* Set status ring size (No of status elements) */
	v32 = BCM_SBF(ring->depth, M2M_STATUS_ENG_SIZE_QUEUE_SIZE);
	W_REG(osh, &status_eng_regs->size, v32);

	v32 = R_REG(osh, &status_eng_regs->ctl);
	v32 = BCM_CBF(v32, M2M_STATUS_ENG_CTL_MACSTATUSSIZE);  /* Status element size */
	v32 |= BCM_SBF(ring->elem_size, M2M_STATUS_ENG_CTL_MACSTATUSSIZE);

#if defined(STS_XFER_M2M_INTR)
	/* Interrupt on every status written to Status Queue. */
	v32 = BCM_CBF(v32, M2M_STATUS_ENG_CTL_LAZYCOUNT);	/* Status interrupt lazycount */
	v32 |= BCM_SBF(0x1, M2M_STATUS_ENG_CTL_LAZYCOUNT);
#endif /* STS_XFER_M2M_INTR */

	W_REG(osh, &status_eng_regs->ctl, v32);

	/* Enable M2M Status engine */
	OR_REG(osh, &status_eng_regs->cfg, M2M_STATUS_ENG_CFG_MODULEEN_MASK);

	WL_INFORM(("wl%d: %s: for %s elem_size[%d] depth[%d] ring size[%d]\n",
		sts_xfer->wlc->pub->unit, __FUNCTION__, ring->name, ring->elem_size,
		ring->depth, ring->ring_size));
} /* __sts_xfer_m2m_status_eng_init() */

/** De-initialize Status (Tx/PhyRx) specific Transfer Engine */
static INLINE void
BCMUNINITFN(__sts_xfer_m2m_status_eng_deinit)(wlc_sts_xfer_t *sts_xfer, sts_xfer_ring_t *ring,
	m2m_status_eng_regs_t *status_eng_regs)
{
	BCM_REFERENCE(ring);
	/* Disable M2M Status engine */
	AND_REG(sts_xfer->wlc->osh, &status_eng_regs->cfg, ~M2M_STATUS_ENG_CFG_MODULEEN_MASK);

} /* __sts_xfer_m2m_status_eng_deinit() */

/** STS_XFER init handler */
static int
BCMINITFN(wlc_sts_xfer_init)(void *ctx)
{
	wlc_info_t	*wlc;
	wlc_sts_xfer_t	*sts_xfer;
	uint32		d11_src_addr;
	uint32		saved_core_id;
	uint32		saved_intr_val;
	uint32		idx, defintmask = 0;
	uint16		v16; /* used in REG read/write */

	sts_xfer = (wlc_sts_xfer_t *)ctx;
	wlc = sts_xfer->wlc;
	BCM_REFERENCE(defintmask);

	WL_TRACE(("wl%d: %s: ENTER\n", wlc->pub->unit, __FUNCTION__));

	if (D11REV_GE(wlc->pub->corerev, 130)) {
		wlc_sts_xfer_m2m_eng_init(sts_xfer);
	}

	/* TODO: STS_XFER_TXS_ENAB() - Prepare and enable Tx Status Engine */

	if (STS_XFER_PHYRXS_ENAB(wlc->pub)) {
		sts_xfer_phyrxs_t	*phyrxs;

		phyrxs = WLC_STS_XFER_PHYRXS(sts_xfer);

		phyrxs->cur_seqnum = 0;
		phyrxs->cons_seqnum = 0;

		for (idx = 0; idx < RX_LIST_PEND_MAX; idx++) {
			RX_LIST_INIT(RX_LIST_PEND(phyrxs, idx));
		}

		/* Reset status ring SW indexes */
		bcm_ring_init(STS_XFER_RING_STATE(ring));

#if defined(STS_XFER_STATS)
		memset((void*)PHYRX_STATS(phyrxs), 0, sizeof(phyrxs_stats_t));
#endif /* STS_XFER_STATS */

		if (STS_XFER_PHYRXS_M2M_ENAB(wlc->pub)) {

			STS_XFER_SWITCHCORE(sts_xfer->sih, &saved_core_id, &saved_intr_val);

			d11_src_addr = SI_ENUM_BASE_BP(sts_xfer->sih);

			if (D11REV_IS(wlc->pub->corerev, 132))
				d11_src_addr += D11_MAC_PHYRXSTS_MEM_OFFSET_REV132;
			else
				d11_src_addr += D11_MAC_PHYRXSTS_MEM_OFFSET;

			/* Prepare and enable M2M PhyRx Status Engine */
			__sts_xfer_m2m_status_eng_init(sts_xfer, &(sts_xfer->phyrxs.ring),
					sts_xfer->phyrxs.phyrxs_regs, d11_src_addr);

			STS_XFER_RESTORECORE(sts_xfer->sih, saved_core_id, saved_intr_val);

#if defined(STS_XFER_M2M_INTR)
			/* Enable M2M Interrupt on update of PhyRx Status Write Index */
			defintmask |= BCM_SBF(1, M2M_CORE_INTCONTROL_PHYRXSWRINDUPD_INTMASK);
#endif /* STS_XFER_M2M_INTR */

			/**
			 * HWA_MACIF_CTL regiter used to enable MAC-HWA interface and program
			 * package count under one PhyRx Status
			 */
			v16 = (uint16) (BCM_SBF(1, D11_HWA_MACIF_CTL_PHYRXSTS_MAC_HWA_IF_EN)
					| BCM_SBF(1, D11_HWA_MACIF_CTL_PHYRXSTS_METHOD)
					| BCM_SBF(1, D11_HWA_MACIF_CTL_PHYRXSTS_PACKAGE_COUNT));
			W_REG(sts_xfer->wlc->osh, D11_HWA_MACIF_CTL(wlc), v16);
		}
	}

#if defined(STS_XFER_M2M_INTR)
	sts_xfer->m2m_intr.defintmask = defintmask;

	if (D11REV_GE(wlc->pub->corerev, 130)) {
		ASSERT(defintmask != 0);
		wlc_sts_xfer_intrson(wlc);
	}
#endif /* STS_XFER_M2M_INTR */

	WL_INFORM(("wl%d: %s: M2M interrupt defintmask[0x%x]\n", wlc->pub->unit, __FUNCTION__,
		defintmask));

	return BCME_OK;
} /* wlc_sts_xfer_init() */

/** STS_XFER de-init handler */
static int
BCMUNINITFN(wlc_sts_xfer_deinit)(void *ctx)
{
	wlc_sts_xfer_t	*sts_xfer;
	uint32		saved_core_id;
	uint32		saved_intr_val;

	sts_xfer = (wlc_sts_xfer_t *) ctx;

	WL_TRACE(("wl%d: %s: ENTER\n", sts_xfer->wlc->pub->unit, __FUNCTION__));

#if defined(STS_XFER_M2M_INTR)
	/* Disable M2M interrupts */
	sts_xfer->m2m_intr.defintmask = 0;
	if (D11REV_GE(sts_xfer->wlc->pub->corerev, 130))
		wlc_sts_xfer_intrsoff(sts_xfer->wlc);
#endif /* STS_XFER_M2M_INTR */

	STS_XFER_SWITCHCORE(sts_xfer->sih, &saved_core_id, &saved_intr_val);

	/* TODO: STS_XFER_TXS_ENAB() - Deinitilize M2M TX status transfer engine and SW states */

	if (STS_XFER_PHYRXS_ENAB(sts_xfer->wlc->pub)) {
		/* Disable M2M PhyRx Status Engine */
		if (STS_XFER_PHYRXS_M2M_ENAB(sts_xfer->wlc->pub)) {
			__sts_xfer_m2m_status_eng_deinit(sts_xfer, &(sts_xfer->phyrxs.ring),
					sts_xfer->phyrxs.phyrxs_regs);
		}
	}

	STS_XFER_RESTORECORE(sts_xfer->sih, saved_core_id, saved_intr_val);

	return 0; /* callbacks */
} /* wlc_sts_xfer_deinit() */

/** Flush pending packets and udpate SW status ring states */
void
wlc_sts_xfer_flush_queues(wlc_info_t *wlc)
{
	wlc_sts_xfer_t	*sts_xfer;
	void		*pkt;
	int		idx;

	WL_TRACE(("wl%d: %s: ENTER \n", wlc->pub->unit, __FUNCTION__));

	sts_xfer = WLC_STS_XFER(wlc->sts_xfer_info);

	/* TODO: STS_XFER_TXS_ENAB() - Reset Tx Status states */

	if (STS_XFER_PHYRXS_ENAB(wlc->pub)) {
		sts_xfer_phyrxs_t	*phyrxs;
		rx_list_t		*rx_list_pend;
		uint16 wridx = 0;

		phyrxs = WLC_STS_XFER_PHYRXS(sts_xfer);

		/* Free all packets from rx_list_pend */
		for (idx = 0; idx < RX_LIST_PEND_MAX; idx++) {
			rx_list_pend = RX_LIST_PEND(phyrxs, idx);
			while (RX_LIST_RX_HEAD(rx_list_pend) != NULL) {
				pkt = RX_LIST_DELETE_HEAD(rx_list_pend);
				PKTFREE(wlc->osh, pkt, FALSE);
			}
		}

		if (STS_XFER_PHYRXS_FIFO_ENAB(wlc->pub)) {
			/* STS FIFO are already reclaimed in wlc_flushqueues.
			 * Reset only SW ring states
			 */
			/* Get STS FIFO DMA attributes */
			wridx = dma_sts_rxin(wlc->hw->di[STS_FIFO]);
		} else { /* STS_XFER_PHYRXS_M2M */
			volatile m2m_status_eng_regs_t *status_eng_regs;
			uint32	saved_core_id;
			uint32	saved_intr_val;
			uint32	v32; /* used in REG read/write */

			status_eng_regs = phyrxs->phyrxs_regs;

			STS_XFER_SWITCHCORE(sts_xfer->sih, &saved_core_id, &saved_intr_val);

			v32 = R_REG(wlc->osh, &status_eng_regs->wridx);
			wridx = BCM_GBF(v32, M2M_STATUS_ENG_WRIDX_QUEUE_WRIDX);

			/* Update M2M status engine read index to write index */
			v32 = R_REG(wlc->osh, &status_eng_regs->rdidx);
			v32 = BCM_CBF(v32, M2M_STATUS_ENG_RDIDX_QUEUE_RDIDX);
			v32 |= BCM_SBF(wridx, M2M_STATUS_ENG_RDIDX_QUEUE_RDIDX);
			W_REG(wlc->osh, &status_eng_regs->rdidx, v32);

			v32 = R_REG(sts_xfer->wlc->osh, &status_eng_regs->rdidx);

			STS_XFER_RESTORECORE(sts_xfer->sih, saved_core_id, saved_intr_val);
		}

		STS_XFER_RING_STATE(&phyrxs->ring)->read = wridx;
		STS_XFER_RING_STATE(&phyrxs->ring)->write = wridx;
	}

} /* wlc_sts_xfer_flush_queues() */

#if defined(BCMDBG)

/** Dump Status Transfer stats */
static int
wlc_sts_xfer_dump(void *ctx, struct bcmstrbuf *b)
{
	wlc_sts_xfer_t *sts_xfer = (wlc_sts_xfer_t *)ctx;
	wlc_info_t *wlc;

	wlc = sts_xfer->wlc;
	BCM_REFERENCE(wlc);

	/* TODO: STS_XFER_TXS_ENAB() - Dump Tx Status transfer stats */

	if (STS_XFER_PHYRXS_ENAB(wlc->pub)) {	/* Dump PhyRx Status transfer stats */
		sts_xfer_phyrxs_t	*phyrxs;
		sts_xfer_ring_t		*phyrxs_ring;

		phyrxs = WLC_STS_XFER_PHYRXS(sts_xfer);
		phyrxs_ring = &(phyrxs->ring);

		bcm_bprintf(b, "PhyRx Status Transfer through - %s\n",
			STS_XFER_PHYRXS_FIFO_ENAB(wlc->pub) ? "RxFIFO-3" : "M2M DMA");

		bcm_bprintf(b, "cur_seqnum[%d] cons_seqnum[%d]\n",
			phyrxs->cur_seqnum, phyrxs->cur_seqnum);

		bcm_bprintf(b, "%s Ring: Element Size[%d] depth[%d] rdidx[%d] wridx[%d]\n",
			phyrxs_ring->name, phyrxs_ring->elem_size, phyrxs_ring->depth,
			STS_XFER_RING_STATE(phyrxs_ring)->read,
			STS_XFER_RING_STATE(phyrxs_ring)->write);

#if defined(STS_XFER_STATS)
		bcm_bprintf(b, "PhyRx Stats: recv[%d] cons[%d] invalid[%d] miss[%d] release[%d]\n",
			PHYRX_STATS(phyrxs)->recv, PHYRX_STATS(phyrxs)->cons,
			PHYRX_STATS(phyrxs)->invalid, PHYRX_STATS(phyrxs)->miss,
			PHYRX_STATS(phyrxs)->release);
#endif /* STS_XFER_STATS */
	}

#if defined(STS_XFER_M2M_INTR)
	{
		sts_xfer_m2m_intr_t *m2m_intr;
		m2m_intr = WLC_STS_XFER_M2M_INTR(sts_xfer);

		bcm_bprintf(b, "\nM2M interrupt:\n");
		bcm_bprintf(b, "defintmask:0x%08x\t intcontrol: 0x%08x\t intstatus: 0x%08x\n",
			m2m_intr->defintmask, m2m_intr->intcontrol, m2m_intr->intstatus);
	}
#endif /* STS_XFER_M2M_INTR */

	return BCME_OK;
} /* wlc_sts_xfer_dump() */

/** Clear Status Transfer stats */
static int
wlc_sts_xfer_dump_clr(void *ctx)
{
	wlc_sts_xfer_t *sts_xfer = (wlc_sts_xfer_t *)ctx;

#if defined(STS_XFER_STATS)
	if (STS_XFER_PHYRXS_ENAB(sts_xfer->wlc->pub)) {	/* Phy Rx Status Transfer Stats */
		sts_xfer_phyrxs_t	*phyrxs;

		phyrxs = WLC_STS_XFER_PHYRXS(sts_xfer);
		memset((void*)PHYRX_STATS(phyrxs), 0, sizeof(phyrxs_stats_t));
	}
#endif /* STS_XFER_STATS */

	return BCME_OK;
} /* wlc_sts_xfer_dump_clr() */

#endif // endif

/**
 * ------------------------------------------------------------------------------------------------
 *  Section: Tx Status Processing
 *  TODO: STS_XFER_TXS_ENAB()
 * ------------------------------------------------------------------------------------------------
 */

/**
 * ------------------------------------------------------------------------------------------------
 * Section: PhyRx Status Processing
 *
 * From MAC rev >= 129, PhyRx Status is sent asynchronously to host (WLAN driver*) independent
 * of Ucode and HW status. Because of the asynchronous nature of PHY status, PHY Status and
 * Ucode status for the MPDU's are "linked" together with a 12bit - sequence number.
 * 12 bit sequence number is maintained by Ucode and is updated if at least one Ucode status was
 * sent out for the current PPDU.
 * The sequence number is indicated in ucode status of MPDU's (d11rxhdr_ge129_t::RxStatus3) and in
 * PhyRx Status (d11phyrxsts_t::seq)
 * Note: MAC will post a dummy PhyRx Status in case the PhyStatus had not arrived from PHY to MAC
 * rather than simply dropping (not posting) with skipped seqnum.
 *
 * - MAC rev = 129: STS_XFER_PHYRXS_FIFO_ENAB()
 *   The Phy Rx status goes out on D11 Rx FIFO-3 (STS_FIFO)
 * - MAC rev >= 130: STS_XFER_PHYRXS_M2M_ENAB()
 *   BME channel# 3 (M2M DMA) is used to transfer Phy Rx Status from MAC to HOST.
 *
 * Note: XXX PhyRx Status processing of uplink multi-user ofdma traffic is not handled yet.
 *
 * STS_XFER module maintains a ring (sts_xfer_ring_t) of PhyRx Status buffers with read and write
 * indexes and programs DMA decriptors/M2M DMA with attributes of the ring.
 * On arrival of Rx packets, one MPDU of an AMPDU (need not to be last) is linked with PhyRx Status
 * buffer and sent up for WLAN Rx Processing. PhyRx Status buffers are released after processing
 * Phy Rx Status or on PKTFREE.
 * WLAN driver will not hold any PhyRx Status buffers. In case of AMPDU reordering, PhyRx Status
 * buffer will be de-linked with Rx packet and only Rx packet will held in AMPDU Rx Queue.
 *
 * A callback (wlc_sts_xfer_phyrxs_free()) is registered with OSL layer to release a PhyRx Status
 * buffer on PKTFREE of Rx packet.
 *
 * Implementation Notes:
 * - Lower N bits of PhyRx Status seqnumber is used as index to PhyRx Status buffer in the ring,
 *   where 2^N is the depth of the ring.
 * - PhyRx Status buffer is linked to Rx packet using SW header (wlc_d11rxhdr_t::d11phyrxsts)
 * - PhyRx Status seqnum is encoded into wlc_pkttag_t::phyrxs_seqid to fetch PhyRx Status ring
 *   element and release in PKTFREE.
 * ------------------------------------------------------------------------------------------------
 */

/** __phyrxs_ring_write_update: Updates the SW write index */
static INLINE void BCMFASTPATH
__phyrxs_ring_write_update(wlc_sts_xfer_t *sts_xfer, sts_xfer_phyrxs_t *phyrxs)
{
	wlc_info_t	*wlc;
	sts_xfer_ring_t *phyrxs_ring;
	uint16 rdidx, wridx;

	BCM_REFERENCE(rdidx);

	wlc = sts_xfer->wlc;
	phyrxs_ring = &phyrxs->ring;

	if (STS_XFER_PHYRXS_FIFO_ENAB(wlc->pub)) {

		/* Receive and sync PhyRx Status buffers from STS_FIFO dma */
		wridx = dma_sts_rx(wlc->hw->di[STS_FIFO]);

	} else { /* STS_XFER_PHYRXS_M2M */
		volatile m2m_status_eng_regs_t *status_eng_regs;
		uint32	saved_core_id;
		uint32	saved_intr_val;
		uint32	v32; /* used in REG read/write */

		ASSERT(STS_XFER_PHYRXS_M2M_ENAB(wlc->pub));
		status_eng_regs = phyrxs->phyrxs_regs;

		STS_XFER_SWITCHCORE(sts_xfer->sih, &saved_core_id, &saved_intr_val);
		v32 = R_REG(wlc->osh, &status_eng_regs->wridx);
		STS_XFER_RESTORECORE(sts_xfer->sih, saved_core_id, saved_intr_val);

		wridx = BCM_GBF(v32, M2M_STATUS_ENG_WRIDX_QUEUE_WRIDX);
	}

#if !defined(OSL_CACHE_COHERENT)
	/* Cache Invalidate PhyRx Status buffers if host is not coherent */
#if defined(BCM_SECURE_DMA)
	/* TODO: STS_XFER is not supoorted in STB platforms. */
	ASSERT(0);
#else /* ! BCM_SECURE_DMA */
	{
		uint8 *mem_addr;
		uint32 mem_size;

		rdidx = STS_XFER_RING_STATE(phyrxs_ring)->read;

		/* Check for roll-over */
		if (rdidx > wridx) {
			mem_size = ((phyrxs_ring->depth - rdidx) * phyrxs_ring->elem_size);
			mem_addr = (uint8 *)phyrxs_ring->memory +
				(rdidx * phyrxs_ring->elem_size);
			OSL_CACHE_INV(mem_addr, mem_size);
			rdidx = 0; /* Overwriting rdidx */
		}

		mem_size = (wridx - rdidx) * phyrxs_ring->elem_size;
		mem_addr = (uint8 *)phyrxs_ring->memory + (rdidx * phyrxs_ring->elem_size);
		OSL_CACHE_INV(mem_addr, mem_size);
	}
#endif /* ! BCM_SECURE_DMA */
#endif /* ! OSL_CACHE_COHERENT */

	WL_INFORM(("wl%d: %s: wridx[%d]\n", wlc->pub->unit, __FUNCTION__, wridx));
	STS_XFER_RING_STATE(phyrxs_ring)->write = wridx;

} /* __phyrxs_ring_write_update() */

/**
 * _phyrxs_seqnum_valid:  Validate a given index to the PhyRx Status buffer.
 * - In Status processing, wridx points to Write index of the Ring.
 * - In Status release, wridx points to the next index of last consumed PhyRx Status buffer.
 */
static bool BCMFASTPATH
_phyrxs_seqnum_valid(uint16 phyrxs_seqnum, uint16 depth,
	uint16 rdidx, uint16 wridx)
{
	uint16 seqidx;
	bool seqnum_valid = FALSE;

	/* Fetch phyrxs->ring lookup index from seqnum and validate index */
	seqidx = phyrxs_seqnum & (depth - 1);
	if (wridx > rdidx) {
		seqnum_valid = ((rdidx <= seqidx) & (seqidx < wridx));
	} else {
		seqnum_valid = ((rdidx <= seqidx) | (seqidx < wridx));
	}
	return seqnum_valid;
} /* _phyrxs_seqnum_valid() */

/** __phyrxs_seqnum_avail: Returns TRUE if PhyRx Status is availabe for a given sequence number */
static INLINE bool BCMFASTPATH
__phyrxs_seqnum_avail(sts_xfer_phyrxs_t *phyrxs, uint16 phyrxs_seqnum)
{
	sts_xfer_ring_t *phyrxs_ring;
	uint16 rdidx, wridx;
	bool seqnum_avail = FALSE;

	phyrxs_ring = &phyrxs->ring;

	if (bcm_ring_is_empty(STS_XFER_RING_STATE(phyrxs_ring)))
		return seqnum_avail;

	rdidx = STS_XFER_RING_STATE(phyrxs_ring)->read;
	wridx = STS_XFER_RING_STATE(phyrxs_ring)->write;

	seqnum_avail = _phyrxs_seqnum_valid(phyrxs_seqnum, phyrxs_ring->depth, rdidx, wridx);
	return seqnum_avail;
} /* __phyrxs_seqnum_avail() */

/**
 * __phyrxs_consume_d11phyrxsts:
 * - Validates Seqnumber and links PhyRx Status buffer to the Rx packet.
 */
static bool BCMFASTPATH
__phyrxs_consume_d11phyrxsts(wlc_info_t *wlc, sts_xfer_phyrxs_t *phyrxs, void *pkt)
{
	d11rxhdr_t	*rxh;
	wlc_d11rxhdr_t	*wrxh;
	d11phyrxsts_t	*d11phyrxsts;
	sts_xfer_ring_t *phyrxs_ring;
	uint16		seqidx, rxh_seqnum;

	rxh = (d11rxhdr_t *)PKTDATA(wlc->osh, pkt);
#if defined(DONGLEBUILD)
	// TODO: Fix rxh_offset in Full dongle
	ASSERT(0);
#endif /* DONGLEBUILD */

	ASSERT(!IS_D11RXHDRSHORT(rxh, wlc->pub->corerev));

	rxh_seqnum = D11RXHDR_GE129_SEQNUM(rxh);

	STS_XFER_STATS_INCR(PHYRX_STATS(phyrxs), recv);

	phyrxs_ring = &phyrxs->ring;

	if (bcm_ring_is_empty(STS_XFER_RING_STATE(phyrxs_ring)))
		goto d11phyrxsts_invalid;

	seqidx = rxh_seqnum & (uint16)(phyrxs_ring->depth -1);

	/** Verify seqnumber in Ucode status with seqnumber in PhyRx Status */
	d11phyrxsts = STS_XFER_RING_ELEM(d11phyrxsts_t, phyrxs_ring, seqidx);
	if ((rxh_seqnum != PHYRXS_D11PHYRXSTS_SEQNUM(d11phyrxsts)) ||
		PHYRXS_D11PHYRXSTS_LEN_INVALID(d11phyrxsts)) {
		goto d11phyrxsts_invalid;
	}

	phyrxs->cons_seqnum = rxh_seqnum;
	STS_XFER_STATS_INCR(PHYRX_STATS(phyrxs), cons);

	/* Set Valid PhyRx Status flag in d11rxhdr_t::dma_flags */
	RXHDR_GE129_SET_DMA_FLAGS_RXS_TYPE(rxh, RXS_MAC_UCODE_PHY);

	/* Set PhyRx Status buffer in SW header */
	wrxh = CONTAINEROF(rxh, wlc_d11rxhdr_t, rxhdr);
	wrxh->d11phyrxsts = d11phyrxsts;

	WL_INFORM(("wl%d: %s: Linking PhyRx Status buffer with seqnum[%d] to pkt[%p]\n",
		wlc->pub->unit, __FUNCTION__, rxh_seqnum, pkt));

	/* Encode seqnumber into PKTTAG */
	WLPKTTAG(pkt)->phyrxs_seqid = PHYRXS_SEQNUM_2_PKTTAG_SEQID(rxh_seqnum);

	return TRUE;

d11phyrxsts_invalid:

	STS_XFER_STATS_INCR(PHYRX_STATS(phyrxs), invalid);
	WL_ERROR(("wl%d: %s: PhyRx Status buffer with seqnum[%d] linking failed\n",
		wlc->pub->unit, __FUNCTION__, rxh_seqnum));

	WLPKTTAG(pkt)->phyrxs_seqid = STS_XFER_PHYRXS_SEQID_INVALID;
	return FALSE;

} /* __phyrxs_consume_d11phyrxsts() */

/**
 * __phyrxs_find_last_mpdu:
 * - Update rx_list_iter with last MPDU among set of packets with same seqnumber of head packet
 *   in rx_list_pend.
 * - All other MPDUS with same sequence of head packet are appended to the rx_list_release.
 * - rxh_next_seqnum_avail is set to TRUE if a packet with next seqnumber is found in rx_list_pend
 */
static INLINE void BCMFASTPATH
__phyrxs_find_last_mpdu(wlc_info_t * wlc, rx_list_t *rx_list_pend,
	rx_list_t *rx_list_iter, rx_list_t *rx_list_release, bool *rxh_next_seqnum_avail)
{
	d11rxhdr_t *rxh;
	void *cur_pkt;
	uint16 rxh_seqnum, prev_rxh_seqnum = 0;

	while (RX_LIST_RX_HEAD(rx_list_pend) != NULL) {

		cur_pkt = RX_LIST_DELETE_HEAD(rx_list_pend);
		rxh = (d11rxhdr_t *)PKTDATA(wlc->osh, cur_pkt);

		if (!IS_D11RXHDRSHORT(rxh, wlc->pub->corerev)) {

			rxh_seqnum = D11RXHDR_GE129_SEQNUM(rxh);
			ASSERT(rxh_seqnum < PHYRXS_SEQNUM_MAX);

			/* Break out on sequence jump, PhyRx Status should be present */
			if ((RX_LIST_RX_HEAD(rx_list_iter) != NULL) &&
				(rxh_seqnum != prev_rxh_seqnum)) {
				RX_LIST_INSERT_HEAD(rx_list_pend, cur_pkt);
				*rxh_next_seqnum_avail = TRUE;

				WL_INFORM(("wl%d: Next rxh_seqnum[%d] is available\n",
					wlc->pub->unit, rxh_seqnum));
				break;
			}

			prev_rxh_seqnum = rxh_seqnum;

			/* Append MPDU (iter_list) to release list */
			if (RX_LIST_RX_HEAD(rx_list_iter) != NULL) {
				RX_LIST_APPEND(rx_list_release, rx_list_iter);
				RX_LIST_INIT(rx_list_iter); /* rx_head = rx_tail = NULL */
			}
		}

		/* Update iter list */
		RX_LIST_INSERT_TAIL(rx_list_iter, cur_pkt);

	} /* while (RX_LIST_RX_HEAD(rx_list_pend) != NULL) */

} /* __phyrxs_find_last_mpdu() */

/*
 * ------------------------------------------------------------------------------------------------
 * wlc_sts_xfer_bmac_recv():
 *   - Three Rx lists are maintained:
 *     1. rx_list_pend: A list of Rx Packets to be linked with PhyRx Status. (Global scope)
 *     2. rx_list_release: A list of Rx Packets ready for WLAN Rx processing in current DPC call.
 *     3. rx_list_iter: Used for iteration of rx_list_pend
 *
 *   On arrival of Rx Packets, new packets are transferred to a Rx Pending list (rx_list_pend) and
 *   SW write index of ring is updated.
 *   If corresponding PhyRx Status is available then PhyRx Status is linked to the Last MPDU of an
 *   AMPDU and move the packets to the rx_list_release.
 *   if PhyRx status or all MPDU's of an AMPDU are not yet recieved then last MPDU with same
 *   sequence number will be held in RX_LIST_PEND and will be processed in next oppurtunity (DPC).
 * ------------------------------------------------------------------------------------------------
 */
void BCMFASTPATH
wlc_sts_xfer_bmac_recv(wlc_info_t *wlc, uint fifo, rx_list_t *rx_list_release)
{
	wlc_sts_xfer_t		*sts_xfer;
	sts_xfer_phyrxs_t	*phyrxs;
	rx_list_t		*rx_list_pend;	/**< list of 'pending' rx packets */
	rx_list_t		rx_list_iter;	/**< iter list; Declared on stack */
	d11rxhdr_t		*rxh;
	void			*cur_pkt;
	uint16			rxh_seqnum;
	bool			rxh_next_seqnum_avail;
	uint8			idx = 0;

	WL_TRACE(("wl%d: %s: ENTER \n", wlc->pub->unit, __FUNCTION__));

	sts_xfer = WLC_STS_XFER(wlc->sts_xfer_info);
	phyrxs = WLC_STS_XFER_PHYRXS(sts_xfer);

#if defined(BCMSPLITRX)
	idx = ((fifo == RX_FIFO2) ? 1 : 0);
#endif /* BCMSPLITRX */

	rx_list_pend = RX_LIST_PEND(phyrxs, idx);

	/* Append newly received packets to pendling list */
	if (RX_LIST_RX_HEAD(rx_list_release) != NULL) {
		RX_LIST_APPEND(rx_list_pend, rx_list_release);
		/* Don't reset rx_list_t::rxfifocnt */
		RX_LIST_RESET(rx_list_release); /* rx_head = rx_tail = NULL */
	}

	/* Update PhyRx Status ring write index to latest */
	__phyrxs_ring_write_update(sts_xfer, phyrxs);

	while (RX_LIST_RX_HEAD(rx_list_pend) != NULL) {

		/* Peek into head packet; CAUTION: Not removing from list */
		cur_pkt = RX_LIST_RX_HEAD(rx_list_pend);

		/* MAC/uCode RXHDR */
		rxh = (d11rxhdr_t *)PKTDATA(wlc->osh, cur_pkt);

		if (!IS_D11RXHDRSHORT(rxh, wlc->pub->corerev)) {
			rxh_next_seqnum_avail = FALSE;
			RX_LIST_INIT(&rx_list_iter); /* rx_head = rx_tail = NULL */

			rxh_seqnum = D11RXHDR_GE129_SEQNUM(rxh);
			ASSERT(rxh_seqnum < PHYRXS_SEQNUM_MAX);

			__phyrxs_find_last_mpdu(wlc, rx_list_pend, &rx_list_iter,
				rx_list_release, &rxh_next_seqnum_avail);

			cur_pkt = RX_LIST_RX_HEAD(&rx_list_iter);
			ASSERT(cur_pkt != NULL);

			if (rxh_seqnum != phyrxs->cur_seqnum) {
				phyrxs->cur_seqnum = rxh_seqnum;
			}

			/* XXX: Seqnum might have already consumed by previous MPDU of AMPDU.
			 * We hit this condition when LAST MPDU DMA is not complemented but
			 * PhyRx Status transfer/dma is completed and wridx is updated.
			 */
			/* Consume new PhyRx Status */
			if ((phyrxs->cons_seqnum == rxh_seqnum) ||
				(__phyrxs_seqnum_avail(phyrxs, rxh_seqnum) &&
					__phyrxs_consume_d11phyrxsts(wlc, phyrxs, cur_pkt))) {

				/* Update release list */
				RX_LIST_APPEND(rx_list_release, &rx_list_iter);
				RX_LIST_INIT(&rx_list_iter); // rx_head = rx_tail = NULL
				continue;

			} else if (rxh_next_seqnum_avail ||
				__phyrxs_seqnum_avail(phyrxs, rxh_seqnum)) {
				d11phyrxsts_t	*d11phyrxsts;
				sts_xfer_ring_t *phyrxs_ring;
				uint16		seqidx;

				/* Missing PhyRx Status */
				if (rxh_next_seqnum_avail)
					STS_XFER_STATS_INCR(PHYRX_STATS(phyrxs), miss);

				phyrxs_ring = &phyrxs->ring;
				seqidx = rxh_seqnum & (uint16)(phyrxs_ring->depth -1);
				d11phyrxsts = STS_XFER_RING_ELEM(d11phyrxsts_t,
					phyrxs_ring, seqidx);

				BCM_REFERENCE(seqidx);
				BCM_REFERENCE(d11phyrxsts);

				WL_ERROR(("%s: Missing Phy Rx Status for rxh_seqnum[%d]\n",
					__FUNCTION__, rxh_seqnum));
				WL_ERROR(("next_seqnum_avail[%d] D11PHYRXSTS_SEQNUM[%d]\n",
					rxh_next_seqnum_avail,
					PHYRXS_D11PHYRXSTS_SEQNUM(d11phyrxsts)));
				WL_ERROR(("seqidx[%d] rdidx[%d] wridx[%d]\n", seqidx,
					STS_XFER_RING_STATE(phyrxs_ring)->read,
					STS_XFER_RING_STATE(phyrxs_ring)->write));

				WL_ERROR(("rxswrst_cnt[%u] prxsfull_cnt[%u]\n",
					wlc_read_shm(wlc, M_RXSWRST_CNT(wlc)),
					wlc_read_shm(wlc, M_PHYRXSFULL_CNT(wlc))));

				WL_ERROR(("STS_FIFO: dma_rxactive[%d]\n",
					dma_rxactive(wlc->hw->di[STS_FIFO])));

#if defined(BCMDBG)
				{
					struct bcmstrbuf b;
					const int	dump_size = 8192;
					char		*buf = MALLOC(wlc->osh, dump_size);

					if (buf != NULL) {
						bcm_binit((void *)&b, buf, dump_size);
						wlc_sts_xfer_dump(sts_xfer, &b);
						WL_ERROR(("%s \n", b.origbuf));
						MFREE(wlc->osh, buf, dump_size);
					}
				}
#endif // endif

				ASSERT(0);

			} else {
				/**
				 * PhyRx Status is not yet available
				 * Move packets from iter list to head of pending list and
				 * hold pkt until sts-fifo intr
				 */
				RX_LIST_PREPEND(rx_list_pend, &rx_list_iter);
				RX_LIST_INIT(&rx_list_iter);
				break;
			}

		} else { /* ! IS_D11RXHDRSHORT() */

			cur_pkt = RX_LIST_DELETE_HEAD(rx_list_pend);
			/* Update release list */
			RX_LIST_INSERT_TAIL(rx_list_release, cur_pkt);
		}

	} /* while (RX_LIST_RX_HEAD(rx_list_pend) != NULL) */

#if defined(DONGLEBUILD)
	/* TODO: Validate dma_rx: bad frame length for RX_FIFO2 frames */
	ASSERT(0);
#endif /* DONGLEBUILD */
} /* wlc_sts_xfer_bmac_recv */

/**
 * ------------------------------------------------------------------------------------------------
 * wlc_sts_xfer_bmac_done:-
 * - Invoked at end of wlc_bmac_recv()
 * - Move read index to the next index of last consumed PhyRx Status buffer and update
 *   corresponding DMA attributes (dma_info_t::rxin or M2M DMA wridx).
 *
 * CAUTION: WLAN driver should not hold any PhyRx Status buffer at end of wlc_bmac_recv().
 * ------------------------------------------------------------------------------------------------
 */
void BCMFASTPATH
wlc_sts_xfer_bmac_recv_done(wlc_info_t *wlc, uint fifo)
{
	wlc_sts_xfer_t		*sts_xfer;
	sts_xfer_phyrxs_t	*phyrxs;
	uint16 rdidx;

	WL_TRACE(("wl%d: %s: ENTER \n", wlc->pub->unit, __FUNCTION__));

	sts_xfer = WLC_STS_XFER(wlc->sts_xfer_info);
	phyrxs = WLC_STS_XFER_PHYRXS(sts_xfer);

	/* Move phyrxs bcm_ring_t::read of to the next index of last consumed buffer */
	rdidx = (phyrxs->cons_seqnum + 1) & (uint16)(phyrxs->ring.depth -1);
	STS_XFER_RING_STATE(&phyrxs->ring)->read = rdidx;

	WL_INFORM(("wl%d: %s: updated rdidx[%d]\n", wlc->pub->unit, __FUNCTION__, rdidx));

	if (STS_XFER_PHYRXS_FIFO_ENAB(sts_xfer->wlc->pub)) {

		/* Move rxin and refill status buffers for STS_FIFO dma */
		dma_sts_rx_done(wlc->hw->di[STS_FIFO], rdidx);
		wlc_bmac_dma_rxfill(sts_xfer->wlc->hw, STS_FIFO);

	} else { /* STS_XFER_PHYRXS_M2M */

		volatile m2m_status_eng_regs_t *status_eng_regs;
		uint32	saved_core_id;
		uint32	saved_intr_val;
		uint32	v32; /* used in REG read/write */

		ASSERT(STS_XFER_PHYRXS_M2M_ENAB(sts_xfer->wlc->pub));
		status_eng_regs = phyrxs->phyrxs_regs;

		STS_XFER_SWITCHCORE(sts_xfer->sih, &saved_core_id, &saved_intr_val);

		/* Update M2M status engine read index */
		v32 = R_REG(wlc->osh, &status_eng_regs->rdidx);
		v32 = BCM_CBF(v32, M2M_STATUS_ENG_RDIDX_QUEUE_RDIDX);
		v32 |= BCM_SBF(rdidx, M2M_STATUS_ENG_RDIDX_QUEUE_RDIDX);
		W_REG(wlc->osh, &status_eng_regs->rdidx, v32);

		STS_XFER_RESTORECORE(sts_xfer->sih, saved_core_id, saved_intr_val);
	}

} /* wlc_sts_xfer_bmac_recv_done() */

/** Returns TRUE if Rx packets are pending with PhyRx Status to be received */
bool BCMFASTPATH
wlc_sts_xfer_phyrxs_rxpend(wlc_info_t *wlc, uint fifo)
{
	wlc_sts_xfer_t		*sts_xfer;
	sts_xfer_phyrxs_t	*phyrxs;
	uint8			idx = 0;

	sts_xfer = WLC_STS_XFER(wlc->sts_xfer_info);
	phyrxs = WLC_STS_XFER_PHYRXS(sts_xfer);

#if defined(BCMSPLITRX)
	idx = ((fifo == RX_FIFO2) ? 1 : 0);
#endif /* BCMSPLITRX */

	if (RX_LIST_RX_HEAD(RX_LIST_PEND(phyrxs, idx)) != NULL)
		return TRUE;

	return FALSE;
} /* wlc_sts_xfer_phyrxs_rxpend() */

/**
 * wlc_sts_xfer_phyrxs_release:
 * It will walk through the AMSDU packet chain and PhyRx Status buffer will be delinked from
 * Rx Packet.
 */
void BCMFASTPATH
wlc_sts_xfer_phyrxs_release(wlc_info_t *wlc, void *pkt)
{
	wlc_sts_xfer_t		*sts_xfer;
	sts_xfer_phyrxs_t	*phyrxs;
	sts_xfer_ring_t		*phyrxs_ring;
	uint16			phyrxs_seqnum;
	uint16			rdidx, seqidx;

	sts_xfer = WLC_STS_XFER(wlc->sts_xfer_info);
	phyrxs = WLC_STS_XFER_PHYRXS(sts_xfer);
	phyrxs_ring = &phyrxs->ring;

	while (pkt != NULL) {

		/* Check if PhyRx Status buffer is linked to Rx packet */
		if ((WLPKTTAG(pkt)->phyrxs_seqid == (uint16)STS_XFER_PHYRXS_SEQID_INVALID) ||
			(WLPKTTAG(pkt)->phyrxs_seqid == 0)) {
			pkt = PKTNEXT(wlc->osh, pkt);
			continue;
		}

		ASSERT(bcm_ring_is_empty(STS_XFER_RING_STATE(phyrxs_ring)) == FALSE);

		phyrxs_seqnum = PHYRXS_PKTTAG_SEQID_2_SEQNUM(WLPKTTAG(pkt)->phyrxs_seqid);
		rdidx = STS_XFER_RING_STATE(phyrxs_ring)->read;
		seqidx = (phyrxs->cons_seqnum + 1) & (uint16)(phyrxs_ring->depth -1);

		WL_INFORM(("wl%d: %s: PhyRx Status with seqnum[%d] is delinked from pkt[%p]\n",
			wlc->pub->unit, __FUNCTION__, phyrxs_seqnum, pkt));

		/* Check for stale PhyRx Status buffer */
		if (!_phyrxs_seqnum_valid(phyrxs_seqnum, phyrxs_ring->depth, rdidx, seqidx)) {

			WL_ERROR(("%s: seqnum[%d] packet is hold after wlc_bmac_recv done\n",
				__FUNCTION__, phyrxs_seqnum));
			WL_ERROR(("seqidx[%x] rdidx[%x] wridx[%x]\n", seqidx,
				STS_XFER_RING_STATE(phyrxs_ring)->read,
				STS_XFER_RING_STATE(phyrxs_ring)->write));

#if defined(BCMDBG)
			{
				struct bcmstrbuf b;
				const int	dump_size = 8192;
				char		*buf = MALLOC(wlc->osh, dump_size);

				if (buf != NULL) {
					bcm_binit((void *)&b, buf, dump_size);
					wlc_sts_xfer_dump(sts_xfer, &b);
					WL_ERROR(("%s \n", b.origbuf));
					MFREE(wlc->osh, buf, dump_size);
				}
			}
#endif // endif
		}

		STS_XFER_STATS_INCR(PHYRX_STATS(phyrxs), release);

		/* Delink PhyRx Status buffer */
		WLPKTTAG(pkt)->phyrxs_seqid = STS_XFER_PHYRXS_SEQID_INVALID;
		pkt = PKTNEXT(wlc->osh, pkt);
	}
} /* wlc_sts_xfer_phyrxs_release() */

/** PKTFREE register callback invoked on PKTFREE of Rx packet chain */
void
wlc_sts_xfer_phyrxs_free(wlc_info_t *wlc, void *pkt)
{
	/* Walk through PKTC/PKTNEXT and release PhyRx Status */
#if defined(PKTC) || defined(PKTC_DONGLE)
	while (pkt != NULL) {
		wlc_sts_xfer_phyrxs_release(wlc, pkt);
		pkt = PKTCLINK(pkt);
	}
#else /* !(PKTC || PKTC_DONGLE) */

	ASSERT(PKTLINK(pkt) == NULL);
	wlc_sts_xfer_phyrxs_release(wlc, pkt);
#endif /* !(PKTC || PKTC_DONGLE) */

} /* wlc_sts_xfer_phyrxs_free() */

#if defined(STS_XFER_M2M_INTR)

/**
 * ================================================================================================
 * Section: M2M interrupt support
 *
 * Register settings to enable interrupts on Tx/PhyRx Status WR index update.
 * - Set bit 30,31 in M2MCORE IntControl/Status registers to enable Tx/PhyRx Status interrupts.
 * - Set Lazycount (M2M_STATUS_ENG_CTL_LAZYCOUNT) to 1 in M2M TXS/PHYRXS control registers.
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
 * PhyRx Status WR index update interrupt:
 *	Triggering WLAN Rx Processing (MI_DMAINT) on PhyRx Status arrival (dummy or valid) as
 *	opposed	to a Data/Mgmt packet arrival on RxFIFO0. By triggering RxProcessing on a
 *	PhyRx Status arrival, we can leverage an entire AMPDUs worth of packets readily available
 *	in a DPC run. MAC (Ucode) will post a dummy PhyRx Status in case the PhyRx Status had not
 *	arrived from PHY to MAC, rather than simply dropping (not posting) with skipped seqnum.
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
wlc_sts_xfer_irqname(wlc_info_t *wlc, void *btparam)
{
	wlc_sts_xfer_t		*sts_xfer;
	sts_xfer_m2m_intr_t	*m2m_intr;

	sts_xfer = WLC_STS_XFER(wlc->sts_xfer_info);
	m2m_intr = WLC_STS_XFER_M2M_INTR(sts_xfer);

	BCM_REFERENCE(btparam);

#if defined(CONFIG_BCM_WLAN_DPDCTL)
	if (btparam != NULL) {
		/* bustype = PCI, even embedded 2x2AX devices have virtual pci underneath */
		snprintf(m2m_intr->irqname, sizeof(m2m_intr->irqname),
			"wlpcie:%s, wlan_%d_m2m", pci_name(btparam), wlc->pub->unit);
	} else
#endif /* CONFIG_BCM_WLAN_DPDCTL */
	{
		snprintf(m2m_intr->irqname, sizeof(m2m_intr->irqname), "wlan_%d_m2m",
			wlc->pub->unit);
	}

	return m2m_intr->irqname;
} /* wlc_sts_xfer_irqname() */

/* Returns si_flag of M2M core */
uint
wlc_sts_xfer_m2m_si_flag(wlc_info_t *wlc)
{
	wlc_sts_xfer_t		*sts_xfer;

	sts_xfer = WLC_STS_XFER(wlc->sts_xfer_info);
	return sts_xfer->m2m_core_si_flag;
}

/** Reads and clear status of M2M interrupts set in sts_xfer_m2m_intr_t::intcontrol */
static uint32 BCMFASTPATH
wlc_sts_xfer_m2m_intstatus(wlc_info_t *wlc)
{
	volatile m2m_core_regs_t *m2m_core_regs;
	wlc_sts_xfer_t		*sts_xfer;
	sts_xfer_m2m_intr_t	*m2m_intr;
	uint32			intstatus;

	sts_xfer = WLC_STS_XFER(wlc->sts_xfer_info);
	m2m_intr = WLC_STS_XFER_M2M_INTR(sts_xfer);
	m2m_core_regs = sts_xfer->m2m_core_regs;

	/* Detect cardbus removed, in power down(suspend) and in reset */
	if (DEVICEREMOVED(wlc))
		return -1;

	intstatus = R_REG(wlc->osh, &m2m_core_regs->intstatus);
	intstatus &= m2m_intr->defintmask;

	if (intstatus) {
		/* Clear interrupt status */
		W_REG(wlc->osh, &m2m_core_regs->intstatus, intstatus);

		/* M2M interrupts are enalbed only for PhyRx Status transfer */
		ASSERT(STS_XFER_PHYRXS_M2M_ENAB(wlc->pub));
		if (BCM_GBF(intstatus, M2M_CORE_INTCONTROL_PHYRXSWRINDUPD_INTMASK)) {
			wlc_intr_process_rxfifo_interrupts(wlc);
		}
	}

	return intstatus;
} /* wlc_sts_xfer_m2m_intstatus() */

/**
 * Function:	wlc_sts_xfer_isr()
 * Description:	IRQ handler for M2M interrupts.
 *		Interrupt status is copied to software variable sts_xfer_m2m_intr_t::intstatus
 *		all M2M interrupts are disabled.
 *		A DPC will be scheduled and Tx/PhyRx Status packages are extracted in DPC.
 */
bool BCMFASTPATH
wlc_sts_xfer_isr(wlc_info_t *wlc, bool *wantdpc)
{
	wlc_sts_xfer_t		*sts_xfer;
	sts_xfer_m2m_intr_t	*m2m_intr;
	uint32			intstatus;

	sts_xfer = WLC_STS_XFER(wlc->sts_xfer_info);
	m2m_intr = WLC_STS_XFER_M2M_INTR(sts_xfer);

	WL_TRACE(("wl%d: %s: ENTER\n", wlc->pub->unit, __FUNCTION__));

	if (wl_powercycle_inprogress(wlc->wl)) {
		return FALSE;
	}

	if (m2m_intr->intcontrol == 0)
		return FALSE;

	/* Read and clear M2M intstatus register */
	intstatus = wlc_sts_xfer_m2m_intstatus(wlc);

	if (intstatus == 0xffffffff) {
		WL_HEALTH_LOG(wlc, DEADCHIP_ERROR);
		WL_ERROR(("DEVICEREMOVED detected in the ISR code path.\n"));
		/* In rare cases, we may reach this condition as a race condition may occur
		 * between disabling interrupts and clearing the SW macintmask.
		 * Clear M2M int status as there is no valid interrupt for us.
		 */
		m2m_intr->intstatus = 0;
		/* assume this is our interrupt as before; note: want_dpc is FALSE */
		return TRUE;
	}

	/* It is not for us */
	if (intstatus == 0) {
		return FALSE;
	}

	/* Save interrupt status bits */
	m2m_intr->intstatus = intstatus;

	WL_INFORM(("wl%d: %s: M2M core instatus 0x%x \n", wlc->pub->unit, __FUNCTION__, intstatus));

	/* Turn off interrupts and schedule DPC */
	*wantdpc |= TRUE;

	return TRUE;
} /* wlc_sts_xfer_isr() */

void
wlc_sts_xfer_intrson(wlc_info_t *wlc)
{
	volatile m2m_core_regs_t *m2m_core_regs;
	wlc_sts_xfer_t		*sts_xfer;
	sts_xfer_m2m_intr_t	*m2m_intr;

	sts_xfer = WLC_STS_XFER(wlc->sts_xfer_info);
	m2m_intr = WLC_STS_XFER_M2M_INTR(sts_xfer);
	m2m_core_regs = sts_xfer->m2m_core_regs;

	WL_TRACE(("wl%d: %s: ENTER\n", wlc->pub->unit, __FUNCTION__));

	m2m_intr->intcontrol = m2m_intr->defintmask;
	/* Enable M2M interrupts */
	W_REG(wlc->osh, &m2m_core_regs->intcontrol, m2m_intr->intcontrol);

} /* wlc_sts_xfer_intrson() */

void
wlc_sts_xfer_intrsoff(wlc_info_t *wlc)
{
	volatile m2m_core_regs_t *m2m_core_regs;
	wlc_sts_xfer_t		*sts_xfer;
	sts_xfer_m2m_intr_t	*m2m_intr;

	sts_xfer = WLC_STS_XFER(wlc->sts_xfer_info);
	m2m_intr = WLC_STS_XFER_M2M_INTR(sts_xfer);
	m2m_core_regs = sts_xfer->m2m_core_regs;

	WL_TRACE(("wl%d: %s: ENTER\n", wlc->pub->unit, __FUNCTION__));

	/* Disable M2M interrupts */
	W_REG(wlc->osh, &m2m_core_regs->intcontrol, 0);
	m2m_intr->intcontrol = 0;

} /* wlc_sts_xfer_intrsoff() */

void
wlc_sts_xfer_intrsrestore(wlc_info_t *wlc, uint32 macintmask)
{
	WL_TRACE(("wl%d: %s: ENTER\n", wlc->pub->unit, __FUNCTION__));

	if (macintmask) {
		wlc_sts_xfer_intrson(wlc);
	} else {
		wlc_sts_xfer_intrsoff(wlc);
	}
} /* wlc_sts_xfer_intrsrestore() */

/**
 * Called by DPC when the DPC is rescheduled, updates sts_xfer_m2m_intr_t->intstatus
 *
 * Prerequisite:
 * - Caller should have acquired a spinlock against isr concurrency, or guarantee that interrupts
 *   have been disabled.
 */
void BCMFASTPATH
wlc_sts_xfer_intrsupd(wlc_info_t *wlc)
{
	wlc_sts_xfer_t		*sts_xfer;
	sts_xfer_m2m_intr_t	*m2m_intr;
	uint32			intstatus;

	sts_xfer = WLC_STS_XFER(wlc->sts_xfer_info);
	m2m_intr = WLC_STS_XFER_M2M_INTR(sts_xfer);

	ASSERT(m2m_intr->intcontrol == 0);
	intstatus = wlc_sts_xfer_m2m_intstatus(wlc);

	/* Device is removed */
	if (intstatus == 0xffffffff) {
		WL_HEALTH_LOG(wlc, DEADCHIP_ERROR);
		return;
	}

	/* Update interrupt status in software */
	m2m_intr->intstatus |= intstatus;

} /* wlc_sts_xfer_intrsupd() */

void BCMFASTPATH
wlc_sts_xfer_process_m2m_intstatus(wlc_info_t *wlc)
{
	wlc_sts_xfer_t		*sts_xfer;
	sts_xfer_m2m_intr_t	*m2m_intr;
	uint32			intstatus;

	sts_xfer = WLC_STS_XFER(wlc->sts_xfer_info);
	m2m_intr = WLC_STS_XFER_M2M_INTR(sts_xfer);

	WL_TRACE(("wl%d: %s: ENTER\n", wlc->pub->unit, __FUNCTION__));

	if (m2m_intr->intstatus == 0)
		return;

	intstatus = m2m_intr->intstatus;

	WL_INFORM(("wl%d: %s: intstatus[%x]\n", wlc->pub->unit, __FUNCTION__, intstatus));

	if (BCM_GBF(intstatus, M2M_CORE_INTCONTROL_PHYRXSWRINDUPD_INTMASK)) {
		/* M2M interrupts are enalbed only for PhyRx Status transfer */
		ASSERT(STS_XFER_PHYRXS_M2M_ENAB(wlc->pub));

		/* M2M/BME DMA transferred PhyRx Status to memory */
		intstatus = BCM_CBF(intstatus, M2M_CORE_INTCONTROL_PHYRXSWRINDUPD_INTMASK);
		wlc->hw->macintstatus |= MI_DMAINT;
	}

	ASSERT(intstatus == 0);
	m2m_intr->intstatus = intstatus;

} /* wlc_sts_xfer_process_m2m_intstatus() */

#endif /* STS_XFER_M2M_INTR */
