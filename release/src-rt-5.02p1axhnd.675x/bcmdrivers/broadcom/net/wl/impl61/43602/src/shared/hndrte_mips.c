/*
 * Initialization and support routines for self-booting
 * compressed image.
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
 * $Id: hndrte_mips.c 413605 2013-07-19 19:06:36Z $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmdevs.h>
#include <siutils.h>
#include <hndmips.h>
#include <mips33_core.h>
#include <mips74k_core.h>
#include <hndcpu.h>

#define ALLINTS (IE_IRQ0 | IE_IRQ1 | IE_IRQ2 | IE_IRQ3 | IE_IRQ4 | IE_IRQ5)

/*
 * Timing support:
 *	hndrte_delay(us): Delay us microseconds.
 *	hndrte_time(): Return milliseconds since boot.
 */

#define	MAXUINT32	0xffffffff	/* Max val for uint32 */

/* Default to 125 MHz */
static uint32 c0counts_per_us = 125000000 / 2000000;
static uint32 c0counts_per_ms = 125000000 / 2000;

/* We'll keep time in ms */
static uint32 lastcount = 0;

uint32
hndrte_update_now(void)
{
	uint32 count, diff;

	count = get_c0_count();
	if (count > lastcount)
		diff = count - lastcount;
	else
		diff = (MAXUINT32 - lastcount) + count + 1;

	if (diff >= c0counts_per_ms) {
		lastcount = count;
		return diff / c0counts_per_ms;
	}

	return 0;
}

void
hndrte_delay(uint32 us)
{
	uint32 curr, lim;

#ifdef BCMQT
	/* To compensate for the slowness of quickturn */
	us /= 3000;
#endif  /* BCMQT */

	curr = get_c0_count();
	lim = curr + (us * c0counts_per_us);

	if (lim < curr)
		while (get_c0_count() > curr)
			;

	while (get_c0_count() < lim)
		;
}

/*
 * hdnrte.
 *
 * Cache support:
 *	flush_dcache(base, size): Flush (wb & inv) dcache range
 *	flush_icache(base, size): Flush (inv) icache range
 *	blast_dcache(): Flush (wb & inv) all of the dcache
 *	blast_icache(): Flush (inv) all of the icache
 *	caches_on(): Initialize and turn caches on
 */

/* Cache and line sizes */
static uint icache_size, ic_lsize, dcache_size, dc_lsize;

void
flush_dcache(uint32 base, uint size)
{
	uint32 start, end, mask;

	if (size >= dcache_size) {
		blast_dcache();
		return;
	}

	mask = dc_lsize - 1;
	start = base & ~mask;
	end = (base + size + mask) & ~mask;

	while (start < end) {
		cache_op(start, Hit_Writeback_Inv_D);
		start += dc_lsize;
	}
}

void
flush_icache(uint32 base, uint size)
{
	uint32 start, end, mask;

	if (size >= icache_size) {
		blast_icache();
		return;
	}

	mask = ic_lsize - 1;
	start = base & ~mask;
	end = (base + size + mask) & ~mask;

	while (start < end) {
		cache_op(start, Hit_Invalidate_I);
		start += ic_lsize;
	}
}

void
blast_dcache(void)
{
	uint32 start, end;

	start = KSEG0;
	end = start + dcache_size;

	while (start < end) {
		cache_op(start, Index_Writeback_Inv_D);
		start += dc_lsize;
	}
}

void
blast_icache(void)
{
	uint32 start, end;

	start = KSEG0;
	end = start + icache_size;

	while (start < end) {
		cache_op(start, Index_Invalidate_I);
		start += ic_lsize;
	}
}

static void
_change_cachability(uint32 cm)
{
	uint32 prid, c0reg;

	c0reg = MFC0(C0_CONFIG, 0);
	c0reg &= ~CONF_CM_CMASK;
	c0reg |= (cm & CONF_CM_CMASK);
	MTC0(C0_CONFIG, 0, c0reg);
	prid = MFC0(C0_PRID, 0);
	if (BCM330X(prid)) {
		c0reg = MFC0(C0_BROADCOM, 0);
		/* Enable icache & dcache */
		c0reg |= BRCM_IC_ENABLE | BRCM_DC_ENABLE;
		MTC0(C0_BROADCOM, 0, c0reg);
	}
}
static void (*change_cachability)(uint32);

void
BCMINITFN(caches_on)(void)
{
	uint32 config1;
	uint start, end, size, lsize;

	/* Save cache config */
	config1 = MFC0(C0_CONFIG, 1);

	icache_probe(config1, &size, &lsize);
	icache_size = size;
	ic_lsize = lsize;

	dcache_probe(config1, &size, &lsize);
	dcache_size = size;
	dc_lsize = lsize;

	/* If caches are not in the default state then
	 * presume that caches are already init'd
	 */
	if ((MFC0(C0_CONFIG, 0) & CONF_CM_CMASK) != CONF_CM_UNCACHED) {
		blast_dcache();
		blast_icache();
		enable_pfc(PFC_AUTO);
		return;
	}

	/* init icache */
	start = KSEG0;
	end = (start + icache_size);
	MTC0(C0_TAGLO, 0, 0);
	MTC0(C0_TAGHI, 0, 0);
	while (start < end) {
		cache_op(start, Index_Store_Tag_I);
		start += ic_lsize;
	}

	/* init dcache */
	start = KSEG0;
	end = (start + dcache_size);
	if (MFC0(C0_CONFIG, 0) & CONF_AR) {
		/* mips32r2 has the data tags in select 2 */
		MTC0(C0_TAGLO, 2, 0);
		MTC0(C0_TAGHI, 2, 0);
	} else {
		MTC0(C0_TAGLO, 0, 0);
		MTC0(C0_TAGHI, 0, 0);
	}
	while (start < end) {
		cache_op(start, Index_Store_Tag_D);
		start += dc_lsize;
	}

	/* Must be in KSEG1 to change cachability */
	change_cachability = (void (*)(uint32)) KSEG1ADDR(_change_cachability);
	change_cachability(CONF_CM_CACHABLE_NONCOHERENT);

	/* And enable pfc */
	enable_pfc(PFC_AUTO);
}

/*
 * hndrte.
 *
 * Initialize and background:
 *	hndrte_cpu_init: Initialize the world.
 */

void
BCMATTACHFN(hndrte_cpu_init)(si_t *sih)
{
	si_mips_init(sih, 0);

	/* Set trap handler */
	hndrte_set_trap((uint32)hndrte_trap_handler);

	/* Initialize timing */
	c0counts_per_us = si_cpu_clock(sih) / 2000000;
	c0counts_per_ms = si_cpu_clock(sih) / 2000;

#ifndef HNDRTE_POLLING
	/* Enable individual interrupts, disable globally */
	MTC0(C0_STATUS, 0, (MFC0(C0_STATUS, 0) | ALLINTS) & ~ST0_IE);
#endif // endif
}

void
hndrte_idle_init(si_t *sih)
{
}

void *
#if BCMDBG_MEM
hndrte_dma_alloc_consistent(uint size, uint16 align_bits, uint *alloced, void *pap,
	char *file, int line)
#else
hndrte_dma_alloc_consistent(uint size, uint16 align_bits, uint *alloced, void *pap)
#endif /* BCMDBG_MEM */
{
	void *buf;
	uint16 align;

	align = (1 << align_bits);

	/* align on a 4k boundary (2^12) */
#if BCMDBG_MEM
	if (!(buf = hndrte_malloc_align(size, align_bits, file, line)))
#else
	if (!(buf = hndrte_malloc_align(size, align_bits)))
#endif /* BCMDBG_MEM */
		return NULL;

	ASSERT(ISALIGNED(buf, align));

	*alloced = size;

	*((ulong *) pap) = PHYSADDR((ulong) buf);

	return (void *) KSEG1ADDR((ulong) buf);
}

void
hndrte_dma_free_consistent(void *va)
{
	/* convert back to cached before freeing */
	hndrte_free((void*)KSEG0ADDR(va));
}

void
hndrte_ack_irq_timer(void)
{
}

void
hndrte_set_irq_timer(uint ms)
{
	uint nCycles;

	nCycles = ms * c0counts_per_ms;

	if (!nCycles)
		nCycles = c0counts_per_us;  /* account for lag b/n read count & write compare */

	MTC0(C0_COMPARE, 0, MFC0(C0_COUNT, 0) + nCycles);
}

void
hndrte_wait_irq(si_t *sih)
{
	__asm__ __volatile__(
		".set\tmips3\n\t"
		"sync\n\t"
		".set\tmips0");
}

void
hndrte_enable_interrupts(void)
{
	MTC0(C0_STATUS, 0, MFC0(C0_STATUS, 0) | ST0_IE);
}

void
hndrte_disable_interrupts(void)
{
	MTC0(C0_STATUS, 0, MFC0(C0_STATUS, 0) & ~ST0_IE);
}

/* Trap initialization */
void
hndrte_trap_init(void)
{
}

void
hndrte_memtrace_enab(bool on)
{
}

/* set fwhalt handler */
void
hndrte_set_fwhalt(hndrte_halt_handler fwhalt_handler)
{
}

void
hndrte_trap_handler(trap_t *tr)
{
	uint exc = (tr->cause & C_EXC) >> C_EXC_SHIFT;
	if (exc != 0) {
#ifndef	BCMSIM
		if (tr->epc == (uint32)&osl_busprobe) {
			/* printf("busprobe failed for addr 0x%08x @ 0x%08x\n", tr->r5, tr->r31); */
			tr->r2 = 0xffffffff;
			tr->epc = tr->r31;
			return;
		}
#endif	/* BCMSIM */
		printf("\n\nTrap type 0x%x @ pc 0x%08x\n",
		       tr->type, (tr->cause & C_BD) ? tr->epc + 4 : tr->epc);
		printf("  cause 0x%08x (exc %d), status 0x%08x, badva 0x%08x\n",
		       tr->cause, exc, tr->status, tr->badvaddr);
#ifdef BCMSPACE
		printf("  zr 0x%08x, at 0x%08x, v0 0x%08x, v1 0x%08x\n",
		       tr->r0, tr->r1, tr->r2, tr->r3);
		printf("  a0 0x%08x, a1 0x%08x, a2 0x%08x, a3 0x%08x\n",
		       tr->r4, tr->r5, tr->r6, tr->r7);
		printf("  t0 0x%08x, t1 0x%08x, t2 0x%08x, t4 0x%08x\n",
		       tr->r8, tr->r9, tr->r10, tr->r11);
		printf("  t4 0x%08x, t4 0x%08x, t6 0x%08x, t7 0x%08x\n",
		       tr->r12, tr->r13, tr->r14, tr->r15);
		printf("  s0 0x%08x, s1 0x%08x, s2 0x%08x, s3 0x%08x\n",
		       tr->r16, tr->r17, tr->r18, tr->r19);
		printf("  s4 0x%08x, s5 0x%08x, s6 0x%08x, s7 0x%08x\n",
		       tr->r20, tr->r21, tr->r22, tr->r23);
		printf("  t8 0x%08x, t9 0x%08x, jp 0x%08x, k0 0x%08x\n",
		       tr->r24, tr->r25, tr->r26, tr->r27);
		printf("  gp 0x%08x, sp 0x%08x, s8 0x%08x, ra 0x%08x\n",
		       tr->r28, tr->r29, tr->r30, tr->r31);
#else /* BCMSPACE */
		printf("  sp 0x%08x, ra 0x%08x\n", tr->r29, tr->r31);
#endif /* BCMSPACE */

		/* Halt processing w/o forcing a trap since we are already in the trap handler. */
		hndrte_die_no_trap();
		/*NOTREACHED*/
	}

#ifndef HNDRTE_POLLING
	/* Disable interrupts during ISR to prevent nesting */
	hndrte_disable_interrupts();

	/* Clear EXL bit so EPC will be set properly if the ISR traps */
	MTC0(C0_STATUS, 0, MFC0(C0_STATUS, 0) & ~ST0_EXL);

	if (tr->cause & C_IRQ5) {
		/* timer interrupt */
		MTC0(C0_COMPARE, 0, MFC0(C0_COMPARE, 0)); /* clear the interrupt */
	}
	hndrte_isr();

	/* Prevent new interrupts until eret */
	MTC0(C0_STATUS, 0, MFC0(C0_STATUS, 0) | ST0_EXL);

	/* Accept interrupts after eret */
	hndrte_enable_interrupts();
#endif	/* !HNDRTE_POLLING */
}
