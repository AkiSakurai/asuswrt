/** \file hnd_isr.c
 *
 * OS independent ISR functions for ISRs or DPCs.
 *
 * Broadcom Proprietary and Confidential. Copyright (C) 2017,
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom.
 *
 * $Id: rte_isr.c 580528 2015-08-19 11:48:07Z $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <osl_ext.h>
#include <bcmutils.h>
#include <hndsoc.h>
#include "rte_priv.h"
#include <rte_isr.h>
#include "rte_isr_priv.h"
#include <bcm_buzzz.h>

/* ISR and DPC instances */
hnd_isr_instance_t *hnd_isr = NULL;
hnd_isr_instance_t *hnd_dpc = NULL;

static int hnd_add_action(hnd_isr_instance_t *instance, uint irq, uint coreid, uint unit,
	hnd_isr_t isr, void *cbdata, uint bus);

/* create instance */
static hnd_isr_instance_t *
BCMATTACHFN(hnd_isr_instance)(osl_t *osh)
{
	hnd_isr_instance_t *instance;

	if ((instance = MALLOCZ(osh, sizeof(hnd_isr_instance_t))) == NULL) {
		return NULL;
	}
	return instance;
}

/* initialize and instantiate instances */
void
BCMATTACHFN(hnd_isr_module_init)(osl_t *osh)
{
	/* create ISR and DPC instances */
	hnd_isr = hnd_isr_instance(osh);
	ASSERT(hnd_isr);
	hnd_dpc = hnd_isr_instance(osh);
	ASSERT(hnd_dpc);
}

/* register isr */
int
BCMATTACHFN(hnd_isr_register)(uint irq, uint coreid, uint unit,
	hnd_isr_t isr, void *cbdata, uint bus)
{
	return hnd_add_action(hnd_isr_get_inst(), irq, coreid, unit, isr, cbdata, bus);
}

/* register dpc */
int
BCMATTACHFN(hnd_dpc_register)(uint irq, uint coreid, uint unit,
	hnd_dpc_t dpc, void *cbdata, uint bus)
{
	return hnd_add_action(hnd_dpc_get_inst(), irq, coreid, unit,
	                      (hnd_isr_t)dpc, cbdata, bus);
}

static int
BCMATTACHFN(hnd_add_action)(hnd_isr_instance_t *instance, uint irq, uint coreid, uint unit,
	hnd_isr_t isr, void *cbdata, uint bus)
{
	si_t *sih = get_hnd_sih();
	osl_t *osh = si_osh(sih);
	void	*regs = NULL;
	uint	origidx;
	hnd_isr_action_t *action;

	if (instance == NULL) {
		return BCME_ERROR;
	}

	if ((action =  MALLOCZ(osh, sizeof(hnd_isr_action_t))) == NULL) {
		return BCME_NOMEM;
	}

	origidx = si_coreidx(sih);
	if (bus == SI_BUS)
		regs = si_setcore(sih, coreid, unit);
#ifdef SBPCI
	else if (bus == PCI_BUS)
		regs = si_setcore(sih, PCI_CORE_ID, 0);
#endif
	BCM_REFERENCE(regs);
	ASSERT(regs);

	action->sbtpsflag = 1 << si_flag(sih);
#ifdef REROUTE_OOBINT
	if (coreid == PMU_CORE_ID) {
		action->sbtpsflag = 1 << PMU_OOB_BIT;
	}
#endif
#ifdef BCM_OL_DEV
	if (coreid == D11_CORE_ID)
		action->sbtpsflag = 1 << si_flag_alt(sih);
#endif
	action->coreid = coreid;
	/* expect hnd_isr_register() to be called in the thread context */
	action->thread = osl_ext_task_current();
	action->event = hnd_isr_get_dpc_event(action->thread, coreid);
	action->isr = isr;
	action->cbdata = cbdata;

	action->next = instance->hnd_isr_action_list;
	instance->hnd_isr_action_list = action;
	instance->hnd_action_flags |= action->sbtpsflag;

	/* restore core original idx */
	si_setcoreidx(sih, origidx);

	return BCME_OK;
}

/* register isr that doesn't belong to any core */
int
BCMATTACHFN(hnd_isr_register_n)(uint irq, uint isr_num, hnd_isr_t isr, void *cbdata)
{
	si_t *sih = get_hnd_sih();
	osl_t *osh = si_osh(sih);
	hnd_isr_instance_t *instance = hnd_isr_get_inst();
	hnd_isr_action_t *action;

	if ((action =  MALLOCZ(osh, sizeof(hnd_isr_action_t))) == NULL) {
		return BCME_NOMEM;
	}

	action->sbtpsflag = 1 << isr_num;
	action->isr = isr;
	action->cbdata = cbdata;
	action->next = instance->hnd_isr_action_list;

	instance->hnd_isr_action_list = action;
	instance->hnd_action_flags |= action->sbtpsflag;

	return BCME_OK;
}

/* run isr based on sbflagst, optional callbacks invoked pre and post isr run */
void
hnd_isr_proc_sbflagst(hnd_isr_instance_t *instance, uint32 sbflagst,
	void (*pre_cb)(hnd_isr_action_t *action),
	void (*post_cb)(hnd_isr_action_t *action))
{
	hnd_isr_action_t *action;

	if (instance == NULL)
		return;

	sbflagst &= instance->hnd_action_flags;

	/* find and run matching ISR */
	if (sbflagst) {
		action = instance->hnd_isr_action_list;
		while (action) {
			if (sbflagst & action->sbtpsflag) {
				sbflagst &= ~action->sbtpsflag;
				/* invoke pre callback */
				if (pre_cb != NULL) {
					(pre_cb)(action);
				}
				/* invoke isr */
				if (action->isr) {
					BUZZZ_LVL1(THREADX_ISR_ENT, 1, (uint32)(action->isr));
					(action->isr)(action->cbdata);
					BUZZZ_LVL1(THREADX_ISR_RTN, 0);
				}
				/* invoke post callback */
				if (post_cb != NULL) {
					(post_cb)(action);
				}
			}
			action = action->next;
		}
	}
}

/* process the event */
bool
hnd_dpc_proc_event(hnd_isr_instance_t *instance, uint32 event)
{
	hnd_isr_action_t *action;

	bool isr_ex = FALSE;

	if (instance == NULL)
		return FALSE;

	action = instance->hnd_isr_action_list;

	while (action) {
		if (action->event == event) {
			if (action->isr != NULL) {
				BUZZZ_LVL1(THREADX_EVT_ISR_ENT, 2, (uint32)(action->isr), event);
				(action->isr)(action->cbdata);
				BUZZZ_LVL1(THREADX_EVT_ISR_RTN, 0);
				isr_ex = TRUE;
			}
		}
		action = action->next;
	}
	return isr_ex;
}
