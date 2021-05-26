/*
 * RTE private interfaces between different modules in RTE.
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
 * $Id: rte_priv.h 782958 2020-01-09 15:29:12Z $
 */

#ifndef _hnd_rte_priv_h_
#define _hnd_rte_priv_h_

#include <typedefs.h>
#include <siutils.h>
#include <osl_decl.h>
#include <hnd_debug.h>
#include <hnd_trap.h>

/* Forward declaration */
extern si_t *hnd_sih;		/* Chip backplane handle */
extern osl_t *hnd_osh;		/* Chip backplane osh */

extern uint32 c0counts_per_us;
extern uint32 c0counts_per_ms;

/* ========================== system ========================== */
/* Each CPU/Arch must implement this interface - wait for interrupt */
void hnd_wait_irq(si_t *sih);

void hnd_enable_interrupts(void);
void hnd_disable_interrupts(void);
#ifdef ATE_BUILD
#define hnd_poll(sih) wl_ate_cmd_proc()
void wl_ate_cmd_proc(void);
#else
void hnd_poll(si_t *sih);
#endif /* !ATE_BUILD */

/* ======================= trap ======================= */
void hnd_trap_init(void);
void hnd_trap_common(trap_t *tr);
void hnd_print_stack(uint32 sp);

/* ================ CPU ================= */
void hnd_cpu_init(si_t *sih);

int hnd_cpu_stats_init(si_t *sih);
void hnd_cpu_stats_upd(uint32 start_time);

void hnd_cpu_load_avg(uint epc, uint lr, uint sp);
int32 hnd_cpu_clockratio(si_t *sih, uint8 div);

#if defined(HND_CPUUTIL)
/** ---------- Broadcom CPU Utilization Tool for Threadx RTE ---------- */

typedef enum hnd_cpuutil_context_type
{
	HND_CPUUTIL_CONTEXT_THR = 0,    /* TX_THREAD, osl_ext_task_t */
	HND_CPUUTIL_CONTEXT_ISR = 1,    /* hnd_israction_t */
	HND_CPUUTIL_CONTEXT_TMR = 2,    /* hnd_timer_t */
	HND_CPUUTIL_CONTEXT_DPC = 3,    /* hnd_dpc_t */
	HND_CPUUTIL_CONTEXT_USR = 4,    /* User function/algorithm profiling */
	HND_CPUUTIL_CONTEXT_TYPE_MAX = 5
} hnd_cpuutil_context_type_t;

typedef enum hnd_cpuutil_transition
{
	HND_CPUUTIL_TRANS_THR_ENT = 0,  /* Threadx: thread execution switch */
	HND_CPUUTIL_TRANS_ISR_ENT,      /* ISR Action Entry */
	HND_CPUUTIL_TRANS_ISR_EXT,      /* ISR Action Exit */
	HND_CPUUTIL_TRANS_TMR_ENT,      /* Timer mainfn Entry */
	HND_CPUUTIL_TRANS_TMR_EXT,      /* Timer mainfn Exit */
	HND_CPUUTIL_TRANS_DPC_ENT,      /* Deferred Procedure Call dpc_fn Entry */
	HND_CPUUTIL_TRANS_DPC_EXT,      /* Deferred Procedure Call dpc_fn Entry */
	HND_CPUUTIL_TRANS_USR_ENT,      /* Entry into a User context */
	HND_CPUUTIL_TRANS_USR_EXT,      /* Exit from a User context */
	HND_CPUUTIL_TRANS_MAX_EVT
} hnd_cpuutil_transition_t;

/** Initialize CPU Util Tool */
int hnd_cpuutil_init(si_t *sih);

/** alloc/free a CPU Utilization context for a RTE execution context type */
void *hnd_cpuutil_context_get(uint32 ctx_type, uint32 func);
void *hnd_cpuutil_context_put(uint32 ctx_type, void *context);

/** Inform CPU Util of a change in RTE execution context */
void hnd_cpuutil_notify(uint32 trans_type, void *context);

/* See also: hnd_thread_cpuutil_bind() in threadx_osl_ext.h|c and threadx.c */

/** Notify CPU Util Tool of a transition in a CPU Util context */
#define HND_CPUUTIL_NOTIFY(trans_type, context)                                \
({                                                                             \
	TX_INTERRUPT_SAVE_AREA                                                     \
	TX_DISABLE                                                                 \
	hnd_cpuutil_notify((trans_type), (context));                               \
	TX_RESTORE                                                                 \
})

/** Allocate a CPU Util context for profiling */
#define HND_CPUUTIL_CONTEXT_GET(ctx_type, func)                                \
	hnd_cpuutil_context_get((ctx_type), (func))

/** Deallocate a CPU Util context for profiling */
#define HND_CPUUTIL_CONTEXT_PUT(ctx_type, context)                             \
	hnd_cpuutil_context_put((ctx_type), (context))

#else  /* ! HND_CPUUTIL */

#define HND_CPUUTIL_NOTIFY(trans, context)      do { /* null body */ } while (0)
#define HND_CPUUTIL_CONTEXT_GET(ctx, func)      NULL
#define HND_CPUUTIL_CONTEXT_PUT(ctx, context)   NULL

#endif /* ! HND_CPUUTIL */

extern void * hnd_thread_cpuutil_bind(unsigned int thread_func);

/* ========================== time ========================== */
uint32 hnd_update_now(void);
uint32 hnd_update_now_us(void);

/* ========================== timer ========================== */
/* Each CPU/Arch must implement this interface - init possible global states */
int hnd_timer_init(si_t *sih);
/* Each CPU/Arch must implement this interface - program the h/w */
void hnd_set_irq_timer(uint us);
/* Each CPU/Arch must implement this interface - ack h/w interrupt */
void hnd_ack_irq_timer(void);
uint64 hnd_thread_get_hw_timer_us(void);
void hnd_thread_set_hw_timer(uint us);
void hnd_thread_ack_hw_timer(void);

/* Each CPU/Arch must implement this interface - run timeouts */
void hnd_advance_time(void);
void hnd_schedule_timer(void);
/* Each CPU/Arch must implement this interface - register possible command line proc */
void hnd_timer_cli_init(void);

/* ======================= debug ===================== */
void hnd_stack_prot(void *stack_top);
hnd_debug_t *hnd_debug_info_get(void);
uint32 hnd_notify_trapcallback(uint8 trap_type);

/* ======================= cache ===================== */
#ifdef RTE_CACHED
void hnd_caches_init(si_t *sih);
#endif // endif

/* accessor functions */
extern si_t* get_hnd_sih(void);
#endif /* _hnd_rte_priv_h_ */
