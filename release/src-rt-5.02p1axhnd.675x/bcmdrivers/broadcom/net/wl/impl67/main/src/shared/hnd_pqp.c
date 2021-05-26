/*
 * +--------------------------------------------------------------------------+
 *  HND Packet Queue Pager (PQP) library
 *
 *  Copyright 2020 Broadcom
 *
 *  This program is the proprietary software of Broadcom and/or
 *  its licensors, and may only be used, duplicated, modified or distributed
 *  pursuant to the terms and conditions of a separate, written license
 *  agreement executed between you and Broadcom (an "Authorized License").
 *  Except as set forth in an Authorized License, Broadcom grants no license
 *  (express or implied), right to use, or waiver of any kind with respect to
 *  the Software, and Broadcom expressly reserves all rights in and to the
 *  Software and all intellectual property rights therein.  IF YOU HAVE NO
 *  AUTHORIZED LICENSE, THEN YOU HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY
 *  WAY, AND SHOULD IMMEDIATELY NOTIFY BROADCOM AND DISCONTINUE ALL USE OF
 *  THE SOFTWARE.
 *
 *  Except as expressly set forth in the Authorized License,
 *
 *  1. This program, including its structure, sequence and organization,
 *  constitutes the valuable trade secrets of Broadcom, and you shall use
 *  all reasonable efforts to protect the confidentiality thereof, and to
 *  use this information only in connection with your use of Broadcom
 *  integrated circuit products.
 *
 *  2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED
 *  "AS IS" AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES,
 *  REPRESENTATIONS OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR
 *  OTHERWISE, WITH RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY
 *  DISCLAIMS ANY AND ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY,
 *  NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES,
 *  ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
 *  CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING
 *  OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
 *
 *  3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL
 *  BROADCOM OR ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL,
 *  SPECIAL, INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR
 *  IN ANY WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN
 *  IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii)
 *  ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF
 *  OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS SHALL APPLY
 *  NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
 *  <<Broadcom-WL-IPTag/Proprietary:>>
 *
 *  $Id$
 *
 *  vim: set ts=4 noet sw=4 tw=80:
 *  -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * +--------------------------------------------------------------------------+
 */

#if defined(DONGLEBUILD) && defined(HNDPQP)
#include <osl.h>
#include <bcmhme.h>
#include <bcmpcie.h>
#include <bcmutils.h>
#include <hnd_pkt.h>
#include <hnd_pktq.h>
#include <hnd_pqp.h>
#if defined(BCMHWA)
#include <hwa_export.h> /* HWA2.1 HWA_REVISION_ID <= 130, hwa_defs.h hwapkt_t */
#endif // endif
#if defined(HNDM2M)
#include <hndm2m.h>
#endif // endif

/**
 * XXX FIXME:
 *
 * 1. PGI resource allocation, with wakeup callback
 *    Support for No-HWA mode? PKT_ID_FIXUP
 *    LCL heap allocation failure ... HOL blocking of a PGI request
 *    Reserve resources: to avoid toggling between discrete resource wakeups
 * 2. _pqp_pktlist_hbm_reset(). Do not traverse list. Reset pkt during iteration
 * 3. PGO of LCL (Best effort?). In 6715, LCL is xfered by driver. If suppressed
 *    retain LCL in DDR.
 *
 * Synchronous M2M, simpler but still not run-to-completion. 43684 M2M HW War
 * requires BL=1, so develop a BME based solution? BME BL can be bumped to 4 by
 * ensuring bme copy requests of >1K be serviced by splitting into a sequence of
 * max 1K copies within the BME driver. [Defer to Phase 3].
 */

/** Compile time tunables */
/* Maximum number of descriptors needed per MPDU */
#define PQP_MGR_M2M_DD_CH0_THRESH   (16)
#define PQP_MGR_M2M_DD_CH1_THRESH   (4)
/* Maximum number of Host BM Packets needed per MPDU. LCL are best effort */
#define PQP_MGR_HBMPKT_THRESH       (16)

/** Conditional Compile: Designer builds for extended debug and statistics */
#define PQP_DEBUG_BUILD
#define PQP_STATS_BUILD

#define PQP_NOOP                    do { /* no-op */ } while(0)
#define PQP_PRINT(fmt, args...)     printf(fmt "\n", ##args)
#define PQP_ERROR(fmt, args...)     printf(fmt "\n", ##args)

#define PQP_CTRS_RDZERO             /* Clear On Read */
#if defined(PQP_CTRS_RDZERO)
#define PQP_CTR_ZERO(CTR)           (CTR) = 0U
#else   /* ! PQP_CTRS_RDZERO */
#define PQP_CTR_ZERO(CTR)           PQP_NOOP
#endif  /* ! PQP_CTRS_RDZERO */

#if defined(PQP_DEBUG_BUILD)
#define PQP_ASSERT(EXPR)            ASSERT(EXPR)
#define PQP_DEBUG(EXPR)             EXPR
#else  /* ! PQP_DEBUG_BUILD */
#define PQP_ASSERT(EXPR)            PQP_NOOP
#define PQP_DEBUG(EXPR)             PQP_NOOP
#endif /* ! PQP_DEBUG_BUILD */

#if defined(PQP_STATS_BUILD)
#define PQP_STATS(EXPR)             EXPR
#define PQP_STATS_ZERO(CTR)         PQP_CTR_ZERO(CTR)
#else   /* ! PQP_STATS_BUILD */
#define PQP_STATS(EXPR)             PQP_NOOP
#define PQP_STATS_ZERO(EXPR)        PQP_NOOP
#endif  /* ! PQP_STATS_BUILD */

#define PQP_HPKT_NULL               ((uintptr)0)
#define PQP_DPKT_NULL               ((hndpkt_t *)NULL)
#define PQP_PKTQ_NULL               ((pktq_prec_t *)NULL)

#define PQP_MAP_NULL                ((pqp_map_t *)NULL)
#define PQP_REQ_NULL                ((pqp_req_t *)NULL)

#define PQP_PKTID_NULL              ((uint16)(0))
#define PQP_HBM_ID_INV              ((uint16)(~0))
#define PQP_HBM_ID_NULL             ((uint16)(0))
#define PQP_HBM_PAGE_INV            ((uint32)(~0))

#define PQP_ASSERT_HBM_ID(ID) \
	PQP_ASSERT(((ID) != PQP_HBM_ID_INV) && ((ID) != PQP_HBM_ID_NULL))

/**
 * Host Packet Buffer Memory
 *
 * Host Packet BM is treated as an array of Packet structures to hold a Lfrag
 * and a D11 data buffer. Host Packet Pool storage is DMA-able and may be
 * assumed to not cross a 32-bit boundary (i.e the haddr64.hi is the same across
 * all packets. Each Host BM packet is tracked by a 16 bit index. PQP uses a
 * mwbmap as a free index allocator. This 16-bit index is saved in the Host BM
 * packet (pqp_mem::hbm_pktid and pqp_mem::hbm_lclid).
 *
 * The Host Memory Extension for LCL Buffer pool is assumed to not overlap the
 * dongle memory address space.
 *
 * PKTNEXT and PKTLINK continue to be used to track single linked lists of MSDUs
 * and MPDUs, respectively, with respect to dongle memory space. These single
 * linked lists are represented with respected to Host BM space using uintptr
 * (hbm_pktlink and hbm_pktnext).
 *
 * Host Packet Pool Storage is managed using a mwbmap
 * - 16-bit index 0xFFFF is treated as invalid (used to detect un-initialized).
 * - Packet at index 0 is reserved (never allocated) to denote an end of list.
 * - The hwa_pkt::w10:rd_index (used for flowring rollback on suppression) is no
 *   longer relevant with PSQ supported by PQP. rd_index is used to save the
 *   Host BM index assigned to a packet.
 * - Likewise the hwa_pkt::w10:<audit_flags> is repurposed to save the HBM lclid
 *
 * A dma64addr_t may be composed by using the PQP_HBM_###PTR as the low 32 bits
 * and the high 32 bits retrieved from the PQP manager Host BM Packet base.
 *
 */

/** Following PQP_HBM_### conversion macros use PQP Host BM base */
/*  - Convert a PQP Host BM Packet Pointer to a PQP Host BM Packet Index */
#define PQP_HBM_PKTID(PQP_MGR, HBM_PKTPTR) \
({  uint16 hbm_pktid; \
	hbm_pktid = (((HBM_PKTPTR) - (PQP_MGR)->hbmP.memptr) / PQP_PKT_SZ); \
	PQP_ASSERT(hbm_pktid != PQP_HBM_ID_NULL); \
	hbm_pktid; \
})
/*  - Convert a PQP Host BM Packet Index to a PQP Host BM Packet Pointer */
#define PQP_HBM_PKTPTR(PQP_MGR, HBM_PKTID) \
({  uintptr hbm_pktptr; \
	PQP_ASSERT_HBM_ID(HBM_PKTID); \
	hbm_pktptr = (PQP_MGR)->hbmP.memptr + ((HBM_PKTID) * PQP_PKT_SZ); \
	hbm_pktptr; \
})
/*   - Convert a PQP Host BM Packet Index to a PQP Host BM D11 Buffer Pointer */
#define PQP_HBM_D11PTR(PQP_MGR, HBM_PKTID) \
	(PQP_HBM_PKTPTR((PQP_MGR), (HBM_PKTID)) + HBMPKT_SIZE)

/*  - Convert a PQP Host BM Lcl Pointer to a PQP Host BM Lcl Index */
#define PQP_HBM_LCLID(PQP_MGR, HBM_LCLPTR) \
({  uint16 hbm_lclid; \
	hbm_lclid = (((HBM_LCLPTR) - (PQP_MGR)->hbmL.memptr) / PQP_LCL_SZ); \
	PQP_ASSERT_HBM_ID(hbm_lclid); \
	hbm_lclid; \
})
/*  - Convert a PQP Host BM Lcl Buffer Index to a PQP Host BM address */
#define PQP_HBM_LCLPTR(PQP_MGR, HBM_LCLID) \
({  uintptr hbm_lclptr; \
	PQP_ASSERT_HBM_ID(HBM_LCLID); \
	hbm_lclptr = (PQP_MGR)->hbmL.memptr + ((HBM_LCLID) * PQP_LCL_SZ); \
	hbm_lclptr; \
})

/**
 * Packet Portability Notes:
 * -------------------------
 * Base packet structure is a lbuf, aka hndpkt_t.
 *
 * In HWA2.1, a hwapkt_t is prepended to a lbuf_frag. However, WLAN driver is
 * unaware of the hwa_pkt_t preamble as all PKTR### macros like PKTNEXT, PKTLINK
 * PKTDATA, etc, operate on the Lbuf.
 * HWA2.1 driver uses HWAPKT2LFRAG(hwa_pkt) and LFRAG2HWAPKT(lbuf), to convert
 * between a hwa_pkt and lbuf, in the HWA3a and HWA3b blocks. PQP driver has
 * repurposed fields in the hwa_pkt_t. See hwa_defs.h
 *
 * In HWA2.2, there is no hwapkt_t preamble. The HWA2.2 Packet Pager is aware
 * of the packet structure. A Packet is essentially a 256 Byte structure to
 * include a Context::[Lbuf and Frag list for 4 Host PktIds] + Data Buffer.
 * Unlike HWA2.1, where a separate HW managed Pool exists Transmit Lfrags and
 * Receive Lfrags, in HWA2.2 a single pool is shared for both Tx and Rx Lfrags.
 *
 * In the case when the data buffer storage does not suffice (e.g. a local heap
 * allocated storage for PktFetched), then the Local Heap buffer is attached.
 *
 * When Packet Queues are paged out, the resource and list management fields are
 * listed below:
 *  - hbm_pktid:   index of the HBMPKT in Host Packet BM
 *  - hbm_lclid:   index of the 2 KByte data buffer in Host Data BM
 *  - hbm_pktlink: PKTLINK equivalent with respect to Host Packet BM
 *  - hbm_pktnext: PKTNEXT equivalent with respect to Host Packet BM
 */

#if defined(BCMHWA)
#if (HWA_REVISION_ID <= 130)
/** HWA2.1 : a hwapkt_t preamble prior to the Lbuf carries HBM fields */
#define HBM2HNDPKT(hbmpkt)          ((hndpkt_t*)(HWAPKT2LFRAG(hbmpkt)))
#define HND2HBMPKT(hndpkt)          ((hwapkt_t*)(LFRAG2HWAPKT(hndpkt)))
#define HBMPKT_SIZE                 (HWA_TXPOST_PKT_BYTES + LBUFFRAGSZ)
#define HNDPKT_D3BUFF(hndpkt)       HND2HBMPKT(hndpkt)->eth_sada.u8
#define HNDPKTSETDATA(osh, hndpkt) \
	HWAPKTSETDATA(LFRAG2HWAPKT(hndpkt), PKTDATA(osh, hndpkt))
#define HNDPKTSETNEXT(osh, hndpkt, hndpkt_next) \
({ \
	PKTSETNEXT(osh, hndpkt, hndpkt_next); \
	HWAPKTSETNEXT(LFRAG2HWAPKT(hndpkt), LFRAG2HWAPKT(hndpkt_next)); \
})
#define HNDPKT_FIXUP(osh, hndpkt) \
({ \
	for (; hndpkt; hndpkt = PKTNEXT(osh, hndpkt)) { \
		if (PKTNEXT(osh, hndpkt)) { \
			HWAPKTSETNEXT(LFRAG2HWAPKT(hndpkt), LFRAG2HWAPKT(PKTNEXT(osh, hndpkt))); \
		} \
	} \
})
#else   /* HWA_REVISION_ID > 130 */
#error  "PQP not ported to HWA2.2, HWA_REVISION_ID > 130"
#endif  /* HWA_REVISION_ID > 130 */
#else   /* !BCMHWA : INTERNAL ONLY not for production */
/** When HWA is disabled for designer debug, Lbuf carries HBM fields */
#define HBM2HNDPKT(hbmpkt)          LBP(hndpkt)
#define HND2HBMPKT(hndpkt)          LBP(hndpkt)
#define HBMPKT_SIZE                 (LBUFFRAGSZ)
#define HNDPKT_D3BUFF(hndpkt)       PQP_DPKT_NULL
#define HNDPKTSETDATA(osh, hndpkt)  PQP_NOOP
#define HNDPKTSETNEXT(osh, hndpkt, hndpkt_next) \
	PKTSETNEXT(osh, hndpkt, hndpkt_next)
#define HNDPKT_FIXUP(osh, hndpkt)   PQP_NOOP
#endif  /* ! BCMHWA */

/** Following PKT_### accessor macros take a hndpkt (lbuf) as a parameter. */
/*  - hndpkt accessor to host bm index, given a hndpkt pointer */
#define PKT_HBM_PKTID(hndpkt)   HND2HBMPKT(hndpkt)->pqp_mem.hbm_pktid
#define PKT_HBM_LCLID(hndpkt)   HND2HBMPKT(hndpkt)->pqp_mem.hbm_lclid
#define PKT_HBM_PAGE(hndpkt)    HND2HBMPKT(hndpkt)->pqp_hbm_page
/*  - hndpkt accessors to PQP Host BM uintptr link and next */
#define PKT_HBM_PKTLINK(hndpkt) HND2HBMPKT(hndpkt)->pqp_list.hbm_pktlinkid
#define PKT_HBM_PKTNEXT(hndpkt) HND2HBMPKT(hndpkt)->pqp_list.hbm_pktnextid

#define PKT_HEAD_FIXUP(PKT, HEAD) \
({ LB_HEAD(pkt) = (HEAD); })

#define PKT_DATA_FIXUP(PKT, DATA) \
({ LB_DATA(pkt) = (DATA); })

#define PKT_END_FIXUP(PKT, END) \
({ LB_END(pkt)  = (END);  })

/**
 * PQP extension to HND Packet Queue (pktq_prec)
 *
 * A pktq_prec when managed by PQP, repurposes the the pktq_prec::head and
 * pktq_prec::tail pointers. Upon the first PAGE-OUT operation on a pktq_prec
 * these fields may not be directly accessible by the pktq_prec owner. The
 * pktq_prec is under PQP management.
 *
 *  - pktq_prec::head is overlayed by a pointer to a pqp_req object that tracks
 *    the PQP page operation.
 *  - pktq_prec::tail is overlayed by a pair of Host Packet BM indices to denote
 *    the head and tail packets of the pktq_prec that have been PAGED to host.
 *
 * PQP uses 0xFFFF to track invalid (uninitialized) index, as value 0x0000 is
 * used to terminate a list.
 */
#define PKTQ_HEAD(PKTQ)             (PKTQ)->head
#define PKTQ_TAIL(PKTQ)             (PKTQ)->tail
#define PKTQ_PKTS(PKTQ)             (PKTQ)->n_pkts

#define PKTQ_PQP_REQ(PKTQ)          (PKTQ)->pqp_req
#define PKTQ_PQP_HBM_HEAD(PKTQ)     (PKTQ)->pqp_hbm.head
#define PKTQ_PQP_HBM_TAIL(PKTQ)     (PKTQ)->pqp_hbm.tail
#define PKTQ_PQP_HBM_PAGE(PKTQ)     (PKTQ)->pqp_hbm_page

/* PQP Request containing dll node */
#define PQP_REQ_DLL2PTR(DLL_NODE)   CONTAINEROF(DLL_NODE, pqp_req_t, node)

/* PQP Request M2M Transfer state */
#if defined(HNDM2M)
/* PQP M2M DMA ongoing (DMA DONE pending) in PGO(D2H) and PGI(H2D) direction */
#define PQP_REQ_DMAO(PQP_REQ)       ((pqp_req_t*)(PQP_REQ))->m2m.xfer.dmaO
#define PQP_REQ_DMAI(PQP_REQ)       ((pqp_req_t*)(PQP_REQ))->m2m.xfer.dmaI
#define PQP_REQ_XFER(PQP_REQ)       ((pqp_req_t*)(PQP_REQ))->m2m.u32
/* Used in M2M DD Transfers, to indicate interest in a DMA done callback */
#define PQP_M2M_DD_XFER_SYNC        (M2M_DD_XFER_COMMIT | M2M_DD_XFER_RESUME)
#define PQP_M2M_DD_XFER_NOOP        (M2M_DD_XFER_NOOP)
#endif  /* HNDM2M */

/* Pool of pqp_req_t are managed in pqp_mgr::pool */
#define PQP_MGR_IS_PQP_REQ(PQP_MGR, PQP_REQ) \
	(((PQP_REQ) >= ((PQP_MGR)->pool)) && \
	 ((PQP_REQ) <  ((PQP_MGR)->pool + (PQP_MGR)->size)))

/* PQP debug dump formatting */
#if defined(PQP_DEBUG_BUILD)
#define PQP_REQ_FLAG_HDR        " Flag PGO PGI  D2H H2D  HBM DBM BUF  ACT APP"
#define PQP_REQ_FLAG_HFMT       "      %3u %3u  %3u %3u  %3u %3u %3u  %3u %3u"
#define PQP_REQ_FLAG_FMT        " POP %u,%u DMA %u,%u BM %u,%u,%u ACT %u APP %u"
#define PQP_REQ_FLAG_VAL(FLAG) \
	((FLAG) & PQP_REQ_FLAG_PGO) > 0, ((FLAG) & PQP_REQ_FLAG_PGI) > 0, \
	((FLAG) & PQP_REQ_FLAG_D2H) > 0, ((FLAG) & PQP_REQ_FLAG_H2D) > 0, \
	((FLAG) & PQP_REQ_FLAG_HBM) > 0, ((FLAG) & PQP_REQ_FLAG_DBM) > 0, \
	((FLAG) & (PQP_REQ_FLAG_D11 | PQP_REQ_FLAG_LCL)) > 0,              \
	((FLAG) & PQP_REQ_FLAG_ACT) > 0, ((FLAG) & PQP_REQ_FLAG_APP) > 0
#else   /* ! PQP_DEBUG_BUILD */
#define PQP_REQ_FLAG_HDR            " Flag"
#define PQP_REQ_FLAG_HFMT           " %04x"
#define PQP_REQ_FLAG_FMT            PQP_REQ_FLAG_HFMT
#define PQP_REQ_FLAG_VAL(FLAG)      (FLAG)
#endif  /* ! PQP_DEBUG_BUILD */

#if defined(HNDM2M)
#define PQP_REQ_HDR     " PktQHead PktQIter PktQTail dmaO dmaI" \
	                    "  pktlink save buff hbmi" PQP_REQ_FLAG_HDR
#define PQP_REQ_HFMT    " %p %p %p %4u %4u %p %4u %4u %4u" PQP_REQ_FLAG_HFMT
#define PQP_REQ_FMT     " h,i,t %p,%p,%p dmaO,I %4u,%4u" \
	                    " l,s,b,i %p %4u %4u %4u" \
	                    PQP_REQ_FLAG_FMT
#define PQP_REQ_VAL(PQP_REQ) \
	((pqp_req_t*)(PQP_REQ))->pgo.head, ((pqp_req_t*)(PQP_REQ))->pgo.iter, \
	((pqp_req_t*)(PQP_REQ))->pgo.tail, \
	PQP_REQ_DMAO(PQP_REQ), PQP_REQ_DMAI(PQP_REQ), \
	((pqp_req_t*)(PQP_REQ))->pgi.link, ((pqp_req_t*)(PQP_REQ))->pgi.save, \
	((pqp_req_t*)(PQP_REQ))->pgi.buff, ((pqp_req_t*)(PQP_REQ))->hbmI, \
	PQP_REQ_FLAG_VAL(((pqp_req_t*)(PQP_REQ))->flag)
#else   /* ! HNDM2M */
#define PQP_REQ_HDR     " PktQHead PktQIter PktQTail" PQP_REQ_FLAG_HDR
#define PQP_REQ_FMT     " %p %p %p" PQP_REQ_FLAG_FMT
#define PQP_REQ_VAL(PQP_REQ) \
	((pqp_req_t*)(PQP_REQ))->head, ((pqp_req_t*)(PQP_REQ))->iter, \
	((pqp_req_t*)(PQP_REQ))->tail, \
	PQP_REQ_FLAG_VAL(((pqp_req_t*)(PQP_REQ))->flag)
#endif  /* ! HNDM2M */

#define PQP_MGR_FMT \
	" Host BM cntP %u cntL %u Req PGO %u PGI %u ACT %u FREE %u MAX %u\n" \
	"\t\t hbmP" HADDR64_FMT " hbmL" HADDR64_FMT

#define PQP_MGR_VAL(PQP_MGR) \
	(PQP_MGR)->cntP, (PQP_MGR)->cntL, \
	(PQP_MGR)->cntO, (PQP_MGR)->cntI, (PQP_MGR)->cntH, \
	(PQP_MGR)->cntF, (PQP_MGR)->size, \
	HADDR64_VAL((PQP_MGR)->hbmP.haddr64), HADDR64_VAL((PQP_MGR)->hbmL.haddr64)

/**
 * +--------------------------------------------------------------------------+
 *  Packet Queue Pager Typedefs
 * +--------------------------------------------------------------------------+
 */

typedef struct bcm_mwbmap pqp_map_t; /* bcmutils.h multiword bitmap allocator */
typedef struct pqp_req    pqp_req_t;

typedef enum pqp_req_flag       /* Per PQP Request flags used in FSM */
{
	PQP_REQ_FLAG_PGO = 1 << 0,  /* PAGE_OUT operation queued */
	PQP_REQ_FLAG_PGI = 1 << 1,  /* PAGE_IN  operation queued */
	PQP_REQ_FLAG_D2H = 1 << 2,  /* PAGE_OUT stall on D2H M2M DD */
	PQP_REQ_FLAG_H2D = 1 << 3,  /* PAGE_IN  stall on H2D M2M DD */
	PQP_REQ_FLAG_HBM = 1 << 4,  /* PAGE_OUT stall on Host BM Packet */
	PQP_REQ_FLAG_DBM = 1 << 5,  /* PAGE_IN  stall on Dongle TxBM Packet */
	PQP_REQ_FLAG_D11 = 1 << 6,  /* PAGE_IN  stall on Dongle D11 buffer */
	PQP_REQ_FLAG_LCL = 1 << 7,  /* PAGE_IN  stall on Dongle D11 buffer */
	PQP_REQ_FLAG_ACT = 1 << 8,  /* pqp_req in dllH, may have D2H in-progress */
	PQP_REQ_FLAG_APP = 1 << 9,  /* SuprPSQ: uses append_policy */
	PQP_REQ_FLAG_QCB = 1 << 10, /* PAGE_IN scheduling completed */
} pqp_req_flag_t;

#if (PQP_REQ_FLAG_PGO != PQP_OP_PGO) || (PQP_REQ_FLAG_PGI != PQP_OP_PGI)
#error "PQP Request operation and flags mismatch"
#endif // endif

#define PQP_REQ_FLAG_POP            (PQP_REQ_FLAG_PGO | PQP_REQ_FLAG_PGI)
#define PQP_REQ_FLAG_PGO_STALL      (PQP_REQ_FLAG_D2H | PQP_REQ_FLAG_HBM)
#define PQP_REQ_FLAG_PGI_STALL      (PQP_REQ_FLAG_H2D | PQP_REQ_FLAG_DBM | \
	                                 PQP_REQ_FLAG_D11 | PQP_REQ_FLAG_LCL)
#define PQP_REQ_FLAG_STALL \
	    (PQP_REQ_FLAG_PGO_STALL | PQP_REQ_FLAG_PGI_STALL)

typedef struct pqp_stats        /* Packet Queue Pager Statistics */
{
	uint32            req;      /* - count of Page OP Requests */
	uint32            cbk;      /* - count of Page OP User Callbacks */
	uint32            pkt;      /* - count of Packets transferred */
	uint32            d11;      /* - count of D11 data buffer transferred */
	uint32            lcl;      /* - count of LCL data buffer transferred */
	uint32            xfer;     /* - count of M2M transfers invoked */
	uint32            done;     /* - count of M2M DMA done callbacks invoked */
	uint32            pend;     /* - count of resource depletion stalls */
	uint32            wake;     /* - count of wakeups from stalls */
	uint32            fail;     /* - count of failures */
} pqp_stats_t;

struct pqp_req                  /* Packet Queue Pager Request */
{
	dll_t             node;     /* Freelist of pqp_req_t */
	pktq_prec_t     * pktq;     /* Link back to managed pktq_prec_t */
	uint16            flag;     /* Flags tracking pqp_req pgo/pgi state */
	uint16            hbmI;     /* PGI iterator over PKTQ:pqp_hbm<head,tail> */
	struct pqp_req_pgi {        /* PGI packet queue tracking (see also hbmI) */
		hndpkt_t    * link;     /* - PGI: hndpkt whose link needs to be set */
		uint16        save;     /* - PGI: saved pktid for fixup */
		uint16        buff;     /* - PGI: pending HBM free due to buffer xfer */
	} pgi;
	struct pqp_req_pgo {        /* PGO packet queue tracking */
		hndpkt_t    * head;     /* - PGO: Head D11 packet DMA in progress */
		hndpkt_t    * iter;     /* - PGO: Iterator of next D11 packet to DMA */
		hndpkt_t    * tail;     /* - PGO: Last D11 packet in queue */
	} pgo;
#if defined(HNDM2M)
	union pqp_req_m2m {
		struct pqp_xfer {       /* HNDM2M M2M DMA xfer (DMA DONE) pending */
			uint16    dmaO;     /* - m2m_dd D2H PKT PGO in progress */
			uint16    dmaI;     /* - m2m_dd H2D PKT PGI in progress */
		} xfer;
		uint32 u32;             /* - test against 0, for either dmaO or dmaI */
	} m2m;
#endif  /* HNDM2M */
	pqp_pgo_pcb_t     pcb;      /* - packet (context, buffer) has been Paged */
	pqp_pgi_qcb_t     qcb;      /* - entire Queue has been Paged */
};

typedef union pqp_hbm {         /* Host Memory Address */
	haddr64_t         haddr64;  /* dma64addr<lo,hi>. lo used as uintptr */
	uintptr           memptr;   /* uintptr arithmetic in PQP_HBM_PKTPTR */
	uint64            u64;
} pqp_hbm_t;

typedef struct pqp_mgr          /* Packet Queue Pager Manager */
{                               /* Double linked list of pqp_req */
	dll_t             dllO;     /* - list of PQP_OP_PGO requests to process */
	dll_t             dllI;     /* - list of PQP_OP_PGI requests to process */
	dll_t             dllH;     /* - requests paged out to Host (active) */
	dll_t             dllC;     /* - list of requests with DMA in progress */
	dll_t             dllF;     /* - free list of requests */
	                            /* Count of requests in dll */
	uint16            cntO;     /* - count of requests in dllO */
	uint16            cntI;     /* - count of requests in dllI */
	uint16            cntH;     /* - count of requests in dllH */
	uint16            cntC;     /* - count of requests in dllC */
	uint16            cntF;     /* - count of requests in dllF */
#if defined(HNDM2M)             /* HNDM2M DD User Registration Keys */
	m2m_dd_key_t      m2mO;     /* - PAGE OUT D2H M2M User registration key */
	m2m_dd_key_t      m2mI;     /* - PAGE IN  D2H M2M User registration key */
#endif  /* HNDM2M */
	pqp_hbm_t         hbmP;     /* Host Packet Pool base */
	pqp_hbm_t         hbmL;     /* Host Lcl Buffer Pool base */
	                            /* Multiword bitmap of Pkt and LCL buffers */
	pqp_map_t       * mapP;     /* - mwbmap of free host packets */
	pqp_map_t       * mapL;     /* - mwbmap of free host lcl buffers */
	uint16            cntP;     /* - available free host packets */
	uint16            cntL;     /* - available free host lcl buffers */
	                            /* System State */
	uint16            rsvd;
	uint16            size;     /* Total number of pqp_req_t in pool */
	PQP_STATS(pqp_stats_t pgo_stats); /* PQP_OP_PGO Statistics */
	PQP_STATS(pqp_stats_t pgi_stats); /* PQP_OP_PGI Statistics */
	                            /* Pool Manager */
	pqp_req_t       * pool;     /* - memory for pool of pqp_req_t */
	osl_t           * osh;

} pqp_mgr_t;

/**
 * +--------------------------------------------------------------------------+
 *  Packet Queue Pager Globals
 * +--------------------------------------------------------------------------+
 */
static
pqp_mgr_t pqp_mgr_g = {         /* global static initialization, see pqp_int */
	.dllO = DLL_STRUCT_INITIALIZER(pqp_mgr_g, dllO),
	.dllI = DLL_STRUCT_INITIALIZER(pqp_mgr_g, dllI),
	.dllH = DLL_STRUCT_INITIALIZER(pqp_mgr_g, dllH),
	.dllC = DLL_STRUCT_INITIALIZER(pqp_mgr_g, dllC),
	.dllF = DLL_STRUCT_INITIALIZER(pqp_mgr_g, dllF),
	.cntO = 0, .cntI = 0, .cntH = 0, .cntC = 0, .cntF = 0,
#if defined(HNDM2M)
	.m2mO = M2M_INVALID, .m2mI = M2M_INVALID,
#endif // endif
	.hbmP = { .u64 = 0 }, .hbmL = { .u64 = 0 },
	.mapP = PQP_MAP_NULL, .mapL = PQP_MAP_NULL, .cntP = 0, .cntL = 0,
	.size = 0,
#if defined(PQP_STATS_BUILD)
	.pgo_stats = { 0 }, .pgi_stats = { 0 },
#endif // endif
	.pool = PQP_REQ_NULL /* this field must be pre-initialized */
};

/**
 * +--------------------------------------------------------------------------+
 *  Section: Packet Queue Pager Functional Interface
 * +--------------------------------------------------------------------------+
 */

// Forward declarations of registered callbacks
#if defined(HNDM2M)
static void _pqp_pgo_m2m_dd_done_cb(void *cbdata,
                                   dma64addr_t *xfer_src, dma64addr_t *xfer_dst,
                                   int xfer_len, uint32 xfer_arg);
static void _pqp_pgo_m2m_dd_wake_cb(void *cbdata);

static void _pqp_pgi_m2m_dd_done_cb(void *cbdata,
                                   dma64addr_t *xfer_src, dma64addr_t *xfer_dst,
                                   int xfer_len, uint32 xfer_arg);
static void _pqp_pgi_m2m_dd_wake_cb(void *cbdata);
#endif  /* HNDM2M */

/**
 * +--------------------------------------------------------------------------+
 *  Section: Private PQP Helper Routine
 * +--------------------------------------------------------------------------+
 */

/**
 * XXX FIXME #1: Move these set of functions into appropriate locations.
 * - Remove all asserts. Pre-allocate 1 pkt, d11 and lcl.
 * - Register wakeup callback
 * - HWA not compiled: PKT_ID_FIXUP how?
 */
static uchar *
_hnd_d11get(void)
{
	uchar *d11_buf;
	d11_buf = lfbufpool_get(D11_LFRAG_BUF_POOL);
	ASSERT(d11_buf != NULL);
	return d11_buf;
} /* _hnd_d11get() */

static uchar *
_hnd_lclget(void)
{
	uchar * heap_buf = hnd_malloc(MAXPKTDATABUFSZ); // 1840
	ASSERT(heap_buf != NULL);
	return heap_buf;
}   /* _hnd_lclget() */

static hndpkt_t *
_hnd_pktget(void)
{
	hwapkt_t * hwapkt;
	hwapkt = hwa_txpost_txbuffer_get(hwa_dev);
	ASSERT(hwapkt != (hwapkt_t*)NULL);
	return (hndpkt_t*)HWAPKT2LFRAG((char *)hwapkt);
} /* _hnd_pktget() */

static INLINE void
__pqp_pkt_hbm_reset(hndpkt_t *pkt)
{
	PKT_HBM_PAGE(pkt)    = PQP_HBM_PAGE_INV;
	PKT_HBM_PKTLINK(pkt) = PQP_PKTID_NULL;
	PKT_HBM_PKTNEXT(pkt) = PQP_PKTID_NULL;
} /* __pqp_hbmpkt_reset() */

static void
_pqp_pktlist_hbm_reset(pqp_mgr_t *pqp_mgr,
                   hndpkt_t *head, hndpkt_t *tail, int n_pkts)
{
	hndpkt_t *next, *link;
	link = head;
	while (link != PQP_DPKT_NULL) {
		next = link;
		do {
			__pqp_pkt_hbm_reset(next);
			next = PKTNEXT(pqp_mgr->osh, next);
		} while (next != PQP_DPKT_NULL);
		n_pkts--;
		link = PKTLINK(link);
	}
	ASSERT(n_pkts == 0);
} /* _pqp_pktlist_hbm_reset() */

static INLINE pqp_req_t * /* Get a pktq_prec's paired pqp_req or PQP_REQ_NULL */
__pqp_req_pair(pktq_prec_t *pktq, hndpkt_t *head, hndpkt_t *tail)
{
	pqp_req_t * pqp_req = PQP_REQ_NULL;

	if (PKTQ_PKTS(pktq) == 0) {        /* no pqp_req had been paired */
		ASSERT(PKTQ_HEAD(pktq) == PQP_DPKT_NULL);
		ASSERT(PKTQ_TAIL(pktq) == PQP_DPKT_NULL);
	} else {
		pqp_req = PKTQ_PQP_REQ(pktq);  /* fetch pqp_req saved in pktq::head */
		ASSERT(pqp_req->pktq == pktq); /* audit back pointer pairing */
		ASSERT(pqp_req->flag != 0);    /* not in free list */
	}

	return pqp_req;                    /* may be PQP_REQ_NULL */
} /* __pqp_req_pair() */

static void /* Default No-OP Packet Callback : see pqp_pgo_pcb_t */
_pqp_pgo_pcb_none(pktq_prec_t *pktq, hndpkt_t *pkt)
{
	PQP_STATS(pqp_mgr_g.pgo_stats.fail++);
	ASSERT(0);
} /* _pqp_pgo_pcb_none() */

static void /* Default No-OP Queue Callback : see pqp_qcb_t */
_pqp_pgi_qcb_none(pktq_prec_t *pktq)
{
	PQP_STATS(pqp_mgr_g.pgi_stats.fail++);
	ASSERT(0);
} /* _pqp_pgi_qcb_none() */

static INLINE uint16 /* Allocate a free id from the PQP Manager's hbmP mwbmap */
__pqp_hbm_pktid_alloc(pqp_mgr_t *pqp_mgr)
{
	uint16 hbm_pktid;
	PQP_ASSERT(pqp_mgr->cntP > 0);
	hbm_pktid = (uint16)bcm_mwbmap_alloc(pqp_mgr->mapP);
	PQP_ASSERT_HBM_ID(hbm_pktid);
	pqp_mgr->cntP--;
	return hbm_pktid;
} /* __pqp_hbm_pktid_alloc() */

static INLINE void /* Free a HBM pktid int the PQP Manager's hbmL mwbmap */
__pqp_hbm_pktid_free(pqp_mgr_t *pqp_mgr, uint16 hbm_pktid)
{
	PQP_ASSERT_HBM_ID(hbm_pktid);
	bcm_mwbmap_free(pqp_mgr->mapP, hbm_pktid);
	pqp_mgr->cntP++;
} /* __pqp_hbm_pktid_free() */

static INLINE uint16 /* Allocate a free id from the PQP Manager's hbmL mwbmap */
__pqp_hbm_lclid_alloc(pqp_mgr_t *pqp_mgr)
{
	uint16 hbm_lclid;
	PQP_ASSERT(pqp_mgr->cntL > 0);
	hbm_lclid = (uint16)bcm_mwbmap_alloc(pqp_mgr->mapL);
	PQP_ASSERT_HBM_ID(hbm_lclid);
	pqp_mgr->cntL--;
	return hbm_lclid;
} /* __pqp_hbm_lclid_alloc() */

static INLINE void /* Free a HBM lclid int the PQP Manager's hbmL mwbmap */
__pqp_hbm_lclid_free(pqp_mgr_t *pqp_mgr, uint16 hbm_lclid)
{
	PQP_ASSERT_HBM_ID(hbm_lclid);
	bcm_mwbmap_free(pqp_mgr->mapL, hbm_lclid);
	pqp_mgr->cntL++;
} /* __pqp_hbm_lclid_free() */

static INLINE void /* Reset a pqp request and insert into pqp mgr's free list */
__pqp_req_free(pqp_mgr_t *pqp_mgr, pqp_req_t *pqp_req)
{
	pqp_req->pktq       = PQP_PKTQ_NULL;     /* not paired with a pktq */
	pqp_req->pgo.head   = PQP_DPKT_NULL;     /* PGO head dngl packet invalid */
	pqp_req->pgo.iter   = PQP_DPKT_NULL;     /* PGO itererator invalid */
	pqp_req->pgo.tail   = PQP_DPKT_NULL;     /* PGO tail dngl packet invalid */
	pqp_req->flag       = 0;                 /* pqp_req is free: no PAGE OP */
	pqp_req->hbmI       = PQP_HBM_ID_INV;    /* PGI hbm iterator invalid */
	pqp_req->pgi.link   = PQP_DPKT_NULL;     /* PGI link iterator invalid */
	pqp_req->pgi.save   = PQP_PKTID_NULL;    /* PGI saved pktid invalid */
	pqp_req->pgi.buff   = PQP_PKTID_NULL;    /* PGI buffer xfer pending */
#if defined(HNDM2M)
	PQP_REQ_XFER(pqp_req) = 0U;              /* m2m.xfer.<dmaO,dmaI> = 0 */
#endif // endif
	pqp_req->pcb        = _pqp_pgo_pcb_none; /* noop packet paged callback */
	pqp_req->qcb        = _pqp_pgi_qcb_none; /* noop queue  paged callback */

	/* append to free list */
	dll_append(&pqp_mgr->dllF, &pqp_req->node);   pqp_mgr->cntF++;
} /* __pqp_req_free() */

// TBD. Asynchronous Resume Callbacks
// - callback when descriptors become available: (PAGE_OUT = CH0, PAGE_IN = CH1)
// - callback when HWA TxBM LFrag / D11 become available for PAGE_IN
// - callback when LCL become available.
//   With HWA2.3 PktPgr, can we retain LCL in Host .. would PP try to copy H2H?

/**
 * +--------------------------------------------------------------------------+
 *  Section: Public PQP Helper Routine
 * +--------------------------------------------------------------------------+
 */

pqp_op_t /* Query whether a pktq_prec_t is managed by PQP */
pqp_pop(pktq_prec_t *pktq)
{
	pqp_op_t   pqp_op_ret;
	pqp_req_t *pqp_req;
	pqp_mgr_t *pqp_mgr = &pqp_mgr_g;

	ASSERT(pktq != PQP_PKTQ_NULL);
	pqp_req = PKTQ_PQP_REQ(pktq);

	if (PQP_MGR_IS_PQP_REQ(pqp_mgr, pqp_req)) { /* is in pqp_mgr's pool */
		ASSERT(pqp_req->pktq == pktq); /* confirm back pointer */
		pqp_op_ret = (pqp_op_t)(pqp_req->flag & PQP_REQ_FLAG_POP);
	} else {
		pqp_op_ret = (pqp_op_t)PQP_OP_NOP; /* not managed by PQP */
	}

	return pqp_op_ret;
} /* pqp_pop() */

/**
 * +--------------------------------------------------------------------------+
 *  Section: PQP Functional Interface
 * +--------------------------------------------------------------------------+
 */
void /* Debug dump the internal PQP runtime state */
pqp_dump(bool verbose)
{
	pqp_mgr_t *pqp_mgr = &pqp_mgr_g;

	PQP_PRINT("PQP: %p" PQP_MGR_FMT, pqp_mgr, PQP_MGR_VAL(pqp_mgr));

	if (verbose) {
		dll_t *iter;
		PQP_PRINT("DLL" PQP_REQ_HDR);
		dll_for_each(iter, &pqp_mgr->dllO) { /* pending PAGE-OUT requests */
			PQP_PRINT("PGO" PQP_REQ_HFMT, PQP_REQ_VAL(iter));
		}
		dll_for_each(iter, &pqp_mgr->dllI) { /* pending PAGE-IN requests */
			PQP_PRINT("PGI" PQP_REQ_HFMT, PQP_REQ_VAL(iter));
		}
		dll_for_each(iter, &pqp_mgr->dllH) { /* requests ACTIVE in host */
			PQP_PRINT("ACT" PQP_REQ_HFMT, PQP_REQ_VAL(iter));
		}
		dll_for_each(iter, &pqp_mgr->dllC) { /* requests pend DMA DONE clear */
			PQP_PRINT("CLR" PQP_REQ_HFMT, PQP_REQ_VAL(iter));
		}
		if (pqp_mgr->mapP) bcm_mwbmap_show(pqp_mgr->mapP);
		if (pqp_mgr->mapL) bcm_mwbmap_show(pqp_mgr->mapL);
	}
} /* pqp_dump() */

void /* Debug dump the internal PQP runtime statistics */
pqp_stats(void)
{
#if defined(PQP_STATS_BUILD)
	pqp_op_t pop;
	pqp_stats_t *stats;
	for (pop = PQP_OP_PGO; pop <= PQP_OP_PGI; pop++) {
		stats = (pop == PQP_OP_PGO) ?
		        &pqp_mgr_g.pgo_stats : &pqp_mgr_g.pgi_stats;
		PQP_PRINT("PQP Statistics PQP_OP_PG%c",
		          (pop == PQP_OP_PGO) ? 'O' : 'I');
		PQP_PRINT("\tRequests  : %8u", stats->req);
		PQP_PRINT("\tCallbacks : %8u", stats->cbk);
		PQP_PRINT("\tPackets   : %8u", stats->pkt);
		PQP_PRINT("\tD11 Xfers : %8u", stats->d11);
		PQP_PRINT("\tLCL Xfers : %8u", stats->lcl);
		PQP_PRINT("\tM2M Xfers : %8u", stats->xfer);
#if defined(HNDM2M)
		PQP_PRINT("\tDMA Dones : %8u", stats->done);
#endif // endif
		PQP_PRINT("\tStalls    : %8u", stats->pend);
		PQP_PRINT("\tWakeups   : %8u", stats->wake);
		PQP_PRINT("\tFailures  : %8u", stats->fail);
	}
#endif  /* PQP_STATS_BUILD */
} /* pqp_stats() */

int /* Dettach the PQP subsystem */
BCMATTACHFN(pqp_fini)(osl_t *osh)
{
	pqp_mgr_t *pqp_mgr = &pqp_mgr_g;

	dll_init(&pqp_mgr->dllO); pqp_mgr->cntO = 0;
	dll_init(&pqp_mgr->dllI); pqp_mgr->cntI = 0;
	dll_init(&pqp_mgr->dllH); pqp_mgr->cntH = 0;
	dll_init(&pqp_mgr->dllC); pqp_mgr->cntC = 0;
	dll_init(&pqp_mgr->dllF); pqp_mgr->cntF = 0;

	if (pqp_mgr->pool) {
		ASSERT(pqp_mgr->size != 0);
		memset(pqp_mgr->pool, 0,  sizeof(pqp_req_t) * pqp_mgr->size);
		MFREE(osh, pqp_mgr->pool, sizeof(pqp_req_t) * pqp_mgr->size);
		pqp_mgr->pool = PQP_REQ_NULL;
	}
	pqp_mgr->size = 0;

	if (pqp_mgr->mapP) bcm_mwbmap_fini(osh, pqp_mgr->mapP);
	if (pqp_mgr->mapL) bcm_mwbmap_fini(osh, pqp_mgr->mapL);
	pqp_mgr->mapP = pqp_mgr->mapL = PQP_MAP_NULL;
	pqp_mgr->cntP = pqp_mgr->cntL = 0;

	return BCME_OK;

} /* pqp_fini() */

int /* Initialize the PQP subsystem. */
BCMATTACHFN(pqp_init)(osl_t *osh, int pqp_req_max)
{
	int idx;
	pqp_req_t *pqp_req;
	pqp_mgr_t *pqp_mgr = &pqp_mgr_g;

	if (pqp_mgr->pool != PQP_REQ_NULL) return BCME_OK; /* singleton global */

	ASSERT(pqp_req_max != 0);

	/* Allocate memory for a pool of pqp requests */
	pqp_mgr->pool = (pqp_req_t *)MALLOCZ(osh, sizeof(pqp_mgr_t) * pqp_req_max);
	if (pqp_mgr->pool == PQP_REQ_NULL) {
		PQP_ERROR("PQP: FAILURE alloc %u req %u bytes",
			pqp_req_max, sizeof(pqp_mgr_t));
		ASSERT(pqp_mgr->pool != PQP_REQ_NULL);
		pqp_mgr->pool = PQP_REQ_NULL;
		goto fail;
	}

	/* Register with DMA Engine channels */
#if defined(HNDM2M)
	/* M2M CH0 is D2H (PAGEOUT). M2M CH1 is H2D (PAGEIN) */
	pqp_mgr->m2mO = m2m_dd_usr_register(M2M_DD_PGO, M2M_DD_CH0,
	                  pqp_mgr, _pqp_pgo_m2m_dd_done_cb,
	                  _pqp_pgo_m2m_dd_wake_cb, PQP_MGR_M2M_DD_CH0_THRESH);
	pqp_mgr->m2mI = m2m_dd_usr_register(M2M_DD_PGI, M2M_DD_CH1,
	                  pqp_mgr, _pqp_pgi_m2m_dd_done_cb,
	                  _pqp_pgi_m2m_dd_wake_cb, PQP_MGR_M2M_DD_CH1_THRESH);
	PQP_PRINT("PQP: m2m_dd_usr_register M2M_DD_PGO %08x", pqp_mgr->m2mO);
	PQP_PRINT("PQP: m2m_dd_usr_register M2M_DD_PGI %08x", pqp_mgr->m2mI);
	if ((pqp_mgr->m2mO == M2M_INVALID) || (pqp_mgr->m2mI == M2M_INVALID)) {
		PQP_ERROR(
			"PQP: FAILURE m2m_dd_usr_register M2M_DD_PGO %08x M2M_DD_PGI %08x",
			pqp_mgr->m2mO, pqp_mgr->m2mI);
		goto fail;
	}
#endif  /* HNDM2M */

	/* Setup multi-word bitmap for free host packets and free host buffers */
	pqp_mgr->mapP = bcm_mwbmap_init(osh, PQP_PKT_MAX);
	pqp_mgr->mapL = bcm_mwbmap_init(osh, PQP_LCL_MAX);
	if ((pqp_mgr->mapP == PQP_MAP_NULL) || (pqp_mgr->mapL == PQP_MAP_NULL)) {
		PQP_ERROR("PQP: FAILURE <map,max> mapP %p,%u mapL %p,%u",
			pqp_mgr->mapP, PQP_PKT_MAX, pqp_mgr->mapL, PQP_LCL_MAX);
		ASSERT(pqp_mgr->mapP && pqp_mgr->mapL);
		goto fail;
	}
	pqp_mgr->cntP = PQP_PKT_MAX;    /* available free host packets */
	pqp_mgr->cntL = PQP_LCL_MAX;    /* available free host buffers */

	/* skip packet and buffer at index 0 */
	PQP_DEBUG({ bcm_mwbmap_force(pqp_mgr->mapP, 0); pqp_mgr->cntP--; });
	PQP_DEBUG({ bcm_mwbmap_force(pqp_mgr->mapL, 0); pqp_mgr->cntL--; });

	/* Initialize pending PGO, PGI, CLR, PAGED  and free list */
	dll_init(&pqp_mgr->dllO); pqp_mgr->cntO = 0;
	dll_init(&pqp_mgr->dllI); pqp_mgr->cntI = 0;
	dll_init(&pqp_mgr->dllH); pqp_mgr->cntH = 0;
	dll_init(&pqp_mgr->dllC); pqp_mgr->cntC = 0;
	dll_init(&pqp_mgr->dllF); pqp_mgr->cntF = 0;

	/* Place all pqp request objects into pqp manager's free list */
	for (idx = 0; idx < pqp_req_max; ++idx) {
		pqp_req = pqp_mgr->pool + idx;
		__pqp_req_free(pqp_mgr, pqp_req); /* reset and place into free list */
	}
	pqp_mgr->size = pqp_req_max; /* total free = pool size */

	pqp_mgr->osh  = osh;

	PQP_PRINT("Packet Queue Pager Initialized with %u pages", pqp_mgr->size);

	return BCME_OK;

fail:
	(void)pqp_fini(osh);

	return BCME_NOMEM;

} /* pqp_init() */

int /* Configure HME pages for Packet Storage and LCL Buffer Storage */
pqp_bind_hme(void)
{
	int ret = BCME_OK;
	PQP_DEBUG(PQP_PRINT("PQP: Bind HME pages PSQPKT %u LCLPKT %u\n",
		PCIE_IPC_HME_PAGES(PQP_PKT_SZ * PQP_PKT_MAX),
		PCIE_IPC_HME_PAGES(PQP_LCL_SZ * PQP_LCL_MAX)));

	ret += hme_attach_mgr(HME_USER_PSQPKT,
	                      PCIE_IPC_HME_PAGES(PQP_PKT_SZ * PQP_PKT_MAX),
	                      PQP_PKT_SZ, PQP_PKT_MAX, HME_MGR_NONE);
	ret += hme_attach_mgr(HME_USER_LCLPKT,
	                      PCIE_IPC_HME_PAGES(PQP_LCL_SZ * PQP_LCL_MAX),
	                      PQP_LCL_SZ, PQP_LCL_MAX, HME_MGR_NONE);
	PQP_ASSERT(ret == BCME_OK);

	return ret;
} /* pqp_bind_hme() */

int /* Link HME memory segments for Packet Storage and Lcl Buffer Storage */
pqp_link_hme(void)
{
	pqp_mgr_t *pqp_mgr = &pqp_mgr_g;

	PQP_ASSERT(pqp_mgr->pool != PQP_REQ_NULL);

	pqp_mgr->hbmP.haddr64 = hme_get(HME_USER_PSQPKT, PQP_PKT_SZ * PQP_PKT_MAX);
	pqp_mgr->hbmL.haddr64 = hme_get(HME_USER_LCLPKT, PQP_LCL_SZ * PQP_LCL_MAX);

	PQP_DEBUG(PQP_PRINT("PQP: Link HME PKT" HADDR64_FMT " LCL" HADDR64_FMT "\n",
		HADDR64_VAL(pqp_mgr->hbmP.haddr64),
		HADDR64_VAL(pqp_mgr->hbmL.haddr64)));

	return BCME_OK;
} /* pqp_link_hme() */

/**
 * +--------------------------------------------------------------------------+
 *  Section: PQP PAGE OUT Implementation
 * +--------------------------------------------------------------------------+
 */
static INLINE void /* PQP PGO helper routine to invoke D2H DMA transfer */
__pqp_pgo_m2m_dd_xfer(pqp_mgr_t *pqp_mgr,
               uintptr xfer_src_lo, uintptr xfer_dst_lo, uintptr xfer_dst_hi,
               int xfer_len, uint32 xfer_arg, uint32 xfer_op)
{
#if defined(HNDM2M)
	m2m_dd_cpy_t m2m_dd_cpy;
	dma64addr_t m2m_dd_src, m2m_dd_dst;

	m2m_dd_src.lo   = xfer_src_lo;
	m2m_dd_src.hi   = 0U;
	m2m_dd_dst.lo   = xfer_dst_lo;
	m2m_dd_dst.hi   = xfer_dst_hi;
	m2m_dd_cpy      = m2m_dd_xfer(pqp_mgr->m2mO, &m2m_dd_src, &m2m_dd_dst,
	                              xfer_len, xfer_arg, xfer_op);
	if (m2m_dd_cpy == M2M_INVALID) {
		PQP_PRINT("PQP: PGO m2m_dd_xfer[%08x->%08x] len %4u arg %08x failure\n",
			m2m_dd_src.lo, m2m_dd_dst.lo, xfer_len, xfer_arg);
		PQP_STATS(pqp_mgr->pgo_stats.fail++);
		ASSERT(0);
	}

	PQP_STATS(pqp_mgr->pgo_stats.xfer++);
#endif  /* HNDM2M */

} /* __pqp_pgo_m2m_dd_xfer() */

static INLINE void /* PQP PGO D2H DMA transfer attached D11 data */
__pqp_pgo_xfer_d11(pqp_mgr_t *pqp_mgr, hndpkt_t *pkt)
{
	uint16  hbm_d11id   = PKT_HBM_PKTID(pkt);  /* D11 saved in Host BM packet */
	uintptr xfer_src_lo = (uintptr)PKTDATA(pqp_mgr->osh, pkt);
	uintptr xfer_dst_lo = PQP_HBM_D11PTR(pqp_mgr, hbm_d11id)
	                    + PKTHEADROOM(pqp_mgr->osh, pkt);
	uintptr xfer_dst_hi = HADDR64_HI(pqp_mgr->hbmP.haddr64); /* HBM PKT POOL */
	int     xfer_len    = (int)PKTLEN(pqp_mgr->osh, pkt);
	uint32  xfer_arg    = 0U;

	__pqp_pgo_m2m_dd_xfer(pqp_mgr, xfer_src_lo, xfer_dst_lo,
		xfer_dst_hi, xfer_len, xfer_arg, PQP_M2M_DD_XFER_NOOP);

	PQP_STATS(pqp_mgr->pgo_stats.d11++);

} /* __pqp_pgo_xfer_d11() */

static INLINE void /* PQP PGO D2H DMA transfer attached LCL data */
__pqp_pgo_xfer_lcl(pqp_mgr_t *pqp_mgr, hndpkt_t *pkt)
{
	uint16  hbm_lclid   = PKT_HBM_LCLID(pkt);
	uintptr xfer_src_lo = (uintptr)PKTDATA(pqp_mgr->osh, pkt);
	uintptr xfer_dst_lo = PQP_HBM_LCLPTR(pqp_mgr, hbm_lclid)
	                    + PKTHEADROOM(pqp_mgr->osh, pkt);
	uintptr xfer_dst_hi = HADDR64_HI(pqp_mgr->hbmL.haddr64); /* HBM LCL POOL */
	int     xfer_len    = (int)PKTLEN(pqp_mgr->osh, pkt);
	uint32  xfer_arg    = 0U;

	__pqp_pgo_m2m_dd_xfer(pqp_mgr, xfer_src_lo, xfer_dst_lo,
		xfer_dst_hi, xfer_len, xfer_arg, PQP_M2M_DD_XFER_NOOP);

	PQP_STATS(pqp_mgr->pgo_stats.lcl++);

} /* __pqp_pgo_xfer_lcl() */

static INLINE void /* PQP PGO D2H DMA transfer hndpkt */
__pqp_pgo_xfer_pkt(pqp_mgr_t *pqp_mgr, hndpkt_t *pkt,
              pqp_req_t *pqp_req, bool commit)
{
	uint16  hbm_pktid   = PKT_HBM_PKTID(pkt); /* already allocated */
	uintptr xfer_src_lo = (uintptr)HND2HBMPKT(pkt);
	uintptr xfer_dst_lo = PQP_HBM_PKTPTR(pqp_mgr, hbm_pktid);
	uintptr xfer_dst_hi = HADDR64_HI(pqp_mgr->hbmP.haddr64); /* HBM PKT POOL */
	int     xfer_len    = HBMPKT_SIZE;
	uint32  xfer_arg    = (uint32)((uintptr)pqp_req);
	uint32 xfer_op      = (commit == FALSE) ?
	                      PQP_M2M_DD_XFER_NOOP : PQP_M2M_DD_XFER_SYNC;

	__pqp_pgo_m2m_dd_xfer(pqp_mgr, xfer_src_lo, xfer_dst_lo,
		xfer_dst_hi, xfer_len, xfer_arg, xfer_op);

	PQP_STATS(pqp_mgr->pgo_stats.pkt++);

} /* __pqp_pgo_xfer_pkt() */

/**
 * +--------------------------------------------------------------------------+
 *
 *  PAGEOUT Algorithm: (see entry point pqp_pgo)
 *
 *  pqp_pgo() uses _pqp_pgo_iterate() to iterate through MPDUs in a PktQ and
 *  PAGEOUT each MPDU (list of MSDU lfrags and data buffers).
 *
 *  1.  Determine the end of iteration (i.e. "last"):
 *        Tail packet is not PAGED out for SuprPSQ to allow append to tail.
 *        Tail packet in PSQ is PAGED OUT (! append_policy)
 *  2.  Iterate upto (not including) last.
 *  2.1   Determine whether sufficient resources are available to service MPDU.
 *        Host Packet BM free count and M2M DD free count are compared against
 *        their respective thresholds (set to around 16)
 *        If resources are not available, the pqp_req is retained at the head of
 *        the PGO dllO, with pqp_req::iter pointing to the MPDU pending
 *        PAGEOUT and PQP Manager state is updated. Iteration bails.
 *        When resources become available (callback), the PQP MGR will re-enter
 *        iteration().
 *        Academic paranoia: Toggling between M2M D2H and Host BM.
 *  2.2   Allocate a Host BM Packet for current if a Host BM Packet was not yet
 *        assigned. Save the allocated Host BM index in curr.
 *  2.3   If current MPDU is link-ed to another MPDU, then allocate a Host BM
 *        packet for the link-ed MPDU and save into link-ed MPDU.
 *        Save the Host BM Packet pointer in the curr packet's HBM_LINK field.
 *        Original lbuf::linkid is left as-is (i.e. not relocated).
 *  2.4   Iterate through list using PKTNEXT
 *  2.4.1   If curr packet has a lfrag for next MSDU, then allocate a Host BM
 *          packet for the next lfrag and save Host BM index into next lfrag.
 *          Save the Host BM Packet pointer in the curr packet's HBM_NEXT.
 *          Original lbuf::next_id is left as-is (i.e. not relocated).
 *  2.4.2   If curr packet has a heap allocated buffer (2 KByte) LCL data buffer
 *          then allocate a Host BM LCL and xfer LCL.
 *          If Host BM LCL pool is exhausted then dongle LCL is retained.
 *          see __pqp_pgo_xfer_lcl().
 *          In HWA2.2, driver may have pre-transferred LCL buffers into a HME
 *          pool not managed by PQP.
 *          NOTE: hndpkt<head,data,end> are NOT relocated.
 *  2.4.3   If curr packet has a D11 pool allocated data buffer (43684), then
 *          commence data transfer to the data portion of the Host BM packet.
 *          Data buffer is copied into HBM PKT's data section.
 *          see __pqp_pgo_xfer_d11().
 *          In HWA2.2, there is no need to commence a d11 data transfer, as the
 *          data buffer is contiguous with the packet context.
 *          NOTE: hndpkt<head,data,end> are NOT relocated.
 *  2.4.4   M2M D2H Transfer curr lfrag.
 *          First lfrag in a msdu list is transferred last, where in the M2M
 *          XmtPtr is advanced and _pqp_pgo_m2m_dd_done_cb() is registered.
 *          see __pqp_pgo_xfer_pkt().
 *  3.  If iter points to last, then move pqp_req from dllO to dllH and select
 *      next pqp_req in dllO
 *
 * +--------------------------------------------------------------------------+
 */
static void /* Iterate through MPDUs, paging out all MSDUs per MPDU upto tail */
_pqp_pgo_iterate(pqp_mgr_t *pqp_mgr, pqp_req_t *pqp_req)
{
	osl_t     * osh     = pqp_mgr->osh;
	hndpkt_t  * last;
#if defined(HNDM2M)
	uint32 m2m_dd_cnt   = m2m_dd_avail(pqp_mgr->m2mO);
#define M2M_DD_CNT_DEC  m2m_dd_cnt--
#else
#define M2M_DD_CNT_DEC  PQP_NOOP
#endif // endif

pqp_req_iterate:

	/* #1. Tail MPDU is not paged in SuprPSQ to allow for future tail append */
	last = (pqp_req->flag & PQP_REQ_FLAG_APP) ?
	       pqp_req->pgo.tail : PQP_DPKT_NULL;

	/* #2. Iterate through PKTLINK list upto last */
	while (pqp_req->pgo.iter != last)
	{
		hndpkt_t *curr, *link;

		/* #2.1 Ensure resources are available for entire MPDU */
		if (pqp_mgr->cntP < PQP_MGR_HBMPKT_THRESH) { /* Host Packet BM */
			PQP_DEBUG(PQP_PRINT("PQP: PGO pqp_req %p HBM pending", pqp_req));
			pqp_req->flag |= PQP_REQ_FLAG_HBM;
			goto pend_noresource;
		}
#if defined(HNDM2M)
		if (m2m_dd_cnt < PQP_MGR_M2M_DD_CH0_THRESH) {
			PQP_DEBUG(PQP_PRINT("PQP: PGO pqp_req %p D2H pending", pqp_req));
			pqp_req->flag |= PQP_REQ_FLAG_D2H;
			m2m_dd_wake_request(pqp_mgr->m2mO); /* toggle m2m WAKE up notif */
			goto pend_noresource;
		}
#endif  /* HNDM2M */
		/* PAGEOUT of LCL is best effort */

		curr = pqp_req->pgo.iter;
		link = PKTLINK(curr);

		/* #2.2 Assign a Host BM index for current lfrag */
		if (PKT_HBM_PKTID(curr) == PQP_HBM_ID_INV) {
			PKT_HBM_PKTID(curr) = __pqp_hbm_pktid_alloc(pqp_mgr);
		}

		/* #2.3 Assign Host BM index for link-ed packet. Set PKT_HBM_PKTLINK */
		if (link != PQP_DPKT_NULL) {
			ASSERT(PKT_HBM_PKTID(link) == PQP_HBM_ID_INV);
			PKT_HBM_PKTID(link)   = __pqp_hbm_pktid_alloc(pqp_mgr);
			PKT_HBM_PKTLINK(curr) = PKT_HBM_PKTID(link);
		} else {
			PKT_HBM_PKTLINK(curr) = PQP_PKTID_NULL;
		}

		/* #2.4 Iterate the PKTNEXT list of lfrags that carry MSDUs */
		do {

			hndpkt_t *next = PKTNEXT(pqp_mgr->osh, curr);

			/* #2.4.1 Assign Host BM index for next pkt. Set PKT_HBM_PKTNEXT */
			if (next != PQP_DPKT_NULL) {
				uint16 hbm_pktid = __pqp_hbm_pktid_alloc(pqp_mgr);
				ASSERT(PKT_HBM_PKTID(next) == PQP_HBM_ID_INV);
				PKT_HBM_PKTID(next)   = hbm_pktid;
				PKT_HBM_PKTNEXT(curr) = hbm_pktid;
			} else {
				PKT_HBM_PKTNEXT(curr) = PQP_PKTID_NULL;
			}

			if (PKTISBUFALLOC(osh, curr))
			{
				/* #2.4.2 Transfer LCL data buffer, if HBM LCL is available */
				if (PKTHASHEAPBUF(osh, curr))
				{
					if (pqp_mgr->cntL > 0)
					{
						PKT_HBM_LCLID(curr)
							= __pqp_hbm_lclid_alloc(pqp_mgr);
						__pqp_pgo_xfer_lcl(pqp_mgr, curr);
					} else {
						ASSERT(0); // FIXME: Best Effort
					}
				} else { /* #2.4.3 Transfer D11 data buffer to host BM Packet */
					__pqp_pgo_xfer_d11(pqp_mgr, curr);
				}
				M2M_DD_CNT_DEC;
			}

			/* #2.4.4 Transfer curr packet to host, with DMA done callback */
			if (curr != pqp_req->pgo.iter) {
				__pqp_pgo_xfer_pkt(pqp_mgr, curr, pqp_req, FALSE);
				M2M_DD_CNT_DEC;
			}

			curr = next;

		} while (curr != PQP_DPKT_NULL);

		/* #2.4.4 Transfer first lfrag of msdu list, COMMIT + RESUME */
		__pqp_pgo_xfer_pkt(pqp_mgr, pqp_req->pgo.iter, pqp_req, TRUE);
		M2M_DD_CNT_DEC;
#if defined(HNDM2M)
		PQP_REQ_DMAO(pqp_req) += 1; /* Sync DMA done_cb on entire D11 packet */
#endif // endif

		pqp_req->pgo.iter = link; /* set pqp_req::iter to link-ed mpdu */

	} /* iterate mpdus, until last <tail|NULL> */

	if (pqp_req->pgo.iter == last) { /* done with pqp_req at head of dllO */
		dll_delete(&pqp_req->node);                 pqp_mgr->cntO--;
		dll_append(&pqp_mgr->dllH, &pqp_req->node); pqp_mgr->cntH++;
		pqp_req->flag |= PQP_REQ_FLAG_ACT;  /* to dllH */
		if (!dll_empty(&pqp_mgr->dllO)) {
			PQP_ASSERT(pqp_mgr->cntO != 0);
			pqp_req = PQP_REQ_DLL2PTR(dll_head_p(&pqp_mgr->dllO));
			goto pqp_req_iterate;
		}
	}

	return;

pend_noresource: /* PQP PGO request is pending resources, bail out */

	PQP_STATS(pqp_mgr->pgo_stats.pend++);
	return;

} /* _pqp_pgo_iterate() */

#if defined(HNDM2M)
static void /* Notification invoked by M2M DD D2H engine of DD availability */
_pqp_pgo_m2m_dd_wake_cb(void *cbdata)
{
	pqp_req_t * pqp_req;
	pqp_mgr_t * pqp_mgr = &pqp_mgr_g;

	PQP_STATS(pqp_mgr->pgo_stats.wake++);

	PQP_ASSERT(pqp_mgr == (pqp_mgr_t*)cbdata);

	/* Head PQP_REQ pending D2H resources may have been cancelled. */
	if (dll_empty(&pqp_mgr->dllO)) return;

	pqp_req = PQP_REQ_DLL2PTR(dll_head_p(&pqp_mgr->dllO)); /* wake pqp_req */

	/* Apply WAKE to head pqp_req, even if its flag is not denoting wake */
	PQP_DEBUG(PQP_PRINT("PQP: " PQP_REQ_FMT, PQP_REQ_VAL(pqp_req)));
	pqp_req->flag &= ~PQP_REQ_FLAG_D2H;

	/* PAGEOUT packets from iter upto tail. */
	_pqp_pgo_iterate(pqp_mgr, pqp_req);

} /* _pqp_pgo_m2m_dd_wake_cb() */

static INLINE void /* Cancel a previous PGO PKT DMA done, due to PGI op */
__pqp_pgo_pkt_abort(pqp_mgr_t *pqp_mgr, pqp_req_t *pqp_req,
                    pktq_prec_t *pktq, hndpkt_t *pkt)
{
	uint16 hbm_pktid, hbm_lclid;

	do /* Return all HBM resources for HBM PKT and HBM LCL */
	{
		hbm_pktid          = PKT_HBM_PKTID(pkt);          /* fetch HBM PKT */
		PKT_HBM_PKTID(pkt) = PQP_HBM_ID_INV;              /* dettach HBM PKT */
		__pqp_hbm_pktid_free(pqp_mgr, hbm_pktid);         /* free HBM PKT */
		// PKT_HBM_PKTNEXT(pkt) = PQP_HPKT_NULL;
		// PKT_HBM_PKTLINK(pkt) = PQP_HPKT_NULL;
		if (PKTHASHEAPBUF(pqp_mgr->osh, pkt)) {           /* with LCL BUF? */
			hbm_lclid = PKT_HBM_LCLID(pkt);               /* fetch HBM LCL */
			if (hbm_lclid != PQP_HBM_ID_INV) {            /* if valid? */
				PKT_HBM_LCLID(pkt) = PQP_HBM_ID_INV;      /* dettach HBM LCL */
				__pqp_hbm_lclid_free(pqp_mgr, hbm_lclid); /* free HBM LCL */
			}
		}
		/* PKTHEAD, PKTDATA, PKTEND do not need to be relocated */
		pkt = PKTNEXT(pqp_mgr->osh, pkt);                 /* next lfrag */

	} while (pkt != PQP_DPKT_NULL);                       /* PKTNEXT list end */

	if (pqp_req->pgo.head == pqp_req->pgo.iter)           /* all PGO DMA done */
	{
		PQP_ASSERT(PQP_REQ_DMAO(pqp_req) == 0);           /* dmaO cnt == 0 */
		if (pqp_req->pgo.iter != PQP_DPKT_NULL) {         /* iter is link pkt */
			hbm_pktid = PKT_HBM_PKTID(pqp_req->pgo.iter); /* iter's HBM PKT */
			__pqp_hbm_pktid_free(pqp_mgr, hbm_pktid);     /* free HBM PKT */
		}

		/* If no PGO or PGI DMA in progress, and no Host BM packets to PGI */
		if ((PQP_REQ_XFER(pqp_req) == 0U) &&               /* no dmaO or dmaI */
		    (pqp_req->flag & PQP_REQ_FLAG_QCB))            /* all PGI done */
		{
			PQP_ASSERT(PKTQ_TAIL(pktq) == (void*)pqp_req->pgo.tail);
			dll_delete(&pqp_req->node); pqp_mgr->cntC--;  /* from dllC */
			PQP_STATS(pqp_mgr->pgi_stats.cbk++);
			pqp_req->qcb(pktq);                           /* user QUEUE cb */
			__pqp_req_free(pqp_mgr, pqp_req); /* dettach pktq, free to dllF */
		}
	}

} /* __pqp_pgo_pkt_abort() */

/**
 * +--------------------------------------------------------------------------+
 * _pqp_pgo_m2m_dd_done_cb() is the callback registered with the D2H M2M engine.
 * As only PKT M2M DMA requests are colored with M2M_DD_XFER_RESUME, the
 * callback will be invoked only when an entire PACKET DMA has completed. This
 * includes all data buffers and LFrags linked using PKTNEXT.
 * This callback advances the pqp_req::head and sets up pktq_prec:<hidx,tidx>.
 * The user registered pqp_pgo_pcb_t callback that is responsible for freeing
 * all packet resources is invoked.
 * +--------------------------------------------------------------------------+
 */
static void
_pqp_pgo_m2m_dd_done_cb(void *cbdata,
    dma64addr_t *xfer_src, dma64addr_t *xfer_dst, int xfer_len, uint32 xfer_arg)
{
	uint16       hbm_pktid;
	pqp_mgr_t   *pqp_mgr    = &pqp_mgr_g;
	pqp_req_t   *pqp_req    = (pqp_req_t*)xfer_arg;
	pktq_prec_t *pktq       = pqp_req->pktq;
	hndpkt_t    *pkt        = pqp_req->pgo.head;

	PQP_STATS(pqp_mgr->pgo_stats.done++);

	PQP_ASSERT(pqp_mgr == (pqp_mgr_t*)cbdata);
	PQP_ASSERT(PQP_MGR_IS_PQP_REQ(pqp_mgr, pqp_req));

	ASSERT(PQP_REQ_DMAO(pqp_req) > 0);
	PQP_REQ_DMAO(pqp_req)  -= 1; /* done_cb invoked per D11 packet */

	/* Advance head of m2m pending */
	pqp_req->pgo.head       = PKTLINK(pkt);

	if ((pqp_req->flag & PQP_REQ_FLAG_PGO) == 0) { /* PGO aborted by PGI */
		/* If no more dma in progress, invoke qcb, dettach pktq, free pqp_req */
		__pqp_pgo_pkt_abort(pqp_mgr, pqp_req, pktq, pkt); /* release hbm */
		return;
	}

	hbm_pktid = PKT_HBM_PKTID(pkt);

	/* Setup pktq_prec::<hidx,tidx> with Host BM Head and Tail Packet ID */
	if (PKTQ_PQP_HBM_HEAD(pktq) == PQP_HBM_ID_INV) {
		PKTQ_PQP_HBM_HEAD(pktq) = hbm_pktid;
	}
	/* Add packet to tail of Active in host BM packets */
	PKTQ_PQP_HBM_TAIL(pktq)     = hbm_pktid;

	PQP_PRINT("PGO_PCB" PQP_REQ_FMT, PQP_REQ_VAL(pqp_req));

	PQP_STATS(pqp_mgr->pgo_stats.cbk++);
	pqp_req->pcb(pqp_req->pktq, pkt);

} /* _pqp_pgo_m2m_dd_done_cb() */

#endif  /* HNDM2M */

/**
 * +--------------------------------------------------------------------------+
 *  PQP PAGE OUT Request Entry Point (exported interface)
 *
 *  Algorithm:
 *  0. Audit parameters
 *  1. Determine whether PktQ is managed by PQP (i.e. an tail append SuprPSQ).
 *     If first time, allocate and attach a pqp_req_t object to pktq. Prepare
 *        pqp_req and place into PGO dll.
 *     else, new pkt list needs to be appended. If PktQ's attached pqp_req is in
 *        host dll (m2m could be in progress) then move it to PGO dll.
 *        As the tail packet is never offloaded, append new pkt list to tail.
 *  2. If other pqp_req ahead in PGO dll, return busy.
 *  3. _pqp_pgo_iterate(): Iterate packet (mpdu) from iter to tail, paging out
 *        each data buffer and lfrag. If resources are not available to PAGEOUT
 *        an entire MPDU, then the pqp_req will be retained in the PGO dllO,
 *        with the pqp_req iterator pointing to the MPDU pending PAGEOUT. PktQ
 *        colored as APPEND_POLICY will not have the tail packet (MPDU) PAGED
 *        allowing a subsequent pktlist to be appended to tail packet.
 * +--------------------------------------------------------------------------+
 */
int /* PQP PAGE-OUT Request Handler */
pqp_pgo(pktq_prec_t *pktq, hndpkt_t *head, hndpkt_t *tail, int n_pkts,
        pqp_pgo_pcb_t pcb, pqp_pgi_qcb_t qcb, bool append_policy)
{
	pqp_req_t *pqp_req;
	pqp_mgr_t *pqp_mgr = &pqp_mgr_g;

	/* Step 0. Audit parameters */
	ASSERT(pktq != PQP_PKTQ_NULL);
	ASSERT((head != PQP_DPKT_NULL) && (tail != PQP_DPKT_NULL) && (n_pkts > 0));

	PQP_STATS(pqp_mgr->pgo_stats.req++);
	PQP_DEBUG({
		PQP_PRINT("PQP: PGO pktq<%p,%p,%u> head<%p> tail<%p> pkts<%u> app<%u>",
			PKTQ_HEAD(pktq), PKTQ_TAIL(pktq), PKTQ_PKTS(pktq),
			head, tail, n_pkts, append_policy);
	});
	_pqp_pktlist_hbm_reset(pqp_mgr, head, tail, n_pkts);

	/* Step 1. Check if pktq is managed by PQP */
	pqp_req = __pqp_req_pair(pktq, head, tail);

	if (pqp_req == PQP_REQ_NULL) /* not paired */
	{
		ASSERT((pcb != NULL) && (qcb != NULL));

		/* First time PAGE-OUT, then need to allocate and attach a pqp_req_t */
		if (pqp_mgr->cntF == 0) {
			PQP_DEBUG(PQP_PRINT("PQP: PGO no free requests"));
			PQP_STATS(pqp_mgr->pgo_stats.fail++);
			return BCME_NORESOURCE;
		}
		/* Allocate a PQP request from the Free list (dllF) */
		/* PQP Requests in Free list are reset by __pqp_req_free() */
		pqp_req = (pqp_req_t*) dll_head_p(&pqp_mgr->dllF);
		dll_delete(&pqp_req->node); pqp_mgr->cntF--;

		/* Attach pqp_req_t to pktq, as it is now managed by PQP */
		pqp_req->pktq     = pktq; /* attach request to pktq */
		pqp_req->pgo.head = head;
		pqp_req->pgo.iter = head;
		pqp_req->pgo.tail = tail;
		pqp_req->flag     = PQP_REQ_FLAG_PGO
		                  | ((append_policy) ? PQP_REQ_FLAG_APP : 0);
		// m2m.u32 = 0 : not a single packet has been queued to m2m
		pqp_req->pcb      = (pcb) ? pcb : _pqp_pgo_pcb_none;
		pqp_req->qcb      = (qcb) ? qcb : _pqp_pgi_qcb_none;

		/* Attach pqp_req_t to pktq_prec. */
		/* NOTE: pktq_prec::head and pktq_prec::tail are garbled here. */
		PKTQ_PQP_REQ(pktq)       = (void*)pqp_req;
		PKTQ_PQP_HBM_PAGE(pktq)  = ~0U; /* <pqp_hbm_head, pqp_hbm_tail> = ~0 */
		PKTQ_PKTS(pktq)          = n_pkts;

		/* Move initialized pqp_request into PGO dll */
		dll_append(&pqp_mgr->dllO, &pqp_req->node); pqp_mgr->cntO++;

		PQP_DEBUG(PQP_PRINT("PQP: PGO pktq %p add pqp_req %p", pktq, pqp_req));
	}
	else /* Not the first PQO invoked on pktq, find the attached pqp_req */
	{
		ASSERT(append_policy == TRUE); /* append to tail invalid for PSQ */
		ASSERT((pqp_req->flag & PQP_REQ_FLAG_APP) != 0);
		ASSERT((pqp_req->flag & PQP_REQ_FLAG_PGI) == 0);

		/* If request was active in host, move into PGO dll */
		if (pqp_req->flag & PQP_REQ_FLAG_ACT) {
			ASSERT(pqp_req->pgo.tail == pqp_req->pgo.iter); /* local tail pkt */
			dll_delete(&pqp_req->node);                 pqp_mgr->cntH--;
			dll_append(&pqp_mgr->dllO, &pqp_req->node); pqp_mgr->cntO++;
			pqp_req->flag &= ~PQP_REQ_FLAG_ACT;
		}

		/* Append new packet list to pktq */
		PKTSETLINK(pqp_req->pgo.tail, head);
		pqp_req->pgo.tail  = tail;
		PKTQ_PKTS(pktq)   += n_pkts;

		PQP_DEBUG(PQP_PRINT("PQP: PGO pktq %p has pqp_req %p", pktq, pqp_req));
	}

	/* pqp_req is in dllO (not necessarily at head of dllO) */
	if (pqp_req != PQP_REQ_DLL2PTR(dll_head_p(&pqp_mgr->dllO))) {
		PQP_DEBUG(PQP_PRINT("PQP: PGO pqp_req %p queued", pqp_req));
		// PQP_STATS(pqp_mgr->pgo_stats.pend++);
		return BCME_BUSY;
	}

	/* Step 3. PAGEOUT packets from iter upto tail. */
	_pqp_pgo_iterate(pqp_mgr, pqp_req);

	return BCME_OK;

} /* pqp_pgo() */

/**
 * +--------------------------------------------------------------------------+
 *  Section: PQP PAGE IN Implementation
 * +--------------------------------------------------------------------------+
 */
static INLINE bool
__pqp_pgi_m2m_dd_avail(pqp_mgr_t *pqp_mgr, pqp_req_t *pqp_req)
{
#if defined(HNDM2M)
	uint32 m2m_dd_cnt = m2m_dd_avail(pqp_mgr->m2mI);
	if (m2m_dd_cnt < PQP_MGR_M2M_DD_CH1_THRESH) {
		PQP_DEBUG(PQP_PRINT("PQP: PGI pqp_req %p H2D pending", pqp_req));
		pqp_req->flag |= PQP_REQ_FLAG_H2D;
		m2m_dd_wake_request(pqp_mgr->m2mI);  /* toggle m2m WAKE up notif */
		PQP_STATS(pqp_mgr->pgi_stats.pend++);
		return FALSE;
	}
#endif  /* HNDM2M */
	return TRUE;
} /* __pqp_pgi_m2m_dd_avail() */

static INLINE uchar *
__pqp_pgi_lcl_alloc(pqp_mgr_t *pqp_mgr, pqp_req_t *pqp_req)
{
	uchar *lcl;
	lcl = _hnd_lclget();
	if (lcl == (uchar*)NULL) {
		PQP_DEBUG(PQP_PRINT("PQP: PGI pqp_req %p LCL pending", pqp_req));
		pqp_req->flag |= PQP_REQ_FLAG_LCL;
		PQP_STATS(pqp_mgr->pgi_stats.pend++);
		return (uchar*)NULL;
	}
	PQP_STATS(pqp_mgr->pgi_stats.lcl++);
	return lcl;
} /* __pqp_pgi_lcl_alloc() */

static INLINE uchar *
__pqp_pgi_d11_alloc(pqp_mgr_t *pqp_mgr, pqp_req_t *pqp_req)
{
	uchar *d11;
	d11 = _hnd_d11get();
	if (d11 == (uchar*)NULL) {
		PQP_DEBUG(PQP_PRINT("PQP: PGI pqp_req %p D11 pending", pqp_req));
		pqp_req->flag |= PQP_REQ_FLAG_D11;
		PQP_STATS(pqp_mgr->pgi_stats.pend++);
		return (uchar*)NULL;
	}
	PQP_STATS(pqp_mgr->pgi_stats.d11++);
	return d11;
} /* __pqp_pgi_d11_alloc() */

static INLINE hndpkt_t *
__pqp_pgi_pkt_alloc(pqp_mgr_t *pqp_mgr, pqp_req_t *pqp_req)
{
	hndpkt_t *pkt;
	pkt = _hnd_pktget();
	if (pkt == PQP_DPKT_NULL) {
		PQP_DEBUG(PQP_PRINT("PQP: PGI pqp_req %p DBM pending", pqp_req));
		pqp_req->flag |= PQP_REQ_FLAG_DBM;
		PQP_STATS(pqp_mgr->pgi_stats.pend++);
		return PQP_DPKT_NULL;
	}
	PQP_STATS(pqp_mgr->pgi_stats.pkt++);
	return pkt;
} /* __pqp_pgi_pkt_alloc() */

static INLINE void
__pqp_pgi_m2m_dd_xfer(pqp_mgr_t *pqp_mgr, pqp_req_t *pqp_req,
               uintptr xfer_src_lo, uintptr xfer_src_hi, uintptr xfer_dst_lo,
               int xfer_len)
{
#if defined(HNDM2M)
	m2m_dd_cpy_t m2m_dd_cpy;
	dma64addr_t m2m_dd_src, m2m_dd_dst;
	uint32 xfer_arg = (uint32)((uintptr)(pqp_req));

	m2m_dd_src.lo   = xfer_src_lo;
	m2m_dd_src.hi   = xfer_src_hi;
	m2m_dd_dst.lo   = xfer_dst_lo;
	m2m_dd_dst.hi   = 0U;
	m2m_dd_cpy      = m2m_dd_xfer(pqp_mgr->m2mI, &m2m_dd_src, &m2m_dd_dst,
	                              xfer_len, xfer_arg, PQP_M2M_DD_XFER_SYNC);
	if (m2m_dd_cpy == M2M_INVALID) {
		PQP_PRINT("PQP: PGI m2m_dd_xfer[%08x->%08x] len %4u arg %08x failure\n",
			xfer_src_lo, xfer_dst_lo, xfer_len, xfer_arg);
		PQP_STATS(pqp_mgr->pgi_stats.fail++);
		ASSERT(0);
	}

	PQP_REQ_DMAI(pqp_req) += 1; /* every xfer is synced with a dma done_cb */
	ASSERT(PQP_REQ_DMAI(pqp_req) <= 2); /* at most a data and pkt */

	PQP_STATS(pqp_mgr->pgi_stats.xfer++);
#endif  /* HNDM2M */

} /* __pqp_pgi_m2m_dd_xfer() */

static INLINE hndpkt_t * /* PQP PGI Allocate and H2D DMA Transfer hndpkt */
__pqp_pgi_pkt_alloc_xfer(pqp_mgr_t *pqp_mgr, pqp_req_t *pqp_req)
{
	hndpkt_t *pkt_alloc;
	uint16 hbm_pktid = pqp_req->hbmI;
	PQP_ASSERT_HBM_ID(hbm_pktid);

	PQP_ASSERT(pqp_req->pgi.save == PQP_PKTID_NULL);

	pkt_alloc = __pqp_pgi_pkt_alloc(pqp_mgr, pqp_req);

	if (pkt_alloc != PQP_DPKT_NULL) { /* H2D Transfer hndpkt_t */
		pqp_req->pgi.save = PKTID(pkt_alloc); /* save pktid before xfer */
		__pqp_pgi_m2m_dd_xfer(pqp_mgr, pqp_req,
		                      PQP_HBM_PKTPTR(pqp_mgr, hbm_pktid),
		                      HADDR64_HI(pqp_mgr->hbmP.haddr64),
		                      (uintptr)HND2HBMPKT(pkt_alloc), HBMPKT_SIZE);
	}

	return pkt_alloc;
} /* __pqp_pgi_pkt_alloc_xfer() */

static INLINE void /* PQP PGI Allocate , Set PKTNEXT, and H2D DMA Transfer hndpkt */
__pqp_pgi_pkt_next_alloc_xfer(pqp_mgr_t *pqp_mgr, pqp_req_t *pqp_req, hndpkt_t *pkt)
{
	hndpkt_t *pkt_alloc;
	uint16 hbm_pktid = pqp_req->hbmI;
	PQP_ASSERT_HBM_ID(hbm_pktid);

	PQP_ASSERT(pqp_req->pgi.save == PQP_PKTID_NULL);

	pkt_alloc = __pqp_pgi_pkt_alloc(pqp_mgr, pqp_req);

	if (pkt_alloc != PQP_DPKT_NULL) { /* H2D Transfer hndpkt_t */
		HNDPKTSETNEXT(pqp_mgr->osh, pkt, pkt_alloc);
		__pqp_pgi_m2m_dd_xfer(pqp_mgr, pqp_req,
		                      PQP_HBM_PKTPTR(pqp_mgr, hbm_pktid),
		                      HADDR64_HI(pqp_mgr->hbmP.haddr64),
		                      (uintptr)HND2HBMPKT(pkt_alloc), HBMPKT_SIZE);
	}
} /* __pqp_pgi_pkt_next_alloc_xfer() */

static INLINE void /* PQP PGI Allocate , Set PKTLINK, and H2D DMA Transfer hndpkt */
__pqp_pgi_pkt_link_alloc_xfer(pqp_mgr_t *pqp_mgr, pqp_req_t *pqp_req, hndpkt_t *pkt)
{
	hndpkt_t *pkt_alloc;
	uint16 hbm_pktid = pqp_req->hbmI;

	PQP_ASSERT_HBM_ID(hbm_pktid);

	PQP_ASSERT(pqp_req->pgi.save == PQP_PKTID_NULL);

	pkt_alloc = __pqp_pgi_pkt_alloc(pqp_mgr, pqp_req);

	if (pkt_alloc != PQP_DPKT_NULL) { /* H2D Transfer hndpkt_t */
		PKTSETLINK(pkt, pkt_alloc); /* FIXUP the link now */
		__pqp_pgi_m2m_dd_xfer(pqp_mgr, pqp_req,
		                      PQP_HBM_PKTPTR(pqp_mgr, hbm_pktid),
		                      HADDR64_HI(pqp_mgr->hbmP.haddr64),
		                      (uintptr)HND2HBMPKT(pkt_alloc), HBMPKT_SIZE);
	}
} /* __pqp_pgi_pkt_link_alloc_xfer() */

static void /* Restore pktq, invoke qcb, free pqp_req and kickstart next pgi */
_pqp_pgi_complete(pqp_mgr_t *pqp_mgr, pqp_req_t *pqp_req, pktq_prec_t *pktq)
{
	PQP_PRINT("PGI_QCB" PQP_REQ_FMT, PQP_REQ_VAL(pqp_req));

	PQP_ASSERT(PKTQ_HEAD(pktq) != PQP_DPKT_NULL);   /* pktq_prec is restored */
	PQP_ASSERT(PKTQ_TAIL(pktq) == pqp_req->pgo.tail);
	PQP_ASSERT(pqp_req->flag & PQP_REQ_FLAG_QCB);   /* no PGI to schedule */
	PQP_ASSERT(PQP_REQ_DMAI(pqp_req) == 0);		    /* no dmaI in progress */

	dll_delete(&pqp_req->node); pqp_mgr->cntI--;

	if (PQP_REQ_DMAO(pqp_req) != 0) { /* D2H dmaO to HBM in progress */
		/* Move to dllC, awaiting PGO DMA done cancellation/abort */
		dll_append(&pqp_mgr->dllC, &pqp_req->node); pqp_mgr->cntC++;
	} else {
		PQP_STATS(pqp_mgr->pgi_stats.cbk++);
		pqp_req->qcb(pqp_req->pktq);
		__pqp_req_free(pqp_mgr, pqp_req); /* dettach pktq, free to dllF */
	}

	if (!dll_empty(&pqp_mgr->dllI)) { /* schedule pgi on next pqp_req */
		pqp_req = PQP_REQ_DLL2PTR(dll_head_p(&pqp_mgr->dllI));
		if (!__pqp_pgi_m2m_dd_avail(pqp_mgr, pqp_req))
			return;
		(void)__pqp_pgi_pkt_alloc_xfer(pqp_mgr, pqp_req); /* kickstart */
	}

} /* _pqp_pgi_complete() */

#if defined(HNDM2M)
static void /* Callback registered with M2M DD H2D engine for PKT|DATA xfer */
_pqp_pgi_m2m_dd_done_cb(void *cbdata,
    dma64addr_t *xfer_src, dma64addr_t *xfer_dst, int xfer_len, uint32 xfer_arg)
{
	pqp_mgr_t   *pqp_mgr        = &pqp_mgr_g;
	pqp_req_t   *pqp_req        = (pqp_req_t*)((uintptr)xfer_arg);
	pktq_prec_t *pktq           = pqp_req->pktq;
	osl_t       *osh            = pqp_mgr->osh;
	hndpkt_t    *pkt;

	PQP_STATS(pqp_mgr->pgi_stats.done++);

	ASSERT(PQP_REQ_DMAI(pqp_req) > 0);
	PQP_REQ_DMAI(pqp_req) -= 1;

	PQP_ASSERT(pqp_mgr == (pqp_mgr_t*)cbdata);

	/* Check whether the DMA transfer was for a Data Buffer or Packet DMA */
	if (pqp_req->pgi.buff != PQP_PKTID_NULL) /* pend is pktid for a data dma */
	{
		uint16 hbm_pktid, hbm_lclid;
		pkt = (hndpkt_t*)PKTPTR(pqp_req->pgi.buff);
		PQP_ASSERT(PKTDATA(osh, pkt) == (uchar*)((uintptr)(xfer_dst->lo)));
		if (PKTHASHEAPBUF(osh, pkt)) { /* free HBM LCL buffer */
			hbm_lclid = PKT_HBM_LCLID(pkt);
			__pqp_hbm_lclid_free(pqp_mgr, hbm_lclid);
			// PKT_HBM_LCLID(pkt) = PQP_HBM_ID_INV;
		}
		hbm_pktid = PKT_HBM_PKTID(pkt); /* free HBM pkt */
		__pqp_hbm_pktid_free(pqp_mgr, hbm_pktid);
		// PKT_HBM_PKTID(pkt) = PQP_HBM_ID_INV;

		/* Clear data xfer pending pktid */
		pqp_req->pgi.buff = PQP_PKTID_NULL;

		if (pqp_req->flag & PQP_REQ_FLAG_QCB) { /* all PGI DMA done? */
			_pqp_pgi_complete(pqp_mgr, pqp_req, pktq);
		}

		return; /* done with data buffer xfer callback */
	}

	/* Callback was on a hndpkt structure, and not a data buffer */

	pkt = HBM2HNDPKT(xfer_dst->lo);

	LB_PKTID(pkt)     = pqp_req->pgi.save; /* fixup the overwritten pktid */
	pqp_req->pgi.save = PQP_PKTID_NULL;

	if (PKTQ_HEAD(pktq) == (void*)PQP_DPKT_NULL) {
		PKTQ_HEAD(pktq) = (void*)pkt; /* restore pktq_prec::head */
	}

	if (PKTQ_PQP_HBM_HEAD(pktq) == pqp_req->hbmI ) { /* hbmI is MPDU hndpkt */
		pqp_req->pgi.link       = pkt; /* pkt that needs PKTLINK fixup */
		PKTQ_PQP_HBM_HEAD(pktq) =  /* advance HEAD iterator to linked HBM PKT */
			(PKTQ_PQP_HBM_HEAD(pktq) == PKTQ_PQP_HBM_TAIL(pktq)) ? /* EOL */
			PQP_HBM_ID_INV : PKT_HBM_PKTLINK(pkt);
	}

	/* ----- Check/Allocate resources, or stall on resource ----- */
	if (!__pqp_pgi_m2m_dd_avail(pqp_mgr, pqp_req)) return;

	/* ----- Transfer data buffer, if attached to hndpkt ----- */

	if (PKTISBUFALLOC(osh, pkt))
	{
		/* Allocate data buffer, attach to packet and initiate H2D xfer */
		/* In 6715, D11 data buffer is in hndpkt -- no need to allocate */
		uchar   * data_buf;
		uint16    hbm_bufid;
		uintptr   hbm_dataptr;
		uint32    xfer_src_hi;
		uint32    buf_size;
		uint32    headroom = PKTHEADROOM(osh, pkt);

		if (PKTHASHEAPBUF(osh, pkt)) {
			buf_size    = MAXPKTDATABUFSZ;
			data_buf    = __pqp_pgi_lcl_alloc(pqp_mgr, pqp_req);
			hbm_bufid   = PKT_HBM_LCLID(pkt);
			hbm_dataptr = PQP_HBM_LCLPTR(pqp_mgr, hbm_bufid) + headroom;
			xfer_src_hi = HADDR64_HI(pqp_mgr->hbmL.haddr64);
		} else {
			buf_size    = MAXPKTFRAGSZ - LBUFFRAGSZ;
			data_buf    = __pqp_pgi_d11_alloc(pqp_mgr, pqp_req);
			hbm_bufid   = PKT_HBM_PKTID(pkt);
			hbm_dataptr = PQP_HBM_D11PTR(pqp_mgr, hbm_bufid) + headroom;
			xfer_src_hi = HADDR64_HI(pqp_mgr->hbmP.haddr64);
		}

		if (data_buf == (uchar*)NULL)
			return;

		PKT_HEAD_FIXUP(pkt, (data_buf + 0U));
		PKT_DATA_FIXUP(pkt, (data_buf + headroom));
		PKT_END_FIXUP(pkt,  (data_buf + buf_size));

		/* H2D Transfer data buffer. (xfer_dst.lo == PKTDATA) used in done_cb */
		__pqp_pgi_m2m_dd_xfer(pqp_mgr, pqp_req, hbm_dataptr, xfer_src_hi,
			(uintptr)PKTDATA(osh, pkt), PKTLEN(osh, pkt));

		pqp_req->pgi.buff = PKTID(pkt); /* hndpkt in HBM pending data xfer */
	}
	else
	{
		uint16    hbm_pktid = PKT_HBM_PKTID(pkt);
		uint32    pkt_len   = PKTLEN(osh, pkt);
		uchar   * data_buf;

		/* no longer need the host PKT, as no data buffer to transfer */
		__pqp_hbm_pktid_free(pqp_mgr, hbm_pktid);
		// PKT_HBM_PKTID(pkt) = PQP_HBM_ID_INV;

		/* If PKTLEN is not zero, driver use eth_sada in HWA header as D3 buffer  */
		if (pkt_len) {
			data_buf = HNDPKT_D3BUFF(pkt);
			ASSERT(data_buf);
		} else {
			data_buf = ((uchar *)pkt + LBUFFRAGSZ);
		}

		/* Fixup head, data, end pointer */
		PKTSETBUF(osh, pkt, data_buf, pkt_len);
	}

	HNDPKTSETDATA(osh, pkt);

	/* Done with current hndpkt, iterate to next or linked hndpkt */

	/* --- Iterate to "next" hndpkt --- */

	if (PKTNEXT(pqp_mgr->osh, pkt) != PQP_DPKT_NULL)
	{
		pqp_req->hbmI = PKT_HBM_PKTNEXT(pkt);
		__pqp_pgi_pkt_next_alloc_xfer(pqp_mgr, pqp_req, pkt);
		return;
	}

	/* --- Iterate to "link" packet --- */

	if (PKTQ_PQP_HBM_HEAD(pktq) == PQP_HBM_ID_INV) /* end of PKTLINK list */
	{
		void *tail = pqp_req->pgo.tail;

		PKTQ_TAIL(pktq) = tail; /* restore pktq */
		if (pqp_req->flag & PQP_REQ_FLAG_APP) {
			/* TAIL pkt is not page out to host, free Host BM index */
			__pqp_hbm_pktid_free(pqp_mgr, PKT_HBM_PKTID(tail));
			/* When HWA is enabled, TAIL pkt needs to be fix up for HWA next pointer */
			HNDPKT_FIXUP(osh, tail);
		}
		pqp_req->flag  |= PQP_REQ_FLAG_QCB;
		if (pqp_req->pgi.buff == PQP_PKTID_NULL) {
			PQP_ASSERT(PQP_REQ_DMAI(pqp_req) == 0);
			_pqp_pgi_complete(pqp_mgr, pqp_req, pktq);
		} else {
			PQP_ASSERT(PQP_REQ_DMAI(pqp_req) == 1); /* data DMA pending */
		}
		return;
	}

	if (pqp_req->hbmI != PQP_HBM_ID_INV) { /* not end of HBM PKTLINK list */
		hndpkt_t * pgi_link = pqp_req->pgi.link; /* PKTLINK fixup */
		pqp_req->hbmI	    = PKTQ_PQP_HBM_HEAD(pktq);
		__pqp_pgi_pkt_link_alloc_xfer(pqp_mgr, pqp_req, pgi_link);
	} else {
		uint16 hbm_pktlink = PKTQ_PQP_HBM_HEAD(pktq);
		__pqp_hbm_pktid_free(pqp_mgr, hbm_pktlink);
	}

} /* _pqp_pgi_m2m_dd_done_cb() */

static void /* Notification invoked by M2M DD H2D engine of DD availability */
_pqp_pgi_m2m_dd_wake_cb(void *cbdata)
{
	pqp_mgr_t  *pqp_mgr = &pqp_mgr_g;
	PQP_ASSERT(pqp_mgr == (pqp_mgr_t*)cbdata);
	PQP_STATS(pqp_mgr->pgi_stats.wake++);

} /* _pqp_pgi_m2m_dd_wake_cb() */

#endif  /* HNDM2M */

/**
 * +--------------------------------------------------------------------------+
 *  PQP PAGE IN Request Entry Point (exported interface)
 *
 *  Algorithm:
 *  1. Fetch the pqp_req paired with the pktq_prec
 *  2. Audit that pqp_req underwent a pqp_pgo page operation and not reentrant.
 *  3. Clear PGO flag and Set PGI flag. Pprevent reentrancy of pqp_pgi()
 *  4. Determine whether pqp_req was active and not pending any PGO DMA and
 *     remove pqp_req from either dllH or dllO, respectively.
 *
 *  5. Check whether all packets are still in dongle memory. If even one D11
 *     packet was paged out, then the pktq::pqp_hbm<head,tail> would be valid.
 *     PKTQ_PQP_HBM_PAGE(pktq) == ~0U, means both head and tail are invalid.
 *   5.1 Restore the pktq and flag as ready for QCB.
 *   5.2 If PGO DMA in progress,
 *           Move the pqp_req to dllC, awaiting PGO DMA done to release HBM
 *           resources. _pqp_pgo_m2m_dd_done_cb() will check whether the PGO DMA
 *           needs to be aborted (i.e. free HBM resources for data and pkt) and
 *           whether the pktq has been restored and user queue callback may be
 *           invoked. End of Algorithm.
 *   5.3 Else, invoke user queue callback and return pqp_req to dllF. EoAlg.
 *
 *  6. pqp_req needs PGI scheduling, so place into dllI.
 *     Setup PGI HBM "iter"ator to the first Lfrag (MPDU) that was paged out.
 *     Initialize "link" to NULL, which will be used to fixup PKTLINK, as the
 *     iterator moves to next MPDU. Initialize "pend" to NULL as no packets
 *     have data DMA in progress.
 *
 *  7. If the pqp_req is at the head of the schedule list dllI, then kickstart
 *     the PGI iteration, by fetching the first packet (MPDU). EoAlg.
 *
 * Asynchronous execution resumes in _pqp_pgi_m2m_dd_done_cb()
 *
 * +--------------------------------------------------------------------------+
 */

int /* Entry point to a PAGEIN request */
pqp_pgi(pktq_prec_t *pktq)
{
	pqp_req_t *pqp_req;
	pqp_mgr_t *pqp_mgr = &pqp_mgr_g;

	/* 1. Fetch the pqp_req paired with the pktq_prec */
	ASSERT(pktq != PQP_PKTQ_NULL);
	pqp_req = PKTQ_PQP_REQ(pktq);
	ASSERT(pqp_req != PQP_REQ_NULL);

	PQP_STATS(pqp_mgr->pgi_stats.req++);
	PQP_DEBUG(PQP_PRINT("PQP: PGI pktq<%p> pqp_req<%p" PQP_REQ_FMT,
	                    pktq, pqp_req, PQP_REQ_VAL(pqp_req)));

	/* 2. Ensure pqp_req underwent a pqp_pgo page operation and not reentrant */
	ASSERT(pqp_req->pktq == pktq);
	ASSERT((pqp_req->flag & PQP_REQ_FLAG_POP) == PQP_REQ_FLAG_PGO);

	/* 3. Toggle: i.e clr(PGO) and set(PGI). Used in reentrancy audit */
	pqp_req->flag ^= PQP_REQ_FLAG_POP;

	/* 4. Determine whether pqp_req was active and not pending any PGO DMA */
	dll_delete(&pqp_req->node);
	if (pqp_req->flag & PQP_REQ_FLAG_ACT)
		pqp_mgr->cntH--; /* from dllH */
	else
		pqp_mgr->cntO--; /* from dllO */

	/* 5. Check whether all packets are still in dongle memory. */
	if (PKTQ_PQP_HBM_PAGE(pktq) == ~0U)
	{
		PKTQ_HEAD(pktq) = (void*)pqp_req->pgo.head; /* restore pktq_prec */
		PKTQ_TAIL(pktq) = (void*)pqp_req->pgo.tail;
		pqp_req->flag  |= PQP_REQ_FLAG_QCB;
#if defined(HNDM2M)
		/* 5.2 If PGO DMA in progress */
		if (PQP_REQ_DMAO(pqp_req) != 0) {
			/* Move to dllC, awaiting PGO DMA done cancellation/abort */
			dll_append(&pqp_mgr->dllC, &pqp_req->node); pqp_mgr->cntC++;
			return BCME_OK;
		}
#endif  /* HNDM2M */
		/* 5.3 Else, invoke user queue callback and return pqp_req to dllF */
		PQP_ASSERT(pqp_req->pgo.iter == pqp_req->pgo.head);
		PQP_STATS(pqp_mgr->pgi_stats.cbk++);
		pqp_req->qcb(pktq);   /* inform caller that queue is restored */
		__pqp_req_free(pqp_mgr, pqp_req); /* dettach pktq, free to dllF */
		return PKTQ_PKTS(pktq); /* >0 return value implies pktq restored */
	}

	/* 6. PKTQ has HBM pkts pktq:pqp_hbm<head,tail> that need to be PAGED IN */
	dll_append(&pqp_mgr->dllI, &pqp_req->node); pqp_mgr->cntI++;

	pqp_req->hbmI     = PKTQ_PQP_HBM_HEAD(pktq); /* setup pgi hbm iterator */
	pqp_req->pgi.link = PQP_DPKT_NULL;
	pqp_req->pgi.save = PQP_PKTID_NULL;
	pqp_req->pgi.buff = PQP_PKTID_NULL;
	PKTQ_HEAD(pktq)   = (void*)PQP_DPKT_NULL; /* pktq to pqp_req link broken */

	/* pqp_req is in dllI (not necessarily at head of dllI) */
	if (pqp_req != PQP_REQ_DLL2PTR(dll_head_p(&pqp_mgr->dllI))) {
		PQP_DEBUG(PQP_PRINT("PQP: PGI pqp_req %p queued", pqp_req));
		return BCME_BUSY;
	}

	/* 7. Kickstart the PGI iteration, by fetching the first packet */
	if (!__pqp_pgi_m2m_dd_avail(pqp_mgr, pqp_req)) return BCME_OK;

	(void)__pqp_pgi_pkt_alloc_xfer(pqp_mgr, pqp_req);

	/* Asynchronous execution resumes in _pqp_pgi_m2m_dd_done_cb() */

	return BCME_OK;

} /* pqp_pgi() */

#endif /* DONGLEBUILD && HNDPQP */
