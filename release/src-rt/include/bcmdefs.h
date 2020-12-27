/*
 * Misc system wide definitions
 *
 * Copyright (C) 2010, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: bcmdefs.h,v 13.69.10.2 2010-10-05 00:46:12 Exp $
 */

#ifndef	_bcmdefs_h_
#define	_bcmdefs_h_

/*
 * One doesn't need to include this file explicitly, gets included automatically if
 * typedefs.h is included.
 */

/* Use BCM_REFERENCE to suppress warnings about intentionally-unused function
 * arguments or local variables.
 */
#define BCM_REFERENCE(data)	((void)data)

/* Reclaiming text and data :
 * The following macros specify special linker sections that can be reclaimed
 * after a system is considered 'up'.
 * BCMATTACHFN is also used for detach functions (it's not worth having a BCMDETACHFN,
 * as in most cases, the attach function calls the detach function to clean up on error).
 */
#ifdef DONGLEBUILD

extern bool bcmreclaimed;

#define BCMATTACHDATA(_data)	__attribute__ ((__section__ (".dataini2." #_data))) _data
#define BCMATTACHFN(_fn)	__attribute__ ((__section__ (".textini2." #_fn), noinline)) _fn

#define BCMPREATTACHDATA(_data)	__attribute__ ((__section__ (".dataini2." #_data))) _data
#define BCMPREATTACHFN(_fn)	__attribute__ ((__section__ (".textini2." #_fn), noinline)) _fn

#if defined(BCMRECLAIM)
#define BCMINITDATA(_data)	__attribute__ ((__section__ (".dataini1." #_data))) _data
#define BCMINITFN(_fn)		__attribute__ ((__section__ (".textini1." #_fn), noinline)) _fn
#define CONST
#else
#define BCMINITDATA(_data)	_data
#define BCMINITFN(_fn)		_fn
#define CONST	const
#endif

/* Non-manufacture or internal attach function/dat */
#if !defined(WLTEST)
#define	BCMNMIATTACHFN(_fn)	BCMATTACHFN(_fn)
#define	BCMNMIATTACHDATA(_data)	BCMATTACHDATA(_data)
#else
#define	BCMNMIATTACHFN(_fn)	_fn
#define	BCMNMIATTACHDATA(_data)	_data
#endif	

#define BCMUNINITFN(_fn)	_fn

#define BCMFASTPATH

#ifdef DONGLEOVERLAYS
#define BCMOVERLAY0DATA(_sym)	__attribute__ ((__section__ (".r0overlay." #_sym))) _sym
#define BCMOVERLAY0FN(_fn)	__attribute__ ((__section__ (".r0overlay." #_fn))) _fn
#define BCMOVERLAY1DATA(_sym)	__attribute__ ((__section__ (".r1overlay." #_sym))) _sym
#define BCMOVERLAY1FN(_fn)	__attribute__ ((__section__ (".r1overlay." #_fn))) _fn
#define BCMOVERLAYERRFN(_fn)	__attribute__ ((__section__ (".overlayerr." #_fn))) _fn
#else
#define BCMOVERLAY0DATA(_sym)	_sym
#define BCMOVERLAY0FN(_fn)	_fn
#define BCMOVERLAY1DATA(_sym)	_sym
#define BCMOVERLAY1FN(_fn)	_fn
#define BCMOVERLAYERRFN(_fn)	_fn
#endif /* DONGLEOVERLAYS */

#else /* DONGLEBUILD */

#define bcmreclaimed 		0
#define BCMATTACHDATA(_data)	_data
#define BCMATTACHFN(_fn)	_fn
#define BCMPREATTACHDATA(_data)	_data
#define BCMPREATTACHFN(_fn)	_fn
#define BCMINITDATA(_data)	_data
#define BCMINITFN(_fn)		_fn
#define BCMUNINITFN(_fn)	_fn
#define	BCMNMIATTACHFN(_fn)	_fn
#define	BCMNMIATTACHDATA(_data)	_data
#define BCMOVERLAY0DATA(_sym)	_sym
#define BCMOVERLAY0FN(_fn)	_fn
#define BCMOVERLAY1DATA(_sym)	_sym
#define BCMOVERLAY1FN(_fn)	_fn
#define BCMOVERLAYERRFN(_fn)	_fn
#define CONST	const
#ifdef mips
#define BCMFASTPATH		__attribute__ ((__section__(".text.fastpath")))
#define BCMFASTPATH_HOST	__attribute__ ((__section__(".text.fastpath_host")))
#else
#define BCMFASTPATH
#define BCMFASTPATH_HOST
#endif

#endif /* DONGLEBUILD */

#if defined(BCMROMBUILD)
typedef struct {
	uint16 esiz;
	uint16 cnt;
	void *addr;
} bcmromdat_patch_t;
#endif

/* Put some library data/code into ROM to reduce RAM requirements */
#if defined(BCMROMBUILD)
#include <bcmjmptbl.h>
#define STATIC	static
#else /* !BCMROMBUILD */
#define BCMROMDATA(_data)	_data
#define BCMROMDAT_NAME(_data)	_data
#define BCMROMFN(_fn)		_fn
#define BCMROMFN_NAME(_fn)	_fn
#define STATIC	static
#define BCMROMDAT_ARYSIZ(data)	ARRAYSIZE(data)
#define BCMROMDAT_SIZEOF(data)	sizeof(data)
#define BCMROMDAT_APATCH(data)
#define BCMROMDAT_SPATCH(data)
#endif /* !BCMROMBUILD */

/* overlay function tagging */
#ifdef DONGLEBUILD
#ifdef DONGLEOVERLAYS
/* force a func to be inline if it's only accessed by overlays and ATTACH code */
#define OVERLAY_INLINE	__attribute__ ((always_inline))
#define OSTATIC
#define BCMOVERLAYDATA(_ovly, _sym) \
	__attribute__ ((aligned(4), __section__ (".r" #_ovly "overlay." #_sym))) _sym
#define BCMOVERLAYFN(_ovly, _fn) \
	__attribute__ ((__section__ (".r" #_ovly "overlay." #_fn))) _fn
#define BCMOVERLAYERRFN(_fn) \
	__attribute__ ((__section__ (".overlayerr." #_fn))) _fn
#define BCMROMOVERLAYDATA(_ovly, _sym)		BCMOVERLAYDATA(_ovly, _sym)
#define BCMROMOVERLAYFN(_ovly, _fn)			BCMOVERLAYFN(_ovly, _fn)
#define BCMATTACHOVERLAYDATA(_ovly, _sym)	BCMOVERLAYDATA(_ovly, _sym)
#define BCMATTACHOVERLAYFN(_ovly, _fn)		BCMOVERLAYFN(_ovly, _fn)
#define BCMINITOVERLAYDATA(_ovly, _sym)		BCMOVERLAYDATA(_ovly, _sym)
#define BCMINITOVERLAYFN(_ovly, _fn)		BCMOVERLAYFN(_ovly, _fn)
#define BCMUNINITOVERLAYFN(_ovly, _fn)		BCMOVERLAYFN(_ovly, _fn)
#else
#define OVERLAY_INLINE
#define OSTATIC			static
#define BCMOVERLAYDATA(_ovly, _sym)	_sym
#define BCMOVERLAYFN(_ovly, _fn)	_fn
#define BCMOVERLAYERRFN(_fn)	_fn
/* revert to standard definitions for BCMATTACH* and BCMINIT* if not overlay build */
#define BCMROMOVERLAYDATA(_ovly, _data)	BCMROMDATA(_data)
#define BCMROMOVERLAYFN(_ovly, _fn)		BCMROMFN(_fn)
#define BCMATTACHOVERLAYDATA(_ovly, _sym)	BCMATTACHDATA(_sym)
#define BCMATTACHOVERLAYFN(_ovly, _fn)		BCMATTACHFN(_fn)
#define BCMINITOVERLAYDATA(_ovly, _sym)		BCMINITDATA(_sym)
#define BCMINITOVERLAYFN(_ovly, _fn)		BCMINITFN(_fn)
#define BCMUNINITOVERLAYFN(_ovly, _fn)		BCMUNINITFN(_fn)
#endif /* DONGLEOVERLAYS */

#else

#define OVERLAY_INLINE
#define OSTATIC			static
#define BCMOVERLAYDATA(_ovly, _sym)	_sym
#define BCMOVERLAYFN(_ovly, _fn)	_fn
#define BCMOVERLAYERRFN(_fn)	_fn
#define BCMROMOVERLAYDATA(_ovly, _data)	BCMROMDATA(_data)
#define BCMROMOVERLAYFN(_ovly, _fn)		BCMROMFN(_fn)
#define BCMATTACHOVERLAYDATA(_ovly, _sym)	BCMATTACHDATA(_sym)
#define BCMATTACHOVERLAYFN(_ovly, _fn)		BCMATTACHFN(_fn)
#define BCMINITOVERLAYDATA(_ovly, _sym)		BCMINITDATA(_sym)
#define BCMINITOVERLAYFN(_ovly, _fn)		BCMINITFN(_fn)
#define BCMUNINITOVERLAYFN(_ovly, _fn)		BCMUNINITFN(_fn)

#endif /* DONGLEBUILD */

/* Bus types */
#define	SI_BUS			0	/* SOC Interconnect */
#define	PCI_BUS			1	/* PCI target */
#define	PCMCIA_BUS		2	/* PCMCIA target */
#define SDIO_BUS		3	/* SDIO target */
#define JTAG_BUS		4	/* JTAG */
#define USB_BUS			5	/* USB (does not support R/W REG) */
#define SPI_BUS			6	/* gSPI target */
#define RPC_BUS			7	/* RPC target */

/* Allows size optimization for single-bus image */
#ifdef BCMBUSTYPE
#define BUSTYPE(bus) 	(BCMBUSTYPE)
#else
#define BUSTYPE(bus) 	(bus)
#endif

/* Allows size optimization for single-backplane image */
#ifdef BCMCHIPTYPE
#define CHIPTYPE(bus) 	(BCMCHIPTYPE)
#else
#define CHIPTYPE(bus) 	(bus)
#endif


/* Allows size optimization for SPROM support */
#if defined(BCMSPROMBUS)
#define SPROMBUS	(BCMSPROMBUS)
#elif defined(SI_PCMCIA_SROM)
#define SPROMBUS	(PCMCIA_BUS)
#else
#define SPROMBUS	(PCI_BUS)
#endif

/* Allows size optimization for single-chip image */
#ifdef BCMCHIPID
#define CHIPID(chip)	(BCMCHIPID)
#else
#define CHIPID(chip)	(chip)
#endif

#ifdef BCMCHIPREV
#define CHIPREV(rev)	(BCMCHIPREV)
#else
#define CHIPREV(rev)	(rev)
#endif

/* Defines for DMA Address Width - Shared between OSL and HNDDMA */
#define DMADDR_MASK_32 0x0		/* Address mask for 32-bits */
#define DMADDR_MASK_30 0xc0000000	/* Address mask for 30-bits */
#define DMADDR_MASK_0  0xffffffff	/* Address mask for 0-bits (hi-part) */

#define	DMADDRWIDTH_30  30 /* 30-bit addressing capability */
#define	DMADDRWIDTH_32  32 /* 32-bit addressing capability */
#define	DMADDRWIDTH_63  63 /* 64-bit addressing capability */
#define	DMADDRWIDTH_64  64 /* 64-bit addressing capability */

#ifdef BCMDMA64OSL
typedef struct {
	uint32 loaddr;
	uint32 hiaddr;
} dma64addr_t;

typedef dma64addr_t dmaaddr_t;
#define PHYSADDRHI(_pa) ((_pa).hiaddr)
#define PHYSADDRHISET(_pa, _val) \
	do { \
		(_pa).hiaddr = (_val);		\
	} while (0)
#define PHYSADDRLO(_pa) ((_pa).loaddr)
#define PHYSADDRLOSET(_pa, _val) \
	do { \
		(_pa).loaddr = (_val);		\
	} while (0)

#else
typedef unsigned long dmaaddr_t;
#define PHYSADDRHI(_pa) (0)
#define PHYSADDRHISET(_pa, _val)
#define PHYSADDRLO(_pa) ((_pa))
#define PHYSADDRLOSET(_pa, _val) \
	do { \
		(_pa) = (_val);			\
	} while (0)
#endif /* BCMDMA64OSL */

/* One physical DMA segment */
typedef struct  {
	dmaaddr_t addr;
	uint32	  length;
} hnddma_seg_t;

#if defined(MACOSX)
/* In MacOS, the OS API may return large number of segments. Setting this number lower
 * will result in failure of dma map
 */
#define MAX_DMA_SEGS 8
#elif defined(__NetBSD__)
#define MAX_DMA_SEGS 16
#else
#define MAX_DMA_SEGS 4
#endif


typedef struct {
	void *oshdmah; /* Opaque handle for OSL to store its information */
	uint origsize; /* Size of the virtual packet */
	uint nsegs;
	hnddma_seg_t segs[MAX_DMA_SEGS];
} hnddma_seg_map_t;


/* packet headroom necessary to accommodate the largest header in the system, (i.e TXOFF).
 * By doing, we avoid the need  to allocate an extra buffer for the header when bridging to WL.
 * There is a compile time check in wlc.c which ensure that this value is at least as big
 * as TXOFF. This value is used in dma_rxfill (hnddma.c).
 */

#if defined(BCM_RPC_NOCOPY) || defined(BCM_RCP_TXNOCOPY)
/* add 40 bytes to allow for extra RPC header and info  */
#define BCMEXTRAHDROOM 220
#else /* BCM_RPC_NOCOPY || BCM_RPC_TXNOCOPY */
#ifdef CTFMAP
#define BCMEXTRAHDROOM 176
#else /* CTFMAP */
#define BCMEXTRAHDROOM 172
#endif /* CTFMAP */
#endif /* BCM_RPC_NOCOPY || BCM_RPC_TXNOCOPY */

/* Packet alignment for most efficient SDIO (can change based on platform) */
#ifndef SDALIGN
#define SDALIGN	32
#endif

/* Headroom required for dongle-to-host communication.  Packets allocated
 * locally in the dongle (e.g. for CDC ioctls or RNDIS messages) should
 * leave this much room in front for low-level message headers which may
 * be needed to get across the dongle bus to the host.  (These messages
 * don't go over the network, so room for the full WL header above would
 * be a waste.).
*/
#define BCMDONGLEHDRSZ 12
#define BCMDONGLEPADSZ 16

#define BCMDONGLEOVERHEAD	(BCMDONGLEHDRSZ + BCMDONGLEPADSZ)

#ifdef BCMDBG

#ifndef BCMDBG_ERR
#define BCMDBG_ERR
#endif /* BCMDBG_ERR */

#ifndef BCMDBG_ASSERT
#define BCMDBG_ASSERT
#endif /* BCMDBG_ASSERT */

#endif /* BCMDBG */

#if defined(BCMDBG_ASSERT)
#define BCMASSERT_SUPPORT
#endif 

/* Macros for doing definition and get/set of bitfields
 * Usage example, e.g. a three-bit field (bits 4-6):
 *    #define <NAME>_M	BITFIELD_MASK(3)
 *    #define <NAME>_S	4
 * ...
 *    regval = R_REG(osh, &regs->regfoo);
 *    field = GFIELD(regval, <NAME>);
 *    regval = SFIELD(regval, <NAME>, 1);
 *    W_REG(osh, &regs->regfoo, regval);
 */
#define BITFIELD_MASK(width) \
		(((unsigned)1 << (width)) - 1)
#define GFIELD(val, field) \
		(((val) >> field ## _S) & field ## _M)
#define SFIELD(val, field, bits) \
		(((val) & (~(field ## _M << field ## _S))) | \
		 ((unsigned)(bits) << field ## _S))

/* define BCMSMALL to remove misc features for memory-constrained environments */
#ifdef BCMSMALL
#undef	BCMSPACE
#define bcmspace	FALSE	/* if (bcmspace) code is discarded */
#else
#define	BCMSPACE
#define bcmspace	TRUE	/* if (bcmspace) code is retained */
#endif

/* Max. nvram variable table size */
#define	MAXSZ_NVRAM_VARS	4096

#define LOCATOR_EXTERN static

#endif /* _bcmdefs_h_ */
