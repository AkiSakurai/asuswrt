/** \file hnd_isr.c
 *
 * OS independent ISR functions for ISRs or DPCs.
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
 * $Id: rte_isr.c 774076 2019-04-09 12:13:18Z $
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

#define INTSTATS_INTERVAL_MIN_MS	1000	/* Minimum statistics measurement time */
#define INTSTATS_INTERVAL_MAX_MS	20000	/* Maximum statistics measurement time */
#define INTSTATS_AVG_COUNT		32	/* Averaging count, must be power of two */
#define INTSTATS_CYCLES_DIV		8	/* Cycle count divider, must be power of two */
#define INTSTATS_CYCLES_SHIFT		3	/* Cycle count divider shift */

#define INTSTATS_OVERFLOW		0xFFFFFFFF

/* ISR instance */
hnd_isr_instance_t *hnd_isr = NULL;

/**
 * Create a statistics object.
 *
 * @param type			Type of associated object.
 * @param id			Pointer to object identifier string, or NULL.
 * @param target		Associated object.
 * @param parent		Parent statistics object.
 * @returns			New statistics object.
 */

hnd_stats_t*
hnd_isr_stats_create(object_type_t type, const char *id, void *target, hnd_stats_t *parent)
{
#ifdef HND_INTSTATS
	hnd_stats_t *stats;

	ASSERT(hnd_isr != NULL);

	stats = (hnd_stats_t*)MALLOCZ(hnd_osh, sizeof(hnd_stats_t));
	if (stats != NULL) {
		stats->type   = type;
		stats->id     = id;
		stats->target = target;
		stats->parent = parent;

		stats->next = hnd_isr->stats_list;
		hnd_isr->stats_list = stats;
	}

	return stats;
#else
	return NULL;
#endif /* HND_INTSTATS */
}

/**
 * Free a statistics object.
 *
 * @param stats			Statistics object to deregister.
 */

void
hnd_isr_stats_free(hnd_stats_t *stats)
{
#ifdef HND_INTSTATS
	hnd_stats_t *cur;

	ASSERT(stats != NULL);
	ASSERT(hnd_isr != NULL);
	ASSERT(hnd_in_isr() == FALSE);

	cur = hnd_isr->stats_list;
	if (stats != cur) {
		while (cur != NULL) {
			if (cur->next == stats) {
				cur->next = stats->next;
				MFREE(hnd_osh, stats, sizeof(hnd_stats_t));
				break;
			}
			cur = cur->next;
		}
	} else {
		hnd_isr->stats_list = stats->next;
		MFREE(hnd_osh, stats, sizeof(hnd_stats_t));
	}
#endif /* HND_INTSTATS */
}

#ifdef HND_INTSTATS

/**
 * Fixup statistics objects parent references.
 *
 * @param parent		Parent object.
 */

void hnd_isr_stats_fixup(hnd_stats_t *parent)
{
	hnd_stats_t *stats;
	for (stats = hnd_isr->stats_list; stats; stats = stats->next) {
		if (stats->parent == (hnd_stats_t*)0xffffffff) {
			stats->parent = parent;
		}
	}
}

/**
 * Create a User-defined statistics object.
 *
 * @param id			Pointer to object identifier string, or NULL.
 * @return			Created statistics object, or NULL on error.
 */

hnd_stats_t*
hnd_isr_stats_create_user(const char *id)
{
	return hnd_isr_stats_create(OBJ_TYPE_USR, id, NULL, NULL);
}

#endif /* HND_INTSTATS */

/**
 * Create a DPC.
 *
 * @param dpc_fn		Pointer to DPC target function.
 * @param cbdata		Pointer to DPC target data.
 * @param thread		Pointer to DPC target thread. Can be NULL to use the
 *				current thread as the target.
 * @param id			DPC identifier, or NULL.
 * @return			Pointer to registered DPC, or NULL on error.
 */

hnd_dpc_t*
hnd_dpc_create_internal(hnd_dpc_fn_t dpc_fn, void *cbdata, osl_ext_task_t *thread, const char *id)
{
	hnd_dpc_t *dpc;

	ASSERT(dpc_fn != NULL);

	/* Use the current thread as the target thread if none is specified */
	if (thread == NULL) {
		thread = osl_ext_task_current();
		ASSERT(thread != NULL);
	}

	/* Create a new DPC */
	dpc = (hnd_dpc_t*)MALLOCZ(hnd_osh, sizeof(hnd_dpc_t));
	if (dpc != NULL) {
		dpc->dpc_fn = dpc_fn;
		dpc->cbdata = cbdata;
		dpc->thread = thread;
		dpc->cpuutil = /* Bind DPC execution context to CPU Util context */
			HND_CPUUTIL_CONTEXT_GET(HND_CPUUTIL_CONTEXT_DPC, (uint32)dpc_fn);

		dpc->stats = hnd_isr_stats_create(OBJ_TYPE_DPC, id, (void*)dpc_fn, thread->stats);
	}

	ASSERT(dpc != NULL);
	return dpc;
}

/**
 * Free a DPC, internal function that does not check deleted state of the DPC.
 *
 * @param dpc			Pointer to DPC.
 */

void
hnd_dpc_free_internal(hnd_dpc_t *dpc)
{
	ASSERT(dpc != NULL);
	ASSERT(hnd_in_isr() == FALSE);

	dpc->cpuutil = /* Unbind DPC execution context from CPU Util context */
		HND_CPUUTIL_CONTEXT_PUT(HND_CPUUTIL_CONTEXT_DPC, dpc->cpuutil);

	hnd_isr_stats_free(dpc->stats);

#ifdef BCMDBG
	/* Detect use after free */
	memset(dpc, 0, sizeof(hnd_dpc_t));
#endif /* BCMDBG */

	MFREE(hnd_osh, dpc, sizeof(hnd_dpc_t));
}

/**
 * Free a DPC.
 *
 * @param dpc			Pointer to DPC.
 */

void
hnd_dpc_free(hnd_dpc_t *dpc)
{
	OSL_INTERRUPT_SAVE_AREA

	ASSERT(dpc != NULL);

	OSL_DISABLE

	if (dpc != NULL && dpc->deleted == FALSE) {
		dpc->deleted = TRUE;

		if (dpc->state != DPC_SCHEDULED && dpc->executing == FALSE) {
			/* DPC is not in use, free it now. */
			hnd_dpc_free_internal(dpc);
		} else {
			/* DPC is scheduled or being executed by the target thread. Mark it
			 * as deleted and reschedule it to have the target thread do the cleanup.
			 */
			if (hnd_dpc_schedule_internal(dpc) == FALSE) {
				/* Scheduling failed, which should not happen when a properly
				 * dimensioned event queue is used.
				 */
				ASSERT(FALSE);
			}
		}
	}

	OSL_RESTORE
}

/**
 * Check if the specified DPC is pending or executing.
 *
 * @param dpc		Pointer to DPC.
 * @return              TRUE if pending or executing
 */

inline bool
hnd_dpc_is_pending_or_executing(hnd_dpc_t *dpc)
{
	ASSERT(dpc->deleted == FALSE);

	return (dpc->state == DPC_SCHEDULED || dpc->executing);
}

/**
 * Check if the specified DPC is pending.
 *
 * @param dpc_fn        Pointer to DPC.
 * @return              TRUE if pending.
 */

inline bool
hnd_dpc_is_pending(hnd_dpc_t *dpc)
{
	ASSERT(dpc->deleted == FALSE);

	return (dpc->state == DPC_SCHEDULED);
}

/**
 * Check if the specified DPC callback function is currently being executed.
 *
 * @param dpc	        Pointer to DPC.
 * @return              TRUE if being executed.
 */

inline bool
hnd_dpc_is_executing(hnd_dpc_t *dpc)
{
	ASSERT(dpc->deleted == FALSE);

	return dpc->executing;
}

/**
 * Initialize interrupt handler module.
 *
 * @param osh			Pointer to OS handle.
 * @param schedule_dpc_fn	Pointer to DPC scheduling function.
 */

void
BCMATTACHFN(hnd_isr_module_init)(osl_t *osh, hnd_schedule_dpc_fn_t schedule_dpc_fn)
{
	hnd_isr = (hnd_isr_instance_t*)MALLOCZ(osh, sizeof(hnd_isr_instance_t));
	ASSERT(hnd_isr != NULL);

	hnd_isr->schedule_dpc_fn = schedule_dpc_fn;
}

/**
 * Return the DPC registered for an interrupt.
 *
 * Caller can query DPC status or reschedule explicitly. Don't call @see hnd_dpc_free
 * on the returned DPC.
 *
 * @param isr		Pointer to interrupt handle, @see hnd_isr_register.
 * @return		Pointer to registered DPC, or NULL if none registered.
 */

hnd_dpc_t*
hnd_dpc_for_isr(hnd_isr_action_t *isr)
{
	ASSERT(isr != NULL);
	return isr->dpc;
}

/**
 * Register an ISR and/or DPC function for an IRQ.
 *
 * When an IRQ is received the registered ISR (if any) is called in interrupt context and the
 * registered DPC (if any) is scheduled for execution on the specified target thread.
 *
 * @param irq		Currently unused.
 * @param coreid	Core number.
 * @param unit		Core unit.
 * @param isr_fn	Pointer to interrupt handler function, or NULL.
 * @param cbdata_isr	Interrupt handler context data.
 * @param dpc_fn	Pointer to DPC target function, or NULL.
 * @param cbdata_dpc	Pointer to DPC data.
 * @param thread	Pointer to DPC target thread. Can be NULL to use the
 *			current thread as the target.
 * @param bus		Bus type.
 * @param id		IRQ handler id.
 * @return		Interrupt handle, or NULL on error.
 */

hnd_isr_action_t*
BCMATTACHFN(hnd_isr_register_internal)(uint irq, uint coreid, uint unit, hnd_isr_fn_t isr_fn,
	void *cbdata_isr, hnd_dpc_fn_t dpc_fn, void *cbdata_dpc, osl_ext_task_t* thread, uint bus,
	const char *isr_id, const char *dpc_id)
{
	return hnd_isr_register_with_dpc_internal(irq, coreid, unit, isr_fn, cbdata_isr,
		hnd_dpc_create_internal(dpc_fn, cbdata_dpc, thread, dpc_id), bus, isr_id);
}

/**
 * Register an ISR and/or DPC for an IRQ.
 *
 * Similar to @see hnd_isr_register but use an already created DPC.
 *
 * @param irq		Currently unused.
 * @param coreid	Core number.
 * @param unit		Core unit.
 * @param isr_fn	Pointer to interrupt handler function, or NULL.
 * @param cbdata_isr	Interrupt handler context data.
 * @param dpc		Pointer to DPC or NULL. @see hnd_dpc_create.
 * @param bus		Bus type.
 * @param id		IRQ handler identifier, or NULL.
 * @return		Interrupt handle, or NULL on error.
 */

hnd_isr_action_t*
BCMATTACHFN(hnd_isr_register_with_dpc_internal)(uint irq, uint coreid, uint unit,
	hnd_isr_fn_t isr_fn, void *cbdata, hnd_dpc_t *dpc, uint bus, const char *id)
{
	si_t		    *sih = hnd_sih;
	osl_t		    *osh = si_osh(sih);
	volatile void	    *regs = NULL;
	uint		    origidx;
	hnd_isr_action_t    *action;
	hnd_isr_instance_t  *instance = hnd_isr_get_inst();

	ASSERT(instance != NULL);
	ASSERT(isr_fn != NULL || dpc != NULL);

	action = (hnd_isr_action_t*)MALLOCZ(osh, sizeof(hnd_isr_action_t));
	if (action == NULL) {
		return NULL;
	}

	origidx = si_coreidx(sih);
	if (bus == SI_BUS)
		regs = si_setcore(sih, coreid, unit);
#ifdef SBPCI
	else if (bus == PCI_BUS)
		regs = si_setcore(sih, PCI_CORE_ID, 0);
#endif // endif
	BCM_REFERENCE(regs);
	ASSERT(regs);

	action->sbtpsflag = (1 << si_flag(sih));
	action->coreid    = coreid;
	action->isr_fn	  = isr_fn;
	action->cbdata    = cbdata;
	action->dpc	  = dpc;
	action->cpuutil   = /* Bind isr execution context to CPU Util context */
		HND_CPUUTIL_CONTEXT_GET(HND_CPUUTIL_CONTEXT_ISR, (uint32)isr_fn);

#ifdef HND_INTSTATS
	action->stats = hnd_isr_stats_create(OBJ_TYPE_ISR, id, (void*)isr_fn,
		dpc ? dpc->thread->stats : NULL);

	/* Update DPC stats identifiers */
	if (dpc != NULL && dpc->stats != NULL) {
		dpc->stats->type   = OBJ_TYPE_IDPC;
		dpc->stats->parent = action->stats;
		if (dpc->stats->id == NULL) {
			dpc->stats->id = id;
		}
	}
#endif /* HND_INTSTATS */

	action->next = instance->hnd_isr_action_list;
	instance->hnd_isr_action_list = action;
	instance->hnd_action_flags |= action->sbtpsflag;

	si_setcoreidx(sih, origidx);

	return action;
}

/**
 * Register an ISR and/or DPC function that doesn't belong to any core.
 *
 * Similar to @see hnd_isr_register.
 *
 * @param irq		Currently unused.
 * @param isr_num	IRQ number.
 * @param isr_fn	Pointer to interrupt handler function, or NULL.
 * @param cbdata_isr	Interrupt handler context data.
 * @param dpc_fn	Pointer to DPC target function, or NULL.
 * @param cbdata_dpc	Pointer to DPC data.
 * @param thread	Pointer to DPC target thread. Can be NULL to use the
 *			current thread as the target.
 * @param id		IRQ handler identifier, or NULL.
 * @return		Interrupt handle, or NULL on error.
 */

hnd_isr_action_t*
BCMATTACHFN(hnd_isr_register_n_internal)(uint irq, uint isr_num, hnd_isr_fn_t isr_fn,
	void *cbdata_isr, hnd_dpc_fn_t dpc_fn, void *cbdata_dpc, osl_ext_task_t* thread,
	const char *isr_id, const char *dpc_id)
{
	si_t		    *sih = hnd_sih;
	osl_t		    *osh = si_osh(sih);
	hnd_isr_instance_t  *instance = hnd_isr_get_inst();
	hnd_isr_action_t    *action;

	ASSERT(instance != NULL);
	ASSERT(isr_fn != NULL || dpc_fn != NULL);

	/* Use the current thread as the target thread if none is specified */
	if (thread == NULL) {
		thread = osl_ext_task_current();
		ASSERT(thread != NULL);
	}

	action = (hnd_isr_action_t*)MALLOCZ(osh, sizeof(hnd_isr_action_t));
	if (action == NULL) {
		return NULL;
	}

	action->sbtpsflag = (1 << isr_num);
	action->isr_fn	  = isr_fn;
	action->cbdata    = cbdata_isr;
	action->cpuutil   = /* Bind isr execution context to CPU Util context */
		HND_CPUUTIL_CONTEXT_GET(HND_CPUUTIL_CONTEXT_ISR, (uint32)isr_fn);

	if (dpc_fn != NULL) {
		/* Create and register a DPC for handling the specified callback */
		action->dpc = hnd_dpc_create_internal(dpc_fn, cbdata_dpc, thread,
			dpc_id ? dpc_id : isr_id);
	}

#ifdef HND_INTSTATS
	action->stats = hnd_isr_stats_create(OBJ_TYPE_ISR, isr_id, (void*)isr_fn, thread->stats);

	/* Update DPC stats identifiers */
	if (action->dpc != NULL && action->dpc->stats != NULL) {
		action->dpc->stats->type   = OBJ_TYPE_IDPC;
		action->dpc->stats->parent = action->stats;
	}
#endif /* HND_INTSTATS */

	action->next = instance->hnd_isr_action_list;
	instance->hnd_isr_action_list = action;
	instance->hnd_action_flags |= action->sbtpsflag;

	return action;
}

/**
 * Register a secondary ISR and/or DPC function for a primary IRQ handler.
 *
 * When an IRQ is received the registered primary ISR (if any) is called in interrupt context and
 * the primary registered DPC (if any) is scheduled for execution on the specified target thread.
 *
 * The primary ISR can call @see hnd_sec_isr_run to call any secondary ISR handlers that match
 * the current interrupt status.
 *
 * The secondary DPC can call @see hnd_sec_dpc_run to call any secondary DPC hanglers that match
 * the current interrupt status. The secondary DPC is called on the same thread as the
 * primary DPC.
 *
 * @param isr		Interrupt handle as returned by @see hnd_isr_register.
 * @param intmask	Interrupt mask.
 * @param isr_fn	Pointer to secondary interrupt handler function, or NULL.
 * @param dpc_fn	Pointer to secondary DPC target function, or NULL.
 * @param cbdata	Pointer to DPC and ISR data.
 * @param id		Secondary handler identifier, or NULL.
 * @return		TRUE on success.
 */

bool
BCMATTACHFN(hnd_sec_isr_register_internal)(hnd_isr_action_t *isr, uint32 intmask,
	hnd_sec_isr_fn_t isr_fn, hnd_sec_dpc_fn_t dpc_fn, void *cbdata,
	const char *isr_id, const char *dpc_id)
{
	si_t			*sih = hnd_sih;
	osl_t			*osh = si_osh(sih);
	hnd_sec_isr_action_t	*action;

	ASSERT(isr != NULL);
	ASSERT(intmask != 0);
	ASSERT(isr_fn != NULL || dpc_fn != NULL);
	ASSERT((isr->sec_handler_intmask & intmask) == 0);

	action = (hnd_sec_isr_action_t*)MALLOCZ(osh, sizeof(hnd_sec_isr_action_t));
	if (action == NULL) {
		return FALSE;
	}

	action->intmask = intmask;
	action->isr_fn	= isr_fn;
	action->dpc_fn	= dpc_fn;
	action->cbdata	= cbdata;

	if (isr_fn != NULL) {
		action->stats[0] = hnd_isr_stats_create(OBJ_TYPE_SEC_ISR, isr_id,
			(void*)isr_fn, isr->stats);
	}
	if (dpc_fn != NULL) {
		action->stats[1] = hnd_isr_stats_create(OBJ_TYPE_SEC_DPC, dpc_id,
			(void*)dpc_fn, isr->dpc ? isr->dpc->stats : isr->stats);
	}

	action->next = isr->sec_handler;
	isr->sec_handler = action;
	isr->sec_handler_intmask |= intmask;

	return TRUE;
}

/**
 * Run any secondary ISRs that match the current interrupt status.
 *
 * To be called from the primary ISR for which the secondary ISRs were registered. Any secondary
 * ISR whose interrupt mask match the specified interrupt status will be invoked.
 *
 * @param isr		Interrupt handle as returned by @see hnd_isr_register.
 * @param intstatus	Interrupt status.
 */

void
hnd_sec_isr_run(hnd_isr_action_t *isr, uint32 intstatus)
{
	if (intstatus & isr->sec_handler_intmask) {
		hnd_sec_isr_action_t *action = isr->sec_handler;
		ASSERT(action != NULL);

		do {
			if (action->isr_fn != NULL && (action->intmask & intstatus)) {
				HND_INTSTATS_NOTIFY(NOTIFICATION_TYPE_ISR_ENTER,
					(void*)action->stats[0]);

				/* Invoke second level ISR */
				(action->isr_fn)(action->cbdata, intstatus);

				HND_INTSTATS_NOTIFY(NOTIFICATION_TYPE_ISR_EXIT,
					(void*)action->stats[0]);
			}
		} while ((action = action->next) != NULL);
	}
}

/**
 * Run any secondary DPCs that match the current interrupt status.
 *
 * To be called from the primary DPC for which the secondary DPCs were registered. Any secondary
 * DPCs whose interrupt mask match the specified interrupt status will be invoked.
 *
 * @param isr		Interrupt handle as returned by @see hnd_isr_register.
 * @param intstatus	Interrupt status.
 */

void
hnd_sec_dpc_run(hnd_isr_action_t *isr, uint32 intstatus)
{
	if (intstatus & isr->sec_handler_intmask) {
		hnd_sec_isr_action_t *action = isr->sec_handler;
		ASSERT(action != NULL);

		do {
			if (action->dpc_fn != NULL && (action->intmask & intstatus)) {
				HND_INTSTATS_NOTIFY(NOTIFICATION_TYPE_DPC_ENTER,
					(void*)action->stats[1]);

				/* Invoke second level DPC */
				(action->dpc_fn)(action->cbdata, intstatus);

				HND_INTSTATS_NOTIFY(NOTIFICATION_TYPE_DPC_EXIT,
					(void*)action->stats[1]);
			}
		} while ((action = action->next) != NULL);
	}
}

/**
 * Run any ISRs and schedule any DPCs that were registered for the specified flags.
 *
 * If an ISR and or DPC was registered for the specified flags, the ISR (if any) is invoked and
 * the DPC (if any) is scheduled to be run in thread context.
 *
 * @param sbflagst	IRQ flags.
 */

void
hnd_isr_proc_sbflagst(uint32 sbflagst)
{
	hnd_isr_instance_t *instance = hnd_isr_get_inst();
	hnd_isr_action_t *action;

	ASSERT(instance != NULL);

	sbflagst &= instance->hnd_action_flags;

	/* Find and run matching ISR */
	action = instance->hnd_isr_action_list;
	while (action && sbflagst != 0) {
		if (sbflagst & action->sbtpsflag) {
			sbflagst &= ~action->sbtpsflag;
			/* Invoke first level ISR */
			if (action->isr_fn != NULL) {
				HND_CPUUTIL_NOTIFY(HND_CPUUTIL_TRANS_ISR_ENT, action->cpuutil);
				HND_INTSTATS_NOTIFY(NOTIFICATION_TYPE_ISR_ENTER,
					(void*)action->stats);
				BUZZZ_LVL3(THREADX_ISR_ENT, 1, (uint32)(action->isr_fn));

				(action->isr_fn)(action->cbdata);

				BUZZZ_LVL3(THREADX_ISR_RTN, 0);
				HND_INTSTATS_NOTIFY(NOTIFICATION_TYPE_ISR_EXIT,
					(void*)action->stats);
				HND_CPUUTIL_NOTIFY(HND_CPUUTIL_TRANS_ISR_EXT, action->cpuutil);
			}

			/* Schedule first level DPC */
			if (action->dpc != NULL) {
				ASSERT(instance->schedule_dpc_fn != NULL);
				(instance->schedule_dpc_fn)(action->dpc);
			}
		}
		action = action->next;
	}
}

/**
 * Get statistics gathering status.
 *
 * @return			TRUE if active.
 */

inline bool
hnd_isr_stats_enabled(void)
{
#ifdef HND_INTSTATS
	return hnd_isr != NULL && hnd_isr->stats_enable;
#else
	return FALSE;
#endif /* HND_INTSTATS */
}

#ifdef HND_INTSTATS

/**
 * Update statistics.
 *
 * @param value			Statistics value set.
 * @param elapsed		Elapsed time.
 */

static void
update_stats(hnd_stats_val_t *value, uint32 elapsed)
{
	uint32 prev_value = value->total;

	value->total += elapsed;
	if (value->total < prev_value) {
		value->total = INTSTATS_OVERFLOW;
	}
	if (elapsed > value->max) {
		value->max = elapsed;
	}
	if (value->min == 0 || elapsed < value->min) {
		value->min = elapsed;
	}
	if (value->avg_count == INTSTATS_AVG_COUNT) {
		value->avg -= (value->avg / INTSTATS_AVG_COUNT);
	} else {
		value->avg_count++;
	}

	prev_value = value->avg;
	value->avg += elapsed;
	if (value->avg < prev_value) {
		value->avg = value->avg_count = 0;
	}
}

/**
 * Check if the current measurement interval has elapsed.
 *
 * @param time			Current time.
 */

static void
hnd_isr_check_interval(uint32 time)
{
	uint32 interval_elapsed;

	if (hnd_isr->stats_interval_last == 0) {
		hnd_isr->stats_interval_last = time;
	}

	interval_elapsed = time - hnd_isr->stats_interval_last;
	if (interval_elapsed != 0) {
		uint32 new = hnd_isr->stats_interval_current + interval_elapsed;
		hnd_isr->stats_interval_current = (new < hnd_isr->stats_interval_current) ?
			INTSTATS_OVERFLOW : new;
		if (hnd_isr->stats_interval != 0) {
			if (hnd_isr->stats_interval_current >= hnd_isr->stats_interval) {
				hnd_isr->stats_enable = FALSE;
				printf("statistics ready\n");
			}
		}
		hnd_isr->stats_interval_last = time;
	}
}

/**
 * End previous interval, and start active period and new interval.
 *
 * @param stats			Statistics object.
 */

static inline void
start_period_interval(hnd_stats_t *stats)
{
	if (stats != NULL) {
		/* End previous interval */
		if (stats->interval_enter != 0) {
			uint32 elapsed = hnd_isr->stats_scaled_time - stats->interval_enter;
			update_stats(&stats->interval, elapsed);
		}

		/* Start new period and interval */
		stats->active_enter = stats->interval_enter = hnd_isr->stats_scaled_time;
		stats->accumulated_time = 0;
		stats->is_active = TRUE;
		stats->count++;
	}
}

/**
 * Pause active period.
 *
 * @param stats			Statistics object.
 */

static inline void
pause_period(hnd_stats_t *stats)
{
	if (stats != NULL && stats->is_active) {
		stats->accumulated_time += hnd_isr->stats_scaled_time - stats->active_enter;
	}
}

/**
 * Resume active period.
 *
 * @param stats			Statistics object.
 */

static inline void
resume_period(hnd_stats_t *stats)
{
	if (stats != NULL && stats->is_active) {
		stats->active_enter = hnd_isr->stats_scaled_time;
	}
}

/**
 * End active period.
 *
 * @param stats			Statistics object.
 */

static inline void
end_period(hnd_stats_t *stats)
{
	if (stats != NULL && stats->is_active) {
		uint32 elapsed = hnd_isr->stats_scaled_time - stats->active_enter;
		elapsed += stats->accumulated_time;
		update_stats(&stats->active, elapsed);
		stats->is_active = FALSE;
	}
}

/**
 * Handle a context switch notification.
 *
 * @param type			Notification type.
 * @param context		Additional context data.
 */

void
hnd_isr_stats_notify(hnd_stats_notification_t type, void *context)
{
	hnd_dpc_t *current_dpc;
	osl_ext_task_t *thread;
	uint32 time, elapsed;
	hnd_stats_t *stats = (hnd_stats_t*)context;

	TX_INTERRUPT_SAVE_AREA

	if (hnd_isr->stats_enable == FALSE) {
		return;
	}

	TX_DISABLE

	OSL_GETCYCLES(time);

	/* Create virtual clock to increase range */
	hnd_isr->stats_accumulated_cycles += time - hnd_isr->stats_cycles_last;
	hnd_isr->stats_scaled_time += hnd_isr->stats_accumulated_cycles >> INTSTATS_CYCLES_SHIFT;
	hnd_isr->stats_accumulated_cycles &= INTSTATS_CYCLES_DIV - 1;
	hnd_isr->stats_cycles_last = time;

	switch (type) {
	case NOTIFICATION_TYPE_THREAD_ENTER:
		if (hnd_isr->stats_usr_only) {
			break;
		}
		thread = hnd_get_last_thread();
		if (thread != NULL) {
			end_period((hnd_stats_t*)thread->stats);	/* Previous thread */

			current_dpc = THREAD_DPC(thread);
			if (current_dpc != NULL) {
				pause_period(current_dpc->stats);	/* Preempted DPC */
			}
		}

		thread = (osl_ext_task_t*)context;
		ASSERT(thread != NULL);

		current_dpc = THREAD_DPC(thread);
		if (current_dpc != NULL) {
			resume_period(current_dpc->stats);		/* Scheduled DPC */
		}
		start_period_interval((hnd_stats_t*)thread->stats);	/* Current thread */
		break;
	case NOTIFICATION_TYPE_ISR_ENTER:
		if (hnd_isr->stats_usr_only) {
			break;
		}
		ASSERT(stats != NULL);
		thread = osl_ext_task_current();
		if (thread != NULL) {
			pause_period(thread->stats);			/* Preempted thread */

			current_dpc = THREAD_DPC(thread);
			if (current_dpc != NULL) {
				pause_period(current_dpc->stats);	/* Preempted DPC */
			}
		}
		start_period_interval(stats);				/* ISR */
		break;
	case NOTIFICATION_TYPE_ISR_EXIT:
		if (hnd_isr->stats_usr_only) {
			break;
		}
		ASSERT(stats != NULL);
		end_period(stats);					/* ISR */

		thread = osl_ext_task_current();
		if (thread != NULL) {
			current_dpc = THREAD_DPC(thread);
			if (current_dpc != NULL) {
				resume_period(current_dpc->stats);	/* Preempted DPC */
			}
			resume_period(thread->stats);			/* Preempted thread */
		}
		break;
	case NOTIFICATION_TYPE_TMR_ENTER:
	case NOTIFICATION_TYPE_DPC_ENTER:
		if (hnd_isr->stats_usr_only == FALSE) {
			start_period_interval(stats);			/* Timer/DPC */
		}
		break;
	case NOTIFICATION_TYPE_USR_ENTER:
		start_period_interval(stats);				/* USR */
		break;
	case NOTIFICATION_TYPE_TMR_EXIT:
	case NOTIFICATION_TYPE_DPC_EXIT:
		if (hnd_isr->stats_usr_only == FALSE) {
			end_period(stats);				/* Timer/DPC */
		}
		hnd_isr_check_interval(hnd_isr->stats_scaled_time);
		break;
	case NOTIFICATION_TYPE_USR_EXIT:
		end_period(stats);					/* USR */
		hnd_isr_check_interval(hnd_isr->stats_scaled_time);
		break;
	case NOTIFICATION_TYPE_QUEUE_ENTER:
		ASSERT(stats != NULL);
		stats->queue_enter = hnd_isr->stats_scaled_time;	/* Enqueued DPC */
		break;
	case NOTIFICATION_TYPE_QUEUE_EXIT:
		ASSERT(stats != NULL);
		if (hnd_isr->stats_usr_only == FALSE) {
			elapsed = hnd_isr->stats_scaled_time - stats->queue_enter;
			update_stats(&stats->queued, elapsed);		/* Dequeued DPC */
		}
		break;
	default:
		break;
	}

	TX_RESTORE
}

static uint32
hnd_isr_convert(uint32 cycles, uint divider, hnd_stats_dumpflags_t flags)
{
	uint32 value = INTSTATS_OVERFLOW;

	cycles = (divider != 0 ? cycles / divider : 0);

	if (flags & INTSTATS_DUMP_CYCLES) {
		if (cycles <= 0xFFFFFFFF / INTSTATS_CYCLES_DIV) {
			value = cycles * INTSTATS_CYCLES_DIV;
		}
	} else if (cycles != INTSTATS_OVERFLOW) {
		if (cycles <= 0xFFFFFFFF / INTSTATS_CYCLES_DIV) {
			value = (cycles * INTSTATS_CYCLES_DIV) / c0counts_per_us;
		} else {
			value = cycles / (c0counts_per_us / INTSTATS_CYCLES_DIV);
		}
	}

	return value;
}

static void
hnd_isr_dump_stats_single(hnd_stats_t *stats, uint level, hnd_stats_dumpflags_t flags,
	uint interval)
{
	static const char *types[] = { "    THREAD", "    TMR_DPC", "    INT_DPC", "    DPC",
				       "    ISR", "    SEC_ISR", "    SEC_DPC", "    USR" };

	uint percent = interval >= 10000 ? (stats->active.total / (interval / 10000)) : 0;

	level = (level > 4 ? 0 : 4 - level);

	printf("%10s %10u %10u %3u.%02u%% %10u %10u %10u %10u %10u %10u %10u %10u %10u [%p] %s\n",
		stats->type < OBJ_TYPE_LAST ? types[stats->type] + level : "",
		stats->count,
		hnd_isr_convert(stats->active.total, 1, flags),
		percent / 100, percent % 100,
		hnd_isr_convert(stats->active.min, 1, flags),
		hnd_isr_convert(stats->active.avg, stats->active.avg_count, flags),
		hnd_isr_convert(stats->active.max, 1, flags),
		hnd_isr_convert(stats->interval.min, 1, flags),
		hnd_isr_convert(stats->interval.avg, stats->interval.avg_count, flags),
		hnd_isr_convert(stats->interval.max, 1, flags),
		hnd_isr_convert(stats->queued.min, 1, flags),
		hnd_isr_convert(stats->queued.avg, stats->queued.avg_count, flags),
		hnd_isr_convert(stats->queued.max, 1, flags),
		stats->target,
		stats->id ? stats->id : "");
}

static void
hnd_isr_dump_stats_internal(hnd_stats_t *stats, uint level, hnd_stats_dumpflags_t flags,
	uint interval)
{
	hnd_stats_t *cur;

	if ((flags & INTSTATS_DUMP_ALL) || stats->count != 0) {
		if (!(flags & INTSTATS_DUMP_USR_ONLY) || stats->type == OBJ_TYPE_USR) {
			hnd_isr_dump_stats_single(stats, level, flags, interval);
			for (cur = hnd_isr->stats_list; cur; cur = cur->next) {
				if (cur->reserved == 1 && cur->parent == stats) {
					cur->reserved = 0;
					hnd_isr_dump_stats_internal(cur, level+1, flags, interval);
				}
			}
		}
	}
	stats->reserved = 0;
}

/**
 * Dump all statistics.
 *
 * @param flags			Dump flags.
 */

void
hnd_isr_dump_stats(hnd_stats_dumpflags_t flags)
{
	hnd_stats_t *stats;
	uint32 interval;
	uint64 total_active = 0;

	printf_suppress_timestamp(TRUE);

	if (hnd_isr->stats_interval != 0) {
		if (hnd_isr->stats_interval_current < hnd_isr->stats_interval) {
			printf("[busy]\n");
			return;
		}
	}

	if (hnd_isr->stats_enable == FALSE) {
		printf("[disabled] ");
	}

	if (hnd_isr->stats_usr_only == TRUE) {
		printf("[usr only] ");
	}

	interval = hnd_isr_convert(hnd_isr->stats_interval_current, 1, flags);
	if ((flags & INTSTATS_DUMP_CYCLES) && interval == INTSTATS_OVERFLOW) {
		interval = hnd_isr->stats_interval_current;
		printf("[interval:%u *%u] ", hnd_isr->stats_interval_current, INTSTATS_CYCLES_DIV);
	} else {
		printf("[interval:%u] ", interval);
	}

	printf("[units:%s]\n", (flags & INTSTATS_DUMP_CYCLES) ? "cycles" : "us");

	printf("TYPE         RUNCOUNT  ACT_TOTAL PERCENT    "
		"ACT_MIN    ACT_AVG    ACT_MAX    "
		"INT_MIN    INT_AVG    INT_MAX  "
		"QUEUE_MIN  QUEUE_AVG  QUEUE_MAX TARGET\n");

	for (stats = hnd_isr->stats_list; stats; stats = stats->next) {
		stats->reserved = 1;
		if (stats->type == OBJ_TYPE_THREAD || stats->type == OBJ_TYPE_ISR) {
			total_active += stats->active.total;
		}
	}
	for (stats = hnd_isr->stats_list; stats; stats = stats->next) {
		if (stats->reserved == 1 && stats->parent == NULL) {
			hnd_isr_dump_stats_internal(stats, 0, flags,
				total_active <= 0xffffffff ? total_active : 0);
		}
	}

	if (flags & INTSTATS_DUMP_RESET) {
		hnd_isr_reset_stats();
	}

	printf_suppress_timestamp(FALSE);
}

/**
 * Reset all statistics.
 */

void hnd_isr_reset_stats(void)
{
	OSL_INTERRUPT_SAVE_AREA

	hnd_stats_t *stats;

	OSL_DISABLE

	for (stats = hnd_isr->stats_list; stats; stats = stats->next) {
		stats->is_active = stats->count = stats->accumulated_time = 0;
		stats->active_enter = stats->interval_enter = stats->queue_enter = 0;
		memset(&stats->active, 0, sizeof(hnd_stats_val_t));
		memset(&stats->interval, 0, sizeof(hnd_stats_val_t));
		memset(&stats->queued, 0, sizeof(hnd_stats_val_t));
	}

	hnd_isr->stats_interval_current = 0;
	hnd_isr->stats_interval_last = 0;
	hnd_isr->stats_interval = 0;
	hnd_isr->stats_accumulated_cycles = 0;
	hnd_isr->stats_scaled_time = 0;

	OSL_RESTORE
}

/**
 * Enable or disable statistics gathering.
 *
 * @param enable		TRUE to enable. Enabling will also reset statistics.
 * @param flags			Enable flags.
 * @param interval		Interval measurement duration, or 0.
 */

void hnd_isr_enable_stats(bool enable, hnd_stats_enableflags_t flags, osl_ext_time_ms_t interval)
{
	OSL_INTERRUPT_SAVE_AREA

	interval = (interval < INTSTATS_INTERVAL_MIN_MS ? INTSTATS_INTERVAL_MIN_MS : interval);
	interval = (interval > INTSTATS_INTERVAL_MAX_MS ? INTSTATS_INTERVAL_MAX_MS : interval);

	OSL_DISABLE

	hnd_isr->stats_enable = enable;
	if (enable) {
		hnd_isr_reset_stats();
		hnd_isr->stats_usr_only = !!(flags & INTSTATS_ENAB_USR_ONLY);
		if (flags & INTSTATS_ENAB_INTERVAL) {
			hnd_isr->stats_interval = (c0counts_per_ms/INTSTATS_CYCLES_DIV) * interval;
		}
	}

	OSL_RESTORE
}

#endif /* HND_INTSTATS */
