/*
 * -----------------------------------------------------------------------------
 * Generic Broadcom Home Networking Division (HND) M2M module.
 *
 * M2MCORE (m2mdma_core.h) may instantiate descriptor based mem2mem DMA engines
 * (DMA Channels #0 and #1), in addition to the Byte Move Engines (hndbme.[hc]).
 * Notation: M2M_DD prefix is used for 'D'escriptor based 'D'MA subsystem.
 *
 * hndbme.[hc] provide a synchronous (poll for DONE) HW assisted memcpy API.
 * hndm2m.[hc] provides an asynchronous HW assisted mem2mem copy API, with an
 * explicit DMA Done Interrupt driven callback to resume an application's
 * processing.
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
 * -----------------------------------------------------------------------------
 */

/**
 * +--------------------------------------------------------------------------+
 *  XXX
 *
 *  BME = Byte Move Engine (synchronous paradigm, using poll DMA Done)
 *  DD  = DMA Descriptor (asynchronous paradigm, using DMA Done INT)
 *  *#* = NIC Mode chipset. DD based driver is not supported.
 *        Need a dedicated PCIe BAR for M2MCORE INT register access in ISR.
 *
 *  CHIPSET REV DD  BME MAC DESCRIPTION
 *  ------- --- --  --- --- --------------------------------------------------
 *  43684A0   2  2   +2     BME(Ch#2,#3)
 *  43684Bx   3  2   +2     BME(Ch#2,#3)
 *  63178A0 128 *2*  +1  +1 BME(Ch#2)        MAC:TxS,PhyRxS(Ch#3)
 *   6710A0 129 *2*  +1  +1 BME(Ch#2)        MAC:TxS,PhyRxS(Ch#3)
 *  43684C0 130  2   +2     BME(Ch#2,#3)
 *   6715A0 131  2   +6  +2 BME(ch#2,4,5,6)  MAC:TxS,PhyRxS(Ch#3)  MAC(Ch#7)
 *   6756A0 132 *2*  +1  +1 BME(Ch#2)        MAC:TxS,PhyRxS(Ch#3)
 *
 *
 *  HW WARs
 *  -------
 *  Rev < 131: HW43684-519 WAR: Burstlen must be reduced to 32 Bytes for SVMP
 *             Access. Applications CSI(M2M_DD_CSI) and AirIQ(M2M_DD_AIQ) use
 *             M2M_DD_CH0 for D2H i.e. SVMP to DDR transfers.
 *
 *  New Features:
 *  -------------
 *  Rev >= 130: CRWLDMA-168: Per descriptor WaitForComplete for D2H DD Channel.
 *
 * +--------------------------------------------------------------------------+
 */

#include <osl.h>
#include <bcmdefs.h>
#include <bcmutils.h>
#include <bcm_ring.h>
#include <hndsoc.h>
#include <siutils.h>
#if defined(DONGLEBUILD)
#include <rte_isr.h>
#endif // endif
#include <pcicfg.h>
#include <sbhnddma.h>               /* dma64regs_t, dma64dd_t */
#include <m2mdma_core.h>            /* m2m_core_regs_t, m2m_eng_regs_t */
#include <bcmendian.h>              /* haddr64 ltoh */
#include <bcmpcie.h>                /* m2m link to host info in pcie_ipc */
#include <hndm2m.h>

struct m2m_sys;                     /* m2m core driver system state */

/**
 * +--------------------------------------------------------------------------+
 * Section: Conditional Compile
 * Designer builds for extended debug, assert and statistics.
 * +--------------------------------------------------------------------------+
 */

#if ((defined STB) || (defined STBAP))
/* Handle the arguments difference of the osl_cache_flush() function */
#define	M2M_OSL_CACHE_FLUSH(OSH, VA, SIZE) \
	(osl_cache_flush(OSH, (void *)VA, SIZE))
#else /* ! (STB || STBAP) */
#define	M2M_OSL_CACHE_FLUSH(OSH, VA, SIZE) \
	({ BCM_REFERENCE(OSH); OSL_CACHE_FLUSH((void*)(VA), SIZE); })
#endif /* ! (STB || STBAP) */

//#define M2M_DEBUG_BUILD
#define M2M_STATS_BUILD
#define M2M_RDCLR_BUILD             /* Counters: Clear On Read */

#define M2M_NOOP                    do { /* no-op */ } while(0)
#define M2M_PRINT                   printf
#define M2M_ERROR                   printf

#if defined(M2M_DEBUG_BUILD)
#define M2M_DEBUG(expr)             expr
#define M2M_ASSERT(expr)            ASSERT(expr)
#else  /* ! M2M_DEBUG_BUILD */
#define M2M_DEBUG(expr)             M2M_NOOP
#define M2M_ASSERT(expr)            M2M_NOOP
#endif /* ! M2M_DEBUG_BUILD */

/* M2M system */
#if defined(DONGLEBUILD)
#define	M2M_SYS(osh) ({ BCM_REFERENCE(osh); m2m_sys_gp; })
#else /* ! DONGLEBUILD */
/* For NIC mode, get the M2M service handle from OSH */
#define M2M_SYS(osh) OSH_GETM2M(osh)
#endif /* ! DONGLEBUILD */

/* The dongle needs to use PCIe to do the M2M transfer to/from host DDR.
 * Whereas in NIC mode, the non-SoC radio chips need to use PCIe.
 * For these transfers, the host DDR address needs to have the 63rd bit set.
 */
#if defined(DONGLEBUILD)
#define M2M_USE_PCI64ADDR(m2m) (1)
#else
#define M2M_USE_PCI64ADDR(m2m) ((m2m) && (m2m->core_rev == 3 || \
	m2m->core_rev == 129 || m2m->core_rev == 130 || \
	m2m->core_rev == 131))
#endif // endif

#define M2M_PCI64ADDR_HIGH          0x80000000  /* PCI64ADDR_HIGH */
#define M2M_DD_D64_RS0_CD_MASK      0x0000FFFF
#define M2M_DD_D64_RS1_AD_MASK      0x0000FFFF
#define M2M_DD_D64_XS0_CD_MASK      0x0000FFFF
#define M2M_DD_D64_XS1_AD_MASK      0x0000FFFF

/**
 * +--------------------------------------------------------------------------+
 * Section: M2M Statistics
 * +--------------------------------------------------------------------------+
 */
#if defined(M2M_STATS_BUILD)
#define M2M_STATS(expr)             expr
#define M2M_STATS_ADD(L, R, name)   (L).name += (R).name
#else  /* ! M2M_STATS_BUILD */
#define M2M_STATS(expr)
#define M2M_STATS_ADD(L, R, name)   M2M_NOOP
#endif /* ! M2M_STATS_BUILD */

#if defined(M2M_RDCLR_BUILD)
#define M2M_CTR_ZERO(ctr)           (ctr) = 0U
#else
#define M2M_CTR_ZERO(ctr)           M2M_NOOP
#endif // endif

#define M2M_STATS_ZERO(S)           \
({  M2M_CTR_ZERO((S).bytes);        \
    M2M_CTR_ZERO((S).xfers);        \
    M2M_CTR_ZERO((S).cbfns);        \
    M2M_CTR_ZERO((S).fails);        \
})

#define M2M_STATS_ACCUM(L, R)       \
({  M2M_STATS_ADD(L, R, bytes);     \
    M2M_STATS_ADD(L, R, xfers);     \
    M2M_STATS_ADD(L, R, cbfns);     \
    M2M_STATS_ADD(L, R, fails);     \
})

#define M2M_STATS_FMT               "%12d %12d %12d %12d "
#define M2M_STATS_VAL(S)            (S).bytes, (S).xfers, (S).cbfns, (S).fails

typedef struct m2m_stats {
	uint32      bytes;                      // bytes transferred
	uint32      xfers;                      // transfers initiated
	uint32      cbfns;                      // callbacks invoked
	uint32      fails;                      // failures: no resource or other
} m2m_stats_t;

/**
 * +--------------------------------------------------------------------------+
 * Section: M2M DD Interrupt Handling
 *
 * M2M Core manages several DMA Processors, descriptor based, non-descriptor
 * base (i.e. BME), MAC managed for TxStatus/PhyRxStatus, and PSM.
 *
 * Each DMA Processor may need to enable interrupts, in which case,
 * a secondary ISR/DPC handler mechanism is needed.
 *
 * Descriptor based M2M enables the RcvInt per DMA channel. XmtInt is ignored.
 * See RcvCtrl WaitForComplete (D64_RC_WC) for DMA Done Interrupt generation.
 * LazyRcv Interrupt is not enabled.
 * +--------------------------------------------------------------------------+
 */

/** M2M Core per Engine Error Interrupt Mask: (DE | DA | DP | RU) */
#define M2M_CORE_CH_ERR_INTMASK           \
	(M2M_CORE_CH_DE_MASK /* DescErr */   | \
	 M2M_CORE_CH_DA_MASK /* DataErr */   | \
	 M2M_CORE_CH_DP_MASK /* DescProto */ | \
	 M2M_CORE_CH_RU_MASK /* RcvDescUf */)

/** M2M DD Interrupt Mask: RcvInt (RI) + Errors (DE | DA | DP | RU) */
/*  XI is intently disabled */
#define M2M_DD_INTMASK	        (M2M_CORE_CH_RI_MASK | M2M_CORE_CH_ERR_INTMASK)

/**
 * +--------------------------------------------------------------------------+
 * Section: M2M Descriptor based DMA Processor and Driver configuration
 *
 * - Ring repth: power of 2, and depth of 256
 *
 * XmtCtrl and RcvCtrl Configuration
 * XC,RC: XmtEn(XE),RcvEn(RE)
 *        PtyChkDisable(PD),
 *        BurstLen(BL), PrefetchControl(PC), PrefetchThreshold(PT),
 *        Coherent(C)
 * - BurstLen = 2 i.e 64 Bytes.
 * - PrefetchControl = 1 (prefetch upto 4 descriptors)
 * - PreftechThreshold = 1 (initiate prefetching of PC descr, when descr < 2)
 *
 * DD also enables the WaitForComplete(WC) in RcvCtrl. In later revisions of the
 * M2M core, control over WC is enabled on a per descriptor basis (CRWLDMA-168).
 * WC-per-descr is an optimization to reduce readbacks, when M2M transaction is
 * constituted of multiple DMA copies, and a readback is only required for the
 * last DMA transfer, for DMA Done interrupt generation.
 *
 * +--------------------------------------------------------------------------+
 */

#if !defined(M2M_DD_DEPTH)
#define M2M_DD_DEPTH                256         /* DMA descriptor ring depth */
#endif /* !M2M_DD_DEPTH */

/* Given a ring base, return a pointer to the element at a requested index */
#define M2M_DD_ELEM(elem_type, elem_base, elem_idx) \
	(((elem_type*)(elem_base)) + (elem_idx))

#define M2M_DD_RING_PROD_IDX(eng)   ((eng)->ring_state.write)
#define M2M_DD_RING_CONS_IDX(eng)   ((eng)->ring_state.read)

#define M2M_DD_PC                  (DMA_PC_4)   /* Prefetch Control */
#define M2M_DD_PT                  (DMA_PT_2)   /* Prefetch Threshold */

#define M2M_DD_XC_CONFIG(m2m_dd_bl)     \
	(D64_XC_PD | /* Parity Disabled */  \
	 BCM_SBF(m2m_dd_bl, D64_XC_BL) |    \
	 BCM_SBF(M2M_DD_PC, D64_XC_PC) |    \
	 BCM_SBF(M2M_DD_PT, D64_XC_PT) |    \
	 BCM_SBIT(D64_XC_CO))

/* WaitForComplete (readback) in D2H direction, for DMA Done INT */
#define M2M_DD_RC_CONFIG(m2m_dd_bl)     \
	(D64_RC_PD | /* Parity Disabled */  \
	 D64_RC_WC | /* Wait Complete */    \
	 BCM_SBF(m2m_dd_bl, D64_RC_BL) |    \
	 BCM_SBF(M2M_DD_PC, D64_RC_PC) |    \
	 BCM_SBF(M2M_DD_PT, D64_RC_PT) |    \
	 BCM_SBIT(D64_RC_CO))

#if (M2M_DD_USR > 8)
#error "M2M: Maximum 8 Users supported for Descriptor based DMA"
#endif // endif

#define M2M_DD_CPY_LEN_SHIFT        0
#define M2M_DD_CPY_LEN_MASK         (0xffff << M2M_DD_CPY_LEN_SHIFT)
#define M2M_DD_CPY_IDX_SHIFT        16
#define M2M_DD_CPY_IDX_MASK         (0xfff << M2M_DD_CPY_IDX_SHIFT)
#define M2M_DD_CPY_USR_SHIFT        28
#define M2M_DD_CPY_USR_MASK         (0xf << M2M_DD_CPY_USR_SHIFT)

#define M2M_DD_CPY_KEY(len, idx, usr)   \
	(BCM_SBF(len, M2M_DD_CPY_LEN) |     \
	 BCM_SBF(idx, M2M_DD_CPY_IDX) |     \
	 BCM_SBF(usr, M2M_DD_CPY_USR))

/** Fetch the container m2m_dd_usr_t given the usr wake dll node */
#define M2M_DD_WAKE_USR(NODE)       CONTAINEROF((NODE), m2m_dd_usr_t, wake)

/**
 * +--------------------------------------------------------------------------+
 * Section: M2M DD Data Structures
 *
 * Theory of operation:
 * +--------------------------------------------------------------------------+
 */

#define M2M_DD_D64_NULL ((m2m_dd_d64_t*)NULL)
#define M2M_DD_XID_NULL ((m2m_dd_xid_t*)NULL)
#define M2M_DD_ENG_NULL ((m2m_dd_eng_t*)NULL)
#define M2M_DD_USR_NULL ((m2m_dd_usr_t*)NULL)

/**
 * +--------------------------------------------------------------------------+
 * M2M DD descriptor layout (Non Volatile version of dma64dd_t)
 * +--------------------------------------------------------------------------+
 */

typedef struct m2m_dd_d64 { /* non volatile version of dma64dd_t */
	uint32            ctrl1;
	uint32            ctrl2;
	dma64addr_t       addr64;
} m2m_dd_d64_t;

/**
 * +--------------------------------------------------------------------------+
 * M2M DD per transfer request information maintained in a shadow ring.
 * +--------------------------------------------------------------------------+
 */

typedef struct m2m_dd_xid {
	uint8             key;                 // identifies m2m_dd_usr_t::cb
	uint8             xop;                 // transfer op: commit, resume
	uint16            len;                 // length of DMA xfer
	void            * arg;                 // callback parameter
} m2m_dd_xid_t;

/**
 * +--------------------------------------------------------------------------+
 * M2M DD Engine context
 * +--------------------------------------------------------------------------+
 */
typedef struct m2m_dd_eng {
	uint8             idx;                  // engine index
	uint16            xd_idx;               // xfer index in xid ring
	bcm_ring_t        ring_state;           // ring WR and RD indices state
	uintptr           txdd_ptr;             // Next TxDD to produce - XmtPtr
	uintptr           rxdd_ptr;             // Next RxDD to produce - RcvPtr
	uint32            intstatus;            // interrupt status
	uint32            intmask;              // interrupt mask
	dll_t             wake_list;            // users requiring a wake callback
	struct m2m_sys  * m2m_sys;              // backpointer to m2m system state
	m2m_dd_d64_t    * txd_table;            // table of Tx DDs
	m2m_dd_d64_t    * rxd_table;            // table of Rx DDs
#if !defined(DONGLEBUILD)
	uint64           txd_table_pa;         // physical addr of the table of Tx DDs
	uint64           rxd_table_pa;         // physical addr of the table of Rx DDs
#endif /* ! DONGLEBUILD */
	m2m_dd_xid_t    * xid_table;            // table of user xfer requests
	m2m_eng_regs_t  * eng_regs;             // DMA processor registers
} m2m_dd_eng_t;

/**
 * +--------------------------------------------------------------------------+
 * M2M DD User context.
 * Several application may leverage a single DD Channel. When an application
 * registers itself for a mem2mem service, callback state and DMA channel's
 * information is maintained here.
 * +--------------------------------------------------------------------------+
 */
typedef struct m2m_dd_usr {
	dll_t             wake;                 // dll node for user wake list
	m2m_dd_eng_t    * eng;                  // channel's engine context
	void            * cbdata;               // usr_ctx passed to callback
	m2m_dd_done_cb_t  done_cb;              // registered DMA DONE callback
	m2m_dd_wake_cb_t  wake_cb;              // registered User WAKE up callback
	uint16            wake_thr;             // WAKE up DD avail threshold
	M2M_STATS(m2m_stats_t stats);
} m2m_dd_usr_t;

/**
 * +--------------------------------------------------------------------------+
 * M2M DD System Context
 * +--------------------------------------------------------------------------+
 */
typedef struct m2m_sys {
	m2m_dd_eng_t      eng[M2M_DD_ENG];      // descriptor based M2M engines
	m2m_dd_usr_t      usr[M2M_DD_USR];      // registered user contexts
	uint32            intstatus;            // saved core intstatus
	m2m_core_regs_t * core_regs;            // m2m core registers
	uint32            core_id;              // m2m core id
	uint32            core_rev;             // m2m core revision
	osl_t           * osh;                  // OS abstraction layer handler
	si_t            * sih;                  // SoC Interconnect handler
#if defined(DONGLEBUILD)
	uint32            sec_isr_enab;         // map of engines with secondary isr
	hnd_isr_action_t* isr_action;           // primary isr action
#else /* ! DONGLEBUILD */
	uint              si_flag;              // M2M core bitmap in oobselouta30 reg
#endif /* ! DONGLEBUILD */
} m2m_sys_t;

#define M2M_SYS_NULL    ((m2m_sys_t *)NULL)
m2m_sys_t * m2m_sys_gp = M2M_SYS_NULL;

static const char * m2m_usr_str[M2M_DD_USR] =
	{ "SYS", "CSI", "AIQ", "PGO", "PGI", "UN5", "UN6", "UN7" };

/**
 * +--------------------------------------------------------------------------+
 * Section: Private System and DD helper routines
 * +--------------------------------------------------------------------------+
 */
static INLINE m2m_dd_d64_t * /* M2M DD ring alloc with alignment and boundary */
__m2m_dd_ring_mem_alloc(osl_t *osh, uint32 ring_depth, const char * ring_name, ulong *pap)
{
	void    * va;
	uintptr   base;
	uint32    alloced;

	uint32    mem_size = ring_depth * sizeof(m2m_dd_d64_t);
	uint32    align_bits = 6;   /* 64-Byte alignment (min 16-Byte reqd) */
	uintptr   boundary = (uintptr)D64RINGBOUNDARY_LARGE; /* 64 KBytes */

	ASSERT(ISPOWEROF2(ring_depth));

	va = DMA_ALLOC_CONSISTENT(osh, mem_size, align_bits, &alloced,
	                          (dmaaddr_t *)pap, NULL);
	if (va == NULL) goto done;

	base = (uintptr)va;
	ASSERT(ISALIGNED(base, (1 << align_bits))); /* hnd_malloc_align audit */

	/* Check if DD ring memory overlaps 64 KBytes boundary */
	if (((base + mem_size - 1) & boundary) != (base & boundary)) {
		uint32 mem_size2bits = mem_size;
		align_bits = 0;
		while (mem_size2bits >>= 1) align_bits++; /* align to mem_size */

		/* Free previous and re-attempt with alignment of mem_size */
		DMA_FREE_CONSISTENT(osh, va, mem_size, *((dmaaddr_t*)pap), NULL);
		va = DMA_ALLOC_CONSISTENT(osh, mem_size, align_bits, &alloced,
		                          (dmaaddr_t *)pap, NULL);
	}

done:
	if (va != NULL) {
		memset(va, 0, mem_size);
		M2M_DEBUG(
		M2M_PRINT("M2M: %s ring memory %p pap %p alloc %u align_bits %u depth %u\n",
			ring_name, va, (void *)(*pap), mem_size, align_bits, ring_depth));
	} else {
		M2M_ERROR("M2M: FAILURE %s alloc %u align_bits %u depth %u\n",
			ring_name, mem_size, align_bits, ring_depth);
	}

	return (m2m_dd_d64_t*)va;

} /* __m2m_dd_ring_mem_alloc() */

static INLINE uint32 /* M2M DD ring consumer current descriptor index */
__m2m_dd_cd_idx(osl_t * osh, m2m_dd_eng_t * eng)
{
	uint32 cd_idx; /* Convert DMA Processor CD to a dd_idx */
	cd_idx = R_REG(osh, &eng->eng_regs->rx.status0) & M2M_DD_D64_RS0_CD_MASK;
#if defined(DONGLEBUILD)
	cd_idx = cd_idx - (((uint32)(eng->rxd_table)) & M2M_DD_D64_RS0_CD_MASK);
#else /* ! DONGLEBUILD */
	cd_idx = cd_idx - (((uint32)(eng->rxd_table_pa)) & M2M_DD_D64_RS0_CD_MASK);
#endif /* ! DONGLEBUILD */
	cd_idx = cd_idx / sizeof(m2m_dd_d64_t);

	return cd_idx;

} /* __m2m_dd_cd_idx() */

/**
 * -----------------------------------------------------------------------------
 * Section: M2M CORE ISR and DPC handling
 *
 * All DD ISR/DPC handling uses a secondary ISR/DPC, registered with m2m_sys,
 * via m2m_sec_isr_register. M2MCore does not have an explicit top-level intmask
 * All per DMA channel masks are embedded in the secondary M2MCORE::int_regs[].
 * Top-level M2MCOre intstatus reports per channel's pending interrupt.
 *
 * This M2M driver, only manages the channel #0 and channel #1 Interrupts.
 * - All channels used for BME do not use interrupts.
 * - All other DMA channels repurposed for miscellaneous DMA are required to
 *   manage the interrupts using the secondary ISR and DPC mechanism.
 * -----------------------------------------------------------------------------
 */

/** Dispatch DD Interrupts */
static void m2m_dd_dispatch(m2m_dd_eng_t *eng);

#if defined(DONGLEBUILD)
/** m2m_core_regs::intstatus ISR handler */
static bool m2m_core_isr(void *cbdata);

/** Register a secondary ISR/worklet for a DMA channel in M2MCORE */
static int m2m_sec_isr_register(uint32 ch, const char * name,
	hnd_isr_sec_fn_t sec_isr_fn, hnd_worklet_fn_t sec_worklet_fn, void * cb_data);

/** m2m_core_regs::int_regs[eng_idx]::intstatus secondary ISR handler */
static bool m2m_dd_isr(void *cbdata, uint index, uint32 intstatus);

int /* Register a secondary ISR and DPC for per DMA channel INT processing */
BCMATTACHFN(m2m_sec_isr_register)(uint32 eng_idx, const char *name,
    hnd_isr_sec_fn_t sec_isr_fn, hnd_worklet_fn_t sec_worklet_fn, void * cbdata)
{
	m2m_sys_t * m2m;
	uint32 sec_isr_enab;

	/* no explicit toplevel intmask in m2mcore */
	sec_isr_enab = (1 << eng_idx);

	if (m2m_sys_gp == M2M_SYS_NULL) {
		M2M_ERROR("M2M: FAILURE %s eng %u ISR %p, %p system not initialized\n",
			name, eng_idx, sec_isr_fn, sec_worklet_fn);
		return BCME_ERROR;
	}

	m2m = m2m_sys_gp;

	/* Register secondary ISR and/or worklet for our primary handler */
	if (hnd_isr_register_sec(m2m->isr_action, HND_ISR_INDEX_DEFAULT, sec_isr_enab,
		sec_isr_fn, cbdata, sec_worklet_fn, cbdata, NULL) == FALSE)
	{
		M2M_ERROR("M2M: FAILURE register %s eng %u ISR %p, %p internal\n",
			name, eng_idx, sec_isr_fn, sec_worklet_fn);
		return BCME_ERROR;
	}

	m2m->sec_isr_enab |= sec_isr_enab;
	M2M_PRINT("M2M: Registered %s eng %u ISR %p DPC %p sec_isr_enab %08x\n",
		name, eng_idx, sec_isr_fn, sec_worklet_fn,
		m2m->sec_isr_enab);

	return BCME_OK;

} /* m2m_sec_isr_register() */

#if defined(RTE_POLL)

/* XXX RTE_POLL: To Be Tested(TBT).
 * RTE_POLL only used during SoC CPU Complex bringup, before ISRs enabled.
 * Unlike ChipCommon, applications like CSI, AirIQ are not part of this bringup.
 *
 * How to clear intstatus without m2m_core_worklet in RTE_POLL mode ? TBD
 *
 * See hnd_add_device() invocation in m2m_init() TBT
 *
 */
static void *
BCMATTACHFM(m2m_probe)(hnd_dev_t * dev, volatile void *regs, uint bus,
    uint16 device, uint coreid, uint u32)
{
#if defined(STRICT_GCC_WARNINGS) && defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif // endif
	return (void *)regs;
#if defined(STRICT_GCC_WARNINGS) && defined(__GNUC__)
#pragma GCC diagnostic pop
#endif // endif
}
static void m2m_poll(hnd_dev_t *dev)
{
	m2m_core_isr(m2m_sys_gp);
}
static hnd_dev_ops_t m2m_dev_ops = {
	probe:  m2m_probe,
	poll:   m2m_poll
};
static hnd_dev_t m2m_dev = {
	name:   "m2m",
	ops:    &m2m_dev_ops
};

#else  /* ! RTE_POLL */

static bool /* m2m_core_regs::intstatus worklet handler */
m2m_core_worklet(void *cbdata)
{
	m2m_sys_t * m2m = (m2m_sys_t *)cbdata;

	/* Run secondary worklets */
	hnd_isr_invoke_sec_worklet(m2m->isr_action, HND_ISR_INDEX_DEFAULT, m2m->intstatus);

	/* Don't reschedule */
	return FALSE;

} /* m2m_core_worklet() */

#endif /* ! RTE_POLL */

#else /* ! DONGLEBUILD */

void /* thread safe disable interrupts for a particular channel */
m2m_intrsoff(osl_t *osh, void *m2m_eng)
{
	m2m_sys_t *m2m;
	volatile m2m_int_regs_t *int_regs;
	m2m_dd_eng_t *eng = (m2m_dd_eng_t *)m2m_eng;

	M2M_ASSERT(eng);
	m2m = eng->m2m_sys;
	int_regs = &m2m->core_regs->int_regs[eng->idx];

	OSL_INTERRUPT_SAVE_AREA
	OSL_DISABLE
	W_REG(osh, &int_regs->intmask, 0U);
	OSL_RESTORE

} /* __m2m_intrsoff() */

#endif /* ! DONGLEBUILD */

static INLINE void /* thread safe enable interrupts */
__m2m_intrson(osl_t *osh, volatile uint32 *reg_intmask, uint32 intmask)
{
	OSL_INTERRUPT_SAVE_AREA
	OSL_DISABLE
	W_REG(osh, reg_intmask, intmask);
	OSL_RESTORE

} /* __m2m_intrson() */

#if defined(DONGLEBUILD)
static bool
#else
bool /* m2m_core_regs::int_regs[eng_idx]::intstatus sec DPC handler */
#endif // endif
m2m_dd_worklet(void *cbdata)
{
	m2m_dd_eng_t * eng = (m2m_dd_eng_t*)cbdata;
	m2m_sys_t    * m2m = eng->m2m_sys;
	volatile m2m_int_regs_t * int_regs = &m2m->core_regs->int_regs[eng->idx];

	//M2M_ASSERT((intstatus & (1 << eng->idx)) != 0U);

	/* Fetch and clear pending intstatus */
	eng->intstatus = R_REG(m2m->osh, &int_regs->intstatus);
#if !defined(DONGLEBUILD)
	if (eng->intstatus == 0) {
		return FALSE;
	}
#endif /* ! DONGLEBUILD */
	W_REG(m2m->osh, &int_regs->intstatus, eng->intstatus);  /* CLR */
	M2M_DEBUG(R_REG(m2m->osh, &int_regs->intstatus)); /* sync read */

	/* DISPATCH pending intstatus */
	m2m_dd_dispatch(eng);
	eng->intstatus = 0U;

	/* Reenable interrupts */
	__m2m_intrson(m2m->osh, &int_regs->intmask, eng->intmask);

	/* Don't reschedule */
	return FALSE;

} /* m2m_dd_worklet() */

#if defined(DONGLEBUILD)
static bool /* m2m_core_regs::int_regs[eng_idx]::intstatus sec ISR handler */
m2m_dd_isr(void *cbdata, uint index, uint32 intstatus)
{
	m2m_dd_eng_t * eng = (m2m_dd_eng_t*)cbdata;
	m2m_sys_t    * m2m = eng->m2m_sys;
	volatile m2m_int_regs_t * int_regs = &m2m->core_regs->int_regs[eng->idx];

	/* disable DD channel's interrupts */
	W_REG(m2m->osh, &int_regs->intmask, 0U);

	/* Request worklet */
	return TRUE;

} /* m2m_dd_isr() */

static bool /* m2m_core_regs::intstatus ISR handler */
m2m_core_isr(void *cbdata)
{
	m2m_sys_t * m2m = (m2m_sys_t *)cbdata;

	/* Save intstatus */
	m2m->intstatus = R_REG(m2m->osh, &m2m->core_regs->intstatus);

	/* No core level intmask to disable */

	/* Deliver intstatus to all registered sec isr handlers */
	hnd_isr_invoke_sec_isr(m2m->isr_action, HND_ISR_INDEX_DEFAULT, m2m->intstatus);

	/* Request worklet */
	return TRUE;

} /* m2m_core_isr() */
#else /* ! DONGLEBUILD */
bool /* m2m_core_regs::int_regs[eng_idx]::intstatus sec ISR handler */
m2m_dd_isr(void *cbdata, bool *wantdpc)
{
	m2m_dd_eng_t * eng = (m2m_dd_eng_t*)cbdata;
	m2m_sys_t    * m2m = eng->m2m_sys;
	volatile m2m_int_regs_t * int_regs = &m2m->core_regs->int_regs[eng->idx];

	eng->intstatus = R_REG(m2m->osh, &int_regs->intstatus);
	if (eng->intstatus == 0) {
		return FALSE; // Not our interrupt
	}

	/* disable DD channel's interrupts */
	W_REG(m2m->osh, &int_regs->intmask, 0U);
	*wantdpc |= TRUE;
	return TRUE;

} /* m2m_dd_isr() */
#endif /* ! DONGLEBUILD */

/**
 * -----------------------------------------------------------------------------
 * Section: M2M System APIs
 * -----------------------------------------------------------------------------
 */

void /* Debug dump the M2M information */
m2m_dump(osl_t *osh, bool verbose)
{
	m2m_sys_t    * m2m;
	m2m_dd_eng_t * eng;
	uint32 usr_idx, eng_idx, dd_idx;

	m2m = M2M_SYS(osh);
	if (m2m == M2M_SYS_NULL) {
		M2M_PRINT("M2M dump() system not initialized\n");
		return;
	}

#if defined(DONGLEBUILD)

	/* Save some column space by suppressing timestamp for actual dump */
	printf_suppress_timestamp(TRUE);

	M2M_PRINT("M2M: sys %p Core id 0x%x rev %d regs %p dd_enab %x dd %u"
		" status %08x sec_isr_enab %08x\n",
		m2m, m2m->core_id, m2m->core_rev, m2m->core_regs,
		M2M_DD_ENAB, M2M_DD_DEPTH, m2m->intstatus, m2m->sec_isr_enab);
#else /* ! DONGLEBUILD */

	M2M_PRINT("M2M: sys %p Core id 0x%x rev %d regs %p dd_enab %x dd %u"
		" status %08x si_flag %08x\n",
		m2m, m2m->core_id, m2m->core_rev, m2m->core_regs,
		M2M_DD_ENAB, M2M_DD_DEPTH, m2m->intstatus, m2m->si_flag);

#endif /* ! DONGLEBUILD */
	M2M_PRINT("\tUSR ENG Callback  DONE_CB  WAKE_CB\n");
	for (usr_idx = 0U; usr_idx < M2M_DD_USR; usr_idx++) {
		m2m_dd_usr_t *usr = &m2m->usr[usr_idx];
		if (usr->eng == M2M_DD_ENG_NULL) continue;
		M2M_PRINT("\t%s %3d %p %p %p\n",
			m2m_usr_str[usr_idx], (int)(usr->eng - m2m->eng),
			usr->cbdata, usr->done_cb, usr->wake_cb);
	}

	M2M_PRINT("\tENG WRIDX RDIDX XDIDX TXDTABLE RXDTABLE XIDTABLE "
		"INTSTATUS INTMASK TXDD_PTR RXDD_PTR\n");

	for (eng_idx = 0U; eng_idx < M2M_DD_ENG; eng_idx++) {
		if (((1 << eng_idx) & M2M_DD_ENAB) == 0) continue;
		eng = &m2m->eng[eng_idx];

		M2M_PRINT("\tCh%d %5u %5u %5u %p %p %p %08x %08x %p %p\n", eng_idx,
			M2M_DD_RING_PROD_IDX(eng), M2M_DD_RING_CONS_IDX(eng), eng->xd_idx,
			eng->txd_table, eng->rxd_table, eng->xid_table,
			eng->intstatus, eng->intmask, (void *)eng->txdd_ptr,
		    (void *)eng->rxdd_ptr);

	}

	if (verbose == FALSE) return;

	for (eng_idx = 0U; eng_idx < M2M_DD_ENG; eng_idx++) {
		if (((1 << eng_idx) & M2M_DD_ENAB) == 0) continue;
		eng = &m2m->eng[eng_idx];

		M2M_PRINT("\tDD# USER FLAG XFER_LEN XFER_ARG Engine %u\n", eng_idx);
		for (dd_idx = 0U; dd_idx < M2M_DD_DEPTH; ++dd_idx) {
			m2m_dd_xid_t * xid = &eng->xid_table[dd_idx];
			M2M_PRINT("\t%3u %04x 0x%02x %8u %p\n",
				dd_idx, xid->key, xid->xop, xid->len, xid->arg);
		}
	}

#if defined(DONGLEBUILD)
	printf_suppress_timestamp(FALSE);
#endif /* ! DONGLEBUILD */

} /* m2m_dump() */

void /* Debug dump the M2M registers */
m2m_regs(osl_t *osh, bool verbose)
{
	m2m_sys_t    * m2m;
	m2m_dd_eng_t * eng;
	m2m_dd_d64_t * txdd;
	m2m_dd_d64_t * rxdd;
	uint32         eng_idx;
	uint32         dd_idx;

	volatile m2m_core_regs_t * m2m_core_regs;
	volatile m2m_int_regs_t  * m2m_int_regs;
	volatile m2m_eng_regs_t  * m2m_eng_regs;

	m2m = M2M_SYS(osh);
	if (m2m == M2M_SYS_NULL) {
		M2M_PRINT("M2M regs() system not initialized\n");
		return;
	}

	m2m_core_regs = m2m->core_regs;
	M2M_PRINT("M2M: control %08x cap %08x intcontrol %08x intstatus %08x\n",
		R_REG(m2m->osh, &m2m_core_regs->control),
		R_REG(m2m->osh, &m2m_core_regs->capabilities),
		R_REG(m2m->osh, &m2m_core_regs->intcontrol),
		R_REG(m2m->osh, &m2m_core_regs->intstatus));

	M2M_PRINT("ENG WRIDX RDIDX DMA "
		" CONTROL  POINTER  ADDRLOW ADDRHIGH  STATUS0  STATUS1\n");

	for (eng_idx = 0U; eng_idx < M2M_DD_ENG; eng_idx++)
	{
		if (((1 << eng_idx) & M2M_DD_ENAB) == 0) continue;
		eng = &m2m->eng[eng_idx];

		m2m_int_regs = &m2m_core_regs->int_regs[eng_idx];
		m2m_eng_regs = &m2m_core_regs->eng_regs[eng_idx];

		ASSERT(m2m_int_regs != (m2m_int_regs_t *)NULL);
		ASSERT(m2m_eng_regs != (m2m_eng_regs_t *)NULL);

		M2M_PRINT("%2u. %5u %5u TXD %08x %08x %08x %08x %08x %08x\n",
			eng_idx, M2M_DD_RING_PROD_IDX(eng), M2M_DD_RING_CONS_IDX(eng),
			R_REG(osh, &m2m_eng_regs->tx.control),
			R_REG(osh, &m2m_eng_regs->tx.ptr),
			R_REG(osh, &m2m_eng_regs->tx.addrlow),
			R_REG(osh, &m2m_eng_regs->tx.addrhigh),
			R_REG(osh, &m2m_eng_regs->tx.status0),
			R_REG(osh, &m2m_eng_regs->tx.status1));
		M2M_PRINT("                RXD %08x %08x %08x %08x %08x %08x\n",
			R_REG(osh, &m2m_eng_regs->rx.control),
			R_REG(osh, &m2m_eng_regs->rx.ptr),
			R_REG(osh, &m2m_eng_regs->rx.addrlow),
			R_REG(osh, &m2m_eng_regs->rx.addrhigh),
			R_REG(osh, &m2m_eng_regs->rx.status0),
			R_REG(osh, &m2m_eng_regs->rx.status1));

		M2M_PRINT("                INT status %08x mask %08x rcvlazy %08x\n\n",
			R_REG(osh, &m2m_int_regs->intstatus),
			R_REG(osh, &m2m_int_regs->intmask),
			R_REG(osh, &m2m_core_regs->intrcvlazy[eng_idx]));

	}

	if (verbose == FALSE) return;

	for (eng_idx = 0U; eng_idx < M2M_DD_ENG; eng_idx++)
	{
		uint32 xfer_len;
		if (((1 << eng_idx) & M2M_DD_ENAB) == 0) continue;
		eng = &m2m->eng[eng_idx];

		txdd = M2M_DD_ELEM(m2m_dd_d64_t, eng->txd_table, 0U);
		rxdd = M2M_DD_ELEM(m2m_dd_d64_t, eng->rxd_table, 0U);

#if (M2M_DD_DEPTH <= 16)
		M2M_PRINT("\tDD# TXDCtrl1 TXDCtrl2 TXDAddrL TXDAddrH"
			           " RXDCtrl1 RXDCtrl2 RXDAddrL RXDAddrH Bytes\n");
		for (dd_idx = 0U; dd_idx < M2M_DD_DEPTH; ++dd_idx, ++txdd, ++rxdd) {
			xfer_len = txdd->ctrl2 & D64_CTRL2_BC_MASK;
			M2M_PRINT("\t%3u %08x %08x %08x %08x "
				"%08x %08x %08x %08x %5u\n", dd_idx,
				txdd->ctrl1, txdd->ctrl2, txdd->addr64.lo, txdd->addr64.hi,
				rxdd->ctrl1, rxdd->ctrl2, rxdd->addr64.lo, rxdd->addr64.hi,
				xfer_len);
		}
#else /* M2M_DD_DEPTH > 16 */
		M2M_PRINT("\tDD# Src_Addr Dst_Addr Bytes Engine %u\n", eng_idx);
		for (dd_idx = 0U; dd_idx < M2M_DD_DEPTH; ++dd_idx, ++txdd, ++rxdd) {
			xfer_len = txdd->ctrl2 & D64_CTRL2_BC_MASK;
			M2M_PRINT("\t%3u %08x %08x %5u\n",
				dd_idx, txdd->addr64.lo, rxdd->addr64.lo, xfer_len);
		}
#endif /* M2M_DD_DEPTH > 16 */
	}

} /* m2m_regs() */

void /* Debug dump statistics: per User, per Engine and M2M system */
m2m_stats(osl_t *osh)
{
#if defined(M2M_STATS_BUILD)
	m2m_sys_t * m2m;
	uint32      usr_idx, eng_idx;
	m2m_stats_t m2m_stats             = {0};
	m2m_stats_t eng_stats[M2M_DD_ENG] = {{0}, {0}};

	m2m = M2M_SYS(osh);
	if (m2m == M2M_SYS_NULL) {
		M2M_PRINT("M2M stats() system not initialized\n");
		return;
	}

	M2M_PRINT("\tM2M %12s %12s %12s %12s\n",
		"Bytes", "Transfers", "Callbacks", "Failures");
	for (usr_idx = 0U; usr_idx < M2M_DD_USR; usr_idx++) {
		m2m_dd_usr_t *usr = &m2m->usr[usr_idx];
		if (usr->eng == M2M_DD_ENG_NULL) continue;
		eng_idx = (uint32)(usr->eng - m2m->eng);
		M2M_STATS_ACCUM(eng_stats[eng_idx], usr->stats);
		M2M_PRINT("\t%s " M2M_STATS_FMT "\n",
			m2m_usr_str[usr_idx], M2M_STATS_VAL(usr->stats));
		M2M_STATS_ZERO(usr->stats);
	}
	M2M_PRINT("\n");

	for (eng_idx = 0U; eng_idx < M2M_DD_ENG; eng_idx++) {
		if (((1 << eng_idx) & M2M_DD_ENAB) == 0) continue;
		M2M_STATS_ACCUM(m2m_stats, eng_stats[eng_idx]);
		M2M_PRINT("\tCH%d " M2M_STATS_FMT "\n",
			eng_idx, M2M_STATS_VAL(eng_stats[eng_idx]));
	}

	M2M_PRINT("\tM2M " M2M_STATS_FMT "\n\n", M2M_STATS_VAL(m2m_stats));
#endif /* M2M_STATS_BUILD */

} /* m2m_stats() */

void /* DONGLEBUILD: Parse dhd cons "m2m -[s|r|R|t|T|v]" and invoke dump */
hnd_cons_m2m_dump(void *arg, int argc, char *argv[])
{
	osl_t *osh = (osl_t *)arg; // TBD change the hnd_cons call accordingly
	bool verbose = FALSE;

	if (argc >= 2) {
		if (strcmp(argv[1], "-h") == 0) {        /* dhd -i eth1 cons "m2m -h" */
			printf("M2M Options: -[hrRsv]\n");
			return;
		} else if (strcmp(argv[1], "-r") == 0) { /* dhd -i eth1 cons "m2m -r" */
			m2m_regs(osh, FALSE); /* dump registers - non verbose version */
			return;
		} else if (strcmp(argv[1], "-R") == 0) { /* dhd -i eth1 cons "m2m -R" */
			m2m_regs(osh, TRUE); /* dump registers - verbose to incld dd rings */
			return;
		} else if (strcmp(argv[1], "-s") == 0) { /* dhd -i eth1 cons "m2m -s" */
			m2m_stats(osh); /* dump statistics */
			return;
		} else if (strcmp(argv[1], "-v") == 0) { /* dhd -i eth1 cons "m2m -v" */
			verbose = TRUE;
		}
	}

	/* verbose or non-verbose dump of m2m runtime state */
	m2m_dump(osh, verbose);

} /* hnd_cons_m2m_dump() */

int /* Deinitialize M2M service */
BCMATTACHFN(m2m_fini)(si_t * sih, osl_t * osh)
{
	uint32         eng_idx;
	m2m_sys_t    * m2m;
	m2m_dd_eng_t * eng;
	dmaaddr_t      pa;

	m2m = M2M_SYS(osh);
	if (m2m == M2M_SYS_NULL)
		goto done;

	PHYSADDRHISET(pa, 0);
	PHYSADDRLOSET(pa, 0);
	BCM_REFERENCE(pa);

	for (eng_idx = 0U; eng_idx < M2M_DD_ENG; eng_idx++) {
		eng = &m2m->eng[eng_idx];
		if (eng->txd_table != NULL)
			DMA_FREE_CONSISTENT(osh, eng->txd_table,
			    sizeof(m2m_dd_d64_t) * M2M_DD_DEPTH, pa, NULL);
		if (eng->rxd_table != NULL)
			DMA_FREE_CONSISTENT(osh, eng->rxd_table,
			    sizeof(m2m_dd_d64_t) * M2M_DD_DEPTH, pa, NULL);
		if (eng->xid_table != M2M_DD_XID_NULL)
			MFREE(osh, eng->xid_table, sizeof(m2m_dd_xid_t) * M2M_DD_DEPTH);
	}

	memset(m2m,  0, sizeof(*m2m));
	MFREE(osh, m2m, sizeof(*m2m));

#if defined(DONGLEBUILD)
	m2m_sys_gp = M2M_SYS_NULL;
#endif // endif
	OSH_SETM2M(osh, M2M_SYS_NULL); /* Set OSH m2m ptr to NULL for FD/NIC */

done:
	return BCME_OK;

} /* m2m_fini() */

int /* Initialize M2M service */
BCMATTACHFN(m2m_init)(si_t * sih, osl_t * osh)
{
	m2m_sys_t    * m2m;
	m2m_dd_eng_t * eng;

	uint32 v32, eng_idx, usr_idx;
	uint32 m2m_core_id, m2m_core_rev;
	uint32 saved_core_id, saved_intr_val;
	uint32 rx_addrhigh, tx_addrhigh;
	uint32 rx_addrlow, tx_addrlow;
	/* physical addr for the descriptor tables, not used for FD */
	ulong rxd_table_pa, txd_table_pa;

	volatile m2m_core_regs_t * m2m_core_regs;
	volatile m2m_int_regs_t  * m2m_int_regs;
	volatile m2m_eng_regs_t  * m2m_eng_regs;
	volatile char *base_addr;

	const uint32 align_bits = 6; /* 64-Byte alignment */
	int ret = BCME_OK;

	BCM_REFERENCE(align_bits);
	BCM_REFERENCE(base_addr);
	m2m = M2M_SYS(osh);

	/* m2m_init invoked after bme_init */
	if (m2m != M2M_SYS_NULL) {
		M2M_PRINT("M2M service already initialized\n");
		return ret;
	}

	if (si_findcoreidx(sih, M2MDMA_CORE_ID, 0) == BADIDX) {
		M2M_PRINT("M2M core not available\n");
		return ret;
	}
	M2M_PRINT("M2M Service Initialization: M2M_DD_ENAB 0x%08x\n", M2M_DD_ENAB);
	m2m_core_regs = (volatile m2m_core_regs_t *)
		si_switch_core(sih, M2MDMA_CORE_ID, &saved_core_id, &saved_intr_val);
	ASSERT(m2m_core_regs != NULL);

	/* Take M2M core out of reset if it's not */
	if (!si_iscoreup(sih)) {
		M2M_PRINT("M2M_CORE found, but not up\n"); // bme_init not called?
		si_core_reset(sih, 0, 0);
	}

	m2m_core_id  = si_coreid(sih);
	m2m_core_rev = si_corerev(sih);

	if (m2m_core_rev < 3) {
		M2M_PRINT("M2M: Unavailable in M2M Core rev %d\n", m2m_core_rev);
		ret = BCME_UNSUPPORTED;
		goto done;
	}
	M2M_PRINT("M2M: M2M Core id 0x%x rev %u\n", m2m_core_id, m2m_core_rev);

	v32 = R_REG(osh, &m2m_core_regs->capabilities);
	M2M_DEBUG(
		M2M_PRINT("M2M: ChannelCnt %u+1 MaxBurstLen %u MaxReadOutstanding %u\n",
			BCM_GBF(v32, M2M_CORE_CAPABILITIES_CHANNELCNT),
			BCM_GBF(v32, M2M_CORE_CAPABILITIES_MAXBURSTLEN),
			BCM_GBF(v32, M2M_CORE_CAPABILITIES_MAXREADSOUTSTANDING)));

	/* Instantiate M2M service */
	m2m = (m2m_sys_t *)MALLOCZ(osh, sizeof(*m2m));
	if (m2m == (m2m_sys_t *)NULL) {
		M2M_ERROR("M2M: FAILURE m2m_sys alloc %u\n", (uint)sizeof(*m2m));
		ret = BCME_NORESOURCE;
		goto done;
	}

	OSH_SETM2M(osh, m2m); /* Set OSH m2m ptr for both FD/NIC */

#if defined(DONGLEBUILD)
	/* Save m2m instance */
	m2m_sys_gp = m2m;
#else /* ! DONGLEBUILD */
#if defined(BCMHWA)
	/**
	 * Both HWA and M2M modules are using sixth 4KB region of PCIE BAR 0 space (0x78) to map
	 * corresponding core register space.
	 * Need two spare "mappable" 4K regions in BAR0 to enable both HWA & M2M interrupts
	 */
#error "Both HNDM2M && BCMHWA features are enabled"
#endif /* BCMHWA */

	/* For Chips over PCIe, map sixth 4KB region of PCIE BAR 0 space (0x78) to
	 * M2M core register space.
	 */
	base_addr = (volatile char *) m2m_core_regs;
	if (BUSTYPE(sih->bustype) == PCI_BUS) {
		/* Set PCIE register PCIE2_BAR0_CORE2_WIN2 with M2M core register space. */
		si_backplane_pci_config_bar0_core2_win2(sih,
			si_addrspace(sih, CORE_SLAVE_PORT_0, CORE_BASE_ADDR_0));

		/* Adjust the M2M regs with offset for PCIE2_BAR0_CORE2_WIN2 */
		base_addr = (volatile char *) (base_addr + PCIE2_BAR0_CORE2_WIN2_OFFSET);
		m2m_core_regs = (m2m_core_regs_t *)base_addr;
		M2M_DEBUG(M2M_PRINT("M2M: PCIE offset'ed m2m_core_regs %p \n", m2m_core_regs));

		/* Get M2M core bitmap in DMP wrapper's oob_select_a_in register (oobselouta30).
		 * Used to configure M2M core interrupt in PCIEIntMask (PCI_INT_MASK) register.
		 */
		m2m->si_flag = si_flag(sih);
	}
#endif /* ! DONGLEBUILD */

	/* Initialize USR context, in particular dll init the wake node */
	for (usr_idx = 0; usr_idx < M2M_DD_USR; usr_idx++)
	{
		m2m_dd_usr_t * m2m_dd_usr_p = &m2m->usr[usr_idx];

		dll_init(&m2m_dd_usr_p->wake);
		m2m_dd_usr_p->eng       = M2M_DD_ENG_NULL;
		m2m_dd_usr_p->cbdata    = NULL;
		m2m_dd_usr_p->done_cb   = NULL;
		m2m_dd_usr_p->wake_cb   = NULL;
	}

#if defined(DONGLEBUILD)
	/* Register polling device or core isr/worklet, prepare sec isr registration */
	M2M_DEBUG(M2M_PRINT("M2M: Register polling device or ISR\n"));
#if defined(RTE_POLL)
	hnd_add_device(sih, &m2m_dev, M2MDMA_CORE_ID, BCM47XX_D11_SOCDEV); /* TBT */
#else  /* ! RTE_POLL */
	/* Creates an internal DPC to invoke the worklet_fn m2m_core_worklet() */
	m2m->isr_action = hnd_isr_register(M2MDMA_CORE_ID, 0, SI_BUS,
	                        m2m_core_isr, m2m, m2m_core_worklet, m2m, NULL);
	if (m2m->isr_action == (hnd_isr_action_t*)NULL) {
		M2M_ERROR("M2M: FAILURE primary ISR register\n");
		m2m_fini(sih, osh);
		goto done;
	}
	M2M_PRINT("M2M: Registered ISR %p worklet %p for primary handler %p\n",
		m2m_core_isr, m2m_core_worklet, m2m->isr_action);
#endif /* ! RTE_POLL */
#endif /* DONGLEBUILD */

	m2m->core_regs = m2m_core_regs;
	m2m->core_id   = m2m_core_id;
	m2m->core_rev  = m2m_core_rev;
	m2m->sih       = sih;
	m2m->osh       = osh;

	/* Allocate M2M memory resources needed for TxD, RxD and resume rings */
	for (eng_idx = 0U; eng_idx < M2M_DD_ENG; eng_idx++)
	{
		eng = &m2m->eng[eng_idx];
		eng->idx     = eng_idx;
		eng->m2m_sys = m2m;

		if (((1 << eng_idx) & M2M_DD_ENAB) == 0U) continue;

		/* Allocate memory resources for the RxD, TxD and XiD rings */
		eng->txd_table = __m2m_dd_ring_mem_alloc(osh, M2M_DD_DEPTH, "TXD",
		                                         &txd_table_pa);
		eng->rxd_table = __m2m_dd_ring_mem_alloc(osh, M2M_DD_DEPTH, "RXD",
		                                         &rxd_table_pa);
#if defined(DONGLEBUILD)
		eng->xid_table = (m2m_dd_xid_t*)
			MALLOC_ALIGN(osh, sizeof(m2m_dd_xid_t) * M2M_DD_DEPTH, align_bits);
#else /* ! DONGLEBUILD */
		eng->xid_table = (m2m_dd_xid_t*)
			MALLOCZ(osh, sizeof(m2m_dd_xid_t) * M2M_DD_DEPTH);
		eng->txd_table_pa = (uintptr)txd_table_pa;
		eng->rxd_table_pa = (uintptr)rxd_table_pa;
		/* Set the PCI64ADDR_HIGH bit of the physical address for PCIe access */
		if (M2M_USE_PCI64ADDR(m2m)) {
			eng->txd_table_pa |= ((uint64)M2M_PCI64ADDR_HIGH << 32);
			eng->rxd_table_pa |= ((uint64)M2M_PCI64ADDR_HIGH << 32);
		}
#endif /* ! DONGLEBUILD */

		if (!eng->txd_table || !eng->rxd_table || !eng->xid_table) {
			M2M_ERROR("M2M: FAILURE eng %u alloc rings <%p,%p>,%p depth %u\n",
				eng_idx, eng->txd_table, eng->rxd_table,
				eng->xid_table, M2M_DD_DEPTH);
			m2m_fini(sih, osh); /* resets m2m_sys to NULL */
			goto done;
		}

		memset(eng->xid_table, 0, sizeof(m2m_dd_xid_t) * M2M_DD_DEPTH);

#if defined(DONGLEBUILD)
		/* Register per DD engine secondary ISR,DPC */
		if (m2m_sec_isr_register(eng_idx, "DD", m2m_dd_isr, m2m_dd_worklet, eng)
		        == BCME_ERROR)
		{
			M2M_PRINT("M2M: FAILURE DD %u sec ISR registration\n", eng_idx);
			m2m_fini(sih, osh);
			goto done;
		}
#else /* ! DONGLEBUILD */
		M2M_DEBUG(M2M_PRINT("M2M: txd_table_pa lo 0x%08x\n",
			(uint32)eng->txd_table_pa));
		M2M_DEBUG(M2M_PRINT("M2M: rxd_table_pa lo 0x%08x\n",
			(uint32)eng->rxd_table_pa));
#endif /* ! DONGLEBUILD */
		M2M_ERROR("M2M: SUCCESS eng %u alloc rings <%p,%p>,%p depth %u\n",
			eng_idx, eng->txd_table, eng->rxd_table,
			eng->xid_table, M2M_DD_DEPTH);
	}

	/* Initialize each M2M engine */
	for (eng_idx = 0U; eng_idx < M2M_DD_ENG; eng_idx++)
	{
		uint32 m2m_dd_bl = DMA_BL_256;

		eng = &m2m->eng[eng_idx];
		M2M_ASSERT(eng->idx == eng_idx);

		if (((1 << eng_idx) & M2M_DD_ENAB) == 0U) continue;

		dll_init(&eng->wake_list);

#if defined(DONGLEBUILD)
		/* Ensure secondary ISR/DPC have been attached */
		M2M_ASSERT((m2m->sec_isr_enab & (1 << eng_idx)) != 0U);
#endif /* DONGLEBUILD */

		/* Get per engine, interrupt registers and DMA processor registers */
		m2m_int_regs = &m2m_core_regs->int_regs[eng_idx];
		m2m_eng_regs = &m2m_core_regs->eng_regs[eng_idx];

		/* Disable routing of DMA INT to other cores */
		AND_REG(osh, &m2m_core_regs->intcontrol /* 0x8 */, ~(1 << eng_idx));

		/* Disable engine's interrupt, and clear its pending interrupt */
		W_REG(osh, &m2m_int_regs->intmask,   0U);  // redundant, disable DD INT
		W_REG(osh, &m2m_int_regs->intstatus, ~0U); // clr DD instatus
		eng->intstatus = 0U;

		/* Need to explicitly re-write IntRcvLazy [FC=1, TO=0] */
		v32 = BCM_SBF(1, M2M_CORE_CH_INTRCVLAZY_FC);
		W_REG(osh, &m2m_core_regs->intrcvlazy[eng_idx], v32);
		M2M_PRINT("M2M: DMA %u IntRcvLazy %08x\n", eng_idx, v32);

		/* CLR top level pending interrupt, for engine */
		v32 = 1 << eng_idx;
		W_REG(osh,   &m2m_core_regs->intstatus /* 0x20 */,  v32);
		/* Enable engine's default interrupt */
		eng->intmask = M2M_DD_INTMASK;
		W_REG(osh, &m2m_int_regs->intmask, eng->intmask);

		/* Always program Receive before Transmit DMA Processor for a channel */

		/* Prepare engine's Receive and Transmit DMA Processor */
		/* dm64regs_t::tx,rx {control ptr addrlow addrhigh status0 status1} */

		/* Initialize WR and RD indices for DD ring */
		eng->xd_idx = 0;
		bcm_ring_init(&eng->ring_state);
		memset(eng->txd_table, 0, (sizeof(m2m_dd_d64_t) * M2M_DD_DEPTH));
		memset(eng->rxd_table, 0, (sizeof(m2m_dd_d64_t) * M2M_DD_DEPTH));

		/* Setup the M2M DMA Processor's Receive and Transmit DD rings */
		M2M_ASSERT(eng->rxd_table != M2M_DD_D64_NULL);
		M2M_ASSERT(eng->txd_table != M2M_DD_D64_NULL);
		rx_addrhigh = 0x0;
		tx_addrhigh = 0x0;
#if defined(DONGLEBUILD)
		rx_addrlow  = (uint32)eng->rxd_table;
		tx_addrlow  = (uint32)eng->txd_table;
#else /* ! DONGLEBUILD */
		rx_addrhigh = (uint32)((uint64)eng->rxd_table_pa >> 32);
		tx_addrhigh = (uint32)((uint64)eng->txd_table_pa >> 32);
		rx_addrlow  = (uint32)(eng->rxd_table_pa);
		tx_addrlow  = (uint32)(eng->txd_table_pa);
#endif /* ! DONGLEBUILD */
		W_REG(osh, &m2m_eng_regs->rx.addrlow, rx_addrlow);
		W_REG(osh, &m2m_eng_regs->rx.addrhigh, rx_addrhigh);
		W_REG(osh, &m2m_eng_regs->tx.addrlow, tx_addrlow);
		W_REG(osh, &m2m_eng_regs->tx.addrhigh, tx_addrhigh);
		M2M_DEBUG(M2M_PRINT("M2M: eng %u Rxaddrhi 0x%08x Rxaddrlo 0x%08x "
		          "Txaddrhi 0x%08x Txaddrlo 0x%08x\n", eng_idx,
		          rx_addrhigh, rx_addrlow, tx_addrhigh, tx_addrlow));

		/* XXX
		 * Unlike PCIECORE's DMA Processor, in M2MCORE, the RcvPtr and XmtPtr
		 * needs to be explicitly initialized with the low 16bits of AddrLow of
		 * their DD rings.
		 * E.g. hnddma.c, on invoking _dma_ddtable_init() which sets the AddrLow
		 * of the Receive and Transmit DMA Processor, the RcvPtr and XmtPtr
		 * inherit the low 16bits in their respective AddrLow.
		 * In M2MCORE, only CD and AD inherit the low 16bits of AddrLow.
		 */

#if defined(DONGLEBUILD)
		eng->rxdd_ptr = rx_addrlow; /* RxD ring AddrLow low16 */
		eng->txdd_ptr = tx_addrlow; /* TxD ring AddrLow low16 */
#else /* ! DONGLEBUILD */
		eng->rxdd_ptr = eng->rxd_table_pa; /* RxD ring AddrLow low16 */
		eng->txdd_ptr = eng->txd_table_pa; /* TxD ring AddrLow low16 */
#endif /* ! DONGLEBUILD */
		W_REG(osh, &m2m_eng_regs->rx.ptr, (uint32)(eng->rxdd_ptr & 0x0000ffff));

		W_REG(osh, &m2m_eng_regs->tx.ptr, (uint32)(eng->txdd_ptr & 0x0000ffff));

#if (HNDM2M < 131)
		if (eng_idx == M2M_DD_D2H) {
			m2m_dd_bl = DMA_BL_32;
		}
#endif // endif

		/* Enable the M2M DMA Channel's Receive and Transmit DMA Processors */
		v32  = R_REG(osh, &m2m_eng_regs->rx.control);
		M2M_DEBUG(M2M_PRINT("M2M: eng %u RcvCtrl 0x%08x\n", eng_idx, v32));
		v32  = BCM_CBF(v32, D64_RC_BL);
		v32 |= M2M_DD_RC_CONFIG(m2m_dd_bl) | D64_RC_RE;
#if !defined(OSL_CACHE_COHERENT)
		v32 = v32 & ~D64_RC_CO_MASK; /* Clear Coherent bit */
#endif // endif
		W_REG(osh, &m2m_eng_regs->rx.control, v32); // 0x05#8#801

		v32  = R_REG(osh, &m2m_eng_regs->tx.control);
		M2M_DEBUG(M2M_PRINT("M2M: eng %u XmtCtrl 0x%08x\n", eng_idx, v32));
		v32  = BCM_CBF(v32, D64_XC_BL);
		v32 |= M2M_DD_XC_CONFIG(m2m_dd_bl) | D64_XC_XE;
#if !defined(OSL_CACHE_COHERENT)
		v32 = v32 & ~D64_XC_CO_MASK; /* Clear Coherent bit */
#endif // endif
		W_REG(osh, &m2m_eng_regs->tx.control, v32); // 0x05#8#801

		M2M_PRINT("M2M: DMA %u RcvCtrl XmtCtrl enabled\n", eng_idx);

		eng->eng_regs = m2m_eng_regs;
	}

done:
	si_restore_core(sih, saved_core_id, saved_intr_val);
	return ret;

} /* m2m_init() */

#if defined(DONGLEBUILD)
int /* Link Host provided IPC information, e.g. addressing mode */
m2m_link_pcie_ipc(struct dngl_bus *pciedev, struct pcie_ipc *pcie_ipc)
{
	if (m2m_sys_gp == M2M_SYS_NULL) {
		M2M_ERROR("M2M: FAILURE m2m_link_pcie_ipc system not initialized\n");
		ASSERT(0);
		return BCME_ERROR;
	}

	return BCME_OK;

} /* m2m_link_pcie_ipc() */
#endif /* DONGLEBUILD */

/**
 * -----------------------------------------------------------------------------
 * Section: M2M Interface for DMA Descriptor based Engines
 * -----------------------------------------------------------------------------
 */
static void /* Default DMA done callback handler */
_m2m_dd_done_cb_noop(void *cbdata,
                     dma64addr_t *xfer_src, dma64addr_t *xfer_dst, int xfer_len,
                     void *xfer_arg)
{
	M2M_DEBUG(M2M_PRINT("M2M: DONE NOOP callback\n"));
}

static void /* Default Wake callback handler */
_m2m_dd_wake_cb_noop(void *usr_cbdata)
{
	M2M_DEBUG(M2M_PRINT("M2M: WAKE NOOP callback\n"));
}

m2m_dd_key_t /* Register a user context. Returns a registration key for copy */
m2m_dd_usr_register(osl_t *osh, uint32 usr_idx, uint32 eng_idx,
                    void *usr_cbdata, m2m_dd_done_cb_t usr_done_cb,
                    m2m_dd_wake_cb_t usr_wake_cb, uint16 usr_wake_thr)
{
	m2m_sys_t     * m2m;
	m2m_dd_usr_t  * usr;
	m2m_dd_eng_t  * eng;
	m2m_dd_key_t    key = M2M_INVALID;

	m2m = M2M_SYS(osh);
	/* Ensure M2M system is initialized */
	if (m2m == M2M_SYS_NULL) {
		M2M_ERROR("M2M: FAILURE DD usr register(%s) system not initialized\n",
			m2m_usr_str[usr_idx]);
		goto failure;
	}
	usr = &m2m->usr[usr_idx];          M2M_ASSERT(usr_idx < M2M_DD_USR);

	/* Ensure User slot is not in use */
	if (usr->eng != (m2m_dd_eng_t*)NULL) {
		M2M_ERROR("M2M: FAILURE DD usr registration(%s) already in use\n",
			m2m_usr_str[usr_idx]);
		goto failure;
	}

	/* Ensure Engine is enabled */
	eng = &m2m->eng[eng_idx];          M2M_ASSERT(eng_idx < M2M_DD_ENG);
#if defined(DONGLEBUILD)
	if ((m2m->sec_isr_enab & (1 << eng_idx)) == 0U) {
		M2M_ERROR("M2M: FAILURE DD usr registration(%s) eng %u not enab %08x\n",
			m2m_usr_str[usr_idx], eng_idx, m2m->sec_isr_enab);
		goto failure;
	}
#endif /* DONGLEBUILD */

	key = usr_idx;                     /* key same as usr_idx, presently */

	if (usr_done_cb == (m2m_dd_done_cb_t)NULL)
		usr_done_cb = _m2m_dd_done_cb_noop; /* bind a noop done callback */

	if (usr_wake_cb == (m2m_dd_wake_cb_t)NULL)
		usr_wake_cb = _m2m_dd_wake_cb_noop; /* bind a noop wake callback */

	usr->eng        = eng;
	usr->cbdata     = usr_cbdata;       /* user callback context */
	usr->done_cb    = usr_done_cb;      /* user DMA DONE callback */
	usr->wake_cb    = usr_wake_cb;      /* user DD Avail WAKE callback */
	usr->wake_thr   = usr_wake_thr;     /* user DD avail WAKE threshold */

	M2M_DEBUG(M2M_PRINT("M2M: usr %s (eng %p) registered \n",
	                    m2m_usr_str[usr_idx], usr->eng));
	return key;

failure:
	ASSERT(0);
	return key;

} /* m2m_dd_usr_register() */

int /* Determine available space in DD ring */
m2m_dd_avail(osl_t *osh, m2m_dd_key_t m2m_dd_key)
{
	uint32 usr_idx;
	m2m_sys_t    * m2m;
	m2m_dd_usr_t * usr;
	m2m_dd_eng_t * eng;

	m2m     = M2M_SYS(osh);            M2M_ASSERT(m2m != M2M_SYS_NULL);
	usr_idx = m2m_dd_key;              M2M_ASSERT(usr_idx < M2M_DD_USR);

	usr     = &m2m->usr[usr_idx];
	eng     = usr->eng;                M2M_ASSERT(eng != (m2m_dd_eng_t*)NULL);

	return bcm_ring_prod_avail(&eng->ring_state, M2M_DD_DEPTH);

} /* m2m_dd_avail() */

int /** User requests a WAKE callback, prior to stalling on lack of DD */
m2m_dd_wake_request(osl_t *osh, m2m_dd_key_t m2m_dd_key)
{
	uint32 usr_idx;
	m2m_sys_t    * m2m;
	m2m_dd_usr_t * usr;
	m2m_dd_eng_t * eng;

	m2m     = M2M_SYS(osh);            M2M_ASSERT(m2m != M2M_SYS_NULL);
	usr_idx = m2m_dd_key;              M2M_ASSERT(usr_idx < M2M_DD_USR);
	usr     = &m2m->usr[usr_idx];
	eng     = usr->eng;                M2M_ASSERT(eng != (m2m_dd_eng_t*)NULL);

	M2M_ASSERT(usr->wake_cb != _m2m_dd_wake_cb_noop);
	dll_delete(&usr->wake);
	dll_append(&eng->wake_list, &usr->wake);

	return BCME_OK;

}   /* m2m_dd_wake_request() */

m2m_dd_cpy_t /* Initiate a m2m copy using a registered user's key */
m2m_dd_xfer(osl_t *osh, m2m_dd_key_t m2m_dd_key,
    dma64addr_t *xfer_src, dma64addr_t *xfer_dst, int xfer_len,
    void * xfer_arg, uint32 xfer_op)
{
	m2m_sys_t    * m2m;
	m2m_dd_usr_t * usr;
	m2m_dd_eng_t * eng;
	m2m_dd_d64_t * txdd;
	m2m_dd_d64_t * rxdd;
#if !defined(DONGLEBUILD)
	uint64        txdd_pa = 0U;
	uint64        rxdd_pa = 0U;
#endif /* DONGLEBUILD */
	m2m_dd_xid_t * xidd;
	uint32         usr_idx;
	uint32         ctrl2;
	uint32         txdd_ctrl1;
	uint32         rxdd_ctrl1;
	m2m_dd_cpy_t   dd_idx;

	m2m               = M2M_SYS(osh);  M2M_ASSERT(m2m != M2M_SYS_NULL);
	usr_idx           = m2m_dd_key;    M2M_ASSERT(usr_idx < M2M_DD_USR);
	usr               = &m2m->usr[usr_idx];
	eng               = usr->eng;      M2M_ASSERT(eng != (m2m_dd_eng_t*)NULL);

	if (bcm_ring_is_full(&eng->ring_state, M2M_DD_DEPTH))
	{
		M2M_DEBUG(M2M_PRINT("M2M: FAILURE copy(%s) %08x %08x %u dd full\n",
			m2m_usr_str[usr_idx], xfer_src->loaddr, xfer_dst->loaddr,
			xfer_len));
		M2M_STATS(usr->stats.fails++);

		return M2M_INVALID; /* INVALID m2m_dd_cpy_t */
	}

	/* --- Cannot fail now --- */

	/* Setup Rx and Tx Descriptors, for requested DMA transaction */

	/* Fetch and commit the next index where a ring element may be produced */
	dd_idx            = bcm_ring_prod(&eng->ring_state, M2M_DD_DEPTH);

	/* Use the dd_idx to fetch the Tx(Pull) Rx(Push) descriptor and XferInfo */
	txdd              = M2M_DD_ELEM(m2m_dd_d64_t, eng->txd_table, dd_idx);
	rxdd              = M2M_DD_ELEM(m2m_dd_d64_t, eng->rxd_table, dd_idx);
#if !defined(DONGLEBUILD)
	txdd_pa           = (uint64)(uintptr)M2M_DD_ELEM(m2m_dd_d64_t,
	                                    (uintptr)eng->txd_table_pa, dd_idx);
	rxdd_pa           = (uint64)(uintptr)M2M_DD_ELEM(m2m_dd_d64_t,
	                                    (uintptr)eng->rxd_table_pa, dd_idx);
#endif /* ! DONGLEBUILD */
	xidd              = M2M_DD_ELEM(m2m_dd_xid_t, eng->xid_table, dd_idx);

	/* Save the transfer transaction attributes */
	xidd->key         = m2m_dd_key;
	xidd->xop         = (uint8)xfer_op;
	xidd->len         = xfer_len; /* used only for xfer_op M2M_DD_XFER_RESUME */
	xidd->arg         = xfer_arg; /* used only for xfer_op M2M_DD_XFER_RESUME */
	M2M_OSL_CACHE_FLUSH(osh, (void*)(xidd), sizeof(m2m_dd_xid_t));

	/* AddrExt = 0, and Parity disabled in Tx (XC_PD) and Rx (RC_PD) */
	ctrl2             = (xfer_len & D64_CTRL2_BC_MASK); /* AE=0, Parity=0 */
	txdd_ctrl1        = D64_CTRL1_SOF | D64_CTRL1_EOF;
	rxdd_ctrl1        = D64_CTRL1_SOF | D64_CTRL1_EOF;

	/* SVMP memory treated as-if SysMem. Need not be coherent */
	if (eng->idx == M2M_DD_D2H) { /* D2H */
		/* CH0: Tx(Pull) from SysMem and Rx(Push) to DDR */
		txdd_ctrl1   |= D64_CTRL1_COHERENT | D64_CTRL1_NOTPCIE;
	} else { /* H2D */
		/* CH1: Tx(Pull) from DDR, Rx(Push) to SysMem */
		rxdd_ctrl1   |= D64_CTRL1_COHERENT | D64_CTRL1_NOTPCIE;
	}

	/* User requested DMA transfer DONE resume callback */
	if (xfer_op & M2M_DD_XFER_RESUME)
		rxdd_ctrl1   |= D64_CTRL1_IOC; /* Enable DMA DONE INT */

	/* EOT Descriptor handling, and XmtPtr,RcvPtr advancement with wrap */
	if (dd_idx != (M2M_DD_DEPTH - 1)) { /* !EOT descriptor, advance by 1 */
#if defined(DONGLEBUILD)
		eng->rxdd_ptr = ((uintptr)(rxdd + 1));
		eng->txdd_ptr = ((uintptr)(txdd + 1));
#else /* ! DONGLEBUILD */
		eng->rxdd_ptr = ((uintptr)((m2m_dd_d64_t *)(uintptr)rxdd_pa + 1));
		eng->txdd_ptr = ((uintptr)((m2m_dd_d64_t *)(uintptr)txdd_pa + 1));
#endif /* ! DONGLEBUILD */
	} else {                            /* EOT descriptor, wrap to start */
		txdd_ctrl1   |= D64_CTRL1_EOT;
		rxdd_ctrl1   |= D64_CTRL1_EOT;
#if defined(DONGLEBUILD)
		eng->rxdd_ptr = (uintptr)eng->rxd_table;
		eng->txdd_ptr = (uintptr)eng->txd_table;
#else /* ! DONGLEBUILD */
		eng->rxdd_ptr = (uintptr)eng->rxd_table_pa;
		eng->txdd_ptr = (uintptr)eng->txd_table_pa;
#endif /* ! DONGLEBUILD */
	}

	/* Compose Rx(Push) descriptor with destination attributes */
	rxdd->ctrl1       = rxdd_ctrl1;
	rxdd->ctrl2       = ctrl2;
	rxdd->addr64.lo   = xfer_dst->loaddr;
	rxdd->addr64.hi   = xfer_dst->hiaddr;

	if (M2M_USE_PCI64ADDR(m2m) && (eng->idx == M2M_DD_D2H)) {
		rxdd->addr64.hi |= M2M_PCI64ADDR_HIGH;
	}
	M2M_OSL_CACHE_FLUSH(osh, (void*)(rxdd), sizeof(m2m_dd_d64_t));

	/* Compose Tx(Pull) descriptor with source attributes */
	txdd->ctrl1       = txdd_ctrl1;
	txdd->ctrl2       = ctrl2;
	txdd->addr64.lo   = xfer_src->loaddr;
	txdd->addr64.hi   = xfer_src->hiaddr;

	if (M2M_USE_PCI64ADDR(m2m) && (eng->idx == M2M_DD_H2D)) {
		txdd->addr64.hi |= M2M_PCI64ADDR_HIGH;
	}
	M2M_OSL_CACHE_FLUSH(osh, (void*)(txdd), sizeof(m2m_dd_d64_t));
	M2M_STATS(usr->stats.bytes += xfer_len);
	M2M_STATS(usr->stats.xfers++);

	/* Kick start Rx and Tx DMA Processor by committing RcvPtr and XmtPtr */
	if ((xfer_op & M2M_DD_XFER_COMMIT) == 0U)
		goto done; /* no kick start */

	DMB(); /* memory barrier */

	/* COMMIT: advance Rx then Tx DMA Processors */
	W_REG(m2m->osh, &eng->eng_regs->rx.ptr,
	      (uint32)(eng->rxdd_ptr & 0x0000ffff));
	W_REG(m2m->osh, &eng->eng_regs->tx.ptr,
	      (uint32)(eng->txdd_ptr & 0x0000ffff));

done:
	M2M_DEBUG(M2M_PRINT("M2M: %s dd %u copy from 0x%08x 0x%08x c1 0x%08x c2 "
		"0x%08x to 0x%08x 0x%08x c1 0x%08x c2 0x%08x %u param %08x xop %x\n",
		m2m_usr_str[usr_idx], dd_idx, txdd->addr64.hi, txdd->addr64.lo,
		txdd->ctrl1, txdd->ctrl2, rxdd->addr64.hi, rxdd->addr64.lo, rxdd->ctrl1,
		rxdd->ctrl2, xfer_len, (uint32)xfer_arg, xfer_op));

	return M2M_DD_CPY_KEY(xfer_len, dd_idx, usr_idx);

} /* m2m_dd_xfer() */

/**
 * Main DPC processing to reclaim processed descriptors.
 * - Invoke user registered DMA Done callback, so it may resume asynchronous
 *   processing.
 * - Given descriptors are available, wakeup users that requested a wakeup
 *   notification on descriptors availability above the user specified free
 *   threshold.
 */
void
m2m_dd_dispatch(m2m_dd_eng_t *eng)
{
	bcm_ring_t     xd_ring;
	m2m_sys_t    * m2m;
	m2m_dd_d64_t * txdd;
	m2m_dd_d64_t * rxdd;
	m2m_dd_xid_t * xidd;
	m2m_dd_usr_t * usr;
	uint32         dd_idx, cd_idx;

	m2m = eng->m2m_sys;

	M2M_DEBUG(M2M_PRINT("M2M: DD %u dispatch intstatus %08x\n",
		eng->idx, eng->intstatus));

	if (eng->intstatus & M2M_CORE_CH_ERR_INTMASK) {
		M2M_ERROR("M2M: FAILURE DD %u intStatus %08x\n",
			eng->idx, eng->intstatus);
		m2m_regs(m2m->osh, TRUE);
		ASSERT(0);
		eng->intmask &= (~M2M_CORE_CH_ERR_INTMASK);
	}

	/* Fetch the CD - index consumed by DMA processor */
	cd_idx = __m2m_dd_cd_idx(eng->m2m_sys->osh, eng);
	M2M_DD_RING_CONS_IDX(eng) = (int)cd_idx; /* m2m_dd_eng::ring_state::read */

	M2M_DEBUG(M2M_PRINT("  > m2m_dd_dispatch: CD %u, XD %u\n",
		cd_idx, eng->xd_idx));
	bcm_ring_setup(&xd_ring, cd_idx, eng->xd_idx); /* init write=cd, read=XD */

	while ((dd_idx = bcm_ring_cons(&xd_ring, M2M_DD_DEPTH)) != BCM_RING_EMPTY)
	{
		xidd = M2M_DD_ELEM(m2m_dd_xid_t, eng->xid_table, dd_idx);
		usr = &m2m->usr[xidd->key];
		//OSL_CACHE_INV((void*)(xidd), sizeof(m2m_dd_xid_t));
		usr = &m2m->usr[xidd->key];

		if (xidd->xop & M2M_DD_XFER_RESUME) {
			txdd = M2M_DD_ELEM(m2m_dd_d64_t, eng->txd_table, dd_idx); /* src */
			rxdd = M2M_DD_ELEM(m2m_dd_d64_t, eng->rxd_table, dd_idx); /* dst */

			M2M_DEBUG(M2M_PRINT(
				"M2M: DD %u XID %u key %x xop %x %08x->%08x len %u arg %x\n",
				eng->idx, dd_idx, xidd->key, xidd->xop,
				txdd->addr64.lo, rxdd->addr64.lo, xidd->len, (uint32)xidd->arg));
			M2M_STATS(usr->stats.cbfns++);

			//OSL_CACHE_INV((void*)(txdd), sizeof(m2m_dd_d64_t));
			//OSL_CACHE_INV((void*)(rxdd), sizeof(m2m_dd_d64_t));

			/* Invoke the user done callback routine with transfer attributes */
			usr->done_cb(usr->cbdata,
			             &txdd->addr64, &rxdd->addr64, xidd->len,
			             xidd->arg);
		} else {
			M2M_DEBUG(M2M_PRINT("M2M: DD %u XID %u key %x xop %x\n",
				eng->idx, dd_idx, xidd->key, xidd->xop));
		}
	}
	eng->xd_idx = xd_ring.write;
	M2M_DEBUG(M2M_PRINT("  < m2m_dd_dispatch: XD %u\n", eng->xd_idx));

	/* Check if any user had requested a WAKE up on DD availability */
	while (!dll_empty(&eng->wake_list)) {
		dll_t * dll_wake = dll_head_p(&eng->wake_list); // dll m2m_dd_usr::wake
		usr = M2M_DD_WAKE_USR(dll_wake); // container m2m_dd_usr

		/* Check if avail m2m eng dd is above the user wake up threshold */
		if (bcm_ring_prod_avail(&eng->ring_state, M2M_DD_DEPTH) < usr->wake_thr)
			break;

		/* Remove user from engine's WAKE up notification list */
		dll_delete(dll_wake);
		dll_init(dll_wake);

		/* WAKE up user */
		usr->wake_cb(usr->cbdata);
	}

} /* m2m_dd_dispatch() */

#if !defined(DONGLEBUILD)
void * // Get the M2M engine pointer
m2m_dd_eng_get(osl_t *osh, m2m_dd_key_t key)
{
	m2m_sys_t *m2m;
	m2m_dd_usr_t *usr;

	m2m = M2M_SYS(osh);
	M2M_ASSERT(m2m != M2M_SYS_NULL);
	usr = &m2m->usr[key];
	M2M_ASSERT(usr != (m2m_dd_usr_t*)NULL);
	M2M_ASSERT(usr->eng != (m2m_dd_eng_t*)NULL);

	return (void *)usr->eng;
}

uint // Get SI flag for the M2M system: M2M core bitmap in oobselouta30 reg
m2m_dd_si_flag_get(osl_t *osh)
{
	m2m_sys_t *m2m = M2M_SYS(osh);
	M2M_ASSERT(m2m);
	return m2m->si_flag;
}
#endif /* ! DONGLEBUILD */
