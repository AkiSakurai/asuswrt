/*
 * OS independent ISR functions for ISRs or worklets - Private to RTE.
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
 * $Id: rte_isr_priv.h 787983 2020-06-17 17:59:06Z $
 */

#ifndef	_hnd_isr_priv_
#define	_hnd_isr_priv_

#include <typedefs.h>
#include <osl_ext.h>
#include <rte_isr.h>
#include "rte_scheduler_priv.h"

/* Maximum number of interrupt status registers per core */
#define HND_ISR_INDEX_COUNT	4

/* Secondary interrupt handler info */
struct hnd_isr_sec_action
{
	hnd_isr_sec_action_t	*next;		/* Next handler */
	hnd_isr_action_t	*primary;	/* Backpointer to primary handler */
	uint32			intmask;	/* Interrupt mask */
	hnd_isr_sec_fn_t	isr_fn;		/* Function to be called in interrupt context */
	void			*cbdata;	/* ISR context data */
	hnd_worklet_t		*worklet;
	hnd_stats_t		*stats;		/* Secondary ISR and worklet statistics */
};

/* Primary interrupt handler info */
struct hnd_isr_action
{
	hnd_isr_action_t	*next;		/* Next handler */
	hnd_isr_sec_action_t	*sec[HND_ISR_INDEX_COUNT];		/* Secondary handlers */
	uint32			sec_intmask[HND_ISR_INDEX_COUNT];	/* Secondary masks */
	uint32			intmask;	/* Interrupt mask */
	hnd_isr_fn_t		isr_fn;		/* Function to be called in interrupt context */
	void			*cbdata;	/* ISR context data */
	hnd_worklet_t		*worklet;	/* Optional worklet to be scheduled after ISR */
	void			*cpuutil;	/* CPU Util context */
	hnd_stats_t		*stats;		/* ISR statistics */
};

typedef struct hnd_isr_instance hnd_isr_instance_t;

struct hnd_isr_instance
{
	hnd_isr_action_t	*actions;		/* Registered primary interrupt handlers */
	uint32			action_intmask;		/* Flags for which handers are registered */
	hnd_schedule_worklet_fn_t schedule_worklet_fn;	/* worklet scheduling function pointer */

#ifdef HND_INTSTATS
	hnd_stats_t		*stats_list;		/* Statistics data */
	bool			stats_enable;		/* Statistics collection enable flag */
	bool			stats_usr_only;		/* Only collect user-defined statistics */
	uint32			stats_interval;		/* Interval measurement interval, or 0 */
	uint32			stats_interval_current;
	uint32			stats_interval_last;
	uint32			stats_cycles_last;
	uint32			stats_accumulated_cycles;
	uint32			stats_scaled_time;
#endif /* HND_INTSTATS */
};

/* Get ISR instance */
extern hnd_isr_instance_t *hnd_isr;
#define hnd_isr_get_inst() hnd_isr

void hnd_isr_process_interrupts(uint32 intstatus);
void hnd_isr_module_init(osl_t *osh, hnd_schedule_worklet_fn_t schedule_worklet_fn);

#endif /* _hnd_isr_priv_ */
