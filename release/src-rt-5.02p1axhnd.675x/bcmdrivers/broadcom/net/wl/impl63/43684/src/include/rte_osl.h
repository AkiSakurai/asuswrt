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
 * $Id: rte_osl.h 778696 2019-09-09 03:36:48Z $
 */

#ifndef _rte_osl_h_
#define _rte_osl_h_

#include <typedefs.h>
#include <osl_decl.h>

#if defined(_RTE_SIM_)
#include <rte_sim.h>
#elif defined(mips)
#include <rte_mips.h>
#elif defined(__arm__) || defined(__thumb__) || defined(__thumb2__)
#include <rte_arm.h>
#else
#error "unsupported CPU architecture"
#endif // endif

/* This definition tells the OSL layer that the OS implement a MEMCHECK API */
#if defined(BCMDBG_MEM) || defined(BCMDBG_HEAPCHECK)
#define OSL_MEMCHECK 1
#endif // endif

/* for ASSERT() */
#include <rte_assert.h>

/* for MALLOC/MFREE */
#include <rte_heap.h>

#include <rte_trap.h>

/* XXX It doesn't belong here but other OSLs do so and there are source files
 * including OSL but not bcmstdlib.h...
 */
#include <bcmstdlib.h>

#define OSH_NULL   NULL

#define DECLSPEC_ALIGN(x)	__attribute__ ((aligned(x)))

/* PCMCIA attribute space access macros */
#define	OSL_PCMCIA_READ_ATTR(osh, offset, buf, size)	\
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
extern uint32 osl_pci_read_config(osl_t *osh, uint offset, uint size);
#define	OSL_PCI_WRITE_CONFIG(osh, offset, size, val) \
		osl_pci_write_config((osh), (offset), (size), (val))
extern void osl_pci_write_config(osl_t *osh, uint offset, uint size, uint val);
/* PCI device bus # and slot # */
#define OSL_PCI_BUS(osh)	osl_pci_bus(osh)
#define OSL_PCI_SLOT(osh)	osl_pci_slot(osh)
extern uint osl_pci_slot(osl_t *osh);
#define OSL_PCIE_DOMAIN(osh)	({BCM_REFERENCE(osh); (0);})
#define OSL_PCIE_BUS(osh)	({BCM_REFERENCE(osh); (0);})
extern uint osl_pci_bus(osl_t *osh);
#else	/* SBPCI */
#define	OSL_PCI_READ_CONFIG(osh, offset, size) \
	({BCM_REFERENCE(osh); (((offset) == 8) ? 0 : 0xffffffff);})
#define	OSL_PCI_WRITE_CONFIG(osh, offset, size, val) \
	({BCM_REFERENCE(osh); BCM_REFERENCE(val);})
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
	 sizeof(*(r)) == sizeof(uint32) ? rreg32((volatile uint32 *)(r)) : \
	 sizeof(*(r)) == sizeof(uint16) ? rreg16((volatile uint16 *)(r)) : \
					  rreg8((volatile uint8 *)(r)); \
	 })
#define	_W_REG(osh, r, v) \
	do { \
		BCM_REFERENCE(osh); \
		if (sizeof(*(r)) == sizeof(uint32)) \
			wreg32((volatile uint32 *)(r), (uint32)(v)); \
		else if (sizeof(*(r)) == sizeof(uint16)) \
			wreg16((volatile uint16 *)(r), (uint16)(v)); \
		else \
			wreg8((volatile uint8 *)(r), (uint8)(v)); \
	} while (0)

#define W_REG(osh, r, v) do { \
	SIZECHECK(*(r), (v)); \
	_W_REG(osh, r, v); \
} while (0)

#define AND_REG(osh, r, v) do { \
	SIZECHECK(*(r), (v)); \
	_W_REG(osh, (r), R_REG(osh, r) & (v)); \
} while (0)

#define OR_REG(osh, r, v) do { \
	SIZECHECK(*(r), (v)); \
	_W_REG(osh, (r), R_REG(osh, r) | (v)); \
} while (0)
/* OSL initialization */
typedef struct osl_cmn_info osl_cmn_t;

struct osl_cmn_info {
	uint pktalloced;	/* Number of allocated packet buffers */
	uint refcount;
};

#ifdef STS_FIFO_RXEN
extern void (*stsbuf_free_cb_fn)(void *ctx, void *pkt);
extern void *stsbuf_free_cb_ctx;
#endif // endif

struct osl_info {
	void *dev;		/* Device handle */
	osl_cmn_t *cmn; /* Common OSL related data */
	pktfree_cb_fn_t tx_fn;	/* Callback function for PKTFREE */
	void *tx_ctx;		/* Context to the callback function */
	void (*rx_fn)(void *rx_ctx, void *p);	/* Rx callback for PKTFREE */
	void *rx_ctx;		/* Context to rx callback function */
	void *bme;		/* Byte Move Engine handler */
};

#ifdef SHARED_OSL_CMN
extern osl_t *osl_attach(void *pdev, void **osl_cmn);
#else
extern osl_t *osl_attach(void *pdev);
#endif /* SHARED_OSL_CMN */
extern void osl_detach(osl_t *osh);

#define PKTFREESETCB(osh, _tx_fn, _tx_ctx) \
	do { \
	   osh->tx_fn = _tx_fn; \
	   osh->tx_ctx = _tx_ctx; \
	} while (0)

#define PKTFREESETRXCB(osh, _rx_fn, _rx_ctx) \
	do { \
	   osh->rx_fn = _rx_fn; \
	   osh->rx_ctx = _rx_ctx; \
	} while (0)

/** Register BME service with OSH */
#define OSH_SETBME(osh, bmeh)   (osh)->bme = (bmeh)
#define OSH_GETBME(osh)	        (osh)->bme

/* general purpose memory allocation */
#define PERSIST_ALWAYS					(0)
#define PERSIST_BEFORE_ATTACHRECLAIM	(1<<0)

extern void osl_malloc_set_nopersist(void);
#define MALLOC_SET_NOPERSIST(osh)	({BCM_REFERENCE(osh); osl_malloc_set_nopersist();})

extern void osl_malloc_clear_nopersist(void);
#define MALLOC_CLEAR_NOPERSIST(osh)	({BCM_REFERENCE(osh); osl_malloc_clear_nopersist();})

#if defined(BCMDBG_MEM) || defined(BCMDBG_MEMFAIL)

extern void *osl_malloc_align(uint size, uint align_bits, void *call_site);
#define	MALLOC_ALIGN(osh, size, align_bits) \
	({BCM_REFERENCE(osh); osl_malloc_align((size), (align_bits), CALL_SITE);})
#define	MALLOC_ALIGN_CALLSITE(osh, size, align_bits, call_site) \
		({BCM_REFERENCE(osh); osl_malloc_align((size), (align_bits), call_site);})

#else /* !BCMDBG_MEM */

extern void *osl_malloc_align(uint size, uint align_bits);
#define	MALLOC_ALIGN(osh, size, align_bits) \
	({BCM_REFERENCE(osh); osl_malloc_align((size), (align_bits));})
#endif /* BCMDBG_MEM */

extern void *osl_malloc(uint size);
#define	MALLOC(osh, size)	({BCM_REFERENCE(osh); osl_malloc((size));})

extern void *osl_mallocz(uint size);
#define	MALLOCZ(osh, size)	\
	({BCM_REFERENCE(osh); osl_mallocz((size));})

extern void *osl_malloc_nopersist(uint size);
#define MALLOC_NOPERSIST(osh, size)	\
	({BCM_REFERENCE(osh); osl_malloc_nopersist((size));})

extern void *osl_mallocz_nopersist(uint size);
#define MALLOCZ_NOPERSIST(osh, size)	\
	({BCM_REFERENCE(osh); osl_mallocz_nopersist((size));})

/* Note: Persist macros must be used only for alloc that are never freed or freed in detach only */

extern void *osl_malloc_persist(uint size, uint32 constraint);
#define	MALLOC_PERSIST(osh, size)	\
	({BCM_REFERENCE(osh); osl_malloc_persist((size), PERSIST_ALWAYS);})

#define	MALLOC_PERSIST_ATTACH(osh, size) \
	({BCM_REFERENCE(osh); osl_malloc_persist((size), PERSIST_BEFORE_ATTACHRECLAIM);})

extern void *osl_mallocz_persist(uint size, uint32 constraint);
#define	MALLOCZ_PERSIST(osh, size) \
	({BCM_REFERENCE(osh); osl_mallocz_persist((size), PERSIST_ALWAYS);})

#define	MALLOCZ_PERSIST_ATTACH(osh, size) \
	({BCM_REFERENCE(osh); osl_mallocz_persist((size), PERSIST_BEFORE_ATTACHRECLAIM);})

#define	MFREE_PERSIST(osh, addr, size)	\
	({BCM_REFERENCE(osh); BCM_REFERENCE(size); hnd_free_persist(addr);})

extern int osl_mfree(void *addr);
#define	MFREE(osh, addr, size)	\
	({BCM_REFERENCE(osh); BCM_REFERENCE(size); osl_mfree((addr));})

/* Add memory block to heap "arena". */
#define OSL_ARENA_ADD(base, size) \
	(hnd_arena_add(base, size))
#define	MALLOCED(osh)		osl_malloced((osh))
extern uint osl_malloced(osl_t *osh);
/* free memory available in pool */
#define OSL_MEM_AVAIL()         (hnd_memavail())
#define	MALLOC_FAILED(osh)	osl_malloc_failed((osh))
extern uint osl_malloc_failed(osl_t *osh);
#define	MALLOC_DUMP(osh, b)	BCM_REFERENCE(osh)
#define OSL_ERROR(bcmerror)	osl_error(bcmerror)
extern int osl_error(int bcmerror);

/* microsecond delay (spinwait) */
#define	OSL_DELAY(usec)		hnd_delay(usec)
extern void hnd_delay(uint32 us);

/* microsecond sleep (thread sleep) */
#define OSL_SLEEP(usec)		hnd_thread_sleep(usec)
extern void hnd_thread_sleep(uint us);

/* get processor cycle count */
#define OSL_GETCYCLES(x)	((x) = osl_getcycles())
/* get system up time in milliseconds */
#define OSL_SYSUPTIME()		hnd_time()
extern uint32 hnd_time(void);
extern uint32 hnd_get_reftime_ms(void);
extern void hnd_set_reftime_ms(uint32 ms);
/* get system up time in microseconds */
/* XXX The current implmentation only works when CPU doesn't run across "wfi"
 * instruction (where CPU cycle counting may stop) in the idle loop...
 * An alternative is to switch to the free running PMU timer if resolution
 * is not a concern...
 * Also this is currently offered as a debug/profile facility and frequent
 * calling to this API is required as the implementation is not handling
 * the counter wrap condition!
 * USE IT WITH CAUTION!
 */
#define OSL_SYSUPTIME_US()	hnd_time_us()
uint64 hnd_time_us(void);

#if defined(BCMDBG_MEM) || defined(BCMDBG_HEAPCHECK)
extern int osl_memcheck(const char *file, int line);
#endif // endif

/* uncached/cached virtual address */
#define	OSL_UNCACHED(va)	hnd_uncached(va)
#define	OSL_CACHED(va)		hnd_cached(va)

#define OSL_CACHE_FLUSH(va, len)	BCM_REFERENCE(va)
#define OSL_CACHE_INV(va, len)		BCM_REFERENCE(va)
#define OSL_CPU_RELAX()			(0)

#define OSL_DISABLE_PREEMPTION(osh) BCM_REFERENCE(osh)
#define OSL_ENABLE_PREEMPTION(osh)	BCM_REFERENCE(osh)

#define OSL_PREF_RANGE_LD(va, sz)	BCM_REFERENCE(va)
#define OSL_PREF_RANGE_ST(va, sz)	BCM_REFERENCE(va)

/* host/bus architecture-specific address byte swap */
#define BUS_SWAP32(v)		(v)

/* dereference an address that may cause a bus exception */
#define	BUSPROBE(val, addr)	osl_busprobe(&(val), (uint32)(addr))
extern int osl_busprobe(uint32 *val, uint32 addr);

/* allocate/free shared (dma-able) consistent (uncached) memory */
#define DMA_CONSISTENT_ALIGN_BITS	2
#define	DMA_CONSISTENT_ALIGN		(1 << DMA_CONSISTENT_ALIGN_BITS)

#define	DMA_ALLOC_CONSISTENT(osh, size, align, tot, pap, dmah) \
	({BCM_REFERENCE(osh); hnd_dma_alloc_consistent(size, align, (tot), (void *)(pap));})
void *hnd_dma_alloc_consistent(uint size, uint16 align_bits, uint *alloced, void *pap);

#define	DMA_FREE_CONSISTENT(osh, va, size, pa, dmah) \
	({BCM_REFERENCE(osh); hnd_dma_free_consistent((void*)(va));})
void hnd_dma_free_consistent(void *va);

/* map/unmap direction */
#define	DMA_TX			1	/* TX direction for DMA */
#define	DMA_RX			2	/* RX direction for DMA */

/** API for DMA addressing capability */
#define OSL_DMADDRWIDTH(osh, addrwidth) BCM_REFERENCE(osh)

/* map/unmap physical to virtual I/O */
#define	REG_MAP(pa, size)		hnd_reg_map(pa, size)
#define	REG_UNMAP(va)			hnd_reg_unmap(va)

/* map/unmap shared (dma-able) memory */
#define	DMA_MAP(osh, va, size, direction, lb, dmah) \
	({ \
	 BCM_REFERENCE(osh); \
	 ((dmaaddr_t)hnd_dma_map(va, size)); \
	 })
#define	DMA_UNMAP(osh, pa, size, direction, p, dmah) \
	({ \
	 BCM_REFERENCE(osh); \
	 hnd_dma_unmap((uint32)pa, size); \
	 })

#define BULK_DMA_MAP(osh, dmah, map_start, map_end) \
	({\
		BCM_REFERENCE(dmah); \
		BCM_REFERENCE(osh); \
		BCM_REFERENCE(map_start); \
		BCM_REFERENCE(map_end); \
	})

#define BULK_DMA_UNMAP(osh, dmah, map_start, map_end) \
	({\
		BCM_REFERENCE(dmah); \
		BCM_REFERENCE(osh); \
		BCM_REFERENCE(map_start); \
		BCM_REFERENCE(map_end); \
	})

#define	DMA_SYNC(osh, pa, size, direction)		BCM_REFERENCE(pa)

#if defined(__ARM_ARCH_7A__) && defined(CA7)
#define DMB()	do { asm volatile("dmb"); } while (0)
#endif // endif

/* shared (dma-able) memory access macros */
#define	R_SM(r)				*(r)
#define	W_SM(r, v)			(*(r) = (v))
#define	BZERO_SM(r, len)		memset((r), '\0', (len))

#define OSL_RAND()			osl_rand()
extern uint32 osl_rand(void);

/* halt the system */
#define OSL_SYS_HALT()			HND_DIE()

/* Kernel: File Operations: start */
#define osl_os_open_image(filename)	({BCM_REFERENCE(osh); NULL;})
#define osl_os_get_image_block(buf, len, image)	({BCM_REFERENCE(osh); 0;})
#define osl_os_close_image(image)	BCM_REFERENCE(osh)
/* Kernel: File Operations: end */

extern void *osl_get_fatal_logbuf(osl_t *osh, uint32 request_size, uint32 *allocated_size);
#define OSL_GET_FATAL_LOGBUF(osh, size, alloced)	\
	(osl_get_fatal_logbuf((osh), (size), (alloced)))
#define OSL_SMP_WMB()                          do { /* noop */ } while (0)

#define	bcopy_hw(src, dst, len)		memcpy((dst), (src), (len))
#define	bcopy_hw_async(src, dst, len)	memcpy((dst), (src), (len))
#define	bcopy_hw_poll_for_completion()
#endif	/* _rte_osl_h_ */
