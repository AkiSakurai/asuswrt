/** \file hnd_isr.c
 *
 * OS independent ISR functions for ISRs or worklets.
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
 * $Id: rte_isr.c 787983 2020-06-17 17:59:06Z $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <osl_ext.h>
#include <bcmutils.h>
#include <hndsoc.h>
#include <rte.h>
#include "rte_priv.h"
#include <rte_isr.h>
#include <rte_isr_stats.h>
#include "rte_isr_stats_priv.h"
#include "rte_isr_priv.h"
#include <rte_cons.h>
#include <bcm_buzzz.h>

#define ISR_ERROR(args)			do { printf args; } while (0)
#define ISR_ERROR_IF(cond, args)	do { if (cond) ISR_ERROR(args); } while (0)
#ifdef BCMDBG
#define ISR_INFORM(args)		do { printf args; } while (0)
#define ISR_INFORM_IF(cond, args)	do { if (cond) ISR_INFORM(args); } while (0)
#else
#define ISR_INFORM(args)
#define ISR_INFORM_IF(cond, args)
#endif /* BCMDBG */

/* ISR instance */
hnd_isr_instance_t *hnd_isr = NULL;

/*
 *
 * Internal Functions
 *
 */

#ifdef BCMDBG

/**
 * Return the id associated with the specified worklet.
 *
 * @param worklet	Worklet pointer.
 * @return		Id, or NULL if not available.
 */

static const char*
get_worklet_id(hnd_worklet_t *worklet)
{
	return worklet != NULL ? hnd_isr_stats_get_id(worklet->stats) : "";
}

static void
isr_show(void)
{
	hnd_isr_instance_t *instance = hnd_isr_get_inst();
	hnd_isr_action_t *action;
	hnd_isr_sec_action_t *sec_action;
	uint index;

	ASSERT(instance != NULL);

	printf_suppress_timestamp(TRUE);

	printf("%-12s %8s %4s %8s %s\n", "TYPE", "IRQMASK", "IDX", "INTMASK", "TARGET");
	action = instance->actions;
	while (action != NULL) {
		printf("%-12s %08x\n", "PRIMARY", action->intmask);
		if (action->isr_fn != NULL) {
			printf("%-12s %22s [%p] %s\n", " ISR", "", action->isr_fn,
				hnd_isr_stats_get_id(action->stats));
		}
		if (action->worklet != NULL) {
			printf("%-12s %22s [%p] %s\n", " WORKLET", "", action->worklet->fn,
				get_worklet_id(action->worklet));
		}

		for (index = 0; index < HND_ISR_INDEX_COUNT; index++) {
			sec_action = action->sec[index];
			while (sec_action != NULL) {
				printf("%-12s %8s %4u %08x\n", " SECONDARY", "", index,
					sec_action->intmask);
				if (sec_action->isr_fn != NULL) {
					printf("%-12s %22s [%p] %s\n", "  ISR", "",
						sec_action->isr_fn,
						hnd_isr_stats_get_id(sec_action->stats));
				}
				if (sec_action->worklet != NULL) {
					printf("%-12s %22s [%p] %s\n", "  WORKLET", "",
						sec_action->worklet->fn,
						get_worklet_id(sec_action->worklet));
				}
				sec_action = sec_action->next;
			}
		}
		action = action->next;
	}

	printf_suppress_timestamp(FALSE);
}

static void
isr_cmd(void *arg, int argc, char *argv[])
{
	if (!strcmp(argv[1], "show")) {
		isr_show();
	}
}

#endif /* BCMDBG */

/*
 *
 * Export functions
 *
 */

/**
 * Return the interrupt mask for an IRQ, used for interrupt registration.
 *
 * @param irq		IRQ number.
 * @return		Interrupt mask.
 */

uint32
BCMATTACHFN(hnd_isr_intmask_for_irq)(uint irq)
{
	ASSERT(irq < 32);
	return (1U << irq);
}

/**
 * Return the interrupt mask for a core, used for interrupt registration.
 *
 * @param coreid	Core ID.
 * @param unit		Unit number.
 * @param bus		Bus type.
 * @return		Interrupt mask.
 */

uint32
BCMATTACHFN(hnd_isr_intmask_for_core)(uint coreid, uint unit, uint bus)
{
	volatile void	*regs = NULL;
	uint		origidx;
	uint32		intmask;

	origidx = si_coreidx(hnd_sih);

	switch (bus) {
		case SI_BUS:	regs = si_setcore(hnd_sih, coreid, unit); break;
#ifdef SBPCI
		case PCI_BUS:	regs = si_setcore(hnd_sih, PCI_CORE_ID, 0); break;
#endif // endif
	}

	BCM_REFERENCE(regs);
	ASSERT(regs != NULL);

	intmask = (1U << si_flag(hnd_sih));
	si_setcoreidx(hnd_sih, origidx);

	ASSERT(intmask != 0x0);
	return intmask;
}

/**
 * Initialize interrupt handler module.
 *
 * @param osh			Pointer to OS handle.
 * @param schedule_worklet_fn	Pointer to worklet scheduling function.
 */

void
BCMATTACHFN(hnd_isr_module_init)(osl_t *osh, hnd_schedule_worklet_fn_t schedule_worklet_fn)
{
	ASSERT(hnd_isr == NULL);

	hnd_isr = (hnd_isr_instance_t*)MALLOCZ(osh, sizeof(hnd_isr_instance_t));
	ASSERT(hnd_isr != NULL);

	hnd_isr->schedule_worklet_fn = schedule_worklet_fn;

#ifdef BCMDBG
	hnd_cons_add_cmd("isr", isr_cmd, hnd_osh);
#endif /* BCMDBG */
}

/**
 * Register an ISR and/or worklet for an IRQ.
 *
 * When an IRQ is received, the registered ISR (if any) is called in interrupt context. If the
 * ISR returns true or if no ISR is registered, the registered worklet (if any) in scheduled
 * for execution.
 *
 * @param intmask		Interrupt mask, see @hnd_isr_intmask_for_irq
 *				or @hnd_isr_intmask_for_core.
 * @param isr_fn		Pointer to interrupt handler function, or NULL.
 * @param cbdata_isr		Interrupt handler context data.
 * @param worklet_fn		Pointer to worklet target function, or NULL.
 * @param cbdata_worklet	Worklet context data.
 * @param thread		Pointer to worklet target thread, or NULL to use the current thread.
 * @param isr_id		ISR id.
 * @param worklet_id		Worklet id.
 * @return			Interrupt handle, or NULL on error.
 */

hnd_isr_action_t*
BCMATTACHFN(hnd_isr_register_internal)(uint32 intmask, hnd_isr_fn_t isr_fn, void *cbdata_isr,
	hnd_worklet_fn_t worklet_fn, void *cbdata_worklet, osl_ext_task_t* thread,
	const char *isr_id, const char *worklet_id)
{
	hnd_worklet_t *worklet = NULL;

	if (worklet_fn != NULL) {
		worklet = hnd_worklet_create_internal(worklet_fn, cbdata_worklet,
			SCHEDULER_DEFAULT_INT_PRIORITY, SCHEDULER_DEFAULT_INT_QUOTA,
			thread, worklet_id);
		ASSERT(worklet);
	}

	return hnd_isr_register_with_worklet_internal(
		intmask, isr_fn, cbdata_isr, worklet, isr_id);
}

/**
 * Register an ISR and/or worklet for an IRQ.
 *
 * Similar to @see hnd_isr_register_internal but registers an already created
 * worklet (@see hnd_worklet_create).
 *
 * @param intmask	Interrupt mask, see @hnd_isr_intmask_for_irq or @hnd_isr_intmask_for_core.
 * @param isr_fn	Pointer to interrupt handler function, or NULL.
 * @param cbdata_isr	Interrupt handler context data.
 * @param worklet	Pointer to worklet, or NULL.
 * @param isr_id	ISR id.
 * @return		Interrupt handle, or NULL on error.
 */

hnd_isr_action_t*
BCMATTACHFN(hnd_isr_register_with_worklet_internal)(uint32 intmask,
	hnd_isr_fn_t isr_fn, void *cbdata, hnd_worklet_t *worklet, const char *id)
{
	hnd_isr_instance_t	*instance = hnd_isr_get_inst();
	hnd_isr_action_t	*action;

	ASSERT(instance != NULL);
	ASSERT(intmask != 0x0);
	ASSERT(isr_fn != NULL || worklet != NULL);

	ISR_ERROR_IF(instance->action_intmask & intmask,
		("%s: duplicate isr for [%s] int %08x\n", __FUNCTION__, id, intmask));
	ASSERT((instance->action_intmask & intmask) == 0);

	action = (hnd_isr_action_t*)MALLOCZ(si_osh(hnd_sih), sizeof(hnd_isr_action_t));
	if (action != NULL) {

		action->intmask	= intmask;
		action->isr_fn	= isr_fn;
		action->cbdata	= cbdata;
		action->worklet	= worklet;
		action->cpuutil	= /* Bind isr execution context to CPU Util context */
			HND_CPUUTIL_CONTEXT_GET(HND_CPUUTIL_CONTEXT_ISR, (uint32)isr_fn);

#ifdef HND_INTSTATS
		if (isr_fn != NULL) {
			action->stats = hnd_isr_stats_create(OBJ_TYPE_ISR, id, (void*)isr_fn,
				worklet ? hnd_worklet_get_thread(worklet)->stats : NULL);
		}

		/* Update worklet stats identifiers */
		if (worklet != NULL && worklet->stats != NULL) {
			worklet->stats->type   = OBJ_TYPE_INT_WORKLET;
			worklet->stats->parent = action->stats;
			if (worklet->stats->id == NULL) {
				worklet->stats->id = id;
			}
		}
#endif /* HND_INTSTATS */

		action->next = instance->actions;
		instance->actions = action;
		instance->action_intmask |= action->intmask;
	}

	return action;
}

/**
 * Register a secondary ISR and/or worklet.
 *
 * A secondary ISR/worklet can be registered to be executed (ISR/worklet) or scheduled (worklet)
 * when the specified interrupt mask matches the contents of an interrupt status register of the
 * core for which the specified primary ISR/worklet is registered.
 *
 * Secondary ISR/worklets can be processed in two ways:
 *
 * - Calling @see hnd_isr_invoke_sec_isr in the primary ISR (in interrupt context) will cause all
 *   matching secondary ISR to be invoked from the primary ISR (in interrupt context). Calling
 *   @see hnd_isr_invoke_sec_worklet in the primary worklet will cause all matching secondary
 *   worklets to be invoked (as opposed to scheduled) directly from the primary worklet.
 *
 * - Calling @see hnd_isr_process_sec_interrupts from the primary ISR (in interrupt context) will
 *   cause all matching secondary ISR to be invoked from the primary ISR (in interrupt context).
 *   For each secondary ISR that returns TRUE, the corresponding secondary worklet will be
 *   scheduled (as opposed to invoked).
 *
 * @param primary		Associated primary ISR/worklet handler as returned by
 *				@see hnd_isr_register.
 * @param index			Interrupt status register index. This allows secondary ISR/worklet
 *				to target additional interrupt status registers associated to a
 *				core. Each interrupt status register for which secondary
 *				ISR/worklets are registered should be assigned an index, and the
 *				same index should be specified during the calls to
 *				@see hnd_isr_invoke_sec_isr, @see hnd_isr_invoke_sec_worklet and
 *				@see hnd_isr_process_sec_interrupts.
 * @param intmask		Interrupt mask.
 * @param isr_fn		Pointer to interrupt handler function, or NULL.
 * @param cbdata_isr		Interrupt handler context data.
 * @param worklet_fn		Pointer to worklet target function, or NULL.
 * @param cbdata_worklet	Worklet context data.
 * @param thread		Pointer to worklet target thread. Specify NULL to use the
 *				current thread.
 * @param isr_id		ISR id.
 * @param worklet_id		Worklet id.
 * @return			Interrupt handle, or NULL on error.
 */

hnd_isr_sec_action_t*
BCMATTACHFN(hnd_isr_register_sec_internal)(hnd_isr_action_t *primary, uint index, uint32 intmask,
	hnd_isr_sec_fn_t isr_fn, void *cbdata_isr,
	hnd_worklet_fn_t worklet_fn, void *cbdata_worklet, osl_ext_task_t *thread,
	const char *isr_id, const char *worklet_id)
{
	osl_t			*osh = si_osh(hnd_sih);
	hnd_isr_sec_action_t	*action;

	ASSERT(primary != NULL);
	ASSERT(index < HND_ISR_INDEX_COUNT);
	ASSERT(intmask != 0x0);
	ASSERT(isr_fn != NULL || worklet_fn != NULL);

	ISR_ERROR_IF(primary->sec_intmask[index] & intmask,
		("%s: duplicate sec isr for [%s] int %08x index %u\n", __FUNCTION__,
		hnd_isr_stats_get_id(primary->stats), intmask, index));
	ASSERT((primary->sec_intmask[index] & intmask) == 0x0);

	action = (hnd_isr_sec_action_t*)MALLOCZ(osh, sizeof(hnd_isr_sec_action_t));
	if (action != NULL) {

		action->primary	= primary;
		action->intmask	= intmask;
		action->isr_fn	= isr_fn;
		action->cbdata	= cbdata_isr;

		if (worklet_fn != NULL) {
			action->worklet = hnd_worklet_create_internal(worklet_fn, cbdata_worklet,
				SCHEDULER_DEFAULT_INT_PRIORITY, SCHEDULER_DEFAULT_INT_QUOTA,
				thread, worklet_id);
			ASSERT(action->worklet != NULL);
		}

#ifdef HND_INTSTATS
		if (isr_fn != NULL) {
			action->stats = hnd_isr_stats_create(OBJ_TYPE_SEC_ISR, isr_id,
				(void*)isr_fn, primary->stats);
		}

		/* Update worklet stats identifiers */
		if (action->worklet != NULL && action->worklet->stats != NULL) {
			action->worklet->stats->type   = OBJ_TYPE_SEC_WORKLET;
			action->worklet->stats->parent =
				(action->stats ? action->stats : primary->stats);
		}
#endif /* HND_INTSTATS */

		action->next = primary->sec[index];
		primary->sec[index] = action;
		primary->sec_intmask[index] |= intmask;
	}

	return action;
}

/* Run (invoke) any secondary ISRs that match the specified interrupt status register index and
 * interrupt status.
 *
 * To be called from the primary ISR (in interrupt context).
 *
 * @note This function is obsoleted by @see hnd_isr_process_sec_interrupts and kept for
 * backwards compatibility.
 *
 * @param action	Interrupt handle as returned by @see hnd_isr_register.
 * @param index		Interrupt status register index.
 * @param intstatus	Interrupt status.
 */

void
hnd_isr_invoke_sec_isr(hnd_isr_action_t *action, uint index, uint32 intstatus)
{
	ASSERT(hnd_in_isr());
	ASSERT(action != NULL);
	ASSERT(index < HND_ISR_INDEX_COUNT);

	if (intstatus & action->sec_intmask[index]) {
		hnd_isr_sec_action_t *sec = action->sec[index];
		do {
			/* Invoke second level ISR */
			if (sec->isr_fn != NULL) {
				if (intstatus & sec->intmask) {
					HND_INTSTATS_NOTIFY(NOTIFICATION_TYPE_ISR_ENTER,
						(void*)sec->stats);

					(void)(sec->isr_fn)(sec->cbdata, index,
						intstatus & sec->intmask);

					HND_INTSTATS_NOTIFY(NOTIFICATION_TYPE_ISR_EXIT,
						(void*)sec->stats);
				}
			}
			sec = sec->next;
		} while (sec != NULL);
	}
}

/**
 * Run (invoke) any secondary worklets that match the specified interrupt status register index and
 * interrupt status.
 *
 * To be called from the primary worklet (in non-interrupt context).
 *
 * @note This function is obsoleted by @see hnd_isr_process_sec_interrupts and kept for
 * backwards compatibility.
 *
 * @param action	Interrupt handle as returned by @see hnd_isr_register.
 * @param index		Interrupt status register index.
 * @param intstatus	Interrupt status.
 */

void
hnd_isr_invoke_sec_worklet(hnd_isr_action_t *action, uint index, uint32 intstatus)
{
	ASSERT(!hnd_in_isr());
	ASSERT(action != NULL);
	ASSERT(index < HND_ISR_INDEX_COUNT);

	if (intstatus & action->sec_intmask[index]) {
		hnd_isr_sec_action_t *sec = action->sec[index];
		do {
			/* Invoke second level worklet */
			if (sec->worklet != NULL) {
				if (intstatus & sec->intmask) {
					HND_INTSTATS_NOTIFY(NOTIFICATION_TYPE_WORKLET_ENTER,
						(void*)sec->worklet->stats);

					(void)(sec->worklet->fn)(sec->worklet->cbdata);

					HND_INTSTATS_NOTIFY(NOTIFICATION_TYPE_WORKLET_EXIT,
						(void*)sec->worklet->stats);
				}
			}
			sec = sec->next;
		} while (sec != NULL);
	}
}

/**
 * Invoke any primary ISRs and schedule any primary worklets that match the specified
 * interrupt status.
 *
 * Will be called when a hardware interrupt occurs. All matching primary ISR will be invoked
 * directly in interrupt context. For each primary ISR that returns TRUE or for interrupts for
 * which no ISR is registered, the corresponding primary worklet will be scheduled.
 *
 * @param intstatus	Interrupt status.
 */

void
hnd_isr_process_interrupts(uint32 intstatus)
{
	hnd_isr_instance_t *instance = hnd_isr_get_inst();
	hnd_isr_action_t   *action;
	bool request_worklet;

	ASSERT(instance != NULL);
	ASSERT(hnd_in_isr());
	ASSERT(intstatus != 0x0);

	ASSERT(instance->schedule_worklet_fn != NULL);

	/* Run matching ISR and optionally schedule matching worklets */
	action = instance->actions;
	while (action != NULL && intstatus != 0x0) {
		if (intstatus & action->intmask) {

			/* Invoke first level ISR */
			if (action->isr_fn != NULL) {
				HND_CPUUTIL_NOTIFY(HND_CPUUTIL_TRANS_ISR_ENT, action->cpuutil);
				HND_INTSTATS_NOTIFY(NOTIFICATION_TYPE_ISR_ENTER,
					(void*)action->stats);
				BUZZZ_LVL3(THREADX_ISR_ENT, 1, (uint32)(action->isr_fn));

				request_worklet = (action->isr_fn)(action->cbdata);

				BUZZZ_LVL3(THREADX_ISR_RTN, 0);
				HND_INTSTATS_NOTIFY(NOTIFICATION_TYPE_ISR_EXIT,
					(void*)action->stats);
				HND_CPUUTIL_NOTIFY(HND_CPUUTIL_TRANS_ISR_EXT, action->cpuutil);
			} else {
				request_worklet = TRUE;
			}

			/* Schedule first level worklet if requested */
			if (request_worklet == TRUE) {
				if (action->worklet != NULL) {
					(instance->schedule_worklet_fn)(action->worklet);
				} else {
					/* Worklet requested but none registered */
					ISR_ERROR(("%s: no worklet for [%s] irq %08x\n",
						__FUNCTION__, hnd_isr_stats_get_id(action->stats),
						action->intmask));
				}
			}

			intstatus &= ~action->intmask;
		}
		action = action->next;
	}

	ISR_ERROR_IF(intstatus != 0x0, ("%s: unhandled irq for [%s] irq %08x\n", __FUNCTION__,
		hnd_isr_stats_get_id(action->stats), intstatus));
}

/**
 * Invoke any secondary ISRs and schedule any secondary worklets that match the specified
 * interrupt status register index and interrupt status.
 *
 * To be called from the primary ISR (in interrupt context). All matching secondary ISR will
 * be invoked directly from the primary ISR (in interrupt context). For each secondary ISR that
 * returns TRUE or for interrupts for which no secondary ISR is registered, the corresponding
 * secondary worklet will be scheduled (as opposed to invoked).
 *
 * @param action	Interrupt handle as returned by @see hnd_isr_register.
 * @param index		Interrupt status register index, as specified to the call to @see
 *			hnd_isr_register_sec.
 * @param intstatus	Interrupt status.
 */

void
hnd_isr_process_sec_interrupts(hnd_isr_action_t *action, uint index, uint32 intstatus)
{
	hnd_isr_instance_t *instance = hnd_isr_get_inst();
	hnd_isr_sec_action_t *sec;
	bool request_worklet;

	ASSERT(instance != NULL);
	ASSERT(hnd_in_isr());
	ASSERT(action != NULL);
	ASSERT(index < HND_ISR_INDEX_COUNT);

	ASSERT(instance->schedule_worklet_fn != NULL);

	sec = action->sec[index];
	while (sec != NULL && intstatus != 0x0) {
		if (intstatus & sec->intmask) {

			/* Invoke second level ISR */
			if (sec->isr_fn != NULL) {
				HND_INTSTATS_NOTIFY(NOTIFICATION_TYPE_ISR_ENTER, (void*)sec->stats);

				request_worklet = (sec->isr_fn)(sec->cbdata, index,
					intstatus & sec->intmask);

				HND_INTSTATS_NOTIFY(NOTIFICATION_TYPE_ISR_EXIT, (void*)sec->stats);
			} else {
				request_worklet = TRUE;
			}

			/* Schedule second level worklet if requested */
			if (request_worklet == TRUE) {
				if (sec->worklet != NULL) {
					(instance->schedule_worklet_fn)(sec->worklet);
				} else {
					/* Worklet requested but none registered */
					ISR_ERROR(("%s: no worklet for [%s] int %08x index %u\n",
						__FUNCTION__, hnd_isr_stats_get_id(action->stats),
						sec->intmask, index));
				}
			}

			intstatus &= ~sec->intmask;
		}
		sec = sec->next;
	}

	ISR_ERROR_IF(intstatus != 0x0, ("%s: unhandled sec int for [%s] int %08x index %u\n",
		__FUNCTION__, hnd_isr_stats_get_id(action->stats), intstatus, index));
}
