/*
 * HND Run Time Environment OS Abstraction Layer.
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
 * $Id: hndrte_osl.h 708652 2017-07-04 04:52:34Z $
 */

#ifndef _hndrte_osl_h_
#define _hndrte_osl_h_

#include <hndrte.h>
#include <hndrte_lbuf.h>

struct osl_info {
	uint pktalloced;	/* Number of allocated packet buffers */
	void *dev;		/* Device handle */
	pktfree_cb_fn_t tx_fn;	/* Callback function for PKTFREE */
	void *tx_ctx;		/* Context to the callback function */
};

#define DECLSPEC_ALIGN(x)	__attribute__ ((aligned(x)))

struct pktpool;
struct bcmstrbuf;

/* PCMCIA attribute space access macros */
#define	OSL_PCMCIA_READ_ATTR(osh, offset, buf, size) 	\
	({ \
	 BCM_REFERENCE(osh); \
	 BCM_REFERENCE(buf); \
	 ASSERT(0); \
	 })
#define	OSL_PCMCIA_WRITE_ATTR(osh, offset, buf, size) 	\
	({ \
	 BCM_REFERENCE(osh); \
	 BCM_REFERENCE(buf); \
	 ASSERT(0); \
	 })

/* PCI configuration space access macros */
#ifdef	SBPCI
#define	OSL_PCI_READ_CONFIG(osh, offset, size) \
		osl_pci_read_config((osh), (offset), (size))
#define	OSL_PCI_WRITE_CONFIG(osh, offset, size, val) \
		osl_pci_write_config((osh), (offset), (size), (val))
extern uint32 osl_pci_read_config(osl_t *osh, uint offset, uint size);
extern void osl_pci_write_config(osl_t *osh, uint offset, uint size, uint val);

/* PCI device bus # and slot # */
#define OSL_PCI_BUS(osh)	osl_pci_bus(osh)
#define OSL_PCI_SLOT(osh)	osl_pci_slot(osh)
#define OSL_PCIE_DOMAIN(osh)	({BCM_REFERENCE(osh); (0);})
#define OSL_PCIE_BUS(osh)	({BCM_REFERENCE(osh); (0);})
extern uint osl_pci_bus(osl_t *osh);
extern uint osl_pci_slot(osl_t *osh);
#else	/* SBPCI */
#define	OSL_PCI_READ_CONFIG(osh, offset, size) \
	({BCM_REFERENCE(osh); (((offset) == 8) ? 0 : 0xffffffff);})
#define	OSL_PCI_WRITE_CONFIG(osh, offset, size, val)	({BCM_REFERENCE(osh); BCM_REFERENCE(val);})

/* PCI device bus # and slot # */
#define OSL_PCI_BUS(osh)	({BCM_REFERENCE(osh); (0);})
#define OSL_PCI_SLOT(osh)	({BCM_REFERENCE(osh); (0);})
#define OSL_PCIE_DOMAIN(osh)	({BCM_REFERENCE(osh); (0);})
#define OSL_PCIE_BUS(osh)	({BCM_REFERENCE(osh); (0);})
#endif	/* SBPCI */

/* register access macros */
#define	R_REG(osh, r) \
	({ \
	 BCM_REFERENCE(osh); \
	 sizeof(*(r)) == sizeof(uint32) ? rreg32((volatile uint32 *)(void *)(r)) : \
	 sizeof(*(r)) == sizeof(uint16) ? rreg16((volatile uint16 *)(void *)(r)) : \
					  rreg8((volatile uint8 *)(void *)(r)); \
	 })
#define	W_REG(osh, r, v) \
	do { \
		BCM_REFERENCE(osh); \
		if (sizeof(*(r)) == sizeof(uint32)) \
			wreg32((volatile uint32 *)(void *)(r), (uint32)(v)); \
		else if (sizeof(*(r)) == sizeof(uint16)) \
			wreg16((volatile uint16 *)(void *)(r), (uint16)(v)); \
		else \
			wreg8((volatile uint8 *)(void *)(r), (uint8)(v)); \
	} while (0)

#define	AND_REG(osh, r, v)		W_REG(osh, (r), R_REG(osh, r) & (v))
#define	OR_REG(osh, r, v)		W_REG(osh, (r), R_REG(osh, r) | (v))

/* OSL initialization */
extern osl_t *osl_attach(void *pdev);
extern void osl_detach(osl_t *osh);

#define PKTFREESETCB(osh, _tx_fn, _tx_ctx) \
	do { \
	   osh->tx_fn = _tx_fn; \
	   osh->tx_ctx = _tx_ctx; \
	} while (0)

/* general purpose memory allocation */
#if defined(BCMDBG_MEM) || defined(BCMDBG_MEMFAIL)
#define	MALLOC(osh, size)	({(void)(osh); osl_malloc((size), __FILE__, __LINE__);})
extern void *osl_malloc(uint size, char *file, int line);
#define	MALLOCZ(osh, size)	({(void)(osh); osl_mallocz((size), __FILE__, __LINE__);})
extern void *osl_mallocz(uint size, char *file, int line);
#define	MALLOC_ALIGN(osh, size, align_bits)	\
		({(void)(osh); osl_malloc_align((size), (align_bits), __FILE__, __LINE__);})
extern void *osl_malloc_align(uint size, uint align_bits, char *file, int line);
#else
#define	MALLOC(osh, size)	({(void)(osh); osl_malloc((size));})
extern void *osl_malloc(uint size);
#define	MALLOCZ(osh, size)	({(void)(osh); osl_mallocz((size));})
extern void *osl_mallocz(uint size);
#define	MALLOC_ALIGN(osh, size, align_bits) ({(void)(osh); osl_malloc_align((size), (align_bits));})
extern void *osl_malloc_align(uint size, uint align_bits);
#endif /* BCMDBG_MEM */

#define	MFREE(osh, addr, size)	({(void)(osh); (void)(size); osl_mfree((addr));})
#define	MALLOCED(osh)		osl_malloced((osh))
#define	MALLOC_FAILED(osh)	osl_malloc_failed((osh))
#define	MALLOC_DUMP(osh, b)	BCM_REFERENCE(osh)
extern int osl_mfree(void *addr);
extern uint osl_malloced(osl_t *osh);
extern uint osl_malloc_failed(osl_t *osh);
extern int osl_error(int bcmerror);

/* microsecond delay */
#define	OSL_DELAY(usec)		hndrte_delay(usec)

/* host/bus architecture-specific address byte swap */
#define BUS_SWAP32(v)		(v)

/* get processor cycle count */
#define OSL_GETCYCLES(x)	((x) = osl_getcycles())

#define OSL_ERROR(bcmerror)	osl_error(bcmerror)

/* uncached/cached virtual address */
#define	OSL_UNCACHED(va)	hndrte_uncached(va)
#define	OSL_CACHED(va)		hndrte_cached(va)

#define OSL_CACHE_FLUSH(va, len)	BCM_REFERENCE(va)
#define OSL_CACHE_INV(va, len)		BCM_REFERENCE(va)
#define OSL_PREFETCH(va)			BCM_REFERENCE(va)

#define OSL_PREF_RANGE_LD(va, sz)	BCM_REFERENCE(va)
#define OSL_PREF_RANGE_ST(va, sz)	BCM_REFERENCE(va)

/* dereference an address that may cause a bus exception */
#define	BUSPROBE(val, addr)	osl_busprobe(&(val), (uint32)(addr))
extern int osl_busprobe(uint32 *val, uint32 addr);

/* allocate/free shared (dma-able) consistent (uncached) memory */
#define DMA_CONSISTENT_ALIGN_BITS	2
#define	DMA_CONSISTENT_ALIGN	(1 << DMA_CONSISTENT_ALIGN_BITS)

#if defined(BCMDBG_MEM) || defined(BCMDBG_MEMFAIL)
#define	DMA_ALLOC_CONSISTENT(osh, size, align, tot, pap, dmah) \
	({ \
	 BCM_REFERENCE(osh); \
	 hndrte_dma_alloc_consistent(size, align, (tot), (void *)(pap), __FILE__, __LINE__); \
	 })
#else
#define	DMA_ALLOC_CONSISTENT(osh, size, align, tot, pap, dmah) \
	({BCM_REFERENCE(osh); hndrte_dma_alloc_consistent(size, align, (tot), (void *)(pap));})
#endif // endif
#define	DMA_FREE_CONSISTENT(osh, va, size, pa, dmah) \
	({BCM_REFERENCE(osh); hndrte_dma_free_consistent((void*)(va));})

/* map/unmap direction */
#define	DMA_TX			1	/* TX direction for DMA */
#define	DMA_RX			2	/* RX direction for DMA */

/* API for DMA addressing capability */
#define OSL_DMADDRWIDTH(osh, addrwidth) BCM_REFERENCE(osh)

/* map/unmap physical to virtual I/O */
#define	REG_MAP(pa, size)		hndrte_reg_map(pa, size)
#define	REG_UNMAP(va)			hndrte_reg_unmap(va)

/* map/unmap shared (dma-able) memory */
#define	DMA_MAP(osh, va, size, direction, lb, dmah) \
	({ \
	 BCM_REFERENCE(osh); \
	 ((dmaaddr_t)hndrte_dma_map(va, size)); \
	 })
#define	DMA_UNMAP(osh, pa, size, direction, p, dmah) \
	({ \
	 BCM_REFERENCE(osh); \
	 hndrte_dma_unmap((uint32)pa, size); \
	 })

/* shared (dma-able) memory access macros */
#define	R_SM(r)				*(r)
#define	W_SM(r, v)			(*(r) = (v))
#define	BZERO_SM(r, len)		memset((r), '\0', (len))

/* assert & debugging */
#define assfail	hndrte_assfail

#define OSH_NULL   NULL

/* the largest reasonable packet buffer driver uses for ethernet MTU in bytes */
#define	PKTBUFSZ			(MAXPKTBUFSZ - LBUFSZ)
#define	PKTFRAGSZ			(MAXPKTFRAGSZ - LBUFFRAGSZ)
#define	PKTRXFRAGSZ			(MAXPKTRXFRAGSZ - LBUFFRAGSZ)

/* packet primitives */
#define PKTGETLF(osh, len, send, lbuf_type)	(void *)osl_pktfrag_get((osh), (len), lbuf_type)
#define PKTGET(osh, len, send)		(void *)osl_pktget((osh), (len))
#define PKTALLOC(osh, len, lbuf_type)	(void *)osl_pktalloc((osh), (len), lbuf_type)
#define PKTFREE(osh, p, send)		osl_pktfree((osh), (p), (send))
#define	PKTDATA(osh, lb)		({BCM_REFERENCE(osh); LBP(lb)->data;})
#define	PKTLEN(osh, lb)			({BCM_REFERENCE(osh); LBP(lb)->len;})
#define	PKTHEADROOM(osh, lb)		({BCM_REFERENCE(osh); (LBP(lb)->data - lb_head(LBP(lb)));})
#define	PKTTAILROOM(osh, lb)		\
	({ \
	 BCM_REFERENCE(osh); \
	 (lb_end(LBP(lb)) - (LBP(lb)->data + LBP(lb)->len)); \
	 })
#define	PKTSETLEN(osh, lb, len)		({BCM_REFERENCE(osh); lb_setlen(LBP(lb), (len));})
#define	PKTPUSH(osh, lb, bytes)		({BCM_REFERENCE(osh); lb_push(LBP(lb), (bytes));})
#define	PKTPULL(osh, lb, bytes)		({BCM_REFERENCE(osh); lb_pull(LBP(lb), (bytes));})
#define PKTDUP(osh, p)			osl_pktdup((osh), (p))
#define	PKTTAG(lb)			((void *)((LBP(lb))->pkttag))
#define	PKTPRIO(lb)			lb_pri(LBP(lb))
#define	PKTSETPRIO(lb, x)		lb_setpri(LBP(lb), (x))
#define PKTSHARED(lb)       (lb_isclone(LBP(lb)) || LBP(lb)->mem.refcnt > 1)
#define PKTALLOCED(osh)			((osl_t *)osh)->pktalloced
#define PKTSUMNEEDED(lb)		lb_sumneeded(LBP(lb))
#define PKTSETSUMNEEDED(lb, x)		lb_setsumneeded(LBP(lb), (x))
#define PKTSUMGOOD(lb)			lb_sumgood(LBP(lb))
#define PKTSETSUMGOOD(lb, x)		lb_setsumgood(LBP(lb), (x))
#define PKTMSGTRACE(lb)			lb_msgtrace(LBP(lb))
#define PKTSETMSGTRACE(lb, x)		lb_setmsgtrace(LBP(lb), (x))
#define PKTDATAOFFSET(lb)		lb_dataoff(LBP(lb))
#define PKTSETDATAOFFSET(lb, dataOff)	lb_setdataoff(LBP(lb), dataOff)
#define PKTSETPOOL(osh, lb, x, y)	({BCM_REFERENCE(osh); lb_setpool(LBP(lb), (x), (y));})
#define PKTPOOL(osh, lb)		({BCM_REFERENCE(osh); lb_pool(LBP(lb));})
#define PKTALTINTF(lb)          lb_altinterface(LBP(lb))
#define PKTSETALTINTF(lb, x)    lb_setaltinterface(LBP(lb), (x))
#ifdef BCMDBG_POOL
#define PKTPOOLSTATE(lb)		lb_poolstate(LBP(lb))
#define PKTPOOLSETSTATE(lb, s)		lb_setpoolstate(LBP(lb), (s))
#endif // endif
#if defined(BCMPKTIDMAP)
#define PKTPTR(id)			(hndrte_pktptr_map[id])
#define PKTID(lb)			lb_getpktid(LBP(lb))
#define PKTSETID(lb, id)	lb_setpktid(LBP(lb), (id))
#define PKTNEXT(osh, lb)	({BCM_REFERENCE(osh); PKTPTR(LBP(lb)->nextid);})
#define PKTSETNEXT(osh, lb, x)	\
	({	\
	  BCM_REFERENCE(osh);	\
	 (LBP(lb)->nextid = ((x) == NULL) ? PKT_NULL_ID : PKTID(LBP(x)));	\
	 })
#define PKTLINK(lb)			(PKTPTR(LBP(lb)->linkid))
#define PKTSETLINK(lb, x)	\
	(LBP(lb)->linkid = ((x) == NULL) ? PKT_NULL_ID : PKTID(LBP(x)))
#define PKTFREELIST(lb)			(LBP(lb)->freelist)
#define PKTSETFREELIST(lb, x)	(LBP(lb)->freelist = LBP(x))
#else  /* ! BCMPKTIDMAP */
#define PKTPTR(lb)			(lb)
#define PKTID(lb)			({BCM_REFERENCE(lb); 0;})
#define PKTSETID(lb, id)	({BCM_REFERENCE(lb); BCM_REFERENCE(id);})
#define	PKTNEXT(osh, lb)		({BCM_REFERENCE(osh); (LBP(lb)->next);})
#define	PKTSETNEXT(osh, lb, x)	({BCM_REFERENCE(osh); (LBP(lb)->next = LBP(x));})
#define	PKTLINK(lb)				(LBP(lb)->link)
#define	PKTSETLINK(lb, x)		(LBP(lb)->link = LBP(x))
#define PKTFREELIST(lb)			PKTLINK(lb)
#define PKTSETFREELIST(lb, x)	PKTSETLINK((lb), (x))
#endif /* ! BCMPKTIDMAP */
#define PKTSHRINK(osh, m)		osl_pktshrink((osh), (m))
#ifdef BCM_DHDHDR
#define PKTSWAPD11BUF(osh, p) lfbufpool_swap_d11_buf(osh, p)
int lfbufpool_swap_d11_buf(osl_t *osh, void *p);
#define PKTBUFEARLYFREE(osh, p) lfbufpool_early_free_buf(osh, p)
void lfbufpool_early_free_buf(osl_t *osh, void *p);
#endif /* BCM_DHDHDR */

#define BCM_DMAPAD
#define PKTDMAPAD(osh, lb)		({BCM_REFERENCE(osh); (LBP(lb)->dmapad);})
#define PKTSETDMAPAD(osh, lb, pad)	({BCM_REFERENCE(osh); (LBP(lb)->dmapad = (pad));})

#define PKTRXCPLID(osh, lb)		({BCM_REFERENCE(osh); (LBP(lb)->rxcpl_id);})
#define PKTSETRXCPLID(osh, lb, id)	({BCM_REFERENCE(osh); (LBP(lb)->rxcpl_id = (id));})
#define PKTRESETRXCPLID(osh, lb)	PKTSETRXCPLID(osh, lb, 0)

#define PKTSETNODROP(osh, lb)		({BCM_REFERENCE(osh); lb_setnodrop(LBP(lb));})
#define PKTNODROP(osh, lb)		({BCM_REFERENCE(osh); lb_nodrop(LBP(lb));})

#define PKTSETTYPEEVENT(osh, lb)	({BCM_REFERENCE(osh); lb_settypeevent(LBP(lb));})
#define PKTTYPEEVENT(osh, lb)		({BCM_REFERENCE(osh); lb_typeevent(LBP(lb));})

#define PKT_SET_DOT3(osh, lb)		({BCM_REFERENCE(osh); lb_set_dot3_pkt(LBP(lb));})
#define PKT_DOT3(osh, lb)		({BCM_REFERENCE(osh); lb_dot3_pkt(LBP(lb));})

#ifdef BCMDBG_PKT /* pkt logging for debugging */
#define PKTLIST_DUMP(osh, buf)		({BCM_REFERENCE(osh); (void)buf;})
#else /* BCMDBG_PKT */
#define PKTLIST_DUMP(osh, buf)		BCM_REFERENCE(osh)
#endif /* BCMDBG_PKT */

#define PKTFRMNATIVE(osh, lb)		((void *)(osl_pktfrmnative((osh), (lb))))
#define PKTTONATIVE(osh, p)		((struct lbuf *)(osl_pkttonative((osh), (p))))
#define PKTSET80211(lb)			lb_set80211pkt(LBP(lb))
#define PKT80211(lb)			lb_80211pkt(LBP(lb))

#if defined(WL_MONITOR)
#define PKTSETMON(lb)                   lb_setmonpkt(LBP(lb))
#define PKTMON(lb)                      lb_monpkt(LBP(lb))
#endif /* WL_MONITOR */

#define PKTIFINDEX(osh, lb)	(LBP(lb)->ifidx)
#define PKTSETIFINDEX(osh, lb, idx) (LBP(lb)->ifidx = (idx))

/* These macros used to set/get a 32-bit value in the pkttag */
#define PKTTAG_SET_VALUE(lb, val) (*((uint32*)PKTTAG(lb)) = (uint32)val)
#define PKTTAG_GET_VALUE(lb) (*((uint32*)PKTTAG(lb)))

/* Lbuf with fraglist */
#define PKTFRAGPKTID(osh, lb)		(LBFP(lb)->flist.finfo[LB_FRAG_CTX].ctx.pktid)
#define PKTSETFRAGPKTID(osh, lb, id)	(LBFP(lb)->flist.finfo[LB_FRAG_CTX].ctx.pktid = (id))
#define PKTFRAGTOTNUM(osh, lb)		LBFP(lb)->flist.finfo[LB_FRAG_CTX].ctx.fnum
#define PKTSETFRAGTOTNUM(osh, lb, tot)	(LBFP(lb)->flist.finfo[LB_FRAG_CTX].ctx.fnum = (tot))
#define PKTFRAGTOTLEN(osh, lb)		LBFP(lb)->flist.flen[LB_FRAG_CTX]
#define PKTSETFRAGTOTLEN(osh, lb, len)		(LBFP(lb)->flist.flen[LB_FRAG_CTX] = (len))
#define PKTFRAGFLOWRINGID(osh, lb)		LBFP(lb)->flist.finfo[LB_FRAG_CTX].ctx.flowid
#define PKTSETFRAGFLOWRINGID(osh, lb, ring)	\
	(LBFP(lb)->flist.finfo[LB_FRAG_CTX].ctx.flowid = (ring))

/* XXX: some of the builds use only one frag and in those cases,
 * would like to use the last one for metadata
*/
/* Lbuf with metadata inside */
#define PKTFRAGMETADATALEN(osh, lb)		LBFP(lb)->flist.flen[LB_FRAG_MAX]
#define PKTFRAGMETADATA_LO(osh, lb)		LBFP(lb)->flist.finfo[LB_FRAG_MAX].frag.data_lo
#define PKTFRAGMETADATA_HI(osh, lb)		LBFP(lb)->flist.finfo[LB_FRAG_MAX].frag.data_hi
#define PKTSETFRAGMETADATALEN(osh, lb, len)	(PKTFRAGMETADATALEN(osh, lb) = (len))
#define PKTSETFRAGMETADATA_LO(osh, lb, addr)	(PKTFRAGMETADATA_LO(osh, lb) = (addr))
#define PKTSETFRAGMETADATA_HI(osh, lb, addr)	(PKTFRAGMETADATA_HI(osh, lb) = (addr))

/* RX Lfrag with Rx completion ID */
#define PKTFRAGRXCPLID(osh, lb)			(LBFP(lb)->flist.rxcpl_id)
#define PKTFRAGSETRXCPLID(osh, lb, id)		(PKTFRAGRXCPLID(osh, lb) = (id))

/* in rx path, reuse totlen as used len */
#define PKTFRAGUSEDLEN(osh, lb)		(PKTISRXFRAG(osh, lb) ? \
	LBFP(lb)->flist.flen[LB_FRAG_CTX] : 0)
#define PKTSETFRAGUSEDLEN(osh, lb, len)		(LBFP(lb)->flist.flen[LB_FRAG_CTX] = len)

#define PKTFRAGLEN(osh, lb, ix)		LBFP(lb)->flist.flen[ix]
#define PKTSETFRAGLEN(osh, lb, ix, len)		(LBFP(lb)->flist.flen[ix] = (len))
#define PKTFRAGDATA_LO(osh, lb, ix)	LBFP(lb)->flist.finfo[ix].frag.data_lo
#define PKTSETFRAGDATA_LO(osh, lb, ix, addr)	(LBFP(lb)->flist.finfo[ix].frag.data_lo = (addr))
#define PKTFRAGDATA_HI(osh, lb, ix)	LBFP(lb)->flist.finfo[ix].frag.data_hi
#define PKTSETFRAGDATA_HI(osh, lb, ix, addr)	(LBFP(lb)->flist.finfo[ix].frag.data_hi = (addr))

#define PKTCOPYFLAGS(osh, lb1, lb2)      ({BCM_REFERENCE(osh); LBP(lb1)->flags \
						= LBP(lb2)->flags;})
/* RX FRAG */
#define PKTISRXFRAG(osh, lb)          ({BCM_REFERENCE(osh); lb_is_rxfrag(LBP(lb));})
#define PKTSETRXFRAG(osh, lb)		({BCM_REFERENCE(osh); lb_set_rxfrag(LBP(lb));})
#define PKTRESETRXFRAG(osh, lb)		({BCM_REFERENCE(osh); lb_reset_rxfrag(LBP(lb));})

/* LFRAG flags */
#define PKTISTXSPROCESSED(osh, lb)	({BCM_REFERENCE(osh); ((LBFP(lb)->flist.finfo\
			[LB_FRAG_CTX].ctx.lfrag_flags & LB_TXS_PROCESSED) ? 1 : 0);})
#define PKTRESETTXSPROCESSED(osh, lb)	({BCM_REFERENCE(osh); LBFP(lb)->flist.finfo\
			[LB_FRAG_CTX].ctx.lfrag_flags &= ~LB_TXS_PROCESSED;})
#define PKTSETTXSPROCESSED(osh, lb)	({BCM_REFERENCE(osh); LBFP(lb)->flist.finfo\
			[LB_FRAG_CTX].ctx.lfrag_flags |= LB_TXS_PROCESSED;})

/* TX FRAG */
#define PKTISTXFRAG(osh, lb)          ({BCM_REFERENCE(osh); lb_is_txfrag(LBP(lb));})
#define PKTSETTXFRAG(osh, lb)		({BCM_REFERENCE(osh); lb_set_txfrag(LBP(lb));})
#define PKTRESETTXFRAG(osh, lb)		({BCM_REFERENCE(osh); lb_reset_txfrag(LBP(lb));})

/* Need Rx completion used for AMPDU reordering */
#define PKTNEEDRXCPL(osh, lb)          ({BCM_REFERENCE(osh); lb_need_rxcpl(LBP(lb));})
#define PKTSETNORXCPL(osh, lb)		({BCM_REFERENCE(osh); lb_set_norxcpl(LBP(lb));})
#define PKTRESETNORXCPL(osh, lb)	({BCM_REFERENCE(osh); lb_clr_norxcpl(LBP(lb));})

/* TX FRAG is using heap allocated buffer */
#define PKTHASHEAPBUF(osh, lb)		({BCM_REFERENCE(osh); lb_has_heapbuf(LBP(lb));})
#define PKTSETHEAPBUF(osh, lb)		({BCM_REFERENCE(osh); lb_set_heapbuf(LBP(lb));})
#define PKTRESETHEAPBUF(osh, lb)	({BCM_REFERENCE(osh); lb_clr_heapbuf(LBP(lb));})

/* flow read index storage access */
#define PKTFRAGRINGINDEX(osh, lb)		LBFP(lb)->flist.ring_idx
#define PKTFRAGSETRINGINDEX(osh, lb, ix)	(LBFP(lb)->flist.ring_idx = (ix))

#define PKTISFRAG(osh, lb)	({BCM_REFERENCE(osh); lb_is_frag(LBP(lb));})
#define PKTPARAMS(osh, lb, len, ix, ft, fbuf, lo, hi)		\
			osl_pktparams(osh, &(lb), &(len), &(ix), &(ft), &(fbuf), &(lo), &(hi))

#define PKTFRAGISCHAINED(osh, i)	({BCM_REFERENCE(osh); (i > LB_FRAG_MAX);})
/* TRIM Tail bytes fomr lfrag */
#define PKTFRAG_TRIM_TAILBYTES(osh, p, len)	lfrag_trim_tailbytes(osh, p, len)

/* packet has metadata */
#define PKTHASMETADATA(osh, lb)		({BCM_REFERENCE(osh); lb_has_metadata(lb);})
#define PKTSETHASMETADATA(osh, lb)	({BCM_REFERENCE(osh); lb_set_has_metadata(lb);})
#define PKTRESETHASMETADATA(osh, lb)	({BCM_REFERENCE(osh); lb_reset_has_metadata(lb);})

#ifdef BCM_DHDHDR
#define PKTSETBUF(osh, lb, buf, n) \
	({BCM_REFERENCE(osh); lb_set_buf((struct lbuf *)lb, buf, n);})
#define PKTHEAD(osh, lb) \
	({BCM_REFERENCE(osh); lb_head(LBP(lb));})
#define PKTFRAGSETTXSTATUS(osh, lb, txs) \
	({BCM_REFERENCE(osh); \
	((LBFP(lb)->flist.finfo[LB_FRAG_MAX].scb_cache.txstatus) = (txs));})
#define PKTFRAGTXSTATUS(osh, lb)  ((LBFP(lb)->flist.finfo[LB_FRAG_MAX].scb_cache.txstatus))
#define PKTSETWLFCSEQ(osh, lb, seq) ({BCM_REFERENCE(osh); ((LBP(lb)->fcseq) = (seq));})
#define PKTWLFCSEQ(osh, lb)  ((LBP(lb)->fcseq))
#define PKTFRAGFCTLV(osh, lb) \
	({BCM_REFERENCE(osh); (LBFP(lb)->flist.finfo[LB_FRAG_MAX].fc_tlv);})
#endif /* BCM_DHDHDR */

#ifdef PKTC_DONGLE

#define	PKTCSETATTR(s, f, p, b)	BCM_REFERENCE(s)
#define	PKTCCLRATTR(s)		BCM_REFERENCE(s)
#define	PKTCGETATTR(s)		({BCM_REFERENCE(s); 0;})
#define	PKTCCNT(skb)		({BCM_REFERENCE(skb); 0;})
#define	PKTCLEN(skb)		({BCM_REFERENCE(skb); 0;})
#define	PKTCGETFLAGS(skb)	({BCM_REFERENCE(skb); 0;})
#define	PKTCSETFLAGS(skb, f)	BCM_REFERENCE(skb)
#define	PKTCCLRFLAGS(skb)	BCM_REFERENCE(skb)
#define	PKTCFLAGS(skb)		({BCM_REFERENCE(skb); 0;})
#define	PKTCSETCNT(skb, c)	BCM_REFERENCE(skb)
#define	PKTCINCRCNT(skb)	BCM_REFERENCE(skb)
#define	PKTCADDCNT(skb, c)	BCM_REFERENCE(skb)
#define	PKTCSETLEN(skb, l)	BCM_REFERENCE(skb)
#define	PKTCADDLEN(skb, l)	BCM_REFERENCE(skb)
#define	PKTCSETFLAG(skb, fb)	BCM_REFERENCE(skb)
#define	PKTCCLRFLAG(skb, fb)	BCM_REFERENCE(skb)
#define	PKTCLINK(skb)		PKTLINK(skb)
#define	PKTSETCLINK(skb, x)	PKTSETLINK(skb, x)
#define	FOREACH_CHAINED_PKT(skb, nskb) \
	for (; (skb) != NULL; (skb) = (nskb)) \
		if ((nskb) = PKTCLINK(skb), PKTSETCLINK((skb), NULL), 1)
#define	PKTCFREE(osh, skb, send) \
do { \
	void *nskb; \
	ASSERT((skb) != NULL); \
	FOREACH_CHAINED_PKT((skb), nskb) { \
		PKTFREE((osh), (skb), (send)); \
	} \
} while (0)
#define PKTCENQTAIL(h, t, p) \
do { \
	if ((t) == NULL) { \
		(h) = (t) = (p); \
	} else { \
		PKTSETCLINK((t), (p)); \
		(t) = (p); \
	} \
} while (0)
#endif /* PKTC_DONGLE */

#ifdef PKTC_TX_DONGLE
#define	PKTSETCHAINED(o, skb)	({BCM_REFERENCE(o); lb_setchained(LBP(skb));})
#define	PKTISCHAINED(skb)	lb_ischained(LBP(skb))
#define	PKTCLRCHAINED(o, skb)	({BCM_REFERENCE(o); lb_clearchained(LBP(skb));})
#endif /* PKTC_TX_DONGLE */

extern void * osl_pktfrmnative(osl_t *osh, struct lbuf *lb);
extern struct lbuf * osl_pkttonative(osl_t *osh, void *p);
extern void * osl_pktget(osl_t *osh, uint len);
extern void * osl_pktalloc(osl_t *osh, uint len, enum lbuf_type lbuf_type);
extern void * osl_pktfrag_get(osl_t *osh, uint len, enum lbuf_type lbuf_type);
extern void osl_pktfree(osl_t *osh, void *p, bool send);
extern void * osl_pktdup(osl_t *osh, void *p);
extern void * osl_pktclone(osl_t *osh, void *p, int offset, int len);
extern void * osl_pktshrink(osl_t *osh, void *p);
extern uchar * osl_pktparams(osl_t *osh, void **p, uint32 *len, uint32 *fragix,
	uint16 *ftot, uint8* fbuf, uint32 *lo_addr, uint32 *hi_addr);

/* get system up time in milliseconds */
#define OSL_SYSUPTIME()		(hndrte_time())

/* Kernel: File Operations: start */
extern void * osl_os_open_image(char * filename);
extern int osl_os_get_image_block(char * buf, int len, void * image);
extern void osl_os_close_image(void * image);
/* Kernel: File Operations: end */

/* free memory available in pool */
#define OSL_MEM_AVAIL()         (hndrte_memavail())

/* Add memory block to heap "arena". */
#define OSL_ARENA_ADD(base, size)	(hndrte_arena_add(base, size))
extern void lfrag_trim_tailbytes(osl_t * osh, void* p, uint16 len);

#define OSL_RAND()		osl_rand()
extern uint32 osl_rand(void);

#define ROMMABLE_ASSERT(exp) do { \
		if (!(exp)) { \
			int *null = NULL; \
			*null = 0; \
		} \
	} while (0)

#endif	/* _hndrte_osl_h_ */
