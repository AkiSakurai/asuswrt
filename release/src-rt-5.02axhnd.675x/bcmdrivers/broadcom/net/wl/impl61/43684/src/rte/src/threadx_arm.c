/*
 * ThreadX ARM port specific routines.
 * Contains functions that are specific to both ARM processor *and* RTE RTOS.
 *
 * Copyright 2019 Broadcom
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
 * $Id: threadx_arm.c 763814 2018-05-22 09:11:06Z $
 */

#include <typedefs.h>
#include "rte_priv.h"
#include "threadx_priv.h"
#include <rte_trap.h>
#ifdef HND_PRINTF_THREAD_SAFE
#include <bcmstdlib_ext.h>
#endif // endif
#include <hndarm.h>
#ifdef SAVERESTORE
#include <saverestore.h>
#endif // endif
#include <hndcpu.h>

/**
 * this handler is called by low level assembly code. It should withstand reentrancy. CM3 nor CR4
 * make use of the FVIC controller in the ARM subsystem. An implication is that a 'normal' interrupt
 * can be preempted by a 'fast interrupt', but not by another 'normal' interrupt.
 */

static uint8 ab_trap_entry_count = 0;
static trap_t *nested_trap_tr = NULL; /* Nested trap debug info */
static void
hnd_trap_process(trap_t *tr)
{
#ifdef HND_PRINTF_THREAD_SAFE
	in_trap_handler ++;
#endif	/* HND_PRINTF_THREAD_SAFE */

	/* clear backplane timeout, if that caused the fault */
#if defined(__ARM_ARCH_7M__)
	if (tr->type == TR_FAULT)
#else
	if (tr->type == TR_DAB || tr->type == TR_IAB ||
		tr->type == TR_SWI || tr->type == TR_UND)
#endif // endif
		si_clear_backplane_to(hnd_sih);

	/* Give following faults one chance to succeed. If we come back here again,
	 * sit tight and do nothing.
	 */
#if defined(__ARM_ARCH_7M__)
	if (tr->type >= TR_NMI && tr->type <= TR_DMON)
#else /* !__ARM_ARCH_7M__ */
	if (tr->type == TR_DAB || tr->type == TR_IAB ||
		tr->type == TR_SWI || tr->type == TR_UND)
#endif /* __ARM_ARCH_7M__ */
	{
		/* already in trap handler, so instead of trapping right away, let it go through */

		/* Nested traps? */
		if (++ab_trap_entry_count > 1)
		{
			hnd_inform_host_die();
			/* Save nested trap info for gdb debug */
			nested_trap_tr = tr;

#ifndef BCM_BOOTLOADER
#if !(defined(BCMDBG_LOADAVG) && defined(__ARM_ARCH_7R__))
			/* Make sure deadman timer is cleared. */
			hnd_cpu_deadman_timer(hnd_sih, 0);
#endif  /* !(defined(BCMDBG_LOADAVG) && defined(__ARM_ARCH_7R__)) */
#endif /* BCM_BOOTLOADER */
			/* sit tight */
			while (1);
		}
	}

	/* Common trap handling */
	hnd_trap_common(tr);

	/* Get the threadx information */
	hnd_trap_threadx_info();

	/* Halt processing, this function is not supposed to return. */
	hnd_the_end();

#ifdef HND_PRINTF_THREAD_SAFE
	in_trap_handler --;
#endif	/* HND_PRINTF_THREAD_SAFE */
} /* hnd_trap_process */

void
hnd_trap_handler(trap_t *tr)
{
#if defined(SAVERESTORE) && !defined(STACK_PROT_TRAP) && defined(__ARM_ARCH_7M__)
	if (SR_ENAB() && tr->type != TR_NMI)
		sr_process_restore(hnd_sih);
#endif // endif

	hnd_trap_process(tr);

#if defined(SAVERESTORE) && !defined(STACK_PROT_TRAP) && defined(__ARM_ARCH_7M__)
	if (SR_ENAB() && tr->type != TR_NMI)
		sr_process_save(hnd_sih);
#endif // endif
}
