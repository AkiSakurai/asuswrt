/*
 * HND Run Time Environment MIPS specific.
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: hndrte_mips.h 410377 2013-07-01 01:34:53Z $
 */

#ifndef _hndrte_mips_h_
#define _hndrte_mips_h_

#include <mips33_core.h>
#include <mips74k_core.h>
#include <mipsinc.h>

/* register access macros */
#ifdef BCMSIM
extern uint32 rreg32(volatile uint32 *r);
extern uint16 rreg16(volatile uint16 *r);
extern uint8 rreg8(volatile uint8 *r);
extern void wreg32(volatile uint32 *r, uint32 v);
extern void wreg16(volatile uint16 *r, uint16 v);
extern void wreg8(volatile uint8 *r, uint8 v);
#else
#define wreg32(r, v)	(*(volatile uint32*)(r) = (uint32)(v))
#define rreg32(r)	(*(volatile uint32*)(r))
#ifdef IL_BIGENDIAN
#define wreg16(r, v)	(*(volatile uint16*)((ulong)(r)^2) = (uint16)(v))
#define rreg16(r)	(*(volatile uint16*)((ulong)(r)^2))
#define wreg8(r, v)	(*(volatile uint8*)((ulong)(r)^3) = (uint8)(v))
#define rreg8(r)	(*(volatile uint8*)((ulong)(r)^3))
#else
#define wreg16(r, v)	(*(volatile uint16*)(r) = (uint16)(v))
#define rreg16(r)	(*(volatile uint16*)(r))
#define wreg8(r, v)	(*(volatile uint8*)(r) = (uint8)(v))
#define rreg8(r)	(*(volatile uint8*)(r))
#endif
#endif	/* BCMSIM */

/* uncached/cached virtual address */
#define	hndrte_uncached(va)	((void *)KSEG1ADDR((ulong)(va)))
#define	hndrte_cached(va)	((void *)KSEG0ADDR((ulong)(va)))

/* get processor cycle count */
#define osl_getcycles(x)	(2 * get_c0_count())

/* map/unmap physical to virtual I/O */
#ifdef BCMSIM
#define	hndrte_reg_map(pa, size)	ioremap((ulong)(pa), (ulong)(size))
extern void *ioremap(ulong offset, ulong size);
#else
#define	hndrte_reg_map(pa, size)	({BCM_REFERENCE(size); (void *)KSEG1ADDR((ulong)(pa));})
#endif	/* BCMSIM */
#define	hndrte_reg_unmap(va)		BCM_REFERENCE(va)

/* map/unmap shared (dma-able) memory */
#define	hndrte_dma_map(va, size) ({ \
	flush_dcache((uint32)va, size); \
	PHYSADDR((ulong)(va)); \
})
#define	hndrte_dma_unmap(pa, size)	({BCM_REFERENCE(pa); BCM_REFERENCE(size);})

/* Cache support */
extern void caches_on(void);
extern void blast_dcache(void);
extern void blast_icache(void);
extern void flush_dcache(uint32 base, uint size);
extern void flush_icache(uint32 base, uint size);

#endif	/* _hndrte_mips_h_ */
