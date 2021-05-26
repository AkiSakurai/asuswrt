/*
 * RTE support code for chipcommon & misc. subcores
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
 * $Id: rte_chipc.c 787983 2020-06-17 17:59:06Z $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmdevs.h>
#include <bcmutils.h>
#include <sbchipc.h>
#include <hndsoc.h>
#include <rte_dev.h>
#include <rte_chipc.h>
#include "rte_chipc_priv.h"
#include <rte_isr.h>
#include "rte_priv.h"
#include <rte_trap.h>

/* interested chipcommon interrupt source
 *  - GPIO
 *  - EXTIF
 *  - ECI
 *  - PMU
 *  - UART
 */

static hnd_isr_action_t *cc_isr = NULL;
static uint32 cc_intstatus = 0;
static uint32 cc_intmask = 0;

chipcregs_t *hnd_ccr = NULL;	/* Chipc core regs */

/**
 * Interface to register chipc secondary ISR and/or DPC function.
 *
 * @param sih		SI handle.
 * @param isr_fn	ISR callback function, or NULL.
 * @param worklet_fn	Worklet callback function, or NULL.
 * @param intmask	Interrupt mask.
 * @param cbdata	Context to be passed to ISR and worklet functions.
 * @param id		Secondary handler id.
 * @return		TRUE on success.
 */

bool
BCMATTACHFN(si_cc_register_isr_internal)(si_t *sih, cc_isr_fn isr_fn, cc_worklet_fn worklet_fn,
	uint32 intmask, void *cbdata, const char *isr_id, const char *worklet_id)
{
	chipcregs_t *regs = hnd_ccr;

	/* Register secondary ISR and/or worklet function */
	if (hnd_isr_register_sec_internal(cc_isr, HND_ISR_INDEX_DEFAULT, intmask,
		(hnd_isr_sec_fn_t)isr_fn, cbdata, (hnd_worklet_fn_t)worklet_fn, cbdata,
		NULL, isr_id, worklet_id) == FALSE) {
			return FALSE;
	}

	ASSERT(regs != NULL);

	/* Update interrupt mask register */
	cc_intmask = R_REG(si_osh(sih), &regs->intmask) | intmask;
	W_REG(si_osh(sih), &regs->intmask, cc_intmask);

	return TRUE;
}

/**
 * Chipcommon primary interrupt handler.
 *
 * @param cbdata	Context as passed to @see hnd_isr_register.
 * @return		TRUE to request worklet.
 */

static bool
hnd_chipc_isr(void *cbdata)
{
	si_t *sih = hnd_sih;
	chipcregs_t *regs = hnd_ccr;

	/* Prior to rev 21 chipc interrupt means uart and gpio */
	if (CCREV(sih->ccrev) >= 21) {
		cc_intstatus = R_REG(si_osh(sih), &regs->intstatus) & cc_intmask;
	} else {
		cc_intstatus = (CI_UART | CI_GPIO);
	}

	/* Disable interrupts */
	W_REG(si_osh(sih), &regs->intmask, 0);

	/* Run secondary ISRs */
	hnd_isr_invoke_sec_isr(cc_isr, HND_ISR_INDEX_DEFAULT, cc_intstatus);

	/* Request worklet */
	return TRUE;
}

#ifndef RTE_POLL

/**
 * Chipcommon primary DPC function.
 *
 * @param cbdata	Context as passed to @see hnd_isr_register.
 */

static bool
hnd_chipc_worklet(void *cbdata)
{
	si_t *sih = hnd_sih;
	chipcregs_t *regs = hnd_ccr;

	/* Run secondary worklet functions */
	hnd_isr_invoke_sec_worklet(cc_isr, HND_ISR_INDEX_DEFAULT, cc_intstatus);

	/* Reenable interrupts */
	W_REG(si_osh(sih), &regs->intmask, cc_intmask);

	return FALSE;
}
#endif /* !RTE_POLL */

/* ======HND====== misc
 *     chipc init
 */

#ifdef RTE_POLL
static void *
BCMATTACHFN(hnd_chipc_probe)(hnd_dev_t *dev, volatile void *regs, uint bus,
                             uint16 device, uint coreid, uint unit)
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

static void
hnd_chipc_poll(hnd_dev_t *dev)
{
	hnd_chipc_isr((void*)dev);
}

static hnd_dev_ops_t chipc_funcs = {
	probe:		hnd_chipc_probe,
	poll:		hnd_chipc_poll
};

static hnd_dev_t chipc_dev = {
	name:		"cc",
	ops:		&chipc_funcs
};
#endif	/* RTE_POLL */

void
BCMATTACHFN(hnd_chipc_init)(si_t *sih)
{
	/* Get chipcommon and its sbconfig addr */
	hnd_ccr = si_setcoreidx(sih, SI_CC_IDX);

	/* Only support chips that have chipcommon */
	ASSERT(hnd_ccr);

	/* Register polling dev or isr */
#ifdef RTE_POLL
	hnd_add_device(sih, &chipc_dev, CC_CORE_ID, BCM4710_DEVICE_ID);
#else
	cc_isr = hnd_isr_register(CC_CORE_ID, 0, SI_BUS, hnd_chipc_isr, NULL,
		hnd_chipc_worklet, NULL, NULL);
	if (cc_isr == NULL) {
		HND_DIE();
	}
#endif	/* !RTE_POLL */
}

/* ======HND====== misc
 *     gci init
 *     eci init
 */

static bool
rte_gci_worklet(void* cbdata)
{
	si_t *sih = (si_t *)cbdata;
	si_gci_handler_process(sih);

	/* Don't reschedule */
	return FALSE;
}

void
BCMATTACHFN(rte_gci_init)(si_t *sih)
{
	if (!(sih->cccaps_ext & CC_CAP_EXT_GCI_PRESENT))
		return;

	si_cc_register_isr(sih, NULL, rte_gci_worklet, CI_ECI, (void *)sih);
}

#ifdef BCMECICOEX
void
BCMATTACHFN(hnd_eci_init)(si_t *sih)
{
	if (CCREV(sih->ccrev) < 21)
		return;
	si_eci_init(sih);
}
#endif	/* BCMECICOEX */
