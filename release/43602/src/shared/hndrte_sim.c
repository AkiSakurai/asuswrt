/*
 * Initialization and support routines for self-booting
 * compressed image.
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: hndrte_sim.c 413938 2013-07-23 01:10:28Z $
 */

#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmdevs.h>
#include <siutils.h>
#include <sbsdram.h>

#include <time.h>

#ifdef DONGLEBUILD
/*
 * Reclaim support stub
 */

bool bcmreclaimed = FALSE;

void
hndrte_reclaim(void)
{
}
#endif /* DONGLEBUILD */

void
hndrte_cpu_init(si_t *sih)
{
	(void)sih;
}

void
hndrte_delay(uint32 us)
{
}

#ifndef __clock_t_defined
#define clock_t unsigned int
#define	CLOCKS_PER_SEC	100
extern clock_t clock(void);
#endif

/* We'll keep time in ms */
static clock_t lastclock = 0;
static uint32 now = 0;

uint32
hndrte_update_now(void)
{
	clock_t newclock;
	uint32 diffms;

	newclock = clock();
	ASSERT(((clock_t)-1) != newclock);

	diffms = ((uint32)newclock - (uint32)lastclock) * 1000 / CLOCKS_PER_SEC;
	if (0 != diffms) {
		lastclock = newclock;
		now += diffms;
	}
	return diffms;
}

#ifdef BCMDBG_MEM
void *
hndrte_dma_alloc_consistent(uint size, uint16 align_bits, uint *alloced, void *pap,
	char *file, int line)
#else
void *
hndrte_dma_alloc_consistent(uint size, uint16 align_bits, uint *alloced, void *pap)
#endif
{
	void *va;


#ifdef BCMDBG_MEM
	va = hndrte_malloc_align(size, align_bits, file, line);
#else
	va = hndrte_malloc_align(size, align_bits);
#endif
	*alloced = size;

	if (NULL != va)
		*(uintptr*)pap = virt_to_phys(va);

	return va;
}

void
hndrte_dma_free_consistent(void *va)
{
	hndrte_free(va);
}

void
hndrte_set_irq_timer(uint ms)
{
}

void
hndrte_ack_irq_timer(void)
{
}

void
hndrte_wait_irq(si_t *sih)
{
}

void
_hndrte_die(bool trap)
{
	while (1)
		;
}

void
hndrte_enable_interrupts(void)
{
}

void
hndrte_disable_interrupts(void)
{
}

uint32
hndrte_set_trap(uint32 hook)
{
	return 0;
}

void
hndrte_set_fwhalt(hndrte_halt_handler fwhalt_handler)
{
}

void
hndrte_trap_init(void)
{
}

void
hndrte_memtrace_enab(bool on)
{
}

void
hndrte_trap_handler(trap_t *tr)
{
}

void
hndrte_cons_addcmd(const char *name, cons_fun_t fun, uint32 arg)
{
}

void
hndrte_cons_init(si_t *sih)
{
}

void *
hndrte_get_active_cons_state(void)
{
	return NULL;
}

int
hndrte_log_init(void)
{
	return 0;
}

#ifndef EXT_CBALL
void
si_cc_isr(si_t *sih, chipcregs_t *regs)
{
}
#endif

#ifdef CORTEXM3TIMESTAMPS

uint32 curr_time()
{
	unsigned int volatile *p = (unsigned int volatile *)0xe000e010;
	unsigned int a = p[0];
	if (a & 0x00010000)
	{
		printf("ERROR: CortexM3 time-stamp overflowed\n");
		return 0xBADBAD;
	}
	else
	{
		/* printf(" elapsed cycles = %u\n",0x01000000 - p[2]);
		return (0x01000000 - p[2]) ;
		*/
		return (0xFFFFFF & p[2]);
	}

}

#endif /* CORTEXM3TIMESTAMPS */
