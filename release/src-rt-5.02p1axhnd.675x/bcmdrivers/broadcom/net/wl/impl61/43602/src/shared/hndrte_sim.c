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
#endif // endif

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
#endif // endif
{
	void *va;

#ifdef BCMDBG_MEM
	va = hndrte_malloc_align(size, align_bits, file, line);
#else
	va = hndrte_malloc_align(size, align_bits);
#endif // endif
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
#endif // endif

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
