/*
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: rte_isr.h 788502 2020-07-02 09:49:18Z $
 */

#ifndef	_RTE_ISR_H
#define	_RTE_ISR_H

#include <typedefs.h>
#include <rte_isr_stats.h>
#include <rte_scheduler.h>

/* Interrupt status register index to use when core supports only one register */
#define HND_ISR_INDEX_DEFAULT		0

typedef struct hnd_isr_action hnd_isr_action_t;
typedef struct hnd_isr_sec_action hnd_isr_sec_action_t;

typedef bool (*hnd_isr_fn_t)(void *cbdata);
typedef bool (*hnd_isr_sec_fn_t)(void *cbdata, uint index, uint32 intstatus);

#define hnd_isr_register(coreid, unit, bus, isr_fn, cbdata_isr,					\
	worklet_fn, cbdata_worklet, thread)							\
	hnd_isr_register_internal(hnd_isr_intmask_for_core((coreid), (unit), (bus)),		\
	(isr_fn), (cbdata_isr), (worklet_fn), (cbdata_worklet), (thread),			\
	OBJECT_ID(isr_fn), OBJECT_ID(worklet_fn))
#define hnd_isr_register_n(irq, isr_fn, cbdata_isr, worklet_fn, cbdata_worklet, thread)		\
	hnd_isr_register_internal(hnd_isr_intmask_for_irq(irq),					\
	(isr_fn), (cbdata_isr), (worklet_fn), (cbdata_worklet), (thread),			\
	OBJECT_ID(isr_fn), OBJECT_ID(worklet_fn))
#define hnd_isr_register_with_worklet(coreid, unit, bus, isr_fn, cbdata, worklet)		\
	hnd_isr_register_with_worklet_internal(hnd_isr_intmask_for_core((coreid), (unit), (bus)), \
	(isr_fn), (cbdata), (worklet), OBJECT_ID(isr_fn))
#define hnd_isr_register_sec(primary, index, intmask, isr_fn, cbdata_isr, worklet_fn,		\
	cbdata_worklet, thread)									\
	hnd_isr_register_sec_internal((primary), (index), (intmask),				\
	(isr_fn), (cbdata_isr), (worklet_fn), (cbdata_worklet), (thread),			\
	OBJECT_ID(isr_fn), OBJECT_ID(worklet_fn))						\

hnd_isr_action_t* hnd_isr_register_internal(uint32 intmask, hnd_isr_fn_t isr_fn, void *cbdata_isr,
	hnd_worklet_fn_t worklet_fn, void *cbdata_worklet, osl_ext_task_t *thread,
	const char *isr_id, const char *worklet_id);
hnd_isr_action_t* hnd_isr_register_with_worklet_internal(uint32 intmask,
	hnd_isr_fn_t isr_fn, void *cbdata, hnd_worklet_t* worklet, const char *id);
hnd_isr_sec_action_t* hnd_isr_register_sec_internal(hnd_isr_action_t *primary, uint index,
	uint32 intmask, hnd_isr_sec_fn_t isr_fn, void *cbdata_isr,
	hnd_worklet_fn_t worklet_fn, void *cbdata_worklet, osl_ext_task_t *thread,
	const char *isr_id, const char *worklet_id);

uint32 hnd_isr_intmask_for_irq(uint irq);
uint32 hnd_isr_intmask_for_core(uint coreid, uint unit, uint bus);

void hnd_isr_invoke_sec_isr(hnd_isr_action_t *action, uint index, uint32 intstatus);
void hnd_isr_invoke_sec_worklet(hnd_isr_action_t *action, uint index, uint32 intstatus);
void hnd_isr_process_sec_interrupts(hnd_isr_action_t *action, uint index, uint32 intstatus);

#endif	/* _RTE_ISR_H */
